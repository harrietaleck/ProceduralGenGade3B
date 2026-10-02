// Enemy.h
// Base enemy class for the tower defence game.
// Supports Basic, Bear and Wolf enemy variants, each with its own behaviour:
//   Basic - walks the path and fights whatever defender or tower is in reach.
//   Wolf  - pack hunter: leaves the path to chase nearby defenders, then rejoins it.
//   Bear  - juggernaut: never stops for defenders, slams everything around it while
//           marching on the tower, and enrages when badly hurt.
// On top of the type, the wave director can make an enemy elite (Shielded, Regenerating, Swift,
// Splitting) and let it reroute over the path network around heavily defended cells.
// Poison and stun statuses drive the defender combos (Shatter, Venom spread).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy.generated.h"

class UHealthComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ADefender;

// The flavour of loot an enemy drops.
UENUM(BlueprintType)
enum class EResourceType : uint8
{
    ArcaneOrb UMETA(DisplayName = "Arcane Orb"),
    ToxicMucus UMETA(DisplayName = "Toxic Mucus")
};

// Enemy variants.
UENUM(BlueprintType)
enum class EEnemyType : uint8
{
    Basic UMETA(DisplayName = "Basic"),
    Bear UMETA(DisplayName = "Bear"),
    Wolf UMETA(DisplayName = "Wolf")
};

// Elite modifiers the wave director can stamp onto an enemy to counter the player's defence.
UENUM(BlueprintType)
enum class EEliteModifier : uint8
{
    None UMETA(DisplayName = "None"),
    Shielded UMETA(DisplayName = "Shielded"),         // Blocks the first few direct hits.
    Regenerating UMETA(DisplayName = "Regenerating"), // Heals over time unless poisoned.
    Swift UMETA(DisplayName = "Swift"),               // Much faster, outruns slow area attacks.
    Splitting UMETA(DisplayName = "Splitting")        // Splits into small Basics on death.
};

class AEnemy;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemySplit, AEnemy*, Parent, AEnemy*, Child);

UCLASS()
class PROCEDURALGENGADE3B_API AEnemy : public AActor
{
    GENERATED_BODY()

public:

    AEnemy();

    // Movement speed along the path.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float MoveSpeed = 150.0f;

    // How quickly the enemy reaches its movement speed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float Acceleration = 68.0f;

    // How quickly the enemy turns.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float TurnRate = 2.6f;

    // Damage dealt to defenders or the tower.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackDamage = 10.0f;

    // Time between attacks.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackInterval = 1.0f;

    // Attack range.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float AttackRange = 280.0f;

    // Detection range.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float DetectionRadius = 450.0f;

    // Reward for defeating this enemy.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    int32 ResourceReward = 25;

    // Resource type.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    EResourceType ResourceType = EResourceType::ArcaneOrb;

    // Height above the terrain.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    float GroundClearance = 50.0f;

    // Enemy variant.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    EEnemyType EnemyType = EEnemyType::Basic;

    // Body colour, set per enemy type so each variant reads differently on the battlefield.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Visual")
    FLinearColor BodyColor = FLinearColor(0.85f, 0.15f, 0.15f);

    // Wolf: how far from itself a wolf will notice a defender and leave the path to hunt it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Wolf")
    float HuntRadius = 750.0f;

    // Wolf: if its prey gets further than this, the wolf gives up and returns to the path.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Wolf")
    float HuntLeashRadius = 1100.0f;

    // Bear: seconds between ground slams.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bear")
    float SlamInterval = 2.5f;

    // Bear: radius of the ground slam that hits every defender around it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bear")
    float SlamRadius = 380.0f;

    // Bear: fraction of AttackDamage dealt to each defender caught in a slam.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bear")
    float SlamDamageScale = 0.6f;

    // Bear: health fraction at which the bear enrages.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bear")
    float EnrageHealthFraction = 0.4f;

    // Bear: movement speed multiplier while enraged.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bear")
    float EnrageSpeedMultiplier = 1.6f;

    // Elite: health multiplier applied on top of the wave's scaling.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Elite")
    float EliteHealthMultiplier = 1.5f;

    // Elite (Shielded): direct hits blocked before the shield breaks.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Elite")
    int32 ShieldHits = 3;

    // Elite (Regenerating): fraction of max health healed per second while not poisoned.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Elite")
    float RegenFractionPerSecond = 0.035f;

    // Elite (Swift): movement speed multiplier.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Elite")
    float SwiftSpeedMultiplier = 1.4f;

