#include "DamageFlashComponent.h"
#include "HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

UDamageFlashComponent::UDamageFlashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDamageFlashComponent::BeginPlay()
{
	Super::BeginPlay();

	Mesh = GetOwner() ? GetOwner()->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (Mesh)
	{
		BaseScale = Mesh->GetRelativeScale3D();
	}

	if (UHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr)
	{
		LastHealth = Health->GetCurrentHealth();
		Health->OnHealthChanged.AddDynamic(this, &UDamageFlashComponent::OnHealthChanged);
	}
}

void UDamageFlashComponent::OnHealthChanged(float CurrentHealth, float MaxHealth)
{
	if (LastHealth >= 0.0f && CurrentHealth < LastHealth - KINDA_SMALL_NUMBER)
	{
		PlayDamageFlash();
	}
	LastHealth = CurrentHealth;
}

void UDamageFlashComponent::PlayDamageFlash()
{
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(BaseScale * 1.18f);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlashTimerHandle);
			World->GetTimerManager().SetTimer(FlashTimerHandle, [this]()
			{
				if (Mesh)
				{
					Mesh->SetRelativeScale3D(BaseScale);
				}
			}, 0.12f, false);
		}
	}

	if (AActor* Owner = GetOwner())
	{
		DrawDebugSphere(GetWorld(), Owner->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f),
			45.0f, 12, FColor::Red, false, 0.15f, 0, 2.0f);
	}
}
