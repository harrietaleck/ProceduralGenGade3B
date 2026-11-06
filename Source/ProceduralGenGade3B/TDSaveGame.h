// TDSaveGame.h
// Data written to the persistent save slot. A match is saved as a checkpoint at the end of a
// wave: the map is rebuilt from its seed, so defenders go back on the exact same build pads.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TDMatchRewards.h"
#include "TDSaveGame.generated.h"

USTRUCT()
struct FTDSavedDefender
{
	GENERATED_BODY()

	/** Defender class, e.g. /Script/ProceduralGenGade3B.ArcherDefender. */
	UPROPERTY()
	FSoftClassPath DefenderClass;

	/** The build pad the defender stands on. */
	UPROPERTY()
	FVector SlotLocation = FVector::ZeroVector;

	/** Damage already taken, so a restored defender is not healed. */
	UPROPERTY()
	float DamageTaken = 0.0f;
};

UCLASS()
class PROCEDURALGENGADE3B_API UTDSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 SaveVersion = CurrentVersion;

	UPROPERTY()
	FDateTime SavedAt;

	//  Shared music volume saved alongside the existing match checkpoint.
	UPROPERTY()
	float MusicVolume = 0.5f;

	UPROPERTY()
	int32 TowerMaxHealthBonus = 0;

	/** Meta currencies (leaf, wood, gem and sun icons). Kept between matches and app restarts. */
	UPROPERTY()
	FMetaCurrencyRewards Wallet;

	/** False when there is no match to resume (e.g. the last match was won). */
	UPROPERTY()
	bool bHasMatch = false;

	UPROPERTY()
	FString LevelName;

	UPROPERTY()
	int32 TerrainSeed = 0;

	/** Waves fully cleared; resuming starts the next one. */
	UPROPERTY()
	int32 WavesCleared = 0;

	/** Loot, the in-match resource. */
	UPROPERTY()
	int32 Loot = 0;

	UPROPERTY()
	int32 BeamUpgradeLevel = 0;

	UPROPERTY()
	float TowerDamageTaken = 0.0f;

	UPROPERTY()
	int32 DefendersPlaced = 0;

	UPROPERTY()
	int32 HitsLanded = 0;

	UPROPERTY()
	int32 EnemiesKilled = 0;

	/** Match rewards already paid into the wallet, so later waves only pay the difference. */
	UPROPERTY()
	FMetaCurrencyRewards PaidMatchRewards;

	UPROPERTY()
	TArray<FTDSavedDefender> Defenders;

	/** Legacy fields retained for save compatibility; wave-scoped upgrades are no longer persisted. */
	UPROPERTY()
	TArray<uint8> UpgradePhases;

	UPROPERTY()
	TArray<float> UpgradeSecondsLeft;

	/** True if the match part of the save can be resumed. */
	bool IsResumable() const
	{
		return bHasMatch && SaveVersion == CurrentVersion && !LevelName.IsEmpty()
			&& TerrainSeed != 0 && WavesCleared > 0;
	}
};
