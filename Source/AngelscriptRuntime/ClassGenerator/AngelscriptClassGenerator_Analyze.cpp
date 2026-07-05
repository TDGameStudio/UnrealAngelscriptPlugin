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

void FAngelscriptClassGenerator::Analyze(FModuleData& ModuleData, FClassData& ClassData)
{
	// Ignore if we've already analyzed this class
	if (ClassData.bAnalyzed)
		return;

	const bool bLoadedPrecompiledCode = ModuleData.NewModule->bLoadedPrecompiledCode;

	// Modules that previously had swap-in errors should always full reload
	if (ModuleData.OldModule.IsValid() && ModuleData.OldModule->bModuleSwapInError && ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
		ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;

	// Resolve the compiled script type for the class
	auto* ScriptType = ClassData.NewClass->ScriptType;

	if (ClassData.OldClass.IsValid() && ClassData.OldClass->ScriptType)
		UpdatedScriptTypeMap.Add(ClassData.OldClass->ScriptType, ScriptType);

	// Make sure the superclass has been analyzed
	TSharedPtr<FAngelscriptClassDesc> AngelscriptSuperClass;
	if (!ClassData.NewClass->bSuperIsCodeClass)
		AngelscriptSuperClass = EnsureClassAnalyzed(ClassData.NewClass->SuperClass);

	// Analyze all properties in the class
	TMap<FString, int32> PropertyIndexMap;
	TArray<FAngelscriptTypeUsage> PropertyTypes;
	if (ScriptType != nullptr)
	{
		int32 PropertyCount = ScriptType->GetPropertyCount();
		PropertyTypes.SetNum(PropertyCount);
		for (int32 i = 0; i < PropertyCount; ++i)
		{
			// Skip inherited properties here, they will be handled by the parent class
			if (ScriptType->IsPropertyInherited(i))
				continue;

			const char* Name;
			ScriptType->GetProperty(i, &Name);

			FString ScriptPropertyName = ANSI_TO_TCHAR(Name);
			PropertyIndexMap.Add(ScriptPropertyName, i);

			auto PropertyType = FAngelscriptTypeUsage::FromProperty(ScriptType, i);
			PropertyTypes[i] = PropertyType;
		}
	}

	// Make sure the previous class is actually replacable
	FString UnrealName = GetUnrealName(ClassData.NewClass->bIsStruct, ClassData.NewClass->ClassName);
	UObject* ReplacedObj = FindObject<UObject>(FAngelscriptEngine::GetPackage(), *UnrealName);
	if (ReplacedObj != nullptr)
	{
		if (ClassData.NewClass->bIsStruct)
		{
			if (!ReplacedObj->IsA<UASStruct>())
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ClassData.NewClass->LineNumber, FString::Printf(
					TEXT("Struct %s has a name conflict with non-struct unreal object %s."),
					*ClassData.NewClass->ClassName, *ReplacedObj->GetPathName()));
				ClassData.ReloadReq = EReloadRequirement::Error;
			}
			else
			{
				UASStruct* ReplacedStruct = CastChecked<UASStruct>(ReplacedObj);
				const bool bIsCurrentReloadedStruct = ClassData.OldClass.IsValid() && ReplacedStruct == ClassData.OldClass->Struct;
				auto* ReplacedScriptType = bIsCurrentReloadedStruct ? nullptr : (asITypeInfo*)ReplacedStruct->ScriptType;
				if (ReplacedScriptType != nullptr)
				{
					auto Module = FAngelscriptEngine::Get().GetModule(ReplacedScriptType->GetModule());
					if (Module.IsValid() && !IsReloadingModule(Module))
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ClassData.NewClass->LineNumber, FString::Printf(
							TEXT("Name conflict: unreal name %s for script type %s is already in use in module %s."),
							*UnrealName, *ClassData.NewClass->ClassName, *Module->ModuleName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
				}
			}
		}
		else
		{
			if (!ReplacedObj->IsA<UASClass>())
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ClassData.NewClass->LineNumber, FString::Printf(
					TEXT("Class %s has a name conflict with non-class unreal object %s."),
					*ClassData.NewClass->ClassName, *ReplacedObj->GetPathName()));
				ClassData.ReloadReq = EReloadRequirement::Error;
			}
			else
			{
				UASClass* ReplacedClass = CastChecked<UASClass>(ReplacedObj);
				const bool bIsCurrentReloadedClass = ClassData.OldClass.IsValid() && ReplacedClass == ClassData.OldClass->Class;
				auto* ReplacedScriptType = bIsCurrentReloadedClass ? nullptr : (asITypeInfo*)ReplacedClass->ScriptTypePtr;
				if (ReplacedScriptType != nullptr)
				{
					auto Module = FAngelscriptEngine::Get().GetModule(ReplacedScriptType->GetModule());
					if (Module.IsValid() && !IsReloadingModule(Module))
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ClassData.NewClass->LineNumber, FString::Printf(
							TEXT("Name conflict: unreal name %s for script type %s is already in use in module %s."),
							*UnrealName, *ClassData.NewClass->ClassName, *Module->ModuleName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
				}
			}
		}
	}

	if (UsedUnrealNames.Contains(UnrealName))
	{
		FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ClassData.NewClass->LineNumber, FString::Printf(
			TEXT("Name conflict: unreal name %s for script type %s is already in use."),
			*UnrealName, *ClassData.NewClass->ClassName));
		ClassData.ReloadReq = EReloadRequirement::Error;
	}
	UsedUnrealNames.Add(UnrealName);

	auto AngelscriptSettings = UAngelscriptSettings::StaticClass()->GetDefaultObject<UAngelscriptSettings>();

	// Some properties without a UPROPERTY() should be added as 
	// hidden properties, for garbage collection purposes.
	//  This will be the case for all properties in structs,
	//  as well as any properties whose type is RequiresProperty()
	for (auto& Elem : PropertyIndexMap)
	{
		bool bShouldMakeProperty = false;

		FAngelscriptTypeUsage PropertyType = PropertyTypes[Elem.Value];

		if (ClassData.NewClass->bIsStruct)
			bShouldMakeProperty = !PropertyType.NeverRequiresGC();

		if (PropertyType.RequiresProperty())
			bShouldMakeProperty = true;

		if (!bShouldMakeProperty)
			continue;

		// Skip properties that are already added
		if (ClassData.NewClass->GetProperty(Elem.Key).IsValid())
			continue;

		// Show an error if we can't create a UPROPERTY for this type
		if (!PropertyType.CanCreateProperty())
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ClassData.NewClass->LineNumber, FString::Printf(
				TEXT("Property %s with type %s is in a context where a UPROPERTY must be generated for GC reasons, but the property type is not supported by UPROPERTY."),
				*Elem.Key, *PropertyType.GetAngelscriptDeclaration()));
			ClassData.ReloadReq = EReloadRequirement::Error;
			continue;
		}

		// Add new property
		auto PropDesc = MakeShared<FAngelscriptPropertyDesc>();
		PropDesc->PropertyName = Elem.Key;
		PropDesc->bBlueprintReadable = false;
		PropDesc->bBlueprintWritable = false;
		PropDesc->bEditableOnDefaults = false;
		PropDesc->bEditableOnInstance = false;

		if (AngelscriptSettings->bMarkNonUpropertyPropertiesAsTransient || !ClassData.NewClass->bIsStruct)
		{
			// Properties without a UPROPERTY macro are marked as Transient by default to avoid unintentional serialization
			PropDesc->bTransient = true;
		}

		ClassData.NewClass->Properties.Add(PropDesc);
	}

	for (auto PropertyDesc : ClassData.NewClass->Properties)
	{
		// Check if this property should be added as a FProperty
		bool bFound = false;

		int32* ScriptIndexPtr = PropertyIndexMap.Find(PropertyDesc->PropertyName);
		if (ScriptIndexPtr != nullptr)
		{
			int32 ScriptIndex = *ScriptIndexPtr;
			const char* Name;
			int PropertyOffset;
			int TypeId;

			bool bIsPrivate;
			bool bIsProtected;

			ScriptType->GetProperty(ScriptIndex, &Name, &TypeId, &bIsPrivate, &bIsProtected, &PropertyOffset);

			auto PropertyType = PropertyTypes[ScriptIndex];
			if (!PropertyType.IsValid() || !PropertyType.CanCreateProperty())
			{
				// Emit an error, this type is not valid for usage as FProperty
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, PropertyDesc->LineNumber, FString::Printf(
					TEXT("Property %s %s in class %s has a type that cannot be a UPROPERTY."),
					*PropertyType.GetAngelscriptDeclaration(), *PropertyDesc->PropertyName, *ClassData.NewClass->ClassName));
				ModuleData.ReloadReq = EReloadRequirement::Error;
				break;
			}

			PropertyDesc->PropertyType = PropertyType;
			PropertyDesc->ScriptPropertyIndex = ScriptIndex;
			PropertyDesc->ScriptPropertyOffset = (SIZE_T)PropertyOffset;

			PropertyDesc->bIsPrivate = bIsPrivate;
			PropertyDesc->bIsProtected = bIsProtected;

#if WITH_EDITOR
			if (PropertyDesc->Meta.Contains(NAME_Actor_DefaultComponent))
			{
				// If the property is a default component and a subobject of that name exists in the code parent, error
				UClass* CodeSuperClass = ClassData.NewClass->CodeSuperClass;
				UObject* CodeCDO = CodeSuperClass != nullptr ? CodeSuperClass->GetDefaultObject() : nullptr;
				if (CodeCDO != nullptr && CodeCDO->GetDefaultSubobjectByName(*PropertyDesc->PropertyName) != nullptr)
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, PropertyDesc->LineNumber, FString::Printf(
						TEXT("Component with name %s in class %s already exists in parent class."), *PropertyDesc->PropertyName, *ClassData.NewClass->ClassName));
					ClassData.ReloadReq = EReloadRequirement::Error;
				}

				// Default component properties can only be subclasses of UActorComponent
				UClass* PropertyCodeSuper = ResolveCodeSuperForProperty(PropertyType);
				if (PropertyCodeSuper == nullptr || !PropertyCodeSuper->IsChildOf(UActorComponent::StaticClass()))
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, PropertyDesc->LineNumber, FString::Printf(
						TEXT("DefaultComponent with name %s and type %s in class %s does not derive from UActorComponent."),
						*PropertyDesc->PropertyName, *PropertyType.GetAngelscriptDeclaration(), *ClassData.NewClass->ClassName));
					ClassData.ReloadReq = EReloadRequirement::Error;
				}

				// Fail closed before swap-in if the attach target can't be resolved from this class or its inherited CDO.
				FString* AttachParentName = PropertyDesc->Meta.Find(NAME_Actor_Attach);
				if (AttachParentName != nullptr
					&& PropertyCodeSuper != nullptr
					&& PropertyCodeSuper->IsChildOf(USceneComponent::StaticClass()))
				{
					bool bAttachParentExists = false;
					if (TSharedPtr<FAngelscriptPropertyDesc> AttachProperty = ClassData.NewClass->GetProperty(*AttachParentName); AttachProperty.IsValid())
					{
						bAttachParentExists = AttachProperty->Meta.Contains(NAME_Actor_DefaultComponent);
					}

					if (!bAttachParentExists)
					{
						bAttachParentExists = CodeCDO->GetDefaultSubobjectByName(**AttachParentName) != nullptr;
					}

					if (!bAttachParentExists)
					{
						FAngelscriptEngine::Get().ScriptCompileError(
							ModuleData.NewModule,
							PropertyDesc->LineNumber,
							FString::Printf(
								TEXT("Attach parent %s does not exist for DefaultComponent %s."),
								**AttachParentName,
								*PropertyDesc->PropertyName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
				}
			}

			if (PropertyDesc->Meta.Contains(NAME_Actor_OverrideComponent))
			{
				UClass* PropertyCodeSuper = ResolveCodeSuperForProperty(PropertyType);
				FString* OverrideComponentName = PropertyDesc->Meta.Find(NAME_Actor_OverrideComponent);
				if (OverrideComponentName != nullptr
					&& PropertyCodeSuper != nullptr
					&& PropertyCodeSuper->IsChildOf(UActorComponent::StaticClass()))
				{
					bool bOverrideTargetExists = false;

					TSharedPtr<FAngelscriptClassDesc> CheckSuperClass = AngelscriptSuperClass;
					while (CheckSuperClass.IsValid())
					{
						if (TSharedPtr<FAngelscriptPropertyDesc> OverrideProperty = CheckSuperClass->GetProperty(*OverrideComponentName); OverrideProperty.IsValid())
						{
							bOverrideTargetExists = OverrideProperty->Meta.Contains(NAME_Actor_DefaultComponent);
						}

						if (bOverrideTargetExists || CheckSuperClass->bSuperIsCodeClass)
						{
							break;
						}

						CheckSuperClass = EnsureClassAnalyzed(CheckSuperClass->SuperClass);
					}

					if (!bOverrideTargetExists)
					{
						for (UClass* CheckCodeSuperClass = ClassData.NewClass->CodeSuperClass;
							CheckCodeSuperClass != nullptr;
							CheckCodeSuperClass = CheckCodeSuperClass->GetSuperClass())
						{
							UObject* CheckCodeCDO = CheckCodeSuperClass->GetDefaultObject();
							if (CheckCodeCDO != nullptr
								&& CheckCodeCDO->GetDefaultSubobjectByName(**OverrideComponentName) != nullptr)
							{
								bOverrideTargetExists = true;
								break;
							}

							for (TFieldIterator<FProperty> It(CheckCodeSuperClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
							{
								FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(*It);
								if (ObjectProperty != nullptr
									&& ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference)
									&& ObjectProperty->GetFName() == **OverrideComponentName
									&& ObjectProperty->PropertyClass != nullptr
									&& ObjectProperty->PropertyClass->HasAllClassFlags(CLASS_Abstract))
								{
									bOverrideTargetExists = true;
									break;
								}
							}

							if (bOverrideTargetExists)
							{
								break;
							}
						}
					}

					if (!bOverrideTargetExists)
					{
						FAngelscriptEngine::Get().ScriptCompileError(
							ModuleData.NewModule,
							PropertyDesc->LineNumber,
							FString::Printf(
								TEXT("OverrideComponent %s::%s could not find component %s in base class to override."),
								*ClassData.NewClass->ClassName,
								*PropertyDesc->PropertyName,
								**OverrideComponentName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
				}
			}
#endif
		}
		else
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, PropertyDesc->LineNumber, FString::Printf(
				TEXT("Could not find property %s in class %s to be a UPROPERTY()."), *PropertyDesc->PropertyName, *ClassData.NewClass->ClassName));
			ClassData.ReloadReq = EReloadRequirement::Error;
		}
	}

	// Collect all functions in the script class for later lookup
	TMap<FString, TArray<asCScriptFunction*>> FunctionMap;
	if (ScriptType != nullptr)
	{
		for (int32 i = 0, MethodCount = ScriptType->GetMethodCount(); i < MethodCount; ++i)
		{
			asCScriptFunction* ScriptFunction = (asCScriptFunction*)ScriptType->GetMethodByIndex(i);

			// Don't consider functions declared in superclasses here, they will be bound by
			// the superclass when we analyze that.
			if (ScriptFunction->GetObjectType() != ScriptType)
				continue;

			FString FunctionName = ANSI_TO_TCHAR(ScriptFunction->GetName());
			FunctionMap.FindOrAdd(FunctionName).Add(ScriptFunction);
		}
	}
	else
	{
		asCModule* ScriptModule = (asCModule*)ModuleData.NewModule->ScriptModule;
		for (int32 i = 0, FunctionCount = ScriptModule->GetFunctionCount(); i < FunctionCount; ++i)
		{
			asCScriptFunction* ScriptFunction = (asCScriptFunction*)ScriptModule->GetFunctionByIndex(i);

			FString FunctionName = ANSI_TO_TCHAR(ScriptFunction->GetName());
			FunctionMap.FindOrAdd(FunctionName).Add(ScriptFunction);
		}
	}

#if WITH_EDITOR
	// Expensive check, so editor only. Check to make sure that 
	// functions that we expect to be UFUNCTION(BlueprintOverride) are
	// actually tagged as such.
	{
		UClass* CodeSuperClass = ClassData.NewClass->CodeSuperClass;

		for (auto& Elem : FunctionMap)
		{
			auto FunctionDesc = ClassData.NewClass->GetMethod(Elem.Key);
			if (FunctionDesc.IsValid() && FunctionDesc->bBlueprintOverride)
				continue;

			bool bHaveParentFunction = false;
			bool bParentIsEvent = false;
			bool bParentIsCpp = false;
			FString ParentName;

			auto* UnrealParentFunction = GetBlueprintEventByScriptName(CodeSuperClass, Elem.Key);
			if (UnrealParentFunction != nullptr)
			{
				bHaveParentFunction = true;
				bParentIsCpp = true;
				bParentIsEvent = UnrealParentFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent);
				ParentName = UnrealParentFunction->GetOuterUClass()->GetName();
			}

			if (!bHaveParentFunction)
			{
				auto CheckSuperClass = AngelscriptSuperClass;
				while (CheckSuperClass.IsValid())
				{
					auto ScriptParentFunction = CheckSuperClass->GetMethod(Elem.Key);
					if (ScriptParentFunction.IsValid())
					{
						bHaveParentFunction = true;
						ParentName = CheckSuperClass->ClassName;

						if (ScriptParentFunction->bBlueprintOverride)
							bParentIsEvent = true;
						else if (ScriptParentFunction->bBlueprintEvent)
							bParentIsEvent = true;
						break;
					}

					if (CheckSuperClass->bSuperIsCodeClass)
						break;

					CheckSuperClass = EnsureClassAnalyzed(CheckSuperClass->SuperClass);
				}
			}

			if (bHaveParentFunction && (FunctionDesc.IsValid() || bParentIsEvent))
			{
				int32 LineNumber = 0;
				auto* ScriptFunction = (asCScriptFunction*)Elem.Value[0];
				if (ScriptFunction != nullptr && ScriptFunction->scriptData != nullptr)
					LineNumber = ScriptFunction->scriptData->declaredAt & 0xFFFFF;

				if (bParentIsEvent)
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, LineNumber, FString::Printf(
						TEXT("Method %s is a BlueprintEvent in parent class %s, override requires the BlueprintOverride function specifier."),
						*Elem.Key, *ParentName));
				}
				else if (bParentIsCpp)
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, LineNumber, FString::Printf(
						TEXT("Method %s already exists in parent class %s, but is not a BlueprintEvent and cannot be overridden."),
						*Elem.Key, *ParentName));
				}
				else
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, LineNumber, FString::Printf(
						TEXT("Method %s is already a UFUNCTION in parent class %s. Use the BlueprintEvent/BlueprintOverride specifiers, or remove UFUNCTION and use the angelscript `override` keyword."),
						*Elem.Key, *ParentName));
				}

				ClassData.ReloadReq = EReloadRequirement::Error;
			}
		}
	}
