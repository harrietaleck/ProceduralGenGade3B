// Tower.cpp — see Tower.h for the overview.

#include "Tower.h"
#include "HealthComponent.h"
#include "DamageFlashComponent.h"
#include "Enemy.h"
#include "Projectile.h"
#include "TDGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"           // TActorIterator, for scanning enemies.
#include "DrawDebugHelpers.h"      // Visual tracer line for each shot.
#include "UObject/ConstructorHelpers.h"

ATower::ATower()
{
	// Firing is driven by a timer, so no per-frame tick is required.
	PrimaryActorTick.bCanEverTick = false;

	// Visual body + root: a cylinder scaled to look like a squat tower.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	SetRootComponent(MeshComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CylinderMesh.Object);
	}
	MeshComponent->SetRelativeScale3D(FVector(1.5f, 1.5f, 3.0f)); // Wide and tall.
	// A solid structure: blocks the player and traces (so the hero can't walk through it and
	// the camera can't clip into it). Only SetCollisionProfileName is called — calling
	// SetCollisionEnabled afterward would desync CollisionEnabled from the profile, leaving the
	// component reporting profile "Custom" instead of a deterministic "BlockAll".
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));

	// Shared health component.
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 500.0f; // Towers are tougher than units by default.

	CreateDefaultSubobject<UDamageFlashComponent>(TEXT("DamageFlash"));
}

void ATower::BeginPlay()
{
	Super::BeginPlay();

	// End the game when the tower dies.
	HealthComponent->OnDeath.AddDynamic(this, &ATower::HandleDeath);

	// Start firing on a fixed interval.
	GetWorldTimerManager().SetTimer(FireTimerHandle, this, &ATower::FireAtNearestEnemy, FireInterval, /*bLoop=*/true);
}

void ATower::FireAtNearestEnemy()
{
	// Dead towers don't shoot.
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

	// Preferred path: launch a projectile that flies to the enemy and applies damage on impact.
	if (ProjectileClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = this;
		if (AProjectile* Shot = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, MuzzleLocation, GetActorRotation(), SpawnParams))
		{
			Shot->InitProjectile(Target, AttackDamage, this);
		}
		return;
	}

	// Fallback (no projectile class set): instant hitscan damage + a debug tracer.
	if (UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
	{
		TargetHealth->ApplyDamage(AttackDamage, this);
		DrawDebugLine(GetWorld(), MuzzleLocation, Target->GetActorLocation(), FColor::Cyan, false, 0.1f, 0, 4.0f);
	}
}

AEnemy* ATower::FindNearestEnemyInRange() const
{
	const FVector Location = GetActorLocation();
	AEnemy* Best = nullptr;
	float BestDistSq = AttackRange * AttackRange;

	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		AEnemy* Enemy = *It;
		UHealthComponent* EnemyHealth = Enemy->FindComponentByClass<UHealthComponent>();
		if (!EnemyHealth || EnemyHealth->IsDead())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Location, Enemy->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Enemy;
		}
	}
	return Best;
}

void ATower::HandleDeath(AActor* Killer)
{
	// Stop firing and tell the game mode the game is over.
	GetWorldTimerManager().ClearTimer(FireTimerHandle);

	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GameMode->NotifyTowerDestroyed();
	}
}
