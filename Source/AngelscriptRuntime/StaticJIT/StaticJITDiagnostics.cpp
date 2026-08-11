#include "StaticJIT/StaticJITDiagnostics.h"

#if AS_WITH_STATIC_JIT_DIAGNOSTICS

#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/PrecompiledData.h"
#include "StaticJIT/StaticJITHeader.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

DEFINE_LOG_CATEGORY_STATIC(LogStaticJITDiagnostics, Log, All);

namespace
{
	TMap<uint32, int32>& GetStaticJITEntryCounters()
	{
		static TMap<uint32, int32> Counters;
		return Counters;
	}

	void SetError(FString* OutError, const FString& Message)
	{
		if (OutError != nullptr)
		{
			*OutError = Message;
		}
	}

	FString SanitizeFunctionArgument(FString FunctionArgument)
	{
		FunctionArgument.TrimStartAndEndInline();
		while (FunctionArgument.EndsWith(TEXT(";")))
		{
			FunctionArgument.LeftChopInline(1, EAllowShrinking::No);
			FunctionArgument.TrimEndInline();
		}

		if (FunctionArgument.Len() >= 2 && FunctionArgument.StartsWith(TEXT("\"")) && FunctionArgument.EndsWith(TEXT("\"")))
		{
			FunctionArgument = FunctionArgument.Mid(1, FunctionArgument.Len() - 2);
		}

		return FunctionArgument;
	}

	bool MatchesFunctionArgument(asIScriptFunction* Function, const FString& FunctionArgument)
	{
		if (Function == nullptr)
		{
			return false;
		}

		const FString FunctionName = ANSI_TO_TCHAR(Function->GetName());
		if (FunctionName == FunctionArgument)
		{
			return true;
		}

		const FString DeclarationNoObject = ANSI_TO_TCHAR(Function->GetDeclaration(false, true, false));
		if (DeclarationNoObject == FunctionArgument)
		{
			return true;
		}

		const FString DeclarationWithObject = ANSI_TO_TCHAR(Function->GetDeclaration(true, true, false));
		return DeclarationWithObject == FunctionArgument;
	}

