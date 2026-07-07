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
#include "source/as_config.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "source/as_scriptobject.h"
#include "source/as_context.h"
#include "source/as_generic.h"
#include "EndAngelscriptHeaders.h"

using namespace AngelscriptClassGeneratorNames;

void FAngelscriptClassGenerator::CreateFullReloadClass(FModuleData& ModuleData, FClassData& ClassData)
{
	auto ClassDesc = ClassData.NewClass;
	auto ModuleDesc = ModuleData.NewModule;

	FString ClassName = ClassDesc->ClassName;

	FString UnrealName = GetUnrealName(false, ClassName);

	// Check if we're replacing a class
	UASClass* ReplacedClass = FindObject<UASClass>(FAngelscriptEngine::GetPackage(), *UnrealName);
	const bool bReplacingSameNameClass = ReplacedClass != nullptr;
	if (ReplacedClass == nullptr)
	{
		ReplacedClass = ResolveClassRedirectReplacedClass(ModuleData, ClassData);
	}

	if (ReplacedClass)
	{
		ReplacedClass->ClassFlags |= CLASS_NewerVersionExists;

		if (bReplacingSameNameClass)
		{
			FString OldClassName = FString::Printf(TEXT("%s_REPLACED_%d"), *ReplacedClass->GetName(), UniqueCounter());
			ReplacedClass->Rename(*OldClassName, nullptr, REN_DontCreateRedirectors);
		}
	}

	LLM_SCOPE_BYTAG(Angelscript);
	UASClass* NewClass = NewObject<UASClass>(
		FAngelscriptEngine::GetPackage(),
		UASClass::StaticClass(),
		FName(*UnrealName),
		RF_Public | RF_Standalone | RF_MarkAsRootSet
	);

	asITypeInfo* ScriptType = ClassDesc->ScriptType;
	if (ScriptType != nullptr)
		ScriptType->SetUserData(NewClass);

	ClassDesc->Class = NewClass;
	ClassData.ReplacedClass = ReplacedClass;

	// Fill the StaticClass global variable so ClassName::StaticClass() works in AS.
	SetScriptStaticClass(ClassDesc, NewClass);

	// If we're creating a new dynamic subsystem class, mark it
	if (ClassDesc->CodeSuperClass->IsChildOf<UDynamicSubsystem>() || ClassDesc->CodeSuperClass->IsChildOf<UWorldSubsystem>())
	{
		if (ReplacedClass != nullptr)
			FSubsystemCollectionBase::DeactivateExternalSubsystem(ReplacedClass);
		ReinstancedSubsystems.Add(NewClass);
	}
}

UASClass* FAngelscriptClassGenerator::ResolveClassRedirectReplacedClass(FModuleData& ModuleData, FClassData& ClassData)
{
	if (!ClassData.NewClass.IsValid()
		|| ClassData.OldClass.IsValid()
		|| ClassData.NewClass->bIsStruct
		|| ClassData.NewClass->bIsStaticsClass)
	{
		return nullptr;
	}

	const auto TryResolvePreviousNames = [&ModuleData](const TArray<FCoreRedirectObjectName>& PreviousClassNames) -> UASClass*
	{
		for (const FCoreRedirectObjectName& PreviousClassName : PreviousClassNames)
		{
			const FString PreviousClassPath = PreviousClassName.ToString();
			for (const TSharedPtr<FAngelscriptClassDesc>& RemovedClass : ModuleData.RemovedClasses)
			{
				if (!RemovedClass.IsValid()
					|| RemovedClass->bIsStruct
					|| RemovedClass->bIsStaticsClass)
				{
					continue;
				}

				UASClass* RemovedASClass = Cast<UASClass>(RemovedClass->Class);
				if (RemovedASClass != nullptr && RemovedASClass->NewerVersion != nullptr)
				{
					continue;
				}

				const FName RemovedClassName(*RemovedClass->ClassName);
				if (PreviousClassName.ObjectName != RemovedClassName
					&& PreviousClassPath != RemovedClass->ClassName
					&& PreviousClassPath != FString::Printf(TEXT("/Script/Angelscript.%s"), *RemovedClass->ClassName))
				{
					continue;
				}

				return RemovedASClass;
			}
		}
		return nullptr;
	};

	TArray<FCoreRedirectObjectName> PreviousClassNames;
	const FString NewClassPath = FString::Printf(TEXT("/Script/Angelscript.%s"), *ClassData.NewClass->ClassName);
	if (FCoreRedirects::FindPreviousNames(
		ECoreRedirectFlags::Type_Class,
		FCoreRedirectObjectName(NewClassPath),
		PreviousClassNames))
	{
		if (UASClass* ReplacedClass = TryResolvePreviousNames(PreviousClassNames))
		{
			return ReplacedClass;
		}
	}

	PreviousClassNames.Reset();
	if (FCoreRedirects::FindPreviousNames(
		ECoreRedirectFlags::Type_Class,
		FCoreRedirectObjectName(ClassData.NewClass->ClassName),
		PreviousClassNames))
	{
		if (UASClass* ReplacedClass = TryResolvePreviousNames(PreviousClassNames))
		{
			return ReplacedClass;
		}
	}

	return nullptr;
}

