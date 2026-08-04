// HeroCharacter.cpp — see HeroCharacter.h for the overview.

#include "HeroCharacter.h"
#include "ProceduralTerrain.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

AHeroCharacter::AHeroCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// The character should face the way it moves (DD/OMD feel), not the controller's yaw.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;             // Turn toward the move direction.
		Move->RotationRate = FRotator(0.0f, 540.0f, 0.0f);  // Snappy turning.
		Move->MaxWalkSpeed = WalkSpeed;
	}

	// --- Camera boom: high behind, angled down, over-the-shoulder, collision + lag ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);           // Root is the capsule.
	CameraBoom->TargetArmLength = TargetArmLength;         // ~600 uu distance.
	CameraBoom->bUsePawnControlRotation = true;            // Boom follows the controller's look.
	CameraBoom->bDoCollisionTest = true;                  // Pull in so it never clips walls/terrain.
	CameraBoom->ProbeSize = 12.0f;
	CameraBoom->bEnableCameraLag = true;                  // Smooth follow on movement.
	CameraBoom->CameraLagSpeed = 10.0f;
	CameraBoom->bEnableCameraRotationLag = true;          // Smooth orbiting.
	CameraBoom->CameraRotationLagSpeed = 10.0f;
	// Raise + shoulder-offset the pivot so the hero sits low-centre and slightly left,
	// leaving the path ahead clearly visible.
	CameraBoom->SocketOffset = FVector(0.0f, 45.0f, 130.0f);

	// --- Follow camera on the end of the boom ---
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;        // Boom already handles rotation.
	FollowCamera->FieldOfView = 80.0f;                    // Wide tactical view.

	// --- Visible placeholder body (capsule collision stays the real collider) ---
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded())
	{
		BodyMesh->SetStaticMesh(Cyl.Object);
	}
	BodyMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.75f));
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sph.Succeeded())
	{
		HeadMesh->SetStaticMesh(Sph.Object);
	}
	HeadMesh->SetRelativeScale3D(FVector(0.45f));
	HeadMesh->SetRelativeLocation(FVector(15.0f, 0.0f, 70.0f)); // Slightly forward = a "face".
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	TargetArm = FMath::Clamp(TargetArmLength, MinZoom, MaxZoom);
	CameraBoom->TargetArmLength = TargetArm;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}

	// Point the controller's look down at DefaultPitch and clamp how far it can tilt.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator Look = PC->GetControlRotation();
		Look.Pitch = DefaultPitch;
		PC->SetControlRotation(Look);

		if (PC->PlayerCameraManager)
		{
			// Clamp pitch to [MinPitch, MaxPitch] (e.g. -60..-25).
			PC->PlayerCameraManager->ViewPitchMin = MinPitch;
			PC->PlayerCameraManager->ViewPitchMax = MaxPitch;
		}
	}

	// The map is generated at runtime, so find the ground and drop onto it.
	SnapToGround();
}

void AHeroCharacter::SnapToGround()
{
	// Prefer the terrain's central tower point as a safe, central spawn spot.
	FVector Target = GetActorLocation();
	for (TActorIterator<AProceduralTerrain> It(GetWorld()); It; ++It)
	{
		Target = It->GetTowerLocation();
		break;
	}

	// Trace straight down from high above to find the terrain surface.
	const FVector Start = FVector(Target.X + 400.0f, Target.Y + 400.0f, Target.Z + 5000.0f);
	const FVector End = Start - FVector(0.0f, 0.0f, 12000.0f);
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		SetActorLocation(Hit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 10.0f));
	}
}

void AHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Hold Right Mouse to orbit/tilt the camera around the hero.
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AHeroCharacter::BeginLook);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AHeroCharacter::EndLook);

	// Mouse wheel zooms.
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AHeroCharacter::ZoomIn);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AHeroCharacter::ZoomOut);
}

void AHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateWalk();

	if (bIsLooking)
	{
		UpdateLook();
	}

	// Ease the boom length toward the desired zoom.
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaSeconds, ZoomInterpSpeed);
}

void AHeroCharacter::UpdateWalk()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	float Forward = 0.0f;
	float Right = 0.0f;
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))    { Forward += 1.0f; }
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))  { Forward -= 1.0f; }
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) { Right += 1.0f; }
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))  { Right -= 1.0f; }

	if (Forward == 0.0f && Right == 0.0f)
	{
		return;
	}

	// Move relative to where the camera is facing (yaw only, flattened to the ground).
	const FRotator YawRot(0.0f, GetControlRotation().Yaw, 0.0f);
	const FVector Fwd = YawRot.RotateVector(FVector::ForwardVector);
	const FVector Rgt = YawRot.RotateVector(FVector::RightVector);
	AddMovementInput(Fwd, Forward);
	AddMovementInput(Rgt, Right);
}

void AHeroCharacter::UpdateLook()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PC->GetInputMouseDelta(MouseX, MouseY);

	// Horizontal drag -> orbit (yaw). Vertical drag -> tilt (pitch); the camera manager
	// clamps pitch to [MinPitch, MaxPitch] for us.
	if (MouseX != 0.0f)
	{
		PC->AddYawInput(MouseX * MouseLookSpeed);
	}
	if (MouseY != 0.0f)
	{
		PC->AddPitchInput(MouseY * MouseLookSpeed);
	}
}

void AHeroCharacter::BeginLook()
{
	bIsLooking = true;
	// Hide the cursor while orbiting so mouse motion drives the camera (and feels like a grab).
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
	}
}

void AHeroCharacter::EndLook()
{
	bIsLooking = false;
	// Restore the cursor for clicking build pads.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
	}
}

void AHeroCharacter::ZoomIn()
{
	TargetArm = FMath::Clamp(TargetArm - ZoomStep, MinZoom, MaxZoom);
}

void AHeroCharacter::ZoomOut()
{
	TargetArm = FMath::Clamp(TargetArm + ZoomStep, MinZoom, MaxZoom);
}
