// Defender.cpp — see Defender.h for the overview.

#include "Defender.h"
#include "HealthComponent.h"
#include "Enemy.h"
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
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 120.0f;
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

	if (UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
	{
		TargetHealth->ApplyDamage(AttackDamage, this);

		// Tracer from the top of the defender to the enemy for visual feedback.
		const FVector MuzzleLocation = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
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
	// Stop firing and remove the actor. Its build slot becomes free automatically because
	// the player controller treats a slot as occupied only while a defender stands on it.
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
	Destroy();
}
