// WaveManager.cpp — see WaveManager.h for the overview.

#include "WaveManager.h"
#include "EnemySpawner.h"
#include "Enemy.h"
#include "HealthComponent.h"
#include "TimerManager.h"

AWaveManager::AWaveManager()
{
	// All pacing is timer-driven, so no per-frame tick is needed.
	PrimaryActorTick.bCanEverTick = false;

	Director = CreateDefaultSubobject<UWaveDirector>(TEXT("WaveDirector"));

	EnsureDefaultWaveTable();
}

void AWaveManager::EnsureDefaultWaveTable()
{
	// Fallback table used only when the adaptive director is switched off.
	if (Waves.Num() > 0)
	{
		return;
	}

	FWaveData Wave1;
	Wave1.EnemyCount = 5;
	Wave1.SpawnDelay = 2.0f;

	FWaveData Wave2;
	Wave2.EnemyCount = 8;
	Wave2.SpawnDelay = 1.8f;
	Wave2.HealthMultiplier = 1.2f;
	Wave2.DamageMultiplier = 1.2f;
	Wave2.RewardMultiplier = 1.2f;

	FWaveData Wave3;
	Wave3.EnemyCount = 12;
	Wave3.SpawnDelay = 1.5f;
	Wave3.HealthMultiplier = 1.4f;
	Wave3.DamageMultiplier = 1.4f;
	Wave3.RewardMultiplier = 1.4f;

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

	// Calling this again after waves began acts as a no-op resume rather than restarting wave 1.
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
	GetWorldTimerManager().ClearTimer(MonitorTimerHandle);
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
	if (!Spawner || CurrentWaveIndex < 0 || CurrentWaveIndex >= GetTotalWaves())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(BreakTimerHandle);
	GetWorldTimerManager().ClearTimer(MonitorTimerHandle);

	// Final victory calls StopWaves; explicitly reopen the manager for this replay.
	bStopped = false;
	OnEnemiesRemainingChanged.Broadcast(0);

	PrepareCurrentWave();
	StartCountdown();
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

	// No more waves -> the player has survived everything. Win condition.
	if (CurrentWaveIndex >= GetTotalWaves())
	{
		TriggerVictory();
		return;
	}

	PrepareCurrentWave();
	StartCountdown();
}

void AWaveManager::PrepareCurrentWave()
{
	SpawnQueue.Reset();

	if (bUseAdaptiveDirector && Director)
	{
		const FWavePlan Plan = Director->PlanWave(GetWorld(), GetCurrentWave());
		SpawnQueue = Plan.Spawns;

		ActiveWave = FWaveData();
		ActiveWave.EnemyCount = FMath::Max(1, Plan.Spawns.Num());
		ActiveWave.HealthMultiplier = Plan.HealthMultiplier;
		ActiveWave.DamageMultiplier = Plan.DamageMultiplier;
		ActiveWave.SpeedMultiplier = Plan.SpeedMultiplier;
		ActiveWave.RewardMultiplier = Plan.RewardMultiplier;
		return;
	}

	if (Waves.IsValidIndex(CurrentWaveIndex))
	{
		ActiveWave = Waves[CurrentWaveIndex];
	}
}

void AWaveManager::StartCountdown()
{
	EnemiesSpawnedThisWave = 0;
	ActiveEnemyCount = 0;
	PackLane = INDEX_NONE;
	bReliefActive = false;
	TrackedEnemies.Reset();

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

	if (bUseAdaptiveDirector && Director)
	{
		Director->BeginWave(GetWorld());
		GetWorldTimerManager().SetTimer(MonitorTimerHandle, this, &AWaveManager::MonitorWave, 0.25f, /*bLoop=*/true);
	}

	OnWaveStarted.Broadcast(GetCurrentWave());
	OnEnemiesRemainingChanged.Broadcast(ActiveEnemyCount);

	// Spawn the first enemy immediately, then continue on the planned delays.
	SpawnTick();
}

void AWaveManager::SpawnTick()
{
	if (bStopped || !Spawner || State != EWaveState::Active)
	{
		return;
	}

	const float NextDelay = SpawnNextEnemy();

	if (EnemiesSpawnedThisWave < ActiveWave.EnemyCount)
	{
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AWaveManager::SpawnTick, NextDelay, /*bLoop=*/false);
	}
	else
	{
		// Every enemy has been sent; now we just wait for them to die.
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		CheckWaveCompletion();
	}
}

