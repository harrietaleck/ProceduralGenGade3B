// EnemyEvolution.h
// The Evolving Blight system. It keeps a record of how every enemy genome did during a wave and, when the wave
// ends, breeds the next generation from the enemies that got furthest.
//
//   NextGenome()    - hands out a genome for each enemy as it spawns.
//   RecordOutcome() - tells it how far that enemy got before it died, or that it reached the tower.
//   EndWave()       - picks the best enemies as parents and prepares the next generation.
//
// This class only deals with genomes. Turning a genome into stats and looks is done by AEnemy::ApplyGenome, and
// deciding when to spawn is done by AWaveManager, so each class keeps one job.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnemyGenome.h"
#include "EnemyEvolution.generated.h"

UCLASS(BlueprintType, DefaultToInstanced, EditInlineNew)
class PROCEDURALGENGADE3B_API UEnemyEvolution : public UObject
{
	GENERATED_BODY()

public:
	// ---- Tuning (editable in the editor, no recompile needed) ----

	/** Biomass shared by Hardiness, Swiftness and Fury at generation 0. 1.5 is an even 0.5 / 0.5 / 0.5 split. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution", meta = (ClampMin = "0.3"))
	float StartingBiomass = 1.5f;

	/** How much the biomass budget grows each generation (0.03 = 3%), so pressure rises gently. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution", meta = (ClampMin = "0.0"))
	float BiomassGrowthPerGeneration = 0.03f;

	/** The budget never grows past this, so enemies cannot become unbeatable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution", meta = (ClampMin = "0.3"))
	float MaxBiomass = 2.1f;

	/** The best enemies of a wave, as a fraction, become parents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ParentFraction = 0.4f;

	/** Chance for each gene of a child to change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MutationChance = 0.25f;

	/** The biggest change a mutation makes to a gene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MutationStrength = 0.15f;

	/** Seeds the random numbers from the map seed. Calling it again with the same seed does nothing. A new seed
	 *  (a new map) starts evolution from scratch. */
	void EnsureSeeded(int32 MapSeed);

	/** Gives out the next genome. Generation 0 is random. Later ones are bred from the last wave's best enemies,
	 *  and the very first one of each generation is the unchanged champion. */
	FEnemyGenome NextGenome();

	/** Records how one enemy did. Progress is how far along its path it got (0 to 1), Leaked means it reached the tower. */
	void RecordOutcome(const FEnemyGenome& Genome, float Progress, bool bLeaked);

	/** Ends the wave: chooses the parents of the next generation and moves on. Does nothing if nothing was recorded. */
	void EndWave();

	UFUNCTION(BlueprintPure, Category = "Evolution")
	int32 GetGeneration() const { return Generation; }

	UFUNCTION(BlueprintPure, Category = "Evolution")
	bool HasChampion() const { return bHasChampion; }

	/** The best enemy of the last finished wave. */
	UFUNCTION(BlueprintPure, Category = "Evolution")
	FEnemyGenome GetChampion() const { return Champion; }

	/** Fitness of the champion, 0 to 1.5. Above 1 means it reached the tower. */
	UFUNCTION(BlueprintPure, Category = "Evolution")
	float GetChampionFitness() const { return ChampionFitness; }

	/** Current biomass budget. */
	UFUNCTION(BlueprintPure, Category = "Evolution")
	float GetBiomassBudget() const;

	/** How well an enemy did. Path progress from 0 to 1, plus 0.5 for reaching the tower. */
	static float ComputeFitness(float Progress, bool bLeaked);

private:
	struct FResult
	{
		FEnemyGenome Genome;
		float Fitness = 0.0f;
	};

	/** Results for the wave in progress. */
	TArray<FResult> WaveResults;

	/** Parents chosen at the end of the last wave. Fewer than 2 means generation 0 (random). */
	TArray<FEnemyGenome> Parents;

	FEnemyGenome Champion;
	float ChampionFitness = 0.0f;
	bool bHasChampion = false;

	/** The champion is handed out once at the start of each generation. */
	bool bChampionIssued = false;

	int32 Generation = 0;
	int32 SeedValue = 0;
	bool bSeeded = false;
	FRandomStream Stream;

	FEnemyGenome MakeRandomGenome();
	FEnemyGenome MakeChild();
	void ResetState();
};
