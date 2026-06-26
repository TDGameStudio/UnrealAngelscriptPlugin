#pragma once

#include "CQTest.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace AngelscriptPIETestUtils
{
	inline UWorld* FindPIEWorld()
	{
		if (GEditor != nullptr)
		{
			if (FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
			{
				if (UWorld* World = PIEWorldContext->World())
				{
					return World;
				}
			}
		}

		if (GEngine == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				return Context.World();
			}
		}

		return nullptr;
	}

	inline bool IsPIEWorldAlive()
	{
		return FindPIEWorld() != nullptr || (GEditor != nullptr && GEditor->PlayWorld != nullptr);
	}

	struct FNetworkPIEWorlds
	{
		UWorld* ServerWorld = nullptr;
		TArray<UWorld*> ClientWorlds;
	};

	inline TArray<UWorld*> FindPIEWorlds()
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

	inline FNetworkPIEWorlds FindNetworkPIEWorlds()
	{
		FNetworkPIEWorlds Result;
		for (UWorld* World : FindPIEWorlds())
		{
			UNetDriver* NetDriver = World != nullptr ? World->GetNetDriver() : nullptr;
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

	inline bool AreClientConnectionsReady(UWorld* ServerWorld, int32 ExpectedClientCount)
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

	inline bool AreClientWorldsReady(const TArray<UWorld*>& ClientWorlds, int32 ExpectedClientCount)
	{
		if (ClientWorlds.Num() != ExpectedClientCount)
		{
			return false;
		}

		for (UWorld* ClientWorld : ClientWorlds)
		{
			if (!IsValid(ClientWorld) || ClientWorld->WorldType != EWorldType::PIE || ClientWorld->GetNetMode() != NM_Client)
			{
				return false;
			}

			UNetDriver* NetDriver = ClientWorld->GetNetDriver();
			if (!IsValid(NetDriver) || NetDriver->IsServer())
			{
				return false;
			}

			if (ClientWorld->PersistentLevel == nullptr || ClientWorld->GetGameInstance() == nullptr || ClientWorld->GetGameState() == nullptr)
			{
				return false;
			}
		}

		return true;
	}

	inline bool HasExpectedNetworkPIEWorlds(int32 ExpectedClientCount)
	{
		const FNetworkPIEWorlds Worlds = FindNetworkPIEWorlds();
		return Worlds.ServerWorld != nullptr
			&& Worlds.ClientWorlds.Num() == ExpectedClientCount
			&& AreClientConnectionsReady(Worlds.ServerWorld, ExpectedClientCount)
			&& AreClientWorldsReady(Worlds.ClientWorlds, ExpectedClientCount);
	}

	inline UWorld* CreateTransientEmptyMap(FAutomationTestBase& Test, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UWorld* EditorWorld = FAutomationEditorCommonUtils::CreateNewMap();
		if (!LocalAssert.IsNotNull(EditorWorld, *FString::Printf(TEXT("%s should create a transient empty editor map"), Context)))
		{
			return nullptr;
		}

		if (!LocalAssert.AreEqual(EWorldType::Editor, EditorWorld->WorldType, *FString::Printf(TEXT("%s editor map should be an editor world"), Context)))
		{
			return nullptr;
		}

		if (!LocalAssert.IsNotNull(EditorWorld->PersistentLevel.Get(), *FString::Printf(TEXT("%s editor map should have a persistent level"), Context)))
		{
			return nullptr;
		}

		return EditorWorld;
	}

	struct FScopedLevelScriptActorClassOverride
	{
		TSubclassOf<ALevelScriptActor> SavedClass;

		explicit FScopedLevelScriptActorClassOverride(UClass* NewClass)
		{
			if (GEngine != nullptr)
			{
				SavedClass = GEngine->LevelScriptActorClass;
				GEngine->LevelScriptActorClass = NewClass;
			}
		}

		~FScopedLevelScriptActorClassOverride()
		{
			if (GEngine != nullptr)
			{
				GEngine->LevelScriptActorClass = SavedClass;
			}
		}
	};

	inline ULevelScriptBlueprint* CreateAndCompileLevelBlueprint(
		FAutomationTestBase& Test,
		UWorld* EditorWorld,
		UClass* ExpectedParentClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(EditorWorld, *FString::Printf(TEXT("%s should have an editor world for Level Blueprint creation"), Context)))
		{
			return nullptr;
		}

		ULevel* PersistentLevel = EditorWorld->PersistentLevel.Get();
		if (!LocalAssert.IsNotNull(PersistentLevel, *FString::Printf(TEXT("%s editor world should have a persistent level for Level Blueprint creation"), Context)))
		{
			return nullptr;
		}

		ULevelScriptBlueprint* LevelBlueprint = PersistentLevel->GetLevelScriptBlueprint();
		if (!LocalAssert.IsNotNull(LevelBlueprint, *FString::Printf(TEXT("%s should create a LevelScriptBlueprint"), Context)))
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(LevelBlueprint);
		if (!LocalAssert.AreEqual(ExpectedParentClass, LevelBlueprint->ParentClass.Get(), *FString::Printf(TEXT("%s Level Blueprint should use the expected parent"), Context)))
		{
			return nullptr;
		}

		UClass* GeneratedClass = LevelBlueprint->GeneratedClass.Get();
		if (!LocalAssert.IsNotNull(GeneratedClass, *FString::Printf(TEXT("%s Level Blueprint should expose a generated class"), Context)))
		{
			return nullptr;
		}

		if (!LocalAssert.IsTrue(GeneratedClass->IsChildOf(ExpectedParentClass), *FString::Printf(TEXT("%s Level Blueprint generated class should inherit from the expected parent"), Context)))
		{
			return nullptr;
		}

		if (AActor* GeneratedDefaultActor = Cast<AActor>(GeneratedClass->GetDefaultObject()))
		{
			GeneratedDefaultActor->SetReplicates(false);
		}

		if (ALevelScriptActor* LevelScriptActor = PersistentLevel->GetLevelScriptActor())
		{
			LevelScriptActor->SetReplicates(false);
		}

		return LevelBlueprint;
	}

	inline bool BuildStandalonePIERequest(FAutomationTestBase& Test, UClass* GameModeClass, FRequestPlaySessionParams& OutParams)
	{
		FNoDiscardAsserter LocalAssert(Test);
		ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>();
		if (!LocalAssert.IsNotNull(PlaySettings, TEXT("Standalone PIE should create transient play settings")))
		{
			return false;
		}

		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
		PlaySettings->SetRunUnderOneProcess(true);
		PlaySettings->GameGetsMouseControl = false;

		FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

		OutParams.WorldType = EPlaySessionWorldType::PlayInEditor;
		OutParams.DestinationSlateViewport = LevelEditorModule.GetFirstActiveViewport();
		OutParams.EditorPlaySettings = PlaySettings;
		OutParams.GameModeOverride = GameModeClass;
		PlaySettings->AddToRoot();
		return true;
	}

	inline bool BuildListenServerPIERequest(
		FAutomationTestBase& Test,
		UClass* GameModeClass,
		int32 ClientWorldCount,
		FRequestPlaySessionParams& OutParams)
	{
		FNoDiscardAsserter LocalAssert(Test);
		ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>();
		if (!LocalAssert.IsNotNull(PlaySettings, TEXT("Listen server PIE should create transient play settings")))
		{
			return false;
		}

		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
		PlaySettings->SetPlayNumberOfClients(ClientWorldCount + 1);
		PlaySettings->SetRunUnderOneProcess(true);
		PlaySettings->bLaunchSeparateServer = false;
		PlaySettings->GameGetsMouseControl = false;

		FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

		OutParams.WorldType = EPlaySessionWorldType::PlayInEditor;
		OutParams.DestinationSlateViewport = LevelEditorModule.GetFirstActiveViewport();
		OutParams.EditorPlaySettings = PlaySettings;
		OutParams.GameModeOverride = GameModeClass;
		PlaySettings->AddToRoot();
		return true;
	}

	inline void StartPIE(const FRequestPlaySessionParams& RequestParams)
	{
		if (GEditor == nullptr)
		{
			return;
		}

		GEditor->RequestPlaySession(RequestParams);
		GEditor->StartQueuedPlaySessionRequest();
	}

	inline void EndPIE()
	{
		if (GUnrealEd != nullptr)
		{
			GUnrealEd->RequestEndPlayMap();
		}
	}

	inline bool HasExpectedGameMode(UWorld* World, UClass* ExpectedGameModeClass)
	{
		if (World == nullptr || ExpectedGameModeClass == nullptr)
		{
			return false;
		}

		AGameModeBase* GameMode = World->GetAuthGameMode();
		return GameMode != nullptr && GameMode->GetClass()->IsChildOf(ExpectedGameModeClass);
	}

	inline bool HasExpectedLevelScriptActor(UWorld* World, UClass* ExpectedLevelScriptParentClass)
	{
		if (World == nullptr || ExpectedLevelScriptParentClass == nullptr || World->PersistentLevel == nullptr)
		{
			return false;
		}

		ALevelScriptActor* LevelScriptActor = World->PersistentLevel->GetLevelScriptActor();
		return LevelScriptActor != nullptr && LevelScriptActor->GetClass()->IsChildOf(ExpectedLevelScriptParentClass);
	}
}

#endif
