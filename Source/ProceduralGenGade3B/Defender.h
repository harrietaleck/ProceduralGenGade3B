// Defender.h
// A small tower the player places. It shoots the nearest enemy in range and can be destroyed.
// It has a Cost that the player controller checks before placing it.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDMatchRewards.h"
#include "Defender.generated.h"

class UHealthComponent;
class UStaticMeshComponent;
class AEnemy;
class AProjectile;

UCLASS()
class PROCEDURALGENGADE3B_API ADefender : public AActor
{
    GENERATED_BODY()
public:
    ADefender();

    /** Resources we pay every wave to keep this defender alive. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0"))
    int32 UpkeepPerWave = 8;

    /** How many resources it costs to place this defender. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0"))
    int32 Cost = 50;

    /** Meta currency that is spent when placing this defender. It carries over between runs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender|Meta")
    FMetaCurrencyRewards MetaCost;

    /** True for the strong defenders that cost gems. The HUD uses this for its hint text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender|Meta")
    bool bStrongDefender = false;

    /** How far away the defender can hit enemies, in Unreal units. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.0"))
    float AttackRange = 600.0f;

    /** Damage done to an enemy with each shot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.0"))
    float AttackDamage = 5.0f;

    /** Seconds between shots. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.05"))
    float FireInterval = 0.7f;

    /** Projectile we shoot at enemies. The constructor sets it to orange balls by default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
    TSubclassOf<AProjectile> ProjectileClass;

    /** Size and colour of the balls this defender shoots. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0.05"))
    float DefenderBallScale = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
    FLinearColor DefenderBallColor = FLinearColor(1.0f, 0.55f, 0.1f);

    /** Where the shots start from, measured from the middle of the defender. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
    FVector MuzzleOffset = FVector(0.0f, 0.0f, 80.0f);

    /** Body colour, so each type of defender looks different. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender|Visual")
    FLinearColor BodyColor = FLinearColor(0.95f, 0.55f, 0.12f);

    /** Name shown on the HUD. The wave director also uses it to work out how the player plays. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
    FString DefenderName = TEXT("Basic");

    /** Rough damage per second this defender adds to its lanes. Used by the wave director. */
    virtual float GetThreatRating() const;

    /** True if this defender hits an area instead of a single enemy. */
    virtual bool IsAreaAttacker() const { return false; }

    /** Shared component that handles health, damage and death. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Defender")
    TObjectPtr<UHealthComponent> HealthComponent;

    /**
     * Remembers which build pad this defender is on so the pad can be freed when it dies.
     */
    UFUNCTION(BlueprintCallable, Category = "Defender")
    void SetOccupiedSlot(const FVector& SlotLocation)
    {
        OccupiedSlotLocation = SlotLocation;
        bHasOccupiedSlot = true;
    }

    //Lets archers and bomb defenders turn off the original single target shooting
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
    bool bUseDefaultAttack = true;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Called by the timer. Finds the nearest enemy in range and shoots it. */
    void FireAtNearestEnemy();

    /** Called when the defender's health hits zero. Removes it, which frees its build pad. */
    UFUNCTION()
    void HandleDeath(AActor* Killer);

    /** The visible mesh, also used as the root. */
    // Protected so the Archer and Bomb defenders can swap in their own shapes.
    UPROPERTY(VisibleAnywhere, Category = "Defender")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
    FTimerHandle FireTimerHandle;

    /** The build pad this defender is standing on. Set by SetOccupiedSlot. */
    FVector OccupiedSlotLocation = FVector::ZeroVector;

    /** True once SetOccupiedSlot has run, so EndPlay knows there is a pad to free. */
    bool bHasOccupiedSlot = false;

    /** Returns the closest living enemy inside AttackRange, or null if there is none. */
    AEnemy* FindNearestEnemyInRange() const;
};