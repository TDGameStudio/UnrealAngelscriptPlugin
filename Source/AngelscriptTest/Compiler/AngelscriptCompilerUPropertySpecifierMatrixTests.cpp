#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UPropertySpecifierMatrixTest
{
	static const FName EditConstModule(TEXT("Tests.Compiler.EditConstSpecifier"));
	static const FName NotEditableModule(TEXT("Tests.Compiler.NotEditableSpecifier"));
	static const FName AdvancedDisplayModule(TEXT("Tests.Compiler.AdvancedDisplaySpecifier"));
	static const FName SaveGameModule(TEXT("Tests.Compiler.SaveGameSpecifier"));
	static const FName TransientModule(TEXT("Tests.Compiler.TransientSpecifier"));
	static const FName ConfigModule(TEXT("Tests.Compiler.ConfigSpecifier"));
}

// ============================================================================
// EditConst -> CPF_EditConst
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerUPropertySpecifierMatrixTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EditConstSpecifierSetsCPFEditConst)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UPropertySpecifierMatrixTest::EditConstModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UPropertySpecifierMatrixTest::EditConstModule,
			TEXT("Tests/Compiler/EditConstSpecifier.as"),
			TEXT(R"AS(
	UCLASS()
	class UEditConstTestObj : UObject
	{
		UPROPERTY(EditConst)
		int LockedValue;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("EditConst should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UEditConstTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		FProperty* Prop = GeneratedClass->FindPropertyByName(TEXT("LockedValue"));
		if (!this->Assert.IsNotNull(Prop, TEXT("Property should exist")))
			return;

		ASSERT_THAT(IsTrue(Prop->HasAnyPropertyFlags(CPF_EditConst), TEXT("EditConst should set CPF_EditConst")));

		}

	}

	TEST_METHOD(NotEditableSpecifierClearsCPFEdit)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UPropertySpecifierMatrixTest::NotEditableModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UPropertySpecifierMatrixTest::NotEditableModule,
			TEXT("Tests/Compiler/NotEditableSpecifier.as"),
			TEXT(R"AS(
	UCLASS()
	class UNotEditableTestObj : UObject
	{
		UPROPERTY(NotEditable)
		int HiddenValue;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("NotEditable should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UNotEditableTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		FProperty* Prop = GeneratedClass->FindPropertyByName(TEXT("HiddenValue"));
		if (!this->Assert.IsNotNull(Prop, TEXT("Property should exist")))
			return;

		ASSERT_THAT(IsFalse(Prop->HasAnyPropertyFlags(CPF_Edit), TEXT("NotEditable should clear CPF_Edit")));

		}

	}

	TEST_METHOD(AdvancedDisplaySpecifierSetsCPFAdvancedDisplay)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UPropertySpecifierMatrixTest::AdvancedDisplayModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UPropertySpecifierMatrixTest::AdvancedDisplayModule,
			TEXT("Tests/Compiler/AdvancedDisplaySpecifier.as"),
			TEXT(R"AS(
	UCLASS()
	class UAdvancedDisplayTestObj : UObject
	{
		UPROPERTY(AdvancedDisplay)
		int AdvancedProp;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("AdvancedDisplay should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UAdvancedDisplayTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		FProperty* Prop = GeneratedClass->FindPropertyByName(TEXT("AdvancedProp"));
		if (!this->Assert.IsNotNull(Prop, TEXT("Property should exist")))
			return;

		ASSERT_THAT(IsTrue(Prop->HasAnyPropertyFlags(CPF_AdvancedDisplay), TEXT("AdvancedDisplay should set CPF_AdvancedDisplay")));

		}

	}

	TEST_METHOD(SaveGameSpecifierSetsCPFSaveGame)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UPropertySpecifierMatrixTest::SaveGameModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UPropertySpecifierMatrixTest::SaveGameModule,
			TEXT("Tests/Compiler/SaveGameSpecifier.as"),
			TEXT(R"AS(
	UCLASS()
	class USaveGameTestObj : UObject
	{
		UPROPERTY(SaveGame)
		int SavedScore;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("SaveGame should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("USaveGameTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		FProperty* Prop = GeneratedClass->FindPropertyByName(TEXT("SavedScore"));
		if (!this->Assert.IsNotNull(Prop, TEXT("Property should exist")))
			return;

		ASSERT_THAT(IsTrue(Prop->HasAnyPropertyFlags(CPF_SaveGame), TEXT("SaveGame should set CPF_SaveGame")));

		}

	}

	TEST_METHOD(TransientSpecifierSetsCPFTransient)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UPropertySpecifierMatrixTest::TransientModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UPropertySpecifierMatrixTest::TransientModule,
			TEXT("Tests/Compiler/TransientSpecifier.as"),
			TEXT(R"AS(
	UCLASS()
	class UTransientTestObj : UObject
	{
		UPROPERTY(Transient)
		int CachedValue;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("Transient should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UTransientTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		FProperty* Prop = GeneratedClass->FindPropertyByName(TEXT("CachedValue"));
		if (!this->Assert.IsNotNull(Prop, TEXT("Property should exist")))
			return;

		ASSERT_THAT(IsTrue(Prop->HasAnyPropertyFlags(CPF_Transient), TEXT("Transient should set CPF_Transient")));

		}

	}

	TEST_METHOD(ConfigSpecifierSetsCPFConfig)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*UPropertySpecifierMatrixTest::ConfigModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			UPropertySpecifierMatrixTest::ConfigModule,
			TEXT("Tests/Compiler/ConfigSpecifier.as"),
			TEXT(R"AS(
	UCLASS(Config=Game)
	class UConfigTestObj : UObject
	{
		UPROPERTY(Config)
		int ConfigValue;
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("Config should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UConfigTestObj"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should exist")))
			return;

		FProperty* Prop = GeneratedClass->FindPropertyByName(TEXT("ConfigValue"));
		if (!this->Assert.IsNotNull(Prop, TEXT("Property should exist")))
			return;

		ASSERT_THAT(IsTrue(Prop->HasAnyPropertyFlags(CPF_Config), TEXT("Config should set CPF_Config")));

		}

	}

};

#endif
