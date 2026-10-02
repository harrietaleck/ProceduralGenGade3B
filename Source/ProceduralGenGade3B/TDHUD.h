// TDHUD.h
// Simple Canvas HUD that draws health bars and the info panel every frame.
// Loot, waves, tower health and the end screens are done in UMG widgets instead.

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
	/** The engine calls this every frame to draw the HUD. */
	virtual void DrawHUD() override;

	/** Show or hide the wave director graph (G). It also appears automatically when the match ends. */
	void ToggleDirectorGraph() { bShowDirectorGraph = !bShowDirectorGraph; }

private:
	bool bShowDirectorGraph = false;

	/** Lane forecast. It refreshes on real time so it still updates while the game is paused. */
	TArray<FLaneForecast> CachedForecast;
	double LastForecastRefresh = -1.0;

	/** Between waves, shows what the next wave has and puts a marker over each lane's spawn point. */
	void DrawWaveForecast(ATDGameMode* GameMode, AWaveManager* WaveManager);

	/** Graph of the difficulty and score for each wave, with the 65% target line. */
	void DrawDirectorGraph(const UWaveDirector* Director);

	/** Draws the info panel with defenders, cost, seed and controls, plus the pause overlay. */
	void DrawInfoPanel(ATDGameMode* GameMode);

	/** Draws one line of text on the info panel. */
	void DrawPanelText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale);

	/** Draws a small health bar above every defender that is still alive. */
	void DrawDefenderHealthBars();

	/** Draws a small health bar above every enemy that is still alive. */
	void DrawEnemyHealthBars();

	/** Turns a world position into a screen position and draws a health bar there.
	 *  Both the defender and enemy bars use this. */
	void DrawWorldHealthBar(const FVector& WorldLocation, float HealthPercent, float BarWidth, float BarHeight);
};
