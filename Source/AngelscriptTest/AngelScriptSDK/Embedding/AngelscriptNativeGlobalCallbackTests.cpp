#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include <cstring>

#if WITH_ANGELSCRIPT_UNITTESTS




TEST_CLASS_WITH_FLAGS(FGlobalCallbackTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.GlobalCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static bool GCalled = false;
	inline static int32 GIntResult = 0;
	inline static bool GCleanupCalled = false;
	inline static bool GCleanupUserDataMatched = false;
	inline static int32 GFourArgInt = 0;
	inline static int32 GFourArgShort = 0;
	inline static int32 GFourArgByte = 0;
	inline static int32 GFourArgTail = 0;
	inline static float GFloatArgA = 0.0f;
	inline static float GFloatArgB = 0.0f;
	inline static double GFloatArgC = 0.0;
	inline static float GFloatArgD = 0.0f;
	inline static double GDoubleArgA = 0.0;
	inline static double GDoubleArgB = 0.0;
	inline static double GDoubleArgC = 0.0;
	inline static double GDoubleArgD = 0.0;

	static void ResetExecuteState()
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

	static bool UsesMaxPortability()
	{
		return std::strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") != nullptr;
	}

	static std::string BuildCallbackSource(const char* Call)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("void Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs"), Call));
		AppendGeneratedAsLine(Source, TEXT("}"));
		const FTCHARToUTF8 SourceUtf8(*Source);
		return std::string(SourceUtf8.Get(), SourceUtf8.Length());
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

	static void CFunctionBasic()
	{
		GCalled = true;
	}

	static void CFunctionBasicGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionBasic();
		}
	}

	static void CleanupContext(asIScriptContext* Context)
	{
		GCleanupCalled = true;
		GCleanupUserDataMatched = Context != nullptr && Context->GetUserData() == reinterpret_cast<void*>(static_cast<SIZE_T>(0xDEADF00D));
	}

	static void CFunctionOneArg(int Value)
	{
		GCalled = true;
		GIntResult = Value;
	}

	static void CFunctionOneArgGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionOneArg(static_cast<int>(Generic->GetArgDWord(0)));
		}
	}

	static void CFunctionTwoArgs(int A, int B)
	{
		GCalled = true;
		GIntResult = A + B;
	}

	static void CFunctionTwoArgsGeneric(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			CFunctionTwoArgs(static_cast<int>(Generic->GetArgDWord(0)), static_cast<int>(Generic->GetArgDWord(1)));
		}
	}

	static void CFunctionFourArgs(int A, short B, char C, int D)
	{
		GCalled = true;
		GFourArgInt = A;
		GFourArgShort = B;
		GFourArgByte = C;
		GFourArgTail = D;
	}

	static void CFunctionFourArgsGeneric(asIScriptGeneric* Generic)
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

	static void CFunctionFloatArgs(float A, float B, double C, float D)
	{
		GCalled = true;
		GFloatArgA = A;
		GFloatArgB = B;
		GFloatArgC = C;
		GFloatArgD = D;
	}

	static void CFunctionDoubleArgs(double A, double B, double C, double D)
	{
		GCalled = true;
		GDoubleArgA = A;
		GDoubleArgB = B;
		GDoubleArgC = C;
		GDoubleArgD = D;
	}

	static void CFunctionFloatArgsGeneric(asIScriptGeneric* Generic)
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

	static void CFunctionDoubleArgsGeneric(asIScriptGeneric* Generic)
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

	void AssertCallbackMetadata(
		asIScriptEngine* ScriptEngine,
		const int32 FunctionId,
		const TCHAR* ExpectedDeclaration,
		const asUINT ExpectedParameterCount)
	{
		using namespace AngelscriptSDKTestSupport;

		asIScriptFunction* const Function = ScriptEngine->GetFunctionById(FunctionId);
		ASSERT_THAT(IsNotNull(
			Function,
			TEXT("SDK callback registration should publish its returned function ID")));
		if (Function == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FunctionId,
			Function->GetId(),
			TEXT("SDK callback registration should preserve the exact function ID")));
		ASSERT_THAT(AreEqual(
			FString(ExpectedDeclaration),
			FString(UTF8_TO_TCHAR(Function->GetDeclaration())),
			TEXT("SDK callback registration should preserve the exact declaration")));
		ASSERT_THAT(AreEqual(
			ExpectedParameterCount,
			Function->GetParamCount(),
			TEXT("SDK callback registration should preserve the exact parameter count")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asTYPEID_VOID),
			Function->GetReturnTypeId(),
			TEXT("SDK callback registration should preserve the void return type")));
	}

	void AssertModuleCleanup(
		asIScriptEngine* ScriptEngine,
		AngelscriptNativeTestSupport::FScopedNativeModule& Module,
		const ANSICHAR* ModuleName)
	{
		using namespace AngelscriptSDKTestSupport;

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("SDK callback test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			TEXT("SDK callback module should be absent after explicit discard")));
	}

	void AssertIndependentEngineHasNoCallbacks()
	{
		using namespace AngelscriptSDKTestSupport;

		AngelscriptNativeTestSupport::FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("SDK callback test should create an independent engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(1),
				IndependentEngine.Get()->GetGlobalFunctionCount(),
				TEXT("Independent callback engine should retain only the support assert(bool) registration")));
			asIScriptFunction* const SupportFunction =
				IndependentEngine.Get()->GetGlobalFunctionByIndex(0);
			ASSERT_THAT(IsNotNull(
				SupportFunction,
				TEXT("Independent callback engine should publish its sole support registration")));
			if (SupportFunction != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("assert")),
					FString(UTF8_TO_TCHAR(SupportFunction->GetName())),
					TEXT("Independent callback engine should not contain a callback product registration")));
			}
		}
	}

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

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
		Engine.Destroy();
		Engine.Create(*TestRunner);
		ResetExecuteState();
	}

	TEST_METHOD(GlobalCallbackBasicInvocation)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK execute basic-callback test should create a script engine")));
		if (ScriptEngine == nullptr)
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
		ASSERT_THAT(IsTrue(RegisterResult >= 0, TEXT("SDK execute basic-callback test should register the callback")));
		AssertCallbackMetadata(
			ScriptEngine,
			RegisterResult,
			TEXT("void cfunction_basic()"),
			0);

		const std::string ScriptSource = BuildCallbackSource("cfunction_basic();");
		ReportSource(
			*TestRunner,
			TEXT("EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES-ZERO"),
			"GlobalCallbackBasic",
			ScriptSource);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"GlobalCallbackBasic",
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("SDK execute basic-callback test should compile its source")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ExecuteScriptVoidFunction(*TestRunner, ScriptEngine, Module, "void Entry()"),
			TEXT("SDK execute basic-callback test should execute its exact script entry")));

		ASSERT_THAT(IsTrue(GCalled, TEXT("SDK execute basic-callback test should call the registered function")));

		{
			asIScriptContext* Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("SDK execute basic-callback test should create a context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};

			Context->SetUserData(reinterpret_cast<void*>(static_cast<SIZE_T>(0xDEADF00D)));
			ScriptEngine->SetContextUserDataCleanupCallback(CleanupContext);
			const int PrepareResult = Context->Prepare(ScriptEngine->GetGlobalFunctionByDecl("void cfunction_basic()"));

			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
				TEXT("SDK execute basic-callback test should prepare the callback function")));
		}

		ASSERT_THAT(IsTrue(GCleanupCalled, TEXT("SDK execute basic-callback test should trigger context cleanup on release")));
		ASSERT_THAT(IsTrue(GCleanupUserDataMatched, TEXT("SDK execute basic-callback test should preserve context user data for cleanup")));
		AssertModuleCleanup(ScriptEngine, Module, "GlobalCallbackBasic");
		AssertIndependentEngineHasNoCallbacks();
	}

	TEST_METHOD(GlobalCallbackOneArgument)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES",
			"one_int");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK execute one-arg test should create a script engine")));
		if (ScriptEngine == nullptr)
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
		ASSERT_THAT(IsTrue(FunctionId >= 0, TEXT("SDK execute one-arg test should register the callback")));
		AssertCallbackMetadata(
			ScriptEngine,
			FunctionId,
			TEXT("void cfunction_one(int)"),
			1);

		const std::string ScriptSource = BuildCallbackSource("cfunction_one(5);");
		ReportSource(
			*TestRunner,
			TEXT("EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES-ONE-INT"),
			"GlobalCallbackOne",
			ScriptSource);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"GlobalCallbackOne",
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("SDK execute one-arg test should compile its source")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ExecuteScriptVoidFunction(*TestRunner, ScriptEngine, Module, "void Entry()"),
			TEXT("SDK execute one-arg test should execute its exact script entry")));

		ASSERT_THAT(IsTrue(GCalled, TEXT("SDK execute one-arg test should call the registered function")));

		ASSERT_THAT(AreEqual(5, GIntResult,
			TEXT("SDK execute one-arg test should pass the correct value through the snippet")));

		{
			ResetExecuteState();
			asIScriptContext* Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("SDK execute one-arg test should create a direct-call context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};

			const int PrepareResult = Context->Prepare(ScriptEngine->GetFunctionById(FunctionId));
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
				TEXT("SDK execute one-arg test should prepare the direct-call context")));

			Context->SetArgDWord(0, 5);
			const int DirectExecuteResult = Context->Execute();

			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), DirectExecuteResult,
				TEXT("SDK execute one-arg test should finish the direct callback execution")));
			ASSERT_THAT(IsTrue(GCalled, TEXT("SDK execute one-arg test should call the direct callback")));
			ASSERT_THAT(AreEqual(5, GIntResult, TEXT("SDK execute one-arg test should preserve the direct callback argument")));
		}
		AssertModuleCleanup(ScriptEngine, Module, "GlobalCallbackOne");
		AssertIndependentEngineHasNoCallbacks();
	}

	TEST_METHOD(GlobalCallbackTwoArguments)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES",
			"two_int");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK execute two-args test should create a script engine")));
		if (ScriptEngine == nullptr)
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
		ASSERT_THAT(IsTrue(RegisterResult >= 0, TEXT("SDK execute two-args test should register the callback")));
		AssertCallbackMetadata(
			ScriptEngine,
			RegisterResult,
			TEXT("void cfunction_two(int, int)"),
			2);

		const std::string ScriptSource = BuildCallbackSource("cfunction_two(5, 9);");
		ReportSource(
			*TestRunner,
			TEXT("EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES-TWO-INT"),
			"GlobalCallbackTwo",
			ScriptSource);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"GlobalCallbackTwo",
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("SDK execute two-args test should compile its source")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ExecuteScriptVoidFunction(*TestRunner, ScriptEngine, Module, "void Entry()"),
			TEXT("SDK execute two-args test should execute its exact script entry")));
		ASSERT_THAT(IsTrue(GCalled, TEXT("SDK execute two-args test should call the registered function")));
		ASSERT_THAT(AreEqual(14, GIntResult, TEXT("SDK execute two-args test should sum both arguments")));
		AssertModuleCleanup(ScriptEngine, Module, "GlobalCallbackTwo");
		AssertIndependentEngineHasNoCallbacks();
	}

	TEST_METHOD(GlobalCallbackMixedWidthArguments)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES",
			"mixed_width");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK execute four-args test should create a script engine")));
		if (ScriptEngine == nullptr)
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
		ASSERT_THAT(IsTrue(RegisterResult >= 0, TEXT("SDK execute four-args test should register the callback")));
		AssertCallbackMetadata(
			ScriptEngine,
			RegisterResult,
			TEXT("void cfunction_four(int, int16, int8, int)"),
			4);

		const std::string ScriptSource = BuildCallbackSource("cfunction_four(5, 9, 1, 3);");
		ReportSource(
			*TestRunner,
			TEXT("EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES-MIXED-WIDTH"),
			"GlobalCallbackMixedWidth",
			ScriptSource);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"GlobalCallbackMixedWidth",
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("SDK execute four-args test should compile its source")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ExecuteScriptVoidFunction(*TestRunner, ScriptEngine, Module, "void Entry()"),
			TEXT("SDK execute four-args test should execute its exact script entry")));
		ASSERT_THAT(IsTrue(GCalled, TEXT("SDK execute four-args test should call the registered function")));
		ASSERT_THAT(AreEqual(5, GFourArgInt, TEXT("SDK execute four-args test should preserve the first argument")));
		ASSERT_THAT(AreEqual(9, GFourArgShort, TEXT("SDK execute four-args test should preserve the int16 argument")));
		ASSERT_THAT(AreEqual(1, GFourArgByte, TEXT("SDK execute four-args test should preserve the int8 argument")));
		ASSERT_THAT(AreEqual(3, GFourArgTail, TEXT("SDK execute four-args test should preserve the trailing argument")));
		AssertModuleCleanup(ScriptEngine, Module, "GlobalCallbackMixedWidth");
		AssertIndependentEngineHasNoCallbacks();
	}

	TEST_METHOD(GlobalCallbackFloatingPointArguments)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES",
			"floating");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK execute float-args test should create a script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const char* Declaration = bFloatUsesFloat64
			? "void cfunction_float(double first, double second, double third, double fourth)"
			: "void cfunction_float(float first, float second, double third, float fourth)";
		const TCHAR* ExpectedDeclaration = bFloatUsesFloat64
			? TEXT("void cfunction_float(float, float, float, float)")
			: TEXT("void cfunction_float(float, float, double, float)");
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
		ASSERT_THAT(IsTrue(RegisterResult >= 0, TEXT("SDK execute float-args test should register the callback")));
		AssertCallbackMetadata(
			ScriptEngine,
			RegisterResult,
			ExpectedDeclaration,
			4);

		const std::string ScriptSource = BuildCallbackSource(ScriptCall);
		ReportSource(
			*TestRunner,
			TEXT("EMBED-GLOBAL-CALLBACK-ARGUMENT-SHAPES-FLOATING"),
			"GlobalCallbackFloating",
			ScriptSource);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"GlobalCallbackFloating",
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("SDK execute float-args test should compile its source")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ExecuteScriptVoidFunction(*TestRunner, ScriptEngine, Module, "void Entry()"),
			TEXT("SDK execute float-args test should execute its exact script entry")));
		ASSERT_THAT(IsTrue(GCalled, TEXT("SDK execute float-args test should call the registered function")));
		if (bFloatUsesFloat64)
		{
			ASSERT_THAT(IsNear(9.2, GDoubleArgA, 0.0001, TEXT("SDK execute float-args test should preserve the first promoted double")));
			ASSERT_THAT(IsNear(13.3, GDoubleArgB, 0.0001, TEXT("SDK execute float-args test should preserve the second promoted double")));
			ASSERT_THAT(IsNear(18.8, GDoubleArgC, 0.0001, TEXT("SDK execute float-args test should preserve the third double")));
			ASSERT_THAT(IsNear(3.1415, GDoubleArgD, 0.0001, TEXT("SDK execute float-args test should preserve the fourth promoted double")));
		}
		else
		{
			ASSERT_THAT(IsNear(9.2f, GFloatArgA, 0.0001f, TEXT("SDK execute float-args test should preserve the first float")));
			ASSERT_THAT(IsNear(13.3f, GFloatArgB, 0.0001f, TEXT("SDK execute float-args test should preserve the second float")));
			ASSERT_THAT(IsNear(18.8, GFloatArgC, 0.0001, TEXT("SDK execute float-args test should preserve the double argument")));
			ASSERT_THAT(IsNear(3.1415f, GFloatArgD, 0.0001f, TEXT("SDK execute float-args test should preserve the trailing float")));
		}
		AssertModuleCleanup(ScriptEngine, Module, "GlobalCallbackFloating");
		AssertIndependentEngineHasNoCallbacks();
	}
};

#endif
