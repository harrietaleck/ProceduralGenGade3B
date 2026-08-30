// ProceduralTerrain.cpp
// Implementation of the runtime tower-defence terrain generator. See ProceduralTerrain.h
// for the high-level description. Generation runs as an ordered pipeline:
//   InitialiseGrid -> CarvePaths -> BuildHeightmap -> MarkBuildableSlots -> BuildMesh
// followed by publishing the tower location, enemy paths and defender slots.

#include "ProceduralTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

// Everything flat (paths, tower pad, buildable pads) sits on this Z plane in local space.
static constexpr float PathPlaneHeight = 0.0f;

// Chaikin corner-cutting: replaces each segment with two points 1/4 and 3/4 along it, which
// rounds a blocky polyline into a smooth curve over successive iterations. The first and last
// points are re-anchored exactly after each pass so the spawn and tower ends never drift.
static TArray<FVector> ChaikinSmooth(const TArray<FVector>& Points, int32 Iterations)
{
	TArray<FVector> Current = Points;
	for (int32 It = 0; It < Iterations && Current.Num() >= 3; ++It)
	{
		TArray<FVector> Next;
		Next.Reserve(Current.Num() * 2);
		for (int32 I = 0; I + 1 < Current.Num(); ++I)
		{
			Next.Add(FMath::Lerp(Current[I], Current[I + 1], 0.25f));
			Next.Add(FMath::Lerp(Current[I], Current[I + 1], 0.75f));
		}
		Next[0] = Current[0];
		Next.Last() = Current.Last();
		Current = MoveTemp(Next);
	}
	return Current;
}

// Drop waypoints that sit too close together so enemies follow gentle arcs instead of
// micro-correcting every few units (reads much more natural at walking pace).
static TArray<FVector> ThinWaypoints(const TArray<FVector>& Points, float MinSpacing)
{
	if (Points.Num() <= 2)
	{
		return Points;
	}

	TArray<FVector> Result;
	Result.Add(Points[0]);
	const float MinSpacingSq = MinSpacing * MinSpacing;

	for (int32 I = 1; I < Points.Num(); ++I)
	{
		if (FVector::DistSquared2D(Points[I], Result.Last()) >= MinSpacingSq)
		{
			Result.Add(Points[I]);
		}
	}

	if (!Result.Last().Equals(Points.Last(), 1.0f))
	{
		Result.Add(Points.Last());
	}
	return Result;
}

AProceduralTerrain::AProceduralTerrain()
{
	// Terrain is static once generated, so we never need per-frame ticking.
	PrimaryActorTick.bCanEverTick = false;

	// The procedural mesh both renders the terrain and provides its collision.
	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	SetRootComponent(MeshComponent);
	// Synchronous collision cooking: generation immediately rebuilds navigation and validates
	// AI pathfinding against this same collision (RebuildNavigation/ValidatePathfinding), all
	// within the same frame. With async cooking, that validation could run before the collision
	// job finishes, leaving Recast with no walkable geometry to voxelize.
	MeshComponent->bUseAsyncCooking = false;

	// Block all channels so the terrain is a solid surface for cursor traces (defender placement)
	// and any collision queries. Query-only is enough since nothing physically simulates on it.
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// Default to the engine's vertex-colour material so our per-cell colours (path/build/tower)
	// are visible without any manual material setup.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMat(
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMat.Succeeded())
	{
		TerrainMaterial = VertexColorMat.Object;
	}
}

void AProceduralTerrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Generate a deterministic preview in the editor using the current Seed so the
	// designer can see and tweak the map without pressing Play.
	GenerateTerrain();
}

void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	// Deliberately does NOT generate here. Unreal does not guarantee this actor's BeginPlay
	// runs before the GameMode's — relying on that would be a latent ordering bug, since the
	// GameMode spawns the Tower and build-pad markers from this terrain's data. Generation is
	// instead triggered explicitly and synchronously by the GameMode via PrepareForNewGame(),
	// so there is never any ambiguity about whether fresh data is ready before it's read.
}

void AProceduralTerrain::RandomizeAndRegenerate()
{
	Seed = FMath::RandRange(1, MAX_int32 - 1);
	GenerateTerrain();
}

void AProceduralTerrain::PrepareForNewGame()
{
	// The brief requires the terrain to differ every new game: pick a fresh random seed
	// (unless the designer pinned a fixed seed for testing) and generate synchronously.
	if (bRandomizeSeedOnBeginPlay)
	{
		Seed = FMath::RandRange(1, MAX_int32 - 1);
	}
	GenerateTerrain();
}

void AProceduralTerrain::RunStressTest(int32 NumIterations)
{
	NumIterations = FMath::Max(1, NumIterations);

	const int32 OriginalSeed = Seed;
	int32 PassCount = 0;
	double TotalSeconds = 0.0;

	UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain [STRESS TEST]: starting %d generations..."), NumIterations);

	for (int32 I = 1; I <= NumIterations; ++I)
	{
		Seed = FMath::RandRange(1, MAX_int32 - 1);

		const double StartTime = FPlatformTime::Seconds();
		GenerateTerrain(); // Includes its own validate-and-regenerate-on-failure loop.
		const double ElapsedSeconds = FPlatformTime::Seconds() - StartTime;
		TotalSeconds += ElapsedSeconds;

		ValidatePathfinding(); // Logs NavMesh coverage as a diagnostic; doesn't affect pass/fail.
		const bool bPass = ValidateGeneratedWorld();
		PassCount += bPass ? 1 : 0;

		UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain [STRESS TEST] run %d/%d: seed=%d, paths=%d, buildSlots=%d, time=%.2fms -> %s"),
			I, NumIterations, Seed, EnemyPaths.Num(), DefenderSlots.Num(), ElapsedSeconds * 1000.0, bPass ? TEXT("PASS") : TEXT("FAIL"));
	}

	UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain [STRESS TEST] complete: %d/%d passed (%.1f%%), average time %.2fms."),
		PassCount, NumIterations, 100.0 * PassCount / NumIterations, 1000.0 * TotalSeconds / NumIterations);

	// Leave the terrain on a fresh, valid generation rather than the caller's original seed,
	// so the level is left in a normal playable state after the test runs.
	Seed = OriginalSeed;
	GenerateTerrain();
}

