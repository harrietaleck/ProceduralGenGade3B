// TowerDebrisPiece.cpp — see TowerDebrisPiece.h

#include "TowerDebrisPiece.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ATowerDebrisPiece::ATowerDebrisPiece()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebrisMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeshComponent->SetLinearDamping(0.35f);
	MeshComponent->SetAngularDamping(0.45f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}

	SetLifeSpan(6.0f);
}

void ATowerDebrisPiece::InitDebris(UStaticMesh* Mesh, const FVector& Scale, const FVector& Impulse)
{
	if (Mesh)
	{
		MeshComponent->SetStaticMesh(Mesh);
	}
	MeshComponent->SetWorldScale3D(Scale);
	PendingImpulse = Impulse;
}

void ATowerDebrisPiece::BeginPlay()
{
	Super::BeginPlay();

	if (!bImpulseApplied && !PendingImpulse.IsNearlyZero())
	{
		MeshComponent->AddImpulse(PendingImpulse, NAME_None, /*bVelChange=*/true);
		bImpulseApplied = true;
	}
}
