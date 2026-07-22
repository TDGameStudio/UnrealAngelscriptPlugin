#include "Support/AngelscriptNativeCoreTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CompilerDiagnosticTest
{
	static void VoidHelper()
	{
	}
}

TEST_CLASS_WITH_FLAGS(FCompilerDiagnosticTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompilerDiagnosticSyntaxError)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler diagnostic syntax-error test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "CompilerDiagnosticSyntaxError");
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, "CompilerDiagnosticSyntaxError", "int Broken( { return 1; }", Module);
		ASSERT_THAT(IsTrue(BuildResult < 0, TEXT("Compiler diagnostic syntax-error test should fail with a negative build result")));
		ASSERT_THAT(IsNotNull(Module, TEXT("Compiler diagnostic syntax-error test should preserve the module handle for diagnostics")));
		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0,
			TEXT("Compiler diagnostic syntax-error test should report at least one compiler diagnostic")));
	}

	TEST_METHOD(CompilerDiagnosticErrorMessage)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler diagnostic error-message test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "CompilerDiagnosticErrorMessage");
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, "CompilerDiagnosticErrorMessage", "int Broken( { return 1; }", Module);
		ASSERT_THAT(IsTrue(BuildResult < 0, TEXT("Compiler diagnostic error-message test should fail with a negative build result")));
		ASSERT_THAT(IsNotNull(Module, TEXT("Compiler diagnostic error-message test should preserve a module handle for diagnostics")));

		const FNativeMessageCollector& Messages = Engine.GetMessages();
		ASSERT_THAT(IsTrue(Messages.Entries.Num() > 0,
			TEXT("Compiler diagnostic error-message test should capture at least one diagnostic entry")));
		if (Messages.Entries.Num() == 0)
		{
			return;
		}

		const FNativeMessageEntry& FirstMessage = Messages.Entries[0];
		ASSERT_THAT(IsTrue(!FirstMessage.Message.IsEmpty(),
			TEXT("Compiler diagnostic error-message test should capture non-empty message text")));
		ASSERT_THAT(IsTrue(FirstMessage.Row > 0,
			TEXT("Compiler diagnostic error-message test should capture a valid source row")));
		ASSERT_THAT(IsTrue(!Engine.GetMessagesText().IsEmpty(),
			TEXT("Compiler diagnostic error-message test should render diagnostics for debugging")));
	}

	TEST_METHOD(CompilerDiagnosticError)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler diagnostic error test should create a standalone engine")));

		FScopedNativeModuleName ModuleScope(Engine, "CompilerDiagnosticMissingReturn");
		ASSERT_THAT(IsNull(BuildNativeModule(ScriptEngine, "CompilerDiagnosticMissingReturn", "int MissingReturn() { }"),
			TEXT("Compiler diagnostic error test should reject a missing return value")));
		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0, TEXT("Compiler diagnostic error test should capture diagnostics")));
	}

	TEST_METHOD(CompilerDiagnosticMultipleErrors)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler multiple-errors test should create a standalone engine")));

		FScopedNativeModuleName ModuleScope(Engine, "CompilerDiagnosticMultipleErrors");
		ASSERT_THAT(IsNull(BuildNativeModule(ScriptEngine, "CompilerDiagnosticMultipleErrors", "int Entry() { int x = UndefinedSymbol; bool y = 123; return x + y; }"),
			TEXT("Compiler multiple-errors test should reject independent errors")));
		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() >= 2, TEXT("Compiler multiple-errors test should emit multiple diagnostics")));
	}

	TEST_METHOD(CompilerDiagnosticTypeMismatch)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler type-mismatch test should create a standalone engine")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction("void DoNothing()", asFUNCTION(CompilerDiagnosticTest::VoidHelper), asCALL_CDECL) >= 0,
			TEXT("Compiler type-mismatch test should register the void helper")));

		FScopedNativeModuleName ModuleScope(Engine, "CompilerDiagnosticTypeMismatch");
		ASSERT_THAT(IsNull(BuildNativeModule(ScriptEngine, "CompilerDiagnosticTypeMismatch", "int Entry() { int x = DoNothing(); return x; }"),
			TEXT("Compiler type-mismatch test should reject assigning void to int")));
	}
};

#endif
