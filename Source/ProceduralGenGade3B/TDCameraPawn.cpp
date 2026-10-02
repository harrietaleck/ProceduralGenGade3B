// TDCameraPawn.cpp - the overview is in TDCameraPawn.h.

#include "TDCameraPawn.h"
#include "ProceduralTerrain.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

ATDCameraPawn::ATDCameraPawn()
{
	// We check input and smooth the zoom every frame.
	PrimaryActorTick.bCanEverTick = true;

	// An empty scene root. The camera pans and turns around this point.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// The spring arm holds the camera up and back. Changing its length is how we zoom.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = TargetArmLength;
	SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = true;    // Pulls the camera in so it doesn't go inside the terrain.
	SpringArm->ProbeSize = 12.0f;          // Size of the sphere used for that check.
	SpringArm->bEnableCameraLag = true;    // Makes movement feel smooth.
	SpringArm->CameraLagSpeed = 10.0f;
	SpringArm->bEnableCameraRotationLag = true; // Smooth the turning as well.
	SpringArm->CameraRotationLagSpeed = 10.0f;

	// The camera sits on the end of the spring arm.
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void ATDCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// Use the pitch and starting zoom set in the editor.
	CurrentPitch = CameraPitch;
	SpringArm->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	TargetArmLength = FMath::Clamp(TargetArmLength, MinZoom, MaxZoom);
	SpringArm->TargetArmLength = TargetArmLength;

	// Start the camera over the tower so the player can see the map straight away.
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

	// Zoom uses events because the wheel moves in clicks. Panning and turning are checked in Tick.
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ATDCameraPawn::ZoomIn);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ATDCameraPawn::ZoomOut);

	// Hold the middle mouse button to turn the view around the map.
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

	// How much the mouse moved since last frame.
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PC->GetInputMouseDelta(MouseX, MouseY);

	// Dragging sideways turns the whole camera around the map.
	if (MouseX != 0.0f)
	{
		FRotator NewRot = GetActorRotation();
		NewRot.Yaw += MouseX * MouseRotateSpeed;
		SetActorRotation(NewRot);
	}

	// Dragging up or down tilts the arm. It stays between MaxPitch (steep) and MinPitch (flat).
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

	// Turn the view while the middle mouse button is held.
	if (bIsDragging)
	{
		UpdateDragRotation();
	}

	// Slowly move the spring arm length towards the target zoom so it feels smooth.
	SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, TargetArmLength, DeltaSeconds, ZoomInterpSpeed);
}

void ATDCameraPawn::UpdateMovement(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Panning with WASD or the arrow keys
	float Forward = 0.0f;
	float Right = 0.0f;
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))    { Forward += 1.0f; }
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))  { Forward -= 1.0f; }
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) { Right += 1.0f; }
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))  { Right -= 1.0f; }

	if (Forward != 0.0f || Right != 0.0f)
	{
		// Move along the ground based on which way the camera faces. Tilt is ignored.
		const FRotator YawOnly(0.0f, GetActorRotation().Yaw, 0.0f);
		const FVector Dir = YawOnly.RotateVector(FVector(Forward, Right, 0.0f)).GetSafeNormal();
		AddActorWorldOffset(Dir * PanSpeed * DeltaSeconds, /*bSweep=*/false);
	}

	// Turning with Q and E
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
