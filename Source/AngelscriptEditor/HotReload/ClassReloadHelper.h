#pragma once
#include "CoreMinimal.h"
#include "Editor.h"

#include "GameFramework/Volume.h"

#include "BlueprintActionDatabase.h"
#include "Kismet2/EnumEditorUtils.h"

#include "ComponentTypeRegistry.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"
#include "ClassReloadHelper.generated.h"

class UBlueprint;

UCLASS()
class UAngelscriptReferenceReplacementHelper : public UObject
{
	GENERATED_BODY()
public:

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
};

#if WITH_DEV_AUTOMATION_TESTS
struct FClassReloadHelperClassReloadTestHooks
{
	TFunction<void(UClass*)> RefreshClassActions;
	TFunction<void(UClass*)> InvalidateComponentClass;
};

struct FClassReloadHelperPostReloadTestHooks
{
	TFunction<void()> RefreshAllActions;
	TFunction<void()> InvalidateComponentRegistry;
	TFunction<void(UWorld*, const TCHAR*)> ExecCommand;
};

struct FClassReloadHelperPerformReinstanceTestHooks
{
	TFunction<bool()> EnterPerformReinstanceBody;
	TFunction<void()> NotifyCustomizationModuleChanged;
	TFunction<void(UEnum*)> RefreshAssetActions;
	TFunction<void()> AddActorFactory;
	TFunction<void()> BroadcastAllPlaceableAssetsChanged;
	TFunction<void()> BroadcastPlaceableItemFilteringChanged;
	TFunction<void(UBlueprint*)> QueueBlueprintForCompilation;
	TFunction<void()> FlushCompilationQueueAndReinstance;
};

struct FClassReloadHelperTestAccess
{
	static void SetClassReloadTestHooks(FClassReloadHelperClassReloadTestHooks InHooks);
	static void ResetClassReloadTestHooks();
	static bool HandleRefreshClassActions(UClass* Class);
	static bool HandleInvalidateComponentClass(UClass* Class);
	static void SetPostReloadTestHooks(FClassReloadHelperPostReloadTestHooks InHooks);
	static void ResetPostReloadTestHooks();
	static bool HandleRefreshAllActions();
	static bool HandleInvalidateComponentRegistry();
	static bool HandleExecCommand(UWorld* World, const TCHAR* Command);
	static void SetPerformReinstanceTestHooks(FClassReloadHelperPerformReinstanceTestHooks InHooks);
	static void ResetPerformReinstanceTestHooks();
};
#endif

struct FClassReloadHelper
{
	struct FReloadState
	{
		bool bRefreshAllActions = false;
		bool bReloadedVolume = false;

		TMap<UClass*, UClass*> ReloadClasses;
		TMap<UObject*, UObject*> ReloadAssets;
		TSet<UClass*> NewClasses;
		TSet<UEnum*> ReloadEnums;
		TSet<UEnum*> NewEnums;
		TMap<UScriptStruct*, UScriptStruct*> ReloadStructs;
		TMap<UDelegateFunction*, UDelegateFunction*> ReloadDelegates;
		TSet<UDelegateFunction*> NewDelegates;

		void PerformReinstance();
	};

	static FReloadState& ReloadState()
	{
		static FReloadState State;
		return State;
	}

