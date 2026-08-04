// TDHUD.cpp — see TDHUD.h for the overview.

#include "TDHUD.h"
#include "TDGameMode.h"
#include "Tower.h"
#include "Defender.h"
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

	if (GameMode->IsGameOver())
	{
		DrawGameOver();
	}
}

void ATDHUD::DrawStatus(ATDGameMode* GameMode)
{
	// GetLargeFont() returns a const UFont*, but AHUD::DrawText wants a non-const UFont*.
	// DrawText only reads the font, so a const_cast here is safe.
	UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetLargeFont() : nullptr);

	// --- Loot (the single shared currency, earned by defeating enemies) ---
	const FString LootText = FString::Printf(TEXT("Loot: %d"), GameMode->GetResources());
	DrawText(LootText, FLinearColor(0.4f, 0.9f, 1.0f), 40.0f, 40.0f, Font, 1.4f);

	// --- Current wave ---
	const FString WaveText = FString::Printf(TEXT("Wave: %d"), GameMode->GetCurrentWave());
	DrawText(WaveText, FLinearColor::White, 40.0f, 80.0f, Font, 1.4f);

	// --- Citadel health ---
	if (ATower* Citadel = GameMode->GetTower())
	{
		if (UHealthComponent* Health = Citadel->HealthComponent)
		{
			const FString HealthText = FString::Printf(TEXT("Citadel: %d / %d"),
				FMath::RoundToInt(Health->GetCurrentHealth()),
				FMath::RoundToInt(Health->MaxHealth));

			// Green when healthy, red when nearly destroyed.
			const float Pct = Health->GetHealthPercent();
			const FLinearColor HealthColor = FMath::Lerp(FLinearColor::Red, FLinearColor::Green, Pct);
			DrawText(HealthText, HealthColor, 40.0f, 120.0f, Font, 1.4f);
		}
	}

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

		// Same green-to-red health language as the Citadel line above; plain white when none placed yet.
		const FLinearColor DefenderColor = AliveCount > 0
			? FMath::Lerp(FLinearColor::Red, FLinearColor::Green, TotalMax > 0.0f ? TotalCurrent / TotalMax : 1.0f)
			: FLinearColor::White;
		DrawText(DefenderText, DefenderColor, 40.0f, 160.0f, Font, 1.4f);
	}
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
