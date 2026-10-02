// TDHUD.h
// A lightweight, code-only heads-up display. It reads live values from the game mode each
// frame and draws them straight onto the screen with the Canvas: defender/enemy health bars,
// and a readable info panel (defenders, cost, seed, controls).
//
// Match HUD values (loot, waves, tower health) and end-of-match screens are handled by UMG
// widgets (UTDHUDWidget / UTDEndScreenWidget) created by the game mode.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WaveDirector.h"
#include "TDHUD.generated.h"

class ATDGameMode;
class AWaveManager;

UCLASS()
class PROCEDURALGENGADE3B_API ATDHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** Called every frame by the engine to paint the HUD. */
	virtual void DrawHUD() override;

	/** Show or hide the wave director graph (G). It also appears automatically when the match ends. */
	void ToggleDirectorGraph() { bShowDirectorGraph = !bShowDirectorGraph; }

private:
	bool bShowDirectorGraph = false;

	/** Lane forecast, refreshed on real time so it keeps updating while the game is paused. */
	TArray<FLaneForecast> CachedForecast;
	double LastForecastRefresh = -1.0;

	/** Between waves: next wave summary plus a marker over every lane's spawn point. */
	void DrawWaveForecast(ATDGameMode* GameMode, AWaveManager* WaveManager);

	/** Difficulty rating and wave score per wave, with the 65% target band. */
	void DrawDirectorGraph(const UWaveDirector* Director);

	/** Draw pause overlay and the readable info panel (defenders, cost, seed, controls). */
	void DrawInfoPanel(ATDGameMode* GameMode);

	/** Draw a line of HUD text on top of the info panel with consistent sizing. */
	void DrawPanelText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale);

	/** Draw a small floating health bar over every living defender. */
	void DrawDefenderHealthBars();

	/** Draw a small floating health bar over every living enemy. */
	void DrawEnemyHealthBars();

	/** Projects a world location to screen space and draws a background + health-coloured
	 *  fill bar there. Shared by defender (and future enemy/tower) health bars. */
	void DrawWorldHealthBar(const FVector& WorldLocation, float HealthPercent, float BarWidth, float BarHeight);
};