	asCScriptFunction* FindFunctionInModule(asIScriptModule& Module, const FString& FunctionArgument)
	{
		FTCHARToUTF8 DeclarationUtf8(*FunctionArgument);
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get()))
		{
			return static_cast<asCScriptFunction*>(Function);
		}

		const asUINT FunctionCount = Module.GetFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* Function = Module.GetFunctionByIndex(FunctionIndex);
			if (MatchesFunctionArgument(Function, FunctionArgument))
			{
				return static_cast<asCScriptFunction*>(Function);
			}
		}

		const asUINT TypeCount = Module.GetObjectTypeCount();
		for (asUINT TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			asITypeInfo* TypeInfo = Module.GetObjectTypeByIndex(TypeIndex);
			if (TypeInfo == nullptr)
			{
				continue;
			}

			if (asIScriptFunction* Function = TypeInfo->GetMethodByDecl(DeclarationUtf8.Get()))
			{
				return static_cast<asCScriptFunction*>(Function);
			}

			const asUINT MethodCount = TypeInfo->GetMethodCount();
			for (asUINT MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
			{
				asIScriptFunction* Function = TypeInfo->GetMethodByIndex(MethodIndex);
				if (MatchesFunctionArgument(Function, FunctionArgument))
				{
					return static_cast<asCScriptFunction*>(Function);
				}
			}
		}

		return nullptr;
	}

	void LogSnapshot(const FStaticJITDiagnostics::FSnapshot& Snapshot)
	{
		UE_LOG(
			LogStaticJITDiagnostics,
			Log,
			TEXT("StaticJIT diagnostics: RegisteredFunctions=%d EntryCounters=%d CompiledInfo=%s CurrentEngine=%s ScriptEngine=%s PrecompiledData=%s CompiledInfoMatchesPrecompiledData=%s"),
			Snapshot.RegisteredFunctionCount,
			Snapshot.EntryCounterCount,
			Snapshot.bHasCompiledInfo ? TEXT("true") : TEXT("false"),
			Snapshot.bHasCurrentEngine ? TEXT("true") : TEXT("false"),
			Snapshot.bHasScriptEngine ? TEXT("true") : TEXT("false"),
			Snapshot.bHasPrecompiledData ? TEXT("true") : TEXT("false"),
			Snapshot.bCompiledInfoMatchesPrecompiledData ? TEXT("true") : TEXT("false"));

		if (Snapshot.bHasPrecompiledData)
		{
			UE_LOG(LogStaticJITDiagnostics, Log, TEXT("StaticJIT diagnostics: PrecompiledDataGuid=%s"), *Snapshot.PrecompiledDataGuid.ToString(EGuidFormats::DigitsWithHyphens));
		}

		if (Snapshot.bHasCompiledInfo)
		{
			UE_LOG(LogStaticJITDiagnostics, Log, TEXT("StaticJIT diagnostics: CompiledInfoGuid=%s"), *Snapshot.CompiledInfoGuid.ToString(EGuidFormats::DigitsWithHyphens));
		}
	}

	FAutoConsoleCommand GStaticJITDumpDiagnosticsCommand(
		TEXT("as.StaticJIT.DumpDiagnostics"),
		TEXT("Dump StaticJIT process and optional function diagnostics. Optional: as.StaticJIT.DumpDiagnostics [FunctionNameOrDeclaration]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FStaticJITDiagnostics::DumpDiagnostics));
}

bool FStaticJITDiagnostics::LoadPrecompiledData(FAngelscriptEngine& Engine, const FString& Filename, FString* OutError)
{
	if (Engine.GetScriptEngine() == nullptr)
	{
		SetError(OutError, TEXT("StaticJIT diagnostics failed to load precompiled data: script engine was null."));
		return false;
	}

	if (!IFileManager::Get().FileExists(*Filename))
	{
		SetError(OutError, FString::Printf(TEXT("StaticJIT diagnostics failed to load precompiled data: cache file '%s' does not exist."), *Filename));
		return false;
	}

	if (Engine.PrecompiledData != nullptr)
	{
		delete Engine.PrecompiledData;
		Engine.PrecompiledData = nullptr;
	}

	Engine.PrecompiledData = new FAngelscriptPrecompiledData(Engine.GetScriptEngine());
	Engine.PrecompiledData->Load(Filename);
	if (!Engine.PrecompiledData->IsValidForCurrentBuild())
	{
		delete Engine.PrecompiledData;
		Engine.PrecompiledData = nullptr;
		SetError(OutError, TEXT("StaticJIT diagnostics failed to load precompiled data: cache build identifier does not match current build."));
		return false;
	}

	if (Engine.StaticJIT != nullptr)
	{
		Engine.StaticJIT->PrecompiledData = Engine.PrecompiledData;
	}

	return true;
}

bool FStaticJITDiagnostics::CompileLoadedPrecompiledData(FAngelscriptEngine& Engine, ECompileType CompileType, FString* OutError)
{
	if (Engine.PrecompiledData == nullptr)
	{
		SetError(OutError, TEXT("StaticJIT diagnostics failed to compile precompiled data: no precompiled data is loaded."));
		return false;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Engine.PrecompiledData->GetModulesToCompile();
	if (ModulesToCompile.Num() == 0)
	{
		SetError(OutError, TEXT("StaticJIT diagnostics failed to compile precompiled data: loaded cache produced no modules."));
		return false;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
	TGuardValue<bool> UseCompatibilityDataGuard(
		Engine.bUseStaticJITCompatibilityData, true);
	TGuardValue<bool> ScriptDevelopmentModeGuard(Engine.bScriptDevelopmentMode, false);
	const ECompileResult CompileResult = Engine.CompileModules(CompileType, ModulesToCompile, CompiledModules);
	if (CompileResult != ECompileResult::FullyHandled && CompileResult != ECompileResult::PartiallyHandled)
	{
		SetError(OutError, TEXT("StaticJIT diagnostics failed to compile precompiled data: module compilation returned an error."));
		return false;
	}

	return true;
}

bool FStaticJITDiagnostics::ResolveFunctionId(const FAngelscriptEngine& Engine, asIScriptFunction* Function, uint32& OutId)
{
	OutId = 0;
	if (Engine.PrecompiledData == nullptr || Function == nullptr)
	{
		return false;
	}

	return Engine.PrecompiledData->GetIdForFunction(Function, OutId);
}

bool FStaticJITDiagnostics::ResolveFunctionId(const FAngelscriptEngine& Engine, asCScriptFunction* Function, uint32& OutId)
{
	return ResolveFunctionId(Engine, static_cast<asIScriptFunction*>(Function), OutId);
}

bool FStaticJITDiagnostics::IsFunctionRegistered(uint32 FunctionId)
{
	return FJITDatabase::Get().Functions.Contains(FunctionId);
}

bool FStaticJITDiagnostics::HasJitFunction(asCScriptFunction* Function)
{
	return Function != nullptr && Function->jitFunction != nullptr;
}

int32 FStaticJITDiagnostics::GetEntryCount(uint32 FunctionId)
{
	return GetStaticJITEntryCounters().FindRef(FunctionId);
}

void FStaticJITDiagnostics::ResetEntryCounters()
{
	GetStaticJITEntryCounters().Reset();
}

void FStaticJITDiagnostics::MarkEntry(uint32 FunctionId)
{
	int32& Counter = GetStaticJITEntryCounters().FindOrAdd(FunctionId);
	++Counter;
}

int32 FStaticJITDiagnostics::GetEntryCounterCount()
{
	return GetStaticJITEntryCounters().Num();
}

FStaticJITDiagnostics::FSnapshot FStaticJITDiagnostics::CaptureSnapshot(const FAngelscriptEngine* Engine)
{
	const FAngelscriptEngine* CurrentEngine = Engine != nullptr ? Engine : FAngelscriptEngine::TryGetCurrentEngine();
	const FJITDatabase& JITDatabase = FJITDatabase::Get();
	const FStaticJITCompiledInfo* CompiledInfo = FStaticJITCompiledInfo::Get();

	FSnapshot Snapshot;
	Snapshot.bHasCurrentEngine = CurrentEngine != nullptr;
	Snapshot.bHasScriptEngine = CurrentEngine != nullptr && CurrentEngine->GetScriptEngine() != nullptr;
	Snapshot.bHasPrecompiledData = CurrentEngine != nullptr && CurrentEngine->PrecompiledData != nullptr;
	Snapshot.bHasCompiledInfo = CompiledInfo != nullptr;
	Snapshot.RegisteredFunctionCount = JITDatabase.Functions.Num();
	Snapshot.EntryCounterCount = GetEntryCounterCount();
	Snapshot.FunctionLookupCount = JITDatabase.FunctionLookups.Num();
	Snapshot.SystemFunctionPointerLookupCount = JITDatabase.SystemFunctionPointerLookups.Num();
	Snapshot.GlobalVarLookupCount = JITDatabase.GlobalVarLookups.Num();
	Snapshot.TypeInfoLookupCount = JITDatabase.TypeInfoLookups.Num();
	Snapshot.PropertyOffsetLookupCount = JITDatabase.PropertyOffsetLookups.Num();

	if (Snapshot.bHasPrecompiledData)
	{
		Snapshot.PrecompiledDataGuid = CurrentEngine->PrecompiledData->DataGuid;
	}

	if (CompiledInfo != nullptr)
	{
		Snapshot.CompiledInfoGuid = CompiledInfo->PrecompiledDataGuid;
		Snapshot.bCompiledInfoMatchesPrecompiledData = Snapshot.bHasPrecompiledData && Snapshot.PrecompiledDataGuid == Snapshot.CompiledInfoGuid;
	}

	return Snapshot;
}

asCScriptFunction* FStaticJITDiagnostics::ResolveFunctionArgument(FAngelscriptEngine& Engine, const FString& FunctionArgument, FString& OutError)
{
	const FString SanitizedArgument = SanitizeFunctionArgument(FunctionArgument);
	if (SanitizedArgument.IsEmpty())
	{
		OutError = TEXT("No function argument was provided.");
		return nullptr;
	}

	for (const TSharedRef<FAngelscriptModuleDesc>& ModuleDesc : Engine.GetActiveModules())
	{
		if (ModuleDesc->ScriptModule == nullptr)
		{
			continue;
		}

		if (asCScriptFunction* Function = FindFunctionInModule(*ModuleDesc->ScriptModule, SanitizedArgument))
		{
			return Function;
		}
	}

	OutError = FString::Printf(TEXT("Could not resolve StaticJIT diagnostics function argument '%s'."), *SanitizedArgument);
	return nullptr;
}

void FStaticJITDiagnostics::DumpDiagnostics(const TArray<FString>& Args)
{
	FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
	const FSnapshot Snapshot = CaptureSnapshot(CurrentEngine);
	LogSnapshot(Snapshot);

	if (Args.IsEmpty())
	{
		return;
	}

	if (CurrentEngine == nullptr)
	{
		UE_LOG(LogStaticJITDiagnostics, Warning, TEXT("StaticJIT diagnostics cannot resolve function '%s': no current Angelscript engine is available."), *FString::Join(Args, TEXT(" ")));
		return;
	}

	const FString FunctionArgument = FString::Join(Args, TEXT(" "));
	FString ResolveError;
	asCScriptFunction* Function = ResolveFunctionArgument(*CurrentEngine, FunctionArgument, ResolveError);
	if (Function == nullptr)
	{
		UE_LOG(LogStaticJITDiagnostics, Warning, TEXT("%s"), *ResolveError);
		return;
	}

	uint32 FunctionId = 0;
	if (!ResolveFunctionId(*CurrentEngine, Function, FunctionId))
	{
		UE_LOG(
			LogStaticJITDiagnostics,
			Warning,
			TEXT("StaticJIT diagnostics resolved '%s' to '%s' but no StaticJIT function id is available."),
			*FunctionArgument,
			ANSI_TO_TCHAR(Function->GetDeclaration(true, true, false)));
		return;
	}

	UE_LOG(
		LogStaticJITDiagnostics,
		Log,
		TEXT("StaticJIT diagnostics function: Argument='%s' Declaration='%s' FunctionId=0x%x Registered=%s HasJitFunction=%s HasRawJitFunction=%s HasParmsJitFunction=%s EntryCount=%d"),
		*FunctionArgument,
		ANSI_TO_TCHAR(Function->GetDeclaration(true, true, false)),
		FunctionId,
		IsFunctionRegistered(FunctionId) ? TEXT("true") : TEXT("false"),
		Function->jitFunction != nullptr ? TEXT("true") : TEXT("false"),
		Function->jitFunction_Raw != nullptr ? TEXT("true") : TEXT("false"),
		Function->jitFunction_ParmsEntry != nullptr ? TEXT("true") : TEXT("false"),
		GetEntryCount(FunctionId));
}

bool FStaticJITDiagnostics::ReferenceGlobalVariableTwice(FAngelscriptPrecompiledData& Data, void* GlobalPtr, int64& OutFirstReference, int64& OutReusedReference, FString& OutFirstName, FString& OutReusedName)
{
	const FAngelscriptPrecompiledReference FirstReference = Data.ReferenceGlobalVariable(GlobalPtr, &OutFirstName);
	const FAngelscriptPrecompiledReference ReusedReference = Data.ReferenceGlobalVariable(GlobalPtr, &OutReusedName);
	OutFirstReference = FirstReference.OldReference;
	OutReusedReference = ReusedReference.OldReference;
	return OutFirstReference != 0 && OutFirstReference == OutReusedReference && !OutReusedName.IsEmpty();
}

bool FStaticJITDiagnostics::ExerciseRepeatedGlobalReferenceLoad(FAngelscriptPrecompiledData& Data, const FString& CacheFilename, int64 GlobalReference, void*& OutFirstResolvedAddress, void*& OutSecondResolvedAddress, bool& bOutCacheClearedAfterLoad)
{
	OutFirstResolvedAddress = nullptr;
	OutSecondResolvedAddress = nullptr;
	bOutCacheClearedAfterLoad = false;

	const FAngelscriptPrecompiledReference Reference{ GlobalReference };
	OutFirstResolvedAddress = Data.GetGlobalVariable(Reference);
	if (OutFirstResolvedAddress == nullptr)
	{
		return false;
	}

	FJitRef_GlobalVar JitRef(static_cast<uint64>(GlobalReference));
	ON_SCOPE_EXIT
	{
		FJITDatabase::Get().GlobalVarLookups.RemoveSingleSwap(&JitRef);
	};

	JitRef.Pointer = reinterpret_cast<void*>(0x1);
	JitRef.Pointer = Data.GetGlobalVariable(Reference);
	if (JitRef.Get() != OutFirstResolvedAddress)
	{
		return false;
	}

	Data.Load(CacheFilename);
	bOutCacheClearedAfterLoad = !Data.CachedPointerReferences.Contains(GlobalReference);
	OutSecondResolvedAddress = Data.GetGlobalVariable(Reference);
	return bOutCacheClearedAfterLoad && OutSecondResolvedAddress == OutFirstResolvedAddress;
}

#endif