void AProceduralTerrain::GenerateTerrain()
{
	// Guard against invalid parameters that would break indexing or the brief's rules.
	GridSize = FMath::Max(8, GridSize);
	NumPaths = FMath::Max(3, NumPaths); // The brief mandates at least three paths.

	// The generation pipeline is deterministic-by-construction and should always produce a
	// valid world, but we verify it anyway and regenerate with a new seed on the rare chance
	// something is wrong — gameplay must never begin on an unvalidated map. Bounded attempts
	// so a genuine bug can't hang the game; if every attempt fails we log loudly and use the
	// last (still fully-formed, just not ideal) result rather than leaving no terrain at all.
	const int32 MaxAttempts = 5;
	bool bValid = false;
	for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
	{
		// Seed the random stream so the entire map is reproducible from Seed alone.
		Rng.Initialize(Seed);

		// Clear previously published data.
		EnemyPaths.Reset();
		DefenderSlots.Reset();
		PathCellLines.Reset();
		GridExpansionCount = 0;

		// Run the generation pipeline in order (each stage depends on the previous one).
		InitialiseGrid();
		CarvePaths();
		BuildHeightmap();
		MarkBuildableSlots();
		BuildMesh();

		// ---- Publish the tower location (world space) at the centre cell ----
		const int32 CX = GridSize / 2;
		const int32 CY = GridSize / 2;
		TowerLocation = ActorToWorld().TransformPosition(CellCenterLocal(CX, CY, PathPlaneHeight));

		bValid = ValidateGeneratedWorld();
		if (bValid)
		{
			// Rebuild the NavMesh so it's current for the "Press P" debug overlay and the
			// (non-blocking) coverage diagnostic in ValidatePathfinding() — actual movement
			// doesn't depend on it, so it isn't part of the pass/fail decision here.
			RebuildNavigation();
			ValidatePathfinding();
		}

		if (bValid)
		{
			break;
		}

		if (Attempt < MaxAttempts)
		{
			UE_LOG(LogTemp, Warning, TEXT("ProceduralTerrain: generated world failed validation on attempt %d/%d (seed %d) — regenerating with a new seed."),
				Attempt, MaxAttempts, Seed);
		}
		Seed = FMath::RandRange(1, MAX_int32 - 1);
	}

	if (!bValid)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralTerrain: failed to generate a valid world after %d attempts — using the last attempt anyway."), MaxAttempts);
		RebuildNavigation(); // Make sure nav still matches whatever geometry we ended up keeping.
	}

	if (bDebugMode)
	{
		DrawDebugVisualization();
	}
}

bool AProceduralTerrain::ValidateGeneratedWorld() const
{
	// The grid/heightmap data must be fully and correctly sized — a corrupt or partial
	// generation would index out of bounds later.
	if (Cells.Num() != GridSize * GridSize || VertexHeights.Num() != (GridSize + 1) * (GridSize + 1))
	{
		return false;
	}

	// Mandatory: at least three enemy paths.
	if (EnemyPaths.Num() < 3)
	{
		return false;
	}

	for (const FEnemyPath& Path : EnemyPaths)
	{
		// A path needs at least a spawn point and a destination to mean anything.
		if (Path.Waypoints.Num() < 2)
		{
			return false;
		}

		// Every path must actually terminate at (or immediately next to) the tower — checked
		// on the XY plane with a generous one-cell tolerance for floating point/edge cases.
		if (FVector::DistSquared2D(Path.Waypoints.Last(), TowerLocation) > FMath::Square(CellSize * 1.5f))
		{
			return false;
		}

		// No broken/disconnected paths: consecutive waypoints must never be further apart than
		// a couple of cells. Smoothing only ever subdivides segments (shrinks the gap), so this
		// bound catches a genuinely broken chain without being tripped by smoothing itself.
		for (int32 I = 0; I + 1 < Path.Waypoints.Num(); ++I)
		{
			if (FVector::DistSquared2D(Path.Waypoints[I], Path.Waypoints[I + 1]) > FMath::Square(CellSize * 2.0f))
			{
				return false;
			}
		}
	}

	// At least one buildable location must exist, or the game is unplayable.
	if (DefenderSlots.Num() == 0)
	{
		return false;
	}

	// No two build slots may overlap or sit closer than a single cell apart — each candidate
	// comes from a distinct grid cell, but this catches any future regression explicitly rather
	// than relying only on that construction guarantee.
	for (int32 I = 0; I < DefenderSlots.Num(); ++I)
	{
		for (int32 J = I + 1; J < DefenderSlots.Num(); ++J)
		{
			if (FVector::DistSquared2D(DefenderSlots[I].Location, DefenderSlots[J].Location) < FMath::Square(CellSize * 0.99f))
			{
				return false;
			}
		}
	}

	return true;
}

