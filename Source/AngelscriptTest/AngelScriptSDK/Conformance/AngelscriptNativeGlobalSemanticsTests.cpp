#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace GlobalSemanticsTest { static constexpr const char* RecursiveSource = "void recursive(int n) { if (n > 0) recursive(n - 1); }"; }

TEST_CLASS_WITH_FLAGS(FGlobalSemanticsTests, "Angelscript.TestModule.AngelScriptSDK.Conformance.GlobalSemantics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GlobalSemanticsDataLimit)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeMessageCollector Messages; asIScriptEngine* const Engine = CreateNativeEngine(&Messages); ON_SCOPE_EXIT { DestroyNativeEngine(Engine); };
		ASSERT_THAT(IsNotNull(Engine, TEXT("Data-limit test should create an engine")));
		asIScriptModule* const Module = BuildNativeModule(Engine, "GlobalDataLimit", GlobalSemanticsTest::RecursiveSource);
		ASSERT_THAT(IsNotNull(Module, TEXT("Data-limit test should compile recursion"))); if (Module == nullptr) return;
		Engine->SetEngineProperty(asEP_INIT_STACK_SIZE, 256); Engine->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		asIScriptContext* const Context = Engine->CreateContext(); ON_SCOPE_EXIT { if (Context != nullptr) Context->Release(); };
		ASSERT_THAT(IsNotNull(Context, TEXT("Data-limit test should create a context"))); if (Context == nullptr) return;
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(const int)")), TEXT("Data-limit test should prepare recursion")));
		Context->SetArgDWord(0, 100);
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute(), TEXT("Data-limit test should report stack overflow as an exception")));
	}
	TEST_METHOD(GlobalSemanticsCallLimit)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeMessageCollector Messages; asIScriptEngine* const Engine = CreateNativeEngine(&Messages); ON_SCOPE_EXIT { DestroyNativeEngine(Engine); };
		ASSERT_THAT(IsNotNull(Engine, TEXT("Call-limit test should create an engine")));
		asIScriptModule* const Module = BuildNativeModule(Engine, "GlobalCallLimit", GlobalSemanticsTest::RecursiveSource);
		ASSERT_THAT(IsNotNull(Module, TEXT("Call-limit test should compile recursion"))); if (Module == nullptr) return;
		Engine->SetEngineProperty(asEP_INIT_CALL_STACK_SIZE, 1); Engine->SetEngineProperty(asEP_MAX_CALL_STACK_SIZE, 1); Engine->SetEngineProperty(asEP_MAX_NESTED_CALLS, 1);
		asIScriptContext* const Context = Engine->CreateContext(); ON_SCOPE_EXIT { if (Context != nullptr) Context->Release(); };
		ASSERT_THAT(IsNotNull(Context, TEXT("Call-limit test should create a context"))); if (Context == nullptr) return;
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(const int)")), TEXT("Call-limit test should prepare recursion")));
		Context->SetArgDWord(0, 1000);
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute(), TEXT("Call-limit test should report nested-call exhaustion as an exception")));
	}
};
#endif
