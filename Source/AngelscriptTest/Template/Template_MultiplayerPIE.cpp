#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "Editor/AngelscriptPIETestUtils.h"

#include "Engine/GameInstance.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Tests/AutomationEditorCommon.h"

#if WITH_ANGELSCRIPT_UNITTESTS && WITH_EDITOR

TEST_CLASS_WITH_FLAGS(FAngelscriptTemplateMultiplayerPIETest,
	"Angelscript.Template.MultiplayerPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr double DefaultTimeoutSeconds = 10.0;

	static FName MakeModuleName(int32 PlayerCount)
	{
		return FName(*FString::Printf(TEXT("ASTemplateMultiplayerPIE%dPlayers"), PlayerCount));
	}

	static FString MakeFilename(int32 PlayerCount)
	{
		return FString::Printf(TEXT("ASTemplateMultiplayerPIE%dPlayers.as"), PlayerCount);
	}

	static FName MakeGameModeClassName(int32 PlayerCount)
	{
		return FName(*FString::Printf(TEXT("ATemplateMultiplayerPIE%dPlayersGameMode"), PlayerCount));
	}

	static FName MakeLevelScriptClassName(int32 PlayerCount)
	{
		return FName(*FString::Printf(TEXT("ATemplateMultiplayerPIE%dPlayersLevelScriptParent"), PlayerCount));
	}

	static FString MakeContext(int32 PlayerCount)
	{
		return FString::Printf(TEXT("Template_MultiplayerPIE_%dPlayers"), PlayerCount);
	}

	static FString MakeScriptSource(int32 PlayerCount)
	{
		FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class __GameModeClassName__ : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class __LevelScriptClassName__ : ALevelScriptActor
			{
				default SetReplicates(false);

				UPROPERTY()
				int TemplatePlayerCount = __PlayerCount__;
			}
		)AS");

		ScriptSource.ReplaceInline(TEXT("__GameModeClassName__"), *MakeGameModeClassName(PlayerCount).ToString());
		ScriptSource.ReplaceInline(TEXT("__LevelScriptClassName__"), *MakeLevelScriptClassName(PlayerCount).ToString());
		ScriptSource.ReplaceInline(TEXT("__PlayerCount__"), *FString::FromInt(PlayerCount));
		return ScriptSource;
	}

	static void DiscardTemplateModule(FName ModuleName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		Engine.DiscardModule(*ModuleName.ToString());
	}

	static void AddExpectedNetworkWarnings()
	{
		TestRunner->AddExpectedErrorPlain(TEXT("FNetGUIDCache::SupportsObject: Level /Temp/"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("RegisterNetGUID_Client: Guid with pathname\\. FullNetGUIDPath: \\[[0-9]+\\]WorldSettings"), EAutomationExpectedErrorFlags::Contains, -1);
	}

	void AssertListenServerWorld(UWorld* ServerWorld, UClass* GameModeClass, UClass* LevelScriptClass, int32 ExpectedClientWorldCount, const FString& Context)
	{
		ASSERT_THAT(IsNotNull(ServerWorld, *FString::Printf(TEXT("%s should expose a server PIE world"), *Context)));
		ASSERT_THAT(AreEqual(EWorldType::PIE, ServerWorld->WorldType, *FString::Printf(TEXT("%s server world type should be PIE"), *Context)));
		ASSERT_THAT(IsNotNull(ServerWorld->PersistentLevel.Get(), *FString::Printf(TEXT("%s server world should have a persistent level"), *Context)));
		ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::HasExpectedLevelScriptActor(ServerWorld, LevelScriptClass), *FString::Printf(TEXT("%s server LevelScriptActor should inherit from the AS parent"), *Context)));
		ASSERT_THAT(AreEqual(NM_ListenServer, ServerWorld->GetNetMode(), *FString::Printf(TEXT("%s server should run as listen server"), *Context)));
		ASSERT_THAT(IsNotNull(ServerWorld->GetGameInstance(), *FString::Printf(TEXT("%s server should have a GameInstance"), *Context)));
		ASSERT_THAT(IsTrue(ServerWorld->GetGameInstance()->GetClass()->IsChildOf(UGameInstance::StaticClass()), *FString::Printf(TEXT("%s server GameInstance should be usable from PIE"), *Context)));

		UNetDriver* ServerNetDriver = ServerWorld->GetNetDriver();
		ASSERT_THAT(IsNotNull(ServerNetDriver, *FString::Printf(TEXT("%s server world should have a net driver"), *Context)));
		ASSERT_THAT(IsTrue(ServerNetDriver->IsServer(), *FString::Printf(TEXT("%s server net driver should be authoritative"), *Context)));
		ASSERT_THAT(AreEqual(ExpectedClientWorldCount, ServerNetDriver->ClientConnections.Num(), *FString::Printf(TEXT("%s server should have the expected client connections"), *Context)));
		ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::AreClientConnectionsReady(ServerWorld, ExpectedClientWorldCount), *FString::Printf(TEXT("%s server client connections should be ready"), *Context)));
		ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::HasExpectedGameMode(ServerWorld, GameModeClass), *FString::Printf(TEXT("%s server should use the AS GameMode class"), *Context)));
	}

	void AssertClientWorld(UWorld* ClientWorld, UClass* LevelScriptClass, int32 ClientIndex, const FString& Context)
	{
		const FString ClientContext = FString::Printf(TEXT("%s client %d"), *Context, ClientIndex + 1);
		ASSERT_THAT(IsNotNull(ClientWorld, *FString::Printf(TEXT("%s should expose a PIE world"), *ClientContext)));
		ASSERT_THAT(AreEqual(EWorldType::PIE, ClientWorld->WorldType, *FString::Printf(TEXT("%s world type should be PIE"), *ClientContext)));
		ASSERT_THAT(IsNotNull(ClientWorld->PersistentLevel.Get(), *FString::Printf(TEXT("%s world should have a persistent level"), *ClientContext)));
		ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::HasExpectedLevelScriptActor(ClientWorld, LevelScriptClass), *FString::Printf(TEXT("%s LevelScriptActor should inherit from the AS parent"), *ClientContext)));
		ASSERT_THAT(AreEqual(NM_Client, ClientWorld->GetNetMode(), *FString::Printf(TEXT("%s should run as network client"), *ClientContext)));
		ASSERT_THAT(IsNotNull(ClientWorld->GetGameInstance(), *FString::Printf(TEXT("%s should have a GameInstance"), *ClientContext)));

		UNetDriver* ClientNetDriver = ClientWorld->GetNetDriver();
		ASSERT_THAT(IsNotNull(ClientNetDriver, *FString::Printf(TEXT("%s world should have a net driver"), *ClientContext)));
		ASSERT_THAT(IsFalse(ClientNetDriver->IsServer(), *FString::Printf(TEXT("%s net driver should not be authoritative"), *ClientContext)));
		ASSERT_THAT(IsNotNull(ClientWorld->GetGameState(), *FString::Printf(TEXT("%s should receive a GameState"), *ClientContext)));
	}

	void RunMultiplayerPIEScenario(int32 PlayerCount)
	{
		const int32 ExpectedClientWorldCount = PlayerCount - 1;
		const FName ModuleName = MakeModuleName(PlayerCount);
		const FString Filename = MakeFilename(PlayerCount);
		const FName GameModeClassName = MakeGameModeClassName(PlayerCount);
		const FName LevelScriptClassName = MakeLevelScriptClassName(PlayerCount);
		const FString Context = MakeContext(PlayerCount);

		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			*FString::Printf(TEXT("%s must start from editor mode with no existing PIE session"), *Context)));

		AddExpectedNetworkWarnings();

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		UClass* GameModeClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			Filename,
			MakeScriptSource(PlayerCount),
			GameModeClassName);
		ASSERT_THAT(IsNotNull(GameModeClass, *FString::Printf(TEXT("%s should compile an AS GameMode class"), *Context)));
		ASSERT_THAT(IsTrue(GameModeClass->IsChildOf(AGameModeBase::StaticClass()), *FString::Printf(TEXT("%s AS GameMode should derive from AGameModeBase"), *Context)));

		UClass* LevelScriptClass = FindGeneratedClass(&Engine, LevelScriptClassName);
		ASSERT_THAT(IsNotNull(LevelScriptClass, *FString::Printf(TEXT("%s should compile an AS LevelScriptActor parent"), *Context)));
		ASSERT_THAT(IsTrue(LevelScriptClass->IsChildOf(ALevelScriptActor::StaticClass()), *FString::Printf(TEXT("%s AS LevelScript parent should derive from ALevelScriptActor"), *Context)));

		AngelscriptPIETestUtils::FScopedLevelScriptActorClassOverride LevelScriptOverride(LevelScriptClass);
		UWorld* EditorWorld = AngelscriptPIETestUtils::CreateTransientEmptyMap(*TestRunner, *Context);
		ASSERT_THAT(IsNotNull(EditorWorld, *FString::Printf(TEXT("%s should create a transient editor map"), *Context)));

		ULevelScriptBlueprint* LevelBlueprint = AngelscriptPIETestUtils::CreateAndCompileLevelBlueprint(
			*TestRunner,
			EditorWorld,
			LevelScriptClass,
			*Context);
		ASSERT_THAT(IsNotNull(LevelBlueprint, *FString::Printf(TEXT("%s should create a Level Blueprint with an AS parent"), *Context)));

		FRequestPlaySessionParams RequestParams;
		ASSERT_THAT(IsTrue(
			AngelscriptPIETestUtils::BuildListenServerPIERequest(*TestRunner, GameModeClass, ExpectedClientWorldCount, RequestParams),
			*FString::Printf(TEXT("%s should build a listen-server PIE request"), *Context)));

		TestCommandBuilder.CleanUpWith(TEXT("Discard Template_MultiplayerPIE AS module"), [ModuleName]()
		{
			DiscardTemplateModule(ModuleName);
		});
		TestCommandBuilder.CleanUpWith(TEXT("End Template_MultiplayerPIE PIE cleanup"), []()
		{
			AngelscriptPIETestUtils::EndPIE();
		});

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for Template_MultiplayerPIE worlds"), [ExpectedClientWorldCount]()
			{
				return AngelscriptPIETestUtils::HasExpectedNetworkPIEWorlds(ExpectedClientWorldCount);
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert Template_MultiplayerPIE worlds"), [this, GameModeClass, LevelScriptClass, ExpectedClientWorldCount, Context]()
			{
				const AngelscriptPIETestUtils::FNetworkPIEWorlds Worlds = AngelscriptPIETestUtils::FindNetworkPIEWorlds();
				ASSERT_THAT(AreEqual(ExpectedClientWorldCount, Worlds.ClientWorlds.Num(), *FString::Printf(TEXT("%s should expose the expected client PIE worlds"), *Context)));
				AssertListenServerWorld(Worlds.ServerWorld, GameModeClass, LevelScriptClass, ExpectedClientWorldCount, Context);
				for (int32 ClientIndex = 0; ClientIndex < Worlds.ClientWorlds.Num(); ++ClientIndex)
				{
					AssertClientWorld(Worlds.ClientWorlds[ClientIndex], LevelScriptClass, ClientIndex, Context);
				}
			})
			.Then(TEXT("End Template_MultiplayerPIE PIE session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for Template_MultiplayerPIE PIE shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
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

	TEST_METHOD(EmptyMap_TwoPlayers_StartPIE_EndPIE_WithAngelscriptGameModeAndLevelBlueprint)
	{
		RunMultiplayerPIEScenario(2);
	}

	TEST_METHOD(EmptyMap_ThreePlayers_StartPIE_EndPIE_WithAngelscriptGameModeAndLevelBlueprint)
	{
		RunMultiplayerPIEScenario(3);
	}

	TEST_METHOD(EmptyMap_FourPlayers_StartPIE_EndPIE_WithAngelscriptGameModeAndLevelBlueprint)
	{
		RunMultiplayerPIEScenario(4);
	}
};

#endif
