// ============================================================================
// AngelscriptSubsystemBindingsTests.cpp
//
// Runtime subsystem binding coverage.
// Automation ID:
//   Angelscript.TestModule.Bindings.Subsystem.*
//
// Sections:
//   NamespaceHelpers          - USubsystemLibrary::Get* helpers execute from script
//   NativeStaticGetAccessors  - native subsystem ClassName::Get() accessors
//   LocalPlayerAccessors      - LocalPlayer and PlayerController subsystem paths
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "AngelscriptEngineSubsystem.h"
#include "AngelscriptGameInstanceSubsystem.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "Net/Subsystems/NetworkSubsystem.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS



namespace AngelscriptSubsystemBindingsTest_Private
{
	constexpr int32 LocalPlayerControllerId = 0;

	bool ExecuteGlobalIntWithObjects(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const TCHAR* FunctionDecl,
		TArrayView<UObject* const> Args,
		int32& OutResult)
	{
		FASGlobalFunctionInvoker Invoker(Test, Engine, Module, FunctionDecl);
		if (!Invoker.IsValid())
		{
			return false;
		}

		for (UObject* Arg : Args)
		{
			Invoker.AddArgObject(Arg);
		}

		OutResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
		return Invoker.HasRun();
	}

	bool ExecuteGlobalIntWithObject(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const TCHAR* FunctionDecl,
		UObject* Arg,
		int32& OutResult)
	{
		TArray<UObject*, TInlineAllocator<1>> Args;
		Args.Add(Arg);
		return ExecuteGlobalIntWithObjects(Test, Engine, Module, FunctionDecl, Args, OutResult);
	}

	struct FStandaloneLocalPlayerFixture
	{
		~FStandaloneLocalPlayerFixture()
		{
			Shutdown();
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Subsystem local-player fixture should have a live GEngine"), GEngine))
			{
				return false;
			}

			const FName PackageName = MakeUniqueObjectName(
				nullptr,
				UPackage::StaticClass(),
				FName(TEXT("/Angelscript_Test_SubsystemBindingsLocalPlayer")));
			Package = NewObject<UPackage>(GetTransientPackage(), PackageName, RF_Transient);
			if (!Test.TestNotNull(TEXT("Subsystem local-player fixture should create a transient world package"), Package))
			{
				return false;
			}

			GameInstance = NewObject<UGameInstance>(GEngine, UGameInstance::StaticClass());
			if (!Test.TestNotNull(TEXT("Subsystem local-player fixture should create an engine-owned game instance"), GameInstance))
			{
				return false;
			}

			GameInstance->InitializeStandalone(TEXT("AngelscriptSubsystemBindingsLocalPlayerWorld"), Package);
			World = GameInstance->GetWorld();
			WorldContext = GameInstance->GetWorldContext();
			if (!Test.TestNotNull(TEXT("Subsystem local-player fixture should initialize a standalone world"), World)
				|| !Test.TestNotNull(TEXT("Subsystem local-player fixture should expose a world context"), WorldContext))
			{
				return false;
			}

			UClass* ViewportClass = GEngine->GameViewportClientClass != nullptr
				? GEngine->GameViewportClientClass.Get()
				: UGameViewportClient::StaticClass();
			GameViewport = NewObject<UGameViewportClient>(GEngine, ViewportClass);
			if (!Test.TestNotNull(TEXT("Subsystem local-player fixture should create a viewport client"), GameViewport))
			{
				return false;
			}

			GameViewport->Init(*WorldContext, GameInstance, /*bCreateNewAudioDevice*/false);
			WorldContext->GameViewport = GameViewport;
			return true;
		}

		void Shutdown()
		{
			if (GameInstance == nullptr && World == nullptr)
			{
				return;
			}

			if (World != nullptr)
			{
				World->BeginTearingDown();
			}

			if (GameInstance != nullptr)
			{
				GameInstance->Shutdown();
			}

			if (WorldContext != nullptr)
			{
				WorldContext->GameViewport = nullptr;
			}

			if (World != nullptr)
			{
				World->DestroyWorld(false);
				if (GEngine != nullptr)
				{
					GEngine->DestroyWorldContext(World);
				}
			}

			GameViewport = nullptr;
			WorldContext = nullptr;
			World = nullptr;
			GameInstance = nullptr;
			Package = nullptr;
		}

