#include "Bind_UWorld.h"

#include "AngelscriptBinds.h"

#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"

/**
 * World, level, world-type, and network-mode binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | enum EWorldType;                                                                           | Declares the world-kind classification.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType::None;                                                                          | Represents an untyped world.                                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType::Game;                                                                          | Represents a game world.                                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType::Editor;                                                                        | Represents an editor world.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType::PIE;                                                                           | Represents a Play-In-Editor world.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType::EditorPreview;                                                                 | Represents an editor preview world.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType::GamePreview;                                                                   | Represents a game preview world.                                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType::GameRPC;                                                                       | Represents a minimal world used for game RPC handling.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType::Inactive;                                                                      | Represents an inactive world.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | enum ENetMode;                                                                             | Declares the world network execution mode.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ENetMode::NM_Client;                                                                       | Represents a network client.                                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ENetMode::NM_DedicatedServer;                                                              | Represents a dedicated server.                                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ENetMode::NM_ListenServer;                                                                 | Represents a listen server.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ENetMode::NM_Standalone;                                                                   | Represents a non-networked standalone world.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ENetMode::NM_MAX;                                                                          | Marks the upper bound of network-mode values.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject __WorldContext();                                                                  | Returns the implicit world-context object used by world-context calls.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UWorld GetCurrentWorld();                                                                  | Returns the current AngelScript execution world.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UWorld.IsGameWorld() const;                                                           | Returns whether this is a game world.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UWorld.IsEditorWorld() const;                                                         | Returns whether this is an editor world.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UWorld.IsPreviewWorld() const;                                                        | Returns whether this is a preview world.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UWorld.ServerTravel(const FString& FURL, bool bAbsolute,                              | Starts server travel to the supplied URL.                                                                            |
 * |     bool bShouldSkipGameNotify);                                                           | @param bAbsolute Treats FURL as absolute rather than relative when true.                                             |
 * |                                                                                            | @param bShouldSkipGameNotify Skips game-mode travel notification when true.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ENetMode UWorld.GetNetMode() const;                                                        | Returns the world network execution mode.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AGameStateBase UWorld.GetGameState() const;                                                | Returns the current game-state actor.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 UWorld.GetTimeSeconds() const;                                                     | Returns dilated gameplay time in seconds, excluding pause.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 UWorld.GetUnpausedTimeSeconds() const;                                             | Returns dilated world time in seconds, including pause.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 UWorld.GetRealTimeSeconds() const;                                                 | Returns real elapsed world time in seconds.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 UWorld.GetAudioTimeSeconds() const;                                                | Returns the audio clock time in seconds.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 UWorld.GetDeltaSeconds() const;                                                    | Returns the current world tick delta in seconds.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UWorld.IsStartingUp() const;                                                          | Returns whether world startup is in progress.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UWorld.IsTearingDown() const;                                                         | Returns whether world teardown is in progress.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UWorld.SetGameInstance(UGameInstance NewGI);                                          | Assigns the game instance associated with this world.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UGameInstance UWorld.GetGameInstance() const;                                              | Returns the game instance associated with this world.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ALevelScriptActor UWorld.GetLevelScriptActor() const;                                      | Returns the persistent level script actor.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ULevel UWorld.GetPersistentLevel() const;                                                  | Returns the world persistent level.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EWorldType UWorld.WorldType;                                                               | Exposes the world-kind classification.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint GFrameNumber;                                                                         | Exposes the global engine frame counter.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ALevelScriptActor ULevel.GetLevelScriptActor() const;                                      | Returns this level script actor.                                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool ULevel.IsVisible() const;                                                             | Returns whether the level is visible.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool ULevel.IsBeingRemoved() const;                                                        | Returns whether the level is being removed from its world.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const TArray<AActor>& ULevel.GetActors() const;                                            | Returns the level actor array; entries may be null.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindWorldType(FAngelscriptBinds& Binds)
	{
		auto WorldType_ = Binds.EnumForTarget("EWorldType");
		WorldType_["None"] = EWorldType::None;
		WorldType_["Game"] = EWorldType::Game;
		WorldType_["Editor"] = EWorldType::Editor;
		WorldType_["PIE"] = EWorldType::PIE;
		WorldType_["EditorPreview"] = EWorldType::EditorPreview;
		WorldType_["GamePreview"] = EWorldType::GamePreview;
		WorldType_["GameRPC"] = EWorldType::GameRPC;
		WorldType_["Inactive"] = EWorldType::Inactive;
	}

	void BindNetMode(FAngelscriptBinds& Binds)
	{
		auto NetMode_ = Binds.EnumForTarget("ENetMode");
		NetMode_["NM_Client"] = ENetMode::NM_Client;
		NetMode_["NM_DedicatedServer"] = ENetMode::NM_DedicatedServer;
		NetMode_["NM_ListenServer"] = ENetMode::NM_ListenServer;
		NetMode_["NM_Standalone"] = ENetMode::NM_Standalone;
		NetMode_["NM_MAX"] = ENetMode::NM_MAX;
	}

	void BindWorldFunctions(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionForTarget("UObject __WorldContext()", &FAngelscriptUWorldBinds::GetWorldContext);
		Binds.BindGlobalFunctionForTarget("UWorld GetCurrentWorld()", &FAngelscriptUWorldBinds::GetCurrentWorld);

		auto UWorld_ = Binds.ExistingClassForTarget("UWorld");
		UWorld_.Method("bool IsGameWorld() const", METHOD_TRIVIAL(UWorld, IsGameWorld));
		UWorld_.Method("bool IsEditorWorld() const", METHOD_TRIVIAL(UWorld, IsEditorWorld));
		UWorld_.Method("bool IsPreviewWorld() const", METHOD_TRIVIAL(UWorld, IsPreviewWorld));
		UWorld_.Method(
			"bool ServerTravel(const FString& FURL, bool bAbsolute, bool bShouldSkipGameNotify)",
			METHOD_TRIVIAL(UWorld, ServerTravel));
		UWorld_.Method("ENetMode GetNetMode() const", METHOD_TRIVIAL(UWorld, GetNetMode));
		UWorld_.Method("AGameStateBase GetGameState() const", &FAngelscriptUWorldBinds::GetGameState);
		UWorld_.Method("float64 GetTimeSeconds() const", METHOD_TRIVIAL(UWorld, GetTimeSeconds));
		UWorld_.Method("float64 GetUnpausedTimeSeconds() const", METHOD_TRIVIAL(UWorld, GetUnpausedTimeSeconds));
		UWorld_.Method("float64 GetRealTimeSeconds() const", METHOD_TRIVIAL(UWorld, GetRealTimeSeconds));
		UWorld_.Method("float64 GetAudioTimeSeconds() const", METHOD_TRIVIAL(UWorld, GetAudioTimeSeconds));
		UWorld_.Method("float32 GetDeltaSeconds() const", METHOD_TRIVIAL(UWorld, GetDeltaSeconds));
		UWorld_.Method("bool IsStartingUp() const", &FAngelscriptUWorldBinds::IsStartingUp);
		UWorld_.Method("bool IsTearingDown() const", &FAngelscriptUWorldBinds::IsTearingDown);
		UWorld_.Method("void SetGameInstance(UGameInstance NewGI)", METHOD_TRIVIAL(UWorld, SetGameInstance));
		UWorld_.Method("UGameInstance GetGameInstance() const", &FAngelscriptUWorldBinds::GetGameInstance);
		UWorld_.Method("ALevelScriptActor GetLevelScriptActor() const", &FAngelscriptUWorldBinds::GetWorldLevelScriptActor);
		UWorld_.Method("ULevel GetPersistentLevel() const", &FAngelscriptUWorldBinds::GetPersistentLevel);
		UWorld_.Property("EWorldType WorldType", &UWorld::WorldType);

		Binds.BindGlobalVariableForTarget("uint GFrameNumber", &GFrameNumber);

		auto ULevel_ = Binds.ExistingClassForTarget("ULevel");
		ULevel_.Method("ALevelScriptActor GetLevelScriptActor() const", &FAngelscriptUWorldBinds::GetLevelScriptActor);
		ULevel_.Method("bool IsVisible() const", &FAngelscriptUWorldBinds::IsLevelVisible);
		ULevel_.Method("bool IsBeingRemoved() const", &FAngelscriptUWorldBinds::IsLevelBeingRemoved);
		ULevel_.Method("const TArray<AActor>& GetActors() const", &FAngelscriptUWorldBinds::GetLevelActors);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_WorldType(
	TEXT("UWorld.WorldType"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindWorldType);

AS_FORCE_LINK const FAngelscriptBind Bind_NetMode(
	TEXT("UWorld.NetMode"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindNetMode);

AS_FORCE_LINK const FAngelscriptBind Bind_World(
	TEXT("UWorld.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindWorldFunctions);
