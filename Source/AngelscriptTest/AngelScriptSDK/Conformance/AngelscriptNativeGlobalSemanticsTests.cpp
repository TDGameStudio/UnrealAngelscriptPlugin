#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FGlobalSemanticsTests, "Angelscript.TestModule.AngelScriptSDK.Conformance.GlobalSemantics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const std::string RecursiveSource = ASTEST_AS_ANSI(R"AS(
		void recursive(int n)
		{
			if (n > 0)
			{
				recursive(n - 1);
			}
		}
	)AS");

public:
	TEST_METHOD(GlobalSemanticsDataLimit)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("CONF-RECURSION-DATA-STACK-LIMIT",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeMessageCollector Messages;
		asIScriptEngine* const Engine = CreateNativeEngine(&Messages);
		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(Engine);
		};
		ASSERT_THAT(IsNotNull(Engine, TEXT("Data-limit test should create an engine")));
		if (Engine == nullptr)
		{
			return;
		}

		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("CONF-RECURSION-DATA-STACK-LIMIT"),
			TEXT("GlobalDataLimit"),
			UTF8_TO_TCHAR(RecursiveSource.c_str()));

		asIScriptModule* const Module = BuildNativeModule(Engine, "GlobalDataLimit", RecursiveSource.c_str());
		ASSERT_THAT(IsNotNull(Module, TEXT("Data-limit test should compile recursion")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->SetEngineProperty(asEP_INIT_STACK_SIZE, 256),
			TEXT("Data-limit test should set the initial data-stack size")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->SetEngineProperty(asEP_MAX_STACK_SIZE, 256),
			TEXT("Data-limit test should set the maximum data-stack size")));
		ASSERT_THAT(AreEqual(
			256,
			static_cast<int32>(Engine->GetEngineProperty(asEP_INIT_STACK_SIZE)),
			TEXT("Data-limit test should read back the initial data-stack size")));
		ASSERT_THAT(AreEqual(
			256,
			static_cast<int32>(Engine->GetEngineProperty(asEP_MAX_STACK_SIZE)),
			TEXT("Data-limit test should read back the maximum data-stack size")));

		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};
		ASSERT_THAT(IsNotNull(Context, TEXT("Data-limit test should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		asIScriptFunction* const RecursiveFunction =
			GetNativeFunctionByDecl(Module, "void recursive(const int)");
		ASSERT_THAT(IsNotNull(
			RecursiveFunction,
			TEXT("Data-limit test should resolve recursion by exact declaration")));
		if (RecursiveFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Prepare(RecursiveFunction),
			TEXT("Data-limit test should prepare recursion")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->SetArgDWord(0, 100),
			TEXT("Data-limit test should set the overflowing recursion depth")));
		ASSERT_THAT(AreEqual(
			asEXECUTION_EXCEPTION,
			Context->Execute(),
			TEXT("Data-limit test should report stack overflow as an exception")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Stack overflow")),
			FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
			TEXT("Data-limit test should preserve the exact stack-overflow diagnostic")));
		ASSERT_THAT(IsNotNull(
			Context->GetExceptionFunction(),
			TEXT("Data-limit test should identify the recursive exception function")));
		ASSERT_THAT(IsTrue(
			Context->GetCallstackSize() > 0,
			TEXT("Data-limit test should retain recursive callstack metadata")));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			TEXT("Data-limit test should unprepare after stack overflow")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Prepare(RecursiveFunction),
			TEXT("Data-limit test should prepare the same context for recovery")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->SetArgDWord(0, 0),
			TEXT("Data-limit test should set a non-recursive recovery depth")));
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED,
			Context->Execute(),
			TEXT("Data-limit context should execute normally after stack-overflow recovery")));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			TEXT("Data-limit test should unprepare the recovered context before release")));
		Context->Release();
		Context = nullptr;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->DiscardModule("GlobalDataLimit"),
			TEXT("Data-limit test should explicitly discard its recursive module")));
		ASSERT_THAT(IsNull(
			Engine->GetModule("GlobalDataLimit", asGM_ONLY_IF_EXISTS),
			TEXT("Data-limit module should be absent after explicit cleanup")));

		FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};
		asIScriptEngine* const ControlScriptEngine = ControlEngine.Get();
		ASSERT_THAT(IsNotNull(
			ControlScriptEngine,
			TEXT("Data-limit isolation should create an independent control engine")));
		if (ControlScriptEngine != nullptr)
		{
			ASSERT_THAT(AreEqual(
				4096,
				static_cast<int32>(ControlScriptEngine->GetEngineProperty(asEP_INIT_STACK_SIZE)),
				TEXT("Data-limit control engine should retain the fork's 4KB initial stack")));
			ASSERT_THAT(AreEqual(
				0,
				static_cast<int32>(ControlScriptEngine->GetEngineProperty(asEP_MAX_STACK_SIZE)),
				TEXT("Data-limit control engine should retain the fork's unlimited maximum stack")));
			ASSERT_THAT(IsNull(
				ControlScriptEngine->GetModule("GlobalDataLimit", asGM_ONLY_IF_EXISTS),
				TEXT("Data-limit module should remain absent from the control engine")));
		}
	}

	TEST_METHOD(GlobalSemanticsCallLimit)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("CONF-CALL-LIMIT-PROPERTIES-STORAGE-ONLY",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeMessageCollector Messages;
		asIScriptEngine* const Engine = CreateNativeEngine(&Messages);
		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(Engine);
		};
		ASSERT_THAT(IsNotNull(Engine, TEXT("Call-limit test should create an engine")));
		if (Engine == nullptr)
		{
			return;
		}

		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("CONF-CALL-LIMIT-PROPERTIES-STORAGE-ONLY"),
			TEXT("GlobalCallLimit"),
			UTF8_TO_TCHAR(RecursiveSource.c_str()));

		asIScriptModule* const Module = BuildNativeModule(Engine, "GlobalCallLimit", RecursiveSource.c_str());
		ASSERT_THAT(IsNotNull(Module, TEXT("Call-limit test should compile recursion")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->SetEngineProperty(asEP_INIT_CALL_STACK_SIZE, 1),
			TEXT("Call-limit test should store the initial call-stack size")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->SetEngineProperty(asEP_MAX_CALL_STACK_SIZE, 1),
			TEXT("Call-limit test should store the maximum call-stack size")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->SetEngineProperty(asEP_MAX_NESTED_CALLS, 1),
			TEXT("Call-limit test should store the maximum nested-call count")));
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(Engine->GetEngineProperty(asEP_INIT_CALL_STACK_SIZE)),
			TEXT("Call-limit test should read back the initial call-stack size")));
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(Engine->GetEngineProperty(asEP_MAX_CALL_STACK_SIZE)),
			TEXT("Call-limit test should read back the maximum call-stack size")));
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(Engine->GetEngineProperty(asEP_MAX_NESTED_CALLS)),
			TEXT("Call-limit test should read back the maximum nested-call count")));

		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};
		ASSERT_THAT(IsNotNull(Context, TEXT("Call-limit test should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		asIScriptFunction* const RecursiveFunction =
			GetNativeFunctionByDecl(Module, "void recursive(const int)");
		ASSERT_THAT(IsNotNull(
			RecursiveFunction,
			TEXT("Call-limit test should resolve recursion by exact declaration")));
		if (RecursiveFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Prepare(RecursiveFunction),
			TEXT("Call-limit test should prepare recursion")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->SetArgDWord(0, 4),
			TEXT("Call-limit test should set a depth exceeding every stored limit")));
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED,
			Context->Execute(),
			TEXT("Current fork should characterize call-limit properties as stored but unenforced")));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			TEXT("Call-limit test should unprepare its completed context before release")));
		Context->Release();
		Context = nullptr;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Engine->DiscardModule("GlobalCallLimit"),
			TEXT("Call-limit test should explicitly discard its recursive module")));
		ASSERT_THAT(IsNull(
			Engine->GetModule("GlobalCallLimit", asGM_ONLY_IF_EXISTS),
			TEXT("Call-limit module should be absent after explicit cleanup")));

		FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};
		asIScriptEngine* const ControlScriptEngine = ControlEngine.Get();
		ASSERT_THAT(IsNotNull(
			ControlScriptEngine,
			TEXT("Call-limit isolation should create an independent control engine")));
		if (ControlScriptEngine != nullptr)
		{
			ASSERT_THAT(AreEqual(
				10,
				static_cast<int32>(ControlScriptEngine->GetEngineProperty(asEP_INIT_CALL_STACK_SIZE)),
				TEXT("Call-limit control engine should retain the fork's initial call-stack size")));
			ASSERT_THAT(AreEqual(
				0,
				static_cast<int32>(ControlScriptEngine->GetEngineProperty(asEP_MAX_CALL_STACK_SIZE)),
				TEXT("Call-limit control engine should retain the fork's unlimited call-stack maximum")));
			ASSERT_THAT(AreEqual(
				100,
				static_cast<int32>(ControlScriptEngine->GetEngineProperty(asEP_MAX_NESTED_CALLS)),
				TEXT("Call-limit control engine should retain the fork's default nested-call limit")));
			ASSERT_THAT(IsNull(
				ControlScriptEngine->GetModule("GlobalCallLimit", asGM_ONLY_IF_EXISTS),
				TEXT("Call-limit module should remain absent from the control engine")));
		}
	}
};
#endif
