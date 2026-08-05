// Defender.cpp — see Defender.h for the overview.

#include "Defender.h"
#include "HealthComponent.h"
#include "Enemy.h"
#include "Projectile.h"
#include "ProceduralTerrain.h"
#include "TDGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

ADefender::ADefender()
{
	// Firing is timer-driven, so no per-frame tick.
	PrimaryActorTick.bCanEverTick = false;

	// Visual body + root: a cube scaled into a small turret block.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DefenderMesh"));
	SetRootComponent(MeshComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}
	MeshComponent->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.2f));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Overlap);

	// Shared health component.
	// NOTE: temporarily lowered from 120 to 3 for easy manual testing (defenders die in one
	// enemy hit) — restore to a real balance value once testing is done.
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 3.0f;
}

void ADefender::BeginPlay()
{
	Super::BeginPlay();

	// Remove ourselves when destroyed by enemies.
	HealthComponent->OnDeath.AddDynamic(this, &ADefender::HandleDeath);

	// Begin firing on a fixed interval.
	GetWorldTimerManager().SetTimer(FireTimerHandle, this, &ADefender::FireAtNearestEnemy, FireInterval, /*bLoop=*/true);
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
		DrawDebugLine(GetWorld(), MuzzleLocation, Target->GetActorLocation(), FColor::Yellow, false, 0.1f, 0, 3.0f);
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

void ADefender::HandleDeath(AActor* Killer)
{
	// Stop firing and remove the actor. Freeing its build slot happens in EndPlay, which fires
	// for every destruction path (not just this one), so occupancy can never go stale.
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
	Destroy();
}

void ADefender::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Free the build slot this defender occupied, however it's being destroyed (death, level
	// teardown, etc.), so the terrain's persisted occupancy state never goes stale.
	if (bHasOccupiedSlot)
	{
		if (ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
		{
			if (AProceduralTerrain* Terrain = GameMode->GetTerrain())
			{
				Terrain->SetSlotOccupied(OccupiedSlotLocation, false);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}
