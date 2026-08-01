// Enemy.h
// An enemy that walks the terrain's path waypoints toward the tower. Along the way it
// stops to attack any defender within range; on reaching the tower it attacks the tower.
// Movement is simple waypoint-following (no navmesh) which is robust on our runtime mesh.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy.generated.h"

class UHealthComponent;
class UStaticMeshComponent;

UCLASS()
class PROCEDURALGENGADE3B_API AEnemy : public AActor
{
	GENERATED_BODY()

public:
	AEnemy();

	/** Movement speed along the path, in Unreal units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float MoveSpeed = 250.0f;

	/** Damage dealt per attack to the tower or a defender. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	/** Seconds between attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.1"))
	float AttackInterval = 1.0f;

	/** Range within which the enemy can hit the tower or a defender. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float AttackRange = 250.0f;

	/** Resources granted to the player when this enemy is killed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0"))
	int32 ResourceReward = 10;

	/** Height the enemy floats above the (flat) path plane so it doesn't sink into the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float GroundClearance = 50.0f;

	/** The reusable health/damage/death component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UHealthComponent> HealthComponent;

	/** Give this enemy the ordered world-space waypoints to walk (spawn -> tower). */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void SetPath(const TArray<FVector>& InWaypoints);

	/** Tell this enemy which actor is the tower it should ultimately attack. */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void SetTargetTower(AActor* InTower) { TargetTower = InTower; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called when HealthComponent reports death: reward the player, then destroy. */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

private:
	/** Simple sphere visual + root. */
	UPROPERTY(VisibleAnywhere, Category = "Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Ordered world-space path points from the spawn point to the tower. */
	UPROPERTY()
	TArray<FVector> Waypoints;

	/** Index of the waypoint we're currently walking toward. */
	int32 CurrentWaypoint = 0;

	/** The tower actor (stored as AActor*; we damage it via its HealthComponent). */
	UPROPERTY()
	TObjectPtr<AActor> TargetTower;

	/** Counts down between attacks. */
	float AttackTimer = 0.0f;

	// --- helpers ---
	void MoveAlongPath(float DeltaSeconds);
	/** Attack a target on cooldown by applying damage to its HealthComponent. */
	void TryAttack(AActor* Target, float DeltaSeconds);
	/** Return the nearest damageable target (defender or tower) inside AttackRange, or null. */
	AActor* FindTargetInRange() const;
};
