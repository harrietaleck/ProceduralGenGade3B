// StrongDefender.cpp — see StrongDefender.h

#include "StrongDefender.h"
#include "HealthComponent.h"

AStrongDefender::AStrongDefender()
{
	bStrongDefender = true;
	Cost = 75;
	AttackDamage = 14.0f;
	AttackRange = 750.0f;
	FireInterval = 0.55f;
	HealthComponent->MaxHealth = 180.0f;

	MetaCost.ForestEssence = 12;
	MetaCost.WoodenMight = 8;
	MetaCost.GemStones = 15;
	MetaCost.LightLanterns = 0;

	DefenderBallColor = FLinearColor(0.55f, 0.35f, 1.0f);
	MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.4f));
}
