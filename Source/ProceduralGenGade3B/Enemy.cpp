// Enemy.cpp

// Handles enemy movement, combat, spawning visuals and enemy variants.

#include "Enemy.h"

#include "HealthComponent.h"
#include "DamageFlashComponent.h"
#include "Defender.h"
#include "ArcherDefender.h"
#include "ProceduralTerrain.h"
#include "TDGameMode.h"
#include "WaveManager.h"
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

FString AEnemy::GetEliteName(EEliteModifier Modifier)
{
    switch (Modifier)
    {
    case EEliteModifier::Shielded:     return TEXT("Shielded");
    case EEliteModifier::Regenerating: return TEXT("Regenerating");
    case EEliteModifier::Swift:        return TEXT("Swift");
    case EEliteModifier::Splitting:    return TEXT("Splitting");
    default:                           return TEXT("None");
    }
}

void AEnemy::MakeElite(EEliteModifier Modifier)
{
    if (Modifier == EEliteModifier::None || !HealthComponent)
    {
        return;
    }

    EliteModifier = Modifier;

    HealthComponent->MaxHealth *= EliteHealthMultiplier;
    HealthComponent->Heal(HealthComponent->MaxHealth);
    ResourceReward = FMath::RoundToInt(ResourceReward * 1.6f);

    //Elites are a bit bigger so they stand out in a crowd
    SpawnTargetScale *= 1.25f;
    if (SpawnEffectElapsed >= SpawnEffectDuration)
    {
        MeshComponent->SetRelativeScale3D(SpawnTargetScale);
    }

    switch (Modifier)
    {
    case EEliteModifier::Shielded:
        ShieldHitsRemaining = ShieldHits;
        break;
    case EEliteModifier::Swift:
        MoveSpeed *= SwiftSpeedMultiplier;
        Acceleration *= 1.5f;
        break;
    default:
        break;
    }

    ShowCombatText(FString::Printf(TEXT("ELITE: %s"), *GetEliteName(Modifier)), FColor(255, 210, 60));
}

void AEnemy::ApplyPoison(float DamagePerSecond, float Duration, AActor* Source)
{
    if (!HealthComponent || HealthComponent->IsDead() || Duration <= 0.0f)
    {
        return;
    }

    if (PoisonTimeRemaining <= 0.0f || DamagePerSecond >= PoisonDamagePerSecond)
    {
        PoisonDamagePerSecond = DamagePerSecond;
        PoisonSource = Source;
    }
    PoisonTimeRemaining = FMath::Max(PoisonTimeRemaining, Duration);
}

float AEnemy::ModifyIncomingDamage(float Amount, AActor* Source)
{
    //Own poison ticks skip shields and combos so they cannot chain
    if (bApplyingStatusDamage)
    {
        return Amount;
    }

    const ADefender* Defender = Cast<ADefender>(Source);
    if (!Defender)
    {
        return Amount;
    }

    if (EliteModifier == EEliteModifier::Shielded && ShieldHitsRemaining > 0)
    {
        --ShieldHitsRemaining;
        ShowCombatText(ShieldHitsRemaining > 0 ? TEXT("BLOCKED") : TEXT("SHIELD BROKEN"), FColor(90, 170, 255));
        return 0.0f;
    }

    ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;

    //Shatter: area attacks hit a stunned enemy much harder. This is the Strong and Bomb combo
    if (IsStunned() && Defender->IsAreaAttacker())
    {
        Amount *= ShatterMultiplier;
        ShowCombatText(TEXT("SHATTER"), FColor(200, 120, 255));
        if (GameMode)
        {
            GameMode->NotifyCombo();
        }
    }

    //Venom spread: an arrow spreads the poison to every enemy nearby. This is the Bomb and Archer combo
    if (IsPoisoned() && Defender->IsA<AArcherDefender>())
    {
        SpreadVenom(const_cast<ADefender*>(Defender));
        if (GameMode)
        {
            GameMode->NotifyCombo();
        }
    }

    return Amount;
}

void AEnemy::SpreadVenom(AActor* Source)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector Origin = GetActorLocation();
    for (TActorIterator<AEnemy> It(World); It; ++It)
    {
        AEnemy* Other = *It;
        if (Other && Other->HealthComponent && !Other->HealthComponent->IsDead()
            && FVector::DistSquared2D(Origin, Other->GetActorLocation()) <= FMath::Square(VenomSpreadRadius))
        {
            Other->ApplyPoison(VenomDamagePerSecond, VenomDuration, Source);
        }
    }

    ShowCombatText(TEXT("VENOM SPREAD"), FColor(90, 255, 120));
    DrawDebugCircle(World, Origin, VenomSpreadRadius, 32, FColor(90, 255, 120), false, 0.6f, 0, 5.0f,
        FVector(1, 0, 0), FVector(0, 1, 0), false);
}

