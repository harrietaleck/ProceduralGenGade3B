// Enemy.cpp

// Handles enemy movement, combat, spawning visuals and enemy variants.

#include "Enemy.h"

#include "HealthComponent.h"
#include "DamageFlashComponent.h"
#include "Defender.h"
#include "TDGameMode.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

static constexpr float WaypointAcceptRadius = 55.0f;

AEnemy::AEnemy()
{
    PrimaryActorTick.bCanEverTick = true;

    //Create the enemy mesh
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyMesh"));
    SetRootComponent(MeshComponent);

    //Basic enemy starts as a sphere
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(SphereMesh.Object);
    }
    MeshComponent->SetRelativeScale3D(FVector(0.6f));

    //We move the enemy manually
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Overlap);

    //Create spawn glow
    SpawnGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("SpawnPortalGlow"));
    SpawnGlow->SetupAttachment(MeshComponent);
    SpawnGlow->SetRelativeLocation(FVector::ZeroVector);
    SpawnGlow->SetLightColor(SpawnGlowColor);
    SpawnGlow->SetIntensity(SpawnGlowIntensity);
    SpawnGlow->SetAttenuationRadius(300.0f);
    SpawnGlow->SetSourceRadius(35.0f);
    SpawnGlow->SetCastShadows(false);

    //Create health
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
    HealthComponent->MaxHealth = 100.0f;

    CreateDefaultSubobject<UDamageFlashComponent>(TEXT("DamageFlash"));
}

FString AEnemy::GetTypeName(EEnemyType Type)
{
    switch (Type)
    {
    case EEnemyType::Bear: return TEXT("Bear");
    case EEnemyType::Wolf: return TEXT("Wolf");
    default:               return TEXT("Basic");
    }
}

void AEnemy::SetEnemyType(EEnemyType InEnemyType)
{
    //Store the enemy type
    EnemyType = InEnemyType;

    //Apply the correct stats and visual
    ApplyEnemyType();
}

void AEnemy::ApplyEnemyType()
{
    FString MeshPath;

    switch (EnemyType)
    {
    case EEnemyType::Bear:
        //Bear is a slow armoured juggernaut that marches on the tower
        MoveSpeed = 80.0f;
        Acceleration = 40.0f;
        TurnRate = 2.0f;
        AttackDamage = 30.0f;
        AttackInterval = 1.6f;
        ResourceReward = 35;
        HealthComponent->MaxHealth = 260.0f;
        MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
        MeshComponent->SetRelativeScale3D(FVector(1.0f));
        BodyColor = FLinearColor(0.42f, 0.24f, 0.08f);
        ResourceType = EResourceType::ArcaneOrb;
        break;

    case EEnemyType::Wolf:
        //Wolf is a fast, fragile hunter that targets defenders
        MoveSpeed = 240.0f;
        Acceleration = 120.0f;
        TurnRate = 4.0f;
        AttackDamage = 12.0f;
        AttackInterval = 0.7f;
        ResourceReward = 25;
        HealthComponent->MaxHealth = 75.0f;
        MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
        MeshComponent->SetRelativeScale3D(FVector(0.45f));
        BodyColor = FLinearColor(0.55f, 0.62f, 0.75f);
        ResourceType = EResourceType::ToxicMucus;
        break;

    default:
        //Basic enemy is balanced
        MoveSpeed = 150.0f;
        Acceleration = 68.0f;
        TurnRate = 2.6f;
        AttackDamage = 10.0f;
        AttackInterval = 1.0f;
        ResourceReward = 20;
        HealthComponent->MaxHealth = 100.0f;
        MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
        MeshComponent->SetRelativeScale3D(FVector(0.6f));
        BodyColor = FLinearColor(0.85f, 0.15f, 0.15f);
        ResourceType = EResourceType::ArcaneOrb;
        break;
    }

    //Apply the selected mesh
    if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath))
    {
        MeshComponent->SetStaticMesh(Mesh);
    }
}

