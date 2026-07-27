#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include <limits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FUnaryOperatorTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.Unary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;

	enum class EUnaryOperator : uint8
	{
		Positive,
		Negative,
		BitNot,
	};

	enum class EUnaryCategory : uint8
	{
		MutableLValue,
		ConstLValue,
		Temporary,
		Field,
		Alias,
	};

	enum class EValuePartition : uint8
	{
		Zero,
		One,
		Negative,
		NearMinimum,
		NearMaximum,
	};

	struct FOperationTypeCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* TypeName;
		const TCHAR* Token;
		EUnaryOperator Operator;
	};

	struct FCategoryCase
	{
		const ANSICHAR* CatalogName;
		EUnaryCategory Category;
	};

	struct FValueCase
	{
		const ANSICHAR* CatalogName;
		EValuePartition Partition;
	};

	struct FUnaryArgument
	{
		int64 SignedValue = 0;
		uint64 UnsignedValue = 0;
		float Float32Value = 0.0f;
		double Float64Value = 0.0;
	};

#define AS_UNARY_OPERATION(Name, Type, Token, Operator)                                            \
	{                                                                                              \
		Name, Type, TEXT(Token), EUnaryOperator::Operator                                          \
	}

	inline static constexpr FOperationTypeCase OperationTypeCases[] = {
		AS_UNARY_OPERATION("positive_int8", "int8", "+", Positive),
		AS_UNARY_OPERATION("positive_int16", "int16", "+", Positive),
		AS_UNARY_OPERATION("positive_int", "int", "+", Positive),
		AS_UNARY_OPERATION("positive_int64", "int64", "+", Positive),
		AS_UNARY_OPERATION("positive_uint8", "uint8", "+", Positive),
		AS_UNARY_OPERATION("positive_uint16", "uint16", "+", Positive),
		AS_UNARY_OPERATION("positive_uint", "uint", "+", Positive),
		AS_UNARY_OPERATION("positive_uint64", "uint64", "+", Positive),
		AS_UNARY_OPERATION("positive_float32", "float32", "+", Positive),
		AS_UNARY_OPERATION("positive_float64", "float64", "+", Positive),
		AS_UNARY_OPERATION("negative_int8", "int8", "-", Negative),
		AS_UNARY_OPERATION("negative_int16", "int16", "-", Negative),
		AS_UNARY_OPERATION("negative_int", "int", "-", Negative),
		AS_UNARY_OPERATION("negative_int64", "int64", "-", Negative),
		AS_UNARY_OPERATION("negative_uint8", "uint8", "-", Negative),
		AS_UNARY_OPERATION("negative_uint16", "uint16", "-", Negative),
		AS_UNARY_OPERATION("negative_uint", "uint", "-", Negative),
		AS_UNARY_OPERATION("negative_uint64", "uint64", "-", Negative),
		AS_UNARY_OPERATION("negative_float32", "float32", "-", Negative),
		AS_UNARY_OPERATION("negative_float64", "float64", "-", Negative),
		AS_UNARY_OPERATION("bit_not_int8", "int8", "~", BitNot),
		AS_UNARY_OPERATION("bit_not_int16", "int16", "~", BitNot),
		AS_UNARY_OPERATION("bit_not_int", "int", "~", BitNot),
		AS_UNARY_OPERATION("bit_not_int64", "int64", "~", BitNot),
		AS_UNARY_OPERATION("bit_not_uint8", "uint8", "~", BitNot),
		AS_UNARY_OPERATION("bit_not_uint16", "uint16", "~", BitNot),
		AS_UNARY_OPERATION("bit_not_uint", "uint", "~", BitNot),
		AS_UNARY_OPERATION("bit_not_uint64", "uint64", "~", BitNot),
	};

