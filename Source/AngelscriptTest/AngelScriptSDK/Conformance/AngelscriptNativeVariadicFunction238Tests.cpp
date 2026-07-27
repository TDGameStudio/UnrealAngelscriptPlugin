#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FVariadicFunction238Tests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.VariadicFunction238",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
	TEST_METHOD(VariadicFunctionAcceptsZeroAndMultipleTrailingArguments)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"V238-DESIRED-BEHAVIOR supersedes this focused Disabled predecessor with variadic_function crossed across parse, compile, metadata, runtime, and cleanup evidence");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int CountArguments(int First, ...)
			{
				return 1 + arguments.length();
			}

			int Entry()
			{
				return CountArguments(1) + CountArguments(1, 2, 3);
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "VariadicFunction238", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Variadic-function target should expose Entry")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Variadic-function target should create a context")));
		if (Function == nullptr || Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Variadic script function should execute")));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(Context->GetReturnDWord()), TEXT("Variadic function should count zero and multiple trailing arguments")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
