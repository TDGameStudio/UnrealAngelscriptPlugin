#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrashSnapshotChildProcessDebugCrashTest,
	"Angelscript.CrashOnly.CrashSnapshot.ChildProcessDebugCrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace AngelscriptTest_Dump_CrashSnapshotProcessTests_Private
{
	FString QuoteArg(const FString& Arg)
	{
		return FString::Printf(TEXT("\"%s\""), *Arg.Replace(TEXT("\""), TEXT("\\\"")));
	}

	FString MakeUniqueCrashSnapshotDir()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("CrashSnapshot"),
			TEXT("ChildProcess"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	bool LoadSnapshotJson(FAutomationTestBase& Test, const FString& SnapshotPath, TSharedPtr<FJsonObject>& OutObject)
	{
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *SnapshotPath))
		{
			Test.AddError(FString::Printf(TEXT("Failed to load crash snapshot '%s'"), *SnapshotPath));
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("Failed to parse crash snapshot JSON '%s'"), *SnapshotPath));
			return false;
		}

		return true;
	}

	bool WaitForProcWithTimeout(FProcHandle& ProcHandle, const double TimeoutSeconds, int32& OutReturnCode)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		while (FPlatformTime::Seconds() - StartSeconds < TimeoutSeconds)
		{
			if (FPlatformProcess::GetProcReturnCode(ProcHandle, &OutReturnCode))
			{
				return true;
			}

			FPlatformProcess::Sleep(0.25f);
		}

		return false;
	}

	bool TryFindCrashReportSnapshotByMarker(const FString& Marker, FString& OutSnapshotPath)
	{
		const FString CrashReportRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Crashes"));
		TArray<FString> SnapshotFiles;
		IFileManager::Get().FindFilesRecursive(
			SnapshotFiles,
			*CrashReportRoot,
			TEXT("AngelscriptCrashSnapshot.json"),
			true,
			false);

		for (const FString& CandidatePath : SnapshotFiles)
		{
			FString JsonString;
			if (FFileHelper::LoadFileToString(JsonString, *CandidatePath) && JsonString.Contains(Marker))
			{
				OutSnapshotPath = CandidatePath;
				return true;
			}
		}

		return false;
	}
}

bool FAngelscriptCrashSnapshotChildProcessDebugCrashTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Dump_CrashSnapshotProcessTests_Private;

	if (!TestTrue(TEXT("Crash-only tests must be launched by RunTests.ps1 with explicit opt-in"), FParse::Param(FCommandLine::Get(), TEXT("AngelscriptRunCrashOnlyTests"))))
	{
		AddError(TEXT("Use Tools\\RunTests.ps1 -TestPrefix \"Angelscript.CrashOnly.CrashSnapshot\" -Label crash-snapshot -TimeoutMs 600000."));
		return false;
	}

	const FString EditorCmd = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/Win64/UnrealEditor-Cmd.exe")));
	if (!TestTrue(TEXT("Crash child process should use an existing UnrealEditor-Cmd.exe"), IFileManager::Get().FileExists(*EditorCmd)))
	{
		AddError(EditorCmd);
		return false;
	}

	const FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	if (!TestTrue(TEXT("Crash child process should use an existing project file"), IFileManager::Get().FileExists(*ProjectFile)))
	{
		AddError(ProjectFile);
		return false;
	}

	const FString OutputDir = MakeUniqueCrashSnapshotDir();
	const FString Marker = FString::Printf(TEXT("child-process-debug-crash-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString ChildLogPath = FPaths::Combine(OutputDir, TEXT("ChildProcess.log"));
	FString SnapshotPath = FPaths::Combine(OutputDir, TEXT("AngelscriptCrashSnapshot.json"));

	const FString ExecCmds = FString::Printf(
		TEXT("as.Test.ConfigureCrashSnapshot %s %s, DEBUG CRASH"),
		*OutputDir,
		*Marker);

	const TArray<FString> ChildArgs = {
		QuoteArg(ProjectFile),
		FString::Printf(TEXT("-ExecCmds=%s"), *QuoteArg(ExecCmds)),
		TEXT("-BUILDMACHINE"),
		TEXT("-Unattended"),
		TEXT("-NoPause"),
		TEXT("-NoSplash"),
		TEXT("-stdout"),
		TEXT("-FullStdOutLogOutput"),
		TEXT("-UTF8Output"),
		TEXT("-CrashForUAT"),
		TEXT("-NOSOUND"),
		TEXT("-NullRHI"),
		FString::Printf(TEXT("-ABSLOG=%s"), *QuoteArg(ChildLogPath))
	};
	const FString Params = FString::Join(ChildArgs, TEXT(" "));

	uint32 ProcessId = 0;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*EditorCmd,
		*Params,
		false,
		true,
		true,
		&ProcessId,
		0,
		*FPaths::ProjectDir(),
		nullptr,
		nullptr);

	if (!TestTrue(TEXT("Crash child process should launch"), ProcHandle.IsValid()))
	{
		AddError(FString::Printf(TEXT("EditorCmd='%s' Params='%s'"), *EditorCmd, *Params));
		return false;
	}

	int32 ReturnCode = 0;
	if (!WaitForProcWithTimeout(ProcHandle, 180.0, ReturnCode))
	{
		FPlatformProcess::TerminateProc(ProcHandle, true);
		AddError(FString::Printf(TEXT("Crash child process timed out. Child log: '%s'"), *ChildLogPath));
		return false;
	}

	TestNotEqual(TEXT("Crash child process should exit with a non-zero code"), ReturnCode, 0);

	if (!IFileManager::Get().FileExists(*SnapshotPath))
	{
		TryFindCrashReportSnapshotByMarker(Marker, SnapshotPath);
	}

	if (!TestTrue(TEXT("Crash child process should write the AS snapshot"), IFileManager::Get().FileExists(*SnapshotPath)))
	{
		AddError(FString::Printf(TEXT("SnapshotPath='%s' ChildLog='%s' ReturnCode=%d ProcessId=%u"), *SnapshotPath, *ChildLogPath, ReturnCode, ProcessId));
		return false;
	}

	TSharedPtr<FJsonObject> SnapshotObject;
	if (!LoadSnapshotJson(*this, SnapshotPath, SnapshotObject))
	{
		return false;
	}

	TestEqual(TEXT("Crash child snapshot should store the configured marker"), SnapshotObject->GetStringField(TEXT("marker")), Marker);
	TestTrue(TEXT("Crash child snapshot should store process id"), SnapshotObject->HasTypedField<EJson::Number>(TEXT("processId")));
	TestTrue(TEXT("Crash child snapshot should store engine initialization state"), SnapshotObject->HasTypedField<EJson::Boolean>(TEXT("engineInitialized")));
	TestTrue(TEXT("Crash child snapshot should store active module count"), SnapshotObject->HasTypedField<EJson::Number>(TEXT("activeModuleCount")));
	TestTrue(TEXT("Crash child snapshot should store modules array"), SnapshotObject->HasTypedField<EJson::Array>(TEXT("modules")));

	return true;
}

#endif
