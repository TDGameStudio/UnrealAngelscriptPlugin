#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "ClassGenerator/AngelscriptClassGeneratorShared.h"
#include "ClassGenerator/AngelscriptClassRedirects.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"

#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/GarbageCollection.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/CoreRedirects.h"
//#include "GarbageCollectionSchema.h"
#include "UObject/GarbageCollection.h"
//#include "CoreUObject/Private/Serialization/UnversionPropertySerialization.h"
//#include "UObject/Private/Serialization/UnversionPropertySerialization.h"
//#include "Source/Runtime/CoreUObject/Private/Serialization/UnversionedPropertySerialization.h"
#include "UnversionedPropertySerialization.h"

#include "Misc/ScopedSlowTask.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Subsystems/SubsystemCollection.h"
#include "Subsystems/WorldSubsystem.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"

#include "AngelscriptType.h"
#include "AngelscriptDebugValue.h"
#include "AngelscriptInclude.h"
#include "AngelscriptMemoryTags.h"
#include "AngelscriptPerformanceStats.h"
#include "AngelscriptSettings.h"
#include "Binds/BlueprintCallableReflectiveFallback.h"
#include "Binds/Helper_FunctionSignature.h"

#include "StartAngelscriptHeaders.h"
//#include "as_config.h"
//#include "as_scriptengine.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
//#include "as_scriptobject.h"
//#include "as_context.h"
#include "source/as_config.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "source/as_scriptobject.h"
#include "source/as_context.h"
#include "source/as_generic.h"
#include "EndAngelscriptHeaders.h"

// ============================================================================
// Interface method dispatch via generic call convention
// ============================================================================

void CallInterfaceMethod(asIScriptGeneric* InGeneric)
{
	asCGeneric* Generic = static_cast<asCGeneric*>(InGeneric);
	auto* Sig = (FInterfaceMethodSignature*)Generic->GetFunction()->GetUserData();
	if (Sig == nullptr) return;

	UObject* Object = (UObject*)Generic->GetObject();
	if (Object == nullptr) return;

	UFunction* RealFunc = Object->FindFunction(Sig->FunctionName);
	if (RealFunc == nullptr) return;
	InvokeReflectionFallbackFromGenericCall(Generic, Object, RealFunc);
}

// Reload-lifecycle delegates have moved to FAngelscriptEngine (see
// Core/AngelscriptEngine.h). Triggering sites in this file now go through
// FAngelscriptEngine::Get().GetOnXxx().Broadcast(...).

int32 FAngelscriptClassGenerator::UniqueCounter()
{
	static int32 UniqueGeneratedCounter = 1;
	return UniqueGeneratedCounter++;
}

void FAngelscriptClassGenerator::AddModule(TSharedRef<FAngelscriptModuleDesc> Module)
{
	FModuleData Data;
	Data.OldModule = FAngelscriptEngine::Get().GetModule(Module->ModuleName);
	Data.NewModule = Module;
	Data.ModuleIndex = Modules.Num();

	Modules.Add(Data);

	ModuleIndexByName.Add(Module->ModuleName, Data.ModuleIndex);
	ModuleIndexByNewScriptModule.Add(Module->ScriptModule, Data.ModuleIndex);
}


TSharedPtr<FAngelscriptClassDesc> FAngelscriptClassGenerator::EnsureClassAnalyzed(const FString& ClassName)
{
	FDataRef* Ref = DataRefByName.Find(ClassName);
	if (Ref != nullptr && Ref->bIsClass)
	{
		auto& ModuleData = Modules[Ref->ModuleIndex];
		auto& ClassData = ModuleData.Classes[Ref->DataIndex];
		if (ClassData.NewClass.IsValid())
		{
			// Class is pending analysis, analyze it now and return it
			Analyze(ModuleData, ClassData);
			return ClassData.NewClass;
		}
	}

	// Class isn't pending analysis, look it up from the manager from previous compile
	return FAngelscriptEngine::Get().GetClass(ClassName);
}