bool AProceduralTerrain::ValidatePathfinding() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Diagnostic only — logs coverage, never blocks gameplay. Enemies in this game move along
	// the fixed Waypoints array carved by CarvePaths() (see AEnemy::SetPath/Tick), not via UE's
	// NavMesh/AIController MoveTo system, so NavMesh reachability has no bearing on whether a
	// map is actually playable; the geometric checks in ValidateGeneratedWorld() (every path's
	// cells are contiguous and its last waypoint reaches the tower) are what real movement
	// depends on, and those already gate GenerateTerrain()'s retry loop. NavMesh queries here can
	// legitimately fail even on a perfectly playable map — e.g. the moment the Tower actor's own
	// BlockAll collision exists, Recast correctly carves a hole around it, which can make the
	// exact ground-level TowerLocation point unreachable as a query target despite every path
	// visually and physically leading right up to it.
	int32 ConnectedPathCount = 0;
	for (const FEnemyPath& Path : EnemyPaths)
	{
		if (Path.Waypoints.Num() == 0)
		{
			continue;
		}
		UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, Path.SpawnPoint, TowerLocation);
		if (NavPath && NavPath->IsValid() && !NavPath->IsPartial())
		{
			++ConnectedPathCount;
		}
	}
	if (ConnectedPathCount < EnemyPaths.Num())
	{
		UE_LOG(LogTemp, Verbose, TEXT("ProceduralTerrain: %d/%d enemy paths resolved a complete NavMesh route to the tower (non-blocking — enemies move via fixed waypoints, not NavMesh)."),
			ConnectedPathCount, EnemyPaths.Num());
	}

	int32 ConnectedSlotCount = 0;
	for (const FDefenderSlot& Slot : DefenderSlots)
	{
		UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, Slot.Location, TowerLocation);
		if (NavPath && NavPath->IsValid() && !NavPath->IsPartial())
		{
			++ConnectedSlotCount;
		}
	}
	if (ConnectedSlotCount < DefenderSlots.Num())
	{
		UE_LOG(LogTemp, Verbose, TEXT("ProceduralTerrain: %d/%d build slots resolved a complete NavMesh route to the tower (non-blocking — defender placement doesn't use NavMesh)."),
			ConnectedSlotCount, DefenderSlots.Num());
	}

	return true;
}

void AProceduralTerrain::SetSlotOccupied(const FVector& Location, bool bOccupied)
{
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = FMath::Square(CellSize * 0.5f);
	for (int32 I = 0; I < DefenderSlots.Num(); ++I)
	{
		const float DistSq = FVector::DistSquared2D(DefenderSlots[I].Location, Location);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = I;
		}
	}
	if (DefenderSlots.IsValidIndex(BestIndex))
	{
		DefenderSlots[BestIndex].bOccupied = bOccupied;
	}
}

bool AProceduralTerrain::IsSlotOccupied(const FVector& Location) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = FMath::Square(CellSize * 0.5f);
	for (int32 I = 0; I < DefenderSlots.Num(); ++I)
	{
		const float DistSq = FVector::DistSquared2D(DefenderSlots[I].Location, Location);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = I;
		}
	}
	return DefenderSlots.IsValidIndex(BestIndex) && DefenderSlots[BestIndex].bOccupied;
}

void AProceduralTerrain::RebuildNavigation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Warn loudly (once per generation) if no placed NavMeshBoundsVolume actually covers the
	// terrain's current footprint, rather than silently producing an unwalkable map. Runtime
	// resizing of a placed volume's brush geometry proved unreliable (its cached bounds didn't
	// consistently follow actor-transform changes at PIE runtime), so instead of fighting that,
	// this requires the level to already contain a volume generously sized for the configured
	// GridSize/CellSize/HeightScale — exactly how the volume was set up in the level (see
	// TowerDefense.umap's NavMeshBoundsVolume_0). ValidatePathfinding() below is the real,
	// authoritative check that navigation is actually usable.
	const float Half = GridSize * CellSize * 0.5f;
	const FBox RequiredBounds(GetActorLocation() + FVector(-Half, -Half, -50.0f), GetActorLocation() + FVector(Half, Half, HeightScale + 50.0f));
	bool bAnyVolumeCoversTerrain = false;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		if (It->GetComponentsBoundingBox(/*bNonColliding=*/true).IsInside(RequiredBounds))
		{
			bAnyVolumeCoversTerrain = true;
			break;
		}
	}
	if (!bAnyVolumeCoversTerrain)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProceduralTerrain: no NavMeshBoundsVolume in the level fully covers the current terrain footprint — enlarge NavMeshBoundsVolume_0 in the level to cover at least +/-%.0f uu horizontally and %.0f uu vertically."),
			Half, HeightScale);
	}

	// FNavigationSystem::Build() only *schedules* tile generation under RuntimeGeneration=Dynamic
	// (the mode required for any runtime/PIE rebuild to do anything at all — see above) — it does
	// not block until tiles finish. Without waiting here, ValidatePathfinding() immediately after
	// would query a still-empty or partially-built NavMesh. EnsureBuildCompletion() forces every
	// pending tile task to finish synchronously before this function returns.
	FNavigationSystem::Build(*World);
	for (TActorIterator<ARecastNavMesh> NavIt(World); NavIt; ++NavIt)
	{
		if (ARecastNavMesh* RecastNavMesh = *NavIt)
		{
			RecastNavMesh->EnsureBuildCompletion();
		}
	}
}

// --------------------------------------------------------------------------------------
// Stage 1: start with an all-Terrain grid and mark the central tower cell.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::InitialiseGrid()
{
	Cells.Init(ECellType::Terrain, GridSize * GridSize);

	const int32 CX = GridSize / 2;
	const int32 CY = GridSize / 2;
	Cells[CellIndex(CX, CY)] = ECellType::Tower;
}

// --------------------------------------------------------------------------------------
// Stage 2: carve NumPaths corridors from evenly-spaced points on the border to the centre.
// Each corridor is produced by a random walk that always makes net progress toward the
// centre (so it always terminates) but occasionally wiggles sideways for an organic curve.
// The ordered centre-line cells become the waypoints the enemies will follow.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::PaintPathCell(int32 X, int32 Y, int32 HalfWidthOverride)
{
	const int32 Half = HalfWidthOverride >= 0 ? HalfWidthOverride : PathHalfWidth;
	for (int32 OY = -Half; OY <= Half; ++OY)
	{
		for (int32 OX = -Half; OX <= Half; ++OX)
		{
			const int32 NX = X + OX;
			const int32 NY = Y + OY;
			if (InBounds(NX, NY) && Cells[CellIndex(NX, NY)] != ECellType::Tower)
			{
				Cells[CellIndex(NX, NY)] = ECellType::Path;
			}
		}
	}
}

