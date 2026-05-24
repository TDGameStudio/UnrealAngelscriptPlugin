// ============================================================================
// AngelscriptSyntaxSmartPointerTests.cpp
//
// Syntax coverage tests for smart pointer types: TSubclassOf,
// TWeakObjectPtr, TSoftObjectPtr — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.SmartPointer.*
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

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxSmartPointerTest,
	"Angelscript.TestModule.Syntax.SmartPointer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// TSubclassOf — Positive
	// ====================================================================

	TEST_METHOD(TSubclassOf_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPSubclassDecl"),
			TEXT(R"(
class AActorSPSubDecl : AActor
{
	UPROPERTY()
	TSubclassOf<AActor> ActorClass;
}
)"),
			TEXT("TSubclassOf declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPSubclassParam"),
			TEXT(R"(
void SpawnActor(TSubclassOf<AActor> Class) { }
)"),
			TEXT("TSubclassOf as function parameter"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPSubclassNullCheck"),
			TEXT(R"(
void Test(TSubclassOf<AActor> Class)
{
	if (Class.IsValid()) { }
}
)"),
			TEXT("TSubclassOf validity check"));
	}

	// ====================================================================
	// TSubclassOf — Negative
	// ====================================================================

	TEST_METHOD(TSubclassOf_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSubclassNoTemplate"),
			TEXT(R"(
void Test() { TSubclassOf Class; }
)"),
			TEXT("TSubclassOf without template param should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSubclassBadType"),
			TEXT(R"(
void Test() { TSubclassOf<NonExistentClass> Class; }
)"),
			TEXT("TSubclassOf of non-existent type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSubclassPrimitive"),
			TEXT(R"(
void Test() { TSubclassOf<int> Class; }
)"),
			TEXT("TSubclassOf of primitive should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSubclassStruct"),
			TEXT(R"(
void Test() { TSubclassOf<FVector> Class; }
)"),
			TEXT("TSubclassOf of struct should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSubclassNested"),
			TEXT(R"(
void Test() { TSubclassOf<TSubclassOf<AActor>> Class; }
)"),
			TEXT("Nested TSubclassOf should fail"));
	}

	// ====================================================================
	// TWeakObjectPtr — Positive
	// ====================================================================

	TEST_METHOD(TWeakObjectPtr_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPWeakDecl"),
			TEXT(R"(
class AActorSPWeakDecl : AActor
{
	UPROPERTY()
	TWeakObjectPtr<AActor> WeakRef;
}
)"),
			TEXT("TWeakObjectPtr declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPWeakIsValid"),
			TEXT(R"(
void Test(TWeakObjectPtr<AActor> Weak)
{
	if (Weak.IsValid()) { }
}
)"),
			TEXT("TWeakObjectPtr.IsValid()"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPWeakGet"),
			TEXT(R"(
void Test(TWeakObjectPtr<AActor> Weak) { AActor A = Weak.Get(); }
)"),
			TEXT("TWeakObjectPtr.Get()"));
	}

	// ====================================================================
	// TWeakObjectPtr — Negative
	// ====================================================================

	TEST_METHOD(TWeakObjectPtr_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPWeakNoTemplate"),
			TEXT(R"(
void Test() { TWeakObjectPtr Weak; }
)"),
			TEXT("TWeakObjectPtr without template should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPWeakNonUObj"),
			TEXT(R"(
void Test() { TWeakObjectPtr<FVector> Weak; }
)"),
			TEXT("TWeakObjectPtr of non-UObject type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPWeakPrimitive"),
			TEXT(R"(
void Test() { TWeakObjectPtr<int> Weak; }
)"),
			TEXT("TWeakObjectPtr of primitive should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPWeakBadType"),
			TEXT(R"(
void Test() { TWeakObjectPtr<NonExistentClass> Weak; }
)"),
			TEXT("TWeakObjectPtr of non-existent type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPWeakNested"),
			TEXT(R"(
void Test() { TWeakObjectPtr<TWeakObjectPtr<AActor>> Weak; }
)"),
			TEXT("Nested TWeakObjectPtr should fail"));
	}

	// ====================================================================
	// TSoftObjectPtr — Positive
	// ====================================================================

	TEST_METHOD(TSoftObjectPtr_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPSoftDecl"),
			TEXT(R"(
class AActorSPSoftDecl : AActor
{
	UPROPERTY()
	TSoftObjectPtr<UStaticMesh> MeshAsset;
}
)"),
			TEXT("TSoftObjectPtr declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPSoftIsValid"),
			TEXT(R"(
void Test(TSoftObjectPtr<UStaticMesh> Soft) { bool B = Soft.IsValid(); }
)"),
			TEXT("TSoftObjectPtr.IsValid()"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxSPSoftGet"),
			TEXT(R"(
void Test(TSoftObjectPtr<UStaticMesh> Soft) { UStaticMesh Mesh = Soft.Get(); }
)"),
			TEXT("TSoftObjectPtr.Get()"));
	}

	// ====================================================================
	// TSoftObjectPtr — Negative
	// ====================================================================

	TEST_METHOD(TSoftObjectPtr_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSoftNoTemplate"),
			TEXT(R"(
void Test() { TSoftObjectPtr Soft; }
)"),
			TEXT("TSoftObjectPtr without template should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSoftNonUObj"),
			TEXT(R"(
void Test() { TSoftObjectPtr<FVector> Soft; }
)"),
			TEXT("TSoftObjectPtr of non-UObject should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSoftPrimitive"),
			TEXT(R"(
void Test() { TSoftObjectPtr<int> Soft; }
)"),
			TEXT("TSoftObjectPtr of primitive should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSoftBadType"),
			TEXT(R"(
void Test() { TSoftObjectPtr<NonExistentClass> Soft; }
)"),
			TEXT("TSoftObjectPtr of non-existent type should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxSPSoftNested"),
			TEXT(R"(
void Test() { TSoftObjectPtr<TSoftObjectPtr<UStaticMesh>> Soft; }
)"),
			TEXT("Nested TSoftObjectPtr should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
