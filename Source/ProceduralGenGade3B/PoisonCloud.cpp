// PoisonCloud.cpp. See PoisonCloud.h for an overview.

#include "PoisonCloud.h"
#include "Defender.h"
#include "HealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// Constructor: sets the default components and values for PoisonCloud.
APoisonCloud::APoisonCloud()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

// Cell world location.
FVector APoisonCloud::CellWorldLocation(int32 X, int32 Y) const
{
	const float Half = (GridSize - 1) * 0.5f;
	return GetActorLocation() + FVector((X - Half) * CellSize, (Y - Half) * CellSize, 0.0f);
}

// Runs when the object enters the world: sets up its state and event bindings.
void APoisonCloud::BeginPlay()
{
	Super::BeginPlay();

	Random.Initialize(FMath::RandRange(1, 100000));
	Age.Init(0, GridSize * GridSize);

	UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Base)
	{
		YoungMaterial = UMaterialInstanceDynamic::Create(Base, this);
		OldMaterial = UMaterialInstanceDynamic::Create(Base, this);
		YoungMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 1.0f, 0.3f));
		OldMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.45f, 0.12f));
	}

	// One flat green disc per cell, laid on the ground. Hidden until its cell is infected.
	const float Scale = CellSize / 100.0f * 0.92f;
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			UStaticMeshComponent* Tile = NewObject<UStaticMeshComponent>(this);
			Tile->SetStaticMesh(Disc);
			Tile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Tile->SetCastShadow(false);
			Tile->SetMobility(EComponentMobility::Movable);
			Tile->SetWorldScale3D(FVector(Scale, Scale, 0.04f));

			FVector Location = CellWorldLocation(X, Y);
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 300), Location - FVector(0, 0, 800), ECC_Visibility))
			{
				Location.Z = Hit.ImpactPoint.Z + 6.0f;
			}
			else
			{
				Location.Z -= 50.0f;
			}
			Tile->SetWorldLocation(Location);
			Tile->SetVisibility(false);
			Tile->RegisterComponentWithWorld(GetWorld());
			Tiles.Add(Tile);
		}
	}

	// Seed the automaton: the centre cell plus one random neighbour.
	const int32 Mid = GridSize / 2;
	Age[Index(Mid, Mid)] = 1;
	Age[Index(Mid + Random.RandRange(-1, 1), Mid + Random.RandRange(-1, 1))] = 1;

	RefreshVisuals();
	DamageDefenders();
	GetWorldTimerManager().SetTimer(StepHandle, this, &APoisonCloud::Step, StepInterval, true);
}

// Step.
void APoisonCloud::Step()
{
	TArray<uint8> Next = Age;
	bool bAnyInfected = false;

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			const uint8 Current = Age[Index(X, Y)];
			if (Current > 0)
			{
				// Infected cells age, then burn out after CellLifetime steps.
				Next[Index(X, Y)] = (Current + 1 > CellLifetime) ? 0 : Current + 1;
			}
			else
			{
				// A clean cell next to young infected cells may catch the poison.
				int32 YoungNeighbours = 0;
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						const int32 NX = X + DX;
						const int32 NY = Y + DY;
						if ((DX == 0 && DY == 0) || NX < 0 || NY < 0 || NX >= GridSize || NY >= GridSize)
						{
							continue;
						}
						const uint8 NeighbourAge = Age[Index(NX, NY)];
						if (NeighbourAge > 0 && NeighbourAge <= 3)
						{
							++YoungNeighbours;
						}
					}
				}
				if (YoungNeighbours > 0 && Random.FRand() < FMath::Min(0.85f, SpreadChance * YoungNeighbours))
				{
					Next[Index(X, Y)] = 1;
				}
			}
			bAnyInfected |= Next[Index(X, Y)] > 0;
		}
	}

	Age = Next;
	++StepCount;
	RefreshVisuals();
	DamageDefenders();

	if (!bAnyInfected)
	{
		Destroy();
	}
}

// Refresh visuals.
void APoisonCloud::RefreshVisuals()
{
	for (int32 I = 0; I < Tiles.Num(); ++I)
	{
		const bool bInfected = Age[I] > 0;
		Tiles[I]->SetVisibility(bInfected);
		if (bInfected)
		{
			Tiles[I]->SetMaterial(0, Age[I] <= 3 ? YoungMaterial.Get() : OldMaterial.Get());
		}
	}
}

// Damage defenders.
void APoisonCloud::DamageDefenders()
{
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		ADefender* Defender = *It;
		UHealthComponent* Health = Defender ? Defender->HealthComponent : nullptr;
		if (!Health || Health->IsDead())
		{
			continue;
		}
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			for (int32 X = 0; X < GridSize; ++X)
			{
				if (Age[Index(X, Y)] > 0 && FVector::Dist2D(CellWorldLocation(X, Y), Defender->GetActorLocation()) < CellSize * 0.75f)
				{
					Health->ApplyDamage(DamagePerStep, this); // one hit per step, however many cells overlap
					X = GridSize;
					break;
				}
			}
		}
	}
}
