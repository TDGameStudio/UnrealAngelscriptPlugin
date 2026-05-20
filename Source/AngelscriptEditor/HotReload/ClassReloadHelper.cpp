#include "HotReload/ClassReloadHelper.h"
#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"
#include "AngelscriptEngine.h"

#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "BlueprintCompilationManager.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/ReloadUtilities.h"
#include "IPlacementModeModule.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ActorFactories/ActorFactoryVolume.h"

namespace
{
#if WITH_DEV_AUTOMATION_TESTS
	FClassReloadHelperClassReloadTestHooks GClassReloadHelperClassReloadTestHooks;
	FClassReloadHelperPostReloadTestHooks GClassReloadHelperPostReloadTestHooks;
	FClassReloadHelperPerformReinstanceTestHooks GClassReloadHelperPerformReinstanceTestHooks;
#endif

	bool EnterPerformReinstanceBodyForReload()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GClassReloadHelperPerformReinstanceTestHooks.EnterPerformReinstanceBody)
		{
			return GClassReloadHelperPerformReinstanceTestHooks.EnterPerformReinstanceBody();
		}
#endif

		return false;
	}

	void NotifyCustomizationModuleChangedForReload()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GClassReloadHelperPerformReinstanceTestHooks.NotifyCustomizationModuleChanged)
		{
			GClassReloadHelperPerformReinstanceTestHooks.NotifyCustomizationModuleChanged();
			return;
		}
#endif

		FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
		if (PropertyModule)
			PropertyModule->NotifyCustomizationModuleChanged();
	}

	void AddActorFactoryForReload(UActorFactory* NewFactory)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GClassReloadHelperPerformReinstanceTestHooks.AddActorFactory)
		{
			GClassReloadHelperPerformReinstanceTestHooks.AddActorFactory();
			return;
		}
#endif

		GEditor->ActorFactories.Add(NewFactory);
	}

	void RefreshAssetActionsForReload(UEnum* ChangedEnum)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GClassReloadHelperPerformReinstanceTestHooks.RefreshAssetActions)
		{
			GClassReloadHelperPerformReinstanceTestHooks.RefreshAssetActions(ChangedEnum);
			return;
		}
#endif

		FBlueprintActionDatabase::Get().RefreshAssetActions(ChangedEnum);
	}

	void BroadcastAllPlaceableAssetsChangedForReload()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GClassReloadHelperPerformReinstanceTestHooks.BroadcastAllPlaceableAssetsChanged)
		{
			GClassReloadHelperPerformReinstanceTestHooks.BroadcastAllPlaceableAssetsChanged();
			return;
		}
#endif

		IPlacementModeModule::Get().OnAllPlaceableAssetsChanged().Broadcast();
	}

	void BroadcastPlaceableItemFilteringChangedForReload()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GClassReloadHelperPerformReinstanceTestHooks.BroadcastPlaceableItemFilteringChanged)
		{
			GClassReloadHelperPerformReinstanceTestHooks.BroadcastPlaceableItemFilteringChanged();
			return;
		}
#endif

		IPlacementModeModule::Get().OnPlaceableItemFilteringChanged().Broadcast();
	}

	void QueueBlueprintForCompilationForReload(UBlueprint* Blueprint)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GClassReloadHelperPerformReinstanceTestHooks.QueueBlueprintForCompilation)
		{
			GClassReloadHelperPerformReinstanceTestHooks.QueueBlueprintForCompilation(Blueprint);
			return;
		}
#endif

		FBlueprintCompilationManager::QueueForCompilation(Blueprint);
	}

	void FlushCompilationQueueAndReinstanceForReload()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GClassReloadHelperPerformReinstanceTestHooks.FlushCompilationQueueAndReinstance)
		{
			GClassReloadHelperPerformReinstanceTestHooks.FlushCompilationQueueAndReinstance();
			return;
		}
#endif

		FBlueprintCompilationManager::FlushCompilationQueueAndReinstance();
	}
}

// The new unreal reload system is not yet up to providing for AS reloads (for example, it does not deal well with changing UFunction definitions)
// However, you can try it out with this CVar and hopefully at some point it can replace the hacked-together mess using the old reloader.
static int32 GAngelscriptUseUnrealReload = 0;
static FAutoConsoleVariableRef CVar_AngelscriptUseUnrealReload(TEXT("angelscript.UseUnrealReload"), GAngelscriptUseUnrealReload, TEXT(""));

UAngelscriptReferenceReplacementHelper* ReplaceHelper = nullptr;

