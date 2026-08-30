// TDGameMode.cpp — see TDGameMode.h for the overview.

#include "TDGameMode.h"
#include "ProceduralTerrain.h"
#include "Tower.h"
#include "Defender.h"
#include "Enemy.h"
#include "EnemySpawner.h"
#include "WaveManager.h"
#include "BuildPadMarker.h"
#include "HealthComponent.h"
#include "TDPlayerController.h"
#include "TDHUD.h"
#include "TDHUDWidget.h"
#include "TDEndScreenWidget.h"
#include "TDWarningBannerWidget.h"
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

	EndScreenWidgetClass = UTDEndScreenWidget::StaticClass();
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

	// Build the whole world (terrain, tower, build pads) and validate it as actually placed
	// before allowing gameplay to begin. Any failure tears down this attempt's actors and
	// regenerates the entire world from scratch — gameplay must never start on an invalid map.
	const int32 MaxWorldAttempts = 5;
	bool bWorldValid = false;
	for (int32 WorldAttempt = 1; WorldAttempt <= MaxWorldAttempts; ++WorldAttempt)
	{
		if (WorldAttempt > 1)
		{
			DestroySpawnedWorldActors();
		}

		// Explicitly trigger generation before reading any terrain data. AProceduralTerrain's own
		// BeginPlay deliberately does nothing, since actor BeginPlay order between this GameMode
		// and the terrain actor is not guaranteed by Unreal — calling this here removes that
		// ambiguity. A fresh seed each attempt (bRandomizeSeedOnBeginPlay permitting) so a failed
		// attempt doesn't just regenerate the exact same invalid layout.
		Terrain->PrepareForNewGame();

		if (!SpawnTowerWithRetry())
		{
			UE_LOG(LogTemp, Warning, TEXT("TDGameMode: world attempt %d/%d failed to place a tower — regenerating entire world."), WorldAttempt, MaxWorldAttempts);
			continue;
		}

		SpawnBuildPadMarkers();

		bWorldValid = ValidateWorldBeforeGameplay();
		if (bWorldValid)
		{
			break;
		}

		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: world validation failed on attempt %d/%d — regenerating entire world."), WorldAttempt, MaxWorldAttempts);
	}

	if (!bWorldValid)
	{
		UE_LOG(LogTemp, Error, TEXT("TDGameMode: failed to produce a valid world after %d attempts. Aborting match start — gameplay will not begin."), MaxWorldAttempts);
		DestroySpawnedWorldActors();
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

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
		WaveManager->OnWaveComplete.AddDynamic(this, &ATDGameMode::HandleWaveComplete);
	}

	// Create the UMG match HUD and end-screen overlay last, now that Tower and WaveManager
	// both exist for them to bind to.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (HUDWidgetClass)
		{
			if (UTDHUDWidget* Widget = CreateWidget<UTDHUDWidget>(PC, HUDWidgetClass))
			{
				MatchHUDWidget = Widget;
				MatchHUDWidget->AddToViewport(0);
				MatchHUDWidget->InitializeHUD(this);
			}
		}

		const TSubclassOf<UTDEndScreenWidget> ScreenClass = EndScreenWidgetClass
			? EndScreenWidgetClass
			: TSubclassOf<UTDEndScreenWidget>(UTDEndScreenWidget::StaticClass());
		if (UTDEndScreenWidget* Screen = CreateWidget<UTDEndScreenWidget>(PC, ScreenClass))
		{
			EndScreenWidget = Screen;
			EndScreenWidget->AddToViewport(100);
			OnGameOver.AddDynamic(EndScreenWidget, &UTDEndScreenWidget::ShowGameOver);
			if (WaveManager)
			{
				WaveManager->OnVictory.AddDynamic(EndScreenWidget, &UTDEndScreenWidget::ShowVictory);
			}
		}

		if (UTDWarningBannerWidget* Warning = CreateWidget<UTDWarningBannerWidget>(PC, UTDWarningBannerWidget::StaticClass()))
		{
			WarningBannerWidget = Warning;
			WarningBannerWidget->AddToViewport(50);
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

void ATDGameMode::ShowInsufficientFundsWarning()
{
	if (WarningBannerWidget)
	{
		WarningBannerWidget->ShowWarning(TEXT("Not enough Loot for a defender"));
	}
	if (MatchHUDWidget)
	{
		MatchHUDWidget->FlashLootInsufficient();
	}
}

void ATDGameMode::RestartGame()
{
	bPaused = false;
	UGameplayStatics::SetGamePaused(this, false);

	// Reopen the current level. Because the terrain randomises its seed on BeginPlay, this
	// produces a fresh map, fresh economy and fresh waves — a brand-new game.
	const FName CurrentLevel(*UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true));
	UGameplayStatics::OpenLevel(this, CurrentLevel);
}

void ATDGameMode::TogglePause()
{
	if (bGameOver || IsVictory())
	{
		return;
	}

	bPaused = !bPaused;
	UGameplayStatics::SetGamePaused(this, bPaused);
}

AProceduralTerrain* ATDGameMode::FindTerrain() const
{
	for (TActorIterator<AProceduralTerrain> It(GetWorld()); It; ++It)
	{
		return *It; // Use the first terrain found.
	}
	return nullptr;
}

