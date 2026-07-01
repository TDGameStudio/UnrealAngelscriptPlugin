#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Math/NumericLimits.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageIntFunctionTests
// -----------------------------------------------------------------------------
// "Übershader-style" coverage for AngelScript integer-family *function usage*
// -- the function parameter / return value half of the int matrix. This file
// covers sub-matrix 6 from OpenSpec: test-coverage/coverage-matrix.md:
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

#if WITH_ANGELSCRIPT_UNITTESTS

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
		int8 AcceptInt8In(int8&in x)
		{
			return x + 10;
		}

		int16 AcceptInt16In(int16&in x)
		{
			return x + 100;
		}

		int AcceptIntIn(int&in x)
		{
			return x * 3;
		}

		int64 AcceptInt64In(int64&in x)
		{
			return x + 1;
		}

		uint8 AcceptUInt8In(uint8&in x)
		{
			return x + 5;
		}

		uint16 AcceptUInt16In(uint16&in x)
		{
			return x + 50;
		}

		uint AcceptUIntIn(uint&in x)
		{
			return x - 100;
		}

		uint64 AcceptUInt64In(uint64&in x)
		{
			return x + 1000;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("Int &in parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int8 AcceptInt8In(int8&in)"));
			int8 Value = 5;
			Invoker.AddArgRef(Value);
			const int8 Result = Invoker.CallAndReturn<int8>(0);
			TestRunner->TestEqual(TEXT("int8 &in parameter"), Result, static_cast<int8>(15));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int16 AcceptInt16In(int16&in)"));
			int16 Value = 200;
			Invoker.AddArgRef(Value);
			const int16 Result = Invoker.CallAndReturn<int16>(0);
			TestRunner->TestEqual(TEXT("int16 &in parameter"), Result, static_cast<int16>(300));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int AcceptIntIn(int&in)"));
			int32 Value = 14;
			Invoker.AddArgRef(Value);
			const int32 Result = Invoker.CallAndReturn<int32>(0);
			TestRunner->TestEqual(TEXT("int &in parameter"), Result, 42);
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int64 AcceptInt64In(int64&in)"));
			int64 Value = 9999999999LL;
			Invoker.AddArgRef(Value);
			const int64 Result = Invoker.CallAndReturn<int64>(static_cast<int64>(0));
			TestRunner->TestEqual(TEXT("int64 &in parameter"), Result, static_cast<int64>(10000000000LL));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint8 AcceptUInt8In(uint8&in)"));
			uint8 Value = 10;
			Invoker.AddArgRef(Value);
			const uint8 Result = Invoker.CallAndReturn<uint8>(0);
			TestRunner->TestEqual(TEXT("uint8 &in parameter"), Result, static_cast<uint8>(15));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint16 AcceptUInt16In(uint16&in)"));
			uint16 Value = 1000;
			Invoker.AddArgRef(Value);
			const uint16 Result = Invoker.CallAndReturn<uint16>(0);
			TestRunner->TestEqual(TEXT("uint16 &in parameter"), Result, static_cast<uint16>(1050));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint AcceptUIntIn(uint&in)"));
			uint32 Value = 3000000042u;
			Invoker.AddArgRef(Value);
			const uint32 Result = Invoker.CallAndReturn<uint32>(static_cast<uint32>(0));
			TestRunner->TestEqual(TEXT("uint &in parameter"), Result, static_cast<uint32>(2999999942u));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint64 AcceptUInt64In(uint64&in)"));
			uint64 Value = 99999ull;
			Invoker.AddArgRef(Value);
			const uint64 Result = Invoker.CallAndReturn<uint64>(static_cast<uint64>(0));
			TestRunner->TestEqual(TEXT("uint64 &in parameter"), Result, static_cast<uint64>(100999ull));
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
		void WriteInt8(int8&out x)
		{
			x = 127;
		}

		void WriteInt16(int16&out x)
		{
			x = 30000;
		}

		void WriteInt(int&out x)
		{
			x = 42;
		}

		void WriteInt64(int64&out x)
		{
			x = 10000000000;
		}

		void WriteUInt8(uint8&out x)
		{
			x = 255;
		}

		void WriteUInt16(uint16&out x)
		{
			x = 60000;
		}

		void WriteUInt(uint&out x)
		{
			x = 3000000000;
		}

		void WriteUInt64(uint64&out x)
		{
			x = 18000000000000000000;
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteInt8(int8&out)"));
			int8 OutValue = 0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("int8 &out parameter writes value"), OutValue, static_cast<int8>(127));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteInt16(int16&out)"));
			int16 OutValue = 0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("int16 &out parameter writes value"), OutValue, static_cast<int16>(30000));
		}
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
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteUInt8(uint8&out)"));
			uint8 OutValue = 0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("uint8 &out parameter writes value"), OutValue, static_cast<uint8>(255));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteUInt16(uint16&out)"));
			uint16 OutValue = 0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("uint16 &out parameter writes value"), OutValue, static_cast<uint16>(60000));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void WriteUInt64(uint64&out)"));
			uint64 OutValue = 0;
			Invoker.AddArgRef(OutValue);
			Invoker.Call();
			TestRunner->TestEqual(TEXT("uint64 &out parameter writes value"), OutValue, static_cast<uint64>(18000000000000000000ull));
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
		void DoubleInt8(int8&inout x)
		{
			x *= 2;
		}

		void DoubleInt16(int16&inout x)
		{
			x *= 2;
		}

		void DoubleInt(int&inout x)
		{
			x *= 2;
		}

		void IncrementInt64(int64&inout x)
		{
			x += 1000;
		}

		void DoubleUInt8(uint8&inout x)
		{
			x *= 2;
		}

		void DoubleUInt16(uint16&inout x)
		{
			x *= 2;
		}

		void DecrementUInt(uint&inout x)
		{
			x -= 50;
		}

		void IncrementUInt64(uint64&inout x)
		{
			x += 1000;
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

		int AddUsingDefault(int a)
		{
			return AddWithDefault(a);
		}

		int64 MultiplyWithDefault(int64 x, int64 y = 2)
		{
			return x * y;
		}

		int64 MultiplyUsingDefault(int64 x)
		{
			return MultiplyWithDefault(x);
		}

		uint ChainDefaults(uint a = 5, uint b = 10, uint c = 15)
		{
			return a + b + c;
		}

		uint ChainUsingDefaults()
		{
			return ChainDefaults();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("Int default-parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		// Call with all arguments
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int AddWithDefault(int, int)"));
			Invoker.AddArg(32).AddArg(10);
			const int32 Result = Invoker.CallAndReturn<int32>(0);
			TestRunner->TestEqual(TEXT("default parameter when explicitly provided"), Result, 42);
		}

		// Call with default (omit second argument)
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int AddUsingDefault(int)"));
			Invoker.AddArg(32);
			const int32 Result = Invoker.CallAndReturn<int32>(0);
			TestRunner->TestEqual(TEXT("default parameter used through script call"), Result, 42);
		}

		// int64 with default
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int64 MultiplyUsingDefault(int64)"));
			Invoker.AddArg(static_cast<int64>(5000000000LL));
			const int64 Result = Invoker.CallAndReturn<int64>(static_cast<int64>(0));
			TestRunner->TestEqual(TEXT("int64 default parameter through script call"), Result, static_cast<int64>(10000000000LL));
		}

		// All defaults
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint ChainUsingDefaults()"));
			const uint32 Result = Invoker.CallAndReturn<uint32>(static_cast<uint32>(0));
			TestRunner->TestEqual(TEXT("chain of default parameters through script call"), Result, static_cast<uint32>(30));
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

	TEST_METHOD(FunctionReferenceParameterCombinations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_ReferenceCombos", ASTEST_AS(R"AS(
		void DefaultAndOut(int&out Result, int Value = 10)
		{
			Result = Value * 2;
		}

		void DefaultAndOutUsingDefault(int&out Result)
		{
			DefaultAndOut(Result);
		}

		void MultipleOutOrder(int Seed, int&out A, int&out B, int&out C)
		{
			A = Seed + 1;
			B = Seed + 2;
			C = Seed + 3;
		}

		void PreserveInOut(int&inout Value)
		{
			int Original = Value;
			Value = Original * 2 + 1;
		}

		int ConstInValue(const int&in Value)
		{
			return Value + 1;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int reference-combination module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void DefaultAndOutUsingDefault(int&out)"));
			int32 Result = 0;
			Invoker.AddArgRef(Result);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("default argument should combine with an int &out parameter")));
			ASSERT_THAT(AreEqual(20, Result, TEXT("default argument should be applied when trailing int is omitted")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void DefaultAndOut(int&out, int)"));
			int32 Result = 0;
			Invoker.AddArgRef(Result).AddArg(7);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("explicit argument should combine with an int &out parameter")));
			ASSERT_THAT(AreEqual(14, Result, TEXT("explicit argument should override default before writing out")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void MultipleOutOrder(int, int&out, int&out, int&out)"));
			int32 A = 0;
			int32 B = 0;
			int32 C = 0;
			Invoker.AddArg(40).AddArgRef(A).AddArgRef(B).AddArgRef(C);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("multiple int &out parameters should execute")));
			ASSERT_THAT(AreEqual(41, A, TEXT("first &out parameter should receive first assigned value")));
			ASSERT_THAT(AreEqual(42, B, TEXT("second &out parameter should receive second assigned value")));
			ASSERT_THAT(AreEqual(43, C, TEXT("third &out parameter should receive third assigned value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("void PreserveInOut(int&inout)"));
			int32 Value = 20;
			Invoker.AddArgRef(Value);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("int &inout parameter should execute")));
			ASSERT_THAT(AreEqual(41, Value, TEXT("int &inout parameter should see and update the initial caller value")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ConstInValue(const int&in)"));
			const int32 Value = 41;
			Invoker.AddArgRef(Value);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("const int &in parameter should read caller value")));
		}
	}

	TEST_METHOD(FunctionReturnControlFlow)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_ReturnFlow", ASTEST_AS(R"AS(
		int Add(int A, int B)
		{
			return A + B;
		}

		int ReturnExpression()
		{
			int A = 20;
			int B = 22;
			return A + B;
		}

		int ReturnFunctionCall()
		{
			return Add(20, 22);
		}

		int ConditionalReturn(int Value)
		{
			return Value > 0 ? Value : -Value;
		}

		int EarlyReturn(int Value)
		{
			if (Value < 0)
			{
				return -1;
			}

			return Value + 1;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int return-control-flow module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int ReturnExpression()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("function should return an integer expression")));
		ASSERT_THAT(AreEqual(42, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int ReturnFunctionCall()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("function should return another function call result")));
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ConditionalReturn(int)"));
			Invoker.AddArg(-42);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("conditional return should select absolute value branch")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int EarlyReturn(int)"));
			Invoker.AddArg(-5);
			ASSERT_THAT(AreEqual(-1, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("early return should exit before final return")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int EarlyReturn(int)"));
			Invoker.AddArg(41);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("fallthrough return should execute when early condition is false")));
		}
	}

	TEST_METHOD(FunctionDefaultParameterEdges)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_DefaultEdges", ASTEST_AS(R"AS(
		int MultipleDefaults(int A, int B = 10, int C = 20)
		{
			return A + B + C;
		}

		int MultipleDefaultsUsingBoth(int A)
		{
			return MultipleDefaults(A);
		}

		int MultipleDefaultsUsingFinal(int A, int B)
		{
			return MultipleDefaults(A, B);
		}

		int NegativeDefault(int Value = -7)
		{
			return Value;
		}

		int NegativeDefaultUsingDefault()
		{
			return NegativeDefault();
		}

		int BoundaryDefault(int Value = 2147483647)
		{
			return Value;
		}

		int BoundaryDefaultUsingDefault()
		{
			return BoundaryDefault();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int default-parameter edge module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int MultipleDefaultsUsingBoth(int)"));
			Invoker.AddArg(12);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("multiple defaults should fill both omitted trailing parameters")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int MultipleDefaultsUsingFinal(int, int)"));
			Invoker.AddArg(12).AddArg(10);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("partial omission should fill the final default parameter")));
		}
		ASSERT_THAT(AreEqual(-7, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int NegativeDefaultUsingDefault()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("negative default int parameter should be applied")));
		ASSERT_THAT(AreEqual(TNumericLimits<int32>::Max(), FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int BoundaryDefaultUsingDefault()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("boundary default int parameter should be applied")));
	}

	TEST_METHOD(FunctionOverloadArityAndNumericResolution)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovIntFunc_OverloadEdges", ASTEST_AS(R"AS(
		int Choose(int A)
		{
			return A + 1;
		}

		int Choose(int A, int B)
		{
			return A + B + 2;
		}

		int Choose(int A, int B, int C)
		{
			return A + B + C + 3;
		}

		int Numeric(int Value)
		{
			return Value + 10;
		}

		int Numeric(double Value)
		{
			return int(Value) + 20;
		}

		int CallChooseOne()
		{
			return Choose(41);
		}

		int CallChooseTwo()
		{
			return Choose(10, 30);
		}

		int CallChooseThree()
		{
			return Choose(10, 20, 9);
		}

		int CallNumericInt()
		{
			return Numeric(32);
		}

		int CallNumericDouble()
		{
			return Numeric(22.5);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("Int overload-edge module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int CallChooseOne()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("single-parameter overload should resolve by arity")));
		ASSERT_THAT(AreEqual(42, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int CallChooseTwo()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("two-parameter overload should resolve by arity")));
		ASSERT_THAT(AreEqual(42, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int CallChooseThree()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("three-parameter overload should resolve by arity")));
		ASSERT_THAT(AreEqual(42, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int CallNumericInt()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("integer literal should resolve to int overload")));
		ASSERT_THAT(AreEqual(42, FASGlobalFunctionInvoker(*TestRunner, Engine, *Module, TEXT("int CallNumericDouble()")).CallAndReturn<int32>(INDEX_NONE),
			TEXT("floating literal should resolve to double overload")));
	}

	TEST_METHOD(UFunctionSpecifierDefaultsAndOutParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntFunction_UFunctionEdges"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntFunctionUFunctionEdges.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntFunctionEdgesActor : AActor
			{
				UFUNCTION(BlueprintCallable, Category = "Coverage|Int")
				int CallableAdd(int A, int B)
				{
					return A + B;
				}

				UFUNCTION(BlueprintPure, Category = "Coverage|Int")
				int PureDouble(int Value) const
				{
					return Value * 2;
				}

				UFUNCTION()
				int DefaultInt(int Value = 10)
				{
					return Value * 4 + 2;
				}

				UFUNCTION()
				void SplitOut(int Input, int&out A, int&out B)
				{
					A = Input + 1;
					B = Input + 2;
				}
			}
			)AS"),
			TEXT("ACoverageIntFunctionEdgesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int UFUNCTION edge actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* CallableAdd = FindGeneratedFunction(ScriptClass, TEXT("CallableAdd"));
		ASSERT_THAT(IsNotNull(CallableAdd, TEXT("CallableAdd should be generated")));
		if (CallableAdd != nullptr)
		{
			ASSERT_THAT(IsTrue(CallableAdd->HasAnyFunctionFlags(FUNC_BlueprintCallable), TEXT("BlueprintCallable int UFUNCTION should set FUNC_BlueprintCallable")));
			ASSERT_THAT(IsFalse(CallableAdd->HasAnyFunctionFlags(FUNC_BlueprintPure), TEXT("BlueprintCallable int UFUNCTION should not set FUNC_BlueprintPure")));
		}

		UFunction* PureDouble = FindGeneratedFunction(ScriptClass, TEXT("PureDouble"));
		ASSERT_THAT(IsNotNull(PureDouble, TEXT("PureDouble should be generated")));
		if (PureDouble != nullptr)
		{
			ASSERT_THAT(IsTrue(PureDouble->HasAnyFunctionFlags(FUNC_BlueprintCallable), TEXT("BlueprintPure int UFUNCTION should also be BlueprintCallable")));
			ASSERT_THAT(IsTrue(PureDouble->HasAnyFunctionFlags(FUNC_BlueprintPure), TEXT("BlueprintPure int UFUNCTION should set FUNC_BlueprintPure")));
			ASSERT_THAT(IsTrue(PureDouble->HasAnyFunctionFlags(FUNC_Const), TEXT("const BlueprintPure int UFUNCTION should set FUNC_Const")));
		}

		UFunction* DefaultInt = FindGeneratedFunction(ScriptClass, TEXT("DefaultInt"));
		ASSERT_THAT(IsNotNull(DefaultInt, TEXT("DefaultInt should be generated")));
		if (DefaultInt != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(DefaultInt, TEXT("Value")), TEXT("DefaultInt should expose its int parameter")));
		}

		UFunction* SplitOut = FindGeneratedFunction(ScriptClass, TEXT("SplitOut"));
		ASSERT_THAT(IsNotNull(SplitOut, TEXT("SplitOut should be generated")));
		if (SplitOut != nullptr)
		{
			FProperty* AParam = FindFProperty<FIntProperty>(SplitOut, TEXT("A"));
			FProperty* BParam = FindFProperty<FIntProperty>(SplitOut, TEXT("B"));
			ASSERT_THAT(IsNotNull(AParam, TEXT("SplitOut should expose first int out parameter")));
			ASSERT_THAT(IsNotNull(BParam, TEXT("SplitOut should expose second int out parameter")));
			if (AParam != nullptr)
			{
				ASSERT_THAT(IsTrue(AParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("SplitOut first int parameter should be marked CPF_OutParm")));
			}
			if (BParam != nullptr)
			{
				ASSERT_THAT(IsTrue(BParam->HasAnyPropertyFlags(CPF_OutParm), TEXT("SplitOut second int parameter should be marked CPF_OutParm")));
			}
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int UFUNCTION edge actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("CallableAdd"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("CallableAdd should be invokable")));
			Invoker.AddParam<int32>(20).AddParam<int32>(22);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("BlueprintCallable int UFUNCTION should execute through FFunctionInvoker")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("PureDouble"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("PureDouble should be invokable")));
			Invoker.AddParam<int32>(21);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("BlueprintPure int UFUNCTION should execute through FFunctionInvoker")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("DefaultInt"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("DefaultInt should be invokable with explicit reflected parameter")));
			Invoker.AddParam<int32>(10);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("UFUNCTION int default parameter signature should execute when populated explicitly")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("SplitOut"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("SplitOut should be invokable")));
			Invoker.AddParam<int32>(40).AddParam<int32>(0).AddParam<int32>(0);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("UFUNCTION int &out combination should execute through FFunctionInvoker")));

			int32 A = 0;
			int32 B = 0;
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(1, A), TEXT("SplitOut should expose first out value after call")));
			ASSERT_THAT(IsTrue(Invoker.ReadParamAfterCall(2, B), TEXT("SplitOut should expose second out value after call")));
			ASSERT_THAT(AreEqual(41, A, TEXT("first UFUNCTION int &out parameter should preserve order")));
			ASSERT_THAT(AreEqual(42, B, TEXT("second UFUNCTION int &out parameter should preserve order")));
		}
	}

	TEST_METHOD(UFunctionAllIntegerWidthsReflectAndInvoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntFunction_UFunctionWidths"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntFunctionUFunctionWidths.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntFunctionWidthsActor : AActor
			{
				UFUNCTION()
				int8 EchoInt8(int8 Value)
				{
					return Value + 1;
				}

				UFUNCTION()
				int16 EchoInt16(int16 Value)
				{
					return Value + 2;
				}

				UFUNCTION()
				int EchoInt(int Value)
				{
					return Value + 3;
				}

				UFUNCTION()
				int64 EchoInt64(int64 Value)
				{
					return Value + 4;
				}

				UFUNCTION()
				uint8 EchoUInt8(uint8 Value)
				{
					return Value + 5;
				}

				UFUNCTION()
				uint16 EchoUInt16(uint16 Value)
				{
					return Value + 6;
				}

				UFUNCTION()
				uint EchoUInt(uint Value)
				{
					return Value + 7;
				}

				UFUNCTION()
				uint64 EchoUInt64(uint64 Value)
				{
					return Value + 8;
				}
			}
			)AS"),
			TEXT("ACoverageIntFunctionWidthsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int UFUNCTION width actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* EchoInt8 = FindGeneratedFunction(ScriptClass, TEXT("EchoInt8"));
		ASSERT_THAT(IsNotNull(EchoInt8, TEXT("EchoInt8 should be generated")));
		if (EchoInt8 != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FInt8Property>(EchoInt8, TEXT("Value")), TEXT("EchoInt8 parameter should reflect as FInt8Property")));
			ASSERT_THAT(IsNotNull(CastField<FInt8Property>(EchoInt8->GetReturnProperty()), TEXT("EchoInt8 return should reflect as FInt8Property")));
		}

		UFunction* EchoInt16 = FindGeneratedFunction(ScriptClass, TEXT("EchoInt16"));
		ASSERT_THAT(IsNotNull(EchoInt16, TEXT("EchoInt16 should be generated")));
		if (EchoInt16 != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FInt16Property>(EchoInt16, TEXT("Value")), TEXT("EchoInt16 parameter should reflect as FInt16Property")));
			ASSERT_THAT(IsNotNull(CastField<FInt16Property>(EchoInt16->GetReturnProperty()), TEXT("EchoInt16 return should reflect as FInt16Property")));
		}

		UFunction* EchoInt = FindGeneratedFunction(ScriptClass, TEXT("EchoInt"));
		ASSERT_THAT(IsNotNull(EchoInt, TEXT("EchoInt should be generated")));
		if (EchoInt != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(EchoInt, TEXT("Value")), TEXT("EchoInt parameter should reflect as FIntProperty")));
			ASSERT_THAT(IsNotNull(CastField<FIntProperty>(EchoInt->GetReturnProperty()), TEXT("EchoInt return should reflect as FIntProperty")));
		}

		UFunction* EchoInt64 = FindGeneratedFunction(ScriptClass, TEXT("EchoInt64"));
		ASSERT_THAT(IsNotNull(EchoInt64, TEXT("EchoInt64 should be generated")));
		if (EchoInt64 != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FInt64Property>(EchoInt64, TEXT("Value")), TEXT("EchoInt64 parameter should reflect as FInt64Property")));
			ASSERT_THAT(IsNotNull(CastField<FInt64Property>(EchoInt64->GetReturnProperty()), TEXT("EchoInt64 return should reflect as FInt64Property")));
		}

		UFunction* EchoUInt8 = FindGeneratedFunction(ScriptClass, TEXT("EchoUInt8"));
		ASSERT_THAT(IsNotNull(EchoUInt8, TEXT("EchoUInt8 should be generated")));
		if (EchoUInt8 != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FByteProperty>(EchoUInt8, TEXT("Value")), TEXT("EchoUInt8 parameter should reflect as FByteProperty")));
			ASSERT_THAT(IsNotNull(CastField<FByteProperty>(EchoUInt8->GetReturnProperty()), TEXT("EchoUInt8 return should reflect as FByteProperty")));
		}

		UFunction* EchoUInt16 = FindGeneratedFunction(ScriptClass, TEXT("EchoUInt16"));
		ASSERT_THAT(IsNotNull(EchoUInt16, TEXT("EchoUInt16 should be generated")));
		if (EchoUInt16 != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FUInt16Property>(EchoUInt16, TEXT("Value")), TEXT("EchoUInt16 parameter should reflect as FUInt16Property")));
			ASSERT_THAT(IsNotNull(CastField<FUInt16Property>(EchoUInt16->GetReturnProperty()), TEXT("EchoUInt16 return should reflect as FUInt16Property")));
		}

		UFunction* EchoUInt = FindGeneratedFunction(ScriptClass, TEXT("EchoUInt"));
		ASSERT_THAT(IsNotNull(EchoUInt, TEXT("EchoUInt should be generated")));
		if (EchoUInt != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FUInt32Property>(EchoUInt, TEXT("Value")), TEXT("EchoUInt parameter should reflect as FUInt32Property")));
			ASSERT_THAT(IsNotNull(CastField<FUInt32Property>(EchoUInt->GetReturnProperty()), TEXT("EchoUInt return should reflect as FUInt32Property")));
		}

		UFunction* EchoUInt64 = FindGeneratedFunction(ScriptClass, TEXT("EchoUInt64"));
		ASSERT_THAT(IsNotNull(EchoUInt64, TEXT("EchoUInt64 should be generated")));
		if (EchoUInt64 != nullptr)
		{
			ASSERT_THAT(IsNotNull(FindFProperty<FUInt64Property>(EchoUInt64, TEXT("Value")), TEXT("EchoUInt64 parameter should reflect as FUInt64Property")));
			ASSERT_THAT(IsNotNull(CastField<FUInt64Property>(EchoUInt64->GetReturnProperty()), TEXT("EchoUInt64 return should reflect as FUInt64Property")));
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int UFUNCTION width actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoInt8"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoInt8 should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<int8>(41);
			ASSERT_THAT(AreEqual(static_cast<int8>(42), Invoker.CallAndReturn<int8>(0), TEXT("UFUNCTION int8 parameter and return should execute")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoInt16"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoInt16 should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<int16>(29998);
			ASSERT_THAT(AreEqual(static_cast<int16>(30000), Invoker.CallAndReturn<int16>(0), TEXT("UFUNCTION int16 parameter and return should execute")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoInt"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoInt should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<int32>(39);
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("UFUNCTION int parameter and return should execute")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoInt64"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoInt64 should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<int64>(9999999996LL);
			ASSERT_THAT(AreEqual(static_cast<int64>(10000000000LL), Invoker.CallAndReturn<int64>(0), TEXT("UFUNCTION int64 parameter and return should execute")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoUInt8"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoUInt8 should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<uint8>(250);
			ASSERT_THAT(AreEqual(static_cast<uint8>(255), Invoker.CallAndReturn<uint8>(0), TEXT("UFUNCTION uint8 parameter and return should execute")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoUInt16"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoUInt16 should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<uint16>(59994);
			ASSERT_THAT(AreEqual(static_cast<uint16>(60000), Invoker.CallAndReturn<uint16>(0), TEXT("UFUNCTION uint16 parameter and return should execute")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoUInt"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoUInt should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<uint32>(2999999993u);
			ASSERT_THAT(AreEqual(static_cast<uint32>(3000000000u), Invoker.CallAndReturn<uint32>(0), TEXT("UFUNCTION uint parameter and return should execute")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("EchoUInt64"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("EchoUInt64 should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<uint64>(11999999999999999992ull);
			ASSERT_THAT(AreEqual(static_cast<uint64>(12000000000000000000ull), Invoker.CallAndReturn<uint64>(0), TEXT("UFUNCTION uint64 parameter and return should execute")));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
