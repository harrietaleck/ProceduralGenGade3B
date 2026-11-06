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
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "TDHUDWidget.h"                 //  HUD upgrade countdown
#include "TDMetaProgressionSubsystem.h"  //  queued (already paid) upgrades

using namespace DefenderUpgrades;

namespace
{
	/** Speed is attacks per second; Active and Cooldown are seconds. */
	struct FUpgradeConfig { const TCHAR* ButtonName; float Health, Speed, Damage, Active, Cooldown; int32 Cost; };

	const FUpgradeConfig Configs[] = {
		{ TEXT("BasicButton"),  130.0f, 4.5f, 16.0f, 20.0f, 10.0f, 50 },
		{ TEXT("ArcherButton"), 115.0f, 5.5f, 14.0f, 30.0f, 20.0f, 65 },
		{ TEXT("PoisonButton"), 140.0f, 4.0f, 20.0f, 15.0f, 15.0f, 50 },
		{ TEXT("StrongButton"), 250.0f, 3.0f, 28.0f, 60.0f, 30.0f, 50 },
		{ TEXT("TowerButton"),  650.0f, 1.8f, 60.0f, 90.0f, 60.0f, 750 },
	};

	constexpr float ArcherArrowSpeedBonus = 1000.0f;
	const FLinearColor ReadyColor(1.0f, 1.0f, 1.0f, 0.0f);
	const FLinearColor GreyColor(0.25f, 0.25f, 0.25f, 0.65f);

	const FUpgradeConfig& ConfigOf(EUpgrade Upgrade)
	{
		return Configs[(int32)Upgrade];
	}

	EUpgrade UpgradeFor(const ADefender* Defender)
	{
		return Defender->IsA<AArcherDefender>() ? EUpgrade::Archer
			: Defender->IsA<APoisonLightBombDefender>() ? EUpgrade::Poison
			: Defender->IsA<AStrongDefender>() ? EUpgrade::Strong
			: EUpgrade::Basic;
	}

	/**  All five are paid in meta currency: leaf unlocks Strong and Tower, wood unlocks Basic. */
	FMetaCurrencyRewards MetaCostOf(EUpgrade Upgrade)
	{
		FMetaCurrencyRewards Cost;
		const int32 Amount = ConfigOf(Upgrade).Cost;
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
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UTDMetaProgressionSubsystem* Meta = GameInstance->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			Meta->OnMetaCurrencyChanged.RemoveDynamic(this, &UDefenderUpgradeComponent::HandleMetaCurrencyChanged);
		}
	}

	ClearScreenBindings();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
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
	GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);

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
		Slot.Label = CreateButtonLabel(Screen, Button, TEXT("_Timer"), 20, FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(8.0f, -6.0f)); // countdown at bottom left
		Slot.StatsLabel = CreateButtonLabel(Screen, Button, TEXT("_Stats"), 13, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 4.0f)); //  stats just below the button
	}

	if (UButton* Button = Slots[(int32)EUpgrade::Basic].Button.Get())  { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnBasicClicked); }
	if (UButton* Button = Slots[(int32)EUpgrade::Archer].Button.Get()) { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnArcherClicked); }
	if (UButton* Button = Slots[(int32)EUpgrade::Poison].Button.Get()) { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnPoisonClicked); }
	if (UButton* Button = Slots[(int32)EUpgrade::Strong].Button.Get()) { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnStrongClicked); }
	if (UButton* Button = Slots[(int32)EUpgrade::Tower].Button.Get())  { Button->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::OnTowerClicked); }

	// The screen's own Exit loads the start menu; during a match it must only close the screen.
	const ATDGameMode* GameMode = GetGameMode();
	if (GameMode && GameMode->GetTower())
	{
		if (UButton* Exit = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ExitBTN"))))
		{
			Exit->OnClicked.Clear();
			Exit->OnClicked.AddDynamic(this, &UDefenderUpgradeComponent::CloseScreen);
		}
	}

	RefreshButtons();
	GetWorld()->GetTimerManager().SetTimer(
		RefreshTimer, this, &UDefenderUpgradeComponent::RefreshButtons, 1.0f, true);
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
	UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(Button->Slot);
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Button->GetParent());
	if (!ButtonSlot || !Canvas || !Screen->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot position countdown label for '%s': expected a CanvasPanel parent."), *Button->GetName());
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
	GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
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
	RefreshButtons();
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
	return GameMode && GameMode->CanAffordMeta(MetaCostOf(Upgrade)); //  same wallet for all five
}

bool UDefenderUpgradeComponent::Spend(EUpgrade Upgrade) const
{
	// TrySpendMeta checks the balance first and updates the HUD, so nothing can go negative.
	ATDGameMode* GameMode = GetGameMode();
	return GameMode && GameMode->TrySpendMeta(MetaCostOf(Upgrade)); //
}

