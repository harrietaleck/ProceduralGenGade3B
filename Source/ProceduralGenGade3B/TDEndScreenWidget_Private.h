// Shared helpers for TDEndScreenWidget implementation units.
#pragma once

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"

namespace TDEndScreenPrivate
{
	inline void StretchWidgetToFillParentSlot(UWidget* Widget)
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

	inline void StyleRewardNumber(UTextBlock* Text)
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

	inline void PlaceNumberUnderIcon(
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
		if (Text->GetParent() != Canvas)
		{
			Canvas->AddChildToCanvas(Text);
		}

		StyleRewardNumber(Text);
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Text->Slot))
		{
			Slot->SetAnchors(FAnchors(AnchorX, AnchorY));
			Slot->SetAlignment(FVector2D(0.5f, 0.0f));
			Slot->SetAutoSize(true);
			Slot->SetZOrder(50);
			Slot->SetOffsets(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
		}
		Member = Text;
	}

	inline void EnsureRewardNumbersUnderIcons(
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
		if (UWidget* OldRow = Owner->GetWidgetFromName(TEXT("ResultRewardRow")))
		{
			OldRow->SetVisibility(ESlateVisibility::Collapsed);
		}
		PlaceNumberUnderIcon(Owner, Tree, Canvas, Essence, TEXT("ResultReward_Essence"), 0.376f, 0.625f);
		PlaceNumberUnderIcon(Owner, Tree, Canvas, Wood, TEXT("ResultReward_Wood"), 0.459f, 0.625f);
		PlaceNumberUnderIcon(Owner, Tree, Canvas, Gems, TEXT("ResultReward_Gems"), 0.542f, 0.625f);
		PlaceNumberUnderIcon(Owner, Tree, Canvas, Lanterns, TEXT("ResultReward_Lanterns"), 0.615f, 0.625f);
	}

	inline void CenterScoreAcrossPanel(UTextBlock* Text)
	{
		if (!Text)
		{
			return;
		}
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(false);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Text->Slot))
		{
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

	inline UTextBlock* MakeLabel(UWidgetTree* Tree, const FString& Name, int32 FontSize, const FLinearColor& Color)
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

	inline UBorder* MakeRewardCell(
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

	inline UButton* MakeActionButton(UWidgetTree* Tree, const FString& Name, const FString& Caption)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
		UTextBlock* Label = MakeLabel(Tree, Name + TEXT("_Label"), 16, FLinearColor::White);
		Label->SetText(FText::FromString(Caption));
		Button->SetContent(Label);
		return Button;
	}
}
