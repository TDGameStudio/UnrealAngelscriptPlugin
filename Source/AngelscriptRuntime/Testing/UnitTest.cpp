#include "UnitTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptSettings.h"
#include "Binds/Bind_Debugging.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Testing/AngelscriptTest.h"
#include "Testing/AngelscriptTestSettings.h"
#include "Testing/Network/FakeNetDriver.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameMapsSettings.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "HAL/Platform.h"
#include "Misc/AutomationTest.h"
#include "Misc/FeedbackContext.h"
#include "NavigationSystem.h"
#include "Slate/SceneViewport.h"
#include "Templates/Models.h"
#include "Framework/Application/SlateApplication.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_module.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

#if WITH_EDITOR
#include "Misc/ScopedSlowTask.h"
#endif

class FAngelscriptUnitTest : public FAngelscriptTest, public TSharedFromThis<FAngelscriptUnitTest>
{
public:
	FAngelscriptUnitTest(FAngelscriptModuleDesc* Module, FAngelscriptTestDesc& TestFunctionDesc,
		FAngelscriptEngine* AngelscriptManager)
		: FAngelscriptTest(Module, TestFunctionDesc.Function, AngelscriptManager), TestFunctionDesc(TestFunctionDesc)
	{
	}

	FAngelscriptTestDesc TestFunctionDesc;
};

static bool EnsureAngelscriptUnitRuntimeInitialized()
{
	if (!FAngelscriptEngine::IsInitialized())
	{
		FAngelscriptRuntimeModule::InitializeAngelscript();
	}

	return FAngelscriptEngine::IsInitialized();
}

void BindAdvanceTime(FAngelscriptBinds& T)
{
	T.Method("void AdvanceTime(float32 DeltaSecs)",
		[](FAngelscriptUnitTest& Self, float DeltaSecs)
		{
			if (!WithEditor())
			{
				return;
			}
			UWorld* World = GEngine->GetWorldFromContextObject(FAngelscriptEngine::TryGetCurrentWorldContextObject(), EGetWorldErrorMode::ReturnNull);
			if (!World)
			{
				Self.Fail("Tried to advance time, but has no world");
				return;
			}

			GFrameCounter++;

			World->TimeSeconds += DeltaSecs;
			World->UnpausedTimeSeconds += DeltaSecs;
			World->RealTimeSeconds += DeltaSecs;
			World->AudioTimeSeconds += DeltaSecs;
			World->DeltaTimeSeconds = DeltaSecs;

			// Tick resets the net driver, so preserve our fake driver in that case.
			UNetDriver* PrevNetDriver = World->GetNetDriver();

			// Force pending timers to be added to the active timer queue
			World->GetTimerManager().Tick(0.f);

			// Advance time and trigger timers in the active timer queue
			GFrameCounter++;
			World->GetTimerManager().Tick(DeltaSecs);

			World->SetNetDriver(PrevNetDriver);
		});
}

void BindAddExpectedError(FAngelscriptBinds& T)
{
	T.Method("void AddExpectedError(FString Regex, int Occurrences = 1)",
		[](FAngelscriptUnitTest& Self, const FString& Regex, int Occurrences) {
			// Only Contains is supported for now (need bindings for EAutomationExpectedErrorFlags?)
			Self.AddExpectedError(Regex, EAutomationExpectedErrorFlags::MatchType::Contains, Occurrences);
		});
}

void BindCallNativeBeginPlayFor(FAngelscriptBinds& T)
{
	T.Method("void CallNativeBeginPlayFor(AActor Actor, bool bFromLevelStreaming = false)",
		[](FAngelscriptUnitTest& Self, AActor* Actor, bool bFromLevelStreaming) {
		if (!Actor)
		{
			Self.Fail("Tried to CallNativeBeginPlayFor a non-existing Actor");
			return;
		}
		Actor->DispatchBeginPlay(bFromLevelStreaming);
	});
	T.Method("void CallNativeBeginPlayFor(UActorComponent ActorComponent)",
		[](FAngelscriptUnitTest& Self, UActorComponent* ActorComponent) {
		if (!ActorComponent)
		{
			Self.Fail("Tried to CallNativeBeginPlayFor a non-existing ActorComponent");
			return;
		}
		ActorComponent->BeginPlay();
	});
}