void FAngelscriptClassGenerator::FullReloadRemoveClass(FModuleData& ModuleData, TSharedPtr<FAngelscriptClassDesc> RemovedClass)
{
	// If we're removing a new dynamic subsystem class, deactivate it
	if (RemovedClass->Class != nullptr && (RemovedClass->Class->IsChildOf<UDynamicSubsystem>() || RemovedClass->Class->IsChildOf<UWorldSubsystem>()))
		FSubsystemCollectionBase::DeactivateExternalSubsystem(RemovedClass->Class);
}

void FAngelscriptClassGenerator::CreateFullReloadStruct(FModuleData& ModuleData, FClassData& ClassData)
{
	auto ClassDesc = ClassData.NewClass;
	auto ModuleDesc = ModuleData.NewModule;

	FString ClassName = ClassDesc->ClassName;

	FString UnrealName = GetUnrealName(true, ClassName);

	// Check if we're replacing a struct
	UASStruct* ReplacedStruct = FindObject<UASStruct>(FAngelscriptEngine::GetPackage(), *UnrealName);
	if (ReplacedStruct)
	{
		FString OldClassName = FString::Printf(TEXT("%s_REPLACED_%d"), *ReplacedStruct->GetName(), UniqueCounter());
		ReplacedStruct->Rename(*OldClassName, nullptr, REN_DontCreateRedirectors);
	}

	LLM_SCOPE_BYTAG(Angelscript);
	UASStruct* NewStruct = NewObject<UASStruct>(
		FAngelscriptEngine::GetPackage(),
		UASStruct::StaticClass(),
		FName(*UnrealName),
		RF_Public | RF_Standalone | RF_MarkAsRootSet
	);

	NewStruct->bIsScriptStruct = true;
	NewStruct->SetSuperStruct(nullptr);

	if (ReplacedStruct != nullptr)
		NewStruct->Guid = ReplacedStruct->Guid;
	else
		NewStruct->SetGuid(NewStruct->GetFName());

	FString DisplayString = NewStruct->GetName();
	DisplayString = FName::NameToDisplayString(DisplayString, false);

#if WITH_EDITOR
	NewStruct->SetMetaData(NAME_DisplayName, *DisplayString);

	for (auto& Elem : ClassDesc->Meta)
		NewStruct->SetMetaData(Elem.Key, *Elem.Value);
#endif

	asITypeInfo* ScriptType = ClassDesc->ScriptType;
	ScriptType->SetUserData(NewStruct);
	NewStruct->SetPropertiesSize(ScriptType->GetSize());

	// Tell the loading system the struct exists
	NotifyRegistrationEvent(TEXT("/Script/Angelscript"), *UnrealName, ENotifyRegistrationType::NRT_Struct,
		ENotifyRegistrationPhase::NRP_Finished, nullptr, false, NewStruct);

	ClassDesc->Struct = NewStruct;
	ClassData.ReplacedStruct = ReplacedStruct;
}

void FAngelscriptClassGenerator::CreateFullReloadDelegate(FModuleData& Module, FDelegateData& Delegate)
{
	auto DelegateDesc = Delegate.NewDelegate;

	FName FunctionName = *(FString::Printf(TEXT("%s"),
			*DelegateDesc->DelegateName
		) + HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX);

	UDelegateFunction* ReplacedFunction = FindObject<UDelegateFunction>(FAngelscriptEngine::GetPackage(), *FunctionName.ToString());
	if (ReplacedFunction)
	{
		FString OldFunctionName = FString::Printf(TEXT("%s_REPLACED_%d"), *ReplacedFunction->GetName(), UniqueCounter());
		ReplacedFunction->Rename(*OldFunctionName, nullptr, REN_DontCreateRedirectors);
	}

	LLM_SCOPE_BYTAG(Angelscript);
	UDelegateFunction* Function = NewObject<UDelegateFunction>(
		FAngelscriptEngine::GetPackage(),
		UDelegateFunction::StaticClass(),
		FunctionName,
		RF_Public | RF_Standalone | RF_MarkAsRootSet
	);

	DelegateDesc->Function = Function;
	if (DelegateDesc->ScriptType)
		DelegateDesc->ScriptType->SetUserData(Function);

	// Tell the loading system the delegate exists
	NotifyRegistrationEvent(TEXT("/Script/Angelscript"), *FunctionName.ToString(), ENotifyRegistrationType::NRT_Struct,
		ENotifyRegistrationPhase::NRP_Finished, nullptr, false, Function);
}

void FAngelscriptClassGenerator::DoFullReload(FModuleData& ModuleData, FClassData& ClassData)
{
	auto ClassDesc = ClassData.NewClass;
	auto ModuleDesc = ModuleData.NewModule;

	// Check if we've already performed the reload on this class
	if (ClassData.bReloaded)
		return;
	ClassData.bReloaded = true;

	// Log the reload if needed
	if (ClassData.OldClass.IsValid())
		UE_LOG(Angelscript, Log, TEXT("Full Reload: %s"), *ClassData.NewClass->ClassName);

	// Structs and classes should be handled slightly differently
	if (ClassData.NewClass->bIsStruct)
	{
		DoFullReloadStruct(ModuleData, ClassData);
	}
	else
	{
		DoFullReloadClass(ModuleData, ClassData);
	}

}

