#include "Shared/AngelscriptLearningTrace.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningCompilerTraceTests_Private
{
	struct FLearningCompilerTestCase
	{
		FString ApiLabel;
		FName ModuleName;
		FString Filename;
		FString Script;
		ECompileType CompileType = ECompileType::SoftReloadOnly;
		bool bUsesPreprocessor = false;
		bool bSuppressCompileErrorLogs = false;
	};

	FString GetCompileTypeLabel(ECompileType CompileType)
	{
		switch (CompileType)
		{
		case ECompileType::Initial:
			return TEXT("Initial");
		case ECompileType::SoftReloadOnly:
			return TEXT("SoftReloadOnly");
		case ECompileType::FullReload:
			return TEXT("FullReload");
		default:
			return TEXT("Unknown");
		}
	}

	FString GetCompileResultLabel(ECompileResult CompileResult)
	{
		switch (CompileResult)
		{
		case ECompileResult::Error:
			return TEXT("Error");
		case ECompileResult::ErrorNeedFullReload:
			return TEXT("ErrorNeedFullReload");
		case ECompileResult::PartiallyHandled:
			return TEXT("PartiallyHandled");
		case ECompileResult::FullyHandled:
			return TEXT("FullyHandled");
		default:
			return TEXT("Unknown");
		}
	}

	FString GetDiagnosticSeverityLabel(const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
	{
		if (Diagnostic.bIsError)
		{
			return TEXT("Error");
		}

		if (Diagnostic.bIsInfo)
		{
			return TEXT("Info");
		}

		return TEXT("Warning");
	}

	TArray<FAngelscriptLearningTraceDiagnostic> ConvertDiagnostics(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics)
	{
		TArray<FAngelscriptLearningTraceDiagnostic> Result;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
		{
			FAngelscriptLearningTraceDiagnostic& TraceDiagnostic = Result.AddDefaulted_GetRef();
			TraceDiagnostic.Section = Diagnostic.Section;
			TraceDiagnostic.Row = Diagnostic.Row;
			TraceDiagnostic.Column = Diagnostic.Column;
			TraceDiagnostic.Severity = GetDiagnosticSeverityLabel(Diagnostic);
			TraceDiagnostic.Message = Diagnostic.Message;
		}
		return Result;
	}

	void TraceCompilerTestCase(FAngelscriptLearningTraceSession& Trace, const FLearningCompilerTestCase& TestCase, const FAngelscriptCompileTraceSummary& Outcome)
	{
		Trace.AddStep(TestCase.ApiLabel, Outcome.bCompileSucceeded ? TEXT("Compilation path completed without a hard error") : TEXT("Compilation path reported an error or missing preprocessing result"));
		Trace.AddKeyValue(TEXT("ScriptFilename"), TestCase.Filename);
		Trace.AddKeyValue(TEXT("CompileType"), GetCompileTypeLabel(TestCase.CompileType));
		Trace.AddKeyValue(TEXT("UsesPreprocessor"), TestCase.bUsesPreprocessor ? TEXT("true") : TEXT("false"));
		Trace.AddKeyValue(TEXT("ModuleDescCount"), FString::FromInt(Outcome.ModuleDescCount));
		Trace.AddKeyValue(TEXT("CompiledModuleCount"), FString::FromInt(Outcome.CompiledModuleCount));
		Trace.AddKeyValue(TEXT("CompileResult"), GetCompileResultLabel(Outcome.CompileResult));
		Trace.AddKeyValue(TEXT("DiagnosticsCount"), FString::FromInt(Outcome.Diagnostics.Num()));

		if (Outcome.ModuleNames.Num() > 0)
		{
			Trace.AddCodeBlock(FormatLearningTraceStringList(Outcome.ModuleNames, TEXT("ModuleNames")));
		}

		const TArray<FAngelscriptLearningTraceDiagnostic> TraceDiagnostics = ConvertDiagnostics(Outcome.Diagnostics);
		if (TraceDiagnostics.Num() > 0)
		{
			Trace.AddCodeBlock(FormatLearningTraceDiagnostics(TraceDiagnostics));
		}
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningCompilerTraceTest,
	"Angelscript.TestModule.Learning.Runtime.CompilerPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningCompilerTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningCompilerTraceTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("LearningCompilerBuildModule"));
		Engine.DiscardModule(TEXT("LearningCompilerPlainModule"));
		Engine.DiscardModule(TEXT("LearningCompilerAnnotatedModule"));
		Engine.DiscardModule(TEXT("LearningCompilerBrokenAnnotatedModule"));
	};

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningCompilerPipeline"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	const FLearningCompilerTestCase BuildModuleTestCase{
		TEXT("BuildModuleStyle.InitialCompile"),
		TEXT("LearningCompilerBuildModule"),
		TEXT("LearningCompilerBuildModule.as"),
		TEXT("int Entry() { return 42; }"),
		ECompileType::Initial,
		true,
	};
	FAngelscriptCompileTraceSummary BuildModuleOutcome;
	CompileModuleWithSummary(&Engine, BuildModuleTestCase.CompileType, BuildModuleTestCase.ModuleName, BuildModuleTestCase.Filename, BuildModuleTestCase.Script, BuildModuleTestCase.bUsesPreprocessor, BuildModuleOutcome, BuildModuleTestCase.bSuppressCompileErrorLogs);
	TraceCompilerTestCase(Trace, BuildModuleTestCase, BuildModuleOutcome);

	const FLearningCompilerTestCase PlainCompileTestCase{
		TEXT("CompileModuleFromMemoryStyle.SoftReloadOnly"),
		TEXT("LearningCompilerPlainModule"),
		TEXT("LearningCompilerPlainModule.as"),
		TEXT("int Entry() { int Base = 40; return Base + 2; }"),
		ECompileType::SoftReloadOnly,
		false,
	};
	FAngelscriptCompileTraceSummary PlainCompileOutcome;
	CompileModuleWithSummary(&Engine, PlainCompileTestCase.CompileType, PlainCompileTestCase.ModuleName, PlainCompileTestCase.Filename, PlainCompileTestCase.Script, PlainCompileTestCase.bUsesPreprocessor, PlainCompileOutcome, PlainCompileTestCase.bSuppressCompileErrorLogs);
	TraceCompilerTestCase(Trace, PlainCompileTestCase, PlainCompileOutcome);

	const FLearningCompilerTestCase AnnotatedCompileTestCase{
		TEXT("CompileAnnotatedModuleFromMemoryStyle.FullReload"),
		TEXT("LearningCompilerAnnotatedModule"),
		TEXT("LearningCompilerAnnotatedModule.as"),
		TEXT(R"(
UCLASS()
class ULearningCompilerTraceCarrier : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 42;
	}
}
)") ,
		ECompileType::FullReload,
		true,
	};
	FAngelscriptCompileTraceSummary AnnotatedCompileOutcome;
	CompileModuleWithSummary(&Engine, AnnotatedCompileTestCase.CompileType, AnnotatedCompileTestCase.ModuleName, AnnotatedCompileTestCase.Filename, AnnotatedCompileTestCase.Script, AnnotatedCompileTestCase.bUsesPreprocessor, AnnotatedCompileOutcome, AnnotatedCompileTestCase.bSuppressCompileErrorLogs);
	TraceCompilerTestCase(Trace, AnnotatedCompileTestCase, AnnotatedCompileOutcome);

	const FLearningCompilerTestCase BrokenAnnotatedTestCase{
		TEXT("CompileAnnotatedModuleFromMemoryStyle.Diagnostics"),
		TEXT("LearningCompilerBrokenAnnotatedModule"),
		TEXT("LearningCompilerBrokenAnnotatedModule.as"),
		TEXT(R"(
UCLASS()
class UBrokenLearningCompilerTraceCarrier : UObject
{
	UFUNCTION()
	MissingType GetValue()
	{
		MissingType Value;
		return Value;
	}
}
)") ,
		ECompileType::FullReload,
		true,
		true,
	};
	FAngelscriptCompileTraceSummary BrokenAnnotatedOutcome;
	CompileModuleWithSummary(&Engine, BrokenAnnotatedTestCase.CompileType, BrokenAnnotatedTestCase.ModuleName, BrokenAnnotatedTestCase.Filename, BrokenAnnotatedTestCase.Script, BrokenAnnotatedTestCase.bUsesPreprocessor, BrokenAnnotatedOutcome, BrokenAnnotatedTestCase.bSuppressCompileErrorLogs);
	TraceCompilerTestCase(Trace, BrokenAnnotatedTestCase, BrokenAnnotatedOutcome);

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("ULearningCompilerTraceCarrier"));
	if (GeneratedClass != nullptr)
	{
		Trace.AddStep(TEXT("GeneratedClassVisibility"), TEXT("Annotated compiler path produced a generated Unreal class that can be discovered after compilation"));
		Trace.AddKeyValue(TEXT("GeneratedClass"), GeneratedClass->GetName());
	}
	else
	{
		Trace.AddStep(TEXT("GeneratedClassVisibility"), TEXT("Annotated compiler path did not leave a discoverable generated Unreal class"));
		Trace.AddKeyValue(TEXT("GeneratedClass"), TEXT("<null>"));
	}

	const bool bBuildModuleSucceeded = TestTrue(TEXT("BuildModule-style initial compile should succeed"), BuildModuleOutcome.bCompileSucceeded);
	const bool bPlainCompileSucceeded = TestTrue(TEXT("CompileModuleFromMemory-style compile should succeed"), PlainCompileOutcome.bCompileSucceeded);
	const bool bAnnotatedCompileSucceeded = TestTrue(TEXT("CompileAnnotatedModuleFromMemory-style compile should succeed"), AnnotatedCompileOutcome.bCompileSucceeded);
	const bool bBrokenCompileFailed = TestFalse(TEXT("Broken annotated compiler input should fail"), BrokenAnnotatedOutcome.bCompileSucceeded);
	const bool bBrokenDiagnosticsPresent = TestTrue(TEXT("Broken annotated compiler input should produce diagnostics"), BrokenAnnotatedOutcome.Diagnostics.Num() > 0);
	const bool bGeneratedClassVisible = TestNotNull(TEXT("Annotated compiler path should leave a generated class visible for later runtime tests"), GeneratedClass);
	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsCompileResultKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("CompileResult"));
	const bool bContainsDiagnosticsKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("DiagnosticsCount"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 5);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bBuildModuleSucceeded
		&& bPlainCompileSucceeded
		&& bAnnotatedCompileSucceeded
		&& bBrokenCompileFailed
		&& bBrokenDiagnosticsPresent
		&& bGeneratedClassVisible
		&& bPhaseSequenceOk
		&& bContainsCompileResultKeyword
		&& bContainsDiagnosticsKeyword
		&& bMinimumEventsOk;

	}
}

#endif
