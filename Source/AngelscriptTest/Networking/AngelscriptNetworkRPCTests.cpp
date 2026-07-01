#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: Compiler Pipeline
// Validates that RPC specifiers (Server, Client, NetMulticast, WithValidation, Unreliable)
// compile through the preprocessor → class generator pipeline and produce correct FUNC_Net* flags.
#if WITH_ANGELSCRIPT_UNITTESTS


namespace AngelscriptNetworkRPCTest
{
	static const FName ServerRPCModuleName(TEXT("Tests.Networking.ServerRPCCompile"));
	static const FName ClientRPCModuleName(TEXT("Tests.Networking.ClientRPCCompile"));
	static const FName MulticastRPCModuleName(TEXT("Tests.Networking.MulticastRPCCompile"));
	static const FName ValidationRPCModuleName(TEXT("Tests.Networking.ValidationRPCCompile"));
	static const FName UnreliableRPCModuleName(TEXT("Tests.Networking.UnreliableRPCCompile"));
	static const FName MixedRPCModuleName(TEXT("Tests.Networking.MixedRPCCompile"));
}

// ============================================================================
// Server RPC compilation test
// ============================================================================

TEST_CLASS_WITH_FLAGS(
	FAngelscriptNetworkRPCTest,
	"Angelscript.TestModule.Networking.RPC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ServerDeclarationCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AngelscriptNetworkRPCTest::ServerRPCModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			AngelscriptNetworkRPCTest::ServerRPCModuleName,
			TEXT("Tests/Networking/ServerRPCCompile.as"),
			TEXT(R"AS(
UCLASS()
class AServerRPCTestActor : AActor
{
	default SetReplicates(true);

	UFUNCTION(Server)
	void ServerDoAction()
	{
	}
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Server RPC declaration should compile successfully"), bCompiled))
		{
			TestRunner->AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
			return;
		}

		TestRunner->TestTrue(TEXT("Server RPC compilation should be fully handled"),
			CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled);

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AServerRPCTestActor"));
		ASSERT_THAT(IsNotNull(GeneratedClass));

		UFunction* ServerFunc = GeneratedClass->FindFunctionByName(TEXT("ServerDoAction"));
		ASSERT_THAT(IsNotNull(ServerFunc));

		TestRunner->TestTrue(TEXT("Server RPC function should carry FUNC_Net flag"),
			ServerFunc->HasAnyFunctionFlags(FUNC_Net));
		TestRunner->TestTrue(TEXT("Server RPC function should carry FUNC_NetServer flag"),
			ServerFunc->HasAnyFunctionFlags(FUNC_NetServer));
		TestRunner->TestTrue(TEXT("Server RPC function should default to reliable (FUNC_NetReliable)"),
			ServerFunc->HasAnyFunctionFlags(FUNC_NetReliable));
		}
	}

// ============================================================================
// Client RPC compilation test
// ============================================================================

	TEST_METHOD(ClientDeclarationCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AngelscriptNetworkRPCTest::ClientRPCModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			AngelscriptNetworkRPCTest::ClientRPCModuleName,
			TEXT("Tests/Networking/ClientRPCCompile.as"),
			TEXT(R"AS(
UCLASS()
class AClientRPCTestActor : AActor
{
	default SetReplicates(true);

	UFUNCTION(Client)
	void ClientReceiveUpdate()
	{
	}
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Client RPC declaration should compile successfully"), bCompiled))
		{
			TestRunner->AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AClientRPCTestActor"));
		ASSERT_THAT(IsNotNull(GeneratedClass));

		UFunction* ClientFunc = GeneratedClass->FindFunctionByName(TEXT("ClientReceiveUpdate"));
		ASSERT_THAT(IsNotNull(ClientFunc));

		TestRunner->TestTrue(TEXT("Client RPC function should carry FUNC_Net flag"),
			ClientFunc->HasAnyFunctionFlags(FUNC_Net));
		TestRunner->TestTrue(TEXT("Client RPC function should carry FUNC_NetClient flag"),
			ClientFunc->HasAnyFunctionFlags(FUNC_NetClient));
		TestRunner->TestTrue(TEXT("Client RPC function should default to reliable (FUNC_NetReliable)"),
			ClientFunc->HasAnyFunctionFlags(FUNC_NetReliable));
		}
	}

// ============================================================================
// NetMulticast RPC compilation test
// ============================================================================

	TEST_METHOD(NetMulticastDeclarationCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AngelscriptNetworkRPCTest::MulticastRPCModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			AngelscriptNetworkRPCTest::MulticastRPCModuleName,
			TEXT("Tests/Networking/MulticastRPCCompile.as"),
			TEXT(R"AS(
UCLASS()
class AMulticastRPCTestActor : AActor
{
	default SetReplicates(true);

	UFUNCTION(NetMulticast)
	void MulticastBroadcastEvent()
	{
	}
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("NetMulticast RPC declaration should compile successfully"), bCompiled))
		{
			TestRunner->AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AMulticastRPCTestActor"));
		ASSERT_THAT(IsNotNull(GeneratedClass));

		UFunction* MulticastFunc = GeneratedClass->FindFunctionByName(TEXT("MulticastBroadcastEvent"));
		ASSERT_THAT(IsNotNull(MulticastFunc));

		TestRunner->TestTrue(TEXT("NetMulticast RPC function should carry FUNC_Net flag"),
			MulticastFunc->HasAnyFunctionFlags(FUNC_Net));
		TestRunner->TestTrue(TEXT("NetMulticast RPC function should carry FUNC_NetMulticast flag"),
			MulticastFunc->HasAnyFunctionFlags(FUNC_NetMulticast));
		TestRunner->TestTrue(TEXT("NetMulticast RPC function should default to reliable (FUNC_NetReliable)"),
			MulticastFunc->HasAnyFunctionFlags(FUNC_NetReliable));
		}
	}

// ============================================================================
// WithValidation RPC compilation test
// ============================================================================

	TEST_METHOD(WithValidationDeclarationCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AngelscriptNetworkRPCTest::ValidationRPCModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			AngelscriptNetworkRPCTest::ValidationRPCModuleName,
			TEXT("Tests/Networking/ValidationRPCCompile.as"),
			TEXT(R"AS(
UCLASS()
class AValidationRPCTestActor : AActor
{
	default SetReplicates(true);

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
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("WithValidation RPC declaration should compile successfully"), bCompiled))
		{
			TestRunner->AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AValidationRPCTestActor"));
		ASSERT_THAT(IsNotNull(GeneratedClass));

		UFunction* ValidatedFunc = GeneratedClass->FindFunctionByName(TEXT("ServerValidatedAction"));
		ASSERT_THAT(IsNotNull(ValidatedFunc));

		TestRunner->TestTrue(TEXT("WithValidation RPC function should carry FUNC_Net flag"),
			ValidatedFunc->HasAnyFunctionFlags(FUNC_Net));
		TestRunner->TestTrue(TEXT("WithValidation RPC function should carry FUNC_NetServer flag"),
			ValidatedFunc->HasAnyFunctionFlags(FUNC_NetServer));
		TestRunner->TestTrue(TEXT("WithValidation RPC function should carry FUNC_NetValidate flag"),
			ValidatedFunc->HasAnyFunctionFlags(FUNC_NetValidate));
		}
	}

// ============================================================================
// Unreliable RPC compilation test
// ============================================================================

	TEST_METHOD(UnreliableDeclarationCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AngelscriptNetworkRPCTest::UnreliableRPCModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			AngelscriptNetworkRPCTest::UnreliableRPCModuleName,
			TEXT("Tests/Networking/UnreliableRPCCompile.as"),
			TEXT(R"AS(
UCLASS()
class AUnreliableRPCTestActor : AActor
{
	default SetReplicates(true);

	UFUNCTION(Client, Unreliable)
	void ClientUnreliableUpdate()
	{
	}
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Unreliable RPC declaration should compile successfully"), bCompiled))
		{
			TestRunner->AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AUnreliableRPCTestActor"));
		ASSERT_THAT(IsNotNull(GeneratedClass));

		UFunction* UnreliableFunc = GeneratedClass->FindFunctionByName(TEXT("ClientUnreliableUpdate"));
		ASSERT_THAT(IsNotNull(UnreliableFunc));

		TestRunner->TestTrue(TEXT("Unreliable RPC function should carry FUNC_Net flag"),
			UnreliableFunc->HasAnyFunctionFlags(FUNC_Net));
		TestRunner->TestTrue(TEXT("Unreliable RPC function should carry FUNC_NetClient flag"),
			UnreliableFunc->HasAnyFunctionFlags(FUNC_NetClient));
		TestRunner->TestFalse(TEXT("Unreliable RPC function should NOT carry FUNC_NetReliable flag"),
			UnreliableFunc->HasAnyFunctionFlags(FUNC_NetReliable));
		}
	}

// ============================================================================
// Mixed RPC compilation test — multiple RPC types in one class
// ============================================================================

	TEST_METHOD(MixedDeclarationsCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AngelscriptNetworkRPCTest::MixedRPCModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			AngelscriptNetworkRPCTest::MixedRPCModuleName,
			TEXT("Tests/Networking/MixedRPCCompile.as"),
			TEXT(R"AS(
UCLASS()
class AMixedRPCTestActor : AActor
{
	default SetReplicates(true);

	UPROPERTY(Replicated)
	int ReplicatedScore = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Health)
	float Health = 100.0;

	UFUNCTION()
	void OnRep_Health()
	{
	}

	UFUNCTION(Server)
	void ServerApplyDamage()
	{
		Health -= 10.0;
	}

	UFUNCTION(Client)
	void ClientNotifyHit()
	{
	}

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayEffect()
	{
	}

	UFUNCTION(Server, WithValidation)
	void ServerValidatedAttack()
	{
	}

	UFUNCTION()
	bool ServerValidatedAttack_Validate()
	{
		return true;
	}
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Mixed RPC + replication declarations should compile successfully"), bCompiled))
		{
			TestRunner->AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AMixedRPCTestActor"));
		ASSERT_THAT(IsNotNull(GeneratedClass));

		// Verify Server RPC
		UFunction* ServerFunc = GeneratedClass->FindFunctionByName(TEXT("ServerApplyDamage"));
		if (TestRunner->TestNotNull(TEXT("Mixed: Server RPC function should exist"), ServerFunc))
		{
			TestRunner->TestTrue(TEXT("Mixed: Server RPC should carry FUNC_NetServer"), ServerFunc->HasAnyFunctionFlags(FUNC_NetServer));
		}

		// Verify Client RPC
		UFunction* ClientFunc = GeneratedClass->FindFunctionByName(TEXT("ClientNotifyHit"));
		if (TestRunner->TestNotNull(TEXT("Mixed: Client RPC function should exist"), ClientFunc))
		{
			TestRunner->TestTrue(TEXT("Mixed: Client RPC should carry FUNC_NetClient"), ClientFunc->HasAnyFunctionFlags(FUNC_NetClient));
		}

		// Verify NetMulticast Unreliable RPC
		UFunction* MulticastFunc = GeneratedClass->FindFunctionByName(TEXT("MulticastPlayEffect"));
		if (TestRunner->TestNotNull(TEXT("Mixed: NetMulticast RPC function should exist"), MulticastFunc))
		{
			TestRunner->TestTrue(TEXT("Mixed: NetMulticast RPC should carry FUNC_NetMulticast"), MulticastFunc->HasAnyFunctionFlags(FUNC_NetMulticast));
			TestRunner->TestFalse(TEXT("Mixed: NetMulticast Unreliable RPC should NOT carry FUNC_NetReliable"), MulticastFunc->HasAnyFunctionFlags(FUNC_NetReliable));
		}

		// Verify Validated Server RPC
		UFunction* ValidatedFunc = GeneratedClass->FindFunctionByName(TEXT("ServerValidatedAttack"));
		if (TestRunner->TestNotNull(TEXT("Mixed: Validated Server RPC function should exist"), ValidatedFunc))
		{
			TestRunner->TestTrue(TEXT("Mixed: Validated Server RPC should carry FUNC_NetServer"), ValidatedFunc->HasAnyFunctionFlags(FUNC_NetServer));
			TestRunner->TestTrue(TEXT("Mixed: Validated Server RPC should carry FUNC_NetValidate"), ValidatedFunc->HasAnyFunctionFlags(FUNC_NetValidate));
		}

		// Verify replicated properties
		FProperty* ScoreProperty = FindFProperty<FProperty>(GeneratedClass, TEXT("ReplicatedScore"));
		if (TestRunner->TestNotNull(TEXT("Mixed: ReplicatedScore property should exist"), ScoreProperty))
		{
			TestRunner->TestTrue(TEXT("Mixed: ReplicatedScore should carry CPF_Net"), ScoreProperty->HasAnyPropertyFlags(CPF_Net));
		}

		FProperty* HealthProperty = FindFProperty<FProperty>(GeneratedClass, TEXT("Health"));
		if (TestRunner->TestNotNull(TEXT("Mixed: Health property should exist"), HealthProperty))
		{
			TestRunner->TestTrue(TEXT("Mixed: Health should carry CPF_Net"), HealthProperty->HasAnyPropertyFlags(CPF_Net));
			TestRunner->TestTrue(TEXT("Mixed: Health should carry CPF_RepNotify"), HealthProperty->HasAnyPropertyFlags(CPF_RepNotify));
		}
		}
	}
};

#endif
