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
#include "Components/OverlaySlot.h"
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
	ResolveOptionalWidgetBindings();
	EnsureThemeArt();
	EnsureBlueprintLayoutFitsScreen();

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
	// Never destroy a designer Widget Blueprint tree. Only build C++ chrome when the
	// widget has no root at all (pure C++ UTDEndScreenWidget with no Blueprint child).
	if (bBuiltFallbackLayout)
	{
		return;
	}

	if (!RootCanvas)
	{
		RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	}

	if (RootCanvas || GetRootWidget())
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

namespace
{
	static void StretchWidgetToFillParentSlot(UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetAutoSize(false);
		}
	}

	static bool IsInLayoutBox(UWidget* Widget)
	{
		if (!Widget || !Widget->Slot)
		{
			return false;
		}
		return Cast<UVerticalBoxSlot>(Widget->Slot)
			|| Cast<UHorizontalBoxSlot>(Widget->Slot)
			|| Cast<UOverlaySlot>(Widget->Slot);
	}

	static void HideIfCanvasFloater(UWidget* Widget)
	{
		if (Widget && Cast<UCanvasPanelSlot>(Widget->Slot) && !IsInLayoutBox(Widget))
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	static void StyleRewardNumber(UTextBlock* Text)
	{
		if (!Text)
		{
			return;
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = 22;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FLinearColor(1.0f, 0.95f, 0.78f));
		Text->SetJustification(ETextJustify::Center);
		Text->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	/** Place one reward number centered under its icon box (matches the framed art). */
	static void PlaceNumberUnderIcon(
		UUserWidget* Owner,
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		TObjectPtr<UTextBlock>& Member,
		const TCHAR* Name,
		float AnchorX,
		float AnchorY)
	{
		if (!Owner || !Tree || !Canvas)
		{
			return;
		}

		UTextBlock* Text = Cast<UTextBlock>(Owner->GetWidgetFromName(Name));
		if (!Text)
		{
			Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			Canvas->AddChildToCanvas(Text);
		}

		// If it was parented into the old HorizontalBox row, move it back onto the root canvas.
		if (Text->GetParent() != Canvas)
		{
			Canvas->AddChildToCanvas(Text);
		}

		StyleRewardNumber(Text);

		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Text->Slot))
		{
			Slot->SetAnchors(FAnchors(AnchorX, AnchorY));
			Slot->SetAlignment(FVector2D(0.5f, 0.0f)); // top-center of text sits just under the icon
			Slot->SetAutoSize(true);
			Slot->SetZOrder(50);
			Slot->SetOffsets(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
		}

		Member = Text;
	}

	static void EnsureRewardNumbersUnderIcons(
		UUserWidget* Owner,
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		TObjectPtr<UTextBlock>& Essence,
		TObjectPtr<UTextBlock>& Wood,
		TObjectPtr<UTextBlock>& Gems,
		TObjectPtr<UTextBlock>& Lanterns)
	{
		if (!Owner || !Tree || !Canvas)
		{
			return;
		}

		// Collapse legacy ResultRewardRow if present.
		if (UWidget* OldRow = Owner->GetWidgetFromName(TEXT("ResultRewardRow")))
		{
			OldRow->SetVisibility(ESlateVisibility::Collapsed);
		}

		// Reward icon centers on Gameover-image (1920x1080): leaf/logs/gems/sun.
		PlaceNumberUnderIcon(Owner, Tree, Canvas, Essence, TEXT("ResultReward_Essence"), 0.376f, 0.625f);
		PlaceNumberUnderIcon(Owner, Tree, Canvas, Wood, TEXT("ResultReward_Wood"), 0.459f, 0.625f);
		PlaceNumberUnderIcon(Owner, Tree, Canvas, Gems, TEXT("ResultReward_Gems"), 0.542f, 0.625f);
		PlaceNumberUnderIcon(Owner, Tree, Canvas, Lanterns, TEXT("ResultReward_Lanterns"), 0.615f, 0.625f);
	}

	static void CenterScoreAcrossPanel(UTextBlock* Text)
	{
		if (!Text)
		{
			return;
		}

		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(false);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Text->Slot))
		{
			// Keep score centered under the SCORE header.
			constexpr float ScoreAnchorY = 0.480f;
			CanvasSlot->SetAnchors(FAnchors(0.0f, ScoreAnchorY, 1.0f, ScoreAnchorY));
			CanvasSlot->SetOffsets(FMargin(0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetZOrder(50);
		}
		else if (UVerticalBoxSlot* VertSlot = Cast<UVerticalBoxSlot>(Text->Slot))
		{
			VertSlot->SetHorizontalAlignment(HAlign_Fill);
			VertSlot->SetVerticalAlignment(VAlign_Center);
			VertSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
		}
	}

	static void PositionTextOnCanvas(UTextBlock* Text, float AnchorX, float AnchorY)
	{
		if (!Text)
		{
			return;
		}

		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Text->Slot))
		{
			Slot->SetAnchors(FAnchors(AnchorX, AnchorY));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetAutoSize(true);
			Slot->SetZOrder(10);
		}

		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	static FString BeamTierLabel(EBeamHealthTier Tier)
	{
		switch (Tier)
		{
		case EBeamHealthTier::Fragile: return TEXT("Fragile");
		case EBeamHealthTier::Steady:  return TEXT("Steady");
		case EBeamHealthTier::Radiant: return TEXT("Radiant");
		default: return TEXT("Steady");
		}
	}
}

