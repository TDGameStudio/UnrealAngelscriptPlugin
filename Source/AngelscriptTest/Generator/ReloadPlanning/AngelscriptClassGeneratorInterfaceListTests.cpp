#include "CQTest.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptClassGeneratorInterfaceListTests,
	"Angelscript.TestModule.Generator.ReloadPlanning.InterfaceList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = FName(TEXT("ASClassGeneratorInterfaceList"));
	inline static const FString Filename = FString(TEXT("ASClassGeneratorInterfaceList.as"));
	inline static const FName ClassName = FName(TEXT("AClassGeneratorInterfaceListActor"));

	struct FReloadDecision
	{
		FAngelscriptClassGenerator::EReloadRequirement Requirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
	};

	static bool IsFullReloadDecision(const FReloadDecision& Decision)
	{
		return Decision.Requirement == FAngelscriptClassGenerator::FullReloadRequired
			|| Decision.Requirement == FAngelscriptClassGenerator::FullReloadSuggested
			|| Decision.bWantsFullReload
			|| Decision.bNeedsFullReload;
	}

	static bool AnalyzeReload(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& InitialSource,
		const FString& ReloadSource,
		FReloadDecision& OutDecision,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, InitialSource),
				*FString::Printf(TEXT("%s should compile the initial module"), Context)))
		{
			return false;
		}

		OutDecision = FReloadDecision();
		return LocalAssert.IsTrue(
			AnalyzeReloadFromMemory(
				&Engine,
				ModuleName,
				Filename,
				ReloadSource,
				OutDecision.Requirement,
				OutDecision.bWantsFullReload,
				OutDecision.bNeedsFullReload),
			*FString::Printf(TEXT("%s should analyze the reload edit"), Context));
	}

	static FString BuildActorSource(const TCHAR* InterfaceList, int32 Value)
	{
		FString Source = ASTEST_AS(R"AS(
			UCLASS()
			class AClassGeneratorInterfaceListActor : AActor__INTERFACE_LIST__
			{
				UPROPERTY()
				int Value = __VALUE__;

				UFUNCTION()
				int GetNativeValue() const
				{
					return Value;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& InOutValue)
				{
					InOutValue += Delta;
				}

				UFUNCTION()
				int GetSecondaryValue() const
				{
					return Value + 100;
				}

				UFUNCTION()
				void SetSecondaryLabel(const FString& NewLabel)
				{
				}
			}
			)AS");
		Source.ReplaceInline(TEXT("__INTERFACE_LIST__"), InterfaceList);
		Source.ReplaceInline(TEXT("__VALUE__"), *FString::FromInt(Value));
		return Source;
	}

	static bool CompileFullReload(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const FString& Source, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		ECompileResult CompileResult = ECompileResult::Error;
		if (!LocalAssert.IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, Filename, Source, CompileResult),
				*FString::Printf(TEXT("%s should compile through full reload"), Context)))
		{
			return false;
		}

		return LocalAssert.IsTrue(
			CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled,
			*FString::Printf(TEXT("%s should use a handled full-reload result"), Context));
	}

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

	TEST_METHOD(AddImplementedInterfaceRequiresFullReloadAndPublishesMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = BuildActorSource(TEXT(""), 10);
		const FString ReloadSource = BuildActorSource(TEXT(", UAngelscriptNativeParentInterface"), 20);

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, InitialSource, ReloadSource, Decision, TEXT("Interface add"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Adding an implemented native interface should request a full reload")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ReloadSource, TEXT("Interface add"))));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, ClassName);
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Interface-add reload should publish the generated class")));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Interface-add reload should publish the added interface metadata")));
	}

	TEST_METHOD(RemoveImplementedInterfaceRequiresFullReloadAndClearsMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = BuildActorSource(TEXT(", UAngelscriptNativeParentInterface"), 10);
		const FString ReloadSource = BuildActorSource(TEXT(""), 20);

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, InitialSource, ReloadSource, Decision, TEXT("Interface remove"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Removing an implemented native interface should request a full reload")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ReloadSource, TEXT("Interface remove"))));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, ClassName);
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Interface-remove reload should publish the generated class")));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(
			GeneratedClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Interface-remove reload should clear the removed interface metadata")));
	}

	TEST_METHOD(ReorderImplementedInterfacesPinsCurrentFullReloadClassification)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeSecondaryInterface::StaticClass());
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = BuildActorSource(TEXT(", UAngelscriptNativeParentInterface, UAngelscriptNativeSecondaryInterface"), 10);
		const FString ReloadSource = BuildActorSource(TEXT(", UAngelscriptNativeSecondaryInterface, UAngelscriptNativeParentInterface"), 20);

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, InitialSource, ReloadSource, Decision, TEXT("Interface reorder"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Reordering implemented interfaces should keep the current full-reload classification")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ReloadSource, TEXT("Interface reorder"))));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, ClassName);
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Interface-reorder reload should publish the generated class")));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(GeneratedClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()), TEXT("Interface-reorder reload should keep parent interface metadata")));
		ASSERT_THAT(IsTrue(GeneratedClass->ImplementsInterface(UAngelscriptNativeSecondaryInterface::StaticClass()), TEXT("Interface-reorder reload should keep secondary interface metadata")));
	}

	TEST_METHOD(BodyOnlyReloadDoesNotEscalateFromInterfaces)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = BuildActorSource(TEXT(", UAngelscriptNativeParentInterface"), 10);
		const FString ReloadSource = InitialSource;

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, InitialSource, ReloadSource, Decision, TEXT("Interface body-only"))));
		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::SoftReload, Decision.Requirement, TEXT("Body-only edit should not escalate due to unchanged interfaces")));
		ASSERT_THAT(IsFalse(Decision.bWantsFullReload, TEXT("Body-only edit should not want full reload due to unchanged interfaces")));
		ASSERT_THAT(IsFalse(Decision.bNeedsFullReload, TEXT("Body-only edit should not need full reload due to unchanged interfaces")));
	}

	TEST_METHOD(FirstCompilePublishesInterfaceMetadataWithoutOldClassComparison)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		ASSERT_THAT(IsTrue(CompileFullReload(
			*TestRunner,
			Engine,
			BuildActorSource(TEXT(", UAngelscriptNativeParentInterface"), 10),
			TEXT("Interface first compile"))));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, ClassName);
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("First interface compile should publish the generated class")));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("First interface compile should publish interface metadata")));
	}
};

#endif