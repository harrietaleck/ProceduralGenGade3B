// TDHUDWidget.cpp — see TDHUDWidget.h for the overview.

#include "TDHUDWidget.h"

#include "TDGameMode.h"
#include "TDPlayerController.h"
#include "Tower.h"
#include "WaveManager.h"
#include "HealthComponent.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"

void UTDHUDWidget::InitializeHUD(ATDGameMode* InGameMode)
{
    GameMode = InGameMode;

    if (!GameMode)
    {
        return;
    }

    //Connect the 3 defenders to the player controller
    BindDefenderButtons();

    if (PauseButton)
    {
        PauseButton->OnClicked.AddDynamic(
            this,
            &UTDHUDWidget::HandlePauseClicked);
    }

    if (SettingButton)
    {
        SettingButton->OnClicked.AddDynamic(
            this,
            &UTDHUDWidget::HandleSettingsClicked);
    }

    Tower = GameMode->GetTower();
    WaveManagerRef = GameMode->GetWaveManager();

    GameMode->OnResourcesChanged.AddDynamic(
        this,
        &UTDHUDWidget::HandleResourcesChanged);

    if (Tower && Tower->HealthComponent)
    {
        Tower->HealthComponent->OnHealthChanged.AddDynamic(
            this,
            &UTDHUDWidget::HandleTowerHealthChanged);

        HandleTowerHealthChanged(
            Tower->HealthComponent->GetCurrentHealth(),
            Tower->HealthComponent->MaxHealth);
    }

    if (WaveManagerRef)
    {
        WaveManagerRef->OnWaveCountdownTick.AddDynamic(
            this,
            &UTDHUDWidget::HandleWaveCountdownTick);

        WaveManagerRef->OnWaveStarted.AddDynamic(
            this,
            &UTDHUDWidget::HandleWaveStarted);

        WaveManagerRef->OnWaveComplete.AddDynamic(
            this,
            &UTDHUDWidget::HandleWaveComplete);

        WaveManagerRef->OnEnemiesRemainingChanged.AddDynamic(
            this,
            &UTDHUDWidget::HandleEnemiesRemainingChanged);

        WaveManagerRef->OnVictory.AddDynamic(
            this,
            &UTDHUDWidget::HandleVictory);
    }

    HandleResourcesChanged(GameMode->GetResources());

    RefreshWaveCounter();

    if (WaveStatusText)
    {
        WaveStatusText->SetText(
            FText::FromString(
                TEXT("Preparing...")));
    }

    if (EnemiesRemainingText)
    {
        EnemiesRemainingText->SetText(
            FText::FromString(
                TEXT("0 Remaining")));
    }

    //Basic is selected when the HUD first appears
    if (BasicDefenderButton)
    {
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(1.0f, 0.85f, 0.05f));
    }

    //Archer starts darker green.
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.15f, 0.55f, 0.2f));
    }

    //Poison Light Bomb starts darker red.
    if (PoisonLightBombButton)
    {
        PoisonLightBombButton->SetBackgroundColor(
            FLinearColor(0.65f, 0.08f, 0.08f));
    }

    GameMode->RefreshMetaHUD();
}

void UTDHUDWidget::BindDefenderButtons()
{
    //Connect the three HUD buttons to their defender selection functions
    //Prevent duplicate button bindings when the HUD is initialized more than once
    if (BasicDefenderButton)
    {
        BasicDefenderButton->OnClicked.RemoveAll(this);

        BasicDefenderButton->OnClicked.AddDynamic(
            this,
            &UTDHUDWidget::HandleBasicDefenderClicked);

        //Give the Basic Defender button its yellow colour
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(0.75f, 0.60f, 0.03f));
    }

    
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->OnClicked.RemoveAll(this);

        ArcherDefenderButton->OnClicked.AddDynamic(
            this,
            &UTDHUDWidget::HandleArcherDefenderClicked);

        //Give the Archer Defender button its green colour
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.15f, 0.55f, 0.2f));
    }

    if (PoisonLightBombButton)
    {
        PoisonLightBombButton->OnClicked.RemoveAll(this);

        PoisonLightBombButton->OnClicked.AddDynamic(
            this,
            &UTDHUDWidget::HandlePoisonLightBombClicked);

        //Give the Poison Light Bomb button its red colour
        PoisonLightBombButton->SetBackgroundColor(
            FLinearColor(0.65f, 0.08f, 0.08f));
    }
}

