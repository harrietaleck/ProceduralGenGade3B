// Projectile.h
// A simple projectile fired by the tower and the defenders. It flies to its target and does damage
// when it gets there. Keeping it as its own actor means new weapons only need a new projectile class.

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

	/** How fast the projectile flies towards its target, in Unreal units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "1.0"))
	float Speed = 2000.0f;

	/** Follows the target while it is alive. If the target dies on the way, it flies to the last spot it saw it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bHoming = true;

	/** Destroys itself after this many seconds so stray projectiles don't pile up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.1"))
	float MaxLifeSeconds = 5.0f;

	/** Size of the projectile mesh. Slightly bigger balls are easier to see. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.05"))
	float VisualScale = 0.28f;

	/** Colour of the projectile ball. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FLinearColor ProjectileColor = FLinearColor(1.0f, 0.82f, 0.2f);

	/**
	 * Sets up the projectile with its target, its damage and who fired it, so the kill is credited.
	 * The shooter calls this straight after spawning it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitProjectile(AActor* InTarget, float InDamage, AActor* InInstigatorActor);

	/** Lets each shooter change the size and colour, so tower balls can look bigger than defender ones. */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void ConfigureVisuals(float InVisualScale, FLinearColor InColor);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** The visible mesh and root. It is a small sphere by default. */
	UPROPERTY(VisibleAnywhere, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** The actor we are flying towards. It might die and be destroyed before we reach it. */
	UPROPERTY()
	TObjectPtr<AActor> Target;

	/** Last place we saw the target, used if it disappears while we are flying. */
	FVector CachedTargetLocation = FVector::ZeroVector;

	/** Damage done when it hits. */
	float Damage = 0.0f;

	/** The tower or defender that fired us. It gets the credit for the kill. */
	UPROPERTY()
	TObjectPtr<AActor> InstigatorActor;

	/** How close we need to get to count as a hit and do damage. */
	static constexpr float HitRadius = 60.0f;

	/** Damages the target if it is still there, then destroys the projectile. */
	void HitTargetAndDie();

	void ApplyVisuals();

	/** One dynamic material per projectile that we keep reusing, so ConfigureVisuals doesn't stack new materials on top of each other. */
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> CachedDynMat;
};
