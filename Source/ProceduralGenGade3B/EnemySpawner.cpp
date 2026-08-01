// EnemySpawner.cpp — see EnemySpawner.h for the overview.

#include "EnemySpawner.h"
#include "Enemy.h"
#include "ProceduralTerrain.h"
#include "EngineUtils.h"

AEnemySpawner::AEnemySpawner()
{
	// Spawning is timer-driven, so no per-frame tick.
	PrimaryActorTick.bCanEverTick = false;

	// Default to the plain C++ enemy unless a designer overrides it.
	EnemyClass = AEnemy::StaticClass();
}

void AEnemySpawner::Initialize(AProceduralTerrain* InTerrain, AActor* InTower)
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
	// Fire SpawnEnemy every SpawnInterval seconds (with an initial delay of one interval).
	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AEnemySpawner::SpawnEnemy, SpawnInterval, /*bLoop=*/true, /*FirstDelay=*/SpawnInterval);
}

void AEnemySpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void AEnemySpawner::SpawnEnemy()
{
	// Need a terrain with at least one path to spawn anything.
	if (!Terrain)
	{
		return;
	}
	const TArray<FEnemyPath>& Paths = Terrain->GetEnemyPaths();
	if (Paths.Num() == 0 || !EnemyClass)
	{
		return;
	}

	// Respect the optional live-enemy cap.
	if (MaxEnemiesAlive > 0 && CountAliveEnemies() >= MaxEnemiesAlive)
	{
		return;
	}

	// Choose the next path in rotation so all routes stay active.
	const int32 PathIndex = NextPathIndex % Paths.Num();
	NextPathIndex = (NextPathIndex + 1) % Paths.Num();
	const FEnemyPath& Path = Paths[PathIndex];

	// Spawn slightly above the path plane; the enemy re-snaps itself in SetPath().
	const FVector SpawnLocation = Path.SpawnPoint + FVector(0.0f, 0.0f, 50.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnemy* Enemy = GetWorld()->SpawnActor<AEnemy>(EnemyClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (Enemy)
	{
		Enemy->SetPath(Path.Waypoints);
		Enemy->SetTargetTower(Tower);
	}
}

int32 AEnemySpawner::CountAliveEnemies() const
{
	int32 Count = 0;
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		++Count;
	}
	return Count;
}
