// TDHUD.cpp — see TDHUD.h for the overview.

#include "TDHUD.h"
#include "TDGameMode.h"
#include "Tower.h"
#include "Defender.h"
#include "Enemy.h"
#include "WaveManager.h"
#include "HealthComponent.h"
#include "ProceduralTerrain.h"
#include "TDPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	static constexpr float InfoPanelX = 20.0f;
	static constexpr float InfoPanelY = 200.0f;
	static constexpr float InfoPanelPadding = 12.0f;
	static constexpr float InfoLineSpacing = 36.0f;
	static constexpr float InfoPrimaryScale = 1.9f;
	static constexpr float InfoSecondaryScale = 1.65f;
}

void ATDHUD::DrawHUD()
{
	Super::DrawHUD();

	// The HUD is a pure view: everything it shows comes from the game mode.
	ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	DrawInfoPanel(GameMode);
	DrawDefenderHealthBars();
	DrawEnemyHealthBars();
}

void ATDHUD::DrawPanelText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
{
	DrawText(Text, Color, X, Y, Font, Scale);
}

void ATDHUD::DrawInfoPanel(ATDGameMode* GameMode)
{
	if (!GameMode || !Canvas)
	{
		return;
	}

	UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetLargeFont() : nullptr);

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

	int32 DefenderCost = 50;
	int32 DefenderUpkeep = 8;
	int32 LivingDefenders = 0;
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayerController()))
	{
		if (PC->DefenderClass)
		{
			DefenderCost = PC->DefenderClass.GetDefaultObject()->Cost;
			DefenderUpkeep = PC->DefenderClass.GetDefaultObject()->UpkeepPerWave;
		}
	}
	for (TActorIterator<ADefender> It(GetWorld()); It; ++It)
	{
		ADefender* Defender = *It;
		if (Defender && Defender->HealthComponent && !Defender->HealthComponent->IsDead())
		{
			++LivingDefenders;
		}
	}

	const FString CostText = FString::Printf(TEXT("Defender Cost: %d  |  Upkeep: %d / wave each"), DefenderCost, DefenderUpkeep);
	const FString UpkeepText = FString::Printf(TEXT("Fielded upkeep this wave: %d"), LivingDefenders * DefenderUpkeep);
	const FString SeedText = GameMode->GetTerrain()
		? FString::Printf(TEXT("Map Seed: %d  |  Grid: %d  |  Lanes: %d"),
			GameMode->GetTerrain()->Seed,
			GameMode->GetTerrain()->GridSize,
			GameMode->GetTerrain()->GetTotalLaneCount())
		: FString();
	const FString HintText = TEXT("P: Pause  |  R: Restart  |  Click pad: Place defender");

	const TArray<FString> Lines = SeedText.IsEmpty()
		? TArray<FString>{ DefenderText, CostText, UpkeepText, HintText }
		: TArray<FString>{ DefenderText, CostText, UpkeepText, SeedText, HintText };

	const TArray<float> Scales = SeedText.IsEmpty()
		? TArray<float>{ InfoPrimaryScale, InfoSecondaryScale, InfoSecondaryScale, InfoSecondaryScale }
		: TArray<float>{ InfoPrimaryScale, InfoSecondaryScale, InfoSecondaryScale, InfoSecondaryScale, InfoSecondaryScale };

	float PanelWidth = 0.0f;
	float PanelHeight = InfoPanelPadding * 2.0f;
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		float LineWidth = 0.0f;
		float LineHeight = 0.0f;
		GetTextSize(Lines[I], LineWidth, LineHeight, Font, Scales[I]);
		PanelWidth = FMath::Max(PanelWidth, LineWidth);
		PanelHeight += LineHeight + (I + 1 < Lines.Num() ? InfoLineSpacing - LineHeight : 0.0f);
	}

	DrawRect(
		FLinearColor(0.02f, 0.02f, 0.05f, 0.72f),
		InfoPanelX,
		InfoPanelY,
		PanelWidth + InfoPanelPadding * 2.0f,
		PanelHeight);

	float TextY = InfoPanelY + InfoPanelPadding;
	const float TextX = InfoPanelX + InfoPanelPadding;

	DrawPanelText(DefenderText, FLinearColor(0.82f, 0.62f, 1.0f), TextX, TextY, Font, InfoPrimaryScale);
	TextY += InfoLineSpacing;

	DrawPanelText(CostText, FLinearColor::White, TextX, TextY, Font, InfoSecondaryScale);
	TextY += InfoLineSpacing;

	DrawPanelText(UpkeepText, FLinearColor(0.95f, 0.8f, 0.55f), TextX, TextY, Font, InfoSecondaryScale);
	TextY += InfoLineSpacing;

	if (!SeedText.IsEmpty())
	{
		DrawPanelText(SeedText, FLinearColor(0.92f, 0.92f, 0.92f), TextX, TextY, Font, InfoSecondaryScale);
		TextY += InfoLineSpacing;
	}

	DrawPanelText(HintText, FLinearColor(1.0f, 0.95f, 0.55f), TextX, TextY, Font, InfoSecondaryScale);

	if (GameMode->IsPaused())
	{
		const float CenterX = Canvas->SizeX * 0.5f;
		const float CenterY = Canvas->SizeY * 0.5f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
		DrawText(TEXT("PAUSED"), FLinearColor(1.f, 0.95f, 0.3f), CenterX - 90.0f, CenterY - 24.0f, Font, 2.8f);
	}
}

void ATDHUD::DrawEnemyHealthBars()
{
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		AEnemy* Enemy = *It;
		UHealthComponent* Health = Enemy ? Enemy->HealthComponent : nullptr;
		if (!Health || Health->IsDead())
		{
			continue;
		}

		const FVector BarLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
		DrawWorldHealthBar(BarLocation, Health->GetHealthPercent(), 55.0f, 6.0f);
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
