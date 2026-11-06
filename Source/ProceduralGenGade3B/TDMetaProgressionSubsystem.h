
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TDMatchRewards.h"
#include "TDMetaProgressionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMetaCurrencyChanged);

UCLASS()
class PROCEDURALGENGADE3B_API UTDMetaProgressionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Meta")
    FOnMetaCurrencyChanged OnMetaCurrencyChanged;

    UFUNCTION(BlueprintPure, Category = "Meta")
    FMetaCurrencyRewards GetWallet() const { return Wallet; }

    UFUNCTION(BlueprintCallable, Category = "Meta")
    void AddRewards(const FMetaCurrencyRewards& Rewards);

    UFUNCTION(BlueprintCallable, Category = "Meta")
    bool TrySpend(const FMetaCurrencyRewards& Cost);

    UFUNCTION(BlueprintPure, Category = "Meta")
    bool CanAfford(const FMetaCurrencyRewards& Cost) const;

    /** Replaces the whole wallet. Used when a save is loaded. */
    void SetWallet(const FMetaCurrencyRewards& NewWallet);

    //  Defender upgrades bought while no matching unit is on the field (e.g. from the Start-menu
    //  shop). They are already paid for and activate when that unit type is first placed.
    void AddPendingUpgrade(int32 Index) { PendingUpgrades |= (1u << Index); }
    bool IsUpgradePending(int32 Index) const { return (PendingUpgrades & (1u << Index)) != 0; }

    bool ConsumePendingUpgrade(int32 Index)
    {
        const bool bWasPending = IsUpgradePending(Index);
        PendingUpgrades &= ~(1u << Index);
        return bWasPending;
    }

    void ClearPendingUpgrades()
    {
        PendingUpgrades = 0;
    }

private:
    UPROPERTY()
    FMetaCurrencyRewards Wallet;

    uint32 PendingUpgrades = 0; //  one bit per upgrade (Basic, Archer, Poison, Strong, Tower)
};