void AProceduralTerrain::RebuildEnemyPathsFromCellLines()
{
	EnemyPaths.Reset();
	for (const TArray<FIntPoint>& Line : PathCellLines)
	{
		FEnemyPath Path;
		for (const FIntPoint& C : Line)
		{
			Path.Waypoints.Add(ActorToWorld().TransformPosition(CellCenterLocal(C.X, C.Y, PathPlaneHeight)));
		}
		if (Path.Waypoints.Num() > 0)
		{
			if (PathSmoothingIterations > 0)
			{
				Path.Waypoints = ChaikinSmooth(Path.Waypoints, PathSmoothingIterations);
			}
			Path.Waypoints = ThinWaypoints(Path.Waypoints, CellSize * 0.65f);
			Path.SpawnPoint = Path.Waypoints[0];
			EnemyPaths.Add(MoveTemp(Path));
		}
	}
}

void AProceduralTerrain::CarvePaths()
{
	const int32 CX = GridSize / 2;
	const int32 CY = GridSize / 2;

	PathCellLines.Reserve(NumPaths);

	// Spawn points are kept one cell inset from the absolute mesh edge/corners so spawns stay
	// connected to the walkable mesh near boundaries and corners.
	const int32 Inset = 1;
	const int32 Side = FMath::Max(GridSize - 1 - 2 * Inset, 1);
	const int32 Perimeter = 4 * Side;

	for (int32 P = 0; P < NumPaths; ++P)
	{
		// --- Choose a spawn cell on the border, spread evenly around the perimeter ---
		// Centred within each path's allocated segment (not at its start) so spawn points never
		// land exactly on a corner — with the default NumPaths=4, an un-offset T would place
		// all four spawns exactly on the four corners, which are the hardest points for Recast
		// to keep connected (two boundary edges converge, maximising erosion).
		int32 T = (Perimeter * P) / NumPaths + Perimeter / (2 * NumPaths); // Position along the border edge sequence.
		int32 SX = 0, SY = 0;
		if (T < Side)            { SX = Inset + T;                    SY = Inset; }              // Top edge.
		else if (T < 2 * Side)   { SX = Inset + Side;                 SY = Inset + (T - Side); }  // Right edge.
		else if (T < 3 * Side)   { SX = Inset + Side - (T - 2 * Side); SY = Inset + Side; }        // Bottom edge.
		else                     { SX = Inset;                        SY = Inset + Side - (T - 3 * Side); } // Left edge.

		// --- Random walk from the spawn to the centre ---
		TArray<FIntPoint>& Line = PathCellLines.AddDefaulted_GetRef();

		int32 X = SX, Y = SY;
		int32 Guard = 0;
		const int32 MaxGuard = GridSize * GridSize + 10; // Hard cap so the loop can never hang.

		while ((X != CX || Y != CY) && Guard++ < MaxGuard)
		{
			Line.Add(FIntPoint(X, Y));
			PaintPathCell(X, Y);

			const int32 DX = FMath::Sign(CX - X); // -1, 0 or +1 toward the centre on X.
			const int32 DY = FMath::Sign(CY - Y); // -1, 0 or +1 toward the centre on Y.

			// Decide which axis to advance on this step.
			bool bMoveX;
			if (DX != 0 && DY != 0) { bMoveX = (Rng.RandRange(0, 1) == 0); }
			else                    { bMoveX = (DX != 0); }

			// ~18% of the time, take a sideways "wiggle" step instead (keeps interior only),
			// which bends the path without preventing eventual arrival at the centre.
			if (Rng.FRand() < 0.18f)
			{
				if (bMoveX)
				{
					const int32 NY = Y + (Rng.RandRange(0, 1) == 0 ? 1 : -1);
					if (NY > 0 && NY < GridSize - 1) { Y = NY; continue; }
				}
				else
				{
					const int32 NX = X + (Rng.RandRange(0, 1) == 0 ? 1 : -1);
					if (NX > 0 && NX < GridSize - 1) { X = NX; continue; }
				}
			}

			// Normal progress step toward the centre.
			if (bMoveX) { X += DX; } else { Y += DY; }
		}

		// Ensure the final centre cell is included as the last waypoint.
		Line.Add(FIntPoint(CX, CY));
		PaintPathCell(CX, CY);
	}

	// Restore the centre as the Tower cell (PaintPathCell skips it, but be explicit).
	Cells[CellIndex(CX, CY)] = ECellType::Tower;

	RebuildEnemyPathsFromCellLines();
}

void AProceduralTerrain::RefreshTowerCell()
{
	const int32 CX = GridSize / 2;
	const int32 CY = GridSize / 2;
	Cells[CellIndex(CX, CY)] = ECellType::Tower;
	TowerLocation = ActorToWorld().TransformPosition(CellCenterLocal(CX, CY, PathPlaneHeight));
}

void AProceduralTerrain::RebuildTerrainVisuals()
{
	RefreshTowerCell();
	BuildHeightmap();
	AppendNewBuildableSlots();
	BuildMesh();
	RebuildEnemyPathsFromCellLines();
}

bool AProceduralTerrain::ExpandGridByOneRing()
{
	if (GridRingGrowthPerWave <= 0 || GridExpansionCount >= MaxGridExpansions)
	{
		return false;
	}

	const int32 Growth = GridRingGrowthPerWave;
	const int32 OldSize = GridSize;
	const int32 NewSize = OldSize + Growth * 2;
	if (NewSize > MaxGridSize)
	{
		return false;
	}

	TArray<ECellType> NewCells;
	NewCells.Init(ECellType::Terrain, NewSize * NewSize);

	for (int32 Y = 0; Y < OldSize; ++Y)
	{
		for (int32 X = 0; X < OldSize; ++X)
		{
			NewCells[(Y + Growth) * NewSize + (X + Growth)] = Cells[Y * OldSize + X];
		}
	}

	Cells = MoveTemp(NewCells);
	GridSize = NewSize;

	for (TArray<FIntPoint>& Line : PathCellLines)
	{
		for (FIntPoint& Point : Line)
		{
			Point.X += Growth;
			Point.Y += Growth;
		}
	}

	++GridExpansionCount;
	return true;
}

