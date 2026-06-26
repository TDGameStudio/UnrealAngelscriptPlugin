#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFloatPropertyTests
// -----------------------------------------------------------------------------
// "Übershader-style" coverage for AngelScript float-family *UPROPERTY usage*
// -- the C++ reflection half of the float matrix. This file covers:
//
//   * UPROPERTY declarations (float / double)
//   * Read-back via FProperty reflection
//   * Write round-trip (C++ → FProperty → C++)
//   * Boundary values (min/max/epsilon)
//   * Special values (NaN / Inf / -Inf / -0.0)
//   * Container properties (TArray, TMap)
//   * Property specifiers (Edit/Visible/Blueprint + meta)
//
// Test pattern: Pattern D (Actor + FProperty reflection)
//
// float family under test:
//   float / double (only 2 types, much simpler than int's 8)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFloatPropertyTest,
	"Angelscript.TestModule.Coverage.FloatProperty",
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
	// Float family declaration defaults: ensure default-initialized float/double
	// properties are zero and readable via FProperty.
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatFamilyDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatDefaultsActor : AActor
			{
				UPROPERTY()
				float FloatValue;

				UPROPERTY()
				double DoubleValue;
			}
			)AS"),
			TEXT("ACoverageFloatDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-defaults actor should spawn")));

		// Float defaults to 0.0f
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), 0.0f, TEXT("float UPROPERTY defaults to 0.0f"));

		// Double defaults to 0.0
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 0.0, TEXT("double UPROPERTY defaults to 0.0"));
	}

	// -------------------------------------------------------------------------
	// Float family write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatFamilyWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatWriteActor : AActor
			{
				UPROPERTY()
				float FloatValue;

				UPROPERTY()
				double DoubleValue;
			}
			)AS"),
			TEXT("ACoverageFloatWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float-write actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-write actor should spawn")));

		// Float write round-trip
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), 3.14159f)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), 3.14159f, TEXT("float write round-trip"));

		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), -2.71828f)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), -2.71828f, TEXT("float negative write round-trip"));

		// Double write round-trip
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 1.4142135623730951)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 1.4142135623730951, TEXT("double write round-trip"));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), -1.7320508075688772)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), -1.7320508075688772, TEXT("double negative write round-trip"));
	}

	// -------------------------------------------------------------------------
	// Float family boundary values: min, max, epsilon.
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatFamilyBoundaryValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatProperty_Boundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatPropertyBoundary.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatBoundaryActor : AActor
			{
				UPROPERTY()
				float FloatValue;

				UPROPERTY()
				double DoubleValue;
			}
			)AS"),
			TEXT("ACoverageFloatBoundaryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float-boundary actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-boundary actor should spawn")));

		// Float boundaries
		const float FloatMin = std::numeric_limits<float>::min();
		const float FloatMax = std::numeric_limits<float>::max();
		const float FloatEpsilon = std::numeric_limits<float>::epsilon();

		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatMin)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatMin, TEXT("float min"));

		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatMax)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatMax, TEXT("float max"));

		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatEpsilon)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatEpsilon, TEXT("float epsilon"));

		// Double boundaries
		const double DoubleMin = std::numeric_limits<double>::min();
		const double DoubleMax = std::numeric_limits<double>::max();
		const double DoubleEpsilon = std::numeric_limits<double>::epsilon();

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleMin)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleMin, TEXT("double min"));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleMax)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleMax, TEXT("double max"));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleEpsilon)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleEpsilon, TEXT("double epsilon"));
	}

	// -------------------------------------------------------------------------
	// Float family special values: NaN, Inf, -Inf, -0.0.
	// Note: NaN != NaN, so we check with std::isnan.
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatFamilySpecialValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatProperty_Special"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatPropertySpecial.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatSpecialActor : AActor
			{
				UPROPERTY()
				float FloatValue;

				UPROPERTY()
				double DoubleValue;
			}
			)AS"),
			TEXT("ACoverageFloatSpecialActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float-special actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-special actor should spawn")));

		// Float special values
		const float FloatNaN = std::numeric_limits<float>::quiet_NaN();
		const float FloatInf = std::numeric_limits<float>::infinity();
		const float FloatNegInf = -std::numeric_limits<float>::infinity();
		const float FloatNegZero = -0.0f;

		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatNaN)));
		float ReadNaN = 0.0f;
		ASSERT_THAT(IsTrue(GetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), ReadNaN)));
		TestRunner->TestTrue(TEXT("float NaN write round-trip"), std::isnan(ReadNaN));

		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatInf)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatInf, TEXT("float Inf"));

		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatNegInf)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatNegInf, TEXT("float -Inf"));

		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatNegZero)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), FloatNegZero, TEXT("float -0.0"));

		// Double special values
		const double DoubleNaN = std::numeric_limits<double>::quiet_NaN();
		const double DoubleInf = std::numeric_limits<double>::infinity();
		const double DoubleNegInf = -std::numeric_limits<double>::infinity();

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleNaN)));
		double ReadDoubleNaN = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), ReadDoubleNaN)));
		TestRunner->TestTrue(TEXT("double NaN write round-trip"), std::isnan(ReadDoubleNaN));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleInf)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleInf, TEXT("double Inf"));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleNegInf)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleNegInf, TEXT("double -Inf"));
	}

	// -------------------------------------------------------------------------
	// Float containers: TArray<float/double>, TMap<*, float/double>.
	// Note: float cannot be TMap keys or TSet elements (hashing issues).
	// -------------------------------------------------------------------------
	TEST_METHOD(FloatContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatContainerActor : AActor
			{
				UPROPERTY()
				TArray<float> FloatArray;

				UPROPERTY()
				TArray<double> DoubleArray;

				UPROPERTY()
				TMap<int, float> IntToFloatMap;

				UPROPERTY()
				TMap<FString, double> StringToDoubleMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FloatArray.Add(1.1f);
					FloatArray.Add(2.2f);
					FloatArray.Add(3.3f);

					DoubleArray.Add(4.4);
					DoubleArray.Add(5.5);

					IntToFloatMap.Add(10, 100.5f);
					IntToFloatMap.Add(20, 200.5f);

					StringToDoubleMap.Add("Pi", 3.141592653589793);
					StringToDoubleMap.Add("E", 2.718281828459045);
				}
			}
			)AS"),
			TEXT("ACoverageFloatContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<float>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("FloatArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<float> should have 3 elements")));

			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatArray[0]"), 1.1f, TEXT("TArray<float>[0]"));
			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatArray[1]"), 2.2f, TEXT("TArray<float>[1]"));
			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatArray[2]"), 3.3f, TEXT("TArray<float>[2]"));
		}

		// TArray<double>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("DoubleArray"), Length)));
			ASSERT_THAT(AreEqual(2, Length, TEXT("TArray<double> should have 2 elements")));

			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleArray[0]"), 4.4, TEXT("TArray<double>[0]"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleArray[1]"), 5.5, TEXT("TArray<double>[1]"));
		}

		// TMap<int, float>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToFloatMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,float> should have 2 entries")));

			float Value = 0.0f;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FFloatProperty, float>(*TestRunner, Actor, TEXT("IntToFloatMap"), 10, Value)));
			TestRunner->TestTrue(TEXT("TMap<int,float>[10] ~= 100.5f"), FMath::IsNearlyEqual(Value, 100.5f, 0.001f));
		}

		// TMap<FString, double>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToDoubleMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,double> should have 2 entries")));

			double Value = 0.0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FDoubleProperty, double>(*TestRunner, Actor, TEXT("StringToDoubleMap"), FString(TEXT("Pi")), Value)));
			TestRunner->TestTrue(TEXT("TMap<FString,double>[\"Pi\"] ~= 3.14159"), FMath::IsNearlyEqual(Value, 3.141592653589793, 0.0001));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
