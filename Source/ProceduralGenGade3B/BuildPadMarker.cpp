// BuildPadMarker.cpp — see BuildPadMarker.h for the overview.

#include "BuildPadMarker.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ABuildPadMarker::ABuildPadMarker()
{
	PrimaryActorTick.bCanEverTick = false; // Purely decorative; no per-frame logic of its own.

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	SetRootComponent(PlatformMesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PlatformMesh->SetStaticMesh(CylinderMesh.Object);
	}

	// The basic cylinder is ~100uu across by default; scale it into a flat disc-shaped dais.
	PlatformMesh->SetRelativeScale3D(FVector(Radius / 50.0f, Radius / 50.0f, Thickness / 100.0f));
	// Purely visual: never blocks clicks, movement, or the build-slot raycast.
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
