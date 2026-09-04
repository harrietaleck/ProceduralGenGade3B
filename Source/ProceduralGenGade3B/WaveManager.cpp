// WaveManager.cpp — see WaveManager.h for the overview.

#include "WaveManager.h"
#include "EnemySpawner.h"
#include "Enemy.h"
#include "HealthComponent.h"

AWaveManager::AWaveManager()
{
	// All pacing is timer-driven, so no per-frame tick is needed.
	PrimaryActorTick.bCanEverTick = false;

	EnsureDefaultWaveTable();
}

void AWaveManager::EnsureDefaultWaveTable()
{
	// Only fill in the default Part 1 progression if nothing was configured — an instance
	// (or a Blueprint child) that already set up its own Waves array is left untouched.
	if (Waves.Num() > 0)
	{
		return;
	}

	// Multipliers here are derived directly from the Basic Enemy's own base stats (100 HP,
	// 10 damage, 25 Loot) against the assignment's exact per-wave target numbers, e.g. Wave 2's
	// 120 HP / 100 base HP = 1.2x. Storing multipliers rather than absolute numbers means the
	// wave table automatically stays correct if the Basic Enemy's base stats are ever retuned.
	FWaveData Wave1;
	Wave1.EnemyCount = 5;
	Wave1.SpawnDelay = 2.0f;
	Wave1.HealthMultiplier = 1.0f;
	Wave1.DamageMultiplier = 1.0f;
	Wave1.SpeedMultiplier = 1.0f;
	Wave1.RewardMultiplier = 1.0f;

	FWaveData Wave2;
	Wave2.EnemyCount = 8;
	Wave2.SpawnDelay = 1.8f;
	Wave2.HealthMultiplier = 1.2f;
	Wave2.DamageMultiplier = 1.2f;
	Wave2.SpeedMultiplier = 1.0f;
	Wave2.RewardMultiplier = 1.2f;

	FWaveData Wave3;
	Wave3.EnemyCount = 12;
	Wave3.SpawnDelay = 1.5f;
	Wave3.HealthMultiplier = 1.4f;
	Wave3.DamageMultiplier = 1.4f;
	Wave3.SpeedMultiplier = 1.0f;
	Wave3.RewardMultiplier = 1.4f;

	// Part 1 victory after wave 3 — keeps the demo shorter while still showing escalation.
	Waves = { Wave1, Wave2, Wave3 };
}

void AWaveManager::Initialize(AEnemySpawner* InSpawner, bool bStartImmediately)
{
	Spawner = InSpawner;

	if (bStartImmediately)
	{
		StartWaves();
	}
}

void AWaveManager::StartWaves()
{
	if (!Spawner || bStopped)
	{
		return;
	}

	// Only (re)start from scratch if nothing has begun yet — matches the old contract where
	// calling this again acts as a no-op resume rather than restarting wave 1.
	if (CurrentWaveIndex < 0)
	{
		BeginNextWave();
	}
}

void AWaveManager::StopWaves()
{
	bStopped = true;
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(BreakTimerHandle);
}

void AWaveManager::HoldForResultsScreen()
{
	GetWorldTimerManager().ClearTimer(BreakTimerHandle);
}

void AWaveManager::ContinueToNextWave()
{
	if (bStopped || State == EWaveState::Victory)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(BreakTimerHandle);
	BeginNextWave();
}

