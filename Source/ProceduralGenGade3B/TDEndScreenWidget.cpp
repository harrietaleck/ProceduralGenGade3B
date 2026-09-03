// TDEndScreenWidget.cpp — see TDEndScreenWidget.h

#include "TDEndScreenWidget.h"
#include "TDGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	static UTextBlock* MakeLabel(UWidgetTree* Tree, const FString& Name, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(Color);
		Label->SetJustification(ETextJustify::Center);
		Label->SetShadowOffset(FVector2D(1.5f, 1.5f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
		return Label;
	}

	static UBorder* MakeRewardCell(
		UWidgetTree* Tree,
		const FString& Name,
		const FString& IconLabel,
		const FString& ResourceName,
		const FLinearColor& Accent,
		TObjectPtr<UTextBlock>& OutAmountText)
	{
		UBorder* Cell = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), *Name);
		Cell->SetPadding(FMargin(8.0f, 6.0f));
		Cell->SetBrushColor(FLinearColor(Accent.R * 0.18f, Accent.G * 0.18f, Accent.B * 0.18f, 0.92f));

		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *(Name + TEXT("_Column")));
		UTextBlock* Icon = MakeLabel(Tree, Name + TEXT("_Icon"), 20, Accent);
		Icon->SetText(FText::FromString(IconLabel));
		UTextBlock* Title = MakeLabel(Tree, Name + TEXT("_Title"), 12, FLinearColor(0.92f, 0.92f, 0.92f));
		Title->SetText(FText::FromString(ResourceName));
		OutAmountText = MakeLabel(Tree, Name + TEXT("_Amount"), 24, FLinearColor(1.0f, 0.86f, 0.35f));
		OutAmountText->SetText(FText::FromString(TEXT("0")));

		if (UVerticalBoxSlot* IconSlot = Column->AddChildToVerticalBox(Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* AmountSlot = Column->AddChildToVerticalBox(OutAmountText))
		{
			AmountSlot->SetHorizontalAlignment(HAlign_Center);
		}

		Cell->SetContent(Column);
		return Cell;
	}

	static UButton* MakeActionButton(UWidgetTree* Tree, const FString& Name, const FString& Caption)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
		UTextBlock* Label = MakeLabel(Tree, Name + TEXT("_Label"), 16, FLinearColor::White);
		Label->SetText(FText::FromString(Caption));
		Button->SetContent(Label);
		return Button;
	}
}

void UTDEndScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureFallbackLayout();
	EnsureThemeArt();

	if (!bBuiltFallbackLayout)
	{
		if (RetryButton)
		{
			RetryButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnRetryClicked);
		}
		if (NextWaveButton)
		{
			NextWaveButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnNextWaveClicked);
		}
		if (MainMenuButton)
		{
			MainMenuButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnMainMenuClicked);
		}
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UTDEndScreenWidget::EnsureFallbackLayout()
{
	if (bBuiltFallbackLayout || RootCanvas)
	{
		return;
	}

	if (!WidgetTree)
	{
		return;
	}

	bBuiltFallbackLayout = true;

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	DimOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimOverlay"));
	DimOverlay->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.78f));
	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	ScreenScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ScreenScaleBox"));
	ScreenScaleBox->SetStretch(EStretch::ScaleToFit);
	ScreenScaleBox->SetStretchDirection(EStretchDirection::Both);

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	PanelSize->SetWidthOverride(900.0f);
	PanelSize->SetHeightOverride(560.0f);

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetPadding(FMargin(24.0f, 20.0f));
	PanelSize->SetContent(PanelBorder);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelLayout"));
	TitleText = MakeLabel(WidgetTree, TEXT("TitleText"), 40, FLinearColor(0.35f, 0.95f, 0.45f));
	TitleText->SetText(FText::FromString(TEXT("VICTORY!")));
	SubtitleText = MakeLabel(WidgetTree, TEXT("SubtitleText"), 16, FLinearColor(0.72f, 0.9f, 0.72f));
	SubtitleText->SetText(FText::FromString(TEXT("THE FOREST IS SAFE")));

	UTextBlock* ScoreHeader = MakeLabel(WidgetTree, TEXT("ScoreHeader"), 14, FLinearColor(0.85f, 0.75f, 0.35f));
	ScoreHeader->SetText(FText::FromString(TEXT("SCORE")));
	ScoreValueText = MakeLabel(WidgetTree, TEXT("ScoreValueText"), 36, FLinearColor(1.0f, 0.86f, 0.35f));
	ScoreValueText->SetText(FText::FromString(TEXT("0")));

	UTextBlock* RewardsHeader = MakeLabel(WidgetTree, TEXT("RewardsHeader"), 14, FLinearColor(0.85f, 0.75f, 0.35f));
	RewardsHeader->SetText(FText::FromString(TEXT("REWARDS")));

	UHorizontalBox* RewardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RewardRow"));
	auto AddReward = [RewardRow](UWidget* Child)
	{
		if (UHorizontalBoxSlot* Slot = RewardRow->AddChildToHorizontalBox(Child))
		{
			Slot->SetPadding(FMargin(4.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};
	AddReward(MakeRewardCell(WidgetTree, TEXT("ForestCell"), TEXT("Leaf"), TEXT("Forest Essence"), FLinearColor(0.25f, 0.9f, 0.35f), ForestEssenceText));
	AddReward(MakeRewardCell(WidgetTree, TEXT("WoodCell"), TEXT("Logs"), TEXT("Wooden Might"), FLinearColor(0.72f, 0.48f, 0.22f), WoodenMightText));
	AddReward(MakeRewardCell(WidgetTree, TEXT("GemCell"), TEXT("Gems"), TEXT("Gem Stones"), FLinearColor(0.45f, 0.65f, 1.0f), GemStonesText));
	AddReward(MakeRewardCell(WidgetTree, TEXT("LanternCell"), TEXT("Sun"), TEXT("Light Lanterns"), FLinearColor(1.0f, 0.82f, 0.25f), LightLanternsText));

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
	RetryButton = MakeActionButton(WidgetTree, TEXT("RetryButton"), TEXT("RETRY"));
	NextWaveButton = MakeActionButton(WidgetTree, TEXT("NextWaveButton"), TEXT("NEXT WAVE"));
	MainMenuButton = MakeActionButton(WidgetTree, TEXT("MainMenuButton"), TEXT("MAIN MENU"));
	RetryButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnRetryClicked);
	NextWaveButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnNextWaveClicked);
	MainMenuButton->OnClicked.AddDynamic(this, &UTDEndScreenWidget::OnMainMenuClicked);

	auto AddRow = [Layout](UWidget* Child, float Top = 8.0f)
	{
		if (UVerticalBoxSlot* Slot = Layout->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.0f, Top, 0.0f, 0.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	AddRow(TitleText, 0.0f);
	AddRow(SubtitleText, 4.0f);
	AddRow(ScoreHeader, 14.0f);
	AddRow(ScoreValueText, 2.0f);
	AddRow(RewardsHeader, 12.0f);
	AddRow(RewardRow, 4.0f);
	AddRow(ButtonRow, 16.0f);

	PanelBorder->SetContent(Layout);
	ScreenScaleBox->SetContent(PanelSize);

	if (UCanvasPanelSlot* ScaleSlot = RootCanvas->AddChildToCanvas(ScreenScaleBox))
	{
		ScaleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ScaleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ScaleSlot->SetAutoSize(true);
	}
}

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
				Image->SetBrushFromTexture(Texture, true);
				return;
			}
		}
	};

	// Always re-bind so soft refs lost after package moves still show art at runtime.
	AssignTexture(VictoryBackground, {
		TEXT("/Game/UI/SourceArt/Victory-image.Victory-image"),
		TEXT("/Game/UI/SourceArt/VictoryBackground-image.VictoryBackground-image")
	});
	AssignTexture(DefeatBackground, {
		TEXT("/Game/UI/SourceArt/Gameover-image.Gameover-image"),
		TEXT("/Game/UI/SourceArt/Defeatbackground-image.Defeatbackground-image")
	});
}

