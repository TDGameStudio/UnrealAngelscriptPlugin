#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FVariablesTests, "Angelscript.TestModule.AngelScriptSDK.Language.Variables", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InitializerExpression)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine; Engine.Create(*TestRunner); ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "VariablesInitializer", "const int computed = 10 * 3 + 7;");
		if (!Module.IsValid()) return;
		const int* const Value = static_cast<const int*>(Module->GetAddressOfGlobalVar(0));
		ASSERT_THAT(IsNotNull(Value, TEXT("Initializer expression should expose global storage")));
		if (Value != nullptr) ASSERT_THAT(AreEqual(37, *Value, TEXT("Initializer expression should evaluate at module build time")));
	}

	TEST_METHOD(VariablesConstReadAccess)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;
		FNativeTestEngine Engine; Engine.Create(*TestRunner); ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "VariablesConstRead", "const int limit = 200; int Entry() { return limit * 2; }");
		if (!Module.IsValid()) return;
		FSdkFunctionInvoker Invoker(*TestRunner, Engine.Get(), Module, "int Entry()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Const global test should resolve its entry")));
		if (Invoker.IsValid()) ASSERT_THAT(AreEqual(400, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("Const global should remain readable from script")));
	}

	TEST_METHOD(DeclarationString)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine; Engine.Create(*TestRunner); ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "VariablesDeclaration", "const double pi = 3.14159; const int answer = 42;");
		if (!Module.IsValid()) return;
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Declaration test should enumerate both globals")));
		for (asUINT Index = 0; Index < Module->GetGlobalVarCount(); ++Index) ASSERT_THAT(IsTrue(Module->GetGlobalVarDeclaration(Index) != nullptr && std::strlen(Module->GetGlobalVarDeclaration(Index)) > 0, TEXT("Declaration test should return non-empty declarations")));
	}
};
#endif
