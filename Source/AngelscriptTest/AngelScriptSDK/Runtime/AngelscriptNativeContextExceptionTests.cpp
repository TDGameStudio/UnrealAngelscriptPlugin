#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FContextExceptionTests, "Angelscript.TestModule.AngelScriptSDK.Runtime.Context.Exceptions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExceptionLocation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-STACK-OVERFLOW-METADATA",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		FNativeMessageCollector Messages;
		asIScriptEngine* const Engine = CreateNativeEngine(&Messages);
		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(Engine);
		};
		ASSERT_THAT(IsNotNull(Engine, TEXT("Exception-location test should create an engine")));
		if (Engine == nullptr)
		{
			return;
		}
		Engine->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void recursive(int n)
			{
				if (n > 0)
				{
					recursive(n - 1);
				}
			}

			int Recover()
			{
				return 7;
			}
			)AS");
		for (const TCHAR* CaseId : {
			TEXT("RT-CTX-STACK-OVERFLOW-METADATA-REASON"),
			TEXT("RT-CTX-STACK-OVERFLOW-METADATA-FUNCTION"),
			TEXT("RT-CTX-STACK-OVERFLOW-METADATA-LINE") })
		{
			PrintGeneratedAsSource(
				*TestRunner,
				CaseId,
				TEXT("ContextExceptionLocation"),
				UTF8_TO_TCHAR(ScriptSource.c_str()));
		}
		asIScriptModule* const Module = BuildNativeModule(
			Engine,
			"ContextExceptionLocation",
			ScriptSource.c_str());
		ASSERT_THAT(IsNotNull(Module, TEXT("Exception-location test should compile recursion")));
		if (Module == nullptr)
		{
			return;
		}
		asIScriptFunction* const RecursiveFunction = GetNativeFunctionByExactDecl(Module, "void recursive(const int)");
		asIScriptFunction* const RecoveryFunction = GetNativeFunctionByExactDecl(Module, "int Recover()");
		ASSERT_THAT(IsNotNull(RecursiveFunction, TEXT("Exception-location test should resolve recursion exactly")));
		ASSERT_THAT(IsNotNull(RecoveryFunction, TEXT("Exception-location test should resolve recovery exactly")));
		if (RecursiveFunction == nullptr || RecoveryFunction == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = Engine->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("Exception-location test should create a context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};

			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(RecursiveFunction), TEXT("Exception-location test should prepare recursion")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 100), TEXT("Exception-location test should set the recursion depth")));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute(), TEXT("Exception-location test should raise an exception")));
			ASSERT_THAT(AreEqual(FString(TEXT("Stack overflow")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())), TEXT("Exception-location test should report the overflow reason")));
			ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(), TEXT("Exception-location test should retain an overflow function")));
			if (Context->GetExceptionFunction() != nullptr)
			{
				ASSERT_THAT(AreEqual(FString(TEXT("recursive")), FString(UTF8_TO_TCHAR(Context->GetExceptionFunction()->GetName())), TEXT("Exception-location test should identify the recursive overflow site")));
			}
			ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber() > 0,
				TEXT("Exception-location test should report a positive overflow line")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("Exception-location test should unprepare the overflow state before recovery")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, RecoveryFunction),
				TEXT("Exception-location test should execute recovery on the same context")));
			ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord()),
				TEXT("Exception-location test recovery should retain its independent return value")));
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
				TEXT("Exception-location test recovery should unprepare the reused context")));
		}

		{
			asIScriptContext* const ControlContext = Engine->CreateContext();
			ASSERT_THAT(IsNotNull(ControlContext,
				TEXT("Exception-location test should create an independent non-overflow control context")));
			if (ControlContext == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				ControlContext->Release();
			};

			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(ControlContext, RecoveryFunction),
				TEXT("Exception-location test independent non-overflow control should finish")));
			ASSERT_THAT(AreEqual(7, static_cast<int32>(ControlContext->GetReturnDWord()),
				TEXT("Exception-location test independent non-overflow control should retain its return value")));
			ASSERT_THAT(AreEqual(asSUCCESS, ControlContext->Unprepare(),
				TEXT("Exception-location test independent non-overflow control should unprepare cleanly")));
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Engine->DiscardModule("ContextExceptionLocation"),
			TEXT("Exception-location test should explicitly discard its module")));
		ASSERT_THAT(IsNull(Engine->GetModule("ContextExceptionLocation", asGM_ONLY_IF_EXISTS),
			TEXT("Exception-location module should be absent after explicit cleanup")));
	}
};
#endif
