#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

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

		if (!TestRunner->TestTrue(TEXT("FName default should compile successfully"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultFNameCarrier"));
		if (!TestRunner->TestNotNull(TEXT("FName default class should be materialized"), GeneratedClass))
			return;

		UObject* CDO = GeneratedClass->GetDefaultObject();
		FNameProperty* Prop = FindFProperty<FNameProperty>(GeneratedClass, TEXT("MyName"));
		if (!TestRunner->TestNotNull(TEXT("CDO should exist"), CDO) || !TestRunner->TestNotNull(TEXT("MyName property should exist"), Prop))
			return;

		FName Value = Prop->GetPropertyValue_InContainer(CDO);
		TestRunner->TestEqual(TEXT("CDO MyName should be TestName"), Value, FName(TEXT("TestName")));

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

		if (!TestRunner->TestTrue(TEXT("Enum default should compile successfully"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultEnumCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Enum default class should be materialized"), GeneratedClass))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* GetFunc = GeneratedClass->FindFunctionByName(TEXT("GetDirectionValue"));
		if (!TestRunner->TestNotNull(TEXT("Instance should exist"), Instance) || !TestRunner->TestNotNull(TEXT("GetDirectionValue should exist"), GetFunc))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GetFunc, Result);
		TestRunner->TestTrue(TEXT("GetDirectionValue should execute"), bExecuted);
		TestRunner->TestEqual(TEXT("Direction should be Right (3)"), Result, 3);

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

		if (!TestRunner->TestTrue(TEXT("Float+Bool default should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultFloatBoolCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* VerifyFunc = GeneratedClass->FindFunctionByName(TEXT("VerifyDefaults"));
		if (!TestRunner->TestNotNull(TEXT("Instance should exist"), Instance) || !TestRunner->TestNotNull(TEXT("VerifyDefaults should exist"), VerifyFunc))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, VerifyFunc, Result);
		TestRunner->TestTrue(TEXT("VerifyDefaults should execute"), bExecuted);
		TestRunner->TestEqual(TEXT("Float+Bool defaults should apply correctly"), Result, 42);

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

		if (!TestRunner->TestTrue(TEXT("FVector default should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultVectorCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* VerifyFunc = GeneratedClass->FindFunctionByName(TEXT("VerifyVector"));
		if (!TestRunner->TestNotNull(TEXT("Instance should exist"), Instance) || !TestRunner->TestNotNull(TEXT("VerifyVector should exist"), VerifyFunc))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, VerifyFunc, Result);
		TestRunner->TestTrue(TEXT("VerifyVector should execute"), bExecuted);
		TestRunner->TestEqual(TEXT("FVector default should apply correctly"), Result, 42);

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

		if (!TestRunner->TestTrue(TEXT("FString default should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultStringCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* VerifyFunc = GeneratedClass->FindFunctionByName(TEXT("VerifyString"));
		if (!TestRunner->TestNotNull(TEXT("Instance should exist"), Instance) || !TestRunner->TestNotNull(TEXT("VerifyString should exist"), VerifyFunc))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, VerifyFunc, Result);
		TestRunner->TestTrue(TEXT("VerifyString should execute"), bExecuted);
		TestRunner->TestEqual(TEXT("FString default should apply correctly"), Result, 42);

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

		if (!TestRunner->TestTrue(TEXT("Tags.Add default should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("ADefaultTagsActor"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* VerifyFunc = GeneratedClass->FindFunctionByName(TEXT("VerifyTags"));
		if (!TestRunner->TestNotNull(TEXT("Instance should exist"), Instance) || !TestRunner->TestNotNull(TEXT("VerifyTags should exist"), VerifyFunc))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, VerifyFunc, Result);
		TestRunner->TestTrue(TEXT("VerifyTags should execute"), bExecuted);
		TestRunner->TestEqual(TEXT("Tags.Add should actually add tags to CDO and instances"), Result, 42);

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

		if (!TestRunner->TestTrue(TEXT("Priority default should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UDefaultPriorityCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		UFunction* GetFunc = GeneratedClass->FindFunctionByName(TEXT("GetScore"));
		if (!TestRunner->TestNotNull(TEXT("Instance should exist"), Instance) || !TestRunner->TestNotNull(TEXT("GetScore should exist"), GetFunc))
			return;

		int32 Result = INDEX_NONE;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GetFunc, Result);
		TestRunner->TestTrue(TEXT("GetScore should execute"), bExecuted);
		TestRunner->TestEqual(TEXT("default should override inline initializer (20 > 10)"), Result, 20);

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

		TestRunner->TestFalse(TEXT("Non-existent property default should fail to compile"), bCompiled);

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

		TestRunner->TestFalse(TEXT("default outside class scope should fail"), bCompiled);

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

		TestRunner->TestFalse(TEXT("Type mismatch default should fail to compile"), bCompiled);

		}

	}

};

#endif