void BindSetIsServer(FAngelscriptBinds& T)
{
	T.Method("void SetIsServer(bool bIsServer)",
		[](FAngelscriptUnitTest& Self, bool bIsServer) {
			if (!WithEditor())
			{
				return;
			}
			UWorld* World = GEngine->GetWorldFromContextObject(FAngelscriptEngine::TryGetCurrentWorldContextObject(), EGetWorldErrorMode::ReturnNull);
			if (!World)
			{
				Self.Fail("Tried to SetIsServer, but has no world");
				return;
			}

			UFakeNetDriver* FakeNetDriver = NewObject<UFakeNetDriver>(World);
			FakeNetDriver->bIsServer = bIsServer;
			World->SetNetDriver(FakeNetDriver);
		});
}

void BindGetParamUnitTest(FAngelscriptBinds& T)
{
	T.Method("FString GetParam()",
		[](FAngelscriptUnitTest& Self) {
			if (!Self.TestFunctionDesc.bIsComplexTest)
			{
				Self.Fail("GetParam() is only allowed for complex unit tests");
			}
			return Self.TestFunctionDesc.ComplexTestParam;
		});
}

void BindForceGarbageCollectionNow(FAngelscriptBinds& T)
{
	T.Method("void ForceGarbageCollectionNow()",
		[](FAngelscriptUnitTest& Self) {
			// Force garbage collection at the end of the frame (or whenever ConditionalCollectGarbage() is called next).
			GEngine->ForceGarbageCollection(true);
			// We set a random frame counter during garbage collection to ensure that garbage collection can run
			// multiple times in a single frame if desired.
			uint64 LastGFrameCounter = GFrameCounter;
			do  {
				GFrameCounter = FMath::Rand();
			} while (GFrameCounter == LastGFrameCounter);
			GEngine->ConditionalCollectGarbage();
			GFrameCounter = LastGFrameCounter;
		});
}

// This class picks up Angelscript errors that aren't coming through ReportError in FAngelscriptTest.
// Otherwise a test would fail in the Unreal test framework but not on hot reload if a test used, say, Error().
// We only need this for hot reload: if we run tests through Unreal it has its own similar output device.
class FUnitTestHotReloadFeedbackContext : public FFeedbackContext
{
public:
	// FOutputDevice interface.
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (Verbosity == ELogVerbosity::Error)
		{
			ErrorMessages.Add(FString(V));
		}
	}

	// Returns any Error-level logs received while this object was registered as a log device.
	const TArray<FString>& GetErrorMessages() const
	{
		return ErrorMessages;
	}

private:
	TArray<FString> ErrorMessages;
};

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AngelscriptUnitTesting(FAngelscriptBinds::EOrder::Late, [] {
	FBindFlags BindFlags;
	BindFlags.bPOD = false;

	FAngelscriptBinds T = FAngelscriptBinds::ValueClass<FAngelscriptTest>("FUnitTest", BindFlags);

	FAngelscriptTest::BindAsserts(T);

	BindAdvanceTime(T);
	BindAddExpectedError(T);
	BindCallNativeBeginPlayFor(T);
	BindSetIsServer(T);
	BindGetParamUnitTest(T);
	BindForceGarbageCollectionNow(T);
});

// Call once per running test.
void MaybeGarbageCollect()
{
	static int TestsRun = 0;

	int N = GetDefault<UAngelscriptTestSettings>()->GarbageCollectEveryNTests;
	if (N == 0)
	{
		return;
	}
	if (++TestsRun % N == 0)
	{
		UE_LOG(Angelscript, Log, TEXT("Unit tests: Forcing GC to avoid running out of engine resources..."));
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	}
}