#endif

	// Determine whether the class is threadsafe
	// Determine if the function is thread safe or in a thread safe class
	const bool bClassThreadSafe = ClassData.NewClass->Meta.Contains(FUNCMETA_BlueprintThreadSafe);

	// Analyze all the functions we want to bind
	for (auto FunctionDesc : ClassData.NewClass->Methods)
	{
		// If there are multiple script functions with this name,
		// we can't bind.
		auto* FunctionList = FunctionMap.Find(FunctionDesc->ScriptFunctionName);
		if (FunctionList == nullptr)
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
				TEXT("Could not find function %s in class %s to be a UFUNCTION()."), *FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
			ClassData.ReloadReq = EReloadRequirement::Error;
			continue;
		}
		if (FunctionList->Num() != 1)
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
				TEXT("Multiple methods with name %s in class %s found. UFUNCTION()s must have unique names."), *FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
			ClassData.ReloadReq = EReloadRequirement::Error;
			continue;
		}

		asCScriptFunction* ScriptFunction = (asCScriptFunction*)(*FunctionList)[0];

		FunctionDesc->ScriptFunction = ScriptFunction;
		FunctionDesc->bIsPrivate = ScriptFunction->IsPrivate();
		FunctionDesc->bIsProtected = ScriptFunction->IsProtected();

		if (!bLoadedPrecompiledCode)
		{
			if (bClassThreadSafe)
				FunctionDesc->bThreadSafe = !FunctionDesc->Meta.Contains(FUNCMETA_NotBlueprintThreadSafe);
			else
				FunctionDesc->bThreadSafe = FunctionDesc->Meta.Contains(FUNCMETA_BlueprintThreadSafe);

			if (FunctionDesc->bBlueprintEvent || FunctionDesc->bBlueprintOverride)
				FunctionDesc->bIsNoOp = ScriptFunction->IsNoOp();
		}

		if (ScriptFunction->IsReadOnly())
			FunctionDesc->bIsConstMethod = true;

		int32 ReturnTypeId = ScriptFunction->GetReturnTypeId();
		if (ReturnTypeId != asTYPEID_VOID)
		{
			FunctionDesc->ReturnType = FAngelscriptTypeUsage::FromReturn(ScriptFunction);
			if (!FunctionDesc->ReturnType.IsValid() || !FunctionDesc->ReturnType.CanCreateProperty() || !FunctionDesc->ReturnType.CanBeReturned() || FunctionDesc->ReturnType.bIsReference)
			{
				if (FunctionDesc->ReturnType.bIsReference)
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
						TEXT("UFUNCTIONs cannot return references, function %s in class %s"), *FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
				}
				else
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
						TEXT("Unknown or invalid return type to function %s in class %s"), *FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
				}

				ClassData.ReloadReq = EReloadRequirement::Error;
				continue;
			}
		}

		int32 ArgCount = ScriptFunction->GetParamCount();
		for (int32 i = 0; i < ArgCount; ++i)
		{
			const char* ParamName = nullptr;
			const char* ParamDefaultValue = nullptr;
			asDWORD RefFlags = 0;
			ScriptFunction->GetParam(i, nullptr, &RefFlags, &ParamName, &ParamDefaultValue);

			auto Type = FAngelscriptTypeUsage::FromParam(ScriptFunction, i);
			if (!Type.IsValid() || !Type.CanBeArgument() || !Type.CanCreateProperty())
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
					TEXT("Unknown or invalid parameter type for parameter %s to function %s in class %s"), 
					ANSI_TO_TCHAR(ParamName), *FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));

				ClassData.ReloadReq = EReloadRequirement::Error;
				break;
			}

			FAngelscriptArgumentDesc ArgDesc;
			ArgDesc.Type = Type;
			ArgDesc.ArgumentName = ANSI_TO_TCHAR(ParamName);
			ArgDesc.DefaultValue = ANSI_TO_TCHAR(ParamDefaultValue);

			// Some types of arguments are forced to be outparam refs
			if (Type.IsValid() && Type.Type->IsParamForcedOutParam() && Type.bIsConst)
			{
				ArgDesc.bInRefForceCopyOut = true;
				ArgDesc.bBlueprintInRef = true;
			}
			else if (Type.bIsReference)
			{
				if ((RefFlags & asTM_INOUTREF) == asTM_INOUTREF)
				{
					if (Type.bIsConst)
					{
						ArgDesc.bBlueprintByValue = true;
					}
					else
					{
						ArgDesc.bBlueprintInRef = true;
					}
				}
				else if ((RefFlags & asTM_OUTREF) != 0)
				{
					ArgDesc.bBlueprintOutRef = true;
				}
				else
				{
					ArgDesc.bBlueprintInRef = true;
				}
			}
			else
			{
				ArgDesc.bBlueprintByValue = true;
			}

			// Object arguments named WorldContext are automatically marked
			// as world context pins, if we don't have one yet.
			if (Type.Type->IsObjectPointer() && ArgDesc.ArgumentName.Equals(STR_Arg_WorldContext, ESearchCase::IgnoreCase))
			{
				if (!FunctionDesc->Meta.Contains(NAME_Arg_WorldContext))
					FunctionDesc->Meta.Add(NAME_Arg_WorldContext, ArgDesc.ArgumentName);
			}

			FunctionDesc->Arguments.Add(ArgDesc);
		}

		// Check that BlueprintPure has a return value
		if (FunctionDesc->bBlueprintPure)
		{
			bool bHasOutParams = false;

			if (FunctionDesc->ReturnType.IsValid())
				bHasOutParams = true;

			for (auto& Param : FunctionDesc->Arguments)
			{
				if (Param.Type.bIsReference && !Param.Type.bIsConst)
					bHasOutParams = true;
			}

			if (!bHasOutParams)
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
					TEXT("BlueprintPure method %s in class %s must have return value."), *FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
				ClassData.ReloadReq = EReloadRequirement::Error;
			}
		}

		UClass* CodeSuperClass = ClassData.NewClass->CodeSuperClass;

		// Check that BlueprintCallable/BlueprintEvent doesn't bump into a superclass function
		if ((FunctionDesc->bBlueprintCallable || FunctionDesc->bBlueprintEvent) && !FunctionDesc->bBlueprintOverride)
		{
			UFunction* ParentFunction = CodeSuperClass->FindFunctionByName(*FunctionDesc->FunctionName);
			if (ParentFunction != nullptr)
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
					TEXT("%s method %s in class %s already specified in superclass %s."),
					FunctionDesc->bBlueprintEvent ? TEXT("BlueprintEvent") : TEXT("BlueprintCallable"),
					*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName, *ClassData.NewClass->SuperClass));
				ClassData.ReloadReq = EReloadRequirement::Error;
			}
			else
			{
				if (AngelscriptSuperClass.IsValid())
				{
					TSharedPtr<FAngelscriptFunctionDesc> ParentScriptFunction = AngelscriptSuperClass->GetMethod(FunctionDesc->FunctionName);
					if (!FunctionDesc->bBlueprintEvent)
					{
						if (ParentScriptFunction.IsValid() && 
							((!ParentScriptFunction->SignatureMatches(FunctionDesc) && ParentScriptFunction->bBlueprintCallable)
								 || ParentScriptFunction->bBlueprintEvent))
						{
							// Function exists in parent script class, but with a different signature
							FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
								TEXT("BlueprintCallable method %s in class %s is specified in superclass %s with a different signature."),
								*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName, *ClassData.NewClass->SuperClass));
							ClassData.ReloadReq = EReloadRequirement::Error;
						}
					}
					else
					{
						if (ParentScriptFunction.IsValid() && (ParentScriptFunction->bBlueprintCallable || ParentScriptFunction->bBlueprintEvent))
						{
							// Function exists in parent script class, but with a different signature
							FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
								TEXT("BlueprintEvent method %s in class %s is already specified in superclass %s."),
								*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName, *ClassData.NewClass->SuperClass));
							ClassData.ReloadReq = EReloadRequirement::Error;
						}
					}

				}
			}
		}

		// Check that BlueprintOverride actually overrides something from a superclass
		if (FunctionDesc->bBlueprintOverride)
		{
			// Check if we should use a displayname override for this function
			FunctionDesc->OriginalFunctionName = FunctionDesc->FunctionName;
			auto* ParentFunction = GetBlueprintEventByScriptName(CodeSuperClass, FunctionDesc->FunctionName);
			if (ParentFunction != nullptr)
				FunctionDesc->FunctionName = ParentFunction->GetName();

			if (ParentFunction == nullptr)
			{
				if (AngelscriptSuperClass.IsValid())
				{
					// Check if our angelscript superclass will create this event before we override it
					TSharedPtr<FAngelscriptClassDesc> CheckSuperClass = AngelscriptSuperClass;
					TSharedPtr<FAngelscriptFunctionDesc> SuperFunctionDesc;

					while (CheckSuperClass.IsValid())
					{
						SuperFunctionDesc = CheckSuperClass->GetMethod(FunctionDesc->FunctionName);
						if (SuperFunctionDesc.IsValid())
							break;

						if (!CheckSuperClass->bSuperIsCodeClass)
							CheckSuperClass = EnsureClassAnalyzed(CheckSuperClass->SuperClass);
						else
							break;
					}

					if (!SuperFunctionDesc.IsValid())
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
							TEXT("BlueprintOverride method %s in class %s does not exist in superclass %s."),
							*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName, *AngelscriptSuperClass->ClassName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
					else if (!SuperFunctionDesc->bBlueprintEvent && !SuperFunctionDesc->bBlueprintOverride)
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
							TEXT("BlueprintOverride method %s in class %s is not marked BlueprintEvent in superclass %s."),
							*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName, *AngelscriptSuperClass->ClassName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
					else if (!SuperFunctionDesc->SignatureMatches(FunctionDesc))
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
							TEXT("BlueprintOverride method %s in class %s does not match signature of event declared in superclass %s."),
							*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName, *AngelscriptSuperClass->ClassName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
					else if (SuperFunctionDesc->Meta.Contains(NAME_Meta_EditorOnly) && !FunctionDesc->Meta.Contains(NAME_Meta_EditorOnly))
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
							TEXT("BlueprintOverride method %s in class %s overrides an editor-only parent function, but is not in editor-only code."),
							*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
				}
				else
				{
					if (auto* NonEvent = CodeSuperClass->FindFunctionByName(*FunctionDesc->FunctionName))
					{
#if WITH_EDITOR
						bool bShouldUseScriptName = false;
						FString ScriptName = FAngelscriptFunctionSignature::GetScriptNameForFunction(NonEvent);

						if (NonEvent->HasAnyFunctionFlags(FUNC_BlueprintEvent))
						{
							if (ScriptName != FunctionDesc->FunctionName
								&& GetBlueprintEventByScriptName(CodeSuperClass, ScriptName) != nullptr)
							{
								bShouldUseScriptName = true;
							}
						}

						if (bShouldUseScriptName)
						{
							FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
								TEXT("Use name `%s` instead to override C++ event %s in parent class %s, as it has a ScriptName or stripped prefix."),
								*ScriptName, *NonEvent->GetName(), *NonEvent->GetOwnerClass()->GetName()));
							ClassData.ReloadReq = EReloadRequirement::Error;
						}
						else
#endif
						{
							FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
								TEXT("Method %s in parent class %s is not a BlueprintImplementableEvent or BlueprintNativeEvent in C++ and cannot be overridden."),
								*NonEvent->GetName(), *NonEvent->GetOwnerClass()->GetName()));
							ClassData.ReloadReq = EReloadRequirement::Error;
						}
					}
					else
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
							TEXT("BlueprintOverride method %s in class %s does not exist in superclass %s, or is not a BlueprintImplementableEvent or BlueprintNativeEvent in C++."),
							*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName, *CodeSuperClass->GetName()));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
				}
			}
			else
			{
				// Make sure our function signature is the same as the event we're overriding
				bool bASReturnsVoid = !FunctionDesc->ReturnType.IsValid();
				bool bUEReturnsVoid = ParentFunction->GetReturnProperty() == nullptr;

				bool bArgCountMismatch = false;
				bool bTypeMismatch = false;
				if (bASReturnsVoid != bUEReturnsVoid)
				{
					bTypeMismatch = true;
				}

				if (!bASReturnsVoid && !bUEReturnsVoid)
				{
					// If the UE return value is a float, but the script one is a double, we need to use our special extendo type for that
					if (FunctionDesc->ReturnType.Type == FAngelscriptType::ScriptDoubleType())
					{
						if (ParentFunction->GetReturnProperty()->IsA<FFloatProperty>())
						{
							FunctionDesc->ReturnType.Type = FAngelscriptType::ScriptFloatParamExtendedToDoubleType();
							ScriptFunction->returnType.SetFloatExtendedToDouble(true);
						}
					}

					if (!FunctionDesc->ReturnType.MatchesProperty(ParentFunction->GetReturnProperty(), FAngelscriptType::EPropertyMatchType::OverrideReturnValue))
						bTypeMismatch = true;
				}


				int32 UEParmCount = ParentFunction->NumParms;
				if (!bUEReturnsVoid)
					UEParmCount -= 1;

				if (FunctionDesc->Arguments.Num() != UEParmCount)
					bArgCountMismatch = true;

				int32 ArgumentIndex = 0;
				for (TFieldIterator<FProperty> It(ParentFunction); It && (It->PropertyFlags & CPF_Parm); ++It)
				{
					FProperty* Property = *It;
					if (Property->PropertyFlags & CPF_ReturnParm)
						continue;

					// Dummy arguments can match any reference to a struct
					if (Property->GetFName() == NAME_AnyStructRef)
					{
						if (FunctionDesc->Arguments.Num() == 0)
						{
							bArgCountMismatch = false;
							continue;
						}

						auto& OverrideArg = FunctionDesc->Arguments[ArgumentIndex];
						if (!OverrideArg.Type.bIsReference
							|| OverrideArg.Type.bIsConst != Property->HasAnyPropertyFlags(CPF_ConstParm)
							|| !OverrideArg.Type.Type.IsValid()
							|| !OverrideArg.Type.Type->IsUnrealStruct())
						{
							bTypeMismatch = true;
						}
						else
						{
							OverrideArg.bBlueprintByValue = false;
							OverrideArg.bBlueprintInRef = true;
							OverrideArg.bBlueprintOutRef = false;
						}

						continue;
					}

					// Check regular argument signatures
					if (ArgumentIndex < FunctionDesc->Arguments.Num())
					{
						auto& OverrideArg = FunctionDesc->Arguments[ArgumentIndex];
						OverrideArg.ArgumentName = Property->GetName();

						// If the UE parameter is a float, but the script one is a double, we need to use our special extendo type for that
						if (OverrideArg.Type.Type == FAngelscriptType::ScriptDoubleType() && Property->IsA<FFloatProperty>())
						{
							OverrideArg.Type.Type = FAngelscriptType::ScriptFloatParamExtendedToDoubleType();
							ScriptFunction->parameterTypes[ArgumentIndex].SetFloatExtendedToDouble(true);
						}

						if (!FunctionDesc->Arguments[ArgumentIndex].Type.MatchesProperty(Property, FAngelscriptType::EPropertyMatchType::OverrideArgument))
							bTypeMismatch = true;

						if (OverrideArg.Type.bIsReference)
						{
							if (Property->HasAnyPropertyFlags(CPF_ReferenceParm))
							{
								OverrideArg.bBlueprintByValue = false;
								OverrideArg.bBlueprintInRef = true;
								OverrideArg.bBlueprintOutRef = false;

								if (Property->HasAnyPropertyFlags(CPF_OutParm))
									OverrideArg.bInRefForceCopyOut = true;
								else
									ensure(false);

								if (Property->HasAnyPropertyFlags(CPF_ConstParm) != OverrideArg.Type.bIsConst)
									bTypeMismatch = true;
							}
							else if (Property->HasAnyPropertyFlags(CPF_OutParm))
							{
								OverrideArg.bBlueprintByValue = false;
								OverrideArg.bBlueprintInRef = false;
								OverrideArg.bBlueprintOutRef = true;

								if (OverrideArg.Type.bIsConst)
									bTypeMismatch = true;
							}
							else
							{
								OverrideArg.bBlueprintByValue = true;
								OverrideArg.bBlueprintInRef = false;
								OverrideArg.bBlueprintOutRef = false;

								if (!OverrideArg.Type.bIsConst)
									bTypeMismatch = true;
							}
						}
						else
						{
							OverrideArg.bBlueprintByValue = true;
							OverrideArg.bBlueprintInRef = false;
							OverrideArg.bBlueprintOutRef = false;

							if (Property->HasAnyPropertyFlags(CPF_ReferenceParm) || Property->HasAnyPropertyFlags(CPF_OutParm))
								bTypeMismatch = true;
						}
					}

					ArgumentIndex += 1;
				}

				if (bTypeMismatch || bArgCountMismatch)
				{
					FString ExpectedSignature;
					if (ScriptType != nullptr)
					{
						asIScriptFunction* ScriptParentFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionDesc->OriginalFunctionName));
						if (ScriptParentFunction != nullptr)
						{
							ExpectedSignature = ANSI_TO_TCHAR(ScriptParentFunction->GetDeclaration(false, false, true, true));
						}
					}

					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
						TEXT("BlueprintOverride method %s in class %s does not match function signature of event in superclass %s.\nExpected Signature: %s"),
						*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName, *CodeSuperClass->GetName(), *ExpectedSignature));
					ClassData.ReloadReq = EReloadRequirement::Error;
				}

				// Check const correctness separately so we can emit a more specific error
				if (ParentFunction->HasAnyFunctionFlags(FUNC_Const))
				{
					if (!ScriptFunction->IsReadOnly())
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
							TEXT("BlueprintOverride method %s in class %s is not const, but is overriding a const method. Please add 'const' to the end of the function declaration."),
							*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
				}
				else
				{
					if (ScriptFunction->IsReadOnly())
					{
						FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
							TEXT("BlueprintOverride method %s in class %s is specified as const, but is overriding a non const method. Please remove 'const' from the end of the function declaration."),
							*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
						ClassData.ReloadReq = EReloadRequirement::Error;
					}
				}

				if (ParentFunction->HasAnyFunctionFlags(FUNC_EditorOnly) && !FunctionDesc->Meta.Contains(NAME_Meta_EditorOnly))
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
						TEXT("BlueprintOverride method %s in class %s overrides an editor-only parent function, but is not in editor-only code."),
						*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
					ClassData.ReloadReq = EReloadRequirement::Error;
				}
			}
		}
	}

	// Analyze if any function needs validation (we do this after the loop above to ensure all functions have their Return type and other values initialized)
	for (auto FunctionDesc : ClassData.NewClass->Methods)
	{
		if (FunctionDesc->bNetValidate)
		{
			auto ValidateFunction = ClassData.NewClass->GetMethod(FunctionDesc->FunctionName + "_Validate");
			if (ValidateFunction)
			{
				if (ValidateFunction->ScriptFunction->GetReturnTypeId() != asTYPEID_BOOL)
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ValidateFunction->LineNumber, FString::Printf(
						TEXT("UFUNCTION() %s in class %s has a _Validate function that is returning a non-bool!"),
						*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
					ClassData.ReloadReq = EReloadRequirement::Error;
				}
				else if (!FunctionDesc->ParametersMatches(ValidateFunction))
				{
					FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ValidateFunction->LineNumber, FString::Printf(
						TEXT("UFUNCTION() %s in class %s has a _Validate function but the parameters don't match!"),
						*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
					ClassData.ReloadReq = EReloadRequirement::Error;
				}
			}
			else
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, FunctionDesc->LineNumber, FString::Printf(
					TEXT("UFUNCTION() %s in class %s is marked as WithValidate but no _Validate function provided! Is it marked as UFUNCTION()?"),
					*FunctionDesc->FunctionName, *ClassData.NewClass->ClassName));
				ClassData.ReloadReq = EReloadRequirement::Error;
			}
		}
	}

	

	UStruct* PrevStruct = nullptr;
	if (ClassData.OldClass.IsValid())
	{
		if (ClassData.OldClass->Class != nullptr)
			PrevStruct = ClassData.OldClass->Class;
		else if (ClassData.OldClass->Struct != nullptr)
			PrevStruct = ClassData.OldClass->Struct;
	}
