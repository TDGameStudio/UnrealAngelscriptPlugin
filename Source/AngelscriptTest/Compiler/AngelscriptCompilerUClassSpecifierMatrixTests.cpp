#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UClassSpecifierMatrixTest
{
	static const FName AbstractModule(TEXT("Tests.Compiler.AbstractClassSpecifier"));
	static const FName BlueprintTypeModule(TEXT("Tests.Compiler.BlueprintTypeClassSpecifier"));
	static const FName DefaultToInstancedModule(TEXT("Tests.Compiler.DefaultToInstancedClassSpecifier"));
	static const FName DeprecatedModule(TEXT("Tests.Compiler.DeprecatedClassSpecifier"));
	static const FName HideCategoriesModule(TEXT("Tests.Compiler.HideCategoriesClassSpecifier"));
}

// ============================================================================
// Abstract -> CLASS_Abstract
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerUClassSpecifierMatrixTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AbstractClassSpecifierSetsFlag)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UClassSpecifierMatrixTest::AbstractModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UClassSpecifierMatrixTest::AbstractModule,
			TEXT("Tests/Compiler/AbstractClassSpecifier.as"),
			TEXT(R"AS(
	UCLASS(Abstract)
	class UAbstractTestObj : UObject
	{
		UPROPERTY()
		int Value;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("Abstract class specifier should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UAbstractTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		ASSERT_THAT(IsTrue(GeneratedClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Abstract should set CLASS_Abstract")));

		}

	}

	TEST_METHOD(BlueprintTypeClassSpecifierSetsMeta)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UClassSpecifierMatrixTest::BlueprintTypeModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UClassSpecifierMatrixTest::BlueprintTypeModule,
			TEXT("Tests/Compiler/BlueprintTypeClassSpecifier.as"),
			TEXT(R"AS(
	UCLASS(BlueprintType)
	class UBlueprintTypeTestObj : UObject
	{
		UPROPERTY()
		int Value;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("BlueprintType class specifier should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UBlueprintTypeTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		ASSERT_THAT(IsTrue(
			GeneratedClass->HasMetaData(TEXT("BlueprintType")),
			TEXT("BlueprintType should set metadata")));

		}

	}

	TEST_METHOD(DefaultToInstancedClassSpecifierSetsFlag)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UClassSpecifierMatrixTest::DefaultToInstancedModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UClassSpecifierMatrixTest::DefaultToInstancedModule,
			TEXT("Tests/Compiler/DefaultToInstancedClassSpecifier.as"),
			TEXT(R"AS(
	UCLASS(DefaultToInstanced)
	class UInstancedTestObj : UObject
	{
		UPROPERTY()
		int Value;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("DefaultToInstanced class specifier should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UInstancedTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		ASSERT_THAT(IsTrue(
			GeneratedClass->HasAnyClassFlags(CLASS_DefaultToInstanced),
			TEXT("DefaultToInstanced should set CLASS_DefaultToInstanced")));

		}

	}

	TEST_METHOD(DeprecatedClassSpecifierSetsFlag)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UClassSpecifierMatrixTest::DeprecatedModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UClassSpecifierMatrixTest::DeprecatedModule,
			TEXT("Tests/Compiler/DeprecatedClassSpecifier.as"),
			TEXT(R"AS(
	UCLASS(Deprecated)
	class UDeprecatedTestObj : UObject
	{
		UPROPERTY()
		int Value;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("Deprecated class specifier should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDeprecatedTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		ASSERT_THAT(IsTrue(
			GeneratedClass->HasAnyClassFlags(CLASS_Deprecated),
			TEXT("Deprecated should set CLASS_Deprecated")));

		}

	}

	TEST_METHOD(HideCategoriesClassSpecifierSetsMeta)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UClassSpecifierMatrixTest::HideCategoriesModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UClassSpecifierMatrixTest::HideCategoriesModule,
			TEXT("Tests/Compiler/HideCategoriesClassSpecifier.as"),
			TEXT(R"AS(
	UCLASS(HideCategories = "Rendering")
	class UHideCategoriesTestObj : UObject
	{
		UPROPERTY()
		int Value;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("HideCategories class specifier should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UHideCategoriesTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		ASSERT_THAT(IsTrue(
			GeneratedClass->HasMetaData(TEXT("HideCategories")),
			TEXT("HideCategories should set metadata")));

		}

	}

};

#endif
