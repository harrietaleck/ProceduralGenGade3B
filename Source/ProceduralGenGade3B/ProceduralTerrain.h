// ProceduralTerrain.h
// Tower defence terrain that is built in code from a random seed, so every game gets a new map.
// It carves the enemy paths to the tower and gives the rest of the game the spawn points, waypoints and build pads.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "TerrainProp.h"
#include "ProceduralTerrain.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** What a grid cell is used for. This decides its colour, height and if you can build on it. */
UENUM(BlueprintType)
enum class ECellType : uint8
{
	Terrain    UMETA(DisplayName = "Terrain"),   // Normal ground, raised by noise. Enemies don't walk here.
	Path       UMETA(DisplayName = "Path"),       // Flat strip the enemies walk along.
	Buildable  UMETA(DisplayName = "Buildable"),  // Flat pad next to a path where a defender can go.
	Tower      UMETA(DisplayName = "Tower")        // The centre cell where the player's tower sits.
};

/**
 * One enemy route: where it spawns and the list of world points it walks to reach the tower.
 * Blueprint can read it so the spawner and enemies can use it.
 */
USTRUCT(BlueprintType)
struct FEnemyPath
{
	GENERATED_BODY()

	/** Where enemies on this route spawn. Same as the first waypoint. */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	FVector SpawnPoint = FVector::ZeroVector;

	/** World points from the spawn to the tower. Enemies walk them in order. */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	TArray<FVector> Waypoints;
};

/**
 * A build pad for a defender, with its location and whether something is on it.
 * The placement code sets bOccupied through the terrain, so we don't have to scan
 * every defender each frame to work it out.
 */
USTRUCT(BlueprintType)
struct FDefenderSlot
{
	GENERATED_BODY()

	/** World location of the pad, at ground level in the centre of the cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	FVector Location = FVector::ZeroVector;

	/** True while a defender is standing on this pad. */
	UPROPERTY(BlueprintReadOnly, Category = "Terrain")
	bool bOccupied = false;
};

UCLASS()
class PROCEDURALGENGADE3B_API AProceduralTerrain : public AActor
{
	GENERATED_BODY()

public:
	AProceduralTerrain();

	// ---- Generation settings you can tweak in the editor or Blueprint ----

