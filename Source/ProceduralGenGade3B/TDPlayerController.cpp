// TDPlayerController.cpp
// Player input and defender placement. See TDPlayerController.h for the overview.

#include "TDPlayerController.h"

#include "Defender.h"

#include "StrongDefender.h"

//Include the new defenders so each button can select its own C++ class

#include "ArcherDefender.h"

#include "BombDefender.h"

#include "ProceduralTerrain.h"

#include "TDGameMode.h"

#include "TDHUDWidget.h"
#include "TDHUD.h"

#include "DrawDebugHelpers.h"

ATDPlayerController::ATDPlayerController()

{

    //Start with the Basic defender selected so clicking a pad works straight away
    DefenderClass = ADefender::StaticClass();

    StrongDefenderClass = AStrongDefender::StaticClass();

    // Tick every frame so we can draw the build pad hover highlight.

    PrimaryActorTick.bCanEverTick = true;

}

void ATDPlayerController::BeginPlay()

{

    Super::BeginPlay();

    // Show the mouse cursor and let clicks hit things in the world.

    bShowMouseCursor = true;

    bEnableClickEvents = true;

    bEnableMouseOverEvents = true;

}

void ATDPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Left click places a defender.
    InputComponent->BindKey(
        EKeys::LeftMouseButton,
        IE_Pressed,
        this,
        &ATDPlayerController::OnPlaceDefenderClicked
    );

    // R restarts the game at any time, as the brief asks.
    InputComponent->BindKey(
        EKeys::R,
        IE_Pressed,
        this,
        &ATDPlayerController::OnRestartPressed
    );

    // P pauses or unpauses the match. It has to work while paused too.
    FInputKeyBinding& PauseBinding =
        InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ATDPlayerController::OnPausePressed);
    PauseBinding.bExecuteWhenPaused = true;

    // N shows or hides the NavMesh debug view.
    InputComponent->BindKey(
        EKeys::N,
        IE_Pressed,
        this,
        &ATDPlayerController::OnToggleNavMeshDebug
    );

    // Tab switches between basic and strong defender placement.
    InputComponent->BindKey(
        EKeys::Tab,
        IE_Pressed,
        this,
        &ATDPlayerController::OnToggleDefenderMode
    );

    // U upgrades the tower beam.
    InputComponent->BindKey(
        EKeys::U,
        IE_Pressed,
        this,
        &ATDPlayerController::OnUpgradeBeamPressed
    );

    //Press 1 to select the basic/original defender
    InputComponent->BindKey(
        EKeys::One,
        IE_Pressed,
        this,
        &ATDPlayerController::SelectBasicDefender
    );

    //Press 2 to select the archer defender
    InputComponent->BindKey(
        EKeys::Two,
        IE_Pressed,
        this,
        &ATDPlayerController::SelectArcherDefender
    );

    //Press 3 to select the Poison Light Bomb defender
    InputComponent->BindKey(
        EKeys::Three,
        IE_Pressed,
        this,
        &ATDPlayerController::SelectPoisonLightBombDefender
    );

    //Press 4 to select the Strong defender
    InputComponent->BindKey(
        EKeys::Four,
        IE_Pressed,
        this,
        &ATDPlayerController::SelectStrongDefender
    );

    //Press G to show the wave director graph. This also works on the paused results screen
    FInputKeyBinding& GraphBinding =
        InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ATDPlayerController::OnToggleDirectorGraph);
    GraphBinding.bExecuteWhenPaused = true;
}

void ATDPlayerController::OnToggleDirectorGraph()
{
    if (ATDHUD* TDHUD = Cast<ATDHUD>(GetHUD()))
    {
        TDHUD->ToggleDirectorGraph();
    }
}

void ATDPlayerController::OnToggleNavMeshDebug()

{

    // "show Navigation" is Unreal's built-in NavMesh debug view, so we just use that.

    ConsoleCommand(TEXT("show Navigation"));

}

void ATDPlayerController::OnPausePressed()

{

    if (ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>())

    {

        GameMode->TogglePause();

    }

}

void ATDPlayerController::OnRestartPressed()

{

    if (ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>())

    {

        GameMode->RestartGame();

    }

}

void ATDPlayerController::OnUpgradeBeamPressed()
{
    if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
    {
        GameMode->TryUpgradeTowerBeam();
    }
}

void ATDPlayerController::OnToggleDefenderMode()

{

    if (ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>())

    {

        if (GameMode->IsGameOver() ||

            GameMode->IsVictory() ||

            GameMode->IsInteractionBlocked())

        {

            return;

        }

    }

    if (!StrongDefenderClass)

    {

        return;

    }

    bPlacingStrongDefender = !bPlacingStrongDefender;

    if (ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>())

    {

        GameMode->RefreshMetaHUD();

    }

}

//Select the original defender and switch off strong defender mode

void ATDPlayerController::SelectBasicDefender()

{

    SelectDefenderClass(ADefender::StaticClass());

}

//Select the archer defender so it can be placed

void ATDPlayerController::SelectArcherDefender()

{

    SelectDefenderClass(AArcherDefender::StaticClass());

}

//Select the bomb defender so it can be placed

void ATDPlayerController::SelectPoisonLightBombDefender()

{

    //Use the actual Poison Light Bomb class from BombDefender.h.

    SelectDefenderClass(APoisonLightBombDefender::StaticClass());

}

void ATDPlayerController::SelectStrongDefender()
{
    if (!StrongDefenderClass)
    {
        return;
    }

    if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
    {
        if (GameMode->IsGameOver() || GameMode->IsVictory() || GameMode->IsInteractionBlocked())
        {
            return;
        }

        bPlacingStrongDefender = true;
        GameMode->RefreshMetaHUD();
    }
}

//Change the defender class but keep using the existing placement system

