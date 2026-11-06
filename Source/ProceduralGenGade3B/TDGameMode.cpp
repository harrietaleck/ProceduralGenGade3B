// TDGameMode.cpp - match startup. The UI, world and match code live in the .inl files at the bottom.
// Splitting into .inl files keeps each file under 500 lines but still builds as one unit,
// which Live Coding needs after a file split.

#include "TDGameMode.h"
#include "DefenderUpgradeComponent.h"
#include "DangerScreenEffectComponent.h"
#include "TDSaveGame.h"
#include "TDSaveSubsystem.h"
#include "TDVFX.h" //  visual pass
#include "Misc/PackageName.h" //  checks the optional ambience asset exists
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
#include "HeroCharacter.h"
#include "TDMatchRewards.h"
#include "TDMetaProgressionSubsystem.h"
#include "TDMenuFunctionLibrary.h"
#include "TDMusicManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Widgets/Layout/Anchors.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "TimerManager.h"

ATDGameMode::ATDGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	PlayerControllerClass = ATDPlayerController::StaticClass();
	DefaultPawnClass = AHeroCharacter::StaticClass();
	HUDClass = ATDHUD::StaticClass();

	TowerClass = ATower::StaticClass();
	SpawnerClass = AEnemySpawner::StaticClass();
	WaveManagerClass = AWaveManager::StaticClass();
	BuildPadMarkerClass = ABuildPadMarker::StaticClass();
	EndScreenWidgetClass = UTDEndScreenWidget::StaticClass();
	DefenderUpgrades = CreateDefaultSubobject<UDefenderUpgradeComponent>(TEXT("DefenderUpgrades"));
	DangerScreenEffect = CreateDefaultSubobject<UDangerScreenEffectComponent>(TEXT("DangerScreenEffect"));

	//  Default visual-pass assets; all of them can be swapped in the editor.
	WaveTransitionVFX = TDVFX::Asset(TEXT("/Game/VFX/Environment/NS_WaveTransition"));
	VictoryVFX = TDVFX::Asset(TEXT("/Game/VFX/Gameplay/NS_Victory"));
	DefeatVFX = TDVFX::Asset(TEXT("/Game/VFX/Gameplay/NS_Defeat"));
	ForestAmbienceVFX = TDVFX::Asset(TEXT("/Game/VFX/Environment/NS_ForestAmbience"));

	StartingMetaWallet.ForestEssence = 40;
	StartingMetaWallet.WoodenMight = 30;
	StartingMetaWallet.GemStones = 12;
	StartingMetaWallet.LightLanterns = 10;
}

// Fantasy-horror mood: a dimmer, cool moonlit sun, a lower sky light and a low violet-teal fog.
// It is dark enough to feel eerie but the lanes and defenders stay easy to read.
static void ApplyMoodLighting(UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			Sun->SetLightColor(FLinearColor(0.62f, 0.7f, 1.0f));
			Sun->SetIntensity(Sun->Intensity * 0.55f);
		}
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (USkyLightComponent* Sky = It->GetLightComponent())
		{
			Sky->SetLightColor(FLinearColor(0.5f, 0.55f, 0.85f));
			Sky->SetIntensity(Sky->Intensity * 0.65f);
		}
	}

	AExponentialHeightFog* Fog = nullptr;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		Fog = *It;
		break;
	}
	if (!Fog)
	{
		Fog = World->SpawnActor<AExponentialHeightFog>(FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	}
	if (Fog && Fog->GetComponent())
	{
		UExponentialHeightFogComponent* FogComp = Fog->GetComponent();
		FogComp->SetFogDensity(0.018f);
		FogComp->SetFogHeightFalloff(0.25f);
		FogComp->SetFogMaxOpacity(0.75f);
		FogComp->SetStartDistance(1500.0f);
		FogComp->FogInscatteringLuminance = FLinearColor(0.16f, 0.12f, 0.3f);
		FogComp->MarkRenderStateDirty();
	}
}

