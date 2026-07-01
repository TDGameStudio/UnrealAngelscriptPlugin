
#include "AngelscriptType.h"
#include "AngelscriptEngine.h"

#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "Binds/Helper_AngelscriptArguments.h"

#include "UObject/UnrealType.h"

#include "AngelscriptInclude.h"
#include "AngelscriptSettings.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_datatype.h"
//#include "as_scriptengine.h"
#include "source/as_context.h"
#include "source/as_datatype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

FAngelscriptTypeUsage FAngelscriptTypeUsage::DefaultUsage;

void* FAngelscriptType::TAG_UserData_Delegate = (void*)(SIZE_T*)0x1;
void* FAngelscriptType::TAG_UserData_Multicast_Delegate = (void*)(SIZE_T*)0x2;

FString FAngelscriptType::GetBoundClassName(UClass* Class)
{
	FString Name = Class->GetPrefixCPP();
	Name += Class->GetName();
	return Name;
}

static FAngelscriptTypeDatabase& GetTypeDatabase()
{
	if (FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		if (FAngelscriptTypeDatabase* DB = Engine->GetTypeDatabase())
		{
			return *DB;
		}
	}
	static FAngelscriptTypeDatabase LegacyDatabase;
	return LegacyDatabase;
}

const TArray<TSharedRef<FAngelscriptType>>& FAngelscriptType::GetTypes()
{
	auto& Database = GetTypeDatabase();
	return Database.RegisteredTypes;
}

void FAngelscriptType::Register(TSharedRef<FAngelscriptType> Type)
{
	auto& Database = GetTypeDatabase();

	FString AngelscriptName = Type->GetAngelscriptTypeName();
	if (Database.TypesByAngelscriptName.Contains(AngelscriptName))
	{
		UE_LOG(Angelscript, Warning, TEXT("Angelscript type %s is already registered. Skipping duplicate registration."), *AngelscriptName);
		return;
	}

	UClass* Class = Type->GetClass(FAngelscriptTypeUsage::DefaultUsage);
	if (Class != nullptr && Database.TypesByClass.Contains(Class))
	{
		UE_LOG(Angelscript, Error, TEXT("Angelscript type %s is bound to UClass %s that already has a binding!"),
			*AngelscriptName, *Class->GetName());
		ensure(false);
		return;
	}

	void* Data = Type->GetData();
	if (Data != nullptr && Database.TypesByData.Contains(Data))
	{
		UE_LOG(Angelscript, Error, TEXT("Angelscript type %s is bound to Data 0x%x that already has a binding!"),
			*AngelscriptName, Data);
		ensure(false);
		return;
	}

	Database.RegisteredTypes.Add(Type);
	Database.TypesByAngelscriptName.Add(AngelscriptName, Type);

	if (Class != nullptr)
	{
		Database.TypesByClass.Add(Class, Type);
	}

	if (Data != nullptr)
	{
		Database.TypesByData.Add(Data, Type);
	}

	if (Type->CanQueryPropertyType())
	{
		Database.TypesImplementingProperties.Add(Type);
	}
}

void FAngelscriptType::ResetTypeDatabase()
{
	auto& Database = GetTypeDatabase();
	Database = FAngelscriptTypeDatabase();
}

void FAngelscriptType::RegisterAlias(const FString& Alias, TSharedRef<FAngelscriptType> Type)
{
	auto& Database = GetTypeDatabase();
	Database.TypesByAngelscriptName.Add(Alias, Type);
}

void FAngelscriptType::RegisterTypeFinder(FTypeFinder Finder)
{
	auto& Database = GetTypeDatabase();
	Database.TypeFinders.Add(Finder);
}

TSharedPtr<FAngelscriptType> FAngelscriptType::GetByAngelscriptTypeName(const FString& Name)
{
	auto* Found = GetTypeDatabase().TypesByAngelscriptName.Find(Name);
	if (Found == nullptr)
		return nullptr;
	else
		return *Found;
}

