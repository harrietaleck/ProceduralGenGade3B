// TowerDustMote.h
// A tiny drifting puff element used for the tower's dust cloud on destruction.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TowerDustMote.generated.h"

class UStaticMeshComponent;

UCLASS()
class PROCEDURALGENGADE3B_API ATowerDustMote : public AActor
{
	GENERATED_BODY()

public:
	ATowerDustMote();

	void InitMote(const FVector& Velocity, float Lifetime);

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Dust")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	FVector Velocity = FVector::ZeroVector;
	float RemainingLife = 1.5f;
};
