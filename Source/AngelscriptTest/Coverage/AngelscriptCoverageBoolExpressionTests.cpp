#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptNativeScriptTestObject.h"

#include "Containers/Map.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageBoolExpressionTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript bool *expression usage* -- operators, literals,
// conversions, and logical operations.
//
// Bool operations:
//   - Logical: &&, ||, !
//   - Equality: ==, !=
//   - Unsupported boundary: &, |, ^
//   - No arithmetic: +, -, *, /, %
//   - No ordering: <, <=, >, >=
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageBoolExpressionTest,
	"Angelscript.TestModule.Coverage.BoolExpression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// Helper
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("bool expression module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("bool expression global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const T Result = Invoker.ExecuteAndGet<T>(T{});
		ASSERT_THAT(AreEqual(Expected, Result, Message));
	}

	template <typename T>
	void ExpectObjectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, UObject* Argument, const T& Expected, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("bool expression module should compile before executing UObject global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("bool expression UObject global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		Invoker.AddArgObject(Argument);
		const T Result = Invoker.ExecuteAndGet<T>(T{});
		ASSERT_THAT(AreEqual(Expected, Result, Message));
	}

	void ExpectCompileFailureDiagnostics(
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName,
		const FString& Source,
		const TCHAR* CaseLabel,
		TArrayView<const FString> ExpectedDiagnosticFragments)
	{
		TestRunner->AddInfo(FString(CaseLabel));

		const FString ModuleNameString(ModuleName);
		FAngelscriptCompileTraceSummary Summary;
		CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FName(*ModuleNameString),
			FString::Printf(TEXT("%s.as"), *ModuleNameString),
			Source,
			true,
			Summary,
			true);
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleNameString); };

		ASSERT_THAT(IsFalse(Summary.bCompileSucceeded, TEXT("bool negative fixture should fail to compile")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("bool negative fixture compile result should be Error")));

		TestRunner->AddInfo(FString::Printf(TEXT("%s compile diagnostics: %d"), CaseLabel, Summary.Diagnostics.Num()));
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("  %s Row%d:Col%d %s"),
				Diagnostic.bIsError ? TEXT("ERROR") : (Diagnostic.bIsInfo ? TEXT("INFO") : TEXT("WARN")),
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
		}

		TMap<FString, int32> ExpectedFragmentCounts;
		for (const FString& ExpectedFragment : ExpectedDiagnosticFragments)
		{
			++ExpectedFragmentCounts.FindOrAdd(ExpectedFragment);
		}

		for (const TPair<FString, int32>& ExpectedFragmentCount : ExpectedFragmentCounts)
		{
			int32 ActualCount = 0;
			for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
			{
				if (Diagnostic.bIsError && Diagnostic.Message.Contains(ExpectedFragmentCount.Key))
				{
					++ActualCount;
				}
			}
			ASSERT_THAT(IsTrue(
				ActualCount >= ExpectedFragmentCount.Value,
				*FString::Printf(
					TEXT("%s diagnostics should contain '%s' at least %d time(s), got %d"),
					CaseLabel,
					*ExpectedFragmentCount.Key,
					ExpectedFragmentCount.Value,
					ActualCount)));
		}
	}

	void ExpectScriptException(
		FAngelscriptEngine& Engine,
		asIScriptModule* Module,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		const FString& ExpectedExceptionContains,
		int32 ExpectedExceptionLine)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("bool negative execution module should compile before exception assertion")));
		if (Module == nullptr)
		{
			return;
		}

		TestRunner->AddInfo(FString(CaseLabel));
		asIScriptFunction* Function = Module->GetFunctionByDecl(TCHAR_TO_ANSI(FunctionDecl));
		ASSERT_THAT(IsNotNull(Function, *FString::Printf(TEXT("%s should resolve target function"), CaseLabel)));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context, *FString::Printf(TEXT("%s should create execution context"), CaseLabel)));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		const int32 PrepareResult = Context->Prepare(Function);
		const int32 ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		const int32 ExceptionLine = Context->GetExceptionLineNumber();

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, *FString::Printf(TEXT("%s should prepare successfully"), CaseLabel)));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult, *FString::Printf(TEXT("%s should raise asEXECUTION_EXCEPTION"), CaseLabel)));
		ASSERT_THAT(IsTrue(ExceptionString.Contains(ExpectedExceptionContains), *FString::Printf(TEXT("%s exception should contain '%s'"), CaseLabel, *ExpectedExceptionContains)));
		ASSERT_THAT(AreEqual(ExpectedExceptionLine, ExceptionLine, *FString::Printf(TEXT("%s exception line should match the pinned AS fixture line"), CaseLabel)));

		TestRunner->AddInfo(FString::Printf(TEXT("%s raised at line %d: %s"), CaseLabel, ExceptionLine, *ExceptionString));
	}

	// -------------------------------------------------------------------------
	// Local declarations: default, const, true/false.
	// -------------------------------------------------------------------------
	TEST_METHOD(LocalDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_LocalDecl", ASTEST_AS(R"AS(
		bool LocalDefaultInit()
		{
			bool Value = true;
			return Value;
		}

		bool LocalDeferredInit()
		{
			bool Value;
			Value = false;
			return Value;
		}

		bool LocalConst()
		{
			const bool Value = true;
			return Value;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LocalDefaultInit()"), true, TEXT("local bool with default true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LocalDeferredInit()"), false, TEXT("local bool deferred init false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LocalConst()"), true, TEXT("local const bool"));
	}

	// -------------------------------------------------------------------------
	// Global const declarations.
	// -------------------------------------------------------------------------
	TEST_METHOD(GlobalConstDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_GlobalConst", ASTEST_AS(R"AS(
		const bool GConstTrue = true;
		const bool GConstFalse = false;

		bool GetGlobalTrue()
		{
			return GConstTrue;
		}

		bool GetGlobalFalse()
		{
			return GConstFalse;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool GetGlobalTrue()"), true, TEXT("global const bool true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool GetGlobalFalse()"), false, TEXT("global const bool false"));
	}

	// -------------------------------------------------------------------------
	// Mutable module-level bool globals are rejected by this fork.
	// -------------------------------------------------------------------------
	TEST_METHOD(GlobalMutableDeclarationsUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Global variable 'GMutable' must be const. Mutable global variables are not supported."));

		// Actual diagnostic: "Global variable 'GMutable' must be const. Mutable global variables are not supported."
		// Reason: this fork only permits const module globals; mutable state must live in locals, objects, or UPROPERTY fields.
		ExpectCompileFailureDiagnostics(Engine, TEXT("ASCovBoolExpr_GlobalMutableUnsupported"), ASTEST_AS(R"AS(
		bool GMutable = true;

		bool ReadGlobalMutable()
		{
			return GMutable;
		}
		)AS"),
			TEXT("mutable module-level bool globals should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics));
	}

	// -------------------------------------------------------------------------
	// Auto deduction: bool literal and expression inference.
	// -------------------------------------------------------------------------
	TEST_METHOD(AutoDeduction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Auto", ASTEST_AS(R"AS(
		bool AutoFromTrue()
		{
			auto Value = true;
			return Value;
		}

		bool AutoFromFalse()
		{
			auto Value = false;
			return Value;
		}

		bool AutoFromComparison()
		{
			auto Value = 7 > 3;
			return Value;
		}

		bool AutoFromLogical()
		{
			auto Value = true && !false;
			return Value;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool AutoFromTrue()"), true, TEXT("auto should infer bool from true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool AutoFromFalse()"), false, TEXT("auto should infer bool from false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool AutoFromComparison()"), true, TEXT("auto should infer bool from comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool AutoFromLogical()"), true, TEXT("auto should infer bool from logical expression"));
	}

	// -------------------------------------------------------------------------
	// Logical operators: &&, ||, !
	// -------------------------------------------------------------------------
	TEST_METHOD(LogicalOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Logical", ASTEST_AS(R"AS(
		bool OpLogicalAnd_TT() { return true && true; }
		bool OpLogicalAnd_TF() { return true && false; }
		bool OpLogicalAnd_FT() { return false && true; }
		bool OpLogicalAnd_FF() { return false && false; }

		bool OpLogicalOr_TT() { return true || true; }
		bool OpLogicalOr_TF() { return true || false; }
		bool OpLogicalOr_FT() { return false || true; }
		bool OpLogicalOr_FF() { return false || false; }

		bool OpLogicalNot_T() { return !true; }
		bool OpLogicalNot_F() { return !false; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// &&
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalAnd_TT()"), true, TEXT("true && true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalAnd_TF()"), false, TEXT("true && false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalAnd_FT()"), false, TEXT("false && true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalAnd_FF()"), false, TEXT("false && false"));

		// ||
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalOr_TT()"), true, TEXT("true || true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalOr_TF()"), true, TEXT("true || false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalOr_FT()"), true, TEXT("false || true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalOr_FF()"), false, TEXT("false || false"));

		// !
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalNot_T()"), false, TEXT("!true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLogicalNot_F()"), true, TEXT("!false"));
	}

	// -------------------------------------------------------------------------
	// Equality operators: ==, !=
	// -------------------------------------------------------------------------
	TEST_METHOD(EqualityOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Equality", ASTEST_AS(R"AS(
		bool OpEquals_TT() { return true == true; }
		bool OpEquals_TF() { return true == false; }
		bool OpEquals_FT() { return false == true; }
		bool OpEquals_FF() { return false == false; }

		bool OpNotEquals_TT() { return true != true; }
		bool OpNotEquals_TF() { return true != false; }
		bool OpNotEquals_FT() { return false != true; }
		bool OpNotEquals_FF() { return false != false; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// ==
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_TT()"), true, TEXT("true == true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_TF()"), false, TEXT("true == false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_FT()"), false, TEXT("false == true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEquals_FF()"), true, TEXT("false == false"));

		// !=
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_TT()"), false, TEXT("true != true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_TF()"), true, TEXT("true != false"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_FT()"), true, TEXT("false != true"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEquals_FF()"), false, TEXT("false != false"));
	}

	// -------------------------------------------------------------------------
	// Bitwise operators: &, |, ^ are not available on bool in this fork.
	// -------------------------------------------------------------------------
	TEST_METHOD(BitwiseOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		for (int32 OperatorIndex = 0; OperatorIndex < 3; ++OperatorIndex)
		{
			ExpectedDiagnostics.Add(TEXT("No conversion from 'bool' to 'int' available."));
			ExpectedDiagnostics.Add(TEXT("No conversion from 'bool' to 'bool' available."));
		}

		// Actual diagnostics for each of &, |, ^:
		// - "No conversion from 'bool' to 'int' available."
		// - "No conversion from 'bool' to 'bool' available."
		// Reason: this fork has no bool-specific bitwise operator overloads; logical bool composition must use &&, ||, and !.
		ExpectCompileFailureDiagnostics(Engine, TEXT("ASCovBoolExpr_BitwiseUnsupported"), ASTEST_AS(R"AS(
		bool OpBitAnd_TT()
		{
			return true & true;
		}

		bool OpBitOr_TT()
		{
			return true | true;
		}

		bool OpBitXor_TT()
		{
			return true ^ true;
		}
		)AS"),
			TEXT("bool bitwise operators should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics));
	}

	// -------------------------------------------------------------------------
	// Literals: true, false.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolLiterals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Literals", ASTEST_AS(R"AS(
		bool LiteralTrue() { return true; }
		bool LiteralFalse() { return false; }
		bool LiteralFromComparison() { return 5 > 3; }
		bool LiteralFromLogical() { return true && true; }
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LiteralTrue()"), true, TEXT("true literal"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LiteralFalse()"), false, TEXT("false literal"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LiteralFromComparison()"), true, TEXT("bool from comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool LiteralFromLogical()"), true, TEXT("bool from logical expr"));
	}

	// -------------------------------------------------------------------------
	// Bool literals are case-sensitive: only lowercase true/false are valid.
	// -------------------------------------------------------------------------
	TEST_METHOD(CaseSensitiveLiterals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		{
			TArray<FString> ExpectedDiagnostics;
			ExpectedDiagnostics.Add(TEXT("'True' is not declared"));
			// Actual diagnostic: "'True' is not declared".
			// Reason: bool literals are lowercase keywords only; uppercase identifiers are ordinary names.
			ExpectCompileFailureDiagnostics(Engine, TEXT("ASCovBoolExpr_UppercaseTrueUnsupported"), ASTEST_AS(R"AS(
			bool ReadUppercaseTrue()
			{
				return True;
			}
			)AS"),
				TEXT("uppercase True should not be accepted as a bool literal"),
				MakeArrayView(ExpectedDiagnostics));
		}

		{
			TArray<FString> ExpectedDiagnostics;
			ExpectedDiagnostics.Add(TEXT("'FALSE' is not declared"));
			// Actual diagnostic: "'FALSE' is not declared".
			// Reason: bool literals are lowercase keywords only; uppercase identifiers are ordinary names.
			ExpectCompileFailureDiagnostics(Engine, TEXT("ASCovBoolExpr_UppercaseFalseUnsupported"), ASTEST_AS(R"AS(
			bool ReadUppercaseFalse()
			{
				return FALSE;
			}
			)AS"),
				TEXT("uppercase FALSE should not be accepted as a bool literal"),
				MakeArrayView(ExpectedDiagnostics));
		}
	}

	// -------------------------------------------------------------------------
	// Type conversions: explicit comparison/ternary patterns plus unsupported casts.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_Conversion", ASTEST_AS(R"AS(
		int BoolToIntPattern_T()
		{
			return true ? 1 : 0;
		}

		int BoolToIntPattern_F()
		{
			return false ? 1 : 0;
		}

		bool IntToBoolPattern_0()
		{
			return 0 != 0;
		}

		bool IntToBoolPattern_1()
		{
			return 1 != 0;
		}

		bool IntToBoolPattern_Neg()
		{
			return -5 != 0;
		}

		bool IntToBoolPattern_Large()
		{
			return 999 != 0;
		}

		bool BoolToFloatPattern_T()
		{
			float Value = true ? 1.0f : 0.0f;
			return Math::IsNearlyEqual(Value, 1.0f, 0.001f);
		}

		bool BoolToFloatPattern_F()
		{
			float Value = false ? 1.0f : 0.0f;
			return Math::IsNearlyEqual(Value, 0.0f, 0.001f);
		}

		bool FloatToBoolPattern_0()
		{
			return 0.0f != 0.0f;
		}

		bool FloatToBoolPattern_Pos()
		{
			return 1.5f != 0.0f;
		}

		bool FloatToBoolPattern_Neg()
		{
			return -2.5f != 0.0f;
		}

		FString BoolToStringPattern_T()
		{
			return true ? "true" : "false";
		}

		FString BoolToStringPattern_F()
		{
			return false ? "true" : "false";
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int BoolToIntPattern_T()"), 1, TEXT("bool true can map to int via ternary"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int BoolToIntPattern_F()"), 0, TEXT("bool false can map to int via ternary"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToBoolPattern_0()"), false, TEXT("int zero can map to bool via explicit comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToBoolPattern_1()"), true, TEXT("int one can map to bool via explicit comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToBoolPattern_Neg()"), true, TEXT("negative int can map to bool via explicit comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToBoolPattern_Large()"), true, TEXT("non-zero int can map to bool via explicit comparison"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool BoolToFloatPattern_T()"), true, TEXT("bool true can map to float via ternary"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool BoolToFloatPattern_F()"), true, TEXT("bool false can map to float via ternary"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool FloatToBoolPattern_0()"), false, TEXT("float zero can map to bool via explicit comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool FloatToBoolPattern_Pos()"), true, TEXT("positive float can map to bool via explicit comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool FloatToBoolPattern_Neg()"), true, TEXT("negative float can map to bool via explicit comparison"));

		FASGlobalFunctionInvoker TrueStringInvoker(*TestRunner, Engine, *Module, TEXT("FString BoolToStringPattern_T()"));
		ASSERT_THAT(IsTrue(TrueStringInvoker.IsValid(), TEXT("BoolToStringPattern_T should resolve and prepare")));
		if (!TrueStringInvoker.IsValid())
		{
			return;
		}
		FString TrueString;
		ASSERT_THAT(IsTrue(TrueStringInvoker.ExecuteAndExtractStruct(TrueString)));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), TrueString, TEXT("bool true can map to FString via ternary")));

		FASGlobalFunctionInvoker FalseStringInvoker(*TestRunner, Engine, *Module, TEXT("FString BoolToStringPattern_F()"));
		ASSERT_THAT(IsTrue(FalseStringInvoker.IsValid(), TEXT("BoolToStringPattern_F should resolve and prepare")));
		if (!FalseStringInvoker.IsValid())
		{
			return;
		}
		FString FalseString;
		ASSERT_THAT(IsTrue(FalseStringInvoker.ExecuteAndExtractStruct(FalseString)));
		ASSERT_THAT(AreEqual(FString(TEXT("false")), FalseString, TEXT("bool false can map to FString via ternary")));

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No conversion from 'const bool' to 'const int' available."));
		ExpectedDiagnostics.Add(TEXT("No conversion from 'const int' to 'const bool' available."));
		ExpectedDiagnostics.Add(TEXT("No conversion from 'const bool' to 'const float' available."));
		ExpectedDiagnostics.Add(TEXT("No conversion from 'const float32' to 'const bool' available."));
		// Actual diagnostics are the four exact "No conversion from ... available." messages above.
		// Reason: constructor-style primitive casts do not define bool numeric conversions; use ternary or explicit comparisons.
		ExpectCompileFailureDiagnostics(Engine, TEXT("ASCovBoolExpr_UnsupportedPrimitiveCasts"), ASTEST_AS(R"AS(
		int BoolToInt_T()
		{
			return int(true);
		}

		bool IntToBool_1()
		{
			return bool(1);
		}

		float BoolToFloat_T()
		{
			return float(true);
		}

		bool FloatToBool_Pos()
		{
			return bool(1.5f);
		}
		)AS"),
			TEXT("bool primitive casts should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics));
	}

	// -------------------------------------------------------------------------
	// UObject handles used in bool-producing null/validity expressions.
	// -------------------------------------------------------------------------
	TEST_METHOD(HandleConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_HandleConversion", ASTEST_AS(R"AS(
		bool ObjectNotNull(UObject Obj)
		{
			return Obj != nullptr;
		}

		bool ObjectIsNull(UObject Obj)
		{
			return Obj == nullptr;
		}

		bool ObjectIsValid(UObject Obj)
		{
			return IsValid(Obj);
		}

		int ObjectAsBranch(UObject Obj)
		{
			if (Obj != nullptr)
			{
				return 1;
			}
			return 0;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("handle conversion module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		TStrongObjectPtr<UAngelscriptNativeScriptTestObject> LiveObject(
			NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage(), TEXT("ASCovBoolHandleLiveObject")));
		ASSERT_THAT(IsNotNull(LiveObject.Get(), TEXT("handle conversion test should create a live UObject")));

		ExpectObjectGlobalReturn<bool>(Engine, Module, TEXT("bool ObjectNotNull(UObject)"), nullptr, false, TEXT("nullptr handle should compare false for != nullptr"));
		ExpectObjectGlobalReturn<bool>(Engine, Module, TEXT("bool ObjectNotNull(UObject)"), LiveObject.Get(), true, TEXT("live UObject handle should compare true for != nullptr"));
		ExpectObjectGlobalReturn<bool>(Engine, Module, TEXT("bool ObjectIsNull(UObject)"), nullptr, true, TEXT("nullptr handle should compare true for == nullptr"));
		ExpectObjectGlobalReturn<bool>(Engine, Module, TEXT("bool ObjectIsNull(UObject)"), LiveObject.Get(), false, TEXT("live UObject handle should compare false for == nullptr"));
		ExpectObjectGlobalReturn<bool>(Engine, Module, TEXT("bool ObjectIsValid(UObject)"), nullptr, false, TEXT("IsValid(nullptr) should return false"));
		ExpectObjectGlobalReturn<bool>(Engine, Module, TEXT("bool ObjectIsValid(UObject)"), LiveObject.Get(), true, TEXT("IsValid(live UObject) should return true"));
		ExpectObjectGlobalReturn<int32>(Engine, Module, TEXT("int ObjectAsBranch(UObject)"), nullptr, 0, TEXT("nullptr handle should take false branch"));
		ExpectObjectGlobalReturn<int32>(Engine, Module, TEXT("int ObjectAsBranch(UObject)"), LiveObject.Get(), 1, TEXT("live UObject handle should take true branch"));
	}

	// -------------------------------------------------------------------------
	// Logical short-circuit evaluation.
	// -------------------------------------------------------------------------
	TEST_METHOD(LogicalShortCircuit)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_ShortCircuit", ASTEST_AS(R"AS(
		bool SideEffect(int&inout Counter)
		{
			Counter++;
			return true;
		}

		bool TestAndShortCircuit()
		{
			int Counter = 0;
			bool result = false && SideEffect(Counter);
			return Counter == 0;  // SideEffect should not be called
		}

		bool TestOrShortCircuit()
		{
			int Counter = 0;
			bool result = true || SideEffect(Counter);
			return Counter == 0;  // SideEffect should not be called
		}

		bool TestAndNoShortCircuit()
		{
			int Counter = 0;
			bool result = true && SideEffect(Counter);
			return Counter == 1;  // SideEffect should be called
		}

		bool TestOrNoShortCircuit()
		{
			int Counter = 0;
			bool result = false || SideEffect(Counter);
			return Counter == 1;  // SideEffect should be called
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestAndShortCircuit()"), true, TEXT("&& short-circuit (false && ...)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestOrShortCircuit()"), true, TEXT("|| short-circuit (true || ...)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestAndNoShortCircuit()"), true, TEXT("&& no short-circuit (true && ...)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestOrNoShortCircuit()"), true, TEXT("|| no short-circuit (false || ...)"));
	}

	// -------------------------------------------------------------------------
	// Bool in control flow: if, loops, ternary, and function-return condition.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolInControlFlow)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_ControlFlow", ASTEST_AS(R"AS(
		bool IsReady()
		{
			return true;
		}

		int IfCondition(bool Value)
		{
			if (Value)
			{
				return 10;
			}
			return 20;
		}

		int WhileCondition()
		{
			bool bContinue = true;
			int Count = 0;
			while (bContinue)
			{
				Count++;
				bContinue = Count < 3;
			}
			return Count;
		}

		int DoWhileCondition()
		{
			bool bContinue = false;
			int Count = 0;
			do
			{
				Count++;
			}
			while (bContinue);
			return Count;
		}

		int ForCondition()
		{
			int Count = 0;
			for (bool bContinue = true; bContinue; bContinue = Count < 2)
			{
				Count++;
			}
			return Count;
		}

		int TernaryCondition(bool Value)
		{
			return Value ? 1 : 0;
		}

		int FunctionReturnCondition()
		{
			if (IsReady())
			{
				return 1;
			}
			return 0;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("bool control-flow module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int IfCondition(bool)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("IfCondition should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(true);
			ASSERT_THAT(AreEqual(10, Invoker.ExecuteAndGet<int32>(INDEX_NONE), TEXT("if condition should take true branch")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int IfCondition(bool)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("IfCondition should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(false);
			ASSERT_THAT(AreEqual(20, Invoker.ExecuteAndGet<int32>(INDEX_NONE), TEXT("if condition should take false branch")));
		}
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int WhileCondition()"), 3, TEXT("while condition should use bool guard"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int DoWhileCondition()"), 1, TEXT("do-while condition should use bool guard"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ForCondition()"), 2, TEXT("for condition should use bool guard"));
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int TernaryCondition(bool)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("TernaryCondition should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(true);
			ASSERT_THAT(AreEqual(1, Invoker.ExecuteAndGet<int32>(INDEX_NONE), TEXT("ternary condition should map true")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int TernaryCondition(bool)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("TernaryCondition should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(false);
			ASSERT_THAT(AreEqual(0, Invoker.ExecuteAndGet<int32>(INDEX_NONE), TEXT("ternary condition should map false")));
		}
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int FunctionReturnCondition()"), 1, TEXT("function returning bool should be usable as condition"));
	}

	// -------------------------------------------------------------------------
	// Script class members are a current execution boundary in this fork.
	// Actual runtime diagnostics:
	// - TestClassMemberAccess: line 29, "Null pointer access".
	// - TestClassMemberModify: line 35, "Null pointer access".
	// - TestClassMemberToggle: line 43, "Null pointer access".
	// Reason: plain script-class bool member execution is not a supported
	// coverage surface here; reflected state should be exercised through
	// UCLASS / UPROPERTY fixtures.
	// -------------------------------------------------------------------------
	TEST_METHOD(ClassMembersNonProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolExpr_ClassMember", ASTEST_AS(R"AS(
		class BoolHolder
		{
			bool Value;

			BoolHolder()
			{
				Value = true;
			}

			bool GetValue() const
			{
				return Value;
			}

			void SetValue(bool v)
			{
				Value = v;
			}

			void Toggle()
			{
				Value = !Value;
			}
		}

		bool TestClassMemberAccess()
		{
			BoolHolder holder;
			return holder.Value;
		}

		bool TestClassMemberModify()
		{
			BoolHolder holder;
			holder.Value = false;
			return holder.GetValue();
		}

		bool TestClassMemberToggle()
		{
			BoolHolder holder;
			holder.Toggle();
			return holder.Value;  // Should be false
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		TestRunner->AddExpectedError(TEXT("Null pointer access"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASCovBoolExpr_ClassMember"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("TestClassMember"), EAutomationExpectedErrorFlags::Contains, 3);

		ExpectScriptException(
			Engine,
			Module,
			TEXT("bool TestClassMemberAccess()"),
			TEXT("bool class member access currently remains a script-class execution boundary"),
			TEXT("Null pointer access"),
			29);

		ExpectScriptException(
			Engine,
			Module,
			TEXT("bool TestClassMemberModify()"),
			TEXT("bool class member modify currently remains a script-class execution boundary"),
			TEXT("Null pointer access"),
			35);

		ExpectScriptException(
			Engine,
			Module,
			TEXT("bool TestClassMemberToggle()"),
			TEXT("bool class member toggle currently remains a script-class execution boundary"),
			TEXT("Null pointer access"),
			43);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
