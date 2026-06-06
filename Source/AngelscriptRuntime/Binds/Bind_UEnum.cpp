#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptDocs.h"
#include "AngelscriptType.h"
#include "AngelscriptDebugValue.h"
#include "AngelscriptBindDatabase.h"
#include "Testing/AngelscriptEnumTableBaselineProbe.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "Engine/UserDefinedEnum.h"

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

static const FName NAME_ENUM_UnderlyingType("UnderlyingType");
struct FEnumType : FAngelscriptType
{
	UEnum* Enum;

	FEnumType(UEnum* InEnum)
		: Enum(InEnum)
	{}

	bool IsPrimitive() const override
	{
		return true;
	}

	FString GetAngelscriptTypeName() const override
	{
		if (Enum == nullptr)
		{
			ensure(false); // Should not happen
			return "int";
		}
		return Enum->GetName();
	}

	FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Enum != nullptr)
			return Enum->GetName();
		else if (Usage.ScriptClass != nullptr)
			return ANSI_TO_TCHAR(Usage.ScriptClass->GetName());

		ensure(false);
		return TEXT("");
	}

	void* GetData() const override
	{
		return Enum;
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		return Enum != nullptr || Usage.ScriptClass != nullptr;
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		auto* EnumProp = CastField<FEnumProperty>(Property);
		UEnum* PropertyEnum = nullptr;
		if (EnumProp != nullptr)
		{
			//PropertyEnum = EnumProp->Enum;
			PropertyEnum = EnumProp->GetEnum();
		}
		else
		{
			auto* ByteProp = CastField<FByteProperty>(Property);
			if (ByteProp != nullptr)
				PropertyEnum = ByteProp->Enum;
		}

		if (PropertyEnum == nullptr)
			return false;

		auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
		return PropertyEnum == UsedEnum;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
		check(UsedEnum);

		if (UsedEnum->GetCppForm() == UEnum::ECppForm::EnumClass || UsedEnum->IsA<UUserDefinedEnum>())
		{
			auto* EnumProp = new FEnumProperty(Params.Outer, Params.PropertyName, RF_Public);
			auto* ByteProp = new FByteProperty(EnumProp, NAME_ENUM_UnderlyingType, RF_Public);

			EnumProp->SetEnum(UsedEnum);
			EnumProp->AddCppProperty(ByteProp);

			return EnumProp;
		}
		else
		{
			auto* ByteProp = new FByteProperty(Params.Outer, Params.PropertyName, RF_Public);
			ByteProp->Enum = UsedEnum;
			return ByteProp;
		}
	}

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override
	{
		// C++ enums have individual type instances, so we don't need to check this
		if (Enum != nullptr)
			return true;

		// If the scriptclass is identical we don't need to check it
		if (Usage.ScriptClass == Other.ScriptClass)
			return true;

		// Shouldn't happen, safety check
		if (Usage.ScriptClass == nullptr || Other.ScriptClass == nullptr)
			return false;

		// Compare script enums by name, because we are likely comparing for changes during a compile
		if (((asCTypeInfo*)Usage.ScriptClass)->name == ((asCTypeInfo*)Other.ScriptClass)->name)
			return true;

		return false;
	}

	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		*(uint8*)DestinationPtr = *(uint8*)SourcePtr;
	}

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		return *(uint8*)DestinationPtr == *(uint8*)SourcePtr;
	}

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return false; }
	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override {}

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return false; }
	void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override {}

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		return 1;
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		uint8* ValuePtr = (uint8*)Data.StackPtr;
		if (Usage.bIsReference)
		{
			uint8& ObjRef = Stack.StepCompiledInRef<FEnumProperty, uint8>(ValuePtr);
			Context->SetArgAddress(ArgumentIndex, &ObjRef);
		}
		else
		{
			Stack.StepCompiledIn<FEnumProperty>(ValuePtr);
			Context->SetArgByte(ArgumentIndex, *ValuePtr);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return !Usage.bIsReference;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		*(uint8*)Destination = (uint8)Context->GetReturnByte();
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		OutValue = InValue;
		if (!OutValue.Contains(TEXT("::")))
		{
			FString EnumName = Enum != nullptr ? Enum->GetName() : ANSI_TO_TCHAR(Usage.ScriptClass->GetName());
			if (OutValue.Len() == 0)
			{
				if (Enum == nullptr)
					return false;

				// Unreal can send us an empty value if this is the 0 value for the enum
				OutValue = Enum->GetNameStringByValue(0);
				OutValue = FString::Printf(TEXT("%s::%s"), *EnumName, *OutValue);
			}
			else
			{
				OutValue = FString::Printf(TEXT("%s::%s"), *EnumName, *OutValue);
			}
		}
		return true;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		OutValue = InValue;
		int32 ScopePos = OutValue.Find(TEXT("::"));
		if (ScopePos != -1)
		{
			OutValue = OutValue.Mid(ScopePos+2);
			OutValue.TrimStartAndEndInline();
		}
		return true;
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
	{
		return 1;
	}

	bool CanHashValue(const FAngelscriptTypeUsage& Usage) const
	{
		return true;
	}

	uint32 GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const
	{
		return GetTypeHash(*(uint8*)Address);
	}

	FASDebugValue* CreateDebugValue(const FAngelscriptTypeUsage& Usage, FDebugValuePrototype& Values, int32 Offset) const override
	{
		if(Usage.bIsReference)
			return Values.Create<TDebug<uint8*>>(Offset);
		else
			return Values.Create<TDebug<uint8>>(Offset);
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
		int32 EnumValue = 0;

		EnumValue = Usage.ResolvePrimitive<uint8>(Address);
		FString EnumName = UsedEnum->GetNameByValue(EnumValue).ToString();

		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.Value = FString::Printf(TEXT("%s (%d)"), *EnumName, EnumValue);

		return true;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		if (Enum == nullptr)
		{
			// Script enums are _always_ 1 byte
			check(GetValueSize(Usage) == 1);
			OutCppForm.CppType = TEXT("uint8");
			return true;
		}

		if (Enum->GetCppForm() == UEnum::ECppForm::EnumClass)
			OutCppForm.CppGenericType = TEXT("uint8");
		else
			OutCppForm.bDisallowNativeNest = true;

		if (!Usage.bIsReference)
		{
			FString HeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Enum);
			if (HeaderPath.Len() != 0)
			{
				OutCppForm.CppType = Enum->GetName();
				if (Enum->GetCppForm() == UEnum::ECppForm::Namespaced)
					OutCppForm.CppType += TEXT("::Type");

				if (!HeaderPath.Contains(TEXT("NoExportTypes.h")))
					OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *HeaderPath);
			}
		}

		return true;
	}

	bool GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const override
	{
		auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
		if (UsedEnum == nullptr)
			return false;

		int32 EnumValue = 0;
		EnumValue = Usage.ResolvePrimitive<uint8>(Address);

		FString EnumName = UsedEnum->GetNameByValue(EnumValue).ToString();
		OutString = EnumName;

		return !EnumName.IsEmpty();
	}

	bool FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const override
	{
		auto* UsedEnum = Enum != nullptr ? Enum : (UEnum*)Usage.ScriptClass->GetUserData();
		if (UsedEnum == nullptr)
			return false;

		int EnumValue = UsedEnum->GetValueByName(*InString);
		if (EnumValue != -1)
		{
			*(uint8*)BufferPtr = EnumValue;
			return true;
		}

		LexFromString(EnumValue, *InString);
		*(uint8*)BufferPtr = EnumValue;
		return true;
	}
};