void AEnemy::ShowCombatText(const FString& Text, const FColor& Color) const
{
    if (UWorld* World = GetWorld())
    {
        DrawDebugString(World, FVector(0.0f, 0.0f, 130.0f), Text, const_cast<AEnemy*>(this), Color, 1.0f, true, 1.3f);
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
        //Bear is slow and tough and walks straight at the tower
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
    //Set the type stats before the components start so health begins at the right maximum
    //This also covers enemies placed in the level by hand instead of by the spawner
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

    //Poison and regeneration keep running while stunned
    TickStatusEffects(DeltaSeconds);
    if (!HealthComponent || HealthComponent->IsDead())
    {
        return;
    }

    if (StunTimeRemaining > 0.0f)
    {
        StunTimeRemaining -= DeltaSeconds;
        CurrentSpeed = 0.0f;
        return;
    }

    AttackTimer -= DeltaSeconds;

    if (bCanReroute)
    {
        RerouteTimer -= DeltaSeconds;
        if (RerouteTimer <= 0.0f)
        {
            RerouteTimer = RerouteCheckInterval;
            TryReroute();
        }
    }

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

//Bear: never stops for defenders, slams them on the way and gets angry when hurt
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

    //Draw a ring so the player can see the slam
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
            if (IsElite())
            {
                //Elites keep a gold aura for their whole life
                SpawnGlow->SetLightColor(FLinearColor(1.0f, 0.75f, 0.15f));
                SpawnGlow->SetIntensity(3000.0f);
            }
            else
            {
                SpawnGlow->SetVisibility(false);
            }
        }
    }
}

void AEnemy::TickStatusEffects(float DeltaSeconds)
{
    UWorld* World = GetWorld();

    if (PoisonTimeRemaining > 0.0f)
    {
        PoisonTimeRemaining -= DeltaSeconds;
        PoisonDamageAccumulated += PoisonDamagePerSecond * DeltaSeconds;
        PoisonTickTimer -= DeltaSeconds;

        if (PoisonTickTimer <= 0.0f || PoisonTimeRemaining <= 0.0f)
        {
            PoisonTickTimer = 0.5f;
            if (World)
            {
                DrawDebugCircle(World, GetActorLocation(), 70.0f, 16, FColor(90, 255, 120), false, 0.5f, 0, 3.0f,
                    FVector(1, 0, 0), FVector(0, 1, 0), false);
            }

            if (PoisonDamageAccumulated > 0.0f)
            {
                const float Damage = PoisonDamageAccumulated;
                PoisonDamageAccumulated = 0.0f;

                TGuardValue<bool> StatusGuard(bApplyingStatusDamage, true);
                HealthComponent->ApplyDamage(Damage, PoisonSource.Get());
                if (HealthComponent->IsDead())
                {
                    return;
                }
            }
        }

        if (PoisonTimeRemaining <= 0.0f)
        {
            PoisonDamagePerSecond = 0.0f;
            PoisonDamageAccumulated = 0.0f;
        }
    }

    //Poison shuts regeneration off, which gives the player a counter to Regenerating elites
    if (EliteModifier == EEliteModifier::Regenerating && !IsPoisoned())
    {
        HealthComponent->Heal(HealthComponent->MaxHealth * RegenFractionPerSecond * DeltaSeconds);
    }

    if (EliteModifier == EEliteModifier::Shielded && ShieldHitsRemaining > 0 && World)
    {
        DrawDebugSphere(World, GetActorLocation(), 85.0f * SpawnTargetScale.X + 40.0f, 12,
            FColor(90, 170, 255), false, -1.0f, 0, 2.0f);
    }
}

