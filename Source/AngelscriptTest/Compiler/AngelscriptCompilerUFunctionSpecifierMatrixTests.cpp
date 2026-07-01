#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace UFunctionSpecifierMatrixTest
{
	static const FName CallInEditorModule(TEXT("Tests.Compiler.CallInEditorSpecifier"));
	static const FName AuthorityOnlyModule(TEXT("Tests.Compiler.BlueprintAuthorityOnlySpecifier"));
	static const FName ExecModule(TEXT("Tests.Compiler.ExecSpecifier"));
}

// ============================================================================
// CallInEditor specifier
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerUFunctionSpecifierMatrixTests,
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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("CallInEditor specifier should compile")));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCallInEditorTestObj"));
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Class should be materialized")));

		UFunction* Func = GeneratedClass->FindFunctionByName(TEXT("EditorOnlyAction"));
		ASSERT_THAT(IsNotNull(Func, TEXT("Function should exist")));

		ASSERT_THAT(IsTrue(
			Func->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor function should have CallInEditor metadata")));

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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("BlueprintAuthorityOnly specifier should compile")));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UAuthorityOnlyTestObj"));
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Class should be materialized")));

		UFunction* Func = GeneratedClass->FindFunctionByName(TEXT("AuthorityAction"));
		ASSERT_THAT(IsNotNull(Func, TEXT("Function should exist")));

		ASSERT_THAT(IsTrue(
			Func->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly),
			TEXT("BlueprintAuthorityOnly should set FUNC_BlueprintAuthorityOnly")));

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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Exec specifier should compile")));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UExecTestObj"));
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Class should be materialized")));

		UFunction* Func = GeneratedClass->FindFunctionByName(TEXT("ConsoleCommand"));
		ASSERT_THAT(IsNotNull(Func, TEXT("Function should exist")));

		ASSERT_THAT(IsTrue(
			Func->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("Exec should set FUNC_Exec flag")));

		}

	}

};

#endif
