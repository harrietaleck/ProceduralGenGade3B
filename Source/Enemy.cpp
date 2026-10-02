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
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(SphereMesh.Object);
    }

    MeshComponent->SetRelativeScale3D(Vector(0.6f));

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

        //Bear is slower but stronger.
        MoveSpeed = 75.0f;

        //Bear is the strongest enemy and deals the most damage.
        AttackDamage = 30.0f;

        ResourceReward = 35;

        //Bear has much more health than the other enemies.
        HealthComponent->MaxHealth = 250.0f;

        MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

        MeshComponent->SetRelativeScale3D(FVector(1.0f));

        ResourceType = EResourceType::ArcaneOrb;

        break;


    case EEnemyType::Wolf:

        //Wolf is faster but weaker.
        MoveSpeed = 240.0f;

        // Wolf hits harder than Basic but less than Bear.
        AttackDamage = 15.0f;

        ResourceReward = 25;

        HealthComponent->MaxHealth = 75.0f;

        MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

        MeshComponent->SetRelativeScale3D(FVector(0.45f));

        ResourceType = EResourceType::ToxicMucus;

        break;


    default:

        // Basic enemy is balanced.
        MoveSpeed = 150.0f;

        //Basic is the weakest attacker.
        AttackDamage = 10.0f;

        ResourceReward = 20;

        HealthComponent->MaxHealth = 100.0f;

        MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");

        MeshComponent->SetRelativeScale3D(FVector(0.6f));

        ResourceType = EResourceType::ArcaneOrb;

        break;
    }

    // Apply the selected mesh.
    MeshComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*MeshPath));
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
        MeshComponent->SetRelativeScale3D(SpawnTargetScale * 0.12f);

        SpawnGlow->SetLightColor( SpawnGlowColor);

        SpawnGlow->SetIntensity(SpawnGlowIntensity);

        SpawnGlow->SetVisibility(true);
    }
    else
    {
        SpawnGlow->SetVisibility(false);
    }

    // Listen for death.
    HealthComponent->OnDeath.AddDynamic(this,& AEnemy::HandleDeath);
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
    UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>();

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
    const float Distance = FVector::Dist(GetActorLocation(),Target->GetActorLocation());

    //Do not attack when the enemy is not in attack range
    if (Distance > AttackRange)
    {
        return;
    }

    //Inflict damage to the health componenet system use on the defenders
    TargetHealth->ApplyDamage(AttackDamage,this);

    //Reset the attack time
    AttackTimer = AttackInterval;
}