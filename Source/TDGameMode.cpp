// TDGameMode.cpp — core startup plus included UI / world / match units.
// Companion .inl files keep each physical file under 500 lines while remaining
// one translation unit (required for Live Coding after a file split).

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
#include "HeroCharacter.h"
#include "TDMatchRewards.h"
#include "TDMetaProgressionSubsystem.h"
#include "TDMenuFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Widgets/Layout/Anchors.h"
#include "EngineUtils.h"
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

	StartingMetaWallet.ForestEssence = 40;
	StartingMetaWallet.WoodenMight = 30;
	StartingMetaWallet.GemStones = 12;
	StartingMetaWallet.LightLanterns = 20;
}

void ATDGameMode::BeginPlay()
{
	Super::BeginPlay();

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
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

	if (!bWorldValid)
	{
		UE_LOG(LogTemp, Error, TEXT("TDGameMode: failed to produce a valid world after %d attempts."), MaxWorldAttempts);
		DestroySpawnedWorldActors();
		return;
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
		WaveManager->Initialize(Spawner, true);
		WaveManager->OnWaveComplete.AddDynamic(this, &ATDGameMode::HandleWaveComplete);
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
}

void ATDGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UTDMenuFunctionLibrary::StretchOpenMenuScreens(this);
	BindSettingsButtons();
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

#include "TDGameMode_UI.inl"
#include "TDGameMode_World.inl"
#include "TDGameMode_Match.inl"