void ConfigureUnitTestWorld(UWorld* World)
{
	// No map is ever loaded in unit tests, so set game state and game mode. These can be configured
	// in .ini files in some cases, and if they're not configured we set empty modes.
	UNavigationSystemV1* NavigationSystem = NewObject<UNavigationSystemV1>(World);
	World->SetNavigationSystem(NavigationSystem);

	World->CreateAISystem();

	// Try to retrieve the UnitTest game mode alias. If it isn't set, this .uproject hasn't configured
	// it and we'll run without a game mode.
	FString GameModeName = GetDefault<UGameMapsSettings>()->GetGameModeForName("UnitTest");
	if (GameModeName == "UnitTest")
	{
		return;
	}

	FURL URL(nullptr, TEXT("/Game/MapThatDoesntExist?game=UnitTest"), ETravelType::TRAVEL_Absolute);
	World->SetGameMode(URL);
}

struct FAngelscriptState
{
	float EditorMaximumScriptExecutionTime;
};

void ConfigureAngelscriptForTest(FAngelscriptState& OutOriginalState)
{
	UAngelscriptSettings* AngelscriptSettings = GetMutableDefault<UAngelscriptSettings>();
	OutOriginalState.EditorMaximumScriptExecutionTime = AngelscriptSettings->EditorMaximumScriptExecutionTime;
	AngelscriptSettings->EditorMaximumScriptExecutionTime = 300.0f;

	if (!GetDefault<UAngelscriptTestSettings>()->bEnableDebugBreaksInTests)
	{
		// Debug breaks on ensures are disabled since tests intentionally provoke ensures all the time.
		AngelscriptDisableDebugBreaks();
	}
}

void RestoreAngelscriptAfterTest(const FAngelscriptState& OriginalState)
{
	AngelscriptEnableDebugBreaks();

	UAngelscriptSettings* AngelscriptSettings = GetMutableDefault<UAngelscriptSettings>();
	AngelscriptSettings->EditorMaximumScriptExecutionTime = OriginalState.EditorMaximumScriptExecutionTime;
}

void RunOneUnitTest(FAngelscriptTest& T)
{
	MaybeGarbageCollect();
	FName UniquePackageName = MakeUniqueObjectName(nullptr, UPackage::StaticClass(), FName(TEXT("/Angelscript_Test")));
	UPackage* Package = NewObject<UPackage>(GetTransientPackage(), UniquePackageName, RF_Transient);
	UClass* GameInstanceClass = GetDefault<UAngelscriptTestSettings>()->UnitTestGameInstanceClass.LoadSynchronous();
	if (GameInstanceClass == nullptr)
	{
		GameInstanceClass = UGameInstance::StaticClass();
	}

	// InitializeStandalone creates a "dummy" world which is intended as a placeholder until the game loads a map.
	// It never will in a unit test however. We need a world anyway so this is fine.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine, GameInstanceClass);

	// Note: subsystems are allowed to check the world name to determine if they're in a unit test, so be careful
	// about changing this name!
	GameInstance->InitializeStandalone(TEXT("UnitTestWorld"), Package);
	UWorld* World = GameInstance->GetWorld();
	FWorldContext* WorldContext = GameInstance->GetWorldContext();
	ConfigureUnitTestWorld(World);

	if (FSlateApplication::IsInitialized())
	{
		UGameViewportClient* TestViewport = NewObject<UGameViewportClient>(GEngine);
		TestViewport->Init(*WorldContext, GameInstance, /*bCreateNewAudioDevice*/false);

		TSharedRef<SOverlay> TestOverlay = SNew(SOverlay);
		TestViewport->SetViewportOverlayWidget(nullptr, TestOverlay);
		TestViewport->Viewport = new FSceneViewport(TestViewport, nullptr);
		WorldContext->GameViewport = TestViewport;
	}

	FAngelscriptState State;
	ConfigureAngelscriptForTest(State);

	FURL URL;
	World->InitializeActorsForPlay(URL);

	// Cache global frame counter so we can "tick" it internally in tests.
	const int GFrameCounterBefore = GFrameCounter;

	FAngelscriptContext Context(World, T.GetTestFunction()->GetEngine());
	T.StartTest(Context);

	RestoreAngelscriptAfterTest(State);

	World->BeginTearingDown();
	World->GetGameInstance()->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	GFrameCounter = GFrameCounterBefore;
}

