// TDGameMode_Match.inl — included by the parent .cpp (not compiled alone).

void ATDGameMode::AddResources(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	Resources += Amount;
	OnResourcesChanged.Broadcast(Resources);
}

bool ATDGameMode::TrySpendResources(int32 Amount)
{
	if (Amount < 0 || Resources < Amount)
	{
		return false;
	}
	Resources -= Amount;
	OnResourcesChanged.Broadcast(Resources);
	return true;
}

void ATDGameMode::NotifyEnemyKilled(AEnemy* DeadEnemy)
{
	++MatchEnemiesKilled;
	if (DeadEnemy)
	{
		AddResources(DeadEnemy->ResourceReward);
	}
	RefreshMetaHUD();
}

void ATDGameMode::NotifyEnemyHit()
{
	++MatchHitsLanded;
	RefreshMetaHUD();
}

void ATDGameMode::NotifyDefenderPlaced()
{
	++MatchDefendersPlaced;
	RefreshMetaHUD();
}

void ATDGameMode::EnsureStartingMetaWallet()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			const FMetaCurrencyRewards Wallet = Meta->GetWallet();
			const bool bWalletEmpty = Wallet.ForestEssence == 0
				&& Wallet.WoodenMight == 0
				&& Wallet.GemStones == 0
				&& Wallet.LightLanterns == 0;
			if (bWalletEmpty)
			{
				Meta->AddRewards(StartingMetaWallet);
			}
		}
	}
}

FMetaCurrencyRewards ATDGameMode::GetMetaWallet() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			return Meta->GetWallet();
		}
	}
	return FMetaCurrencyRewards();
}

bool ATDGameMode::TrySpendMeta(const FMetaCurrencyRewards& Cost)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			if (Meta->TrySpend(Cost))
			{
				RefreshMetaHUD();
				return true;
			}
		}
	}
	return false;
}

bool ATDGameMode::CanAffordMeta(const FMetaCurrencyRewards& Cost) const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			return Meta->CanAfford(Cost);
		}
	}
	return false;
}

bool ATDGameMode::IsInteractionBlocked() const
{
	return bPaused || bGameOver || IsVictory();
}

bool ATDGameMode::TryUpgradeTowerBeam()
{
	if (!Tower || bGameOver || IsVictory())
	{
		return false;
	}

	if (BeamUpgradeLevel >= MaxBeamUpgradeLevel)
	{
		ShowInsufficientFundsWarning();
		return false;
	}

	FMetaCurrencyRewards Cost;
	Cost.LightLanterns = BeamUpgradeLanternCost;
	if (!TrySpendMeta(Cost))
	{
		if (WarningBannerWidget)
		{
			WarningBannerWidget->ShowWarning(TEXT("Not enough Light Lanterns for beam upgrade"));
		}
		return false;
	}

	if (BaseTowerAttackDamage <= 0.0f)
	{
		BaseTowerAttackDamage = Tower->AttackDamage;
	}

	++BeamUpgradeLevel;
	Tower->AttackDamage = BaseTowerAttackDamage + BeamUpgradeLevel * BeamUpgradeDamageBonus;
	RefreshMetaHUD();
	return true;
}

int32 ATDGameMode::GetLiveMatchScore() const
{
	int32 SurvivingDefenders = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADefender> It(World); It; ++It)
		{
			const ADefender* Defender = *It;
			if (Defender && Defender->HealthComponent && !Defender->HealthComponent->IsDead())
			{
				++SurvivingDefenders;
			}
		}
	}

	return (MatchDefendersPlaced * 10)
		+ (MatchHitsLanded * 30)
		+ (MatchEnemiesKilled * 20)
		+ (SurvivingDefenders * 5);
}

void ATDGameMode::RefreshMetaHUD() const
{
	if (!MatchHUDWidget)
	{
		return;
	}

	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		MatchHUDWidget->RefreshMetaCurrency(GetMetaWallet(), BeamUpgradeLevel, PC->IsPlacingStrongDefender());
	}
	else
	{
		MatchHUDWidget->RefreshMetaCurrency(GetMetaWallet(), BeamUpgradeLevel, false);
	}
}

void ATDGameMode::FinalizeMatchResult(bool bVictory, int32 WavesClearedOverride)
{
	int32 WavesCleared = 0;
	const int32 TotalWaves = WaveManager ? WaveManager->GetTotalWaves() : 0;
	if (WavesClearedOverride >= 0)
	{
		WavesCleared = WavesClearedOverride;
	}
	else if (WaveManager)
	{
		if (bVictory)
		{
			WavesCleared = TotalWaves;
		}
		else
		{
			const EWaveState WaveState = WaveManager->GetWaveState();
			const int32 CurrentWave = WaveManager->GetCurrentWave();
			WavesCleared = (WaveState == EWaveState::Complete)
				? CurrentWave
				: FMath::Max(0, CurrentWave - 1);
		}
	}

	float TowerHealth = 0.0f;
	float TowerMaxHealth = 1.0f;
	if (Tower && Tower->HealthComponent)
	{
		TowerHealth = Tower->HealthComponent->GetCurrentHealth();
		TowerMaxHealth = Tower->HealthComponent->MaxHealth;
	}

	int32 SurvivingDefenders = 0;
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		const ADefender* Defender = *It;
		if (Defender && Defender->HealthComponent && !Defender->HealthComponent->IsDead())
		{
			++SurvivingDefenders;
		}
	}

	const FMetaCurrencyRewards WalletBeforePayout = GetMetaWallet();

	LastMatchResult = UTDMatchRewards::BuildMatchResult(
		bVictory,
		TowerHealth,
		TowerMaxHealth,
		WavesCleared,
		TotalWaves,
		MatchDefendersPlaced,
		MatchHitsLanded,
		MatchEnemiesKilled,
		SurvivingDefenders);
	LastMatchResult.Wallet = WalletBeforePayout;

	FMetaCurrencyRewards Delta;
	Delta.ForestEssence = FMath::Max(0, LastMatchResult.Rewards.ForestEssence - PaidMatchRewards.ForestEssence);
	Delta.WoodenMight = FMath::Max(0, LastMatchResult.Rewards.WoodenMight - PaidMatchRewards.WoodenMight);
	Delta.GemStones = FMath::Max(0, LastMatchResult.Rewards.GemStones - PaidMatchRewards.GemStones);
	Delta.LightLanterns = FMath::Max(0, LastMatchResult.Rewards.LightLanterns - PaidMatchRewards.LightLanterns);

	PaidMatchRewards = LastMatchResult.Rewards;
	LastMatchResult.Rewards = Delta;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTDMetaProgressionSubsystem* Meta = GI->GetSubsystem<UTDMetaProgressionSubsystem>())
		{
			Meta->AddRewards(Delta);
		}
	}
}

void ATDGameMode::HandleMatchVictory()
{
	if (bGameOver || bWaveResultsVisible)
	{
		return;
	}

	FinalizeMatchResult(true);
	ShowEndScreen(true);
}

void ATDGameMode::NotifyTowerDestroyed()
{
	if (bGameOver)
	{
		return;
	}
	bGameOver = true;

	if (WaveManager)
	{
		WaveManager->StopWaves();
	}
	if (Spawner)
	{
		Spawner->StopSpawning();
	}

	OnGameOver.Broadcast();
	FinalizeMatchResult(false);
	ShowEndScreen(false);
	UE_LOG(LogTemp, Display, TEXT("TDGameMode: GAME OVER - the tower was destroyed."));
}
