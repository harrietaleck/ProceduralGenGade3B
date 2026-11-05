// TDVFX.h
// One place that plays the game's Niagara effects. Each class keeps its effect as an editable soft
// reference (set to a /Game/VFX default in its constructor), so effects load on first use, can be
// swapped in the editor without changing C++, and a missing asset simply plays nothing.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace TDVFX
{
	/** Shared effect colours, matching the forest palette. */
	inline const FLinearColor Gold(1.0f, 0.78f, 0.2f);
	inline const FLinearColor Turquoise(0.2f, 1.0f, 0.85f);
	inline const FLinearColor Green(0.45f, 1.0f, 0.35f);
	inline const FLinearColor Corruption(0.55f, 0.12f, 0.9f);

	/** "/Game/VFX/Towers/NS_TowerFire" -> soft reference to that Niagara system. */
	inline TSoftObjectPtr<UNiagaraSystem> Asset(const TCHAR* PackagePath)
	{
		const FString Path(PackagePath);
		return TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Path + TEXT(".") + FPaths::GetBaseFilename(Path)));
	}

	/**
	 * Plays an effect at a location, or attached to a component when AttachTo is set. One-shot
	 * effects use Niagara's component pool. Lifetime > 0 stops looping effects after that many
	 * seconds. Color is written to an optional "User.Color" parameter on the system.
	 */
	inline UNiagaraComponent* Play(const UObject* WorldContext, const TSoftObjectPtr<UNiagaraSystem>& System,
		const FVector& Location, const FLinearColor& Color, float Lifetime = 0.0f, USceneComponent* AttachTo = nullptr)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World || System.IsNull() || World->GetNetMode() == NM_DedicatedServer)
		{
			return nullptr;
		}

		UNiagaraSystem* Loaded = System.LoadSynchronous();
		if (!Loaded)
		{
			return nullptr;
		}

		UNiagaraComponent* Component = AttachTo
			? UNiagaraFunctionLibrary::SpawnSystemAttached(Loaded, AttachTo, NAME_None, FVector::ZeroVector,
				FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true)
			: UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Loaded, Location, FRotator::ZeroRotator,
				FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
		if (!Component)
		{
			return nullptr;
		}

		Component->SetVariableLinearColor(TEXT("User.Color"), Color);
		if (Lifetime > 0.0f)
		{
			FTimerHandle StopHandle;
			World->GetTimerManager().SetTimer(StopHandle,
				FTimerDelegate::CreateWeakLambda(Component, [Component]() { Component->Deactivate(); }), Lifetime, false);
		}
		return Component;
	}
}