TSharedPtr<FAngelscriptClassDesc> FAngelscriptClassGenerator::GetClassDesc(const FString& ClassName)
{
	FDataRef* Ref = DataRefByName.Find(ClassName);
	if (Ref != nullptr && Ref->bIsClass)
	{
		auto& ModuleData = Modules[Ref->ModuleIndex];
		auto& ClassData = ModuleData.Classes[Ref->DataIndex];
		if (ClassData.NewClass.IsValid())
			return ClassData.NewClass;
	}

	return FAngelscriptEngine::Get().GetClass(ClassName);
}

FString FAngelscriptClassGenerator::GetUnrealName(bool bIsStruct, const FString& ClassName)
{
	FString UnrealName = ClassName;
	if (bIsStruct && UnrealName.Len() >= 2)
	{
		if (UnrealName[0] == 'F')
		{
			if (FChar::IsUpper(UnrealName[1]))
				UnrealName = UnrealName.Mid(1);
		}
	}
	return UnrealName;
}

void FAngelscriptClassGenerator::PerformFullReload()
{
	PerformReload(true);
}

void FAngelscriptClassGenerator::PerformSoftReload()
{
	PerformReload(false);
}

void FAngelscriptClassGenerator::PerformReload(bool bFullReload)
{
	AS_PERF_SCOPE_CLASS_GENERATOR_RELOAD();

	// Create progress indicator
	FScopedSlowTask SlowTask(1.8f);
	if (bFullReload && FAngelscriptEngine::Get().bIsInitialCompileFinished)
		SlowTask.MakeDialogDelayed(0.5f);

	bIsDoingFullReload = bFullReload;

	{
		FAngelscriptScopeTimer Timer(TEXT("class generator reload"));

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.1f);

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.1f);

		// Link or create classes to the new script types
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (ShouldFullReload(ClassData))
				{
					if (ClassData.NewClass->bIsStruct)
						CreateFullReloadStruct(ModuleData, ClassData);
					else
						CreateFullReloadClass(ModuleData, ClassData);
				}
				else
				{
					LinkSoftReloadClasses(ModuleData, ClassData);
				}
			}

			for (auto& EnumData : ModuleData.Enums)
			{
				if (!ShouldFullReload(EnumData))
				{
					LinkSoftReloadClasses(ModuleData, EnumData);
				}
			}

			for (auto& DelegateData : ModuleData.Delegates)
			{
				if (ShouldFullReload(DelegateData))
				{
					CreateFullReloadDelegate(ModuleData, DelegateData);
				}
				else
				{
					LinkSoftReloadClasses(ModuleData, DelegateData);
				}
			}

			for (auto RemovedClass : ModuleData.RemovedClasses)
			{
				FullReloadRemoveClass(ModuleData, RemovedClass);
			}
		}

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.1f);

		// Reload any enums that have changed
		for (auto& ModuleData : Modules)
		{
			for (auto& EnumData : ModuleData.Enums)
			{
				if (ShouldFullReload(EnumData))
				{
					DoFullReload(ModuleData, EnumData);
				}
			}
		}

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.1f);

		// Structs should reload first
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (ShouldFullReload(ClassData) && ClassData.NewClass->bIsStruct)
				{
					DoFullReload(ModuleData, ClassData);
				}
			}
		}

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.1f);

		// Reload any delegates that have changed
		for (auto& ModuleData : Modules)
		{
			for (auto& DelegateData : ModuleData.Delegates)
			{
				if (ShouldFullReload(DelegateData))
				{
					DoFullReload(ModuleData, DelegateData);
				}
			}
		}

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.1f);

		// Do preparatory work for soft reloads to happen after the full reloads are done
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (!ClassData.NewClass->bIsStruct)
				{
					if (!ShouldFullReload(ClassData))
					{
						PrepareSoftReload(ModuleData, ClassData);
					}
				}
			}
		}

		// Reload all full reload classes, now that we have all structs
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (!ClassData.NewClass->bIsStruct)
				{
					if (ShouldFullReload(ClassData))
					{
						DoFullReload(ModuleData, ClassData);
					}
				}
			}
		}

		// Soft reloads need to happen after all the other reloads and prepare reloads are done
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (!ClassData.NewClass->bIsStruct)
				{
					if (!ShouldFullReload(ClassData))
					{
						DoSoftReload(ModuleData, ClassData);
					}
				}
			}
		}

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.1f);

		// Finalize all classes after reload
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (!ClassData.NewClass->bIsStruct)
				{
					if (ShouldFullReload(ClassData))
					{
						FinalizeClass(ModuleData, ClassData);
					}
				}
			}
		}

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.1f);

		// Initialize default objects for all classes after reload
		CallPostInitFunctions();
		InitDefaultObjects();

		// Very last verification step after all default objects are created
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (!ClassData.NewClass->bIsStruct)
					VerifyClass(ModuleData, ClassData.NewClass);
			}
		}
	}

	if (bIsDoingFullReload)
	{
		// Update progress indicator
		if (bReinstancingAny && FAngelscriptEngine::Get().bIsInitialCompileFinished)
			SlowTask.MakeDialog();
		SlowTask.EnterProgressFrame(0.5f, FText::FromString(TEXT("Unreal Hot Reload")));

		FAngelscriptScopeTimer PostTimer(TEXT("post full reload"));

		// Inform about all changed classes and structs
		for (auto& ModuleData : Modules)
		{
			for (auto& ClassData : ModuleData.Classes)
			{
				if (ClassData.NewClass->bIsStruct)
				{
					UScriptStruct* OldStruct = nullptr;
					UScriptStruct* NewStruct = nullptr;

					if (ClassData.OldClass.IsValid())
						OldStruct = (UScriptStruct*)ClassData.OldClass->Struct;
					else
						OldStruct = (UScriptStruct*)ClassData.ReplacedStruct;

					if (ClassData.NewClass.IsValid())
					{
						NewStruct = (UScriptStruct*)ClassData.NewClass->Struct;
						check(ClassData.ReplacedStruct == nullptr || OldStruct == (UScriptStruct*)ClassData.ReplacedStruct);
					}

					if ((OldStruct != nullptr || NewStruct != nullptr) && OldStruct != NewStruct)
					{
						if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
							HookEngine->GetOnStructReload().Broadcast(OldStruct, NewStruct);
					}
				}
				else
				{
					UClass* OldClass = nullptr;
					UClass* NewClass = nullptr;

					if (ClassData.OldClass.IsValid())
						OldClass = ClassData.OldClass->Class;
					else
						OldClass = ClassData.ReplacedClass;

					if (ClassData.NewClass.IsValid())
					{
						NewClass = ClassData.NewClass->Class;
						check(ClassData.ReplacedClass == nullptr || OldClass == ClassData.ReplacedClass);
					}

					if ((OldClass != nullptr || NewClass != nullptr) && OldClass != NewClass)
					{
						if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
							HookEngine->GetOnClassReload().Broadcast(OldClass, NewClass);
					}
				}
			}
		}

		// Call new reinstancing if needed
		if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
			HookEngine->GetOnFullReload().Broadcast();

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.5f, FText::FromString(TEXT("Unreal Editor Refresh")));

		// Classes that are no longer present in script should be marked as such
		TSet<void*> RemovedScriptTypes;
		for (auto& ModuleData : Modules)
		{
			for (auto RemovedClass : ModuleData.RemovedClasses)
			{
				UASClass* Class = (UASClass*)RemovedClass->Class;
				if (Class && Class->ScriptTypePtr != nullptr)
					RemovedScriptTypes.Add(Class->ScriptTypePtr);

				CleanupRemovedClass(RemovedClass);
			}
		}

		// Notify that a reload has just been performed
		{
			FAngelscriptScopeTimer Timer(TEXT("new class propagation"));
			if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
				HookEngine->GetOnPostReload().Broadcast(bIsDoingFullReload);
		}

		// Null out script types on old classes, we're
		// going to be deleting the module soon.
		if (bReinstancingAny)
		{
			for (auto& ModuleData : Modules)
			{
				for (auto& ClassData : ModuleData.Classes)
				{
					if (ClassData.ReplacedClass != nullptr)
					{
						if (ClassData.ReplacedClass->ScriptTypePtr != nullptr)
							RemovedScriptTypes.Add(ClassData.ReplacedClass->ScriptTypePtr);
						ClassData.ReplacedClass->ScriptTypePtr = nullptr;
						ClassData.ReplacedClass->OwnerScriptEngine = nullptr;
						ClassData.ReplacedClass->ConstructFunction = nullptr;
						ClassData.ReplacedClass->DefaultsFunction = nullptr;
					}
					else if (ClassData.ReplacedStruct != nullptr)
					{
						UASStruct* ReplacedStruct = (UASStruct*)ClassData.ReplacedStruct;
						ReplacedStruct->ScriptType = nullptr;
						ReplacedStruct->UpdateScriptType();
					}
				}
			}
		}

		// Delete script types from all classes that were refering to an old one
		if (RemovedScriptTypes.Num() != 0)
		{
			for (UClass* Class : TObjectRange<UClass>())
			{
				UASClass* asClass = Cast<UASClass>(Class);

				//if (Class->ScriptTypePtr == nullptr)
				//if (asClass != nullptr && asClass->ScriptTypePtr == nullptr)
			if (asClass == nullptr || asClass->ScriptTypePtr == nullptr)
				continue;
			if (RemovedScriptTypes.Contains(asClass->ScriptTypePtr))
			{
				asClass->ScriptTypePtr = nullptr;
				asClass->OwnerScriptEngine = nullptr;
				asClass->ConstructFunction = nullptr;
				asClass->DefaultsFunction = nullptr;
			}
			}
		}

		// Force a garbage collection step if we reinstanced any classes, so we don't litter with old instances
		if (bReinstancingAny)
			GEngine->ForceGarbageCollection(true);

		// If we've created any dynamic subsystem classes, inform subsystem collections
		if (ReinstancedSubsystems.Num() != 0)
		{
			if (GEngine != nullptr)
			{
				// The engine is initialized so we can activate our subsystems now
				for (UClass* NewSubsystem : ReinstancedSubsystems)
					FSubsystemCollectionBase::ActivateExternalSubsystem(NewSubsystem);
			}
			else
			{
				// This is likely an initial compile, we should wait with activating subsystems until the engine is inited
				FCoreDelegates::GetOnPostEngineInit().AddLambda([AddedSubsystems = ReinstancedSubsystems]()
					{
						for (UClass* NewSubsystem : AddedSubsystems)
							FSubsystemCollectionBase::ActivateExternalSubsystem(NewSubsystem);
					});
			}
		}
	}
	else
	{
		FAngelscriptScopeTimer PostTimer(TEXT("post soft reload"));
		if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
		{
			for (auto& ModuleData : Modules)
			{
				for (auto& ClassData : ModuleData.Classes)
				{
					if (ClassData.NewClass->bIsStruct)
						continue;

					UClass* NewClass = ClassData.NewClass.IsValid() ? ClassData.NewClass->Class : nullptr;
					UClass* OldClass = nullptr;
					if (ClassData.OldClass.IsValid())
						OldClass = ClassData.OldClass->Class;
					else
						OldClass = ClassData.ReplacedClass;

					if ((OldClass != nullptr || NewClass != nullptr) && OldClass != NewClass)
						HookEngine->GetOnClassReload().Broadcast(OldClass, NewClass);
				}
			}

			HookEngine->GetOnPostReload().Broadcast(bIsDoingFullReload);
		}
	}