// Level dressing: glowing crystal clusters and rock piles beside every lane, each crystal with a small
// coloured light, so the paths read clearly in the dim light. Positions come from the lane waypoints.
static void DecorateLanes(UWorld* World, AProceduralTerrain* Terrain)
{
	if (!World || !Terrain)
	{
		return;
	}
	UStaticMesh* Crystal = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ImportedAssets/Kenney/SM_detail_crystal_large"));
	UStaticMesh* Rocks = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ImportedAssets/Kenney/SM_detail_rocks_large"));
	if (!Crystal && !Rocks)
	{
		return;
	}

	FRandomStream Random(Terrain->Seed);
	const FLinearColor Glows[] = { FLinearColor(0.6f, 0.25f, 1.0f), FLinearColor(0.2f, 0.9f, 0.85f), FLinearColor(1.0f, 0.55f, 0.2f) };
	int32 Placed = 0;
	for (const FEnemyPath& Path : Terrain->GetEnemyPaths())
	{
		float SinceLast = 0.0f;
		for (int32 I = 1; I < Path.Waypoints.Num() && Placed < 60; ++I)
		{
			SinceLast += FVector::Dist2D(Path.Waypoints[I - 1], Path.Waypoints[I]);
			if (SinceLast < 650.0f)
			{
				continue;
			}
			SinceLast = 0.0f;

			const FVector Dir = (Path.Waypoints[I] - Path.Waypoints[I - 1]).GetSafeNormal2D();
			const FVector Side(-Dir.Y, Dir.X, 0.0f);
			const float Offset = (Random.FRand() > 0.5f ? 1.0f : -1.0f) * Random.FRandRange(420.0f, 640.0f);
			FVector Pos = Path.Waypoints[I] + Side * Offset;

			// Drop the prop onto the ground. Skip it if nothing is below.
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Pos + FVector(0, 0, 2000), Pos - FVector(0, 0, 2000), ECC_Visibility))
			{
				continue;
			}
			Pos = Hit.ImpactPoint;

			const bool bCrystal = Crystal && (!Rocks || Random.FRand() > 0.35f);
			UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(Terrain);
			Prop->SetStaticMesh(bCrystal ? Crystal : Rocks);
			Prop->SetMobility(EComponentMobility::Movable);
			Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Prop->SetWorldLocationAndRotation(Pos, FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f));
			Prop->SetWorldScale3D(FVector(Random.FRandRange(1.6f, 2.4f)));
			Prop->RegisterComponentWithWorld(World);
			++Placed;

			if (bCrystal)
			{
				UPointLightComponent* Glow = NewObject<UPointLightComponent>(Terrain);
				Glow->SetMobility(EComponentMobility::Movable);
				Glow->SetLightColor(Glows[Random.RandRange(0, 2)]);
				Glow->SetIntensity(2500.0f);
				Glow->SetAttenuationRadius(550.0f);
				Glow->SetCastShadows(false);
				Glow->SetWorldLocation(Pos + FVector(0, 0, 90));
				Glow->RegisterComponentWithWorld(World);
			}
		}
	}
}

