#include "Bind_FName.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "StaticJIT/StaticJITHelperFunctions.h"

#include "Helper_ToString.h"

/**
 * FName construction, comparison, numeric suffixes, string operators, hashing, constants, and formatting.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Name();                                                                                        | Constructs NAME_None.                                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Name(const FName& Other);                                                                      | Copy-constructs a name.                                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Name(const FString& Other);                                                                    | Interns a string as an FName.                                                                                    |
 * |                                                                                                      | @param Other Text containing the plain name and optional numeric suffix.                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Left = Right;                                                                                        | Assigns a name.                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Left == Right;                                                                         | Compares two names using FName identity.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Order = Name.Compare(const FName& Other) const;                                                | Returns negative, zero, or positive lexical/name-number ordering.                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bNone = Name.IsNone() const;                                                                    | Reports whether the value is NAME_None.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Number = Name.GetNumber() const;                                                               | Returns the stored numeric suffix component.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Name.SetNumber(int32 NewNumber);                                                                     | Replaces the stored numeric suffix component.                                                                    |
 * |                                                                                                      | @param NewNumber Internal FName number value.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Plain = Name.GetPlainNameString() const;                                                     | Returns the name text without its numeric suffix.                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Name.IsEqual(const FName& Other, bool bIgnoreCase = true,                              | Compares names with explicit case and numeric-suffix policy.                                                     |
 * |     bool bCompareNumber = true) const;                                                               | @param bIgnoreCase Performs case-insensitive comparison when true.                                               |
 * |                                                                                                      | @param bCompareNumber Includes the numeric suffix when true.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint Hash = Name.GetHash() const;                                                                    | Returns the hash used for name-keyed containers.                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Combined = Name + String;                                                                    | Prefixes a string with the name's text.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Name += String;                                                                                      | Prefixes the string operand in place with the name's text.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = Name == String;                                                                        | Compares a name with string text.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const FName NAME_None;                                                                               | Exposes the canonical empty name constant.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const FName& Name = __STATIC_NAME(int Id);                                                           | Returns an engine-interned static name by compiler-assigned identifier.                                          |
 * |                                                                                                      | @param Id Static-name table index emitted by the script compiler.                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{Name}";                                                                            | Formats the name through the shared formatter.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	FName ScriptNameNone(NAME_None);



}

AS_FORCE_LINK const FAngelscriptBind Bind_FName_Type(
	TEXT("FName.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FName>("FName", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FName_Infrastructure(
	TEXT("FName.Infrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FNameType>());
		FToStringHelper::Register(Binds, TEXT("FName"), &FAngelscriptFNameBinds::AppendToString, true);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FName(
	TEXT("FName.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
