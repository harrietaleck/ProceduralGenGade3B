
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TDSaveSubsystem.generated.h"

class UTDSaveGame;
class UUserWidget;

UCLASS()
class PROCEDURALGENGADE3B_API UTDSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Loads the save slot. Returns null if it is missing or unreadable. */
    UTDSaveGame* LoadSave() const;

    /** Writes the save slot to disk. */
    bool WriteSave(UTDSaveGame* Save) const;

    /** True if the slot holds a match that can be resumed. */
    UFUNCTION(BlueprintPure, Category = "Save")
    bool HasResumableSave() const;

    /** Loads the saved match level; the GameMode then restores the checkpoint. */
    UFUNCTION(BlueprintCallable, Category = "Save")
    void ResumeGame();

    /** Returns the save to restore if Resume was pressed, and clears the request. */
    UTDSaveGame* ConsumeResumeRequest();

    /** Wires the Start Screen buttons once per Start Screen instance. */
    void BindStartScreen(UWorld* World);

private:
    bool bResumePending = false;
    TWeakObjectPtr<UUserWidget> BoundStartScreen;

    UFUNCTION()
    void OnNewGameClicked();

    UFUNCTION()
    void OnResumeClicked();

    UFUNCTION()
    void OnExitClicked();
};
