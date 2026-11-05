// DefenderUpgradeComponent.h
// Temporary upgrades bought from the Defenders screen. Each of the five upgrades (Basic, Archer,
// Poison, Strong Guardian, Tower) runs Ready -> Active -> Cooldown -> Ready on its own timer.
// Stats are raised towards fixed target values and the exact amounts added are undone on expiry,
// so permanent upgrades made in the meantime are kept.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "DefenderUpgradeComponent.generated.h"

class ADefender;
class ATDGameMode;
class UButton;
class UTextBlock;
class UUserWidget;

namespace DefenderUpgrades
{
	enum class EUpgrade : uint8 { Basic, Archer, Poison, Strong, Tower, Count };
	enum class EPhase : uint8 { Ready, Active, Cooldown };

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
		EPhase Phase = EPhase::Ready;
		FTimerHandle PhaseTimer;
		TArray<FBoost> Boosts;
		TWeakObjectPtr<UButton> Button;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UTextBlock> StatsLabel; // *** current and upgraded stats shown under the button
		double NoTargetUntil = 0.0;            // *** world time until which "No units" is shown
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

	/** Phase (0 Ready, 1 Active, 2 Cooldown) and seconds left of each upgrade, for save games. */
	void GetSaveState(TArray<uint8>& OutPhases, TArray<float>& OutSecondsLeft) const;

	/** Restores saved phases and timers; active boosts are re-applied without charging again. */
	void RestoreSaveState(const TArray<uint8>& Phases, const TArray<float>& SecondsLeft);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	DefenderUpgrades::FSlot Slots[(int32)DefenderUpgrades::EUpgrade::Count];
	TWeakObjectPtr<UUserWidget> BoundScreen;
	FTimerHandle RefreshTimer;

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
	void CloseScreen();

	void BindScreen(UUserWidget* Screen);
	// *** One helper now creates both the countdown label and the stats label on a button.
	UTextBlock* CreateButtonLabel(UUserWidget* Screen, UButton* Button, const TCHAR* Suffix, int32 FontSize,
		FVector2D Pivot, FVector2D Alignment, FVector2D PixelOffset) const;
	FString DescribeStats(DefenderUpgrades::EUpgrade Upgrade) const; // *** live stats, or class defaults if none spawned
	void TryActivate(DefenderUpgrades::EUpgrade Upgrade);
	void EndActive(DefenderUpgrades::EUpgrade Upgrade);
	void EndCooldown(DefenderUpgrades::EUpgrade Upgrade);
	void ApplyBoost(DefenderUpgrades::EUpgrade Upgrade, AActor* Target);
	void RemoveBoosts(DefenderUpgrades::EUpgrade Upgrade);
	void RefreshButtons();
	void GatherTargets(DefenderUpgrades::EUpgrade Upgrade, TArray<AActor*>& OutTargets) const;
	bool CanAfford(DefenderUpgrades::EUpgrade Upgrade) const;
	bool Spend(DefenderUpgrades::EUpgrade Upgrade) const;
	ATDGameMode* GetGameMode() const;
};
