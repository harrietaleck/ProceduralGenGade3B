// TDHUDWidget.h
// The always-visible in-match HUD, built as a real UMG widget (not hand-drawn Canvas) so it
// can host proper anchored layout, styled text, and a health bar. Every value it shows is
// pushed to it by the gameplay systems' own delegates — it never polls, and it never ticks.
// InitializeHUD() is called once, right after the match's actors exist, to bind those
// delegates and do a single first-paint refresh; from then on the widget is purely reactive.

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
	/** Wires this widget up to the current match's GameMode/Tower/WaveManager and does an
	 *  immediate first refresh. Must be called once, after those actors exist (BeginPlay). */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void InitializeHUD(ATDGameMode* InGameMode);

	/** Briefly flash the Loot counter red when placement is rejected. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void FlashLootInsufficient();

	// --- Part A layout: five zones, one widget each, bound by exact name to the matching
	// elements the Widget Blueprint's designer view must contain. ---

	/** Top-left: "Wave 3 / 5". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WaveText;

	/** Top-centre: "Preparing...", "Wave Starting", "Wave Active", "Wave Complete", etc. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WaveStatusText;

	/** Top-right: "Loot: 175". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LootText;

	/** Bottom-left: the Citadel's health bar. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> TowerHealthBar;

	/** Bottom-left: "82 / 100 HP" underneath the bar. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TowerHealthText;

	/** Bottom-right: "7 Remaining". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnemiesRemainingText;

	/** Optional: persistent meta-currency readout (leaf/logs/gems/lanterns). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MetaCurrencyText;

	/** Optional: which defender type is selected + controls hint. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DefenderModeText;

	/** Optional: pause / settings button on the match HUD. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PauseButton;

	/** Optional alternate settings button name used by WBP_MatchHUD_V2. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingButton;

	/** Live match score (WBP_MatchHUD_V2: Score). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Score;

	/** Forest Essence amount (WBP_MatchHUD_V2: forestScore). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> forestScore;

	/** Wooden Might amount (WBP_MatchHUD_V2: WoodScore). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WoodScore;

	/** Gem Stones amount (WBP_MatchHUD_V2: GemScore). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GemScore;

	/** Light Lanterns amount (WBP_MatchHUD_V2: LightScore). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LightScore;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void RefreshMetaCurrency(const FMetaCurrencyRewards& Wallet, int32 BeamLevel, bool bStrongDefenderSelected);

	/** Push the live match score into the HUD Score text. */
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
	void HandleTowerHealthChanged(float CurrentHealth, float MaxHealth);

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

	void ResolveHudBindings();
	void BindHudButtons();
	void ConfigureHudHitTesting();
	void StretchTopBanner();

	/** Refreshes the parts of the display that don't have their own dedicated delegate
	 *  (the "Wave X / Y" counter, which changes alongside several different events). */
	void RefreshWaveCounter();

	/** Applies the Green/Yellow/Red colour rule to the tower health bar for a given fraction. */
	void UpdateTowerHealthBarColor(float HealthFraction);
};
