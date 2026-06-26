#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadModuleRecordTests,
	"Angelscript.TestModule.HotReload.ModuleRecordTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FTrackedModuleExpectation
{
	FName ModuleName;
	FString Filename;
	FString EnumName;
	FString DelegateName;
	FName ClassName;
};

inline static const FTrackedModuleExpectation ModuleTrackedTypesA
{
	TEXT("ModuleTrackedTypesA"),
	TEXT("ModuleTrackedTypesA.as"),
	TEXT("ETrackedModuleAState"),
	TEXT("FTrackedModuleASignal"),
	TEXT("UTrackedModuleA"),
};

inline static const FTrackedModuleExpectation ModuleTrackedTypesB
{
	TEXT("ModuleTrackedTypesB"),
	TEXT("ModuleTrackedTypesB.as"),
	TEXT("ETrackedModuleBState"),
	TEXT("FTrackedModuleBSignal"),
	TEXT("UTrackedModuleB"),
};

static FString BuildTrackedTypesScript(const FTrackedModuleExpectation& Expectation)
{
	return FString::Printf(
		TEXT(R"AS(
UENUM(BlueprintType)
enum class %s : uint8
{
Alpha,
Beta = 3
}

delegate void %s(int Value);

UCLASS()
class %s : UObject
{
UPROPERTY()
%s State;

UPROPERTY()
%s Signal;
}
)AS"),
		*Expectation.EnumName,
		*Expectation.DelegateName,
		*Expectation.ClassName.ToString(),
		*Expectation.EnumName,
		*Expectation.DelegateName);
}

static TSharedPtr<FAngelscriptDelegateDesc> FindDelegateDesc(
	const TSharedPtr<FAngelscriptModuleDesc>& ModuleRecord,
	const FString& DelegateName)
{
	if (!ModuleRecord.IsValid())
	{
		return nullptr;
	}

	for (const TSharedRef<FAngelscriptDelegateDesc>& DelegateDesc : ModuleRecord->Delegates)
	{
		if (DelegateDesc->DelegateName == DelegateName)
		{
			return DelegateDesc;
		}
	}

	return nullptr;
}

