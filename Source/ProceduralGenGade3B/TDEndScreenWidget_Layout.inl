// TDEndScreenWidget_Layout.inl — included by the parent .cpp (not compiled alone).

void UTDEndScreenWidget::EnsureBlueprintLayoutFitsScreen()
{
	if (UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get())
	{
		if (ViewportSubsystem->IsWidgetAdded(this))
		{
			FGameViewportWidgetSlot ViewportSlot = ViewportSubsystem->GetWidgetSlot(this);
			ViewportSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			ViewportSlot.Offsets = FMargin(0.0f);
			ViewportSlot.Alignment = FVector2D(0.0f, 0.0f);
			ViewportSubsystem->SetWidgetSlot(this, ViewportSlot);
		}
	}
	else
	{
		SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		SetPositionInViewport(FVector2D::ZeroVector, false);
		SetAlignmentInViewport(FVector2D::ZeroVector);
	}

	if (!DefeatBackground)
	{
		DefeatBackground = Cast<UImage>(GetWidgetFromName(TEXT("DefeatBackground")));
	}
	if (!VictoryBackground)
	{
		VictoryBackground = Cast<UImage>(GetWidgetFromName(TEXT("VictoryBackground")));
	}

	UWidget* Root = GetRootWidget();
	if (USizeBox* RootSize = Cast<USizeBox>(Root))
	{
		RootSize->ClearWidthOverride();
		RootSize->ClearHeightOverride();
	}
	if (UScaleBox* RootScale = Cast<UScaleBox>(Root))
	{
		RootScale->SetStretch(EStretch::ScaleToFill);
		RootScale->SetStretchDirection(EStretchDirection::Both);
	}

	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Root))
	{
		for (UPanelSlot* ChildSlot : Canvas->GetSlots())
		{
			UWidget* Child = ChildSlot ? ChildSlot->Content : nullptr;
			if (Cast<UImage>(Child) || Cast<UBorder>(Child) || Cast<UScaleBox>(Child) || Cast<USizeBox>(Child))
			{
				StretchWidgetToFillParentSlot(Child);
			}
		}
	}

	if (ScreenScaleBox)
	{
		ScreenScaleBox->SetStretch(EStretch::ScaleToFill);
		ScreenScaleBox->SetStretchDirection(EStretchDirection::Both);
		StretchWidgetToFillParentSlot(ScreenScaleBox);
	}

	StretchWidgetToFillParentSlot(DimOverlay);
	StretchWidgetToFillParentSlot(DefeatBackground);
	StretchWidgetToFillParentSlot(VictoryBackground);

	auto StretchImageBrush = [](UImage* Image)
	{
		if (!Image)
		{
			return;
		}
		FSlateBrush Brush = Image->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.ImageSize = FVector2D(1920.0f, 1080.0f);
		Image->SetBrush(Brush);
		Image->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	};
	StretchImageBrush(DefeatBackground);
	StretchImageBrush(VictoryBackground);
}

void UTDEndScreenWidget::ResolveOptionalWidgetBindings()
{
	if (bBuiltFallbackLayout)
	{
		return;
	}

	auto ResolveText = [this](TObjectPtr<UTextBlock>& Member, const TCHAR* Primary, const TCHAR* Alt = nullptr)
	{
		if (Member)
		{
			return;
		}
		if (UWidget* Found = GetWidgetFromName(Primary))
		{
			Member = Cast<UTextBlock>(Found);
		}
		if (!Member && Alt)
		{
			Member = Cast<UTextBlock>(GetWidgetFromName(Alt));
		}
	};
	auto ResolveButton = [this](TObjectPtr<UButton>& Member, const TCHAR* Primary, const TCHAR* Alt = nullptr)
	{
		if (Member)
		{
			return;
		}
		if (UWidget* Found = GetWidgetFromName(Primary))
		{
			Member = Cast<UButton>(Found);
		}
		if (!Member && Alt)
		{
			Member = Cast<UButton>(GetWidgetFromName(Alt));
		}
	};

	ResolveText(ScoreValueText, TEXT("ScoreValueText"), TEXT("Score"));
	ResolveText(ForestEssenceText, TEXT("ForestEssenceText"), TEXT("forestScore"));
	if (!ForestEssenceText)
	{
		ForestEssenceText = Cast<UTextBlock>(GetWidgetFromName(TEXT("EssenceText")));
	}
	ResolveText(WoodenMightText, TEXT("WoodenMightText"), TEXT("WoodScore"));
	if (!WoodenMightText)
	{
		WoodenMightText = Cast<UTextBlock>(GetWidgetFromName(TEXT("WoodText")));
	}
	ResolveText(GemStonesText, TEXT("GemStonesText"), TEXT("GemScore"));
	if (!GemStonesText)
	{
		GemStonesText = Cast<UTextBlock>(GetWidgetFromName(TEXT("GemsText")));
	}
	ResolveText(LightLanternsText, TEXT("LightLanternsText"), TEXT("LightScore"));
	if (!LightLanternsText)
	{
		LightLanternsText = Cast<UTextBlock>(GetWidgetFromName(TEXT("LanternsText")));
	}
	ResolveText(BeamHealthText, TEXT("BeamHealthText"));
	ResolveText(TierText, TEXT("TierText"));
	ResolveText(WavesText, TEXT("WavesText"));
	ResolveButton(RetryButton, TEXT("RetryButton"), TEXT("RetryBTN"));
	ResolveButton(NextWaveButton, TEXT("NextWaveButton"), TEXT("NextWaveBTN"));
	ResolveButton(MainMenuButton, TEXT("MainMenuButton"), TEXT("MainMenuBTN"));
}

