// Projectile.cpp
// Moves the projectile to its target and does damage on hit. See Projectile.h for more.

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

	// Small sphere for the body and root. It is only for looks, the movement is done in code and not with physics.
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

	// Follow the target if homing is on and it is still there. Otherwise fly to the last spot we saw it.
	if (bHoming && Target)
	{
		CachedTargetLocation = Target->GetActorLocation();
	}

	const FVector Location = GetActorLocation();
	const FVector ToTarget = CachedTargetLocation - Location;
	const float Distance = ToTarget.Size();
	const float Step = Speed * DeltaSeconds;

	// If we are close enough, or would fly past it this frame, count it as a hit.
	if (Distance <= FMath::Max(Step, HitRadius))
	{
		HitTargetAndDie();
		return;
	}

	// Move towards the target and face the way we are flying.
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
