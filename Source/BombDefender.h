#pragma once

#include "CoreMinimal.h"
#include "Defender.h"

//Header matches
#include "BombDefender.generated.h"

//Specialise the defender to creat bomb areas
UCLASS()
class PROCEDURALGENGADE3B_API APoisonLightBombDefender :
    public ADefender
{
    GENERATED_BODY()

public:
    APoisonLightBombDefender();

protected:
    virtual void BeginPlay() override;

private:
    //Stores the sphere mesh so it can be reused during gameplay
    UPROPERTY()
    TObjectPtr<UStaticMesh> BombAreaMesh;

    //Two attack locations are selected for the bomb area
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb")
    FVector AttackAreaOne = FVector(500.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb")
    FVector AttackAreaTwo = FVector(1000.0f, 0.0f, 0.0f);

    //Create radium to effect the area
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "50.0"))
    float BombRadius = 300.0f;

    //Create a damage for alal enemies in the radius
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0.0"))
    float BombDamage = 12.0f;

    //Set a timer between bomb attacks
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0.1"))
    float BombCooldown = 2.5f;

    //Set a time for the lighted area of the bomb
    UPROPERTY(EditAnywhere, Category = "Poison Light Bomb", meta = (ClampMin = "0.1"))
    float LightDuration = 0.8f;

    //Set a timer for the bomb attacks
    FTimerHandle BombTimerHandle;

    //Activate the bombs in the areas
    void DetonateBomb();

    //Damage the enemies in the areas
    void DamageEnemiesInArea(const FVector& AreaLocation);

    //Find which areas of the bomb areas have enemies
    FVector GetBestAttackArea() const;
};