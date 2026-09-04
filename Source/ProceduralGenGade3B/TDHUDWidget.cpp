// TDHUDWidget.cpp — see TDHUDWidget.h for the overview.

#include "TDHUDWidget.h"
#include "TDGameMode.h"
#include "Tower.h"
#include "WaveManager.h"
#include "HealthComponent.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "TimerManager.h"
#include "Widgets/Layout/Anchors.h"

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

void UTDHUDWidget::ResolveHudBindings()
{
	auto ResolveText = [this](TObjectPtr<UTextBlock>& Member, const TCHAR* const* Names, int32 Count)
	{
		if (Member)
		{
			return;
		}
		for (int32 i = 0; i < Count; ++i)
		{
			if (UTextBlock* Found = Cast<UTextBlock>(GetWidgetFromName(Names[i])))
			{
				Member = Found;
				return;
			}
		}
	};

	auto ResolveButton = [this](TObjectPtr<UButton>& Member, const TCHAR* const* Names, int32 Count)
	{
		if (Member)
		{
			return;
		}
		for (int32 i = 0; i < Count; ++i)
		{
			if (UButton* Found = Cast<UButton>(GetWidgetFromName(Names[i])))
			{
				Member = Found;
				return;
			}
		}
	};

	const TCHAR* ScoreNames[] = { TEXT("Score"), TEXT("Score_1"), TEXT("ScoreValueText") };
	const TCHAR* ForestNames[] = { TEXT("forestScore"), TEXT("ForestEssenceText"), TEXT("EssenceText") };
	const TCHAR* WoodNames[] = { TEXT("WoodScore"), TEXT("WoodenMightText"), TEXT("WoodText") };
	const TCHAR* GemNames[] = { TEXT("GemScore"), TEXT("GemStonesText"), TEXT("GemsText") };
	const TCHAR* LightNames[] = { TEXT("LightScore"), TEXT("LightLanternsText"), TEXT("LanternsText") };
	const TCHAR* PauseNames[] = { TEXT("PauseButton") };
	const TCHAR* SettingNames[] = { TEXT("SettingButton"), TEXT("SettingsButton") };

	ResolveText(Score, ScoreNames, UE_ARRAY_COUNT(ScoreNames));
	ResolveText(forestScore, ForestNames, UE_ARRAY_COUNT(ForestNames));
	ResolveText(WoodScore, WoodNames, UE_ARRAY_COUNT(WoodNames));
	ResolveText(GemScore, GemNames, UE_ARRAY_COUNT(GemNames));
	ResolveText(LightScore, LightNames, UE_ARRAY_COUNT(LightNames));
	ResolveButton(PauseButton, PauseNames, UE_ARRAY_COUNT(PauseNames));
	ResolveButton(SettingButton, SettingNames, UE_ARRAY_COUNT(SettingNames));
}

void UTDHUDWidget::BindHudButtons()
{
	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveAll(this);
		PauseButton->OnClicked.AddDynamic(this, &UTDHUDWidget::HandlePauseClicked);
		PauseButton->SetVisibility(ESlateVisibility::Visible);
	}
	if (SettingButton)
	{
		SettingButton->OnClicked.RemoveAll(this);
		SettingButton->OnClicked.AddDynamic(this, &UTDHUDWidget::HandleSettingsClicked);
		SettingButton->SetVisibility(ESlateVisibility::Visible);
	}
}

void UTDHUDWidget::ConfigureHudHitTesting()
{
	// Full-screen RootCanvas must not eat world clicks (placement / camera).
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UWidget* Root = GetRootWidget())
	{
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UTDHUDWidget::StretchTopBanner()
{
	// Pin the whole match HUD to the viewport so a designer fixed size cannot leave
	// a black strip beside the top bar.
	if (UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get())
	{
		if (ViewportSubsystem->IsWidgetAdded(this))
		{
			FGameViewportWidgetSlot ViewportSlot = ViewportSubsystem->GetWidgetSlot(this);
			ViewportSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			ViewportSlot.Offsets = FMargin(0.0f);
			ViewportSlot.Alignment = FVector2D(0.0f, 0.0f);
			ViewportSubsystem->SetWidgetSlot(this, ViewportSlot);
		}
	}
	else
	{
		SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
	}

	if (USizeBox* RootSize = Cast<USizeBox>(GetRootWidget()))
	{
		RootSize->ClearWidthOverride();
		RootSize->ClearHeightOverride();
	}

	auto StretchBannerImage = [](UImage* Image)
	{
		if (!Image)
		{
			return;
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
		{
			const FMargin Old = CanvasSlot->GetOffsets();
			float Height = Old.Bottom;
			if (Height < 1.0f)
			{
				Height = CanvasSlot->GetSize().Y;
			}
			if (Height < 1.0f)
			{
				Height = 160.0f;
			}

			// Full horizontal stretch, pinned to the top edge.
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
			CanvasSlot->SetOffsets(FMargin(0.0f, Old.Top, 0.0f, Height));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			CanvasSlot->SetAutoSize(false);
		}

		FSlateBrush Brush = Image->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Image->SetBrush(Brush);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	};

	// Designer names used by WBP_MatchHUD_V2 for the ornate top strip.
	const TCHAR* BannerNames[] = {
		TEXT("HUD-stand"),
		TEXT("Top"),
		TEXT("HUD-UPDATE"),
		TEXT("HUDStand"),
		TEXT("TopBanner"),
		TEXT("TopBar")
	};

	for (const TCHAR* Name : BannerNames)
	{
		if (UImage* Banner = Cast<UImage>(GetWidgetFromName(Name)))
		{
			StretchBannerImage(Banner);
		}
	}
}

void UTDHUDWidget::InitializeHUD(ATDGameMode* InGameMode)
{
	GameMode = InGameMode;
	if (!GameMode)
	{
		return;
	}

	ResolveHudBindings();
	ConfigureHudHitTesting();
	StretchTopBanner();
	BindHudButtons();

	Tower = GameMode->GetTower();
	WaveManagerRef = GameMode->GetWaveManager();

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

	GameMode->RefreshMetaHUD();
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
	if (GameMode)
	{
		GameMode->RefreshMetaHUD();
	}
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

void UTDHUDWidget::HandleSettingsClicked()
{
	if (GameMode)
	{
		GameMode->ShowSettings();
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

void UTDHUDWidget::RefreshMatchScore(int32 MatchScore)
{
	auto SetNumber = [](UTextBlock* Text, int32 Value)
	{
		if (Text)
		{
			Text->SetText(FText::AsNumber(Value));
		}
	};

	SetNumber(Score, MatchScore);
	if (UTextBlock* ScoreAlt = Cast<UTextBlock>(GetWidgetFromName(TEXT("Score_1"))))
	{
		SetNumber(ScoreAlt, MatchScore);
	}
}

void UTDHUDWidget::RefreshMetaCurrency(const FMetaCurrencyRewards& Wallet, int32 BeamLevel, bool bStrongDefenderSelected)
{
	ResolveHudBindings();

	auto SetNumber = [](UTextBlock* Text, int32 Value)
	{
		if (Text)
		{
			Text->SetText(FText::AsNumber(Value));
		}
	};

	SetNumber(forestScore, Wallet.ForestEssence);
	SetNumber(WoodScore, Wallet.WoodenMight);
	SetNumber(GemScore, Wallet.GemStones);
	SetNumber(LightScore, Wallet.LightLanterns);

	if (GameMode)
	{
		RefreshMatchScore(GameMode->GetLiveMatchScore());
	}

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