void FAngelscriptClassGenerator::DoFullReloadStruct(FModuleData& ModuleData, FClassData& ClassData)
{
	auto ClassDesc = ClassData.NewClass;
	auto ModuleDesc = ModuleData.NewModule;

	FString ClassName = ClassDesc->ClassName;
	auto* ScriptType = ClassDesc->ScriptType;

	UASStruct* NewStruct = (UASStruct*)ClassDesc->Struct;
	UASStruct* ReplacedStruct = (UASStruct*)ClassData.ReplacedStruct;

	// Set up the class' base data

	NewStruct->PropertyLink = nullptr;
	NewStruct->MinAlignment = ScriptType->alignment;
	NewStruct->Bind();

	// Record data about the class in the descriptor for further pipeline use
	ClassDesc->Struct = NewStruct;
	NewStruct->ScriptType = ScriptType;
	NewStruct->SetPropertiesSize(ScriptType->GetSize());

	NewStruct->SetCppStructOps(NewStruct->CreateCppStructOps());
	NewStruct->PrepareCppStructOps();

	// Add all properties from angelscript as FProperty to the class
	int32 PropertiesSize = AddClassProperties(ClassDesc);
	NewStruct->SetPropertiesSize(PropertiesSize);

	NewStruct->StaticLink();
	NewStruct->DestructorLink = nullptr;

	// Tell the hot-reloader to replace the old class
	if (ReplacedStruct != nullptr)
	{
		bReinstancingAny = true;
		ReplacedStruct->NewerVersion = NewStruct;
	}
}

