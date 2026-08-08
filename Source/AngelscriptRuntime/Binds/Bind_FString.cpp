#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Containers/UnrealString.h"
#include "Engine/UserDefinedEnum.h"

#include "Helper_CppType.h"
#include "Helper_GetTypeInfo.h"
#include "Helper_ToString.h"
#include "Bind_FString_Functions.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "ClassGenerator/ASClass.h"
#include "EndAngelscriptHeaders.h"

struct FStringType : TAngelscriptCppPropertyType<FStrProperty>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FString");
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		OutValue = FString::Printf(TEXT("\"%s\""), *InValue);
		return true;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		OutValue = InValue.TrimQuotes();
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		FString& NativeValue = Usage.ResolvePrimitive<FString>(Address);

		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.Value = TEXT("\"") + NativeValue + TEXT("\"");

		return true;
	}

	bool GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const override
	{
		OutString = *(FString*)Address;
		return true;
	}

	bool FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const
	{
		new(BufferPtr) FString(InString);
		return true;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}

	virtual bool IsOrdered(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	virtual int32 CompareOrder(const FAngelscriptTypeUsage& Usage, void* Value, void* OtherValue) const override
	{
		FString& A = Usage.ResolvePrimitive<FString>(Value);
		FString& B = Usage.ResolvePrimitive<FString>(OtherValue);
		return A.Compare(B);
	}
};

template <typename InternalType, typename ExternalType>
FORCEINLINE static bool AddPrimitiveFormatOrderedArgument(FStringFormatOrderedArguments& OutFormatOrderedArguments, const void* Ptr)
{
	const InternalType Value = *reinterpret_cast<const ExternalType*>(Ptr);
	OutFormatOrderedArguments.Emplace(FStringFormatArg(Value));
	return true;
}

static bool AddFormatOrderedArgument(FStringFormatOrderedArguments& OutFormatOrderedArguments, const void* Ptr, int TypeId)
{
	// primitive types
	switch (TypeId & asTYPEID_MASK_SEQNBR)
	{
	case asTYPEID_INT8:		return AddPrimitiveFormatOrderedArgument<int32, int8>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_INT16:	return AddPrimitiveFormatOrderedArgument<int32, int16>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_INT32:	return AddPrimitiveFormatOrderedArgument<int32, int32>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_INT64:	return AddPrimitiveFormatOrderedArgument<int64, int64>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_UINT8:	return AddPrimitiveFormatOrderedArgument<uint32, uint8>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_UINT16:	return AddPrimitiveFormatOrderedArgument<uint32, uint16>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_UINT32:	return AddPrimitiveFormatOrderedArgument<uint32, uint32>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_UINT64:	return AddPrimitiveFormatOrderedArgument<uint64, uint64>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_FLOAT32:	return AddPrimitiveFormatOrderedArgument<double, float>(OutFormatOrderedArguments, Ptr);
	case asTYPEID_FLOAT64:	return AddPrimitiveFormatOrderedArgument<double, double>(OutFormatOrderedArguments, Ptr);
	}

	// custom types
	asITypeInfo* TypeInfo = FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId);
	if (ensure(TypeInfo != nullptr))
	{
		// enum
		if ((TypeInfo->GetFlags() & asOBJ_ENUM) != 0)
		{
			const uint32 Value = *reinterpret_cast<const uint8*>(Ptr);
			OutFormatOrderedArguments.Emplace(FStringFormatArg(Value));
			return true;
		}

		// fstring
		if (TGetStaticTypeInfo<FString>::IsForEngine(FAngelscriptEngine::Get().GetScriptEngine(), TypeInfo))
		{
			const FString& Value = *reinterpret_cast<const FString*>(Ptr);
			OutFormatOrderedArguments.Emplace(FStringFormatArg(Value));
			return true;
		}

		const FString Message = FString::Printf(TEXT("Invalid argument type passed to FText::Format: %s"), ANSI_TO_TCHAR(TypeInfo->GetName()));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return false;
	}

	FAngelscriptEngine::Throw("Invalid argument type passed to FText::Format");
	return false;
}

void FAngelscriptFStringBinds::Format(asIScriptGeneric* Generic)
{
	const FString& Format = *reinterpret_cast<FString*>(Generic->GetArgAddress(0));

	bool bSuccess = true;
	FStringFormatOrderedArguments FormatOrderedArguments;
	for (int i = 1; bSuccess && i < Generic->GetArgCount(); ++i)
	{
		bSuccess &= AddFormatOrderedArgument(FormatOrderedArguments, Generic->GetArgAddress(i), Generic->GetArgTypeId(i));
	}

	const FString OutString = bSuccess ? FString::Format(*Format, FormatOrderedArguments) : FString();
	new (Generic->GetAddressOfReturnLocation()) FString(OutString);
}

class FAngelscriptFStringFactory final : public asIStringFactory
{
public:
	const void* GetStringConstant(const char* Data, asUINT Length) override
	{
		FUTF8ToTCHAR Convertor(Data, Length);
		auto* String = new FString();
		String->AppendChars(Convertor.Get(), Convertor.Length());
		return String;
	}

	int ReleaseStringConstant(const void* String) override
	{
		delete static_cast<const FString*>(String);
		return 0;
	}

	int GetRawStringData(const void* String, char* Data, asUINT* Length) const override
	{
		const FString* UnrealString = static_cast<const FString*>(String);
		if (UnrealString->Len() == 0)
		{
			if (Length != nullptr)
			{
				*Length = 0;
			}
			return 0;
		}

		FTCHARToUTF8 Convertor(&(*UnrealString)[0], UnrealString->Len());
		if (Length != nullptr)
		{
			*Length = Convertor.Length();
		}
		if (Data != nullptr)
		{
			FMemory::Memcpy(Data, Convertor.Get(), Convertor.Length());
		}

		return 0;
	}
};

