// ============================================================================
// AngelscriptPreprocessorStructTests.cpp
//
// Preprocessor tests for USTRUCT handling: inheritance rejection and
// default property edit specifier settings.
//
// Migrated from:
//   - AngelscriptPreprocessorStructTests.cpp (original IMPLEMENT_SIMPLE_AUTOMATION_TEST)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Structs.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "AngelscriptSettings.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorStructTest,
	"Angelscript.TestModule.Preprocessor.Structs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// InheritanceRejected — USTRUCT with ":" inheritance syntax fails with
	// a stable diagnostic at the struct declaration line
	// ========================================================================
	TEST_METHOD(InheritanceRejected)
	{
		static const FString ExpectedMessage =
			TEXT("Error parsing script struct FDerivedStruct. Structs may not inherit from anything.");

		TestRunner->AddExpectedError(ExpectedMessage, EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/Structs/InvalidInheritance.as"), TEXT(R"(
USTRUCT() struct FDerivedStruct : FBaseStruct
{
    UPROPERTY() int Value;
}
)"));

		auto Result = RunPreprocess(Engine, File);

		AssertPreprocessFailed(*TestRunner, Result);
		AssertErrorCount(*TestRunner, Result, 1);
		AssertDiagnosticContains(*TestRunner, Result, ExpectedMessage);
		AssertDiagnosticAt(*TestRunner, Result, ExpectedMessage, 1);
		AssertModuleCount(*TestRunner, Result, 1);

		}
	}

	// ========================================================================
	// DefaultPropertySpecifierUsesStructSettings — struct properties use
	// DefaultPropertyEditSpecifierForStructs, class properties use
	// DefaultPropertyEditSpecifier
	// ========================================================================
	TEST_METHOD(DefaultPropertySpecifierUsesStructSettings)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!TestRunner->TestNotNull(TEXT("Should access mutable settings"), Settings))
		{
			return;
		}

		const EAngelscriptPropertyEditSpecifier PrevClassSpec = Settings->DefaultPropertyEditSpecifier;
		const EAngelscriptPropertyEditSpecifier PrevStructSpec = Settings->DefaultPropertyEditSpecifierForStructs;
		ON_SCOPE_EXIT
		{
			Settings->DefaultPropertyEditSpecifier = PrevClassSpec;
			Settings->DefaultPropertyEditSpecifierForStructs = PrevStructSpec;
		};

		Settings->DefaultPropertyEditSpecifier = EAngelscriptPropertyEditSpecifier::NotEditable;
		Settings->DefaultPropertyEditSpecifierForStructs = EAngelscriptPropertyEditSpecifier::EditDefaultsOnly;

		FFixtureFile File(TEXT("Tests/Preprocessor/Structs/DefaultPropertySpecifierUsesStructSettings.as"), TEXT(R"(
USTRUCT()
struct FStructDefaultSpecifierCarrier
{
    UPROPERTY() int StructValue;
}

UCLASS()
class UClassDefaultSpecifierCarrier : UObject
{
    UPROPERTY() int ClassValue;
}
)"));

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertErrorCount(*TestRunner, Session.Result, 0);
		AssertNoDiagnostics(*TestRunner, Session.Result);
		AssertModuleCount(*TestRunner, Session.Result, 1);

		FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Session.Result,
			TEXT("Tests.Preprocessor.Structs.DefaultPropertySpecifierUsesStructSettings"));
		if (Module == nullptr)
		{
			return;
		}

		// Check struct property
		const TSharedPtr<FAngelscriptClassDesc> StructDesc = Module->GetClass(TEXT("FStructDefaultSpecifierCarrier"));
		if (TestRunner->TestTrue(TEXT("Should find struct descriptor"), StructDesc.IsValid()))
		{
			TestRunner->TestTrue(TEXT("Should be marked as struct"), StructDesc->bIsStruct);
			const TSharedPtr<FAngelscriptPropertyDesc> Prop = StructDesc->GetProperty(TEXT("StructValue"));
			if (TestRunner->TestTrue(TEXT("Should find StructValue property"), Prop.IsValid()))
			{
				TestRunner->TestTrue(TEXT("Struct prop: editable on defaults (EditDefaultsOnly)"), Prop->bEditableOnDefaults);
				TestRunner->TestFalse(TEXT("Struct prop: not editable on instances"), Prop->bEditableOnInstance);
			}
		}

		// Check class property
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Module->GetClass(TEXT("UClassDefaultSpecifierCarrier"));
		if (TestRunner->TestTrue(TEXT("Should find class descriptor"), ClassDesc.IsValid()))
		{
			TestRunner->TestFalse(TEXT("Should not be marked as struct"), ClassDesc->bIsStruct);
			const TSharedPtr<FAngelscriptPropertyDesc> Prop = ClassDesc->GetProperty(TEXT("ClassValue"));
			if (TestRunner->TestTrue(TEXT("Should find ClassValue property"), Prop.IsValid()))
			{
				TestRunner->TestFalse(TEXT("Class prop: not editable on defaults (NotEditable)"), Prop->bEditableOnDefaults);
				TestRunner->TestFalse(TEXT("Class prop: not editable on instances"), Prop->bEditableOnInstance);
			}
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
