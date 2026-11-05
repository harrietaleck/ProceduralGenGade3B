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
		// Other menus go on top of the start screen.
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
			// Only stretch images that are meant to be full screen backgrounds.
			// If we stretch icons too they cover the whole screen and block clicks
			// on the menu buttons.
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

	// Don't use SetDesiredSizeInViewport here. It forces point anchors and a fixed size.
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
	// Only stretch the background images. Buttons and other controls keep their designer layout.
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
	// Get every user widget in the world so we can find the menu screens.
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

		// Skip screens that were removed, we don't want to bring them back. The Blueprint
		// picks which screen is open. This just fixes the size of the open ones.
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

// *** Root cause of shifted menu elements: the background art is stretched to the whole viewport
// *** (StretchImagesRecursive), but buttons/text keep fixed designer pixel anchors. UMG's DPI curve
// *** scales those pixels by screen height only, so on any aspect ratio other than the designer's
// *** 16:9 canvas they drift sideways from the art. Re-anchoring each element proportionally to the
// *** designer canvas makes it scale exactly like the stretched background, keeping the original layout.
void UTDMenuFunctionLibrary::PinCanvasLayoutToDesignSize(UUserWidget* Widget, FVector2D DesignSize)
{
	UCanvasPanel* Canvas = Widget ? Cast<UCanvasPanel>(Widget->GetRootWidget()) : nullptr; // ***
	if (!Canvas || DesignSize.X <= 0.0f || DesignSize.Y <= 0.0f)
	{
		return;
	}

	for (UPanelSlot* PanelSlot : Canvas->GetSlots()) // ***
	{
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot);
		UWidget* Child = CanvasSlot ? CanvasSlot->Content : nullptr;
		const FString ChildName = Child ? Child->GetName() : FString();
		if (!Child || ChildName.EndsWith(TEXT("_Timer")) || ChildName.EndsWith(TEXT("_Stats"))) // *** runtime labels place themselves
		{
			continue;
		}

		const FAnchors Anchors = CanvasSlot->GetAnchors();
		const FMargin Offsets = CanvasSlot->GetOffsets();
		const FVector2D AnchorMin = Anchors.Minimum * DesignSize; // *** anchor rect in designer pixels
		const FVector2D AnchorMax = Anchors.Maximum * DesignSize;
		if ((Anchors.Maximum - Anchors.Minimum).GetMin() > 0.9f) // *** already full-screen (the background art)
		{
			continue;
		}

		if (CanvasSlot->GetAutoSize() || Anchors.Minimum != Anchors.Maximum)
		{
			if (Anchors.Minimum != Anchors.Maximum) // *** stretched element: convert its margins into a proportional rect
			{
				const FVector2D TopLeft = AnchorMin + FVector2D(Offsets.Left, Offsets.Top);
				const FVector2D BottomRight = AnchorMax - FVector2D(Offsets.Right, Offsets.Bottom);
				CanvasSlot->SetAnchors(FAnchors(TopLeft.X / DesignSize.X, TopLeft.Y / DesignSize.Y,
					BottomRight.X / DesignSize.X, BottomRight.Y / DesignSize.Y));
				CanvasSlot->SetOffsets(FMargin(0.0f));
				continue;
			}

			// *** Size-to-content element: keep its own size, only pin its alignment point proportionally.
			const FVector2D Pivot = (AnchorMin + FVector2D(Offsets.Left, Offsets.Top)) / DesignSize;
			CanvasSlot->SetAnchors(FAnchors(Pivot.X, Pivot.Y));
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
			continue;
		}

		// *** Fixed-size element: its designer rect becomes proportional anchors, so it scales with the art.
		const FVector2D Size(Offsets.Right, Offsets.Bottom);
		const FVector2D TopLeft = AnchorMin + FVector2D(Offsets.Left, Offsets.Top) - CanvasSlot->GetAlignment() * Size;
		const FVector2D BottomRight = TopLeft + Size;
		CanvasSlot->SetAnchors(FAnchors(TopLeft.X / DesignSize.X, TopLeft.Y / DesignSize.Y,
			BottomRight.X / DesignSize.X, BottomRight.Y / DesignSize.Y));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}
}
