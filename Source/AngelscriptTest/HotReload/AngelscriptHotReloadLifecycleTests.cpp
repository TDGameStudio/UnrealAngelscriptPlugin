#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadLifecycleTests,
	"Angelscript.TestModule.HotReload.SoftReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName LifecycleModuleName = FName(TEXT("HotReloadLifecycle"));
	inline static const FString LifecycleFilename = FString(TEXT("HotReloadLifecycle.as"));
	inline static const FName LifecycleClassName = FName(TEXT("AHotReloadLifecycleTarget"));

	static void InitializeLifecycleSpawner(FActorTestSpawner& Spawner)
	{
		Spawner.InitializeGameSubsystems();
	}

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool ExecuteGetValue(
		FAutomationTestBase& Test,
		AActor* Actor,
		UFunction* Function,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Actor, *FString::Printf(TEXT("%s should have a live actor"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsNotNull(Function, *FString::Printf(TEXT("%s should expose GetValue"), Context)))
		{
			return false;
		}

		int32 Result = 0;
		if (!LocalAssert.IsTrue(
				ExecuteGeneratedIntEventOnGameThread(Actor, Function, Result),
				*FString::Printf(TEXT("%s should execute GetValue"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedValue,
			Result,
			*FString::Printf(TEXT("%s should surface the expected GetValue result"), Context));
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

	TEST_METHOD(DoesNotReplayBeginPlayOnLiveActor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*LifecycleModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadLifecycleTarget : AActor
			{
				UPROPERTY()
				int BeginPlayCount = 0;

				UPROPERTY()
				int PersistentCounter = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
					Log(n"HotReloadLifecycleTests", "V1 BeginPlay Count=" + BeginPlayCount + " PersistentCounter=" + PersistentCounter);
				}

				UFUNCTION()
				int GetValue()
				{
					Log(n"HotReloadLifecycleTests", "V1 GetValue PersistentCounter=" + PersistentCounter);
					return PersistentCounter;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadLifecycleTarget : AActor
			{
				UPROPERTY()
				int BeginPlayCount = 0;

				UPROPERTY()
				int PersistentCounter = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
					Log(n"HotReloadLifecycleTests", "V2 BeginPlay Count=" + BeginPlayCount + " PersistentCounter=" + PersistentCounter);
				}

				UFUNCTION()
				int GetValue()
				{
					int Result = PersistentCounter + 1;
					Log(n"HotReloadLifecycleTests", "V2 GetValue PersistentCounter=" + PersistentCounter + " Result=" + Result);
					return Result;
				}
			}
			)AS");

		UClass* InitialClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			LifecycleModuleName,
			LifecycleFilename,
			ScriptV1,
			LifecycleClassName);
		ASSERT_THAT(IsNotNull(InitialClass, TEXT("Soft reload lifecycle test case should compile the initial script class")));

		FIntProperty* BeginPlayCountProperty = FindFProperty<FIntProperty>(InitialClass, TEXT("BeginPlayCount"));
		FIntProperty* PersistentCounterProperty = FindFProperty<FIntProperty>(InitialClass, TEXT("PersistentCounter"));
		UFunction* GetValueBeforeReload = FindGeneratedFunction(InitialClass, TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(BeginPlayCountProperty, TEXT("Soft reload lifecycle test case should expose BeginPlayCount before reload")));
		ASSERT_THAT(IsNotNull(PersistentCounterProperty, TEXT("Soft reload lifecycle test case should expose PersistentCounter before reload")));
		ASSERT_THAT(IsNotNull(GetValueBeforeReload, TEXT("Soft reload lifecycle test case should expose GetValue before reload")));

		FActorTestSpawner Spawner;
		InitializeLifecycleSpawner(Spawner);

		AActor* ExistingActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, InitialClass);
		ASSERT_THAT(IsNotNull(ExistingActor, TEXT("Soft reload lifecycle test case should spawn the script actor")));

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *ExistingActor);

		int32 BeginPlayCountBeforeReload = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, ExistingActor, TEXT("BeginPlayCount"), BeginPlayCountBeforeReload),
			TEXT("Soft reload lifecycle test case should read BeginPlayCount before reload")));

		ASSERT_THAT(AreEqual(
			1,
			BeginPlayCountBeforeReload,
			TEXT("Soft reload lifecycle test case should invoke BeginPlay exactly once before reload")));

		PersistentCounterProperty->SetPropertyValue_InContainer(ExistingActor, 41);
		ASSERT_THAT(IsTrue(ExecuteGetValue(
			*TestRunner,
			ExistingActor,
			GetValueBeforeReload,
			41,
			TEXT("Soft reload lifecycle test case before reload"))));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, LifecycleModuleName, LifecycleFilename, ScriptV2, ReloadResult),
			TEXT("Soft reload lifecycle test case should compile the body-only update on the soft reload path")));

		ASSERT_THAT(IsTrue(
			IsHandledReloadResult(ReloadResult),
			TEXT("Soft reload lifecycle test case should remain on a handled soft reload path")));

		UClass* ReloadedClass = FindGeneratedClass(&Engine, LifecycleClassName);
		ASSERT_THAT(IsNotNull(ReloadedClass, TEXT("Soft reload lifecycle test case should still expose the generated class after reload")));
		ASSERT_THAT(AreEqual(InitialClass, ReloadedClass, TEXT("Soft reload lifecycle test case should preserve the live UClass object")));
		ASSERT_THAT(AreEqual(ReloadedClass, ExistingActor->GetClass(), TEXT("Soft reload lifecycle test case should keep the live actor on the preserved class")));
		ASSERT_THAT(IsTrue(ExistingActor->HasActorBegunPlay(), TEXT("Soft reload lifecycle test case should keep the actor in begun-play state")));

		FIntProperty* ReloadedBeginPlayCountProperty = FindFProperty<FIntProperty>(ReloadedClass, TEXT("BeginPlayCount"));
		FIntProperty* ReloadedPersistentCounterProperty = FindFProperty<FIntProperty>(ReloadedClass, TEXT("PersistentCounter"));
		UFunction* GetValueAfterReload = FindGeneratedFunction(ReloadedClass, TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(ReloadedBeginPlayCountProperty, TEXT("Soft reload lifecycle test case should still expose BeginPlayCount after reload")));
		ASSERT_THAT(IsNotNull(ReloadedPersistentCounterProperty, TEXT("Soft reload lifecycle test case should still expose PersistentCounter after reload")));
		ASSERT_THAT(IsNotNull(GetValueAfterReload, TEXT("Soft reload lifecycle test case should still expose GetValue after reload")));

		int32 BeginPlayCountAfterReload = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, ExistingActor, TEXT("BeginPlayCount"), BeginPlayCountAfterReload),
			TEXT("Soft reload lifecycle test case should read BeginPlayCount after reload")));

		ASSERT_THAT(AreEqual(
			1,
			BeginPlayCountAfterReload,
			TEXT("Soft reload lifecycle test case should not replay BeginPlay on the live actor after reload")));

		int32 PersistentCounterAfterReload = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, ExistingActor, TEXT("PersistentCounter"), PersistentCounterAfterReload),
			TEXT("Soft reload lifecycle test case should read PersistentCounter after reload")));

		ASSERT_THAT(AreEqual(
			41,
			PersistentCounterAfterReload,
			TEXT("Soft reload lifecycle test case should preserve the live actor runtime state after reload")));

		ASSERT_THAT(IsTrue(ExecuteGetValue(
			*TestRunner,
			ExistingActor,
			GetValueAfterReload,
			42,
			TEXT("Soft reload lifecycle test case after reload"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