    // Elite (Splitting): number of small Basics released on death.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Elite")
    int32 SplitCount = 2;

    // Elite (Splitting): each child's max health as a fraction of the parent's.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Elite")
    float SplitHealthFraction = 0.35f;

    // Combo "Shatter": area damage multiplier against a stunned enemy.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combos")
    float ShatterMultiplier = 1.5f;

    // Combo "Venom spread": radius poison jumps to when an arrow hits a poisoned enemy.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combos")
    float VenomSpreadRadius = 320.0f;

    // Combo "Venom spread": poison damage per second applied to each enemy it reaches.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combos")
    float VenomDamagePerSecond = 6.0f;

    // Combo "Venom spread": how long the spread poison lasts.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combos")
    float VenomDuration = 2.5f;

    // Rerouting: whether this enemy looks for a less defended route (set by the wave manager).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Routing")
    bool bCanReroute = false;

    // Rerouting: seconds between route evaluations.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Routing")
    float RerouteCheckInterval = 1.5f;

    // Rerouting: switch only if the new route costs at most this fraction of the current one.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Routing")
    float RerouteImprovementRatio = 0.7f;

    // Rerouting: route cost added per point of defender damage-per-second covering a cell.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Routing")
    float DangerCostWeight = 0.15f;

    // Rerouting: limit so an enemy cannot flip between routes forever.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Routing")
    int32 MaxReroutes = 2;

    // Fired when a Splitting elite releases a child, so the wave manager can track it.
    UPROPERTY(BlueprintAssignable, Category = "Enemy|Elite")
    FOnEnemySplit OnSplit;

    // Spawn effect duration.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn Effect")
    float SpawnEffectDuration = 0.85f;

    // Spawn glow intensity.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn Effect")
    float SpawnGlowIntensity = 6500.0f;

    // Spawn glow colour.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Spawn Effect")
    FLinearColor SpawnGlowColor =
        FLinearColor(0.15f, 0.9f, 1.0f);

    // Health component.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    TObjectPtr<UHealthComponent> HealthComponent;

    // Set the enemy type before BeginPlay.
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void SetEnemyType(EEnemyType InEnemyType);

    // Give the enemy its path.
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void SetPath(
        const TArray<FVector>& InWaypoints);

    // Give the enemy its tower target.
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void SetTargetTower(AActor* InTower)
    {
        TargetTower = InTower;
    }

    // True once the enemy has walked its whole path and is standing at the tower.
    UFUNCTION(BlueprintPure, Category = "Enemy")
    bool HasReachedTower() const
    {
        return Waypoints.Num() > 0 && CurrentWaypoint >= Waypoints.Num();
    }

    // True while a bear is enraged.
    UFUNCTION(BlueprintPure, Category = "Enemy|Bear")
    bool IsEnraged() const { return bEnraged; }

    // Freeze movement and attacks for Duration seconds (bears shrug off half of it).
    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void ApplyStun(float Duration);

    UFUNCTION(BlueprintPure, Category = "Enemy")
    bool IsStunned() const { return StunTimeRemaining > 0.0f; }

    // Display name for the HUD and logs.
    static FString GetTypeName(EEnemyType Type);

    static FString GetEliteName(EEliteModifier Modifier);

    // Turn this enemy into an elite (call after spawning and wave scaling).
    UFUNCTION(BlueprintCallable, Category = "Enemy|Elite")
    void MakeElite(EEliteModifier Modifier);