TSharedPtr<FAngelscriptType> FAngelscriptType::GetByClass(UClass* ForClass)
{
	auto* Found = GetTypeDatabase().TypesByClass.Find(ForClass);
	if (Found == nullptr)
		return nullptr;
	else
		return *Found;
}

TSharedPtr<FAngelscriptType> FAngelscriptType::GetByData(void* ForData)
{
	auto* Found = GetTypeDatabase().TypesByData.Find(ForData);
	if (Found == nullptr)
		return nullptr;
	else
		return *Found;
}

TSharedPtr<FAngelscriptType> FAngelscriptType::GetByProperty(FProperty* Property, bool bQueryTypeFinders)
{
	auto& Database = GetTypeDatabase();

	// Query any Type Finders that were registered for a type
	if (bQueryTypeFinders)
	{
		FAngelscriptTypeUsage Usage;
		for (auto& Finder : Database.TypeFinders)
		{
			if (Finder(Property, Usage))
				return Usage.Type;
		}
	}

	// Type finders didn't result in anything, query the list of
	// types that implement properties.
	for (auto& CheckType : Database.TypesImplementingProperties)
	{
		if (CheckType->MatchesProperty(FAngelscriptTypeUsage::DefaultUsage, Property, FAngelscriptType::EPropertyMatchType::TypeFinder))
			return CheckType;
	}

	// No implementing type found
	return nullptr;
}

FString FAngelscriptType::GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode) const
{
	FString Decl = GetAngelscriptTypeName(Usage);
	if (Usage.SubTypes.Num() != 0)
	{
		Decl += TEXT("<");
		for(int32 i = 0, Num = Usage.SubTypes.Num(); i < Num; ++i)
		{
			if (i != 0)
				Decl += TEXT(", ");

			EAngelscriptDeclarationMode InnerMode = Mode;
			if (Mode == EAngelscriptDeclarationMode::MemberVariable)
				InnerMode = EAngelscriptDeclarationMode::MemberVariable_InContainer;

			Decl += Usage.SubTypes[i].GetAngelscriptDeclaration(InnerMode);
		}
		Decl += TEXT(">");
	}
	return Decl;
}

FString FAngelscriptTypeUsage::GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode Mode) const
{
	if (!Type.IsValid())
		return TEXT("void");

	FString Decl;
	if (bIsConst)
		Decl += TEXT("const ");
	Decl += Type->GetAngelscriptDeclaration(*this, Mode);
	if (bIsReference)
		Decl += TEXT("&");
	return Decl;
}

UClass* FAngelscriptTypeUsage::GetClass() const
{
	if (!Type.IsValid())
		return nullptr;

	UClass* Class = Type->GetClass(*this);
	if (Class != nullptr)
		return Class;

	// Might need to look it up from the scriptclass instead
	if (ScriptClass != nullptr)
	{
		if (ScriptClass->GetFlags() & asOBJ_SCRIPT_OBJECT)
		{
			if (ScriptClass->GetFlags() & asOBJ_REF)
			{
				return (UClass*)ScriptClass->GetUserData();
			}
		}
	}

	return nullptr;
}

FAngelscriptTypeUsage FAngelscriptTypeUsage::FromProperty(FProperty* Property)
{
	FAngelscriptTypeUsage Usage;

	// Query any Type Finders that were registered for a type
	auto& Database = GetTypeDatabase();
	for (auto& Finder : Database.TypeFinders)
	{
		if (Finder(Property, Usage))
			break;
	}

	if (!Usage.Type.IsValid())
	{
		Usage.Type = FAngelscriptType::GetByProperty(Property, false);
	}

	if (Property->HasAnyPropertyFlags(CPF_ConstParm))
		Usage.bIsConst = true;

	const bool bIsReferenceProperty = Property->HasAnyPropertyFlags(CPF_ReferenceParm)
		|| (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ReturnParm));
	if (bIsReferenceProperty)
		Usage.bIsReference = true;

	return Usage;
}

