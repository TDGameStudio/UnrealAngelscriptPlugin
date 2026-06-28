#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUFunctionTest,
	"Angelscript.TestModule.Coverage.UFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static UFunction* RequireFunction(UClass* ScriptClass, FName FunctionName)
	{
		if (ScriptClass == nullptr)
			return nullptr;

		return FindGeneratedFunction(ScriptClass, FunctionName);
	}

	static FProperty* RequireParameter(UFunction* Function, FName ParameterName)
	{
		if (Function == nullptr)
			return nullptr;

		return FindFProperty<FProperty>(Function, ParameterName);
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

	TEST_METHOD(UFunctionSpecifiersAndMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_SpecifiersAndMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionSpecifiersAndMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionSpecifiersActor : AActor
			{
				UPROPERTY()
				int StoredValue = 0;

				UFUNCTION(BlueprintCallable, Category="Coverage|Functions", CallInEditor, meta=(DisplayName="Visible Action", Keywords="coverage keyword action", ToolTip="Function tooltip text", ShortToolTip="Short function tooltip", CompactNodeTitle="ACT"))
				void VisibleAction(int Value)
				{
					StoredValue = Value;
				}

				UFUNCTION(BlueprintPure, Category="Coverage|Functions", meta=(DisplayName="Read Stored Value"))
				int ReadStoredValue() const
				{
					return StoredValue;
				}

				UFUNCTION(Exec, Category="Coverage|Console")
				void CoverageExecCommand()
				{
					StoredValue = 77;
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionSpecifiersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION specifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* VisibleAction = RequireFunction(ScriptClass, TEXT("VisibleAction"));
		ASSERT_THAT(IsNotNull(VisibleAction, TEXT("VisibleAction should be generated")));
		if (VisibleAction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VisibleAction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintCallable should set FUNC_BlueprintCallable")));
		ASSERT_THAT(IsFalse(VisibleAction->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("BlueprintCallable action should not be pure")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Functions")), VisibleAction->GetMetaData(TEXT("Category")),
			TEXT("UFUNCTION Category should be preserved")));
		ASSERT_THAT(IsTrue(VisibleAction->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor should be preserved as function metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Visible Action")), VisibleAction->GetMetaData(TEXT("DisplayName")),
			TEXT("meta DisplayName should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("coverage keyword action")), VisibleAction->GetMetaData(TEXT("Keywords")),
			TEXT("meta Keywords should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Function tooltip text")), VisibleAction->GetMetaData(TEXT("ToolTip")),
			TEXT("meta ToolTip should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Short function tooltip")), VisibleAction->GetMetaData(TEXT("ShortToolTip")),
			TEXT("meta ShortToolTip should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("ACT")), VisibleAction->GetMetaData(TEXT("CompactNodeTitle")),
			TEXT("meta CompactNodeTitle should be preserved")));

		UFunction* ReadStoredValue = RequireFunction(ScriptClass, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsNotNull(ReadStoredValue, TEXT("ReadStoredValue should be generated")));
		if (ReadStoredValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReadStoredValue->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintPure should also be BlueprintCallable for reflection")));
		ASSERT_THAT(IsTrue(ReadStoredValue->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("BlueprintPure should set FUNC_BlueprintPure")));
		ASSERT_THAT(IsTrue(ReadStoredValue->HasAnyFunctionFlags(FUNC_Const),
			TEXT("const method should set FUNC_Const")));
		ASSERT_THAT(AreEqual(FString(TEXT("Read Stored Value")), ReadStoredValue->GetMetaData(TEXT("DisplayName")),
			TEXT("BlueprintPure DisplayName metadata should be preserved")));

		UFunction* CoverageExecCommand = RequireFunction(ScriptClass, TEXT("CoverageExecCommand"));
		ASSERT_THAT(IsNotNull(CoverageExecCommand, TEXT("CoverageExecCommand should be generated")));
		if (CoverageExecCommand == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(CoverageExecCommand->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("Exec should set FUNC_Exec")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Console")), CoverageExecCommand->GetMetaData(TEXT("Category")),
			TEXT("Exec function Category should be preserved")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UFUNCTION specifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker VisibleActionInvoker(*TestRunner, Actor, TEXT("VisibleAction"));
		ASSERT_THAT(IsTrue(VisibleActionInvoker.IsValid(), TEXT("VisibleAction should be invokable through reflection")));
		if (!VisibleActionInvoker.IsValid())
		{
			return;
		}
		VisibleActionInvoker.AddParam<int32>(42);
		ASSERT_THAT(IsTrue(VisibleActionInvoker.Call(), TEXT("VisibleAction reflected invocation should succeed")));

		FFunctionInvoker ReadStoredValueInvoker(*TestRunner, Actor, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsTrue(ReadStoredValueInvoker.IsValid(), TEXT("ReadStoredValue should be invokable through reflection")));
		if (!ReadStoredValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, ReadStoredValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("BlueprintCallable reflected invocation should update state visible to the pure getter")));

		FFunctionInvoker CoverageExecInvoker(*TestRunner, Actor, TEXT("CoverageExecCommand"));
		ASSERT_THAT(IsTrue(CoverageExecInvoker.IsValid(), TEXT("CoverageExecCommand should be invokable through reflection")));
		if (!CoverageExecInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(CoverageExecInvoker.Call(), TEXT("Exec reflected invocation should succeed")));
		ASSERT_THAT(AreEqual(77, ReadStoredValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Exec reflected invocation should update script actor state")));
	}

	TEST_METHOD(UFunctionParameterMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ParameterMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionParameterMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionParameterActor : AActor
			{
				UFUNCTION(BlueprintCallable, Category="Coverage|Parameters", meta=(
					AdvancedDisplay="OptionalValue,OptionalLabel",
					DefaultToSelf="Target",
					HidePin="Target",
					AutoCreateRefTerm="OptionalLabel"))
				void ConfigureAdvanced(UObject Target, int RequiredValue, int OptionalValue, const FString&in OptionalLabel)
				{
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionParameterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION parameter metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ConfigureAdvanced = RequireFunction(ScriptClass, TEXT("ConfigureAdvanced"));
		ASSERT_THAT(IsNotNull(ConfigureAdvanced, TEXT("ConfigureAdvanced should be generated")));
		if (ConfigureAdvanced == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("OptionalValue,OptionalLabel")), ConfigureAdvanced->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("AdvancedDisplay metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), ConfigureAdvanced->GetMetaData(TEXT("DefaultToSelf")),
			TEXT("DefaultToSelf metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), ConfigureAdvanced->GetMetaData(TEXT("HidePin")),
			TEXT("HidePin metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionalLabel")), ConfigureAdvanced->GetMetaData(TEXT("AutoCreateRefTerm")),
			TEXT("AutoCreateRefTerm metadata should be preserved")));

		FProperty* TargetParam = RequireParameter(ConfigureAdvanced, TEXT("Target"));
		ASSERT_THAT(IsNotNull(TargetParam, TEXT("Target parameter should be reflected")));
		if (TargetParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(TargetParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("Target should not be marked AdvancedDisplay")));

		FProperty* RequiredValueParam = RequireParameter(ConfigureAdvanced, TEXT("RequiredValue"));
		ASSERT_THAT(IsNotNull(RequiredValueParam, TEXT("RequiredValue parameter should be reflected")));
		if (RequiredValueParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(RequiredValueParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("RequiredValue should not be marked AdvancedDisplay")));

		FProperty* OptionalValueParam = RequireParameter(ConfigureAdvanced, TEXT("OptionalValue"));
		ASSERT_THAT(IsNotNull(OptionalValueParam, TEXT("OptionalValue parameter should be reflected")));
		if (OptionalValueParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(OptionalValueParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("OptionalValue should be marked AdvancedDisplay")));

		FProperty* OptionalLabelParam = RequireParameter(ConfigureAdvanced, TEXT("OptionalLabel"));
		ASSERT_THAT(IsNotNull(OptionalLabelParam, TEXT("OptionalLabel parameter should be reflected")));
		if (OptionalLabelParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(OptionalLabelParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("OptionalLabel should be marked AdvancedDisplay")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
