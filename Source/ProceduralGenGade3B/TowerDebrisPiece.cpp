// TowerDebrisPiece.cpp - the overview is in TowerDebrisPiece.h.

#include "TowerDebrisPiece.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

// Constructor: sets the default components and values for TowerDebrisPiece.
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

// Sets the mesh, the size, and the push away from the tower centre.
void ATowerDebrisPiece::InitDebris(UStaticMesh* Mesh, const FVector& Scale, const FVector& Impulse)
{
	if (Mesh)
	{
		MeshComponent->SetStaticMesh(Mesh);
	}
	MeshComponent->SetWorldScale3D(Scale);
	PendingImpulse = Impulse;
}

// Runs when the object enters the world: sets up its state and event bindings.
void ATowerDebrisPiece::BeginPlay()
{
	Super::BeginPlay();

	if (!bImpulseApplied && !PendingImpulse.IsNearlyZero())
	{
		MeshComponent->AddImpulse(PendingImpulse, NAME_None, /*bVelChange=*/true);
		bImpulseApplied = true;
	}
}
