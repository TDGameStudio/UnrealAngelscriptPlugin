#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDiskCompileTest,
	"Angelscript.TestModule.FileSystem.DiskCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static FString GetDiskCompileTestRoot()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("DiskCompile"));
}

static void CleanDiskCompileTestRoot()
{
	IFileManager::Get().DeleteDirectory(*GetDiskCompileTestRoot(), false, true);
}

static bool WriteDiskCompileTestFile(const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
{
	OutAbsolutePath = FPaths::Combine(GetDiskCompileTestRoot(), RelativePath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
	return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

public:
	TEST_METHOD(ReadsUpdatedSourceFromPath)
	{
CleanDiskCompileTestRoot();

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
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
		ASSERT_THAT(IsTrue(WriteDiskCompileTestFile(TEXT("RuntimeDiskModule.as"), ScriptV1, AbsolutePath), TEXT("Write initial disk-compile script should succeed")));

		ASSERT_THAT(IsTrue(CompileModuleFromDiskPath(&Engine, TEXT("RuntimeDiskModule"), AbsolutePath), TEXT("Disk compile helper should let runtime read the initial script from disk")));

	int32 InitialResult = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("RuntimeDiskModule"), TEXT("int Entry()"), InitialResult), TEXT("Initial disk-compiled module should execute")));
		ASSERT_THAT(AreEqual(42, InitialResult, TEXT("Initial disk-compiled module should return the initial file contents")));
		ASSERT_THAT(IsTrue(Engine.GetModuleByFilename(AbsolutePath).IsValid(), TEXT("Initial disk-compiled module should be discoverable by filename")));

		ASSERT_THAT(IsTrue(WriteDiskCompileTestFile(TEXT("RuntimeDiskModule.as"), ScriptV2, AbsolutePath), TEXT("Overwrite disk-compile script with updated contents should succeed")));

		ASSERT_THAT(IsTrue(CompileModuleFromDiskPath(&Engine, TEXT("RuntimeDiskModule"), AbsolutePath), TEXT("Second disk compile should reread the script from disk instead of reusing stale in-memory text")));

	int32 UpdatedResult = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, TEXT("RuntimeDiskModule"), TEXT("int Entry()"), UpdatedResult), TEXT("Updated disk-compiled module should execute")));
		ASSERT_THAT(AreEqual(17, UpdatedResult, TEXT("Updated disk-compiled module should reflect the latest file contents")));

		ASSERT_THAT(IsTrue(Engine.GetModuleByFilename(AbsolutePath).IsValid(), TEXT("Updated disk-compiled module should remain discoverable by filename")));
		}
	}
};

#endif
