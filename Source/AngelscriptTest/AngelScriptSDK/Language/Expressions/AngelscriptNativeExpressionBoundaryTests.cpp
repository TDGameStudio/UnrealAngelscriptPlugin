#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExpressionBoundaryTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.Boundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class EBoundaryScenario : uint8
	{
		ParenthesesOne,
		ParenthesesEight,
		ParenthesesSixtyFour,
		ChainOne,
		ChainEight,
		ChainThirtyTwo,
		ArgumentsZero,
		ArgumentsOne,
		ArgumentsMany,
		NumericMinimum,
		NumericMaximum,
		Whitespace,
		Comments,
		Multiline,
	};

	enum class EBoundaryContext : uint8
	{
		Initializer,
		Argument,
		Return,
		Condition,
		Index,
	};

	enum class EBoundaryBuild : uint8
	{
		First,
		SameRebuild,
		ChangedRebuild,
	};

	struct FBoundaryScenarioCase
	{
		const ANSICHAR* CatalogName;
		EBoundaryScenario Scenario;
	};

	struct FBoundaryContextCase
	{
		const ANSICHAR* CatalogName;
		EBoundaryContext Context;
	};

	struct FBoundaryBuildCase
	{
		const ANSICHAR* CatalogName;
		EBoundaryBuild Build;
	};

	struct FBoundaryWorkflowState
	{
		FString ModuleName;
		FString FirstSource;
		FString FirstBytecodeStructure;
		FString FirstDeclaration;
		FString FirstSection;
		int64 FirstResult = 0;
		int32 FirstFunctionId = asNO_FUNCTION;
		asUINT FirstBytecodeLength = 0;
		asIScriptFunction* RetainedFirstFunction = nullptr;
	};

	inline static constexpr FBoundaryScenarioCase ScenarioCases[] = {
		{"parentheses_one", EBoundaryScenario::ParenthesesOne},
		{"parentheses_eight", EBoundaryScenario::ParenthesesEight},
		{"parentheses_sixty_four", EBoundaryScenario::ParenthesesSixtyFour},
		{"chain_one", EBoundaryScenario::ChainOne},
		{"chain_eight", EBoundaryScenario::ChainEight},
		{"chain_thirty_two", EBoundaryScenario::ChainThirtyTwo},
		{"arguments_zero", EBoundaryScenario::ArgumentsZero},
		{"arguments_one", EBoundaryScenario::ArgumentsOne},
		{"arguments_many", EBoundaryScenario::ArgumentsMany},
		{"numeric_minimum", EBoundaryScenario::NumericMinimum},
		{"numeric_maximum", EBoundaryScenario::NumericMaximum},
		{"whitespace", EBoundaryScenario::Whitespace},
		{"comments", EBoundaryScenario::Comments},
		{"multiline", EBoundaryScenario::Multiline},
	};

	inline static constexpr FBoundaryContextCase ContextCases[] = {
		{"initializer", EBoundaryContext::Initializer},
		{"argument", EBoundaryContext::Argument},
		{"return", EBoundaryContext::Return},
		{"condition", EBoundaryContext::Condition},
		{"index", EBoundaryContext::Index},
	};

	inline static constexpr FBoundaryBuildCase BuildCases[] = {
		{"first", EBoundaryBuild::First},
		{"same_rebuild", EBoundaryBuild::SameRebuild},
		{"changed_rebuild", EBoundaryBuild::ChangedRebuild},
	};

	static bool IsChainScenario(const FBoundaryScenarioCase& ScenarioCase)
	{
		return ScenarioCase.Scenario == EBoundaryScenario::ChainOne ||
			   ScenarioCase.Scenario == EBoundaryScenario::ChainEight ||
			   ScenarioCase.Scenario == EBoundaryScenario::ChainThirtyTwo;
	}

	static bool IsArgumentScenario(const FBoundaryScenarioCase& ScenarioCase)
	{
		return ScenarioCase.Scenario == EBoundaryScenario::ArgumentsZero ||
			   ScenarioCase.Scenario == EBoundaryScenario::ArgumentsOne ||
			   ScenarioCase.Scenario == EBoundaryScenario::ArgumentsMany;
	}

	static int32 ParenthesesDepth(const FBoundaryScenarioCase& ScenarioCase)
	{
		switch (ScenarioCase.Scenario)
		{
		case EBoundaryScenario::ParenthesesOne:
			return 1;
		case EBoundaryScenario::ParenthesesEight:
			return 8;
		case EBoundaryScenario::ParenthesesSixtyFour:
			return 64;
		default:
			return 0;
		}
	}

	static int32 ChainDepth(const FBoundaryScenarioCase& ScenarioCase)
	{
		switch (ScenarioCase.Scenario)
		{
		case EBoundaryScenario::ChainOne:
			return 1;
		case EBoundaryScenario::ChainEight:
			return 8;
		case EBoundaryScenario::ChainThirtyTwo:
			return 32;
		default:
			return 0;
		}
	}

	static int32 ArgumentCount(const FBoundaryScenarioCase& ScenarioCase)
	{
		switch (ScenarioCase.Scenario)
		{
		case EBoundaryScenario::ArgumentsZero:
			return 0;
		case EBoundaryScenario::ArgumentsOne:
			return 1;
		case EBoundaryScenario::ArgumentsMany:
			return 16;
		default:
			return INDEX_NONE;
		}
	}

	static int64 VersionSeed(const FBoundaryBuildCase& BuildCase)
	{
		return BuildCase.Build == EBoundaryBuild::ChangedRebuild ? 58 : 41;
	}

	static int64 ExpectedBoundaryResult(
		const FBoundaryScenarioCase& ScenarioCase, const FBoundaryBuildCase& BuildCase)
	{
		const int64 Seed = VersionSeed(BuildCase);
		switch (ScenarioCase.Scenario)
		{
		case EBoundaryScenario::ChainOne:
		case EBoundaryScenario::ChainEight:
		case EBoundaryScenario::ChainThirtyTwo:
			return Seed + ChainDepth(ScenarioCase);
		case EBoundaryScenario::ArgumentsMany:
			return Seed + 120;
		case EBoundaryScenario::NumericMinimum:
			return BuildCase.Build == EBoundaryBuild::ChangedRebuild
					   ? static_cast<int64>(MIN_int32) + 17
					   : static_cast<int64>(MIN_int32);
		case EBoundaryScenario::NumericMaximum:
			return BuildCase.Build == EBoundaryBuild::ChangedRebuild
					   ? static_cast<int64>(MAX_int32) - 17
					   : static_cast<int64>(MAX_int32);
		case EBoundaryScenario::Whitespace:
		case EBoundaryScenario::Comments:
		case EBoundaryScenario::Multiline:
			return Seed + 3;
		default:
			return Seed;
		}
	}

	static FString ParenthesizedExpression(const int32 Depth, const int64 Seed)
	{
		FString Expression = FString::Printf(TEXT("int64(%lld)"), static_cast<long long>(Seed));
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Expression = TEXT("(") + Expression + TEXT(")");
		}
		return Expression;
	}

	static FString ChainExpression(const int32 Depth, const int64 Seed)
	{
		FString Expression =
			FString::Printf(TEXT("MakeBoundaryChain(%lld)"), static_cast<long long>(Seed));
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Expression += TEXT(".Step()");
		}
		Expression += TEXT(".Value");
		return Expression;
	}

	static FString ManyArgumentExpression(const int64 Seed)
	{
		FString Expression =
			FString::Printf(TEXT("BoundaryArgumentsMany(%lld"), static_cast<long long>(Seed));
		for (int32 Value = 1; Value < 16; ++Value)
		{
			Expression += FString::Printf(TEXT(", %d"), Value);
		}
		Expression += TEXT(")");
		return Expression;
	}

	static FString BoundaryExpression(
		const FBoundaryScenarioCase& ScenarioCase, const FBoundaryBuildCase& BuildCase)
	{
		const int64 Seed = VersionSeed(BuildCase);
		const int32 Parentheses = ParenthesesDepth(ScenarioCase);
		if (Parentheses > 0)
		{
			return ParenthesizedExpression(Parentheses, Seed);
		}
		const int32 Chain = ChainDepth(ScenarioCase);
		if (Chain > 0)
		{
			return ChainExpression(Chain, Seed);
		}

		switch (ScenarioCase.Scenario)
		{
		case EBoundaryScenario::ArgumentsZero:
			return TEXT("BoundaryArgumentsZero()");
		case EBoundaryScenario::ArgumentsOne:
			return FString::Printf(
				TEXT("BoundaryArgumentsOne(%lld)"), static_cast<long long>(Seed));
		case EBoundaryScenario::ArgumentsMany:
			return ManyArgumentExpression(Seed);
		case EBoundaryScenario::NumericMinimum:
			return BuildCase.Build == EBoundaryBuild::ChangedRebuild
					   ? TEXT("int64(-2147483647 - 1) + 17")
					   : TEXT("int64(-2147483647 - 1)");
		case EBoundaryScenario::NumericMaximum:
			return BuildCase.Build == EBoundaryBuild::ChangedRebuild
					   ? TEXT("int64(2147483647) - 17")
					   : TEXT("int64(2147483647)");
		case EBoundaryScenario::Whitespace:
			return FString::Printf(TEXT("int64(  %lld\t+\t3  )"), static_cast<long long>(Seed));
		case EBoundaryScenario::Comments:
			return FString::Printf(
				TEXT("int64(%lld /* seed */ + /* delta */ 3)"), static_cast<long long>(Seed));
		case EBoundaryScenario::Multiline:
			{
				FString MultilineExpression = TEXT("int64(");
				MultilineExpression.AppendChar(TEXT('\n'));
				MultilineExpression += TEXT("\t\t");
				MultilineExpression += FString::Printf(TEXT("%lld"), static_cast<long long>(Seed));
				MultilineExpression.AppendChar(TEXT('\n'));
				MultilineExpression += TEXT("\t\t+");
				MultilineExpression.AppendChar(TEXT('\n'));
				MultilineExpression += TEXT("\t\t3");
				MultilineExpression.AppendChar(TEXT('\n'));
				MultilineExpression += TEXT("\t)");
				return MultilineExpression;
			}
		default:
			return FString::Printf(TEXT("int64(%lld)"), static_cast<long long>(Seed));
		}
	}

	static void AppendBoundaryChainDeclaration(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryChain"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint64 Value = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryChain()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryChain(int64 InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFBoundaryChain Step() const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FBoundaryChain(Value + 1);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("FBoundaryChain MakeBoundaryChain(int64 Seed)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FBoundaryChain(Seed);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString ManyArgumentDeclaration()
	{
		FString Declaration = TEXT("int64 BoundaryArgumentsMany(");
		for (int32 Index = 0; Index < 16; ++Index)
		{
			if (Index > 0)
			{
				Declaration += TEXT(", ");
			}
			Declaration += FString::Printf(TEXT("int64 A%d"), Index);
		}
		Declaration += TEXT(")");
		return Declaration;
	}

	static void AppendBoundaryArgumentDeclaration(FString& Source,
		const FBoundaryScenarioCase& ScenarioCase,
		const FBoundaryBuildCase& BuildCase)
	{
		using namespace AngelscriptNativeTestSupport;

		switch (ScenarioCase.Scenario)
		{
		case EBoundaryScenario::ArgumentsZero:
			AppendGeneratedAsLine(Source, TEXT("int64 BoundaryArgumentsZero()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("\treturn %lld;"), static_cast<long long>(VersionSeed(BuildCase))));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			break;
		case EBoundaryScenario::ArgumentsOne:
			AppendGeneratedAsLine(Source, TEXT("int64 BoundaryArgumentsOne(int64 A0)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn A0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			break;
		case EBoundaryScenario::ArgumentsMany:
			AppendGeneratedAsLine(Source, ManyArgumentDeclaration());
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn A0 + A1 + A2 + A3 + A4 + A5 + A6 + A7"));
			AppendGeneratedAsLine(
				Source, TEXT("\t\t+ A8 + A9 + A10 + A11 + A12 + A13 + A14 + A15;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			break;
		default:
			break;
		}
	}

	static void AppendBoundaryContextHelpers(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int64 ObserveBoundary(int64 Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FBoundaryIndex"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint64 opIndex(int64 Index) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Index;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendBoundaryEntry(FString& Source,
		const FBoundaryScenarioCase& ScenarioCase,
		const FBoundaryContextCase& ContextCase,
		const FBoundaryBuildCase& BuildCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Expression = BoundaryExpression(ScenarioCase, BuildCase);
		const int64 Expected = ExpectedBoundaryResult(ScenarioCase, BuildCase);
		if (ContextCase.Context == EBoundaryContext::Return)
		{
			AppendGeneratedAsLine(Source, TEXT("int64 ProduceBoundary()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int64 RunExpressionBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (ContextCase.Context)
		{
		case EBoundaryContext::Initializer:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tint64 Result = %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
			break;
		case EBoundaryContext::Argument:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn ObserveBoundary(%s);"), *Expression));
			break;
		case EBoundaryContext::Return:
			AppendGeneratedAsLine(Source, TEXT("\treturn ProduceBoundary();"));
			break;
		case EBoundaryContext::Condition:
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("\tif (%s == %lld)"), *Expression, static_cast<long long>(Expected)));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t\treturn %lld;"), static_cast<long long>(Expected)));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			break;
		case EBoundaryContext::Index:
			AppendGeneratedAsLine(Source, TEXT("\tFBoundaryIndex Probe;"));
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn Probe[%s];"), *Expression));
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int64 RecoverExpressionBoundary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 181;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildExpressionBoundarySource(const FBoundaryScenarioCase& ScenarioCase,
		const FBoundaryContextCase& ContextCase,
		const FBoundaryBuildCase& BuildCase)
	{
		FString Source;
		if (IsChainScenario(ScenarioCase))
		{
			AppendBoundaryChainDeclaration(Source);
		}
		if (IsArgumentScenario(ScenarioCase))
		{
			AppendBoundaryArgumentDeclaration(Source, ScenarioCase, BuildCase);
		}
		AppendBoundaryContextHelpers(Source);
		AppendBoundaryEntry(Source, ScenarioCase, ContextCase, BuildCase);
		return Source;
	}

	asIScriptModule* CompileAndReport(FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int32 BuildResult =
			CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		FNoDiscardAsserter LocalAssert(*TestRunner);
		const bool bBuildSucceeded = LocalAssert.IsTrue(BuildResult >= 0,
			*Case.DescribeResult("<module build>",
				TEXT("successful boundary compilation"),
				Engine.GetMessagesText()));
		const bool bModulePublished = LocalAssert.IsNotNull(
			Module,
			*Case.Describe(TEXT("boundary generation should publish its module")));
		return bBuildSucceeded && bModulePublished ? Module : nullptr;
	}

	static asIScriptFunction* ExpressionOwnerFunction(
		asIScriptModule& Module, const FBoundaryContextCase& ContextCase)
	{
		return ContextCase.Context == EBoundaryContext::Return
				   ? Module.GetFunctionByDecl("int64 ProduceBoundary()")
				   : Module.GetFunctionByDecl("int64 RunExpressionBoundary()");
	}

	static bool CaptureOpcodeSequence(asIScriptFunction& Function, TArray<asBYTE>& OutOpcodes)
	{
		OutOpcodes.Reset();
		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode = Function.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT Offset = 0;
		while (Offset < BytecodeLength)
		{
			const asBYTE Opcode = *(reinterpret_cast<const asBYTE*>(&Bytecode[Offset]));
			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0 ||
				Offset + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				OutOpcodes.Reset();
				return false;
			}
			OutOpcodes.Add(Opcode);
			Offset += static_cast<asUINT>(InstructionSize);
		}
		return Offset == BytecodeLength;
	}

	static FString CaptureModuleBytecodeStructure(asIScriptModule& Module)
	{
		TArray<FString> FunctionStructures;
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function == nullptr)
			{
				continue;
			}
			TArray<asBYTE> Opcodes;
			if (!CaptureOpcodeSequence(*Function, Opcodes))
			{
				return FString();
			}
			FString Structure = UTF8_TO_TCHAR(Function->GetDeclaration(true, true, false));
			Structure += TEXT("|");
			for (const asBYTE Opcode : Opcodes)
			{
				Structure += FString::Printf(TEXT("%u,"), static_cast<uint32>(Opcode));
			}
			FunctionStructures.Add(MoveTemp(Structure));
		}
		FunctionStructures.Sort();
		FString LineSeparator;
		LineSeparator.AppendChar(TEXT('\n'));
		return FString::Join(FunctionStructures, *LineSeparator);
	}

	static int32 CountOpcode(asIScriptFunction& Function, const asBYTE ExpectedOpcode)
	{
		TArray<asBYTE> Opcodes;
		if (!CaptureOpcodeSequence(Function, Opcodes))
		{
			return 0;
		}
		int32 Count = 0;
		for (const asBYTE Opcode : Opcodes)
		{
			if (Opcode == ExpectedOpcode)
			{
				++Count;
			}
		}
		return Count;
	}

	static int32 CountCalls(asIScriptFunction& Function)
	{
		TArray<asBYTE> Opcodes;
		if (!CaptureOpcodeSequence(Function, Opcodes))
		{
			return 0;
		}
		int32 Count = 0;
		for (const asBYTE Opcode : Opcodes)
		{
			if (Opcode == asBC_CALL || Opcode == asBC_CALLINTF || Opcode == asBC_CALLSYS ||
				Opcode == asBC_CALLBND)
			{
				++Count;
			}
		}
		return Count;
	}

	static bool HasBranch(asIScriptFunction& Function)
	{
		return CountOpcode(Function, asBC_JZ) > 0 || CountOpcode(Function, asBC_JNZ) > 0 ||
			   CountOpcode(Function, asBC_JLowZ) > 0 || CountOpcode(Function, asBC_JLowNZ) > 0;
	}

	void VerifyBoundaryMetadata(const FNativeCaseContext& Case,
		const FBoundaryScenarioCase& ScenarioCase,
		const FBoundaryContextCase& ContextCase,
		asIScriptEngine& Engine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int64 RunExpressionBoundary()");
		asIScriptFunction* const Owner = ExpressionOwnerFunction(Module, ContextCase);
		ASSERT_THAT(IsNotNull(
			Entry, *Case.Describe(TEXT("boundary module should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(
			Owner, *Case.Describe(TEXT("boundary expression should retain an owning function"))));
		if (Entry == nullptr || Owner == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(Engine.GetTypeIdByDecl("int64"),
			Entry->GetReturnTypeId(),
			*Case.Describe(TEXT("boundary entry should retain its exact return type"))));
		ASSERT_THAT(AreEqual(0,
			static_cast<int32>(Entry->GetParamCount()),
			*Case.Describe(TEXT("boundary entry should retain its exact arity"))));
		ASSERT_THAT(
			IsTrue(FString(UTF8_TO_TCHAR(Owner->GetScriptSectionName())) == Module.GetName(),
				*Case.Describe(TEXT("boundary owner should retain its generated section"))));
		// The owner is emitted after generated declarations, so a fixed absolute
		// line cannot be used as a portable lower bound. The fork's stable query
		// contract is that an out-of-range line is rejected; bytecode presence
		// proves that executable debug metadata exists for the owner.
		ASSERT_THAT(IsTrue(Owner->FindNextLineWithCode(100000) < 0,
			*Case.Describe(TEXT("boundary owner should reject an out-of-range debug line"))));
		asUINT OwnerBytecodeLength = 0;
		ASSERT_THAT(IsNotNull(Owner->GetByteCode(&OwnerBytecodeLength),
			*Case.Describe(TEXT("boundary owner should retain executable bytecode metadata"))));
		ASSERT_THAT(IsTrue(OwnerBytecodeLength > 0,
			*Case.Describe(TEXT("boundary owner should retain a non-empty bytecode range"))));

		if (IsArgumentScenario(ScenarioCase))
		{
			const int32 Count = ArgumentCount(ScenarioCase);
			FString Declaration;
			if (Count == 0)
			{
				Declaration = TEXT("int64 BoundaryArgumentsZero()");
			}
			else if (Count == 1)
			{
				Declaration = TEXT("int64 BoundaryArgumentsOne(int64 A0)");
			}
			else
			{
				Declaration = TEXT("int64 BoundaryArgumentsMany(");
				for (int32 Index = 0; Index < Count; ++Index)
				{
					if (Index > 0)
					{
						Declaration += TEXT(", ");
					}
					Declaration += FString::Printf(TEXT("int64 A%d"), Index);
				}
				Declaration += TEXT(")");
			}
			asIScriptFunction* const Target = Module.GetFunctionByDecl(TCHAR_TO_ANSI(*Declaration));
			ASSERT_THAT(IsTrue(
				Target != nullptr && static_cast<int32>(Target->GetParamCount()) == Count,
				*Case.Describe(TEXT("argument boundary should retain its exact target arity"))));
		}

		if (IsChainScenario(ScenarioCase))
		{
			asITypeInfo* const ChainType = Module.GetTypeInfoByDecl("FBoundaryChain");
			ASSERT_THAT(IsTrue(
				ChainType != nullptr &&
					ChainType->GetMethodByDecl("FBoundaryChain Step() const") != nullptr,
				*Case.Describe(TEXT("chain boundary should retain its exact step declaration"))));
			ASSERT_THAT(IsTrue(CountCalls(*Owner) >= ChainDepth(ScenarioCase),
				*Case.Describe(TEXT("chain boundary bytecode should retain every step call"))));
		}
		if (ContextCase.Context == EBoundaryContext::Condition)
		{
			TArray<asBYTE> ConditionOpcodes;
			ASSERT_THAT(IsTrue(CaptureOpcodeSequence(*Owner, ConditionOpcodes) &&
							ConditionOpcodes.Num() > 0,
				*Case.Describe(TEXT("condition boundary should retain executable bytecode"))));
		}
		if (ContextCase.Context == EBoundaryContext::Index)
		{
			ASSERT_THAT(IsTrue(CountCalls(*Owner) > 0,
				*Case.Describe(TEXT("index boundary should retain its operator call"))));
		}
	}

	int64 ExecuteBoundaryEntry(
		const FNativeCaseContext& Case, asIScriptEngine& Engine, asIScriptModule& Module)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter LocalAssert(*TestRunner);

		asIScriptFunction* const Entry =
			GetNativeFunctionByExactDecl(&Module, "int64 RunExpressionBoundary()");
		asIScriptFunction* const Recovery =
			GetNativeFunctionByExactDecl(&Module, "int64 RecoverExpressionBoundary()");
		const bool bEntryResolved = LocalAssert.IsNotNull(
			Entry,
			*Case.Describe(TEXT("boundary execution should resolve its exact entry")));
		const bool bRecoveryResolved = LocalAssert.IsNotNull(
			Recovery,
			*Case.Describe(TEXT("boundary execution should resolve its recovery")));
		if (!bEntryResolved || !bRecoveryResolved)
		{
			return MAX_int64;
		}
		asIScriptContext* const Context = Engine.CreateContext();
		if (!LocalAssert.IsNotNull(
			Context,
			*Case.Describe(TEXT("boundary execution should create a context"))))
		{
			return MAX_int64;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (!LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("boundary entry should execute to completion"))))
		{
			return MAX_int64;
		}
		const int64 Result = static_cast<int64>(Context->GetReturnQWord());
		if (!LocalAssert.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("boundary entry should unprepare cleanly"))))
		{
			return MAX_int64;
		}
		if (!LocalAssert.AreEqual(asSUCCESS,
			Context->Prepare(Recovery),
			*Case.Describe(TEXT("boundary recovery should prepare on the same context"))))
		{
			return MAX_int64;
		}
		if (!LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Case.Describe(TEXT("boundary recovery should execute on the same context"))))
		{
			return MAX_int64;
		}
		if (!LocalAssert.AreEqual(static_cast<int64>(181),
			static_cast<int64>(Context->GetReturnQWord()),
			*Case.Describe(TEXT("boundary recovery should return its sentinel"))))
		{
			return MAX_int64;
		}
		if (!LocalAssert.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("boundary recovery should unprepare cleanly"))))
		{
			return MAX_int64;
		}
		return Result;
	}

	void VerifyRetainedFirstFunction(const FNativeCaseContext& Case,
		const FBoundaryWorkflowState& Workflow,
		asIScriptFunction& CurrentFunction)
	{
		ASSERT_THAT(IsNotNull(Workflow.RetainedFirstFunction,
			*Case.Describe(TEXT("rebuild should retain its first-generation function"))));
		if (Workflow.RetainedFirstFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Workflow.RetainedFirstFunction != &CurrentFunction,
			*Case.Describe(TEXT("rebuild should publish a new function object"))));
		ASSERT_THAT(IsTrue(Workflow.FirstFunctionId != CurrentFunction.GetId(),
			*Case.Describe(TEXT("retained first generation should prevent function ID reuse"))));
		ASSERT_THAT(AreEqual(Workflow.FirstDeclaration,
			FString(
				UTF8_TO_TCHAR(Workflow.RetainedFirstFunction->GetDeclaration(true, true, false))),
			*Case.Describe(TEXT("retained first function should preserve its declaration"))));
		ASSERT_THAT(AreEqual(Workflow.FirstSection,
			FString(UTF8_TO_TCHAR(Workflow.RetainedFirstFunction->GetScriptSectionName())),
			*Case.Describe(TEXT("retained first function should preserve its section"))));
		asUINT RetainedLength = 0;
		ASSERT_THAT(
			IsTrue(Workflow.RetainedFirstFunction->GetByteCode(&RetainedLength) != nullptr &&
					   RetainedLength == Workflow.FirstBytecodeLength,
				*Case.Describe(TEXT("retained first function should preserve readable bytecode"))));
	}

	void RunGeneration(FNativeTestEngine& Engine,
		const FBoundaryBuildCase& BuildCase,
		const FBoundaryContextCase& ContextCase,
		const FBoundaryScenarioCase& ScenarioCase,
		FBoundaryWorkflowState& Workflow)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-SOURCE-BOUNDARY",
			{
				ANSI_TO_TCHAR(BuildCase.CatalogName),
				ANSI_TO_TCHAR(ContextCase.CatalogName),
				ANSI_TO_TCHAR(ScenarioCase.CatalogName),
			}));
		const FString Source = BuildExpressionBoundarySource(ScenarioCase, ContextCase, BuildCase);
		ASSERT_THAT(IsTrue(Engine.Get()->GetModule(
							   TCHAR_TO_ANSI(*Workflow.ModuleName), asGM_ONLY_IF_EXISTS) == nullptr,
			*Case.Describe(TEXT("boundary generation should begin without a published module"))));
		asIScriptModule* const Module =
			CompileAndReport(Engine, Case, Case.GetId(), Workflow.ModuleName, Source);
		if (Module == nullptr)
		{
			return;
		}

		VerifyBoundaryMetadata(Case, ScenarioCase, ContextCase, *Engine.Get(), *Module);
		asIScriptFunction* const Entry = Module->GetFunctionByDecl("int64 RunExpressionBoundary()");
		ASSERT_THAT(
			IsNotNull(Entry, *Case.Describe(TEXT("boundary generation should expose its entry"))));
		const FString BytecodeStructure = CaptureModuleBytecodeStructure(*Module);
		ASSERT_THAT(IsTrue(!BytecodeStructure.IsEmpty(),
			*Case.Describe(TEXT("boundary generation should expose valid bytecode structure"))));
		const int64 Result = ExecuteBoundaryEntry(Case, *Engine.Get(), *Module);
		const int64 Expected = ExpectedBoundaryResult(ScenarioCase, BuildCase);
		ASSERT_THAT(AreEqual(Expected,
			Result,
			*Case.Describe(TEXT("boundary generation should return its versioned result"))));

		if (Entry != nullptr)
		{
			if (BuildCase.Build == EBoundaryBuild::First)
			{
				Workflow.FirstSource = Source;
				Workflow.FirstBytecodeStructure = BytecodeStructure;
				Workflow.FirstResult = Result;
				Workflow.FirstDeclaration = UTF8_TO_TCHAR(Entry->GetDeclaration(true, true, false));
				Workflow.FirstSection = UTF8_TO_TCHAR(Entry->GetScriptSectionName());
				Workflow.FirstFunctionId = Entry->GetId();
				Entry->GetByteCode(&Workflow.FirstBytecodeLength);
				Entry->AddRef();
				Workflow.RetainedFirstFunction = Entry;
			}
			else
			{
				VerifyRetainedFirstFunction(Case, Workflow, *Entry);
				if (BuildCase.Build == EBoundaryBuild::SameRebuild)
				{
					ASSERT_THAT(AreEqual(Workflow.FirstSource,
						Source,
						*Case.Describe(TEXT("same rebuild should reproduce the exact source"))));
					ASSERT_THAT(AreEqual(Workflow.FirstBytecodeStructure,
						BytecodeStructure,
						*Case.Describe(
							TEXT("same rebuild should reproduce normalized bytecode structure"))));
					ASSERT_THAT(AreEqual(Workflow.FirstResult,
						Result,
						*Case.Describe(TEXT("same rebuild should reproduce the first result"))));
				}
				else
				{
					ASSERT_THAT(IsTrue(Workflow.FirstSource != Source,
						*Case.Describe(
							TEXT("changed rebuild should change its generated source"))));
					ASSERT_THAT(IsTrue(Workflow.FirstResult != Result,
						*Case.Describe(TEXT("changed rebuild should change its runtime result"))));
				}
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*Workflow.ModuleName);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("boundary generation should discard its module"))));
		if (BuildCase.Build == EBoundaryBuild::ChangedRebuild &&
			Workflow.RetainedFirstFunction != nullptr)
		{
			Workflow.RetainedFirstFunction->Release();
			Workflow.RetainedFirstFunction = nullptr;
		}
	}

