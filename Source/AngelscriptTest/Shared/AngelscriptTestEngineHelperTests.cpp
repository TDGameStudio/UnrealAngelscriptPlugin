#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptNativeScriptTestObject.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "Components/ActorTestSpawner.h"
#include "Components/ActorComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestNull(...) Test.TestNull(__VA_ARGS__)
#define AddError(...) Test.AddError(__VA_ARGS__)

static bool RunCompileModuleFromMemory(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperCompileModule"));
	};

	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("HelperCompileModule"),
		TEXT("HelperCompileModule.as"),
		TEXT("int Entry() { return 42; }"));

	TestTrue(TEXT("CompileModuleFromMemory should compile a trivial module"), bCompiled);
	return bCompiled;
}

static bool RunExecuteIntFunction(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperExecuteInt"));
	};

	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("HelperExecuteInt"),
		TEXT("HelperExecuteInt.as"),
		TEXT("int DoubleValue(int Value) { return Value * 2; } int Entry() { return DoubleValue(21); }"));
	if (!TestTrue(TEXT("ExecuteIntFunction test module compiles"), bCompiled))
	{
		return false;
	}

	int32 Result = 0;
	const bool bExecuted = ExecuteIntFunction(&Engine, TEXT("HelperExecuteInt"), TEXT("int Entry()"), Result);
	if (!TestTrue(TEXT("ExecuteIntFunction should execute the compiled entry point"), bExecuted))
	{
		return false;
	}

	TestEqual(TEXT("ExecuteIntFunction returns the expected result"), Result, 42);
	return Result == 42;
}

static bool RunGeneratedSymbolLookup(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperAnnotatedModule"));
	};

	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("HelperAnnotatedModule"),
		TEXT("HelperAnnotatedModule.as"),
		TEXT(R"(
UCLASS()
class UAnnotatedHelperObject : UObject
{
    UFUNCTION()
    int GetValue()
    {
        return 42;
    }
}
)") );
	if (!TestTrue(TEXT("CompileAnnotatedModuleFromMemory should compile an annotated class module"), bCompiled))
	{
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UAnnotatedHelperObject"));
	if (!TestNotNull(TEXT("FindGeneratedClass should locate the generated class"), GeneratedClass))
	{
		return false;
	}

	UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, TEXT("GetValue"));
	TestNotNull(TEXT("FindGeneratedFunction should locate the generated UFunction"), GeneratedFunction);
	return GeneratedFunction != nullptr;
}

static bool RunFailedAnnotatedModuleDoesNotPolluteLaterCompiles(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperBrokenAnnotated"));
		Engine.DiscardModule(TEXT("HelperRecoveredAnnotated"));
	};

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bBrokenCompiled = AnalyzeReloadFromMemory(
		&Engine,
		TEXT("HelperBrokenAnnotated"),
		TEXT("HelperBrokenAnnotated.as"),
		TEXT(R"(
UCLASS()
class UBrokenHelperObject : UObject
{
	UFUNCTION()
	MissingType GetValue()
	{
		MissingType Value;
		return Value;
	}
}
	)") ,
		ReloadRequirement,
		bWantsFullReload,
		bNeedsFullReload);
	if (!TestFalse(TEXT("Invalid annotated helper module should fail to compile"), bBrokenCompiled))
	{
		return false;
	}
	TestEqual(TEXT("Broken annotated helper module should report an error reload requirement"), ReloadRequirement, FAngelscriptClassGenerator::Error);
	TestFalse(TEXT("Broken annotated helper module should not suggest a full reload"), bWantsFullReload);
	TestFalse(TEXT("Broken annotated helper module should not require a full reload"), bNeedsFullReload);

	const bool bRecoveredCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("HelperRecoveredAnnotated"),
		TEXT("HelperRecoveredAnnotated.as"),
		TEXT(R"(
UCLASS()
class URecoveredHelperObject : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 7;
	}
}
)") );
	if (!TestTrue(TEXT("A later valid annotated helper module should compile after a failed one"), bRecoveredCompiled))
	{
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("URecoveredHelperObject"));
	if (!TestNotNull(TEXT("Recovered generated class should be discoverable"), GeneratedClass))
	{
		return false;
	}

	UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, TEXT("GetValue"));
	TestNotNull(TEXT("Recovered generated function should be discoverable"), GeneratedFunction);
	return GeneratedFunction != nullptr;
}