    UFUNCTION(BlueprintPure, Category = "Enemy|Elite")
    EEliteModifier GetEliteModifier() const { return EliteModifier; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Elite")
    bool IsElite() const { return EliteModifier != EEliteModifier::None; }

    UFUNCTION(BlueprintPure, Category = "Enemy|Elite")
    int32 GetShieldHitsRemaining() const { return ShieldHitsRemaining; }

    // Poison this enemy. DamagePerSecond may be 0 to only mark it (bomb clouds deal their own damage).
    UFUNCTION(BlueprintCallable, Category = "Enemy|Combos")
    void ApplyPoison(float DamagePerSecond, float Duration, AActor* Source);

    UFUNCTION(BlueprintPure, Category = "Enemy|Combos")
    bool IsPoisoned() const { return PoisonTimeRemaining > 0.0f; }

    // Called by the health component for every hit: shields, Shatter and Venom spread.
    float ModifyIncomingDamage(float Amount, AActor* Source);

    UFUNCTION(BlueprintPure, Category = "Enemy|Routing")
    int32 GetRerouteCount() const { return RerouteCount; }

protected:

    virtual void BeginPlay() override;

    virtual void Tick(float DeltaSeconds) override;

    // Called when health reaches zero.
    UFUNCTION()
    void HandleDeath(AActor* Killer);

private:

    // Visual mesh.
    UPROPERTY(VisibleAnywhere, Category = "Enemy")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    // Spawn glow.
    UPROPERTY(VisibleAnywhere, Category = "Enemy|Spawn Effect")
    TObjectPtr<UPointLightComponent> SpawnGlow;

    // Per-enemy material so each type can have its own colour.
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

    // Path waypoints.
    UPROPERTY()
    TArray<FVector> Waypoints;

    // Current waypoint.
    int32 CurrentWaypoint = 0;

    // Tower target.
    UPROPERTY()
    TObjectPtr<AActor> TargetTower;

    // Wolf: the defender currently being hunted.
    TWeakObjectPtr<ADefender> HuntTarget;

    // Attack cooldown.
    float AttackTimer = 0.0f;

    // Bear: slam cooldown.
    float SlamTimer = 0.0f;

    // Bear: true once enraged (one-way).
    bool bEnraged = false;

    // Seconds of stun left.
    float StunTimeRemaining = 0.0f;

    EEliteModifier EliteModifier = EEliteModifier::None;
    int32 ShieldHitsRemaining = 0;

    // Poison status (bomb clouds mark it, venom spread also deals damage over time).
    float PoisonTimeRemaining = 0.0f;
    float PoisonDamagePerSecond = 0.0f;
    float PoisonDamageAccumulated = 0.0f;
    float PoisonTickTimer = 0.0f;
    TWeakObjectPtr<AActor> PoisonSource;

    // True while this enemy applies its own poison tick, so ticks skip shields and combos.
    bool bApplyingStatusDamage = false;

    float RerouteTimer = 0.0f;
    int32 RerouteCount = 0;

    // Current movement speed.
    float CurrentSpeed = 0.0f;

    // Spawn effect timer.
    float SpawnEffectElapsed = 0.0f;

    // Final scale after spawn effect.
    FVector SpawnTargetScale = FVector::OneVector;

    // Apply the correct stats and mesh for the enemy type.
    void ApplyEnemyType();

    // Push BodyColor onto the mesh material.
    void ApplyBodyColor();

    // Spawn animation.
    void UpdateSpawnEffect(float DeltaSeconds);

    // Per-type behaviour, called from Tick.
    void TickBasic(float DeltaSeconds);
    void TickWolf(float DeltaSeconds);
    void TickBear(float DeltaSeconds);

    // Movement.
    void MoveAlongPath(float DeltaSeconds);

    // Steer towards a world location on the ground plane. Returns the remaining distance.
    float MoveTowardsLocation(const FVector& Destination, float DeltaSeconds);

    // Turn to face a target (used while attacking).
    void FaceTowards(const FVector& Location, float DeltaSeconds);

    // After leaving the path (wolf hunt), continue from the closest remaining waypoint.
    void RejoinPath();

    // Attack.
    void TryAttack(
        AActor* Target,
        float DeltaSeconds);

    // Closest living defender within Radius, or null.
    ADefender* FindClosestDefender(float Radius) const;

    // Basic: closest defender in attack range, otherwise the tower if in range.
    AActor* FindTargetInRange() const;

    // Bear: damage every defender within SlamRadius.
    void PerformGroundSlam();

    // Poison damage over time, regeneration and the elite/shield visuals.
    void TickStatusEffects(float DeltaSeconds);

    // Combo "Venom spread": poison every enemy around this one.
    void SpreadVenom(AActor* Source);

    // Splitting elite: release small Basics that carry on along this enemy's route.
    void SpawnSplitChildren();

    // Shrink and weaken a freshly spawned split child.
    void ConfigureAsSplitChild(float ParentMaxHealth);

    // Look for a cheaper (less defended) route to the tower over the path network.
    void TryReroute();

    // Route cost from Start along Points, using the same cell cost as the pathfinder.
    float EvaluateRouteCost(const FVector& Start, const TArray<FVector>& Points,
        TFunctionRef<float(const FVector&)> CellCost, float CellSize) const;

    // Floating combat text above the enemy.
    void ShowCombatText(const FString& Text, const FColor& Color) const;
};