bool RunOneUnitTestHotReload(FAngelscriptTest& T)
{
#if WITH_EDITOR
	if (GEditor && (GEditor->PlayWorld || GIsPlayInEditorWorld))
	{
		UE_LOG(Angelscript, Error, TEXT("I can't run unit tests while you have a PIE session running."));
		return false;
	}
#endif

	// On hot reload we need to install our own log device so we pick up Error-level logs outside the
	// test framework.
	FUnitTestHotReloadFeedbackContext LogDevice;
	GLog->AddOutputDevice(&LogDevice);

	double TestStartTime = FPlatformTime::Seconds();
	UE_LOG(Angelscript, Log, TEXT("[RUN]    %s"), *T.QualifiedTestName());

	RunOneUnitTest(T);

	GLog->RemoveOutputDevice(&LogDevice);
	double ElapsedSecs = FPlatformTime::Seconds() - TestStartTime;

	// We also need to replicate the AddExpectedError mechanism from Unreal's test framework.
	int NumErrors = 0;
	for (const FString& ErrorMsg: LogDevice.GetErrorMessages())
	{
		if (T.IsExpectedError(ErrorMsg))
		{
			if (ErrorMsg.StartsWith("Assertion failed"))
			{
				T.ReportErrorOnCurrentLine(FString::Printf(
					TEXT("Incorrect test: %s squelched the assertion error %s with an AddExpectedError. This is not allowed."),
					*T.QualifiedTestName(), *ErrorMsg));
				NumErrors++;
			}
		}
		else
		{
			NumErrors++;
		}
	}

	if (NumErrors > 0 || !T.HasMetExpectedErrors())
	{
		UE_LOG(Angelscript, Log, TEXT("[FAILED] %s (%.4f ms)"), *T.QualifiedTestName(), ElapsedSecs * 1000.0);
		return false;
	}
	else
	{
		UE_LOG(Angelscript, Log, TEXT("[OK]     %s (%.4f ms)"), *T.QualifiedTestName(), ElapsedSecs * 1000.0);
		return true;
	}
}

// This is the entry point for running the scripts on hot reload.
bool RunAngelscriptUnitTests(
	const TArray<TSharedRef<FAngelscriptModuleDesc>>& ActiveModules,
	FAngelscriptEngine* AngelscriptManager,
	int CurrentBatchOnHotReload,
	int TotalBatchesOnHotReload)
{
	bool bAllTestsPass = true;
#if WITH_EDITOR
	FScopedSlowTask SlowTask(1.f, FText::FromString(FString::Printf(TEXT("Running Unit Tests on Hot Reload %d/%d"), CurrentBatchOnHotReload, TotalBatchesOnHotReload)));
	SlowTask.MakeDialog();
	SlowTask.EnterProgressFrame(0.0f);

	int NumTests = 0;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		NumTests += Module->UnitTestFunctions.Num();
	}

	if (NumTests == 0)
	{
		SlowTask.EnterProgressFrame(1.0f);
		return true;
	}

	float ProgressStep = 1.0f / float(NumTests);
	for (const TSharedRef<FAngelscriptModuleDesc> &Module : ActiveModules)
	{
		for (auto Element : Module->UnitTestFunctions)
		{
			FAngelscriptTestDesc TestDesc = Element.Value;
			FAngelscriptUnitTest T(&(*Module), TestDesc, AngelscriptManager);
			bAllTestsPass &= RunOneUnitTestHotReload(T);
			SlowTask.EnterProgressFrame(ProgressStep);
		}
	}

#endif  // WITH_EDITOR
	return bAllTestsPass;
}

FHotReloadTestRunner::FHotReloadTestRunner()
	: TotalBatchesOnHotReload(0)
	, CurrentBatchOnHotReload(0)
{
}