#if AS_CAN_HOTRELOAD
	else if (FAngelscriptEngine::Get().bIsInitialCompileFinished)
	{
		UASClass* ReplacedClass = FindObject<UASClass>(FAngelscriptEngine::GetPackage(), *UnrealName);
		if (ReplacedClass != nullptr)
		{
			//[UE++]: Downgrade to FullReloadSuggested so SoftReloadOnly can still swap in the module;
			// ShouldFullReload() will route brand-new classes through CreateFullReloadClass during soft reload
			if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
				ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
			//[UE--]
		}
	}
#endif

	if (PrevStruct != nullptr)
	{
		// If our superclass changed, we need a full reload
		if (ClassData.OldClass->SuperClass != ClassData.NewClass->SuperClass)
		{
			if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
				ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
		}

		// Check if any properties from the old class have been
		// removed or changed type.
		for (auto OldPropertyDesc : ClassData.OldClass->Properties)
		{
			bool bFound = false;
			for (auto PropertyDesc : ClassData.NewClass->Properties)
			{
				if (PropertyDesc->PropertyName == OldPropertyDesc->PropertyName)
				{
					bFound = true;

					// If the property type has changes, we must do a full reload
					if (PropertyDesc->PropertyType != OldPropertyDesc->PropertyType)
					{
						if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
						{
							ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
							ClassData.ReloadReqLines.AddUnique(PropertyDesc->LineNumber);
						}
					}
					else if (HasReloadedReflectedScriptType(OldPropertyDesc->PropertyType, PropertyDesc->PropertyType))
					{
						if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
						{
							ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
							ClassData.ReloadReqLines.AddUnique(PropertyDesc->LineNumber);
						}
					}

					// If the definition has changed, we must do a full reload
					if (!PropertyDesc->IsDefinitionEquivalent(*OldPropertyDesc))
					{
						if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
						{
							ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
							ClassData.ReloadReqLines.AddUnique(PropertyDesc->LineNumber);
						}
					}

					// If the metadata changed suggest a full reload
					if (!PropertyDesc->Meta.OrderIndependentCompareEqual(OldPropertyDesc->Meta))
					{
						if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
						{
							ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
							ClassData.ReloadReqLines.AddUnique(PropertyDesc->LineNumber);
						}
					}
				}
			}

			// If any properties were removed, we must do a full reload
			if (!bFound)
			{
				if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
				{
					ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
					ClassData.ReloadReqLines.AddUnique(OldPropertyDesc->LineNumber);
				}
			}
		}

		// Check if we added any new properties that weren't in the old class.
		for (auto PropertyDesc : ClassData.NewClass->Properties)
		{
			TSharedPtr<FAngelscriptPropertyDesc> PreviousProperty;
			bool bFound = false;
			for (auto OldPropertyDesc : ClassData.OldClass->Properties)
			{
				if (PropertyDesc->PropertyName == OldPropertyDesc->PropertyName
					&& OldPropertyDesc->bHasUnrealProperty)
				{
					PreviousProperty = OldPropertyDesc;
					bFound = true;
					break;
				}
			}

			// If we added a new property, we should suggest a full reload so we can use it
			if (!bFound)
			{
				if (ClassData.NewClass->bIsStruct || PropertyDesc->PropertyType.RequiresProperty())
				{
					// If the property was required to be added, we need to do a full
					// reload and can't just suggest one.
					if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
					{
						ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
						ClassData.ReloadReqLines.AddUnique(PropertyDesc->LineNumber);
					}
				}
				else
				{
					if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
					{
						ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
						ClassData.ReloadReqLines.AddUnique(PropertyDesc->LineNumber);
					}
				}
			}
			else
			{
				// In structs, changing the offset of a property also requires a full reload,
				// since we can't loop over all instances of a struct to change them.
				if (ClassData.NewClass->bIsStruct)
				{
					if (PreviousProperty->ScriptPropertyOffset != PropertyDesc->ScriptPropertyOffset)
					{
						if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
						{
							ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
							ClassData.ReloadReqLines.AddUnique(PropertyDesc->LineNumber);
						}
					}
				}
			}
		}

		// Check if any bound methods from the old class have been removed or changed signature
		for (auto OldFunctionDesc : ClassData.OldClass->Methods)
		{
			auto NewFunctionDesc = ClassData.NewClass->GetMethod(OldFunctionDesc->FunctionName);
			if (!NewFunctionDesc.IsValid())
			{
				// Method was removed, need full reload
				if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
				{
					ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
					ClassData.ReloadReqLines.AddUnique(OldFunctionDesc->LineNumber);
				}
			}
			else
			{
				if (!OldFunctionDesc->SignatureMatches(NewFunctionDesc))
				{
					// Method changed signature, need full reload
					if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
					{
						ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
						ClassData.ReloadReqLines.AddUnique(NewFunctionDesc->LineNumber);
					}
				}
				else if (HasReloadedReflectedScriptType(*OldFunctionDesc, *NewFunctionDesc))
				{
					if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
					{
						ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
						ClassData.ReloadReqLines.AddUnique(NewFunctionDesc->LineNumber);
					}
				}
				else
				{
					// Check if any default values have changed
					for (int32 ArgIndex = 0, ArgCount = NewFunctionDesc->Arguments.Num(); ArgIndex < ArgCount; ++ArgIndex)
					{
						if (!ensure(OldFunctionDesc->Arguments.IsValidIndex(ArgIndex)))
							continue;
						auto& NewArgument = NewFunctionDesc->Arguments[ArgIndex];
						auto& OldArgument = OldFunctionDesc->Arguments[ArgIndex];
						if (NewArgument.DefaultValue != OldArgument.DefaultValue
							|| NewArgument.ArgumentName != OldArgument.ArgumentName)
						{
							// We should suggest a full reload to propagate this change
							if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
							{
								ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
								ClassData.ReloadReqLines.AddUnique(NewFunctionDesc->LineNumber);
							}
						}
					}
				}

				// If the definition has changed, we must do a full reload
				if (!OldFunctionDesc->IsDefinitionEquivalent(*NewFunctionDesc))
				{
					if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
					{
						ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
						ClassData.ReloadReqLines.AddUnique(NewFunctionDesc->LineNumber);
					}
				}

				// If the metadata changed suggest a full reload
				if (!NewFunctionDesc->Meta.OrderIndependentCompareEqual(OldFunctionDesc->Meta))
				{
					if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
					{
						ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
						ClassData.ReloadReqLines.AddUnique(NewFunctionDesc->LineNumber);
					}
				}
			}
		}

		// Check if we added any bound methods that weren't in the old class
		for (auto NewFunctionDesc : ClassData.NewClass->Methods)
		{
			auto OldFunctionDesc = ClassData.OldClass->GetMethod(NewFunctionDesc->FunctionName);
			if (!OldFunctionDesc.IsValid() || OldFunctionDesc->Function == nullptr)
			{
				// We added a new function, we should suggest a full reload but not require it
				if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
				{
					ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
					ClassData.ReloadReqLines.AddUnique(NewFunctionDesc->LineNumber);
				}

				// Unless this is a BlueprintEvent, which requires a full reload to be added
				// since the event thunk calls back into the blueprint vm to handle the virtualness
				if (NewFunctionDesc->bBlueprintEvent)
				{
					if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
					{
						ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
						ClassData.ReloadReqLines.AddUnique(NewFunctionDesc->LineNumber);
					}
				}
			}
		}

		// If we changed code in 'default' statements, we need to suggest a full reload
		// to propagate the changes to properties properly.
		if (ClassData.OldClass->DefaultsCode != ClassData.NewClass->DefaultsCode)
		{
			if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
				ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
		}

		// If the size of the new script type is larger than the old one (+debug slack),
		// then we can't replace the script objects in-place, so we need a full reload
		UASClass* OldClass = (UASClass*)ClassData.OldClass->Class;
		if (OldClass != nullptr && ScriptType != nullptr)
		{
			int32 ScriptSize = ScriptType->GetSize();
			if (ScriptSize > OldClass->GetPropertiesSize())
			{
				if (ClassData.ReloadReq < EReloadRequirement::FullReloadRequired)
					ClassData.ReloadReq = EReloadRequirement::FullReloadRequired;
			}
		}

		// If the metadata changed suggest a full reload
		if (!ClassData.NewClass->Meta.OrderIndependentCompareEqual(ClassData.OldClass->Meta))
		{
			if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
				ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
		}

		// If one of the class' flags have changed, we should try to do a full reload
		if (!ClassData.NewClass->AreFlagsEqual(*ClassData.OldClass.Get()))
		{
			if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
				ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
		}
	}
	else
	{
		//[UE++]: Downgrade to FullReloadSuggested so SoftReloadOnly can swap in the module;
		// ShouldFullReload() materializes brand-new classes via CreateFullReloadClass during soft reload
		if (ClassData.ReloadReq < EReloadRequirement::FullReloadSuggested)
			ClassData.ReloadReq = EReloadRequirement::FullReloadSuggested;
		//[UE--]
	}

	if (!ClassData.NewClass->ComposeOntoClass.IsEmpty())
	{
		auto ComposeOntoClassDesc = GetClassDesc(ClassData.NewClass->ComposeOntoClass);
		if (!ComposeOntoClassDesc.IsValid())
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ClassData.NewClass->LineNumber, FString::Printf(
				TEXT("Class %s declares ComposeOntoClass %s, but the target class is missing."),
				*ClassData.NewClass->ClassName, *ClassData.NewClass->ComposeOntoClass));
			ClassData.ReloadReq = EReloadRequirement::Error;
		}
		else
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, ClassData.NewClass->LineNumber, FString::Printf(
				TEXT("Class %s declares ComposeOntoClass %s, but compose materialization is not implemented yet."),
				*ClassData.NewClass->ClassName, *ClassData.NewClass->ComposeOntoClass));
			ClassData.ReloadReq = EReloadRequirement::Error;
		}
	}

	// Make sure any composed structs we're composing have the ComposedStruct metatag on their property
	// as well, or weird stuff will happen.
