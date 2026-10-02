// EnemySpawner.cpp
// Handles automatic spawning of Basic, Bear and Wolf enemies.

#include "EnemySpawner.h"

#include "Enemy.h"
#include "ProceduralTerrain.h"
#include "EngineUtils.h"

AEnemySpawner::AEnemySpawner()
{
    PrimaryActorTick.bCanEverTick = false;

    // Use the C++ enemy by default.
    EnemyClass = AEnemy::StaticClass();

    // Start the enemy sequence from Basic.
    SpawnedEnemyTypeIndex = 0;
}

void AEnemySpawner::Initialize(
    AProceduralTerrain* InTerrain,
    AActor* InTower)
{
    Terrain = InTerrain;
    Tower = InTower;

    if (bAutoStart)
    {
        StartSpawning();
    }
}

void AEnemySpawner::StartSpawning()
{
    GetWorldTimerManager().SetTimer(
        SpawnTimerHandle,
        this,
        &AEnemySpawner::SpawnEnemy,
        SpawnInterval,
        true,
        SpawnInterval);
}

void AEnemySpawner::StopSpawning()
{
    GetWorldTimerManager().ClearTimer(
        SpawnTimerHandle);
}

void AEnemySpawner::SpawnEnemy()
{
    SpawnSingleEnemy();
}

AEnemy* AEnemySpawner::SpawnSingleEnemy()
{
    // We need terrain.
    if (!Terrain)
    {
        return nullptr;
    }

    const TArray<FEnemyPath>& Paths =
        Terrain->GetEnemyPaths();

    if (Paths.Num() == 0 || !EnemyClass)
    {
        return nullptr;
    }

    // Respect enemy limit.
    if (MaxEnemiesAlive > 0 &&
        CountAliveEnemies() >= MaxEnemiesAlive)
    {
        return nullptr;
    }

    // Choose the next path.
    const int32 PathIndex =
        NextPathIndex % Paths.Num();

    NextPathIndex =
        (NextPathIndex + 1) % Paths.Num();

    const FEnemyPath& Path =
        Paths[PathIndex];

    // Choose Basic, Bear or Wolf.
    EEnemyType EnemyType =
        static_cast<EEnemyType>(
            SpawnedEnemyTypeIndex);

    // Move to the next enemy type.
    SpawnedEnemyTypeIndex =
        (SpawnedEnemyTypeIndex + 1) % 3;

    // Create the spawn location.
    const FVector SpawnLocation =
        Path.SpawnPoint +
        FVector(
            0.0f,
            0.0f,
            EnemyClass.GetDefaultObject()->GroundClearance);

    const FTransform SpawnTransform(
        FRotator::ZeroRotator,
        SpawnLocation);

    // Create the enemy without starting BeginPlay yet.
    AEnemy* Enemy =
        GetWorld()->SpawnActorDeferred<AEnemy>(
            EnemyClass,
            SpawnTransform,
            this,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!Enemy)
    {
        return nullptr;
    }

    // Set the enemy type before BeginPlay.
    Enemy->SetEnemyType(EnemyType);

    // Give the enemy its path.
    Enemy->SetPath(Path.Waypoints);

    // Give the enemy its tower target.
    Enemy->SetTargetTower(Tower);

    // Finish spawning and start BeginPlay.
    Enemy->FinishSpawning(SpawnTransform);

    return Enemy;
}

int32 AEnemySpawner::CountAliveEnemies() const
{
    int32 Count = 0;

    for (TActorIterator<AEnemy> It(GetWorld());
         It;
         ++It)
    {
        ++Count;
    }

    return Count;
}