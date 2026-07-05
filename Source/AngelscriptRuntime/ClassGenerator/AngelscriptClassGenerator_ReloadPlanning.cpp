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

void FAngelscriptClassGenerator::AddReloadDependency(FReloadPropagation* Source, const FAngelscriptTypeUsage& Type)
{
	EReloadRequirement Req = EReloadRequirement::SoftReload;

	// Recursively propagate for any subtypes in this type (ex. array element type)
	for (const FAngelscriptTypeUsage& SubType : Type.SubTypes)
		AddReloadDependency(Source, SubType);

	// Types that don't have a script class will never reload
	if (Type.ScriptClass == nullptr || !(Type.ScriptClass->GetFlags() & asOBJ_SCRIPT_OBJECT))
		return;

	AddReloadDependency(Source, Type.ScriptClass);
}

void FAngelscriptClassGenerator::AddReloadDependency(FReloadPropagation* Source, asITypeInfo* TypeInfo)
{
	if (TypeInfo == nullptr)
		return;

	FDataRef* Ref = DataRefByNewScriptType.Find(TypeInfo);
	if (Ref != nullptr)
	{
		FModuleData& ModuleData = Modules[Ref->ModuleIndex];
		if (Ref->bIsClass)
		{
			FClassData& ClassData = ModuleData.Classes[Ref->DataIndex];
			check(ClassData.NewClass->ScriptType == TypeInfo);

			PropagateReloadRequirements(ModuleData, ClassData);
			if (!ClassData.bFinishedPropagating || ClassData.bHasOutstandingDependencies)
			{
				ClassData.PendingDependees.AddUnique(Source);
				Source->bHasOutstandingDependencies = true;
			}

			if (ClassData.ReloadReq > Source->ReloadReq)
				Source->ReloadReq = ClassData.ReloadReq;
		}
		else if (Ref->bIsDelegate)
		{
			FDelegateData& DelegateData = ModuleData.Delegates[Ref->DataIndex];
			check(DelegateData.NewDelegate->ScriptType == TypeInfo);

			PropagateReloadRequirements(ModuleData, DelegateData);
			if (!DelegateData.bFinishedPropagating || DelegateData.bHasOutstandingDependencies)
			{
				DelegateData.PendingDependees.AddUnique(Source);
				Source->bHasOutstandingDependencies = true;
			}

			if (DelegateData.ReloadReq > Source->ReloadReq)
				Source->ReloadReq = DelegateData.ReloadReq;
		}
	}
	else
	{
		// If there's any subtypes, we should depend on those as well
		int32 SubTypeCount = TypeInfo->GetSubTypeCount();
		if (SubTypeCount != 0)
		{
			for (int32 i = 0; i < SubTypeCount; ++i)
				AddReloadDependency(Source, TypeInfo->GetSubType(i));
		}
	}
}

