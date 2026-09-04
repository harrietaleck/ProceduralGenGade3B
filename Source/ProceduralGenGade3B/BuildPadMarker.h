// BuildPadMarker.h
// A small stone platform placed on every generated defender build pad. Purely visual — it
// carries no gameplay logic — but it gives the player a clear, persistent signal of exactly
// where defenders may be placed, directly answering the brief's UI/UX question "is it clear
// where I can build?". The player controller layers a coloured hover highlight on top of
// these base markers to additionally show whether the pad the cursor is over is valid right now.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "BuildPadMarker.generated.h"

class UImage;
class USceneComponent;
class UStaticMeshComponent;
class UTexture2D;
class UWidgetComponent;

/** Minimal world-space widget used to display the defender portal texture. */
UCLASS()
class PROCEDURALGENGADE3B_API UBuildPadPortalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPortalTexture(UTexture2D* Texture);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UImage> PortalImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PendingTexture;
};

UCLASS()
class PROCEDURALGENGADE3B_API ABuildPadMarker : public AActor
{
	GENERATED_BODY()

public:
	ABuildPadMarker();
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

public:
	/** Radius of the platform disc, in uu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildPad", meta = (ClampMin = "10.0"))
	float Radius = 90.0f;

	/** Platform thickness, in uu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildPad", meta = (ClampMin = "1.0"))
	float Thickness = 12.0f;

	/** Texture shown on the build pad. Defaults to UI/SourceArt/DefendersPortal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildPad")
	TSoftObjectPtr<UTexture2D> PortalTexture;

private:
	UPROPERTY(VisibleAnywhere, Category = "BuildPad", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** World-space image displaying DefendersPortal. */
	UPROPERTY(VisibleAnywhere, Category = "BuildPad", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> PortalWidgetComponent;

	/** Legacy disc used only when the portal texture is unavailable. */
	UPROPERTY(VisibleAnywhere, Category = "BuildPad", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlatformMesh;
};
