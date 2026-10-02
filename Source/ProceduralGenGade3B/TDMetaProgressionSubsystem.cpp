// TDMetaProgressionSubsystem.cpp — see TDMetaProgressionSubsystem.h

#include "TDMetaProgressionSubsystem.h"

void UTDMetaProgressionSubsystem::AddRewards(const FMetaCurrencyRewards& Rewards)
{
	Wallet.ForestEssence += Rewards.ForestEssence;
	Wallet.WoodenMight += Rewards.WoodenMight;
	Wallet.GemStones += Rewards.GemStones;
	Wallet.LightLanterns += Rewards.LightLanterns;
	OnMetaCurrencyChanged.Broadcast();
}

bool UTDMetaProgressionSubsystem::CanAfford(const FMetaCurrencyRewards& Cost) const
{
	return Wallet.ForestEssence >= Cost.ForestEssence
		&& Wallet.WoodenMight >= Cost.WoodenMight
		&& Wallet.GemStones >= Cost.GemStones
		&& Wallet.LightLanterns >= Cost.LightLanterns;
}

bool UTDMetaProgressionSubsystem::TrySpend(const FMetaCurrencyRewards& Cost)
{
	if (!CanAfford(Cost))
	{
		return false;
	}

	Wallet.ForestEssence -= Cost.ForestEssence;
	Wallet.WoodenMight -= Cost.WoodenMight;
	Wallet.GemStones -= Cost.GemStones;
	Wallet.LightLanterns -= Cost.LightLanterns;
	OnMetaCurrencyChanged.Broadcast();
	return true;
}
