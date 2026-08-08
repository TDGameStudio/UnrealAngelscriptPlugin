#include "Binds/Bind_TSoftObjectPtr.h"

#include "AngelscriptBindDatabase.h"
#include "AngelscriptEngine.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

FBaseSoftReferenceType::FBaseSoftReferenceType(const FAngelscriptBindDatabase& InBindDatabase)
	: BindDatabase(&InBindDatabase)
{
}

UClass* FBaseSoftReferenceType::GetSubTypeClass(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.SubTypes.Num() == 0)
		return nullptr;
	return Usage.SubTypes[0].GetClass();
}

UClass* FBaseSoftReferenceType::GetClassOfObject(const FAngelscriptTypeUsage& Usage) const
{
	return nullptr;
}

bool FBaseSoftReferenceType::DescribesCompleteType(const FAngelscriptTypeUsage& Usage) const
{
	return Usage.SubTypes.Num() >= 1 && Usage.SubTypes[0].IsValid();
}

bool FBaseSoftReferenceType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.SubTypes.Num() == 0)
		return false;

	if (!Usage.SubTypes[0].IsValid())
		return false;

	// At analyze-time we don't have an actual UClass yet for script classes, so assume it will be created in time
	if (Usage.SubTypes[0].Type->IsObjectPointer() && Usage.SubTypes[0].Type.Get() != this && Usage.SubTypes[0].ScriptClass != nullptr)
		return true;

	UClass* SubClass = Usage.SubTypes[0].GetClass();
	if (SubClass == nullptr)
		return false;

	return true;
}

bool FBaseSoftReferenceType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FBaseSoftReferenceType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const
{
	FSoftObjectPtr* ValuePtr = (FSoftObjectPtr*)Data.StackPtr;
	new(ValuePtr) FSoftObjectPtr();

	if (Usage.bIsReference)
	{
		FSoftObjectPtr& ObjRef = Stack.StepCompiledInRef<FSoftObjectProperty, FSoftObjectPtr>(ValuePtr);
		Context->SetArgAddress(ArgumentIndex, &ObjRef);
	}
	else
	{
		Stack.StepCompiledIn<FSoftObjectProperty>(ValuePtr);
		Context->SetArgObject(ArgumentIndex, ValuePtr);
	}
}

bool FBaseSoftReferenceType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FBaseSoftReferenceType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
{
	if(Usage.bIsReference)
	{
		*(FSoftObjectPtr**)Destination = (FSoftObjectPtr*)Context->GetReturnAddress();
	}
	else
	{
		void* ReturnedObject = Context->GetReturnObject();
		if (ReturnedObject == nullptr)
			return;
		*(FSoftObjectPtr*)Destination = *(FSoftObjectPtr*)ReturnedObject;
	}
}

bool FBaseSoftReferenceType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
{
	FSoftObjectPtr& SoftPtr = Usage.ResolvePrimitive<FSoftObjectPtr>(Address);

	UObject* Object = SoftPtr.Get();
	if (Object == nullptr)
	{
		Value.Value = FString::Printf(TEXT("{ Pending %s }"), *SoftPtr.ToString());
		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.bHasMembers = false;
		return true;
	}

	FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(GetClassOfObject(Usage)));
	if (ObjectUsage.IsValid())
	{
		const UObject*& ObjectRef = Value.AllocatePODLiteral<const UObject*>();
		ObjectRef = Object;

		if (ObjectUsage.GetDebuggerValue(&ObjectRef, Value))
		{
			Value.Type = Usage.GetAngelscriptDeclaration();
			return true;
		}
	}

	Value.Value = FString::Printf(TEXT("{ Object %s }"), *SoftPtr.ToString());
	Value.Type = Usage.GetAngelscriptDeclaration();
	Value.Usage = Usage;
	Value.Address = Address;
	Value.bHasMembers = false;
	return true;
}

bool FBaseSoftReferenceType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
{
	FSoftObjectPtr& SoftPtr = Usage.ResolvePrimitive<FSoftObjectPtr>(Address);

	UObject* Object = SoftPtr.Get();
	if (Object == nullptr)
		return false;

	FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(GetClassOfObject(Usage)));
	if (ObjectUsage.IsValid())
	{
		if (ObjectUsage.GetDebuggerScope(&Object, Scope))
			return true;
	}

	return false;
}

