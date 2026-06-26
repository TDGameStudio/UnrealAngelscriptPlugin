#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageIntFunctionTests
// -----------------------------------------------------------------------------
// "Übershader-style" coverage for AngelScript integer-family *function usage*
// -- the function parameter / return value half of the int matrix. This file
// covers sub-matrix 6 from Documents/Coverage/Coverage_IntProperty.md:
//
//   * Function parameters (value / &in / &out / &inout)
//   * Return values
//   * Default parameters
//   * Multiple return values (&out)
//   * Function overloading by width
//   * UFUNCTION parameter/return
//
// Test patterns:
//   - Pattern B: Global functions via FASGlobalFunctionInvoker
//   - Pattern C: UFUNCTION via FFunctionInvoker (requires Actor context)
//
// int family under test:
//   int8 / int16 / int (int32) / int64 / uint8 / uint16 / uint (uint32) / uint64
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageIntFunctionTest,
	"Angelscript.TestModule.Coverage.IntFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// -------------------------------------------------------------------------
	// Function parameters: value passing across the int family.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_ParamValue", ASTEST_AS(R"AS(
		int8 AcceptInt8(int8 x)
		{
			return x + 1;
		}

		int16 AcceptInt16(int16 x)
		{
			return x + 100;
		}

		int AcceptInt(int x)
		{
			return x * 2;
		}

		int64 AcceptInt64(int64 x)
		{
			return x + 1000000;
		}

		uint8 AcceptUInt8(uint8 x)
		{
			return x + 1;
		}

		uint16 AcceptUInt16(uint16 x)
		{
			return x + 1000;
		}

		uint AcceptUInt(uint x)
		{
			return x + 100;
		}

		uint64 AcceptUInt64(uint64 x)
		{
			return x + 1000000000000;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Invoke with arguments using AddArg chaining
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int8 AcceptInt8(int8)"));
			Invoker.AddArg(static_cast<int8>(41));
			const int8 Result = Invoker.CallAndReturn<int8>(static_cast<int8>(0));
			TestRunner->TestEqual(TEXT("int8 value parameter"), Result, static_cast<int8>(42));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int16 AcceptInt16(int16)"));
			Invoker.AddArg(static_cast<int16>(29900));
			const int16 Result = Invoker.CallAndReturn<int16>(static_cast<int16>(0));
			TestRunner->TestEqual(TEXT("int16 value parameter"), Result, static_cast<int16>(30000));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int AcceptInt(int)"));
			Invoker.AddArg(static_cast<int32>(21));
			const int32 Result = Invoker.CallAndReturn<int32>(0);
			TestRunner->TestEqual(TEXT("int value parameter"), Result, 42);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int64 AcceptInt64(int64)"));
			Invoker.AddArg(static_cast<int64>(9000000000LL));
			const int64 Result = Invoker.CallAndReturn<int64>(static_cast<int64>(0));
			TestRunner->TestEqual(TEXT("int64 value parameter"), Result, static_cast<int64>(9001000000LL));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint8 AcceptUInt8(uint8)"));
			Invoker.AddArg(static_cast<uint8>(254));
			const uint8 Result = Invoker.CallAndReturn<uint8>(static_cast<uint8>(0));
			TestRunner->TestEqual(TEXT("uint8 value parameter"), Result, static_cast<uint8>(255));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint16 AcceptUInt16(uint16)"));
			Invoker.AddArg(static_cast<uint16>(59000));
			const uint16 Result = Invoker.CallAndReturn<uint16>(static_cast<uint16>(0));
			TestRunner->TestEqual(TEXT("uint16 value parameter"), Result, static_cast<uint16>(60000));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint AcceptUInt(uint)"));
			Invoker.AddArg(static_cast<uint32>(2999999900u));
			const uint32 Result = Invoker.CallAndReturn<uint32>(static_cast<uint32>(0));
			TestRunner->TestEqual(TEXT("uint value parameter"), Result, static_cast<uint32>(3000000000u));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint64 AcceptUInt64(uint64)"));
			Invoker.AddArg(static_cast<uint64>(12000000000000ull));
			const uint64 Result = Invoker.CallAndReturn<uint64>(static_cast<uint64>(0));
			TestRunner->TestEqual(TEXT("uint64 value parameter"), Result, static_cast<uint64>(13000000000000ull));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &in (const reference semantics).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersIn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_ParamIn", ASTEST_AS(R"AS(
		int AcceptIntIn(int&in x)
		{
			return x * 3;
		}

		int64 AcceptInt64In(int64&in x)
		{
			return x + 1;
		}

		uint AcceptUIntIn(uint&in x)
		{
			return x - 100;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int AcceptIntIn(int&in)"));
			Invoker.AddArg(static_cast<int32>(14));
			const int32 Result = Invoker.CallAndReturn<int32>(0);
			TestRunner->TestEqual(TEXT("int &in parameter"), Result, 42);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int64 AcceptInt64In(int64&in)"));
			Invoker.AddArg(static_cast<int64>(9999999999LL));
			const int64 Result = Invoker.CallAndReturn<int64>(static_cast<int64>(0));
			TestRunner->TestEqual(TEXT("int64 &in parameter"), Result, static_cast<int64>(10000000000LL));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint AcceptUIntIn(uint&in)"));
			Invoker.AddArg(static_cast<uint32>(3000000042u));
			const uint32 Result = Invoker.CallAndReturn<uint32>(static_cast<uint32>(0));
			TestRunner->TestEqual(TEXT("uint &in parameter"), Result, static_cast<uint32>(2999999942u));
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &out (output parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_ParamOut", ASTEST_AS(R"AS(
		void WriteInt(int&out x)
		{
			x = 42;
		}

		void WriteInt64(int64&out x)
		{
			x = 10000000000;
		}

		void WriteUInt(uint&out x)
		{
			x = 3000000000;
		}

		void MultipleOut(int&out a, int&out b)
		{
			a = 10;
			b = 20;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteInt(int&out)"));
			int32 OutValue = 0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("int &out parameter writes value"), OutValue, 42);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteInt64(int64&out)"));
			int64 OutValue = 0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("int64 &out parameter writes value"), OutValue, static_cast<int64>(10000000000LL));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteUInt(uint&out)"));
			uint32 OutValue = 0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("uint &out parameter writes value"), OutValue, static_cast<uint32>(3000000000u));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void MultipleOut(int&out, int&out)"));
			int32 OutA = 0;
			int32 OutB = 0;
			Invoker.AddArgRef(OutA).AddArgRef(OutB);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("multiple &out parameter A"), OutA, 10);
			TestRunner->TestEqual(TEXT("multiple &out parameter B"), OutB, 20);
		}
	}

	// -------------------------------------------------------------------------
	// Function parameters: &inout (read-write parameter).
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionParametersInOut)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_ParamInOut", ASTEST_AS(R"AS(
		void DoubleInt(int&inout x)
		{
			x *= 2;
		}

		void IncrementInt64(int64&inout x)
		{
			x += 1000;
		}

		void DecrementUInt(uint&inout x)
		{
			x -= 50;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void DoubleInt(int&inout)"));
			int32 Value = 21;
			Invoker.AddArgRef(Value);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("int &inout parameter modifies in place"), Value, 42);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void IncrementInt64(int64&inout)"));
			int64 Value = 9999999000LL;
			Invoker.AddArgRef(Value);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("int64 &inout parameter modifies in place"), Value, static_cast<int64>(10000000000LL));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void DecrementUInt(uint&inout)"));
			uint32 Value = 3000000050u;
			Invoker.AddArgRef(Value);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("uint &inout parameter modifies in place"), Value, static_cast<uint32>(3000000000u));
		}
	}

	// -------------------------------------------------------------------------
	// Function return values across the int family.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionReturnValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_Return", ASTEST_AS(R"AS(
		int8 ReturnInt8()
		{
			return -42;
		}

		int16 ReturnInt16()
		{
			return 30000;
		}

		int ReturnInt()
		{
			return 123456;
		}

		int64 ReturnInt64()
		{
			return 10000000000;
		}

		uint8 ReturnUInt8()
		{
			return 255;
		}

		uint16 ReturnUInt16()
		{
			return 60000;
		}

		uint ReturnUInt()
		{
			return 3000000000;
		}

		uint64 ReturnUInt64()
		{
			return 18000000000000000000;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		TestRunner->TestEqual(TEXT("int8 return value"),   FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int8 ReturnInt8()")).CallAndReturn<int8>(static_cast<int8>(0)),     static_cast<int8>(-42));
		TestRunner->TestEqual(TEXT("int16 return value"),  FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int16 ReturnInt16()")).CallAndReturn<int16>(static_cast<int16>(0)), static_cast<int16>(30000));
		TestRunner->TestEqual(TEXT("int return value"),    FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int ReturnInt()")).CallAndReturn<int32>(0),     123456);
		TestRunner->TestEqual(TEXT("int64 return value"),  FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int64 ReturnInt64()")).CallAndReturn<int64>(static_cast<int64>(0)), static_cast<int64>(10000000000LL));
		TestRunner->TestEqual(TEXT("uint8 return value"),  FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint8 ReturnUInt8()")).CallAndReturn<uint8>(static_cast<uint8>(0)), static_cast<uint8>(255));
		TestRunner->TestEqual(TEXT("uint16 return value"), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint16 ReturnUInt16()")).CallAndReturn<uint16>(static_cast<uint16>(0)), static_cast<uint16>(60000));
		TestRunner->TestEqual(TEXT("uint return value"),   FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint ReturnUInt()")).CallAndReturn<uint32>(static_cast<uint32>(0)),   static_cast<uint32>(3000000000u));
		TestRunner->TestEqual(TEXT("uint64 return value"), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint64 ReturnUInt64()")).CallAndReturn<uint64>(static_cast<uint64>(0)), static_cast<uint64>(18000000000000000000ull));
	}

	// -------------------------------------------------------------------------
	// Default parameters.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionDefaultParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_Default", ASTEST_AS(R"AS(
		int AddWithDefault(int a, int b = 10)
		{
			return a + b;
		}

		int64 MultiplyWithDefault(int64 x, int64 y = 2)
		{
			return x * y;
		}

		uint ChainDefaults(uint a = 5, uint b = 10, uint c = 15)
		{
			return a + b + c;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		// Call with all arguments
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int AddWithDefault(int, int)"));
			Invoker.AddArg(32).AddArg(10);
			const int32 Result = Invoker.CallAndReturn<int32>(0);
			TestRunner->TestEqual(TEXT("default parameter when explicitly provided"), Result, 42);
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int AddWithDefault(int)"));
			Invoker.AddArg(32);
			const int32 Result = Invoker.CallAndReturn<int32>(0);
			TestRunner->TestEqual(TEXT("default parameter used"), Result, 42);
		}

		// int64 with default
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int64 MultiplyWithDefault(int64)"));
			Invoker.AddArg(static_cast<int64>(5000000000LL));
			const int64 Result = Invoker.CallAndReturn<int64>(static_cast<int64>(0));
			TestRunner->TestEqual(TEXT("int64 default parameter"), Result, static_cast<int64>(10000000000LL));
		}

		// All defaults
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint ChainDefaults()"));
			const uint32 Result = Invoker.CallAndReturn<uint32>(static_cast<uint32>(0));
			TestRunner->TestEqual(TEXT("chain of default parameters"), Result, static_cast<uint32>(30));
		}
	}

	// -------------------------------------------------------------------------
	// Function overloading by width.
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionOverloading)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_Overload", ASTEST_AS(R"AS(
		int Process(int x)
		{
			return x + 100;
		}

		int64 Process(int64 x)
		{
			return x + 1000000;
		}

		uint Process(uint x)
		{
			return x + 200;
		}

		int CallProcessInt()
		{
			return Process(42);
		}

		int64 CallProcessInt64()
		{
			return Process(int64(9000000000));
		}

		uint CallProcessUInt()
		{
			return Process(uint(3000000000));
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		TestRunner->TestEqual(TEXT("overload resolves to int version"),   FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int CallProcessInt()")).CallAndReturn<int32>(0),     142);
		TestRunner->TestEqual(TEXT("overload resolves to int64 version"), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int64 CallProcessInt64()")).CallAndReturn<int64>(static_cast<int64>(0)), static_cast<int64>(9001000000LL));
		TestRunner->TestEqual(TEXT("overload resolves to uint version"),  FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("uint CallProcessUInt()")).CallAndReturn<uint32>(static_cast<uint32>(0)),  static_cast<uint32>(3000000200u));
	}

	// -------------------------------------------------------------------------
	// UFUNCTION parameter and return values (Pattern C: requires Actor context).
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionParametersAndReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntFunction_UFUNCTION"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntFunctionUFUNCTION.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntFunctionActor : AActor
			{
				UFUNCTION()
				int AddInts(int a, int b)
				{
					return a + b;
				}

				UFUNCTION()
				int64 MultiplyInt64(int64 x, int64 y)
				{
					return x * y;
				}

				UFUNCTION()
				uint SubtractUInt(uint a, uint b)
				{
					return a - b;
				}

				UFUNCTION()
				void WriteOut(int&out result)
				{
					result = 999;
				}
			}
			)AS"),
			TEXT("ACoverageIntFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int-function UFUNCTION actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-function UFUNCTION actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// UFUNCTION with int parameters and return
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AddInts"));
			Invoker.AddParam(20);
			Invoker.AddParam(22);
			const int32 Result = Invoker.CallAndReturn<int32>();
			TestRunner->TestEqual(TEXT("UFUNCTION int parameters and return"), Result, 42);
		}

		// UFUNCTION with int64
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MultiplyInt64"));
			Invoker.AddParam(static_cast<int64>(5000000000LL));
			Invoker.AddParam(static_cast<int64>(2));
			const int64 Result = Invoker.CallAndReturn<int64>();
			TestRunner->TestEqual(TEXT("UFUNCTION int64 parameters and return"), Result, static_cast<int64>(10000000000LL));
		}

		// UFUNCTION with uint
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("SubtractUInt"));
			Invoker.AddParam(static_cast<uint32>(3000000042u));
			Invoker.AddParam(static_cast<uint32>(42));
			const uint32 Result = Invoker.CallAndReturn<uint32>();
			TestRunner->TestEqual(TEXT("UFUNCTION uint parameters and return"), Result, static_cast<uint32>(3000000000u));
		}

		// UFUNCTION with &out parameter
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("WriteOut"));
			int32 OutValue = 0;
			Invoker.AddParam(0);
			Invoker.Call();
			Invoker.ReadParamAfterCall(0, OutValue);
			TestRunner->TestEqual(TEXT("UFUNCTION int &out parameter"), OutValue, 999);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
