// TDEndScreenWidget_Theme.inl — included by the parent .cpp (not compiled alone).

void UTDEndScreenWidget::EnsureThemeArt()
{
	auto AssignTexture = [](UImage* Image, const TArray<const TCHAR*>& Paths)
	{
		if (!Image)
		{
			return;
		}
		for (const TCHAR* Path : Paths)
		{
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path))
			{
				Image->SetBrushFromTexture(Texture, false);
				StretchWidgetToFillParentSlot(Image);
				return;
			}
		}
	};

	AssignTexture(VictoryBackground, {
		TEXT("/Game/UI/SourceArt/Victory-image.Victory-image"),
		TEXT("/Game/UI/SourceArt/VictoryBackground-image.VictoryBackground-image")
	});
	AssignTexture(DefeatBackground, {
		TEXT("/Game/UI/SourceArt/Gameover-image.Gameover-image"),
		TEXT("/Game/UI/SourceArt/Defeatbackground-image.Defeatbackground-image")
	});
}

void UTDEndScreenWidget::ApplyTheme(bool bVictory, EBeamHealthTier /*Tier*/, bool bOfferNextWave)
{
	EnsureThemeArt();

	const bool bUsesConceptArt = VictoryBackground != nullptr || DefeatBackground != nullptr;
	const FLinearColor TitleColor = bVictory
		? FLinearColor(0.92f, 0.78f, 0.28f)
		: FLinearColor(0.86f, 0.42f, 0.95f);
	const FLinearColor SubtitleColor = bVictory
		? FLinearColor(0.55f, 0.9f, 0.45f)
		: FLinearColor(0.82f, 0.68f, 0.92f);
	const FLinearColor PanelColor = bVictory
		? FLinearColor(0.04f, 0.12f, 0.08f, 0.96f)
		: FLinearColor(0.1f, 0.04f, 0.14f, 0.96f);
	const FLinearColor ButtonFill = bUsesConceptArt
		? FLinearColor(0.05f, 0.2f, 0.1f, 0.0f)
		: (bVictory
			? FLinearColor(0.08f, 0.28f, 0.16f, 0.95f)
			: FLinearColor(0.22f, 0.08f, 0.28f, 0.95f));

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(bVictory
			? (bOfferNextWave ? TEXT("WAVE CLEARED") : TEXT("VICTORY!"))
			: TEXT("DEFEAT")));
		TitleText->SetColorAndOpacity(TitleColor);
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(FText::FromString(bVictory
			? (bOfferNextWave ? TEXT("PREPARE FOR THE NEXT ASSAULT") : TEXT("THE FOREST IS SAFE"))
			: TEXT("THE LIGHT HAS FADED")));
		SubtitleText->SetColorAndOpacity(SubtitleColor);
	}
	if (ScoreHeaderText)
	{
		ScoreHeaderText->SetText(FText::FromString(TEXT("â€” SCORE â€”")));
	}
	if (RewardsHeaderText)
	{
		RewardsHeaderText->SetText(FText::FromString(TEXT("â€” REWARDS â€”")));
	}
	if (PanelBorder)
	{
		PanelBorder->SetBrushColor(PanelColor);
	}

	if (bVictory && !VictoryBackground && DefeatBackground)
	{
		if (UTexture2D* VictoryTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/SourceArt/Victory-image.Victory-image")))
		{
			DefeatBackground->SetBrushFromTexture(VictoryTex, false);
			StretchWidgetToFillParentSlot(DefeatBackground);
			DefeatBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	else if (!bVictory && DefeatBackground)
	{
		if (UTexture2D* DefeatTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/SourceArt/Gameover-image.Gameover-image")))
		{
			DefeatBackground->SetBrushFromTexture(DefeatTex, false);
			StretchWidgetToFillParentSlot(DefeatBackground);
		}
	}

	if (VictoryBackground)
	{
		VictoryBackground->SetVisibility(bVictory ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DefeatBackground)
	{
		if (VictoryBackground)
		{
			DefeatBackground->SetVisibility(bVictory ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		else
		{
			DefeatBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	if (PanelBorder)
	{
		PanelBorder->SetVisibility(bUsesConceptArt ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (RetryButton)
	{
		RetryButton->SetBackgroundColor(ButtonFill);
		RetryButton->SetVisibility(ESlateVisibility::Visible);
		if (UTextBlock* RetryLabel = Cast<UTextBlock>(RetryButton->GetContent()))
		{
			RetryLabel->SetText(FText::FromString(
				bOfferNextWave ? TEXT("NEXT WAVE") : (bVictory ? TEXT("RETRY WAVE") : TEXT("RETRY"))));
		}
	}
	if (NextWaveButton)
	{
		NextWaveButton->SetBackgroundColor(ButtonFill);
		NextWaveButton->SetVisibility(bOfferNextWave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bOfferNextWave && RetryButton)
		{
			RetryButton->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (MainMenuButton)
	{
		MainMenuButton->SetBackgroundColor(ButtonFill);
	}
}

void UTDEndScreenWidget::ApplyTierTypography(const FMatchResult& Result)
{
	const int32 ScoreSize = UTDMatchRewards::GetScoreFontSize(Result.BeamTier);
	const int32 RewardSize = UTDMatchRewards::GetRewardFontSize(Result.BeamTier);

	auto SetFontSize = [](UTextBlock* Text, int32 Size)
	{
		if (!Text)
		{
			return;
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	};

	SetFontSize(ScoreValueText, ScoreSize);
	SetFontSize(ForestEssenceText, RewardSize);
	SetFontSize(WoodenMightText, RewardSize);
	SetFontSize(GemStonesText, RewardSize);
	SetFontSize(LightLanternsText, RewardSize);
}
