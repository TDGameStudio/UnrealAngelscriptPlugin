#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FLambda238Tests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.Lambda238",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
	TEST_METHOD(AnonymousFunctionCompilesInvokesAndReturnsValue)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"V238-DESIRED-BEHAVIOR supersedes this focused Disabled predecessor with lambda crossed across parse, compile, metadata, runtime, and cleanup evidence");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			funcdef int UnaryOperation(int);

			int Entry()
			{
				UnaryOperation@ AddTwo = function(int Value)
				{
					return Value + 2;
				};
				return AddTwo(40);
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "Lambda238", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Anonymous-function target should expose Entry")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Anonymous-function target should create a context")));
		if (Function == nullptr || Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Anonymous function should execute")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Anonymous function should return its computed value")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
