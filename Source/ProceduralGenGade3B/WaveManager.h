// WaveManager.h
// Drives enemy attacks as a sequence of escalating waves rather than an endless trickle.
// It owns the *rhythm* of combat: how many enemies a wave contains, the gap between each
// spawn, and the breather between waves. The actual spawning is delegated to AEnemySpawner,
// so this class only decides "spawn one now" — it never touches enemy internals.
//
// This split keeps a single responsibility per class (WaveManager = pacing, EnemySpawner =
// creation) and makes future work — boss waves, mixed enemy rosters, difficulty scaling —
// a matter of changing the pacing rules here, not rewriting spawning.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveManager.generated.h"

class AEnemySpawner;

// Broadcast at the start of each wave so the UI can show "Wave N". Param: the new wave number.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveChanged, int32, WaveNumber);

UCLASS()
class PROCEDURALGENGADE3B_API AWaveManager : public AActor
{
	GENERATED_BODY()

public:
	AWaveManager();

	/** Enemies in the very first wave. Each subsequent wave adds EnemiesPerWaveIncrement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "1"))
	int32 EnemiesInFirstWave = 5;

	/** How many extra enemies each new wave adds over the previous one (difficulty scaling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
	int32 EnemiesPerWaveIncrement = 2;

	/** Seconds between individual spawns within a wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.05"))
	float TimeBetweenSpawns = 1.5f;

	/** Seconds of calm between the last spawn of one wave and the first of the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.0"))
	float TimeBetweenWaves = 6.0f;

	/** Broadcast whenever a new wave begins (the HUD listens to this). */
	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnWaveChanged OnWaveChanged;

	/** Hand the manager the spawner it should drive, then optionally begin wave 1. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void Initialize(AEnemySpawner* InSpawner, bool bStartImmediately = true);

	/** Begin the first wave (or resume from the current wave). */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StartWaves();

	/** Halt all spawning and pacing timers (e.g. on game over). */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StopWaves();

	/** The wave currently in progress (1-based; 0 before the first wave starts). */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCurrentWave() const { return CurrentWave; }

private:
	/** The spawner that actually creates enemies on our command. */
	UPROPERTY()
	TObjectPtr<AEnemySpawner> Spawner;

	/** 1-based index of the wave in progress. */
	int32 CurrentWave = 0;

	/** How many enemies are still to be spawned in the current wave. */
	int32 EnemiesLeftThisWave = 0;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle WaveGapTimerHandle;

	/** Set up and start the next wave (increments CurrentWave, broadcasts, begins spawning). */
	void BeginNextWave();

	/** Timer callback: spawn one enemy; when the wave is exhausted, schedule the next wave. */
	void SpawnTick();

	/** Enemy count for a given 1-based wave number. */
	int32 EnemiesForWave(int32 WaveNumber) const;
};
