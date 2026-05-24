// ============================================================================
// AngelscriptIntVectorBindingsTests.cpp
//
// Integer vector value-type binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.IntVector.FAngelscriptIntVectorBindingsTest.*
//
// Sections:
//   IntPointArithmetic  — construction, indexing, negate+add, multiply/divide, GetMax/GetMin
//   IntVectorOps        — zero check, construction, indexing, +=, -=, *=, /=
//   IntVector2Compat    — uniform construction, copy, assignment, indexing
//   IntVector4Arithmetic — negate, add, subtract, multiply/divide, indexing
//
// CQTest adaptation notes:
//   All values are literals — no C++ template substitution needed.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptIntVectorBindingsTest,
	"Angelscript.TestModule.Bindings.IntVector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: IntPointArithmetic
	// ====================================================================

	TEST_METHOD(IntPointArithmetic)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASIntVector_IntPoint"), TEXT(R"(
int IntPoint_Construction()
{
	FIntPoint P(4, 9);
	return (P.X == 4 && P.Y == 9) ? 1 : 0;
}
int IntPoint_Indexing()
{
	FIntPoint P(4, 9);
	return (P[0] == 4 && P[1] == 9) ? 1 : 0;
}
int IntPoint_NegateAdd()
{
	FIntPoint P(4, 9);
	return ((-P + FIntPoint(10, 20)) == FIntPoint(6, 11)) ? 1 : 0;
}
int IntPoint_MulDiv()
{
	FIntPoint P(4, 9);
	return (((P * 2) / 2) == P) ? 1 : 0;
}
int IntPoint_GetMax()
{
	FIntPoint P(4, 9);
	return (P.GetMax() == 9) ? 1 : 0;
}
int IntPoint_GetMin()
{
	FIntPoint P(4, 9);
	return (P.GetMin() == 4) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntPoint_Construction()"), TEXT("FIntPoint construction should set X and Y"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntPoint_Indexing()"), TEXT("FIntPoint operator[] should access X and Y"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntPoint_NegateAdd()"), TEXT("FIntPoint negate+add should compute correctly"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntPoint_MulDiv()"), TEXT("FIntPoint multiply then divide should roundtrip"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntPoint_GetMax()"), TEXT("FIntPoint GetMax should return largest component"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntPoint_GetMin()"), TEXT("FIntPoint GetMin should return smallest component"), 1);
	}

	// ====================================================================
	// Section: IntVectorOps
	// ====================================================================

	TEST_METHOD(IntVectorOps)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASIntVector_IntVec"), TEXT(R"(
int IntVec_ZeroIsZero()
{
	FIntVector V = FIntVector();
	return V.IsZero() ? 1 : 0;
}
int IntVec_Construction()
{
	FIntVector V(1, 2, 3);
	return (!V.IsZero()) ? 1 : 0;
}
int IntVec_Indexing()
{
	FIntVector V(1, 2, 3);
	return (V[2] == 3) ? 1 : 0;
}
int IntVec_AddAssign()
{
	FIntVector V(1, 2, 3);
	V += FIntVector(4, 5, 6);
	return (V == FIntVector(5, 7, 9)) ? 1 : 0;
}
int IntVec_SubAssign()
{
	FIntVector V(5, 7, 9);
	V -= FIntVector(4, 5, 6);
	return (V == FIntVector(1, 2, 3)) ? 1 : 0;
}
int IntVec_MulDivAssign()
{
	FIntVector V(1, 2, 3);
	V *= 3;
	V /= 3;
	return (V == FIntVector(1, 2, 3)) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec_ZeroIsZero()"), TEXT("Default FIntVector should be zero"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec_Construction()"), TEXT("FIntVector(1,2,3) should not be zero"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec_Indexing()"), TEXT("FIntVector operator[] should access Z"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec_AddAssign()"), TEXT("FIntVector += should add componentwise"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec_SubAssign()"), TEXT("FIntVector -= should subtract componentwise"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec_MulDivAssign()"), TEXT("FIntVector *= then /= should roundtrip"), 1);
	}

	// ====================================================================
	// Section: IntVector2Compat
	// ====================================================================

	TEST_METHOD(IntVector2Compat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASIntVector_IntVec2"), TEXT(R"(
int IntVec2_Uniform()
{
	FIntVector2 V(7);
	return (V.X == 7 && V.Y == 7) ? 1 : 0;
}
int IntVec2_Copy()
{
	FIntVector2 V(7);
	FIntVector2 C(V);
	return (C == V) ? 1 : 0;
}
int IntVec2_Assignment()
{
	FIntVector2 V(7);
	FIntVector2 A = FIntVector2();
	A = V;
	return (A == V) ? 1 : 0;
}
int IntVec2_Indexing()
{
	FIntVector2 V(7);
	FIntVector2 C(V);
	return (C[1] == 7) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec2_Uniform()"), TEXT("FIntVector2 uniform ctor should set both components"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec2_Copy()"), TEXT("FIntVector2 copy ctor should produce equal vector"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec2_Assignment()"), TEXT("FIntVector2 assignment should produce equal vector"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec2_Indexing()"), TEXT("FIntVector2 operator[] should access Y"), 1);
	}

	// ====================================================================
	// Section: IntVector4Arithmetic
	// ====================================================================

	TEST_METHOD(IntVector4Arithmetic)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASIntVector_IntVec4"), TEXT(R"(
int IntVec4_Negate()
{
	FIntVector4 V(1, 2, 3, 4);
	return ((-V) == FIntVector4(-1, -2, -3, -4)) ? 1 : 0;
}
int IntVec4_Add()
{
	FIntVector4 V(1, 2, 3, 4);
	return ((V + FIntVector4(1, 1, 1, 1)) == FIntVector4(2, 3, 4, 5)) ? 1 : 0;
}
int IntVec4_Subtract()
{
	FIntVector4 V(1, 2, 3, 4);
	return ((V - FIntVector4(1, 1, 1, 1)) == FIntVector4(0, 1, 2, 3)) ? 1 : 0;
}
int IntVec4_MulDiv()
{
	FIntVector4 V(1, 2, 3, 4);
	return (((V * 2) / 2) == V) ? 1 : 0;
}
int IntVec4_Indexing()
{
	FIntVector4 V(1, 2, 3, 4);
	return (V[3] == 4) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec4_Negate()"), TEXT("FIntVector4 negate should flip all components"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec4_Add()"), TEXT("FIntVector4 addition should be componentwise"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec4_Subtract()"), TEXT("FIntVector4 subtraction should be componentwise"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec4_MulDiv()"), TEXT("FIntVector4 multiply then divide should roundtrip"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IntVec4_Indexing()"), TEXT("FIntVector4 operator[] should access W"), 1);
	}
};

#endif
