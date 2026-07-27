// Raw SDK native call-function coverage.
// Tests for as_callfunc.cpp - native function call dispatch edge cases.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.Embedding.CallFunction.*

#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FCallFunctionTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.CallFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 AddFour(int32 A, int32 B, int32 C, int32 D)
	{
		return A + B + C + D;
	}

	static double MultiplyDouble(double A, double B)
	{
		return A * B;
	}

	inline static int32 GSideEffectAccumulator = 0;

	static void AccumulateValue(int32 Value)
	{
		GSideEffectAccumulator += Value;
	}

	static int32 IncrementAndReturn(int32 Value)
	{
		return Value + 1;
	}

	static int32 SumSix(int32 A, int32 B, int32 C, int32 D, int32 E, int32 F)
	{
		return A + B + C + D + E + F;
	}

	static int64 WidenAndScale(int32 Value)
	{
		return static_cast<int64>(Value) * 1000000000LL;
	}

	static double MixIn025(int32 I, double D)
	{
		return static_cast<double>(I) + D;
	}

	static bool IsPositive(int32 Value)
	{
		return Value > 0;
	}
	static void DivMod(int32 A, int32 B, int32& OutQuotient, int32& OutRemainder)
	{
		OutQuotient = (B != 0) ? (A / B) : 0;
		OutRemainder = (B != 0) ? (A % B) : 0;
	}

	static void ReportSource(
		FAutomationTestBase& Test,
		const TCHAR* SourceId,
		const ANSICHAR* ModuleName,
		const std::string& Source)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			SourceId,
			UTF8_TO_TCHAR(ModuleName),
			UTF8_TO_TCHAR(Source.c_str()));
	}

