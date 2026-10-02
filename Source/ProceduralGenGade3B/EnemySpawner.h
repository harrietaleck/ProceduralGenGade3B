// EnemySpawner.h
// Spawns Basic, Bear and Wolf enemies in turn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy.h"
#include "EnemySpawner.generated.h"

class AProceduralTerrain;
class AEnemy;

UCLASS()
class PROCEDURALGENGADE3B_API AEnemySpawner : public AActor
{
    GENERATED_BODY()

public:
    AEnemySpawner();

    // Enemy class to spawn.
    // Normally this is AEnemy or a Blueprint child.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
    TSubclassOf<AEnemy> EnemyClass;

    // Time between enemies.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
    float SpawnInterval = 2.0f;

    // Maximum number of enemies alive.
    // 0 means unlimited.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
    int32 MaxEnemiesAlive = 0;

    // Automatically start spawning.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
    bool bAutoStart = true;

    // Give the spawner its terrain and tower.
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    void Initialize(
        AProceduralTerrain* InTerrain,
        AActor* InTower);

    // Start spawning.
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    void StartSpawning();

    // Stop spawning.
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    void StopSpawning();

    // Spawns one enemy. It takes turns between Basic, Bear and Wolf, and between the lanes.
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    AEnemy* SpawnSingleEnemy();

    // Spawns a chosen enemy type on a chosen lane. The wave director uses this.
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    AEnemy* SpawnEnemyOfType(EEnemyType Type, int32 PathIndex);

    // How far an enemy has to walk on a lane from its spawn point to the tower.
    UFUNCTION(BlueprintPure, Category = "Spawner")
    float GetPathLength(int32 PathIndex) const;

protected:

    void SpawnEnemy();

private:

    // Procedural terrain.
    UPROPERTY()
    TObjectPtr<AProceduralTerrain> Terrain;

    // Tower target.
    UPROPERTY()
    TObjectPtr<AActor> Tower;

    // Which path to use next.
    int32 NextPathIndex = 0;

    // Which enemy type to spawn next.
    int32 SpawnedEnemyTypeIndex = 0;

    // Spawn timer.
    FTimerHandle SpawnTimerHandle;

    // Count living enemies.
    int32 CountAliveEnemies() const;
};