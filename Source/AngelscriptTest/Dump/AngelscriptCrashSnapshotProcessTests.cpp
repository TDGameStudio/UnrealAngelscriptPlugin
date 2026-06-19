#include "CQTest.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

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

TEST_CLASS_WITH_FLAGS(FAngelscriptCrashSnapshotChildProcessTest,
	"Angelscript.CrashOnly.CrashSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ChildProcessDebugCrash)
	{
		using namespace AngelscriptTest_Dump_CrashSnapshotProcessTests_Private;

		if (!FParse::Param(FCommandLine::Get(), TEXT("AngelscriptRunCrashOnlyTests")))
		{
			TestRunner->AddError(TEXT("Use Tools\\RunTests.ps1 -TestPrefix \"Angelscript.CrashOnly.CrashSnapshot\" -Label crash-snapshot -TimeoutMs 600000."));
		}
		ASSERT_THAT(IsTrue(
			FParse::Param(FCommandLine::Get(), TEXT("AngelscriptRunCrashOnlyTests")),
			TEXT("Crash-only tests must be launched by RunTests.ps1 with explicit opt-in")));

		const FString EditorCmd = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/Win64/UnrealEditor-Cmd.exe")));
		if (!IFileManager::Get().FileExists(*EditorCmd))
		{
			TestRunner->AddError(EditorCmd);
		}
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*EditorCmd), TEXT("Crash child process should use an existing UnrealEditor-Cmd.exe")));

		const FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		if (!IFileManager::Get().FileExists(*ProjectFile))
		{
			TestRunner->AddError(ProjectFile);
		}
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*ProjectFile), TEXT("Crash child process should use an existing project file")));

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

		if (!ProcHandle.IsValid())
		{
			TestRunner->AddError(FString::Printf(TEXT("EditorCmd='%s' Params='%s'"), *EditorCmd, *Params));
		}
		ASSERT_THAT(IsTrue(ProcHandle.IsValid(), TEXT("Crash child process should launch")));

		int32 ReturnCode = 0;
		if (!WaitForProcWithTimeout(ProcHandle, 180.0, ReturnCode))
		{
			FPlatformProcess::TerminateProc(ProcHandle, true);
			TestRunner->AddError(FString::Printf(TEXT("Crash child process timed out. Child log: '%s'"), *ChildLogPath));
			return;
		}

		ASSERT_THAT(IsFalse(ReturnCode == 0, TEXT("Crash child process should exit with a non-zero code")));

		if (!IFileManager::Get().FileExists(*SnapshotPath))
		{
			TryFindCrashReportSnapshotByMarker(Marker, SnapshotPath);
		}

		if (!IFileManager::Get().FileExists(*SnapshotPath))
		{
			TestRunner->AddError(FString::Printf(TEXT("SnapshotPath='%s' ChildLog='%s' ReturnCode=%d ProcessId=%u"), *SnapshotPath, *ChildLogPath, ReturnCode, ProcessId));
		}
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*SnapshotPath), TEXT("Crash child process should write the AS snapshot")));

		TSharedPtr<FJsonObject> SnapshotObject;
		ASSERT_THAT(IsTrue(LoadSnapshotJson(*TestRunner, SnapshotPath, SnapshotObject), TEXT("Crash child snapshot JSON should load")));

		ASSERT_THAT(AreEqual(Marker, SnapshotObject->GetStringField(TEXT("marker")), TEXT("Crash child snapshot should store the configured marker")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Number>(TEXT("processId")), TEXT("Crash child snapshot should store process id")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Boolean>(TEXT("engineInitialized")), TEXT("Crash child snapshot should store engine initialization state")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Number>(TEXT("activeModuleCount")), TEXT("Crash child snapshot should store active module count")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Array>(TEXT("modules")), TEXT("Crash child snapshot should store modules array")));
	}
};

#endif
