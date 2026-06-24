// ============================================================================
// AngelscriptGameInstanceLocalPlayerBindingsTests.cpp
//
// GameInstance local player binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.Bindings.GameInstanceLocalPlayer.*
//
// Sections:
//   Compat — Full create/lookup/remove lifecycle through script bindings
//
// CQTest adaptation notes:
//   Original single legacy automation test converted to one
//   TEST_CLASS with one TEST_METHOD. The $TOKEN$ replacement pattern is
//   preserved via ReplaceInline. The custom fixture and argument-binding
//   helpers are retained for the object-arg calling convention.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Shared helpers
// ----------------------------------------------------------------------------

namespace GameInstanceLocalPlayerTestHelpers
{
	static constexpr int32 LocalPlayerControllerId = 7;

	bool SetArgObjectChecked(
		FAutomationTestBase& Test,
		asIScriptContext& Context,
		asUINT ArgumentIndex,
		void* Object,
		const TCHAR* ContextLabel)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.AreEqual(
			static_cast<int32>(asSUCCESS),
			Context.SetArgObject(ArgumentIndex, Object),
			*FString::Printf(TEXT("%s should bind object argument %u"), ContextLabel, static_cast<uint32>(ArgumentIndex)));
	}

	struct FGameInstanceLocalPlayerFixture
	{
		~FGameInstanceLocalPlayerFixture()
		{
			Shutdown();
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			FNoDiscardAsserter LocalAssert(Test);
			if (!LocalAssert.IsNotNull(GEngine, TEXT("GameInstance local-player bindings test should have a live GEngine")))
			{
				return false;
			}

			const FName PackageName = MakeUniqueObjectName(
				nullptr,
				UPackage::StaticClass(),
				FName(TEXT("/Angelscript_Test_GameInstanceLocalPlayer")));
			Package = NewObject<UPackage>(GetTransientPackage(), PackageName, RF_Transient);
			if (!LocalAssert.IsNotNull(Package, TEXT("GameInstance local-player bindings test should create a transient world package")))
			{
				return false;
			}

			GameInstance = NewObject<UGameInstance>(GEngine, UGameInstance::StaticClass());
			if (!LocalAssert.IsNotNull(GameInstance, TEXT("GameInstance local-player bindings test should create a standalone game instance")))
			{
				return false;
			}

			GameInstance->InitializeStandalone(TEXT("AngelscriptGameInstanceLocalPlayerWorld"), Package);
			World = GameInstance->GetWorld();
			WorldContext = GameInstance->GetWorldContext();
			if (!LocalAssert.IsNotNull(World, TEXT("GameInstance local-player bindings test should initialize a standalone world"))
				|| !LocalAssert.IsNotNull(WorldContext, TEXT("GameInstance local-player bindings test should expose a world context")))
			{
				return false;
			}

			UClass* ViewportClass = GEngine->GameViewportClientClass != nullptr
				? GEngine->GameViewportClientClass.Get()
				: UGameViewportClient::StaticClass();
			GameViewport = NewObject<UGameViewportClient>(GEngine, ViewportClass);
			if (!LocalAssert.IsNotNull(GameViewport, TEXT("GameInstance local-player bindings test should create a viewport client")))
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

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptGameInstanceLocalPlayerBindingsTest,
	"Angelscript.TestModule.Bindings.GameInstanceLocalPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: Compat
	// ====================================================================

	TEST_METHOD(Compat)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);

		GameInstanceLocalPlayerTestHelpers::FGameInstanceLocalPlayerFixture Fixture;
		if (!Fixture.Initialize(*TestRunner)) return;

		UWorld* TestWorld = Fixture.World;
		UGameInstance* GameInstance = Fixture.GameInstance;
		ASSERT_THAT(IsNotNull(TestWorld));
		ASSERT_THAT(IsNotNull(GameInstance));

		const int32 InitialLocalPlayerCount = GameInstance->GetNumLocalPlayers();
		if (!this->Assert.AreEqual(
			0,
			InitialLocalPlayerCount,
			TEXT("GameInstance local-player bindings test should start from a world without pre-existing local players")))
		{
			return;
		}

		if (!this->Assert.IsTrue(
			GameInstance->FindLocalPlayerFromControllerId(GameInstanceLocalPlayerTestHelpers::LocalPlayerControllerId) == nullptr,
			TEXT("GameInstance local-player bindings test should reserve controller id 7 before script execution")))
		{
			return;
		}

		FString Script = TEXT(R"(
int VerifyGameInstanceLocalPlayerCompat(UWorld ExpectedWorld, UGameInstance GameInstance)
{
	int MismatchMask = 0;

	if (ExpectedWorld == null)
		MismatchMask |= 1;
	if (GameInstance == null)
		MismatchMask |= 2;
	if (MismatchMask != 0)
		return MismatchMask;

	if (GameInstance.GetNumLocalPlayers() != $INITIAL_COUNT$)
		MismatchMask |= 4;

	FString OutError;
	ULocalPlayer Created = GameInstance.CreateLocalPlayer($CONTROLLER_ID$, OutError, false);
	if (Created == null)
		return MismatchMask | 8;

	if (OutError.Len() != 0)
		MismatchMask |= 16;
	if (GameInstance.GetNumLocalPlayers() != ($INITIAL_COUNT$ + 1))
		MismatchMask |= 32;
	if (GameInstance.GetLocalPlayerByIndex($INITIAL_COUNT$) != Created)
		MismatchMask |= 64;
	if (GameInstance.FindLocalPlayerFromControllerId($CONTROLLER_ID$) != Created)
		MismatchMask |= 128;
	if (GameInstance.GetFirstGamePlayer() != Created)
		MismatchMask |= 256;
	if (Created.GetGameInstance() != GameInstance)
		MismatchMask |= 512;
	if (Created.GetWorld() != ExpectedWorld)
		MismatchMask |= 1024;
	if (Created.GetControllerId() != $CONTROLLER_ID$)
		MismatchMask |= 2048;
	if (GameInstance.GetFirstLocalPlayerController(ExpectedWorld) != null)
		MismatchMask |= 4096;

	if (!GameInstance.RemoveLocalPlayer(Created))
		return MismatchMask | 8192;

	if (GameInstance.GetNumLocalPlayers() != $INITIAL_COUNT$)
		MismatchMask |= 16384;
	if (GameInstance.FindLocalPlayerFromControllerId($CONTROLLER_ID$) != null)
		MismatchMask |= 32768;

	return MismatchMask;
}
)");
		Script.ReplaceInline(TEXT("$INITIAL_COUNT$"), *LexToString(InitialLocalPlayerCount));
		Script.ReplaceInline(TEXT("$CONTROLLER_ID$"), *LexToString(GameInstanceLocalPlayerTestHelpers::LocalPlayerControllerId));

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASGameInstanceLocalPlayerCompat", Script);
		if (Module == nullptr) return;

		FScopedTestWorldContextScope WorldContextScope(TestWorld);

		asIScriptFunction* EntryFunction = GetFunctionByDecl(
			*TestRunner,
			*Module,
			TEXT("int VerifyGameInstanceLocalPlayerCompat(UWorld, UGameInstance)"));
		if (EntryFunction == nullptr) return;

		asIScriptContext* Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context));

		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};

		const int PrepareResult = Context->Prepare(EntryFunction);
		if (!this->Assert.AreEqual(
			static_cast<int32>(asSUCCESS),
			PrepareResult,
			TEXT("GameInstance local-player bindings test should prepare the verification function")))
		{
			return;
		}

		if (!GameInstanceLocalPlayerTestHelpers::SetArgObjectChecked(*TestRunner, *Context, 0, TestWorld, TEXT("VerifyGameInstanceLocalPlayerCompat"))
			|| !GameInstanceLocalPlayerTestHelpers::SetArgObjectChecked(*TestRunner, *Context, 1, GameInstance, TEXT("VerifyGameInstanceLocalPlayerCompat")))
		{
			return;
		}

		const int ExecuteResult = Context->Execute();
		if (!this->Assert.AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			TEXT("GameInstance local-player bindings test should execute the verification function")))
		{
			return;
		}

		const int32 ResultMask = static_cast<int32>(Context->GetReturnDWord());

		ASSERT_THAT(AreEqual(
			0,
			ResultMask,
			TEXT("GameInstance local-player bindings should preserve create, lookup, world/game-instance linkage and remove semantics")));
		ASSERT_THAT(AreEqual(
			InitialLocalPlayerCount,
			GameInstance->GetNumLocalPlayers(),
			TEXT("GameInstance local-player bindings test should restore the native local-player count after script removal")));
		ASSERT_THAT(IsTrue(
			GameInstance->FindLocalPlayerFromControllerId(GameInstanceLocalPlayerTestHelpers::LocalPlayerControllerId) == nullptr,
			TEXT("GameInstance local-player bindings test should remove the created controller-id lookup after script cleanup")));
		ASSERT_THAT(IsTrue(
			GameInstance->GetFirstLocalPlayerController(TestWorld) == nullptr,
			TEXT("GameInstance local-player bindings test should keep GetFirstLocalPlayerController native baseline at null when no controller is spawned")));
	}
};

#endif
