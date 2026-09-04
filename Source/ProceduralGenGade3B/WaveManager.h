// WaveManager.h
// The single authority for wave progression. It owns the wave table (data-driven, no
// hardcoded gameplay values), the pre-wave countdown, spawning cadence, live enemy tracking,
// difficulty scaling, and the win/lose hooks the rest of the game reacts to.
//
// Spawning itself still goes through AEnemySpawner (it alone knows how to place an enemy on
// a generated path) — but WaveManager decides *if/when* that happens and applies this wave's
// stat multipliers afterward, so AEnemySpawner and AEnemy stay untouched, generic, and
// reusable for any future wave shape.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveManager.generated.h"

class AEnemySpawner;
class AEnemy;

/** Where the wave sequence currently is — read by the HUD to decide what to display. */
UENUM(BlueprintType)
enum class EWaveState : uint8
{
	Idle          UMETA(DisplayName = "Idle"),           // Before the first wave has begun.
	CountingDown  UMETA(DisplayName = "Counting Down"),  // "WAVE N" + 3..2..1 countdown.
	Active        UMETA(DisplayName = "Active"),         // Enemies spawning and/or still alive.
	Complete      UMETA(DisplayName = "Wave Complete"),  // Brief breather before the next wave.
	Victory       UMETA(DisplayName = "Victory")          // Final configured wave cleared.
};

/**
 * One wave's full configuration. Every gameplay number a wave needs lives here — nothing
 * about wave shape or difficulty is hardcoded in WaveManager's logic, so designers can add,
 * remove, or retune waves (or add new enemy types later) without touching C++.
 */
USTRUCT(BlueprintType)
struct FWaveData
{
	GENERATED_BODY()

	/** How many enemies this wave spawns in total. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "1"))
	int32 EnemyCount = 5;

	/** Which enemy class this wave spawns (Part 1: always the Basic Enemy; future parts can vary this per wave). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TSubclassOf<AEnemy> EnemyType;

	/** Seconds between each individual spawn within the wave — enemies always trickle in, never all at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.05"))
	float SpawnDelay = 2.0f;

	/** Multiplies the enemy class's own base health for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.1"))
	float HealthMultiplier = 1.0f;

	/** Multiplies the enemy class's own base attack damage for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.1"))
	float DamageMultiplier = 1.0f;

	/** Multiplies the enemy class's own base move speed for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.1"))
	float SpeedMultiplier = 1.0f;

	/** Multiplies the enemy class's own base Loot reward for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.1"))
	float RewardMultiplier = 1.0f;
};

// --- Events other systems (chiefly the HUD) subscribe to instead of polling WaveManager state. ---

// Fires once per second during the pre-wave countdown (3, 2, 1).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveCountdownTick, int32, SecondsRemaining);
// Fires the moment a wave's spawning actually begins (countdown reached zero).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStarted, int32, WaveNumber);
// Fires once a wave is fully cleared (every enemy spawned AND all of them dead).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveComplete, int32, WaveNumber);
// Fires whenever the number of currently-alive tracked enemies changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemiesRemainingChanged, int32, Remaining);
// Fires once, after the final configured wave is completed.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVictory);

UCLASS()
class PROCEDURALGENGADE3B_API AWaveManager : public AActor
{
	GENERATED_BODY()

public:
	AWaveManager();

	/** The wave table. Defaults to the 5-wave Part 1 progression; fully editable per-instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	TArray<FWaveData> Waves;

	/** Length of the "3, 2, 1" countdown shown before each wave starts, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
	int32 CountdownSeconds = 3;

	/** How long the "WAVE COMPLETE" breather lasts before the next wave's countdown begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.0"))
	float BreakDuration = 5.0f;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnWaveCountdownTick OnWaveCountdownTick;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnWaveStarted OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnWaveComplete OnWaveComplete;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnEnemiesRemainingChanged OnEnemiesRemainingChanged;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnVictory OnVictory;

	/** Hand the manager the spawner it should drive, then optionally begin wave 1. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void Initialize(AEnemySpawner* InSpawner, bool bStartImmediately = true);

	/** Begin wave progression (starts wave 1's countdown). */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StartWaves();

	/** Permanently halt all wave timers and stop tracking further enemy deaths (game over / victory). */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StopWaves();

	/** Cancel the automatic break timer so a results screen can be shown between waves. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void HoldForResultsScreen();

	/** Resume after HoldForResultsScreen — starts the next wave countdown, or victory if none remain. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void ContinueToNextWave();

	/** Replay the wave that just completed, including the final wave after victory. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void RetryCurrentWave();

	/** Mark the match won without firing another OnWaveComplete (used after the final wave results). */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void DeclareVictory();

	/** The wave currently in progress (1-based; 0 before the first wave starts). */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCurrentWave() const { return CurrentWaveIndex + 1; }

	/** Total number of configured waves (Part 1: 5). */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetTotalWaves() const { return Waves.Num(); }

	/** How many of the current wave's enemies are still alive. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetEnemiesRemaining() const { return ActiveEnemyCount; }

	/** Seconds left in the pre-wave countdown (only meaningful while State == CountingDown). */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCountdownSecondsRemaining() const { return CountdownSecondsRemaining; }

	/** Where the wave sequence currently is — drives what the HUD shows. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	EWaveState GetWaveState() const { return State; }

	/** True once the final configured wave has been cleared. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	bool IsVictory() const { return State == EWaveState::Victory; }

private:
	/** The spawner that actually creates enemies on our command. */
	UPROPERTY()
	TObjectPtr<AEnemySpawner> Spawner;

	/** 0-based index into Waves for the wave in progress. */
	int32 CurrentWaveIndex = -1;

	/** How many of this wave's enemies have been spawned so far. */
	int32 EnemiesSpawnedThisWave = 0;

	/** How many spawned-and-still-alive enemies this wave currently has. */
	int32 ActiveEnemyCount = 0;

	/** Live countdown value shown by the HUD while State == CountingDown. */
	int32 CountdownSecondsRemaining = 0;

	EWaveState State = EWaveState::Idle;

	/** Set once StopWaves() has run, so any in-flight timers/callbacks become no-ops. */
	bool bStopped = false;

	FTimerHandle CountdownTimerHandle;
	FTimerHandle SpawnTimerHandle;
	FTimerHandle BreakTimerHandle;

	/** Populate Waves with the Part 1 progression (3 waves) if nothing was configured. */
	void EnsureDefaultWaveTable();

	/** Advance to the next configured wave, or trigger Victory if none remain. */
	void BeginNextWave();

	/** Timer callback: ticks the pre-wave "3, 2, 1" countdown down to zero. */
	void CountdownTick();

	/** Timer callback: spawns one enemy for the active wave, scaled by its multipliers. */
	void SpawnTick();

	/** Bound to each tracked enemy's death: updates the live count and checks for completion. */
	UFUNCTION()
	void HandleTrackedEnemyDeath(AActor* Killer);

	/** True only when every scheduled enemy has spawned AND none of them are still alive. */
	void CheckWaveCompletion();

	/** Ends wave progression in the Victory state and broadcasts OnVictory. */
	void TriggerVictory();
};
