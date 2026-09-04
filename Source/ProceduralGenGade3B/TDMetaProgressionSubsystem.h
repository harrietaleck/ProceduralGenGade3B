// TDMetaProgressionSubsystem.h
// Banks meta-currency between matches (defenders, beam upgrades, elite units).

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

private:
	UPROPERTY()
	FMetaCurrencyRewards Wallet;
};
