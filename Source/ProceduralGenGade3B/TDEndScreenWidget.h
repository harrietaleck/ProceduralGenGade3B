// TDEndScreenWidget.h
// The victory and defeat screen. C++ handles the logic and reward numbers.
// A Widget Blueprint child gives the art layout through optional bindings.

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

	/** Shows a match result on the screen. Can be called from C++ or Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void PresentMatchResult(bool bVictory, const FMatchResult& Result);

	/** Called after the result is shown. Add any extra Blueprint styling here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnMatchResultPresented(bool bVictory, const FMatchResult& Result);

protected:
	/** Optional root canvas. If it is missing, C++ builds a simple fullscreen layout instead. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DimOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScaleBox> ScreenScaleBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PanelBorder;

	/** The victory panel art, from the left half of the concept sheet. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> VictoryBackground;

	/** The defeat panel art, from the right half of the concept sheet. */
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