#if WITH_DEV_AUTOMATION_TESTS
void FClassReloadHelperTestAccess::SetClassReloadTestHooks(FClassReloadHelperClassReloadTestHooks InHooks)
{
	GClassReloadHelperClassReloadTestHooks = MoveTemp(InHooks);
}

void FClassReloadHelperTestAccess::ResetClassReloadTestHooks()
{
	GClassReloadHelperClassReloadTestHooks = FClassReloadHelperClassReloadTestHooks();
}

bool FClassReloadHelperTestAccess::HandleRefreshClassActions(UClass* Class)
{
	if (GClassReloadHelperClassReloadTestHooks.RefreshClassActions)
	{
		GClassReloadHelperClassReloadTestHooks.RefreshClassActions(Class);
		return true;
	}

	return false;
}

bool FClassReloadHelperTestAccess::HandleInvalidateComponentClass(UClass* Class)
{
	if (GClassReloadHelperClassReloadTestHooks.InvalidateComponentClass)
	{
		GClassReloadHelperClassReloadTestHooks.InvalidateComponentClass(Class);
		return true;
	}

	return false;
}

void FClassReloadHelperTestAccess::SetPostReloadTestHooks(FClassReloadHelperPostReloadTestHooks InHooks)
{
	GClassReloadHelperPostReloadTestHooks = MoveTemp(InHooks);
}

void FClassReloadHelperTestAccess::ResetPostReloadTestHooks()
{
	GClassReloadHelperPostReloadTestHooks = FClassReloadHelperPostReloadTestHooks();
}

bool FClassReloadHelperTestAccess::HandleRefreshAllActions()
{
	if (GClassReloadHelperPostReloadTestHooks.RefreshAllActions)
	{
		GClassReloadHelperPostReloadTestHooks.RefreshAllActions();
		return true;
	}

	return false;
}

bool FClassReloadHelperTestAccess::HandleInvalidateComponentRegistry()
{
	if (GClassReloadHelperPostReloadTestHooks.InvalidateComponentRegistry)
	{
		GClassReloadHelperPostReloadTestHooks.InvalidateComponentRegistry();
		return true;
	}

	return false;
}

bool FClassReloadHelperTestAccess::HandleExecCommand(UWorld* World, const TCHAR* Command)
{
	if (GClassReloadHelperPostReloadTestHooks.ExecCommand)
	{
		GClassReloadHelperPostReloadTestHooks.ExecCommand(World, Command);
		return true;
	}

	return false;
}

void FClassReloadHelperTestAccess::SetPerformReinstanceTestHooks(FClassReloadHelperPerformReinstanceTestHooks InHooks)
{
	GClassReloadHelperPerformReinstanceTestHooks = MoveTemp(InHooks);
}

void FClassReloadHelperTestAccess::ResetPerformReinstanceTestHooks()
{
	GClassReloadHelperPerformReinstanceTestHooks = FClassReloadHelperPerformReinstanceTestHooks();
	GClassReloadHelperPostReloadTestHooks = FClassReloadHelperPostReloadTestHooks();
	GClassReloadHelperClassReloadTestHooks = FClassReloadHelperClassReloadTestHooks();
}
#endif

