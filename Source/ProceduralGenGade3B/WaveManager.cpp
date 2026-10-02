// WaveManager.cpp
// The overview of how this works is at the top of WaveManager.h.

#include "WaveManager.h"
#include "EnemySpawner.h"
#include "Enemy.h"
#include "HealthComponent.h"
#include "TimerManager.h"

AWaveManager::AWaveManager()
{
	// Everything runs on timers, so we don't need Tick.
	PrimaryActorTick.bCanEverTick = false;

	Director = CreateDefaultSubobject<UWaveDirector>(TEXT("WaveDirector"));

	EnsureDefaultWaveTable();
}

void AWaveManager::EnsureDefaultWaveTable()
{
	// Backup table that is only used when the adaptive director is turned off.
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

	// If the waves already started, calling this again does nothing instead of restarting wave 1.
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

	// Winning the game calls StopWaves, so we have to turn the manager back on for the replay.
	bStopped = false;
	OnEnemiesRemainingChanged.Broadcast(0);

	PrepareWave(CurrentWaveIndex);
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

	// No waves left means the player survived them all, so they win.
	if (CurrentWaveIndex >= GetTotalWaves())
	{
		TriggerVictory();
		return;
	}

	// Usually the plan is already made when the last wave ended, so the forecast could show it.
	if (PreparedWaveIndex != CurrentWaveIndex)
	{
		PrepareWave(CurrentWaveIndex);
	}
	StartCountdown();
}

void AWaveManager::PrepareWave(int32 WaveIndex)
{
	SpawnQueue.Reset();
	PreparedWaveIndex = WaveIndex;

	if (bUseAdaptiveDirector && Director)
	{
		const FWavePlan Plan = Director->PlanWave(GetWorld(), WaveIndex + 1);
		SpawnQueue = Plan.Spawns;

		ActiveWave = FWaveData();
		ActiveWave.EnemyCount = FMath::Max(1, Plan.Spawns.Num());
		ActiveWave.HealthMultiplier = Plan.HealthMultiplier;
		ActiveWave.DamageMultiplier = Plan.DamageMultiplier;
		ActiveWave.SpeedMultiplier = Plan.SpeedMultiplier;
		ActiveWave.RewardMultiplier = Plan.RewardMultiplier;
		return;
	}

	if (Waves.IsValidIndex(WaveIndex))
	{
		ActiveWave = Waves[WaveIndex];
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
		// A countdown of zero is allowed, so just go straight to spawning.
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

	// The countdown is done, so the wave starts now.
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	State = EWaveState::Active;

	if (bUseAdaptiveDirector && Director)
	{
		Director->BeginWave(GetWorld());
		GetWorldTimerManager().SetTimer(MonitorTimerHandle, this, &AWaveManager::MonitorWave, 0.25f, /*bLoop=*/true);
	}

	OnWaveStarted.Broadcast(GetCurrentWave());
	OnEnemiesRemainingChanged.Broadcast(ActiveEnemyCount);

	// Spawn the first enemy straight away, then use the planned delays for the rest.
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
		// Every enemy has been sent. Now we just wait for them to die.
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		CheckWaveCompletion();
	}
}

