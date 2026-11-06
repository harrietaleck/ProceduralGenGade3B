// DefenderUpgradeComponent_Extra.inl - included by DefenderUpgradeComponent.cpp (not compiled alone).
// Queued purchases, wave-scoped status text and the stats text shown on the Defenders screen.

void UDefenderUpgradeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UTDMetaProgressionSubsystem* Meta = GameInstance->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			Meta->OnMetaCurrencyChanged.AddUniqueDynamic(this, &UDefenderUpgradeComponent::HandleMetaCurrencyChanged);
		}
	}
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
	RefreshUpgradeUI();
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

void UDefenderUpgradeComponent::UpdateStatusText()
{
	const ATDGameMode* GameMode = GetGameMode();
	UTDHUDWidget* Hud = GameMode ? GameMode->GetMatchHUDWidget() : nullptr;
	if (!Hud || !Hud->DefendersCountdown)
	{
		return;
	}

	static const TCHAR* Names[] = {
		TEXT("Basic"),
		TEXT("Archer"),
		TEXT("Poison"),
		TEXT("Strong"),
		TEXT("Tower")
	};
	TArray<FString> StatusLines;
	for (int32 Index = 0; Index < (int32)EUpgrade::Count; ++Index)
	{
		const EUpgrade Upgrade = (EUpgrade)Index;
		const FSlot& Slot = Slots[Index];
		const bool bQueued = Slot.Phase == EPhase::Ready && IsPending(Upgrade);
		const bool bActive = Slot.Phase == EPhase::Active;
		const bool bDisabledAfterWave = Slot.Phase == EPhase::DisabledAfterWave;
		const TCHAR* State = bDisabledAfterWave ? TEXT("DISABLED")
			: bQueued ? TEXT("QUEUED")
			: bActive ? TEXT("ENABLED")
			: TEXT("DISABLED");
		FString Status = FString::Printf(TEXT("%s: %s"), Names[Index], State);
		StatusLines.Add(MoveTemp(Status));
	}

	Hud->DefendersCountdown->SetText(FText::FromString(FString::Join(StatusLines, TEXT("\n"))));
	Hud->DefendersCountdown->SetColorAndOpacity(FLinearColor::White);
	Hud->DefendersCountdown->SetRenderOpacity(1.0f);
	Hud->DefendersCountdown->SetShadowOffset(FVector2D(2.0f, 2.0f));
	Hud->DefendersCountdown->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	Hud->DefendersCountdown->SetAutoWrapText(false);
	FSlateFontInfo StatusFont = Hud->DefendersCountdown->GetFont();
	StatusFont.Size = FMath::Max(StatusFont.Size, 18);
	Hud->DefendersCountdown->SetFont(StatusFont);
	if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Hud->DefendersCountdown->Slot))
	{
		Slot->SetAutoSize(true);
		Slot->SetZOrder(FMath::Max(Slot->GetZOrder(), 100));
	}
	Hud->DefendersCountdown->SetVisibility(ESlateVisibility::HitTestInvisible);
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
