#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageLiteralAssetTests
// -----------------------------------------------------------------------------
// Coverage for the AngelScript `asset` keyword literal asset declaration from:
//
//   OpenSpec: test-coverage/coverage-matrix.md
//
// Axes covered here:
//   * asset declaration syntax: `asset MyAsset of UMyClass { }`
//   * property initialization within asset block
//   * asset getter function access
//   * single instance behavior (singleton pattern)
//   * asset materialization at compile time
//
// Note: This tests the `asset` keyword feature, not C++ literal string syntax.
// Advanced scenarios (hot reload, editor serialization, multiple coexistence)
// are covered in dedicated test suites (HotReload, ClassGenerator, Editor).
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageLiteralAssetTest,
	"Angelscript.TestModule.Coverage.LiteralAsset",
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

	// -------------------------------------------------------------------------
	// Basic asset declaration and property initialization
	// -------------------------------------------------------------------------
	TEST_METHOD(AssetDeclarationBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UBasicAssetCarrier : UObject
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				FString Name;
			}

			asset MyBasicAsset of UBasicAssetCarrier
			{
				Value = 42;
				Name = "TestAsset";
			}

			int GetAssetValue()
			{
				UBasicAssetCarrier Asset = GetMyBasicAsset();
				return Asset != null ? Asset.Value : -1;
			}

			int CheckAssetName()
			{
				UBasicAssetCarrier Asset = GetMyBasicAsset();
				return (Asset != null && Asset.Name == "TestAsset") ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLiteralAsset_Basics"), ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int GetAssetValue()"),
			TEXT("asset declaration should initialize Value property"), 42);
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int CheckAssetName()"),
			TEXT("asset declaration should initialize Name property"), 1);
	}

	// -------------------------------------------------------------------------
	// Asset getter returns same instance (singleton behavior)
	// -------------------------------------------------------------------------
	TEST_METHOD(AssetSingletonBehavior)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class USingletonAssetCarrier : UObject
			{
				UPROPERTY()
				int AccessCount = 0;
			}

			asset MySingletonAsset of USingletonAssetCarrier
			{
				AccessCount = 1;
			}

			int TestSingletonIdentity()
			{
				USingletonAssetCarrier First = GetMySingletonAsset();
				USingletonAssetCarrier Second = GetMySingletonAsset();

				// Should be the same object pointer
				if (First != Second)
					return 0;

				// Should have same access count (not re-initialized)
				if (First.AccessCount != 1)
					return 0;

				// Modify and verify both references see the change
				First.AccessCount = 99;
				return Second.AccessCount == 99 ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLiteralAsset_Singleton"), ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int TestSingletonIdentity()"),
			TEXT("asset getter should return the same instance every time"), 1);
	}

	// -------------------------------------------------------------------------
	// Asset materialization happens at compile time
	// -------------------------------------------------------------------------
	TEST_METHOD(AssetCompileTimeMaterialization)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		static const FName AssetName(TEXT("MyMaterializedAsset"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UMaterializedAssetCarrier : UObject
			{
				UPROPERTY()
				bool bInitialized = false;
			}

			asset MyMaterializedAsset of UMaterializedAssetCarrier
			{
				bInitialized = true;
			}
		)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLiteralAsset_Materialization"), ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		// Asset should exist in the assets package immediately after compile
		UObject* Asset = FindObject<UObject>(Engine.AssetsPackage, *AssetName.ToString());
		ASSERT_THAT(IsNotNull(Asset, TEXT("asset should be materialized in AssetsPackage after compile")));

		// Check that initialization block was executed
		FBoolProperty* InitProp = FindFProperty<FBoolProperty>(Asset->GetClass(), TEXT("bInitialized"));
		ASSERT_THAT(IsNotNull(InitProp, TEXT("asset class should have bInitialized property")));

		bool bInitValue = InitProp->GetPropertyValue_InContainer(Asset);
		ASSERT_THAT(IsTrue(bInitValue, TEXT("asset initialization block should have executed at compile time")));
	}

	// -------------------------------------------------------------------------
	// Empty asset declaration (no initialization block)
	// -------------------------------------------------------------------------
	TEST_METHOD(AssetEmptyDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UEmptyAssetCarrier : UObject
			{
				UPROPERTY()
				int DefaultValue = 123;
			}

			asset MyEmptyAsset of UEmptyAssetCarrier
			{
			}

			int GetEmptyAssetDefaultValue()
			{
				UEmptyAssetCarrier Asset = GetMyEmptyAsset();
				return Asset != null ? Asset.DefaultValue : -1;
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLiteralAsset_Empty"), ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int GetEmptyAssetDefaultValue()"),
			TEXT("empty asset declaration should still use class default values"), 123);
	}

	// -------------------------------------------------------------------------
	// Asset with complex initialization expressions
	// -------------------------------------------------------------------------
	TEST_METHOD(AssetComplexInitialization)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UComplexAssetCarrier : UObject
			{
				UPROPERTY()
				int Sum = 0;

				UPROPERTY()
				bool Condition = false;

				UPROPERTY()
				FVector Position;
			}

			asset MyComplexAsset of UComplexAssetCarrier
			{
				Sum = 10 + 20 + 30;
				Condition = (Sum > 50);
				Position = FVector(1.0, 2.0, 3.0);
			}

			int TestComplexInitialization()
			{
				UComplexAssetCarrier Asset = GetMyComplexAsset();
				if (Asset == null)
					return 0;

				if (Asset.Sum != 60)
					return 0;

				if (!Asset.Condition)
					return 0;

				if (!Asset.Position.Equals(FVector(1.0, 2.0, 3.0), 0.001))
					return 0;

				return 1;
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLiteralAsset_Complex"), ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int TestComplexInitialization()"),
			TEXT("asset initialization should support complex expressions"), 1);
	}

	// -------------------------------------------------------------------------
	// Asset null check
	// -------------------------------------------------------------------------
	TEST_METHOD(AssetNullSafety)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UNullSafeAssetCarrier : UObject
			{
				UPROPERTY()
				int Value = 777;
			}

			asset MyNullSafeAsset of UNullSafeAssetCarrier
			{
			}

			int TestAssetNullCheck()
			{
				UNullSafeAssetCarrier Asset = GetMyNullSafeAsset();

				// Asset should never be null after successful compilation
				if (Asset == null)
					return 0;

				// Should be able to access properties
				if (Asset.Value != 777)
					return 0;

				return 1;
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLiteralAsset_NullSafety"), ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int TestAssetNullCheck()"),
			TEXT("asset getter should never return null for successfully compiled asset"), 1);
	}

	// -------------------------------------------------------------------------
	// Asset with incremental initialization
	// -------------------------------------------------------------------------
	TEST_METHOD(AssetIncrementalInitialization)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UIncrementalAssetCarrier : UObject
			{
				UPROPERTY()
				int Counter = 0;
			}

			asset MyIncrementalAsset of UIncrementalAssetCarrier
			{
				Counter += 10;
				Counter += 20;
				Counter += 30;
			}

			int GetIncrementalCounter()
			{
				UIncrementalAssetCarrier Asset = GetMyIncrementalAsset();
				return Asset != null ? Asset.Counter : -1;
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLiteralAsset_Incremental"), ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int GetIncrementalCounter()"),
			TEXT("asset initialization should support incremental operations"), 60);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