static void BindFStringTypeDeclarations(FAngelscriptBinds& Binds)
{
	Binds.ValueClassForTarget<FString>("FString", FBindFlags());
}

static void BindFStringInfrastructure(FAngelscriptBinds& Binds)
{
	auto FString_ = Binds.ExistingClassForTarget("FString");
	Binds.RegisterTypeForTarget(MakeShared<FStringType>());
	TGetStaticTypeInfo<FString>::SetForEngine(&Binds.GetTargetScriptEngine(), FString_.GetTypeInfo());
	Binds.GetTargetScriptEngine().RegisterStringFactory("FString", new FAngelscriptFStringFactory());
}

static void BindFStringCore(FAngelscriptBinds& Binds)
{
	auto FString_ = Binds.ExistingClassForTarget("FString");

	FString_.Constructor("void f()", &FAngelscriptFStringBinds::ConstructDefault, "FString", true).NoDiscard();

	FString_.Constructor("void f(const FString& Other)", &FAngelscriptFStringBinds::ConstructCopy, "FString", true)
		.NoDiscard();

	FString_.Destructor("void f()", &FAngelscriptFStringBinds::Destruct, "FString", true).NoDiscard();

	// Bind operator overloads
	FString_.Method("FString& opAssign(const FString& Other)", METHODPR_TRIVIAL(FString&, FString, operator=, (const FString&)));
	FString_.Method("FString& opAddAssign(const FString& Other)", METHODPR_TRIVIAL(FString&, FString, operator+=, (const FString&)));
	FString_.Method("bool opEquals(const FString& Other) const", &FAngelscriptFStringBinds::Equals);
	FString_.Method("int opCmp(const FString& Other) const", &FAngelscriptFStringBinds::Compare);
	FString_.Method("FString opAdd(const FString& Other) const", &FAngelscriptFStringBinds::Add);
	FString_.Method("int16& opIndex(int32 Index)", &FAngelscriptFStringBinds::Index);
	FString_.Method("const int16& opIndex(int32 Index) const", &FAngelscriptFStringBinds::IndexConst);

	FString_.Method("FString& Append(const FString& Other) accept_temporary_this", METHODPR_TRIVIAL(FString&, FString, Append, (const FString&)));
	FString_.Method("FString& AppendChar(int16 Character) accept_temporary_this", METHODPR_TRIVIAL(FString&, FString, AppendChar, (TCHAR)));
	FString_.Method("void AppendInt(int32 InNum)", METHOD_TRIVIAL(FString, AppendInt));
	FString_.Method("void InsertAt(int32 Index, int16 Character)", METHODPR_TRIVIAL(void, FString, InsertAt, (int32, TCHAR)));
	FString_.Method("void InsertAt(int32 Index, const FString& Characters)", METHODPR_TRIVIAL(void, FString, InsertAt, (int32, const FString&)));

	// Manipulation as array
	FString_.Method("void Empty()", METHODPR_TRIVIAL(void, FString, Empty, ()));
	FString_.Method("void Empty(int Slack)", METHODPR_TRIVIAL(void, FString, Empty, (int32)));
	FString_.Method("bool IsEmpty() const", METHOD_TRIVIAL(FString, IsEmpty));
	FString_.Method("void Reset(int NewReservedSize = 0)", METHOD_TRIVIAL(FString, Reset));
	FString_.Method("void Reserve(int Count)", METHOD_TRIVIAL(FString, Reserve));
	FString_.Method("void Shrink()", METHOD_TRIVIAL(FString, Shrink));
	FString_.Method("bool IsValidIndex(int Index) const", METHOD_TRIVIAL(FString, IsValidIndex));
	FString_.Method("void RemoveAt(int Index, int Count)", &FAngelscriptFStringBinds::RemoveAt);
	FString_.Method("void RemoveSpacesInline()", METHOD_TRIVIAL(FString, RemoveSpacesInline));

	// Handling as string
	FString_.Method("int Len() const", METHOD_TRIVIAL(FString, Len));
	FString_.Method("bool IsNumeric() const", METHOD_TRIVIAL(FString, IsNumeric));
	FString_.Method("FString Reverse() const", METHOD_TRIVIAL(FString, Reverse));

	FString_.Method("FString ConvertTabsToSpaces(int32 InSpacesPerTab) const", &FAngelscriptFStringBinds::ConvertTabsToSpaces);

	// Substring handling
	FString_.Method("bool RemoveFromStart(const FString& Prefix, ESearchCase SearchCase = ESearchCase::IgnoreCase)", METHODPR_TRIVIAL(bool, FString, RemoveFromStart, (const FString&,ESearchCase::Type)));
	FString_.Method("bool RemoveFromEnd(const FString& Suffix, ESearchCase SearchCase = ESearchCase::IgnoreCase)", METHODPR_TRIVIAL(bool, FString, RemoveFromEnd, (const FString&,ESearchCase::Type)));

	FString_.Method("FString Left(int Count) const", METHODPR_TRIVIAL(FString, FString, Left, (int32) const &));
	FString_.Method("FString LeftChop(int Count) const", METHODPR_TRIVIAL(FString, FString, LeftChop, (int32) const &));
	FString_.Method("FString Right(int Count) const", METHODPR_TRIVIAL(FString, FString, Right, (int32) const &));
	FString_.Method("FString RightChop(int Count) const", METHODPR_TRIVIAL(FString, FString, RightChop, (int32) const &));
	FString_.Method("FString Mid(int Start, int Count = MAX_int32) const", METHODPR_TRIVIAL(FString, FString, Mid, (int32, int32) const &));

	FString_.Method("bool Split(const FString& Needle, FString& OutLeft, FString& OutRight, "
		"ESearchCase SearchCase = ESearchCase::IgnoreCase, ESearchDir SearchDir = ESearchDir::FromStart) const",
		&FAngelscriptFStringBinds::Split);

	FString_.Method("FString Replace(const FString& From, const FString& To, ESearchCase SearchCase = ESearchCase::IgnoreCase) const",
		&FAngelscriptFStringBinds::Replace);

	FString_.Method("int ReplaceInline(const FString& SearchText, const FString& ReplacementText, ESearchCase SearchCase = ESearchCase::IgnoreCase)",
		&FAngelscriptFStringBinds::ReplaceInline);

	FString_.Method("FString ReplaceCharWithEscapedChar() const", &FAngelscriptFStringBinds::ReplaceCharWithEscapedChar);

	FString_.Method("FString ReplaceEscapedCharWithChar() const", &FAngelscriptFStringBinds::ReplaceEscapedCharWithChar);

	// Substring finding
	FString_.Method(
		"int Find(const FString& SubStr, ESearchCase SearchCase = ESearchCase::IgnoreCase, "
		"ESearchDir SearchDir = ESearchDir::FromStart, int StartPosition=-1) const",
		METHODPR_TRIVIAL(int32, FString, Find, (const FString&, ESearchCase::Type, ESearchDir::Type, int32)const));

	FString_.Method(
		"bool Contains(const FString& SubStr, ESearchCase SearchCase = ESearchCase::IgnoreCase, "
		"ESearchDir SearchDir = ESearchDir::FromStart) const",
		METHODPR_TRIVIAL(bool, FString, Contains, (const FString&, ESearchCase::Type, ESearchDir::Type)const));

	FString_.Method("bool FindChar(int16 Char, int& Index) const", METHOD_TRIVIAL(FString, FindChar));
	FString_.Method("bool FindLastChar(int16 Char, int& Index) const", METHOD_TRIVIAL(FString, FindLastChar));

	FString_.Method("bool StartsWith(const FString& SubStr, ESearchCase SearchCase = ESearchCase::IgnoreCase) const",
		METHODPR_TRIVIAL(bool, FString, StartsWith, (const FString&, ESearchCase::Type)const));

	FString_.Method("bool EndsWith(const FString& SubStr, ESearchCase SearchCase = ESearchCase::IgnoreCase) const",
		METHODPR_TRIVIAL(bool, FString, EndsWith, (const FString&, ESearchCase::Type)const));

	FString_.Method("bool MatchesWildcard(const FString& Wildcard, ESearchCase SearchCase = ESearchCase::IgnoreCase) const",
		METHODPR_TRIVIAL(bool, FString, MatchesWildcard, (const FString&, ESearchCase::Type)const));

	FString_.Method("bool Equals(const FString& Other, ESearchCase SearchCase = ESearchCase::CaseSensitive) const",
		METHODPR_TRIVIAL(bool, FString, Equals, (const FString&, ESearchCase::Type)const));

	// Case handling
	FString_.Method("FString ToUpper() const", &FAngelscriptFStringBinds::ToUpper);
	FString_.Method("FString ToLower() const", &FAngelscriptFStringBinds::ToLower);

	// Whitespace handling
	FString_.Method("FString LeftPad(int Count) const", METHOD_TRIVIAL(FString, LeftPad));
	FString_.Method("FString RightPad(int Count) const", METHOD_TRIVIAL(FString, RightPad));
	FString_.Method("FString TrimQuotes(bool& OutQuotesRemoved) const", &FAngelscriptFStringBinds::TrimQuotes);
	FString_.Method("FString TrimStartAndEnd() const", &FAngelscriptFStringBinds::TrimStartAndEnd);
	FString_.Method("FString TrimStart() const", &FAngelscriptFStringBinds::TrimStart);
	FString_.Method("FString TrimEnd() const", &FAngelscriptFStringBinds::TrimEnd);
	FString_.Method("FString TrimChar(int16 CharacterToTrim) const", &FAngelscriptFStringBinds::TrimChar);

	FString_.Method("int32 Compare(const FString& Other, ESearchCase SearchCase = ESearchCase::CaseSensitive) const", METHODPR_TRIVIAL(int32, FString, Compare, (const FString&, ESearchCase::Type) const));

	// Conversion
	FString_.Method("bool ToBool() const", METHOD_TRIVIAL(FString, ToBool));
	FString_.Method("FString ToDisplayName(bool bIsBool = false) const", &FAngelscriptFStringBinds::ToDisplayName);
	FString_.Method("uint GetHash() const", &FAngelscriptFStringBinds::GetHash);

}

