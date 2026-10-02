#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DamageFlashComponent.generated.h"

class UHealthComponent;
class UStaticMeshComponent;

/** Shows when a unit takes damage. It listens to the HealthComponent and quickly
 *  scales up the owner's mesh, so hits are easy to see even with basic materials. */
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