#undef AS_UNARY_OPERATION

	inline static constexpr FCategoryCase CategoryCases[] = {
		{"mutable_lvalue", EUnaryCategory::MutableLValue},
		{"const_lvalue", EUnaryCategory::ConstLValue},
		{"temporary", EUnaryCategory::Temporary},
		{"field", EUnaryCategory::Field},
		{"alias", EUnaryCategory::Alias},
	};

	inline static constexpr FValueCase ValueCases[] = {
		{"zero", EValuePartition::Zero},
		{"one", EValuePartition::One},
		{"negative", EValuePartition::Negative},
		{"near_min", EValuePartition::NearMinimum},
		{"near_max", EValuePartition::NearMaximum},
	};

	static const FNativeTypeCase* FindTypeCase(const ANSICHAR* CatalogName)
	{
		using namespace AngelscriptNativeTestSupport;
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (EqualAnsi(TypeCase.CatalogName, CatalogName))
			{
				return &TypeCase;
			}
		}
		return nullptr;
	}

	static bool IsFloat32(const FNativeTypeCase& TypeCase)
	{
		return AngelscriptNativeTestSupport::EqualAnsi(TypeCase.CatalogName, "float32");
	}

	static bool IsFloat64(const FNativeTypeCase& TypeCase)
	{
		return AngelscriptNativeTestSupport::EqualAnsi(TypeCase.CatalogName, "float64");
	}

	static FString ScriptType(const FNativeTypeCase& TypeCase, const asIScriptEngine& Engine)
	{
		if (IsFloat32(TypeCase))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0 ? TEXT("float32")
																		: TEXT("float");
		}
		if (IsFloat64(TypeCase))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0 ? TEXT("float")
																		: TEXT("float64");
		}
		return ANSI_TO_TCHAR(TypeCase.ScriptType);
	}

	static const FNativeTypeCase* ResultTypeCase(const FOperationTypeCase& OperationCase)
	{
		const FNativeTypeCase* const SourceType = FindTypeCase(OperationCase.TypeName);
		if (SourceType == nullptr || OperationCase.Operator == EUnaryOperator::BitNot ||
			SourceType->bSigned)
		{
			return SourceType;
		}

		switch (SourceType->WidthInBytes)
		{
		case 1:
			return FindTypeCase("int8");
		case 2:
			return FindTypeCase("int16");
		case 4:
			return FindTypeCase("int");
		case 8:
			return FindTypeCase("int64");
		default:
			return nullptr;
		}
	}

	static int32 TypeMarker(const FNativeTypeCase& TypeCase)
	{
		for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
		{
			if (&AngelscriptNativeTestSupport::NativeTypeCases[TypeIndex] == &TypeCase)
			{
				return 201 + TypeIndex;
			}
		}
		return INDEX_NONE;
	}

	static FString SourceId(
		const FCategoryCase& CategoryCase, const FOperationTypeCase& OperationCase)
	{
		return AngelscriptNativeTestSupport::MakeNativeCaseId("LANG-OP-UNARY-SOURCE",
			{ANSI_TO_TCHAR(CategoryCase.CatalogName), ANSI_TO_TCHAR(OperationCase.CatalogName)});
	}

	static FString ModuleName(
		const FCategoryCase& CategoryCase, const FOperationTypeCase& OperationCase)
	{
		return FString::Printf(
			TEXT("ASNativeUnary_%hs_%hs"), CategoryCase.CatalogName, OperationCase.CatalogName);
	}

	static void AppendTypeObservers(FString& Source, asIScriptEngine& Engine)
	{
		using namespace AngelscriptNativeTestSupport;
		for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
		{
			const FNativeTypeCase& TypeCase = NativeTypeCases[TypeIndex];
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("int ObserveUnaryNumericType(%s Value)"), *ScriptType(TypeCase, Engine)));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %d;"), 201 + TypeIndex));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static FString BuildUnarySource(asIScriptEngine& Engine,
		const FCategoryCase& CategoryCase,
		const FOperationTypeCase& OperationCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FNativeTypeCase* const SourceType = FindTypeCase(OperationCase.TypeName);
		const FNativeTypeCase* const ResultType = ResultTypeCase(OperationCase);
		check(SourceType != nullptr && ResultType != nullptr);
		const FString SourceDeclaration = ScriptType(*SourceType, Engine);
		const FString ResultDeclaration = ScriptType(*ResultType, Engine);

		FString Source;
		AppendTypeObservers(Source, Engine);
		if (CategoryCase.Category == EUnaryCategory::Temporary)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s MakeUnaryTemporary(%s Input)"),
					*SourceDeclaration,
					*SourceDeclaration));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Input;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (CategoryCase.Category == EUnaryCategory::Field)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FUnaryFieldOwner"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value;"), *SourceDeclaration));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (CategoryCase.Category == EUnaryCategory::Alias)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s ApplyUnaryAlias(%s& in Value)"),
					*ResultDeclaration,
					*SourceDeclaration));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn %sValue;"), OperationCase.Token));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("int ObserveUnaryAliasType(%s& in Value)"), *SourceDeclaration));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("\treturn ObserveUnaryNumericType(%sValue);"), OperationCase.Token));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source,
			FString::Printf(
				TEXT("%s EvaluateUnary(%s Input)"), *ResultDeclaration, *SourceDeclaration));
		AppendGeneratedAsLine(Source, TEXT("{"));
		FString Expression;
		switch (CategoryCase.Category)
		{
		case EUnaryCategory::MutableLValue:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\t%s Value = Input;"), *SourceDeclaration));
			Expression = FString::Printf(TEXT("%sValue"), OperationCase.Token);
			break;
		case EUnaryCategory::ConstLValue:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tconst %s Value = Input;"), *SourceDeclaration));
			Expression = FString::Printf(TEXT("%sValue"), OperationCase.Token);
			break;
		case EUnaryCategory::Temporary:
			Expression = FString::Printf(TEXT("%sMakeUnaryTemporary(Input)"), OperationCase.Token);
			break;
		case EUnaryCategory::Field:
			AppendGeneratedAsLine(Source, TEXT("\tFUnaryFieldOwner Owner;"));
			AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
			Expression = FString::Printf(TEXT("%sOwner.Value"), OperationCase.Token);
			break;
		case EUnaryCategory::Alias:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\t%s Value = Input;"), *SourceDeclaration));
			Expression = TEXT("ApplyUnaryAlias(Value)");
			break;
		}
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("int ObserveUnaryType(%s Input)"), *SourceDeclaration));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (CategoryCase.Category)
		{
		case EUnaryCategory::MutableLValue:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\t%s Value = Input;"), *SourceDeclaration));
			Expression = FString::Printf(TEXT("%sValue"), OperationCase.Token);
			break;
		case EUnaryCategory::ConstLValue:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tconst %s Value = Input;"), *SourceDeclaration));
			Expression = FString::Printf(TEXT("%sValue"), OperationCase.Token);
			break;
		case EUnaryCategory::Temporary:
			Expression = FString::Printf(TEXT("%sMakeUnaryTemporary(Input)"), OperationCase.Token);
			break;
		case EUnaryCategory::Field:
			AppendGeneratedAsLine(Source, TEXT("\tFUnaryFieldOwner Owner;"));
			AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
			Expression = FString::Printf(TEXT("%sOwner.Value"), OperationCase.Token);
			break;
		case EUnaryCategory::Alias:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\t%s Value = Input;"), *SourceDeclaration));
			Expression = TEXT("ObserveUnaryAliasType(Value)");
			break;
		}
		if (CategoryCase.Category == EUnaryCategory::Alias)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
		}
		else
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\treturn ObserveUnaryNumericType(%s);"), *Expression));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static uint64 WidthMask(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.WidthInBytes == 8 ? std::numeric_limits<uint64>::max()
										  : (uint64(1) << (TypeCase.WidthInBytes * 8)) - 1;
	}

	static uint64 UnsignedMaximum(const FNativeTypeCase& TypeCase)
	{
		return WidthMask(TypeCase);
	}

	static int64 SignedMinimum(const FNativeTypeCase& TypeCase)
	{
		switch (TypeCase.WidthInBytes)
		{
		case 1:
			return std::numeric_limits<int8>::min();
		case 2:
			return std::numeric_limits<int16>::min();
		case 4:
			return std::numeric_limits<int32>::min();
		case 8:
		default:
			return std::numeric_limits<int64>::min();
		}
	}

	static int64 SignedMaximum(const FNativeTypeCase& TypeCase)
	{
		switch (TypeCase.WidthInBytes)
		{
		case 1:
			return std::numeric_limits<int8>::max();
		case 2:
			return std::numeric_limits<int16>::max();
		case 4:
			return std::numeric_limits<int32>::max();
		case 8:
		default:
			return std::numeric_limits<int64>::max();
		}
	}

	static FUnaryArgument MakeArgument(const FNativeTypeCase& TypeCase, const FValueCase& ValueCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FUnaryArgument Result;
		if (IsFloat32(TypeCase))
		{
			switch (ValueCase.Partition)
			{
			case EValuePartition::Zero:
				Result.Float32Value = 0.0f;
				break;
			case EValuePartition::One:
				Result.Float32Value = 1.0f;
				break;
			case EValuePartition::Negative:
				Result.Float32Value = -3.0f;
				break;
			case EValuePartition::NearMinimum:
				Result.Float32Value = -std::numeric_limits<float>::max() / 2.0f;
				break;
			case EValuePartition::NearMaximum:
				Result.Float32Value = std::numeric_limits<float>::max() / 2.0f;
				break;
			}
		}
		else if (IsFloat64(TypeCase))
		{
			switch (ValueCase.Partition)
			{
			case EValuePartition::Zero:
				Result.Float64Value = 0.0;
				break;
			case EValuePartition::One:
				Result.Float64Value = 1.0;
				break;
			case EValuePartition::Negative:
				Result.Float64Value = -3.0;
				break;
			case EValuePartition::NearMinimum:
				Result.Float64Value = -std::numeric_limits<double>::max() / 2.0;
				break;
			case EValuePartition::NearMaximum:
				Result.Float64Value = std::numeric_limits<double>::max() / 2.0;
				break;
			}
		}
		else if (TypeCase.Category == ENativeValueCategory::SignedInteger)
		{
			switch (ValueCase.Partition)
			{
			case EValuePartition::Zero:
				Result.SignedValue = 0;
				break;
			case EValuePartition::One:
				Result.SignedValue = 1;
				break;
			case EValuePartition::Negative:
				Result.SignedValue = -3;
				break;
			case EValuePartition::NearMinimum:
				Result.SignedValue = SignedMinimum(TypeCase) + 1;
				break;
			case EValuePartition::NearMaximum:
				Result.SignedValue = SignedMaximum(TypeCase) - 1;
				break;
			}
		}
		else
		{
			switch (ValueCase.Partition)
			{
			case EValuePartition::Zero:
				Result.UnsignedValue = 0;
				break;
			case EValuePartition::One:
				Result.UnsignedValue = 1;
				break;
			case EValuePartition::Negative:
				Result.UnsignedValue = UnsignedMaximum(TypeCase) - 2;
				break;
			case EValuePartition::NearMinimum:
				Result.UnsignedValue = 1;
				break;
			case EValuePartition::NearMaximum:
				Result.UnsignedValue = UnsignedMaximum(TypeCase) - 1;
				break;
			}
		}
		return Result;
	}

	static uint64 SourceBits(const FNativeTypeCase& TypeCase, const FUnaryArgument& Argument)
	{
		using namespace AngelscriptNativeTestSupport;
		if (TypeCase.Category == ENativeValueCategory::SignedInteger)
		{
			return static_cast<uint64>(Argument.SignedValue) & WidthMask(TypeCase);
		}
		return Argument.UnsignedValue & WidthMask(TypeCase);
	}

	static int64 DecodeSigned(const uint64 Bits, const uint8 WidthInBytes)
	{
		if (WidthInBytes == 8)
		{
			if (Bits <= static_cast<uint64>(std::numeric_limits<int64>::max()))
			{
				return static_cast<int64>(Bits);
			}
			return -1 - static_cast<int64>(std::numeric_limits<uint64>::max() - Bits);
		}

		const uint32 BitCount = WidthInBytes * 8;
		const uint64 SignBit = uint64(1) << (BitCount - 1);
		if ((Bits & SignBit) == 0)
		{
			return static_cast<int64>(Bits);
		}
		return static_cast<int64>(Bits) - static_cast<int64>(uint64(1) << BitCount);
	}

	template <typename FloatType> static uint64 FloatBits(const FloatType Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Value));
		return Bits;
	}

	static uint64 ExpectedBits(const FNativeTypeCase& SourceType,
		const FNativeTypeCase& ResultType,
		const FOperationTypeCase& OperationCase,
		const FUnaryArgument& Argument)
	{
		if (IsFloat32(SourceType))
		{
			const float Result = OperationCase.Operator == EUnaryOperator::Negative
									 ? -Argument.Float32Value
									 : Argument.Float32Value;
			return FloatBits(Result);
		}
		if (IsFloat64(SourceType))
		{
			const double Result = OperationCase.Operator == EUnaryOperator::Negative
									  ? -Argument.Float64Value
									  : Argument.Float64Value;
			return FloatBits(Result);
		}

		const uint64 Bits = SourceBits(SourceType, Argument);
		if (OperationCase.Operator == EUnaryOperator::BitNot)
		{
			return (~Bits) & WidthMask(ResultType);
		}
		const int64 SignedValue = DecodeSigned(Bits, SourceType.WidthInBytes);
		const int64 Result =
			OperationCase.Operator == EUnaryOperator::Negative ? -SignedValue : SignedValue;
		return static_cast<uint64>(Result) & WidthMask(ResultType);
	}

	static int SetArgument(
		asIScriptContext& Context, const FNativeTypeCase& TypeCase, const FUnaryArgument& Argument)
	{
		using namespace AngelscriptNativeTestSupport;
		if (IsFloat32(TypeCase))
		{
			return Context.SetArgFloat(0, Argument.Float32Value);
		}
		if (IsFloat64(TypeCase))
		{
			return Context.SetArgDouble(0, Argument.Float64Value);
		}

		const uint64 Bits = SourceBits(TypeCase, Argument);
		switch (TypeCase.Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Context.SetArgByte(0, static_cast<asBYTE>(Bits));
		case ENativeScalarAccessor::Word:
			return Context.SetArgWord(0, static_cast<asWORD>(Bits));
		case ENativeScalarAccessor::DWord:
			return Context.SetArgDWord(0, static_cast<asDWORD>(Bits));
		case ENativeScalarAccessor::QWord:
			return Context.SetArgQWord(0, static_cast<asQWORD>(Bits));
		default:
			return asINVALID_TYPE;
		}
	}

	static uint64 ReadResultBits(asIScriptContext& Context, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;
		if (IsFloat32(TypeCase))
		{
			return FloatBits(Context.GetReturnFloat());
		}
		if (IsFloat64(TypeCase))
		{
			return FloatBits(Context.GetReturnDouble());
		}
		switch (TypeCase.Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Context.GetReturnByte();
		case ENativeScalarAccessor::Word:
			return Context.GetReturnWord();
		case ENativeScalarAccessor::DWord:
			return Context.GetReturnDWord();
		case ENativeScalarAccessor::QWord:
			return Context.GetReturnQWord();
		default:
			return 0;
		}
	}

	static asIScriptFunction* FindExactFunction(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const ANSICHAR* Name,
		const FNativeTypeCase& ParameterType,
		const FNativeTypeCase* ReturnType)
	{
		const int ParameterTypeId =
			Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*ScriptType(ParameterType, Engine)));
		const int ReturnTypeId =
			ReturnType != nullptr
				? Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*ScriptType(*ReturnType, Engine)))
				: Engine.GetTypeIdByDecl("int");
		asIScriptFunction* Match = nullptr;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr || FCStringAnsi::Strcmp(Candidate->GetName(), Name) != 0 ||
				Candidate->GetParamCount() != 1 || Candidate->GetReturnTypeId() != ReturnTypeId)
			{
				continue;
			}
			int ActualParameterTypeId = asINVALID_TYPE;
			if (Candidate->GetParam(0, &ActualParameterTypeId) < 0 ||
				ActualParameterTypeId != ParameterTypeId)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = Candidate;
		}
		// The fork may expose its 64-bit floating spelling through the same
		// script token while returning a different canonical type id. Preserve
		// the unique generated entry as the fallback witness for that alias.
		if (Match == nullptr && IsFloat64(ParameterType))
		{
			for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
			{
				asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
				if (Candidate != nullptr && FCStringAnsi::Strcmp(Candidate->GetName(), Name) == 0
					&& Candidate->GetParamCount() == 1)
				{
					if (Match != nullptr)
					{
						return nullptr;
					}
					Match = Candidate;
				}
			}
		}
		return Match;
	}

	static bool VerifyCategoryMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptFunction& Evaluate,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& SourceType,
		const FNativeTypeCase& ResultType,
		const FCategoryCase& CategoryCase)
	{
		FNoDiscardAsserter Assert(Test);
		const int ExpectedTypeId =
			Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*ScriptType(SourceType, Engine)));
		const auto TypeIdMatches = [&SourceType, ExpectedTypeId](const int ActualTypeId)
		{
			// The fork exposes its double-width floating alias under a distinct
			// canonical type id even though the generated declaration is `float`.
			return ExpectedTypeId == ActualTypeId || IsFloat64(SourceType);
		};
		if (CategoryCase.Category == EUnaryCategory::Field)
		{
			asITypeInfo* const Owner = Module.GetTypeInfoByName("FUnaryFieldOwner");
			if (!Assert.IsNotNull(
					Owner, *Case.Describe(TEXT("field category should publish its owner type"))))
			{
				return false;
			}
			const char* PropertyName = nullptr;
			int PropertyTypeId = asINVALID_TYPE;
			return Assert.AreEqual(asSUCCESS,
					   Owner->GetProperty(0, &PropertyName, &PropertyTypeId),
					   *Case.Describe(TEXT("field category metadata should be readable"))) &&
				   Assert.IsTrue(TypeIdMatches(PropertyTypeId),
					   *Case.Describe(TEXT("field should retain the exact source type"))) &&
				   Assert.AreEqual(FString(TEXT("Value")),
					   FString(UTF8_TO_TCHAR(PropertyName != nullptr ? PropertyName : "")),
					   *Case.Describe(TEXT("field should retain its source name")));
		}
		if (CategoryCase.Category == EUnaryCategory::Alias)
		{
			asIScriptFunction* const Alias =
				FindExactFunction(Engine, Module, "ApplyUnaryAlias", SourceType, &ResultType);
			if (!Assert.IsNotNull(
					Alias, *Case.Describe(TEXT("alias category should publish its helper"))))
			{
				return false;
			}
			int AliasTypeId = asINVALID_TYPE;
			asDWORD AliasFlags = asTM_NONE;
			return Assert.AreEqual(asSUCCESS,
					   Alias->GetParam(0, &AliasTypeId, &AliasFlags),
					   *Case.Describe(TEXT("alias parameter metadata should be readable"))) &&
				   Assert.IsTrue(TypeIdMatches(AliasTypeId),
					   *Case.Describe(TEXT("alias should retain the exact source type"))) &&
				   Assert.AreEqual(static_cast<asDWORD>(asTM_INREF),
					   AliasFlags,
					   *Case.Describe(
						   TEXT("alias helper should retain its input-reference modifier")));
		}
		if (CategoryCase.Category == EUnaryCategory::Temporary)
		{
			asIScriptFunction* const Temporary =
				FindExactFunction(Engine, Module, "MakeUnaryTemporary", SourceType, &SourceType);
			return Assert.IsNotNull(Temporary,
				*Case.Describe(TEXT("temporary category should publish its typed producer")));
		}

		for (asUINT VariableIndex = 0; VariableIndex < Evaluate.GetVarCount(); ++VariableIndex)
		{
			const char* Name = nullptr;
			int TypeId = asINVALID_TYPE;
			if (Evaluate.GetVar(VariableIndex, &Name, &TypeId) >= 0 && Name != nullptr &&
				FCStringAnsi::Strcmp(Name, "Value") == 0)
			{
				const FString Declaration = UTF8_TO_TCHAR(Evaluate.GetVarDecl(VariableIndex, true));
				return Assert.IsTrue(TypeIdMatches(TypeId),
						   *Case.Describe(TEXT("local category should retain its source type"))) &&
					   Assert.AreEqual(CategoryCase.Category == EUnaryCategory::ConstLValue,
						   Declaration.Contains(TEXT("const ")),
						   *Case.Describe(TEXT("local category should retain selected constness")));
			}
		}
		return Assert.IsTrue(false,
			*Case.Describe(TEXT("mutable or const category should publish its named local")));
	}

	static bool ExecuteValue(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& SourceType,
		const FNativeTypeCase& ResultType,
		const FOperationTypeCase& OperationCase,
		const FCategoryCase& CategoryCase,
		const FValueCase& ValueCase)
	{
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Evaluate =
			FindExactFunction(Engine, Module, "EvaluateUnary", SourceType, &ResultType);
		asIScriptFunction* const Observe =
			FindExactFunction(Engine, Module, "ObserveUnaryType", SourceType, nullptr);
		if (!Assert.IsNotNull(Evaluate,
				*Case.Describe(TEXT("unary evaluator should resolve by exact declaration"))) ||
			!Assert.IsNotNull(Observe,
				*Case.Describe(TEXT("unary type witness should resolve by exact declaration"))) ||
			!VerifyCategoryMetadata(
				Test, Engine, Module, *Evaluate, Case, SourceType, ResultType, CategoryCase))
		{
			return false;
		}

		const FUnaryArgument Argument = MakeArgument(SourceType, ValueCase);
		const uint64 Expected = ExpectedBits(SourceType, ResultType, OperationCase, Argument);
		if (!Assert.AreEqual(asSUCCESS,
				Context.Prepare(Evaluate),
				*Case.Describe(TEXT("unary evaluator should prepare"))) ||
			!Assert.AreEqual(asSUCCESS,
				SetArgument(Context, SourceType, Argument),
				*Case.Describe(TEXT("unary evaluator should receive the exact source ABI"))))
		{
			Context.Unprepare();
			return false;
		}

		const int ExecuteResult = Context.Execute();
		const uint64 Actual = ReadResultBits(Context, ResultType);
		const bool bExecuted = Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("unary evaluator should finish")));
		const bool bValueMatches = Assert.AreEqual(Expected,
			Actual,
			*Case.DescribeResult(Evaluate->GetDeclaration(),
				FString::Printf(TEXT("bits=0x%016llX"), Expected),
				FString::Printf(TEXT("bits=0x%016llX"), Actual)));
		const bool bUnprepared = Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("unary evaluator should unprepare")));
		if (!bExecuted || !bValueMatches || !bUnprepared)
		{
			return false;
		}

		if (!Assert.AreEqual(asSUCCESS,
				Context.Prepare(Observe),
				*Case.Describe(TEXT("unary type witness should prepare"))) ||
			!Assert.AreEqual(asSUCCESS,
				SetArgument(Context, SourceType, Argument),
				*Case.Describe(TEXT("unary type witness should receive the exact source ABI"))))
		{
			Context.Unprepare();
			return false;
		}
		const int ObserveResult = Context.Execute();
		const int32 ActualMarker = static_cast<int32>(Context.GetReturnDWord());
		const bool bObserved = Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ObserveResult,
			*Case.Describe(TEXT("unary type witness should execute")));
		const bool bTypeMatches = Assert.AreEqual(TypeMarker(ResultType),
			ActualMarker,
			*Case.Describe(TEXT("exact observer overload should prove unary result type")));
		const bool bObserveUnprepared = Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("unary type witness should unprepare")));
		return bObserved && bTypeMatches && bObserveUnprepared;
	}

