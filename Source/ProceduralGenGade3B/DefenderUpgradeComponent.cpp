// DefenderUpgradeComponent.cpp - see DefenderUpgradeComponent.h.

#include "DefenderUpgradeComponent.h"
#include "ArcherDefender.h"
#include "BombDefender.h"
#include "Defender.h"
#include "HealthComponent.h"
#include "StrongDefender.h"
#include "TDGameMode.h"
#include "TDMatchRewards.h"
#include "TDMenuFunctionLibrary.h"
#include "Tower.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "TimerManager.h"

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
	const FLinearColor GreyColor(0.25f, 0.25f, 0.25f, 0.8f);

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

	/** Basic, Archer, Poison and Strong are paid in meta currency; the Tower is paid in Loot. */
	FMetaCurrencyRewards MetaCostOf(EUpgrade Upgrade)
	{
		FMetaCurrencyRewards Cost;
		const int32 Amount = ConfigOf(Upgrade).Cost;
		switch (Upgrade)
		{
		case EUpgrade::Basic:  Cost.ForestEssence = Amount; break;
		case EUpgrade::Archer: Cost.GemStones = Amount; break;
		case EUpgrade::Poison: Cost.LightLanterns = Amount; break;
		case EUpgrade::Strong: Cost.ForestEssence = Amount; break;
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
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearAllTimersForObject(this); }
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
	UTDMenuFunctionLibrary::PinCanvasLayoutToDesignSize(Screen); // *** fixes elements drifting off the stretched art

	for (int32 Index = 0; Index < (int32)EUpgrade::Count; ++Index)
	{
		FSlot& Slot = Slots[Index];
		UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(Configs[Index].ButtonName));
		Slot.Button = Button;
		Slot.Label = nullptr;
		if (!Button) { continue; }

		Button->OnClicked.RemoveAll(this);
		Slot.Label = CreateButtonLabel(Screen, Button, TEXT("_Timer"), 30, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector); // *** centred countdown
		Slot.StatsLabel = CreateButtonLabel(Screen, Button, TEXT("_Stats"), 13, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 4.0f)); // *** stats just below the button
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
		RefreshTimer, this, &UDefenderUpgradeComponent::RefreshButtons, 0.25f, true);
}

UTextBlock* UDefenderUpgradeComponent::CreateButtonLabel(UUserWidget* Screen, UButton* Button, const TCHAR* Suffix, int32 FontSize,
	FVector2D Pivot, FVector2D Alignment, FVector2D PixelOffset) const // ***
{
	UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(Button->Slot);
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Button->GetParent());
	if (!ButtonSlot || !Canvas || !Screen->WidgetTree) { return nullptr; }

	const FName LabelName(*(Button->GetName() + Suffix)); // ***
	UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(LabelName));
	if (!Label)
	{
		Label = Screen->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		Canvas->AddChildToCanvas(Label);
	}

	UCanvasPanelSlot* LabelSlot = Cast<UCanvasPanelSlot>(Label->Slot);
	if (!LabelSlot) { return nullptr; }

	// Sit on the centre of the button without touching the button's own layout.
	// *** Pivot picks the spot on the button (0.5,0.5 = centre, 0.5,1 = bottom edge). Buttons are now
	// *** proportionally anchored, so that spot is an anchor point and the label follows the art too.
	const FAnchors Anchors = ButtonSlot->GetAnchors();
	LabelSlot->SetAlignment(Alignment); // ***
	LabelSlot->SetAutoSize(true);       // ***
	if (Anchors.Minimum == Anchors.Maximum)
	{
		LabelSlot->SetAnchors(Anchors);
		LabelSlot->SetPosition(ButtonSlot->GetPosition()
			+ (Pivot - ButtonSlot->GetAlignment()) * ButtonSlot->GetSize() + PixelOffset); // ***
	}
	else
	{
		const FVector2D Point = Anchors.Minimum + (Anchors.Maximum - Anchors.Minimum) * Pivot; // ***
		LabelSlot->SetAnchors(FAnchors(Point.X, Point.Y)); // ***
		LabelSlot->SetPosition(PixelOffset);               // ***
	}
	LabelSlot->SetZOrder(ButtonSlot->GetZOrder() + 1);

	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize; // ***
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
		BindOpenScreen();
	}
}

