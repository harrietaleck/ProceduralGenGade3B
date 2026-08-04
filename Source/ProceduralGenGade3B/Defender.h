// Defender.h
// A player-placed defensive unit. Like a small tower, it automatically fires at the nearest
// enemy in range. It has health and can be destroyed by enemies. It carries a resource Cost
// that the player controller checks before placing one.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	/** Resource cost to place this defender. Read by the player controller before spending. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0"))
	int32 Cost = 50;

	/** How far the defender can hit enemies (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.0"))
	float AttackRange = 600.0f;

	/** Damage dealt to an enemy per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.0"))
	float AttackDamage = 5.0f;

	/** Seconds between shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.05"))
	float FireInterval = 0.7f;

	/** Projectile fired at enemies. If left empty, the defender falls back to instant (hitscan) damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
	TSubclassOf<AProjectile> ProjectileClass;

	/** Local-space offset from the defender origin where shots originate (the muzzle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
	FVector MuzzleOffset = FVector(0.0f, 0.0f, 80.0f);

	/** Shared health/damage/death component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Defender")
	TObjectPtr<UHealthComponent> HealthComponent;

protected:
	virtual void BeginPlay() override;

	/** Timer callback: pick the nearest in-range enemy and shoot it. */
	void FireAtNearestEnemy();

	/** Called when the defender's health reaches zero: remove it (frees its build slot). */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

private:
	/** Simple cube visual + root. */
	UPROPERTY(VisibleAnywhere, Category = "Defender", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	FTimerHandle FireTimerHandle;

	/** Returns the closest living enemy within AttackRange, or null if none. */
	AEnemy* FindNearestEnemyInRange() const;
};