void UTDEndScreenWidget::EnsureBlueprintLayoutFitsScreen()
{
	// Pin the UserWidget itself to the viewport. Without this, UMG uses the Blueprint's
	// design resolution (e.g. 1920x1080) and the screen sits at a fixed size.
	SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	SetAlignmentInViewport(FVector2D(0.0f, 0.0f));

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
	ResolveButton(MainMenuButton, TEXT("MainMenuButton"), TEXT("MainMenuBTN"));
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

	// Hide duplicate Blueprint reward text so runtime labels own the values.
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

	EnsureRewardNumbersUnderIcons(
		this,
		WidgetTree,
		Canvas,
		ForestEssenceText,
		WoodenMightText,
		GemStonesText,
		LightLanternsText);
}

void UTDEndScreenWidget::LayoutDefeatResultWidgets()
{
	CenterScoreAcrossPanel(ScoreValueText);

	if (UWidget* OldRow = GetWidgetFromName(TEXT("ResultRewardRow")))
	{
		OldRow->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Re-pin each value under its icon (leaf / logs / gems / sun).
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
	// Prefer match rewards earned this run; fall back to wallet if rewards are empty.
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
				// Do not match texture pixel size — that breaks full-screen Blueprint layouts.
				Image->SetBrushFromTexture(Texture, false);
				StretchWidgetToFillParentSlot(Image);
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

void UTDEndScreenWidget::ApplyTheme(bool bVictory, EBeamHealthTier Tier, bool bOfferNextWave)
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

	// When this Blueprint only has a defeat background image, swap in victory art for wave results.
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
	if (PanelBorder && bUsesConceptArt)
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
		// Between waves, Retry acts as Next Wave when no NextWaveButton exists.
		RetryButton->SetVisibility(ESlateVisibility::Visible);
		if (UTextBlock* RetryLabel = Cast<UTextBlock>(RetryButton->GetContent()))
		{
			RetryLabel->SetText(FText::FromString(bOfferNextWave ? TEXT("NEXT WAVE") : TEXT("RETRY")));
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

void UTDEndScreenWidget::PresentMatchResult(bool bVictory, const FMatchResult& Result)
{
	EnsureFallbackLayout();
	ResolveOptionalWidgetBindings();
	EnsureResultTextWidgets();
	EnsureBlueprintLayoutFitsScreen();

	const bool bOfferNextWave = bVictory
		&& Result.TotalWaves > 0
		&& Result.WavesCleared < Result.TotalWaves;
	bPendingNextWave = bOfferNextWave;

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
	OnRetryClicked();
}
