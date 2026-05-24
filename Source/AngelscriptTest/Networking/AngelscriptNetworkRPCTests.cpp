#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: Compiler Pipeline
// Validates that RPC specifiers (Server, Client, NetMulticast, WithValidation, Unreliable)
// compile through the preprocessor → class generator pipeline and produce correct FUNC_Net* flags.
#if WITH_DEV_AUTOMATION_TESTS


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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNetworkServerRPCCompileTest,
	"Angelscript.TestModule.Networking.RPC.ServerDeclarationCompiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNetworkServerRPCCompileTest::RunTest(const FString& Parameters)
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

	if (!TestTrue(TEXT("Server RPC declaration should compile successfully"), bCompiled))
	{
		AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
		return false;
	}

	TestTrue(TEXT("Server RPC compilation should be fully handled"),
		CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled);

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AServerRPCTestActor"));
	if (!TestNotNull(TEXT("Server RPC test actor class should be materialized"), GeneratedClass))
	{
		return false;
	}

	UFunction* ServerFunc = GeneratedClass->FindFunctionByName(TEXT("ServerDoAction"));
	if (!TestNotNull(TEXT("Server RPC function should exist on generated class"), ServerFunc))
	{
		return false;
	}

	TestTrue(TEXT("Server RPC function should carry FUNC_Net flag"),
		ServerFunc->HasAnyFunctionFlags(FUNC_Net));
	TestTrue(TEXT("Server RPC function should carry FUNC_NetServer flag"),
		ServerFunc->HasAnyFunctionFlags(FUNC_NetServer));
	TestTrue(TEXT("Server RPC function should default to reliable (FUNC_NetReliable)"),
		ServerFunc->HasAnyFunctionFlags(FUNC_NetReliable));
	}

	return true;
}

// ============================================================================
// Client RPC compilation test
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNetworkClientRPCCompileTest,
	"Angelscript.TestModule.Networking.RPC.ClientDeclarationCompiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNetworkClientRPCCompileTest::RunTest(const FString& Parameters)
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

	if (!TestTrue(TEXT("Client RPC declaration should compile successfully"), bCompiled))
	{
		AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AClientRPCTestActor"));
	if (!TestNotNull(TEXT("Client RPC test actor class should be materialized"), GeneratedClass))
	{
		return false;
	}

	UFunction* ClientFunc = GeneratedClass->FindFunctionByName(TEXT("ClientReceiveUpdate"));
	if (!TestNotNull(TEXT("Client RPC function should exist on generated class"), ClientFunc))
	{
		return false;
	}

	TestTrue(TEXT("Client RPC function should carry FUNC_Net flag"),
		ClientFunc->HasAnyFunctionFlags(FUNC_Net));
	TestTrue(TEXT("Client RPC function should carry FUNC_NetClient flag"),
		ClientFunc->HasAnyFunctionFlags(FUNC_NetClient));
	TestTrue(TEXT("Client RPC function should default to reliable (FUNC_NetReliable)"),
		ClientFunc->HasAnyFunctionFlags(FUNC_NetReliable));
	}

	return true;
}

// ============================================================================
// NetMulticast RPC compilation test
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNetworkMulticastRPCCompileTest,
	"Angelscript.TestModule.Networking.RPC.NetMulticastDeclarationCompiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNetworkMulticastRPCCompileTest::RunTest(const FString& Parameters)
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

	if (!TestTrue(TEXT("NetMulticast RPC declaration should compile successfully"), bCompiled))
	{
		AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AMulticastRPCTestActor"));
	if (!TestNotNull(TEXT("NetMulticast RPC test actor class should be materialized"), GeneratedClass))
	{
		return false;
	}

	UFunction* MulticastFunc = GeneratedClass->FindFunctionByName(TEXT("MulticastBroadcastEvent"));
	if (!TestNotNull(TEXT("NetMulticast RPC function should exist on generated class"), MulticastFunc))
	{
		return false;
	}

	TestTrue(TEXT("NetMulticast RPC function should carry FUNC_Net flag"),
		MulticastFunc->HasAnyFunctionFlags(FUNC_Net));
	TestTrue(TEXT("NetMulticast RPC function should carry FUNC_NetMulticast flag"),
		MulticastFunc->HasAnyFunctionFlags(FUNC_NetMulticast));
	TestTrue(TEXT("NetMulticast RPC function should default to reliable (FUNC_NetReliable)"),
		MulticastFunc->HasAnyFunctionFlags(FUNC_NetReliable));
	}

	return true;
}

// ============================================================================
// WithValidation RPC compilation test
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNetworkValidationRPCCompileTest,
	"Angelscript.TestModule.Networking.RPC.WithValidationDeclarationCompiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNetworkValidationRPCCompileTest::RunTest(const FString& Parameters)
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

	if (!TestTrue(TEXT("WithValidation RPC declaration should compile successfully"), bCompiled))
	{
		AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AValidationRPCTestActor"));
	if (!TestNotNull(TEXT("WithValidation RPC test actor class should be materialized"), GeneratedClass))
	{
		return false;
	}

	UFunction* ValidatedFunc = GeneratedClass->FindFunctionByName(TEXT("ServerValidatedAction"));
	if (!TestNotNull(TEXT("WithValidation RPC function should exist on generated class"), ValidatedFunc))
	{
		return false;
	}

	TestTrue(TEXT("WithValidation RPC function should carry FUNC_Net flag"),
		ValidatedFunc->HasAnyFunctionFlags(FUNC_Net));
	TestTrue(TEXT("WithValidation RPC function should carry FUNC_NetServer flag"),
		ValidatedFunc->HasAnyFunctionFlags(FUNC_NetServer));
	TestTrue(TEXT("WithValidation RPC function should carry FUNC_NetValidate flag"),
		ValidatedFunc->HasAnyFunctionFlags(FUNC_NetValidate));
	}

	return true;
}

