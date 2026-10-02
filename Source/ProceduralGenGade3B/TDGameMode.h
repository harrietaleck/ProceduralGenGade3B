// TDGameMode.h
// Runs the match. On BeginPlay it finds the terrain, spawns the tower in the middle and starts the enemies.
// It also holds the Loot economy and the game over state that the UI reads.

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
class UUserWidget;
class UButton;

// Fires whenever the player's resources change. The UI listens to this.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResourcesChanged, int32, NewAmount);

// Fires once when the tower is destroyed and the game ends.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameOver);

UCLASS()
class PROCEDURALGENGADE3B_API ATDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATDGameMode();

	/** Loot the player starts with. The economy spec says 200. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules", meta = (ClampMin = "0"))
	int32 StartingResources = 200;

	/** Loot taken away for every living defender after each cleared wave.
	 *  Kills grow the economy, but keeping defenders out slowly drains it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules", meta = (ClampMin = "0"))
	int32 DefenderUpkeepPerWave = 8;

	/** Tower class to spawn. Defaults to the C++ ATower, but a Blueprint child works too. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<ATower> TowerClass;

	/** Enemy spawner class to use. Defaults to the C++ AEnemySpawner, but a Blueprint child works too. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<AEnemySpawner> SpawnerClass;

	/** Wave manager class to use. Defaults to the C++ AWaveManager, but a Blueprint child works too. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<AWaveManager> WaveManagerClass;

	/** Marker spawned on every generated build pad so the player can see it. Defaults to ABuildPadMarker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<ABuildPadMarker> BuildPadMarkerClass;

	/** The match HUD that is always on screen. It should be a Widget Blueprint child of UTDHUDWidget.
	 *  It gets created at the end of BeginPlay, once the tower and wave manager exist. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<UTDHUDWidget> HUDWidgetClass;

	/** Full screen defeat overlay. It uses the C++ widget by default, so put the Gameoverscreen BP here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<UTDEndScreenWidget> EndScreenWidgetClass;

	/** Full screen victory overlay. The plain VictoryScreen UserWidget works here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<UUserWidget> VictoryScreenWidgetClass;

	/** Pause and settings overlay. This is a Blueprint UserWidget and the settings controls stay in Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules")
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	/** Meta currency given on the first match if the wallet is empty, so the player can buy defenders straight away. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules|Meta")
	FMetaCurrencyRewards StartingMetaWallet;

	/** How many Light Lanterns each tower beam upgrade costs during a match. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules|Meta", meta = (ClampMin = "1"))
	int32 BeamUpgradeLanternCost = 12;

	/** Extra tower damage per beam upgrade level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules|Meta", meta = (ClampMin = "0.1"))
	float BeamUpgradeDamageBonus = 8.0f;

	/** Most beam upgrades the player can buy in one match. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rules|Meta", meta = (ClampMin = "1"))
	int32 MaxBeamUpgradeLevel = 5;

	/** The match HUD widget in use. Can be null if no WBP asset was set. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	UTDHUDWidget* GetMatchHUDWidget() const { return MatchHUDWidget; }

	/** Shows a warning at the bottom centre when a defender can't be placed, like when there isn't enough Loot. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void ShowInsufficientFundsWarning();

	/** Fired when resources change. */
	UPROPERTY(BlueprintAssignable, Category = "Rules")
	FOnResourcesChanged OnResourcesChanged;

	/** Fired when the game ends because the tower was destroyed. */
	UPROPERTY(BlueprintAssignable, Category = "Rules")
	FOnGameOver OnGameOver;

	// ---- Economy functions, used by the player controller when placing defenders ----

	/** How many resources the player has right now. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetResources() const { return Resources; }

	/** Adds resources, like an enemy bounty, and tells anything listening. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void AddResources(int32 Amount);

	/** Spends the resources if the player can afford it. Returns false if there isn't enough. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	bool TrySpendResources(int32 Amount);

	/** True once the tower has been destroyed. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsGameOver() const { return bGameOver; }

	/** True once the last wave has been cleared. The wave manager owns this value.
	 *  The game mode just asks it and never keeps its own copy. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsVictory() const;

	/** Reloads the level for a fresh game. This makes new terrain and resets the economy. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void RestartGame();

	/** Leaves the match and opens the start menu level. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void ReturnToMainMenu();

	/** Hides the results screen between waves and starts the next wave. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void ContinueToNextWave();

	/** Hides the victory results and plays the wave we just finished again. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void RetryCurrentWave();

	/** Pauses or unpauses the match and shows or hides the settings widget. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void TogglePause();

	/** Opens the settings widget and pauses the match. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void ShowSettings();

	/** Closes settings and carries on with the match. Call this from the Blueprint Resume buttons. */
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void ResumeFromSettings();

	/** True while the match is paused. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsPaused() const { return bPaused; }

	/** True while the settings overlay is visible. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsSettingsVisible() const;

	/** The terrain found at startup. It gives us the paths, build slots and tower location. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	AProceduralTerrain* GetTerrain() const { return Terrain; }

	/** The spawned tower. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	ATower* GetTower() const { return Tower; }

	/** The wave manager that runs the enemy waves. Can be null before BeginPlay is done. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	AWaveManager* GetWaveManager() const { return WaveManager; }

	/** Current wave number for the HUD. Returns 0 if the waves haven't started yet. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetCurrentWave() const;

	/** Score and meta currency rewards from the last finished match. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	FMatchResult GetLastMatchResult() const { return LastMatchResult; }

	/** The meta currency wallet that is kept between matches. */
	UFUNCTION(BlueprintPure, Category = "Rules|Meta")
	FMetaCurrencyRewards GetMetaWallet() const;

	/** Spends meta currency if the wallet has enough. */
	UFUNCTION(BlueprintCallable, Category = "Rules|Meta")
	bool TrySpendMeta(const FMetaCurrencyRewards& Cost);

	/** True when the wallet has enough for a defender or upgrade. */
	UFUNCTION(BlueprintPure, Category = "Rules|Meta")
	bool CanAffordMeta(const FMetaCurrencyRewards& Cost) const;

	/** True while paused, after a loss or after a win. The player can't place or upgrade then. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsInteractionBlocked() const;

	/** Sends the current meta currency totals to the match HUD. */
	void RefreshMetaHUD() const;

	/** Current match score, worked out from placements, hits, kills and surviving defenders. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetLiveMatchScore() const;

	/** Spends Light Lanterns to make the tower beam stronger for the rest of this match. */
	UFUNCTION(BlueprintCallable, Category = "Rules|Meta")
	bool TryUpgradeTowerBeam();

	/** Defenders placed so far this match. The wave director uses this to count losses per wave. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetMatchDefendersPlaced() const { return MatchDefendersPlaced; }

	/** How many defender status combos, like Shatter or Venom spread, happened this match. */
	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetMatchCombos() const { return MatchCombos; }

	/** Adds one to the defender status combo count. */
	void NotifyCombo() { ++MatchCombos; }

	/** How many beam upgrades have been bought this match. */
	UFUNCTION(BlueprintPure, Category = "Rules|Meta")
	int32 GetBeamUpgradeLevel() const { return BeamUpgradeLevel; }

	// ---- Notifications called by other actors ----

	/** Called by an enemy when it dies. Gives the player its bounty. */
	void NotifyEnemyKilled(AEnemy* DeadEnemy);

	/** Counts a hit on an enemy from the tower, a defender or a projectile. */
	void NotifyEnemyHit();

	/** Counts a defender that was placed this match. */
	void NotifyDefenderPlaced();

	/** Called by the tower when it dies. Ends the game. */
	void NotifyTowerDestroyed();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** The player's current resources. */
	UPROPERTY(VisibleAnywhere, Category = "Rules", meta = (AllowPrivateAccess = "true"))
	int32 Resources = 0;

	/** True after the tower is destroyed. */
	bool bGameOver = false;

	/** True while the player has paused the match. */
	bool bPaused = false;

	// Saved pointers to the main actors so we don't have to search for them again.
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
	TObjectPtr<UTDEndScreenWidget> EndScreenWidget;   // the defeat screen

	UPROPERTY()
	TObjectPtr<UUserWidget> VictoryScreenWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> SettingsWidget;

	UPROPERTY()
	TObjectPtr<UTDWarningBannerWidget> WarningBannerWidget;

	FTimerHandle MenuStretchTimerHandle;

	/** Platforms spawned on every build pad in this attempt. We keep track of them so they
	 *  can be removed if the world check fails and the terrain has to be made again. */
	UPROPERTY()
	TArray<TObjectPtr<ABuildPadMarker>> BuildPadMarkers;

	/** Finds the terrain actor that is already placed in the level. */
	AProceduralTerrain* FindTerrain() const;

	/** Spawns the tower at the terrain's tower location. If the spawn fails or lands in the
	 *  wrong place, it makes new terrain and tries again. Returns false if it runs out of tries. */
	bool SpawnTowerWithRetry();

	/** Spawns a platform on every build slot the terrain has right now. */
	void SpawnBuildPadMarkers();

	/** Spawns markers just for the build slots added since last time. Used after the paths grow. */
	void SpawnBuildPadMarkersFromIndex(int32 StartSlotIndex);

	/** Takes upkeep for every living defender after a wave ends. */
	void ApplyDefenderUpkeep();

	/** Runs when a wave is cleared. Grows the lanes, adds new build pads, charges upkeep and shows the results. */
	UFUNCTION()
	void HandleWaveComplete(int32 WaveNumber);

	/** Runs when the final wave is won. Does nothing if the results already showed when that wave was cleared. */
	UFUNCTION()
	void HandleMatchVictory();

	void FinalizeMatchResult(bool bVictory, int32 WavesClearedOverride = -1);
	void ShowEndScreen(bool bVictory);
	void HideEndScreens();
	void RestoreGameplayInput();
	void BindSettingsButtons();
	void BindVictoryScreenButtons();
	void PresentVictoryScreen();

	UPROPERTY()
	FMatchResult LastMatchResult;

	/** Total rewards already paid out this match, so each wave only pays the difference. */
	FMetaCurrencyRewards PaidMatchRewards;

	bool bWaveResultsVisible = false;

	int32 BeamUpgradeLevel = 0;
	float BaseTowerAttackDamage = 0.0f;

	int32 MatchDefendersPlaced = 0;
	int32 MatchHitsLanded = 0;
	int32 MatchEnemiesKilled = 0;
	int32 MatchCombos = 0;

	void EnsureStartingMetaWallet();

	/** Loads the default HUD and end screen Widget Blueprints without using ConstructorHelpers. */
	void EnsureDefaultWidgetClasses();

	/** Destroys the tower and all build pad markers spawned so far. This way a failed world
	 *  check can start over clean, without old actors left behind from the last try. */
	void DestroySpawnedWorldActors();

	/** Last check before enemies and the HUD start. It looks at the terrain, tower, build pads
	 *  and navigation as they were really spawned, not just the terrain's own check during generation. */
	bool ValidateWorldBeforeGameplay() const;
};