FAngelscriptTypeUsage FAngelscriptTypeUsage::FromProperty(class asITypeInfo* ScriptType, int32 PropertyIndex)
{
	int TypeId;
	ScriptType->GetProperty(PropertyIndex, nullptr, &TypeId);

	return FAngelscriptTypeUsage::FromTypeId(TypeId);
}

FAngelscriptTypeUsage FAngelscriptTypeUsage::FromReturn(asIScriptFunction* Function)
{
	asDWORD ReturnFlags;
	int TypeId = Function->GetReturnTypeId(&ReturnFlags);

	auto Type = FAngelscriptTypeUsage::FromTypeId(TypeId);
	Type.bIsReference = (ReturnFlags & asTM_INOUTREF) != 0;
	Type.bIsConst = (ReturnFlags & asTM_CONST) != 0;
	return Type;
}

FAngelscriptTypeUsage FAngelscriptTypeUsage::FromDataType(const asCDataType& DataType)
{
	int32 TypeId = ((asCScriptEngine*)FAngelscriptEngine::Get().Engine)->GetTypeIdFromDataType(DataType);

	auto Type = FAngelscriptTypeUsage::FromTypeId(TypeId);
	Type.bIsReference = DataType.IsReference();
	Type.bIsConst = DataType.IsObjectConst();
	return Type;
}

FAngelscriptTypeUsage FAngelscriptTypeUsage::FromClass(UClass* Class)
{
	FAngelscriptTypeUsage Usage;
	Usage.Type = FAngelscriptType::GetByClass(Class);
	if (!Usage.Type.IsValid())
	{
		UASClass* ASClass = Cast<UASClass>(Class);
		if (ASClass != nullptr && ASClass->ScriptTypePtr != nullptr)
		{
			Usage.Type = FAngelscriptType::GetScriptObject();
			Usage.ScriptClass = (asITypeInfo*)ASClass->ScriptTypePtr;
		}
	}

	return Usage;
}

FAngelscriptTypeUsage FAngelscriptTypeUsage::FromStruct(UScriptStruct* Struct)
{
	FAngelscriptTypeUsage Usage;
	Usage.Type = FAngelscriptType::GetByData(Struct);
	if (!Usage.Type.IsValid())
	{
		UASStruct* ASStruct = Cast<UASStruct>(Struct);
		if (ASStruct != nullptr && ASStruct->ScriptType != nullptr)
		{
			Usage.Type = FAngelscriptType::GetScriptStruct();
			Usage.ScriptClass = (asITypeInfo*)ASStruct->ScriptType;
		}
	}

	return Usage;
}

FAngelscriptTypeUsage FAngelscriptTypeUsage::FromParam(asIScriptFunction* Function, int32 ParamIndex)
{
	int TypeId;
	asDWORD Flags;
	
	// TODO: Default values

	Function->GetParam(ParamIndex, &TypeId, &Flags);

	auto Type = FAngelscriptTypeUsage::FromTypeId(TypeId);
	Type.bIsReference = (Flags & asTM_INOUTREF) != 0;
	Type.bIsConst = (Flags & asTM_CONST) != 0;
	return Type;
}