void UDefenderUpgradeComponent::TryActivate(EUpgrade Upgrade)
{
	FSlot& Slot = Slots[(int32)Upgrade];
	const ATDGameMode* GameMode = GetGameMode();
	//  Was IsInteractionBlocked(), which is also true on the between-wave results screen and silently
	//  ate clicks there. Only a finished match blocks purchases now. A queued upgrade can't be bought twice.
	if (Slot.Phase != EPhase::Ready || !GameMode || GameMode->IsGameOver() || GameMode->IsVictory() || IsPending(Upgrade)) { return; }

	TArray<AActor*> Targets;
	GatherTargets(Upgrade, Targets);
	if (!Spend(Upgrade)) //  the single deduction; nothing is charged when this fails
	{
		UE_LOG(LogTemp, Warning, TEXT("Defender upgrade %s refused: not enough resources."), ConfigOf(Upgrade).ButtonName); //
		RefreshButtons();
		return;
	}

	//  Root cause of "buttons do nothing": the Defenders screen is the Start-menu shop, where no units
	//  exist, so every click used to be refused. Now the paid upgrade is queued and starts as soon as
	//  that unit type is on the field (placement, or match start for the Tower).
	if (Targets.Num() == 0)
	{
		if (UTDMetaProgressionSubsystem* Meta = GetWorld()->GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			Meta->AddPendingUpgrade((int32)Upgrade);
		}
		UE_LOG(LogTemp, Display, TEXT("Defender upgrade %s bought and queued until that unit is on the field."), ConfigOf(Upgrade).ButtonName);
		RefreshButtons();
		return;
	}
	StartActive(Upgrade, Targets); //
}

void UDefenderUpgradeComponent::EndActive(EUpgrade Upgrade)
{
	RemoveBoosts(Upgrade);

	FSlot& Slot = Slots[(int32)Upgrade];
	Slot.Phase = EPhase::Cooldown;
	GetWorld()->GetTimerManager().SetTimer(
		Slot.PhaseTimer,
		FTimerDelegate::CreateUObject(this, &UDefenderUpgradeComponent::EndCooldown, Upgrade),
		ConfigOf(Upgrade).Cooldown,
		false);
	RefreshButtons();
}

void UDefenderUpgradeComponent::EndCooldown(EUpgrade Upgrade)
{
	Slots[(int32)Upgrade].Phase = EPhase::Ready;
	RefreshButtons();
}

void UDefenderUpgradeComponent::HandleDefenderSpawned(ADefender* Defender)
{
	if (!Defender) { return; }
	const EUpgrade Upgrade = UpgradeFor(Defender);
	if (Slots[(int32)Upgrade].Phase == EPhase::Active) { ApplyBoost(Upgrade, Defender); }
	else { ActivatePendingUpgrades(); } //  first unit of a queued (already paid) upgrade type
}

void UDefenderUpgradeComponent::ApplyBoost(EUpgrade Upgrade, AActor* Target)
{
	UHealthComponent* Health = Target ? Target->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Health || Health->IsDead()) { return; }

	// Raise each stat to its target, never lower it, and remember exactly how much was added.
	const FUpgradeConfig& Config = ConfigOf(Upgrade);
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
}

void UDefenderUpgradeComponent::RemoveBoosts(EUpgrade Upgrade)
{
	FSlot& Slot = Slots[(int32)Upgrade];
	for (const FBoost& Boost : Slot.Boosts)
	{
		AActor* Target = Boost.Target.Get();
		if (!Target) { continue; }

		if (UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>()) { Health->AdjustMaxHealth(-Boost.Health); }

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
}

//  DescribeStats moved to DefenderUpgradeComponent_Extra.inl to keep this file under 500 lines.

void UDefenderUpgradeComponent::RefreshButtons()
{
	UpdateHudCountdown();

	UUserWidget* Screen = BoundScreen.Get();
	if (!Screen || !Screen->IsInViewport())
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
		ClearScreenBindings();
		return;
	}

	const FTimerManager& Timers = GetWorld()->GetTimerManager();

	//  The Defenders screen has no C++ parent and its graph never sets its resource text, so its
	//  four Text Blocks (LeafR, WoodR, GemR, LanternR) stayed empty. Fill them from the existing
	//  meta wallet here; this runs when the screen is bound, after every purchase and every 0.25 s.
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

		const bool bUnavailable = Slot.Phase != EPhase::Ready;
		const bool bQueued = !bUnavailable && IsPending(Upgrade);
		const bool bCanAfford = CanAfford(Upgrade);
		const bool bUnlocked = !bUnavailable && !bQueued && bCanAfford;
		const FLinearColor StateColor = bUnavailable
			? FLinearColor(0.9f, 0.12f, 0.12f, 1.0f)
			: bQueued
				? FLinearColor(1.0f, 0.42f, 0.05f, 1.0f)
				: bUnlocked
					? ReadyColor
					: GreyColor;

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
		Button->SetBackgroundColor(FLinearColor::White);

		if (bUnavailable || bQueued || !bCanAfford)
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

		if (UTextBlock* Stats = Slot.StatsLabel.Get()) //  refreshed every 0.25 s, so activation/expiry show at once
		{
			Stats->SetText(FText::FromString(DescribeStats(Upgrade))); //
			//  Keep generated stats hidden below the buttons; button labels and purchase logic remain unchanged.
			Stats->SetVisibility(ESlateVisibility::Collapsed);          //  never blocks button clicks
		}

		UTextBlock* Label = Slot.Label.Get();
		if (!Label) { continue; }
		if (bQueued)
		{
			Label->SetText(FText::FromString(TEXT("Queued")));
			Label->SetColorAndOpacity(FLinearColor::Black);
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
			continue;
		}
		if (Slot.Phase == EPhase::Ready)
		{
			if (!bCanAfford)
			{
				Label->SetText(FText::FromString(TEXT("Insufficient resources")));
				Label->SetColorAndOpacity(FLinearColor::Black);
				Label->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Label->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}

		const int32 Seconds = FMath::CeilToInt(FMath::Max(0.0f, Timers.GetTimerRemaining(Slot.PhaseTimer)));
		Label->SetText(FText::FromString(FString::Printf(TEXT("%d s"), Seconds)));
		Label->SetColorAndOpacity(FLinearColor::Black);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

#include "DefenderUpgradeComponent_Extra.inl" //  queued purchases, HUD countdown, stats text
