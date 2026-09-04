// TDGameMode_UI.inl — included by the parent .cpp (not compiled alone).

void ATDGameMode::BindSettingsButtons()
{
	TArray<UUserWidget*> SettingsScreens;
	if (SettingsWidget)
	{
		SettingsScreens.Add(SettingsWidget);
	}

	TArray<UUserWidget*> OpenWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this, OpenWidgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* Widget : OpenWidgets)
	{
		if (Widget && Widget->GetClass()->GetName().Contains(TEXT("SettingScreen")))
		{
			SettingsScreens.AddUnique(Widget);
		}
	}

	for (UUserWidget* Screen : SettingsScreens)
	{
		if (UButton* ResumeBtn = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ResumeBTN"))))
		{
			ResumeBtn->OnClicked.RemoveAll(this);
			ResumeBtn->OnClicked.AddDynamic(this, &ATDGameMode::ResumeFromSettings);
		}
		if (UButton* ExitResumeBtn = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ExitResumeBTN"))))
		{
			ExitResumeBtn->OnClicked.RemoveAll(this);
			ExitResumeBtn->OnClicked.AddDynamic(this, &ATDGameMode::ResumeFromSettings);
		}
		if (UButton* RestartBtn = Cast<UButton>(Screen->GetWidgetFromName(TEXT("RestartBTN"))))
		{
			RestartBtn->OnClicked.RemoveAll(this);
			RestartBtn->OnClicked.AddDynamic(this, &ATDGameMode::RestartGame);
		}
		if (UButton* MenuBtn = Cast<UButton>(Screen->GetWidgetFromName(TEXT("StartScreenMenuBTN"))))
		{
			MenuBtn->OnClicked.RemoveAll(this);
			MenuBtn->OnClicked.AddDynamic(this, &ATDGameMode::ReturnToMainMenu);
		}
	}
}

void ATDGameMode::BindVictoryScreenButtons()
{
	if (!VictoryScreenWidget)
	{
		return;
	}

	auto PrepareButton = [](UButton* Button)
	{
		if (!Button)
		{
			return;
		}
		Button->SetIsEnabled(true);
		Button->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Button->Slot))
		{
			const FVector2D Size = CanvasSlot->GetSize();
			if (Size.X <= 1.0f || Size.Y <= 1.0f || Size.X > 450.0f || Size.Y > 180.0f)
			{
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetSize(FVector2D(240.0f, 72.0f));
			}
			CanvasSlot->SetZOrder(100);
		}
	};

	if (UButton* Retry = Cast<UButton>(VictoryScreenWidget->GetWidgetFromName(TEXT("RetryButton"))))
	{
		Retry->OnClicked.Clear();
		Retry->OnClicked.AddDynamic(this, &ATDGameMode::RetryCurrentWave);
		PrepareButton(Retry);
	}
	if (UButton* Next = Cast<UButton>(VictoryScreenWidget->GetWidgetFromName(TEXT("NextWaveButton"))))
	{
		Next->OnClicked.Clear();
		Next->OnClicked.AddDynamic(this, &ATDGameMode::ContinueToNextWave);
		PrepareButton(Next);
	}

	UButton* Menu = Cast<UButton>(VictoryScreenWidget->GetWidgetFromName(TEXT("MainMenuBTN")));
	if (!Menu)
	{
		Menu = Cast<UButton>(VictoryScreenWidget->GetWidgetFromName(TEXT("MainMenuButton")));
	}
	if (Menu)
	{
		Menu->OnClicked.Clear();
		Menu->OnClicked.AddDynamic(this, &ATDGameMode::ReturnToMainMenu);
		PrepareButton(Menu);
	}
}

void ATDGameMode::PresentVictoryScreen()
{
	if (!VictoryScreenWidget)
	{
		return;
	}

	BindVictoryScreenButtons();

	auto SetNumber = [this](const TCHAR* Name, int32 Value, float AnchorX, float AnchorY)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(VictoryScreenWidget->GetWidgetFromName(Name)))
		{
			Text->SetText(FText::AsNumber(Value));
			Text->SetVisibility(ESlateVisibility::HitTestInvisible);
			Text->SetJustification(ETextJustify::Center);
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Text->Slot))
			{
				CanvasSlot->SetAnchors(FAnchors(AnchorX, AnchorY));
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				CanvasSlot->SetOffsets(FMargin(0.0f));
				CanvasSlot->SetAutoSize(true);
				CanvasSlot->SetZOrder(110);
			}
		}
	};

	FMetaCurrencyRewards Shown = LastMatchResult.Rewards;
	const bool bRewardsEmpty = Shown.ForestEssence == 0
		&& Shown.WoodenMight == 0
		&& Shown.GemStones == 0
		&& Shown.LightLanterns == 0;
	if (bRewardsEmpty)
	{
		Shown = LastMatchResult.Wallet;
	}

	SetNumber(TEXT("Score"), LastMatchResult.Score, 0.501f, 0.455f);
	SetNumber(TEXT("forestScore"), Shown.ForestEssence, 0.386f, 0.638f);
	SetNumber(TEXT("WoodScore"), Shown.WoodenMight, 0.464f, 0.638f);
	SetNumber(TEXT("GemScore"), Shown.GemStones, 0.539f, 0.638f);
	SetNumber(TEXT("LightScore"), Shown.LightLanterns, 0.614f, 0.638f);

	const bool bHasNextWave = LastMatchResult.TotalWaves > 0
		&& LastMatchResult.WavesCleared < LastMatchResult.TotalWaves;
	if (UButton* Next = Cast<UButton>(VictoryScreenWidget->GetWidgetFromName(TEXT("NextWaveButton"))))
	{
		Next->SetVisibility(bHasNextWave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	UTDMenuFunctionLibrary::StretchWidgetToFillScreen(VictoryScreenWidget, false);
	VictoryScreenWidget->SetVisibility(ESlateVisibility::Visible);
}

void ATDGameMode::TogglePause()
{
	if (bGameOver || IsVictory() || bWaveResultsVisible)
	{
		return;
	}

	if (bPaused || IsSettingsVisible())
	{
		ResumeFromSettings();
	}
	else
	{
		ShowSettings();
	}
}

void ATDGameMode::ShowSettings()
{
	if (bGameOver || IsVictory() || bWaveResultsVisible)
	{
		return;
	}

	if (!SettingsWidget && SettingsWidgetClass)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			SettingsWidget = CreateWidget<UUserWidget>(PC, SettingsWidgetClass);
			if (SettingsWidget)
			{
				SettingsWidget->AddToViewport(150);
			}
		}
	}

	if (!SettingsWidget)
	{
		bPaused = true;
		UGameplayStatics::SetGamePaused(this, true);
		return;
	}

	BindSettingsButtons();
	bPaused = true;
	UTDMenuFunctionLibrary::StretchWidgetToFillScreen(SettingsWidget, false);
	SettingsWidget->SetVisibility(ESlateVisibility::Visible);

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	UGameplayStatics::SetGamePaused(this, true);
}

