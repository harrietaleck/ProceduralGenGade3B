// UpgradeVisualComponent.h
// Shows which upgrade tier a defender or the tower is on, so the player can see it at a glance.
//   Tier 0: no change.
//   Tier I: a glowing gold orb floats above the unit and its body colour shifts a little towards gold.
//   Tier II: a bigger orb, a glowing ring on the ground around the unit and a stronger gold body colour.
// It never scales the mesh, because UDamageFlashComponent already scales the mesh on every hit and
// resets it afterwards, so a scale change here would be wiped out by the next hit.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UpgradeVisualComponent.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

UCLASS(ClassGroup = (TowerDefense), meta = (BlueprintSpawnableComponent))
class PROCEDURALGENGADE3B_API UUpgradeVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUpgradeVisualComponent();

	/** Shows the look for a tier. 0 removes every upgrade visual and restores the original body colour. */
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void SetTier(int32 NewTier);

	UFUNCTION(BlueprintPure, Category = "Upgrade")
	int32 GetTier() const { return Tier; }

	/** Material of the floating orb. */
	UPROPERTY(EditAnywhere, Category = "Upgrade|Visual")
	TSoftObjectPtr<UMaterialInterface> OrbMaterial;

	/** Material of the ground ring shown from tier II. */
	UPROPERTY(EditAnywhere, Category = "Upgrade|Visual")
	TSoftObjectPtr<UMaterialInterface> RingMaterial;

	/** Colour the body is blended towards. */
	UPROPERTY(EditAnywhere, Category = "Upgrade|Visual")
	FLinearColor TierTint = FLinearColor(1.0f, 0.78f, 0.2f);

	/** How far the body colour moves towards TierTint for each tier (0 = not at all, 1 = fully). */
	UPROPERTY(EditAnywhere, Category = "Upgrade|Visual", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float TintPerTier = 0.35f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	int32 Tier = 0;

	/** Body colour before any tint was applied, remembered so tier 0 can restore it exactly. */
	FLinearColor BaseBodyColor = FLinearColor::White;
	bool bHasBaseBodyColor = false;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> OrbMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RingMesh;

	/** The owner's main mesh (its root), used to measure where the top and bottom of the unit are. */
	UStaticMeshComponent* GetBodyMesh() const;

	/** Creates the orb and ring the first time a tier above 0 is shown. */
	void EnsureMeshes(UStaticMeshComponent* Body);

	/** Moves, sizes and shows or hides the orb and ring for the current tier. */
	void Layout(UStaticMeshComponent* Body);

	/** Blends the body material's Color parameter towards TierTint (only if it is a dynamic instance). */
	void ApplyBodyTint(UStaticMeshComponent* Body);
};
