
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

    //  Store music volume in the existing save slot, not a second settings system.
    float GetMusicVolume() const;
    bool SaveMusicVolume(float NewVolume);

    int32 GetTowerMaxHealthBonus() const;
    bool AddTowerMaxHealthBonus(int32 Bonus);

    /** True if the slot holds a match that can be resumed. */
    UFUNCTION(BlueprintPure, Category = "Save")
    bool HasResumableSave() const;

    /** Loads the saved match level; the GameMode then restores the checkpoint. */
    UFUNCTION(BlueprintCallable, Category = "Save")
    void ResumeGame();

    /** Starts a clean match and resets the persistent starting wallet. */
    void StartNewGame();

    /** Returns the save to restore if Resume was pressed, and clears the request. */
    UTDSaveGame* ConsumeResumeRequest();

    /** Wires the Start Screen buttons once per Start Screen instance. */
    void BindStartScreen(UWorld* World);

private:
    float MusicVolume = 0.5f;
    int32 TowerMaxHealthBonus = 0;
    bool bResumePending = false;
    TWeakObjectPtr<UUserWidget> BoundStartScreen;

    UFUNCTION()
    void HandleMetaCurrencyChanged();

    UFUNCTION()
    void OnNewGameClicked();

    UFUNCTION()
    void OnResumeClicked();

    UFUNCTION()
    void OnExitClicked();
};