void FAngelscriptClassGenerator::DoFullReloadClass(FModuleData& ModuleData, FClassData& ClassData)
{
	auto ClassDesc = ClassData.NewClass;
	auto ModuleDesc = ModuleData.NewModule;

	FString ClassName = ClassDesc->ClassName;
	asITypeInfo* ScriptType = ClassDesc->ScriptType;

	UClass* ParentCodeClass = ClassDesc->CodeSuperClass;
	UASClass* ParentASClass = nullptr;

	if (!ClassDesc->bSuperIsCodeClass)
	{
		// If the type's super class is in a module we're currently compiling,
		// force that to do its full reload first.
		asITypeInfo* ScriptSuperType = ScriptType->GetBaseType();
		asIScriptModule* ScriptSuperModule = ScriptSuperType->GetModule();

		bool bFoundInCompilingModules = false;
		for (auto& CheckModuleData : Modules)
		{
			if (CheckModuleData.NewModule->ScriptModule == ScriptSuperModule)
			{
				// Make sure class is actually reloaded
				for (auto& CheckClassData : CheckModuleData.Classes)
				{
					if (CheckClassData.NewClass->ClassName == ClassDesc->SuperClass)
					{
						EnsureReloaded(CheckModuleData, CheckClassData);

						ParentASClass = (UASClass*)CheckClassData.NewClass->Class;
						bFoundInCompilingModules = true;
						break;
					}
				}
				break;
			}
		}

		if (!bFoundInCompilingModules)
		{
			ParentASClass = Cast<UASClass>(FAngelscriptEngine::Get().GetClass(ClassDesc->SuperClass)->Class);
			check(ParentASClass);
		}
	}

	UClass* SuperClass = ParentASClass ? ParentASClass : ParentCodeClass;
	UASClass* NewClass = (UASClass*)ClassDesc->Class;
	UASClass* ReplacedClass = ClassData.ReplacedClass;

	// Need to make sure all instances we're going to full reload are fully constructed before we can do anything
	TArray<UObject*> Instances;
	TArray<UObject*> CDOInstances;
	GetObjectsOfClass(ReplacedClass, Instances, true, RF_NoFlags);

	// Set up the class' base data
	NewClass->ClassFlags = CLASS_CompiledFromBlueprint;
	NewClass->bIsScriptClass = true;
	NewClass->ClassFlags |= (SuperClass->ClassFlags & CLASS_ScriptInherit);

	if (ClassDesc->ConfigName.Len() != 0)
	{
		NewClass->ClassFlags |= CLASS_Config;
		NewClass->ClassConfigName = FName(*ClassDesc->ConfigName);
	}
	else
	{
		NewClass->ClassConfigName = SuperClass->ClassConfigName;
	}

	if ((NewClass->ClassFlags & CLASS_Config) != 0)
	{
		if (ClassDesc->Meta.Contains(NAME_Class_DefaultConfig))
			NewClass->ClassFlags |= CLASS_DefaultConfig;
	}

	NewClass->PropertyLink = SuperClass->PropertyLink;
	NewClass->SetSuperStruct(SuperClass);

#if WITH_EDITOR
	CopyClassInheritedMetaData(SuperClass, NewClass);

	if (!ClassDesc->bIsStaticsClass)
	{
		NewClass->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		NewClass->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
	}

	if (!ClassDesc->Meta.Contains("DisplayName"))
	{
		FString DisplayString = NewClass->GetName();
		DisplayString = FName::NameToDisplayString(DisplayString, false);
		NewClass->SetMetaData(NAME_DisplayName, *DisplayString);
	}

	for (auto& Elem : ClassDesc->Meta)
		NewClass->SetMetaData(Elem.Key, *Elem.Value);

	if (ClassDesc->Meta.Contains(TEXT("NotBlueprintable")))
	{
		NewClass->SetMetaData(TEXT("IsBlueprintBase"), TEXT("false"));
		NewClass->RemoveMetaData(TEXT("Blueprintable"));
	}
	else if (ClassDesc->Meta.Contains(TEXT("Blueprintable")))
	{
		NewClass->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
		NewClass->RemoveMetaData(TEXT("NotBlueprintable"));
	}

	// Don't inherit BlueprintThreadSafe ever
	if (!ClassDesc->Meta.Contains(FUNCMETA_BlueprintThreadSafe))
		NewClass->RemoveMetaData(FUNCMETA_BlueprintThreadSafe);
#endif

	NewClass->ClassWithin = UObject::StaticClass();
	NewClass->Bind();

	if(!ClassDesc->bPlaceable)
		NewClass->ClassFlags |= CLASS_NotPlaceable;
	else
		NewClass->ClassFlags &= ~CLASS_NotPlaceable;

	if (ClassDesc->bAbstract)
		NewClass->ClassFlags |= CLASS_Abstract;

	if (ClassDesc->bTransient)
		NewClass->ClassFlags |= CLASS_Transient;

	if (ClassDesc->bHideDropdown)
		NewClass->ClassFlags |= CLASS_HideDropDown;

	if (ClassDesc->bDefaultToInstanced)
		NewClass->ClassFlags |= CLASS_DefaultToInstanced;

	if (ClassDesc->bEditInlineNew)
		NewClass->ClassFlags |= CLASS_EditInlineNew;

	if (ClassDesc->bIsDeprecatedClass)
		NewClass->ClassFlags |= CLASS_Deprecated;

#if WITH_EDITOR
	FString HideCategories = NewClass->GetMetaData(TEXT("HideCategories"));
	if (!HideCategories.Contains(TEXT(" DefaultComponents")) && SuperClass->IsChildOf(AActor::StaticClass()))
		NewClass->SetMetaData(TEXT("HideCategories"), *(HideCategories + TEXT(" DefaultComponents")));
#endif

	NewClass->bHasASClassParent = ParentASClass != nullptr;

	// Add all properties from angelscript as FProperty to the class
	int32 PropertiesSize = AddClassProperties(ClassDesc);
	int32 MinAlignment = SuperClass->GetMinAlignment();
	//check(PropertiesSize >= SuperClass->GetContainerSize());
	const int32 SuperClassPropertiesSize = Cast<UASClass>(SuperClass) != nullptr
		? CastChecked<UASClass>(SuperClass)->GetContainerSize()
		: SuperClass->GetPropertiesSize();
	check(PropertiesSize >= SuperClassPropertiesSize);

	TArray<UASFunction*> FunctionsWithValidate;

	// Add any functions from angelscript as UFunction to the class
	for (auto FunctionDesc : ClassDesc->Methods)
	{
		FName FunctionName(*FunctionDesc->FunctionName);
		UFunction* ParentFunction = SuperClass->FindFunctionByName(FunctionName);

		asCScriptFunction* ScriptFunction = (asCScriptFunction*)FunctionDesc->ScriptFunction;
		ScriptFunction->isInUse = true;

		// Initialize UFunction object
		auto* NewFunction = UASFunction::AllocateFunctionFor(NewClass, FunctionName, FunctionDesc);
		NewFunction->SetSuperStruct(ParentFunction);
		NewFunction->ReturnValueOffset = MAX_uint16;
		NewFunction->FirstPropertyToInit = NULL;
		//NewFunction->FunctionFlags |= FUNC_RuntimeGenerated;
		NewFunction->ScriptFunction = FunctionDesc->ScriptFunction;
		NewFunction->GeneratedSourceLineNumber = FunctionDesc->LineNumber + 1;
		NewFunction->NumParms = 0;
		NewFunction->ParmsSize = 0;
		NewFunction->bIsNoOp = FunctionDesc->bIsNoOp;

		if (ScriptFunction->traits.GetTrait(asTRAIT_FINAL))
		{
			NewFunction->JitFunction = ScriptFunction->jitFunction;
			NewFunction->JitFunction_Raw = ScriptFunction->jitFunction_Raw;
			NewFunction->JitFunction_ParmsEntry = ScriptFunction->jitFunction_ParmsEntry;
		}
		NewFunction->FunctionFlags |= FUNC_Native;
		NewFunction->SetNativeFunc(&UASFunctionNativeThunk);

		#if WITH_EDITOR
		for (auto& Elem : FunctionDesc->Meta)
			NewFunction->SetMetaData(Elem.Key, *Elem.Value);

		// Record which argument was the mixin argument
		if (ScriptFunction->traits.GetTrait(asTRAIT_MIXIN)
			&& ScriptFunction->parameterNames.GetLength() >= 1)
		{
			FString MixinArgumentName =  ANSI_TO_TCHAR(ScriptFunction->parameterNames[0].AddressOf());
			NewFunction->SetMetaData(NAME_Function_MixinArgument, *MixinArgumentName);
			NewFunction->SetMetaData(NAME_Function_DefaultToSelf, *MixinArgumentName);
		}

		if (NewFunction->bIsNoOp)
			NewFunction->SetMetaData(FUNCMETA_ScriptNoOp, TEXT("true"));
#endif

		FunctionDesc->Function = NewFunction;

#if WITH_EDITOR
		if (FunctionDesc->bIsProtected && FunctionDesc->bBlueprintCallable)
			NewFunction->SetMetaData(FUNCMETA_BlueprintProtected, TEXT("true"));
#endif

		if (FunctionDesc->bBlueprintCallable && !FunctionDesc->bIsPrivate)
			NewFunction->FunctionFlags |= FUNC_BlueprintCallable;
		if ((FunctionDesc->bBlueprintEvent && FunctionDesc->bCanOverrideEvent) || FunctionDesc->bBlueprintOverride)
			NewFunction->FunctionFlags |= FUNC_BlueprintEvent;
		if (FunctionDesc->bBlueprintPure && !FunctionDesc->bIsPrivate)
			NewFunction->FunctionFlags |= FUNC_BlueprintPure;
		if (FunctionDesc->bIsStatic)
			NewFunction->FunctionFlags |= FUNC_Static;
		if (FunctionDesc->bNetMulticast)
			NewFunction->FunctionFlags |= FUNC_NetMulticast;
		if (FunctionDesc->bNetClient)
			NewFunction->FunctionFlags |= FUNC_NetClient;
		if (FunctionDesc->bNetServer)
			NewFunction->FunctionFlags |= FUNC_NetServer;
		if (FunctionDesc->bNetValidate)
		{
			NewFunction->FunctionFlags |= FUNC_NetValidate;
			FunctionsWithValidate.Add(NewFunction);
		}
		if (FunctionDesc->bBlueprintAuthorityOnly)
			NewFunction->FunctionFlags |= FUNC_BlueprintAuthorityOnly;
		if (FunctionDesc->bExec)
			NewFunction->FunctionFlags |= FUNC_Exec;
		if ((NewFunction->FunctionFlags & FUNC_NetFuncFlags) != 0)
		{
			NewFunction->FunctionFlags |= FUNC_Net;
			if (!FunctionDesc->bUnreliable)
				NewFunction->FunctionFlags |= FUNC_NetReliable;
		}
		if (FunctionDesc->bIsConstMethod)
			NewFunction->FunctionFlags |= FUNC_Const;
		if (FunctionDesc->Meta.Contains(NAME_Meta_EditorOnly))
			NewFunction->FunctionFlags |= FUNC_EditorOnly;

		if (ParentFunction)
		{
			// Copy some flags we need from the parent
			NewFunction->FunctionFlags |= (ParentFunction->FunctionFlags & (FUNC_FuncInherit | FUNC_Public | FUNC_Protected | FUNC_Private | FUNC_BlueprintPure | FUNC_HasOutParms));
#if WITH_EDITOR
			FMetaData::CopyMetadata(ParentFunction, NewFunction);

			if (!NewFunction->bIsNoOp)
				NewFunction->RemoveMetaData(FUNCMETA_ScriptNoOp);
#endif
		}

		FProperty* ReturnProperty = AddFunctionReturnType(NewFunction, FunctionDesc->ReturnType);
		if (ReturnProperty != nullptr)
		{
			NewFunction->FunctionFlags |= FUNC_HasOutParms;
		}

		FProperty* WorldContextProperty = nullptr;

		// Generate a hidden world context argument for all static functions by default
		if (FunctionDesc->bIsStatic)
		{
			FString* WorldContextParam = FunctionDesc->Meta.Find(NAME_Arg_WorldContext);

			int32 ParamIndex = -1;
			if (WorldContextParam != nullptr && WorldContextParam->Len() != 0)
			{
				for (int32 ArgIndex = 0, ArgCount = FunctionDesc->Arguments.Num(); ArgIndex < ArgCount; ++ArgIndex)
				{
					if (FunctionDesc->Arguments[ArgIndex].ArgumentName == *WorldContextParam)
					{
						ParamIndex = ArgIndex;
						break;
					}
				}
			}

			if (ParamIndex == -1)
			{
				// No existing world context, generate a fake one
				FAngelscriptArgumentDesc ArgDesc;
				ArgDesc.ArgumentName = TEXT("_World_Context");
				ArgDesc.Type.Type = FAngelscriptType::GetByClass(UObject::StaticClass());

				FProperty* Prop = AddFunctionArgument(NewFunction, ArgDesc, false);
				//Prop->PropertyFlags |= CPF_WorldContext;
				WorldContextProperty = Prop;

#if WITH_EDITOR
				NewFunction->SetMetaData(NAME_Arg_WorldContext, *Prop->GetName());
#endif

				NewFunction->WorldContextIndex = FunctionDesc->Arguments.Num();
				NewFunction->bIsWorldContextGenerated = true;
			}
			else
			{
				NewFunction->WorldContextIndex = ParamIndex;
				NewFunction->bIsWorldContextGenerated = false;
			}
		}

		// Add properties to the function for all arguments
		TArray<FProperty*> ArgumentProperties;
		for (auto& ArgDesc : FunctionDesc->Arguments)
		{
			FProperty* Prop = AddFunctionArgument(NewFunction, ArgDesc);
			ArgumentProperties.Add(Prop);

			if (Prop->HasAnyPropertyFlags(CPF_OutParm))
				NewFunction->FunctionFlags |= FUNC_HasOutParms;
		}

		if (WorldContextProperty != nullptr)
			ArgumentProperties.Add(WorldContextProperty);

		// Link arguments in the correct order
		for (int32 i = ArgumentProperties.Num() - 1; i >= 0; --i)
		{
			auto* NewProperty = ArgumentProperties[i];

			// If we were doing a world context, flag it
			//if (i == NewFunction->WorldContextIndex)
			//	NewProperty->PropertyFlags |= CPF_WorldContext;

			NewProperty->Next = NewFunction->ChildProperties;
			NewFunction->ChildProperties = NewProperty;
		}

#if WITH_EDITOR
		// Set advanced display flag on any properties marked that way
		const FString& AdvancedMeta = NewFunction->GetMetaData(NAME_Arg_AdvancedDisplay);
		if (AdvancedMeta.Len() != 0)
		{
			TArray<FString> AdvancedParams;
			AdvancedMeta.ParseIntoArray(AdvancedParams, TEXT(","), true);

			for (FString& AdvParam : AdvancedParams)
			{
				AdvParam.TrimStartAndEndInline();

				for (FProperty* ArgProperty : ArgumentProperties)
				{
					if (ArgProperty->GetName() == AdvParam)
					{
						ArgProperty->PropertyFlags |= CPF_AdvancedDisplay;
						break;
					}
				}
			}
		}
#endif

		// Link into class
		NewFunction->Next = NewClass->Children;
		NewClass->Children = NewFunction;

		// Add the function to it's owner class function name -> function map
		NewClass->AddFunctionToFunctionMap(NewFunction, NewFunction->GetFName());

		// Link function
		NewFunction->StaticLink(true);
		NewFunction->FinalizeArguments();

		// Record offsets after linking
		if (NewFunction->ReturnArgument.Property != nullptr)
			NewFunction->ReturnValueOffset = NewFunction->ReturnArgument.Property->GetOffset_ForUFunction();

		if (NewFunction->WorldContextIndex >= 0 && ArgumentProperties.IsValidIndex(NewFunction->WorldContextIndex))
		{
			NewFunction->WorldContextOffsetInParms = ArgumentProperties[NewFunction->WorldContextIndex]->GetOffset_ForUFunction();
		}

		if (WorldContextProperty != nullptr)
		{
			// If we generated a world context property ourselves, it is always the last argument
			NewFunction->ParmsSize = WorldContextProperty->GetOffset_ForUFunction() + WorldContextProperty->GetSize();
		}
		else if(NewFunction->Arguments.Num() != 0)
		{
			// Parameter size is the byte count after the last argument
			auto* LastArgProperty = NewFunction->Arguments.Last().Property;
			NewFunction->ParmsSize = LastArgProperty->GetOffset_ForUFunction() + LastArgProperty->GetSize();
		}

		if (NewFunction->ReturnArgument.Property != nullptr)
		{
			const uint16 ReturnEndOffset = static_cast<uint16>(
				NewFunction->ReturnArgument.Property->GetOffset_ForUFunction() + NewFunction->ReturnArgument.Property->GetSize());
			NewFunction->ParmsSize = FMath::Max(NewFunction->ParmsSize, ReturnEndOffset);
		}
	}

	// Cache _Validate functions for all functions requiring validation
	for (auto* Function : FunctionsWithValidate)
	{
		Function->ValidateFunction = NewClass->FindFunctionByName(FName(*(Function->GetName() + TEXT("_Validate"))));
	}

	NewClass->ContainerSize = PropertiesSize;

#if AS_CAN_HOTRELOAD && (WITH_EDITOR || (!UE_BUILD_TEST && !UE_BUILD_SHIPPING))
	// Add some slack for new properties later
	PropertiesSize += 128;
#endif

	NewClass->SetPropertiesSize(PropertiesSize);
	NewClass->StaticLink(false);
	NewClass->AssembleReferenceTokenStream(true);

	NewClass->MinAlignment = MinAlignment;
	NewClass->ScriptPropertyOffset = ParentCodeClass->GetPropertiesSize();
	NewClass->ScriptTypePtr = ScriptType;
	NewClass->OwnerScriptEngine = ScriptType ? ScriptType->GetEngine() : nullptr;
	NewClass->CodeSuperClass = ParentCodeClass;

	// Record data about the class in the descriptor for further pipeline use
	ClassDesc->ScriptType = ScriptType;
	ClassDesc->Class = NewClass;

	UpdateConstructAndDefaultsFunctions(ClassDesc, NewClass);

	// Some properties should be refcounted but are not UProperties, detect them now
	DetectAngelscriptReferences(ClassDesc);

#if WITH_AS_DEBUGVALUES
	CreateDebugValuePrototype(ClassDesc);
#endif

	// Tell the hot-reloader to replace the old class
	if (ReplacedClass != nullptr)
	{
		bReinstancingAny = true;
		ReplacedClass->NewerVersion = NewClass;
	}
	else
	{
		AddedClasses.Add(NewClass);
	}
}

