// BuildPadMarker.cpp — see BuildPadMarker.h for the overview.

#include "BuildPadMarker.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

void UBuildPadPortalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	PortalImage = Cast<UImage>(WidgetTree->RootWidget);
	if (!PortalImage)
	{
		PortalImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortalImage"));
		WidgetTree->RootWidget = PortalImage;
	}

	PortalImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (PendingTexture)
	{
		PortalImage->SetBrushFromTexture(PendingTexture, true);
	}
}

void UBuildPadPortalWidget::SetPortalTexture(UTexture2D* Texture)
{
	PendingTexture = Texture;
	if (PortalImage && Texture)
	{
		PortalImage->SetBrushFromTexture(Texture, true);
	}
}

ABuildPadMarker::ABuildPadMarker()
{
	PrimaryActorTick.bCanEverTick = false; // Purely decorative; no per-frame logic of its own.

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(SceneRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PlatformMesh->SetStaticMesh(CylinderMesh.Object);
	}

	// Purely visual: never blocks clicks, movement, or the build-slot raycast.
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PortalWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DefendersPortal"));
	PortalWidgetComponent->SetupAttachment(SceneRoot);
	PortalWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	PortalWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
	PortalWidgetComponent->SetTwoSided(true);
	PortalWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortalWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	PortalWidgetComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	PortalWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));

	PortalTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/UI/SourceArt/DefendersPortal.DefendersPortal")));
}

void ABuildPadMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The basic cylinder is ~100uu across by default; keep it as a missing-art fallback.
	PlatformMesh->SetRelativeScale3D(
		FVector(Radius / 50.0f, Radius / 50.0f, Thickness / 100.0f));

	const int32 PortalPixels = FMath::Max(64, FMath::RoundToInt(Radius * 2.0f));
	PortalWidgetComponent->SetDrawSize(FVector2D(PortalPixels, PortalPixels));
}

void ABuildPadMarker::BeginPlay()
{
	Super::BeginPlay();

	UTexture2D* Texture = PortalTexture.LoadSynchronous();
	if (!Texture)
	{
		PortalWidgetComponent->SetVisibility(false);
		PlatformMesh->SetVisibility(true);
		UE_LOG(LogTemp, Warning,
			TEXT("BuildPadMarker: save/import DefendersPortal at /Game/UI/SourceArt/DefendersPortal."));
		return;
	}

	if (UBuildPadPortalWidget* PortalWidget =
		CreateWidget<UBuildPadPortalWidget>(GetWorld(), UBuildPadPortalWidget::StaticClass()))
	{
		PortalWidget->SetPortalTexture(Texture);
		PortalWidgetComponent->SetWidget(PortalWidget);
		PortalWidgetComponent->SetVisibility(true);
		PlatformMesh->SetVisibility(false);
	}
}
