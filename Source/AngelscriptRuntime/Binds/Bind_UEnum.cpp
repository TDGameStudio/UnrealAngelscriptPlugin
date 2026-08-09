#include "Bind_UEnum.h"

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

/**
 * Reflected enum type expansion and UEnum manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | enum <EligibleUEnum> { <reflected enumerators> };                                          | Expands once per loaded UEnum accepted by ShouldBindEngineType. Enumerator names have native qualification removed.  |
 * |                                                                                            | Script-generated /Script/Angelscript enums, explicit exclusions, and opt-out metadata are not expanded.              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | enum EGetByNameFlags;                                                                      | Declares flags controlling reflected enum name lookup.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EGetByNameFlags::None;                                                                     | Uses default reflected enum lookup behavior.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EGetByNameFlags::ErrorIfNotFound;                                                          | Requests an error when a reflected enum name is not found.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EGetByNameFlags::CaseSensitive;                                                            | Makes reflected enum name lookup case-sensitive.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EGetByNameFlags::CheckAuthoredName;                                                        | Also checks the authored enum entry name.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName UEnum.GetNameByIndex(int32 InIndex) const;                                           | Returns the qualified name at an enum index.                                                                         |
 * |                                                                                            | @param InIndex Reflected enumerator index.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 UEnum.GetIndexByName(FName InName,                                                   | Returns the index matching a reflected enum name under Flags.                                                        |
 * |     EGetByNameFlags Flags = EGetByNameFlags::None) const;                                  |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName UEnum.GetNameByValue(int64 InValue) const;                                           | Returns the qualified name matching an enum value.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int64 UEnum.GetValueByName(FName InName,                                                   | Returns the value matching a reflected enum name under Flags.                                                        |
 * |     EGetByNameFlags Flags = EGetByNameFlags::None) const;                                  |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UEnum.GetNameStringByIndex(int32 InIndex) const;                                   | Returns the unqualified name string at an enum index.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 UEnum.GetIndexByNameString(const FString& SearchString,                              | Returns the index matching an enum name string under Flags.                                                          |
 * |     EGetByNameFlags Flags = EGetByNameFlags::None) const;                                  |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UEnum.GetNameStringByValue(int64 InValue) const;                                   | Returns the unqualified name string matching an enum value.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int64 UEnum.GetValueByNameString(const FString& SearchString,                              | Returns the value matching an enum name string under Flags.                                                          |
 * |     EGetByNameFlags Flags = EGetByNameFlags::None) const;                                  |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FText UEnum.GetDisplayNameTextByIndex(int32 InIndex) const;                                | Returns localized display text at an enum index.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FText UEnum.GetDisplayNameTextByValue(int64 InValue) const;                                | Returns localized display text matching an enum value.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int64 UEnum.GetMaxEnumValue() const;                                                       | Returns the greatest reflected enumerator value.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 UEnum.NumEnums() const;                                                              | Returns the number of reflected enumerator entries.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnum.IsValidEnumValue(int64 InValue) const;                                          | Returns whether InValue is a declared enumerator value.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnum.IsValidEnumName(FName InName) const;                                            | Returns whether InName is a declared enumerator name.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UEnum.ContainsExistingMax() const;                                                    | Returns whether the enum already declares a conventional maximum entry.                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString UEnum.GenerateEnumPrefix() const;                                                  | Generates the common reflected enumerator-name prefix.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

static const FName NAME_Enum_BlueprintType("BlueprintType");
static const FName NAME_Enum_NotBlueprintType("NotBlueprintType");
static const FName NAME_Enum_NotInAngelscript("NotInAngelscript");
// NOTE: Per-engine lookup of script-defined UEnum -> asITypeInfo* lives on
// `FAngelscriptEngine::ScriptEnumTypeLookupByName` and is captured explicitly
// by the owning engine's type finder below.
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




AS_FORCE_LINK const FAngelscriptBind Bind_Enums(
	TEXT("Enums"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		auto& BindDB = Binds.GetTargetBindDatabase();
		FAngelscriptEngine& TargetEngine = Binds.GetTargetEngine();
		Binds.GetTargetEngine().GetScriptEnumTypeLookup().Reset();

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

		auto ProcessOneEnum = [&Binds, &BindDB, &TargetEngine](UEnum* Enum)
		{
			// PerEnumLoop: GetNameByIndex + ToString + RightChop + RegisterEnumValue (incl. O(N) dedupe)
			FAngelscriptBinds::FEnumBind EnumBind = Binds.EnumForTarget(Enum->GetName());
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
				Binds.RegisterTypeForTarget(MakeShared<FEnumType>(Enum, Binds.GetTargetBindDatabase()));
				BindDB.BoundEnums.Add(Enum);
			}

	#if WITH_EDITOR
			const FString& Doc = Enum->GetMetaData(TEXT("ToolTip"));
			if (Doc.Len() != 0)
				FAngelscriptDocs::AddUnrealDocumentationForType(Binds.GetTargetEngine(), EnumBind.TypeId, Doc);
	#endif

			{
				FAngelscriptEnumTableBaselineSegmentScope SegmentScope(
					FAngelscriptEnumTableBaselineProbe::EBindEnumsSegment::LookupRegister);
				if (auto* EnumScriptType = EnumBind.GetTypeInfo())
				{
					EnumScriptType->SetUserData(Enum);
					TargetEngine.GetScriptEnumTypeLookup().Add(*Enum->GetName(), EnumScriptType);
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

			auto EnumBind = Binds.EnumForTarget(Enum->GetName());
			for (int32 Index = 0, Num = Enum->NumEnums(); Index < Num; ++Index)
			{
				FString Name = Enum->GetNameByIndex(Index).ToString();

				int32 ColonPos;
				if (Name.FindLastChar((TCHAR)':', ColonPos))
					Name = Name.RightChop(ColonPos+1);

				EnumBind[Name] = Enum->GetValueByIndex(Index);
			}

			Binds.RegisterTypeForTarget(MakeShared<FEnumType>(Enum, Binds.GetTargetBindDatabase()));
			BindDB.BoundEnums.Add(Enum);

	#if WITH_EDITOR
			const FString& Doc = Enum->GetMetaData(TEXT("ToolTip"));
			if (Doc.Len() != 0)
				FAngelscriptDocs::AddUnrealDocumentationForType(Binds.GetTargetEngine(), EnumBind.TypeId, Doc);
	#endif

			if (auto* EnumScriptType = EnumBind.GetTypeInfo())
			{
				EnumScriptType->SetUserData(Enum);
				TargetEngine.GetScriptEnumTypeLookup().Add(*Enum->GetName(), EnumScriptType);
			}
		}
	#endif

		// Register a type finder into the type system that
		// can look up an EnumProperty's inner angelscript type.
		FAngelscriptTypeDatabase* TargetTypeDatabase = &Binds.GetTargetTypeDatabase();
		Binds.RegisterTypeFinderForTarget([TargetEngine = &TargetEngine, TargetTypeDatabase](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property);
			if (EnumProperty != nullptr)
			{
				UEnum* Enum = EnumProperty->GetEnum();
				const TSharedRef<FAngelscriptType>* RegisteredType = TargetTypeDatabase->TypesByData.Find(Enum);
				Usage.Type = RegisteredType != nullptr ? RegisteredType->ToSharedPtr() : nullptr;
				if (!Usage.Type.IsValid())
				{
					if (Enum != nullptr && Enum->GetOutermost() == FAngelscriptEngine::GetPackage())
					{
						if (TargetEngine != nullptr)
						{
							auto* ScriptEnum = TargetEngine->GetScriptEnumTypeLookup().FindRef(*Enum->GetName());
							if (ScriptEnum != nullptr)
							{
								Usage.Type = TargetTypeDatabase->ScriptEnumType;
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
				const TSharedRef<FAngelscriptType>* RegisteredType = TargetTypeDatabase->TypesByData.Find(ByteProperty->Enum);
				Usage.Type = RegisteredType != nullptr ? RegisteredType->ToSharedPtr() : nullptr;
				if (!Usage.Type.IsValid())
				{
					if (ByteProperty->Enum != nullptr && ByteProperty->Enum->GetOutermost() == FAngelscriptEngine::GetPackage())
					{
						if (TargetEngine != nullptr)
						{
							auto* ScriptEnum = TargetEngine->GetScriptEnumTypeLookup().FindRef(*ByteProperty->Enum->GetName());
							if (ScriptEnum != nullptr)
							{
								Usage.Type = TargetTypeDatabase->ScriptEnumType;
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
		auto ScriptEnumType = MakeShared<FEnumType>(nullptr, Binds.GetTargetBindDatabase());
		Binds.GetTargetTypeDatabase().ScriptEnumType = ScriptEnumType;
	});

AS_FORCE_LINK const FAngelscriptBind Bind_EGetByNameFlags(
	TEXT("EGetByNameFlags"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		auto EGetByNameFlags_ = Binds.EnumForTarget("EGetByNameFlags");
		EGetByNameFlags_["None"] = EGetByNameFlags::None;
		EGetByNameFlags_["ErrorIfNotFound"] = EGetByNameFlags::ErrorIfNotFound;
		EGetByNameFlags_["CaseSensitive"] = EGetByNameFlags::CaseSensitive;
		EGetByNameFlags_["CheckAuthoredName"] = EGetByNameFlags::CheckAuthoredName;
	});

AS_FORCE_LINK const FAngelscriptBind Bind_UEnum(
	TEXT("UEnum"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto UEnum_ = Binds.ExistingClassForTarget("UEnum");
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