static bool RunSharedTestEngineNeverSilentlyAttachesToProductionEngine(FAutomationTestBase& Test)
{
	FAngelscriptEngine* PreviousCurrentEngine = FAngelscriptTestEngineScopeAccess::GetCurrentEngine();
	FAngelscriptEngine* PreviousGlobalEngine = FAngelscriptTestEngineScopeAccess::GetGlobalEngine();
	FAngelscriptEngine& SharedEngine = GetOrCreateSharedCloneEngine();
	return TestTrue(
		TEXT("Explicit shared clone helper should resolve to the shared clone engine instance"),
		&GetOrCreateSharedCloneEngine() == &SharedEngine)
		&& TestTrue(
		TEXT("Clean shared clone helper should keep using the shared clone engine instance"),
		&AcquireCleanSharedCloneEngine() == &SharedEngine)
		&& TestTrue(
		TEXT("Shared clone helper should not silently replace the current engine"),
		FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == PreviousCurrentEngine)
		&& TestTrue(
		TEXT("Shared clone helper should not silently install itself as the legacy global engine"),
		FAngelscriptTestEngineScopeAccess::GetGlobalEngine() == PreviousGlobalEngine);
}

static bool RunScriptScanFreeEngineStartsClean(FAutomationTestBase& Test)
{
	TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting();
	if (!TestNotNull(TEXT("Script-scan-free test engine should be created"), Engine.Get()))
	{
		return false;
	}

	TestTrue(TEXT("Script-scan-free test engine should still satisfy the initial compile gate"), Engine->IsInitialCompileFinished());
	TestEqual(TEXT("Script-scan-free test engine should not contain modules compiled from Script roots"), Engine->GetActiveModules().Num(), 0);

	asCScriptEngine* ScriptEngine = reinterpret_cast<asCScriptEngine*>(Engine->GetScriptEngine());
	if (!TestNotNull(TEXT("Script-scan-free test engine should own a script engine"), ScriptEngine))
	{
		return false;
	}

	TArray<FString> RawModuleNames;
	const asUINT RawModuleCount = ScriptEngine->GetModuleCount();
	RawModuleNames.Reserve(static_cast<int32>(RawModuleCount));
	for (asUINT ModuleIndex = 0; ModuleIndex < RawModuleCount; ++ModuleIndex)
	{
		if (asIScriptModule* Module = ScriptEngine->GetModuleByIndex(ModuleIndex))
		{
			RawModuleNames.Add(UTF8_TO_TCHAR(Module->GetName()));
		}
	}

	if (RawModuleNames.Num() > 0)
	{
		AddError(FString::Printf(TEXT("Script-scan-free test engine unexpectedly has raw modules: %s"), *FString::Join(RawModuleNames, TEXT(", "))));
	}

	return TestEqual(TEXT("Script-scan-free test engine should not contain raw AS modules"), RawModuleCount, static_cast<asUINT>(0));
}

static bool RunAcquireCleanSharedCloneEngineResetsModules(FAutomationTestBase& Test)
{
	static const FName ModuleName(TEXT("HelperAcquireCleanResetModules"));
	static const FString Filename(TEXT("HelperAcquireCleanResetModules.as"));

	DestroySharedTestEngine();
	ON_SCOPE_EXIT
	{
		DestroySharedTestEngine();
	};

	FAngelscriptEngine& SharedEngine = GetOrCreateSharedCloneEngine();
	const bool bCompiled = CompileModuleFromMemory(
		&SharedEngine,
		ModuleName,
		Filename,
		TEXT("int Entry() { return 17; }"));
	if (!TestTrue(TEXT("AcquireCleanSharedCloneEngine regression fixture should compile before reset"), bCompiled))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("AcquireCleanSharedCloneEngine regression fixture should register a tracked module before reset"),
		SharedEngine.GetModuleByModuleName(ModuleName.ToString()).IsValid()))
	{
		return false;
	}

	FAngelscriptEngine& ResetEngine = AcquireCleanSharedCloneEngine();
	return TestTrue(
		TEXT("AcquireCleanSharedCloneEngine should reuse the shared engine instance"),
		&ResetEngine == &SharedEngine)
		&& TestFalse(
		TEXT("AcquireCleanSharedCloneEngine should clear tracked modules from the shared engine"),
		ResetEngine.GetModuleByModuleName(ModuleName.ToString()).IsValid())
		&& TestNull(
		TEXT("AcquireCleanSharedCloneEngine should discard the backing script module"),
		ResetEngine.GetScriptEngine()->GetModule(TCHAR_TO_UTF8(*ModuleName.ToString()), asGM_ONLY_IF_EXISTS));
}

