#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UFunctionSpecifierMatrixTest
{
	static const FName CallInEditorModule(TEXT("Tests.Compiler.CallInEditorSpecifier"));
	static const FName AuthorityOnlyModule(TEXT("Tests.Compiler.BlueprintAuthorityOnlySpecifier"));
	static const FName ExecModule(TEXT("Tests.Compiler.ExecSpecifier"));
}

// ============================================================================
// CallInEditor specifier
// ============================================================================

TEST_CLASS_WITH_FLAGS(FCompilerPipelineUFunctionSpecifierMatrixTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CallInEditorSpecifierSetsFlag)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UFunctionSpecifierMatrixTest::CallInEditorModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UFunctionSpecifierMatrixTest::CallInEditorModule,
			TEXT("Tests/Compiler/CallInEditorSpecifier.as"),
			TEXT(R"AS(
	UCLASS()
	class UCallInEditorTestObj : UObject
	{
		UFUNCTION(CallInEditor)
		void EditorOnlyAction()
		{
		}
	}
	)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("CallInEditor specifier should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCallInEditorTestObj"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UFunction* Func = GeneratedClass->FindFunctionByName(TEXT("EditorOnlyAction"));
		if (!TestRunner->TestNotNull(TEXT("Function should exist"), Func))
			return;

		TestRunner->TestTrue(TEXT("CallInEditor function should have CallInEditor metadata"),
			Func->HasMetaData(TEXT("CallInEditor")));

		}

	}

	TEST_METHOD(BlueprintAuthorityOnlySpecifierSetsFlag)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UFunctionSpecifierMatrixTest::AuthorityOnlyModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UFunctionSpecifierMatrixTest::AuthorityOnlyModule,
			TEXT("Tests/Compiler/BlueprintAuthorityOnlySpecifier.as"),
			TEXT(R"AS(
	UCLASS()
	class UAuthorityOnlyTestObj : UObject
	{
		UFUNCTION(BlueprintAuthorityOnly)
		void AuthorityAction()
		{
		}
	}
	)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("BlueprintAuthorityOnly specifier should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UAuthorityOnlyTestObj"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UFunction* Func = GeneratedClass->FindFunctionByName(TEXT("AuthorityAction"));
		if (!TestRunner->TestNotNull(TEXT("Function should exist"), Func))
			return;

		TestRunner->TestTrue(TEXT("BlueprintAuthorityOnly should set FUNC_BlueprintAuthorityOnly"),
			Func->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));

		}

	}

	TEST_METHOD(ExecSpecifierSetsFlag)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UFunctionSpecifierMatrixTest::ExecModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UFunctionSpecifierMatrixTest::ExecModule,
			TEXT("Tests/Compiler/ExecSpecifier.as"),
			TEXT(R"AS(
	UCLASS()
	class UExecTestObj : UObject
	{
		UFUNCTION(Exec)
		void ConsoleCommand()
		{
		}
	}
	)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Exec specifier should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UExecTestObj"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UFunction* Func = GeneratedClass->FindFunctionByName(TEXT("ConsoleCommand"));
		if (!TestRunner->TestNotNull(TEXT("Function should exist"), Func))
			return;

		TestRunner->TestTrue(TEXT("Exec should set FUNC_Exec flag"),
			Func->HasAnyFunctionFlags(FUNC_Exec));

		}

	}

};

#endif
