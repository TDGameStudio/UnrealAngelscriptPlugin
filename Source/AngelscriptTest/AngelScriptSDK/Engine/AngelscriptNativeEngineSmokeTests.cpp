#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"

// Minimal raw-engine behavior and exact declaration lookup coverage.

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FEngineSmokeTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RawEngineCompilesAndExecutesMinimalFunction)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native smoke test should create a standalone AngelScript engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Test()
			{
				return 1;
			}
			)AS");

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "NativeSmoke", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test()");
		if (!this->Assert.IsNotNull(Function, TEXT("Native smoke test should resolve the compiled function by declaration")))
		{
			TestRunner->AddInfo(FString::Printf(TEXT("Native smoke module functions: %s"), *CollectFunctionDeclarations(Module)));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Native smoke test should create a native execution context")));
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native smoke test should finish execution successfully")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native smoke test should return the expected integer result")));
	}

	TEST_METHOD(DeclarationLookupRejectsDifferentOverload)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		const std::string OverloadSource = ASTEST_AS_ANSI(R"AS(
			int Select(int Value)
			{
				return Value;
			}

			int Select(float Value)
			{
				return int(Value) + 1;
			}
			)AS");

		FScopedNativeModule Module(*TestRunner, Engine, "ExactDeclaration", OverloadSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(IsNull(GetNativeFunctionByDecl(Module, "int Select(double)"),
			TEXT("Declaration lookup must not select another overload by function name")));

		const TArray<asIScriptFunction*> Matches = FindNativeFunctionsByName(Module, "Select");
		ASSERT_THAT(AreEqual(2, Matches.Num(),
			TEXT("Name lookup should explicitly expose both overloaded functions")));
	}
};

#endif
