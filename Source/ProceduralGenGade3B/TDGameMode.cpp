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
#include "TDMatchRewards.h"
#include "TDMetaProgressionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Layout/Anchors.h"
#include "EngineUtils.h"

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

	// Widget Blueprint classes are loaded in BeginPlay via LoadClass (not ConstructorHelpers)
	// so missing/moved packages do not spam CDO Constructor errors on editor startup.
	EndScreenWidgetClass = UTDEndScreenWidget::StaticClass();

	StartingMetaWallet.ForestEssence = 40;
	StartingMetaWallet.WoodenMight = 30;
	StartingMetaWallet.GemStones = 12;
	StartingMetaWallet.LightLanterns = 20;
}

void ATDGameMode::BeginPlay()
{
	Super::BeginPlay();

	EnsureDefaultWidgetClasses();

	EnsureStartingMetaWallet();

	MatchDefendersPlaced = 0;
	MatchHitsLanded = 0;
	MatchEnemiesKilled = 0;

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

		// Defeat screen
		const TSubclassOf<UTDEndScreenWidget> DefeatClass = EndScreenWidgetClass
			? EndScreenWidgetClass
			: TSubclassOf<UTDEndScreenWidget>(UTDEndScreenWidget::StaticClass());
		if (UTDEndScreenWidget* Screen = CreateWidget<UTDEndScreenWidget>(PC, DefeatClass))
		{
			EndScreenWidget = Screen;
			EndScreenWidget->AddToViewport(100);
			EndScreenWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			EndScreenWidget->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
		}

		// Victory screen — use dedicated class if set, otherwise reuse the defeat widget
		if (VictoryScreenWidgetClass)
		{
			if (UTDEndScreenWidget* VScreen = CreateWidget<UTDEndScreenWidget>(PC, VictoryScreenWidgetClass))
			{
				VictoryScreenWidget = VScreen;
				VictoryScreenWidget->AddToViewport(100);
				VictoryScreenWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				VictoryScreenWidget->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
			}
		}
		else
		{
			VictoryScreenWidget = EndScreenWidget; // same widget handles both
		}

		if (WaveManager && (EndScreenWidget || VictoryScreenWidget))
		{
			WaveManager->OnVictory.AddDynamic(this, &ATDGameMode::HandleMatchVictory);
		}

		if (UTDWarningBannerWidget* Warning = CreateWidget<UTDWarningBannerWidget>(PC, UTDWarningBannerWidget::StaticClass()))
		{
			WarningBannerWidget = Warning;
			WarningBannerWidget->AddToViewport(50);
		}

		RefreshMetaHUD();
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
	bGameOver = false;
	BeamUpgradeLevel = 0;
	MatchDefendersPlaced = 0;
	MatchHitsLanded = 0;
	MatchEnemiesKilled = 0;
	UGameplayStatics::SetGamePaused(this, false);

	if (EndScreenWidget)
	{
		EndScreenWidget->HideScreen();
	}
	if (VictoryScreenWidget && VictoryScreenWidget != EndScreenWidget)
	{
		VictoryScreenWidget->HideScreen();
	}

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

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
			BaseTowerAttackDamage = Tower->AttackDamage;
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
	++MatchEnemiesKilled;
	if (DeadEnemy)
	{
		AddResources(DeadEnemy->ResourceReward);
	}
}

void ATDGameMode::NotifyEnemyHit()
{
	++MatchHitsLanded;
}

void ATDGameMode::NotifyDefenderPlaced()
{
	++MatchDefendersPlaced;
}

void ATDGameMode::EnsureDefaultWidgetClasses()
{
	// Prefer V2 widgets; fall back silently if a package was moved or failed to compile.
	if (!HUDWidgetClass)
	{
		if (UClass* FoundHUD = LoadClass<UTDHUDWidget>(
			nullptr, TEXT("/Game/UI/WBP_MatchHUD_V2.WBP_MatchHUD_V2_C")))
		{
			HUDWidgetClass = FoundHUD;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("TDGameMode: WBP_MatchHUD_V2 not found — match HUD will be skipped."));
		}
	}

	if (!EndScreenWidgetClass || EndScreenWidgetClass == UTDEndScreenWidget::StaticClass())
	{
		// Try your custom Gameoverscreen Blueprint first, fall back to the V2 generic, then pure C++
		UClass* FoundDefeat = LoadClass<UTDEndScreenWidget>(nullptr, TEXT("/Game/UI/ScreenWidgets/Gameoverscreen.Gameoverscreen_C"));
		if (!FoundDefeat)
		{
			FoundDefeat = LoadClass<UTDEndScreenWidget>(nullptr, TEXT("/Game/UI/WBP_EndScreen_V2.WBP_EndScreen_V2_C"));
		}
		if (FoundDefeat)
		{
			EndScreenWidgetClass = FoundDefeat;
		}
		else
		{
			EndScreenWidgetClass = UTDEndScreenWidget::StaticClass();
			UE_LOG(LogTemp, Warning, TEXT("TDGameMode: No defeat screen Blueprint found — using C++ fallback."));
		}
	}

	if (!VictoryScreenWidgetClass)
	{
		// Try your custom VictoryScreen Blueprint first, fall back to the V2 generic
		UClass* FoundVictory = LoadClass<UTDEndScreenWidget>(nullptr, TEXT("/Game/UI/ScreenWidgets/VictoryScreen.VictoryScreen_C"));
		if (!FoundVictory)
		{
			FoundVictory = LoadClass<UTDEndScreenWidget>(nullptr, TEXT("/Game/UI/WBP_EndScreen_V2.WBP_EndScreen_V2_C"));
			if (FoundVictory)
			{
				UE_LOG(LogTemp, Warning, TEXT("TDGameMode: VictoryScreen Blueprint not found — using WBP_EndScreen_V2."));
			}
		}
		if (FoundVictory)
		{
			VictoryScreenWidgetClass = FoundVictory;
		}
	}
}

