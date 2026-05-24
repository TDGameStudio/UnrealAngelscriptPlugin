#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineStructTest
{
	static const FName ModuleName(TEXT("CompilerAnnotatedStructRoundTrip"));
	static const FString ScriptFilename(TEXT("CompilerAnnotatedStructRoundTrip.as"));
	static const FName GeneratedStructName(TEXT("AnnotatedCarrier"));
	static const FName ValuePropertyName(TEXT("Value"));
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelineStructTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AnnotatedStructRoundTrip)
	{


		const FString TestScriptSource = TEXT(R"AS(
	USTRUCT()
	struct FAnnotatedCarrier
	{
		UPROPERTY()
		int Value = 7;
	}

	int Entry()
	{
		FAnnotatedCarrier Carrier;
		return Carrier.Value;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineStructTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineStructTest::ModuleName,
			CompilerPipelineStructTest::ScriptFilename,
			TestScriptSource,
			true,
			Summary);

		TestRunner->TestTrue(
			TEXT("Annotated struct round-trip test should compile through the preprocessor-enabled full-reload pipeline"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("Annotated struct round-trip test should report preprocessor usage in the compile summary"),
			Summary.bUsedPreprocessor);
		TestRunner->TestEqual(
			TEXT("Annotated struct round-trip test should finish with a fully handled compile result"),
			Summary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Annotated struct round-trip test should compile without diagnostics"),
			Summary.Diagnostics.Num(),
			0);
		if (!bCompiled)
		{
			return;
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerPipelineStructTest::ScriptFilename,
			CompilerPipelineStructTest::ModuleName,
			TEXT("int Entry()"),
			EntryResult);
		TestRunner->TestTrue(
			TEXT("Annotated struct round-trip test should execute the compiled Entry function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Annotated struct round-trip test should read the annotated struct field through runtime execution"),
				EntryResult,
				7);
		}

		UScriptStruct* GeneratedStruct = FindObject<UScriptStruct>(FAngelscriptEngine::GetPackage(), *CompilerPipelineStructTest::GeneratedStructName.ToString());
		if (!TestRunner->TestNotNull(TEXT("Annotated struct round-trip test should materialize a backing UScriptStruct"), GeneratedStruct))
		{
			return;
		}

		FIntProperty* ValueProperty = FindFProperty<FIntProperty>(GeneratedStruct, CompilerPipelineStructTest::ValuePropertyName);
		TestRunner->TestNotNull(
			TEXT("Annotated struct round-trip test should preserve the reflected Value property on the generated UScriptStruct"),
			ValueProperty);

		}

	}

};

#endif
