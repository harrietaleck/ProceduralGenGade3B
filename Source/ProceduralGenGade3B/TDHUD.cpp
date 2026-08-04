// TDHUD.cpp — see TDHUD.h for the overview.

#include "TDHUD.h"
#include "TDGameMode.h"
#include "Tower.h"
#include "HealthComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

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