public:
	TEST_METHOD(CallFunctionMultipleArgs)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("EMBED-NATIVE-CALL-ABI-SHAPES",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(AddFour);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"int AddFour(int A, int B, int C, int D)",
			asFUNCTION(AddFour),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(
			RegistrationResult >= 0,
			*FString::Printf(
				TEXT("Native call ABI should register the four-integer function (Result=%d, Messages=%s)"),
				RegistrationResult,
				*Engine.GetMessagesText())));
		if (RegistrationResult < 0)
		{
			return;
		}
		asIScriptFunction* const RegisteredFunction =
			SE->GetFunctionById(RegistrationResult);
		ASSERT_THAT(IsNotNull(
			RegisteredFunction,
			TEXT("Native call ABI should publish the registration-returned function ID")));
		if (RegisteredFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(
				RegistrationResult,
				RegisteredFunction->GetId(),
				TEXT("Native call ABI should preserve the registration-returned function ID")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("int AddFour(int, int, int, int)")),
				FString(UTF8_TO_TCHAR(RegisteredFunction->GetDeclaration())),
				TEXT("Native call ABI should publish the exact normalized four-integer declaration")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(4),
				static_cast<int32>(RegisteredFunction->GetParamCount()),
				TEXT("Native call ABI should publish four exact argument slots")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asTYPEID_INT32),
				RegisteredFunction->GetReturnTypeId(),
				TEXT("Native call ABI should publish the exact integer return type")));
		}
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return AddFour(10, 20, 30, 40);
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-FOUR-INT-ARGS"), "CallFuncMultiArgs", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncMultiArgs", ScriptSource);
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(100, Result, TEXT("AddFour(10,20,30,40)=100")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			M.Discard(),
			TEXT("Native call ABI should explicitly discard the four-argument module")));
		ASSERT_THAT(IsNull(
			SE->GetModule("CallFuncMultiArgs", asGM_ONLY_IF_EXISTS),
			TEXT("Four-argument module should be absent after cleanup")));

		FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};
		asIScriptEngine* const ControlScriptEngine = ControlEngine.Get();
		ASSERT_THAT(IsNotNull(
			ControlScriptEngine,
			TEXT("Native call ABI isolation should create an independent control engine")));
		if (ControlScriptEngine != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(1),
				ControlScriptEngine->GetGlobalFunctionCount(),
				TEXT("Independent native-call engine should retain only the support assert(bool) registration")));
			asIScriptFunction* const SupportFunction =
				ControlScriptEngine->GetGlobalFunctionByIndex(0);
			ASSERT_THAT(IsNotNull(
				SupportFunction,
				TEXT("Independent native-call engine should publish its sole support registration")));
			if (SupportFunction != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("assert")),
					FString(UTF8_TO_TCHAR(SupportFunction->GetName())),
					TEXT("Independent native-call engine should not contain AddFour or another product registration")));
			}
		}
	}

	TEST_METHOD(CallFunctionFloatPrecision)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-NATIVE-CALL-ABI-SHAPES",
			"double_args_return");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(MultiplyDouble);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"double MultiplyDouble(double A, double B)",
			asFUNCTION(MultiplyDouble),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("Native call ABI should register the double function")));
		if (RegistrationResult < 0)
		{
			return;
		}
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			double Entry()
			{
				return MultiplyDouble(3.14159, 2.0);
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-DOUBLE-ARGS-RETURN"), "CallFuncFloat", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncFloat", ScriptSource);
		if (!M.IsValid()) return;
		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		ASSERT_THAT(IsNear(3.14159 * 2.0, Result, 1e-10, TEXT("MultiplyDouble precision")));
	}

	TEST_METHOD(CallFunctionVoidSideEffect)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-NATIVE-CALL-ABI-SHAPES",
			"void_side_effect");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(AccumulateValue);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"void AccumulateValue(int Value)",
			asFUNCTION(AccumulateValue),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("Native call ABI should register the void side-effect function")));
		if (RegistrationResult < 0)
		{
			return;
		}
		GSideEffectAccumulator = 0;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void Entry()
			{
				AccumulateValue(10);
				AccumulateValue(20);
				AccumulateValue(12);
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-VOID-SIDE-EFFECT"), "CallFuncVoid", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncVoid", ScriptSource);
		if (!M.IsValid()) return;
		if (!ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()")) return;
		ASSERT_THAT(AreEqual(42, GSideEffectAccumulator, TEXT("Accumulator=42")));
	}

	TEST_METHOD(CallFunctionNestedCall)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-NATIVE-CALL-ABI-SHAPES",
			"nested_call");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(IncrementAndReturn);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"int IncrementAndReturn(int Value)",
			asFUNCTION(IncrementAndReturn),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("Native call ABI should register the nested-call function")));
		if (RegistrationResult < 0)
		{
			return;
		}
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return IncrementAndReturn(IncrementAndReturn(IncrementAndReturn(0)));
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-NESTED-CALL"), "CallFuncNested", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncNested", ScriptSource);
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(3, Result, TEXT("Nested 3x increment = 3")));
	}

	TEST_METHOD(CallFunctionManyArgs)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-NATIVE-CALL-ABI-SHAPES",
			"six_int_args");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(SumSix);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"int SumSix(int A, int B, int C, int D, int E, int F)",
			asFUNCTION(SumSix),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("Native call ABI should register the six-integer function")));
		if (RegistrationResult < 0)
		{
			return;
		}
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return SumSix(1, 2, 3, 4, 5, 6);
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-SIX-INT-ARGS"), "CallFuncManyArgs", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncManyArgs", ScriptSource);
		if (!M.IsValid()) return;
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(21, Result, TEXT("SumSix(1..6)=21")));
	}

	TEST_METHOD(CallFunctionWideReturn)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-NATIVE-CALL-ABI-SHAPES",
			"wide_return");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(WidenAndScale);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"int64 WidenAndScale(int Value)",
			asFUNCTION(WidenAndScale),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("Native call ABI should register the int64-return function")));
		if (RegistrationResult < 0)
		{
			return;
		}
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int64 Entry()
			{
				return WidenAndScale(3);
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-WIDE-RETURN"), "CallFuncWideReturn", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncWideReturn", ScriptSource);
		if (!M.IsValid()) return;
		asIScriptFunction* Func = GetNativeFunctionByDecl(M, "int64 Entry()");
		ASSERT_THAT(IsNotNull(Func, TEXT("Should resolve")));
		asIScriptContext* Ctx = SE->CreateContext();
		ASSERT_THAT(IsNotNull(Ctx, TEXT("Context")));
		ON_SCOPE_EXIT
		{
			Ctx->Release();
		};
		const int Ret = PrepareAndExecute(Ctx, Func);
		const int64 Result = static_cast<int64>(Ctx->GetReturnQWord());
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Ret, TEXT("Finished")));
		ASSERT_THAT(AreEqual(static_cast<int64>(3000000000LL), Result,
			TEXT("WidenAndScale(3) returns 3,000,000,000 through int64")));
	}

	TEST_METHOD(MixedIntDoubleArgs)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-NATIVE-CALL-ABI-SHAPES",
			"mixed_int_double");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(MixIn025);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"double MixIn025(int I, double D)",
			asFUNCTION(MixIn025),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("Native call ABI should register the mixed integer/double function")));
		if (RegistrationResult < 0)
		{
			return;
		}
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			double Entry()
			{
				return MixIn025(7, 0.25);
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-MIXED-INT-DOUBLE"), "CallFuncMixed", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncMixed", ScriptSource);
		if (!M.IsValid()) return;
		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		ASSERT_THAT(IsNear(7.25, Result, 1e-8,
			TEXT("MixIn025(7, 0.25) = 7.25 (int+double arg marshalling)")));
	}

	TEST_METHOD(CallFunctionBoolReturn)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-NATIVE-CALL-ABI-SHAPES",
			"bool_return");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(IsPositive);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"bool IsPositive(int Value)",
			asFUNCTION(IsPositive),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("Native call ABI should register the bool-return function")));
		if (RegistrationResult < 0)
		{
			return;
		}
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			bool Entry()
			{
				return IsPositive(5) && !IsPositive(-3) && !IsPositive(0);
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-BOOL-RETURN"), "CallFuncBool", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncBool", ScriptSource);
		if (!M.IsValid()) return;
		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "bool Entry()", bResult)) return;
		ASSERT_THAT(IsTrue(bResult, TEXT("IsPositive native bool returns marshal correctly")));
	}

	TEST_METHOD(CallFunctionOutParams)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-NATIVE-CALL-ABI-SHAPES",
			"out_params");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Native call ABI should create a case-owned raw SDK engine")));
		if (SE == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(DivMod);
		const int RegistrationResult = SE->RegisterGlobalFunction(
			"void DivMod(int A, int B, int& out OutQuotient, int& out OutRemainder)",
			asFUNCTION(DivMod),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("Native call ABI should register the two-out-parameter function")));
		if (RegistrationResult < 0)
		{
			return;
		}
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			bool Entry()
			{
				int q = 0;
				int r = 0;
				DivMod(17, 5, q, r);
				return q == 3 && r == 2;
			}
			)AS");
		ReportSource(*TestRunner, TEXT("EMBED-NATIVE-CALL-ABI-SHAPES-OUT-PARAMS"), "CallFuncOut", ScriptSource);
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "CallFuncOut", ScriptSource);
		if (!M.IsValid()) return;
		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "bool Entry()", bResult)) return;
		ASSERT_THAT(IsTrue(bResult, TEXT("DivMod(17,5) writes back q=3, r=2 through native &out params")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