		UPackage* Package = nullptr;
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		FWorldContext* WorldContext = nullptr;
		UGameViewportClient* GameViewport = nullptr;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptSubsystemBindingsTest,
	"Angelscript.TestModule.Bindings.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(NamespaceHelpers)
	{
		using namespace AngelscriptSubsystemBindingsTest_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		UWorld* TestWorld = ContextActor.GetWorld();
		UGameInstance* GameInstance = TestWorld != nullptr ? TestWorld->GetGameInstance() : nullptr;
		if (!TestRunner->TestNotNull(TEXT("Subsystem namespace helper test should have a live GEngine"), GEngine)
			|| !TestRunner->TestNotNull(TEXT("Subsystem namespace helper test should create a test world"), TestWorld)
			|| !TestRunner->TestNotNull(TEXT("Subsystem namespace helper test should expose a game instance"), GameInstance))
		{
			return;
		}

		UAngelscriptEngineSubsystem* ExpectedEngineSubsystem = GEngine->GetEngineSubsystem<UAngelscriptEngineSubsystem>();
		UAngelscriptGameInstanceSubsystem* ExpectedGameInstanceSubsystem = GameInstance->GetSubsystem<UAngelscriptGameInstanceSubsystem>();
		UNetworkSubsystem* ExpectedWorldSubsystem = TestWorld->GetSubsystem<UNetworkSubsystem>();
		if (!TestRunner->TestNotNull(TEXT("Subsystem namespace helper test should expose the Angelscript engine subsystem"), ExpectedEngineSubsystem)
			|| !TestRunner->TestNotNull(TEXT("Subsystem namespace helper test should expose the Angelscript game-instance subsystem"), ExpectedGameInstanceSubsystem)
			|| !TestRunner->TestNotNull(TEXT("Subsystem namespace helper test should expose the network world subsystem"), ExpectedWorldSubsystem))
		{
			return;
		}

		const FString ScriptSource = TEXT(R"(
int VerifySubsystemNamespaceHelpers(
	UAngelscriptEngineSubsystem ExpectedEngineSubsystem,
	UAngelscriptGameInstanceSubsystem ExpectedGameInstanceSubsystem,
	UNetworkSubsystem ExpectedWorldSubsystem,
	UClass NullClass)
{
	int MismatchMask = 0;

	if (Cast<UAngelscriptEngineSubsystem>(USubsystemLibrary::GetEngineSubsystem(UAngelscriptEngineSubsystem::StaticClass())) != ExpectedEngineSubsystem)
		MismatchMask |= 1;
	if (Cast<UAngelscriptGameInstanceSubsystem>(USubsystemLibrary::GetGameInstanceSubsystem(UAngelscriptGameInstanceSubsystem::StaticClass())) != ExpectedGameInstanceSubsystem)
		MismatchMask |= 2;
	if (Cast<UNetworkSubsystem>(USubsystemLibrary::GetWorldSubsystem(UNetworkSubsystem::StaticClass())) != ExpectedWorldSubsystem)
		MismatchMask |= 4;

	if (USubsystemLibrary::GetEngineSubsystem(NullClass) != null)
		MismatchMask |= 8;
	if (USubsystemLibrary::GetGameInstanceSubsystem(NullClass) != null)
		MismatchMask |= 16;
	if (USubsystemLibrary::GetWorldSubsystem(NullClass) != null)
		MismatchMask |= 32;

	if (USubsystemLibrary::GetEngineSubsystem(AActor::StaticClass()) != null)
		MismatchMask |= 64;
	if (USubsystemLibrary::GetEngineSubsystem(UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
		MismatchMask |= 128;
	if (USubsystemLibrary::GetGameInstanceSubsystem(UAngelscriptEngineSubsystem::StaticClass()) != null)
		MismatchMask |= 256;
	if (USubsystemLibrary::GetWorldSubsystem(UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
		MismatchMask |= 512;

	return MismatchMask;
}
)");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSubsystem_NamespaceHelpers"), ScriptSource);
		if (!ModuleScope.IsValid())
		{
			return;
		}

		FScopedTestWorldContextScope WorldContextScope(&ContextActor);
		TArray<UObject*, TInlineAllocator<4>> Args;
		Args.Add(ExpectedEngineSubsystem);
		Args.Add(ExpectedGameInstanceSubsystem);
		Args.Add(ExpectedWorldSubsystem);

		Args.Add(nullptr);
		int32 ResultMask = INDEX_NONE;
		if (!ExecuteGlobalIntWithObjects(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifySubsystemNamespaceHelpers(UAngelscriptEngineSubsystem, UAngelscriptGameInstanceSubsystem, UNetworkSubsystem, UClass)"),
			Args,
			ResultMask))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Subsystem namespace helpers should return matching subsystem instances and reject null or mismatched classes"),
			ResultMask,
			0);
	}

	TEST_METHOD(NativeStaticGetAccessors)
	{
		using namespace AngelscriptSubsystemBindingsTest_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		UWorld* TestWorld = ContextActor.GetWorld();
		UGameInstance* GameInstance = TestWorld != nullptr ? TestWorld->GetGameInstance() : nullptr;
		if (!TestRunner->TestNotNull(TEXT("Subsystem static accessor test should have a live GEngine"), GEngine)
			|| !TestRunner->TestNotNull(TEXT("Subsystem static accessor test should create a test world"), TestWorld)
			|| !TestRunner->TestNotNull(TEXT("Subsystem static accessor test should expose a game instance"), GameInstance))
		{
			return;
		}

		UAngelscriptEngineSubsystem* ExpectedEngineSubsystem = GEngine->GetEngineSubsystem<UAngelscriptEngineSubsystem>();
		UAngelscriptGameInstanceSubsystem* ExpectedGameInstanceSubsystem = GameInstance->GetSubsystem<UAngelscriptGameInstanceSubsystem>();
		UNetworkSubsystem* ExpectedWorldSubsystem = TestWorld->GetSubsystem<UNetworkSubsystem>();
		if (!TestRunner->TestNotNull(TEXT("Subsystem static accessor test should expose the Angelscript engine subsystem"), ExpectedEngineSubsystem)
			|| !TestRunner->TestNotNull(TEXT("Subsystem static accessor test should expose the Angelscript game-instance subsystem"), ExpectedGameInstanceSubsystem)
			|| !TestRunner->TestNotNull(TEXT("Subsystem static accessor test should expose the network world subsystem"), ExpectedWorldSubsystem))
		{
			return;
		}

		const FString ScriptSource = TEXT(R"(
int VerifyNativeSubsystemStaticGetAccessors(
	UAngelscriptEngineSubsystem ExpectedEngineSubsystem,
	UAngelscriptGameInstanceSubsystem ExpectedGameInstanceSubsystem,
	UNetworkSubsystem ExpectedWorldSubsystem)
{
	int MismatchMask = 0;

	if (UAngelscriptEngineSubsystem::Get() != ExpectedEngineSubsystem)
		MismatchMask |= 1;
	if (UAngelscriptGameInstanceSubsystem::Get() != ExpectedGameInstanceSubsystem)
		MismatchMask |= 2;
	if (UNetworkSubsystem::Get() != ExpectedWorldSubsystem)
		MismatchMask |= 4;

	return MismatchMask;
}
)");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSubsystem_NativeStaticGet"), ScriptSource);
		if (!ModuleScope.IsValid())
		{
			return;
		}

