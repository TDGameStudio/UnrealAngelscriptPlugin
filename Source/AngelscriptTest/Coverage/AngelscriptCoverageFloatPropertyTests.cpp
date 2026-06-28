#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

#include <cmath>
#include <limits>

// -----------------------------------------------------------------------------
// AngelscriptCoverageFloatPropertyTests
// -----------------------------------------------------------------------------
// Shader-style coverage for AngelScript float-family *UPROPERTY usage*
// -- the C++ reflection half of the float matrix. This file covers:
//
//   * UPROPERTY declarations (float / double)
//   * Read-back via FProperty reflection
//   * Write round-trip (C++ -> FProperty -> C++)
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
private:
	using FScriptFloatProperty = FDoubleProperty;
	using FScriptFloatValue = double;

	static constexpr FScriptFloatValue AsScriptFloat(float Value)
	{
		return static_cast<FScriptFloatValue>(Value);
	}

public:
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-defaults actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), AsScriptFloat(0.0f),
			TEXT("AS float UPROPERTY defaults to 0.0 and reflects as FDoubleProperty"))));

		// Double defaults to 0.0
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 0.0, TEXT("double UPROPERTY defaults to 0.0"))));
	}

	// -------------------------------------------------------------------------
	// Float family write round-trip: SetByPath -> read back.
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-write actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), AsScriptFloat(3.14159f))));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), AsScriptFloat(3.14159f),
			TEXT("AS float UPROPERTY write round-trip"))));

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), AsScriptFloat(-2.71828f))));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), AsScriptFloat(-2.71828f),
			TEXT("AS float UPROPERTY negative write round-trip"))));

		// Double write round-trip
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 1.4142135623730951)));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 1.4142135623730951, TEXT("double write round-trip"))));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), -1.7320508075688772)));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), -1.7320508075688772, TEXT("double negative write round-trip"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-boundary actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		const FScriptFloatValue FloatMin = AsScriptFloat(std::numeric_limits<float>::min());
		const FScriptFloatValue FloatMax = AsScriptFloat(std::numeric_limits<float>::max());
		const FScriptFloatValue FloatEpsilon = AsScriptFloat(std::numeric_limits<float>::epsilon());

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatMin)));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatMin, TEXT("AS float min"))));

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatMax)));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatMax, TEXT("AS float max"))));

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatEpsilon)));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatEpsilon, TEXT("AS float epsilon"))));

		// Double boundaries
		const double DoubleMin = std::numeric_limits<double>::min();
		const double DoubleMax = std::numeric_limits<double>::max();
		const double DoubleEpsilon = std::numeric_limits<double>::epsilon();

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleMin)));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleMin, TEXT("double min"))));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleMax)));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleMax, TEXT("double max"))));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleEpsilon)));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleEpsilon, TEXT("double epsilon"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-special actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		const FScriptFloatValue FloatNaN = AsScriptFloat(std::numeric_limits<float>::quiet_NaN());
		const FScriptFloatValue FloatInf = AsScriptFloat(std::numeric_limits<float>::infinity());
		const FScriptFloatValue FloatNegInf = AsScriptFloat(-std::numeric_limits<float>::infinity());
		const FScriptFloatValue FloatNegZero = AsScriptFloat(-0.0f);

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatNaN)));
		FScriptFloatValue ReadNaN = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), ReadNaN)));
		ASSERT_THAT(IsTrue(std::isnan(ReadNaN), TEXT("float NaN write round-trip")));

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatInf)));
		FScriptFloatValue ReadInf = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), ReadInf)));
		ASSERT_THAT(IsTrue(std::isinf(ReadInf) && ReadInf > 0.0, TEXT("AS float Inf write round-trip")));

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatNegInf)));
		FScriptFloatValue ReadNegInf = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), ReadNegInf)));
		ASSERT_THAT(IsTrue(std::isinf(ReadNegInf) && ReadNegInf < 0.0, TEXT("AS float -Inf write round-trip")));

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatNegZero)));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatNegZero, TEXT("AS float -0.0"))));

		// Double special values
		const double DoubleNaN = std::numeric_limits<double>::quiet_NaN();
		const double DoubleInf = std::numeric_limits<double>::infinity();
		const double DoubleNegInf = -std::numeric_limits<double>::infinity();

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleNaN)));
		double ReadDoubleNaN = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), ReadDoubleNaN)));
		ASSERT_THAT(IsTrue(std::isnan(ReadDoubleNaN), TEXT("double NaN write round-trip")));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleInf)));
		double ReadDoubleInf = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), ReadDoubleInf)));
		ASSERT_THAT(IsTrue(std::isinf(ReadDoubleInf) && ReadDoubleInf > 0.0, TEXT("double Inf write round-trip")));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleNegInf)));
		double ReadDoubleNegInf = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), ReadDoubleNegInf)));
		ASSERT_THAT(IsTrue(std::isinf(ReadDoubleNegInf) && ReadDoubleNegInf < 0.0, TEXT("double -Inf write round-trip")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float-container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// TArray<float>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("FloatArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<float> should have 3 elements")));

			ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatArray[0]"), AsScriptFloat(1.1f), TEXT("TArray<float>[0]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatArray[1]"), AsScriptFloat(2.2f), TEXT("TArray<float>[1]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatArray[2]"), AsScriptFloat(3.3f), TEXT("TArray<float>[2]"))));
		}

		// TArray<double>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("DoubleArray"), Length)));
			ASSERT_THAT(AreEqual(2, Length, TEXT("TArray<double> should have 2 elements")));

			ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleArray[0]"), 4.4, TEXT("TArray<double>[0]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleArray[1]"), 5.5, TEXT("TArray<double>[1]"))));
		}

		// TMap<int, float>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToFloatMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,float> should have 2 entries")));

			FScriptFloatValue Value = 0.0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("IntToFloatMap"), 10, Value)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value, AsScriptFloat(100.5f), 0.001), TEXT("TMap<int,float>[10] ~= 100.5f")));
		}

		// TMap<FString, double>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToDoubleMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,double> should have 2 entries")));

			double Value = 0.0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FDoubleProperty, double>(*TestRunner, Actor, TEXT("StringToDoubleMap"), FString(TEXT("Pi")), Value)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value, 3.141592653589793, 0.0001), TEXT("TMap<FString,double>[\"Pi\"] ~= 3.14159")));
		}
	}

	TEST_METHOD(FloatPropertySpecifierFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatProperty_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatPropertySpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatSpecifierActor : AActor
			{
				UPROPERTY(meta = (ClampMin = "0.0", ClampMax = "1.0"))
				float ClampedFloat = 0.5f;

				UPROPERTY(meta = (UIMin = "0.0", UIMax = "100.0"))
				float UIFloat = 50.0f;

				UPROPERTY(meta = (Units = "Degrees"))
				float AngleDegrees = 90.0f;

				UPROPERTY(meta = (Units = "Centimeters"))
				float DistanceCentimeters = 100.0f;

				UPROPERTY(Category = "FloatCoverage")
				float CategorizedFloat = 1.0f;
			}
			)AS"),
			TEXT("ACoverageFloatSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float-specifier actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		auto FindProperty = [&](const TCHAR* PropertyName) -> const FProperty*
		{
			return ScriptClass->FindPropertyByName(FName(PropertyName));
		};

		auto VerifyMeta = [&](const TCHAR* PropertyName, const TCHAR* MetaKey, const TCHAR* ExpectedValue) -> bool
		{
			FNoDiscardAsserter LocalAssert(*TestRunner);
			const FProperty* Property = FindProperty(PropertyName);
			if (!LocalAssert.IsNotNull(Property, *FString::Printf(TEXT("%s should be registered"), PropertyName)))
			{
				return false;
			}

			const FString ActualValue = Property->GetMetaData(MetaKey);
			return LocalAssert.AreEqual(
				FString(ExpectedValue),
				ActualValue,
				*FString::Printf(TEXT("%s %s meta should round-trip"), PropertyName, MetaKey));
		};

		const FProperty* ClampedFloat = FindProperty(TEXT("ClampedFloat"));
		ASSERT_THAT(IsNotNull(ClampedFloat, TEXT("ClampedFloat should be registered")));
		if (ClampedFloat == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ClampedFloat->IsA<FScriptFloatProperty>(), TEXT("ClampMin/ClampMax coverage target should be an AS float property")));

		const FProperty* UIFloat = FindProperty(TEXT("UIFloat"));
		ASSERT_THAT(IsNotNull(UIFloat, TEXT("UIFloat should be registered")));
		if (UIFloat == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(UIFloat->IsA<FScriptFloatProperty>(), TEXT("UIMin/UIMax coverage target should be an AS float property")));

		const FProperty* AngleDegrees = FindProperty(TEXT("AngleDegrees"));
		ASSERT_THAT(IsNotNull(AngleDegrees, TEXT("AngleDegrees should be registered")));
		if (AngleDegrees == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AngleDegrees->IsA<FScriptFloatProperty>(), TEXT("Units=\"Degrees\" coverage target should be an AS float property")));

		const FProperty* DistanceCentimeters = FindProperty(TEXT("DistanceCentimeters"));
		ASSERT_THAT(IsNotNull(DistanceCentimeters, TEXT("DistanceCentimeters should be registered")));
		if (DistanceCentimeters == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(DistanceCentimeters->IsA<FScriptFloatProperty>(), TEXT("Units=\"Centimeters\" coverage target should be an AS float property")));

		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("ClampedFloat"), TEXT("ClampMin"), TEXT("0.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("ClampedFloat"), TEXT("ClampMax"), TEXT("1.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("UIFloat"), TEXT("UIMin"), TEXT("0.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("UIFloat"), TEXT("UIMax"), TEXT("100.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("AngleDegrees"), TEXT("Units"), TEXT("Degrees"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("DistanceCentimeters"), TEXT("Units"), TEXT("Centimeters"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("CategorizedFloat"), TEXT("Category"), TEXT("FloatCoverage"))));
#endif
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