static bool RunResetSharedEngineReleasesGeneratedComponentClasses(FAutomationTestBase& Test)
{
	static const FName ModuleName(TEXT("HelperResetGeneratedComponent"));
	static const FString Filename(TEXT("HelperResetGeneratedComponent.as"));
	static const FName GeneratedClassName(TEXT("UHelperResetGeneratedComponent"));

	DestroySharedAndStrayGlobalTestEngine();
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		ModuleName,
		Filename,
		TEXT(R"AS(
UCLASS()
class UHelperResetGeneratedComponent : UActorComponent
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaSeconds)
	{
		TickCount += 1;
	}
}
)AS"));
	if (!TestTrue(TEXT("Generated component helper module should compile"), bCompiled))
	{
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, GeneratedClassName);
	if (!TestNotNull(TEXT("Generated component helper class should exist before reset"), GeneratedClass))
	{
		return false;
	}
	UASClass* GeneratedASClass = Cast<UASClass>(GeneratedClass);
	if (!TestNotNull(TEXT("Generated component helper class should be a UASClass"), GeneratedASClass))
	{
		return false;
	}
	TWeakObjectPtr<UASClass> WeakGeneratedClass(GeneratedASClass);
	const FString GeneratedClassPath = GeneratedASClass->GetPathName();

	{
		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& HostActor = Spawner.SpawnActor<AActor>();
		UActorComponent* Component = NewObject<UActorComponent>(&HostActor, GeneratedClass);
		if (!TestNotNull(TEXT("Generated component helper should instantiate"), Component))
		{
			return false;
		}

		HostActor.AddInstanceComponent(Component);
		Component->OnComponentCreated();
		Component->RegisterComponent();
		Component->PrimaryComponentTick.bCanEverTick = true;
		Component->SetComponentTickEnabled(true);
		Component->Activate(true);

		{
			FAngelscriptEngineScope EngineScope(Engine, &HostActor);
			HostActor.DispatchBeginPlay();
		}

		{
			FAngelscriptEngineScope EngineScope(Engine, Component);
			Component->TickComponent(0.016f, ELevelTick::LEVELTICK_All, &Component->PrimaryComponentTick);
		}
	}
	// Spawner destroyed here — World, Actor, and Component are released so GC
	// can reclaim the generated UASClass after ResetSharedCloneEngine.

	ASTEST_RESET_ENGINE(Engine);

	UASClass* FoundGeneratedClassByPath = FindObject<UASClass>(nullptr, *GeneratedClassPath);
	int32 MatchingClasses = 0;
	int32 RootedMatchingClasses = 0;
	int32 StandaloneMatchingClasses = 0;
	for (TObjectIterator<UASClass> It; It; ++It)
	{
		if (It->GetFName() != GeneratedClassName)
		{
			continue;
		}

		++MatchingClasses;
		if (It->IsRooted())
		{
			++RootedMatchingClasses;
		}
		if (It->HasAnyFlags(RF_Standalone))
		{
			++StandaloneMatchingClasses;
		}
	}

	TestFalse(TEXT("Generated component class weak pointer should be invalid after shared reset"), WeakGeneratedClass.IsValid());
	TestNull(TEXT("Generated component class should not be findable by path after shared reset"), FoundGeneratedClassByPath);
	TestEqual(TEXT("Generated component class should be garbage collected after shared reset"), MatchingClasses, 0);
	TestEqual(TEXT("Generated component class should not remain rooted after shared reset"), RootedMatchingClasses, 0);
	TestEqual(TEXT("Generated component class should not remain standalone after shared reset"), StandaloneMatchingClasses, 0);
	return !WeakGeneratedClass.IsValid()
		&& FoundGeneratedClassByPath == nullptr
		&& MatchingClasses == 0
		&& RootedMatchingClasses == 0
		&& StandaloneMatchingClasses == 0;
}

static bool RunResetSharedEngineReleasesGeneratedStructs(FAutomationTestBase& Test)
{
	static const FName ModuleName(TEXT("HelperResetGeneratedStruct"));
	static const FString Filename(TEXT("HelperResetGeneratedStruct.as"));
	static const FName GeneratedStructName(TEXT("HelperResetGeneratedStruct"));

	DestroySharedAndStrayGlobalTestEngine();
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		ModuleName,
		Filename,
		TEXT(R"AS(
USTRUCT()
struct FHelperResetGeneratedStruct
{
	UPROPERTY()
	int Value = 11;
}
)AS"));
	if (!TestTrue(TEXT("Generated struct helper module should compile"), bCompiled))
	{
		return false;
	}

	UASStruct* GeneratedStruct = FindObject<UASStruct>(FAngelscriptEngine::GetPackage(), *GeneratedStructName.ToString());
	if (!TestNotNull(TEXT("Generated helper struct should exist before reset"), GeneratedStruct))
	{
		return false;
	}
	TWeakObjectPtr<UASStruct> WeakGeneratedStruct(GeneratedStruct);
	const FString GeneratedStructPath = GeneratedStruct->GetPathName();

	ASTEST_RESET_ENGINE(Engine);

	UASStruct* FoundGeneratedStructByPath = FindObject<UASStruct>(nullptr, *GeneratedStructPath);
	int32 MatchingStructs = 0;
	int32 RootedMatchingStructs = 0;
	int32 StandaloneMatchingStructs = 0;
	for (TObjectIterator<UASStruct> It; It; ++It)
	{
		if (It->GetFName() != GeneratedStructName)
		{
			continue;
		}

		++MatchingStructs;
		if (It->IsRooted())
		{
			++RootedMatchingStructs;
		}
		if (It->HasAnyFlags(RF_Standalone))
		{
			++StandaloneMatchingStructs;
		}
	}

	TestFalse(TEXT("Generated struct weak pointer should be invalid after shared reset"), WeakGeneratedStruct.IsValid());
	TestNull(TEXT("Generated struct should not be findable by path after shared reset"), FoundGeneratedStructByPath);
	TestEqual(TEXT("Generated struct should be garbage collected after shared reset"), MatchingStructs, 0);
	TestEqual(TEXT("Generated struct should not remain rooted after shared reset"), RootedMatchingStructs, 0);
	TestEqual(TEXT("Generated struct should not remain standalone after shared reset"), StandaloneMatchingStructs, 0);
	return !WeakGeneratedStruct.IsValid()
		&& FoundGeneratedStructByPath == nullptr
		&& MatchingStructs == 0
		&& RootedMatchingStructs == 0
		&& StandaloneMatchingStructs == 0;
}

