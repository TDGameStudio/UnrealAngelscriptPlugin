#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#include <cstring>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FPowerOperatorTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.Power",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;

	enum class ESourceShape : uint8
	{
		Constant,
		MutableLvalue,
		ConstLvalue,
		FunctionReturn,
	};

	enum class EScenario : uint8
	{
		ZeroExponent,
		OneExponent,
		NearLimit,
		Overflow,
		NegativeExponent,
		FractionalExponent,
	};

	enum class EResultKind : uint8
	{
		Signed32,
		Unsigned32,
		Signed64,
		Unsigned64,
		Float32,
		Float64,
	};

	struct FSourceShapeCase
	{
		const ANSICHAR* CatalogName;
		ESourceShape Shape;
	};

	struct FPowerCase
	{
		const FNativeTypeCase* BaseType = nullptr;
		const FNativeTypeCase* ExponentType = nullptr;
		const FSourceShapeCase* SourceShape = nullptr;
		EScenario Scenario = EScenario::ZeroExponent;
	};

	inline static constexpr FSourceShapeCase SourceShapeCases[] = {
		{"constant", ESourceShape::Constant},
		{"mutable_lvalue", ESourceShape::MutableLvalue},
		{"const_lvalue", ESourceShape::ConstLvalue},
		{"function_return", ESourceShape::FunctionReturn},
	};

	static bool IsNumeric(const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;
		return TypeCase.Category == ENativeValueCategory::SignedInteger
			|| TypeCase.Category == ENativeValueCategory::UnsignedInteger
			|| TypeCase.Category == ENativeValueCategory::FloatingPoint;
	}

	static bool IsSignedInteger(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == AngelscriptNativeTestSupport::ENativeValueCategory::SignedInteger;
	}

	static bool IsUnsignedInteger(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == AngelscriptNativeTestSupport::ENativeValueCategory::UnsignedInteger;
	}

	static bool IsIntegerType(const FNativeTypeCase& TypeCase)
	{
		return IsSignedInteger(TypeCase) || IsUnsignedInteger(TypeCase);
	}

	static bool HasMixedIntegerSigns(const FPowerCase& PowerCase)
	{
		return IsIntegerType(*PowerCase.BaseType)
			&& IsIntegerType(*PowerCase.ExponentType)
			&& (IsUnsignedInteger(*PowerCase.BaseType) != IsUnsignedInteger(*PowerCase.ExponentType));
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
		const bool bFloatIsFloat64 = Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		if (IsFloat32(TypeCase))
		{
			return bFloatIsFloat64 ? TEXT("float32") : TEXT("float");
		}
		if (IsFloat64(TypeCase))
		{
			return bFloatIsFloat64 ? TEXT("float") : TEXT("float64");
		}
		return ANSI_TO_TCHAR(TypeCase.ScriptType);
	}

	static bool IsConstantSource(const FPowerCase& PowerCase)
	{
		return PowerCase.SourceShape->Shape == ESourceShape::Constant;
	}

	static bool IsCompileTimeKnownSource(const FPowerCase& PowerCase)
	{
		return PowerCase.SourceShape->Shape == ESourceShape::Constant
			|| PowerCase.SourceShape->Shape == ESourceShape::ConstLvalue;
	}

	static EResultKind ResultKind(const FPowerCase& PowerCase)
	{
		const FNativeTypeCase& BaseType = *PowerCase.BaseType;
		const FNativeTypeCase& ExponentType = *PowerCase.ExponentType;
		if (IsFloat64(BaseType) || IsFloat64(ExponentType))
		{
			return EResultKind::Float64;
		}
		if (IsFloat32(BaseType) || IsFloat32(ExponentType))
		{
			return EResultKind::Float32;
		}

		const bool bWide = BaseType.WidthInBytes == 8 || ExponentType.WidthInBytes == 8;
		const bool bSigned = IsConstantSource(PowerCase)
			? !IsUnsignedInteger(BaseType) && !IsUnsignedInteger(ExponentType)
			: IsSignedInteger(BaseType) || IsSignedInteger(ExponentType);
		if (bWide)
		{
			return bSigned ? EResultKind::Signed64 : EResultKind::Unsigned64;
		}
		return bSigned ? EResultKind::Signed32 : EResultKind::Unsigned32;
	}

	static bool IsIntegerResult(const EResultKind Kind)
	{
		return Kind == EResultKind::Signed32 || Kind == EResultKind::Unsigned32
			|| Kind == EResultKind::Signed64 || Kind == EResultKind::Unsigned64;
	}

	static FString ResultType(const EResultKind Kind, const asIScriptEngine& Engine)
	{
		const bool bFloatIsFloat64 = Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
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
		case EResultKind::Float32:
			return bFloatIsFloat64 ? TEXT("float32") : TEXT("float");
		case EResultKind::Float64:
			return bFloatIsFloat64 ? TEXT("float") : TEXT("float64");
		default:
			return TEXT("void");
		}
	}

	static const ANSICHAR* PublicResultTypeName(const EResultKind Kind)
	{
		switch (Kind)
		{
		case EResultKind::Float32:
			return "float32";
		case EResultKind::Float64:
			return "float64";
		default:
			return nullptr;
		}
	}

	static const TCHAR* ScenarioName(const EScenario Scenario)
	{
		switch (Scenario)
		{
		case EScenario::ZeroExponent:
			return TEXT("zero_exponent");
		case EScenario::OneExponent:
			return TEXT("one_exponent");
		case EScenario::NearLimit:
			return TEXT("near_limit");
		case EScenario::Overflow:
			return TEXT("overflow");
		case EScenario::NegativeExponent:
			return TEXT("negative_exponent");
		case EScenario::FractionalExponent:
			return TEXT("fractional_exponent");
		default:
			return TEXT("unknown");
		}
	}

	static FString MakeCaseId(const FString& ProductId, const FPowerCase& PowerCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FTCHARToUTF8 ProductIdUtf8(*ProductId);
		// ANSI_TO_TCHAR() returns a pointer into a temporary conversion object.  Do
		// not retain those pointers between statements: each conversion can reuse
		// the same scratch storage, which silently collapsed all three axes to the
		// last value (for example CONSTANT) and caused duplicate module names.
		const FString BaseName = UTF8_TO_TCHAR(PowerCase.BaseType->CatalogName);
		const FString ExponentName = UTF8_TO_TCHAR(PowerCase.ExponentType->CatalogName);
		const FString ShapeName = UTF8_TO_TCHAR(PowerCase.SourceShape->CatalogName);
		return MakeNativeCaseId(
			ProductIdUtf8.Get(), {*BaseName, *ExponentName, *ShapeName, ScenarioName(PowerCase.Scenario)});
	}

	static FString ModuleName(const FString& ProductId, const FPowerCase& PowerCase)
	{
		return FString::Printf(TEXT("ASNativePower_%s_%hs_%hs_%hs_%s"),
			*ProductId.RightChop(FString(TEXT("LANG-OP-POWER-")).Len()),
			PowerCase.BaseType->CatalogName,
			PowerCase.ExponentType->CatalogName,
			PowerCase.SourceShape->CatalogName,
			ScenarioName(PowerCase.Scenario));
	}

	static FString CastExpression(const FString& TypeName, const FString& Literal)
	{
		return FString::Printf(TEXT("%s(%s)"), *TypeName, *Literal);
	}

	static FString OverflowExponentLiteral(const EResultKind Kind)
	{
		switch (Kind)
		{
		case EResultKind::Signed32:
			return TEXT("31");
		case EResultKind::Unsigned32:
			return TEXT("32");
		case EResultKind::Signed64:
			return TEXT("63");
		case EResultKind::Unsigned64:
			return TEXT("64");
		case EResultKind::Float32:
			return TEXT("128");
		case EResultKind::Float64:
			return TEXT("1024");
		default:
			return TEXT("1");
		}
	}

	static FString NearLimitExponentLiteral(const FPowerCase& PowerCase)
	{
		// The current fork deliberately selects POWdi for a float64 base and any
		// integral exponent. Its documented implementation narrows that operand to
		// int, so the route-specific boundary is the signed 32-bit boundary rather
		// than the source declaration's wider maximum.
		if (IsFloat64(*PowerCase.BaseType))
		{
			if (IsSignedInteger(*PowerCase.ExponentType))
			{
				if (PowerCase.ExponentType->WidthInBytes >= 4)
				{
					return TEXT("2147483647");
				}
			}
			else if (IsUnsignedInteger(*PowerCase.ExponentType)
				&& PowerCase.ExponentType->WidthInBytes >= 4)
			{
				return TEXT("2147483647");
			}
		}

		return ANSI_TO_TCHAR(PowerCase.ExponentType->MaximumLiteral);
	}

	static void OperandLiterals(const FPowerCase& PowerCase,
		FString& OutBaseLiteral,
		FString& OutExponentLiteral)
	{
		switch (PowerCase.Scenario)
		{
		case EScenario::ZeroExponent:
			OutBaseLiteral = TEXT("2");
			OutExponentLiteral = TEXT("0");
			break;
		case EScenario::OneExponent:
			OutBaseLiteral = TEXT("2");
			OutExponentLiteral = TEXT("1");
			break;
		case EScenario::NearLimit:
			OutBaseLiteral = TEXT("1");
			OutExponentLiteral = NearLimitExponentLiteral(PowerCase);
			break;
		case EScenario::Overflow:
			if ((IsFloat32(*PowerCase.BaseType) || IsFloat64(*PowerCase.BaseType))
				&& (IsSignedInteger(*PowerCase.ExponentType)
					|| IsUnsignedInteger(*PowerCase.ExponentType)))
			{
				// The float64/integer fast path narrows the exponent to int. A maximal
				// floating base and the representable exponent two still exercise overflow
				// without constructing an out-of-range operand for int8 or uint8.
				OutBaseLiteral = ANSI_TO_TCHAR(PowerCase.BaseType->MaximumLiteral);
				OutExponentLiteral = TEXT("2");
			}
			else
			{
				OutBaseLiteral = TEXT("2");
				OutExponentLiteral = OverflowExponentLiteral(ResultKind(PowerCase));
			}
			break;
		case EScenario::NegativeExponent:
			OutBaseLiteral = TEXT("2");
			OutExponentLiteral = TEXT("-2");
			break;
		case EScenario::FractionalExponent:
			OutBaseLiteral = TEXT("4");
			OutExponentLiteral = TEXT("0.5");
			break;
		default:
			OutBaseLiteral = TEXT("0");
			OutExponentLiteral = TEXT("0");
			break;
		}
	}

	static FString BuildPowerSource(asIScriptEngine& Engine, const FPowerCase& PowerCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString BaseTypeName = ScriptType(*PowerCase.BaseType, Engine);
		const FString ExponentTypeName = ScriptType(*PowerCase.ExponentType, Engine);
		const FString ReturnTypeName = ResultType(ResultKind(PowerCase), Engine);
		FString BaseLiteral;
		FString ExponentLiteral;
		OperandLiterals(PowerCase, BaseLiteral, ExponentLiteral);
		const FString BaseExpression = CastExpression(BaseTypeName, BaseLiteral);
		const FString ExponentExpression = CastExpression(ExponentTypeName, ExponentLiteral);

		FString Source;
		if (PowerCase.SourceShape->Shape == ESourceShape::FunctionReturn)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s PowerBaseValue()"), *BaseTypeName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *BaseExpression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s PowerExponentValue()"), *ExponentTypeName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\treturn %s;"), *ExponentExpression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("%s EvaluatePower()"), *ReturnTypeName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (PowerCase.SourceShape->Shape)
		{
		case ESourceShape::Constant:
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\treturn %s ** %s; // POWER_CAUSE"),
					*BaseExpression,
					*ExponentExpression));
			break;
		case ESourceShape::MutableLvalue:
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s Base = %s;"), *BaseTypeName, *BaseExpression));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s Exponent = %s;"), *ExponentTypeName, *ExponentExpression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Base ** Exponent; // POWER_CAUSE"));
			break;
		case ESourceShape::ConstLvalue:
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\tconst %s Base = %s;"), *BaseTypeName, *BaseExpression));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\tconst %s Exponent = %s;"), *ExponentTypeName, *ExponentExpression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Base ** Exponent; // POWER_CAUSE"));
			break;
		case ESourceShape::FunctionReturn:
			AppendGeneratedAsLine(Source,
				TEXT("\treturn PowerBaseValue() ** PowerExponentValue(); // POWER_CAUSE"));
			break;
		default:
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int PowerFollowUp()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 17;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int EvaluatePower()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 17;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int PowerFollowUp()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 17;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
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

	static TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> ErrorMessages(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> Errors;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				Errors.Add(Entry);
			}
		}
		return Errors;
	}

	static int32 LastSourceLineContaining(const FString& Source, const TCHAR* Needle)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
		{
			if (Lines[Index].Contains(Needle))
			{
				return Index + 1;
			}
		}
		return INDEX_NONE;
	}

	static bool ExpectedBuildFailure(const FPowerCase& PowerCase)
	{
		const EResultKind Kind = ResultKind(PowerCase);
		if (PowerCase.Scenario == EScenario::Overflow && IsCompileTimeKnownSource(PowerCase))
		{
			// A direct constant is folded and rejected for overflow.  A const local
			// follows the runtime route for mixed signed/unsigned operands, where the
			// fork keeps the sign-conversion warning and returns the wrapped value.
			return IsConstantSource(PowerCase) || !HasMixedIntegerSigns(PowerCase);
		}
		if (PowerCase.Scenario == EScenario::NegativeExponent
			&& IsCompileTimeKnownSource(PowerCase)
			&& IsUnsignedInteger(*PowerCase.BaseType)
			&& IsIntegerType(*PowerCase.ExponentType))
		{
			return true;
		}
		return !IsCompileTimeKnownSource(PowerCase) && IsIntegerResult(Kind);
	}

	static FString ExpectedBuildError(const FPowerCase& PowerCase)
	{
		if ((PowerCase.Scenario == EScenario::Overflow
			&& (IsConstantSource(PowerCase)
				|| (IsCompileTimeKnownSource(PowerCase) && !HasMixedIntegerSigns(PowerCase))))
			|| (PowerCase.Scenario == EScenario::NegativeExponent
				&& IsCompileTimeKnownSource(PowerCase)
				&& IsUnsignedInteger(*PowerCase.BaseType)
				&& IsIntegerType(*PowerCase.ExponentType)))
		{
			return TEXT("Overflow in exponent operation");
		}
		return TEXT("Cannot pow on integer values");
	}

	static bool ExpectedRuntimeOverflow(const FPowerCase& PowerCase)
	{
		return PowerCase.Scenario == EScenario::Overflow && !IsCompileTimeKnownSource(PowerCase)
			&& !IsIntegerResult(ResultKind(PowerCase));
	}

	static uint64 FloatBits(const float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static uint64 DoubleBits(const double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static uint64 ExpectedResultBits(const FPowerCase& PowerCase)
	{
		const EResultKind Kind = ResultKind(PowerCase);
		if (PowerCase.Scenario == EScenario::Overflow
			&& PowerCase.SourceShape->Shape == ESourceShape::ConstLvalue
			&& HasMixedIntegerSigns(PowerCase))
		{
			return Kind == EResultKind::Signed64 || Kind == EResultKind::Unsigned64
				? (uint64(1) << 63)
				: (uint64(1) << 31);
		}
		double Value = 0.0;
		switch (PowerCase.Scenario)
		{
		case EScenario::ZeroExponent:
		case EScenario::NearLimit:
			Value = 1.0;
			break;
		case EScenario::OneExponent:
			Value = 2.0;
			break;
		case EScenario::NegativeExponent:
			Value = IsIntegerResult(Kind) ? 0.0 : 0.25;
			break;
		case EScenario::FractionalExponent:
			Value = 2.0;
			break;
		default:
			break;
		}

		switch (Kind)
		{
		case EResultKind::Signed32:
			return static_cast<asDWORD>(static_cast<int32>(Value));
		case EResultKind::Unsigned32:
			return static_cast<asDWORD>(static_cast<asDWORD>(Value));
		case EResultKind::Signed64:
			return static_cast<asQWORD>(static_cast<asINT64>(Value));
		case EResultKind::Unsigned64:
			return static_cast<asQWORD>(Value);
		case EResultKind::Float32:
			return FloatBits(static_cast<float>(Value));
		case EResultKind::Float64:
			return DoubleBits(Value);
		default:
			return 0;
		}
	}

	static uint64 ReadReturnBits(asIScriptContext& Context, const EResultKind Kind)
	{
		switch (Kind)
		{
		case EResultKind::Signed32:
		case EResultKind::Unsigned32:
			return Context.GetReturnDWord();
		case EResultKind::Signed64:
		case EResultKind::Unsigned64:
			return Context.GetReturnQWord();
		case EResultKind::Float32:
			return FloatBits(Context.GetReturnFloat());
		case EResultKind::Float64:
			return DoubleBits(Context.GetReturnDouble());
		default:
			return 0;
		}
	}

	static bool ContainsOpcode(asIScriptFunction& Function, const asEBCInstr ExpectedOpcode)
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
			if (Opcode == ExpectedOpcode)
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

	static bool ContainsAnyPowerOpcode(asIScriptFunction& Function)
	{
		return ContainsOpcode(Function, asBC_POWi) || ContainsOpcode(Function, asBC_POWu)
			|| ContainsOpcode(Function, asBC_POWi64) || ContainsOpcode(Function, asBC_POWu64)
			|| ContainsOpcode(Function, asBC_POWf) || ContainsOpcode(Function, asBC_POWd)
			|| ContainsOpcode(Function, asBC_POWdi);
	}

	static asEBCInstr ExpectedFloatingOpcode(const FPowerCase& PowerCase)
	{
		if (ResultKind(PowerCase) == EResultKind::Float32)
		{
			return asBC_POWf;
		}
		return IsFloat64(*PowerCase.BaseType)
				&& (IsSignedInteger(*PowerCase.ExponentType)
					|| IsUnsignedInteger(*PowerCase.ExponentType))
			? asBC_POWdi
			: asBC_POWd;
	}

	static int CompileReportedSource(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const FString& SourceId,
		const FString& ModuleNameValue,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;
		PrintGeneratedAsSource(Test, SourceId, ModuleNameValue, Source);
		Engine.Reset(Test);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleNameValue);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), OutModule);
	}

	static bool ExecuteRecovery(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		asIScriptContext& Context,
		const FString& CaseId,
		const FString& ModuleNameValue)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptModule* RecoveryModule = nullptr;
		const FString RecoverySource = BuildRecoverySource();
		const int BuildResult = CompileReportedSource(
			Test, Engine, CaseId + TEXT("-RECOVERY"), ModuleNameValue, RecoverySource, RecoveryModule);
		bool bPassed = Assert.IsTrue(BuildResult >= 0,
			*FString::Printf(TEXT("[%s] same-name power recovery should compile. Messages={%s}"),
				*CaseId,
				*Engine.GetMessagesText()));
		bPassed &= Assert.IsNotNull(
			RecoveryModule, TEXT("power recovery should publish its replacement module"));
		if (BuildResult >= 0 && RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				GetNativeFunctionByExactDecl(RecoveryModule, "int EvaluatePower()");
			bPassed &= Assert.IsNotNull(
				Recovery, TEXT("power recovery should expose its exact replacement function"));
			if (Recovery != nullptr)
			{
				const int ExecuteResult = PrepareAndExecute(&Context, Recovery);
				bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
					ExecuteResult,
					TEXT("power recovery should execute through the faulted context"));
				bPassed &= Assert.AreEqual(17,
					static_cast<int32>(Context.GetReturnDWord()),
					TEXT("power recovery should return its independent sentinel"));
				bPassed &= Assert.AreEqual(asSUCCESS,
					Context.Unprepare(),
					TEXT("power recovery context should unprepare"));
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleNameValue);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		bPassed &= Assert.IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("same-name power recovery module should discard"));
		return bPassed;
	}

	static bool VerifyBuildFailure(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FPowerCase& PowerCase,
		const FString& ModuleNameValue,
		const FString& Source,
		const int BuildResult)
	{
		FNoDiscardAsserter Assert(Test);
		bool bPassed = Assert.IsTrue(BuildResult < 0,
			*Case.DescribeResult("<power build>",
				TEXT("negative build result"),
				FString::Printf(TEXT("%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		const TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> Errors =
			ErrorMessages(Engine.GetMessages());
		bPassed &= Assert.AreEqual(1,
			Errors.Num(),
			*Case.Describe(TEXT("power rejection should report one causal diagnostic")));
		const int32 CauseLine = LastSourceLineContaining(Source, TEXT("POWER_CAUSE"));
		bPassed &= Assert.IsTrue(CauseLine > 0,
			*Case.Describe(TEXT("power rejection source should retain its marked operator line")));
		if (Errors.Num() == 1)
		{
			bPassed &= Assert.AreEqual(CauseLine,
				Errors[0].Row,
				*Case.Describe(TEXT("power rejection should own the marked operator line")));
			bPassed &= Assert.IsTrue(Errors[0].Message.Contains(ExpectedBuildError(PowerCase)),
				*Case.DescribeResult("<power rejection text>",
					ExpectedBuildError(PowerCase),
					Errors[0].Message));
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleNameValue);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		bPassed &= Assert.IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("failed power source should discard before same-name recovery"));
		bPassed &= ExecuteRecovery(Test, Engine, Context, Case.GetId(), ModuleNameValue);
		return bPassed;
	}

	static bool ExecuteFollowUp(FAutomationTestBase& Test,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const FollowUp = GetNativeFunctionByExactDecl(&Module, "int PowerFollowUp()");
		if (!Assert.IsNotNull(FollowUp, *Case.Describe(TEXT("power follow-up should resolve exactly"))))
		{
			return false;
		}
		bool bPassed = Assert.AreEqual(asSUCCESS,
			Context.Prepare(FollowUp),
			*Case.Describe(TEXT("power follow-up should prepare on the reused context")));
		const int ExecuteResult = bPassed ? Context.Execute() : asERROR;
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("power follow-up should execute after the selected outcome")));
		bPassed &= Assert.AreEqual(17,
			static_cast<int32>(Context.GetReturnDWord()),
			*Case.Describe(TEXT("power follow-up should retain an independent result")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("power follow-up should unprepare the reused context")));
		return bPassed;
	}

	static bool VerifySuccessfulCase(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FPowerCase& PowerCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		const EResultKind Kind = ResultKind(PowerCase);
		const FString ReturnTypeName = ResultType(Kind, *Engine.Get());
		const FString Declaration = FString::Printf(TEXT("%s EvaluatePower()"), *ReturnTypeName);
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asIScriptFunction* const Evaluate =
			GetNativeFunctionByExactDecl(&Module, DeclarationUtf8.Get());
		bool bPassed = Assert.IsNotNull(
			Evaluate, *Case.Describe(TEXT("power evaluator should publish its exact declaration")));
		bPassed &= Assert.IsTrue(HasNoErrors(Engine.GetMessages()),
			*Case.Describe(TEXT("successful power source should not report compile errors")));
		if (Evaluate == nullptr)
		{
			return false;
		}

		const ANSICHAR* const PublicTypeName = PublicResultTypeName(Kind);
		const int ExpectedReturnTypeId = PublicTypeName != nullptr
			? Engine.Get()->GetTypeIdByDecl(PublicTypeName)
			: Engine.Get()->GetTypeIdByDecl(TCHAR_TO_UTF8(*ReturnTypeName));
		const int ActualReturnTypeId = Evaluate->GetReturnTypeId();
		if (ExpectedReturnTypeId != ActualReturnTypeId)
		{
			Test.AddInfo(*Case.Describe(*FString::Printf(
				TEXT("power return metadata trace: expectedSpelling='%s' expectedTypeId=%d actualTypeId=%d declaration='%hs' floatIsFloat64=%d"),
				*ReturnTypeName,
				ExpectedReturnTypeId,
				ActualReturnTypeId,
				Evaluate->GetDeclaration(),
				Engine.Get()->GetEngineProperty(asEP_FLOAT_IS_FLOAT64))));
		}
		bPassed &= Assert.AreEqual(ExpectedReturnTypeId,
			ActualReturnTypeId,
			*Case.Describe(TEXT("power evaluator should preserve the independently promoted result type")));
		if (IsCompileTimeKnownSource(PowerCase))
		{
			bPassed &= Assert.IsFalse(ContainsAnyPowerOpcode(*Evaluate),
				*Case.Describe(TEXT("constant power source should fold without a runtime power opcode")));
		}
		else if (PowerCase.SourceShape->Shape == ESourceShape::MutableLvalue
			|| PowerCase.SourceShape->Shape == ESourceShape::FunctionReturn)
		{
			if (!IsIntegerResult(Kind))
			{
				bPassed &= Assert.IsTrue(ContainsOpcode(*Evaluate, ExpectedFloatingOpcode(PowerCase)),
					*Case.Describe(TEXT("runtime floating power should emit its fork bytecode route")));
			}
		}

		const bool bPrepared = Assert.AreEqual(asSUCCESS,
			Context.Prepare(Evaluate),
			*Case.Describe(TEXT("power evaluator should prepare")));
		const int ExecuteResult = bPrepared ? Context.Execute() : asERROR;
		if (ExpectedRuntimeOverflow(PowerCase))
		{
			bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION),
				ExecuteResult,
				*Case.Describe(TEXT("runtime floating power overflow should raise an execution exception")));
			bPassed &= Assert.AreEqual(FString(TEXT("Overflow in exponent operation")),
				FString(UTF8_TO_TCHAR(Context.GetExceptionString() != nullptr ? Context.GetExceptionString() : "")),
				*Case.Describe(TEXT("runtime floating power overflow should preserve its exact exception")));
			bPassed &= Assert.IsNotNull(Context.GetExceptionFunction(),
				*Case.Describe(TEXT("runtime floating power overflow should retain its owner function")));
			bPassed &= Assert.IsTrue(Context.GetExceptionLineNumber() > 0,
				*Case.Describe(TEXT("runtime floating power overflow should retain a source line")));
		}
		else
		{
			const uint64 ActualBits = ReadReturnBits(Context, Kind);
			const uint64 ExpectedBits = ExpectedResultBits(PowerCase);
			bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				ExecuteResult,
				*Case.Describe(TEXT("power evaluator should execute")));
			bPassed &= Assert.AreEqual(ExpectedBits,
				ActualBits,
				*Case.DescribeResult(DeclarationUtf8.Get(),
					FString::Printf(TEXT("bits=0x%016llX"), ExpectedBits),
					FString::Printf(TEXT("bits=0x%016llX"), ActualBits)));
		}
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("power evaluator context should unprepare")));
		bPassed &= ExecuteFollowUp(Test, Module, Context, Case);
		return bPassed;
	}

	static bool ExecuteCase(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		asIScriptContext& Context,
		const FString& ProductId,
		const FPowerCase& PowerCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FNativeCaseContext Case(MakeCaseId(ProductId, PowerCase));
		const FString CurrentModuleName = ModuleName(ProductId, PowerCase);
		const FString Source = BuildPowerSource(*Engine.Get(), PowerCase);
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileReportedSource(
			Test, Engine, Case.GetId(), CurrentModuleName, Source, Module);
		if (ExpectedBuildFailure(PowerCase))
		{
			return VerifyBuildFailure(
				Test, Engine, Context, Case, PowerCase, CurrentModuleName, Source, BuildResult);
		}

		FNoDiscardAsserter Assert(Test);
		bool bPassed = Assert.IsTrue(BuildResult >= 0,
			*Case.DescribeResult("<power build>",
				TEXT("successful build"),
				FString::Printf(TEXT("%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		bPassed &= Assert.IsNotNull(Module,
			*Case.Describe(TEXT("successful power source should publish its module")));
		if (BuildResult >= 0 && Module != nullptr)
		{
			bPassed &= VerifySuccessfulCase(Test, Engine, *Module, Context, Case, PowerCase);
		}
		const FTCHARToUTF8 ModuleNameUtf8(*CurrentModuleName);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		bPassed &= Assert.IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("successful power module should discard")));
		return bPassed;
	}

	static bool RunProduct(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		asIScriptContext& Context,
		const FString& ProductId,
		const TArray<FPowerCase>& Cases,
		const int32 ExpectedCaseCount)
	{
		TSet<FString> UniqueIds;
		FNoDiscardAsserter Assert(Test);
		bool bAllCasesPassed = true;
		for (const FPowerCase& PowerCase : Cases)
		{
			const FString CaseId = MakeCaseId(ProductId, PowerCase);
			const bool bUniqueCaseId = !UniqueIds.Contains(CaseId);
			UniqueIds.Add(CaseId);
			bAllCasesPassed &= Assert.IsTrue(bUniqueCaseId,
				*FString::Printf(TEXT("[%s] power case ID should be unique"), *CaseId));
			bAllCasesPassed &= ExecuteCase(Test, Engine, Context, ProductId, PowerCase);
		}

		bAllCasesPassed &= Assert.AreEqual(ExpectedCaseCount,
			Cases.Num(),
			TEXT("power product should construct every planned case"));
		bAllCasesPassed &= Assert.AreEqual(Cases.Num(),
			UniqueIds.Num(),
			TEXT("power product should construct no duplicate IDs"));
		return bAllCasesPassed;
	}

	static void AddCasesForTypes(TArray<FPowerCase>& OutCases,
		const TArray<const FNativeTypeCase*>& BaseTypes,
		const TArray<const FNativeTypeCase*>& ExponentTypes,
		const TArray<EScenario>& Scenarios)
	{
		for (const FNativeTypeCase* const BaseType : BaseTypes)
		{
			for (const FNativeTypeCase* const ExponentType : ExponentTypes)
			{
				for (const FSourceShapeCase& SourceShape : SourceShapeCases)
				{
					for (const EScenario Scenario : Scenarios)
					{
						OutCases.Add({BaseType, ExponentType, &SourceShape, Scenario});
					}
				}
			}
		}
	}

	static TArray<const FNativeTypeCase*> NumericTypes()
	{
		TArray<const FNativeTypeCase*> Types;
		for (const FNativeTypeCase& TypeCase : AngelscriptNativeTestSupport::NativeTypeCases)
		{
			if (IsNumeric(TypeCase))
			{
				Types.Add(&TypeCase);
			}
		}
		return Types;
	}

	static TArray<const FNativeTypeCase*> SignedOrFloatingTypes()
	{
		TArray<const FNativeTypeCase*> Types;
		for (const FNativeTypeCase& TypeCase : AngelscriptNativeTestSupport::NativeTypeCases)
		{
			if (IsSignedInteger(TypeCase) || IsFloat32(TypeCase) || IsFloat64(TypeCase))
			{
				Types.Add(&TypeCase);
			}
		}
		return Types;
	}

	static TArray<const FNativeTypeCase*> FloatingTypes()
	{
		TArray<const FNativeTypeCase*> Types;
		for (const FNativeTypeCase& TypeCase : AngelscriptNativeTestSupport::NativeTypeCases)
		{
			if (IsFloat32(TypeCase) || IsFloat64(TypeCase))
			{
				Types.Add(&TypeCase);
			}
		}
		return Types;
	}

public:
	TEST_METHOD(UniversalTypesBySourceShapeAndValue)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-POWER-UNIVERSAL",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Runtime
				| ENativeEvidence::Metadata | ENativeEvidence::Bytecode | ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		asIScriptContext* const Context =
			ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		if (ScriptEngine == nullptr || Context == nullptr)
		{
			TestRunner->AddError(TEXT("Power product should create a case-owned raw SDK engine and context"));
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		TArray<FPowerCase> Cases;
		const TArray<const FNativeTypeCase*> Types = NumericTypes();
		AddCasesForTypes(Cases,
			Types,
			Types,
			{EScenario::ZeroExponent, EScenario::OneExponent, EScenario::NearLimit, EScenario::Overflow});
		ASSERT_THAT(IsTrue(RunProduct(*TestRunner,
			NativeEngine,
			*Context,
			TEXT("LANG-OP-POWER-UNIVERSAL"),
			Cases,
			1600),
			TEXT("every universal power product case should satisfy its selected fork evidence")));
	}

	TEST_METHOD(NegativeExponentTypesBySourceShape)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-POWER-NEGATIVE-EXPONENT",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Runtime
				| ENativeEvidence::Metadata | ENativeEvidence::Bytecode | ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		asIScriptContext* const Context =
			ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		if (ScriptEngine == nullptr || Context == nullptr)
		{
			TestRunner->AddError(TEXT("Power product should create a case-owned raw SDK engine and context"));
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		TArray<FPowerCase> Cases;
		AddCasesForTypes(Cases,
			NumericTypes(),
			SignedOrFloatingTypes(),
			{EScenario::NegativeExponent});
		ASSERT_THAT(IsTrue(RunProduct(*TestRunner,
			NativeEngine,
			*Context,
			TEXT("LANG-OP-POWER-NEGATIVE-EXPONENT"),
			Cases,
			240),
			TEXT("every negative-exponent power product case should satisfy its selected fork evidence")));
	}

	TEST_METHOD(FractionalExponentTypesBySourceShape)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-POWER-FRACTIONAL-EXPONENT",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata
				| ENativeEvidence::Bytecode | ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		asIScriptContext* const Context =
			ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		if (ScriptEngine == nullptr || Context == nullptr)
		{
			TestRunner->AddError(TEXT("Power product should create a case-owned raw SDK engine and context"));
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		TArray<FPowerCase> Cases;
		AddCasesForTypes(Cases,
			NumericTypes(),
			FloatingTypes(),
			{EScenario::FractionalExponent});
		ASSERT_THAT(IsTrue(RunProduct(*TestRunner,
			NativeEngine,
			*Context,
			TEXT("LANG-OP-POWER-FRACTIONAL-EXPONENT"),
			Cases,
			80),
			TEXT("every fractional-exponent power product case should satisfy its selected fork evidence")));
	}
};

#endif
