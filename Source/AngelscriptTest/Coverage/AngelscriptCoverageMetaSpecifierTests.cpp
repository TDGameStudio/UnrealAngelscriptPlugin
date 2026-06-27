#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

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

#if WITH_EDITOR
		auto FindProp = [&](const TCHAR* Name) -> const FProperty*
		{
			const FProperty* Found = ScriptClass->FindPropertyByName(FName(Name));
			if (Found == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("Meta property '%s' should exist"), Name));
			}
			return Found;
		};

		// --- ClampedInt: ClampMin + ClampMax ---
		if (const FProperty* ClampedInt = FindProp(TEXT("ClampedInt")))
		{
			TestRunner->TestEqual(TEXT("ClampedInt ClampMin meta"), ClampedInt->GetMetaData(TEXT("ClampMin")), FString(TEXT("0")));
			TestRunner->TestEqual(TEXT("ClampedInt ClampMax meta"), ClampedInt->GetMetaData(TEXT("ClampMax")), FString(TEXT("100")));
		}

		// --- ClampedFloat: float with ClampMin + ClampMax ---
		if (const FProperty* ClampedFloat = FindProp(TEXT("ClampedFloat")))
		{
			TestRunner->TestEqual(TEXT("ClampedFloat ClampMin meta"), ClampedFloat->GetMetaData(TEXT("ClampMin")), FString(TEXT("0.0")));
			TestRunner->TestEqual(TEXT("ClampedFloat ClampMax meta"), ClampedFloat->GetMetaData(TEXT("ClampMax")), FString(TEXT("1.0")));
		}

		// --- ClampedDouble: double with negative ClampMin ---
		if (const FProperty* ClampedDouble = FindProp(TEXT("ClampedDouble")))
		{
			TestRunner->TestEqual(TEXT("ClampedDouble ClampMin meta"), ClampedDouble->GetMetaData(TEXT("ClampMin")), FString(TEXT("-10.0")));
			TestRunner->TestEqual(TEXT("ClampedDouble ClampMax meta"), ClampedDouble->GetMetaData(TEXT("ClampMax")), FString(TEXT("10.0")));
		}

		// --- OnlyMinInt: ClampMin only (no max) ---
		if (const FProperty* OnlyMinInt = FindProp(TEXT("OnlyMinInt")))
		{
			TestRunner->TestEqual(TEXT("OnlyMinInt ClampMin meta"), OnlyMinInt->GetMetaData(TEXT("ClampMin")), FString(TEXT("0")));
			TestRunner->TestTrue(TEXT("OnlyMinInt should not have ClampMax"), OnlyMinInt->GetMetaData(TEXT("ClampMax")).IsEmpty());
		}

		// --- OnlyMaxInt: ClampMax only (no min) ---
		if (const FProperty* OnlyMaxInt = FindProp(TEXT("OnlyMaxInt")))
		{
			TestRunner->TestEqual(TEXT("OnlyMaxInt ClampMax meta"), OnlyMaxInt->GetMetaData(TEXT("ClampMax")), FString(TEXT("255")));
			TestRunner->TestTrue(TEXT("OnlyMaxInt should not have ClampMin"), OnlyMaxInt->GetMetaData(TEXT("ClampMin")).IsEmpty());
		}
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

