// TDHUDWidget.cpp — see TDHUDWidget.h for the overview.

#include "TDHUDWidget.h"
#include "TDGameMode.h"
#include "Tower.h"
#include "WaveManager.h"
#include "HealthComponent.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "TimerManager.h"

void UTDHUDWidget::FlashLootInsufficient()
{
	if (!LootText)
	{
		return;
	}

	DefaultLootColor = LootText->GetColorAndOpacity().GetSpecifiedColor();
	LootText->SetColorAndOpacity(FLinearColor(1.0f, 0.2f, 0.2f));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LootFlashTimerHandle);
		World->GetTimerManager().SetTimer(
			LootFlashTimerHandle,
			this,
			&UTDHUDWidget::HideLootFlash,
			2.5f,
			/*bLoop=*/false);
	}
}

void UTDHUDWidget::HideLootFlash()
{
	if (LootText)
	{
		LootText->SetColorAndOpacity(DefaultLootColor);
	}
}

void UTDHUDWidget::InitializeHUD(ATDGameMode* InGameMode)
{
	GameMode = InGameMode;
	if (!GameMode)
	{
		return;
	}

	Tower = GameMode->GetTower();
	WaveManagerRef = GameMode->GetWaveManager();

	// Bind once. From here on every value shown is pushed by these delegates — nothing here
	// polls or ticks.
	GameMode->OnResourcesChanged.AddDynamic(this, &UTDHUDWidget::HandleResourcesChanged);

	if (Tower && Tower->HealthComponent)
	{
		Tower->HealthComponent->OnHealthChanged.AddDynamic(this, &UTDHUDWidget::HandleTowerHealthChanged);
	}

	if (WaveManagerRef)
	{
		WaveManagerRef->OnWaveCountdownTick.AddDynamic(this, &UTDHUDWidget::HandleWaveCountdownTick);
		WaveManagerRef->OnWaveStarted.AddDynamic(this, &UTDHUDWidget::HandleWaveStarted);
		WaveManagerRef->OnWaveComplete.AddDynamic(this, &UTDHUDWidget::HandleWaveComplete);
		WaveManagerRef->OnEnemiesRemainingChanged.AddDynamic(this, &UTDHUDWidget::HandleEnemiesRemainingChanged);
		WaveManagerRef->OnVictory.AddDynamic(this, &UTDHUDWidget::HandleVictory);
	}

	if (!PauseButton)
	{
		PauseButton = Cast<UButton>(GetWidgetFromName(TEXT("PauseButton")));
	}
	if (!PauseButton)
	{
		PauseButton = Cast<UButton>(GetWidgetFromName(TEXT("SettingsButton")));
	}
	if (PauseButton)
	{
		PauseButton->OnClicked.AddDynamic(this, &UTDHUDWidget::HandlePauseClicked);
	}

	// One-time first paint so the HUD is correct immediately, before the first real event
	// (e.g. resources already equal StartingResources at this point, but OnResourcesChanged
	// was broadcast slightly earlier in BeginPlay, before this widget existed to hear it).
	HandleResourcesChanged(GameMode->GetResources());
	if (Tower && Tower->HealthComponent)
	{
		HandleTowerHealthChanged(Tower->HealthComponent->GetCurrentHealth(), Tower->HealthComponent->MaxHealth);
	}
	if (WaveStatusText)
	{
		WaveStatusText->SetText(FText::FromString(TEXT("Preparing...")));
	}
	RefreshWaveCounter();
	if (EnemiesRemainingText)
	{
		EnemiesRemainingText->SetText(FText::FromString(TEXT("0 Remaining")));
	}
}

void UTDHUDWidget::HandleResourcesChanged(int32 NewAmount)
{
	if (LootText)
	{
		LootText->SetText(FText::FromString(FString::Printf(TEXT("Loot: %d"), NewAmount)));
	}
}