static bool VerifyTrackedModuleArtifacts(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FTrackedModuleExpectation& Expected,
	const FTrackedModuleExpectation& Unexpected)
{
	bool bPassed = true;

	const TSharedPtr<FAngelscriptModuleDesc> ModuleRecord = Engine.GetModuleByModuleName(Expected.ModuleName.ToString());
	const TSharedPtr<FAngelscriptModuleDesc> UnexpectedRecord = Engine.GetModuleByModuleName(Unexpected.ModuleName.ToString());

	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("%s module record should exist"), *Expected.ModuleName.ToString()),
		ModuleRecord.IsValid());

	if (!ModuleRecord.IsValid())
	{
		return false;
	}

	bPassed &= Test.TestEqual(
		*FString::Printf(TEXT("%s module record should track exactly one class"), *Expected.ModuleName.ToString()),
		ModuleRecord->Classes.Num(),
		1);
	bPassed &= Test.TestEqual(
		*FString::Printf(TEXT("%s module record should track exactly one enum"), *Expected.ModuleName.ToString()),
		ModuleRecord->Enums.Num(),
		1);
	bPassed &= Test.TestEqual(
		*FString::Printf(TEXT("%s module record should track exactly one delegate"), *Expected.ModuleName.ToString()),
		ModuleRecord->Delegates.Num(),
		1);
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("%s module record should expose the generated class"), *Expected.ModuleName.ToString()),
		ModuleRecord->GetClass(Expected.ClassName.ToString()).IsValid());

	const TSharedPtr<FAngelscriptEnumDesc> ModuleEnum = ModuleRecord->GetEnum(Expected.EnumName);
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("%s module record should expose its enum"), *Expected.ModuleName.ToString()),
		ModuleEnum.IsValid());
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("%s module record should not expose %s"), *Expected.ModuleName.ToString(), *Unexpected.EnumName),
		!ModuleRecord->GetEnum(Unexpected.EnumName).IsValid());

	const TSharedPtr<FAngelscriptDelegateDesc> ModuleDelegate = FindDelegateDesc(ModuleRecord, Expected.DelegateName);
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("%s module record should expose its delegate"), *Expected.ModuleName.ToString()),
		ModuleDelegate.IsValid());
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("%s module record should not expose %s"), *Expected.ModuleName.ToString(), *Unexpected.DelegateName),
		!FindDelegateDesc(ModuleRecord, Unexpected.DelegateName).IsValid());

	if (ModuleDelegate.IsValid())
	{
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s delegate name should match"), *Expected.ModuleName.ToString()),
			ModuleDelegate->DelegateName,
			Expected.DelegateName);
		bPassed &= Test.TestNotNull(
			*FString::Printf(TEXT("%s delegate should keep a generated signature function"), *Expected.ModuleName.ToString()),
			ModuleDelegate->Function);
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, Expected.ClassName);
	bPassed &= Test.TestNotNull(
		*FString::Printf(TEXT("%s generated class should remain queryable"), *Expected.ClassName.ToString()),
		GeneratedClass);

	FDelegateProperty* DelegateProperty = GeneratedClass != nullptr
		? FindFProperty<FDelegateProperty>(GeneratedClass, TEXT("Signal"))
		: nullptr;
	bPassed &= Test.TestNotNull(
		*FString::Printf(TEXT("%s generated class should keep the Signal delegate property"), *Expected.ClassName.ToString()),
		DelegateProperty);

	if (ModuleDelegate.IsValid() && DelegateProperty != nullptr)
	{
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s delegate property should target the tracked signature function"), *Expected.ClassName.ToString()),
			DelegateProperty->SignatureFunction == ModuleDelegate->Function);
	}

	TSharedPtr<FAngelscriptModuleDesc> EnumFoundInModule;
	const TSharedPtr<FAngelscriptEnumDesc> EngineEnum = Engine.GetEnum(Expected.EnumName, &EnumFoundInModule);
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("Engine-level enum lookup should find %s"), *Expected.EnumName),
		EngineEnum.IsValid());
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("Engine-level enum lookup should return %s as the owning module"), *Expected.ModuleName.ToString()),
		EnumFoundInModule.IsValid() && EnumFoundInModule->ModuleName == Expected.ModuleName.ToString());
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("Engine-level enum lookup should not attribute %s to %s"), *Expected.EnumName, *Unexpected.ModuleName.ToString()),
		EnumFoundInModule != UnexpectedRecord);

	if (ModuleEnum.IsValid() && EngineEnum.IsValid())
	{
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("Engine-level enum lookup should return the same descriptor tracked by %s"), *Expected.ModuleName.ToString()),
			EngineEnum == ModuleEnum);
	}

	TSharedPtr<FAngelscriptModuleDesc> DelegateFoundInModule;
	const TSharedPtr<FAngelscriptDelegateDesc> EngineDelegate = Engine.GetDelegate(Expected.DelegateName, &DelegateFoundInModule);
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("Engine-level delegate lookup should find %s"), *Expected.DelegateName),
		EngineDelegate.IsValid());
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("Engine-level delegate lookup should return %s as the owning module"), *Expected.ModuleName.ToString()),
		DelegateFoundInModule.IsValid() && DelegateFoundInModule->ModuleName == Expected.ModuleName.ToString());
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("Engine-level delegate lookup should not attribute %s to %s"), *Expected.DelegateName, *Unexpected.ModuleName.ToString()),
		DelegateFoundInModule != UnexpectedRecord);

	if (ModuleDelegate.IsValid() && EngineDelegate.IsValid())
	{
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("Engine-level delegate lookup should return the same descriptor tracked by %s"), *Expected.ModuleName.ToString()),
			EngineDelegate == ModuleDelegate);
	}

	return bPassed;
}

static bool ModuleRecordTracksEnumAndDelegateArtifacts(FAutomationTestBase& Test)
{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	bool bModuleAPassed = false;
	bool bModuleBPassed = false;
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleTrackedTypesA.ModuleName.ToString());
		Engine.DiscardModule(*ModuleTrackedTypesB.ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	if (!Test.TestTrue(
		TEXT("Tracked-types module A compile should succeed"),
		CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleTrackedTypesA.ModuleName,
			ModuleTrackedTypesA.Filename,
			BuildTrackedTypesScript(ModuleTrackedTypesA))))
	{
		return false;
	}

	if (!Test.TestTrue(
		TEXT("Tracked-types module B compile should succeed"),
		CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleTrackedTypesB.ModuleName,
			ModuleTrackedTypesB.Filename,
			BuildTrackedTypesScript(ModuleTrackedTypesB))))
	{
		return false;
	}

	bModuleAPassed = VerifyTrackedModuleArtifacts(Test, Engine, ModuleTrackedTypesA, ModuleTrackedTypesB);
	bModuleBPassed = VerifyTrackedModuleArtifacts(Test, Engine, ModuleTrackedTypesB, ModuleTrackedTypesA);

	}

	return bModuleAPassed && bModuleBPassed;
}

public:
	TEST_METHOD(EnumAndDelegateArtifacts)
	{
		ASSERT_THAT(IsTrue(ModuleRecordTracksEnumAndDelegateArtifacts(*TestRunner)));
	}
};
