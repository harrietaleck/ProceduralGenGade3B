// HealthComponent.cpp
// Damage, healing and death for the health component. See the header for more.

#include "HealthComponent.h"
#include "Enemy.h"
#include "Defender.h"
#include "TDGameMode.h"

UHealthComponent::UHealthComponent()
{
    // Health only changes in ApplyDamage and Heal, so it does not need to tick.
    PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    // Start at full health and tell the UI so it can set itself up.
    CurrentHealth = MaxHealth;
    bIsDead = false;
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::ApplyDamage(float Amount, AActor* Killer)
{
    // Ignore damage if already dead or if the amount is zero or less. Healing uses Heal instead.
    if (bIsDead || Amount <= 0.0f)
    {
        return;
    }

    // Lower the damage when a defender attacks an enemy
    if (ADefender* DefenderAttacker = Cast<ADefender>(Killer))
    {
        // Check if the actor being attacked is an enemy
        if (AEnemy* EnemyOwner = Cast<AEnemy>(GetOwner()))
        {
            // Bears only take 50% damage
            if (EnemyOwner->EnemyType == EEnemyType::Bear)
            {
                Amount *= 0.50f;
            }

            // Wolves take 75% damage
            else if (EnemyOwner->EnemyType == EEnemyType::Wolf)
            {
                Amount *= 0.75f;
            }

            // Basic enemies take the full damage
            else
            {
                Amount *= 1.0f;
            }
        }
    }

    // Elite shields and defender combos
    if (AEnemy* EnemyOwner = Cast<AEnemy>(GetOwner()))
    {
        Amount = EnemyOwner->ModifyIncomingDamage(Amount, Killer);
        if (Amount <= 0.0f)
        {
            return;
        }
    }

    CurrentHealth =
        FMath::Clamp(CurrentHealth - Amount, 0.0f, MaxHealth);

    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

    // Count the player's hits on enemies for the score on the end screen.
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

    // Only die once.
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