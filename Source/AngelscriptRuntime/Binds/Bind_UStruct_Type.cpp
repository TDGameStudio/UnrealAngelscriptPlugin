#include "Bind_UStruct.h"

#include "AngelscriptBindDatabase.h"
#include "AngelscriptDebugValue.h"
#include "AngelscriptEngine.h"

#include "Binds/Bind_Helpers.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "StaticJIT/AngelscriptStaticJIT.h"

#include "UObject/GarbageCollection.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/ScriptMacros.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

static const FName NAME_Struct_MetaDebuggable("ScriptDebuggable");

FUStructType::FUStructType(
	UScriptStruct* InStruct,
	const FString& InStructName,
	const FAngelscriptBindDatabase& InBindDatabase)
	: Struct(InStruct)
	, StructName(InStructName)
	, BindDatabase(&InBindDatabase)
{
}

bool FUStructType::IsUnrealStruct() const
{ return true; }

bool FUStructType::IsValidType(const FAngelscriptTypeUsage& Usage) const
{
	return Struct != nullptr || Usage.ScriptClass != nullptr;
}

UStruct* FUStructType::GetUnrealStruct(const FAngelscriptTypeUsage& Usage) const
{
	return GetStruct(Usage);
}

bool FUStructType::IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const
{
	// C++ structs have individual type instances, so we don't need to check this
	if (Struct != nullptr)
		return true;

	// If the scriptclass is identical we don't need to check it
	if (Usage.ScriptClass == Other.ScriptClass)
		return true;

	// Shouldn't happen, safety check
	if (Usage.ScriptClass == nullptr || Other.ScriptClass == nullptr)
		return false;

	// Compare script structs by name, because we are likely comparing for changes during a compile
	if (((asCObjectType*)Usage.ScriptClass)->name == ((asCObjectType*)Other.ScriptClass)->name)
		return true;

	return false;
}

UScriptStruct* FUStructType::GetStruct(const FAngelscriptTypeUsage& Usage) const
{
	if (Struct != nullptr)
		return Struct;
	if (Usage.ScriptClass == nullptr)
		return nullptr;
	return (UScriptStruct*)Usage.ScriptClass->GetUserData();
}

asITypeInfo* FUStructType::GetScriptType(const FAngelscriptTypeUsage& Usage) const
{
	if (Usage.ScriptClass != nullptr)
		return Usage.ScriptClass;
	return ScriptTypeInfo;
}

UScriptStruct::ICppStructOps* FUStructType::GetOps(const FAngelscriptTypeUsage& Usage) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	if (UsedStruct == nullptr)
		return nullptr;
	return UsedStruct->GetCppStructOps();
}

FString FUStructType::GetAngelscriptTypeName() const
{
	ensure(Struct != nullptr);
	return StructName;
}

FString FUStructType::GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const
{
	if (Struct != nullptr)
		return StructName;
	else if (Usage.ScriptClass != nullptr)
		return ANSI_TO_TCHAR(Usage.ScriptClass->GetName());

	ensure(false);
	return TEXT("");
}

void* FUStructType::GetData() const
{
	return Struct;
}

bool FUStructType::HasReferences(const FAngelscriptTypeUsage& Usage) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);

	// We're a script struct, but we haven't been generated yet.
	//  Let's just assume we have references for now.
	if (UsedStruct == nullptr)
		return true;

	if (UsedStruct->StructFlags & STRUCT_AddStructReferencedObjects)
		return true;

	TArray<const FStructProperty*> EncounteredStructProps;

	FProperty* Property = UsedStruct->PropertyLink;
	while( Property )
	{
		if (Property->ContainsObjectReference(EncounteredStructProps))
			return true;
		Property = Property->PropertyLinkNext;
	}

	return false;
}

void FUStructType::EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	check(UsedStruct);

	if (!HasReferences(Usage))
		return;

	if (UsedStruct->StructFlags & STRUCT_AddStructReferencedObjects)
	{
		UE::GC::StructAROFn StructARO = UsedStruct->GetCppStructOps()->AddStructReferencedObjects();
		Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::MemberARO, StructARO) );
	}

	TArray<const FStructProperty*> EncounteredStructProps;
	for (FProperty* Property = UsedStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		Property->EmitReferenceInfo(*Params.Schema, Params.AtOffset, EncounteredStructProps, *Params.DebugPath);
	}
}

bool FUStructType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	return IsValidType(Usage);
}

FProperty* FUStructType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);

	auto* StructProp = new FStructProperty(Params.Outer, Params.PropertyName);
	StructProp->Struct = UsedStruct;
	if (CanHashValue(Usage))
		StructProp->SetPropertyFlags(CPF_HasGetValueTypeHash);

	return StructProp;
}

