#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleCompileFunctionTests,
	"Angelscript.TestModule.AngelScriptSDK.Module.ApiContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* Id,
		const TCHAR* ModuleName,
		const char* Source)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			Test,
			Id,
			ModuleName,
			FString(UTF8_TO_TCHAR(Source != nullptr ? Source : "")));
	}

	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* Id,
		const TCHAR* ModuleName,
		const std::string& Source)
	{
		PrintSource(Test, Id, ModuleName, Source.c_str());
	}

	static bool ExecuteFunction(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		asIScriptFunction& Function,
		int32 ExpectedValue)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assert(Test);
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (!Assert.IsNotNull(
			Context,
			TEXT("Module API function execution should create a context")))
		{
			return false;
		}

		const int32 ExecuteResult = PrepareAndExecute(Context, &Function);
		const int32 ActualValue = static_cast<int32>(Context->GetReturnDWord());
		const int ReleaseResult = Context->Release();
		return Assert.AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			TEXT("Module API function should finish execution"))
			&& Assert.AreEqual(
				ExpectedValue,
				ActualValue,
				TEXT("Module API function should return the exact expected value"))
			&& Assert.AreEqual(
				0,
				ReleaseResult,
				TEXT("Module API function execution should release its case-owned context"));
	}

public:
	TEST_METHOD(CompileFunctionDetachedAttachedAndInvalid)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-API-COMPILE-FUNCTION",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Module CompileFunction contract should have a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "ModuleApiCompileFunction");
		asIScriptModule* const Module =
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Module CompileFunction contract should create its owner module")));
		if (Module == nullptr)
		{
			return;
		}

		const std::string DetachedSource = ASTEST_AS_ANSI(R"AS(
			int DetachedValue()
			{
				return 31;
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-API-COMPILE-FUNCTION-DETACHED"),
			TEXT("ModuleApiCompileFunction"),
			DetachedSource);
		const asUINT InitialCount = Module->GetFunctionCount();
		asIScriptFunction* DetachedFunction = nullptr;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module->CompileFunction(
				"DetachedSection.as",
				DetachedSource.c_str(),
				4,
				0,
				&DetachedFunction),
			TEXT("Detached CompileFunction should succeed")));
		ASSERT_THAT(IsNotNull(
			DetachedFunction,
			TEXT("Detached CompileFunction should return its function")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(InitialCount),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Detached CompileFunction should not publish into the module inventory")));
		if (DetachedFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("DetachedSection.as")),
				FString(UTF8_TO_TCHAR(DetachedFunction->GetScriptSectionName())),
				TEXT("Detached function should preserve its section name")));
			ASSERT_THAT(IsTrue(ExecuteFunction(
				*TestRunner,
				*ScriptEngine,
				*DetachedFunction,
				31)));
			ASSERT_THAT(AreEqual(
				0,
				DetachedFunction->Release(),
				TEXT("Detached CompileFunction release should consume its final external reference")));
			DetachedFunction = nullptr;
		}

		const std::string AttachedSource = ASTEST_AS_ANSI(R"AS(
			int AttachedValue()
			{
				return 47;
			}
		)AS");
		PrintSource(
			*TestRunner,
			TEXT("MOD-API-COMPILE-FUNCTION-ATTACHED"),
			TEXT("ModuleApiCompileFunction"),
			AttachedSource);
		asIScriptFunction* AttachedFunction = nullptr;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module->CompileFunction(
				"AttachedSection.as",
				AttachedSource.c_str(),
				0,
				asCOMP_ADD_TO_MODULE,
				&AttachedFunction),
			TEXT("Attached CompileFunction should succeed")));
		ASSERT_THAT(IsNotNull(
			AttachedFunction,
			TEXT("Attached CompileFunction should return its function")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(InitialCount + 1),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Attached CompileFunction should publish exactly one function")));
		ASSERT_THAT(AreEqual(
			AttachedFunction,
			Module->GetFunctionByDecl("int AttachedValue()"),
			TEXT("Attached CompileFunction should support exact declaration lookup")));
		asIScriptFunction* const AttachedFunctionIdentity = AttachedFunction;
		if (AttachedFunction != nullptr)
		{
			ASSERT_THAT(IsTrue(ExecuteFunction(
				*TestRunner,
				*ScriptEngine,
				*AttachedFunction,
				47)));
			ASSERT_THAT(AreEqual(
				0,
				AttachedFunction->Release(),
				TEXT("Attached CompileFunction release should consume its caller-owned external reference")));
			AttachedFunction = nullptr;
		}

		PrintSource(
			*TestRunner,
			TEXT("MOD-API-COMPILE-FUNCTION-INVALID-FLAGS"),
			TEXT("ModuleApiCompileFunction"),
			AttachedSource);
		asIScriptFunction* InvalidFunction =
			reinterpret_cast<asIScriptFunction*>(static_cast<UPTRINT>(1));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_ARG),
			Module->CompileFunction(
				"InvalidFlags.as",
				AttachedSource.c_str(),
				0,
				0x80000000u,
				&InvalidFunction),
			TEXT("CompileFunction should reject unsupported flags")));
		ASSERT_THAT(IsNull(
			InvalidFunction,
			TEXT("Failed CompileFunction should clear the out-function pointer")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(InitialCount + 1),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Invalid flags should not mutate the original module function inventory")));
		ASSERT_THAT(AreEqual(
			AttachedFunctionIdentity,
			Module->GetFunctionByDecl("int AttachedValue()"),
			TEXT("Invalid flags should preserve the attached function identity")));

		PrintSource(
			*TestRunner,
			TEXT("MOD-API-COMPILE-FUNCTION-INVALID-SOURCE"),
			TEXT("ModuleApiCompileFunction"),
			nullptr);
		InvalidFunction =
			reinterpret_cast<asIScriptFunction*>(static_cast<UPTRINT>(1));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_ARG),
			Module->CompileFunction(
				"InvalidSource.as",
				nullptr,
				0,
				0,
				&InvalidFunction),
			TEXT("CompileFunction should reject a null source")));
		ASSERT_THAT(IsNull(
			InvalidFunction,
			TEXT("Null-source CompileFunction should clear the out-function pointer")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(InitialCount + 1),
			static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Null source should not mutate the original module function inventory")));
		ASSERT_THAT(AreEqual(
			AttachedFunctionIdentity,
			Module->GetFunctionByDecl("int AttachedValue()"),
			TEXT("Null source should preserve the attached function identity")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("CompileFunction cleanup should discard the attached-function module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("CompileFunction cleanup should remove the module lookup")));
		asIScriptModule* const CleanModule =
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			CleanModule,
			TEXT("CompileFunction cleanup should permit a clean same-name module")));
		if (CleanModule != nullptr)
		{
			ASSERT_THAT(AreEqual(
				0,
				static_cast<int32>(CleanModule->GetFunctionCount()),
				TEXT("Clean same-name module should not retain attached functions")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->DiscardModule(ModuleScope.Get()),
				TEXT("Clean CompileFunction module should discard after inventory inspection")));
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("CompileFunction cleanup should leave no module after the clean baseline")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
