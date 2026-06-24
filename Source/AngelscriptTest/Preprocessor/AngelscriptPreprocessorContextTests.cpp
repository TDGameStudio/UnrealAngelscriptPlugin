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

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorContextTest,
	"Angelscript.TestModule.Preprocessor.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExplicitContextControlsFlagsAndDefaults)
	{
		using namespace PreprocessorTestHelpers;

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

		ASSERT_THAT(IsFalse(
			Module->GetClass(TEXT("UWrongContextCarrier")).IsValid(),
			TEXT("Inactive context branch should not be detected")));

		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Module->GetClass(TEXT("UExplicitContextCarrier"));
		ASSERT_THAT(IsTrue(ClassDesc.IsValid(), TEXT("Explicit context class should be detected")));

		const TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc = ClassDesc->GetMethod(TEXT("ImplicitFunction"));
		if (this->Assert.IsTrue(FunctionDesc.IsValid(), TEXT("Implicit function should be detected")))
		{
			ASSERT_THAT(IsFalse(
				FunctionDesc->bBlueprintCallable,
				TEXT("Implicit UFUNCTION should use explicit context default callable flag")));
		}

		const TSharedPtr<FAngelscriptPropertyDesc> PropertyDesc = ClassDesc->GetProperty(TEXT("ImplicitProperty"));
		if (this->Assert.IsTrue(PropertyDesc.IsValid(), TEXT("Implicit property should be detected")))
		{
			ASSERT_THAT(IsTrue(
				PropertyDesc->bEditableOnDefaults,
				TEXT("Implicit property should use explicit EditDefaultsOnly default")));
			ASSERT_THAT(IsFalse(
				PropertyDesc->bEditableOnInstance,
				TEXT("Implicit property should not be instance-editable under explicit EditDefaultsOnly default")));
			ASSERT_THAT(IsTrue(
				PropertyDesc->bBlueprintReadable,
				TEXT("Implicit property should be blueprint-readable under explicit BlueprintReadOnly default")));
			ASSERT_THAT(IsFalse(
				PropertyDesc->bBlueprintWritable,
				TEXT("Implicit property should not be blueprint-writable under explicit BlueprintReadOnly default")));
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

		ASSERT_THAT(AreEqual(
			CompatibilityPreprocessor.PreprocessorFlags.Num(),
			ExplicitPreprocessor.PreprocessorFlags.Num(),
			TEXT("Context factory should preserve compatibility constructor flag count")));

		for (const TPair<FString, bool>& Flag : CompatibilityPreprocessor.PreprocessorFlags)
		{
			const bool* ExplicitValue = ExplicitPreprocessor.PreprocessorFlags.Find(Flag.Key);
			if (this->Assert.IsNotNull(
				ExplicitValue,
				*FString::Printf(TEXT("Explicit context should contain flag %s"), *Flag.Key)))
			{
				ASSERT_THAT(AreEqual(
					Flag.Value,
					*ExplicitValue,
					*FString::Printf(TEXT("Explicit context flag %s should match compatibility constructor"), *Flag.Key)));
			}
		}

		ASSERT_THAT(AreEqual(
			CompatibilityPreprocessor.bDefaultFunctionBlueprintCallable,
			ExplicitPreprocessor.bDefaultFunctionBlueprintCallable,
			TEXT("Default function callable setting should match compatibility constructor")));
		ASSERT_THAT(AreEqual(
			CompatibilityPreprocessor.DefaultPropertyEditSpecifier,
			ExplicitPreprocessor.DefaultPropertyEditSpecifier,
			TEXT("Default class property edit setting should match compatibility constructor")));
		ASSERT_THAT(AreEqual(
			CompatibilityPreprocessor.DefaultPropertyEditSpecifierForStructs,
			ExplicitPreprocessor.DefaultPropertyEditSpecifierForStructs,
			TEXT("Default struct property edit setting should match compatibility constructor")));
		ASSERT_THAT(AreEqual(
			CompatibilityPreprocessor.DefaultPropertyBlueprintSpecifier,
			ExplicitPreprocessor.DefaultPropertyBlueprintSpecifier,
			TEXT("Default property blueprint setting should match compatibility constructor")));

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
