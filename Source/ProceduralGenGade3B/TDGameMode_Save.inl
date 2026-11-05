// TDGameMode_Save.inl - included by TDGameMode.cpp (not compiled alone).
// Wave checkpoints: capture the match at the end of a wave, write it to the save slot and
// restore it when the player presses Resume. The middle of a wave is not saved, because live
// enemies, projectiles and poison clouds have no saveable state; resuming starts the next wave.

UTDSaveGame* ATDGameMode::CaptureCheckpoint(int32 WavesCleared) const
{
	UTDSaveGame* Save = NewObject<UTDSaveGame>(const_cast<ATDGameMode*>(this));
	Save->Wallet = GetMetaWallet();
	Save->bHasMatch = true;
	Save->LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	Save->TerrainSeed = Terrain ? Terrain->Seed : 0;
	Save->WavesCleared = WavesCleared;
	Save->Loot = Resources;
	Save->BeamUpgradeLevel = BeamUpgradeLevel;
	Save->DefendersPlaced = MatchDefendersPlaced;
	Save->HitsLanded = MatchHitsLanded;
	Save->EnemiesKilled = MatchEnemiesKilled;
	Save->PaidMatchRewards = PaidMatchRewards;

	// Damage taken, not raw health, so restoring never heals and works with temporary upgrades.
	if (Tower && Tower->HealthComponent)
	{
		Save->TowerDamageTaken = Tower->HealthComponent->MaxHealth - Tower->HealthComponent->GetCurrentHealth();
	}

	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		const ADefender* Defender = *It;
		if (!Defender->HealthComponent || Defender->HealthComponent->IsDead() || !Defender->HasOccupiedSlot())
		{
			continue;
		}
		FTDSavedDefender& Entry = Save->Defenders.AddDefaulted_GetRef();
		Entry.DefenderClass = FSoftClassPath(Defender->GetClass());
		Entry.SlotLocation = Defender->GetOccupiedSlotLocation();
		Entry.DamageTaken = Defender->HealthComponent->MaxHealth - Defender->HealthComponent->GetCurrentHealth();
	}

	if (DefenderUpgrades)
	{
		DefenderUpgrades->GetSaveState(Save->UpgradePhases, Save->UpgradeSecondsLeft);
	}
	return Save;
}

void ATDGameMode::WriteCheckpoint(int32 WavesCleared)
{
	UTDSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr;
	if (!Saves || !Terrain || !Tower || WavesCleared <= 0)
	{
		return;
	}
	LastCheckpoint = CaptureCheckpoint(WavesCleared);
	if (Saves->WriteSave(LastCheckpoint) && WarningBannerWidget)
	{
		WarningBannerWidget->ShowWarning(TEXT("Progress saved"));
	}
}

void ATDGameMode::WriteMatchFinishedSave()
{
	UTDSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr;
	if (!Saves)
	{
		return;
	}
	// Keep the earned currencies, but there is no match left to resume.
	UTDSaveGame* Save = NewObject<UTDSaveGame>(this);
	Save->Wallet = GetMetaWallet();
	Save->bHasMatch = false;
	Saves->WriteSave(Save);
	LastCheckpoint = nullptr;
}

void ATDGameMode::SaveGameNow()
{
	UTDSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr;
	if (!Saves)
	{
		return;
	}
	// Mid-wave this writes the last wave-end checkpoint rather than a half-finished wave.
	if (!LastCheckpoint)
	{
		if (WarningBannerWidget)
		{
			WarningBannerWidget->ShowWarning(TEXT("Clear a wave first - the game saves at the end of each wave"));
		}
		return;
	}
	if (Saves->WriteSave(LastCheckpoint) && WarningBannerWidget)
	{
		WarningBannerWidget->ShowWarning(TEXT("Game saved"));
	}
}

