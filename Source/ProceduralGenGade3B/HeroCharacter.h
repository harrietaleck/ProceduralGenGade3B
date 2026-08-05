// HeroCharacter.h
// A Dungeon Defenders / Orcs Must Die-style playable hero: a third-person character the
// player walks around the battlefield while the tower-defence systems run. The camera is a
// tactical follow rig (Spring Arm + Camera) positioned high and behind, angled down ~40°,
// biased so the hero sits low-centre and the player sees the paths ahead.
//
// Controls: WASD walk (relative to the camera), hold Right Mouse to orbit/tilt the camera,
// mouse wheel to zoom. All feel values are exposed to Blueprint so BP_Hero can tune them.

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

	// ---- Camera feel (all editable in BP_Hero / the Details panel) ----

	/** Boom length = camera distance. ~600 gives the tactical "high behind" framing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "100.0"))
	float TargetArmLength = 600.0f;

	/** Closest zoom (min boom length). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "100.0"))
	float MinZoom = 450.0f;

	/** Furthest zoom (max boom length). Raised well past the old 900 so the player can pull
	 *  back for a wide tactical view of the whole battlefield when they want it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "100.0"))
	float MaxZoom = 3000.0f;

	/** How far one wheel notch changes zoom. Raised alongside MaxZoom so scrolling out to the
	 *  new far limit takes a reasonable number of notches rather than dozens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "1.0"))
	float ZoomStep = 150.0f;

	/** How quickly zoom eases toward its target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "0.1"))
	float ZoomInterpSpeed = 10.0f;

	/** Starting downward tilt of the camera (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float DefaultPitch = -40.0f;

	/** Steepest allowed tilt (closest to straight-down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float MinPitch = -60.0f;

	/** Shallowest allowed tilt (closest to level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float MaxPitch = -25.0f;

	/** Degrees of camera rotation per pixel of mouse movement while holding Right Mouse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Camera", meta = (ClampMin = "0.0"))
	float MouseLookSpeed = 0.3f;

	/** Ground movement speed (also pushed onto the CharacterMovement component). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 600.0f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Right-mouse press/release: enter/leave camera-orbit mode. */
	void BeginLook();
	void EndLook();

	/** Mouse-wheel handlers: nudge target zoom in/out (clamped). */
	void ZoomIn();
	void ZoomOut();

private:
	/** The follow boom: holds the camera high and behind; its length is the zoom. */
	UPROPERTY(VisibleAnywhere, Category = "Hero|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** The tactical follow camera at the end of the boom. */
	UPROPERTY(VisibleAnywhere, Category = "Hero|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Simple visible body so the hero is seen until a proper mesh is assigned. */
	UPROPERTY(VisibleAnywhere, Category = "Hero", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** A small sphere "head" purely to make facing direction readable. */
	UPROPERTY(VisibleAnywhere, Category = "Hero", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	/** True while the Right Mouse button is held (camera-orbit mode). */
	bool bIsLooking = false;

	/** Zoom distance we ease toward each frame. */
	float TargetArm = 600.0f;

	/** Poll WASD and move relative to the camera's yaw. */
	void UpdateWalk();

	/** While Right Mouse is held, turn mouse motion into camera yaw + clamped pitch. */
	void UpdateLook();

	/** Drop the hero onto an open spot on the procedurally-generated ground at start. */
	void SnapToGround();

	/** Force this hero's camera to be the player's view (beats any auto-activating level camera). */
	void ForceViewToSelf();
};
