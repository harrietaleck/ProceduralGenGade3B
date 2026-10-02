// StrongDefender.h
// A stronger defender that also costs Gem Stones. It fires a bolt that stuns its target,
// which holds enemies still so the other defenders can finish them off.

#pragma once

#include "CoreMinimal.h"
#include "Defender.h"
#include "StrongDefender.generated.h"

UCLASS()
class PROCEDURALGENGADE3B_API AStrongDefender : public ADefender
{
	GENERATED_BODY()

public:
	AStrongDefender();

	/** How many seconds each bolt stuns its target. Bears only get stunned for half as long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strong Defender", meta = (ClampMin = "0.0"))
	float StunDuration = 0.9f;

	virtual float GetThreatRating() const override;

protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle BoltTimerHandle;

	/** Picks the enemy in range with the most health and hits it, since that gets the most out of a stun. */
	void FireStunBolt();
};
