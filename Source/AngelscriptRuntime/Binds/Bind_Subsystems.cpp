#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "Bind_Subsystems_Functions.h"

#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/Subsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "EditorSubsystem.h"
#endif

namespace
{
	void BindSubsystems(FAngelscriptBinds& Binds)
	{
		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "USubsystemLibrary");
			Binds.BindGlobalFunctionForTarget(
				"UObject GetEngineSubsystem(UClass Class)",
				&FAngelscriptSubsystemsBinds::GetEngineSubsystem);
			Binds.BindGlobalFunctionForTarget(
				"UObject GetGameInstanceSubsystem(UClass Class)",
				&FAngelscriptSubsystemsBinds::GetGameInstanceSubsystem);
			Binds.BindGlobalFunctionForTarget(
				"UObject GetLocalPlayerSubsystem(UClass Class)",
				&FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystem);
			Binds.BindGlobalFunctionForTarget(
				"UObject GetWorldSubsystem(UClass Class)",
				&FAngelscriptSubsystemsBinds::GetWorldSubsystem);
			Binds.BindGlobalFunctionForTarget(
				"UObject GetLocalPlayerSubsystemFromLocalPlayer(ULocalPlayer LocalPlayer, UClass Class)",
				&FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystemFromLocalPlayer);
			Binds.BindGlobalFunctionForTarget(
				"UObject GetLocalPlayerSubsystemFromPlayerController(APlayerController PlayerController, UClass Class)",
				&FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystemFromPlayerController);
		}

		// Bind easy ::Get() accessors for every native subsystem type discovered by
		// reflection. The class pointer remains per-function user data, as before.
		for (UClass* Class : TObjectRange<UClass>())
		{
			if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated)
				|| !Class->HasAllClassFlags(CLASS_Native)
				|| !Class->IsChildOf(USubsystem::StaticClass()))
			{
				continue;
			}

			const TSharedPtr<FAngelscriptType> Type = FAngelscriptType::GetByClass(
				Binds.GetTargetTypeDatabase(),
				Class);
			if (!Type.IsValid())
			{
				continue;
			}

			const FString ClassName = Type->GetAngelscriptTypeName();
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), ClassName);

#if WITH_EDITOR
			if (Class->IsChildOf(UEditorSubsystem::StaticClass()))
			{
				if (Binds.GetTargetEngine().ShouldUseEditorScripts())
				{
					Binds.BindGlobalFunctionForTarget(
						ClassName + TEXT(" Get()"),
						&FAngelscriptSubsystemsBinds::GetEditorSubsystemForClass,
						Class);
				}
			}
			else
#endif
			if (Class->IsChildOf(UEngineSubsystem::StaticClass()))
			{
				Binds.BindGlobalFunctionForTarget(
					ClassName + TEXT(" Get()"),
					&FAngelscriptSubsystemsBinds::GetEngineSubsystemForClass,
					Class);
			}
			else if (Class->IsChildOf(UGameInstanceSubsystem::StaticClass()))
			{
				Binds.BindGlobalFunctionForTarget(
					ClassName + TEXT(" Get()"),
					&FAngelscriptSubsystemsBinds::GetGameInstanceSubsystemForClass,
					Class);
			}
			else if (Class->IsChildOf(UWorldSubsystem::StaticClass()))
			{
				Binds.BindGlobalFunctionForTarget(
					ClassName + TEXT(" Get()"),
					&FAngelscriptSubsystemsBinds::GetWorldSubsystemForClass,
					Class);
			}
			else if (Class->IsChildOf(ULocalPlayerSubsystem::StaticClass()))
			{
				Binds.BindGlobalFunctionForTarget(
					ClassName + TEXT(" Get(ULocalPlayer LocalPlayer)"),
					&FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystemForLocalPlayer,
					Class);
				Binds.BindGlobalFunctionForTarget(
					ClassName + TEXT(" Get(APlayerController LocalPlayer)"),
					&FAngelscriptSubsystemsBinds::GetLocalPlayerSubsystemForPlayerController,
					Class);
			}
		}
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_Subsystems(
	TEXT("Subsystems.PostReflection"),
	EAngelscriptBindPhase::PostReflectionBindings,
	&BindSubsystems);
