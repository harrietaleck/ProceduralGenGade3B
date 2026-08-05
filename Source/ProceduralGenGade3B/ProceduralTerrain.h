// ProceduralTerrain.h
// A runtime-generated tower-defence terrain.
//
// Responsibilities:
//   * Build a 3D grid mesh entirely in code (no pre-authored landscape), so the map
//     is different every time the game starts (driven by a random seed).
//   * Carve at least three walkable PATHS from the map edges to a central TOWER point.
//   * Publish the data the rest of the game needs: enemy spawn points, the ordered
//     path waypoints enemies walk along, the tower location, and the buildable
//     locations where the player may place defenders (never on the path).
//
// The mesh itself is drawn with a UProceduralMeshComponent. We use one vertex-per-cell-corner
// (a low-poly / faceted look) which makes per-cell colouring and path flattening simple.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "ProceduralTerrain.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/** What a single grid cell is used for. Drives colour, height flattening and buildability. */
UENUM(BlueprintType)
enum class ECellType : uint8
{
	Terrain    UMETA(DisplayName = "Terrain"),   // Default decorative ground (raised by noise, not walkable).
	Path       UMETA(DisplayName = "Path"),       // Flattened corridor the enemies walk along.
	Buildable  UMETA(DisplayName = "Buildable"),  // Flat pad next to a path where a defender may be placed.
	Tower      UMETA(DisplayName = "Tower")        // The single central cell that holds the player's tower.
};

/**
 * One enemy route through the map: the spawn point plus the ordered list of world-space
 * waypoints leading to the tower. Exposed to Blueprint so the spawner/enemies can read it.
 */
USTRUCT(BlueprintType)
struct FEnemyPath
{
	GENERATED_BODY()

	/** World location where enemies for this route spawn (first waypoint). */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	FVector SpawnPoint = FVector::ZeroVector;

	/** Ordered world-space points from the spawn point to the tower. Enemies walk these in order. */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	TArray<FVector> Waypoints;
};

/**
 * A single procedurally-placed defender build pad: its world location plus whether a defender
 * currently stands on it. Occupancy is explicit, persisted state set by the placement flow
 * (ATDPlayerController::SetSlotOccupied via the terrain) rather than re-derived every frame by
 * scanning all placed defenders.
 */
USTRUCT(BlueprintType)
struct FDefenderSlot
{
	GENERATED_BODY()

	/** World-space location of the pad (ground level, cell centre). */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	FVector Location = FVector::ZeroVector;

	/** True while a defender occupies this pad. */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	bool bOccupied = false;
};

UCLASS()
class PROCEDURALGENGADE3B_API AProceduralTerrain : public AActor
{
	GENERATED_BODY()

public:
	AProceduralTerrain();

	// ---- Tunable generation parameters (editable per-instance in the editor / Blueprint) ----

	/** Number of cells along each side of the square grid. Larger = bigger, more detailed map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "8"))
	int32 GridSize = 40;

	/** World size (Unreal units) of one square cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "50.0"))
	float CellSize = 200.0f;

	/** Maximum height (uu) that noise can raise a terrain vertex above the path plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "0.0"))
	float HeightScale = 600.0f;

	/** Number of fractal-noise octaves summed for the height field. More octaves = more fine
	 *  detail layered on top of the broad shape, at a small extra generation cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	/** Base noise frequency: 1/N means the broadest terrain feature is roughly N cells across.
	 *  Smaller values = larger, smoother hills; larger values = tighter, choppier terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float NoiseBaseFrequency = 0.125f;

	/** How much each successive octave's amplitude shrinks (0..1). Lower = smoother overall;
	 *  higher = rougher, more jagged detail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float NoisePersistence = 0.5f;

	/** How many separate enemy paths to carve. The brief requires at least three. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "3"))
	int32 NumPaths = 4;

	/** Half-width of a carved path, in cells (0 = single-cell path, 1 = three-cell-wide, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "0", ClampMax = "3"))
	int32 PathHalfWidth = 1;

	/** Maximum number of buildable defender pads to expose (chosen from all valid candidates). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Defenders", meta = (ClampMin = "1"))
	int32 MaxDefenderSlots = 24;

	/** Chaikin corner-cutting passes applied to each path's waypoints after the raw random walk
	 *  is carved. 0 = the raw, blocky cell-by-cell line; higher values round it into a smoother
	 *  curve. The spawn point and tower point are always kept exact regardless of this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "0", ClampMax = "5"))
	int32 PathSmoothingIterations = 2;

	/** Material applied to the terrain mesh. Defaults to a vertex-colour material so the
	 *  path/buildable/tower cell colours are visible. Assignable to a custom material later. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Visual")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** When true, BeginPlay picks a brand-new random seed so every play session is different. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Seed")
	bool bRandomizeSeedOnBeginPlay = true;

	/** The seed used to drive all randomness. Same seed => identical map (useful for debugging). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Seed")
	int32 Seed = 12345;

	/** When true, draws the terrain bounds, every path, the tower, build slots and spawn
	 *  points as debug shapes, and logs the seed. Purely diagnostic — safe to leave off;
	 *  toggling it off removes every trace of the visualisation (nothing else depends on it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debug")
	bool bDebugMode = false;

	/** How long (seconds) the debug shapes persist once drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debug", meta = (ClampMin = "0.0", EditCondition = "bDebugMode"))
	float DebugDrawDuration = 20.0f;

	// ---- Generation entry points ----

	/** Regenerates the whole terrain from the current parameters. Callable from the editor Details panel. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terrain")
	void GenerateTerrain();

	/** Picks a new random seed and regenerates. Handy as an editor preview button. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terrain")
	void RandomizeAndRegenerate();

	/**
	 * The single, explicit entry point a match's GameMode must call — once, before reading
	 * any terrain data — to guarantee this actor's world is freshly generated and current.
	 * Deliberately NOT done automatically in BeginPlay(): Unreal does not guarantee this
	 * actor's BeginPlay runs before GameMode's, so leaving generation implicit there was a
	 * latent ordering bug (GameMode could have read stale, pre-randomisation data). Making
	 * the trigger explicit removes that ambiguity entirely.
	 */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void PrepareForNewGame();

