// Tower.cpp — see Tower.h for the overview.

#include "Tower.h"
#include "HealthComponent.h"
#include "DamageFlashComponent.h"
#include "Enemy.h"
#include "Projectile.h"
#include "TowerDebrisPiece.h"
#include "TowerDustMote.h"
#include "TDGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "UObject/ConstructorHelpers.h"

ATower::ATower()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	SetRootComponent(MeshComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CylinderMesh.Object);
	}
	MeshComponent->SetRelativeScale3D(FVector(1.5f, 1.5f, 3.0f));
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 500.0f;

	CreateDefaultSubobject<UDamageFlashComponent>(TEXT("DamageFlash"));

	ProjectileClass = AProjectile::StaticClass();
}

void ATower::BeginPlay()
{
	Super::BeginPlay();

	HealthComponent->OnDeath.AddDynamic(this, &ATower::HandleDeath);
	GetWorldTimerManager().SetTimer(FireTimerHandle, this, &ATower::FireAtNearestEnemy, FireInterval, /*bLoop=*/true);
}

void ATower::FireAtNearestEnemy()
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

	if (ProjectileClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = this;
		if (AProjectile* Shot = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, MuzzleLocation, GetActorRotation(), SpawnParams))
		{
			Shot->InitProjectile(Target, AttackDamage, this);
			Shot->ConfigureVisuals(TowerBallScale, TowerBallColor);
		}
		return;
	}

	if (UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
	{
		TargetHealth->ApplyDamage(AttackDamage, this);
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

void ATower::PlayDestructionEffect()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GetActorLocation();
	const FVector Base = Origin + FVector(0.0f, 0.0f, 50.0f);

	static UStaticMesh* CubeMesh = nullptr;
	static UStaticMesh* SphereMesh = nullptr;
	if (!CubeMesh || !SphereMesh)
	{
		CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 I = 0; I < DebrisPieceCount; ++I)
	{
		const float Angle = (2.0f * PI * I) / DebrisPieceCount + FMath::FRandRange(-0.2f, 0.2f);
		const float Outward = FMath::FRandRange(120.0f, 420.0f);
		const FVector Offset(
			FMath::Cos(Angle) * FMath::FRandRange(40.0f, 160.0f),
			FMath::Sin(Angle) * FMath::FRandRange(40.0f, 160.0f),
			FMath::FRandRange(80.0f, 420.0f));

		const FVector SpawnLocation = Base + Offset;
		const FVector Impulse(
			FMath::Cos(Angle) * Outward,
			FMath::Sin(Angle) * Outward,
			FMath::FRandRange(250.0f, 700.0f));

		UStaticMesh* Mesh = (I % 3 == 0) ? SphereMesh : CubeMesh;
		const FVector Scale(
			FMath::FRandRange(0.18f, 0.45f),
			FMath::FRandRange(0.18f, 0.45f),
			FMath::FRandRange(0.18f, 0.55f));

		if (ATowerDebrisPiece* Debris = World->SpawnActor<ATowerDebrisPiece>(ATowerDebrisPiece::StaticClass(), SpawnLocation, FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f)), SpawnParams))
		{
			Debris->InitDebris(Mesh, Scale, Impulse);
		}
	}

	for (int32 I = 0; I < DustMoteCount; ++I)
	{
		const FVector DustOffset(
			FMath::FRandRange(-220.0f, 220.0f),
			FMath::FRandRange(-220.0f, 220.0f),
			FMath::FRandRange(20.0f, 320.0f));

		const FVector DustVelocity(
			FMath::FRandRange(-180.0f, 180.0f),
			FMath::FRandRange(-180.0f, 180.0f),
			FMath::FRandRange(120.0f, 360.0f));

		if (ATowerDustMote* Mote = World->SpawnActor<ATowerDustMote>(ATowerDustMote::StaticClass(), Base + DustOffset, FRotator::ZeroRotator, SpawnParams))
		{
			Mote->InitMote(DustVelocity, FMath::FRandRange(1.0f, 2.0f));
		}
	}

	static UParticleSystem* DustBurst = nullptr;
	if (!DustBurst)
	{
		DustBurst = LoadObject<UParticleSystem>(nullptr, TEXT("/Engine/EngineVFX/Blueprints/DefaultVFX/DefaultTexturedSmoke.DefaultTexturedSmoke"));
	}
	if (DustBurst)
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, DustBurst, Base, FRotator::ZeroRotator, FVector(2.5f), /*bAutoDestroy=*/true, EPSCPoolMethod::None, /*bAutoActivateSystem=*/true);
	}
}

void ATower::HandleDeath(AActor* Killer)
{
	GetWorldTimerManager().ClearTimer(FireTimerHandle);

	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	PlayDestructionEffect();
	SetLifeSpan(6.0f);

	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GameMode->NotifyTowerDestroyed();
	}
}
