// ============================================================================
// AngelscriptPreprocessorFunctionMacroTests.cpp
//
// Preprocessor tests for UFUNCTION macro handling: unsupported conditional
// placement, EDITOR conditional acceptance, and invalid function specifiers.
//
// Migrated from:
//   - AngelscriptPreprocessorFunctionMacroErrorTests.cpp
//
// Automation prefix: Angelscript.TestModule.Preprocessor.FunctionMacros.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorFunctionMacroTest,
	"Angelscript.TestModule.Preprocessor.FunctionMacros",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// RejectUnsupportedConditionalPlacement — UFUNCTION/UPROPERTY inside
	// #ifndef UNKNOWN_FLAG fails; inside #if EDITOR succeeds with EditorOnly meta
	// ========================================================================
	TEST_METHOD(RejectUnsupportedConditionalPlacement)
	{
		TestRunner->AddExpectedError(
			TEXT("Cannot put a UPROPERTY or UFUNCTION inside preprocessor conditions other than EDITOR or flags declared in configuration."),
			EAutomationExpectedErrorFlags::Contains, 2);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		struct FConditionalCase
		{
			const TCHAR* Label;
			const TCHAR* RelativePath;
			const TCHAR* Source;
			int32 ExpectedRow;
		};

		const TArray<FConditionalCase> InvalidCases = {
			{
				TEXT("Function in unsupported conditional"),
				TEXT("Tests/Preprocessor/FunctionMacros/InvalidConditionalFunction.as"),
				TEXT(R"(
UCLASS()
class UBadFunctionConditionalCarrier : UObject
{
#ifndef UNKNOWN_FLAG
    UFUNCTION()
    int BadFunction()
    {
        return 1;
    }
#endif
}
)"),
				5
			},
			{
				TEXT("Property in unsupported conditional"),
				TEXT("Tests/Preprocessor/FunctionMacros/InvalidConditionalProperty.as"),
				TEXT(R"(
UCLASS()
class UBadPropertyConditionalCarrier : UObject
{
#ifndef UNKNOWN_FLAG
    UPROPERTY()
    int BadValue;
#endif
}
)"),
				5
			}
		};

		for (const FConditionalCase& Case : InvalidCases)
		{
			Engine.ResetDiagnostics();
			Engine.LastEmittedDiagnostics.Empty();

			FFixtureFile File(Case.RelativePath, Case.Source);
			auto Result = RunPreprocess(Engine, File);

			TestRunner->TestFalse(
				FString::Printf(TEXT("%s should fail"), Case.Label), Result.bSuccess);
			AssertErrorCount(*TestRunner, Result, 1);
			AssertDiagnosticContains(*TestRunner, Result,
				TEXT("Cannot put a UPROPERTY or UFUNCTION inside preprocessor conditions"));
			AssertDiagnosticAt(*TestRunner, Result,
				TEXT("Cannot put a UPROPERTY or UFUNCTION"), Case.ExpectedRow, 1);
			AssertNoCompilableCode(*TestRunner, Result);
		}

		// Positive case: EDITOR conditional should succeed
		Engine.ResetDiagnostics();
		Engine.LastEmittedDiagnostics.Empty();
		const bool bOriginalUseEditorScripts = Engine.ShouldUseEditorScripts();
		Engine.SetUseEditorScriptsForTesting(true);
		ON_SCOPE_EXIT { Engine.SetUseEditorScriptsForTesting(bOriginalUseEditorScripts); };

		FFixtureFile EditorFile(TEXT("Tests/Preprocessor/FunctionMacros/EditorConditionalMembers.as"), TEXT(R"(
UCLASS()
class UEditorConditionalCarrier : UObject
{
#if EDITOR
    UPROPERTY()
    int EditorValue;

    UFUNCTION()
    int ReadEditorValue()
    {
        return 7;
    }
#endif
}
)"));

		auto EditorResult = RunPreprocess(Engine, EditorFile);

		AssertPreprocessSucceeded(*TestRunner, EditorResult);

		FAngelscriptModuleDesc* EditorModule = EditorResult.FindModule(
			TEXT("Tests.Preprocessor.FunctionMacros.EditorConditionalMembers"));
		if (TestRunner->TestNotNull(TEXT("Should find editor module"), EditorModule))
		{
			const TSharedPtr<FAngelscriptClassDesc> ClassDesc = EditorModule->GetClass(TEXT("UEditorConditionalCarrier"));
			TestRunner->TestTrue(TEXT("Should have class descriptor"), ClassDesc.IsValid());
			if (ClassDesc.IsValid())
			{
				const TSharedPtr<FAngelscriptPropertyDesc> Prop = ClassDesc->GetProperty(TEXT("EditorValue"));
				const TSharedPtr<FAngelscriptFunctionDesc> Func = ClassDesc->GetMethod(TEXT("ReadEditorValue"));
				TestRunner->TestTrue(TEXT("Should have EditorValue property"), Prop.IsValid());
				TestRunner->TestTrue(TEXT("Should have ReadEditorValue method"), Func.IsValid());
				if (Prop.IsValid())
				{
					TestRunner->TestTrue(TEXT("Property should have EditorOnly meta"),
						Prop->Meta.Contains(FName(TEXT("EditorOnly"))));
				}
				if (Func.IsValid())
				{
					TestRunner->TestTrue(TEXT("Function should have EditorOnly meta"),
						Func->Meta.Contains(FName(TEXT("EditorOnly"))));
				}
			}
		}

		}
	}

	// ========================================================================
	// InvalidSpecifiersReportDiagnostics — global BlueprintEvent, conflicting
	// Event+Override, and unknown specifiers all fail with stable messages
	// ========================================================================
	TEST_METHOD(InvalidSpecifiersReportDiagnostics)
	{
		TestRunner->AddExpectedErrorPlain(
			TEXT("Global UFUNCTION() BadGlobalEvent may not be marked BlueprintEvent."),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("UFUNCTION() Conflict cannot be both BlueprintEvent and BlueprintOverride."),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Unknown function specifier DefinitelyUnknownSpecifier on method UBadCarrier::Unknown."),
			EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		struct FSpecifierCase
		{
			const TCHAR* Label;
			const TCHAR* RelativePath;
			const TCHAR* Source;
			const TCHAR* ExpectedMessage;
			int32 ExpectedRow;
		};

		const TArray<FSpecifierCase> Cases = {
			{
				TEXT("Global BlueprintEvent"),
				TEXT("Tests/Preprocessor/FunctionMacros/InvalidSpecifierGlobalBlueprintEvent.as"),
				TEXT(R"(
UFUNCTION(BlueprintEvent)
int BadGlobalEvent()
{
    return 1;
}
)"),
				TEXT("Global UFUNCTION() BadGlobalEvent may not be marked BlueprintEvent."),
				1
			},
			{
				TEXT("Conflicting BlueprintEvent+Override"),
				TEXT("Tests/Preprocessor/FunctionMacros/InvalidSpecifierBlueprintConflict.as"),
				TEXT(R"(
UCLASS()
class UBadCarrier : UObject
{
    UFUNCTION(BlueprintEvent, BlueprintOverride)
    int Conflict()
    {
        return 1;
    }
}
)"),
				TEXT("UFUNCTION() Conflict cannot be both BlueprintEvent and BlueprintOverride."),
				4
			},
			{
				TEXT("Unknown function specifier"),
				TEXT("Tests/Preprocessor/FunctionMacros/InvalidSpecifierUnknown.as"),
				TEXT(R"(
UCLASS()
class UBadCarrier : UObject
{
    UFUNCTION(DefinitelyUnknownSpecifier)
    void Unknown()
    {
    }
}
)"),
				TEXT("Unknown function specifier DefinitelyUnknownSpecifier on method UBadCarrier::Unknown."),
				4
			}
		};

		for (const FSpecifierCase& Case : Cases)
		{
			Engine.ResetDiagnostics();
			Engine.LastEmittedDiagnostics.Empty();

			FFixtureFile File(Case.RelativePath, Case.Source);
			auto Result = RunPreprocess(Engine, File);

			TestRunner->TestFalse(
				FString::Printf(TEXT("%s should fail"), Case.Label), Result.bSuccess);
			AssertErrorCount(*TestRunner, Result, 1);
			AssertDiagnosticContains(*TestRunner, Result, Case.ExpectedMessage);
			AssertDiagnosticAt(*TestRunner, Result, Case.ExpectedMessage, Case.ExpectedRow, 1);
			AssertNoCompilableCode(*TestRunner, Result);
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
