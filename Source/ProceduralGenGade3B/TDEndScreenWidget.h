// TDEndScreenWidget.h
// Full-screen game over / victory overlay built entirely in C++ (no widget blueprint required).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDEndScreenWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;

UCLASS()
class PROCEDURALGENGADE3B_API UTDEndScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowGameOver();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowVictory();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideScreen();

private:
	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UBorder> DimOverlay;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;

	void PresentScreen(const FString& Title, const FLinearColor& TitleColor);
};
