
#include "TDMetaProgressionSubsystem.h"

// Add rewards.
void UTDMetaProgressionSubsystem::AddRewards(const FMetaCurrencyRewards& Rewards)
{
    Wallet.ForestEssence += Rewards.ForestEssence;
    Wallet.WoodenMight += Rewards.WoodenMight;
    Wallet.GemStones += Rewards.GemStones;
    Wallet.LightLanterns += Rewards.LightLanterns;

    OnMetaCurrencyChanged.Broadcast();
}

// Replaces the whole wallet. Used when a save is loaded.
void UTDMetaProgressionSubsystem::SetWallet(const FMetaCurrencyRewards& NewWallet)
{
    Wallet.ForestEssence = FMath::Max(0, NewWallet.ForestEssence);
    Wallet.WoodenMight = FMath::Max(0, NewWallet.WoodenMight);
    Wallet.GemStones = FMath::Max(0, NewWallet.GemStones);
    Wallet.LightLanterns = FMath::Max(0, NewWallet.LightLanterns);

    OnMetaCurrencyChanged.Broadcast();
}

// Can afford.
bool UTDMetaProgressionSubsystem::CanAfford(const FMetaCurrencyRewards& Cost) const
{
    return Wallet.ForestEssence >= Cost.ForestEssence
        && Wallet.WoodenMight >= Cost.WoodenMight
        && Wallet.GemStones >= Cost.GemStones
        && Wallet.LightLanterns >= Cost.LightLanterns;
}

// Try spend.
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
