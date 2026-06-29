#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadNetworkingTests,
	"Angelscript.TestModule.HotReload.Networking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName RpcModuleName = FName(TEXT("HotReloadNetworkingRpc"));
	inline static const FString RpcFilename = FString(TEXT("HotReloadNetworkingRpc.as"));
	inline static const FName RpcClassName = FName(TEXT("AHotReloadNetworkingRpcActor"));

	inline static const FName ReplicationModuleName = FName(TEXT("HotReloadNetworkingReplication"));
	inline static const FString ReplicationFilename = FString(TEXT("HotReloadNetworkingReplication.as"));
	inline static const FName ReplicationClassName = FName(TEXT("AHotReloadNetworkingReplicationActor"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool HasReplicatedProperty(const UASClass* ScriptClass, const FProperty* ExpectedProperty)
	{
		if (ScriptClass == nullptr || ExpectedProperty == nullptr)
		{
			return false;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		ScriptClass->GetLifetimeScriptReplicationList(LifetimeProperties);
		for (const FLifetimeProperty& LifetimeProperty : LifetimeProperties)
		{
			if (ExpectedProperty->RepIndex == LifetimeProperty.RepIndex)
			{
				return true;
			}
		}

		return false;
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

	TEST_METHOD(RpcFlagsAndValidateCacheUpdateAfterFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*RpcModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadNetworkingRpcActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Client, Reliable)
				void RoutedAction(int Value)
				{
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, RpcModuleName, RpcFilename, ReloadV1Source),
			TEXT("Initial networking RPC module should compile")));

		UClass* ClassBeforeReload = FindGeneratedClass(&Engine, RpcClassName);
		ASSERT_THAT(IsNotNull(ClassBeforeReload, TEXT("RPC class should exist before reload")));

		UFunction* RoutedActionBeforeReload = FindGeneratedFunction(ClassBeforeReload, TEXT("RoutedAction"));
		ASSERT_THAT(IsNotNull(RoutedActionBeforeReload, TEXT("RoutedAction should exist before reload")));
		ASSERT_THAT(IsTrue(RoutedActionBeforeReload->HasAnyFunctionFlags(FUNC_Net), TEXT("Initial RPC should carry FUNC_Net")));
		ASSERT_THAT(IsTrue(RoutedActionBeforeReload->HasAnyFunctionFlags(FUNC_NetClient), TEXT("Initial RPC should be a Client RPC")));
		ASSERT_THAT(IsTrue(RoutedActionBeforeReload->HasAnyFunctionFlags(FUNC_NetReliable), TEXT("Initial RPC should be reliable")));
		ASSERT_THAT(IsFalse(RoutedActionBeforeReload->HasAnyFunctionFlags(FUNC_NetServer), TEXT("Initial RPC should not be a Server RPC")));
		ASSERT_THAT(IsFalse(RoutedActionBeforeReload->HasAnyFunctionFlags(FUNC_NetValidate), TEXT("Initial RPC should not carry validation")));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadNetworkingRpcActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Server, Reliable, WithValidation)
				void RoutedAction(int Value)
				{
				}

				UFUNCTION()
				bool RoutedAction_Validate(int Value)
				{
					return Value >= 0;
				}
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RpcModuleName, RpcFilename, ReloadV2Source, ReloadResult),
			TEXT("Networking RPC full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Networking RPC full reload should be handled")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, RpcClassName);
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("RPC class should exist after reload")));
		ASSERT_THAT(AreNotEqual(ClassBeforeReload, ClassAfterReload, TEXT("RPC full reload should replace the generated class")));

		UFunction* RoutedActionAfterReload = FindGeneratedFunction(ClassAfterReload, TEXT("RoutedAction"));
		UFunction* ValidateAfterReload = FindGeneratedFunction(ClassAfterReload, TEXT("RoutedAction_Validate"));
		ASSERT_THAT(IsNotNull(RoutedActionAfterReload, TEXT("RoutedAction should exist after reload")));
		ASSERT_THAT(IsNotNull(ValidateAfterReload, TEXT("RoutedAction_Validate should exist after reload")));
		ASSERT_THAT(AreNotEqual(RoutedActionBeforeReload, RoutedActionAfterReload, TEXT("RPC full reload should replace the routed UFunction")));

		ASSERT_THAT(IsTrue(RoutedActionAfterReload->HasAnyFunctionFlags(FUNC_Net), TEXT("Reloaded RPC should carry FUNC_Net")));
		ASSERT_THAT(IsTrue(RoutedActionAfterReload->HasAnyFunctionFlags(FUNC_NetServer), TEXT("Reloaded RPC should be a Server RPC")));
		ASSERT_THAT(IsTrue(RoutedActionAfterReload->HasAnyFunctionFlags(FUNC_NetReliable), TEXT("Reloaded RPC should be reliable")));
		ASSERT_THAT(IsTrue(RoutedActionAfterReload->HasAnyFunctionFlags(FUNC_NetValidate), TEXT("Reloaded RPC should carry validation")));
		ASSERT_THAT(IsFalse(RoutedActionAfterReload->HasAnyFunctionFlags(FUNC_NetClient), TEXT("Reloaded RPC should not keep the old Client flag")));
		ASSERT_THAT(IsFalse(ValidateAfterReload->HasAnyFunctionFlags(FUNC_Net), TEXT("Validation companion should not be generated as a routed RPC")));

		UASFunction* RoutedASFunction = Cast<UASFunction>(RoutedActionAfterReload);
		ASSERT_THAT(IsNotNull(RoutedASFunction, TEXT("Reloaded routed RPC should be a UASFunction")));
		ASSERT_THAT(AreEqual(ValidateAfterReload, RoutedASFunction->GetRuntimeValidateFunction(), TEXT("Reloaded RPC should resolve its newest validation companion")));

		ASSERT_THAT(IsTrue(RoutedActionBeforeReload->HasAnyFunctionFlags(FUNC_NetClient), TEXT("Old RPC should keep its original Client flag")));
		ASSERT_THAT(IsFalse(RoutedActionBeforeReload->HasAnyFunctionFlags(FUNC_NetValidate), TEXT("Old RPC should keep its original non-validated state")));
	}

	TEST_METHOD(ReplicationMetadataAndLifetimeListUpdateAfterFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ReplicationModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadNetworkingReplicationActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(Replicated)
				int Score = 1;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ReplicationModuleName, ReplicationFilename, ReloadV1Source),
			TEXT("Initial networking replication module should compile")));

		UClass* ClassBeforeReload = FindGeneratedClass(&Engine, ReplicationClassName);
		ASSERT_THAT(IsNotNull(ClassBeforeReload, TEXT("Replication class should exist before reload")));

		FProperty* ScoreBeforeReload = FindFProperty<FProperty>(ClassBeforeReload, TEXT("Score"));
		ASSERT_THAT(IsNotNull(ScoreBeforeReload, TEXT("Score should exist before reload")));
		ASSERT_THAT(IsTrue(ScoreBeforeReload->HasAnyPropertyFlags(CPF_Net), TEXT("Initial Score should be replicated")));
		ASSERT_THAT(IsFalse(ScoreBeforeReload->HasAnyPropertyFlags(CPF_RepNotify), TEXT("Initial Score should not be RepNotify")));
		ASSERT_THAT(AreEqual(COND_None, ScoreBeforeReload->GetBlueprintReplicationCondition(), TEXT("Initial Score should use unconditional replication")));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadNetworkingReplicationActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(ReplicatedUsing=OnRep_Score, ReplicationCondition=OwnerOnly)
				int Score = 2;

				UFUNCTION()
				void OnRep_Score()
				{
				}
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ReplicationModuleName, ReplicationFilename, ReloadV2Source, ReloadResult),
			TEXT("Networking replication full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Networking replication full reload should be handled")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, ReplicationClassName);
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("Replication class should exist after reload")));
		ASSERT_THAT(AreNotEqual(ClassBeforeReload, ClassAfterReload, TEXT("Replication full reload should replace the generated class")));

		FProperty* ScoreAfterReload = FindFProperty<FProperty>(ClassAfterReload, TEXT("Score"));
		UFunction* RepNotifyFunction = FindGeneratedFunction(ClassAfterReload, TEXT("OnRep_Score"));
		ASSERT_THAT(IsNotNull(ScoreAfterReload, TEXT("Score should exist after reload")));
		ASSERT_THAT(IsNotNull(RepNotifyFunction, TEXT("OnRep_Score should exist after reload")));

		ASSERT_THAT(IsTrue(ScoreAfterReload->HasAnyPropertyFlags(CPF_Net), TEXT("Reloaded Score should remain replicated")));
		ASSERT_THAT(IsTrue(ScoreAfterReload->HasAnyPropertyFlags(CPF_RepNotify), TEXT("Reloaded Score should carry RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_Score")), ScoreAfterReload->RepNotifyFunc, TEXT("Reloaded Score should point at the RepNotify function")));
		ASSERT_THAT(AreEqual(COND_OwnerOnly, ScoreAfterReload->GetBlueprintReplicationCondition(), TEXT("Reloaded Score should use the new replication condition")));
		ASSERT_THAT(IsFalse(RepNotifyFunction->HasAnyFunctionFlags(FUNC_Net), TEXT("RepNotify function should not be routed as an RPC")));

		UASClass* ScriptClassAfterReload = Cast<UASClass>(ClassAfterReload);
		ASSERT_THAT(IsNotNull(ScriptClassAfterReload, TEXT("Reloaded replication class should be a UASClass")));
		ASSERT_THAT(IsTrue(HasReplicatedProperty(ScriptClassAfterReload, ScoreAfterReload), TEXT("Reloaded lifetime replication list should include Score")));

		ASSERT_THAT(IsFalse(ScoreBeforeReload->HasAnyPropertyFlags(CPF_RepNotify), TEXT("Old Score property should keep its original non-RepNotify state")));
		ASSERT_THAT(AreEqual(COND_None, ScoreBeforeReload->GetBlueprintReplicationCondition(), TEXT("Old Score property should keep its original replication condition")));
	}
};

#endif
