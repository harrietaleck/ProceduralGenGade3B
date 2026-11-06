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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnHealthChanged(float CurrentHealth, float MaxHealth);

	void PlayDamageFlash();
	void ResetDamageFlash();

	TWeakObjectPtr<UStaticMeshComponent> Mesh;
	TWeakObjectPtr<UHealthComponent> HealthComponent;

	FVector BaseScale = FVector::OneVector;
	float LastHealth = -1.0f;
	FTimerHandle FlashTimerHandle;
};
