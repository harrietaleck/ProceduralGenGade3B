// WaveDirector.cpp — see WaveDirector.h for the overview.

#include "WaveDirector.h"
#include "Defender.h"
#include "ArcherDefender.h"
#include "HealthComponent.h"
#include "ProceduralTerrain.h"
#include "TDGameMode.h"
#include "Tower.h"
#include "EngineUtils.h"

namespace
{
	ATDGameMode* GetTDGameMode(UWorld* World)
	{
		return World ? World->GetAuthGameMode<ATDGameMode>() : nullptr;
	}

	UHealthComponent* GetTowerHealth(UWorld* World)
	{
		ATDGameMode* GameMode = GetTDGameMode(World);
		ATower* Tower = GameMode ? GameMode->GetTower() : nullptr;
		return Tower ? Tower->HealthComponent.Get() : nullptr;
	}
}

// ---------------------------------------------------------------------------------------------
// 1. Read the player
// ---------------------------------------------------------------------------------------------

int32 UWaveDirector::CountLivingDefenders(UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<ADefender> It(World); It; ++It)
	{
		if (It->HealthComponent && !It->HealthComponent->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

void UWaveDirector::BuildProfile(UWorld* World)
{
	Profile = FPlayerProfile();

	int32 AreaCount = 0;
	int32 ArcherCount = 0;
	for (TActorIterator<ADefender> It(World); It; ++It)
	{
		const ADefender* Defender = *It;
		if (!Defender->HealthComponent || Defender->HealthComponent->IsDead())
		{
			continue;
		}

		++Profile.DefenderCount;
		if (Defender->IsAreaAttacker())
		{
			++AreaCount;
		}
		if (Defender->IsA<AArcherDefender>())
		{
			++ArcherCount;
		}
	}

	const ATDGameMode* GameMode = GetTDGameMode(World);
	const AProceduralTerrain* Terrain = GameMode ? GameMode->GetTerrain() : nullptr;
	const int32 Lanes = Terrain ? FMath::Max(1, Terrain->GetEnemyPaths().Num()) : 1;

	Profile.DefendersPerLane = static_cast<float>(Profile.DefenderCount) / Lanes;
	if (Profile.DefenderCount > 0)
	{
		Profile.AreaShare = static_cast<float>(AreaCount) / Profile.DefenderCount;
		Profile.ArcherShare = static_cast<float>(ArcherCount) / Profile.DefenderCount;
	}

	if (Profile.DefenderCount == 0)
	{
		Profile.StyleLabel = TEXT("Tower only");
	}
	else if (Profile.AreaShare >= 0.4f)
	{
		Profile.StyleLabel = TEXT("Area-damage focus");
	}
	else if (Profile.ArcherShare >= 0.4f)
	{
		Profile.StyleLabel = TEXT("Long-range focus");
	}
	else if (Profile.DefendersPerLane >= 2.0f)
	{
		Profile.StyleLabel = TEXT("Defender-heavy");
	}
	else if (Profile.DefendersPerLane < 1.0f)
	{
		Profile.StyleLabel = TEXT("Light defence");
	}
	else
	{
		Profile.StyleLabel = TEXT("Balanced");
	}
}

void UWaveDirector::BeginWave(UWorld* World)
{
	UHealthComponent* TowerHealth = GetTowerHealth(World);
	TowerHealthAtStart = TowerHealth ? TowerHealth->GetCurrentHealth() : 0.0f;
	DefendersAtStart = CountLivingDefenders(World);

	const ATDGameMode* GameMode = GetTDGameMode(World);
	PlacedAtStart = GameMode ? GameMode->GetMatchDefendersPlaced() : 0;

	KillProgressSum = 0.0f;
	OutcomeCount = 0;
	LeakCount = 0;
}

void UWaveDirector::RecordEnemyOutcome(float KillProgress, bool bLeaked)
{
	KillProgressSum += bLeaked ? 1.0f : FMath::Clamp(KillProgress, 0.0f, 1.0f);
	++OutcomeCount;
	if (bLeaked)
	{
		++LeakCount;
	}
}

bool UWaveDirector::ShouldGrantRelief(UWorld* World) const
{
	const UHealthComponent* TowerHealth = GetTowerHealth(World);
	if (!TowerHealth || TowerHealth->MaxHealth <= 0.0f)
	{
		return false;
	}

	const float LostThisWave = (TowerHealthAtStart - TowerHealth->GetCurrentHealth()) / TowerHealth->MaxHealth;
	return LostThisWave >= ReliefTowerLossFraction;
}

// ---------------------------------------------------------------------------------------------
// 2. Adapt difficulty
// ---------------------------------------------------------------------------------------------

void UWaveDirector::EvaluateWave(UWorld* World)
{
	FWavePerformance Result;
	Result.bValid = true;

	// Tower: losing TowerLossForZeroScore of max health in one wave scores zero.
	if (const UHealthComponent* TowerHealth = GetTowerHealth(World))
	{
		Result.TowerDamageFraction = FMath::Max(0.0f, TowerHealthAtStart - TowerHealth->GetCurrentHealth())
			/ FMath::Max(1.0f, TowerHealth->MaxHealth);
	}
	const float TowerScore = 1.0f - FMath::Clamp(Result.TowerDamageFraction / TowerLossForZeroScore, 0.0f, 1.0f);

	// Kill speed: how far along their path enemies got before dying (leaks count as 1).
	Result.AverageKillProgress = OutcomeCount > 0 ? KillProgressSum / OutcomeCount : 0.0f;
	Result.Leaks = LeakCount;
	const float KillScore = 1.0f - Result.AverageKillProgress;

	// Defenders: share of the defence that survived the wave.
	const ATDGameMode* GameMode = GetTDGameMode(World);
	const int32 PlacedDuring = GameMode ? FMath::Max(0, GameMode->GetMatchDefendersPlaced() - PlacedAtStart) : 0;
	const int32 Fielded = DefendersAtStart + PlacedDuring;
	Result.DefendersLost = FMath::Max(0, Fielded - CountLivingDefenders(World));
	const float DefenderScore = Fielded > 0 ? 1.0f - static_cast<float>(Result.DefendersLost) / Fielded : 1.0f;

	// Economy: a big unspent bank means the player is comfortable.
	Result.BankedLoot = GameMode ? GameMode->GetResources() : 0;
	const float EconomyScore = FMath::Clamp(Result.BankedLoot / 300.0f, 0.0f, 1.0f);

	Result.TowerScore = TowerScore;
	Result.KillScore = KillScore;
	Result.DefenderScore = DefenderScore;
	Result.EconomyScore = EconomyScore;
	Result.Score = 0.4f * TowerScore + 0.3f * KillScore + 0.2f * DefenderScore + 0.1f * EconomyScore;

	// Rubber band towards the target: above target -> harder, below -> easier.
	const float PreviousDifficulty = Difficulty;
	Difficulty = FMath::Clamp(Difficulty + (Result.Score - TargetPerformance) * AdaptRate, MinDifficulty, MaxDifficulty);
	Result.DifficultyBefore = PreviousDifficulty;
	Result.DifficultyAfter = Difficulty;
	LastPerformance = Result;

	UE_LOG(LogTemp, Display,
		TEXT("WaveDirector: wave %d score %.2f (tower %.2f, kills %.2f, defenders %.2f, economy %.2f) -> difficulty %.2f -> %.2f [%s]"),
		CurrentPlan.WaveNumber, Result.Score, TowerScore, KillScore, DefenderScore, EconomyScore,
		PreviousDifficulty, Difficulty, *GetSkillLabel());
}

float UWaveDirector::NormalisedDifficulty() const
{
	return FMath::GetRangePct(MinDifficulty, MaxDifficulty, Difficulty);
}

FString UWaveDirector::GetSkillLabel() const
{
	if (Difficulty < 0.9f)
	{
		return TEXT("Struggling");
	}
	if (Difficulty <= 1.1f)
	{
		return TEXT("Holding");
	}
	return TEXT("Dominating");
}

// ---------------------------------------------------------------------------------------------
// 3. Plan the wave
// ---------------------------------------------------------------------------------------------

FWavePlan UWaveDirector::PlanWave(UWorld* World, int32 WaveNumber)
{
	BuildProfile(World);

	// Same map seed + same play = same waves, while different play gives different waves.
	const ATDGameMode* GameMode = GetTDGameMode(World);
	const AProceduralTerrain* Terrain = GameMode ? GameMode->GetTerrain() : nullptr;
	Random.Initialize((Terrain ? Terrain->Seed : 0) + WaveNumber * 7919 + FMath::RoundToInt(Difficulty * 100.0f));

	FWavePlan Plan;
	Plan.WaveNumber = WaveNumber;
	Plan.ThreatBudget = (BaseThreatBudget + BudgetGrowthPerWave * (WaveNumber - 1)) * Difficulty;

	// When to spawn which type: unlock gradually, then weight by the player's style.
	const float Pressure = FMath::Clamp(Profile.DefendersPerLane / 3.0f, 0.0f, 1.0f);
	const bool bWolvesUnlocked = WaveNumber >= WolfUnlockWave;
	const bool bBearsUnlocked = WaveNumber >= BearUnlockWave;

	// Basic: the filler, more of them for a struggling player (easiest to handle).
	const float BasicWeight = 1.0f + (Difficulty < 0.9f ? 0.8f : 0.0f);
	// Wolves hunt defenders: answer defender-heavy builds and slow single-target archers.
	const float WolfWeight = bWolvesUnlocked ? 0.6f + 1.0f * Pressure + 0.8f * Profile.ArcherShare : 0.0f;
	// Bears tank area damage and punish thin defences by marching straight to the tower.
	const float BearWeight = bBearsUnlocked
		? 0.4f + 1.0f * Profile.AreaShare + 0.6f * (1.0f - Pressure) + (Difficulty > 1.1f ? 0.4f : 0.0f)
		: 0.0f;

	const float StandardDelay = FMath::Max(MinSpawnDelay,
		(BaseSpawnDelay - 0.12f * (WaveNumber - 1)) / FMath::Sqrt(Difficulty));

	float Remaining = Plan.ThreatBudget;

	auto AddSpawn = [&Plan, StandardDelay](EEnemyType Type, bool bFollower, float Delay)
	{
		FPlannedSpawn Spawn;
		Spawn.Type = Type;
		Spawn.bPackFollower = bFollower;
		Spawn.DelayAfter = Delay > 0.0f ? Delay : StandardDelay;
		Plan.Spawns.Add(Spawn);
	};

	auto AddWolfPack = [&](int32 PackSize)
	{
		for (int32 i = 0; i < PackSize; ++i)
		{
			const bool bLast = i == PackSize - 1;
			AddSpawn(EEnemyType::Wolf, i > 0, bLast ? StandardDelay : PackSpawnDelay);
			Remaining -= WolfCost;
		}
	};

	// Introduce each new type the wave it unlocks so the player always meets it.
	if (WaveNumber == WolfUnlockWave && Remaining >= WolfCost * 2.0f)
	{
		AddWolfPack(2);
	}
	if (WaveNumber == BearUnlockWave && Remaining >= BearCost)
	{
		AddSpawn(EEnemyType::Bear, false, 0.0f);
		Remaining -= BearCost;
	}

	// Spend the rest of the budget with a weighted random pick among affordable types.
	while (Remaining >= BasicCost)
	{
		const float W0 = BasicWeight;
		const float W1 = Remaining >= WolfCost ? WolfWeight : 0.0f;
		const float W2 = Remaining >= BearCost ? BearWeight : 0.0f;
		const float Roll = Random.FRand() * (W0 + W1 + W2);

		if (Roll < W0)
		{
			AddSpawn(EEnemyType::Basic, false, 0.0f);
			Remaining -= BasicCost;
		}
		else if (Roll < W0 + W1)
		{
			const int32 MaxPack = FMath::FloorToInt(Remaining / WolfCost);
			AddWolfPack(FMath::Clamp(Random.RandRange(2, 3), 1, MaxPack));
		}
		else
		{
			AddSpawn(EEnemyType::Bear, false, 0.0f);
			Remaining -= BearCost;
		}
	}

	// Open each wave with a Basic so the first contact is readable, keep the rest shuffled
	// in pack-sized blocks so wolf packs stay together.
	TArray<TArray<FPlannedSpawn>> Blocks;
	for (const FPlannedSpawn& Spawn : Plan.Spawns)
	{
		if (Spawn.bPackFollower && Blocks.Num() > 0)
		{
			Blocks.Last().Add(Spawn);
		}
		else
		{
			Blocks.Add({ Spawn });
		}
	}
	for (int32 i = Blocks.Num() - 1; i > 0; --i)
	{
		Blocks.Swap(i, Random.RandRange(0, i));
	}
	const int32 FirstBasic = Blocks.IndexOfByPredicate([](const TArray<FPlannedSpawn>& Block)
	{
		return Block[0].Type == EEnemyType::Basic;
	});
	if (FirstBasic > 0)
	{
		Blocks.Swap(0, FirstBasic);
	}

	Plan.Spawns.Reset();
	for (const TArray<FPlannedSpawn>& Block : Blocks)
	{
		Plan.Spawns.Append(Block);
	}

	for (const FPlannedSpawn& Spawn : Plan.Spawns)
	{
		switch (Spawn.Type)
		{
		case EEnemyType::Wolf: ++Plan.WolfCount; break;
		case EEnemyType::Bear: ++Plan.BearCount; break;
		default:               ++Plan.BasicCount; break;
		}
	}

	// Stat scaling: steady growth per wave, nudged by how well the player is doing.
	const float Norm = NormalisedDifficulty();
	Plan.HealthMultiplier = (1.0f + 0.08f * (WaveNumber - 1)) * FMath::Lerp(0.85f, 1.15f, Norm);
	Plan.DamageMultiplier = (1.0f + 0.06f * (WaveNumber - 1)) * FMath::Lerp(0.9f, 1.1f, Norm);
	Plan.SpeedMultiplier = Difficulty > 1.2f ? 1.1f : 1.0f;
	// Catch-up economy: struggling players earn a little more per kill.
	Plan.RewardMultiplier = Difficulty < 1.0f ? 1.2f : 1.0f;

	CurrentPlan = Plan;
	RoundRobinLane = 0;

	UE_LOG(LogTemp, Display,
		TEXT("WaveDirector: wave %d plan - budget %.1f, %d Basic / %d Wolf / %d Bear, HP x%.2f, delay %.2fs | player %s, style %s (%d defenders)"),
		WaveNumber, Plan.ThreatBudget, Plan.BasicCount, Plan.WolfCount, Plan.BearCount,
		Plan.HealthMultiplier, StandardDelay, *GetSkillLabel(), *Profile.StyleLabel, Profile.DefenderCount);

	return Plan;
}

// ---------------------------------------------------------------------------------------------
// 4. Choose lanes
// ---------------------------------------------------------------------------------------------

float UWaveDirector::LaneCoverage(UWorld* World, const FEnemyPath& Path) const
{
	float Coverage = 0.0f;
	for (TActorIterator<ADefender> It(World); It; ++It)
	{
		const ADefender* Defender = *It;
		if (!Defender->HealthComponent || Defender->HealthComponent->IsDead())
		{
			continue;
		}

		const FVector DefenderLocation = Defender->GetActorLocation();
		const float RangeSq = FMath::Square(Defender->AttackRange);
		for (const FVector& Point : Path.Waypoints)
		{
			if (FVector::DistSquared2D(DefenderLocation, Point) <= RangeSq)
			{
				Coverage += Defender->GetThreatRating();
				break;
			}
		}
	}
	return Coverage;
}

int32 UWaveDirector::ChooseLane(UWorld* World, EEnemyType Type)
{
	const ATDGameMode* GameMode = GetTDGameMode(World);
	const AProceduralTerrain* Terrain = GameMode ? GameMode->GetTerrain() : nullptr;
	if (!Terrain || Terrain->GetEnemyPaths().Num() == 0)
	{
		return 0;
	}

	const TArray<FEnemyPath>& Paths = Terrain->GetEnemyPaths();

	// A struggling player gets an even spread, and so do Basics for a player who is holding.
	const bool bEvenSpread = Difficulty < 0.9f || (Type == EEnemyType::Basic && Difficulty <= 1.1f);
	if (bEvenSpread)
	{
		return RoundRobinLane++ % Paths.Num();
	}

	TArray<float> Weights;
	float Total = 0.0f;
	for (const FEnemyPath& Path : Paths)
	{
		const float Coverage = LaneCoverage(World, Path);
		// Wolves are drawn to defended lanes (they hunt defenders); Bears and confident
		// Basics probe the weakest lane.
		const float Weight = Type == EEnemyType::Wolf
			? 1.0f + Coverage
			: 1.0f / (1.0f + Coverage * 0.2f);
		Weights.Add(Weight);
		Total += Weight;
	}

	float Roll = Random.FRand() * Total;
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		Roll -= Weights[i];
		if (Roll <= 0.0f)
		{
			return i;
		}
	}
	return Weights.Num() - 1;
}