FAngelscriptTypeUsage FAngelscriptTypeUsage::FromTypeId(int32 TypeId)
{
	FAngelscriptTypeUsage Usage;

	// Look up numerical types
	switch (TypeId & asTYPEID_MASK_SEQNBR)
	{
	case asTYPEID_BOOL: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("bool")); break;
	case asTYPEID_INT8: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int8")); break;
	case asTYPEID_INT16: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int16")); break;
	case asTYPEID_INT32: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int")); break;
	case asTYPEID_INT64: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int64")); break;
	case asTYPEID_UINT8: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("uint8")); break;
	case asTYPEID_UINT16: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("uint16")); break;
	case asTYPEID_UINT32: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("uint")); break;
	case asTYPEID_UINT64: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("uint64")); break;
	case asTYPEID_FLOAT32: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("float32")); break;
	case asTYPEID_FLOAT64: Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(TEXT("float64")); break;
	}

	// Look up other script types
	if(!Usage.Type.IsValid())
	{
		auto& Manager = FAngelscriptEngine::Get();
		asITypeInfo* ScriptType = Manager.Engine->GetTypeInfoById(TypeId);
		if (ScriptType != nullptr)
		{
			// Script objects use a specialized type that isn't in the normal by-name map
			if (ScriptType->GetFlags() & asOBJ_SCRIPT_OBJECT)
			{
				if (ScriptType->GetFlags() & asOBJ_VALUE)
				{
					void* UserData = ScriptType->GetUserData();
					if (UserData == FAngelscriptType::TAG_UserData_Delegate)
					{
						Usage.Type = FAngelscriptType::GetScriptDelegate();
					}
					else if (UserData == FAngelscriptType::TAG_UserData_Multicast_Delegate)
					{
						Usage.Type = FAngelscriptType::GetScriptMulticastDelegate();
					}
					else if (UserData == nullptr)
					{
						Usage.Type = FAngelscriptType::GetScriptStruct();
					}
					else
					{
						UObject* UserObj = (UObject*)UserData;
						UDelegateFunction* DelegateSignature = Cast<UDelegateFunction>(UserObj);
						if (DelegateSignature != nullptr)
						{
							if (DelegateSignature->HasAnyFunctionFlags(FUNC_MulticastDelegate))
								Usage.Type = FAngelscriptType::GetScriptMulticastDelegate();
							else
								Usage.Type = FAngelscriptType::GetScriptDelegate();
						}
						else
						{
							Usage.Type = FAngelscriptType::GetScriptStruct();
						}
					}
				}
				else
					Usage.Type = FAngelscriptType::GetScriptObject();

				Usage.ScriptClass = ScriptType;
				return Usage;
			}

			if (ScriptType->GetFlags() & asOBJ_ENUM)
			{
				if (ScriptType->GetModule() != nullptr)
				{
					// This is a script enum, so we should use our generic script enum type
					Usage.Type = FAngelscriptType::GetScriptEnum();
					Usage.ScriptClass = ScriptType;
					return Usage;
				}
			}

			Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(ANSI_TO_TCHAR(ScriptType->GetName()));
			Usage.ScriptClass = ScriptType;

			// Add type usage for template subtypes
			for (int32 i = 0, SubTypeCount = ScriptType->GetSubTypeCount(); i < SubTypeCount; ++i)
			{
				Usage.SubTypes.Add(FAngelscriptTypeUsage::FromTypeId(ScriptType->GetSubTypeId(i)));
				if (!Usage.SubTypes.Last().IsValid())
				{
					Usage.Type = nullptr;
					Usage.SubTypes.Empty();
					break;
				}
			}
		}
	}

	return Usage;
}

bool FAngelscriptTypeUsage::operator==(const FAngelscriptTypeUsage& Other) const
{
	return bIsReference == Other.bIsReference
		&& Type == Other.Type
		&& SubTypes == Other.SubTypes
		&& bIsConst == Other.bIsConst
		&& (ScriptClass == Other.ScriptClass || !Type.IsValid() || Type->IsTypeEquivalent(*this, Other));
}

bool FAngelscriptTypeUsage::EqualsUnqualified(const FAngelscriptTypeUsage& Other) const
{
	if (SubTypes.Num() != Other.SubTypes.Num())
	{
		return false;
	}

	for (int i = 0; i < SubTypes.Num(); i++)
	{
		if (!SubTypes[i].EqualsUnqualified(Other.SubTypes[i]))
		{
			return false;
		}
	}

	return Type == Other.Type
		&& (ScriptClass == Other.ScriptClass || !Type.IsValid() || Type->IsTypeEquivalent(*this, Other));
}

