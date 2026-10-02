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
    if (!Terrain || Terrain->GetEnemyPaths().Num() == 0)
    {
        return nullptr;
    }

    // Choose the next path.
    const int32 PathIndex = NextPathIndex % Terrain->GetEnemyPaths().Num();
    NextPathIndex = PathIndex + 1;

    // Choose Basic, Bear or Wolf in turn.
    const EEnemyType EnemyType = static_cast<EEnemyType>(SpawnedEnemyTypeIndex);
    SpawnedEnemyTypeIndex = (SpawnedEnemyTypeIndex + 1) % 3;

    return SpawnEnemyOfType(EnemyType, PathIndex);
}

float AEnemySpawner::GetPathLength(int32 PathIndex) const
{
    if (!Terrain || !Terrain->GetEnemyPaths().IsValidIndex(PathIndex))
    {
        return 0.0f;
    }

    const FEnemyPath& Path = Terrain->GetEnemyPaths()[PathIndex];
    float Length = 0.0f;
    FVector Previous = Path.SpawnPoint;
    for (const FVector& Point : Path.Waypoints)
    {
        Length += FVector::Dist2D(Previous, Point);
        Previous = Point;
    }
    return Length;
}

AEnemy* AEnemySpawner::SpawnEnemyOfType(EEnemyType EnemyType, int32 PathIndex)
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

    const FEnemyPath& Path =
        Paths[FMath::Clamp(PathIndex, 0, Paths.Num() - 1)];

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