#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreNetTypes.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace AngelscriptTestSupport;

namespace CompilerPipelinePropertyReplicationConditionTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.PropertyReplicationConditionRoundTrip"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/PropertyReplicationConditionRoundTrip.as"));
	static const FString ClassName(TEXT("APropertyReplicationConditionCarrier"));
	static const FString OwnerOnlyPropertyName(TEXT("OwnerOnlyValue"));
	static const FString SkipReplayPropertyName(TEXT("SkipReplayValue"));
	static const FString EntryFunctionDeclaration(TEXT("int Entry()"));
	static const int32 ExpectedEntryValue = 42;

	FString JoinDiagnostics(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics)
	{
		TArray<FString> Lines;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
		{
			Lines.Add(FString::Printf(
				TEXT("[%s] %s(%d:%d) %s"),
				Diagnostic.bIsError ? TEXT("Error") : (Diagnostic.bIsInfo ? TEXT("Info") : TEXT("Warning")),
				*Diagnostic.Section,
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
		}

		return FString::Join(Lines, TEXT(" | "));
	}
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelinePropertyReplicationConditionTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PropertyReplicationConditionRoundTrip)
	{
	using namespace AngelscriptTestSupport;


		const FString TestScriptSource = TEXT(R"AS(
	UCLASS()
	class APropertyReplicationConditionCarrier : AActor
	{
		UPROPERTY(Replicated, ReplicationCondition=OwnerOnly)
		int OwnerOnlyValue = 11;

		UPROPERTY(Replicated, ReplicationCondition=SkipReplay)
		int SkipReplayValue = 31;
	}

	int Entry()
	{
		return 42;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelinePropertyReplicationConditionTest::ModuleName.ToString());
		};

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelinePropertyReplicationConditionTest::ModuleName,
			CompilerPipelinePropertyReplicationConditionTest::RelativeScriptPath,
			TestScriptSource,
			true,
			Summary,
			true);

		if (Summary.Diagnostics.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Compile diagnostics: %s"),
				*CompilerPipelinePropertyReplicationConditionTest::JoinDiagnostics(Summary.Diagnostics)));
		}

		TestRunner->TestTrue(
			TEXT("Property replication-condition round-trip should compile through the normal preprocessor pipeline"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("Property replication-condition round-trip should record preprocessor usage in the compile summary"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("Property replication-condition round-trip should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("Property replication-condition round-trip should stay on the full-reload handled path"),
			Summary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Property replication-condition round-trip should keep compile diagnostics empty"),
			Summary.Diagnostics.Num(),
			0);
		if (!bCompiled || !Summary.bCompileSucceeded)
		{
			return;
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerPipelinePropertyReplicationConditionTest::RelativeScriptPath,
			CompilerPipelinePropertyReplicationConditionTest::ModuleName,
			CompilerPipelinePropertyReplicationConditionTest::EntryFunctionDeclaration,
			EntryResult);
		TestRunner->TestTrue(
			TEXT("Property replication-condition round-trip should execute the compiled entry function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Property replication-condition round-trip should preserve module execution after metadata propagation"),
				EntryResult,
				CompilerPipelinePropertyReplicationConditionTest::ExpectedEntryValue);
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelinePropertyReplicationConditionTest::ClassName);
		if (!TestRunner->TestNotNull(TEXT("Property replication-condition round-trip should materialize the generated class"), GeneratedClass))
		{
			return;
		}

		FIntProperty* OwnerOnlyProperty = FindFProperty<FIntProperty>(GeneratedClass, *CompilerPipelinePropertyReplicationConditionTest::OwnerOnlyPropertyName);
		FIntProperty* SkipReplayProperty = FindFProperty<FIntProperty>(GeneratedClass, *CompilerPipelinePropertyReplicationConditionTest::SkipReplayPropertyName);
		if (!TestRunner->TestNotNull(TEXT("Property replication-condition round-trip should materialize the OwnerOnly property"), OwnerOnlyProperty)
			|| !TestRunner->TestNotNull(TEXT("Property replication-condition round-trip should materialize the SkipReplay property"), SkipReplayProperty))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("OwnerOnly property should carry CPF_Net"),
			OwnerOnlyProperty->HasAnyPropertyFlags(CPF_Net));
		TestRunner->TestTrue(
			TEXT("SkipReplay property should carry CPF_Net"),
			SkipReplayProperty->HasAnyPropertyFlags(CPF_Net));
		TestRunner->TestEqual(
			TEXT("OwnerOnly property should preserve COND_OwnerOnly"),
			OwnerOnlyProperty->GetBlueprintReplicationCondition(),
			COND_OwnerOnly);
		TestRunner->TestEqual(
			TEXT("SkipReplay property should preserve COND_SkipReplay"),
			SkipReplayProperty->GetBlueprintReplicationCondition(),
			COND_SkipReplay);

		}

	}

};

#endif
