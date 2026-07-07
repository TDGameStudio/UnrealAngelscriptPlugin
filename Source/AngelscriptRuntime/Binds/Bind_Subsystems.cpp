#include "CoreMinimal.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "Subsystems/Subsystem.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorSubsystem.h"
#endif

namespace
{
	template<typename TSubsystem>
	TSubclassOf<TSubsystem> MakeSubsystemClass(UClass* Class)
	{
		if (Class == nullptr || !Class->IsChildOf(TSubsystem::StaticClass()))
			return TSubclassOf<TSubsystem>();

		return TSubclassOf<TSubsystem>(Class);
	}
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Subsystems((int32)FAngelscriptBinds::EOrder::Late + 150, []
{
	{
		FAngelscriptBinds::FNamespace Namespace("USubsystemLibrary");

		FAngelscriptBinds::BindGlobalFunction("UObject GetEngineSubsystem(UClass Class)",
		[](UClass* Class) -> UObject*
		{
			const TSubclassOf<UEngineSubsystem> SubsystemClass = MakeSubsystemClass<UEngineSubsystem>(Class);
			return SubsystemClass ? USubsystemBlueprintLibrary::GetEngineSubsystem(SubsystemClass) : nullptr;
		});

		FAngelscriptBinds::BindGlobalFunction("UObject GetGameInstanceSubsystem(UClass Class)",
		[](UClass* Class) -> UObject*
		{
			const TSubclassOf<UGameInstanceSubsystem> SubsystemClass = MakeSubsystemClass<UGameInstanceSubsystem>(Class);
			return SubsystemClass ? USubsystemBlueprintLibrary::GetGameInstanceSubsystem(FAngelscriptEngine::TryGetCurrentWorldContextObject(), SubsystemClass) : nullptr;
		});

		FAngelscriptBinds::BindGlobalFunction("UObject GetLocalPlayerSubsystem(UClass Class)",
		[](UClass* Class) -> UObject*
		{
			const TSubclassOf<ULocalPlayerSubsystem> SubsystemClass = MakeSubsystemClass<ULocalPlayerSubsystem>(Class);
			return SubsystemClass ? USubsystemBlueprintLibrary::GetLocalPlayerSubsystem(FAngelscriptEngine::TryGetCurrentWorldContextObject(), SubsystemClass) : nullptr;
		});

		FAngelscriptBinds::BindGlobalFunction("UObject GetWorldSubsystem(UClass Class)",
		[](UClass* Class) -> UObject*
		{
			const TSubclassOf<UWorldSubsystem> SubsystemClass = MakeSubsystemClass<UWorldSubsystem>(Class);
			return SubsystemClass ? USubsystemBlueprintLibrary::GetWorldSubsystem(FAngelscriptEngine::TryGetCurrentWorldContextObject(), SubsystemClass) : nullptr;
		});

		FAngelscriptBinds::BindGlobalFunction("UObject GetLocalPlayerSubsystemFromLocalPlayer(ULocalPlayer LocalPlayer, UClass Class)",
		[](ULocalPlayer* LocalPlayer, UClass* Class) -> UObject*
		{
			const TSubclassOf<ULocalPlayerSubsystem> SubsystemClass = MakeSubsystemClass<ULocalPlayerSubsystem>(Class);
			return LocalPlayer != nullptr && SubsystemClass ? LocalPlayer->GetSubsystemBase(SubsystemClass) : nullptr;
		});

		FAngelscriptBinds::BindGlobalFunction("UObject GetLocalPlayerSubsystemFromPlayerController(APlayerController PlayerController, UClass Class)",
		[](APlayerController* PlayerController, UClass* Class) -> UObject*
		{
			const TSubclassOf<ULocalPlayerSubsystem> SubsystemClass = MakeSubsystemClass<ULocalPlayerSubsystem>(Class);
			return PlayerController != nullptr && SubsystemClass ? USubsystemBlueprintLibrary::GetLocalPlayerSubSystemFromPlayerController(PlayerController, SubsystemClass) : nullptr;
		});
	}
	// Bind easy ::Get() accessor functions for all subsystem classes
	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated))
			continue;
		if (!Class->HasAllClassFlags(CLASS_Native))
			continue;
		if (!Class->IsChildOf(USubsystem::StaticClass()))
			continue;

		auto Type = FAngelscriptType::GetByClass(Class);
		if (!Type.IsValid())
			continue;

		FString ClassName = Type->GetAngelscriptTypeName();
		FAngelscriptBinds::FNamespace ns(ClassName);

#if WITH_EDITOR
		if (Class->IsChildOf(UEditorSubsystem::StaticClass()))
		{
			if (FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext())
			{
				FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get()"),
					[]() -> UEditorSubsystem*
					{
						UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
						return GEditor->GetEditorSubsystemBase(SubsystemClass);
					}, Class);
			}
		}
		else
#endif
		if (Class->IsChildOf(UEngineSubsystem::StaticClass()))
		{
			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get()"),
			[]() -> UEngineSubsystem*
			{
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				return GEngine->GetEngineSubsystemBase(SubsystemClass);
			}, Class);
		}
		else if (Class->IsChildOf(UGameInstanceSubsystem::StaticClass()))
		{
			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get()"),
			[]() -> UGameInstanceSubsystem*
			{
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				UWorld* World = GEngine->GetWorldFromContextObject(FAngelscriptEngine::TryGetCurrentWorldContextObject(), EGetWorldErrorMode::ReturnNull);
				if (World == nullptr)
					return nullptr;
				const UGameInstance* GameInstance = World->GetGameInstance();
				if (GameInstance == nullptr)
					return nullptr;

				return GameInstance->GetSubsystemBase(SubsystemClass);
			}, Class);
		}
		else if (Class->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get()"),
			[]() -> UWorldSubsystem*
			{
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				UWorld* World = GEngine->GetWorldFromContextObject(FAngelscriptEngine::TryGetCurrentWorldContextObject(), EGetWorldErrorMode::ReturnNull);
				if (World == nullptr)
					return nullptr;

				return World->GetSubsystemBase(SubsystemClass);
			}, Class);
		}
		else if (Class->IsChildOf(ULocalPlayerSubsystem::StaticClass()))
		{
			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get(ULocalPlayer LocalPlayer)"),
			[](ULocalPlayer* LocalPlayer) -> ULocalPlayerSubsystem*
			{
				if (LocalPlayer == nullptr)
					return nullptr;
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				return LocalPlayer->GetSubsystemBase(SubsystemClass);
			}, Class);

			FAngelscriptBinds::BindGlobalFunction(ClassName + TEXT(" Get(APlayerController LocalPlayer)"),
			[](APlayerController* PlayerController) -> ULocalPlayerSubsystem*
			{
				if (PlayerController == nullptr)
					return nullptr;
				ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
				if (LocalPlayer == nullptr)
					return nullptr;
				UClass* SubsystemClass = FAngelscriptEngine::GetCurrentFunctionUserData<UClass>();
				return LocalPlayer->GetSubsystemBase(SubsystemClass);
			}, Class);
		}
	}
});