void UTDHUDWidget::HandleBasicDefenderClicked()
{
    //Select basic defender
    if (ATDPlayerController* PC =
        GetWorld()->GetFirstPlayerController<ATDPlayerController>())
    {
        PC->SelectBasicDefender();
    }

    //Basic BTN becomes yellow whenn  selected
    if (BasicDefenderButton)
    {
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(1.0f, 0.85f, 0.05f));
    }

    //Archer BTN becomes darker whenn not selected
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.15f, 0.55f, 0.2f));
    }

    //Bomb BTN becomes darker whenn not selected
    if (PoisonLightBombButton)
    {
        PoisonLightBombButton->SetBackgroundColor(
            FLinearColor(0.65f, 0.08f, 0.08f));
    }
}

void UTDHUDWidget::HandleArcherDefenderClicked()
{
    //Select archer defender
    if (ATDPlayerController* PC =
        GetWorld()->GetFirstPlayerController<ATDPlayerController>())
    {
        PC->SelectArcherDefender();
    }

    //Basic BTN becomes darker whenn not selected
    if (BasicDefenderButton)
    {
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(0.75f, 0.60f, 0.03f));
    }

    //Archer BTN becomes green whenn  selected
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.25f, 1.0f, 0.3f));
    }

    //Bomb BTN becomes darker whenn not selected
    if (PoisonLightBombButton)
    {
        PoisonLightBombButton->SetBackgroundColor(
            FLinearColor(0.65f, 0.08f, 0.08f));
    }
}

void UTDHUDWidget::HandlePoisonLightBombClicked()
{
    //Select the Poison Light Bomb defender
    if (ATDPlayerController* PC =
        GetWorld()->GetFirstPlayerController<ATDPlayerController>())
    {
        PC->SelectPoisonLightBombDefender();
    }

    //Basic BTN becomes darker whenn not selected
    if (BasicDefenderButton)
    {
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(0.75f, 0.60f, 0.03f));
    }

    //Archer BTN becomes darker whenn not selected
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.15f, 0.55f, 0.2f));
    }

    //Bomb button become bright red when selected
    if (PoisonLightBombButton)
    {
        PoisonLightBombButton->SetBackgroundColor(
            FLinearColor(1.0f, 0.12f, 0.12f));
    }
}

void UTDHUDWidget::HandleResourcesChanged(int32 NewAmount)
{
    if (LootText)
    {
        LootText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("Loot: %d"),
                    NewAmount)));
    }
}

void UTDHUDWidget::HandleTowerHealthChanged(
    float CurrentHealth,
    float MaxHealth)
{
    const float Percent =
        MaxHealth > 0.0f
        ? CurrentHealth / MaxHealth
        : 0.0f;

    if (TowerHealthBar)
    {
        TowerHealthBar->SetPercent(Percent);
    }

    if (TowerHealthText)
    {
        TowerHealthText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("%d / %d HP"),
                    FMath::RoundToInt(CurrentHealth),
                    FMath::RoundToInt(MaxHealth))));
    }

    UpdateTowerHealthBarColor(Percent);
}

void UTDHUDWidget::UpdateTowerHealthBarColor(
    float HealthFraction)
{
    if (!TowerHealthBar)
    {
        return;
    }

    if (HealthFraction > 0.6f)
    {
        TowerHealthBar->SetFillColorAndOpacity(
            FLinearColor(0.15f, 0.85f, 0.25f));
    }
    else if (HealthFraction > 0.3f)
    {
        TowerHealthBar->SetFillColorAndOpacity(
            FLinearColor(0.95f, 0.85f, 0.1f));
    }
    else
    {
        TowerHealthBar->SetFillColorAndOpacity(
            FLinearColor(0.9f, 0.15f, 0.15f));
    }
}

