#include "AngelscriptLearningTrace.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningHotReloadDecisionTraceTests_Private
{
	struct FLearningHotReloadDecisionTestCase
	{
		FString TestCaseLabel;
		FName ModuleName;
		FString Filename;
		FString BaselineScript;
		FString UpdatedScript;
		FString TriggerExplanation;
		FAngelscriptClassGenerator::EReloadRequirement ExpectedRequirement = FAngelscriptClassGenerator::Error;
		bool bExpectedWantsFullReload = false;
		bool bExpectedNeedsFullReload = false;
	};

	struct FLearningHotReloadDecisionOutcome
	{
		bool bBaselineCompiled = false;
		bool bAnalyzed = false;
		FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
	};

	FString GetHotReloadDecisionRequirementLabel(FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement)
	{
		switch (ReloadRequirement)
		{
		case FAngelscriptClassGenerator::SoftReload:
			return TEXT("SoftReload");
		case FAngelscriptClassGenerator::FullReloadSuggested:
			return TEXT("FullReloadSuggested");
		case FAngelscriptClassGenerator::FullReloadRequired:
			return TEXT("FullReloadRequired");
		case FAngelscriptClassGenerator::Error:
		default:
			return TEXT("Error");
		}
	}

	FLearningHotReloadDecisionOutcome RunDecisionTestCase(FAngelscriptEngine& Engine, const FLearningHotReloadDecisionTestCase& TestCase)
	{
		FLearningHotReloadDecisionOutcome Outcome;
		ASTEST_RESET_ENGINE(Engine);
		Outcome.bBaselineCompiled = CompileAnnotatedModuleFromMemory(&Engine, TestCase.ModuleName, TestCase.Filename, TestCase.BaselineScript);
		if (!Outcome.bBaselineCompiled)
		{
			return Outcome;
		}

		Outcome.bAnalyzed = AnalyzeReloadFromMemory(
			&Engine,
			TestCase.ModuleName,
			TestCase.Filename,
			TestCase.UpdatedScript,
			Outcome.ReloadRequirement,
			Outcome.bWantsFullReload,
			Outcome.bNeedsFullReload);
		return Outcome;
	}

	void TraceDecisionTestCase(FAngelscriptLearningTraceSession& Trace, const FLearningHotReloadDecisionTestCase& TestCase, const FLearningHotReloadDecisionOutcome& Outcome)
	{
		Trace.AddStep(TestCase.TestCaseLabel, Outcome.bAnalyzed ? TEXT("Hot-reload analysis produced a stable decision for this script change") : TEXT("Hot-reload analysis stopped before producing a decision"));
		Trace.AddKeyValue(TEXT("ModuleName"), TestCase.ModuleName.ToString());
		Trace.AddKeyValue(TEXT("Filename"), TestCase.Filename);
		Trace.AddKeyValue(TEXT("TriggerExplanation"), TestCase.TriggerExplanation);
		Trace.AddKeyValue(TEXT("BaselineCompiled"), Outcome.bBaselineCompiled ? TEXT("true") : TEXT("false"));
		Trace.AddKeyValue(TEXT("ReloadRequirement"), GetHotReloadDecisionRequirementLabel(Outcome.ReloadRequirement));
		Trace.AddKeyValue(TEXT("WantsFullReload"), Outcome.bWantsFullReload ? TEXT("true") : TEXT("false"));
		Trace.AddKeyValue(TEXT("NeedsFullReload"), Outcome.bNeedsFullReload ? TEXT("true") : TEXT("false"));
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningHotReloadDecisionTraceTest,
	"Angelscript.TestModule.Learning.Runtime.HotReloadDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningHotReloadDecisionTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningHotReloadDecisionTraceTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ASTEST_RESET_ENGINE(Engine);
	ON_SCOPE_EXIT
	{
		ASTEST_RESET_ENGINE(Engine);
	};

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningHotReloadDecision"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	const TArray<FLearningHotReloadDecisionTestCase> TestCases = {
		{
			TEXT("HotReload.NoChange"),
			TEXT("LearningHotReloadNoChangeModule"),
			TEXT("LearningHotReloadNoChangeModule.as"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadNoChangeTarget : UObject
{
	UPROPERTY()
	int Value;

	default Value = 10;

	UFUNCTION()
	int GetValue()
	{
		return Value;
	}
}
)AS"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadNoChangeTarget : UObject
{
	UPROPERTY()
	int Value;

	default Value = 10;

	UFUNCTION()
	int GetValue()
	{
		return Value;
	}
}
)AS"),
			TEXT("Nothing changed between baseline and update, so the generated class layout and callable signatures remain identical."),
			FAngelscriptClassGenerator::SoftReload,
			false,
			false,
		},
		{
			TEXT("HotReload.BodyOnlyChange"),
			TEXT("LearningHotReloadBodyOnlyModule"),
			TEXT("LearningHotReloadBodyOnlyModule.as"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadBodyOnlyTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadBodyOnlyTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 2;
	}
}
)AS"),
			TEXT("Only the function body changed; no reflected property, class, or signature metadata moved."),
			FAngelscriptClassGenerator::SoftReload,
			false,
			false,
		},
		{
			TEXT("HotReload.PropertyCountChange"),
			TEXT("LearningHotReloadPropertyCountModule"),
			TEXT("LearningHotReloadPropertyCountModule.as"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadPropertyTarget : UObject
{
	UPROPERTY()
	int Value;
}
)AS"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadPropertyTarget : UObject
{
	UPROPERTY()
	int Value;

	UPROPERTY()
	int ExtraValue;
}
)AS"),
			TEXT("Adding a reflected property changes the generated class layout, so soft reload is no longer sufficient."),
			FAngelscriptClassGenerator::FullReloadSuggested,
			true,
			false,
		},
		{
			TEXT("HotReload.SuperClassChange"),
			TEXT("LearningHotReloadSuperClassModule"),
			TEXT("LearningHotReloadSuperClassModule.as"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadSuperTarget : UObject
{
}
)AS"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadSuperTarget : AActor
{
}
)AS"),
			TEXT("Changing the reflected super class changes inheritance and object layout, which forces a full rebuild of generated Unreal types."),
			FAngelscriptClassGenerator::FullReloadRequired,
			true,
			true,
		},
		{
			TEXT("HotReload.ClassAddedOrRemoved"),
			TEXT("LearningHotReloadClassDeltaModule"),
			TEXT("LearningHotReloadClassDeltaModule.as"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadSurvivorTarget : UObject
{
}

UCLASS()
class ULearningHotReloadRemovedTarget : UObject
{
}
)AS"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadSurvivorTarget : UObject
{
}

UCLASS()
class ULearningHotReloadAddedTarget : UObject
{
}
)AS"),
			TEXT("The set of generated script classes changed between revisions, so the reload path must recreate type registration rather than patch a single body."),
			FAngelscriptClassGenerator::FullReloadRequired,
			true,
			true,
		},
		{
			TEXT("HotReload.FunctionSignatureChanged"),
			TEXT("LearningHotReloadSignatureModule"),
			TEXT("LearningHotReloadSignatureModule.as"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadFunctionTarget : UObject
{
	UFUNCTION()
	int ComputeValue()
	{
		return 1;
	}
}
)AS"),
			TEXT(R"AS(
UCLASS()
class ULearningHotReloadFunctionTarget : UObject
{
	UFUNCTION()
	float ComputeValue(float Scale)
	{
		return Scale;
	}
}
)AS"),
			TEXT("Changing a reflected function signature invalidates the generated UFunction contract, so the old type can no longer be patched in place."),
			FAngelscriptClassGenerator::FullReloadRequired,
			true,
			true,
		},
	};

	TArray<FString> DecisionSummaries;
	bool bAllChecksPassed = true;
	for (const FLearningHotReloadDecisionTestCase& TestCase : TestCases)
	{
		const FLearningHotReloadDecisionOutcome Outcome = RunDecisionTestCase(Engine, TestCase);
		TraceDecisionTestCase(Trace, TestCase, Outcome);
		DecisionSummaries.Add(FString::Printf(
			TEXT("%s => %s (wants=%s needs=%s)"),
			*TestCase.TestCaseLabel,
			*GetHotReloadDecisionRequirementLabel(Outcome.ReloadRequirement),
			Outcome.bWantsFullReload ? TEXT("true") : TEXT("false"),
			Outcome.bNeedsFullReload ? TEXT("true") : TEXT("false")));

		const bool bBaselineCompiled = TestTrue(*FString::Printf(TEXT("%s should compile the baseline module"), *TestCase.TestCaseLabel), Outcome.bBaselineCompiled);
		const bool bAnalyzed = TestTrue(*FString::Printf(TEXT("%s should analyze successfully"), *TestCase.TestCaseLabel), Outcome.bAnalyzed);
		const bool bRequirementMatches = TestEqual(*FString::Printf(TEXT("%s should report the expected reload requirement"), *TestCase.TestCaseLabel), Outcome.ReloadRequirement, TestCase.ExpectedRequirement);
		const bool bWantsMatches = TestEqual(*FString::Printf(TEXT("%s should report the expected wants-full-reload flag"), *TestCase.TestCaseLabel), Outcome.bWantsFullReload, TestCase.bExpectedWantsFullReload);
		const bool bNeedsMatches = TestEqual(*FString::Printf(TEXT("%s should report the expected needs-full-reload flag"), *TestCase.TestCaseLabel), Outcome.bNeedsFullReload, TestCase.bExpectedNeedsFullReload);
		bAllChecksPassed = bAllChecksPassed && bBaselineCompiled && bAnalyzed && bRequirementMatches && bWantsMatches && bNeedsMatches;
	}

	if (DecisionSummaries.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(DecisionSummaries, TEXT("HotReloadDecisionMatrix")));
	}

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsSoftReloadKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("SoftReload"));
	const bool bContainsFullReloadSuggestedKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("FullReloadSuggested"));
	const bool bContainsFullReloadRequiredKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("FullReloadRequired"));
	const bool bContainsTriggerExplanationKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("TriggerExplanation"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 6);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bAllChecksPassed
		&& bPhaseSequenceOk
		&& bContainsSoftReloadKeyword
		&& bContainsFullReloadSuggestedKeyword
		&& bContainsFullReloadRequiredKeyword
		&& bContainsTriggerExplanationKeyword
		&& bMinimumEventsOk;

	}
}

#endif
