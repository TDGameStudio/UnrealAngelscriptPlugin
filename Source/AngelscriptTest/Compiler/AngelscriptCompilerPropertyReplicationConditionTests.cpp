#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreNetTypes.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPropertyReplicationConditionTest
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

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerPropertyReplicationConditionTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PropertyReplicationConditionRoundTrip)
	{


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
			Engine.DiscardModule(*CompilerPropertyReplicationConditionTest::ModuleName.ToString());
		};

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPropertyReplicationConditionTest::ModuleName,
			CompilerPropertyReplicationConditionTest::RelativeScriptPath,
			TestScriptSource,
			true,
			Summary,
			true);

		if (Summary.Diagnostics.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Compile diagnostics: %s"),
				*CompilerPropertyReplicationConditionTest::JoinDiagnostics(Summary.Diagnostics)));
		}

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Property replication-condition round-trip should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Property replication-condition round-trip should record preprocessor usage in the compile summary")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Property replication-condition round-trip should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("Property replication-condition round-trip should stay on the full-reload handled path")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Property replication-condition round-trip should keep compile diagnostics empty")));
		if (!bCompiled || !Summary.bCompileSucceeded)
		{
			return;
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerPropertyReplicationConditionTest::RelativeScriptPath,
			CompilerPropertyReplicationConditionTest::ModuleName,
			CompilerPropertyReplicationConditionTest::EntryFunctionDeclaration,
			EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Property replication-condition round-trip should execute the compiled entry function")));
		ASSERT_THAT(AreEqual(
			CompilerPropertyReplicationConditionTest::ExpectedEntryValue,
			EntryResult,
			TEXT("Property replication-condition round-trip should preserve module execution after metadata propagation")));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPropertyReplicationConditionTest::ClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Property replication-condition round-trip should materialize the generated class")))
		{
			return;
		}

		FIntProperty* OwnerOnlyProperty = FindFProperty<FIntProperty>(GeneratedClass, *CompilerPropertyReplicationConditionTest::OwnerOnlyPropertyName);
		FIntProperty* SkipReplayProperty = FindFProperty<FIntProperty>(GeneratedClass, *CompilerPropertyReplicationConditionTest::SkipReplayPropertyName);
		if (!this->Assert.IsNotNull(OwnerOnlyProperty, TEXT("Property replication-condition round-trip should materialize the OwnerOnly property"))
			|| !this->Assert.IsNotNull(SkipReplayProperty, TEXT("Property replication-condition round-trip should materialize the SkipReplay property")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			OwnerOnlyProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("OwnerOnly property should carry CPF_Net")));
		ASSERT_THAT(IsTrue(
			SkipReplayProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("SkipReplay property should carry CPF_Net")));
		ASSERT_THAT(AreEqual(
			COND_OwnerOnly,
			OwnerOnlyProperty->GetBlueprintReplicationCondition(),
			TEXT("OwnerOnly property should preserve COND_OwnerOnly")));
		ASSERT_THAT(AreEqual(
			COND_SkipReplay,
			SkipReplayProperty->GetBlueprintReplicationCondition(),
			TEXT("SkipReplay property should preserve COND_SkipReplay")));

		}

	}

};

#endif