static bool RunResetSharedEngineReleasesGeneratedEnumsAndDelegates(FAutomationTestBase& Test)
{
	static const FName ModuleName(TEXT("HelperResetGeneratedEnumDelegate"));
	static const FString Filename(TEXT("HelperResetGeneratedEnumDelegate.as"));

	DestroySharedAndStrayGlobalTestEngine();
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		ModuleName,
		Filename,
		TEXT(R"(
UENUM(BlueprintType)
enum class EHelperResetGeneratedState : uint8
{
	Idle,
	Active
}

delegate void FHelperResetGeneratedDelegate(int Value);
event void FHelperResetGeneratedEvent(int Value);
)"));
	if (!TestTrue(TEXT("Generated enum/delegate helper fixture should compile"), bCompiled))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptEnumDesc> GeneratedEnumDesc = Engine.GetEnum(TEXT("EHelperResetGeneratedState"));
	if (!TestTrue(TEXT("Generated enum descriptor should exist before reset"), GeneratedEnumDesc.IsValid()))
	{
		return false;
	}
	UEnum* GeneratedEnum = GeneratedEnumDesc->Enum;
	if (!TestNotNull(TEXT("Generated enum UObject should exist before reset"), GeneratedEnum))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> GeneratedDelegateDesc = Engine.GetDelegate(TEXT("FHelperResetGeneratedDelegate"));
	if (!TestTrue(TEXT("Generated delegate descriptor should exist before reset"), GeneratedDelegateDesc.IsValid()))
	{
		return false;
	}
	TestFalse(TEXT("Generated delegate descriptor should be single-cast before reset"), GeneratedDelegateDesc->bIsMulticast);
	UDelegateFunction* GeneratedDelegateFunction = GeneratedDelegateDesc->Function;
	if (!TestNotNull(TEXT("Generated delegate function should exist before reset"), GeneratedDelegateFunction))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> GeneratedEventDesc = Engine.GetDelegate(TEXT("FHelperResetGeneratedEvent"));
	if (!TestTrue(TEXT("Generated event descriptor should exist before reset"), GeneratedEventDesc.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("Generated event descriptor should be multicast before reset"), GeneratedEventDesc->bIsMulticast);
	UDelegateFunction* GeneratedEventFunction = GeneratedEventDesc->Function;
	if (!TestNotNull(TEXT("Generated event function should exist before reset"), GeneratedEventFunction))
	{
		return false;
	}

	TWeakObjectPtr<UEnum> WeakGeneratedEnum(GeneratedEnum);
	TWeakObjectPtr<UDelegateFunction> WeakGeneratedDelegateFunction(GeneratedDelegateFunction);
	TWeakObjectPtr<UDelegateFunction> WeakGeneratedEventFunction(GeneratedEventFunction);
	const FString GeneratedEnumPath = GeneratedEnum->GetPathName();
	const FString GeneratedDelegateFunctionPath = GeneratedDelegateFunction->GetPathName();
	const FString GeneratedEventFunctionPath = GeneratedEventFunction->GetPathName();

	ASTEST_RESET_ENGINE(Engine);

	UEnum* FoundGeneratedEnumByPath = FindObject<UEnum>(nullptr, *GeneratedEnumPath);
	UDelegateFunction* FoundGeneratedDelegateFunctionByPath = FindObject<UDelegateFunction>(nullptr, *GeneratedDelegateFunctionPath);
	UDelegateFunction* FoundGeneratedEventFunctionByPath = FindObject<UDelegateFunction>(nullptr, *GeneratedEventFunctionPath);

	TestFalse(TEXT("Generated enum weak pointer should be invalid after shared reset"), WeakGeneratedEnum.IsValid());
	TestFalse(TEXT("Generated delegate function weak pointer should be invalid after shared reset"), WeakGeneratedDelegateFunction.IsValid());
	TestFalse(TEXT("Generated event function weak pointer should be invalid after shared reset"), WeakGeneratedEventFunction.IsValid());
	TestNull(TEXT("Generated enum should not be findable by path after shared reset"), FoundGeneratedEnumByPath);
	TestNull(TEXT("Generated delegate function should not be findable by path after shared reset"), FoundGeneratedDelegateFunctionByPath);
	TestNull(TEXT("Generated event function should not be findable by path after shared reset"), FoundGeneratedEventFunctionByPath);
	return !WeakGeneratedEnum.IsValid()
		&& !WeakGeneratedDelegateFunction.IsValid()
		&& !WeakGeneratedEventFunction.IsValid()
		&& FoundGeneratedEnumByPath == nullptr
		&& FoundGeneratedDelegateFunctionByPath == nullptr
		&& FoundGeneratedEventFunctionByPath == nullptr;
}

static bool RunProductionHelperRejectsMissingProductionEngine(FAutomationTestBase& Test)
{
	FAngelscriptEngine* ProductionEngine = TryGetRunningProductionEngine();
	if (UAngelscriptGameInstanceSubsystem* Subsystem = UAngelscriptGameInstanceSubsystem::GetCurrent())
	{
		return TestTrue(
			TEXT("Production-engine probe should resolve the subsystem-attached engine when one is available"),
			ProductionEngine == Subsystem->GetEngine());
	}

	if (FAngelscriptEngine::IsInitialized())
	{
		return TestTrue(
			TEXT("Production-engine probe should resolve the global runtime engine when the runtime is initialized without a subsystem owner"),
			ProductionEngine == &FAngelscriptEngine::Get());
	}

	return TestNull(
		TEXT("Production-engine probe should return null when no production engine is attached"),
		ProductionEngine);
}

static bool RunProductionDebuggerHelperPrefersDebuggableEngineOverScopedTestEngine(FAutomationTestBase& Test)
{
	FAngelscriptEngine* DebuggableEngine = TryGetRunningProductionDebuggerEngine();
	if (!TestNotNull(TEXT("Scoped production-debugger helper test requires an active debuggable production engine"), DebuggableEngine))
	{
		return false;
	}

	DestroySharedTestEngine();
	FAngelscriptEngine& SharedEngine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		DestroySharedTestEngine();
	};

	if (!TestTrue(
		TEXT("Shared helper should create a distinct test engine when a debuggable production engine exists"),
		&SharedEngine != DebuggableEngine))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Shared helper should become the current scoped engine while its persistent scope is active"),
		FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == &SharedEngine))
	{
		return false;
	}

	return TestTrue(
		TEXT("Production-debugger helper should still prefer the debuggable production engine over a scoped shared test engine"),
		TryGetRunningProductionDebuggerEngine() == DebuggableEngine);
}