void ATDGameMode::ResumeFromSettings()
{
	bPaused = false;
	UGameplayStatics::SetGamePaused(this, false);

	TArray<UUserWidget*> OpenWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this, OpenWidgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* Widget : OpenWidgets)
	{
		if (!Widget || !Widget->GetClass()->GetName().Contains(TEXT("SettingScreen")))
		{
			continue;
		}

		if (Widget == SettingsWidget)
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Widget->RemoveFromParent();
		}
	}

	RestoreGameplayInput();
}

bool ATDGameMode::IsSettingsVisible() const
{
	if (SettingsWidget && SettingsWidget->IsVisible())
	{
		return true;
	}

	TArray<UUserWidget*> OpenWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		GetWorld(), OpenWidgets, UUserWidget::StaticClass(), true);
	for (const UUserWidget* Widget : OpenWidgets)
	{
		if (Widget && Widget->IsVisible()
			&& Widget->GetClass()->GetName().Contains(TEXT("SettingScreen")))
		{
			return true;
		}
	}
	return false;
}

void ATDGameMode::HideEndScreens()
{
	if (EndScreenWidget)
	{
		EndScreenWidget->HideScreen();
	}
	if (VictoryScreenWidget)
	{
		VictoryScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ATDGameMode::RestoreGameplayInput()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
}

void ATDGameMode::EnsureDefaultWidgetClasses()
{
	if (!HUDWidgetClass)
	{
		if (UClass* FoundHUD = LoadClass<UTDHUDWidget>(
			nullptr, TEXT("/Game/UI/WBP_MatchHUD_V2.WBP_MatchHUD_V2_C")))
		{
			HUDWidgetClass = FoundHUD;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("TDGameMode: WBP_MatchHUD_V2 not found."));
		}
	}

	if (!EndScreenWidgetClass || EndScreenWidgetClass == UTDEndScreenWidget::StaticClass())
	{
		UClass* FoundDefeat = LoadClass<UTDEndScreenWidget>(
			nullptr, TEXT("/Game/UI/ScreenWidgets/Gameoverscreen.Gameoverscreen_C"));
		if (FoundDefeat)
		{
			EndScreenWidgetClass = FoundDefeat;
		}
		else
		{
			EndScreenWidgetClass = UTDEndScreenWidget::StaticClass();
			UE_LOG(LogTemp, Warning, TEXT("TDGameMode: No defeat screen Blueprint found."));
		}
	}

	if (!VictoryScreenWidgetClass
		|| VictoryScreenWidgetClass->IsChildOf(UTDEndScreenWidget::StaticClass()))
	{
		UClass* FoundVictory = LoadClass<UUserWidget>(
			nullptr, TEXT("/Game/UI/ScreenWidgets/VictoryScreen.VictoryScreen_C"));
		if (FoundVictory)
		{
			VictoryScreenWidgetClass = FoundVictory;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("TDGameMode: VictoryScreen not found."));
		}
	}

	if (!SettingsWidgetClass)
	{
		if (UClass* FoundSettings = LoadClass<UUserWidget>(
			nullptr, TEXT("/Game/UI/ScreenWidgets/SettingScreen.SettingScreen_C")))
		{
			SettingsWidgetClass = FoundSettings;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("TDGameMode: SettingScreen Blueprint not found."));
		}
	}
}

void ATDGameMode::ShowEndScreen(bool bVictory)
{
	const bool bUseDedicatedVictory = bVictory && VictoryScreenWidget;
	if (!bUseDedicatedVictory && !EndScreenWidget)
	{
		return;
	}

	bPaused = true;
	bWaveResultsVisible = true;

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	if (bUseDedicatedVictory)
	{
		PresentVictoryScreen();
	}
	else
	{
		EndScreenWidget->PresentMatchResult(bVictory, LastMatchResult);
	}
}