#if WITH_EDITOR
	if ((ClassData.NewClass->bIsStruct && ClassData.NewClass->Meta.Contains("ComposedStruct"))
		|| !ClassData.NewClass->ComposeOntoClass.IsEmpty())
	{
		for (auto Property : ClassData.NewClass->Properties)
		{
			if (Property->Meta.Contains("NoCompose"))
				continue;
			if (Property->Meta.Contains("CustomCompose"))
				continue;
			if (Property->Meta.Contains("ComposedStruct"))
				continue;

			if (!Property->PropertyType.Type.IsValid())
				continue;

			if (Property->PropertyType.Type != FAngelscriptType::GetScriptStruct())
				continue;

			auto StructData = GetClassDesc(ANSI_TO_TCHAR(Property->PropertyType.ScriptClass->GetName()));
			if (!StructData.IsValid())
				continue;

			if (StructData->Meta.Contains("ComposedStruct"))
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, Property->LineNumber, FString::Printf(
					TEXT("Composed struct property %s does not have ComposedStruct meta in its UPROPERTY()."),
					*Property->PropertyName));
				ClassData.ReloadReq = EReloadRequirement::Error;
			}
		}
	}
#endif

#if WITH_EDITOR
	// Apply additional compile errors that the code class wants if applicable
	if (ClassData.NewClass.IsValid())
	{
		auto& AdditionalCompileChecks = FAngelscriptEngine::Get().AdditionalCompileChecks;
		UClass* CodeParent = ClassData.NewClass->CodeSuperClass;
		while (CodeParent != nullptr)
		{
			auto* CheckBind = AdditionalCompileChecks.Find(CodeParent);
			if (CheckBind != nullptr && (*CheckBind).IsValid())
			{
				if (!(*CheckBind)->ScriptCompileAdditionalChecks(ModuleData.NewModule, ClassData.NewClass))
				{
					ClassData.ReloadReq = EReloadRequirement::Error;
				}
			}

			CodeParent = CodeParent->GetSuperClass();
		}
	}
