#pragma once

#include "CoreMinimal.h"
#include "Defender.h"
#include "ArcherDefender.generated.h"

//Create a defender that attacks from a long distance.
//It always shoots the enemies closest to the tower, since they are the biggest threat.
//Every few shots it fires a volley that hits several enemies at once.
UCLASS()
class PROCEDURALGENGADE3B_API AArcherDefender : public ADefender
{
    GENERATED_BODY()

public:
    AArcherDefender();

    //Every Nth shot is a volley
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archer", meta = (ClampMin = "1"))
    int32 VolleyEveryNShots = 3;

    //How many different enemies a volley hits
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archer", meta = (ClampMin = "1"))
    int32 VolleyArrowCount = 3;

    //Extra damage against fast wolves
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archer", meta = (ClampMin = "1.0"))
    float WolfDamageMultiplier = 1.5f;

    virtual float GetThreatRating() const override;

protected:
    virtual void BeginPlay() override;

private:
    //Create a timer for the long range
    FTimerHandle ArcherFireTimerHandle;

    //Counts shots so every Nth one becomes a volley
    int32 ShotCounter = 0;

    //Fires the archer's arrows at enemies far away
    void FireArrow();

    //Spawn one arrow at an enemy
    void ShootAt(AEnemy* Target);

    //Living enemies in range, sorted so the one closest to the tower comes first
    void GatherTargetsByThreat(TArray<AEnemy*>& OutTargets) const;
};
