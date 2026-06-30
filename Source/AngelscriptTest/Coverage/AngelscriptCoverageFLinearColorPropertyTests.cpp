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
private:
	static bool GetIntLinearColorMapValue(
		FAutomationTestBase& Test,
		UObject* Object,
		FStringView Path,
		const int32 Key,
		FLinearColor& OutValue)
	{
		FPropertyBindingPathIndirection Leaf;
		if (!ResolvePathOnObject(Test, Object, Path, Leaf))
		{
			return false;
		}

		const FMapProperty* MapProperty = CastField<const FMapProperty>(Leaf.GetProperty());
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Property '%.*s' should be a TMap"), Path.Len(), Path.GetData()),
				MapProperty))
		{
			return false;
		}

		const FIntProperty* KeyProperty = CastField<const FIntProperty>(MapProperty->KeyProp);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("TMap key property at '%.*s' should be FIntProperty"), Path.Len(), Path.GetData()),
				KeyProperty))
		{
			return false;
		}

		const FStructProperty* ValueProperty = CastField<const FStructProperty>(MapProperty->ValueProp);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("TMap value property at '%.*s' should be FStructProperty"), Path.Len(), Path.GetData()),
				ValueProperty))
		{
			return false;
		}

		const UScriptStruct* ExpectedStruct = TBaseStructure<FLinearColor>::Get();
		if (!Test.TestTrue(
				*FString::Printf(TEXT("TMap value property at '%.*s' should be FLinearColor"), Path.Len(), Path.GetData()),
				ValueProperty->Struct != nullptr && ExpectedStruct != nullptr
				&& ValueProperty->Struct->IsChildOf(ExpectedStruct)))
		{
			return false;
		}

		FScriptMapHelper Helper(MapProperty, Leaf.GetPropertyAddress());
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (KeyProperty->GetPropertyValue(Helper.GetKeyPtr(SparseIndex)) == Key)
			{
				ValueProperty->CopySingleValue(&OutValue, Helper.GetValuePtr(SparseIndex));
				return true;
			}
		}

		Test.AddError(FString::Printf(
			TEXT("TMap at '%.*s' does not contain key %d"),
			Path.Len(), Path.GetData(),
			Key));
		return false;
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

		// No explicit default uses the bound FLinearColor default constructor: black with opaque alpha.
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("NoDefaultColor.R"), 0.0f, TEXT("FLinearColor no default R"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("NoDefaultColor.A"), 1.0f, TEXT("FLinearColor no default A"));

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

			FLinearColor ColorValue = FLinearColor::Transparent;
			ASSERT_THAT(IsTrue(GetIntLinearColorMapValue(*TestRunner, Actor, TEXT("IntToColorMap"), 1, ColorValue)));
			ASSERT_THAT(IsNear(1.0f, ColorValue.R, 0.001f, TEXT("TMap<int,FLinearColor>[1].R (White)")));
			ASSERT_THAT(IsNear(1.0f, ColorValue.G, 0.001f, TEXT("TMap<int,FLinearColor>[1].G (White)")));

			ASSERT_THAT(IsTrue(GetIntLinearColorMapValue(*TestRunner, Actor, TEXT("IntToColorMap"), 2, ColorValue)));
			ASSERT_THAT(IsNear(0.0f, ColorValue.R, 0.001f, TEXT("TMap<int,FLinearColor>[2].R (Black)")));
		}
	}

	TEST_METHOD(FLinearColorClassMemberExecution)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFLinearColorProperty_ClassMembers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFLinearColorPropertyClassMembers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFLinearColorClassMemberActor : AActor
			{
				UPROPERTY()
				FLinearColor EditableColor = FLinearColor(0.2, 0.4, 0.6, 0.8);

				UPROPERTY()
				TArray<FLinearColor> ColorHistory;

				FLinearColor RuntimeTint = FLinearColor::LucBlue;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FLinearColor LocalTint = EditableColor + RuntimeTint.GetClamped(0.0, 1.0);
					RuntimeTint = LocalTint.GetClamped(0.0, 1.0);
					ColorHistory.Add(EditableColor);
					ColorHistory.Add(ReadConstTint());
					ColorHistory.Add(FLinearColor::MakeFromHex(0xFFFFFFFF, false));
				}

				UFUNCTION()
				FLinearColor ReadRuntimeTint()
				{
					return RuntimeTint;
				}

				UFUNCTION()
				FLinearColor ReadConstTint()
				{
					const FLinearColor ConstTint = FLinearColor::Yellow;
					return ConstTint;
				}

				UFUNCTION()
				FLinearColor BlendHistory()
				{
					FLinearColor Total = FLinearColor::Transparent;
					for (int Index = 0; Index < ColorHistory.Num(); ++Index)
					{
						Total += ColorHistory[Index];
					}
					return Total.GetClamped(0.0, 1.0);
				}
			}
			)AS"),
			TEXT("ACoverageFLinearColorClassMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FLinearColor class-member actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FLinearColor class-member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("EditableColor.R"), 0.2f, TEXT("UPROPERTY FLinearColor custom R should persist"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("EditableColor.A"), 0.8f, TEXT("UPROPERTY FLinearColor custom A should persist"))));

		int32 Length = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ColorHistory"), Length), TEXT("ColorHistory length should resolve")));
		ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FLinearColor> UPROPERTY should collect three class-member colors")));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorHistory[0].G"), 0.4f, TEXT("ColorHistory should store editable color"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorHistory[1].R"), 1.0f, TEXT("ColorHistory should store const predefined color"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ColorHistory[2].B"), 1.0f, TEXT("ColorHistory should store MakeFromHex linear white"))));

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("ReadRuntimeTint"));
			const FLinearColor Result = Invoker.CallAndReturn<FLinearColor>();
			ASSERT_THAT(IsTrue(Result.Equals(FLinearColor(0.2f, 1.0f, 1.0f, 1.0f), 0.001f), TEXT("Non-UPROPERTY FLinearColor class member should update through methods")));
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("ReadConstTint"));
			const FLinearColor Result = Invoker.CallAndReturn<FLinearColor>();
			ASSERT_THAT(IsTrue(Result.Equals(FLinearColor::Yellow, 0.001f), TEXT("const local FLinearColor should return predefined color")));
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("BlendHistory"));
			const FLinearColor Result = Invoker.CallAndReturn<FLinearColor>();
			ASSERT_THAT(IsTrue(Result.Equals(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), 0.001f), TEXT("FLinearColor UPROPERTY array should blend and clamp in UFUNCTION")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