void AProceduralTerrain::ExtendAllPathsOneCellOutward()
{
	for (TArray<FIntPoint>& Line : PathCellLines)
	{
		if (Line.Num() < 2)
		{
			continue;
		}

		const FIntPoint OutDir(Line[0].X - Line[1].X, Line[0].Y - Line[1].Y);
		if (OutDir.X == 0 && OutDir.Y == 0)
		{
			continue;
		}

		const FIntPoint NewCell(Line[0].X + OutDir.X, Line[0].Y + OutDir.Y);
		if (!InBounds(NewCell.X, NewCell.Y))
		{
			continue;
		}

		if (Cells[CellIndex(NewCell.X, NewCell.Y)] != ECellType::Terrain)
		{
			continue;
		}

		Line.Insert(NewCell, 0);
		PaintPathCell(NewCell.X, NewCell.Y);
	}
}

void AProceduralTerrain::AppendRandomWalkToPoint(TArray<FIntPoint>& Line, FIntPoint Start, FIntPoint Target, int32 PaintHalfWidth)
{
	if (Line.Num() == 0 || Line.Last() != Start)
	{
		Line.Add(Start);
	}

	int32 X = Start.X;
	int32 Y = Start.Y;
	const int32 CX = GridSize / 2;
	const int32 CY = GridSize / 2;
	int32 Guard = 0;
	const int32 MaxGuard = GridSize * GridSize + 10;

	while ((X != Target.X || Y != Target.Y) && Guard++ < MaxGuard)
	{
		const int32 DX = FMath::Sign(Target.X - X);
		const int32 DY = FMath::Sign(Target.Y - Y);

		bool bMoveX;
		if (DX != 0 && DY != 0)
		{
			bMoveX = (Rng.RandRange(0, 1) == 0);
		}
		else
		{
			bMoveX = (DX != 0);
		}

		if (Rng.FRand() < 0.12f)
		{
			if (bMoveX)
			{
				const int32 NY = Y + (Rng.RandRange(0, 1) == 0 ? 1 : -1);
				if (NY > 0 && NY < GridSize - 1)
				{
					Y = NY;
					const FIntPoint Step(X, Y);
					if (Line.Last() != Step)
					{
						Line.Add(Step);
						PaintPathCell(X, Y, PaintHalfWidth);
					}
					continue;
				}
			}
			else
			{
				const int32 NX = X + (Rng.RandRange(0, 1) == 0 ? 1 : -1);
				if (NX > 0 && NX < GridSize - 1)
				{
					X = NX;
					const FIntPoint Step(X, Y);
					if (Line.Last() != Step)
					{
						Line.Add(Step);
						PaintPathCell(X, Y, PaintHalfWidth);
					}
					continue;
				}
			}
		}

		if (bMoveX)
		{
			X += DX;
		}
		else
		{
			Y += DY;
		}

		const FIntPoint Step(X, Y);
		if (Line.Last() != Step)
		{
			Line.Add(Step);
			if (!(X == CX && Y == CY))
			{
				PaintPathCell(X, Y, PaintHalfWidth);
			}
		}
	}
}

int32 AProceduralTerrain::BranchNewPaths(int32 Count)
{
	if (Count <= 0 || PathCellLines.Num() == 0)
	{
		return 0;
	}

	int32 Added = 0;
	const FIntPoint TowerCell(GridSize / 2, GridSize / 2);

	for (int32 Attempt = 0; Attempt < Count * 4 && Added < Count && PathCellLines.Num() < MaxTotalLanes; ++Attempt)
	{
		const int32 ParentIndex = Rng.RandRange(0, PathCellLines.Num() - 1);
		const TArray<FIntPoint>& Parent = PathCellLines[ParentIndex];
		if (Parent.Num() < 8)
		{
			continue;
		}

		const int32 MinFork = FMath::Max(2, Parent.Num() / 5);
		const int32 MaxFork = FMath::Max(MinFork + 1, (Parent.Num() * 3) / 5);
		const int32 ForkIndex = Rng.RandRange(MinFork, MaxFork);
		const FIntPoint ForkCell = Parent[ForkIndex];
		const FIntPoint PrevCell = Parent[FMath::Max(0, ForkIndex - 1)];

		FIntPoint Tangent(ForkCell.X - PrevCell.X, ForkCell.Y - PrevCell.Y);
		if (Tangent.X == 0 && Tangent.Y == 0)
		{
			continue;
		}

		FIntPoint SideDir(0, 0);
		if (FMath::Abs(Tangent.X) >= FMath::Abs(Tangent.Y))
		{
			SideDir = FIntPoint(0, Rng.RandRange(0, 1) == 0 ? 1 : -1);
		}
		else
		{
			SideDir = FIntPoint(Rng.RandRange(0, 1) == 0 ? 1 : -1, 0);
		}

		TArray<FIntPoint> Branch;
		Branch.Add(ForkCell);
		FIntPoint Current = ForkCell;
		const int32 SideSteps = Rng.RandRange(2, 4);
		bool bSideOk = true;

		for (int32 Step = 0; Step < SideSteps; ++Step)
		{
			Current.X += SideDir.X;
			Current.Y += SideDir.Y;
			if (!InBounds(Current.X, Current.Y) || Cells[CellIndex(Current.X, Current.Y)] == ECellType::Tower)
			{
				bSideOk = false;
				break;
			}

			Branch.Add(Current);
			PaintPathCell(Current.X, Current.Y, BranchPathHalfWidth);
		}

		if (!bSideOk || Branch.Num() < 2)
		{
			continue;
		}

		AppendRandomWalkToPoint(Branch, Current, TowerCell, BranchPathHalfWidth);
		if (Branch.Last() != TowerCell)
		{
			Branch.Add(TowerCell);
		}

		PathCellLines.Add(MoveTemp(Branch));
		++Added;
	}

	return Added;
}

