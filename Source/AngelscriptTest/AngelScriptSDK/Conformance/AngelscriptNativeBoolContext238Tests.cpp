#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FBoolContext238Tests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.BoolContext238",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
	TEST_METHOD(BoolContextSelectsBranchAndLoopConditions)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"V238-DESIRED-BEHAVIOR supersedes this focused Disabled predecessor with bool_context crossed across parse, compile, metadata, runtime, and cleanup evidence");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class Flag
			{
				int Remaining = 2;
				opImplConv bool() const
				{
					return Remaining > 0;
				}
			}

			int Entry()
			{
				Flag State;
				int Count = 0;
				while (State)
				{
					++Count;
					--State.Remaining;
				}
				return Count;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "BoolContext238", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Bool-context target should expose Entry")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Bool-context target should create a context")));
		if (Function == nullptr || Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Bool context should execute branch and loop conditions")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord()), TEXT("Bool context should select the expected loop iterations")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
