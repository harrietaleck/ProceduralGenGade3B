// TDPlayerController.cpp — see TDPlayerController.h for the overview.

#include "TDPlayerController.h"
#include "Defender.h"
#include "StrongDefender.h"
#include "ProceduralTerrain.h"
#include "TDGameMode.h"
#include "TDHUDWidget.h"
#include "DrawDebugHelpers.h"

ATDPlayerController::ATDPlayerController()
{
	// Default to the plain C++ defender unless overridden.
	DefenderClass = ADefender::StaticClass();
	StrongDefenderClass = AStrongDefender::StaticClass();

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

	// Bind R to restart at any time (brief: restart whenever the player wants).
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ATDPlayerController::OnRestartPressed);

	// Bind P to pause/unpause the match.
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ATDPlayerController::OnPausePressed);

	// Bind N to toggle the NavMesh debug overlay.
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ATDPlayerController::OnToggleNavMeshDebug);

	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ATDPlayerController::OnToggleDefenderMode);
	InputComponent->BindKey(EKeys::U, IE_Pressed, this, &ATDPlayerController::OnUpgradeBeamPressed);
}

void ATDPlayerController::OnToggleNavMeshDebug()
{
	// "show Navigation" is the engine's own NavMesh debug-draw toggle — reuse it rather than
	// reimplementing NavMesh visualisation.
	ConsoleCommand(TEXT("show Navigation"));
}

void ATDPlayerController::OnPausePressed()
{
	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GameMode->TogglePause();
	}
}

void ATDPlayerController::OnRestartPressed()
{
	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GameMode->RestartGame();
	}
}

void ATDPlayerController::OnToggleDefenderMode()
{
	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		if (GameMode->IsGameOver() || GameMode->IsVictory() || GameMode->IsInteractionBlocked())
		{
			return;
		}
	}

	if (!StrongDefenderClass)
	{
		return;
	}

	bPlacingStrongDefender = !bPlacingStrongDefender;

	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GameMode->RefreshMetaHUD();
	}
}

void ATDPlayerController::OnUpgradeBeamPressed()
{
	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GameMode->TryUpgradeTowerBeam();
	}
}

TSubclassOf<ADefender> ATDPlayerController::GetActiveDefenderClass() const
{
	if (bPlacingStrongDefender && StrongDefenderClass)
	{
		return StrongDefenderClass;
	}
	return DefenderClass;
}

bool ATDPlayerController::CanAffordDefender(const ADefender* Defaults, const ATDGameMode* GameMode) const
{
	if (!Defaults || !GameMode)
	{
		return false;
	}
	return GameMode->GetResources() >= Defaults->Cost && GameMode->CanAffordMeta(Defaults->MetaCost);
}

void ATDPlayerController::OnPlaceDefenderClicked()
{
	// No placing while paused or after the game ends.
	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	const TSubclassOf<ADefender> ActiveClass = GetActiveDefenderClass();
	if (!GameMode || GameMode->IsInteractionBlocked() || !ActiveClass)
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
	const ADefender* Defaults = ActiveClass.GetDefaultObject();
	if (!CanAffordDefender(Defaults, GameMode))
	{
		GameMode->ShowInsufficientFundsWarning();
		return;
	}

	const int32 Cost = Defaults->Cost;
	const FMetaCurrencyRewards MetaCost = Defaults->MetaCost;

	// Spawn the defender on the slot, raised so its base rests on the ground.
	// (Named DefenderSpawnLocation to avoid hiding the inherited APlayerController::SpawnLocation.)
	const FVector DefenderSpawnLocation = SlotLocation + FVector(0.0f, 0.0f, 60.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADefender* NewDefender = GetWorld()->SpawnActor<ADefender>(ActiveClass, DefenderSpawnLocation, FRotator::ZeroRotator, SpawnParams);

	// Only pay for a placement that actually succeeded.
	if (NewDefender)
	{
		GameMode->TrySpendResources(Cost);
		GameMode->TrySpendMeta(MetaCost);
		NewDefender->SetOccupiedSlot(SlotLocation);
		if (AProceduralTerrain* Terrain = GameMode->GetTerrain())
		{
			Terrain->SetSlotOccupied(SlotLocation, true);
		}
	}
}

bool ATDPlayerController::IsSlotOccupied(const FVector& SlotLocation) const
{
	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	AProceduralTerrain* Terrain = GameMode ? GameMode->GetTerrain() : nullptr;
	return Terrain && Terrain->IsSlotOccupied(SlotLocation);
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
	const TArray<FDefenderSlot>& Slots = Terrain->GetDefenderSlots();
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = SlotClickTolerance * SlotClickTolerance;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const float DistSq = FVector::DistSquared2D(CursorLocation, Slots[i].Location);
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
	OutSlotLocation = Slots[BestIndex].Location;
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
	const TSubclassOf<ADefender> ActiveClass = GetActiveDefenderClass();
	if (!GameMode || GameMode->IsInteractionBlocked() || !ActiveClass)
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
	const ADefender* Defaults = ActiveClass.GetDefaultObject();
	const bool bAffordable = CanAffordDefender(Defaults, GameMode);
	const bool bValid = !bOccupied && bAffordable;

	const FColor HighlightColor = bValid ? FColor(60, 220, 90, 140) : FColor(220, 60, 60, 140);

	// A flat, short-lived translucent patch over the pad. Redrawn every tick so it tracks the
	// cursor smoothly (same technique as the muzzle tracers in Tower/Defender) without ever
	// leaving stale debug geometry behind if the cursor moves off the pad.
	DrawDebugSolidPlane(GetWorld(), FPlane(FVector::UpVector, SlotLocation.Z + 6.0f),
		SlotLocation, 85.0f, HighlightColor, false, 0.05f);
}
