// Enemy.cpp

// Handles enemy movement, combat, spawning visuals and enemy variants.

#include "Enemy.h"

#include "HealthComponent.h"

#include "DamageFlashComponent.h"

#include "Defender.h"

#include "ProceduralTerrain.h"

#include "TDGameMode.h"

#include "Components/PointLightComponent.h"

#include "Components/StaticMeshComponent.h"

#include "Engine/StaticMesh.h"

#include "EngineUtils.h"

#include "Engine/World.h"

#include "UObject/ConstructorHelpers.h"

static constexpr float WaypointAcceptRadius = 55.0f;

AEnemy::AEnemy()

{

    PrimaryActorTick.bCanEverTick = true;

    // Create the enemy mesh.

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(

        TEXT("EnemyMesh"));

    SetRootComponent(MeshComponent);

    // Basic enemy starts as a sphere.

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(

        TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereMesh.Succeeded())

    {

        MeshComponent->SetStaticMesh(

            SphereMesh.Object);

    }

    MeshComponent->SetRelativeScale3D(

        FVector(0.6f));

    // We move the enemy manually.

    MeshComponent->SetCollisionEnabled(

        ECollisionEnabled::QueryOnly);

    MeshComponent->SetCollisionResponseToAllChannels(

        ECR_Overlap);

    // Create spawn glow.

    SpawnGlow = CreateDefaultSubobject<UPointLightComponent>(

        TEXT("SpawnPortalGlow"));

    SpawnGlow->SetupAttachment(MeshComponent);

    SpawnGlow->SetRelativeLocation(FVector::ZeroVector);

    SpawnGlow->SetLightColor(SpawnGlowColor);

    SpawnGlow->SetIntensity(SpawnGlowIntensity);

    SpawnGlow->SetAttenuationRadius(300.0f);

    SpawnGlow->SetSourceRadius(35.0f);

    SpawnGlow->SetCastShadows(false);

    // Create health.

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(

        TEXT("HealthComponent"));

    HealthComponent->MaxHealth = 100.0f;

    CreateDefaultSubobject<UDamageFlashComponent>(

        TEXT("DamageFlash"));

}


void AEnemy::SetEnemyType(EEnemyType InEnemyType)

{

    // Store the enemy type.

    EnemyType = InEnemyType;

    // Apply the correct stats and visual.

    ApplyEnemyType();

}


void AEnemy::ApplyEnemyType()

{

    FString MeshPath;

    switch (EnemyType)

    {

    case EEnemyType::Bear:

        // Bear is slower but stronger.

        MoveSpeed = 75.0f;

        AttackDamage = 30.0f;

        ResourceReward = 35;

        HealthComponent->MaxHealth = 250.0f;

        MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

        MeshComponent->SetRelativeScale3D(FVector(1.0f));

        ResourceType = EResourceType::ArcaneOrb;

        break;

    case EEnemyType::Wolf:

        // Wolf is faster but weaker.

        MoveSpeed = 240.0f;

        AttackDamage = 15.0f;

        ResourceReward = 25;

        HealthComponent->MaxHealth = 75.0f;

        MeshPath =

            TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

        MeshComponent->SetRelativeScale3D(

            FVector(0.45f));

        ResourceType =

            EResourceType::ToxicMucus;

        break;

    default:

        // Basic enemy is balanced.

        MoveSpeed = 150.0f;

        AttackDamage = 10.0f;

        ResourceReward = 20;

        HealthComponent->MaxHealth = 100.0f;

        MeshPath =

            TEXT("/Engine/BasicShapes/Sphere.Sphere");

        MeshComponent->SetRelativeScale3D(

            FVector(0.6f));

        ResourceType =

            EResourceType::ArcaneOrb;

        break;

    }

    // Apply the selected mesh.

    MeshComponent->SetStaticMesh(

        LoadObject<UStaticMesh>(

            nullptr,

            *MeshPath));

}


void AEnemy::BeginPlay()

