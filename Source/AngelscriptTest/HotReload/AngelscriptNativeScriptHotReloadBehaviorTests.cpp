#include "CQTest.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS



#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeScriptHotReloadBehaviorTests,
	"Angelscript.TestModule.HotReload.NativeScript.Phase2B",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
inline static const FName NativeScriptNamespaceReloadModuleName = FName(TEXT("HotReloadPhase2BMathNamespace"));
inline static const FString NativeScriptNamespaceReloadFilename = FString(TEXT("HotReloadPhase2BMathNamespace.as"));
inline static const FName NativeScriptNamespaceReloadClassName = FName(TEXT("UNativeHotReloadPhase2BMathCarrier"));
inline static const FName NativeScriptNamespaceReloadFunctionName = FName(TEXT("ComputeSquare"));

inline static const FString NativeScriptNamespaceReloadScriptV1 = TEXT(R"AS(
namespace NativeHotReloadMath
{
int Square(int X)
{
	return X * X;
}
}

UCLASS()
class UNativeHotReloadPhase2BMathCarrier : UObject
{
UFUNCTION()
int ComputeSquare(int X)
{
	return NativeHotReloadMath::Square(X);
}
};
)AS");

inline static const FString NativeScriptNamespaceReloadScriptV2 = TEXT(R"AS(
namespace NativeHotReloadMath
{
int Square(int X)
{
	return X * X + 1;
}
}

UCLASS()
class UNativeHotReloadPhase2BMathCarrier : UObject
{
UFUNCTION()
int ComputeSquare(int X)
{
	return NativeHotReloadMath::Square(X);
}
};
)AS");

struct FSingleIntParamAndReturnValue
{
	int32 X = 0;
	int32 ReturnValue = 0;
};

static bool ExecuteGeneratedIntFunctionOnGameThread(
	FAngelscriptEngine& Engine,
	UObject* Object,
	UFunction* Function,
	int32 InputValue,
	int32& OutResult)
{
	if (!::IsValid(Object) || Function == nullptr)
	{
		return false;
	}

	auto Invoke = [&Engine, Object, Function, InputValue, &OutResult]()
	{
		FSingleIntParamAndReturnValue Params;
		Params.X = InputValue;

		FAngelscriptEngineScope EngineScope(Engine, Object);
		Object->ProcessEvent(Function, &Params);
		OutResult = Params.ReturnValue;
	};

	if (IsInGameThread())
	{
		Invoke();
		return true;
	}

	FEvent* CompletedEvent = FPlatformProcess::GetSynchEventFromPool(true);
	AsyncTask(ENamedThreads::GameThread, [Invoke, CompletedEvent]() mutable
	{
		Invoke();
		CompletedEvent->Trigger();
	});

	CompletedEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(CompletedEvent);
	return true;
}

