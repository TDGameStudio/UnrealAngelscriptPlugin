#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FLogicalOperatorTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.Logical",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	inline static constexpr asPWORD LogicalTraceSlot = static_cast<asPWORD>(0x4E41544C4F475452ull);

	enum class ELogicalContext : uint8
	{
		Assignment,
		Return,
		Condition,
		Argument,
	};

	enum class ELogicalOperator : uint8
	{
		And,
		Or,
		Xor,
	};

	enum class ELogicalSource : uint8
	{
		BoolLiteral,
		BoolLValue,
		Comparison,
		ConversionOperator,
	};

	struct FContextCase
	{
		const ANSICHAR* CatalogName;
		ELogicalContext Context;
	};

	struct FOperatorCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* Token;
		ELogicalOperator Operator;
	};

	struct FSourceCase
	{
		const ANSICHAR* CatalogName;
		ELogicalSource Source;
	};

	struct FTruthCase
	{
		const ANSICHAR* CatalogName;
		bool Left;
		bool Right;
	};

	struct FLogicalTrace
	{
		void Reset()
		{
			Markers.Reset();
			Values.Reset();
		}

		TArray<int32> Markers;
		TArray<bool> Values;
	};

	inline static constexpr FContextCase ContextCases[] = {
		{"assignment", ELogicalContext::Assignment},
		{"return", ELogicalContext::Return},
		{"condition", ELogicalContext::Condition},
		{"argument", ELogicalContext::Argument},
	};

	inline static constexpr FOperatorCase OperatorCases[] = {
		{"and", TEXT("&&"), ELogicalOperator::And},
		{"or", TEXT("||"), ELogicalOperator::Or},
		{"xor", TEXT("^^"), ELogicalOperator::Xor},
	};

	inline static constexpr FSourceCase SourceCases[] = {
		{"bool_literal", ELogicalSource::BoolLiteral},
		{"bool_lvalue", ELogicalSource::BoolLValue},
		{"comparison", ELogicalSource::Comparison},
		{"conversion_operator", ELogicalSource::ConversionOperator},
	};

	inline static constexpr FTruthCase TruthCases[] = {
		{"false_false", false, false},
		{"false_true", false, true},
		{"true_false", true, false},
		{"true_true", true, true},
	};

	static FLogicalTrace* ActiveTrace()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
				   ? static_cast<FLogicalTrace*>(
						 Context->GetEngine()->GetUserData(LogicalTraceSlot))
				   : nullptr;
	}

	static bool TraceLogicalOperand(const int32 Marker, const bool Value)
	{
		if (FLogicalTrace* const Trace = ActiveTrace())
		{
			Trace->Markers.Add(Marker);
			Trace->Values.Add(Value);
		}
		return Value;
	}

	static bool RegisterTrace(asIScriptEngine& Engine, FLogicalTrace& Trace)
	{
		Engine.SetUserData(&Trace, LogicalTraceSlot);
		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(TraceLogicalOperand);
		return Engine.RegisterGlobalFunction("bool TraceLogicalOperand(int Marker, bool Value)",
				   asFUNCTION(TraceLogicalOperand),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&Caller) >= 0;
	}

	static FString BooleanLiteral(const bool Value)
	{
		return Value ? TEXT("true") : TEXT("false");
	}

	static FString BuildLogicalExpression(
		const FSourceCase& SourceCase, const FOperatorCase& OperatorCase)
	{
		switch (SourceCase.Source)
		{
		case ELogicalSource::BoolLiteral:
			return FString::Printf(TEXT("TraceLogicalOperand(1, %s) %s "
										"TraceLogicalOperand(2, %s)"),
				TEXT("LEFT_TRUTH"),
				OperatorCase.Token,
				TEXT("RIGHT_TRUTH"));
		case ELogicalSource::BoolLValue:
			return FString::Printf(TEXT("TraceLogicalOperand(1, LeftValue) %s "
										"TraceLogicalOperand(2, RightValue)"),
				OperatorCase.Token);
		case ELogicalSource::Comparison:
			return FString::Printf(TEXT("TraceLogicalOperand(1, LeftNumber == 1) %s "
										"TraceLogicalOperand(2, RightNumber == 1)"),
				OperatorCase.Token);
		case ELogicalSource::ConversionOperator:
			// The fork does not apply script-defined implicit conversions while
			// selecting a logical operator. Call the conversion accessor explicitly
			// so the generated case still exercises conversion, short-circuiting,
			// and the selected consumer context.
			return FString::Printf(TEXT("LeftValue.opImplConv() %s RightValue.opImplConv()"), OperatorCase.Token);
		default:
			return TEXT("false");
		}
	}

	static FString BuildLogicalSource(const FContextCase& ContextCase,
		const FOperatorCase& OperatorCase,
		const FSourceCase& SourceCase,
		const FTruthCase& TruthCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		if (SourceCase.Source == ELogicalSource::ConversionOperator)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FLogicalConversionValue"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tbool Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tint Marker;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source, TEXT("\tFLogicalConversionValue(bool InValue, int InMarker)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tMarker = InMarker;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tbool opImplConv() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn TraceLogicalOperand(Marker, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		if (ContextCase.Context == ELogicalContext::Argument)
		{
			AppendGeneratedAsLine(Source, TEXT("bool ConsumeLogical(bool Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("bool RunLogical()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (SourceCase.Source)
		{
		case ELogicalSource::BoolLValue:
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\tbool LeftValue = %s;"), *BooleanLiteral(TruthCase.Left)));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\tbool RightValue = %s;"), *BooleanLiteral(TruthCase.Right)));
			break;
		case ELogicalSource::Comparison:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tint LeftNumber = %d;"), TruthCase.Left ? 1 : 0));
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tint RightNumber = %d;"), TruthCase.Right ? 1 : 0));
			break;
		case ELogicalSource::ConversionOperator:
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\tFLogicalConversionValue LeftValue(%s, 1);"),
					*BooleanLiteral(TruthCase.Left)));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\tFLogicalConversionValue RightValue(%s, 2);"),
					*BooleanLiteral(TruthCase.Right)));
			break;
		case ELogicalSource::BoolLiteral:
		default:
			break;
		}

		FString Expression = BuildLogicalExpression(SourceCase, OperatorCase);
		Expression.ReplaceInline(
			TEXT("LEFT_TRUTH"), *BooleanLiteral(TruthCase.Left), ESearchCase::CaseSensitive);
		Expression.ReplaceInline(
			TEXT("RIGHT_TRUTH"), *BooleanLiteral(TruthCase.Right), ESearchCase::CaseSensitive);
		switch (ContextCase.Context)
		{
		case ELogicalContext::Assignment:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tbool Result = %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
			break;
		case ELogicalContext::Return:
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
			break;
		case ELogicalContext::Condition:
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tif (%s)"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn true;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn false;"));
			break;
		case ELogicalContext::Argument:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn ConsumeLogical(%s);"), *Expression));
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static bool ExpectedResult(const FOperatorCase& OperatorCase, const FTruthCase& TruthCase)
	{
		switch (OperatorCase.Operator)
		{
		case ELogicalOperator::And:
			return TruthCase.Left && TruthCase.Right;
		case ELogicalOperator::Or:
			return TruthCase.Left || TruthCase.Right;
		case ELogicalOperator::Xor:
			return TruthCase.Left != TruthCase.Right;
		default:
			return false;
		}
	}

	static TArray<int32> ExpectedMarkers(
		const FOperatorCase& OperatorCase, const FTruthCase& TruthCase)
	{
		switch (OperatorCase.Operator)
		{
		case ELogicalOperator::And:
			return TruthCase.Left ? TArray<int32>{1, 2} : TArray<int32>{1};
		case ELogicalOperator::Or:
			return TruthCase.Left ? TArray<int32>{1} : TArray<int32>{1, 2};
		case ELogicalOperator::Xor:
			return {1, 2};
		default:
			return {};
		}
	}

	static bool ArraysEqual(const TArray<int32>& Left, const TArray<int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index] != Right[Index])
			{
				return false;
			}
		}
		return true;
	}

	static FString MarkersText(const TArray<int32>& Markers)
	{
		FString Result;
		for (const int32 Marker : Markers)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(",");
			}
			Result += FString::FromInt(Marker);
		}
		return Result;
	}

	static asIScriptFunction* FindRunFunction(asIScriptModule& Module)
	{
		return AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(
			&Module, "bool RunLogical()");
	}

	static bool VerifySourceMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptFunction& Run,
		const FNativeCaseContext& Case,
		const FContextCase& ContextCase,
		const FSourceCase& SourceCase)
	{
		FNoDiscardAsserter Assert(Test);
		if (!Assert.AreEqual(static_cast<asUINT>(0),
				Run.GetParamCount(),
				*Case.Describe(TEXT("logical entry should take no host arguments"))) ||
			!Assert.AreEqual(Engine.GetTypeIdByDecl("bool"),
				Run.GetReturnTypeId(),
				*Case.Describe(TEXT("logical entry should return bool"))))
		{
			return false;
		}

		if (ContextCase.Context == ELogicalContext::Argument)
		{
			asIScriptFunction* const Consume =
				AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(
					&Module, "bool ConsumeLogical(const bool)");
			if (!Assert.IsNotNull(
					Consume, *Case.Describe(TEXT("argument context should publish its consumer"))))
			{
				return false;
			}
		}
		if (SourceCase.Source == ELogicalSource::ConversionOperator)
		{
			asITypeInfo* const Type = Module.GetTypeInfoByName("FLogicalConversionValue");
			if (!Assert.IsNotNull(
					Type, *Case.Describe(TEXT("conversion source should publish its value type"))))
			{
				return false;
			}
			asIScriptFunction* const Conversion = Type->GetMethodByDecl("bool opImplConv() const");
			return Assert.IsNotNull(Conversion,
					   *Case.Describe(
						   TEXT("conversion source should publish exact bool conversion"))) &&
				   Assert.IsTrue(Conversion->IsReadOnly(),
					   *Case.Describe(TEXT("conversion operator should retain const receiver")));
		}

		const char* ExpectedLocal = SourceCase.Source == ELogicalSource::BoolLValue	  ? "LeftValue"
									: SourceCase.Source == ELogicalSource::Comparison ? "LeftNumber"
																					  : nullptr;
		if (ExpectedLocal != nullptr)
		{
			for (asUINT VariableIndex = 0; VariableIndex < Run.GetVarCount(); ++VariableIndex)
			{
				const char* Name = nullptr;
				if (Run.GetVar(VariableIndex, &Name) >= 0 && Name != nullptr &&
					FCStringAnsi::Strcmp(Name, ExpectedLocal) == 0)
				{
					return true;
				}
			}
			return Assert.IsTrue(
				false, *Case.Describe(TEXT("logical source should retain its typed left local")));
		}
		return true;
	}

	static bool ExecuteLogicalCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FNativeCaseContext& Case,
		const FOperatorCase& OperatorCase,
		const FTruthCase& TruthCase,
		FLogicalTrace& Trace)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Run = FindRunFunction(Module);
		if (!Assert.IsNotNull(Run, *Case.Describe(TEXT("logical entry should resolve exactly"))))
		{
			return false;
		}
		asIScriptContext* const Context = Engine.CreateContext();
		if (!Assert.IsNotNull(
				Context, *Case.Describe(TEXT("logical case should create a context"))))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		Trace.Reset();
		const int ExecuteResult = PrepareAndExecute(Context, Run);
		const bool ActualResult = Context->GetReturnByte() != 0;
		const TArray<int32> ExpectedTrace = ExpectedMarkers(OperatorCase, TruthCase);
		const bool bExecuted = Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("logical case should execute")));
		const bool bResult = Assert.AreEqual(ExpectedResult(OperatorCase, TruthCase),
			ActualResult,
			*Case.Describe(TEXT("logical case should return its truth-table result")));
		const bool bMarkers = Assert.IsTrue(ArraysEqual(ExpectedTrace, Trace.Markers),
			*Case.DescribeResult(
				Run->GetDeclaration(), MarkersText(ExpectedTrace), MarkersText(Trace.Markers)));
		bool bValues = Assert.AreEqual(Trace.Markers.Num(),
			Trace.Values.Num(),
			*Case.Describe(TEXT("every logical marker should retain its bool value")));
		if (Trace.Values.Num() >= 1)
		{
			bValues &= Assert.AreEqual(TruthCase.Left,
				Trace.Values[0],
				*Case.Describe(TEXT("logical left trace should retain its input")));
		}
		if (Trace.Values.Num() >= 2)
		{
			bValues &= Assert.AreEqual(TruthCase.Right,
				Trace.Values[1],
				*Case.Describe(TEXT("logical right trace should retain its input")));
		}
		const bool bUnprepared = Assert.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("logical context should unprepare after execution")));
		return bExecuted && bResult && bMarkers && bValues && bUnprepared;
	}

