// HeroCharacter.h
// Third person hero the player walks around the map with, a bit like Dungeon Defenders.
// The camera sits high behind the hero. WASD walks, hold right mouse to turn the camera,
// and the mouse wheel zooms. The settings can be tweaked in BP_Hero.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HeroCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class PROCEDURALGENGADE3B_API AHeroCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHeroCharacter();

	// Camera settings. These can all be changed in BP_Hero or the Details panel.

	/** How far the camera is from the hero. About 600 gives a good view from high behind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "100.0"))
	float TargetArmLength = 600.0f;

	/** The closest the camera can zoom in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "100.0"))
	float MinZoom = 450.0f;

	/** The furthest the camera can zoom out. It gets bigger at runtime based on
	 *  the map size, so the whole map can still be seen after it grows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "100.0"))
	float MaxZoom = 10000.0f;

	/** The terrain width is multiplied by this to work out the max zoom at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "1.0"))
	float MapZoomOutMultiplier = 2.35f;

	/** How much one click of the mouse wheel changes the zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "1.0"))
	float ZoomStep = 300.0f;

	/** How quickly the zoom catches up to its target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "0.1"))
	float ZoomInterpSpeed = 10.0f;

	/** How far the camera tilts down at the start, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float DefaultPitch = -40.0f;

	/** The steepest tilt allowed, closest to looking straight down. A wider range helps when zoomed out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float MinPitch = -75.0f;

	/** The flattest tilt allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float MaxPitch = -25.0f;

	/** How many degrees the camera turns per pixel of mouse movement while holding right mouse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "0.0"))
	float MouseLookSpeed = 0.3f;

	/** How fast the hero walks. This also gets copied to the CharacterMovement component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 600.0f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Pressing and releasing right mouse starts and stops turning the camera. */
	void BeginLook();
	void EndLook();

	/** Mouse wheel handlers. They move the target zoom in or out, within the limits. */
	void ZoomIn();
	void ZoomOut();

private:
	/** Holds the camera high and behind the hero. Its length is the zoom. */
	UPROPERTY(VisibleAnywhere, Category = "Hero|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** The follow camera on the end of the camera boom. */
	UPROPERTY(VisibleAnywhere, Category = "Hero|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** A simple body mesh to show the hero until a proper mesh is set. */
	UPROPERTY(VisibleAnywhere, Category = "Hero", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** A small sphere head so you can tell which way the hero is facing. */
	UPROPERTY(VisibleAnywhere, Category = "Hero", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	/** True while the right mouse button is held down. */
	bool bIsLooking = false;

	/** The zoom distance we move towards each frame. */
	float TargetArm = 600.0f;

	/** The max zoom set in the editor, before the terrain size is taken into account. */
	float BaseMaxZoom = 10000.0f;

	/** Works out MaxZoom again from the current map size, since the map grows after each wave. */
	void RefreshZoomLimitsFromTerrain();

	/** Checks WASD and moves the hero based on which way the camera faces. */
	void UpdateWalk();

	/** While right mouse is held, mouse movement turns and tilts the camera. */
	void UpdateLook();

	/** Puts the hero on an open spot on the generated ground at the start. */
	void SnapToGround();

	/** Makes the hero's camera the player's view, even if a level camera tried to take over. */
	void ForceViewToSelf();
};