void FToStringHelper::Register(FAngelscriptBinds& Binds, const FString& TypeName, FToStringHelper::FToStringFunction ToString, bool bImplicitConversion, bool bIsHandleType)
{
	Binds.GetTargetToStringList().Add({TypeName, nullptr, ToString, bImplicitConversion, bIsHandleType});
}

void FToStringHelper::Generic_AppendToString(FString& AppendTo, void* ValuePtr, int TypeId)
{
	FAngelscriptEngine& RuntimeEngine = FAngelscriptEngine::Get();
	FAngelscriptBinds RuntimeBinds(RuntimeEngine);
	asITypeInfo* TypeInfo = RuntimeEngine.Engine->GetTypeInfoById(TypeId);

	// If it's a UObject, print its name
	if (TypeInfo != nullptr && (TypeInfo->GetFlags() & asOBJ_REF) != 0)
	{
		UObject* Object = *(UObject**)ValuePtr;
		if (Object == nullptr)
		{
			AppendTo += TEXT("nullptr");
		}
		else
		{
			UClass* ObjClass = Object->GetClass();
			UASClass* asClass = Cast<UASClass>(ObjClass);

			if (asClass == nullptr) return;

			FString Suffix;
			auto& Delegate = RuntimeEngine.GetDebugObjectSuffix();
			if (Delegate.IsBound())
			{
				Delegate.Execute(Object, Suffix);
			}

#if WITH_EDITOR
			if (AActor* Actor = Cast<AActor>(Object))
			{
				

				AppendTo += FString::Printf(TEXT("{ %s %s(%s%s) (ID: %s) }"),
					*Actor->GetActorLabel(),
					*Suffix,
					//(ObjClass->HasAnyClassFlags(CLASS_Native) || ObjClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					(ObjClass->HasAnyClassFlags(CLASS_Native) || asClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					*ObjClass->GetName(),
					*Object->GetName());
			}
			else
#endif
			{

				AppendTo += FString::Printf(TEXT("{ %s %s(%s%s) }"),
					*Object->GetName(),
					*Suffix,
					//(ObjClass->HasAnyClassFlags(CLASS_Native) || ObjClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					(ObjClass->HasAnyClassFlags(CLASS_Native) || asClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					*ObjClass->GetName());
			}
		}
		return;
	}

	// If it's an enum, print it like that
	if (TypeInfo != nullptr && (TypeInfo->GetFlags() & asOBJ_ENUM) != 0)
	{
		UUserDefinedEnum* UnrealEnum = (UUserDefinedEnum*)TypeInfo->GetUserData();
		if (UnrealEnum != nullptr)
		{
			FString UnrealValueName = UnrealEnum->GetNameStringByValue(*(uint8*)ValuePtr);
			AppendTo += FString::Printf(
				TEXT("%s::%s (%d)"),
				ANSI_TO_TCHAR(TypeInfo->GetName()),
				*UnrealValueName,
				*(uint8*)ValuePtr
			);
			return;
		}

		asUINT EnumCount = TypeInfo->GetEnumValueCount();
		if (EnumCount == 0)
		{
			AppendTo += FString::Printf(
				TEXT("%s::%d"),
				ANSI_TO_TCHAR(TypeInfo->GetName()),
				*(uint8*)ValuePtr
			);
			return;
		}

		for(asUINT i = 0; i < EnumCount; ++i)
		{
			int EnumValue;
			const char* ValueName = TypeInfo->GetEnumValueByIndex(i, &EnumValue);
			if (EnumValue == *(uint8*)ValuePtr && ValueName != nullptr)
			{
				AppendTo += FString::Printf(
					TEXT("%s::%s (%d)"),
					ANSI_TO_TCHAR(TypeInfo->GetName()),
					ANSI_TO_TCHAR(ValueName),
					EnumValue
				);
				return;
			}
		}

		if (EnumCount != 0)
		{
			AppendTo += FString::Printf(
				TEXT("Invalid %s (%d)"),
				ANSI_TO_TCHAR(TypeInfo->GetName()),
				*(uint8*)ValuePtr
			);
			return;
		}
	}

	// See if we have any ToString helper functions
	if (TypeInfo != nullptr && (TypeInfo->GetFlags() & asOBJ_VALUE))
	{
		for (const FToStringType& ToString : RuntimeBinds.GetTargetToStringList())
		{
			if (ToString.TypeInfo == TypeInfo)
			{
				ToString.ToString(ValuePtr, AppendTo);
				return;
			}
		}
	}

	// Delegates show their binds when append to a string
	if (TypeInfo != nullptr && (TypeInfo->GetFlags() & asOBJ_SCRIPT_OBJECT) && (TypeInfo->GetFlags() & asOBJ_VALUE))
	{
		void* UserData = TypeInfo->GetUserData();
		if (UserData != nullptr
			&& UserData != FAngelscriptType::TAG_UserData_Delegate
			&& UserData != FAngelscriptType::TAG_UserData_Multicast_Delegate)
		{
			UObject* UserObj = (UObject*)UserData;
			UDelegateFunction* DelegateSignature = Cast<UDelegateFunction>(UserObj);
			if (DelegateSignature != nullptr)
			{
				if (DelegateSignature->HasAnyFunctionFlags(FUNC_MulticastDelegate))
				{
					FMulticastScriptDelegate& Delegate = *(FMulticastScriptDelegate*)ValuePtr;
					if (Delegate.IsBound())
						AppendTo += Delegate.ToString<UObject>();
					else
						AppendTo += TEXT("Unbound");
					return;
				}
				else
				{
					FScriptDelegate& Delegate = *(FScriptDelegate*)ValuePtr;
					if (Delegate.IsBound())
					{
						UObject* Object = Delegate.GetUObject();
						FName FunctionName = Delegate.GetFunctionName();

						AppendTo += FString::Printf(TEXT("%s.%s"),
							*GetNameSafe(Object),
							*FunctionName.ToString());
					}
					else
					{
						AppendTo += TEXT("Unbound");
					}
					return;
				}
			}
		}
	}


	FAngelscriptEngine::Throw("Invalid type to append to string.");
};


// Formats a value along a python-style format specifier
struct FFormatSpecifier
{
	enum class EAlign : uint8
	{
		None,
		Left,
		Right,
		Middle,
		AfterSign,
	};

	EAlign Align = EAlign::None;

	enum class ESign : uint8
	{
		Both,
		Negative,
		LeadingSpace,
	};

	ESign Sign = ESign::Negative;

	bool bPrefixBase = false;
	bool bCommas = false;
	FString MinimumWidth;
	FString Precision;

	TCHAR Fill = ' ';
	TCHAR Type = ' ';

	enum EState
	{
		OnStart,
		OnSign,
		OnAlternateForm,
		OnMinimumWidth,
		OnPrecision,
		OnType
	};

	FFormatSpecifier(const FString& Specifier)
	{
		EState State = EState::OnStart;

		for (int32 Pos = 0, Length = Specifier.Len(); Pos < Length; ++Pos)
		{
			int16 Char = Specifier[Pos];
			switch (Char)
			{
				case '<':
					Align = EAlign::Left;
					State = EState::OnSign;
					Type = ' ';
					MinimumWidth.Reset();
					if (Pos > 0)
						Fill = Specifier[Pos - 1];
				break;
				case '>':
					Align = EAlign::Right;
					State = EState::OnSign;
					Type = ' ';
					MinimumWidth.Reset();
					if (Pos > 0)
						Fill = Specifier[Pos - 1];
				break;
				case '^':
					Align = EAlign::Middle;
					State = EState::OnSign;
					MinimumWidth.Reset();
					Type = ' ';
					if (Pos > 0)
						Fill = Specifier[Pos - 1];
				break;
				case '=':
					Align = EAlign::AfterSign;
					State = EState::OnSign;
					MinimumWidth.Reset();
					Type = ' ';
					if (Pos > 0)
						Fill = Specifier[Pos - 1];
				break;
				case '+':
					if (State <= EState::OnSign)
					{
						Sign = ESign::Both;
						State = EState::OnAlternateForm;
					}
				break;
				case '-':
					if (State <= EState::OnSign)
					{
						Sign = ESign::Negative;
						State = EState::OnAlternateForm;
					}
				break;
				case ' ':
					if (State <= EState::OnSign)
					{
						Sign = ESign::LeadingSpace;
						State = EState::OnAlternateForm;
					}
				break;
				case '#':
					if (State <= EState::OnAlternateForm)
					{
						bPrefixBase = true;
						State = EState::OnMinimumWidth;
					}
				break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					if (State <= EState::OnMinimumWidth)
					{
						if (MinimumWidth.Len() == 0 && Char == '0')
						{
							Align = EAlign::AfterSign;
							Fill = '0';
						}

						State = EState::OnMinimumWidth;
						MinimumWidth.AppendChar(Char);
					}
					else if (State <= EState::OnPrecision)
					{
						State = EState::OnPrecision;
						Precision.AppendChar(Char);
					}
				break;
				case '.':
					if (State <= EState::OnPrecision)
					{
						State = EState::OnPrecision;
					}
				break;
				case ',':
					bCommas = true;
				break;
				case 'd':
				case 'x':
				case 'X':
				case 'b':
				case 'c':
				case 'o':
				case 'n':
				case 'e':
				case 'E':
				case 'f':
				case 'F':
				case 'g':
				case 'G':
				case '%':
					Type = Char;
				break;
			}
		}
	}

	void AlignString(FString& Str)
	{
		if (Align == EAlign::None)
			return;
		if (MinimumWidth.Len() == 0)
			return;

		int32 Length = 0;
		LexFromString(Length, *MinimumWidth);

		int Count = Length - Str.Len();
		if (Count <= 0)
			return;

		FString NewStr;
		NewStr.Reserve(Str.Len() + Count);

		switch (Align)
		{
		case EAlign::Left:
		{
			NewStr.Append(Str);
			for (int i = 0; i < Count; ++i)
				NewStr.AppendChar(Fill);
		}
		break;
		case EAlign::Right:
		case EAlign::AfterSign:
		{
			for (int i = 0; i < Count; ++i)
				NewStr.AppendChar(Fill);
			NewStr.Append(Str);
		}
		break;
		case EAlign::Middle:
		{
			int32 LeftCount = Count / 2;
			int32 RightCount = Count - LeftCount;

			for (int i = 0; i < LeftCount; ++i)
				NewStr.AppendChar(Fill);
			NewStr.Append(Str);
			for (int i = 0; i < RightCount; ++i)
				NewStr.AppendChar(Fill);
		}
		break;
		}

		Str = NewStr;
	}

	template<typename T>
	void PrependSign(T Number, FString& OutStr)
	{
		switch(Sign)
		{
			case FFormatSpecifier::ESign::Both:
				if (Number >= 0)
				{
					FString Result;
					Result.Reserve(OutStr.Len()+1);
					Result.AppendChar('+');
					Result.Append(OutStr);
					OutStr = MoveTemp(Result);
				}
				else
				{
					FString Result;
					Result.Reserve(OutStr.Len()+1);
					Result.AppendChar('-');
					Result.Append(OutStr);
					OutStr = MoveTemp(Result);
				}
			break;
			case FFormatSpecifier::ESign::Negative:
				if (Number < 0)
				{
					FString Result;
					Result.Reserve(OutStr.Len()+1);
					Result.AppendChar('-');
					Result.Append(OutStr);
					OutStr = MoveTemp(Result);
				}
			break;
			case FFormatSpecifier::ESign::LeadingSpace:
				if (Number >= 0)
				{
					FString Result;
					Result.Reserve(OutStr.Len()+1);
					Result.AppendChar(' ');
					Result.Append(OutStr);
					OutStr = MoveTemp(Result);
				}
				else
				{
					FString Result;
					Result.Reserve(OutStr.Len()+1);
					Result.AppendChar('-');
					Result.Append(OutStr);
					OutStr = MoveTemp(Result);
				}
			break;
		}
	}

	void InsertCommas(FString& OutStr)
	{
		int32 Count = 0;
		for (int32 Pos = OutStr.Len() - 1; Pos >= 0; --Pos)
		{
			if (OutStr[Pos] == '.')
				Count = 0;
			else
				Count += 1;

			if (Count == 3)
			{
				OutStr.InsertAt(Pos, ',');
				Count = 0;
			}
		}
	}
};

FString FAngelscriptFStringBinds::ApplyFormatString(const FString& Str, const FString& Specifier)
{
	FString OutStr = Str;
	FFormatSpecifier Spec(Specifier);
	Spec.AlignString(OutStr);
	return OutStr;
}

FString FAngelscriptFStringBinds::ApplyFormatValue(void* ValuePtr, int TypeId, const FString& Specifier)
{
	FFormatSpecifier Spec(Specifier);
	FString OutStr;

	asITypeInfo* TypeInfo = FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId);
	if (TypeInfo != nullptr && (TypeInfo->GetFlags() & asOBJ_ENUM) != 0)
	{
		if (Spec.Type == 'n')
		{
			UUserDefinedEnum* UnrealEnum = (UUserDefinedEnum*)TypeInfo->GetUserData();
			if (UnrealEnum != nullptr)
			{
				OutStr = UnrealEnum->GetNameStringByValue(*(uint8*)ValuePtr);
			}
			else
			{
				// Only print the value of the enum in :n mode
				asUINT EnumCount = TypeInfo->GetEnumValueCount();
				for(asUINT i = 0; i < EnumCount; ++i)
				{
					int EnumValue;
					const char* ValueName = TypeInfo->GetEnumValueByIndex(i, &EnumValue);
					if (EnumValue == *(uint8*)ValuePtr && ValueName != nullptr)
					{
						OutStr = ANSI_TO_TCHAR(ValueName);
						break;
					}
				}

				if (OutStr.IsEmpty())
				{
					OutStr = FString::Printf(
						TEXT("%d"),
						*(uint8*)ValuePtr
					);
				}
			}
		}
		else
		{
			FToStringHelper::Generic_AppendToString(OutStr, ValuePtr, TypeId);
		}
	}
	else
	{
		FToStringHelper::Generic_AppendToString(OutStr, ValuePtr, TypeId);
	}

	Spec.AlignString(OutStr);
	
	return OutStr;
}

