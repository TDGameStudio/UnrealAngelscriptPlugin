#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "Core/AngelscriptSettings.h"
#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptTestSuite.h"

#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptTestDiscoveryTests,
	"Angelscript.TestModule.Testing.ScriptTestFramework.Discovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(NativeSuiteBaseIsAbstractTransientAndLifecycleReady)
	{
		UClass* SuiteClass = UAngelscriptTestSuite::StaticClass();
		ASSERT_THAT(IsNotNull(SuiteClass, TEXT("The native script test suite base should be registered")));
		ASSERT_THAT(IsTrue(SuiteClass->IsChildOf(UObject::StaticClass()),
			TEXT("The native script test suite base should derive from UObject")));
		ASSERT_THAT(IsTrue(SuiteClass->HasAnyClassFlags(CLASS_Abstract),
			TEXT("The native script test suite base should be abstract")));
		ASSERT_THAT(IsTrue(SuiteClass->HasAnyClassFlags(CLASS_Transient),
			TEXT("The native script test suite base should be transient")));
		ASSERT_THAT(IsNotNull(SuiteClass->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAngelscriptTestSuite, BeforeAll))));
		ASSERT_THAT(IsNotNull(SuiteClass->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAngelscriptTestSuite, BeforeEach))));
		ASSERT_THAT(IsNotNull(SuiteClass->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAngelscriptTestSuite, AfterEach))));
		ASSERT_THAT(IsNotNull(SuiteClass->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAngelscriptTestSuite, AfterAll))));
	}

	TEST_METHOD(MarkedDirectMethodsProduceStableDescriptors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		ASSERT_THAT(IsNotNull(Settings));
		const bool bPreviousDefaultCallable = Settings->bDefaultFunctionBlueprintCallable;
		Settings->bDefaultFunctionBlueprintCallable = true;
		ON_SCOPE_EXIT
		{
			Settings->bDefaultFunctionBlueprintCallable = bPreviousDefaultCallable;
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UCalculatorScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void AdditionProducesExpectedValue()
				{
				}

				void OrdinaryHelper()
				{
				}
			}

			UCLASS(Abstract, meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UAbstractScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void AbstractMethodIsNotALeaf()
				{
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UConcreteInheritedOnlyTests : UAbstractScriptTests
			{
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UUnrelatedMarkedClass : UObject
			{
				UFUNCTION(BlueprintPure, meta=(AngelscriptTest))
				int NotDerivedFromSuite()
				{
					return 0;
				}
			}

			UCLASS(Abstract)
			class UCallableParentForScriptTest : UObject
			{
				UFUNCTION(BlueprintEvent)
				void InheritedCallable()
				{
				}
			}

			UCLASS()
			class UMarkedOverrideOutsideSuite : UCallableParentForScriptTest
			{
				UFUNCTION(BlueprintOverride, meta=(AngelscriptTest))
				void InheritedCallable()
				{
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestDiscovery_Marked"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("The script-suite discovery fixture should compile")));

		const FAngelscriptScriptTestRegistryBuildResult BuildResult =
			FAngelscriptScriptTestRegistry::BuildSnapshot(Engine.GetActiveModules(), 17, true);
		ASSERT_THAT(IsNotNull(BuildResult.Snapshot.Get()));
		ASSERT_THAT(AreEqual(0, BuildResult.Diagnostics.Num(),
			TEXT("The valid discovery fixture should not emit diagnostics")));
		ASSERT_THAT(AreEqual(1, BuildResult.Snapshot->Tests.Num(),
			TEXT("Only the directly declared marked method on the concrete derived suite should be discovered")));

		const FAngelscriptScriptTestDescriptor& Descriptor = BuildResult.Snapshot->Tests[0];
		ASSERT_THAT(AreEqual(TEXT("ASTesting_ScriptTestDiscovery_Marked"), Descriptor.Id.ModuleName));
		ASSERT_THAT(AreEqual(TEXT("UCalculatorScriptTests"), Descriptor.Id.SuiteName));
		ASSERT_THAT(AreEqual(TEXT("AdditionProducesExpectedValue"), Descriptor.Id.MethodName));
		ASSERT_THAT(AreEqual(
			TEXT("Angelscript.ScriptTests.ASTesting_ScriptTestDiscovery_Marked.UCalculatorScriptTests.AdditionProducesExpectedValue"),
			Descriptor.DisplayName));
		ASSERT_THAT(AreEqual(static_cast<uint64>(17), Descriptor.Generation));
		ASSERT_THAT(AreEqual(
			EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter,
			Descriptor.Flags));
		ASSERT_THAT(IsTrue(Descriptor.SourceLine > 0));
		ASSERT_THAT(IsFalse(Descriptor.SourceFile.IsEmpty()));

		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
			Engine.GetModule(TEXT("ASTesting_ScriptTestDiscovery_Marked"));
		ASSERT_THAT(IsTrue(ModuleDesc.IsValid()));
		const TSharedPtr<FAngelscriptClassDesc> SuiteDesc =
			ModuleDesc->GetClass(TEXT("UCalculatorScriptTests"));
		ASSERT_THAT(IsTrue(SuiteDesc.IsValid()));
		const TSharedPtr<FAngelscriptFunctionDesc> MethodDesc =
			SuiteDesc->GetMethod(TEXT("AdditionProducesExpectedValue"));
		ASSERT_THAT(IsTrue(MethodDesc.IsValid()));
		ASSERT_THAT(IsTrue(MethodDesc->Meta.Contains(TEXT("AngelscriptTest")),
			TEXT("The marker should remain on the function descriptor")));
		ASSERT_THAT(IsFalse(MethodDesc->bBlueprintCallable,
			TEXT("A marked test method should not become ordinarily Blueprint-callable")));
		ASSERT_THAT(IsNotNull(MethodDesc->Function,
			TEXT("A marked test method should still materialize a reflected UFunction")));
		ASSERT_THAT(IsTrue(MethodDesc->Function->HasMetaData(TEXT("AngelscriptTest")),
			TEXT("The generated UFunction should retain the marker metadata")));
		ASSERT_THAT(IsFalse(MethodDesc->Function->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("The generated test UFunction should suppress FUNC_BlueprintCallable")));

		const TSharedPtr<FAngelscriptClassDesc> UnrelatedDesc =
			ModuleDesc->GetClass(TEXT("UUnrelatedMarkedClass"));
		ASSERT_THAT(IsTrue(UnrelatedDesc.IsValid()));
		const TSharedPtr<FAngelscriptFunctionDesc> PureMethodDesc =
			UnrelatedDesc->GetMethod(TEXT("NotDerivedFromSuite"));
		ASSERT_THAT(IsTrue(PureMethodDesc.IsValid()));
		ASSERT_THAT(IsFalse(PureMethodDesc->bBlueprintCallable));
		ASSERT_THAT(IsFalse(PureMethodDesc->bBlueprintPure,
			TEXT("A test marker must not leave an inconsistent BlueprintPure-without-callable descriptor")));
		ASSERT_THAT(IsFalse(PureMethodDesc->Function->HasAnyFunctionFlags(
			FUNC_BlueprintCallable | FUNC_BlueprintPure)));

		const TSharedPtr<FAngelscriptClassDesc> OverrideDesc =
			ModuleDesc->GetClass(TEXT("UMarkedOverrideOutsideSuite"));
		ASSERT_THAT(IsTrue(OverrideDesc.IsValid()));
		const TSharedPtr<FAngelscriptFunctionDesc> OverrideMethodDesc =
			OverrideDesc->GetMethod(TEXT("InheritedCallable"));
		ASSERT_THAT(IsTrue(OverrideMethodDesc.IsValid()));
		ASSERT_THAT(IsFalse(OverrideMethodDesc->Function->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Inherited callable flags must not re-expose a marked test method")));
	}

	TEST_METHOD(ExactAutomationFlagsValidateShapeAndTokens)
	{
		EAutomationTestFlags Flags = EAutomationTestFlags::None;
		FString Error;

		ASSERT_THAT(IsTrue(FAngelscriptScriptTestRegistry::ParseAutomationFlags(
			TEXT(" EditorContext ; ClientContext;ServerContext;CommandletContext;NonNullRHI;HighPriority;Disabled;EngineFilter "),
			Flags,
			Error)));
		ASSERT_THAT(AreEqual(
			EAutomationTestFlags::EditorContext
				| EAutomationTestFlags::ClientContext
				| EAutomationTestFlags::ServerContext
				| EAutomationTestFlags::CommandletContext
				| EAutomationTestFlags::NonNullRHI
				| EAutomationTestFlags::HighPriority
				| EAutomationTestFlags::Disabled
				| EAutomationTestFlags::EngineFilter,
			Flags));
		ASSERT_THAT(IsTrue(Error.IsEmpty()));

		const TArray<FString> InvalidValues = {
			TEXT("EditorContext;UnknownFlag;EngineFilter"),
			TEXT("EditorContext;;EngineFilter"),
			TEXT("EditorContext;EditorContext;EngineFilter"),
			TEXT("EngineFilter"),
			TEXT("EditorContext"),
			TEXT("EditorContext;EngineFilter;ProductFilter"),
			TEXT("EditorContext;None;EngineFilter"),
			TEXT("EditorContext;ApplicationContextMask;EngineFilter"),
			TEXT("EditorContext;FilterMask"),
		};

		for (const FString& InvalidValue : InvalidValues)
		{
			Flags = EAutomationTestFlags::None;
			Error.Reset();
			ASSERT_THAT(IsFalse(FAngelscriptScriptTestRegistry::ParseAutomationFlags(
				InvalidValue,
				Flags,
				Error),
				*FString::Printf(TEXT("Invalid flags should be rejected: %s"), *InvalidValue)));
			ASSERT_THAT(IsFalse(Error.IsEmpty(),
				*FString::Printf(TEXT("Invalid flags should explain the rejection: %s"), *InvalidValue)));
		}
	}

	TEST_METHOD(InvalidSuitesAndMethodsAreDiagnosedAndOmitted)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UInvalidMethodScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				int ReturnsValue()
				{
					return 1;
				}

				UFUNCTION(meta=(AngelscriptTest))
				void TakesParameter(int Value)
				{
				}
			}

			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EditorContext;EngineFilter"))
			class UInvalidFlagsScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void NeverRegistered()
				{
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestDiscovery_Invalid"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Invalid discovery declarations should compile before registry validation")));

		const FAngelscriptScriptTestRegistryBuildResult BuildResult =
			FAngelscriptScriptTestRegistry::BuildSnapshot(Engine.GetActiveModules(), 18, true);
		ASSERT_THAT(AreEqual(0, BuildResult.Snapshot->Tests.Num()));
		ASSERT_THAT(AreEqual(3, BuildResult.Diagnostics.Num(),
			TEXT("Two invalid method signatures and one invalid flag list should each produce a diagnostic")));
		for (const FAngelscriptScriptTestDiagnostic& Diagnostic : BuildResult.Diagnostics)
		{
			ASSERT_THAT(IsTrue(Diagnostic.SourceLine > 0));
			ASSERT_THAT(IsFalse(Diagnostic.SourceFile.IsEmpty()));
			ASSERT_THAT(IsFalse(Diagnostic.Message.IsEmpty()));
		}
	}

	TEST_METHOD(DisabledDiscoveryPublishesAnEmptyGeneration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FAngelscriptScriptTestRegistryBuildResult BuildResult =
			FAngelscriptScriptTestRegistry::BuildSnapshot(Engine.GetActiveModules(), 19, false);
		ASSERT_THAT(AreEqual(static_cast<uint64>(19), BuildResult.Snapshot->Generation));
		ASSERT_THAT(AreEqual(0, BuildResult.Snapshot->Tests.Num()));
		ASSERT_THAT(AreEqual(0, BuildResult.Diagnostics.Num()));
	}

	TEST_METHOD(RebuildPublishesImmutableMonotonicSnapshots)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
			class UGenerationScriptTests : UAngelscriptTestSuite
			{
				UFUNCTION(meta=(AngelscriptTest))
				void RemainsInOldSnapshot()
				{
				}
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTesting_ScriptTestDiscovery_Generation"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));

		FAngelscriptScriptTestRegistry& Registry =
			FAngelscriptScriptTestRegistry::Get();
		const FAngelscriptScriptTestRegistryBuildResult Enabled =
			Registry.Rebuild(Engine.GetActiveModules(), true);
		const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> OldSnapshot =
			Enabled.Snapshot;
		ASSERT_THAT(AreEqual(1, OldSnapshot->Tests.Num()));

		const FAngelscriptScriptTestRegistryBuildResult Disabled =
			Registry.Rebuild(Engine.GetActiveModules(), false);
		ASSERT_THAT(AreEqual(OldSnapshot->Generation + 1, Disabled.Snapshot->Generation));
		ASSERT_THAT(AreEqual(0, Disabled.Snapshot->Tests.Num()));
		ASSERT_THAT(AreEqual(1, OldSnapshot->Tests.Num(),
			TEXT("Publishing an empty generation must not mutate retained snapshots")));
		ASSERT_THAT(AreEqual(
			Disabled.Snapshot->Generation,
			Registry.GetSnapshot()->Generation));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