#if WITH_EDITOR
		auto FindProp = [&](const TCHAR* Name) -> const FProperty*
		{
			const FProperty* Found = ScriptClass->FindPropertyByName(FName(Name));
			if (Found == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("Meta property '%s' should exist"), Name));
			}
			return Found;
		};

		// --- UIRangedInt: UIMin + UIMax ---
		if (const FProperty* UIRangedInt = FindProp(TEXT("UIRangedInt")))
		{
			TestRunner->TestEqual(TEXT("UIRangedInt UIMin meta"), UIRangedInt->GetMetaData(TEXT("UIMin")), FString(TEXT("0")));
			TestRunner->TestEqual(TEXT("UIRangedInt UIMax meta"), UIRangedInt->GetMetaData(TEXT("UIMax")), FString(TEXT("100")));
		}

		// --- UIRangedFloat: float with UIMin + UIMax ---
		if (const FProperty* UIRangedFloat = FindProp(TEXT("UIRangedFloat")))
		{
			TestRunner->TestEqual(TEXT("UIRangedFloat UIMin meta"), UIRangedFloat->GetMetaData(TEXT("UIMin")), FString(TEXT("0.0")));
			TestRunner->TestEqual(TEXT("UIRangedFloat UIMax meta"), UIRangedFloat->GetMetaData(TEXT("UIMax")), FString(TEXT("1.0")));
		}

		// --- UIRangedDouble: double with negative UIMin ---
		if (const FProperty* UIRangedDouble = FindProp(TEXT("UIRangedDouble")))
		{
			TestRunner->TestEqual(TEXT("UIRangedDouble UIMin meta"), UIRangedDouble->GetMetaData(TEXT("UIMin")), FString(TEXT("-180.0")));
			TestRunner->TestEqual(TEXT("UIRangedDouble UIMax meta"), UIRangedDouble->GetMetaData(TEXT("UIMax")), FString(TEXT("180.0")));
		}

		// --- ComboRangedInt: ClampMin/ClampMax + UIMin/UIMax (different ranges) ---
		if (const FProperty* ComboRangedInt = FindProp(TEXT("ComboRangedInt")))
		{
			TestRunner->TestEqual(TEXT("ComboRangedInt ClampMin meta"), ComboRangedInt->GetMetaData(TEXT("ClampMin")), FString(TEXT("0")));
			TestRunner->TestEqual(TEXT("ComboRangedInt ClampMax meta"), ComboRangedInt->GetMetaData(TEXT("ClampMax")), FString(TEXT("255")));
			TestRunner->TestEqual(TEXT("ComboRangedInt UIMin meta"), ComboRangedInt->GetMetaData(TEXT("UIMin")), FString(TEXT("0")));
			TestRunner->TestEqual(TEXT("ComboRangedInt UIMax meta"), ComboRangedInt->GetMetaData(TEXT("UIMax")), FString(TEXT("100")));
		}
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

#if WITH_EDITOR
		auto FindProp = [&](const TCHAR* Name) -> const FProperty*
		{
			const FProperty* Found = ScriptClass->FindPropertyByName(FName(Name));
			if (Found == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("Meta property '%s' should exist"), Name));
			}
			return Found;
		};

		// --- AngleDegrees: Units = Degrees ---
		if (const FProperty* AngleDegrees = FindProp(TEXT("AngleDegrees")))
		{
			TestRunner->TestEqual(TEXT("AngleDegrees Units meta"), AngleDegrees->GetMetaData(TEXT("Units")), FString(TEXT("Degrees")));
		}

		// --- AngleRadians: Units = Radians ---
		if (const FProperty* AngleRadians = FindProp(TEXT("AngleRadians")))
		{
			TestRunner->TestEqual(TEXT("AngleRadians Units meta"), AngleRadians->GetMetaData(TEXT("Units")), FString(TEXT("Radians")));
		}

		// --- DistanceCM: Units = Centimeters ---
		if (const FProperty* DistanceCM = FindProp(TEXT("DistanceCM")))
		{
			TestRunner->TestEqual(TEXT("DistanceCM Units meta"), DistanceCM->GetMetaData(TEXT("Units")), FString(TEXT("Centimeters")));
		}

		// --- DistanceM: Units = Meters ---
		if (const FProperty* DistanceM = FindProp(TEXT("DistanceM")))
		{
			TestRunner->TestEqual(TEXT("DistanceM Units meta"), DistanceM->GetMetaData(TEXT("Units")), FString(TEXT("Meters")));
		}

		// --- TimeSeconds: Units = Seconds ---
		if (const FProperty* TimeSeconds = FindProp(TEXT("TimeSeconds")))
		{
			TestRunner->TestEqual(TEXT("TimeSeconds Units meta"), TimeSeconds->GetMetaData(TEXT("Units")), FString(TEXT("Seconds")));
		}

		// --- TimeMS: Units = Milliseconds ---
		if (const FProperty* TimeMS = FindProp(TEXT("TimeMS")))
		{
			TestRunner->TestEqual(TEXT("TimeMS Units meta"), TimeMS->GetMetaData(TEXT("Units")), FString(TEXT("Milliseconds")));
		}

		// --- Percentage: Units = Percent ---
		if (const FProperty* Percentage = FindProp(TEXT("Percentage")))
		{
			TestRunner->TestEqual(TEXT("Percentage Units meta"), Percentage->GetMetaData(TEXT("Units")), FString(TEXT("Percent")));
		}

		// --- MassKG: Units = kg (custom unit) ---
		if (const FProperty* MassKG = FindProp(TEXT("MassKG")))
		{
			TestRunner->TestEqual(TEXT("MassKG Units meta"), MassKG->GetMetaData(TEXT("Units")), FString(TEXT("kg")));
		}
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

