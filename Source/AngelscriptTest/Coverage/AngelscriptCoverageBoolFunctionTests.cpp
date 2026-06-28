#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageBoolFunctionTest,
	"Angelscript.TestModule.Coverage.BoolFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool EnsureModuleBuilt(FAutomationTestBase& Test, asIScriptModule* Module, const TCHAR* Message)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsNotNull(Module, Message);
	}

	static bool EnsureInvokerValid(FAutomationTestBase& Test, FASGlobalFunctionInvoker& Invoker, const TCHAR* Message)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsTrue(Invoker.IsValid(), Message);
	}

	static bool EnsureInvokerValid(FAutomationTestBase& Test, FFunctionInvoker& Invoker, const TCHAR* Message)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsTrue(Invoker.IsValid(), Message);
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(FunctionParametersValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamValue", ASTEST_AS(R"AS(
		bool Negate(bool b)
		{
			return !b;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("bool value parameter module should compile")))
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool Negate(bool)"));
		if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("Negate(bool) should resolve and prepare")))
		{
			return;
		}
		Invoker.AddArg(true);
		const bool Result = Invoker.CallAndReturn<bool>(false);
		ASSERT_THAT(IsFalse(Result, TEXT("bool value parameter")));
	}

	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamIn", ASTEST_AS(R"AS(
		bool PassThrough(bool&in b)
		{
			return b;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("bool &in parameter module should compile")))
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool PassThrough(bool&in)"));
		if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("PassThrough(bool&in) should resolve and prepare")))
		{
			return;
		}
		bool Value = true;
		Invoker.AddArgRef(Value);
		const bool Result = Invoker.CallAndReturn<bool>(false);
		ASSERT_THAT(IsTrue(Result, TEXT("bool &in parameter")));
	}

	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamOut", ASTEST_AS(R"AS(
		void SetTrue(bool&out b)
		{
			b = true;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("bool &out parameter module should compile")))
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void SetTrue(bool&out)"));
		if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("SetTrue(bool&out) should resolve and prepare")))
		{
			return;
		}
		bool OutValue = false;
		Invoker.AddArgRef(OutValue);
		Invoker.Call();
		ASSERT_THAT(IsTrue(OutValue, TEXT("bool &out parameter")));
	}

	TEST_METHOD(FunctionParametersMultipleOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamMultipleOut", ASTEST_AS(R"AS(
		void SetPair(bool&out First, bool&out Second)
		{
			First = true;
			Second = false;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("bool multiple &out parameter module should compile")))
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void SetPair(bool&out, bool&out)"));
		if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("SetPair(bool&out, bool&out) should resolve and prepare")))
		{
			return;
		}
		bool First = false;
		bool Second = true;
		Invoker.AddArgRef(First);
		Invoker.AddArgRef(Second);
		Invoker.Call();
		ASSERT_THAT(IsTrue(First, TEXT("first bool &out parameter")));
		ASSERT_THAT(IsFalse(Second, TEXT("second bool &out parameter")));
	}

	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_ParamInOut", ASTEST_AS(R"AS(
		void Toggle(bool&inout b)
		{
			b = !b;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("bool &inout parameter module should compile")))
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void Toggle(bool&inout)"));
		if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("Toggle(bool&inout) should resolve and prepare")))
		{
			return;
		}
		bool Value = true;
		Invoker.AddArgRef(Value);
		Invoker.Call();
		ASSERT_THAT(IsFalse(Value, TEXT("bool &inout parameter")));
	}

	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_Return", ASTEST_AS(R"AS(
		bool ReturnTrue()
		{
			return true;
		}

		bool ReturnFalse()
		{
			return false;
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("bool return module should compile")))
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool ReturnTrue()"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("ReturnTrue should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("bool true return value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool ReturnFalse()"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("ReturnFalse should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(IsFalse(Invoker.CallAndReturn<bool>(true), TEXT("bool false return value")));
		}
	}

	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_DefaultParam", ASTEST_AS(R"AS(
		bool EchoDefaultTrue(bool b = true)
		{
			return b;
		}

		bool EchoDefaultFalse(bool b = false)
		{
			return b;
		}

		bool CallDefaultTrue()
		{
			return EchoDefaultTrue();
		}

		bool CallDefaultFalse()
		{
			return EchoDefaultFalse();
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("bool default parameter module should compile")))
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool CallDefaultTrue()"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("CallDefaultTrue should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("bool default true parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool CallDefaultFalse()"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("CallDefaultFalse should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(IsFalse(Invoker.CallAndReturn<bool>(true), TEXT("bool default false parameter")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("bool EchoDefaultTrue(bool)"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("EchoDefaultTrue(bool) should resolve and prepare")))
			{
				return;
			}
			Invoker.AddArg(false);
			ASSERT_THAT(IsFalse(Invoker.CallAndReturn<bool>(true), TEXT("explicit bool argument should override default true")));
		}
	}

	TEST_METHOD(FunctionOverloading)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovBoolFunc_Overload", ASTEST_AS(R"AS(
		int Pick(bool b)
		{
			return b ? 10 : 20;
		}

		int Pick(int Value)
		{
			return Value + 100;
		}

		int CallBoolOverload()
		{
			return Pick(true);
		}

		int CallIntOverload()
		{
			return Pick(5);
		}
		)AS"));
		ON_SCOPE_EXIT { if (Module) Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName())); };
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("bool overload module should compile")))
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int CallBoolOverload()"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("CallBoolOverload should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(AreEqual(10, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("bool overload should resolve bool signature")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int CallIntOverload()"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("CallIntOverload should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(AreEqual(105, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("int overload should resolve int signature")));
		}
	}

	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolFunctionActor : AActor
			{
				UPROPERTY()
				bool LastInput = false;

				UPROPERTY()
				bool LastOutput = false;

				UFUNCTION()
				bool EchoBool(bool b)
				{
					LastInput = b;
					return b;
				}

				UFUNCTION()
				bool ToggleBool(bool b)
				{
					LastInput = b;
					LastOutput = !b;
					return LastOutput;
				}
			}
			)AS"),
			TEXT("ACoverageBoolFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-function UFUNCTION actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Bool-function UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoBool"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("EchoBool should resolve and prepare")))
			{
				return;
			}
			Invoker.AddParam<bool>(true);
			const bool Result = Invoker.CallAndReturn<bool>(false);
			ASSERT_THAT(IsTrue(Result, TEXT("UFUNCTION bool parameter and true return")));
			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LastInput"), true, TEXT("UFUNCTION should receive true bool parameter"))));
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("ToggleBool"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("ToggleBool should resolve and prepare")))
			{
				return;
			}
			Invoker.AddParam<bool>(true);
			const bool Result = Invoker.CallAndReturn<bool>(true);
			ASSERT_THAT(IsFalse(Result, TEXT("UFUNCTION bool parameter and false return")));
			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LastOutput"), false, TEXT("UFUNCTION should return toggled bool value"))));
		}
	}
};

#endif
