// ============================================================================
// AngelscriptMapBindingsTests.cpp
//
// TMap binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Container.Map.FAngelscriptMapBindingsTest.*
//
// Sections:
//   MapCompat                          — FName→int / FName→FName baseline,
//                                        type matrix, API coverage, return types,
//                                        log diagnostic
//   MapFindFailureAndFindOrAddRefCompat — Find failure + FindOrAdd ref semantics
//
// CQTest adaptation notes:
//   Two IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   Each RunTest shell becomes a TEST_METHOD calling the corresponding
//   sub-section runner functions.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Section: MapCompat (17 cases from original if/return)
// ----------------------------------------------------------------------------

namespace
{
	bool RunMapSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASMap_Compat"), TEXT(R"(
int MapEmpty_IsEmpty()
{
	TMap<FName, int> M;
	return M.IsEmpty() ? 1 : 0;
}

int MapAddOverwrite_Num()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 4);
	M.Add(FName("Alpha"), 7);
	return M.Num();
}

int MapContains_Key()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	return M.Contains(FName("Alpha")) ? 1 : 0;
}

int MapFind_Success()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 4);
	M.Add(FName("Alpha"), 7);
	int Value = 0;
	return M.Find(FName("Alpha"), Value) ? 1 : 0;
}

int MapFind_Value()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 4);
	M.Add(FName("Alpha"), 7);
	int Value = 0;
	M.Find(FName("Alpha"), Value);
	return Value;
}

int MapFindOrAdd_Existing()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	return M.FindOrAdd(FName("Alpha"));
}

int MapFindOrAdd_NewDefault()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	return M.FindOrAdd(FName("Beta"), 11);
}

int MapNumAfterAdds()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.FindOrAdd(FName("Beta"), 11);
	return M.Num();
}

int MapContainsBoth()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.FindOrAdd(FName("Beta"), 11);
	return (M.Contains(FName("Alpha")) && M.Contains(FName("Beta")) && M.Num() == 2) ? 1 : 0;
}

int MapCopy_Num()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.FindOrAdd(FName("Beta"), 11);
	TMap<FName, int> Copy = M;
	return Copy.Num();
}

int MapCopy_ContainsBoth()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.FindOrAdd(FName("Beta"), 11);
	TMap<FName, int> Copy = M;
	return (Copy.Contains(FName("Alpha")) && Copy.Contains(FName("Beta"))) ? 1 : 0;
}

int MapRemove_Returns()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.FindOrAdd(FName("Beta"), 11);
	TMap<FName, int> Copy = M;
	return Copy.Remove(FName("Alpha")) ? 1 : 0;
}

int MapRemove_NotContains()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.FindOrAdd(FName("Beta"), 11);
	TMap<FName, int> Copy = M;
	Copy.Remove(FName("Alpha"));
	return Copy.Contains(FName("Alpha")) ? 1 : 0;
}

int MapReset_IsEmpty()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.FindOrAdd(FName("Beta"), 11);
	TMap<FName, int> Copy = M;
	Copy.Remove(FName("Alpha"));
	Copy.Reset();
	return Copy.IsEmpty() ? 1 : 0;
}

int MapFNameFind_Success()
{
	TMap<FName, FName> Names;
	Names.Add(FName("A"), FName("Alpha"));
	FName Value;
	return Names.Find(FName("A"), Value) ? 1 : 0;
}

