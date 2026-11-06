// UpgradeVisualComponent.cpp - see UpgradeVisualComponent.h.

#include "UpgradeVisualComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// Orb size for each tier, as a scale of the 100 unit engine sphere.
	constexpr float OrbScaleTier1 = 0.22f;
	constexpr float OrbScaleTier2 = 0.34f;

	// How far above the top of the unit the orb floats.
	constexpr float OrbHeightAboveTop = 55.0f;
}

UUpgradeVisualComponent::UUpgradeVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// These glow materials already exist in the project and are used for the tower crystals.
	OrbMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/Instances/MI_Glow_Gold.MI_Glow_Gold")));
	RingMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/Instances/MI_Glow_Turquoise.MI_Glow_Turquoise")));
}

UStaticMeshComponent* UUpgradeVisualComponent::GetBodyMesh() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Cast<UStaticMeshComponent>(Owner->GetRootComponent()) : nullptr;
}

void UUpgradeVisualComponent::SetTier(int32 NewTier)
{
	NewTier = FMath::Clamp(NewTier, 0, 2);
	if (NewTier == Tier && (NewTier == 0 || OrbMesh))
	{
		return;
	}
	Tier = NewTier;

	UStaticMeshComponent* Body = GetBodyMesh();
	if (!Body)
	{
		return;
	}

	if (Tier > 0)
	{
		EnsureMeshes(Body);
	}
	Layout(Body);
	ApplyBodyTint(Body);
}

void UUpgradeVisualComponent::EnsureMeshes(UStaticMeshComponent* Body)
{
	if (OrbMesh && RingMesh)
	{
		return;
	}

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	AActor* Owner = GetOwner();
	if (!Owner || !Sphere || !Cylinder)
	{
		return;
	}

	// Creates one of the glow meshes: no collision and no shadow so it can never block placement or shots.
	auto MakeMesh = [&](UStaticMesh* Mesh, const TSoftObjectPtr<UMaterialInterface>& Material) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
		Component->SetStaticMesh(Mesh);
		if (UMaterialInterface* Loaded = Material.LoadSynchronous())
		{
			Component->SetMaterial(0, Loaded);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetUsingAbsoluteScale(true); // ignore the unit's own (non-uniform) scale
		Component->SetupAttachment(Body);
		Component->RegisterComponent();
		Component->SetVisibility(false);
		return Component;
	};

	if (!OrbMesh)
	{
		OrbMesh = MakeMesh(Sphere, OrbMaterial);
	}
	if (!RingMesh)
	{
		RingMesh = MakeMesh(Cylinder, RingMaterial);
	}
}

void UUpgradeVisualComponent::Layout(UStaticMeshComponent* Body)
{
	// Measure the unit from its own mesh only. Using the whole actor would include our own orb and
	// make the unit look taller every time the tier changes.
	const FBoxSphereBounds Bounds = Body->Bounds;
	const float Top = Bounds.Origin.Z + Bounds.BoxExtent.Z;
	const float Bottom = Bounds.Origin.Z - Bounds.BoxExtent.Z;

	if (OrbMesh)
	{
		OrbMesh->SetWorldLocation(FVector(Bounds.Origin.X, Bounds.Origin.Y, Top + OrbHeightAboveTop));
		const float OrbScale = Tier >= 2 ? OrbScaleTier2 : OrbScaleTier1;
		OrbMesh->SetWorldScale3D(FVector(OrbScale));
		OrbMesh->SetVisibility(Tier >= 1);
	}

	if (RingMesh)
	{
		// A very flat disc just above the ground, wider than the unit so it reads as a ring of light.
		const float Diameter = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.8f;
		RingMesh->SetWorldLocation(FVector(Bounds.Origin.X, Bounds.Origin.Y, Bottom + 6.0f));
		RingMesh->SetWorldScale3D(FVector(Diameter / 100.0f, Diameter / 100.0f, 0.04f));
		RingMesh->SetVisibility(Tier >= 2);
	}
}

void UUpgradeVisualComponent::ApplyBodyTint(UStaticMeshComponent* Body)
{
	// Defenders give their body a dynamic material with a "Color" parameter. The tower does not,
	// so it simply gets the orb and ring.
	UMaterialInstanceDynamic* BodyMaterial = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(0));
	if (!BodyMaterial)
	{
		return;
	}

	if (!bHasBaseBodyColor)
	{
		BaseBodyColor = BodyMaterial->K2_GetVectorParameterValue(TEXT("Color"));
		bHasBaseBodyColor = true;
	}

	const float Amount = FMath::Clamp(TintPerTier * Tier, 0.0f, 1.0f);
	BodyMaterial->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(BaseBodyColor, TierTint, Amount));
}

void UUpgradeVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The meshes belong to the owner and are destroyed with it; just drop our pointers.
	OrbMesh = nullptr;
	RingMesh = nullptr;
	Super::EndPlay(EndPlayReason);
}
