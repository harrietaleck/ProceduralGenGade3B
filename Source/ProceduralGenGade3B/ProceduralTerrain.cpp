// ProceduralTerrain.cpp
// Builds the tower defence terrain. The steps run in this order: InitialiseGrid, CarvePaths,
// BuildHeightmap, MarkBuildableSlots, BuildMesh. Then the tower, paths and build slots are saved.

#include "ProceduralTerrain.h"
#include "ProceduralMeshComponent.h"
#include "TerrainProp.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Algo/Reverse.h"

// All the flat parts (paths, tower pad, build pads) sit at this local Z height.
static constexpr float PathPlaneHeight = 0.0f;

// Chaikin smoothing: each segment is replaced by two points at 1/4 and 3/4, which rounds off
// the blocky path a bit more every pass. The first and last points are put back after each
// pass so the spawn and tower ends don't move.
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

// Removes waypoints that are too close together. Otherwise enemies keep turning every few
// units, and smooth curves look more natural at walking speed.
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
	// The terrain doesn't change after it is built, so it doesn't need to tick.
	PrimaryActorTick.bCanEverTick = false;

	// The procedural mesh draws the terrain and also gives it collision.
	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	SetRootComponent(MeshComponent);
	// Cook collision straight away instead of in the background. Right after generation we
	// rebuild the NavMesh in the same frame, and if the collision wasn't ready yet the NavMesh
	// would have nothing to walk on.
	MeshComponent->bUseAsyncCooking = false;

	// Block everything so mouse traces for placing defenders hit the ground.
	// Query only is enough because nothing does physics on the terrain.
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// Use the engine's vertex colour material by default so the cell colours show up
	// without having to set up a material.
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

	// Build a preview in the editor from the current Seed so we can see and tweak the map
	// without pressing Play.
	GenerateTerrain();
}

void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	// We don't generate here because the GameMode's BeginPlay might run first and spawn the
	// tower and build pads from old terrain data. The GameMode calls PrepareForNewGame instead.
}

void AProceduralTerrain::RandomizeAndRegenerate()
{
	Seed = FMath::RandRange(1, MAX_int32 - 1);
	GenerateTerrain();
}

void AProceduralTerrain::PrepareForNewGame()
{
	// The brief says the map must be different every game, so pick a new seed.
	// You can turn this off to keep a fixed seed for testing.
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
		GenerateTerrain(); // This already retries with a new seed if the map fails its checks.
		const double ElapsedSeconds = FPlatformTime::Seconds() - StartTime;
		TotalSeconds += ElapsedSeconds;

		ValidatePathfinding(); // Only logs NavMesh info. It doesn't change pass or fail.
		const bool bPass = ValidateGeneratedWorld();
		PassCount += bPass ? 1 : 0;

		UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain [STRESS TEST] run %d/%d: seed=%d, paths=%d, buildSlots=%d, time=%.2fms -> %s"),
			I, NumIterations, Seed, EnemyPaths.Num(), DefenderSlots.Num(), ElapsedSeconds * 1000.0, bPass ? TEXT("PASS") : TEXT("FAIL"));
	}

	UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain [STRESS TEST] complete: %d/%d passed (%.1f%%), average time %.2fms."),
		PassCount, NumIterations, 100.0 * PassCount / NumIterations, 1000.0 * TotalSeconds / NumIterations);

	// Put the old seed back and generate once more so the level is playable again after the test.
	Seed = OriginalSeed;
	GenerateTerrain();
}

void AProceduralTerrain::GenerateTerrain()
{
	// Stop bad settings from breaking the array indexing or the brief's rules.
	GridSize = FMath::Max(8, GridSize);
	NumPaths = FMath::Max(3, NumPaths); // The brief says at least three paths.

	// The map should always come out valid, but we check it anyway and try a new seed if
	// something is wrong, so the game never starts on a broken map. The number of tries is
	// capped so a bug can't freeze the game. If they all fail we log an error and keep the
	// last attempt, which is better than having no terrain.
	const int32 MaxAttempts = 5;
	bool bValid = false;
	for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
	{
		// Seed the random stream so the same Seed always gives the same map.
		Rng.Initialize(Seed);

		// Clear out the data from the last map.
		EnemyPaths.Reset();
		DefenderSlots.Reset();
		PathCellLines.Reset();
		GridExpansionCount = 0;
		ClearDecorations();
		DecoratedCellKeys.Reset();

		// Run the steps in order. Each one needs the result of the one before.
		InitialiseGrid();
		CarvePaths();
		BuildHeightmap();
		MarkBuildableSlots();
		BuildMesh();

		// ---- Save the tower location in world space, at the centre cell ----
		const int32 CX = GridSize / 2;
		const int32 CY = GridSize / 2;
		TowerLocation = ActorToWorld().TransformPosition(CellCenterLocal(CX, CY, PathPlaneHeight));

		bValid = ValidateGeneratedWorld();
		if (bValid)
		{
			RebuildNavigation();
			ValidatePathfinding();
			ScatterDecorations();
		}

		if (bValid)
		{
			break;
		}

		if (Attempt < MaxAttempts)
		{
			UE_LOG(LogTemp, Warning, TEXT("ProceduralTerrain: generated world failed validation on attempt %d/%d (seed %d) - regenerating with a new seed."),
				Attempt, MaxAttempts, Seed);
		}
		Seed = FMath::RandRange(1, MAX_int32 - 1);
	}

	if (!bValid)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralTerrain: failed to generate a valid world after %d attempts - using the last attempt anyway."), MaxAttempts);
		RebuildNavigation(); // Keep the NavMesh matching whatever terrain we ended up with.
	}

	if (bDebugMode)
	{
		DrawDebugVisualization();
	}
}