bool FUStructType::CanQueryPropertyType() const
{
	return false;
}

bool FUStructType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	const FStructProperty* StructProp = CastField<FStructProperty>(Property);
	if (StructProp == nullptr)
		return false;

	UScriptStruct* UsedStruct = GetStruct(Usage);
	if (UsedStruct == nullptr)
	{
		// Workaround: We don't know our actual type yet, so
		// we compare the script types by name.
		check(Usage.ScriptClass != nullptr);
		FString CheckName = ANSI_TO_TCHAR(Usage.ScriptClass->GetName());
		CheckName.RemoveFromStart(TEXT("F"));

		FString PropClassName = StructProp->Struct->GetName();
		return PropClassName == CheckName;
	}
	else
	{
		if (StructProp->Struct != GetStruct(Usage))
			return false;
		return true;
	}
}

bool FUStructType::CanCopy(const FAngelscriptTypeUsage& Usage) const
{
	return IsValidType(Usage);
}

bool FUStructType::NeedCopy(const FAngelscriptTypeUsage& Usage) const
{
	auto* Ops = GetOps(Usage);
	return Ops == nullptr || !Ops->IsPlainOldData();
}

bool FUStructType::CanHashValue(const FAngelscriptTypeUsage& Usage) const
{
	auto* Ops = GetOps(Usage);
	if (Ops != nullptr && Ops->HasGetTypeHash())
		return true;

	asITypeInfo* ScriptType = GetScriptType(Usage);
	asCObjectType* ObjectType = ScriptType != nullptr ? CastToObjectType((asCTypeInfo*)ScriptType) : nullptr;
	if (ObjectType == nullptr || ObjectType->GetFirstMethod("Hash") == nullptr)
		return false;

	return FAngelscriptType::FindScriptStructHashFunction(ScriptType) != nullptr;
}

uint32 FUStructType::GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const
{
	auto* Ops = GetOps(Usage);
	if (Ops == nullptr)
		return 0;
	return Ops->GetStructTypeHash(Address);
}

void FUStructType::CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	return UsedStruct->CopyScriptStruct(DestinationPtr, SourcePtr, 1);
}

bool FUStructType::CanConstruct(const FAngelscriptTypeUsage& Usage) const
{
	return IsValidType(Usage);
}

bool FUStructType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	if (UsedStruct == nullptr)
		return true;

	auto* Ops = GetOps(Usage);
	return Ops == nullptr || !Ops->HasNoopConstructor() || UsedStruct->GetPropertiesSize() > Ops->GetSize();
}

void FUStructType::ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	UsedStruct->InitializeStruct(DestinationPtr, 1);
}

bool FUStructType::CanDestruct(const FAngelscriptTypeUsage& Usage) const
{
	return IsValidType(Usage);
}

bool FUStructType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	if (UsedStruct == nullptr)
		return true;

	return !(UsedStruct->StructFlags & (STRUCT_IsPlainOldData | STRUCT_NoDestructor));
}

void FUStructType::DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	UsedStruct->DestroyStruct(DestinationPtr, 1);
}

int32 FUStructType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	if (UsedStruct == nullptr)
		return Usage.ScriptClass->GetSize();
	return UsedStruct->GetPropertiesSize();
}

bool FUStructType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

bool FUStructType::IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	return UsedStruct->CompareScriptStruct(SourcePtr, DestinationPtr, 0);
}

bool FUStructType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{ return true; }

void FUStructType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	uint8* StructMemory = (uint8*)Data.StackPtr;
	UsedStruct->InitializeStruct(StructMemory, 1);

	if (Usage.bIsReference)
	{
		uint8& RefValue = Stack.StepCompiledInRef<FStructProperty, uint8>(StructMemory);
		Context->SetArgAddress(ArgumentIndex, &RefValue);
	}
	else
	{
		Stack.StepCompiledIn<FStructProperty>(StructMemory);
		Context->SetArgObject(ArgumentIndex, StructMemory);
	}
}

bool FUStructType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FUStructType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
{
	if (Usage.bIsReference)
	{
		*(void**)Destination = Context->GetReturnAddress();
	}
	else
	{
		void* ReturnedObject = Context->GetReturnObject();
		if (ReturnedObject == nullptr)
			return;

		UScriptStruct* UsedStruct = GetStruct(Usage);
		UsedStruct->CopyScriptStruct(Destination, ReturnedObject, 1);
	}
}

int32 FUStructType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	UScriptStruct* UsedStruct = GetStruct(Usage);
	if (UsedStruct != nullptr)
		return UsedStruct->GetMinAlignment();
	if (Usage.ScriptClass != nullptr)
		return Usage.ScriptClass->alignment;

	checkf(false, TEXT("Attempted to request alignment from an angelscript struct type without type information."));
	return 8;
}

