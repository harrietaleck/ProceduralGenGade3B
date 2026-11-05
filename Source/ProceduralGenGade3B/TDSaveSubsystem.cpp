// TDSaveSubsystem.cpp - see TDSaveSubsystem.h.

#include "TDSaveSubsystem.h"
#include "TDSaveGame.h"
#include "TDMetaProgressionSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	const TCHAR* SaveSlotName = TEXT("GroveMeadowsSave");
	constexpr int32 SaveUserIndex = 0;
}

void UTDSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTDMetaProgressionSubsystem>();

	// Bring the meta currencies back after the application was closed.
	if (const UTDSaveGame* Save = LoadSave())
	{
		if (UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			Meta->SetWallet(Save->Wallet);
		}
	}
}

UTDSaveGame* UTDSaveSubsystem::LoadSave() const
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		return nullptr;
	}

	// A corrupted, empty or older-format file loads as null or as the wrong class.
	UTDSaveGame* Save = Cast<UTDSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!Save || Save->SaveVersion != UTDSaveGame::CurrentVersion)
	{
		UE_LOG(LogTemp, Warning, TEXT("TDSave: save slot '%s' is unreadable or outdated and was ignored."), SaveSlotName);
		return nullptr;
	}
	return Save;
}

bool UTDSaveSubsystem::WriteSave(UTDSaveGame* Save) const
{
	if (!Save)
	{
		return false;
	}
	Save->SaveVersion = UTDSaveGame::CurrentVersion;
	Save->SavedAt = FDateTime::Now();
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, SaveUserIndex);
	UE_LOG(LogTemp, Display, TEXT("TDSave: %s save slot '%s' (wave %d cleared)."),
		bSaved ? TEXT("wrote") : TEXT("FAILED to write"), SaveSlotName, Save->WavesCleared);
	return bSaved;
}

bool UTDSaveSubsystem::HasResumableSave() const
{
	const UTDSaveGame* Save = LoadSave();
	return Save && Save->IsResumable();
}

void UTDSaveSubsystem::ResumeGame()
{
	const UTDSaveGame* Save = LoadSave();
	if (!Save || !Save->IsResumable())
	{
		UE_LOG(LogTemp, Warning, TEXT("TDSave: no saved game to resume."));
		return;
	}
	bResumePending = true;
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Save->LevelName));
}

UTDSaveGame* UTDSaveSubsystem::ConsumeResumeRequest()
{
	if (!bResumePending)
	{
		return nullptr;
	}
	bResumePending = false;
	UTDSaveGame* Save = LoadSave();
	return Save && Save->IsResumable() ? Save : nullptr;
}

void UTDSaveSubsystem::BindStartScreen(UWorld* World)
{
	if (BoundStartScreen.IsValid() && BoundStartScreen->IsInViewport())
	{
		return;
	}

	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* Widget : Widgets)
	{
		if (!Widget || !Widget->IsInViewport() || !Widget->GetClass()->GetName().Contains(TEXT("StartScreen")))
		{
			continue;
		}
		BoundStartScreen = Widget;

		// PlayButton keeps its own Blueprint OpenLevel; this only makes sure it starts fresh.
		if (UButton* NewGame = Cast<UButton>(Widget->GetWidgetFromName(TEXT("PlayButton"))))
		{
			NewGame->OnClicked.AddUniqueDynamic(this, &UTDSaveSubsystem::OnNewGameClicked);
		}
		if (UButton* Resume = Cast<UButton>(Widget->GetWidgetFromName(TEXT("ResumeBTN"))))
		{
			const bool bCanResume = HasResumableSave();
			Resume->OnClicked.AddUniqueDynamic(this, &UTDSaveSubsystem::OnResumeClicked);
			Resume->SetIsEnabled(bCanResume);
			Resume->SetToolTipText(bCanResume
				? FText::FromString(TEXT("Continue from your last saved wave"))
				: FText::FromString(TEXT("No saved game found")));
		}
		// ExitButton had no click handler in the widget.
		if (UButton* Exit = Cast<UButton>(Widget->GetWidgetFromName(TEXT("ExitButton"))))
		{
			Exit->OnClicked.AddUniqueDynamic(this, &UTDSaveSubsystem::OnExitClicked);
		}
		return;
	}
}

void UTDSaveSubsystem::OnNewGameClicked()
{
	bResumePending = false;
}

void UTDSaveSubsystem::OnResumeClicked()
{
	ResumeGame();
}

void UTDSaveSubsystem::OnExitClicked()
{
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		UKismetSystemLibrary::QuitGame(World, World->GetFirstPlayerController(), EQuitPreference::Quit, false);
	}
}