bool ATDGameMode::SpawnTowerWithRetry()
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Bounded retry: if a spawn is ever rejected (nullptr) or lands away from the terrain's
	// published tower location (e.g. blocked by another actor at that transform), regenerate the
	// terrain and try again rather than starting a match with a missing/misplaced tower.
	const int32 MaxTowerSpawnAttempts = 5;
	const float TowerPlacementToleranceSq = FMath::Square(50.0f);
	for (int32 Attempt = 1; Attempt <= MaxTowerSpawnAttempts; ++Attempt)
	{
		if (Tower)
		{
			Tower->Destroy();
			Tower = nullptr;
		}

		const FVector TowerSpawnLocation = Terrain->GetTowerLocation() + FVector(0.0f, 0.0f, 150.0f);
		Tower = GetWorld()->SpawnActor<ATower>(TowerClass, TowerSpawnLocation, FRotator::ZeroRotator, SpawnParams);

		const bool bLocationMatches = Tower && FVector::DistSquared(Tower->GetActorLocation(), TowerSpawnLocation) <= TowerPlacementToleranceSq;
		if (Tower && bLocationMatches)
		{
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: tower spawn attempt %d/%d failed validation, regenerating terrain."), Attempt, MaxTowerSpawnAttempts);
		Terrain->RandomizeAndRegenerate();
	}

	return false;
}

void ATDGameMode::SpawnBuildPadMarkers()
{
	SpawnBuildPadMarkersFromIndex(0);
}

void ATDGameMode::SpawnBuildPadMarkersFromIndex(int32 StartSlotIndex)
{
	if (!Terrain || !BuildPadMarkerClass || StartSlotIndex < 0)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const TArray<FDefenderSlot>& Slots = Terrain->GetDefenderSlots();
	for (int32 I = StartSlotIndex; I < Slots.Num(); ++I)
	{
		const FDefenderSlot& Slot = Slots[I];
		if (ABuildPadMarker* Marker = GetWorld()->SpawnActor<ABuildPadMarker>(BuildPadMarkerClass, Slot.Location + FVector(0.0f, 0.0f, 4.0f), FRotator::ZeroRotator, SpawnParams))
		{
			BuildPadMarkers.Add(Marker);
		}
	}
}

void ATDGameMode::ApplyDefenderUpkeep()
{
	int32 TotalUpkeep = 0;
	int32 DefenderCount = 0;
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		ADefender* Defender = *It;
		if (!Defender || !Defender->HealthComponent || Defender->HealthComponent->IsDead())
		{
			continue;
		}

		++DefenderCount;
		const int32 PerDefender = Defender->UpkeepPerWave > 0 ? Defender->UpkeepPerWave : DefenderUpkeepPerWave;
		TotalUpkeep += PerDefender;
	}

	if (TotalUpkeep <= 0)
	{
		return;
	}

	Resources = FMath::Max(0, Resources - TotalUpkeep);
	OnResourcesChanged.Broadcast(Resources);
	UE_LOG(LogTemp, Display, TEXT("TDGameMode: defender upkeep charged %d Loot (%d defenders)."), TotalUpkeep, DefenderCount);
}

void ATDGameMode::HandleWaveComplete(int32 WaveNumber)
{
	if (!Terrain || bGameOver || IsVictory())
	{
		return;
	}

	const int32 OldSlotCount = Terrain->GetDefenderSlots().Num();
	const int32 ExtendedLanes = Terrain->ExpandWorldAfterWave();
	if (ExtendedLanes > 0)
	{
		SpawnBuildPadMarkersFromIndex(OldSlotCount);
	}

	ApplyDefenderUpkeep();
}

void ATDGameMode::DestroySpawnedWorldActors()
{
	if (Tower)
	{
		Tower->Destroy();
		Tower = nullptr;
	}
	for (ABuildPadMarker* Marker : BuildPadMarkers)
	{
		if (Marker)
		{
			Marker->Destroy();
		}
	}
	BuildPadMarkers.Reset();
}

bool ATDGameMode::ValidateWorldBeforeGameplay() const
{
	// Terrain generated, with the minimum required systems.
	if (!Terrain || Terrain->GetEnemyPaths().Num() < 3 || Terrain->GetDefenderSlots().Num() == 0)
	{
		return false;
	}

	// Tower generated, and not floating: its spawned Z must closely match the terrain's
	// published tower ground height (SpawnTowerWithRetry already guarantees XY/overall placement
	// matches, this re-confirms it as actually placed in the world, not just requested).
	if (!Tower)
	{
		return false;
	}
	if (FMath::Abs(Tower->GetActorLocation().Z - (Terrain->GetTowerLocation().Z + 150.0f)) > 50.0f)
	{
		return false;
	}

	// Build slots valid: exactly one marker per published slot, none floating.
	if (BuildPadMarkers.Num() != Terrain->GetDefenderSlots().Num())
	{
		return false;
	}
	for (const ABuildPadMarker* Marker : BuildPadMarkers)
	{
		if (!Marker)
		{
			return false;
		}
	}

	// No overlapping actors: the tower's world bounds must not intersect any build-pad marker's.
	const FBox TowerBounds = Tower->GetComponentsBoundingBox(/*bNonColliding=*/true);
	for (const ABuildPadMarker* Marker : BuildPadMarkers)
	{
		if (TowerBounds.Intersect(Marker->GetComponentsBoundingBox(/*bNonColliding=*/true)))
		{
			return false;
		}
	}

	// Navigation coverage is logged by Terrain->ValidatePathfinding() as a diagnostic (see its
	// own comment) but doesn't gate gameplay here — enemies move via fixed waypoints, not the
	// NavMesh, and rebuilding navigation after the Tower actor exists would only make its own
	// exact ground point look artificially unreachable (Recast correctly carving a hole around
	// its BlockAll collision), not reveal anything about whether the map is actually playable.
	Terrain->ValidatePathfinding();

	return true;
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
