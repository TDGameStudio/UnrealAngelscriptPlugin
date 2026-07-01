#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptMacroValidationTest,
	"Angelscript.TestModule.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InlineSourceMacro)
	{
		const FString Source = ASTEST_AS(R"AS(
			class AMacroInlineSourceActor : AActor
			{
				int Entry()
				{
					return 41;
				}
			}
		)AS");

		ASSERT_THAT(AreEqual(
			TEXT("class AMacroInlineSourceActor : AActor\n{\n\tint Entry()\n\t{\n\t\treturn 41;\n\t}\n}"),
			Source,
			TEXT("ASTEST_AS should be available from AngelscriptTestMacros.h and normalize visual indentation")));
	}

	TEST_METHOD(GlobalBindingsMacro)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
			}
		};

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASGlobalVariableCompatMacro"), TEXT(R"(
int Entry()
{
	if (CollisionProfile::BlockAllDynamic.Compare(FName("BlockAllDynamic")) != 0)
		return 10;

	FComponentQueryParams FreshParams;
	if (FComponentQueryParams::DefaultComponentQueryParams.ShapeCollisionMask.Bits != FreshParams.ShapeCollisionMask.Bits)
		return 20;

	return 1;
}
)"));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Global variable compat macro module should compile")));
		ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int Entry()"),
			TEXT("Global variable compat operations via macro should preserve core bound namespace globals and defaults"),
			1);
	}

	TEST_METHOD(SharedCleanMacro)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSharedCleanMacroValidation"), TEXT(R"(
int Entry()
{
	return 17;
}
)"));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Shared clean lifecycle macro module should compile")));
		ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int Entry()"),
			TEXT("Shared clean lifecycle macro pair should compile and run"),
			17);
	}

	TEST_METHOD(SharedFreshMacro)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSharedFreshMacroValidation"), TEXT(R"(
int Entry()
{
	return 23;
}
)"));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Shared fresh lifecycle macro module should compile")));
		ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int Entry()"),
			TEXT("Shared fresh lifecycle macro pair should compile and run"),
			23);
	}

	TEST_METHOD(ModuleCleanMacro)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		const int32 BaselineActiveModules = Engine.GetActiveModules().Num();
		{
			FAngelscriptEngineScope Scope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);
			FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASModuleCleanMacroValidation"), TEXT(R"(
int Entry()
{
	return 31;
}
)"));
			ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Module clean lifecycle macro module should compile")));
			ExpectGlobalInt(
				*TestRunner,
				Engine,
				ModuleScope.GetModule(),
				TEXT("int Entry()"),
				TEXT("Module clean lifecycle macro pair should compile and run"),
				31);
		}
		ASSERT_THAT(AreEqual(
			BaselineActiveModules,
			Engine.GetActiveModules().Num(),
			TEXT("Module clean lifecycle should discard its module delta")));
	}
};

#endif
