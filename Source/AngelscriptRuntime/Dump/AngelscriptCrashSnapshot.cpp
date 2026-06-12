#include "Dump/AngelscriptCrashSnapshot.h"

#include "Core/AngelscriptEngine.h"

#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTLS.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptCrashSnapshot, Log, All);

namespace AngelscriptCrashSnapshot_Private
{
	FDelegateHandle GSystemErrorHandle;
	FString GOverrideOutputDir;
	FString GMarker;
	std::atomic<bool> GHandlingCrash(false);

	FString SanitizeCommandArg(FString Arg)
	{
		Arg.TrimStartAndEndInline();
		while (Arg.EndsWith(TEXT(";")))
		{
			Arg.LeftChopInline(1, EAllowShrinking::No);
			Arg.TrimEndInline();
		}

		if (Arg.Len() >= 2 && Arg.StartsWith(TEXT("\"")) && Arg.EndsWith(TEXT("\"")))
		{
			Arg = Arg.Mid(1, Arg.Len() - 2);
		}

		return Arg;
	}

	FString MakeTimestampDirectoryName()
	{
		return FString::Printf(
			TEXT("%u_%s"),
			FPlatformProcess::GetCurrentProcessId(),
			*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_%s")));
	}

	FString ResolveOutputDir(const FString& OutputDir)
	{
		if (!OutputDir.IsEmpty())
		{
			return FPaths::ConvertRelativePathToFull(OutputDir);
		}

		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Angelscript"),
			TEXT("CrashSnapshots"),
			MakeTimestampDirectoryName());
	}

	TSharedRef<FJsonObject> BuildModuleObject(const FAngelscriptModuleDesc& Module)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("name"), Module.ModuleName);
		Object->SetNumberField(TEXT("classCount"), Module.Classes.Num());
		Object->SetNumberField(TEXT("enumCount"), Module.Enums.Num());
		Object->SetNumberField(TEXT("delegateCount"), Module.Delegates.Num());
		Object->SetNumberField(TEXT("codeSectionCount"), Module.Code.Num());
		Object->SetBoolField(TEXT("compileError"), Module.bCompileError);
		Object->SetBoolField(TEXT("loadedPrecompiledCode"), Module.bLoadedPrecompiledCode);
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> BuildModulesArray(FAngelscriptEngine* Engine, int32& OutClassCount, int32& OutEnumCount, int32& OutDelegateCount)
	{
		TArray<TSharedPtr<FJsonValue>> Modules;
		OutClassCount = 0;
		OutEnumCount = 0;
		OutDelegateCount = 0;

		if (Engine == nullptr)
		{
			return Modules;
		}

		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine->GetActiveModules();
		Modules.Reserve(ActiveModules.Num());
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			OutClassCount += Module->Classes.Num();
			OutEnumCount += Module->Enums.Num();
			OutDelegateCount += Module->Delegates.Num();
			Modules.Add(MakeShared<FJsonValueObject>(BuildModuleObject(Module.Get())));
		}

		return Modules;
	}

	TArray<TSharedPtr<FJsonValue>> BuildDebugStackArray()
	{
		TArray<TSharedPtr<FJsonValue>> Frames;
		if (GAngelscriptStack == nullptr)
		{
			return Frames;
		}

		const int32 MaxFrames = 16;
		const int32 FrameCount = FMath::Min(GAngelscriptStack->Frames.Num(), MaxFrames);
		Frames.Reserve(FrameCount);
		for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
		{
			const FAngelscriptEngine::FAngelscriptDebugFrame& Frame = GAngelscriptStack->Frames[FrameIndex];
			TSharedRef<FJsonObject> FrameObject = MakeShared<FJsonObject>();
			FrameObject->SetNumberField(TEXT("index"), FrameIndex);
			FrameObject->SetStringField(TEXT("file"), Frame.File != nullptr ? ANSI_TO_TCHAR(Frame.File) : FString());
			FrameObject->SetStringField(TEXT("function"), Frame.Function != nullptr ? ANSI_TO_TCHAR(Frame.Function) : FString());
			FrameObject->SetStringField(TEXT("class"), Frame.Class != nullptr ? ANSI_TO_TCHAR(Frame.Class) : FString());
			FrameObject->SetNumberField(TEXT("line"), Frame.LineNumber);
			FrameObject->SetStringField(TEXT("thisObject"), Frame.This != nullptr ? GetPathNameSafe(Frame.This) : FString());
			Frames.Add(MakeShared<FJsonValueObject>(FrameObject));
		}

		return Frames;
	}

	FAngelscriptCrashSnapshot::FWriteResult WriteSnapshot(const FString& OutputDir, const FString& Marker, const bool bAttachToCrashContext)
	{
		FAngelscriptCrashSnapshot::FWriteResult Result;
		const FString ResolvedOutputDir = ResolveOutputDir(OutputDir);

		if (!IFileManager::Get().DirectoryExists(*ResolvedOutputDir)
			&& !IFileManager::Get().MakeDirectory(*ResolvedOutputDir, true))
		{
			Result.ErrorMessage = FString::Printf(TEXT("Failed to create crash snapshot directory '%s'."), *ResolvedOutputDir);
			return Result;
		}

		FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
		const bool bEngineInitialized = Engine != nullptr && Engine->GetScriptEngine() != nullptr;

		int32 TotalClassCount = 0;
		int32 TotalEnumCount = 0;
		int32 TotalDelegateCount = 0;
		TArray<TSharedPtr<FJsonValue>> Modules = BuildModulesArray(Engine, TotalClassCount, TotalEnumCount, TotalDelegateCount);

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schemaVersion"), 1);
		Root->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
		Root->SetStringField(TEXT("marker"), Marker);
		Root->SetNumberField(TEXT("processId"), FPlatformProcess::GetCurrentProcessId());
		Root->SetNumberField(TEXT("threadId"), FPlatformTLS::GetCurrentThreadId());
		Root->SetBoolField(TEXT("enginePresent"), Engine != nullptr);
		Root->SetBoolField(TEXT("engineInitialized"), bEngineInitialized);
		Root->SetBoolField(TEXT("initialCompileFinished"), Engine != nullptr && Engine->IsInitialCompileFinished());
		Root->SetBoolField(TEXT("initialCompileSucceeded"), Engine != nullptr && Engine->bDidInitialCompileSucceed);
		Root->SetNumberField(TEXT("activeModuleCount"), Modules.Num());
		Root->SetNumberField(TEXT("totalClassCount"), TotalClassCount);
		Root->SetNumberField(TEXT("totalEnumCount"), TotalEnumCount);
		Root->SetNumberField(TEXT("totalDelegateCount"), TotalDelegateCount);
		Root->SetArrayField(TEXT("modules"), MoveTemp(Modules));
		Root->SetArrayField(TEXT("debugStack"), BuildDebugStackArray());

		FString JsonString;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString, 0);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			Result.ErrorMessage = TEXT("Failed to serialize Angelscript crash snapshot JSON.");
			return Result;
		}

		Result.SnapshotPath = FPaths::Combine(ResolvedOutputDir, TEXT("AngelscriptCrashSnapshot.json"));
		if (!FFileHelper::SaveStringToFile(JsonString, *Result.SnapshotPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Result.ErrorMessage = FString::Printf(TEXT("Failed to write Angelscript crash snapshot '%s'."), *Result.SnapshotPath);
			return Result;
		}

		Result.bSuccess = true;

		if (bAttachToCrashContext)
		{
			FGenericCrashContext::SetGameData(TEXT("Angelscript.CrashSnapshotPath"), Result.SnapshotPath);
			FGenericCrashContext::AddFile(Result.SnapshotPath);
		}

		return Result;
	}

	void ExecuteConfigureCrashSnapshot(const TArray<FString>& Args)
	{
		const FString OutputDir = Args.Num() >= 1 ? SanitizeCommandArg(Args[0]) : FString();
		const FString Marker = Args.Num() >= 2 ? SanitizeCommandArg(Args[1]) : FString();
		FAngelscriptCrashSnapshot::ConfigureForTesting(OutputDir, Marker);
		UE_LOG(LogAngelscriptCrashSnapshot, Log, TEXT("Configured Angelscript crash snapshot output dir '%s' marker '%s'."), *OutputDir, *Marker);
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAutoConsoleCommand GConfigureCrashSnapshotCommand(
		TEXT("as.Test.ConfigureCrashSnapshot"),
		TEXT("Configure Angelscript crash snapshot output before an isolated crash test."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ExecuteConfigureCrashSnapshot));
#endif
}

