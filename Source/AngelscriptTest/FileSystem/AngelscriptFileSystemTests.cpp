#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptFileSystemTest,
	"Angelscript.TestModule.FileSystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static FString GetFileSystemTestRoot()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("FileSystem"));
}

static FString GetLegacyFileSystemTestRoot()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Script") / TEXT("Automation") / TEXT("FileSystem"));
}

static void CleanFileSystemTestRoot()
{
	IFileManager::Get().DeleteDirectory(*GetFileSystemTestRoot(), false, true);
	IFileManager::Get().DeleteDirectory(*GetLegacyFileSystemTestRoot(), false, true);
}

static bool WriteFileSystemTestFile(const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
{
	OutAbsolutePath = FPaths::Combine(GetFileSystemTestRoot(), RelativePath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
	return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

public:
	TEST_METHOD(ModuleLookupByFilename)
	{
CleanFileSystemTestRoot();
	ON_SCOPE_EXIT
	{
		CleanFileSystemTestRoot();
	};

FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString Script = TEXT(R"AS(
int PatrolEntry()
{
	return 5;
}
)AS");
	FString AbsolutePath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Game/AI/Patrol.as"), Script, AbsolutePath), TEXT("Write module lookup script file should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), AbsolutePath, Script), TEXT("Compile module lookup script should succeed")));

	TSharedPtr<FAngelscriptModuleDesc> ModuleByName = Engine.GetModule(TEXT("Game.AI.Patrol"));
	TSharedPtr<FAngelscriptModuleDesc> ModuleByFilename = Engine.GetModuleByFilename(AbsolutePath);
	TSharedPtr<FAngelscriptModuleDesc> ModuleByEither = Engine.GetModuleByFilenameOrModuleName(AbsolutePath, TEXT("Game.AI.Patrol"));

	ASSERT_THAT(IsTrue(ModuleByName.IsValid(), TEXT("Lookup by module name should succeed")));
	ASSERT_THAT(IsTrue(ModuleByFilename.IsValid(), TEXT("Lookup by absolute filename should succeed")));
	ASSERT_THAT(IsTrue(ModuleByEither.IsValid(), TEXT("Lookup by filename-or-module should succeed")));

	ASSERT_THAT(AreEqual(FString(TEXT("Game.AI.Patrol")), ModuleByFilename->ModuleName, TEXT("Filename lookup should resolve the same module name")));
	ASSERT_THAT(AreEqual(FString(TEXT("Game.AI.Patrol")), ModuleByEither->ModuleName, TEXT("Filename-or-module lookup should resolve the same module name")));
}
	}

	TEST_METHOD(CompileFromDisk)
	{
CleanFileSystemTestRoot();
	ON_SCOPE_EXIT
	{
		CleanFileSystemTestRoot();
	};

FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString Source = TEXT(R"AS(
int Entry()
{
	return 42;
}
)AS");
	FString AbsolutePath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Plain/RuntimeDiskModule.as"), Source, AbsolutePath), TEXT("Write compile-from-disk script file should succeed")));

	FString LoadedSource;
	ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(LoadedSource, *AbsolutePath), TEXT("Load compile-from-disk script file should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Plain.RuntimeDiskModule"), AbsolutePath, LoadedSource), TEXT("Compile loaded script from disk path should succeed")));

	int32 Result = 0;
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("Plain.RuntimeDiskModule"), TEXT("int Entry()"), Result), TEXT("Execute disk-loaded script should succeed")));

	ASSERT_THAT(AreEqual(42, Result, TEXT("Disk-loaded script should return expected value")));
}
	}

	TEST_METHOD(PartialFailurePreservesGoodModules)
	{
CleanFileSystemTestRoot();
	ON_SCOPE_EXIT
	{
		CleanFileSystemTestRoot();
	};

FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString GoodSource = TEXT(R"AS(
int SurvivorEntry()
{
	return 99;
}
)AS");
	FString GoodAbsolutePath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Good/Survivor.as"), GoodSource, GoodAbsolutePath), TEXT("Write good module script file should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Good.Survivor"), GoodAbsolutePath, GoodSource), TEXT("Compile good module from disk path should succeed")));

	int32 SurvivorResult = 0;
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("Good.Survivor"), TEXT("int SurvivorEntry()"), SurvivorResult), TEXT("Execute good module before failure should succeed")));
	ASSERT_THAT(AreEqual(99, SurvivorResult, TEXT("Good module should return expected value before failure")));

	const FString BadSource = TEXT(R"AS(
int BrokenEntry()
{
	return 0;
}
)AS");
	FString BadAbsolutePath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Bad/Broken.as"), BadSource, BadAbsolutePath), TEXT("Write bad module script file should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Bad.Broken"), BadAbsolutePath, BadSource), TEXT("Compile second module from disk path should succeed")));
	TSharedPtr<FAngelscriptModuleDesc> SurvivorModule = Engine.GetModuleByFilenameOrModuleName(GoodAbsolutePath, TEXT("Good.Survivor"));
	ASSERT_THAT(IsTrue(SurvivorModule.IsValid(), TEXT("Good module should still be discoverable after a failed compile in another module")));
}
	}

	TEST_METHOD(Discovery)
	{
CleanFileSystemTestRoot();
	ON_SCOPE_EXIT
	{
		CleanFileSystemTestRoot();
	};

FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	FString UnusedPath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("RootScript.as"), TEXT("int Entry() { return 1; }"), UnusedPath), TEXT("Write root script file should succeed")));
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Game/Player.as"), TEXT("int Entry() { return 2; }"), UnusedPath), TEXT("Write nested script file should succeed")));
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Game/AI/Patrol.as"), TEXT("int Entry() { return 3; }"), UnusedPath), TEXT("Write deeply nested script file should succeed")));
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("NotAScript.txt"), TEXT("ignored"), UnusedPath), TEXT("Write non-script file should succeed")));

	TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, true);
	const TArray<FString> PreviousRoots = Engine.AllRootPaths;
	Engine.AllRootPaths = {GetFileSystemTestRoot()};

	TArray<FAngelscriptEngine::FFilenamePair> Files;
	Engine.FindAllScriptFilenames(Files);

	Engine.AllRootPaths = PreviousRoots;

	ASSERT_THAT(AreEqual(3, Files.Num(), TEXT("Discovery should find exactly three .as files")));

	TSet<FString> FoundRelativePaths;
	for (const FAngelscriptEngine::FFilenamePair& File : Files)
	{
		FoundRelativePaths.Add(File.RelativePath.Replace(TEXT("\\"), TEXT("/")));
	}

	ASSERT_THAT(IsTrue(FoundRelativePaths.Contains(TEXT("RootScript.as")), TEXT("Discovery should include RootScript.as")));
	ASSERT_THAT(IsTrue(FoundRelativePaths.Contains(TEXT("Game/Player.as")), TEXT("Discovery should include Game/Player.as")));
	ASSERT_THAT(IsTrue(FoundRelativePaths.Contains(TEXT("Game/AI/Patrol.as")), TEXT("Discovery should include Game/AI/Patrol.as")));
}
	}

	TEST_METHOD(SkipRules)
	{
CleanFileSystemTestRoot();
	ON_SCOPE_EXIT
	{
		CleanFileSystemTestRoot();
	};

FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	FString UnusedPath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Gameplay/Main.as"), TEXT("int GameplayEntry() { return 1; }"), UnusedPath), TEXT("Write gameplay script file should succeed")));
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Examples/ExampleOnly.as"), TEXT("int ExampleEntry() { return 2; }"), UnusedPath), TEXT("Write examples script file should succeed")));
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Dev/DevOnly.as"), TEXT("int DevEntry() { return 3; }"), UnusedPath), TEXT("Write dev script file should succeed")));
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Editor/EditorOnly.as"), TEXT("int EditorEntry() { return 4; }"), UnusedPath), TEXT("Write editor script file should succeed")));

	TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, false);
	const TArray<FString> PreviousRoots = Engine.AllRootPaths;
	Engine.AllRootPaths = {GetFileSystemTestRoot()};

	TArray<FAngelscriptEngine::FFilenamePair> Files;
	Engine.FindAllScriptFilenames(Files);

	Engine.AllRootPaths = PreviousRoots;

	ASSERT_THAT(AreEqual(1, Files.Num(), TEXT("Skip rules should keep only gameplay scripts when editor scripts are disabled")));
	if (Files.Num() == 1)
	{
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay/Main.as")), Files[0].RelativePath.Replace(TEXT("\\"), TEXT("/")), TEXT("Skip rules should keep the gameplay relative path")));
	}
}
	}

	TEST_METHOD(RenameUpdatesModuleLookup)
	{
CleanFileSystemTestRoot();
	ON_SCOPE_EXIT
	{
		CleanFileSystemTestRoot();
	};

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString Script = TEXT(R"AS(
int PatrolEntry()
{
	return 7;
}
)AS");

	FString OldAbsolutePath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Game/AI/OldPatrol.as"), Script, OldAbsolutePath), TEXT("Write old filename script should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), OldAbsolutePath, Script), TEXT("Compile old filename module should succeed")));

	ASSERT_THAT(IsTrue(Engine.GetModuleByFilename(OldAbsolutePath).IsValid(), TEXT("Old filename lookup should resolve the original module before rename")));
	Engine.DiscardModule(TEXT("Game.AI.Patrol"));

	FString NewAbsolutePath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Game/AI/NewPatrol.as"), Script, NewAbsolutePath), TEXT("Write renamed filename script should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.AI.Patrol"), NewAbsolutePath, Script), TEXT("Compile renamed filename module should succeed")));

	ASSERT_THAT(IsTrue(Engine.GetModuleByFilename(NewAbsolutePath).IsValid(), TEXT("Rename lookup should resolve the module by its new filename")));
	ASSERT_THAT(IsTrue(!Engine.GetModuleByFilename(OldAbsolutePath).IsValid(), TEXT("Rename lookup should stop resolving the old filename after the renamed module is recompiled")));
	ASSERT_THAT(IsTrue(Engine.GetModuleByFilenameOrModuleName(NewAbsolutePath, TEXT("Game.AI.Patrol")).IsValid(), TEXT("Rename lookup should keep module-name lookup alive after the filename switch")));
}
	}

	TEST_METHOD(PathNormalizationLookup)
	{
CleanFileSystemTestRoot();
	ON_SCOPE_EXIT
	{
		CleanFileSystemTestRoot();
	};

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString Script = TEXT(R"AS(
int NormalizeEntry()
{
	return 11;
}
)AS");

	FString AbsolutePath;
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Game/Path/Normalize.as"), Script, AbsolutePath), TEXT("Write normalization script should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.Path.Normalize"), AbsolutePath, Script), TEXT("Compile normalization module should succeed")));

	const FString BackslashPath = AbsolutePath.Replace(TEXT("/"), TEXT("\\"));
	TSharedPtr<FAngelscriptModuleDesc> ModuleByForwardSlash = Engine.GetModuleByFilename(AbsolutePath);
	TSharedPtr<FAngelscriptModuleDesc> ModuleByEither = Engine.GetModuleByFilenameOrModuleName(BackslashPath, TEXT("Game.Path.Normalize"));

	ASSERT_THAT(IsTrue(ModuleByForwardSlash.IsValid(), TEXT("Normalization lookup should resolve the forward-slash absolute filename")));
	ASSERT_THAT(IsTrue(ModuleByEither.IsValid(), TEXT("Normalization lookup should resolve filename-or-module after slash normalization")));

	ASSERT_THAT(AreEqual(FString(TEXT("Game.Path.Normalize")), ModuleByForwardSlash->ModuleName, TEXT("Normalization lookup should keep the same module name for forward slashes")));
	ASSERT_THAT(AreEqual(FString(TEXT("Game.Path.Normalize")), ModuleByEither->ModuleName, TEXT("Normalization lookup should not duplicate the module when normalizing paths through filename-or-module fallback")));
}
	}

	TEST_METHOD(MixedSuccessFailureRecoveryAndRemap)
	{
TestRunner->AddExpectedError(TEXT("Automation/FileSystem/Mixed/Bad.as:"), EAutomationExpectedErrorFlags::Contains, 1);
	TestRunner->AddExpectedError(TEXT("Identifier 'MissingType' is not a data type"), EAutomationExpectedErrorFlags::Contains, 1);
	TestRunner->AddExpectedError(TEXT("Hot reload failed due to script compile errors. Keeping all old script code."), EAutomationExpectedErrorFlags::Contains, 1);
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
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Mixed/Good.as"), GoodScriptV1, GoodPath), TEXT("Write good script should succeed")));
	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Mixed/Bad.as"), BadBrokenScript, BadPath), TEXT("Write bad script should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.Mixed.Good"), GoodPath, GoodScriptV1), TEXT("Compile good module should succeed")));

	int32 GoodResultBefore = 0;
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Good"), TEXT("int SurvivorEntry()"), GoodResultBefore), TEXT("Good module should execute before bad compile")));
	ASSERT_THAT(AreEqual(7, GoodResultBefore, TEXT("Good module should return initial value")));

	ECompileResult BadCompileResult = ECompileResult::FullyHandled;
	const bool bBadCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("Game.Mixed.Bad"),
		BadPath,
		BadBrokenScript,
		BadCompileResult);
	ASSERT_THAT(IsFalse(bBadCompiled, TEXT("Broken bad module compile should fail")));
	ASSERT_THAT(IsTrue(BadCompileResult == ECompileResult::Error || BadCompileResult == ECompileResult::ErrorNeedFullReload, TEXT("Broken bad module compile should report error state")));

	int32 GoodResultAfterBadFailure = 0;
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Good"), TEXT("int SurvivorEntry()"), GoodResultAfterBadFailure), TEXT("Good module should keep executing after unrelated bad compile failure")));
	ASSERT_THAT(AreEqual(7, GoodResultAfterBadFailure, TEXT("Good module should still return initial value after bad compile failure")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.Mixed.Good"), GoodPath, GoodScriptV2), TEXT("Recompile good module update should succeed")));

	int32 GoodResultAfterUpdate = 0;
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Good"), TEXT("int SurvivorEntry()"), GoodResultAfterUpdate), TEXT("Updated good module should execute")));
	ASSERT_THAT(AreEqual(17, GoodResultAfterUpdate, TEXT("Updated good module should return new value")));

	ASSERT_THAT(IsTrue(WriteFileSystemTestFile(TEXT("Mixed/Bad.as"), BadFixedScript, BadPath), TEXT("Fix bad script on disk should succeed")));
	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.Mixed.Bad"), BadPath, BadFixedScript), TEXT("Compile fixed bad module should succeed")));

	int32 BadResultAfterFix = 0;
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Bad"), TEXT("int BrokenEntry()"), BadResultAfterFix), TEXT("Fixed bad module should execute")));
	ASSERT_THAT(AreEqual(23, BadResultAfterFix, TEXT("Fixed bad module should return expected value")));

	const FString GoodPathRenamed = FPaths::Combine(GetFileSystemTestRoot(), TEXT("Mixed/GoodRenamed.as"));
	ASSERT_THAT(IsTrue(IFileManager::Get().Move(*GoodPathRenamed, *GoodPath, true, true), TEXT("Rename good script file should succeed")));

	ASSERT_THAT(IsTrue(CompileModuleFromMemory(&Engine, TEXT("Game.Mixed.Good"), GoodPathRenamed, GoodScriptV2), TEXT("Recompile good module with renamed path should succeed")));

	ASSERT_THAT(IsTrue(Engine.GetModuleByFilename(GoodPathRenamed).IsValid(), TEXT("Renamed good path lookup should resolve module")));
	ASSERT_THAT(IsTrue(!Engine.GetModuleByFilename(GoodPath).IsValid(), TEXT("Old good path lookup should no longer resolve module after remap")));

	int32 GoodResultAfterRename = 0;
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("Game.Mixed.Good"), TEXT("int SurvivorEntry()"), GoodResultAfterRename), TEXT("Renamed good module should still execute")));
	ASSERT_THAT(AreEqual(17, GoodResultAfterRename, TEXT("Renamed good module should preserve updated behavior")));

	}
	}
};

#endif

