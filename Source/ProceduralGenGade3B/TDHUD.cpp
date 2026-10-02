// TDHUD.cpp. See TDHUD.h for an overview.

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

	// The HUD only draws things. All the values come from the game mode.
	ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	// Don't draw the match HUD on the start menu level.
	const FString LevelName = GetWorld()->GetMapName();
	if (LevelName.Contains(TEXT("StartScreen")))
	{
		return;
	}

	DrawInfoPanel(GameMode);
	DrawDefenderHealthBars();
	DrawEnemyHealthBars();

	AWaveManager* WaveManager = GameMode->GetWaveManager();
	if (WaveManager && WaveManager->bUseAdaptiveDirector && WaveManager->Director)
	{
		DrawWaveForecast(GameMode, WaveManager);

		if (bShowDirectorGraph || GameMode->IsGameOver() || GameMode->IsVictory())
		{
			DrawDirectorGraph(WaveManager->Director);
		}
	}
}

void ATDHUD::DrawWaveForecast(ATDGameMode* GameMode, AWaveManager* WaveManager)
{
	const EWaveState State = WaveManager->GetWaveState();
	const bool bShow = State == EWaveState::CountingDown
		|| (State == EWaveState::Complete && WaveManager->HasUpcomingWavePlan());
	if (!bShow || !Canvas)
	{
		CachedForecast.Reset();
		LastForecastRefresh = -1.0;
		return;
	}

	const UWaveDirector* Director = WaveManager->Director;
	const FWavePlan& Plan = Director->GetCurrentPlan();

	const double Now = FPlatformTime::Seconds();
	if (LastForecastRefresh < 0.0 || Now - LastForecastRefresh > 0.5)
	{
		CachedForecast = Director->ForecastLanes(GetWorld());
		LastForecastRefresh = Now;
	}

	UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetLargeFont() : nullptr);
	const FLinearColor Heading(1.0f, 0.85f, 0.3f);
	const FLinearColor Body(0.92f, 0.92f, 0.92f);
	const FLinearColor Danger(1.0f, 0.5f, 0.35f);

	TArray<TPair<FString, FLinearColor>> Lines;
	Lines.Add({ FString::Printf(TEXT("NEXT WAVE %d FORECAST"), Plan.WaveNumber), Heading });
	Lines.Add({ FString::Printf(TEXT("%d Basic   %d Wolf   %d Bear   (threat %.0f, HP x%.2f)"),
		Plan.BasicCount, Plan.WolfCount, Plan.BearCount, Plan.ThreatBudget, Plan.HealthMultiplier), Body });
	if (Plan.EliteCount > 0)
	{
		Lines.Add({ FString::Printf(TEXT("%d Elite  -  countering you with: %s"),
			Plan.EliteCount, *AEnemy::GetEliteName(Plan.CounterElite)), Danger });
	}

	if (const AProceduralTerrain* Terrain = GameMode->GetTerrain())
	{
		const TArray<FEnemyPath>& Paths = Terrain->GetEnemyPaths();
		for (const FLaneForecast& Lane : CachedForecast)
		{
			if (Lane.Total() == 0)
			{
				continue;
			}

			FString Text = FString::Printf(TEXT("Lane %d:  "), Lane.Lane + 1);
			if (Lane.BasicCount > 0) { Text += FString::Printf(TEXT("%d Basic  "), Lane.BasicCount); }
			if (Lane.WolfCount > 0)  { Text += FString::Printf(TEXT("%d Wolf  "), Lane.WolfCount); }
			if (Lane.BearCount > 0)  { Text += FString::Printf(TEXT("%d Bear  "), Lane.BearCount); }
			if (Lane.EliteCount > 0) { Text += FString::Printf(TEXT("(%d elite)"), Lane.EliteCount); }
			const bool bHeavy = Lane.BearCount > 0 || Lane.EliteCount > 0;
			Lines.Add({ Text, bHeavy ? Danger : Body });

			// Put a marker over the lane's spawn point so the player can see where enemies come from.
			if (Paths.IsValidIndex(Lane.Lane) && PlayerOwner)
			{
				FVector2D ScreenPos;
				if (PlayerOwner->ProjectWorldLocationToScreen(Paths[Lane.Lane].SpawnPoint + FVector(0.0f, 0.0f, 180.0f), ScreenPos))
				{
					const FString Marker = FString::Printf(TEXT("L%d: %d%s"), Lane.Lane + 1, Lane.Total(),
						Lane.EliteCount > 0 ? TEXT(" + ELITE") : TEXT(""));
					float W = 0.0f, H = 0.0f;
					GetTextSize(Marker, W, H, Font, 1.4f);
					DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), ScreenPos.X - W * 0.5f - 8.0f, ScreenPos.Y - 4.0f, W + 16.0f, H + 8.0f);
					DrawText(Marker, bHeavy ? Danger : Heading, ScreenPos.X - W * 0.5f, ScreenPos.Y, Font, 1.4f);
				}
			}
		}
	}
	Lines.Add({ TEXT("Lanes update live as you place defenders"), FLinearColor(0.6f, 0.75f, 0.9f) });

	const float Scale = 1.5f;
	const float LineSpacing = 32.0f;
	float PanelWidth = 0.0f;
	for (const TPair<FString, FLinearColor>& Line : Lines)
	{
		float W = 0.0f, H = 0.0f;
		GetTextSize(Line.Key, W, H, Font, Scale);
		PanelWidth = FMath::Max(PanelWidth, W);
	}

	const float Padding = 12.0f;
	const float PanelX = Canvas->SizeX - PanelWidth - Padding * 2.0f - 20.0f;
	const float PanelY = 200.0f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.05f, 0.78f), PanelX, PanelY, PanelWidth + Padding * 2.0f, Lines.Num() * LineSpacing + Padding * 2.0f);

	float TextY = PanelY + Padding;
	for (const TPair<FString, FLinearColor>& Line : Lines)
	{
		DrawText(Line.Key, Line.Value, PanelX + Padding, TextY, Font, Scale);
		TextY += LineSpacing;
	}
}

