// Enemy.cpp — see Enemy.h for the overview.

#include "Enemy.h"
#include "HealthComponent.h"
#include "Defender.h"
#include "TDGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"         // Complete UStaticMesh type for the mesh finder.
#include "EngineUtils.h"               // TActorIterator, for scanning defenders.
#include "UObject/ConstructorHelpers.h"

// How close (uu) the enemy must get to a waypoint before it targets the next one.
static constexpr float WaypointAcceptRadius = 40.0f;

AEnemy::AEnemy()
{
	// Enemies move and fight every frame.
	PrimaryActorTick.bCanEverTick = true;

	// Visual body + root. A simple sphere is fine as a placeholder.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyMesh"));
	SetRootComponent(MeshComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}
	MeshComponent->SetRelativeScale3D(FVector(0.6f)); // ~30uu radius.
	// We move the enemy manually, so it only needs query collision (for hit tests), not physics.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Overlap);

	// The shared health component.
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	// React to our own death (reward the player and remove ourselves).
	HealthComponent->OnDeath.AddDynamic(this, &AEnemy::HandleDeath);
}

void AEnemy::SetPath(const TArray<FVector>& InWaypoints)
{
	Waypoints = InWaypoints;

	// Snap to the first waypoint and head toward the second (if any).
	if (Waypoints.Num() > 0)
	{
		SetActorLocation(Waypoints[0] + FVector(0.0f, 0.0f, GroundClearance));
	}
	CurrentWaypoint = Waypoints.Num() > 1 ? 1 : 0;
}

void AEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HealthComponent->IsDead())
	{
		return;
	}

	// If something we can hit is in range, stop and attack it; otherwise keep walking.
	if (AActor* Target = FindTargetInRange())
	{
		TryAttack(Target, DeltaSeconds);
	}
	else
	{
		MoveAlongPath(DeltaSeconds);
	}
}

void AEnemy::MoveAlongPath(float DeltaSeconds)
{
	// No more waypoints => we've arrived (and the tower, if any, is handled by attacking).
	if (!Waypoints.IsValidIndex(CurrentWaypoint))
	{
		return;
	}

	const FVector TargetPos = Waypoints[CurrentWaypoint] + FVector(0.0f, 0.0f, GroundClearance);
	const FVector Location = GetActorLocation();
	const FVector ToTarget = TargetPos - Location;
	const float Distance = ToTarget.Size();
	const float Step = MoveSpeed * DeltaSeconds;

	if (Distance <= FMath::Max(Step, WaypointAcceptRadius))
	{
		// Reached this waypoint; advance to the next one.
		SetActorLocation(TargetPos);
		++CurrentWaypoint;
	}
	else
	{
		const FVector Direction = ToTarget / Distance;
		SetActorLocation(Location + Direction * Step);

		// Face the direction of travel (ignore pitch so the sphere stays upright).
		FRotator Facing = Direction.Rotation();
		Facing.Pitch = 0.0f;
		Facing.Roll = 0.0f;
		SetActorRotation(Facing);
	}
}

void AEnemy::TryAttack(AActor* Target, float DeltaSeconds)
{
	AttackTimer -= DeltaSeconds;
	if (AttackTimer > 0.0f)
	{
		return;
	}
	AttackTimer = AttackInterval;

	// Damage the target through its health component (works for tower and defenders alike).
	if (UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
	{
		if (!TargetHealth->IsDead())
		{
			TargetHealth->ApplyDamage(AttackDamage, this);
		}
	}
}

AActor* AEnemy::FindTargetInRange() const
{
	const FVector Location = GetActorLocation();

	// Priority 1: the tower, if we're close enough and it's still alive.
	if (TargetTower)
	{
		if (UHealthComponent* TowerHealth = TargetTower->FindComponentByClass<UHealthComponent>())
		{
			if (!TowerHealth->IsDead() &&
				FVector::Dist(Location, TargetTower->GetActorLocation()) <= AttackRange)
			{
				return TargetTower;
			}
		}
	}

	// Priority 2: the nearest living defender within range (an obstacle to clear).
	AActor* Best = nullptr;
	float BestDistSq = AttackRange * AttackRange;
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		AActor* Defender = *It;
		UHealthComponent* DefenderHealth = Defender->FindComponentByClass<UHealthComponent>();
		if (!DefenderHealth || DefenderHealth->IsDead())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Location, Defender->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Defender;
		}
	}
	return Best;
}

void AEnemy::HandleDeath(AActor* Killer)
{
	// Reward the player (resource economy) via the game mode, then remove ourselves.
	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GameMode->NotifyEnemyKilled(this);
	}
	Destroy();
}