static bool NativeScriptHotReloadPhase2BNamespaceFunctionBehaviorSwitch(FAutomationTestBase& Test)
{
FAngelscriptEngine* ProductionEngine = RequireRunningProductionEngine(
		Test,
		TEXT("Native script namespace hot reload behavior tests require a production engine."));
	if (ProductionEngine == nullptr)
	{
		return false;
	}

	FAngelscriptEngine& Engine = *ProductionEngine;
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*NativeScriptNamespaceReloadModuleName.ToString());
	};

	ECompileResult InitialCompileResult = ECompileResult::Error;
	if (!TestTrue(
			TEXT("Phase2B namespace function behavior switch should compile V1 on the full reload path"),
			CompileModuleWithResult(
				&Engine,
				ECompileType::FullReload,
				NativeScriptNamespaceReloadModuleName,
				NativeScriptNamespaceReloadFilename,
				NativeScriptNamespaceReloadScriptV1,
				InitialCompileResult)))
	{
		return false;
	}

	UClass* ClassBeforeReload = FindGeneratedClass(&Engine, NativeScriptNamespaceReloadClassName);
	if (!TestNotNull(TEXT("Phase2B namespace function behavior switch should expose the generated class before reload"), ClassBeforeReload))
	{
		return false;
	}

	UFunction* ComputeSquareBeforeReload = FindGeneratedFunction(ClassBeforeReload, NativeScriptNamespaceReloadFunctionName);
	if (!TestNotNull(TEXT("Phase2B namespace function behavior switch should expose ComputeSquare before reload"), ComputeSquareBeforeReload))
	{
		return false;
	}

	UObject* ExistingObject = NewObject<UObject>(GetTransientPackage(), ClassBeforeReload);
	if (!TestNotNull(TEXT("Phase2B namespace function behavior switch should create a V1 carrier object"), ExistingObject))
	{
		return false;
	}

	int32 ValueBeforeReload = 0;
	if (!TestTrue(
			TEXT("Phase2B namespace function behavior switch should execute V1 before reload"),
			ExecuteGeneratedIntFunctionOnGameThread(Engine, ExistingObject, ComputeSquareBeforeReload, 3, ValueBeforeReload)))
	{
		return false;
	}
	TestEqual(TEXT("Phase2B namespace function behavior switch should return 9 before reload"), ValueBeforeReload, 9);

	ECompileResult ReloadCompileResult = ECompileResult::Error;
	if (!TestTrue(
			TEXT("Phase2B namespace function behavior switch should compile V2 on the soft reload path"),
			CompileModuleWithResult(
				&Engine,
				ECompileType::SoftReloadOnly,
				NativeScriptNamespaceReloadModuleName,
				NativeScriptNamespaceReloadFilename,
				NativeScriptNamespaceReloadScriptV2,
				ReloadCompileResult)))
	{
		return false;
	}

	if (!TestTrue(
			TEXT("Phase2B namespace function behavior switch should stay on a handled reload path"),
			ReloadCompileResult == ECompileResult::FullyHandled || ReloadCompileResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassAfterReload = FindGeneratedClass(&Engine, NativeScriptNamespaceReloadClassName);
	if (!TestNotNull(TEXT("Phase2B namespace function behavior switch should expose the generated class after reload"), ClassAfterReload))
	{
		return false;
	}

	UFunction* ComputeSquareAfterReload = FindGeneratedFunction(ClassAfterReload, NativeScriptNamespaceReloadFunctionName);
	if (!TestNotNull(TEXT("Phase2B namespace function behavior switch should expose ComputeSquare after reload"), ComputeSquareAfterReload))
	{
		return false;
	}

	UObject* NewObjectAfterReload = NewObject<UObject>(GetTransientPackage(), ClassAfterReload);
	if (!TestNotNull(TEXT("Phase2B namespace function behavior switch should create a fresh carrier object after reload"), NewObjectAfterReload))
	{
		return false;
	}

	int32 ValueAfterReload = 0;
	if (!TestTrue(
			TEXT("Phase2B namespace function behavior switch should execute the reloaded function on a new object"),
			ExecuteGeneratedIntFunctionOnGameThread(Engine, NewObjectAfterReload, ComputeSquareAfterReload, 3, ValueAfterReload)))
	{
		return false;
	}
	TestEqual(TEXT("Phase2B namespace function behavior switch should return 10 after reload on a new object"), ValueAfterReload, 10);

	int32 ExistingObjectValueAfterReload = 0;
	if (!TestTrue(
			TEXT("Phase2B namespace function behavior switch should execute the reloaded function on the existing object"),
			ExecuteGeneratedIntFunctionOnGameThread(Engine, ExistingObject, ComputeSquareAfterReload, 3, ExistingObjectValueAfterReload)))
	{
		return false;
	}
	TestEqual(
		TEXT("Phase2B namespace function behavior switch should update the existing object dispatch to the new namespace function body"),
		ExistingObjectValueAfterReload,
		10);

	return true;
}

#undef TestTrue
#undef TestEqual
#undef TestNotNull

public:
	TEST_METHOD(NamespaceFunctionBehaviorSwitch)
	{
		ASSERT_THAT(IsTrue(NativeScriptHotReloadPhase2BNamespaceFunctionBehaviorSwitch(*TestRunner)));
	}
};

#endif
