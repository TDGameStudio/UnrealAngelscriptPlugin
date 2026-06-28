#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Containers/Set.h"
#include "Engine/World.h"
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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(OwnerClass, TEXT("networking generated class should be available for function lookup")))
		{
			return nullptr;
		}

		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		LocalAssert.IsNotNull(Function, *FString::Printf(TEXT("networking function '%s' should be generated"), *FunctionName.ToString()));
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
		LocalAssert.IsNotNull(Property, *FString::Printf(TEXT("networking property '%s' should be generated"), *PropertyName.ToString()));
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
		LocalAssert.IsTrue(
			Function->HasAnyFunctionFlags(FUNC_Net),
			*FString::Printf(TEXT("%s should carry FUNC_Net"), Context));
		LocalAssert.IsTrue(
			Function->HasAnyFunctionFlags(RequiredEndpointFlag),
			*FString::Printf(TEXT("%s should carry the expected RPC endpoint flag"), Context));
		LocalAssert.AreEqual(
			bExpectedReliable,
			Function->HasAnyFunctionFlags(FUNC_NetReliable),
			*FString::Printf(TEXT("%s reliability flag should match declaration"), Context));
		LocalAssert.AreEqual(
			bExpectedValidate,
			Function->HasAnyFunctionFlags(FUNC_NetValidate),
			*FString::Printf(TEXT("%s validation flag should match declaration"), Context));
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
};

#endif // WITH_DEV_AUTOMATION_TESTS
