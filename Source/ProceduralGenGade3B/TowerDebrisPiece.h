// TowerDebrisPiece.h
// One stone chunk that uses physics. These spawn when the main tower is destroyed.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TowerDebrisPiece.generated.h"

class UStaticMeshComponent;

UCLASS()
class PROCEDURALGENGADE3B_API ATowerDebrisPiece : public AActor
{
	GENERATED_BODY()

public:
	ATowerDebrisPiece();

	/** Sets the mesh, the size, and the push away from the tower centre. */
	void InitDebris(UStaticMesh* Mesh, const FVector& Scale, const FVector& Impulse);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Debris")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	FVector PendingImpulse = FVector::ZeroVector;
	bool bImpulseApplied = false;
};
