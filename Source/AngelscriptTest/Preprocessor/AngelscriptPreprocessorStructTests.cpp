// ============================================================================
// AngelscriptPreprocessorStructTests.cpp
//
// Preprocessor tests for USTRUCT handling: inheritance rejection and
// default property edit specifier settings.
//
// Migrated to TEST_CLASS_WITH_FLAGS.
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Structs.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "AngelscriptSettings.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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
		using namespace PreprocessorTestHelpers;

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
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		ASSERT_THAT(IsNotNull(Settings, TEXT("Should access mutable settings")));

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
		ASSERT_THAT(IsTrue(StructDesc.IsValid(), TEXT("Should find struct descriptor")));
		ASSERT_THAT(IsTrue(StructDesc->bIsStruct, TEXT("Should be marked as struct")));
		const TSharedPtr<FAngelscriptPropertyDesc> StructProp = StructDesc->GetProperty(TEXT("StructValue"));
		ASSERT_THAT(IsTrue(StructProp.IsValid(), TEXT("Should find StructValue property")));
		ASSERT_THAT(IsTrue(StructProp->bEditableOnDefaults, TEXT("Struct prop: editable on defaults (EditDefaultsOnly)")));
		ASSERT_THAT(IsFalse(StructProp->bEditableOnInstance, TEXT("Struct prop: not editable on instances")));

		// Check class property
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Module->GetClass(TEXT("UClassDefaultSpecifierCarrier"));
		ASSERT_THAT(IsTrue(ClassDesc.IsValid(), TEXT("Should find class descriptor")));
		ASSERT_THAT(IsFalse(ClassDesc->bIsStruct, TEXT("Should not be marked as struct")));
		const TSharedPtr<FAngelscriptPropertyDesc> ClassProp = ClassDesc->GetProperty(TEXT("ClassValue"));
		ASSERT_THAT(IsTrue(ClassProp.IsValid(), TEXT("Should find ClassValue property")));
		ASSERT_THAT(IsFalse(ClassProp->bEditableOnDefaults, TEXT("Class prop: not editable on defaults (NotEditable)")));
		ASSERT_THAT(IsFalse(ClassProp->bEditableOnInstance, TEXT("Class prop: not editable on instances")));

		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
