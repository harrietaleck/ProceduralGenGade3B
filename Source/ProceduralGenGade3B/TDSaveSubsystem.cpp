
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

// Sets the object up before first use.
void UTDSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UTDMetaProgressionSubsystem>();

    if (UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
    {
        Meta->OnMetaCurrencyChanged.AddUniqueDynamic(this, &UTDSaveSubsystem::HandleMetaCurrencyChanged);
    }

    // Bring the meta currencies back after the application was closed.
    if (const UTDSaveGame* Save = LoadSave())
    {
        //  Restore the shared music level from the existing persistent save slot.
        MusicVolume = FMath::Clamp(Save->MusicVolume, 0.0f, 1.0f);
        TowerMaxHealthBonus = FMath::Max(0, Save->TowerMaxHealthBonus);
        if (UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
        {
            Meta->SetWallet(Save->Wallet);
        }
    }
}

// Loads the save slot. Returns null if it is missing or unreadable.
UTDSaveGame* UTDSaveSubsystem::LoadSave() const
{
    if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
    {
        return nullptr;
    }

    // A corrupted, empty or older-format file loads as null or as the wrong class.
    UTDSaveGame* Save = Cast<UTDSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));

    if (!Save || Save->SaveVersion <= 0 || Save->SaveVersion > UTDSaveGame::CurrentVersion)
    {
        UE_LOG(LogTemp, Warning, TEXT("TDSave: save slot '%s' is unreadable or outdated and was ignored."), SaveSlotName);
        return nullptr;
    }

    if (Save->SaveVersion < UTDSaveGame::CurrentVersion)
    {
        // Version 1 checkpoints were captured after waves and must keep that resume behavior.
        Save->UpgradePhases.Reset();
        Save->UpgradeSecondsLeft.Reset();
        Save->bResumeAtCurrentWave = false;
        Save->SaveVersion = UTDSaveGame::CurrentVersion;
    }

    return Save;
}

// Writes the save slot to disk.
bool UTDSaveSubsystem::WriteSave(UTDSaveGame* Save) const
{
    if (!Save)
    {
        return false;
    }

    Save->SaveVersion = UTDSaveGame::CurrentVersion;
    Save->SavedAt = FDateTime::Now();
    //  Carry the selected volume through every match checkpoint and wallet save.
    Save->MusicVolume = MusicVolume;
    Save->TowerMaxHealthBonus = TowerMaxHealthBonus;

    const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, SaveUserIndex);

    UE_LOG(LogTemp, Display, TEXT("TDSave: %s save slot '%s' (wave %d cleared)."),
        bSaved ? TEXT("wrote") : TEXT("FAILED to write"), SaveSlotName, Save->WavesCleared);

    return bSaved;
}

// Handle meta currency changed.
void UTDSaveSubsystem::HandleMetaCurrencyChanged()
{
    UTDSaveGame* Save = LoadSave();
    if (!Save)
    {
        Save = NewObject<UTDSaveGame>(this);
    }

    if (const UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
    {
        Save->Wallet = Meta->GetWallet();
        WriteSave(Save);
    }
}

//  Return the persisted shared volume, defaulting to the manager's initial value.
float UTDSaveSubsystem::GetMusicVolume() const
{
    return MusicVolume;
}

//  Reuse the current save object so a volume change cannot discard match progress.
bool UTDSaveSubsystem::SaveMusicVolume(float NewVolume)
{
    MusicVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
    UTDSaveGame* Save = LoadSave();
    if (!Save)
    {
        Save = NewObject<UTDSaveGame>(this);
        if (UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
        {
            Save->Wallet = Meta->GetWallet();
        }
    }

    return WriteSave(Save);
}

// Get tower max health bonus.
int32 UTDSaveSubsystem::GetTowerMaxHealthBonus() const
{
    return TowerMaxHealthBonus;
}

// Add tower max health bonus.
bool UTDSaveSubsystem::AddTowerMaxHealthBonus(int32 Bonus)
{
    if (Bonus <= 0)
    {
        return false;
    }

    UTDSaveGame* Save = LoadSave();
    if (!Save)
    {
        Save = NewObject<UTDSaveGame>(this);
        if (UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
        {
            Save->Wallet = Meta->GetWallet();
        }
    }

    TowerMaxHealthBonus += Bonus;
    return WriteSave(Save);
}

// True if the slot holds a match that can be resumed.
bool UTDSaveSubsystem::HasResumableSave() const
{
    const UTDSaveGame* Save = LoadSave();
    return Save && Save->IsResumable();
}

// Loads the saved match level; the GameMode then restores the checkpoint.
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

// Returns the save to restore if Resume was pressed, and clears the request.
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

// Wires the Start Screen buttons once per Start Screen instance.
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

// On new game clicked.
void UTDSaveSubsystem::OnNewGameClicked()
{
    StartNewGame();
}

// Starts a clean match and resets the persistent starting wallet.
void UTDSaveSubsystem::StartNewGame()
{
    bResumePending = false;

    if (UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
    {
        Meta->ClearPendingUpgrades();
    }

    FMetaCurrencyRewards StartingWallet;
    StartingWallet.ForestEssence = 40;
    StartingWallet.WoodenMight = 30;
    StartingWallet.GemStones = 12;
    StartingWallet.LightLanterns = 10;

    if (UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
    {
        Meta->SetWallet(StartingWallet);
    }

    UTDSaveGame* Save = LoadSave();
    if (!Save)
    {
        Save = NewObject<UTDSaveGame>(this);
    }
    Save->Wallet = StartingWallet;
    Save->bHasMatch = false;
    Save->WavesCleared = 0;
    Save->bResumeAtCurrentWave = false;
    Save->UpgradePhases.Reset();
    Save->UpgradeSecondsLeft.Reset();
    WriteSave(Save);
}

// On resume clicked.
void UTDSaveSubsystem::OnResumeClicked()
{
    ResumeGame();
}

// On exit clicked.
void UTDSaveSubsystem::OnExitClicked()
{
    if (UWorld* World = GetGameInstance()->GetWorld())
    {
        UKismetSystemLibrary::QuitGame(World, World->GetFirstPlayerController(), EQuitPreference::Quit, false);
    }
}
