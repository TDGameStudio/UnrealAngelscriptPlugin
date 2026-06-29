#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Containers/Set.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(OwnerClass, TEXT("networking generated class should be available for function lookup")))
		{
			return nullptr;
		}

		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		(void)LocalAssert.IsNotNull(Function, *FString::Printf(TEXT("networking function '%s' should be generated"), *FunctionName.ToString()));
		return Function;
	}

	static FProperty* RequireGeneratedProperty(FAutomationTestBase& Test, UClass* OwnerClass, FName PropertyName)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(OwnerClass, TEXT("networking generated class should be available for property lookup")))
		{
			return nullptr;
		}

		FProperty* Property = FindFProperty<FProperty>(OwnerClass, PropertyName);
		(void)LocalAssert.IsNotNull(Property, *FString::Printf(TEXT("networking property '%s' should be generated"), *PropertyName.ToString()));
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

		FNoDiscardAsserter LocalAssert(Test);
		(void)LocalAssert.IsTrue(
			Function->HasAnyFunctionFlags(FUNC_Net),
			*FString::Printf(TEXT("%s should carry FUNC_Net"), Context));
		(void)LocalAssert.IsTrue(
			Function->HasAnyFunctionFlags(RequiredEndpointFlag),
			*FString::Printf(TEXT("%s should carry the expected RPC endpoint flag"), Context));
		(void)LocalAssert.AreEqual(
			bExpectedReliable,
			Function->HasAnyFunctionFlags(FUNC_NetReliable),
			*FString::Printf(TEXT("%s reliability flag should match declaration"), Context));
		(void)LocalAssert.AreEqual(
			bExpectedValidate,
			Function->HasAnyFunctionFlags(FUNC_NetValidate),
			*FString::Printf(TEXT("%s validation flag should match declaration"), Context));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageNetworkingTest,
	"Angelscript.TestModule.Coverage.Networking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void CollectNonReturnParameters(const UFunction* Function, TArray<FProperty*>& OutParameters)
	{
		if (Function == nullptr)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				OutParameters.Add(*It);
			}
		}
	}

	static FProperty* FindReturnProperty(const UFunction* Function)
	{
		if (Function == nullptr)
		{
			return nullptr;
		}

		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				return *It;
			}
		}

		return nullptr;
	}

	static UEnum* FindEnumForProperty(const FProperty* Property)
	{
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			return ByteProperty->Enum;
		}

		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum();
		}

		return nullptr;
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

	TEST_METHOD(InheritedReplicationLifetimeList)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_InheritedReplicationList"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ParentClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingInheritedReplicationList.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingReplicationParent : AActor
			{
				default SetReplicates(true);

				UPROPERTY(Replicated)
				int ParentReplicatedValue = 11;

				UPROPERTY(ReplicatedUsing=OnRep_ParentTrackedValue)
				int ParentTrackedValue = 12;

				UFUNCTION()
				void OnRep_ParentTrackedValue()
				{
				}
			}

			UCLASS()
			class ACoverageNetworkingReplicationChild : ACoverageNetworkingReplicationParent
			{
				UPROPERTY(Replicated, ReplicationCondition=OwnerOnly)
				int ChildOwnerOnlyValue = 21;

				UPROPERTY(ReplicatedUsing=OnRep_ChildTrackedValue, ReplicationCondition=SkipOwner)
				int ChildTrackedValue = 22;

				UFUNCTION()
				void OnRep_ChildTrackedValue()
				{
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingReplicationParent"));
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("inherited replication parent actor should compile")));
		if (ParentClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageNetworkingReplicationChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("inherited replication child actor should compile")));
		if (ChildClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(ParentClass, ChildClass->GetSuperClass(),
			TEXT("child replication class should preserve the script parent class")));

		using namespace AngelscriptCoverageNetworkingTest;
		FProperty* ParentReplicatedValue = RequireGeneratedProperty(*TestRunner, ChildClass, TEXT("ParentReplicatedValue"));
		FProperty* ParentTrackedValue = RequireGeneratedProperty(*TestRunner, ChildClass, TEXT("ParentTrackedValue"));
		FProperty* ChildOwnerOnlyValue = RequireGeneratedProperty(*TestRunner, ChildClass, TEXT("ChildOwnerOnlyValue"));
		FProperty* ChildTrackedValue = RequireGeneratedProperty(*TestRunner, ChildClass, TEXT("ChildTrackedValue"));
		ASSERT_THAT(IsNotNull(ParentReplicatedValue, TEXT("inherited unconditional property should be visible on child")));
		ASSERT_THAT(IsNotNull(ParentTrackedValue, TEXT("inherited RepNotify property should be visible on child")));
		ASSERT_THAT(IsNotNull(ChildOwnerOnlyValue, TEXT("child OwnerOnly property should be generated")));
		ASSERT_THAT(IsNotNull(ChildTrackedValue, TEXT("child RepNotify property should be generated")));
		if (ParentReplicatedValue == nullptr
			|| ParentTrackedValue == nullptr
			|| ChildOwnerOnlyValue == nullptr
			|| ChildTrackedValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(COND_None,
			ParentReplicatedValue->GetBlueprintReplicationCondition(),
			TEXT("inherited unconditional property should preserve COND_None")));
		ASSERT_THAT(IsTrue(ParentTrackedValue->HasAnyPropertyFlags(CPF_RepNotify),
			TEXT("inherited RepNotify property should keep CPF_RepNotify on the child surface")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_ParentTrackedValue")),
			ParentTrackedValue->RepNotifyFunc,
			TEXT("inherited RepNotify property should preserve the parent callback name")));
		ASSERT_THAT(AreEqual(COND_OwnerOnly,
			ChildOwnerOnlyValue->GetBlueprintReplicationCondition(),
			TEXT("child replicated property should preserve COND_OwnerOnly")));
		ASSERT_THAT(IsTrue(ChildTrackedValue->HasAnyPropertyFlags(CPF_RepNotify),
			TEXT("child RepNotify property should carry CPF_RepNotify")));
		ASSERT_THAT(AreEqual(COND_SkipOwner,
			ChildTrackedValue->GetBlueprintReplicationCondition(),
			TEXT("child RepNotify property should preserve COND_SkipOwner")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_ChildTrackedValue")),
			ChildTrackedValue->RepNotifyFunc,
			TEXT("child RepNotify property should preserve the child callback name")));

		UASClass* ChildASClass = Cast<UASClass>(ChildClass);
		ASSERT_THAT(IsNotNull(ChildASClass, TEXT("inherited replication child should be backed by UASClass")));
		if (ChildASClass == nullptr)
		{
			return;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		ChildASClass->GetLifetimeScriptReplicationList(LifetimeProperties);
		const TSet<FName> LifetimePropertyNames = CollectReplicatedPropertyNames(ChildClass, LifetimeProperties);

		ASSERT_THAT(AreEqual(4, LifetimeProperties.Num(),
			TEXT("child lifetime replication list should include inherited and direct replicated properties")));
		ASSERT_THAT(AreEqual(4, LifetimePropertyNames.Num(),
			TEXT("child lifetime replication entries should resolve to unique names")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("ParentReplicatedValue"))),
			TEXT("child lifetime list should include inherited unconditional property")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("ParentTrackedValue"))),
			TEXT("child lifetime list should include inherited RepNotify property")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("ChildOwnerOnlyValue"))),
			TEXT("child lifetime list should include direct OwnerOnly property")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("ChildTrackedValue"))),
			TEXT("child lifetime list should include direct RepNotify property")));
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

	TEST_METHOD(ReplicationConditionsMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_ReplicationConditions"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingReplicationConditions.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingConditionsActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(Replicated, ReplicationCondition=InitialOnly)
				int InitialOnlyValue = 1;

				UPROPERTY(Replicated, ReplicationCondition=OwnerOnly)
				int OwnerOnlyValue = 2;

				UPROPERTY(Replicated, ReplicationCondition=SkipOwner)
				int SkipOwnerValue = 3;

				UPROPERTY(Replicated, ReplicationCondition=SimulatedOnly)
				int SimulatedOnlyValue = 4;

				UPROPERTY(Replicated, ReplicationCondition=AutonomousOnly)
				int AutonomousOnlyValue = 5;

				UPROPERTY(Replicated, ReplicationCondition=SimulatedOrPhysics)
				int SimulatedOrPhysicsValue = 6;

				UPROPERTY(Replicated, ReplicationCondition=InitialOrOwner)
				int InitialOrOwnerValue = 7;

				UPROPERTY(Replicated, ReplicationCondition=Custom)
				int CustomValue = 8;

				UPROPERTY(Replicated, ReplicationCondition=ReplayOrOwner)
				int ReplayOrOwnerValue = 9;

				UPROPERTY(Replicated, ReplicationCondition=ReplayOnly)
				int ReplayOnlyValue = 10;

				UPROPERTY(Replicated, ReplicationCondition=SimulatedOnlyNoReplay)
				int SimulatedOnlyNoReplayValue = 11;

				UPROPERTY(Replicated, ReplicationCondition=SimulatedOrPhysicsNoReplay)
				int SimulatedOrPhysicsNoReplayValue = 12;

				UPROPERTY(Replicated, ReplicationCondition=SkipReplay)
				int SkipReplayValue = 13;

				UPROPERTY(Replicated, ReplicationCondition=Never)
				int NeverValue = 14;
			}
			)AS"),
			TEXT("ACoverageNetworkingConditionsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking replication conditions actor should compile")));

		using namespace AngelscriptCoverageNetworkingTest;

		struct FConditionTestCase
		{
			FName PropertyName;
			ELifetimeCondition ExpectedCondition;
			const TCHAR* ConditionName;
		};

		const FConditionTestCase TestCases[] = {
			{ TEXT("InitialOnlyValue"), COND_InitialOnly, TEXT("InitialOnly") },
			{ TEXT("OwnerOnlyValue"), COND_OwnerOnly, TEXT("OwnerOnly") },
			{ TEXT("SkipOwnerValue"), COND_SkipOwner, TEXT("SkipOwner") },
			{ TEXT("SimulatedOnlyValue"), COND_SimulatedOnly, TEXT("SimulatedOnly") },
			{ TEXT("AutonomousOnlyValue"), COND_AutonomousOnly, TEXT("AutonomousOnly") },
			{ TEXT("SimulatedOrPhysicsValue"), COND_SimulatedOrPhysics, TEXT("SimulatedOrPhysics") },
			{ TEXT("InitialOrOwnerValue"), COND_InitialOrOwner, TEXT("InitialOrOwner") },
			{ TEXT("CustomValue"), COND_Custom, TEXT("Custom") },
			{ TEXT("ReplayOrOwnerValue"), COND_ReplayOrOwner, TEXT("ReplayOrOwner") },
			{ TEXT("ReplayOnlyValue"), COND_ReplayOnly, TEXT("ReplayOnly") },
			{ TEXT("SimulatedOnlyNoReplayValue"), COND_SimulatedOnlyNoReplay, TEXT("SimulatedOnlyNoReplay") },
			{ TEXT("SimulatedOrPhysicsNoReplayValue"), COND_SimulatedOrPhysicsNoReplay, TEXT("SimulatedOrPhysicsNoReplay") },
			{ TEXT("SkipReplayValue"), COND_SkipReplay, TEXT("SkipReplay") },
			{ TEXT("NeverValue"), COND_Never, TEXT("Never") },
		};

		for (const FConditionTestCase& TestCase : TestCases)
		{
			FProperty* Property = RequireGeneratedProperty(*TestRunner, ScriptClass, TestCase.PropertyName);
			if (Property != nullptr)
			{
				ASSERT_THAT(IsTrue(Property->HasAnyPropertyFlags(CPF_Net),
					*FString::Printf(TEXT("%s should carry CPF_Net"), *TestCase.PropertyName.ToString())));
				ASSERT_THAT(AreEqual(TestCase.ExpectedCondition,
					Property->GetBlueprintReplicationCondition(),
					*FString::Printf(TEXT("%s should preserve %s replication condition"),
						*TestCase.PropertyName.ToString(), TestCase.ConditionName)));
			}
		}
	}

	TEST_METHOD(RPCReliabilityVariations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_RPCReliability"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingRPCReliability.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingRPCReliabilityActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Server, Reliable)
				void ServerReliableExplicit()
				{
				}

				UFUNCTION(Server, Unreliable)
				void ServerUnreliable()
				{
				}

				UFUNCTION(Client, Reliable)
				void ClientReliableExplicit()
				{
				}

				UFUNCTION(Client, Unreliable)
				void ClientUnreliable()
				{
				}

				UFUNCTION(NetMulticast, Reliable)
				void MulticastReliableExplicit()
				{
				}

				UFUNCTION(NetMulticast, Unreliable)
				void MulticastUnreliable()
				{
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingRPCReliabilityActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking RPC reliability actor should compile")));

		using namespace AngelscriptCoverageNetworkingTest;

		struct FReliabilityTestCase
		{
			FName FunctionName;
			EFunctionFlags ExpectedEndpoint;
			bool bExpectedReliable;
			const TCHAR* Description;
		};

		const FReliabilityTestCase TestCases[] = {
			{ TEXT("ServerReliableExplicit"), FUNC_NetServer, true, TEXT("Server+Reliable should be reliable") },
			{ TEXT("ServerUnreliable"), FUNC_NetServer, false, TEXT("Server+Unreliable should be unreliable") },
			{ TEXT("ClientReliableExplicit"), FUNC_NetClient, true, TEXT("Client+Reliable should be reliable") },
			{ TEXT("ClientUnreliable"), FUNC_NetClient, false, TEXT("Client+Unreliable should be unreliable") },
			{ TEXT("MulticastReliableExplicit"), FUNC_NetMulticast, true, TEXT("NetMulticast+Reliable should be reliable") },
			{ TEXT("MulticastUnreliable"), FUNC_NetMulticast, false, TEXT("NetMulticast+Unreliable should be unreliable") },
		};

		for (const FReliabilityTestCase& TestCase : TestCases)
		{
			UFunction* Function = RequireGeneratedFunction(*TestRunner, ScriptClass, TestCase.FunctionName);
			if (Function != nullptr)
			{
				AssertNetFunctionFlags(*TestRunner, Function, TestCase.Description,
					TestCase.ExpectedEndpoint, TestCase.bExpectedReliable, false);
			}
		}
	}

	TEST_METHOD(ComplexReplicatedTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_ComplexTypes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingComplexTypes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingComplexTypesActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(Replicated)
				TArray<int> ReplicatedIntArray;

				UPROPERTY(Replicated)
				TArray<FVector> ReplicatedVectorArray;

				UPROPERTY(Replicated)
				FVector ReplicatedVector;

				UPROPERTY(Replicated)
				FRotator ReplicatedRotator;

				UPROPERTY(Replicated)
				FTransform ReplicatedTransform;

				UPROPERTY(Replicated)
				FString ReplicatedString;

				UPROPERTY(Replicated)
				FName ReplicatedName;

				UPROPERTY(Replicated)
				FText ReplicatedText;

				UPROPERTY(ReplicatedUsing=OnRep_ReplicatedActor)
				AActor ReplicatedActorRef;

				UFUNCTION()
				void OnRep_ReplicatedActor()
				{
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingComplexTypesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking complex types actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		using namespace AngelscriptCoverageNetworkingTest;

		const FName PropertyNames[] = {
			TEXT("ReplicatedIntArray"),
			TEXT("ReplicatedVectorArray"),
			TEXT("ReplicatedVector"),
			TEXT("ReplicatedRotator"),
			TEXT("ReplicatedTransform"),
			TEXT("ReplicatedString"),
			TEXT("ReplicatedName"),
			TEXT("ReplicatedText"),
			TEXT("ReplicatedActorRef"),
		};

		for (const FName& PropertyName : PropertyNames)
		{
			FProperty* Property = RequireGeneratedProperty(*TestRunner, ScriptClass, PropertyName);
			if (Property != nullptr)
			{
				ASSERT_THAT(IsTrue(Property->HasAnyPropertyFlags(CPF_Net),
					*FString::Printf(TEXT("%s should carry CPF_Net"), *PropertyName.ToString())));
			}
		}

		FTextProperty* ReplicatedTextProperty = FindFProperty<FTextProperty>(ScriptClass, TEXT("ReplicatedText"));
		ASSERT_THAT(IsNotNull(ReplicatedTextProperty,
			TEXT("ReplicatedText should be reflected as FTextProperty")));
		if (ReplicatedTextProperty == nullptr)
		{
			return;
		}

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptASClass,
			TEXT("networking complex types class should be backed by UASClass")));
		if (ScriptASClass == nullptr)
		{
			return;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		ScriptASClass->GetLifetimeScriptReplicationList(LifetimeProperties);
		const TSet<FName> LifetimePropertyNames = CollectReplicatedPropertyNames(ScriptClass, LifetimeProperties);
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("ReplicatedText"))),
			TEXT("lifetime replication list should include ReplicatedText")));

		FProperty* ActorRefProperty = FindFProperty<FProperty>(ScriptClass, TEXT("ReplicatedActorRef"));
		if (ActorRefProperty != nullptr)
		{
			ASSERT_THAT(IsTrue(ActorRefProperty->HasAnyPropertyFlags(CPF_RepNotify),
				TEXT("ReplicatedActorRef should carry CPF_RepNotify")));
			ASSERT_THAT(AreEqual(FName(TEXT("OnRep_ReplicatedActor")),
				ActorRefProperty->RepNotifyFunc,
				TEXT("ReplicatedActorRef should preserve RepNotify function name")));
		}
	}

	TEST_METHOD(ActorOwnerAndRelevancySettings)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_OwnerRelevancy"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingOwnerRelevancy.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingOwnerRelevancyActor : AActor
			{
				default SetReplicates(true);
				default bOnlyRelevantToOwner = true;
				default bAlwaysRelevant = true;
				default bNetUseOwnerRelevancy = true;
				default NetPriority = 3.5f;
				default SetNetUpdateFrequency(24.0f);
				default SetMinNetUpdateFrequency(6.0f);
				default SetNetCullDistanceSquared(4096.0f);

				UPROPERTY()
				bool bOwnerRoundTrip = false;

				UPROPERTY()
				bool bFrequencyRoundTrip = false;

				UFUNCTION()
				void ExerciseOwnerAndRelevancy(AActor NewOwner)
				{
					SetOwner(NewOwner);
					bOwnerRoundTrip = GetOwner() == NewOwner;
					SetNetUpdateFrequency(12.0f);
					SetMinNetUpdateFrequency(4.0f);
					SetNetCullDistanceSquared(1024.0f);
					bFrequencyRoundTrip =
						GetNetUpdateFrequency() == 12.0f
						&& GetMinNetUpdateFrequency() == 4.0f
						&& GetNetCullDistanceSquared() == 1024.0f;
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingOwnerRelevancyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking owner/relevancy actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		AActor* DefaultActor = Cast<AActor>(ScriptClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(DefaultActor, TEXT("networking owner/relevancy CDO should be an actor")));
		if (DefaultActor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(DefaultActor->GetIsReplicated(),
			TEXT("owner/relevancy actor should be configured for replication")));
		ASSERT_THAT(IsTrue(DefaultActor->bOnlyRelevantToOwner,
			TEXT("bOnlyRelevantToOwner should be settable from AS defaults")));
		ASSERT_THAT(IsTrue(DefaultActor->bAlwaysRelevant,
			TEXT("bAlwaysRelevant should be settable from AS defaults")));
		ASSERT_THAT(IsTrue(DefaultActor->bNetUseOwnerRelevancy,
			TEXT("bNetUseOwnerRelevancy should be settable from AS defaults")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DefaultActor->NetPriority, 3.5f),
			TEXT("NetPriority should be settable from AS defaults")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DefaultActor->GetNetUpdateFrequency(), 24.0f),
			TEXT("SetNetUpdateFrequency default should configure owner/relevancy CDO")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DefaultActor->GetMinNetUpdateFrequency(), 6.0f),
			TEXT("SetMinNetUpdateFrequency default should configure owner/relevancy CDO")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DefaultActor->GetNetCullDistanceSquared(), 4096.0f),
			TEXT("SetNetCullDistanceSquared default should configure owner/relevancy CDO")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& OwnerActor = Spawner.SpawnActor<AActor>();

		AActor* ScriptActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptActor, TEXT("owner/relevancy script actor should spawn")));
		if (ScriptActor == nullptr)
		{
			return;
		}

		FFunctionInvoker ExerciseInvoker(*TestRunner, ScriptActor, TEXT("ExerciseOwnerAndRelevancy"));
		ASSERT_THAT(IsTrue(ExerciseInvoker.IsValid(), TEXT("owner/relevancy exercise function should be invokable")));
		if (!ExerciseInvoker.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExerciseInvoker.AddParam<AActor*>(&OwnerActor).Call(),
			TEXT("owner/relevancy exercise function should execute")));

		ASSERT_THAT(AreEqual(&OwnerActor, ScriptActor->GetOwner(),
			TEXT("SetOwner/GetOwner should round-trip the network owner through AS")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ScriptActor->GetNetUpdateFrequency(), 12.0f),
			TEXT("SetNetUpdateFrequency should round-trip at runtime through AS")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ScriptActor->GetMinNetUpdateFrequency(), 4.0f),
			TEXT("SetMinNetUpdateFrequency should round-trip at runtime through AS")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ScriptActor->GetNetCullDistanceSquared(), 1024.0f),
			TEXT("SetNetCullDistanceSquared should round-trip at runtime through AS")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, ScriptActor, TEXT("bOwnerRoundTrip"), true,
			TEXT("AS should observe SetOwner/GetOwner network-owner state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, ScriptActor, TEXT("bFrequencyRoundTrip"), true,
			TEXT("AS should observe net relevancy tuning getter/setter state"))));
	}

	TEST_METHOD(MultipleRPCsInSingleClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_MultipleRPCs"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingMultipleRPCs.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingMultiRPCActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Server, Reliable)
				void ServerAction1()
				{
				}

				UFUNCTION(Server, Reliable)
				void ServerAction2()
				{
				}

				UFUNCTION(Client, Reliable)
				void ClientNotify1()
				{
				}

				UFUNCTION(Client, Reliable)
				void ClientNotify2()
				{
				}

				UFUNCTION(NetMulticast, Unreliable)
				void MulticastEvent1()
				{
				}

				UFUNCTION(NetMulticast, Unreliable)
				void MulticastEvent2()
				{
				}

				UFUNCTION(Server, Reliable, WithValidation)
				void ServerValidated1()
				{
				}

				UFUNCTION()
				bool ServerValidated1_Validate()
				{
					return true;
				}

				UFUNCTION(Server, Reliable, WithValidation)
				void ServerValidated2()
				{
				}

				UFUNCTION()
				bool ServerValidated2_Validate()
				{
					return true;
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingMultiRPCActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking multiple RPCs actor should compile")));

		using namespace AngelscriptCoverageNetworkingTest;

		const FName ServerFunctions[] = { TEXT("ServerAction1"), TEXT("ServerAction2"), TEXT("ServerValidated1"), TEXT("ServerValidated2") };
		const FName ClientFunctions[] = { TEXT("ClientNotify1"), TEXT("ClientNotify2") };
		const FName MulticastFunctions[] = { TEXT("MulticastEvent1"), TEXT("MulticastEvent2") };
		const FName ValidationFunctions[] = { TEXT("ServerValidated1_Validate"), TEXT("ServerValidated2_Validate") };

		for (const FName& FuncName : ServerFunctions)
		{
			UFunction* Function = RequireGeneratedFunction(*TestRunner, ScriptClass, FuncName);
			if (Function != nullptr)
			{
				ASSERT_THAT(IsTrue(Function->HasAnyFunctionFlags(FUNC_NetServer),
					*FString::Printf(TEXT("%s should be a Server RPC"), *FuncName.ToString())));
			}
		}

		for (const FName& FuncName : ClientFunctions)
		{
			UFunction* Function = RequireGeneratedFunction(*TestRunner, ScriptClass, FuncName);
			if (Function != nullptr)
			{
				ASSERT_THAT(IsTrue(Function->HasAnyFunctionFlags(FUNC_NetClient),
					*FString::Printf(TEXT("%s should be a Client RPC"), *FuncName.ToString())));
			}
		}

		for (const FName& FuncName : MulticastFunctions)
		{
			UFunction* Function = RequireGeneratedFunction(*TestRunner, ScriptClass, FuncName);
			if (Function != nullptr)
			{
				ASSERT_THAT(IsTrue(Function->HasAnyFunctionFlags(FUNC_NetMulticast),
					*FString::Printf(TEXT("%s should be a NetMulticast RPC"), *FuncName.ToString())));
			}
		}

		for (const FName& FuncName : ValidationFunctions)
		{
			UFunction* Function = RequireGeneratedFunction(*TestRunner, ScriptClass, FuncName);
			if (Function != nullptr)
			{
				ASSERT_THAT(IsFalse(Function->HasAnyFunctionFlags(FUNC_Net),
					*FString::Printf(TEXT("%s should not be a network RPC itself"), *FuncName.ToString())));
			}
		}
	}

	TEST_METHOD(RPCWithParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_RPCParams"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingRPCParams.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingRPCParamsActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Server, Reliable)
				void ServerActionWithInt(int Value)
				{
				}

				UFUNCTION(Server, Reliable)
				void ServerActionWithMultipleParams(int Value, float Rate, FVector Location)
				{
				}

				UFUNCTION(Client, Reliable)
				void ClientNotifyWithString(FString Message)
				{
				}

				UFUNCTION(NetMulticast, Unreliable)
				void MulticastEventWithLocation(FVector Location, FRotator Rotation)
				{
				}

				UFUNCTION(Server, Reliable, WithValidation)
				void ServerValidatedWithParams(int Damage, AActor Target)
				{
				}

				UFUNCTION()
				bool ServerValidatedWithParams_Validate(int Damage, AActor Target)
				{
					return Damage >= 0 && Target != nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingRPCParamsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking RPC params actor should compile")));

		using namespace AngelscriptCoverageNetworkingTest;

		struct FRPCParamTestCase
		{
			FName FunctionName;
			int32 ExpectedParamCount;
			EFunctionFlags ExpectedEndpoint;
		};

		const FRPCParamTestCase TestCases[] = {
			{ TEXT("ServerActionWithInt"), 1, FUNC_NetServer },
			{ TEXT("ServerActionWithMultipleParams"), 3, FUNC_NetServer },
			{ TEXT("ClientNotifyWithString"), 1, FUNC_NetClient },
			{ TEXT("MulticastEventWithLocation"), 2, FUNC_NetMulticast },
			{ TEXT("ServerValidatedWithParams"), 2, FUNC_NetServer },
		};

		for (const FRPCParamTestCase& TestCase : TestCases)
		{
			UFunction* Function = RequireGeneratedFunction(*TestRunner, ScriptClass, TestCase.FunctionName);
			if (Function != nullptr)
			{
				ASSERT_THAT(IsTrue(Function->HasAnyFunctionFlags(FUNC_Net),
					*FString::Printf(TEXT("%s should be a network function"), *TestCase.FunctionName.ToString())));
				ASSERT_THAT(IsTrue(Function->HasAnyFunctionFlags(TestCase.ExpectedEndpoint),
					*FString::Printf(TEXT("%s should have correct endpoint flag"), *TestCase.FunctionName.ToString())));

				int32 ParamCount = 0;
				for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
				{
					if (!(It->PropertyFlags & CPF_ReturnParm))
					{
						ParamCount++;
					}
				}

				ASSERT_THAT(AreEqual(TestCase.ExpectedParamCount, ParamCount,
					*FString::Printf(TEXT("%s should have %d parameters"),
						*TestCase.FunctionName.ToString(), TestCase.ExpectedParamCount)));
			}
		}
	}

	TEST_METHOD(RPCValidationSignatureMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_RPCValidationSignature"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingRPCValidationSignature.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingRPCValidationSignatureActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Server, Reliable, WithValidation)
				void ServerValidatedPayload(int Damage, float Scale, FVector HitLocation, AActor Target)
				{
				}

				UFUNCTION()
				bool ServerValidatedPayload_Validate(int Damage, float Scale, FVector HitLocation, AActor Target)
				{
					return Damage >= 0 && Scale >= 0.0f && Target != nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingRPCValidationSignatureActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("RPC validation signature actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		using namespace AngelscriptCoverageNetworkingTest;
		UFunction* ServerFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ServerValidatedPayload"));
		UFunction* ValidateFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ServerValidatedPayload_Validate"));
		ASSERT_THAT(IsNotNull(ServerFunction, TEXT("validated server RPC should be generated")));
		ASSERT_THAT(IsNotNull(ValidateFunction, TEXT("validated server RPC companion should be generated")));
		if (ServerFunction == nullptr || ValidateFunction == nullptr)
		{
			return;
		}

		AssertNetFunctionFlags(*TestRunner, ServerFunction, TEXT("ServerValidatedPayload"),
			FUNC_NetServer, true, true);
		ASSERT_THAT(IsFalse(ValidateFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("validation companion should not be generated as a routed RPC")));

		UASFunction* ServerASFunction = Cast<UASFunction>(ServerFunction);
		ASSERT_THAT(IsNotNull(ServerASFunction, TEXT("validated server RPC should be a UASFunction")));
		if (ServerASFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(ValidateFunction, ServerASFunction->GetRuntimeValidateFunction(),
			TEXT("validated server RPC should cache its _Validate companion")));

		TArray<FProperty*> ServerParameters;
		TArray<FProperty*> ValidateParameters;
		CollectNonReturnParameters(ServerFunction, ServerParameters);
		CollectNonReturnParameters(ValidateFunction, ValidateParameters);
		ASSERT_THAT(AreEqual(4, ServerParameters.Num(),
			TEXT("validated server RPC should expose all payload parameters")));
		ASSERT_THAT(AreEqual(ServerParameters.Num(), ValidateParameters.Num(),
			TEXT("validation companion should mirror server RPC parameter count")));
		if (ServerParameters.Num() != ValidateParameters.Num())
		{
			return;
		}

		for (int32 Index = 0; Index < ServerParameters.Num(); ++Index)
		{
			FProperty* ServerParameter = ServerParameters[Index];
			FProperty* ValidateParameter = ValidateParameters[Index];
			ASSERT_THAT(IsNotNull(ServerParameter,
				*FString::Printf(TEXT("server RPC parameter %d should be reflected"), Index)));
			ASSERT_THAT(IsNotNull(ValidateParameter,
				*FString::Printf(TEXT("validate RPC parameter %d should be reflected"), Index)));
			if (ServerParameter == nullptr || ValidateParameter == nullptr)
			{
				return;
			}

			ASSERT_THAT(AreEqual(ServerParameter->GetFName(), ValidateParameter->GetFName(),
				*FString::Printf(TEXT("validation parameter %d should preserve the RPC parameter name"), Index)));
			ASSERT_THAT(AreEqual(ServerParameter->GetClass(), ValidateParameter->GetClass(),
				*FString::Printf(TEXT("validation parameter %d should preserve the RPC property class"), Index)));
			ASSERT_THAT(AreEqual(ServerParameter->GetCPPType(), ValidateParameter->GetCPPType(),
				*FString::Printf(TEXT("validation parameter %d should preserve the RPC cpp type"), Index)));
		}

		FProperty* ValidateReturnProperty = FindReturnProperty(ValidateFunction);
		ASSERT_THAT(IsNotNull(ValidateReturnProperty,
			TEXT("validation companion should expose a return property")));
		if (ValidateReturnProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ValidateReturnProperty->IsA<FBoolProperty>(),
			TEXT("validation companion should return bool")));
	}

	TEST_METHOD(ReplicatedPropertiesWithDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_ReplicatedDefaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingReplicatedDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingDefaultsActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(Replicated)
				int Health = 100;

				UPROPERTY(Replicated)
				float Speed = 600.0f;

				UPROPERTY(Replicated)
				bool bIsAlive = true;

				UPROPERTY(Replicated)
				FString PlayerName = "DefaultPlayer";

				UPROPERTY(ReplicatedUsing=OnRep_Score)
				int Score = 0;

				UFUNCTION()
				void OnRep_Score()
				{
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking replicated defaults actor should compile")));

		AActor* DefaultActor = ScriptClass != nullptr ? Cast<AActor>(ScriptClass->GetDefaultObject()) : nullptr;
		ASSERT_THAT(IsNotNull(DefaultActor, TEXT("networking defaults actor CDO should exist")));

		using namespace AngelscriptCoverageNetworkingTest;

		const FName PropertyNames[] = {
			TEXT("Health"),
			TEXT("Speed"),
			TEXT("bIsAlive"),
			TEXT("PlayerName"),
			TEXT("Score"),
		};

		for (const FName& PropertyName : PropertyNames)
		{
			FProperty* Property = RequireGeneratedProperty(*TestRunner, ScriptClass, PropertyName);
			if (Property != nullptr)
			{
				ASSERT_THAT(IsTrue(Property->HasAnyPropertyFlags(CPF_Net),
					*FString::Printf(TEXT("%s should be replicated"), *PropertyName.ToString())));
			}
		}
	}

	TEST_METHOD(NetworkRoleAndModeEnums)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageNetworking_RoleAndModeEnums", ASTEST_AS(R"AS(
			int NetworkRoleEnumValues()
			{
				return int(ENetRole::ROLE_None)
					+ int(ENetRole::ROLE_SimulatedProxy)
					+ int(ENetRole::ROLE_AutonomousProxy)
					+ int(ENetRole::ROLE_Authority);
			}

			bool AuthorityRoleComparison()
			{
				return int(ENetRole::ROLE_Authority) > int(ENetRole::ROLE_AutonomousProxy)
					&& int(ENetRole::ROLE_AutonomousProxy) > int(ENetRole::ROLE_SimulatedProxy)
					&& int(ENetRole::ROLE_SimulatedProxy) > int(ENetRole::ROLE_None);
			}

			int NetworkModeEnumValues()
			{
				return int(ENetMode::NM_Standalone)
					+ int(ENetMode::NM_DedicatedServer)
					+ int(ENetMode::NM_ListenServer)
					+ int(ENetMode::NM_Client);
			}

			bool NetworkModeComparisons()
			{
				return ENetMode::NM_Client != ENetMode::NM_Standalone
					&& ENetMode::NM_DedicatedServer != ENetMode::NM_ListenServer;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("networking role and mode enum module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		FAngelscriptTestExecutor RoleSumExecutor(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int NetworkRoleEnumValues()"));
		const int32 ExpectedRoleSum = static_cast<int32>(ROLE_None)
			+ static_cast<int32>(ROLE_SimulatedProxy)
			+ static_cast<int32>(ROLE_AutonomousProxy)
			+ static_cast<int32>(ROLE_Authority);
		ASSERT_THAT(AreEqual(ExpectedRoleSum, RoleSumExecutor.ExecuteAndGet<int32>(INDEX_NONE),
			TEXT("ENetRole should expose all four role enum values to AS")));

		FAngelscriptTestExecutor RoleComparisonExecutor(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool AuthorityRoleComparison()"));
		ASSERT_THAT(IsTrue(RoleComparisonExecutor.ExecuteAndGet<bool>(false),
			TEXT("ENetRole should support authority/client-side ordering checks in AS")));

		FAngelscriptTestExecutor ModeSumExecutor(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int NetworkModeEnumValues()"));
		const int32 ExpectedModeSum = static_cast<int32>(NM_Standalone)
			+ static_cast<int32>(NM_DedicatedServer)
			+ static_cast<int32>(NM_ListenServer)
			+ static_cast<int32>(NM_Client);
		ASSERT_THAT(AreEqual(ExpectedModeSum, ModeSumExecutor.ExecuteAndGet<int32>(INDEX_NONE),
			TEXT("ENetMode should expose Standalone, DedicatedServer, ListenServer, and Client values to AS")));

		FAngelscriptTestExecutor ModeComparisonExecutor(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool NetworkModeComparisons()"));
		ASSERT_THAT(IsTrue(ModeComparisonExecutor.ExecuteAndGet<bool>(false),
			TEXT("ENetMode should support mode comparisons in AS")));
	}

#if !WITH_ANGELSCRIPT_HAZE
	TEST_METHOD(PawnControllerAndLocalControlQueries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_PawnControllerQueries"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingPawnControllerQueries.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingPawnControllerQueries : APawn
			{
				UFUNCTION()
				int QueryUnpossessedPawnControllerState()
				{
					int Mask = 0;

					if (GetController() == null)
						Mask |= 1;
					if (GetPlayerController() == null)
						Mask |= 2;
					if (!IsLocallyControlled())
						Mask |= 4;
					if (!IsPlayerControlled())
						Mask |= 8;
					if (!IsBotControlled())
						Mask |= 16;
					if (GetPlayerState() == null)
						Mask |= 32;

					return Mask;
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingPawnControllerQueries"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("pawn controller query actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		APawn* ScriptPawn = SpawnScriptActor<APawn>(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptPawn, TEXT("pawn controller query actor should spawn as APawn")));
		if (ScriptPawn == nullptr)
		{
			return;
		}

		FFunctionInvoker QueryInvoker(*TestRunner, ScriptPawn, TEXT("QueryUnpossessedPawnControllerState"));
		ASSERT_THAT(IsTrue(QueryInvoker.IsValid(),
			TEXT("pawn controller query function should be invokable through the reflected helper")));
		if (!QueryInvoker.IsValid())
		{
			return;
		}

		int32 ExpectedMask = 0;
		if (ScriptPawn->GetController() == nullptr)
		{
			ExpectedMask |= 1;
		}
		if (Cast<APlayerController>(ScriptPawn->GetController()) == nullptr)
		{
			ExpectedMask |= 2;
		}
		if (!ScriptPawn->IsLocallyControlled())
		{
			ExpectedMask |= 4;
		}
		if (!ScriptPawn->IsPlayerControlled())
		{
			ExpectedMask |= 8;
		}
		if (!ScriptPawn->IsBotControlled())
		{
			ExpectedMask |= 16;
		}
		if (ScriptPawn->GetPlayerState() == nullptr)
		{
			ExpectedMask |= 32;
		}

		ASSERT_THAT(AreEqual(ExpectedMask, QueryInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("AS should expose APawn controller, PlayerController, PlayerState, and local-control queries")));
	}

	TEST_METHOD(WorldGameStateAndServerTravelSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageNetworking_WorldGameStateSurface", ASTEST_AS(R"AS(
			bool QueryWorldGameState(UWorld World)
			{
				if (World == null)
					return false;

				AGameStateBase GameState = World.GetGameState();
				return GameState == World.GetGameState();
			}

			bool QueryServerTravelSurface(UWorld World)
			{
				if (World == null)
					return false;

				return World.ServerTravel("/Game/Maps/NetworkingCoverage", false, true);
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("world GameState and ServerTravel surface module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(Module->GetFunctionByName("QueryWorldGameState"),
			TEXT("UWorld.GetGameState should be visible to AS for GameState access coverage")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByName("QueryServerTravelSurface"),
			TEXT("UWorld.ServerTravel should be visible to AS for server travel coverage")));
	}

	TEST_METHOD(GameModeGameStateAndPlayerStateStaticSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_GameFrameworkStaticSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* GameModeClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingGameFrameworkStaticSurface.as"),
			ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class ACoverageNetworkingStaticGameMode : AGameModeBase
			{
				UPROPERTY()
				int LoginCount = 0;

				UFUNCTION()
				void RecordPostLogin(APlayerController NewPlayer)
				{
					if (NewPlayer != nullptr)
						LoginCount += 1;
				}

				UFUNCTION()
				void RecordLogout(AController Exiting)
				{
					if (Exiting != nullptr)
						LoginCount -= 1;
				}
			}

			UCLASS(Blueprintable)
			class ACoverageNetworkingStaticGameState : AGameStateBase
			{
				UPROPERTY(Replicated)
				int MatchSeconds = 0;

				UPROPERTY(ReplicatedUsing=OnRep_TeamScore)
				int TeamScore = 0;

				UFUNCTION()
				void OnRep_TeamScore()
				{
				}
			}

			UCLASS(Blueprintable)
			class ACoverageNetworkingStaticPlayerState : APlayerState
			{
				UPROPERTY(Replicated)
				int ScoreBucket = 0;

				UPROPERTY(ReplicatedUsing=OnRep_DisplayName)
				FString DisplayName = "Player";

				UFUNCTION()
				void OnRep_DisplayName()
				{
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingStaticGameMode"));
		ASSERT_THAT(IsNotNull(GameModeClass, TEXT("networking static GameMode class should compile")));
		if (GameModeClass == nullptr)
		{
			return;
		}

		UClass* GameStateClass = FindGeneratedClass(&Engine, TEXT("ACoverageNetworkingStaticGameState"));
		UClass* PlayerStateClass = FindGeneratedClass(&Engine, TEXT("ACoverageNetworkingStaticPlayerState"));
		ASSERT_THAT(IsNotNull(GameStateClass, TEXT("networking static GameState class should compile")));
		ASSERT_THAT(IsNotNull(PlayerStateClass, TEXT("networking static PlayerState class should compile")));
		if (GameStateClass == nullptr || PlayerStateClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(GameModeClass->IsChildOf(AGameModeBase::StaticClass()),
			TEXT("AS GameMode should derive from AGameModeBase")));
		ASSERT_THAT(IsTrue(GameStateClass->IsChildOf(AGameStateBase::StaticClass()),
			TEXT("AS GameState should derive from AGameStateBase")));
		ASSERT_THAT(IsTrue(PlayerStateClass->IsChildOf(APlayerState::StaticClass()),
			TEXT("AS PlayerState should derive from APlayerState")));

		using namespace AngelscriptCoverageNetworkingTest;
		UFunction* RecordPostLoginFunction = RequireGeneratedFunction(*TestRunner, GameModeClass, TEXT("RecordPostLogin"));
		UFunction* RecordLogoutFunction = RequireGeneratedFunction(*TestRunner, GameModeClass, TEXT("RecordLogout"));
		ASSERT_THAT(IsNotNull(RecordPostLoginFunction,
			TEXT("GameMode login-style function should be generated with PlayerController parameter")));
		ASSERT_THAT(IsNotNull(RecordLogoutFunction,
			TEXT("GameMode logout-style function should be generated with Controller parameter")));
		if (RecordPostLoginFunction == nullptr || RecordLogoutFunction == nullptr)
		{
			return;
		}

		TArray<FProperty*> LoginParameters;
		TArray<FProperty*> LogoutParameters;
		CollectNonReturnParameters(RecordPostLoginFunction, LoginParameters);
		CollectNonReturnParameters(RecordLogoutFunction, LogoutParameters);
		ASSERT_THAT(AreEqual(1, LoginParameters.Num(),
			TEXT("GameMode login-style function should expose one PlayerController parameter")));
		ASSERT_THAT(AreEqual(1, LogoutParameters.Num(),
			TEXT("GameMode logout-style function should expose one Controller parameter")));
		if (LoginParameters.Num() != 1 || LogoutParameters.Num() != 1)
		{
			return;
		}

		FObjectProperty* LoginPlayerProperty = CastField<FObjectProperty>(LoginParameters[0]);
		FObjectProperty* LogoutControllerProperty = CastField<FObjectProperty>(LogoutParameters[0]);
		ASSERT_THAT(IsNotNull(LoginPlayerProperty,
			TEXT("GameMode login-style parameter should be an object property")));
		ASSERT_THAT(IsNotNull(LogoutControllerProperty,
			TEXT("GameMode logout-style parameter should be an object property")));
		if (LoginPlayerProperty == nullptr || LogoutControllerProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(APlayerController::StaticClass(), LoginPlayerProperty->PropertyClass,
			TEXT("GameMode login-style parameter should preserve APlayerController type")));
		ASSERT_THAT(AreEqual(AController::StaticClass(), LogoutControllerProperty->PropertyClass,
			TEXT("GameMode logout-style parameter should preserve AController type")));

		const struct FReplicatedClassCase
		{
			UClass* ScriptClass;
			FName ReplicatedName;
			FName RepNotifyName;
			FName RepNotifyFunctionName;
			const TCHAR* Context;
		} ReplicatedClasses[] = {
			{ GameStateClass, TEXT("MatchSeconds"), TEXT("TeamScore"), TEXT("OnRep_TeamScore"), TEXT("GameState") },
			{ PlayerStateClass, TEXT("ScoreBucket"), TEXT("DisplayName"), TEXT("OnRep_DisplayName"), TEXT("PlayerState") },
		};

		for (const FReplicatedClassCase& TestCase : ReplicatedClasses)
		{
			FProperty* ReplicatedProperty = RequireGeneratedProperty(*TestRunner, TestCase.ScriptClass, TestCase.ReplicatedName);
			FProperty* RepNotifyProperty = RequireGeneratedProperty(*TestRunner, TestCase.ScriptClass, TestCase.RepNotifyName);
			UFunction* RepNotifyFunction = RequireGeneratedFunction(*TestRunner, TestCase.ScriptClass, TestCase.RepNotifyFunctionName);
			ASSERT_THAT(IsNotNull(ReplicatedProperty,
				*FString::Printf(TEXT("%s replicated property should be generated"), TestCase.Context)));
			ASSERT_THAT(IsNotNull(RepNotifyProperty,
				*FString::Printf(TEXT("%s RepNotify property should be generated"), TestCase.Context)));
			ASSERT_THAT(IsNotNull(RepNotifyFunction,
				*FString::Printf(TEXT("%s RepNotify function should be generated"), TestCase.Context)));
			if (ReplicatedProperty == nullptr || RepNotifyProperty == nullptr || RepNotifyFunction == nullptr)
			{
				continue;
			}

			ASSERT_THAT(IsTrue(ReplicatedProperty->HasAnyPropertyFlags(CPF_Net),
				*FString::Printf(TEXT("%s replicated property should carry CPF_Net"), TestCase.Context)));
			ASSERT_THAT(IsTrue(RepNotifyProperty->HasAnyPropertyFlags(CPF_Net),
				*FString::Printf(TEXT("%s RepNotify property should carry CPF_Net"), TestCase.Context)));
			ASSERT_THAT(IsTrue(RepNotifyProperty->HasAnyPropertyFlags(CPF_RepNotify),
				*FString::Printf(TEXT("%s RepNotify property should carry CPF_RepNotify"), TestCase.Context)));
			ASSERT_THAT(AreEqual(TestCase.RepNotifyFunctionName,
				RepNotifyProperty->RepNotifyFunc,
				*FString::Printf(TEXT("%s RepNotify property should preserve callback name"), TestCase.Context)));
			ASSERT_THAT(IsFalse(RepNotifyFunction->HasAnyFunctionFlags(FUNC_Net),
				*FString::Printf(TEXT("%s RepNotify function should not be a routed RPC"), TestCase.Context)));

			UASClass* ScriptASClass = Cast<UASClass>(TestCase.ScriptClass);
			ASSERT_THAT(IsNotNull(ScriptASClass,
				*FString::Printf(TEXT("%s class should be backed by UASClass"), TestCase.Context)));
			if (ScriptASClass == nullptr)
			{
				continue;
			}

			TArray<FLifetimeProperty> LifetimeProperties;
			ScriptASClass->GetLifetimeScriptReplicationList(LifetimeProperties);
			const TSet<FName> LifetimePropertyNames = CollectReplicatedPropertyNames(TestCase.ScriptClass, LifetimeProperties);
			ASSERT_THAT(AreEqual(2, LifetimeProperties.Num(),
				*FString::Printf(TEXT("%s lifetime list should include both replicated properties"), TestCase.Context)));
			ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(TestCase.ReplicatedName),
				*FString::Printf(TEXT("%s lifetime list should include the plain replicated property"), TestCase.Context)));
			ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(TestCase.RepNotifyName),
				*FString::Printf(TEXT("%s lifetime list should include the RepNotify property"), TestCase.Context)));
		}
	}
#endif

#if !WITH_ANGELSCRIPT_HAZE
	TEST_METHOD(WorldNetModeQueryIsVisible)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageNetworking_WorldNetModeQuery", ASTEST_AS(R"AS(
			bool IsNetMode(UWorld World, ENetMode ExpectedMode)
			{
				return World != null && World.GetNetMode() == ExpectedMode;
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("world NetMode query module should compile when UWorld.GetNetMode is bound")));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* IsNetModeFunction = Module->GetFunctionByName("IsNetMode");
		ASSERT_THAT(IsNotNull(IsNetModeFunction,
			TEXT("UWorld.GetNetMode visibility should compile into a callable AS helper")));
	}
#else
	TEST_METHOD(WorldNetModeQueryUnsupportedInHaze)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> ExpectedDiagnostics = {
			TEXT("GetNetMode")
		};
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageNetworking_WorldNetModeUnsupported"),
			ASTEST_AS(R"AS(
				bool IsCurrentWorldClient()
				{
					UWorld World = GetCurrentWorld();
					return World != null && World.GetNetMode() == ENetMode::NM_Client;
				}
				)AS"),
			TEXT("UWorld.GetNetMode should remain an explicit AS binding boundary under WITH_ANGELSCRIPT_HAZE"),
			ExpectedDiagnostics),
			TEXT("UWorld.GetNetMode should fail to compile when the Haze build excludes the binding")));
	}
#endif

	TEST_METHOD(ActorNetworkRoleQueriesAreVisible)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_ActorRoleQueries"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingActorRoleQueries.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingRoleQueryActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION()
				int QueryRemoteRoleAfterCheckingLocalAuthority()
				{
					ENetRole LocalRole = GetLocalRole();
					if (!HasAuthority()
						|| LocalRole != ENetRole::ROLE_Authority
						|| int(LocalRole) <= int(ENetRole::ROLE_AutonomousProxy))
					{
						return -1;
					}

					return int(GetRemoteRole());
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingRoleQueryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("networking role query actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		using namespace AngelscriptCoverageNetworkingTest;
		UFunction* CheckRoleQueriesFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("QueryRemoteRoleAfterCheckingLocalAuthority"));
		ASSERT_THAT(IsNotNull(CheckRoleQueriesFunction, TEXT("role query test function should be generated")));
		if (CheckRoleQueriesFunction == nullptr)
		{
			return;
		}

		UFunction* NativeGetLocalRoleFunction = AActor::StaticClass()->FindFunctionByName(TEXT("GetLocalRole"));
		UFunction* NativeGetRemoteRoleFunction = AActor::StaticClass()->FindFunctionByName(TEXT("GetRemoteRole"));
		UFunction* NativeHasAuthorityFunction = AActor::StaticClass()->FindFunctionByName(TEXT("HasAuthority"));
		ASSERT_THAT(IsNotNull(NativeGetLocalRoleFunction, TEXT("AActor.GetLocalRole should be reflected")));
		ASSERT_THAT(IsNotNull(NativeGetRemoteRoleFunction, TEXT("AActor.GetRemoteRole should be reflected")));
		ASSERT_THAT(IsNotNull(NativeHasAuthorityFunction, TEXT("AActor.HasAuthority should be reflected")));
		if (NativeGetLocalRoleFunction == nullptr
			|| NativeGetRemoteRoleFunction == nullptr
			|| NativeHasAuthorityFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(NativeGetLocalRoleFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("AActor.GetLocalRole should be BlueprintCallable and eligible for AS binding")));
		ASSERT_THAT(IsTrue(NativeGetRemoteRoleFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("AActor.GetRemoteRole should be BlueprintCallable and eligible for AS binding")));
		ASSERT_THAT(IsTrue(NativeHasAuthorityFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("AActor.HasAuthority should be BlueprintCallable and eligible for AS binding")));

		AActor* DefaultActor = Cast<AActor>(ScriptClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(DefaultActor, TEXT("networking role query CDO should be an actor")));
		if (DefaultActor == nullptr)
		{
			return;
		}

		FFunctionInvoker CheckRoleQueriesInvoker(*TestRunner, DefaultActor, TEXT("QueryRemoteRoleAfterCheckingLocalAuthority"));
		ASSERT_THAT(IsTrue(CheckRoleQueriesInvoker.IsValid(),
			TEXT("role query test function should be invokable through the reflected helper")));
		if (!CheckRoleQueriesInvoker.IsValid())
		{
			return;
		}

		const int32 ActualRemoteRole = CheckRoleQueriesInvoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(static_cast<int32>(DefaultActor->GetRemoteRole()), ActualRemoteRole,
			TEXT("AS should call HasAuthority, GetLocalRole, and GetRemoteRole on a replicated actor CDO")));
	}

	TEST_METHOD(ActorAuthorityQueryBranchesExecuteHeadless)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_AuthorityQueryBranches"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingAuthorityQueryBranches.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingAuthorityBranchActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY()
				int LastRoleMask = 0;

				UFUNCTION()
				int QueryAuthorityRoleMask()
				{
					int Mask = 0;
					ENetRole LocalRole = GetLocalRole();

					if (HasAuthority())
						Mask |= 1;
					if (LocalRole == ENetRole::ROLE_Authority)
						Mask |= 2;
					if (int(LocalRole) < int(ENetRole::ROLE_Authority))
						Mask |= 4;
					if (GetRemoteRole() == ENetRole::ROLE_None)
						Mask |= 8;

					LastRoleMask = Mask;
					return Mask;
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingAuthorityBranchActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("authority branch actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor* ScriptActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptActor, TEXT("authority branch actor should spawn in a headless test world")));
		if (ScriptActor == nullptr)
		{
			return;
		}

		FFunctionInvoker QueryInvoker(*TestRunner, ScriptActor, TEXT("QueryAuthorityRoleMask"));
		ASSERT_THAT(IsTrue(QueryInvoker.IsValid(),
			TEXT("authority role branch function should be invokable")));
		if (!QueryInvoker.IsValid())
		{
			return;
		}

		int32 ExpectedMask = 0;
		const ENetRole LocalRole = ScriptActor->GetLocalRole();
		if (ScriptActor->HasAuthority())
		{
			ExpectedMask |= 1;
		}
		if (LocalRole == ROLE_Authority)
		{
			ExpectedMask |= 2;
		}
		if (LocalRole < ROLE_Authority)
		{
			ExpectedMask |= 4;
		}
		if (ScriptActor->GetRemoteRole() == ROLE_None)
		{
			ExpectedMask |= 8;
		}

		const int32 ActualMask = QueryInvoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(ExpectedMask, ActualMask,
			TEXT("AS authority and client-role branch checks should match native actor role state")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ScriptActor, TEXT("LastRoleMask"), ExpectedMask,
			TEXT("AS should store the authority role mask through a reflected property"))));
	}

	TEST_METHOD(ActorNetworkRolePropertiesUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> RoleDiagnostics = { TEXT("Role") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageNetworking_RolePropertyUnsupported"),
			ASTEST_AS(R"AS(
				bool ProbeRoleProperty(AActor Actor)
				{
					return Actor.Role == ENetRole::ROLE_Authority;
				}
				)AS"),
			TEXT("AActor.Role should remain an explicit AS binding boundary"),
			RoleDiagnostics),
			TEXT("unbound AActor.Role should fail to compile as a documented boundary")));

		const TArray<FString> RemoteRoleDiagnostics = { TEXT("RemoteRole") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageNetworking_RemoteRolePropertyUnsupported"),
			ASTEST_AS(R"AS(
				bool ProbeRemoteRoleProperty(AActor Actor)
				{
					return Actor.RemoteRole == ENetRole::ROLE_AutonomousProxy;
				}
				)AS"),
			TEXT("AActor.RemoteRole should remain an explicit AS binding boundary"),
			RemoteRoleDiagnostics),
			TEXT("unbound AActor.RemoteRole should fail to compile as a documented boundary")));
	}

	TEST_METHOD(ReplicationMetadataStaticSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_StaticReplicationSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingStaticReplicationSurface.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingStaticReplicationActor : AActor
			{
				default SetReplicates(true);
				default SetReplicateMovement(true);

				UPROPERTY(Replicated)
				int UnconditionalValue = 1;

				UPROPERTY(ReplicatedUsing=OnRep_TrackedValue)
				int TrackedValue = 2;

				UPROPERTY(Replicated, ReplicationCondition=InitialOnly)
				int InitialOnlyValue = 3;

				UPROPERTY(Replicated, ReplicationCondition=OwnerOnly)
				int OwnerOnlyValue = 4;

				UPROPERTY(Replicated, ReplicationCondition=SkipOwner)
				int SkipOwnerValue = 5;

				UPROPERTY(Replicated, ReplicationCondition=AutonomousOnly)
				int AutonomousOnlyValue = 6;

				UPROPERTY(Replicated, ReplicationCondition=ReplayOrOwner)
				int ReplayOrOwnerValue = 7;

				UPROPERTY(Replicated, ReplicationCondition=Custom)
				int CustomValue = 8;

				UFUNCTION()
				void OnRep_TrackedValue()
				{
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingStaticReplicationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("static replication surface actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		AActor* DefaultActor = Cast<AActor>(ScriptClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(DefaultActor, TEXT("static replication surface CDO should be an actor")));
		if (DefaultActor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(DefaultActor->GetIsReplicated(),
			TEXT("static replication surface should enable bReplicates on the CDO")));
		ASSERT_THAT(IsTrue(DefaultActor->IsReplicatingMovement(),
			TEXT("static replication surface should enable bReplicateMovement on the CDO")));

		using namespace AngelscriptCoverageNetworkingTest;

		struct FReplicationStaticCase
		{
			FName PropertyName;
			ELifetimeCondition ExpectedCondition;
			bool bExpectedRepNotify;
		};

		const FReplicationStaticCase TestCases[] = {
			{ TEXT("UnconditionalValue"), COND_None, false },
			{ TEXT("TrackedValue"), COND_None, true },
			{ TEXT("InitialOnlyValue"), COND_InitialOnly, false },
			{ TEXT("OwnerOnlyValue"), COND_OwnerOnly, false },
			{ TEXT("SkipOwnerValue"), COND_SkipOwner, false },
			{ TEXT("AutonomousOnlyValue"), COND_AutonomousOnly, false },
			{ TEXT("ReplayOrOwnerValue"), COND_ReplayOrOwner, false },
			{ TEXT("CustomValue"), COND_Custom, false },
		};

		for (const FReplicationStaticCase& TestCase : TestCases)
		{
			FProperty* Property = RequireGeneratedProperty(*TestRunner, ScriptClass, TestCase.PropertyName);
			ASSERT_THAT(IsNotNull(Property,
				*FString::Printf(TEXT("%s should be generated"), *TestCase.PropertyName.ToString())));
			if (Property == nullptr)
			{
				continue;
			}

			ASSERT_THAT(IsTrue(Property->HasAnyPropertyFlags(CPF_Net),
				*FString::Printf(TEXT("%s should carry CPF_Net"), *TestCase.PropertyName.ToString())));
			ASSERT_THAT(AreEqual(TestCase.ExpectedCondition,
				Property->GetBlueprintReplicationCondition(),
				*FString::Printf(TEXT("%s should preserve its replication condition"), *TestCase.PropertyName.ToString())));
			ASSERT_THAT(AreEqual(TestCase.bExpectedRepNotify,
				Property->HasAnyPropertyFlags(CPF_RepNotify),
				*FString::Printf(TEXT("%s RepNotify flag should match its specifier"), *TestCase.PropertyName.ToString())));
		}

		FProperty* TrackedValueProperty = RequireGeneratedProperty(*TestRunner, ScriptClass, TEXT("TrackedValue"));
		ASSERT_THAT(IsNotNull(TrackedValueProperty, TEXT("TrackedValue property should be available for RepNotify assertions")));
		if (TrackedValueProperty != nullptr)
		{
			ASSERT_THAT(AreEqual(FName(TEXT("OnRep_TrackedValue")),
				TrackedValueProperty->RepNotifyFunc,
				TEXT("ReplicatedUsing should preserve the RepNotify callback name")));
		}

		UFunction* RepNotifyFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("OnRep_TrackedValue"));
		ASSERT_THAT(IsNotNull(RepNotifyFunction, TEXT("RepNotify callback should be generated")));
		if (RepNotifyFunction != nullptr)
		{
			ASSERT_THAT(IsFalse(RepNotifyFunction->HasAnyFunctionFlags(FUNC_Net),
				TEXT("RepNotify callback should not be generated as a routed RPC")));
		}

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptASClass, TEXT("static replication surface class should be backed by UASClass")));
		if (ScriptASClass == nullptr)
		{
			return;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		ScriptASClass->GetLifetimeScriptReplicationList(LifetimeProperties);
		const TSet<FName> LifetimePropertyNames = CollectReplicatedPropertyNames(ScriptClass, LifetimeProperties);

		ASSERT_THAT(AreEqual(static_cast<int32>(UE_ARRAY_COUNT(TestCases)), LifetimeProperties.Num(),
			TEXT("lifetime replication list should include every static replicated property")));
		for (const FReplicationStaticCase& TestCase : TestCases)
		{
			ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(TestCase.PropertyName),
				*FString::Printf(TEXT("lifetime replication list should include %s"), *TestCase.PropertyName.ToString())));
		}
	}

	TEST_METHOD(RPCSpecifierFlagsAreExclusiveStaticMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_RPCSpecifierExclusivity"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingRPCSpecifierExclusivity.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingRPCSpecifierActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Server, Reliable, WithValidation)
				void ServerValidatedReliable(int Payload)
				{
				}

				UFUNCTION()
				bool ServerValidatedReliable_Validate(int Payload)
				{
					return Payload >= 0;
				}

				UFUNCTION(Server, Unreliable)
				void ServerUnreliableAction()
				{
				}

				UFUNCTION(Client, Reliable)
				void ClientReliableAction()
				{
				}

				UFUNCTION(NetMulticast, Unreliable)
				void MulticastUnreliableAction()
				{
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingRPCSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("RPC specifier exclusivity actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		using namespace AngelscriptCoverageNetworkingTest;

		struct FRPCStaticCase
		{
			FName FunctionName;
			EFunctionFlags ExpectedEndpointFlag;
			EFunctionFlags ForbiddenEndpointFlagA;
			EFunctionFlags ForbiddenEndpointFlagB;
			bool bExpectedReliable;
			bool bExpectedValidate;
		};

		const FRPCStaticCase TestCases[] = {
			{ TEXT("ServerValidatedReliable"), FUNC_NetServer, FUNC_NetClient, FUNC_NetMulticast, true, true },
			{ TEXT("ServerUnreliableAction"), FUNC_NetServer, FUNC_NetClient, FUNC_NetMulticast, false, false },
			{ TEXT("ClientReliableAction"), FUNC_NetClient, FUNC_NetServer, FUNC_NetMulticast, true, false },
			{ TEXT("MulticastUnreliableAction"), FUNC_NetMulticast, FUNC_NetServer, FUNC_NetClient, false, false },
		};

		for (const FRPCStaticCase& TestCase : TestCases)
		{
			UFunction* Function = RequireGeneratedFunction(*TestRunner, ScriptClass, TestCase.FunctionName);
			ASSERT_THAT(IsNotNull(Function,
				*FString::Printf(TEXT("%s should be generated"), *TestCase.FunctionName.ToString())));
			if (Function == nullptr)
			{
				continue;
			}

			AssertNetFunctionFlags(*TestRunner, Function, *TestCase.FunctionName.ToString(),
				TestCase.ExpectedEndpointFlag, TestCase.bExpectedReliable, TestCase.bExpectedValidate);
			ASSERT_THAT(IsFalse(Function->HasAnyFunctionFlags(TestCase.ForbiddenEndpointFlagA),
				*FString::Printf(TEXT("%s should not carry another RPC endpoint flag"), *TestCase.FunctionName.ToString())));
			ASSERT_THAT(IsFalse(Function->HasAnyFunctionFlags(TestCase.ForbiddenEndpointFlagB),
				*FString::Printf(TEXT("%s should not carry another RPC endpoint flag"), *TestCase.FunctionName.ToString())));
		}

		UFunction* ValidateFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("ServerValidatedReliable_Validate"));
		ASSERT_THAT(IsNotNull(ValidateFunction, TEXT("WithValidation should generate a non-RPC validation callback")));
		if (ValidateFunction != nullptr)
		{
			ASSERT_THAT(IsFalse(ValidateFunction->HasAnyFunctionFlags(FUNC_Net),
				TEXT("validation callback should not carry FUNC_Net")));
			ASSERT_THAT(IsFalse(ValidateFunction->HasAnyFunctionFlags(FUNC_NetValidate),
				TEXT("validation callback should not itself carry FUNC_NetValidate")));
		}
	}

#if !WITH_ANGELSCRIPT_HAZE
	TEST_METHOD(NetworkConsoleAndClientTravelBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> GlobalConsoleDiagnostics = { TEXT("ConsoleCommand") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageNetworking_GlobalNetConsoleUnsupported"),
			ASTEST_AS(R"AS(
				void TryNetworkConsoleCommands()
				{
					ConsoleCommand("Net PktLag=100");
					ConsoleCommand("Net PktLoss=10");
					ConsoleCommand("stat net");
					ConsoleCommand("stat netgraph");
					ConsoleCommand("Log LogNet Verbose");
					ConsoleCommand("Log LogNetTraffic Verbose");
					ConsoleCommand("Log LogRep Verbose");
				}
				)AS"),
			TEXT("network console execution helpers should remain outside direct AS exposure"),
			MakeArrayView(GlobalConsoleDiagnostics)),
			TEXT("global network console command execution should compile-fail at the documented AS boundary")));

		const TArray<FString> PlayerControllerConsoleDiagnostics = { TEXT("ConsoleCommand") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageNetworking_PlayerControllerNetConsoleUnsupported"),
			ASTEST_AS(R"AS(
				void TryPlayerControllerNetworkConsoleCommands(APlayerController Controller)
				{
					Controller.ConsoleCommand("Net PktLag=100");
					Controller.ConsoleCommand("Net PktLoss=10");
					Controller.ConsoleCommand("stat net");
					Controller.ConsoleCommand("stat netgraph");
				}
				)AS"),
			TEXT("APlayerController ConsoleCommand should remain outside direct AS exposure"),
			MakeArrayView(PlayerControllerConsoleDiagnostics)),
			TEXT("player-controller network console command execution should compile-fail at the documented AS boundary")));

		const TArray<FString> ClientTravelDiagnostics = { TEXT("ClientTravel") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageNetworking_ClientTravelUnsupported"),
			ASTEST_AS(R"AS(
				void TryClientTravel(APlayerController Controller)
				{
					Controller.ClientTravel("/Game/Maps/NetworkingCoverage", ETravelType::TRAVEL_Absolute);
				}
				)AS"),
			TEXT("APlayerController.ClientTravel wrapper is a native UFUNCTION boundary, not direct AS surface"),
			MakeArrayView(ClientTravelDiagnostics)),
			TEXT("ClientTravel wrapper should compile-fail as a documented AS boundary")));

		UFunction* ClientTravelFunction = APlayerController::StaticClass()->FindFunctionByName(TEXT("ClientTravel"));
		UFunction* ClientTravelInternalFunction = APlayerController::StaticClass()->FindFunctionByName(TEXT("ClientTravelInternal"));
		UFunction* ConsoleCommandFunction = APlayerController::StaticClass()->FindFunctionByName(TEXT("ConsoleCommand"));
		ASSERT_THAT(IsNotNull(ClientTravelFunction,
			TEXT("native APlayerController.ClientTravel wrapper should exist for the client-travel boundary")));
		ASSERT_THAT(IsNotNull(ClientTravelInternalFunction,
			TEXT("native APlayerController.ClientTravelInternal RPC should exist for client-travel metadata")));
		ASSERT_THAT(IsNull(ConsoleCommandFunction,
			TEXT("native APlayerController.ConsoleCommand should remain outside UFUNCTION reflection")));
		if (ClientTravelFunction == nullptr || ClientTravelInternalFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(ClientTravelFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("ClientTravel wrapper should not be bound through BlueprintCallable AS fallback")));
		ASSERT_THAT(IsTrue(ClientTravelInternalFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("ClientTravelInternal should remain a routed network function")));
		ASSERT_THAT(IsTrue(ClientTravelInternalFunction->HasAnyFunctionFlags(FUNC_NetClient),
			TEXT("ClientTravelInternal should remain a client RPC")));
		ASSERT_THAT(IsTrue(ClientTravelInternalFunction->HasAnyFunctionFlags(FUNC_NetReliable),
			TEXT("ClientTravelInternal should remain reliable")));
	}

	TEST_METHOD(GameStatePlayerArrayAndPlayerStateIdentitySurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_GameStatePlayerSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingGameStatePlayerSurface.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingGameStatePlayerSurface : AActor
			{
				UFUNCTION()
				int QueryGameStateAndPlayerState(AGameStateBase GameState, APlayerState PlayerState)
				{
					int Mask = 0;

					if (GameState != nullptr && GameState.PlayerArray.Num() >= 0)
						Mask |= 1;
					if (PlayerState != nullptr)
					{
						if (PlayerState.GetPlayerName().Len() >= 0)
							Mask |= 2;
						if (PlayerState.GetScore() >= 0.0f)
							Mask |= 4;
					}

					return Mask;
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingGameStatePlayerSurface"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("GameState/PlayerState networking surface actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FArrayProperty* PlayerArrayProperty = FindFProperty<FArrayProperty>(AGameStateBase::StaticClass(), TEXT("PlayerArray"));
		ASSERT_THAT(IsNotNull(PlayerArrayProperty,
			TEXT("AGameStateBase.PlayerArray should be reflected for static networking coverage")));
		if (PlayerArrayProperty == nullptr)
		{
			return;
		}

		FObjectPropertyBase* PlayerArrayInnerProperty = CastField<FObjectPropertyBase>(PlayerArrayProperty->Inner);
		ASSERT_THAT(IsNotNull(PlayerArrayInnerProperty,
			TEXT("AGameStateBase.PlayerArray should contain object references")));
		if (PlayerArrayInnerProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(APlayerState::StaticClass(), PlayerArrayInnerProperty->PropertyClass,
			TEXT("AGameStateBase.PlayerArray should contain APlayerState entries")));

		FProperty* ScoreProperty = FindFProperty<FProperty>(APlayerState::StaticClass(), TEXT("Score"));
		FProperty* PlayerNameProperty = FindFProperty<FProperty>(APlayerState::StaticClass(), TEXT("PlayerNamePrivate"));
		UFunction* GetPlayerNameFunction = APlayerState::StaticClass()->FindFunctionByName(TEXT("GetPlayerName"));
		UFunction* GetScoreFunction = APlayerState::StaticClass()->FindFunctionByName(TEXT("GetScore"));
		ASSERT_THAT(IsNotNull(ScoreProperty, TEXT("APlayerState.Score should exist as a replicated native property")));
		ASSERT_THAT(IsNotNull(PlayerNameProperty, TEXT("APlayerState.PlayerNamePrivate should exist as a replicated native property")));
		ASSERT_THAT(IsNotNull(GetPlayerNameFunction, TEXT("APlayerState.GetPlayerName should be reflected")));
		ASSERT_THAT(IsNotNull(GetScoreFunction, TEXT("APlayerState.GetScore should be reflected")));
		if (ScoreProperty == nullptr
			|| PlayerNameProperty == nullptr
			|| GetPlayerNameFunction == nullptr
			|| GetScoreFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScoreProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("APlayerState.Score should be replicated")));
		ASSERT_THAT(IsTrue(ScoreProperty->HasAnyPropertyFlags(CPF_RepNotify),
			TEXT("APlayerState.Score should use RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_Score")),
			ScoreProperty->RepNotifyFunc,
			TEXT("APlayerState.Score should preserve OnRep_Score metadata")));
		ASSERT_THAT(IsTrue(PlayerNameProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("APlayerState.PlayerNamePrivate should be replicated")));
		ASSERT_THAT(IsTrue(PlayerNameProperty->HasAnyPropertyFlags(CPF_RepNotify),
			TEXT("APlayerState.PlayerNamePrivate should use RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_PlayerName")),
			PlayerNameProperty->RepNotifyFunc,
			TEXT("APlayerState.PlayerNamePrivate should preserve OnRep_PlayerName metadata")));
		ASSERT_THAT(IsTrue(GetPlayerNameFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure),
			TEXT("APlayerState.GetPlayerName should be AS-bindable through BlueprintCallable or BlueprintPure")));
		ASSERT_THAT(IsTrue(GetScoreFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure),
			TEXT("APlayerState.GetScore BlueprintGetter should be AS-bindable through BlueprintCallable or BlueprintPure")));
	}

	TEST_METHOD(PlayerControllerConnectionSurfaceCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageNetworking_PlayerControllerConnectionSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageNetworkingPlayerControllerConnectionSurface.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNetworkingPlayerControllerConnectionSurface : AActor
			{
				UFUNCTION()
				int QueryControllerConnection(AController Controller, APlayerController PlayerController)
				{
					int Mask = 0;

					if (Controller != nullptr)
					{
						if (Controller.IsLocalController())
							Mask |= 1;
						if (Controller.IsPlayerController())
							Mask |= 2;
						if (Controller.IsLocalPlayerController())
							Mask |= 4;
					}

					if (PlayerController != nullptr)
					{
						if (PlayerController.GetPlayerState() == nullptr)
							Mask |= 8;
						if (PlayerController.GetLocalPlayer() == nullptr)
							Mask |= 16;
					}

					return Mask;
				}
			}
			)AS"),
			TEXT("ACoverageNetworkingPlayerControllerConnectionSurface"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("PlayerController connection surface actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		using namespace AngelscriptCoverageNetworkingTest;
		UFunction* QueryFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("QueryControllerConnection"));
		ASSERT_THAT(IsNotNull(QueryFunction, TEXT("PlayerController connection query should be generated")));
		if (QueryFunction == nullptr)
		{
			return;
		}

		TArray<FProperty*> Parameters;
		CollectNonReturnParameters(QueryFunction, Parameters);
		ASSERT_THAT(AreEqual(2, Parameters.Num(),
			TEXT("PlayerController connection query should expose controller and player-controller parameters")));
		if (Parameters.Num() != 2)
		{
			return;
		}

		FObjectProperty* ControllerParameter = CastField<FObjectProperty>(Parameters[0]);
		FObjectProperty* PlayerControllerParameter = CastField<FObjectProperty>(Parameters[1]);
		ASSERT_THAT(IsNotNull(ControllerParameter,
			TEXT("connection query controller parameter should be an object property")));
		ASSERT_THAT(IsNotNull(PlayerControllerParameter,
			TEXT("connection query player-controller parameter should be an object property")));
		if (ControllerParameter == nullptr || PlayerControllerParameter == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(AController::StaticClass(), ControllerParameter->PropertyClass,
			TEXT("connection query should preserve AController parameter type")));
		ASSERT_THAT(AreEqual(APlayerController::StaticClass(), PlayerControllerParameter->PropertyClass,
			TEXT("connection query should preserve APlayerController parameter type")));

		FProperty* ReturnProperty = FindReturnProperty(QueryFunction);
		ASSERT_THAT(IsNotNull(ReturnProperty,
			TEXT("PlayerController connection query should expose an int return property")));
		if (ReturnProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReturnProperty->IsA<FIntProperty>(),
			TEXT("PlayerController connection query should return int")));
	}

	TEST_METHOD(GameModeLoginLogoutNativeSurface)
	{
		UFunction* PostLoginFunction = AGameModeBase::StaticClass()->FindFunctionByName(TEXT("PostLogin"));
		UFunction* LogoutFunction = AGameModeBase::StaticClass()->FindFunctionByName(TEXT("Logout"));
		ASSERT_THAT(IsNotNull(PostLoginFunction,
			TEXT("AGameModeBase.PostLogin should be reflected for server-side player login coverage")));
		ASSERT_THAT(IsNotNull(LogoutFunction,
			TEXT("AGameModeBase.Logout should be reflected for server-side player logout coverage")));
		if (PostLoginFunction == nullptr || LogoutFunction == nullptr)
		{
			return;
		}

		TArray<FProperty*> PostLoginParameters;
		TArray<FProperty*> LogoutParameters;
		CollectNonReturnParameters(PostLoginFunction, PostLoginParameters);
		CollectNonReturnParameters(LogoutFunction, LogoutParameters);
		ASSERT_THAT(AreEqual(1, PostLoginParameters.Num(),
			TEXT("AGameModeBase.PostLogin should expose one PlayerController parameter")));
		ASSERT_THAT(AreEqual(1, LogoutParameters.Num(),
			TEXT("AGameModeBase.Logout should expose one Controller parameter")));
		if (PostLoginParameters.Num() != 1 || LogoutParameters.Num() != 1)
		{
			return;
		}

		FObjectProperty* PostLoginPlayerParameter = CastField<FObjectProperty>(PostLoginParameters[0]);
		FObjectProperty* LogoutControllerParameter = CastField<FObjectProperty>(LogoutParameters[0]);
		ASSERT_THAT(IsNotNull(PostLoginPlayerParameter,
			TEXT("AGameModeBase.PostLogin parameter should be an object property")));
		ASSERT_THAT(IsNotNull(LogoutControllerParameter,
			TEXT("AGameModeBase.Logout parameter should be an object property")));
		if (PostLoginPlayerParameter == nullptr || LogoutControllerParameter == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(APlayerController::StaticClass(), PostLoginPlayerParameter->PropertyClass,
			TEXT("AGameModeBase.PostLogin should preserve APlayerController parameter type")));
		ASSERT_THAT(AreEqual(AController::StaticClass(), LogoutControllerParameter->PropertyClass,
			TEXT("AGameModeBase.Logout should preserve AController parameter type")));
		ASSERT_THAT(IsFalse(PostLoginFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("AGameModeBase.PostLogin should remain a server-local lifecycle callback, not an RPC")));
		ASSERT_THAT(IsFalse(LogoutFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("AGameModeBase.Logout should remain a server-local lifecycle callback, not an RPC")));
	}

	TEST_METHOD(ActorDormancyAndUpdateNativeSurface)
	{
		UFunction* SetNetDormancyFunction = AActor::StaticClass()->FindFunctionByName(TEXT("SetNetDormancy"));
		UFunction* FlushNetDormancyFunction = AActor::StaticClass()->FindFunctionByName(TEXT("FlushNetDormancy"));
		UFunction* ForceNetUpdateFunction = AActor::StaticClass()->FindFunctionByName(TEXT("ForceNetUpdate"));
		ASSERT_THAT(IsNotNull(SetNetDormancyFunction,
			TEXT("AActor.SetNetDormancy should be reflected for dormancy coverage")));
		ASSERT_THAT(IsNotNull(FlushNetDormancyFunction,
			TEXT("AActor.FlushNetDormancy should be reflected for dormancy wake coverage")));
		ASSERT_THAT(IsNotNull(ForceNetUpdateFunction,
			TEXT("AActor.ForceNetUpdate should be reflected for network update coverage")));
		if (SetNetDormancyFunction == nullptr
			|| FlushNetDormancyFunction == nullptr
			|| ForceNetUpdateFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SetNetDormancyFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("AActor.SetNetDormancy should be BlueprintCallable and eligible for AS binding")));
		ASSERT_THAT(IsTrue(FlushNetDormancyFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("AActor.FlushNetDormancy should be BlueprintCallable and eligible for AS binding")));
		ASSERT_THAT(IsTrue(ForceNetUpdateFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("AActor.ForceNetUpdate should be BlueprintCallable and eligible for AS binding")));

		TArray<FProperty*> SetNetDormancyParameters;
		CollectNonReturnParameters(SetNetDormancyFunction, SetNetDormancyParameters);
		ASSERT_THAT(AreEqual(1, SetNetDormancyParameters.Num(),
			TEXT("AActor.SetNetDormancy should expose one dormancy parameter")));
		if (SetNetDormancyParameters.Num() != 1)
		{
			return;
		}

		UEnum* DormancyEnum = FindEnumForProperty(SetNetDormancyParameters[0]);
		ASSERT_THAT(IsNotNull(DormancyEnum,
			TEXT("AActor.SetNetDormancy parameter should preserve ENetDormancy enum metadata")));
		if (DormancyEnum == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FName(TEXT("ENetDormancy")), DormancyEnum->GetFName(),
			TEXT("AActor.SetNetDormancy should use ENetDormancy")));
		ASSERT_THAT(IsFalse(SetNetDormancyFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("AActor.SetNetDormancy should configure dormancy locally rather than route as an RPC")));
		ASSERT_THAT(IsFalse(FlushNetDormancyFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("AActor.FlushNetDormancy should configure dormancy locally rather than route as an RPC")));
		ASSERT_THAT(IsFalse(ForceNetUpdateFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("AActor.ForceNetUpdate should request an update locally rather than route as an RPC")));
	}
#endif
};

#endif // WITH_DEV_AUTOMATION_TESTS