void FAngelscriptCrashSnapshot::Startup()
{
	using namespace AngelscriptCrashSnapshot_Private;
	if (!GSystemErrorHandle.IsValid())
	{
		GSystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddStatic(&FAngelscriptCrashSnapshot::HandleSystemError);
	}
}

void FAngelscriptCrashSnapshot::Shutdown()
{
	using namespace AngelscriptCrashSnapshot_Private;
	if (GSystemErrorHandle.IsValid())
	{
		FCoreDelegates::OnHandleSystemError.Remove(GSystemErrorHandle);
		GSystemErrorHandle.Reset();
	}
}

FAngelscriptCrashSnapshot::FWriteResult FAngelscriptCrashSnapshot::WriteSnapshotForTesting(const FString& OutputDir, const FString& Marker)
{
	return AngelscriptCrashSnapshot_Private::WriteSnapshot(OutputDir, Marker, false);
}

void FAngelscriptCrashSnapshot::ConfigureForTesting(const FString& OutputDir, const FString& Marker)
{
	AngelscriptCrashSnapshot_Private::GOverrideOutputDir = OutputDir;
	AngelscriptCrashSnapshot_Private::GMarker = Marker;
}

void FAngelscriptCrashSnapshot::HandleSystemError()
{
	using namespace AngelscriptCrashSnapshot_Private;
	bool bExpected = false;
	if (!GHandlingCrash.compare_exchange_strong(bExpected, true))
	{
		return;
	}

	const FWriteResult Result = WriteSnapshot(GOverrideOutputDir, GMarker, true);
	if (Result.bSuccess)
	{
		UE_LOG(LogAngelscriptCrashSnapshot, Log, TEXT("Angelscript crash snapshot written to '%s'."), *Result.SnapshotPath);
	}
	else
	{
		UE_LOG(LogAngelscriptCrashSnapshot, Warning, TEXT("Angelscript crash snapshot failed: %s"), *Result.ErrorMessage);
	}
}
