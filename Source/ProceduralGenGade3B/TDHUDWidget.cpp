// TDHUDWidget.cpp. See TDHUDWidget.h for an overview.

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

    //Hook the 3 defender buttons up to the player controller
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

    //Basic starts selected, so it gets the bright yellow
    if (BasicDefenderButton)
    {
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(1.0f, 0.85f, 0.05f));
    }

    //Archer starts as a darker green
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.15f, 0.55f, 0.2f));
    }

    //Poison Light Bomb starts as a darker red
    if (PoisonLightBombButton)
    {
        PoisonLightBombButton->SetBackgroundColor(
            FLinearColor(0.65f, 0.08f, 0.08f));
    }

    GameMode->RefreshMetaHUD();
}

void UTDHUDWidget::BindDefenderButtons()
{
    //Link each HUD button to its defender select function
    //RemoveAll first so we don't bind twice if the HUD gets set up again
    if (BasicDefenderButton)
    {
        BasicDefenderButton->OnClicked.RemoveAll(this);

        BasicDefenderButton->OnClicked.AddDynamic(
            this,
            &UTDHUDWidget::HandleBasicDefenderClicked);

        //Basic Defender button is yellow
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(0.75f, 0.60f, 0.03f));
    }

    
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->OnClicked.RemoveAll(this);

        ArcherDefenderButton->OnClicked.AddDynamic(
            this,
            &UTDHUDWidget::HandleArcherDefenderClicked);

        //Archer Defender button is green
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.15f, 0.55f, 0.2f));
    }

    if (PoisonLightBombButton)
    {
        PoisonLightBombButton->OnClicked.RemoveAll(this);

        PoisonLightBombButton->OnClicked.AddDynamic(
            this,
            &UTDHUDWidget::HandlePoisonLightBombClicked);

        //Poison Light Bomb button is red
        PoisonLightBombButton->SetBackgroundColor(
            FLinearColor(0.65f, 0.08f, 0.08f));
    }
}

void UTDHUDWidget::HandleBasicDefenderClicked()
{
    //Select the basic defender
    if (ATDPlayerController* PC =
        GetWorld()->GetFirstPlayerController<ATDPlayerController>())
    {
        PC->SelectBasicDefender();
    }

    //Basic button goes bright yellow when selected
    if (BasicDefenderButton)
    {
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(1.0f, 0.85f, 0.05f));
    }

    //Archer button goes darker when not selected
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.15f, 0.55f, 0.2f));
    }

    //Bomb button goes darker when not selected
    if (PoisonLightBombButton)
    {
        PoisonLightBombButton->SetBackgroundColor(
            FLinearColor(0.65f, 0.08f, 0.08f));
    }
}

void UTDHUDWidget::HandleArcherDefenderClicked()
{
    //Select the archer defender
    if (ATDPlayerController* PC =
        GetWorld()->GetFirstPlayerController<ATDPlayerController>())
    {
        PC->SelectArcherDefender();
    }

    //Basic button goes darker when not selected
    if (BasicDefenderButton)
    {
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(0.75f, 0.60f, 0.03f));
    }

    //Archer button goes bright green when selected
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.25f, 1.0f, 0.3f));
    }

    //Bomb button goes darker when not selected
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

    //Basic button goes darker when not selected
    if (BasicDefenderButton)
    {
        BasicDefenderButton->SetBackgroundColor(
            FLinearColor(0.75f, 0.60f, 0.03f));
    }

    //Archer button goes darker when not selected
    if (ArcherDefenderButton)
    {
        ArcherDefenderButton->SetBackgroundColor(
            FLinearColor(0.15f, 0.55f, 0.2f));
    }

    //Bomb button goes bright red when selected
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

//Empty for now, it just gives the declared function a body
void UTDHUDWidget::ResolveHudBindings()
{
}

//Binds the HUD buttons
void UTDHUDWidget::BindHudButtons()
{
    BindDefenderButtons();
}

//Empty, the Widget Blueprint handles the HUD layout and hit testing
void UTDHUDWidget::ConfigureHudHitTesting()
{
}

//Empty, the anchors in the Widget Blueprint handle stretching the HUD
void UTDHUDWidget::StretchTopBanner()
{
}