// ============================================================================
// Unreliable RPC compilation test
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNetworkUnreliableRPCCompileTest,
	"Angelscript.TestModule.Networking.RPC.UnreliableDeclarationCompiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNetworkUnreliableRPCCompileTest::RunTest(const FString& Parameters)
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

	if (!TestTrue(TEXT("Unreliable RPC declaration should compile successfully"), bCompiled))
	{
		AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AUnreliableRPCTestActor"));
	if (!TestNotNull(TEXT("Unreliable RPC test actor class should be materialized"), GeneratedClass))
	{
		return false;
	}

	UFunction* UnreliableFunc = GeneratedClass->FindFunctionByName(TEXT("ClientUnreliableUpdate"));
	if (!TestNotNull(TEXT("Unreliable RPC function should exist on generated class"), UnreliableFunc))
	{
		return false;
	}

	TestTrue(TEXT("Unreliable RPC function should carry FUNC_Net flag"),
		UnreliableFunc->HasAnyFunctionFlags(FUNC_Net));
	TestTrue(TEXT("Unreliable RPC function should carry FUNC_NetClient flag"),
		UnreliableFunc->HasAnyFunctionFlags(FUNC_NetClient));
	TestFalse(TEXT("Unreliable RPC function should NOT carry FUNC_NetReliable flag"),
		UnreliableFunc->HasAnyFunctionFlags(FUNC_NetReliable));
	}

	return true;
}

// ============================================================================
// Mixed RPC compilation test — multiple RPC types in one class
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNetworkMixedRPCCompileTest,
	"Angelscript.TestModule.Networking.RPC.MixedDeclarationsCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNetworkMixedRPCCompileTest::RunTest(const FString& Parameters)
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

	if (!TestTrue(TEXT("Mixed RPC + replication declarations should compile successfully"), bCompiled))
	{
		AddError(FString::Printf(TEXT("Compile result: %d"), static_cast<int32>(CompileResult)));
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("AMixedRPCTestActor"));
	if (!TestNotNull(TEXT("Mixed RPC test actor class should be materialized"), GeneratedClass))
	{
		return false;
	}

	// Verify Server RPC
	UFunction* ServerFunc = GeneratedClass->FindFunctionByName(TEXT("ServerApplyDamage"));
	if (TestNotNull(TEXT("Mixed: Server RPC function should exist"), ServerFunc))
	{
		TestTrue(TEXT("Mixed: Server RPC should carry FUNC_NetServer"), ServerFunc->HasAnyFunctionFlags(FUNC_NetServer));
	}

	// Verify Client RPC
	UFunction* ClientFunc = GeneratedClass->FindFunctionByName(TEXT("ClientNotifyHit"));
	if (TestNotNull(TEXT("Mixed: Client RPC function should exist"), ClientFunc))
	{
		TestTrue(TEXT("Mixed: Client RPC should carry FUNC_NetClient"), ClientFunc->HasAnyFunctionFlags(FUNC_NetClient));
	}

	// Verify NetMulticast Unreliable RPC
	UFunction* MulticastFunc = GeneratedClass->FindFunctionByName(TEXT("MulticastPlayEffect"));
	if (TestNotNull(TEXT("Mixed: NetMulticast RPC function should exist"), MulticastFunc))
	{
		TestTrue(TEXT("Mixed: NetMulticast RPC should carry FUNC_NetMulticast"), MulticastFunc->HasAnyFunctionFlags(FUNC_NetMulticast));
		TestFalse(TEXT("Mixed: NetMulticast Unreliable RPC should NOT carry FUNC_NetReliable"), MulticastFunc->HasAnyFunctionFlags(FUNC_NetReliable));
	}

	// Verify Validated Server RPC
	UFunction* ValidatedFunc = GeneratedClass->FindFunctionByName(TEXT("ServerValidatedAttack"));
	if (TestNotNull(TEXT("Mixed: Validated Server RPC function should exist"), ValidatedFunc))
	{
		TestTrue(TEXT("Mixed: Validated Server RPC should carry FUNC_NetServer"), ValidatedFunc->HasAnyFunctionFlags(FUNC_NetServer));
		TestTrue(TEXT("Mixed: Validated Server RPC should carry FUNC_NetValidate"), ValidatedFunc->HasAnyFunctionFlags(FUNC_NetValidate));
	}

	// Verify replicated properties
	FProperty* ScoreProperty = FindFProperty<FProperty>(GeneratedClass, TEXT("ReplicatedScore"));
	if (TestNotNull(TEXT("Mixed: ReplicatedScore property should exist"), ScoreProperty))
	{
		TestTrue(TEXT("Mixed: ReplicatedScore should carry CPF_Net"), ScoreProperty->HasAnyPropertyFlags(CPF_Net));
	}

	FProperty* HealthProperty = FindFProperty<FProperty>(GeneratedClass, TEXT("Health"));
	if (TestNotNull(TEXT("Mixed: Health property should exist"), HealthProperty))
	{
		TestTrue(TEXT("Mixed: Health should carry CPF_Net"), HealthProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("Mixed: Health should carry CPF_RepNotify"), HealthProperty->HasAnyPropertyFlags(CPF_RepNotify));
	}
	}

	return true;
}

#endif
