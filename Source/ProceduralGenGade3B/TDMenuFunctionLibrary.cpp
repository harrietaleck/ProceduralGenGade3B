#include "TDMenuFunctionLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UTDMenuFunctionLibrary::StartGameplayFromMenu(UObject* WorldContextObject, FName GameplayLevelName)
{
	if (!WorldContextObject || GameplayLevelName.IsNone())
	{
		return;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		UWidgetBlueprintLibrary::RemoveAllWidgets(PC);
	}

	UGameplayStatics::OpenLevel(World, GameplayLevelName);
}