bool AProceduralTerrain::ValidateGeneratedWorld() const
{
	// The grid and height arrays must be the right size, or we would read out of bounds later.
	if (Cells.Num() != GridSize * GridSize || VertexHeights.Num() != (GridSize + 1) * (GridSize + 1))
	{
		return false;
	}

	// We need at least three enemy paths.
	if (EnemyPaths.Num() < 3)
	{
		return false;
	}

	for (const FEnemyPath& Path : EnemyPaths)
	{
		// A path needs at least a start and an end point.
		if (Path.Waypoints.Num() < 2)
		{
			return false;
		}

		// Every path has to end at the tower or right next to it. We only check X and Y,
		// and allow about a cell and a half of slack for rounding.
		if (FVector::DistSquared2D(Path.Waypoints.Last(), TowerLocation) > FMath::Square(CellSize * 1.5f))
		{
			return false;
		}

		// Check for broken paths. Two waypoints in a row should never be more than two cells apart.
		// Smoothing only makes the gaps smaller, so it won't set this off by mistake.
		for (int32 I = 0; I + 1 < Path.Waypoints.Num(); ++I)
		{
			if (FVector::DistSquared2D(Path.Waypoints[I], Path.Waypoints[I + 1]) > FMath::Square(CellSize * 2.0f))
			{
				return false;
			}
		}
	}

	// There has to be at least one build slot, or the player can't do anything.
	if (DefenderSlots.Num() == 0)
	{
		return false;
	}

	// No two build slots should be less than a cell apart. Each one comes from its own cell
	// so this shouldn't happen, but the check will catch it if a later change breaks that.
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

	// This only logs info and never stops the game. Our enemies follow the Waypoints from
	// CarvePaths (see AEnemy::SetPath and Tick), not the NavMesh, so the checks that really
	// matter are in ValidateGeneratedWorld. These NavMesh queries can fail on a perfectly fine
	// map. For example, the tower's collision cuts a hole in the NavMesh, so the exact
	// TowerLocation point can look unreachable even though every path leads right up to it.
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
		UE_LOG(LogTemp, Verbose, TEXT("ProceduralTerrain: %d/%d enemy paths resolved a complete NavMesh route to the tower (non-blocking - enemies move via fixed waypoints, not NavMesh)."),
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
		UE_LOG(LogTemp, Verbose, TEXT("ProceduralTerrain: %d/%d build slots resolved a complete NavMesh route to the tower (non-blocking - defender placement doesn't use NavMesh)."),
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

	// Log a warning if no NavMeshBoundsVolume in the level covers the whole terrain, instead of
	// quietly making a map with no nav. Resizing the volume at runtime didn't work well because
	// its bounds didn't always update in PIE. So the level needs a volume that is already big
	// enough, like NavMeshBoundsVolume_0 in TowerDefense.umap.
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
		UE_LOG(LogTemp, Warning, TEXT("ProceduralTerrain: no NavMeshBoundsVolume in the level fully covers the current terrain footprint - enlarge NavMeshBoundsVolume_0 in the level to cover at least +/-%.0f uu horizontally and %.0f uu vertically."),
			Half, HeightScale);
	}

	// With dynamic runtime generation, FNavigationSystem::Build only queues the tile builds and
	// doesn't wait for them. If we didn't wait here, ValidatePathfinding would test a half built
	// NavMesh. EnsureBuildCompletion makes every tile finish before we return.
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
// Step 1: fill the grid with Terrain cells and mark the tower cell in the middle.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::InitialiseGrid()
{
	Cells.Init(ECellType::Terrain, GridSize * GridSize);

	const int32 CX = GridSize / 2;
	const int32 CY = GridSize / 2;
	Cells[CellIndex(CX, CY)] = ECellType::Tower;
}

// --------------------------------------------------------------------------------------
// Step 2: carve NumPaths paths from evenly spaced points on the edge to the centre.
// Each path is a random walk that keeps moving toward the centre, so it always gets there,
// but sometimes steps sideways so it bends a bit. The middle cells become the enemy waypoints.
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

	// Keep spawn points one cell in from the edge of the mesh so they stay connected to
	// the walkable ground near the edges and corners.
	const int32 Inset = 1;
	const int32 Side = FMath::Max(GridSize - 1 - 2 * Inset, 1);
	const int32 Perimeter = 4 * Side;

	for (int32 P = 0; P < NumPaths; ++P)
	{
		// --- Pick a spawn cell on the edge, spread evenly around the border ---
		// We use the middle of each path's share of the border, not the start. With 4 paths,
		// starting at 0 would put every spawn on a corner, and corners are where the NavMesh
		// is most likely to get cut off.
		int32 T = (Perimeter * P) / NumPaths + Perimeter / (2 * NumPaths); // How far along the border we are.
		int32 SX = 0, SY = 0;
		if (T < Side)            { SX = Inset + T;                    SY = Inset; }              // Top edge.
		else if (T < 2 * Side)   { SX = Inset + Side;                 SY = Inset + (T - Side); }  // Right edge.
		else if (T < 3 * Side)   { SX = Inset + Side - (T - 2 * Side); SY = Inset + Side; }        // Bottom edge.
		else                     { SX = Inset;                        SY = Inset + Side - (T - 3 * Side); } // Left edge.

		// --- Random walk from the spawn to the centre ---
		TArray<FIntPoint>& Line = PathCellLines.AddDefaulted_GetRef();

		int32 X = SX, Y = SY;
		int32 Guard = 0;
		const int32 MaxGuard = GridSize * GridSize + 10; // Limit so the loop can't run forever.

		while ((X != CX || Y != CY) && Guard++ < MaxGuard)
		{
			Line.Add(FIntPoint(X, Y));
			PaintPathCell(X, Y);

			const int32 DX = FMath::Sign(CX - X); // -1, 0 or +1 toward the centre on X.
			const int32 DY = FMath::Sign(CY - Y); // -1, 0 or +1 toward the centre on Y.

			// Pick which axis to move along this step.
			bool bMoveX;
			if (DX != 0 && DY != 0) { bMoveX = (Rng.RandRange(0, 1) == 0); }
			else                    { bMoveX = (DX != 0); }

			// About 18% of the time, step sideways instead, but never onto the outer edge.
			// This bends the path and it still reaches the centre in the end.
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

			// Normal step toward the centre.
			if (bMoveX) { X += DX; } else { Y += DY; }
		}

		// Add the centre cell as the last waypoint.
		Line.Add(FIntPoint(CX, CY));
		PaintPathCell(CX, CY);
	}

	// Set the centre back to Tower. PaintPathCell already skips it, this is just to be safe.
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
	ClearDecorations();
	DecoratedCellKeys.Reset();
	ScatterDecorations();

	UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain: post-wave expansion - grid=%d, lanes=%d, branches added=%d."),
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
// Step 3: fill the vertex heights with fractal noise, then flatten every vertex that touches
// a Path or Tower cell down to the path level so the paths are flat.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::BuildHeightmap()
{
	const int32 VertsPerSide = GridSize + 1;
	VertexHeights.SetNumUninitialized(VertsPerSide * VertsPerSide);

	// Starting ground height from the seeded fractal noise.
	for (int32 VY = 0; VY < VertsPerSide; ++VY)
	{
		for (int32 VX = 0; VX < VertsPerSide; ++VX)
		{
			VertexHeights[VertIndex(VX, VY)] = FractalNoise((float)VX, (float)VY) * HeightScale;
		}
	}

	// Flatten the four corners of every flat cell to the path level. Cells next to each
	// other share corners, so the paths come out level with no gaps.
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
// Step 4: pick the defender pads. We look for Terrain cells right next to a path, not
// diagonally, so defenders sit beside the path and never on it. We shuffle them, keep up to
// MaxDefenderSlots, flatten each one into a pad and save its world location.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::MarkBuildableSlots()
{
	// Collect every Terrain cell that touches a Path cell.
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
					break; // One path neighbour is enough. Stops us adding the cell twice.
				}
			}
		}
	}

	// Fisher-Yates shuffle with the seeded stream, so the same seed picks the same slots.
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

		// Flatten the pad so defenders stand level on it. We also flatten the cells around it,
		// without changing their type. A single flat cell with cliffs around it can get cut
		// out of the NavMesh completely, so the pad would be unreachable even if it looks fine.
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
// Step 5: turn the grid and heights into a mesh. Each cell gets its own quad with four
// vertices, which gives a low poly look and makes colouring each cell easy.
// --------------------------------------------------------------------------------------
void AProceduralTerrain::BuildMesh()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents; // Left empty. We don't need smooth shading for low poly.

	const int32 CellCount = GridSize * GridSize;
	Vertices.Reserve(CellCount * 4);
	Triangles.Reserve(CellCount * 6);

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			// Corner heights come from the shared vertex heights, so there are no gaps between cells.
			const float H00 = VertexHeights[VertIndex(X,     Y)];
			const float H10 = VertexHeights[VertIndex(X + 1, Y)];
			const float H01 = VertexHeights[VertIndex(X,     Y + 1)];
			const float H11 = VertexHeights[VertIndex(X + 1, Y + 1)];

			// Corner positions in local space. The grid is centred on the actor.
			const FVector V0 = CornerLocal(X,     Y,     H00); // Bottom-left
			const FVector V1 = CornerLocal(X + 1, Y,     H10); // Bottom-right
			const FVector V2 = CornerLocal(X,     Y + 1, H01); // Top-left
			const FVector V3 = CornerLocal(X + 1, Y + 1, H11); // Top-right

			const int32 Base = Vertices.Num();
			Vertices.Add(V0);
			Vertices.Add(V1);
			Vertices.Add(V2);
			Vertices.Add(V3);

			// Two triangles, (0,2,3) and (0,3,1), so the quad faces up with Unreal's winding order.
			Triangles.Add(Base + 0); Triangles.Add(Base + 2); Triangles.Add(Base + 3);
			Triangles.Add(Base + 0); Triangles.Add(Base + 3); Triangles.Add(Base + 1);

			// One flat normal for the whole quad, from the cross product of two edges. It points up.
			const FVector Normal = FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();
			Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal);

			// UVs just follow the grid, so the texture tiles once per cell.
			UVs.Add(FVector2D(X, Y));
			UVs.Add(FVector2D(X + 1, Y));
			UVs.Add(FVector2D(X, Y + 1));
			UVs.Add(FVector2D(X + 1, Y + 1));

			// Colour the quad by cell type so you can see the layout without a real material.
			const float AvgHeight = (H00 + H10 + H01 + H11) * 0.25f;
			const FLinearColor Color = CellColor(Cells[CellIndex(X, Y)], AvgHeight);
			Colors.Add(Color); Colors.Add(Color); Colors.Add(Color); Colors.Add(Color);
		}
	}

	// Rebuild section 0, replacing the old mesh. Collision is on so pawns can walk on it.
	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, /*bCreateCollision=*/true);

	// Put the terrain material on the section so the vertex colours or a custom material show up.
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
		// Terrain blends from grass when low to rock when high, based on height.
		const float T = HeightScale > 0.0f ? FMath::Clamp(AvgHeight / HeightScale, 0.0f, 1.0f) : 0.0f;
		return FMath::Lerp(FLinearColor(0.13f, 0.38f, 0.12f), FLinearColor(0.45f, 0.45f, 0.45f), T);
	}
	}
}

