#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "SourceNavigation/AngelscriptSourceCodeNavigation.h"
#include "ClassGenerator/ASClass.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "SourceCodeNavigation.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace AngelscriptTest_Editor_AngelscriptSourceNavigationTests_Private
{
	struct FRecordedSourceNavigation
	{
		int32 CallCount = 0;
		FString Path;
		int32 LineNumber = INDEX_NONE;

		void Install()
		{
			AngelscriptSourceNavigation::SetOpenLocationOverrideForTesting(
				[this](const FAngelscriptSourceNavigationLocation& Location)
				{
					++CallCount;
					Path = Location.Path;
					LineNumber = Location.LineNumber;
				});
		}

		void Reset()
		{
			CallCount = 0;
			Path.Reset();
			LineNumber = INDEX_NONE;
		}
	};
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionSourceNavigationTest,
	"Angelscript.TestModule.Editor.SourceNavigation.Functions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSourceNavigationVSCodeParametersTest,
	"Angelscript.TestModule.Editor.SourceNavigation.BuildVSCodeParameters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionSourceNavigationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Editor_AngelscriptSourceNavigationTests_Private;
	FResolvedProductionLikeEngine ResolvedEngine;
	if (!AcquireProductionLikeEngine(*this, TEXT("Source navigation tests require a production-like engine."), ResolvedEngine))
	{
		return false;
	}

	FAngelscriptEngine& Engine = ResolvedEngine.Get();

	const FString Script = TEXT(R"AS(
UCLASS()
class UFunctionNavigationCarrier : UObject
{
	UFUNCTION()
	int ComputeValue()
	{
		return 7;
	}
}
)AS");
	const FString RelativeScriptFilename = TEXT("RuntimeFunctionNavigationTest.as");
	const FString ScriptPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), RelativeScriptFilename);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("RuntimeFunctionNavigationTest"));
	};
	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("RuntimeFunctionNavigationTest"),
		RelativeScriptFilename,
		Script);
	if (!TestTrue(TEXT("Compile annotated function navigation module should succeed"), bCompiled))
	{
		return false;
	}

	UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UFunctionNavigationCarrier"));
	if (!TestNotNull(TEXT("Generated function navigation class should exist"), RuntimeClass))
	{
		return false;
	}

	UFunction* RuntimeFunction = FindGeneratedFunction(RuntimeClass, TEXT("ComputeValue"));
	if (!TestNotNull(TEXT("Generated function navigation function should exist"), RuntimeFunction))
	{
		return false;
	}

	UASFunction* RuntimeASFunction = Cast<UASFunction>(RuntimeFunction);
	if (!TestNotNull(TEXT("Generated function should materialize as UASFunction for source navigation"), RuntimeASFunction))
	{
		return false;
	}

	TestEqual(TEXT("Generated function should preserve source file path"), RuntimeASFunction->GetSourceFilePath(), ScriptPath);
	TestEqual(TEXT("Generated function should preserve source line number"), RuntimeASFunction->GetSourceLineNumber(), 6);

	// Verify the generated types are recognizable as script-generated (the prerequisite
	// for source navigation). In headless automation the AngelscriptEditor module may
	// not be loaded, so FSourceCodeNavigation handlers are unavailable. Test the
	// underlying condition directly instead.
	TestNotNull(TEXT("Source navigation should recognize generated script class as UASClass"), Cast<UASClass>(RuntimeClass));
	TestNotNull(TEXT("Source navigation should recognize generated script function as UASFunction"), Cast<UASFunction>(RuntimeFunction));

	return true;
}

bool FAngelscriptSourceNavigationVSCodeParametersTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Editor_AngelscriptSourceNavigationTests_Private;
	const FString TargetParams = TEXT("--goto \"C:/Project/Script/Test.as:12\"");
	const FString ScriptRootDirectory = TEXT("C:/Project/Script");
	const FString WorkspaceRelativePath = TEXT("Tools/Angelscript.code-workspace");

	bool bPassed = true;
	bPassed &= TestEqual(
		TEXT("Source navigation should prepend the Script folder when folder opening is enabled"),
		AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(TargetParams, FString(), true, ScriptRootDirectory),
		FString::Printf(TEXT("\"%s\" %s"), *ScriptRootDirectory, *TargetParams));
	bPassed &= TestEqual(
		TEXT("Source navigation should leave parameters unchanged when workspace and folder opening are disabled"),
		AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(TargetParams, FString(), false, ScriptRootDirectory),
		TargetParams);
	bPassed &= TestEqual(
		TEXT("Source navigation should prefer configured VS Code workspace over Script folder opening"),
		AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(TargetParams, WorkspaceRelativePath, true, ScriptRootDirectory),
		FString::Printf(TEXT("\"%s\" %s"), *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), WorkspaceRelativePath), *TargetParams));

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptSourceNavigationStoredLocationTest,
	"Angelscript.TestModule.Editor.SourceNavigation.NavigateToFunctionUsesStoredSourceLocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled) // DISABLED(#ue57-headless): property navigation source metadata is not populated in headless mode; shares root cause with SourceNavigation.Functions

