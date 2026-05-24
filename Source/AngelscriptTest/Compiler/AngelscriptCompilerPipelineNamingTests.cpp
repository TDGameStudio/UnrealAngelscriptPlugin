#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineNamingTest
{
	static const FName ModuleName(TEXT("CompilerGeneratedClassExactNameLookup"));
	static const FString ScriptFilename(TEXT("Tests/Compiler/CompilerGeneratedClassExactNameLookup.as"));
	static const FName GeneratedClassName(TEXT("UExactNameCarrier"));
	static const FName GeneratedFunctionName(TEXT("GetValue"));

	const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UExactNameCarrier : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 42;
	}
}
)AS");
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelineNamingTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratedClassExactNameLookup)
	{
	using namespace CompilerPipelineNamingTest;


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineNamingTest::ModuleName.ToString());
		};

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineNamingTest::ModuleName,
			CompilerPipelineNamingTest::ScriptFilename,
			CompilerPipelineNamingTest::ScriptSource,
			true,
			Summary,
			false);

		TestRunner->TestTrue(
			TEXT("Generated-class exact-name lookup test case should compile successfully"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("Generated-class exact-name lookup test case should use the preprocessor path"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("Generated-class exact-name lookup test case should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("Generated-class exact-name lookup test case should report FullyHandled"),
			Summary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Generated-class exact-name lookup test case should not emit diagnostics"),
			Summary.Diagnostics.Num(),
			0);
		if (!bCompiled)
		{
			return;
		}

		UPackage* Package = Engine.GetPackageInstance();
		if (!TestRunner->TestNotNull(TEXT("Generated-class exact-name lookup test case should expose the engine package"), Package))
		{
			return;
		}

		UClass* ExactLookupClass = FindObject<UClass>(Package, *CompilerPipelineNamingTest::GeneratedClassName.ToString());
		if (!TestRunner->TestNotNull(TEXT("Generated-class exact-name lookup test case should find the generated class by its exact script name"), ExactLookupClass))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Generated-class exact-name lookup test case should keep the exact UObject name"),
			ExactLookupClass->GetName(),
			CompilerPipelineNamingTest::GeneratedClassName.ToString());

		UClass* StrippedLookupClass = FindObject<UClass>(Package, TEXT("ExactNameCarrier"));
		TestRunner->TestNull(
			TEXT("Generated-class exact-name lookup test case should not leave behind a stripped-name alias"),
			StrippedLookupClass);

		UClass* HelperLookupClass = FindGeneratedClass(&Engine, CompilerPipelineNamingTest::GeneratedClassName);
		if (!TestRunner->TestNotNull(TEXT("Generated-class exact-name lookup test case should still resolve through FindGeneratedClass"), HelperLookupClass))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("Generated-class exact-name lookup test case should have helper lookup resolve to the same exact-name object"),
			HelperLookupClass == ExactLookupClass);

		UFunction* GetValueFunction = FindGeneratedFunction(ExactLookupClass, CompilerPipelineNamingTest::GeneratedFunctionName);
		if (!TestRunner->TestNotNull(TEXT("Generated-class exact-name lookup test case should expose GetValue on the exact-name class"), GetValueFunction))
		{
			return;
		}

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), ExactLookupClass, TEXT("CompilerExactNameCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Generated-class exact-name lookup test case should instantiate the exact-name class"), RuntimeObject))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, GetValueFunction, Result);
		TestRunner->TestTrue(
			TEXT("Generated-class exact-name lookup test case should execute GetValue on the exact-name class"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Generated-class exact-name lookup test case should return the expected value"),
				Result,
				42);
		}

		}

	}

};

#endif
