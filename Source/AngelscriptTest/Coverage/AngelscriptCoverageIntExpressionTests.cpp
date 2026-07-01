#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageIntExpressionTests
// -----------------------------------------------------------------------------
// "Übershader-style" coverage for AngelScript integer-family *expressions* and
// *declaration contexts* -- the non-UPROPERTY half of the int matrix. Where
// AngelscriptCoverageIntPropertyTests.cpp drives values through the UE property
// layer (Pattern D), this file exercises the language surface directly by
// compiling module-level global functions and invoking them through
// FASGlobalFunctionInvoker (Pattern B/F from the Angelscript test guide).
//
// Axes covered here (OpenSpec: test-coverage/coverage-matrix.md):
//   * LocalDeclarations          - sub-matrix 2: local default-init / deferred
//                                  init / local const / auto inference.
//   * GlobalConstDeclarations     - sub-matrix 2: module-level `const` globals
//                                  across the whole int family. Mutable module
//                                  globals are forbidden by this fork, so the
//                                  `const` form is the *only* legal global int
//                                  declaration and must be covered explicitly.
//   * ArithmeticOperators        - sub-matrix 7: + - * / % and unary minus.
//   * BitwiseAndShiftOperators   - sub-matrix 7: & | ^ ~ << >>.
//   * ComparisonOperators        - sub-matrix 7: == != < <= > >=.
//   * CompoundAssignmentOperators- sub-matrix 7: += -= *= /= %= &= |= ^= <<= >>=
//                                  plus pre/post ++ / --.
//   * IntegerLiterals            - sub-matrix 8: decimal / hex / binary / octal /
//                                  int64 promotion / unsigned range.
//   * IntegerConversions         - sub-matrix 9: widening / truncation /
//                                  signed<->unsigned / int<->double / int->int8.
//
// int family under test:
//   int8 / int16 / int (int32) / int64 / uint8 / uint16 / uint (uint32) / uint64
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageIntExpressionTest,
	"Angelscript.TestModule.Coverage.IntExpression",
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

	// Compile + invoke one module-level global function, returning its value and
	// asserting it equals Expected. Keeps each axis below to a flat list of
	// expectations instead of per-call boilerplate.
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, T Expected, const TCHAR* Label)
	{
		if (Module == nullptr)
		{
			TestRunner->AddError(FString::Printf(TEXT("%s: backing module failed to build"), Label));
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		const T Actual = Invoker.CallAndReturn<T>();
		TestRunner->TestEqual(Label, Actual, Expected);
	}

	template <typename T>
	bool ExecuteStructGlobal(FAngelscriptEngine& Engine, asIScriptModule& Module, const TCHAR* Declaration, T& OutValue)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, Module, Declaration);
		return Invoker.ExecuteAndExtractStruct(OutValue);
	}

	void ExpectGlobalException(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const TCHAR* Label, const TCHAR* ExpectedException)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("Int expression exception-boundary module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, Declaration);
		ASSERT_THAT(IsNotNull(Function, TEXT("Int expression exception-boundary function should resolve")));
		if (Function == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			Label,
			ExpectedException)));
	}

	// -------------------------------------------------------------------------
	// Local declaration contexts: default init, deferred init, const, auto.
	// -------------------------------------------------------------------------
	TEST_METHOD(LocalDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_LocalDecl", ASTEST_AS(R"AS(
		int LocalDefaultInit()
		{
			int Value = 5;
			return Value;
		}

		int LocalDeferredInit()
		{
			int Value;
			Value = 7;
			return Value;
		}

		int LocalConst()
		{
			const int Value = 9;
			return Value;
		}

		int LocalAutoNarrow()
		{
			auto Value = 11;
			return Value;
		}

		int64 LocalAutoWide()
		{
			auto Value = 10000000000;
			return Value;
		}

		int8 LocalInt8()
		{
			int8 Value = -42;
			return Value;
		}

		int16 LocalInt16()
		{
			int16 Value = 30000;
			return Value;
		}

		int64 LocalInt64()
		{
			int64 Value = 10000000000;
			return Value;
		}

		uint8 LocalUInt8()
		{
			uint8 Value = 255;
			return Value;
		}

		uint16 LocalUInt16()
		{
			uint16 Value = 60000;
			return Value;
		}

		uint LocalUInt()
		{
			uint Value = 3000000000;
			return Value;
		}

		uint64 LocalUInt64()
		{
			uint64 Value = 18000000000000000000;
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

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int LocalDefaultInit()"),  5,  TEXT("local int with default initializer"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int LocalDeferredInit()"), 7,  TEXT("local int declared then assigned"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int LocalConst()"),        9,  TEXT("local const int"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int LocalAutoNarrow()"),   11, TEXT("auto infers int from int literal"));
		ExpectGlobalReturn<int64>(Engine, Module, TEXT("int64 LocalAutoWide()"),   static_cast<int64>(10000000000LL), TEXT("auto infers int64 from large literal"));
		ExpectGlobalReturn<int8> (Engine, Module, TEXT("int8 LocalInt8()"),        static_cast<int8>(-42), TEXT("local int8"));
		ExpectGlobalReturn<int16>(Engine, Module, TEXT("int16 LocalInt16()"),      static_cast<int16>(30000), TEXT("local int16"));
		ExpectGlobalReturn<int64>(Engine, Module, TEXT("int64 LocalInt64()"),      static_cast<int64>(10000000000LL), TEXT("local int64"));
		ExpectGlobalReturn<uint8> (Engine, Module, TEXT("uint8 LocalUInt8()"),     static_cast<uint8>(255), TEXT("local uint8"));
		ExpectGlobalReturn<uint16>(Engine, Module, TEXT("uint16 LocalUInt16()"),   static_cast<uint16>(60000), TEXT("local uint16"));
		ExpectGlobalReturn<uint32>(Engine, Module, TEXT("uint LocalUInt()"),       static_cast<uint32>(3000000000u), TEXT("local uint"));
		ExpectGlobalReturn<uint64>(Engine, Module, TEXT("uint64 LocalUInt64()"),   static_cast<uint64>(18000000000000000000ull), TEXT("local uint64"));
	}

	// -------------------------------------------------------------------------
	// Module-level const globals across the whole int family. Mutable module
	// globals are rejected by this fork, so const is the only legal global int
	// declaration form -- it MUST be covered.
	// -------------------------------------------------------------------------
	TEST_METHOD(GlobalConstDeclarations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_GlobalConst", ASTEST_AS(R"AS(
		const int8 GConstInt8 = -42;
		const int16 GConstInt16 = -12345;
		const int GConstInt = -987654;
		const int64 GConstInt64 = -9000000000;
		const uint8 GConstUInt8 = 200;
		const uint16 GConstUInt16 = 54321;
		const uint GConstUInt32 = 3000000000;
		const uint64 GConstUInt64 = 12000000000000000000;

		int8 GetConstInt8()
		{
			return GConstInt8;
		}

		int16 GetConstInt16()
		{
			return GConstInt16;
		}

		int GetConstInt()
		{
			return GConstInt;
		}

		int64 GetConstInt64()
		{
			return GConstInt64;
		}

		uint8 GetConstUInt8()
		{
			return GConstUInt8;
		}

		uint16 GetConstUInt16()
		{
			return GConstUInt16;
		}

		uint GetConstUInt32()
		{
			return GConstUInt32;
		}

		uint64 GetConstUInt64()
		{
			return GConstUInt64;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int8>  (Engine, Module, TEXT("int8 GetConstInt8()"),     static_cast<int8>(-42),                       TEXT("const int8 global"));
		ExpectGlobalReturn<int16> (Engine, Module, TEXT("int16 GetConstInt16()"),   static_cast<int16>(-12345),                   TEXT("const int16 global"));
		ExpectGlobalReturn<int32> (Engine, Module, TEXT("int GetConstInt()"),       -987654,                                      TEXT("const int global"));
		ExpectGlobalReturn<int64> (Engine, Module, TEXT("int64 GetConstInt64()"),   static_cast<int64>(-9000000000LL),            TEXT("const int64 global"));
		ExpectGlobalReturn<uint8> (Engine, Module, TEXT("uint8 GetConstUInt8()"),   static_cast<uint8>(200),                      TEXT("const uint8 global"));
		ExpectGlobalReturn<uint16>(Engine, Module, TEXT("uint16 GetConstUInt16()"), static_cast<uint16>(54321),                   TEXT("const uint16 global"));
		ExpectGlobalReturn<uint32>(Engine, Module, TEXT("uint GetConstUInt32()"),   static_cast<uint32>(3000000000u),             TEXT("const uint global"));
		ExpectGlobalReturn<uint64>(Engine, Module, TEXT("uint64 GetConstUInt64()"), static_cast<uint64>(12000000000000000000ull), TEXT("const uint64 global"));
	}

	// -------------------------------------------------------------------------
	// Arithmetic: + - * / % and unary minus.
	// -------------------------------------------------------------------------
	TEST_METHOD(ArithmeticOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_Arith", ASTEST_AS(R"AS(
		int OpAdd()
		{
			int a = 7;
			int b = 5;
			return a + b;
		}

		int OpSub()
		{
			return 7 - 5;
		}

		int OpMul()
		{
			return 7 * 5;
		}

		int OpDiv()
		{
			return 37 / 5;
		}

		int OpMod()
		{
			return 37 % 5;
		}

		int OpNegate()
		{
			int a = 5;
			return -a;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpAdd()"),    12, TEXT("int addition"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpSub()"),    2,  TEXT("int subtraction"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpMul()"),    35, TEXT("int multiplication"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpDiv()"),    7,  TEXT("int truncating division"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpMod()"),    2,  TEXT("int modulo"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpNegate()"), -5, TEXT("int unary minus"));
	}

	TEST_METHOD(IntWidthOperatorSamples)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_WidthOps", ASTEST_AS(R"AS(
		int8 Int8Arithmetic()
		{
			int8 Value = 10;
			Value += 5;
			Value *= 2;
			return Value - 3;
		}

		int16 Int16Bitwise()
		{
			int16 Value = 0x00F0;
			Value |= 0x000F;
			Value ^= 0x0033;
			return Value;
		}

		int64 Int64ShiftAndCompare()
		{
			int64 Value = 1;
			Value <<= 40;
			if (Value > int64(1000000000000))
			{
				return Value >> 20;
			}

			return -1;
		}

		uint8 UInt8WrapAndIncrement()
		{
			uint8 Value = 250;
			Value += 5;
			return ++Value;
		}

		uint16 UInt16ShiftAndMask()
		{
			uint16 Value = 0x00FF;
			Value <<= 4;
			return Value & 0x0FF0;
		}

		uint UIntLogicalShift()
		{
			uint Value = 0x80000000;
			return Value >> 28;
		}

		uint64 UInt64BitwiseAndCompare()
		{
			uint64 Value = 0x100000000;
			Value |= 0xFF;
			return Value > uint64(0xFFFFFFFF) ? Value : uint64(0);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int width-operator module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<int8>(27), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int8 Int8Arithmetic()")).CallAndReturn<int8>(0),
			TEXT("int8 arithmetic and compound assignment should evaluate")));
		ASSERT_THAT(AreEqual(static_cast<int16>(0x00CC), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int16 Int16Bitwise()")).CallAndReturn<int16>(0),
			TEXT("int16 bitwise compound operators should evaluate")));
		ASSERT_THAT(AreEqual(static_cast<int64>(1LL << 20), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int64 Int64ShiftAndCompare()")).CallAndReturn<int64>(0),
			TEXT("int64 shift and comparison should evaluate")));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint8 UInt8WrapAndIncrement()")).CallAndReturn<uint8>(0),
			TEXT("uint8 compound add and pre-increment should preserve current wrap behavior")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(0x0FF0), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint16 UInt16ShiftAndMask()")).CallAndReturn<uint16>(0),
			TEXT("uint16 shift and mask should evaluate")));
		ASSERT_THAT(AreEqual(static_cast<uint32>(8), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint UIntLogicalShift()")).CallAndReturn<uint32>(0),
			TEXT("uint right shift should evaluate on the unsigned domain")));
		ASSERT_THAT(AreEqual(static_cast<uint64>(0x1000000FFull), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint64 UInt64BitwiseAndCompare()")).CallAndReturn<uint64>(0),
			TEXT("uint64 bitwise OR and comparison should evaluate")));
	}

	// -------------------------------------------------------------------------
	// Arithmetic safety: division by zero, overflow, underflow behavior.
	// -------------------------------------------------------------------------
	TEST_METHOD(ArithmeticSafety)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_Safety", ASTEST_AS(R"AS(
		// Division by zero - should handle gracefully or return specific value
		int DivideByZero()
		{
			int x = 10;
			int y = 0;
			return x / y;
		}

		// Modulo by zero
		int ModuloByZero()
		{
			int x = 10;
			int y = 0;
			return x % y;
		}

		// Integer overflow (INT_MAX + 1)
		int Overflow()
		{
			int x = 2147483647;  // INT_MAX
			return x + 1;
		}

		// Integer underflow (INT_MIN - 1)
		int Underflow()
		{
			int x = -2147483648;  // INT_MIN
			return x - 1;
		}

		// Multiplication overflow
		int MultiplyOverflow()
		{
			int x = 1000000;
			int y = 10000;
			return x * y;
		}

		// Negative number right shift
		int NegativeShift()
		{
			int x = -8;
			return x >> 1;
		}

		// Large shift amount (>= bit width)
		int LargeShift()
		{
			int x = 1;
			return x << 32;  // shift amount >= 32 bits
		}

		// Unsigned overflow wrapping
		uint UnsignedOverflow()
		{
			uint x = 4294967295;  // UINT_MAX
			return x + 1;
		}

		// Self-operation
		int SelfAdd()
		{
			int x = 100;
			x += x;
			return x;
		}

		int SelfMultiply()
		{
			int x = 5;
			x *= x;
			return x;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalException(Engine, Module, TEXT("int DivideByZero()"), TEXT("int division by zero should raise a script exception"), TEXT("Divide by zero"));
		ExpectGlobalException(Engine, Module, TEXT("int ModuloByZero()"), TEXT("int modulo by zero should raise a script exception"), TEXT("Divide by zero"));

		// Overflow behavior (typically wraps in two's complement)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int Overflow()"));
			int32 Result = Invoker.ExecuteAndGet<int32>(0);
			// INT_MAX + 1 typically wraps to INT_MIN
			TestRunner->AddInfo(FString::Printf(TEXT("INT_MAX + 1 = %d (overflow behavior)"), Result));
		}

		// Underflow behavior
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int Underflow()"));
			int32 Result = Invoker.ExecuteAndGet<int32>(0);
			// INT_MIN - 1 typically wraps to INT_MAX
			TestRunner->AddInfo(FString::Printf(TEXT("INT_MIN - 1 = %d (underflow behavior)"), Result));
		}

		// Multiply overflow
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int MultiplyOverflow()"));
			int32 Result = Invoker.ExecuteAndGet<int32>(0);
			TestRunner->AddInfo(FString::Printf(TEXT("1000000 * 10000 = %d (multiply overflow)"), Result));
		}

		// Negative shift (arithmetic vs logical)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int NegativeShift()"));
			int32 Result = Invoker.ExecuteAndGet<int32>(0);
			TestRunner->AddInfo(FString::Printf(TEXT("-8 >> 1 = %d (negative shift)"), Result));
		}

		// Large shift amount
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int LargeShift()"));
			int32 Result = Invoker.ExecuteAndGet<int32>(0);
			TestRunner->AddInfo(FString::Printf(TEXT("1 << 32 = %d (large shift)"), Result));
		}

		// Unsigned overflow (should wrap to 0)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint UnsignedOverflow()"));
			uint32 Result = Invoker.ExecuteAndGet<uint32>(0);
			TestRunner->TestEqual(TEXT("UINT_MAX + 1 wraps to 0"), Result, static_cast<uint32>(0));
		}

		// Self operations
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int SelfAdd()"), 200, TEXT("x += x"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int SelfMultiply()"), 25, TEXT("x *= x"));
	}

	// -------------------------------------------------------------------------
	// Bitwise and shift: & | ^ ~ << >> >>>.
	// -------------------------------------------------------------------------
	TEST_METHOD(BitwiseAndShiftOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_Bitwise", ASTEST_AS(R"AS(
		int OpBitAnd()
		{
			return 0xF0 & 0x3C;
		}

		int OpBitOr()
		{
			return 0xF0 | 0x0F;
		}

		int OpBitXor()
		{
			return 0xFF ^ 0x0F;
		}

		int OpBitNot()
		{
			return ~0;
		}

		int OpShiftLeft()
		{
			return 1 << 4;
		}

		int OpShiftRight()
		{
			return 256 >> 2;
		}

		int OpArithmeticShiftRight()
		{
			int x = -256;
			return x >>> 2;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpBitAnd()"),     0x30, TEXT("bitwise AND"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpBitOr()"),      0xFF, TEXT("bitwise OR"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpBitXor()"),     0xF0, TEXT("bitwise XOR"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpBitNot()"),     -1,   TEXT("bitwise NOT of 0"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpShiftLeft()"),  16,   TEXT("left shift"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpShiftRight()"), 64,   TEXT("right shift"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpArithmeticShiftRight()"), -64, TEXT("arithmetic right shift preserves sign bit"));
	}

	// -------------------------------------------------------------------------
	// Comparison operators all return bool.
	// -------------------------------------------------------------------------
	TEST_METHOD(ComparisonOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_Compare", ASTEST_AS(R"AS(
		bool OpEqual()
		{
			return 5 == 5;
		}

		bool OpNotEqual()
		{
			return 5 != 6;
		}

		bool OpLess()
		{
			return 5 < 6;
		}

		bool OpLessEqual()
		{
			return 5 <= 5;
		}

		bool OpGreater()
		{
			return 6 > 5;
		}

		bool OpGreaterEqual()
		{
			return 5 >= 5;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpEqual()"),        true, TEXT("equality"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpNotEqual()"),     true, TEXT("inequality"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLess()"),         true, TEXT("less-than"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpLessEqual()"),    true, TEXT("less-or-equal"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpGreater()"),      true, TEXT("greater-than"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool OpGreaterEqual()"), true, TEXT("greater-or-equal"));
	}

	// -------------------------------------------------------------------------
	// Compound assignment and increment / decrement.
	// -------------------------------------------------------------------------
	TEST_METHOD(CompoundAssignmentOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_Compound", ASTEST_AS(R"AS(
		int OpAddAssign()
		{
			int x = 10;
			x += 5;
			return x;
		}

		int OpSubAssign()
		{
			int x = 10;
			x -= 3;
			return x;
		}

		int OpMulAssign()
		{
			int x = 10;
			x *= 3;
			return x;
		}

		int OpDivAssign()
		{
			int x = 10;
			x /= 2;
			return x;
		}

		int OpModAssign()
		{
			int x = 10;
			x %= 3;
			return x;
		}

		int OpAndAssign()
		{
			int x = 0xFF;
			x &= 0x0F;
			return x;
		}

		int OpOrAssign()
		{
			int x = 0xF0;
			x |= 0x0F;
			return x;
		}

		int OpXorAssign()
		{
			int x = 0xFF;
			x ^= 0x0F;
			return x;
		}

		int OpShlAssign()
		{
			int x = 1;
			x <<= 4;
			return x;
		}

		int OpShrAssign()
		{
			int x = 256;
			x >>= 2;
			return x;
		}

		int OpPreIncrement()
		{
			int x = 5;
			return ++x;
		}

		int OpPostIncrement()
		{
			int x = 5;
			x++;
			return x;
		}

		int OpPreDecrement()
		{
			int x = 5;
			return --x;
		}

		int OpPostDecrement()
		{
			int x = 5;
			x--;
			return x;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpAddAssign()"),     15,   TEXT("+= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpSubAssign()"),     7,    TEXT("-= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpMulAssign()"),     30,   TEXT("*= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpDivAssign()"),     5,    TEXT("/= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpModAssign()"),     1,    TEXT("%= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpAndAssign()"),     0x0F, TEXT("&= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpOrAssign()"),      0xFF, TEXT("|= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpXorAssign()"),     0xF0, TEXT("^= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpShlAssign()"),     16,   TEXT("<<= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpShrAssign()"),     64,   TEXT(">>= compound assignment"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpPreIncrement()"),  6,    TEXT("pre-increment returns incremented value"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpPostIncrement()"), 6,    TEXT("post-increment mutates the variable"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpPreDecrement()"),  4,    TEXT("pre-decrement returns decremented value"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int OpPostDecrement()"), 4,    TEXT("post-decrement mutates the variable"));
	}

	// -------------------------------------------------------------------------
	// Integer literal forms: decimal / hex / binary / octal / promotion / range.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntegerLiterals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_Literals", ASTEST_AS(R"AS(
		int LitDecimal()
		{
			return 123;
		}

		int LitHex()
		{
			return 0xFF;
		}

		int LitBinary()
		{
			return 0b1010;
		}

		int LitOctal()
		{
			return 0o17;
		}

		int64 LitInt64Promotion()
		{
			return 10000000000;
		}

		uint LitUnsignedRange()
		{
			return 3000000000;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32> (Engine, Module, TEXT("int LitDecimal()"),         123,                               TEXT("decimal literal"));
		ExpectGlobalReturn<int32> (Engine, Module, TEXT("int LitHex()"),             0xFF,                              TEXT("hexadecimal literal"));
		ExpectGlobalReturn<int32> (Engine, Module, TEXT("int LitBinary()"),          10,                                TEXT("binary literal 0b1010"));
		ExpectGlobalReturn<int32> (Engine, Module, TEXT("int LitOctal()"),           15,                                TEXT("octal literal 0o17"));
		ExpectGlobalReturn<int64> (Engine, Module, TEXT("int64 LitInt64Promotion()"), static_cast<int64>(10000000000LL), TEXT("literal exceeding int32 promotes to int64"));
		ExpectGlobalReturn<uint32>(Engine, Module, TEXT("uint LitUnsignedRange()"),  static_cast<uint32>(3000000000u),  TEXT("literal above int32 max fits uint"));
	}

	// -------------------------------------------------------------------------
	// Integer conversions: widening / truncation / sign / float interop.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntegerConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_Convert", ASTEST_AS(R"AS(
		int64 WidenIntToInt64()
		{
			int x = 123456;
			int64 y = x;
			return y;
		}

		int TruncateInt64ToInt()
		{
			int64 l = 10000000000;
			int x = int(l);
			return x;
		}

		uint SignedToUnsigned()
		{
			int x = -1;
			uint u = uint(x);
			return u;
		}

		double IntToDouble()
		{
			int x = 7;
			double d = x;
			return d;
		}

		int DoubleToInt()
		{
			double d = 9.9;
			int x = int(d);
			return x;
		}

		int8 IntToInt8()
		{
			int x = 5;
			int8 y = int8(x);
			return y;
		}

		enum ETestEnum
		{
			None = 0,
			First = 1,
			Second = 2,
			Large = 100
		}

		int EnumToInt()
		{
			ETestEnum e = ETestEnum::Large;
			return int(e);
		}

		ETestEnum IntToEnum()
		{
			int x = 2;
			return ETestEnum(x);
		}

		bool EnumRoundTrip()
		{
			ETestEnum original = ETestEnum::First;
			int i = int(original);
			ETestEnum back = ETestEnum(i);
			return back == ETestEnum::First;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int64> (Engine, Module, TEXT("int64 WidenIntToInt64()"),  static_cast<int64>(123456),               TEXT("int widens to int64"));
		ExpectGlobalReturn<int32> (Engine, Module, TEXT("int TruncateInt64ToInt()"), static_cast<int32>(10000000000LL),        TEXT("int64 truncates to int"));
		ExpectGlobalReturn<uint32>(Engine, Module, TEXT("uint SignedToUnsigned()"),  static_cast<uint32>(static_cast<uint32>(-1)), TEXT("-1 reinterpreted as uint"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double IntToDouble()"),     7.0,                                      TEXT("int converts to double"));
		ExpectGlobalReturn<int32> (Engine, Module, TEXT("int DoubleToInt()"),        9,                                        TEXT("double truncates to int"));
		ExpectGlobalReturn<int8>  (Engine, Module, TEXT("int8 IntToInt8()"),         static_cast<int8>(5),                     TEXT("int narrows to int8"));
		ExpectGlobalReturn<int32> (Engine, Module, TEXT("int EnumToInt()"),          100,                                      TEXT("enum converts to int"));
		ExpectGlobalReturn<int32> (Engine, Module, TEXT("ETestEnum IntToEnum()"),    2,                                        TEXT("int converts to enum"));
		ExpectGlobalReturn<bool>  (Engine, Module, TEXT("bool EnumRoundTrip()"),     true,                                     TEXT("enum ↔ int round-trip"));
	}

	// -------------------------------------------------------------------------
	// Class members (non-UPROPERTY): script-visible int fields without
	// reflection, accessed directly within script code.
	// -------------------------------------------------------------------------
	TEST_METHOD(ClassMembersNonProperty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_ClassMember", ASTEST_AS(R"AS(
		class IntHolder
		{
			int Value;
			int64 BigValue;
			uint UnsignedValue;

			IntHolder()
			{
				Value = 42;
				BigValue = 10000000000;
				UnsignedValue = 3000000000;
			}

			int GetValue() const
			{
				return Value;
			}

			void SetValue(int v)
			{
				Value = v;
			}

			int64 GetBigValue() const
			{
				return BigValue;
			}

			uint GetUnsignedValue() const
			{
				return UnsignedValue;
			}
		}

		int TestClassMemberAccess()
		{
			IntHolder holder;
			return holder.Value;
		}

		int TestClassMemberModify()
		{
			IntHolder holder;
			holder.Value = 100;
			return holder.GetValue();
		}

		int64 TestClassMemberInt64()
		{
			IntHolder holder;
			return holder.BigValue;
		}

		uint TestClassMemberUInt()
		{
			IntHolder holder;
			return holder.UnsignedValue;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalException(Engine, Module, TEXT("int TestClassMemberAccess()"),
			TEXT("plain class int member access should remain a null-boundary"), TEXT("Null pointer access"));
		ExpectGlobalException(Engine, Module, TEXT("int TestClassMemberModify()"),
			TEXT("plain class int member mutation should remain a null-boundary"), TEXT("Null pointer access"));
		ExpectGlobalException(Engine, Module, TEXT("int64 TestClassMemberInt64()"),
			TEXT("plain class int64 member access should remain a null-boundary"), TEXT("Null pointer access"));
		ExpectGlobalException(Engine, Module, TEXT("uint TestClassMemberUInt()"),
			TEXT("plain class uint member access should remain a null-boundary"), TEXT("Null pointer access"));
	}

	// -------------------------------------------------------------------------
	// Mixed-type arithmetic: test operator overloads across different widths
	// and signedness. Verifies implicit type promotion rules.
	// -------------------------------------------------------------------------
	TEST_METHOD(MixedTypeArithmetic)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_MixedType", ASTEST_AS(R"AS(
		// int + int64 -> int64
		int64 IntPlusInt64()
		{
			int a = 100;
			int64 b = 10000000000;
			return a + b;
		}

		// int * int64 -> int64
		int64 IntMultiplyInt64()
		{
			int a = 2;
			int64 b = 5000000000;
			return a * b;
		}

		// int + uint -> int (or uint, depends on values)
		int IntPlusUInt()
		{
			int a = 50;
			uint b = 100;
			return a + b;
		}

		// int8 + int -> int
		int Int8PlusInt()
		{
			int8 a = 10;
			int b = 32;
			return a + b;
		}

		// int16 * int -> int
		int Int16MultiplyInt()
		{
			int16 a = 100;
			int b = 5;
			return a * b;
		}

		// uint + uint64 -> uint64
		uint64 UIntPlusUInt64()
		{
			uint a = 1000000000;
			uint64 b = 10000000000;
			return a + b;
		}

		// Mixed comparison: int < int64
		bool IntLessThanInt64()
		{
			int a = 100;
			int64 b = 10000000000;
			return a < b;
		}

		// Mixed comparison: uint > int (requires care with signedness)
		bool UIntGreaterThanInt()
		{
			uint a = 100;
			int b = 50;
			return a > b;
		}

		// Left-side type promotion: int8 + int64
		int64 Int8PlusInt64()
		{
			int8 a = 42;
			int64 b = 9000000000;
			return a + b;
		}

		// Right-side type promotion: int64 + int8
		int64 Int64PlusInt8()
		{
			int64 a = 9000000000;
			int8 b = 42;
			return a + b;
		}

		// Signed + unsigned: int + uint
		int SignedPlusUnsigned()
		{
			int a = -50;
			uint b = 100;
			return a + int(b);
		}

		// Unsigned - signed: uint - int
		uint UnsignedMinusSigned()
		{
			uint a = 200;
			int b = 100;
			return a - uint(b);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int64>(Engine, Module, TEXT("int64 IntPlusInt64()"), static_cast<int64>(10000000100LL), TEXT("int + int64 promotes to int64"));
		ExpectGlobalReturn<int64>(Engine, Module, TEXT("int64 IntMultiplyInt64()"), static_cast<int64>(10000000000LL), TEXT("int * int64 promotes to int64"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int IntPlusUInt()"), 150, TEXT("int + uint"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int Int8PlusInt()"), 42, TEXT("int8 + int promotes to int"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int Int16MultiplyInt()"), 500, TEXT("int16 * int promotes to int"));
		ExpectGlobalReturn<uint64>(Engine, Module, TEXT("uint64 UIntPlusUInt64()"), static_cast<uint64>(11000000000ull), TEXT("uint + uint64 promotes to uint64"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntLessThanInt64()"), true, TEXT("int < int64 comparison"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool UIntGreaterThanInt()"), true, TEXT("uint > int comparison"));
		ExpectGlobalReturn<int64>(Engine, Module, TEXT("int64 Int8PlusInt64()"), static_cast<int64>(9000000042LL), TEXT("int8 + int64 (left promotes)"));
		ExpectGlobalReturn<int64>(Engine, Module, TEXT("int64 Int64PlusInt8()"), static_cast<int64>(9000000042LL), TEXT("int64 + int8 (right promotes)"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int SignedPlusUnsigned()"), 50, TEXT("signed + unsigned with explicit cast"));
		ExpectGlobalReturn<uint32>(Engine, Module, TEXT("uint UnsignedMinusSigned()"), static_cast<uint32>(100), TEXT("unsigned - signed with explicit cast"));
	}

	// -------------------------------------------------------------------------
	// Int with UE Math Types: test operators between int/float and FVector,
	// FRotator, FLinearColor, etc. Verifies UE type system integration.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntWithUEMathTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_UEMath", ASTEST_AS(R"AS(
		// FVector * int (scalar multiplication)
		FVector VectorTimesInt()
		{
			FVector v(1.0f, 2.0f, 3.0f);
			return v * 2;
		}

		// FVector / int
		FVector VectorDivInt()
		{
			FVector v(10.0f, 20.0f, 30.0f);
			return v / 2;
		}

		// FVector indexing with int
		float VectorIndexInt()
		{
			FVector v(1.0f, 2.0f, 3.0f);
			return v[1];
		}

		// FVector2D * int
		FVector2D Vector2DTimesInt()
		{
			FVector2D v(3.0f, 4.0f);
			return v * 5;
		}

		// FRotator * float (from int)
		FRotator RotatorTimesInt()
		{
			FRotator r(10.0f, 20.0f, 30.0f);
			return r * 2;
		}

		// FLinearColor * float (from int)
		FLinearColor ColorTimesInt()
		{
			FLinearColor c(0.5f, 0.5f, 0.5f, 1.0f);
			return c * 2;
		}

		// FLinearColor with int indexing
		float ColorIndexInt()
		{
			FLinearColor c(0.1f, 0.2f, 0.3f, 0.4f);
			return c.R;
		}

		// FVector with float from int literal
		FVector VectorTimesIntLiteral()
		{
			FVector v(1.0f, 1.0f, 1.0f);
			return v * 10;
		}

		// FBox expansion with int-based vector
		FBox BoxPlusVector()
		{
			FBox box(FVector(0, 0, 0), FVector(10, 10, 10));
			return box + FVector(5, 5, 5);
		}

		// Int comparison with vector component
		bool CompareIntWithVectorComponent()
		{
			FVector v(5.0f, 10.0f, 15.0f);
			int x = 5;
			return int(v.X) == x;
		}

		// FIntVector arithmetic (all int)
		FIntVector IntVectorAdd()
		{
			FIntVector a(1, 2, 3);
			FIntVector b(4, 5, 6);
			return a + b;
		}

		// FIntVector * int scalar
		FIntVector IntVectorTimesInt()
		{
			FIntVector v(2, 3, 4);
			return v * 3;
		}

		// FIntPoint (2D int vector)
		FIntPoint IntPointAdd()
		{
			FIntPoint a(10, 20);
			FIntPoint b(5, 15);
			return a + b;
		}

		// FIntPoint component access
		int IntPointComponent()
		{
			FIntPoint p(100, 200);
			return p.X + p.Y;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("int with UE math type module should compile")));

		// FVector tests
		if (Module != nullptr)
		{
			{
				FVector Result = FVector::ZeroVector;
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FVector VectorTimesInt()"), Result)));
				TestRunner->TestTrue(TEXT("FVector * int"), Result.Equals(FVector(2.0f, 4.0f, 6.0f), 0.001f));
			}
			{
				FVector Result = FVector::ZeroVector;
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FVector VectorDivInt()"), Result)));
				TestRunner->TestTrue(TEXT("FVector / int"), Result.Equals(FVector(5.0f, 10.0f, 15.0f), 0.001f));
			}
			{
				FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("float VectorIndexInt()"));
				double Result = Invoker.ExecuteAndGet<double>(0.0);
				TestRunner->TestTrue(TEXT("FVector[int]"), FMath::IsNearlyEqual(Result, 2.0, 0.001));
			}

			// FVector2D tests
			{
				FVector2D Result = FVector2D::ZeroVector;
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FVector2D Vector2DTimesInt()"), Result)));
				TestRunner->TestTrue(TEXT("FVector2D * int"), Result.Equals(FVector2D(15.0f, 20.0f), 0.001f));
			}

			// FRotator tests
			{
				FRotator Result = FRotator::ZeroRotator;
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FRotator RotatorTimesInt()"), Result)));
				TestRunner->TestTrue(TEXT("FRotator * int"), Result.Equals(FRotator(20.0f, 40.0f, 60.0f), 0.001f));
			}

			// FLinearColor tests
			{
				FLinearColor Result = FLinearColor::Black;
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FLinearColor ColorTimesInt()"), Result)));
				TestRunner->TestTrue(TEXT("FLinearColor * int"), Result.Equals(FLinearColor(1.0f, 1.0f, 1.0f, 2.0f), 0.001f));
			}

			// FBox tests
			{
				FBox Result(EForceInit::ForceInit);
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FBox BoxPlusVector()"), Result)));
				TestRunner->TestTrue(TEXT("FBox + FVector should include the added point"), Result.Min.Equals(FVector(0, 0, 0), 0.001f) && Result.Max.Equals(FVector(10, 10, 10), 0.001f));
			}

			// Comparison test
			{
				FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool CompareIntWithVectorComponent()"));
				bool Result = Invoker.ExecuteAndGet<bool>(false);
				TestRunner->TestTrue(TEXT("int == int(FVector.X)"), Result);
			}

			// FIntVector tests
			{
				FIntVector Result = FIntVector::ZeroValue;
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FIntVector IntVectorAdd()"), Result)));
				TestRunner->TestTrue(TEXT("FIntVector + FIntVector"), Result == FIntVector(5, 7, 9));
			}
			{
				FIntVector Result = FIntVector::ZeroValue;
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FIntVector IntVectorTimesInt()"), Result)));
				TestRunner->TestTrue(TEXT("FIntVector * int"), Result == FIntVector(6, 9, 12));
			}

			// FIntPoint tests
			{
				FIntPoint Result = FIntPoint::ZeroValue;
				ASSERT_THAT(IsTrue(ExecuteStructGlobal(Engine, *Module, TEXT("FIntPoint IntPointAdd()"), Result)));
				TestRunner->TestTrue(TEXT("FIntPoint + FIntPoint"), Result == FIntPoint(15, 35));
			}
			{
				FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int IntPointComponent()"));
				int32 Result = Invoker.ExecuteAndGet<int32>(0);
				TestRunner->TestEqual(TEXT("FIntPoint.X + FIntPoint.Y"), Result, 300);
			}
		}

		const TArray<FString> ExpectedDiagnostics = {
			TEXT("No conversion from 'FVector' to math type available")
		};
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovIntExpr_IntTimesVectorUnsupported"),
			ASTEST_AS(R"AS(
			FVector TryIntTimesVector()
			{
				FVector V(1.0f, 2.0f, 3.0f);
				return 2 * V;
			}
			)AS"),
			TEXT("int * FVector should remain an explicit unsupported operator-order boundary; use FVector * scalar"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Operator precedence and associativity: ensure correct evaluation order.
	// -------------------------------------------------------------------------
	TEST_METHOD(OperatorPrecedenceAndAssociativity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_Precedence", ASTEST_AS(R"AS(
		// Multiplication before addition: 2 + 3 * 4 = 14
		int MultiplyBeforeAdd()
		{
			return 2 + 3 * 4;
		}

		// Division before subtraction: 20 - 10 / 2 = 15
		int DivideBeforeSub()
		{
			return 20 - 10 / 2;
		}

		// Parentheses override: (2 + 3) * 4 = 20
		int ParenthesesOverride()
		{
			return (2 + 3) * 4;
		}

		// Left-to-right associativity: 100 / 5 / 2 = 10
		int LeftToRightDiv()
		{
			return 100 / 5 / 2;
		}

		// Bitwise before comparison: (5 & 3) < 4
		bool BitwiseBeforeComparison()
		{
			return (5 & 3) < 4;
		}

		// Shift before addition: (1 << 3) + 2 = 10
		int ShiftBeforeAdd()
		{
			return (1 << 3) + 2;
		}

		// Comparison before logical: (5 > 3) && (2 < 4)
		bool ComparisonBeforeLogical()
		{
			return (5 > 3) && (2 < 4);
		}

		// Complex expression: 2 + 3 * 4 - 10 / 2 = 9
		int ComplexExpression()
		{
			return 2 + 3 * 4 - 10 / 2;
		}

		// Unary minus precedence: -5 * 3 = -15
		int UnaryMinusPrecedence()
		{
			return -5 * 3;
		}

		// Pre-increment in expression: (++x) * 2
		int PreIncrementInExpression()
		{
			int x = 5;
			return (++x) * 2;
		}

		// Post-increment in expression: (x++) * 2
		int PostIncrementInExpression()
		{
			int x = 5;
			int result = (x++) * 2;
			return result;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int MultiplyBeforeAdd()"), 14, TEXT("* before +"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int DivideBeforeSub()"), 15, TEXT("/ before -"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ParenthesesOverride()"), 20, TEXT("() override precedence"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int LeftToRightDiv()"), 10, TEXT("left-to-right associativity"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool BitwiseBeforeComparison()"), true, TEXT("& before <"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ShiftBeforeAdd()"), 10, TEXT("<< before +"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool ComparisonBeforeLogical()"), true, TEXT("> before &&"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ComplexExpression()"), 9, TEXT("complex precedence"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int UnaryMinusPrecedence()"), -15, TEXT("unary - before *"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int PreIncrementInExpression()"), 12, TEXT("++x in expression"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int PostIncrementInExpression()"), 10, TEXT("x++ in expression"));
	}

	TEST_METHOD(DeclarationContextEdges)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_DeclarationEdges", ASTEST_AS(R"AS(
		int MultipleLocalDeclarations()
		{
			int A = 1, B = 2, C = 3;
			return A + B + C;
		}

		int ExpressionInitializedLocal()
		{
			int A = 10;
			int B = 20;
			int Result = A + B * 2;
			return Result;
		}

		int GetInitializerValue()
		{
			return 33;
		}

		int FunctionCallInitializedLocal()
		{
			int Result = GetInitializerValue();
			return Result;
		}

		int AutoComplexExpression()
		{
			auto Result = int8(4) + int16(8) + 30;
			return Result;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int declaration-edge module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(6, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int MultipleLocalDeclarations()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("multiple local int declarations should initialize in order")));
		ASSERT_THAT(AreEqual(50, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int ExpressionInitializedLocal()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("local int should initialize from an expression")));
		ASSERT_THAT(AreEqual(33, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int FunctionCallInitializedLocal()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("local int should initialize from a function call")));
		ASSERT_THAT(AreEqual(42, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int AutoComplexExpression()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("auto should infer an int-compatible result from mixed narrow integer expression")));
	}

	TEST_METHOD(ClassMembersNonPropertyAllWidths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_ClassMemberWidths", ASTEST_AS(R"AS(
		class IntWidthHolder
		{
			int8 Int8Value;
			int16 Int16Value;
			uint8 UInt8Value;
			uint16 UInt16Value;
			uint64 UInt64Value;

			IntWidthHolder()
			{
				Int8Value = -12;
				Int16Value = -1234;
				UInt8Value = 250;
				UInt16Value = 60000;
				UInt64Value = 12000000000000000000;
			}
		}

		int8 ReadInt8Member()
		{
			IntWidthHolder Holder;
			return Holder.Int8Value;
		}

		int16 ReadInt16Member()
		{
			IntWidthHolder Holder;
			return Holder.Int16Value;
		}

		uint8 ReadUInt8Member()
		{
			IntWidthHolder Holder;
			return Holder.UInt8Value;
		}

		uint16 ReadUInt16Member()
		{
			IntWidthHolder Holder;
			return Holder.UInt16Value;
		}

		uint64 ReadUInt64Member()
		{
			IntWidthHolder Holder;
			return Holder.UInt64Value;
		}

		int ModifyNarrowMembers()
		{
			IntWidthHolder Holder;
			Holder.Int8Value = 12;
			Holder.Int16Value = 30;
			Holder.UInt8Value = 40;
			Holder.UInt16Value = 50;
			return Holder.Int8Value + Holder.Int16Value + Holder.UInt8Value + Holder.UInt16Value;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int class-member width module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ExpectGlobalException(Engine, Module, TEXT("int8 ReadInt8Member()"),
			TEXT("plain class int8 member access should remain a null-boundary"), TEXT("Null pointer access"));
		ExpectGlobalException(Engine, Module, TEXT("int16 ReadInt16Member()"),
			TEXT("plain class int16 member access should remain a null-boundary"), TEXT("Null pointer access"));
		ExpectGlobalException(Engine, Module, TEXT("uint8 ReadUInt8Member()"),
			TEXT("plain class uint8 member access should remain a null-boundary"), TEXT("Null pointer access"));
		ExpectGlobalException(Engine, Module, TEXT("uint16 ReadUInt16Member()"),
			TEXT("plain class uint16 member access should remain a null-boundary"), TEXT("Null pointer access"));
		ExpectGlobalException(Engine, Module, TEXT("uint64 ReadUInt64Member()"),
			TEXT("plain class uint64 member access should remain a null-boundary"), TEXT("Null pointer access"));
		ExpectGlobalException(Engine, Module, TEXT("int ModifyNarrowMembers()"),
			TEXT("plain class narrow integer member mutation should remain a null-boundary"), TEXT("Null pointer access"));
	}

	TEST_METHOD(IntegerLiteralEdges)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_LiteralEdges", ASTEST_AS(R"AS(
		int NegativeHexLiteral()
		{
			return -0x10;
		}

		int NegativeBinaryLiteral()
		{
			return -0b1010;
		}

		uint HexUIntMaxLiteral()
		{
			return 0xFFFFFFFF;
		}

		int64 LargeHexInt64Literal()
		{
			return 0x100000000;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int literal-edge module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(-16, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int NegativeHexLiteral()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("negative hexadecimal literal should evaluate")));
		ASSERT_THAT(AreEqual(-10, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int NegativeBinaryLiteral()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("negative binary literal should evaluate")));
		ASSERT_THAT(AreEqual(static_cast<uint32>(0xFFFFFFFFu), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint HexUIntMaxLiteral()")).CallAndReturn<uint32>(0),
			TEXT("0xFFFFFFFF should be usable as uint max")));
		ASSERT_THAT(AreEqual(static_cast<int64>(0x100000000LL), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int64 LargeHexInt64Literal()")).CallAndReturn<int64>(0),
			TEXT("large hexadecimal literal should fit int64")));
	}

	TEST_METHOD(IntegerConversionLossAndOutOfRange)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_ConversionLoss", ASTEST_AS(R"AS(
		int TruncateLargeInt64ToInt()
		{
			int64 Value = 10000000000;
			return int(Value);
		}

		int UnsignedToSignedLargeValue()
		{
			uint Value = 3000000000;
			return int(Value);
		}

		int DoubleTruncatesTowardZero()
		{
			double Value = -9.9;
			return int(Value);
		}

		int8 NarrowIntToInt8Wraps()
		{
			int Value = 128;
			return int8(Value);
		}

		enum EConversionEdgeEnum
		{
			Zero = 0,
			One = 1
		}

		int OutOfRangeEnumAsInt()
		{
			EConversionEdgeEnum Value = EConversionEdgeEnum(999);
			return int(Value);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int conversion-loss module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<int32>(10000000000LL), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int TruncateLargeInt64ToInt()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("int64 to int conversion should preserve the current truncation behavior")));
		ASSERT_THAT(AreEqual(static_cast<int32>(3000000000u), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int UnsignedToSignedLargeValue()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("large uint to int conversion should preserve the current bit pattern behavior")));
		ASSERT_THAT(AreEqual(-9, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int DoubleTruncatesTowardZero()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("double to int conversion should truncate toward zero for negative values")));
		ASSERT_THAT(AreEqual(static_cast<int8>(-128), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int8 NarrowIntToInt8Wraps()")).CallAndReturn<int8>(0),
			TEXT("int to int8 narrowing should preserve current wrap behavior")));
		ASSERT_THAT(AreEqual(-25, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int OutOfRangeEnumAsInt()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("out-of-range enum conversion should preserve the current int8-backed narrowing boundary")));
	}

	TEST_METHOD(ChainedNumericPromotionExpressions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_PromotionChains", ASTEST_AS(R"AS(
		int64 Int8IntInt64Chain()
		{
			int8 A = 12;
			int B = 30;
			int64 C = 9000000000;
			return A + B + C;
		}

		int64 ComplexSignedPromotion()
		{
			int8 A = 4;
			int16 B = 6;
			int64 C = 1000000000;
			return (A + B) * C;
		}

		uint64 UnsignedPromotionChain()
		{
			uint8 A = 200;
			uint B = 3000000000;
			uint64 C = 10000000000000000000;
			return A + B + C;
		}

		int64 ExplicitSignedUnsignedChain()
		{
			int8 A = -5;
			uint16 B = 40;
			int64 C = 1000;
			return int64(A) + int64(B) + C;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int promotion-chain module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<int64>(9000000042LL), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int64 Int8IntInt64Chain()")).CallAndReturn<int64>(0),
			TEXT("int8 + int + int64 should promote through the chain")));
		ASSERT_THAT(AreEqual(static_cast<int64>(10000000000LL), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int64 ComplexSignedPromotion()")).CallAndReturn<int64>(0),
			TEXT("(int8 + int16) * int64 should promote before multiplication")));
		ASSERT_THAT(AreEqual(static_cast<uint64>(10000000003000000200ull), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint64 UnsignedPromotionChain()")).CallAndReturn<uint64>(0),
			TEXT("uint8 + uint + uint64 should promote through the chain")));
		ASSERT_THAT(AreEqual(static_cast<int64>(1035), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int64 ExplicitSignedUnsignedChain()")).CallAndReturn<int64>(0),
			TEXT("explicit signed/unsigned promotion chain should evaluate")));
	}

	TEST_METHOD(ComplexIntegerExpressionEvaluation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntExpr_ComplexEvaluation", ASTEST_AS(R"AS(
		int NestedArithmeticExpression()
		{
			int A = 8;
			int B = 4;
			int C = 20;
			int D = 5;
			int E = 23;
			int F = 6;
			return (A + B) * (C - D) / (E % F);
		}

		int MixedBitwiseExpression()
		{
			int A = 0xF0;
			int B = 0xCC;
			int C = 0x0F;
			int D = 0x33;
			return (A & B) | (C ^ D);
		}

		int TernaryIntegerExpression(bool bUsePositive)
		{
			int Positive = 42;
			int Negative = -42;
			return bUsePositive ? Positive : Negative;
		}

		int AssignmentExpressionValue()
		{
			int X = 0;
			int Y = 0;
			X = Y = 7;
			return X * 10 + Y;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int complex-expression module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(36, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int NestedArithmeticExpression()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("nested arithmetic expression should evaluate in the expected order")));
		ASSERT_THAT(AreEqual(0xFC, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int MixedBitwiseExpression()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("mixed bitwise expression should evaluate in the expected order")));
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int TernaryIntegerExpression(bool)"));
			Invoker.AddArg(true);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("ternary integer expression should choose true branch")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int TernaryIntegerExpression(bool)"));
			Invoker.AddArg(false);
			ASSERT_THAT(AreEqual(-42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("ternary integer expression should choose false branch")));
		}
		ASSERT_THAT(AreEqual(77, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int AssignmentExpressionValue()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("chained assignment should update both integer variables")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