void UTDEndScreenWidget::BindActionButtons()
{
	auto PrepareButton = [](UButton* Button)
	{
		if (!Button)
		{
			return;
		}
		Button->SetIsEnabled(true);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Button->Slot))
		{
			CanvasSlot->SetZOrder(200);
		}
	};

	if (RetryButton)
	{
		RetryButton->OnClicked.Clear();
		RetryButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnRetryClicked);
		PrepareButton(RetryButton);
	}
	if (NextWaveButton)
	{
		NextWaveButton->OnClicked.Clear();
		NextWaveButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnNextWaveClicked);
		PrepareButton(NextWaveButton);
	}
	if (MainMenuButton)
	{
		MainMenuButton->OnClicked.Clear();
		MainMenuButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnMainMenuClicked);
		PrepareButton(MainMenuButton);
	}
}

void UTDEndScreenWidget::EnsureResultTextWidgets()
{
	UCanvasPanel* Canvas = RootCanvas;
	if (!Canvas)
	{
		Canvas = Cast<UCanvasPanel>(GetRootWidget());
		RootCanvas = Canvas;
	}
	if (!Canvas && DefeatBackground)
	{
		Canvas = Cast<UCanvasPanel>(DefeatBackground->GetParent());
		RootCanvas = Canvas;
	}
	if (!Canvas || !WidgetTree)
	{
		return;
	}

	static const TCHAR* HideNames[] = {
		TEXT("ForestEssenceText"), TEXT("ForestEssenceText_1"),
		TEXT("WoodenMightText"), TEXT("WoodenMightText_1"),
		TEXT("GemStonesText"), TEXT("GemStonesText_1"),
		TEXT("LightLanternsText"), TEXT("LightLanternsText_1"),
		TEXT("ResultOverlay_Score"), TEXT("ResultOverlay_Essence"),
		TEXT("ResultOverlay_Wood"), TEXT("ResultOverlay_Gems"), TEXT("ResultOverlay_Lanterns")
	};
	for (const TCHAR* Name : HideNames)
	{
		if (UWidget* W = GetWidgetFromName(Name))
		{
			W->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	ForestEssenceText = nullptr;
	WoodenMightText = nullptr;
	GemStonesText = nullptr;
	LightLanternsText = nullptr;
	EnsureRewardNumbersUnderIcons(this, WidgetTree, Canvas, ForestEssenceText, WoodenMightText, GemStonesText, LightLanternsText);
}

void UTDEndScreenWidget::LayoutDefeatResultWidgets()
{
	CenterScoreAcrossPanel(ScoreValueText);
	if (UWidget* OldRow = GetWidgetFromName(TEXT("ResultRewardRow")))
	{
		OldRow->SetVisibility(ESlateVisibility::Collapsed);
	}

	auto PinUnderIcon = [](UTextBlock* Text, float AnchorX, float AnchorY)
	{
		if (!Text)
		{
			return;
		}
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Text->Slot))
		{
			Slot->SetAnchors(FAnchors(AnchorX, AnchorY));
			Slot->SetAlignment(FVector2D(0.5f, 0.0f));
			Slot->SetAutoSize(true);
			Slot->SetZOrder(50);
			Slot->SetOffsets(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
		}
	};
	PinUnderIcon(ForestEssenceText, 0.376f, 0.625f);
	PinUnderIcon(WoodenMightText, 0.459f, 0.625f);
	PinUnderIcon(GemStonesText, 0.542f, 0.625f);
	PinUnderIcon(LightLanternsText, 0.615f, 0.625f);
}

void UTDEndScreenWidget::ApplyMatchResultToWidgets(const FMatchResult& Result)
{
	FMetaCurrencyRewards Shown = Result.Rewards;
	const bool bRewardsEmpty = Shown.ForestEssence == 0
		&& Shown.WoodenMight == 0
		&& Shown.GemStones == 0
		&& Shown.LightLanterns == 0;
	if (bRewardsEmpty)
	{
		Shown = Result.Wallet;
	}

	auto SetCenteredNumber = [this](UTextBlock* Text, int32 Value)
	{
		if (!Text)
		{
			return;
		}
		Text->SetText(FText::FromString(FString::FromInt(Value)));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		Text->SetJustification(ETextJustify::Center);
		if (Text == ScoreValueText)
		{
			CenterScoreAcrossPanel(Text);
		}
	};

	SetCenteredNumber(ScoreValueText, Result.Score);
	SetCenteredNumber(ForestEssenceText, Shown.ForestEssence);
	SetCenteredNumber(WoodenMightText, Shown.WoodenMight);
	SetCenteredNumber(GemStonesText, Shown.GemStones);
	SetCenteredNumber(LightLanternsText, Shown.LightLanterns);

	if (BeamHealthText)
	{
		BeamHealthText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TierText)
	{
		TierText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (WavesText)
	{
		WavesText->SetVisibility(ESlateVisibility::Collapsed);
	}
}
