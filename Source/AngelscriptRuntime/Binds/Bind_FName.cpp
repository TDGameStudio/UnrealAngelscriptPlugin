#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "StaticJIT/StaticJITHelperFunctions.h"

#include "Bind_FName_Functions.h"
#include "Helper_ToString.h"
#include "Binds/Helper_PODType.h"

struct FNameType : TAngelscriptPODPropertyType<FNameProperty>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FName");
	}

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* Address) const override
	{
		new(Address) FName();
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("None") || InValue == TEXT("NAME_None") || InValue.IsEmpty())
		{
			OutValue = TEXT("NAME_None");
			return true;
		}
		else
		{
			OutValue = FString::Printf(TEXT("FName(\"%s\")"), *OutValue);
			return true;
		}
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		OutValue = InValue;
		OutValue.TrimStartAndEndInline();
		if (OutValue == TEXT("None") || OutValue == TEXT("NAME_None"))
		{
			OutValue = TEXT("None");
			return true;
		}

		if (OutValue.RemoveFromStart(TEXT("FName")))
		{
			OutValue.TrimStartAndEndInline();
			OutValue.RemoveFromStart(TEXT("("));
			OutValue.RemoveFromEnd(TEXT(")"));
		}

		// FName literals have been turned into __STATIC_NAME calls by the script preprocessor
		if (OutValue.RemoveFromStart(TEXT("__STATIC_NAME (")))
		{
			int32 Index = -1;
			LexFromString(Index, *OutValue);

			FName StaticName;
			if (FAngelscriptEngine::TryGetStaticName(Index, StaticName))
			{
				OutValue = StaticName.ToString();
				return true;
			}
			else
			{
				return false;
			}
		}

		OutValue.TrimStartAndEndInline();
		OutValue = OutValue.TrimQuotes();
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		FName& NativeValue = Usage.ResolvePrimitive<FName>(Address);

		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Address = Address;
		// Ugly hack since we don't have access to the ComparisonIndex directly, but it should be stored at the start of the struct
		Value.SetAddressToMonitor(&NativeValue, sizeof(FNameEntryId));
		Value.Value = TEXT("n\"") + NativeValue.ToString() + TEXT("\"");

		return true;
	}

	bool GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const override
	{
		if (((FName*)Address)->GetNumber() == 0)
		{
			OutString = ((FName*)Address)->GetPlainNameString();
			return true;
		}
		else
		{
			return false;
		}
	}

	bool FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const
	{
		new(BufferPtr) FName(*InString);
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
		FName& A = Usage.ResolvePrimitive<FName>(Value);
		FName& B = Usage.ResolvePrimitive<FName>(OtherValue);
		return A.Compare(B);
	}
};

namespace
{
	FName ScriptNameNone(NAME_None);

	void BindFNameType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FName>("FName", Flags);
	}

	void BindFNameInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FNameType>());
		FToStringHelper::Register(Binds, TEXT("FName"), &FAngelscriptFNameBinds::AppendToString, true);
	}

	void BindFNameFunctions(FAngelscriptBinds& Binds)
	{
		auto FName_ = Binds.ExistingClassForTarget("FName");

		FName_.Constructor(
			"void f()",
			&FAngelscriptFNameBinds::ConstructDefault,
			"FName",
			true)
			.NoDiscard();

		FName_.Constructor(
			"void f(const FName& Other)",
			&FAngelscriptFNameBinds::ConstructCopy,
			"FName",
			true)
			.NoDiscard();

		FName_.Constructor("void f(const FString& Other)", &FAngelscriptFNameBinds::ConstructFromString).NoDiscard();

		FName_.Method("FName& opAssign(const FName& Other)", METHODPR_TRIVIAL(FName&, FName, operator=, (const FName&)));
		FName_.Method("bool opEquals(const FName& Other) const", FUNC_TRIVIAL(FStaticJITHelperFunctions::FName_Equals));
		FName_.Method("int32 Compare(const FName& Other) const", METHODPR_TRIVIAL(int32, FName, Compare, (const FName&) const));

		FName_.Method("bool IsNone() const", METHODPR_TRIVIAL(bool, FName, IsNone, () const));
		FName_.Method("int32 GetNumber() const", METHODPR_TRIVIAL(int32, FName, GetNumber, () const));
		FName_.Method("void SetNumber(int32 NewNumber)", METHODPR_TRIVIAL(void, FName, SetNumber, (const int32)));

		FName_.Method("FString GetPlainNameString() const", METHODPR_TRIVIAL(FString, FName, GetPlainNameString, () const));

		FName_.Method(
			"bool IsEqual(const FName& Other, bool bIgnoreCase = true, bool bCompareNumber = true) const",
			&FAngelscriptFNameBinds::IsEqual);

		FName_.Method("uint GetHash() const", &FAngelscriptFNameBinds::GetHash);

		Binds.BindGlobalVariableForTarget("const FName NAME_None", &ScriptNameNone);

		auto FString_ = Binds.ExistingClassForTarget("FString");
		FString_.Method("FString opAdd_r(const FName& Value) const", &FAngelscriptFNameBinds::PrefixName);
		FString_.Method("FString& opAddAssign_r(const FName& Value) const", &FAngelscriptFNameBinds::PrefixNameAssign);

		FName_.Method("bool opEquals(const FString& Other) const", &FAngelscriptFNameBinds::EqualsString);

		Binds.BindGlobalFunctionForTarget(
			"const FName& __STATIC_NAME(int Id) no_discard",
			FUNC_TRIVIAL(FAngelscriptEngine::GetStaticName));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FName_Type(
	TEXT("FName.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFNameType);

AS_FORCE_LINK const FAngelscriptBind Bind_FName_Infrastructure(
	TEXT("FName.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFNameInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FName(
	TEXT("FName.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFNameFunctions);
