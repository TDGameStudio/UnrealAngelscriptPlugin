#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptHotReloadDecisionMatrixTest
{
	struct FExpectedReloadDecision
	{
		FAngelscriptClassGenerator::EReloadRequirement Requirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
	};

	static bool AnalyzeDecisionCase(
		FAutomationTestBase& Test,
		const FName ModuleName,
		const FString& Filename,
		const FString& ScriptV1,
		const FString& ScriptV2,
		const FExpectedReloadDecision& Expected)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		if (!Test.TestTrue(
				*FString::Printf(TEXT("%s should compile before reload analysis"), *ModuleName.ToString()),
				CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, ScriptV1)))
		{
			return false;
		}

		FAngelscriptClassGenerator::EReloadRequirement ActualRequirement = FAngelscriptClassGenerator::Error;
		bool bActualWantsFullReload = false;
		bool bActualNeedsFullReload = false;
		if (!Test.TestTrue(
				*FString::Printf(TEXT("%s should analyze the reload edit"), *ModuleName.ToString()),
				AnalyzeReloadFromMemory(
					&Engine,
					ModuleName,
					Filename,
					ScriptV2,
					ActualRequirement,
					bActualWantsFullReload,
					bActualNeedsFullReload)))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should report the expected reload requirement"), *ModuleName.ToString()),
			ActualRequirement,
			Expected.Requirement);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should report whether full reload is wanted"), *ModuleName.ToString()),
			bActualWantsFullReload,
			Expected.bWantsFullReload);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should report whether full reload is required"), *ModuleName.ToString()),
			bActualNeedsFullReload,
			Expected.bNeedsFullReload);

		return bPassed;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadDecisionMatrixTests,
	"Angelscript.TestModule.HotReload.DecisionMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionDefaultArgumentChangeSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionDefaultTarget : UObject
{
	UFUNCTION()
	int SumWithDefault(int Value = 1)
	{
		return Value;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionDefaultTarget : UObject
{
	UFUNCTION()
	int SumWithDefault(int Value = 2)
	{
		return Value;
	}
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionFunctionDefault"),
			TEXT("HotReloadDecisionFunctionDefault.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}

	TEST_METHOD(FunctionMetadataChangeSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionMetadataTarget : UObject
{
	UFUNCTION(meta=(DisplayName="Alpha"))
	int GetValue()
	{
		return 1;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionMetadataTarget : UObject
{
	UFUNCTION(meta=(DisplayName="Beta"))
	int GetValue()
	{
		return 1;
	}
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionFunctionMetadata"),
			TEXT("HotReloadDecisionFunctionMetadata.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}

	TEST_METHOD(FunctionBlueprintSpecifierChangeRequiresFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionSpecifierTarget : UObject
{
	UFUNCTION(BlueprintCallable)
	int GetValue()
	{
		return 1;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionSpecifierTarget : UObject
{
	UFUNCTION(BlueprintPure)
	int GetValue() const
	{
		return 1;
	}
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionFunctionSpecifier"),
			TEXT("HotReloadDecisionFunctionSpecifier.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadRequired, true, true })));
	}

	TEST_METHOD(FunctionAddedSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionAddedTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionAddedTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}

	UFUNCTION()
	int GetExtraValue()
	{
		return 2;
	}
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionFunctionAdded"),
			TEXT("HotReloadDecisionFunctionAdded.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}

	TEST_METHOD(FunctionRemovedRequiresFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionRemovedTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}

	UFUNCTION()
	int GetRemovedValue()
	{
		return 2;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionRemovedTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionFunctionRemoved"),
			TEXT("HotReloadDecisionFunctionRemoved.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadRequired, true, true })));
	}

	TEST_METHOD(FunctionArgumentNameChangeSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionArgumentNameTarget : UObject
{
	UFUNCTION()
	int Echo(int FirstValue)
	{
		return FirstValue;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionFunctionArgumentNameTarget : UObject
{
	UFUNCTION()
	int Echo(int SecondValue)
	{
		return SecondValue;
	}
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionFunctionArgumentName"),
			TEXT("HotReloadDecisionFunctionArgumentName.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}

	TEST_METHOD(BlueprintEventAddedRequiresFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionBlueprintEventAddedTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionBlueprintEventAddedTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}

	UFUNCTION(BlueprintEvent)
	int GetExtraValue()
	{
		return 2;
	}
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionBlueprintEventAdded"),
			TEXT("HotReloadDecisionBlueprintEventAdded.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadRequired, true, true })));
	}

	TEST_METHOD(DelegateAddedSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionDelegateAddedTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
delegate void FHotReloadDecisionAddedSignal(int Value);

UCLASS()
class UHotReloadDecisionDelegateAddedTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionDelegateAdded"),
			TEXT("HotReloadDecisionDelegateAdded.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}

	TEST_METHOD(DelegateKindChangeRequiresFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
delegate void FHotReloadDecisionDelegateKindSignal(int Value);

UCLASS()
class UHotReloadDecisionDelegateKindTarget : UObject
{
	UPROPERTY()
	FHotReloadDecisionDelegateKindSignal Signal;
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
event void FHotReloadDecisionDelegateKindSignal(int Value);

UCLASS()
class UHotReloadDecisionDelegateKindTarget : UObject
{
	UPROPERTY()
	FHotReloadDecisionDelegateKindSignal Signal;
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionDelegateKind"),
			TEXT("HotReloadDecisionDelegateKind.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadRequired, true, true })));
	}

	TEST_METHOD(DefaultStatementChangeSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionDefaultStatementTarget : UObject
{
	UPROPERTY()
	int Value;

	default Value = 1;
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionDefaultStatementTarget : UObject
{
	UPROPERTY()
	int Value;

	default Value = 2;
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionDefaultStatement"),
			TEXT("HotReloadDecisionDefaultStatement.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}

	TEST_METHOD(EnumMetadataChangeSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UENUM(BlueprintType)
enum class EHotReloadDecisionEnumMetadataState : uint8
{
	Alpha UMETA(DisplayName="Alpha"),
	Beta
}

UCLASS()
class UHotReloadDecisionEnumMetadataTarget : UObject
{
	UPROPERTY()
	EHotReloadDecisionEnumMetadataState State;

	default State = EHotReloadDecisionEnumMetadataState::Alpha;
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UENUM(BlueprintType)
enum class EHotReloadDecisionEnumMetadataState : uint8
{
	Alpha UMETA(DisplayName="Alpha Reloaded"),
	Beta
}

UCLASS()
class UHotReloadDecisionEnumMetadataTarget : UObject
{
	UPROPERTY()
	EHotReloadDecisionEnumMetadataState State;

	default State = EHotReloadDecisionEnumMetadataState::Alpha;
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionEnumMetadata"),
			TEXT("HotReloadDecisionEnumMetadata.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}

	TEST_METHOD(ClassMetadataChangeSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS(meta=(DisplayName="Alpha"))
class UHotReloadDecisionClassMetadataTarget : UObject
{
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS(meta=(DisplayName="Beta"))
class UHotReloadDecisionClassMetadataTarget : UObject
{
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionClassMetadata"),
			TEXT("HotReloadDecisionClassMetadata.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}

	TEST_METHOD(ClassFlagChangeSuggestsFullReload)
	{
		using namespace AngelscriptHotReloadDecisionMatrixTest;

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UHotReloadDecisionClassFlagTarget : UObject
{
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS(Abstract)
class UHotReloadDecisionClassFlagTarget : UObject
{
}
)AS");

		ASSERT_THAT(IsTrue(AnalyzeDecisionCase(
			*TestRunner,
			TEXT("HotReloadDecisionClassFlag"),
			TEXT("HotReloadDecisionClassFlag.as"),
			ScriptV1,
			ScriptV2,
			{ FAngelscriptClassGenerator::FullReloadSuggested, true, false })));
	}
};

#endif
