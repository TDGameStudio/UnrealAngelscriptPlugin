#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheSettings.h"

#include "AngelscriptEngine.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AngelscriptCacheConsoleCommands_Private
{
	static bool ResolveStatusJsonPath(
		FString RequestedPath,
		FString& OutPath)
	{
		RequestedPath.TrimStartAndEndInline();
		if (RequestedPath.Len() >= 2
			&& RequestedPath.StartsWith(TEXT("\""))
			&& RequestedPath.EndsWith(TEXT("\"")))
		{
			RequestedPath = RequestedPath.Mid(1, RequestedPath.Len() - 2);
		}
		if (RequestedPath.IsEmpty())
		{
			return false;
		}

		const FString SavedRoot =
			FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		if (FPaths::IsRelative(RequestedPath))
		{
			OutPath = FPaths::Combine(
				SavedRoot, TEXT("Angelscript"), TEXT("Diagnostics"),
				RequestedPath);
		}
		else
		{
			OutPath = FPaths::ConvertRelativePathToFull(RequestedPath);
		}
		FPaths::NormalizeFilename(OutPath);
		return FPaths::IsUnderDirectory(OutPath, SavedRoot);
	}

	static void EmitDiagnosticJson(
		const TCHAR* CommandName,
		const FString& OutputPath,
		FOutputDevice& OutputDevice)
	{
		const FAngelscriptCacheDiagnosticJsonResult Result =
			CaptureCurrentAngelscriptCacheDiagnosticJson();
		if (!Result.IsSuccess())
		{
			OutputDevice.Logf(
				TEXT("%s failed: Error=%u Detail=%s"),
				CommandName,
				static_cast<uint32>(Result.Error),
				*Result.Detail);
			return;
		}

		if (OutputPath.IsEmpty())
		{
			OutputDevice.Log(*Result.Json);
			return;
		}

		const FString OutputDirectory = FPaths::GetPath(OutputPath);
		if (!IFileManager::Get().MakeDirectory(
			*OutputDirectory, true)
			|| !FFileHelper::SaveStringToFile(
				Result.Json,
				*OutputPath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutputDevice.Logf(
				TEXT("%s failed to write session JSON."), CommandName);
			return;
		}
		OutputDevice.Logf(
			TEXT("%s wrote Cache V2 session JSON to %s"),
			CommandName, *OutputPath);
	}

	static FAngelscriptCacheService* ResolveCurrentService()
	{
		FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
		if (Engine == nullptr && FAngelscriptEngine::IsInitialized())
		{
			Engine = &FAngelscriptEngine::Get();
		}
		return Engine == nullptr ? nullptr : Engine->GetCacheService();
	}

	static FAngelscriptEngine* ResolveCurrentEngine()
	{
		FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
		if (Engine == nullptr && FAngelscriptEngine::IsInitialized())
		{
			Engine = &FAngelscriptEngine::Get();
		}
		return Engine;
	}

	static bool TryParseHash256(
		const FString& Text,
		FAngelscriptHash256& OutHash)
	{
		if (Text.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Text)
		{
			if (!FChar::IsHexDigit(Character))
			{
				return false;
			}
		}
		OutHash.Value = FBlake3Hash(FWideStringView(Text));
		return true;
	}

	static void ExecuteStatus(
		const TArray<FString>& Args,
		FOutputDevice& OutputDevice)
	{
		FString OutputPath;
		if (Args.Num() > 1
			|| (Args.Num() == 1
				&& (!Args[0].StartsWith(TEXT("Json="),
					ESearchCase::IgnoreCase)
					|| !ResolveStatusJsonPath(
						Args[0].Mid(5), OutputPath))))
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Status failed: expected no arguments or Json=<path-under-ProjectSaved>."));
			return;
		}

		EmitDiagnosticJson(TEXT("as.Cache.Status"), OutputPath, OutputDevice);
	}

	static FAutoConsoleCommand GAngelscriptCacheStatusCommand(
		TEXT("as.Cache.Status"),
		TEXT("Print the current Engine's pointer-free Cache V2 status as deterministic JSON. Optional: Json=<path-under-ProjectSaved>."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(
			&ExecuteStatus));

	static void ExecuteFlush(
		const TArray<FString>& Args,
		FOutputDevice& OutputDevice)
	{
		double TimeoutSeconds = 0.0;
		if (Args.Num() > 1
			|| (Args.Num() == 1
				&& (!Args[0].StartsWith(
					TEXT("Timeout="), ESearchCase::IgnoreCase)
					|| !LexTryParseString(
						TimeoutSeconds, *Args[0].Mid(8))
					|| !FMath::IsFinite(TimeoutSeconds)
					|| TimeoutSeconds <= 0.0)))
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Flush failed: expected no arguments or Timeout=<positive-seconds>."));
			return;
		}

		const FAngelscriptCacheFlushApiResult Result =
			FlushCurrentAngelscriptCacheToStore(TimeoutSeconds);
		if (!Result.IsSuccess())
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Flush failed: ApiError=%u LifecycleError=%u CurrentPrepare=%u CurrentStore=%u PendingPrepare=%u PendingStore=%u Detail=%s"),
				static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Flush.Error),
				static_cast<uint32>(Result.Flush.Current.Preparation.Error),
				static_cast<uint32>(Result.Flush.Current.Publication.Error),
				static_cast<uint32>(
					Result.Flush.PendingColdStart.Preparation.Error),
				static_cast<uint32>(
					Result.Flush.PendingColdStart.Publication.Error),
				*Result.Detail);
			return;
		}

		OutputDevice.Logf(
			TEXT("as.Cache.Flush succeeded: CurrentAttempted=%d CurrentCommit=%u CurrentGeneration=%s PendingAttempted=%d PendingCommit=%u PendingGeneration=%s Detail=%s"),
			Result.Flush.Current.bAttempted ? 1 : 0,
			static_cast<uint32>(
				Result.Flush.Current.Publication.CommitState),
			*Result.Flush.Current.GenerationId.ToHexString(),
			Result.Flush.PendingColdStart.bAttempted ? 1 : 0,
			static_cast<uint32>(
				Result.Flush.PendingColdStart.Publication.CommitState),
			*Result.Flush.PendingColdStart.GenerationId.ToHexString(),
			*Result.Detail);
	}

	static FAutoConsoleCommand GAngelscriptCacheFlushCommand(
		TEXT("as.Cache.Flush"),
		TEXT("Commit already-frozen Cache V2 Current/PendingColdStart publications. Optional: Timeout=<positive-seconds>."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(
			&ExecuteFlush));

	static void ExecuteVerify(
		const TArray<FString>& Args,
		FOutputDevice& OutputDevice)
	{
		EAngelscriptCacheDiagnosticGeneration Generation =
			EAngelscriptCacheDiagnosticGeneration::Current;
		bool bDeep = false;
		bool bSawGeneration = false;
		bool bSawDeep = false;
		for (const FString& Arg : Args)
		{
			if (Arg.StartsWith(TEXT("Generation="), ESearchCase::IgnoreCase)
				&& !bSawGeneration)
			{
				bSawGeneration = true;
				const FString Value = Arg.Mid(11);
				if (Value.Equals(TEXT("Current"), ESearchCase::IgnoreCase))
				{
					Generation =
						EAngelscriptCacheDiagnosticGeneration::Current;
				}
				else if (Value.Equals(
					TEXT("Previous"), ESearchCase::IgnoreCase))
				{
					Generation =
						EAngelscriptCacheDiagnosticGeneration::Previous;
				}
				else if (Value.Equals(TEXT("Pending"),
					ESearchCase::IgnoreCase)
					|| Value.Equals(TEXT("PendingColdStart"),
						ESearchCase::IgnoreCase))
				{
					Generation = EAngelscriptCacheDiagnosticGeneration::
						PendingColdStart;
				}
				else
				{
					OutputDevice.Logf(
						TEXT("as.Cache.Verify failed: Generation must be Current, Previous, or Pending."));
					return;
				}
			}
			else if (Arg.StartsWith(TEXT("Deep="), ESearchCase::IgnoreCase)
				&& !bSawDeep)
			{
				bSawDeep = true;
				int32 DeepValue = 0;
				if (!LexTryParseString(DeepValue, *Arg.Mid(5))
					|| (DeepValue != 0 && DeepValue != 1))
				{
					OutputDevice.Logf(
						TEXT("as.Cache.Verify failed: Deep must be 0 or 1."));
					return;
				}
				bDeep = DeepValue != 0;
			}
			else
			{
				OutputDevice.Logf(
					TEXT("as.Cache.Verify failed: expected optional Generation=Current|Previous|Pending and Deep=0|1."));
				return;
			}
		}

		const FAngelscriptCacheVerifyApiResult Result =
			VerifyAngelscriptCacheStore(
				ResolveCurrentEngine(), Generation, bDeep);
		if (!Result.IsSuccess())
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Verify failed: ApiError=%u StoreError=%u Stage=%u PathCategory=%u Generation=%s Deep=%d Detail=%s"),
				static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Store.Error),
				static_cast<uint32>(Result.Store.Stage),
				static_cast<uint32>(Result.Store.PathCategory),
				Result.GenerationId.IsSet()
					? *Result.GenerationId->ToHexString() : TEXT("<none>"),
				Result.bDeep ? 1 : 0,
				*Result.Detail);
			return;
		}
		OutputDevice.Logf(
			TEXT("as.Cache.Verify succeeded: Generation=%s Deep=%d Modules=%d ManifestRecords=%d ReachableRecords=%d Packs=%d StoredBytes=%llu DecompressedBytes=%llu DecodedBytes=%llu Detail=%s"),
			*Result.GenerationId->ToHexString(),
			Result.bDeep ? 1 : 0,
			Result.ManifestModuleCount,
			Result.ManifestRecordCount,
			Result.ReachableRecordCount,
			Result.ReferencedPackCount,
			Result.StoredBytesRead,
			Result.DecompressedBytesRead,
			Result.DecodedBytesRetained,
			*Result.Detail);
	}

	static FAutoConsoleCommand GAngelscriptCacheVerifyCommand(
		TEXT("as.Cache.Verify"),
		TEXT("Verify one persisted Cache V2 slot: Generation=Current|Previous|Pending Deep=0|1."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(
			&ExecuteVerify));

	static void ExecuteCompact(
		const TArray<FString>& Args,
		FOutputDevice& OutputDevice)
	{
		double TimeoutSeconds = 0.0;
		if (Args.Num() > 1
			|| (Args.Num() == 1
				&& (!Args[0].StartsWith(
					TEXT("Timeout="), ESearchCase::IgnoreCase)
					|| !LexTryParseString(
						TimeoutSeconds, *Args[0].Mid(8))
					|| !FMath::IsFinite(TimeoutSeconds)
					|| TimeoutSeconds <= 0.0)))
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Compact failed: expected no arguments or Timeout=<positive-seconds>."));
			return;
		}

		const FAngelscriptCacheCompactApiResult Result =
			CompactAngelscriptCacheStoreForEngine(
				ResolveCurrentEngine(), TimeoutSeconds);
		if (!Result.IsSuccess())
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Compact failed: ApiError=%u StoreError=%u Stage=%u Commit=%u Before=%s After=%s Detail=%s"),
				static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Store.Error),
				static_cast<uint32>(Result.Store.Stage),
				static_cast<uint32>(Result.Store.CommitState),
				Result.Store.GenerationBefore.IsSet()
					? *Result.Store.GenerationBefore->ToHexString()
					: TEXT("<none>"),
				Result.Store.GenerationAfter.IsSet()
					? *Result.Store.GenerationAfter->ToHexString()
					: TEXT("<none>"),
				*Result.Detail);
			return;
		}
		OutputDevice.Logf(
			TEXT("as.Cache.Compact succeeded: Commit=%u Before=%s After=%s Detail=%s"),
			static_cast<uint32>(Result.Store.CommitState),
			Result.Store.GenerationBefore.IsSet()
				? *Result.Store.GenerationBefore->ToHexString()
				: TEXT("<none>"),
			Result.Store.GenerationAfter.IsSet()
				? *Result.Store.GenerationAfter->ToHexString()
				: TEXT("<none>"),
			*Result.Detail);
	}

	static FAutoConsoleCommand GAngelscriptCacheCompactCommand(
		TEXT("as.Cache.Compact"),
		TEXT("Run bounded two-phase Cache V2 compaction using immutable Current publication authority. Optional: Timeout=<positive-seconds>."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(
			&ExecuteCompact));

	static void ExecuteForceClean(
		const TArray<FString>& Args,
		FOutputDevice& OutputDevice)
	{
		FString ModuleSelector;
		if (Args.Num() > 1
			|| (Args.Num() == 1
				&& (!Args[0].StartsWith(
					TEXT("Module="), ESearchCase::IgnoreCase)
					|| (ModuleSelector = Args[0].Mid(7)).IsEmpty())))
		{
			OutputDevice.Logf(
				TEXT("as.Cache.ForceClean failed: expected no arguments or Module=<canonical-name|64-hex-stable-key>."));
			return;
		}

		const FAngelscriptCacheForceCleanApiResult Result =
			ForceCleanAngelscriptCache(
				ResolveCurrentEngine(), MoveTemp(ModuleSelector));
		if (!Result.IsSuccess())
		{
			OutputDevice.Logf(
				TEXT("as.Cache.ForceClean failed: ApiError=%u Outcome=%u Modules=%s Detail=%s"),
				static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Outcome),
				*FString::Join(Result.SelectedModuleNames, TEXT(",")),
				*Result.Detail);
			return;
		}
		OutputDevice.Logf(
			TEXT("as.Cache.ForceClean succeeded: Outcome=%u Modules=%s Detail=%s"),
			static_cast<uint32>(Result.Outcome),
			*FString::Join(Result.SelectedModuleNames, TEXT(",")),
			*Result.Detail);
	}

	static FAutoConsoleCommand GAngelscriptCacheForceCleanCommand(
		TEXT("as.Cache.ForceClean"),
		TEXT("Recompile all active modules or one Module=<name|stable-key> through the authoritative forced-clean transaction."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(
			&ExecuteForceClean));

	static void ExecuteRuntimeReload(
		const TArray<FString>& Args,
		FOutputDevice& OutputDevice)
	{
		if (!Args.IsEmpty())
		{
			OutputDevice.Logf(
				TEXT("as.ReloadScripts failed: no arguments are accepted."));
			return;
		}
		FAngelscriptEngine* Engine = ResolveCurrentEngine();
		if (Engine == nullptr)
		{
			OutputDevice.Logf(
				TEXT("as.ReloadScripts failed: AngelScript Engine is unavailable."));
			return;
		}
		const EAngelscriptRuntimeReloadRequestStatus Status =
			Engine->RequestPackagedRuntimeReload();
		OutputDevice.Logf(
			TEXT("as.ReloadScripts request status=%u."),
			static_cast<uint32>(Status));
	}

	static FAutoConsoleCommand GAngelscriptRuntimeReloadCommand(
		TEXT("as.ReloadScripts"),
		TEXT("Queue one packaged Runtime loose-source reload at the next game-thread safe point."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(
			&ExecuteRuntimeReload));

	static void ExecuteTrace(
		const TArray<FString>& Args,
		FOutputDevice& OutputDevice)
	{
		if (Args.IsEmpty())
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Trace failed: expected Enable [Capacity=1..65536], Disable, Clear, or Dump [Json=<path-under-ProjectSaved>]."));
			return;
		}

		if (Args[0].Equals(TEXT("Dump"), ESearchCase::IgnoreCase))
		{
			FString OutputPath;
			if (Args.Num() > 2
				|| (Args.Num() == 2
					&& (!Args[1].StartsWith(
						TEXT("Json="), ESearchCase::IgnoreCase)
						|| !ResolveStatusJsonPath(
							Args[1].Mid(5), OutputPath))))
			{
				OutputDevice.Logf(
					TEXT("as.Cache.Trace failed: Dump accepts only optional Json=<path-under-ProjectSaved>."));
				return;
			}
			EmitDiagnosticJson(TEXT("as.Cache.Trace"), OutputPath, OutputDevice);
			return;
		}

		FAngelscriptCacheService* Service = ResolveCurrentService();
		if (Service == nullptr)
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Trace failed: Cache V2 Service is unavailable."));
			return;
		}

		if (Args[0].Equals(TEXT("Enable"), ESearchCase::IgnoreCase))
		{
			const UAngelscriptCacheSettings* Settings =
				GetDefault<UAngelscriptCacheSettings>();
			uint32 Capacity = Settings == nullptr
				? 1024
				: Settings->DecisionTraceCapacity;
			if (Args.Num() > 2
				|| (Args.Num() == 2
					&& (!Args[1].StartsWith(
						TEXT("Capacity="), ESearchCase::IgnoreCase)
						|| !LexTryParseString(Capacity, *Args[1].Mid(9))
						|| Capacity < 1
						|| Capacity > 65536)))
			{
				OutputDevice.Logf(
					TEXT("as.Cache.Trace failed: Enable accepts only optional Capacity=1..65536."));
				return;
			}
			Service->ConfigureDecisionTrace(true, Capacity);
			OutputDevice.Logf(
				TEXT("as.Cache.Trace enabled with Capacity=%u."), Capacity);
			return;
		}

		if (Args.Num() != 1)
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Trace failed: Disable and Clear accept no additional arguments."));
			return;
		}
		if (Args[0].Equals(TEXT("Disable"), ESearchCase::IgnoreCase))
		{
			const FAngelscriptCacheDecisionTraceSnapshot Snapshot =
				Service->CaptureDecisionTrace();
			Service->ConfigureDecisionTrace(false, Snapshot.Capacity);
			OutputDevice.Logf(TEXT("as.Cache.Trace disabled."));
			return;
		}
		if (Args[0].Equals(TEXT("Clear"), ESearchCase::IgnoreCase))
		{
			Service->ClearDecisionTrace();
			OutputDevice.Logf(TEXT("as.Cache.Trace cleared."));
			return;
		}

		OutputDevice.Logf(
			TEXT("as.Cache.Trace failed: unknown action %s."), *Args[0]);
	}

	static FAutoConsoleCommand GAngelscriptCacheTraceCommand(
		TEXT("as.Cache.Trace"),
		TEXT("Control bounded pointer-free Cache V2 decision tracing: Enable [Capacity=N], Disable, Clear, Dump [Json=path]."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(
			&ExecuteTrace));

	static void ExecuteExplain(
		const TArray<FString>& Args,
		FOutputDevice& OutputDevice)
	{
		FAngelscriptCacheExplainRequest Request;
		for (const FString& Arg : Args)
		{
			uint64 Ordinal = 0;
			uint8 Stage = 0;
			FAngelscriptHash256 Hash;
			if (Arg.StartsWith(TEXT("Event="), ESearchCase::IgnoreCase)
				&& LexTryParseString(Ordinal, *Arg.Mid(6)))
			{
				Request.EventOrdinal = Ordinal;
			}
			else if (Arg.StartsWith(
				TEXT("Transaction="), ESearchCase::IgnoreCase)
				&& LexTryParseString(Ordinal, *Arg.Mid(12)))
			{
				Request.TransactionOrdinal = Ordinal;
			}
			else if (Arg.StartsWith(TEXT("Stage="), ESearchCase::IgnoreCase)
				&& LexTryParseString(Stage, *Arg.Mid(6))
				&& Stage >= static_cast<uint8>(
					EAngelscriptCacheDecisionStage::StartupSelection)
				&& Stage <= static_cast<uint8>(
					EAngelscriptCacheDecisionStage::RuntimeReload))
			{
				Request.Stage = static_cast<EAngelscriptCacheDecisionStage>(
					Stage);
			}
			else if (Arg.StartsWith(TEXT("Module="), ESearchCase::IgnoreCase)
				&& TryParseHash256(Arg.Mid(7), Hash))
			{
				Request.ModuleKey = FAngelscriptStableModuleKey{Hash};
			}
			else if (Arg.StartsWith(
				TEXT("Function="), ESearchCase::IgnoreCase)
				&& TryParseHash256(Arg.Mid(9), Hash))
			{
				Request.FunctionKey = FAngelscriptStableFunctionKey{Hash};
			}
			else if (Arg.StartsWith(TEXT("Record="), ESearchCase::IgnoreCase))
			{
				FString KindText;
				FString HashText;
				uint8 Kind = 0;
				if (!Arg.Mid(7).Split(TEXT(":"), &KindText, &HashText)
					|| !LexTryParseString(Kind, *KindText)
					|| Kind < static_cast<uint8>(
						EAngelscriptCacheRecordKind::SourceIndex)
					|| Kind > static_cast<uint8>(
						EAngelscriptCacheRecordKind::ModuleSnapshot)
					|| !TryParseHash256(HashText, Hash))
				{
					OutputDevice.Logf(
						TEXT("as.Cache.Explain failed: Record must be <kind-number>:<64-hex-content-hash>."));
					return;
				}
				Request.RecordId = FAngelscriptCacheRecordId{
					static_cast<EAngelscriptCacheRecordKind>(Kind), Hash};
			}
			else
			{
				OutputDevice.Logf(
					TEXT("as.Cache.Explain failed: selectors are Event=N, Transaction=N, Stage=N, Module=<64-hex-key>, Function=<64-hex-key>, Record=<kind>:<64-hex-hash>."));
				return;
			}
		}

		const FAngelscriptCacheExplainResult Result =
			ExplainAngelscriptCacheDecisions(ResolveCurrentEngine(), Request);
		if (!Result.IsSuccess())
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Explain failed: Error=%u Detail=%s"),
				static_cast<uint32>(Result.Error), *Result.Detail);
			return;
		}
		FString Json;
		if (!SerializeAngelscriptCacheExplainResultJson(Result, Json))
		{
			OutputDevice.Logf(
				TEXT("as.Cache.Explain failed: JSON serialization failed."));
			return;
		}
		OutputDevice.Log(*Json);
	}

	static FAutoConsoleCommand GAngelscriptCacheExplainCommand(
		TEXT("as.Cache.Explain"),
		TEXT("Explain already-captured Cache V2 decisions using one or more typed selectors: Event=N Transaction=N Stage=N Module=<key> Function=<key> Record=<kind>:<hash>."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(
			&ExecuteExplain));
}
