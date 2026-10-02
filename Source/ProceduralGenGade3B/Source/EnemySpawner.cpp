// EnemySpawner.cpp — see EnemySpawner.h for the overview.

#include "EnemySpawner.h"
#include "Enemy.h"
#include "ProceduralTerrain.h"
#include "EngineUtils.h"

AEnemySpawner::AEnemySpawner()
{
   	//Create the spawner setup to which runs when the enemy timer begins
    PrimaryActorTick.bCanEverTick = false;

    //Create a default enemy class when an enemy isnt assigned 
    EnemyClass = AEnemy::StaticClass();

    //Create default enemy types
    EnemyTypes.Add(EEnemyType::Basic);
    EnemyTypes.Add(EEnemyType::Bear);
    EnemyTypes.Add(EEnemyType::Wolf);
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
	// The timer path simply requests one enemy per tick.
	SpawnSingleEnemy();
}

AEnemy* AEnemySpawner::SpawnSingleEnemy()
{
	// Need a terrain with at least one path to spawn anything.
	if (!Terrain)
	{
		return nullptr;
	}
	const TArray<FEnemyPath>& Paths = Terrain->GetEnemyPaths();
	if (Paths.Num() == 0 || !EnemyClass)
	{
		return nullptr;
	}

	// Respect the optional live-enemy cap.
	if (MaxEnemiesAlive > 0 && CountAliveEnemies() >= MaxEnemiesAlive)
	{
		return nullptr;
	}

	// Choose the next path in rotation so all routes stay active.
	const int32 PathIndex = NextPathIndex % Paths.Num();
	NextPathIndex = (NextPathIndex + 1) % Paths.Num();
	const FEnemyPath& Path = Paths[PathIndex];

	// Spawn at the path start with the same height offset the movement code uses.
	const FVector SpawnLocation = Path.SpawnPoint + FVector(0.0f, 0.0f, EnemyClass.GetDefaultObject()->GroundClearance);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnemy* Enemy = GetWorld()->SpawnActor<AEnemy>(EnemyClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (Enemy)
	{
		//Create an ability to choose an enemy type in the array 
		if (bUseEnemyTypes && EnemyTypes.Num() > 0)
		{
			//Use the index that is current to select the enemy type
			const int32 TypeIndex =	SpawnedEnemyTypeIndex % EnemyTypes.Num();

			//Illustrate what enemy type the spawn enemy is
			Enemy->EnemyType = EnemyTypes[TypeIndex];

			//Increment onto the next spawn
			SpawnedEnemyTypeIndex =	(SpawnedEnemyTypeIndex + 1) % EnemyTypes.Num();
		}
		//Give the enemy its path to tread on
		Enemy->SetPath(Path.Waypoints);
		//Designate the enemy the tower that needs to be attacked
		Enemy->SetTargetTower(Tower);
	}
	return Enemy;
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
