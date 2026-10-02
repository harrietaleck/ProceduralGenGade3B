// HealthComponent.h
// A small reusable component that gives any actor hit points, damage handling and a
// death event. The tower, enemies and defenders all use this so damage logic lives in
// exactly one place (rather than being copy-pasted onto three different actors).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

// Fired whenever health changes (damage or healing). Params: current and max health.
// Dynamic multicast so both C++ and Blueprint/UI widgets can bind to it.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);

// Fired once when health reaches zero. Param: the actor that dealt the killing blow (may be null).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeath, AActor*, Killer);

UCLASS(ClassGroup = (TowerDefense), meta = (BlueprintSpawnableComponent))
class PROCEDURALGENGADE3B_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	/** Starting and maximum hit points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Broadcast on any health change — UI health bars bind to this. */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	/** Broadcast exactly once when this actor dies. */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeath OnDeath;

	/**
	 * Apply damage to this actor. Clamps health to zero, broadcasts OnHealthChanged, and
	 * broadcasts OnDeath the first time health hits zero. Ignored if already dead or amount <= 0.
	 * @param Amount    How many hit points to remove.
	 * @param Killer     The actor responsible (used for scoring / resource rewards). May be null.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Amount, AActor* Killer = nullptr);

	/** Restore hit points, clamped to MaxHealth. Ignored if dead. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	/** Current hit points. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	/** Health as a 0..1 fraction — handy for progress bars. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	/** True once this actor has died. */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

protected:
	virtual void BeginPlay() override;

private:
	/** Live hit points. Initialised to MaxHealth in BeginPlay. */
	UPROPERTY(VisibleAnywhere, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	/** Guards against broadcasting death more than once. */
	bool bIsDead = false;
};