#if WITH_EDITOR
static const TArray<FName> InheritedCategoryKeywords = {
	"ShowCategories",
	"AutoExpandCategories",
	"AutoCollapseCategories",
	"PrioritizeCategories",
	"HideCategories"
};
static const TArray<FName> InheritedMetaData = {
	"HideFunctions",
	"SparseClassDataTypes"
};
static const FName NAME_IgnoreCategoryKeywords("IgnoreCategoryKeywordsInSubclasses");
void FAngelscriptClassGenerator::CopyClassInheritedMetaData(UClass* SuperClass, UClass* NewClass)
{
	auto* SuperMeta = FMetaData::GetMapForObject(SuperClass);
	if (SuperMeta != nullptr)
	{
		// Need to copy, because calling SetMetaData could invalidate the SuperMeta pointer,
		// because it adds a new entry into the metadata map for the new class object.
		TArray<TPair<FName, FString>, TInlineAllocator<8>> CopiedMetaData;

		if (!SuperClass->HasMetaData(NAME_IgnoreCategoryKeywords))
		{
			for (FName MetaName : InheritedCategoryKeywords)
			{
				FString* MetaValue = SuperMeta->Find(MetaName);
				if (MetaValue != nullptr)
					CopiedMetaData.Add(TPair<FName, FString>{MetaName, *MetaValue});
			}
		}

		for (FName MetaName : InheritedMetaData)
		{
			FString* MetaValue = SuperMeta->Find(MetaName);
			if (MetaValue != nullptr)
				CopiedMetaData.Add(TPair<FName, FString>{MetaName, *MetaValue});
		}

		for (auto& MetaPair : CopiedMetaData)
			NewClass->SetMetaData(MetaPair.Key, *MetaPair.Value);
	}
}
#endif

