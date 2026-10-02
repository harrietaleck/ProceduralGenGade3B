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
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyMesh"));
    SetRootComponent(MeshComponent);

    // Basic enemy starts as a sphere.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(SphereMesh.Object);
    }

    //Vector was changed to FVector so the Unreal vector type is recognised
    MeshComponent->SetRelativeScale3D(FVector(0.6f));

    // We move the enemy manually.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Overlap);

    // Create spawn glow.
    SpawnGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("SpawnPortalGlow"));
    SpawnGlow->SetupAttachment(MeshComponent);
    SpawnGlow->SetRelativeLocation(FVector::ZeroVector);
    SpawnGlow->SetLightColor(SpawnGlowColor);
    SpawnGlow->SetIntensity(SpawnGlowIntensity);
    SpawnGlow->SetAttenuationRadius(300.0f);
    SpawnGlow->SetSourceRadius(35.0f);
    SpawnGlow->SetCastShadows(false);

    // Create health.
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
    HealthComponent->MaxHealth = 100.0f;
    CreateDefaultSubobject<UDamageFlashComponent>(TEXT("DamageFlash"));
}


//Starts the enemy's health/death system and applies its variant stats
void AEnemy::BeginPlay()
{
    Super::BeginPlay();

    if (HealthComponent)
    {
        HealthComponent->OnDeath.AddDynamic(
            this,
            &AEnemy::HandleDeath);
    }

    ApplyEnemyType();

    CurrentSpeed = 0.0f;
    AttackTimer = 0.0f;
    SpawnEffectElapsed = 0.0f;

    SpawnTargetScale = MeshComponent
        ? MeshComponent->GetRelativeScale3D()
        : FVector::OneVector;
}


//Updates the spawn effect, attacking and movement every frame
void AEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HealthComponent || HealthComponent->IsDead())
    {
        return;
    }

    UpdateSpawnEffect(DeltaSeconds);

    //Look for a defender first
    AActor* Target = FindTargetInRange();

    if (Target)
    {
        TryAttack(Target, DeltaSeconds);
        return;
    }

    //If there is no defender to attack, continue toward the tower
    MoveAlongPath(DeltaSeconds);
}


// Changes the enemy variant before BeginPlay
void AEnemy::SetEnemyType(EEnemyType InEnemyType)
{
    EnemyType = InEnemyType;
}


//Gives this enemy the terrain path it must follow
void AEnemy::SetPath(const TArray<FVector>& InWaypoints)
{
    Waypoints = InWaypoints;
    CurrentWaypoint = 0;
}


//Applies different stats to Basic, Bear and Wolf
void AEnemy::ApplyEnemyType()
{
    switch (EnemyType)
    {
    case EEnemyType::Basic:

        MoveSpeed = 150.0f;
        AttackDamage = 10.0f;
        AttackInterval = 1.0f;
        AttackRange = 280.0f;

        if (HealthComponent)
        {
            HealthComponent->MaxHealth = 100.0f;
        }

        break;


    case EEnemyType::Bear:

        //Bear is slower but has much more health and damage
        MoveSpeed = 90.0f;
        AttackDamage = 20.0f;
        AttackInterval = 1.2f;
        AttackRange = 300.0f;

        if (HealthComponent)
        {
            HealthComponent->MaxHealth = 250.0f;
        }

        break;


    case EEnemyType::Wolf:

        //Wolf is faster but has less health than the Bear
        MoveSpeed = 220.0f;
        AttackDamage = 15.0f;
        AttackInterval = 0.7f;
        AttackRange = 300.0f;

        if (HealthComponent)
        {
            HealthComponent->MaxHealth = 140.0f;
        }

        break;
    }
}


//Handles the enemy's glowing spawn animation
void AEnemy::UpdateSpawnEffect(float DeltaSeconds)
{
    if (!MeshComponent)
    {
        return;
    }

    SpawnEffectElapsed += DeltaSeconds;

    if (SpawnEffectDuration <= 0.0f)
    {
        MeshComponent->SetRelativeScale3D(SpawnTargetScale);

        if (SpawnGlow)
        {
            SpawnGlow->SetVisibility(false);
        }

        return;
    }

    const float Alpha =
        FMath::Clamp(
            SpawnEffectElapsed / SpawnEffectDuration,
            0.0f,
            1.0f);

    const float ScaleMultiplier =
        FMath::Lerp(0.15f, 1.0f, Alpha);

    MeshComponent->SetRelativeScale3D(
        SpawnTargetScale * ScaleMultiplier);

    if (SpawnGlow)
    {
        SpawnGlow->SetIntensity(
            SpawnGlowIntensity * (1.0f - Alpha));

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
        return;
    }

    //If all waypoints are complete, attack the tower
    if (CurrentWaypoint >= Waypoints.Num())
    {
        if (TargetTower)
        {
            TryAttack(TargetTower, DeltaSeconds);
        }

        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    const FVector TargetLocation = Waypoints[CurrentWaypoint];

    FVector Direction =
        TargetLocation - CurrentLocation;

    Direction.Z = 0.0f;

    const float Distance = Direction.Size();

    if (Distance <= WaypointAcceptRadius)
    {
        CurrentWaypoint++;

        //Attack the tower after reaching the final waypoint
        if (CurrentWaypoint >= Waypoints.Num() && TargetTower)
        {
            TryAttack(TargetTower, DeltaSeconds);
        }

        return;
    }

    Direction.Normalize();

    CurrentSpeed = FMath::FInterpTo(
        CurrentSpeed,
        MoveSpeed,
        DeltaSeconds,
        Acceleration);

    const FVector NewLocation =
        CurrentLocation +
        Direction * CurrentSpeed * DeltaSeconds;

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


//Damages the selected defender or tower
void AEnemy::TryAttack(
    AActor* Target,
    float DeltaSeconds)
{
    if (!Target || !HealthComponent || HealthComponent->IsDead())
    {
        return;
    }

    UHealthComponent* TargetHealth =
        Target->FindComponentByClass<UHealthComponent>();

    if (!TargetHealth || TargetHealth->IsDead())
    {
        return;
    }

    const float Distance =
        FVector::Dist(
            GetActorLocation(),
            Target->GetActorLocation());

    if (Distance > AttackRange)
    {
        return;
    }

    AttackTimer -= DeltaSeconds;

    if (AttackTimer <= 0.0f)
    {
        //The enemy damages the defender/tower directly
        TargetHealth->ApplyDamage(
            AttackDamage,
            this);

        AttackTimer = AttackInterval;
    }
}


//Finds a living defender within attack range
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

        if (!DefenderHealth || DefenderHealth->IsDead())
        {
            continue;
        }

        const float DistanceSquared =
            FVector::DistSquared(
                GetActorLocation(),
                Defender->GetActorLocation());

        if (DistanceSquared >
            FMath::Square(AttackRange))
        {
            continue;
        }

        //Keep the nearest defender as the target
        if (DistanceSquared < ClosestDistanceSquared)
        {
            if (HasLineOfSightTo(Defender))
            {
                ClosestDistanceSquared = DistanceSquared;
                ClosestDefender = Defender;
            }
        }
    }

    return ClosestDefender;
}


//Checks whether the enemy can see the defender
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

    //If nothing blocks the trace, the target can be attacked
    if (!bHit)
    {
        return true;
    }

    return Hit.GetActor() == Target;
}


//Removes the enemy when its health reaches zero
void AEnemy::HandleDeath(AActor* Killer)
{
    Destroy();
}