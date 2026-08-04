// TDHUD.cpp — see TDHUD.h for the overview.

#include "TDHUD.h"
#include "TDGameMode.h"
#include "Tower.h"
#include "Defender.h"
#include "WaveManager.h"
#include "HealthComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

void ATDHUD::DrawHUD()
{
	Super::DrawHUD();

	// The HUD is a pure view: everything it shows comes from the game mode.
	ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	DrawStatus(GameMode);
	DrawDefenderHealthBars();
	DrawInsufficientFundsMessage();

	if (GameMode->IsGameOver())
	{
		DrawGameOver();
	}
	else if (GameMode->IsVictory())
	{
		DrawVictory();
	}
}

void ATDHUD::DrawStatus(ATDGameMode* GameMode)
{
	// GetLargeFont() returns a const UFont*, but AHUD::DrawText wants a non-const UFont*.
	// DrawText only reads the font, so a const_cast here is safe.
	UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetLargeFont() : nullptr);

	// Loot, Wave status, and Citadel health now live in the real UMG HUD (UTDHUDWidget /
	// WBP_TDHUD) — see TDHUDWidget.cpp. Kept here in Canvas: the defender summary below (no
	// UMG equivalent yet), the insufficient-funds banner, and the Game Over / Victory screens.

	// --- Defender summary: how many are alive and their combined health, at a glance ---
	{
		int32 AliveCount = 0;
		float TotalCurrent = 0.0f;
		float TotalMax = 0.0f;
		for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
		{
			ADefender* Defender = *It;
			UHealthComponent* Health = Defender ? Defender->HealthComponent : nullptr;
			if (!Health || Health->IsDead())
			{
				continue;
			}
			++AliveCount;
			TotalCurrent += Health->GetCurrentHealth();
			TotalMax += Health->MaxHealth;
		}

		const FString DefenderText = AliveCount > 0
			? FString::Printf(TEXT("Defenders: %d (%d / %d HP)"), AliveCount, FMath::RoundToInt(TotalCurrent), FMath::RoundToInt(TotalMax))
			: TEXT("Defenders: 0");

		// Deep purple identifies this as the "defenders" line at a glance, distinct from the
		// Citadel's red/green health colouring above it. Kept dark/saturated (rather than a
		// pale lavender) so it stays readable against light terrain in the background.
		// Positioned well below the UMG HUD's top-left "Wave X / Y" text so the two never overlap.
		const FLinearColor DefenderColor(0.35f, 0.0f, 0.55f);
		DrawText(DefenderText, DefenderColor, 40.0f, 220.0f, Font, 1.4f);
	}
}

void ATDHUD::ShowInsufficientFundsMessage()
{
	if (UWorld* World = GetWorld())
	{
		InsufficientFundsMessageExpireTime = World->GetTimeSeconds() + InsufficientFundsMessageDuration;
	}
}

bool ATDHUD::IsShowingInsufficientFundsMessage() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < InsufficientFundsMessageExpireTime;
}

void ATDHUD::DrawInsufficientFundsMessage()
{
	if (!IsShowingInsufficientFundsMessage())
	{
		return; // Never triggered, or it already expired -> nothing to draw, no timer to clean up.
	}

	UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetLargeFont() : nullptr);
	const float CenterX = Canvas ? Canvas->SizeX * 0.5f : 400.0f;
	DrawText(TEXT("Not Enough Loot"), FLinearColor::Red, CenterX - 150.0f, 40.0f, Font, 1.6f);
}

void ATDHUD::DrawDefenderHealthBars()
{
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		ADefender* Defender = *It;
		UHealthComponent* Health = Defender ? Defender->HealthComponent : nullptr;
		if (!Health || Health->IsDead())
		{
			continue; // Destroyed/dying defenders don't need a bar drawn over them.
		}

		// Float the bar a little above the defender's mesh so it doesn't overlap the model.
		const FVector BarLocation = Defender->GetActorLocation() + FVector(0.0f, 0.0f, 140.0f);
		DrawWorldHealthBar(BarLocation, Health->GetHealthPercent(), 70.0f, 8.0f);
	}
}

void ATDHUD::DrawWorldHealthBar(const FVector& WorldLocation, float HealthPercent, float BarWidth, float BarHeight)
{
	if (!PlayerOwner)
	{
		return;
	}

	// Project the 3D world position to a 2D screen position. Returns false if the point is
	// behind the camera, in which case there's nothing sensible to draw.
	FVector2D ScreenPos;
	if (!PlayerOwner->ProjectWorldLocationToScreen(WorldLocation, ScreenPos))
	{
		return;
	}

	const float Left = ScreenPos.X - BarWidth * 0.5f;
	const float Top = ScreenPos.Y - BarHeight * 0.5f;

	// Dark background so the bar reads clearly against any part of the battlefield...
	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.75f), Left, Top, BarWidth, BarHeight);

	// ...then a green-to-red fill scaled by remaining health, same colour language as the
	// Citadel readout so health always means the same thing everywhere on screen.
	const float Pct = FMath::Clamp(HealthPercent, 0.0f, 1.0f);
	const FLinearColor FillColor = FMath::Lerp(FLinearColor::Red, FLinearColor::Green, Pct);
	DrawRect(FillColor, Left, Top, BarWidth * Pct, BarHeight);
}

void ATDHUD::DrawGameOver()
{
	// GetLargeFont() returns a const UFont*, but AHUD::DrawText wants a non-const UFont*.
	// DrawText only reads the font, so a const_cast here is safe.
	UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetLargeFont() : nullptr);

	// Centre the banner roughly on screen (Canvas gives us the viewport size).
	const float CenterX = Canvas ? Canvas->SizeX * 0.5f : 400.0f;
	const float CenterY = Canvas ? Canvas->SizeY * 0.5f : 300.0f;

	const FString OverText = TEXT("GAME OVER");
	const FString HintText = TEXT("Press R to restart");

	// DrawText positions from the top-left of the string, so nudge left to look centred.
	DrawText(OverText, FLinearColor::Red, CenterX - 120.0f, CenterY - 40.0f, Font, 2.5f);
	DrawText(HintText, FLinearColor::White, CenterX - 110.0f, CenterY + 20.0f, Font, 1.4f);
}

void ATDHUD::DrawVictory()
{
	UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetLargeFont() : nullptr);

	const float CenterX = Canvas ? Canvas->SizeX * 0.5f : 400.0f;
	const float CenterY = Canvas ? Canvas->SizeY * 0.5f : 300.0f;

	const FString VictoryText = TEXT("VICTORY");
	const FString HintText = TEXT("Press R to restart");

	DrawText(VictoryText, FLinearColor::Green, CenterX - 100.0f, CenterY - 40.0f, Font, 2.5f);
	DrawText(HintText, FLinearColor::White, CenterX - 110.0f, CenterY + 20.0f, Font, 1.4f);
}
