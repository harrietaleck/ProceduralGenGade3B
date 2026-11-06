// EnemyGenome.h
// The genes of one enemy in the Evolving Blight system. A genome decides how strong the enemy is, how big it
// is, what colour it is and how its body ripples. UEnemyEvolution breeds genomes from wave to wave.
//
// The three "power" genes (Hardiness, Swiftness, Fury) share a biomass budget. A genome can only be as strong
// as the budget allows, so getting stronger in one gene means getting weaker in another. That is what makes
// evolution produce tanks, glass cannons and speedsters instead of simply making every enemy better.

#pragma once

#include "CoreMinimal.h"
#include "EnemyGenome.generated.h"

USTRUCT(BlueprintType)
struct FEnemyGenome
{
	GENERATED_BODY()

	/** Health. A high value makes a slow tank. */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	float Hardiness = 0.5f;

	/** Movement speed. A high value outruns slow single-target defenders. */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	float Swiftness = 0.5f;

	/** Attack damage. */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	float Fury = 0.5f;

	/** Body size. This is only for looks and for how big a target the enemy is. It does not use biomass. */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	float Girth = 0.5f;

	/** How violently the body ripples (the amplitude of the vertex displacement shader). Looks only. */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	float Pulse = 0.5f;

	/** Colour shift around the enemy type's own colour. Looks only. */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	float Hue = 0.5f;

	/** Which generation this genome was born in. Generation 0 is random. */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	int32 Generation = 0;

	/** Generated name, for example "Skitrot the Swift". */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	FString Name;

	/** True for the unchanged copy of the last wave's best enemy. */
	UPROPERTY(BlueprintReadOnly, Category = "Genome")
	bool bChampion = false;

	// ---- What the genes mean. Every multiplier is 1.0 when its gene is 0.5 (an even split of the budget). ----

	float HealthMultiplier() const { return 0.5f + Hardiness; }
	float SpeedMultiplier() const { return 0.6f + 0.8f * Swiftness; }
	float DamageMultiplier() const { return 0.5f + Fury; }
	float SizeScale() const { return 0.75f + 0.5f * Girth; }

	/** Total of the three power genes. This is the number the biomass budget limits. */
	float BiomassUsed() const { return Hardiness + Swiftness + Fury; }

	/** Overall strength, 1.0 for an even split of the starting budget. Stronger genomes drop more loot. */
	float PowerRating() const { return BiomassUsed() / 1.5f; }

	/** Keeps every gene in range and scales the three power genes down if they use more than Budget. */
	void ClampToBudget(float Budget);

	/** Builds a name from the dominant genes, for example "Skitrot the Swift". */
	static FString BuildName(const FEnemyGenome& Genome, FRandomStream& Stream);
};
