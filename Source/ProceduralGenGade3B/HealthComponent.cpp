// HealthComponent.cpp — see HealthComponent.h for the overview.

#include "HealthComponent.h"

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

	CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

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
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
