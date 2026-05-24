#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_FileSystem_AngelscriptDiskCompileTests_Private
{
	FString GetDiskCompileTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("DiskCompile"));
	}

	void CleanDiskCompileTestRoot()
	{
		IFileManager::Get().DeleteDirectory(*GetDiskCompileTestRoot(), false, true);
	}

	bool WriteDiskCompileTestFile(const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
	{
		OutAbsolutePath = FPaths::Combine(GetDiskCompileTestRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
		return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDiskCompileReadsUpdatedSourceFromPathTest,
	"Angelscript.TestModule.FileSystem.DiskCompileReadsUpdatedSourceFromPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptDiskCompileReadsUpdatedSourceFromPathTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_AngelscriptDiskCompileTests_Private;
	CleanDiskCompileTestRoot();

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	bool bFinalLookupValid = false;
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("RuntimeDiskModule"));
		CleanDiskCompileTestRoot();
	};

	const FString ScriptV1 = TEXT(R"AS(
int Entry()
{
	return 42;
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
int Entry()
{
	return 17;
}
)AS");

	FString AbsolutePath;
	if (!TestTrue(TEXT("Write initial disk-compile script should succeed"), WriteDiskCompileTestFile(TEXT("RuntimeDiskModule.as"), ScriptV1, AbsolutePath)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Disk compile helper should let runtime read the initial script from disk"), CompileModuleFromDiskPath(&Engine, TEXT("RuntimeDiskModule"), AbsolutePath)))
	{
		return false;
	}

	int32 InitialResult = 0;
	if (!TestTrue(TEXT("Initial disk-compiled module should execute"), ExecuteIntFunction(&Engine, TEXT("RuntimeDiskModule"), TEXT("int Entry()"), InitialResult)))
	{
		return false;
	}
	if (!TestEqual(TEXT("Initial disk-compiled module should return the initial file contents"), InitialResult, 42))
	{
		return false;
	}
	if (!TestTrue(TEXT("Initial disk-compiled module should be discoverable by filename"), Engine.GetModuleByFilename(AbsolutePath).IsValid()))
	{
		return false;
	}

	if (!TestTrue(TEXT("Overwrite disk-compile script with updated contents should succeed"), WriteDiskCompileTestFile(TEXT("RuntimeDiskModule.as"), ScriptV2, AbsolutePath)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Second disk compile should reread the script from disk instead of reusing stale in-memory text"), CompileModuleFromDiskPath(&Engine, TEXT("RuntimeDiskModule"), AbsolutePath)))
	{
		return false;
	}

	int32 UpdatedResult = 0;
	if (!TestTrue(TEXT("Updated disk-compiled module should execute"), ExecuteIntFunction(&Engine, TEXT("RuntimeDiskModule"), TEXT("int Entry()"), UpdatedResult)))
	{
		return false;
	}
	if (!TestEqual(TEXT("Updated disk-compiled module should reflect the latest file contents"), UpdatedResult, 17))
	{
		return false;
	}

	bFinalLookupValid =
		TestTrue(TEXT("Updated disk-compiled module should remain discoverable by filename"), Engine.GetModuleByFilename(AbsolutePath).IsValid());
	}

	return bFinalLookupValid;
}

#endif
