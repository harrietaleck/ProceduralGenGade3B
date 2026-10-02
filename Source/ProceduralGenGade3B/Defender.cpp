// Defender.cpp
// Basic defender that shoots the nearest enemy. See Defender.h for more.
#include "Defender.h"

#include "HealthComponent.h"
#include "DamageFlashComponent.h"
#include "Enemy.h"
#include "Projectile.h"
#include "ProceduralTerrain.h"
#include "TDGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ADefender::ADefender()
{
    // Shooting runs on a timer, so we don't need Tick.
    PrimaryActorTick.bCanEverTick = false;

    // The mesh is the body and the root of the actor.
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DefenderMesh"));
    SetRootComponent(MeshComponent);

    // The Basic Defender uses a cylinder instead of a cube.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
    );

    // Use the cylinder mesh if it was found.
    if (CylinderMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(CylinderMesh.Object);
    }

    // Scale the cylinder so it looks like a small tower.
    MeshComponent->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.2f));

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Overlap);

    // Shared health component.
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
    HealthComponent->MaxHealth = 120.0f;
    CreateDefaultSubobject<UDamageFlashComponent>(TEXT("DamageFlash"));
    ProjectileClass = AProjectile::StaticClass();
    MetaCost.ForestEssence = 8;
    MetaCost.WoodenMight = 5;
    MetaCost.GemStones = 0;
    MetaCost.LightLanterns = 0;
}

float ADefender::GetThreatRating() const
{
    return FireInterval > 0.0f ? AttackDamage / FireInterval : AttackDamage;
}

void ADefender::BeginPlay()
{
    Super::BeginPlay();

    if (MeshComponent)
    {
        if (UMaterialInterface* BaseMaterial = MeshComponent->GetMaterial(0))
        {
            if (UMaterialInstanceDynamic* BodyMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this))
            {
                BodyMaterial->SetVectorParameterValue(TEXT("Color"), BodyColor);
                MeshComponent->SetMaterial(0, BodyMaterial);
            }
        }
    }

    // Remove the defender when enemies kill it.
    HealthComponent->OnDeath.AddDynamic(this, &ADefender::HandleDeath);

    //The basic defender keeps the original firing, but the archer and bomb defenders use their own behaviour
    if (bUseDefaultAttack)
    {
        // Start shooting on a fixed interval.
        GetWorldTimerManager().SetTimer(
            FireTimerHandle,
            this,
            &ADefender::FireAtNearestEnemy,
            FireInterval,
            true
        );
    }
}

void ADefender::FireAtNearestEnemy()
{
    if (HealthComponent->IsDead())
    {
        return;
    }

    AEnemy* Target = FindNearestEnemyInRange();
    if (!Target)
    {
        return;
    }

    const FVector MuzzleLocation = GetActorLocation() + MuzzleOffset;

    // Normally we fire a projectile that flies to the enemy and does damage when it hits.
    if (ProjectileClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = this;

        if (AProjectile* Shot =
            GetWorld()->SpawnActor<AProjectile>(
                ProjectileClass,
                MuzzleLocation,
                GetActorRotation(),
                SpawnParams))
        {
            Shot->InitProjectile(Target, AttackDamage, this);
            Shot->ConfigureVisuals(DefenderBallScale, DefenderBallColor);
        }

        return;
    }

    // If no projectile class is set, just do the damage straight away.
    if (UHealthComponent* TargetHealth =
        Target->FindComponentByClass<UHealthComponent>())
    {
        TargetHealth->ApplyDamage(AttackDamage, this);
    }
}

AEnemy* ADefender::FindNearestEnemyInRange() const
{
    const FVector Location = GetActorLocation();
    AEnemy* Best = nullptr;
    float BestDistSq = AttackRange * AttackRange;

    for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
    {
        AEnemy* Enemy = *It;

        UHealthComponent* EnemyHealth =
            Enemy->FindComponentByClass<UHealthComponent>();

        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }

        const float DistSq =
            FVector::DistSquared(
                Location,
                Enemy->GetActorLocation()
            );

        if (DistSq <= BestDistSq)
        {
            BestDistSq = DistSq;
            Best = Enemy;
        }
    }

    return Best;
}

void ADefender::HandleDeath(AActor* Killer)
{
    // Stop shooting and remove the actor. The build pad is freed in EndPlay,
    // because EndPlay runs no matter how the defender gets destroyed.
    GetWorldTimerManager().ClearTimer(FireTimerHandle);
    Destroy();
}

void ADefender::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Free the build pad this defender was on, whether it died or the level is closing.
    // This stops the terrain from thinking the pad is still taken.
    if (bHasOccupiedSlot)
    {
        if (ATDGameMode* GameMode =
            GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
        {
            if (AProceduralTerrain* Terrain = GameMode->GetTerrain())
            {
                Terrain->SetSlotOccupied(
                    OccupiedSlotLocation,
                    false
                );
            }
        }
    }

    Super::EndPlay(EndPlayReason);
}