void ATDHUD::DrawDirectorGraph(const UWaveDirector* Director)
{
	if (!Canvas || !Director)
	{
		return;
	}

	UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetLargeFont() : nullptr);
	const TArray<FWavePerformance>& History = Director->GetHistory();

	const float Width = 560.0f;
	const float Height = 320.0f;
	const float X = Canvas->SizeX - Width - 20.0f;
	const float Y = Canvas->SizeY - Height - 40.0f;
	const float Left = X + 60.0f;
	const float Right = X + Width - 60.0f;
	const float Top = Y + 50.0f;
	const float Bottom = Y + Height - 45.0f;

	DrawRect(FLinearColor(0.02f, 0.02f, 0.05f, 0.85f), X, Y, Width, Height);
	DrawText(TEXT("WAVE DIRECTOR  (G to toggle)"), FLinearColor(1.0f, 0.85f, 0.3f), X + 12.0f, Y + 10.0f, Font, 1.3f);

	const int32 LastWave = History.Num() > 0 ? History.Last().WaveNumber : 1;
	const int32 MaxWave = FMath::Max(LastWave + 1, 2);
	auto Map = [](float Value, float InMin, float InMax, float OutMin, float OutMax)
	{
		return FMath::Lerp(OutMin, OutMax, FMath::Clamp((Value - InMin) / (InMax - InMin), 0.0f, 1.0f));
	};
	auto WaveToX = [&](float Wave) { return Map(Wave, 1.0f, static_cast<float>(MaxWave), Left, Right); };
	// Difficulty goes on the left axis from 0.6 to 1.7. Score goes on the right axis from 0 to 100%.
	auto DifficultyToY = [&](float D) { return Map(D, 0.6f, 1.7f, Bottom, Top); };
	auto ScoreToY = [&](float S) { return Map(S, 0.0f, 1.0f, Bottom, Top); };

	const FLinearColor Axis(0.6f, 0.6f, 0.65f);
	DrawLine(Left, Top, Left, Bottom, Axis, 2.0f);
	DrawLine(Left, Bottom, Right, Bottom, Axis, 2.0f);
	DrawLine(Right, Top, Right, Bottom, Axis, 2.0f);

	// The score the director is trying to hit.
	const float TargetY = ScoreToY(Director->TargetPerformance);
	DrawLine(Left, TargetY, Right, TargetY, FLinearColor(0.3f, 0.8f, 0.4f, 0.8f), 1.5f);
	DrawText(TEXT("target"), FLinearColor(0.3f, 0.8f, 0.4f), Right - 60.0f, TargetY - 22.0f, Font, 0.9f);

	for (int32 Wave = 1; Wave <= MaxWave; ++Wave)
	{
		DrawText(FString::FromInt(Wave), Axis, WaveToX(Wave) - 5.0f, Bottom + 6.0f, Font, 1.0f);
	}
	DrawText(TEXT("1.7"), FLinearColor(0.4f, 0.75f, 1.0f), X + 18.0f, Top - 8.0f, Font, 0.9f);
	DrawText(TEXT("0.6"), FLinearColor(0.4f, 0.75f, 1.0f), X + 18.0f, Bottom - 12.0f, Font, 0.9f);
	DrawText(TEXT("100%"), FLinearColor(1.0f, 0.6f, 0.3f), Right + 6.0f, Top - 8.0f, Font, 0.9f);
	DrawText(TEXT("0%"), FLinearColor(1.0f, 0.6f, 0.3f), Right + 6.0f, Bottom - 12.0f, Font, 0.9f);

	if (History.Num() == 0)
	{
		DrawText(TEXT("Clear a wave to start the graph"), Axis, Left + 20.0f, (Top + Bottom) * 0.5f, Font, 1.1f);
		return;
	}

	// Plot the difficulty used for each wave, then the difficulty picked for the next one.
	const FLinearColor DifficultyColor(0.4f, 0.75f, 1.0f);
	const FLinearColor ScoreColor(1.0f, 0.6f, 0.3f);
	FVector2D PrevDifficulty(-1.0f, -1.0f);
	FVector2D PrevScore(-1.0f, -1.0f);
	for (const FWavePerformance& Entry : History)
	{
		const FVector2D DPoint(WaveToX(Entry.WaveNumber), DifficultyToY(Entry.DifficultyBefore));
		const FVector2D SPoint(WaveToX(Entry.WaveNumber), ScoreToY(Entry.Score));
		if (PrevDifficulty.X >= 0.0f)
		{
			DrawLine(PrevDifficulty.X, PrevDifficulty.Y, DPoint.X, DPoint.Y, DifficultyColor, 3.0f);
			DrawLine(PrevScore.X, PrevScore.Y, SPoint.X, SPoint.Y, ScoreColor, 3.0f);
		}
		DrawRect(DifficultyColor, DPoint.X - 5.0f, DPoint.Y - 5.0f, 10.0f, 10.0f);
		DrawRect(ScoreColor, SPoint.X - 5.0f, SPoint.Y - 5.0f, 10.0f, 10.0f);
		DrawText(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Entry.Score * 100.0f)), ScoreColor, SPoint.X + 6.0f, SPoint.Y - 22.0f, Font, 0.9f);
		PrevDifficulty = DPoint;
		PrevScore = SPoint;
	}

	const FWavePerformance& Last = History.Last();
	const FVector2D NextPoint(WaveToX(Last.WaveNumber + 1), DifficultyToY(Last.DifficultyAfter));
	DrawLine(PrevDifficulty.X, PrevDifficulty.Y, NextPoint.X, NextPoint.Y, DifficultyColor * 0.6f, 2.0f);
	DrawRect(DifficultyColor, NextPoint.X - 5.0f, NextPoint.Y - 5.0f, 10.0f, 10.0f);
	DrawText(FString::Printf(TEXT("x%.2f"), Last.DifficultyAfter), DifficultyColor, NextPoint.X - 20.0f, NextPoint.Y - 26.0f, Font, 0.9f);

	DrawText(TEXT("Difficulty"), DifficultyColor, X + 12.0f, Y + Height - 30.0f, Font, 1.0f);
	DrawText(TEXT("Wave score"), ScoreColor, X + 140.0f, Y + Height - 30.0f, Font, 1.0f);
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

	FString SelectedText = TEXT("Selected: none - press 1-4");
	int32 DefenderUpkeep = 8;
	int32 LivingDefenders = 0;
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayerController()))
	{
		if (const TSubclassOf<ADefender> ActiveClass = PC->GetActiveDefenderClass())
		{
			const ADefender* Defaults = ActiveClass.GetDefaultObject();
			DefenderUpkeep = Defaults->UpkeepPerWave;
			SelectedText = FString::Printf(TEXT("Selected: %s  |  Cost: %d  |  Upkeep: %d / wave"),
				*Defaults->DefenderName, Defaults->Cost, DefenderUpkeep);
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

	TArray<FString> Lines;
	TArray<float> Scales;
	TArray<FLinearColor> Colors;
	auto AddLine = [&](const FString& Text, float Scale, const FLinearColor& Color)
	{
		Lines.Add(Text);
		Scales.Add(Scale);
		Colors.Add(Color);
	};

	AddLine(DefenderText, InfoPrimaryScale, FLinearColor(0.82f, 0.62f, 1.0f));
	AddLine(SelectedText, InfoSecondaryScale, FLinearColor::White);
	AddLine(FString::Printf(TEXT("Fielded upkeep this wave: %d"), LivingDefenders * DefenderUpkeep),
		InfoSecondaryScale, FLinearColor(0.95f, 0.8f, 0.55f));

	if (const AProceduralTerrain* Terrain = GameMode->GetTerrain())
	{
		AddLine(FString::Printf(TEXT("Map Seed: %d  |  Grid: %d  |  Lanes: %d"),
			Terrain->Seed, Terrain->GridSize, Terrain->GetTotalLaneCount()),
			InfoSecondaryScale, FLinearColor(0.92f, 0.92f, 0.92f));
	}

	// Wave director info, so you can see the difficulty change while you play.
	const AWaveManager* WaveManager = GameMode->GetWaveManager();
	const UWaveDirector* Director = WaveManager && WaveManager->bUseAdaptiveDirector ? WaveManager->Director.Get() : nullptr;
	if (Director)
	{
		const FWavePerformance& Last = Director->GetLastPerformance();
		const FString ScoreText = Last.bValid
			? FString::Printf(TEXT("  |  Last wave: %d%%"), FMath::RoundToInt(Last.Score * 100.0f))
			: FString();
		AddLine(FString::Printf(TEXT("Director: difficulty x%.2f (%s)%s"),
			Director->GetDifficulty(), *Director->GetSkillLabel(), *ScoreText),
			InfoSecondaryScale, FLinearColor(0.55f, 0.9f, 1.0f));

		if (Last.bValid)
		{
			const FLinearColor TrendColor = Last.DifficultyAfter > Last.DifficultyBefore + KINDA_SMALL_NUMBER
				? FLinearColor(1.0f, 0.5f, 0.4f)
				: (Last.DifficultyAfter < Last.DifficultyBefore - KINDA_SMALL_NUMBER
					? FLinearColor(0.5f, 1.0f, 0.55f)
					: FLinearColor(0.55f, 0.9f, 1.0f));
			AddLine(FString::Printf(TEXT("Score: tower %d  kills %d  defenders %d  loot %d  ->  x%.2f to x%.2f"),
				FMath::RoundToInt(Last.TowerScore * 100.0f), FMath::RoundToInt(Last.KillScore * 100.0f),
				FMath::RoundToInt(Last.DefenderScore * 100.0f), FMath::RoundToInt(Last.EconomyScore * 100.0f),
				Last.DifficultyBefore, Last.DifficultyAfter),
				InfoSecondaryScale, TrendColor);
		}

		const FPlayerProfile& Profile = Director->GetProfile();
		AddLine(FString::Printf(TEXT("Play style: %s  (%.1f defenders / lane)"),
			*Profile.StyleLabel, Profile.DefendersPerLane),
			InfoSecondaryScale, FLinearColor(0.55f, 0.9f, 1.0f));

		const FWavePlan& Plan = Director->GetCurrentPlan();
		if (Plan.WaveNumber > 0)
		{
			const FString EliteText = Plan.EliteCount > 0 ? FString::Printf(TEXT(", %d Elite"), Plan.EliteCount) : FString();
			AddLine(FString::Printf(TEXT("Wave %d: %d Basic, %d Wolf, %d Bear%s  (threat %.0f)"),
				Plan.WaveNumber, Plan.BasicCount, Plan.WolfCount, Plan.BearCount, *EliteText, Plan.ThreatBudget),
				InfoSecondaryScale, FLinearColor(0.55f, 0.9f, 1.0f));
		}

		if (GameMode->GetMatchCombos() > 0)
		{
			AddLine(FString::Printf(TEXT("Defender combos: %d  (Shatter / Venom spread)"), GameMode->GetMatchCombos()),
				InfoSecondaryScale, FLinearColor(0.75f, 0.6f, 1.0f));
		}

		const float EventAge = GetWorld()->GetTimeSeconds() - WaveManager->GetLastDirectorEventTime();
		if (EventAge < 6.0f && !WaveManager->GetLastDirectorEvent().IsEmpty())
		{
			AddLine(WaveManager->GetLastDirectorEvent(), InfoSecondaryScale, FLinearColor(1.0f, 0.55f, 0.3f));
		}
	}

	AddLine(TEXT("1 Basic  2 Archer  3 Bomb  4 Strong  |  G: Graph  |  P: Pause  |  R: Restart"),
		InfoSecondaryScale, FLinearColor(1.0f, 0.95f, 0.55f));

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
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		DrawPanelText(Lines[I], Colors[I], TextX, TextY, Font, Scales[I]);
		TextY += InfoLineSpacing;
	}

	// Don't draw the paused banner while the settings screen is open.
	if (GameMode->IsPaused() && !GameMode->IsGameOver() && !GameMode->IsVictory() && !GameMode->IsSettingsVisible())
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
		DrawWorldHealthBar(BarLocation, Health->GetHealthPercent(), Enemy->IsElite() ? 75.0f : 55.0f, 6.0f);

		// Show the elite type and any status effects above the bar.
		FString Status;
		if (Enemy->IsElite())
		{
			Status = AEnemy::GetEliteName(Enemy->GetEliteModifier());
			if (Enemy->GetEliteModifier() == EEliteModifier::Shielded && Enemy->GetShieldHitsRemaining() > 0)
			{
				Status += FString::Printf(TEXT(" [%d]"), Enemy->GetShieldHitsRemaining());
			}
		}
		if (Enemy->IsPoisoned())
		{
			Status += Status.IsEmpty() ? TEXT("Poisoned") : TEXT(" | Poisoned");
		}
		if (Enemy->IsStunned())
		{
			Status += Status.IsEmpty() ? TEXT("Stunned") : TEXT(" | Stunned");
		}

		FVector2D ScreenPos;
		if (!Status.IsEmpty() && PlayerOwner && PlayerOwner->ProjectWorldLocationToScreen(BarLocation, ScreenPos))
		{
			UFont* Font = const_cast<UFont*>(GEngine ? GEngine->GetSmallFont() : nullptr);
			float W = 0.0f, H = 0.0f;
			GetTextSize(Status, W, H, Font, 1.0f);
			const FLinearColor Color = Enemy->IsElite() ? FLinearColor(1.0f, 0.8f, 0.2f) : FLinearColor(0.6f, 1.0f, 0.6f);
			DrawText(Status, Color, ScreenPos.X - W * 0.5f, ScreenPos.Y - H - 8.0f, Font, 1.0f);
		}
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
			continue; // Dead defenders don't need a health bar.
		}

		// Put the bar a bit above the defender so it doesn't cover the model.
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

	// Turn the 3D world position into a 2D screen position. If the point is behind
	// the camera this returns false and we skip it.
	FVector2D ScreenPos;
	if (!PlayerOwner->ProjectWorldLocationToScreen(WorldLocation, ScreenPos))
	{
		return;
	}

	const float Left = ScreenPos.X - BarWidth * 0.5f;
	const float Top = ScreenPos.Y - BarHeight * 0.5f;

	// Dark background first so the bar is easy to see on any part of the map.
	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.75f), Left, Top, BarWidth, BarHeight);

	// Then fill it from red to green based on health, the same colours as the tower health
	// so it reads the same everywhere.
	const float Pct = FMath::Clamp(HealthPercent, 0.0f, 1.0f);
	const FLinearColor FillColor = FMath::Lerp(FLinearColor::Red, FLinearColor::Green, Pct);
	DrawRect(FillColor, Left, Top, BarWidth * Pct, BarHeight);
}