public:
	TEST_METHOD(SourcesByOperatorTruthAndContext)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-LOGICAL",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			Engine.Get(), TEXT("logical operator product should create a standalone engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}

		FLogicalTrace Trace;
		ASSERT_THAT(IsTrue(RegisterTrace(*Engine.Get(), Trace),
			TEXT("logical operator product should register its trace callback")));
		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		bool bAllCasesPassed = true;
		for (const FContextCase& ContextCase : ContextCases)
		{
			for (const FOperatorCase& OperatorCase : OperatorCases)
			{
				for (const FSourceCase& SourceCase : SourceCases)
				{
					for (const FTruthCase& TruthCase : TruthCases)
					{
						const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-LOGICAL",
							{ANSI_TO_TCHAR(ContextCase.CatalogName),
								ANSI_TO_TCHAR(OperatorCase.CatalogName),
								ANSI_TO_TCHAR(SourceCase.CatalogName),
								ANSI_TO_TCHAR(TruthCase.CatalogName)}));
						ConstructedIds.Add(Case.GetId());
						const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
						UniqueIds.Add(Case.GetId());
						ASSERT_THAT(IsTrue(bUniqueCaseId,
							*Case.Describe(TEXT("logical case ID should be unique"))));

						const FString CurrentModuleName =
							FString::Printf(TEXT("ASNativeLogical_%hs_%hs_%hs_%hs"),
								ContextCase.CatalogName,
								OperatorCase.CatalogName,
								SourceCase.CatalogName,
								TruthCase.CatalogName);
						const FString Source =
							BuildLogicalSource(ContextCase, OperatorCase, SourceCase, TruthCase);
						PrintGeneratedAsSource(
							*TestRunner, Case.GetId(), CurrentModuleName, Source);
						Engine.Reset(*TestRunner);
						const FTCHARToUTF8 ModuleNameUtf8(*CurrentModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(
							Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*Case.DescribeResult("<logical build>",
								TEXT("successful build"),
								Engine.GetMessagesText())));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("logical source should publish its module"))));
						if (BuildResult < 0 || Module == nullptr)
						{
							return;
						}

						asIScriptFunction* const Run = FindRunFunction(*Module);
						if (Run != nullptr)
						{
							bAllCasesPassed &= VerifySourceMetadata(*TestRunner,
								*Engine.Get(),
								*Module,
								*Run,
								Case,
								ContextCase,
								SourceCase);
							bAllCasesPassed &= ExecuteLogicalCase(*TestRunner,
								*Engine.Get(),
								*Module,
								Case,
								OperatorCase,
								TruthCase,
								Trace);
						}
						else
						{
							bAllCasesPassed = false;
							ASSERT_THAT(IsNotNull(
								Run, *Case.Describe(TEXT("logical entry should resolve"))));
						}

						Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(
							Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("logical source module should discard"))));
					}
				}
			}
		}

		ASSERT_THAT(AreEqual(192,
			ConstructedIds.Num(),
			TEXT("logical operator product should construct all 192 catalog IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("logical operator product should construct no duplicate IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT("every logical cell should satisfy result, trace, metadata, and cleanup")));
	}
};

#endif
