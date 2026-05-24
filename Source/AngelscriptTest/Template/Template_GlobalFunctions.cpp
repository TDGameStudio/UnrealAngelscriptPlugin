#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// Template_GlobalFunctions
// -----------------------------------------------------------------------------
// Demonstrates how C++ tests should invoke AngelScript *global* functions
// (script-module level, not UFUNCTION / not member of an AS UCLASS) without
// hand-rolling the asIScriptContext setup:
//
//   asIScriptModule* Module = BuildModule(...);
//   FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("int Sum(int, int)"));
//   Invoker.AddArg(static_cast<int32>(17)).AddArg(static_cast<int32>(25));
//   const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
//
// The helper lives in Shared/AngelscriptGlobalFunctionInvoker.h. It mirrors
// the UFUNCTION-focused FFunctionInvoker from AngelscriptReflectiveAccess.h but
// routes through asIScriptContext::SetArgXxx + Execute instead of
// UObject::ProcessEvent / UASFunction::RuntimeCallEvent.
//
// Two tests are provided:
//
//   1. GlobalInvoke       — minimal smoke test over void / int / return-int.
//                            Demonstrates the Call()-only path and the
//                            CallAndReturn<T>() integer-return path.
//   2. GlobalInvokeAllArgs — matrix over every argument kind AS supports at
//                            the raw-context level (bool, int family, float,
//                            double, FString ref, FName ref, struct by-ref,
//                            out-ref, return struct).
//
// Note: the Angelscript fork used in this project rejects mutable module-level
// globals ("Global variable must be const. Mutable global variables are not
// supported."). So scripts here never declare mutable module-scope variables;
// state is passed through parameters and return values instead.
//
// Second quirk: this fork runs with asEP_FLOAT_IS_FLOAT64=1, so `float` at the
// AS source level is actually an 8-byte double. Tests that bind C++-side
// references to `float&out` parameters would silently truncate the stack
// slot, so out-reference cases in this template use `double` on the script
// side for clarity. SetArgFloat/GetReturnFloat still work for passing
// scalar `float` values because the AS runtime operates on the low half of
// the slot consistently when both paths use the 4-byte accessors.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS


// =============================================================================
// Test 1: GlobalInvoke — minimal smoke coverage
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTemplateGlobalFunctionInvokeTest,
	"Angelscript.Template.GlobalFunctions.GlobalInvoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTemplateGlobalFunctionInvokeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		ASTEST_RESET_ENGINE(Engine);
	};

	asIScriptModule* Module = BuildModule(
		*this, Engine, "ASTemplateGlobalInvoke",
		TEXT(R"(
// The AS fork used by this project forbids mutable module-level globals
// ("Global variable 'X' must be const. Mutable global variables are not
// supported."), so "module state between calls" is demonstrated here via
// function composition rather than shared mutable storage: Trigger() is a
// void global the invoker executes, and NextPayload() returns a value
// computed from its arguments. Both exercise the same asIScriptContext
// plumbing — void-return / value-return / multi-arg — which is the thing
// the template is illustrating.

void Trigger()
{
	// Intentionally empty — the template just needs a void global to
	// demonstrate the Call()-only path on FASGlobalFunctionInvoker.
}

int Sum(int A, int B)
{
	return A + B;
}

int NextPayload(int Base, int Delta)
{
	return Base + Delta;
}
)"));
	if (!TestNotNull(TEXT("Template GlobalInvoke module should compile"), Module))
	{
		return false;
	}

	// Void global — no args, no return. Demonstrates the Call()-only path.
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("void Trigger()"));
		if (!Invoker.IsValid() || !Invoker.Call())
		{
			return false;
		}
	}

	// Int-returning global — two args.
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("int Sum(int, int)"));
		Invoker.AddArg(static_cast<int32>(17)).AddArg(static_cast<int32>(25));
		const int32 Result = Invoker.CallAndReturn<int32>(/*Fallback=*/INDEX_NONE);
		TestEqual(TEXT("Sum(17, 25) should return 42"), Result, 42);
	}

	// A second int-returning global, demonstrating that multiple
	// FASGlobalFunctionInvoker instances can drive the same module.
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("int NextPayload(int, int)"));
		Invoker.AddArg(static_cast<int32>(100)).AddArg(static_cast<int32>(23));
		const int32 Result = Invoker.CallAndReturn<int32>(/*Fallback=*/INDEX_NONE);
		TestEqual(TEXT("NextPayload(100, 23) should return 123"), Result, 123);
	}

	}
	return true;
}

