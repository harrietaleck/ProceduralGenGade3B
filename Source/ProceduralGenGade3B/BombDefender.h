#pragma once

#include "CoreMinimal.h"
#include "Defender.h"

//Header matches
#include "BombDefender.generated.h"

//A lingering poison cloud left behind by a bomb
struct FPoisonCloud
{
    FVector Location = FVector::ZeroVector;
    int32 TicksRemaining = 0;
};

//Specialise the defender to throw area bombs at the densest group of enemies in range.
//Each bomb deals impact damage and leaves a poison cloud that keeps damaging enemies inside it.
UCLASS()
class PROCEDURALGENGADE3B_API APoisonLightBombDefender :
    public ADefender
{
    GENERATED_BODY()

public:
    APoisonLightBombDefender();

    virtual float GetThreatRating() const override;
    virtual bool IsAreaAttacker() const override { return true; }

protected:
    virtual void BeginPlay() override;

private:
    //Stores the sphere mesh so it can be reused during gameplay
    UPROPERTY()
    TObjectPtr<UStaticMesh> BombAreaMesh;

    //Create radius to effect the area
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "50.0"))
    float BombRadius = 300.0f;

    //Impact damage for all enemies in the radius
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0.0"))
    float BombDamage = 12.0f;

    //Set a timer between bomb attacks
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0.1"))
    float BombCooldown = 2.5f;

    //Set a time for the lighted area of the bomb
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0.1"))
    float LightDuration = 0.8f;

    //Poison damage dealt to each enemy in the cloud per tick
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0.0"))
    float PoisonDamagePerTick = 3.0f;

    //How many poison ticks a cloud lasts
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0"))
    int32 PoisonTicks = 4;

    //Seconds between poison ticks
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0.1"))
    float PoisonTickInterval = 0.5f;

    //Set a timer for the bomb attacks
    FTimerHandle BombTimerHandle;

    //Timer that ticks every active poison cloud
    FTimerHandle PoisonTimerHandle;

    //Clouds still poisoning the ground
    TArray<FPoisonCloud> ActiveClouds;

    //Throw a bomb at the best cluster, if any enemy is in range
    void DetonateBomb();

    //Show the blast light and area marker
    void SpawnBlastVisuals(const FVector& AreaLocation);

    //Damage the enemies in the area, returns how many were hit
    int32 DamageEnemiesInArea(const FVector& AreaLocation, float Damage);

    //Apply one poison tick to every active cloud
    void TickPoisonClouds();

    //Find the enemy whose surroundings contain the most enemies (the densest cluster)
    bool FindBestClusterCentre(FVector& OutCentre) const;
};
