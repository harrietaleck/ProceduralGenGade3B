// TDPlayerController.h
// Handles the player's only interaction in Part 1: clicking a buildable slot to place a
// defender. It traces under the mouse cursor, snaps the click to the nearest terrain build
// slot, checks the slot is free and affordable, then spends resources and spawns a defender.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TDPlayerController.generated.h"

class ADefender;

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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Left-click handler: attempt to place a defender under the cursor. */
	void OnPlaceDefenderClicked();

	/** R-key handler: restart the game (only has an effect once the game is over). */
	void OnRestartPressed();

private:
	/** True if the terrain's stored slot state says a defender already occupies this world slot. */
	bool IsSlotOccupied(const FVector& SlotLocation) const;

	/** Finds the terrain's build slot nearest the cursor, within SlotClickTolerance. Shared
	 *  by the click handler and the every-frame hover highlight so they never disagree. */
	bool FindNearestSlotUnderCursor(FVector& OutSlotLocation) const;

	/** Draws a translucent green (valid) or red (occupied / unaffordable) patch over the
	 *  build pad nearest the cursor, each frame, so placement validity is always clear. */
	void UpdateBuildPadHighlight() const;
};
