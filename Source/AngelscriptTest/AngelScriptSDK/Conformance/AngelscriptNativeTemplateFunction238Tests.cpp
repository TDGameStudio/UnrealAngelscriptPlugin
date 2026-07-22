#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FTemplateFunction238Tests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.TemplateFunction238",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
	TEST_METHOD(TemplateFunctionInstantiatesForTwoPrimitiveTypes)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			T Identity<T>(T Value)
			{
				return Value;
			}

			int Entry()
			{
				return Identity<int>(40) + int(Identity<float>(2.0f));
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "TemplateFunction238", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Template-function target should expose Entry")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Template-function target should create a context")));
		if (Function == nullptr || Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Template function should execute for both primitive instantiations")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Template function should preserve both primitive values")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
