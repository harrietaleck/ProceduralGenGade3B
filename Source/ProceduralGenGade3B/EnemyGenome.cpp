// EnemyGenome.cpp - see EnemyGenome.h.

#include "EnemyGenome.h"

namespace
{
	// A gene never reaches exactly 0, so no multiplier becomes extreme.
	constexpr float MinGene = 0.05f;
	constexpr float MaxGene = 1.0f;
}

// Keeps every gene in range and scales the three power genes down if they use more than Budget.
void FEnemyGenome::ClampToBudget(float Budget)
{
	Hardiness = FMath::Clamp(Hardiness, MinGene, MaxGene);
	Swiftness = FMath::Clamp(Swiftness, MinGene, MaxGene);
	Fury = FMath::Clamp(Fury, MinGene, MaxGene);
	Girth = FMath::Clamp(Girth, MinGene, MaxGene);
	Pulse = FMath::Clamp(Pulse, MinGene, MaxGene);
	Hue = FMath::Clamp(Hue, 0.0f, 1.0f);

	// Over budget: shrink all three power genes by the same factor, so the shape of the build stays the same
	// (a tank stays a tank) but it fits the budget.
	const float Used = BiomassUsed();
	if (Used > Budget && Used > KINDA_SMALL_NUMBER)
	{
		const float Scale = Budget / Used;
		Hardiness = FMath::Max(MinGene, Hardiness * Scale);
		Swiftness = FMath::Max(MinGene, Swiftness * Scale);
		Fury = FMath::Max(MinGene, Fury * Scale);
	}
}

FString FEnemyGenome::BuildName(const FEnemyGenome& Genome, FRandomStream& Stream)
{
	// The four "shape" genes can name a creature. The strongest one gives the first syllable and the second
	// strongest gives the epithet, so the name tells the player what kind of mutant this is.
	enum { Tough, Swift, Furious, Bloated, Count };
	const float Values[Count] = { Genome.Hardiness, Genome.Swiftness, Genome.Fury, Genome.Girth };

	int32 First = 0;
	for (int32 I = 1; I < Count; ++I)
	{
		if (Values[I] > Values[First]) { First = I; }
	}
	int32 Second = First == 0 ? 1 : 0;
	for (int32 I = 0; I < Count; ++I)
	{
		if (I != First && Values[I] > Values[Second]) { Second = I; }
	}

	static const TCHAR* Starts[Count][3] = {
		{ TEXT("Grub"), TEXT("Bulk"), TEXT("Mold") },    // tough
		{ TEXT("Skit"), TEXT("Zip"),  TEXT("Flit") },    // swift
		{ TEXT("Rend"), TEXT("Snarl"), TEXT("Gnash") },  // furious
		{ TEXT("Gob"),  TEXT("Plump"), TEXT("Murk") },   // bloated
	};
	static const TCHAR* Ends[] = { TEXT("rot"), TEXT("fang"), TEXT("maw"), TEXT("back"), TEXT("gnaw"), TEXT("spore"), TEXT("claw") };
	static const TCHAR* Epithets[Count] = { TEXT("the Steady"), TEXT("the Swift"), TEXT("the Furious"), TEXT("the Bloated") };

	return FString::Printf(TEXT("%s%s %s"),
		Starts[First][Stream.RandRange(0, 2)],
		Ends[Stream.RandRange(0, UE_ARRAY_COUNT(Ends) - 1)],
		Epithets[Second]);
}
