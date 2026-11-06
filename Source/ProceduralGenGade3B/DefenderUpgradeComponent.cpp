// DefenderUpgradeComponent.cpp - see DefenderUpgradeComponent.h.

#include "DefenderUpgradeComponent.h"
#include "ArcherDefender.h"
#include "BombDefender.h"
#include "Defender.h"
#include "HealthComponent.h"
#include "StrongDefender.h"
#include "TDGameMode.h"
#include "TDPlayerController.h"
#include "TDMatchRewards.h"
#include "TDMenuFunctionLibrary.h"
#include "Tower.h"
#include "UpgradeVisualComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "TDHUDWidget.h"                 //  HUD upgrade status text
#include "TDMetaProgressionSubsystem.h"  //  queued (already paid) upgrades

using namespace DefenderUpgrades;

namespace
{
	struct FUpgradeConfig { const TCHAR* ButtonName; float Health, Speed, Damage; int32 Cost; };

	const FUpgradeConfig Configs[] = {
		{ TEXT("BasicButton"),  130.0f, 4.5f, 16.0f, 250 },
		{ TEXT("ArcherButton"), 115.0f, 5.5f, 14.0f, 65 },
		{ TEXT("PoisonButton"), 140.0f, 4.0f, 20.0f, 15 },
		{ TEXT("StrongButton"), 250.0f, 3.0f, 28.0f, 250 },
		{ TEXT("TowerButton"),  650.0f, 1.8f, 60.0f, 750 },
	};

	// Tier II: bought with the same button while tier I is active. About 30% stronger than tier I
	// for roughly 1.6 to 2 times the price, so it is a real choice and not an automatic purchase.
	const FUpgradeConfig Tier2Configs[] = {
		{ TEXT("BasicButton"),  165.0f, 5.5f, 22.0f, 400 },
		{ TEXT("ArcherButton"), 150.0f, 7.0f, 20.0f, 110 },
		{ TEXT("PoisonButton"), 175.0f, 5.0f, 28.0f, 30 },
		{ TEXT("StrongButton"), 330.0f, 3.8f, 38.0f, 450 },
		{ TEXT("TowerButton"),  900.0f, 2.4f, 85.0f, 1300 },
	};

	/** Every unit has two upgrade tiers: tier I first, then tier II later in the same wave. */
	constexpr int32 MaxTier = 2;

	constexpr float ArcherArrowSpeedBonus = 1000.0f;
	const FLinearColor ReadyColor(1.0f, 1.0f, 1.0f, 0.0f);
	const FLinearColor MaxTierColor(0.95f, 0.75f, 0.15f, 1.0f);

	const FUpgradeConfig& ConfigOf(EUpgrade Upgrade)
	{
		return Configs[(int32)Upgrade];
	}

	/** Stats and price of a tier. Anything above tier I uses the tier II table. */
	const FUpgradeConfig& TierConfigOf(EUpgrade Upgrade, int32 Tier)
	{
		return Tier >= 2 ? Tier2Configs[(int32)Upgrade] : Configs[(int32)Upgrade];
	}

	EUpgrade UpgradeFor(const ADefender* Defender)
	{
		return Defender->IsA<AArcherDefender>() ? EUpgrade::Archer
			: Defender->IsA<APoisonLightBombDefender>() ? EUpgrade::Poison
			: Defender->IsA<AStrongDefender>() ? EUpgrade::Strong
			: EUpgrade::Basic;
	}

	/**  All five are paid in meta currency: leaf unlocks Strong and Tower, wood unlocks Basic. */
	FMetaCurrencyRewards MetaCostOf(EUpgrade Upgrade, int32 Tier = 1)
	{
		FMetaCurrencyRewards Cost;
		const int32 Amount = TierConfigOf(Upgrade, Tier).Cost;
		switch (Upgrade)
		{
		case EUpgrade::Basic:  Cost.WoodenMight = Amount; break;   //  wood
		case EUpgrade::Archer: Cost.GemStones = Amount; break;     // gem
		case EUpgrade::Poison: Cost.LightLanterns = Amount; break; // sun
		case EUpgrade::Strong: Cost.ForestEssence = Amount; break; // leaf
		case EUpgrade::Tower:  Cost.ForestEssence = Amount; break; //  leaf (was Loot)
		default: break;
		}
		return Cost;
	}