void ATDGameMode::EnsureStartingMetaWallet()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			const FMetaCurrencyRewards Wallet = Meta->GetWallet();
			const bool bWalletEmpty = Wallet.ForestEssence == 0
				&& Wallet.WoodenMight == 0
				&& Wallet.GemStones == 0
				&& Wallet.LightLanterns == 0;
			if (bWalletEmpty)
			{
				Meta->AddRewards(StartingMetaWallet);
			}
		}
	}
}

FMetaCurrencyRewards ATDGameMode::GetMetaWallet() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			return Meta->GetWallet();
		}
	}
	return FMetaCurrencyRewards();
}

bool ATDGameMode::TrySpendMeta(const FMetaCurrencyRewards& Cost)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			if (Meta->TrySpend(Cost))
			{
				RefreshMetaHUD();
				return true;
			}
		}
	}
	return false;
}

bool ATDGameMode::CanAffordMeta(const FMetaCurrencyRewards& Cost) const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			return Meta->CanAfford(Cost);
		}
	}
	return false;
}

bool ATDGameMode::IsInteractionBlocked() const
{
	return bPaused || bGameOver || IsVictory();
}

bool ATDGameMode::TryUpgradeTowerBeam()
{
	if (!Tower || bGameOver || IsVictory())
	{
		return false;
	}

	if (BeamUpgradeLevel >= MaxBeamUpgradeLevel)
	{
		ShowInsufficientFundsWarning();
		return false;
	}

	FMetaCurrencyRewards Cost;
	Cost.LightLanterns = BeamUpgradeLanternCost;
	if (!TrySpendMeta(Cost))
	{
		if (WarningBannerWidget)
		{
			WarningBannerWidget->ShowWarning(TEXT("Not enough Light Lanterns for beam upgrade"));
		}
		return false;
	}

	if (BaseTowerAttackDamage <= 0.0f)
	{
		BaseTowerAttackDamage = Tower->AttackDamage;
	}

	++BeamUpgradeLevel;
	Tower->AttackDamage = BaseTowerAttackDamage + BeamUpgradeLevel * BeamUpgradeDamageBonus;
	RefreshMetaHUD();
	return true;
}

void ATDGameMode::RefreshMetaHUD() const
{
	if (!MatchHUDWidget)
	{
		return;
	}

	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		MatchHUDWidget->RefreshMetaCurrency(GetMetaWallet(), BeamUpgradeLevel, PC->IsPlacingStrongDefender());
	}
	else
	{
		MatchHUDWidget->RefreshMetaCurrency(GetMetaWallet(), BeamUpgradeLevel, false);
	}
}

void ATDGameMode::FinalizeMatchResult(bool bVictory)
{
	int32 WavesCleared = 0;
	const int32 TotalWaves = WaveManager ? WaveManager->GetTotalWaves() : 0;
	if (WaveManager)
	{
		if (bVictory)
		{
			WavesCleared = TotalWaves;
		}
		else
		{
			const EWaveState WaveState = WaveManager->GetWaveState();
			const int32 CurrentWave = WaveManager->GetCurrentWave();
			if (WaveState == EWaveState::Complete)
			{
				WavesCleared = CurrentWave;
			}
			else
			{
				WavesCleared = FMath::Max(0, CurrentWave - 1);
			}
		}
	}

	float TowerHealth = 0.0f;
	float TowerMaxHealth = 1.0f;
	if (Tower && Tower->HealthComponent)
	{
		TowerHealth = Tower->HealthComponent->GetCurrentHealth();
		TowerMaxHealth = Tower->HealthComponent->MaxHealth;
	}

	int32 SurvivingDefenders = 0;
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		const ADefender* Defender = *It;
		if (Defender && Defender->HealthComponent && !Defender->HealthComponent->IsDead())
		{
			++SurvivingDefenders;
		}
	}

	const FMetaCurrencyRewards WalletBeforePayout = GetMetaWallet();

	LastMatchResult = UTDMatchRewards::BuildMatchResult(
		bVictory,
		TowerHealth,
		TowerMaxHealth,
		WavesCleared,
		TotalWaves,
		MatchDefendersPlaced,
		MatchHitsLanded,
		MatchEnemiesKilled,
		SurvivingDefenders);
	LastMatchResult.Wallet = WalletBeforePayout;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			Meta->AddRewards(LastMatchResult.Rewards);
		}
	}
}

void ATDGameMode::ShowEndScreen(bool bVictory)
{
	UTDEndScreenWidget* TargetWidget = bVictory ? VictoryScreenWidget : EndScreenWidget;
	if (!TargetWidget)
	{
		return;
	}

	// Do NOT call SetGamePaused here — that triggers the engine's built-in pause menu
	// widget instead of our end screen. Gameplay is already frozen because the wave
	// manager and spawner were stopped before ShowEndScreen was called.
	bPaused = true;

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	if (bVictory)
	{
		TargetWidget->PresentMatchResult(true, LastMatchResult);
	}
	else
	{
		TargetWidget->PresentMatchResult(false, LastMatchResult);
	}
}

void ATDGameMode::HandleMatchVictory()
{
	if (bGameOver)
	{
		return;
	}

	FinalizeMatchResult(true);
	ShowEndScreen(true);
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
	FinalizeMatchResult(false);
	ShowEndScreen(false);
	UE_LOG(LogTemp, Display, TEXT("TDGameMode: GAME OVER - the tower was destroyed."));
}
