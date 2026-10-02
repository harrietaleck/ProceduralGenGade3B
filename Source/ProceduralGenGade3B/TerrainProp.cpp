// TerrainProp.cpp — see TerrainProp.h

#include "TerrainProp.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ATerrainProp::ATerrainProp()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(Root);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseMesh->SetCastShadow(true);

	AccentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccentMesh"));
	AccentMesh->SetupAttachment(BaseMesh);
	AccentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AccentMesh->SetCastShadow(true);
	AccentMesh->SetVisibility(false);
}

void ATerrainProp::ConfigureDecoration(
	ETerrainDecorationKind InKind,
	UStaticMesh* InBaseMesh,
	const FVector& BaseScale,
	float YawDegrees,
	UStaticMesh* InAccentMesh,
	const FVector& AccentRelativeLocation,
	const FVector& AccentScale)
{
	Kind = InKind;

	if (BaseMesh && InBaseMesh)
	{
		BaseMesh->SetStaticMesh(InBaseMesh);
		BaseMesh->SetRelativeScale3D(BaseScale);
		BaseMesh->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));
	}

	if (AccentMesh)
	{
		if (InAccentMesh)
		{
			AccentMesh->SetStaticMesh(InAccentMesh);
			AccentMesh->SetRelativeLocation(AccentRelativeLocation);
			AccentMesh->SetRelativeScale3D(AccentScale);
			AccentMesh->SetVisibility(true);
		}
		else
		{
			AccentMesh->SetVisibility(false);
		}
	}
}