	bool IsAlive(const AActor* Actor)
	{
		const UHealthComponent* Health = Actor ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		return Health && !Health->IsDead();
	}
}

UDefenderUpgradeComponent::UDefenderUpgradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDefenderUpgradeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		for (FSlot& Slot : Slots)
		{
			World->GetTimerManager().ClearTimer(Slot.CooldownTimer);
		}
	}

	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UTDMetaProgressionSubsystem* Meta = GameInstance->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			Meta->OnMetaCurrencyChanged.RemoveDynamic(this, &UDefenderUpgradeComponent::HandleMetaCurrencyChanged);
		}
	}

	ClearScreenBindings();
	Super::EndPlay(EndPlayReason);
}

ATDGameMode* UDefenderUpgradeComponent::GetGameMode() const
{
	return Cast<ATDGameMode>(GetOwner());
}

void UDefenderUpgradeComponent::BindOpenScreen()
{
	if (UUserWidget* Screen = BoundScreen.Get(); Screen && Screen->IsInViewport()) { return; }

	BoundScreen.Reset();
	ClearScreenBindings();

	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget && Widget->IsInViewport() && Widget->GetClass()->GetName().Contains(TEXT("DefendersScreen")))
		{
			BindScreen(Widget);
			return;
		}
	}
}

void UDefenderUpgradeComponent::BindScreen(UUserWidget* Screen)
{
	BoundScreen = Screen;
	UTDMenuFunctionLibrary::PinCanvasLayoutToDesignSize(Screen); //  fixes elements drifting off the stretched art

	for (int32 Index = 0; Index < (int32)EUpgrade::Count; ++Index)
	{
		FSlot& Slot = Slots[Index];
		UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(Configs[Index].ButtonName));
		Slot.Button = Button;
		Slot.Label = nullptr;
		if (!Button)
		{
			UE_LOG(LogTemp, Warning, TEXT("DefendersScreen is missing expected button '%s'."), Configs[Index].ButtonName);
			continue;
		}
		Slot.OriginalButtonStyle = Button->GetStyle();
		Slot.OriginalTextColors.Reset();
		Screen->WidgetTree->ForEachWidget([&Slot, Button](UWidget* Widget)
		{
			UTextBlock* Text = Cast<UTextBlock>(Widget);
			for (UPanelWidget* Parent = Text ? Text->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
			{
				if (Parent == Button)
				{
					Slot.OriginalTextColors.Add({ Text, Text->GetColorAndOpacity() });
					break;
				}
			}
		});

		Button->OnClicked.RemoveAll(this);
		Slot.Label = CreateButtonLabel(Screen, Button, TEXT("_Status"), 20, FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(8.0f, -6.0f));
		Slot.StatsLabel = CreateButtonLabel(Screen, Button, TEXT("_Stats"), 13, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 4.0f));
	}

	if (UButton* Button = Slots[(int32)EUpgrade::Basic].Button.Get())  { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnBasicClicked); }
	if (UButton* Button = Slots[(int32)EUpgrade::Archer].Button.Get()) { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnArcherClicked); }
	if (UButton* Button = Slots[(int32)EUpgrade::Poison].Button.Get()) { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnPoisonClicked); }
	if (UButton* Button = Slots[(int32)EUpgrade::Strong].Button.Get()) { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnStrongClicked); }
	if (UButton* Button = Slots[(int32)EUpgrade::Tower].Button.Get())  { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnTowerClicked); }

	// The screen's own Exit loads the start menu; during a match it must only close the screen.
	ATDGameMode* GameMode = GetGameMode();
	if (GameMode && GameMode->GetTower())
	{
		if (UButton* Exit = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ExitBTN"))))
		{
			Exit->OnClicked.Clear();
			Exit->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::CloseScreen);
		}
	}

	RefreshUpgradeUI();
}