void FClassReloadHelper::FReloadState::PerformReinstance()
{
	// PerformReinstance is the entire surface of the editor-time hot-reload
	// flow: it allocates new UClass instances, clones BPGCs, and fixes up
	// referencers. Tag the whole call tree under Angelscript so
	// hot-reload memory growth shows up under one bucket instead of being
	// smeared across UnrealEd / Engine.
	LLM_SCOPE_BYTAG(Angelscript);

	// We never go into the reinstance path in an initial compile, there's no point
	if (!FAngelscriptEngine::Get().bIsInitialCompileFinished)
		return;

	if (EnterPerformReinstanceBodyForReload())
		return;

	// Init our replace helper that ReparentHierarchies will try to replace stuff in later
	if (ReplaceHelper == nullptr)
	{
		ReplaceHelper = NewObject<UAngelscriptReferenceReplacementHelper>(GetTransientPackage());
		ReplaceHelper->AddToRoot();
	}

	if (GAngelscriptUseUnrealReload == 0)
	{
		TArray<UBlueprint*> DependencyBPs;
		TArray<UK2Node*> AllNodes;

		// Go through all blueprints and find any that are using a struct
		// or delegate that we have replaced, and change their pins
		// to point to the new ones instead.
		{
			TMap<UObject*, UObject*> ClassReplaceList;
			for (auto& Elem : ReloadClasses)
				ClassReplaceList.Add(Elem.Key, Elem.Value);
			for (auto& Elem : ReloadStructs)
				ClassReplaceList.Add(Elem.Key, Elem.Value);

			auto ReplacePinType = [&](FEdGraphPinType& PinType) -> bool
			{
				if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
				{
					UScriptStruct* Struct = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get());
					if (Struct == nullptr)
						return false;

					UScriptStruct** NewStruct = ReloadStructs.Find(Struct);
					if (NewStruct == nullptr)
						return false;

					PinType.PinSubCategoryObject = *NewStruct;
					return true;
				}
				else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum || PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
				{
					if (ReloadEnums.Contains((UEnum*)PinType.PinSubCategoryObject.Get()))
						return true;
					else
						return false;
				}
				else
				{
					return false;
				}
			};

			AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols ImpactSymbols;
			for (const auto& ReloadClass : ReloadClasses)
			{
				ImpactSymbols.Classes.Add(ReloadClass.Key);
				ImpactSymbols.ReplacementObjects.Add(ReloadClass.Key, ReloadClass.Value);
			}
			for (const auto& ReloadStruct : ReloadStructs)
			{
				ImpactSymbols.Structs.Add(ReloadStruct.Key);
				ImpactSymbols.ReplacementObjects.Add(ReloadStruct.Key, ReloadStruct.Value);
			}
			for (UEnum* ReloadEnum : ReloadEnums)
			{
				ImpactSymbols.Enums.Add(ReloadEnum);
			}
			for (const auto& ReloadDelegate : ReloadDelegates)
			{
				ImpactSymbols.Delegates.Add(ReloadDelegate.Key);
				ImpactSymbols.Delegates.Add(ReloadDelegate.Value);
			}
			for (UDelegateFunction* NewDelegate : NewDelegates)
			{
				ImpactSymbols.Delegates.Add(NewDelegate);
			}

			for (TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
			{
				UBlueprint* BP = *BlueprintIt;
				TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> ImpactReasons;
				const bool bHasDependency = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*BP, ImpactSymbols, ImpactReasons);

				AllNodes.Reset();
				FBlueprintEditorUtils::GetAllNodesOfClass(BP, AllNodes);
				for (UK2Node* Node : AllNodes)
				{
					for (auto* Pin : Node->Pins)
					{
						ReplacePinType(Pin->PinType);
					}

					if (auto* EditableBase = Cast<UK2Node_EditablePinBase>(Node))
					{
						for (auto Desc : EditableBase->UserDefinedPins)
						{
							ReplacePinType(Desc->PinType);
						}
					}

					if (auto* MacroInst = Cast<UK2Node_MacroInstance>(Node))
					{
						ReplacePinType(MacroInst->ResolvedWildcardType);
					}
				}

				for (auto& Variable : BP->NewVariables)
				{
					ReplacePinType(Variable.VarType);
				}

				if (bHasDependency)
				{
					DependencyBPs.Add(BP);
				}
			}

			for (auto& Struct : ReloadStructs)
			{
				//FStructureEditorUtils::BroadcastPreChange(Struct.Key);
				UUserDefinedStruct* userStruct = Cast<UUserDefinedStruct>(Struct.Key);
				if (userStruct != nullptr)
				{
					FStructureEditorUtils::BroadcastPreChange(userStruct);
				}

				// Update struct pointers in DataTable with newly generated replacements.
				TArray<UDataTable*> Tables = GetTablesDependentOnStruct(Struct.Key);
				for (UDataTable* Table : Tables)
				{
					Table->RowStruct = Struct.Value;
				}
			}
		}

		// Do a full-on garbage collection step to make sure old stuff is gone
		// before we start reinstancing things we no longer need.
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);

		//WILL-EDIT-Temp
		//extern UNREALED_API TMap<UObject*, UObject*> GAngelscriptAdditionalReplacementObjects;
		//for (auto& Elem : ReloadStructs)
		//	GAngelscriptAdditionalReplacementObjects.Add(Elem.Key, Elem.Value);
		//for (auto& Elem : ReloadDelegates)
		//	GAngelscriptAdditionalReplacementObjects.Add(Elem.Key, Elem.Value);
		//for (auto& Elem : ReloadAssets)
		//	GAngelscriptAdditionalReplacementObjects.Add(Elem.Key, Elem.Value);

		// Call into unreal's standard reinstancing system to
		// actually recreate objects using the old classes.
		FBlueprintCompilationManager::ReparentHierarchies(ReloadClasses);

		//WILL-EDIT-TEMP
		//GAngelscriptAdditionalReplacementObjects.Reset();

		// Do a full-on garbage collection step to make sure old stuff is gone
		// by the time we recompile blueprints below.
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);

		// Make sure all blueprints that had dependencies to structs or delegates
		// are now properly recompiled.
		if (DependencyBPs.Num() != 0)
		{
			TSet<UClass*> NewlyCreatedClasses;
			for (auto& Elem : ReloadClasses)
				NewlyCreatedClasses.Add(Elem.Value);
			TSet<UScriptStruct*> NewlyCreatedStructs;
			for (auto& Elem : ReloadStructs)
				NewlyCreatedStructs.Add(Elem.Value);

			// Refresh nodes in blueprint graphs that depend on stuff we've reloaded.
			// If we don't do this then we will get errors until the nodes are manually refreshed!
			auto RefreshRelevantNodesInBP = [&](UBlueprint* BP)
			{
				AllNodes.Reset();
				FBlueprintEditorUtils::GetAllNodesOfClass(BP, AllNodes);

				auto CheckRefresh = [&](FEdGraphPinType& PinType) -> bool
				{
					if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
					{
						UScriptStruct* Struct = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get());
						return NewlyCreatedStructs.Contains(Struct);
					}
					else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum || PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
					{
						if (ReloadEnums.Contains((UEnum*)PinType.PinSubCategoryObject.Get()))
							return true;
						else
							return false;
					}
					else
					{
						return false;
					}
				};

				for (UK2Node* Node : AllNodes)
				{
					TArray<UStruct*> Dependencies;
					bool bShouldRefresh = false;

					if (Node->HasExternalDependencies(&Dependencies))
					{
						for (UStruct* Struct : Dependencies)
						{
							if (NewlyCreatedClasses.Contains((UClass*)Struct))
							{
								bShouldRefresh = true;
								break;
							}
							if (NewlyCreatedStructs.Contains((UScriptStruct*)Struct))
							{
								bShouldRefresh = true;
								break;
							}
						}
					}

					if (NewlyCreatedStructs.Num() != 0 && !bShouldRefresh)
					{
						for (auto* Pin : Node->Pins)
						{
							bShouldRefresh |= CheckRefresh(Pin->PinType);
						}
					}

					if (auto* EditableBase = Cast<UK2Node_EditablePinBase>(Node))
					{
						for (auto Desc : EditableBase->UserDefinedPins)
						{
							bShouldRefresh |= CheckRefresh(Desc->PinType);
						}
					}

					if (auto* Event = Cast<UK2Node_Event>(Node))
					{
						//if (auto* Function = Cast<UDelegateFunction>(Event->GetTiedSignatureFunction()))
						if (auto* Function = Cast<UDelegateFunction>(Event->FindEventSignatureFunction()))
						{
							if (NewDelegates.Contains(Function) || ReloadDelegates.Contains(Function))
							{
								bShouldRefresh = true;
							}
						}
					}

					if (auto* MacroInst = Cast<UK2Node_MacroInstance>(Node))
					{
						bShouldRefresh |= CheckRefresh(MacroInst->ResolvedWildcardType);
					}

					if (bShouldRefresh)
					{
						const UEdGraphSchema* Schema = Node->GetGraph()->GetSchema();
						Schema->ReconstructNode(*Node, true);
					}
				}
			};

			// Trigger a compile of all blueprints that we detected dependencies to our class in
			for (UBlueprint* BP : DependencyBPs)
			{
				RefreshRelevantNodesInBP(BP);
				QueueBlueprintForCompilationForReload(BP);
			}

			FlushCompilationQueueAndReinstanceForReload();
		}

		for (auto& Struct : ReloadStructs)
		{
			//FStructureEditorUtils::BroadcastPostChange(Struct.Value);
			UUserDefinedStruct* userStruct = Cast<UUserDefinedStruct>(Struct.Value);
			if (userStruct != nullptr)
			{
				FStructureEditorUtils::BroadcastPostChange(userStruct);
			}
		}
	}
	else
	{
		// Do a full-on garbage collection step to make sure old stuff is gone
		// before we start reinstancing things we no longer need.
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);

		// Call into unreal's standard reinstancing system to
		// actually recreate objects using the old classes.
		auto Reload = new FReload(EActiveReloadType::Reinstancing, TEXT("Angelscript"), *GLog);

		for (auto& ReloadInfo : ReloadStructs)
			Reload->NotifyChange(ReloadInfo.Value, ReloadInfo.Key);
		for (auto& ReloadInfo : ReloadClasses)
			Reload->NotifyChange(ReloadInfo.Value, ReloadInfo.Key);
		for (UEnum* ReloadEnum : ReloadEnums)
			Reload->NotifyChange(ReloadEnum, ReloadEnum);

		Reload->Reinstance();
		Reload->Finalize();
		delete Reload;
	}

	// We want to force-update all the property editing UI now that we've done this reload.
	//  The easiest way to do that is to send this NotifyCustomizationModuleChanged, since
	//  all this does is a refresh on the UI, but there's no separate 'force refresh'.
	NotifyCustomizationModuleChangedForReload();

	// If we've created any new volumes, we want to make sure they have actor factories
	TArray<UClass*> NewVolumeClasses;
	for (UClass* NewClass : NewClasses)
	{
		if (NewClass != nullptr && NewClass->IsChildOf(AVolume::StaticClass()))
			NewVolumeClasses.Add(NewClass);
	}

	if (NewVolumeClasses.Num() != 0)
	{
		TArray<UClass*> VolumeFactoryClasses;

		// Find all actor factories we use for volumes
		for (TObjectIterator<UClass> ObjectIt; ObjectIt; ++ObjectIt)
		{
			UClass* TestClass = *ObjectIt;
			if (TestClass->IsChildOf(UActorFactoryVolume::StaticClass()) && !TestClass->HasAnyClassFlags(CLASS_Abstract))
				VolumeFactoryClasses.Add(TestClass);
		}

		// Generate factories for the volume actor we just created
		for (UClass* VolumeFactoryClass : VolumeFactoryClasses)
		{
			for (UClass* VolumeClass : NewVolumeClasses)
			{
				UActorFactory* NewFactory = NewObject<UActorFactory>(GetTransientPackage(), VolumeFactoryClass);
				check(NewFactory);
				NewFactory->NewActorClass = VolumeClass;
				AddActorFactoryForReload(NewFactory);
			}
		}
	}

	// Refresh actions for enums
	for (UEnum* ChangedEnum : ReloadEnums)
		RefreshAssetActionsForReload(ChangedEnum);
	for (UEnum* ChangedEnum : NewEnums)
		RefreshAssetActionsForReload(ChangedEnum);

	if (NewClasses.Num() != 0)
	{
		BroadcastAllPlaceableAssetsChangedForReload();
		BroadcastPlaceableItemFilteringChangedForReload();
	}
}

