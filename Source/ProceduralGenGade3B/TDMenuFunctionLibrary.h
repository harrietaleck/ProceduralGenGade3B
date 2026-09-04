// Blueprint helpers for menu screens (StartScreen, etc.). UI stays in Widget Blueprints;
// these nodes only handle leaving the menu into gameplay and fullscreen layout.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TDMenuFunctionLibrary.generated.h"

class UUserWidget;

UCLASS()
class PROCEDURALGENGADE3B_API UTDMenuFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Removes all widgets and opens the gameplay map. Wire StartScreen PlayButton to this. */
	UFUNCTION(BlueprintCallable, Category = "Menu", meta = (WorldContext = "WorldContextObject"))
	static void StartGameplayFromMenu(UObject* WorldContextObject, FName GameplayLevelName = TEXT("TowerDefense"));

	/** Pins a menu widget to the full viewport and stretches its background image. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	static void StretchWidgetToFillScreen(UUserWidget* Widget, bool bCaptureMouseFocus = true);

	/** Stretch Start / Settings / Upgrades / Defenders store widgets currently in the viewport. */
	UFUNCTION(BlueprintCallable, Category = "Menu", meta = (WorldContext = "WorldContextObject"))
	static void StretchOpenMenuScreens(UObject* WorldContextObject);
};
