// TerrainProp.h
// A simple decoration that AProceduralTerrain spawns, like a tree, rock or building.
// The meshes come from the terrain's scatter pools. By default these use VRS_LowPolyNatureEssentials.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerrainProp.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETerrainDecorationKind : uint8
{
	Tree     UMETA(DisplayName = "Tree"),
	Rock     UMETA(DisplayName = "Rock"),
	Building UMETA(DisplayName = "Building")
};

UCLASS()
class PROCEDURALGENGADE3B_API ATerrainProp : public AActor
{
	GENERATED_BODY()

public:
	ATerrainProp();

	void ConfigureDecoration(
		ETerrainDecorationKind InKind,
		UStaticMesh* BaseMesh,
		const FVector& BaseScale,
		float YawDegrees,
		UStaticMesh* AccentMesh = nullptr,
		const FVector& AccentRelativeLocation = FVector::ZeroVector,
		const FVector& AccentScale = FVector::OneVector);

	ETerrainDecorationKind GetDecorationKind() const { return Kind; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Decoration")
	ETerrainDecorationKind Kind = ETerrainDecorationKind::Tree;

	UPROPERTY(VisibleAnywhere, Category = "Decoration")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "Decoration")
	TObjectPtr<UStaticMeshComponent> AccentMesh;
};