void UAngelscriptReferenceReplacementHelper::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	if (InThis->HasAnyFlags(RF_ClassDefaultObject))
		return;
	if (GEditor == nullptr)
		return;

	UAssetEditorSubsystem* SubSystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (SubSystem == nullptr)
		return;

	TArray<TObjectPtr<UObject>> OpenAssetPtrs;

	TArray<UObject*> OpenAssets = SubSystem->GetAllEditedAssets();
	for (UObject* Obj : OpenAssets)
		OpenAssetPtrs.Add(Obj);

	Collector.AddReferencedObjects(OpenAssetPtrs);
}

void UAngelscriptReferenceReplacementHelper::Serialize(FStructuredArchive::FRecord Record)
{
	UObject::Serialize(Record);
	if (HasAnyFlags(RF_ClassDefaultObject))
		return;

	FArchive& UnderlyingArchive = Record.GetUnderlyingArchive();

	if (UnderlyingArchive.IsObjectReferenceCollector())
	{
		// Workaround to replace references to open assets in the asset editor subsystem.
		// If we don't do this, we will have stale object pointers, because it doesn't GC them.
		UAssetEditorSubsystem* SubSystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

		TArray<UObject*> OpenAssets = SubSystem->GetAllEditedAssets();
		for (UObject* OriginalAsset : OpenAssets)
		{
			UObject* ReplacedAsset = OriginalAsset;
			UnderlyingArchive << ReplacedAsset;

			if (ReplacedAsset != OriginalAsset)
			{
				auto Editors = SubSystem->FindEditorsForAsset(OriginalAsset);
				for (auto* EditorInstance : Editors)
				{
					SubSystem->NotifyAssetClosed(OriginalAsset, EditorInstance);
					SubSystem->NotifyAssetOpened(ReplacedAsset, EditorInstance);
				}
			}
		}
	}
}

