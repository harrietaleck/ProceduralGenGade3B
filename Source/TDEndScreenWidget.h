// TDEndScreenWidget.h
// Victory / defeat overlay. C++ drives logic and tiered reward numbers; a Widget Blueprint
// child supplies the art layout via BindWidgetOptional bindings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDMatchRewards.h"
#include "TDEndScreenWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UScaleBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class PROCEDURALGENGADE3B_API UTDEndScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowGameOver();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowVictory();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideScreen();

	/** Push a computed match result into bound widgets (works from C++ or Blueprint graphs). */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void PresentMatchResult(bool bVictory, const FMatchResult& Result);

	/** Fired after PresentMatchResult updates the UI — hook custom Blueprint styling here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnMatchResultPresented(bool bVictory, const FMatchResult& Result);

protected:
	/** Optional designer root — if absent, C++ builds a fullscreen fallback layout. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DimOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScaleBox> ScreenScaleBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PanelBorder;

	/** Full victory panel art (left half of the concept sheet). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> VictoryBackground;

	/** Full defeat panel art (right half of the concept sheet). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DefeatBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreHeaderText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RewardsHeaderText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ForestEssenceText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WoodenMightText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GemStonesText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LightLanternsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BeamHealthText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TierText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WavesText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> NextWaveButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;

private:
	bool bBuiltFallbackLayout = false;

	void EnsureFallbackLayout();
	void EnsureThemeArt();
	void EnsureBlueprintLayoutFitsScreen();
	void ResolveOptionalWidgetBindings();
	void EnsureResultTextWidgets();
	void BindActionButtons();
	void LayoutDefeatResultWidgets();
	void ApplyMatchResultToWidgets(const FMatchResult& Result);
	void ApplyTheme(bool bVictory, EBeamHealthTier Tier, bool bOfferNextWave);
	void ApplyTierTypography(const FMatchResult& Result);

	bool bPendingNextWave = false;
	bool bPresentedVictory = false;

	UFUNCTION()
	void OnRetryClicked();

	UFUNCTION()
	void OnNextWaveClicked();

	UFUNCTION()
	void OnMainMenuClicked();
};
