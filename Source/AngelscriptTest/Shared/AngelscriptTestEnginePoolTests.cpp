#include "CQTest.h"
#include "AngelscriptTestEnginePool.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "Testing/AngelscriptBindExecutionObservation.h"

#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestNull(...) Test.TestNull(__VA_ARGS__)

namespace AngelscriptTest_Shared_AngelscriptTestEnginePoolTests_Private
{
	int32 CountRootedDetachedASClasses()
	{
		int32 Count = 0;
		for (TObjectIterator<UASClass> It; It; ++It)
		{
			if (It->ScriptTypePtr == nullptr && It->IsRooted())
			{
				++Count;
			}
		}
		return Count;
	}
}


static bool RunPrewarmCachesBindDatabase(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Shared_AngelscriptTestEnginePoolTests_Private;
	ShutdownTestEnginePool();
	FAngelscriptBindExecutionObservation::Reset();

	FAngelscriptEngine& SourceEngine = PrewarmTestEnginePool();
	const FAngelscriptBindExecutionSnapshot PrewarmSnapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
	if (!TestTrue(TEXT("Prewarm should create a fully bound source engine"), PrewarmSnapshot.BindScriptTypesDurationSeconds > 0.0))
	{
		return false;
	}

	FAngelscriptBindExecutionObservation::Reset();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);
		TestTrue(TEXT("Module-clean acquire should reuse the prewarmed source engine"), &Engine == &SourceEngine);
	}

	TestEqual(TEXT("Module-clean acquire should not replay BindScriptTypes"), FAngelscriptBindExecutionObservation::GetInvocationCount(), 0);
	ShutdownTestEnginePool();
	return true;
}

static bool RunModuleCleanDiscardsOnlyDelta(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Shared_AngelscriptTestEnginePoolTests_Private;
	ShutdownTestEnginePool();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();

	{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);
		asIScriptModule* Module = BuildModule(
			Test,
			Engine,
			"PoolDeltaModule",
			TEXT("int Entry() { return 12; }"));
		if (!TestNotNull(TEXT("Module-clean test should compile its delta module"), Module))
		{
			return false;
		}
		TestEqual(TEXT("Delta module should be active inside the scoped lease"), Engine.GetActiveModules().Num(), 1);
	}

	TestEqual(TEXT("Module-clean scope should discard its active module delta"), Engine.GetActiveModules().Num(), 0);
	const FAngelscriptTestEnginePoolMetrics Metrics = GetTestEnginePoolMetrics();
	TestTrue(TEXT("Module-clean scope should record at least one cleanup"), Metrics.ModuleCleanCount >= 1);
	TestTrue(TEXT("Module-clean scope should not force GC for a plain module"), Metrics.GarbageCollectCount == 0);

	ShutdownTestEnginePool();
	return true;
}

static bool RunGeneratedClassCleanupIsBounded(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Shared_AngelscriptTestEnginePoolTests_Private;
	ShutdownTestEnginePool();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();

	{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("PoolGeneratedClassModule"),
			TEXT("PoolGeneratedClassModule.as"),
			TEXT(R"(
UCLASS()
class UPoolGeneratedClassObject : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 9;
	}
}
)"));
		if (!TestTrue(TEXT("Generated-class pool fixture should compile"), bCompiled))
		{
			return false;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UPoolGeneratedClassObject"));
		if (!TestNotNull(TEXT("Generated class should be visible inside the scoped lease"), GeneratedClass))
		{
			return false;
		}
	}

	const FAngelscriptTestEnginePoolMetrics Metrics = GetTestEnginePoolMetrics();
	TestEqual(TEXT("Generated-class cleanup should leave no rooted detached UASClass objects"), CountRootedDetachedASClasses(), 0);
	TestTrue(TEXT("Generated-class cleanup should discard the generated class module"), Metrics.LastActiveModuleDiscardCount >= 1);
	TestTrue(TEXT("Generated-class cleanup should inspect detached generated classes"), Metrics.LastDetachedClassCount >= 1);

	ShutdownTestEnginePool();
	return true;
}

