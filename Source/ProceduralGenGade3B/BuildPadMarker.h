// BuildPadMarker.h
// A small marker placed on every build pad so the player can always see where defenders can go.
// It is only visual and has no gameplay logic. The player controller adds a coloured
// hover highlight on top to show if the pad under the mouse can be used right now.

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

/** A basic widget in the world that shows the defender portal picture. */
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
	/** Radius of the platform, in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildPad", meta = (ClampMin = "10.0"))
	float Radius = 90.0f;

	/** How thick the platform is, in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildPad", meta = (ClampMin = "1.0"))
	float Thickness = 12.0f;

	/** The picture shown on the build pad. By default it is UI/SourceArt/DefendersPortal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildPad")
	TSoftObjectPtr<UTexture2D> PortalTexture;

private:
	UPROPERTY(VisibleAnywhere, Category = "BuildPad", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Shows the DefendersPortal picture in the world. */
	UPROPERTY(VisibleAnywhere, Category = "BuildPad", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> PortalWidgetComponent;

	/** Old disc mesh. We only show it if the portal picture can't be loaded. */
	UPROPERTY(VisibleAnywhere, Category = "BuildPad", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlatformMesh;
};
