// ============================================================================
// AngelscriptSyntaxCastingTests.cpp
//
// Syntax coverage tests for type casting and conversions in AngelScript.
// Tests Cast<T>, implicit/explicit conversions, numeric type promotions,
// and nullptr handling — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.Casting.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxCastingTest,
	"Angelscript.TestModule.Syntax.Casting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Cast<T> — Positive
	// ====================================================================

	TEST_METHOD(Cast_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("CastP_ToParent"),
			TEXT(R"(
void Test(APawn P) { AActor A = Cast<AActor>(P); }
)"),
			TEXT("Cast to parent class"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("CastP_Downcast"),
			TEXT(R"(
void Test(AActor A) { APawn P = Cast<APawn>(A); }
)"),
			TEXT("Downcast with Cast<T>"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("CastP_NullCheck"),
			TEXT(R"(
void Test(AActor A)
{
	APawn P = Cast<APawn>(A);
	if (P != nullptr) { }
}
)"),
			TEXT("Cast with null check"));
	}

	// ====================================================================
	// Cast<T> — Negative
	// ====================================================================

	TEST_METHOD(Cast_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_BadType"),
			TEXT(R"(
void Test(AActor A) { auto X = Cast<NonExistentClass>(A); }
)"),
			TEXT("Cast to non-existent type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_NoTemplate"),
			TEXT(R"(
void Test(AActor A) { auto X = Cast(A); }
)"),
			TEXT("Cast without template argument should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_Primitive"),
			TEXT(R"(
void Test() { int X = 5; auto Y = Cast<float>(X); }
)"),
			TEXT("Cast on primitive type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_NoArgs"),
			TEXT(R"(
void Test() { auto X = Cast<AActor>(); }
)"),
			TEXT("Cast without argument should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_Unrelated"),
			TEXT(R"(
void Test() { FString S = "hello"; auto X = Cast<AActor>(S); }
)"),
			TEXT("Cast between unrelated types should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_ToStruct"),
			TEXT(R"(
void Test(AActor A) { auto X = Cast<FVector>(A); }
)"),
			TEXT("Cast to struct type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_MultiTemplate"),
			TEXT(R"(
void Test(AActor A) { auto X = Cast<APawn, AActor>(A); }
)"),
			TEXT("Cast with multiple template arguments should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_TooManyArgs"),
			TEXT(R"(
void Test(AActor A, AActor B) { auto X = Cast<APawn>(A, B); }
)"),
			TEXT("Cast with too many arguments should fail"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 Cast<T>(nullptr) 编译通过，不视为编译错误
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_FromNullLiteral"),
			TEXT(R"(
void Test() { auto X = Cast<APawn>(nullptr); }
)"),
			TEXT("Cast from nullptr literal should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_AsLvalue"),
			TEXT(R"(
void Test(AActor A) { Cast<APawn>(A) = nullptr; }
)"),
			TEXT("Cast result as lvalue should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("CastN_ToEnum"),
			TEXT(R"(
void Test(AActor A) { auto X = Cast<ENetRole>(A); }
)"),
			TEXT("Cast to enum type should fail"));
	}

	// ====================================================================
	// Implicit Conversions — Positive
	// ====================================================================

	TEST_METHOD(Implicit_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ImplP_IntToFloat"),
			TEXT(R"(
void Test() { int X = 5; float Y = X; }
)"),
			TEXT("Implicit int to float conversion"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ImplP_IntToInt64"),
			TEXT(R"(
void Test() { int X = 5; int64 Y = X; }
)"),
			TEXT("Implicit int to int64 widening"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ImplP_Uint8ToInt"),
			TEXT(R"(
void Test() { uint8 X = 5; int Y = X; }
)"),
			TEXT("Implicit uint8 to int widening"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ImplP_DerivedToBase"),
			TEXT(R"(
void TakeActor(AActor A) { }

void Test(APawn P)
{
	TakeActor(P);
}
)"),
			TEXT("Implicit derived to base conversion"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ImplP_LiteralToFloat"),
			TEXT(R"(
void Test() { float X = 5; }
)"),
			TEXT("Implicit integer literal to float"));
	}

	// ====================================================================
	// Implicit Conversions — Negative
	// ====================================================================

	TEST_METHOD(Implicit_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_StrToInt"),
			TEXT(R"(
void Test() { FString S = "5"; int X = S; }
)"),
			TEXT("Implicit string to int should fail"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 float→int 隐式缩窄转换
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_FloatToInt"),
			TEXT(R"(
void Test() { float X = 5.5f; int Y = X; }
)"),
			TEXT("Implicit float to int narrowing should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_BaseToDerived"),
			TEXT(R"(
void TakePawn(APawn P) { }

void Test(AActor A)
{
	TakePawn(A);
}
)"),
			TEXT("Implicit base to derived should fail"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 bool→int 隐式转换
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_BoolToInt"),
			TEXT(R"(
void Test() { bool B = true; int X = B; }
)"),
			TEXT("Implicit bool to int should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_IntToBool"),
			TEXT(R"(
void Test() { int X = 1; bool B = X; }
)"),
			TEXT("Implicit int to bool should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_StructToStruct"),
			TEXT(R"(
void Test() { FVector V = FVector(1,0,0); FRotator R = V; }
)"),
			TEXT("Implicit conversion between unrelated structs should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_ArrayToElement"),
			TEXT(R"(
void Test() { TArray<int> Arr; int X = Arr; }
)"),
			TEXT("Implicit array to element should fail"));

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 int64→int 隐式缩窄转换
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_Int64ToInt"),
			TEXT(R"(
void Test() { int64 X = 999999999999; int Y = X; }
)"),
			TEXT("Implicit int64 to int narrowing should fail"));
#endif

		// DISABLED(#as-engine-behavior): implicit-conversion-permissive — AS 允许 float→uint8 隐式缩窄转换
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ImplN_FloatToUint8"),
			TEXT(R"(
void Test() { float X = 3.14f; uint8 Y = X; }
)"),
			TEXT("Implicit float to uint8 narrowing should fail"));
#endif
	}

	// ====================================================================
	// Explicit Numeric Conversions — Positive
	// ====================================================================

	TEST_METHOD(Explicit_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ExplP_FloatToInt"),
			TEXT(R"(
void Test() { float X = 5.5f; int Y = int(X); }
)"),
			TEXT("Explicit float to int cast"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ExplP_IntToUint8"),
			TEXT(R"(
void Test() { int X = 300; uint8 Y = uint8(X); }
)"),
			TEXT("Explicit int to uint8 narrowing cast"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ExplP_IntToFloat"),
			TEXT(R"(
void Test() { int X = 5; float Y = float(X); }
)"),
			TEXT("Explicit int to float cast"));
	}

	// ====================================================================
	// Explicit Conversions — Negative
	// ====================================================================

	TEST_METHOD(Explicit_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ExplN_StrToInt"),
			TEXT(R"(
void Test() { FString S = "hello"; int X = int(S); }
)"),
			TEXT("Explicit string to int cast should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ExplN_BadArgs"),
			TEXT(R"(
void Test() { int X = int(1, 2, 3); }
)"),
			TEXT("Explicit cast with wrong number of args should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ExplN_ToVoid"),
			TEXT(R"(
void Test() { int X = 5; void(X); }
)"),
			TEXT("Cast to void should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ExplN_ObjectToInt"),
			TEXT(R"(
void Test(AActor A) { int X = int(A); }
)"),
			TEXT("Explicit object to int cast should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ExplN_BoolToString"),
			TEXT(R"(
void Test() { bool B = true; FString S = FString(B); }
)"),
			TEXT("Explicit bool to FString cast should fail"));
	}

	// ====================================================================
	// Nullptr — Mixed (Positive + Negative)
	// ====================================================================

	TEST_METHOD(Nullptr_Mixed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// --- Positive ---

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("NullP_Assign"),
			TEXT(R"(
void Test() { AActor A = nullptr; }
)"),
			TEXT("Assign nullptr to object reference"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("NullP_Compare"),
			TEXT(R"(
void Test(AActor A)
{
	if (A == nullptr) { }
	if (A != nullptr) { }
}
)"),
			TEXT("Compare with nullptr"));

		// --- Negative ---

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("NullN_ToInt"),
			TEXT(R"(
void Test() { int X = nullptr; }
)"),
			TEXT("Assign nullptr to primitive should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("NullN_ToStruct"),
			TEXT(R"(
void Test() { FVector V = nullptr; }
)"),
			TEXT("Assign nullptr to value type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("NullN_ToFloat"),
			TEXT(R"(
void Test() { float X = nullptr; }
)"),
			TEXT("Assign nullptr to float should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("NullN_ToBool"),
			TEXT(R"(
void Test() { bool B = nullptr; }
)"),
			TEXT("Assign nullptr to bool should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("NullN_Arithmetic"),
			TEXT(R"(
void Test() { int X = nullptr + 1; }
)"),
			TEXT("Arithmetic with nullptr should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
