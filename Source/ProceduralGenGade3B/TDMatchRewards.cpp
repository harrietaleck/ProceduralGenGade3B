// TDMatchRewards.cpp — see TDMatchRewards.h

#include "TDMatchRewards.h"

namespace
{
	static float TierMultiplier(EBeamHealthTier Tier)
	{
		switch (Tier)
		{
		case EBeamHealthTier::Fragile: return 0.45f;
		case EBeamHealthTier::Steady:  return 1.0f;
		case EBeamHealthTier::Radiant: return 1.55f;
		default: return 1.0f;
		}
	}

	static int32 ScaleReward(int32 BaseAmount, float TierMult, bool bVictory)
	{
		const float OutcomeMult = bVictory ? 1.0f : 0.35f;
		return FMath::Max(0, FMath::RoundToInt(BaseAmount * TierMult * OutcomeMult));
	}
}

EBeamHealthTier UTDMatchRewards::GetBeamHealthTier(int32 BeamHealthPercent)
{
	const int32 Clamped = FMath::Clamp(BeamHealthPercent, 0, 100);
	if (Clamped <= 30)
	{
		return EBeamHealthTier::Fragile;
	}
	if (Clamped <= 70)
	{
		return EBeamHealthTier::Steady;
	}
	return EBeamHealthTier::Radiant;
}

int32 UTDMatchRewards::GetRewardFontSize(EBeamHealthTier Tier)
{
	switch (Tier)
	{
	case EBeamHealthTier::Fragile: return 20;
	case EBeamHealthTier::Steady:  return 26;
	case EBeamHealthTier::Radiant: return 32;
	default: return 24;
	}
}

int32 UTDMatchRewards::GetScoreFontSize(EBeamHealthTier Tier)
{
	switch (Tier)
	{
	case EBeamHealthTier::Fragile: return 30;
	case EBeamHealthTier::Steady:  return 40;
	case EBeamHealthTier::Radiant: return 50;
	default: return 36;
	}
}

FMatchResult UTDMatchRewards::BuildMatchResult(
	bool bVictory,
	float TowerCurrentHealth,
	float TowerMaxHealth,
	int32 WavesCleared,
	int32 TotalWaves)
{
	FMatchResult Result;
	Result.bVictory = bVictory;

	const float HealthFraction = (TowerMaxHealth > 0.0f)
		? FMath::Clamp(TowerCurrentHealth / TowerMaxHealth, 0.0f, 1.0f)
		: 0.0f;
	Result.TowerBeamHealthPercent = FMath::RoundToInt(HealthFraction * 100.0f);
	Result.BeamTier = GetBeamHealthTier(Result.TowerBeamHealthPercent);

	const float TierMult = TierMultiplier(Result.BeamTier);
	const int32 WaveBonus = FMath::Max(0, WavesCleared);
	Result.Score = ScaleReward(WaveBonus * 180, 1.0f, bVictory)
		+ ScaleReward(Result.TowerBeamHealthPercent * 12, 1.0f, bVictory)
		+ (bVictory ? 400 : 0);

	FMetaCurrencyRewards Base;
	Base.ForestEssence = 90 + WaveBonus * 18;
	Base.WoodenMight = 70 + WaveBonus * 14;
	Base.GemStones = 20 + WaveBonus * 6;
	Base.LightLanterns = 55 + WaveBonus * 10;

	Result.Rewards.ForestEssence = ScaleReward(Base.ForestEssence, TierMult, bVictory);
	Result.Rewards.WoodenMight = ScaleReward(Base.WoodenMight, TierMult, bVictory);
	Result.Rewards.GemStones = ScaleReward(Base.GemStones, TierMult, bVictory);
	Result.Rewards.LightLanterns = ScaleReward(Base.LightLanterns, TierMult, bVictory);

	if (TotalWaves > 0 && WavesCleared >= TotalWaves && bVictory)
	{
		Result.Rewards.GemStones += 15;
		Result.Rewards.LightLanterns += 20;
	}

	return Result;
}
