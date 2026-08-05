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

AProceduralTerrain::AProceduralTerrain()
{
	// Terrain is static once generated, so we never need per-frame ticking.
	PrimaryActorTick.bCanEverTick = false;

	// The procedural mesh both renders the terrain and provides its collision.
	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->bUseAsyncCooking = true; // Cook collision off the game thread to avoid hitches.

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

		const bool bPass = ValidateGeneratedWorld() && ValidatePathfinding();
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
			// Only rebuild the (expensive) NavMesh once the geometry itself is valid, then
			// confirm AI can actually walk every path before accepting this attempt.
			RebuildNavigation();
			bValid = ValidatePathfinding();
			if (!bValid)
			{
				UE_LOG(LogTemp, Warning, TEXT("ProceduralTerrain: generated world passed geometry validation but failed AI pathfinding on attempt %d/%d (seed %d) — regenerating with a new seed."),
					Attempt, MaxAttempts, Seed);
			}
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

	// AI must actually be able to walk every path, not just have geometrically valid waypoints —
	// query the just-rebuilt NavMesh from each spawn point to the tower and require a complete
	// (non-partial) route.
	for (const FEnemyPath& Path : EnemyPaths)
	{
		if (Path.Waypoints.Num() == 0)
		{
			return false;
		}

		UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, Path.SpawnPoint, TowerLocation);
		if (!NavPath || !NavPath->IsValid() || NavPath->IsPartial())
		{
			return false;
		}
	}

	// Every build slot must also be reachable from the tower, not just the enemy paths.
	for (const FDefenderSlot& Slot : DefenderSlots)
	{
		UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, TowerLocation, Slot.Location);
		if (!NavPath || !NavPath->IsValid() || NavPath->IsPartial())
		{
			return false;
		}
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

	// Grow/reposition every NavMeshBoundsVolume in the level to fully cover whatever extent was
	// just generated. GridSize/CellSize/HeightScale are all designer-tunable, so a fixed,
	// hand-placed volume could silently stop covering the play area (missing NavMesh tiles at
	// the edges) the moment those values change. Resizing by scale factor — recomputed fresh
	// from the volume's current (already-scaled) bounds each call — works regardless of the
	// volume's originally-authored size and never compounds drift across repeated calls.
	const float Half = GridSize * CellSize * 0.5f;
	const FVector RequiredExtent(Half + CellSize * 2.0f, Half + CellSize * 2.0f, HeightScale * 0.5f + 300.0f);
	const FVector RequiredCenter = GetActorLocation() + FVector(0.0f, 0.0f, HeightScale * 0.5f);

	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		ANavMeshBoundsVolume* BoundsVolume = *It;

		// A hand-placed volume defaults to Static mobility, which silently rejects the
		// transform changes below (logs a warning, does nothing) — force it Movable so the
		// resize below actually takes effect.
		if (USceneComponent* Root = BoundsVolume->GetRootComponent())
		{
			Root->SetMobility(EComponentMobility::Movable);
		}

		const FVector CurrentScale = BoundsVolume->GetActorScale3D();
		const FVector SafeCurrentScale(FMath::Max(FMath::Abs(CurrentScale.X), KINDA_SMALL_NUMBER),
			FMath::Max(FMath::Abs(CurrentScale.Y), KINDA_SMALL_NUMBER), FMath::Max(FMath::Abs(CurrentScale.Z), KINDA_SMALL_NUMBER));
		const FVector CurrentExtent = BoundsVolume->GetComponentsBoundingBox(/*bNonColliding=*/true).GetExtent();
		const FVector UnscaledExtent(FMath::Max(CurrentExtent.X / SafeCurrentScale.X, 1.0f),
			FMath::Max(CurrentExtent.Y / SafeCurrentScale.Y, 1.0f), FMath::Max(CurrentExtent.Z / SafeCurrentScale.Z, 1.0f));

		BoundsVolume->SetActorScale3D(RequiredExtent / UnscaledExtent);
		BoundsVolume->SetActorLocation(RequiredCenter);
	}

	// Forces a full, synchronous NavMesh rebuild so navigation data is always current with
	// whatever terrain (and NavMeshBoundsVolume extent) was just generated.
	FNavigationSystem::Build(*World);
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
void AProceduralTerrain::CarvePaths()
{
	const int32 CX = GridSize / 2;
	const int32 CY = GridSize / 2;

	// Records, per path, the ordered centre-line cells (used later to build world waypoints).
	TArray<TArray<FIntPoint>> PathCellLines;
	PathCellLines.Reserve(NumPaths);

	// Total number of cells around the border; we place spawn points evenly along it.
	const int32 Perimeter = 4 * (GridSize - 1);

	// Helper: paint a cell (and PathHalfWidth neighbours) as Path, never overwriting the tower.
	auto PaintPath = [this](int32 X, int32 Y)
	{
		for (int32 OY = -PathHalfWidth; OY <= PathHalfWidth; ++OY)
		{
			for (int32 OX = -PathHalfWidth; OX <= PathHalfWidth; ++OX)
			{
				const int32 NX = X + OX;
				const int32 NY = Y + OY;
				if (InBounds(NX, NY) && Cells[CellIndex(NX, NY)] != ECellType::Tower)
				{
					Cells[CellIndex(NX, NY)] = ECellType::Path;
				}
			}
		}
	};

	for (int32 P = 0; P < NumPaths; ++P)
	{
		// --- Choose a spawn cell on the border, spread evenly around the perimeter ---
		int32 T = (Perimeter * P) / NumPaths; // Position along the border edge sequence.
		int32 SX = 0, SY = 0;
		const int32 Side = GridSize - 1;
		if (T < Side)            { SX = T;            SY = 0; }            // Top edge.
		else if (T < 2 * Side)   { SX = Side;         SY = T - Side; }     // Right edge.
		else if (T < 3 * Side)   { SX = Side - (T - 2 * Side); SY = Side; } // Bottom edge.
		else                     { SX = 0;            SY = Side - (T - 3 * Side); } // Left edge.

		// --- Random walk from the spawn to the centre ---
		TArray<FIntPoint>& Line = PathCellLines.AddDefaulted_GetRef();

		int32 X = SX, Y = SY;
		int32 Guard = 0;
		const int32 MaxGuard = GridSize * GridSize + 10; // Hard cap so the loop can never hang.

		while ((X != CX || Y != CY) && Guard++ < MaxGuard)
		{
			Line.Add(FIntPoint(X, Y));
			PaintPath(X, Y);

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
		PaintPath(CX, CY);
	}

	// Restore the centre as the Tower cell (PaintPath skips it, but be explicit).
	Cells[CellIndex(CX, CY)] = ECellType::Tower;

	// --- Convert the centre-line cells into world-space enemy paths ---
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
			Path.SpawnPoint = Path.Waypoints[0];
			EnemyPaths.Add(MoveTemp(Path));
		}
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

		// Flatten the pad so defenders sit level and it reads as a distinct build spot.
		VertexHeights[VertIndex(C.X,     C.Y)]     = PathPlaneHeight;
		VertexHeights[VertIndex(C.X + 1, C.Y)]     = PathPlaneHeight;
		VertexHeights[VertIndex(C.X,     C.Y + 1)] = PathPlaneHeight;
		VertexHeights[VertIndex(C.X + 1, C.Y + 1)] = PathPlaneHeight;

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