void ATDGameMode::ApplyResumeSave(UTDSaveGame* Save)
{
	if (!Save || !Terrain || !Tower || !WaveManager)
	{
		return;
	}

	if (UTDMetaProgressionSubsystem* Meta = GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>())
	{
		Meta->SetWallet(Save->Wallet);
	}
	Resources = FMath::Max(0, Save->Loot);
	OnResourcesChanged.Broadcast(Resources);
	MatchDefendersPlaced = Save->DefendersPlaced;
	MatchHitsLanded = Save->HitsLanded;
	MatchEnemiesKilled = Save->EnemiesKilled;
	PaidMatchRewards = Save->PaidMatchRewards;

	// Permanent beam upgrades, applied the same way TryUpgradeTowerBeam adds them.
	BeamUpgradeLevel = FMath::Clamp(Save->BeamUpgradeLevel, 0, MaxBeamUpgradeLevel);
	Tower->AttackDamage += BeamUpgradeLevel * BeamUpgradeDamageBonus;

	auto RestoreDamage = [](UHealthComponent* Health, float DamageTaken)
	{
		if (Health && !Health->IsDead() && DamageTaken > 0.0f)
		{
			Health->ApplyDamage(FMath::Min(DamageTaken, Health->GetCurrentHealth() - 1.0f));
		}
	};
	RestoreDamage(Tower->HealthComponent, Save->TowerDamageTaken);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FTDSavedDefender& Entry : Save->Defenders)
	{
		UClass* DefenderClass = Entry.DefenderClass.TryLoadClass<ADefender>();
		// A pad can only hold one defender, so a duplicate entry is never spawned twice.
		if (!DefenderClass || Terrain->IsSlotOccupied(Entry.SlotLocation))
		{
			continue;
		}
		if (ADefender* Defender = GetWorld()->SpawnActor<ADefender>(
			DefenderClass, Entry.SlotLocation + FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator, SpawnParams))
		{
			Defender->SetOccupiedSlot(Entry.SlotLocation);
			Terrain->SetSlotOccupied(Entry.SlotLocation, true);
			RestoreDamage(Defender->HealthComponent, Entry.DamageTaken);
		}
	}

	if (DefenderUpgrades)
	{
		DefenderUpgrades->RestoreSaveState(Save->UpgradePhases, Save->UpgradeSecondsLeft);
	}

	LastCheckpoint = Save;
	RefreshMetaHUD();
	UE_LOG(LogTemp, Display, TEXT("TDSave: resumed after wave %d with %d defenders."), Save->WavesCleared, Save->Defenders.Num());

	// Same as pressing Next Wave on the results screen of the saved wave.
	WaveManager->SetCompletedWaves(Save->WavesCleared);
	ContinueToNextWave();
}

void UDefenderUpgradeComponent::GetSaveState(TArray<uint8>& OutPhases, TArray<float>& OutSecondsLeft) const
{
	const FTimerManager& Timers = GetWorld()->GetTimerManager();
	for (const DefenderUpgrades::FSlot& Slot : Slots)
	{
		OutPhases.Add((uint8)Slot.Phase);
		OutSecondsLeft.Add(Slot.Phase == DefenderUpgrades::EPhase::Ready ? 0.0f : Timers.GetTimerRemaining(Slot.PhaseTimer));
	}
}

void UDefenderUpgradeComponent::RestoreSaveState(const TArray<uint8>& Phases, const TArray<float>& SecondsLeft)
{
	using namespace DefenderUpgrades;
	for (int32 Index = 0; Index < (int32)EUpgrade::Count && Index < Phases.Num() && Index < SecondsLeft.Num(); ++Index)
	{
		const EUpgrade Upgrade = (EUpgrade)Index;
		FSlot& Slot = Slots[Index];
		if (Slot.Phase != EPhase::Ready || SecondsLeft[Index] <= 0.0f)
		{
			continue;
		}

		if (Phases[Index] == (uint8)EPhase::Active)
		{
			// Re-apply the boost without charging again; it was paid for before the save.
			TArray<AActor*> Targets;
			GatherTargets(Upgrade, Targets);
			for (AActor* Target : Targets)
			{
				ApplyBoost(Upgrade, Target);
			}
			Slot.Phase = EPhase::Active;
			GetWorld()->GetTimerManager().SetTimer(Slot.PhaseTimer,
				FTimerDelegate::CreateUObject(this, &UDefenderUpgradeComponent::EndActive, Upgrade), SecondsLeft[Index], false);
		}
		else if (Phases[Index] == (uint8)EPhase::Cooldown)
		{
			Slot.Phase = EPhase::Cooldown;
			GetWorld()->GetTimerManager().SetTimer(Slot.PhaseTimer,
				FTimerDelegate::CreateUObject(this, &UDefenderUpgradeComponent::EndCooldown, Upgrade), SecondsLeft[Index], false);
		}
	}
	RefreshButtons();
}
