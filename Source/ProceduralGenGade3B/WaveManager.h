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
    Idle UMETA(DisplayName = "Idle"),
    CountingDown UMETA(DisplayName = "Counting Down"),
    Active UMETA(DisplayName = "Active"),
    Complete UMETA(DisplayName = "Wave Complete"),
    Victory UMETA(DisplayName = "Victory")
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

    /** Seconds between each spawn in the wave. */
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

// Events the HUD and other systems listen to.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveCountdownTick, int32, SecondsRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStarted, int32, WaveNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveComplete, int32, WaveNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemiesRemainingChanged, int32, Remaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVictory);

UCLASS()
class PROCEDURALGENGADE3B_API AWaveManager : public AActor
{
    GENERATED_BODY()

public:
    AWaveManager();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
    bool bUseAdaptiveDirector = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "1"))
    int32 TotalWaves = 5;

    UPROPERTY(VisibleAnywhere, Instanced, BlueprintReadOnly, Category = "Waves")
    TObjectPtr<UWaveDirector> Director;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
    TArray<FWaveData> Waves;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
    int32 CountdownSeconds = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.0"))
    float BreakDuration = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves|Adaptive", meta = (ClampMin = "1.0"))
    float ReliefDelayScale = 1.5f;

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

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void Initialize(AEnemySpawner* InSpawner, bool bStartImmediately = true);

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void StartWaves();

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void StopWaves();

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void HoldForResultsScreen();

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void ContinueToNextWave();

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void RetryCurrentWave();

    void SetCompletedWaves(int32 Count);

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void DeclareVictory();

    UFUNCTION(BlueprintPure, Category = "Waves")
    int32 GetCurrentWave() const { return CurrentWaveIndex + 1; }

    UFUNCTION(BlueprintPure, Category = "Waves")
    int32 GetTotalWaves() const { return bUseAdaptiveDirector ? TotalWaves : Waves.Num(); }

    UFUNCTION(BlueprintPure, Category = "Waves")
    int32 GetEnemiesRemaining() const { return ActiveEnemyCount; }

    UFUNCTION(BlueprintPure, Category = "Waves")
    int32 GetCountdownSecondsRemaining() const { return CountdownSecondsRemaining; }

    UFUNCTION(BlueprintPure, Category = "Waves")
    EWaveState GetWaveState() const { return State; }

    UFUNCTION(BlueprintPure, Category = "Waves")
    bool IsVictory() const { return State == EWaveState::Victory; }

    UFUNCTION(BlueprintPure, Category = "Waves|Adaptive")
    FString GetLastDirectorEvent() const { return LastDirectorEvent; }

    UFUNCTION(BlueprintPure, Category = "Waves|Adaptive")
    float GetLastDirectorEventTime() const { return LastDirectorEventTime; }

    UFUNCTION(BlueprintCallable, Category = "Waves|Adaptive")
    void ReportDirectorEvent(const FString& Message) { SetDirectorEvent(Message); }

    UFUNCTION(BlueprintPure, Category = "Waves|Adaptive")
    bool HasUpcomingWavePlan() const
    {
        return PreparedWaveIndex != INDEX_NONE && PreparedWaveIndex > CurrentWaveIndex;
    }

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

    UPROPERTY()
    TObjectPtr<AEnemySpawner> Spawner;

    int32 CurrentWaveIndex = -1;
    int32 PreparedWaveIndex = INDEX_NONE;

    FWaveData ActiveWave;
    TArray<FPlannedSpawn> SpawnQueue;

    int32 PackLane = INDEX_NONE;
    int32 EnemiesSpawnedThisWave = 0;
    int32 ActiveEnemyCount = 0;
    int32 CountdownSecondsRemaining = 0;

    EWaveState State = EWaveState::Idle;

    bool bStopped = false;
    bool bReliefActive = false;

    TArray<FTrackedEnemy> TrackedEnemies;

    FString LastDirectorEvent;
    float LastDirectorEventTime = -100.0f;

    FTimerHandle CountdownTimerHandle;
    FTimerHandle SpawnTimerHandle;
    FTimerHandle BreakTimerHandle;
    FTimerHandle MonitorTimerHandle;

    void EnsureDefaultWaveTable();
    void PrepareWave(int32 WaveIndex);
    void TrackEnemy(AEnemy* Enemy, float SpawnTime, float ExpectedTravelTime);

    UFUNCTION()
    void HandleEnemySplit(AEnemy* Parent, AEnemy* Child);

    void StartCountdown();
    void BeginNextWave();
    void CountdownTick();
    void SpawnTick();
    float SpawnNextEnemy();
    void MonitorWave();
    void ResolveTrackedEnemies();
    void SetDirectorEvent(const FString& Message);

    UFUNCTION()
    void HandleTrackedEnemyDeath(AActor* Killer);

    void CheckWaveCompletion();
    void TriggerVictory();
};