void FHotReloadTestRunner::SortModulesForTestExecution(
	const TArray<TSharedRef<FAngelscriptModuleDesc>>& ActiveModules,
	const TArray<FString>& FileList,
	TArray<FString>& ModulesOrdered)
{
	// We are ordering the modules in breadth first search order.
	// This order will let tests that are closer, at dependency level, to be
	// executed first.

	// Build ModuleName to Module map
	TMap<FString, TSharedRef<struct FAngelscriptModuleDesc>> ModuleNameToModule;
	ModuleNameToModule.Reserve(ActiveModules.Num());
	for (auto& Module : ActiveModules)
	{
		ModuleNameToModule.Add((*Module).ModuleName, Module);
	}

	// Book-keeping over the modules which have been visited when
	// traversing the module dependencies.
	TSet<FAngelscriptModuleDesc*> VisitedModules;
	VisitedModules.Reserve(ModuleNameToModule.Num());

	// A job stack of modules which should be visited
	TArray<FAngelscriptModuleDesc*> ModuleJobs;
	ModuleJobs.Reserve(ModuleNameToModule.Num());

	// Build a lookup table from relative file path -> module
	TMap<FString, FAngelscriptModuleDesc*> RelativeFileToModule;
	RelativeFileToModule.Reserve(ModuleNameToModule.Num() * 2);

	TMap<asCModule*, FAngelscriptModuleDesc*> ScriptModuleToModule;
	ScriptModuleToModule.Reserve(ModuleNameToModule.Num() * 2);

	for (auto& Module : ModuleNameToModule)
	{
		auto ModulePtr = &(Module.Value.Get());
		for (const auto& Section : ModulePtr->Code)
			RelativeFileToModule.Add(Section.RelativeFilename, ModulePtr);

		if (ModulePtr->ScriptModule != nullptr)
			ScriptModuleToModule.Add((asCModule*)ModulePtr->ScriptModule, ModulePtr);
	}

	for (auto& File : FileList)
	{
		if (auto ModulePtr = RelativeFileToModule.Find(File))
		{
			ModuleJobs.AddUnique(*ModulePtr);
			VisitedModules.Add(*ModulePtr);
		}
	}

	// Build a reverse dependency map for module->dependent modules (non-recursive)
	TMap<FAngelscriptModuleDesc*, TArray<FAngelscriptModuleDesc*>> ReverseDeps;
	if (ModuleJobs.Num() > 0)
	{
		ReverseDeps.Reserve(ModuleNameToModule.Num());
		for (auto& Module : ModuleNameToModule)
		{
			auto ModulePtr = &(Module.Value.Get());
			if (ModulePtr->Code.Num() == 0)
				return;
			auto* ScriptModule = (asCModule*)ModulePtr->ScriptModule;
			if (ScriptModule == nullptr)
				return;
			for (const auto& Pair : ScriptModule->moduleDependencies)
			{
				asCModule* Dependency = Pair.Key;
				auto ImportedModulePtr = &(ModuleNameToModule.Find(Dependency->GetName())->Get());
				ReverseDeps.FindOrAdd(ImportedModulePtr).Add(ModulePtr);
			}
		}
	}

	while (ModuleJobs.Num() > 0)
	{
		auto ModulePtr = ModuleJobs[0];
		ModuleJobs.RemoveAt(0, 1, EAllowShrinking::No);
		ModulesOrdered.Add(ModulePtr->ModuleName);

		if (auto DependentModulesPtr = ReverseDeps.Find(ModulePtr))
		{
			for (auto ModuleDepPtr : *DependentModulesPtr)
			{
				if (!VisitedModules.Contains(ModuleDepPtr))
				{
					VisitedModules.Add(ModuleDepPtr);
					ModuleJobs.Push(ModuleDepPtr);
				}
			}
		}
	}
}