	// Per-engine extension that subscribes to the (now engine-owned) reload hooks.
	// Pre-deglobalization the same wiring lived as direct AddLambda calls into
	// process-wide static delegates on FAngelscriptClassGenerator. After that
	// refactor (see refactor-as-runtime-deglobalize-completion / Section 4-6)
	// the hooks live on FAngelscriptEngineHooks (per-engine), so subscribers
	// must attach when each engine is created and detach when it is destroyed.
	// FClassReloadHelperExtension is the per-engine binder; FReloadState below
	// remains a single static for now (Editor only ever drives one engine
	// concurrently in practice — see Section 6 of the change for the deferred
	// per-engine partition).
	class FClassReloadHelperExtension : public IAngelscriptExtension
	{
	public:
		virtual void OnEngineAttached(FAngelscriptEngine& Engine) override
		{
			FAngelscriptEngineHooks& Hooks = Engine.GetHooks();

			AttachedHandles.Add(Hooks.GetOnStructReload().AddLambda(
			[](UScriptStruct* OldStruct, UScriptStruct* NewStruct)
			{
				ReloadState().ReloadStructs.Add(OldStruct, NewStruct);
				ReloadState().bRefreshAllActions = true;
			}));

			AttachedHandles.Add(Hooks.GetOnClassReload().AddLambda(
			[](UClass* OldClass, UClass* NewClass)
			{
				if (OldClass != nullptr)
					ReloadState().ReloadClasses.Add(OldClass, NewClass);
				else
					ReloadState().NewClasses.Add(NewClass);

				const bool bTouchesInterfaceReload =
					(OldClass != nullptr && (OldClass->HasAnyClassFlags(CLASS_Interface) || OldClass->Interfaces.Num() > 0))
					|| (NewClass != nullptr && (NewClass->HasAnyClassFlags(CLASS_Interface) || NewClass->Interfaces.Num() > 0));
				if (bTouchesInterfaceReload)
				{
					ReloadState().bRefreshAllActions = true;
				}

				bool bRefreshedAll = false;
				if (ReloadState().bRefreshAllActions)
					bRefreshedAll = true;

				if (OldClass != nullptr)
				{
					if (!bRefreshedAll && GEngine != nullptr)
					{
#if WITH_DEV_AUTOMATION_TESTS
						if (!FClassReloadHelperTestAccess::HandleRefreshClassActions(OldClass))
#endif
						{
							auto& Database = FBlueprintActionDatabase::Get();
							Database.RefreshClassActions(OldClass);
						}
					}
				}

				if (NewClass != nullptr)
				{
					if (!bRefreshedAll && GEngine != nullptr)
					{
#if WITH_DEV_AUTOMATION_TESTS
						if (!FClassReloadHelperTestAccess::HandleRefreshClassActions(NewClass))
#endif
						{
							auto& Database = FBlueprintActionDatabase::Get();
							Database.RefreshClassActions(NewClass);
						}
					}

					if (NewClass->IsChildOf(UActorComponent::StaticClass()))
					{
#if WITH_DEV_AUTOMATION_TESTS
						if (!FClassReloadHelperTestAccess::HandleInvalidateComponentClass(NewClass))
#endif
						{
							FComponentTypeRegistry::Get().InvalidateClass(NewClass);
						}
					}

					if (NewClass->IsChildOf(AVolume::StaticClass()))
						ReloadState().bReloadedVolume = true;
				}
			}));

			AttachedHandles.Add(Hooks.GetOnDelegateReload().AddLambda(
			[](UDelegateFunction* OldDelegate, UDelegateFunction* NewDelegate)
			{
				ReloadState().ReloadDelegates.Add(OldDelegate, NewDelegate);
				ReloadState().NewDelegates.Add(NewDelegate);
			}));

			AttachedHandles.Add(Hooks.GetOnLiteralAssetReload().AddLambda(
			[](UObject* OldObject, UObject* NewObject)
			{
				ReloadState().ReloadAssets.Add(OldObject, NewObject);
			}));

			AttachedHandles.Add(Hooks.GetOnEnumChanged().AddLambda(
			[](UEnum* Enum, const TArray<TPair<FName, int64>>& OldNames)
			{
				ReloadState().ReloadEnums.Add(Enum);
				//WILL-EDIT
				// Need to refresh blueprints that depend on this
				//FEnumEditorUtils::BroadcastChanges((UUserDefinedEnum*)Enum, OldNames);
			}));

			AttachedHandles.Add(Hooks.GetOnEnumCreated().AddLambda(
			[](UEnum* Enum)
			{
				ReloadState().NewEnums.Add(Enum);
			}));

			AttachedHandles.Add(Hooks.GetOnFullReload().AddLambda(
			[]()
			{
				// Do the actual reinstancing required
				ReloadState().PerformReinstance();
			}));

			AttachedHandles.Add(Hooks.GetOnPostReload().AddLambda(
			[](bool bFullReload)
			{
				// Refresh action list in blueprint, this is what
				// is used to populate the right click menu.
				if (ReloadState().bRefreshAllActions && GEngine != nullptr)
				{
#if WITH_DEV_AUTOMATION_TESTS
					if (!FClassReloadHelperTestAccess::HandleRefreshAllActions())
#endif
					{
						auto& Database = FBlueprintActionDatabase::Get();
						Database.RefreshAll();
					}
				}

				// Refresh class lists by pretending we just compiled a bp
				if (bFullReload && GEditor != nullptr)
				{
					GEditor->BroadcastBlueprintCompiled();
				}

				if (!FAngelscriptEngine::IsInitialized() || !FAngelscriptEngine::Get().IsInitialCompileFinished())
				{
#if WITH_DEV_AUTOMATION_TESTS
					if (!FClassReloadHelperTestAccess::HandleInvalidateComponentRegistry())
#endif
					{
						FComponentTypeRegistry::Get().Invalidate();
					}
				}

				// If we reloaded any volume classes, trigger a geometry rebuild
				if (ReloadState().bReloadedVolume && GEngine != nullptr)
				{
					auto* World = GEditor->GetEditorWorldContext().World();
					ULevel* CurrentLevel = World->GetCurrentLevel();

#if WITH_DEV_AUTOMATION_TESTS
					if (!FClassReloadHelperTestAccess::HandleExecCommand(World, TEXT("MAP REBUILD ALLVISIBLE")))
#endif
					{
						GEngine->Exec( World, TEXT("MAP REBUILD ALLVISIBLE") );
					}

					// Map rebuild ("Build Geometry") is currently bugged (as of 5.4.1) and resets the CurrentLevel.
					// To avoid being annoying and resetting the active level on hotreload, restore it after the rebuild
					if (IsValid(CurrentLevel) && World->GetLevels().Contains(CurrentLevel))
						World->SetCurrentLevel(CurrentLevel);
				}

				// Reset state
				ReloadState() = FReloadState();
			}));
		}