public:
	TEST_METHOD(ScenariosByContextAndBuild)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EXPR-SOURCE-BOUNDARY",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata |
				ENativeEvidence::Bytecode | ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Expression boundary product should create a standalone raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const int32 WorkflowCount = UE_ARRAY_COUNT(ContextCases) * UE_ARRAY_COUNT(ScenarioCases);
		TArray<FBoundaryWorkflowState> Workflows;
		Workflows.SetNum(WorkflowCount);
		for (int32 ContextIndex = 0; ContextIndex < UE_ARRAY_COUNT(ContextCases); ++ContextIndex)
		{
			for (int32 ScenarioIndex = 0; ScenarioIndex < UE_ARRAY_COUNT(ScenarioCases);
				++ScenarioIndex)
			{
				FBoundaryWorkflowState& Workflow =
					Workflows[ContextIndex * UE_ARRAY_COUNT(ScenarioCases) + ScenarioIndex];
				Workflow.ModuleName = FString::Printf(TEXT("ExpressionBoundary_%hs_%hs"),
					ContextCases[ContextIndex].CatalogName,
					ScenarioCases[ScenarioIndex].CatalogName);
			}
		}

		for (const FBoundaryBuildCase& BuildCase : BuildCases)
		{
			for (int32 ContextIndex = 0; ContextIndex < UE_ARRAY_COUNT(ContextCases);
				++ContextIndex)
			{
				for (int32 ScenarioIndex = 0; ScenarioIndex < UE_ARRAY_COUNT(ScenarioCases);
					++ScenarioIndex)
				{
					FBoundaryWorkflowState& Workflow =
						Workflows[ContextIndex * UE_ARRAY_COUNT(ScenarioCases) + ScenarioIndex];
					RunGeneration(Engine,
						BuildCase,
						ContextCases[ContextIndex],
						ScenarioCases[ScenarioIndex],
						Workflow);
				}
			}
		}

		for (FBoundaryWorkflowState& Workflow : Workflows)
		{
			ASSERT_THAT(IsNull(Workflow.RetainedFirstFunction,
				TEXT("Expression boundary workflow should release every retained function")));
			if (Workflow.RetainedFirstFunction != nullptr)
			{
				Workflow.RetainedFirstFunction->Release();
				Workflow.RetainedFirstFunction = nullptr;
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