void FHotReloadTestRunner::PrepareTests(
	const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& ActiveModules,
	const TArray<TSharedRef<FAngelscriptModuleDesc>>& ModulesToCompile,
	const TArray<FString>& FileList,
	bool AutomaticImport)
{
	if (!ShouldRunUnitTestsOnHotReload())
	{
		return;
	}

	// As we are executing tests after hot reload compilation finishes and we are allowing ticks between
	// test execution, hot reload could be triggered while we are executing tests
	if (TestAfterHotReload.Num() > 0)
	{
		TestAfterHotReload.Empty();
		CurrentBatchOnHotReload = 0;
		TotalBatchesOnHotReload = 0;
	}

	// Store modules with unit test to execute them after hot reload is done
	int NModulesLimit = GetDefault<UAngelscriptTestUserSettings>()->LimitNModulesToTestOnHotReload;
	if (!AutomaticImport && NModulesLimit > 0)
	{
		// Since we won't run all tests, prefer tests that are close in the dependency tree (walk dependency tree breadth first).
		TArray<FString> ModulesOrdered;
		ModulesOrdered.Reserve(ActiveModules.Num());
		SortModulesForTestExecution(ActiveModules, FileList, ModulesOrdered);

		TMap<FString, TSharedRef<FAngelscriptModuleDesc>> ModuleNameToModule;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ModulesToCompile)
		{
			ModuleNameToModule.Add(Module->ModuleName, Module);
		}

		for (const FString& ModuleName : ModulesOrdered)
		{
			TSharedRef<FAngelscriptModuleDesc>* Module = ModuleNameToModule.Find(ModuleName);
			if ((*Module)->UnitTestFunctions.Num() > 0)
			{
				TestAfterHotReload.Add(*Module);
			}
			if (TestAfterHotReload.Num() >= NModulesLimit)
			{
				break;
			}
		}
	}
	else
	{
		// Run all the tests.
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ModulesToCompile)
		{
			if (Module->UnitTestFunctions.Num() > 0)
			{
				TestAfterHotReload.Add(Module);
			}
		}
	}
}

bool FHotReloadTestRunner::RunTests(FAngelscriptEngine* AngelscriptManager)
{
	if (!ShouldRunUnitTestsOnHotReload())
		return true;

	bool AllUnitTestsPass = true;

	// We need to execute the tests in batches when hot reloading. If we don't do this, after an amount
	// of test executions (between 450-500 tests) PHYSX causes the editor to stop responding:
	//   PxScene::unlockRead() called without matching call to PxScene::lockRead(), behaviour will be undefined.
	// So we need to tick between tests executions.
	if (TestAfterHotReload.Num() > 0) {
		int TestsPerBatch = GetDefault<UAngelscriptTestSettings>()->GarbageCollectEveryNTests;
		if (TestsPerBatch == 0)
		{
			TestsPerBatch = INT_MAX;
		}

		// Plan batches execution
		if (TotalBatchesOnHotReload == 0)
		{
			TotalBatchesOnHotReload = 1;
			int TestsForBatch = 0;
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : TestAfterHotReload)
			{
				int TestsInModule = Module->UnitTestFunctions.Num();
				TestsForBatch += TestsInModule;
				if (TestsForBatch >= TestsPerBatch)
				{
					TestsForBatch = 0;
					++TotalBatchesOnHotReload;
				}
			}

			CurrentBatchOnHotReload = 1;
		}

		// Execute test batch
		int TestsInBatch = 0;
		TArray<TSharedRef<FAngelscriptModuleDesc>> TestBatch;
		while (TestsInBatch <= TestsPerBatch && TestAfterHotReload.Num() > 0)
		{
			TSharedRef<FAngelscriptModuleDesc> Module = TestAfterHotReload.Pop(EAllowShrinking::No);
			TestBatch.Add(Module);
			TestsInBatch += Module->UnitTestFunctions.Num();
		}
		AllUnitTestsPass = RunAngelscriptUnitTests(TestBatch, AngelscriptManager, CurrentBatchOnHotReload, TotalBatchesOnHotReload);
		++CurrentBatchOnHotReload;

		// Reset batches information
		if (TestAfterHotReload.Num() == 0)
		{
			TotalBatchesOnHotReload = 0;
			CurrentBatchOnHotReload = 0;
		}
	}

	return AllUnitTestsPass;
}

