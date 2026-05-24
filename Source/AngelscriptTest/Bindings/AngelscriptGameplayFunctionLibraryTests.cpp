// ============================================================================
// AngelscriptGameplayFunctionLibraryTests.cpp
//
// Gameplay function library async save/load delegate binding coverage — CQTest
// refactor. Automation IDs:
//   Angelscript.TestModule.FunctionLibraries.Gameplay.FAngelscriptGameplayFunctionLibraryTest.*
//
// Sections:
//   AsyncSaveLoadDelegates        — async save + load round-trip + missing slot
//   ImmediateFailureCallbacks     — null-save, empty-slot, missing-slot error paths
//
// CQTest adaptation notes:
//   Two IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   Async harness pattern preserved with pumped callbacks.
//   Uses `*TestRunner` instead of `this` for assertions.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"

#include "AngelscriptGameplayFunctionLibraryTestTypes.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptFunctionalTestUtils;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace
{
	static const FName GameplayFunctionLibraryModuleName(TEXT("ASGameplayFunctionLibraryAsyncSaveLoad"));
	static const FString GameplayFunctionLibraryFilename(TEXT("GameplayFunctionLibraryAsyncSaveLoad.as"));
	static const FName GameplayFunctionLibraryClassName(TEXT("UAsyncSaveLoadScriptHarness"));
	static const FName GameplayFunctionLibraryImmediateFailureModuleName(TEXT("ASGameplayFunctionLibraryImmediateFailure"));
	static const FString GameplayFunctionLibraryImmediateFailureFilename(TEXT("GameplayFunctionLibraryImmediateFailure.as"));
	static const FName GameplayFunctionLibraryImmediateFailureClassName(TEXT("UAsyncSaveLoadImmediateFailureScriptHarness"));
	static constexpr double AsyncSaveLoadTimeoutSeconds = 5.0;

	struct FStartAsyncSaveParams
	{
		USaveGame* SaveGameObject = nullptr;
		UObject* Receiver = nullptr;
		FString SlotName;
		int32 UserIndex = 0;
	};

	struct FStartAsyncLoadParams
	{
		UObject* Receiver = nullptr;
		FString SlotName;
		int32 UserIndex = 0;
	};

	bool InvokeGeneratedVoidMethod(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UClass* OwnerClass,
		FName FunctionName,
		void* Params)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("Gameplay function library script method '%s' should exist"), *FunctionName.ToString()),
			Function))
		{
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine, Object);
		Object->ProcessEvent(Function, Params);
		return true;
	}

	void PumpAsyncSaveLoadCallbacks()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
		FTSTicker::GetCoreTicker().Tick(0.0f);
		FPlatformProcess::Sleep(0.001f);
	}

	bool WaitUntil(
		FAutomationTestBase& Test,
		TFunctionRef<bool()> Predicate,
		double TimeoutSeconds,
		const TCHAR* FailureContext)
	{
		const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
		while (FPlatformTime::Seconds() < Deadline)
		{
			if (Predicate())
			{
				return true;
			}

			PumpAsyncSaveLoadCallbacks();
		}

		Test.AddError(FString::Printf(TEXT("%s did not complete within %.2f seconds."), FailureContext, TimeoutSeconds));
		return false;
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptGameplayFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.Gameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: AsyncSaveLoadDelegates
	// ====================================================================

	TEST_METHOD(AsyncSaveLoadDelegates)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*GameplayFunctionLibraryModuleName.ToString());
		};

		const FString SlotName = FString::Printf(TEXT("AsyncSaveLoad_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString MissingSlotName = FString::Printf(TEXT("%s_Missing"), *SlotName);
		constexpr int32 UserIndex = 7;
		constexpr int32 ExpectedMarker = 1337;

		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
		UGameplayStatics::DeleteGameInSlot(MissingSlotName, UserIndex);
		ON_SCOPE_EXIT
		{
			UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
			UGameplayStatics::DeleteGameInSlot(MissingSlotName, UserIndex);
		};

		UClass* ScriptHarnessClass = CompileScriptModule(
			*TestRunner,
			Engine,
			GameplayFunctionLibraryModuleName,
			GameplayFunctionLibraryFilename,
			TEXT(R"AS(
UCLASS()
class UAsyncSaveLoadScriptHarness : UObject
{
	UFUNCTION()
	void StartAsyncSave(USaveGame SaveGameObject, UObject Receiver, const FString& SlotName, int32 UserIndex)
	{
		FAsyncSaveGameToSlotDynamicDelegate SaveDelegate;
		SaveDelegate.BindUFunction(Receiver, n"OnSaveComplete");
		UGameplayLibrary::AsyncSaveGameToSlot(SaveGameObject, SlotName, UserIndex, SaveDelegate);
	}

	UFUNCTION()
	void StartAsyncLoad(UObject Receiver, const FString& SlotName, int32 UserIndex)
	{
		FAsyncLoadGameFromSlotDynamicDelegate LoadDelegate;
		LoadDelegate.BindUFunction(Receiver, n"OnLoadComplete");
		UGameplayLibrary::AsyncLoadGameFromSlot(SlotName, UserIndex, LoadDelegate);
	}
}
)AS"),
			GameplayFunctionLibraryClassName);
		if (ScriptHarnessClass == nullptr)
		{
			return;
		}

		UObject* ScriptHarness = NewObject<UObject>(GetTransientPackage(), ScriptHarnessClass, TEXT("AsyncSaveLoadScriptHarness"));
		UAngelscriptAsyncSaveLoadCallbackRecorder* Recorder = NewObject<UAngelscriptAsyncSaveLoadCallbackRecorder>(GetTransientPackage(), TEXT("AsyncSaveLoadRecorder"));
		UAngelscriptAsyncSaveGameTestObject* SaveGameObject = NewObject<UAngelscriptAsyncSaveGameTestObject>(GetTransientPackage(), TEXT("AsyncSaveLoadSaveGame"));
		if (!TestRunner->TestNotNull(TEXT("Async save/load delegate test case should create the script harness"), ScriptHarness)
			|| !TestRunner->TestNotNull(TEXT("Async save/load delegate test case should create the callback recorder"), Recorder)
			|| !TestRunner->TestNotNull(TEXT("Async save/load delegate test case should create the save object"), SaveGameObject))
		{
			return;
		}

		ScriptHarness->AddToRoot();
		Recorder->AddToRoot();
		SaveGameObject->AddToRoot();
		ON_SCOPE_EXIT
		{
			SaveGameObject->RemoveFromRoot();
			Recorder->RemoveFromRoot();
			ScriptHarness->RemoveFromRoot();
		};

		SaveGameObject->Marker = ExpectedMarker;
		Recorder->ResetSaveState();
		Recorder->ResetLoadState();

		FStartAsyncSaveParams SaveParams;
		SaveParams.SaveGameObject = SaveGameObject;
		SaveParams.Receiver = Recorder;
		SaveParams.SlotName = SlotName;
		SaveParams.UserIndex = UserIndex;
		if (!InvokeGeneratedVoidMethod(*TestRunner, Engine, ScriptHarness, ScriptHarnessClass, TEXT("StartAsyncSave"), &SaveParams))
		{
			return;
		}

		if (!WaitUntil(
			*TestRunner,
			[Recorder]() { return Recorder->SaveCallbackCount >= 1; },
			AsyncSaveLoadTimeoutSeconds,
			TEXT("Async save callback")))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Async save helper should invoke the callback exactly once"), Recorder->SaveCallbackCount, 1);
		TestRunner->TestEqual(TEXT("Async save helper should forward the original slot name"), Recorder->SaveSlotName, SlotName);
		TestRunner->TestEqual(TEXT("Async save helper should forward the original user index"), Recorder->SaveUserIndex, UserIndex);
		TestRunner->TestTrue(TEXT("Async save helper should report save success"), Recorder->bLastSaveSuccess);
		TestRunner->TestTrue(TEXT("Async save helper should dispatch the callback on the game thread"), Recorder->bSaveCallbackOnGameThread);
		TestRunner->TestTrue(TEXT("Async save helper should create the slot on disk"), UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex));

		Recorder->ResetLoadState();
		FStartAsyncLoadParams LoadParams;
		LoadParams.Receiver = Recorder;
		LoadParams.SlotName = SlotName;
		LoadParams.UserIndex = UserIndex;
		if (!InvokeGeneratedVoidMethod(*TestRunner, Engine, ScriptHarness, ScriptHarnessClass, TEXT("StartAsyncLoad"), &LoadParams))
		{
			return;
		}

		if (!WaitUntil(
			*TestRunner,
			[Recorder]() { return Recorder->LoadCallbackCount >= 1; },
			AsyncSaveLoadTimeoutSeconds,
			TEXT("Async load callback")))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Async load helper should invoke the callback exactly once"), Recorder->LoadCallbackCount, 1);
		TestRunner->TestEqual(TEXT("Async load helper should forward the original slot name"), Recorder->LoadSlotName, SlotName);
		TestRunner->TestEqual(TEXT("Async load helper should forward the original user index"), Recorder->LoadUserIndex, UserIndex);
		TestRunner->TestFalse(TEXT("Async load helper should return a non-null save object for an existing slot"), Recorder->bLoadReceivedNullObject);
		TestRunner->TestTrue(TEXT("Async load helper should dispatch the callback on the game thread"), Recorder->bLoadCallbackOnGameThread);
		TestRunner->TestEqual(TEXT("Async load helper should deserialize the saved marker"), Recorder->LoadedMarker, ExpectedMarker);

		Recorder->ResetLoadState();
		LoadParams.SlotName = MissingSlotName;
		if (!InvokeGeneratedVoidMethod(*TestRunner, Engine, ScriptHarness, ScriptHarnessClass, TEXT("StartAsyncLoad"), &LoadParams))
		{
			return;
		}

		if (!WaitUntil(
			*TestRunner,
			[Recorder]() { return Recorder->LoadCallbackCount >= 1; },
			AsyncSaveLoadTimeoutSeconds,
			TEXT("Missing-slot async load callback")))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Missing-slot async load should invoke the callback exactly once"), Recorder->LoadCallbackCount, 1);
		TestRunner->TestEqual(TEXT("Missing-slot async load should still forward the requested slot name"), Recorder->LoadSlotName, MissingSlotName);
		TestRunner->TestEqual(TEXT("Missing-slot async load should still forward the requested user index"), Recorder->LoadUserIndex, UserIndex);
		TestRunner->TestTrue(TEXT("Missing-slot async load should report a null save object"), Recorder->bLoadReceivedNullObject);
		TestRunner->TestEqual(TEXT("Missing-slot async load should keep the marker sentinel"), Recorder->LoadedMarker, INDEX_NONE);
		TestRunner->TestTrue(TEXT("Missing-slot async load should still run on the game thread"), Recorder->bLoadCallbackOnGameThread);
	}

	// ====================================================================
	// Section: ImmediateFailureCallbacks
	// ====================================================================

	TEST_METHOD(ImmediateFailureCallbacks)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*GameplayFunctionLibraryImmediateFailureModuleName.ToString());
		};

		const FString MissingSlotName = FString::Printf(TEXT("AsyncImmediateFailureMissing_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		constexpr int32 UserIndex = 19;
		constexpr int32 SaveMarker = 4242;

		UGameplayStatics::DeleteGameInSlot(MissingSlotName, UserIndex);
		ON_SCOPE_EXIT
		{
			UGameplayStatics::DeleteGameInSlot(MissingSlotName, UserIndex);
		};

		UClass* ScriptHarnessClass = CompileScriptModule(
			*TestRunner,
			Engine,
			GameplayFunctionLibraryImmediateFailureModuleName,
			GameplayFunctionLibraryImmediateFailureFilename,
			TEXT(R"AS(
UCLASS()
class UAsyncSaveLoadImmediateFailureScriptHarness : UObject
{
	UFUNCTION()
	void StartAsyncSave(USaveGame SaveGameObject, UObject Receiver, const FString& SlotName, int32 UserIndex)
	{
		FAsyncSaveGameToSlotDynamicDelegate SaveDelegate;
		SaveDelegate.BindUFunction(Receiver, n"OnSaveComplete");
		UGameplayLibrary::AsyncSaveGameToSlot(SaveGameObject, SlotName, UserIndex, SaveDelegate);
	}

	UFUNCTION()
	void StartAsyncLoad(UObject Receiver, const FString& SlotName, int32 UserIndex)
	{
		FAsyncLoadGameFromSlotDynamicDelegate LoadDelegate;
		LoadDelegate.BindUFunction(Receiver, n"OnLoadComplete");
		UGameplayLibrary::AsyncLoadGameFromSlot(SlotName, UserIndex, LoadDelegate);
	}
}
)AS"),
			GameplayFunctionLibraryImmediateFailureClassName);
		if (ScriptHarnessClass == nullptr)
		{
			return;
		}

		UObject* ScriptHarness = NewObject<UObject>(GetTransientPackage(), ScriptHarnessClass, TEXT("AsyncSaveLoadImmediateFailureScriptHarness"));
		UAngelscriptAsyncSaveLoadCallbackRecorder* Recorder = NewObject<UAngelscriptAsyncSaveLoadCallbackRecorder>(GetTransientPackage(), TEXT("AsyncSaveLoadImmediateFailureRecorder"));
		UAngelscriptAsyncSaveGameTestObject* SaveGameObject = NewObject<UAngelscriptAsyncSaveGameTestObject>(GetTransientPackage(), TEXT("AsyncImmediateFailureSaveGame"));
		if (!TestRunner->TestNotNull(TEXT("Gameplay async immediate-failure test should create the script harness"), ScriptHarness)
			|| !TestRunner->TestNotNull(TEXT("Gameplay async immediate-failure test should create the callback recorder"), Recorder)
			|| !TestRunner->TestNotNull(TEXT("Gameplay async immediate-failure test should create the save object"), SaveGameObject))
		{
			return;
		}

		ScriptHarness->AddToRoot();
		Recorder->AddToRoot();
		SaveGameObject->AddToRoot();
		ON_SCOPE_EXIT
		{
			SaveGameObject->RemoveFromRoot();
			Recorder->RemoveFromRoot();
			ScriptHarness->RemoveFromRoot();
		};

		SaveGameObject->Marker = SaveMarker;

		auto RunInvalidSaveCase = [this, &Engine, ScriptHarness, ScriptHarnessClass, Recorder, UserIndex](
			const TCHAR* CaseLabel,
			USaveGame* SaveObject,
			const FString& SlotName) -> bool
		{
			Recorder->ResetSaveState();

			FStartAsyncSaveParams SaveParams;
			SaveParams.SaveGameObject = SaveObject;
			SaveParams.Receiver = Recorder;
			SaveParams.SlotName = SlotName;
			SaveParams.UserIndex = UserIndex;
			if (!InvokeGeneratedVoidMethod(*TestRunner, Engine, ScriptHarness, ScriptHarnessClass, TEXT("StartAsyncSave"), &SaveParams))
			{
				return false;
			}

			if (!WaitUntil(
					*TestRunner,
					[Recorder]() { return Recorder->SaveCallbackCount >= 1; },
					AsyncSaveLoadTimeoutSeconds,
					CaseLabel))
			{
				return false;
			}

			return TestRunner->TestEqual(FString::Printf(TEXT("%s should invoke the save callback exactly once"), CaseLabel), Recorder->SaveCallbackCount, 1)
				&& TestRunner->TestEqual(FString::Printf(TEXT("%s should preserve the requested slot name"), CaseLabel), Recorder->SaveSlotName, SlotName)
				&& TestRunner->TestEqual(FString::Printf(TEXT("%s should preserve the requested user index"), CaseLabel), Recorder->SaveUserIndex, UserIndex)
				&& TestRunner->TestFalse(FString::Printf(TEXT("%s should report save failure"), CaseLabel), Recorder->bLastSaveSuccess)
				&& TestRunner->TestTrue(FString::Printf(TEXT("%s should dispatch the save callback on the game thread"), CaseLabel), Recorder->bSaveCallbackOnGameThread);
		};

		auto RunInvalidLoadCase = [this, &Engine, ScriptHarness, ScriptHarnessClass, Recorder, UserIndex](
			const TCHAR* CaseLabel,
			const FString& SlotName) -> bool
		{
			Recorder->ResetLoadState();

			FStartAsyncLoadParams LoadParams;
			LoadParams.Receiver = Recorder;
			LoadParams.SlotName = SlotName;
			LoadParams.UserIndex = UserIndex;
			if (!InvokeGeneratedVoidMethod(*TestRunner, Engine, ScriptHarness, ScriptHarnessClass, TEXT("StartAsyncLoad"), &LoadParams))
			{
				return false;
			}

			if (!WaitUntil(
					*TestRunner,
					[Recorder]() { return Recorder->LoadCallbackCount >= 1; },
					AsyncSaveLoadTimeoutSeconds,
					CaseLabel))
			{
				return false;
			}

			return TestRunner->TestEqual(FString::Printf(TEXT("%s should invoke the load callback exactly once"), CaseLabel), Recorder->LoadCallbackCount, 1)
				&& TestRunner->TestEqual(FString::Printf(TEXT("%s should preserve the requested slot name"), CaseLabel), Recorder->LoadSlotName, SlotName)
				&& TestRunner->TestEqual(FString::Printf(TEXT("%s should preserve the requested user index"), CaseLabel), Recorder->LoadUserIndex, UserIndex)
				&& TestRunner->TestTrue(FString::Printf(TEXT("%s should report a null save object"), CaseLabel), Recorder->bLoadReceivedNullObject)
				&& TestRunner->TestTrue(FString::Printf(TEXT("%s should dispatch the load callback on the game thread"), CaseLabel), Recorder->bLoadCallbackOnGameThread)
				&& TestRunner->TestEqual(FString::Printf(TEXT("%s should keep the marker sentinel when load fails"), CaseLabel), Recorder->LoadedMarker, INDEX_NONE)
				&& TestRunner->TestTrue(FString::Printf(TEXT("%s should keep the loaded save object null"), CaseLabel), Recorder->LastLoadedSaveGame == nullptr);
		};

		if (!RunInvalidSaveCase(
				TEXT("Gameplay async immediate-failure null-save empty-slot path"),
				nullptr,
				FString()))
		{
			return;
		}

		if (!RunInvalidSaveCase(
				TEXT("Gameplay async immediate-failure valid-save empty-slot path"),
				SaveGameObject,
				FString()))
		{
			return;
		}

		if (!RunInvalidLoadCase(
				TEXT("Gameplay async immediate-failure empty-slot load path"),
				FString()))
		{
			return;
		}

		if (!RunInvalidLoadCase(
				TEXT("Gameplay async immediate-failure missing-slot load path"),
				MissingSlotName))
		{
			return;
		}
	}
};

#endif
