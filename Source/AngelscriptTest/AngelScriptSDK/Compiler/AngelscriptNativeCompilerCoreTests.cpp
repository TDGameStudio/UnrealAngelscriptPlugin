#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCompilerCoreTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Core",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompilerCoreSimpleFunction)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained exact single-function lookup smoke; LANG-DECL-FAMILY-SCOPE-ORDER owns declaration family, order, scope, runtime, and cleanup combinations.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler core simple-function test should create a standalone SDK engine")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Test()
			{
				return 42;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "CompilerCoreSimpleFunction", Source);
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "int Test()"),
			TEXT("Compiler core simple-function test should expose the compiled function by its exact declaration")));
	}

	TEST_METHOD(MultipleFunctions)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained sibling-function inventory smoke; LANG-DECL-FAMILY-SCOPE-ORDER owns staged declaration order/scope use and COMPILER-BUILDER-DECLARATION-PUBLICATION owns builder-table publication.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler core multiple-functions test should create a standalone SDK engine")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			void A()
			{
			}

			void B()
			{
			}

			int C()
			{
				return 42;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "CompilerCoreMultipleFunctions", Source);
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(3, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Compiler core multiple-functions test should expose every compiled function")));
		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "int C()"),
			TEXT("Compiler core multiple-functions test should retain exact declaration lookup after compiling siblings")));
	}

	TEST_METHOD(CompilerCoreGlobalVariables)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"AggregateSupport",
			"Retained two-global public runtime smoke; COMPILER-BUILDER-CONST-GLOBAL-STATE owns descriptor, address, folding, type, scope, bytecode, runtime, and cleanup depth.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler core global-variables test should create a standalone SDK engine")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			const int First = 40;
			const int Second = 2;

			int Read()
			{
				return First + Second;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "CompilerCoreGlobalVariables", Source);
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Compiler core global-variables test should preserve both global declarations")));
		asIScriptFunction* const ReadFunction = GetNativeFunctionByDecl(Module, "int Read()");
		ASSERT_THAT(IsNotNull(ReadFunction, TEXT("Compiler core global-variables test should expose the reader function")));
		if (ReadFunction == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Compiler core global-variables test should create an execution context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, PrepareAndExecute(Context, ReadFunction),
			TEXT("Compiler core global-variables test should execute the initialized constants")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Compiler core global-variables test should preserve constant values at runtime")));
	}

	TEST_METHOD(CompilerCoreBasic)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"AggregateSupport",
			"Retained end-to-end aggregate smoke; COMPILER-BYTECODE-SHAPE and focused declaration/global products own its individual compile, metadata, bytecode, runtime, and cleanup contracts.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler core basic test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "CompilerCoreBasic", ASTEST_AS_ANSI(R"AS(
			const int GlobalVar = 42;
			int Multiply(int A, int B)
			{
				return A * B;
			}

			bool Entry()
			{
				return GlobalVar == 42 && Multiply(6, 7) == 42;
			}
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool Entry()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Compiler core basic test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Compiler core basic test should compile and execute core constructs")));
		}
	}

	TEST_METHOD(CompilerCoreConfig)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"Infrastructure",
			"Retained setter/registration compatibility probe only; it does not prove copied-script-section lifetime and is not credited as Compiler product coverage.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler core configuration test should create a standalone engine")));

		ASSERT_THAT(IsTrue(ScriptEngine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, true) >= 0,
			TEXT("Compiler core configuration test should enable copied script sections")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, false);
		};
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType("TestConfigType", 0, asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			TEXT("Compiler core configuration test should register a reference type")));
	}

	TEST_METHOD(RecompileAfterError)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained same-name recovery smoke; COMPILER-BUILDER-REBUILD-RECOVERY owns failure families, same/fresh-engine routes, diagnostics, publication exclusion, runtime, and cleanup.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Compiler recompile test should create a standalone engine")));

		FScopedNativeModuleName ModuleScope(Engine, "CompilerRecompileAfterError");
		const std::string InvalidSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return NotDefined;
			}
			)AS");
		ASSERT_THAT(IsNull(BuildNativeModule(ScriptEngine, "CompilerRecompileAfterError", InvalidSource),
			TEXT("Compiler recompile test should reject the first invalid source")));
		Engine.ResetMessages();
		const std::string RecoverySource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 7;
			}
			)AS");
		asIScriptModule* const SuccessModule = BuildNativeModule(ScriptEngine, "CompilerRecompileAfterError", RecoverySource);
		ASSERT_THAT(IsNotNull(SuccessModule, TEXT("Compiler recompile test should compile corrected source under the same module name")));
		if (SuccessModule == nullptr)
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, SuccessModule, "int Entry()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Compiler recompile test should resolve corrected entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(AreEqual(7, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("Compiler recompile test should execute corrected module")));
		}
	}
};

#endif
