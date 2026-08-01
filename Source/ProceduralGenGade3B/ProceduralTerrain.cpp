// ProceduralTerrain.cpp
// Implementation of the runtime tower-defence terrain generator. See ProceduralTerrain.h
// for the high-level description. Generation runs as an ordered pipeline:
//   InitialiseGrid -> CarvePaths -> BuildHeightmap -> MarkBuildableSlots -> BuildMesh
// followed by publishing the tower location, enemy paths and defender slots.

#include "ProceduralTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Everything flat (paths, tower pad, buildable pads) sits on this Z plane in local space.
static constexpr float PathPlaneHeight = 0.0f;

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

	// The brief requires the terrain to differ every new game: pick a fresh random
	// seed at play time (unless the designer pinned a fixed seed for testing).
	if (bRandomizeSeedOnBeginPlay)
	{
		Seed = FMath::RandRange(1, MAX_int32 - 1);
	}
	GenerateTerrain();
}

void AProceduralTerrain::RandomizeAndRegenerate()
{
	Seed = FMath::RandRange(1, MAX_int32 - 1);
	GenerateTerrain();
}

void AProceduralTerrain::GenerateTerrain()
{
	// Guard against invalid parameters that would break indexing or the brief's rules.
	GridSize = FMath::Max(8, GridSize);
	NumPaths = FMath::Max(3, NumPaths); // The brief mandates at least three paths.

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

		DefenderSlots.Add(ActorToWorld().TransformPosition(CellCenterLocal(C.X, C.Y, PathPlaneHeight)));
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

// Fractal (fBm) noise: sum a few octaves of value noise at rising frequency / falling amplitude.
float AProceduralTerrain::FractalNoise(float X, float Y) const
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float Frequency = 1.0f / 8.0f; // Base feature size ~8 cells across.
	float MaxValue = 0.0f;

	for (int32 Octave = 0; Octave < 4; ++Octave)
	{
		Total += ValueNoise(X * Frequency, Y * Frequency) * Amplitude;
		MaxValue += Amplitude;
		Amplitude *= 0.5f;
		Frequency *= 2.0f;
	}
	return MaxValue > 0.0f ? Total / MaxValue : 0.0f; // Normalised back to [0,1].
}