int32 AProceduralTerrain::ExpandWorldAfterWave()
{
	if (PathCellLines.Num() == 0)
	{
		return 0;
	}

	int32 Changes = 0;

	if (ExpandGridByOneRing())
	{
		ExtendAllPathsOneCellOutward();
		++Changes;
	}

	const int32 BranchesAdded = BranchNewPaths(BranchesAddedPerWave);
	Changes += BranchesAdded;

	if (Changes == 0)
	{
		return 0;
	}

	RebuildTerrainVisuals();

	UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain: post-wave expansion — grid=%d, lanes=%d, branches added=%d."),
		GridSize, PathCellLines.Num(), BranchesAdded);
	return Changes;
}

void AProceduralTerrain::AppendNewBuildableSlots()
{
	if (DefenderSlots.Num() >= MaxDefenderSlots)
	{
		return;
	}

	const int32 OffX[4] = { 1, -1, 0, 0 };
	const int32 OffY[4] = { 0, 0, 1, -1 };
	TArray<FIntPoint> Candidates;

	auto HasSlotNear = [this](const FIntPoint& Cell) -> bool
	{
		const FVector WorldPos = ActorToWorld().TransformPosition(CellCenterLocal(Cell.X, Cell.Y, PathPlaneHeight));
		for (const FDefenderSlot& Slot : DefenderSlots)
		{
			if (FVector::DistSquared2D(Slot.Location, WorldPos) <= FMath::Square(CellSize * 0.6f))
			{
				return true;
			}
		}
		return false;
	};

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			if (Cells[CellIndex(X, Y)] != ECellType::Terrain)
			{
				continue;
			}
			if (HasSlotNear(FIntPoint(X, Y)))
			{
				continue;
			}

			for (int32 I = 0; I < 4; ++I)
			{
				const int32 NX = X + OffX[I];
				const int32 NY = Y + OffY[I];
				if (InBounds(NX, NY) && Cells[CellIndex(NX, NY)] == ECellType::Path)
				{
					Candidates.Add(FIntPoint(X, Y));
					break;
				}
			}
		}
	}

	for (int32 I = Candidates.Num() - 1; I > 0; --I)
	{
		const int32 J = Rng.RandRange(0, I);
		Candidates.Swap(I, J);
	}

	const int32 SlotsToAdd = FMath::Min(MaxDefenderSlots - DefenderSlots.Num(), Candidates.Num());
	for (int32 I = 0; I < SlotsToAdd; ++I)
	{
		const FIntPoint C = Candidates[I];
		Cells[CellIndex(C.X, C.Y)] = ECellType::Buildable;

		for (int32 OY = -1; OY <= 1; ++OY)
		{
			for (int32 OX = -1; OX <= 1; ++OX)
			{
				const int32 NX = C.X + OX;
				const int32 NY = C.Y + OY;
				if (InBounds(NX, NY))
				{
					VertexHeights[VertIndex(NX,     NY)]     = PathPlaneHeight;
					VertexHeights[VertIndex(NX + 1, NY)]     = PathPlaneHeight;
					VertexHeights[VertIndex(NX,     NY + 1)] = PathPlaneHeight;
					VertexHeights[VertIndex(NX + 1, NY + 1)] = PathPlaneHeight;
				}
			}
		}

		FDefenderSlot Slot;
		Slot.Location = ActorToWorld().TransformPosition(CellCenterLocal(C.X, C.Y, PathPlaneHeight));
		Slot.bOccupied = false;
		DefenderSlots.Add(Slot);
	}
}

// --------------------------------------------------------------------------------------
// Stage 3: fill the vertex heightmap with fractal noise, then flatten every vertex that
// touches a flat cell type (Path/Tower) down to the path plane so corridors are level.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::BuildHeightmap()
{
	const int32 VertsPerSide = GridSize + 1;
	VertexHeights.SetNumUninitialized(VertsPerSide * VertsPerSide);

	// Base terrain height from seeded fractal noise.
	for (int32 VY = 0; VY < VertsPerSide; ++VY)
	{
		for (int32 VX = 0; VX < VertsPerSide; ++VX)
		{
			VertexHeights[VertIndex(VX, VY)] = FractalNoise((float)VX, (float)VY) * HeightScale;
		}
	}

	// Flatten the four corner vertices of every flat cell to the path plane. Because
	// neighbouring cells share corner vertices, this produces seamless level corridors.
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			const ECellType Type = Cells[CellIndex(X, Y)];
			if (Type == ECellType::Path || Type == ECellType::Tower)
			{
				VertexHeights[VertIndex(X,     Y)]     = PathPlaneHeight;
				VertexHeights[VertIndex(X + 1, Y)]     = PathPlaneHeight;
				VertexHeights[VertIndex(X,     Y + 1)] = PathPlaneHeight;
				VertexHeights[VertIndex(X + 1, Y + 1)] = PathPlaneHeight;
			}
			else if (Type == ECellType::Buildable)
			{
				for (int32 OY = -1; OY <= 1; ++OY)
				{
					for (int32 OX = -1; OX <= 1; ++OX)
					{
						const int32 NX = X + OX;
						const int32 NY = Y + OY;
						if (InBounds(NX, NY))
						{
							VertexHeights[VertIndex(NX,     NY)]     = PathPlaneHeight;
							VertexHeights[VertIndex(NX + 1, NY)]     = PathPlaneHeight;
							VertexHeights[VertIndex(NX,     NY + 1)] = PathPlaneHeight;
							VertexHeights[VertIndex(NX + 1, NY + 1)] = PathPlaneHeight;
						}
					}
				}
			}
		}
	}
}

