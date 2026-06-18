#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include <cstring>

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptSDKTestSupport;

namespace
{
	bool GCalled = false;
	int32 GIntResult = 0;
	bool GCleanupCalled = false;
	bool GCleanupUserDataMatched = false;
	int32 GFourArgInt = 0;
	int32 GFourArgShort = 0;
	int32 GFourArgByte = 0;
	int32 GFourArgTail = 0;
	float GFloatArgA = 0.0f;
	float GFloatArgB = 0.0f;
	double GFloatArgC = 0.0;
	float GFloatArgD = 0.0f;
	double GDoubleArgA = 0.0;
	double GDoubleArgB = 0.0;
	double GDoubleArgC = 0.0;
	double GDoubleArgD = 0.0;

	void ResetExecuteState()
	{
		GCalled = false;
		GIntResult = 0;
		GCleanupCalled = false;
		GCleanupUserDataMatched = false;
		GFourArgInt = 0;
		GFourArgShort = 0;
		GFourArgByte = 0;
		GFourArgTail = 0;
		GFloatArgA = 0.0f;
		GFloatArgB = 0.0f;
		GFloatArgC = 0.0;
		GFloatArgD = 0.0f;
		GDoubleArgA = 0.0;
		GDoubleArgB = 0.0;
		GDoubleArgC = 0.0;
		GDoubleArgD = 0.0;
	}

	bool UsesMaxPortability()
	{
		return std::strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") != nullptr;
	}

	void CFunctionBasic()
	{
		GCalled = true;
	}

