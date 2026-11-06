// StrongDefender.cpp
// The strong defender that stuns enemies. See StrongDefender.h for more.

#include "StrongDefender.h"
#include "Enemy.h"
#include "HealthComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Projectile.h" // *** visual spear
#include "TDVFX.h"      // ***

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

	DefenderBallColor = FLinearColor(1.0f, 0.8f, 0.2f); // *** golden spear (was purple)

	// *** Magical spear: a larger long cone (about 112 x 16 units) with a stronger impact burst.
	ProjectileShape = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cone.Cone")));
	ProjectileShapeScale = FVector(0.45f, 0.45f, 3.2f);
	ProjectileImpactVFX = TDVFX::Asset(TEXT("/Game/VFX/Enemies/NS_EnemyDeath"));
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

void AStrongDefender::SetAttackInterval(float NewInterval)
{
	Super::SetAttackInterval(NewInterval);
	if (GetWorldTimerManager().IsTimerActive(BoltTimerHandle))
	{
		GetWorldTimerManager().SetTimer(BoltTimerHandle, this, &AStrongDefender::FireStunBolt, FireInterval, true);
	}
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
	PlayFireVFX(Muzzle); // *** stun bolt flash

	// *** Visual-only spear that flies to the target (0 damage). The stun and damage below stay
	// *** instant, exactly as before, so timing, damage and targeting are unchanged.
	if (ProjectileClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = this;
		if (AProjectile* Spear = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, Muzzle, GetActorRotation(), SpawnParams))
		{
			Spear->InitProjectile(Best, 0.0f, this);
			StyleProjectile(Spear);
		}
	}
	DrawDebugLine(GetWorld(), Muzzle, Best->GetActorLocation(), DefenderBallColor.ToFColor(true), false, 0.15f, 0, 10.0f);
	DrawDebugSphere(GetWorld(), Best->GetActorLocation(), 60.0f, 12, DefenderBallColor.ToFColor(true), false, StunDuration, 0, 3.0f);

	Best->ApplyStun(StunDuration);
	Best->HealthComponent->ApplyDamage(AttackDamage, this);
}
