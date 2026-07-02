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
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

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

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptSubsystemBindingsTest,
	"Angelscript.TestModule.Bindings.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 LocalPlayerControllerId = 0;

	static bool ExecuteGlobalIntWithObjects(
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

	static bool ExecuteGlobalIntWithObject(
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
			FNoDiscardAsserter LocalAssert(Test);
			if (!LocalAssert.IsNotNull(GEngine, TEXT("Subsystem local-player fixture should have a live GEngine")))
			{
				return false;
			}

			const FName PackageName = MakeUniqueObjectName(
				nullptr,
				UPackage::StaticClass(),
				FName(TEXT("/Angelscript_Test_SubsystemBindingsLocalPlayer")));
			Package = NewObject<UPackage>(GetTransientPackage(), PackageName, RF_Transient);
			if (!LocalAssert.IsNotNull(Package, TEXT("Subsystem local-player fixture should create a transient world package")))
			{
				return false;
			}

			GameInstance = NewObject<UGameInstance>(GEngine, UGameInstance::StaticClass());
			if (!LocalAssert.IsNotNull(GameInstance, TEXT("Subsystem local-player fixture should create an engine-owned game instance")))
			{
				return false;
			}

			GameInstance->InitializeStandalone(TEXT("AngelscriptSubsystemBindingsLocalPlayerWorld"), Package);
			World = GameInstance->GetWorld();
			WorldContext = GameInstance->GetWorldContext();
			bool bHasWorldContext = true;
			bHasWorldContext &= LocalAssert.IsNotNull(World, TEXT("Subsystem local-player fixture should initialize a standalone world"));
			bHasWorldContext &= LocalAssert.IsNotNull(WorldContext, TEXT("Subsystem local-player fixture should expose a world context"));
			if (!bHasWorldContext)
			{
				return false;
			}

			UClass* ViewportClass = GEngine->GameViewportClientClass != nullptr
				? GEngine->GameViewportClientClass.Get()
				: UGameViewportClient::StaticClass();
			GameViewport = NewObject<UGameViewportClient>(GEngine, ViewportClass);
			if (!LocalAssert.IsNotNull(GameViewport, TEXT("Subsystem local-player fixture should create a viewport client")))
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

	TEST_METHOD(NamespaceHelpers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		UWorld* TestWorld = ContextActor.GetWorld();
		UGameInstance* GameInstance = TestWorld != nullptr ? TestWorld->GetGameInstance() : nullptr;
		ASSERT_THAT(IsNotNull(GEngine, TEXT("Subsystem namespace helper test should have a live GEngine")));
		ASSERT_THAT(IsNotNull(TestWorld, TEXT("Subsystem namespace helper test should create a test world")));
		ASSERT_THAT(IsNotNull(GameInstance, TEXT("Subsystem namespace helper test should expose a game instance")));

		UAngelscriptEngineSubsystem* ExpectedEngineSubsystem = GEngine->GetEngineSubsystem<UAngelscriptEngineSubsystem>();
		UAngelscriptGameInstanceSubsystem* ExpectedGameInstanceSubsystem = GameInstance->GetSubsystem<UAngelscriptGameInstanceSubsystem>();
		UNetworkSubsystem* ExpectedWorldSubsystem = TestWorld->GetSubsystem<UNetworkSubsystem>();
		ASSERT_THAT(IsNotNull(ExpectedEngineSubsystem, TEXT("Subsystem namespace helper test should expose the Angelscript engine subsystem")));
		ASSERT_THAT(IsNotNull(ExpectedGameInstanceSubsystem, TEXT("Subsystem namespace helper test should expose the Angelscript game-instance subsystem")));
		ASSERT_THAT(IsNotNull(ExpectedWorldSubsystem, TEXT("Subsystem namespace helper test should expose the network world subsystem")));

		const FString ScriptSource = ASTEST_AS(R"AS(
			int VerifySubsystemNamespaceHelpers(
			UAngelscriptEngineSubsystem ExpectedEngineSubsystem,
			UAngelscriptGameInstanceSubsystem ExpectedGameInstanceSubsystem,
			UNetworkSubsystem ExpectedWorldSubsystem,
			UClass NullClass)
			{
				int MismatchMask = 0;

				if (Cast<UAngelscriptEngineSubsystem>(USubsystemLibrary::GetEngineSubsystem(UAngelscriptEngineSubsystem::StaticClass())) != ExpectedEngineSubsystem)
				{
					MismatchMask |= 1;
				}
				if (Cast<UAngelscriptGameInstanceSubsystem>(USubsystemLibrary::GetGameInstanceSubsystem(UAngelscriptGameInstanceSubsystem::StaticClass())) != ExpectedGameInstanceSubsystem)
				{
					MismatchMask |= 2;
				}
				if (Cast<UNetworkSubsystem>(USubsystemLibrary::GetWorldSubsystem(UNetworkSubsystem::StaticClass())) != ExpectedWorldSubsystem)
				{
					MismatchMask |= 4;
				}

				if (USubsystemLibrary::GetEngineSubsystem(NullClass) != null)
				{
					MismatchMask |= 8;
				}
				if (USubsystemLibrary::GetGameInstanceSubsystem(NullClass) != null)
				{
					MismatchMask |= 16;
				}
				if (USubsystemLibrary::GetWorldSubsystem(NullClass) != null)
				{
					MismatchMask |= 32;
				}

				if (USubsystemLibrary::GetEngineSubsystem(AActor::StaticClass()) != null)
				{
					MismatchMask |= 64;
				}
				if (USubsystemLibrary::GetEngineSubsystem(UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
				{
					MismatchMask |= 128;
				}
				if (USubsystemLibrary::GetGameInstanceSubsystem(UAngelscriptEngineSubsystem::StaticClass()) != null)
				{
					MismatchMask |= 256;
				}
				if (USubsystemLibrary::GetWorldSubsystem(UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
				{
					MismatchMask |= 512;
				}

				return MismatchMask;
			}
			)AS");

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

		ASSERT_THAT(AreEqual(
			0,
			ResultMask,
			TEXT("Subsystem namespace helpers should return matching subsystem instances and reject null or mismatched classes")));
	}

	TEST_METHOD(NativeStaticGetAccessors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		UWorld* TestWorld = ContextActor.GetWorld();
		UGameInstance* GameInstance = TestWorld != nullptr ? TestWorld->GetGameInstance() : nullptr;
		ASSERT_THAT(IsNotNull(GEngine, TEXT("Subsystem static accessor test should have a live GEngine")));
		ASSERT_THAT(IsNotNull(TestWorld, TEXT("Subsystem static accessor test should create a test world")));
		ASSERT_THAT(IsNotNull(GameInstance, TEXT("Subsystem static accessor test should expose a game instance")));

		UAngelscriptEngineSubsystem* ExpectedEngineSubsystem = GEngine->GetEngineSubsystem<UAngelscriptEngineSubsystem>();
		UAngelscriptGameInstanceSubsystem* ExpectedGameInstanceSubsystem = GameInstance->GetSubsystem<UAngelscriptGameInstanceSubsystem>();
		UNetworkSubsystem* ExpectedWorldSubsystem = TestWorld->GetSubsystem<UNetworkSubsystem>();
		ASSERT_THAT(IsNotNull(ExpectedEngineSubsystem, TEXT("Subsystem static accessor test should expose the Angelscript engine subsystem")));
		ASSERT_THAT(IsNotNull(ExpectedGameInstanceSubsystem, TEXT("Subsystem static accessor test should expose the Angelscript game-instance subsystem")));
		ASSERT_THAT(IsNotNull(ExpectedWorldSubsystem, TEXT("Subsystem static accessor test should expose the network world subsystem")));

		const FString ScriptSource = ASTEST_AS(R"AS(
			int VerifyNativeSubsystemStaticGetAccessors(
			UAngelscriptEngineSubsystem ExpectedEngineSubsystem,
			UAngelscriptGameInstanceSubsystem ExpectedGameInstanceSubsystem,
			UNetworkSubsystem ExpectedWorldSubsystem)
			{
				int MismatchMask = 0;

				if (UAngelscriptEngineSubsystem::Get() != ExpectedEngineSubsystem)
				{
					MismatchMask |= 1;
				}
				if (UAngelscriptGameInstanceSubsystem::Get() != ExpectedGameInstanceSubsystem)
				{
					MismatchMask |= 2;
				}
				if (UNetworkSubsystem::Get() != ExpectedWorldSubsystem)
				{
					MismatchMask |= 4;
				}

				return MismatchMask;
			}
			)AS");

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

		ASSERT_THAT(AreEqual(
			0,
			ResultMask,
			TEXT("Native subsystem static Get accessors should return the same instances as the C++ baselines")));
	}

	TEST_METHOD(LocalPlayerAccessors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FStandaloneLocalPlayerFixture Fixture;
		if (!Fixture.Initialize(*TestRunner))
		{
			return;
		}

		UWorld* TestWorld = Fixture.World;
		UGameInstance* GameInstance = Fixture.GameInstance;
		ASSERT_THAT(IsNotNull(TestWorld, TEXT("Subsystem local-player accessor test should create a test world")));
		ASSERT_THAT(IsNotNull(GameInstance, TEXT("Subsystem local-player accessor test should expose a game instance")));

		FString OutError;
		ULocalPlayer* LocalPlayer = GameInstance->CreateLocalPlayer(
			LocalPlayerControllerId,
			OutError,
			false);
		ON_SCOPE_EXIT
		{
			if (GameInstance != nullptr && LocalPlayer != nullptr && GameInstance->GetLocalPlayers().Contains(LocalPlayer))
			{
				GameInstance->RemoveLocalPlayer(LocalPlayer);
			}
		};

		bool bLocalPlayerCreated = true;
		bLocalPlayerCreated &= this->Assert.IsNotNull(LocalPlayer, TEXT("Subsystem local-player accessor test should create a local player"));
		bLocalPlayerCreated &= this->Assert.IsTrue(OutError.IsEmpty(), TEXT("Subsystem local-player accessor test should create the local player without an error"));
		if (!bLocalPlayerCreated)
		{
			return;
		}

		APlayerController* PlayerController = TestWorld->SpawnActor<APlayerController>();
		ASSERT_THAT(IsNotNull(PlayerController, TEXT("Subsystem local-player accessor test should spawn a player controller")));

		PlayerController->SetPlayer(LocalPlayer);

		UEnhancedInputLocalPlayerSubsystem* ExpectedLocalPlayerSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		ASSERT_THAT(IsNotNull(ExpectedLocalPlayerSubsystem, TEXT("Subsystem local-player accessor test should expose the Enhanced Input local-player subsystem")));
		ASSERT_THAT(IsTrue(PlayerController->GetLocalPlayer() == LocalPlayer, TEXT("Subsystem local-player accessor test should bind the spawned player controller to the local player")));

		const FString ScriptSource = ASTEST_AS(R"AS(
			int VerifyLocalPlayerSubsystemAccessors(
			ULocalPlayer LocalPlayer,
			APlayerController PlayerController,
			UEnhancedInputLocalPlayerSubsystem ExpectedLocalPlayerSubsystem,
			ULocalPlayer NullLocalPlayer,
			APlayerController NullPlayerController)
			{
				int MismatchMask = 0;

				if (Cast<UEnhancedInputLocalPlayerSubsystem>(USubsystemLibrary::GetLocalPlayerSubsystemFromLocalPlayer(LocalPlayer, UEnhancedInputLocalPlayerSubsystem::StaticClass())) != ExpectedLocalPlayerSubsystem)
				{
					MismatchMask |= 1;
				}
				if (Cast<UEnhancedInputLocalPlayerSubsystem>(USubsystemLibrary::GetLocalPlayerSubsystemFromPlayerController(PlayerController, UEnhancedInputLocalPlayerSubsystem::StaticClass())) != ExpectedLocalPlayerSubsystem)
				{
					MismatchMask |= 2;
				}
				if (UEnhancedInputLocalPlayerSubsystem::Get(LocalPlayer) != ExpectedLocalPlayerSubsystem)
				{
					MismatchMask |= 4;
				}
				if (UEnhancedInputLocalPlayerSubsystem::Get(PlayerController) != ExpectedLocalPlayerSubsystem)
				{
					MismatchMask |= 8;
				}

				if (USubsystemLibrary::GetLocalPlayerSubsystemFromLocalPlayer(NullLocalPlayer, UEnhancedInputLocalPlayerSubsystem::StaticClass()) != null)
				{
					MismatchMask |= 16;
				}
				if (USubsystemLibrary::GetLocalPlayerSubsystemFromPlayerController(NullPlayerController, UEnhancedInputLocalPlayerSubsystem::StaticClass()) != null)
				{
					MismatchMask |= 32;
				}
				if (USubsystemLibrary::GetLocalPlayerSubsystemFromLocalPlayer(LocalPlayer, UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
				{
					MismatchMask |= 64;
				}
				if (USubsystemLibrary::GetLocalPlayerSubsystemFromPlayerController(PlayerController, UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
				{
					MismatchMask |= 128;
				}

				return MismatchMask;
			}

			int VerifyAmbientLocalPlayerSubsystemAccessor(UEnhancedInputLocalPlayerSubsystem ExpectedLocalPlayerSubsystem)
			{
				int MismatchMask = 0;

				if (Cast<UEnhancedInputLocalPlayerSubsystem>(USubsystemLibrary::GetLocalPlayerSubsystem(UEnhancedInputLocalPlayerSubsystem::StaticClass())) != ExpectedLocalPlayerSubsystem)
				{
					MismatchMask |= 1;
				}
				if (USubsystemLibrary::GetLocalPlayerSubsystem(UAngelscriptGameInstanceSubsystem::StaticClass()) != null)
				{
					MismatchMask |= 2;
				}

				return MismatchMask;
			}
			)AS");

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

		ASSERT_THAT(AreEqual(
			0,
			DirectResultMask,
			TEXT("Local-player subsystem helpers and native static accessors should resolve through explicit local-player and player-controller inputs")));

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

		ASSERT_THAT(AreEqual(
			0,
			LocalPlayerAmbientResultMask,
			TEXT("Ambient local-player subsystem helper should resolve when the world context object is a local player")));
		ASSERT_THAT(AreEqual(
			0,
			PlayerControllerAmbientResultMask,
			TEXT("Ambient local-player subsystem helper should resolve when the world context object is a player controller")));
	}
};

#endif
