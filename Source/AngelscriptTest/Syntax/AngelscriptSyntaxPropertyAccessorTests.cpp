// ============================================================================
// AngelscriptSyntaxPropertyAccessorTests.cpp
//
// Syntax coverage for the post-refactor accessor surface:
// explicit GetX/SetX methods are valid, and property-style access is rejected.
//
// Automation prefix: Angelscript.TestModule.Syntax.PropertyAccessor.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxPropertyAccessorTest,
	"Angelscript.TestModule.Syntax.PropertyAccessor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ExplicitMethodsCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxPAExplicitMethods"),
			TEXT(R"(
class AActorPAExplicit : AActor
{
	private int _Health = 100;

	int GetHealth() const
	{
		return _Health;
	}

	void SetHealth(int Value)
	{
		_Health = Value;
	}

	void Test()
	{
		SetHealth(5);
		int Current = GetHealth();
	}
}
)"),
			TEXT("Explicit getter/setter methods"));
	}

	TEST_METHOD(PropertyDecoratorFails)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Intentional removal probe: keep one legacy accessor form here so the
		// parser removal diagnostic stays covered by an automated test.
		SyntaxTestHelpers::AssertFailsWithError(*TestRunner, Engine, TEXT("ASSyntaxPAPropertyDecorator"),
			TEXT(R"(
class AActorPAProperty : AActor
{
	int GetHealth() property
	{
		return 100;
	}
}
)"),
			TEXT("The 'property' decorator has been removed"),
			TEXT("property decorator should be rejected"));
	}

	TEST_METHOD(VirtualPropertyFails)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Intentional removal probe for the removed virtual-property syntax.
		SyntaxTestHelpers::AssertFailsWithError(*TestRunner, Engine, TEXT("ASSyntaxPAVirtualProperty"),
			TEXT(R"(
class AActorPAVirtual : AActor
{
	int Health
	{
		get
		{
			return 100;
		}

		set
		{
		}
	}
}
)"),
			TEXT("Virtual property syntax has been removed"),
			TEXT("virtual property syntax should be rejected"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
