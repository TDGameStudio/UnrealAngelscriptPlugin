#pragma once

#include "CoreMinimal.h"

#include <initializer_list>

namespace AngelscriptNativeTestSupport
{
	enum class ENativeValueCategory : uint8
	{
		SignedInteger,
		UnsignedInteger,
		FloatingPoint,
		Boolean,
		Enum,
		Typedef,
		ScriptValue,
		NativeValue,
		ScriptReference,
		NativeReference,
		Null,
	};

	enum class ENativeScalarAccessor : uint8
	{
		None,
		Byte,
		Word,
		DWord,
		QWord,
		Float,
		Double,
		Address,
		Object,
	};

	struct FNativeTypeCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
		ENativeValueCategory Category;
		ENativeScalarAccessor Accessor;
		uint8 WidthInBytes;
		bool bSigned;
		const ANSICHAR* ZeroLiteral;
		const ANSICHAR* OneLiteral;
		const ANSICHAR* MinimumLiteral;
		const ANSICHAR* MaximumLiteral;
		const ANSICHAR* NearBoundaryLiteral;
	};

#define AS_NATIVE_TYPE_CASE(CatalogName, ScriptType, Category, Accessor, Width, Signed, Zero, One, Minimum, Maximum, NearBoundary) \
	{ CatalogName, ScriptType, ENativeValueCategory::Category, ENativeScalarAccessor::Accessor, Width, Signed, Zero, One, Minimum, Maximum, NearBoundary }

	inline constexpr FNativeTypeCase NativeTypeCases[] =
	{
		AS_NATIVE_TYPE_CASE("int8", "int8", SignedInteger, Byte, 1, true, "0", "1", "-128", "127", "126"),
		AS_NATIVE_TYPE_CASE("int16", "int16", SignedInteger, Word, 2, true, "0", "1", "-32768", "32767", "32766"),
		AS_NATIVE_TYPE_CASE("int", "int", SignedInteger, DWord, 4, true, "0", "1", "-2147483647 - 1", "2147483647", "2147483646"),
		AS_NATIVE_TYPE_CASE("int64", "int64", SignedInteger, QWord, 8, true, "0", "1", "-9223372036854775807 - 1", "9223372036854775807", "9223372036854775806"),
		AS_NATIVE_TYPE_CASE("uint8", "uint8", UnsignedInteger, Byte, 1, false, "0", "1", "0", "255", "254"),
		AS_NATIVE_TYPE_CASE("uint16", "uint16", UnsignedInteger, Word, 2, false, "0", "1", "0", "65535", "65534"),
		AS_NATIVE_TYPE_CASE("uint", "uint", UnsignedInteger, DWord, 4, false, "0", "1", "0", "4294967295", "4294967294"),
		AS_NATIVE_TYPE_CASE("uint64", "uint64", UnsignedInteger, QWord, 8, false, "0", "1", "0", "18446744073709551615", "18446744073709551614"),
		AS_NATIVE_TYPE_CASE("float32", "float", FloatingPoint, Float, 4, true, "0.0f", "1.0f", "-3.402823466e+38f", "3.402823466e+38f", "3.402823263e+38f"),
		AS_NATIVE_TYPE_CASE("float64", "double", FloatingPoint, Double, 8, true, "0.0", "1.0", "-1.7976931348623157e+308", "1.7976931348623157e+308", "1.7976931348623155e+308"),
		AS_NATIVE_TYPE_CASE("bool", "bool", Boolean, Byte, 1, false, "false", "true", "false", "true", "true"),
		AS_NATIVE_TYPE_CASE("enum", "ENativeCaseEnum", Enum, DWord, 4, true, "ENativeCaseEnum::Zero", "ENativeCaseEnum::One", "ENativeCaseEnum::Minimum", "ENativeCaseEnum::Maximum", "ENativeCaseEnum::NearMaximum"),
		AS_NATIVE_TYPE_CASE("typedef", "NativeCaseAlias", Typedef, DWord, 4, true, "NativeCaseAlias(0)", "NativeCaseAlias(1)", "NativeCaseAlias(-2147483647 - 1)", "NativeCaseAlias(2147483647)", "NativeCaseAlias(2147483646)"),
		AS_NATIVE_TYPE_CASE("script_value", "FScriptCaseValue", ScriptValue, Object, 0, false, "FScriptCaseValue()", "FScriptCaseValue(1)", "FScriptCaseValue(-1)", "FScriptCaseValue(2147483647)", "FScriptCaseValue(2147483646)"),
		AS_NATIVE_TYPE_CASE("native_value", "FNativeCaseValue", NativeValue, Object, 0, false, "FNativeCaseValue()", "FNativeCaseValue(1)", "FNativeCaseValue(-1)", "FNativeCaseValue(2147483647)", "FNativeCaseValue(2147483646)"),
		AS_NATIVE_TYPE_CASE("script_reference", "FScriptCaseReference", ScriptReference, Object, 0, false, "nullptr", "FScriptCaseReference()", "nullptr", "FScriptCaseReference()", "FScriptCaseReference()"),
		AS_NATIVE_TYPE_CASE("native_reference", "FNativeCaseReference", NativeReference, Object, 0, false, "nullptr", "FNativeCaseReference()", "nullptr", "FNativeCaseReference()", "FNativeCaseReference()"),
		AS_NATIVE_TYPE_CASE("null", "auto", Null, Address, 0, false, "nullptr", "nullptr", "nullptr", "nullptr", "nullptr"),
	};