static bool RunResetSharedEngineDiscardsRawModules(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		ASTEST_RESET_ENGINE(Engine);
	};

	{
		FAngelscriptEngineScope GlobalScope(Engine);
		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule("HelperRawSharedReset", asGM_ALWAYS_CREATE);
		if (!TestNotNull(TEXT("Raw shared-engine reset test should create a script module"), Module))
		{
			return false;
		}

		asIScriptFunction* Function = nullptr;
		const int32 CompileResult = Module->CompileFunction("HelperRawSharedReset", "int Entry() { return 9; }", 0, 0, &Function);
		if (Function != nullptr)
		{
			Function->Release();
		}

		if (!TestEqual(TEXT("Raw shared-engine reset test should compile successfully"), CompileResult, asSUCCESS))
		{
			return false;
		}
	}

	if (!TestFalse(
		TEXT("Raw shared-engine reset test should not populate tracked module descriptors for direct script-engine modules"),
		Engine.GetModuleByModuleName(TEXT("HelperRawSharedReset")).IsValid()))
	{
		return false;
	}

	if (!TestNotNull(
		TEXT("Raw shared-engine reset test should leave the raw module registered before helper cleanup"),
		Engine.GetScriptEngine()->GetModule("HelperRawSharedReset", asGM_ONLY_IF_EXISTS)))
	{
		return false;
	}

	ASTEST_RESET_ENGINE(Engine);
	return TestNull(
		TEXT("ResetSharedCloneEngine should also discard raw direct-compile script modules"),
		Engine.GetScriptEngine()->GetModule("HelperRawSharedReset", asGM_ONLY_IF_EXISTS));
}

static bool RunCompileRestoresOuterCurrentEngine(FAutomationTestBase& Test)
{
	FAngelscriptEngine* PreviousCurrentEngine = FAngelscriptTestEngineScopeAccess::GetCurrentEngine();
	FAngelscriptEngine& SharedEngine = GetOrCreateSharedCloneEngine();
	TUniquePtr<FAngelscriptEngine> IsolatedEngine = CreateIsolatedCloneEngine();
	if (!TestNotNull(TEXT("Compile restore test should create an isolated engine"), IsolatedEngine.Get()))
	{
		return false;
	}

	{
		FAngelscriptEngineScope SharedScope(SharedEngine);
		const bool bCompiled = CompileModuleFromMemory(
			IsolatedEngine.Get(),
			TEXT("HelperScopedGlobalRestore"),
			TEXT("HelperScopedGlobalRestore.as"),
			TEXT("int Entry() { return 1; }"));

		if (!TestTrue(TEXT("Scoped current-engine restore test module should compile"), bCompiled))
		{
			return false;
		}

		if (!TestTrue(
			TEXT("Compiling through helper should restore the previous scoped current engine"),
			FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == &SharedEngine))
		{
			return false;
		}
	}

	return TestTrue(
		TEXT("Leaving the outer scope should restore the previous current engine when no enclosing scoped test engine exists"),
		FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == PreviousCurrentEngine);
}

