#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestLegacyHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Validation_AngelscriptMacroValidationTests_Private
{
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptGlobalBindingsMacroValidationTest,
	"Angelscript.TestModule.Validation.GlobalBindingsMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSharedCleanMacroValidationTest,
	"Angelscript.TestModule.Validation.SharedCleanMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSharedFreshMacroValidationTest,
	"Angelscript.TestModule.Validation.SharedFreshMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptModuleCleanMacroValidationTest,
	"Angelscript.TestModule.Validation.ModuleCleanMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptGlobalBindingsMacroValidationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Validation_AngelscriptMacroValidationTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(
		Engine,
		"ASGlobalVariableCompatMacro",
		TEXT(R"(
int Entry()
{
	if (CollisionProfile::BlockAllDynamic.Compare(FName("BlockAllDynamic")) != 0)
		return 10;

	FComponentQueryParams FreshParams;
	if (FComponentQueryParams::DefaultComponentQueryParams.ShapeCollisionMask.Bits != FreshParams.ShapeCollisionMask.Bits)
		return 20;

	FGameplayTag EmptyTagCopy = FGameplayTag::EmptyTag;
	if (EmptyTagCopy.IsValid())
		return 30;
	if (!FGameplayTagContainer::EmptyContainer.IsEmpty())
		return 40;
	if (!FGameplayTagQuery::EmptyQuery.IsEmpty())
		return 50;

	return 1;
}
		)"),
		TEXT("int Entry()"),
		Result);

	bPassed = TestEqual(TEXT("Global variable compat operations via macro should preserve bound namespace globals and defaults"), Result, 1);

	}
	return bPassed;
}

bool FAngelscriptSharedCleanMacroValidationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Validation_AngelscriptMacroValidationTests_Private;
	bool bPassed = false;
	int32 Result = 0;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	ASTEST_COMPILE_RUN_INT(
		Engine,
		"ASSharedCleanMacroValidation",
		TEXT(R"(
int Entry()
{
	return 17;
}
		)"),
		TEXT("int Entry()"),
		Result);

	bPassed = TestEqual(TEXT("Shared clean lifecycle macro pair should compile and run"), Result, 17);
	}
	return bPassed;
}

bool FAngelscriptSharedFreshMacroValidationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Validation_AngelscriptMacroValidationTests_Private;
	bool bPassed = false;
	int32 Result = 0;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	ASTEST_COMPILE_RUN_INT(
		Engine,
		"ASSharedFreshMacroValidation",
		TEXT(R"(
int Entry()
{
	return 23;
}
		)"),
		TEXT("int Entry()"),
		Result);

	bPassed = TestEqual(TEXT("Shared fresh lifecycle macro pair should compile and run"), Result, 23);
	}
	return bPassed;
}

bool FAngelscriptModuleCleanMacroValidationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Validation_AngelscriptMacroValidationTests_Private;
	bool bPassed = false;
	int32 Result = 0;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	const int32 BaselineActiveModules = Engine.GetActiveModules().Num();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

	ASTEST_COMPILE_RUN_INT(
		Engine,
		"ASModuleCleanMacroValidation",
		TEXT(R"(
int Entry()
{
	return 31;
}
		)"),
		TEXT("int Entry()"),
		Result);

	bPassed = TestEqual(TEXT("Module clean lifecycle macro pair should compile and run"), Result, 31);
	}

	return bPassed && TestEqual(TEXT("Module clean lifecycle should discard its module delta"), Engine.GetActiveModules().Num(), BaselineActiveModules);
}

#endif
