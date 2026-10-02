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

    if (Paths.Num() == 0 ||
        !EnemyClass)
    {
        return nullptr;
    }

    // Respect enemy limit.
    if (MaxEnemiesAlive > 0 &&
        CountAliveEnemies() >= MaxEnemiesAlive)
    {
        return nullptr;
    }

    // Choose path.
    const int32 PathIndex =
        NextPathIndex % Paths.Num();

    NextPathIndex =
        (NextPathIndex + 1) % Paths.Num();

    const FEnemyPath& Path =
        Paths[PathIndex];

    // Decide which enemy type to create.
    EEnemyType EnemyType;

    switch (SpawnedEnemyTypeIndex % 3)
    {
    case 0:
        EnemyType = EEnemyType::Basic;
        break;

    case 1:
        EnemyType = EEnemyType::Bear;
        break;

    default:
        EnemyType = EEnemyType::Wolf;
        break;
    }

    // Move to the next enemy type for the next spawn.
    SpawnedEnemyTypeIndex =
        (SpawnedEnemyTypeIndex + 1) % 3;

    // Use the default enemy's clearance for initial placement.
    const FVector SpawnLocation =
        Path.SpawnPoint +
        FVector(
            0.0f,
            0.0f,
            EnemyClass.GetDefaultObject()->GroundClearance);

    FActorSpawnParameters SpawnParams;

    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // IMPORTANT:
    // Deferred spawning lets us select Basic/Bear/Wolf
    // BEFORE BeginPlay executes.
    AEnemy* Enemy =
        GetWorld()->SpawnActorDeferred<AEnemy>(
            EnemyClass,
            FTransform(
                FRotator::ZeroRotator,
                SpawnLocation));

    if (!Enemy)
    {
        return nullptr;
    }

    // Set the enemy type before BeginPlay.
    Enemy->SetEnemyType(EnemyType);

    // Finish the spawn.
    Enemy->FinishSpawning(
        FTransform(
            FRotator::ZeroRotator,
            SpawnLocation));

    // Give the enemy its path.
    Enemy->SetPath(Path.Waypoints);

    // Give the enemy its tower target.
    Enemy->SetTargetTower(Tower);

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