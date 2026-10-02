// TDPlayerController.h
// Handles player input, mainly clicking a build slot to place a defender.
// It finds the nearest slot under the mouse, checks it is free and that we can pay, then spawns the defender.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TDPlayerController.generated.h"

class ADefender;
class ATDGameMode;

UCLASS()
class PROCEDURALGENGADE3B_API ATDPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ATDPlayerController();

    /** Which defender to place. Starts as the C++ ADefender but can be a Blueprint child. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TowerDefense")
    TSubclassOf<ADefender> DefenderClass;

    /** How close a click has to be to a slot to select it, in Unreal units on the ground. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TowerDefense", meta = (ClampMin = "1.0"))
    float SlotClickTolerance = 160.0f;

    /** The strong defender, switched on with Tab. It costs Gem Stones from the meta wallet. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TowerDefense|Meta")
    TSubclassOf<ADefender> StrongDefenderClass;

    UFUNCTION(BlueprintPure, Category = "TowerDefense|Meta")
    bool IsPlacingStrongDefender() const { return bPlacingStrongDefender; }

    //Select the basic/original defender from the HUD
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectBasicDefender();

    //Select the archers defender from the HUD
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectArcherDefender();

    //Select the Poison Light Bomb defender from the HUD
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectPoisonLightBombDefender();

    //Select the Strong defender that costs gems. Press 4 or toggle with Tab
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectStrongDefender();

    //Change the active defender without changing the existing placement system
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectDefenderClass(TSubclassOf<ADefender> NewDefenderClass);

    //The defender a click would place right now. Null if nothing is selected
    TSubclassOf<ADefender> GetActiveDefenderClass() const;

protected:

    virtual void BeginPlay() override;

    virtual void SetupInputComponent() override;

    virtual void Tick(float DeltaSeconds) override;

    /** Left click tries to place a defender under the cursor. */
    void OnPlaceDefenderClicked();

    /** R restarts the match with a new procedural map. */
    void OnRestartPressed();

    /** P pauses or unpauses the match. */
    void OnPausePressed();

    /** N turns Unreal's NavMesh debug view on or off. */
    void OnToggleNavMeshDebug();

    /** Tab switches between basic and strong defender placement. */
    void OnToggleDefenderMode();

    /** U spends Light Lanterns to upgrade the tower beam. */
    void OnUpgradeBeamPressed();

    /** G shows or hides the wave director graph. */
    void OnToggleDirectorGraph();

private:

    bool bPlacingStrongDefender = false;

    bool CanAffordDefender(
        const ADefender* Defaults,
        const ATDGameMode* GameMode
    ) const;

    /** True if the terrain says a defender is already on this slot. */
    bool IsSlotOccupied(const FVector& SlotLocation) const;

    /** Finds the build slot nearest the cursor, within SlotClickTolerance. The click and
     *  the hover highlight both use this so they always pick the same slot. */
    bool FindNearestSlotUnderCursor(FVector& OutSlotLocation) const;

    /** Draws a see-through patch on the build pad under the cursor every frame.
     *  Green means you can place there, red means it is taken or too expensive. */
    void UpdateBuildPadHighlight() const;
};