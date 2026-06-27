#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerNamingTest
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

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerNamingTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratedClassExactNameLookup)
	{
	using namespace CompilerNamingTest;


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerNamingTest::ModuleName.ToString());
		};

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerNamingTest::ModuleName,
			CompilerNamingTest::ScriptFilename,
			CompilerNamingTest::ScriptSource,
			true,
			Summary,
			false);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Generated-class exact-name lookup test case should compile successfully")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Generated-class exact-name lookup test case should use the preprocessor path")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Generated-class exact-name lookup test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("Generated-class exact-name lookup test case should report FullyHandled")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Generated-class exact-name lookup test case should not emit diagnostics")));
		if (!bCompiled)
		{
			return;
		}

		UPackage* Package = Engine.GetPackageInstance();
		if (!this->Assert.IsNotNull(Package, TEXT("Generated-class exact-name lookup test case should expose the engine package")))
		{
			return;
		}

		UClass* ExactLookupClass = FindObject<UClass>(Package, *CompilerNamingTest::GeneratedClassName.ToString());
		if (!this->Assert.IsNotNull(ExactLookupClass, TEXT("Generated-class exact-name lookup test case should find the generated class by its exact script name")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CompilerNamingTest::GeneratedClassName.ToString(),
			ExactLookupClass->GetName(),
			TEXT("Generated-class exact-name lookup test case should keep the exact UObject name")));

		UClass* StrippedLookupClass = FindObject<UClass>(Package, TEXT("ExactNameCarrier"));
		ASSERT_THAT(IsNull(
			StrippedLookupClass,
			TEXT("Generated-class exact-name lookup test case should not leave behind a stripped-name alias")));

		UClass* HelperLookupClass = FindGeneratedClass(&Engine, CompilerNamingTest::GeneratedClassName);
		if (!this->Assert.IsNotNull(HelperLookupClass, TEXT("Generated-class exact-name lookup test case should still resolve through FindGeneratedClass")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			HelperLookupClass == ExactLookupClass,
			TEXT("Generated-class exact-name lookup test case should have helper lookup resolve to the same exact-name object")));

		UFunction* GetValueFunction = FindGeneratedFunction(ExactLookupClass, CompilerNamingTest::GeneratedFunctionName);
		if (!this->Assert.IsNotNull(GetValueFunction, TEXT("Generated-class exact-name lookup test case should expose GetValue on the exact-name class")))
		{
			return;
		}

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), ExactLookupClass, TEXT("CompilerExactNameCarrier"));
		if (!this->Assert.IsNotNull(RuntimeObject, TEXT("Generated-class exact-name lookup test case should instantiate the exact-name class")))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, GetValueFunction, Result);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Generated-class exact-name lookup test case should execute GetValue on the exact-name class")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				42,
				Result,
				TEXT("Generated-class exact-name lookup test case should return the expected value")));
		}

		}

	}

};

#endif