#if WITH_EDITOR
	// Apply additional compile errors that the code classes want if applicable
	auto& AdditionalCompileChecks = FAngelscriptEngine::Get().AdditionalCompileChecks;
	for (auto& ModuleData : Modules)
	{
		for (auto& ClassData : ModuleData.Classes)
		{
			if (ClassData.NewClass->bIsStruct)
				continue;

			UClass* CodeParent = ClassData.NewClass->CodeSuperClass;
			while (CodeParent != nullptr)
			{
				auto* CheckBind = AdditionalCompileChecks.Find(CodeParent);
				if (CheckBind != nullptr && (*CheckBind).IsValid())
					(*CheckBind)->PostReloadAdditionalChecks(bIsDoingFullReload, ModuleData.NewModule, ClassData.NewClass);

				CodeParent = CodeParent->GetSuperClass();
			}
		}
	}
#endif
}

void FAngelscriptClassGenerator::EnsureReloaded(FModuleData& Module, FClassData& Class)
{
	if (Class.bReloaded)
		return;

	if (ShouldFullReload(Class))
		DoFullReload(Module, Class);
	else if (!Class.NewClass->bIsStruct)
		DoSoftReload(Module, Class);
}

void FAngelscriptClassGenerator::EnsureReloaded(UASClass* Class)
{
	for (auto& ModuleData : Modules)
	{
		for (auto& ClassData : ModuleData.Classes)
		{
			if (!ClassData.OldClass.IsValid())
				continue;

			if (ClassData.OldClass->Class == Class)
			{
				EnsureReloaded(ModuleData, ClassData);
				return;
			}
		}
	}
}

