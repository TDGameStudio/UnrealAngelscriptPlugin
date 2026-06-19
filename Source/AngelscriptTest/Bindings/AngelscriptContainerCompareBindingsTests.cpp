// ============================================================================
// AngelscriptContainerCompareBindingsTests.cpp
//
// Container compare binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.ContainerCompare.FAngelscriptContainerCompareBindingsTest.*
//
// Sections:
//   SetCompare              — TSet equality, reorder tolerance, size mismatch
//   SetCompareSameSizeMismatch — TSet same-size with different elements
//   MapCompare              — TMap equality, reorder tolerance, size mismatch
//   MapCompareValue         — TMap value-sensitive equality
//   OptionalTypeCompare     — TOptional<int> type-level compare
//   MapDebugger             — TMap debugger summary and member access
//
// CQTest adaptation notes:
//   Six legacy automation tests merged into one TEST_CLASS.
//   Script-based tests use FScopedAngelscriptModule + ExpectGlobalInt.
//   OptionalTypeCompare and MapDebugger use direct C++ type system assertions.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "../../AngelscriptRuntime/Binds/Bind_TMap.h"
#include "../../AngelscriptRuntime/Binds/Bind_TOptional.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptContainerCompareBindingsTest,
	"Angelscript.TestModule.Bindings.ContainerCompare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: SetCompare
	// ====================================================================

	TEST_METHOD(SetCompare)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASContainerCompare_SetCompare"), TEXT(R"(
int SetCompare_EqualReordered()
{
	TSet<int> Left;
	Left.Add(1);
	Left.Add(4);

	TSet<int> Right;
	Right.Add(4);
	Right.Add(1);

	return (Left == Right) ? 1 : 0;
}
int SetCompare_DifferentSize()
{
	TSet<int> Left;
	Left.Add(1);
	Left.Add(4);

	TSet<int> Right;
	Right.Add(4);
	Right.Add(1);
	Right.Add(7);

	return (Left == Right) ? 0 : 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SetCompare_EqualReordered()"), TEXT("TSet reordered elements should compare equal"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SetCompare_DifferentSize()"), TEXT("TSet with different sizes should compare unequal"), 1);
	}

	// ====================================================================
	// Section: SetCompareSameSizeMismatch
	// ====================================================================

	TEST_METHOD(SetCompareSameSizeMismatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASContainerCompare_SetSizeMismatch"), TEXT(R"(
int SetMismatch_ReorderedEqual()
{
	TSet<int> Left;
	Left.Add(1);
	Left.Add(4);

	TSet<int> Reordered;
	Reordered.Add(4);
	Reordered.Add(1);

	return (Left == Reordered) ? 1 : 0;
}
int SetMismatch_SameSizeDifferent()
{
	TSet<int> Left;
	Left.Add(1);
	Left.Add(4);

	TSet<int> DifferentSameSize;
	DifferentSameSize.Add(1);
	DifferentSameSize.Add(9);

	return (Left == DifferentSameSize) ? 0 : 1;
}
int SetMismatch_CopyEqual()
{
	TSet<int> Left;
	Left.Add(1);
	Left.Add(4);

	TSet<int> Copy = Left;

	return (Copy == Left) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SetMismatch_ReorderedEqual()"), TEXT("TSet reordered should compare equal"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SetMismatch_SameSizeDifferent()"), TEXT("TSet same-size mismatched members should compare unequal"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SetMismatch_CopyEqual()"), TEXT("TSet copy should compare equal to original"), 1);
	}

	// ====================================================================
	// Section: MapCompare
	// ====================================================================

	TEST_METHOD(MapCompare)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASContainerCompare_MapCompare"), TEXT(R"(
int MapCompare_EqualReordered()
{
	TMap<FName, int> Left;
	Left.Add(FName("Alpha"), 2);
	Left.Add(FName("Beta"), 5);

	TMap<FName, int> Right;
	Right.Add(FName("Beta"), 5);
	Right.Add(FName("Alpha"), 2);

	return (Left == Right) ? 1 : 0;
}
int MapCompare_DifferentSize()
{
	TMap<FName, int> Left;
	Left.Add(FName("Alpha"), 2);
	Left.Add(FName("Beta"), 5);

	TMap<FName, int> Right;
	Right.Add(FName("Beta"), 5);
	Right.Add(FName("Alpha"), 2);
	Right.Add(FName("Gamma"), 7);

	return (Left == Right) ? 0 : 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MapCompare_EqualReordered()"), TEXT("TMap reordered entries should compare equal"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MapCompare_DifferentSize()"), TEXT("TMap with different sizes should compare unequal"), 1);
	}

	// ====================================================================
	// Section: MapCompareValue
	// ====================================================================

	TEST_METHOD(MapCompareValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASContainerCompare_MapCompareValue"), TEXT(R"(
int MapValue_EqualReordered()
{
	TMap<FName, int> Left;
	Left.Add(FName("Alpha"), 2);
	Left.Add(FName("Beta"), 5);

	TMap<FName, int> Right;
	Right.Add(FName("Beta"), 5);
	Right.Add(FName("Alpha"), 2);

	return (Left == Right) ? 1 : 0;
}
int MapValue_DifferentValue()
{
	TMap<FName, int> Left;
	Left.Add(FName("Alpha"), 2);
	Left.Add(FName("Beta"), 5);

	TMap<FName, int> DifferentValue;
	DifferentValue.Add(FName("Alpha"), 99);
	DifferentValue.Add(FName("Beta"), 5);

	return (Left == DifferentValue) ? 0 : 1;
}
int MapValue_EmptyCompare()
{
	TMap<FName, int> Left;
	Left.Add(FName("Alpha"), 2);
	Left.Add(FName("Beta"), 5);

	TMap<FName, int> Empty;
	TMap<FName, int> AnotherEmpty;

	if (Left == Empty)
		return 0;
	return (Empty == AnotherEmpty) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MapValue_EqualReordered()"), TEXT("TMap value-equal reordered entries should compare equal"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MapValue_DifferentValue()"), TEXT("TMap with different values should compare unequal"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MapValue_EmptyCompare()"), TEXT("TMap empty maps should compare equal and non-empty vs empty should differ"), 1);
	}

	// ====================================================================
	// Section: OptionalTypeCompare
	// ====================================================================

	TEST_METHOD(OptionalTypeCompare)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FAngelscriptTypeUsage IntUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("int")));
		FAngelscriptTypeUsage OptionalUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("TOptional")));
		OptionalUsage.SubTypes.Add(IntUsage);

		ASSERT_THAT(IsTrue(OptionalUsage.CanCompare(), TEXT("TOptional<int> should report type-level compare support")));

		const SIZE_T OptionalSize = static_cast<SIZE_T>(OptionalUsage.GetValueSize());
		const uint32 OptionalAlignment = static_cast<uint32>(OptionalUsage.GetValueAlignment());

		void* LeftStorage = FMemory::Malloc(OptionalSize, OptionalAlignment);
		void* RightStorage = FMemory::Malloc(OptionalSize, OptionalAlignment);
		ON_SCOPE_EXIT
		{
			FMemory::Free(LeftStorage);
			FMemory::Free(RightStorage);
		};

		OptionalUsage.ConstructValue(LeftStorage);
		OptionalUsage.ConstructValue(RightStorage);

		FOptionalOperations OptionalOps(IntUsage);
		FAngelscriptOptional& LeftOptional = *static_cast<FAngelscriptOptional*>(LeftStorage);
		FAngelscriptOptional& RightOptional = *static_cast<FAngelscriptOptional*>(RightStorage);

		if (!this->Assert.IsTrue(OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional), TEXT("Two unset optionals should compare equal")))
		{
			OptionalUsage.DestructValue(LeftStorage);
			OptionalUsage.DestructValue(RightStorage);
			return;
		}

		int32 LeftValue = 7;
		int32 RightValue = 7;
		OptionalOps.Set(LeftOptional, &LeftValue);
		OptionalOps.Set(RightOptional, &RightValue);

		if (!this->Assert.IsTrue(OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional), TEXT("Two equal set optionals should compare equal")))
		{
			OptionalUsage.DestructValue(LeftStorage);
			OptionalUsage.DestructValue(RightStorage);
			return;
		}

		RightValue = 9;
		OptionalOps.Set(RightOptional, &RightValue);
		if (!this->Assert.IsFalse(OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional), TEXT("Different set optionals should compare unequal")))
		{
			OptionalUsage.DestructValue(LeftStorage);
			OptionalUsage.DestructValue(RightStorage);
			return;
		}

		OptionalOps.Reset(RightOptional);
		const bool bSetVsUnsetEqual = OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional);
		OptionalUsage.DestructValue(LeftStorage);
		OptionalUsage.DestructValue(RightStorage);

		ASSERT_THAT(IsFalse(bSetVsUnsetEqual, TEXT("Set and unset optionals should compare unequal")));
	}

	// ====================================================================
	// Section: MapDebugger
	// ====================================================================

	TEST_METHOD(MapDebugger)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FAngelscriptTypeUsage KeyUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("FName")));
		FAngelscriptTypeUsage ValueUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("int")));
		FAngelscriptTypeUsage MapUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("TMap")));
		MapUsage.SubTypes.Add(KeyUsage);
		MapUsage.SubTypes.Add(ValueUsage);
		MapUsage.TypeIndex = 0;

		FScriptMap TestMap;
		FMapOperations MapOps(KeyUsage, ValueUsage);
		FName AlphaName(TEXT("Alpha"));
		FName BetaName(TEXT("Beta"));
		int32 AlphaValue = 2;
		int32 BetaValue = 5;
		MapOps.Add(TestMap, &AlphaName, &AlphaValue);
		MapOps.Add(TestMap, &BetaName, &BetaValue);

		FDebuggerValue SummaryValue;
		if (!this->Assert.IsTrue(MapUsage.GetDebuggerValue(&TestMap, SummaryValue), TEXT("TMap debugger summary should be available")))
		{
			MapOps.Empty(TestMap, 0);
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Num = 2")), SummaryValue.Value, TEXT("TMap debugger summary should show element count")));
		ASSERT_THAT(IsTrue(SummaryValue.bHasMembers, TEXT("TMap debugger summary should report child members")));

		FDebuggerValue NumValue;
		if (!this->Assert.IsTrue(MapUsage.GetDebuggerMember(&TestMap, TEXT("Num"), NumValue), TEXT("TMap debugger should expose Num member")))
		{
			MapOps.Empty(TestMap, 0);
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("2")), NumValue.Value, TEXT("TMap debugger Num member should match element count")));

		FDebuggerValue AlphaDebugValue;
		const bool bAlphaFound = MapUsage.GetDebuggerMember(&TestMap, TEXT("[Alpha]"), AlphaDebugValue);
		MapOps.Empty(TestMap, 0);
		ASSERT_THAT(IsTrue(bAlphaFound, TEXT("TMap debugger should expose FName-keyed members by string identifier")));

		ASSERT_THAT(AreEqual(FString(TEXT("2")), AlphaDebugValue.Value, TEXT("TMap debugger key lookup should return the mapped value")));
	}
};

#endif