void AEnemy::ApplyBodyColor()
{
    if (!MeshComponent)
    {
        return;
    }

    if (!BodyMaterial)
    {
        UMaterialInterface* BaseMaterial = MeshComponent->GetMaterial(0);
        if (!BaseMaterial)
        {
            return;
        }
        BodyMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        MeshComponent->SetMaterial(0, BodyMaterial);
    }

    if (BodyMaterial)
    {
        BodyMaterial->SetVectorParameterValue(TEXT("Color"), BodyColor);
    }
}

void AEnemy::BeginPlay()
{
    //Apply type stats before the components start so health begins at the type's maximum
    //(also covers enemies placed directly in the level instead of by the spawner)
    ApplyEnemyType();

    Super::BeginPlay();

    ApplyBodyColor();

    //Start the spawn effect
    SpawnTargetScale = MeshComponent->GetRelativeScale3D();
    SpawnEffectElapsed = 0.0f;
    SlamTimer = SlamInterval;

    if (SpawnEffectDuration > 0.0f)
    {
        MeshComponent->SetRelativeScale3D(SpawnTargetScale * 0.12f);
        SpawnGlow->SetLightColor(SpawnGlowColor);
        SpawnGlow->SetIntensity(SpawnGlowIntensity);
        SpawnGlow->SetVisibility(true);
    }
    else
    {
        SpawnGlow->SetVisibility(false);
    }

    //Listen for death
    HealthComponent->OnDeath.AddDynamic(this, &AEnemy::HandleDeath);
}

void AEnemy::ApplyStun(float Duration)
{
    const float Scaled = EnemyType == EEnemyType::Bear ? Duration * 0.5f : Duration;
    StunTimeRemaining = FMath::Max(StunTimeRemaining, Scaled);
}

//Stores the waypoint path given by the enemy spawner
void AEnemy::SetPath(const TArray<FVector>& InWaypoints)
{
    Waypoints = InWaypoints;
    CurrentWaypoint = 0;
    CurrentSpeed = 0.0f;
}

//Updates movement and the behaviour of each enemy type
void AEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HealthComponent || HealthComponent->IsDead())
    {
        return;
    }

    //Play the enemy spawn animation
    UpdateSpawnEffect(DeltaSeconds);

    if (StunTimeRemaining > 0.0f)
    {
        StunTimeRemaining -= DeltaSeconds;
        CurrentSpeed = 0.0f;
        return;
    }

    AttackTimer -= DeltaSeconds;

    switch (EnemyType)
    {
    case EEnemyType::Wolf: TickWolf(DeltaSeconds); break;
    case EEnemyType::Bear: TickBear(DeltaSeconds); break;
    default:               TickBasic(DeltaSeconds); break;
    }
}

//Basic: hold position and fight anything in reach, otherwise keep walking
void AEnemy::TickBasic(float DeltaSeconds)
{
    if (AActor* Target = FindTargetInRange())
    {
        FaceTowards(Target->GetActorLocation(), DeltaSeconds);
        TryAttack(Target, DeltaSeconds);
        return;
    }

    MoveAlongPath(DeltaSeconds);
}

//Wolf: leave the path to hunt nearby defenders, then rejoin the path
void AEnemy::TickWolf(float DeltaSeconds)
{
    ADefender* Prey = HuntTarget.Get();
    const bool bWasHunting = HuntTarget.IsValid() || !HuntTarget.IsExplicitlyNull();

    //Give up on prey that died or ran too far away
    if (Prey)
    {
        const bool bPreyDead = !Prey->HealthComponent || Prey->HealthComponent->IsDead();
        const bool bPreyTooFar = FVector::Dist2D(GetActorLocation(), Prey->GetActorLocation()) > HuntLeashRadius;
        if (bPreyDead || bPreyTooFar)
        {
            Prey = nullptr;
        }
    }

    if (!Prey && bWasHunting)
    {
        HuntTarget.Reset();
        RejoinPath();
    }

    //Look for new prey while still travelling the path
    if (!Prey && !HasReachedTower())
    {
        Prey = FindClosestDefender(HuntRadius);
        HuntTarget = Prey;
    }

    if (Prey)
    {
        const FVector PreyLocation = Prey->GetActorLocation();
        if (FVector::Dist2D(GetActorLocation(), PreyLocation) > AttackRange * 0.7f)
        {
            MoveTowardsLocation(PreyLocation, DeltaSeconds);
        }
        else
        {
            FaceTowards(PreyLocation, DeltaSeconds);
            TryAttack(Prey, DeltaSeconds);
        }
        return;
    }

    MoveAlongPath(DeltaSeconds);
}

