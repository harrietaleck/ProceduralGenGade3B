//First header matches
#include "BombDefender.h"

#include "Enemy.h"
#include "HealthComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APoisonLightBombDefender::APoisonLightBombDefender()
{
    //Disable the settings of the orginial firing system
    bUseDefaultAttack = false;

    //Use a sphere for the Poison Light Bomb Defender body
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere")
    );

    //Replace the inherited cylinder with the sphere shape
    if (SphereMesh.Succeeded() && MeshComponent)
    {
        MeshComponent->SetStaticMesh(SphereMesh.Object);

        //Save the sphere mesh so DetonateBomb() can reuse it safely
        BombAreaMesh = SphereMesh.Object;
    }

    //Make the bomb defender a compact orb shape
    if (MeshComponent)
    {
        MeshComponent->SetRelativeScale3D(
            FVector(0.8f, 0.8f, 0.8f)
        );
    }

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

    //creat light effects the areas selected
    UPointLightComponent* BombLight =
        NewObject<UPointLightComponent>(this);

    if (BombLight)
    {
        BombLight->RegisterComponent();

        BombLight->SetWorldLocation(
            SelectedArea + FVector(0.0f, 0.0f, 50.0f)
        );

        BombLight->SetLightColor(
            FLinearColor(0.2f, 1.0f, 0.4f)
        );

        BombLight->SetIntensity(5000.0f);
        BombLight->SetAttenuationRadius(BombRadius);
        BombLight->SetCastShadows(false);
        BombLight->SetVisibility(true);

        //Automaticall remove the lighted area after duration
        FTimerHandle LightTimerHandle;

        GetWorldTimerManager().SetTimer(
            LightTimerHandle,
            [BombLight]()
            {
                if (BombLight)
                {
                    BombLight->DestroyComponent();
                }
            },
            LightDuration,
            false
        );
    }

    //Create a temporary sphere show the highlighted area
    UStaticMeshComponent* BombAreaVisual =
        NewObject<UStaticMeshComponent>(this);

    if (BombAreaVisual)
    {
        // *** CHANGED: Reuse the sphere loaded in the constructor.
        // *** ConstructorHelpers cannot be used here because this function runs during gameplay.
        if (BombAreaMesh)
        {
            BombAreaVisual->SetStaticMesh(BombAreaMesh);
        }

        BombAreaVisual->RegisterComponent();

        BombAreaVisual->SetWorldLocation(
            SelectedArea
        );

        const float VisualScale =
            BombRadius / 50.0f;

        BombAreaVisual->SetWorldScale3D(
            FVector(
                VisualScale,
                VisualScale,
                0.08f
            )
        );

        BombAreaVisual->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );

        BombAreaVisual->SetVisibility(true);

        //Remove highlighted area after the duration
        FTimerHandle VisualTimerHandle;

        GetWorldTimerManager().SetTimer(
            VisualTimerHandle,
            [BombAreaVisual]()
            {
                if (BombAreaVisual)
                {
                    BombAreaVisual->DestroyComponent();
                }
            },
            LightDuration,
            false
        );
    }

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

        UHealthComponent* EnemyHealth =
            Enemy->FindComponentByClass<UHealthComponent>();

        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }

        const float Distance =
            FVector::Dist(
                AreaLocation,
                Enemy->GetActorLocation()
            );

        //Make all the enemies in that area damaged
        if (Distance <= BombRadius)
        {
            EnemyHealth->ApplyDamage(
                BombDamage,
                this
            );
        }
    }
}

FVector APoisonLightBombDefender::GetBestAttackArea() const
{
    const FVector WorldAreaOne =
        GetActorTransform().TransformPosition(AttackAreaOne);

    const FVector WorldAreaTwo =
        GetActorTransform().TransformPosition(AttackAreaTwo);

    int32 EnemiesInAreaOne = 0;
    int32 EnemiesInAreaTwo = 0;

    for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
    {
        AEnemy* Enemy = *It;

        if (!Enemy)
        {
            continue;
        }

        UHealthComponent* EnemyHealth =
            Enemy->FindComponentByClass<UHealthComponent>();

        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }

        const float DistanceOne =
            FVector::Dist(
                WorldAreaOne,
                Enemy->GetActorLocation()
            );

        const float DistanceTwo =
            FVector::Dist(
                WorldAreaTwo,
                Enemy->GetActorLocation()
            );

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