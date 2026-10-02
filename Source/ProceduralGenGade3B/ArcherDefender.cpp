#include "ArcherDefender.h"

#include "Enemy.h"
#include "HealthComponent.h"
#include "Projectile.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AArcherDefender::AArcherDefender()
{
    //Due not use the timer that is exactly like the original defender
    bUseDefaultAttack = false;

    // *** NEW: Use a cone mesh to give the Archer Defender a tall, pointed silhouette.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(
        TEXT("/Engine/BasicShapes/Cone.Cone")
    );

    // *** NEW: Replace the inherited cylinder with the archer's cone shape.
    if (ConeMesh.Succeeded() && MeshComponent)
    {
        MeshComponent->SetStaticMesh(ConeMesh.Object);
    }

    // *** NEW: Make the cone taller and narrower so it looks different from the Basic Defender.
    if (MeshComponent)
    {
        MeshComponent->SetRelativeScale3D(
            FVector(0.65f, 0.65f, 1.5f)
        );
    }

    //Set the long range attack distance
    AttackRange = 1000.0f;

    //Set the archer damage
    AttackDamage = 8.0f;

    //Set the arrow speed to slow due to a long distance to seem realistic
    FireInterval = 1.0f;

    //Set the defender projectile shape to smaller than the original
    DefenderBallScale = 0.18f;

    //Use a colour to distiguish the projectiles
    DefenderBallColor = FLinearColor(0.2f, 0.8f, 1.0f);

    //Place defender higher than normal defenders so it is realistic
    MuzzleOffset = FVector(0.0f, 0.0f, 100.0f);

    //Make the defender have 100% health
    HealthComponent->MaxHealth = 100.0f;
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

    AEnemy* Target = FindNearestEnemyForArcher();
    if (!Target)
    {
        return;
    }

    //Create the projectile from the archers muzzle
    const FVector MuzzleLocation = GetActorLocation() + MuzzleOffset;

    if (ProjectileClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = this;

        if (AProjectile* Arrow =
            GetWorld()->SpawnActor<AProjectile>(
                ProjectileClass,
                MuzzleLocation,
                GetActorRotation(),
                SpawnParams))
        {
            //The rpojectile system remains the same to attack all enemies
            Arrow->InitProjectile(
                Target,
                AttackDamage,
                this
            );

            Arrow->ConfigureVisuals(
                DefenderBallScale,
                DefenderBallColor
            );
        }
    }
    else
    {
        //When a projectile is not being selected but will do on default
        //Create target health component before applying fallback damage
        UHealthComponent* TargetHealth =
            Target->FindComponentByClass<UHealthComponent>();

        if (TargetHealth)
        {
            TargetHealth->ApplyDamage(
                AttackDamage,
                this
            );
        }
    }
}

AEnemy* AArcherDefender::FindNearestEnemyForArcher() const
{
    //Archer searches using its own long-range distance
    const FVector Location = GetActorLocation();

    AEnemy* BestEnemy = nullptr;
    float BestDistanceSquared = AttackRange * AttackRange;

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

        const float DistanceSquared =
            FVector::DistSquared(
                Location,
                Enemy->GetActorLocation());

        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestEnemy = Enemy;
        }
    }

    return BestEnemy;
}