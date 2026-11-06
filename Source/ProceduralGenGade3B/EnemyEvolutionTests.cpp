// EnemyEvolutionTests.cpp
// Automated tests for the Evolving Blight system. They need no map and no player, so they can run many
// generations in a moment and prove the rules hold: the biomass budget is respected, the same seed gives the
// same Blight, evolution really does climb towards what survives, and the champion is never lost.
//
// Run them from the editor (Tools > Session Frontend > Automation, filter "ProceduralGen") or from the command line:
//   UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests ProceduralGen.EnemyEvolution; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "EnemyEvolution.h"
#include "EnemyGenome.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A pretend wave: records an outcome for every enemy using the given fitness function, then ends the wave. */
	template <typename TFitnessFn>
	float RunWave(UEnemyEvolution* Evolution, int32 EnemyCount, TFitnessFn ProgressOf, TArray<FEnemyGenome>* OutGenomes = nullptr)
	{
		TArray<FEnemyGenome> Genomes;
		for (int32 I = 0; I < EnemyCount; ++I)
		{
			Genomes.Add(Evolution->NextGenome());
		}
		float Total = 0.0f;
		for (const FEnemyGenome& Genome : Genomes)
		{
			const float Progress = ProgressOf(Genome);
			Evolution->RecordOutcome(Genome, Progress, Progress >= 0.95f);
			Total += Progress;
		}
		Evolution->EndWave();
		if (OutGenomes)
		{
			*OutGenomes = Genomes;
		}
		return Total / EnemyCount;
	}

	float MeanSwiftness(const TArray<FEnemyGenome>& Genomes)
	{
		float Sum = 0.0f;
		for (const FEnemyGenome& Genome : Genomes) { Sum += Genome.Swiftness; }
		return Genomes.Num() > 0 ? Sum / Genomes.Num() : 0.0f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyEvolutionBudgetTest, "ProceduralGen.EnemyEvolution.GenomesRespectTheBiomassBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEnemyEvolutionBudgetTest::RunTest(const FString& Parameters)
{
	// Many seeds and many generations, with a fitness that rewards raw strength, which is the worst case for the budget.
	for (int32 Seed = 1; Seed <= 25; ++Seed)
	{
		UEnemyEvolution* Evolution = NewObject<UEnemyEvolution>();
		Evolution->EnsureSeeded(Seed);

		for (int32 Wave = 0; Wave < 12; ++Wave)
		{
			const float Budget = Evolution->GetBiomassBudget();
			TArray<FEnemyGenome> Genomes;
			RunWave(Evolution, 14, [](const FEnemyGenome& G) { return FMath::Clamp(G.PowerRating() * 0.7f, 0.0f, 1.0f); }, &Genomes);

			for (const FEnemyGenome& G : Genomes)
			{
				TestTrue(TEXT("power genes stay inside the biomass budget"), G.BiomassUsed() <= Budget + 0.001f);
				TestTrue(TEXT("every gene stays between 0.05 and 1"),
					FMath::Min3(G.Hardiness, G.Swiftness, G.Fury) >= 0.05f - KINDA_SMALL_NUMBER
					&& FMath::Max3(G.Hardiness, G.Swiftness, G.Fury) <= 1.0f + KINDA_SMALL_NUMBER
					&& G.Girth >= 0.05f - KINDA_SMALL_NUMBER && G.Girth <= 1.0f + KINDA_SMALL_NUMBER);
				TestFalse(TEXT("every genome has a name"), G.Name.IsEmpty());
			}
		}
		TestTrue(TEXT("the budget never passes its cap"), Evolution->GetBiomassBudget() <= Evolution->MaxBiomass + KINDA_SMALL_NUMBER);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyEvolutionSeedTest, "ProceduralGen.EnemyEvolution.SameSeedGivesTheSameBlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Runs this automated test and reports any failure.
bool FEnemyEvolutionSeedTest::RunTest(const FString& Parameters)
{
	auto Sequence = [](int32 Seed)
	{
		UEnemyEvolution* Evolution = NewObject<UEnemyEvolution>();
		Evolution->EnsureSeeded(Seed);
		TArray<FString> Names;
		for (int32 Wave = 0; Wave < 4; ++Wave)
		{
			TArray<FEnemyGenome> Genomes;
			RunWave(Evolution, 8, [](const FEnemyGenome& G) { return G.Swiftness; }, &Genomes);
			for (const FEnemyGenome& G : Genomes) { Names.Add(FString::Printf(TEXT("%s|%.4f|%.4f"), *G.Name, G.Hardiness, G.Fury)); }
		}
		return Names;
	};

	TestTrue(TEXT("the same seed and the same play reproduce the same Blight"), Sequence(1234) == Sequence(1234));
	TestTrue(TEXT("a different seed gives a different Blight"), Sequence(1234) != Sequence(98765));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyEvolutionSelectionTest, "ProceduralGen.EnemyEvolution.EvolutionClimbsTowardsWhatSurvives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEnemyEvolutionSelectionTest::RunTest(const FString& Parameters)
{
	// A pretend defence that only archers (slow single shots) guard: fast enemies get furthest. If selection
	// works, the Blight should grow faster over the generations.
	float FirstMean = 0.0f;
	float LastMean = 0.0f;
	const int32 Seeds = 20;

	for (int32 Seed = 1; Seed <= Seeds; ++Seed)
	{
		UEnemyEvolution* Evolution = NewObject<UEnemyEvolution>();
		Evolution->EnsureSeeded(Seed * 31);

		for (int32 Wave = 0; Wave < 9; ++Wave)
		{
			TArray<FEnemyGenome> Genomes;
			RunWave(Evolution, 16, [](const FEnemyGenome& G) { return FMath::Clamp(G.Swiftness, 0.0f, 1.0f); }, &Genomes);
			if (Wave == 0) { FirstMean += MeanSwiftness(Genomes); }
			if (Wave == 8) { LastMean += MeanSwiftness(Genomes); }
		}
	}

	FirstMean /= Seeds;
	LastMean /= Seeds;
	AddInfo(FString::Printf(TEXT("Mean Swiftness went from %.3f in generation 0 to %.3f in generation 8."), FirstMean, LastMean));
	TestTrue(TEXT("selection pushes the population towards the trait that survives"), LastMean > FirstMean + 0.08f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyEvolutionChampionTest, "ProceduralGen.EnemyEvolution.ChampionSurvivesUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Runs this automated test and reports any failure.
bool FEnemyEvolutionChampionTest::RunTest(const FString& Parameters)
{
	UEnemyEvolution* Evolution = NewObject<UEnemyEvolution>();
	Evolution->EnsureSeeded(777);

	TestEqual(TEXT("starts at generation 0"), Evolution->GetGeneration(), 0);
	Evolution->EndWave();
	TestEqual(TEXT("ending a wave with no results changes nothing"), Evolution->GetGeneration(), 0);
	TestFalse(TEXT("no champion yet"), Evolution->HasChampion());

	TArray<FEnemyGenome> First;
	RunWave(Evolution, 10, [](const FEnemyGenome& G) { return G.Hardiness; }, &First);
	TestEqual(TEXT("a finished wave moves to the next generation"), Evolution->GetGeneration(), 1);
	TestTrue(TEXT("a champion exists"), Evolution->HasChampion());

	// The champion is the genome with the highest hardiness (the fitness we used).
	FEnemyGenome Best = First[0];
	for (const FEnemyGenome& G : First) { if (G.Hardiness > Best.Hardiness) { Best = G; } }
	const FEnemyGenome Champion = Evolution->GetChampion();
	TestEqual(TEXT("the champion is the fittest enemy"), Champion.Name, Best.Name);

	// The first genome of the next generation is that champion, unchanged.
	const FEnemyGenome Next = Evolution->NextGenome();
	TestTrue(TEXT("the first of the next generation is flagged as the champion"), Next.bChampion);
	TestEqual(TEXT("the champion's hardiness is unchanged"), Next.Hardiness, Champion.Hardiness);
	TestEqual(TEXT("the champion's swiftness is unchanged"), Next.Swiftness, Champion.Swiftness);
	TestEqual(TEXT("the champion's fury is unchanged"), Next.Fury, Champion.Fury);

	// Only one champion per generation: the next genome is a child.
	TestFalse(TEXT("only the first of the generation is the champion"), Evolution->NextGenome().bChampion);

	// A different map seed starts the Blight again from nothing.
	Evolution->EnsureSeeded(778);
	TestEqual(TEXT("a new map resets the generation"), Evolution->GetGeneration(), 0);
	TestFalse(TEXT("a new map forgets the old champion"), Evolution->HasChampion());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