void UDefenderUpgradeComponent::CloseScreen()
{
	if (UUserWidget* Screen = BoundScreen.Get()) { Screen->RemoveFromParent(); }
	BoundScreen.Reset();
	GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
}

void UDefenderUpgradeComponent::OnBasicClicked()  { TryActivate(EUpgrade::Basic); }
void UDefenderUpgradeComponent::OnArcherClicked() { TryActivate(EUpgrade::Archer); }
void UDefenderUpgradeComponent::OnPoisonClicked() { TryActivate(EUpgrade::Poison); }
void UDefenderUpgradeComponent::OnStrongClicked() { TryActivate(EUpgrade::Strong); }
void UDefenderUpgradeComponent::OnTowerClicked()  { TryActivate(EUpgrade::Tower); }

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
	return GameMode && (Upgrade == EUpgrade::Tower
		? GameMode->GetResources() >= ConfigOf(Upgrade).Cost
		: GameMode->CanAffordMeta(MetaCostOf(Upgrade)));
}

bool UDefenderUpgradeComponent::Spend(EUpgrade Upgrade) const
{
	// Both calls check the balance first and update the HUD, so nothing can go negative.
	ATDGameMode* GameMode = GetGameMode();
	return GameMode && (Upgrade == EUpgrade::Tower
		? GameMode->TrySpendResources(ConfigOf(Upgrade).Cost)
		: GameMode->TrySpendMeta(MetaCostOf(Upgrade)));
}