void UDefenderUpgradeComponent::ClearScreenBindings()
{
	for (FSlot& Slot : Slots)
	{
		if (UButton* Button = Slot.Button.Get())
		{
			Button->OnClicked.RemoveAll(this);
		}
		Slot.Button.Reset();
		Slot.Label.Reset();
		Slot.StatsLabel.Reset();
		Slot.OriginalTextColors.Reset();
	}
	BoundScreen.Reset();
}

UTextBlock* UDefenderUpgradeComponent::CreateButtonLabel(UUserWidget* Screen, UButton* Button, const TCHAR* Suffix, int32 FontSize,
	FVector2D Pivot, FVector2D Alignment, FVector2D PixelOffset) const //
{
	if (FCString::Strcmp(Suffix, TEXT("_Status")) == 0)
	{
		const FName LabelName(*(Button->GetName() + Suffix));
		UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(LabelName));
		UOverlay* Overlay = Cast<UOverlay>(Button->GetContent());
		if (!Overlay)
		{
			Overlay = Screen->WidgetTree->ConstructWidget<UOverlay>(
				UOverlay::StaticClass(), FName(*(Button->GetName() + TEXT("_StatusOverlay"))));
			UWidget* ExistingContent = Button->GetContent();
			Button->SetContent(Overlay);
			if (ExistingContent)
			{
				if (UOverlaySlot* ContentSlot = Overlay->AddChildToOverlay(ExistingContent))
				{
					ContentSlot->SetHorizontalAlignment(HAlign_Fill);
					ContentSlot->SetVerticalAlignment(VAlign_Fill);
				}
			}
		}

		if (!Label)
		{
			Label = Screen->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		}
		if (UPanelWidget* Parent = Label->GetParent(); Parent && Parent != Overlay)
		{
			Parent->RemoveChild(Label);
		}
		if (Label->GetParent() != Overlay)
		{
			Overlay->AddChildToOverlay(Label);
		}

		if (UOverlaySlot* LabelSlot = Cast<UOverlaySlot>(Label->Slot))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			LabelSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 3.0f));
		}
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FLinearColor::Black);
		Label->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Label->SetShadowColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.8f));
		Label->SetRenderOpacity(1.0f);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Label;
	}

	UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(Button->Slot);
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Button->GetParent());
	if (!ButtonSlot || !Canvas || !Screen->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot position stats label for '%s': expected a CanvasPanel parent."), *Button->GetName());
		return nullptr;
	}

	const FName LabelName(*(Button->GetName() + Suffix)); //
	UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(LabelName));
	if (!Label)
	{
		Label = Screen->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		Canvas->AddChildToCanvas(Label);
	}

	UCanvasPanelSlot* LabelSlot = Cast<UCanvasPanelSlot>(Label->Slot);
	if (!LabelSlot) { return nullptr; }

	// Sit on the centre of the button without touching the button's own layout.
	//  Pivot picks the spot on the button (0.5,0.5 = centre, 0.5,1 = bottom edge). Buttons are now
	//  proportionally anchored, so that spot is an anchor point and the label follows the art too.
	const FAnchors Anchors = ButtonSlot->GetAnchors();
	LabelSlot->SetAlignment(Alignment); //
	LabelSlot->SetAutoSize(true);       //
	if (Anchors.Minimum == Anchors.Maximum)
	{
		LabelSlot->SetAnchors(Anchors);
		LabelSlot->SetPosition(ButtonSlot->GetPosition()
			+ (Pivot - ButtonSlot->GetAlignment()) * ButtonSlot->GetSize() + PixelOffset); //
	}
	else
	{
		const FVector2D Point = Anchors.Minimum + (Anchors.Maximum - Anchors.Minimum) * Pivot; //
		LabelSlot->SetAnchors(FAnchors(Point.X, Point.Y)); //
		LabelSlot->SetPosition(PixelOffset);               //
	}
	LabelSlot->SetZOrder(ButtonSlot->GetZOrder() + 1);

	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize; //
	Label->SetFont(Font);
	Label->SetJustification(ETextJustify::Center);
	Label->SetShadowOffset(FVector2D(2.0f, 2.0f));
	Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	Label->SetVisibility(ESlateVisibility::Collapsed);
	return Label;
}

