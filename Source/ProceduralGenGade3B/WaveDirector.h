// WaveDirector.h
// The "brain" behind procedural enemy waves. AWaveManager owns the timing and state machine;
// this object decides WHAT each wave contains and WHERE each enemy enters.
//
// Every wave it runs one loop:
//   1. Read the player   - how the last wave went (tower damage, kill speed, defenders lost,
//                          banked loot) and how they build (defender count / mix per lane).
//   2. Adapt difficulty  - nudge a difficulty rating towards a target performance so strong
//                          players get pushed and struggling players get breathing room.
//   3. Plan the wave     - spend a threat budget (grows each wave, scaled by difficulty) on
//                          enemy types weighted to counter the player's play style.
//   4. Choose lanes      - per spawn, pick a lane from how well each lane is defended.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Enemy.h"
#include "WaveDirector.generated.h"

struct FEnemyPath;

/** One enemy the director has scheduled for the coming wave. */
USTRUCT(BlueprintType)
struct FPlannedSpawn
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	EEnemyType Type = EEnemyType::Basic;

	/** Seconds to wait after this spawn before the next one. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float DelayAfter = 2.0f;

	/** Wolf pack members after the leader reuse the leader's lane. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	bool bPackFollower = false;
};

/** The full plan for one wave. */
USTRUCT(BlueprintType)
struct FWavePlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 WaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float ThreatBudget = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	TArray<FPlannedSpawn> Spawns;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float HealthMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float DamageMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float SpeedMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float RewardMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 BasicCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 WolfCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 BearCount = 0;
};

/** How the player is building their defence. */
USTRUCT(BlueprintType)
struct FPlayerProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 DefenderCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float DefendersPerLane = 0.0f;

	/** Fraction of defenders that deal area damage (Bomb). */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float AreaShare = 0.0f;

	/** Fraction of defenders that are long-range single-target (Archer). */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float ArcherShare = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	FString StyleLabel = TEXT("Unknown");
};

/** How well the player handled the last wave. */
USTRUCT(BlueprintType)
struct FWavePerformance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float TowerDamageFraction = 0.0f;

	/** 0 = enemies died the instant they spawned, 1 = every enemy reached the tower. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float AverageKillProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 Leaks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 DefendersLost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 BankedLoot = 0;

	/** 0..1 parts of the score, weighted 40 / 30 / 20 / 10. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float TowerScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float KillScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float DefenderScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float EconomyScore = 0.0f;

	/** Weighted 0..1 score; the director tries to keep this near TargetPerformance. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float Score = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float DifficultyBefore = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float DifficultyAfter = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	bool bValid = false;
};

UCLASS(BlueprintType, DefaultToInstanced, EditInlineNew)
class PROCEDURALGENGADE3B_API UWaveDirector : public UObject
{
	GENERATED_BODY()

public:
	// ---- Difficulty scaling ----

	/** Threat points available in wave 1 (a Basic enemy costs 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Budget")
	float BaseThreatBudget = 6.0f;

	/** Extra threat points added every wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Budget")
	float BudgetGrowthPerWave = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Budget")
	float BasicCost = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Budget")
	float WolfCost = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Budget")
	float BearCost = 3.0f;

	// ---- Adaptation ----

	/** Performance score the director aims for: challenged but winning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetPerformance = 0.65f;

	/** How strongly one wave's result moves the difficulty rating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float AdaptRate = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float MinDifficulty = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float MaxDifficulty = 1.6f;

	/** Losing this fraction of tower health in one wave counts as a total tower failure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float TowerLossForZeroScore = 0.25f;

	/** Mid-wave relief kicks in once the tower loses this fraction of its health in one wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float ReliefTowerLossFraction = 0.3f;

	// ---- Enemy introduction ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Enemies")
	int32 WolfUnlockWave = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Enemies")
	int32 BearUnlockWave = 3;

	// ---- Pacing ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Pacing")
	float BaseSpawnDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Pacing")
	float MinSpawnDelay = 0.8f;

	/** Gap between wolves in the same pack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Pacing")
	float PackSpawnDelay = 0.45f;

	// ---- Wave loop ----

	/** Build the player profile and the plan for the given wave. */
	FWavePlan PlanWave(UWorld* World, int32 WaveNumber);

	/** Snapshot tower health / defender count when a wave's spawning begins. */
	void BeginWave(UWorld* World);

	/** Record how far one enemy got (0..1 of its expected walk) and whether it reached the tower. */
	void RecordEnemyOutcome(float KillProgress, bool bLeaked);

	/** Score the finished wave and move the difficulty rating. */
	void EvaluateWave(UWorld* World);

	/** Pick the lane for one spawn based on the enemy type and live lane coverage. */
	int32 ChooseLane(UWorld* World, EEnemyType Type);

	/** True when the tower has lost enough health this wave that spawning should ease off. */
	bool ShouldGrantRelief(UWorld* World) const;

	// ---- Read-outs (HUD, logs) ----

	UFUNCTION(BlueprintPure, Category = "Wave Director")
	float GetDifficulty() const { return Difficulty; }

	UFUNCTION(BlueprintPure, Category = "Wave Director")
	FString GetSkillLabel() const;

	UFUNCTION(BlueprintPure, Category = "Wave Director")
	const FPlayerProfile& GetProfile() const { return Profile; }

	UFUNCTION(BlueprintPure, Category = "Wave Director")
	const FWavePerformance& GetLastPerformance() const { return LastPerformance; }

	UFUNCTION(BlueprintPure, Category = "Wave Director")
	const FWavePlan& GetCurrentPlan() const { return CurrentPlan; }

private:
	float Difficulty = 1.0f;

	FPlayerProfile Profile;
	FWavePerformance LastPerformance;
	FWavePlan CurrentPlan;
	FRandomStream Random;

	// Wave-start snapshot
	float TowerHealthAtStart = 0.0f;
	int32 DefendersAtStart = 0;
	int32 PlacedAtStart = 0;

	// Per-enemy outcomes for the current wave
	float KillProgressSum = 0.0f;
	int32 OutcomeCount = 0;
	int32 LeakCount = 0;

	int32 RoundRobinLane = 0;

	void BuildProfile(UWorld* World);

	/** Sum of defender threat covering any point of the path. */
	float LaneCoverage(UWorld* World, const FEnemyPath& Path) const;

	/** 0 at MinDifficulty, 1 at MaxDifficulty. */
	float NormalisedDifficulty() const;

	static int32 CountLivingDefenders(UWorld* World);
};