int MapFNameFind_Value()
{
	TMap<FName, FName> Names;
	Names.Add(FName("A"), FName("Alpha"));
	FName Value;
	Names.Find(FName("A"), Value);
	return (Value == FName("Alpha")) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int MapEmpty_IsEmpty()"),        TEXT("Empty TMap should be empty"),                          1 },
			{ TEXT("int MapAddOverwrite_Num()"),     TEXT("TMap same-key Add overwrites, Num=1"),                 1 },
			{ TEXT("int MapContains_Key()"),         TEXT("TMap should contain added key"),                       1 },
			{ TEXT("int MapFind_Success()"),         TEXT("TMap Find should succeed for existing key"),           1 },
			{ TEXT("int MapFind_Value()"),           TEXT("TMap Find value should be overwritten value 7"),       7 },
			{ TEXT("int MapFindOrAdd_Existing()"),   TEXT("TMap FindOrAdd existing key returns 7"),               7 },
			{ TEXT("int MapFindOrAdd_NewDefault()"), TEXT("TMap FindOrAdd new key with default returns 11"),      11 },
			{ TEXT("int MapNumAfterAdds()"),         TEXT("TMap Num after Beta add should be 2"),                 2 },
			{ TEXT("int MapContainsBoth()"),         TEXT("TMap should contain both Alpha and Beta"),             1 },
			{ TEXT("int MapCopy_Num()"),             TEXT("TMap copy Num should be 2"),                           2 },
			{ TEXT("int MapCopy_ContainsBoth()"),    TEXT("TMap copy should contain both keys"),                  1 },
			{ TEXT("int MapRemove_Returns()"),       TEXT("TMap Remove should return true"),                      1 },
			{ TEXT("int MapRemove_NotContains()"),   TEXT("TMap should not contain removed key"),                 0 },
			{ TEXT("int MapReset_IsEmpty()"),        TEXT("TMap Reset should leave map empty"),                   1 },
			{ TEXT("int MapFNameFind_Success()"),    TEXT("TMap<FName,FName> Find should succeed"),               1 },
			{ TEXT("int MapFNameFind_Value()"),      TEXT("TMap<FName,FName> Find value should match Alpha"),     1 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: MapTypeMatrix
	//
	// TMap keys must satisfy CanHashValue && CanCompare (same chain as TSet).
	// FVector is hashable in UE 5.7 (Vector.h provides
	// GetTypeHash(const TVector<T>&)), so we exercise it both as a value
	// (FName→FVector) and as a key (FVector→int) below.
	// -----------------------------------------------------------------------

	bool RunMapTypeMatrixSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASMap_TypeMatrix"), TEXT(R"(
// ---- int → FString ----
int MapIntString_Num()
{
	TMap<int, FString> M;
	M.Add(1, "Alpha");
	M.Add(2, "Beta");
	M.Add(3, "Gamma");
	return M.Num();
}
int MapIntString_FindReturns()
{
	TMap<int, FString> M;
	M.Add(2, "Beta");
	FString Out;
	return M.Find(2, Out) ? 1 : 0;
}
int MapIntString_FindValueLen()
{
	TMap<int, FString> M;
	M.Add(2, "Beta");
	FString Out;
	M.Find(2, Out);
	return Out.Len();
}
int MapIntString_FindMissing()
{
	TMap<int, FString> M;
	M.Add(2, "Beta");
	FString Out;
	return M.Find(999, Out) ? 1 : 0;
}

// ---- FString → int ----
int MapStringInt_Num()
{
	TMap<FString, int> M;
	M.Add("A", 1);
	M.Add("B", 2);
	M.Add("A", 99); // overwrite
	return M.Num();
}
int MapStringInt_Overwrite()
{
	TMap<FString, int> M;
	M.Add("A", 1);
	M.Add("A", 99);
	int Out = 0;
	M.Find("A", Out);
	return Out;
}

// ---- FName → FVector ----
int MapNameVector_FindX()
{
	TMap<FName, FVector> M;
	M.Add(FName("Pos"), FVector(10, 20, 30));
	FVector Out;
	M.Find(FName("Pos"), Out);
	return (Out.X > 9.9f && Out.X < 10.1f) ? 1 : 0;
}
int MapNameVector_FindY()
{
	TMap<FName, FVector> M;
	M.Add(FName("Pos"), FVector(10, 20, 30));
	FVector Out;
	M.Find(FName("Pos"), Out);
	return (Out.Y > 19.9f && Out.Y < 20.1f) ? 1 : 0;
}
int MapNameVector_FindZ()
{
	TMap<FName, FVector> M;
	M.Add(FName("Pos"), FVector(10, 20, 30));
	FVector Out;
	M.Find(FName("Pos"), Out);
	return (Out.Z > 29.9f && Out.Z < 30.1f) ? 1 : 0;
}
int MapNameVector_FindOrAddModifyX()
{
	TMap<FName, FVector> M;
	FVector& Ref = M.FindOrAdd(FName("New"), FVector(1, 0, 0));
	Ref = FVector(7, 8, 9);
	FVector Out;
	M.Find(FName("New"), Out);
	return (Out.X > 6.9f && Out.X < 7.1f) ? 1 : 0;
}
int MapNameVector_FindOrAddModifyY()
{
	TMap<FName, FVector> M;
	FVector& Ref = M.FindOrAdd(FName("New"), FVector(1, 0, 0));
	Ref = FVector(7, 8, 9);
	FVector Out;
	M.Find(FName("New"), Out);
	return (Out.Y > 7.9f && Out.Y < 8.1f) ? 1 : 0;
}
int MapNameVector_FindOrAddModifyZ()
{
	TMap<FName, FVector> M;
	FVector& Ref = M.FindOrAdd(FName("New"), FVector(1, 0, 0));
	Ref = FVector(7, 8, 9);
	FVector Out;
	M.Find(FName("New"), Out);
	return (Out.Z > 8.9f && Out.Z < 9.1f) ? 1 : 0;
}

// ---- int → UObject handle ----
int MapIntObject_NullFind()
{
	TMap<int, UObject> M;
	M.Add(1, nullptr);
	UObject Out;
	return M.Find(1, Out) ? 1 : 0;
}
int MapIntObject_RemoveNum()
{
	TMap<int, UObject> M;
	M.Add(1, nullptr);
	M.Add(2, nullptr);
	M.Remove(1);
	return M.Num();
}
int MapIntObject_RemoveContainsRemoved()
{
	TMap<int, UObject> M;
	M.Add(1, nullptr);
	M.Add(2, nullptr);
	M.Remove(1);
	return M.Contains(1) ? 1 : 0;
}
int MapIntObject_RemoveContainsKept()
{
	TMap<int, UObject> M;
	M.Add(1, nullptr);
	M.Add(2, nullptr);
	M.Remove(1);
	return M.Contains(2) ? 1 : 0;
}

// ---- enum key ----
int MapEnumKey_FindReturns()
{
	TMap<ETickingGroup, int> M;
	M.Add(ETickingGroup::TG_PrePhysics, 100);
	M.Add(ETickingGroup::TG_PostPhysics, 200);
	int Out = 0;
	return M.Find(ETickingGroup::TG_PostPhysics, Out) ? 1 : 0;
}
int MapEnumKey_FindValue()
{
	TMap<ETickingGroup, int> M;
	M.Add(ETickingGroup::TG_PrePhysics, 100);
	M.Add(ETickingGroup::TG_PostPhysics, 200);
	int Out = 0;
	M.Find(ETickingGroup::TG_PostPhysics, Out);
	return Out;
}

// ---- FVector key ---- (UE 5.7 ships GetTypeHash(const TVector<T>&), so
// FVector is a valid TMap key.)
int MapVectorKey_FindReturns()
{
	TMap<FVector, int> M;
	M.Add(FVector(1, 2, 3), 100);
	M.Add(FVector(4, 5, 6), 200);
	int Out = 0;
	return M.Find(FVector(4, 5, 6), Out) ? 1 : 0;
}
int MapVectorKey_FindValue()
{
	TMap<FVector, int> M;
	M.Add(FVector(1, 2, 3), 100);
	M.Add(FVector(4, 5, 6), 200);
	int Out = 0;
	M.Find(FVector(4, 5, 6), Out);
	return Out;
}
int MapVectorKey_Overwrite()
{
	TMap<FVector, int> M;
	M.Add(FVector(1, 2, 3), 100);
	M.Add(FVector(1, 2, 3), 999); // overwrite
	int Out = 0;
	M.Find(FVector(1, 2, 3), Out);
	return Out;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int MapIntString_Num()"),                  TEXT("TMap<int,FString> with 3 unique keys should have Num=3"), 3 },
			{ TEXT("int MapIntString_FindReturns()"),          TEXT("TMap<int,FString> Find should succeed for stored key"),   1 },
			{ TEXT("int MapIntString_FindValueLen()"),         TEXT("TMap<int,FString> Find value 'Beta' should have length 4"),4 },
			{ TEXT("int MapIntString_FindMissing()"),          TEXT("TMap<int,FString> Find missing key should fail"),         0 },
			{ TEXT("int MapStringInt_Num()"),                  TEXT("TMap<FString,int> Add-overwrite should keep Num=2"),      2 },
			{ TEXT("int MapStringInt_Overwrite()"),            TEXT("TMap<FString,int> Add same key should overwrite value"),  99 },
			{ TEXT("int MapNameVector_FindX()"),               TEXT("TMap<FName,FVector> Find value.X should be ~10"),         1 },
			{ TEXT("int MapNameVector_FindY()"),               TEXT("TMap<FName,FVector> Find value.Y should be ~20"),         1 },
			{ TEXT("int MapNameVector_FindZ()"),               TEXT("TMap<FName,FVector> Find value.Z should be ~30"),         1 },
			{ TEXT("int MapNameVector_FindOrAddModifyX()"),    TEXT("FindOrAdd ref.X assignment should propagate to map (~7)"),1 },
			{ TEXT("int MapNameVector_FindOrAddModifyY()"),    TEXT("FindOrAdd ref.Y assignment should propagate to map (~8)"),1 },
			{ TEXT("int MapNameVector_FindOrAddModifyZ()"),    TEXT("FindOrAdd ref.Z assignment should propagate to map (~9)"),1 },
			{ TEXT("int MapIntObject_NullFind()"),             TEXT("TMap<int,UObject> with null handle Find should still succeed"),1 },
			{ TEXT("int MapIntObject_RemoveNum()"),            TEXT("TMap<int,UObject> Remove should leave Num=1"),            1 },
			{ TEXT("int MapIntObject_RemoveContainsRemoved()"),TEXT("TMap<int,UObject> should not contain removed key"),       0 },
			{ TEXT("int MapIntObject_RemoveContainsKept()"),   TEXT("TMap<int,UObject> should still contain kept key"),        1 },
			{ TEXT("int MapEnumKey_FindReturns()"),            TEXT("TMap<ETickingGroup,int> Find should succeed for enum key"),1 },
			{ TEXT("int MapEnumKey_FindValue()"),              TEXT("TMap<ETickingGroup,int> Find value should be 200"),       200 },
			{ TEXT("int MapVectorKey_FindReturns()"),          TEXT("TMap<FVector,int> Find should succeed by hashed vector"), 1 },
			{ TEXT("int MapVectorKey_FindValue()"),            TEXT("TMap<FVector,int> Find value should be 200"),             200 },
			{ TEXT("int MapVectorKey_Overwrite()"),            TEXT("TMap<FVector,int> Add same key should overwrite value"),  999 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: MapApiCoverage
	//
	// Exercise every binding method on Bind_TMap.cpp not yet covered by the
	// baseline sections: opIndex (read + write), RemoveAndCopyValue,
	// GetKeys, GetValues, Empty(slack), opEquals, opAssign.
	// -----------------------------------------------------------------------

	bool RunMapApiCoverageSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASMap_ApiCoverage"), TEXT(R"(
int MapApi_OpIndexRead()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	return M[FName("Alpha")];
}
int MapApi_OpIndexWrite()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M[FName("Alpha")] = 42;
	int Out = 0;
	M.Find(FName("Alpha"), Out);
	return Out;
}
int MapApi_OpIndexInsertDefault_Num()
{
	// AS opIndex on missing key throws rather than auto-inserting (unlike
	// C++ operator[]). Use FindOrAdd to test the insert path instead.
	TMap<FName, int> M;
	M.FindOrAdd(FName("New")) = 99;
	return M.Num();
}
int MapApi_OpIndexInsertDefault_Contains()
{
	TMap<FName, int> M;
	M.FindOrAdd(FName("New")) = 99;
	return M.Contains(FName("New")) ? 1 : 0;
}
int MapApi_RemoveAndCopyValue_Returns()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.Add(FName("Beta"), 11);
	int Out = 0;
	bool bOk = M.RemoveAndCopyValue(FName("Alpha"), Out);
	return bOk ? 1 : 0;
}
int MapApi_RemoveAndCopyValue_OutValue()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.Add(FName("Beta"), 11);
	int Out = 0;
	M.RemoveAndCopyValue(FName("Alpha"), Out);
	return Out;
}
int MapApi_RemoveAndCopyValue_Num()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.Add(FName("Beta"), 11);
	int Out = 0;
	M.RemoveAndCopyValue(FName("Alpha"), Out);
	return M.Num();
}
int MapApi_RemoveAndCopyValue_Contains()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	M.Add(FName("Beta"), 11);
	int Out = 0;
	M.RemoveAndCopyValue(FName("Alpha"), Out);
	return M.Contains(FName("Alpha")) ? 1 : 0;
}
int MapApi_RemoveAndCopyValue_MissingReturns()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	int Out = 123;
	bool bOk = M.RemoveAndCopyValue(FName("Missing"), Out);
	return bOk ? 1 : 0;
}
int MapApi_RemoveAndCopyValue_MissingNum()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 7);
	int Out = 123;
	M.RemoveAndCopyValue(FName("Missing"), Out);
	return M.Num();
}
int MapApi_GetKeys()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 1);
	M.Add(FName("Beta"), 2);
	M.Add(FName("Gamma"), 3);
	TArray<FName> Keys;
	M.GetKeys(Keys);
	return Keys.Num();
}
int MapApi_GetKeys_HasAlpha()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 1);
	M.Add(FName("Beta"), 2);
	TArray<FName> Keys;
	M.GetKeys(Keys);
	bool bHasAlpha = false;
	foreach (FName K : Keys)
	{
		if (K == FName("Alpha")) bHasAlpha = true;
	}
	return bHasAlpha ? 1 : 0;
}
int MapApi_GetKeys_HasBeta()
{
	TMap<FName, int> M;
	M.Add(FName("Alpha"), 1);
	M.Add(FName("Beta"), 2);
	TArray<FName> Keys;
	M.GetKeys(Keys);
	bool bHasBeta = false;
	foreach (FName K : Keys)
	{
		if (K == FName("Beta")) bHasBeta = true;
	}
	return bHasBeta ? 1 : 0;
}
int MapApi_GetValues_Sum()
{
	TMap<FName, int> M;
	M.Add(FName("A"), 1);
	M.Add(FName("B"), 2);
	M.Add(FName("C"), 3);
	TArray<int> Values;
	M.GetValues(Values);
	int Sum = 0;
	foreach (int V : Values)
		Sum += V;
	return Sum;
}
int MapApi_Empty_Slack_IsEmpty()
{
	TMap<FName, int> M;
	M.Add(FName("A"), 1);
	M.Add(FName("B"), 2);
	M.Empty(32);
	return M.IsEmpty() ? 1 : 0;
}
int MapApi_OpEquals_Equal()
{
	TMap<FName, int> A;
	A.Add(FName("X"), 1);
	A.Add(FName("Y"), 2);
	TMap<FName, int> B;
	B.Add(FName("Y"), 2);
	B.Add(FName("X"), 1);
	return (A == B) ? 1 : 0;
}
int MapApi_OpEquals_DifferentValue()
{
	TMap<FName, int> A;
	A.Add(FName("X"), 1);
	TMap<FName, int> B;
	B.Add(FName("X"), 2);
	return (A == B) ? 1 : 0;
}
int MapApi_OpAssign_Num()
{
	TMap<FName, int> Src;
	Src.Add(FName("X"), 1);
	Src.Add(FName("Y"), 2);
	TMap<FName, int> Dst;
	Dst.Add(FName("Z"), 99);
	Dst = Src;
	return Dst.Num();
}
int MapApi_OpAssign_ContainsCopiedX()
{
	TMap<FName, int> Src;
	Src.Add(FName("X"), 1);
	Src.Add(FName("Y"), 2);
	TMap<FName, int> Dst;
	Dst.Add(FName("Z"), 99);
	Dst = Src;
	return Dst.Contains(FName("X")) ? 1 : 0;
}
int MapApi_OpAssign_ContainsCopiedY()
{
	TMap<FName, int> Src;
	Src.Add(FName("X"), 1);
	Src.Add(FName("Y"), 2);
	TMap<FName, int> Dst;
	Dst.Add(FName("Z"), 99);
	Dst = Src;
	return Dst.Contains(FName("Y")) ? 1 : 0;
}
int MapApi_OpAssign_DropsOldKey()
{
	TMap<FName, int> Src;
	Src.Add(FName("X"), 1);
	Src.Add(FName("Y"), 2);
	TMap<FName, int> Dst;
	Dst.Add(FName("Z"), 99);
	Dst = Src;
	return Dst.Contains(FName("Z")) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int MapApi_OpIndexRead()"),                       TEXT("opIndex read should recover stored value"),                 7 },
			{ TEXT("int MapApi_OpIndexWrite()"),                      TEXT("opIndex write should overwrite existing value"),            42 },
			{ TEXT("int MapApi_OpIndexInsertDefault_Num()"),          TEXT("opIndex insert on missing key should leave Num=1"),         1 },
			{ TEXT("int MapApi_OpIndexInsertDefault_Contains()"),     TEXT("opIndex insert on missing key should populate Contains"),   1 },
			{ TEXT("int MapApi_RemoveAndCopyValue_Returns()"),        TEXT("RemoveAndCopyValue should return true on hit"),             1 },
			{ TEXT("int MapApi_RemoveAndCopyValue_OutValue()"),       TEXT("RemoveAndCopyValue should copy value 7 to OutValue"),       7 },
			{ TEXT("int MapApi_RemoveAndCopyValue_Num()"),            TEXT("RemoveAndCopyValue should leave Num=1"),                    1 },
			{ TEXT("int MapApi_RemoveAndCopyValue_Contains()"),       TEXT("RemoveAndCopyValue should drop Contains for removed key"),  0 },
			{ TEXT("int MapApi_RemoveAndCopyValue_MissingReturns()"), TEXT("RemoveAndCopyValue on missing key should return false"),    0 },
			{ TEXT("int MapApi_RemoveAndCopyValue_MissingNum()"),     TEXT("RemoveAndCopyValue on missing key should not change Num"),  1 },
			{ TEXT("int MapApi_GetKeys()"),                           TEXT("GetKeys should populate an array sized Num"),               3 },
			{ TEXT("int MapApi_GetKeys_HasAlpha()"),                  TEXT("GetKeys should include Alpha"),                              1 },
			{ TEXT("int MapApi_GetKeys_HasBeta()"),                   TEXT("GetKeys should include Beta"),                               1 },
			{ TEXT("int MapApi_GetValues_Sum()"),                     TEXT("GetValues sum should be 1+2+3=6"),                          6 },
			{ TEXT("int MapApi_Empty_Slack_IsEmpty()"),               TEXT("Empty(32) should clear the map"),                           1 },
			{ TEXT("int MapApi_OpEquals_Equal()"),                    TEXT("opEquals should be insertion-order independent"),            1 },
			{ TEXT("int MapApi_OpEquals_DifferentValue()"),           TEXT("opEquals with different value should be false"),             0 },
			{ TEXT("int MapApi_OpAssign_Num()"),                      TEXT("opAssign should give destination Num=2"),                    2 },
			{ TEXT("int MapApi_OpAssign_ContainsCopiedX()"),          TEXT("opAssign should copy key X into destination"),               1 },
			{ TEXT("int MapApi_OpAssign_ContainsCopiedY()"),          TEXT("opAssign should copy key Y into destination"),               1 },
			{ TEXT("int MapApi_OpAssign_DropsOldKey()"),              TEXT("opAssign should drop destination's prior key Z"),            0 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: MapReturnType — exercise non-int return type paths through
	// TMap operations. Covers bool/FString/FVector/TMap/TArray returns.
	// -----------------------------------------------------------------------

	bool RunMapReturnTypeSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASMap_ReturnType"), TEXT(R"(
// Direct bool returns
bool MapRet_Bool_Contains()
{
	TMap<FName, int> M;
	M.Add(FName("A"), 1);
	return M.Contains(FName("A"));
}
bool MapRet_Bool_NotContains()
{
	TMap<FName, int> M;
	M.Add(FName("A"), 1);
	return M.Contains(FName("Missing"));
}
bool MapRet_Bool_Find()
{
	TMap<FName, int> M;
	M.Add(FName("A"), 1);
	int Out = 0;
	return M.Find(FName("A"), Out);
}
bool MapRet_Bool_FindMissing()
{
	TMap<FName, int> M;
	int Out = 0;
	return M.Find(FName("Missing"), Out);
}
bool MapRet_Bool_Remove()
{
	TMap<FName, int> M;
	M.Add(FName("A"), 1);
	return M.Remove(FName("A"));
}
bool MapRet_Bool_RemoveAndCopy()
{
	TMap<FName, int> M;
	M.Add(FName("A"), 7);
	int Out = 0;
	return M.RemoveAndCopyValue(FName("A"), Out);
}
bool MapRet_Bool_IsEmpty()
{
	TMap<FName, int> M;
	return M.IsEmpty();
}
bool MapRet_Bool_OpEquals()
{
	TMap<FName, int> A;
	A.Add(FName("X"), 1);
	TMap<FName, int> B;
	B.Add(FName("X"), 1);
	return A == B;
}

// Return TMap from function, verify in script
TMap<FName, int> MakeMap()
{
	TMap<FName, int> M;
	M.Add(FName("A"), 1);
	M.Add(FName("B"), 2);
	return M;
}
int MapRet_VerifyMapReturn_Num()
{
	TMap<FName, int> M = MakeMap();
	return M.Num();
}
int MapRet_VerifyMapReturn_ContainsA()
{
	TMap<FName, int> M = MakeMap();
	return M.Contains(FName("A")) ? 1 : 0;
}
int MapRet_VerifyMapReturn_FindB()
{
	TMap<FName, int> M = MakeMap();
	int Out = 0;
	M.Find(FName("B"), Out);
	return Out;
}

// Return TArray<FName> from GetKeys, verify in script
TArray<FName> GetMapKeys()
{
	TMap<FName, int> M;
	M.Add(FName("X"), 1);
	M.Add(FName("Y"), 2);
	TArray<FName> Keys;
	M.GetKeys(Keys);
	return Keys;
}
int MapRet_VerifyKeysReturn_Num()
{
	TArray<FName> Keys = GetMapKeys();
	return Keys.Num();
}

// Return TArray<int> from GetValues, verify in script
TArray<int> GetMapValues()
{
	TMap<FName, int> M;
	M.Add(FName("X"), 10);
	M.Add(FName("Y"), 20);
	TArray<int> Values;
	M.GetValues(Values);
	return Values;
}
int MapRet_VerifyValuesReturn_Sum()
{
	TArray<int> Values = GetMapValues();
	int Sum = 0;
	foreach (int V : Values)
		Sum += V;
	return Sum;
}

// Return FString value from opIndex
FString MapRet_String_OpIndex()
{
	TMap<int, FString> M;
	M.Add(1, "Hello");
	return M[1];
}
int MapRet_VerifyString_OpIndex()
{
	FString V = MapRet_String_OpIndex();
	return (V == "Hello") ? 1 : 0;
}

// Return FVector value from Find
FVector MapRet_Vector_FromFind()
{
	TMap<FName, FVector> M;
	M.Add(FName("P"), FVector(1, 2, 3));
	FVector Out;
	M.Find(FName("P"), Out);
	return Out;
}
int MapRet_VerifyVector_X()
{
	FVector V = MapRet_Vector_FromFind();
	return (V.X > 0.9f && V.X < 1.1f) ? 1 : 0;
}
int MapRet_VerifyVector_Y()
{
	FVector V = MapRet_Vector_FromFind();
	return (V.Y > 1.9f && V.Y < 2.1f) ? 1 : 0;
}
int MapRet_VerifyVector_Z()
{
	FVector V = MapRet_Vector_FromFind();
	return (V.Z > 2.9f && V.Z < 3.1f) ? 1 : 0;
}

// Return TMap<int,FString> from function
TMap<int, FString> MakeIntStringMap()
{
	TMap<int, FString> M;
	M.Add(1, "Alpha");
	M.Add(2, "Beta");
	M.Add(3, "Gamma");
	return M;
}
int MapRet_VerifyIntStringMap_Num()
{
	TMap<int, FString> M = MakeIntStringMap();
	return M.Num();
}
int MapRet_VerifyIntStringMap_ContainsOne()
{
	TMap<int, FString> M = MakeIntStringMap();
	return M.Contains(1) ? 1 : 0;
}
int MapRet_VerifyIntStringMap_FindTwo()
{
	TMap<int, FString> M = MakeIntStringMap();
	FString Out;
	M.Find(2, Out);
	return (Out == "Beta") ? 1 : 0;
}

// Return TArray<FVector> from TMap values
TArray<FVector> GetMapVectorValues()
{
	TMap<FName, FVector> M;
	M.Add(FName("A"), FVector(1, 0, 0));
	M.Add(FName("B"), FVector(0, 2, 0));
	TArray<FVector> Values;
	M.GetValues(Values);
	return Values;
}
int MapRet_VerifyVectorValues_Num()
{
	TArray<FVector> Values = GetMapVectorValues();
	return Values.Num();
}
int MapRet_VerifyVectorValues_XSum()
{
	TArray<FVector> Values = GetMapVectorValues();
	float XSum = 0.0f;
	foreach (FVector V : Values)
		XSum += V.X;
	return (XSum > 0.9f && XSum < 1.1f) ? 1 : 0;
}
int MapRet_VerifyVectorValues_YSum()
{
	TArray<FVector> Values = GetMapVectorValues();
	float YSum = 0.0f;
	foreach (FVector V : Values)
		YSum += V.Y;
	return (YSum > 1.9f && YSum < 2.1f) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		bool bPassed = true;

		// Direct bool return assertions
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool MapRet_Bool_Contains()"), TEXT("bool return: Contains hit should be true"), true);
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool MapRet_Bool_NotContains()"), TEXT("bool return: Contains miss should be false"), false);
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool MapRet_Bool_Find()"), TEXT("bool return: Find hit should be true"), true);
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool MapRet_Bool_FindMissing()"), TEXT("bool return: Find miss should be false"), false);
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool MapRet_Bool_Remove()"), TEXT("bool return: Remove hit should be true"), true);
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool MapRet_Bool_RemoveAndCopy()"), TEXT("bool return: RemoveAndCopyValue hit should be true"), true);
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool MapRet_Bool_IsEmpty()"), TEXT("bool return: empty map IsEmpty should be true"), true);
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool MapRet_Bool_OpEquals()"), TEXT("bool return: equal maps opEquals should be true"), true);

		// Container/struct returns verified via script-side int wrappers
		const FExpectedGlobalInt IntCases[] = {
			{ TEXT("int MapRet_VerifyMapReturn_Num()"),          TEXT("TMap<FName,int> return: Num should be 2"),                 2 },
			{ TEXT("int MapRet_VerifyMapReturn_ContainsA()"),    TEXT("TMap<FName,int> return: should contain key A"),             1 },
			{ TEXT("int MapRet_VerifyMapReturn_FindB()"),        TEXT("TMap<FName,int> return: Find B value should be 2"),         2 },
			{ TEXT("int MapRet_VerifyKeysReturn_Num()"),         TEXT("TArray<FName> return from GetKeys: Num should be 2"),      2 },
			{ TEXT("int MapRet_VerifyValuesReturn_Sum()"),       TEXT("TArray<int> return from GetValues: sum should be 30"),     30 },
			{ TEXT("int MapRet_VerifyString_OpIndex()"),         TEXT("FString return from opIndex should match 'Hello'"),        1 },
			{ TEXT("int MapRet_VerifyVector_X()"),               TEXT("FVector return: X should be ~1"),                          1 },
			{ TEXT("int MapRet_VerifyVector_Y()"),               TEXT("FVector return: Y should be ~2"),                          1 },
			{ TEXT("int MapRet_VerifyVector_Z()"),               TEXT("FVector return: Z should be ~3"),                          1 },
			{ TEXT("int MapRet_VerifyIntStringMap_Num()"),       TEXT("TMap<int,FString> return: Num should be 3"),               3 },
			{ TEXT("int MapRet_VerifyIntStringMap_ContainsOne()"),TEXT("TMap<int,FString> return: should contain key 1"),          1 },
			{ TEXT("int MapRet_VerifyIntStringMap_FindTwo()"),   TEXT("TMap<int,FString> return: Find(2) should match 'Beta'"),   1 },
			{ TEXT("int MapRet_VerifyVectorValues_Num()"),       TEXT("TArray<FVector> return: Num should be 2"),                 2 },
			{ TEXT("int MapRet_VerifyVectorValues_XSum()"),      TEXT("TArray<FVector> return: X sum should be ~1"),              1 },
			{ TEXT("int MapRet_VerifyVectorValues_YSum()"),      TEXT("TArray<FVector> return: Y sum should be ~2"),              1 },
		};
		bPassed &= ExpectGlobalInts(Test, Engine, Module,  IntCases);

		return bPassed;
	}

	bool RunMapFindFailureSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASMap_FindFailure"), TEXT(R"(
int MapFindMissing_ReturnsFalse()
{
	TMap<FName, int> M;
	int OutValue = 99;
	return M.Find(FName("Missing"), OutValue) ? 1 : 0;
}

int MapFindMissing_PreservesOut()
{
	TMap<FName, int> M;
	int OutValue = 99;
	M.Find(FName("Missing"), OutValue);
	return OutValue;
}

int MapFindOrAddRef_Gamma()
{
	TMap<FName, int> M;
	int& Gamma = M.FindOrAdd(FName("Gamma"));
	Gamma = 33;
	int OutValue = 0;
	if (!M.Find(FName("Gamma"), OutValue))
		return -1;
	return OutValue;
}

int MapFindOrAddRef_Delta()
{
	TMap<FName, int> M;
	int& Delta = M.FindOrAdd(FName("Delta"), 11);
	Delta = 12;
	int OutValue = 0;
	if (!M.Find(FName("Delta"), OutValue))
		return -1;
	return OutValue;
}

int MapFindOrAdd_FinalNum()
{
	TMap<FName, int> M;
	int& Gamma = M.FindOrAdd(FName("Gamma"));
	Gamma = 33;
	int& Delta = M.FindOrAdd(FName("Delta"), 11);
	Delta = 12;
	return M.Num();
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int MapFindMissing_ReturnsFalse()"), TEXT("TMap Find missing key should return false"),            0 },
			{ TEXT("int MapFindMissing_PreservesOut()"),  TEXT("TMap Find failure should preserve out variable"),       99 },
			{ TEXT("int MapFindOrAddRef_Gamma()"),        TEXT("TMap FindOrAdd ref assign then Find should get 33"),    33 },
			{ TEXT("int MapFindOrAddRef_Delta()"),        TEXT("TMap FindOrAdd+default ref assign should get 12"),      12 },
			{ TEXT("int MapFindOrAdd_FinalNum()"),        TEXT("TMap final Num should be 2"),                           2 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: MapLogDiagnostic — Log() different TMap K/V types.
	// -----------------------------------------------------------------------

	bool RunMapLogDiagnosticSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASMap_LogDiag"), TEXT(R"(
int MapLog_Types()
{
	TMap<FName, int> MNameInt;
	MNameInt.Add(FName("A"), 1);
	MNameInt.Add(FName("B"), 2);
	Log("MapLog TMap<FName,int> Num: " + MNameInt.Num());

	TMap<int, FString> MIntStr;
	MIntStr.Add(1, "Alpha");
	MIntStr.Add(2, "Beta");
	Log("MapLog TMap<int,FString> Num: " + MIntStr.Num());

	TMap<FString, int> MStrInt;
	MStrInt.Add("X", 10);
	MStrInt.Add("Y", 20);
	Log("MapLog TMap<FString,int> Num: " + MStrInt.Num());

	TMap<FName, FVector> MNameVec;
	MNameVec.Add(FName("Pos"), FVector(1, 2, 3));
	Log("MapLog TMap<FName,FVector> Num: " + MNameVec.Num());

	TMap<FVector, int> MVecInt;
	MVecInt.Add(FVector(1, 0, 0), 100);
	Log("MapLog TMap<FVector,int> Num: " + MVecInt.Num());

	// Log key/value pairs via foreach
	foreach (int V, FName K : MNameInt)
		Log("MapLog K=" + K + " V=" + V);
	foreach (FString V, int K : MIntStr)
		Log("MapLog K=" + K + " V=" + V);
	foreach (FVector V, FName K : MNameVec)
		Log("MapLog K=" + K + " V=" + V);

	return 1;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		return ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int MapLog_Types()"),
			TEXT("Log diagnostic: TMap types should compile and log without crash"), 1);
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptMapBindingsTest,
	"Angelscript.TestModule.Bindings.Container.Map",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(MapCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunMapSection(*TestRunner, Engine);
		RunMapTypeMatrixSection(*TestRunner, Engine);
		RunMapApiCoverageSection(*TestRunner, Engine);
		RunMapReturnTypeSection(*TestRunner, Engine);
		RunMapLogDiagnosticSection(*TestRunner, Engine);
	}

	TEST_METHOD(MapFindFailureAndFindOrAddRefCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunMapFindFailureSection(*TestRunner, Engine);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
