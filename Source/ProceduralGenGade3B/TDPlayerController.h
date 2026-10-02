// TDPlayerController.h
// Handles the player's only interaction in Part 1: clicking a buildable slot to place a
// defender. It traces under the mouse cursor, snaps the click to the nearest terrain build
// slot, checks the slot is free and affordable, then spends resources and spawns a defender.
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

    /** Which defender to place (defaults to the C++ ADefender; can be a Blueprint child). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TowerDefense")
    TSubclassOf<ADefender> DefenderClass;

    /** How close (uu, on the ground plane) a click must be to a slot to count as selecting it. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TowerDefense", meta = (ClampMin = "1.0"))
    float SlotClickTolerance = 160.0f;

    /** Elite defender class toggled with Tab (costs Gem Stones from the meta wallet). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TowerDefense|Meta")
    TSubclassOf<ADefender> StrongDefenderClass;

    UFUNCTION(BlueprintPure, Category = "TowerDefense|Meta")
    bool IsPlacingStrongDefender() const { return bPlacingStrongDefender; }

    //Select the basic /original defender from the HUD
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectBasicDefender();

    //Select the archers defender from the HUD
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectArcherDefender();

    //Select the Poison Light Bomb defender from the HUD
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectPoisonLightBombDefender();

    //Select the gem-priced Strong defender (key 4, or toggle with Tab)
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectStrongDefender();

    //Change the defender that is active without chnaging the existing placement system
    UFUNCTION(BlueprintCallable, Category = "TowerDefense|Defenders")
    void SelectDefenderClass(TSubclassOf<ADefender> NewDefenderClass);

    //The defender class a click would place right now (null if nothing is selected)
    TSubclassOf<ADefender> GetActiveDefenderClass() const;

protected:

    virtual void BeginPlay() override;

    virtual void SetupInputComponent() override;

    virtual void Tick(float DeltaSeconds) override;

    /** Left-click handler: attempt to place a defender under the cursor. */
    void OnPlaceDefenderClicked();

    /** R-key handler: restart the match (new procedural map). */
    void OnRestartPressed();

    /** P-key handler: pause/unpause the match. */
    void OnPausePressed();

    /** N-key handler: toggle the engine's NavMesh debug overlay. */
    void OnToggleNavMeshDebug();

    /** Tab: toggle basic vs strong defender placement. */
    void OnToggleDefenderMode();

    /** U: spend Light Lanterns to upgrade the tower beam. */
    void OnUpgradeBeamPressed();

private:

    bool bPlacingStrongDefender = false;

    bool CanAffordDefender(
        const ADefender* Defaults,
        const ATDGameMode* GameMode
    ) const;

    /** True if the terrain's stored slot state says a defender already occupies this world slot. */
    bool IsSlotOccupied(const FVector& SlotLocation) const;

    /** Finds the terrain's build slot nearest the cursor, within SlotClickTolerance. Shared
     *  by the click handler and the every-frame hover highlight so they never disagree. */
    bool FindNearestSlotUnderCursor(FVector& OutSlotLocation) const;

    /** Draws a translucent green (valid) or red (occupied / unaffordable) patch over the
     *  build pad nearest the cursor, each frame, so placement validity is always clear. */
    void UpdateBuildPadHighlight() const;
};