static bool RunNestedCurrentEngineScopeRestoresPreviousEngine(FAutomationTestBase& Test)
{
	FAngelscriptEngine* PreviousCurrentEngine = FAngelscriptTestEngineScopeAccess::GetCurrentEngine();
	TUniquePtr<FAngelscriptEngine> EngineA = CreateIsolatedCloneEngine();
	TUniquePtr<FAngelscriptEngine> EngineB = CreateIsolatedCloneEngine();
	if (!TestNotNull(TEXT("Nested scope restore test should create engine A"), EngineA.Get())
		|| !TestNotNull(TEXT("Nested scope restore test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	{
		FAngelscriptEngineScope ScopeA(*EngineA);
		if (!TestTrue(TEXT("Outer scope should install engine A as the current engine"), FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == EngineA.Get()))
		{
			return false;
		}

		{
			FAngelscriptEngineScope ScopeB(*EngineB);
			if (!TestTrue(TEXT("Inner scope should temporarily install engine B as the current engine"), FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == EngineB.Get()))
			{
				return false;
			}
		}

		if (!TestTrue(TEXT("Leaving inner scope should restore engine A as the current engine"), FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == EngineA.Get()))
		{
			return false;
		}
	}

	return TestTrue(
		TEXT("Leaving the outer scope should restore the previous current engine when no enclosing scoped test engine exists"),
		FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == PreviousCurrentEngine);
}

static bool RunWorldContextScopeRestoresPreviousContext(FAutomationTestBase& Test)
{
	UObject* PreviousWorldContext = FAngelscriptEngine::GetAmbientWorldContext();
	UObject* DummyContext = NewObject<UAngelscriptNativeScriptTestObject>();
	if (!TestNotNull(TEXT("World context scope restore test should create a dummy context object"), DummyContext))
	{
		return false;
	}

	{
		FScopedTestWorldContextScope WorldContextScope(DummyContext);
		if (!TestTrue(TEXT("World context scope should install the dummy context"), FAngelscriptEngine::GetAmbientWorldContext() == DummyContext))
		{
			return false;
		}
	}

	return TestTrue(TEXT("World context scope should restore the previous context"), FAngelscriptEngine::GetAmbientWorldContext() == PreviousWorldContext);
}

static bool RunEngineScopeRestoresWorldContextAndCurrentEngine(FAutomationTestBase& Test)
{
	FAngelscriptEngine* PreviousCurrentEngine = FAngelscriptTestEngineScopeAccess::GetCurrentEngine();
	TUniquePtr<FAngelscriptEngine> Engine = CreateIsolatedCloneEngine();
	UObject* PreviousWorldContext = FAngelscriptEngine::GetAmbientWorldContext();
	UObject* DummyContext = NewObject<UAngelscriptNativeScriptTestObject>();
	if (!TestNotNull(TEXT("Engine-scope world context test should create an isolated engine"), Engine.Get())
		|| !TestNotNull(TEXT("Engine-scope world context test should create a dummy context object"), DummyContext))
	{
		return false;
	}

	{
		FAngelscriptEngineScope EngineScope(*Engine, DummyContext);
		if (!TestTrue(TEXT("Engine scope should install the isolated engine as current"), FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == Engine.Get()))
		{
			return false;
		}

		if (!TestTrue(TEXT("Engine scope should install the dummy world context"), FAngelscriptEngine::TryGetCurrentWorldContextObject() == DummyContext))
		{
			return false;
		}
	}

	if (!TestTrue(
		TEXT("Engine scope should restore the previous current engine"),
		FAngelscriptTestEngineScopeAccess::GetCurrentEngine() == PreviousCurrentEngine))
	{
		return false;
	}

	return TestTrue(TEXT("Engine scope should restore the previous world context"), FAngelscriptEngine::GetAmbientWorldContext() == PreviousWorldContext);
}

static bool RunCompileSummaryPlainModule(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperCompileSummaryPlain"));
	};

	FAngelscriptCompileTraceSummary Summary;
	const bool bCompiled = CompileModuleWithSummary(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("HelperCompileSummaryPlain"),
		TEXT("HelperCompileSummaryPlain.as"),
		TEXT("int Entry() { return 42; }"),
		false,
		Summary);

	if (!TestTrue(TEXT("CompileModuleWithSummary should compile a plain module"), bCompiled))
	{
		return false;
	}

	TestFalse(TEXT("Plain module summary should report no preprocessor usage"), Summary.bUsedPreprocessor);
	TestEqual(TEXT("Plain module summary should report one module descriptor"), Summary.ModuleDescCount, 1);
	TestTrue(TEXT("Plain module summary should produce at least one compiled module"), Summary.CompiledModuleCount >= 1);
	TestEqual(TEXT("Plain module summary should report no diagnostics"), Summary.Diagnostics.Num(), 0);
	return Summary.ModuleDescCount == 1 && Summary.Diagnostics.Num() == 0;
}

static bool RunCompileSummaryDiagnosticCapture(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = GetOrCreateSharedCloneEngine();
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("HelperCompileSummaryBroken"));
	};

	FAngelscriptCompileTraceSummary Summary;
	const bool bCompiled = CompileModuleWithSummary(
		&Engine,
		ECompileType::FullReload,
		TEXT("HelperCompileSummaryBroken"),
		TEXT("HelperCompileSummaryBroken.as"),
		TEXT(R"(
UCLASS()
class UBrokenCompileSummaryObject : UObject
{
	UFUNCTION()
	MissingType GetValue()
	{
		MissingType Value;
		return Value;
	}
}
)"),
		true,
		Summary,
		true);

	TestFalse(TEXT("CompileModuleWithSummary should fail for broken annotated input"), bCompiled);
	TestTrue(TEXT("Broken annotated summary should report preprocessor usage"), Summary.bUsedPreprocessor);
	TestTrue(TEXT("Broken annotated summary should capture diagnostics"), Summary.Diagnostics.Num() > 0);
	TestEqual(TEXT("Broken annotated summary should report an error compile result"), Summary.CompileResult, ECompileResult::Error);
	return !bCompiled && Summary.Diagnostics.Num() > 0 && Summary.CompileResult == ECompileResult::Error;
}

