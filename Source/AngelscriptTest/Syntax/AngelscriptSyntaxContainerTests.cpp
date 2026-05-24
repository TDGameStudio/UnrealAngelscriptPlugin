// ============================================================================
// AngelscriptSyntaxContainerTests.cpp
//
// Syntax coverage tests for container types: TArray, TMap, TSet, TOptional.
// Tests declaration, initialization, access patterns, and common operations
// — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.Container.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxContainerTest,
	"Angelscript.TestModule.Syntax.Container",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// TArray — Positive
	// ====================================================================

	TEST_METHOD(TArray_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrDecl"),
			TEXT(R"(
void Test() { TArray<int> Arr; }
)"),
			TEXT("TArray declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrAdd"),
			TEXT(R"(
void Test() { TArray<int> Arr; Arr.Add(1); Arr.Add(2); }
)"),
			TEXT("TArray Add"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrAccess"),
			TEXT(R"(
void Test() { TArray<int> Arr; Arr.Add(5); int X = Arr[0]; }
)"),
			TEXT("TArray index access"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrNum"),
			TEXT(R"(
void Test() { TArray<int> Arr; Arr.Add(1); int Count = Arr.Num(); }
)"),
			TEXT("TArray.Num()"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrRemoveAt"),
			TEXT(R"(
void Test()
{
	TArray<int> Arr;
	Arr.Add(1);
	Arr.Add(2);
	Arr.RemoveAt(0);
}
)"),
			TEXT("TArray RemoveAt"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrEmpty"),
			TEXT(R"(
void Test() { TArray<int> Arr; Arr.Add(1); Arr.Empty(); }
)"),
			TEXT("TArray Empty"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrStruct"),
			TEXT(R"(
void Test() { TArray<FVector> Vectors; Vectors.Add(FVector(1, 0, 0)); }
)"),
			TEXT("TArray of struct"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrString"),
			TEXT(R"(
void Test() { TArray<FString> Names; Names.Add("Hello"); }
)"),
			TEXT("TArray of FString"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_ArrContains"),
			TEXT(R"(
void Test() { TArray<int> Arr; Arr.Add(5); bool B = Arr.Contains(5); }
)"),
			TEXT("TArray.Contains"));
	}

	// ====================================================================
	// TArray — Negative
	// ====================================================================

	TEST_METHOD(TArray_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_ArrNoTemplate"),
			TEXT(R"(
void Test() { TArray Arr; }
)"),
			TEXT("TArray without template type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_ArrWrongTypeAdd"),
			TEXT(R"(
void Test() { TArray<int> Arr; Arr.Add("hello"); }
)"),
			TEXT("Adding wrong type to TArray should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_ArrBadType"),
			TEXT(R"(
void Test() { TArray<NonExistent> Arr; }
)"),
			TEXT("TArray with non-existent element type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_ArrNested"),
			TEXT(R"(
void Test() { TArray<TArray<int>> Arr; }
)"),
			TEXT("Nested TArray should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_ArrVoid"),
			TEXT(R"(
void Test() { TArray<void> Arr; }
)"),
			TEXT("TArray<void> should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_ArrIndexStr"),
			TEXT(R"(
void Test() { TArray<int> Arr; Arr.Add(1); int X = Arr["key"]; }
)"),
			TEXT("TArray index with string should fail"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 float 作为数组索引（隐式转换为 int）
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_ArrIndexFloat"),
			TEXT(R"(
void Test() { TArray<int> Arr; Arr.Add(1); int X = Arr[0.5f]; }
)"),
			TEXT("TArray index with float should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_ArrAssignWrong"),
			TEXT(R"(
void Test() { TArray<int> A; TArray<FString> B; A = B; }
)"),
			TEXT("TArray assignment with wrong element type should fail"));
	}

	// ====================================================================
	// TMap — Positive
	// ====================================================================

	TEST_METHOD(TMap_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_MapDecl"),
			TEXT(R"(
void Test() { TMap<FString, int> Map; }
)"),
			TEXT("TMap declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_MapAdd"),
			TEXT(R"(
void Test() { TMap<FString, int> Map; Map.Add("key", 42); }
)"),
			TEXT("TMap Add"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_MapAccess"),
			TEXT(R"(
void Test()
{
	TMap<FString, int> Map;
	Map.Add("key", 42);
	int Val = Map["key"];
}
)"),
			TEXT("TMap bracket access"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_MapContains"),
			TEXT(R"(
void Test()
{
	TMap<FString, int> Map;
	Map.Add("key", 1);
	bool B = Map.Contains("key");
}
)"),
			TEXT("TMap.Contains"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_MapNum"),
			TEXT(R"(
void Test() { TMap<FString, int> Map; int N = Map.Num(); }
)"),
			TEXT("TMap.Num()"));
	}

	// ====================================================================
	// TMap — Negative
	// ====================================================================

	TEST_METHOD(TMap_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_MapNoTemplate"),
			TEXT(R"(
void Test() { TMap Map; }
)"),
			TEXT("TMap without template parameters should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_MapOneParam"),
			TEXT(R"(
void Test() { TMap<int> Map; }
)"),
			TEXT("TMap with only one template param should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_MapWrongKey"),
			TEXT(R"(
void Test() { TMap<FString, int> Map; Map.Add(42, 1); }
)"),
			TEXT("Adding wrong key type to TMap should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_MapWrongVal"),
			TEXT(R"(
void Test() { TMap<FString, int> Map; Map.Add("key", "value"); }
)"),
			TEXT("Adding wrong value type to TMap should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_MapBracketWrong"),
			TEXT(R"(
void Test() { TMap<FString, int> Map; int X = Map[42]; }
)"),
			TEXT("TMap bracket access with wrong key type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_MapVoid"),
			TEXT(R"(
void Test() { TMap<FString, void> Map; }
)"),
			TEXT("TMap with void value type should fail"));
	}

	// ====================================================================
	// TSet — Positive
	// ====================================================================

	TEST_METHOD(TSet_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_SetDecl"),
			TEXT(R"(
void Test() { TSet<int> S; }
)"),
			TEXT("TSet declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_SetAdd"),
			TEXT(R"(
void Test() { TSet<int> S; S.Add(1); S.Add(2); }
)"),
			TEXT("TSet Add"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_SetContains"),
			TEXT(R"(
void Test() { TSet<FString> S; S.Add("hello"); bool B = S.Contains("hello"); }
)"),
			TEXT("TSet.Contains"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_SetRemove"),
			TEXT(R"(
void Test() { TSet<int> S; S.Add(1); S.Remove(1); }
)"),
			TEXT("TSet.Remove"));
	}

	// ====================================================================
	// TSet — Negative
	// ====================================================================

	TEST_METHOD(TSet_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_SetNoTemplate"),
			TEXT(R"(
void Test() { TSet S; }
)"),
			TEXT("TSet without template type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_SetWrongType"),
			TEXT(R"(
void Test() { TSet<int> S; S.Add("hello"); }
)"),
			TEXT("Adding wrong type to TSet should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_SetBadType"),
			TEXT(R"(
void Test() { TSet<NonExistent> S; }
)"),
			TEXT("TSet with non-existent type should fail"));
	}

	// ====================================================================
	// TOptional — Mixed (Positive + Negative)
	// ====================================================================

	TEST_METHOD(TOptional_Mixed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Positive: Declaration
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_OptDecl"),
			TEXT(R"(
void Test() { TOptional<int> Opt; }
)"),
			TEXT("TOptional declaration"));

		// Positive: Set value
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_OptSet"),
			TEXT(R"(
void Test() { TOptional<int> Opt; Opt = 42; }
)"),
			TEXT("TOptional set value"));

		// Positive: IsSet
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_OptIsSet"),
			TEXT(R"(
void Test() { TOptional<int> Opt; bool B = Opt.IsSet(); }
)"),
			TEXT("TOptional.IsSet()"));

		// Positive: GetValue
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxCon_OptGetValue"),
			TEXT(R"(
void Test() { TOptional<int> Opt; Opt = 5; int X = Opt.GetValue(); }
)"),
			TEXT("TOptional.GetValue()"));

		// Negative: No template param
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_OptNoTemplate"),
			TEXT(R"(
void Test() { TOptional Opt; }
)"),
			TEXT("TOptional without template type should fail"));

		// Negative: Wrong type assign
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxCon_OptWrongType"),
			TEXT(R"(
void Test() { TOptional<int> Opt; Opt = "hello"; }
)"),
			TEXT("Assigning wrong type to TOptional should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
