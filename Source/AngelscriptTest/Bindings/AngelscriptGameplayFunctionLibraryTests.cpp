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
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptFunctionalTestUtils.h"

#include "Bindings/AngelscriptGameplayFunctionLibraryTestTypes.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

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
		FNoDiscardAsserter Assert(Test);
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		if (!Assert.IsNotNull(
			Function,
			*FString::Printf(TEXT("Gameplay function library script method '%s' should exist"), *FunctionName.ToString())))
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
		bool bObjectsCreated = true;
		bObjectsCreated &= this->Assert.IsNotNull(ScriptHarness, TEXT("Async save/load delegate test case should create the script harness"));
		bObjectsCreated &= this->Assert.IsNotNull(Recorder, TEXT("Async save/load delegate test case should create the callback recorder"));
		bObjectsCreated &= this->Assert.IsNotNull(SaveGameObject, TEXT("Async save/load delegate test case should create the save object"));
		if (!bObjectsCreated)
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

		bool bAsyncSaveLoadPassed = true;
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(1, Recorder->SaveCallbackCount, TEXT("Async save helper should invoke the callback exactly once"));
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(SlotName, Recorder->SaveSlotName, TEXT("Async save helper should forward the original slot name"));
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(UserIndex, Recorder->SaveUserIndex, TEXT("Async save helper should forward the original user index"));
		bAsyncSaveLoadPassed &= this->Assert.IsTrue(Recorder->bLastSaveSuccess, TEXT("Async save helper should report save success"));
		bAsyncSaveLoadPassed &= this->Assert.IsTrue(Recorder->bSaveCallbackOnGameThread, TEXT("Async save helper should dispatch the callback on the game thread"));
		bAsyncSaveLoadPassed &= this->Assert.IsTrue(UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex), TEXT("Async save helper should create the slot on disk"));

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

		bAsyncSaveLoadPassed &= this->Assert.AreEqual(1, Recorder->LoadCallbackCount, TEXT("Async load helper should invoke the callback exactly once"));
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(SlotName, Recorder->LoadSlotName, TEXT("Async load helper should forward the original slot name"));
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(UserIndex, Recorder->LoadUserIndex, TEXT("Async load helper should forward the original user index"));
		bAsyncSaveLoadPassed &= this->Assert.IsFalse(Recorder->bLoadReceivedNullObject, TEXT("Async load helper should return a non-null save object for an existing slot"));
		bAsyncSaveLoadPassed &= this->Assert.IsTrue(Recorder->bLoadCallbackOnGameThread, TEXT("Async load helper should dispatch the callback on the game thread"));
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(ExpectedMarker, Recorder->LoadedMarker, TEXT("Async load helper should deserialize the saved marker"));

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

		bAsyncSaveLoadPassed &= this->Assert.AreEqual(1, Recorder->LoadCallbackCount, TEXT("Missing-slot async load should invoke the callback exactly once"));
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(MissingSlotName, Recorder->LoadSlotName, TEXT("Missing-slot async load should still forward the requested slot name"));
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(UserIndex, Recorder->LoadUserIndex, TEXT("Missing-slot async load should still forward the requested user index"));
		bAsyncSaveLoadPassed &= this->Assert.IsTrue(Recorder->bLoadReceivedNullObject, TEXT("Missing-slot async load should report a null save object"));
		bAsyncSaveLoadPassed &= this->Assert.AreEqual(INDEX_NONE, Recorder->LoadedMarker, TEXT("Missing-slot async load should keep the marker sentinel"));
		bAsyncSaveLoadPassed &= this->Assert.IsTrue(Recorder->bLoadCallbackOnGameThread, TEXT("Missing-slot async load should still run on the game thread"));
		if (!bAsyncSaveLoadPassed)
		{
			return;
		}
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
		bool bObjectsCreated = true;
		bObjectsCreated &= this->Assert.IsNotNull(ScriptHarness, TEXT("Gameplay async immediate-failure test should create the script harness"));
		bObjectsCreated &= this->Assert.IsNotNull(Recorder, TEXT("Gameplay async immediate-failure test should create the callback recorder"));
		bObjectsCreated &= this->Assert.IsNotNull(SaveGameObject, TEXT("Gameplay async immediate-failure test should create the save object"));
		if (!bObjectsCreated)
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

			bool bPassed = true;
			bPassed &= this->Assert.AreEqual(1, Recorder->SaveCallbackCount, FString::Printf(TEXT("%s should invoke the save callback exactly once"), CaseLabel));
			bPassed &= this->Assert.AreEqual(SlotName, Recorder->SaveSlotName, FString::Printf(TEXT("%s should preserve the requested slot name"), CaseLabel));
			bPassed &= this->Assert.AreEqual(UserIndex, Recorder->SaveUserIndex, FString::Printf(TEXT("%s should preserve the requested user index"), CaseLabel));
			bPassed &= this->Assert.IsFalse(Recorder->bLastSaveSuccess, FString::Printf(TEXT("%s should report save failure"), CaseLabel));
			bPassed &= this->Assert.IsTrue(Recorder->bSaveCallbackOnGameThread, FString::Printf(TEXT("%s should dispatch the save callback on the game thread"), CaseLabel));
			return bPassed;
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

			bool bPassed = true;
			bPassed &= this->Assert.AreEqual(1, Recorder->LoadCallbackCount, FString::Printf(TEXT("%s should invoke the load callback exactly once"), CaseLabel));
			bPassed &= this->Assert.AreEqual(SlotName, Recorder->LoadSlotName, FString::Printf(TEXT("%s should preserve the requested slot name"), CaseLabel));
			bPassed &= this->Assert.AreEqual(UserIndex, Recorder->LoadUserIndex, FString::Printf(TEXT("%s should preserve the requested user index"), CaseLabel));
			bPassed &= this->Assert.IsTrue(Recorder->bLoadReceivedNullObject, FString::Printf(TEXT("%s should report a null save object"), CaseLabel));
			bPassed &= this->Assert.IsTrue(Recorder->bLoadCallbackOnGameThread, FString::Printf(TEXT("%s should dispatch the load callback on the game thread"), CaseLabel));
			bPassed &= this->Assert.AreEqual(INDEX_NONE, Recorder->LoadedMarker, FString::Printf(TEXT("%s should keep the marker sentinel when load fails"), CaseLabel));
			bPassed &= this->Assert.IsTrue(Recorder->LastLoadedSaveGame == nullptr, FString::Printf(TEXT("%s should keep the loaded save object null"), CaseLabel));
			return bPassed;
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