void FAngelscriptClassGenerator::EnsureReloaded(int TypeId)
{
	FModuleData* ModuleDataPtr = nullptr;
	FClassData* ClassDataPtr = nullptr;
	FDelegateData* DelegateDataPtr = nullptr;

	asITypeInfo* ScriptType = FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId);
	if (ScriptType == nullptr)
		return;

	// Also ensure reloads on subtypes
	for (int32 i = 0, SubCount = ScriptType->GetSubTypeCount(); i < SubCount; ++i)
	{
		int32 SubTypeId = ScriptType->GetSubTypeId(i);
		if ((SubTypeId & asTYPEID_OBJHANDLE) == 0)
			EnsureReloaded(SubTypeId);
	}

	// Ensure that the type we're acting on is actually reloaded
	if (GetDataFor(ScriptType, ModuleDataPtr, ClassDataPtr, DelegateDataPtr))
	{
		if (ClassDataPtr != nullptr)
			EnsureReloaded(*ModuleDataPtr, *ClassDataPtr);
	}
}

void FAngelscriptClassGenerator::EnsureClassFinalized(UASClass* Class)
{
	FModuleData* ModuleData = nullptr;
	FClassData* ClassData = nullptr;
	FDelegateData* DelegateData = nullptr;
	if (GetDataFor((asITypeInfo*)Class->ScriptTypePtr, ModuleData, ClassData, DelegateData))
	{
		if (ClassData != nullptr && !ClassData->bFinalized && ShouldFullReload(*ClassData))
			FinalizeClass(*ModuleData, *ClassData);
	}
}

void FAngelscriptClassGenerator::GetFullReloadLines(TSharedRef<FAngelscriptModuleDesc> Module, TArray<int32>& OutLines)
{
	for (auto& ModuleData : Modules)
	{
		if (ModuleData.NewModule == Module)
			OutLines.Append(ModuleData.ReloadReqLines);
	}
}

bool FAngelscriptClassGenerator::WantsFullReload(TSharedRef<FAngelscriptModuleDesc> Module)
{
	for (auto& ModuleData : Modules)
	{
		if (ModuleData.NewModule == Module)
		{
			return ModuleData.ReloadReq >= EReloadRequirement::FullReloadSuggested;
		}
	}
	return false;
}

bool FAngelscriptClassGenerator::NeedsFullReload(TSharedRef<FAngelscriptModuleDesc> Module)
{
	for (auto& ModuleData : Modules)
	{
		if (ModuleData.NewModule == Module)
		{
			return ModuleData.ReloadReq >= EReloadRequirement::FullReloadRequired;
		}
	}
	return false;
}
