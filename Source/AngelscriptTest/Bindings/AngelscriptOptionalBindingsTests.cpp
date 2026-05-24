// ============================================================================
// AngelscriptOptionalBindingsTests.cpp
//
// TOptional binding coverage. Automation IDs:
//   - Angelscript.TestModule.Bindings.Container.Optional.OptionalCompat
//   - Angelscript.TestModule.Bindings.Container.Optional.OptionalGetValueUnsetError
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Section: OptionalCompat — baseline (int / FName)
// ----------------------------------------------------------------------------

namespace
{
	bool RunOptionalSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FCoverageModuleScope ModuleScope(Test, Engine, TEXT("ASOptional_Compat"), TEXT(R"(
int OptEmpty_IsSet()
{
	TOptional<int> O;
	return O.IsSet() ? 1 : 0;
}

int OptEmpty_GetFallback()
{
	TOptional<int> O;
	return O.Get(7);
}

int OptSet_IsSet()
{
	TOptional<int> O;
	O.Set(42);
	return O.IsSet() ? 1 : 0;
}

int OptSet_GetValue()
{
	TOptional<int> O;
	O.Set(42);
	return O.GetValue();
}

int OptCopy_Equals()
{
	TOptional<int> O;
	O.Set(42);
	TOptional<int> Copy(O);
	return (Copy == O) ? 1 : 0;
}

int OptAssign_GetValue()
{
	TOptional<int> O;
	O.Set(42);
	TOptional<int> Copy(O);
	Copy = 19;
	return Copy.GetValue();
}

int OptReset_IsSet()
{
	TOptional<int> O;
	O.Set(42);
	TOptional<int> Copy(O);
	Copy.Reset();
	return Copy.IsSet() ? 1 : 0;
}

int OptFName_IsSet()
{
	TOptional<FName> O(FName("Alpha"));
	return O.IsSet() ? 1 : 0;
}

int OptFName_GetValue()
{
	TOptional<FName> O(FName("Alpha"));
	return (O.GetValue() == FName("Alpha")) ? 1 : 0;
}

int OptFName_GetWithValue()
{
	TOptional<FName> O(FName("Alpha"));
	return (O.Get(FName("Fallback")) == FName("Alpha")) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int OptEmpty_IsSet()"),       TEXT("Empty TOptional should not be set"),                 0 },
			{ TEXT("int OptEmpty_GetFallback()"),  TEXT("Empty TOptional Get should return fallback 7"),      7 },
			{ TEXT("int OptSet_IsSet()"),          TEXT("TOptional after Set(42) should be set"),             1 },
			{ TEXT("int OptSet_GetValue()"),       TEXT("TOptional GetValue should return 42"),               42 },
			{ TEXT("int OptCopy_Equals()"),        TEXT("Copy-constructed TOptional should equal original"),  1 },
			{ TEXT("int OptAssign_GetValue()"),    TEXT("Assigned TOptional GetValue should be 19"),          19 },
			{ TEXT("int OptReset_IsSet()"),        TEXT("Reset TOptional should not be set"),                 0 },
			{ TEXT("int OptFName_IsSet()"),        TEXT("FName TOptional should be set"),                     1 },
			{ TEXT("int OptFName_GetValue()"),     TEXT("FName TOptional GetValue should match Alpha"),       1 },
			{ TEXT("int OptFName_GetWithValue()"), TEXT("FName TOptional Get with value returns value not fallback"), 1 },
		};
		return AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: OptionalGetValueUnsetError — exception path with log suppression
	// -----------------------------------------------------------------------

	bool RunOptionalErrorSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FCoverageModuleScope ModuleScope(Test, Engine, TEXT("ASOptional_GetValueUnsetError"), TEXT(R"(
int TriggerGetValueUnset()
{
	TOptional<int> Empty;
	return Empty.GetValue();
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		// Let the exception log through normally; AddExpectedError marks it as
		// expected so the automation framework won't count it as a failure.

		Test.AddExpectedErrorPlain(
			FString(TEXT("ASOptional_GetValueUnsetError")),
			EAutomationExpectedErrorFlags::Contains, 0);
		Test.AddExpectedErrorPlain(
			TEXT("GetValue() called on Optional when not set"),
			EAutomationExpectedErrorFlags::Contains, 0);
		Test.AddExpectedErrorPlain(
			TEXT("int TriggerGetValueUnset()"),
			EAutomationExpectedErrorFlags::Contains, 0);

		return AngelscriptTestBindings::ExecuteFunctionExpectingScriptException(
			Test, Engine, Module, 
			TEXT("int TriggerGetValueUnset()"),
			TEXT("Unset TOptional.GetValue should raise exception"),
			FString(TEXT("GetValue() called on Optional when not set")));
	}

	// -----------------------------------------------------------------------
	// Section: OptionalTypeMatrix — bool / float / FString / FVector / enum /
	// UObject handle. Each case stays tiny so a failing type is localizable.
	// -----------------------------------------------------------------------

	bool RunOptionalTypeMatrixSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FCoverageModuleScope ModuleScope(Test, Engine, TEXT("ASOptional_TypeMatrix"), TEXT(R"(
// ---- bool ----
int OptBool_True_IsSet()
{
	TOptional<bool> O(true);
	return O.IsSet() ? 1 : 0;
}
int OptBool_True_GetValue()
{
	TOptional<bool> O(true);
	return O.GetValue() ? 1 : 0;
}
int OptBool_False_IsSet()
{
	TOptional<bool> O(false);
	return O.IsSet() ? 1 : 0;
}
int OptBool_False_GetValue()
{
	TOptional<bool> O(false);
	return O.GetValue() ? 1 : 0;
}

// ---- double ----
int OptDouble_GetValue()
{
	TOptional<float> O(3.5f);
	return (O.GetValue() > 3.4f && O.GetValue() < 3.6f) ? 1 : 0;
}
int OptDouble_GetFallback()
{
	TOptional<float> O;
	float V = O.Get(7.25f);
	return (V > 7.24f && V < 7.26f) ? 1 : 0;
}

// ---- FString ----
int OptString_GetValue()
{
	TOptional<FString> O(FString("Hello"));
	return (O.GetValue() == "Hello") ? 1 : 0;
}
int OptString_FallbackVsValue()
{
	TOptional<FString> O(FString("Real"));
	return (O.Get("Fallback") == "Real") ? 1 : 0;
}
int OptString_EmptyFallback()
{
	TOptional<FString> O;
	return (O.Get("Fallback") == "Fallback") ? 1 : 0;
}

// ---- FVector ----
// XYZ asserted independently as int return so failure pinpoints the bad axis.
int OptVector_GetValueX()
{
	TOptional<FVector> O(FVector(1, 2, 3));
	float X = O.GetValue().X;
	return (X > 0.9f && X < 1.1f) ? 1 : 0;
}
int OptVector_GetValueY()
{
	TOptional<FVector> O(FVector(1, 2, 3));
	float Y = O.GetValue().Y;
	return (Y > 1.9f && Y < 2.1f) ? 1 : 0;
}
int OptVector_GetValueZ()
{
	TOptional<FVector> O(FVector(1, 2, 3));
	float Z = O.GetValue().Z;
	return (Z > 2.9f && Z < 3.1f) ? 1 : 0;
}
int OptVector_Reset_IsSet()
{
	TOptional<FVector> O(FVector(1, 2, 3));
	O.Reset();
	return O.IsSet() ? 1 : 0;
}

// ---- enum (use existing EAngelscriptTypeId-style: ESlateVisibility is
// always registered. Fall back to a UE-known enum that is universally bound.)
// We use ETickingGroup which is part of CoreUObject reflection.
int OptEnum_GetValue()
{
	TOptional<ETickingGroup> O(ETickingGroup::TG_PrePhysics);
	return (O.GetValue() == ETickingGroup::TG_PrePhysics) ? 1 : 0;
}

// ---- UObject handle ----
// TOptional<UObject> stores the handle (nullable) — IsSet means the handle
// slot has been populated, regardless of the handle pointing to null.
int OptObject_NullSet_IsSet()
{
	TOptional<UObject> O(nullptr);
	return O.IsSet() ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int OptBool_True_IsSet()"),       TEXT("TOptional<bool>(true) should be set"),                        1 },
			{ TEXT("int OptBool_True_GetValue()"),    TEXT("TOptional<bool>(true).GetValue should be true"),              1 },
			{ TEXT("int OptBool_False_IsSet()"),      TEXT("TOptional<bool>(false) should still be set"),                  1 },
			{ TEXT("int OptBool_False_GetValue()"),   TEXT("TOptional<bool>(false).GetValue should be false"),             0 },
			{ TEXT("int OptDouble_GetValue()"),       TEXT("TOptional<float>(3.5).GetValue should be ~3.5"),              1 },
			{ TEXT("int OptDouble_GetFallback()"),    TEXT("Empty TOptional<float>.Get(7.25) should fall back to 7.25"),  1 },
			{ TEXT("int OptString_GetValue()"),       TEXT("TOptional<FString>.GetValue should match"),                   1 },
			{ TEXT("int OptString_FallbackVsValue()"),TEXT("TOptional<FString>.Get(default) should return real value"),   1 },
			{ TEXT("int OptString_EmptyFallback()"),  TEXT("Empty TOptional<FString>.Get(default) should return default"),1 },
			{ TEXT("int OptVector_GetValueX()"),      TEXT("TOptional<FVector>.GetValue.X should be ~1"),                 1 },
			{ TEXT("int OptVector_GetValueY()"),      TEXT("TOptional<FVector>.GetValue.Y should be ~2"),                 1 },
			{ TEXT("int OptVector_GetValueZ()"),      TEXT("TOptional<FVector>.GetValue.Z should be ~3"),                 1 },
			{ TEXT("int OptVector_Reset_IsSet()"),    TEXT("TOptional<FVector>.Reset should clear IsSet"),                0 },
			{ TEXT("int OptEnum_GetValue()"),         TEXT("TOptional<ETickingGroup>.GetValue should match TG_PrePhysics"),1 },
			{ TEXT("int OptObject_NullSet_IsSet()"),  TEXT("TOptional<UObject>(nullptr) construction still sets the slot"),1 },
		};
		return AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: OptionalApiCoverage
	//
	// Goal: exercise every binding entry-point in Bind_TOptional.cpp once:
	//   - opAssign(const TOptional<T>&)        ← optional-to-optional
	//   - opAssign(const T&in)                 ← raw-value-to-optional
	//   - opEquals between empty/empty, set/set, set/empty, set(a)/set(b)
	//   - Set/Reset cycling
	//   - InitConstruct from value / CopyConstruct from set / from empty
	// -----------------------------------------------------------------------

	bool RunOptionalApiCoverageSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FCoverageModuleScope ModuleScope(Test, Engine, TEXT("ASOptional_ApiCoverage"), TEXT(R"(
int OptApi_EmptyEqualsEmpty()
{
	TOptional<int> A;
	TOptional<int> B;
	return (A == B) ? 1 : 0;
}
int OptApi_SetNotEqualsEmpty()
{
	TOptional<int> A;
	A.Set(5);
	TOptional<int> B;
	return (A == B) ? 1 : 0;
}
int OptApi_SetEqualsSameValue()
{
	TOptional<int> A;
	A.Set(5);
	TOptional<int> B;
	B.Set(5);
	return (A == B) ? 1 : 0;
}
int OptApi_SetNotEqualsDifferentValue()
{
	TOptional<int> A;
	A.Set(5);
	TOptional<int> B;
	B.Set(6);
	return (A == B) ? 1 : 0;
}
int OptApi_AssignFromValue_IsSet()
{
	TOptional<int> O;
	O = 42;
	return O.IsSet() ? 1 : 0;
}
int OptApi_AssignFromValue_GetValue()
{
	TOptional<int> O;
	O = 42;
	return O.GetValue();
}
int OptApi_AssignFromValueOverwrites()
{
	TOptional<int> O;
	O.Set(1);
	O = 9;
	return O.GetValue();
}
int OptApi_AssignOptionalFromSet_IsSet()
{
	TOptional<int> Src;
	Src.Set(7);
	TOptional<int> Dst;
	Dst = Src;
	return Dst.IsSet() ? 1 : 0;
}
int OptApi_AssignOptionalFromSet_GetValue()
{
	TOptional<int> Src;
	Src.Set(7);
	TOptional<int> Dst;
	Dst = Src;
	return Dst.GetValue();
}
int OptApi_AssignOptionalUnsetClears()
{
	TOptional<int> Src;
	TOptional<int> Dst;
	Dst.Set(99);
	Dst = Src;
	return Dst.IsSet() ? 1 : 0;
}
int OptApi_ResetThenSetRoundtrip_IsSet()
{
	TOptional<int> O;
	O.Set(1);
	O.Reset();
	O.Set(2);
	return O.IsSet() ? 1 : 0;
}
int OptApi_ResetThenSetRoundtrip_GetValue()
{
	TOptional<int> O;
	O.Set(1);
	O.Reset();
	O.Set(2);
	return O.GetValue();
}
int OptApi_GetMutableViaRef()
{
	TOptional<int> O;
	O.Set(10);
	int& Ref = O.GetValue();
	Ref = 20;
	return O.GetValue();
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int OptApi_EmptyEqualsEmpty()"),                TEXT("empty == empty should be true"),                      1 },
			{ TEXT("int OptApi_SetNotEqualsEmpty()"),                TEXT("set vs empty should be unequal"),                      0 },
			{ TEXT("int OptApi_SetEqualsSameValue()"),               TEXT("set(5) == set(5) should be equal"),                   1 },
			{ TEXT("int OptApi_SetNotEqualsDifferentValue()"),       TEXT("set(5) == set(6) should be unequal"),                 0 },
			{ TEXT("int OptApi_AssignFromValue_IsSet()"),            TEXT("opAssign(value) on empty should set the slot"),       1 },
			{ TEXT("int OptApi_AssignFromValue_GetValue()"),         TEXT("opAssign(value) should store the assigned value"),    42 },
			{ TEXT("int OptApi_AssignFromValueOverwrites()"),        TEXT("opAssign(value) should overwrite previous content"),  9 },
			{ TEXT("int OptApi_AssignOptionalFromSet_IsSet()"),      TEXT("opAssign(optional) from set should leave Dst set"),   1 },
			{ TEXT("int OptApi_AssignOptionalFromSet_GetValue()"),   TEXT("opAssign(optional) from set should propagate value"), 7 },
			{ TEXT("int OptApi_AssignOptionalUnsetClears()"),        TEXT("opAssign(optional) from unset should clear Dst"),     0 },
			{ TEXT("int OptApi_ResetThenSetRoundtrip_IsSet()"),      TEXT("Reset+Set should leave optional set"),                1 },
			{ TEXT("int OptApi_ResetThenSetRoundtrip_GetValue()"),   TEXT("Reset+Set(2) should store value 2"),                  2 },
			{ TEXT("int OptApi_GetMutableViaRef()"),                 TEXT("non-const GetValue should return mutable reference"), 20 },
		};
		return AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: OptionalReturnType — exercise non-int return type paths.
	// Script functions return bool / float / FString / FVector directly
	// from Optional operations. C++ side uses typed assertion helpers.
	// -----------------------------------------------------------------------

	bool RunOptionalReturnTypeSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FCoverageModuleScope ModuleScope(Test, Engine, TEXT("ASOptional_ReturnType"), TEXT(R"(
bool OptRet_Bool_IsSet()
{
	TOptional<int> O;
	O.Set(42);
	return O.IsSet();
}

bool OptRet_Bool_IsSetEmpty()
{
	TOptional<int> O;
	return O.IsSet();
}

float OptRet_Float_GetValue()
{
	TOptional<float> O(3.5f);
	return O.GetValue();
}

float OptRet_Float_GetFallback()
{
	TOptional<float> O;
	return O.Get(7.25f);
}

FString OptRet_String_GetValue()
{
	TOptional<FString> O(FString("Hello"));
	return O.GetValue();
}

FString OptRet_String_GetFallback()
{
	TOptional<FString> O;
	return O.Get("Fallback");
}

// Verify FString returns via script-side comparison (C++ reads int).
int OptRet_VerifyString_GetValue()
{
	TOptional<FString> O(FString("Hello"));
	FString V = O.GetValue();
	return (V == "Hello") ? 1 : 0;
}
int OptRet_VerifyString_GetFallback()
{
	TOptional<FString> O;
	FString V = O.Get("Fallback");
	return (V == "Fallback") ? 1 : 0;
}

FVector OptRet_Vector_GetValue()
{
	TOptional<FVector> O(FVector(1, 2, 3));
	return O.GetValue();
}

// Verify FVector returns via script-side decomposition.
int OptRet_VerifyVector_X()
{
	FVector V = OptRet_Vector_GetValue();
	return (V.X > 0.9f && V.X < 1.1f) ? 1 : 0;
}
int OptRet_VerifyVector_Y()
{
	FVector V = OptRet_Vector_GetValue();
	return (V.Y > 1.9f && V.Y < 2.1f) ? 1 : 0;
}
int OptRet_VerifyVector_Z()
{
	FVector V = OptRet_Vector_GetValue();
	return (V.Z > 2.9f && V.Z < 3.1f) ? 1 : 0;
}

// Return TOptional<int> from function
TOptional<int> MakeOptionalInt()
{
	TOptional<int> O;
	O.Set(42);
	return O;
}
int OptRet_VerifyOptionalInt_IsSet()
{
	TOptional<int> O = MakeOptionalInt();
	return O.IsSet() ? 1 : 0;
}
int OptRet_VerifyOptionalInt_GetValue()
{
	TOptional<int> O = MakeOptionalInt();
	return O.GetValue();
}

// Return empty TOptional<int>
TOptional<int> MakeOptionalIntEmpty()
{
	TOptional<int> O;
	return O;
}
int OptRet_VerifyOptionalIntEmpty_IsSet()
{
	TOptional<int> O = MakeOptionalIntEmpty();
	return O.IsSet() ? 1 : 0;
}

// Return TOptional<FString> from function
TOptional<FString> MakeOptionalString()
{
	TOptional<FString> O(FString("World"));
	return O;
}
int OptRet_VerifyOptionalString_IsSet()
{
	TOptional<FString> O = MakeOptionalString();
	return O.IsSet() ? 1 : 0;
}
int OptRet_VerifyOptionalString_Value()
{
	TOptional<FString> O = MakeOptionalString();
	return (O.GetValue() == "World") ? 1 : 0;
}

// Return TOptional<FVector> from function
TOptional<FVector> MakeOptionalVector()
{
	TOptional<FVector> O(FVector(10, 20, 30));
	return O;
}
int OptRet_VerifyOptionalVector_IsSet()
{
	TOptional<FVector> O = MakeOptionalVector();
	return O.IsSet() ? 1 : 0;
}
int OptRet_VerifyOptionalVector_X()
{
	TOptional<FVector> O = MakeOptionalVector();
	return (O.GetValue().X > 9.9f && O.GetValue().X < 10.1f) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		// Direct bool return assertions
		AngelscriptTestBindings::ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool OptRet_Bool_IsSet()"), TEXT("bool return: set Optional.IsSet() should be true"), true);
		AngelscriptTestBindings::ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool OptRet_Bool_IsSetEmpty()"), TEXT("bool return: empty Optional.IsSet() should be false"), false);

		// Direct float return assertions
		AngelscriptTestBindings::ExpectGlobalReturnFloat(Test, Engine, Module, 
			TEXT("float OptRet_Float_GetValue()"), TEXT("float return: GetValue should be 3.5"), 3.5f);
		AngelscriptTestBindings::ExpectGlobalReturnFloat(Test, Engine, Module, 
			TEXT("float OptRet_Float_GetFallback()"), TEXT("float return: Get fallback should be 7.25"), 7.25f);

		// FString / FVector returns verified via script-side int wrappers
		const FExpectedGlobalInt IntCases[] = {
			{ TEXT("int OptRet_VerifyString_GetValue()"),         TEXT("FString return: GetValue should match 'Hello'"),         1 },
			{ TEXT("int OptRet_VerifyString_GetFallback()"),      TEXT("FString return: Get fallback should match 'Fallback'"),  1 },
			{ TEXT("int OptRet_VerifyVector_X()"),                TEXT("FVector return: X should be ~1"),                        1 },
			{ TEXT("int OptRet_VerifyVector_Y()"),                TEXT("FVector return: Y should be ~2"),                        1 },
			{ TEXT("int OptRet_VerifyVector_Z()"),                TEXT("FVector return: Z should be ~3"),                        1 },
			{ TEXT("int OptRet_VerifyOptionalInt_IsSet()"),       TEXT("TOptional<int> return: should be set"),                  1 },
			{ TEXT("int OptRet_VerifyOptionalInt_GetValue()"),    TEXT("TOptional<int> return: GetValue should be 42"),          42 },
			{ TEXT("int OptRet_VerifyOptionalIntEmpty_IsSet()"),  TEXT("TOptional<int> empty return: should not be set"),        0 },
			{ TEXT("int OptRet_VerifyOptionalString_IsSet()"),    TEXT("TOptional<FString> return: should be set"),              1 },
			{ TEXT("int OptRet_VerifyOptionalString_Value()"),    TEXT("TOptional<FString> return: value should match 'World'"),  1 },
			{ TEXT("int OptRet_VerifyOptionalVector_IsSet()"),    TEXT("TOptional<FVector> return: should be set"),              1 },
			{ TEXT("int OptRet_VerifyOptionalVector_X()"),        TEXT("TOptional<FVector> return: X should be ~10"),            1 },
		};
		AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  IntCases);

		return true;
	}

	// -----------------------------------------------------------------------
	// Section: OptionalLogDiagnostic — Log() different Optional types to
	// exercise the FString + TOptional<T> ToString path. Returns 1 on
	// completion so C++ can verify the script compiled and ran.
	// -----------------------------------------------------------------------

	bool RunOptionalLogDiagnosticSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FCoverageModuleScope ModuleScope(Test, Engine, TEXT("ASOptional_LogDiag"), TEXT(R"(
int OptLog_IntAndString()
{
	TOptional<int> OInt;
	OInt.Set(42);
	Log("OptLog TOptional<int> set: " + OInt.GetValue());

	TOptional<int> OEmpty;
	Log("OptLog TOptional<int> empty IsSet: " + OEmpty.IsSet());

	TOptional<FString> OStr(FString("Hello"));
	Log("OptLog TOptional<FString>: " + OStr.GetValue());

	TOptional<FName> OName(FName("TestName"));
	Log("OptLog TOptional<FName>: " + OName.GetValue());

	TOptional<float> OFloat(3.14f);
	Log("OptLog TOptional<float>: " + OFloat.GetValue());

	TOptional<bool> OBool(true);
	Log("OptLog TOptional<bool>: " + OBool.GetValue());

	TOptional<FVector> OVec(FVector(1, 2, 3));
	Log("OptLog TOptional<FVector>: " + OVec.GetValue());

	return 1;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		return AngelscriptTestBindings::ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int OptLog_IntAndString()"),
			TEXT("Log diagnostic: TOptional types should compile and log without crash"), 1);
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptOptionalBindingsTest,
	"Angelscript.TestModule.Bindings.Container.Optional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(OptionalCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		RunOptionalSection(*TestRunner, Engine);
		RunOptionalTypeMatrixSection(*TestRunner, Engine);
		RunOptionalApiCoverageSection(*TestRunner, Engine);
		RunOptionalReturnTypeSection(*TestRunner, Engine);
		RunOptionalLogDiagnosticSection(*TestRunner, Engine);

		}
	}

	TEST_METHOD(OptionalGetValueUnsetError)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		RunOptionalErrorSection(*TestRunner, Engine);

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
