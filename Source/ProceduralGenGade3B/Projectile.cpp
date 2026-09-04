// Projectile.cpp — see Projectile.h for the overview.

#include "Projectile.h"
#include "HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AProjectile::AProjectile()
{
	// Projectiles move every frame.
	PrimaryActorTick.bCanEverTick = true;

	// Small sphere body + root. Purely cosmetic; movement is done in code, not physics.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	SetRootComponent(MeshComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisuals();
	SetLifeSpan(MaxLifeSeconds);
}

void AProjectile::ConfigureVisuals(float InVisualScale, FLinearColor InColor)
{
	VisualScale = InVisualScale;
	ProjectileColor = InColor;
	ApplyVisuals();
}

void AProjectile::ApplyVisuals()
{
	MeshComponent->SetRelativeScale3D(FVector(VisualScale));

	if (!CachedDynMat)
	{
		UMaterialInterface* BaseMat = MeshComponent->GetMaterial(0);
		if (!BaseMat)
		{
			return;
		}

		if (UMaterialInstanceDynamic* ExistingMid = Cast<UMaterialInstanceDynamic>(BaseMat))
		{
			CachedDynMat = ExistingMid;
		}
		else
		{
			CachedDynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (CachedDynMat)
			{
				MeshComponent->SetMaterial(0, CachedDynMat);
			}
		}
	}

	if (CachedDynMat)
	{
		CachedDynMat->SetVectorParameterValue(TEXT("Color"), ProjectileColor);
	}
}

void AProjectile::InitProjectile(AActor* InTarget, float InDamage, AActor* InInstigatorActor)
{
	Target = InTarget;
	Damage = InDamage;
	InstigatorActor = InInstigatorActor;

	if (Target)
	{
		CachedTargetLocation = Target->GetActorLocation();
	}
}

void AProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Track a living, homing target; otherwise fly to the last spot we saw it.
	if (bHoming && Target)
	{
		CachedTargetLocation = Target->GetActorLocation();
	}

	const FVector Location = GetActorLocation();
	const FVector ToTarget = CachedTargetLocation - Location;
	const float Distance = ToTarget.Size();
	const float Step = Speed * DeltaSeconds;

	// Close enough (or we'd overshoot this frame) => impact.
	if (Distance <= FMath::Max(Step, HitRadius))
	{
		HitTargetAndDie();
		return;
	}

	// Advance toward the target and face the direction of travel.
	const FVector Direction = ToTarget / Distance;
	SetActorLocation(Location + Direction * Step);
	SetActorRotation(Direction.Rotation());
}

void AProjectile::HitTargetAndDie()
{
	// Only apply damage if the target still exists and is still alive.
	if (Target)
	{
		if (UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
		{
			if (!TargetHealth->IsDead())
			{
				TargetHealth->ApplyDamage(Damage, InstigatorActor);
			}
		}
	}
	Destroy();
}
