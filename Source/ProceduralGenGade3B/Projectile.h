// Projectile.h
// A simple travelling projectile fired by the Citadel (tower) and defenders. It flies toward
// a target actor and, on arrival, applies its damage through the target's HealthComponent.
//
// Keeping "the thing that flies" as its own actor (rather than baking instant hitscan damage
// into every shooter) means new weapons — arrows, bolts, alchemist flasks, ballista spears —
// only need a different projectile class and mesh, not new firing code. Shooters that leave
// their ProjectileClass empty simply fall back to instant damage, so nothing old breaks.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Projectile.generated.h"

class UStaticMeshComponent;

UCLASS()
class PROCEDURALGENGADE3B_API AProjectile : public AActor
{
	GENERATED_BODY()

public:
	AProjectile();

	/** How fast the projectile travels toward its target (uu/second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "1.0"))
	float Speed = 2000.0f;

	/** If the target is still alive it homes onto it; if it dies mid-flight we coast to the last spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bHoming = true;

	/** Self-destruct after this many seconds so stray projectiles never leak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.1"))
	float MaxLifeSeconds = 5.0f;

	/** Visual scale applied to the projectile mesh (balls read better slightly larger). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.05"))
	float VisualScale = 0.28f;

	/** Tint colour for the projectile ball. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FLinearColor ProjectileColor = FLinearColor(1.0f, 0.82f, 0.2f);

	/**
	 * Arm the projectile: who to hit, how hard, and who fired it (for kill attribution).
	 * Called by the shooter immediately after spawning.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitProjectile(AActor* InTarget, float InDamage, AActor* InInstigatorActor);

	/** Optional per-shot visual override (tower balls can be larger/brighter than defenders). */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void ConfigureVisuals(float InVisualScale, FLinearColor InColor);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** Visual body + root (a small sphere by default). */
	UPROPERTY(VisibleAnywhere, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** The actor we're flying toward (weak: it may die and be destroyed before we land). */
	UPROPERTY()
	TObjectPtr<AActor> Target;

	/** Last known world position of the target, used if the target disappears mid-flight. */
	FVector CachedTargetLocation = FVector::ZeroVector;

	/** Damage applied on impact. */
	float Damage = 0.0f;

	/** The actor credited with the kill (the tower or defender that fired us). */
	UPROPERTY()
	TObjectPtr<AActor> InstigatorActor;

	/** Distance (uu) at which we count as having "hit" and apply damage. */
	static constexpr float HitRadius = 60.0f;

	/** Apply damage to the target (if still valid) and destroy the projectile. */
	void HitTargetAndDie();

	void ApplyVisuals();
};
