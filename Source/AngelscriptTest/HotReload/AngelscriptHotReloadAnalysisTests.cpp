#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadNoChangeTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.NoChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadPropertyCountChangeTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.PropertyCountChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadSuperClassChangeTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.SuperClassChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadSoftReloadRequirementTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.SoftReloadRequirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadClassAddedTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.ClassAdded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadClassRemovedTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.ClassRemoved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadFunctionSignatureChangedTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.FunctionSignatureChanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadEnumValueChangeTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.EnumValueChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAnalyzeReloadDelegateSignatureChangeTest,
	"Angelscript.TestModule.HotReload.AnalyzeReload.DelegateSignatureChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptAnalyzeReloadNoChangeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
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

	if (!TestTrue(TEXT("Initial module compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ReloadNoChangeMod"), TEXT("ReloadNoChangeMod.as"), ScriptV1)))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = true;
	bool bNeedsFullReload = true;
	const bool bAnalyzed = AnalyzeReloadFromMemory(&Engine, TEXT("ReloadNoChangeMod"), TEXT("ReloadNoChangeMod.as"), ScriptV1, ReloadRequirement, bWantsFullReload, bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for unchanged module"), bAnalyzed))
	{
		return false;
	}

	TestEqual(TEXT("Unchanged module should remain soft reload"), ReloadRequirement, FAngelscriptClassGenerator::SoftReload);
	TestFalse(TEXT("Unchanged module should not suggest full reload"), bWantsFullReload);
	TestFalse(TEXT("Unchanged module should not require full reload"), bNeedsFullReload);
	}

	return true;
}

bool FAngelscriptAnalyzeReloadPropertyCountChangeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UReloadPropertyTarget : UObject
{
	UPROPERTY()
	int Value;
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UReloadPropertyTarget : UObject
{
	UPROPERTY()
	int Value;

	UPROPERTY()
	int ExtraValue;
}
)AS");

	if (!TestTrue(TEXT("Initial property-count module compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ReloadPropertyMod"), TEXT("ReloadPropertyMod.as"), ScriptV1)))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bAnalyzed = AnalyzeReloadFromMemory(&Engine, TEXT("ReloadPropertyMod"), TEXT("ReloadPropertyMod.as"), ScriptV2, ReloadRequirement, bWantsFullReload, bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for property count change"), bAnalyzed))
	{
		return false;
	}

	TestTrue(TEXT("Property count change should request a full reload path"), bWantsFullReload || bNeedsFullReload);
	TestTrue(TEXT("Property count change should not remain soft reload"), ReloadRequirement == FAngelscriptClassGenerator::FullReloadRequired || ReloadRequirement == FAngelscriptClassGenerator::FullReloadSuggested);
	}

	return true;
}

bool FAngelscriptAnalyzeReloadSuperClassChangeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UReloadSuperTarget : UObject
{
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UReloadSuperTarget : AActor
{
}
)AS");

	if (!TestTrue(TEXT("Initial super-class module compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ReloadSuperMod"), TEXT("ReloadSuperMod.as"), ScriptV1)))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bAnalyzed = AnalyzeReloadFromMemory(&Engine, TEXT("ReloadSuperMod"), TEXT("ReloadSuperMod.as"), ScriptV2, ReloadRequirement, bWantsFullReload, bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for super-class change"), bAnalyzed))
	{
		return false;
	}

	TestTrue(TEXT("Super-class change should request a full reload path"), bWantsFullReload || bNeedsFullReload);
	TestEqual(TEXT("Super-class change should require a full reload"), ReloadRequirement, FAngelscriptClassGenerator::FullReloadRequired);
	}

	return true;
}

bool FAngelscriptAnalyzeReloadSoftReloadRequirementTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
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
	const FString ScriptV2 = TEXT(R"AS(
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

	if (!TestTrue(TEXT("Initial soft-requirement module compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ReloadSoftRequirementMod"), TEXT("ReloadSoftRequirementMod.as"), ScriptV1)))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = true;
	bool bNeedsFullReload = true;
	const bool bAnalyzed = AnalyzeReloadFromMemory(&Engine, TEXT("ReloadSoftRequirementMod"), TEXT("ReloadSoftRequirementMod.as"), ScriptV2, ReloadRequirement, bWantsFullReload, bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for body-only change"), bAnalyzed))
	{
		return false;
	}

	TestEqual(TEXT("Body-only change should remain soft reload"), ReloadRequirement, FAngelscriptClassGenerator::SoftReload);
	TestFalse(TEXT("Body-only change should not suggest full reload"), bWantsFullReload);
	TestFalse(TEXT("Body-only change should not require full reload"), bNeedsFullReload);
	}

	return true;
}

bool FAngelscriptAnalyzeReloadClassAddedTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UExistingReloadTarget : UObject
{
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UExistingReloadTarget : UObject
{
}

UCLASS()
class UNewReloadTarget : UObject
{
}
)AS");

	if (!TestTrue(TEXT("Initial class-added baseline compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ReloadClassAddedMod"), TEXT("ReloadClassAddedMod.as"), ScriptV1)))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bAnalyzed = AnalyzeReloadFromMemory(&Engine, TEXT("ReloadClassAddedMod"), TEXT("ReloadClassAddedMod.as"), ScriptV2, ReloadRequirement, bWantsFullReload, bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for class add"), bAnalyzed))
	{
		return false;
	}

	TestTrue(TEXT("Class add should request a full reload path"), bWantsFullReload || bNeedsFullReload);
	TestEqual(TEXT("Class add should suggest a full reload"), ReloadRequirement, FAngelscriptClassGenerator::FullReloadSuggested);
	}

	return true;
}