void FAngelscriptClassGenerator::PropagateReloadRequirements(FModuleData& ModuleData, FClassData& ClassData)
{
	if (ClassData.bStartedPropagating)
		return;
	ClassData.bStartedPropagating = true;

	// Don't need to propagate if we're already forcing a full reload
	if (ClassData.ReloadReq >= EReloadRequirement::FullReloadRequired)
		return;

	auto ClassDesc = ClassData.NewClass;

	if (!ClassDesc->bSuperIsCodeClass)
	{
		FModuleData* OtherModule = nullptr;
		FClassData* OtherClass = nullptr;
		FDelegateData* OtherDelegate = nullptr;
		asITypeInfo* SuperScriptType = ClassDesc->ScriptType->GetBaseType();

		// Check if it's a class we're reloading
		if (SuperScriptType != nullptr)
			AddReloadDependency(&ClassData, SuperScriptType);
	}

	if (ClassData.NewClass->ScriptType != nullptr)
	{
		asCObjectType* ObjType = (asCObjectType*)ClassData.NewClass->ScriptType;
		int PropCount = ObjType->localProperties.GetLength();
		for (int PropIndex = 0; PropIndex < PropCount; ++PropIndex)
		{
			asCObjectProperty* Prop = ObjType->localProperties[PropIndex];
			if (Prop->type.IsObject())
				AddReloadDependency(&ClassData, Prop->type.GetTypeInfo());
		}

		int MethodCount = ObjType->methods.GetLength();
		for (int MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
		{
			asCScriptFunction* Func = (asCScriptFunction*)ObjType->engine->GetFunctionById(ObjType->methods[MethodIndex]);
			if (Func->returnType.IsObject())
				AddReloadDependency(&ClassData, Func->returnType.GetTypeInfo());

			int ParamCount = Func->parameterTypes.GetLength();
			for (int ParamIndex = 0; ParamIndex < ParamCount; ++ParamIndex)
			{
				if (Func->parameterTypes[ParamIndex].IsObject())
					AddReloadDependency(&ClassData, Func->parameterTypes[ParamIndex].GetTypeInfo());
			}
		}
	}
	else
	{
		for (auto Property : ClassDesc->Properties)
		{
			AddReloadDependency(&ClassData, Property->PropertyType);
		}

		for (auto Function : ClassDesc->Methods)
		{
			AddReloadDependency(&ClassData, Function->ReturnType);
			for (auto Argument : Function->Arguments)
				AddReloadDependency(&ClassData, Argument.Type);
		}
	}

	ClassData.bFinishedPropagating = true;
	ResolvePendingReloadDependees(&ClassData);
}

void FAngelscriptClassGenerator::PropagateReloadRequirements(FModuleData& ModuleData, FDelegateData& DelegateData)
{
	if (DelegateData.bStartedPropagating)
		return;
	DelegateData.bStartedPropagating = true;

	// Don't need to propagate if we're already forcing a full reload
	if (DelegateData.ReloadReq >= EReloadRequirement::FullReloadRequired)
		return;

	auto Function = DelegateData.NewDelegate->Signature;
	AddReloadDependency(&DelegateData, Function->ReturnType);
	for (auto Argument : Function->Arguments)
		AddReloadDependency(&DelegateData, Argument.Type);

	DelegateData.bFinishedPropagating = true;
	ResolvePendingReloadDependees(&DelegateData);
}

void FAngelscriptClassGenerator::ResolvePendingReloadDependees(FReloadPropagation* Source)
{
	check(Source->bFinishedPropagating);

	// Anything that was marked dependent on us before we finished propagation should
	// receive our latest reload requirement via recursive push.
	for (FReloadPropagation* Dependee : Source->PendingDependees)
	{
		if (Source->ReloadReq > Dependee->ReloadReq)
		{
			Dependee->ReloadReq = Source->ReloadReq;

			// Need to recurse so we apply the same reload requirement forward
			ResolvePendingReloadDependees(Dependee);
		}
	}
}

bool FAngelscriptClassGenerator::ShouldFullReload(FClassData& Class)
{
	if (bIsDoingFullReload && Class.ReloadReq >= EReloadRequirement::FullReloadSuggested)
		return true;
	if (HasInterfaceListChanged(Class))
		return true;
	//[UE++]: Materialize brand-new classes during soft reload (no OldClass to link against)
	if (!Class.OldClass.IsValid() && !Class.NewClass->bIsStaticsClass)
		return true;
	//[UE--]
	return false;
}

bool FAngelscriptClassGenerator::HasInterfaceListChanged(FClassData& Class) const
{
	if (!Class.OldClass.IsValid())
		return Class.NewClass->ImplementedInterfaces.Num() > 0;

	return Class.NewClass->ImplementedInterfaces != Class.OldClass->ImplementedInterfaces;
}

FAngelscriptClassGenerator::EReloadRequirement FAngelscriptClassGenerator::GetReloadRequirementForNewScriptType(asITypeInfo* ScriptType) const
{
	const FDataRef* Ref = DataRefByNewScriptType.Find(ScriptType);
	if (Ref == nullptr)
	{
		return EReloadRequirement::SoftReload;
	}

	const FModuleData& ModuleData = Modules[Ref->ModuleIndex];
	if (Ref->bIsClass)
	{
		return ModuleData.Classes[Ref->DataIndex].ReloadReq;
	}

	if (Ref->bIsDelegate)
	{
		return ModuleData.Delegates[Ref->DataIndex].ReloadReq;
	}

	return EReloadRequirement::SoftReload;
}

static UObject* GetReflectedObjectForScriptType(asITypeInfo* ScriptType)
{
	if (ScriptType == nullptr)
	{
		return nullptr;
	}

	void* UserData = ScriptType->GetUserData();
	if (UserData == nullptr
		|| UserData == FAngelscriptType::TAG_UserData_Delegate
		|| UserData == FAngelscriptType::TAG_UserData_Multicast_Delegate)
	{
		return nullptr;
	}

	return static_cast<UObject*>(UserData);
}

bool FAngelscriptClassGenerator::HasReloadedReflectedScriptType(const FAngelscriptTypeUsage& OldType, const FAngelscriptTypeUsage& NewType) const
{
	if (!OldType.Type.IsValid() || !NewType.Type.IsValid())
	{
		return false;
	}

	if (OldType.ScriptClass != NewType.ScriptClass
		&& OldType.ScriptClass != nullptr
		&& NewType.ScriptClass != nullptr
		&& NewType.CanCreateProperty())
	{
		if (UpdatedScriptTypeMap.FindRef(OldType.ScriptClass) == NewType.ScriptClass)
		{
			if (GetReloadRequirementForNewScriptType(NewType.ScriptClass) >= EReloadRequirement::FullReloadSuggested)
			{
				return true;
			}
		}
		else
		{
			UObject* OldReflectedObject = GetReflectedObjectForScriptType(OldType.ScriptClass);
			UObject* NewReflectedObject = GetReflectedObjectForScriptType(NewType.ScriptClass);
			if (NewReflectedObject != nullptr
				&& OldReflectedObject != NewReflectedObject)
			{
				return true;
			}
		}
	}

	if (OldType.SubTypes.Num() != NewType.SubTypes.Num())
	{
		return false;
	}

	for (int32 SubTypeIndex = 0, SubTypeCount = OldType.SubTypes.Num(); SubTypeIndex < SubTypeCount; ++SubTypeIndex)
	{
		if (HasReloadedReflectedScriptType(OldType.SubTypes[SubTypeIndex], NewType.SubTypes[SubTypeIndex]))
		{
			return true;
		}
	}

	return false;
}

bool FAngelscriptClassGenerator::HasReloadedReflectedScriptType(const FAngelscriptFunctionDesc& OldFunction, const FAngelscriptFunctionDesc& NewFunction) const
{
	if (HasReloadedReflectedScriptType(OldFunction.ReturnType, NewFunction.ReturnType))
	{
		return true;
	}

	if (OldFunction.Arguments.Num() != NewFunction.Arguments.Num())
	{
		return false;
	}

	for (int32 ArgIndex = 0, ArgCount = OldFunction.Arguments.Num(); ArgIndex < ArgCount; ++ArgIndex)
	{
		if (HasReloadedReflectedScriptType(OldFunction.Arguments[ArgIndex].Type, NewFunction.Arguments[ArgIndex].Type))
		{
			return true;
		}
	}

	return false;
}

bool FAngelscriptClassGenerator::ShouldFullReload(FEnumData& Enum)
{
	if (bIsDoingFullReload && Enum.bNeedReload)
		return true;
	if (!Enum.OldEnum.IsValid())
		return true;
	return false;
}

bool FAngelscriptClassGenerator::ShouldFullReload(FDelegateData& Delegate)
{
	if (bIsDoingFullReload && Delegate.ReloadReq >= EReloadRequirement::FullReloadSuggested)
		return true;
	//[UE++]: Materialize brand-new delegates during soft reload
	if (!Delegate.OldDelegate.IsValid())
		return true;
	//[UE--]
	return false;
}