static bool RunExecutingOneTestEngineDoesNotLeakContextIntoNextTest(FAutomationTestBase& Test)
{
	TUniquePtr<FAngelscriptEngine> EngineA = CreateIsolatedCloneEngine();
	TUniquePtr<FAngelscriptEngine> EngineB = CreateIsolatedCloneEngine();
	if (!TestNotNull(TEXT("Context isolation test should create engine A"), EngineA.Get())
		|| !TestNotNull(TEXT("Context isolation test should create engine B"), EngineB.Get()))
	{
		return false;
	}

	const bool bCompiledA = CompileModuleFromMemory(
		EngineA.Get(),
		TEXT("HelperIsolationA"),
		TEXT("HelperIsolationA.as"),
		TEXT("int EntryA() { return 11; }"));
	const bool bCompiledB = CompileModuleFromMemory(
		EngineB.Get(),
		TEXT("HelperIsolationB"),
		TEXT("HelperIsolationB.as"),
		TEXT("int EntryB() { return 22; }"));
	if (!TestTrue(TEXT("Context isolation test should compile module A"), bCompiledA)
		|| !TestTrue(TEXT("Context isolation test should compile module B"), bCompiledB))
	{
		return false;
	}

	int32 ResultA = 0;
	int32 ResultB = 0;
	if (!TestTrue(TEXT("Engine A should execute its own module"), ExecuteIntFunction(EngineA.Get(), TEXT("HelperIsolationA"), TEXT("int EntryA()"), ResultA))
		|| !TestTrue(TEXT("Engine B should execute its own module"), ExecuteIntFunction(EngineB.Get(), TEXT("HelperIsolationB"), TEXT("int EntryB()"), ResultB)))
	{
		return false;
	}

	TestEqual(TEXT("Engine A should return its own result"), ResultA, 11);
	return TestEqual(TEXT("Engine B should return its own result without context leakage"), ResultB, 22);
}