bool FAngelscriptAnalyzeReloadClassRemovedTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UReloadSurvivorTarget : UObject
{
}

UCLASS()
class UReloadRemovedTarget : UObject
{
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UReloadSurvivorTarget : UObject
{
}
)AS");

	if (!TestTrue(TEXT("Initial class-removed baseline compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ReloadClassRemovedMod"), TEXT("ReloadClassRemovedMod.as"), ScriptV1)))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bAnalyzed = AnalyzeReloadFromMemory(&Engine, TEXT("ReloadClassRemovedMod"), TEXT("ReloadClassRemovedMod.as"), ScriptV2, ReloadRequirement, bWantsFullReload, bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for class remove"), bAnalyzed))
	{
		return false;
	}

	TestTrue(TEXT("Class remove should request a full reload path"), bWantsFullReload || bNeedsFullReload);
	TestEqual(TEXT("Class remove should require a full reload"), ReloadRequirement, FAngelscriptClassGenerator::FullReloadRequired);
	}

	return true;
}

bool FAngelscriptAnalyzeReloadFunctionSignatureChangedTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
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
	const FString ScriptV2 = TEXT(R"AS(
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

	if (!TestTrue(TEXT("Initial function-signature baseline compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ReloadFunctionMod"), TEXT("ReloadFunctionMod.as"), ScriptV1)))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bAnalyzed = AnalyzeReloadFromMemory(&Engine, TEXT("ReloadFunctionMod"), TEXT("ReloadFunctionMod.as"), ScriptV2, ReloadRequirement, bWantsFullReload, bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for function signature change"), bAnalyzed))
	{
		return false;
	}

	TestTrue(TEXT("Function signature change should request a full reload path"), bWantsFullReload || bNeedsFullReload);
	TestEqual(TEXT("Function signature change should require a full reload"), ReloadRequirement, FAngelscriptClassGenerator::FullReloadRequired);
	}

	return true;
}

bool FAngelscriptAnalyzeReloadEnumValueChangeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
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
	const FString ScriptV2 = TEXT(R"AS(
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

	if (!TestTrue(TEXT("Initial enum-value baseline compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("ReloadEnumValueMod"), TEXT("ReloadEnumValueMod.as"), ScriptV1)))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bAnalyzed = AnalyzeReloadFromMemory(&Engine, TEXT("ReloadEnumValueMod"), TEXT("ReloadEnumValueMod.as"), ScriptV2, ReloadRequirement, bWantsFullReload, bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for enum value-only change"), bAnalyzed))
	{
		return false;
	}

	TestEqual(TEXT("Enum value-only change should suggest a full reload"), ReloadRequirement, FAngelscriptClassGenerator::FullReloadSuggested);
	TestTrue(TEXT("Enum value-only change should request a full reload path"), bWantsFullReload);
	TestFalse(TEXT("Enum value-only change should not be marked as full reload required"), bNeedsFullReload);
	}

	return true;
}

bool FAngelscriptAnalyzeReloadDelegateSignatureChangeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("ReloadDelegateSignatureMod"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
	};

	const FString ScriptV1 = TEXT(R"AS(
delegate void FReloadAnalysisSignal(int Value);

UCLASS()
class UReloadDelegateAnalysisCarrier : UObject
{
	UPROPERTY()
	FReloadAnalysisSignal Signal;
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
delegate void FReloadAnalysisSignal(int Value, int Tag);

UCLASS()
class UReloadDelegateAnalysisCarrier : UObject
{
	UPROPERTY()
	FReloadAnalysisSignal Signal;
}
)AS");

	if (!TestTrue(
			TEXT("Initial delegate-signature analysis baseline compile should succeed"),
			CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("ReloadDelegateSignatureMod.as"), ScriptV1)))
	{
		return false;
	}

	if (!TestNotNull(
			TEXT("Delegate-signature analysis baseline should publish the carrier class"),
			FindGeneratedClass(&Engine, TEXT("UReloadDelegateAnalysisCarrier"))))
	{
		return false;
	}

	if (!TestTrue(
			TEXT("Delegate-signature analysis baseline should publish the delegate metadata"),
			Engine.GetDelegate(TEXT("FReloadAnalysisSignal")).IsValid()))
	{
		return false;
	}

	FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
	bool bWantsFullReload = false;
	bool bNeedsFullReload = false;
	const bool bAnalyzed = AnalyzeReloadFromMemory(
		&Engine,
		ModuleName,
		TEXT("ReloadDelegateSignatureMod.as"),
		ScriptV2,
		ReloadRequirement,
		bWantsFullReload,
		bNeedsFullReload);

	if (!TestTrue(TEXT("Reload analysis should succeed for delegate signature change"), bAnalyzed))
	{
		return false;
	}

	TestEqual(
		TEXT("Delegate signature change should require a full reload"),
		ReloadRequirement,
		FAngelscriptClassGenerator::FullReloadRequired);
	TestTrue(
		TEXT("Delegate signature change should request a full reload"),
		bWantsFullReload);
	TestTrue(
		TEXT("Delegate signature change should be marked as full reload required"),
		bNeedsFullReload);
	}

	return true;
}

#endif
