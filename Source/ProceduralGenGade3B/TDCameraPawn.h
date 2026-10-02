// TDCameraPawn.h
// RTS style floating camera for the tower defence view. WASD or arrows pan, Q and E rotate,
// the mouse wheel zooms. Keys are read every tick, so no project input mappings are needed.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TDCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class PROCEDURALGENGADE3B_API ATDCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ATDCameraPawn();

	/** How fast the camera pans, in Unreal units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float PanSpeed = 2500.0f;

	/** How fast Q and E turn the camera, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float RotateSpeed = 90.0f;

	/** How many degrees the camera turns per pixel of mouse movement while holding the middle mouse button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float MouseRotateSpeed = 0.35f;

	/** How much one click of the mouse wheel changes the spring arm length, in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "1.0"))
	float ZoomStep = 350.0f;

	/** The closest the camera can zoom in. This is the shortest spring arm length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "100.0"))
	float MinZoom = 900.0f;

	/** The furthest the camera can zoom out. This is the longest spring arm length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "100.0"))
	float MaxZoom = 6000.0f;

	/** How quickly the zoom catches up to its target. Higher is faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.1"))
	float ZoomInterpSpeed = 10.0f;

	/** How far the camera tilts down at the start, in degrees.
	 *  -40 gives an angled view instead of a steep top down look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float CameraPitch = -40.0f;

	/** The flattest tilt allowed when dragging with the mouse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float MinPitch = -25.0f;

	/** The steepest tilt allowed when dragging, closest to looking straight down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float MaxPitch = -70.0f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Mouse wheel handlers. They move the target zoom in or out, within the limits. */
	void ZoomIn();
	void ZoomOut();

	/** Pressing and releasing the middle mouse button starts and stops drag rotation. */
	void BeginDragRotate();
	void EndDragRotate();

private:
	/** Holds the camera up and back from the pivot. Its length is how we zoom. */
	UPROPERTY(VisibleAnywhere, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	/** The camera we actually see through. */
	UPROPERTY(VisibleAnywhere, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	/** The spring arm length we are moving towards. */
	float TargetArmLength = 3000.0f;

	/** True while the middle mouse button is held down. */
	bool bIsDragging = false;

	/** The current tilt. Dragging up and down changes it, and it stays between MaxPitch and MinPitch. */
	float CurrentPitch = -40.0f;

	/** Reads WASD, the arrows, Q and E, then pans and turns the camera for this frame. */
	void UpdateMovement(float DeltaSeconds);

	/** While dragging, sideways mouse movement turns the camera and up and down movement tilts it. */
	void UpdateDragRotation();
};
