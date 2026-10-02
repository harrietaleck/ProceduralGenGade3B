// Blueprint helper nodes for the menu screens like StartScreen. The UI itself is in Widget Blueprints.
// These nodes just start the game from the menu and stretch the menus to fill the screen.

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
	/** Removes all widgets and opens the gameplay map. Hook the StartScreen PlayButton up to this. */
	UFUNCTION(BlueprintCallable, Category = "Menu", meta = (WorldContext = "WorldContextObject"))
	static void StartGameplayFromMenu(UObject* WorldContextObject, FName GameplayLevelName = TEXT("TowerDefense"));

	/** Makes a menu widget fill the whole viewport and stretches its background image. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	static void StretchWidgetToFillScreen(UUserWidget* Widget, bool bCaptureMouseFocus = true);

	/** Stretches any Start, Settings, Upgrades or Defenders store screens that are open right now. */
	UFUNCTION(BlueprintCallable, Category = "Menu", meta = (WorldContext = "WorldContextObject"))
	static void StretchOpenMenuScreens(UObject* WorldContextObject);
};
