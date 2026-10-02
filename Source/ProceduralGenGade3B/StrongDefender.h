// StrongDefender.h — elite defender that costs Gem Stones in addition to forest resources.
// Instead of lobbing balls it fires a heavy arcane bolt that stuns its target, holding
// enemies in place so the rest of the defence can finish them.

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

	/** Seconds each bolt stuns its target (bears resist half). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strong Defender", meta = (ClampMin = "0.0"))
	float StunDuration = 0.9f;

	virtual float GetThreatRating() const override;

protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle BoltTimerHandle;

	/** Picks the healthiest enemy in range (best use of a stun) and strikes it. */
	void FireStunBolt();
};