void FAngelscriptClassGenerator::DoFullReload(FModuleData& ModuleData, FEnumData& EnumData)
{
	// Log the reload if needed
	if (EnumData.OldEnum.IsValid())
		UE_LOG(Angelscript, Log, TEXT("Full Reload: %s"), *EnumData.NewEnum->EnumName);

	auto EnumDesc = EnumData.NewEnum;

	UUserDefinedEnum* Enum = nullptr;
	TArray<TPair<FName, int64>> OldNames;

	bool bExistingEnum = EnumData.OldEnum.IsValid() && EnumData.OldEnum->Enum != nullptr;
	bool bHasChanged = true;
	if (bExistingEnum)
	{
		Enum = (UUserDefinedEnum*)EnumData.OldEnum->Enum;

		const int32 EnumeratorsToCopy = Enum->NumEnums() - 1;
		for (int32 Index = 0; Index < EnumeratorsToCopy; Index++)
		{
			FName Name = Enum->GetNameByIndex(Index);
			int64 Value = Enum->GetValueByIndex(Index);
			OldNames.Emplace(Name, Value);

			if (Index >= EnumDesc->EnumValues.Num()
				|| EnumDesc->EnumValues[Index] != Value
				|| EnumDesc->ValueNames[Index] != Name)
			{
				bHasChanged = true;
			}
		}

		if (EnumDesc->EnumValues.Num() != EnumeratorsToCopy)
			bHasChanged = true;

#if WITH_EDITOR
		// Remove old metadata
		for (auto& MetaElement : EnumData.OldEnum->Meta)
		{
			if (!EnumData.NewEnum->Meta.Contains(MetaElement.Key))
			{
				if (MetaElement.Key.Value < Enum->NumEnums())
					Enum->RemoveMetaData(*MetaElement.Key.Key.ToString(), MetaElement.Key.Value);
			}
		}
#endif
	}
	else
	{
		LLM_SCOPE_BYTAG(Angelscript);
		Enum = NewObject<UUserDefinedEnum>(
			FAngelscriptEngine::GetPackage(),
			UUserDefinedEnum::StaticClass(),
			FName(*EnumDesc->EnumName),
			RF_Public | RF_Standalone | RF_MarkAsRootSet
		);

		TArray<TPair<FName, int64>> EmptyNames;
		Enum->SetEnums(EmptyNames, UEnum::ECppForm::Namespaced, UEnum::EUnderlyingType::uint8, EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::Yes);

#if WITH_EDITOR
		Enum->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
#endif

		// Tell the loading system the enum exists
		NotifyRegistrationEvent(TEXT("/Script/Angelscript"), *EnumDesc->EnumName, ENotifyRegistrationType::NRT_Enum,
			ENotifyRegistrationPhase::NRP_Finished, nullptr, false, Enum);
	}

	if (bHasChanged)
	{
		EnumDesc->Enum = Enum;

		TArray<TPair<FName, int64>> Values;
		for (int32 i = 0, Count = EnumDesc->ValueNames.Num(); i < Count; ++i)
		{
			const FString FullNameStr = Enum->GenerateFullEnumName(*EnumDesc->ValueNames[i].ToString());
			Values.Emplace(*FullNameStr, EnumDesc->EnumValues[i]);
		}

		Enum->SetEnums(Values, UEnum::ECppForm::Namespaced, UEnum::EUnderlyingType::uint8, EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::Yes);

		for (int32 i = 0, Count = Values.Num(); i < Count; ++i)
		{
			FText DisplayName;

#if WITH_EDITOR
			auto* DisplayNameMeta = EnumDesc->Meta.Find(TPair<FName,int32>(NAME_DisplayName, i));
			if (DisplayNameMeta != nullptr)
				DisplayName = FText::FromString(*DisplayNameMeta);
			else
#endif
				DisplayName = FText::FromName(Values[i].Key);

			Enum->DisplayNameMap.Add(Values[i].Key, DisplayName);
		}
	}

#if WITH_EDITOR
	// Add specified metadata
	for (auto& MetaElement : EnumDesc->Meta)
	{
		if (MetaElement.Key.Value == INDEX_NONE)
		{
			Enum->SetMetaData(*MetaElement.Key.Key.ToString(), *MetaElement.Value);
		}
		else if (MetaElement.Key.Value < Enum->NumEnums())
		{
			Enum->SetMetaData(*MetaElement.Key.Key.ToString(), *MetaElement.Value, MetaElement.Key.Value);
		}
	}
#endif

#if WITH_EDITOR
	if (EnumDesc->Documentation.Len() != 0)
	{
		// Add documentation about the enum itself
		Enum->SetMetaData(TEXT("ToolTip"), *EnumDesc->Documentation);
	}
#endif

	// Set the enum on the script type
	EnumDesc->ScriptType->SetUserData(Enum);

	// Need to inform editor if we changed an existing enum
	if (!bExistingEnum)
	{
		if (FAngelscriptEngine::Get().IsInitialCompileFinished())
		{
			if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
				HookEngine->GetOnEnumCreated().Broadcast(Enum);
		}
	}
	else if (bHasChanged)
	{
		if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
			HookEngine->GetOnEnumChanged().Broadcast(Enum, OldNames);
	}
}