bool FBaseSoftReferenceType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
{
	FSoftObjectPtr& SoftPtr = Usage.ResolvePrimitive<FSoftObjectPtr>(Address);

	UObject* Object = SoftPtr.Get();
	if (Object == nullptr)
		return false;

	FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(GetClassOfObject(Usage)));
	if (ObjectUsage.IsValid())
	{
		if (ObjectUsage.GetDebuggerMember(&Object, Member, Value))
			return true;
	}

	return false;
}

FSoftObjectPtrType::FSoftObjectPtrType(const FAngelscriptBindDatabase& InBindDatabase)
	: FBaseSoftReferenceType(InBindDatabase)
{
}

FString FSoftObjectPtrType::GetAngelscriptTypeName() const
{
	return TEXT("TSoftObjectPtr");
}

UClass* FSoftObjectPtrType::GetClassOfObject(const FAngelscriptTypeUsage& Usage) const
{
	return GetSubTypeClass(Usage);
}

FProperty* FSoftObjectPtrType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const
{
	if (Usage.SubTypes.Num() == 0)
		return nullptr;

	auto* ObjectProp = new FSoftObjectProperty(Params.Outer, Params.PropertyName);
	ObjectProp->PropertyClass = GetClassOfObject(Usage);

	return ObjectProp;
}

bool FSoftObjectPtrType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	const FSoftObjectProperty* ObjProp = CastField<FSoftObjectProperty>(Property);
	if (ObjProp == nullptr)
		return false;
	return true;
}

bool FSoftObjectPtrType::CanQueryPropertyType() const
{
	return false;
}

bool FSoftObjectPtrType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	UClass* MetaClass = GetClassOfObject(Usage);
	if (MetaClass != nullptr)
	{
		const FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(MetaClass, *BindDatabase);
		if (!ClassHeaderPath.IsEmpty())
		{
			OutCppForm.CppType = FString::Printf(TEXT("TSoftObjectPtr<%s%s>"), MetaClass->GetPrefixCPP(), *MetaClass->GetName());
			OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *ClassHeaderPath);
		}
	}

	OutCppForm.CppGenericType = TEXT("TSoftObjectPtr<UObject>");
	OutCppForm.TemplateObjectForm = TEXT("TSoftObjectPtr<UObject>");
	return true;
}

FSoftClassPtrType::FSoftClassPtrType(const FAngelscriptBindDatabase& InBindDatabase)
	: FBaseSoftReferenceType(InBindDatabase)
{
}

FString FSoftClassPtrType::GetAngelscriptTypeName() const
{
	return TEXT("TSoftClassPtr");
}

UClass* FSoftClassPtrType::GetClassOfObject(const FAngelscriptTypeUsage& Usage) const
{
	return UClass::StaticClass();
}

FProperty* FSoftClassPtrType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const
{
	if (Usage.SubTypes.Num() == 0)
		return nullptr;

	auto* ClassProp = new FSoftClassProperty(Params.Outer, Params.PropertyName);
	ClassProp->PropertyClass = UClass::StaticClass();
	ClassProp->MetaClass = GetSubTypeClass(Usage);

	return ClassProp;
}

bool FSoftClassPtrType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	const FSoftClassProperty* ClassProp = CastField<FSoftClassProperty>(Property);
	if (ClassProp == nullptr)
		return false;
	return true;
}

bool FSoftClassPtrType::CanQueryPropertyType() const
{
	return false;
}

bool FSoftClassPtrType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	UClass* MetaClass = GetSubTypeClass(Usage);
	if (MetaClass != nullptr)
	{
		const FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(MetaClass, *BindDatabase);
		if (!ClassHeaderPath.IsEmpty())
		{
			OutCppForm.CppType = FString::Printf(TEXT("TSoftClassPtr<%s%s>"), MetaClass->GetPrefixCPP(), *MetaClass->GetName());
			OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *ClassHeaderPath);
		}
	}

	OutCppForm.CppGenericType = TEXT("TSoftClassPtr<UObject>");
	OutCppForm.TemplateObjectForm = TEXT("TSoftClassPtr<UObject>");
	return true;
}
