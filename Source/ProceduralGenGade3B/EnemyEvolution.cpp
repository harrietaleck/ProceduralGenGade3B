// EnemyEvolution.cpp - see EnemyEvolution.h.

#include "EnemyEvolution.h"

float UEnemyEvolution::ComputeFitness(float Progress, bool bLeaked)
{
	return FMath::Clamp(Progress, 0.0f, 1.0f) + (bLeaked ? 0.5f : 0.0f);
}

float UEnemyEvolution::GetBiomassBudget() const
{
	// Compound growth, capped, so the budget rises each generation but never runs away.
	return FMath::Min(MaxBiomass, StartingBiomass * FMath::Pow(1.0f + BiomassGrowthPerGeneration, static_cast<float>(Generation)));
}

void UEnemyEvolution::ResetState()
{
	WaveResults.Reset();
	Parents.Reset();
	Champion = FEnemyGenome();
	ChampionFitness = 0.0f;
	bHasChampion = false;
	bChampionIssued = false;
	Generation = 0;
}

void UEnemyEvolution::EnsureSeeded(int32 MapSeed)
{
	if (bSeeded && SeedValue == MapSeed)
	{
		return;
	}

	// A new map means a new Blight: forget the old lineage and start again from random genomes.
	ResetState();
	SeedValue = MapSeed;
	bSeeded = true;
	Stream.Initialize(MapSeed ^ 0x5bd1e995);
}

FEnemyGenome UEnemyEvolution::MakeRandomGenome()
{
	FEnemyGenome Genome;
	Genome.Hardiness = Stream.FRandRange(0.15f, 0.9f);
	Genome.Swiftness = Stream.FRandRange(0.15f, 0.9f);
	Genome.Fury = Stream.FRandRange(0.15f, 0.9f);
	Genome.Girth = Stream.FRandRange(0.1f, 0.9f);
	Genome.Pulse = Stream.FRandRange(0.1f, 0.9f);
	Genome.Hue = Stream.FRand();
	Genome.Generation = Generation;
	Genome.ClampToBudget(GetBiomassBudget());
	Genome.Name = FEnemyGenome::BuildName(Genome, Stream);
	return Genome;
}

FEnemyGenome UEnemyEvolution::MakeChild()
{
	// Uniform crossover: every gene comes from one parent or the other with equal chance.
	const FEnemyGenome& A = Parents[Stream.RandRange(0, Parents.Num() - 1)];
	const FEnemyGenome& B = Parents[Stream.RandRange(0, Parents.Num() - 1)];
	auto Pick = [this](float FromA, float FromB) { return Stream.FRand() < 0.5f ? FromA : FromB; };

	FEnemyGenome Child;
	Child.Hardiness = Pick(A.Hardiness, B.Hardiness);
	Child.Swiftness = Pick(A.Swiftness, B.Swiftness);
	Child.Fury = Pick(A.Fury, B.Fury);
	Child.Girth = Pick(A.Girth, B.Girth);
	Child.Pulse = Pick(A.Pulse, B.Pulse);
	Child.Hue = Pick(A.Hue, B.Hue);

	// Mutation: each gene has a chance to shift by a small random amount.
	auto Mutate = [this](float& Gene)
	{
		if (Stream.FRand() < MutationChance)
		{
			Gene += Stream.FRandRange(-1.0f, 1.0f) * MutationStrength;
		}
	};
	Mutate(Child.Hardiness);
	Mutate(Child.Swiftness);
	Mutate(Child.Fury);
	Mutate(Child.Girth);
	Mutate(Child.Pulse);
	Mutate(Child.Hue);

	Child.Generation = Generation;
	Child.ClampToBudget(GetBiomassBudget());
	Child.Name = FEnemyGenome::BuildName(Child, Stream);
	return Child;
}

FEnemyGenome UEnemyEvolution::NextGenome()
{
	// Generation 0, or nothing survived to breed from: random genomes.
	if (Parents.Num() < 1)
	{
		return MakeRandomGenome();
	}

	// The first enemy of each generation is the previous champion, unchanged (elitism), so the best
	// design is never lost to a bad mutation.
	if (bHasChampion && !bChampionIssued)
	{
		bChampionIssued = true;
		FEnemyGenome Copy = Champion;
		Copy.Generation = Generation;
		Copy.bChampion = true;
		return Copy;
	}

	return MakeChild();
}

void UEnemyEvolution::RecordOutcome(const FEnemyGenome& Genome, float Progress, bool bLeaked)
{
	FResult Result;
	Result.Genome = Genome;
	Result.Fitness = ComputeFitness(Progress, bLeaked);
	WaveResults.Add(Result);
}

void UEnemyEvolution::EndWave()
{
	if (WaveResults.Num() == 0)
	{
		return;
	}

	WaveResults.Sort([](const FResult& A, const FResult& B) { return A.Fitness > B.Fitness; });

	// The top slice become parents. At least two when we have them, so there is something to cross.
	const int32 Wanted = FMath::CeilToInt(WaveResults.Num() * ParentFraction);
	const int32 ParentCount = FMath::Clamp(FMath::Max(2, Wanted), 1, WaveResults.Num());
	Parents.Reset();
	for (int32 I = 0; I < ParentCount; ++I)
	{
		Parents.Add(WaveResults[I].Genome);
	}

	Champion = WaveResults[0].Genome;
	Champion.bChampion = false;
	ChampionFitness = WaveResults[0].Fitness;
	bHasChampion = true;
	bChampionIssued = false;

	UE_LOG(LogTemp, Display,
		TEXT("EnemyEvolution: generation %d ended with %d enemies. Champion %s (fitness %.2f: hardiness %.2f, swiftness %.2f, fury %.2f, girth %.2f). %d parents."),
		Generation, WaveResults.Num(), *Champion.Name, ChampionFitness,
		Champion.Hardiness, Champion.Swiftness, Champion.Fury, Champion.Girth, ParentCount);

	WaveResults.Reset();
	++Generation;
}