void ATDPlayerController::SelectDefenderClass(

    TSubclassOf<ADefender> NewDefenderClass)

{

    if (!NewDefenderClass)

    {

        return;

    }

    if (ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>())

    {

        //Do not allow defender selection after a match or when input is blocked

        if (GameMode->IsGameOver() ||

            GameMode->IsVictory() ||

            GameMode->IsInteractionBlocked())

        {

            return;

        }

    }

    //Pick one of the 3 normal defenders and turn off strong mode

    bPlacingStrongDefender = false;

    DefenderClass = NewDefenderClass;

    //Refresh the HUD so the defender info updates straight away

    if (ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>())

    {

        GameMode->RefreshMetaHUD();

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

bool ATDPlayerController::CanAffordDefender(

    const ADefender* Defaults,

    const ATDGameMode* GameMode) const

{

    if (!Defaults || !GameMode)

    {

        return false;

    }

    return GameMode->GetResources() >= Defaults->Cost &&

        GameMode->CanAffordMeta(Defaults->MetaCost);

}

void ATDPlayerController::OnPlaceDefenderClicked()

{

    // No placing while paused or after the game ends.

    ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>();

    const TSubclassOf<ADefender> ActiveClass =

        GetActiveDefenderClass();

    if (!GameMode ||

        GameMode->IsInteractionBlocked() ||

        !ActiveClass)

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

    // Check we can pay the defender's Cost, but don't spend yet.
    // Loot is only taken once the defender has really spawned, further down.

    const ADefender* Defaults =

        ActiveClass.GetDefaultObject();

    if (!CanAffordDefender(Defaults, GameMode))

    {

        GameMode->ShowInsufficientFundsWarning();

        return;

    }

    const int32 Cost = Defaults->Cost;

    const FMetaCurrencyRewards MetaCost = Defaults->MetaCost;

    // Spawn the defender on the slot, raised so its base rests on the ground.

    // It is called DefenderSpawnLocation so it does not hide APlayerController's own SpawnLocation.

    const FVector DefenderSpawnLocation =

        SlotLocation + FVector(0.0f, 0.0f, 60.0f);

    FActorSpawnParameters SpawnParams;

    SpawnParams.SpawnCollisionHandlingOverride =

        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    //Spawn whichever defender is selected

    ADefender* NewDefender =

        GetWorld()->SpawnActor<ADefender>(

            ActiveClass,

            DefenderSpawnLocation,

            FRotator::ZeroRotator,

            SpawnParams

        );

    // Only pay if the defender really spawned.

    if (NewDefender)

    {

        GameMode->TrySpendResources(Cost);

        GameMode->TrySpendMeta(MetaCost);

        GameMode->NotifyDefenderPlaced();

        NewDefender->SetOccupiedSlot(SlotLocation);

        if (AProceduralTerrain* Terrain =

            GameMode->GetTerrain())

        {

            Terrain->SetSlotOccupied(

                SlotLocation,

                true

            );

        }

    }

}

bool ATDPlayerController::IsSlotOccupied(

    const FVector& SlotLocation) const

{

    ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>();

    AProceduralTerrain* Terrain =

        GameMode ? GameMode->GetTerrain() : nullptr;

    return Terrain &&

        Terrain->IsSlotOccupied(SlotLocation);

}

bool ATDPlayerController::FindNearestSlotUnderCursor(

    FVector& OutSlotLocation) const

{

    ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>();

    AProceduralTerrain* Terrain =

        GameMode ? GameMode->GetTerrain() : nullptr;

    if (!Terrain)

    {

        return false;

    }

    // Trace under the cursor to find where in the world the player is pointing.

    FHitResult Hit;

    if (!GetHitResultUnderCursor(

        ECC_Visibility,

        /*bTraceComplex=*/false,

        Hit))

    {

        return false;

    }

    const FVector CursorLocation = Hit.Location;

    // Snap to the nearest build slot. We only compare X and Y and ignore height.

    const TArray<FDefenderSlot>& Slots =

        Terrain->GetDefenderSlots();

    int32 BestIndex = INDEX_NONE;

    float BestDistSq =

        SlotClickTolerance * SlotClickTolerance;

    for (int32 i = 0; i < Slots.Num(); ++i)

    {

        const float DistSq =

            FVector::DistSquared2D(

                CursorLocation,

                Slots[i].Location

            );

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

    ATDGameMode* GameMode =

        GetWorld()->GetAuthGameMode<ATDGameMode>();

    const TSubclassOf<ADefender> ActiveClass =

        GetActiveDefenderClass();

    if (!GameMode ||

        GameMode->IsInteractionBlocked() ||

        !ActiveClass)

    {

        return;

    }

    FVector SlotLocation;

    if (!FindNearestSlotUnderCursor(SlotLocation))

    {

        return; // Cursor isn't near a build pad, so there is nothing to draw.

    }

    // Valid means the slot is free and the player can afford this defender.

    const bool bOccupied =

        IsSlotOccupied(SlotLocation);

    const ADefender* Defaults =

        ActiveClass.GetDefaultObject();

    const bool bAffordable =

        CanAffordDefender(Defaults, GameMode);

    const bool bValid =

        !bOccupied && bAffordable;

    const FColor HighlightColor =

        bValid

            ? FColor(60, 220, 90, 140)

            : FColor(220, 60, 60, 140);

    // A flat see-through patch over the pad that only lasts a moment. We redraw it every tick
    // so it follows the cursor, like the tracers in Tower and Defender, and old patches never stay behind.

    DrawDebugSolidPlane(

        GetWorld(),

        FPlane(

            FVector::UpVector,

            SlotLocation.Z + 6.0f

        ),

        SlotLocation,

        85.0f,

        HighlightColor,

        false,

        0.05f

    );

}
