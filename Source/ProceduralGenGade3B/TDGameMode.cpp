// TDGameMode.cpp — see TDGameMode.h for the overview.

#include "TDGameMode.h"
#include "ProceduralTerrain.h"
#include "Tower.h"
#include "Enemy.h"
#include "EnemySpawner.h"
#include "TDPlayerController.h"
#include "GameFramework/DefaultPawn.h"
#include "EngineUtils.h"

ATDGameMode::ATDGameMode()
{
	// Use our own player controller (mouse-driven defender placement) and a free-flying
	// default pawn so the player can look around the battlefield.
	PlayerControllerClass = ATDPlayerController::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();

	// Default to the plain C++ classes; a designer can override these with Blueprint children.
	TowerClass = ATower::StaticClass();
	SpawnerClass = AEnemySpawner::StaticClass();
}

void ATDGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Seed the economy.
	Resources = StartingResources;
	OnResourcesChanged.Broadcast(Resources);

	// Find the procedural terrain that was placed in the level.
	Terrain = FindTerrain();
	if (!Terrain)
	{
		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: no AProceduralTerrain found in the level."));
		return;
	}

	// Spawn the tower on the terrain's central tower cell, raised so its base sits on the ground.
	const FVector TowerLocation = Terrain->GetTowerLocation() + FVector(0.0f, 0.0f, 150.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Tower = GetWorld()->SpawnActor<ATower>(TowerClass, TowerLocation, FRotator::ZeroRotator, SpawnParams);

	// Spawn the enemy spawner and hand it the terrain (for spawn points/paths) and the tower (target).
	Spawner = GetWorld()->SpawnActor<AEnemySpawner>(SpawnerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (Spawner)
	{
		Spawner->Initialize(Terrain, Tower);
	}
}

AProceduralTerrain* ATDGameMode::FindTerrain() const
{
	for (TActorIterator<AProceduralTerrain> It(GetWorld()); It; ++It)
	{
		return *It; // Use the first terrain found.
	}
	return nullptr;
}

void ATDGameMode::AddResources(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	Resources += Amount;
	OnResourcesChanged.Broadcast(Resources);
}

bool ATDGameMode::TrySpendResources(int32 Amount)
{
	if (Amount < 0 || Resources < Amount)
	{
		return false; // Can't afford it.
	}
	Resources -= Amount;
	OnResourcesChanged.Broadcast(Resources);
	return true;
}

void ATDGameMode::NotifyEnemyKilled(AEnemy* DeadEnemy)
{
	if (DeadEnemy)
	{
		AddResources(DeadEnemy->ResourceReward);
	}
}

void ATDGameMode::NotifyTowerDestroyed()
{
	if (bGameOver)
	{
		return; // Only end the game once.
	}
	bGameOver = true;

	// Stop spawning more enemies.
	if (Spawner)
	{
		Spawner->StopSpawning();
	}

	OnGameOver.Broadcast();
	UE_LOG(LogTemp, Display, TEXT("TDGameMode: GAME OVER - the tower was destroyed."));
}
