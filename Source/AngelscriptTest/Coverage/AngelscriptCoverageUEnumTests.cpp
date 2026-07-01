#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageUEnumTests
// -----------------------------------------------------------------------------
// Comprehensive coverage for AngelScript UENUM declarations and usage.
// Based on OpenSpec: test-coverage/coverage-matrix.md section 2 (UENUM).
//
// Coverage matrix:
//   * UEnumBasicDeclaration     - enum / UENUM() / explicit values / namespace
//   * UEnumSpecifiers           - BlueprintType / Category / DisplayName / ToolTip
//   * UEnumMeta                 - DisplayName / ToolTip / Hidden (UMETA)
//   * UEnumUsage                - local / UPROPERTY / function param / return
//   * UEnumSwitch               - switch statement with enum cases
//   * UEnumConversion           - int <-> enum conversions
//   * UEnumBitflags             - bitwise operations on enums (| & ^ ~)
//   * UEnumInContainers         - TArray<EEnum> / TMap<EEnum, *> / TMap<*, EEnum>
//   * UEnumClassUsage           - enum class / explicit underlying type
//   * UEnumInvalidDiagnostics   - unsupported/invalid enum compile diagnostics
//
// Note: AngelScript does not support the Bitflags UENUM specifier. Bitwise
// operations are performed by converting enum values to int.
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUEnumTest,
	"Angelscript.TestModule.Coverage.UEnum",
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
	// Basic enum and UENUM() declarations with explicit values and namespace.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumBasicDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_BasicDecl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumBasicDecl.as"),
			ASTEST_AS(R"AS(
			// Plain enum without UENUM
			enum EPlainEnum
			{
				PlainA,
				PlainB,
				PlainC
			}

			// UENUM() with default increment
			UENUM()
			enum EDefaultIncrement
			{
				First,
				Second,
				Third
			}

			// UENUM() with explicit values
			UENUM()
			enum EExplicitValues
			{
				Low = 1,
				Medium = 5,
				High = 10
			}

			// Namespace enum
			namespace CoverageNS
			{
				enum ENamespaceEnum
				{
					NSValueA,
					NSValueB
				}
			}

			UCLASS()
			class ACoverageUEnumBasicActor : AActor
			{
				UPROPERTY()
				EDefaultIncrement DefaultValue = EDefaultIncrement::Second;

				UPROPERTY()
				EExplicitValues ExplicitValue = EExplicitValues::Medium;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Verify default increment (0, 1, 2)
					int FirstVal = int(EDefaultIncrement::First);
					int SecondVal = int(EDefaultIncrement::Second);
					int ThirdVal = int(EDefaultIncrement::Third);
					check(FirstVal == 0);
					check(SecondVal == 1);
					check(ThirdVal == 2);

					// Verify explicit values
					check(int(EExplicitValues::Low) == 1);
					check(int(EExplicitValues::Medium) == 5);
					check(int(EExplicitValues::High) == 10);

					// Verify namespace enum
					CoverageNS::ENamespaceEnum NSVal = CoverageNS::ENamespaceEnum::NSValueB;
					check(int(NSVal) == 1);

					// Verify plain enum
					EPlainEnum PlainVal = EPlainEnum::PlainB;
					check(int(PlainVal) == 1);
				}
			}
			)AS"),
			TEXT("ACoverageUEnumBasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum basic declaration actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum basic declaration actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify UPROPERTY defaults via reflection
		FEnumProperty* DefaultProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("DefaultValue"));
		ASSERT_THAT(IsNotNull(DefaultProp, TEXT("DefaultValue property should exist")));
		if (DefaultProp == nullptr)
		{
			return;
		}

		uint8* DefaultValuePtr = DefaultProp->ContainerPtrToValuePtr<uint8>(Actor);
		int64 DefaultEnumValue = DefaultProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(DefaultValuePtr);
		ASSERT_THAT(AreEqual(1LL, DefaultEnumValue, TEXT("DefaultValue should be Second (1)")));

		FEnumProperty* ExplicitProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("ExplicitValue"));
		ASSERT_THAT(IsNotNull(ExplicitProp, TEXT("ExplicitValue property should exist")));
		if (ExplicitProp == nullptr)
		{
			return;
		}

		uint8* ExplicitValuePtr = ExplicitProp->ContainerPtrToValuePtr<uint8>(Actor);
		int64 ExplicitEnumValue = ExplicitProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ExplicitValuePtr);
		ASSERT_THAT(AreEqual(5LL, ExplicitEnumValue, TEXT("ExplicitValue should be Medium (5)")));
	}

	// -------------------------------------------------------------------------
	// UENUM specifiers: BlueprintType, Category, DisplayName, ToolTip.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumSpecifiers.as"),
			ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EBlueprintTypeEnum
			{
				BPOption1,
				BPOption2,
				BPOption3
			}

			UENUM(Category="MyCategory", DisplayName="My Enum Display", ToolTip="Enum with multiple specifiers")
			enum EMultiSpecifierEnum
			{
				Value1,
				Value2,
				Value3
			}

			UCLASS()
			class ACoverageUEnumSpecifiersActor : AActor
			{
				UPROPERTY()
				EBlueprintTypeEnum BPType = EBlueprintTypeEnum::BPOption1;

				UPROPERTY()
				EMultiSpecifierEnum MultiSpec = EMultiSpecifierEnum::Value2;
			}
			)AS"),
			TEXT("ACoverageUEnumSpecifiersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum specifiers actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum specifiers actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FEnumProperty* BPTypeProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("BPType"));
		ASSERT_THAT(IsNotNull(BPTypeProp, TEXT("BPType property should exist")));
		if (BPTypeProp == nullptr)
		{
			return;
		}

		UEnum* BPTypeEnum = BPTypeProp->GetEnum();
		ASSERT_THAT(IsNotNull(BPTypeEnum, TEXT("BPType should have UEnum")));
		if (BPTypeEnum == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(BPTypeEnum->HasMetaData(TEXT("BlueprintType")),
			TEXT("BlueprintType enum should expose BlueprintType metadata on generated UEnum")));

		FEnumProperty* MultiSpecProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("MultiSpec"));
		ASSERT_THAT(IsNotNull(MultiSpecProp, TEXT("MultiSpec property should exist")));
		if (MultiSpecProp == nullptr)
		{
			return;
		}

		UEnum* MultiSpecEnum = MultiSpecProp->GetEnum();
		ASSERT_THAT(IsNotNull(MultiSpecEnum, TEXT("MultiSpec should have UEnum")));
		if (MultiSpecEnum == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("MyCategory")), MultiSpecEnum->GetMetaData(TEXT("Category")),
			TEXT("UENUM Category specifier should be preserved as UEnum metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("My Enum Display")), MultiSpecEnum->GetMetaData(TEXT("DisplayName")),
			TEXT("UENUM DisplayName specifier should be preserved as UEnum metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Enum with multiple specifiers")), MultiSpecEnum->GetMetaData(TEXT("ToolTip")),
			TEXT("UENUM ToolTip specifier should be preserved as UEnum metadata")));
	}

	TEST_METHOD(UEnumMetaBitflags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_MetaBitflags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumMetaBitflags.as"),
			ASTEST_AS(R"AS(
			UENUM(meta = (Bitflags, BitmaskEnum = "EFlagMetaEnum"))
			enum EFlagMetaEnum
			{
				FlagA = 1,
				FlagB = 2,
				FlagC = 4
			}

			UCLASS()
			class ACoverageUEnumMetaBitflagsActor : AActor
			{
				UPROPERTY()
				EFlagMetaEnum Value = EFlagMetaEnum::FlagB;

				UPROPERTY(meta = (Bitmask, BitmaskEnum = "EFlagMetaEnum"))
				int ActiveFlags = 0;
			}
			)AS"),
			TEXT("ACoverageUEnumMetaBitflagsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum meta bitflags actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FEnumProperty* ValueProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ValueProp, TEXT("Value property should exist")));
		if (ValueProp == nullptr)
		{
			return;
		}

		UEnum* FlagEnum = ValueProp->GetEnum();
		ASSERT_THAT(IsNotNull(FlagEnum, TEXT("Value should have UEnum")));
		if (FlagEnum == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FlagEnum->HasMetaData(TEXT("Bitflags")),
			TEXT("UENUM meta=(Bitflags) should be preserved as UEnum metadata key presence")));
		ASSERT_THAT(AreEqual(FString(TEXT("EFlagMetaEnum")), FlagEnum->GetMetaData(TEXT("BitmaskEnum")),
			TEXT("UENUM meta=(BitmaskEnum=...) should be preserved as UEnum metadata")));

		FIntProperty* ActiveFlagsProp = FindFProperty<FIntProperty>(ScriptClass, TEXT("ActiveFlags"));
		ASSERT_THAT(IsNotNull(ActiveFlagsProp, TEXT("ActiveFlags bitmask property should exist")));
		if (ActiveFlagsProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ActiveFlagsProp->HasMetaData(TEXT("Bitmask")),
			TEXT("UPROPERTY meta=(Bitmask) should be preserved on bitmask integer property")));
		ASSERT_THAT(AreEqual(FString(TEXT("EFlagMetaEnum")), ActiveFlagsProp->GetMetaData(TEXT("BitmaskEnum")),
			TEXT("UPROPERTY meta=(BitmaskEnum=...) should preserve the linked UENUM name")));
	}

	TEST_METHOD(UEnumBitflagsSpecifierRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TestRunner->AddExpectedError(TEXT("Unknown enum specifier Bitflags"), EAutomationExpectedErrorFlags::Contains, 1);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM(Bitflags)
			enum EUnsupportedBitflagsSpecifier
			{
				FlagA = 1,
				FlagB = 2
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FName(TEXT("ASCoverageUEnum_BitflagsSpecifierRejected")),
			TEXT("ASCoverageUEnum_BitflagsSpecifierRejected.as"),
			ScriptSource,
			/*bUsePreprocessor=*/ true,
			Summary,
			/*bSuppressCompileErrorLogs=*/ true);

		ASSERT_THAT(IsFalse(bCompiled,
			TEXT("UENUM(Bitflags) should fail to compile as an explicit unsupported specifier boundary")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult,
			TEXT("UENUM(Bitflags) should surface Error compile result")));

		// Preprocessor MacroError diagnostics may not always land in Summary.Diagnostics;
		// scan both the compile trace summary and the engine diagnostic map.
		auto DiagnosticContains = [](const FString& Message, const TCHAR* Fragment) -> bool
		{
			return Message.Contains(Fragment);
		};

		bool bFoundDiagnostic = false;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			if (Diagnostic.bIsError && DiagnosticContains(Diagnostic.Message, TEXT("Unknown enum specifier Bitflags")))
			{
				bFoundDiagnostic = true;
				break;
			}
		}
		if (!bFoundDiagnostic)
		{
			for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& FileDiagnostics : Engine.Diagnostics)
			{
				for (const FAngelscriptEngine::FDiagnostic& Diagnostic : FileDiagnostics.Value.Diagnostics)
				{
					if (Diagnostic.bIsError && DiagnosticContains(Diagnostic.Message, TEXT("Unknown enum specifier Bitflags")))
					{
						bFoundDiagnostic = true;
						break;
					}
				}
				if (bFoundDiagnostic)
				{
					break;
				}
			}
		}

		ASSERT_THAT(IsTrue(bFoundDiagnostic,
			TEXT("UENUM(Bitflags) should report unknown enum specifier diagnostic")));

		Engine.DiscardModule(TEXT("ASCoverageUEnum_BitflagsSpecifierRejected"));
	}

	// -------------------------------------------------------------------------
	// UENUM meta: DisplayName, ToolTip, Hidden on enum entries.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumMeta)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_Meta"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumMeta.as"),
			ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EMetaEnum
			{
				OptionA UMETA(DisplayName="Option Alpha", ToolTip="This is option A"),
				OptionB UMETA(DisplayName="Option Beta"),
				OptionC UMETA(Hidden),
				OptionD
			}

			UCLASS()
			class ACoverageUEnumMetaActor : AActor
			{
				UPROPERTY()
				EMetaEnum Value = EMetaEnum::OptionA;
			}
			)AS"),
			TEXT("ACoverageUEnumMetaActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum meta actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum meta actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		// Verify meta enum property
		FEnumProperty* MetaProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(MetaProp, TEXT("Value property should exist")));
		if (MetaProp == nullptr)
		{
			return;
		}

		UEnum* MetaEnum = MetaProp->GetEnum();
		ASSERT_THAT(IsNotNull(MetaEnum, TEXT("Value should have UEnum")));
		if (MetaEnum == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(MetaEnum->NumEnums() >= 4, TEXT("MetaEnum should have at least 4 entries")));
		ASSERT_THAT(AreEqual(FString(TEXT("Option Alpha")), MetaEnum->GetMetaData(TEXT("DisplayName"), 0),
			TEXT("OptionA UMETA DisplayName should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("This is option A")), MetaEnum->GetMetaData(TEXT("ToolTip"), 0),
			TEXT("OptionA UMETA ToolTip should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Option Beta")), MetaEnum->GetDisplayNameTextByIndex(1).ToString(),
			TEXT("OptionB display text should use UMETA DisplayName")));
		ASSERT_THAT(IsTrue(MetaEnum->HasMetaData(TEXT("Hidden"), 2),
			TEXT("OptionC UMETA Hidden should be preserved as bool metadata")));
		ASSERT_THAT(IsFalse(MetaEnum->HasMetaData(TEXT("Hidden"), 3),
			TEXT("OptionD should not inherit Hidden metadata from OptionC")));
	}

	TEST_METHOD(UEnumClassUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_ClassUsage"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumClassUsage.as"),
			ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EClassScopedState : uint16
			{
				Idle,
				Armed = 7,
				Fired = 12
			}

			UCLASS()
			class ACoverageUEnumClassUsageActor : AActor
			{
				UPROPERTY()
				EClassScopedState CurrentState = EClassScopedState::Armed;

				UPROPERTY()
				int ScopedValue = 0;

				UPROPERTY()
				int ReturnedValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					EClassScopedState LocalState = EClassScopedState::Fired;
					check(LocalState == EClassScopedState::Fired);

					CurrentState = GetNextState(EClassScopedState::Idle);
					check(CurrentState == EClassScopedState::Armed);
					ScopedValue = int(CurrentState);
					ReturnedValue = int(LocalState);
				}

				EClassScopedState GetNextState(EClassScopedState State)
				{
					if (State == EClassScopedState::Idle)
						return EClassScopedState::Armed;

					return EClassScopedState::Fired;
				}
			}
			)AS"),
			TEXT("ACoverageUEnumClassUsageActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum class usage actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FEnumProperty* StateProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("CurrentState"));
		ASSERT_THAT(IsNotNull(StateProp, TEXT("CurrentState enum class property should exist")));
		if (StateProp == nullptr)
		{
			return;
		}

		UEnum* StateEnum = StateProp->GetEnum();
		ASSERT_THAT(IsNotNull(StateEnum, TEXT("CurrentState should have generated UEnum")));
		if (StateEnum == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(StateProp->GetUnderlyingProperty(),
			TEXT("enum class property should expose an underlying numeric property")));
		if (StateProp->GetUnderlyingProperty() == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StateEnum->GetMaxEnumValue() >= 12,
			TEXT("enum class explicit values should be preserved on the generated UEnum")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum class usage actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		void* StateValuePtr = StateProp->ContainerPtrToValuePtr<void>(Actor);
		const uint64 StateValue = StateProp->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(StateValuePtr);
		ASSERT_THAT(AreEqual(7ULL, StateValue, TEXT("CurrentState should store Armed (7) after enum class function call")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ScopedValue"), 7, TEXT("enum class scoped value should convert to int"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReturnedValue"), 12, TEXT("enum class local value should convert to int"))));
	}

	TEST_METHOD(UEnumReflectionQuery)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_ReflectionQuery"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumReflectionQuery.as"),
			ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EReflectionValueEnum
			{
				None = 0,
				Alpha = 3,
				Beta = 8
			}

			UENUM(BlueprintType)
			enum EReflectionMetaEnum
			{
				NoSelection UMETA(DisplayName="No Selection"),
				AlphaChoice UMETA(DisplayName="Alpha Choice", ToolTip="Alpha tooltip"),
				BetaHidden UMETA(Hidden)
			}

			UCLASS()
			class ACoverageUEnumReflectionQueryActor : AActor
			{
				UPROPERTY()
				EReflectionValueEnum Value = EReflectionValueEnum::Alpha;

				UPROPERTY()
				EReflectionMetaEnum MetaValue = EReflectionMetaEnum::AlphaChoice;
			}
			)AS"),
			TEXT("ACoverageUEnumReflectionQueryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum reflection query actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FEnumProperty* ValueProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ValueProp, TEXT("Value enum property should exist")));
		if (ValueProp == nullptr)
		{
			return;
		}

		UEnum* ValueEnum = ValueProp->GetEnum();
		ASSERT_THAT(IsNotNull(ValueEnum, TEXT("Value should expose generated UEnum")));
		if (ValueEnum == nullptr)
		{
			return;
		}

		const int32 NoneIndex = ValueEnum->GetIndexByNameString(TEXT("None"));
		const int32 AlphaIndex = ValueEnum->GetIndexByNameString(TEXT("Alpha"));
		const int32 BetaIndex = ValueEnum->GetIndexByNameString(TEXT("Beta"));
		ASSERT_THAT(AreEqual(0, NoneIndex, TEXT("None should be queryable by entry name")));
		ASSERT_THAT(AreEqual(1, AlphaIndex, TEXT("Alpha should be queryable by entry name")));
		ASSERT_THAT(AreEqual(2, BetaIndex, TEXT("Beta should be queryable by entry name")));
		if (NoneIndex == INDEX_NONE || AlphaIndex == INDEX_NONE || BetaIndex == INDEX_NONE)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0LL, ValueEnum->GetValueByIndex(NoneIndex), TEXT("None should preserve explicit value 0")));
		ASSERT_THAT(AreEqual(3LL, ValueEnum->GetValueByIndex(AlphaIndex), TEXT("Alpha should preserve explicit value 3")));
		ASSERT_THAT(AreEqual(8LL, ValueEnum->GetValueByIndex(BetaIndex), TEXT("Beta should preserve explicit value 8")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha")), ValueEnum->GetNameStringByValue(3),
			TEXT("Generated UEnum should map explicit value 3 back to Alpha")));
		ASSERT_THAT(AreEqual(8LL, ValueEnum->GetValueByNameString(TEXT("Beta")),
			TEXT("Generated UEnum should map Beta name back to explicit value 8")));

		FEnumProperty* MetaValueProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("MetaValue"));
		ASSERT_THAT(IsNotNull(MetaValueProp, TEXT("MetaValue enum property should exist")));
		if (MetaValueProp == nullptr)
		{
			return;
		}

		UEnum* MetaEnum = MetaValueProp->GetEnum();
		ASSERT_THAT(IsNotNull(MetaEnum, TEXT("MetaValue should expose generated UEnum")));
		if (MetaEnum == nullptr)
		{
			return;
		}

		const int32 AlphaChoiceIndex = MetaEnum->GetIndexByNameString(TEXT("AlphaChoice"));
		const int32 BetaHiddenIndex = MetaEnum->GetIndexByNameString(TEXT("BetaHidden"));
		ASSERT_THAT(AreEqual(1, AlphaChoiceIndex, TEXT("AlphaChoice should be queryable by entry name")));
		ASSERT_THAT(AreEqual(2, BetaHiddenIndex, TEXT("BetaHidden should be queryable by entry name")));
		if (AlphaChoiceIndex == INDEX_NONE || BetaHiddenIndex == INDEX_NONE)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha Choice")), MetaEnum->GetDisplayNameTextByIndex(AlphaChoiceIndex).ToString(),
			TEXT("DisplayName metadata should drive reflected display text")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha tooltip")), MetaEnum->GetMetaData(TEXT("ToolTip"), AlphaChoiceIndex),
			TEXT("ToolTip metadata should remain queryable by enum index")));
		ASSERT_THAT(IsTrue(MetaEnum->HasMetaData(TEXT("Hidden"), BetaHiddenIndex),
			TEXT("Hidden metadata should remain queryable by enum index")));
	}

	// -------------------------------------------------------------------------
	// Enum usage: local variable, UPROPERTY, function parameter, return value.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_Usage"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumUsage.as"),
			ASTEST_AS(R"AS(
			UENUM()
			enum EUsageEnum
			{
				StateIdle,
				StateRunning,
				StatePaused,
				StateStopped
			}

			UCLASS()
			class ACoverageUEnumUsageActor : AActor
			{
				UPROPERTY()
				EUsageEnum CurrentState = EUsageEnum::StateIdle;

				UPROPERTY()
				int FunctionCallCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Local variable
					EUsageEnum LocalState = EUsageEnum::StateRunning;
					check(LocalState == EUsageEnum::StateRunning);

					// Call function with enum parameter
					SetState(EUsageEnum::StatePaused);
					check(CurrentState == EUsageEnum::StatePaused);

					// Get enum from function return
					EUsageEnum ReturnedState = GetNextState(CurrentState);
					check(ReturnedState == EUsageEnum::StateStopped);

					FunctionCallCount = 42;
				}

				void SetState(EUsageEnum NewState)
				{
					CurrentState = NewState;
				}

				EUsageEnum GetNextState(EUsageEnum Current)
				{
					if (Current == EUsageEnum::StateIdle)
						return EUsageEnum::StateRunning;
					else if (Current == EUsageEnum::StateRunning)
						return EUsageEnum::StatePaused;
					else if (Current == EUsageEnum::StatePaused)
						return EUsageEnum::StateStopped;
					else
						return EUsageEnum::StateIdle;
				}
			}
			)AS"),
			TEXT("ACoverageUEnumUsageActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum usage actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum usage actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify the state was changed via function call
		FEnumProperty* StateProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("CurrentState"));
		ASSERT_THAT(IsNotNull(StateProp, TEXT("CurrentState property should exist")));
		if (StateProp == nullptr)
		{
			return;
		}

		uint8* StateValuePtr = StateProp->ContainerPtrToValuePtr<uint8>(Actor);
		int64 StateValue = StateProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(StateValuePtr);
		ASSERT_THAT(AreEqual(2LL, StateValue, TEXT("CurrentState should be StatePaused (2)")));

		// Verify function was executed
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FunctionCallCount"), 42, TEXT("Function call count should be 42"))));
	}

	// -------------------------------------------------------------------------
	// Enum in switch statement.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumSwitch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_Switch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumSwitch.as"),
			ASTEST_AS(R"AS(
			UENUM()
			enum EColorEnum
			{
				Red,
				Green,
				Blue,
				Yellow
			}

			UCLASS()
			class ACoverageUEnumSwitchActor : AActor
			{
				UPROPERTY()
				int RedCount = 0;

				UPROPERTY()
				int GreenCount = 0;

				UPROPERTY()
				int BlueCount = 0;

				UPROPERTY()
				int DefaultCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ProcessColor(EColorEnum::Red);
					ProcessColor(EColorEnum::Green);
					ProcessColor(EColorEnum::Blue);
					ProcessColor(EColorEnum::Blue);
					ProcessColor(EColorEnum::Yellow);
				}

				void ProcessColor(EColorEnum Color)
				{
					switch (Color)
					{
						case EColorEnum::Red:
							RedCount++;
							break;
						case EColorEnum::Green:
							GreenCount++;
							break;
						case EColorEnum::Blue:
							BlueCount++;
							break;
						default:
							DefaultCount++;
							break;
					}
				}
			}
			)AS"),
			TEXT("ACoverageUEnumSwitchActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum switch actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum switch actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RedCount"), 1, TEXT("Red case should execute once"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("GreenCount"), 1, TEXT("Green case should execute once"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BlueCount"), 2, TEXT("Blue case should execute twice"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DefaultCount"), 1, TEXT("Default case should execute once"))));
	}

	// -------------------------------------------------------------------------
	// Enum <-> int conversion.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumConversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_Conversion"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumConversion.as"),
			ASTEST_AS(R"AS(
			UENUM()
			enum EConversionEnum
			{
				ValueZero = 0,
				ValueTen = 10,
				ValueTwenty = 20
			}

			UCLASS()
			class ACoverageUEnumConversionActor : AActor
			{
				UPROPERTY()
				int EnumToIntResult = 0;

				UPROPERTY()
				EConversionEnum IntToEnumResult = EConversionEnum::ValueZero;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Enum to int
					EConversionEnum EnumVal = EConversionEnum::ValueTwenty;
					int IntVal = int(EnumVal);
					check(IntVal == 20);
					EnumToIntResult = IntVal;

					// int to enum
					int SourceInt = 10;
					EConversionEnum ConvertedEnum = EConversionEnum(SourceInt);
					check(ConvertedEnum == EConversionEnum::ValueTen);
					IntToEnumResult = ConvertedEnum;
				}
			}
			)AS"),
			TEXT("ACoverageUEnumConversionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum conversion actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum conversion actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EnumToIntResult"), 20, TEXT("Enum to int should convert to 20"))));

		FEnumProperty* ResultProp = FindFProperty<FEnumProperty>(ScriptClass, TEXT("IntToEnumResult"));
		ASSERT_THAT(IsNotNull(ResultProp, TEXT("IntToEnumResult property should exist")));
		if (ResultProp == nullptr)
		{
			return;
		}

		uint8* ResultValuePtr = ResultProp->ContainerPtrToValuePtr<uint8>(Actor);
		int64 ResultEnumValue = ResultProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ResultValuePtr);
		ASSERT_THAT(AreEqual(10LL, ResultEnumValue, TEXT("Int to enum should convert to ValueTen (10)")));
	}

	// -------------------------------------------------------------------------
	// Enum with bitwise operations (flag-style enum without Bitflags specifier).
	// Note: AngelScript does not support the Bitflags specifier, but enums can
	// still be used for bitwise operations by converting to int.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumBitflags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_Bitflags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumBitflags.as"),
			ASTEST_AS(R"AS(
			UENUM()
			enum EPermissionFlags
			{
				None = 0,
				Read = 1,
				Write = 2,
				Execute = 4,
				Delete = 8
			}

			UCLASS()
			class ACoverageUEnumBitflagsActor : AActor
			{
				UPROPERTY()
				int OrResult = 0;

				UPROPERTY()
				int AndResult = 0;

				UPROPERTY()
				int XorResult = 0;

				UPROPERTY()
				int NotResult = 0;

				UPROPERTY()
				int CompoundOrResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bitwise OR
					int Flags1 = int(EPermissionFlags::Read) | int(EPermissionFlags::Write);
					check(Flags1 == 3);
					OrResult = Flags1;

					// Bitwise AND
					int Flags2 = Flags1 & int(EPermissionFlags::Read);
					check(Flags2 == 1);
					AndResult = Flags2;

					// Bitwise XOR
					int Flags3 = Flags1 ^ int(EPermissionFlags::Write);
					check(Flags3 == 1);
					XorResult = Flags3;

					// Bitwise NOT (inverts all bits, result is negative for small positive values)
					int Flags4 = ~int(EPermissionFlags::Read);
					check(Flags4 == -2);
					NotResult = Flags4;

					// Compound assignment
					int Flags5 = int(EPermissionFlags::Read);
					Flags5 |= int(EPermissionFlags::Execute);
					check(Flags5 == 5);
					CompoundOrResult = Flags5;
				}
			}
			)AS"),
			TEXT("ACoverageUEnumBitflagsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum bitflags actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum bitflags actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OrResult"), 3, TEXT("OR should give Read|Write (3)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AndResult"), 1, TEXT("AND should give Read (1)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("XorResult"), 1, TEXT("XOR should give Read (1)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NotResult"), -2, TEXT("NOT should give ~1 (-2)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CompoundOrResult"), 5, TEXT("Compound OR should give Read|Execute (5)"))));
	}

	// -------------------------------------------------------------------------
	// Enums in containers: TArray<EEnum>, TMap<EEnum, int>.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumInContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUEnum_Containers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUEnumContainers.as"),
			ASTEST_AS(R"AS(
			UENUM()
			enum EContainerEnum
			{
				Item1,
				Item2,
				Item3
			}

			UCLASS()
			class ACoverageUEnumContainersActor : AActor
			{
				UPROPERTY()
				TArray<EContainerEnum> EnumArray;

				UPROPERTY()
				TSet<EContainerEnum> EnumSet;

				UPROPERTY()
				TMap<EContainerEnum, int> EnumToIntMap;

				UPROPERTY()
				TMap<int, EContainerEnum> IntToEnumMap;

				UPROPERTY()
				int ArraySize = 0;

				UPROPERTY()
				int MapSize = 0;

				UPROPERTY()
				int MapLookupResult = 0;

				UPROPERTY()
				bool bSetContainsItem2 = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// TArray<EEnum>
					EnumArray.Add(EContainerEnum::Item1);
					EnumArray.Add(EContainerEnum::Item3);
					EnumArray.Add(EContainerEnum::Item2);
					ArraySize = EnumArray.Num();
					check(ArraySize == 3);
					check(EnumArray[1] == EContainerEnum::Item3);

					// TSet<EEnum>
					EnumSet.Add(EContainerEnum::Item1);
					EnumSet.Add(EContainerEnum::Item2);
					EnumSet.Add(EContainerEnum::Item2);
					check(EnumSet.Num() == 2);
					bSetContainsItem2 = EnumSet.Contains(EContainerEnum::Item2);

					// TMap<EEnum, int>
					EnumToIntMap.Add(EContainerEnum::Item1, 100);
					EnumToIntMap.Add(EContainerEnum::Item2, 200);
					MapSize = EnumToIntMap.Num();
					check(MapSize == 2);
					MapLookupResult = EnumToIntMap[EContainerEnum::Item2];
					check(MapLookupResult == 200);

					// TMap<int, EEnum>
					IntToEnumMap.Add(1, EContainerEnum::Item1);
					IntToEnumMap.Add(2, EContainerEnum::Item2);
					check(IntToEnumMap[2] == EContainerEnum::Item2);
				}
			}
			)AS"),
			TEXT("ACoverageUEnumContainersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum containers actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum containers actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArraySize"), 3, TEXT("Array should have 3 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapSize"), 2, TEXT("Map should have 2 elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapLookupResult"), 200, TEXT("Map lookup should return 200"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetContainsItem2"), true, TEXT("Enum set should contain Item2"))));

		// Verify array property
		FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(ScriptClass, TEXT("EnumArray"));
		ASSERT_THAT(IsNotNull(ArrayProp, TEXT("EnumArray property should exist")));
		if (ArrayProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayProp->Inner->IsA<FEnumProperty>(), TEXT("EnumArray inner should be enum")));

		FSetProperty* SetProp = FindFProperty<FSetProperty>(ScriptClass, TEXT("EnumSet"));
		ASSERT_THAT(IsNotNull(SetProp, TEXT("EnumSet property should exist")));
		if (SetProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetProp->ElementProp->IsA<FEnumProperty>(), TEXT("EnumSet element should be enum")));

		// Verify map property
		FMapProperty* MapProp = FindFProperty<FMapProperty>(ScriptClass, TEXT("EnumToIntMap"));
		ASSERT_THAT(IsNotNull(MapProp, TEXT("EnumToIntMap property should exist")));
		if (MapProp == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapProp->KeyProp->IsA<FEnumProperty>(), TEXT("EnumToIntMap key should be enum")));
		ASSERT_THAT(IsTrue(MapProp->ValueProp->IsA<FIntProperty>(), TEXT("EnumToIntMap value should be int")));
	}

	TEST_METHOD(UEnumInvalidDiagnostics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("MissingOption"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM()
			enum EInvalidAssignmentEnum
			{
				OptionA,
				OptionB
			}

			UCLASS()
			class ACoverageUEnumInvalidDiagnosticsActor : AActor
			{
				UPROPERTY()
				EInvalidAssignmentEnum Value = EInvalidAssignmentEnum::MissingOption;
			}
			)AS");
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUEnum_InvalidDiagnostics"),
			ScriptSource,
			TEXT("Referencing a missing enum entry should remain a compile error"),
			MakeArrayView(ExpectedDiagnostics))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
