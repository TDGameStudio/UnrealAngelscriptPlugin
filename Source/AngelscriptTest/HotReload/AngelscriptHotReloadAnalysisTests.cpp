#include "CQTest.h"
#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptAnalyzeReloadTests,
	"Angelscript.TestModule.HotReload.AnalyzeReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName NoChangeModuleName = FName(TEXT("ReloadNoChangeMod"));
	inline static const FString NoChangeFilename = FString(TEXT("ReloadNoChangeMod.as"));

	inline static const FName PropertyModuleName = FName(TEXT("ReloadPropertyMod"));
	inline static const FString PropertyFilename = FString(TEXT("ReloadPropertyMod.as"));

	inline static const FName SuperModuleName = FName(TEXT("ReloadSuperMod"));
	inline static const FString SuperFilename = FString(TEXT("ReloadSuperMod.as"));

	inline static const FName SoftRequirementModuleName = FName(TEXT("ReloadSoftRequirementMod"));
	inline static const FString SoftRequirementFilename = FString(TEXT("ReloadSoftRequirementMod.as"));

	inline static const FName ClassAddedModuleName = FName(TEXT("ReloadClassAddedMod"));
	inline static const FString ClassAddedFilename = FString(TEXT("ReloadClassAddedMod.as"));

	inline static const FName ClassRemovedModuleName = FName(TEXT("ReloadClassRemovedMod"));
	inline static const FString ClassRemovedFilename = FString(TEXT("ReloadClassRemovedMod.as"));

	inline static const FName FunctionModuleName = FName(TEXT("ReloadFunctionMod"));
	inline static const FString FunctionFilename = FString(TEXT("ReloadFunctionMod.as"));

	inline static const FName EnumValueModuleName = FName(TEXT("ReloadEnumValueMod"));
	inline static const FString EnumValueFilename = FString(TEXT("ReloadEnumValueMod.as"));

	inline static const FName DelegateSignatureModuleName = FName(TEXT("ReloadDelegateSignatureMod"));
	inline static const FString DelegateSignatureFilename = FString(TEXT("ReloadDelegateSignatureMod.as"));
	inline static const FName DelegateSignatureCarrierClassName = FName(TEXT("UReloadDelegateAnalysisCarrier"));
	inline static const FString DelegateSignatureName = FString(TEXT("FReloadAnalysisSignal"));

	struct FReloadDecision
	{
		FAngelscriptClassGenerator::EReloadRequirement Requirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
	};

	static void DiscardModule(FAngelscriptEngine& Engine, const FName ModuleName)
	{
		Engine.DiscardModule(*ModuleName.ToString());
	}

	static bool AnalyzeReloadCase(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& ScriptV1,
		const FString& ScriptV2,
		const TCHAR* Context,
		FReloadDecision& OutDecision)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, ScriptV1),
				*FString::Printf(TEXT("%s should compile before reload analysis"), Context)))
		{
			return false;
		}

		OutDecision = FReloadDecision();
		return LocalAssert.IsTrue(
			AnalyzeReloadFromMemory(
				&Engine,
				ModuleName,
				Filename,
				ScriptV2,
				OutDecision.Requirement,
				OutDecision.bWantsFullReload,
				OutDecision.bNeedsFullReload),
			*FString::Printf(TEXT("%s should analyze the reload edit"), Context));
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

	TEST_METHOD(NoChange)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, NoChangeModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadNoChangeTarget : UObject
			{
				UPROPERTY()
				int Value;

				default Value = 10;

				UFUNCTION()
				int GetValue()
				{
					return Value;
				}
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReloadCase(
			*TestRunner,
			Engine,
			NoChangeModuleName,
			NoChangeFilename,
			ScriptV1,
			ScriptV1,
			TEXT("Unchanged module"),
			Decision)));

		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::SoftReload, Decision.Requirement, TEXT("Unchanged module should remain soft reload")));
		ASSERT_THAT(IsFalse(Decision.bWantsFullReload, TEXT("Unchanged module should not suggest full reload")));
		ASSERT_THAT(IsFalse(Decision.bNeedsFullReload, TEXT("Unchanged module should not require full reload")));
	}

	TEST_METHOD(PropertyCountChange)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, PropertyModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadPropertyTarget : UObject
			{
				UPROPERTY()
				int Value;
			}
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadPropertyTarget : UObject
			{
				UPROPERTY()
				int Value;

				UPROPERTY()
				int ExtraValue;
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReloadCase(
			*TestRunner,
			Engine,
			PropertyModuleName,
			PropertyFilename,
			ScriptV1,
			ScriptV2,
			TEXT("Property count change"),
			Decision)));

		ASSERT_THAT(IsTrue(Decision.bWantsFullReload || Decision.bNeedsFullReload, TEXT("Property count change should request a full reload path")));
		ASSERT_THAT(IsTrue(
			Decision.Requirement == FAngelscriptClassGenerator::FullReloadRequired
				|| Decision.Requirement == FAngelscriptClassGenerator::FullReloadSuggested,
			TEXT("Property count change should not remain soft reload")));
	}

	TEST_METHOD(SuperClassChange)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, SuperModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadSuperTarget : UObject
			{
			}
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadSuperTarget : AActor
			{
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReloadCase(
			*TestRunner,
			Engine,
			SuperModuleName,
			SuperFilename,
			ScriptV1,
			ScriptV2,
			TEXT("Super-class change"),
			Decision)));

		ASSERT_THAT(IsTrue(Decision.bWantsFullReload || Decision.bNeedsFullReload, TEXT("Super-class change should request a full reload path")));
		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::FullReloadRequired, Decision.Requirement, TEXT("Super-class change should require a full reload")));
	}

	TEST_METHOD(SoftReloadRequirement)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, SoftRequirementModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadSoftRequirementTarget : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					return 1;
				}
			}
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadSoftRequirementTarget : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					return 2;
				}
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReloadCase(
			*TestRunner,
			Engine,
			SoftRequirementModuleName,
			SoftRequirementFilename,
			ScriptV1,
			ScriptV2,
			TEXT("Body-only change"),
			Decision)));

		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::SoftReload, Decision.Requirement, TEXT("Body-only change should remain soft reload")));
		ASSERT_THAT(IsFalse(Decision.bWantsFullReload, TEXT("Body-only change should not suggest full reload")));
		ASSERT_THAT(IsFalse(Decision.bNeedsFullReload, TEXT("Body-only change should not require full reload")));
	}

	TEST_METHOD(ClassAdded)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, ClassAddedModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UExistingReloadTarget : UObject
			{
			}
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UExistingReloadTarget : UObject
			{
			}

			UCLASS()
			class UNewReloadTarget : UObject
			{
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReloadCase(
			*TestRunner,
			Engine,
			ClassAddedModuleName,
			ClassAddedFilename,
			ScriptV1,
			ScriptV2,
			TEXT("Class add"),
			Decision)));

		ASSERT_THAT(IsTrue(Decision.bWantsFullReload || Decision.bNeedsFullReload, TEXT("Class add should request a full reload path")));
		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::FullReloadSuggested, Decision.Requirement, TEXT("Class add should suggest a full reload")));
	}

	TEST_METHOD(ClassRemoved)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, ClassRemovedModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadSurvivorTarget : UObject
			{
			}

			UCLASS()
			class UReloadRemovedTarget : UObject
			{
			}
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadSurvivorTarget : UObject
			{
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReloadCase(
			*TestRunner,
			Engine,
			ClassRemovedModuleName,
			ClassRemovedFilename,
			ScriptV1,
			ScriptV2,
			TEXT("Class remove"),
			Decision)));

		ASSERT_THAT(IsTrue(Decision.bWantsFullReload || Decision.bNeedsFullReload, TEXT("Class remove should request a full reload path")));
		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::FullReloadRequired, Decision.Requirement, TEXT("Class remove should require a full reload")));
	}

	TEST_METHOD(FunctionSignatureChanged)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, FunctionModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadFunctionTarget : UObject
			{
				UFUNCTION()
				int ComputeValue()
				{
					return 1;
				}
			}
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UReloadFunctionTarget : UObject
			{
				UFUNCTION()
				float ComputeValue(float Scale)
				{
					return Scale;
				}
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReloadCase(
			*TestRunner,
			Engine,
			FunctionModuleName,
			FunctionFilename,
			ScriptV1,
			ScriptV2,
			TEXT("Function signature change"),
			Decision)));

		ASSERT_THAT(IsTrue(Decision.bWantsFullReload || Decision.bNeedsFullReload, TEXT("Function signature change should request a full reload path")));
		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::FullReloadRequired, Decision.Requirement, TEXT("Function signature change should require a full reload")));
	}

	TEST_METHOD(EnumValueChange)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, EnumValueModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EReloadAnalysisState : uint16
			{
				Alpha = 1,
				Beta = 4
			}

			UCLASS()
			class UReloadEnumValueCarrier : UObject
			{
				UPROPERTY()
				EReloadAnalysisState State;

				default State = EReloadAnalysisState::Alpha;
			}
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EReloadAnalysisState : uint16
			{
				Alpha = 1,
				Beta = 7
			}

			UCLASS()
			class UReloadEnumValueCarrier : UObject
			{
				UPROPERTY()
				EReloadAnalysisState State;

				default State = EReloadAnalysisState::Alpha;
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReloadCase(
			*TestRunner,
			Engine,
			EnumValueModuleName,
			EnumValueFilename,
			ScriptV1,
			ScriptV2,
			TEXT("Enum value-only change"),
			Decision)));

		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::FullReloadSuggested, Decision.Requirement, TEXT("Enum value-only change should suggest a full reload")));
		ASSERT_THAT(IsTrue(Decision.bWantsFullReload, TEXT("Enum value-only change should request a full reload path")));
		ASSERT_THAT(IsFalse(Decision.bNeedsFullReload, TEXT("Enum value-only change should not be marked as full reload required")));
	}

	TEST_METHOD(DelegateSignatureChange)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, DelegateSignatureModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			delegate void FReloadAnalysisSignal(int Value);

			UCLASS()
			class UReloadDelegateAnalysisCarrier : UObject
			{
				UPROPERTY()
				FReloadAnalysisSignal Signal;
			}
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			delegate void FReloadAnalysisSignal(int Value, int Tag);

			UCLASS()
			class UReloadDelegateAnalysisCarrier : UObject
			{
				UPROPERTY()
				FReloadAnalysisSignal Signal;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, DelegateSignatureModuleName, DelegateSignatureFilename, ScriptV1),
			TEXT("Delegate-signature analysis baseline should compile")));

		ASSERT_THAT(IsNotNull(
			FindGeneratedClass(&Engine, DelegateSignatureCarrierClassName),
			TEXT("Delegate-signature analysis baseline should publish the carrier class")));

		ASSERT_THAT(IsTrue(
			Engine.GetDelegate(DelegateSignatureName).IsValid(),
			TEXT("Delegate-signature analysis baseline should publish the delegate metadata")));

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(
			AnalyzeReloadFromMemory(
				&Engine,
				DelegateSignatureModuleName,
				DelegateSignatureFilename,
				ScriptV2,
				Decision.Requirement,
				Decision.bWantsFullReload,
				Decision.bNeedsFullReload),
			TEXT("Reload analysis should succeed for delegate signature change")));

		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::FullReloadRequired, Decision.Requirement, TEXT("Delegate signature change should require a full reload")));
		ASSERT_THAT(IsTrue(Decision.bWantsFullReload, TEXT("Delegate signature change should request a full reload")));
		ASSERT_THAT(IsTrue(Decision.bNeedsFullReload, TEXT("Delegate signature change should be marked as full reload required")));
	}
};
