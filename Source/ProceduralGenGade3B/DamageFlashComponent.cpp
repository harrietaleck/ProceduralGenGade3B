#include "DamageFlashComponent.h"
#include "HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

// Constructor: sets the default components and values for DamageFlashComponent.
UDamageFlashComponent::UDamageFlashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Runs when the object enters the world: sets up its state and event bindings.
void UDamageFlashComponent::BeginPlay()
{
	Super::BeginPlay();

	Mesh = GetOwner() ? GetOwner()->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (UStaticMeshComponent* MeshComponent = Mesh.Get())
	{
		BaseScale = MeshComponent->GetRelativeScale3D();
	}

	if (UHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr)
	{
		HealthComponent = Health;
		LastHealth = Health->GetCurrentHealth();
		Health->OnHealthChanged.AddDynamic(this, &UDamageFlashComponent::OnHealthChanged);
	}
}

// Runs when the object leaves the world: removes bindings and cleans up.
void UDamageFlashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHealthComponent* Health = HealthComponent.Get())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &UDamageFlashComponent::OnHealthChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlashTimerHandle);
	}

	HealthComponent.Reset();
	Mesh.Reset();
	Super::EndPlay(EndPlayReason);
}

// On health changed.
void UDamageFlashComponent::OnHealthChanged(float CurrentHealth, float MaxHealth)
{
	if (LastHealth >= 0.0f && CurrentHealth < LastHealth - KINDA_SMALL_NUMBER)
	{
		PlayDamageFlash();
	}
	LastHealth = CurrentHealth;
}

// Play damage flash.
void UDamageFlashComponent::PlayDamageFlash()
{
	if (UStaticMeshComponent* MeshComponent = Mesh.Get())
	{
		MeshComponent->SetRelativeScale3D(BaseScale * 1.18f);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlashTimerHandle);
			World->GetTimerManager().SetTimer(
				FlashTimerHandle,
				this,
				&UDamageFlashComponent::ResetDamageFlash,
				0.12f,
				false);
		}
	}

	if (AActor* Owner = GetOwner())
	{
		DrawDebugSphere(GetWorld(), Owner->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f),
			45.0f, 12, FColor::Red, false, 0.15f, 0, 2.0f);
	}
}

// Reset damage flash.
void UDamageFlashComponent::ResetDamageFlash()
{
	if (UStaticMeshComponent* MeshComponent = Mesh.Get())
	{
		MeshComponent->SetRelativeScale3D(BaseScale);
	}
}
