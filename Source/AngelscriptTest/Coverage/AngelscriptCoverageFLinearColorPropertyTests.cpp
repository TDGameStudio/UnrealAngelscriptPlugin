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
// AngelscriptCoverageFLinearColorPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FLinearColor *UPROPERTY usage* -- the FProperty
// reflection half of the FLinearColor matrix.
//
// FLinearColor is a linear space color (float RGBA):
//   - Construction: FLinearColor(r, g, b, a), predefined colors
//   - Operations: +, -, *, /
//   - Methods: ToFColor(), Desaturate(), etc.
//
// Test pattern: Pattern D (Actor + FProperty reflection)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFLinearColorPropertyTest,
	"Angelscript.TestModule.Coverage.FLinearColorProperty",
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
	// FLinearColor declaration defaults: predefined colors, custom values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FLinearColorDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFLinearColorProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFLinearColorPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFLinearColorDefaultsActor : AActor
			{
				UPROPERTY()
				FLinearColor WhiteColor = FLinearColor::White;

				UPROPERTY()
				FLinearColor RedColor = FLinearColor::Red;

				UPROPERTY()
				FLinearColor BlackColor = FLinearColor::Black;

				UPROPERTY()
				FLinearColor CustomColor = FLinearColor(0.5, 0.25, 0.75, 1.0);

				UPROPERTY()
				FLinearColor NoDefaultColor;

				UPROPERTY()
				FLinearColor BlueColor = FLinearColor::Blue;
			}
			)AS"),
			TEXT("ACoverageFLinearColorDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FLinearColor-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FLinearColor-defaults actor should spawn")));

		// White color (1, 1, 1, 1)
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("WhiteColor.R"), 1.0f, TEXT("FLinearColor::White.R"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("WhiteColor.G"), 1.0f, TEXT("FLinearColor::White.G"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("WhiteColor.B"), 1.0f, TEXT("FLinearColor::White.B"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("WhiteColor.A"), 1.0f, TEXT("FLinearColor::White.A"));

		// Red color (1, 0, 0, 1)
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("RedColor.R"), 1.0f, TEXT("FLinearColor::Red.R"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("RedColor.G"), 0.0f, TEXT("FLinearColor::Red.G"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("RedColor.B"), 0.0f, TEXT("FLinearColor::Red.B"));

		// Black color (0, 0, 0, 1)
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("BlackColor.R"), 0.0f, TEXT("FLinearColor::Black.R"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("BlackColor.G"), 0.0f, TEXT("FLinearColor::Black.G"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("BlackColor.B"), 0.0f, TEXT("FLinearColor::Black.B"));

		// Custom color
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("CustomColor.R"), 0.5f, TEXT("FLinearColor custom R"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("CustomColor.G"), 0.25f, TEXT("FLinearColor custom G"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("CustomColor.B"), 0.75f, TEXT("FLinearColor custom B"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("CustomColor.A"), 1.0f, TEXT("FLinearColor custom A"));

		// No default (should be black)
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("NoDefaultColor.R"), 0.0f, TEXT("FLinearColor no default R"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("NoDefaultColor.A"), 0.0f, TEXT("FLinearColor no default A"));

		// Blue color (0, 0, 1, 1)
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("BlueColor.B"), 1.0f, TEXT("FLinearColor::Blue.B"));
	}

	// -------------------------------------------------------------------------
	// FLinearColor write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(FLinearColorWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFLinearColorProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFLinearColorPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFLinearColorWriteActor : AActor
			{
				UPROPERTY()
				FLinearColor ColorValue;
			}
			)AS"),
			TEXT("ACoverageFLinearColorWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FLinearColor-write actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FLinearColor-write actor should spawn")));

		// Write color values
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.R"), 0.8f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.G"), 0.6f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.B"), 0.4f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.A"), 0.9f)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.R"), 0.8f, TEXT("FLinearColor write R"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.G"), 0.6f, TEXT("FLinearColor write G"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.B"), 0.4f, TEXT("FLinearColor write B"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.A"), 0.9f, TEXT("FLinearColor write A"));

		// Write zero (black transparent)
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.R"), 0.0f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.G"), 0.0f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.B"), 0.0f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.A"), 0.0f)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.R"), 0.0f, TEXT("FLinearColor write zero R"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.A"), 0.0f, TEXT("FLinearColor write zero A"));

		// Write full intensity
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.R"), 1.0f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.G"), 1.0f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.B"), 1.0f)));
		ASSERT_THAT(IsTrue(SetByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.A"), 1.0f)));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorValue.R"), 1.0f, TEXT("FLinearColor write full R"));
	}

	// -------------------------------------------------------------------------
	// FLinearColor containers: TArray.
	// -------------------------------------------------------------------------
	TEST_METHOD(FLinearColorContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFLinearColorProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFLinearColorPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFLinearColorContainerActor : AActor
			{
				UPROPERTY()
				TArray<FLinearColor> ColorArray;

				UPROPERTY()
				TMap<int, FLinearColor> IntToColorMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ColorArray.Add(FLinearColor::Red);
					ColorArray.Add(FLinearColor::Green);
					ColorArray.Add(FLinearColor::Blue);

					IntToColorMap.Add(1, FLinearColor::White);
					IntToColorMap.Add(2, FLinearColor::Black);
					IntToColorMap.Add(3, FLinearColor::Yellow);
				}
			}
			)AS"),
			TEXT("ACoverageFLinearColorContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FLinearColor-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FLinearColor-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<FLinearColor>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ColorArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FLinearColor> should have 3 elements")));

			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorArray[0].R"), 1.0f, TEXT("TArray<FLinearColor>[0].R (Red)"));
			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorArray[0].G"), 0.0f, TEXT("TArray<FLinearColor>[0].G (Red)"));
			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorArray[1].G"), 1.0f, TEXT("TArray<FLinearColor>[1].G (Green)"));
			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorArray[2].B"), 1.0f, TEXT("TArray<FLinearColor>[2].B (Blue)"));
		}

		// TMap<int, FLinearColor>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToColorMap"), Count)));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int,FLinearColor> should have 3 entries")));

			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("IntToColorMap[1].R"), 1.0f, TEXT("TMap<int,FLinearColor>[1].R (White)"));
			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("IntToColorMap[1].G"), 1.0f, TEXT("TMap<int,FLinearColor>[1].G (White)"));
			VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("IntToColorMap[2].R"), 0.0f, TEXT("TMap<int,FLinearColor>[2].R (Black)"));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