FString FAngelscriptTypeUsage::GetFriendlyTypeName() const
{
	if (SubTypes.Num() == 0)
	{
		return Type->GetAngelscriptTypeName(*this);
	}

	FString TypeName = Type->GetAngelscriptTypeName(*this) + "<";
	for (int i = 0; i < SubTypes.Num(); i++)
	{
		TypeName += SubTypes[i].GetFriendlyTypeName();

		if (i != SubTypes.Num() - 1)
		{
			TypeName += ", ";
		}
	}
	TypeName += ">";

	return TypeName;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::GetScriptObject()
{
	return GetTypeDatabase().ScriptObjectType;
}

void FAngelscriptType::SetScriptObject(TSharedPtr<FAngelscriptType> Type)
{
	GetScriptObject() = Type;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::GetScriptStruct()
{
	return GetTypeDatabase().ScriptStructType;
}

void FAngelscriptType::SetScriptStruct(TSharedPtr<FAngelscriptType> Type)
{
	GetScriptStruct() = Type;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::GetScriptDelegate()
{
	return GetTypeDatabase().ScriptDelegateType;
}

void FAngelscriptType::SetScriptDelegate(TSharedPtr<FAngelscriptType> Type)
{
	GetScriptDelegate() = Type;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::GetScriptMulticastDelegate()
{
	return GetTypeDatabase().ScriptMulticastDelegateType;
}

void FAngelscriptType::SetScriptMulticastDelegate(TSharedPtr<FAngelscriptType> Type)
{
	GetScriptMulticastDelegate() = Type;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::GetScriptEnum()
{
	return GetTypeDatabase().ScriptEnumType;
}

void FAngelscriptType::SetScriptEnum(TSharedPtr<FAngelscriptType> Type)
{
	GetScriptEnum() = Type;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::ScriptFloatType()
{
	return GetTypeDatabase().ScriptFloatType;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::ScriptDoubleType()
{
	return GetTypeDatabase().ScriptDoubleType;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::ScriptFloatParamExtendedToDoubleType()
{
	return GetTypeDatabase().ScriptFloatParamExtendedToDoubleType;
}

TSharedPtr<FAngelscriptType>& FAngelscriptType::ScriptBoolType()
{
	return GetTypeDatabase().ScriptBoolType;
}

asITypeInfo* FAngelscriptType::GetArrayTemplateTypeInfo()
{
	return GetTypeDatabase().ArrayTemplateTypeInfo;
}

void FAngelscriptType::SetArrayTemplateTypeInfo(asITypeInfo* TypeInfo)
{
	GetTypeDatabase().ArrayTemplateTypeInfo = TypeInfo;
}

FString FAngelscriptType::BuildFunctionDeclaration(const FAngelscriptTypeUsage& ReturnType, const FString& FunctionName, const TArray<FAngelscriptTypeUsage>& ArgumentTypes, const TArray<FString>& ArgumentNames, const TArray<FString>& ArgumentDefaults, bool bConstMethod)
{
	// Create the angelscript function declaration
	FString Declaration;
	if (ReturnType.IsValid())
		Declaration = ReturnType.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionReturnValue);
	else
		Declaration = TEXT("void");
	Declaration += TEXT(" ");
	Declaration += FunctionName;
	Declaration += TEXT("(");

	bool bAreDefaultsValid = true;
	int32 LastArgumentWithoutDefault = -1;
	TArray<FString, TInlineAllocator<5>> AngelscriptDefaultValues;
	for (int32 i = 0, Count = ArgumentTypes.Num(); i < Count; ++i)
	{
		bool bValidDefault = false;

		FString& AngelscriptDefaultValue = AngelscriptDefaultValues.Emplace_GetRef();

		// Convert unreal default value to angelscript
		if (ArgumentDefaults.IsValidIndex(i)
			&& ArgumentDefaults[i] != TEXT("-"))
		{
			if (ArgumentTypes[i].DefaultValue_UnrealToAngelscript(ArgumentDefaults[i], AngelscriptDefaultValue))
				bValidDefault = true;
		}
		else
		{
			// Fallback defaults on a per-type basis
			if (ArgumentTypes[i].DefaultValue_AngelscriptFallback(AngelscriptDefaultValue))
				bValidDefault = true;
		}

		// Remember if no default value was available
		if (!bValidDefault)
		{
			LastArgumentWithoutDefault = i;
		}
	}

	bool bFirstArgument = true;
	for(int32 i = 0, Count = ArgumentTypes.Num(); i < Count; ++i)
	{
		if (i != 0)
			Declaration += TEXT(",");

		Declaration += ArgumentTypes[i].GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument);
		Declaration += TEXT(" ");
		Declaration += ArgumentNames[i];

		if (i > LastArgumentWithoutDefault)
		{
			if (AngelscriptDefaultValues.IsValidIndex(i)
				&& AngelscriptDefaultValues[i].Len() > 0)
			{
				Declaration += TEXT(" = ");
				Declaration += AngelscriptDefaultValues[i];
			}
		}
	}

	Declaration += TEXT(")");
	if (bConstMethod)
		Declaration += TEXT(" const");
	return Declaration;
}

asIScriptFunction* FAngelscriptType::FindScriptStructHashFunction(asITypeInfo* ScriptType)
{
	if (ScriptType == nullptr)
		return nullptr;

	if (asIScriptFunction* HashFunction = ScriptType->GetMethodByDecl("uint Hash() const"))
		return HashFunction;

	return ScriptType->GetMethodByDecl("uint32 Hash() const");
}

bool FAngelscriptType::GetDebuggerValueFromFunction(asIScriptFunction* InScriptFunction, void* Object, FDebuggerValue& OutValue, class asITypeInfo* ContainerScriptType, UStruct* ContainerClass, const FString& PropertyAddrToSearchFor)
{
	asCScriptFunction* ScriptFunction = (asCScriptFunction*)InScriptFunction;

	bool bHasWorldContext = false;
	if (Object == nullptr)
	{
		if (ScriptFunction->GetObjectType() != nullptr)
			return false;
		if (ScriptFunction->GetParamCount() == 1)
		{
			if (ScriptFunction->hiddenArgumentIndex != 0 || ScriptFunction->hiddenArgumentDefault != "__WorldContext()")
				return false;
			else
				bHasWorldContext = true;
		}
		else if (ScriptFunction->GetParamCount() != 0)
		{
			return false;
		}
	}
	else
	{
		if (ScriptFunction->GetObjectType() == nullptr)
			return false;
		if (ScriptFunction->GetParamCount() != 0)
			return false;
	}

	// Check if the function is blacklisted for debugger evaluation
	UAngelscriptSettings* Config = FAngelscriptEngine::Get().ConfigSettings;

	FString FunctionPath;
	if (ScriptFunction->GetObjectType() != nullptr)
	{
		FunctionPath = ANSI_TO_TCHAR(ScriptFunction->GetObjectType()->GetName());
		FunctionPath += TEXT(".");
	}

	FunctionPath += ANSI_TO_TCHAR(ScriptFunction->GetName());

	// Unconditional blacklist
	if (Config->DebuggerBlacklistAutomaticFunctionEvaluation.Contains(FunctionPath))
		return false;

	if (Object == nullptr || ((ScriptFunction->GetObjectType()->GetFlags() & asOBJ_REF) == 0) || ((UObject*)Object)->GetWorld() == nullptr)
	{
		// Blacklist only for objects that aren't in a valid world
		if (Config->DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext.Contains(FunctionPath))
			return false;
	}

	FAngelscriptTypeUsage ReturnValue = FAngelscriptTypeUsage::FromReturn(ScriptFunction);
	if (ReturnValue.IsValid() && ReturnValue.CanCopy() && ReturnValue.CanBeReturned())
	{
		FDebuggerValue TempValue;
		void* ValueAddress = TempValue.AllocateLiteral(ReturnValue);
		void* ValueRef = nullptr;

		if (ReturnValue.NeedConstruct())
			ReturnValue.ConstructValue(ValueAddress);
		else
			FMemory::Memzero(ValueAddress, ReturnValue.GetValueSize());

		{
			FAngelscriptContext Context(ScriptFunction->GetEngine());
			if (!PrepareAngelscriptContextWithLog(Context, ScriptFunction, TEXT("FAngelscriptType::GetDebuggerValue")))
			{
				return false;
			}
			if (Object != nullptr)
				Context->SetObject(Object);
			if (bHasWorldContext)
				Context->SetArgObject(0, FAngelscriptEngine::TryGetCurrentWorldContextObject());
			Context->Execute();

			if (ReturnValue.bIsReference)
			{
				ReturnValue.GetReturnValue(Context, &ValueRef);
				if (ValueRef != nullptr)
					ReturnValue.CopyValue(ValueRef, ValueAddress);
				else
					ReturnValue.ConstructValue(ValueAddress);

				// Because we copied the value into temporary memory now, the value is no longer a reference (pointer)
				// but the struct itself, so we clear the reference flag
				ReturnValue.bIsReference = false;
			}
			else
			{
				ReturnValue.GetReturnValue(Context, ValueAddress);
			}
		}

		bool bHadValue = false;
		if (ReturnValue.GetDebuggerValue(ValueAddress, TempValue))
		{
			bHadValue = true;
		}

		if (bHadValue)
		{
			auto FetchAddressForMonitoring = [&]() -> void {

				if (ValueRef != nullptr)
				{
					OutValue.NonTemporaryAddress = ValueRef;
					return;
				}

				if (PropertyAddrToSearchFor.IsEmpty())
				{
					return;
				}

				auto* ScriptType = ContainerScriptType;
				FDebuggerValue Value;
				while (ScriptType != nullptr)
				{
					int32 PropCount = ScriptType->GetPropertyCount();
					for (int32 i = 0; i < PropCount; ++i)
					{
						const char* PropName;
						int32 Offset;
						ScriptType->GetProperty(i, &PropName, nullptr, nullptr, nullptr, &Offset);

						FString Name = ANSI_TO_TCHAR(PropName);
						if (Name != PropertyAddrToSearchFor)
							continue;

						FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(ScriptType, i);
						if (PropUsage.GetDebuggerValue((void*)((SIZE_T)Object + (SIZE_T)Offset), Value))
						{
							if (PropUsage.EqualsUnqualified(ReturnValue) && PropUsage.bIsReference == ReturnValue.bIsReference)
							{
								OutValue.NonTemporaryAddress = Value.Address;
							}

							if (Value.AddressToMonitor != nullptr)
							{
								OutValue.SetAddressToMonitor(Value.AddressToMonitor, Value.AddressToMonitorValueSize);
							}
							return;
						}
					}

					if (((asCObjectType*)ScriptType)->derivedFrom != nullptr)
						ScriptType = ((asCObjectType*)ScriptType)->derivedFrom;
					else if (((asCObjectType*)ScriptType)->shadowType != nullptr)
						ScriptType = ((asCObjectType*)ScriptType)->shadowType;
					else
						break;
				}

				if (ContainerClass == nullptr)
				{
					return;
				}

				for (TFieldIterator<FProperty> It(ContainerClass); It; ++It)
				{
					FProperty* Property = *It;
					if (Property->GetName() != PropertyAddrToSearchFor)
						continue;

					// Can't bind static arrays. SAD!
					if (Property->ArrayDim != 1)
						continue;

					FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
					if (!PropUsage.IsValid())
						continue;

					if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(Object), Value, Property))
					{
						if (PropUsage.EqualsUnqualified(ReturnValue) && PropUsage.bIsReference == ReturnValue.bIsReference)
						{
							OutValue.NonTemporaryAddress = Value.Address;
						}

						if (Value.AddressToMonitor != nullptr)
						{
							OutValue.SetAddressToMonitor(Value.AddressToMonitor, Value.AddressToMonitorValueSize);
						}
						return;
					}
				}
			};

			OutValue = MoveTemp(TempValue);
			OutValue.bTemporaryValue = true;
			FetchAddressForMonitoring();
		}
		return bHadValue;
	}

	return false;
}