static bool RunGeneratedStructCleanupIsBounded(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Shared_AngelscriptTestEnginePoolTests_Private;
	static const FName GeneratedStructName(TEXT("PoolGeneratedStruct"));

	ShutdownTestEnginePool();
	FAngelscriptTestEnginePool::Get().SetGarbageCollectEveryNCleanups(1);
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();

	TWeakObjectPtr<UASStruct> WeakGeneratedStruct;
	FString GeneratedStructPath;

	{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("PoolGeneratedStructModule"),
			TEXT("PoolGeneratedStructModule.as"),
			TEXT(R"(
USTRUCT()
struct FPoolGeneratedStruct
{
	UPROPERTY()
	int Value = 5;
}
)"));
		if (!TestTrue(TEXT("Generated-struct pool fixture should compile"), bCompiled))
		{
			return false;
		}

		UASStruct* GeneratedStruct = FindObject<UASStruct>(FAngelscriptEngine::GetPackage(), *GeneratedStructName.ToString());
		if (!TestNotNull(TEXT("Generated struct should be visible inside the scoped lease"), GeneratedStruct))
		{
			return false;
		}

		WeakGeneratedStruct = GeneratedStruct;
		GeneratedStructPath = GeneratedStruct->GetPathName();
	}

	UASStruct* FoundGeneratedStructByPath = FindObject<UASStruct>(nullptr, *GeneratedStructPath);
	const FAngelscriptTestEnginePoolMetrics Metrics = GetTestEnginePoolMetrics();
	TestFalse(TEXT("Module-clean generated struct weak pointer should be invalid after cleanup"), WeakGeneratedStruct.IsValid());
	TestNull(TEXT("Module-clean generated struct should not be findable by path after cleanup"), FoundGeneratedStructByPath);
	TestTrue(TEXT("Generated-struct cleanup should inspect detached generated structs"), Metrics.LastDetachedStructCount >= 1);

	ShutdownTestEnginePool();
	return !WeakGeneratedStruct.IsValid()
		&& FoundGeneratedStructByPath == nullptr
		&& Metrics.LastDetachedStructCount >= 1;
}

