#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerStructTest
{
	static const FName ModuleName(TEXT("CompilerAnnotatedStructRoundTrip"));
	static const FString ScriptFilename(TEXT("CompilerAnnotatedStructRoundTrip.as"));
	static const FName GeneratedStructName(TEXT("AnnotatedCarrier"));
	static const FName ValuePropertyName(TEXT("Value"));
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerStructTests,
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
			Engine.DiscardModule(*CompilerStructTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerStructTest::ModuleName,
			CompilerStructTest::ScriptFilename,
			TestScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Annotated struct round-trip test should compile through the preprocessor-enabled full-reload pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Annotated struct round-trip test should report preprocessor usage in the compile summary")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("Annotated struct round-trip test should finish with a fully handled compile result")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Annotated struct round-trip test should compile without diagnostics")));
		if (!bCompiled)
		{
			return;
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerStructTest::ScriptFilename,
			CompilerStructTest::ModuleName,
			TEXT("int Entry()"),
			EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Annotated struct round-trip test should execute the compiled Entry function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				7,
				EntryResult,
				TEXT("Annotated struct round-trip test should read the annotated struct field through runtime execution")));
		}

		UScriptStruct* GeneratedStruct = FindObject<UScriptStruct>(FAngelscriptEngine::GetPackage(), *CompilerStructTest::GeneratedStructName.ToString());
		ASSERT_THAT(IsNotNull(GeneratedStruct, TEXT("Annotated struct round-trip test should materialize a backing UScriptStruct")));

		FIntProperty* ValueProperty = FindFProperty<FIntProperty>(GeneratedStruct, CompilerStructTest::ValuePropertyName);
		ASSERT_THAT(IsNotNull(
			ValueProperty,
			TEXT("Annotated struct round-trip test should preserve the reflected Value property on the generated UScriptStruct")));

		}

	}

};

#endif