#endif

	ClassData.bAnalyzed = true;
}

void FAngelscriptClassGenerator::Analyze(FModuleData& ModuleData, FDelegateData& DelegateData)
{
	// Ignore if we've already analyzed this delegate
	if (DelegateData.bAnalyzed)
		return;
	DelegateData.bAnalyzed = true;

	auto DelegateDesc = DelegateData.NewDelegate;

	// Check already compiled modules for conflicting delegates
	TSharedPtr<FAngelscriptModuleDesc> FoundInModule;
	auto ExistingDelegate = FAngelscriptEngine::Get().GetDelegate(DelegateDesc->DelegateName, &FoundInModule);
	if (ExistingDelegate.IsValid() && FoundInModule.IsValid())
	{
		// Only allow this if the module it was in is being reloaded
		if (!IsReloadingModule(FoundInModule))
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, 1, FString::Printf(
				TEXT("Delegate/Event %s in module %s already exists in module %s."),
				*DelegateDesc->DelegateName, *ModuleData.NewModule->ModuleName, *FoundInModule->ModuleName));
			ModuleData.ReloadReq = EReloadRequirement::Error;
		}
	}

	// Check other modules we're reloading for conflicting delegates
	for (auto& OtherModule : Modules)
	{
		for (auto& OtherDelegate : OtherModule.Delegates)
		{
			if (OtherDelegate.NewDelegate->DelegateName == DelegateDesc->DelegateName
				&& DelegateDesc != OtherDelegate.NewDelegate)
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, 1, FString::Printf(
					TEXT("Delegate/Event %s in module %s already exists in module %s."),
					*DelegateDesc->DelegateName, *ModuleData.NewModule->ModuleName, *OtherModule.NewModule->ModuleName));
				ModuleData.ReloadReq = EReloadRequirement::Error;
			}
		}
	}

	// Resolve the compiled script type for the delegate
	auto* ScriptType = DelegateDesc->ScriptType;

	if (ScriptType == nullptr)
	{
		FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, DelegateData.NewDelegate->LineNumber, FString::Printf(
			TEXT("Could not find generated struct for delegate type %s"), *DelegateDesc->DelegateName));
		DelegateData.ReloadReq = EReloadRequirement::Error;
		return;
	}

	if (DelegateData.OldDelegate.IsValid() && DelegateData.OldDelegate->ScriptType)
		UpdatedScriptTypeMap.Add(DelegateData.OldDelegate->ScriptType, ScriptType);

	// Find the signature function in the delegate's struct class
	asIScriptFunction* ScriptSignature = nullptr;

	if (DelegateDesc->bIsMulticast)
		ScriptSignature = ScriptType->GetMethodByName("Broadcast");
	else
		ScriptSignature = ScriptType->GetMethodByName("Execute");

	if (ScriptSignature == nullptr)
	{
		FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, DelegateData.NewDelegate->LineNumber, FString::Printf(
			TEXT("Could not find generated execute method for delegate type %s"), *DelegateDesc->DelegateName));
		DelegateData.ReloadReq = EReloadRequirement::Error;
		return;
	}

	auto FunctionDesc = MakeShared<FAngelscriptFunctionDesc>();
	DelegateDesc->Signature = FunctionDesc;

	int32 ReturnTypeId = ScriptSignature->GetReturnTypeId();
	if (ReturnTypeId != asTYPEID_VOID)
	{
		FunctionDesc->ReturnType = FAngelscriptTypeUsage::FromReturn(ScriptSignature);
		if (!FunctionDesc->ReturnType.IsValid() || !FunctionDesc->ReturnType.CanCreateProperty() || !FunctionDesc->ReturnType.CanBeReturned())
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, DelegateData.NewDelegate->LineNumber, FString::Printf(
				TEXT("Unknown or invalid return type to function %s in delegate %s."), *FunctionDesc->FunctionName, *DelegateDesc->DelegateName));
			DelegateData.ReloadReq = EReloadRequirement::Error;
		}
	}

	int32 ArgCount = ScriptSignature->GetParamCount();
	for (int32 i = 0; i < ArgCount; ++i)
	{
		const char* ParamName = nullptr;
		const char* ParamDefaultValue = nullptr;
		asDWORD RefFlags = 0;
		ScriptSignature->GetParam(i, nullptr, &RefFlags, &ParamName, &ParamDefaultValue);

		auto Type = FAngelscriptTypeUsage::FromParam(ScriptSignature, i);
		if (!Type.IsValid() || !Type.CanBeArgument() || !Type.CanCreateProperty())
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, DelegateData.NewDelegate->LineNumber, FString::Printf(
				TEXT("Unknown or invalid parameter type for parameter %s to delegate %s."), 
				ANSI_TO_TCHAR(ParamName), *DelegateDesc->DelegateName));
			DelegateData.ReloadReq = EReloadRequirement::Error;
			break;
		}

		FAngelscriptArgumentDesc ArgDesc;
		ArgDesc.Type = Type;
		ArgDesc.ArgumentName = ANSI_TO_TCHAR(ParamName);
		ArgDesc.DefaultValue = ANSI_TO_TCHAR(ParamDefaultValue);

		// Some types of arguments are forced to be outparam refs
		if (Type.IsValid() && Type.Type->IsParamForcedOutParam() && Type.bIsConst)
		{
			ArgDesc.bInRefForceCopyOut = true;
			ArgDesc.bBlueprintInRef = true;
		}
		else if (Type.bIsReference)
		{
			if ((RefFlags & asTM_INOUTREF) == asTM_INOUTREF)
			{
				if (Type.bIsConst)
				{
					ArgDesc.bBlueprintByValue = true;
				}
				else
				{
					ArgDesc.bBlueprintInRef = true;
				}
			}
			else if ((RefFlags & asTM_OUTREF) != 0)
			{
				ArgDesc.bBlueprintOutRef = true;
			}
			else
			{
				ArgDesc.bBlueprintInRef = true;
			}
		}
		else
		{
			ArgDesc.bBlueprintByValue = true;
		}

		FunctionDesc->Arguments.Add(ArgDesc);
	}

	if (DelegateData.OldDelegate.IsValid())
	{
		if (!DelegateData.OldDelegate->Signature.IsValid()
			|| !DelegateData.OldDelegate->Signature->SignatureMatches(FunctionDesc, true)
			|| !DelegateData.OldDelegate->Signature->IsDefinitionEquivalent(*FunctionDesc))
		{
			// Signature changed, need full reload
			if (DelegateData.ReloadReq < EReloadRequirement::FullReloadRequired)
			{
				DelegateData.ReloadReq = EReloadRequirement::FullReloadRequired;
				DelegateData.ReloadReqLines.AddUnique(DelegateDesc->LineNumber);
			}
		}
		else if (HasReloadedReflectedScriptType(*DelegateData.OldDelegate->Signature, *FunctionDesc))
		{
			if (DelegateData.ReloadReq < EReloadRequirement::FullReloadSuggested)
			{
				DelegateData.ReloadReq = EReloadRequirement::FullReloadSuggested;
				DelegateData.ReloadReqLines.AddUnique(DelegateDesc->LineNumber);
			}
		}
	}
	else
	{
		//[UE++]: Downgrade to FullReloadSuggested so SoftReloadOnly can swap in the module
		if (DelegateData.ReloadReq < EReloadRequirement::FullReloadSuggested)
		{
			DelegateData.ReloadReq = EReloadRequirement::FullReloadSuggested;
			DelegateData.ReloadReqLines.AddUnique(DelegateDesc->LineNumber);
		}
		//[UE--]
	}
}