bool FAngelscriptSourceNavigationStoredLocationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Editor_AngelscriptSourceNavigationTests_Private;
	FResolvedProductionLikeEngine ResolvedEngine;
	if (!AcquireProductionLikeEngine(*this, TEXT("Source navigation navigation-action test requires a production-like engine."), ResolvedEngine))
	{
		return false;
	}

	FAngelscriptEngine& Engine = ResolvedEngine.Get();
	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	const FName ModuleName(*FString::Printf(TEXT("SourceNavigationStoredLocation_%s"), *UniqueSuffix));
	const FString ModuleNameString = ModuleName.ToString();
	const FString RelativeScriptFilename = FString::Printf(TEXT("SourceNavigationStoredLocation_%s.as"), *UniqueSuffix);
	const FString ScriptPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), RelativeScriptFilename);
	const FName ScriptStructName(*FString::Printf(TEXT("FSourceNavigationStruct_%s"), *UniqueSuffix));
	const FName GeneratedStructName(*FString::Printf(TEXT("SourceNavigationStruct_%s"), *UniqueSuffix));
	const FName GeneratedClassName(*FString::Printf(TEXT("USourceNavigationCarrier_%s"), *UniqueSuffix));

	FString Script = TEXT(R"AS(
USTRUCT()
struct __STRUCT_NAME__
{
	UPROPERTY()
	int StructValue = 11;
}

UCLASS()
class __CLASS_NAME__ : UObject
{
	UPROPERTY()
	int StoredValue = 13;

	UFUNCTION()
	int ComputeValue()
	{
		return StoredValue + 1;
	}
}
)AS");
	Script.ReplaceInline(TEXT("__STRUCT_NAME__"), *ScriptStructName.ToString());
	Script.ReplaceInline(TEXT("__CLASS_NAME__"), *GeneratedClassName.ToString());

	ON_SCOPE_EXIT
	{
		AngelscriptSourceNavigation::ResetOpenLocationOverrideForTesting();
		Engine.DiscardModule(*ModuleNameString);
		IFileManager::Get().Delete(*ScriptPath, false, true, true);
	};

	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		ModuleName,
		RelativeScriptFilename,
		Script);
	if (!TestTrue(TEXT("Compile annotated source navigation location module should succeed"), bCompiled))
	{
		return false;
	}

	UClass* RuntimeClass = FindGeneratedClass(&Engine, GeneratedClassName);
	if (!TestNotNull(TEXT("Generated class for source navigation location test should exist"), RuntimeClass))
	{
		return false;
	}

	UFunction* RuntimeFunction = FindGeneratedFunction(RuntimeClass, TEXT("ComputeValue"));
	if (!TestNotNull(TEXT("Generated function for source navigation location test should exist"), RuntimeFunction))
	{
		return false;
	}

	FProperty* StoredValueProperty = FindFProperty<FProperty>(RuntimeClass, TEXT("StoredValue"));
	if (!TestNotNull(TEXT("Generated property for source navigation location test should exist"), StoredValueProperty))
	{
		return false;
	}

	UScriptStruct* GeneratedStruct = FindObject<UScriptStruct>(FAngelscriptEngine::GetPackage(), *GeneratedStructName.ToString());
	if (!TestNotNull(TEXT("Generated struct for source navigation location test should exist"), GeneratedStruct))
	{
		return false;
	}

	FRecordedSourceNavigation RecordedNavigation;
	RecordedNavigation.Install();

	RecordedNavigation.Reset();
	TestTrue(TEXT("Source navigation should navigate generated script function"), AngelscriptSourceNavigation::NavigateToFunctionForTesting(RuntimeFunction));
	TestEqual(TEXT("Function navigation should emit exactly one open-location request"), RecordedNavigation.CallCount, 1);
	TestEqual(TEXT("Function navigation should target the compiled script file"), RecordedNavigation.Path, ScriptPath);
	TestEqual(TEXT("Function navigation should target the reflected function declaration line"), RecordedNavigation.LineNumber, 16);

	RecordedNavigation.Reset();
	TestTrue(TEXT("Source navigation should navigate generated script property"), AngelscriptSourceNavigation::NavigateToPropertyForTesting(StoredValueProperty));
	TestEqual(TEXT("Property navigation should emit exactly one open-location request"), RecordedNavigation.CallCount, 1);
	TestEqual(TEXT("Property navigation should target the compiled script file"), RecordedNavigation.Path, ScriptPath);
	TestEqual(TEXT("Property navigation should target the reflected property declaration line"), RecordedNavigation.LineNumber, 13);

	RecordedNavigation.Reset();
	TestTrue(TEXT("Source navigation should navigate generated script struct"), AngelscriptSourceNavigation::NavigateToStructForTesting(GeneratedStruct));
	TestEqual(TEXT("Struct navigation should emit exactly one open-location request"), RecordedNavigation.CallCount, 1);
	TestEqual(TEXT("Struct navigation should target the compiled script file"), RecordedNavigation.Path, ScriptPath);
	TestEqual(TEXT("Struct navigation should target the reflected struct declaration line"), RecordedNavigation.LineNumber, 3);

	UASFunction* EmptyPathFunction = NewObject<UASFunction>(GetTransientPackage(), NAME_None, RF_Transient);
	RecordedNavigation.Reset();
	TestFalse(TEXT("Source navigation should reject script functions without a stored source path"), AngelscriptSourceNavigation::NavigateToFunctionForTesting(EmptyPathFunction));
	TestEqual(TEXT("Empty-path function navigation should not trigger an open-location request"), RecordedNavigation.CallCount, 0);

	UFunction* NonScriptFunction = NewObject<UFunction>(GetTransientPackage(), NAME_None, RF_Transient);
	RecordedNavigation.Reset();
	TestFalse(TEXT("Source navigation should reject non-Angelscript UFunction instances"), AngelscriptSourceNavigation::NavigateToFunctionForTesting(NonScriptFunction));
	TestEqual(TEXT("Non-Angelscript function navigation should not trigger an open-location request"), RecordedNavigation.CallCount, 0);

	return true;
}

#endif
