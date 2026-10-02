// TDWarningBannerWidget.cpp — see TDWarningBannerWidget.h

#include "TDWarningBannerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

bool UTDWarningBannerWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!WidgetTree)
	{
		return true;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WarningCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	Banner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WarningBanner"));
	Banner->SetBrushColor(FLinearColor(0.45f, 0.05f, 0.05f, 0.94f));
	Banner->SetPadding(FMargin(22.0f, 12.0f));
	Banner->SetVisibility(ESlateVisibility::Collapsed);

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WarningMessage"));
	FSlateFontInfo Font = MessageText->GetFont();
	Font.Size = 30;
	MessageText->SetFont(Font);
	MessageText->SetColorAndOpacity(FLinearColor::White);
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetShadowOffset(FVector2D(1.5f, 1.5f));
	MessageText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	Banner->SetContent(MessageText);

	if (UCanvasPanelSlot* BannerSlot = RootCanvas->AddChildToCanvas(Banner))
	{
		BannerSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		BannerSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		BannerSlot->SetPosition(FVector2D(0.0f, -130.0f));
		BannerSlot->SetAutoSize(true);
	}

	return true;
}

void UTDWarningBannerWidget::ShowWarning(const FString& Message, float DurationSeconds)
{
	if (MessageText)
	{
		MessageText->SetText(FText::FromString(Message));
	}
	if (Banner)
	{
		Banner->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
		World->GetTimerManager().SetTimer(
			HideTimerHandle,
			this,
			&UTDWarningBannerWidget::HideWarningInternal,
			DurationSeconds,
			/*bLoop=*/false);
	}
}

void UTDWarningBannerWidget::HideWarning()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}
	HideWarningInternal();
}

void UTDWarningBannerWidget::HideWarningInternal()
{
	if (Banner)
	{
		Banner->SetVisibility(ESlateVisibility::Collapsed);
	}
}
