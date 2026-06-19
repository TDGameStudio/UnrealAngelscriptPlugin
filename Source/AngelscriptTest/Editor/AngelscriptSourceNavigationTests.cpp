#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
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


TEST_CLASS_WITH_FLAGS(
	FAngelscriptFunctionSourceNavigationTest,
	"Angelscript.TestModule.Editor.SourceNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Functions)
	{
		using namespace AngelscriptTest_Editor_AngelscriptSourceNavigationTests_Private;
		FResolvedProductionLikeEngine ResolvedEngine;
		ASSERT_THAT(IsTrue(AcquireProductionLikeEngine(*TestRunner, TEXT("Source navigation tests require a production-like engine."), ResolvedEngine)));

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
		ASSERT_THAT(IsTrue(bCompiled));

		UClass* RuntimeClass = FindGeneratedClass(&Engine, TEXT("UFunctionNavigationCarrier"));
		ASSERT_THAT(IsNotNull(RuntimeClass));

		UFunction* RuntimeFunction = FindGeneratedFunction(RuntimeClass, TEXT("ComputeValue"));
		ASSERT_THAT(IsNotNull(RuntimeFunction));

		UASFunction* RuntimeASFunction = Cast<UASFunction>(RuntimeFunction);
		ASSERT_THAT(IsNotNull(RuntimeASFunction));

		TestRunner->TestEqual(TEXT("Generated function should preserve source file path"), RuntimeASFunction->GetSourceFilePath(), ScriptPath);
		TestRunner->TestEqual(TEXT("Generated function should preserve source line number"), RuntimeASFunction->GetSourceLineNumber(), 6);

		// Verify the generated types are recognizable as script-generated (the prerequisite
		// for source navigation). In headless automation the AngelscriptEditor module may
		// not be loaded, so FSourceCodeNavigation handlers are unavailable. Test the
		// underlying condition directly instead.
		TestRunner->TestNotNull(TEXT("Source navigation should recognize generated script class as UASClass"), Cast<UASClass>(RuntimeClass));
		TestRunner->TestNotNull(TEXT("Source navigation should recognize generated script function as UASFunction"), Cast<UASFunction>(RuntimeFunction));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptSourceNavigationVSCodeParametersTest,
	"Angelscript.TestModule.Editor.SourceNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BuildVSCodeParameters)
	{
		const FString TargetParams = TEXT("--goto \"C:/Project/Script/Test.as:12\"");
		const FString ScriptRootDirectory = TEXT("C:/Project/Script");
		const FString WorkspaceRelativePath = TEXT("Tools/Angelscript.code-workspace");

		ASSERT_THAT(AreEqual(
			FString::Printf(TEXT("\"%s\" %s"), *ScriptRootDirectory, *TargetParams),
			AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(TargetParams, FString(), true, ScriptRootDirectory)));
		ASSERT_THAT(AreEqual(
			TargetParams,
			AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(TargetParams, FString(), false, ScriptRootDirectory)));
		ASSERT_THAT(AreEqual(
			FString::Printf(TEXT("\"%s\" %s"), *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), WorkspaceRelativePath), *TargetParams),
			AngelscriptSourceNavigation::BuildVSCodeOpenParametersForTesting(TargetParams, WorkspaceRelativePath, true, ScriptRootDirectory)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptSourceNavigationStoredLocationTest,
	"Angelscript.TestModule.Editor.SourceNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled) // DISABLED(#ue57-headless): property navigation source metadata is not populated in headless mode; shares root cause with SourceNavigation.Functions
{
	TEST_METHOD(NavigateToFunctionUsesStoredSourceLocation)
	{
		using namespace AngelscriptTest_Editor_AngelscriptSourceNavigationTests_Private;
		FResolvedProductionLikeEngine ResolvedEngine;
		ASSERT_THAT(IsTrue(AcquireProductionLikeEngine(*TestRunner, TEXT("Source navigation navigation-action test requires a production-like engine."), ResolvedEngine)));

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
		ASSERT_THAT(IsTrue(bCompiled));

		UClass* RuntimeClass = FindGeneratedClass(&Engine, GeneratedClassName);
		ASSERT_THAT(IsNotNull(RuntimeClass));

		UFunction* RuntimeFunction = FindGeneratedFunction(RuntimeClass, TEXT("ComputeValue"));
		ASSERT_THAT(IsNotNull(RuntimeFunction));

		FProperty* StoredValueProperty = FindFProperty<FProperty>(RuntimeClass, TEXT("StoredValue"));
		ASSERT_THAT(IsNotNull(StoredValueProperty));

		UScriptStruct* GeneratedStruct = FindObject<UScriptStruct>(FAngelscriptEngine::GetPackage(), *GeneratedStructName.ToString());
		ASSERT_THAT(IsNotNull(GeneratedStruct));

		FRecordedSourceNavigation RecordedNavigation;
		RecordedNavigation.Install();

		RecordedNavigation.Reset();
		TestRunner->TestTrue(TEXT("Source navigation should navigate generated script function"), AngelscriptSourceNavigation::NavigateToFunctionForTesting(RuntimeFunction));
		TestRunner->TestEqual(TEXT("Function navigation should emit exactly one open-location request"), RecordedNavigation.CallCount, 1);
		TestRunner->TestEqual(TEXT("Function navigation should target the compiled script file"), RecordedNavigation.Path, ScriptPath);
		TestRunner->TestEqual(TEXT("Function navigation should target the reflected function declaration line"), RecordedNavigation.LineNumber, 16);

		RecordedNavigation.Reset();
		TestRunner->TestTrue(TEXT("Source navigation should navigate generated script property"), AngelscriptSourceNavigation::NavigateToPropertyForTesting(StoredValueProperty));
		TestRunner->TestEqual(TEXT("Property navigation should emit exactly one open-location request"), RecordedNavigation.CallCount, 1);
		TestRunner->TestEqual(TEXT("Property navigation should target the compiled script file"), RecordedNavigation.Path, ScriptPath);
		TestRunner->TestEqual(TEXT("Property navigation should target the reflected property declaration line"), RecordedNavigation.LineNumber, 13);

		RecordedNavigation.Reset();
		TestRunner->TestTrue(TEXT("Source navigation should navigate generated script struct"), AngelscriptSourceNavigation::NavigateToStructForTesting(GeneratedStruct));
		TestRunner->TestEqual(TEXT("Struct navigation should emit exactly one open-location request"), RecordedNavigation.CallCount, 1);
		TestRunner->TestEqual(TEXT("Struct navigation should target the compiled script file"), RecordedNavigation.Path, ScriptPath);
		TestRunner->TestEqual(TEXT("Struct navigation should target the reflected struct declaration line"), RecordedNavigation.LineNumber, 3);

		UASFunction* EmptyPathFunction = NewObject<UASFunction>(GetTransientPackage(), NAME_None, RF_Transient);
		RecordedNavigation.Reset();
		TestRunner->TestFalse(TEXT("Source navigation should reject script functions without a stored source path"), AngelscriptSourceNavigation::NavigateToFunctionForTesting(EmptyPathFunction));
		TestRunner->TestEqual(TEXT("Empty-path function navigation should not trigger an open-location request"), RecordedNavigation.CallCount, 0);

		UFunction* NonScriptFunction = NewObject<UFunction>(GetTransientPackage(), NAME_None, RF_Transient);
		RecordedNavigation.Reset();
		TestRunner->TestFalse(TEXT("Source navigation should reject non-Angelscript UFunction instances"), AngelscriptSourceNavigation::NavigateToFunctionForTesting(NonScriptFunction));
		TestRunner->TestEqual(TEXT("Non-Angelscript function navigation should not trigger an open-location request"), RecordedNavigation.CallCount, 0);
	}
};

#endif
