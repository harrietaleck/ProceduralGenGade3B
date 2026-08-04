// TDHUD.h
// A lightweight, code-only heads-up display. It reads live values from the game mode each
// frame and draws them straight onto the screen with the Canvas: the player's Essence, the
// Citadel's health, the current wave, and — when the Citadel falls — a "GAME OVER / press R
// to restart" banner.
//
// Doing the HUD in C++ (rather than a UMG widget blueprint) keeps Part 1 entirely self-
// contained: there are no widget assets to wire up, it compiles and runs as-is, and it's
// trivial to later replace with a designed UMG screen without touching gameplay code.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TDHUD.generated.h"

class ATDGameMode;

UCLASS()
class PROCEDURALGENGADE3B_API ATDHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** Called every frame by the engine to paint the HUD. */
	virtual void DrawHUD() override;

private:
	/** Draw the top-left status readout (Loot, Citadel health, wave). */
	void DrawStatus(ATDGameMode* GameMode);

	/** Draw a small floating health bar over every living defender, so it's always clear
	 *  how much health each one has left. */
	void DrawDefenderHealthBars();

	/** Projects a world location to screen space and draws a background + health-coloured
	 *  fill bar there. Shared by defender (and future enemy/tower) health bars. */
	void DrawWorldHealthBar(const FVector& WorldLocation, float HealthPercent, float BarWidth, float BarHeight);

	/** Draw the centred "GAME OVER — press R to restart" banner. */
	void DrawGameOver();
};
