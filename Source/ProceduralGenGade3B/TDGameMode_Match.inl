// TDGameMode_Match.inl - economy, meta currency and match results.
// This file is included by TDGameMode.cpp and is not compiled on its own.

void ATDGameMode::AddResources(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	Resources += Amount;
	OnResourcesChanged.Broadcast(Resources);
}

// Spends the resources if the player can afford it. Returns false if there isn't enough.
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

// Called by an enemy when it dies. Gives the player its bounty.
void ATDGameMode::NotifyEnemyKilled(AEnemy* DeadEnemy)
{
	++MatchEnemiesKilled;
	if (DeadEnemy)
	{
		AddResources(DeadEnemy->ResourceReward);
	}
	RefreshMetaHUD();
}

// Counts a hit on an enemy from the tower, a defender or a projectile.
void ATDGameMode::NotifyEnemyHit()
{
	++MatchHitsLanded;
	RefreshMetaHUD();
}

// Counts a defender that was placed this match.
void ATDGameMode::NotifyDefenderPlaced()
{
	++MatchDefendersPlaced;
	RefreshMetaHUD();
}

// Ensure starting meta wallet.
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

// The meta currency wallet that is kept between matches.
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

// Spends meta currency if the wallet has enough.
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

// True when the wallet has enough for a defender or upgrade.
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

// True while paused, after a loss or after a win. The player can't place or upgrade then.
bool ATDGameMode::IsInteractionBlocked() const
{
	return bPaused || bGameOver || IsVictory();
}

// Spends Light Lanterns to make the tower beam stronger for the rest of this match.
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
	// Add rather than assign, so a running temporary tower upgrade is not overwritten.
	Tower->AttackDamage += BeamUpgradeDamageBonus;
	Tower->PlayUpgradeVFX(); //  beam upgrade energy
	SaveCurrentWaveCheckpoint();
	RefreshMetaHUD();
	return true;
}

// Current match score, worked out from placements, hits, kills and surviving defenders.
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

// Sends the current meta currency totals to the match HUD.
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

// Finalize match result.
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

// Runs when the final wave is won. Does nothing if the results already showed when that wave was cleared.
void ATDGameMode::HandleMatchVictory()
{
	if (bGameOver)
	{
		return;
	}

	if (ATDMusicManager* MusicManager = ATDMusicManager::GetOrCreate(this))
	{
		MusicManager->PlayVictoryMusic();
	}
	if (!bWaveResultsVisible)
	{
		FinalizeMatchResult(true);
	}

	if (!bVictoryHealthRewardGranted)
	{
		if (UTDSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr)
		{
			const float ScaledBonus = FMath::GetMappedRangeValueClamped(
				FVector2D(0.0f, 10000.0f), FVector2D(20.0f, 100.0f), static_cast<float>(LastMatchResult.Score));
			const int32 HealthBonus = FMath::RoundToInt(ScaledBonus);
			if (Saves->AddTowerMaxHealthBonus(HealthBonus))
			{
				bVictoryHealthRewardGranted = true;
				UE_LOG(LogTemp, Display, TEXT("Victory health upgrade: +%d Tower max HP for score %d."), HealthBonus, LastMatchResult.Score);
			}
		}
	}

	if (bWaveResultsVisible)
	{
		return;
	}
	ShowEndScreen(true);
	PlayMatchVFX(VictoryVFX, TDVFX::Gold, 4.0f); //  victory celebration
}

// Called by the tower when it dies. Ends the game.
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

	if (ATDMusicManager* MusicManager = ATDMusicManager::GetOrCreate(this))
	{
		MusicManager->PlayDefeatMusic();
	}
	OnGameOver.Broadcast();
	FinalizeMatchResult(false);
	ShowEndScreen(false);
	PlayMatchVFX(DefeatVFX, TDVFX::Corruption, 4.0f); //  subtle dark defeat particles
	UE_LOG(LogTemp, Display, TEXT("TDGameMode: GAME OVER - the tower was destroyed."));
}
