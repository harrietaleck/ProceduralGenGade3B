#include "PoisonLightBombDefender.h"
#include "Enemy.h"
#include "HealthComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

APoisonLightBombDefender::APoisonLightBombDefender()
{
    //Disable the settings of the orginial firing system
    bUseDefaultAttack = false;

    //Make sure the defender doesnt rely on projectile
    ProjectileClass = nullptr;

    //Make the defender have an large effective area to attack
    AttackRange = 1200.0f;

    //Set the health of the bomb defender
    HealthComponent->MaxHealth = 140.0f;

    //Set a cost that can be adjusted for stronger attacks
    Cost = 75;

    //The meta cost identifies the bomb deferder as stronger in attacks
    bStrongDefender = true;
}

void APoisonLightBombDefender::BeginPlay()
{
    Super::BeginPlay();

    //Start the timer for the bomb defender
    GetWorldTimerManager().SetTimer(
        BombTimerHandle,
        this,
        &APoisonLightBombDefender::DetonateBomb,
        BombCooldown,
        true
    );
}

void APoisonLightBombDefender::DetonateBomb()
{
    if (!HealthComponent || HealthComponent->IsDead())
    {
        return;
    }

    //Select 1 - 2 bomb areas
    const FVector SelectedArea = GetBestAttackArea();

    //Draw areas where the bombs will hit to be visisble t the player
    DrawDebugSphere(
        GetWorld(),
        SelectedArea,
        BombRadius,
        32,
        FColor::Green,
        false,
        LightDuration,
        0,
        5.0f
    );

    //Apply damagae to enemies in that area
    DamageEnemiesInArea(SelectedArea);
}

void APoisonLightBombDefender::DamageEnemiesInArea(
    const FVector& AreaLocation)
{
    if (!GetWorld())
    {
        return;
    }

    for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
    {
        AEnemy* Enemy = *It;

        if (!Enemy)
        {
            continue;
        }

        UHealthComponent* EnemyHealth = Enemy->FindComponentByClass<UHealthComponent>();

        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }

        const float Distance = FVector::Dist(AreaLocation,Enemy->GetActorLocation());

        //Make all the enemies in that area damaged
        if (Distance <= BombRadius)
        {
            EnemyHealth->ApplyDamage(BombDamage,this);
        }
    }
}

FVector APoisonLightBombDefender::GetBestAttackArea() const
{
    const FVector WorldAreaOne = GetActorTransform().TransformPosition(AttackAreaOne);

    const FVector WorldAreaTwo = GetActorTransform().TransformPosition(AttackAreaTwo);

    int32 EnemiesInAreaOne = 0;
    int32 EnemiesInAreaTwo = 0;

    for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
    {
        AEnemy* Enemy = *It;

        if (!Enemy)
        {
            continue;
        }

        UHealthComponent* EnemyHealth = Enemy->FindComponentByClass<UHealthComponent>();

        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }

        const float DistanceOne = FVector::Dist(WorldAreaOne, Enemy->GetActorLocation());

        const float DistanceTwo = FVector::Dist(WorldAreaTwo,Enemy->GetActorLocation());

        if (DistanceOne <= BombRadius)
        {
            ++EnemiesInAreaOne;
        }

        if (DistanceTwo <= BombRadius)
        {
            ++EnemiesInAreaTwo;
        }
    }

    //Select areas with more enemies
    if (EnemiesInAreaTwo > EnemiesInAreaOne)
    {
        return WorldAreaTwo;
    }

    return WorldAreaOne;
}