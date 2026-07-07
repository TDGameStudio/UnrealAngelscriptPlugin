#include "Engine/Engine.h"
#include "Engine/World.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_WorldType((int32)FAngelscriptBinds::EOrder::Early, []
{
	auto WorldType_ = FAngelscriptBinds::Enum("EWorldType");
	WorldType_["None"]			= EWorldType::None;
	WorldType_["Game"]			= EWorldType::Game;
	WorldType_["Editor"]		= EWorldType::Editor;
	WorldType_["PIE"]			= EWorldType::PIE;
	WorldType_["EditorPreview"]	= EWorldType::EditorPreview;
	WorldType_["GamePreview"]	= EWorldType::GamePreview;
	WorldType_["GameRPC"]		= EWorldType::GameRPC;
	WorldType_["Inactive"]		= EWorldType::Inactive;
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_NetMode((int32)FAngelscriptBinds::EOrder::Early, []
{
	auto NetMode_ = FAngelscriptBinds::Enum("ENetMode");
	NetMode_["NM_Client"] = ENetMode::NM_Client;
	NetMode_["NM_DedicatedServer"] = ENetMode::NM_DedicatedServer;
	NetMode_["NM_ListenServer"] = ENetMode::NM_ListenServer;
	NetMode_["NM_Standalone"] = ENetMode::NM_Standalone;
	NetMode_["NM_MAX"] = ENetMode::NM_MAX;
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_World((int32)FAngelscriptBinds::EOrder::Late, []
{
	FAngelscriptBinds::BindGlobalFunction("UObject __WorldContext()",
	[]() -> UObject*
	{
		return FAngelscriptEngine::TryGetCurrentWorldContextObject();
	});
	FAngelscriptBinds::BindGlobalFunction("UWorld GetCurrentWorld()",
	[]() -> UWorld*
	{
		return GEngine->GetWorldFromContextObject(FAngelscriptEngine::TryGetCurrentWorldContextObject(), EGetWorldErrorMode::ReturnNull);
	});

	auto UWorld_ = FAngelscriptBinds::ExistingClass("UWorld");
	UWorld_.Method("bool IsGameWorld() const", METHOD_TRIVIAL(UWorld, IsGameWorld));
	UWorld_.Method("bool IsEditorWorld() const", METHOD_TRIVIAL(UWorld, IsEditorWorld));
	UWorld_.Method("bool IsPreviewWorld() const", METHOD_TRIVIAL(UWorld, IsPreviewWorld));

	UWorld_.Method("bool ServerTravel(const FString& FURL, bool bAbsolute, bool bShouldSkipGameNotify)", METHOD_TRIVIAL(UWorld, ServerTravel));

	UWorld_.Method("ENetMode GetNetMode() const", METHOD_TRIVIAL(UWorld, GetNetMode));

	UWorld_.Method("AGameStateBase GetGameState() const", [](UWorld* World) -> AGameStateBase*
	{
		return World->GetGameState();
	});
	
	UWorld_.Method("float64 GetTimeSeconds() const", METHOD_TRIVIAL(UWorld, GetTimeSeconds));
	UWorld_.Method("float64 GetUnpausedTimeSeconds() const", METHOD_TRIVIAL(UWorld, GetUnpausedTimeSeconds));
	UWorld_.Method("float64 GetRealTimeSeconds() const", METHOD_TRIVIAL(UWorld, GetRealTimeSeconds));
	UWorld_.Method("float64 GetAudioTimeSeconds() const", METHOD_TRIVIAL(UWorld, GetAudioTimeSeconds));
	UWorld_.Method("float32 GetDeltaSeconds() const", METHOD_TRIVIAL(UWorld, GetDeltaSeconds));
	UWorld_.Method("bool IsStartingUp() const", [](UWorld* World) -> bool
	{
		return World->bStartup;
	});
	UWorld_.Method("bool IsTearingDown() const", [](UWorld* World) -> bool
	{
		return World->bIsTearingDown;
	});
	UWorld_.Method("void SetGameInstance(UGameInstance NewGI)", METHOD_TRIVIAL(UWorld, SetGameInstance));
	UWorld_.Method("UGameInstance GetGameInstance() const", [](UWorld* World) -> UGameInstance*
	{
		return World->GetGameInstance();
	});
	UWorld_.Method("ALevelScriptActor GetLevelScriptActor() const", [](UWorld* World) -> ALevelScriptActor*
	{
		return World->GetLevelScriptActor();
	});
	UWorld_.Method("ULevel GetPersistentLevel() const", [](UWorld* World) -> ULevel*
	{
		return World->PersistentLevel;
	});

	UWorld_.Property("EWorldType WorldType", &UWorld::WorldType);

	FAngelscriptBinds::BindGlobalVariable("uint GFrameNumber", &GFrameNumber);

	auto ULevel_ = FAngelscriptBinds::ExistingClass("ULevel");
	ULevel_.Method("ALevelScriptActor GetLevelScriptActor() const", [](ULevel* Level) -> ALevelScriptActor*
	{
		return Level->LevelScriptActor;
	});

	ULevel_.Method("bool IsVisible() const", [](ULevel* Level) -> bool
	{
		return Level->bIsVisible;
	});

	ULevel_.Method("bool IsBeingRemoved() const", [](ULevel* Level) -> bool
	{
		return Level->bIsBeingRemoved;
	});

	ULevel_.Method("const TArray<AActor>& GetActors() const", [](ULevel* Level) -> const TArray<AActor*>&
	{
		return Level->Actors;
	});
});
