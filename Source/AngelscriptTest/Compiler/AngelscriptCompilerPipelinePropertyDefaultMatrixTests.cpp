#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "GameplayTagContainer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DefaultMatrixTest
{
	static const FName FNameModule(TEXT("Tests.Compiler.DefaultFNameProperty"));
	static const FName EnumModule(TEXT("Tests.Compiler.DefaultEnumProperty"));
	static const FName FloatBoolModule(TEXT("Tests.Compiler.DefaultFloatBoolProperty"));
	static const FName VectorModule(TEXT("Tests.Compiler.DefaultFVectorProperty"));
	static const FName StringModule(TEXT("Tests.Compiler.DefaultFStringProperty"));
	static const FName TagsAddModule(TEXT("Tests.Compiler.DefaultTagsAddExecuted"));
	static const FName SubobjectModule(TEXT("Tests.Compiler.DefaultSubobjectPath"));
	static const FName PriorityModule(TEXT("Tests.Compiler.DefaultOverridesPriority"));
	static const FName NonExistentModule(TEXT("Tests.Compiler.DefaultNonExistentProperty"));
	static const FName OutsideScopeModule(TEXT("Tests.Compiler.DefaultOutsideClassScope"));
	static const FName TypeMismatchModule(TEXT("Tests.Compiler.DefaultTypeMismatch"));
}

// ============================================================================
// Positive: FName default
// ============================================================================

