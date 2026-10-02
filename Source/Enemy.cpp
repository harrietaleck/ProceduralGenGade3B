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

    MeshComponent->SetRelativeScale3D(FVector(0.6f));

    // We move the enemy manually.
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Overlap);

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
    UStaticMesh* NewMesh = nullptr;

    switch (EnemyType)
    {
    case EEnemyType::Bear:

        NewMesh = LoadObject<UStaticMesh>(
            nullptr,
            TEXT("/Engine/BasicShapes/Cube.Cube"));

        MeshComponent->SetRelativeScale3D(FVector(1.0f));
        break;

    case EEnemyType::Wolf:

        NewMesh = LoadObject<UStaticMesh>(
            nullptr,
            TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

        MeshComponent->SetRelativeScale3D(FVector(0.45f));
        break;

    case EEnemyType::Basic:
    default:

        NewMesh = LoadObject<UStaticMesh>(
            nullptr,
            TEXT("/Engine/BasicShapes/Sphere.Sphere"));

        MeshComponent->SetRelativeScale3D(FVector(0.6f));
        break;
    }

    if (NewMesh)
    {
        MeshComponent->SetStaticMesh(NewMesh);
    }
}

void AEnemy::BeginPlay()
{
    Super::BeginPlay();

    // Apply the enemy settings again in case the enemy was placed
    // directly in the level instead of being created by the spawner.
    ApplyEnemyType();

    // Start the spawn effect.
    SpawnTargetScale = MeshComponent->GetRelativeScale3D();
    SpawnEffectElapsed = 0.0f;

    if (SpawnEffectDuration > 0.0f)
    {
        MeshComponent->SetRelativeScale3D(
            SpawnTargetScale * 0.12f);

        SpawnGlow->SetLightColor(SpawnGlowColor);
        SpawnGlow->SetIntensity(SpawnGlowIntensity);
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

void AEnemy::SetPath(const TArray<FVector>& InWaypoints)
{
    // Store the path.
    Waypoints = InWaypoints;

    CurrentWaypoint = 0;
    CurrentSpeed = 0.0f;
}

void AEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateSpawnEffect(DeltaSeconds);

    if (HealthComponent->IsDead())
    {
        return;
    }

    // Attack a valid target if one is close enough.
    if (AActor* Target = FindTargetInRange())
    {
        CurrentSpeed = FMath::Max(
            0.0f,
            CurrentSpeed - Acceleration * 2.0f * DeltaSeconds);

        TryAttack(Target, DeltaSeconds);
    }
    else
    {
        MoveAlongPath(DeltaSeconds);
    }
}

void AEnemy::UpdateSpawnEffect(float DeltaSeconds)
{
    if (SpawnEffectDuration <= 0.0f ||
        SpawnEffectElapsed >= SpawnEffectDuration)
    {
        return;
    }

    SpawnEffectElapsed = FMath::Min(
        SpawnEffectElapsed + DeltaSeconds,
        SpawnEffectDuration);

    const float Alpha =
        SpawnEffectElapsed / SpawnEffectDuration;

    const float SmoothAlpha =
        FMath::InterpEaseOut(
            0.0f,
            1.0f,
            Alpha,
            3.0f);

    MeshComponent->SetRelativeScale3D(
        FMath::Lerp(
            SpawnTargetScale * 0.12f,
            SpawnTargetScale,
            SmoothAlpha));

    SpawnGlow->SetIntensity(
        FMath::Lerp(
            SpawnGlowIntensity,
            0.0f,
            SmoothAlpha));

    if (Alpha >= 1.0f)
    {
        MeshComponent->SetRelativeScale3D(
            SpawnTargetScale);

        SpawnGlow->SetVisibility(false);
    }
}

void AEnemy::MoveAlongPath(float DeltaSeconds)
{
    if (Waypoints.Num() == 0)
    {
        return;
    }

    FVector Location = GetActorLocation();

    const float AcceptRadiusSq =
        WaypointAcceptRadius * WaypointAcceptRadius;

    // Skip waypoints we have already reached.
    while (Waypoints.IsValidIndex(CurrentWaypoint))
    {
        const FVector TargetPos =
            Waypoints[CurrentWaypoint] +
            FVector(0.0f, 0.0f, GroundClearance);

        if (FVector::DistSquared2D(
                Location,
                TargetPos) > AcceptRadiusSq)
        {
            break;
        }

        ++CurrentWaypoint;
    }

    if (!Waypoints.IsValidIndex(CurrentWaypoint))
    {
        return;
    }

    const FVector TargetPos =
        Waypoints[CurrentWaypoint] +
        FVector(0.0f, 0.0f, GroundClearance);

    FVector ToTarget =
        TargetPos - Location;

    ToTarget.Z = 0.0f;

    const float Distance = ToTarget.Size();

    if (Distance <= KINDA_SMALL_NUMBER)
    {
        ++CurrentWaypoint;
        return;
    }

    const FVector Direction =
        ToTarget / Distance;

    // Slow slightly before corners.
    float TargetSpeed = MoveSpeed;

    if (Waypoints.IsValidIndex(CurrentWaypoint + 1))
    {
        FVector ToNext =
            Waypoints[CurrentWaypoint + 1] -
            Waypoints[CurrentWaypoint];

        ToNext.Z = 0.0f;

        const float NextLen = ToNext.Size();

        if (NextLen > KINDA_SMALL_NUMBER)
        {
            ToNext /= NextLen;

            const float TurnAlignment =
                FVector::DotProduct(
                    Direction,
                    ToNext);

            const float TurnFactor =
                FMath::Lerp(
                    0.55f,
                    1.0f,
                    FMath::Clamp(
                        (TurnAlignment + 1.0f) * 0.5f,
                        0.0f,
                        1.0f));

            TargetSpeed =
                MoveSpeed * TurnFactor;
        }
    }

    if (CurrentSpeed < TargetSpeed)
    {
        CurrentSpeed =
            FMath::Min(
                TargetSpeed,
                CurrentSpeed +
                Acceleration * DeltaSeconds);
    }
    else
    {
        CurrentSpeed =
            FMath::Max(
                TargetSpeed,
                CurrentSpeed -
                Acceleration * DeltaSeconds);
    }

    const float Step =
        CurrentSpeed * DeltaSeconds;

    const float MoveAmount =
        FMath::Min(Step, Distance);

    FVector NewLocation =
        Location +
        Direction * MoveAmount;

    NewLocation.Z = TargetPos.Z;

    SetActorLocation(NewLocation);

    const FRotator TargetFacing(
        0.0f,
        Direction.Rotation().Yaw,
        0.0f);

    SetActorRotation(
        FMath::RInterpTo(
            GetActorRotation(),
            TargetFacing,
            DeltaSeconds,
            TurnRate));
}

void AEnemy::TryAttack(
    AActor* Target,
    float DeltaSeconds)
{
    AttackTimer -= DeltaSeconds;

    if (AttackTimer > 0.0f)
    {
        return;
    }

    AttackTimer = AttackInterval;

    if (Target)
    {
        FVector ToTarget =
            Target->GetActorLocation() -
            GetActorLocation();

        ToTarget.Z = 0.0f;

        if (!ToTarget.IsNearlyZero())
        {
            SetActorRotation(
                FRotator(
                    0.0f,
                    ToTarget.Rotation().Yaw,
                    0.0f));
        }
    }

    if (UHealthComponent* TargetHealth =
        Target->FindComponentByClass<UHealthComponent>())
    {
        if (!TargetHealth->IsDead())
        {
            TargetHealth->ApplyDamage(
                AttackDamage,
                this);
        }
    }
}

AActor* AEnemy::FindTargetInRange() const
{
    const FVector Location =
        GetActorLocation();

    // First priority: tower.
    if (TargetTower)
    {
        if (UHealthComponent* TowerHealth =
            TargetTower->FindComponentByClass<UHealthComponent>())
        {
            if (!TowerHealth->IsDead() &&
                FVector::Dist(
                    Location,
                    TargetTower->GetActorLocation()) <= AttackRange &&
                HasLineOfSightTo(TargetTower))
            {
                return TargetTower;
            }
        }
    }

    // Second priority: defenders.
    AActor* Best = nullptr;

    float BestDistSq =
        DetectionRadius * DetectionRadius;

    for (TActorIterator<ADefender> It(GetWorld());
         It;
         ++It)
    {
        AActor* Defender = *It;

        UHealthComponent* DefenderHealth =
            Defender->FindComponentByClass<UHealthComponent>();

        if (!DefenderHealth ||
            DefenderHealth->IsDead())
        {
            continue;
        }

        const float DistSq =
            FVector::DistSquared(
                Location,
                Defender->GetActorLocation());

        if (DistSq <= BestDistSq)
        {
            BestDistSq = DistSq;
            Best = Defender;
        }
    }

    if (Best &&
        FVector::Dist(
            Location,
            Best->GetActorLocation()) <= AttackRange &&
        HasLineOfSightTo(Best))
    {
        return Best;
    }

    return nullptr;
}

bool AEnemy::HasLineOfSightTo(
    const AActor* Target) const
{
    if (!Target)
    {
        return false;
    }

    FCollisionQueryParams Params;

    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(Target);

    // Ignore procedural terrain.
    for (TActorIterator<AProceduralTerrain> It(GetWorld());
         It;
         ++It)
    {
        Params.AddIgnoredActor(*It);
    }

    const FVector Start =
        GetActorLocation() +
        FVector(0.0f, 0.0f, GroundClearance);

    const FVector End =
        Target->GetActorLocation() +
        FVector(0.0f, 0.0f, GroundClearance);

    FHitResult Hit;

    return !GetWorld()->LineTraceSingleByChannel(
        Hit,
        Start,
        End,
        ECC_Visibility,
        Params);
}

void AEnemy::HandleDeath(AActor* Killer)
{
    // Give the player the enemy reward.
    if (ATDGameMode* GameMode =
        GetWorld()->GetAuthGameMode<ATDGameMode>())
    {
        GameMode->NotifyEnemyKilled(this);
    }

    Destroy();
}