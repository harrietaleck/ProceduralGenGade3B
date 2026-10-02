// The main match HUD, made as a UMG widget so it can use anchors, styled text and a health bar.
// It doesn't tick. It only updates when the gameplay delegates tell it something changed.
// InitializeHUD() binds those delegates once and fills in the starting values.

#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"

#include "TDMatchRewards.h"

#include "TDHUDWidget.generated.h"

class ATDGameMode;

class ATower;

class AWaveManager;

class UTextBlock;

class UProgressBar;

class UButton;

enum class EWaveState : uint8;

UCLASS()

class PROCEDURALGENGADE3B_API UTDHUDWidget : public UUserWidget

{

    GENERATED_BODY()

public:

    /** Connects the widget to the game mode, tower and wave manager and shows the starting values.
     *  Call it once in BeginPlay, after those actors exist. */

    UFUNCTION(BlueprintCallable, Category = "HUD")

    void InitializeHUD(ATDGameMode* InGameMode);

    /** Turns the Loot text red when the player can't afford a defender. */

    UFUNCTION(BlueprintCallable, Category = "HUD")

    void FlashLootInsufficient();

    // Part A layout. Each of these binds to a widget with the same name in the Widget Blueprint,
    // so the names have to match.

    /** Top left, shows "Wave 3 / 5". */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> WaveText;

    /** Top centre, shows the wave status like "Preparing..." or "Wave Active". */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> WaveStatusText;

    /** Top right, shows "Loot: 175". */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> LootText;

    /** Bottom left, the tower's health bar. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UProgressBar> TowerHealthBar;

    /** Bottom left, shows "82 / 100 HP" under the bar. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> TowerHealthText;

    /** Bottom right, shows "7 Remaining". */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> EnemiesRemainingText;

    /** Optional text that shows the saved currencies, like essence, wood, gems and lanterns. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> MetaCurrencyText;

    /** Optional text that shows which defender is selected. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> DefenderModeText;

    /** Optional pause button on the match HUD. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UButton> PauseButton;

    /** Optional settings button, this is the name WBP_MatchHUD_V2 uses. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UButton> SettingButton;

    /** The current match score. Called Score in WBP_MatchHUD_V2. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> Score;

    /** How much Forest Essence we have. Called forestScore in WBP_MatchHUD_V2. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> forestScore;

    /** How much Wooden Might we have. Called WoodScore in WBP_MatchHUD_V2. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> WoodScore;

    /** How many Gem Stones we have. Called GemScore in WBP_MatchHUD_V2. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> GemScore;

    /** How many Light Lanterns we have. Called LightScore in WBP_MatchHUD_V2. */

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UTextBlock> LightScore;

    //Button that selects the basic defender

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UButton> BasicDefenderButton;

    //Button that selects the archer defender

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UButton> ArcherDefenderButton;

    //Button that selects the Poison Light Bomb defender

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))

    TObjectPtr<UButton> PoisonLightBombButton;

    UFUNCTION(BlueprintCallable, Category = "HUD")

    void RefreshMetaCurrency(

        const FMetaCurrencyRewards& Wallet,

        int32 BeamLevel,

        bool bStrongDefenderSelected

    );

    /** Updates the Score text with the current match score. */

    UFUNCTION(BlueprintCallable, Category = "HUD")

    void RefreshMatchScore(int32 MatchScore);

private:

    UPROPERTY()

    TObjectPtr<ATDGameMode> GameMode;

    UPROPERTY()

    TObjectPtr<ATower> Tower;

    UPROPERTY()

    TObjectPtr<AWaveManager> WaveManagerRef;

    FTimerHandle LootFlashTimerHandle;

    FLinearColor DefaultLootColor = FLinearColor::White;

    void HideLootFlash();

    UFUNCTION()

    void HandleResourcesChanged(int32 NewAmount);

    UFUNCTION()

    void HandleTowerHealthChanged(

        float CurrentHealth,

        float MaxHealth

    );

    UFUNCTION()

    void HandleWaveCountdownTick(int32 SecondsRemaining);

    UFUNCTION()

    void HandleWaveStarted(int32 WaveNumber);

    UFUNCTION()

    void HandleWaveComplete(int32 WaveNumber);

    UFUNCTION()

    void HandleEnemiesRemainingChanged(int32 Remaining);

    UFUNCTION()

    void HandleVictory();

    UFUNCTION()

    void HandlePauseClicked();

    UFUNCTION()

    void HandleSettingsClicked();

    //Hooks the 3 defender buttons up to the player controller

    void BindDefenderButtons();

    //Runs when the basic defender button is clicked

    UFUNCTION()

    void HandleBasicDefenderClicked();

    //Runs when the archer defender button is clicked

    UFUNCTION()

    void HandleArcherDefenderClicked();

    //Runs when the Poison Light Bomb button is clicked

    UFUNCTION()

    void HandlePoisonLightBombClicked();

    void ResolveHudBindings();

    void BindHudButtons();

    void ConfigureHudHitTesting();

    void StretchTopBanner();

    /** Updates the "Wave X / Y" counter. It has no delegate of its own because
     *  a few different wave events can change it. */

    void RefreshWaveCounter();

    /** Sets the tower health bar to green, yellow or red depending on how much health is left. */

    void UpdateTowerHealthBarColor(float HealthFraction);

};