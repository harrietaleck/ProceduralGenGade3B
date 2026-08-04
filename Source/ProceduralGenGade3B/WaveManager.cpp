// WaveManager.cpp — see WaveManager.h for the overview.

#include "WaveManager.h"
#include "EnemySpawner.h"

AWaveManager::AWaveManager()
{
	// All pacing is timer-driven, so no per-frame tick is needed.
	PrimaryActorTick.bCanEverTick = false;
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
	// Guard: nothing to drive without a spawner.
	if (!Spawner)
	{
		return;
	}

	// If we haven't started yet, kick off wave 1; otherwise this acts as a resume.
	if (CurrentWave == 0)
	{
		BeginNextWave();
	}
}

void AWaveManager::StopWaves()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(WaveGapTimerHandle);
}

int32 AWaveManager::EnemiesForWave(int32 WaveNumber) const
{
	// Wave 1 = EnemiesInFirstWave; each later wave adds EnemiesPerWaveIncrement.
	const int32 Count = EnemiesInFirstWave + (WaveNumber - 1) * EnemiesPerWaveIncrement;
	return FMath::Max(1, Count);
}

void AWaveManager::BeginNextWave()
{
	++CurrentWave;
	EnemiesLeftThisWave = EnemiesForWave(CurrentWave);

	// Let the UI (and anything else) know a new wave has begun.
	OnWaveChanged.Broadcast(CurrentWave);

	// Spawn one enemy immediately, then the rest on the spawn timer.
	SpawnTick();
}

void AWaveManager::SpawnTick()
{
	if (!Spawner)
	{
		return;
	}

	// Spawn one enemy for this wave.
	if (EnemiesLeftThisWave > 0)
	{
		Spawner->SpawnSingleEnemy();
		--EnemiesLeftThisWave;
	}

	if (EnemiesLeftThisWave > 0)
	{
		// More to come in this wave: schedule the next spawn.
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AWaveManager::SpawnTick, TimeBetweenSpawns, /*bLoop=*/false);
	}
	else
	{
		// Wave exhausted: pause, then roll into the next (harder) wave.
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		GetWorldTimerManager().SetTimer(WaveGapTimerHandle, this, &AWaveManager::BeginNextWave, TimeBetweenWaves, /*bLoop=*/false);
	}
}
