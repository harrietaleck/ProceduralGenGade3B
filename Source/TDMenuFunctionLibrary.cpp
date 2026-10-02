#include "TDMenuFunctionLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Layout/Anchors.h"

namespace
{
	static bool IsMenuScreenClass(const FString& Name)
	{
		return Name.Contains(TEXT("StartScreen"))
			|| Name.Contains(TEXT("SettingScreen"))
			|| Name.Contains(TEXT("GameOverview"))
			|| Name.Contains(TEXT("DefendersScreen"))
			|| Name.Contains(TEXT("UpgradesScreen"))
			|| Name.Contains(TEXT("Upgrade"))
			|| Name.Contains(TEXT("Store"));
	}

	static int32 PreferredMenuZOrder(const FString& Name)
	{
		// Keep overlays above the start screen.
		if (Name.Contains(TEXT("StartScreen")))
		{
			return 100;
		}
		return 150;
	}

	static void StretchCanvasChild(UWidget* Child)
	{
		if (!Child)
		{
			return;
		}
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Child->Slot))
		{
			Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			Slot->SetOffsets(FMargin(0.0f));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetAutoSize(false);
		}
	}

	static void StretchImageBrush(UImage* Image)
	{
		if (!Image)
		{
			return;
		}
		StretchCanvasChild(Image);
		FSlateBrush Brush = Image->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.ImageSize = FVector2D(1920.0f, 1080.0f);
		Image->SetBrush(Brush);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	static void StretchImagesRecursive(UWidget* Root, bool bForceScreenImage)
	{
		if (!Root)
		{
			return;
		}

		if (UImage* Image = Cast<UImage>(Root))
		{
			// Only resize images that already represent a screen-sized background.
			// Stretching icons and decorative images makes them overlap the entire
			// viewport and intercept clicks intended for menu buttons.
			if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
			{
				const FVector2D Size = CanvasSlot->GetSize();
				const FAnchors Anchors = CanvasSlot->GetAnchors();
				const bool bAlreadyStretched =
					(Anchors.Maximum.X - Anchors.Minimum.X) > 0.9f
					&& (Anchors.Maximum.Y - Anchors.Minimum.Y) > 0.9f;
				const bool bLargeFixedBackground = Size.X >= 800.0f && Size.Y >= 450.0f;
				const UObject* BrushResource = Image->GetBrush().GetResourceObject();
				const FString ResourceName = BrushResource ? BrushResource->GetName() : FString();
				const FString WidgetName = Image->GetName();
				const bool bKnownScreenBackground =
					WidgetName.Contains(TEXT("Background"))
					|| ResourceName.Contains(TEXT("StartScreen-image"))
					|| ResourceName.Contains(TEXT("Settings-image"))
					|| ResourceName.Contains(TEXT("gaveoverview-image"))
					|| ResourceName.Contains(TEXT("Gameover-image"))
					|| ResourceName.Contains(TEXT("Victory-image"));

				if (bForceScreenImage || bAlreadyStretched || bLargeFixedBackground || bKnownScreenBackground)
				{
					StretchImageBrush(Image);
				}
			}
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			const int32 Count = Panel->GetChildrenCount();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				StretchImagesRecursive(Panel->GetChildAt(Index), bForceScreenImage);
			}
		}
	}
}

void UTDMenuFunctionLibrary::StartGameplayFromMenu(UObject* WorldContextObject, FName GameplayLevelName)
{
	if (!WorldContextObject || GameplayLevelName.IsNone())
	{
		return;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	UWidgetLayoutLibrary::RemoveAllWidgets(World);
	UGameplayStatics::OpenLevel(World, GameplayLevelName);
}

void UTDMenuFunctionLibrary::StretchWidgetToFillScreen(UUserWidget* Widget, bool bCaptureMouseFocus)
{
	if (!Widget)
	{
		return;
	}

	const FString ClassName = Widget->GetClass()->GetName();
	const int32 WantedZ = PreferredMenuZOrder(ClassName);
	const bool bForceScreenImage =
		ClassName.Contains(TEXT("SettingScreen"))
		|| ClassName.Contains(TEXT("GameOverview"));

	// Do NOT call SetDesiredSizeInViewport — that forces point anchors and a fixed size.
	if (UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get())
	{
		FGameViewportWidgetSlot Slot;
		if (ViewportSubsystem->IsWidgetAdded(Widget))
		{
			Slot = ViewportSubsystem->GetWidgetSlot(Widget);
		}
		else
		{
			Widget->AddToViewport(WantedZ);
			Slot = ViewportSubsystem->GetWidgetSlot(Widget);
		}

		Slot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
		Slot.Offsets = FMargin(0.0f);
		Slot.Alignment = FVector2D(0.0f, 0.0f);
		Slot.ZOrder = FMath::Max(Slot.ZOrder, WantedZ);
		ViewportSubsystem->SetWidgetSlot(Widget, Slot);
	}
	else
	{
		Widget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		Widget->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
		Widget->SetPositionInViewport(FVector2D::ZeroVector, false);
	}

	UWidget* Root = Widget->GetRootWidget();
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
	// Stretch only screen-sized background images; controls retain their designer slots.
	StretchImagesRecursive(Root, bForceScreenImage);

	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->SetIsEnabled(true);

	if (bCaptureMouseFocus)
	{
		if (APlayerController* PC = Widget->GetOwningPlayer())
		{
			PC->bShowMouseCursor = true;
			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
		}
	}
}

void UTDMenuFunctionLibrary::StretchOpenMenuScreens(UObject* WorldContextObject)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!World)
	{
		return;
	}

	TArray<UUserWidget*> MenuWidgets;
	// Include non-top-level in case a screen was parented oddly.
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		World,
		MenuWidgets,
		UUserWidget::StaticClass(),
		/*TopLevelOnly=*/true);

	for (UUserWidget* Widget : MenuWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		const FString Name = Widget->GetClass()->GetName();
		if (!IsMenuScreenClass(Name))
		{
			continue;
		}

		// Do not resurrect removed/hidden menu screens. The owning Blueprint decides
		// which screen is active; this helper only corrects active screen geometry.
		if (UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get())
		{
			if (!ViewportSubsystem->IsWidgetAdded(Widget))
			{
				continue;
			}
		}

		const ESlateVisibility PreviousVisibility = Widget->GetVisibility();
		const bool bWasEnabled = Widget->GetIsEnabled();
		StretchWidgetToFillScreen(Widget, /*bCaptureMouseFocus=*/false);
		Widget->SetVisibility(PreviousVisibility);
		Widget->SetIsEnabled(bWasEnabled);
	}
}
