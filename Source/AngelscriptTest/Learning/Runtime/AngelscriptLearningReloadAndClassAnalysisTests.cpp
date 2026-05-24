#include "AngelscriptLearningTrace.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningReloadAndClassAnalysisTests_Private
{
	struct FLearningReloadTestCase
	{
		FString TestCaseLabel;
		FName ModuleName;
		FString Filename;
		FString BaselineScript;
		FString UpdatedScript;
		FString ChangeSummary;
		FAngelscriptClassGenerator::EReloadRequirement ExpectedRequirement = FAngelscriptClassGenerator::Error;
		bool bExpectedWantsFullReload = false;
		bool bExpectedNeedsFullReload = false;
	};

	struct FLearningReloadOutcome
	{
		bool bBaselineCompiled = false;
		bool bAnalyzed = false;
		FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
	};

	FString GetReloadRequirementLabel(FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement)
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

	FLearningReloadOutcome RunReloadTestCase(FAngelscriptEngine& Engine, const FLearningReloadTestCase& TestCase)
	{
		FLearningReloadOutcome Outcome;
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

	void TraceReloadTestCase(FAngelscriptLearningTraceSession& Trace, const FLearningReloadTestCase& TestCase, const FLearningReloadOutcome& Outcome)
	{
		Trace.AddStep(TestCase.TestCaseLabel, Outcome.bAnalyzed ? TEXT("Reload analysis completed for the updated script version") : TEXT("Reload analysis stopped before producing a stable decision"));
		Trace.AddKeyValue(TEXT("ModuleName"), TestCase.ModuleName.ToString());
		Trace.AddKeyValue(TEXT("Filename"), TestCase.Filename);
		Trace.AddKeyValue(TEXT("ChangeSummary"), TestCase.ChangeSummary);
		Trace.AddKeyValue(TEXT("BaselineCompiled"), Outcome.bBaselineCompiled ? TEXT("true") : TEXT("false"));
		Trace.AddKeyValue(TEXT("ReloadRequirement"), GetReloadRequirementLabel(Outcome.ReloadRequirement));
		Trace.AddKeyValue(TEXT("WantsFullReload"), Outcome.bWantsFullReload ? TEXT("true") : TEXT("false"));
		Trace.AddKeyValue(TEXT("NeedsFullReload"), Outcome.bNeedsFullReload ? TEXT("true") : TEXT("false"));
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningReloadAndClassAnalysisTest,
	"Angelscript.TestModule.Learning.Runtime.ReloadAnalysis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningReloadAndClassAnalysisTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningReloadAndClassAnalysisTests_Private;
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

	FAngelscriptLearningTraceSession Trace(TEXT("LearningReloadAnalysis"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	const FLearningReloadTestCase BodyOnlyTestCase{
		TEXT("AnalyzeReload.BodyOnlyChange"),
		TEXT("LearningReloadBodyOnlyModule"),
		TEXT("LearningReloadBodyOnlyModule.as"),
		TEXT(R"AS(
UCLASS()
class ULearningReloadBodyOnlyTarget : UObject
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
class ULearningReloadBodyOnlyTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 2;
	}
}
)AS"),
		TEXT("Only the function body changed; class layout and signatures stayed the same."),
		FAngelscriptClassGenerator::SoftReload,
		false,
		false,
	};
	const FLearningReloadOutcome BodyOnlyOutcome = RunReloadTestCase(Engine, BodyOnlyTestCase);
	TraceReloadTestCase(Trace, BodyOnlyTestCase, BodyOnlyOutcome);

	const FLearningReloadTestCase ClassAddedTestCase{
		TEXT("AnalyzeReload.ClassAdded"),
		TEXT("LearningReloadClassAddedModule"),
		TEXT("LearningReloadClassAddedModule.as"),
		TEXT(R"AS(
UCLASS()
class UExistingLearningReloadTarget : UObject
{
}
)AS"),
		TEXT(R"AS(
UCLASS()
class UExistingLearningReloadTarget : UObject
{
}

UCLASS()
class UNewLearningReloadTarget : UObject
{
}
)AS"),
		TEXT("A second script class was added to the module, changing the generated-type set."),
		FAngelscriptClassGenerator::FullReloadSuggested,
		true,
		false,
	};
	const FLearningReloadOutcome ClassAddedOutcome = RunReloadTestCase(Engine, ClassAddedTestCase);
	TraceReloadTestCase(Trace, ClassAddedTestCase, ClassAddedOutcome);

	const FLearningReloadTestCase SignatureChangeTestCase{
		TEXT("AnalyzeReload.FunctionSignatureChanged"),
		TEXT("LearningReloadSignatureModule"),
		TEXT("LearningReloadSignatureModule.as"),
		TEXT(R"AS(
UCLASS()
class ULearningReloadSignatureTarget : UObject
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
class ULearningReloadSignatureTarget : UObject
{
	UFUNCTION()
	float ComputeValue(float Scale)
	{
		return Scale;
	}
}
)AS"),
		TEXT("The UFUNCTION signature changed from int ComputeValue() to float ComputeValue(float)."),
		FAngelscriptClassGenerator::FullReloadRequired,
		true,
		true,
	};
	const FLearningReloadOutcome SignatureChangeOutcome = RunReloadTestCase(Engine, SignatureChangeTestCase);
	TraceReloadTestCase(Trace, SignatureChangeTestCase, SignatureChangeOutcome);

	const bool bBodyOnlyBaselineCompiled = TestTrue(TEXT("Body-only baseline module should compile"), BodyOnlyOutcome.bBaselineCompiled);
	const bool bBodyOnlyAnalyzed = TestTrue(TEXT("Body-only change should analyze successfully"), BodyOnlyOutcome.bAnalyzed);
	const bool bBodyOnlyRequirement = TestEqual(TEXT("Body-only change should remain soft reload"), BodyOnlyOutcome.ReloadRequirement, BodyOnlyTestCase.ExpectedRequirement);
	const bool bBodyOnlyFlags = TestFalse(TEXT("Body-only change should not request full reload"), BodyOnlyOutcome.bWantsFullReload || BodyOnlyOutcome.bNeedsFullReload);

	const bool bClassAddedBaselineCompiled = TestTrue(TEXT("Class-added baseline module should compile"), ClassAddedOutcome.bBaselineCompiled);
	const bool bClassAddedAnalyzed = TestTrue(TEXT("Class-added change should analyze successfully"), ClassAddedOutcome.bAnalyzed);
	const bool bClassAddedRequirement = TestEqual(TEXT("Class-added change should suggest a full reload"), ClassAddedOutcome.ReloadRequirement, ClassAddedTestCase.ExpectedRequirement);
	const bool bClassAddedFlags = TestTrue(TEXT("Class-added change should request the full reload path"), ClassAddedOutcome.bWantsFullReload && !ClassAddedOutcome.bNeedsFullReload);

	const bool bSignatureBaselineCompiled = TestTrue(TEXT("Signature-change baseline module should compile"), SignatureChangeOutcome.bBaselineCompiled);
	const bool bSignatureAnalyzed = TestTrue(TEXT("Signature change should analyze successfully"), SignatureChangeOutcome.bAnalyzed);
	const bool bSignatureRequirement = TestEqual(TEXT("Signature change should require a full reload"), SignatureChangeOutcome.ReloadRequirement, SignatureChangeTestCase.ExpectedRequirement);
	const bool bSignatureFlags = TestTrue(TEXT("Signature change should mark full reload as required"), SignatureChangeOutcome.bWantsFullReload && SignatureChangeOutcome.bNeedsFullReload);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsSoftReloadKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("SoftReload"));
	const bool bContainsFullReloadRequiredKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("FullReloadRequired"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 3);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bBodyOnlyBaselineCompiled
		&& bBodyOnlyAnalyzed
		&& bBodyOnlyRequirement
		&& bBodyOnlyFlags
		&& bClassAddedBaselineCompiled
		&& bClassAddedAnalyzed
		&& bClassAddedRequirement
		&& bClassAddedFlags
		&& bSignatureBaselineCompiled
		&& bSignatureAnalyzed
		&& bSignatureRequirement
		&& bSignatureFlags
		&& bPhaseSequenceOk
		&& bContainsSoftReloadKeyword
		&& bContainsFullReloadRequiredKeyword
		&& bMinimumEventsOk;

	}
}

#endif
