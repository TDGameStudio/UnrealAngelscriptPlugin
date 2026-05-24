// ============================================================================
// AngelscriptSyntaxMixinTests.cpp
//
// Syntax coverage tests for Mixin declarations and usage — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.Mixin.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
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

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxMixinTest,
	"Angelscript.TestModule.Syntax.Mixin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Mixin — Positive
	// ====================================================================

	TEST_METHOD(Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 2.33 fork 不支持 mixin class 语法
#if 0
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMixBasicDecl"),
			TEXT(R"(
mixin class UHealthMixinBasic
{
	UPROPERTY()
	int Health = 100;

	void TakeDamage(int Amount)
	{
		Health -= Amount;
	}
}
)"),
			TEXT("Basic mixin declaration"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMixApply"),
			TEXT(R"(
mixin class UHealthMixinApply
{
	UPROPERTY()
	int Health = 100;
}

class AMixApplyActor : AActor
{
	mixin UHealthMixinApply;
}
)"),
			TEXT("Mixin applied to class"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMixMultiple"),
			TEXT(R"(
mixin class UHealthMixinMulti
{
	int Health = 100;
}

mixin class UMoveMixin
{
	float Speed = 5.0f;
}

class AMixMultiActor : AActor
{
	mixin UHealthMixinMulti;
	mixin UMoveMixin;
}
)"),
			TEXT("Multiple mixins on one class"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxMixMethod"),
			TEXT(R"(
mixin class UCombatMixin
{
	int Damage = 10;

	int GetDamage() { return Damage; }
}
)"),
			TEXT("Mixin with method"));
#endif
	}

	// ====================================================================
	// Mixin — Negative
	// ====================================================================

	TEST_METHOD(Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixNoPrefix"),
			TEXT(R"(
mixin class HealthMixin
{
	int Health = 100;
}
)"),
			TEXT("Mixin without U prefix should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixNonExist"),
			TEXT(R"(
class AMixNonExistActor : AActor
{
	mixin UNonExistentMixin;
}
)"),
			TEXT("Using non-existent mixin should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixOnStruct"),
			TEXT(R"(
mixin class UHealthMixinOnStruct
{
	int Health = 100;
}

struct FMixStruct
{
	mixin UHealthMixinOnStruct;
}
)"),
			TEXT("Mixin on struct should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixDuplicate"),
			TEXT(R"(
mixin class UHealthMixinDup
{
	int Health = 100;
}

class AMixDupActor : AActor
{
	mixin UHealthMixinDup;
	mixin UHealthMixinDup;
}
)"),
			TEXT("Duplicate mixin application should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixGlobal"),
			TEXT(R"(
mixin class UHealthMixinGlobal
{
	int Health = 100;
}

mixin UHealthMixinGlobal;
)"),
			TEXT("Mixin at global scope should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixNonMixin"),
			TEXT(R"(
class ABaseMixN : AActor { }

class AChildMixN : AActor
{
	mixin ABaseMixN;
}
)"),
			TEXT("Using non-mixin class as mixin should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixInheritMixin"),
			TEXT(R"(
mixin class UBaseMixin { int X = 0; }
mixin class UChildMixin : UBaseMixin { int Y = 0; }
)"),
			TEXT("Mixin inheriting mixin should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixUPropNonActor"),
			TEXT(R"(
mixin class UHealthMixinUPropN
{
	UPROPERTY()
	int Health = 100;
}

class FMyPlainClass
{
	mixin UHealthMixinUPropN;
}
)"),
			TEXT("Mixin with UPROPERTY on non-actor should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixCircular"),
			TEXT(R"(
mixin class UMixinA { mixin UMixinB; }
mixin class UMixinB { mixin UMixinA; }
)"),
			TEXT("Circular mixin reference should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASSyntaxMixTypo"),
			TEXT(R"(
mixn class UBadMixin { int X = 0; }
)"),
			TEXT("Mixin keyword typo should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
