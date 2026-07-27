#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBitwiseOperatorTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.Bitwise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;

	enum class EBitwiseOperator : uint8
	{
		BitAnd,
		BitOr,
		BitXor,
		ShiftLeft,
		ShiftRightLogical,
		ShiftRightArithmetic,
	};

	enum class ERightPartition : uint8
	{
		Zero,
		One,
		SourceWidthMinusOne,
		SourceWidth,
		Negative,
	};

	enum class EOperandCategory : uint8
	{
		MutableLValue,
		ConstLValue,
		Temporary,
		Field,
		Alias,
	};

	struct FOperatorCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* Token;
		const TCHAR* FunctionSuffix;
		EBitwiseOperator Operator;
	};

	struct FRightCase
	{
		const ANSICHAR* CatalogName;
		ERightPartition Partition;
	};

	struct FCategoryCase
	{
		const ANSICHAR* CatalogName;
		EOperandCategory Category;
	};

	enum class EResultKind : uint8
	{
		Signed32,
		Unsigned32,
		Signed64,
		Unsigned64,
	};

	inline static constexpr FOperatorCase OperatorCases[] = {
		{"bit_and", TEXT("&"), TEXT("BitAnd"), EBitwiseOperator::BitAnd},
		{"bit_or", TEXT("|"), TEXT("BitOr"), EBitwiseOperator::BitOr},
		{"bit_xor", TEXT("^"), TEXT("BitXor"), EBitwiseOperator::BitXor},
		{"shift_left", TEXT("<<"), TEXT("ShiftLeft"), EBitwiseOperator::ShiftLeft},
		{"shift_right_logical",
			TEXT(">>"),
			TEXT("ShiftRightLogical"),
			EBitwiseOperator::ShiftRightLogical},
		{"shift_right_arithmetic",
			TEXT(">>>"),
			TEXT("ShiftRightArithmetic"),
			EBitwiseOperator::ShiftRightArithmetic},
	};

	inline static constexpr FRightCase RightCases[] = {
		{"zero", ERightPartition::Zero},
		{"one", ERightPartition::One},
		{"source_width_minus_one", ERightPartition::SourceWidthMinusOne},
		{"source_width", ERightPartition::SourceWidth},
		{"negative", ERightPartition::Negative},
	};

	inline static constexpr FCategoryCase CategoryCases[] = {
		{"mutable_lvalue", EOperandCategory::MutableLValue},
		{"const_lvalue", EOperandCategory::ConstLValue},
		{"temporary", EOperandCategory::Temporary},
		{"field", EOperandCategory::Field},
		{"alias", EOperandCategory::Alias},
	};

	static bool IsIntegralType(const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;
		return TypeCase.Category == ENativeValueCategory::SignedInteger ||
			   TypeCase.Category == ENativeValueCategory::UnsignedInteger;
	}

	static bool IsSigned(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category ==
			   AngelscriptNativeTestSupport::ENativeValueCategory::SignedInteger;
	}

	static EResultKind ResultKind(const FNativeTypeCase& TypeCase)
	{
		if (TypeCase.WidthInBytes == 8)
		{
			return IsSigned(TypeCase) ? EResultKind::Signed64 : EResultKind::Unsigned64;
		}
		return IsSigned(TypeCase) ? EResultKind::Signed32 : EResultKind::Unsigned32;
	}

	static FString ResultType(const EResultKind Kind)
	{
		switch (Kind)
		{
		case EResultKind::Signed32:
			return TEXT("int");
		case EResultKind::Unsigned32:
			return TEXT("uint");
		case EResultKind::Signed64:
			return TEXT("int64");
		case EResultKind::Unsigned64:
			return TEXT("uint64");
		default:
			return TEXT("void");
		}
	}

	static int32 ResultMarker(const EResultKind Kind)
	{
		switch (Kind)
		{
		case EResultKind::Signed32:
			return 301;
		case EResultKind::Unsigned32:
			return 302;
		case EResultKind::Signed64:
			return 303;
		case EResultKind::Unsigned64:
			return 304;
		default:
			return INDEX_NONE;
		}
	}

	static int32 ResultWidth(const EResultKind Kind)
	{
		return Kind == EResultKind::Signed64 || Kind == EResultKind::Unsigned64 ? 64 : 32;
	}

	static FString EvaluateName(const FOperatorCase& OperatorCase)
	{
		return FString::Printf(TEXT("Evaluate%s"), OperatorCase.FunctionSuffix);
	}

	static FString ObserveName(const FOperatorCase& OperatorCase)
	{
		return FString::Printf(TEXT("Observe%sType"), OperatorCase.FunctionSuffix);
	}

	static FString AliasName(const FOperatorCase& OperatorCase)
	{
		return FString::Printf(TEXT("Apply%sAlias"), OperatorCase.FunctionSuffix);
	}

	static FString BuildCategoryExpression(
		const FOperatorCase& OperatorCase, const FCategoryCase& CategoryCase)
	{
		switch (CategoryCase.Category)
		{
		case EOperandCategory::MutableLValue:
		case EOperandCategory::ConstLValue:
			return FString::Printf(TEXT("Value %s Right"), OperatorCase.Token);
		case EOperandCategory::Temporary:
			return FString::Printf(
				TEXT("MakeBitwiseTemporary(Input) %s Right"), OperatorCase.Token);
		case EOperandCategory::Field:
			return FString::Printf(TEXT("Owner.Value %s Right"), OperatorCase.Token);
		case EOperandCategory::Alias:
			return FString::Printf(TEXT("%s(Value, Right)"), *AliasName(OperatorCase));
		default:
			return TEXT("0");
		}
	}

	static void AppendTypeObservers(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;
		const struct
		{
			const TCHAR* Type;
			int32 Marker;
		} Observers[] = {
			{TEXT("int"), 301},
			{TEXT("uint"), 302},
			{TEXT("int64"), 303},
			{TEXT("uint64"), 304},
		};
		for (const auto& Observer : Observers)
		{
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("int ObserveBitwiseType(%s Value)"), Observer.Type));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %d;"), Observer.Marker));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static FString BuildBitwiseSource(
		const FNativeTypeCase& TypeCase, const FCategoryCase& CategoryCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString SourceType = ANSI_TO_TCHAR(TypeCase.ScriptType);
		const FString OutputType = ResultType(ResultKind(TypeCase));
		FString Source;
		AppendTypeObservers(Source);
		if (CategoryCase.Category == EOperandCategory::Temporary)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("%s MakeBitwiseTemporary(%s Input)"), *SourceType, *SourceType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Input;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (CategoryCase.Category == EOperandCategory::Field)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FBitwiseFieldOwner"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value;"), *SourceType));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		for (const FOperatorCase& OperatorCase : OperatorCases)
		{
			if (CategoryCase.Category == EOperandCategory::Alias)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("%s %s(%s& in Value, int Right)"),
						*OutputType,
						*AliasName(OperatorCase),
						*SourceType));
				AppendGeneratedAsLine(Source, TEXT("{"));
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("\treturn Value %s Right;"), OperatorCase.Token));
				AppendGeneratedAsLine(Source, TEXT("}"));
				AppendGeneratedAsLine(Source);
			}

			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s %s(%s Input, int Right)"),
					*OutputType,
					*EvaluateName(OperatorCase),
					*SourceType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			switch (CategoryCase.Category)
			{
			case EOperandCategory::MutableLValue:
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("\t%s Value = Input;"), *SourceType));
				break;
			case EOperandCategory::ConstLValue:
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("\tconst %s Value = Input;"), *SourceType));
				break;
			case EOperandCategory::Field:
				AppendGeneratedAsLine(Source, TEXT("\tFBitwiseFieldOwner Owner;"));
				AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
				break;
			case EOperandCategory::Temporary:
			case EOperandCategory::Alias:
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%s Value = Input;"), *SourceType));
				break;
			}
			const FString Expression = BuildCategoryExpression(OperatorCase, CategoryCase);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);

			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("int %s(%s Input, int Right)"), *ObserveName(OperatorCase), *SourceType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			switch (CategoryCase.Category)
			{
			case EOperandCategory::MutableLValue:
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("\t%s Value = Input;"), *SourceType));
				break;
			case EOperandCategory::ConstLValue:
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("\tconst %s Value = Input;"), *SourceType));
				break;
			case EOperandCategory::Field:
				AppendGeneratedAsLine(Source, TEXT("\tFBitwiseFieldOwner Owner;"));
				AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
				break;
			case EOperandCategory::Temporary:
			case EOperandCategory::Alias:
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%s Value = Input;"), *SourceType));
				break;
			}
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn ObserveBitwiseType(%s);"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		return Source;
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

	static asIScriptModule* CompileAndReport(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FNativeTypeCase& TypeCase,
		const FCategoryCase& CategoryCase,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString SourceId = MakeNativeCaseId("LANG-OP-INTEGRAL-BITWISE-SOURCE",
			{ANSI_TO_TCHAR(TypeCase.CatalogName), ANSI_TO_TCHAR(CategoryCase.CatalogName)});
		const FString Source = BuildBitwiseSource(TypeCase, CategoryCase);
		PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		Engine.Reset(Test);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		FNoDiscardAsserter Assert(Test);
		const FString Description =
			FString::Printf(TEXT("[%s] bitwise source should compile. Build=%d Messages={%s}"),
				*SourceId,
				BuildResult,
				*Engine.GetMessagesText());
		if (!Assert.IsTrue(BuildResult >= 0, *Description) ||
			!Assert.IsNotNull(Module, *Description) ||
			!Assert.IsTrue(HasNoErrors(Engine.GetMessages()), *Description))
		{
			return nullptr;
		}
		return Module;
	}

	static asIScriptFunction* FindFunction(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FString& Name,
		const FNativeTypeCase& TypeCase,
		const int ReturnTypeId)
	{
		const int SourceTypeId = Engine.GetTypeIdByDecl(TypeCase.ScriptType);
		const int RightTypeId = Engine.GetTypeIdByDecl("int");
		asIScriptFunction* Match = nullptr;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr || FString(UTF8_TO_TCHAR(Candidate->GetName())) != Name ||
				Candidate->GetParamCount() != 2 || Candidate->GetReturnTypeId() != ReturnTypeId)
			{
				continue;
			}
			int SourceParameterTypeId = asINVALID_TYPE;
			int RightParameterTypeId = asINVALID_TYPE;
			if (Candidate->GetParam(0, &SourceParameterTypeId) >= 0 &&
				Candidate->GetParam(1, &RightParameterTypeId) >= 0 &&
				SourceParameterTypeId == SourceTypeId && RightParameterTypeId == RightTypeId)
			{
				if (Match != nullptr)
				{
					return nullptr;
				}
				Match = Candidate;
			}
		}
		return Match;
	}

	static asIScriptFunction* FindAliasFunction(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FOperatorCase& OperatorCase,
		const FNativeTypeCase& TypeCase)
	{
		return FindFunction(Engine,
			Module,
			AliasName(OperatorCase),
			TypeCase,
			Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*ResultType(ResultKind(TypeCase)))));
	}

	static asIScriptFunction* FindTemporaryFunction(
		asIScriptEngine& Engine, asIScriptModule& Module, const FNativeTypeCase& TypeCase)
	{
		const int TypeId = Engine.GetTypeIdByDecl(TypeCase.ScriptType);
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr ||
				FCStringAnsi::Strcmp(Candidate->GetName(), "MakeBitwiseTemporary") != 0 ||
				Candidate->GetParamCount() != 1 || Candidate->GetReturnTypeId() != TypeId)
			{
				continue;
			}
			int ParameterTypeId = asINVALID_TYPE;
			if (Candidate->GetParam(0, &ParameterTypeId) >= 0 && ParameterTypeId == TypeId)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static bool VerifyCategoryMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptFunction& Evaluate,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FCategoryCase& CategoryCase)
	{
		FNoDiscardAsserter Assert(Test);
		const int ExpectedTypeId = Engine.GetTypeIdByDecl(TypeCase.ScriptType);
		int SourceTypeId = asINVALID_TYPE;
		int RightTypeId = asINVALID_TYPE;
		asDWORD SourceFlags = asTM_NONE;
		asDWORD RightFlags = asTM_NONE;
		const char* SourceName = nullptr;
		const char* RightName = nullptr;
		if (!Assert.AreEqual(asSUCCESS,
				Evaluate.GetParam(0, &SourceTypeId, &SourceFlags, &SourceName),
				*Case.Describe(TEXT("bitwise source parameter metadata should be readable"))) ||
			!Assert.AreEqual(asSUCCESS,
				Evaluate.GetParam(1, &RightTypeId, &RightFlags, &RightName),
				*Case.Describe(TEXT("bitwise right parameter metadata should be readable"))) ||
			!Assert.AreEqual(ExpectedTypeId,
				SourceTypeId,
				*Case.Describe(TEXT("bitwise source parameter should retain exact type"))) ||
			!Assert.AreEqual(Engine.GetTypeIdByDecl("int"),
				RightTypeId,
				*Case.Describe(TEXT("bitwise right parameter should remain runtime int"))) ||
			!Assert.AreEqual(static_cast<asDWORD>(asTM_CONST),
				SourceFlags,
				*Case.Describe(TEXT("bitwise source value parameter should retain the fork read-only flag"))) ||
			!Assert.AreEqual(static_cast<asDWORD>(asTM_CONST),
				RightFlags,
				*Case.Describe(TEXT("bitwise right value parameter should retain the fork read-only flag"))) ||
			!Assert.AreEqual(FString(TEXT("Input")),
				FString(UTF8_TO_TCHAR(SourceName != nullptr ? SourceName : "")),
				*Case.Describe(TEXT("bitwise source parameter should retain its name"))) ||
			!Assert.AreEqual(FString(TEXT("Right")),
				FString(UTF8_TO_TCHAR(RightName != nullptr ? RightName : "")),
				*Case.Describe(TEXT("bitwise right parameter should retain its name"))))
		{
			return false;
		}
		if (CategoryCase.Category == EOperandCategory::Field)
		{
			asITypeInfo* const Owner = Module.GetTypeInfoByName("FBitwiseFieldOwner");
			if (!Assert.IsNotNull(Owner,
					*Case.Describe(TEXT("bitwise field category should publish owner type"))))
			{
				return false;
			}
			const char* PropertyName = nullptr;
			int PropertyTypeId = asINVALID_TYPE;
			return Assert.AreEqual(asSUCCESS,
					   Owner->GetProperty(0, &PropertyName, &PropertyTypeId),
					   *Case.Describe(TEXT("bitwise field metadata should be readable"))) &&
				   Assert.AreEqual(ExpectedTypeId,
					   PropertyTypeId,
					   *Case.Describe(TEXT("bitwise field should retain source type"))) &&
				   Assert.AreEqual(FString(TEXT("Value")),
					   FString(UTF8_TO_TCHAR(PropertyName != nullptr ? PropertyName : "")),
					   *Case.Describe(TEXT("bitwise field should retain source name")));
		}
		if (CategoryCase.Category == EOperandCategory::Alias)
		{
			asIScriptFunction* const Alias =
				FindAliasFunction(Engine, Module, OperatorCase, TypeCase);
			if (!Assert.IsNotNull(
					Alias, *Case.Describe(TEXT("bitwise alias helper should resolve exactly"))))
			{
				return false;
			}
			int AliasTypeId = asINVALID_TYPE;
			asDWORD AliasFlags = asTM_NONE;
			return Assert.AreEqual(asSUCCESS,
					   Alias->GetParam(0, &AliasTypeId, &AliasFlags),
					   *Case.Describe(TEXT("bitwise alias metadata should be readable"))) &&
				   Assert.AreEqual(ExpectedTypeId,
					   AliasTypeId,
					   *Case.Describe(TEXT("bitwise alias should retain source type"))) &&
				   Assert.AreEqual(static_cast<asDWORD>(asTM_INREF),
					   AliasFlags,
					   *Case.Describe(
						   TEXT("bitwise alias should retain input-reference modifier")));
		}
		if (CategoryCase.Category == EOperandCategory::Temporary)
		{
			asIScriptFunction* const Temporary = FindTemporaryFunction(Engine, Module, TypeCase);
			return Assert.IsNotNull(
				Temporary, *Case.Describe(TEXT("bitwise temporary should publish typed producer")));
		}

		for (asUINT VariableIndex = 0; VariableIndex < Evaluate.GetVarCount(); ++VariableIndex)
		{
			const char* Name = nullptr;
			int TypeId = asINVALID_TYPE;
			if (Evaluate.GetVar(VariableIndex, &Name, &TypeId) >= 0 && Name != nullptr &&
				FCStringAnsi::Strcmp(Name, "Value") == 0)
			{
				const FString Declaration = UTF8_TO_TCHAR(Evaluate.GetVarDecl(VariableIndex, true));
				return Assert.AreEqual(ExpectedTypeId,
						   TypeId,
						   *Case.Describe(TEXT("bitwise local should retain source type"))) &&
					   Assert.AreEqual(CategoryCase.Category == EOperandCategory::ConstLValue,
						   Declaration.Contains(TEXT("const ")),
						   *Case.Describe(TEXT("bitwise local should retain constness")));
			}
		}
		return Assert.IsTrue(
			false, *Case.Describe(TEXT("bitwise local category should publish named local")));
	}

	static uint64 WidthMask(const int32 Width)
	{
		return Width == 64 ? MAX_uint64 : (uint64{1} << Width) - 1;
	}

	static uint64 SourceBits(const FNativeTypeCase& TypeCase)
	{
		switch (TypeCase.WidthInBytes)
		{
		case 1:
			return 0xA5ull;
		case 2:
			return 0xA55Aull;
		case 4:
			return 0xA55AA55Aull;
		case 8:
			return 0xA55AA55AA55AA55Aull;
		default:
			return 0;
		}
	}

	static uint64 PromotedBits(const FNativeTypeCase& TypeCase)
	{
		const int32 SourceWidth = TypeCase.WidthInBytes * 8;
		const int32 OutputWidth = ResultWidth(ResultKind(TypeCase));
		const uint64 Bits = SourceBits(TypeCase) & WidthMask(SourceWidth);
		if (!IsSigned(TypeCase) || SourceWidth == OutputWidth ||
			(Bits & (uint64{1} << (SourceWidth - 1))) == 0)
		{
			return Bits & WidthMask(OutputWidth);
		}
		return (Bits | ~WidthMask(SourceWidth)) & WidthMask(OutputWidth);
	}

	static int32 RightValue(const FNativeTypeCase& TypeCase, const FRightCase& RightCase)
	{
		const int32 SourceWidth = TypeCase.WidthInBytes * 8;
		switch (RightCase.Partition)
		{
		case ERightPartition::Zero:
			return 0;
		case ERightPartition::One:
			return 1;
		case ERightPartition::SourceWidthMinusOne:
			return SourceWidth - 1;
		case ERightPartition::SourceWidth:
			return SourceWidth;
		case ERightPartition::Negative:
			return -1;
		default:
			return 0;
		}
	}

	static uint64 ArithmeticShiftRight(const uint64 Bits, const int32 Count, const int32 Width)
	{
		const int32 EffectiveCount = Count & (Width - 1);
		const uint64 Mask = WidthMask(Width);
		const uint64 Value = Bits & Mask;
		if (EffectiveCount == 0)
		{
			return Value;
		}
		const uint64 Shifted = Value >> EffectiveCount;
		if ((Value & (uint64{1} << (Width - 1))) == 0)
		{
			return Shifted;
		}
		const uint64 Fill = Mask ^ ((uint64{1} << (Width - EffectiveCount)) - 1);
		return (Shifted | Fill) & Mask;
	}

	static uint64 ExpectedBits(const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FRightCase& RightCase)
	{
		const EResultKind Kind = ResultKind(TypeCase);
		const int32 Width = ResultWidth(Kind);
		const uint64 Mask = WidthMask(Width);
		const uint64 Left = PromotedBits(TypeCase);
		const int32 RightSigned = RightValue(TypeCase, RightCase);
		const uint64 Right = static_cast<uint64>(static_cast<int64>(RightSigned)) & Mask;
		const int32 Count = RightSigned & (Width - 1);
		switch (OperatorCase.Operator)
		{
		case EBitwiseOperator::BitAnd:
			return Left & Right;
		case EBitwiseOperator::BitOr:
			return Left | Right;
		case EBitwiseOperator::BitXor:
			return Left ^ Right;
		case EBitwiseOperator::ShiftLeft:
			return (Left << Count) & Mask;
		case EBitwiseOperator::ShiftRightLogical:
			return Left >> Count;
		case EBitwiseOperator::ShiftRightArithmetic:
			return ArithmeticShiftRight(Left, Count, Width);
		default:
			return 0;
		}
	}

	static int SetSourceArgument(asIScriptContext& Context, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const uint64 Bits = SourceBits(TypeCase);
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

	static uint64 ReadResultBits(asIScriptContext& Context, const EResultKind Kind)
	{
		return ResultWidth(Kind) == 64 ? Context.GetReturnQWord() : Context.GetReturnDWord();
	}

	static asEBCInstr ExpectedOpcode(const FOperatorCase& OperatorCase, const EResultKind Kind)
	{
		const bool bWide = ResultWidth(Kind) == 64;
		switch (OperatorCase.Operator)
		{
		case EBitwiseOperator::BitAnd:
			return bWide ? asBC_BAND64 : asBC_BAND;
		case EBitwiseOperator::BitOr:
			return bWide ? asBC_BOR64 : asBC_BOR;
		case EBitwiseOperator::BitXor:
			return bWide ? asBC_BXOR64 : asBC_BXOR;
		case EBitwiseOperator::ShiftLeft:
			return bWide ? asBC_BSLL64 : asBC_BSLL;
		case EBitwiseOperator::ShiftRightLogical:
			return bWide ? asBC_BSRL64 : asBC_BSRL;
		case EBitwiseOperator::ShiftRightArithmetic:
			return bWide ? asBC_BSRA64 : asBC_BSRA;
		default:
			return asBC_MAXBYTECODE;
		}
	}

	static bool ContainsOpcode(asIScriptFunction& Function, const asEBCInstr Expected)
	{
		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode = Function.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}
		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (Opcode == Expected)
			{
				return true;
			}
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}
			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				return false;
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}
		return false;
	}

	static asIScriptFunction* OpcodeOwner(asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptFunction& Evaluate,
		const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FCategoryCase& CategoryCase)
	{
		return CategoryCase.Category == EOperandCategory::Alias
				   ? FindAliasFunction(Engine, Module, OperatorCase, TypeCase)
				   : &Evaluate;
	}

	static bool ExecuteCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FRightCase& RightCase,
		const FCategoryCase& CategoryCase)
	{
		FNoDiscardAsserter Assert(Test);
		const EResultKind Kind = ResultKind(TypeCase);
		asIScriptFunction* const Evaluate = FindFunction(Engine,
			Module,
			EvaluateName(OperatorCase),
			TypeCase,
			Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*ResultType(Kind))));
		asIScriptFunction* const Observe = FindFunction(
			Engine, Module, ObserveName(OperatorCase), TypeCase, Engine.GetTypeIdByDecl("int"));
		if (!Assert.IsNotNull(
				Evaluate, *Case.Describe(TEXT("bitwise evaluator should resolve exactly"))) ||
			!Assert.IsNotNull(
				Observe, *Case.Describe(TEXT("bitwise type witness should resolve exactly"))) ||
			!VerifyCategoryMetadata(
				Test, Engine, Module, *Evaluate, Case, TypeCase, OperatorCase, CategoryCase))
		{
			return false;
		}
		asIScriptFunction* const Owner =
			OpcodeOwner(Engine, Module, *Evaluate, TypeCase, OperatorCase, CategoryCase);
		if (!Assert.IsNotNull(
				Owner, *Case.Describe(TEXT("bitwise opcode owner should resolve exactly"))))
		{
			return false;
		}
		bool bPassed = Assert.IsTrue(ContainsOpcode(*Owner, ExpectedOpcode(OperatorCase, Kind)),
			*Case.Describe(TEXT("bitwise source should emit exact width-specific fork opcode")));
		const int32 Right = RightValue(TypeCase, RightCase);
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Prepare(Evaluate),
			*Case.Describe(TEXT("bitwise evaluator should prepare")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			SetSourceArgument(Context, TypeCase),
			*Case.Describe(TEXT("bitwise source argument should bind exact ABI width")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.SetArgDWord(1, static_cast<asDWORD>(Right)),
			*Case.Describe(TEXT("bitwise right partition should bind as runtime int")));
		const int ExecuteResult = Context.Execute();
		const uint64 Actual = ReadResultBits(Context, Kind);
		const uint64 Expected = ExpectedBits(TypeCase, OperatorCase, RightCase);
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("bitwise evaluator should execute")));
		bPassed &= Assert.AreEqual(Expected,
			Actual,
			*Case.DescribeResult(Evaluate->GetDeclaration(),
				FString::Printf(TEXT("bits=0x%016llX"), Expected),
				FString::Printf(TEXT("bits=0x%016llX"), Actual)));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("bitwise evaluator should unprepare")));

		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Prepare(Observe),
			*Case.Describe(TEXT("bitwise type witness should prepare")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			SetSourceArgument(Context, TypeCase),
			*Case.Describe(TEXT("bitwise type witness should receive exact source ABI")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.SetArgDWord(1, static_cast<asDWORD>(Right)),
			*Case.Describe(TEXT("bitwise type witness should receive runtime right operand")));
		const int ObserveResult = Context.Execute();
		const int32 ActualMarker = static_cast<int32>(Context.GetReturnDWord());
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ObserveResult,
			*Case.Describe(TEXT("bitwise type witness should execute")));
		bPassed &= Assert.AreEqual(ResultMarker(Kind),
			ActualMarker,
			*Case.Describe(TEXT("exact observer overload should prove bitwise result type")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("bitwise type witness should unprepare")));
		return bPassed;
	}

