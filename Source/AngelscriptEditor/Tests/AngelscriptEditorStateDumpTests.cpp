#include "CQTest.h"
#include "HotReload/ClassReloadHelper.h"
#include "Dump/AngelscriptStateDump.h"
#include "EditorMenuExtensions/ScriptEditorMenuExtension.h"
#include "Tests/AngelscriptEditorMenuExtensionsTestTypes.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor::Private
{
	void RegisterStateDumpExtension(FDelegateHandle& OutHandle);
	void UnregisterStateDumpExtension(FDelegateHandle& InOutHandle);
}

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorStateDumpTests_Private
{
	FString MakeUniqueStateDumpPath(const FString& Prefix)
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("StateDump"),
			FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	bool EnsureDirectoryExists(FAutomationTestBase& Test, const FString& Directory)
	{
		if (IFileManager::Get().DirectoryExists(*Directory))
		{
			return true;
		}

		const bool bCreated = IFileManager::Get().MakeDirectory(*Directory, true);
		Test.TestTrue(*FString::Printf(TEXT("StateDump test should create directory '%s'"), *Directory), bCreated);
		return bCreated;
	}

	bool LoadCsvLines(FAutomationTestBase& Test, const FString& Filename, TArray<FString>& OutLines)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *Filename))
		{
			Test.AddError(FString::Printf(TEXT("Failed to load CSV '%s'"), *Filename));
			return false;
		}

		Contents.ParseIntoArrayLines(OutLines, true);
		return true;
	}

	FString BuildReloadClassRow(UClass* OldClass, UClass* NewClass)
	{
		return FString::Printf(TEXT("ReloadClass,%s,%s"), *GetNameSafe(OldClass), *GetNameSafe(NewClass));
	}

	FString BuildMenuSnapshotRow(const FName ExtensionPoint, const EScriptEditorMenuExtensionLocation Location, const FName SectionName)
	{
		const UEnum* LocationEnum = StaticEnum<EScriptEditorMenuExtensionLocation>();
		const FString LocationString = LocationEnum != nullptr
			? LocationEnum->GetNameStringByValue(static_cast<int64>(Location))
			: FString();
		return FString::Printf(TEXT("%s,%s,%s"), *ExtensionPoint.ToString(), *LocationString, *SectionName.ToString());
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunRegistersExtensionAndWritesExpectedCsvFiles(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorStateDumpTests_Private;
	FClassReloadHelper::FReloadState SavedReloadState = FClassReloadHelper::ReloadState();
	FAngelscriptStateDump::FDumpExtensionsDelegate SavedDumpExtensions;
	Swap(SavedDumpExtensions, FAngelscriptStateDump::OnDumpExtensions);

	FDelegateHandle StateDumpHandle;
	const FString FirstOutputDir = MakeUniqueStateDumpPath(TEXT("EditorStateDumpFirst"));
	const FString SecondOutputDir = MakeUniqueStateDumpPath(TEXT("EditorStateDumpSecond"));

	const FGuid UniqueGuid = FGuid::NewGuid();
	const FName UniqueExtensionPoint(*FString::Printf(TEXT("Automation.StateDump.%s"), *UniqueGuid.ToString(EGuidFormats::Digits)));

	UAngelscriptActorMenuExtensionTestShim* SnapshotExtension = NewObject<UAngelscriptActorMenuExtensionTestShim>(GetTransientPackage());
	if (!TestNotNull(TEXT("Editor.StateDump test should create a menu extension shim"), SnapshotExtension))
	{
		Swap(SavedDumpExtensions, FAngelscriptStateDump::OnDumpExtensions);
		return false;
	}

	SnapshotExtension->ExtensionMenu = EScriptEditorMenuExtensionLocation::LevelViewport_ContextMenu;
	SnapshotExtension->ExtensionPoint = UniqueExtensionPoint;
	SnapshotExtension->AddToRoot();

	ON_SCOPE_EXIT
	{
		AngelscriptEditor::Private::UnregisterStateDumpExtension(StateDumpHandle);
		FAngelscriptEditorMenuExtensionTestAccess::UnregisterExtensions();
		FAngelscriptEditorMenuExtensionTestAccess::RegisterExtensions();
		SnapshotExtension->RemoveFromRoot();
		FClassReloadHelper::ReloadState() = SavedReloadState;
		Swap(SavedDumpExtensions, FAngelscriptStateDump::OnDumpExtensions);
		IFileManager::Get().DeleteDirectory(*FirstOutputDir, false, true);
		IFileManager::Get().DeleteDirectory(*SecondOutputDir, false, true);
	};

	FClassReloadHelper::ReloadState() = FClassReloadHelper::FReloadState();
	FClassReloadHelper::ReloadState().ReloadClasses.Add(AActor::StaticClass(), APawn::StaticClass());

	FAngelscriptEditorMenuExtensionTestAccess::RegisterExtension(SnapshotExtension);

	AngelscriptEditor::Private::RegisterStateDumpExtension(StateDumpHandle);
	const FDelegateHandle FirstRegisterHandle = StateDumpHandle;
	if (!TestTrue(TEXT("Editor.StateDump test should receive a valid delegate handle after registration"), StateDumpHandle.IsValid()))
	{
		return false;
	}

	AngelscriptEditor::Private::RegisterStateDumpExtension(StateDumpHandle);
	if (!TestTrue(TEXT("Editor.StateDump test should keep registration idempotent when called with the same handle"), StateDumpHandle == FirstRegisterHandle))
	{
		return false;
	}

	if (!EnsureDirectoryExists(Test, FirstOutputDir))
	{
		return false;
	}

	FAngelscriptStateDump::OnDumpExtensions.Broadcast(FirstOutputDir);

	const FString ReloadStateCsvPath = FPaths::Combine(FirstOutputDir, TEXT("EditorReloadState.csv"));
	const FString MenuExtensionsCsvPath = FPaths::Combine(FirstOutputDir, TEXT("EditorMenuExtensions.csv"));
	if (!TestTrue(TEXT("Editor.StateDump test should write EditorReloadState.csv after registration"), IFileManager::Get().FileExists(*ReloadStateCsvPath))
		|| !TestTrue(TEXT("Editor.StateDump test should write EditorMenuExtensions.csv after registration"), IFileManager::Get().FileExists(*MenuExtensionsCsvPath)))
	{
		return false;
	}

	TArray<FString> ReloadStateLines;
	TArray<FString> MenuExtensionLines;
	if (!LoadCsvLines(Test, ReloadStateCsvPath, ReloadStateLines)
		|| !LoadCsvLines(Test, MenuExtensionsCsvPath, MenuExtensionLines))
	{
		return false;
	}

	if (!TestEqual(TEXT("Editor.StateDump test should keep the reload-state CSV header stable"), ReloadStateLines[0], FString(TEXT("Category,OldName,NewName"))))
	{
		return false;
	}
	if (!TestEqual(TEXT("Editor.StateDump test should keep the menu-extension CSV header stable"), MenuExtensionLines[0], FString(TEXT("ExtensionPoint,Location,SectionName"))))
	{
		return false;
	}

	if (!TestEqual(TEXT("Editor.StateDump test should export exactly one reload row plus the header"), ReloadStateLines.Num(), 2))
	{
		return false;
	}
	if (!TestTrue(TEXT("Editor.StateDump test should export the configured ReloadClass row"), ReloadStateLines.Contains(BuildReloadClassRow(AActor::StaticClass(), APawn::StaticClass()))))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("Editor.StateDump test should export the registered menu extension snapshot row"),
			MenuExtensionLines.Contains(BuildMenuSnapshotRow(
				UniqueExtensionPoint,
				EScriptEditorMenuExtensionLocation::LevelViewport_ContextMenu,
				NAME_None))))
	{
		return false;
	}

	AngelscriptEditor::Private::UnregisterStateDumpExtension(StateDumpHandle);
	if (!TestFalse(TEXT("Editor.StateDump test should invalidate the handle after unregister"), StateDumpHandle.IsValid()))
	{
		return false;
	}

	if (!EnsureDirectoryExists(Test, SecondOutputDir))
	{
		return false;
	}

	FAngelscriptStateDump::OnDumpExtensions.Broadcast(SecondOutputDir);
	TestFalse(TEXT("Editor.StateDump test should stop writing EditorReloadState.csv after unregister"), IFileManager::Get().FileExists(*FPaths::Combine(SecondOutputDir, TEXT("EditorReloadState.csv"))));
	return TestFalse(TEXT("Editor.StateDump test should stop writing EditorMenuExtensions.csv after unregister"), IFileManager::Get().FileExists(*FPaths::Combine(SecondOutputDir, TEXT("EditorMenuExtensions.csv"))));
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorStateDumpTests,
	"Angelscript.TestModule.Editor.StateDump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RegistersExtensionAndWritesExpectedCsvFiles)
	{
		ASSERT_THAT(IsTrue(RunRegistersExtensionAndWritesExpectedCsvFiles(*TestRunner)));
	}
};

#endif
