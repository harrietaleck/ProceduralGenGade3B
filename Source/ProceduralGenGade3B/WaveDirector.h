// WaveDirector.h
// The brain behind our procedural enemy waves. AWaveManager handles the timing.
// This class decides what goes into each wave and which lane each enemy comes from.
//
// Every wave it goes through the same loop:
//   1. Read the player   - look at how the last wave went, like tower damage, kill speed,
//                          defenders lost and saved loot, and at how the player builds.
//   2. Adapt difficulty  - move the difficulty rating towards a target score, so good players
//                          get pushed and struggling players get some room to breathe.
//   3. Plan the wave     - spend a threat budget on enemies. The budget grows every wave and
//                          the enemy types are picked to counter how the player plays.
//   4. Choose lanes      - for every spawn, pick a lane based on how well each lane is defended.
// From wave 3 it also adds elite enemies that counter the player's style. It can also
// predict the next wave lane by lane so the HUD can show it.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Enemy.h"
#include "WaveDirector.generated.h"

struct FEnemyPath;

/** One enemy the director has planned for the next wave. */
USTRUCT(BlueprintType)
struct FPlannedSpawn
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	EEnemyType Type = EEnemyType::Basic;

	/** How many seconds to wait after this spawn before the next one. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float DelayAfter = 2.0f;

	/** Wolves that follow the pack leader use the same lane as the leader. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	bool bPackFollower = false;

	/** The elite modifier on this enemy. None means it is a normal enemy. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	EEliteModifier Elite = EEliteModifier::None;
};

/** How many enemies we expect on one lane in the next wave. Used for the forecast. */
USTRUCT(BlueprintType)
struct FLaneForecast
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 Lane = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 BasicCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 WolfCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 BearCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 EliteCount = 0;

	int32 Total() const { return BasicCount + WolfCount + BearCount; }
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

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 EliteCount = 0;

	/** The modifier picked to counter the player's style this wave. Stays None until elites unlock. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	EEliteModifier CounterElite = EEliteModifier::None;
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

	/** How much of the defence does area damage, like the Bomb defender. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float AreaShare = 0.0f;

	/** How much of the defence is long range and hits one target, like the Archer. */
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
	int32 WaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float TowerDamageFraction = 0.0f;

	/** 0 means enemies died as soon as they spawned. 1 means every enemy reached the tower. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float AverageKillProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 Leaks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 DefendersLost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	int32 BankedLoot = 0;

	/** The four parts of the score, each from 0 to 1. They count for 40, 30, 20 and 10 percent. */
	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float TowerScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float KillScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float DefenderScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave Director")
	float EconomyScore = 0.0f;

	/** The final score from 0 to 1. The director tries to keep it close to TargetPerformance. */
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

	/** Threat points we can spend in wave 1. A Basic enemy costs 1. */
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

	/** The score the director aims for. The player should feel challenged but still win. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetPerformance = 0.65f;

	/** How much one wave's result changes the difficulty rating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float AdaptRate = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float MinDifficulty = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float MaxDifficulty = 1.6f;

	/** If the tower loses this much of its health in one wave, the tower score is zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float TowerLossForZeroScore = 0.25f;

	/** Spawns slow down mid-wave once the tower has lost this much of its health in the wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Adaptation")
	float ReliefTowerLossFraction = 0.3f;

	// ---- Enemy introduction ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Enemies")
	int32 WolfUnlockWave = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Enemies")
	int32 BearUnlockWave = 3;

	// ---- Elites ----

	/** The first wave that can have elites. That wave always has at least one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Elites")
	int32 EliteUnlockWave = 3;

	/** Chance for each spawn to be an elite in the first elite wave, before difficulty is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Elites")
	float EliteBaseChance = 0.12f;

	/** Extra elite chance added for every wave after the first elite wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Elites")
	float EliteChancePerWave = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Elites")
	float EliteMaxChance = 0.4f;

	/** How many elites get the modifier that counters the player. The rest get a random one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Elites", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CounterEliteShare = 0.65f;

	/** Once the difficulty is above this, Basic enemies can take a different route around defences. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Routing")
	float SmartBasicDifficulty = 1.1f;

	// ---- Pacing ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Pacing")
	float BaseSpawnDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Pacing")
	float MinSpawnDelay = 0.8f;

	/** Time between wolves in the same pack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Director|Pacing")
	float PackSpawnDelay = 0.45f;

	// ---- Wave loop ----

	/** Works out the player profile and then plans the given wave. */
	FWavePlan PlanWave(UWorld* World, int32 WaveNumber);

	/** Saves the tower health and defender count when a wave starts spawning. */
	void BeginWave(UWorld* World);

	/** Saves how far one enemy got along its path, from 0 to 1, and if it reached the tower. */
	void RecordEnemyOutcome(float KillProgress, bool bLeaked);

	/** Scores the wave that just ended and updates the difficulty rating. */
	void EvaluateWave(UWorld* World);

	/** Picks a lane for one spawn using the enemy type and how well each lane is defended right now. */
	int32 ChooseLane(UWorld* World, EEnemyType Type);

	/** True when the tower has lost so much health this wave that spawning should slow down. */
	bool ShouldGrantRelief(UWorld* World) const;

	/**
	 * Guesses which lane each planned enemy will use, based on the current defences. It uses a
	 * copy of the random stream, so it matches the real wave if the player changes nothing.
	 */
	TArray<FLaneForecast> ForecastLanes(UWorld* World) const;

	/** Whether an enemy of this type and elite modifier should look for less defended routes. */
	bool ShouldEnemyReroute(EEnemyType Type, EEliteModifier Elite) const;

	/** The modifier that works best against the player's current defence. */
	EEliteModifier ChooseCounterElite() const;

	// ---- Getters for the HUD and logs ----

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

	/** Every scored wave this match, in order. Used for the graph at the end of the match. */
	UFUNCTION(BlueprintPure, Category = "Wave Director")
	const TArray<FWavePerformance>& GetHistory() const { return History; }

private:
	TArray<FWavePerformance> History;

	/** The lane picking code that both the real spawns and the forecast use. */
	int32 ChooseLaneWith(UWorld* World, EEnemyType Type, FRandomStream& Stream, int32& RoundRobin) const;

	float Difficulty = 1.0f;

	FPlayerProfile Profile;
	FWavePerformance LastPerformance;
	FWavePlan CurrentPlan;
	FRandomStream Random;

	// Values saved when the wave starts
	float TowerHealthAtStart = 0.0f;
	int32 DefendersAtStart = 0;
	int32 PlacedAtStart = 0;

	// How each enemy did in the current wave
	float KillProgressSum = 0.0f;
	int32 OutcomeCount = 0;
	int32 LeakCount = 0;

	int32 RoundRobinLane = 0;

	void BuildProfile(UWorld* World);

	/** Adds up the threat of every defender that can reach any point on the path. */
	float LaneCoverage(UWorld* World, const FEnemyPath& Path) const;

	/** Gives 0 at MinDifficulty and 1 at MaxDifficulty. */
	float NormalisedDifficulty() const;

	static int32 CountLivingDefenders(UWorld* World);
};
