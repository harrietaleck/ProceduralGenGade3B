// TDWarningBannerWidget.h
// Bottom-centre toast for placement rejections (e.g. not enough Loot). Lives on its own
// viewport layer so it never fights with the match HUD layout or Wave Active text.

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