void UTDHUDWidget::HandleWaveCountdownTick(
    int32 SecondsRemaining)
{
    if (WaveStatusText)
    {
        WaveStatusText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("Wave Starting: %d"),
                    SecondsRemaining)));
    }

    RefreshWaveCounter();
}

void UTDHUDWidget::HandleWaveStarted(int32 WaveNumber)
{
    if (WaveStatusText)
    {
        WaveStatusText->SetText(
            FText::FromString(TEXT("Wave Active")));
    }

    RefreshWaveCounter();
}

void UTDHUDWidget::HandleWaveComplete(int32 WaveNumber)
{
    if (WaveStatusText)
    {
        WaveStatusText->SetText(
            FText::FromString(TEXT("Wave Complete")));
    }

    RefreshWaveCounter();
}

void UTDHUDWidget::HandleEnemiesRemainingChanged(
    int32 Remaining)
{
    if (EnemiesRemainingText)
    {
        EnemiesRemainingText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("%d Remaining"),
                    Remaining)));
    }
}

void UTDHUDWidget::HandleVictory()
{
    if (WaveStatusText)
    {
        WaveStatusText->SetText(
            FText::FromString(TEXT("Victory")));
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
    if (WaveText && WaveManagerRef)
    {
        WaveText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("%d / %d"),
                    WaveManagerRef->GetCurrentWave(),
                    WaveManagerRef->GetTotalWaves())));
    }
}

void UTDHUDWidget::RefreshMatchScore(
    int32 MatchScore)
{
    if (Score)
    {
        Score->SetText(
            FText::AsNumber(MatchScore));
    }
}

void UTDHUDWidget::RefreshMetaCurrency(
    const FMetaCurrencyRewards& Wallet,
    int32 BeamLevel,
    bool bStrongDefenderSelected)
{
    if (forestScore)
    {
        forestScore->SetText(
            FText::AsNumber(Wallet.ForestEssence));
    }

    if (WoodScore)
    {
        WoodScore->SetText(
            FText::AsNumber(Wallet.WoodenMight));
    }

    if (GemScore)
    {
        GemScore->SetText(
            FText::AsNumber(Wallet.GemStones));
    }

    if (LightScore)
    {
        LightScore->SetText(
            FText::AsNumber(Wallet.LightLanterns));
    }

    if (MetaCurrencyText)
    {
        MetaCurrencyText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("Essence %d | Wood %d | Gems %d | Lanterns %d"),
                    Wallet.ForestEssence,
                    Wallet.WoodenMight,
                    Wallet.GemStones,
                    Wallet.LightLanterns)));
    }

    if (DefenderModeText)
    {
        DefenderModeText->SetText(
            FText::FromString(
                bStrongDefenderSelected
                ? TEXT("Strong Defender")
                : TEXT("Basic Defender")));
    }

    RefreshMatchScore(
        GameMode
        ? GameMode->GetLiveMatchScore()
        : 0);
}

void UTDHUDWidget::FlashLootInsufficient()
{
    if (LootText)
    {
        LootText->SetColorAndOpacity(
            FLinearColor(1.0f, 0.2f, 0.2f));
    }
}

void UTDHUDWidget::HideLootFlash()
{
    if (LootText)
    {
        LootText->SetColorAndOpacity(
            DefaultLootColor);
    }
}

//Placed to have match implementations declared
void UTDHUDWidget::ResolveHudBindings()
{
}

//Buttton bind
void UTDHUDWidget::BindHudButtons()
{
    BindDefenderButtons();
}

//Keep the Hud layout for the widget blueprint
void UTDHUDWidget::ConfigureHudHitTesting()
{
}

//HUD stretching is controlled by the Widget Blueprint anchors
void UTDHUDWidget::StretchTopBanner()
{
}