bool FUStructType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
{
	void* NativeValue = (void*)&Usage.ResolvePrimitive<int>(Address);
	auto* UsedStruct = GetStruct(Usage);

	if (Struct != nullptr)
		Value.Type = Struct->GetStructCPPName();
	else if(Usage.ScriptClass != nullptr)
		Value.Type = Usage.ScriptClass->GetName();

	bool bHasToString = false;

	auto* ScriptType = GetScriptType(Usage);
	if (ScriptType != nullptr)
	{
		auto* Func = ScriptType->GetMethodByDecl("FString ToString() const");
		if (Func != nullptr)
		{
			FAngelscriptContext Context(Func->GetEngine());
			if (PrepareAngelscriptContextWithLog(Context, Func, TEXT("FUStructType::GetDebuggerValue")))
			{
				Context->SetObject(NativeValue);
				Context->Execute();

				FString* ReturnString = (FString*)Context->GetReturnObject();
				if (ReturnString != nullptr)
				{
					Value.Value = *ReturnString;
					bHasToString = true;
				}
			}
		}
	}

	if(!bHasToString)
		Value.Value = FString::Printf(TEXT("%s{}"), *Value.Type);

	Value.Usage = Usage;
	Value.Address = Address;
	Value.bHasMembers = true;

	return true;
}

bool FUStructType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
{
	void* NativeValue = (void*)&Usage.ResolvePrimitive<int>(Address);

	bool bHasMembers = false;
	auto* UsedStruct = GetStruct(Usage);

	TSet<FString> FoundProperties;

	for (TFieldIterator<FProperty> It(UsedStruct); It; ++It)
	{
		FProperty* Property = *It;
#if WITH_EDITOR
		bool bMetaDebuggable = Property->HasMetaData(NAME_Struct_MetaDebuggable);
#else
		bool bMetaDebuggable = false;
#endif

		if (Struct != nullptr)
		{
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && (!Property->HasAnyPropertyFlags(CPF_Edit)
			 || Property->HasAllPropertyFlags(CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate))
				&& !bMetaDebuggable)
			{
				continue;
			}
		}

		// Can't bind static arrays. SAD!
		if (Property->ArrayDim != 1)
			continue;

		FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
		if (!PropUsage.IsValid())
			continue;

		FDebuggerValue DbgValue;
		if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(NativeValue), DbgValue, Property))
		{
			DbgValue.Name = Property->GetName();
			if (bMetaDebuggable)
				DbgValue.Name = TEXT("<") + DbgValue.Name + TEXT(">");
			FoundProperties.Add(DbgValue.Name);
			Scope.Values.Add(MoveTemp(DbgValue));
			bHasMembers = true;
		}
	}

	auto* ScriptType = GetScriptType(Usage);
	if (ScriptType != nullptr)
	{
		int32 PropCount = ScriptType->GetPropertyCount();
		for (int32 i = 0; i < PropCount; ++i)
		{
			const char* PropName;
			int32 Offset;
			ScriptType->GetProperty(i, &PropName, nullptr, nullptr, nullptr, &Offset);

			FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(ScriptType, i);

			FDebuggerValue VarValue;
			if (PropUsage.GetDebuggerValue((void*)((SIZE_T)NativeValue + (SIZE_T)Offset), VarValue))
			{
				VarValue.Name = ANSI_TO_TCHAR(PropName);
				if (!FoundProperties.Contains(VarValue.Name))
				{
					FoundProperties.Add(VarValue.Name);
					Scope.Values.Add(MoveTemp(VarValue));
					bHasMembers = true;
				}
			}
		}
	}

	if (ScriptType != nullptr)
	{
		int32 FuncCount = ScriptType->GetMethodCount();
		for (int32 i = 0; i < FuncCount; ++i)
		{
			asIScriptFunction* ScriptFunction = ScriptType->GetMethodByIndex(i);
			if (!ScriptFunction->IsReadOnly())
				continue;
			if (ScriptFunction->GetParamCount() != 0)
				continue;

			FString FuncName = ANSI_TO_TCHAR(ScriptFunction->GetName());
			if (FuncName.StartsWith(TEXT("Get")))
			{
				FString VarName = FuncName.Mid(3);

				FDebuggerValue VarValue;
				if (!FoundProperties.Contains(VarName))
				{
					if (GetDebuggerValueFromFunction(ScriptFunction, NativeValue, VarValue, GetScriptType(Usage), UsedStruct, VarName))
					{
						VarValue.Name = VarName;
						VarValue.Name += TEXT("$");
						Scope.Values.Add(MoveTemp(VarValue));
						bHasMembers = true;
					}
					FoundProperties.Add(VarName);
				}
			}
		}
	}

	return bHasMembers;
}

