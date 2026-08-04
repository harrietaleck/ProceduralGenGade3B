// EnemySpawner.h
// Spawns enemies at the terrain's path spawn points on a fixed interval, cycling through
// the paths so every route is used. Each spawned enemy is handed its path waypoints and
// the tower to target. The game mode creates and initialises one of these.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class AProceduralTerrain;
class AEnemy;

UCLASS()
class PROCEDURALGENGADE3B_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	/** Which enemy class to spawn (defaults to the C++ AEnemy; can be a Blueprint child). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AEnemy> EnemyClass;

	/** Seconds between spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (ClampMin = "0.1"))
	float SpawnInterval = 2.0f;

	/** Optional cap on simultaneously-alive enemies (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (ClampMin = "0"))
	int32 MaxEnemiesAlive = 0;

	/** If true, begin spawning as soon as Initialize() is called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	bool bAutoStart = true;

	/** Provide the terrain (paths) and the tower (target), then optionally start spawning. */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void Initialize(AProceduralTerrain* InTerrain, AActor* InTower);

	/** Start the repeating spawn timer. */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void StartSpawning();

	/** Stop spawning (e.g. on game over). */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void StopSpawning();

	/**
	 * Spawn exactly one enemy on the next path in rotation and return it (may be null if it
	 * couldn't spawn, e.g. no terrain or the live cap is reached). This is the single unit of
	 * spawning; both the built-in timer and the WaveManager go through it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	AEnemy* SpawnSingleEnemy();

protected:
	/** Timer callback used by StartSpawning(): just spawns one enemy per tick. */
	void SpawnEnemy();

private:
	UPROPERTY()
	TObjectPtr<AProceduralTerrain> Terrain;

	UPROPERTY()
	TObjectPtr<AActor> Tower;

	/** Which path to use for the next spawn (cycles 0..NumPaths-1). */
	int32 NextPathIndex = 0;

	FTimerHandle SpawnTimerHandle;

	/** Count currently-alive enemies (used only when MaxEnemiesAlive > 0). */
	int32 CountAliveEnemies() const;
};