void AEnemy::TryReroute()
{
    if (RerouteCount >= MaxReroutes || HasReachedTower() || Waypoints.Num() == 0)
    {
        return;
    }

    ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
    const AProceduralTerrain* Terrain = GameMode ? GameMode->GetTerrain() : nullptr;
    if (!Terrain)
    {
        return;
    }

    //Grab the range and threat of every living defender once for this check
    struct FThreatSource
    {
        FVector Location;
        float RangeSq;
        float Threat;
    };
    TArray<FThreatSource> Threats;
    for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
    {
        const ADefender* Defender = *It;
        if (Defender && Defender->HealthComponent && !Defender->HealthComponent->IsDead())
        {
            Threats.Add({ Defender->GetActorLocation(), FMath::Square(Defender->AttackRange), Defender->GetThreatRating() });
        }
    }
    if (Threats.Num() == 0)
    {
        return;
    }

    const float Weight = DangerCostWeight;
    auto CellCost = [&Threats, Weight](const FVector& Point)
    {
        float Danger = 0.0f;
        for (const FThreatSource& Threat : Threats)
        {
            if (FVector::DistSquared2D(Point, Threat.Location) <= Threat.RangeSq)
            {
                Danger += Threat.Threat;
            }
        }
        return 1.0f + Danger * Weight;
    };

    TArray<FVector> Remaining;
    for (int32 i = CurrentWaypoint; i < Waypoints.Num(); ++i)
    {
        Remaining.Add(Waypoints[i]);
    }

    const float CurrentCost = EvaluateRouteCost(GetActorLocation(), Remaining, CellCost, Terrain->CellSize);

    TArray<FVector> NewRoute;
    float NewCost = 0.0f;
    if (!Terrain->FindLowestCostRoute(GetActorLocation(), CellCost, NewRoute, NewCost) || NewRoute.Num() == 0)
    {
        return;
    }

    if (NewCost > CurrentCost * RerouteImprovementRatio)
    {
        return;
    }

    Waypoints = NewRoute;
    CurrentWaypoint = 0;
    ++RerouteCount;

    //Show the new route as a trail of small dots so the player can see the enemy dodging their defence
    const FVector Lift(0.0f, 0.0f, 25.0f);
    const float DotSpacing = 70.0f;
    FVector Previous = GetActorLocation();
    if (Waypoints.Num() > 0)
    {
        Previous.Z = Waypoints[0].Z;
    }

    float NextDot = 0.0f;
    for (const FVector& Point : Waypoints)
    {
        const FVector Segment = Point - Previous;
        const float Length = Segment.Size();
        while (Length > KINDA_SMALL_NUMBER && NextDot <= Length)
        {
            const FVector Dot = Previous + Segment * (NextDot / Length) + Lift;
            DrawDebugSphere(GetWorld(), Dot, 8.0f, 8, FColor(255, 150, 40), false, 2.5f, 0, 5.0f);
            NextDot += DotSpacing;
        }
        NextDot -= Length;
        Previous = Point;
    }
    ShowCombatText(TEXT("REROUTE"), FColor(255, 150, 40));

    if (AWaveManager* WaveManager = GameMode->GetWaveManager())
    {
        WaveManager->ReportDirectorEvent(FString::Printf(TEXT("%s%s rerouted around heavy defence (route cost %.0f -> %.0f)"),
            IsElite() ? TEXT("Elite ") : TEXT(""), *GetTypeName(EnemyType), CurrentCost, NewCost));
    }
}

float AEnemy::EvaluateRouteCost(const FVector& Start, const TArray<FVector>& Points,
    TFunctionRef<float(const FVector&)> CellCost, float CellSize) const
{
    float Cost = 0.0f;
    FVector Previous = Start;
    for (const FVector& Point : Points)
    {
        const FVector Segment(Point.X - Previous.X, Point.Y - Previous.Y, 0.0f);
        const float Length = Segment.Size();
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / CellSize));
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            const FVector Sample = Previous + Segment * ((Step + 0.5f) / Steps);
            Cost += CellCost(Sample) * (Length / CellSize) / Steps;
        }
        Previous = FVector(Point.X, Point.Y, Previous.Z);
    }
    return Cost;
}

void AEnemy::SpawnSplitChildren()
{
    UWorld* World = GetWorld();
    if (!World || SplitCount <= 0)
    {
        return;
    }

    TArray<FVector> Remaining;
    for (int32 i = CurrentWaypoint; i < Waypoints.Num(); ++i)
    {
        Remaining.Add(Waypoints[i]);
    }

    const float ParentMaxHealth = HealthComponent ? HealthComponent->MaxHealth : 100.0f;
    const FVector Side = GetActorRightVector();

    for (int32 i = 0; i < SplitCount; ++i)
    {
        const float Offset = (i - (SplitCount - 1) * 0.5f) * 90.0f;
        const FTransform SpawnTransform(GetActorRotation(), GetActorLocation() + Side * Offset);

        AEnemy* Child = World->SpawnActorDeferred<AEnemy>(GetClass(), SpawnTransform, GetOwner(), nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Child)
        {
            continue;
        }

        Child->SetEnemyType(EEnemyType::Basic);
        Child->SetPath(Remaining);
        Child->SetTargetTower(TargetTower);
        Child->FinishSpawning(SpawnTransform);
        Child->ConfigureAsSplitChild(ParentMaxHealth);

        OnSplit.Broadcast(this, Child);
    }

    ShowCombatText(TEXT("SPLIT!"), FColor(255, 210, 60));
}

void AEnemy::ConfigureAsSplitChild(float ParentMaxHealth)
{
    HealthComponent->MaxHealth = FMath::Max(10.0f, ParentMaxHealth * SplitHealthFraction);
    HealthComponent->Heal(HealthComponent->MaxHealth);
    MoveSpeed *= 1.2f;
    ResourceReward = 5;
    SpawnTargetScale = FVector(0.38f);
    SpawnEffectDuration = 0.3f;
    BodyColor = FLinearColor(1.0f, 0.55f, 0.45f);
    ApplyBodyColor();
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

    //Carry on from the closest waypoint that is not too far behind where the wolf got to
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
    //Do not attack if there is no target or the attack is still cooling down
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

//Handles enemy death and keeps the normal loot drop working
void AEnemy::HandleDeath(AActor* Killer)
{
    //Give out the loot and score before the enemy is destroyed
    if (ATDGameMode* GameMode = Cast<ATDGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GameMode->NotifyEnemyKilled(this);
    }

    if (EliteModifier == EEliteModifier::Splitting)
    {
        SpawnSplitChildren();
    }

    Destroy();
}
