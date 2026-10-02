// TDMatchRewards.h
// Works out the meta currency rewards shown on the victory and defeat screens.
// The reward tier depends on how much beam health the tower has left.

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

/** Forest resources earned after a match. These are kept between matches. */
USTRUCT(BlueprintType)
struct FMetaCurrencyRewards
{
	GENERATED_BODY()

	/** Leaf currency. Used for basic defenders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 ForestEssence = 0;

	/** Log currency. Used for basic defenders and upkeep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 WoodenMight = 0;

	/** Gem currency. Used for elite and strong defenders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 GemStones = 0;

	/** Lantern currency. Used to upgrade the tower beam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 LightLanterns = 0;
};

/** Everything the end screen needs to show after a match. */
USTRUCT(BlueprintType)
struct FMatchResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bVictory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 Score = 0;

	/** Tower beam health left at the end of the match, as a percentage from 0 to 100. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 TowerBeamHealthPercent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 WavesCleared = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 TotalWaves = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 DefendersPlaced = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 HitsLanded = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 EnemiesKilled = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 SurvivingDefenders = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	EBeamHealthTier BeamTier = EBeamHealthTier::Fragile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FMetaCurrencyRewards Rewards;

	/** Wallet totals from the HUD at the moment the match ended. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FMetaCurrencyRewards Wallet;
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
		int32 TotalWaves,
		int32 DefendersPlaced = 0,
		int32 HitsLanded = 0,
		int32 EnemiesKilled = 0,
		int32 SurvivingDefenders = 0);
};