void UDefenderUpgradeComponent::ToggleScreen()
{
	const ATDGameMode* GameMode = GetGameMode();
	if (!GameMode || !GameMode->GetTower()) { return; }

	if (UUserWidget* Screen = BoundScreen.Get(); Screen && Screen->IsInViewport())
	{
		CloseScreen();
		return;
	}

	UClass* ScreenClass = LoadClass<UUserWidget>(
		nullptr, TEXT("/Game/UI/ScreenWidgets/DefendersScreen.DefendersScreen_C"));
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!ScreenClass || !PC) { return; }

	if (UUserWidget* Screen = CreateWidget<UUserWidget>(PC, ScreenClass))
	{
		Screen->AddToViewport(140);
		UTDMenuFunctionLibrary::StretchWidgetToFillScreen(Screen, false);
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(Screen->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		BindOpenScreen();
	}
}

void UDefenderUpgradeComponent::CloseScreen()
{
	if (UUserWidget* Screen = BoundScreen.Get()) { Screen->RemoveFromParent(); }
	ClearScreenBindings();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
}

void UDefenderUpgradeComponent::OnBasicClicked()  { TryActivate(EUpgrade::Basic); }
void UDefenderUpgradeComponent::OnArcherClicked() { TryActivate(EUpgrade::Archer); }
void UDefenderUpgradeComponent::OnPoisonClicked() { TryActivate(EUpgrade::Poison); }
void UDefenderUpgradeComponent::OnStrongClicked() { TryActivate(EUpgrade::Strong); }
void UDefenderUpgradeComponent::OnTowerClicked()  { TryActivate(EUpgrade::Tower); }

void UDefenderUpgradeComponent::HandleMetaCurrencyChanged()
{
	RefreshUpgradeUI();
}

void UDefenderUpgradeComponent::GatherTargets(EUpgrade Upgrade, TArray<AActor*>& OutTargets) const
{
	const ATDGameMode* GameMode = GetGameMode();
	if (!GameMode) { return; }

	if (Upgrade == EUpgrade::Tower)
	{
		ATower* Tower = GameMode->GetTower();
		if (IsAlive(Tower)) { OutTargets.Add(Tower); }
		return;
	}

	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		if (IsAlive(*It) && UpgradeFor(*It) == Upgrade) { OutTargets.Add(*It); }
	}
}

bool UDefenderUpgradeComponent::CanAfford(EUpgrade Upgrade) const
{
	const ATDGameMode* GameMode = GetGameMode();
	// The price is for the next tier: tier I when nothing is bought yet, tier II once tier I is active.
	return GameMode && GameMode->CanAffordMeta(MetaCostOf(Upgrade, FMath::Clamp(Slots[(int32)Upgrade].Tier + 1, 1, MaxTier)));
}

bool UDefenderUpgradeComponent::Spend(EUpgrade Upgrade) const
{
	// TrySpendMeta checks the balance first and updates the HUD, so nothing can go negative.
	ATDGameMode* GameMode = GetGameMode();
	return GameMode && GameMode->TrySpendMeta(MetaCostOf(Upgrade, FMath::Clamp(Slots[(int32)Upgrade].Tier + 1, 1, MaxTier)));
}

