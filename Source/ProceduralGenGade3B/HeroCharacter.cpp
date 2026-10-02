// HeroCharacter.cpp - the overview is in HeroCharacter.h.

#include "HeroCharacter.h"
#include "ProceduralTerrain.h"
#include "TDGameMode.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AHeroCharacter::AHeroCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// The hero faces the way it walks, not the way the controller is pointing.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;             // Turn to face the way we move.
		Move->RotationRate = FRotator(0.0f, 540.0f, 0.0f);  // Quick turning.
		Move->MaxWalkSpeed = WalkSpeed;
	}

	// Camera boom. It sits high behind the hero and looks down over the shoulder.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);           // The root is the capsule.
	CameraBoom->TargetArmLength = TargetArmLength;         // About 600 units away.
	CameraBoom->bUsePawnControlRotation = true;            // The boom turns with the controller.
	CameraBoom->bDoCollisionTest = false;                 // Off because hills kept pulling the camera in and stopping zoom out.
	CameraBoom->ProbeSize = 12.0f;
	CameraBoom->bEnableCameraLag = true;                  // Smooth follow when moving.
	CameraBoom->CameraLagSpeed = 10.0f;
	CameraBoom->bEnableCameraRotationLag = true;          // Smooth turning around the hero.
	CameraBoom->CameraRotationLagSpeed = 10.0f;
	// Lift the pivot up to about head height, so the camera swings around the eyes
	// and not the feet.
	CameraBoom->TargetOffset = FVector(0.0f, 0.0f, 80.0f);
	// Then move the camera a little to the side, after rotation, so the hero is
	// slightly off-centre on screen.
	CameraBoom->SocketOffset = FVector(0.0f, 40.0f, 20.0f);

	// Follow camera on the end of the boom.
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;        // The boom already does the turning.
	FollowCamera->FieldOfView = 80.0f;                    // A bit wider so more of the map is visible.

	// These meshes stay so old Blueprints still work, but the hero is hidden.
	// The capsule still handles movement and collision, and the camera stays attached.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetHiddenInGame(true);
	BodyMesh->SetVisibility(false);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(RootComponent);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetHiddenInGame(true);
	HeadMesh->SetVisibility(false);
}

void AHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	BaseMaxZoom = MaxZoom;
	RefreshZoomLimitsFromTerrain();

	TargetArm = FMath::Clamp(TargetArmLength, MinZoom, MaxZoom);
	CameraBoom->TargetArmLength = TargetArm;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}

	// Tilt the controller down to DefaultPitch and limit how far it can tilt.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator Look = PC->GetControlRotation();
		Look.Pitch = DefaultPitch;
		PC->SetControlRotation(Look);

		if (PC->PlayerCameraManager)
		{
			// Keep the pitch between MinPitch and MaxPitch.
			PC->PlayerCameraManager->ViewPitchMin = MinPitch;
			PC->PlayerCameraManager->ViewPitchMax = MaxPitch;
		}
	}

	// The terrain builds itself in its own BeginPlay, which might run after this one.
	// So we wait a moment to make sure the paths and ground exist before we trace.
	FTimerHandle PlaceTimer;
	GetWorldTimerManager().SetTimer(PlaceTimer, this, &AHeroCharacter::SnapToGround, 0.2f, false);

	// An old camera in the level can take over the view. Once everything has run BeginPlay,
	// we switch back to the hero's camera with a short blend.
	FTimerHandle ViewTimer;
	GetWorldTimerManager().SetTimer(ViewTimer, this, &AHeroCharacter::ForceViewToSelf, 0.4f, false);
}

void AHeroCharacter::ForceViewToSelf()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetViewTargetWithBlend(this, 0.4f);
	}
}

void AHeroCharacter::SnapToGround()
{
	// Pick an open spot partway along a flat enemy path, so the hero has room and the
	// camera isn't stuck against the tower or a hill. If there are no paths, use the tower point.
	FVector Target = GetActorLocation();
	float FaceYaw = 0.0f;
	for (TActorIterator<AProceduralTerrain> It(GetWorld()); It; ++It)
	{
		const FVector TowerLoc = It->GetTowerLocation();
		Target = TowerLoc;

		const TArray<FEnemyPath>& Paths = It->GetEnemyPaths();
		if (Paths.Num() > 0 && Paths[0].Waypoints.Num() > 1)
		{
			// Stand halfway along the first path, out in the open.
			const TArray<FVector>& WP = Paths[0].Waypoints;
			Target = WP[FMath::Clamp(WP.Num() / 2, 0, WP.Num() - 1)];
			// Face out along the path towards the enemy spawn, so enemies come from
			// in front and the tower is behind the camera.
			FaceYaw = (Paths[0].SpawnPoint - TowerLoc).Rotation().Yaw;
		}
		break;
	}

	// Trace straight down from high up to find the ground at that spot.
	const FVector Start = FVector(Target.X, Target.Y, Target.Z + 5000.0f);
	const FVector End = Start - FVector(0.0f, 0.0f, 12000.0f);
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		SetActorLocation(Hit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 10.0f));
	}

	// Turn the hero to FaceYaw and put the camera behind it.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator Look = PC->GetControlRotation();
		Look.Yaw = FaceYaw;
		Look.Pitch = DefaultPitch;
		PC->SetControlRotation(Look);
	}
	SetActorRotation(FRotator(0.0f, FaceYaw, 0.0f));
}

void AHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Hold right mouse to turn and tilt the camera around the hero.
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

	// Slowly move the boom length towards the target zoom.
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaSeconds, ZoomInterpSpeed);
}

void AHeroCharacter::UpdateWalk()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Don't walk while a pause, results or game over menu is open.
	if (const ATDGameMode* GameMode = PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
	{
		if (GameMode->IsInteractionBlocked())
		{
			return;
		}
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

	// Move based on which way the camera faces, flat along the ground.
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

	// Moving the mouse sideways turns the camera, and up and down tilts it.
	// The camera manager keeps the tilt between MinPitch and MaxPitch for us.
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
	// Hide the cursor while turning so the mouse only moves the camera.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
	}
}

void AHeroCharacter::EndLook()
{
	bIsLooking = false;
	// Show the cursor again so we can click build pads.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
	}
}

void AHeroCharacter::ZoomIn()
{
	RefreshZoomLimitsFromTerrain();
	TargetArm = FMath::Clamp(TargetArm - ZoomStep, MinZoom, MaxZoom);
}

void AHeroCharacter::ZoomOut()
{
	RefreshZoomLimitsFromTerrain();
	TargetArm = FMath::Clamp(TargetArm + ZoomStep, MinZoom, MaxZoom);
}

void AHeroCharacter::RefreshZoomLimitsFromTerrain()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AProceduralTerrain> It(GetWorld()); It; ++It)
	{
		const AProceduralTerrain* Terrain = *It;
		if (!Terrain)
		{
			continue;
		}

		const float MapWorldSize = Terrain->GridSize * Terrain->CellSize;
		const float TerrainScaledMax = MapWorldSize * MapZoomOutMultiplier;
		const float ExpansionCap = Terrain->MaxGridSize * Terrain->CellSize * MapZoomOutMultiplier;
		MaxZoom = FMath::Max(BaseMaxZoom, TerrainScaledMax, ExpansionCap);
		TargetArm = FMath::Clamp(TargetArm, MinZoom, MaxZoom);
		return;
	}
}