void UTDHUDWidget::HandleTowerHealthChanged(float CurrentHealth, float MaxHealth)
{
	const float Fraction = MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;

	if (TowerHealthBar)
	{
		TowerHealthBar->SetPercent(Fraction);
	}
	if (TowerHealthText)
	{
		TowerHealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d HP"),
			FMath::RoundToInt(CurrentHealth), FMath::RoundToInt(MaxHealth))));
	}
	UpdateTowerHealthBarColor(Fraction);
}

void UTDHUDWidget::UpdateTowerHealthBarColor(float HealthFraction)
{
	if (!TowerHealthBar)
	{
		return;
	}

	// Green above 60%, yellow between 30-60%, red below 30% — the brief's three-state rule.
	FLinearColor BarColor;
	if (HealthFraction > 0.6f)
	{
		BarColor = FLinearColor(0.15f, 0.85f, 0.25f);
	}
	else if (HealthFraction > 0.3f)
	{
		BarColor = FLinearColor(0.95f, 0.85f, 0.1f);
	}
	else
	{
		BarColor = FLinearColor(0.9f, 0.15f, 0.15f);
	}
	TowerHealthBar->SetFillColorAndOpacity(BarColor);
}

void UTDHUDWidget::HandleWaveCountdownTick(int32 SecondsRemaining)
{
	if (WaveStatusText)
	{
		WaveStatusText->SetText(FText::FromString(
			SecondsRemaining > 0
				? FString::Printf(TEXT("Wave Starting: %d"), SecondsRemaining)
				: TEXT("Wave Starting")));
	}
	RefreshWaveCounter();
}

void UTDHUDWidget::HandleWaveStarted(int32 WaveNumber)
{
	if (WaveStatusText)
	{
		WaveStatusText->SetText(FText::FromString(TEXT("Wave Active")));
	}
	RefreshWaveCounter();
}

void UTDHUDWidget::HandleWaveComplete(int32 WaveNumber)
{
	if (WaveStatusText)
	{
		WaveStatusText->SetText(FText::FromString(TEXT("Wave Complete")));
	}
	RefreshWaveCounter();
}

void UTDHUDWidget::HandleEnemiesRemainingChanged(int32 Remaining)
{
	if (EnemiesRemainingText)
	{
		EnemiesRemainingText->SetText(FText::FromString(FString::Printf(TEXT("%d Remaining"), Remaining)));
	}
}

void UTDHUDWidget::HandleVictory()
{
	if (WaveStatusText)
	{
		WaveStatusText->SetText(FText::FromString(TEXT("Victory")));
	}
	RefreshWaveCounter();
}

void UTDHUDWidget::HandlePauseClicked()
{
	if (GameMode)
	{
		GameMode->TogglePause();
	}
}

void UTDHUDWidget::RefreshWaveCounter()
{
	if (!WaveText || !WaveManagerRef)
	{
		return;
	}
	WaveText->SetText(FText::FromString(FString::Printf(TEXT("Wave %d / %d"),
		WaveManagerRef->GetCurrentWave(), WaveManagerRef->GetTotalWaves())));
}

void UTDHUDWidget::RefreshMetaCurrency(const FMetaCurrencyRewards& Wallet, int32 BeamLevel, bool bStrongDefenderSelected)
{
	if (MetaCurrencyText)
	{
		MetaCurrencyText->SetText(FText::FromString(FString::Printf(
			TEXT("Essence %d | Wood %d | Gems %d | Lanterns %d"),
			Wallet.ForestEssence,
			Wallet.WoodenMight,
			Wallet.GemStones,
			Wallet.LightLanterns)));
	}

	if (DefenderModeText)
	{
		DefenderModeText->SetText(FText::FromString(FString::Printf(
			TEXT("%s Defender | Beam Lv %d | [Tab] swap | [U] upgrade beam"),
			bStrongDefenderSelected ? TEXT("Strong") : TEXT("Basic"),
			BeamLevel)));
	}
}
