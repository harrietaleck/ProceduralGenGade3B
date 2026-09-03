// Defender.h
// A player-placed defensive unit. Like a small tower, it automatically fires at the nearest
// enemy in range. It has health and can be destroyed by enemies. It carries a resource Cost
// that the player controller checks before placing one.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDMatchRewards.h"
#include "Defender.generated.h"

class UHealthComponent;
class UStaticMeshComponent;
class AEnemy;
class AProjectile;

UCLASS()
class PROCEDURALGENGADE3B_API ADefender : public AActor
{
	GENERATED_BODY()

public:
	ADefender();

	/** Resource upkeep charged per wave for each living defender. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0"))
	int32 UpkeepPerWave = 8;

	/** Resource cost to place this defender. Read by the player controller before spending. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0"))
	int32 Cost = 50;

	/** Persistent meta-currency spent when placing this defender type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender|Meta")
	FMetaCurrencyRewards MetaCost;

	/** True for gem-priced elite defenders (used by the HUD hint text). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender|Meta")
	bool bStrongDefender = false;

	/** How far the defender can hit enemies (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.0"))
	float AttackRange = 600.0f;

	/** Damage dealt to an enemy per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.0"))
	float AttackDamage = 5.0f;

	/** Seconds between shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.05"))
	float FireInterval = 0.7f;

	/** Projectile fired at enemies. Defaults to orange ball shots in the constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
	TSubclassOf<AProjectile> ProjectileClass;

	/** Scale and colour for defender shot balls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.05"))
	float DefenderBallScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
	FLinearColor DefenderBallColor = FLinearColor(1.0f, 0.55f, 0.1f);

	/** Local-space offset from the defender origin where shots originate (the muzzle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
	FVector MuzzleOffset = FVector(0.0f, 0.0f, 80.0f);

	/** Shared health/damage/death component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Defender")
	TObjectPtr<UHealthComponent> HealthComponent;

	/** Records which terrain build slot this defender was placed on, so it can be freed again
	 *  when this defender is destroyed. Called once by the placement flow right after spawning. */
	UFUNCTION(BlueprintCallable, Category = "Defender")
	void SetOccupiedSlot(const FVector& SlotLocation) { OccupiedSlotLocation = SlotLocation; bHasOccupiedSlot = true; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Timer callback: pick the nearest in-range enemy and shoot it. */
	void FireAtNearestEnemy();

	/** Called when the defender's health reaches zero: remove it (frees its build slot). */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

private:
	/** Simple cube visual + root. */
protected:
	UPROPERTY(VisibleAnywhere, Category = "Defender", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

private:

	FTimerHandle FireTimerHandle;

	/** The terrain build slot this defender occupies, set via SetOccupiedSlot at placement. */
	FVector OccupiedSlotLocation = FVector::ZeroVector;

	/** True once SetOccupiedSlot has been called, so EndPlay knows there's a slot to free. */
	bool bHasOccupiedSlot = false;

	/** Returns the closest living enemy within AttackRange, or null if none. */
	AEnemy* FindNearestEnemyInRange() const;
};
