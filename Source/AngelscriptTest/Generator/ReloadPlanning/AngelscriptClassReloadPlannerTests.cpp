#include "CQTest.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "ClassGenerator/AngelscriptClassReloadPlanner.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptClassReloadPlannerTests,
	"Angelscript.TestModule.Generator.ReloadPlanning.Planner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using EReloadRequirement = FAngelscriptClassGenerator::EReloadRequirement;

public:
	TEST_METHOD(SingleDependencyPropagatesSuggestedToConsumer)
	{
		FAngelscriptClassReloadPlanner Planner;
		const FAngelscriptClassReloadPlanner::FNodeHandle Provider = Planner.AddNode(TEXT("Provider"), EReloadRequirement::FullReloadSuggested);
		const FAngelscriptClassReloadPlanner::FNodeHandle Consumer = Planner.AddNode(TEXT("Consumer"), EReloadRequirement::SoftReload);

		Planner.AddDependency(Consumer, Provider);
		Planner.PropagateAll();

		ASSERT_THAT(AreEqual(EReloadRequirement::FullReloadSuggested, Planner.GetRequirement(Consumer), TEXT("Consumer should inherit provider reload suggestion")));
		ASSERT_THAT(AreEqual(EReloadRequirement::FullReloadSuggested, Planner.GetRequirement(Provider), TEXT("Provider should keep its own requirement")));
	}

	TEST_METHOD(MultiHopDependencyPropagatesRequiredToRoot)
	{
		FAngelscriptClassReloadPlanner Planner;
		const FAngelscriptClassReloadPlanner::FNodeHandle Provider = Planner.AddNode(TEXT("Provider"), EReloadRequirement::FullReloadRequired);
		const FAngelscriptClassReloadPlanner::FNodeHandle Middle = Planner.AddNode(TEXT("Middle"), EReloadRequirement::SoftReload);
		const FAngelscriptClassReloadPlanner::FNodeHandle Root = Planner.AddNode(TEXT("Root"), EReloadRequirement::SoftReload);

		Planner.AddDependency(Middle, Provider);
		Planner.AddDependency(Root, Middle);
		Planner.PropagateAll();

		ASSERT_THAT(AreEqual(EReloadRequirement::FullReloadRequired, Planner.GetRequirement(Root), TEXT("Root should inherit provider requirement through the middle node")));
		ASSERT_THAT(AreEqual(EReloadRequirement::FullReloadRequired, Planner.GetRequirement(Middle), TEXT("Middle should inherit provider requirement")));
	}

	TEST_METHOD(CyclicDependencyTerminatesWithHighestRequirement)
	{
		FAngelscriptClassReloadPlanner Planner;
		const FAngelscriptClassReloadPlanner::FNodeHandle First = Planner.AddNode(TEXT("First"), EReloadRequirement::SoftReload);
		const FAngelscriptClassReloadPlanner::FNodeHandle Second = Planner.AddNode(TEXT("Second"), EReloadRequirement::FullReloadSuggested);
		const FAngelscriptClassReloadPlanner::FNodeHandle Third = Planner.AddNode(TEXT("Third"), EReloadRequirement::Error);

		Planner.AddDependency(First, Second);
		Planner.AddDependency(Second, Third);
		Planner.AddDependency(Third, First);
		Planner.PropagateAll();

		ASSERT_THAT(AreEqual(EReloadRequirement::Error, Planner.GetRequirement(First), TEXT("First should converge to the cycle's highest requirement")));
		ASSERT_THAT(AreEqual(EReloadRequirement::Error, Planner.GetRequirement(Second), TEXT("Second should converge to the cycle's highest requirement")));
		ASSERT_THAT(AreEqual(EReloadRequirement::Error, Planner.GetRequirement(Third), TEXT("Third should keep the cycle's highest requirement")));
	}

	TEST_METHOD(ResetClearsNodesAndDependencies)
	{
		FAngelscriptClassReloadPlanner Planner;
		const FAngelscriptClassReloadPlanner::FNodeHandle Provider = Planner.AddNode(TEXT("Provider"), EReloadRequirement::FullReloadRequired);
		const FAngelscriptClassReloadPlanner::FNodeHandle Consumer = Planner.AddNode(TEXT("Consumer"), EReloadRequirement::SoftReload);
		Planner.AddDependency(Consumer, Provider);

		Planner.Reset();
		const FAngelscriptClassReloadPlanner::FNodeHandle Fresh = Planner.AddNode(TEXT("Fresh"), EReloadRequirement::SoftReload);
		Planner.PropagateAll();

		ASSERT_THAT(AreEqual(EReloadRequirement::SoftReload, Planner.GetRequirement(Fresh), TEXT("Reset should discard previous nodes and edges")));
	}
};

#endif
