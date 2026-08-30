// TDEndScreenWidget.cpp — see TDEndScreenWidget.h

#include "TDEndScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace
{
	static UTextBlock* MakeCenteredLabel(UWidgetTree* Tree, const FString& Name, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(Color);
		Label->SetJustification(ETextJustify::Center);
		Label->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		return Label;
	}
}

bool UTDEndScreenWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!WidgetTree)
	{
		return true;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EndScreenCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	DimOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimOverlay"));
	DimOverlay->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	TitleText = MakeCenteredLabel(WidgetTree, TEXT("TitleText"), 96, FLinearColor(0.95f, 0.15f, 0.15f));
	if (UCanvasPanelSlot* TitleSlot = RootCanvas->AddChildToCanvas(TitleText))
	{
		TitleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		TitleSlot->SetPosition(FVector2D(0.0f, -40.0f));
		TitleSlot->SetAutoSize(true);
	}

	HintText = MakeCenteredLabel(WidgetTree, TEXT("HintText"), 34, FLinearColor::White);
	if (UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(HintText))
	{
		HintSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		HintSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		HintSlot->SetPosition(FVector2D(0.0f, 50.0f));
		HintSlot->SetAutoSize(true);
	}

	SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

void UTDEndScreenWidget::PresentScreen(const FString& Title, const FLinearColor& TitleColor)
{
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Title));
		TitleText->SetColorAndOpacity(TitleColor);
	}
	if (HintText)
	{
		HintText->SetText(FText::FromString(TEXT("Press R to restart")));
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UTDEndScreenWidget::ShowGameOver()
{
	PresentScreen(TEXT("GAME OVER"), FLinearColor(0.95f, 0.15f, 0.15f));
}

void UTDEndScreenWidget::ShowVictory()
{
	PresentScreen(TEXT("VICTORY"), FLinearColor(0.2f, 0.95f, 0.35f));
}

void UTDEndScreenWidget::HideScreen()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
