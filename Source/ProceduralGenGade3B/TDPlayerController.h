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

	/** A slot counts as occupied if a defender stands within this radius of it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TowerDefense", meta = (ClampMin = "1.0"))
	float SlotOccupiedRadius = 120.0f;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Left-click handler: attempt to place a defender under the cursor. */
	void OnPlaceDefenderClicked();

private:
	/** True if a living defender already occupies the given world slot. */
	bool IsSlotOccupied(const FVector& SlotLocation) const;
};
