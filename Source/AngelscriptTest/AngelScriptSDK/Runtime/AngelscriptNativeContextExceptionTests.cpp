#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FContextExceptionTests, "Angelscript.TestModule.AngelScriptSDK.Runtime.Context.Exceptions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExceptionLocation)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeMessageCollector Messages; asIScriptEngine* const Engine = CreateNativeEngine(&Messages); ON_SCOPE_EXIT { DestroyNativeEngine(Engine); };
		ASSERT_THAT(IsNotNull(Engine, TEXT("Exception-location test should create an engine")));
		Engine->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		asIScriptModule* const Module = BuildNativeModule(Engine, "ContextExceptionLocation", "void recursive(int n) { if (n > 0) recursive(n - 1); }");
		ASSERT_THAT(IsNotNull(Module, TEXT("Exception-location test should compile recursion"))); if (Module == nullptr) return;
		asIScriptContext* const Context = Engine->CreateContext(); ON_SCOPE_EXIT { if (Context != nullptr) Context->Release(); };
		ASSERT_THAT(IsNotNull(Context, TEXT("Exception-location test should create a context"))); if (Context == nullptr) return;
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(const int)")), TEXT("Exception-location test should prepare recursion")));
		Context->SetArgDWord(0, 100);
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute(), TEXT("Exception-location test should raise an exception")));
		ASSERT_THAT(AreEqual(FString(TEXT("Stack overflow")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())), TEXT("Exception-location test should report the overflow reason")));
		ASSERT_THAT(AreEqual(FString(TEXT("recursive")), FString(UTF8_TO_TCHAR(Context->GetExceptionFunction()->GetName())), TEXT("Exception-location test should identify the recursive overflow site")));
	}
};
#endif
