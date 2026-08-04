// TDGameMode.h
// The central coordinator for a match. On BeginPlay it locates the procedural terrain,
// spawns the tower at the terrain's central tower location, and starts the enemy spawner.
// It also owns the resource economy and the game-over state that the UI reads.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TDGameMode.generated.h"

class AProceduralTerrain;
class ATower;
class AEnemySpawner;
class AEnemy;
class AWaveManager;
class ABuildPadMarker;

// Broadcast whenever the player's resource count changes (UI binds to this).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResourcesChanged, int32, NewAmount);

// Broadcast once when the tower is destroyed and the game ends.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameOver);

UCLASS()
class PROCEDURALGENGADE3B_API ATDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATDGameMode();

	/** Loot the player starts with (Economy spec: 200). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules", meta = (ClampMin = "0"))
	int32 StartingResources = 200;

	/** Which tower class to spawn (defaults to the C++ ATower; can be a Blueprint child). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<ATower> TowerClass;

	/** Which spawner class to use (defaults to the C++ AEnemySpawner; can be a Blueprint child). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<AEnemySpawner> SpawnerClass;

	/** Which wave manager class to use (defaults to the C++ AWaveManager; can be a Blueprint child). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<AWaveManager> WaveManagerClass;

	/** Visual marker spawned on every generated build pad (defaults to ABuildPadMarker). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<ABuildPadMarker> BuildPadMarkerClass;

	/** Fired when resources change. */
	UPROPERTY(BlueprintAssignable, Category = "Rules")
	FOnResourcesChanged OnResourcesChanged;

	/** Fired when the game ends (tower destroyed). */
	UPROPERTY(BlueprintAssignable, Category = "Rules")
	FOnGameOver OnGameOver;

	// ---- Economy API (used by the player controller when placing defenders) ----

	/** Current resource total. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetResources() const { return Resources; }

	/** Add resources (e.g. an enemy bounty) and notify listeners. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void AddResources(int32 Amount);

	/** Spend resources if affordable. Returns true and deducts on success, false otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	bool TrySpendResources(int32 Amount);

	/** True once the tower has been destroyed. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsGameOver() const { return bGameOver; }

	/** True once the final configured wave has been cleared. WaveManager is the sole
	 *  authority for this — GameMode only forwards the query, it never tracks its own copy. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsVictory() const;

	/** Reload the current level for a fresh game (regenerates terrain, resets economy). */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void RestartGame();

	/** The procedural terrain located at startup (source of paths / slots / tower location). */
	UFUNCTION(BlueprintPure, Category = "Rules")
	AProceduralTerrain* GetTerrain() const { return Terrain; }

	/** The spawned tower. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	ATower* GetTower() const { return Tower; }

	/** The wave manager driving enemy waves (may be null before BeginPlay finishes). */
	UFUNCTION(BlueprintPure, Category = "Rules")
	AWaveManager* GetWaveManager() const { return WaveManager; }

	/** Convenience for the HUD: the current wave number (0 if waves haven't started). */
	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetCurrentWave() const;

	// ---- Notifications called by other actors ----

	/** Called by an enemy when it dies: award its bounty. */
	void NotifyEnemyKilled(AEnemy* DeadEnemy);

	/** Called by the tower when it dies: end the game. */
	void NotifyTowerDestroyed();

protected:
	virtual void BeginPlay() override;

private:
	/** Live resource total. */
	UPROPERTY(VisibleAnywhere, Category = "Rules", meta = (AllowPrivateAccess = "true"))
	int32 Resources = 0;

	/** True after the tower is destroyed. */
	bool bGameOver = false;

	// Cached references to the key actors.
	UPROPERTY()
	TObjectPtr<AProceduralTerrain> Terrain;

	UPROPERTY()
	TObjectPtr<ATower> Tower;

	UPROPERTY()
	TObjectPtr<AEnemySpawner> Spawner;

	UPROPERTY()
	TObjectPtr<AWaveManager> WaveManager;

	/** Find the terrain actor already placed in the level. */
	AProceduralTerrain* FindTerrain() const;
};
