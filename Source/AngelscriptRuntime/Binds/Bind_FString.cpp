#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptRuntimeModule.h"

#include "Containers/UnrealString.h"
#include "Engine/UserDefinedEnum.h"

#include "Helper_CppType.h"
#include "Helper_GetTypeInfo.h"
#include "Helper_ToString.h"

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
		if (TypeInfo == TGetStaticTypeInfo<FString>::TypeInfo)
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

static void Generic_FormatString(asIScriptGeneric* Generic)
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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FString(FAngelscriptBinds::EOrder::Early, []
{
	// Register string type
	auto FString_ = FAngelscriptBinds::ValueClass<FString>("FString", FBindFlags());
	FAngelscriptType::Register(MakeShared<FStringType>());

	TGetStaticTypeInfo<FString>::TypeInfo = FString_.GetTypeInfo();

	FString_.Constructor("void f()", [](FString* Address)
	{
		new(Address) FString();
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FString_, "FString");

	FString_.Constructor("void f(const FString& Other)", [](FString* Address, const FString& Other)
	{
		new(Address) FString(Other);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FString_, "FString");

	FString_.Destructor("void f()", [](FString& Str)
	{
		Str.~FString();
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_DESTRUCTOR(FString_, "FString");
	
	// Bind operator overloads
	FString_.Method("FString& opAssign(const FString& Other)", METHODPR_TRIVIAL(FString&, FString, operator=, (const FString&)));
	FString_.Method("FString& opAddAssign(const FString& Other)", METHODPR_TRIVIAL(FString&, FString, operator+=, (const FString&)));
	FString_.Method("bool opEquals(const FString& Other) const", [](const FString& Left, const FString& Right) -> bool
	{
		return Left == Right;
	});
	FString_.Method("int opCmp(const FString& Other) const", [](const FString& Left, const FString& Right) -> int
	{
		return Left.Compare(Right);
	});
	FString_.Method("FString opAdd(const FString& Other) const", [](const FString& Left, const FString& Right)
	{
		return Left + Right;
	});

	FString_.Method("int16& opIndex(int32 Index)", [](FString& String, int32 Index) -> TCHAR&
	{
		if (!String.IsValidIndex(Index))
		{
			FAngelscriptEngine::Throw("String index out of bounds.");
			static TCHAR InvalidChar;
			return InvalidChar;
		}

		return String[Index];
	});

	FString_.Method("const int16& opIndex(int32 Index) const", [](const FString& String, int32 Index) -> const TCHAR&
	{
		if (!String.IsValidIndex(Index))
		{
			FAngelscriptEngine::Throw("String index out of bounds.");
			static TCHAR InvalidChar;
			return InvalidChar;
		}

		return String[Index];
	});

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
	FString_.Method("void RemoveAt(int Index, int Count)", [](FString& String, int32 Index, int32 Count)
	{
		String.RemoveAt(Index, Count);
	});
	FString_.Method("void RemoveSpacesInline()", METHOD_TRIVIAL(FString, RemoveSpacesInline));

	// Handling as string
	FString_.Method("int Len() const", METHOD_TRIVIAL(FString, Len));
	FString_.Method("bool IsNumeric() const", METHOD_TRIVIAL(FString, IsNumeric));
	FString_.Method("FString Reverse() const", METHOD_TRIVIAL(FString, Reverse));

	FString_.Method("FString ConvertTabsToSpaces(int32 InSpacesPerTab) const",
	[](const FString& Str, int32 InSpacesPerTab) -> FString
	{
		return Str.ConvertTabsToSpaces(InSpacesPerTab);
	});

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
	[](const FString& Str, const FString& Needle, FString& OutLeft, FString& OutRight, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir)
	{
		return Str.Split(Needle, &OutLeft, &OutRight, SearchCase, SearchDir);
	});

	FString_.Method("FString Replace(const FString& From, const FString& To, ESearchCase SearchCase = ESearchCase::IgnoreCase) const",
	[](const FString& Str, const FString& From, const FString& To, ESearchCase::Type SearchCase) -> FString
	{
		return Str.Replace(*From, *To, SearchCase);
	});

	FString_.Method("int ReplaceInline(const FString& SearchText, const FString& ReplacementText, ESearchCase SearchCase = ESearchCase::IgnoreCase)",
	[](FString& Str, const FString& SearchText, const FString& ReplacementText, ESearchCase::Type SearchCase) -> int32
	{
		return Str.ReplaceInline(*SearchText, *ReplacementText, SearchCase);
	});

	FString_.Method("FString ReplaceCharWithEscapedChar() const",
	[](const FString& Str) -> FString
	{
		return Str.ReplaceCharWithEscapedChar();
	});

	FString_.Method("FString ReplaceEscapedCharWithChar() const",
	[](const FString& Str) -> FString
	{
		return Str.ReplaceEscapedCharWithChar();
	});

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
	FString_.Method("FString ToUpper() const", [](const FString& Str) -> FString
	{
		return Str.ToUpper();
	});

	FString_.Method("FString ToLower() const", [](const FString& Str) -> FString
	{
		return Str.ToLower();
	});

	// Whitespace handling
	FString_.Method("FString LeftPad(int Count) const", METHOD_TRIVIAL(FString, LeftPad));
	FString_.Method("FString RightPad(int Count) const", METHOD_TRIVIAL(FString, RightPad));
	FString_.Method("FString TrimQuotes(bool& OutQuotesRemoved) const", METHODPR_TRIVIAL(FString, FString, TrimQuotes, (bool*) const &));

	FString_.Method("FString TrimStartAndEnd() const", [](const FString& Str) -> FString
	{
		return Str.TrimStartAndEnd();
	});

	FString_.Method("FString TrimStart() const", [](const FString& Str) -> FString
	{
		return Str.TrimStart();
	});

	FString_.Method("FString TrimEnd() const", [](const FString& Str) -> FString
	{
		return Str.TrimEnd();
	});

	FString_.Method("FString TrimChar(int16 CharacterToTrim) const",
	[](const FString& Str, TCHAR CharacterToTrim) -> FString
	{
		return Str.TrimChar(CharacterToTrim);
	});

	FString_.Method("int32 Compare(const FString& Other, ESearchCase SearchCase = ESearchCase::CaseSensitive) const", METHODPR_TRIVIAL(int32, FString, Compare, (const FString&, ESearchCase::Type) const));

	// Conversion
	FString_.Method("bool ToBool() const", METHOD_TRIVIAL(FString, ToBool));

	class FStringFactory : public asIStringFactory
	{
		const void* GetStringConstant(const char* Data, asUINT Length) override
		{
			FUTF8ToTCHAR Convertor(Data, Length);
			auto* Str = new FString();
			Str->AppendChars(Convertor.Get(), Convertor.Length());
			return Str;
		}

		int ReleaseStringConstant(const void* Str) override
		{
			delete (FString*)Str;
			return 0;
		}

		int GetRawStringData(const void* Str, char* Data, asUINT* Length) const override
		{
			FString* UnrealString = (FString*)Str;
			if (UnrealString->Len() == 0)
			{
				if (Length != nullptr)
					*Length = 0;
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

	FString_.Method("FString ToDisplayName(bool bIsBool = false) const", 
	[](FString& Str, bool bIsBool) -> FString
	{
		return FName::NameToDisplayString(Str, bIsBool);
	});
	
	FString_.Method("uint GetHash() const", [](const FString& Str) -> uint32
	{
		return GetTypeHash(Str);
	});

	// Register string factory with engine
	FAngelscriptEngine::Get().Engine->RegisterStringFactory("FString", new FStringFactory());
});

static TArray<FToStringType>& GetToStringList()
{
	if (FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		if (TArray<FToStringType>* List = Engine->GetToStringList())
		{
			return *List;
		}
	}
	static TArray<FToStringType> LegacyToStringList;
	return LegacyToStringList;
}

void FToStringHelper::Register(const FString& TypeName, FToStringHelper::FToStringFunction ToString, bool bImplicitConversion, bool bIsHandleType)
{
	GetToStringList().Add({TypeName, nullptr, ToString, bImplicitConversion, bIsHandleType});
}

void FToStringHelper::Reset()
{
	GetToStringList().Reset();
}

#if WITH_DEV_AUTOMATION_TESTS
int32 FToStringHelper::GetRegisteredTypeCountForTesting()
{
	return GetToStringList().Num();
}
#endif

static FString Type_ToString(void* Obj, asCScriptFunction* ScriptFunction)
{
	auto Func = (FToStringHelper::FToStringFunction)ScriptFunction->userData;
	FString Str;
	Func(Obj, Str);
	return Str;
}

void FToStringHelper::Generic_AppendToString(FString& AppendTo, void* ValuePtr, int TypeId)
{
	asITypeInfo* TypeInfo = FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId);

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
			auto& Delegate = FAngelscriptRuntimeModule::GetDebugObjectSuffix();
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
		for (auto& ToString : GetToStringList())
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

static FString ApplyFormatString(const FString& Str, const FString& Specifier)
{
	FString OutStr = Str;
	FFormatSpecifier Spec(Specifier);
	Spec.AlignString(OutStr);
	return OutStr;
}

static FString ApplyFormat(void* ValuePtr, int TypeId, const FString& Specifier)
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

static FString ApplyFormatBool(bool Value, const FString& Specifier)
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
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%o"), UnsignedValue);
			OutStr += Buffer;
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
static FString ApplyFormatFloat(T Number, const FString& Specifier)
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
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%E"), AbsValue);
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
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%.*G"), Precision, AbsValue);
			OutStr += Buffer;
		}
		else
		{
			TCHAR Buffer[64];
			FCString::Snprintf(Buffer, UE_ARRAY_COUNT(Buffer), TEXT("%G"), AbsValue);
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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FString_Conversion((int32)FAngelscriptBinds::EOrder::Late+10, []
{
	auto FString_ = FAngelscriptBinds::ExistingClass("FString");

	for (auto& ToString : GetToStringList())
	{
		FString QualifiedType = ToString.TypeName;
		FString ObjectType = ToString.TypeName;

		if (!ToString.bIsHandleType)
			QualifiedType += TEXT("&");


		{
			FString Decl = FString::Printf(TEXT("FString opAdd(const %s Value) const"), *QualifiedType);
			FString_.Method(Decl, [](FString& Str, asCScriptFunction* ScriptFunction, void* Value) -> FString
			{
				FString OutValue = Str;

				auto Func = (FToStringHelper::FToStringFunction)ScriptFunction->userData;
				Func(Value, OutValue);

				return OutValue;
			}, (void*)ToString.ToString);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
		}

		{
			FString Decl = FString::Printf(TEXT("FString& opAddAssign(const %s Value)"), *QualifiedType);
			FString_.Method(Decl, [](FString& Str, asCScriptFunction* ScriptFunction, void* Value) -> FString&
			{
				auto Func = (FToStringHelper::FToStringFunction)ScriptFunction->userData;
				Func(Value, Str);
				return Str;
			}, (void*)ToString.ToString);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
		}

		{
			FString Decl = FString::Printf(TEXT("FString& Append(const %s Value) accept_temporary_this"), *QualifiedType);
			FString_.Method(Decl, [](FString& Str, asCScriptFunction* ScriptFunction, void* Value) -> FString&
			{
				auto Func = (FToStringHelper::FToStringFunction)ScriptFunction->userData;
				Func(Value, Str);
				return Str;
			}, (void*)ToString.ToString);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
		}

		auto* Type = FAngelscriptEngine::Get().Engine->GetTypeInfoByName(TCHAR_TO_ANSI(*ObjectType));
		if (Type != nullptr)
		{
			ToString.TypeInfo = Type;
			FAngelscriptBinds::BindMethodDirect(Type->GetName(), "FString ToString() const",
				asFUNCTION(Type_ToString), asCALL_CDECL_OBJFIRST,
				ASAutoCaller::MakeFunctionCaller(Type_ToString), (void*)ToString.ToString);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
		}

		if (ToString.bImplicitConversion)
		{
			FString Decl = FString::Printf(TEXT("void f(const %s Value)"), *QualifiedType);
			FString_.Constructor(Decl, [](FString* Str, asCScriptFunction* ScriptFunction, void* Value)
			{
				new(Str) FString();
				auto Func = (FToStringHelper::FToStringFunction)ScriptFunction->userData;
				Func(Value, *Str);
			}, (void*)ToString.ToString);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
		}
	}

	// Static conversion
	{
		FAngelscriptBinds::FNamespace ns("FString");

		FAngelscriptBinds::BindGlobalFunction("FString Join(const TArray<FString>& StringArray, const FString& Separator) no_discard",
		[](const TArray<FString>& StringArray, const FString& Separator) {
			return FString::Join(StringArray, *Separator);
		});

		FAngelscriptBinds::BindGlobalFunction("FString FromInt(int32 Num) no_discard",
		[](int32 Num) { return FString::FromInt(Num); });

		FAngelscriptBinds::BindGlobalFunction("FString SanitizeFloat(float64 InFloat, int32 InMinFractionalDigits = 1) no_discard",
		[](double InFloat, int32 InMinFractionalDigits) { return FString::SanitizeFloat(InFloat, InMinFractionalDigits); });

		FAngelscriptBinds::BindGlobalFunction("FString FormatAsNumber(int32 InNumber) no_discard",
		[](int32 InNumber) { return FString::FormatAsNumber(InNumber); });

		FAngelscriptBinds::BindGlobalFunction("FString Chr(int16 Ch) no_discard",
		[](TCHAR Ch) { return FString::Chr(Ch); });

		FAngelscriptBinds::BindGlobalFunction("FString ChrN(int32 NumCharacters, int16 Char) no_discard",
		[](int32 NumCharacters, TCHAR Char) { return FString::ChrN(NumCharacters, Char); });

		FAngelscriptBinds::BindGlobalGenericFunction("FString Format(const FString& Format, const ?& Arg0) no_discard", &Generic_FormatString);
		FAngelscriptBinds::BindGlobalGenericFunction("FString Format(const FString& Format, const ?& Arg0, const ?& Arg1) no_discard", &Generic_FormatString);
		FAngelscriptBinds::BindGlobalGenericFunction("FString Format(const FString& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2) no_discard", &Generic_FormatString);
		FAngelscriptBinds::BindGlobalGenericFunction("FString Format(const FString& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2, const ?& Arg3) no_discard", &Generic_FormatString);
		FAngelscriptBinds::BindGlobalGenericFunction("FString Format(const FString& Format, const ?& Arg0, const ?& Arg1, const ?& Arg2, const ?& Arg3, const ?& Arg4) no_discard", &Generic_FormatString);

		// Format specifiers
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(int32 Value, const FString& Specifier)", &ApplyFormatInteger<int32, false, uint32>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(uint32 Value, const FString& Specifier)", &ApplyFormatInteger<uint32, true, uint32>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(int64 Value, const FString& Specifier)", &ApplyFormatInteger<int64, false, uint64>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(uint64 Value, const FString& Specifier)", &ApplyFormatInteger<uint64, true, uint64>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(int16 Value, const FString& Specifier)", &ApplyFormatInteger<int16, false, uint16>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(uint16 Value, const FString& Specifier)", &ApplyFormatInteger<uint16, true, uint16>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(int8 Value, const FString& Specifier)", &ApplyFormatInteger<int8, false, uint8>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(uint8 Value, const FString& Specifier)", &ApplyFormatInteger<uint8, true, uint8>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(bool Value, const FString& Specifier)", &ApplyFormatBool);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(float32 Value, const FString& Specifier)", &ApplyFormatFloat<float>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(float64 Value, const FString& Specifier)", &ApplyFormatFloat<double>);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(const FString& Value, const FString& Specifier)", &ApplyFormatString);
		FAngelscriptBinds::BindGlobalFunction("FString ApplyFormat(const ?& Value, const FString& Specifier)", &ApplyFormat);
	}

	FString_.Method("FString opAdd(const ?& Value) const",
	[](const FString& Str, void* ValuePtr, int TypeId) -> FString
	{
		FString NewStr = Str;
		FToStringHelper::Generic_AppendToString(NewStr, ValuePtr, TypeId);
		return NewStr;
	});

	FString_.Method("FString& opAddAssign(const ?& Value)",
	[](FString& Str, void* ValuePtr, int TypeId) -> FString&
	{
		FToStringHelper::Generic_AppendToString(Str, ValuePtr, TypeId);
		return Str;
	});

	FString_.Method("FString& Append(const ?& Value) accept_temporary_this",
	[](FString& Str, void* ValuePtr, int TypeId) -> FString&
	{
		FToStringHelper::Generic_AppendToString(Str, ValuePtr, TypeId);
		return Str;
	});

	// Array parsing
	FString_.Method("int ParseIntoArray(TArray<FString>& OutArray, const FString& Delimiter, bool bCullEmpty = true) const",
	[](const FString& Str, TArray<FString>& OutArray, const FString& Delimiter, bool bCullEmpty) -> int
	{
		return Str.ParseIntoArray(OutArray, *Delimiter, bCullEmpty);
	});

	FString_.Method("int ParseIntoArray(TArray<FString>& OutArray, const TArray<FString>& Delimiters, bool bCullEmpty = true) const",
	[](const FString& Str, TArray<FString>& OutArray, const TArray<FString>& Delimiters, bool bCullEmpty) -> int
	{
		if (Delimiters.Num() > 16)
		{
			FAngelscriptEngine::Throw("More than 16 delimiters is not supported by ParseIntoArray.");
			return 0;
		}

		const TCHAR* DelimList[16];
		for (int32 i = 0, Count = Delimiters.Num(); i < Count; ++i)
			DelimList[i] = *Delimiters[i];

		return Str.ParseIntoArray(OutArray, DelimList, Delimiters.Num(), bCullEmpty);
	});

	FString_.Method("int ParseIntoArrayLines(TArray<FString>& OutArray, bool bCullEmpty = true) const",
	[](const FString& Str, TArray<FString>& OutArray, bool bCullEmpty) -> int
	{
		return Str.ParseIntoArrayLines(OutArray, bCullEmpty);
	});

	FString_.Method("int ParseIntoArrayWS(TArray<FString>& OutArray, bool bCullEmpty = true) const",
	[](const FString& Str, TArray<FString>& OutArray, bool bCullEmpty) -> int
	{
		return Str.ParseIntoArrayWS(OutArray, nullptr, bCullEmpty);
	});
});
