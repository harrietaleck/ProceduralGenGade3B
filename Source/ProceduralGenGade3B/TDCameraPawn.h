// TDCameraPawn.h
// The player's "camera" for the tower-defence view. There is no walking character — in a
// tower defence the player is really just a floating camera looking down over the map,
// placing defenders with the mouse. This pawn gives that an RTS feel:
//
//   * WASD / arrow keys  -> pan across the map
//   * Q / E              -> rotate the view left / right
//   * Mouse wheel        -> zoom in / out
//
// The mouse is deliberately left free (the player controller shows the cursor) so clicking
// build pads keeps working while the camera moves. Movement is polled from the possessing
// controller each tick, so it needs no project-wide input mappings to be set up.

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

	/** Horizontal pan speed in Unreal units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float PanSpeed = 2500.0f;

	/** Rotation speed in degrees per second (Q/E). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float RotateSpeed = 90.0f;

	/** How far one mouse-wheel notch changes the zoom (spring-arm length), in uu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "1.0"))
	float ZoomStep = 350.0f;

	/** Closest the camera can zoom in (min spring-arm length, uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "100.0"))
	float MinZoom = 900.0f;

	/** Furthest the camera can zoom out (max spring-arm length, uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "100.0"))
	float MaxZoom = 6000.0f;

	/** How quickly the zoom eases toward its target (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.1"))
	float ZoomInterpSpeed = 10.0f;

	/** Downward tilt of the camera, in degrees (e.g. -55 looks down at the battlefield). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float CameraPitch = -55.0f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Mouse-wheel handlers: nudge the target zoom in or out (clamped). */
	void ZoomIn();
	void ZoomOut();

private:
	/** Boom that holds the camera up and back from the pivot; also drives zoom via its length. */
	UPROPERTY(VisibleAnywhere, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	/** The actual view camera. */
	UPROPERTY(VisibleAnywhere, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	/** The zoom distance we're easing toward (spring-arm target length). */
	float TargetArmLength = 3000.0f;

	/** Reads WASD/arrows/Q/E from the controller and applies pan + rotation this frame. */
	void UpdateMovement(float DeltaSeconds);
};
