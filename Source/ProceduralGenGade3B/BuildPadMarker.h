// BuildPadMarker.h
// A small stone platform placed on every generated defender build pad. Purely visual — it
// carries no gameplay logic — but it gives the player a clear, persistent signal of exactly
// where defenders may be placed, directly answering the brief's UI/UX question "is it clear
// where I can build?". The player controller layers a coloured hover highlight on top of
// these base markers to additionally show whether the pad the cursor is over is valid right now.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildPadMarker.generated.h"

class UStaticMeshComponent;

UCLASS()
class PROCEDURALGENGADE3B_API ABuildPadMarker : public AActor
{
	GENERATED_BODY()

public:
	ABuildPadMarker();

	/** Radius of the platform disc, in uu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildPad", meta = (ClampMin = "10.0"))
	float Radius = 90.0f;

	/** Platform thickness, in uu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildPad", meta = (ClampMin = "1.0"))
	float Thickness = 12.0f;

private:
	/** Flattened cylinder standing in for a carved stone platform / rune dais. */
	UPROPERTY(VisibleAnywhere, Category = "BuildPad", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlatformMesh;
};