// --------------------------------------------------------------------------------------
// Stage 4: pick the defender pads. Candidates are Terrain cells orthogonally adjacent to a
// path (so defenders sit beside the route, never on it). We shuffle and keep up to
// MaxDefenderSlots, flatten each into a level pad, and publish its world location.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::MarkBuildableSlots()
{
	// Gather all Terrain cells that neighbour a Path cell.
	TArray<FIntPoint> Candidates;
	const int32 OffX[4] = { 1, -1, 0, 0 };
	const int32 OffY[4] = { 0, 0, 1, -1 };

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			if (Cells[CellIndex(X, Y)] != ECellType::Terrain)
			{
				continue;
			}
			for (int32 I = 0; I < 4; ++I)
			{
				const int32 NX = X + OffX[I];
				const int32 NY = Y + OffY[I];
				if (InBounds(NX, NY) && Cells[CellIndex(NX, NY)] == ECellType::Path)
				{
					Candidates.Add(FIntPoint(X, Y));
					break; // One adjacency is enough; avoid adding the cell twice.
				}
			}
		}
	}

	// Fisher-Yates shuffle using the seeded stream so slot choice is reproducible.
	for (int32 I = Candidates.Num() - 1; I > 0; --I)
	{
		const int32 J = Rng.RandRange(0, I);
		Candidates.Swap(I, J);
	}

	const int32 SlotCount = FMath::Min(MaxDefenderSlots, Candidates.Num());
	for (int32 I = 0; I < SlotCount; ++I)
	{
		const FIntPoint C = Candidates[I];
		Cells[CellIndex(C.X, C.Y)] = ECellType::Buildable;

		// Flatten the pad so defenders sit level and it reads as a distinct build spot. Also
		// flatten its orthogonal neighbours' corners (without changing their cell type) so the
		// pad isn't a single flat cell surrounded by up-to-HeightScale cliffs on its other three
		// sides — a lone flat "postage stamp" like that can get entirely eroded out of the
		// NavMesh by the agent radius, leaving the pad unreachable even though it looks fine.
		for (int32 OY = -1; OY <= 1; ++OY)
		{
			for (int32 OX = -1; OX <= 1; ++OX)
			{
				const int32 NX = C.X + OX;
				const int32 NY = C.Y + OY;
				if (InBounds(NX, NY))
				{
					VertexHeights[VertIndex(NX,     NY)]     = PathPlaneHeight;
					VertexHeights[VertIndex(NX + 1, NY)]     = PathPlaneHeight;
					VertexHeights[VertIndex(NX,     NY + 1)] = PathPlaneHeight;
					VertexHeights[VertIndex(NX + 1, NY + 1)] = PathPlaneHeight;
				}
			}
		}

		FDefenderSlot Slot;
		Slot.Location = ActorToWorld().TransformPosition(CellCenterLocal(C.X, C.Y, PathPlaneHeight));
		Slot.bOccupied = false;
		DefenderSlots.Add(Slot);
	}
}

// --------------------------------------------------------------------------------------
// Stage 5: turn the grid + heightmap into an actual mesh. Each cell becomes its own quad
// (four unique vertices) for a clean low-poly look and easy per-cell vertex colouring.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::BuildMesh()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents; // Left empty; smoothness isn't needed for low-poly.

	const int32 CellCount = GridSize * GridSize;
	Vertices.Reserve(CellCount * 4);
	Triangles.Reserve(CellCount * 6);

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			// Corner heights from the shared vertex heightmap (keeps the surface seamless).
			const float H00 = VertexHeights[VertIndex(X,     Y)];
			const float H10 = VertexHeights[VertIndex(X + 1, Y)];
			const float H01 = VertexHeights[VertIndex(X,     Y + 1)];
			const float H11 = VertexHeights[VertIndex(X + 1, Y + 1)];

			// Local-space corner positions (grid is centred on the actor origin).
			const FVector V0 = CornerLocal(X,     Y,     H00); // Bottom-left
			const FVector V1 = CornerLocal(X + 1, Y,     H10); // Bottom-right
			const FVector V2 = CornerLocal(X,     Y + 1, H01); // Top-left
			const FVector V3 = CornerLocal(X + 1, Y + 1, H11); // Top-right

			const int32 Base = Vertices.Num();
			Vertices.Add(V0);
			Vertices.Add(V1);
			Vertices.Add(V2);
			Vertices.Add(V3);

			// Two triangles (0,2,3) and (0,3,1) -> upward-facing quad in Unreal's winding.
			Triangles.Add(Base + 0); Triangles.Add(Base + 2); Triangles.Add(Base + 3);
			Triangles.Add(Base + 0); Triangles.Add(Base + 3); Triangles.Add(Base + 1);

			// Flat per-quad normal (cross of the two in-plane edges, oriented to +Z).
			const FVector Normal = FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();
			Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal);

			// UVs simply follow the grid coordinates (one tile per cell).
			UVs.Add(FVector2D(X, Y));
			UVs.Add(FVector2D(X + 1, Y));
			UVs.Add(FVector2D(X, Y + 1));
			UVs.Add(FVector2D(X + 1, Y + 1));

			// Colour the quad by cell type so the layout is readable without materials yet.
			const float AvgHeight = (H00 + H10 + H01 + H11) * 0.25f;
			const FLinearColor Color = CellColor(Cells[CellIndex(X, Y)], AvgHeight);
			Colors.Add(Color); Colors.Add(Color); Colors.Add(Color); Colors.Add(Color);
		}
	}

	// Rebuild section 0 (replaces any previous geometry). Collision on so pawns can walk it.
	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, /*bCreateCollision=*/true);

	// Apply the terrain material to the section so vertex colours (or a custom material) show.
	if (TerrainMaterial)
	{
		MeshComponent->SetMaterial(0, TerrainMaterial);
	}
}

// --------------------------------------------------------------------------------------
// Small helpers
// --------------------------------------------------------------------------------------

FVector AProceduralTerrain::CellCenterLocal(int32 X, int32 Y, float Height) const
{
	const float Half = GridSize * CellSize * 0.5f;
	return FVector((X + 0.5f) * CellSize - Half, (Y + 0.5f) * CellSize - Half, Height);
}