void AWaveManager::RetryCurrentWave()
{
	if (!Spawner || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(BreakTimerHandle);

	// Final victory calls StopWaves; explicitly reopen the manager for this replay.
	bStopped = false;
	EnemiesSpawnedThisWave = 0;
	ActiveEnemyCount = 0;
	State = EWaveState::CountingDown;
	CountdownSecondsRemaining = CountdownSeconds;

	OnEnemiesRemainingChanged.Broadcast(0);
	OnWaveCountdownTick.Broadcast(CountdownSecondsRemaining);

	if (CountdownSecondsRemaining <= 0)
	{
		CountdownTick();
		return;
	}

	GetWorldTimerManager().SetTimer(
		CountdownTimerHandle, this, &AWaveManager::CountdownTick, 1.0f, /*bLoop=*/true);
}

void AWaveManager::DeclareVictory()
{
	if (State == EWaveState::Victory)
	{
		return;
	}

	TriggerVictory();
}

void AWaveManager::BeginNextWave()
{
	if (bStopped)
	{
		return;
	}

	++CurrentWaveIndex;

	// No more configured waves -> the player has cleared everything. Win condition.
	if (CurrentWaveIndex >= Waves.Num())
	{
		TriggerVictory();
		return;
	}

	EnemiesSpawnedThisWave = 0;
	ActiveEnemyCount = 0;
	State = EWaveState::CountingDown;
	CountdownSecondsRemaining = CountdownSeconds;

	OnWaveCountdownTick.Broadcast(CountdownSecondsRemaining);

	if (CountdownSecondsRemaining <= 0)
	{
		// A zero-length countdown is valid configuration -> skip straight to spawning.
		CountdownTick();
		return;
	}

	GetWorldTimerManager().SetTimer(CountdownTimerHandle, this, &AWaveManager::CountdownTick, 1.0f, /*bLoop=*/true);
}

void AWaveManager::CountdownTick()
{
	if (bStopped)
	{
		return;
	}

	--CountdownSecondsRemaining;

	if (CountdownSecondsRemaining > 0)
	{
		OnWaveCountdownTick.Broadcast(CountdownSecondsRemaining);
		return;
	}

	// Countdown finished -> the wave is now live.
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	State = EWaveState::Active;
	OnWaveStarted.Broadcast(GetCurrentWave());
	OnEnemiesRemainingChanged.Broadcast(ActiveEnemyCount);

	// Spawn the first enemy immediately, then continue on this wave's own spawn delay.
	SpawnTick();
}

void AWaveManager::SpawnTick()
{
	if (bStopped || !Spawner || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	const FWaveData& WaveData = Waves[CurrentWaveIndex];

	// Let this wave's data pick the enemy class (Part 1: always the Basic Enemy). The spawner
	// itself is untouched — we simply set which class it should hand out next.
	if (WaveData.EnemyType)
	{
		Spawner->EnemyClass = WaveData.EnemyType;
	}

	if (AEnemy* NewEnemy = Spawner->SpawnSingleEnemy())
	{
		// Scale this enemy's stats from its own class defaults using the wave's multipliers,
		// rather than hardcoding base numbers here — the Basic Enemy stays the single source
		// of truth for what "100% difficulty" means.
		if (UHealthComponent* Health = NewEnemy->HealthComponent)
		{
			Health->MaxHealth *= WaveData.HealthMultiplier;
			Health->Heal(Health->MaxHealth); // Top current health up to the new (scaled) max.
		}
		NewEnemy->AttackDamage *= WaveData.DamageMultiplier;
		NewEnemy->MoveSpeed *= WaveData.SpeedMultiplier;
		NewEnemy->ResourceReward = FMath::RoundToInt(NewEnemy->ResourceReward * WaveData.RewardMultiplier);

		// Track this specific enemy so we know the instant it dies (event-driven — no per-
		// frame polling or GetAllActorsOfClass scans needed to know when the wave is clear).
		if (NewEnemy->HealthComponent)
		{
			NewEnemy->HealthComponent->OnDeath.AddDynamic(this, &AWaveManager::HandleTrackedEnemyDeath);
		}

		++ActiveEnemyCount;
		OnEnemiesRemainingChanged.Broadcast(ActiveEnemyCount);
	}

	++EnemiesSpawnedThisWave;

	if (EnemiesSpawnedThisWave < WaveData.EnemyCount)
	{
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AWaveManager::SpawnTick, WaveData.SpawnDelay, /*bLoop=*/false);
	}
	else
	{
		// All of this wave's enemies have been scheduled; now we just wait for them to die.
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		CheckWaveCompletion();
	}
}

void AWaveManager::HandleTrackedEnemyDeath(AActor* Killer)
{
	if (bStopped)
	{
		return;
	}

	ActiveEnemyCount = FMath::Max(0, ActiveEnemyCount - 1);
	OnEnemiesRemainingChanged.Broadcast(ActiveEnemyCount);

	CheckWaveCompletion();
}

void AWaveManager::CheckWaveCompletion()
{
	if (bStopped || State != EWaveState::Active || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	const bool bAllSpawned = EnemiesSpawnedThisWave >= Waves[CurrentWaveIndex].EnemyCount;
	if (!bAllSpawned || ActiveEnemyCount > 0)
	{
		return; // Still enemies left to spawn or still enemies alive -> not complete yet.
	}

	State = EWaveState::Complete;
	OnWaveComplete.Broadcast(GetCurrentWave());

	// Do not auto-start the next wave — GameMode shows the results screen and calls
	// ContinueToNextWave() when the player is ready.
}

void AWaveManager::TriggerVictory()
{
	State = EWaveState::Victory;
	StopWaves(); // No further countdowns/spawns/breaks once every wave is cleared.
	OnVictory.Broadcast();
}
