// DangerScreenEffectComponent.h
// A screen-space post-process that tells the player the tower is in trouble.
// It drives the PP_TowerDanger material (Content/Materials/PostProcess), which drains the colour out of the
// picture and pulses a red vignette around the screen edges like a heartbeat.
//   * Below DangerStartsBelow of tower health the effect fades in, reaching full strength at zero health.
//   * Every time the tower takes damage it flashes briefly, even when its health is still high.
//   * It switches off on victory.
// This only reads the tower's health every frame, so it keeps working when the tower is replaced on restart.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DangerScreenEffectComponent.generated.h"

class APostProcessVolume;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UCLASS(ClassGroup = (TowerDefense), meta = (BlueprintSpawnableComponent))
class PROCEDURALGENGADE3B_API UDangerScreenEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDangerScreenEffectComponent();

	/** Post-process material to apply. It must have a scalar parameter named "Intensity". */
	UPROPERTY(EditAnywhere, Category = "Danger Effect")
	TSoftObjectPtr<UMaterialInterface> EffectMaterial;

	/** The effect starts when tower health falls below this fraction (0.45 = below 45%). */
	UPROPERTY(EditAnywhere, Category = "Danger Effect", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float DangerStartsBelow = 0.45f;

	/** How much intensity each hit on the tower adds for a moment. */
	UPROPERTY(EditAnywhere, Category = "Danger Effect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitFlash = 0.3f;

	/** How quickly a hit flash fades, in intensity per second. */
	UPROPERTY(EditAnywhere, Category = "Danger Effect", meta = (ClampMin = "0.1"))
	float HitFlashFadePerSecond = 1.2f;

	/** How quickly the picture follows the target intensity. Higher is snappier. */
	UPROPERTY(EditAnywhere, Category = "Danger Effect", meta = (ClampMin = "0.5"))
	float ResponseSpeed = 5.0f;

	/** Strength currently on screen, 0 to 1. Exposed for Blueprints and debugging. */
	UFUNCTION(BlueprintPure, Category = "Danger Effect")
	float GetCurrentIntensity() const { return CurrentIntensity; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Unbound volume that applies the material to every camera in the level. */
	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> Volume;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	float CurrentIntensity = 0.0f;
	float HitFlashRemaining = 0.0f;
	float LastHealthPercent = 1.0f;
	bool bLoggedVisible = false;

	/** Where the intensity should be this frame, from tower health, recent hits and the match state. */
	float ComputeTargetIntensity(float DeltaTime);
};
