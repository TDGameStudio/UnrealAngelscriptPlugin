#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private
{
	FString GetFileSystemTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("FileSystem"));
	}

	FString GetLegacyFileSystemTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Script") / TEXT("Automation") / TEXT("FileSystem"));
	}

	void CleanFileSystemTestRoot()
	{
		IFileManager::Get().DeleteDirectory(*GetFileSystemTestRoot(), false, true);
		IFileManager::Get().DeleteDirectory(*GetLegacyFileSystemTestRoot(), false, true);
	}

	bool WriteFileSystemTestFile(const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
	{
		OutAbsolutePath = FPaths::Combine(GetFileSystemTestRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
		return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptModuleLookupByFilenameTest,
	"Angelscript.TestModule.FileSystem.ModuleLookupByFilename",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompileFromDiskTest,
	"Angelscript.TestModule.FileSystem.CompileFromDisk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPartialFailurePreservesGoodModulesTest,
	"Angelscript.TestModule.FileSystem.PartialFailurePreservesGoodModules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDiscoverScriptFilenamesTest,
	"Angelscript.TestModule.FileSystem.Discovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDiscoverySkipRulesTest,
	"Angelscript.TestModule.FileSystem.SkipRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptRenameUpdatesModuleLookupTest,
	"Angelscript.TestModule.FileSystem.RenameUpdatesModuleLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPathNormalizationLookupTest,
	"Angelscript.TestModule.FileSystem.PathNormalizationLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMixedSuccessFailureRecoveryAndRemapTest,
	"Angelscript.TestModule.FileSystem.MixedSuccessFailureRecoveryAndRemap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)


bool FAngelscriptModuleLookupByFilenameTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private;
	CleanFileSystemTestRoot();

	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString Script = TEXT(R"AS(
int PatrolEntry()
{
	return 5;
}
)AS");
	FString AbsolutePath;
	if (!TestTrue(TEXT("Write module lookup script file should succeed"), WriteFileSystemTestFile(TEXT("Game/AI/Patrol.as"), Script, AbsolutePath)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Compile module lookup script should succeed"), CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), AbsolutePath, Script)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleByName = Engine.GetModule(TEXT("Game.AI.Patrol"));
	TSharedPtr<FAngelscriptModuleDesc> ModuleByFilename = Engine.GetModuleByFilename(AbsolutePath);
	TSharedPtr<FAngelscriptModuleDesc> ModuleByEither = Engine.GetModuleByFilenameOrModuleName(AbsolutePath, TEXT("Game.AI.Patrol"));

	if (!TestTrue(TEXT("Lookup by module name should succeed"), ModuleByName.IsValid()) ||
		!TestTrue(TEXT("Lookup by absolute filename should succeed"), ModuleByFilename.IsValid()) ||
		!TestTrue(TEXT("Lookup by filename-or-module should succeed"), ModuleByEither.IsValid()))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	TestEqual(TEXT("Filename lookup should resolve the same module name"), ModuleByFilename->ModuleName, FString(TEXT("Game.AI.Patrol")));
	TestEqual(TEXT("Filename-or-module lookup should resolve the same module name"), ModuleByEither->ModuleName, FString(TEXT("Game.AI.Patrol")));

	CleanFileSystemTestRoot();
}

	return true;
}

bool FAngelscriptCompileFromDiskTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private;
	CleanFileSystemTestRoot();

	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString Source = TEXT(R"AS(
int Entry()
{
	return 42;
}
)AS");
	FString AbsolutePath;
	if (!TestTrue(TEXT("Write compile-from-disk script file should succeed"), WriteFileSystemTestFile(TEXT("Plain/RuntimeDiskModule.as"), Source, AbsolutePath)))
	{
		return false;
	}

	FString LoadedSource;
	if (!TestTrue(TEXT("Load compile-from-disk script file should succeed"), FFileHelper::LoadFileToString(LoadedSource, *AbsolutePath)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	if (!TestTrue(TEXT("Compile loaded script from disk path should succeed"), CompileModuleFromMemory(&Engine, TEXT("Plain.RuntimeDiskModule"), AbsolutePath, LoadedSource)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	int32 Result = 0;
	if (!TestTrue(TEXT("Execute disk-loaded script should succeed"), ExecuteIntFunction(&Engine, TEXT("Plain.RuntimeDiskModule"), TEXT("int Entry()"), Result)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	TestEqual(TEXT("Disk-loaded script should return expected value"), Result, 42);
	CleanFileSystemTestRoot();
}

	return true;
}

bool FAngelscriptPartialFailurePreservesGoodModulesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private;
	CleanFileSystemTestRoot();

	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString GoodSource = TEXT(R"AS(
int SurvivorEntry()
{
	return 99;
}
)AS");
	FString GoodAbsolutePath;
	if (!TestTrue(TEXT("Write good module script file should succeed"), WriteFileSystemTestFile(TEXT("Good/Survivor.as"), GoodSource, GoodAbsolutePath)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Compile good module from disk path should succeed"), CompileModuleFromMemory(&Engine, TEXT("Good.Survivor"), GoodAbsolutePath, GoodSource)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	int32 SurvivorResult = 0;
	if (!TestTrue(TEXT("Execute good module before failure should succeed"), ExecuteIntFunction(&Engine, TEXT("Good.Survivor"), TEXT("int SurvivorEntry()"), SurvivorResult)))
	{
		CleanFileSystemTestRoot();
		return false;
	}
	TestEqual(TEXT("Good module should return expected value before failure"), SurvivorResult, 99);

	const FString BadSource = TEXT(R"AS(
int BrokenEntry()
{
	return 0;
}
)AS");
	FString BadAbsolutePath;
	if (!TestTrue(TEXT("Write bad module script file should succeed"), WriteFileSystemTestFile(TEXT("Bad/Broken.as"), BadSource, BadAbsolutePath)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	if (!TestTrue(TEXT("Compile second module from disk path should succeed"), CompileModuleFromMemory(&Engine, TEXT("Bad.Broken"), BadAbsolutePath, BadSource)))
	{
		CleanFileSystemTestRoot();
		return false;
	}
	TSharedPtr<FAngelscriptModuleDesc> SurvivorModule = Engine.GetModuleByFilenameOrModuleName(GoodAbsolutePath, TEXT("Good.Survivor"));
	if (!TestTrue(TEXT("Good module should still be discoverable after a failed compile in another module"), SurvivorModule.IsValid()))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	CleanFileSystemTestRoot();
}

	return true;
}

bool FAngelscriptDiscoverScriptFilenamesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private;
	CleanFileSystemTestRoot();

	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	FString UnusedPath;
	if (!TestTrue(TEXT("Write root script file should succeed"), WriteFileSystemTestFile(TEXT("RootScript.as"), TEXT("int Entry() { return 1; }"), UnusedPath)) ||
		!TestTrue(TEXT("Write nested script file should succeed"), WriteFileSystemTestFile(TEXT("Game/Player.as"), TEXT("int Entry() { return 2; }"), UnusedPath)) ||
		!TestTrue(TEXT("Write deeply nested script file should succeed"), WriteFileSystemTestFile(TEXT("Game/AI/Patrol.as"), TEXT("int Entry() { return 3; }"), UnusedPath)) ||
		!TestTrue(TEXT("Write non-script file should succeed"), WriteFileSystemTestFile(TEXT("NotAScript.txt"), TEXT("ignored"), UnusedPath)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, true);
	const TArray<FString> PreviousRoots = Engine.AllRootPaths;
	Engine.AllRootPaths = {GetFileSystemTestRoot()};

	TArray<FAngelscriptEngine::FFilenamePair> Files;
	Engine.FindAllScriptFilenames(Files);

	Engine.AllRootPaths = PreviousRoots;

	TestEqual(TEXT("Discovery should find exactly three .as files"), Files.Num(), 3);

	TSet<FString> FoundRelativePaths;
	for (const FAngelscriptEngine::FFilenamePair& File : Files)
	{
		FoundRelativePaths.Add(File.RelativePath.Replace(TEXT("\\"), TEXT("/")));
	}

	TestTrue(TEXT("Discovery should include RootScript.as"), FoundRelativePaths.Contains(TEXT("RootScript.as")));
	TestTrue(TEXT("Discovery should include Game/Player.as"), FoundRelativePaths.Contains(TEXT("Game/Player.as")));
	TestTrue(TEXT("Discovery should include Game/AI/Patrol.as"), FoundRelativePaths.Contains(TEXT("Game/AI/Patrol.as")));

	CleanFileSystemTestRoot();
}

	return true;
}

bool FAngelscriptDiscoverySkipRulesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private;
	CleanFileSystemTestRoot();

	FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	FString UnusedPath;
	if (!TestTrue(TEXT("Write gameplay script file should succeed"), WriteFileSystemTestFile(TEXT("Gameplay/Main.as"), TEXT("int GameplayEntry() { return 1; }"), UnusedPath)) ||
		!TestTrue(TEXT("Write examples script file should succeed"), WriteFileSystemTestFile(TEXT("Examples/ExampleOnly.as"), TEXT("int ExampleEntry() { return 2; }"), UnusedPath)) ||
		!TestTrue(TEXT("Write dev script file should succeed"), WriteFileSystemTestFile(TEXT("Dev/DevOnly.as"), TEXT("int DevEntry() { return 3; }"), UnusedPath)) ||
		!TestTrue(TEXT("Write editor script file should succeed"), WriteFileSystemTestFile(TEXT("Editor/EditorOnly.as"), TEXT("int EditorEntry() { return 4; }"), UnusedPath)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, false);
	const TArray<FString> PreviousRoots = Engine.AllRootPaths;
	Engine.AllRootPaths = {GetFileSystemTestRoot()};

	TArray<FAngelscriptEngine::FFilenamePair> Files;
	Engine.FindAllScriptFilenames(Files);

	Engine.AllRootPaths = PreviousRoots;

	TestEqual(TEXT("Skip rules should keep only gameplay scripts when editor scripts are disabled"), Files.Num(), 1);
	if (Files.Num() == 1)
	{
		TestEqual(TEXT("Skip rules should keep the gameplay relative path"), Files[0].RelativePath.Replace(TEXT("\\"), TEXT("/")), FString(TEXT("Gameplay/Main.as")));
	}

	CleanFileSystemTestRoot();
}

	return true;
}

bool FAngelscriptRenameUpdatesModuleLookupTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private;
	CleanFileSystemTestRoot();

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString Script = TEXT(R"AS(
int PatrolEntry()
{
	return 7;
}
)AS");

	FString OldAbsolutePath;
	if (!TestTrue(TEXT("Write old filename script should succeed"), WriteFileSystemTestFile(TEXT("Game/AI/OldPatrol.as"), Script, OldAbsolutePath)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Compile old filename module should succeed"), CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), OldAbsolutePath, Script)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	TestTrue(TEXT("Old filename lookup should resolve the original module before rename"), Engine.GetModuleByFilename(OldAbsolutePath).IsValid());
	Engine.DiscardModule(TEXT("Game.AI.Patrol"));

	FString NewAbsolutePath;
	if (!TestTrue(TEXT("Write renamed filename script should succeed"), WriteFileSystemTestFile(TEXT("Game/AI/NewPatrol.as"), Script, NewAbsolutePath)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	if (!TestTrue(TEXT("Compile renamed filename module should succeed"), CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), NewAbsolutePath, Script)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	TestTrue(TEXT("Rename lookup should resolve the module by its new filename"), Engine.GetModuleByFilename(NewAbsolutePath).IsValid());
	TestTrue(TEXT("Rename lookup should stop resolving the old filename after the renamed module is recompiled"), !Engine.GetModuleByFilename(OldAbsolutePath).IsValid());
	TestTrue(TEXT("Rename lookup should keep module-name lookup alive after the filename switch"), Engine.GetModuleByFilenameOrModuleName(NewAbsolutePath, TEXT("Game.AI.Patrol")).IsValid());

	CleanFileSystemTestRoot();
	}

	return true;
}

bool FAngelscriptPathNormalizationLookupTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private;
	CleanFileSystemTestRoot();

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString Script = TEXT(R"AS(
int NormalizeEntry()
{
	return 11;
}
)AS");

	FString AbsolutePath;
	if (!TestTrue(TEXT("Write normalization script should succeed"), WriteFileSystemTestFile(TEXT("Game/Path/Normalize.as"), Script, AbsolutePath)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Compile normalization module should succeed"), CompileModuleFromMemory(&Engine, TEXT("Game.Path.Normalize"), AbsolutePath, Script)))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	const FString BackslashPath = AbsolutePath.Replace(TEXT("/"), TEXT("\\"));
	TSharedPtr<FAngelscriptModuleDesc> ModuleByForwardSlash = Engine.GetModuleByFilename(AbsolutePath);
	TSharedPtr<FAngelscriptModuleDesc> ModuleByEither = Engine.GetModuleByFilenameOrModuleName(BackslashPath, TEXT("Game.Path.Normalize"));

	if (!TestTrue(TEXT("Normalization lookup should resolve the forward-slash absolute filename"), ModuleByForwardSlash.IsValid())
		|| !TestTrue(TEXT("Normalization lookup should resolve filename-or-module after slash normalization"), ModuleByEither.IsValid()))
	{
		CleanFileSystemTestRoot();
		return false;
	}

	TestEqual(TEXT("Normalization lookup should keep the same module name for forward slashes"), ModuleByForwardSlash->ModuleName, FString(TEXT("Game.Path.Normalize")));
	TestEqual(TEXT("Normalization lookup should not duplicate the module when normalizing paths through filename-or-module fallback"), ModuleByEither->ModuleName, FString(TEXT("Game.Path.Normalize")));

	CleanFileSystemTestRoot();
	}

	return true;
}

bool FAngelscriptMixedSuccessFailureRecoveryAndRemapTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptFileSystemTests_Private;
	AddExpectedError(TEXT("Automation/FileSystem/Mixed/Bad.as:"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Identifier 'MissingType' is not a data type"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Hot reload failed due to script compile errors. Keeping all old script code."), EAutomationExpectedErrorFlags::Contains, 1);
	CleanFileSystemTestRoot();

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("Game.Mixed.Good"));
		Engine.DiscardModule(TEXT("Game.Mixed.Bad"));
		CleanFileSystemTestRoot();
	};

	const FString GoodScriptV1 = TEXT(R"AS(
int SurvivorEntry()
{
	return 7;
}
)AS");
	const FString GoodScriptV2 = TEXT(R"AS(
int SurvivorEntry()
{
	return 17;
}
)AS");
	const FString BadBrokenScript = TEXT(R"AS(
int BrokenEntry()
{
	MissingType Value;
	return 1;
}
)AS");
	const FString BadFixedScript = TEXT(R"AS(
int BrokenEntry()
{
	return 23;
}
)AS");

	FString GoodPath;
	FString BadPath;
	if (!TestTrue(TEXT("Write good script should succeed"), WriteFileSystemTestFile(TEXT("Mixed/Good.as"), GoodScriptV1, GoodPath))
		|| !TestTrue(TEXT("Write bad script should succeed"), WriteFileSystemTestFile(TEXT("Mixed/Bad.as"), BadBrokenScript, BadPath)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Compile good module should succeed"), CompileModuleFromMemory(&Engine, TEXT("Game.Mixed.Good"), GoodPath, GoodScriptV1)))
	{
		return false;
	}

	int32 GoodResultBefore = 0;
	if (!TestTrue(TEXT("Good module should execute before bad compile"), ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Good"), TEXT("int SurvivorEntry()"), GoodResultBefore)))
	{
		return false;
	}
	TestEqual(TEXT("Good module should return initial value"), GoodResultBefore, 7);

	ECompileResult BadCompileResult = ECompileResult::FullyHandled;
	const bool bBadCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("Game.Mixed.Bad"),
		BadPath,
		BadBrokenScript,
		BadCompileResult);
	TestFalse(TEXT("Broken bad module compile should fail"), bBadCompiled);
	TestTrue(TEXT("Broken bad module compile should report error state"), BadCompileResult == ECompileResult::Error || BadCompileResult == ECompileResult::ErrorNeedFullReload);

	int32 GoodResultAfterBadFailure = 0;
	if (!TestTrue(TEXT("Good module should keep executing after unrelated bad compile failure"), ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Good"), TEXT("int SurvivorEntry()"), GoodResultAfterBadFailure)))
	{
		return false;
	}
	TestEqual(TEXT("Good module should still return initial value after bad compile failure"), GoodResultAfterBadFailure, 7);

	if (!TestTrue(TEXT("Recompile good module update should succeed"), CompileModuleFromMemory(&Engine, TEXT("Game.Mixed.Good"), GoodPath, GoodScriptV2)))
	{
		return false;
	}

	int32 GoodResultAfterUpdate = 0;
	if (!TestTrue(TEXT("Updated good module should execute"), ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Good"), TEXT("int SurvivorEntry()"), GoodResultAfterUpdate)))
	{
		return false;
	}
	TestEqual(TEXT("Updated good module should return new value"), GoodResultAfterUpdate, 17);

	if (!TestTrue(TEXT("Fix bad script on disk should succeed"), WriteFileSystemTestFile(TEXT("Mixed/Bad.as"), BadFixedScript, BadPath)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Compile fixed bad module should succeed"), CompileModuleFromMemory(&Engine, TEXT("Game.Mixed.Bad"), BadPath, BadFixedScript)))
	{
		return false;
	}

	int32 BadResultAfterFix = 0;
	if (!TestTrue(TEXT("Fixed bad module should execute"), ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Bad"), TEXT("int BrokenEntry()"), BadResultAfterFix)))
	{
		return false;
	}
	TestEqual(TEXT("Fixed bad module should return expected value"), BadResultAfterFix, 23);

	const FString GoodPathRenamed = FPaths::Combine(GetFileSystemTestRoot(), TEXT("Mixed/GoodRenamed.as"));
	if (!TestTrue(TEXT("Rename good script file should succeed"), IFileManager::Get().Move(*GoodPathRenamed, *GoodPath, true, true)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Recompile good module with renamed path should succeed"), CompileModuleFromMemory(&Engine, TEXT("Game.Mixed.Good"), GoodPathRenamed, GoodScriptV2)))
	{
		return false;
	}

	TestTrue(TEXT("Renamed good path lookup should resolve module"), Engine.GetModuleByFilename(GoodPathRenamed).IsValid());
	TestTrue(TEXT("Old good path lookup should no longer resolve module after remap"), !Engine.GetModuleByFilename(GoodPath).IsValid());

	int32 GoodResultAfterRename = 0;
	if (!TestTrue(TEXT("Renamed good module should still execute"), ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Good"), TEXT("int SurvivorEntry()"), GoodResultAfterRename)))
	{
		return false;
	}
	TestEqual(TEXT("Renamed good module should preserve updated behavior"), GoodResultAfterRename, 17);

	}

	return true;
}

#endif

