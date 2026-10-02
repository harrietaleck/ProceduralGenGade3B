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
    //Disable the settings of the original firing system
    bUseDefaultAttack = false;
    DefenderName = TEXT("Bomb");

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
        MeshComponent->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.8f));
    }

    //Make sure the defender doesnt rely on projectile
    ProjectileClass = nullptr;

    //Bombs are lobbed over a wide area
    AttackRange = 1200.0f;

    //Set the health of the bomb defender
    HealthComponent->MaxHealth = 140.0f;

    //Set a cost that can be adjusted for stronger attacks
    Cost = 75;

    BodyColor = FLinearColor(0.2f, 0.85f, 0.3f);
}

float APoisonLightBombDefender::GetThreatRating() const
{
    //Impact plus the full poison cloud, assuming a couple of enemies per blast
    const float PerBomb = BombDamage + PoisonDamagePerTick * PoisonTicks;
    return PerBomb * 2.0f / BombCooldown;
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

    GetWorldTimerManager().SetTimer(
        PoisonTimerHandle,
        this,
        &APoisonLightBombDefender::TickPoisonClouds,
        PoisonTickInterval,
        true
    );
}

void APoisonLightBombDefender::DetonateBomb()
{
    if (!HealthComponent || HealthComponent->IsDead())
    {
        return;
    }

    //Hold the bomb until there is something to hit
    FVector ClusterCentre;
    if (!FindBestClusterCentre(ClusterCentre))
    {
        return;
    }

    SpawnBlastVisuals(ClusterCentre);
    DamageEnemiesInArea(ClusterCentre, BombDamage);

    if (PoisonTicks > 0)
    {
        FPoisonCloud Cloud;
        Cloud.Location = ClusterCentre;
        Cloud.TicksRemaining = PoisonTicks;
        ActiveClouds.Add(Cloud);
    }
}

void APoisonLightBombDefender::TickPoisonClouds()
{
    if (!HealthComponent || HealthComponent->IsDead())
    {
        ActiveClouds.Reset();
        return;
    }

    for (int32 i = ActiveClouds.Num() - 1; i >= 0; --i)
    {
        FPoisonCloud& Cloud = ActiveClouds[i];
        DamageEnemiesInArea(Cloud.Location, PoisonDamagePerTick, true);

        DrawDebugCircle(GetWorld(), Cloud.Location + FVector(0.0f, 0.0f, 10.0f), BombRadius, 32,
            FColor(80, 255, 110), false, PoisonTickInterval, 0, 4.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);

        if (--Cloud.TicksRemaining <= 0)
        {
            ActiveClouds.RemoveAtSwap(i);
        }
    }
}

void APoisonLightBombDefender::SpawnBlastVisuals(const FVector& AreaLocation)
{
    //Create a light effect over the blast area
    if (UPointLightComponent* BombLight = NewObject<UPointLightComponent>(this))
    {
        BombLight->RegisterComponent();
        BombLight->SetWorldLocation(AreaLocation + FVector(0.0f, 0.0f, 50.0f));
        BombLight->SetLightColor(FLinearColor(0.2f, 1.0f, 0.4f));
        BombLight->SetIntensity(5000.0f);
        BombLight->SetAttenuationRadius(BombRadius);
        BombLight->SetCastShadows(false);
        BombLight->SetVisibility(true);

        //Automatically remove the lighted area after the duration
        FTimerHandle LightTimerHandle;
        TWeakObjectPtr<UPointLightComponent> WeakLight(BombLight);
        GetWorldTimerManager().SetTimer(LightTimerHandle, [WeakLight]()
        {
            if (WeakLight.IsValid())
            {
                WeakLight->DestroyComponent();
            }
        }, LightDuration, false);
    }

    //Create a temporary flattened sphere to show the blast area
    if (UStaticMeshComponent* BombAreaVisual = NewObject<UStaticMeshComponent>(this))
    {
        //Reuse the sphere loaded in the constructor (ConstructorHelpers can't run during gameplay)
        if (BombAreaMesh)
        {
            BombAreaVisual->SetStaticMesh(BombAreaMesh);
        }

        BombAreaVisual->RegisterComponent();
        BombAreaVisual->SetWorldLocation(AreaLocation);

        const float VisualScale = BombRadius / 50.0f;
        BombAreaVisual->SetWorldScale3D(FVector(VisualScale, VisualScale, 0.08f));
        BombAreaVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BombAreaVisual->SetVisibility(true);

        //Remove the highlighted area after the duration
        FTimerHandle VisualTimerHandle;
        TWeakObjectPtr<UStaticMeshComponent> WeakVisual(BombAreaVisual);
        GetWorldTimerManager().SetTimer(VisualTimerHandle, [WeakVisual]()
        {
            if (WeakVisual.IsValid())
            {
                WeakVisual->DestroyComponent();
            }
        }, LightDuration, false);
    }
}

int32 APoisonLightBombDefender::DamageEnemiesInArea(const FVector& AreaLocation, float Damage, bool bPoison)
{
    if (!GetWorld())
    {
        return 0;
    }

    //Collect first so enemies destroyed by the damage don't disturb the iteration
    TArray<UHealthComponent*> Victims;
    TArray<AEnemy*> PoisonTargets;
    for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
    {
        AEnemy* Enemy = *It;
        UHealthComponent* EnemyHealth = Enemy ? Enemy->HealthComponent.Get() : nullptr;
        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }

        if (FVector::Dist2D(AreaLocation, Enemy->GetActorLocation()) <= BombRadius)
        {
            Victims.Add(EnemyHealth);
            PoisonTargets.Add(Enemy);
        }
    }

    //Mark enemies standing in the cloud as poisoned so Archer arrows can spread it
    if (bPoison)
    {
        for (AEnemy* Enemy : PoisonTargets)
        {
            Enemy->ApplyPoison(0.0f, PoisonTickInterval * 2.0f, this);
        }
    }

    for (UHealthComponent* Victim : Victims)
    {
        if (IsValid(Victim) && !Victim->IsDead())
        {
            Victim->ApplyDamage(Damage, this);
        }
    }

    return Victims.Num();
}

bool APoisonLightBombDefender::FindBestClusterCentre(FVector& OutCentre) const
{
    //Living enemies inside the throwing range
    TArray<FVector> Positions;
    const FVector Location = GetActorLocation();
    const float RangeSq = AttackRange * AttackRange;

    for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
    {
        const AEnemy* Enemy = *It;
        const UHealthComponent* EnemyHealth = Enemy ? Enemy->HealthComponent.Get() : nullptr;
        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }

        if (FVector::DistSquared2D(Location, Enemy->GetActorLocation()) <= RangeSq)
        {
            Positions.Add(Enemy->GetActorLocation());
        }
    }

    if (Positions.Num() == 0)
    {
        return false;
    }

    //Score each enemy by how many others stand within one blast radius of it,
    //then aim at the average position of the best group so the blast is centred on it
    const float RadiusSq = BombRadius * BombRadius;
    int32 BestCount = 0;
    FVector BestCentre = Positions[0];

    for (const FVector& Candidate : Positions)
    {
        int32 Count = 0;
        FVector Sum = FVector::ZeroVector;
        for (const FVector& Other : Positions)
        {
            if (FVector::DistSquared2D(Candidate, Other) <= RadiusSq)
            {
                ++Count;
                Sum += Other;
            }
        }

        if (Count > BestCount)
        {
            BestCount = Count;
            BestCentre = Sum / Count;
        }
    }

    OutCentre = BestCentre;
    return true;
}
