// TDWarningBannerWidget.h
// Small popup at the bottom centre when you can't place a defender, like when you're out of Loot.
// It sits on its own viewport layer so it doesn't mess with the match HUD layout.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDWarningBannerWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;

UCLASS()
class PROCEDURALGENGADE3B_API UTDWarningBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowWarning(const FString& Message, float DurationSeconds = 2.5f);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideWarning();

private:
	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UBorder> Banner;

	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;

	FTimerHandle HideTimerHandle;

	UFUNCTION()
	void HideWarningInternal();
};
