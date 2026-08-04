// Tower.h
// The player's central tower. It automatically fires at the nearest enemy in range on a
// timer, and ends the game (via the game mode) when its health is depleted.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tower.generated.h"

class UHealthComponent;
class UStaticMeshComponent;
class AEnemy;
class AProjectile;

UCLASS()
class PROCEDURALGENGADE3B_API ATower : public AActor
{
	GENERATED_BODY()

public:
	ATower();

	/** How far the tower can hit enemies (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower", meta = (ClampMin = "0.0"))
	float AttackRange = 1000.0f;

	/** Damage dealt to an enemy per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower", meta = (ClampMin = "0.0"))
	float AttackDamage = 5.0f;

	/** Seconds between shots (0.5 = two shots per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower", meta = (ClampMin = "0.05"))
	float FireInterval = 0.5f;

	/** Projectile fired at enemies. If left empty, the tower falls back to instant (hitscan) damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower")
	TSubclassOf<AProjectile> ProjectileClass;

	/** Local-space offset from the tower origin where shots originate (the muzzle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower")
	FVector MuzzleOffset = FVector(0.0f, 0.0f, 300.0f);

	/** Shared health/damage/death component. Game over fires when this dies. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
	TObjectPtr<UHealthComponent> HealthComponent;

protected:
	virtual void BeginPlay() override;

	/** Timer callback: pick the nearest in-range enemy and shoot it. */
	void FireAtNearestEnemy();

	/** Called when the tower's health reaches zero -> triggers game over. */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

private:
	/** Simple cylinder visual + root. */
	UPROPERTY(VisibleAnywhere, Category = "Tower", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Handle for the repeating fire timer. */
	FTimerHandle FireTimerHandle;

	/** Returns the closest living enemy within AttackRange, or null if none. */
	AEnemy* FindNearestEnemyInRange() const;
};