	/** How many cells along each side of the square grid. Bigger number, bigger map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "8"))
	int32 GridSize = 40;

	/** Size of one square cell in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "50.0"))
	float CellSize = 200.0f;

	/** The highest the noise can push the ground above the path level, in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "0.0"))
	float HeightScale = 600.0f;

	/** How many layers of noise get added together for the height. More layers add finer
	 *  detail on top of the big shapes, but cost a little more to generate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	/** Starting noise frequency. A value of 1/N makes the biggest hills about N cells wide.
	 *  Smaller values give bigger, smoother hills. Larger values give choppier ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float NoiseBaseFrequency = 0.125f;

	/** How much weaker each noise layer is than the one before (0 to 1). Lower is smoother,
	 *  higher is rougher and more jagged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Noise", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float NoisePersistence = 0.5f;

	/** How many enemy paths to carve. The brief says we need at least three. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "3"))
	int32 NumPaths = 4;

	/** Half the width of a path in cells. 0 makes a one-cell path, 1 makes it three cells wide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "0", ClampMax = "3"))
	int32 PathHalfWidth = 1;

	/** Most defender pads we hand out, picked from all the valid spots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Defenders", meta = (ClampMin = "1"))
	int32 MaxDefenderSlots = 24;

	/** How many Chaikin smoothing passes to run on each path after it is carved.
	 *  0 keeps the blocky cell by cell line, higher values make it curvier.
	 *  The spawn point and tower point never move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "0", ClampMax = "5"))
	int32 PathSmoothingIterations = 1;

	/** How many cells the grid grows on each side after a wave is cleared. Lanes can also branch then. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "0"))
	int32 GridRingGrowthPerWave = 2;

	/** How many times the grid can grow during one match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "0"))
	int32 MaxGridExpansions = 8;

	/** The biggest the grid is allowed to get after growing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "8"))
	int32 MaxGridSize = 64;

	/** How many new branch lanes split off existing paths after each wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "0"))
	int32 BranchesAddedPerWave = 2;

	/** Most enemy lanes allowed in total, main paths plus branches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "3"))
	int32 MaxTotalLanes = 16;

	/** Half width for branch lanes. They are thinner than the main paths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Paths", meta = (ClampMin = "0", ClampMax = "3"))
	int32 BranchPathHalfWidth = 0;

	/** Material on the terrain mesh. By default it uses a vertex colour material so you can
	 *  see the path, build pad and tower colours. You can swap in your own material later. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Visual")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** If true, a new random seed is picked each game so every session is different. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Seed")
	bool bRandomizeSeedOnBeginPlay = true;

	/** Seed for all the randomness. The same seed always gives the same map, which helps with debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Seed")
	int32 Seed = 12345;

	/** If true, draws the terrain bounds, paths, tower, build slots and spawn points as debug
	 *  shapes and logs the seed. It is only for debugging, nothing else depends on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debug")
	bool bDebugMode = false;

	/** How many seconds the debug shapes stay on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debug", meta = (ClampMin = "0.0", EditCondition = "bDebugMode"))
	float DebugDrawDuration = 20.0f;

	// ---- Decorations (trees, rocks, buildings) ----

	/** If false, no props get scattered on the terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Decorations")
	bool bScatterDecorations = true;

	/** Chance from 0 to 1 that a free terrain cell gets a tree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Decorations", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TreeDensity = 0.07f;

	/** Chance from 0 to 1 that a free terrain cell gets a rock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Decorations", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RockDensity = 0.045f;

	/** Chance from 0 to 1 that a free terrain cell gets a building. Keep this low. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Decorations", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BuildingDensity = 0.018f;

	/** How many cells props must stay away from paths, build pads and the tower. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Decorations", meta = (ClampMin = "0"))
	int32 DecorationPathBufferCells = 2;

	/** Tree meshes to scatter. If empty, the trees and bushes from VRS_LowPolyNatureEssentials are loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Decorations|Meshes")
	TArray<TObjectPtr<UStaticMesh>> TreeMeshes;

	/** Rock, stump and log meshes. If empty, the VRS_LowPolyNatureEssentials rocks are loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Decorations|Meshes")
	TArray<TObjectPtr<UStaticMesh>> RockMeshes;

	/** Ruin, fence and prop meshes. If empty, the VRS_LowPolyNatureEssentials props are loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Decorations|Meshes")
	TArray<TObjectPtr<UStaticMesh>> BuildingMeshes;

	// ---- Generation functions ----

	/** Builds the whole terrain again from the current settings. There is a button for it in the Details panel. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terrain")
	void GenerateTerrain();

	/** Picks a new random seed and builds the terrain again. Useful for previewing in the editor. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terrain")
	void RandomizeAndRegenerate();

	/**
	 * The GameMode calls this once at the start of a match, before it reads any terrain data,
	 * so the map is freshly generated. We don't do this in BeginPlay because the GameMode
	 * might run first and read old terrain data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void PrepareForNewGame();

	/**
	 * Generates and checks the map NumIterations times, with a new random seed each time.
	 * It logs a pass or fail for every run and a summary at the end, so we can test lots
	 * of maps in a few seconds without starting Play each time.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terrain|Debug")
	void RunStressTest(int32 NumIterations = 20);

	/** Grows the grid, extends the spawns outward and adds branch lanes. Called after each wave. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	int32 ExpandWorldAfterWave();

	UFUNCTION(BlueprintPure, Category = "Terrain")
	int32 GetGridExpansionCount() const { return GridExpansionCount; }

	UFUNCTION(BlueprintPure, Category = "Terrain")
	int32 GetTotalLaneCount() const { return PathCellLines.Num(); }

	// ---- Getters the rest of the game uses (also work in Blueprint) ----

	/** World location of the tower cell, at the centre of its top surface. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	FVector GetTowerLocation() const { return TowerLocation; }

	/** All the enemy routes, each with a spawn point and waypoints. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	const TArray<FEnemyPath>& GetEnemyPaths() const { return EnemyPaths; }

	/** All the build pads next to the paths, and whether each one is taken. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	const TArray<FDefenderSlot>& GetDefenderSlots() const { return DefenderSlots; }

	/** Marks the slot closest to Location as taken or free. The placement code calls this when
	 *  a defender is placed and again when it is removed or destroyed. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void SetSlotOccupied(const FVector& Location, bool bOccupied);

	/** Returns true if the slot closest to Location is taken. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	bool IsSlotOccupied(const FVector& Location) const;

	/**
	 * A* search over the path and tower cells, from From to the tower.
	 * CellCost gets the world centre of a cell and returns how much it costs to walk through,
	 * at least 1. This lets callers make cells near defenders more expensive.
	 * Gives back smoothed world waypoints and the total cost of the route.
	 */
	bool FindLowestCostRoute(const FVector& From, TFunctionRef<float(const FVector&)> CellCost,
		TArray<FVector>& OutWaypoints, float& OutCost) const;

