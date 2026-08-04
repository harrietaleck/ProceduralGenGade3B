// TDPlayerController.cpp — see TDPlayerController.h for the overview.

#include "TDPlayerController.h"
#include "Defender.h"
#include "ProceduralTerrain.h"
#include "TDGameMode.h"
#include "EngineUtils.h"

ATDPlayerController::ATDPlayerController()
{
	// Default to the plain C++ defender unless overridden.
	DefenderClass = ADefender::StaticClass();
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

	AProceduralTerrain* Terrain = GameMode->GetTerrain();
	if (!Terrain)
	{
		return;
	}

	// Trace under the cursor to find where on the world the player clicked.
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex=*/false, Hit))
	{
		return;
	}
	const FVector ClickLocation = Hit.Location;

	// Snap the click to the nearest buildable slot (compared on the ground plane, ignoring Z).
	const TArray<FVector>& Slots = Terrain->GetDefenderSlots();
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = SlotClickTolerance * SlotClickTolerance;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FVector Delta(ClickLocation.X - Slots[i].X, ClickLocation.Y - Slots[i].Y, 0.0f);
		const float DistSq = Delta.SizeSquared();
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = i;
		}
	}

	// Clicked away from any slot.
	if (BestIndex == INDEX_NONE)
	{
		return;
	}
	const FVector SlotLocation = Slots[BestIndex];

	// Can't stack two defenders on one slot.
	if (IsSlotOccupied(SlotLocation))
	{
		return;
	}

	// Check affordability using the defender's own Cost, then spend.
	const int32 Cost = DefenderClass.GetDefaultObject()->Cost;
	if (!GameMode->TrySpendResources(Cost))
	{
		return; // Not enough resources.
	}

	// Spawn the defender on the slot, raised so its base rests on the ground.
	// (Named DefenderSpawnLocation to avoid hiding the inherited APlayerController::SpawnLocation.)
	const FVector DefenderSpawnLocation = SlotLocation + FVector(0.0f, 0.0f, 60.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<ADefender>(DefenderClass, DefenderSpawnLocation, FRotator::ZeroRotator, SpawnParams);
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
