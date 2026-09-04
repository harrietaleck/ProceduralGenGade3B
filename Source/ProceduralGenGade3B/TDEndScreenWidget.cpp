// TDEndScreenWidget.cpp — construct / show / clicks, plus included layout / theme units.

#include "TDEndScreenWidget.h"
#include "TDEndScreenWidget_Private.h"
#include "TDGameMode.h"
#include "Blueprint/GameViewportSubsystem.h"
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

using namespace TDEndScreenPrivate;

void UTDEndScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureFallbackLayout();
	ResolveOptionalWidgetBindings();
	EnsureThemeArt();
	EnsureBlueprintLayoutFitsScreen();
	BindActionButtons();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTDEndScreenWidget::EnsureFallbackLayout()
{
	if (bBuiltFallbackLayout)
	{
		return;
	}
	if (!RootCanvas)
	{
		RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	}
	if (RootCanvas || GetRootWidget() || !WidgetTree)
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
	auto AddButton = [ButtonRow](UWidget* Child)
	{
		if (UHorizontalBoxSlot* Slot = ButtonRow->AddChildToHorizontalBox(Child))
		{
			Slot->SetPadding(FMargin(6.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};
	AddButton(RetryButton);
	AddButton(NextWaveButton);
	AddButton(MainMenuButton);

	auto AddRow = [Layout](UWidget* Child, float Top = 8.0f, EHorizontalAlignment HAlign = HAlign_Center)
	{
		if (UVerticalBoxSlot* Slot = Layout->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.0f, Top, 0.0f, 0.0f));
			Slot->SetHorizontalAlignment(HAlign);
		}
	};
	AddRow(TitleText, 0.0f);
	AddRow(SubtitleText, 4.0f);
	AddRow(ScoreHeader, 14.0f);
	AddRow(ScoreValueText, 2.0f);
	AddRow(RewardsHeader, 12.0f);
	AddRow(RewardRow, 4.0f, HAlign_Fill);
	AddRow(ButtonRow, 16.0f, HAlign_Fill);

	PanelBorder->SetContent(Layout);
	ScreenScaleBox->SetContent(PanelSize);
	if (UCanvasPanelSlot* ScaleSlot = RootCanvas->AddChildToCanvas(ScreenScaleBox))
	{
		ScaleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ScaleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ScaleSlot->SetAutoSize(true);
	}
}

void UTDEndScreenWidget::PresentMatchResult(bool bVictory, const FMatchResult& Result)
{
	EnsureFallbackLayout();
	ResolveOptionalWidgetBindings();
	EnsureResultTextWidgets();
	EnsureBlueprintLayoutFitsScreen();
	BindActionButtons();

	const bool bOfferNextWave = bVictory
		&& Result.TotalWaves > 0
		&& Result.WavesCleared < Result.TotalWaves;
	bPendingNextWave = bOfferNextWave;
	bPresentedVictory = bVictory;

	ApplyTheme(bVictory, Result.BeamTier, bOfferNextWave);
	ApplyTierTypography(Result);
	ApplyMatchResultToWidgets(Result);
	LayoutDefeatResultWidgets();

	SetVisibility(ESlateVisibility::Visible);
	OnMatchResultPresented(bVictory, Result);
}

void UTDEndScreenWidget::ShowGameOver()
{
	FMatchResult Result;
	if (UWorld* World = GetWorld())
	{
		if (ATDGameMode* GameMode = World->GetAuthGameMode<ATDGameMode>())
		{
			Result = GameMode->GetLastMatchResult();
		}
	}
	PresentMatchResult(false, Result);
}

void UTDEndScreenWidget::ShowVictory()
{
	FMatchResult Result;
	if (UWorld* World = GetWorld())
	{
		if (ATDGameMode* GameMode = World->GetAuthGameMode<ATDGameMode>())
		{
			Result = GameMode->GetLastMatchResult();
		}
	}
	PresentMatchResult(true, Result);
}

void UTDEndScreenWidget::HideScreen()
{
	bPendingNextWave = false;
	bPresentedVictory = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTDEndScreenWidget::OnRetryClicked()
{
	if (ATDGameMode* GameMode = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (bPendingNextWave)
		{
			GameMode->ContinueToNextWave();
		}
		else if (bPresentedVictory)
		{
			GameMode->RetryCurrentWave();
		}
		else
		{
			GameMode->RestartGame();
		}
	}
}

void UTDEndScreenWidget::OnNextWaveClicked()
{
	if (ATDGameMode* GameMode = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->ContinueToNextWave();
	}
}

void UTDEndScreenWidget::OnMainMenuClicked()
{
	if (ATDGameMode* GameMode = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->ReturnToMainMenu();
	}
}

#include "TDEndScreenWidget_Layout.inl"
#include "TDEndScreenWidget_Theme.inl"
