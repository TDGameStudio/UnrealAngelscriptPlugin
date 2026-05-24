// ============================================================================
// AngelscriptSetBindingsTests.cpp
//
// TSet binding coverage — 1 Automation ID:
//   - Angelscript.TestModule.Bindings.Container.SetCompat
//
// Refactored from AngelscriptContainerBindingsTests.cpp using the
// Shared/ Coverage Section base layer.
//
// Coverage matrix (all sections live under the single SetCompat ID):
//   - RunSetSection            : int / FName baseline (9 cases)
//   - RunSetTypeMatrixSection  : FString / FVector(via Add equality) /
//                                double / enum / UObject handle (10 cases)
//   - RunSetApiCoverageSection : Append(TArray), Append(TSet), Empty(slack),
//                                opAssign, opEquals incl. order-independent,
//                                explicit Iterator() walk (8 cases)
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestModuleScope.h"
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
// Section: SetCompat (9 cases from original if/return)
// ----------------------------------------------------------------------------

namespace
{
	bool RunSetSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASSet_Compat"), TEXT(R"(
int SetEmpty_IsEmpty()
{
	TSet<int> S;
	return S.IsEmpty() ? 1 : 0;
}

int SetAddDedup_Num()
{
	TSet<int> S;
	S.Add(4);
	S.Add(4);
	return S.Num();
}

int SetContains_Added()
{
	TSet<int> S;
	S.Add(4);
	return S.Contains(4) ? 1 : 0;
}

int SetCopy_Match()
{
	TSet<int> S;
	S.Add(4);
	TSet<int> Copy = S;
	return (Copy.Contains(4) && Copy.Num() == S.Num()) ? 1 : 0;
}

int SetCopyAdd_Num()
{
	TSet<int> S;
	S.Add(4);
	TSet<int> Copy = S;
	Copy.Add(7);
	return Copy.Num();
}

int SetRemove_Returns()
{
	TSet<int> S;
	S.Add(4);
	S.Add(7);
	return S.Remove(4) ? 1 : 0;
}

int SetRemove_NotContains()
{
	TSet<int> S;
	S.Add(4);
	S.Add(7);
	S.Remove(4);
	return S.Contains(4) ? 1 : 0;
}

int SetReset_IsEmpty()
{
	TSet<int> S;
	S.Add(4);
	S.Add(7);
	S.Remove(4);
	S.Reset();
	return S.IsEmpty() ? 1 : 0;
}

int SetFName_Contains()
{
	TSet<FName> Names;
	Names.Add(FName("Alpha"));
	return Names.Contains(FName("Alpha")) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int SetEmpty_IsEmpty()"),      TEXT("Empty TSet should be empty"),                1 },
			{ TEXT("int SetAddDedup_Num()"),        TEXT("TSet Add dedup should keep Num=1"),          1 },
			{ TEXT("int SetContains_Added()"),      TEXT("TSet should contain added element"),          1 },
			{ TEXT("int SetCopy_Match()"),          TEXT("TSet copy should match original"),            1 },
			{ TEXT("int SetCopyAdd_Num()"),         TEXT("TSet copy Add new element should give Num=2"), 2 },
			{ TEXT("int SetRemove_Returns()"),      TEXT("TSet Remove should return true"),              1 },
			{ TEXT("int SetRemove_NotContains()"),  TEXT("TSet should not contain removed element"),     0 },
			{ TEXT("int SetReset_IsEmpty()"),       TEXT("TSet Reset should leave set empty"),           1 },
			{ TEXT("int SetFName_Contains()"),      TEXT("TSet<FName> should contain added FName"),      1 },
		};
		return AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: SetTypeMatrix
	//
	// TSet requires T to satisfy CanHashValue && CanCompare. The default
	// CppType implementation answers true whenever C++ provides
	// GetTypeHash(const T&) (TModels<CGetTypeHashable, T>::Value). UE 5.7's
	// Vector.h defines GetTypeHash(const TVector<T>&), so TSet<FVector>
	// works out-of-the-box — and is included below to lock that in.
	// -----------------------------------------------------------------------

	bool RunSetTypeMatrixSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASSet_TypeMatrix"), TEXT(R"(
// ---- FString ----
int SetString_AddDedupNum()
{
	TSet<FString> S;
	S.Add("Alpha");
	S.Add("Alpha");
	return S.Num();
}
int SetString_ContainsAlpha()
{
	TSet<FString> S;
	S.Add("Alpha");
	S.Add("Beta");
	return S.Contains("Alpha") ? 1 : 0;
}
int SetString_ContainsBeta()
{
	TSet<FString> S;
	S.Add("Alpha");
	S.Add("Beta");
	return S.Contains("Beta") ? 1 : 0;
}
int SetString_NotContainsMissing()
{
	TSet<FString> S;
	S.Add("Alpha");
	S.Add("Beta");
	return S.Contains("Missing") ? 1 : 0;
}
int SetString_RemoveContains()
{
	TSet<FString> S;
	S.Add("Alpha");
	S.Add("Beta");
	S.Remove("Alpha");
	return S.Contains("Alpha") ? 1 : 0;
}

// ---- bool ----
int SetBool_AddNum()
{
	TSet<bool> S;
	S.Add(true);
	S.Add(false);
	S.Add(true);
	return S.Num();
}
int SetBool_ContainsTrue()
{
	TSet<bool> S;
	S.Add(true);
	return S.Contains(true) ? 1 : 0;
}
int SetBool_NotContainsFalse()
{
	TSet<bool> S;
	S.Add(true);
	return S.Contains(false) ? 1 : 0;
}

// ---- double ----
int SetFloat_ContainsLow()
{
	TSet<float> S;
	S.Add(1.5f);
	S.Add(2.5f);
	return S.Contains(1.5f) ? 1 : 0;
}
int SetFloat_ContainsHigh()
{
	TSet<float> S;
	S.Add(1.5f);
	S.Add(2.5f);
	return S.Contains(2.5f) ? 1 : 0;
}
int SetFloat_Num()
{
	TSet<float> S;
	S.Add(1.5f);
	S.Add(2.5f);
	return S.Num();
}

// ---- enum ----
int SetEnum_ContainsPre()
{
	TSet<ETickingGroup> S;
	S.Add(ETickingGroup::TG_PrePhysics);
	S.Add(ETickingGroup::TG_PostPhysics);
	return S.Contains(ETickingGroup::TG_PrePhysics) ? 1 : 0;
}
int SetEnum_NotContainsDuring()
{
	TSet<ETickingGroup> S;
	S.Add(ETickingGroup::TG_PrePhysics);
	S.Add(ETickingGroup::TG_PostPhysics);
	return S.Contains(ETickingGroup::TG_DuringPhysics) ? 1 : 0;
}
int SetEnum_Num()
{
	TSet<ETickingGroup> S;
	S.Add(ETickingGroup::TG_PrePhysics);
	S.Add(ETickingGroup::TG_PrePhysics);
	S.Add(ETickingGroup::TG_PostPhysics);
	return S.Num();
}

// ---- UObject handle ---- (null handles are allowed, any two nulls dedupe)
int SetObject_NullDedup()
{
	TSet<UObject> S;
	S.Add(nullptr);
	S.Add(nullptr);
	return S.Num();
}

// ---- int32 explicit ----
int SetInt_LargeBatchNum()
{
	TSet<int> S;
	for (int i = 0; i < 10; i += 1)
		S.Add(i);
	for (int i = 0; i < 10; i += 1)
		S.Add(i); // duplicates
	return S.Num();
}

// ---- FVector ---- (UE 5.7 ships GetTypeHash(const TVector<T>&), so
// FVector is hashable and TSet<FVector> is valid.)
int SetVector_AddDedupNum()
{
	TSet<FVector> S;
	S.Add(FVector(1, 2, 3));
	S.Add(FVector(1, 2, 3)); // duplicate
	S.Add(FVector(4, 5, 6));
	return S.Num();
}
int SetVector_ContainsFirst()
{
	TSet<FVector> S;
	S.Add(FVector(1, 2, 3));
	S.Add(FVector(4, 5, 6));
	return S.Contains(FVector(1, 2, 3)) ? 1 : 0;
}
int SetVector_ContainsSecond()
{
	TSet<FVector> S;
	S.Add(FVector(1, 2, 3));
	S.Add(FVector(4, 5, 6));
	return S.Contains(FVector(4, 5, 6)) ? 1 : 0;
}
int SetVector_NotContainsMissing()
{
	TSet<FVector> S;
	S.Add(FVector(1, 2, 3));
	S.Add(FVector(4, 5, 6));
	return S.Contains(FVector(9, 9, 9)) ? 1 : 0;
}
int SetVector_RemoveReturns()
{
	TSet<FVector> S;
	S.Add(FVector(1, 2, 3));
	S.Add(FVector(4, 5, 6));
	return S.Remove(FVector(1, 2, 3)) ? 1 : 0;
}
int SetVector_RemoveNum()
{
	TSet<FVector> S;
	S.Add(FVector(1, 2, 3));
	S.Add(FVector(4, 5, 6));
	S.Remove(FVector(1, 2, 3));
	return S.Num();
}
int SetVector_RemoveContains()
{
	TSet<FVector> S;
	S.Add(FVector(1, 2, 3));
	S.Add(FVector(4, 5, 6));
	S.Remove(FVector(1, 2, 3));
	return S.Contains(FVector(1, 2, 3)) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int SetString_AddDedupNum()"),       TEXT("TSet<FString> Add duplicate should dedup to Num=1"),         1 },
			{ TEXT("int SetString_ContainsAlpha()"),     TEXT("TSet<FString> should contain Alpha"),                         1 },
			{ TEXT("int SetString_ContainsBeta()"),      TEXT("TSet<FString> should contain Beta"),                          1 },
			{ TEXT("int SetString_NotContainsMissing()"),TEXT("TSet<FString> should not contain Missing"),                   0 },
			{ TEXT("int SetString_RemoveContains()"),    TEXT("TSet<FString> Remove should drop membership"),                0 },
			{ TEXT("int SetBool_AddNum()"),              TEXT("TSet<bool> should dedup {true,false,true} to Num=2"),         2 },
			{ TEXT("int SetBool_ContainsTrue()"),        TEXT("TSet<bool> Contains(true) should be true"),                   1 },
			{ TEXT("int SetBool_NotContainsFalse()"),    TEXT("TSet<bool> Contains(false) should be false"),                 0 },
			{ TEXT("int SetFloat_ContainsLow()"),        TEXT("TSet<float> should contain 1.5"),                             1 },
			{ TEXT("int SetFloat_ContainsHigh()"),       TEXT("TSet<float> should contain 2.5"),                             1 },
			{ TEXT("int SetFloat_Num()"),                TEXT("TSet<float> Num should be 2"),                                2 },
			{ TEXT("int SetEnum_ContainsPre()"),         TEXT("TSet<ETickingGroup> should contain TG_PrePhysics"),           1 },
			{ TEXT("int SetEnum_NotContainsDuring()"),   TEXT("TSet<ETickingGroup> should not contain TG_DuringPhysics"),     0 },
			{ TEXT("int SetEnum_Num()"),                 TEXT("TSet<ETickingGroup> dedup should yield Num=2"),               2 },
			{ TEXT("int SetObject_NullDedup()"),         TEXT("TSet<UObject> with two nulls should dedup to Num=1"),         1 },
			{ TEXT("int SetInt_LargeBatchNum()"),        TEXT("TSet<int> with 0..9 added twice should contain Num=10"),      10 },
			{ TEXT("int SetVector_AddDedupNum()"),       TEXT("TSet<FVector> Add duplicate should dedup to Num=2"),          2 },
			{ TEXT("int SetVector_ContainsFirst()"),     TEXT("TSet<FVector> should contain (1,2,3)"),                       1 },
			{ TEXT("int SetVector_ContainsSecond()"),    TEXT("TSet<FVector> should contain (4,5,6)"),                       1 },
			{ TEXT("int SetVector_NotContainsMissing()"),TEXT("TSet<FVector> should not contain (9,9,9)"),                   0 },
			{ TEXT("int SetVector_RemoveReturns()"),     TEXT("TSet<FVector> Remove should return true"),                     1 },
			{ TEXT("int SetVector_RemoveNum()"),         TEXT("TSet<FVector> Num should drop to 1 after Remove"),             1 },
			{ TEXT("int SetVector_RemoveContains()"),    TEXT("TSet<FVector> should not contain removed vector"),             0 },
		};
		return AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: SetApiCoverage
	//
	// Every public method on Bind_TSet.cpp that the baseline SetCompat did
	// not already touch: Append(TArray), Append(TSet), Empty(slack),
	// opAssign, opEquals (incl. order-independent), explicit Iterator().
	// -----------------------------------------------------------------------

	bool RunSetApiCoverageSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASSet_ApiCoverage"), TEXT(R"(
int SetApi_AppendArray_Num()
{
	TSet<int> S;
	S.Add(1);
	TArray<int> More;
	More.Add(2);
	More.Add(3);
	More.Add(1); // duplicate relative to S
	S.Append(More);
	return S.Num();
}
int SetApi_AppendArray_ContainsSeed()
{
	TSet<int> S;
	S.Add(1);
	TArray<int> More;
	More.Add(2);
	More.Add(3);
	S.Append(More);
	return S.Contains(1) ? 1 : 0;
}
int SetApi_AppendArray_ContainsTwo()
{
	TSet<int> S;
	S.Add(1);
	TArray<int> More;
	More.Add(2);
	More.Add(3);
	S.Append(More);
	return S.Contains(2) ? 1 : 0;
}
int SetApi_AppendArray_ContainsThree()
{
	TSet<int> S;
	S.Add(1);
	TArray<int> More;
	More.Add(2);
	More.Add(3);
	S.Append(More);
	return S.Contains(3) ? 1 : 0;
}
int SetApi_AppendSet_Num()
{
	TSet<int> A;
	A.Add(1);
	A.Add(2);
	TSet<int> B;
	B.Add(2);
	B.Add(3);
	A.Append(B);
	return A.Num();
}
int SetApi_OpAssign_Num()
{
	TSet<int> A;
	A.Add(1);
	A.Add(2);
	TSet<int> B;
	B.Add(99);
	B = A;
	return B.Num();
}
int SetApi_OpAssign_ContainsCopiedOne()
{
	TSet<int> A;
	A.Add(1);
	A.Add(2);
	TSet<int> B;
	B.Add(99);
	B = A;
	return B.Contains(1) ? 1 : 0;
}
int SetApi_OpAssign_ContainsCopiedTwo()
{
	TSet<int> A;
	A.Add(1);
	A.Add(2);
	TSet<int> B;
	B.Add(99);
	B = A;
	return B.Contains(2) ? 1 : 0;
}
int SetApi_OpAssign_DropsOldElement()
{
	TSet<int> A;
	A.Add(1);
	A.Add(2);
	TSet<int> B;
	B.Add(99);
	B = A;
	return B.Contains(99) ? 1 : 0;
}
int SetApi_OpEquals_Equal()
{
	TSet<int> A;
	A.Add(1);
	A.Add(2);
	TSet<int> B;
	B.Add(2);
	B.Add(1); // insertion order reversed — should still compare equal
	return (A == B) ? 1 : 0;
}
int SetApi_OpEquals_DifferentSize()
{
	TSet<int> A;
	A.Add(1);
	TSet<int> B;
	B.Add(1);
	B.Add(2);
	return (A == B) ? 1 : 0;
}
int SetApi_Empty_Slack_IsEmpty()
{
	TSet<int> S;
	S.Add(1);
	S.Add(2);
	S.Empty(16);
	return S.IsEmpty() ? 1 : 0;
}
int SetApi_Iterator_Sum()
{
	TSet<int> S;
	S.Add(2);
	S.Add(3);
	S.Add(5);
	int Sum = 0;
	foreach (int V : S)
		Sum += V;
	return Sum;
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int SetApi_AppendArray_Num()"),              TEXT("Append(TArray) with 1 duplicate should produce Num=3"), 3 },
			{ TEXT("int SetApi_AppendArray_ContainsSeed()"),     TEXT("Append(TArray) should preserve seed element 1"),         1 },
			{ TEXT("int SetApi_AppendArray_ContainsTwo()"),      TEXT("Append(TArray) should add element 2"),                   1 },
			{ TEXT("int SetApi_AppendArray_ContainsThree()"),    TEXT("Append(TArray) should add element 3"),                   1 },
			{ TEXT("int SetApi_AppendSet_Num()"),                TEXT("Append(TSet) union should give Num=3"),                  3 },
			{ TEXT("int SetApi_OpAssign_Num()"),                 TEXT("opAssign should give destination Num=2"),                2 },
			{ TEXT("int SetApi_OpAssign_ContainsCopiedOne()"),   TEXT("opAssign should copy element 1 into destination"),       1 },
			{ TEXT("int SetApi_OpAssign_ContainsCopiedTwo()"),   TEXT("opAssign should copy element 2 into destination"),       1 },
			{ TEXT("int SetApi_OpAssign_DropsOldElement()"),     TEXT("opAssign should drop destination's prior element 99"),   0 },
			{ TEXT("int SetApi_OpEquals_Equal()"),               TEXT("Two sets with same members in diff order should be =="), 1 },
			{ TEXT("int SetApi_OpEquals_DifferentSize()"),       TEXT("Different-sized sets should not compare equal"),         0 },
			{ TEXT("int SetApi_Empty_Slack_IsEmpty()"),          TEXT("Empty(16) should clear contents"),                       1 },
			{ TEXT("int SetApi_Iterator_Sum()"),                 TEXT("Iterator walk sum of {2,3,5} should be 10"),             10 },
		};
		return AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: SetReturnType — exercise non-int return type paths through
	// TSet operations. Tests bool/float/FString return types and returning
	// container types (TSet, TArray) from script functions.
	// -----------------------------------------------------------------------

	bool RunSetReturnTypeSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASSet_ReturnType"), TEXT(R"(
// Direct bool returns from TSet operations
bool SetRet_Bool_Contains()
{
	TSet<int> S;
	S.Add(42);
	return S.Contains(42);
}
bool SetRet_Bool_NotContains()
{
	TSet<int> S;
	S.Add(42);
	return S.Contains(99);
}
bool SetRet_Bool_Remove()
{
	TSet<int> S;
	S.Add(42);
	return S.Remove(42);
}
bool SetRet_Bool_IsEmpty()
{
	TSet<int> S;
	return S.IsEmpty();
}
bool SetRet_Bool_IsEmptyAfterAdd()
{
	TSet<int> S;
	S.Add(1);
	return S.IsEmpty();
}
bool SetRet_Bool_OpEquals()
{
	TSet<int> A;
	A.Add(1);
	A.Add(2);
	TSet<int> B;
	B.Add(2);
	B.Add(1);
	return A == B;
}

// Return a TSet from a function, verify in script
TSet<int> MakeSet123()
{
	TSet<int> S;
	S.Add(1);
	S.Add(2);
	S.Add(3);
	return S;
}
int SetRet_VerifySetReturn_Num()
{
	TSet<int> S = MakeSet123();
	return S.Num();
}
int SetRet_VerifySetReturn_ContainsOne()
{
	TSet<int> S = MakeSet123();
	return S.Contains(1) ? 1 : 0;
}
int SetRet_VerifySetReturn_ContainsThree()
{
	TSet<int> S = MakeSet123();
	return S.Contains(3) ? 1 : 0;
}

// Return a TArray from TSet iteration, verify in script
TArray<int> SetToSortedArray()
{
	TSet<int> S;
	S.Add(3);
	S.Add(1);
	S.Add(2);
	TArray<int> Result;
	foreach (int V : S)
		Result.Add(V);
	Result.Sort();
	return Result;
}
int SetRet_VerifyArrayReturn_Num()
{
	TArray<int> A = SetToSortedArray();
	return A.Num();
}
int SetRet_VerifyArrayReturn_First()
{
	TArray<int> A = SetToSortedArray();
	return A[0];
}
int SetRet_VerifyArrayReturn_Last()
{
	TArray<int> A = SetToSortedArray();
	return A[A.Num() - 1];
}

// Return TSet<FString> from function
TSet<FString> MakeStringSet()
{
	TSet<FString> S;
	S.Add("Alpha");
	S.Add("Beta");
	S.Add("Gamma");
	return S;
}
int SetRet_VerifyStringSet_Num()
{
	TSet<FString> S = MakeStringSet();
	return S.Num();
}
int SetRet_VerifyStringSet_ContainsAlpha()
{
	TSet<FString> S = MakeStringSet();
	return S.Contains("Alpha") ? 1 : 0;
}
int SetRet_VerifyStringSet_ContainsGamma()
{
	TSet<FString> S = MakeStringSet();
	return S.Contains("Gamma") ? 1 : 0;
}
int SetRet_VerifyStringSet_NotContainsMissing()
{
	TSet<FString> S = MakeStringSet();
	return S.Contains("Missing") ? 1 : 0;
}

// Return TSet<FVector> from function
TSet<FVector> MakeVectorSet()
{
	TSet<FVector> S;
	S.Add(FVector(1, 0, 0));
	S.Add(FVector(0, 1, 0));
	return S;
}
int SetRet_VerifyVectorSet_Num()
{
	TSet<FVector> S = MakeVectorSet();
	return S.Num();
}
int SetRet_VerifyVectorSet_ContainsFirst()
{
	TSet<FVector> S = MakeVectorSet();
	return S.Contains(FVector(1, 0, 0)) ? 1 : 0;
}

// Return TArray<FString> built from TSet
TArray<FString> SetToStringArray()
{
	TSet<FString> S;
	S.Add("X");
	S.Add("Y");
	TArray<FString> Result;
	foreach (FString V : S)
		Result.Add(V);
	return Result;
}
int SetRet_VerifyStringArray_Num()
{
	TArray<FString> A = SetToStringArray();
	return A.Num();
}
)"));
		if (!ModuleScope.IsValid())
		{
			return false;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		bool bPassed = true;

		// Direct bool return assertions
		bPassed &= AngelscriptTestBindings::ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool SetRet_Bool_Contains()"), TEXT("bool return: Contains existing should be true"), true);
		bPassed &= AngelscriptTestBindings::ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool SetRet_Bool_NotContains()"), TEXT("bool return: Contains missing should be false"), false);
		bPassed &= AngelscriptTestBindings::ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool SetRet_Bool_Remove()"), TEXT("bool return: Remove existing should be true"), true);
		bPassed &= AngelscriptTestBindings::ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool SetRet_Bool_IsEmpty()"), TEXT("bool return: empty set IsEmpty should be true"), true);
		bPassed &= AngelscriptTestBindings::ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool SetRet_Bool_IsEmptyAfterAdd()"), TEXT("bool return: non-empty set IsEmpty should be false"), false);
		bPassed &= AngelscriptTestBindings::ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool SetRet_Bool_OpEquals()"), TEXT("bool return: equal sets opEquals should be true"), true);

		// Container return verified via script-side int wrappers
		const FExpectedGlobalInt IntCases[] = {
			{ TEXT("int SetRet_VerifySetReturn_Num()"),              TEXT("TSet<int> return: Num should be 3"),                    3 },
			{ TEXT("int SetRet_VerifySetReturn_ContainsOne()"),      TEXT("TSet<int> return: should contain 1"),                   1 },
			{ TEXT("int SetRet_VerifySetReturn_ContainsThree()"),    TEXT("TSet<int> return: should contain 3"),                   1 },
			{ TEXT("int SetRet_VerifyArrayReturn_Num()"),            TEXT("TArray<int> from TSet return: Num should be 3"),        3 },
			{ TEXT("int SetRet_VerifyArrayReturn_First()"),          TEXT("TArray<int> from TSet return: sorted first should be 1"),1 },
			{ TEXT("int SetRet_VerifyArrayReturn_Last()"),           TEXT("TArray<int> from TSet return: sorted last should be 3"),3 },
			{ TEXT("int SetRet_VerifyStringSet_Num()"),              TEXT("TSet<FString> return: Num should be 3"),                3 },
			{ TEXT("int SetRet_VerifyStringSet_ContainsAlpha()"),    TEXT("TSet<FString> return: should contain Alpha"),           1 },
			{ TEXT("int SetRet_VerifyStringSet_ContainsGamma()"),    TEXT("TSet<FString> return: should contain Gamma"),           1 },
			{ TEXT("int SetRet_VerifyStringSet_NotContainsMissing()"),TEXT("TSet<FString> return: should not contain Missing"),    0 },
			{ TEXT("int SetRet_VerifyVectorSet_Num()"),              TEXT("TSet<FVector> return: Num should be 2"),                2 },
			{ TEXT("int SetRet_VerifyVectorSet_ContainsFirst()"),    TEXT("TSet<FVector> return: should contain (1,0,0)"),         1 },
			{ TEXT("int SetRet_VerifyStringArray_Num()"),            TEXT("TArray<FString> from TSet return: Num should be 2"),    2 },
		};
		bPassed &= AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  IntCases);

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: SetLogDiagnostic — Log() different TSet element types.
	// -----------------------------------------------------------------------

	bool RunSetLogDiagnosticSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASSet_LogDiag"), TEXT(R"(
int SetLog_Types()
{
	TSet<int> SInt;
	SInt.Add(1); SInt.Add(2); SInt.Add(3);
	Log("SetLog TSet<int> Num: " + SInt.Num());

	TSet<FString> SStr;
	SStr.Add("Alpha"); SStr.Add("Beta");
	Log("SetLog TSet<FString> Num: " + SStr.Num());

	TSet<FName> SName;
	SName.Add(FName("A")); SName.Add(FName("B"));
	Log("SetLog TSet<FName> Num: " + SName.Num());

	TSet<float> SFloat;
	SFloat.Add(1.5f); SFloat.Add(2.5f);
	Log("SetLog TSet<float> Num: " + SFloat.Num());

	TSet<FVector> SVec;
	SVec.Add(FVector(1,0,0)); SVec.Add(FVector(0,1,0));
	Log("SetLog TSet<FVector> Num: " + SVec.Num());

	// Log each element via foreach
	foreach (int V : SInt)
		Log("SetLog TSet<int> element: " + V);
	foreach (FString V : SStr)
		Log("SetLog TSet<FString> element: " + V);
	foreach (FVector V : SVec)
		Log("SetLog TSet<FVector> element: " + V);

	return 1;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		return AngelscriptTestBindings::ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int SetLog_Types()"),
			TEXT("Log diagnostic: TSet types should compile and log without crash"), 1);
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSetBindingsTest,
	"Angelscript.TestModule.Bindings.Container.Set",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(SetCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		RunSetSection(*TestRunner, Engine);
		RunSetTypeMatrixSection(*TestRunner, Engine);
		RunSetApiCoverageSection(*TestRunner, Engine);
		RunSetReturnTypeSection(*TestRunner, Engine);
		RunSetLogDiagnosticSection(*TestRunner, Engine);

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
