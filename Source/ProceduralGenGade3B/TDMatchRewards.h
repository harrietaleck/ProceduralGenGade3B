// TDMatchRewards.h
// Meta-currency reward calculation for victory / defeat screens.
// Reward tier is driven by the tower's remaining beam health (0-100%).

#pragma once

#include "CoreMinimal.h"
#include "TDMatchRewards.generated.h"

/** How much beam health remained when the match ended. */
UENUM(BlueprintType)
enum class EBeamHealthTier : uint8
{
	Fragile UMETA(DisplayName = "Fragile (0-30%)"),
	Steady  UMETA(DisplayName = "Steady (31-70%)"),
	Radiant UMETA(DisplayName = "Radiant (71-100%)")
};

/** Persistent forest resources earned after a match. */
USTRUCT(BlueprintType)
struct FMetaCurrencyRewards
{
	GENERATED_BODY()

	/** Leaf currency — basic defenders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 ForestEssence = 0;

	/** Log currency — basic defenders and upkeep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 WoodenMight = 0;

	/** Gem currency — elite / strong defenders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 GemStones = 0;

	/** Lantern currency — tower beam power upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 LightLanterns = 0;
};

/** Everything the end-screen needs to display after a match. */
USTRUCT(BlueprintType)
struct FMatchResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bVictory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 Score = 0;

	/** Tower beam health as a 0..100 percentage at match end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 TowerBeamHealthPercent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	EBeamHealthTier BeamTier = EBeamHealthTier::Fragile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FMetaCurrencyRewards Rewards;
};

UCLASS()
class PROCEDURALGENGADE3B_API UTDMatchRewards : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Match|Rewards")
	static EBeamHealthTier GetBeamHealthTier(int32 BeamHealthPercent);

	UFUNCTION(BlueprintPure, Category = "Match|Rewards")
	static int32 GetRewardFontSize(EBeamHealthTier Tier);

	UFUNCTION(BlueprintPure, Category = "Match|Rewards")
	static int32 GetScoreFontSize(EBeamHealthTier Tier);

	UFUNCTION(BlueprintCallable, Category = "Match|Rewards")
	static FMatchResult BuildMatchResult(
		bool bVictory,
		float TowerCurrentHealth,
		float TowerMaxHealth,
		int32 WavesCleared,
		int32 TotalWaves);
};