bool FHotReloadTestRunner::ShouldRunUnitTestsOnHotReload() const
{
	return GetDefault<UAngelscriptTestUserSettings>()->bRunUnitTestsOnHotReload;
}


#if WITH_DEV_AUTOMATION_TESTS

// This test exposes the tests to the Test Automation tool in the editor, so that
// devs can run the tests there as well.
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FAngelscriptUnitTests, "Angelscript.UnitTests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

// Finds one test. Fails if the module or test don't exist (in that case there
// is a bug in DiscoverTests.cpp).
bool LookupUnitTest(const FString& ModuleAndTestName, TSharedPtr<struct FAngelscriptModuleDesc>* OutModule, FAngelscriptTestDesc* OutTestFuncDesc)
{
	FString ModuleName;
	FString TestFunctionName;
	if (!ModuleAndTestName.Split(".", &ModuleName, &TestFunctionName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		UE_LOG(Angelscript, Error, TEXT("Invalid module and test name: %s"), *ModuleAndTestName);
		return false;
	}

	if (!EnsureAngelscriptUnitRuntimeInitialized())
	{
		UE_LOG(Angelscript, Error, TEXT("Angelscript runtime is not initialized while looking up unit test %s."), *ModuleAndTestName);
		return false;
	}

	FAngelscriptEngine* AngelscriptManager = &FAngelscriptEngine::Get();
	*OutModule = AngelscriptManager->GetModule(ModuleName);
	if (*OutModule == nullptr)
	{
		UE_LOG(Angelscript, Error, TEXT("Was asked to run module %s, but it does not exist"), *ModuleName);
		return false;
	}

	FAngelscriptTestDesc* ModuleTestFuncDesc = (*OutModule)->GetUnitTestFunction(TestFunctionName);
	if (ModuleTestFuncDesc == nullptr)
	{
		UE_LOG(Angelscript, Error, TEXT("Was asked to run test %s, but it does not exist"), *ModuleAndTestName);
		return false;
	}
	*OutTestFuncDesc = *ModuleTestFuncDesc;

	FString NamingConvention = GetDefault<UAngelscriptTestSettings>()->UnitTestNamingConvention;
	if (NamingConvention.Len() > 0)
	{
		if (!ModuleName.MatchesWildcard(NamingConvention))
		{
			UE_LOG(Angelscript, Error, TEXT("Test '%s' found in module '%s', but tests should only be in modules matching '%s'."),
				*TestFunctionName, *ModuleName, *NamingConvention);
			return false;
		}
	}

	return true;
}

// Runs one test listed by GetTests.
bool FAngelscriptUnitTests::RunTest(const FString& ModuleAndTestName)
{
#if WITH_EDITOR
	if (GEditor && (GEditor->PlayWorld || GIsPlayInEditorWorld))
	{
		UE_LOG(Angelscript, Error, TEXT("I can't run unit tests while you have a PIE session running."));
		return false;
	}
#endif

	FAngelscriptTestDesc TestFunctionDesc;
	TSharedPtr<struct FAngelscriptModuleDesc> Module;
	if (!LookupUnitTest(ModuleAndTestName, &Module, &TestFunctionDesc))
	{
		return false;
	}
	FAngelscriptUnitTest T(&(*Module), TestFunctionDesc, &FAngelscriptEngine::Get());
	RunOneUnitTest(T);
	return true;
}

// Returns the list of tests to execute independently.
void FAngelscriptUnitTests::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	if (!EnsureAngelscriptUnitRuntimeInitialized())
	{
		return;
	}

	auto ActiveModules = FAngelscriptEngine::Get().GetActiveModules();
	for (const TSharedRef<FAngelscriptModuleDesc> &Module : ActiveModules)
	{
		for (auto Element : Module->UnitTestFunctions)
		{
			FString ModuleAndTestName = Module->ModuleName + "." + Element.Key;
			OutBeautifiedNames.Add(ModuleAndTestName);
			OutTestCommands.Add(ModuleAndTestName);
		}
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