{

    Super::BeginPlay();

    // Apply the enemy settings again in case the enemy was placed

    // directly in the level instead of being created by the spawner.

    ApplyEnemyType();

    // Start the spawn effect.

    SpawnTargetScale =

        MeshComponent->GetRelativeScale3D();

    SpawnEffectElapsed = 0.0f;

    if (SpawnEffectDuration > 0.0f)

    {

        MeshComponent->SetRelativeScale3D(

            SpawnTargetScale * 0.12f);

        SpawnGlow->SetLightColor(

            SpawnGlowColor);

        SpawnGlow->SetIntensity(

            SpawnGlowIntensity);

        SpawnGlow->SetVisibility(true);

    }

    else

    {

        SpawnGlow->SetVisibility(false);

    }

    // Listen for death.

    HealthComponent->OnDeath.AddDynamic(

        this,

        &AEnemy::HandleDeath);

}


//Stores the waypoint path given by the enemy spawner

void AEnemy::SetPath(

    const TArray<FVector>& InWaypoints)

{

    Waypoints = InWaypoints;

    CurrentWaypoint = 0;

    CurrentSpeed = 0.0f;

}


//Updates movement and animal attack behaviour

void AEnemy::Tick(float DeltaSeconds)

{

    Super::Tick(DeltaSeconds);

    if (!HealthComponent || HealthComponent->IsDead())

    {

        return;

    }

    //Play the enemy spawn animation

    UpdateSpawnEffect(DeltaSeconds);

    //Bear and Wolf look for defenders to attack

    if (EnemyType == EEnemyType::Bear ||

        EnemyType == EEnemyType::Wolf)

    {

        AActor* DefenderTarget = FindTargetInRange();

        if (DefenderTarget)

        {

            TryAttack(

                DefenderTarget,

                DeltaSeconds);

            return;

        }

    }

    //Continue moving toward the tower when there is no defender

    MoveAlongPath(DeltaSeconds);

}


//Handles the enemy spawn animation

void AEnemy::UpdateSpawnEffect(float DeltaSeconds)

{

    if (!MeshComponent)

    {

        return;

    }

    if (SpawnEffectDuration <= 0.0f)

    {

        MeshComponent->SetRelativeScale3D(

            SpawnTargetScale);

        return;

    }

    SpawnEffectElapsed += DeltaSeconds;

    const float Alpha =

        FMath::Clamp(

            SpawnEffectElapsed /

            SpawnEffectDuration,

            0.0f,

            1.0f);

    const float ScaleMultiplier =

        FMath::Lerp(

            0.12f,

            1.0f,

            Alpha);

    MeshComponent->SetRelativeScale3D(

        SpawnTargetScale * ScaleMultiplier);

    if (SpawnGlow)

    {

        SpawnGlow->SetIntensity(

            SpawnGlowIntensity *

            (1.0f - Alpha));

        if (Alpha >= 1.0f)

        {

            SpawnGlow->SetVisibility(false);

        }

    }

}


//Moves the enemy through its waypoint path

void AEnemy::MoveAlongPath(float DeltaSeconds)

{

    if (Waypoints.Num() == 0)

    {

        // If there is no path, try attacking the tower

        if (TargetTower)

        {

            TryAttack(

                TargetTower,

                DeltaSeconds);

        }

        return;

    }

    //Reached the final waypoint, so attack the tower

    if (CurrentWaypoint >= Waypoints.Num())

    {

        if (TargetTower)

        {

            TryAttack(

                TargetTower,

                DeltaSeconds);

        }

        return;

    }

    const FVector CurrentLocation =

        GetActorLocation();

    const FVector TargetLocation =

        Waypoints[CurrentWaypoint];

    FVector Direction =

        TargetLocation - CurrentLocation;

    Direction.Z = 0.0f;

    const float Distance =

        Direction.Size();

    if (Distance <= WaypointAcceptRadius)

    {

        CurrentWaypoint++;

        return;

    }

    Direction.Normalize();

    CurrentSpeed =

        FMath::FInterpTo(

            CurrentSpeed,

            MoveSpeed,

            DeltaSeconds,

            Acceleration);

    const FVector NewLocation =

        CurrentLocation +

        Direction *

        CurrentSpeed *

        DeltaSeconds;

    SetActorLocation(NewLocation);

    if (!Direction.IsNearlyZero())

    {

        const FRotator TargetRotation =

            Direction.Rotation();

        const FRotator NewRotation =

            FMath::RInterpTo(

                GetActorRotation(),

                TargetRotation,

                DeltaSeconds,

                TurnRate);

        SetActorRotation(NewRotation);

    }

}


