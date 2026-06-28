#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMetaSpecifierTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript UPROPERTY meta specifiers that are commonly used
// for numerical properties and editor customization:
//
//   * ClampMin / ClampMax     - Clamp numeric value ranges
//   * UIMin / UIMax           - UI slider ranges (doesn't clamp actual values)
//   * Units                   - Display units (Degrees, Centimeters, etc.)
//   * DisplayName             - Override property display name in editor
//   * EditCondition           - Conditional editing based on another property
//   * InlineEditConditionToggle - Property acts as its own edit condition toggle
//
// These meta specifiers are primarily editor-only (WITH_EDITOR) and stored in
// the property's metadata map. This test validates that AngelScript's property
// preprocessor correctly parses and forwards these meta keys to UE's property
// system.
//
// Test pattern: Pattern D (Actor + FProperty reflection + metadata reading)
// Coverage matrix: Documents/Coverage/Coverage_IntProperty.md (sub-matrix 4)
//                  Documents/Coverage/Coverage_FloatProperty.md (sub-matrix 4)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMetaSpecifierTest,
	"Angelscript.TestModule.Coverage.MetaSpecifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const FProperty* RequireProperty(UClass* ScriptClass, const TCHAR* PropertyName)
	{
		return ScriptClass != nullptr ? ScriptClass->FindPropertyByName(FName(PropertyName)) : nullptr;
	}

	static UFunction* RequireFunction(UClass* ScriptClass, const TCHAR* FunctionName)
	{
		return ScriptClass != nullptr ? FindGeneratedFunction(ScriptClass, FName(FunctionName)) : nullptr;
	}

	static FProperty* RequireFunctionParam(UFunction* Function, const TCHAR* ParamName)
	{
		return Function != nullptr ? FindFProperty<FProperty>(Function, FName(ParamName)) : nullptr;
	}

	static FString GetMetaDataOrEmpty(const FProperty* Property, const TCHAR* Key)
	{
		return Property != nullptr ? Property->GetMetaData(Key) : FString();
	}

	static bool HasMetaData(const FProperty* Property, const TCHAR* Key)
	{
		return Property != nullptr && Property->HasMetaData(Key);
	}

	static bool ExpectPropertyMeta(FAutomationTestBase& Test, UClass* ScriptClass, const TCHAR* PropertyName, const TCHAR* Key, const TCHAR* ExpectedValue)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FProperty* Property = RequireProperty(ScriptClass, PropertyName);
		if (!LocalAssert.IsNotNull(Property, *FString::Printf(TEXT("%s property should exist"), PropertyName)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			FString(ExpectedValue),
			GetMetaDataOrEmpty(Property, Key),
			*FString::Printf(TEXT("%s %s meta should be preserved"), PropertyName, Key));
	}

	static bool ExpectPropertyHasMeta(FAutomationTestBase& Test, UClass* ScriptClass, const TCHAR* PropertyName, const TCHAR* Key)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FProperty* Property = RequireProperty(ScriptClass, PropertyName);
		if (!LocalAssert.IsNotNull(Property, *FString::Printf(TEXT("%s property should exist"), PropertyName)))
		{
			return false;
		}

		return LocalAssert.IsTrue(
			HasMetaData(Property, Key),
			*FString::Printf(TEXT("%s %s meta should be present"), PropertyName, Key));
	}

	static bool ExpectPropertyLacksMeta(FAutomationTestBase& Test, UClass* ScriptClass, const TCHAR* PropertyName, const TCHAR* Key)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FProperty* Property = RequireProperty(ScriptClass, PropertyName);
		if (!LocalAssert.IsNotNull(Property, *FString::Printf(TEXT("%s property should exist"), PropertyName)))
		{
			return false;
		}

		return LocalAssert.IsTrue(
			GetMetaDataOrEmpty(Property, Key).IsEmpty(),
			*FString::Printf(TEXT("%s should not have %s metadata"), PropertyName, Key));
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
	// ClampMin / ClampMax: Numeric range clamping metadata.
	// Used for int/float properties to enforce hard limits.
	// -------------------------------------------------------------------------
	TEST_METHOD(ClampMinMaxMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_Clamp"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaClamp.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaClampActor : AActor
			{
				UPROPERTY(meta = (ClampMin = "0", ClampMax = "100"))
				int ClampedInt = 50;

				UPROPERTY(meta = (ClampMin = "0.0", ClampMax = "1.0"))
				float ClampedFloat = 0.5f;

				UPROPERTY(meta = (ClampMin = "-10.0", ClampMax = "10.0"))
				double ClampedDouble = 0.0;

				UPROPERTY(meta = (ClampMin = "0"))
				int OnlyMinInt = 10;

				UPROPERTY(meta = (ClampMax = "255"))
				int OnlyMaxInt = 100;
			}
			)AS"),
			TEXT("ACoverageMetaClampActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Meta clamp actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ClampedInt"), TEXT("ClampMin"), TEXT("0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ClampedInt"), TEXT("ClampMax"), TEXT("100"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ClampedFloat"), TEXT("ClampMin"), TEXT("0.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ClampedFloat"), TEXT("ClampMax"), TEXT("1.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ClampedDouble"), TEXT("ClampMin"), TEXT("-10.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ClampedDouble"), TEXT("ClampMax"), TEXT("10.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("OnlyMinInt"), TEXT("ClampMin"), TEXT("0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyLacksMeta(*TestRunner, ScriptClass, TEXT("OnlyMinInt"), TEXT("ClampMax"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("OnlyMaxInt"), TEXT("ClampMax"), TEXT("255"))));
		ASSERT_THAT(IsTrue(ExpectPropertyLacksMeta(*TestRunner, ScriptClass, TEXT("OnlyMaxInt"), TEXT("ClampMin"))));
#endif
	}

	// -------------------------------------------------------------------------
	// UIMin / UIMax: UI slider range metadata (doesn't enforce hard limits).
	// Used to control the slider range in the editor without clamping values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UIMinMaxMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_UIRange"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaUIRange.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaUIRangeActor : AActor
			{
				UPROPERTY(meta = (UIMin = "0", UIMax = "100"))
				int UIRangedInt = 50;

				UPROPERTY(meta = (UIMin = "0.0", UIMax = "1.0"))
				float UIRangedFloat = 0.5f;

				UPROPERTY(meta = (UIMin = "-180.0", UIMax = "180.0"))
				double UIRangedDouble = 0.0;

				UPROPERTY(meta = (ClampMin = "0", ClampMax = "255", UIMin = "0", UIMax = "100"))
				int ComboRangedInt = 50;
			}
			)AS"),
			TEXT("ACoverageMetaUIRangeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Meta UI range actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("UIRangedInt"), TEXT("UIMin"), TEXT("0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("UIRangedInt"), TEXT("UIMax"), TEXT("100"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("UIRangedFloat"), TEXT("UIMin"), TEXT("0.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("UIRangedFloat"), TEXT("UIMax"), TEXT("1.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("UIRangedDouble"), TEXT("UIMin"), TEXT("-180.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("UIRangedDouble"), TEXT("UIMax"), TEXT("180.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ComboRangedInt"), TEXT("ClampMin"), TEXT("0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ComboRangedInt"), TEXT("ClampMax"), TEXT("255"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ComboRangedInt"), TEXT("UIMin"), TEXT("0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("ComboRangedInt"), TEXT("UIMax"), TEXT("100"))));
#endif
	}

	// -------------------------------------------------------------------------
	// Units: Display units metadata for numeric properties.
	// Common units: Degrees, Radians, Centimeters, Meters, Seconds, etc.
	// -------------------------------------------------------------------------
	TEST_METHOD(UnitsMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_Units"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaUnits.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaUnitsActor : AActor
			{
				UPROPERTY(meta = (Units = "Degrees"))
				float AngleDegrees = 90.0f;

				UPROPERTY(meta = (Units = "Radians"))
				float AngleRadians = 1.5708f;

				UPROPERTY(meta = (Units = "Centimeters"))
				float DistanceCM = 100.0f;

				UPROPERTY(meta = (Units = "Meters"))
				float DistanceM = 1.0f;

				UPROPERTY(meta = (Units = "Seconds"))
				float TimeSeconds = 5.0f;

				UPROPERTY(meta = (Units = "Milliseconds"))
				float TimeMS = 5000.0f;

				UPROPERTY(meta = (Units = "Percent"))
				float Percentage = 75.0f;

				UPROPERTY(meta = (Units = "kg"))
				float MassKG = 10.0f;
			}
			)AS"),
			TEXT("ACoverageMetaUnitsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Meta units actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("AngleDegrees"), TEXT("Units"), TEXT("Degrees"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("AngleRadians"), TEXT("Units"), TEXT("Radians"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("DistanceCM"), TEXT("Units"), TEXT("Centimeters"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("DistanceM"), TEXT("Units"), TEXT("Meters"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("TimeSeconds"), TEXT("Units"), TEXT("Seconds"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("TimeMS"), TEXT("Units"), TEXT("Milliseconds"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("Percentage"), TEXT("Units"), TEXT("Percent"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("MassKG"), TEXT("Units"), TEXT("kg"))));
#endif
	}

	// -------------------------------------------------------------------------
	// DisplayName: Override property display name in the editor.
	// -------------------------------------------------------------------------
	TEST_METHOD(DisplayNameMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_DisplayName"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaDisplayName.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaDisplayNameActor : AActor
			{
				UPROPERTY(meta = (DisplayName = "Health Points"))
				int HP = 100;

				UPROPERTY(meta = (DisplayName = "Movement Speed (m/s)"))
				float Speed = 5.0f;

				UPROPERTY(meta = (DisplayName = "Player Name"))
				FString PlayerName = "John";

				UPROPERTY(meta = (DisplayName = "Is Active?"))
				bool bActive = true;
			}
			)AS"),
			TEXT("ACoverageMetaDisplayNameActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Meta DisplayName actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("HP"), TEXT("DisplayName"), TEXT("Health Points"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("Speed"), TEXT("DisplayName"), TEXT("Movement Speed (m/s)"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("PlayerName"), TEXT("DisplayName"), TEXT("Player Name"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("bActive"), TEXT("DisplayName"), TEXT("Is Active?"))));
#endif
	}

	// -------------------------------------------------------------------------
	// EditCondition: Conditional editing based on another property.
	// The property is only editable when the condition evaluates to true.
	// -------------------------------------------------------------------------
	TEST_METHOD(EditConditionMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_EditCondition"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaEditCondition.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaEditConditionActor : AActor
			{
				UPROPERTY()
				bool bEnableHealth = true;

				UPROPERTY(meta = (EditCondition = "bEnableHealth"))
				int Health = 100;

				UPROPERTY()
				bool bEnableSpeed = false;

				UPROPERTY(meta = (EditCondition = "bEnableSpeed"))
				float Speed = 5.0f;

				UPROPERTY()
				bool bEnableDamage = true;

				UPROPERTY(meta = (EditCondition = "bEnableDamage"))
				float DamageMultiplier = 1.5f;
			}
			)AS"),
			TEXT("ACoverageMetaEditConditionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Meta EditCondition actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("Health"), TEXT("EditCondition"), TEXT("bEnableHealth"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("Speed"), TEXT("EditCondition"), TEXT("bEnableSpeed"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("DamageMultiplier"), TEXT("EditCondition"), TEXT("bEnableDamage"))));
#endif
	}

	// -------------------------------------------------------------------------
	// InlineEditConditionToggle: Property acts as its own edit condition toggle.
	// This meta specifier is applied to the bool property that acts as the gate.
	// -------------------------------------------------------------------------
	TEST_METHOD(InlineEditConditionToggleMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_InlineToggle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaInlineToggle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaInlineToggleActor : AActor
			{
				UPROPERTY(meta = (InlineEditConditionToggle))
				bool bEnableHealth = true;

				UPROPERTY(meta = (EditCondition = "bEnableHealth"))
				int Health = 100;

				UPROPERTY(meta = (InlineEditConditionToggle))
				bool bEnableSpeed = false;

				UPROPERTY(meta = (EditCondition = "bEnableSpeed"))
				float Speed = 5.0f;
			}
			)AS"),
			TEXT("ACoverageMetaInlineToggleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Meta InlineEditConditionToggle actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		ASSERT_THAT(IsTrue(ExpectPropertyHasMeta(*TestRunner, ScriptClass, TEXT("bEnableHealth"), TEXT("InlineEditConditionToggle"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("Health"), TEXT("EditCondition"), TEXT("bEnableHealth"))));
		ASSERT_THAT(IsTrue(ExpectPropertyHasMeta(*TestRunner, ScriptClass, TEXT("bEnableSpeed"), TEXT("InlineEditConditionToggle"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("Speed"), TEXT("EditCondition"), TEXT("bEnableSpeed"))));
#endif
	}

	TEST_METHOD(FloatEditorMetaSpecifierRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_FloatEditor"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaFloatEditor.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaFloatEditorActor : AActor
			{
				UPROPERTY(meta = (ClampMin = "-45.5", ClampMax = "45.5"))
				float PitchDegrees = 0.0f;

				UPROPERTY(meta = (UIMin = "0.25", UIMax = "250.75"))
				float SliderValue = 100.0f;

				UPROPERTY(meta = (ClampMin = "0.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "900.0", Units = "Centimeters"))
				float TravelDistance = 125.0f;

				UPROPERTY(meta = (Units = "Degrees"))
				float HeadingDegrees = 90.0f;

				UPROPERTY(meta = (ClampMin = "-1.25", ClampMax = "1.25", UIMin = "-1.0", UIMax = "1.0", Units = "Degrees"))
				double NormalizedAngle = 0.0;
			}
			)AS"),
			TEXT("ACoverageMetaFloatEditorActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Float editor meta actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("PitchDegrees"), TEXT("ClampMin"), TEXT("-45.5"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("PitchDegrees"), TEXT("ClampMax"), TEXT("45.5"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("SliderValue"), TEXT("UIMin"), TEXT("0.25"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("SliderValue"), TEXT("UIMax"), TEXT("250.75"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("TravelDistance"), TEXT("ClampMin"), TEXT("0.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("TravelDistance"), TEXT("ClampMax"), TEXT("1000.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("TravelDistance"), TEXT("UIMin"), TEXT("10.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("TravelDistance"), TEXT("UIMax"), TEXT("900.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("TravelDistance"), TEXT("Units"), TEXT("Centimeters"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("HeadingDegrees"), TEXT("Units"), TEXT("Degrees"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("NormalizedAngle"), TEXT("ClampMin"), TEXT("-1.25"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("NormalizedAngle"), TEXT("ClampMax"), TEXT("1.25"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("NormalizedAngle"), TEXT("UIMin"), TEXT("-1.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("NormalizedAngle"), TEXT("UIMax"), TEXT("1.0"))));
		ASSERT_THAT(IsTrue(ExpectPropertyMeta(*TestRunner, ScriptClass, TEXT("NormalizedAngle"), TEXT("Units"), TEXT("Degrees"))));
#endif
	}

	TEST_METHOD(UFunctionBasicSpecifiersAndFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_UFunctionFlags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaUFunctionFlags.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaUFunctionFlagsActor : AActor
			{
				UPROPERTY()
				int Value = 7;

				UFUNCTION(NotBlueprintCallable)
				void BasicMethod()
				{
					Value += 1;
				}

				UFUNCTION(BlueprintCallable, Category = "Coverage|Functions")
				int CallableAdd(int Input)
				{
					return Value + Input;
				}

				UFUNCTION(BlueprintPure)
				int PureValue() const
				{
					return Value;
				}

				UFUNCTION(CallInEditor, Exec)
				void EditorExecMethod()
				{
					Value += 10;
				}
			}
			)AS"),
			TEXT("ACoverageMetaUFunctionFlagsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION specifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UFUNCTION specifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		UFunction* BasicMethod = RequireFunction(ScriptClass, TEXT("BasicMethod"));
		UFunction* CallableAdd = RequireFunction(ScriptClass, TEXT("CallableAdd"));
		UFunction* PureValue = RequireFunction(ScriptClass, TEXT("PureValue"));
		UFunction* EditorExecMethod = RequireFunction(ScriptClass, TEXT("EditorExecMethod"));
		ASSERT_THAT(IsNotNull(BasicMethod, TEXT("Basic UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(CallableAdd, TEXT("BlueprintCallable UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(PureValue, TEXT("BlueprintPure UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(EditorExecMethod, TEXT("CallInEditor/Exec UFUNCTION should be generated")));
		if (BasicMethod == nullptr || CallableAdd == nullptr || PureValue == nullptr || EditorExecMethod == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(BasicMethod->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("NotBlueprintCallable should suppress the default BlueprintCallable flag")));
		ASSERT_THAT(IsTrue(CallableAdd->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintCallable should set FUNC_BlueprintCallable")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Functions")), CallableAdd->GetMetaData(TEXT("Category")),
			TEXT("UFUNCTION Category should be preserved as function metadata")));
		ASSERT_THAT(IsTrue(PureValue->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("BlueprintPure should set FUNC_BlueprintPure")));
		ASSERT_THAT(IsTrue(PureValue->HasAnyFunctionFlags(FUNC_Const),
			TEXT("const UFUNCTION should set FUNC_Const")));
		ASSERT_THAT(IsTrue(EditorExecMethod->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("Exec should set FUNC_Exec")));
		ASSERT_THAT(IsTrue(EditorExecMethod->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor should be preserved as function metadata")));

		FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("CallableAdd"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("CallableAdd should be invokable through reflection")));
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddParam<int32>(5);
		const int32 AddResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(12, AddResult, TEXT("BlueprintCallable UFUNCTION should execute through reflected invocation")));
	}

	TEST_METHOD(UFunctionDisplayAndParameterMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_UFunctionMeta"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaUFunctionMeta.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaUFunctionMetaActor : AActor
			{
				UFUNCTION(BlueprintCallable, Category = "Coverage|Meta", meta = (
					DisplayName = "Apply Meta Value",
					Keywords = "coverage meta function",
					ToolTip = "Applies metadata",
					ShortToolTip = "Apply meta",
					CompactNodeTitle = "META",
					AdvancedDisplay = "Scale,Offset",
					AutoCreateRefTerm = "Label"))
				int ApplyMetaValue(
					UPARAM(DisplayName = "Input Value") int Input,
					UPARAM(DisplayName = "Scale Value") int Scale,
					UPARAM(DisplayName = "Offset Value") int Offset,
					UPARAM(DisplayName = "Label Text") const FString&in Label)
				{
					return Input * Scale + Offset + Label.Len();
				}
			}
			)AS"),
			TEXT("ACoverageMetaUFunctionMetaActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION meta actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ApplyMetaValue = RequireFunction(ScriptClass, TEXT("ApplyMetaValue"));
		ASSERT_THAT(IsNotNull(ApplyMetaValue, TEXT("ApplyMetaValue UFUNCTION should be generated")));
		if (ApplyMetaValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Meta")), ApplyMetaValue->GetMetaData(TEXT("Category")),
			TEXT("UFUNCTION Category should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("Apply Meta Value")), ApplyMetaValue->GetMetaData(TEXT("DisplayName")),
			TEXT("UFUNCTION DisplayName meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("coverage meta function")), ApplyMetaValue->GetMetaData(TEXT("Keywords")),
			TEXT("UFUNCTION Keywords meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("Applies metadata")), ApplyMetaValue->GetMetaData(TEXT("ToolTip")),
			TEXT("UFUNCTION ToolTip meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("Apply meta")), ApplyMetaValue->GetMetaData(TEXT("ShortToolTip")),
			TEXT("UFUNCTION ShortToolTip meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("META")), ApplyMetaValue->GetMetaData(TEXT("CompactNodeTitle")),
			TEXT("UFUNCTION CompactNodeTitle meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("Scale,Offset")), ApplyMetaValue->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("UFUNCTION AdvancedDisplay meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label")), ApplyMetaValue->GetMetaData(TEXT("AutoCreateRefTerm")),
			TEXT("UFUNCTION AutoCreateRefTerm meta should be reflected")));

		FProperty* InputParam = RequireFunctionParam(ApplyMetaValue, TEXT("Input"));
		FProperty* ScaleParam = RequireFunctionParam(ApplyMetaValue, TEXT("Scale"));
		FProperty* OffsetParam = RequireFunctionParam(ApplyMetaValue, TEXT("Offset"));
		FProperty* LabelParam = RequireFunctionParam(ApplyMetaValue, TEXT("Label"));
		ASSERT_THAT(IsNotNull(InputParam, TEXT("Input parameter should be reflected")));
		ASSERT_THAT(IsNotNull(ScaleParam, TEXT("Scale parameter should be reflected")));
		ASSERT_THAT(IsNotNull(OffsetParam, TEXT("Offset parameter should be reflected")));
		ASSERT_THAT(IsNotNull(LabelParam, TEXT("Label parameter should be reflected")));
		if (InputParam == nullptr || ScaleParam == nullptr || OffsetParam == nullptr || LabelParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Input Value")), InputParam->GetMetaData(TEXT("DisplayName")),
			TEXT("UPARAM DisplayName should be reflected on Input")));
		ASSERT_THAT(AreEqual(FString(TEXT("Scale Value")), ScaleParam->GetMetaData(TEXT("DisplayName")),
			TEXT("UPARAM DisplayName should be reflected on Scale")));
		ASSERT_THAT(AreEqual(FString(TEXT("Offset Value")), OffsetParam->GetMetaData(TEXT("DisplayName")),
			TEXT("UPARAM DisplayName should be reflected on Offset")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label Text")), LabelParam->GetMetaData(TEXT("DisplayName")),
			TEXT("UPARAM DisplayName should be reflected on Label")));
		ASSERT_THAT(IsTrue(ScaleParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should set CPF_AdvancedDisplay on Scale parameter")));
		ASSERT_THAT(IsTrue(OffsetParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should set CPF_AdvancedDisplay on Offset parameter")));
	}

	TEST_METHOD(UFunctionWorldContextAndPinMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_UFunctionPinMeta"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaUFunctionPinMeta.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageMetaFunctionPinStatics : UObject
			{
				UFUNCTION(BlueprintCallable, meta = (
					WorldContext = "WorldContextObject",
					DefaultToSelf = "WorldContextObject",
					HidePin = "WorldContextObject",
					AdvancedDisplay = "OptionalValue"))
				static int CoveragePinMetaFunction(UObject WorldContextObject, int RequiredValue, int OptionalValue)
				{
					return RequiredValue + OptionalValue;
				}
			}
			)AS"),
			TEXT("UCoverageMetaFunctionPinStatics"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION pin meta statics class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* PinMetaFunction = RequireFunction(ScriptClass, TEXT("CoveragePinMetaFunction"));
		ASSERT_THAT(IsNotNull(PinMetaFunction, TEXT("Static UFUNCTION pin-meta function should be generated")));
		if (PinMetaFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("WorldContextObject")), PinMetaFunction->GetMetaData(TEXT("WorldContext")),
			TEXT("WorldContext meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("WorldContextObject")), PinMetaFunction->GetMetaData(TEXT("DefaultToSelf")),
			TEXT("DefaultToSelf meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("WorldContextObject")), PinMetaFunction->GetMetaData(TEXT("HidePin")),
			TEXT("HidePin meta should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionalValue")), PinMetaFunction->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("AdvancedDisplay parameter meta should be reflected")));

		FProperty* WorldContextParam = RequireFunctionParam(PinMetaFunction, TEXT("WorldContextObject"));
		FProperty* OptionalParam = RequireFunctionParam(PinMetaFunction, TEXT("OptionalValue"));
		ASSERT_THAT(IsNotNull(WorldContextParam, TEXT("WorldContextObject parameter should be reflected")));
		ASSERT_THAT(IsNotNull(OptionalParam, TEXT("OptionalValue parameter should be reflected")));
		if (WorldContextParam == nullptr || OptionalParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(IsAngelscriptWorldContextProperty(WorldContextParam),
			TEXT("WorldContextObject should be classified as an AS world-context parameter")));
		ASSERT_THAT(IsTrue(OptionalParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should set CPF_AdvancedDisplay on OptionalValue")));
	}

	TEST_METHOD(UFunctionRecursion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMetaSpecifier_UFunctionRecursion"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMetaUFunctionRecursion.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMetaUFunctionRecursionActor : AActor
			{
				UFUNCTION(BlueprintCallable)
				int Factorial(int Value) const
				{
					if (Value <= 1)
					{
						return 1;
					}

					return Value * Factorial(Value - 1);
				}
			}
			)AS"),
			TEXT("ACoverageMetaUFunctionRecursionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Recursive UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Recursive UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		UFunction* FactorialFunction = RequireFunction(ScriptClass, TEXT("Factorial"));
		ASSERT_THAT(IsNotNull(FactorialFunction, TEXT("Factorial UFUNCTION should be generated")));
		if (FactorialFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(FactorialFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Recursive BlueprintCallable should set FUNC_BlueprintCallable")));
		ASSERT_THAT(IsTrue(FactorialFunction->HasAnyFunctionFlags(FUNC_Const),
			TEXT("Recursive const UFUNCTION should set FUNC_Const")));

		FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("Factorial"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Factorial should be invokable through reflection")));
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddParam<int32>(5);
		const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(120, Result, TEXT("Recursive UFUNCTION should call itself and return 5!")));
	}
};

#endif
