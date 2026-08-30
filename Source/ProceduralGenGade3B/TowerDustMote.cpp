// TowerDustMote.cpp — see TowerDustMote.h

#include "TowerDustMote.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATowerDustMote::ATowerDustMote()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DustMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}
	MeshComponent->SetRelativeScale3D(FVector(0.18f));

	if (UMaterialInterface* BaseMat = MeshComponent->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.5f, 0.45f));
			MeshComponent->SetMaterial(0, DynMat);
		}
	}
}

void ATowerDustMote::InitMote(const FVector& InVelocity, float Lifetime)
{
	Velocity = InVelocity;
	RemainingLife = Lifetime;
}

void ATowerDustMote::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RemainingLife -= DeltaSeconds;
	if (RemainingLife <= 0.0f)
	{
		Destroy();
		return;
	}

	Velocity.Z -= 120.0f * DeltaSeconds;
	SetActorLocation(GetActorLocation() + Velocity * DeltaSeconds);

	const float Alpha = FMath::Clamp(RemainingLife / 1.5f, 0.0f, 1.0f);
	const float Scale = FMath::Lerp(0.05f, 0.22f, Alpha);
	SetActorScale3D(FVector(Scale));
}