void AEnemy::TryAttack(AActor* Target, float DeltaSeconds)

{

    //Do not attack if there isnt a target

    if (!Target)

    {

        return;

    }

    //Count down the attack timer

    AttackTimer -= DeltaSeconds;

    //Create a wait time till the enemy can attack

    if (AttackTimer > 0.0f)

    {

        return;

    }

    //Find the health componenet

    UHealthComponent* TargetHealth =

        Target->FindComponentByClass<UHealthComponent>();

    //Stop attacking if there isnt a healt ccomponent

    if (!TargetHealth)

    {

        return;

    }

    //Stop attacking whatt is dead

    if (TargetHealth->IsDead())

    {

        return;

    }

    //Caculate the distance to the target

    const float Distance =

        FVector::Dist(

            GetActorLocation(),

            Target->GetActorLocation());

    //Do not attack when the enemy is not in attack range

    if (Distance > AttackRange)

    {

        return;

    }

    //Inflict damage to the health componenet system use on the defenders

    TargetHealth->ApplyDamage(

        AttackDamage,

        this);

    // Reset the attack timer.

    AttackTimer = AttackInterval;

}


//Finds the closest living defender for the Bear or Wolf

AActor* AEnemy::FindTargetInRange() const

{

    UWorld* World = GetWorld();

    if (!World)

    {

        return nullptr;

    }

    ADefender* ClosestDefender = nullptr;

    float ClosestDistanceSquared = BIG_NUMBER;

    for (TActorIterator<ADefender> It(World);

         It;

         ++It)

    {

        ADefender* Defender = *It;

        if (!Defender)

        {

            continue;

        }

        UHealthComponent* DefenderHealth =

            Defender->FindComponentByClass<UHealthComponent>();

        //Ignore defenders that are already dead

        if (!DefenderHealth ||

            DefenderHealth->IsDead())

        {

            continue;

        }

        const float DistanceSquared =

            FVector::DistSquared(

                GetActorLocation(),

                Defender->GetActorLocation());

        //Only target defenders inside the enemy attack range

        if (DistanceSquared >

            FMath::Square(AttackRange))

        {

            continue;

        }

        //Keep the closest defender

        if (DistanceSquared < ClosestDistanceSquared)

        {

            ClosestDistanceSquared =

                DistanceSquared;

            ClosestDefender =

                Defender;

        }

    }

    return ClosestDefender;

}


//Checks whether the enemy has a clear view of its target

bool AEnemy::HasLineOfSightTo(

    const AActor* Target) const

{

    if (!Target || !GetWorld())

    {

        return false;

    }

    FHitResult Hit;

    FCollisionQueryParams QueryParams;

    QueryParams.AddIgnoredActor(this);

    const bool bHit =

        GetWorld()->LineTraceSingleByChannel(

            Hit,

            GetActorLocation(),

            Target->GetActorLocation(),

            ECC_Visibility,

            QueryParams);

    if (!bHit)

    {

        return true;

    }

    return Hit.GetActor() == Target;

}


// Handles enemy death while preserving the existing loot system

void AEnemy::HandleDeath(AActor* Killer)

{

    //Award the enemy's loot and score before destroying it

    if (ATDGameMode* GameMode =

        Cast<ATDGameMode>(GetWorld()->GetAuthGameMode()))

    {

        GameMode->NotifyEnemyKilled(this);

    }

    Destroy();

}