		virtual void OnEngineDetached(FAngelscriptEngine& Engine) override
		{
			FAngelscriptEngineHooks& Hooks = Engine.GetHooks();
			// Multicast delegates only expose RemoveAll/Remove(Handle); since we
			// captured each handle on attach, remove them individually so other
			// subscribers on the same engine remain intact.
			for (FDelegateHandle& Handle : AttachedHandles)
			{
				if (Handle.IsValid())
				{
					Hooks.GetOnStructReload().Remove(Handle);
					Hooks.GetOnClassReload().Remove(Handle);
					Hooks.GetOnDelegateReload().Remove(Handle);
					Hooks.GetOnLiteralAssetReload().Remove(Handle);
					Hooks.GetOnEnumChanged().Remove(Handle);
					Hooks.GetOnEnumCreated().Remove(Handle);
					Hooks.GetOnFullReload().Remove(Handle);
					Hooks.GetOnPostReload().Remove(Handle);
				}
			}
			AttachedHandles.Reset();

			// Drop any partial reload state so a re-attached engine starts clean.
			ReloadState() = FReloadState();
		}

	private:
		// Each Add* call returns a unique FDelegateHandle. We don't know which
		// hook owns which handle at detach time, but Remove() on a multicast
		// delegate that doesn't own the handle is a no-op, so spraying the
		// removal across all 8 hooks is safe and cheap (8 * O(N) per hook).
		TArray<FDelegateHandle> AttachedHandles;
	};

	static void Init()
	{
		// Per-engine subscription replaces the previous module-startup
		// AddLambda calls into process-wide statics. ReplayCurrentEngine() makes
		// sure the extension still attaches when an engine already exists at
		// the time Init() runs (e.g. editor reload after the runtime module is
		// already up).
		FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(
			MakeShared<FClassReloadHelperExtension>());
		FAngelscriptEngineExtensionRegistry::Get().ReplayCurrentEngine();
	}

	static TArray<UDataTable*> GetTablesDependentOnStruct(UStruct* Struct)
	{
		TArray<UDataTable*> Result;
		if (Struct)
		{
			TArray<UObject*> DataTables;
			GetObjectsOfClass(UDataTable::StaticClass(), DataTables);
			for (UObject* DataTableObj : DataTables)
			{
				UDataTable* DataTable = Cast<UDataTable>(DataTableObj);
				if (DataTable && (Struct == DataTable->RowStruct))
				{
					Result.Add(DataTable);
				}
			}
		}
		return Result;
	}
};
