// ============================================================================
// AngelscriptPreprocessorContextTests.cpp
//
// Preprocessor tests for explicit value-style preprocessing context.
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Context.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorContextTest,
	"Angelscript.TestModule.Preprocessor.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExplicitContextControlsFlagsAndDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FAngelscriptPreprocessorContext Context = FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		Context.PreprocessorFlags.Add(TEXT("CONTEXT_ENABLED"), true);
		Context.bUseAutomaticImportMethod = false;
		Context.bDefaultFunctionBlueprintCallable = false;
		Context.DefaultPropertyEditSpecifier = EAngelscriptPropertyEditSpecifier::EditDefaultsOnly;
		Context.DefaultPropertyBlueprintSpecifier = EAngelscriptPropertyBlueprintSpecifier::BlueprintReadOnly;

		FFixtureFile File(TEXT("Tests/Preprocessor/Context/ExplicitContextControlsFlagsAndDefaults.as"), TEXT(R"(
#if CONTEXT_ENABLED
UCLASS()
class UExplicitContextCarrier : UObject
{
    UFUNCTION()
    void ImplicitFunction()
    {
    }

    UPROPERTY()
    int ImplicitProperty;
}
#else
UCLASS()
class UWrongContextCarrier : UObject
{
    UPROPERTY()
    int WrongProperty;
}
#endif
)"));

		FPreprocessSession Session = RunPreprocessSession(Engine, File, Context);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertErrorCount(*TestRunner, Session.Result, 0);
		AssertNoDiagnostics(*TestRunner, Session.Result);

		FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner,
			Session.Result,
			TEXT("Tests.Preprocessor.Context.ExplicitContextControlsFlagsAndDefaults"));
		if (Module == nullptr)
		{
			return;
		}

		TestRunner->TestFalse(
			TEXT("Inactive context branch should not be detected"),
			Module->GetClass(TEXT("UWrongContextCarrier")).IsValid());

		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Module->GetClass(TEXT("UExplicitContextCarrier"));
		if (!TestRunner->TestTrue(TEXT("Explicit context class should be detected"), ClassDesc.IsValid()))
		{
			return;
		}

		const TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc = ClassDesc->GetMethod(TEXT("ImplicitFunction"));
		if (TestRunner->TestTrue(TEXT("Implicit function should be detected"), FunctionDesc.IsValid()))
		{
			TestRunner->TestFalse(
				TEXT("Implicit UFUNCTION should use explicit context default callable flag"),
				FunctionDesc->bBlueprintCallable);
		}

		const TSharedPtr<FAngelscriptPropertyDesc> PropertyDesc = ClassDesc->GetProperty(TEXT("ImplicitProperty"));
		if (TestRunner->TestTrue(TEXT("Implicit property should be detected"), PropertyDesc.IsValid()))
		{
			TestRunner->TestTrue(
				TEXT("Implicit property should use explicit EditDefaultsOnly default"),
				PropertyDesc->bEditableOnDefaults);
			TestRunner->TestFalse(
				TEXT("Implicit property should not be instance-editable under explicit EditDefaultsOnly default"),
				PropertyDesc->bEditableOnInstance);
			TestRunner->TestTrue(
				TEXT("Implicit property should be blueprint-readable under explicit BlueprintReadOnly default"),
				PropertyDesc->bBlueprintReadable);
			TestRunner->TestFalse(
				TEXT("Implicit property should not be blueprint-writable under explicit BlueprintReadOnly default"),
				PropertyDesc->bBlueprintWritable);
		}

		}
	}

	TEST_METHOD(CurrentEngineContextMatchesCompatibilityConstructor)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FAngelscriptPreprocessor CompatibilityPreprocessor;
		const FAngelscriptPreprocessorContext Context = FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		FAngelscriptPreprocessor ExplicitPreprocessor(Context);

		TestRunner->TestEqual(
			TEXT("Context factory should preserve compatibility constructor flag count"),
			ExplicitPreprocessor.PreprocessorFlags.Num(),
			CompatibilityPreprocessor.PreprocessorFlags.Num());

		for (const TPair<FString, bool>& Flag : CompatibilityPreprocessor.PreprocessorFlags)
		{
			const bool* ExplicitValue = ExplicitPreprocessor.PreprocessorFlags.Find(Flag.Key);
			if (TestRunner->TestNotNull(
				*FString::Printf(TEXT("Explicit context should contain flag %s"), *Flag.Key),
				ExplicitValue))
			{
				TestRunner->TestEqual(
					*FString::Printf(TEXT("Explicit context flag %s should match compatibility constructor"), *Flag.Key),
					*ExplicitValue,
					Flag.Value);
			}
		}

		TestRunner->TestEqual(
			TEXT("Default function callable setting should match compatibility constructor"),
			ExplicitPreprocessor.bDefaultFunctionBlueprintCallable,
			CompatibilityPreprocessor.bDefaultFunctionBlueprintCallable);
		TestRunner->TestEqual(
			TEXT("Default class property edit setting should match compatibility constructor"),
			ExplicitPreprocessor.DefaultPropertyEditSpecifier,
			CompatibilityPreprocessor.DefaultPropertyEditSpecifier);
		TestRunner->TestEqual(
			TEXT("Default struct property edit setting should match compatibility constructor"),
			ExplicitPreprocessor.DefaultPropertyEditSpecifierForStructs,
			CompatibilityPreprocessor.DefaultPropertyEditSpecifierForStructs);
		TestRunner->TestEqual(
			TEXT("Default property blueprint setting should match compatibility constructor"),
			ExplicitPreprocessor.DefaultPropertyBlueprintSpecifier,
			CompatibilityPreprocessor.DefaultPropertyBlueprintSpecifier);

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