float AWaveManager::SpawnNextEnemy()
{
	AEnemy* NewEnemy = nullptr;
	float Delay = ActiveWave.SpawnDelay;
	int32 Lane = INDEX_NONE;
	EEliteModifier Elite = EEliteModifier::None;

	if (bUseAdaptiveDirector && Director && SpawnQueue.IsValidIndex(EnemiesSpawnedThisWave))
	{
		// The director planned this spawn. The type comes from the plan and the lane is picked from the current defences.
		const FPlannedSpawn& Planned = SpawnQueue[EnemiesSpawnedThisWave];
		Lane = (Planned.bPackFollower && PackLane != INDEX_NONE)
			? PackLane
			: Director->ChooseLane(GetWorld(), Planned.Type);
		PackLane = Lane;
		Delay = Planned.DelayAfter;
		Elite = Planned.Elite;
		NewEnemy = Spawner->SpawnEnemyOfType(Planned.Type, Lane);
		if (NewEnemy)
		{
			NewEnemy->bCanReroute = Director->ShouldEnemyReroute(Planned.Type, Elite);
		}
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
		// Scale this enemy's stats by the wave's multipliers.
		if (UHealthComponent* Health = NewEnemy->HealthComponent)
		{
			Health->MaxHealth *= ActiveWave.HealthMultiplier;
			Health->Heal(Health->MaxHealth); // Fill health up to the new scaled max.
		}
		NewEnemy->AttackDamage *= ActiveWave.DamageMultiplier;
		NewEnemy->MoveSpeed *= ActiveWave.SpeedMultiplier;
		NewEnemy->ResourceReward = FMath::RoundToInt(NewEnemy->ResourceReward * ActiveWave.RewardMultiplier);

		// Elite modifiers are added on top of the wave scaling.
		NewEnemy->MakeElite(Elite);

		float ExpectedTravelTime = 30.0f;
		const float PathLength = Lane != INDEX_NONE ? Spawner->GetPathLength(Lane) : 0.0f;
		if (PathLength > 0.0f && NewEnemy->MoveSpeed > 0.0f)
		{
			ExpectedTravelTime = PathLength / NewEnemy->MoveSpeed;
		}
		TrackEnemy(NewEnemy, GetWorld()->GetTimeSeconds(), ExpectedTravelTime);
	}

	++EnemiesSpawnedThisWave;

	return bReliefActive ? Delay * ReliefDelayScale : Delay;
}

void AWaveManager::TrackEnemy(AEnemy* Enemy, float SpawnTime, float ExpectedTravelTime)
{
	if (UHealthComponent* Health = Enemy->HealthComponent)
	{
		Health->OnDeath.AddDynamic(this, &AWaveManager::HandleTrackedEnemyDeath);
	}
	Enemy->OnSplit.AddDynamic(this, &AWaveManager::HandleEnemySplit);

	FTrackedEnemy Tracked;
	Tracked.Enemy = Enemy;
	Tracked.SpawnTime = SpawnTime;
	Tracked.ExpectedTravelTime = ExpectedTravelTime;
	TrackedEnemies.Add(Tracked);

	++ActiveEnemyCount;
	OnEnemiesRemainingChanged.Broadcast(ActiveEnemyCount);
}

void AWaveManager::HandleEnemySplit(AEnemy* Parent, AEnemy* Child)
{
	if (bStopped || !Child)
	{
		return;
	}

	// Children use the parent's spawn time, so the distance the parent already walked still counts for the score.
	float SpawnTime = GetWorld()->GetTimeSeconds();
	float ExpectedTravelTime = 30.0f;
	for (const FTrackedEnemy& Tracked : TrackedEnemies)
	{
		if (Tracked.Enemy.Get() == Parent)
		{
			SpawnTime = Tracked.SpawnTime;
			ExpectedTravelTime = Tracked.ExpectedTravelTime;
			break;
		}
	}

	TrackEnemy(Child, SpawnTime, ExpectedTravelTime);
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

	// Relief. The tower is getting hit hard this wave, so space out the spawns that are left.
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

	// Pressure. The player killed everything, so don't make them wait for the next enemy.
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

	// Wait one tick so the children of a Splitting elite get counted before the wave can end.
	GetWorldTimerManager().SetTimerForNextTick(this, &AWaveManager::CheckWaveCompletion);
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
		return; // Enemies are still left to spawn or still alive, so the wave is not done yet.
	}

	GetWorldTimerManager().ClearTimer(MonitorTimerHandle);

	if (bUseAdaptiveDirector && Director)
	{
		ResolveTrackedEnemies();
		Director->EvaluateWave(GetWorld());
	}

	State = EWaveState::Complete;
	OnWaveComplete.Broadcast(GetCurrentWave());

	// Plan the next wave now, after the game mode has made the map bigger, so the forecast can
	// show it during the break. Lanes are still picked when each enemy spawns.
	if (bUseAdaptiveDirector && Director && !bStopped && State == EWaveState::Complete
		&& CurrentWaveIndex + 1 < GetTotalWaves())
	{
		PrepareWave(CurrentWaveIndex + 1);
	}

	// Don't start the next wave here. GameMode shows the results screen and calls
	// ContinueToNextWave() when the player is ready.
}

void AWaveManager::TriggerVictory()
{
	State = EWaveState::Victory;
	StopWaves(); // No more countdowns, spawns or breaks once every wave is cleared.
	OnVictory.Broadcast();
}
