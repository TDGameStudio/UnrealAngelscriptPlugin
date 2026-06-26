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
// AngelscriptCoverageBoolPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript bool *UPROPERTY usage*.
//
// Bool is the simplest type:
//   - Only 1 type (bool)
//   - Only 2 values (true/false)
//   - No arithmetic operations
//   - No methods
//
// Test pattern: Pattern D (Actor + FProperty reflection)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageBoolPropertyTest,
	"Angelscript.TestModule.Coverage.BoolProperty",
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
	// Bool declaration defaults: true, false, no default.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolDefaultsActor : AActor
			{
				UPROPERTY()
				bool TrueValue = true;

				UPROPERTY()
				bool FalseValue = false;

				UPROPERTY()
				bool NoDefaultValue;
			}
			)AS"),
			TEXT("ACoverageBoolDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Bool-defaults actor should spawn")));

		// true default
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TrueValue"), true, TEXT("bool UPROPERTY with true default"));

		// false default
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FalseValue"), false, TEXT("bool UPROPERTY with false default"));

		// no default (should be false)
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NoDefaultValue"), false, TEXT("bool UPROPERTY without default should be false"));
	}

	// -------------------------------------------------------------------------
	// Bool write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolWriteActor : AActor
			{
				UPROPERTY()
				bool BoolValue;
			}
			)AS"),
			TEXT("ACoverageBoolWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-write actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Bool-write actor should spawn")));

		// Write true
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true)));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true, TEXT("bool write true round-trip"));

		// Write false
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), false)));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), false, TEXT("bool write false round-trip"));

		// Toggle multiple times
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true)));
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), false)));
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true)));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true, TEXT("bool multiple toggle"));
	}

	// -------------------------------------------------------------------------
	// Bool containers: TArray, TMap, TSet.
	// Note: TSet<bool> can only have 0, 1, or 2 elements (true/false).
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolContainerActor : AActor
			{
				UPROPERTY()
				TArray<bool> BoolArray;

				UPROPERTY()
				TMap<int, bool> IntToBoolMap;

				UPROPERTY()
				TMap<bool, int> BoolToIntMap;

				UPROPERTY()
				TSet<bool> BoolSet;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BoolArray.Add(true);
					BoolArray.Add(false);
					BoolArray.Add(true);

					IntToBoolMap.Add(1, true);
					IntToBoolMap.Add(2, false);

					BoolToIntMap.Add(true, 100);
					BoolToIntMap.Add(false, 200);

					BoolSet.Add(true);
					BoolSet.Add(false);
					BoolSet.Add(true);  // Duplicate
				}
			}
			)AS"),
			TEXT("ACoverageBoolContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Bool-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<bool>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("BoolArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<bool> should have 3 elements")));

			VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolArray[0]"), true, TEXT("TArray<bool>[0]"));
			VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolArray[1]"), false, TEXT("TArray<bool>[1]"));
			VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolArray[2]"), true, TEXT("TArray<bool>[2]"));
		}

		// TMap<int, bool>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToBoolMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,bool> should have 2 entries")));

			bool Value = false;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FBoolProperty, bool>(*TestRunner, Actor, TEXT("IntToBoolMap"), 1, Value)));
			ASSERT_THAT(AreEqual(true, Value, TEXT("TMap<int,bool>[1] should be true")));
		}

		// TMap<bool, int> (only 2 possible keys)
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("BoolToIntMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<bool,int> should have 2 entries (max possible)")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<bool, FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolToIntMap"), true, Value)));
			ASSERT_THAT(AreEqual(100, Value, TEXT("TMap<bool,int>[true] should be 100")));

			ASSERT_THAT(IsTrue(GetMapValueByPath<bool, FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolToIntMap"), false, Value)));
			ASSERT_THAT(AreEqual(200, Value, TEXT("TMap<bool,int>[false] should be 200")));
		}

		// TSet<bool> (only 2 possible elements)
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("BoolSet"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<bool> should have 2 elements (max possible, deduplicated)")));

			bool bContainsTrue = SetContainsByPath<bool>(*TestRunner, Actor, TEXT("BoolSet"), true);
			bool bContainsFalse = SetContainsByPath<bool>(*TestRunner, Actor, TEXT("BoolSet"), false);
			ASSERT_THAT(IsTrue(bContainsTrue, TEXT("TSet<bool> should contain true")));
			ASSERT_THAT(IsTrue(bContainsFalse, TEXT("TSet<bool> should contain false")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