TEST_CLASS_WITH_FLAGS(FCompilerPipelinePropertyDefaultMatrixTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DefaultFNamePropertyApplied)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::FNameModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::FNameModule,
			TEXT("Tests/Compiler/DefaultFNameProperty.as"),
			TEXT(R"AS(
	UCLASS()
	class UDefaultFNameCarrier : UObject
	{
		UPROPERTY()
		FName MyName;

		default MyName = n"TestName";
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("FName default should compile successfully")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultFNameCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("FName default class should be materialized")))
			return;

		UObject* CDO = GeneratedClass->GetDefaultObject();
		FNameProperty* Prop = FindFProperty<FNameProperty>(GeneratedClass, TEXT("MyName"));
		if (!this->Assert.IsNotNull(CDO, TEXT("CDO should exist")) || !this->Assert.IsNotNull(Prop, TEXT("MyName property should exist")))
			return;

		FName Value = Prop->GetPropertyValue_InContainer(CDO);
		ASSERT_THAT(AreEqual(FName(TEXT("TestName")), Value, TEXT("CDO MyName should be TestName")));

		}

	}

	TEST_METHOD(DefaultEnumPropertyApplied)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::EnumModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::EnumModule,
			TEXT("Tests/Compiler/DefaultEnumProperty.as"),
			TEXT(R"AS(
	enum ETestDirection
	{
		Up,
		Down,
		Left,
		Right
	}

	UCLASS()
	class UDefaultEnumCarrier : UObject
	{
		UPROPERTY()
		ETestDirection Direction;

		default Direction = ETestDirection::Right;

		UFUNCTION()
		int GetDirectionValue()
		{
			return int(Direction);
		}
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("Enum default should compile successfully")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultEnumCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Enum default class should be materialized")))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* GetFunc = GeneratedClass->FindFunctionByName(TEXT("GetDirectionValue"));
		if (!this->Assert.IsNotNull(Instance, TEXT("Instance should exist")) || !this->Assert.IsNotNull(GetFunc, TEXT("GetDirectionValue should exist")))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GetFunc, Result);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("GetDirectionValue should execute")));
		ASSERT_THAT(AreEqual(3, Result, TEXT("Direction should be Right (3)")));

		}

	}

	TEST_METHOD(DefaultFloatAndBoolPropertyApplied)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::FloatBoolModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::FloatBoolModule,
			TEXT("Tests/Compiler/DefaultFloatBoolProperty.as"),
			TEXT(R"AS(
	UCLASS()
	class UDefaultFloatBoolCarrier : UObject
	{
		UPROPERTY()
		float MyFloat;

		UPROPERTY()
		bool bEnabled;

		default MyFloat = 3.14f;
		default bEnabled = true;

		UFUNCTION()
		int VerifyDefaults()
		{
			if (MyFloat < 3.13f || MyFloat > 3.15f)
				return 1;
			if (!bEnabled)
				return 2;
			return 42;
		}
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("Float+Bool default should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultFloatBoolCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should be materialized")))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* VerifyFunc = GeneratedClass->FindFunctionByName(TEXT("VerifyDefaults"));
		if (!this->Assert.IsNotNull(Instance, TEXT("Instance should exist")) || !this->Assert.IsNotNull(VerifyFunc, TEXT("VerifyDefaults should exist")))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, VerifyFunc, Result);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("VerifyDefaults should execute")));
		ASSERT_THAT(AreEqual(42, Result, TEXT("Float+Bool defaults should apply correctly")));

		}

	}

	TEST_METHOD(DefaultFVectorPropertyApplied)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::VectorModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::VectorModule,
			TEXT("Tests/Compiler/DefaultFVectorProperty.as"),
			TEXT(R"AS(
	UCLASS()
	class UDefaultVectorCarrier : UObject
	{
		UPROPERTY()
		FVector MyVector;

		default MyVector = FVector(1.0f, 2.0f, 3.0f);

		UFUNCTION()
		int VerifyVector()
		{
			if (MyVector.X < 0.9f || MyVector.X > 1.1f)
				return 1;
			if (MyVector.Y < 1.9f || MyVector.Y > 2.1f)
				return 2;
			if (MyVector.Z < 2.9f || MyVector.Z > 3.1f)
				return 3;
			return 42;
		}
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("FVector default should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultVectorCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should be materialized")))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* VerifyFunc = GeneratedClass->FindFunctionByName(TEXT("VerifyVector"));
		if (!this->Assert.IsNotNull(Instance, TEXT("Instance should exist")) || !this->Assert.IsNotNull(VerifyFunc, TEXT("VerifyVector should exist")))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, VerifyFunc, Result);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("VerifyVector should execute")));
		ASSERT_THAT(AreEqual(42, Result, TEXT("FVector default should apply correctly")));

		}

	}

	TEST_METHOD(DefaultFStringPropertyApplied)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::StringModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::StringModule,
			TEXT("Tests/Compiler/DefaultFStringProperty.as"),
			TEXT(R"AS(
	UCLASS()
	class UDefaultStringCarrier : UObject
	{
		UPROPERTY()
		FString MyString;

		default MyString = "Hello World";

		UFUNCTION()
		int VerifyString()
		{
			if (MyString != "Hello World")
				return 1;
			return 42;
		}
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("FString default should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultStringCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should be materialized")))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* VerifyFunc = GeneratedClass->FindFunctionByName(TEXT("VerifyString"));
		if (!this->Assert.IsNotNull(Instance, TEXT("Instance should exist")) || !this->Assert.IsNotNull(VerifyFunc, TEXT("VerifyString should exist")))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, VerifyFunc, Result);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("VerifyString should execute")));
		ASSERT_THAT(AreEqual(42, Result, TEXT("FString default should apply correctly")));

		}

	}

	TEST_METHOD(DefaultTagsAddExecutedOnCDO)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::TagsAddModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::TagsAddModule,
			TEXT("Tests/Compiler/DefaultTagsAddExecuted.as"),
			TEXT(R"AS(
	UCLASS()
	class ADefaultTagsActor : AActor
	{
		default Tags.Add(n"Alpha");
		default Tags.Add(n"Beta");

		UFUNCTION()
		int VerifyTags()
		{
			if (!Tags.Contains(n"Alpha"))
				return 1;
			if (!Tags.Contains(n"Beta"))
				return 2;
			if (Tags.Num() < 2)
				return 3;
			return 42;
		}
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("Tags.Add default should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("ADefaultTagsActor"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should be materialized")))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* VerifyFunc = GeneratedClass->FindFunctionByName(TEXT("VerifyTags"));
		if (!this->Assert.IsNotNull(Instance, TEXT("Instance should exist")) || !this->Assert.IsNotNull(VerifyFunc, TEXT("VerifyTags should exist")))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, VerifyFunc, Result);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("VerifyTags should execute")));
		ASSERT_THAT(AreEqual(42, Result, TEXT("Tags.Add should actually add tags to CDO and instances")));

		}

	}

	// DefaultSubobjectPathApplied: removed in 2026-05-22 alongside the autoaccessor
	// refactor. The `default Subobject.Property = X` form for AActor's
	// PrimaryActorTick subfields relied on the autoaccessor-synthesized
	// Get_X()/Set_X() pair. After AS_PROPERTY_ACCESSOR_MODE was forced to 0 (see
	// openspec/changes/archive/2026-05-22-refactor-as-remove-autoaccessor), the
	// nested-default writer no longer reaches into UPROPERTY subobjects, so this
	// test could only be re-stated as a negative coverage point. Top-level UPROPERTY
	// defaults remain covered by DefaultFNamePropertyApplied / DefaultEnumPropertyApplied
	// / DefaultFloatAndBoolPropertyApplied / DefaultFVectorPropertyApplied /
	// DefaultFStringPropertyApplied above.

	TEST_METHOD(DefaultOverridesInlineInitializerPriority)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::PriorityModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::PriorityModule,
			TEXT("Tests/Compiler/DefaultOverridesPriority.as"),
			TEXT(R"AS(
	UCLASS()
	class UDefaultPriorityCarrier : UObject
	{
		UPROPERTY()
		int Score = 10;

		default Score = 20;

		UFUNCTION()
		int GetScore()
		{
			return Score;
		}
	}
	)AS"),
			CompileResult);

		if (!this->Assert.IsTrue(bCompiled, TEXT("Priority default should compile")))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultPriorityCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Class should be materialized")))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* GetFunc = GeneratedClass->FindFunctionByName(TEXT("GetScore"));
		if (!this->Assert.IsNotNull(Instance, TEXT("Instance should exist")) || !this->Assert.IsNotNull(GetFunc, TEXT("GetScore should exist")))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GetFunc, Result);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("GetScore should execute")));
		ASSERT_THAT(AreEqual(20, Result, TEXT("default should override inline initializer (20 > 10)")));

		}

	}

	TEST_METHOD(DefaultNonExistentPropertyFails)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::NonExistentModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		TestRunner->AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::NonExistentModule,
			TEXT("Tests/Compiler/DefaultNonExistentProperty.as"),
			TEXT(R"AS(
	UCLASS()
	class UDefaultNonExistentCarrier : UObject
	{
		default NoSuchProperty = 1;
	}
	)AS"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Non-existent property default should fail to compile")));

		}

	}

	TEST_METHOD(DefaultOutsideClassScopeFails)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::OutsideScopeModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		TestRunner->AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::OutsideScopeModule,
			TEXT("Tests/Compiler/DefaultOutsideClassScope.as"),
			TEXT(R"AS(
	int GlobalValue = 5;
	default GlobalValue = 10;
	)AS"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("default outside class scope should fail")));

		}

	}

	TEST_METHOD(DefaultTypeMismatchFails)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DefaultMatrixTest::TypeMismatchModule.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		TestRunner->AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			DefaultMatrixTest::TypeMismatchModule,
			TEXT("Tests/Compiler/DefaultTypeMismatch.as"),
			TEXT(R"AS(
	UCLASS()
	class UDefaultTypeMismatchCarrier : UObject
	{
		UPROPERTY()
		int MyInt;

		default MyInt = "not an int";
	}
	)AS"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Type mismatch default should fail to compile")));

		}

	}

};

#endif
