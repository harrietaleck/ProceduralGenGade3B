#pragma once

#include "CoreMinimal.h"
#include "Defender.h"
#include "ArcherDefender.generated.h"

//Create a defender that attacks in long distances
UCLASS()
class PROCEDURALGENGADE3B_API AArcherDefender : public ADefender
{
    GENERATED_BODY()

public:
    AArcherDefender();

protected:
    virtual void BeginPlay() override;

private:
    //Create a timer for the long range
    FTimerHandle ArcherFireTimerHandle;

    //Form the archer functionality to fire arrows afar
    void FireArrow();

    //Defender finds the closest enemy in the long range
    AEnemy* FindNearestEnemyForArcher() const;
};