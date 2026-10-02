// HealthComponent.cpp — see HealthComponent.h for the overview.

#include "HealthComponent.h"
#include "Enemy.h"
#include "Defender.h"
#include "TDGameMode.h"

UHealthComponent::UHealthComponent()
{
    // Health logic is event-driven (ApplyDamage / Heal), so no per-frame tick is needed.
    PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    // Start at full health and let any bound UI initialise itself.
    CurrentHealth = MaxHealth;
    bIsDead = false;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::ApplyDamage(float Amount, AActor* Killer)
{
    // Ignore damage once dead, or non-positive amounts (healing goes through Heal()).
    if (bIsDead || Amount <= 0.0f)
    {
        return;
    }

    //Decrease the damage when a defender attacks an enemy
    if (ADefender* DefenderAttacker = Cast<ADefender>(Killer))
    {
        //Check if the object being attacked is an enemy
        if (AEnemy* EnemyOwner = Cast<AEnemy>(GetOwner()))
        {
            //If a bear let it get 50% damage
            if (EnemyOwner->EnemyType == EEnemyType::Bear)
            {
                Amount *= 0.50f;
            }

            //If a wolf let it get 75% damage
            else if (EnemyOwner->EnemyType == EEnemyType::Wolf)
            {
                Amount *= 0.75f;
            }

            //If a basic enemy let it get 100% damage
            else
            {
                Amount *= 1.0f;
            }
        }
    }

    CurrentHealth =
        FMath::Clamp(CurrentHealth - Amount, 0.0f, MaxHealth);

    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

    // Count player combat hits against enemies for the end-screen score.
    if (Cast<AEnemy>(GetOwner()))
    {
        if (UWorld* World = GetWorld())
        {
            if (ATDGameMode* GameMode = World->GetAuthGameMode<ATDGameMode>())
            {
                GameMode->NotifyEnemyHit();
            }
        }
    }

    // Transition to dead exactly once.
    if (CurrentHealth <= 0.0f)
    {
        bIsDead = true;
        OnDeath.Broadcast(Killer);
    }
}

void UHealthComponent::Heal(float Amount)
{
    if (bIsDead || Amount <= 0.0f)
    {
        return;
    }

    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, MaxHealth);

    OnHealthChanged.Broadcast(CurrentHealth,MaxHealth);
}