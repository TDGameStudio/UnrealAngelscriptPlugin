#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include <cmath>
#include <limits>
#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNumericBinaryOperatorTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.NumericBinary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;

	enum class ENumericResultKind : uint8
	{
		Signed32,
		Unsigned32,
		Signed64,
		Unsigned64,
		Float32,
		Float64,
		Boolean,
	};

	enum class ENumericOperator : uint8
	{
		Add,
		Subtract,
		Multiply,
		Divide,
		Modulo,
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		Equal,
		NotEqual,
	};

	enum class EValuePartition : uint8
	{
		Zero,
		One,
		Negative,
		NearMinimum,
		NearMaximum,
	};

	struct FOperatorCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* Token;
		const TCHAR* FunctionName;
		const TCHAR* TypeFunctionName;
		ENumericOperator Operator;
		bool bComparison;
	};

	struct FValueCase
	{
		const ANSICHAR* CatalogName;
		EValuePartition Partition;
	};

	struct FNumericArgument
	{
		int64 SignedValue = 0;
		uint64 UnsignedValue = 0;
		float Float32Value = 0.0f;
		double Float64Value = 0.0;
	};

	struct FNumericResult
	{
		ENumericResultKind Kind = ENumericResultKind::Signed32;
		uint64 Bits = 0;
	};

	struct FCompiledPair
	{
		const FNativeTypeCase* LeftType = nullptr;
		const FNativeTypeCase* RightType = nullptr;
		FString ModuleName;
		asIScriptModule* Module = nullptr;
	};

	inline static constexpr FOperatorCase OperatorCases[] = {
		{"add",
			TEXT("+"),
			TEXT("EvaluateAdd"),
			TEXT("ObserveAddType"),
			ENumericOperator::Add,
			false},
		{"subtract",
			TEXT("-"),
			TEXT("EvaluateSubtract"),
			TEXT("ObserveSubtractType"),
			ENumericOperator::Subtract,
			false},
		{"multiply",
			TEXT("*"),
			TEXT("EvaluateMultiply"),
			TEXT("ObserveMultiplyType"),
			ENumericOperator::Multiply,
			false},
		{"divide",
			TEXT("/"),
			TEXT("EvaluateDivide"),
			TEXT("ObserveDivideType"),
			ENumericOperator::Divide,
			false},
		{"modulo",
			TEXT("%"),
			TEXT("EvaluateModulo"),
			TEXT("ObserveModuloType"),
			ENumericOperator::Modulo,
			false},
		{"less",
			TEXT("<"),
			TEXT("EvaluateLess"),
			TEXT("ObserveLessType"),
			ENumericOperator::Less,
			true},
		{"less_equal",
			TEXT("<="),
			TEXT("EvaluateLessEqual"),
			TEXT("ObserveLessEqualType"),
			ENumericOperator::LessEqual,
			true},
		{"greater",
			TEXT(">"),
			TEXT("EvaluateGreater"),
			TEXT("ObserveGreaterType"),
			ENumericOperator::Greater,
			true},
		{"greater_equal",
			TEXT(">="),
			TEXT("EvaluateGreaterEqual"),
			TEXT("ObserveGreaterEqualType"),
			ENumericOperator::GreaterEqual,
			true},
		{"equal",
			TEXT("=="),
			TEXT("EvaluateEqual"),
			TEXT("ObserveEqualType"),
			ENumericOperator::Equal,
			true},
		{"not_equal",
			TEXT("!="),
			TEXT("EvaluateNotEqual"),
			TEXT("ObserveNotEqualType"),
			ENumericOperator::NotEqual,
			true},
	};

	inline static constexpr FValueCase ValueCases[] = {
		{"zero", EValuePartition::Zero},
		{"one", EValuePartition::One},
		{"negative", EValuePartition::Negative},
		{"near_min", EValuePartition::NearMinimum},
		{"near_max", EValuePartition::NearMaximum},
	};

	static bool IsNumericType(const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;
		return TypeCase.Category == ENativeValueCategory::SignedInteger ||
			   TypeCase.Category == ENativeValueCategory::UnsignedInteger ||
			   TypeCase.Category == ENativeValueCategory::FloatingPoint;
	}

	static bool IsFloat32(const FNativeTypeCase& TypeCase)
	{
		return AngelscriptNativeTestSupport::EqualAnsi(TypeCase.CatalogName, "float32");
	}

	static bool IsFloat64(const FNativeTypeCase& TypeCase)
	{
		return AngelscriptNativeTestSupport::EqualAnsi(TypeCase.CatalogName, "float64");
	}

	static bool IsSignedInteger(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == AngelscriptNativeTestSupport::ENativeValueCategory::SignedInteger;
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

	static int PublicTypeId(asIScriptEngine& Engine, const FNativeTypeCase& TypeCase)
	{
		if (IsFloat32(TypeCase))
		{
			return Engine.GetTypeIdByDecl("float32");
		}
		if (IsFloat64(TypeCase))
		{
			return Engine.GetTypeIdByDecl("float64");
		}
		return Engine.GetTypeIdByDecl(TypeCase.ScriptType);
	}

	static const ANSICHAR* PublicResultTypeName(const ENumericResultKind Kind)
	{
		switch (Kind)
		{
		case ENumericResultKind::Float32:
			return "float32";
		case ENumericResultKind::Float64:
			return "float64";
		default:
			return nullptr;
		}
	}

	static int PublicResultTypeId(asIScriptEngine& Engine, const ENumericResultKind Kind)
	{
		if (const ANSICHAR* const PublicName = PublicResultTypeName(Kind))
		{
			return Engine.GetTypeIdByDecl(PublicName);
		}
		const FString Decl = ResultType(Kind, Engine);
		return Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*Decl));
	}

	static ENumericResultKind PromotedKind(
		const FNativeTypeCase& LeftType, const FNativeTypeCase& RightType)
	{
		using namespace AngelscriptNativeTestSupport;
		if (IsFloat64(LeftType) || IsFloat64(RightType))
		{
			return ENumericResultKind::Float64;
		}
		if (IsFloat32(LeftType) || IsFloat32(RightType))
		{
			return ENumericResultKind::Float32;
		}

		const bool bWide = LeftType.WidthInBytes == 8 || RightType.WidthInBytes == 8;
		const bool bSigned = LeftType.Category == ENativeValueCategory::SignedInteger ||
							 RightType.Category == ENativeValueCategory::SignedInteger;
		if (bWide)
		{
			return bSigned ? ENumericResultKind::Signed64 : ENumericResultKind::Unsigned64;
		}
		return bSigned ? ENumericResultKind::Signed32 : ENumericResultKind::Unsigned32;
	}

	static FString ResultType(const ENumericResultKind Kind, const asIScriptEngine& Engine)
	{
		switch (Kind)
		{
		case ENumericResultKind::Signed32:
			return TEXT("int");
		case ENumericResultKind::Unsigned32:
			return TEXT("uint");
		case ENumericResultKind::Signed64:
			return TEXT("int64");
		case ENumericResultKind::Unsigned64:
			return TEXT("uint64");
		case ENumericResultKind::Float32:
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0 ? TEXT("float32")
																		: TEXT("float");
		case ENumericResultKind::Float64:
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0 ? TEXT("float")
																		: TEXT("float64");
		case ENumericResultKind::Boolean:
			return TEXT("bool");
		default:
			return TEXT("void");
		}
	}

	static FString PairKey(const FNativeTypeCase& LeftType, const FNativeTypeCase& RightType)
	{
		return FString::Printf(TEXT("%hs_%hs"), LeftType.CatalogName, RightType.CatalogName);
	}

	static FString PairModuleName(const FNativeTypeCase& LeftType, const FNativeTypeCase& RightType)
	{
		return FString::Printf(
			TEXT("ASNativeOperatorNumeric_%hs_%hs"), LeftType.CatalogName, RightType.CatalogName);
	}

	static FString PairSourceId(const FNativeTypeCase& LeftType, const FNativeTypeCase& RightType)
	{
		return AngelscriptNativeTestSupport::MakeNativeCaseId("LANG-OP-NUMERIC-BINARY-SOURCE",
			{ANSI_TO_TCHAR(LeftType.CatalogName), ANSI_TO_TCHAR(RightType.CatalogName)});
	}

	static FString BuildNumericBinarySource(
		asIScriptEngine& Engine, const FNativeTypeCase& LeftType, const FNativeTypeCase& RightType)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString LeftDeclaration = ScriptType(LeftType, Engine);
		const FString RightDeclaration = ScriptType(RightType, Engine);
		const ENumericResultKind ArithmeticKind = PromotedKind(LeftType, RightType);

		FString Source;
		const struct
		{
			ENumericResultKind Kind;
			int32 Marker;
		} TypeObservers[] = {
			{ENumericResultKind::Signed32, 101},
			{ENumericResultKind::Unsigned32, 102},
			{ENumericResultKind::Signed64, 103},
			{ENumericResultKind::Unsigned64, 104},
			{ENumericResultKind::Float32, 105},
			{ENumericResultKind::Float64, 106},
			{ENumericResultKind::Boolean, 107},
		};
		for (const auto& Observer : TypeObservers)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("int ObserveNumericType(%s Value)"), *ResultType(Observer.Kind, Engine)));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %d;"), Observer.Marker));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		for (const FOperatorCase& OperatorCase : OperatorCases)
		{
			const FString ReturnDeclaration = ResultType(
				OperatorCase.bComparison ? ENumericResultKind::Boolean : ArithmeticKind, Engine);
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s %s(%s Left, %s Right)"),
					*ReturnDeclaration,
					OperatorCase.FunctionName,
					*LeftDeclaration,
					*RightDeclaration));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn Left %s Right;"), OperatorCase.Token));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("int %s(%s Left, %s Right)"),
					OperatorCase.TypeFunctionName,
					*LeftDeclaration,
					*RightDeclaration));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("\treturn ObserveNumericType(Left %s Right);"), OperatorCase.Token));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		return Source;
	}

	static uint64 UnsignedMaximum(const FNativeTypeCase& TypeCase)
	{
		switch (TypeCase.WidthInBytes)
		{
		case 1:
			return std::numeric_limits<uint8>::max();
		case 2:
			return std::numeric_limits<uint16>::max();
		case 4:
			return std::numeric_limits<uint32>::max();
		case 8:
		default:
			return std::numeric_limits<uint64>::max();
		}
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

	static FNumericArgument MakePartitionArgument(
		const FNativeTypeCase& TypeCase, const FValueCase& ValueCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FNumericArgument Result;
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
			return Result;
		}
		if (IsFloat64(TypeCase))
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
			return Result;
		}

		if (TypeCase.Category == ENativeValueCategory::SignedInteger)
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

	static FNumericArgument MakeSmallPositiveArgument(
		const FNativeTypeCase& TypeCase, const int32 Value)
	{
		using namespace AngelscriptNativeTestSupport;
		FNumericArgument Result;
		if (IsFloat32(TypeCase))
		{
			Result.Float32Value = static_cast<float>(Value);
		}
		else if (IsFloat64(TypeCase))
		{
			Result.Float64Value = static_cast<double>(Value);
		}
		else if (TypeCase.Category == ENativeValueCategory::SignedInteger)
		{
			Result.SignedValue = Value;
		}
		else
		{
			Result.UnsignedValue = static_cast<uint64>(Value);
		}
		return Result;
	}

	static FNumericArgument MakeRightArgument(const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FValueCase& ValueCase)
	{
		if (OperatorCase.bComparison)
		{
			return MakePartitionArgument(TypeCase, ValueCase);
		}

		switch (OperatorCase.Operator)
		{
		case ENumericOperator::Multiply:
			return MakeSmallPositiveArgument(TypeCase,
				ValueCase.Partition == EValuePartition::NearMinimum ||
						ValueCase.Partition == EValuePartition::NearMaximum
					? 1
					: 2);
		case ENumericOperator::Divide:
			return MakeSmallPositiveArgument(TypeCase,
				ValueCase.Partition == EValuePartition::NearMinimum ||
						ValueCase.Partition == EValuePartition::NearMaximum
					? 1
					: 2);
		case ENumericOperator::Modulo:
			return MakeSmallPositiveArgument(TypeCase, 2);
		case ENumericOperator::Add:
		case ENumericOperator::Subtract:
		default:
			return MakeSmallPositiveArgument(TypeCase, 1);
		}
	}

	static int32 DecodeSigned32(const uint32 Bits)
	{
		if (Bits <= static_cast<uint32>(std::numeric_limits<int32>::max()))
		{
			return static_cast<int32>(Bits);
		}
		return -1 - static_cast<int32>(std::numeric_limits<uint32>::max() - Bits);
	}

	static int64 DecodeSigned64(const uint64 Bits)
	{
		if (Bits <= static_cast<uint64>(std::numeric_limits<int64>::max()))
		{
			return static_cast<int64>(Bits);
		}
		return -1 - static_cast<int64>(std::numeric_limits<uint64>::max() - Bits);
	}

	static uint64 IntegerSourceBits(
		const FNativeTypeCase& TypeCase, const FNumericArgument& Argument)
	{
		using namespace AngelscriptNativeTestSupport;
		const uint64 Mask = TypeCase.WidthInBytes == 8
								? std::numeric_limits<uint64>::max()
								: (uint64(1) << (TypeCase.WidthInBytes * 8)) - 1;
		if (TypeCase.Category == ENativeValueCategory::SignedInteger)
		{
			return static_cast<uint64>(Argument.SignedValue) & Mask;
		}
		return Argument.UnsignedValue & Mask;
	}

	static uint32 ToUnsigned32(const FNativeTypeCase& TypeCase, const FNumericArgument& Argument)
	{
		return static_cast<uint32>(IntegerSourceBits(TypeCase, Argument));
	}

	static int32 ToSigned32(const FNativeTypeCase& TypeCase, const FNumericArgument& Argument)
	{
		const uint32 Bits = ToUnsigned32(TypeCase, Argument);
		if (IsSignedInteger(TypeCase) && TypeCase.WidthInBytes < sizeof(uint32))
		{
			const uint32 ValueMask = (uint32(1) << (TypeCase.WidthInBytes * 8)) - 1;
			const uint32 SignBit = uint32(1) << (TypeCase.WidthInBytes * 8 - 1);
			const uint32 Extended = (Bits & SignBit) != 0 ? Bits | ~ValueMask : Bits;
			return static_cast<int32>(Extended);
		}
		return DecodeSigned32(Bits);
	}

	static uint64 ToUnsigned64(const FNativeTypeCase& TypeCase, const FNumericArgument& Argument)
	{
		return IntegerSourceBits(TypeCase, Argument);
	}

	static int64 ToSigned64(const FNativeTypeCase& TypeCase, const FNumericArgument& Argument)
	{
		const uint64 Bits = ToUnsigned64(TypeCase, Argument);
		if (IsSignedInteger(TypeCase) && TypeCase.WidthInBytes < sizeof(uint64))
		{
			const uint64 ValueMask = (uint64(1) << (TypeCase.WidthInBytes * 8)) - 1;
			const uint64 SignBit = uint64(1) << (TypeCase.WidthInBytes * 8 - 1);
			const uint64 Extended = (Bits & SignBit) != 0 ? Bits | ~ValueMask : Bits;
			return static_cast<int64>(Extended);
		}
		return DecodeSigned64(Bits);
	}

	template <typename FloatType>
	static FloatType ToFloating(const FNativeTypeCase& TypeCase, const FNumericArgument& Argument)
	{
		using namespace AngelscriptNativeTestSupport;
		if (IsFloat32(TypeCase))
		{
			return static_cast<FloatType>(Argument.Float32Value);
		}
		if (IsFloat64(TypeCase))
		{
			return static_cast<FloatType>(Argument.Float64Value);
		}
		if (TypeCase.Category == ENativeValueCategory::SignedInteger)
		{
			return static_cast<FloatType>(Argument.SignedValue);
		}
		return static_cast<FloatType>(Argument.UnsignedValue);
	}

	template <typename NumericType>
	static bool EvaluateComparison(
		const NumericType Left, const NumericType Right, const ENumericOperator Operator)
	{
		switch (Operator)
		{
		case ENumericOperator::Less:
			return Left < Right;
		case ENumericOperator::LessEqual:
			return Left <= Right;
		case ENumericOperator::Greater:
			return Left > Right;
		case ENumericOperator::GreaterEqual:
			return Left >= Right;
		case ENumericOperator::Equal:
			return Left == Right;
		case ENumericOperator::NotEqual:
			return Left != Right;
		default:
			return false;
		}
	}

	template <typename NumericType>
	static NumericType EvaluateArithmetic(
		const NumericType Left, const NumericType Right, const ENumericOperator Operator)
	{
		switch (Operator)
		{
		case ENumericOperator::Add:
			return Left + Right;
		case ENumericOperator::Subtract:
			return Left - Right;
		case ENumericOperator::Multiply:
			return Left * Right;
		case ENumericOperator::Divide:
			return Left / Right;
		case ENumericOperator::Modulo:
			if constexpr (std::is_same_v<NumericType, float>)
			{
				return std::fmod(Left, Right);
			}
			else if constexpr (std::is_same_v<NumericType, double>)
			{
				return std::fmod(Left, Right);
			}
			else
			{
				return Left % Right;
			}
		default:
			return NumericType{};
		}
	}

	template <typename NumericType> static uint64 NumericBits(const NumericType Value)
	{
		if constexpr (std::is_same_v<NumericType, float>)
		{
			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Value));
			return Bits;
		}
		else if constexpr (std::is_same_v<NumericType, double>)
		{
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Value));
			return Bits;
		}
		else
		{
			using UnsignedType = std::make_unsigned_t<NumericType>;
			return static_cast<uint64>(static_cast<UnsignedType>(Value));
		}
	}

	template <typename NumericType>
	static FNumericResult EvaluateTyped(const ENumericResultKind Kind,
		const NumericType Left,
		const NumericType Right,
		const FOperatorCase& OperatorCase)
	{
		if (OperatorCase.bComparison)
		{
			return {ENumericResultKind::Boolean,
				EvaluateComparison(Left, Right, OperatorCase.Operator) ? 1ull : 0ull};
		}
		return {Kind, NumericBits(EvaluateArithmetic(Left, Right, OperatorCase.Operator))};
	}

	static FNumericResult ExpectedResult(const FNativeTypeCase& LeftType,
		const FNativeTypeCase& RightType,
		const FNumericArgument& Left,
		const FNumericArgument& Right,
		const FOperatorCase& OperatorCase)
	{
		const ENumericResultKind Kind = PromotedKind(LeftType, RightType);
		switch (Kind)
		{
		case ENumericResultKind::Signed32:
			return EvaluateTyped(
				Kind, ToSigned32(LeftType, Left), ToSigned32(RightType, Right), OperatorCase);
		case ENumericResultKind::Unsigned32:
			return EvaluateTyped(
				Kind, ToUnsigned32(LeftType, Left), ToUnsigned32(RightType, Right), OperatorCase);
		case ENumericResultKind::Signed64:
			return EvaluateTyped(
				Kind, ToSigned64(LeftType, Left), ToSigned64(RightType, Right), OperatorCase);
		case ENumericResultKind::Unsigned64:
			return EvaluateTyped(
				Kind, ToUnsigned64(LeftType, Left), ToUnsigned64(RightType, Right), OperatorCase);
		case ENumericResultKind::Float32:
			return EvaluateTyped(Kind,
				ToFloating<float>(LeftType, Left),
				ToFloating<float>(RightType, Right),
				OperatorCase);
		case ENumericResultKind::Float64:
			return EvaluateTyped(Kind,
				ToFloating<double>(LeftType, Left),
				ToFloating<double>(RightType, Right),
				OperatorCase);
		case ENumericResultKind::Boolean:
		default:
			return {ENumericResultKind::Boolean, 0};
		}
	}

	static int SetArgument(asIScriptContext& Context,
		const asUINT Index,
		const FNativeTypeCase& TypeCase,
		const FNumericArgument& Argument)
	{
		using namespace AngelscriptNativeTestSupport;
		if (IsFloat32(TypeCase))
		{
			return Context.SetArgFloat(Index, Argument.Float32Value);
		}
		if (IsFloat64(TypeCase))
		{
			return Context.SetArgDouble(Index, Argument.Float64Value);
		}

		const uint64 Bits = IntegerSourceBits(TypeCase, Argument);
		switch (TypeCase.Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Context.SetArgByte(Index, static_cast<asBYTE>(Bits));
		case ENativeScalarAccessor::Word:
			return Context.SetArgWord(Index, static_cast<asWORD>(Bits));
		case ENativeScalarAccessor::DWord:
			return Context.SetArgDWord(Index, static_cast<asDWORD>(Bits));
		case ENativeScalarAccessor::QWord:
			return Context.SetArgQWord(Index, static_cast<asQWORD>(Bits));
		default:
			return asINVALID_TYPE;
		}
	}

	static FNumericResult ReadResult(asIScriptContext& Context, const ENumericResultKind Kind)
	{
		switch (Kind)
		{
		case ENumericResultKind::Signed32:
		case ENumericResultKind::Unsigned32:
			return {Kind, Context.GetReturnDWord()};
		case ENumericResultKind::Signed64:
		case ENumericResultKind::Unsigned64:
			return {Kind, Context.GetReturnQWord()};
		case ENumericResultKind::Float32:
			return {Kind, NumericBits(Context.GetReturnFloat())};
		case ENumericResultKind::Float64:
			return {Kind, NumericBits(Context.GetReturnDouble())};
		case ENumericResultKind::Boolean:
			return {Kind, Context.GetReturnByte() != 0 ? 1ull : 0ull};
		default:
			return {Kind, 0};
		}
	}

	static int32 TypeMarker(const ENumericResultKind Kind)
	{
		switch (Kind)
		{
		case ENumericResultKind::Signed32:
			return 101;
		case ENumericResultKind::Unsigned32:
			return 102;
		case ENumericResultKind::Signed64:
			return 103;
		case ENumericResultKind::Unsigned64:
			return 104;
		case ENumericResultKind::Float32:
			return 105;
		case ENumericResultKind::Float64:
			return 106;
		case ENumericResultKind::Boolean:
			return 107;
		default:
			return INDEX_NONE;
		}
	}

	static bool HasNoErrors(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				return false;
			}
		}
		return true;
	}

	static asIScriptModule* CompilePair(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FNativeTypeCase& LeftType,
		const FNativeTypeCase& RightType,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString Source = BuildNumericBinarySource(*Engine.Get(), LeftType, RightType);
		PrintGeneratedAsSource(Test, PairSourceId(LeftType, RightType), ModuleName, Source);
		Engine.Reset(Test);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);

		FNoDiscardAsserter Assert(Test);
		const FString Context = FString::Printf(
			TEXT("[%s] numeric operator pair should compile without errors. Messages={%s}"),
			*PairSourceId(LeftType, RightType),
			*Engine.GetMessagesText());
		if (!Assert.IsTrue(BuildResult >= 0, *Context) || !Assert.IsNotNull(Module, *Context) ||
			!Assert.IsTrue(HasNoErrors(Engine.GetMessages()), *Context))
		{
			return nullptr;
		}
		return Module;
	}

	static bool VerifyMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptFunction& Function,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& LeftType,
		const FNativeTypeCase& RightType,
		const FOperatorCase& OperatorCase)
	{
		FNoDiscardAsserter Assert(Test);
		if (!Assert.AreEqual(static_cast<asUINT>(2),
				Function.GetParamCount(),
				*Case.Describe(TEXT("numeric operator should retain two parameters"))))
		{
			return false;
		}

		const ENumericResultKind ArithmeticKind = PromotedKind(LeftType, RightType);
		const ENumericResultKind ExpectedResultKind =
			OperatorCase.bComparison ? ENumericResultKind::Boolean : ArithmeticKind;
		int LeftTypeId = asINVALID_TYPE;
		int RightTypeId = asINVALID_TYPE;
		asDWORD LeftFlags = asTM_NONE;
		asDWORD RightFlags = asTM_NONE;
		const char* LeftName = nullptr;
		const char* RightName = nullptr;
		if (!Assert.AreEqual(asSUCCESS,
				Function.GetParam(0, &LeftTypeId, &LeftFlags, &LeftName),
				*Case.Describe(TEXT("left parameter metadata should be readable"))) ||
			!Assert.AreEqual(asSUCCESS,
				Function.GetParam(1, &RightTypeId, &RightFlags, &RightName),
				*Case.Describe(TEXT("right parameter metadata should be readable"))))
		{
			return false;
		}

		return Assert.AreEqual(PublicTypeId(Engine, LeftType),
				   LeftTypeId,
				   *Case.Describe(TEXT("left parameter should retain its exact source type"))) &&
			   Assert.AreEqual(PublicTypeId(Engine, RightType),
				   RightTypeId,
				   *Case.Describe(TEXT("right parameter should retain its exact source type"))) &&
				Assert.AreEqual(static_cast<asDWORD>(asTM_CONST),
					LeftFlags,
					*Case.Describe(TEXT("left value parameter should retain the fork read-only flag"))) &&
				Assert.AreEqual(static_cast<asDWORD>(asTM_CONST),
					RightFlags,
					*Case.Describe(TEXT("right value parameter should retain the fork read-only flag"))) &&
			   Assert.AreEqual(FString(TEXT("Left")),
				   FString(UTF8_TO_TCHAR(LeftName != nullptr ? LeftName : "")),
				   *Case.Describe(TEXT("left parameter should retain its source name"))) &&
			   Assert.AreEqual(FString(TEXT("Right")),
				   FString(UTF8_TO_TCHAR(RightName != nullptr ? RightName : "")),
				   *Case.Describe(TEXT("right parameter should retain its source name"))) &&
			   Assert.AreEqual(PublicResultTypeId(Engine, ExpectedResultKind),
				   Function.GetReturnTypeId(),
				   *Case.Describe(TEXT("operator result should retain the promoted type")));
	}

	static asIScriptFunction* FindExactPairFunction(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const TCHAR* FunctionName,
		const FNativeTypeCase& LeftType,
		const FNativeTypeCase& RightType,
		const ENumericResultKind ReturnKind)
	{
		const FString LeftDeclaration = ScriptType(LeftType, Engine);
		const FString RightDeclaration = ScriptType(RightType, Engine);
		const int ExpectedLeftTypeId = PublicTypeId(Engine, LeftType);
		const int ExpectedRightTypeId = PublicTypeId(Engine, RightType);
		const int ExpectedReturnTypeId = PublicResultTypeId(Engine, ReturnKind);

		asIScriptFunction* Match = nullptr;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr ||
				FCStringAnsi::Strcmp(Candidate->GetName(), TCHAR_TO_UTF8(FunctionName)) != 0 ||
				Candidate->GetParamCount() != 2 ||
				Candidate->GetReturnTypeId() != ExpectedReturnTypeId)
			{
				continue;
			}

			int LeftTypeId = asINVALID_TYPE;
			int RightTypeId = asINVALID_TYPE;
			if (Candidate->GetParam(0, &LeftTypeId) < 0 ||
				Candidate->GetParam(1, &RightTypeId) < 0 || LeftTypeId != ExpectedLeftTypeId ||
				RightTypeId != ExpectedRightTypeId)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = Candidate;
		}
		return Match;
	}

	static bool ExecuteCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& LeftType,
		const FNativeTypeCase& RightType,
		const FOperatorCase& OperatorCase,
		const FValueCase& ValueCase)
	{
		FNoDiscardAsserter Assert(Test);
		const ENumericResultKind ArithmeticKind = PromotedKind(LeftType, RightType);
		const ENumericResultKind ExpectedKind =
			OperatorCase.bComparison ? ENumericResultKind::Boolean : ArithmeticKind;
		asIScriptFunction* const Function = FindExactPairFunction(Engine,
			Module,
			OperatorCase.FunctionName,
			LeftType,
			RightType,
			ExpectedKind);
		if (!Assert.IsNotNull(Function,
				*Case.Describe(TEXT("numeric operator function should resolve by its exact "
									"two-parameter declaration"))))
		{
			return false;
		}
		if (!VerifyMetadata(Test, Engine, *Function, Case, LeftType, RightType, OperatorCase))
		{
			return false;
		}

		const FNumericArgument Left = MakePartitionArgument(LeftType, ValueCase);
		const FNumericArgument Right = MakeRightArgument(RightType, OperatorCase, ValueCase);
		const FNumericResult Expected =
			ExpectedResult(LeftType, RightType, Left, Right, OperatorCase);

		if (!Assert.AreEqual(asSUCCESS,
				Context.Prepare(Function),
				*Case.Describe(TEXT("numeric operator context should prepare"))) ||
			!Assert.AreEqual(asSUCCESS,
				SetArgument(Context, 0, LeftType, Left),
				*Case.Describe(TEXT("left argument should use its exact ABI width"))) ||
			!Assert.AreEqual(asSUCCESS,
				SetArgument(Context, 1, RightType, Right),
				*Case.Describe(TEXT("right argument should use its exact ABI width"))))
		{
			Context.Unprepare();
			return false;
		}

		const int ExecuteResult = Context.Execute();
		const FNumericResult Actual = ReadResult(Context, Expected.Kind);
		const bool bExecutionMatches = Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("numeric operator execution should finish")));
		const bool bKindMatches = Assert.AreEqual(static_cast<uint8>(Expected.Kind),
			static_cast<uint8>(Actual.Kind),
			*Case.Describe(TEXT("runtime result reader should use the promoted type")));
		const bool bValueMatches = Assert.AreEqual(Expected.Bits,
			Actual.Bits,
			*Case.DescribeResult(Function->GetDeclaration(),
				FString::Printf(TEXT("bits=0x%016llX"), Expected.Bits),
				FString::Printf(TEXT("bits=0x%016llX"), Actual.Bits)));
		const bool bUnprepared = Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("numeric operator context should unprepare for reuse")));
		if (!bExecutionMatches || !bKindMatches || !bValueMatches || !bUnprepared)
		{
			return false;
		}

		asIScriptFunction* const TypeFunction = FindExactPairFunction(
			Engine,
			Module,
			OperatorCase.TypeFunctionName,
			LeftType,
			RightType,
			ENumericResultKind::Signed32);
		if (!Assert.IsNotNull(TypeFunction,
				*Case.Describe(TEXT("numeric operator type witness should resolve"))) ||
			!Assert.AreEqual(asSUCCESS,
				Context.Prepare(TypeFunction),
				*Case.Describe(TEXT("numeric operator type witness should prepare"))) ||
			!Assert.AreEqual(asSUCCESS,
				SetArgument(Context, 0, LeftType, Left),
				*Case.Describe(TEXT("type witness should receive the exact left ABI"))) ||
			!Assert.AreEqual(asSUCCESS,
				SetArgument(Context, 1, RightType, Right),
				*Case.Describe(TEXT("type witness should receive the exact right ABI"))))
		{
			Context.Unprepare();
			return false;
		}

		const int TypeExecuteResult = Context.Execute();
		const int32 ActualTypeMarker = static_cast<int32>(Context.GetReturnDWord());
		const bool bTypeExecuted = Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			TypeExecuteResult,
			*Case.Describe(TEXT("numeric operator type witness should execute")));
		const bool bTypeSelected = Assert.AreEqual(TypeMarker(Expected.Kind),
			ActualTypeMarker,
			*Case.Describe(
				TEXT("exact ObserveNumericType overload should prove the expression result type")));
		const bool bTypeUnprepared = Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("numeric operator type witness should unprepare")));
		return bTypeExecuted && bTypeSelected && bTypeUnprepared;
	}

