#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
// Validates script classes subclassing UScriptWorldSubsystem and
// UScriptGameInstanceSubsystem. The preprocessor auto-generates a Get()
// static method whose Cast<SubClass>(USubsystemLibrary::GetXxxSubsystem(...))
// expression requires correct TObjectPtr ↔ UObject* type routing in the
// binding layer.
//
// These tests assert compile SUCCESS after the CPF_TObjectPtr fix in
// Bind_BlueprintType.cpp restored correct type-finder routing.
#if WITH_ANGELSCRIPT_UNITTESTS


// ============================================================================
// World Subsystem compilation tests — validate script subclassing compiles
// ============================================================================

TEST_CLASS_WITH_FLAGS(
	FAngelscriptTestWorldSubsystemTests,
	"Angelscript.TestModule.WorldSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Lifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
	static const FName ModuleName(TEXT("TestWorldSubsystemLifecycle"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	ECompileResult CompileResult = ECompileResult::FullyHandled;
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::FullReload,
		ModuleName,
		TEXT("TestWorldSubsystemLifecycle.as"),
		TEXT(R"AS(
UCLASS()
class UTestWorldLifecycleTracker : UScriptWorldSubsystem
{
	UFUNCTION(BlueprintOverride)
	void Initialize()
	{
	}

	UFUNCTION(BlueprintOverride)
	void Deinitialize()
	{
	}
}
)AS"),
		CompileResult);

	// CPF_TObjectPtr fix landed: subsystem subclass should now compile successfully.
	ASSERT_THAT(IsTrue(bCompiled, TEXT("Script world subsystem lifecycle subclass should compile after TObjectPtr fix")));
	}

	TEST_METHOD(Tick)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
	static const FName ModuleName(TEXT("TestWorldSubsystemTick"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	ECompileResult CompileResult = ECompileResult::FullyHandled;
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::FullReload,
		ModuleName,
		TEXT("TestWorldSubsystemTick.as"),
		TEXT(R"AS(
UCLASS()
class UTestWorldTicker : UScriptWorldSubsystem
{
	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
	}
}
)AS"),
		CompileResult);

	// CPF_TObjectPtr fix landed: subsystem subclass should now compile successfully.
	ASSERT_THAT(IsTrue(bCompiled, TEXT("Script world subsystem tick subclass should compile after TObjectPtr fix")));
	}

	TEST_METHOD(ActorAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
	static const FName ModuleName(TEXT("TestWorldSubsystemActorAccess"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	ECompileResult CompileResult = ECompileResult::FullyHandled;
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::FullReload,
		ModuleName,
		TEXT("TestWorldSubsystemActorAccess.as"),
		TEXT(R"AS(
UCLASS()
class UTestWorldActorWatcher : UScriptWorldSubsystem
{
	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		GetWorld().GetPersistentLevel().GetActors().Num();
	}
}

UCLASS()
class ATestWorldSubsystemActorAccessActor : AActor
{
}
)AS"),
		CompileResult);

	// CPF_TObjectPtr fix landed: subsystem subclass should now compile successfully.
	ASSERT_THAT(IsTrue(bCompiled, TEXT("Script world subsystem actor access subclass should compile after TObjectPtr fix")));
	}
};


TEST_CLASS_WITH_FLAGS(
	FAngelscriptTestGameInstanceSubsystemTests,
	"Angelscript.TestModule.GameInstanceSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Lifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
	static const FName ModuleName(TEXT("TestGameInstanceSubsystemLifecycle"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	ECompileResult CompileResult = ECompileResult::FullyHandled;
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::FullReload,
		ModuleName,
		TEXT("TestGameInstanceSubsystemLifecycle.as"),
		TEXT(R"AS(
UCLASS()
class UTestGameInstanceLifecycleTracker : UScriptGameInstanceSubsystem
{
	UFUNCTION(BlueprintOverride)
	void Initialize()
	{
	}

	UFUNCTION(BlueprintOverride)
	void Deinitialize()
	{
	}
}
)AS"),
		CompileResult);

	// CPF_TObjectPtr fix landed: subsystem subclass should now compile successfully.
	ASSERT_THAT(IsTrue(bCompiled, TEXT("Script game-instance subsystem lifecycle subclass should compile after TObjectPtr fix")));
	}
};

#endif
