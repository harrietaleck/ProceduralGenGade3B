// Blueprint helpers for menu screens (StartScreen, etc.). UI stays in Widget Blueprints;
// these nodes only handle leaving the menu into gameplay.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TDMenuFunctionLibrary.generated.h"

UCLASS()
class PROCEDURALGENGADE3B_API UTDMenuFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Removes all widgets and opens the gameplay map. Wire StartScreen PlayButton to this. */
	UFUNCTION(BlueprintCallable, Category = "Menu", meta = (WorldContext = "WorldContextObject"))
	static void StartGameplayFromMenu(UObject* WorldContextObject, FName GameplayLevelName = TEXT("TowerDefense"));
};