void UDefenderUpgradeComponent::TryActivate(EUpgrade Upgrade)
{
	UE_LOG(LogTemp, Display, TEXT("Defender upgrade purchase requested: %s."), ConfigOf(Upgrade).ButtonName);
	FSlot& Slot = Slots[(int32)Upgrade];
	ATDGameMode* GameMode = GetGameMode();
	// Each upgrade has two tiers. Tier I is bought first, tier II is bought later in the same wave with the
	// same button. Once tier II is active there is nothing left to buy until the wave ends. Before this,
	// clicking an already-active upgrade charged again for no extra effect.
	const int32 NextTier = Slot.Tier + 1;
	if (NextTier > MaxTier || Slot.Phase == EPhase::DisabledAfterWave || Slot.CooldownSecondsRemaining > 0
		|| !GameMode || GameMode->IsGameOver() || GameMode->IsVictory() || IsPending(Upgrade))
	{
		return;
	}

	TArray<AActor*> Targets;
	GatherTargets(Upgrade, Targets);

	// Tier II only makes sense for units that are on the field right now, so never take payment for it otherwise.
	if (NextTier > 1 && Targets.Num() == 0)
	{
		return;
	}

	if (!Spend(Upgrade)) //  the single deduction; nothing is charged when this fails
	{
		UE_LOG(LogTemp, Warning, TEXT("Defender upgrade %s refused: not enough resources."), ConfigOf(Upgrade).ButtonName); //
		if (ATDGameMode* MutableGameMode = GetGameMode())
		{
			MutableGameMode->ShowUpgradeInsufficientFundsWarning();
		}
		RefreshUpgradeUI();
		return;
	}

	// If there is no matching unit yet, keep the already-paid upgrade queued until one is placed.
	if (NextTier == 1 && Targets.Num() == 0)
	{
		if (UTDMetaProgressionSubsystem* Meta = GetWorld()->GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			Meta->AddPendingUpgrade((int32)Upgrade);
		}
		UE_LOG(LogTemp, Display, TEXT("Defender upgrade %s bought and queued until that unit is on the field."), ConfigOf(Upgrade).ButtonName);
		GameMode->SaveCurrentWaveCheckpoint();
		RefreshUpgradeUI();
		return;
	}
	if (NextTier == 1)
	{
		StartActive(Upgrade, Targets);
	}
	else
	{
		StartTierTwo(Upgrade, Targets);
	}
	GameMode->SaveCurrentWaveCheckpoint();
}

void UDefenderUpgradeComponent::StartCooldown(EUpgrade Upgrade)
{
	FSlot& Slot = Slots[(int32)Upgrade];
	if (Slot.CooldownSecondsRemaining > 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Slot.CooldownSecondsRemaining = FMath::RandRange(10, 30);
	const int32 UpgradeIndex = (int32)Upgrade;
	World->GetTimerManager().SetTimer(
		Slot.CooldownTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, UpgradeIndex]()
		{
			TickUpgradeCooldown(UpgradeIndex);
		}),
		1.0f,
		true);
}

void UDefenderUpgradeComponent::TickUpgradeCooldown(int32 UpgradeIndex)
{
	if (UpgradeIndex < 0 || UpgradeIndex >= (int32)EUpgrade::Count)
	{
		return;
	}

	FSlot& Slot = Slots[UpgradeIndex];
	Slot.CooldownSecondsRemaining = FMath::Max(0, Slot.CooldownSecondsRemaining - 1);
	if (Slot.CooldownSecondsRemaining == 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Slot.CooldownTimer);
		}
		if (Slot.Phase == EPhase::DisabledAfterWave)
		{
			Slot.Phase = EPhase::Ready;
		}
	}
	RefreshUpgradeUI();
}

void UDefenderUpgradeComponent::HandleDefenderSpawned(ADefender* Defender)
{
	if (!Defender) { return; }
	const EUpgrade Upgrade = UpgradeFor(Defender);
	const bool bWasPending = IsPending(Upgrade);
	ActivatePendingUpgrades();
	if (!bWasPending && Slots[(int32)Upgrade].Phase == EPhase::Active)
	{
		ApplyBoost(Upgrade, Defender);
	}
	RefreshUpgradeUI();
}

