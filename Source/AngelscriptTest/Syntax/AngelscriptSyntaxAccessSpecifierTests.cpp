// ============================================================================
// AngelscriptSyntaxAccessSpecifierTests.cpp
//
// Syntax coverage tests for access specifiers (public/private/protected) and
// scope rules in AngelScript classes — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.AccessSpecifier.*
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

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxAccessSpecifierTest,
	"Angelscript.TestModule.Syntax.AccessSpecifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Access Specifiers — Positive
	// ====================================================================

	TEST_METHOD(Access_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("AccPos_PublicDefault"),
			TEXT(R"(
class AActorPubDefault : AActor
{
	int PublicVar = 0;
	void PublicFunc() { }
}
)"),
			TEXT("Public members by default"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("AccPos_PrivateDecl"),
			TEXT(R"(
class AActorPrivDecl : AActor
{
	private int SecretVal = 42;
	private void SecretFunc() { SecretVal = 10; }
}
)"),
			TEXT("Private member declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("AccPos_ProtectedDecl"),
			TEXT(R"(
class AActorProtDecl : AActor
{
	protected int ProtectedVal = 0;
	protected void ProtectedFunc() { }
}
)"),
			TEXT("Protected member declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("AccPos_SelfPrivate"),
			TEXT(R"(
class AActorSelfPriv : AActor
{
	private int X = 0;

	void SetX(int Val) { X = Val; }
	int GetX() { return X; }
}
)"),
			TEXT("Accessing own private member from within class"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("AccPos_DerivedProtected"),
			TEXT(R"(
class ABaseActor : AActor
{
	protected int BaseVal = 10;
}

class ADerivedActor : ABaseActor
{
	void UseBase() { BaseVal = 20; }
}
)"),
			TEXT("Derived class accessing protected member"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("AccPos_MixedLevels"),
			TEXT(R"(
class AActorMixedLevels : AActor
{
	int PubA = 0;
	private int PrivB = 1;
	protected int ProtC = 2;

	void PubMethod() { PrivB = ProtC; }
}
)"),
			TEXT("Mixed access levels in class"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("AccPos_UPROPERTY"),
			TEXT(R"(
class AActorUPropPriv : AActor
{
	UPROPERTY()
	private int Health = 100;
}
)"),
			TEXT("UPROPERTY with private access"));
	}

	// ====================================================================
	// Access Specifiers — Negative
	// ====================================================================

	TEST_METHOD(Access_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_PrivateRead"),
			TEXT(R"(
class AActorPrivRead : AActor
{
	private int Secret = 42;
}

void Test()
{
	AActorPrivRead A;
	int X = A.Secret;
}
)"),
			TEXT("Reading private member from outside"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_PrivateMethod"),
			TEXT(R"(
class AActorPrivMethod : AActor
{
	private void SecretMethod() { }
}

void Test()
{
	AActorPrivMethod A;
	A.SecretMethod();
}
)"),
			TEXT("Calling private method from outside"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_ProtectedOutside"),
			TEXT(R"(
class AActorProtOut : AActor
{
	protected int ProtVal = 10;
}

void Test()
{
	AActorProtOut A;
	int X = A.ProtVal;
}
)"),
			TEXT("Accessing protected member from outside"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_ProtectedUnrelated"),
			TEXT(R"(
class AActorProtUnrel : AActor
{
	protected int ProtVal = 10;
}

class AOtherActor : AActor
{
	void Foo()
	{
		AActorProtUnrel A;
		int X = A.ProtVal;
	}
}
)"),
			TEXT("Accessing protected from unrelated class"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_PrivateDerived"),
			TEXT(R"(
class ABaseActorPrivDeriv : AActor
{
	private int Secret = 42;
}

class ADerivedActorPriv : ABaseActorPrivDeriv
{
	void Foo() { Secret = 10; }
}
)"),
			TEXT("Accessing private member from derived class"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_InvalidKeyword"),
			TEXT(R"(
class AActorBadKeyword : AActor
{
	internal int X = 0;
}
)"),
			TEXT("Invalid access specifier keyword internal"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_GlobalScope"),
			TEXT(R"(
private int GlobalVar = 5;
)"),
			TEXT("Access specifier at global scope"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_PrivateWrite"),
			TEXT(R"(
class AActorPrivWrite : AActor
{
	private int X = 0;
}

void Test()
{
	AActorPrivWrite A;
	A.X = 5;
}
)"),
			TEXT("Writing to private member from outside"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_ProtectedMethodOutside"),
			TEXT(R"(
class AActorProtMethodOut : AActor
{
	protected void InternalMethod() { }
}

void Test()
{
	AActorProtMethodOut A;
	A.InternalMethod();
}
)"),
			TEXT("Calling protected method from outside"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_PrivateViaUnrelated"),
			TEXT(R"(
class AActorPrivViaUnrel : AActor
{
	private int Secret = 42;
}

class AOther : AActor
{
	void Foo()
	{
		AActorPrivViaUnrel A;
		int X = A.Secret;
	}
}
)"),
			TEXT("Reading private from unrelated class"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_ProtectedWrite"),
			TEXT(R"(
class AActorProtWrite : AActor
{
	protected int ProtVal = 0;
}

void Test()
{
	AActorProtWrite A;
	A.ProtVal = 99;
}
)"),
			TEXT("Writing to protected member from outside"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_PrivateStaticCall"),
			TEXT(R"(
class AActorPrivStatic : AActor
{
	private void Init() { }
}

void Test()
{
	AActorPrivStatic A;
	A.Init();
}
)"),
			TEXT("Calling private Init from outside"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("AccNeg_PrivateSibling"),
			TEXT(R"(
class ABase : AActor { }

class ASiblingA : ABase
{
	private int X = 1;
}

class ASiblingB : ABase
{
	void Foo()
	{
		ASiblingA A;
		int Y = A.X;
	}
}
)"),
			TEXT("Accessing private from sibling class"));
	}

	// ====================================================================
	// Scope Rules — Positive
	// ====================================================================

	TEST_METHOD(Scope_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("ScopePos_Shadow"),
			TEXT(R"(
void Test() { int X = 1; { int X = 2; } }
)"),
			TEXT("Variable shadowing in inner scope"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("ScopePos_BlockIsolation"),
			TEXT(R"(
void Test() { { int X = 1; } { int X = 2; } }
)"),
			TEXT("Same name in separate blocks"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine,
			TEXT("ScopePos_LoopVar"),
			TEXT(R"(
void Test()
{
	for (int I = 0; I < 5; ++I) { int X = I; }
	for (int I = 0; I < 3; ++I) { }
}
)"),
			TEXT("Loop variable scope isolation"));
	}

	// ====================================================================
	// Scope Rules — Negative
	// ====================================================================

	TEST_METHOD(Scope_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ScopeNeg_AfterBlock"),
			TEXT(R"(
void Test() { { int X = 1; } int Y = X; }
)"),
			TEXT("Access variable after block ends"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ScopeNeg_AfterLoop"),
			TEXT(R"(
void Test() { for (int I = 0; I < 5; ++I) { } int X = I; }
)"),
			TEXT("Access loop variable after loop"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ScopeNeg_UseBeforeDecl"),
			TEXT(R"(
void Test() { int Y = X; int X = 5; }
)"),
			TEXT("Use variable before declaration"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