static const FName NAME_Enum_BlueprintType("BlueprintType");
static const FName NAME_Enum_NotBlueprintType("NotBlueprintType");
static const FName NAME_Enum_NotInAngelscript("NotInAngelscript");
// NOTE: Per-engine lookup of script-defined UEnum -> asITypeInfo* lives on
// `FAngelscriptEngine::ScriptEnumTypeLookupByName`. Access through
// `FAngelscriptEngine::Get().GetScriptEnumTypeLookup()`.
bool ShouldBindEngineType(UEnum* Enum)
{
	if (Enum == nullptr)
		return false;

	// Skip enums generated by Angelscript script compilation (UUserDefinedEnum
	// living in /Script/Angelscript). When a second engine instance is created
	// (e.g. in multi-engine tests), Bind_Enums would pre-register these as
	// native types, then the script compiler would try to declare the same enum
	// again, triggering an "extended data type" name conflict.
	if (Enum->IsA<UUserDefinedEnum>())
	{
		UPackage* Pkg = Enum->GetOutermost();
		if (Pkg != nullptr && Pkg->GetName() == TEXT("/Script/Angelscript"))
		{
			return false;
		}
	}

	const FString EnumName = Enum->GetName();
	if (EnumName == TEXT("EObjectTypeQuery") || EnumName == TEXT("EDateTimeStyle"))
		return false;

#if WITH_EDITOR
	if (Enum->GetBoolMetaData(NAME_Enum_NotBlueprintType))
		return false;
	if (Enum->GetBoolMetaData(NAME_Enum_NotInAngelscript))
		return false;

	// Apparently not all enums have blueprinttype even if they
	// should be usable by blueprint ??
	/*if (Enum->GetBoolMetaData(NAME_Enum_BlueprintType))
		return true;*/
#endif
	
	return true;
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Enums((int32)FAngelscriptBinds::EOrder::Early-1, []
{
	auto& BindDB = FAngelscriptBindDatabase::Get();
	FAngelscriptEngine::Get().GetScriptEnumTypeLookup().Reset();

#if WITH_DEV_AUTOMATION_TESTS
	// Phase 0 baseline probe: capture filtered candidate enums up-front so the
	// per-enum-loop segment can be measured cleanly without re-running the
	// TObjectRange + ShouldBindEngineType filter inside the timing scope.
	TArray<UEnum*> EligibleEnums;
	{
		FAngelscriptEnumTableBaselineSegmentScope SegmentScope(
			FAngelscriptEnumTableBaselineProbe::EBindEnumsSegment::TObjectRangeFilter);
		EligibleEnums.Reserve(2048);
		for (UEnum* Enum : TObjectRange<UEnum>())
		{
			if (ShouldBindEngineType(Enum))
			{
				EligibleEnums.Add(Enum);
			}
		}
	}

	auto ProcessOneEnum = [&BindDB](UEnum* Enum)
	{
		// PerEnumLoop: GetNameByIndex + ToString + RightChop + RegisterEnumValue (incl. O(N) dedupe)
		FAngelscriptBinds::FEnumBind EnumBind = FAngelscriptBinds::Enum(Enum->GetName());
		int32 EnumValueCount = 0;
		{
			FAngelscriptEnumTableBaselineSegmentScope SegmentScope(
				FAngelscriptEnumTableBaselineProbe::EBindEnumsSegment::PerEnumLoop);
			for (int32 Index = 0, Num = Enum->NumEnums(); Index < Num; ++Index)
			{
				FString Name = Enum->GetNameByIndex(Index).ToString();

				int32 ColonPos;
				if (Name.FindLastChar((TCHAR)':', ColonPos))
					Name = Name.RightChop(ColonPos+1);

				EnumBind[Name] = Enum->GetValueByIndex(Index);
				++EnumValueCount;
			}
		}
		FAngelscriptEnumTableBaselineProbe::RecordBindEnumsEnumProcessed(EnumValueCount);

		{
			FAngelscriptEnumTableBaselineSegmentScope SegmentScope(
				FAngelscriptEnumTableBaselineProbe::EBindEnumsSegment::TypeRegister);
			FAngelscriptType::Register(MakeShared<FEnumType>(Enum));
			BindDB.BoundEnums.Add(Enum);
		}

#if WITH_EDITOR
		const FString& Doc = Enum->GetMetaData(TEXT("ToolTip"));
		if (Doc.Len() != 0)
			FAngelscriptDocs::AddUnrealDocumentationForType(EnumBind.TypeId, Doc);
#endif

		{
			FAngelscriptEnumTableBaselineSegmentScope SegmentScope(
				FAngelscriptEnumTableBaselineProbe::EBindEnumsSegment::LookupRegister);
			if (auto* EnumScriptType = EnumBind.GetTypeInfo())
			{
				EnumScriptType->SetUserData(Enum);
				FAngelscriptEngine::Get().GetScriptEnumTypeLookup().Add(*Enum->GetName(), EnumScriptType);
			}
		}
	};

	for (UEnum* Enum : EligibleEnums)
	{
		ProcessOneEnum(Enum);
	}
#else
	// Production path: original implementation, untouched.
	for (UEnum* Enum : TObjectRange<UEnum>())
	{
		if (!ShouldBindEngineType(Enum))
			continue;

		auto EnumBind = FAngelscriptBinds::Enum(Enum->GetName());
		for (int32 Index = 0, Num = Enum->NumEnums(); Index < Num; ++Index)
		{
			FString Name = Enum->GetNameByIndex(Index).ToString();

			int32 ColonPos;
			if (Name.FindLastChar((TCHAR)':', ColonPos))
				Name = Name.RightChop(ColonPos+1);

			EnumBind[Name] = Enum->GetValueByIndex(Index);
		}

		FAngelscriptType::Register(MakeShared<FEnumType>(Enum));
		BindDB.BoundEnums.Add(Enum);

#if WITH_EDITOR
		const FString& Doc = Enum->GetMetaData(TEXT("ToolTip"));
		if (Doc.Len() != 0)
			FAngelscriptDocs::AddUnrealDocumentationForType(EnumBind.TypeId, Doc);
#endif

		if (auto* EnumScriptType = EnumBind.GetTypeInfo())
		{
			EnumScriptType->SetUserData(Enum);
			FAngelscriptEngine::Get().GetScriptEnumTypeLookup().Add(*Enum->GetName(), EnumScriptType);
		}
	}
#endif

	// Register a type finder into the type system that
	// can look up an EnumProperty's inner angelscript type.
	FAngelscriptType::RegisterTypeFinder([](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property);
		if (EnumProperty != nullptr)
		{
			UEnum* Enum = EnumProperty->GetEnum();
			//Usage.Type = FAngelscriptType::GetByData(EnumProperty->Enum);
			Usage.Type = FAngelscriptType::GetByData(Enum);
			if (!Usage.Type.IsValid())
			{
				//if (EnumProperty->Enum != nullptr && EnumProperty->Enum->GetOutermost() == FAngelscriptEngine::GetPackage())
				if (Enum != nullptr && Enum->GetOutermost() == FAngelscriptEngine::GetPackage())
				{
					// Type-finder lambdas can run outside the bind path (asset
					// scans, dumps, class-generator passes), so fall back to a
					// no-match when there is no current engine instead of
					// crashing through Get().
					if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
					{
						auto* ScriptEnum = CurrentEngine->GetScriptEnumTypeLookup().FindRef(*Enum->GetName());
						if (ScriptEnum != nullptr)
						{
							Usage.Type = FAngelscriptType::GetScriptEnum();
							Usage.ScriptClass = ScriptEnum;
							return true;
						}
					}
				}

				return false;
			}
			else if (EnumProperty->GetUnderlyingProperty()->IsA<FByteProperty>())
			{
				Usage.TypeIndex = 1;
				return true;
			}
			else if (EnumProperty->GetUnderlyingProperty()->IsA<FIntProperty>())
			{
				Usage.TypeIndex = 4;
				return true;
			}
			else
			{
				return false;
			}
		}

		FByteProperty* ByteProperty = CastField<FByteProperty>(Property);
		if (ByteProperty != nullptr && ByteProperty->Enum != nullptr)
		{
			Usage.Type = FAngelscriptType::GetByData(ByteProperty->Enum);
			if (!Usage.Type.IsValid())
			{
				if (ByteProperty->Enum != nullptr && ByteProperty->Enum->GetOutermost() == FAngelscriptEngine::GetPackage())
				{
					// Same defensive guard as the EnumProperty branch above:
					// no current engine means there is nothing to look up.
					if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
					{
						auto* ScriptEnum = CurrentEngine->GetScriptEnumTypeLookup().FindRef(*ByteProperty->Enum->GetName());
						if (ScriptEnum != nullptr)
						{
							Usage.Type = FAngelscriptType::GetScriptEnum();
							Usage.ScriptClass = ScriptEnum;
							return true;
						}
					}
				}

				return false;
			}
			else
			{
				Usage.TypeIndex = 1;
				return true;
			}
		}
		return false;
	});

	// Register a type that handles script enums generically
	auto ScriptEnumType = MakeShared<FEnumType>(nullptr);
	FAngelscriptType::SetScriptEnum(ScriptEnumType);
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_EGetByNameFlags(FAngelscriptBinds::EOrder::Early, []
{
	auto EGetByNameFlags_ = FAngelscriptBinds::Enum("EGetByNameFlags");
	EGetByNameFlags_["None"] = EGetByNameFlags::None;
	EGetByNameFlags_["ErrorIfNotFound"] = EGetByNameFlags::ErrorIfNotFound;
	EGetByNameFlags_["CaseSensitive"] = EGetByNameFlags::CaseSensitive;
	EGetByNameFlags_["CheckAuthoredName"] = EGetByNameFlags::CheckAuthoredName;
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_UEnum((int32)FAngelscriptBinds::EOrder::Late, []
{
	auto UEnum_ = FAngelscriptBinds::ExistingClass("UEnum");
	UEnum_.Method("FName GetNameByIndex(int32 InIndex) const", METHOD_TRIVIAL(UEnum, GetNameByIndex));
	UEnum_.Method("int32 GetIndexByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const", METHOD_TRIVIAL(UEnum, GetIndexByName));
	UEnum_.Method("FName GetNameByValue(int64 InValue) const", METHOD_TRIVIAL(UEnum, GetNameByValue));
	UEnum_.Method("int64 GetValueByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const", METHOD_TRIVIAL(UEnum, GetValueByName));
	UEnum_.Method("FString GetNameStringByIndex(int32 InIndex) const", METHOD_TRIVIAL(UEnum, GetNameStringByIndex));
	UEnum_.Method("int32 GetIndexByNameString(const FString& SearchString, EGetByNameFlags Flags = EGetByNameFlags::None) const", METHOD_TRIVIAL(UEnum, GetIndexByNameString));
	UEnum_.Method("FString GetNameStringByValue(int64 InValue) const", METHOD_TRIVIAL(UEnum, GetNameStringByValue));
	UEnum_.Method("int64 GetValueByNameString(const FString& SearchString, EGetByNameFlags Flags = EGetByNameFlags::None) const", METHOD_TRIVIAL(UEnum, GetValueByNameString));
	UEnum_.Method("FText GetDisplayNameTextByIndex(int32 InIndex) const", METHOD_TRIVIAL(UEnum, GetDisplayNameTextByIndex));
	UEnum_.Method("FText GetDisplayNameTextByValue(int64 InValue) const", METHOD_TRIVIAL(UEnum, GetDisplayNameTextByValue));

	UEnum_.Method("int64 GetMaxEnumValue() const", METHOD_TRIVIAL(UEnum, GetMaxEnumValue));
	UEnum_.Method("int32 NumEnums() const", METHOD_TRIVIAL(UEnum, NumEnums));

	UEnum_.Method("bool IsValidEnumValue(int64 InValue) const", METHOD_TRIVIAL(UEnum, IsValidEnumValue));
	UEnum_.Method("bool IsValidEnumName(FName InName) const", METHOD_TRIVIAL(UEnum, IsValidEnumName));

	UEnum_.Method("bool ContainsExistingMax() const", METHOD_TRIVIAL(UEnum, ContainsExistingMax));
	UEnum_.Method("FString GenerateEnumPrefix() const", METHOD_TRIVIAL(UEnum, GenerateEnumPrefix));
});