void UDefenderUpgradeComponent::ResetTemporaryUpgradesForWaveEnd()
{
	UWorld* World = GetWorld();
	UTDMetaProgressionSubsystem* Meta = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>()
		: nullptr;
	bool bRefundQueued[(int32)EUpgrade::Count] = {};
	if (Meta)
	{
		for (int32 Index = 0; Index < (int32)EUpgrade::Count; ++Index)
		{
			bRefundQueued[Index] = Meta->IsUpgradePending(Index);
		}
		Meta->ClearPendingUpgrades();
	}

	for (int32 Index = 0; Index < (int32)EUpgrade::Count; ++Index)
	{
		FSlot& Slot = Slots[Index];
		const bool bWasActive = Slot.Phase == EPhase::Active;
		const bool bWasQueued = bRefundQueued[Index];
		RemoveBoosts((EUpgrade)Index);
		if (bWasActive || bWasQueued)
		{
			Slot.Phase = EPhase::DisabledAfterWave;
		}
		else if (Slot.Phase != EPhase::DisabledAfterWave)
		{
			Slot.Phase = EPhase::Ready;
		}

		if (Meta && bRefundQueued[Index])
		{
			// A queued purchase that never had a matching unit received no effect this wave.
			Meta->AddRewards(MetaCostOf((EUpgrade)Index));
		}
		if (bWasActive)
		{
			StartCooldown((EUpgrade)Index);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("Wave-end upgrade reset complete; queued purchases cleared."));
	RefreshUpgradeUI();
}

void UDefenderUpgradeComponent::HandleWaveStarted(int32 WaveNumber)
{
	for (FSlot& Slot : Slots)
	{
		if (Slot.Phase == EPhase::DisabledAfterWave)
		{
			Slot.Phase = EPhase::Ready;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Refreshing defender upgrades for wave %d."), WaveNumber);
	RefreshUpgradeUI();
}

void UDefenderUpgradeComponent::ApplyBoost(EUpgrade Upgrade, AActor* Target)
{
	UHealthComponent* Health = Target ? Target->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Health || Health->IsDead()) { return; }

	// Raise each stat to its target, never lower it, and remember exactly how much was added.
	// Use the stats of whichever tier is active. Calling this again for tier II only adds the difference,
	// because every value is raised towards its target and never past it.
	const int32 ActiveTier = FMath::Max(1, Slots[(int32)Upgrade].Tier);
	const FUpgradeConfig& Config = TierConfigOf(Upgrade, ActiveTier);
	const float TargetInterval = 1.0f / Config.Speed;
	FBoost Boost;
	Boost.Target = Target;
	Boost.Health = FMath::Max(0.0f, Config.Health - Health->MaxHealth);
	Health->AdjustMaxHealth(Boost.Health);

	if (ATower* Tower = Cast<ATower>(Target))
	{
		Boost.Interval = FMath::Min(0.0f, TargetInterval - Tower->FireInterval);
		Boost.Damage = FMath::Max(0.0f, Config.Damage - Tower->AttackDamage);
		Tower->AttackDamage += Boost.Damage;
		if (Boost.Interval < 0.0f) { Tower->SetFireInterval(Tower->FireInterval + Boost.Interval); }
	}
	else if (ADefender* Defender = Cast<ADefender>(Target))
	{
		Boost.Interval = FMath::Min(0.0f, TargetInterval - Defender->GetAttackInterval());
		Boost.Damage = FMath::Max(0.0f, Config.Damage - Defender->GetAttackDamage());
		Defender->SetAttackDamage(Defender->GetAttackDamage() + Boost.Damage);
		if (Boost.Interval < 0.0f) { Defender->SetAttackInterval(Defender->GetAttackInterval() + Boost.Interval); }
		if (Upgrade == EUpgrade::Archer)
		{
			Boost.ProjectileSpeed = ArcherArrowSpeedBonus;
			Defender->ProjectileSpeedBonus += Boost.ProjectileSpeed;
		}
	}

	Slots[(int32)Upgrade].Boosts.Add(Boost);

	// Show the tier on the unit itself (orb, ring and a gold tint), not only as a one-off particle burst.
	if (UUpgradeVisualComponent* Visual = Target->FindComponentByClass<UUpgradeVisualComponent>())
	{
		Visual->SetTier(ActiveTier);
	}
}

void UDefenderUpgradeComponent::RemoveBoosts(EUpgrade Upgrade)
{
	FSlot& Slot = Slots[(int32)Upgrade];
	for (const FBoost& Boost : Slot.Boosts)
	{
		AActor* Target = Boost.Target.Get();
		if (!Target) { continue; }

		if (UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>()) { Health->AdjustMaxHealth(-Boost.Health); }
		if (UUpgradeVisualComponent* Visual = Target->FindComponentByClass<UUpgradeVisualComponent>()) { Visual->SetTier(0); }

		if (ATower* Tower = Cast<ATower>(Target))
		{
			Tower->AttackDamage = FMath::Max(0.0f, Tower->AttackDamage - Boost.Damage);
			if (Boost.Interval < 0.0f) { Tower->SetFireInterval(Tower->FireInterval - Boost.Interval); }
		}
		else if (ADefender* Defender = Cast<ADefender>(Target))
		{
			Defender->SetAttackDamage(FMath::Max(0.0f, Defender->GetAttackDamage() - Boost.Damage));
			if (Boost.Interval < 0.0f) { Defender->SetAttackInterval(Defender->GetAttackInterval() - Boost.Interval); }
			Defender->ProjectileSpeedBonus = FMath::Max(0.0f, Defender->ProjectileSpeedBonus - Boost.ProjectileSpeed);
		}
	}
	Slot.Boosts.Reset();
	Slot.Tier = 0; // upgrades last one wave, so the next wave starts again from tier I
}

//  DescribeStats moved to DefenderUpgradeComponent_Extra.inl to keep this file under 500 lines.

void UDefenderUpgradeComponent::RefreshUpgradeUI()
{
	UpdateStatusText();

	if (ATDGameMode* GameMode = GetGameMode())
	{
		GameMode->RefreshMetaHUD();
	}

	UUserWidget* Screen = BoundScreen.Get();
	if (!Screen || !Screen->IsInViewport())
	{
		ClearScreenBindings();
		return;
	}

	// Fill the screen's resource text from the existing meta wallet when it opens or changes.
	if (const ATDGameMode* GameMode = GetGameMode())
	{
		const FMetaCurrencyRewards Wallet = GameMode->GetMetaWallet();
		auto SetValue = [Screen](const TCHAR* Name, int32 Value)
		{
			if (UTextBlock* Text = Cast<UTextBlock>(Screen->GetWidgetFromName(Name)))
			{
				Text->SetText(FText::AsNumber(Value));
			}
		};
		SetValue(TEXT("LeafR"), Wallet.ForestEssence);
		SetValue(TEXT("WoodR"), Wallet.WoodenMight);
		SetValue(TEXT("GemR"), Wallet.GemStones);
		SetValue(TEXT("LanternR"), Wallet.LightLanterns);
	}
	for (int32 Index = 0; Index < (int32)EUpgrade::Count; ++Index)
	{
		const EUpgrade Upgrade = (EUpgrade)Index;
		FSlot& Slot = Slots[Index];
		UButton* Button = Slot.Button.Get();
		if (!Button) { continue; }

		const bool bQueued = Slot.Phase == EPhase::Ready && IsPending(Upgrade);
		const bool bActive = Slot.Phase == EPhase::Active;
		const bool bDisabledAfterWave = Slot.Phase == EPhase::DisabledAfterWave;
		const bool bCoolingDown = Slot.CooldownSecondsRemaining > 0;
		const bool bHasColoredState = bQueued || bActive || bDisabledAfterWave || bCoolingDown;
		const FLinearColor StateColor = bCoolingDown || bDisabledAfterWave
			? FLinearColor(0.85f, 0.05f, 0.05f, 1.0f)
			: bQueued ? FLinearColor(0.12f, 0.72f, 0.18f, 1.0f)
			: bActive ? (Slot.Tier >= MaxTier ? MaxTierColor : FLinearColor(0.12f, 0.72f, 0.18f, 1.0f)) // gold once tier II is bought
			: ReadyColor;

		// Keep clicks available so the purchase handler can show why a purchase is refused.
		// TryActivate still enforces phase, queue, match state, and the actual wallet balance.
		Button->SetIsEnabled(true);
		FButtonStyle StateStyle = Slot.OriginalButtonStyle;
		StateStyle.Normal.TintColor = FSlateColor(StateColor);
		StateStyle.Hovered.TintColor = FSlateColor(StateColor);
		StateStyle.Pressed.TintColor = FSlateColor(StateColor);
		StateStyle.Disabled = StateStyle.Normal;
		StateStyle.Disabled.TintColor = FSlateColor(StateColor);
		Button->SetStyle(StateStyle);
		Button->SetBackgroundColor(StateColor);

		if (bHasColoredState)
		{
			for (const FSlot::FTextColor& TextColor : Slot.OriginalTextColors)
			{
				if (UTextBlock* Text = TextColor.Widget.Get())
				{
					Text->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
				}
			}
		}
		else
		{
			for (const FSlot::FTextColor& TextColor : Slot.OriginalTextColors)
			{
				if (UTextBlock* Text = TextColor.Widget.Get())
				{
					Text->SetColorAndOpacity(TextColor.OriginalColor);
				}
			}
		}

		if (UTextBlock* Stats = Slot.StatsLabel.Get())
		{
			Stats->SetText(FText::FromString(DescribeStats(Upgrade))); //
			//  Keep generated stats hidden below the buttons; button labels and purchase logic remain unchanged.
			Stats->SetVisibility(ESlateVisibility::Collapsed);          //  never blocks button clicks
		}

		UTextBlock* Label = Slot.Label.Get();
		if (!Label)
		{
			if (bQueued || bActive)
			{
				UE_LOG(LogTemp, Warning, TEXT("DefendersScreen has no visible status label for upgrade button '%s'."),
					Configs[Index].ButtonName);
			}
			continue;
		}

		if (bQueued || bActive || bCoolingDown)
		{
			FString Status;
			if (bQueued)
			{
				Status = TEXT("PURCHASED - QUEUED");
			}
			else if (bActive && bCoolingDown)
			{
				Status = FString::Printf(TEXT("PURCHASED - %ds"), Slot.CooldownSecondsRemaining);
			}
			else if (bActive)
			{
				// Tier I leaves the button live, so tell the player the same button now buys tier II.
				Status = Slot.Tier >= MaxTier ? TEXT("TIER II - MAX") : TEXT("TIER I - CLICK FOR II");
			}
			else if (bDisabledAfterWave && bCoolingDown)
			{
				Status = FString::Printf(TEXT("DISABLED - %ds"), Slot.CooldownSecondsRemaining);
			}
			else if (!bActive && !bQueued && bCoolingDown)
			{
				Status = FString::Printf(TEXT("%ds"), Slot.CooldownSecondsRemaining);
			}
			else
			{
				Status = TEXT("PURCHASED");
			}
			Label->SetText(FText::FromString(Status));
			Label->SetColorAndOpacity(bCoolingDown ? FLinearColor::White : FLinearColor::Black);
			Label->SetRenderOpacity(1.0f);
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Label->SetText(FText::GetEmpty());
			Label->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

#include "DefenderUpgradeComponent_Extra.inl" //  queued purchases, HUD countdown, stats text
