#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Containers/Set.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreNet.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

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

	static FName ResolveReplicatedPropertyName(const UClass* OwnerClass, const FLifetimeProperty& LifetimeProperty)
	{
		for (TFieldIterator<FProperty> It(OwnerClass); It; ++It)
		{
			if (It->RepIndex == LifetimeProperty.RepIndex)
			{
				return It->GetFName();
			}
		}

		return NAME_None;
	}

	static TSet<FName> CollectReplicatedPropertyNames(
		const UClass* OwnerClass,
		const TArray<FLifetimeProperty>& LifetimeProperties)
	{
		TSet<FName> PropertyNames;
		for (const FLifetimeProperty& LifetimeProperty : LifetimeProperties)
		{
			const FName PropertyName = ResolveReplicatedPropertyName(OwnerClass, LifetimeProperty);
			if (PropertyName != NAME_None)
			{
				PropertyNames.Add(PropertyName);
			}
		}

		return PropertyNames;
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

				UPROPERTY()
				float InitializedFloat = 1.25f;

				UPROPERTY()
				double InitializedDouble = 2.5;
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

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 0.0, TEXT("double UPROPERTY defaults to 0.0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("InitializedFloat"), AsScriptFloat(1.25f),
			TEXT("AS float UPROPERTY initializer should read back through reflection"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("InitializedDouble"), 2.5, TEXT("double UPROPERTY initializer should read back through reflection"))));

		AActor* DefaultActor = Cast<AActor>(ScriptClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(DefaultActor, TEXT("Float-defaults actor CDO should be available")));
		if (DefaultActor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, DefaultActor, TEXT("FloatValue"), AsScriptFloat(0.0f),
			TEXT("AS float UPROPERTY CDO default should remain zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, DefaultActor, TEXT("DoubleValue"), 0.0,
			TEXT("double UPROPERTY CDO default should remain zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, DefaultActor, TEXT("InitializedFloat"), AsScriptFloat(1.25f),
			TEXT("AS float UPROPERTY initializer should be present on CDO"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, DefaultActor, TEXT("InitializedDouble"), 2.5,
			TEXT("double UPROPERTY initializer should be present on CDO"))));
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

	TEST_METHOD(FloatPropertyScriptMutationRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatProperty_ScriptMutation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatPropertyScriptMutation.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatScriptMutationActor : AActor
			{
				UPROPERTY()
				float FloatValue = 1.5f;

				UPROPERTY()
				double DoubleValue = 2.25;

				UFUNCTION()
				void AssignValues(float NewFloat, double NewDouble)
				{
					FloatValue = NewFloat;
					DoubleValue = NewDouble;
				}

				UFUNCTION()
				float AddToFloat(float Delta)
				{
					FloatValue += Delta;
					return FloatValue;
				}

				UFUNCTION()
				double AddToDouble(double Delta)
				{
					DoubleValue += Delta;
					return DoubleValue;
				}
			}
			)AS"),
			TEXT("ACoverageFloatScriptMutationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float script-mutation actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Float script-mutation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AssignValues"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AssignValues should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}

			Invoker.AddParam<double>(12.5).AddParam<double>(42.75);
			ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("AssignValues should execute")));
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), AsScriptFloat(12.5f),
			TEXT("AS float UPROPERTY should reflect script-assigned value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 42.75,
			TEXT("double UPROPERTY should reflect script-assigned value"))));

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AddToFloat"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AddToFloat should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}

			Invoker.AddParam<double>(0.25);
			const double Result = Invoker.CallAndReturn<double>(0.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 12.75, 0.001),
				TEXT("AS float UFUNCTION return should expose the mutated UPROPERTY value")));
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), AsScriptFloat(12.75f),
			TEXT("AS float UPROPERTY should retain script arithmetic mutation"))));

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("AddToDouble"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("AddToDouble should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}

			Invoker.AddParam<double>(0.125);
			const double Result = Invoker.CallAndReturn<double>(0.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 42.875, 0.0001),
				TEXT("double UFUNCTION return should expose the mutated UPROPERTY value")));
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 42.875,
			TEXT("double UPROPERTY should retain script arithmetic mutation"))));
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
		const FScriptFloatValue FloatPosZero = AsScriptFloat(0.0f);
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

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatPosZero)));
		FScriptFloatValue ReadPosZero = 1.0;
		ASSERT_THAT(IsTrue(GetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), ReadPosZero)));
		ASSERT_THAT(AreEqual(0.0, ReadPosZero, TEXT("AS float +0.0 should compare equal to zero")));
		ASSERT_THAT(IsFalse(std::signbit(ReadPosZero), TEXT("AS float +0.0 should not carry a negative sign bit")));

		ASSERT_THAT(IsTrue(SetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), FloatNegZero)));
		FScriptFloatValue ReadNegZero = 1.0;
		ASSERT_THAT(IsTrue(GetByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("FloatValue"), ReadNegZero)));
		ASSERT_THAT(AreEqual(0.0, ReadNegZero, TEXT("AS float -0.0 should compare equal to zero")));
		ASSERT_THAT(IsTrue(std::signbit(ReadNegZero), TEXT("AS float -0.0 should preserve its sign bit")));

		// Double special values
		const double DoubleNaN = std::numeric_limits<double>::quiet_NaN();
		const double DoubleInf = std::numeric_limits<double>::infinity();
		const double DoubleNegInf = -std::numeric_limits<double>::infinity();
		const double DoublePosZero = 0.0;
		const double DoubleNegZero = -0.0;

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

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoublePosZero)));
		double ReadDoublePosZero = 1.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), ReadDoublePosZero)));
		ASSERT_THAT(AreEqual(0.0, ReadDoublePosZero, TEXT("double +0.0 should compare equal to zero")));
		ASSERT_THAT(IsFalse(std::signbit(ReadDoublePosZero), TEXT("double +0.0 should not carry a negative sign bit")));

		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), DoubleNegZero)));
		double ReadDoubleNegZero = 1.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), ReadDoubleNegZero)));
		ASSERT_THAT(AreEqual(0.0, ReadDoubleNegZero, TEXT("double -0.0 should compare equal to zero")));
		ASSERT_THAT(IsTrue(std::signbit(ReadDoubleNegZero), TEXT("double -0.0 should preserve its sign bit")));
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

	TEST_METHOD(FloatNestedContainerBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> ExpectedDiagnostics = { TEXT("Containers cannot be nested in other containers") };

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageFloatProperty_NestedArrayUnsupported"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatNestedArrayActor : AActor
			{
				UPROPERTY()
				TArray<TArray<float>> Matrix;
			}
			)AS"),
			TEXT("TArray<TArray<float>> should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(FloatReplicatedProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFloatProperty_Replication"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFloatPropertyReplication.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFloatReplicationActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(Replicated)
				float ReplicatedFloat = 1.25f;

				UPROPERTY(Replicated)
				double ReplicatedDouble = 2.5;

				UPROPERTY(ReplicatedUsing=OnRep_RepNotifyFloat)
				float RepNotifyFloat = 3.75f;

				UPROPERTY(ReplicatedUsing=OnRep_PreciseValue)
				double PreciseValue = 4.5;

				UFUNCTION()
				void OnRep_RepNotifyFloat()
				{
				}

				UFUNCTION()
				void OnRep_PreciseValue()
				{
				}
			}
			)AS"),
			TEXT("ACoverageFloatReplicationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float-replication actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FScriptFloatProperty* ReplicatedFloatProperty = CastField<FScriptFloatProperty>(ScriptClass->FindPropertyByName(FName(TEXT("ReplicatedFloat"))));
		const FDoubleProperty* ReplicatedDoubleProperty = CastField<FDoubleProperty>(ScriptClass->FindPropertyByName(FName(TEXT("ReplicatedDouble"))));
		const FScriptFloatProperty* RepNotifyFloatProperty = CastField<FScriptFloatProperty>(ScriptClass->FindPropertyByName(FName(TEXT("RepNotifyFloat"))));
		const FDoubleProperty* PreciseValueProperty = CastField<FDoubleProperty>(ScriptClass->FindPropertyByName(FName(TEXT("PreciseValue"))));
		ASSERT_THAT(IsNotNull(ReplicatedFloatProperty, TEXT("Replicated float property should be generated as FDoubleProperty under float64 mode")));
		ASSERT_THAT(IsNotNull(ReplicatedDoubleProperty, TEXT("Replicated double property should be generated as FDoubleProperty")));
		ASSERT_THAT(IsNotNull(RepNotifyFloatProperty, TEXT("RepNotify float property should be generated as FDoubleProperty under float64 mode")));
		ASSERT_THAT(IsNotNull(PreciseValueProperty, TEXT("RepNotify double property should be generated as FDoubleProperty")));
		if (ReplicatedFloatProperty == nullptr || ReplicatedDoubleProperty == nullptr || RepNotifyFloatProperty == nullptr || PreciseValueProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReplicatedFloatProperty->HasAnyPropertyFlags(CPF_Net), TEXT("Replicated float should carry CPF_Net")));
		ASSERT_THAT(IsFalse(ReplicatedFloatProperty->HasAnyPropertyFlags(CPF_RepNotify), TEXT("plain Replicated float should not carry CPF_RepNotify")));
		ASSERT_THAT(IsTrue(ReplicatedDoubleProperty->HasAnyPropertyFlags(CPF_Net), TEXT("Replicated double should carry CPF_Net")));
		ASSERT_THAT(IsFalse(ReplicatedDoubleProperty->HasAnyPropertyFlags(CPF_RepNotify), TEXT("plain Replicated double should not carry CPF_RepNotify")));
		ASSERT_THAT(IsTrue(RepNotifyFloatProperty->HasAnyPropertyFlags(CPF_Net), TEXT("ReplicatedUsing float should carry CPF_Net")));
		ASSERT_THAT(IsTrue(RepNotifyFloatProperty->HasAnyPropertyFlags(CPF_RepNotify), TEXT("ReplicatedUsing float should carry CPF_RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_RepNotifyFloat")), RepNotifyFloatProperty->RepNotifyFunc,
			TEXT("ReplicatedUsing float should preserve the RepNotify function name")));
		ASSERT_THAT(IsTrue(PreciseValueProperty->HasAnyPropertyFlags(CPF_Net), TEXT("ReplicatedUsing double should carry CPF_Net")));
		ASSERT_THAT(IsTrue(PreciseValueProperty->HasAnyPropertyFlags(CPF_RepNotify), TEXT("ReplicatedUsing double should carry CPF_RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_PreciseValue")), PreciseValueProperty->RepNotifyFunc,
			TEXT("ReplicatedUsing double should preserve the RepNotify function name")));

		const UFunction* RepNotifyFloatFunction = FindGeneratedFunction(ScriptClass, TEXT("OnRep_RepNotifyFloat"));
		const UFunction* RepNotifyDoubleFunction = FindGeneratedFunction(ScriptClass, TEXT("OnRep_PreciseValue"));
		ASSERT_THAT(IsNotNull(RepNotifyFloatFunction, TEXT("float RepNotify callback should be generated")));
		ASSERT_THAT(IsNotNull(RepNotifyDoubleFunction, TEXT("double RepNotify callback should be generated")));
		if (RepNotifyFloatFunction == nullptr || RepNotifyDoubleFunction == nullptr)
		{
			return;
		}

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptASClass, TEXT("Float-replication actor should be backed by UASClass")));
		if (ScriptASClass == nullptr)
		{
			return;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		ScriptASClass->GetLifetimeScriptReplicationList(LifetimeProperties);
		const TSet<FName> LifetimePropertyNames = CollectReplicatedPropertyNames(ScriptClass, LifetimeProperties);

		ASSERT_THAT(AreEqual(4, LifetimeProperties.Num(),
			TEXT("float lifetime replication list should contain all script replicated float-family properties")));
		ASSERT_THAT(AreEqual(4, LifetimePropertyNames.Num(),
			TEXT("float lifetime replication entries should resolve to unique property names")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("ReplicatedFloat"))),
			TEXT("float lifetime replication list should include ReplicatedFloat")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("ReplicatedDouble"))),
			TEXT("float lifetime replication list should include ReplicatedDouble")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("RepNotifyFloat"))),
			TEXT("float lifetime replication list should include RepNotifyFloat")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("PreciseValue"))),
			TEXT("float lifetime replication list should include PreciseValue")));
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
				UPROPERTY(EditAnywhere)
				float EditAnywhereFloat = 1.0f;

				UPROPERTY(EditDefaultsOnly)
				float EditDefaultsOnlyFloat = 2.0f;

				UPROPERTY(EditInstanceOnly)
				float EditInstanceOnlyFloat = 3.0f;

				UPROPERTY(NotEditable)
				float NotEditableFloat = 4.0f;

				UPROPERTY(EditConst)
				float EditConstFloat = 5.0f;

				UPROPERTY(VisibleAnywhere)
				float VisibleAnywhereFloat = 6.0f;

				UPROPERTY(BlueprintReadWrite)
				float BlueprintReadWriteFloat = 7.0f;

				UPROPERTY(BlueprintReadOnly)
				float BlueprintReadOnlyFloat = 8.0f;

				UPROPERTY(Transient)
				float TransientFloat = 9.0f;

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

				UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
				float EditableClampedFloat = 0.25f;

				UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoubleCoverage", meta = (ClampMin = "-10.0", ClampMax = "10.0", UIMin = "-5.0", UIMax = "5.0", Units = "Seconds"))
				double EditableReadonlySeconds = 1.5;
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

		auto CheckFlag = [&](const TCHAR* PropertyName, EPropertyFlags Flag, bool bExpected, const TCHAR* Label)
		{
			const FProperty* Property = FindProperty(PropertyName);
			ASSERT_THAT(IsNotNull(Property, *FString::Printf(TEXT("%s should be registered"), PropertyName)));
			if (Property == nullptr)
			{
				return;
			}

			if (bExpected)
			{
				ASSERT_THAT(IsTrue(Property->HasAnyPropertyFlags(Flag), Label));
			}
			else
			{
				ASSERT_THAT(IsFalse(Property->HasAnyPropertyFlags(Flag), Label));
			}
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

		CheckFlag(TEXT("EditAnywhereFloat"), CPF_Edit, true, TEXT("EditAnywhere float -> CPF_Edit"));
		CheckFlag(TEXT("EditAnywhereFloat"), CPF_DisableEditOnInstance, false, TEXT("EditAnywhere float -> editable on instance"));
		CheckFlag(TEXT("EditAnywhereFloat"), CPF_DisableEditOnTemplate, false, TEXT("EditAnywhere float -> editable on defaults"));

		CheckFlag(TEXT("EditDefaultsOnlyFloat"), CPF_Edit, true, TEXT("EditDefaultsOnly float -> CPF_Edit"));
		CheckFlag(TEXT("EditDefaultsOnlyFloat"), CPF_DisableEditOnInstance, true, TEXT("EditDefaultsOnly float -> disabled on instance"));
		CheckFlag(TEXT("EditDefaultsOnlyFloat"), CPF_DisableEditOnTemplate, false, TEXT("EditDefaultsOnly float -> editable on defaults"));

		CheckFlag(TEXT("EditInstanceOnlyFloat"), CPF_Edit, true, TEXT("EditInstanceOnly float -> CPF_Edit"));
		CheckFlag(TEXT("EditInstanceOnlyFloat"), CPF_DisableEditOnTemplate, true, TEXT("EditInstanceOnly float -> disabled on defaults"));
		CheckFlag(TEXT("EditInstanceOnlyFloat"), CPF_DisableEditOnInstance, false, TEXT("EditInstanceOnly float -> editable on instance"));

		CheckFlag(TEXT("NotEditableFloat"), CPF_Edit, false, TEXT("NotEditable float -> clears CPF_Edit"));

		CheckFlag(TEXT("EditConstFloat"), CPF_Edit, true, TEXT("EditConst float keeps default CPF_Edit"));
		CheckFlag(TEXT("EditConstFloat"), CPF_EditConst, true, TEXT("EditConst float -> CPF_EditConst"));

		CheckFlag(TEXT("VisibleAnywhereFloat"), CPF_Edit, true, TEXT("VisibleAnywhere float -> CPF_Edit"));
		CheckFlag(TEXT("VisibleAnywhereFloat"), CPF_EditConst, true, TEXT("VisibleAnywhere float -> CPF_EditConst"));
		CheckFlag(TEXT("VisibleAnywhereFloat"), CPF_DisableEditOnInstance, false, TEXT("VisibleAnywhere float -> visible on instance"));
		CheckFlag(TEXT("VisibleAnywhereFloat"), CPF_DisableEditOnTemplate, false, TEXT("VisibleAnywhere float -> visible on defaults"));

		CheckFlag(TEXT("BlueprintReadWriteFloat"), CPF_BlueprintVisible, true, TEXT("BlueprintReadWrite float -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("BlueprintReadWriteFloat"), CPF_BlueprintReadOnly, false, TEXT("BlueprintReadWrite float -> not read-only"));

		CheckFlag(TEXT("BlueprintReadOnlyFloat"), CPF_BlueprintVisible, true, TEXT("BlueprintReadOnly float -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("BlueprintReadOnlyFloat"), CPF_BlueprintReadOnly, true, TEXT("BlueprintReadOnly float -> CPF_BlueprintReadOnly"));

		CheckFlag(TEXT("TransientFloat"), CPF_Transient, true, TEXT("Transient float -> CPF_Transient"));
		CheckFlag(TEXT("EditableClampedFloat"), CPF_Edit, true, TEXT("EditAnywhere+Clamp float -> CPF_Edit"));
		CheckFlag(TEXT("EditableReadonlySeconds"), CPF_Edit, true, TEXT("EditAnywhere double -> CPF_Edit"));
		CheckFlag(TEXT("EditableReadonlySeconds"), CPF_BlueprintVisible, true, TEXT("BlueprintReadOnly double -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("EditableReadonlySeconds"), CPF_BlueprintReadOnly, true, TEXT("BlueprintReadOnly double -> CPF_BlueprintReadOnly"));

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

		const FProperty* EditableReadonlySeconds = FindProperty(TEXT("EditableReadonlySeconds"));
		ASSERT_THAT(IsNotNull(EditableReadonlySeconds, TEXT("EditableReadonlySeconds should be registered")));
		if (EditableReadonlySeconds == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(EditableReadonlySeconds->IsA<FDoubleProperty>(), TEXT("double specifier coverage target should be a double property")));

		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("ClampedFloat"), TEXT("ClampMin"), TEXT("0.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("ClampedFloat"), TEXT("ClampMax"), TEXT("1.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("UIFloat"), TEXT("UIMin"), TEXT("0.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("UIFloat"), TEXT("UIMax"), TEXT("100.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("AngleDegrees"), TEXT("Units"), TEXT("Degrees"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("DistanceCentimeters"), TEXT("Units"), TEXT("Centimeters"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("CategorizedFloat"), TEXT("Category"), TEXT("FloatCoverage"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("EditableClampedFloat"), TEXT("ClampMin"), TEXT("0.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("EditableClampedFloat"), TEXT("ClampMax"), TEXT("1.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("EditableReadonlySeconds"), TEXT("ClampMin"), TEXT("-10.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("EditableReadonlySeconds"), TEXT("ClampMax"), TEXT("10.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("EditableReadonlySeconds"), TEXT("UIMin"), TEXT("-5.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("EditableReadonlySeconds"), TEXT("UIMax"), TEXT("5.0"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("EditableReadonlySeconds"), TEXT("Units"), TEXT("Seconds"))));
		ASSERT_THAT(IsTrue(VerifyMeta(TEXT("EditableReadonlySeconds"), TEXT("Category"), TEXT("DoubleCoverage"))));
#endif
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
