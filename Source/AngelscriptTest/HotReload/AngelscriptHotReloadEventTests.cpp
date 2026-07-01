#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadEventTests,
	"Angelscript.TestModule.HotReload.Events",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName PostReloadModeModuleName = FName(TEXT("HotReloadPostReloadModeMod"));
	inline static const FString PostReloadModeFilename = FString(TEXT("HotReloadPostReloadModeMod.as"));
	inline static const FName PostReloadModeClassName = FName(TEXT("UPostReloadModeTarget"));

	inline static const FName FailedReloadModuleName = FName(TEXT("HotReloadFailedReloadEventMod"));
	inline static const FString FailedReloadFilename = FString(TEXT("HotReloadFailedReloadEventMod.as"));
	inline static const FName FailedReloadClassName = FName(TEXT("UFailedReloadEventTarget"));

	struct FPostReloadObservation
	{
		bool bWasFullReload = false;
		UClass* VisibleClass = nullptr;
	};

	struct FClassReloadObservation
	{
		UClass* OldClass = nullptr;
		UClass* NewClass = nullptr;
	};

	struct FReloadEventObservation
	{
		TArray<FPostReloadObservation> PostReloads;
		TArray<FClassReloadObservation> ClassReloads;
		int32 FullReloadCount = 0;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool IsFailedReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::Error || ReloadResult == ECompileResult::ErrorNeedFullReload;
	}

	static bool ExecuteGetValue(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UClass* Class,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Class, *FString::Printf(TEXT("%s should expose the generated class"), Context)))
		{
			return false;
		}

		UFunction* GetValueFunction = FindGeneratedFunction(Class, TEXT("GetValue"));
		if (!LocalAssert.IsNotNull(GetValueFunction, *FString::Printf(TEXT("%s should expose GetValue"), Context)))
		{
			return false;
		}

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), Class);
		if (!LocalAssert.IsNotNull(RuntimeObject, *FString::Printf(TEXT("%s should instantiate the generated class"), Context)))
		{
			return false;
		}

		int32 Result = 0;
		if (!LocalAssert.IsTrue(
				ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, GetValueFunction, Result),
				*FString::Printf(TEXT("%s should execute GetValue on the game thread"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedValue,
			Result,
			*FString::Printf(TEXT("%s should surface the expected GetValue result"), Context));
	}

	static void RegisterFailedReloadExpectedErrors(FAutomationTestBase& Test)
	{
		Test.AddExpectedError(TEXT("HotReloadFailedReloadEventMod.as:"), EAutomationExpectedErrorFlags::Contains, 2);
		Test.AddExpectedError(TEXT("Identifier 'MissingType' is not a data type in global namespace"), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedError(TEXT("Identifier 'MissingType' is not a data type"), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedError(TEXT("Hot reload failed due to script compile errors. Keeping all old script code."), EAutomationExpectedErrorFlags::Contains, 1);
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

	TEST_METHOD(PostReloadModeFlagMatchesReloadPath)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			TArray<FPostReloadObservation> PostReloadObservations;
			FDelegateHandle PostReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnPostReload().Remove(PostReloadHandle);
				Engine.DiscardModule(*PostReloadModeModuleName.ToString());
			};

			const FString ScriptV1 = ASTEST_AS(R"AS(
				UCLASS()
				class UPostReloadModeTarget : UObject
				{
					UFUNCTION()
					int GetValue()
					{
						int Result = 1;
						Log(n"HotReloadEventTests", "PostReloadMode V1 GetValue Result=" + Result);
						return Result;
					}
				}
				)AS");

			const FString ScriptV2 = ASTEST_AS(R"AS(
				UCLASS()
				class UPostReloadModeTarget : UObject
				{
					UFUNCTION()
					int GetValue()
					{
						int Result = 2;
						Log(n"HotReloadEventTests", "PostReloadMode V2 GetValue Result=" + Result);
						return Result;
					}
				}
				)AS");

			const FString ScriptV3 = ASTEST_AS(R"AS(
				UCLASS()
				class UPostReloadModeTarget : UObject
				{
					UPROPERTY()
					int Epoch = 3;

					UFUNCTION()
					int GetValue()
					{
						Log(n"HotReloadEventTests", "PostReloadMode V3 GetValue Epoch=" + Epoch);
						return Epoch;
					}
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, PostReloadModeModuleName, PostReloadModeFilename, ScriptV1),
				TEXT("Post-reload mode-flag test should compile the initial module")));

			UClass* InitialClass = FindGeneratedClass(&Engine, PostReloadModeClassName);
			ASSERT_THAT(IsTrue(ExecuteGetValue(*TestRunner, Engine, InitialClass, 1, TEXT("Initial post-reload mode-flag baseline"))));

			PostReloadHandle = Engine.GetOnPostReload().AddLambda(
				[&Engine, &PostReloadObservations](const bool bWasFullReload)
				{
					FPostReloadObservation& Observation = PostReloadObservations.AddDefaulted_GetRef();
					Observation.bWasFullReload = bWasFullReload;
					Observation.VisibleClass = FindGeneratedClass(&Engine, PostReloadModeClassName);
				});

			ECompileResult SoftReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileModuleWithResult(
					&Engine,
					ECompileType::SoftReloadOnly,
					PostReloadModeModuleName,
					PostReloadModeFilename,
					ScriptV2,
					SoftReloadResult),
				TEXT("Post-reload mode-flag test should compile the body-only update on the soft reload path")));

			ASSERT_THAT(IsTrue(IsHandledReloadResult(SoftReloadResult), TEXT("Soft reload should stay on a handled reload path")));

			UClass* ClassAfterSoftReload = FindGeneratedClass(&Engine, PostReloadModeClassName);
			ASSERT_THAT(IsNotNull(ClassAfterSoftReload, TEXT("Soft reload should keep the generated class visible")));
			ASSERT_THAT(AreEqual(InitialClass, ClassAfterSoftReload, TEXT("Soft reload should preserve the live UClass object")));

			ASSERT_THAT(AreEqual(1, PostReloadObservations.Num(), TEXT("Soft reload should trigger exactly one post-reload event")));
			if (PostReloadObservations.Num() >= 1)
			{
				ASSERT_THAT(IsFalse(PostReloadObservations[0].bWasFullReload, TEXT("Soft reload should be reported as soft reload by the post-reload event")));
				ASSERT_THAT(AreEqual(ClassAfterSoftReload, PostReloadObservations[0].VisibleClass, TEXT("Soft reload should already expose the canonical class when post-reload broadcasts")));
			}

			ASSERT_THAT(IsTrue(ExecuteGetValue(*TestRunner, Engine, ClassAfterSoftReload, 2, TEXT("Soft reload post-reload mode-flag baseline"))));

			ECompileResult FullReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileModuleWithResult(
					&Engine,
					ECompileType::FullReload,
					PostReloadModeModuleName,
					PostReloadModeFilename,
					ScriptV3,
					FullReloadResult),
				TEXT("Post-reload mode-flag test should compile the structural update on the full reload path")));

			ASSERT_THAT(IsTrue(IsHandledReloadResult(FullReloadResult), TEXT("Full reload should stay on a handled reload path")));

			UClass* ClassAfterFullReload = FindGeneratedClass(&Engine, PostReloadModeClassName);
			ASSERT_THAT(IsNotNull(ClassAfterFullReload, TEXT("Full reload should keep the generated class visible")));

			ASSERT_THAT(AreEqual(2, PostReloadObservations.Num(), TEXT("Full reload should append a second post-reload event")));
			if (PostReloadObservations.Num() >= 2)
			{
				ASSERT_THAT(IsTrue(PostReloadObservations[1].bWasFullReload, TEXT("Full reload should be reported as full reload by the post-reload event")));
				ASSERT_THAT(AreEqual(ClassAfterFullReload, PostReloadObservations[1].VisibleClass, TEXT("Full reload should already expose the canonical class when post-reload broadcasts")));
			}

			ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ClassAfterFullReload, TEXT("Epoch")), TEXT("Full reload should expose the newly added Epoch property")));
			ASSERT_THAT(IsTrue(ExecuteGetValue(*TestRunner, Engine, ClassAfterFullReload, 3, TEXT("Full reload post-reload mode-flag baseline"))));
		}
	}

	TEST_METHOD(FailedReloadDoesNotBroadcastReloadDelegates)
	{
		RegisterFailedReloadExpectedErrors(*TestRunner);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FReloadEventObservation ReloadEvents;
			FDelegateHandle PostReloadHandle;
			FDelegateHandle ClassReloadHandle;
			FDelegateHandle FullReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnPostReload().Remove(PostReloadHandle);
				Engine.GetOnClassReload().Remove(ClassReloadHandle);
				Engine.GetOnFullReload().Remove(FullReloadHandle);
				Engine.DiscardModule(*FailedReloadModuleName.ToString());
			};

			const FString ScriptV1 = ASTEST_AS(R"AS(
				UCLASS()
				class UFailedReloadEventTarget : UObject
				{
					UFUNCTION()
					int GetValue()
					{
						int Result = 5;
						Log(n"HotReloadEventTests", "FailedReload V1 GetValue Result=" + Result);
						return Result;
					}
				}
				)AS");

			const FString BrokenScript = ASTEST_AS(R"AS(
				UCLASS()
				class UFailedReloadEventTarget : UObject
				{
					UFUNCTION()
					MissingType GetValue()
					{
						MissingType Value;
						return Value;
					}
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, FailedReloadModuleName, FailedReloadFilename, ScriptV1),
				TEXT("Failed-reload event test should compile the initial module")));

			UClass* ClassBeforeFailure = FindGeneratedClass(&Engine, FailedReloadClassName);
			ASSERT_THAT(IsNotNull(ClassBeforeFailure, TEXT("Failed-reload event test should expose the generated class before reload failure")));
			ASSERT_THAT(IsTrue(ExecuteGetValue(*TestRunner, Engine, ClassBeforeFailure, 5, TEXT("Failed-reload event baseline"))));

			PostReloadHandle = Engine.GetOnPostReload().AddLambda(
				[&ReloadEvents](const bool bWasFullReload)
				{
					FPostReloadObservation& Observation = ReloadEvents.PostReloads.AddDefaulted_GetRef();
					Observation.bWasFullReload = bWasFullReload;
				});

			ClassReloadHandle = Engine.GetOnClassReload().AddLambda(
				[&ReloadEvents](UClass* OldClass, UClass* NewClass)
				{
					FClassReloadObservation& Observation = ReloadEvents.ClassReloads.AddDefaulted_GetRef();
					Observation.OldClass = OldClass;
					Observation.NewClass = NewClass;
				});

			FullReloadHandle = Engine.GetOnFullReload().AddLambda(
				[&ReloadEvents]()
				{
					++ReloadEvents.FullReloadCount;
				});

			ECompileResult ReloadResult = ECompileResult::FullyHandled;
			const bool bCompiled = CompileModuleWithResult(
				&Engine,
				ECompileType::SoftReloadOnly,
				FailedReloadModuleName,
				FailedReloadFilename,
				BrokenScript,
				ReloadResult);

			ASSERT_THAT(IsFalse(bCompiled, TEXT("Failed-reload event test should fail the broken hot reload compile")));
			ASSERT_THAT(IsTrue(IsFailedReloadResult(ReloadResult), TEXT("Failed-reload event test should report an error reload state")));
			ASSERT_THAT(AreEqual(0, ReloadEvents.PostReloads.Num(), TEXT("Failed-reload event test should not broadcast post-reload when compilation fails")));
			ASSERT_THAT(AreEqual(0, ReloadEvents.ClassReloads.Num(), TEXT("Failed-reload event test should not broadcast class-reload when compilation fails")));
			ASSERT_THAT(AreEqual(0, ReloadEvents.FullReloadCount, TEXT("Failed-reload event test should not broadcast full-reload when compilation fails")));

			UClass* ClassAfterFailure = FindGeneratedClass(&Engine, FailedReloadClassName);
			ASSERT_THAT(AreEqual(ClassBeforeFailure, ClassAfterFailure, TEXT("Failed-reload event test should keep the old generated class visible after the failed reload")));
			ASSERT_THAT(IsTrue(ExecuteGetValue(*TestRunner, Engine, ClassAfterFailure, 5, TEXT("Failed-reload event fallback"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