public:
	TEST_METHOD(OperandTypesByOperatorAndValue)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-NUMERIC-BINARY",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(IsNotNull(Engine.Get(),
			TEXT("numeric operator product should create a standalone native engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}

		TArray<const FNativeTypeCase*> NumericTypes;
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (IsNumericType(TypeCase))
			{
				NumericTypes.Add(&TypeCase);
			}
		}
		ASSERT_THAT(AreEqual(10,
			NumericTypes.Num(),
			TEXT("numeric operator product should use all ten catalogued numeric types")));
		if (NumericTypes.Num() != 10)
		{
			return;
		}

		TArray<FCompiledPair> CompiledPairs;
		TMap<FString, asIScriptModule*> ModulesByPair;
		for (const FNativeTypeCase* LeftType : NumericTypes)
		{
			for (const FNativeTypeCase* RightType : NumericTypes)
			{
				FCompiledPair& Pair = CompiledPairs.AddDefaulted_GetRef();
				Pair.LeftType = LeftType;
				Pair.RightType = RightType;
				Pair.ModuleName = PairModuleName(*LeftType, *RightType);
				Pair.Module =
					CompilePair(Engine, *TestRunner, *LeftType, *RightType, Pair.ModuleName);
				if (Pair.Module == nullptr)
				{
					return;
				}
				ModulesByPair.Add(PairKey(*LeftType, *RightType), Pair.Module);
			}
		}

		ON_SCOPE_EXIT
		{
			for (const FCompiledPair& Pair : CompiledPairs)
			{
				const FTCHARToUTF8 ModuleNameUtf8(*Pair.ModuleName);
				Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
			}
		};

		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("numeric operator product should create one reusable execution context")));
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
		for (const FNativeTypeCase* LeftType : NumericTypes)
		{
			for (const FOperatorCase& OperatorCase : OperatorCases)
			{
				for (const FNativeTypeCase* RightType : NumericTypes)
				{
					asIScriptModule* const* Module =
						ModulesByPair.Find(PairKey(*LeftType, *RightType));
					ASSERT_THAT(IsNotNull(
						Module, TEXT("compiled numeric operator pair should remain indexed")));
					if (Module == nullptr || *Module == nullptr)
					{
						return;
					}

					for (const FValueCase& ValueCase : ValueCases)
					{
						const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-NUMERIC-BINARY",
							{ANSI_TO_TCHAR(LeftType->CatalogName),
								ANSI_TO_TCHAR(OperatorCase.CatalogName),
								ANSI_TO_TCHAR(RightType->CatalogName),
								ANSI_TO_TCHAR(ValueCase.CatalogName)}));
						ConstructedIds.Add(Case.GetId());
						const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
						UniqueIds.Add(Case.GetId());
						ASSERT_THAT(IsTrue(bUniqueCaseId,
							*Case.Describe(TEXT("numeric operator case ID should be unique"))));
						bAllCasesPassed &= ExecuteCase(*TestRunner,
							*Engine.Get(),
							**Module,
							*Context,
							Case,
							*LeftType,
							*RightType,
							OperatorCase,
							ValueCase);
					}
				}
			}
		}

		ASSERT_THAT(AreEqual(5500,
			ConstructedIds.Num(),
			TEXT("numeric operator product should construct all 5,500 catalog IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("numeric operator product should construct no duplicate IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT("every numeric operator catalog cell should satisfy metadata and runtime "
				 "evidence")));

		for (const FCompiledPair& Pair : CompiledPairs)
		{
			const FTCHARToUTF8 ModuleNameUtf8(*Pair.ModuleName);
			Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
			ASSERT_THAT(IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				TEXT("numeric operator pair module should discard cleanly")));
		}
		CompiledPairs.Reset();
	}
};

#endif