float AWaveManager::SpawnNextEnemy()
{
	AEnemy* NewEnemy = nullptr;
	float Delay = ActiveWave.SpawnDelay;
	int32 Lane = INDEX_NONE;

	if (bUseAdaptiveDirector && Director && SpawnQueue.IsValidIndex(EnemiesSpawnedThisWave))
	{
		// Director-planned spawn: type from the plan, lane chosen from live lane coverage.
		const FPlannedSpawn& Planned = SpawnQueue[EnemiesSpawnedThisWave];
		Lane = (Planned.bPackFollower && PackLane != INDEX_NONE)
			? PackLane
			: Director->ChooseLane(GetWorld(), Planned.Type);
		PackLane = Lane;
		Delay = Planned.DelayAfter;
		NewEnemy = Spawner->SpawnEnemyOfType(Planned.Type, Lane);
	}
	else
	{
		if (ActiveWave.EnemyType)
		{
			Spawner->EnemyClass = ActiveWave.EnemyType;
		}
		NewEnemy = Spawner->SpawnSingleEnemy();
	}

	if (NewEnemy)
	{
		// Scale this enemy's own type stats by the wave's multipliers.
		if (UHealthComponent* Health = NewEnemy->HealthComponent)
		{
			Health->MaxHealth *= ActiveWave.HealthMultiplier;
			Health->Heal(Health->MaxHealth); // Top current health up to the new (scaled) max.
			Health->OnDeath.AddDynamic(this, &AWaveManager::HandleTrackedEnemyDeath);
		}
		NewEnemy->AttackDamage *= ActiveWave.DamageMultiplier;
		NewEnemy->MoveSpeed *= ActiveWave.SpeedMultiplier;
		NewEnemy->ResourceReward = FMath::RoundToInt(NewEnemy->ResourceReward * ActiveWave.RewardMultiplier);

		FTrackedEnemy Tracked;
		Tracked.Enemy = NewEnemy;
		Tracked.SpawnTime = GetWorld()->GetTimeSeconds();
		const float PathLength = Lane != INDEX_NONE ? Spawner->GetPathLength(Lane) : 0.0f;
		if (PathLength > 0.0f && NewEnemy->MoveSpeed > 0.0f)
		{
			Tracked.ExpectedTravelTime = PathLength / NewEnemy->MoveSpeed;
		}
		TrackedEnemies.Add(Tracked);

		++ActiveEnemyCount;
		OnEnemiesRemainingChanged.Broadcast(ActiveEnemyCount);
	}

	++EnemiesSpawnedThisWave;

	return bReliefActive ? Delay * ReliefDelayScale : Delay;
}

void AWaveManager::MonitorWave()
{
	if (bStopped || State != EWaveState::Active || !Director)
	{
		return;
	}

	ResolveTrackedEnemies();

	FTimerManager& Timers = GetWorldTimerManager();
	const bool bSpawnsRemaining = EnemiesSpawnedThisWave < ActiveWave.EnemyCount;

	// Relief: the tower is taking a beating this wave, so stretch out the remaining spawns.
	if (!bReliefActive && Director->ShouldGrantRelief(GetWorld()))
	{
		bReliefActive = true;
		SetDirectorEvent(TEXT("Relief: tower under heavy attack - spawns slowed"));

		if (bSpawnsRemaining && Timers.IsTimerActive(SpawnTimerHandle))
		{
			const float Remaining = Timers.GetTimerRemaining(SpawnTimerHandle);
			Timers.SetTimer(SpawnTimerHandle, this, &AWaveManager::SpawnTick, Remaining * ReliefDelayScale, false);
		}
		return;
	}

	// Pressure: the player wiped the board, so don't leave them waiting for the next enemy.
	if (!bReliefActive && bSpawnsRemaining && ActiveEnemyCount == 0
		&& Timers.IsTimerActive(SpawnTimerHandle)
		&& Timers.GetTimerRemaining(SpawnTimerHandle) > PressureEarlySpawnThreshold)
	{
		Timers.ClearTimer(SpawnTimerHandle);
		SetDirectorEvent(TEXT("Pressure: board cleared - next enemy sent early"));
		SpawnTick();
	}
}

void AWaveManager::ResolveTrackedEnemies()
{
	const float Now = GetWorld()->GetTimeSeconds();

	for (FTrackedEnemy& Tracked : TrackedEnemies)
	{
		if (Tracked.bResolved)
		{
			continue;
		}

		const AEnemy* Enemy = Tracked.Enemy.Get();
		const bool bDead = !Enemy || !Enemy->HealthComponent || Enemy->HealthComponent->IsDead();

		if (!bDead)
		{
			if (!Tracked.bLeaked && Enemy->HasReachedTower())
			{
				Tracked.bLeaked = true;
			}
			continue;
		}

		Tracked.bResolved = true;
		if (Director)
		{
			const float Progress = (Now - Tracked.SpawnTime) / FMath::Max(1.0f, Tracked.ExpectedTravelTime);
			Director->RecordEnemyOutcome(Progress, Tracked.bLeaked);
		}
	}
}

void AWaveManager::SetDirectorEvent(const FString& Message)
{
	LastDirectorEvent = Message;
	LastDirectorEventTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Display, TEXT("WaveDirector: wave %d - %s"), GetCurrentWave(), *Message);
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
	if (bStopped || State != EWaveState::Active)
	{
		return;
	}

	const bool bAllSpawned = EnemiesSpawnedThisWave >= ActiveWave.EnemyCount;
	if (!bAllSpawned || ActiveEnemyCount > 0)
	{
		return; // Still enemies left to spawn or still enemies alive -> not complete yet.
	}

	GetWorldTimerManager().ClearTimer(MonitorTimerHandle);

	if (bUseAdaptiveDirector && Director)
	{
		ResolveTrackedEnemies();
		Director->EvaluateWave(GetWorld());
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
