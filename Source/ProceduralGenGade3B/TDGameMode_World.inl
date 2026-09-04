// TDGameMode_World.inl — included by the parent .cpp (not compiled alone).

AProceduralTerrain* ATDGameMode::FindTerrain() const
{
	for (TActorIterator<AProceduralTerrain> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

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
			BaseTowerAttackDamage = Tower->AttackDamage;
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: tower spawn attempt %d/%d failed."), Attempt, MaxTowerSpawnAttempts);
		Terrain->RandomizeAndRegenerate();
	}

	return false;
}

void ATDGameMode::SpawnBuildPadMarkers()
{
	SpawnBuildPadMarkersFromIndex(0);
}

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

void ATDGameMode::HandleWaveComplete(int32 WaveNumber)
{
	if (bGameOver)
	{
		return;
	}

	if (WaveManager)
	{
		WaveManager->HoldForResultsScreen();
	}

	const int32 TotalWaves = WaveManager ? WaveManager->GetTotalWaves() : 0;
	const bool bFinalWave = TotalWaves > 0 && WaveNumber >= TotalWaves;

	FinalizeMatchResult(true, WaveNumber);
	ShowEndScreen(true);

	if (bFinalWave && WaveManager)
	{
		WaveManager->DeclareVictory();
	}

	UE_LOG(LogTemp, Display, TEXT("TDGameMode: wave %d/%d complete â€” showing results."), WaveNumber, TotalWaves);
}

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

	if (WaveManager)
	{
		WaveManager->ContinueToNextWave();
	}
}

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
	WaveManager->RetryCurrentWave();
	RefreshMetaHUD();
}

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
