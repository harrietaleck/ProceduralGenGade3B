// TDCameraPawn.cpp — see TDCameraPawn.h for the overview.

#include "TDCameraPawn.h"
#include "ProceduralTerrain.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

ATDCameraPawn::ATDCameraPawn()
{
	// We poll input and ease the zoom every frame.
	PrimaryActorTick.bCanEverTick = true;

	// Bare scene root acts as the pivot the camera orbits/pans around.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Spring arm holds the camera up and back; its length is our zoom control.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = TargetArmLength;
	SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = true;    // Shorten the boom so the lens never clips terrain.
	SpringArm->ProbeSize = 12.0f;          // Collision probe radius for that test.
	SpringArm->bEnableCameraLag = true;    // Smooth, weighty movement.
	SpringArm->CameraLagSpeed = 10.0f;
	SpringArm->bEnableCameraRotationLag = true; // Ease rotation too, so turns glide.
	SpringArm->CameraRotationLagSpeed = 10.0f;

	// The view camera on the end of the boom.
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void ATDCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// Apply the (possibly designer-tuned) pitch and starting zoom.
	CurrentPitch = CameraPitch;
	SpringArm->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	TargetArmLength = FMath::Clamp(TargetArmLength, MinZoom, MaxZoom);
	SpringArm->TargetArmLength = TargetArmLength;

	// Centre the view over the terrain's tower point so the player starts looking at the map.
	for (TActorIterator<AProceduralTerrain> It(GetWorld()); It; ++It)
	{
		const FVector Focus = It->GetTowerLocation();
		SetActorLocation(FVector(Focus.X, Focus.Y, Focus.Z + 200.0f));
		break;
	}
}

void ATDCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Zoom is event-driven (the wheel fires discrete notches); pan/rotate are polled in Tick.
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ATDCameraPawn::ZoomIn);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ATDCameraPawn::ZoomOut);

	// Hold the middle mouse button to freely rotate the view around the battlefield.
	PlayerInputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ATDCameraPawn::BeginDragRotate);
	PlayerInputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &ATDCameraPawn::EndDragRotate);
}

void ATDCameraPawn::BeginDragRotate()
{
	bIsDragging = true;
}

void ATDCameraPawn::EndDragRotate()
{
	bIsDragging = false;
}

void ATDCameraPawn::UpdateDragRotation()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Raw mouse delta since last frame.
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PC->GetInputMouseDelta(MouseX, MouseY);

	// Horizontal drag -> yaw the whole rig around the battlefield.
	if (MouseX != 0.0f)
	{
		FRotator NewRot = GetActorRotation();
		NewRot.Yaw += MouseX * MouseRotateSpeed;
		SetActorRotation(NewRot);
	}

	// Vertical drag -> tilt the boom, clamped between MaxPitch (steep) and MinPitch (shallow).
	if (MouseY != 0.0f)
	{
		CurrentPitch = FMath::Clamp(CurrentPitch + MouseY * MouseRotateSpeed, MaxPitch, MinPitch);
		SpringArm->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	}
}

void ATDCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateMovement(DeltaSeconds);

	// Free-rotate the view while the middle mouse button is held.
	if (bIsDragging)
	{
		UpdateDragRotation();
	}

	// Ease the spring-arm length toward the desired zoom for a smooth feel.
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArmLength, DeltaSeconds, ZoomInterpSpeed);
}

void ATDCameraPawn::UpdateMovement(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// --- Pan (WASD + arrow keys) ---
	float Forward = 0.0f;
	float Right = 0.0f;
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))    { Forward += 1.0f; }
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))  { Forward -= 1.0f; }
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) { Right += 1.0f; }
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))  { Right -= 1.0f; }

	if (Forward != 0.0f || Right != 0.0f)
	{
		// Pan on the ground plane relative to the current view yaw (ignore pitch).
		const FRotator YawOnly(0.0f, GetActorRotation().Yaw, 0.0f);
		const FVector Dir = YawOnly.RotateVector(FVector(Forward, Right, 0.0f)).GetSafeNormal();
		AddActorWorldOffset(Dir * PanSpeed * DeltaSeconds, /*bSweep=*/false);
	}

	// --- Rotate (Q/E) ---
	float Turn = 0.0f;
	if (PC->IsInputKeyDown(EKeys::E)) { Turn += 1.0f; }
	if (PC->IsInputKeyDown(EKeys::Q)) { Turn -= 1.0f; }
	if (Turn != 0.0f)
	{
		FRotator NewRot = GetActorRotation();
		NewRot.Yaw += Turn * RotateSpeed * DeltaSeconds;
		SetActorRotation(NewRot);
	}
}

void ATDCameraPawn::ZoomIn()
{
	TargetArmLength = FMath::Clamp(TargetArmLength - ZoomStep, MinZoom, MaxZoom);
}

void ATDCameraPawn::ZoomOut()
{
	TargetArmLength = FMath::Clamp(TargetArmLength + ZoomStep, MinZoom, MaxZoom);
}