void UDefenderUpgradeComponent::TryActivate(EUpgrade Upgrade)
{
	FSlot& Slot = Slots[(int32)Upgrade];
	const ATDGameMode* GameMode = GetGameMode();
	if (Slot.Phase != EPhase::Ready || !GameMode || GameMode->IsInteractionBlocked()) { return; }

	TArray<AActor*> Targets;
	GatherTargets(Upgrade, Targets);
	// *** Log why a click was refused, so a silent "no response" can be told apart from a broken binding.
	if (Targets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Defender upgrade %s refused: no living target in this level (place one in a match first)."), ConfigOf(Upgrade).ButtonName); // ***
		RefreshButtons();
		return;
	}
	if (!Spend(Upgrade))
	{
		UE_LOG(LogTemp, Warning, TEXT("Defender upgrade %s refused: not enough resources."), ConfigOf(Upgrade).ButtonName); // ***
		RefreshButtons();
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("Defender upgrade %s activated on %d target(s)."), ConfigOf(Upgrade).ButtonName, Targets.Num()); // ***

	Slot.Phase = EPhase::Active;
	for (AActor* Target : Targets)
	{
		ApplyBoost(Upgrade, Target);
	}

	GetWorld()->GetTimerManager().SetTimer(
		Slot.PhaseTimer,
		FTimerDelegate::CreateUObject(this, &UDefenderUpgradeComponent::EndActive, Upgrade),
		ConfigOf(Upgrade).Active,
		false);
	RefreshButtons();
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

// *** Reads the real stats: from a living spawned defender/tower of that type when one exists,
// *** otherwise from the class defaults (CDO) that new spawns start with, so it never shows zeros.
FString UDefenderUpgradeComponent::DescribeStats(EUpgrade Upgrade) const
{
	TArray<AActor*> Targets;
	GatherTargets(Upgrade, Targets);
	const ATDGameMode* GameMode = GetGameMode();
	const AActor* Source = Targets.Num() > 0 ? Targets[0] : nullptr; // ***
	if (!Source)
	{
		UClass* Class = Upgrade == EUpgrade::Tower ? (GameMode && GameMode->TowerClass ? GameMode->TowerClass.Get() : ATower::StaticClass())
			: Upgrade == EUpgrade::Archer ? AArcherDefender::StaticClass()
			: Upgrade == EUpgrade::Poison ? APoisonLightBombDefender::StaticClass()
			: Upgrade == EUpgrade::Strong ? AStrongDefender::StaticClass()
			: ADefender::StaticClass(); // ***
		Source = Class->GetDefaultObject<AActor>(); // ***
	}

	float Health = 0.0f, Interval = 1.0f, Damage = 0.0f;
	// *** Uses the explicit HealthComponent members so class defaults (not yet spawned) read correctly too.
	if (const ATower* Tower = Cast<ATower>(Source))
	{
		Interval = Tower->FireInterval; Damage = Tower->AttackDamage;
		Health = Tower->HealthComponent ? Tower->HealthComponent->MaxHealth : 0.0f;
	}
	else if (const ADefender* Defender = Cast<ADefender>(Source))
	{
		Interval = Defender->GetAttackInterval(); Damage = Defender->GetAttackDamage();
		Health = Defender->HealthComponent ? Defender->HealthComponent->MaxHealth : 0.0f;
	}

	// *** Speed is attacks per second, the same unit used by the upgrade targets.
	const FUpgradeConfig& Config = ConfigOf(Upgrade);
	return FString::Printf(TEXT("HP %.0f  SPD %.1f  DMG %.0f\nUpgrade: %.0f / %.1f / %.0f"),
		Health, Interval > 0.0f ? 1.0f / Interval : 0.0f, Damage, Config.Health, Config.Speed, Config.Damage);
}

void UDefenderUpgradeComponent::RefreshButtons()
{
	const FTimerManager& Timers = GetWorld()->GetTimerManager();

	// *** The Defenders screen has no C++ parent and its graph never sets its resource text, so its
	// *** four Text Blocks (LeafR, WoodR, GemR, LanternR) stayed empty. Fill them from the existing
	// *** meta wallet here; this runs when the screen is bound, after every purchase and every 0.25 s.
	if (UUserWidget* Screen = BoundScreen.Get())
	{
		if (const ATDGameMode* GameMode = GetGameMode())
		{
			const FMetaCurrencyRewards Wallet = GameMode->GetMetaWallet(); // ***
			auto SetValue = [Screen](const TCHAR* Name, int32 Value) // ***
			{
				if (UTextBlock* Text = Cast<UTextBlock>(Screen->GetWidgetFromName(Name))) // ***
				{
					Text->SetText(FText::AsNumber(Value)); // ***
				}
			};
			SetValue(TEXT("LeafR"), Wallet.ForestEssence);     // *** Forest Essence
			SetValue(TEXT("WoodR"), Wallet.WoodenMight);       // *** Wooden Might
			SetValue(TEXT("GemR"), Wallet.GemStones);          // *** Gem Stones
			SetValue(TEXT("LanternR"), Wallet.LightLanterns);  // *** Light Lanterns
		}
	}
	for (int32 Index = 0; Index < (int32)EUpgrade::Count; ++Index)
	{
		const EUpgrade Upgrade = (EUpgrade)Index;
		FSlot& Slot = Slots[Index];
		UButton* Button = Slot.Button.Get();
		if (!Button) { continue; }

		bool bUsable = false;
		if (Slot.Phase == EPhase::Ready && CanAfford(Upgrade))
		{
			TArray<AActor*> Targets;
			GatherTargets(Upgrade, Targets);
			bUsable = Targets.Num() > 0;
		}
		Button->SetBackgroundColor(bUsable || Slot.Phase == EPhase::Active ? ReadyColor : GreyColor);

		if (UTextBlock* Stats = Slot.StatsLabel.Get()) // *** refreshed every 0.25 s, so activation/expiry show at once
		{
			Stats->SetText(FText::FromString(DescribeStats(Upgrade))); // ***
			Stats->SetVisibility(ESlateVisibility::HitTestInvisible);   // *** never blocks button clicks
		}

		UTextBlock* Label = Slot.Label.Get();
		if (!Label) { continue; }
		if (Slot.Phase == EPhase::Ready)
		{
			Label->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const int32 Seconds = FMath::CeilToInt(FMath::Max(0.0f, Timers.GetTimerRemaining(Slot.PhaseTimer)));
		Label->SetText(FText::FromString(FString::Printf(TEXT("%ds"), Seconds)));
		Label->SetColorAndOpacity(Slot.Phase == EPhase::Active
			? FLinearColor(0.55f, 1.0f, 0.45f)
			: FLinearColor::White);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
