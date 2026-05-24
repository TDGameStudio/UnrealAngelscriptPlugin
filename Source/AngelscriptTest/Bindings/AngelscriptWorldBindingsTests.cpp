// ============================================================================
// AngelscriptWorldBindingsTests.cpp
//
// World context and globals binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.Bindings.World.FAngelscriptWorldBindingsTest.*
//
// Sections:
//   ContextAndGlobalsCompat — world context, current world, persistent level,
//                             game instance, world type, and frame number
//
// CQTest adaptation notes:
//   The verification function takes seven parameters injected from C++ via
//   FASGlobalFunctionInvoker (UObject, UWorld, ULevel, UGameInstance, bool,
//   uint, uint).  A bitmask return encodes per-property mismatches so that
//   all checks run in a single script call.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptWorldBindingsTest,
	"Angelscript.TestModule.Bindings.World",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: ContextAndGlobalsCompat
	// ====================================================================

	TEST_METHOD(ContextAndGlobalsCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		UWorld* TestWorld = ContextActor.GetWorld();
		if (!TestRunner->TestNotNull(TEXT("World context bindings test should access the spawned test world"), TestWorld))
		{
			return;
		}

		ULevel* PersistentLevel = TestWorld->PersistentLevel;
		UGameInstance* GameInstance = TestWorld->GetGameInstance();
		if (!TestRunner->TestNotNull(TEXT("World context bindings test should expose a persistent level"), PersistentLevel)
			|| !TestRunner->TestNotNull(TEXT("World context bindings test should expose a game instance"), GameInstance))
		{
			return;
		}

		const FString ScriptSource = TEXT(R"(
int VerifyWorldBindings(
	UObject ExpectedContext,
	UWorld ExpectedWorld,
	ULevel ExpectedPersistentLevel,
	UGameInstance ExpectedGameInstance,
	bool bExpectedIsGameWorld,
	uint ExpectedWorldType,
	uint ExpectedFrameNumber)
{
	int MismatchMask = 0;

	if (__WorldContext() != ExpectedContext)
		MismatchMask |= 1;

	UWorld CurrentWorld = GetCurrentWorld();
	if (CurrentWorld == null)
		return MismatchMask | 2 | 4 | 8 | 16 | 32 | 64;

	if (CurrentWorld != ExpectedWorld)
		MismatchMask |= 2;
	if (CurrentWorld.IsGameWorld() != bExpectedIsGameWorld)
		MismatchMask |= 4;
	if (CurrentWorld.GetPersistentLevel() != ExpectedPersistentLevel)
		MismatchMask |= 8;
	if (CurrentWorld.GetGameInstance() != ExpectedGameInstance)
		MismatchMask |= 16;
	if (uint(CurrentWorld.WorldType) != ExpectedWorldType)
		MismatchMask |= 32;
	if (GFrameNumber != ExpectedFrameNumber)
		MismatchMask |= 64;

	return MismatchMask;
}
)");

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASWorld_ContextGlobals"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		FScopedTestWorldContextScope WorldContextScope(&ContextActor);

		const bool bExpectedIsGameWorld = TestWorld->IsGameWorld();
		const uint32 ExpectedWorldType = static_cast<uint32>(TestWorld->WorldType);
		const uint32 ExpectedFrameNumber = static_cast<uint32>(GFrameNumber);

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, M,
			TEXT("int VerifyWorldBindings(UObject, UWorld, ULevel, UGameInstance, bool, uint, uint)"));
		if (!Invoker.IsValid()) return;
		Invoker.AddArgObject(&ContextActor);
		Invoker.AddArgObject(TestWorld);
		Invoker.AddArgObject(PersistentLevel);
		Invoker.AddArgObject(GameInstance);
		Invoker.AddArg(bExpectedIsGameWorld);
		Invoker.AddArg(ExpectedWorldType);
		Invoker.AddArg(ExpectedFrameNumber);

		const int32 ResultMask = Invoker.CallAndReturn<int32>(INDEX_NONE);

		TestRunner->TestEqual(
			TEXT("World context bindings should preserve world context, world globals, persistent level, game instance, world type and frame number"),
			ResultMask,
			0);
		TestRunner->TestTrue(
			TEXT("World context bindings test should observe a game world in the spawned test case world"),
			bExpectedIsGameWorld);
		TestRunner->TestEqual(
			TEXT("World context bindings test should use the native world type baseline"),
			ExpectedWorldType,
			static_cast<uint32>(TestWorld->WorldType));
		TestRunner->TestEqual(
			TEXT("World context bindings test should capture the current frame number baseline"),
			ExpectedFrameNumber,
			static_cast<uint32>(GFrameNumber));
	}
};

#endif