#if WITH_EDITOR
		auto FindProp = [&](const TCHAR* Name) -> const FProperty*
		{
			const FProperty* Found = ScriptClass->FindPropertyByName(FName(Name));
			if (Found == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("Meta property '%s' should exist"), Name));
			}
			return Found;
		};

		// --- HP: DisplayName = "Health Points" ---
		if (const FProperty* HP = FindProp(TEXT("HP")))
		{
			TestRunner->TestEqual(TEXT("HP DisplayName meta"), HP->GetMetaData(TEXT("DisplayName")), FString(TEXT("Health Points")));
		}

		// --- Speed: DisplayName with special characters ---
		if (const FProperty* Speed = FindProp(TEXT("Speed")))
		{
			TestRunner->TestEqual(TEXT("Speed DisplayName meta"), Speed->GetMetaData(TEXT("DisplayName")), FString(TEXT("Movement Speed (m/s)")));
		}

		// --- PlayerName: DisplayName on FString ---
		if (const FProperty* PlayerName = FindProp(TEXT("PlayerName")))
		{
			TestRunner->TestEqual(TEXT("PlayerName DisplayName meta"), PlayerName->GetMetaData(TEXT("DisplayName")), FString(TEXT("Player Name")));
		}

		// --- bActive: DisplayName on bool ---
		if (const FProperty* bActive = FindProp(TEXT("bActive")))
		{
			TestRunner->TestEqual(TEXT("bActive DisplayName meta"), bActive->GetMetaData(TEXT("DisplayName")), FString(TEXT("Is Active?")));
		}
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

#if WITH_EDITOR
		auto FindProp = [&](const TCHAR* Name) -> const FProperty*
		{
			const FProperty* Found = ScriptClass->FindPropertyByName(FName(Name));
			if (Found == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("Meta property '%s' should exist"), Name));
			}
			return Found;
		};

		// --- Health: EditCondition = "bEnableHealth" ---
		if (const FProperty* Health = FindProp(TEXT("Health")))
		{
			TestRunner->TestEqual(TEXT("Health EditCondition meta"), Health->GetMetaData(TEXT("EditCondition")), FString(TEXT("bEnableHealth")));
		}

		// --- Speed: EditCondition = "bEnableSpeed" ---
		if (const FProperty* Speed = FindProp(TEXT("Speed")))
		{
			TestRunner->TestEqual(TEXT("Speed EditCondition meta"), Speed->GetMetaData(TEXT("EditCondition")), FString(TEXT("bEnableSpeed")));
		}

		// --- DamageMultiplier: EditCondition = "bEnableDamage" ---
		if (const FProperty* DamageMultiplier = FindProp(TEXT("DamageMultiplier")))
		{
			TestRunner->TestEqual(TEXT("DamageMultiplier EditCondition meta"), DamageMultiplier->GetMetaData(TEXT("EditCondition")), FString(TEXT("bEnableDamage")));
		}
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

#if WITH_EDITOR
		auto FindProp = [&](const TCHAR* Name) -> const FProperty*
		{
			const FProperty* Found = ScriptClass->FindPropertyByName(FName(Name));
			if (Found == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("Meta property '%s' should exist"), Name));
			}
			return Found;
		};

		// --- bEnableHealth: InlineEditConditionToggle ---
		if (const FProperty* bEnableHealth = FindProp(TEXT("bEnableHealth")))
		{
			TestRunner->TestEqual(TEXT("bEnableHealth InlineEditConditionToggle meta"),
				bEnableHealth->GetMetaData(TEXT("InlineEditConditionToggle")), FString(TEXT("true")));
		}

		// --- Health: EditCondition = "bEnableHealth" ---
		if (const FProperty* Health = FindProp(TEXT("Health")))
		{
			TestRunner->TestEqual(TEXT("Health EditCondition meta"),
				Health->GetMetaData(TEXT("EditCondition")), FString(TEXT("bEnableHealth")));
		}

		// --- bEnableSpeed: InlineEditConditionToggle ---
		if (const FProperty* bEnableSpeed = FindProp(TEXT("bEnableSpeed")))
		{
			TestRunner->TestEqual(TEXT("bEnableSpeed InlineEditConditionToggle meta"),
				bEnableSpeed->GetMetaData(TEXT("InlineEditConditionToggle")), FString(TEXT("true")));
		}

		// --- Speed: EditCondition = "bEnableSpeed" ---
		if (const FProperty* Speed = FindProp(TEXT("Speed")))
		{
			TestRunner->TestEqual(TEXT("Speed EditCondition meta"),
				Speed->GetMetaData(TEXT("EditCondition")), FString(TEXT("bEnableSpeed")));
		}
#endif
	}
};

#endif