// Seeded 2D value noise from 0 to 1. We hash the four whole number corners and blend between them smoothly.
float AProceduralTerrain::ValueNoise(float X, float Y) const
{
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const float FX = X - X0;
	const float FY = Y - Y0;

	// Hash a grid point to a float from 0 to 1. The Seed is mixed in so each seed gives new terrain.
	auto Hash = [this](int32 IX, int32 IY) -> float
	{
		uint32 H = (uint32)IX * 374761393u + (uint32)IY * 668265263u + (uint32)Seed * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= (H >> 16);
		return (H & 0xFFFFFFu) / (float)0x1000000; // Keep 24 bits, which gives a value from 0 up to 1.
	};

	// Smoothstep weights so the blend is gentle.
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

// Fractal (fBm) noise. Adds up several layers of value noise, each one with double the
// frequency and less strength than the last. NoiseOctaves, NoiseBaseFrequency and
// NoisePersistence can all be changed in the editor.
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
	return MaxValue > 0.0f ? Total / MaxValue : 0.0f; // Scaled back to between 0 and 1.
}

void AProceduralTerrain::EnsureDefaultDecorationMeshes()
{
	if (!DefaultCylinderMesh)
	{
		DefaultCylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
	if (!DefaultCubeMesh)
	{
		DefaultCubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (!DefaultSphereMesh)
	{
		DefaultSphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}

	auto CompactNulls = [](TArray<TObjectPtr<UStaticMesh>>& Pool)
	{
		for (int32 Index = Pool.Num() - 1; Index >= 0; --Index)
		{
			if (!Pool[Index])
			{
				Pool.RemoveAt(Index);
			}
		}
	};

	CompactNulls(TreeMeshes);
	CompactNulls(RockMeshes);
	CompactNulls(BuildingMeshes);

	// Fill the mesh lists from VRS_LowPolyNatureEssentials if they don't have any pack meshes,
	// so props still use the nice meshes even if the arrays were cleared or hold placeholders.
	auto TryAddMesh = [](TArray<TObjectPtr<UStaticMesh>>& Pool, const TCHAR* Path)
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path))
		{
			Pool.AddUnique(Mesh);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ProceduralTerrain: failed to load decoration mesh '%s'"), Path);
		}
	};

	auto PoolHasNatureMesh = [](const TArray<TObjectPtr<UStaticMesh>>& Pool) -> bool
	{
		for (const TObjectPtr<UStaticMesh>& Mesh : Pool)
		{
			if (Mesh && Mesh->GetPathName().Contains(TEXT("/VRS_LowPolyNatureEssentials/")))
			{
				return true;
			}
		}
		return false;
	};

	if (!PoolHasNatureMesh(TreeMeshes))
	{
		TreeMeshes.Reset();
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/Oak/SM_OakAdultD.SM_OakAdultD"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/Birch/SM_BirchTreeAdultA.SM_BirchTreeAdultA"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/Birch/SM_BirchTreeYoungCSimple.SM_BirchTreeYoungCSimple"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/GenericTrees/SM_GenericTreeB.SM_GenericTreeB"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/GenericTrees/SM_GenericTreeD.SM_GenericTreeD"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/Pines/SM_PineVariantAMatureB.SM_PineVariantAMatureB"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/Pines/SM_PineVariantAGrowingB.SM_PineVariantAGrowingB"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/Pines/SM_PineVariantAYoungC.SM_PineVariantAYoungC"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/WeepingWillow/SM_WillowTreeAdultA.SM_WillowTreeAdultA"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/WitheredTrees/SM_WitheredTreeB.SM_WitheredTreeB"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/WitheredTrees/SM_SmallWitheredTreeB.SM_SmallWitheredTreeB"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Bushes/SM_BushA.SM_BushA"));
		TryAddMesh(TreeMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Bushes/SM_Bush8.SM_Bush8"));
	}

	if (!PoolHasNatureMesh(RockMeshes))
	{
		RockMeshes.Reset();
		TryAddMesh(RockMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Rocks/SM_RockBigC.SM_RockBigC"));
		TryAddMesh(RockMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Rocks/SM_RockNormD.SM_RockNormD"));
		TryAddMesh(RockMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Rocks/SM_RockBlueD.SM_RockBlueD"));
		TryAddMesh(RockMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Rocks/SM_RockSmallA.SM_RockSmallA"));
		TryAddMesh(RockMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Rocks/SM_RockSmallE.SM_RockSmallE"));
		TryAddMesh(RockMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/TreeStumps/SM_TreeStumpNormalC.SM_TreeStumpNormalC"));
		TryAddMesh(RockMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/TreeStumps/SM_TreeStumpShortD.SM_TreeStumpShortD"));
		TryAddMesh(RockMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Env/Trees/TreeLogs/SM_TreeLogOakB.SM_TreeLogOakB"));
	}

	if (!PoolHasNatureMesh(BuildingMeshes))
	{
		BuildingMeshes.Reset();
		TryAddMesh(BuildingMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Arch/RuinedWalls/SM_RuinedWallA.SM_RuinedWallA"));
		TryAddMesh(BuildingMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Arch/RuinedWalls/SM_RuinedWallRubbleA.SM_RuinedWallRubbleA"));
		TryAddMesh(BuildingMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Arch/Fence/SM_SWFModA.SM_SWFModA"));
		TryAddMesh(BuildingMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Arch/Fence/SM_SWFModShortA.SM_SWFModShortA"));
		TryAddMesh(BuildingMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Props/Campfire/SM_CampfireASmall.SM_CampfireASmall"));
		TryAddMesh(BuildingMeshes, TEXT("/Game/VRS_LowPolyNatureEssentials/Meshes/Props/Planks/SM_PlankStackB.SM_PlankStackB"));
	}

	UE_LOG(LogTemp, Display,
		TEXT("ProceduralTerrain: decoration pools ready (trees=%d rocks=%d buildings=%d)"),
		TreeMeshes.Num(), RockMeshes.Num(), BuildingMeshes.Num());
}

void AProceduralTerrain::ClearDecorations()
{
	for (ATerrainProp* Prop : SpawnedDecorations)
	{
		if (Prop)
		{
			Prop->Destroy();
		}
	}
	SpawnedDecorations.Reset();
}

float AProceduralTerrain::SampleCellSurfaceHeight(int32 X, int32 Y) const
{
	if (!InBounds(X, Y) || VertexHeights.Num() == 0)
	{
		return PathPlaneHeight;
	}

	const float H00 = VertexHeights[VertIndex(X, Y)];
	const float H10 = VertexHeights[VertIndex(X + 1, Y)];
	const float H01 = VertexHeights[VertIndex(X, Y + 1)];
	const float H11 = VertexHeights[VertIndex(X + 1, Y + 1)];
	return (H00 + H10 + H01 + H11) * 0.25f;
}

bool AProceduralTerrain::IsCellEligibleForDecoration(int32 X, int32 Y) const
{
	if (!InBounds(X, Y))
	{
		return false;
	}

	if (Cells[CellIndex(X, Y)] != ECellType::Terrain)
	{
		return false;
	}

	const int32 Buffer = FMath::Max(0, DecorationPathBufferCells);
	const int32 CX = GridSize / 2;
	const int32 CY = GridSize / 2;

	for (int32 DY = -Buffer; DY <= Buffer; ++DY)
	{
		for (int32 DX = -Buffer; DX <= Buffer; ++DX)
		{
			const int32 NX = X + DX;
			const int32 NY = Y + DY;
			if (!InBounds(NX, NY))
			{
				continue;
			}

			const ECellType Neighbour = Cells[CellIndex(NX, NY)];
			if (Neighbour == ECellType::Path || Neighbour == ECellType::Buildable || Neighbour == ECellType::Tower)
			{
				return false;
			}

			if (FMath::Abs(NX - CX) <= Buffer && FMath::Abs(NY - CY) <= Buffer)
			{
				return false;
			}
		}
	}

	return true;
}

UStaticMesh* AProceduralTerrain::PickDecorationMesh(ETerrainDecorationKind Kind)
{
	const TArray<TObjectPtr<UStaticMesh>>* Pool = nullptr;
	switch (Kind)
	{
	case ETerrainDecorationKind::Tree:     Pool = &TreeMeshes; break;
	case ETerrainDecorationKind::Rock:     Pool = &RockMeshes; break;
	case ETerrainDecorationKind::Building: Pool = &BuildingMeshes; break;
	default: break;
	}

	if (Pool && Pool->Num() > 0)
	{
		for (int32 Attempt = 0; Attempt < Pool->Num(); ++Attempt)
		{
			const int32 Index = Rng.RandRange(0, Pool->Num() - 1);
			if ((*Pool)[Index])
			{
				return (*Pool)[Index];
			}
		}

		for (const TObjectPtr<UStaticMesh>& Candidate : *Pool)
		{
			if (Candidate)
			{
				return Candidate;
			}
		}
	}

	switch (Kind)
	{
	case ETerrainDecorationKind::Tree:     return DefaultCylinderMesh;
	case ETerrainDecorationKind::Rock:     return DefaultSphereMesh;
	case ETerrainDecorationKind::Building: return DefaultCubeMesh;
	default: return DefaultCubeMesh;
	}
}

void AProceduralTerrain::SpawnDecorationAtCell(int32 X, int32 Y, ETerrainDecorationKind Kind)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	EnsureDefaultDecorationMeshes();

	const float SurfaceZ = SampleCellSurfaceHeight(X, Y);
	const FVector LocalPos = CellCenterLocal(X, Y, SurfaceZ);
	const FVector WorldPos = ActorToWorld().TransformPosition(LocalPos);
	const float Yaw = Rng.FRandRange(0.0f, 360.0f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	ATerrainProp* Prop = World->SpawnActor<ATerrainProp>(ATerrainProp::StaticClass(), WorldPos, FRotator::ZeroRotator, SpawnParams);
	if (!Prop)
	{
		return;
	}

	UStaticMesh* BaseMesh = PickDecorationMesh(Kind);
	UStaticMesh* AccentMesh = nullptr;
	FVector BaseScale = FVector::OneVector;
	FVector AccentOffset = FVector::ZeroVector;
	FVector AccentScale = FVector::OneVector;

	const bool bUsingPackTree = BaseMesh && BaseMesh->GetPathName().Contains(TEXT("/VRS_LowPolyNatureEssentials/"));
	const bool bUsingPackRock = BaseMesh && BaseMesh->GetPathName().Contains(TEXT("/VRS_LowPolyNatureEssentials/"));
	const bool bUsingPackBuilding = BaseMesh && BaseMesh->GetPathName().Contains(TEXT("/VRS_LowPolyNatureEssentials/"));

	switch (Kind)
	{
	case ETerrainDecorationKind::Tree:
	{
		if (bUsingPackTree)
		{
			// The pack trees are already the right size, so only change the scale a little and keep it even.
			const float TreeScale = Rng.FRandRange(0.75f, 1.25f);
			BaseScale = FVector(TreeScale);
		}
		else
		{
			// Old placeholder tree, a stretched cylinder trunk with a sphere on top.
			const float TrunkHeight = Rng.FRandRange(2.8f, 4.2f);
			const float TrunkRadius = Rng.FRandRange(0.35f, 0.55f);
			BaseScale = FVector(TrunkRadius, TrunkRadius, TrunkHeight);
			BaseMesh = DefaultCylinderMesh;
			AccentMesh = DefaultSphereMesh;
			AccentOffset = FVector(0.0f, 0.0f, 50.0f * TrunkHeight);
			AccentScale = FVector(Rng.FRandRange(1.8f, 2.6f));
		}
		break;
	}
	case ETerrainDecorationKind::Rock:
	{
		if (bUsingPackRock)
		{
			const float RockSize = Rng.FRandRange(0.7f, 1.35f);
			BaseScale = FVector(
				RockSize,
				RockSize * Rng.FRandRange(0.85f, 1.15f),
				RockSize * Rng.FRandRange(0.8f, 1.1f));
		}
		else
		{
			const float RockSize = Rng.FRandRange(0.9f, 1.8f);
			BaseScale = FVector(RockSize, RockSize * Rng.FRandRange(0.8f, 1.2f), RockSize * Rng.FRandRange(0.6f, 1.0f));
		}
		break;
	}
	case ETerrainDecorationKind::Building:
	{
		if (bUsingPackBuilding)
		{
			const float BuildingScale = Rng.FRandRange(0.85f, 1.2f);
			BaseScale = FVector(BuildingScale);
		}
		else
		{
			const float Width = Rng.FRandRange(1.4f, 2.4f);
			const float Depth = Rng.FRandRange(1.2f, 2.0f);
			const float Height = Rng.FRandRange(1.8f, 3.2f);
			BaseScale = FVector(Width, Depth, Height);
		}
		break;
	}
	default:
		break;
	}

	Prop->ConfigureDecoration(Kind, BaseMesh, BaseScale, Yaw, AccentMesh, AccentOffset, AccentScale);
	SpawnedDecorations.Add(Prop);
	DecoratedCellKeys.Add(CellIndex(X, Y));
}

void AProceduralTerrain::ScatterDecorations()
{
	if (!bScatterDecorations || Cells.Num() == 0)
	{
		return;
	}

	EnsureDefaultDecorationMeshes();
	const int32 CX = GridSize / 2;
	const int32 CY = GridSize / 2;

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			const int32 Key = CellIndex(X, Y);
			if (DecoratedCellKeys.Contains(Key))
			{
				continue;
			}

			if (!IsCellEligibleForDecoration(X, Y))
			{
				continue;
			}

			const float Roll = Rng.FRand();
			const int32 DistFromCentre = FMath::Max(FMath::Abs(X - CX), FMath::Abs(Y - CY));

			if (DistFromCentre >= GridSize / 5 && Roll < BuildingDensity)
			{
				SpawnDecorationAtCell(X, Y, ETerrainDecorationKind::Building);
				continue;
			}

			if (Roll < TreeDensity)
			{
				SpawnDecorationAtCell(X, Y, ETerrainDecorationKind::Tree);
				continue;
			}

			if (Roll < TreeDensity + RockDensity)
			{
				SpawnDecorationAtCell(X, Y, ETerrainDecorationKind::Rock);
			}
		}
	}

	UE_LOG(LogTemp, Display, TEXT("ProceduralTerrain: scattered %d decorations (trees/rocks/buildings)."), SpawnedDecorations.Num());
}

// --------------------------------------------------------------------------------------
// Debug drawing. It only runs when bDebugMode is on. With it off nothing is drawn or
// logged, so it is fine to leave in and easy to remove later.
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

	// --- Terrain bounds, a box around the whole map. ---
	const float Half = GridSize * CellSize * 0.5f;
	const FVector Center = GetActorLocation() + FVector(0.0f, 0.0f, HeightScale * 0.5f);
	const FVector Extent(Half, Half, HeightScale * 0.5f + 50.0f);
	DrawDebugBox(World, Center, Extent, FColor::White, false, DebugDrawDuration, 0, 6.0f);

	// --- Tower position, a magenta sphere in the centre. ---
	DrawDebugSphere(World, TowerLocation + FVector(0, 0, 60.0f), 80.0f, 16, FColor::Magenta, false, DebugDrawDuration, 0, 6.0f);

	// --- Each path gets a coloured line through its waypoints and a marker at its spawn. ---
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

		// Bigger marker for the spawn point so it stands out from the line.
		if (Path.Waypoints.Num() > 0)
		{
			DrawDebugSphere(World, Path.SpawnPoint + FVector(0, 0, 60.0f), 50.0f, 12, Color, false, DebugDrawDuration, 0, 4.0f);
		}
	}

	// --- Build slots are green when free and red when taken. ---
	for (const FDefenderSlot& Slot : DefenderSlots)
	{
		DrawDebugPoint(World, Slot.Location + FVector(0, 0, 30.0f), 12.0f, Slot.bOccupied ? FColor::Red : FColor::Green, false, DebugDrawDuration, 0);
	}
}

// --------------------------------------------------------------------------------------
// Routing that avoids defenders
// --------------------------------------------------------------------------------------

bool AProceduralTerrain::FindLowestCostRoute(const FVector& From, TFunctionRef<float(const FVector&)> CellCost,
	TArray<FVector>& OutWaypoints, float& OutCost) const
{
	OutWaypoints.Reset();
	OutCost = 0.0f;

	const int32 NumCells = GridSize * GridSize;
	if (NumCells <= 0 || Cells.Num() != NumCells)
	{
		return false;
	}

	auto IsWalkable = [this](int32 X, int32 Y)
	{
		if (!InBounds(X, Y))
		{
			return false;
		}
		const ECellType Type = Cells[CellIndex(X, Y)];
		return Type == ECellType::Path || Type == ECellType::Tower;
	};

	// Start from the walkable cell under the enemy, or the closest one to it.
	const FVector Local = ActorToWorld().InverseTransformPosition(From);
	const float Half = GridSize * CellSize * 0.5f;
	const int32 SX = FMath::FloorToInt((Local.X + Half) / CellSize);
	const int32 SY = FMath::FloorToInt((Local.Y + Half) / CellSize);

	FIntPoint Start(INDEX_NONE, INDEX_NONE);
	for (int32 Radius = 0; Radius <= 3 && Start.X == INDEX_NONE; ++Radius)
	{
		float BestDistSq = TNumericLimits<float>::Max();
		for (int32 DY = -Radius; DY <= Radius; ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius || !IsWalkable(SX + DX, SY + DY))
				{
					continue;
				}
				const float DistSq = static_cast<float>(DX * DX + DY * DY);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Start = FIntPoint(SX + DX, SY + DY);
				}
			}
		}
	}
	if (Start.X == INDEX_NONE)
	{
		return false;
	}

	const FIntPoint Goal(GridSize / 2, GridSize / 2);
	const int32 StartIndex = CellIndex(Start.X, Start.Y);
	const int32 GoalIndex = CellIndex(Goal.X, Goal.Y);

	TArray<float> CostSoFar;
	CostSoFar.Init(TNumericLimits<float>::Max(), NumCells);
	TArray<int32> CameFrom;
	CameFrom.Init(INDEX_NONE, NumCells);
	TArray<float> CachedCellCost;
	CachedCellCost.Init(-1.0f, NumCells);

	auto GetCellCost = [&](int32 X, int32 Y)
	{
		float& Cached = CachedCellCost[CellIndex(X, Y)];
		if (Cached < 0.0f)
		{
			const FVector Centre = ActorToWorld().TransformPosition(CellCenterLocal(X, Y, PathPlaneHeight));
			Cached = FMath::Max(1.0f, CellCost(Centre));
		}
		return Cached;
	};

	// Octile distance. It never overestimates because every cell costs at least 1.
	auto Heuristic = [&Goal](int32 X, int32 Y)
	{
		const int32 DX = FMath::Abs(X - Goal.X);
		const int32 DY = FMath::Abs(Y - Goal.Y);
		return static_cast<float>(FMath::Max(DX, DY)) + (UE_SQRT_2 - 1.0f) * FMath::Min(DX, DY);
	};

	struct FOpenNode
	{
		float Priority;
		int32 Index;
	};
	auto OpenOrder = [](const FOpenNode& A, const FOpenNode& B) { return A.Priority < B.Priority; };

	TArray<FOpenNode> Open;
	CostSoFar[StartIndex] = 0.0f;
	Open.HeapPush({ Heuristic(Start.X, Start.Y), StartIndex }, OpenOrder);

	static const int32 OffsetX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static const int32 OffsetY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

	while (Open.Num() > 0)
	{
		FOpenNode Node;
		Open.HeapPop(Node, OpenOrder, EAllowShrinking::No);
		if (Node.Index == GoalIndex)
		{
			break;
		}

		const int32 X = Node.Index % GridSize;
		const int32 Y = Node.Index / GridSize;
		if (Node.Priority > CostSoFar[Node.Index] + Heuristic(X, Y) + KINDA_SMALL_NUMBER)
		{
			continue; // Old entry, we already found a cheaper way here.
		}

		for (int32 Dir = 0; Dir < 8; ++Dir)
		{
			const int32 NX = X + OffsetX[Dir];
			const int32 NY = Y + OffsetY[Dir];
			if (!IsWalkable(NX, NY))
			{
				continue;
			}

			const bool bDiagonal = Dir >= 4;
			// No cutting corners. A diagonal step needs both side cells to be walkable too.
			if (bDiagonal && (!IsWalkable(NX, Y) || !IsWalkable(X, NY)))
			{
				continue;
			}

			const int32 NIndex = CellIndex(NX, NY);
			const float NewCost = CostSoFar[Node.Index] + GetCellCost(NX, NY) * (bDiagonal ? UE_SQRT_2 : 1.0f);
			if (NewCost < CostSoFar[NIndex])
			{
				CostSoFar[NIndex] = NewCost;
				CameFrom[NIndex] = Node.Index;
				Open.HeapPush({ NewCost + Heuristic(NX, NY), NIndex }, OpenOrder);
			}
		}
	}

	if (CameFrom[GoalIndex] == INDEX_NONE && GoalIndex != StartIndex)
	{
		return false;
	}

	TArray<FVector> Route;
	for (int32 Index = GoalIndex; Index != INDEX_NONE; Index = CameFrom[Index])
	{
		Route.Add(ActorToWorld().TransformPosition(CellCenterLocal(Index % GridSize, Index / GridSize, PathPlaneHeight)));
		if (Index == StartIndex)
		{
			break;
		}
	}
	Algo::Reverse(Route);

	if (PathSmoothingIterations > 0)
	{
		Route = ChaikinSmooth(Route, PathSmoothingIterations);
	}
	OutWaypoints = ThinWaypoints(Route, CellSize * 0.65f);
	OutCost = CostSoFar[GoalIndex];
	return true;
}
