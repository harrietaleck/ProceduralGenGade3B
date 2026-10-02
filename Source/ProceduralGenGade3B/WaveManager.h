// WaveManager.h
// Runs the waves: the countdown, spawning timing, keeping track of living enemies and
// telling the game when a wave is won.
// The UWaveDirector it owns decides what goes into each wave. The fixed Waves table is
// a backup for when bUseAdaptiveDirector is turned off.
// AEnemySpawner still does the actual spawning because it knows how to put an enemy on a
// generated path. This class decides when and what to spawn, then scales the enemy stats.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveDirector.h"
#include "WaveManager.generated.h"

class AEnemySpawner;
class AEnemy;

/** What stage the waves are in. The HUD uses this to decide what to show. */
UENUM(BlueprintType)
enum class EWaveState : uint8
{
	Idle          UMETA(DisplayName = "Idle"),           // Before the first wave has started.
	CountingDown  UMETA(DisplayName = "Counting Down"),  // Shows the wave number and counts down 3, 2, 1.
	Active        UMETA(DisplayName = "Active"),         // Enemies are spawning or still alive.
	Complete      UMETA(DisplayName = "Wave Complete"),  // Short break before the next wave.
	Victory       UMETA(DisplayName = "Victory")          // The last wave has been cleared.
};

/**
 * Settings for one wave in the fixed backup table, used when the director is off.
 */
USTRUCT(BlueprintType)
struct FWaveData
{
	GENERATED_BODY()

	/** How many enemies this wave spawns in total. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "1"))
	int32 EnemyCount = 5;

	/** Which enemy class this wave spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TSubclassOf<AEnemy> EnemyType;

	/** Seconds between each spawn in the wave. Enemies come in one by one, not all at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.05"))
	float SpawnDelay = 2.0f;

	/** Multiplies the enemy's base health for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.1"))
	float HealthMultiplier = 1.0f;

	/** Multiplies the enemy's base attack damage for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.1"))
	float DamageMultiplier = 1.0f;

	/** Multiplies the enemy's base move speed for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.1"))
	float SpeedMultiplier = 1.0f;

	/** Multiplies the loot the enemy gives for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.1"))
	float RewardMultiplier = 1.0f;
};

// --- Events the HUD and other systems listen to, so they don't have to keep checking. ---

// Fires every second of the countdown before a wave.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveCountdownTick, int32, SecondsRemaining);
// Fires when the countdown hits zero and the wave starts spawning.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStarted, int32, WaveNumber);
// Fires when a wave is cleared, so every enemy has spawned and they are all dead.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveComplete, int32, WaveNumber);
// Fires whenever the number of living enemies changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemiesRemainingChanged, int32, Remaining);
// Fires once after the last wave is done.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVictory);

UCLASS()
class PROCEDURALGENGADE3B_API AWaveManager : public AActor
{
	GENERATED_BODY()

public:
	AWaveManager();

	/** If true, the adaptive director plans every wave. If false, we use the fixed Waves table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	bool bUseAdaptiveDirector = true;

	/** How many waves the player has to survive to win when the director plans the waves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "1"))
	int32 TotalWaves = 5;

	/** Decides what goes into each wave and which lane enemies come from. */
	UPROPERTY(VisibleAnywhere, Instanced, BlueprintReadOnly, Category = "Waves")
	TObjectPtr<UWaveDirector> Director;

	/** Fixed wave table. Only used when bUseAdaptiveDirector is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	TArray<FWaveData> Waves;

	/** How long the countdown before each wave lasts, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
	int32 CountdownSeconds = 5;

	/** How long the break after a wave lasts before the next countdown starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.0"))
	float BreakDuration = 5.0f;

	/** Spawn delays get multiplied by this when spawns are slowed down to help the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves|Adaptive", meta = (ClampMin = "1.0"))
	float ReliefDelayScale = 1.5f;

	/** If the board is empty and the next spawn is further away than this, send it now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves|Adaptive", meta = (ClampMin = "0.0"))
	float PressureEarlySpawnThreshold = 0.75f;

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

	/** Gives the manager its spawner, and can also start wave 1 straight away. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void Initialize(AEnemySpawner* InSpawner, bool bStartImmediately = true);

	/** Starts the waves by starting the countdown for wave 1. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StartWaves();

	/** Stops all wave timers for good and stops counting enemy deaths. Used on game over or victory. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StopWaves();

	/** Cancels the break timer so a results screen can be shown between waves. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void HoldForResultsScreen();

	/** Carries on after HoldForResultsScreen. Starts the next countdown, or wins the game if there are no waves left. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void ContinueToNextWave();

	/** Plays the wave that just ended again. This also works for the last wave after winning. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void RetryCurrentWave();

	/** Marks the match as won without firing OnWaveComplete again. Used after the last wave's results. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void DeclareVictory();

	/** The current wave number, starting at 1. It is 0 before the first wave starts. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCurrentWave() const { return CurrentWaveIndex + 1; }

	/** Total number of waves to survive. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetTotalWaves() const { return bUseAdaptiveDirector ? TotalWaves : Waves.Num(); }

	/** How many of the current wave's enemies are still alive. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetEnemiesRemaining() const { return ActiveEnemyCount; }

	/** Seconds left in the countdown. Only useful while the state is CountingDown. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCountdownSecondsRemaining() const { return CountdownSecondsRemaining; }

	/** What stage the waves are in. The HUD uses this to decide what to show. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	EWaveState GetWaveState() const { return State; }

	/** True once the last wave has been cleared. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	bool IsVictory() const { return State == EWaveState::Victory; }

	/** The latest message about the director changing the wave, like slowing or speeding up spawns. Shown on the HUD. */
	UFUNCTION(BlueprintPure, Category = "Waves|Adaptive")
	FString GetLastDirectorEvent() const { return LastDirectorEvent; }