//Bear: never stop for defenders, slam them while marching, enrage when hurt
void AEnemy::TickBear(float DeltaSeconds)
{
    if (!bEnraged && HealthComponent->GetHealthPercent() <= EnrageHealthFraction)
    {
        bEnraged = true;
        MoveSpeed *= EnrageSpeedMultiplier;
        AttackInterval *= 0.75f;
        BodyColor = FLinearColor(1.0f, 0.25f, 0.05f);
        ApplyBodyColor();
    }

    SlamTimer -= DeltaSeconds;
    if (SlamTimer <= 0.0f && FindClosestDefender(SlamRadius))
    {
        PerformGroundSlam();
        SlamTimer = bEnraged ? SlamInterval * 0.7f : SlamInterval;
    }

    MoveAlongPath(DeltaSeconds);
}

void AEnemy::PerformGroundSlam()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector Origin = GetActorLocation();
    const float SlamDamage = AttackDamage * SlamDamageScale;

    for (TActorIterator<ADefender> It(World); It; ++It)
    {
        ADefender* Defender = *It;
        UHealthComponent* DefenderHealth = Defender ? Defender->HealthComponent.Get() : nullptr;
        if (!DefenderHealth || DefenderHealth->IsDead())
        {
            continue;
        }

        if (FVector::Dist2D(Origin, Defender->GetActorLocation()) <= SlamRadius)
        {
            DefenderHealth->ApplyDamage(SlamDamage, this);
        }
    }

    //Shockwave ring so the slam reads clearly
    const FColor RingColor = bEnraged ? FColor(255, 70, 20) : FColor(160, 100, 40);
    DrawDebugCircle(World, Origin - FVector(0.0f, 0.0f, GroundClearance - 5.0f), SlamRadius, 40,
        RingColor, false, 0.4f, 0, 8.0f, FVector(1, 0, 0), FVector(0, 1, 0), false);
}

//Handles the enemy spawn animation
void AEnemy::UpdateSpawnEffect(float DeltaSeconds)
{
    if (!MeshComponent || SpawnEffectElapsed >= SpawnEffectDuration)
    {
        return;
    }

    if (SpawnEffectDuration <= 0.0f)
    {
        MeshComponent->SetRelativeScale3D(SpawnTargetScale);
        return;
    }

    SpawnEffectElapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(SpawnEffectElapsed / SpawnEffectDuration, 0.0f, 1.0f);
    MeshComponent->SetRelativeScale3D(SpawnTargetScale * FMath::Lerp(0.12f, 1.0f, Alpha));

    if (SpawnGlow)
    {
        SpawnGlow->SetIntensity(SpawnGlowIntensity * (1.0f - Alpha));
        if (Alpha >= 1.0f)
        {
            SpawnGlow->SetVisibility(false);
        }
    }
}

//Moves the enemy through its waypoint path, attacking the tower at the end
void AEnemy::MoveAlongPath(float DeltaSeconds)
{
    if (Waypoints.Num() == 0 || CurrentWaypoint >= Waypoints.Num())
    {
        if (TargetTower)
        {
            FaceTowards(TargetTower->GetActorLocation(), DeltaSeconds);
            TryAttack(TargetTower, DeltaSeconds);
        }
        return;
    }

    if (MoveTowardsLocation(Waypoints[CurrentWaypoint], DeltaSeconds) <= WaypointAcceptRadius)
    {
        CurrentWaypoint++;
    }
}