FString FAngelscriptFStringBinds::ApplyFormatBool(bool Value, const FString& Specifier)
{
	FString OutStr = Value ? TEXT("true") : TEXT("false");

	FFormatSpecifier Spec(Specifier);
	Spec.AlignString(OutStr);
	
	return OutStr;
}

template<typename T, bool IsUnsigned, typename UnsignedType>
static FString ApplyFormatInteger(T Number, const FString& Specifier)
{
	FString OutStr;
	OutStr.Reserve(16);

	UnsignedType UnsignedValue = *(UnsignedType*)&Number;

	T AbsValue = Number;
	if (!IsUnsigned && Number < 0)
		AbsValue = -1 * Number;

	FFormatSpecifier Spec(Specifier);

	// Format the actual number
	bool bInsertCommas = Spec.bCommas;
	switch (Spec.Type)
	{
	case 'b':
	{
		bool bFoundDigits = false;
		for (int32 i = sizeof(T) * 8 - 1; i >= 0; --i)
		{
			if ((UnsignedValue & (((UnsignedType)1) << i)) != 0)
			{
				OutStr.AppendChar('1');
				bFoundDigits = true;
			}
			else if (bFoundDigits)
			{
				OutStr.AppendChar('0');
			}
		}
	}
	break;
	case 'c':
		OutStr.AppendChar((int16)Number);
	break;
	case 'n':
		bInsertCommas = true;
		// Fallthrough to 'd'
	default:
	case 'd':
		if (sizeof(T) == 8)
		{
			if (IsUnsigned)
				OutStr += FString::Printf(TEXT("%lu"), UnsignedValue);
			else
				OutStr += FString::Printf(TEXT("%ld"), AbsValue);
		}
		else
		{
			if (IsUnsigned)
				OutStr += FString::Printf(TEXT("%u"), UnsignedValue);
			else
				OutStr += FString::Printf(TEXT("%d"), AbsValue);
		}
	break;
	case 'o':
		{
			TCHAR Buffer[32];
			int32 BufferIndex = UE_ARRAY_COUNT(Buffer) - 1;
			Buffer[BufferIndex] = TEXT('\0');

			do
			{
				--BufferIndex;
				Buffer[BufferIndex] = static_cast<TCHAR>(TEXT('0') + (UnsignedValue & 7));
				UnsignedValue >>= 3;
			}
			while (UnsignedValue != 0);

			OutStr += &Buffer[BufferIndex];
		}
	break;
	case 'x':
		OutStr += FString::Printf(TEXT("%x"), UnsignedValue);
	break;
	case 'X':
		OutStr += FString::Printf(TEXT("%X"), UnsignedValue);
	break;
	}

	// Insert commas
	if (bInsertCommas)
		Spec.InsertCommas(OutStr);

	FString Prefix;
	if (!IsUnsigned)
	{
		switch (Spec.Sign)
		{
		case FFormatSpecifier::ESign::Both:
			Prefix.AppendChar(Number >= 0 ? '+' : '-');
		break;
		case FFormatSpecifier::ESign::Negative:
			if (Number < 0)
			{
				Prefix.AppendChar('-');
			}
		break;
		case FFormatSpecifier::ESign::LeadingSpace:
			Prefix.AppendChar(Number >= 0 ? ' ' : '-');
		break;
		}
	}

	if (Spec.bPrefixBase)
	{
		switch (Spec.Type)
		{
			case 'b':
				Prefix += TEXT("0b");
			break;
			case 'x':
			case 'X':
				Prefix += TEXT("0x");
			break;
			case 'o':
				Prefix += TEXT("0o");
			break;
		}
	}

	// Align after sign/base prefix so width accounting matches the final visible output.
	if (Spec.Align == FFormatSpecifier::EAlign::AfterSign && Spec.MinimumWidth.Len() != 0)
	{
		int32 Length = 0;
		LexFromString(Length, *Spec.MinimumWidth);

		const int32 Count = Length - Prefix.Len() - OutStr.Len();
		if (Count > 0)
		{
			FString Padded;
			Padded.Reserve(OutStr.Len() + Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Padded.AppendChar(Spec.Fill);
			}
			Padded.Append(OutStr);
			OutStr = MoveTemp(Padded);
		}
	}

	if (!Prefix.IsEmpty())
	{
		Prefix.Append(OutStr);
		OutStr = MoveTemp(Prefix);
	}

	// Align after sign
	if (Spec.Align != FFormatSpecifier::EAlign::AfterSign)
		Spec.AlignString(OutStr);

	return OutStr;
}

