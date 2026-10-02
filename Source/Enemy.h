// Enemy.h
// Base enemy class for the tower defence game.
// Supports Basic, Bear and Wolf enemy variants.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy.generated.h"

class UHealthComponent;
class UPointLightComponent;
class UStaticMeshComponent;

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

    // Path waypoints.
    UPROPERTY()
    TArray<FVector> Waypoints;

    // Current waypoint.
    int32 CurrentWaypoint = 0;

    // Tower target.
    UPROPERTY()
    TObjectPtr<AActor> TargetTower;

    // Attack cooldown.
    float AttackTimer = 0.0f;

    // Current movement speed.
    float CurrentSpeed = 0.0f;

    // Spawn effect timer.
    float SpawnEffectElapsed = 0.0f;

    // Final scale after spawn effect.
    FVector SpawnTargetScale = FVector::OneVector;

    // Apply the correct stats and mesh for the enemy type.
    void ApplyEnemyType();

    // Spawn animation.
    void UpdateSpawnEffect(float DeltaSeconds);

    // Movement.
    void MoveAlongPath(float DeltaSeconds);

    // Attack.
    void TryAttack(
        AActor* Target,
        float DeltaSeconds);

    // Find a valid target.
    AActor* FindTargetInRange() const;

    // Check line of sight.
    bool HasLineOfSightTo(
        const AActor* Target) const;
};