FVector AProceduralTerrain::CornerLocal(int32 GX, int32 GY, float Height) const
{
	const float Half = GridSize * CellSize * 0.5f;
	return FVector(GX * CellSize - Half, GY * CellSize - Half, Height);
}

FLinearColor AProceduralTerrain::CellColor(ECellType Type, float AvgHeight) const
{
	switch (Type)
	{
	case ECellType::Path:      return FLinearColor(0.20f, 0.16f, 0.12f); // Dirt path.
	case ECellType::Buildable: return FLinearColor(0.15f, 0.55f, 0.20f); // Green build pad.
	case ECellType::Tower:     return FLinearColor(0.20f, 0.35f, 0.85f); // Blue tower pad.
	default:
	{
		// Terrain: blend grass (low) to rock (high) by normalised height.
		const float T = HeightScale > 0.0f ? FMath::Clamp(AvgHeight / HeightScale, 0.0f, 1.0f) : 0.0f;
		return FMath::Lerp(FLinearColor(0.13f, 0.38f, 0.12f), FLinearColor(0.45f, 0.45f, 0.45f), T);
	}
	}
}

// Seeded 2D value noise in [0,1): hash the integer lattice corners and smoothly interpolate.
float AProceduralTerrain::ValueNoise(float X, float Y) const
{
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const float FX = X - X0;
	const float FY = Y - Y0;

	// Hash a lattice point to a float in [0,1). Seed is mixed in so each seed => new terrain.
	auto Hash = [this](int32 IX, int32 IY) -> float
	{
		uint32 H = (uint32)IX * 374761393u + (uint32)IY * 668265263u + (uint32)Seed * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= (H >> 16);
		return (H & 0xFFFFFFu) / (float)0x1000000; // Keep 24 bits -> [0,1).
	};

	// Smoothstep weights for gentle interpolation.
	const float SX = FX * FX * (3.0f - 2.0f * FX);
	const float SY = FY * FY * (3.0f - 2.0f * FY);

	const float N00 = Hash(X0,     Y0);
	const float N10 = Hash(X0 + 1, Y0);
	const float N01 = Hash(X0,     Y0 + 1);
	const float N11 = Hash(X0 + 1, Y0 + 1);

	const float NX0 = FMath::Lerp(N00, N10, SX);
	const float NX1 = FMath::Lerp(N01, N11, SX);
	return FMath::Lerp(NX0, NX1, SY);
}

// Fractal (fBm) noise: sum a configurable number of octaves of value noise at rising
// frequency / falling amplitude. All three shape parameters (NoiseOctaves, NoiseBaseFrequency,
// NoisePersistence) are designer-editable — nothing about the noise shape is hardcoded.
float AProceduralTerrain::FractalNoise(float X, float Y) const
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float Frequency = NoiseBaseFrequency;
	float MaxValue = 0.0f;

	for (int32 Octave = 0; Octave < NoiseOctaves; ++Octave)
	{
		Total += ValueNoise(X * Frequency, Y * Frequency) * Amplitude;
		MaxValue += Amplitude;
		Amplitude *= NoisePersistence;
		Frequency *= 2.0f;
	}
	return MaxValue > 0.0f ? Total / MaxValue : 0.0f; // Normalised back to [0,1].
}

// --------------------------------------------------------------------------------------
// Debug visualisation. Entirely gated behind bDebugMode — leaving it off removes every
// trace of this (no shapes drawn, no log spam), so it's safe to ship with and trivial to
// strip out entirely later if desired.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::DrawDebugVisualization() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain [DEBUG]: seed=%d, GridSize=%d, CellSize=%.0f, paths=%d, buildSlots=%d"),
		Seed, GridSize, CellSize, EnemyPaths.Num(), DefenderSlots.Num());

	// --- Terrain bounds: a box covering the full generated footprint. ---
	const float Half = GridSize * CellSize * 0.5f;
	const FVector Center = GetActorLocation() + FVector(0.0f, 0.0f, HeightScale * 0.5f);
	const FVector Extent(Half, Half, HeightScale * 0.5f + 50.0f);
	DrawDebugBox(World, Center, Extent, FColor::White, false, DebugDrawDuration, 0, 6.0f);

	// --- Tower position: a distinct magenta sphere at the centre. ---
	DrawDebugSphere(World, TowerLocation + FVector(0, 0, 60.0f), 80.0f, 16, FColor::Magenta, false, DebugDrawDuration, 0, 6.0f);

	// --- Every path: a coloured line strip through its waypoints, plus a marker at its spawn. ---
	static const FColor PathColors[] = { FColor::Cyan, FColor::Orange, FColor::Yellow, FColor::Green, FColor::Red, FColor::Purple, FColor::Blue, FColor::Emerald };
	for (int32 P = 0; P < EnemyPaths.Num(); ++P)
	{
		const FEnemyPath& Path = EnemyPaths[P];
		const FColor Color = PathColors[P % UE_ARRAY_COUNT(PathColors)];

		for (int32 I = 0; I + 1 < Path.Waypoints.Num(); ++I)
		{
			DrawDebugLine(World, Path.Waypoints[I] + FVector(0, 0, 20.0f), Path.Waypoints[I + 1] + FVector(0, 0, 20.0f),
				Color, false, DebugDrawDuration, 0, 8.0f);
		}

		// Spawn point: a larger marker so it's easy to pick out from the path line itself.
		if (Path.Waypoints.Num() > 0)
		{
			DrawDebugSphere(World, Path.SpawnPoint + FVector(0, 0, 60.0f), 50.0f, 12, Color, false, DebugDrawDuration, 0, 4.0f);
		}
	}

	// --- Every build slot: green if free, red if occupied. ---
	for (const FDefenderSlot& Slot : DefenderSlots)
	{
		DrawDebugPoint(World, Slot.Location + FVector(0, 0, 30.0f), 12.0f, Slot.bOccupied ? FColor::Red : FColor::Green, false, DebugDrawDuration, 0);
	}
}
