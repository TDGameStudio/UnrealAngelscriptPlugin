#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "LevelEditor.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace TemplateMultiplayerPIETest
{
	constexpr double DefaultTimeoutSeconds = 10.0;
	constexpr int32 ExpectedClientWorldCount = 1;
	constexpr int32 ExpectedPIEInstanceCount = ExpectedClientWorldCount + 1;

	struct FMultiplayerPIEWorlds
	{
		UWorld* ServerWorld = nullptr;
		TArray<UWorld*> ClientWorlds;
	};

	TArray<UWorld*> FindPIEWorlds()
	{
		TArray<UWorld*> Worlds;
		if (GEngine == nullptr)
		{
			return Worlds;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType == EWorldType::PIE && IsValid(World))
			{
				Worlds.Add(World);
			}
		}

		return Worlds;
	}

	FMultiplayerPIEWorlds FindNetworkPIEWorlds()
	{
		FMultiplayerPIEWorlds Result;
		for (UWorld* World : FindPIEWorlds())
		{
			UNetDriver* NetDriver = World->GetNetDriver();
			if (!IsValid(NetDriver))
			{
				continue;
			}

			if (NetDriver->IsServer())
			{
				if (Result.ServerWorld == nullptr)
				{
					Result.ServerWorld = World;
				}
			}
			else
			{
				Result.ClientWorlds.Add(World);
			}
		}

		return Result;
	}

	bool AreClientConnectionsReady(UWorld* ServerWorld, int32 ExpectedClientCount)
	{
		if (!IsValid(ServerWorld))
		{
			return false;
		}

		UNetDriver* NetDriver = ServerWorld->GetNetDriver();
		if (!IsValid(NetDriver) || !NetDriver->IsServer())
		{
			return false;
		}

		if (NetDriver->ClientConnections.Num() != ExpectedClientCount)
		{
			return false;
		}

		for (const UNetConnection* ClientConnection : NetDriver->ClientConnections)
		{
			if (ClientConnection == nullptr || ClientConnection->ViewTarget == nullptr)
			{
				return false;
			}
		}

		return true;
	}

	bool HasExpectedMultiplayerPIEWorlds()
	{
		const FMultiplayerPIEWorlds Worlds = FindNetworkPIEWorlds();
		return Worlds.ServerWorld != nullptr
			&& Worlds.ClientWorlds.Num() == ExpectedClientWorldCount
			&& AreClientConnectionsReady(Worlds.ServerWorld, ExpectedClientWorldCount);
	}

	bool IsAnyPIEWorldAlive()
	{
		return FindPIEWorlds().Num() > 0 || (GEditor != nullptr && GEditor->PlayWorld != nullptr);
	}

	UWorld* CreateTransientEmptyMap(FAutomationTestBase& Test)
	{
		UWorld* EditorWorld = FAutomationEditorCommonUtils::CreateNewMap();
		if (!Test.TestNotNull(TEXT("Template_MultiplayerPIE should create a transient empty editor map"), EditorWorld))
		{
			return nullptr;
		}

		Test.TestTrue(TEXT("Template_MultiplayerPIE editor map should be an editor world"), EditorWorld->WorldType == EWorldType::Editor);
		Test.TestNotNull(TEXT("Template_MultiplayerPIE editor map should have a persistent level"), EditorWorld->PersistentLevel.Get());
		return EditorWorld;
	}

	bool BuildMultiplayerPIERequest(FAutomationTestBase& Test, FRequestPlaySessionParams& OutParams)
	{
		ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>();
		if (!Test.TestNotNull(TEXT("Template_MultiplayerPIE should create transient play settings"), PlaySettings))
		{
			return false;
		}

		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
		PlaySettings->SetPlayNumberOfClients(ExpectedPIEInstanceCount);
		PlaySettings->SetRunUnderOneProcess(true);
		PlaySettings->bLaunchSeparateServer = false;
		PlaySettings->GameGetsMouseControl = false;

		FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

		OutParams.WorldType = EPlaySessionWorldType::PlayInEditor;
		OutParams.DestinationSlateViewport = LevelEditorModule.GetFirstActiveViewport();
		OutParams.EditorPlaySettings = PlaySettings;
		OutParams.GameModeOverride = AGameModeBase::StaticClass();

		PlaySettings->AddToRoot();
		return true;
	}

	void AssertMultiplayerPIEWorlds(FAutomationTestBase& Test)
	{
		const FMultiplayerPIEWorlds Worlds = FindNetworkPIEWorlds();
		UWorld* ServerWorld = Worlds.ServerWorld;
		if (!Test.TestNotNull(TEXT("Template_MultiplayerPIE should expose a server PIE world"), ServerWorld))
		{
			return;
		}

		Test.TestEqual(TEXT("Template_MultiplayerPIE should expose one client PIE world"), Worlds.ClientWorlds.Num(), ExpectedClientWorldCount);
		if (Worlds.ClientWorlds.Num() != ExpectedClientWorldCount)
		{
			return;
		}

		UWorld* ClientWorld = Worlds.ClientWorlds[0];
		Test.TestNotNull(TEXT("Template_MultiplayerPIE should expose a client PIE world"), ClientWorld);
		Test.TestTrue(TEXT("Template_MultiplayerPIE server world type should be PIE"), ServerWorld->WorldType == EWorldType::PIE);
		Test.TestTrue(TEXT("Template_MultiplayerPIE client world type should be PIE"), ClientWorld->WorldType == EWorldType::PIE);
		Test.TestNotNull(TEXT("Template_MultiplayerPIE server world should have a persistent level"), ServerWorld->PersistentLevel.Get());
		Test.TestNotNull(TEXT("Template_MultiplayerPIE client world should have a persistent level"), ClientWorld->PersistentLevel.Get());

		UNetDriver* ServerNetDriver = ServerWorld->GetNetDriver();
		UNetDriver* ClientNetDriver = ClientWorld->GetNetDriver();
		Test.TestNotNull(TEXT("Template_MultiplayerPIE server world should have a net driver"), ServerNetDriver);
		Test.TestNotNull(TEXT("Template_MultiplayerPIE client world should have a net driver"), ClientNetDriver);
		if (ServerNetDriver != nullptr && ClientNetDriver != nullptr)
		{
			Test.TestTrue(TEXT("Template_MultiplayerPIE server net driver should be authoritative"), ServerNetDriver->IsServer());
			Test.TestFalse(TEXT("Template_MultiplayerPIE client net driver should not be authoritative"), ClientNetDriver->IsServer());
			Test.TestTrue(TEXT("Template_MultiplayerPIE server should have one client connection"),
				AreClientConnectionsReady(ServerWorld, ExpectedClientWorldCount));
		}

		Test.TestTrue(TEXT("Template_MultiplayerPIE server should run as listen server"), ServerWorld->GetNetMode() == NM_ListenServer);
		Test.TestTrue(TEXT("Template_MultiplayerPIE client should run as network client"), ClientWorld->GetNetMode() == NM_Client);
	}

	class FWaitForMultiplayerPIEWorldsCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForMultiplayerPIEWorldsCommand(FAutomationTestBase& InTest, double InTimeoutSeconds = DefaultTimeoutSeconds)
			: Test(InTest)
			, TimeoutSeconds(InTimeoutSeconds)
		{
		}

		virtual bool Update() override
		{
			if (HasExpectedMultiplayerPIEWorlds())
			{
				AssertMultiplayerPIEWorlds(Test);
				return true;
			}

			if (GetCurrentRunTime() > TimeoutSeconds)
			{
				const FMultiplayerPIEWorlds Worlds = FindNetworkPIEWorlds();
				Test.AddError(FString::Printf(
					TEXT("Template_MultiplayerPIE timed out after %.1f seconds waiting for one listen server and one client PIE world. ServerWorld=%s ClientWorlds=%d."),
					TimeoutSeconds,
					Worlds.ServerWorld != nullptr ? TEXT("true") : TEXT("false"),
					Worlds.ClientWorlds.Num()));
				return true;
			}

			return false;
		}

	private:
		FAutomationTestBase& Test;
		double TimeoutSeconds;
	};

	class FWaitForPIEEndCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForPIEEndCommand(FAutomationTestBase& InTest, double InTimeoutSeconds = DefaultTimeoutSeconds)
			: Test(InTest)
			, TimeoutSeconds(InTimeoutSeconds)
		{
		}

		virtual bool Update() override
		{
			if (!IsAnyPIEWorldAlive())
			{
				return true;
			}

			if (GetCurrentRunTime() > TimeoutSeconds)
			{
				Test.AddError(FString::Printf(TEXT("Template_MultiplayerPIE timed out after %.1f seconds waiting for PIE shutdown."), TimeoutSeconds));
				return true;
			}

			return false;
		}

	private:
		FAutomationTestBase& Test;
		double TimeoutSeconds;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTemplateMultiplayerPIETest,
	"Angelscript.Template.MultiplayerPIE.EmptyMap_ListenServerAndClient_StartPIE_EndPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTemplateMultiplayerPIETest::RunTest(const FString& Parameters)
{
	using namespace TemplateMultiplayerPIETest;

	if (IsAnyPIEWorldAlive())
	{
		AddError(TEXT("Template_MultiplayerPIE must start from editor mode with no existing PIE session."));
		return false;
	}

	if (CreateTransientEmptyMap(*this) == nullptr)
	{
		return false;
	}

	FRequestPlaySessionParams RequestParams;
	if (!BuildMultiplayerPIERequest(*this, RequestParams))
	{
		return false;
	}

	AddExpectedErrorPlain(TEXT("FNetGUIDCache::SupportsObject: Level /Temp/"), EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedErrorPlain(TEXT("RegisterNetGUID_Client: Guid with pathname. FullNetGUIDPath: [16]WorldSettings"), EAutomationExpectedErrorFlags::Contains, -1);

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(RequestParams));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMultiplayerPIEWorldsCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]() -> bool
	{
		TemplateMultiplayerPIETest::AssertMultiplayerPIEWorlds(*this);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(*this));

	return true;
}

#endif