void FAngelscriptClassGenerator::DoFullReload(FModuleData& Module, FDelegateData& DelegateData)
{
	auto FunctionDesc = DelegateData.NewDelegate->Signature;
	UDelegateFunction* NewFunction = DelegateData.NewDelegate->Function;
	NewFunction->ReturnValueOffset = MAX_uint16;
	NewFunction->FirstPropertyToInit = NULL;
	//NewFunction->FunctionFlags |= FUNC_RuntimeGenerated;
	NewFunction->NumParms = 0;
	NewFunction->ParmsSize = 0;

	NewFunction->FunctionFlags |= FUNC_Delegate;
	if (DelegateData.NewDelegate->bIsMulticast)
		NewFunction->FunctionFlags |= FUNC_MulticastDelegate;

	auto* ReturnProp = AddFunctionReturnType(NewFunction, FunctionDesc->ReturnType);

	// Add properties to the function for all arguments
	TArray<FProperty*> ArgumentProperties;
	for (auto& ArgDesc : FunctionDesc->Arguments)
	{
		FProperty* Prop = AddFunctionArgument(NewFunction, ArgDesc);
		ArgumentProperties.Add(Prop);
	}

	// Link arguments in the correct order
	for (int32 i = ArgumentProperties.Num() - 1; i >= 0; --i)
	{
		auto* NewProperty = ArgumentProperties[i];
		NewProperty->Next = NewFunction->ChildProperties;
		NewFunction->ChildProperties = NewProperty;
	}

	// Link function
	NewFunction->StaticLink(true);

	// Record offsets after linking
	if (ReturnProp != nullptr)
		NewFunction->ReturnValueOffset = ReturnProp->GetOffset_ForUFunction();
	for (FProperty* ArgProp : ArgumentProperties)
		NewFunction->ParmsSize = ArgProp->GetOffset_ForUFunction() + ArgProp->GetSize();

	// Tell unreal about the change
	if (DelegateData.OldDelegate.IsValid() && DelegateData.OldDelegate->Function != nullptr)
	{
		if (FAngelscriptEngine* HookEngine = FAngelscriptEngine::TryGetCurrentEngine())
			HookEngine->GetOnDelegateReload().Broadcast(DelegateData.OldDelegate->Function, NewFunction);
	}
}

