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
