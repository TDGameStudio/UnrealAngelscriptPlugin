#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FDefaultSpecialMembers238Tests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.DefaultSpecialMembers238",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
	TEST_METHOD(GeneratedDefaultAndCopyMembersPreserveValues)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class Sample
			{
				int Value = 42;
				Sample() = default;
				Sample(const Sample& in Other) = default;
			}

			int Entry()
			{
				Sample Original;
				Sample Copy(Original);
				return Copy.Value;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "DefaultSpecialMembers238", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Default special-member target should expose Entry")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Default special-member target should create a context")));
		if (Function == nullptr || Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Generated special members should execute")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Generated copy member should preserve values")));
	}

	TEST_METHOD(DeletedDefaultOrCopyMemberRejectsTheMatchingUse)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class NonDefaultConstructible
			{
				NonDefaultConstructible() = delete;
			}

			void Entry()
			{
				NonDefaultConstructible Value;
			}
			)AS");
		const int Result = CompileSnippet("DeletedSpecialMember238", ScriptSource.c_str(), Messages);
		ASSERT_THAT(IsTrue(Result < 0, TEXT("Deleted default constructor should reject a matching construction")));
		ASSERT_THAT(IsTrue(Messages.Entries.Num() > 0, TEXT("Deleted default constructor should report a diagnostic")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
