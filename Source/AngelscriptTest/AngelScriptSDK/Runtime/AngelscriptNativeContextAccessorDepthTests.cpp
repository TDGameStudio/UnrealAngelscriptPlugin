#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeContextAccessorDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.ContextAccessorDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FString BuildSource(const bool bFloatIsFloat64)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int AccessorEcho(int8 A, int16 B, int C, int64 D, uint8 E, uint16 F, uint G, uint64 H, bool Flag)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Result = int(A) + int(B) + C + int(D) + int(E) + int(F) + int(G) + int(H);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Flag ? Result + 1 : Result;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int AccessorOverload(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value + 100;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("double AccessorOverload(double Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value + 0.25;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, bFloatIsFloat64
			? TEXT("double AccessorReturn(double Value)")
			: TEXT("float AccessorReturn(float Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value + 0.5;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static bool ExecuteIntegerEcho(
		FAutomationTestBase& Test,
		asIScriptFunction* Function,
		asIScriptContext* Context)
	{
		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsNotNull(Function, TEXT("Context accessor product should resolve the exact integer overload"))
			|| !Assert.IsNotNull(Context, TEXT("Context accessor product should create its shared context")))
		{
			return false;
		}

		const bool bPrepared = Assert.AreEqual(asSUCCESS, Context->Prepare(Function), TEXT("Context accessor product should prepare the full scalar argument list"));
		if (!bPrepared)
		{
			return false;
		}

		bool bValid = true;
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgByte(0, static_cast<asBYTE>(static_cast<int8>(-2))), TEXT("Context accessor product should set the int8 argument through the byte slot"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgWord(1, static_cast<asWORD>(static_cast<int16>(3))), TEXT("Context accessor product should set the int16 argument through the word slot"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgDWord(2, static_cast<asDWORD>(4)), TEXT("Context accessor product should set the int argument through the dword slot"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgQWord(3, static_cast<asQWORD>(5)), TEXT("Context accessor product should set the int64 argument through the qword slot"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgByte(4, static_cast<asBYTE>(6)), TEXT("Context accessor product should set the uint8 argument through the byte slot"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgWord(5, static_cast<asWORD>(7)), TEXT("Context accessor product should set the uint16 argument through the word slot"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgDWord(6, static_cast<asDWORD>(8)), TEXT("Context accessor product should set the uint argument through the dword slot"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgQWord(7, static_cast<asQWORD>(9)), TEXT("Context accessor product should set the uint64 argument through the qword slot"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->SetArgByte(8, 1), TEXT("Context accessor product should set the bool argument through the byte slot"));
		for (asUINT ArgumentIndex = 0; ArgumentIndex < 9; ++ArgumentIndex)
		{
			bValid &= Assert.IsNotNull(
				Context->GetAddressOfArg(ArgumentIndex),
				*FString::Printf(TEXT("Context accessor product should expose a valid address for scalar argument %u"), ArgumentIndex));
		}
		bValid &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Context accessor product should execute the complete scalar signature"));
		bValid &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), static_cast<int32>(Context->GetState()), TEXT("Context accessor product should expose the finished state after execution"));
		bValid &= Assert.AreEqual(41, static_cast<int32>(Context->GetReturnDWord()), TEXT("Context accessor product should preserve every scalar slot in the integer result"));
		bValid &= Assert.AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Context accessor product should unprepare after the scalar invocation"));
		return bValid;
	}

	static asIScriptFunction* FindExactScalarFunction(
		asIScriptModule* Module,
		const ANSICHAR* Name,
		const int ParameterTypeId,
		const int ReturnTypeId)
	{
		if (Module == nullptr || Name == nullptr)
		{
			return nullptr;
		}

		asIScriptFunction* Match = nullptr;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module->GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Candidate = Module->GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr
				|| FCStringAnsi::Strcmp(Candidate->GetName(), Name) != 0
				|| (Candidate->GetNamespace() != nullptr && Candidate->GetNamespace()[0] != '\0')
				|| Candidate->GetParamCount() != 1
				|| Candidate->GetReturnTypeId() != ReturnTypeId)
			{
				continue;
			}

			int CandidateParameterTypeId = asINVALID_TYPE;
			if (Candidate->GetParam(0, &CandidateParameterTypeId) < 0
				|| CandidateParameterTypeId != ParameterTypeId)
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

public:

	TEST_METHOD(ScalarArgumentSlotsAndExactOverloads)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-ARGUMENT-SCALAR-SLOTS",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		AS_NATIVE_PRODUCT("RT-CTX-STATE-TRANSITIONS",
			ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);
		AS_NATIVE_PRODUCT("RT-CTX-OVERLOAD-EXACT-INVOCATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Context accessor products should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const bool bFloatIsFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString ModuleName = TEXT("NativeContextAccessorDepth");
		const FString Source = BuildSource(bFloatIsFloat64);
		PrintGeneratedAsSource(*TestRunner, TEXT("RT-CTX-ARGUMENT-SCALAR-SLOTS"), ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult >= 0,
			*FString::Printf(TEXT("Context accessor source should compile. Build=%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(Module, TEXT("Context accessor source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleNameUtf8.Get()); };

		asIScriptFunction* const ScalarFunction = GetNativeFunctionByExactDecl(
			Module,
			"int AccessorEcho(const int8, const int16, const int, const int64, const uint8, const uint16, const uint, const uint64, const bool)");
		asIScriptFunction* const IntegerOverload = GetNativeFunctionByExactDecl(Module, "int AccessorOverload(const int)");
		// The fork normalizes floating declarations differently across float ABI
		// modes, so resolve the overload by its exact reflected parameter/return
		// type ids instead of relying on one spelling of GetFunctionByDecl().
		const int DoubleTypeId = ScriptEngine->GetTypeIdByDecl("double");
		asIScriptFunction* const FloatingOverload = FindExactScalarFunction(Module, "AccessorOverload", DoubleTypeId, DoubleTypeId);
		const int FloatingReturnTypeId = ScriptEngine->GetTypeIdByDecl(bFloatIsFloat64 ? "double" : "float");
		asIScriptFunction* const FloatingReturn = FindExactScalarFunction(Module, "AccessorReturn", FloatingReturnTypeId, FloatingReturnTypeId);
		ASSERT_THAT(IsNotNull(ScalarFunction, TEXT("Context accessor product should resolve the complete scalar declaration exactly")));
		ASSERT_THAT(IsNotNull(IntegerOverload, TEXT("Context accessor product should resolve the integer overload exactly")));
		ASSERT_THAT(IsNotNull(FloatingOverload, TEXT("Context accessor product should resolve the floating overload exactly")));
		ASSERT_THAT(IsNotNull(FloatingReturn, TEXT("Context accessor product should resolve the floating return function exactly")));
		if (ScalarFunction == nullptr || IntegerOverload == nullptr || FloatingOverload == nullptr || FloatingReturn == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Context accessor product should create one reusable context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_UNINITIALIZED), static_cast<int32>(Context->GetState()), TEXT("Context accessor product should begin uninitialized")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(ScalarFunction), TEXT("Context accessor product should expose the prepared state before arguments are written")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_PREPARED), static_cast<int32>(Context->GetState()), TEXT("Context accessor product should report the prepared state")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Context accessor product should allow an explicit unprepare before invocation")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_UNINITIALIZED), static_cast<int32>(Context->GetState()), TEXT("Context accessor product should return to the uninitialized state after unprepare")));
		TestRunner->AddInfo(TEXT("[AS-FORK-LIMITATION] asCContext::SetArg* and GetAddressOfArg do not bounds-check indices in this fork; unsafe out-of-range probes are intentionally not executed"));

		const bool bIntegerEchoSucceeded = ExecuteIntegerEcho(*TestRunner, ScalarFunction, Context);
		ASSERT_THAT(IsTrue(bIntegerEchoSucceeded, TEXT("Context accessor product should complete and clean up the scalar invocation")));
		if (!bIntegerEchoSucceeded)
		{
			return;
		}

		const int IntegerPrepareResult = Context->Prepare(IntegerOverload);
		ASSERT_THAT(AreEqual(asSUCCESS, IntegerPrepareResult, TEXT("Context accessor product should prepare the integer overload")));
		if (IntegerPrepareResult != asSUCCESS)
		{
			return;
		}
		const int IntegerSetResult = Context->SetArgDWord(0, 2);
		ASSERT_THAT(AreEqual(asSUCCESS, IntegerSetResult, TEXT("Context accessor product should set the integer overload argument")));
		if (IntegerSetResult != asSUCCESS)
		{
			Context->Unprepare();
			return;
		}
		const int IntegerExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), IntegerExecuteResult, TEXT("Context accessor product should execute the integer overload")));
		if (IntegerExecuteResult != asEXECUTION_FINISHED)
		{
			Context->Unprepare();
			return;
		}
		ASSERT_THAT(AreEqual(102, static_cast<int32>(Context->GetReturnDWord()), TEXT("Context accessor product should prove the integer overload was selected")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Context accessor product should unprepare the integer overload")));

		const int FloatingPrepareResult = Context->Prepare(FloatingOverload);
		ASSERT_THAT(AreEqual(asSUCCESS, FloatingPrepareResult, TEXT("Context accessor product should prepare the floating overload")));
		if (FloatingPrepareResult != asSUCCESS)
		{
			return;
		}
		int FloatingSetResult = asERROR;
		if (bFloatIsFloat64)
		{
			FloatingSetResult = Context->SetArgDouble(0, 2.0);
			ASSERT_THAT(AreEqual(asSUCCESS, FloatingSetResult, TEXT("Context accessor product should set a double-backed floating overload argument")));
		}
		else
		{
			FloatingSetResult = Context->SetArgFloat(0, 2.0f);
			ASSERT_THAT(AreEqual(asSUCCESS, FloatingSetResult, TEXT("Context accessor product should set a float-backed floating overload argument")));
		}
		if (FloatingSetResult != asSUCCESS)
		{
			Context->Unprepare();
			return;
		}
		const int FloatingExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), FloatingExecuteResult, TEXT("Context accessor product should execute the floating overload")));
		if (FloatingExecuteResult != asEXECUTION_FINISHED)
		{
			Context->Unprepare();
			return;
		}
		if (bFloatIsFloat64)
		{
			ASSERT_THAT(IsNear(2.25, Context->GetReturnDouble(), 0.0001, TEXT("Context accessor product should prove the double-backed floating overload was selected")));
		}
		else
		{
			ASSERT_THAT(IsNear(2.25f, Context->GetReturnFloat(), 0.0001f, TEXT("Context accessor product should prove the float-backed floating overload was selected")));
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Context accessor product should unprepare the floating overload")));

		const int FloatingReturnPrepareResult = Context->Prepare(FloatingReturn);
		ASSERT_THAT(AreEqual(asSUCCESS, FloatingReturnPrepareResult, TEXT("Context accessor product should prepare the floating return path")));
		if (FloatingReturnPrepareResult != asSUCCESS)
		{
			return;
		}
		int FloatingReturnSetResult = asERROR;
		if (bFloatIsFloat64)
		{
			FloatingReturnSetResult = Context->SetArgDouble(0, 3.5);
			ASSERT_THAT(AreEqual(asSUCCESS, FloatingReturnSetResult, TEXT("Context accessor product should set the double-backed return input")));
		}
		else
		{
			FloatingReturnSetResult = Context->SetArgFloat(0, 3.5f);
			ASSERT_THAT(AreEqual(asSUCCESS, FloatingReturnSetResult, TEXT("Context accessor product should set the float-backed return input")));
		}
		if (FloatingReturnSetResult != asSUCCESS)
		{
			Context->Unprepare();
			return;
		}
		const int FloatingReturnExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), FloatingReturnExecuteResult, TEXT("Context accessor product should execute the floating return path")));
		if (FloatingReturnExecuteResult != asEXECUTION_FINISHED)
		{
			Context->Unprepare();
			return;
		}
		if (bFloatIsFloat64)
		{
			ASSERT_THAT(IsNear(4.0, Context->GetReturnDouble(), 0.0001, TEXT("Context accessor product should read the double-backed return slot")));
		}
		else
		{
			ASSERT_THAT(IsNear(4.0f, Context->GetReturnFloat(), 0.0001f, TEXT("Context accessor product should read the float-backed return slot")));
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Context accessor product should unprepare the floating return path")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