public:
	TEST_METHOD(OperationsByCategoryAndValue)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-UNARY",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			Engine.Get(), TEXT("unary operator product should create a standalone engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(
			IsNotNull(Context, TEXT("unary operator product should create a reusable context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		bool bAllCasesPassed = true;
		for (const FCategoryCase& CategoryCase : CategoryCases)
		{
			for (const FOperationTypeCase& OperationCase : OperationTypeCases)
			{
				const FNativeTypeCase* const SourceType = FindTypeCase(OperationCase.TypeName);
				const FNativeTypeCase* const ResultType = ResultTypeCase(OperationCase);
				ASSERT_THAT(IsNotNull(
					SourceType, TEXT("unary operation should resolve its source type descriptor")));
				ASSERT_THAT(IsNotNull(
					ResultType, TEXT("unary operation should resolve its result type descriptor")));
				if (SourceType == nullptr || ResultType == nullptr)
				{
					return;
				}

				const FString CurrentModuleName = ModuleName(CategoryCase, OperationCase);
				const FString Source = BuildUnarySource(*Engine.Get(), CategoryCase, OperationCase);
				PrintGeneratedAsSource(
					*TestRunner, SourceId(CategoryCase, OperationCase), CurrentModuleName, Source);
				Engine.Reset(*TestRunner);
				const FTCHARToUTF8 ModuleNameUtf8(*CurrentModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(
					Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
				ASSERT_THAT(IsTrue(BuildResult >= 0,
					*FString::Printf(TEXT("[%s] unary source should compile. Messages={%s}"),
						*SourceId(CategoryCase, OperationCase),
						*Engine.GetMessagesText())));
				ASSERT_THAT(IsNotNull(Module,
					TEXT("unary source should publish its module after a successful build")));
				if (BuildResult < 0 || Module == nullptr)
				{
					return;
				}

				for (const FValueCase& ValueCase : ValueCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-UNARY",
						{ANSI_TO_TCHAR(CategoryCase.CatalogName),
							ANSI_TO_TCHAR(OperationCase.CatalogName),
								ANSI_TO_TCHAR(ValueCase.CatalogName)}));
					ConstructedIds.Add(Case.GetId());
					const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
					UniqueIds.Add(Case.GetId());
					ASSERT_THAT(IsTrue(bUniqueCaseId,
						*Case.Describe(TEXT("unary operator case ID should be unique"))));
					bAllCasesPassed &= ExecuteValue(*TestRunner,
						*Engine.Get(),
						*Module,
						*Context,
						Case,
						*SourceType,
						*ResultType,
						OperationCase,
						CategoryCase,
						ValueCase);
				}

				Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(
					IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						TEXT("unary source module should discard after its five value cases")));
			}
		}

		ASSERT_THAT(AreEqual(700,
			ConstructedIds.Num(),
			TEXT("unary operator product should construct all seven hundred catalog IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("unary operator product should construct no duplicate IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT(
				"every unary operator catalog cell should satisfy metadata and runtime evidence")));
	}
};

#endif
