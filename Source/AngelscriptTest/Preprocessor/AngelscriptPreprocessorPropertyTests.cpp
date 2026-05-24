// ============================================================================
// AngelscriptPreprocessorPropertyTests.cpp
//
// Preprocessor tests for UPROPERTY macro handling: invalid callback specifiers,
// unknown replication conditions, default blueprint access settings, and
// component specifiers (DefaultComponent/ShowOnActor).
//
// Migrated from:
//   - AngelscriptPreprocessorPropertyMacroErrorTests.cpp (InvalidSpecifiers, UnknownReplicationCondition)
//   - AngelscriptPreprocessorPropertyDefaultSpecifierTests.cpp (DefaultBlueprintAccess)
//   - AngelscriptPreprocessorComponentSpecifierTests.cpp (ShowOnActorRequiresDefaultComponent)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Properties.*
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

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorPropertyTest,
	"Angelscript.TestModule.Preprocessor.Properties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// InvalidCallbackSpecifiersReportDiagnostics — ReplicatedUsing/BlueprintSetter/
	// BlueprintGetter without callback, and unknown specifiers, all fail
	// ========================================================================
	TEST_METHOD(InvalidCallbackSpecifiersReportDiagnostics)
	{
		TestRunner->AddExpectedErrorPlain(
			TEXT("No function specified for ReplicatedUsing on property UBadPropertyCarrier::TrackedValue."),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("No function specified for BlueprintSetter on property UBadPropertyCarrier::TrackedValue."),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("No function specified for BlueprintGetter on property UBadPropertyCarrier::TrackedValue."),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Unknown property specifier DefinitelyUnknownSpecifier on property UBadPropertyCarrier::TrackedValue."),
			EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		struct FPropertyErrorCase
		{
			const TCHAR* Label;
			const TCHAR* RelativePath;
			const TCHAR* Source;
			const TCHAR* ExpectedMessage;
			int32 ExpectedRow;
		};

		const TArray<FPropertyErrorCase> Cases = {
			{
				TEXT("ReplicatedUsing without callback"),
				TEXT("Tests/Preprocessor/PropertyMacros/InvalidReplicatedUsingSpecifier.as"),
				TEXT("UCLASS()\nclass UBadPropertyCarrier : UObject\n{\n    UPROPERTY(ReplicatedUsing)\n    int TrackedValue;\n}\n"),
				TEXT("No function specified for ReplicatedUsing on property UBadPropertyCarrier::TrackedValue."),
				4
			},
			{
				TEXT("BlueprintSetter without callback"),
				TEXT("Tests/Preprocessor/PropertyMacros/InvalidBlueprintSetterSpecifier.as"),
				TEXT("UCLASS()\nclass UBadPropertyCarrier : UObject\n{\n    UPROPERTY(BlueprintSetter)\n    int TrackedValue;\n}\n"),
				TEXT("No function specified for BlueprintSetter on property UBadPropertyCarrier::TrackedValue."),
				4
			},
			{
				TEXT("BlueprintGetter without callback"),
				TEXT("Tests/Preprocessor/PropertyMacros/InvalidBlueprintGetterSpecifier.as"),
				TEXT("UCLASS()\nclass UBadPropertyCarrier : UObject\n{\n    UPROPERTY(BlueprintGetter)\n    int TrackedValue;\n}\n"),
				TEXT("No function specified for BlueprintGetter on property UBadPropertyCarrier::TrackedValue."),
				4
			},
			{
				TEXT("Unknown property specifier"),
				TEXT("Tests/Preprocessor/PropertyMacros/InvalidUnknownPropertySpecifier.as"),
				TEXT("UCLASS()\nclass UBadPropertyCarrier : UObject\n{\n    UPROPERTY(DefinitelyUnknownSpecifier)\n    int TrackedValue;\n}\n"),
				TEXT("Unknown property specifier DefinitelyUnknownSpecifier on property UBadPropertyCarrier::TrackedValue."),
				4
			}
		};

		for (const FPropertyErrorCase& Case : Cases)
		{
			Engine.ResetDiagnostics();
			Engine.LastEmittedDiagnostics.Empty();

			FFixtureFile File(Case.RelativePath, Case.Source);
			auto Result = RunPreprocess(Engine, File);

			TestRunner->TestFalse(
				FString::Printf(TEXT("%s should fail preprocessing"), Case.Label),
				Result.bSuccess);
			AssertErrorCount(*TestRunner, Result, 1);
			AssertDiagnosticContains(*TestRunner, Result, Case.ExpectedMessage);
			AssertDiagnosticAt(*TestRunner, Result, Case.ExpectedMessage, Case.ExpectedRow, 1);
			AssertNoCompilableCode(*TestRunner, Result);
		}

		}
	}

	// ========================================================================
	// UnknownReplicationConditionReportsDiagnostic — ReplicationCondition=X
	// with an invalid value fails with a stable error
	// ========================================================================
	TEST_METHOD(UnknownReplicationConditionReportsDiagnostic)
	{
		TestRunner->AddExpectedErrorPlain(
			TEXT("Unknown ReplicationCondition DefinitelyUnknown on property UBadPropertyCarrier::TrackedValue."),
			EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		Engine.ResetDiagnostics();
		Engine.LastEmittedDiagnostics.Empty();

		FFixtureFile File(TEXT("Tests/Preprocessor/PropertyMacros/InvalidUnknownReplicationConditionSpecifier.as"), TEXT(R"(
UCLASS()
class UBadPropertyCarrier : UObject
{
    UPROPERTY(Replicated, ReplicationCondition=DefinitelyUnknown)
    int TrackedValue;
}
)"));

		auto Result = RunPreprocess(Engine, File);

		AssertPreprocessFailed(*TestRunner, Result);
		AssertErrorCount(*TestRunner, Result, 1);
		AssertDiagnosticContains(*TestRunner, Result,
			TEXT("Unknown ReplicationCondition DefinitelyUnknown on property UBadPropertyCarrier::TrackedValue."));
		AssertDiagnosticAt(*TestRunner, Result,
			TEXT("Unknown ReplicationCondition"), 4, 1);
		AssertNoCompilableCode(*TestRunner, Result);

		}
	}

	// ========================================================================
	// DefaultBlueprintAccessUsesSettings — implicit UPROPERTY() uses the
	// global DefaultPropertyBlueprintSpecifier setting
	// ========================================================================
	TEST_METHOD(DefaultBlueprintAccessUsesSettings)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!TestRunner->TestNotNull(TEXT("Should access mutable settings"), Settings))
		{
			return;
		}

		const EAngelscriptPropertyBlueprintSpecifier PreviousSpecifier =
			Settings->DefaultPropertyBlueprintSpecifier;
		ON_SCOPE_EXIT { Settings->DefaultPropertyBlueprintSpecifier = PreviousSpecifier; };

		Settings->DefaultPropertyBlueprintSpecifier = EAngelscriptPropertyBlueprintSpecifier::BlueprintReadOnly;

		FFixtureFile File(TEXT("Tests/Preprocessor/Properties/DefaultBlueprintAccessUsesSettings.as"), TEXT(R"(
UCLASS()
class UBlueprintAccessDefaultSpecifierCarrier : UObject
{
    UPROPERTY() int ImplicitAccess;
    UPROPERTY(BlueprintReadWrite) int ExplicitAccess;
}
)"));

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertErrorCount(*TestRunner, Session.Result, 0);
		AssertNoDiagnostics(*TestRunner, Session.Result);
		AssertModuleCount(*TestRunner, Session.Result, 1);

		FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Session.Result,
			TEXT("Tests.Preprocessor.Properties.DefaultBlueprintAccessUsesSettings"));
		if (Module == nullptr)
		{
			return;
		}

		// Find class and properties
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Module->GetClass(TEXT("UBlueprintAccessDefaultSpecifierCarrier"));
		if (!TestRunner->TestTrue(TEXT("Should find class descriptor"), ClassDesc.IsValid()))
		{
			return;
		}

		const TSharedPtr<FAngelscriptPropertyDesc> ImplicitProp = ClassDesc->GetProperty(TEXT("ImplicitAccess"));
		const TSharedPtr<FAngelscriptPropertyDesc> ExplicitProp = ClassDesc->GetProperty(TEXT("ExplicitAccess"));
		if (!TestRunner->TestTrue(TEXT("Should find implicit property"), ImplicitProp.IsValid())
			|| !TestRunner->TestTrue(TEXT("Should find explicit property"), ExplicitProp.IsValid()))
		{
			return;
		}

		// Implicit: should follow settings (BlueprintReadOnly)
		TestRunner->TestTrue(TEXT("Implicit property should be blueprint-readable"), ImplicitProp->bBlueprintReadable);
		TestRunner->TestFalse(TEXT("Implicit property should not be blueprint-writable"), ImplicitProp->bBlueprintWritable);

		// Explicit: should follow explicit specifier (BlueprintReadWrite)
		TestRunner->TestTrue(TEXT("Explicit property should be blueprint-readable"), ExplicitProp->bBlueprintReadable);
		TestRunner->TestTrue(TEXT("Explicit property should be blueprint-writable"), ExplicitProp->bBlueprintWritable);

		}
	}

	// ========================================================================
	// ShowOnActorRequiresDefaultComponent — ShowOnActor without DefaultComponent
	// fails; with DefaultComponent it succeeds and records proper metadata
	// ========================================================================
	TEST_METHOD(ShowOnActorRequiresDefaultComponent)
	{
		TestRunner->AddExpectedErrorPlain(
			TEXT("ShowOnActor can only be used on default components in actors"),
			EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		// Invalid case: ShowOnActor without DefaultComponent
		{
			Engine.ResetDiagnostics();
			Engine.LastEmittedDiagnostics.Empty();

			FFixtureFile File(TEXT("Tests/Preprocessor/Components/ShowOnActorRequiresDefaultComponent_Invalid.as"), TEXT(R"(
UCLASS()
class AShowOnActorInvalidCarrier : AActor
{
    UPROPERTY(ShowOnActor)
    int PlainValue;
}
)"));

			auto Result = RunPreprocess(Engine, File);

			TestRunner->TestFalse(TEXT("ShowOnActor without DefaultComponent should fail"), Result.bSuccess);
			AssertErrorCount(*TestRunner, Result, 1);
			AssertDiagnosticContains(*TestRunner, Result,
				TEXT("ShowOnActor can only be used on default components in actors"));
			AssertDiagnosticAt(*TestRunner, Result,
				TEXT("ShowOnActor can only be used"), 4, 1);
		}

		// Valid case: ShowOnActor with DefaultComponent
		{
			Engine.ResetDiagnostics();
			Engine.LastEmittedDiagnostics.Empty();

			FFixtureFile File(TEXT("Tests/Preprocessor/Components/ShowOnActorRequiresDefaultComponent_Valid.as"), TEXT(R"(
UCLASS()
class AShowOnActorValidCarrier : AActor
{
    UPROPERTY(DefaultComponent, ShowOnActor, RootComponent)
    USceneComponent RootScene;
}
)"));

			auto Session = RunPreprocessSession(Engine, File);

			AssertPreprocessSucceeded(*TestRunner, Session.Result);
			AssertErrorCount(*TestRunner, Session.Result, 0);
			AssertNoDiagnostics(*TestRunner, Session.Result);

			FAngelscriptModuleDesc* Module = Session.Result.FindModule(
				TEXT("Tests.Preprocessor.Components.ShowOnActorRequiresDefaultComponent_Valid"));
			if (Module != nullptr)
			{
				const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Module->GetClass(TEXT("AShowOnActorValidCarrier"));
				if (TestRunner->TestTrue(TEXT("Should find valid carrier class"), ClassDesc.IsValid()))
				{
					const TSharedPtr<FAngelscriptPropertyDesc> Prop = ClassDesc->GetProperty(TEXT("RootScene"));
					if (TestRunner->TestTrue(TEXT("Should find RootScene property"), Prop.IsValid()))
					{
						TestRunner->TestTrue(TEXT("Should be instanced reference"), Prop->bInstancedReference);
						TestRunner->TestTrue(TEXT("Should be editable on defaults"), Prop->bEditableOnDefaults);
						TestRunner->TestTrue(TEXT("Should be editable on instances"), Prop->bEditableOnInstance);
						TestRunner->TestTrue(TEXT("Should be blueprint-readable"), Prop->bBlueprintReadable);

						// Check metadata
						const FString* DefaultComponentMeta = Prop->Meta.Find(FName(TEXT("DefaultComponent")));
						TestRunner->TestNotNull(TEXT("Should have DefaultComponent metadata"), DefaultComponentMeta);
						TestRunner->TestTrue(TEXT("Should have RootComponent metadata"),
							Prop->Meta.Contains(FName(TEXT("RootComponent"))));
					}
				}
			}
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
