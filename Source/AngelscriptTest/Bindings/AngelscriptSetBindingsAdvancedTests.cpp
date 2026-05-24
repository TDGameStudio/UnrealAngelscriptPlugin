// ============================================================================
// AngelscriptSetBindingsAdvancedTests.cpp
//
// TSet advanced binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.SetAdvanced.FAngelscriptSetAdvancedBindingsTest.*
//
// Sections:
//   AppendFromArrayAndSet — Append from TArray (dedup) + Append from TSet (merge)
//   CopyIsolation         — Copy a set, modify copy, verify original unchanged
//   AssignmentAndEmpty    — Assignment replaces content, Empty clears independently
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


TEST_CLASS_WITH_FLAGS(FAngelscriptSetAdvancedBindingsTest,
	"Angelscript.TestModule.Bindings.SetAdvanced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: AppendFromArrayAndSet
	// ====================================================================

	TEST_METHOD(AppendFromArrayAndSet)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSetAdvanced_AppendArraySet"), TEXT(R"(
int AppendArray_Num()
{
	TArray<int> Arr;
	Arr.Add(4);
	Arr.Add(7);
	Arr.Add(7);
	TSet<int> S;
	S.Append(Arr);
	return S.Num();
}

int AppendArray_Contains4()
{
	TArray<int> Arr;
	Arr.Add(4);
	Arr.Add(7);
	TSet<int> S;
	S.Append(Arr);
	return S.Contains(4) ? 1 : 0;
}

int AppendArray_Contains7()
{
	TArray<int> Arr;
	Arr.Add(4);
	Arr.Add(7);
	TSet<int> S;
	S.Append(Arr);
	return S.Contains(7) ? 1 : 0;
}

int AppendSet_MergeNum()
{
	TArray<int> Arr;
	Arr.Add(4);
	Arr.Add(7);
	TSet<int> Combined;
	Combined.Append(Arr);

	TSet<int> Extra;
	Extra.Add(1);
	Extra.Add(4);
	Combined.Append(Extra);
	return Combined.Num();
}

int AppendSet_MergeContainsAll()
{
	TArray<int> Arr;
	Arr.Add(4);
	Arr.Add(7);
	TSet<int> Combined;
	Combined.Append(Arr);

	TSet<int> Extra;
	Extra.Add(1);
	Extra.Add(4);
	Combined.Append(Extra);
	return (Combined.Contains(1) && Combined.Contains(4) && Combined.Contains(7)) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int AppendArray_Num()"),
			TEXT("Append from TArray with duplicates should deduplicate"), 2);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int AppendArray_Contains4()"),
			TEXT("Appended set should contain 4"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int AppendArray_Contains7()"),
			TEXT("Appended set should contain 7"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int AppendSet_MergeNum()"),
			TEXT("Append from TSet should merge to 3 unique elements"), 3);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int AppendSet_MergeContainsAll()"),
			TEXT("Merged set should contain all three elements"), 1);
	}

	// ====================================================================
	// Section: CopyIsolation
	// ====================================================================

	TEST_METHOD(CopyIsolation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSetAdvanced_CopyIsolation"), TEXT(R"(
int Copy_AddToCopy()
{
	TSet<int> Orig;
	Orig.Add(1);
	Orig.Add(4);
	Orig.Add(7);
	TSet<int> Copy = Orig;
	Copy.Add(9);
	return Copy.Contains(9) ? 1 : 0;
}

int Copy_RemoveFromCopy()
{
	TSet<int> Orig;
	Orig.Add(1);
	Orig.Add(4);
	Orig.Add(7);
	TSet<int> Copy = Orig;
	Copy.Remove(1);
	return Copy.Contains(1) ? 1 : 0;
}

int Copy_OriginalUnchangedNum()
{
	TSet<int> Orig;
	Orig.Add(1);
	Orig.Add(4);
	Orig.Add(7);
	TSet<int> Copy = Orig;
	Copy.Add(9);
	Copy.Remove(1);
	return Orig.Num();
}

int Copy_OriginalUnchangedContent()
{
	TSet<int> Orig;
	Orig.Add(1);
	Orig.Add(4);
	Orig.Add(7);
	TSet<int> Copy = Orig;
	Copy.Add(9);
	Copy.Remove(1);
	return (Orig.Contains(1) && Orig.Contains(4) && Orig.Contains(7) && !Orig.Contains(9)) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Copy_AddToCopy()"),
			TEXT("Adding to copy should be visible in copy"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Copy_RemoveFromCopy()"),
			TEXT("Removing from copy should not leave element in copy"), 0);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Copy_OriginalUnchangedNum()"),
			TEXT("Original set Num should remain 3 after mutating copy"), 3);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Copy_OriginalUnchangedContent()"),
			TEXT("Original set content should be unchanged after mutating copy"), 1);
	}

	// ====================================================================
	// Section: AssignmentAndEmpty
	// ====================================================================

	TEST_METHOD(AssignmentAndEmpty)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSetAdvanced_AssignEmpty"), TEXT(R"(
int Assign_ReplacesNum()
{
	TSet<int> Source;
	Source.Add(1);
	Source.Add(4);
	Source.Add(7);
	TSet<int> Target;
	Target.Add(42);
	Target.Add(99);
	Target = Source;
	return Target.Num();
}

int Assign_ReplacesContent()
{
	TSet<int> Source;
	Source.Add(1);
	Source.Add(4);
	Source.Add(7);
	TSet<int> Target;
	Target.Add(42);
	Target.Add(99);
	Target = Source;
	return (Target.Contains(1) && Target.Contains(4) && Target.Contains(7)) ? 1 : 0;
}

int Assign_OldContentGone()
{
	TSet<int> Source;
	Source.Add(1);
	Source.Add(4);
	Source.Add(7);
	TSet<int> Target;
	Target.Add(42);
	Target.Add(99);
	Target = Source;
	return (!Target.Contains(42) && !Target.Contains(99)) ? 1 : 0;
}

int Empty_ClearsSet()
{
	TSet<int> Source;
	Source.Add(1);
	Source.Add(4);
	Source.Add(7);
	TSet<int> Target;
	Target.Add(42);
	Target.Add(99);
	Target = Source;
	Target.Empty(8);
	return Target.IsEmpty() ? 1 : 0;
}

int Empty_NoElementsLeft()
{
	TSet<int> Source;
	Source.Add(1);
	Source.Add(4);
	Source.Add(7);
	TSet<int> Target = Source;
	Target.Empty(8);
	return (!Target.Contains(1) && !Target.Contains(4) && !Target.Contains(7)) ? 1 : 0;
}

int Empty_SourceUnaffected()
{
	TSet<int> Source;
	Source.Add(1);
	Source.Add(4);
	Source.Add(7);
	TSet<int> Target = Source;
	Target.Empty(8);
	return (Source.Num() == 3 && Source.Contains(1) && Source.Contains(4) && Source.Contains(7)) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Assign_ReplacesNum()"),
			TEXT("Assignment should replace target with source count"), 3);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Assign_ReplacesContent()"),
			TEXT("Assignment should copy source content into target"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Assign_OldContentGone()"),
			TEXT("Assignment should discard previous target content"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Empty_ClearsSet()"),
			TEXT("Empty should clear the set"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Empty_NoElementsLeft()"),
			TEXT("Empty should leave no elements accessible"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int Empty_SourceUnaffected()"),
			TEXT("Empty on target should not affect source set"), 1);
	}
};

#endif
