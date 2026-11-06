// DefenderUpgradeComponent_Extra.inl - included by DefenderUpgradeComponent.cpp (not compiled alone).
// Queued purchases, the HUD upgrade countdown and the stats text shown on the Defenders screen.

void UDefenderUpgradeComponent::BeginPlay()
{
	Super::BeginPlay();
	// Timer, not Tick. World timers pause with the game, so the countdown freezes and resumes with it.
	GetWorld()->GetTimerManager().SetTimer(CountdownTimer, this, &UDefenderUpgradeComponent::UpdateHudCountdown, 0.2f, true);
}

bool UDefenderUpgradeComponent::IsPending(EUpgrade Upgrade) const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UTDMetaProgressionSubsystem* Meta = GameInstance ? GameInstance->GetSubsystem<UTDMetaProgressionSubsystem>() : nullptr;
	return Meta && Meta->IsUpgradePending((int32)Upgrade);
}

void UDefenderUpgradeComponent::StartActive(EUpgrade Upgrade, const TArray<AActor*>& Targets)
{
	UE_LOG(LogTemp, Display, TEXT("Defender upgrade %s activated on %d target(s)."), ConfigOf(Upgrade).ButtonName, Targets.Num());
	FSlot& Slot = Slots[(int32)Upgrade];
	Slot.Phase = EPhase::Active;
	for (AActor* Target : Targets)
	{
		ApplyBoost(Upgrade, Target);
		if (const ATower* Tower = Cast<ATower>(Target)) { Tower->PlayUpgradeVFX(); }
		else if (const ADefender* Defender = Cast<ADefender>(Target)) { Defender->PlayActivationVFX(); }
	}
	GetWorld()->GetTimerManager().SetTimer(Slot.PhaseTimer,
		FTimerDelegate::CreateUObject(this, &UDefenderUpgradeComponent::EndActive, Upgrade), ConfigOf(Upgrade).Active, false);
	RefreshButtons();
}

void UDefenderUpgradeComponent::ActivatePendingUpgrades()
{
	UTDMetaProgressionSubsystem* Meta = GetWorld()->GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>();
	for (int32 Index = 0; Meta && Index < (int32)EUpgrade::Count; ++Index)
	{
		const EUpgrade Upgrade = (EUpgrade)Index;
		if (Slots[Index].Phase != EPhase::Ready || !Meta->IsUpgradePending(Index))
		{
			continue;
		}
		TArray<AActor*> Targets;
		GatherTargets(Upgrade, Targets);
		if (Targets.Num() > 0 && Meta->ConsumePendingUpgrade(Index))
		{
			StartActive(Upgrade, Targets);
		}
	}
}

// Bottom-right HUD countdown (MM:SS) for the upgrade whose active time or cooldown ends next.
void UDefenderUpgradeComponent::UpdateHudCountdown()
{
	const ATDGameMode* GameMode = GetGameMode();
	UTDHUDWidget* Hud = GameMode ? GameMode->GetMatchHUDWidget() : nullptr;
	if (!Hud || !Hud->WidgetTree)
	{
		return; // Start menu: no match HUD.
	}

	UTextBlock* Label = CountdownLabel.Get();
	if (!Label)
	{
		UCanvasPanel* Canvas = nullptr;
		Hud->WidgetTree->ForEachWidget([&Canvas](UWidget* Widget) { if (!Canvas) { Canvas = Cast<UCanvasPanel>(Widget); } });
		if (!Canvas)
		{
			return;
		}
		Label = Hud->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UpgradeCountdown"));
		UCanvasPanelSlot* LabelSlot = Canvas->AddChildToCanvas(Label);
		LabelSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		LabelSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		LabelSlot->SetPosition(FVector2D(-40.0f, -40.0f));
		LabelSlot->SetAutoSize(true);
		LabelSlot->SetZOrder(100);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 24;
		Label->SetFont(Font);
		Label->SetJustification(ETextJustify::Right);
		Label->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		CountdownLabel = Label;
	}

	const FTimerManager& Timers = GetWorld()->GetTimerManager();
	int32 Next = INDEX_NONE;
	float NextLeft = 0.0f;
	for (int32 Index = 0; Index < (int32)EUpgrade::Count; ++Index)
	{
		const float Left = Slots[Index].Phase == EPhase::Ready ? -1.0f : Timers.GetTimerRemaining(Slots[Index].PhaseTimer);
		if (Left >= 0.0f && (Next == INDEX_NONE || Left < NextLeft))
		{
			Next = Index;
			NextLeft = Left;
		}
	}
	if (Next == INDEX_NONE)
	{
		Label->SetVisibility(ESlateVisibility::Collapsed); // nothing running: every upgrade is available
		return;
	}

	static const TCHAR* Names[] = { TEXT("Basic"), TEXT("Archer"), TEXT("Poison"), TEXT("Strong Guardian"), TEXT("Tower") };
	const bool bActive = Slots[Next].Phase == EPhase::Active;
	const int32 Seconds = FMath::CeilToInt(NextLeft);
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s upgrade %s %02d:%02d"),
		Names[Next], bActive ? TEXT("active") : TEXT("ready in"), Seconds / 60, Seconds % 60)));
	Label->SetColorAndOpacity(bActive ? FLinearColor(0.55f, 1.0f, 0.45f) : FLinearColor::White);
	Label->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// Reads the real stats: from a living spawned defender/tower of that type when one exists,
// otherwise from the class defaults (CDO) that new spawns start with, so it never shows zeros.
FString UDefenderUpgradeComponent::DescribeStats(EUpgrade Upgrade) const
{
	TArray<AActor*> Targets;
	GatherTargets(Upgrade, Targets);
	const ATDGameMode* GameMode = GetGameMode();
	const AActor* Source = Targets.Num() > 0 ? Targets[0] : nullptr;
	if (!Source)
	{
		UClass* Class = Upgrade == EUpgrade::Tower ? (GameMode && GameMode->TowerClass ? GameMode->TowerClass.Get() : ATower::StaticClass())
			: Upgrade == EUpgrade::Archer ? AArcherDefender::StaticClass()
			: Upgrade == EUpgrade::Poison ? APoisonLightBombDefender::StaticClass()
			: Upgrade == EUpgrade::Strong ? AStrongDefender::StaticClass()
			: ADefender::StaticClass();
		Source = Class->GetDefaultObject<AActor>();
	}

	float Health = 0.0f, Interval = 1.0f, Damage = 0.0f;
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

	// Speed is attacks per second, the same unit used by the upgrade targets.
	const FUpgradeConfig& Config = ConfigOf(Upgrade);
	return FString::Printf(TEXT("HP %.0f  SPD %.1f  DMG %.0f\nUpgrade: %.0f / %.1f / %.0f"),
		Health, Interval > 0.0f ? 1.0f / Interval : 0.0f, Damage, Config.Health, Config.Speed, Config.Damage);
}
