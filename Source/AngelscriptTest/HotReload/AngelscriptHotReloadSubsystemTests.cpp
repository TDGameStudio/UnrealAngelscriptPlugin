#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Subsystem/ScriptGameInstanceSubsystem.h"
#include "Subsystem/ScriptWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadSubsystemTests,
	"Angelscript.TestModule.HotReload.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName WorldSubsystemModuleName = FName(TEXT("HotReloadSubsystemWorld"));
	inline static const FString WorldSubsystemFilename = FString(TEXT("HotReloadSubsystemWorld.as"));
	inline static const FName WorldSubsystemClassName = FName(TEXT("UHotReloadWorldSubsystemTarget"));

	inline static const FName GameInstanceSubsystemModuleName = FName(TEXT("HotReloadSubsystemGameInstance"));
	inline static const FString GameInstanceSubsystemFilename = FString(TEXT("HotReloadSubsystemGameInstance.as"));
	inline static const FName GameInstanceSubsystemClassName = FName(TEXT("UHotReloadGameInstanceSubsystemTarget"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static UObject* NewSubsystemInstance(FAutomationTestBase& Test, UClass* SubsystemClass, UObject* Outer, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(SubsystemClass, *FString::Printf(TEXT("%s should have a generated subsystem class"), Context)))
		{
			return nullptr;
		}

		if (!LocalAssert.IsNotNull(Outer, *FString::Printf(TEXT("%s should have a subsystem outer"), Context)))
		{
			return nullptr;
		}

		UObject* Instance = NewObject<UObject>(Outer, SubsystemClass);
		if (!LocalAssert.IsNotNull(Instance, *FString::Printf(TEXT("%s should create a subsystem instance"), Context)))
		{
			return nullptr;
		}

		return Instance;
	}

	static bool ExecuteSubsystemValue(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Instance,
		UClass* OwnerClass,
		int32 ExpectedValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Instance, *FString::Printf(TEXT("%s should have an instance"), Context)))
		{
			return false;
		}

		UFunction* GetValueFunction = FindGeneratedFunction(OwnerClass, TEXT("GetValue"));
		if (!LocalAssert.IsNotNull(GetValueFunction, *FString::Printf(TEXT("%s should expose GetValue"), Context)))
		{
			return false;
		}

		int32 ActualValue = 0;
		if (!LocalAssert.IsTrue(
				ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GetValueFunction, ActualValue),
				*FString::Printf(TEXT("%s should execute GetValue"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedValue, ActualValue, *FString::Printf(TEXT("%s should observe the expected value"), Context));
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

	TEST_METHOD(WorldSubsystemSoftReloadUpdatesCallableBehavior)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		UWorld* TestWorldOuter = NewObject<UWorld>(GetTransientPackage(), TEXT("HotReloadWorldSubsystemOuter"));
		ASSERT_THAT(IsNotNull(TestWorldOuter, TEXT("World subsystem hot reload test should create a world outer")));

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*WorldSubsystemModuleName.ToString());
			if (TestWorldOuter != nullptr)
			{
				TestWorldOuter->MarkAsGarbage();
			}
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadWorldSubsystemTarget : UScriptWorldSubsystem
			{
				UFUNCTION()
				int GetValue()
				{
					return 41;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, WorldSubsystemModuleName, WorldSubsystemFilename, ReloadV1Source),
			TEXT("Initial world subsystem hot reload module should compile")));

		UClass* ClassBeforeReload = FindGeneratedClass(&Engine, WorldSubsystemClassName);
		ASSERT_THAT(IsNotNull(ClassBeforeReload, TEXT("World subsystem class should exist before reload")));
		ASSERT_THAT(IsTrue(ClassBeforeReload->IsChildOf(UScriptWorldSubsystem::StaticClass()), TEXT("Generated class should derive from UScriptWorldSubsystem")));

		UObject* SubsystemInstance = NewSubsystemInstance(*TestRunner, ClassBeforeReload, TestWorldOuter, TEXT("World subsystem V1"));
		ASSERT_THAT(IsTrue(ExecuteSubsystemValue(*TestRunner, Engine, SubsystemInstance, ClassBeforeReload, 41, TEXT("World subsystem V1"))));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadWorldSubsystemTarget : UScriptWorldSubsystem
			{
				UFUNCTION()
				int GetValue()
				{
					return 64;
				}
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, WorldSubsystemModuleName, WorldSubsystemFilename, ReloadV2Source, ReloadResult),
			TEXT("World subsystem soft reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("World subsystem soft reload should be handled")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, WorldSubsystemClassName);
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("World subsystem class should exist after reload")));
		ASSERT_THAT(AreEqual(ClassBeforeReload, ClassAfterReload, TEXT("World subsystem soft reload should preserve UClass identity")));
		ASSERT_THAT(IsTrue(ExecuteSubsystemValue(*TestRunner, Engine, SubsystemInstance, ClassAfterReload, 64, TEXT("World subsystem V2 existing instance"))));
	}

	TEST_METHOD(GameInstanceSubsystemSoftReloadUpdatesCallableBehavior)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		UGameInstance* TestGameInstanceOuter = NewObject<UGameInstance>(GetTransientPackage(), TEXT("HotReloadGameInstanceSubsystemOuter"));
		ASSERT_THAT(IsNotNull(TestGameInstanceOuter, TEXT("Game-instance subsystem hot reload test should create a game-instance outer")));

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*GameInstanceSubsystemModuleName.ToString());
			if (TestGameInstanceOuter != nullptr)
			{
				TestGameInstanceOuter->MarkAsGarbage();
			}
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadGameInstanceSubsystemTarget : UScriptGameInstanceSubsystem
			{
				UFUNCTION()
				int GetValue()
				{
					return 13;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, GameInstanceSubsystemModuleName, GameInstanceSubsystemFilename, ReloadV1Source),
			TEXT("Initial game-instance subsystem hot reload module should compile")));

		UClass* ClassBeforeReload = FindGeneratedClass(&Engine, GameInstanceSubsystemClassName);
		ASSERT_THAT(IsNotNull(ClassBeforeReload, TEXT("Game-instance subsystem class should exist before reload")));
		ASSERT_THAT(IsTrue(ClassBeforeReload->IsChildOf(UScriptGameInstanceSubsystem::StaticClass()), TEXT("Generated class should derive from UScriptGameInstanceSubsystem")));

		UObject* SubsystemInstance = NewSubsystemInstance(*TestRunner, ClassBeforeReload, TestGameInstanceOuter, TEXT("Game-instance subsystem V1"));
		ASSERT_THAT(IsTrue(ExecuteSubsystemValue(*TestRunner, Engine, SubsystemInstance, ClassBeforeReload, 13, TEXT("Game-instance subsystem V1"))));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadGameInstanceSubsystemTarget : UScriptGameInstanceSubsystem
			{
				UFUNCTION()
				int GetValue()
				{
					return 31;
				}
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, GameInstanceSubsystemModuleName, GameInstanceSubsystemFilename, ReloadV2Source, ReloadResult),
			TEXT("Game-instance subsystem soft reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Game-instance subsystem soft reload should be handled")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, GameInstanceSubsystemClassName);
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("Game-instance subsystem class should exist after reload")));
		ASSERT_THAT(AreEqual(ClassBeforeReload, ClassAfterReload, TEXT("Game-instance subsystem soft reload should preserve UClass identity")));
		ASSERT_THAT(IsTrue(ExecuteSubsystemValue(*TestRunner, Engine, SubsystemInstance, ClassAfterReload, 31, TEXT("Game-instance subsystem V2 existing instance"))));
	}
};

#endif