void UTDEndScreenWidget::ApplyTheme(bool bVictory, EBeamHealthTier Tier)
{
	EnsureThemeArt();

	const bool bUsesConceptArt = VictoryBackground != nullptr;
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
		TitleText->SetText(FText::FromString(bVictory ? TEXT("VICTORY!") : TEXT("DEFEAT")));
		TitleText->SetColorAndOpacity(TitleColor);
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(FText::FromString(bVictory ? TEXT("THE FOREST IS SAFE") : TEXT("THE LIGHT HAS FADED")));
		SubtitleText->SetColorAndOpacity(SubtitleColor);
	}
	if (ScoreHeaderText)
	{
		ScoreHeaderText->SetText(FText::FromString(TEXT("— SCORE —")));
	}
	if (RewardsHeaderText)
	{
		RewardsHeaderText->SetText(FText::FromString(TEXT("— REWARDS —")));
	}
	if (PanelBorder)
	{
		PanelBorder->SetBrushColor(PanelColor);
	}
	if (VictoryBackground)
	{
		VictoryBackground->SetVisibility(bVictory ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DefeatBackground)
	{
		DefeatBackground->SetVisibility(bVictory ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (PanelBorder && VictoryBackground)
	{
		PanelBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
	else if (PanelBorder)
	{
		PanelBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (RetryButton)
	{
		RetryButton->SetBackgroundColor(ButtonFill);
		RetryButton->SetVisibility(ESlateVisibility::Visible);
	}
	if (NextWaveButton)
	{
		NextWaveButton->SetBackgroundColor(ButtonFill);
		NextWaveButton->SetVisibility(bVictory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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

void UTDEndScreenWidget::PresentMatchResult(bool bVictory, const FMatchResult& Result)
{
	EnsureFallbackLayout();
	ApplyTheme(bVictory, Result.BeamTier);
	ApplyTierTypography(Result);

	if (ScoreValueText)
	{
		ScoreValueText->SetText(FText::AsNumber(Result.Score));
	}
	if (ForestEssenceText)
	{
		ForestEssenceText->SetText(FText::AsNumber(Result.Rewards.ForestEssence));
	}
	if (WoodenMightText)
	{
		WoodenMightText->SetText(FText::AsNumber(Result.Rewards.WoodenMight));
	}
	if (GemStonesText)
	{
		GemStonesText->SetText(FText::AsNumber(Result.Rewards.GemStones));
	}
	if (LightLanternsText)
	{
		LightLanternsText->SetText(FText::AsNumber(Result.Rewards.LightLanterns));
	}

	SetVisibility(ESlateVisibility::Visible);
	OnMatchResultPresented(bVictory, Result);
}

void UTDEndScreenWidget::ShowGameOver()
{
	if (ATDGameMode* GameMode = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		PresentMatchResult(false, GameMode->GetLastMatchResult());
	}
	else
	{
		PresentMatchResult(false, FMatchResult());
	}
}

void UTDEndScreenWidget::ShowVictory()
{
	if (ATDGameMode* GameMode = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		PresentMatchResult(true, GameMode->GetLastMatchResult());
	}
	else
	{
		PresentMatchResult(true, FMatchResult());
	}
}

void UTDEndScreenWidget::HideScreen()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTDEndScreenWidget::OnRetryClicked()
{
	if (ATDGameMode* GameMode = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->RestartGame();
	}
}

void UTDEndScreenWidget::OnNextWaveClicked()
{
	OnRetryClicked();
}

void UTDEndScreenWidget::OnMainMenuClicked()
{
	OnRetryClicked();
}
