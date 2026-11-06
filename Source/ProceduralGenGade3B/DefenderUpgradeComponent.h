// DefenderUpgradeComponent.h
// Temporary upgrades bought from the Defenders screen remain active for one wave.
// Stats are raised towards fixed target values and the exact amounts added are undone at wave end,
// so permanent upgrades made in the meantime are kept.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Styling/SlateTypes.h"
#include "DefenderUpgradeComponent.generated.h"

class ADefender;
class ATDGameMode;
class UButton;
class UTextBlock;
class UUserWidget;

namespace DefenderUpgrades
{
	enum class EUpgrade : uint8 { Basic, Archer, Poison, Strong, Tower, Count };
	enum class EPhase : uint8 { Ready, Active, DisabledAfterWave };

	/** What one activation added to one actor, so the same amounts can be taken off again. */
	struct FBoost
	{
		TWeakObjectPtr<AActor> Target;
		float Health = 0.0f;
		float Interval = 0.0f;
		float Damage = 0.0f;
		float ProjectileSpeed = 0.0f;
	};

	struct FSlot
	{
		struct FTextColor
		{
			TWeakObjectPtr<UTextBlock> Widget;
			FSlateColor OriginalColor;
		};

		EPhase Phase = EPhase::Ready;
		int32 Tier = 0; // 0 = nothing bought this wave, 1 = tier I active, 2 = tier II active
		TArray<FBoost> Boosts;
		int32 CooldownSecondsRemaining = 0;
		FTimerHandle CooldownTimer;
		TWeakObjectPtr<UButton> Button;
		FButtonStyle OriginalButtonStyle;
		TArray<FTextColor> OriginalTextColors;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UTextBlock> StatsLabel; //  current and upgraded stats shown under the button
	};
}

UCLASS(ClassGroup = (TowerDefense))
class PROCEDURALGENGADE3B_API UDefenderUpgradeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDefenderUpgradeComponent();

	/** Wires the upgrade buttons of an open Defenders screen. Returns quickly when already bound. */
	void BindOpenScreen();

	/** Opens or closes the Defenders screen during a match. */
	void ToggleScreen();

	/** Gives a newly placed defender any upgrade that is active for its type. */
	void HandleDefenderSpawned(ADefender* Defender);

	/** Refreshes upgrade button colors and status text from the current upgrade states. */
	void RefreshUpgradeUI();

	/** Makes upgrades available when the next wave begins. */
	UFUNCTION()
	void HandleWaveStarted(int32 WaveNumber);

	/** Legacy save compatibility: wave-scoped upgrades are not persisted between waves. */
	void GetSaveState(TArray<uint8>& OutPhases, TArray<float>& OutSecondsLeft) const;

	/** Legacy save compatibility: old timed upgrade states are intentionally not restored. */
	void RestoreSaveState(const TArray<uint8>& Phases, const TArray<float>& SecondsLeft);

	/**  Starts any already-paid (queued) upgrade whose unit is now on the field, e.g. the Tower at match start. */
	void ActivatePendingUpgrades();

	/** Ends all wave-scoped upgrades and clears paid upgrades still waiting for a matching defender. */
	void ResetTemporaryUpgradesForWaveEnd();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	DefenderUpgrades::FSlot Slots[(int32)DefenderUpgrades::EUpgrade::Count];
	TWeakObjectPtr<UUserWidget> BoundScreen;
	UFUNCTION()
	void OnBasicClicked();

	UFUNCTION()
	void OnArcherClicked();

	UFUNCTION()
	void OnPoisonClicked();

	UFUNCTION()
	void OnStrongClicked();

	UFUNCTION()
	void OnTowerClicked();

	UFUNCTION()
	void HandleMetaCurrencyChanged();

	UFUNCTION()
	void CloseScreen();

	void BindScreen(UUserWidget* Screen);
	void ClearScreenBindings();
	//  One helper creates the status and optional stats labels on a button.
	UTextBlock* CreateButtonLabel(UUserWidget* Screen, UButton* Button, const TCHAR* Suffix, int32 FontSize,
		FVector2D Pivot, FVector2D Alignment, FVector2D PixelOffset) const;
	FString DescribeStats(DefenderUpgrades::EUpgrade Upgrade) const; //  live stats, or class defaults if none spawned
	void TryActivate(DefenderUpgrades::EUpgrade Upgrade);
	void StartCooldown(DefenderUpgrades::EUpgrade Upgrade);
	void TickUpgradeCooldown(int32 UpgradeIndex);
	void ApplyBoost(DefenderUpgrades::EUpgrade Upgrade, AActor* Target);
	void RemoveBoosts(DefenderUpgrades::EUpgrade Upgrade);
	void GatherTargets(DefenderUpgrades::EUpgrade Upgrade, TArray<AActor*>& OutTargets) const;
	bool CanAfford(DefenderUpgrades::EUpgrade Upgrade) const;
	bool Spend(DefenderUpgrades::EUpgrade Upgrade) const;
	ATDGameMode* GetGameMode() const;

	//  Queued purchases and status text (DefenderUpgradeComponent_Extra.inl).
	void StartActive(DefenderUpgrades::EUpgrade Upgrade, const TArray<AActor*>& Targets);
	/** Buys the second tier: raises every living unit of that type to the tier II stats. */
	void StartTierTwo(DefenderUpgrades::EUpgrade Upgrade, const TArray<AActor*>& Targets);
	bool IsPending(DefenderUpgrades::EUpgrade Upgrade) const;
	void UpdateStatusText();
};
