// Enemy.cpp — see Enemy.h for the overview.

#include "Enemy.h"
#include "HealthComponent.h"
#include "DamageFlashComponent.h"
#include "Defender.h"
#include "TDGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"         // Complete UStaticMesh type for the mesh finder.
#include "EngineUtils.h"               // TActorIterator, for scanning defenders.
#include "Engine/World.h"              // LineTraceSingleByChannel, for the line-of-sight check.
#include "UObject/ConstructorHelpers.h"

// How close (uu) the enemy must get to a waypoint before it targets the next one.
static constexpr float WaypointAcceptRadius = 55.0f;

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

	// The shared health component. Explicit MaxHealth here (rather than relying on the
	// component's own default) so the Basic Enemy spec's "100 HP" is self-documenting.
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 100.0f;

	CreateDefaultSubobject<UDamageFlashComponent>(TEXT("DamageFlash"));
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
	CurrentWaypoint = 0;
	CurrentSpeed = 0.0f;

	// Do not teleport to waypoint 0 here — the spawner already places us at the spawn
	// point. MoveAlongPath will skip any waypoints we are already standing on.
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
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed - Acceleration * 2.0f * DeltaSeconds);
		TryAttack(Target, DeltaSeconds);
	}
	else
	{
		MoveAlongPath(DeltaSeconds);
	}
}

void AEnemy::MoveAlongPath(float DeltaSeconds)
{
	if (Waypoints.Num() == 0)
	{
		return;
	}

	FVector Location = GetActorLocation();
	const float AcceptRadiusSq = WaypointAcceptRadius * WaypointAcceptRadius;

	// After Chaikin smoothing the first few waypoints can sit very close together. Skip
	// any we have already reached without teleporting — snapping was the main source of the
	// jerky "stutter" right after spawn.
	while (Waypoints.IsValidIndex(CurrentWaypoint))
	{
		const FVector TargetPos = Waypoints[CurrentWaypoint] + FVector(0.0f, 0.0f, GroundClearance);
		if (FVector::DistSquared2D(Location, TargetPos) > AcceptRadiusSq)
		{
			break;
		}
		++CurrentWaypoint;
	}

	if (!Waypoints.IsValidIndex(CurrentWaypoint))
	{
		return;
	}

	const FVector TargetPos = Waypoints[CurrentWaypoint] + FVector(0.0f, 0.0f, GroundClearance);
	FVector ToTarget = TargetPos - Location;
	ToTarget.Z = 0.0f; // Paths are flat — keep movement horizontal for steady speed.
	const float Distance = ToTarget.Size();

	if (Distance <= KINDA_SMALL_NUMBER)
	{
		++CurrentWaypoint;
		return;
	}

	const FVector Direction = ToTarget / Distance;

	// Ease off before sharp corners so enemies arc through bends instead of skating at full speed.
	float TargetSpeed = MoveSpeed;
	if (Waypoints.IsValidIndex(CurrentWaypoint + 1))
	{
		FVector ToNext = Waypoints[CurrentWaypoint + 1] - Waypoints[CurrentWaypoint];
		ToNext.Z = 0.0f;
		const float NextLen = ToNext.Size();
		if (NextLen > KINDA_SMALL_NUMBER)
		{
			ToNext /= NextLen;
			const float TurnAlignment = FVector::DotProduct(Direction, ToNext); // 1 = straight, 0 = 90°
			const float TurnFactor = FMath::Lerp(0.55f, 1.0f, FMath::Clamp((TurnAlignment + 1.0f) * 0.5f, 0.0f, 1.0f));
			TargetSpeed = MoveSpeed * TurnFactor;
		}
	}

	if (CurrentSpeed < TargetSpeed)
	{
		CurrentSpeed = FMath::Min(TargetSpeed, CurrentSpeed + Acceleration * DeltaSeconds);
	}
	else
	{
		CurrentSpeed = FMath::Max(TargetSpeed, CurrentSpeed - Acceleration * DeltaSeconds);
	}
	const float Step = CurrentSpeed * DeltaSeconds;
	const float MoveAmount = FMath::Min(Step, Distance);
	FVector NewLocation = Location + Direction * MoveAmount;
	NewLocation.Z = TargetPos.Z;
	SetActorLocation(NewLocation);

	const FRotator TargetFacing(0.0f, Direction.Rotation().Yaw, 0.0f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetFacing, DeltaSeconds, TurnRate));
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

	// Priority 1: the tower, if we're close enough, it's still alive, and nothing blocks
	// the shot. Never the player character — TargetTower is the only "reach the goal"
	// target this enemy ever considers.
	if (TargetTower)
	{
		if (UHealthComponent* TowerHealth = TargetTower->FindComponentByClass<UHealthComponent>())
		{
			if (!TowerHealth->IsDead() &&
				FVector::Dist(Location, TargetTower->GetActorLocation()) <= AttackRange &&
				HasLineOfSightTo(TargetTower))
			{
				return TargetTower;
			}
		}
	}

	// Priority 2: the nearest living defender we've detected (an obstacle to clear).
	// DetectionRadius bounds the search (wider, "have we noticed it"); AttackRange plus a
	// clear line of sight is the strict gate that actually lets us stop and attack it.
	AActor* Best = nullptr;
	float BestDistSq = DetectionRadius * DetectionRadius;
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

	if (Best &&
		FVector::Dist(Location, Best->GetActorLocation()) <= AttackRange &&
		HasLineOfSightTo(Best))
	{
		return Best;
	}
	return nullptr; // Detected but not yet in strict attack range (or blocked) -> keep walking.
}

bool AEnemy::HasLineOfSightTo(const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Target);

	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, GroundClearance);
	const FVector End = Target->GetActorLocation();

	// If the trace hits anything else first (terrain, another actor) something is blocking
	// the shot -> no line of sight. "Never attack through walls."
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
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