template<typename T>
static FString ApplyFormatFloatingPoint(T Number, const FString& Specifier)
{
	FString OutStr;
	OutStr.Reserve(16);

	T AbsValue = FMath::Abs(Number);
	FFormatSpecifier Spec(Specifier);

	// Format the actual number
	bool bInsertCommas = Spec.bCommas;
	switch (Spec.Type)
	{
	default:
	case 'f':
	case 'F':
	{
		if (Spec.Precision.Len() != 0)
		{
			int32 Precision = 0;
			LexFromString(Precision, *Spec.Precision);
			OutStr += FString::Printf(TEXT("%.*f"), Precision, AbsValue);
		}
		else
		{
			OutStr += FString::SanitizeFloat(AbsValue);
		}
	}
	break;
	case 'e':
		{
			TCHAR Buffer[64];
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%e"), AbsValue);
			OutStr += Buffer;
		}
	break;
	case 'E':
		{
			TCHAR Buffer[64];
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%e"), AbsValue);
			OutStr += Buffer;
		}
	break;
	case 'n':
		bInsertCommas = true;
		// Fallthrough to 'g'
	case 'g':
		if (Spec.Precision.Len() != 0)
		{
			int32 Precision = 0;
			LexFromString(Precision, *Spec.Precision);
			TCHAR Buffer[64];
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%.*g"), Precision, AbsValue);
			OutStr += Buffer;
		}
		else
		{
			TCHAR Buffer[64];
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%g"), AbsValue);
			OutStr += Buffer;
		}
	break;
	case 'G':
		if (Spec.Precision.Len() != 0)
		{
			int32 Precision = 0;
			LexFromString(Precision, *Spec.Precision);
			TCHAR Buffer[64];
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%.*g"), Precision, AbsValue);
			OutStr += Buffer;
		}
		else
		{
			TCHAR Buffer[64];
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%g"), AbsValue);
			OutStr += Buffer;
		}
	break;
	case '%':
		AbsValue *= 100;
		if (Spec.Precision.Len() != 0)
		{
			int32 Precision = 0;
			LexFromString(Precision, *Spec.Precision);
			OutStr += FString::Printf(TEXT("%.*f%%"), Precision, AbsValue);
		}
		else
		{
			OutStr += FString::Printf(TEXT("%.0f%%"), AbsValue);
		}
	break;
	}

	// Insert commas
	if (bInsertCommas)
		Spec.InsertCommas(OutStr);

	// Align before sign
	if (Spec.Align == FFormatSpecifier::EAlign::AfterSign)
		Spec.AlignString(OutStr);

	// Uppercase?
	if (Spec.Type == 'E' || Spec.Type == 'G' || Spec.Type == 'F')
		OutStr.ToUpperInline();

	// Add sign
	Spec.PrependSign(Number, OutStr);

	// Align after sign
	if (Spec.Align != FFormatSpecifier::EAlign::AfterSign)
		Spec.AlignString(OutStr);

	return OutStr;
}

