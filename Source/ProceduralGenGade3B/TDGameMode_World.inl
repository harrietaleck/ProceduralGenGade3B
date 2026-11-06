// TDGameMode_World.inl - spawning the tower and build pads, and wave flow.
// This file is included by TDGameMode.cpp and is not compiled on its own.

AProceduralTerrain* ATDGameMode::FindTerrain() const
{
	for (TActorIterator<AProceduralTerrain> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

// Spawns the tower at the terrain's tower location. If the spawn fails or lands in the
// wrong place, it makes new terrain and tries again. Returns false if it runs out of tries.
bool ATDGameMode::SpawnTowerWithRetry()
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const int32 MaxTowerSpawnAttempts = 5;
	const float TowerPlacementToleranceSq = FMath::Square(50.0f);
	for (int32 Attempt = 1; Attempt <= MaxTowerSpawnAttempts; ++Attempt)
	{
		if (Tower)
		{
			Tower->Destroy();
			Tower = nullptr;
		}

		const FVector TowerSpawnLocation = Terrain->GetTowerLocation() + FVector(0.0f, 0.0f, 150.0f);
		Tower = GetWorld()->SpawnActor<ATower>(TowerClass, TowerSpawnLocation, FRotator::ZeroRotator, SpawnParams);

		const bool bLocationMatches = Tower
			&& FVector::DistSquared(Tower->GetActorLocation(), TowerSpawnLocation) <= TowerPlacementToleranceSq;
		if (Tower && bLocationMatches)
		{
			if (UTDSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDSaveSubsystem>() : nullptr)
			{
				const int32 HealthBonus = Saves->GetTowerMaxHealthBonus();
				if (HealthBonus > 0 && Tower->HealthComponent)
				{
					Tower->HealthComponent->AdjustMaxHealth(static_cast<float>(HealthBonus));
				}
			}
			BaseTowerAttackDamage = Tower->AttackDamage;
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: tower spawn attempt %d/%d failed."), Attempt, MaxTowerSpawnAttempts);
		Terrain->RandomizeAndRegenerate();
	}

	return false;
}

// Spawns a platform on every build slot the terrain has right now.
void ATDGameMode::SpawnBuildPadMarkers()
{
	SpawnBuildPadMarkersFromIndex(0);
}

// Spawns markers just for the build slots added since last time. Used after the paths grow.
void ATDGameMode::SpawnBuildPadMarkersFromIndex(int32 StartSlotIndex)
{
	if (!Terrain || !BuildPadMarkerClass || StartSlotIndex < 0)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const TArray<FDefenderSlot>& Slots = Terrain->GetDefenderSlots();
	for (int32 I = StartSlotIndex; I < Slots.Num(); ++I)
	{
		const FDefenderSlot& Slot = Slots[I];
		if (ABuildPadMarker* Marker = GetWorld()->SpawnActor<ABuildPadMarker>(
			BuildPadMarkerClass, Slot.Location + FVector(0.0f, 0.0f, 4.0f), FRotator::ZeroRotator, SpawnParams))
		{
			BuildPadMarkers.Add(Marker);
		}
	}
}

// Takes upkeep for every living defender after a wave ends.
void ATDGameMode::ApplyDefenderUpkeep()
{
	int32 TotalUpkeep = 0;
	int32 DefenderCount = 0;
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		ADefender* Defender = *It;
		if (!Defender || !Defender->HealthComponent || Defender->HealthComponent->IsDead())
		{
			continue;
		}

		++DefenderCount;
		const int32 PerDefender = Defender->UpkeepPerWave > 0 ? Defender->UpkeepPerWave : DefenderUpkeepPerWave;
		TotalUpkeep += PerDefender;
	}

	if (TotalUpkeep <= 0)
	{
		return;
	}

	Resources = FMath::Max(0, Resources - TotalUpkeep);
	OnResourcesChanged.Broadcast(Resources);
	UE_LOG(LogTemp, Display, TEXT("TDGameMode: defender upkeep charged %d Loot (%d defenders)."), TotalUpkeep, DefenderCount);
}

// Runs when a wave is cleared. Grows the lanes, adds new build pads, charges upkeep and shows the results.
void ATDGameMode::HandleWaveComplete(int32 WaveNumber)
{
	if (bGameOver)
	{
		return;
	}

	if (DefenderUpgrades)
	{
		DefenderUpgrades->ResetTemporaryUpgradesForWaveEnd();
	}

	if (WaveManager)
	{
		WaveManager->HoldForResultsScreen();
	}

	const int32 TotalWaves = WaveManager ? WaveManager->GetTotalWaves() : 0;
	const bool bFinalWave = TotalWaves > 0 && WaveNumber >= TotalWaves;

	FinalizeMatchResult(true, WaveNumber);
	ShowEndScreen(true);

	// Autosave once the wave's rewards are banked. A won match has nothing left to resume.
	if (bFinalWave)
	{
		WriteMatchFinishedSave();
		PlayMatchVFX(VictoryVFX, TDVFX::Gold, 4.0f); //  victory celebration
	}
	else
	{
		WriteCheckpoint(WaveNumber);
		PlayMatchVFX(WaveTransitionVFX, TDVFX::Turquoise, 2.0f); //  wave transition
	}

	if (bFinalWave && WaveManager)
	{
		WaveManager->DeclareVictory();
	}

	UE_LOG(LogTemp, Display, TEXT("TDGameMode: wave %d/%d complete - showing results."), WaveNumber, TotalWaves);
}

// Hides the results screen between waves and starts the next wave.
void ATDGameMode::ContinueToNextWave()
{
	if (bGameOver || IsVictory())
	{
		return;
	}

	if (Terrain)
	{
		const int32 OldSlotCount = Terrain->GetDefenderSlots().Num();
		const int32 ExtendedLanes = Terrain->ExpandWorldAfterWave();
		if (ExtendedLanes > 0)
		{
			SpawnBuildPadMarkersFromIndex(OldSlotCount);
		}
	}
	ApplyDefenderUpkeep();

	HideEndScreens();
	bPaused = false;
	bWaveResultsVisible = false;
	UGameplayStatics::SetGamePaused(this, false);
	RestoreGameplayInput();
	if (ATDMusicManager* MusicManager = ATDMusicManager::GetOrCreate(this))
	{
		MusicManager->PlayGameplayMusic();
	}

	if (WaveManager)
	{
		WaveManager->ContinueToNextWave();
	}
}

// Hides the victory results and plays the wave we just finished again.
void ATDGameMode::RetryCurrentWave()
{
	if (bGameOver || !WaveManager)
	{
		return;
	}

	HideEndScreens();
	bPaused = false;
	bWaveResultsVisible = false;
	UGameplayStatics::SetGamePaused(this, false);
	RestoreGameplayInput();
	if (ATDMusicManager* MusicManager = ATDMusicManager::GetOrCreate(this))
	{
		MusicManager->PlayGameplayMusic();
	}
	WaveManager->RetryCurrentWave();
	RefreshMetaHUD();
}

// Destroys the tower and all build pad markers spawned so far. This way a failed world
// check can start over clean, without old actors left behind from the last try.
void ATDGameMode::DestroySpawnedWorldActors()
{
	if (Tower)
	{
		Tower->Destroy();
		Tower = nullptr;
	}
	for (ABuildPadMarker* Marker : BuildPadMarkers)
	{
		if (Marker)
		{
			Marker->Destroy();
		}
	}
	BuildPadMarkers.Reset();
}

// Last check before enemies and the HUD start. It looks at the terrain, tower, build pads
// and navigation as they were really spawned, not just the terrain's own check during generation.
bool ATDGameMode::ValidateWorldBeforeGameplay() const
{
	if (!Terrain || Terrain->GetEnemyPaths().Num() < 3 || Terrain->GetDefenderSlots().Num() == 0)
	{
		return false;
	}

	if (!Tower)
	{
		return false;
	}
	if (FMath::Abs(Tower->GetActorLocation().Z - (Terrain->GetTowerLocation().Z + 150.0f)) > 50.0f)
	{
		return false;
	}

	if (BuildPadMarkers.Num() != Terrain->GetDefenderSlots().Num())
	{
		return false;
	}
	for (const ABuildPadMarker* Marker : BuildPadMarkers)
	{
		if (!Marker)
		{
			return false;
		}
	}

	const FBox TowerBounds = Tower->GetComponentsBoundingBox(true);
	for (const ABuildPadMarker* Marker : BuildPadMarkers)
	{
		if (TowerBounds.Intersect(Marker->GetComponentsBoundingBox(true)))
		{
			return false;
		}
	}

	Terrain->ValidatePathfinding();
	return true;
}
