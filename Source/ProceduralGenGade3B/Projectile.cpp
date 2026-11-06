// Projectile.cpp
// Moves the projectile to its target and does damage on hit. See Projectile.h for more.

#include "Projectile.h"
#include "HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "TDVFX.h" //  visual pass

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

	//  Default visual-pass assets; all of them can be swapped in the editor.
	TrailVFX = TDVFX::Asset(TEXT("/Game/VFX/Projectiles/NS_ProjectileTrail"));
	ImpactVFX = TDVFX::Asset(TEXT("/Game/VFX/Towers/NS_TowerFire"));
	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/Instances/MI_Glow_Turquoise.MI_Glow_Turquoise")));
}

// Runs when the object enters the world: sets up its state and event bindings.
void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisuals();
	SetLifeSpan(MaxLifeSeconds);
}

// Lets each shooter change the size and colour, so tower balls can look bigger than defender ones.
void AProjectile::ConfigureVisuals(float InVisualScale, FLinearColor InColor)
{
	VisualScale = InVisualScale;
	ProjectileColor = InColor;
	ApplyVisuals();

	//  One glowing trail per projectile, in the shooter's colour. It is attached, so it is
	//  cleaned up automatically when the projectile is destroyed.
	if (!TrailComponent)
	{
		TrailComponent = TDVFX::Play(this, TrailVFX, GetActorLocation(), ProjectileColor, 0.0f, MeshComponent);
	}
	else
	{
		TrailComponent->SetVariableLinearColor(TEXT("User.Color"), ProjectileColor);
	}
}

void AProjectile::ApplyVisuals()
{
	MeshComponent->SetRelativeScale3D(ShapeScale * VisualScale); //  ShapeScale is (1,1,1) for plain orbs

	if (!CachedDynMat)
	{
		//  Use the glowing M_MagicGlow instance when it is available, otherwise the mesh's own material.
		UMaterialInterface* GlowBase = GlowMaterial.LoadSynchronous();
		UMaterialInterface* BaseMat = GlowBase ? GlowBase : MeshComponent->GetMaterial(0);
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
		CachedDynMat->SetVectorParameterValue(TEXT("GlowColor"), ProjectileColor); //  M_MagicGlow's colour parameter
	}
}

void AProjectile::ConfigureShape(UStaticMesh* InMesh, const FVector& InShapeScale, const TSoftObjectPtr<UNiagaraSystem>& ImpactOverride)
{
	if (InMesh)
	{
		MeshComponent->SetStaticMesh(InMesh); // the glow material override on slot 0 is kept
		ShapeScale = InShapeScale;
		bPointAlongFlight = true;
		const FVector ToTarget = CachedTargetLocation - GetActorLocation();
		if (!ToTarget.IsNearlyZero())
		{
			SetActorRotation(FRotationMatrix::MakeFromZ(ToTarget.GetSafeNormal()).Rotator()); // face the target from the first frame
		}
	}
	if (!ImpactOverride.IsNull())
	{
		ImpactVFX = ImpactOverride;
	}
	ApplyVisuals();
}

// Sets up the projectile with its target, its damage and who fired it, so the kill is credited.
// The shooter calls this straight after spawning it.
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
	SetActorRotation(bPointAlongFlight ? FRotationMatrix::MakeFromZ(Direction).Rotator() : Direction.Rotation()); //  arrows/spears point forward
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
	TDVFX::Play(this, ImpactVFX, GetActorLocation(), ProjectileColor); //  hit flash (pooled, one-shot)
	Destroy();
}
