#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DamageFlashComponent.generated.h"

class UHealthComponent;
class UStaticMeshComponent;

/** Briefs require clear feedback when units take damage. Listens to HealthComponent and
 *  briefly scales the owner's mesh so hits are visible even with placeholder materials. */
UCLASS(ClassGroup = (TowerDefense), meta = (BlueprintSpawnableComponent))
class PROCEDURALGENGADE3B_API UDamageFlashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageFlashComponent();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnHealthChanged(float CurrentHealth, float MaxHealth);

	void PlayDamageFlash();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	FVector BaseScale = FVector::OneVector;
	float LastHealth = -1.0f;
	FTimerHandle FlashTimerHandle;
};
