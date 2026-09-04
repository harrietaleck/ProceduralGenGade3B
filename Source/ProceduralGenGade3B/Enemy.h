// Enemy.h
// The Basic Enemy: walks the terrain's path waypoints toward the tower. Along the way it
// stops to attack any defender that blocks it; once the defender falls it resumes the path.
// On reaching the tower it attacks the tower until either dies. It NEVER targets the player
// character — only the tower and defenders are valid targets. Movement is simple waypoint-
// following (no navmesh), which is robust on our runtime-generated mesh.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy.generated.h"

class UHealthComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * The flavour of loot an enemy drops. Every type still converts into the single shared
 * Essence currency — the enum only records *what* it looked like, which lets the UI/VFX
 * differ per enemy and lets future systems (e.g. crafting) treat drops distinctly without
 * changing the economy. New enemies just pick a different value.
 */
UENUM(BlueprintType)
enum class EResourceType : uint8
{
	ArcaneOrb   UMETA(DisplayName = "Arcane Orb"),
	ToxicMucus  UMETA(DisplayName = "Toxic Mucus")
};

UCLASS()
class PROCEDURALGENGADE3B_API AEnemy : public AActor
{
	GENERATED_BODY()

public:
	AEnemy();

	/** Movement speed along the path, in Unreal units per second. Tuned for the 200uu cell
	 *  grid so a full border-to-tower walk takes ~25–35s — deliberate but not crawling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float MoveSpeed = 150.0f;

	/** How quickly the enemy reaches full MoveSpeed from a standstill (uu/s²). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "1.0"))
	float Acceleration = 68.0f;

	/** How quickly the mesh yaws to face the direction of travel (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.1"))
	float TurnRate = 2.6f;

	/** Damage dealt per attack to the tower or a defender (Basic Enemy spec: 10). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	/** Seconds between attacks — the attack cooldown (Basic Enemy spec: 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.1"))
	float AttackInterval = 1.0f;

	/** Strict range that actually gates stopping + dealing damage. Slightly wider so enemies
	 *  reliably stop beside build pads that sit one cell off the path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float AttackRange = 280.0f;

	/** How far the enemy "notices" a defender worth considering as a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float DetectionRadius = 450.0f;

	/** Gold granted to the player when this enemy is killed (Basic Enemy spec: 25). Flows
	 *  into the game's shared Loot pool via TDGameMode::AddResources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0"))
	int32 ResourceReward = 25;

	/** Which flavour of drop this enemy leaves (cosmetic/future-facing; still becomes Essence). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	EResourceType ResourceType = EResourceType::ArcaneOrb;

	/** Height the enemy floats above the (flat) path plane so it doesn't sink into the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0.0"))
	float GroundClearance = 50.0f;

	/** Duration of the glow-and-scale portal arrival effect. Set to zero to disable it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn Effect", meta = (ClampMin = "0.0"))
	float SpawnEffectDuration = 0.85f;

	/** Light intensity at the start of the portal arrival effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn Effect", meta = (ClampMin = "0.0"))
	float SpawnGlowIntensity = 6500.0f;

	/** Colour of the portal glow around a newly spawned enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn Effect")
	FLinearColor SpawnGlowColor = FLinearColor(0.15f, 0.9f, 1.0f);

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

	/** Brief cyan light that fades as this enemy emerges from its portal. */
	UPROPERTY(VisibleAnywhere, Category = "Enemy|Spawn Effect", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> SpawnGlow;

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

	/** Live travel speed — ramps up from zero so enemies ease into motion after spawning. */
	float CurrentSpeed = 0.0f;

	float SpawnEffectElapsed = 0.0f;
	FVector SpawnTargetScale = FVector::OneVector;

	// --- helpers ---
	void UpdateSpawnEffect(float DeltaSeconds);
	void MoveAlongPath(float DeltaSeconds);
	/** Attack a target on cooldown by applying damage to its HealthComponent. */
	void TryAttack(AActor* Target, float DeltaSeconds);
	/** Return the nearest damageable target (defender or tower) inside AttackRange, or null. */
	AActor* FindTargetInRange() const;
	/** True if nothing (terrain, other geometry) blocks a straight line to Target — enforces
	 *  "never attack through walls." */
	bool HasLineOfSightTo(const AActor* Target) const;
};
