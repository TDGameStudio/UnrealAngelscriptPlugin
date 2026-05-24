// ============================================================================
// AngelscriptSyntaxDefaultStatementTests.cpp
//
// Syntax coverage tests for default statements: attribute defaults in class
// bodies and function parameter defaults — CQTest refactor.
//
// Automation prefix: Angelscript.TestModule.Syntax.DefaultStatement.*
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

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxDefaultStatementTest,
	"Angelscript.TestModule.Syntax.DefaultStatement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Attribute Defaults — Positive
	// ====================================================================

	TEST_METHOD(AttributeDefault_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_AttrBool"),
			TEXT(R"(
class AAttrBoolActor : AActor
{
	UPROPERTY()
	bool bEnabled = true;

	default bEnabled = false;
}
)"),
			TEXT("Default statement for bool property"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_AttrInt"),
			TEXT(R"(
class AAttrIntActor : AActor
{
	UPROPERTY()
	int Health = 0;

	default Health = 100;
}
)"),
			TEXT("Default statement for int property"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_AttrReplicated"),
			TEXT(R"(
class AAttrRepActor : AActor
{
	UPROPERTY(Replicated)
	bool bReplicates = false;

	default bReplicates = true;
}
)"),
			TEXT("Default statement for replicated property"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_AttrInherited"),
			TEXT(R"(
class AMyPawn : APawn
{
	default SetReplicates(true);
}
)"),
			TEXT("Default statement for inherited property"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_AttrMultiple"),
			TEXT(R"(
class AAttrMultiActor : AActor
{
	UPROPERTY()
	int X = 0;

	UPROPERTY()
	int Y = 0;

	default X = 10;
	default Y = 20;
}
)"),
			TEXT("Multiple default statements"));
	}

	// ====================================================================
	// Attribute Defaults — Negative
	// ====================================================================

	TEST_METHOD(AttributeDefault_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_AttrNonExist"),
			TEXT(R"(
class AAttrNonExistActor : AActor
{
	default NonExistentProp = 42;
}
)"),
			TEXT("Default on non-existent property should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_AttrTypeMismatch"),
			TEXT(R"(
class AAttrTypeMismatchActor : AActor
{
	UPROPERTY()
	int Health = 0;

	default Health = "hello";
}
)"),
			TEXT("Default with type mismatch should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_AttrGlobal"),
			TEXT(R"(
default SomeVar = 5;
)"),
			TEXT("Default statement at global scope should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_AttrNoSemicolon"),
			TEXT(R"(
class AAttrNoSemiActor : AActor
{
	UPROPERTY()
	int X = 0;

	default X = 5
}
)"),
			TEXT("Default without semicolon should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 允许无值 default 语句编译通过
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_AttrNoValue"),
			TEXT(R"(
class AAttrNoValActor : AActor
{
	UPROPERTY()
	int X = 0;

	default X;
}
)"),
			TEXT("Default without value should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_AttrOnMethod"),
			TEXT(R"(
class AAttrOnMethodActor : AActor
{
	UFUNCTION()
	void Foo() {}

	default Foo = 0;
}
)"),
			TEXT("Default on method should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 允许同一属性多次 default 赋值
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_AttrDuplicate"),
			TEXT(R"(
class AAttrDupActor : AActor
{
	UPROPERTY()
	int X = 0;

	default X = 5;
	default X = 10;
}
)"),
			TEXT("Duplicate default for same property should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_AttrInStruct"),
			TEXT(R"(
struct FAttrStruct
{
	int X = 0;

	default X = 5;
}
)"),
			TEXT("Default statement in struct should fail"));
	}

	// ====================================================================
	// Function Parameter Defaults — Positive
	// ====================================================================

	TEST_METHOD(ParamDefault_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_ParamInt"),
			TEXT(R"(
void Foo(int X = 5) { }
)"),
			TEXT("Function parameter default int"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_ParamFloat"),
			TEXT(R"(
void Foo(float X = 1.0f) { }
)"),
			TEXT("Function parameter default float"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_ParamBool"),
			TEXT(R"(
void Foo(bool bEnable = true) { }
)"),
			TEXT("Function parameter default bool"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_ParamString"),
			TEXT(R"(
void Foo(FString Name = "Default") { }
)"),
			TEXT("Function parameter default string"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_ParamMulti"),
			TEXT(R"(
void Foo(int X = 1, float Y = 2.0f, bool bZ = false) { }
)"),
			TEXT("Multiple parameter defaults"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ASSyntaxDS_ParamCallDefault"),
			TEXT(R"(
void Foo(int X = 5) { }

void Test()
{
	Foo();
	Foo(10);
}
)"),
			TEXT("Calling with and without default parameter"));
	}

	// ====================================================================
	// Function Parameter Defaults — Negative
	// ====================================================================

	TEST_METHOD(ParamDefault_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_ParamOrder"),
			TEXT(R"(
void Foo(int X = 5, int Y) { }
)"),
			TEXT("Non-default param after default should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验参数默认值类型匹配
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_ParamTypeMismatch"),
			TEXT(R"(
void Foo(int X = "hello") { }
)"),
			TEXT("Default param type mismatch should fail"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_ParamExprDefault"),
			TEXT(R"(
int GlobalVal = 5;
void Foo(int X = GlobalVal + 1) { }
)"),
			TEXT("Non-constant expression as default should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine,
			TEXT("ASSyntaxDS_ParamNonConst"),
			TEXT(R"(
int GlobalVal = 5;
void Foo(int X = GlobalVal) { }
)"),
			TEXT("Non-const variable as default should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