	/**
	 * Runs the full generate-and-validate pipeline NumIterations times in a row (a fresh
	 * random seed each time), logging a pass/fail line per run and an aggregate summary at
	 * the end. Lets the generation pipeline be stress-tested many times over in seconds,
	 * without needing a separate PIE session per run. Callable from the editor Details panel.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terrain|Debug")
	void RunStressTest(int32 NumIterations = 20);

	// ---- Data queries used by the rest of the game (Blueprint-friendly) ----

	/** World-space location of the tower cell (top surface, centre). */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	FVector GetTowerLocation() const { return TowerLocation; }

	/** All enemy routes (spawn point + waypoints). */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	const TArray<FEnemyPath>& GetEnemyPaths() const { return EnemyPaths; }

	/** All buildable pads (flat spots beside paths, never on a path) with their occupied state. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	const TArray<FDefenderSlot>& GetDefenderSlots() const { return DefenderSlots; }

	/** Marks the slot nearest to Location as occupied/free. Called by the placement flow when a
	 *  defender is placed, and when one is removed/destroyed, so occupancy is always current. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void SetSlotOccupied(const FVector& Location, bool bOccupied);

	/** Whether the slot nearest to Location is currently occupied. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	bool IsSlotOccupied(const FVector& Location) const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	/** The mesh that renders the generated terrain. */
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> MeshComponent;

	// ---- Internal generation state (rebuilt each GenerateTerrain call) ----

	/** Random source seeded from Seed; all "random" choices go through this for reproducibility. */
	FRandomStream Rng;

	/** Per-cell type, indexed by CellIndex(x,y). Size = GridSize * GridSize. */
	TArray<ECellType> Cells;

	/** Per-vertex height, indexed by VertIndex(x,y). Size = (GridSize+1) * (GridSize+1). */
	TArray<float> VertexHeights;

	// Published results (filled by generation, read by getters above).
	FVector TowerLocation = FVector::ZeroVector;
	UPROPERTY()
	TArray<FEnemyPath> EnemyPaths;
	UPROPERTY()
	TArray<FDefenderSlot> DefenderSlots;

	// ---- Generation helper stages ----
	void InitialiseGrid();
	void CarvePaths();
	void BuildHeightmap();
	void MarkBuildableSlots();
	void BuildMesh();

	/** Checks the just-generated world is actually playable: grid data intact, at least
	 *  three paths, every path's last waypoint actually reaches the tower, and at least one
	 *  build slot exists. GenerateTerrain() regenerates automatically if this ever fails. */
	bool ValidateGeneratedWorld() const;

	/** Runs synchronous NavMesh pathfinding queries from every path's spawn point to the tower,
	 *  and from the tower to every build slot, failing if any query is unreachable or only
	 *  partially successful. Requires the NavMesh to already be rebuilt (RebuildNavigation)
	 *  against the current geometry before being called. */
	bool ValidatePathfinding() const;

	/** Grows/repositions the level's NavMesh bounds volume to cover the freshly generated
	 *  terrain and forces a synchronous NavMesh rebuild, so navigation is always current. */
	void RebuildNavigation();

	/** Draws the terrain bounds, every path, the tower, every build slot, every spawn point,
	 *  and logs the seed — only when bDebugMode is enabled. */
	void DrawDebugVisualization() const;

	// ---- Small index / coordinate helpers ----

	/** Flatten a cell coordinate to a 1D index. Assumes 0 <= x,y < GridSize. */
	FORCEINLINE int32 CellIndex(int32 X, int32 Y) const { return Y * GridSize + X; }

	/** Flatten a vertex coordinate to a 1D index. Vertex grid is (GridSize+1) wide. */
	FORCEINLINE int32 VertIndex(int32 X, int32 Y) const { return Y * (GridSize + 1) + X; }

	/** True if a cell coordinate is inside the grid. */
	FORCEINLINE bool InBounds(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < GridSize && Y < GridSize; }

	/** Centre of a cell in the actor's local space, at the given height. */
	FVector CellCenterLocal(int32 X, int32 Y, float Height) const;

	/** Corner (grid-vertex) position in the actor's local space, at the given height. */
	FVector CornerLocal(int32 GX, int32 GY, float Height) const;

	/** Vertex colour for a cell, used to visualise the layout before materials are applied. */
	FLinearColor CellColor(ECellType Type, float AvgHeight) const;

	/** Deterministic value-noise in [0,1] for fractal terrain height (seeded, tileable-free). */
	float ValueNoise(float X, float Y) const;
	float FractalNoise(float X, float Y) const;
};
