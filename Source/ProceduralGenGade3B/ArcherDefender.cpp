#include "ArcherDefender.h"

#include "Enemy.h"
#include "HealthComponent.h"
#include "Projectile.h"
#include "Tower.h"
#include "TDGameMode.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AArcherDefender::AArcherDefender()
{
    //Don't use the original defender's timer, the archer has its own
    bUseDefaultAttack = false;
    DefenderName = TEXT("Archer");

    //Use a cone mesh to give the Archer Defender a tall, pointed silhouette
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(
        TEXT("/Engine/BasicShapes/Cone.Cone")
    );
    if (ConeMesh.Succeeded() && MeshComponent)
    {
        MeshComponent->SetStaticMesh(ConeMesh.Object);
    }

    //Make the cone taller and narrower so it looks different from the Basic Defender
    if (MeshComponent)
    {
        MeshComponent->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.5f));
    }

    //Set the long range attack distance
    AttackRange = 1000.0f;

    //Set the archer damage
    AttackDamage = 8.0f;

    //Arrows are slow to draw over a long distance
    FireInterval = 1.0f;

    //Arrows are smaller than the original projectile
    DefenderBallScale = 0.18f;

    //Use colours to distinguish the archer and its arrows
    DefenderBallColor = FLinearColor(0.2f, 0.8f, 1.0f);
    BodyColor = FLinearColor(0.15f, 0.6f, 0.95f);

    //Shots leave from the tip of the cone
    MuzzleOffset = FVector(0.0f, 0.0f, 100.0f);

    HealthComponent->MaxHealth = 100.0f;
}

float AArcherDefender::GetThreatRating() const
{
    //Average damage per second including the volley shots
    const float ArrowsPerCycle = (VolleyEveryNShots - 1) + VolleyArrowCount;
    return Super::GetThreatRating() * ArrowsPerCycle / FMath::Max(1, VolleyEveryNShots);
}

void AArcherDefender::BeginPlay()
{
    Super::BeginPlay();

    //Start the archer's firing timer
    GetWorldTimerManager().SetTimer(
        ArcherFireTimerHandle,
        this,
        &AArcherDefender::FireArrow,
        FireInterval,
        true
    );
}

void AArcherDefender::FireArrow()
{
    if (!HealthComponent || HealthComponent->IsDead())
    {
        return;
    }

    TArray<AEnemy*> Targets;
    GatherTargetsByThreat(Targets);
    if (Targets.Num() == 0)
    {
        return;
    }

    ++ShotCounter;
    const bool bVolley = VolleyEveryNShots > 0 && ShotCounter % VolleyEveryNShots == 0;
    const int32 ArrowCount = bVolley ? FMath::Min(VolleyArrowCount, Targets.Num()) : 1;

    for (int32 i = 0; i < ArrowCount; ++i)
    {
        ShootAt(Targets[i]);
    }
}

void AArcherDefender::ShootAt(AEnemy* Target)
{
    const float Damage = Target->EnemyType == EEnemyType::Wolf
        ? AttackDamage * WolfDamageMultiplier
        : AttackDamage;

    if (ProjectileClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = this;

        if (AProjectile* Arrow = GetWorld()->SpawnActor<AProjectile>(
                ProjectileClass, GetActorLocation() + MuzzleOffset, GetActorRotation(), SpawnParams))
        {
            Arrow->InitProjectile(Target, Damage, this);
            Arrow->ConfigureVisuals(DefenderBallScale, DefenderBallColor);
        }
        return;
    }

    //Fallback when no projectile class is set
    if (UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
    {
        TargetHealth->ApplyDamage(Damage, this);
    }
}

void AArcherDefender::GatherTargetsByThreat(TArray<AEnemy*>& OutTargets) const
{
    const FVector Location = GetActorLocation();
    const float RangeSq = AttackRange * AttackRange;

    for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
    {
        AEnemy* Enemy = *It;
        UHealthComponent* EnemyHealth = Enemy ? Enemy->HealthComponent.Get() : nullptr;
        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }

        if (FVector::DistSquared(Location, Enemy->GetActorLocation()) <= RangeSq)
        {
            OutTargets.Add(Enemy);
        }
    }

    //Closest to the tower first, so the archer always deals with the biggest threat
    const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
    const AActor* Tower = GameMode ? GameMode->GetTower() : nullptr;
    const FVector TowerLocation = Tower ? Tower->GetActorLocation() : Location;

    OutTargets.Sort([&TowerLocation](const AEnemy& A, const AEnemy& B)
    {
        return FVector::DistSquared2D(A.GetActorLocation(), TowerLocation)
             < FVector::DistSquared2D(B.GetActorLocation(), TowerLocation);
    });
}