		FScopedTestWorldContextScope WorldContextScope(&ContextActor);
		TArray<UObject*, TInlineAllocator<3>> Args;
		Args.Add(ExpectedEngineSubsystem);
		Args.Add(ExpectedGameInstanceSubsystem);
		Args.Add(ExpectedWorldSubsystem);

		int32 ResultMask = INDEX_NONE;
		if (!ExecuteGlobalIntWithObjects(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyNativeSubsystemStaticGetAccessors(UAngelscriptEngineSubsystem, UAngelscriptGameInstanceSubsystem, UNetworkSubsystem)"),
			Args,
			ResultMask))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Native subsystem static Get accessors should return the same instances as the C++ baselines"),
			ResultMask,
			0);
	}

	TEST_METHOD(LocalPlayerAccessors)
	{
		using namespace AngelscriptSubsystemBindingsTest_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FStandaloneLocalPlayerFixture Fixture;
		if (!Fixture.Initialize(*TestRunner))
		{
			return;
		}

		UWorld* TestWorld = Fixture.World;
		UGameInstance* GameInstance = Fixture.GameInstance;
		if (!TestRunner->TestNotNull(TEXT("Subsystem local-player accessor test should create a test world"), TestWorld)
			|| !TestRunner->TestNotNull(TEXT("Subsystem local-player accessor test should expose a game instance"), GameInstance))
		{
			return;
		}

		FString OutError;
		ULocalPlayer* LocalPlayer = GameInstance->CreateLocalPlayer(LocalPlayerControllerId, OutError, false);
		ON_SCOPE_EXIT
		{
			if (GameInstance != nullptr && LocalPlayer != nullptr && GameInstance->GetLocalPlayers().Contains(LocalPlayer))
			{
				GameInstance->RemoveLocalPlayer(LocalPlayer);
			}
		};

		if (!TestRunner->TestNotNull(TEXT("Subsystem local-player accessor test should create a local player"), LocalPlayer)
			|| !TestRunner->TestTrue(TEXT("Subsystem local-player accessor test should create the local player without an error"), OutError.IsEmpty()))
		{
			return;
		}

		APlayerController* PlayerController = TestWorld->SpawnActor<APlayerController>();
		if (!TestRunner->TestNotNull(TEXT("Subsystem local-player accessor test should spawn a player controller"), PlayerController))
		{
			return;
		}

		PlayerController->SetPlayer(LocalPlayer);

		UEnhancedInputLocalPlayerSubsystem* ExpectedLocalPlayerSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (!TestRunner->TestNotNull(TEXT("Subsystem local-player accessor test should expose the Enhanced Input local-player subsystem"), ExpectedLocalPlayerSubsystem)
			|| !TestRunner->TestTrue(TEXT("Subsystem local-player accessor test should bind the spawned player controller to the local player"), PlayerController->GetLocalPlayer() == LocalPlayer))
		{
			return;
		}

		const FString ScriptSource = TEXT(R"(
int VerifyLocalPlayerSubsystemAccessors(
	ULocalPlayer LocalPlayer,
	APlayerController PlayerController,
	UEnhancedInputLocalPlayerSubsystem ExpectedLocalPlayerSubsystem,
	ULocalPlayer NullLocalPlayer,
	APlayerController NullPlayerController)
{
	int MismatchMask = 0;

	if (Cast<UEnhancedInputLocalPlayerSubsystem>(USubsystemLibrary::GetLocalPlayerSubsystemFromLocalPlayer(LocalPlayer, UEnhancedInputLocalPlayerSubsystem::StaticClass())) != ExpectedLocalPlayerSubsystem)
		MismatchMask |= 1;
	if (Cast<UEnhancedInputLocalPlayerSubsystem>(USubsystemLibrary::GetLocalPlayerSubsystemFromPlayerController(PlayerController, UEnhancedInputLocalPlayerSubsystem::StaticClass())) != ExpectedLocalPlayerSubsystem)
		MismatchMask |= 2;
	if (UEnhancedInputLocalPlayerSubsystem::Get(LocalPlayer) != ExpectedLocalPlayerSubsystem)
		MismatchMask |= 4;
	if (UEnhancedInputLocalPlayerSubsystem::Get(PlayerController) != ExpectedLocalPlayerSubsystem)
		MismatchMask |= 8;

	if (USubsystemLibrary::GetLocalPlayerSubsystemFromLocalPlayer(NullLocalPlayer, UEnhancedInputLocalPlayerSubsystem::StaticClass()) != null)
		MismatchMask |= 16;
	if (USubsystemLibrary::GetLocalPlayerSubsystemFromPlayerController(NullPlayerController, UEnhancedInputLocalPlayerSubsystem::StaticClass()) != null)
		MismatchMask |= 32;
	if (USubsystemLibrary::GetLocalPlayerSubsystemFromLocalPlayer(LocalPlayer, UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
		MismatchMask |= 64;
	if (USubsystemLibrary::GetLocalPlayerSubsystemFromPlayerController(PlayerController, UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
		MismatchMask |= 128;

	return MismatchMask;
}

int VerifyAmbientLocalPlayerSubsystemAccessor(UEnhancedInputLocalPlayerSubsystem ExpectedLocalPlayerSubsystem)
{
	int MismatchMask = 0;

	if (Cast<UEnhancedInputLocalPlayerSubsystem>(USubsystemLibrary::GetLocalPlayerSubsystem(UEnhancedInputLocalPlayerSubsystem::StaticClass())) != ExpectedLocalPlayerSubsystem)
		MismatchMask |= 1;
	if (USubsystemLibrary::GetLocalPlayerSubsystem(UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
		MismatchMask |= 2;

	return MismatchMask;
}
)");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSubsystem_LocalPlayer"), ScriptSource);
		if (!ModuleScope.IsValid())
		{
			return;
		}

		TArray<UObject*, TInlineAllocator<5>> Args;
		Args.Add(LocalPlayer);
		Args.Add(PlayerController);
		Args.Add(ExpectedLocalPlayerSubsystem);
		Args.Add(nullptr);
		Args.Add(nullptr);

		int32 DirectResultMask = INDEX_NONE;
		if (!ExecuteGlobalIntWithObjects(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyLocalPlayerSubsystemAccessors(ULocalPlayer, APlayerController, UEnhancedInputLocalPlayerSubsystem, ULocalPlayer, APlayerController)"),
			Args,
			DirectResultMask))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Local-player subsystem helpers and native static accessors should resolve through explicit local-player and player-controller inputs"),
			DirectResultMask,
			0);

		int32 LocalPlayerAmbientResultMask = INDEX_NONE;
		{
			FScopedTestWorldContextScope LocalPlayerContextScope(LocalPlayer);
			if (!ExecuteGlobalIntWithObject(
				*TestRunner,
				Engine,
				ModuleScope.GetModule(),
				TEXT("int VerifyAmbientLocalPlayerSubsystemAccessor(UEnhancedInputLocalPlayerSubsystem)"),
				ExpectedLocalPlayerSubsystem,
				LocalPlayerAmbientResultMask))
			{
				return;
			}
		}

		int32 PlayerControllerAmbientResultMask = INDEX_NONE;
		{
			FScopedTestWorldContextScope PlayerControllerContextScope(PlayerController);
			if (!ExecuteGlobalIntWithObject(
				*TestRunner,
				Engine,
				ModuleScope.GetModule(),
				TEXT("int VerifyAmbientLocalPlayerSubsystemAccessor(UEnhancedInputLocalPlayerSubsystem)"),
				ExpectedLocalPlayerSubsystem,
				PlayerControllerAmbientResultMask))
			{
				return;
			}
		}

		TestRunner->TestEqual(
			TEXT("Ambient local-player subsystem helper should resolve when the world context object is a local player"),
			LocalPlayerAmbientResultMask,
			0);
		TestRunner->TestEqual(
			TEXT("Ambient local-player subsystem helper should resolve when the world context object is a player controller"),
			PlayerControllerAmbientResultMask,
			0);
	}
};

#endif
