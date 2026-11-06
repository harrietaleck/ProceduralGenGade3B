// TDGameMode_Save.inl - included by TDGameMode.cpp (not compiled alone).
// Checkpoints restart the saved wave because enemies and projectiles are not serialized.

UTDSaveGame* ATDGameMode::CaptureCheckpoint(int32 WavesCleared, bool bResumeAtCurrentWave) const
{
	UTDSaveGame* Save = NewObject<UTDSaveGame>(const_cast<ATDGameMode*>(this));
	Save->Wallet = GetMetaWallet();
	Save->bHasMatch = true;
	Save->LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	Save->TerrainSeed = Terrain ? Terrain->Seed : 0;
	Save->WavesCleared = WavesCleared;
	Save->bResumeAtCurrentWave = bResumeAtCurrentWave;
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

void ATDGameMode::WriteCheckpoint(int32 WavesCleared, bool bResumeAtCurrentWave)
{
	UTDSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr;
	if (!Saves || !Terrain || !Tower || WavesCleared < 0)
	{
		return;
	}
	LastCheckpoint = CaptureCheckpoint(WavesCleared, bResumeAtCurrentWave);
	if (Saves->WriteSave(LastCheckpoint) && WarningBannerWidget)
	{
		WarningBannerWidget->ShowWarning(TEXT("Progress saved"));
	}
}

void ATDGameMode::SaveCurrentWaveCheckpoint()
{
	if (WaveManager)
	{
		WriteCheckpoint(FMath::Max(0, WaveManager->GetCurrentWave() - 1), true);
	}
}

void ATDGameMode::HandleWaveStartedForCheckpoint(int32 WaveNumber)
{
	WriteCheckpoint(FMath::Max(0, WaveNumber - 1), true);
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
	// This writes the last consistent checkpoint and restarts its wave from the beginning.
	if (!LastCheckpoint)
	{
		if (WarningBannerWidget)
		{
			WarningBannerWidget->ShowWarning(TEXT("The current wave has not been saved yet"));
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
		Meta->ClearPendingUpgrades();
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
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TArray<TPair<UHealthComponent*, float>> SavedDamage;
	SavedDamage.Emplace(Tower->HealthComponent, Save->TowerDamageTaken);
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
			SavedDamage.Emplace(Defender->HealthComponent, Entry.DamageTaken);
		}
	}

	if (DefenderUpgrades)
	{
		DefenderUpgrades->RestoreSaveState(Save->UpgradePhases, Save->UpgradeSecondsLeft);
	}
	for (const TPair<UHealthComponent*, float>& Entry : SavedDamage)
	{
		RestoreDamage(Entry.Key, Entry.Value);
	}

	LastCheckpoint = Save;
	RefreshMetaHUD();
	UE_LOG(LogTemp, Display, TEXT("TDSave: resumed after wave %d with %d defenders."), Save->WavesCleared, Save->Defenders.Num());

	// A wave-start checkpoint restarts that wave as-is; an end-of-wave checkpoint advances once.
	WaveManager->SetCompletedWaves(Save->WavesCleared);
	if (Save->bResumeAtCurrentWave)
	{
		WaveManager->ContinueToNextWave();
	}
	else
	{
		ContinueToNextWave();
	}
}

void UDefenderUpgradeComponent::GetSaveState(TArray<uint8>& OutPhases, TArray<float>& OutSecondsLeft) const
{
	for (int32 Index = 0; Index < (int32)DefenderUpgrades::EUpgrade::Count; ++Index)
	{
		const DefenderUpgrades::EUpgrade Upgrade = (DefenderUpgrades::EUpgrade)Index;
		const DefenderUpgrades::FSlot& Slot = Slots[Index];
		const uint8 PendingBit = IsPending(Upgrade) ? 0x80 : 0;
		OutPhases.Add((uint8)Slot.Phase | PendingBit);
		OutSecondsLeft.Add((float)Slot.CooldownSecondsRemaining);
	}
}

void UDefenderUpgradeComponent::RestoreSaveState(const TArray<uint8>& Phases, const TArray<float>& SecondsLeft)
{
	UWorld* World = GetWorld();
	UTDMetaProgressionSubsystem* Meta = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UTDMetaProgressionSubsystem>()
		: nullptr;
	if (Meta)
	{
		Meta->ClearPendingUpgrades();
	}

	for (int32 Index = 0; Index < (int32)DefenderUpgrades::EUpgrade::Count; ++Index)
	{
		DefenderUpgrades::FSlot& Slot = Slots[Index];
		if (World)
		{
			World->GetTimerManager().ClearTimer(Slot.CooldownTimer);
		}

		const uint8 EncodedPhase = Phases.IsValidIndex(Index) ? Phases[Index] : (uint8)DefenderUpgrades::EPhase::Ready;
		const uint8 PhaseValue = EncodedPhase & 0x7f;
		Slot.Phase = PhaseValue <= (uint8)DefenderUpgrades::EPhase::DisabledAfterWave
			? (DefenderUpgrades::EPhase)PhaseValue
			: DefenderUpgrades::EPhase::Ready;
		Slot.CooldownSecondsRemaining = SecondsLeft.IsValidIndex(Index)
			? FMath::Max(0, FMath::RoundToInt(SecondsLeft[Index]))
			: 0;

		if ((EncodedPhase & 0x80) != 0 && Meta)
		{
			Meta->AddPendingUpgrade(Index);
		}

		if (Slot.Phase == DefenderUpgrades::EPhase::Active)
		{
			TArray<AActor*> Targets;
			GatherTargets((DefenderUpgrades::EUpgrade)Index, Targets);
			StartActive((DefenderUpgrades::EUpgrade)Index, Targets);
		}
		else if (Slot.CooldownSecondsRemaining > 0 && World)
		{
			const int32 UpgradeIndex = Index;
			World->GetTimerManager().SetTimer(
				Slot.CooldownTimer,
				FTimerDelegate::CreateWeakLambda(this, [this, UpgradeIndex]()
				{
					TickUpgradeCooldown(UpgradeIndex);
				}),
				1.0f,
				true);
		}
	}
	ActivatePendingUpgrades();
	RefreshUpgradeUI();
}
