#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

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

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"The exact compile-execute path is retained as a minimal predecessor while deeper Language and Runtime products own substantive compilation, invocation, return, and cleanup behavior");

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
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-SMOKE-MINIMAL-COMPILE-EXECUTE"),
			TEXT("NativeSmoke"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));

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

		AS_NATIVE_PRODUCT("ENG-EXACT-DECLARATION-OVERLOAD-REJECTION",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

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
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-EXACT-DECLARATION-OVERLOAD-REJECTION"),
			TEXT("ExactDeclaration"),
			UTF8_TO_TCHAR(OverloadSource.c_str()));

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
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module.Discard(),
			TEXT("Declaration lookup should explicitly discard its overload module")));
		ASSERT_THAT(IsNull(
			Engine.Get()->GetModule("ExactDeclaration", asGM_ONLY_IF_EXISTS),
			TEXT("Declaration lookup module should be absent after cleanup")));
	}
};

#endif