float AEnemy::MoveTowardsLocation(const FVector& Destination, float DeltaSeconds)
{
    const FVector CurrentLocation = GetActorLocation();
    FVector Direction = Destination - CurrentLocation;
    Direction.Z = 0.0f;

    const float Distance = Direction.Size();
    if (Distance <= WaypointAcceptRadius)
    {
        return Distance;
    }
    Direction /= Distance;

    CurrentSpeed = FMath::FInterpTo(CurrentSpeed, MoveSpeed, DeltaSeconds, Acceleration);
    const float Step = FMath::Min(CurrentSpeed * DeltaSeconds, Distance);
    SetActorLocation(CurrentLocation + Direction * Step);

    const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), Direction.Rotation(), DeltaSeconds, TurnRate);
    SetActorRotation(NewRotation);

    return Distance - Step;
}

void AEnemy::FaceTowards(const FVector& Location, float DeltaSeconds)
{
    FVector ToTarget = Location - GetActorLocation();
    ToTarget.Z = 0.0f;
    if (ToTarget.IsNearlyZero())
    {
        return;
    }

    const FRotator Desired(0.0f, ToTarget.Rotation().Yaw, 0.0f);
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, TurnRate * 2.0f));
}

void AEnemy::RejoinPath()
{
    if (Waypoints.Num() == 0)
    {
        return;
    }

    //Continue from the closest waypoint that is not far behind the wolf's progress
    const FVector Location = GetActorLocation();
    int32 BestIndex = FMath::Clamp(CurrentWaypoint, 0, Waypoints.Num() - 1);
    float BestDistSq = TNumericLimits<float>::Max();

    for (int32 i = FMath::Max(0, CurrentWaypoint - 1); i < Waypoints.Num(); ++i)
    {
        const float DistSq = FVector::DistSquared2D(Location, Waypoints[i]);
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestIndex = i;
        }
    }

    CurrentWaypoint = BestIndex;
}

void AEnemy::TryAttack(AActor* Target, float DeltaSeconds)
{
    //Do not attack if there isnt a target or the attack is cooling down
    if (!Target || AttackTimer > 0.0f)
    {
        return;
    }

    UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>();
    if (!TargetHealth || TargetHealth->IsDead())
    {
        return;
    }

    //Do not attack when the enemy is not in attack range
    if (FVector::Dist(GetActorLocation(), Target->GetActorLocation()) > AttackRange)
    {
        return;
    }

    TargetHealth->ApplyDamage(AttackDamage, this);
    AttackTimer = AttackInterval;
}

ADefender* AEnemy::FindClosestDefender(float Radius) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    ADefender* ClosestDefender = nullptr;
    float ClosestDistanceSquared = FMath::Square(Radius);

    for (TActorIterator<ADefender> It(World); It; ++It)
    {
        ADefender* Defender = *It;
        UHealthComponent* DefenderHealth = Defender ? Defender->HealthComponent.Get() : nullptr;

        //Ignore defenders that are already dead
        if (!DefenderHealth || DefenderHealth->IsDead())
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Defender->GetActorLocation());
        if (DistanceSquared <= ClosestDistanceSquared)
        {
            ClosestDistanceSquared = DistanceSquared;
            ClosestDefender = Defender;
        }
    }

    return ClosestDefender;
}

AActor* AEnemy::FindTargetInRange() const
{
    if (ADefender* Defender = FindClosestDefender(AttackRange))
    {
        return Defender;
    }

    if (TargetTower && FVector::Dist(GetActorLocation(), TargetTower->GetActorLocation()) <= AttackRange)
    {
        return TargetTower;
    }

    return nullptr;
}

//Handles enemy death while preserving the existing loot system
void AEnemy::HandleDeath(AActor* Killer)
{
    //Award the enemy's loot and score before destroying it
    if (ATDGameMode* GameMode = Cast<ATDGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GameMode->NotifyEnemyKilled(this);
    }

    Destroy();
}
