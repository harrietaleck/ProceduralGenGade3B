// HealthComponent.h
// Gives any actor health, damage and a death event. The tower, enemies and defenders
// all share it so the damage code is only written once.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

// Fires whenever health goes up or down. Sends the current and max health.
// It is dynamic so both C++ and Blueprint widgets can bind to it.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnHealthChanged,
    float,
    CurrentHealth,
    float,
    MaxHealth);

// Fires once when health hits zero. Sends the actor that got the kill, which can be null.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnDeath,
    AActor*,
    Killer);

UCLASS(ClassGroup = (TowerDefense), meta = (BlueprintSpawnableComponent))
class PROCEDURALGENGADE3B_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UHealthComponent();

    /** Starting and maximum hit points. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    /** Sent on any health change. The UI health bars listen to this. */
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthChanged OnHealthChanged;

    /** Sent once when this actor dies. */
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnDeath OnDeath;

    // Apply damage to the actor
    UFUNCTION(BlueprintCallable, Category = "Health")
    void ApplyDamage(float Amount, AActor* Killer = nullptr);

    /** Gives back health, up to MaxHealth. Does nothing if dead. */
    UFUNCTION(BlueprintCallable, Category = "Health")
    void Heal(float Amount);

    /** Current hit points. */
    UFUNCTION(BlueprintPure, Category = "Health")
    float GetCurrentHealth() const
    {
        return CurrentHealth;
    }

    /** Health as a value from 0 to 1. Useful for progress bars. */
    UFUNCTION(BlueprintPure, Category = "Health")
    float GetHealthPercent() const
    {
        return MaxHealth > 0.0f
            ? CurrentHealth / MaxHealth
            : 0.0f;
    }

    /** True once this actor has died. */
    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsDead() const
    {
        return bIsDead;
    }

protected:

    virtual void BeginPlay() override;

private:

    /** Current hit points. Set to MaxHealth in BeginPlay. */
    UPROPERTY(VisibleAnywhere, Category = "Health", meta = (AllowPrivateAccess = "true"))
    float CurrentHealth = 0.0f;

    /** Stops the death event from being sent more than once. */
    bool bIsDead = false;
};