void FAngelscriptClassGenerator::InitEnums(FModuleData& ModuleData)
{
	auto ModuleDesc = ModuleData.NewModule;
	asIScriptModule* ScriptModule = ModuleData.NewModule->ScriptModule;
	if (ScriptModule == nullptr)
	{
		ensure(false);
		return;
	}

	// Create new enum descriptors for all enums found in the script module
	int32 EnumCount = ScriptModule->GetEnumCount();
	for (int32 i = 0; i < EnumCount; ++i)
	{
		asITypeInfo* EnumType = ScriptModule->GetEnumByIndex(i);
		FString EnumName = ANSI_TO_TCHAR(EnumType->GetName());

		// The preprocessor might have already created an enum for this
		auto EnumDesc = ModuleData.NewModule->GetEnum(EnumName);
		if (!EnumDesc.IsValid())
		{
			EnumDesc = MakeShared<FAngelscriptEnumDesc>();
			EnumDesc->EnumName = MoveTemp(EnumName);
			ModuleData.NewModule->Enums.Add(EnumDesc.ToSharedRef());

#if WITH_EDITOR
			EnumDesc->LineNumber = ((asCTypeInfo*)EnumType)->declaredAt & 0xFFFFF;
#endif
		}

		EnumDesc->ScriptType = EnumType;

		FEnumData EnumData;
		EnumData.NewEnum = EnumDesc;
		EnumData.DataIndex = ModuleData.Enums.Num();

		// Add all values from script into the enum
		int32 ValueCount = EnumType->GetEnumValueCount();
		for (int32 v = 0; v < ValueCount; ++v)
		{
			int32 Value;
			FString Name = ANSI_TO_TCHAR(EnumType->GetEnumValueByIndex(v, &Value));
			EnumDesc->ValueNames.Add(*Name);
			EnumDesc->EnumValues.Add(Value);
		}

		// Look up the previous descriptor for the enum
		if (ModuleData.OldModule.IsValid())
		{
			EnumData.OldEnum = ModuleData.OldModule->GetEnum(EnumDesc->EnumName);
			if (EnumData.OldEnum.IsValid())
			{
				EnumData.NewEnum->Enum = EnumData.OldEnum->Enum;
			}
		}

		ModuleData.Enums.Add(EnumData);

		check(!DataRefByNewScriptType.Contains(EnumType));

		DataRefByNewScriptType.Add(EnumType, FDataRef(ModuleData, EnumData));
		DataRefByName.Add(EnumDesc->EnumName, FDataRef(ModuleData, EnumData));
	}
}

