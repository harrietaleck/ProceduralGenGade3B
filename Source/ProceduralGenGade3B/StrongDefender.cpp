// StrongDefender.cpp
// The strong defender that stuns enemies. See StrongDefender.h for more.

#include "StrongDefender.h"
#include "Enemy.h"
#include "HealthComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AStrongDefender::AStrongDefender()
{
	bStrongDefender = true;
	bUseDefaultAttack = false;
	DefenderName = TEXT("Strong");
	Cost = 75;
	AttackDamage = 14.0f;
	AttackRange = 750.0f;
	FireInterval = 1.1f;
	HealthComponent->MaxHealth = 180.0f;

	MetaCost.ForestEssence = 12;
	MetaCost.WoodenMight = 8;
	MetaCost.GemStones = 10;
	MetaCost.LightLanterns = 0;

	DefenderBallColor = FLinearColor(0.55f, 0.35f, 1.0f);
	BodyColor = FLinearColor(0.5f, 0.3f, 0.95f);
	MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.4f));
}

float AStrongDefender::GetThreatRating() const
{
	// The stun keeps enemies in range of the other defenders for longer, so it counts for more than just damage.
	return Super::GetThreatRating() * 1.5f;
}

void AStrongDefender::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(BoltTimerHandle, this, &AStrongDefender::FireStunBolt, FireInterval, true);
}

void AStrongDefender::FireStunBolt()
{
	if (!HealthComponent || HealthComponent->IsDead())
	{
		return;
	}

	const FVector Location = GetActorLocation();
	const float RangeSq = AttackRange * AttackRange;
	AEnemy* Best = nullptr;
	float BestHealth = -1.0f;

	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		AEnemy* Enemy = *It;
		UHealthComponent* EnemyHealth = Enemy ? Enemy->HealthComponent.Get() : nullptr;
		if (!EnemyHealth || EnemyHealth->IsDead() || Enemy->IsStunned())
		{
			continue;
		}

		if (FVector::DistSquared(Location, Enemy->GetActorLocation()) > RangeSq)
		{
			continue;
		}

		if (EnemyHealth->GetCurrentHealth() > BestHealth)
		{
			BestHealth = EnemyHealth->GetCurrentHealth();
			Best = Enemy;
		}
	}

	if (!Best)
	{
		return;
	}

	const FVector Muzzle = Location + MuzzleOffset;
	DrawDebugLine(GetWorld(), Muzzle, Best->GetActorLocation(), DefenderBallColor.ToFColor(true), false, 0.15f, 0, 10.0f);
	DrawDebugSphere(GetWorld(), Best->GetActorLocation(), 60.0f, 12, DefenderBallColor.ToFColor(true), false, StunDuration, 0, 3.0f);

	Best->ApplyStun(StunDuration);
	Best->HealthComponent->ApplyDamage(AttackDamage, this);
}
