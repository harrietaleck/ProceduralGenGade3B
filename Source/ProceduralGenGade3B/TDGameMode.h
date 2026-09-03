// TDGameMode.h
// The central coordinator for a match. On BeginPlay it locates the procedural terrain,
// spawns the tower at the terrain's central tower location, and starts the enemy spawner.
// It also owns the resource economy and the game-over state that the UI reads.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TDMatchRewards.h"
#include "TDGameMode.generated.h"

class AProceduralTerrain;
class ATower;
class AEnemySpawner;
class AEnemy;
class AWaveManager;
class ABuildPadMarker;
class UTDHUDWidget;
class UTDEndScreenWidget;
class UTDWarningBannerWidget;

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

	/** Loot deducted after each cleared wave for every living defender (lecture: economy
	 *  grows from kills but drains while defenders are fielded). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules", meta = (ClampMin = "0"))
	int32 DefenderUpkeepPerWave = 8;

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

	/** The always-visible UMG match HUD (Widget Blueprint child of UTDHUDWidget). Created and
	 *  wired up once Tower/WaveManager exist, at the end of BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<UTDHUDWidget> HUDWidgetClass;

	/** Full-screen game over / victory overlay (C++ widget by default). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<UTDEndScreenWidget> EndScreenWidgetClass;

	/** Meta-currency granted on the first match if the wallet is empty (lets defenders work immediately). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules|Meta")
	FMetaCurrencyRewards StartingMetaWallet;

	/** Light Lantern cost for each tower beam upgrade during a match. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules|Meta", meta = (ClampMin = "1"))
	int32 BeamUpgradeLanternCost = 12;

	/** Extra tower damage per beam upgrade level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules|Meta", meta = (ClampMin = "0.1"))
	float BeamUpgradeDamageBonus = 8.0f;

	/** Maximum beam upgrades purchasable in one match. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules|Meta", meta = (ClampMin = "1"))
	int32 MaxBeamUpgradeLevel = 5;

	/** Active match HUD widget instance (may be null if no WBP asset is configured). */
	UFUNCTION(BlueprintPure, Category = "Rules")
	UTDHUDWidget* GetMatchHUDWidget() const { return MatchHUDWidget; }

	/** Bottom-centre warning when defender placement is rejected (e.g. not enough Loot). */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void ShowInsufficientFundsWarning();

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

	/** Toggle match pause (freezes gameplay actors; UI remains visible). */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void TogglePause();

	/** True while the match is paused. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsPaused() const { return bPaused; }

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

	/** Score + meta-currency rewards from the most recently finished match. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	FMatchResult GetLastMatchResult() const { return LastMatchResult; }

	/** Current persistent meta-currency wallet. */
	UFUNCTION(BlueprintPure, Category = "Rules|Meta")
	FMetaCurrencyRewards GetMetaWallet() const;

	/** Spend meta-currency if the wallet can afford it. */
	UFUNCTION(BlueprintCallable, Category = "Rules|Meta")
	bool TrySpendMeta(const FMetaCurrencyRewards& Cost);

	/** True when the wallet can cover a defender or upgrade cost. */
	UFUNCTION(BlueprintPure, Category = "Rules|Meta")
	bool CanAffordMeta(const FMetaCurrencyRewards& Cost) const;

	/** True while paused, defeated, or victorious — blocks placement and upgrades. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsInteractionBlocked() const;

	/** Push current meta-currency totals into the match HUD. */
	void RefreshMetaHUD() const;

	/** Spend Light Lanterns to permanently boost the tower beam for this match. */
	UFUNCTION(BlueprintCallable, Category = "Rules|Meta")
	bool TryUpgradeTowerBeam();

	/** How many beam upgrades have been purchased this match. */
	UFUNCTION(BlueprintPure, Category = "Rules|Meta")
	int32 GetBeamUpgradeLevel() const { return BeamUpgradeLevel; }

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

	/** True while the player has paused the match. */
	bool bPaused = false;

	// Cached references to the key actors.
	UPROPERTY()
	TObjectPtr<AProceduralTerrain> Terrain;

	UPROPERTY()
	TObjectPtr<ATower> Tower;

	UPROPERTY()
	TObjectPtr<AEnemySpawner> Spawner;

	UPROPERTY()
	TObjectPtr<AWaveManager> WaveManager;

	UPROPERTY()
	TObjectPtr<UTDHUDWidget> MatchHUDWidget;

	UPROPERTY()
	TObjectPtr<UTDEndScreenWidget> EndScreenWidget;

	UPROPERTY()
	TObjectPtr<UTDWarningBannerWidget> WarningBannerWidget;

	/** Visual platforms spawned on every generated build pad this attempt — tracked so a failed
	 *  world-validation pass can tear them down before regenerating. */
	UPROPERTY()
	TArray<TObjectPtr<ABuildPadMarker>> BuildPadMarkers;

	/** Find the terrain actor already placed in the level. */
	AProceduralTerrain* FindTerrain() const;

	/** Spawns the tower on the terrain's published tower location, retrying with a fresh terrain
	 *  regeneration if the spawn is ever rejected or lands away from that location. Returns false
	 *  if no valid placement was reached within the attempt budget. */
	bool SpawnTowerWithRetry();

	/** Spawns a visual platform on every one of the terrain's current build slots. */
	void SpawnBuildPadMarkers();

	/** Spawns markers only for build slots added since the last call (used after path expansion). */
	void SpawnBuildPadMarkersFromIndex(int32 StartSlotIndex);

	/** Deduct upkeep for all living defenders after a wave ends. */
	void ApplyDefenderUpkeep();

	/** Wave-complete hook: extend lanes, add new pads, charge upkeep. */
	UFUNCTION()
	void HandleWaveComplete(int32 WaveNumber);

	/** Computes rewards from tower beam health, banks meta-currency, then shows the end screen. */
	UFUNCTION()
	void HandleMatchVictory();

	void FinalizeMatchResult(bool bVictory);
	void ShowEndScreen(bool bVictory);

	UPROPERTY()
	FMatchResult LastMatchResult;

	int32 BeamUpgradeLevel = 0;
	float BaseTowerAttackDamage = 0.0f;

	void EnsureStartingMetaWallet();

	/** Resolve default Match HUD / End Screen Widget Blueprints without ConstructorHelpers. */
	void EnsureDefaultWidgetClasses();

	/** Destroys the tower and every build-pad marker spawned so far, so a failed world-validation
	 *  attempt can regenerate cleanly rather than leaving stale actors from the last attempt. */
	void DestroySpawnedWorldActors();

	/** Final gate before gameplay (enemy spawning, HUD) is allowed to start: re-checks the whole
	 *  placed world — terrain, tower, build pads, navigation — as actually spawned, not just the
	 *  terrain's own self-validation during generation. */
	bool ValidateWorldBeforeGameplay() const;
};