bool FAngelscriptClassGenerator::IsReloadingModule(TSharedPtr<FAngelscriptModuleDesc> Module)
{
	for (auto& ModuleData : Modules)
	{
		if (ModuleData.OldModule == Module)
			return true;
		if (ModuleData.NewModule == Module)
			return true;
	}
	return false;
}

void FAngelscriptClassGenerator::AnalyzeEnums(FModuleData& ModuleData)
{
	// Make sure our enums don't collide with any other existing enums
	for (auto& EnumData : ModuleData.Enums)
	{
		auto EnumDesc = EnumData.NewEnum;

		// Check already compiled modules
		TSharedPtr<FAngelscriptModuleDesc> FoundInModule;
		auto ExistingEnum = FAngelscriptEngine::Get().GetEnum(EnumDesc->EnumName, &FoundInModule);
		if (ExistingEnum.IsValid() && FoundInModule.IsValid())
		{
			// Only allow this if the module it was in is being reloaded
			if (!IsReloadingModule(FoundInModule))
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, 1, FString::Printf(
					TEXT("Enum %s in module %s already exists in module %s."),
					*EnumDesc->EnumName, *ModuleData.NewModule->ModuleName, *FoundInModule->ModuleName));
				ModuleData.ReloadReq = EReloadRequirement::Error;
			}
		}

		// Make sure the unreal name is not being used
		// NB: Enums here actually _do_ have the `E` prefix in their unreal name!
		FString UnrealName = EnumDesc->EnumName;
		if (UsedUnrealNames.Contains(UnrealName))
		{
			FAngelscriptEngine::Get().ScriptCompileError(ModuleData.NewModule, EnumDesc->LineNumber, FString::Printf(
				TEXT("Name conflict: unreal name %s for script enum %s is already in use."),
				*UnrealName, *EnumDesc->EnumName));
			ModuleData.ReloadReq = EReloadRequirement::Error;
		}
		UsedUnrealNames.Add(UnrealName);

		// Check if we've changed the enum
		if (EnumData.OldEnum.IsValid())
		{
			if (EnumData.NewEnum->ValueNames != EnumData.OldEnum->ValueNames
				|| EnumData.NewEnum->EnumValues != EnumData.OldEnum->EnumValues)
			{
				if (ModuleData.ReloadReq < EReloadRequirement::FullReloadSuggested)
				{
					ModuleData.ReloadReq = EReloadRequirement::FullReloadSuggested;
					ModuleData.ReloadReqLines.AddUnique(EnumDesc->LineNumber);
				}
				EnumData.bNeedReload = true;
			}

#if WITH_EDITOR
			if (EnumData.NewEnum->Documentation != EnumData.OldEnum->Documentation)
			{
				if (ModuleData.ReloadReq < EReloadRequirement::FullReloadSuggested)
				{
					ModuleData.ReloadReq = EReloadRequirement::FullReloadSuggested;
					ModuleData.ReloadReqLines.AddUnique(EnumDesc->LineNumber);
				}
				EnumData.bNeedReload = true;
			}
#endif

			// Check if the metadata has changed
			if (!EnumData.NewEnum->Meta.OrderIndependentCompareEqual(EnumData.OldEnum->Meta))
			{
				if (ModuleData.ReloadReq < EReloadRequirement::FullReloadSuggested)
				{
					ModuleData.ReloadReq = EReloadRequirement::FullReloadSuggested;
					ModuleData.ReloadReqLines.AddUnique(EnumDesc->LineNumber);
				}
				EnumData.bNeedReload = true;
			}
		}

		// Check if we've added the enum
		if (!EnumData.OldEnum.IsValid() || EnumData.OldEnum->Enum == nullptr)
		{
			if (ModuleData.ReloadReq < EReloadRequirement::FullReloadSuggested)
			{
				ModuleData.ReloadReq = EReloadRequirement::FullReloadSuggested;
				ModuleData.ReloadReqLines.AddUnique(EnumDesc->LineNumber);
			}
			EnumData.bNeedReload = true;
		}
	}
}