	/** Asks the NavMesh for a path from each spawn point to the tower, and from each build slot
	 *  to the tower, and logs how many work. The NavMesh has to be rebuilt first with
	 *  RebuildNavigation. It is public so other classes like TDGameMode can check it again. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	bool ValidatePathfinding() const;

	/** Rebuilds the NavMesh and waits until every tile is done. It is public so code that adds
	 *  new obstacles after generation, like TDGameMode spawning the tower, can update the
	 *  NavMesh before testing paths. With dynamic nav generation Unreal only queues the
	 *  update, it doesn't wait for it to finish. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void RebuildNavigation();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	/** The mesh that draws the terrain. */
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> MeshComponent;

	// ---- Generation data (rebuilt every time GenerateTerrain runs) ----

	/** Random stream seeded from Seed. All random choices use it so a seed always gives the same map. */
	FRandomStream Rng;

	/** Type of each cell, looked up with CellIndex(x,y). Holds GridSize * GridSize entries. */
	TArray<ECellType> Cells;

	/** Height of each vertex, looked up with VertIndex(x,y). Holds (GridSize+1) * (GridSize+1) entries. */
	TArray<float> VertexHeights;

	// Results that generation fills in and the getters above return.
	FVector TowerLocation = FVector::ZeroVector;
	UPROPERTY()
	TArray<FEnemyPath> EnemyPaths;
	UPROPERTY()
	TArray<FDefenderSlot> DefenderSlots;

	/** The middle line of cells for each path. We use it to extend and branch lanes between waves. */
	TArray<TArray<FIntPoint>> PathCellLines;

	/** How many times the grid has grown so far this match. */
	int32 GridExpansionCount = 0;

	UPROPERTY()
	TArray<TObjectPtr<ATerrainProp>> SpawnedDecorations;

	TSet<int32> DecoratedCellKeys;

	UStaticMesh* DefaultCylinderMesh = nullptr;
	UStaticMesh* DefaultCubeMesh = nullptr;
	UStaticMesh* DefaultSphereMesh = nullptr;

	void ClearDecorations();
	void ScatterDecorations();
	bool IsCellEligibleForDecoration(int32 X, int32 Y) const;
	float SampleCellSurfaceHeight(int32 X, int32 Y) const;
	UStaticMesh* PickDecorationMesh(ETerrainDecorationKind Kind);
	void SpawnDecorationAtCell(int32 X, int32 Y, ETerrainDecorationKind Kind);
	void EnsureDefaultDecorationMeshes();

	// ---- Generation steps ----
	void InitialiseGrid();
	void CarvePaths();
	void PaintPathCell(int32 X, int32 Y, int32 HalfWidthOverride = -1);
	void RebuildEnemyPathsFromCellLines();
	void AppendNewBuildableSlots();
	bool ExpandGridByOneRing();
	void ExtendAllPathsOneCellOutward();
	int32 BranchNewPaths(int32 Count);
	void AppendRandomWalkToPoint(TArray<FIntPoint>& Line, FIntPoint Start, FIntPoint Target, int32 PaintHalfWidth);
	void RefreshTowerCell();
	void RebuildTerrainVisuals();
	void BuildHeightmap();
	void MarkBuildableSlots();
	void BuildMesh();

	/** Checks the new map is playable. The grid data must be the right size, there must be at
	 *  least three paths that all reach the tower, and at least one build slot.
	 *  GenerateTerrain tries again with a new seed if this fails. */
	bool ValidateGeneratedWorld() const;

	/** Draws the terrain bounds, paths, tower, build slots and spawn points, and logs the seed.
	 *  Only runs when bDebugMode is on. */
	void DrawDebugVisualization() const;

	// ---- Small index and coordinate helpers ----

	/** Turns a cell coordinate into a 1D array index. X and Y must be inside the grid. */
	FORCEINLINE int32 CellIndex(int32 X, int32 Y) const { return Y * GridSize + X; }

	/** Turns a vertex coordinate into a 1D array index. The vertex grid is GridSize+1 wide. */
	FORCEINLINE int32 VertIndex(int32 X, int32 Y) const { return Y * (GridSize + 1) + X; }

	/** True if the cell coordinate is inside the grid. */
	FORCEINLINE bool InBounds(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < GridSize && Y < GridSize; }

	/** Centre of a cell in the actor's local space, at the given height. */
	FVector CellCenterLocal(int32 X, int32 Y, float Height) const;

	/** Position of a grid corner in the actor's local space, at the given height. */
	FVector CornerLocal(int32 GX, int32 GY, float Height) const;

	/** Vertex colour for a cell, so the layout is easy to see without a proper material. */
	FLinearColor CellColor(ECellType Type, float AvgHeight) const;

	/** Seeded value noise between 0 and 1, used to build the fractal terrain height. */
	float ValueNoise(float X, float Y) const;
	float FractalNoise(float X, float Y) const;
};