#undef AS_NATIVE_TYPE_CASE

	constexpr bool EqualAnsi(const ANSICHAR* Left, const ANSICHAR* Right)
	{
		if (Left == nullptr || Right == nullptr)
		{
			return Left == Right;
		}
		while (*Left != '\0' && *Right != '\0')
		{
			if (*Left++ != *Right++)
			{
				return false;
			}
		}
		return *Left == *Right;
	}

	template <uint32 Count>
	constexpr bool HasUniqueNativeTypeNames(const FNativeTypeCase (&Cases)[Count])
	{
		for (uint32 Left = 0; Left < Count; ++Left)
		{
			for (uint32 Right = Left + 1; Right < Count; ++Right)
			{
				if (EqualAnsi(Cases[Left].CatalogName, Cases[Right].CatalogName))
				{
					return false;
				}
			}
		}
		return true;
	}

	static_assert(UE_ARRAY_COUNT(NativeTypeCases) == 18, "Every native core type category must have exactly one case definition.");
	static_assert(HasUniqueNativeTypeNames(NativeTypeCases), "Native core type case names must be unique.");

	enum class ENativeEvidence : uint16
	{
		None = 0,
		Compile = 1 << 0,
		Diagnostic = 1 << 1,
		Runtime = 1 << 2,
		Metadata = 1 << 3,
		Bytecode = 1 << 4,
		Lifecycle = 1 << 5,
		Debug = 1 << 6,
		Cleanup = 1 << 7,
		Isolation = 1 << 8,
		SaveLoad = 1 << 9,
		Recovery = 1 << 10,
	};

	constexpr ENativeEvidence operator|(const ENativeEvidence Left, const ENativeEvidence Right)
	{
		return static_cast<ENativeEvidence>(static_cast<uint16>(Left) | static_cast<uint16>(Right));
	}

	constexpr bool HasEvidence(const ENativeEvidence Value, const ENativeEvidence Required)
	{
		return (static_cast<uint16>(Value) & static_cast<uint16>(Required)) == static_cast<uint16>(Required);
	}

	struct FNativeProductContract
	{
		const ANSICHAR* ProductId = nullptr;
		ENativeEvidence Evidence = ENativeEvidence::None;

		bool IsValid() const
		{
			return ProductId != nullptr && ProductId[0] != '\0' && Evidence != ENativeEvidence::None;
		}
	};

	class FNativeCaseContext
	{
	public:
		explicit FNativeCaseContext(FString InCaseId)
			: CaseId(MoveTemp(InCaseId))
		{
		}

		const FString& GetId() const
		{
			return CaseId;
		}

		FString Describe(const TCHAR* Expectation) const
		{
			return FString::Printf(TEXT("[%s] %s"), *CaseId, Expectation != nullptr ? Expectation : TEXT("native SDK case failed"));
		}

		FString MakeModuleName(const TCHAR* Prefix) const
		{
			return FString::Printf(
				TEXT("%s_%s"),
				Prefix != nullptr ? Prefix : TEXT("NativeSdkCase"),
				*CaseId);
		}

		FString DescribeResult(
			const ANSICHAR* ExactDeclaration,
			const FString& Expected,
			const FString& Actual) const
		{
			return FString::Printf(
				TEXT("[%s] Declaration='%hs' Expected='%s' Actual='%s'"),
				*CaseId,
				ExactDeclaration != nullptr ? ExactDeclaration : "<none>",
				*Expected,
				*Actual);
		}

		FString DescribeResult(
			const TCHAR* ExactDeclaration,
			const FString& Expected,
			const FString& Actual) const
		{
			return FString::Printf(
				TEXT("[%s] Declaration='%s' Expected='%s' Actual='%s'"),
				*CaseId,
				ExactDeclaration != nullptr ? ExactDeclaration : TEXT("<none>"),
				*Expected,
				*Actual);
		}

	private:
		FString CaseId;
	};

	template <typename CaseType, uint32 Count, typename CallbackType>
	void ForEachNativeCase(const CaseType (&Cases)[Count], CallbackType&& Callback)
	{
		for (uint32 CaseIndex = 0; CaseIndex < Count; ++CaseIndex)
		{
			Callback(Cases[CaseIndex]);
		}
	}

	inline FString MakeNativeCaseId(const ANSICHAR* ProductId, std::initializer_list<const TCHAR*> AxisValues)
	{
		FString Result = UTF8_TO_TCHAR(ProductId != nullptr ? ProductId : "INVALID-PRODUCT");
		for (const TCHAR* AxisValue : AxisValues)
		{
			Result += TEXT("-");
			Result += AxisValue != nullptr ? AxisValue : TEXT("INVALID");
		}
		Result = Result.ToUpper();
		Result.ReplaceInline(TEXT("_"), TEXT("-"));
		return Result;
	}
}

#define AS_NATIVE_JOIN_INNER(Left, Right) Left##Right
#define AS_NATIVE_JOIN(Left, Right) AS_NATIVE_JOIN_INNER(Left, Right)
#define AS_NATIVE_PRODUCT(ProductIdLiteral, EvidenceExpression) \
	[[maybe_unused]] const AngelscriptNativeTestSupport::FNativeProductContract AS_NATIVE_JOIN(NativeProductContract_, __LINE__) \
	{ ProductIdLiteral, EvidenceExpression }
#define AS_NATIVE_PRODUCT_PART(ProductIdLiteral, ScenarioLiteral) \
	static_assert(sizeof(ProductIdLiteral) > 1 && sizeof(ScenarioLiteral) > 1, \
		"Native SDK product parts require a product ID and scenario")
#define AS_NATIVE_NON_PRODUCT(DispositionLiteral, RationaleLiteral) \
	static_assert(sizeof(DispositionLiteral) > 1 && sizeof(RationaleLiteral) > 1, \
		"Native SDK non-product dispositions require a state and rationale")
