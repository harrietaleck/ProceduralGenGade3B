// PoisonCloud.h
// A patch of poison that spreads like a cellular automaton. The slime enemy releases it when it is near a
// defender and when it dies. Each cell on a small grid is either clean or infected. Every step an infected
// cell ages and burns out, and a clean cell next to young infected cells may catch the poison. Defenders
// standing in an infected cell take heavy damage, so poison released near a defender destroys it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoisonCloud.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class PROCEDURALGENGADE3B_API APoisonCloud : public AActor
{
	GENERATED_BODY()

public:
	APoisonCloud();

	/** Damage each infected cell deals to a defender standing in it, once per step. */
	UPROPERTY(EditAnywhere, Category = "Poison")
	float DamagePerStep = 32.0f;

	/** Seconds between automaton steps. */
	UPROPERTY(EditAnywhere, Category = "Poison", meta = (ClampMin = "0.1"))
	float StepInterval = 0.45f;

	/** Chance per young infected neighbour that a clean cell catches the poison each step. */
	UPROPERTY(EditAnywhere, Category = "Poison", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpreadChance = 0.4f;

	/** How many steps a cell stays infected before it burns out. */
	UPROPERTY(EditAnywhere, Category = "Poison", meta = (ClampMin = "1"))
	int32 CellLifetime = 5;

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 GridSize = 7;

	float CellSize = 130.0f;
	int32 StepCount = 0;

	/** 0 = clean, 1..CellLifetime = infected, the number is the cell's age. */
	TArray<uint8> Age;
	TArray<TObjectPtr<UStaticMeshComponent>> Tiles;
	TObjectPtr<UMaterialInstanceDynamic> YoungMaterial;
	TObjectPtr<UMaterialInstanceDynamic> OldMaterial;
	FRandomStream Random;
	FTimerHandle StepHandle;

	int32 Index(int32 X, int32 Y) const { return Y * GridSize + X; }
	FVector CellWorldLocation(int32 X, int32 Y) const;
	void Step();
	void RefreshVisuals();
	void DamageDefenders();
};