bool FUStructType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
{
	void* NativeValue = (void*)&Usage.ResolvePrimitive<int>(Address);
	auto* UsedStruct = GetStruct(Usage);

	auto* ScriptType = GetScriptType(Usage);
	if (Member.EndsWith(TEXT("()")) && ScriptType != nullptr)
	{
		FString FunctionName = Member.Mid(0, Member.Len() - 2);
		asIScriptFunction* ScriptFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
		if (ScriptFunction != nullptr)
		{
			if (GetDebuggerValueFromFunction(ScriptFunction, NativeValue, Value, ScriptType, UsedStruct, FunctionName.Mid(3)))
			{
				Value.Name = Member;
				return true;
			}
		}
	}

	if (Member.EndsWith(TEXT("$")) && ScriptType != nullptr)
	{
		FString FunctionName = TEXT("Get") + Member.Mid(0, Member.Len() - 1);
		asIScriptFunction* ScriptFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
		if (ScriptFunction != nullptr)
		{
			if (GetDebuggerValueFromFunction(ScriptFunction, NativeValue, Value, ScriptType, UsedStruct, FunctionName.Mid(3)))
			{
				Value.Name = Member;
				return true;
			}
		}
	}

	for (TFieldIterator<FProperty> It(UsedStruct); It; ++It)
	{
		FProperty* Property = *It;
#if WITH_EDITOR
		bool bMetaDebuggable = Property->HasMetaData(NAME_Struct_MetaDebuggable);
#else
		bool bMetaDebuggable = false;
#endif

		if (Struct != nullptr)
		{
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && (!Property->HasAnyPropertyFlags(CPF_Edit)
			 || Property->HasAllPropertyFlags(CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate))
				&& !bMetaDebuggable)
			{
				continue;
			}
		}

		FString PropertyName = Property->GetName();
		if (bMetaDebuggable)
			PropertyName = TEXT("<") + PropertyName + TEXT(">");

		if (PropertyName != Member)
			continue;

		if (Property->ArrayDim != 1)
			continue;

		FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
		if (!PropUsage.IsValid())
			continue;

		if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(NativeValue), Value, Property))
			return true;
	}

	if (ScriptType != nullptr)
	{
		int32 PropCount = ScriptType->GetPropertyCount();
		for (int32 i = 0; i < PropCount; ++i)
		{
			const char* PropName;
			int32 Offset;
			ScriptType->GetProperty(i, &PropName, nullptr, nullptr, nullptr, &Offset);

			FString Name = ANSI_TO_TCHAR(PropName);
			if (Name != Member)
				continue;

			FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(ScriptType, i);
			if (PropUsage.GetDebuggerValue((void*)((SIZE_T)NativeValue + (SIZE_T)Offset), Value))
			{
				Value.Name = Name;
				return true;
			}
		}
	}

	if (ScriptType != nullptr)
	{
		FString FunctionName = TEXT("Get") + Member;
		asIScriptFunction* ScriptFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
		if (ScriptFunction != nullptr && ScriptFunction->IsReadOnly())
		{
			if (GetDebuggerValueFromFunction(ScriptFunction, NativeValue, Value, ScriptType, UsedStruct, FunctionName.Mid(3)))
			{
				Value.Name = Member;
				return true;
			}
		}
	}

	return false;
}

bool FUStructType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	if (Struct == nullptr)
	{
		// Script types that are POD can still be represented natively. Helps for template containers
#if AS_CAN_GENERATE_JIT
		asCObjectType* ObjectType = (asCObjectType*)Usage.ScriptClass;
		if (ObjectType != nullptr && (ObjectType->flags & asOBJ_POD) != 0
			&& FAngelscriptEngine::Get().StaticJIT != nullptr
			&& !FAngelscriptEngine::Get().StaticJIT->IsTypePotentiallyDifferent(ObjectType)
			&& ObjectType->GetFirstMethod("opAssign") == nullptr
		)
		{
			int Size = GetValueSize(Usage);
			if (Size == 0)
				OutCppForm.CppType = FString::Printf(TEXT("TScriptPODEmptyStruct<%d>"), GetValueAlignment(Usage));
			else
				OutCppForm.CppType = FString::Printf(TEXT("TScriptPODStruct<%d,%d>"), GetValueSize(Usage), GetValueAlignment(Usage));
			return true;
		}
#endif

		return false;
	}
	else
	{
		FString HeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Struct, *BindDatabase);
		if (HeaderPath.Len() != 0)
		{
			OutCppForm.CppType = StructName;
			if (!HeaderPath.Contains(TEXT("NoExportTypes.h")))
				OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *HeaderPath);
		}

		return true;
	}
}