	void CFunctionBasicGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionBasic();
		}
	}

	void CleanupContext(asIScriptContext* Context)
	{
		GCleanupCalled = true;
		GCleanupUserDataMatched = Context != nullptr && Context->GetUserData() == reinterpret_cast<void*>(static_cast<SIZE_T>(0xDEADF00D));
	}

	void CFunctionOneArg(int Value)
	{
		GCalled = true;
		GIntResult = Value;
	}

	void CFunctionOneArgGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionOneArg(static_cast<int>(Generic->GetArgDWord(0)));
		}
	}

	void CFunctionTwoArgs(int A, int B)
	{
		GCalled = true;
		GIntResult = A + B;
	}

	void CFunctionTwoArgsGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionTwoArgs(static_cast<int>(Generic->GetArgDWord(0)), static_cast<int>(Generic->GetArgDWord(1)));
		}
	}

	void CFunctionFourArgs(int A, short B, char C, int D)
	{
		GCalled = true;
		GFourArgInt = A;
		GFourArgShort = B;
		GFourArgByte = C;
		GFourArgTail = D;
	}

	void CFunctionFourArgsGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionFourArgs(
				static_cast<int>(Generic->GetArgDWord(0)),
				*static_cast<short*>(Generic->GetAddressOfArg(1)),
				*static_cast<char*>(Generic->GetAddressOfArg(2)),
				static_cast<int>(Generic->GetArgDWord(3)));
		}
	}

	void CFunctionFloatArgs(float A, float B, double C, float D)
	{
		GCalled = true;
		GFloatArgA = A;
		GFloatArgB = B;
		GFloatArgC = C;
		GFloatArgD = D;
	}

	void CFunctionDoubleArgs(double A, double B, double C, double D)
	{
		GCalled = true;
		GDoubleArgA = A;
		GDoubleArgB = B;
		GDoubleArgC = C;
		GDoubleArgD = D;
	}

	void CFunctionFloatArgsGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionFloatArgs(
				Generic->GetArgFloat(0),
				Generic->GetArgFloat(1),
				Generic->GetArgDouble(2),
				Generic->GetArgFloat(3));
		}
	}

	void CFunctionDoubleArgsGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionDoubleArgs(
				Generic->GetArgDouble(0),
				Generic->GetArgDouble(1),
				Generic->GetArgDouble(2),
				Generic->GetArgDouble(3));
		}
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKExecuteTests,
	"Angelscript.TestModule.AngelScriptSDK.Execute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
		ResetExecuteState();
	}

	TEST_METHOD(BasicCallback)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK execute basic-callback test should create a script engine"), ScriptEngine))
		{
			return;
		}

		const int RegisterResult = UsesMaxPortability()
			? ScriptEngine->RegisterGlobalFunction("void cfunction_basic()", asFUNCTION(CFunctionBasicGeneric), asCALL_GENERIC)
			: [&]()
			{
				const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(CFunctionBasic);
				return ScriptEngine->RegisterGlobalFunction("void cfunction_basic()", asFUNCTION(CFunctionBasic), asCALL_CDECL, *(asFunctionCaller*)&Caller);
			}();
		if (!TestRunner->TestEqual(TEXT("SDK execute basic-callback test should register the callback"), RegisterResult >= 0, true))
		{
			return;
		}

		const int ExecuteResult = SDKExecuteString(ScriptEngine, "cfunction_basic();");
		if (!TestRunner->TestEqual(TEXT("SDK execute basic-callback test should execute a statement snippet"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK execute basic-callback test should call the registered function"), GCalled))
		{
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("SDK execute basic-callback test should create a context"), Context))
		{
			return;
		}

		Context->SetUserData(reinterpret_cast<void*>(static_cast<SIZE_T>(0xDEADF00D)));
		ScriptEngine->SetContextUserDataCleanupCallback(CleanupContext);
		const int PrepareResult = Context->Prepare(ScriptEngine->GetGlobalFunctionByDecl("void cfunction_basic()"));
		Context->Release();

		TestRunner->TestEqual(TEXT("SDK execute basic-callback test should prepare the callback function"), PrepareResult, static_cast<int32>(asSUCCESS));
		TestRunner->TestTrue(TEXT("SDK execute basic-callback test should trigger context cleanup on release"), GCleanupCalled);
		TestRunner->TestTrue(TEXT("SDK execute basic-callback test should preserve context user data for cleanup"), GCleanupUserDataMatched);
	}

	TEST_METHOD(OneArg)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK execute one-arg test should create a script engine"), ScriptEngine))
		{
			return;
		}

		const int FunctionId = UsesMaxPortability()
			? ScriptEngine->RegisterGlobalFunction("void cfunction_one(int value)", asFUNCTION(CFunctionOneArgGeneric), asCALL_GENERIC)
			: [&]()
			{
				const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(CFunctionOneArg);
				return ScriptEngine->RegisterGlobalFunction("void cfunction_one(int value)", asFUNCTION(CFunctionOneArg), asCALL_CDECL, *(asFunctionCaller*)&Caller);
			}();
		if (!TestRunner->TestTrue(TEXT("SDK execute one-arg test should register the callback"), FunctionId >= 0))
		{
			return;
		}

		const int ExecuteResult = SDKExecuteString(ScriptEngine, "cfunction_one(5);");
		if (!TestRunner->TestEqual(TEXT("SDK execute one-arg test should execute the statement snippet"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK execute one-arg test should call the registered function"), GCalled))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK execute one-arg test should pass the correct value through the snippet"), GIntResult, 5))
		{
			return;
		}

		ResetExecuteState();
		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("SDK execute one-arg test should create a direct-call context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(ScriptEngine->GetFunctionById(FunctionId));
		if (!TestRunner->TestEqual(TEXT("SDK execute one-arg test should prepare the direct-call context"), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return;
		}

		Context->SetArgDWord(0, 5);
		const int DirectExecuteResult = Context->Execute();
		Context->Release();

		TestRunner->TestEqual(TEXT("SDK execute one-arg test should finish the direct callback execution"), DirectExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestTrue(TEXT("SDK execute one-arg test should call the direct callback"), GCalled);
		TestRunner->TestEqual(TEXT("SDK execute one-arg test should preserve the direct callback argument"), GIntResult, 5);
	}

	TEST_METHOD(TwoArgs)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK execute two-args test should create a script engine"), ScriptEngine))
		{
			return;
		}

		const int RegisterResult = UsesMaxPortability()
			? ScriptEngine->RegisterGlobalFunction("void cfunction_two(int left, int right)", asFUNCTION(CFunctionTwoArgsGeneric), asCALL_GENERIC)
			: [&]()
			{
				const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(CFunctionTwoArgs);
				return ScriptEngine->RegisterGlobalFunction("void cfunction_two(int left, int right)", asFUNCTION(CFunctionTwoArgs), asCALL_CDECL, *(asFunctionCaller*)&Caller);
			}();
		if (!TestRunner->TestTrue(TEXT("SDK execute two-args test should register the callback"), RegisterResult >= 0))
		{
			return;
		}

		const int ExecuteResult = SDKExecuteString(ScriptEngine, "cfunction_two(5, 9);");
		TestRunner->TestEqual(TEXT("SDK execute two-args test should execute the statement snippet"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestTrue(TEXT("SDK execute two-args test should call the registered function"), GCalled);
		TestRunner->TestEqual(TEXT("SDK execute two-args test should sum both arguments"), GIntResult, 14);
	}

	TEST_METHOD(FourArgs)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK execute four-args test should create a script engine"), ScriptEngine))
		{
			return;
		}

		const int RegisterResult = UsesMaxPortability()
			? ScriptEngine->RegisterGlobalFunction("void cfunction_four(int first, int16 second, int8 third, int fourth)", asFUNCTION(CFunctionFourArgsGeneric), asCALL_GENERIC)
			: [&]()
			{
				const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(CFunctionFourArgs);
				return ScriptEngine->RegisterGlobalFunction("void cfunction_four(int first, int16 second, int8 third, int fourth)", asFUNCTION(CFunctionFourArgs), asCALL_CDECL, *(asFunctionCaller*)&Caller);
			}();
		if (!TestRunner->TestTrue(TEXT("SDK execute four-args test should register the callback"), RegisterResult >= 0))
		{
			return;
		}

		const int ExecuteResult = SDKExecuteString(ScriptEngine, "cfunction_four(5, 9, 1, 3);");
		TestRunner->TestEqual(TEXT("SDK execute four-args test should execute the statement snippet"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestTrue(TEXT("SDK execute four-args test should call the registered function"), GCalled);
		TestRunner->TestEqual(TEXT("SDK execute four-args test should preserve the first argument"), GFourArgInt, 5);
		TestRunner->TestEqual(TEXT("SDK execute four-args test should preserve the int16 argument"), GFourArgShort, 9);
		TestRunner->TestEqual(TEXT("SDK execute four-args test should preserve the int8 argument"), GFourArgByte, 1);
		TestRunner->TestEqual(TEXT("SDK execute four-args test should preserve the trailing argument"), GFourArgTail, 3);
	}

	TEST_METHOD(FloatArgs)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK execute float-args test should create a script engine"), ScriptEngine))
		{
			return;
		}

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const char* Declaration = bFloatUsesFloat64
			? "void cfunction_float(double first, double second, double third, double fourth)"
			: "void cfunction_float(float first, float second, double third, float fourth)";
		const char* ScriptCall = bFloatUsesFloat64
			? "cfunction_float(9.2, 13.3, 18.8, 3.1415);"
			: "cfunction_float(9.2f, 13.3f, 18.8, 3.1415f);";

		int RegisterResult = asERROR;
		if (!UsesMaxPortability())
		{
			const ASAutoCaller::FunctionCaller Caller = bFloatUsesFloat64
				? ASAutoCaller::MakeFunctionCaller(CFunctionDoubleArgs)
				: ASAutoCaller::MakeFunctionCaller(CFunctionFloatArgs);
			RegisterResult = ScriptEngine->RegisterGlobalFunction(
				Declaration,
				bFloatUsesFloat64 ? asFUNCTION(CFunctionDoubleArgs) : asFUNCTION(CFunctionFloatArgs),
				asCALL_CDECL,
				*(asFunctionCaller*)&Caller);
		}

		if (RegisterResult < 0)
		{
			RegisterResult = ScriptEngine->RegisterGlobalFunction(
				Declaration,
				bFloatUsesFloat64 ? asFUNCTION(CFunctionDoubleArgsGeneric) : asFUNCTION(CFunctionFloatArgsGeneric),
				asCALL_GENERIC);
		}
		if (!TestRunner->TestTrue(TEXT("SDK execute float-args test should register the callback"), RegisterResult >= 0))
		{
			return;
		}

		const int ExecuteResult = SDKExecuteString(ScriptEngine, ScriptCall);
		TestRunner->TestEqual(TEXT("SDK execute float-args test should execute the statement snippet"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED));
		TestRunner->TestTrue(TEXT("SDK execute float-args test should call the registered function"), GCalled);
		if (bFloatUsesFloat64)
		{
			TestRunner->TestTrue(TEXT("SDK execute float-args test should preserve the first promoted double"), FMath::IsNearlyEqual(GDoubleArgA, 9.2, 0.0001));
			TestRunner->TestTrue(TEXT("SDK execute float-args test should preserve the second promoted double"), FMath::IsNearlyEqual(GDoubleArgB, 13.3, 0.0001));
			TestRunner->TestTrue(TEXT("SDK execute float-args test should preserve the third double"), FMath::IsNearlyEqual(GDoubleArgC, 18.8, 0.0001));
			TestRunner->TestTrue(TEXT("SDK execute float-args test should preserve the fourth promoted double"), FMath::IsNearlyEqual(GDoubleArgD, 3.1415, 0.0001));
		}
		else
		{
			TestRunner->TestTrue(TEXT("SDK execute float-args test should preserve the first float"), FMath::IsNearlyEqual(GFloatArgA, 9.2f, 0.0001f));
			TestRunner->TestTrue(TEXT("SDK execute float-args test should preserve the second float"), FMath::IsNearlyEqual(GFloatArgB, 13.3f, 0.0001f));
			TestRunner->TestTrue(TEXT("SDK execute float-args test should preserve the double argument"), FMath::IsNearlyEqual(GFloatArgC, 18.8, 0.0001));
			TestRunner->TestTrue(TEXT("SDK execute float-args test should preserve the trailing float"), FMath::IsNearlyEqual(GFloatArgD, 3.1415f, 0.0001f));
		}
	}
};

#endif
