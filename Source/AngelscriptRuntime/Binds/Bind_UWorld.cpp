#include "AngelscriptBinds.h"

#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"

#include "Bind_UWorld_Functions.h"

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
