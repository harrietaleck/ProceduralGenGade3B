// TDGameMode.cpp — see TDGameMode.h for the overview.

#include "TDGameMode.h"
#include "ProceduralTerrain.h"
#include "Tower.h"
#include "Enemy.h"
#include "EnemySpawner.h"
#include "WaveManager.h"
#include "BuildPadMarker.h"
#include "TDPlayerController.h"
#include "TDHUD.h"
#include "TDHUDWidget.h"
#include "TDCameraPawn.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ATDGameMode::ATDGameMode()
{
	// Use our own player controller (mouse-driven defender placement) and a third-person
	// hero the player walks around the battlefield (Dungeon Defenders / Orcs Must Die style).
	PlayerControllerClass = ATDPlayerController::StaticClass();
	DefaultPawnClass = AHeroCharacter::StaticClass();
	HUDClass = ATDHUD::StaticClass();

	// Default to the plain C++ classes; a designer can override these with Blueprint children.
	TowerClass = ATower::StaticClass();
	SpawnerClass = AEnemySpawner::StaticClass();
	WaveManagerClass = AWaveManager::StaticClass();
	BuildPadMarkerClass = ABuildPadMarker::StaticClass();

	// Default to the UMG match HUD asset if one exists at this path (created via the UMG
	// editor tools as a child of UTDHUDWidget). Missing gracefully means no HUD is shown
	// rather than a hard error — matches how meshes are defaulted elsewhere in this project.
	static ConstructorHelpers::FClassFinder<UTDHUDWidget> HUDWidgetFinder(TEXT("/Game/UI/WBP_MatchHUD"));
	if (HUDWidgetFinder.Succeeded())
	{
		HUDWidgetClass = HUDWidgetFinder.Class;
	}
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

	// Mark every generated build pad with a visual platform, so valid placement locations
	// are always obvious (brief: "is it clear to the player how/where they can build?").
	if (BuildPadMarkerClass)
	{
		for (const FVector& Slot : Terrain->GetDefenderSlots())
		{
			GetWorld()->SpawnActor<ABuildPadMarker>(BuildPadMarkerClass, Slot + FVector(0.0f, 0.0f, 4.0f), FRotator::ZeroRotator, SpawnParams);
		}
	}

	// Spawn the enemy spawner and hand it the terrain (for spawn points/paths) and the tower (target).
	// We disable its self-driven timer: the WaveManager decides when each enemy spawns.
	Spawner = GetWorld()->SpawnActor<AEnemySpawner>(SpawnerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (Spawner)
	{
		Spawner->bAutoStart = false;
		Spawner->Initialize(Terrain, Tower);
	}

	// Spawn the wave manager and let it drive the spawner in escalating waves.
	WaveManager = GetWorld()->SpawnActor<AWaveManager>(WaveManagerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (WaveManager && Spawner)
	{
		WaveManager->Initialize(Spawner, /*bStartImmediately=*/true);
	}

	// Create the UMG match HUD last, now that Tower and WaveManager both exist for it to
	// bind to. A missing HUDWidgetClass (no WBP_TDHUD asset yet) is a silent no-op.
	if (HUDWidgetClass)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (UTDHUDWidget* Widget = CreateWidget<UTDHUDWidget>(PC, HUDWidgetClass))
			{
				Widget->AddToViewport();
				Widget->InitializeHUD(this);
			}
		}
	}
}

int32 ATDGameMode::GetCurrentWave() const
{
	return WaveManager ? WaveManager->GetCurrentWave() : 0;
}

bool ATDGameMode::IsVictory() const
{
	return WaveManager && WaveManager->IsVictory();
}

void ATDGameMode::RestartGame()
{
	// Reopen the current level. Because the terrain randomises its seed on BeginPlay, this
	// produces a fresh map, fresh economy and fresh waves — a brand-new game.
	const FName CurrentLevel(*UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true));
	UGameplayStatics::OpenLevel(this, CurrentLevel);
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

	// Stop spawning more enemies (halt both the wave pacing and any spawner timer).
	if (WaveManager)
	{
		WaveManager->StopWaves();
	}
	if (Spawner)
	{
		Spawner->StopSpawning();
	}

	OnGameOver.Broadcast();
	UE_LOG(LogTemp, Display, TEXT("TDGameMode: GAME OVER - the tower was destroyed."));
}
