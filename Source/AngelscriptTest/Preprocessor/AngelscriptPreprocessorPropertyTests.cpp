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

#if WITH_ANGELSCRIPT_UNITTESTS

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
		using namespace PreprocessorTestHelpers;

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

			ASSERT_THAT(IsFalse(
				Result.bSuccess,
				FString::Printf(TEXT("%s should fail preprocessing"), Case.Label)));
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
		using namespace PreprocessorTestHelpers;

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
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		ASSERT_THAT(IsNotNull(Settings, TEXT("Should access mutable settings")));

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
		ASSERT_THAT(IsTrue(ClassDesc.IsValid(), TEXT("Should find class descriptor")));

		const TSharedPtr<FAngelscriptPropertyDesc> ImplicitProp = ClassDesc->GetProperty(TEXT("ImplicitAccess"));
		const TSharedPtr<FAngelscriptPropertyDesc> ExplicitProp = ClassDesc->GetProperty(TEXT("ExplicitAccess"));
		ASSERT_THAT(IsTrue(ImplicitProp.IsValid(), TEXT("Should find implicit property")));
		ASSERT_THAT(IsTrue(ExplicitProp.IsValid(), TEXT("Should find explicit property")));

		// Implicit: should follow settings (BlueprintReadOnly)
		ASSERT_THAT(IsTrue(ImplicitProp->bBlueprintReadable, TEXT("Implicit property should be blueprint-readable")));
		ASSERT_THAT(IsFalse(ImplicitProp->bBlueprintWritable, TEXT("Implicit property should not be blueprint-writable")));

		// Explicit: should follow explicit specifier (BlueprintReadWrite)
		ASSERT_THAT(IsTrue(ExplicitProp->bBlueprintReadable, TEXT("Explicit property should be blueprint-readable")));
		ASSERT_THAT(IsTrue(ExplicitProp->bBlueprintWritable, TEXT("Explicit property should be blueprint-writable")));

		}
	}

	// ========================================================================
	// ShowOnActorRequiresDefaultComponent — ShowOnActor without DefaultComponent
	// fails; with DefaultComponent it succeeds and records proper metadata
	// ========================================================================
	TEST_METHOD(ShowOnActorRequiresDefaultComponent)
	{
		using namespace PreprocessorTestHelpers;

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

			ASSERT_THAT(IsFalse(Result.bSuccess, TEXT("ShowOnActor without DefaultComponent should fail")));
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
				if (this->Assert.IsTrue(ClassDesc.IsValid(), TEXT("Should find valid carrier class")))
				{
					const TSharedPtr<FAngelscriptPropertyDesc> Prop = ClassDesc->GetProperty(TEXT("RootScene"));
					if (this->Assert.IsTrue(Prop.IsValid(), TEXT("Should find RootScene property")))
					{
						ASSERT_THAT(IsTrue(Prop->bInstancedReference, TEXT("Should be instanced reference")));
						ASSERT_THAT(IsTrue(Prop->bEditableOnDefaults, TEXT("Should be editable on defaults")));
						ASSERT_THAT(IsTrue(Prop->bEditableOnInstance, TEXT("Should be editable on instances")));
						ASSERT_THAT(IsTrue(Prop->bBlueprintReadable, TEXT("Should be blueprint-readable")));

						// Check metadata
						const FString* DefaultComponentMeta = Prop->Meta.Find(FName(TEXT("DefaultComponent")));
						ASSERT_THAT(IsNotNull(DefaultComponentMeta, TEXT("Should have DefaultComponent metadata")));
						ASSERT_THAT(IsTrue(
							Prop->Meta.Contains(FName(TEXT("RootComponent"))),
							TEXT("Should have RootComponent metadata")));
					}
				}
			}
		}

		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