	/** The world time when the last director message happened. The HUD fades it out after a few seconds. */
	UFUNCTION(BlueprintPure, Category = "Waves|Adaptive")
	float GetLastDirectorEventTime() const { return LastDirectorEventTime; }

	/** Shows a message on the HUD's director line. Enemies use this, for example when they change route. */
	UFUNCTION(BlueprintCallable, Category = "Waves|Adaptive")
	void ReportDirectorEvent(const FString& Message) { SetDirectorEvent(Message); }

	/** True when the next wave has already been planned, so the forecast can be shown. */
	UFUNCTION(BlueprintPure, Category = "Waves|Adaptive")
	bool HasUpcomingWavePlan() const { return PreparedWaveIndex != INDEX_NONE && PreparedWaveIndex > CurrentWaveIndex; }

private:
	/** A spawned enemy that we still need to report back to the director. */
	struct FTrackedEnemy
	{
		TWeakObjectPtr<AEnemy> Enemy;
		float SpawnTime = 0.0f;
		float ExpectedTravelTime = 30.0f;
		bool bLeaked = false;
		bool bResolved = false;
	};

	/** The spawner that creates the enemies when we tell it to. */
	UPROPERTY()
	TObjectPtr<AEnemySpawner> Spawner;

	/** Index of the current wave, starting at 0. */
	int32 CurrentWaveIndex = -1;

	/** The wave that is already planned in ActiveWave and SpawnQueue. We plan ahead for the forecast. */
	int32 PreparedWaveIndex = INDEX_NONE;

	/** Settings for the current wave. They come from the director's plan or the fixed table. */
	FWaveData ActiveWave;

	/** The spawns the director planned for the current wave, in order. */
	TArray<FPlannedSpawn> SpawnQueue;

	/** Lane used by the current wolf pack so followers stay with their leader. */
	int32 PackLane = INDEX_NONE;

	/** How many of this wave's enemies have been spawned so far. */
	int32 EnemiesSpawnedThisWave = 0;

	/** How many enemies from this wave have spawned and are still alive. */
	int32 ActiveEnemyCount = 0;

	/** The countdown number the HUD shows while the state is CountingDown. */
	int32 CountdownSecondsRemaining = 0;

	EWaveState State = EWaveState::Idle;

	/** Set when StopWaves() runs, so any timers or callbacks still waiting just do nothing. */
	bool bStopped = false;

	/** True when spawns have been slowed down for the rest of this wave. */
	bool bReliefActive = false;

	TArray<FTrackedEnemy> TrackedEnemies;

	FString LastDirectorEvent;
	float LastDirectorEventTime = -100.0f;

	FTimerHandle CountdownTimerHandle;
	FTimerHandle SpawnTimerHandle;
	FTimerHandle BreakTimerHandle;
	FTimerHandle MonitorTimerHandle;

	/** Fills in the fixed backup table if it was left empty. */
	void EnsureDefaultWaveTable();

	/** Fills ActiveWave and SpawnQueue for the given wave index. */
	void PrepareWave(int32 WaveIndex);

	/** Starts keeping track of a spawned enemy, its death, any split children and its result for the director. */
	void TrackEnemy(AEnemy* Enemy, float SpawnTime, float ExpectedTravelTime);

	/** Called when a Splitting elite makes a child enemy. The child counts as part of this wave. */
	UFUNCTION()
	void HandleEnemySplit(AEnemy* Parent, AEnemy* Child);

	/** Resets the counters and starts the countdown for the planned wave. */
	void StartCountdown();

	/** Moves on to the next wave, or wins the game if there are none left. */
	void BeginNextWave();

	/** Runs on a timer and counts the countdown down to zero. */
	void CountdownTick();

	/** Runs on a timer and spawns one enemy for the current wave, with the wave's stat scaling. */
	void SpawnTick();

	/** Spawns the next enemy and returns how long to wait before the one after it. */
	float SpawnNextEnemy();

	/** Runs on a timer during a wave. It reports finished enemies and adjusts the spawns mid-wave. */
	void MonitorWave();

	/** Tells the director how each finished enemy did. */
	void ResolveTrackedEnemies();

	void SetDirectorEvent(const FString& Message);

	/** Called when a tracked enemy dies. Updates the enemy count and checks if the wave is done. */
	UFUNCTION()
	void HandleTrackedEnemyDeath(AActor* Killer);

	/** Ends the wave once every planned enemy has spawned and none of them are still alive. */
	void CheckWaveCompletion();

	/** Stops the waves, sets the state to Victory and fires OnVictory. */
	void TriggerVictory();
};
