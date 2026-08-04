// TDPlayerController.cpp — see TDPlayerController.h for the overview.

#include "TDPlayerController.h"
#include "Defender.h"
#include "ProceduralTerrain.h"
#include "TDGameMode.h"
#include "TDHUD.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

ATDPlayerController::ATDPlayerController()
{
	// Default to the plain C++ defender unless overridden.
	DefenderClass = ADefender::StaticClass();

	// Ticks every frame to draw the build-pad hover highlight.
	PrimaryActorTick.bCanEverTick = true;
}

void ATDPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Show the cursor and allow click hit-tests against the world.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ATDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Bind left mouse button directly (simple and sufficient for Part 1 placement).
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ATDPlayerController::OnPlaceDefenderClicked);

	// Bind R to restart (only acts once the game is over — see OnRestartPressed).
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ATDPlayerController::OnRestartPressed);
}

void ATDPlayerController::OnRestartPressed()
{
	// Only allow a restart after the game has ended, so R can't be spammed mid-match.
	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		if (GameMode->IsGameOver())
		{
			GameMode->RestartGame();
		}
	}
}

void ATDPlayerController::OnPlaceDefenderClicked()
{
	// No placing once the game is over.
	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode || GameMode->IsGameOver() || !DefenderClass)
	{
		return;
	}

	FVector SlotLocation;
	if (!FindNearestSlotUnderCursor(SlotLocation))
	{
		return; // Clicked away from any slot.
	}

	// Can't stack two defenders on one slot.
	if (IsSlotOccupied(SlotLocation))
	{
		return;
	}

	// Check affordability using the defender's own Cost, but don't spend yet — Loot is only
	// ever deducted for a placement that actually happens (see below).
	const int32 Cost = DefenderClass.GetDefaultObject()->Cost;
	if (GameMode->GetResources() < Cost)
	{
		// Reject the placement and tell the player why, without touching their Loot.
		if (ATDHUD* HUD = Cast<ATDHUD>(GetHUD()))
		{
			HUD->ShowInsufficientFundsMessage();
		}
		return;
	}

	// Spawn the defender on the slot, raised so its base rests on the ground.
	// (Named DefenderSpawnLocation to avoid hiding the inherited APlayerController::SpawnLocation.)
	const FVector DefenderSpawnLocation = SlotLocation + FVector(0.0f, 0.0f, 60.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADefender* NewDefender = GetWorld()->SpawnActor<ADefender>(DefenderClass, DefenderSpawnLocation, FRotator::ZeroRotator, SpawnParams);

	// Only pay for a placement that actually succeeded.
	if (NewDefender)
	{
		GameMode->TrySpendResources(Cost);
	}
}

bool ATDPlayerController::IsSlotOccupied(const FVector& SlotLocation) const
{
	const float RadiusSq = SlotOccupiedRadius * SlotOccupiedRadius;
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		const FVector Delta(It->GetActorLocation().X - SlotLocation.X,
		                    It->GetActorLocation().Y - SlotLocation.Y, 0.0f);
		if (Delta.SizeSquared() <= RadiusSq)
		{
			return true;
		}
	}
	return false;
}

bool ATDPlayerController::FindNearestSlotUnderCursor(FVector& OutSlotLocation) const
{
	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	AProceduralTerrain* Terrain = GameMode ? GameMode->GetTerrain() : nullptr;
	if (!Terrain)
	{
		return false;
	}

	// Trace under the cursor to find where on the world the player is pointing.
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex=*/false, Hit))
	{
		return false;
	}
	const FVector CursorLocation = Hit.Location;

	// Snap to the nearest buildable slot (compared on the ground plane, ignoring Z).
	const TArray<FVector>& Slots = Terrain->GetDefenderSlots();
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = SlotClickTolerance * SlotClickTolerance;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FVector Delta(CursorLocation.X - Slots[i].X, CursorLocation.Y - Slots[i].Y, 0.0f);
		const float DistSq = Delta.SizeSquared();
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = i;
		}
	}

	if (BestIndex == INDEX_NONE)
	{
		return false;
	}
	OutSlotLocation = Slots[BestIndex];
	return true;
}

void ATDPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBuildPadHighlight();
}

void ATDPlayerController::UpdateBuildPadHighlight() const
{
	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode || GameMode->IsGameOver() || !DefenderClass)
	{
		return;
	}

	FVector SlotLocation;
	if (!FindNearestSlotUnderCursor(SlotLocation))
	{
		return; // Cursor isn't near any build pad right now -> no highlight to draw.
	}

	// Valid = free slot AND the player can currently afford this defender.
	const bool bOccupied = IsSlotOccupied(SlotLocation);
	const int32 Cost = DefenderClass.GetDefaultObject()->Cost;
	const bool bAffordable = GameMode->GetResources() >= Cost;
	const bool bValid = !bOccupied && bAffordable;

	const FColor HighlightColor = bValid ? FColor(60, 220, 90, 140) : FColor(220, 60, 60, 140);

	// A flat, short-lived translucent patch over the pad. Redrawn every tick so it tracks the
	// cursor smoothly (same technique as the muzzle tracers in Tower/Defender) without ever
	// leaving stale debug geometry behind if the cursor moves off the pad.
	DrawDebugSolidPlane(GetWorld(), FPlane(FVector::UpVector, SlotLocation.Z + 6.0f),
		SlotLocation, 85.0f, HighlightColor, false, 0.05f);
}
