#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FMemberInitialization238Tests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.MemberInitialization238",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
	TEST_METHOD(MemberInitializationExpressionReportsMissingSymbol)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptModule* Module = nullptr;
		const int Result = CompileNativeModule(Engine.Get(), "MemberInitializationMissingSymbol", "enum SomeEnum { en_A } int GetVal(SomeEnum Value) { return 0; } class B { int SomeVal = GetVal(en_B); }", Module);
		ASSERT_THAT(IsTrue(Result < 0, TEXT("Member initialization with an unknown symbol should fail")));
		bool bFound = false;
		for (const FNativeMessageEntry& Entry : Engine.GetMessages().Entries) bFound |= Entry.Message.Contains(TEXT("'en_B' is not declared"));
		ASSERT_THAT(IsTrue(bFound, TEXT("Member initialization rejection should preserve missing-symbol diagnostic")));
	}
	TEST_METHOD(ConstructorMemberInitializerEvaluatesInDeclarationOrder)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class ValuePair
			{
				int First;
				int Second;

				ValuePair(int Value) : First(Value), Second(First + 1)
				{
				}
			}

			int Entry()
			{
				ValuePair Pair(40);
				return Pair.First + Pair.Second;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "MemberInitialization238", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Member-initialization target should expose Entry")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Member-initialization target should create a context")));
		if (Function == nullptr || Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Member initializer should execute")));
		ASSERT_THAT(AreEqual(81, static_cast<int32>(Context->GetReturnDWord()), TEXT("Member initialization should observe declaration order")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
