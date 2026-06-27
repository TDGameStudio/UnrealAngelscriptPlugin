#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Containers/Set.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreNet.h"
#include "UObject/CoreNetTypes.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageNetworkingTests
// -----------------------------------------------------------------------------
// Coverage landing file for networking declarations that can be verified without
// a PIE multiplayer runtime. These cases validate compile-time and generated
// reflection metadata for RPCs, replicated properties, replication conditions,
// and lifetime replication lists.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptCoverageNetworkingTest
{
	static const FName RpcModuleName(TEXT("ASCoverageNetworking_RPCMetadata"));
	static const FName ReplicationModuleName(TEXT("ASCoverageNetworking_ReplicationMetadata"));
	static const FName ActorConfigModuleName(TEXT("ASCoverageNetworking_ActorConfig"));

	static FName ResolveReplicatedPropertyName(const UClass* OwnerClass, const FLifetimeProperty& LifetimeProperty)
	{
		for (TFieldIterator<FProperty> It(OwnerClass); It; ++It)
		{
			if (It->RepIndex == LifetimeProperty.RepIndex)
			{
				return It->GetFName();
			}
		}

		return NAME_None;
	}

	static TSet<FName> CollectReplicatedPropertyNames(
		const UClass* OwnerClass,
		const TArray<FLifetimeProperty>& LifetimeProperties)
	{
		TSet<FName> PropertyNames;
		for (const FLifetimeProperty& LifetimeProperty : LifetimeProperties)
		{
			const FName PropertyName = ResolveReplicatedPropertyName(OwnerClass, LifetimeProperty);
			if (PropertyName != NAME_None)
			{
				PropertyNames.Add(PropertyName);
			}
		}

		return PropertyNames;
	}

	static UFunction* RequireGeneratedFunction(FAutomationTestBase& Test, UClass* OwnerClass, FName FunctionName)
	{
		if (!Test.TestNotNull(TEXT("networking generated class should be available for function lookup"), OwnerClass))
		{
			return nullptr;
		}

		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		Test.TestNotNull(*FString::Printf(TEXT("networking function '%s' should be generated"), *FunctionName.ToString()), Function);
		return Function;
	}

	static FProperty* RequireGeneratedProperty(FAutomationTestBase& Test, UClass* OwnerClass, FName PropertyName)
	{
		if (!Test.TestNotNull(TEXT("networking generated class should be available for property lookup"), OwnerClass))
		{
			return nullptr;
		}

		FProperty* Property = FindFProperty<FProperty>(OwnerClass, PropertyName);
		Test.TestNotNull(*FString::Printf(TEXT("networking property '%s' should be generated"), *PropertyName.ToString()), Property);
		return Property;
	}

	static void AssertNetFunctionFlags(
		FAutomationTestBase& Test,
		const UFunction* Function,
		const TCHAR* Context,
		EFunctionFlags RequiredEndpointFlag,
		bool bExpectedReliable,
		bool bExpectedValidate)
	{
		if (Function == nullptr)
		{
			return;
		}

		Test.TestTrue(
			*FString::Printf(TEXT("%s should carry FUNC_Net"), Context),
			Function->HasAnyFunctionFlags(FUNC_Net));
		Test.TestTrue(
			*FString::Printf(TEXT("%s should carry the expected RPC endpoint flag"), Context),
			Function->HasAnyFunctionFlags(RequiredEndpointFlag));
		Test.TestEqual(
			*FString::Printf(TEXT("%s reliability flag should match declaration"), Context),
			Function->HasAnyFunctionFlags(FUNC_NetReliable),
			bExpectedReliable);
		Test.TestEqual(
			*FString::Printf(TEXT("%s validation flag should match declaration"), Context),
			Function->HasAnyFunctionFlags(FUNC_NetValidate),
			bExpectedValidate);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageNetworkingTest,
	"Angelscript.TestModule.Coverage.Networking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(RPCMetadataFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(AngelscriptCoverageNetworkingTest::RpcModuleName);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingRPCMetadata.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageNetworkingRPCActor : AActor
{
	default SetReplicates(true);

	UFUNCTION(Server)
	void ServerReliableAction()
	{
	}

	UFUNCTION(Server, Unreliable)
	void ServerUnreliableAction()
	{
	}

	UFUNCTION(Client)
	void ClientReliableNotify()
	{
	}

	UFUNCTION(Client, Unreliable)
	void ClientUnreliableNotify()
	{
	}

	UFUNCTION(NetMulticast)
	void MulticastReliableEvent()
	{
	}

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastUnreliableEvent()
	{
	}

	UFUNCTION(Server, WithValidation)
	void ServerValidatedAction()
	{
	}

	UFUNCTION()
	bool ServerValidatedAction_Validate()
	{
		return true;
	}
}
)AS"),
			TEXT("ACoverageNetworkingRPCActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking RPC metadata actor should compile")));

		using namespace AngelscriptCoverageNetworkingTest;
		AssertNetFunctionFlags(*TestRunner, RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ServerReliableAction")),
			TEXT("ServerReliableAction"), FUNC_NetServer, true, false);
		AssertNetFunctionFlags(*TestRunner, RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ServerUnreliableAction")),
			TEXT("ServerUnreliableAction"), FUNC_NetServer, false, false);
		AssertNetFunctionFlags(*TestRunner, RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ClientReliableNotify")),
			TEXT("ClientReliableNotify"), FUNC_NetClient, true, false);
		AssertNetFunctionFlags(*TestRunner, RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ClientUnreliableNotify")),
			TEXT("ClientUnreliableNotify"), FUNC_NetClient, false, false);
		AssertNetFunctionFlags(*TestRunner, RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("MulticastReliableEvent")),
			TEXT("MulticastReliableEvent"), FUNC_NetMulticast, true, false);
		AssertNetFunctionFlags(*TestRunner, RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("MulticastUnreliableEvent")),
			TEXT("MulticastUnreliableEvent"), FUNC_NetMulticast, false, false);
		AssertNetFunctionFlags(*TestRunner, RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ServerValidatedAction")),
			TEXT("ServerValidatedAction"), FUNC_NetServer, true, true);

		UFunction* ValidateFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ServerValidatedAction_Validate"));
		ASSERT_THAT(IsNotNull(ValidateFunction, TEXT("WithValidation should generate the validation callback")));
		ASSERT_THAT(IsTrue(ValidateFunction != nullptr && !ValidateFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("validation callback itself should not be a routed RPC")));
	}

	TEST_METHOD(ReplicatedPropertiesAndLifetimeList)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(AngelscriptCoverageNetworkingTest::ReplicationModuleName);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingReplicationMetadata.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageNetworkingReplicationActor : AActor
{
	default SetReplicates(true);

	UPROPERTY(Replicated)
	int ReplicatedScore = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Health)
	float Health = 100.0;

	UPROPERTY(Replicated, ReplicationCondition=OwnerOnly)
	int OwnerOnlyAmmo = 30;

	UPROPERTY(Replicated, ReplicationCondition=SkipReplay)
	int SkipReplayFrame = 7;

	UFUNCTION()
	void OnRep_Health()
	{
	}
}
)AS"),
			TEXT("ACoverageNetworkingReplicationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking replication metadata actor should compile")));

		using namespace AngelscriptCoverageNetworkingTest;
		FProperty* ReplicatedScoreProperty = RequireGeneratedProperty(*TestRunner, ScriptClass, TEXT("ReplicatedScore"));
		FProperty* HealthProperty = RequireGeneratedProperty(*TestRunner, ScriptClass, TEXT("Health"));
		FProperty* OwnerOnlyAmmoProperty = RequireGeneratedProperty(*TestRunner, ScriptClass, TEXT("OwnerOnlyAmmo"));
		FProperty* SkipReplayFrameProperty = RequireGeneratedProperty(*TestRunner, ScriptClass, TEXT("SkipReplayFrame"));
		if (ReplicatedScoreProperty == nullptr
			|| HealthProperty == nullptr
			|| OwnerOnlyAmmoProperty == nullptr
			|| SkipReplayFrameProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReplicatedScoreProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("ReplicatedScore should carry CPF_Net")));
		ASSERT_THAT(IsTrue(HealthProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("Health should carry CPF_Net")));
		ASSERT_THAT(IsTrue(HealthProperty->HasAnyPropertyFlags(CPF_RepNotify),
			TEXT("Health should carry CPF_RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_Health")),
			HealthProperty->RepNotifyFunc,
			TEXT("Health should preserve the RepNotify function name")));
		ASSERT_THAT(IsTrue(OwnerOnlyAmmoProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("OwnerOnlyAmmo should carry CPF_Net")));
		ASSERT_THAT(IsTrue(SkipReplayFrameProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("SkipReplayFrame should carry CPF_Net")));

		ASSERT_THAT(AreEqual(COND_None,
			ReplicatedScoreProperty->GetBlueprintReplicationCondition(),
			TEXT("unconditional replicated property should preserve COND_None")));
		ASSERT_THAT(AreEqual(COND_None,
			HealthProperty->GetBlueprintReplicationCondition(),
			TEXT("RepNotify property without condition should preserve COND_None")));
		ASSERT_THAT(AreEqual(COND_OwnerOnly,
			OwnerOnlyAmmoProperty->GetBlueprintReplicationCondition(),
			TEXT("OwnerOnlyAmmo should preserve COND_OwnerOnly")));
		ASSERT_THAT(AreEqual(COND_SkipReplay,
			SkipReplayFrameProperty->GetBlueprintReplicationCondition(),
			TEXT("SkipReplayFrame should preserve COND_SkipReplay")));

		UFunction* OnRepHealthFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("OnRep_Health"));
		ASSERT_THAT(IsNotNull(OnRepHealthFunction, TEXT("RepNotify callback should be generated")));

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptASClass, TEXT("networking replication class should be backed by UASClass")));
		if (ScriptASClass == nullptr)
		{
			return;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		ScriptASClass->GetLifetimeScriptReplicationList(LifetimeProperties);
		const TSet<FName> LifetimePropertyNames = CollectReplicatedPropertyNames(ScriptClass, LifetimeProperties);

		ASSERT_THAT(AreEqual(4, LifetimeProperties.Num(),
			TEXT("lifetime replication list should contain every script replicated property")));
		ASSERT_THAT(AreEqual(4, LifetimePropertyNames.Num(),
			TEXT("lifetime replication list entries should resolve to unique property names")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("ReplicatedScore"))),
			TEXT("lifetime replication list should include ReplicatedScore")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("Health"))),
			TEXT("lifetime replication list should include Health")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("OwnerOnlyAmmo"))),
			TEXT("lifetime replication list should include OwnerOnlyAmmo")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("SkipReplayFrame"))),
			TEXT("lifetime replication list should include SkipReplayFrame")));
	}

	TEST_METHOD(ActorReplicationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(AngelscriptCoverageNetworkingTest::ActorConfigModuleName);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingActorConfig.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageNetworkingConfigActor : AActor
{
	default SetReplicates(true);
	default SetReplicateMovement(true);

	UPROPERTY(Replicated)
	int Health = 100;
}
)AS"),
			TEXT("ACoverageNetworkingConfigActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking actor config class should compile")));

		AActor* DefaultActor = ScriptClass != nullptr ? Cast<AActor>(ScriptClass->GetDefaultObject()) : nullptr;
		ASSERT_THAT(IsNotNull(DefaultActor, TEXT("networking actor config CDO should be an actor")));
		if (DefaultActor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(DefaultActor->GetIsReplicated(),
			TEXT("default SetReplicates(true) should enable actor replication")));
		ASSERT_THAT(IsTrue(DefaultActor->IsReplicatingMovement(),
			TEXT("default SetReplicateMovement(true) should enable movement replication")));

		FProperty* HealthProperty = AngelscriptCoverageNetworkingTest::RequireGeneratedProperty(*TestRunner, ScriptClass, TEXT("Health"));
		ASSERT_THAT(IsTrue(HealthProperty != nullptr && HealthProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("actor config replicated property should carry CPF_Net")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