// =============================================================================
// Test 2: GlobalInvokeAllArgs — argument / return matrix
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTemplateGlobalFunctionAllArgsTest,
	"Angelscript.Template.GlobalFunctions.GlobalInvokeAllArgs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTemplateGlobalFunctionAllArgsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		ASTEST_RESET_ENGINE(Engine);
	};

	asIScriptModule* Module = BuildModule(
		*this, Engine, "ASTemplateGlobalInvokeAllArgs",
		TEXT(R"(
// ---- Primitives ----
int EchoBool(bool Flag)   { return Flag ? 1 : 0; }
int EchoByte(uint8 Value) { return int(Value); }
int EchoInt(int Value)    { return Value; }
int64 EchoInt64(int64 Value)   { return Value; }
float EchoFloat(float Value)   { return Value; }
double EchoDouble(double Value){ return Value; }

// ---- Text ----
FString EchoString(const FString&in Value) { return Value + "!"; }
FName   EchoName(const FName&in Value)     { return Value; }

// ---- Struct by const ref ----
double VectorMagnitude(const FVector&in V)
{
	return Math::Sqrt(V.X * V.X + V.Y * V.Y + V.Z * V.Z);
}

// ---- Out reference ----
// Use `double` explicitly because this AS fork runs with
// asEP_FLOAT_IS_FLOAT64=1 — `float` in a script declaration is actually
// 8 bytes at runtime, so binding a 4-byte C++ `float*` to `float&out`
// would corrupt the stack. Using `double` makes the script/C++ layout
// match cleanly.
void SplitVector(const FVector&in V, double&out OutX, double&out OutY, double&out OutZ)
{
	OutX = V.X;
	OutY = V.Y;
	OutZ = V.Z;
}

// ---- Struct return (by value through AS return register) ----
// Declared with `double` for the same layout reason as above.
FVector MakeVector(double X, double Y, double Z)
{
	return FVector(X, Y, Z);
}
)"));
	if (!TestNotNull(TEXT("Template GlobalInvokeAllArgs module should compile"), Module))
	{
		return false;
	}

	// bool in → int out
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("int EchoBool(bool)"));
		Invoker.AddArg(true);
		TestEqual(TEXT("EchoBool(true) should return 1"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	// uint8
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("int EchoByte(uint8)"));
		Invoker.AddArg(static_cast<uint8>(250));
		TestEqual(TEXT("EchoByte(250) should return 250"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 250);
	}

	// int32
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("int EchoInt(int)"));
		Invoker.AddArg(static_cast<int32>(-314));
		TestEqual(TEXT("EchoInt(-314) should return -314"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), -314);
	}

	// int64
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("int64 EchoInt64(int64)"));
		Invoker.AddArg(static_cast<int64>(9223372036854775000LL));
		TestEqual(TEXT("EchoInt64 should preserve a wide-int value"),
			Invoker.CallAndReturn<int64>(0), static_cast<int64>(9223372036854775000LL));
	}

	// float (AS runtime default is asEP_FLOAT_IS_FLOAT64=1 — but at the
	// context level the parameter is still declared `float`, so we use
	// SetArgFloat. The UFUNCTION path would have required double.)
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("float EchoFloat(float)"));
		Invoker.AddArg(1.5f);
		const float Result = Invoker.CallAndReturn<float>(0.0f);
		TestTrue(TEXT("EchoFloat(1.5) should return 1.5"), FMath::IsNearlyEqual(Result, 1.5f));
	}

	// double
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("double EchoDouble(double)"));
		Invoker.AddArg(2.71828);
		const double Result = Invoker.CallAndReturn<double>(0.0);
		TestTrue(TEXT("EchoDouble should return its AS-side argument"),
			FMath::IsNearlyEqual(Result, 2.71828));
	}

	// FString by const ref — AS returns an FString
	{
		FString Input(TEXT("Hello"));
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("FString EchoString(const FString&in)"));
		Invoker.AddArgRef(Input);
		if (!Invoker.Call())
		{
			return false;
		}
		FString Result;
		if (!Invoker.ReadReturnStruct<FString>(Result))
		{
			return false;
		}
		TestEqual(TEXT("EchoString should append an exclamation mark"),
			Result, FString(TEXT("Hello!")));
	}

	// FName by const ref — AS returns an FName
	{
		FName Input(TEXT("AlphaName"));
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("FName EchoName(const FName&in)"));
		Invoker.AddArgRef(Input);
		if (!Invoker.Call())
		{
			return false;
		}
		FName Result;
		if (!Invoker.ReadReturnStruct<FName>(Result))
		{
			return false;
		}
		TestEqual(TEXT("EchoName should round-trip an FName"), Result, Input);
	}

	// FVector by const ref → double
	{
		FVector Input(3.0, 4.0, 12.0); // magnitude = 13
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("double VectorMagnitude(const FVector&in)"));
		Invoker.AddArgRef(Input);
		const double Result = Invoker.CallAndReturn<double>(0.0);
		TestTrue(TEXT("VectorMagnitude should compute sqrt(x^2 + y^2 + z^2)"),
			FMath::IsNearlyEqual(Result, 13.0, 1e-6));
	}

	// Out-reference arguments — AS writes back into our live doubles.
	// We use `double` here (not `float`) because this AS fork runs with
	// asEP_FLOAT_IS_FLOAT64=1; binding a 4-byte `float*` to `float&out`
	// would only fill half the stack slot and corrupt subsequent args.
	// The script declares the parameters as `double&out` for clarity.
	{
		FVector Input(7.0, 8.0, 9.0);
		double OutX = 0.0;
		double OutY = 0.0;
		double OutZ = 0.0;
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module,
			TEXT("void SplitVector(const FVector&in, double&out, double&out, double&out)"));
		Invoker.AddArgRef(Input);
		Invoker.AddArgRef(OutX);
		Invoker.AddArgRef(OutY);
		Invoker.AddArgRef(OutZ);
		if (!Invoker.Call())
		{
			return false;
		}
		TestTrue(TEXT("SplitVector should fill OutX"), FMath::IsNearlyEqual(OutX, 7.0));
		TestTrue(TEXT("SplitVector should fill OutY"), FMath::IsNearlyEqual(OutY, 8.0));
		TestTrue(TEXT("SplitVector should fill OutZ"), FMath::IsNearlyEqual(OutZ, 9.0));
	}

	// Struct return by value — AS returns its FVector via the return value
	// buffer. GetAddressOfReturnValue() exposes a pointer to that buffer,
	// which we memcpy into our FVector out-parameter.
	{
		FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("FVector MakeVector(double, double, double)"));
		Invoker.AddArg(1.0).AddArg(2.0).AddArg(3.0);
		if (!Invoker.Call())
		{
			return false;
		}
		FVector Result = FVector::ZeroVector;
		if (!Invoker.ReadReturnStruct<FVector>(Result))
		{
			return false;
		}
		TestTrue(TEXT("MakeVector should return an FVector with the supplied components"),
			Result.Equals(FVector(1.0, 2.0, 3.0)));
	}

	}
	return true;
}

#endif