void FAngelscriptClassGenerator::SetupModule(FModuleData& ModuleData)
{
	// Create internal class data for each class
	for (auto ClassDesc : ModuleData.NewModule->Classes)
	{	
		FClassData ClassData;
		ClassData.NewClass = ClassDesc;
		ClassData.DataIndex = ModuleData.Classes.Num();

		// Find the old class we're replacing
		if (ModuleData.OldModule.IsValid())
		{
			for (auto OldClassDesc : ModuleData.OldModule->Classes)
			{
				if (OldClassDesc->ClassName == ClassDesc->ClassName
					&& OldClassDesc->bIsStruct == ClassDesc->bIsStruct)
				{
					ClassData.OldClass = OldClassDesc;
					break;
				}
			}
		}
		
		ModuleData.Classes.Add(ClassData);

		// Store lookup for this class
		if (!ClassDesc->bIsStaticsClass)
		{
			auto* ScriptType = GetNamespacedTypeInfoForClass(ClassData.NewClass, ModuleData.NewModule);
			ClassData.NewClass->ScriptType = ScriptType;

			check(!DataRefByNewScriptType.Contains(ScriptType));

			DataRefByNewScriptType.Add(ClassData.NewClass->ScriptType, FDataRef(ModuleData, ClassData));
			DataRefByName.Add(ClassData.NewClass->ClassName, FDataRef(ModuleData, ClassData));
		}
	}

	// Create internal delegate data
	for (auto DelegateDesc : ModuleData.NewModule->Delegates)
	{
		FDelegateData DelegateData;
		DelegateData.NewDelegate = DelegateDesc;
		DelegateData.DataIndex = ModuleData.Delegates.Num();

		// Find the old delegate we're replacing
		if (ModuleData.OldModule.IsValid())
		{
			for (auto OldDelegateDesc : ModuleData.OldModule->Delegates)
			{
				if (OldDelegateDesc->DelegateName == DelegateDesc->DelegateName)
				{
					DelegateData.OldDelegate = OldDelegateDesc;
					break;
				}
			}
		}

		ModuleData.Delegates.Add(DelegateData);

		// Store lookup for this delegate
		auto* ScriptType = ModuleData.NewModule->ScriptModule->GetTypeInfoByName(TCHAR_TO_ANSI(*DelegateDesc->DelegateName));
		DelegateData.NewDelegate->ScriptType = ScriptType;

		// Tag the script type as a delegate so we can classify it before the
		// actual delegate signature function is generated.
		if (DelegateDesc->bIsMulticast)
			ScriptType->SetUserData(FAngelscriptType::TAG_UserData_Multicast_Delegate);
		else
			ScriptType->SetUserData(FAngelscriptType::TAG_UserData_Delegate);

		check(!DataRefByNewScriptType.Contains(ScriptType));

		DataRefByNewScriptType.Add(DelegateData.NewDelegate->ScriptType, FDataRef(ModuleData, DelegateData));
		DataRefByName.Add(DelegateData.NewDelegate->DelegateName, FDataRef(ModuleData, DelegateData));
	}
}

void FAngelscriptClassGenerator::Analyze(FModuleData& ModuleData)
{
	// Analyze each delegate in the module
	for (auto& DelegateData : ModuleData.Delegates)
	{
		Analyze(ModuleData, DelegateData);

		// The delegate that requires the highest reload type should determine it
		if (DelegateData.ReloadReq > ModuleData.ReloadReq)
		{
			ModuleData.ReloadReq = DelegateData.ReloadReq;
#if WITH_EDITOR
			ModuleData.ReloadReqLines.AddUnique(DelegateData.NewDelegate->LineNumber);
#endif
		}

#if WITH_EDITOR
		for (int32 ReloadLine : DelegateData.ReloadReqLines)
			ModuleData.ReloadReqLines.AddUnique(ReloadLine);
#endif
	}

	// Analyze each class in the module
	for (auto& ClassData : ModuleData.Classes)
	{
		Analyze(ModuleData, ClassData);

		// The class that requires the highest reload type should determine it
		if (ClassData.ReloadReq > ModuleData.ReloadReq)
		{
			ModuleData.ReloadReq = ClassData.ReloadReq;
#if WITH_EDITOR
			ModuleData.ReloadReqLines.AddUnique(ClassData.NewClass->LineNumber);
#endif
		}

#if WITH_EDITOR
		for (int32 ReloadLine : ClassData.ReloadReqLines)
			ModuleData.ReloadReqLines.AddUnique(ReloadLine);
#endif
	}

	// Make sure enums from the file are analyzed
	InitEnums(ModuleData);
	AnalyzeEnums(ModuleData);

	// If any classes from the old module aren't in the new module,
	// immediately require a full reload.
	if (ModuleData.OldModule.IsValid())
	{
		for (auto OldClassDesc : ModuleData.OldModule->Classes)
		{
			if (!ModuleData.NewModule->GetClass(OldClassDesc->ClassName).IsValid())
			{
				ModuleData.RemovedClasses.Add(OldClassDesc);
				if (ModuleData.ReloadReq < EReloadRequirement::FullReloadRequired)
					ModuleData.ReloadReq = EReloadRequirement::FullReloadRequired;
			}
		}
	}
}

void FAngelscriptClassGenerator::TryGenerateClassRenameRedirects(FModuleData& ModuleData)
{
#if WITH_EDITOR
	TSharedPtr<FAngelscriptClassDesc> RemovedRenameClass;
	for (const TSharedPtr<FAngelscriptClassDesc>& RemovedClass : ModuleData.RemovedClasses)
	{
		if (!RemovedClass.IsValid()
			|| RemovedClass->bIsStruct
			|| RemovedClass->bIsStaticsClass
			|| Cast<UASClass>(RemovedClass->Class) == nullptr)
		{
			continue;
		}

		if (RemovedRenameClass.IsValid())
		{
			return;
		}

		RemovedRenameClass = RemovedClass;
	}

	if (!RemovedRenameClass.IsValid())
	{
		return;
	}

	FClassData* AddedRenameClass = nullptr;
	for (FClassData& ClassData : ModuleData.Classes)
	{
		if (!ClassData.NewClass.IsValid()
			|| ClassData.OldClass.IsValid()
			|| ClassData.NewClass->bIsStruct
			|| ClassData.NewClass->bIsStaticsClass)
		{
			continue;
		}

		if (AddedRenameClass != nullptr)
		{
			return;
		}

		AddedRenameClass = &ClassData;
	}

	if (AddedRenameClass == nullptr)
	{
		return;
	}

	FAngelscriptClassRedirects::TryAddGeneratedCoreRedirect(
		RemovedRenameClass->ClassName,
		AddedRenameClass->NewClass->ClassName);
#endif
}

FAngelscriptClassGenerator::EReloadRequirement FAngelscriptClassGenerator::Setup()
{
	AS_PERF_SCOPE_CLASS_GENERATOR_SETUP();
	FAngelscriptScopeTimer Timer(TEXT("class generator analysis"));

	// Create data structures for each module we're generating for to use during analysis
	for (auto& ModuleData : Modules)
		SetupModule(ModuleData);

	// Analyze all modules we're generating classes for
	for (auto& ModuleData : Modules)
		Analyze(ModuleData);

	for (auto& ModuleData : Modules)
		TryGenerateClassRenameRedirects(ModuleData);

	// Make sure all classes have the reload requirements of their
	// dependencies propagated to them.
	for (auto& ModuleData : Modules)
	{
		for (auto& ClassData : ModuleData.Classes)
		{
			PropagateReloadRequirements(ModuleData, ClassData);
		}
		for (auto& DelegateData : ModuleData.Delegates)
		{
			PropagateReloadRequirements(ModuleData, DelegateData);
		}
	}

	// Determine what kind of reload we require
	EReloadRequirement ReloadReq = EReloadRequirement::SoftReload;
	for (auto& ModuleData : Modules)
	{
		if (ModuleData.ReloadReq > ReloadReq)
			ReloadReq = ModuleData.ReloadReq;
	}
	return ReloadReq;
}