static bool RunSubsystemAttachedProductionEngineDoesNotHijackIsolatedTestEngine(FAutomationTestBase& Test)
{
	FAngelscriptEngine& SharedEngine = GetOrCreateSharedCloneEngine();
	TUniquePtr<FAngelscriptEngine> IsolatedEngine = CreateIsolatedCloneEngine();
	if (!TestNotNull(TEXT("Subsystem hijack test should create an isolated engine"), IsolatedEngine.Get()))
	{
		return false;
	}

	const bool bCompiled = CompileModuleFromMemory(
		IsolatedEngine.Get(),
		TEXT("HelperIsolationHijack"),
		TEXT("HelperIsolationHijack.as"),
		TEXT("int Entry() { return 5; }"));
	if (!TestTrue(TEXT("Subsystem hijack test module should compile"), bCompiled))
	{
		return false;
	}

	TestTrue(TEXT("Isolated engine should keep its own module record"), IsolatedEngine->GetModuleByModuleName(TEXT("HelperIsolationHijack")).IsValid());
	return TestTrue(TEXT("Shared test engine should not receive isolated engine modules"), !SharedEngine.GetModuleByModuleName(TEXT("HelperIsolationHijack")).IsValid());
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull
#undef TestNull
#undef AddError

TEST_CLASS_WITH_FLAGS(
	FAngelscriptTestEngineHelperTests,
	"Angelscript.TestModule.Shared.EngineHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompileModuleFromMemory)
	{
		ASSERT_THAT(IsTrue(RunCompileModuleFromMemory(*TestRunner)));
	}

	TEST_METHOD(ExecuteIntFunction)
	{
		ASSERT_THAT(IsTrue(RunExecuteIntFunction(*TestRunner)));
	}

	TEST_METHOD(GeneratedSymbolLookup)
	{
		ASSERT_THAT(IsTrue(RunGeneratedSymbolLookup(*TestRunner)));
	}

	TEST_METHOD(FailedAnnotatedModuleDoesNotPolluteLaterCompiles)
	{
		ASSERT_THAT(IsTrue(RunFailedAnnotatedModuleDoesNotPolluteLaterCompiles(*TestRunner)));
	}

	TEST_METHOD(SharedTestEngineNeverSilentlyAttachesToProductionEngine)
	{
		ASSERT_THAT(IsTrue(RunSharedTestEngineNeverSilentlyAttachesToProductionEngine(*TestRunner)));
	}

	TEST_METHOD(ScriptScanFreeEngineStartsClean)
	{
		ASSERT_THAT(IsTrue(RunScriptScanFreeEngineStartsClean(*TestRunner)));
	}

	TEST_METHOD(AcquireCleanSharedCloneEngineResetsModules)
	{
		ASSERT_THAT(IsTrue(RunAcquireCleanSharedCloneEngineResetsModules(*TestRunner)));
	}

	TEST_METHOD(ProductionHelperRejectsMissingProductionEngine)
	{
		ASSERT_THAT(IsTrue(RunProductionHelperRejectsMissingProductionEngine(*TestRunner)));
	}

	TEST_METHOD(ResetSharedEngineDiscardsRawModules)
	{
		ASSERT_THAT(IsTrue(RunResetSharedEngineDiscardsRawModules(*TestRunner)));
	}

	TEST_METHOD(ResetSharedEngineReleasesGeneratedComponentClasses)
	{
		ASSERT_THAT(IsTrue(RunResetSharedEngineReleasesGeneratedComponentClasses(*TestRunner)));
	}

	TEST_METHOD(ResetSharedEngineReleasesGeneratedStructs)
	{
		ASSERT_THAT(IsTrue(RunResetSharedEngineReleasesGeneratedStructs(*TestRunner)));
	}

	TEST_METHOD(ResetSharedEngineReleasesGeneratedEnumsAndDelegates)
	{
		ASSERT_THAT(IsTrue(RunResetSharedEngineReleasesGeneratedEnumsAndDelegates(*TestRunner)));
	}

	TEST_METHOD(CompileRestoresOuterCurrentEngine)
	{
		ASSERT_THAT(IsTrue(RunCompileRestoresOuterCurrentEngine(*TestRunner)));
	}

	TEST_METHOD(NestedCurrentEngineScopeRestoresPreviousEngine)
	{
		ASSERT_THAT(IsTrue(RunNestedCurrentEngineScopeRestoresPreviousEngine(*TestRunner)));
	}

	TEST_METHOD(WorldContextScopeRestoresPreviousContext)
	{
		ASSERT_THAT(IsTrue(RunWorldContextScopeRestoresPreviousContext(*TestRunner)));
	}

	TEST_METHOD(EngineScopeRestoresWorldContextAndCurrentEngine)
	{
		ASSERT_THAT(IsTrue(RunEngineScopeRestoresWorldContextAndCurrentEngine(*TestRunner)));
	}

	TEST_METHOD(CompileSummaryPlainModule)
	{
		ASSERT_THAT(IsTrue(RunCompileSummaryPlainModule(*TestRunner)));
	}

	TEST_METHOD(CompileSummaryDiagnosticCapture)
	{
		ASSERT_THAT(IsTrue(RunCompileSummaryDiagnosticCapture(*TestRunner)));
	}

	TEST_METHOD(ExecutingOneTestEngineDoesNotLeakContextIntoNextTest)
	{
		ASSERT_THAT(IsTrue(RunExecutingOneTestEngineDoesNotLeakContextIntoNextTest(*TestRunner)));
	}

	TEST_METHOD(SubsystemAttachedProductionEngineDoesNotHijackIsolatedTestEngine)
	{
		ASSERT_THAT(IsTrue(RunSubsystemAttachedProductionEngineDoesNotHijackIsolatedTestEngine(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptTestEngineHelperProductionDebuggerTests,
	"Angelscript.TestModule.Shared.EngineHelper.ProductionDebuggerHelperPrefersDebuggableEngineOverScopedTestEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled) // DISABLED(#ue57-headless): TryGetRunningProductionDebuggerEngine returns null in headless batch automation on UE 5.7; requires interactive editor session
{
	TEST_METHOD(Run)
	{
		ASSERT_THAT(IsTrue(RunProductionDebuggerHelperPrefersDebuggableEngineOverScopedTestEngine(*TestRunner)));
	}
};

#endif