static bool RunGeneratedEnumDelegateCleanupIsBounded(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Shared_AngelscriptTestEnginePoolTests_Private;
	ShutdownTestEnginePool();
	FAngelscriptTestEnginePool::Get().SetGarbageCollectEveryNCleanups(1);
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();

	TWeakObjectPtr<UEnum> WeakGeneratedEnum;
	TWeakObjectPtr<UDelegateFunction> WeakGeneratedDelegateFunction;
	TWeakObjectPtr<UDelegateFunction> WeakGeneratedEventFunction;
	FString GeneratedEnumPath;
	FString GeneratedDelegateFunctionPath;
	FString GeneratedEventFunctionPath;

	{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("PoolGeneratedEnumDelegateModule"),
			TEXT("PoolGeneratedEnumDelegateModule.as"),
			TEXT(R"(
UENUM(BlueprintType)
enum class EPoolGeneratedState : uint8
{
	Idle,
	Active
}

delegate void FPoolGeneratedDelegate(int Value);
event void FPoolGeneratedEvent(int Value);
)"));
		if (!TestTrue(TEXT("Generated enum/delegate pool fixture should compile"), bCompiled))
		{
			return false;
		}

		const TSharedPtr<FAngelscriptEnumDesc> GeneratedEnumDesc = Engine.GetEnum(TEXT("EPoolGeneratedState"));
		if (!TestTrue(TEXT("Generated enum descriptor should be visible inside the scoped lease"), GeneratedEnumDesc.IsValid()))
		{
			return false;
		}
		UEnum* GeneratedEnum = GeneratedEnumDesc->Enum;
		if (!TestNotNull(TEXT("Generated enum UObject should be visible inside the scoped lease"), GeneratedEnum))
		{
			return false;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> GeneratedDelegateDesc = Engine.GetDelegate(TEXT("FPoolGeneratedDelegate"));
		if (!TestTrue(TEXT("Generated delegate descriptor should be visible inside the scoped lease"), GeneratedDelegateDesc.IsValid()))
		{
			return false;
		}
		TestFalse(TEXT("Generated delegate descriptor should be single-cast inside the scoped lease"), GeneratedDelegateDesc->bIsMulticast);
		UDelegateFunction* GeneratedDelegateFunction = GeneratedDelegateDesc->Function;
		if (!TestNotNull(TEXT("Generated delegate function should be visible inside the scoped lease"), GeneratedDelegateFunction))
		{
			return false;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> GeneratedEventDesc = Engine.GetDelegate(TEXT("FPoolGeneratedEvent"));
		if (!TestTrue(TEXT("Generated event descriptor should be visible inside the scoped lease"), GeneratedEventDesc.IsValid()))
		{
			return false;
		}
		TestTrue(TEXT("Generated event descriptor should be multicast inside the scoped lease"), GeneratedEventDesc->bIsMulticast);
		UDelegateFunction* GeneratedEventFunction = GeneratedEventDesc->Function;
		if (!TestNotNull(TEXT("Generated event function should be visible inside the scoped lease"), GeneratedEventFunction))
		{
			return false;
		}

		WeakGeneratedEnum = GeneratedEnum;
		WeakGeneratedDelegateFunction = GeneratedDelegateFunction;
		WeakGeneratedEventFunction = GeneratedEventFunction;
		GeneratedEnumPath = GeneratedEnum->GetPathName();
		GeneratedDelegateFunctionPath = GeneratedDelegateFunction->GetPathName();
		GeneratedEventFunctionPath = GeneratedEventFunction->GetPathName();
	}

	UEnum* FoundGeneratedEnumByPath = FindObject<UEnum>(nullptr, *GeneratedEnumPath);
	UDelegateFunction* FoundGeneratedDelegateFunctionByPath = FindObject<UDelegateFunction>(nullptr, *GeneratedDelegateFunctionPath);
	UDelegateFunction* FoundGeneratedEventFunctionByPath = FindObject<UDelegateFunction>(nullptr, *GeneratedEventFunctionPath);
	const FAngelscriptTestEnginePoolMetrics Metrics = GetTestEnginePoolMetrics();
	TestFalse(TEXT("Module-clean generated enum weak pointer should be invalid after cleanup"), WeakGeneratedEnum.IsValid());
	TestFalse(TEXT("Module-clean generated delegate function weak pointer should be invalid after cleanup"), WeakGeneratedDelegateFunction.IsValid());
	TestFalse(TEXT("Module-clean generated event function weak pointer should be invalid after cleanup"), WeakGeneratedEventFunction.IsValid());
	TestNull(TEXT("Module-clean generated enum should not be findable by path after cleanup"), FoundGeneratedEnumByPath);
	TestNull(TEXT("Module-clean generated delegate function should not be findable by path after cleanup"), FoundGeneratedDelegateFunctionByPath);
	TestNull(TEXT("Module-clean generated event function should not be findable by path after cleanup"), FoundGeneratedEventFunctionByPath);
	TestTrue(TEXT("Generated enum/delegate cleanup should inspect discarded generated enums"), Metrics.LastDiscardedEnumCount >= 1);
	TestTrue(TEXT("Generated enum/delegate cleanup should inspect discarded generated delegate functions"), Metrics.LastDiscardedDelegateFunctionCount >= 2);

	ShutdownTestEnginePool();
	return !WeakGeneratedEnum.IsValid()
		&& !WeakGeneratedDelegateFunction.IsValid()
		&& !WeakGeneratedEventFunction.IsValid()
		&& FoundGeneratedEnumByPath == nullptr
		&& FoundGeneratedDelegateFunctionByPath == nullptr
		&& FoundGeneratedEventFunctionByPath == nullptr
		&& Metrics.LastDiscardedEnumCount >= 1
		&& Metrics.LastDiscardedDelegateFunctionCount >= 2;
}

static bool RunGeneratedClassActionCacheIsCleared(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Shared_AngelscriptTestEnginePoolTests_Private;
	ShutdownTestEnginePool();
	FAngelscriptTestEnginePool::Get().SetGarbageCollectEveryNCleanups(1);
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();

	TWeakObjectPtr<UASClass> WeakGeneratedClass;
	FString GeneratedClassPath;

	{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("PoolGeneratedClassActionCacheModule"),
			TEXT("PoolGeneratedClassActionCacheModule.as"),
			TEXT(R"(
UCLASS()
class UPoolGeneratedClassActionCacheObject : UObject
{
	UPROPERTY()
	int Value = 3;

	UFUNCTION()
	int GetValue()
	{
		return Value;
	}
}
)"));
		if (!TestTrue(TEXT("Generated-class action-cache pool fixture should compile"), bCompiled))
		{
			return false;
		}

		UASClass* GeneratedClass = Cast<UASClass>(FindGeneratedClass(&Engine, TEXT("UPoolGeneratedClassActionCacheObject")));
		if (!TestNotNull(TEXT("Generated class should be visible before module-clean scope exits"), GeneratedClass))
		{
			return false;
		}

#if WITH_EDITOR
		FBlueprintActionDatabase::Get().RefreshClassActions(GeneratedClass);
#endif
		WeakGeneratedClass = GeneratedClass;
		GeneratedClassPath = GeneratedClass->GetPathName();
	}

	UASClass* FoundGeneratedClassByPath = FindObject<UASClass>(nullptr, *GeneratedClassPath);
	const FAngelscriptTestEnginePoolMetrics Metrics = GetTestEnginePoolMetrics();
	TestFalse(TEXT("Module-clean generated class weak pointer should be invalid after action-cache cleanup"), WeakGeneratedClass.IsValid());
	TestNull(TEXT("Module-clean generated class should not be findable by path after action-cache cleanup"), FoundGeneratedClassByPath);
	TestTrue(TEXT("Module-clean cleanup should clear generated-class Blueprint action entries"), Metrics.LastBlueprintActionCacheClearedCount >= 1);

	ShutdownTestEnginePool();
	return !WeakGeneratedClass.IsValid()
		&& FoundGeneratedClassByPath == nullptr
		&& Metrics.LastBlueprintActionCacheClearedCount >= 1;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull
#undef TestNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptTestEnginePoolTests,
	"Angelscript.TestModule.Shared.TestEnginePool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PrewarmCachesBindDatabase)
	{
		ASSERT_THAT(IsTrue(RunPrewarmCachesBindDatabase(*TestRunner)));
	}

	TEST_METHOD(ModuleCleanDiscardsOnlyDelta)
	{
		ASSERT_THAT(IsTrue(RunModuleCleanDiscardsOnlyDelta(*TestRunner)));
	}

	TEST_METHOD(GeneratedClassCleanupIsBounded)
	{
		ASSERT_THAT(IsTrue(RunGeneratedClassCleanupIsBounded(*TestRunner)));
	}

	TEST_METHOD(GeneratedStructCleanupIsBounded)
	{
		ASSERT_THAT(IsTrue(RunGeneratedStructCleanupIsBounded(*TestRunner)));
	}

	TEST_METHOD(GeneratedEnumDelegateCleanupIsBounded)
	{
		ASSERT_THAT(IsTrue(RunGeneratedEnumDelegateCleanupIsBounded(*TestRunner)));
	}

	TEST_METHOD(GeneratedClassActionCacheIsCleared)
	{
		ASSERT_THAT(IsTrue(RunGeneratedClassActionCacheIsCleared(*TestRunner)));
	}
};

#endif