public:
	TEST_METHOD(TypesByOperatorCountAndCategory)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-INTEGRAL-BITWISE",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata |
				ENativeEvidence::Bytecode);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(
			IsNotNull(Engine.Get(), TEXT("bitwise product should create a standalone engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("bitwise product should create one reusable context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		TArray<const FNativeTypeCase*> IntegralTypes;
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (IsIntegralType(TypeCase))
			{
				IntegralTypes.Add(&TypeCase);
			}
		}
		ASSERT_THAT(AreEqual(8,
			IntegralTypes.Num(),
			TEXT("bitwise product should retain all eight integral types")));
		if (IntegralTypes.Num() != 8)
		{
			return;
		}

		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		bool bAllCasesPassed = true;
		for (const FCategoryCase& CategoryCase : CategoryCases)
		{
			for (const FOperatorCase& OperatorCase : OperatorCases)
			{
				for (const FRightCase& RightCase : RightCases)
				{
					for (const FNativeTypeCase* TypeCase : IntegralTypes)
					{
						const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-INTEGRAL-BITWISE",
							{ANSI_TO_TCHAR(CategoryCase.CatalogName),
								ANSI_TO_TCHAR(OperatorCase.CatalogName),
								ANSI_TO_TCHAR(RightCase.CatalogName),
								ANSI_TO_TCHAR(TypeCase->CatalogName)}));
						ConstructedIds.Add(Case.GetId());
						const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
						UniqueIds.Add(Case.GetId());
						ASSERT_THAT(IsTrue(bUniqueCaseId,
							*Case.Describe(TEXT("bitwise case ID should be unique"))));
					}
				}
			}
		}

		for (const FNativeTypeCase* TypeCase : IntegralTypes)
		{
			for (const FCategoryCase& CategoryCase : CategoryCases)
			{
				const FString ModuleName = FString::Printf(TEXT("ASNativeBitwise_%hs_%hs"),
					TypeCase->CatalogName,
					CategoryCase.CatalogName);
				asIScriptModule* const Module =
					CompileAndReport(Engine, *TestRunner, *TypeCase, CategoryCase, ModuleName);
				if (Module == nullptr)
				{
					return;
				}
				for (const FOperatorCase& OperatorCase : OperatorCases)
				{
					for (const FRightCase& RightCase : RightCases)
					{
						const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-INTEGRAL-BITWISE",
							{ANSI_TO_TCHAR(CategoryCase.CatalogName),
								ANSI_TO_TCHAR(OperatorCase.CatalogName),
								ANSI_TO_TCHAR(RightCase.CatalogName),
								ANSI_TO_TCHAR(TypeCase->CatalogName)}));
						bAllCasesPassed &= ExecuteCase(*TestRunner,
							*Engine.Get(),
							*Module,
							*Context,
							Case,
							*TypeCase,
							OperatorCase,
							RightCase,
							CategoryCase);
					}
				}
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				ASSERT_THAT(AreEqual(asSUCCESS,
					Engine.Get()->DiscardModule(ModuleNameUtf8.Get()),
					TEXT("bitwise module should discard")));
				ASSERT_THAT(
					IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						TEXT("bitwise module should leave no stale owner")));
			}
		}

		ASSERT_THAT(AreEqual(1200,
			ConstructedIds.Num(),
			TEXT("bitwise product should construct all 1,200 catalog IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("bitwise product should construct no duplicate IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT("every bitwise cell should satisfy category, promoted type, exact opcode, "
				 "runtime bit pattern, count behavior, and cleanup")));
	}
};

#endif
