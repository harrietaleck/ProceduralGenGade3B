// DangerScreenEffectComponent.cpp - see DangerScreenEffectComponent.h.

#include "DangerScreenEffectComponent.h"
#include "HealthComponent.h"
#include "TDGameMode.h"
#include "Tower.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UDangerScreenEffectComponent::UDangerScreenEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	EffectMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Materials/PostProcess/PP_TowerDanger.PP_TowerDanger")));
}

void UDangerScreenEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	UMaterialInterface* Base = EffectMaterial.LoadSynchronous();
	if (!World || !Base)
	{
		UE_LOG(LogTemp, Warning, TEXT("DangerScreenEffect: post-process material could not be loaded, effect disabled."));
		SetComponentTickEnabled(false);
		return;
	}

	Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetScalarParameterValue(TEXT("Intensity"), 0.0f);

	// An unbound volume affects the whole level, so it works with whichever camera the player is using.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Volume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, SpawnParams);
	if (Volume)
	{
		Volume->bUnbound = true;
		Volume->BlendWeight = 1.0f;
		Volume->Priority = 100.0f; // above any other volume so the warning is never hidden
		Volume->Settings.AddBlendable(Material, 1.0f);
	}
}

void UDangerScreenEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Volume)
	{
		Volume->Destroy();
		Volume = nullptr;
	}
	Material = nullptr;
	Super::EndPlay(EndPlayReason);
}

float UDangerScreenEffectComponent::ComputeTargetIntensity(float DeltaTime)
{
	const ATDGameMode* GameMode = Cast<ATDGameMode>(GetOwner());
	if (!GameMode || GameMode->IsVictory())
	{
		LastHealthPercent = 1.0f;
		HitFlashRemaining = 0.0f;
		return 0.0f;
	}

	const ATower* Tower = GameMode->GetTower();
	const UHealthComponent* Health = Tower ? Tower->HealthComponent.Get() : nullptr;
	if (!Health)
	{
		LastHealthPercent = 1.0f;
		return 0.0f;
	}

	const float Percent = Health->GetHealthPercent();

	// A drop in health since last frame means the tower was just hit, so flash.
	if (Percent < LastHealthPercent - KINDA_SMALL_NUMBER)
	{
		HitFlashRemaining = FMath::Min(1.0f, HitFlashRemaining + HitFlash);
	}
	LastHealthPercent = Percent;
	HitFlashRemaining = FMath::Max(0.0f, HitFlashRemaining - HitFlashFadePerSecond * DeltaTime);

	// Danger rises from 0 at the threshold to 1 at zero health. A dead tower is always full strength.
	float Danger = FMath::Clamp((DangerStartsBelow - Percent) / DangerStartsBelow, 0.0f, 1.0f);
	if (Health->IsDead())
	{
		Danger = 1.0f;
	}

	return FMath::Clamp(Danger + HitFlashRemaining, 0.0f, 1.0f);
}

void UDangerScreenEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Material)
	{
		return;
	}

	const float Target = ComputeTargetIntensity(DeltaTime);
	CurrentIntensity = FMath::FInterpTo(CurrentIntensity, Target, DeltaTime, ResponseSpeed);
	if (CurrentIntensity < 0.002f && Target <= 0.0f)
	{
		CurrentIntensity = 0.0f; // fully off, so nothing is drawn while the tower is healthy
	}
	Material->SetScalarParameterValue(TEXT("Intensity"), CurrentIntensity);

	// Log the first time the warning becomes visible, which makes it easy to confirm the effect ran.
	if (!bLoggedVisible && CurrentIntensity > 0.05f)
	{
		bLoggedVisible = true;
		UE_LOG(LogTemp, Display, TEXT("DangerScreenEffect: warning visible (intensity %.2f)."), CurrentIntensity);
	}
}