void ATDGameMode::BeginPlay()
{
	Super::BeginPlay();

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	ATDMusicManager::GetOrCreate(this);
	if (!LevelName.Contains(TEXT("StartScreen")))
	{
		ApplyMoodLighting(GetWorld());
	}
	if (LevelName.Contains(TEXT("StartScreen")))
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->bShowMouseCursor = true;
			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
		}

		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<ATDGameMode> WeakThis(this);
			World->GetTimerManager().SetTimer(
				MenuStretchTimerHandle,
				FTimerDelegate::CreateLambda([WeakThis, World]()
				{
					if (!WeakThis.IsValid() || !IsValid(World))
					{
						return;
					}

					UTDMenuFunctionLibrary::StretchOpenMenuScreens(World);

					// Wire New Game / Resume / Exit on the Start Screen, and grey Resume out with no save.
					if (UTDSaveSubsystem* Saves = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr)
					{
						Saves->BindStartScreen(World);
					}

					TArray<UUserWidget*> MenuWidgets;
					UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
						World, MenuWidgets, UUserWidget::StaticClass(), true);
					bool bHasStart = false;
					for (UUserWidget* Widget : MenuWidgets)
					{
						if (Widget && Widget->GetClass()->GetName().Contains(TEXT("StartScreen")))
						{
							bHasStart = true;
							break;
						}
					}
					if (!bHasStart)
					{
						if (UClass* StartClass = LoadClass<UUserWidget>(
							nullptr, TEXT("/Game/UI/ScreenWidgets/StartScreen.StartScreen_C")))
						{
							if (APlayerController* PC = World->GetFirstPlayerController())
							{
								if (UUserWidget* StartWidget = CreateWidget<UUserWidget>(PC, StartClass))
								{
									StartWidget->AddToViewport(100);
									UTDMenuFunctionLibrary::StretchWidgetToFillScreen(StartWidget, false);
								}
							}
						}
					}
				}),
				0.15f,
				true);
		}
		return;
	}

	EnsureDefaultWidgetClasses();
	EnsureStartingMetaWallet();

	MatchDefendersPlaced = 0;
	MatchHitsLanded = 0;
	MatchEnemiesKilled = 0;
	PaidMatchRewards = FMetaCurrencyRewards();
	bWaveResultsVisible = false;

	Resources = StartingResources;
	OnResourcesChanged.Broadcast(Resources);

	Terrain = FindTerrain();
	if (!Terrain)
	{
		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: no AProceduralTerrain found in the level."));
		return;
	}

	// Resume: rebuild the saved map from its seed so saved defenders land on the same build pads.
	UTDSaveGame* ResumeSave = nullptr;
	if (UTDSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr)
	{
		ResumeSave = Saves->ConsumeResumeRequest();
	}
	const bool bRandomSeedSetting = Terrain->bRandomizeSeedOnBeginPlay;
	if (ResumeSave)
	{
		Terrain->bRandomizeSeedOnBeginPlay = false;
		Terrain->Seed = ResumeSave->TerrainSeed;
	}

	const int32 MaxWorldAttempts = 5;
	bool bWorldValid = false;
	for (int32 WorldAttempt = 1; WorldAttempt <= MaxWorldAttempts; ++WorldAttempt)
	{
		if (WorldAttempt > 1)
		{
			DestroySpawnedWorldActors();
		}

		Terrain->PrepareForNewGame();

		if (!SpawnTowerWithRetry())
		{
			UE_LOG(LogTemp, Warning, TEXT("TDGameMode: world attempt %d/%d failed to place a tower."), WorldAttempt, MaxWorldAttempts);
			continue;
		}

		SpawnBuildPadMarkers();
		bWorldValid = ValidateWorldBeforeGameplay();
		if (bWorldValid)
		{
			break;
		}

		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: world validation failed on attempt %d/%d."), WorldAttempt, MaxWorldAttempts);
	}

	Terrain->bRandomizeSeedOnBeginPlay = bRandomSeedSetting;
	if (!bWorldValid)
	{
		UE_LOG(LogTemp, Error, TEXT("TDGameMode: failed to produce a valid world after %d attempts."), MaxWorldAttempts);
		DestroySpawnedWorldActors();
		return;
	}

	// If generation had to pick another seed, the saved layout no longer matches: start fresh instead.
	if (ResumeSave && Terrain->Seed != ResumeSave->TerrainSeed)
	{
		UE_LOG(LogTemp, Warning, TEXT("TDSave: saved map could not be rebuilt; starting a new game instead."));
		ResumeSave = nullptr;
	}
	if (ResumeSave)
	{
		// Rebuild the world expansions that existed when this checkpoint was captured.
		const int32 ExpansionsToRestore = ResumeSave->bResumeAtCurrentWave
			? ResumeSave->WavesCleared
			: FMath::Max(0, ResumeSave->WavesCleared - 1);
		for (int32 Expansion = 0; Expansion < ExpansionsToRestore; ++Expansion)
		{
			const int32 OldSlotCount = Terrain->GetDefenderSlots().Num();
			if (Terrain->ExpandWorldAfterWave() > 0)
			{
				SpawnBuildPadMarkersFromIndex(OldSlotCount);
			}
		}
	}

	//  Visual pass: tower crystals + activation burst (after validation, so they never affect it),
	//  and looping forest ambience if that system has been created.
	Tower->ActivateTowerVisuals();
	DecorateLanes(GetWorld(), Terrain);
	if (!ResumeSave)
	{
		DefenderUpgrades->ActivatePendingUpgrades(); //  a Tower upgrade bought in the Start-menu shop starts now
	}
	if (FPackageName::DoesPackageExist(ForestAmbienceVFX.ToSoftObjectPath().GetLongPackageName()))
	{
		TDVFX::Play(this, ForestAmbienceVFX, Tower->GetActorLocation() + FVector(0.0f, 0.0f, 300.0f), TDVFX::Turquoise);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Spawner = GetWorld()->SpawnActor<AEnemySpawner>(SpawnerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (Spawner)
	{
		Spawner->bAutoStart = false;
		Spawner->Initialize(Terrain, Tower);
	}

	WaveManager = GetWorld()->SpawnActor<AWaveManager>(WaveManagerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (WaveManager && Spawner)
	{
		WaveManager->Initialize(Spawner, ResumeSave == nullptr);
		WaveManager->OnWaveComplete.AddDynamic(this, &ATDGameMode::HandleWaveComplete);
		if (DefenderUpgrades)
		{
			WaveManager->OnWaveStarted.AddDynamic(DefenderUpgrades, &UDefenderUpgradeComponent::HandleWaveStarted);
		}
		WaveManager->OnWaveStarted.AddDynamic(this, &ATDGameMode::HandleWaveStartedForCheckpoint);
	}
	if (!ResumeSave && WaveManager)
	{
		SaveCurrentWaveCheckpoint();
	}

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (HUDWidgetClass)
		{
			if (UTDHUDWidget* Widget = CreateWidget<UTDHUDWidget>(PC, HUDWidgetClass))
			{
				MatchHUDWidget = Widget;
				MatchHUDWidget->AddToViewport(0);
				MatchHUDWidget->InitializeHUD(this);
				if (DefenderUpgrades)
				{
					DefenderUpgrades->RefreshUpgradeUI();
				}
			}
		}

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

		if (VictoryScreenWidgetClass)
		{
			if (UUserWidget* VScreen = CreateWidget<UUserWidget>(PC, VictoryScreenWidgetClass))
			{
				VictoryScreenWidget = VScreen;
				VictoryScreenWidget->AddToViewport(100);
				UTDMenuFunctionLibrary::StretchWidgetToFillScreen(VictoryScreenWidget, false);
				VictoryScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
				BindVictoryScreenButtons();
			}
		}

		if (SettingsWidgetClass)
		{
			if (UUserWidget* Settings = CreateWidget<UUserWidget>(PC, SettingsWidgetClass))
			{
				SettingsWidget = Settings;
				SettingsWidget->AddToViewport(150);
				UTDMenuFunctionLibrary::StretchWidgetToFillScreen(SettingsWidget, false);
				SettingsWidget->SetVisibility(ESlateVisibility::Collapsed);
				BindSettingsButtons();
			}
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
		bPaused = false;
		RestoreGameplayInput();
	}

	if (ResumeSave)
	{
		ApplyResumeSave(ResumeSave);
	}
}

void ATDGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UTDMenuFunctionLibrary::StretchOpenMenuScreens(this);

	// Look for a newly opened settings widget twice a second instead of scanning every widget each frame.
	SettingsBindTimer += DeltaSeconds;
	if (SettingsBindTimer >= 0.5f)
	{
		SettingsBindTimer = 0.0f;
		BindSettingsButtons();
	}
	if (DefenderUpgrades)
	{
		DefenderUpgrades->BindOpenScreen();
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

void ATDGameMode::ShowUpgradeInsufficientFundsWarning()
{
	if (WarningBannerWidget)
	{
		WarningBannerWidget->ShowWarning(TEXT("Not enough resources for this upgrade"));
	}
}

void ATDGameMode::RestartGame()
{
	if (UTDSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr)
	{
		Saves->StartNewGame();
	}

	bPaused = false;
	bGameOver = false;
	BeamUpgradeLevel = 0;
	MatchDefendersPlaced = 0;
	MatchHitsLanded = 0;
	MatchEnemiesKilled = 0;
	PaidMatchRewards = FMetaCurrencyRewards();
	bWaveResultsVisible = false;
	UGameplayStatics::SetGamePaused(this, false);

	if (EndScreenWidget)
	{
		EndScreenWidget->HideScreen();
	}
	if (VictoryScreenWidget)
	{
		VictoryScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (SettingsWidget)
	{
		SettingsWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	UGameplayStatics::OpenLevel(this, FName(TEXT("TowerDefense")));
}

void ATDGameMode::ReturnToMainMenu()
{
	bPaused = false;
	bGameOver = false;
	bWaveResultsVisible = false;
	UGameplayStatics::SetGamePaused(this, false);

	if (SettingsWidget)
	{
		SettingsWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	HideEndScreens();
	UGameplayStatics::OpenLevel(this, FName(TEXT("StartScreenLvl")));
}

void ATDGameMode::PlayMatchVFX(const TSoftObjectPtr<UNiagaraSystem>& System, const FLinearColor& Color, float Lifetime) const
{
	if (Tower)
	{
		TDVFX::Play(this, System, Tower->GetActorLocation(), Color, Lifetime); //  pooled, stops after Lifetime
	}
}

#include "TDGameMode_UI.inl"
#include "TDGameMode_World.inl"
#include "TDGameMode_Match.inl"
#include "TDGameMode_Save.inl"