FString FAngelscriptFStringBinds::ApplyFormatInt32(int32 Value, const FString& Specifier)
{
	return ApplyFormatInteger<int32, false, uint32>(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatUInt32(uint32 Value, const FString& Specifier)
{
	return ApplyFormatInteger<uint32, true, uint32>(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatInt64(int64 Value, const FString& Specifier)
{
	return ApplyFormatInteger<int64, false, uint64>(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatUInt64(uint64 Value, const FString& Specifier)
{
	return ApplyFormatInteger<uint64, true, uint64>(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatInt16(int16 Value, const FString& Specifier)
{
	return ApplyFormatInteger<int16, false, uint16>(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatUInt16(uint16 Value, const FString& Specifier)
{
	return ApplyFormatInteger<uint16, true, uint16>(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatInt8(int8 Value, const FString& Specifier)
{
	return ApplyFormatInteger<int8, false, uint8>(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatUInt8(uint8 Value, const FString& Specifier)
{
	return ApplyFormatInteger<uint8, true, uint8>(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatFloat(float Value, const FString& Specifier)
{
	return ApplyFormatFloatingPoint(Value, Specifier);
}

FString FAngelscriptFStringBinds::ApplyFormatDouble(double Value, const FString& Specifier)
{
	return ApplyFormatFloatingPoint(Value, Specifier);
}

static void BindFStringConversion(FAngelscriptBinds& Binds)
{
	auto FString_ = Binds.ExistingClassForTarget("FString");

	auto& ToStringList = Binds.GetTargetToStringList();

	for (auto& ToString : ToStringList)
	{
		FString QualifiedType = ToString.TypeName;
		FString ObjectType = ToString.TypeName;

		if (!ToString.bIsHandleType)
			QualifiedType += TEXT("&");


		{
			FString Decl = FString::Printf(TEXT("FString opAdd(const %s Value) const"), *QualifiedType);
			FString_.Method(Decl, &FAngelscriptFStringBinds::AddConverted, (void*)ToString.ToString)
				.PassScriptFunctionAsFirstParam();
		}

		{
			FString Decl = FString::Printf(TEXT("FString& opAddAssign(const %s Value)"), *QualifiedType);
			FString_.Method(Decl, &FAngelscriptFStringBinds::AddAssignConverted, (void*)ToString.ToString)
				.PassScriptFunctionAsFirstParam();
		}

		{
			FString Decl = FString::Printf(TEXT("FString& Append(const %s Value) accept_temporary_this"), *QualifiedType);
			FString_.Method(Decl, &FAngelscriptFStringBinds::AppendConverted, (void*)ToString.ToString)
				.PassScriptFunctionAsFirstParam();
		}

		auto* Type = Binds.GetTargetScriptEngine().GetTypeInfoByName(TCHAR_TO_ANSI(*ObjectType));
		if (Type != nullptr)
		{
			ToString.TypeInfo = Type;
			auto BoundType = Binds.ExistingClassForTarget(Type->GetName());
			BoundType.Method("FString ToString() const", &FAngelscriptFStringBinds::TypeToString, (void*)ToString.ToString)
				.PassScriptFunctionAsFirstParam();
		}

		if (ToString.bImplicitConversion)
		{
			FString Decl = FString::Printf(TEXT("void f(const %s Value)"), *QualifiedType);
			FString_.Constructor(Decl, &FAngelscriptFStringBinds::ConstructConverted, (void*)ToString.ToString)
				.PassScriptFunctionAsFirstParam();
		}
	}

	// Static conversion
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FString");

		Binds.BindGlobalFunctionForTarget(
			"FString Join(const TArray<FString>& StringArray, const FString& Separator) no_discard",
			&FAngelscriptFStringBinds::Join);
		Binds.BindGlobalFunctionForTarget("FString FromInt(int32 Num) no_discard", &FAngelscriptFStringBinds::FromInt);
		Binds.BindGlobalFunctionForTarget(
			"FString SanitizeFloat(float64 InFloat, int32 InMinFractionalDigits = 1) no_discard",
			&FAngelscriptFStringBinds::SanitizeFloat);
		Binds.BindGlobalFunctionForTarget(
			"FString FormatAsNumber(int32 InNumber) no_discard",
			&FAngelscriptFStringBinds::FormatAsNumber);
		Binds.BindGlobalFunctionForTarget("FString Chr(int16 Ch) no_discard", &FAngelscriptFStringBinds::Chr);
		Binds.BindGlobalFunctionForTarget(
			"FString ChrN(int32 NumCharacters, int16 Char) no_discard",
			&FAngelscriptFStringBinds::ChrN);

		Binds.BindGlobalGenericFunctionForTarget(
			"FString Format(const FString& Format, const ?& Arg0) no_discard",
			&FAngelscriptFStringBinds::Format);
		Binds.BindGlobalGenericFunctionForTarget(
			"FString Format(const FString& Format, const ?& Arg0, const ?& Arg1) no_discard",
			&FAngelscriptFStringBinds::Format);
		Binds.BindGlobalGenericFunctionForTarget(
			"FString Format(const FString& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2) no_discard",
			&FAngelscriptFStringBinds::Format);
		Binds.BindGlobalGenericFunctionForTarget(
			"FString Format(const FString& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2, const ?& Arg3) no_discard",
			&FAngelscriptFStringBinds::Format);
		Binds.BindGlobalGenericFunctionForTarget(
			"FString Format(const FString& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2, const ?& Arg3, const ?& Arg4) no_discard",
			&FAngelscriptFStringBinds::Format);

		// Format specifiers
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(int32 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatInt32);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(uint32 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatUInt32);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(int64 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatInt64);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(uint64 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatUInt64);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(int16 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatInt16);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(uint16 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatUInt16);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(int8 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatInt8);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(uint8 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatUInt8);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(bool Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatBool);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(float32 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatFloat);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(float64 Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatDouble);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(const FString& Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatString);
		Binds.BindGlobalFunctionForTarget("FString ApplyFormat(const ?& Value, const FString& Specifier)", &FAngelscriptFStringBinds::ApplyFormatValue);
	}

	FString_.Method("FString opAdd(const ?& Value) const", &FAngelscriptFStringBinds::AddValue);
	FString_.Method("FString& opAddAssign(const ?& Value)", &FAngelscriptFStringBinds::AddAssignValue);
	FString_.Method(
		"FString& Append(const ?& Value) accept_temporary_this",
		&FAngelscriptFStringBinds::AppendValue);

	// Array parsing
	FString_.Method(
		"int ParseIntoArray(TArray<FString>& OutArray, const FString& Delimiter, bool bCullEmpty = true) const",
		&FAngelscriptFStringBinds::ParseIntoArray);
	FString_.Method(
		"int ParseIntoArray(TArray<FString>& OutArray, const TArray<FString>& Delimiters, bool bCullEmpty = true) const",
		&FAngelscriptFStringBinds::ParseIntoArrayMulti);
	FString_.Method(
		"int ParseIntoArrayLines(TArray<FString>& OutArray, bool bCullEmpty = true) const",
		&FAngelscriptFStringBinds::ParseIntoArrayLines);
	FString_.Method(
		"int ParseIntoArrayWS(TArray<FString>& OutArray, bool bCullEmpty = true) const",
		&FAngelscriptFStringBinds::ParseIntoArrayWhitespace);
}

static void BindFStringManualBindings(FAngelscriptBinds& Binds)
{
	BindFStringCore(Binds);
	BindFStringConversion(Binds);
}

AS_FORCE_LINK const FAngelscriptBind Bind_FString_TypeDeclarations(
	TEXT("FString.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFStringTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_FString_TypeInfrastructure(
	TEXT("FString.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFStringInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FString_ManualBindings(
	TEXT("FString.ManualBindings"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFStringManualBindings);
