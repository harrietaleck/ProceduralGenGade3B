// Tower.h
// The player's main tower. It shoots the closest enemy in range on a timer.
// When its health runs out, the game mode ends the game.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tower.generated.h"

class UHealthComponent;
class UStaticMeshComponent;
class AEnemy;
class AProjectile;

UCLASS()
class PROCEDURALGENGADE3B_API ATower : public AActor
{
	GENERATED_BODY()

public:
	ATower();

	/** How far away the tower can hit enemies, in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower", meta = (ClampMin = "0.0"))
	float AttackRange = 1000.0f;

	/** How much damage each shot does to an enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	/** Seconds between shots. 0.5 means two shots per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower", meta = (ClampMin = "0.05"))
	float FireInterval = 0.5f;

	/** The projectile the tower shoots. The constructor sets it to the ball projectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower")
	TSubclassOf<AProjectile> ProjectileClass;

	/** Size and colour of the balls the tower shoots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower", meta = (ClampMin = "0.05"))
	float TowerBallScale = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower")
	FLinearColor TowerBallColor = FLinearColor(1.0f, 0.78f, 0.15f);

	/** How many stone chunks spawn when the tower is destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower|Destruction", meta = (ClampMin = "1"))
	int32 DebrisPieceCount = 18;

	/** How many dust bits spawn in the cloud when the tower falls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower|Destruction", meta = (ClampMin = "0"))
	int32 DustMoteCount = 28;

	/** Where shots start from, as an offset from the tower's origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower")
	FVector MuzzleOffset = FVector(0.0f, 0.0f, 300.0f);

	/** Handles health, damage and death. The game ends when this dies. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
	TObjectPtr<UHealthComponent> HealthComponent;

protected:
	virtual void BeginPlay() override;

	/** Runs on the timer. Finds the closest enemy in range and shoots it. */
	void FireAtNearestEnemy();

	/** Called when the tower's health hits zero. This starts the game over. */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

private:
	/** A simple cylinder mesh. It is also the root component. */
	UPROPERTY(VisibleAnywhere, Category = "Tower", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Handle for the repeating fire timer. */
	FTimerHandle FireTimerHandle;

	/** Gives back the closest living enemy inside AttackRange, or null if there is none. */
	AEnemy* FindNearestEnemyInRange() const;

	/** Spawns falling stones and dust when the tower is destroyed. */
	void PlayDestructionEffect();
};
