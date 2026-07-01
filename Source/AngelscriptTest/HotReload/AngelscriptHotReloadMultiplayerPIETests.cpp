#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "ClassGenerator/ASClass.h"
#include "Editor/AngelscriptPIETestUtils.h"

#include "Engine/GameInstance.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS && WITH_EDITOR

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadMultiplayerPIETests,
	"Angelscript.TestModule.HotReload.MultiplayerPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FString SoftReloadFilename = TEXT("ASHotReloadMultiplayerPIESoft.as");
	inline static const FName SoftReloadModuleName = TEXT("ASHotReloadMultiplayerPIESoft");
	inline static const FName SoftReloadGameModeClassName = TEXT("AHotReloadMultiplayerPIESoftGameMode");
	inline static const FName SoftReloadLevelScriptClassName = TEXT("AHotReloadMultiplayerPIESoftLevelScript");

	inline static const FString SuggestedReloadFilename = TEXT("ASHotReloadMultiplayerPIESuggested.as");
	inline static const FName SuggestedReloadModuleName = TEXT("ASHotReloadMultiplayerPIESuggested");
	inline static const FName SuggestedReloadGameModeClassName = TEXT("AHotReloadMultiplayerPIESuggestedGameMode");
	inline static const FName SuggestedReloadLevelScriptClassName = TEXT("AHotReloadMultiplayerPIESuggestedLevelScript");

	static constexpr int32 TwoPlayerClientWorldCount = 1;
	static constexpr double DefaultTimeoutSeconds = 10.0;

	struct FMultiplayerPIEFixture
	{
		UClass* GameModeClass = nullptr;
		UClass* LevelScriptClass = nullptr;
		UASClass* InitialLevelScriptASClass = nullptr;
		UWorld* EditorWorld = nullptr;
		ULevelScriptBlueprint* LevelBlueprint = nullptr;
		FRequestPlaySessionParams RequestParams;
	};

	class FStartPIEFromRequestProvider : public IAutomationLatentCommand
	{
	public:
		explicit FStartPIEFromRequestProvider(TFunction<FRequestPlaySessionParams()> InRequestProvider)
			: RequestProvider(MoveTemp(InRequestProvider))
		{
		}

		bool Update() override
		{
			if (!StartCommand.IsValid())
			{
				FRequestPlaySessionParams RequestParams = RequestProvider();
				if (FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest())
				{
					if (CurrentTest->HasAnyErrors())
					{
						return true;
					}
				}

				StartCommand = MakeUnique<FStartPIEForAutomationCommand>(RequestParams);
			}

			return StartCommand->Update();
		}

	private:
		TFunction<FRequestPlaySessionParams()> RequestProvider;
		TUniquePtr<FStartPIEForAutomationCommand> StartCommand;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static void AddExpectedNetworkWarnings()
	{
		TestRunner->AddExpectedErrorPlain(TEXT("FNetGUIDCache::SupportsObject: Level /Temp/"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("RegisterNetGUID_Client: Guid with pathname\\. FullNetGUIDPath: \\[[0-9]+\\]WorldSettings"), EAutomationExpectedErrorFlags::Contains, -1);
	}

	static void DiscardModule(const FName ModuleName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		Engine.DiscardModule(*ModuleName.ToString());
	}

	static bool PrepareMultiplayerPIEFixture(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& ScriptSource,
		const FName GameModeClassName,
		const FName LevelScriptClassName,
		const TCHAR* Context,
		FMultiplayerPIEFixture& OutFixture)
	{
		FNoDiscardAsserter LocalAssert(Test);

		OutFixture.GameModeClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			Filename,
			ScriptSource,
			GameModeClassName);
		if (!LocalAssert.IsNotNull(OutFixture.GameModeClass, *FString::Printf(TEXT("%s should compile an AS GameMode"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsTrue(
				OutFixture.GameModeClass->IsChildOf(AGameModeBase::StaticClass()),
				*FString::Printf(TEXT("%s GameMode should derive from AGameModeBase"), Context)))
		{
			return false;
		}

		OutFixture.LevelScriptClass = FindGeneratedClass(&Engine, LevelScriptClassName);
		if (!LocalAssert.IsNotNull(OutFixture.LevelScriptClass, *FString::Printf(TEXT("%s should compile an AS LevelScriptActor parent"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsTrue(
				OutFixture.LevelScriptClass->IsChildOf(ALevelScriptActor::StaticClass()),
				*FString::Printf(TEXT("%s LevelScript parent should derive from ALevelScriptActor"), Context)))
		{
			return false;
		}

		OutFixture.InitialLevelScriptASClass = Cast<UASClass>(OutFixture.LevelScriptClass);
		if (!LocalAssert.IsNotNull(OutFixture.InitialLevelScriptASClass, *FString::Printf(TEXT("%s LevelScript parent should be a UASClass"), Context)))
		{
			return false;
		}

		AngelscriptPIETestUtils::FScopedLevelScriptActorClassOverride LevelScriptOverride(OutFixture.LevelScriptClass);
		OutFixture.EditorWorld = AngelscriptPIETestUtils::CreateTransientEmptyMap(Test, Context);
		if (!LocalAssert.IsNotNull(OutFixture.EditorWorld, *FString::Printf(TEXT("%s should create a transient editor map"), Context)))
		{
			return false;
		}

		OutFixture.LevelBlueprint = AngelscriptPIETestUtils::CreateAndCompileLevelBlueprint(
			Test,
			OutFixture.EditorWorld,
			OutFixture.LevelScriptClass,
			Context);
		if (!LocalAssert.IsNotNull(OutFixture.LevelBlueprint, *FString::Printf(TEXT("%s should create a Level Blueprint"), Context)))
		{
			return false;
		}

		return AngelscriptPIETestUtils::BuildListenServerPIERequest(
			Test,
			OutFixture.GameModeClass,
			TwoPlayerClientWorldCount,
			OutFixture.RequestParams);
	}

	static bool RebuildListenServerPIERequest(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName GameModeClassName,
		const TCHAR* Context,
		FRequestPlaySessionParams& OutRequestParams)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UClass* ReloadedGameModeClass = FindGeneratedClass(&Engine, GameModeClassName);
		if (!LocalAssert.IsNotNull(ReloadedGameModeClass, *FString::Printf(TEXT("%s should resolve current GameMode class"), Context)))
		{
			return false;
		}

		OutRequestParams = FRequestPlaySessionParams();
		return AngelscriptPIETestUtils::BuildListenServerPIERequest(
			Test,
			ReloadedGameModeClass,
			TwoPlayerClientWorldCount,
			OutRequestParams);
	}

	static bool RecompileLevelBlueprint(FAutomationTestBase& Test, ULevelScriptBlueprint* LevelBlueprint, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(LevelBlueprint, *FString::Printf(TEXT("%s should have a Level Blueprint to compile"), Context)))
		{
			return false;
		}

		FKismetEditorUtilities::CompileBlueprint(LevelBlueprint);
		return LocalAssert.IsNotNull(LevelBlueprint->GeneratedClass.Get(), *FString::Printf(TEXT("%s should expose a generated Level Blueprint class"), Context));
	}

	static bool LevelBlueprintParentChainResolvesTo(
		FAutomationTestBase& Test,
		UClass* LevelBlueprintClass,
		UClass* ExpectedMostUpToDateClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UASClass* ASParent = UASClass::GetFirstASClass(LevelBlueprintClass);
		if (!LocalAssert.IsNotNull(ASParent, *FString::Printf(TEXT("%s should resolve an AS parent through the Level Blueprint parent chain"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedMostUpToDateClass,
			ASParent->GetMostUpToDateClass(),
			*FString::Printf(TEXT("%s should resolve the expected most up-to-date AS parent"), Context));
	}

	static ALevelScriptActor* GetLevelScriptActor(FAutomationTestBase& Test, UWorld* World, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(World, *FString::Printf(TEXT("%s should have a PIE world"), Context)))
		{
			return nullptr;
		}

		if (!LocalAssert.IsNotNull(World->PersistentLevel.Get(), *FString::Printf(TEXT("%s PIE world should have a persistent level"), Context)))
		{
			return nullptr;
		}

		ALevelScriptActor* LevelScriptActor = World->PersistentLevel->GetLevelScriptActor();
		return LocalAssert.IsNotNull(LevelScriptActor, *FString::Printf(TEXT("%s should expose a PIE LevelScriptActor"), Context))
			? LevelScriptActor
			: nullptr;
	}

	static bool InvokeGetValue(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UClass* OwnerClass,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Object, *FString::Printf(TEXT("%s should have a target object"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsNotNull(OwnerClass, *FString::Printf(TEXT("%s should have an owner class"), Context)))
		{
			return false;
		}

		UFunction* GetValueFunction = FindGeneratedFunction(OwnerClass, TEXT("GetValue"));
		if (!LocalAssert.IsNotNull(GetValueFunction, *FString::Printf(TEXT("%s should expose GetValue"), Context)))
		{
			return false;
		}

		int32 ActualValue = 0;
		if (!LocalAssert.IsTrue(
				ExecuteGeneratedIntEventOnGameThread(&Engine, Object, GetValueFunction, ActualValue),
				*FString::Printf(TEXT("%s should execute GetValue"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedValue,
			ActualValue,
			*FString::Printf(TEXT("%s should observe the expected GetValue result"), Context));
	}

	static bool ReadIntProperty(
		FAutomationTestBase& Test,
		UObject* Object,
		const FName PropertyName,
		int32& OutValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bRead = AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, PropertyName, OutValue);
		return LocalAssert.IsTrue(bRead, *FString::Printf(TEXT("%s should read property '%s'"), Context, *PropertyName.ToString()));
	}

	static bool AssertNetworkPIEWorldsUseClasses(
		FAutomationTestBase& Test,
		UClass* GameModeClass,
		UClass* LevelScriptClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const AngelscriptPIETestUtils::FNetworkPIEWorlds Worlds = AngelscriptPIETestUtils::FindNetworkPIEWorlds();
		if (!LocalAssert.IsNotNull(Worlds.ServerWorld, *FString::Printf(TEXT("%s should expose a listen-server world"), Context)))
		{
			return false;
		}

		if (!LocalAssert.AreEqual(TwoPlayerClientWorldCount, Worlds.ClientWorlds.Num(), *FString::Printf(TEXT("%s should expose one client world"), Context)))
		{
			return false;
		}

		if (!LocalAssert.AreEqual(NM_ListenServer, Worlds.ServerWorld->GetNetMode(), *FString::Printf(TEXT("%s server should run as listen server"), Context)))
		{
			return false;
		}

		UNetDriver* ServerNetDriver = Worlds.ServerWorld->GetNetDriver();
		if (!LocalAssert.IsNotNull(ServerNetDriver, *FString::Printf(TEXT("%s server world should have a net driver"), Context)) ||
			!LocalAssert.IsTrue(ServerNetDriver->IsServer(), *FString::Printf(TEXT("%s server net driver should be authoritative"), Context)) ||
			!LocalAssert.AreEqual(TwoPlayerClientWorldCount, ServerNetDriver->ClientConnections.Num(), *FString::Printf(TEXT("%s server should have one client connection"), Context)) ||
			!LocalAssert.IsTrue(AngelscriptPIETestUtils::AreClientConnectionsReady(Worlds.ServerWorld, TwoPlayerClientWorldCount), *FString::Printf(TEXT("%s server client connection should be ready"), Context)) ||
			!LocalAssert.IsTrue(AngelscriptPIETestUtils::HasExpectedGameMode(Worlds.ServerWorld, GameModeClass), *FString::Printf(TEXT("%s server should use the expected AS GameMode"), Context)) ||
			!LocalAssert.IsTrue(AngelscriptPIETestUtils::HasExpectedLevelScriptActor(Worlds.ServerWorld, LevelScriptClass), *FString::Printf(TEXT("%s server LevelScriptActor should inherit from the AS parent"), Context)))
		{
			return false;
		}

		UWorld* ClientWorld = Worlds.ClientWorlds[0];
		if (!LocalAssert.IsNotNull(ClientWorld, *FString::Printf(TEXT("%s should expose a client PIE world"), Context)) ||
			!LocalAssert.AreEqual(EWorldType::PIE, ClientWorld->WorldType, *FString::Printf(TEXT("%s client world type should be PIE"), Context)) ||
			!LocalAssert.AreEqual(NM_Client, ClientWorld->GetNetMode(), *FString::Printf(TEXT("%s client should run as network client"), Context)) ||
			!LocalAssert.IsNotNull(ClientWorld->GetGameInstance(), *FString::Printf(TEXT("%s client should have a GameInstance"), Context)) ||
			!LocalAssert.IsNotNull(ClientWorld->GetGameState(), *FString::Printf(TEXT("%s client should receive a GameState"), Context)) ||
			!LocalAssert.IsTrue(AngelscriptPIETestUtils::HasExpectedLevelScriptActor(ClientWorld, LevelScriptClass), *FString::Printf(TEXT("%s client LevelScriptActor should inherit from the AS parent"), Context)))
		{
			return false;
		}

		UNetDriver* ClientNetDriver = ClientWorld->GetNetDriver();
		return LocalAssert.IsNotNull(ClientNetDriver, *FString::Printf(TEXT("%s client world should have a net driver"), Context)) &&
			LocalAssert.IsFalse(ClientNetDriver->IsServer(), *FString::Printf(TEXT("%s client net driver should not be authoritative"), Context));
	}

	bool AssertServerAndClientGetValue(UClass* LevelScriptClass, const int32 ExpectedValue, const TCHAR* Context)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		const AngelscriptPIETestUtils::FNetworkPIEWorlds Worlds = AngelscriptPIETestUtils::FindNetworkPIEWorlds();

		ALevelScriptActor* ServerLevelScriptActor = GetLevelScriptActor(*TestRunner, Worlds.ServerWorld, *FString::Printf(TEXT("%s server"), Context));
		if (!ServerLevelScriptActor)
		{
			return false;
		}

		if (!InvokeGetValue(*TestRunner, Engine, ServerLevelScriptActor, LevelScriptClass, ExpectedValue, *FString::Printf(TEXT("%s server"), Context)))
		{
			return false;
		}

		if (Worlds.ClientWorlds.Num() != TwoPlayerClientWorldCount)
		{
			TestRunner->AddError(FString::Printf(TEXT("%s should expose one client world for GetValue"), Context));
			return false;
		}

		ALevelScriptActor* ClientLevelScriptActor = GetLevelScriptActor(*TestRunner, Worlds.ClientWorlds[0], *FString::Printf(TEXT("%s client"), Context));
		return ClientLevelScriptActor != nullptr &&
			InvokeGetValue(*TestRunner, Engine, ClientLevelScriptActor, LevelScriptClass, ExpectedValue, *FString::Printf(TEXT("%s client"), Context));
	}

	bool AssertServerAndClientLackProperty(const FName PropertyName, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		const AngelscriptPIETestUtils::FNetworkPIEWorlds Worlds = AngelscriptPIETestUtils::FindNetworkPIEWorlds();
		ALevelScriptActor* ServerLevelScriptActor = GetLevelScriptActor(*TestRunner, Worlds.ServerWorld, *FString::Printf(TEXT("%s server"), Context));
		ALevelScriptActor* ClientLevelScriptActor = Worlds.ClientWorlds.Num() > 0
			? GetLevelScriptActor(*TestRunner, Worlds.ClientWorlds[0], *FString::Printf(TEXT("%s client"), Context))
			: nullptr;

		if (!ServerLevelScriptActor || !ClientLevelScriptActor)
		{
			return false;
		}

		return LocalAssert.IsNull(
				FindFProperty<FIntProperty>(ServerLevelScriptActor->GetClass(), PropertyName),
				*FString::Printf(TEXT("%s server should not expose deferred property '%s'"), Context, *PropertyName.ToString())) &&
			LocalAssert.IsNull(
				FindFProperty<FIntProperty>(ClientLevelScriptActor->GetClass(), PropertyName),
				*FString::Printf(TEXT("%s client should not expose deferred property '%s'"), Context, *PropertyName.ToString()));
	}

	bool AssertServerAndClientPropertyValue(const FName PropertyName, const int32 ExpectedValue, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		const AngelscriptPIETestUtils::FNetworkPIEWorlds Worlds = AngelscriptPIETestUtils::FindNetworkPIEWorlds();
		ALevelScriptActor* ServerLevelScriptActor = GetLevelScriptActor(*TestRunner, Worlds.ServerWorld, *FString::Printf(TEXT("%s server"), Context));
		ALevelScriptActor* ClientLevelScriptActor = Worlds.ClientWorlds.Num() > 0
			? GetLevelScriptActor(*TestRunner, Worlds.ClientWorlds[0], *FString::Printf(TEXT("%s client"), Context))
			: nullptr;

		if (!ServerLevelScriptActor || !ClientLevelScriptActor)
		{
			return false;
		}

		int32 ServerValue = 0;
		int32 ClientValue = 0;
		return ReadIntProperty(*TestRunner, ServerLevelScriptActor, PropertyName, ServerValue, *FString::Printf(TEXT("%s server"), Context)) &&
			ReadIntProperty(*TestRunner, ClientLevelScriptActor, PropertyName, ClientValue, *FString::Printf(TEXT("%s client"), Context)) &&
			LocalAssert.AreEqual(ExpectedValue, ServerValue, *FString::Printf(TEXT("%s server property '%s' should match"), Context, *PropertyName.ToString())) &&
			LocalAssert.AreEqual(ExpectedValue, ClientValue, *FString::Printf(TEXT("%s client property '%s' should match"), Context, *PropertyName.ToString()));
	}

	void RegisterCleanup(const FName ModuleName)
	{
		TestCommandBuilder.CleanUpWith(TEXT("End HotReload MultiplayerPIE cleanup"), []()
		{
			AngelscriptPIETestUtils::EndPIE();
		});

		TestCommandBuilder.CleanUpWith(TEXT("Discard HotReload MultiplayerPIE AS module"), [ModuleName]()
		{
			DiscardModule(ModuleName);
		});
	}

	void QueuePIEStart(TFunction<FRequestPlaySessionParams()> RequestProvider)
	{
		AddCommand(MakeShared<FStartPIEFromRequestProvider>(MoveTemp(RequestProvider)));
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

	TEST_METHOD(SoftReloadDuringTwoPlayerPIEUpdatesServerAndClientLevelScripts)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("HotReload MultiplayerPIE soft test must start with no active PIE session")));

		AddExpectedNetworkWarnings();
		RegisterCleanup(SoftReloadModuleName);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadMultiplayerPIESoftGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadMultiplayerPIESoftLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 100;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadMultiplayerPIESoftGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadMultiplayerPIESoftLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 200;
				}
			}
			)AS");

		FMultiplayerPIEFixture Fixture;
		ASSERT_THAT(IsTrue(PrepareMultiplayerPIEFixture(
			*TestRunner,
			Engine,
			SoftReloadModuleName,
			SoftReloadFilename,
			ScriptV1,
			SoftReloadGameModeClassName,
			SoftReloadLevelScriptClassName,
			TEXT("HotReload MultiplayerPIE soft"),
			Fixture)));

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(Fixture.RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload MultiplayerPIE soft worlds"), []()
			{
				return AngelscriptPIETestUtils::HasExpectedNetworkPIEWorlds(TwoPlayerClientWorldCount);
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload MultiplayerPIE soft baseline"), [this, Fixture]()
			{
				ASSERT_THAT(IsTrue(AssertNetworkPIEWorldsUseClasses(
					*TestRunner,
					Fixture.GameModeClass,
					Fixture.LevelScriptClass,
					TEXT("HotReload MultiplayerPIE soft baseline"))));
				ASSERT_THAT(IsTrue(AssertServerAndClientGetValue(
					Fixture.LevelScriptClass,
					100,
					TEXT("HotReload MultiplayerPIE soft baseline"))));
			})
			.Then(TEXT("Soft reload HotReload MultiplayerPIE body while PIE is running"), [this, Fixture, ScriptV2]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftReloadModuleName, SoftReloadFilename, ScriptV2, ReloadResult),
					TEXT("HotReload MultiplayerPIE soft should compile the body-only reload during PIE")));
				ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("HotReload MultiplayerPIE soft should stay on the pure soft reload path during PIE")));

				UClass* ReloadedLevelScriptClass = FindGeneratedClass(&Engine, SoftReloadLevelScriptClassName);
				ASSERT_THAT(IsNotNull(ReloadedLevelScriptClass, TEXT("HotReload MultiplayerPIE soft should resolve LevelScript class after reload")));
				ASSERT_THAT(AreEqual(Fixture.LevelScriptClass, ReloadedLevelScriptClass, TEXT("HotReload MultiplayerPIE soft should keep LevelScript UClass identity")));
				ASSERT_THAT(IsTrue(AssertNetworkPIEWorldsUseClasses(
					*TestRunner,
					Fixture.GameModeClass,
					ReloadedLevelScriptClass,
					TEXT("HotReload MultiplayerPIE soft after reload"))));
				ASSERT_THAT(IsTrue(AssertServerAndClientGetValue(
					ReloadedLevelScriptClass,
					200,
					TEXT("HotReload MultiplayerPIE soft after reload"))));
			})
			.Then(TEXT("End HotReload MultiplayerPIE soft session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload MultiplayerPIE soft shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}

	TEST_METHOD(SuggestedFullReloadDuringTwoPlayerPIEDefersShapeToNextSession)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("HotReload MultiplayerPIE suggested test must start with no active PIE session")));

		AddExpectedNetworkWarnings();
		RegisterCleanup(SuggestedReloadModuleName);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadMultiplayerPIESuggestedGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadMultiplayerPIESuggestedLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UPROPERTY()
				int ExistingValue = 10;

				UFUNCTION()
				int GetValue()
				{
					return ExistingValue + 1;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadMultiplayerPIESuggestedGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadMultiplayerPIESuggestedLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UPROPERTY()
				int ExistingValue = 10;

				UPROPERTY()
				int AddedValue = 40;

				UFUNCTION()
				int GetValue()
				{
					return ExistingValue + 2;
				}
			}
			)AS");

		TSharedRef<FMultiplayerPIEFixture> Fixture = MakeShared<FMultiplayerPIEFixture>();
		ASSERT_THAT(IsTrue(PrepareMultiplayerPIEFixture(
			*TestRunner,
			Engine,
			SuggestedReloadModuleName,
			SuggestedReloadFilename,
			ScriptV1,
			SuggestedReloadGameModeClassName,
			SuggestedReloadLevelScriptClassName,
			TEXT("HotReload MultiplayerPIE suggested"),
			Fixture.Get())));

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		QueuePIEStart([Fixture]()
		{
			return Fixture->RequestParams;
		});
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload MultiplayerPIE suggested worlds"), []()
			{
				return AngelscriptPIETestUtils::HasExpectedNetworkPIEWorlds(TwoPlayerClientWorldCount);
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload MultiplayerPIE suggested baseline"), [this, Fixture]()
			{
				ASSERT_THAT(IsTrue(AssertNetworkPIEWorldsUseClasses(
					*TestRunner,
					Fixture->GameModeClass,
					Fixture->LevelScriptClass,
					TEXT("HotReload MultiplayerPIE suggested baseline"))));
				ASSERT_THAT(IsTrue(AssertServerAndClientGetValue(
					Fixture->LevelScriptClass,
					11,
					TEXT("HotReload MultiplayerPIE suggested baseline"))));
			})
			.Then(TEXT("Apply suggested full reload while MultiplayerPIE is running"), [this, Fixture, ScriptV2]()
			{
				TestRunner->AddExpectedErrorPlain(TEXT("Performing a Soft Reload during PIE"), EAutomationExpectedErrorFlags::Contains, 0);

				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SuggestedReloadModuleName, SuggestedReloadFilename, ScriptV2, ReloadResult),
					TEXT("HotReload MultiplayerPIE suggested should compile the suggested full reload during PIE")));
				ASSERT_THAT(AreEqual(ECompileResult::PartiallyHandled, ReloadResult, TEXT("HotReload MultiplayerPIE suggested should report the deferred full reload path during PIE")));

				Fixture->LevelScriptClass = FindGeneratedClass(&Engine, SuggestedReloadLevelScriptClassName);
				ASSERT_THAT(IsNotNull(Fixture->LevelScriptClass, TEXT("HotReload MultiplayerPIE suggested should resolve LevelScript after soft reload")));
				ASSERT_THAT(IsTrue(AssertNetworkPIEWorldsUseClasses(
					*TestRunner,
					Fixture->GameModeClass,
					Fixture->LevelScriptClass,
					TEXT("HotReload MultiplayerPIE suggested after soft reload"))));
				ASSERT_THAT(IsTrue(AssertServerAndClientGetValue(
					Fixture->LevelScriptClass,
					12,
					TEXT("HotReload MultiplayerPIE suggested after soft reload"))));
				ASSERT_THAT(IsTrue(AssertServerAndClientLackProperty(
					TEXT("AddedValue"),
					TEXT("HotReload MultiplayerPIE suggested after soft reload"))));
			})
			.Then(TEXT("End HotReload MultiplayerPIE suggested first session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload MultiplayerPIE suggested first shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Apply full reload after HotReload MultiplayerPIE suggested session"), [this, Fixture, ScriptV2]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult FullReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::FullReload, SuggestedReloadModuleName, SuggestedReloadFilename, ScriptV2, FullReloadResult),
					TEXT("HotReload MultiplayerPIE suggested should accept a full reload after PIE ends")));
				ASSERT_THAT(IsTrue(IsHandledReloadResult(FullReloadResult), TEXT("HotReload MultiplayerPIE suggested full reload after PIE should be handled")));

				UClass* ReloadedLevelScriptClass = FindGeneratedClass(&Engine, SuggestedReloadLevelScriptClassName);
				ASSERT_THAT(IsNotNull(ReloadedLevelScriptClass, TEXT("HotReload MultiplayerPIE suggested should resolve LevelScript after full reload")));
				ASSERT_THAT(IsTrue(ReloadedLevelScriptClass != Fixture->LevelScriptClass, TEXT("HotReload MultiplayerPIE suggested full reload should replace LevelScript class")));
				ASSERT_THAT(AreEqual(ReloadedLevelScriptClass, Fixture->InitialLevelScriptASClass->GetMostUpToDateClass(), TEXT("HotReload MultiplayerPIE suggested initial AS class should point at the full-reload version")));
				Fixture->LevelScriptClass = ReloadedLevelScriptClass;

				ASSERT_THAT(IsTrue(RecompileLevelBlueprint(*TestRunner, Fixture->LevelBlueprint, TEXT("HotReload MultiplayerPIE suggested after full reload"))));
				ASSERT_THAT(IsTrue(LevelBlueprintParentChainResolvesTo(
					*TestRunner,
					Fixture->LevelBlueprint->GeneratedClass.Get(),
					ReloadedLevelScriptClass,
					TEXT("HotReload MultiplayerPIE suggested after full reload"))));

				ASSERT_THAT(IsTrue(RebuildListenServerPIERequest(
					*TestRunner,
					Engine,
					SuggestedReloadGameModeClassName,
					TEXT("HotReload MultiplayerPIE suggested before second session"),
					Fixture->RequestParams)));
			})
			;

		QueuePIEStart([Fixture]()
		{
			return Fixture->RequestParams;
		});
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload MultiplayerPIE suggested second worlds"), []()
			{
				return AngelscriptPIETestUtils::HasExpectedNetworkPIEWorlds(TwoPlayerClientWorldCount);
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload MultiplayerPIE suggested second session"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				UClass* ReloadedGameModeClass = FindGeneratedClass(&Engine, SuggestedReloadGameModeClassName);
				ASSERT_THAT(IsNotNull(ReloadedGameModeClass, TEXT("HotReload MultiplayerPIE suggested second session should resolve GameMode")));
				ASSERT_THAT(IsTrue(AssertNetworkPIEWorldsUseClasses(
					*TestRunner,
					ReloadedGameModeClass,
					Fixture->LevelScriptClass,
					TEXT("HotReload MultiplayerPIE suggested second session"))));
				ASSERT_THAT(IsTrue(AssertServerAndClientGetValue(
					Fixture->LevelScriptClass,
					12,
					TEXT("HotReload MultiplayerPIE suggested second session"))));
				ASSERT_THAT(IsTrue(AssertServerAndClientPropertyValue(
					TEXT("AddedValue"),
					40,
					TEXT("HotReload MultiplayerPIE suggested second session"))));
			})
			.Then(TEXT("End HotReload MultiplayerPIE suggested second session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload MultiplayerPIE suggested second shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}
};

#endif
