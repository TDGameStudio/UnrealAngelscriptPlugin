#include "AngelscriptEngine.h"
#include "AngelscriptBinds.h"
#include "AngelscriptBindDatabase.h"
#include "AngelscriptSourceProvider.h"
#include "AngelscriptMemoryTags.h"
#include "AngelscriptPerformanceStats.h"
#include "HAL/MallocLeakDetection.h"
#include "Binds/BlueprintEventSignatureRegistry.h"
#include "Binds/Helper_GetTypeInfo.h"
#include "Binds/Helper_ToString.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheCompileReuse.h"
#include "Cache/AngelscriptCacheExactStartup.h"
#include "Cache/Private/AngelscriptCacheRuntimeState.h"
#include "Cache/AngelscriptCacheEnvironmentProfile.h"
#include "Cache/AngelscriptCacheLegacyCutover.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheSettings.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Compilation/AngelscriptCompilationContext.h"
#include "Compilation/AngelscriptCompilationEvents.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopeExit.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Engine/Engine.h"
#include "Engine/AssetManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Engine/UserDefinedEnum.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Interfaces/IPluginManager.h"

#include "AngelscriptRuntimeModule.h"
#include "AngelscriptSubsystem.h"
#include "AngelscriptInclude.h"
#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "AngelscriptBindDatabase.h"
#include "Binds/Helper_ToString.h"

#include "StaticJIT/PrecompiledData.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/StaticJITBinds.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/ThreadSafeBool.h"
#include "Templates/Atomic.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
//#include "as_module.h"
//#include "as_builder.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "source/as_module.h"
#include "source/as_builder.h"
#include "EndAngelscriptHeaders.h"

#include "Testing/AngelscriptBindExecutionObservation.h"
#include "Testing/AngelscriptEnumTableBaselineProbe.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Testing/AngelscriptScriptTestAutomation.h"
#endif
#include "Testing/AngelscriptScriptTestHotReloadRunner.h"
#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "Testing/AngelscriptTestSettings.h"

#if WITH_AS_COVERAGE
#include "Extension/CodeCoverage/AngelscriptCodeCoverage.h"
#endif

DEFINE_LOG_CATEGORY(Angelscript);

static FName NAME_ReplicatedUsing("ReplicatedUsing");
static FName NAME_BlueprintSetter("BlueprintSetter");
static FName NAME_BlueprintGetter("BlueprintGetter");

static TArray<FAngelscriptEngine*> GAngelscriptEngineContextStack;
#if WITH_DEV_AUTOMATION_TESTS
static thread_local int32 GAngelscriptEngineResolutionSuppressionDepthForTesting = 0;
#endif
static TArray<FName> GLegacyStaticNames;
static TMap<FName, int32> GLegacyStaticNamesByIndex;
static int32 GAngelscriptPackageRefCount = 0;
static int32 GAngelscriptAssetsPackageRefCount = 0;
FAngelscriptEngine::FAngelscriptDebugStack* GAngelscriptStack = nullptr;
// GAngelscriptEngine removed — engine resolution now uses FAngelscriptEngineContextStack
static bool GAngelscriptLineReentry = false;
bool FAngelscriptEngine::bStaticJITTranspiledCodeLoaded = false;

namespace AngelscriptStaticTypeInfo_Private
{
	static TArray<FAngelscriptStaticTypeInfoClearer>& GetClearers()
	{
		static TArray<FAngelscriptStaticTypeInfoClearer> Clearers;
		return Clearers;
	}
}

void FAngelscriptStaticTypeInfoRegistry::RegisterClearer(FAngelscriptStaticTypeInfoClearer Clearer)
{
	if (Clearer == nullptr)
	{
		return;
	}

	TArray<FAngelscriptStaticTypeInfoClearer>& Clearers = AngelscriptStaticTypeInfo_Private::GetClearers();
	if (!Clearers.Contains(Clearer))
	{
		Clearers.Add(Clearer);
	}
}

void FAngelscriptStaticTypeInfoRegistry::ClearForEngine(asIScriptEngine* ScriptEngine)
{
	if (ScriptEngine == nullptr)
	{
		return;
	}

	for (FAngelscriptStaticTypeInfoClearer Clearer : AngelscriptStaticTypeInfo_Private::GetClearers())
	{
		Clearer(ScriptEngine);
	}
}

static int32 GAngelscriptRecompileAvoidance = 1;
static FAutoConsoleVariableRef CVar_AngelscriptRecompileAvoidance(TEXT("angelscript.UseRecompileAvoidance"), GAngelscriptRecompileAvoidance, TEXT(""));

namespace AngelscriptEnginePackages_Private
{
	static constexpr const TCHAR* ScriptPackageName = TEXT("/Script/Angelscript");
	static constexpr const TCHAR* AssetsPackageName = TEXT("/Script/AngelscriptAssets");
}

namespace AngelscriptEngineCompilationEvents_Private
{
	void AddModuleSummary(FAngelscriptCompilationEvent& Event, const TSharedRef<FAngelscriptModuleDesc>& Module)
	{
		Event.ModuleNames.AddUnique(Module->ModuleName);
		for (const FAngelscriptModuleDesc::FCodeSection& Section : Module->Code)
		{
			Event.FileNames.AddUnique(Section.RelativeFilename);
		}

		for (const FString& ImportedModuleName : Module->ImportedModules)
		{
			Event.ImportedModuleNames.AddUnique(ImportedModuleName);
		}

		Event.ImportCount += Module->ImportedModules.Num();
		Event.ClassCount += Module->Classes.Num();
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Module->Classes)
		{
			Event.FunctionCount += ClassDesc->Methods.Num();
		}

		Event.bLoadedPrecompiledCode |= Module->bLoadedPrecompiledCode;
		Event.bLoadedIncrementalCache |= Module->bLoadedIncrementalCache;
	}

	void FinalizeModuleCounts(FAngelscriptCompilationEvent& Event)
	{
		Event.ModuleCount = Event.ModuleNames.Num();
		Event.FileCount = Event.FileNames.Num();
	}

	void AddModulesSummary(FAngelscriptCompilationEvent& Event, const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules)
	{
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			AddModuleSummary(Event, Module);
		}
		FinalizeModuleCounts(Event);
	}

	void AddDiagnosticSummary(FAngelscriptCompilationEvent& Event, const TMap<FString, FAngelscriptEngine::FDiagnostics>& Diagnostics)
	{
		for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& DiagnosticSet : Diagnostics)
		{
			for (const FAngelscriptEngine::FDiagnostic& Diagnostic : DiagnosticSet.Value.Diagnostics)
			{
				++Event.DiagnosticCount;
				Event.Messages.Add(Diagnostic.Message);
			}
		}
	}

	void BroadcastCompileEvent(
		EAngelscriptCompilationEventType EventType,
		FName Phase,
		uint64 CompilationRunId,
		ECompileType CompileType,
		EAngelscriptCompileCachePolicy CachePolicy,
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules)
	{
		if (!FAngelscriptCompilationEvents::HasListeners())
		{
			return;
		}

		FAngelscriptCompilationEvent Event;
		Event.Type = EventType;
		Event.Phase = Phase;
		Event.CompilationRunId = CompilationRunId;
		Event.CompileType = CompileType;
		Event.CachePolicy = CachePolicy;
		AddModulesSummary(Event, Modules);
		FAngelscriptCompilationEvents::Broadcast(Event);
	}

	void BroadcastModuleEvent(
		EAngelscriptCompilationEventType EventType,
		FName Phase,
		uint64 CompilationRunId,
		ECompileType CompileType,
		EAngelscriptCompileCachePolicy CachePolicy,
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		bool bSucceeded,
		bool bJitAvailable = false,
		bool bJitHandoff = false)
	{
		if (!FAngelscriptCompilationEvents::HasListeners())
		{
			return;
		}

		FAngelscriptCompilationEvent Event;
		Event.Type = EventType;
		Event.Phase = Phase;
		Event.CompilationRunId = CompilationRunId;
		Event.CompileType = CompileType;
		Event.CachePolicy = CachePolicy;
		Event.bSucceeded = bSucceeded;
		Event.bFailed = !bSucceeded;
		Event.bJitAvailable = bJitAvailable;
		Event.bJitHandoff = bJitHandoff;
		AddModuleSummary(Event, Module);
		FinalizeModuleCounts(Event);
		FAngelscriptCompilationEvents::Broadcast(Event);
	}
}

static UObject* GAmbientWorldContext = nullptr;
class asCThreadLocalData* FAngelscriptEngine::GameThreadTLD = nullptr;
thread_local FAngelscriptContextPool GAngelscriptContextPool;

bool PrepareAngelscriptContextWithLog(asIScriptContext* Context, asIScriptFunction* ScriptFunction, const TCHAR* Callsite)
{
	check(Context != nullptr);
	check(ScriptFunction != nullptr);

	const int32 PrepareResult = Context->Prepare(ScriptFunction);
	if (PrepareResult >= 0)
	{
		return true;
	}

	UE_LOG(
		Angelscript,
		Error,
		TEXT("Failed to prepare Angelscript context for '%s' using '%s' (Result=%d, ContextEngine=%p, FunctionEngine=%p)."),
		Callsite != nullptr ? Callsite : TEXT("<unknown>"),
		ANSI_TO_TCHAR(ScriptFunction->GetDeclaration(true, true, false, true)),
		PrepareResult,
		Context->GetEngine(),
		ScriptFunction->GetEngine());
	return false;
}

bool FAngelscriptEngine::CanCastScriptObjectToUnrealInterface(asITypeInfo* RuntimeType, asITypeInfo* TargetType, void* ObjectPtr)
{
	if (RuntimeType == nullptr || TargetType == nullptr || ObjectPtr == nullptr)
	{
		return false;
	}

	UClass* TargetClass = reinterpret_cast<UClass*>(TargetType->GetUserData());
	if (TargetClass == nullptr || !TargetClass->HasAnyClassFlags(CLASS_Interface))
	{
		return false;
	}

	UObject* Object = reinterpret_cast<UObject*>(ObjectPtr);
	UClass* ObjectClass = Object != nullptr ? Object->GetClass() : nullptr;
	const bool bImplementsInterface = ObjectClass != nullptr && ObjectClass->ImplementsInterface(TargetClass);
	UE_LOG(
		Angelscript,
		Display,
		TEXT("QuickScriptInterfaceCast runtimeType=%hs targetType=%hs targetClass=%s objectClass=%s implements=%s"),
		RuntimeType->GetName(),
		TargetType->GetName(),
		*TargetClass->GetName(),
		ObjectClass != nullptr ? *ObjectClass->GetName() : TEXT("<null>"),
		bImplementsInterface ? TEXT("true") : TEXT("false"));
	return bImplementsInterface;
}

struct FAngelscriptEngineLifetimeToken
{
};

void LogAngelscriptException(asIScriptContext* Context);
void AngelscriptLineCallback(asCContext* Context);
void AngelscriptStackPopCallback(asCContext* Context, void* OldStackFrameStart, void* OldStackFrameEnd);
void AngelscriptLoopDetectionCallback(asCContext* Context);

static asCContext* TryTakeContextFromPool(TArray<asCContext*>& Pool, asIScriptEngine* DesiredScriptEngine)
{
	if (Pool.Num() == 0)
	{
		return nullptr;
	}

	if (DesiredScriptEngine == nullptr)
	{
		return Pool.Pop(EAllowShrinking::No);
	}

	for (int32 Index = Pool.Num() - 1; Index >= 0; --Index)
	{
		asCContext* Candidate = Pool[Index];
		if (Candidate != nullptr && Candidate->GetEngine() == DesiredScriptEngine)
		{
			Pool.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			return Candidate;
		}
	}

	return nullptr;
}

static void ReleaseContextsForScriptEngine(TArray<asCContext*>& Pool, asIScriptEngine* ScriptEngine)
{
	if (ScriptEngine == nullptr)
	{
		return;
	}

	for (int32 Index = Pool.Num() - 1; Index >= 0; --Index)
	{
		asCContext* Context = Pool[Index];
		if (Context == nullptr || Context->GetEngine() != ScriptEngine)
		{
			continue;
		}

		check(Context->GetState() != asEXECUTION_ACTIVE);
		check(Context->GetState() != asEXECUTION_SUSPENDED);
		Context->Unprepare();
		Context->Release();
		Pool.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}
}

static void ReleaseAllContextsInPool(TArray<asCContext*>& Pool)
{
	for (asCContext* Context : Pool)
	{
		if (Context == nullptr)
		{
			continue;
		}

		check(Context->GetState() != asEXECUTION_ACTIVE);
		check(Context->GetState() != asEXECUTION_SUSPENDED);
		if (Context->GetState() != asEXECUTION_UNINITIALIZED)
		{
			check(Context->Unprepare() >= 0);
		}
		Context->Release();
	}

	Pool.Empty();
}

static asCContext* CreateConfiguredContext(asIScriptEngine* ScriptEngine)
{
	check(ScriptEngine != nullptr);

	auto* Context = static_cast<asCContext*>(ScriptEngine->CreateContext());
	Context->SetExceptionCallback(asFUNCTION(LogAngelscriptException), 0, asCALL_CDECL);
#if WITH_AS_DEBUGVALUES || WITH_AS_DEBUGSERVER
	Context->SetLineCallback(AngelscriptLineCallback);
	Context->SetStackPopCallback(AngelscriptStackPopCallback);
#endif
#if WITH_EDITOR
	if (!IsRunningCommandlet())
	{
		Context->SetLoopDetectionCallback(AngelscriptLoopDetectionCallback);
	}
#elif !UE_BUILD_TEST && !UE_BUILD_SHIPPING
	Context->SetLoopDetectionCallback(AngelscriptLoopDetectionCallback);
#endif
	return Context;
}

static void ResetContextForPooling(asCContext* Context)
{
	check(Context != nullptr);
	check(Context->GetState() != asEXECUTION_ACTIVE);
	check(Context->GetState() != asEXECUTION_SUSPENDED);
	check(Context->Unprepare() >= 0);
}

static void SetAmbientWorldContext(UObject* NewWorldContext)
{
	if (NewWorldContext != nullptr && !NewWorldContext->IsValidLowLevelFast(false))
	{
		NewWorldContext = nullptr;
	}

	*(UObject* volatile*)&GAmbientWorldContext = NewWorldContext;
	check(FAngelscriptEngine::CanUseGameThreadData());

#if WITH_EDITOR
	extern ANGELSCRIPTRUNTIME_API void SetAngelscriptWorldContextAvailable(bool bAvailable);
	SetAngelscriptWorldContextAvailable(
		(NewWorldContext != nullptr)
		&& !NewWorldContext->HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject)
		&& (NewWorldContext->GetWorld() != nullptr));
#endif
}

static void SyncAmbientWorldContextFromCurrentEngine()
{
	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		SetAmbientWorldContext(CurrentEngine->GetCurrentWorldContextObject());
		return;
	}

	SetAmbientWorldContext(nullptr);
}

UObject* FAngelscriptEngine::GetAmbientWorldContext()
{
	return GAmbientWorldContext;
}

bool FAngelscriptEngine::IsSimulatingCookedForCurrentContext()
{
	if (FAngelscriptEngine* Eng = TryGetCurrentEngine()) return Eng->bSimulateCooked;
	return false;
}

bool FAngelscriptEngine::IsTestingErrorsForCurrentContext()
{
	if (FAngelscriptEngine* Eng = TryGetCurrentEngine()) return Eng->bTestErrors;
	return false;
}

bool FAngelscriptEngine::IsHotReloadingForCurrentContext()
{
	if (FAngelscriptEngine* Eng = TryGetCurrentEngine()) return Eng->bIsHotReloading;
	return false;
}

bool FAngelscriptEngine::IsForcingPreprocessEditorCodeForCurrentContext()
{
	if (FAngelscriptEngine* Eng = TryGetCurrentEngine()) return Eng->bForcePreprocessEditorCode;
	return false;
}

void LogAngelscriptError(asSMessageInfo* Message, void* DataPtr);
void LogAngelscriptException(asIScriptContext* Context);
void AngelscriptLineCallback(asCContext* Context);
void AngelscriptStackPopCallback(asCContext* Context, void* OldStackFrameStart, void* OldStackFrameEnd);
void AngelscriptLoopDetectionCallback(asCContext* Context);

bool MakePathRelativeTo_IgnoreCase(FString& InPath, const TCHAR* InRelativeTo);

asIScriptContext* AngelscriptRequestContext(asIScriptEngine* Engine, void* Data);
void AngelscriptReturnContext(asIScriptEngine* Engine, asIScriptContext* Context, void* Data);

void FAngelscriptEngineContextStack::Push(FAngelscriptEngine* Engine)
{
	if (Engine != nullptr)
	{
		GAngelscriptEngineContextStack.Add(Engine);
	}
}

void FAngelscriptEngineContextStack::Pop(FAngelscriptEngine* Engine)
{
	if (Engine == nullptr || GAngelscriptEngineContextStack.Num() == 0)
	{
		return;
	}

	ensureAlwaysMsgf(GAngelscriptEngineContextStack.Last() == Engine, TEXT("Angelscript engine context stack pop order mismatch."));
	if (GAngelscriptEngineContextStack.Last() == Engine)
	{
		GAngelscriptEngineContextStack.Pop();
	}
}

FAngelscriptEngine* FAngelscriptEngineContextStack::Peek()
{
	return GAngelscriptEngineContextStack.Num() > 0 ? GAngelscriptEngineContextStack.Last() : nullptr;
}

bool FAngelscriptEngineContextStack::IsEmpty()
{
	return GAngelscriptEngineContextStack.Num() == 0;
}

#if WITH_DEV_AUTOMATION_TESTS
TArray<FAngelscriptEngine*> FAngelscriptEngineContextStack::SnapshotAndClear()
{
	TArray<FAngelscriptEngine*> Saved = MoveTemp(GAngelscriptEngineContextStack);
	GAngelscriptEngineContextStack.Empty();
	return Saved;
}

void FAngelscriptEngineContextStack::RestoreSnapshot(TArray<FAngelscriptEngine*>&& SavedStack)
{
	GAngelscriptEngineContextStack = MoveTemp(SavedStack);
}

void FAngelscriptEngineContextStack::PushEngineResolutionSuppressionForTesting()
{
	++GAngelscriptEngineResolutionSuppressionDepthForTesting;
}

void FAngelscriptEngineContextStack::PopEngineResolutionSuppressionForTesting()
{
	checkf(GAngelscriptEngineResolutionSuppressionDepthForTesting > 0,
		TEXT("Angelscript engine-resolution suppression scope underflow."));
	--GAngelscriptEngineResolutionSuppressionDepthForTesting;
}

bool FAngelscriptEngineContextStack::IsEngineResolutionSuppressedForTesting()
{
	return GAngelscriptEngineResolutionSuppressionDepthForTesting > 0;
}

FScopedAngelscriptEngineResolutionSuppressionForTesting::FScopedAngelscriptEngineResolutionSuppressionForTesting()
{
	FAngelscriptEngineContextStack::PushEngineResolutionSuppressionForTesting();
}

FScopedAngelscriptEngineResolutionSuppressionForTesting::~FScopedAngelscriptEngineResolutionSuppressionForTesting()
{
	FAngelscriptEngineContextStack::PopEngineResolutionSuppressionForTesting();
}
#endif

FAngelscriptEngineScope::FAngelscriptEngineScope(FAngelscriptEngine& InEngine, UObject* InWorldContext)
	: Engine(&InEngine)
{
	PreviousEngineWorldContext = InEngine.WorldContextObject;
	FAngelscriptEngineContextStack::Push(Engine);
	UE_LOG(Angelscript, VeryVerbose, TEXT("[EngineScope] Push engine=%p stackDepth=%d worldCtx=%s"),
		Engine, GAngelscriptEngineContextStack.Num(),
		InWorldContext ? *InWorldContext->GetName() : TEXT("none"));
	if (InWorldContext != nullptr)
	{
		PreviousWorldContext = GAmbientWorldContext;
		FAngelscriptEngine::AssignWorldContext(InWorldContext);
		bChangedWorldContext = true;
	}
	else
	{
		SyncAmbientWorldContextFromCurrentEngine();
	}
}

FAngelscriptEngineScope::~FAngelscriptEngineScope()
{
	Reset();
}

FAngelscriptEngineScope::FAngelscriptEngineScope(FAngelscriptEngineScope&& Other) noexcept
	: Engine(Other.Engine)
	, PreviousWorldContext(Other.PreviousWorldContext)
	, PreviousEngineWorldContext(Other.PreviousEngineWorldContext)
	, bChangedWorldContext(Other.bChangedWorldContext)
{
	Other.Engine = nullptr;
	Other.PreviousWorldContext = nullptr;
	Other.PreviousEngineWorldContext = nullptr;
	Other.bChangedWorldContext = false;
}

FAngelscriptEngineScope& FAngelscriptEngineScope::operator=(FAngelscriptEngineScope&& Other) noexcept
{
	if (this != &Other)
	{
		Reset();
		Engine = Other.Engine;
		PreviousWorldContext = Other.PreviousWorldContext;
		PreviousEngineWorldContext = Other.PreviousEngineWorldContext;
		bChangedWorldContext = Other.bChangedWorldContext;
		Other.Engine = nullptr;
		Other.PreviousWorldContext = nullptr;
		Other.PreviousEngineWorldContext = nullptr;
		Other.bChangedWorldContext = false;
	}
	return *this;
}

void FAngelscriptEngineScope::Reset()
{
	if (Engine == nullptr)
	{
		return;
	}

	UE_LOG(Angelscript, VeryVerbose, TEXT("[EngineScope] Pop engine=%p stackDepthBefore=%d"),
		Engine, GAngelscriptEngineContextStack.Num());

	if (bChangedWorldContext)
	{
		Engine->WorldContextObject = PreviousEngineWorldContext;
	}

	FAngelscriptEngineContextStack::Pop(Engine);
	SyncAmbientWorldContextFromCurrentEngine();
	Engine = nullptr;
	PreviousWorldContext = nullptr;
	PreviousEngineWorldContext = nullptr;
	bChangedWorldContext = false;
}

FAngelscriptCachePackPolicy ResolveAngelscriptCacheWriterPolicy(
	const FAngelscriptEngineConfig& Config,
	const uint32 ConfiguredPackTargetMiB,
	const bool bConfiguredParallelPreparation,
	const uint32 ConfiguredPreparationWorkerCount)
{
	FAngelscriptCachePackPolicy Policy;
	const uint32 TargetMiB = FMath::Clamp<uint32>(
		Config.CacheV2PackTargetMiBOverride > 0
			? Config.CacheV2PackTargetMiBOverride
			: ConfiguredPackTargetMiB,
		1, 256);
	Policy.TargetRawBytesPerPack =
		static_cast<uint64>(TargetMiB) * 1024 * 1024;
	const bool bUseParallel = bConfiguredParallelPreparation
		&& !Config.bForceSerialCacheV2Preparation;
	Policy.ExecutionMode = bUseParallel
		? EAngelscriptCachePreparationExecutionMode::BoundedParallel
		: EAngelscriptCachePreparationExecutionMode::ForcedSerial;
	Policy.MaxWorkerCount = bUseParallel
		? FMath::Clamp<uint32>(
			Config.CacheV2PreparationWorkerCountOverride > 0
				? Config.CacheV2PreparationWorkerCountOverride
				: ConfiguredPreparationWorkerCount,
			1, 64)
		: 1;
	return Policy;
}

FAngelscriptEngineConfig FAngelscriptEngineConfig::FromCurrentProcess()
{
	FAngelscriptEngineConfig Config;
	Config.bForceThreadedInitialize = FParse::Param(FCommandLine::Get(), TEXT("as-force-threaded-initialize"));
	Config.bSkipThreadedInitialize = FParse::Param(FCommandLine::Get(), TEXT("as-skip-threaded-initialize"));
	Config.bSimulateCooked = FParse::Param(FCommandLine::Get(), TEXT("as-simulate-cooked"));
	Config.bTestErrors = FParse::Param(FCommandLine::Get(), TEXT("as-test-errors"));
	Config.bForcePreprocessEditorCode = FParse::Param(FCommandLine::Get(), TEXT("as-force-preprocess-editor-code"));
	Config.bDevelopmentMode = FParse::Param(FCommandLine::Get(), TEXT("as-development-mode"));
	Config.bSkipWriteBindDB = FParse::Param(FCommandLine::Get(), TEXT("as-skip-write-bind-db"));
	Config.bWriteBindDB = FParse::Param(FCommandLine::Get(), TEXT("as-write-bind-db"));
	Config.bExitOnError = FParse::Param(FCommandLine::Get(), TEXT("as-exit-on-error"));
	Config.bDumpDocumentation = FParse::Param(FCommandLine::Get(), TEXT("dump-as-doc"));
	FParse::Value(FCommandLine::Get(), TEXT("-asdebugport="), Config.DebugServerPort);
	FParse::Value(
		FCommandLine::Get(),
		TEXT("-as-cache-report="),
		Config.CacheV2ReportPathOverride);
	Config.bExitAfterStartupForCacheSmoke =
		FParse::Param(FCommandLine::Get(), TEXT("as-cache-exit-after-startup"));
	Config.bForceEnableCacheV2DecisionTrace =
		FParse::Param(FCommandLine::Get(), TEXT("as-cache-trace"));
	FParse::Value(
		FCommandLine::Get(),
		TEXT("-as-cache-trace-capacity="),
		Config.CacheV2DecisionTraceCapacityOverride);
	FParse::Value(
		FCommandLine::Get(),
		TEXT("-as-cache-pack-target-mib="),
		Config.CacheV2PackTargetMiBOverride);
	FParse::Value(
		FCommandLine::Get(),
		TEXT("-as-cache-preparation-workers="),
		Config.CacheV2PreparationWorkerCountOverride);
	Config.bForceSerialCacheV2Preparation = FParse::Param(
		FCommandLine::Get(), TEXT("as-cache-force-serial-preparation"));
#if WITH_EDITOR
	Config.bIsEditor = GIsEditor;
#else
	Config.bIsEditor = false;
#endif
	Config.bRunningCommandlet = IsRunningCommandlet();
	Config.bIsUnattended = FApp::IsUnattended();
	return Config;
}

EAngelscriptStartupCompileFailureResponse
ResolveAngelscriptStartupCompileFailureResponse(
	const FAngelscriptEngineConfig& Config,
	bool bInteractiveRetryAvailable)
{
	if (Config.bRunningCommandlet
		|| Config.bExitOnError
		|| Config.bIsUnattended
		|| !bInteractiveRetryAvailable)
	{
		return EAngelscriptStartupCompileFailureResponse::RequestExit;
	}

	return EAngelscriptStartupCompileFailureResponse::InteractiveRetry;
}

bool ShouldRequestAngelscriptCachePackageSmokeExit(
	const FAngelscriptEngineConfig& Config,
	bool bInitialCompileSucceeded)
{
	return Config.bExitAfterStartupForCacheSmoke && bInitialCompileSucceeded;
}

FAngelscriptStartupCompileFailureExitRequest
ResolveAngelscriptStartupCompileFailureExitRequest(
	const FAngelscriptEngineConfig& Config)
{
	(void)Config;
	FAngelscriptStartupCompileFailureExitRequest Request;
	Request.bForce = true;
	Request.bBeginCacheShutdownBeforeDiagnosticReport = true;
	Request.bWriteRequestedDiagnosticReportBeforeExit = true;
	Request.Status = 3;
	return Request;
}

FAngelscriptEngineDependencies FAngelscriptEngineDependencies::CreateDefault()
{
	FAngelscriptEngineDependencies Dependencies;
	Dependencies.GetProjectDir = []()
	{
		return FPaths::ProjectDir();
	};
	Dependencies.ConvertRelativePathToFull = [](const FString& Path)
	{
		return FPaths::ConvertRelativePathToFull(Path);
	};
	Dependencies.DirectoryExists = [](const FString& Path)
	{
		return IFileManager::Get().DirectoryExists(*Path);
	};
	Dependencies.MakeDirectory = [](const FString& Path, bool bTree)
	{
		return IFileManager::Get().MakeDirectory(*Path, bTree);
	};
	Dependencies.GetEnabledPluginScriptRoots = []()
	{
		TArray<FString> ScriptRoots;
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			ScriptRoots.Add(Plugin->GetBaseDir() / TEXT("Script"));
		}
		return ScriptRoots;
	};
	Dependencies.GetEnabledPluginScriptRootDescriptors = []()
	{
		TArray<FAngelscriptPluginScriptRoot> ScriptRoots;
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			FAngelscriptPluginScriptRoot& ScriptRoot = ScriptRoots.AddDefaulted_GetRef();
			ScriptRoot.PluginName = Plugin->GetName();
			ScriptRoot.ScriptRoot = Plugin->GetBaseDir() / TEXT("Script");
		}
		return ScriptRoots;
	};
	Dependencies.SourceProvider = MakeShared<FAngelscriptDiskSourceProvider>();
	return Dependencies;
}

FAngelscriptEngine::FAngelscriptEngine()
	: FAngelscriptEngine(FAngelscriptEngineConfig::FromCurrentProcess(), FAngelscriptEngineDependencies::CreateDefault())
{
}

FAngelscriptEngine::FAngelscriptEngine(const FAngelscriptEngineConfig& InConfig, const FAngelscriptEngineDependencies& InDependencies)
	: LifetimeToken(MakeShared<FAngelscriptEngineLifetimeToken>())
	, RuntimeConfig(InConfig)
	, Dependencies(InDependencies)
{
	CacheService = MakeUnique<FAngelscriptCacheService>();
	const UAngelscriptCacheSettings* CacheSettings =
		GetDefault<UAngelscriptCacheSettings>();
	bool bEnableDecisionTrace =
		RuntimeConfig.bForceEnableCacheV2DecisionTrace;
	uint32 DecisionTraceCapacity =
		RuntimeConfig.CacheV2DecisionTraceCapacityOverride > 0
		? RuntimeConfig.CacheV2DecisionTraceCapacityOverride
		: 1024;
	if (CacheSettings != nullptr)
	{
		bEnableDecisionTrace = bEnableDecisionTrace
			|| CacheSettings->bEnableDecisionTrace;
		if (RuntimeConfig.CacheV2DecisionTraceCapacityOverride == 0)
		{
			DecisionTraceCapacity = CacheSettings->DecisionTraceCapacity;
		}
	}
	CacheService->ConfigureDecisionTrace(
		bEnableDecisionTrace, DecisionTraceCapacity);
	CacheService->ConfigureWriterPolicy(ResolveAngelscriptCacheWriterPolicy(
		RuntimeConfig,
		CacheSettings != nullptr ? CacheSettings->PackTargetMiB : 64,
		CacheSettings == nullptr || CacheSettings->bEnableParallelPreparation,
		CacheSettings != nullptr
			? CacheSettings->MaxPreparationWorkerCount
			: 4));

	if (CacheSettings != nullptr)
	{
		PackagedRuntimeReloadMode =
			RuntimeConfig.bOverridePackagedRuntimeReloadMode
			? RuntimeConfig.PackagedRuntimeReloadMode
			: CacheSettings->PackagedRuntimeReloadMode;
		PackagedRuntimeReloadScanIntervalSeconds = FMath::Max(
			0.1f,
			RuntimeConfig.bOverridePackagedRuntimeReloadMode
			? RuntimeConfig.PackagedRuntimeReloadScanIntervalSeconds
			: CacheSettings->RuntimeReloadScanIntervalSeconds);
	}
	else if (RuntimeConfig.bOverridePackagedRuntimeReloadMode)
	{
		PackagedRuntimeReloadMode = RuntimeConfig.PackagedRuntimeReloadMode;
		PackagedRuntimeReloadScanIntervalSeconds = FMath::Max(
			0.1f,
			RuntimeConfig.PackagedRuntimeReloadScanIntervalSeconds);
	}

	// Editor source changes remain owned by AngelscriptEditor's directory
	// watcher and class-reinstancing path. This policy only governs a
	// non-editor Runtime engine, including packaged Development/Shipping.
	if (RuntimeConfig.bIsEditor)
	{
		PackagedRuntimeReloadMode =
			EAngelscriptPackagedRuntimeReloadMode::Disabled;
	}
	if (!Dependencies.SourceProvider.IsValid())
	{
		Dependencies.SourceProvider = MakeShared<FAngelscriptDiskSourceProvider>();
	}
}

FAngelscriptEngine::~FAngelscriptEngine()
{
	UE_LOG(Angelscript, Verbose, TEXT("[EngineLifecycle] Destroying engine=%p"), this);
	Shutdown();
}

FString FAngelscriptEngine::MakeModuleName(const FString& ModuleName) const
{
	return ModuleName;
}

TUniquePtr<FAngelscriptEngine> FAngelscriptEngine::Create(const FAngelscriptEngineConfig& InConfig, const FAngelscriptEngineDependencies& InDependencies)
{
	TUniquePtr<FAngelscriptEngine> EngineInstance = MakeUnique<FAngelscriptEngine>(InConfig, InDependencies);
	UE_LOG(Angelscript, Verbose, TEXT("[EngineLifecycle] Create -> %p (bSkipInitialCompile=%d)"),
		EngineInstance.Get(),
		InConfig.bSkipInitialCompile ? 1 : 0);
	const bool bInitialized = InConfig.bSkipInitialCompile
		? EngineInstance->InitializeWithoutInitialCompile()
		: EngineInstance->Initialize();
	if (!bInitialized)
	{
		EngineInstance.Reset();
	}
	return EngineInstance;
}

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
	void AngelscriptResolveObjectPtr(void** PointerToObjectPtr)
	{
		(void)((FObjectPtr*)PointerToObjectPtr)->Get();
	}
#endif

bool FAngelscriptEngine::IsInitialized()
{
	if (FAngelscriptEngineContextStack::Peek() != nullptr)
	{
		return true;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (FAngelscriptEngineContextStack::IsEngineResolutionSuppressedForTesting())
	{
		return false;
	}
#endif

	if (UAngelscriptSubsystem* Subsystem = UAngelscriptSubsystem::Get())
	{
		return Subsystem->GetEngine() != nullptr;
	}

	return false;
}

UObject* FAngelscriptEngine::TryGetCurrentWorldContextObject()
{
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		return CurrentEngine->GetCurrentWorldContextObject();
	}

	return GAmbientWorldContext;
}

bool FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext()
{
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		return CurrentEngine->ShouldUseEditorScripts();
	}

	return false;
}

bool FAngelscriptEngine::ShouldUseAutomaticImportMethodForCurrentContext()
{
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		return CurrentEngine->ShouldUseAutomaticImportMethod();
	}

	return false;
}

bool FAngelscriptEngine::IsScriptDevelopmentModeForCurrentContext()
{
	if (FAngelscriptEngine* Eng = TryGetCurrentEngine()) return Eng->bScriptDevelopmentMode;
	return false;
}

FAngelscriptEngine* FAngelscriptEngine::TryGetCurrentEngine()
{
	if (FAngelscriptEngine* ScopedEngine = FAngelscriptEngineContextStack::Peek())
	{
		return ScopedEngine;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (FAngelscriptEngineContextStack::IsEngineResolutionSuppressedForTesting())
	{
		return nullptr;
	}
#endif

	if (UAngelscriptSubsystem* Subsystem = UAngelscriptSubsystem::Get())
	{
		if (FAngelscriptEngine* AttachedEngine = Subsystem->GetEngine())
		{
			return AttachedEngine;
		}
	}

	return nullptr;
}

FAngelscriptEngine* FAngelscriptEngine::TryGetGlobalEngine()
{
	return TryGetCurrentEngine();
}

void FAngelscriptEngine::SetGlobalEngine(FAngelscriptEngine* InEngine)
{
	SyncAmbientWorldContextFromCurrentEngine();
}

void FAngelscriptEngine::AssignWorldContext(UObject* NewWorldContext)
{
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		CurrentEngine->WorldContextObject = NewWorldContext;
	}

	SetAmbientWorldContext(NewWorldContext);
}

FAngelscriptEngine& FAngelscriptEngine::Get()
{
	FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine();
	if (UNLIKELY(CurrentEngine == nullptr))
	{
		UE_LOG(Angelscript, Error, TEXT("[EngineResolve] Get() failed: no engine available. contextStack=%d. "
			"Likely missing FAngelscriptEngineScope in the calling context."),
			GAngelscriptEngineContextStack.Num());
	}
	checkf(CurrentEngine != nullptr, TEXT("Attempted to use angelscript manager before initialization. Make sure FAngelscriptRuntimeModule::InitializeAngelscript has been called."));
	return *CurrentEngine;
}

FAngelscriptEngine& FAngelscriptEngine::GetOrCreate()
{
	FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine();
	checkf(CurrentEngine != nullptr, TEXT("GetOrCreate() is deprecated. Engine must be created by RuntimeModule or Subsystem."));
	return *CurrentEngine;
}

bool FAngelscriptEngine::DestroyGlobal()
{
	return false;
}

FString FAngelscriptEngine::GetScriptRootDirectory()
{
	FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine();
	if (UNLIKELY(CurrentEngine == nullptr))
	{
		UE_LOG(Angelscript, Error, TEXT("[EngineResolve] GetScriptRootDirectory() failed: no engine available. contextStack=%d. "
			"Likely missing FAngelscriptEngineScope in the calling context."),
			GAngelscriptEngineContextStack.Num());
	}
	checkf(CurrentEngine != nullptr, TEXT("Attempted to access Angelscript script roots before an engine was available."));
	const auto& AllRootPaths = CurrentEngine->AllRootPaths;
	// The first root in the list of roots is the game project root.
	return AllRootPaths.IsEmpty() ? TEXT("") : CurrentEngine->AllRootPaths[0];
}

UPackage* FAngelscriptEngine::GetPackage()
{
	FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine();
	if (UNLIKELY(CurrentEngine == nullptr))
	{
		UE_LOG(Angelscript, Error, TEXT("[EngineResolve] GetPackage() failed: no engine available. contextStack=%d. "
			"Likely missing FAngelscriptEngineScope in the calling context."),
			GAngelscriptEngineContextStack.Num());
	}
	checkf(CurrentEngine != nullptr, TEXT("Attempted to access the Angelscript package before an engine was available."));
	return CurrentEngine->AngelscriptPackage;
}

bool FAngelscriptEngine::ShouldInitializeThreaded()
{
	if (RuntimeConfig.bIsEditor)
	{
		return RuntimeConfig.bForceThreadedInitialize;
	}

#if AS_USE_BIND_DB
	// Defensive measure (pending verification), NOT a known hard requirement.
	//
	// Hazelight upstream runs cooked initialization on a worker thread by default and it
	// works there, so threaded cooked init is not inherently unsafe. The cooked crash we
	// originally chased was actually an ordering bug: Load(Binds.Cache) ran before this
	// engine's owned FAngelscriptBindDatabase was constructed, so the cache populated the
	// fallback LegacyBindDatabase and bind-time readers saw an empty database (every
	// reflected engine type unregistered). That root cause is fixed in Initialize_AnyThread()
	// by constructing BindDatabase before the Load call.
	//
	// We still force game-thread init for cooked builds as a precaution, because this fork's
	// engine-scoped-state refactor may have introduced other worker-thread hazards that have
	// not yet been validated against a packaged build. Once a threaded cooked run is verified
	// end-to-end, this block can be removed to regain upstream's parallel-init startup.
	if (!RuntimeConfig.bForceThreadedInitialize)
	{
		return false;
	}
#endif

	return !RuntimeConfig.bSkipThreadedInitialize;
}

namespace
{
	bool ExecuteThreadedInitializationAndWait(TFunction<bool()> InitializationWork)
	{
		TAtomic<bool> bInitializationDone(false);
		TAtomic<bool> bInitializationSucceeded(false);
		AsyncTask(
			ENamedThreads::AnyHiPriThreadHiPriTask,
			[InitializationWork = MoveTemp(InitializationWork),
			 &bInitializationDone,
			 &bInitializationSucceeded]() mutable
			{
				bInitializationSucceeded.Store(InitializationWork());
				bInitializationDone.Store(true);
			});

		while (!bInitializationDone.Load())
		{
			FCoreDelegates::OnAsyncLoadingFlushUpdate.Broadcast();
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FPlatformProcess::Sleep(0.002f);
		}

		return bInitializationSucceeded.Load();
	}
}

#if WITH_DEV_AUTOMATION_TESTS
ANGELSCRIPTRUNTIME_API bool GAngelscriptRunThreadedInitializationResultTransportForTesting(
	const bool bWorkerResult)
{
	return ExecuteThreadedInitializationAndWait([bWorkerResult]()
	{
		return bWorkerResult;
	});
}
#endif

bool FAngelscriptEngine::Initialize()
{
	if (bReadyForPublication)
	{
		return true;
	}
	if (Engine != nullptr)
	{
		return false;
	}

	bReadyForPublication = false;
	FAngelscriptEngineScope ScopedInitializingEngine(*this);

	PreInitialize_GameThread();

	if (ShouldInitializeThreaded())
	{
		const bool bInitializationSucceeded = ExecuteThreadedInitializationAndWait([this]()
		{
			FGCScopeGuard GCLock;

			auto* RealGameThreadTLD = GameThreadTLD;
			GameThreadTLD = asCThreadManager::GetLocalData();
			GameThreadTLD->primaryContext = RealGameThreadTLD->primaryContext;

			const bool bSucceeded = Initialize_AnyThread();

			GameThreadTLD->primaryContext = nullptr;
			GameThreadTLD = RealGameThreadTLD;
			return bSucceeded;
		});

		if (!bInitializationSucceeded)
		{
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::RecordPublicationResult(this, false);
			#endif
			return false;
		}
	}
	else
	{
		if (!Initialize_AnyThread())
		{
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::RecordPublicationResult(this, false);
			#endif
			return false;
		}
	}

	PostInitialize_GameThread();
	bReadyForPublication = true;
	FAngelscriptEngineExtensionRegistry::Get().AttachEngine(*this);
	bExtensionsAttached = true;
	#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptBindExecutionObservation::RecordPublicationResult(this, true);
	#endif
	return true;
}

bool FAngelscriptEngine::InitializeWithoutInitialCompile()
{
	if (Engine != nullptr)
	{
		return bReadyForPublication;
	}
	bReadyForPublication = false;
	FAngelscriptEngineScope ScopedInitializingEngine(*this);

	bSimulateCooked = RuntimeConfig.bSimulateCooked;
	bTestErrors = RuntimeConfig.bTestErrors;
	bForcePreprocessEditorCode = RuntimeConfig.bForcePreprocessEditorCode;
	bUseEditorScripts = WITH_EDITOR
		&& ((RuntimeConfig.bIsEditor && !RuntimeConfig.bRunningCommandlet) || bForcePreprocessEditorCode)
		&& !bSimulateCooked;
	bCollectStaticJITCompatibilityBinds =
		RuntimeConfig.bCollectStaticJITCompatibilityBinds;
	bScriptDevelopmentMode = RuntimeConfig.bIsEditor || RuntimeConfig.bDevelopmentMode;
	bUseStaticJITCompatibilityData = false;

	PreInitialize_GameThread();

	Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
	Engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, 1);
	Engine->SetEngineProperty(asEP_ALLOW_MULTILINE_STRINGS, 1);
	Engine->SetEngineProperty(asEP_SCRIPT_SCANNER, 1);
	Engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 1);
	Engine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, 0);
	Engine->SetEngineProperty(asEP_ALTER_SYNTAX_NAMED_ARGS, 1);
	Engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, 1);
	Engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, 1);
	Engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, 1);
	Engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, 1);
	Engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY, 1);
	Engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT, 1);
	Engine->SetEngineProperty(asEP_MEMBER_INIT_MODE, 0);
	Engine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 0);
	Engine->SetEngineProperty(asEP_TYPECHECK_SWITCH_ENUMS, 1);
	Engine->SetEngineProperty(asEP_FLOAT_IS_FLOAT64, ConfigSettings->bScriptFloatIsFloat64 ? 1 : 0);
	Engine->SetEngineProperty(asEP_ALLOW_DOUBLE_TYPE, ConfigSettings->bDeprecateDoubleType ? 0 : 1);
	Engine->SetEngineProperty(asEP_WARN_ON_FLOAT_CONSTANTS_FOR_DOUBLES, ConfigSettings->bWarnOnFloatConstantsForDoubleValues ? 1 : 0);
	Engine->SetEngineProperty(asEP_WARN_INTEGER_DIVISION, ConfigSettings->bWarnIntegerDivision ? 1 : 0);

	if (ShouldUseAutomaticImportMethod())
	{
		Engine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 1);
	}

	Engine->SetMessageCallback(asFUNCTION(LogAngelscriptError), 0, asCALL_CDECL);
	Engine->SetContextCallbacks(&AngelscriptRequestContext, &AngelscriptReturnContext, nullptr);

	// Construct the engine's owned databases as direct TUniquePtr<...> fields
	// on FAngelscriptEngine.
	if (!TypeDatabase.IsValid())
	{
		TypeDatabase = MakeUnique<FAngelscriptTypeDatabase>();
	}
	if (!BindState.IsValid())
	{
		BindState = MakeUnique<FAngelscriptBindState>();
	}
	if (!ToStringList.IsValid())
	{
		ToStringList = MakeUnique<TArray<FToStringType>>();
	}
	if (!BindDatabase.IsValid())
	{
		LLM_SCOPE_BYTAG(Angelscript);
		BindDatabase = MakeUnique<FAngelscriptBindDatabase>();
	}
	if (!BlueprintEventSignatureRegistry.IsValid())
	{
		BlueprintEventSignatureRegistry = MakeUnique<FBlueprintEventSignatureRegistry>();
	}
	if (!DocumentationState.IsValid())
	{
		DocumentationState = MakeUnique<FAngelscriptDocumentationState>();
	}
#if AS_CAN_GENERATE_JIT
	if (!NativeFormState.IsValid())
	{
		NativeFormState = MakeUnique<FAngelscriptNativeFormState>();
	}
#endif

	{
		FAngelscriptEngineScope ScopedTestingEngine(*this);
		if (!BindScriptTypes())
		{
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::RecordPublicationResult(this, false);
			#endif
			return false;
		}
	}
	GameThreadTLD->primaryContext = CreateContext();
	bIsInitialCompileFinished = true;

#if WITH_AS_DEBUGSERVER
	if (RuntimeConfig.DebugServerPort > 0)
	{
		DebugServer = new FAngelscriptDebugServer(this, RuntimeConfig.DebugServerPort);
	}
#endif

#if WITH_AS_COVERAGE
	FAngelscriptCodeCoverageExtension::EnsureAttached(*this);
#endif

	bReadyForPublication = true;
	check(CacheService.IsValid());
	CacheService->TransitionToRuntimeGameThread();
	FAngelscriptEngineExtensionRegistry::Get().AttachEngine(*this);
	bExtensionsAttached = true;
	#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptBindExecutionObservation::RecordPublicationResult(this, true);
	#endif
	return true;
}

void FAngelscriptEngine::AcquireProcessPackages()
{
	check(IsInGameThread());

	if (bHoldsProcessPackageReference)
	{
		return;
	}

	using namespace AngelscriptEnginePackages_Private;

	AngelscriptPackage = CreatePackage(ScriptPackageName);
	check(AngelscriptPackage != nullptr);
	AngelscriptPackage->SetFlags(RF_Public | RF_Standalone);
	AngelscriptPackage->SetPackageFlags(PKG_CompiledIn);
	if (GAngelscriptPackageRefCount++ == 0 && !AngelscriptPackage->IsRooted())
	{
		AngelscriptPackage->AddToRoot();
	}

	AssetsPackage = CreatePackage(AssetsPackageName);
	check(AssetsPackage != nullptr);
	AssetsPackage->SetFlags(RF_Public | RF_Standalone);
	AssetsPackage->SetPackageFlags(PKG_CompiledIn);
	if (GAngelscriptAssetsPackageRefCount++ == 0 && !AssetsPackage->IsRooted())
	{
		AssetsPackage->AddToRoot();
	}

	bHoldsProcessPackageReference = true;
}

void FAngelscriptEngine::ReleaseProcessPackages()
{
	if (!bHoldsProcessPackageReference)
	{
		AngelscriptPackage = nullptr;
		AssetsPackage = nullptr;
		return;
	}

	if (AngelscriptPackage != nullptr && GAngelscriptPackageRefCount > 0 && --GAngelscriptPackageRefCount == 0)
	{
		if (AngelscriptPackage->IsValidLowLevelFast() && AngelscriptPackage->IsRooted())
		{
			AngelscriptPackage->RemoveFromRoot();
		}
		if (AngelscriptPackage->IsValidLowLevelFast())
		{
			AngelscriptPackage->ClearFlags(RF_Standalone);
		}
	}

	if (AssetsPackage != nullptr && GAngelscriptAssetsPackageRefCount > 0 && --GAngelscriptAssetsPackageRefCount == 0)
	{
		if (AssetsPackage->IsValidLowLevelFast() && AssetsPackage->IsRooted())
		{
			AssetsPackage->RemoveFromRoot();
		}
		if (AssetsPackage->IsValidLowLevelFast())
		{
			AssetsPackage->ClearFlags(RF_Standalone);
		}
	}

	bHoldsProcessPackageReference = false;
	AngelscriptPackage = nullptr;
	AssetsPackage = nullptr;
}

FAngelscriptTypeDatabase* FAngelscriptEngine::GetTypeDatabase() const
{
	// TUniquePtr<>::Get() returns nullptr when empty, preserving the
	// pre-flatten "nullptr before Initialize*()" contract without needing
	// an extra IsValid() guard.
	return TypeDatabase.Get();
}

FAngelscriptBindState* FAngelscriptEngine::GetBindState() const
{
	return BindState.Get();
}

FBlueprintEventSignatureRegistry* FAngelscriptEngine::GetBlueprintEventSignatureRegistry() const
{
	return BlueprintEventSignatureRegistry.Get();
}

FAngelscriptDocumentationState* FAngelscriptEngine::GetDocumentationState() const
{
	return DocumentationState.Get();
}

#if AS_CAN_GENERATE_JIT
FAngelscriptNativeFormState* FAngelscriptEngine::GetNativeFormState() const
{
	return NativeFormState.Get();
}
#endif

TArray<FToStringType>* FAngelscriptEngine::GetToStringList() const
{
	return ToStringList.Get();
}

FAngelscriptBindDatabase* FAngelscriptEngine::GetBindDatabase() const
{
	return BindDatabase.Get();
}

#if WITH_DEV_AUTOMATION_TESTS
int32 FAngelscriptEngine::GetLocalPooledContextCountForTesting(asIScriptEngine* ScriptEngine)
{
	int32 MatchCount = 0;
	for (asCContext* Context : GAngelscriptContextPool.FreeContexts)
	{
		if (Context != nullptr && (ScriptEngine == nullptr || Context->GetEngine() == ScriptEngine))
		{
			++MatchCount;
		}
	}

	return MatchCount;
}

int32 FAngelscriptEngine::GetToStringEntryCountForTesting() const
{
	if (TArray<FToStringType>* List = GetToStringList())
	{
		return List->Num();
	}
	return 0;
}

FAngelscriptBindDatabase& FAngelscriptEngine::GetBindDatabaseForTesting() const
{
	check(BindDatabase.IsValid());
	return *BindDatabase;
}

void FAngelscriptEngine::SetUseEditorScriptsForTesting(bool bEnabled)
{
	bUseEditorScripts = bEnabled;
}

void FAngelscriptEngine::SetAutomaticImportMethodForTesting(bool bEnabled)
{
	bUseAutomaticImportMethod = bEnabled;
}
#endif

const FName& FAngelscriptEngine::GetStaticName(int32 Index)
{
	return GetStaticNames()[Index];
}

FName FAngelscriptEngine::ResolveStaticName(
	const int32 Index,
	const FString& CanonicalName)
{
	const FName ExpectedName(*CanonicalName);
	const TArray<FName>& Names = GetStaticNames();
	if (Names.IsValidIndex(Index) && Names[Index] == ExpectedName)
	{
		return Names[Index];
	}

	// Cache V2 may execute this bytecode in a fresh Engine whose static-name
	// registration order differs from the producer. The index is only a hot-path
	// hint; canonical text is the cross-Engine authority and needs no table write.
	return ExpectedName;
}

bool FAngelscriptEngine::TryGetStaticName(int32 Index, FName& OutName)
{
	const TArray<FName>& Names = GetStaticNames();
	if (!Names.IsValidIndex(Index))
	{
		return false;
	}

	OutName = Names[Index];
	return true;
}

int32 FAngelscriptEngine::GetOrAddStaticName(FName Name)
{
	TArray<FName>* Names = &GLegacyStaticNames;
	TMap<FName, int32>* NamesByIndex = &GLegacyStaticNamesByIndex;
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		Names = &CurrentEngine->StaticNames;
		NamesByIndex = &CurrentEngine->StaticNamesByIndex;
	}

	if (int32* FoundIndex = NamesByIndex->Find(Name))
	{
		return *FoundIndex;
	}

	const int32 Index = Names->Emplace(Name);
	NamesByIndex->Add(Name, Index);
	return Index;
}

int32 FAngelscriptEngine::GetStaticNameCount()
{
	return GetStaticNames().Num();
}

void FAngelscriptEngine::ReserveStaticNames(int32 Count)
{
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		CurrentEngine->StaticNames.Reserve(Count);
		CurrentEngine->StaticNamesByIndex.Reserve(Count);
		return;
	}

	GLegacyStaticNames.Reserve(Count);
	GLegacyStaticNamesByIndex.Reserve(Count);
}

void FAngelscriptEngine::ResetStaticNames()
{
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		CurrentEngine->StaticNames.Reset();
		CurrentEngine->StaticNamesByIndex.Reset();
		return;
	}

	GLegacyStaticNames.Reset();
	GLegacyStaticNamesByIndex.Reset();
}

void FAngelscriptEngine::AddStaticNameFromPrecompiled(FName Name)
{
	TArray<FName>* Names = &GLegacyStaticNames;
	TMap<FName, int32>* NamesByIndex = &GLegacyStaticNamesByIndex;
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		Names = &CurrentEngine->StaticNames;
		NamesByIndex = &CurrentEngine->StaticNamesByIndex;
	}

	const int32 Index = Names->Add(Name);
	NamesByIndex->Add(Name, Index);
}

const TArray<FName>& FAngelscriptEngine::GetStaticNames()
{
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		return CurrentEngine->StaticNames;
	}

	return GLegacyStaticNames;
}

bool FAngelscriptEngine::DiscardModule(const TCHAR* ModuleName)
{
	if (Engine == nullptr)
		return false;

	asIScriptEngine* ScriptEngine = Engine;
	ReleaseContextsForScriptEngine(GAngelscriptContextPool.FreeContexts, ScriptEngine);
	{
		FScopeLock Lock(&GlobalContextPoolLock);
		ReleaseContextsForScriptEngine(GlobalContextPool, ScriptEngine);
	}

	const FString InternalModuleName = MakeModuleName(ModuleName);
	TSharedPtr<FAngelscriptModuleDesc> ModuleToDiscard = GetModule(ModuleName);
	asCModule* ScriptModuleToDiscard = ModuleToDiscard.IsValid() && ModuleToDiscard->ScriptModule != nullptr
		? static_cast<asCModule*>(ModuleToDiscard->ScriptModule)
		: nullptr;
	auto AnsiName = StringCast<ANSICHAR>(*InternalModuleName);
	if (ScriptModuleToDiscard == nullptr || Engine->GetModule(AnsiName.Get(), false) != ScriptModuleToDiscard)
	{
		return false;
	}

	ScriptModuleToDiscard->RemoveTypesAndGlobalsFromEngineAvailability();
	int r = Engine->DiscardModule(AnsiName.Get());
	if (r < 0)
		return false;

	if (ModuleToDiscard.IsValid())
	{
		if (ModuleToDiscard->ScriptModule != nullptr)
		{
			ModulesByScriptModule.Remove(ModuleToDiscard->ScriptModule);
		}

		for (const TSharedRef<FAngelscriptClassDesc>& Class : ModuleToDiscard->Classes)
		{
			if (UASClass* ScriptClass = Cast<UASClass>(Class->Class))
			{
				ScriptClass->ScriptTypePtr = nullptr;
				ScriptClass->OwnerScriptEngine = nullptr;
				ScriptClass->ConstructFunction = nullptr;
				ScriptClass->DefaultsFunction = nullptr;

				for (TFieldIterator<UFunction> FunctionIt(ScriptClass, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
				{
					if (UASFunction* ScriptFunction = Cast<UASFunction>(*FunctionIt))
					{
						ScriptFunction->ScriptFunction = nullptr;
						ScriptFunction->ValidateFunction = nullptr;
					}
				}
			}

			if (UASStruct* ScriptStruct = Cast<UASStruct>(Class->Struct))
			{
				ScriptStruct->ScriptType = nullptr;
				ScriptStruct->UpdateScriptType();
			}

			ActiveClassesByName.Remove(Class->ClassName);
		}

		for (const TSharedRef<FAngelscriptEnumDesc>& Enum : ModuleToDiscard->Enums)
		{
			ActiveEnumsByName.Remove(Enum->EnumName);
		}

		for (const TSharedRef<FAngelscriptDelegateDesc>& Delegate : ModuleToDiscard->Delegates)
		{
			ActiveDelegatesByName.Remove(Delegate->DelegateName);
		}

		for (const FAngelscriptModuleDesc::FCodeSection& Section : ModuleToDiscard->Code)
		{
			const FFilenamePair FilenamePair{ Section.AbsoluteFilename, Section.RelativeFilename, Section.VirtualPath };
			FileHotReloadState.Remove(MakeSourceStateKey(FilenamePair));
			if (!Section.RelativeFilename.IsEmpty())
			{
				FileHotReloadState.Remove(Section.RelativeFilename);
			}
			PreviouslyFailedReloadFiles.Remove(FilenamePair);
			QueuedFullReloadFiles.Remove(FilenamePair);
			Diagnostics.Remove(Section.AbsoluteFilename);
			LastEmittedDiagnostics.Remove(Section.AbsoluteFilename);
		}

		FileChangesDetectedForReload.RemoveAll([&ModuleToDiscard](const FFilenamePair& FilenamePair)
		{
			for (const FAngelscriptModuleDesc::FCodeSection& Section : ModuleToDiscard->Code)
			{
				if (FilenamePair.AbsolutePath == Section.AbsoluteFilename && FilenamePair.RelativePath == Section.RelativeFilename)
				{
					return true;
				}
			}

			return false;
		});

		FileDeletionsDetectedForReload.RemoveAll([&ModuleToDiscard](const FFilenamePair& FilenamePair)
		{
			for (const FAngelscriptModuleDesc::FCodeSection& Section : ModuleToDiscard->Code)
			{
				if (FilenamePair.AbsolutePath == Section.AbsoluteFilename && FilenamePair.RelativePath == Section.RelativeFilename)
				{
					return true;
				}
			}

			return false;
		});
	}

	//[UE++]: Remove module record from ActiveModules so GetModuleByModuleName returns null after discard
	ActiveModules.Remove(InternalModuleName);
	RebuildFunctionRouteSnapshot();
	//[UE--]
	return true;
}

// Set once an owned engine is released during process exit. Game-thread only; the
// exit purge that reads it is single-threaded, so a plain bool is sufficient.
static bool GAngelscriptEnginesReleasedForExit = false;

#if WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS
void GAngelscriptNativeModuleFunctionBindingUnregisterEngine(FAngelscriptEngine& Engine);
#endif

bool FAngelscriptEngine::AreEnginesReleasedForExit()
{
	return GAngelscriptEnginesReleasedForExit;
}

#if WITH_DEV_AUTOMATION_TESTS
void FAngelscriptEngine::EnsureScriptTestHotReloadRunnerForTesting()
{
	if (ScriptTestHotReloadRunner == nullptr)
	{
		ScriptTestHotReloadRunner =
			new FAngelscriptScriptTestHotReloadRunner();
	}
}
#endif

void FAngelscriptEngine::WriteRequestedCacheV2ProcessReport() const
{
	if (Engine == nullptr
		|| RuntimeConfig.CacheV2ReportPathOverride.IsEmpty())
	{
		return;
	}

	const FAngelscriptCacheReportWriteResult Report =
		WriteAngelscriptCacheDiagnosticJsonReport(
			this, RuntimeConfig.CacheV2ReportPathOverride);
	if (Report.IsSuccess())
	{
		UE_LOG(Angelscript, Display,
			TEXT("[CacheV2][ProcessReport] %s"),
			*Report.Detail);
	}
	else
	{
		UE_LOG(Angelscript, Error,
			TEXT("[CacheV2][ProcessReport] Error=%u Detail=%s"),
			static_cast<uint32>(Report.Error),
			*Report.Detail);
	}
}

void FAngelscriptEngine::Shutdown()
{
	bReadyForPublication = false;
	const bool bHadAttachedExtensions = bExtensionsAttached;
	bExtensionsAttached = false;
	const bool bHadInitializedEngine = Engine != nullptr;

	if (bPackagedRuntimeReloadQueued)
	{
		bPackagedRuntimeReloadQueued = false;
		FAngelscriptRuntimeReloadResult Cancelled;
		Cancelled.Outcome = EAngelscriptRuntimeReloadOutcome::Cancelled;
		Cancelled.Diagnostics =
			TEXT("The queued Runtime reload was cancelled by Engine shutdown.");
		CompletedPackagedRuntimeReloadResult.Emplace(MoveTemp(Cancelled));
	}

	if (CacheService.IsValid())
	{
		if (!bCacheV2ShutdownFlushAttempted)
		{
			bCacheV2ShutdownFlushAttempted = true;
			const UAngelscriptCacheSettings* CacheSettings =
				GetDefault<UAngelscriptCacheSettings>();
			if (bHadInitializedEngine
				&& CacheSettings != nullptr
				&& CacheSettings->bEnableCacheV2
				&& !RuntimeConfig.bDisableCacheV2Persistence)
			{
				FString RequestedBaseRoot;
				const FAngelscriptCacheStoreResult RootSelection =
					ResolveAngelscriptCacheRequestedBaseRootForEngine(
						*this, RequestedBaseRoot);
				if (RootSelection.IsSuccess())
				{
					const FAngelscriptCacheLifecycleFlushResult Flush =
						CacheService->BeginEngineShutdownAndFlushToStore(
							RequestedBaseRoot,
							CacheSettings->ShutdownFlushTimeoutSeconds);
					if (Flush.IsSuccess())
					{
						UE_LOG(Angelscript, Display,
							TEXT("[CacheV2] Engine shutdown flush: Error=%u CurrentCommit=%u PendingCommit=%u Detail=%s"),
							static_cast<uint32>(Flush.Error),
							static_cast<uint32>(
								Flush.Current.Publication.CommitState),
							static_cast<uint32>(
								Flush.PendingColdStart.Publication.CommitState),
							*Flush.Detail);
					}
					else
					{
						UE_LOG(Angelscript, Warning,
							TEXT("[CacheV2] Engine shutdown flush: Error=%u CurrentCommit=%u PendingCommit=%u Detail=%s"),
							static_cast<uint32>(Flush.Error),
							static_cast<uint32>(
								Flush.Current.Publication.CommitState),
							static_cast<uint32>(
								Flush.PendingColdStart.Publication.CommitState),
							*Flush.Detail);
					}
				}
				else
				{
					CacheService->BeginEngineShutdown();
					UE_LOG(Angelscript, Warning,
						TEXT("[CacheV2] Engine shutdown cache root selection failed: Error=%u Stage=%u PathCategory=%u"),
						static_cast<uint32>(RootSelection.Error),
						static_cast<uint32>(RootSelection.Stage),
						static_cast<uint32>(RootSelection.PathCategory));
				}
			}
			else
			{
				CacheService->BeginEngineShutdown();
			}

			if (bHadInitializedEngine)
			{
				WriteRequestedCacheV2ProcessReport();
			}
		}
		else
		{
			CacheService->BeginEngineShutdown();
		}
	}
	const bool bShouldReleaseOwnedEngine = Engine != nullptr;

	UE_LOG(Angelscript, Verbose, TEXT("[EngineLifecycle] Shutdown engine=%p hadEngine=%s willRelease=%s"),
		this,
		bHadInitializedEngine ? TEXT("true") : TEXT("false"),
		bShouldReleaseOwnedEngine ? TEXT("true") : TEXT("false"));

#if WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS
	// The native-module bridge keeps explicit engine targets so late feature
	// arrival and unload can update each engine-owned binding table. Detach this
	// target before BindState is reset; queued replay then observes an invalidated
	// target record instead of retaining a raw engine pointer past Shutdown().
	GAngelscriptNativeModuleFunctionBindingUnregisterEngine(*this);
#endif

	if (bHadInitializedEngine && IsInGameThread())
	{
		// Active reflected tests retain script functions, suite instances, and
		// callbacks owned by this engine. Cancel the leaves and close the
		// suite-level Automation session before any AS-owned state is detached
		// or released.
		FAngelscriptScriptTestRunner::CancelEngine(
			this,
			TEXT("The owning AngelScript engine is shutting down."));
#if WITH_DEV_AUTOMATION_TESTS
		FAngelscriptScriptTestAutomation::Get()
			.CancelEngineBeforeShutdown(this);
#endif
	}

	// The automatic hot-reload scheduler owns a separate All-hook session.
	// Destroy it while engine extensions and the old script generation are
	// still usable so its destructor can run AfterAll safely.
	if (ScriptTestHotReloadRunner != nullptr)
	{
		delete ScriptTestHotReloadRunner;
		ScriptTestHotReloadRunner = nullptr;
	}

	if (bHadAttachedExtensions)
	{
		FAngelscriptEngineExtensionRegistry::Get().DetachEngine(*this);
	}

#if WITH_AS_COVERAGE
	// Full initialization can attach coverage before direct binding completes.
	// Clean that one extension precisely when initialization fails before the
	// registry lifecycle begins; never broadcast a detach to unrelated extensions.
	FAngelscriptCodeCoverageExtension::EnsureDetached(*this);
#endif

	// Single-owner releases: the engine releases its own fields directly.
#if WITH_AS_DEBUGSERVER
	if (bShouldReleaseOwnedEngine && DebugServer != nullptr)
	{
		delete DebugServer;
		DebugServer = nullptr;
	}
#endif

	if (bShouldReleaseOwnedEngine && StaticJIT != nullptr)
	{
		delete StaticJIT;
		StaticJIT = nullptr;
	}

	if (bShouldReleaseOwnedEngine && PrecompiledData != nullptr)
	{
		delete PrecompiledData;
		PrecompiledData = nullptr;
	}

	if (bShouldReleaseOwnedEngine && GameThreadTLD != nullptr && GameThreadTLD->primaryContext != nullptr)
	{
		GameThreadTLD->primaryContext->Release();
		GameThreadTLD->primaryContext = nullptr;
	}

	if (bShouldReleaseOwnedEngine && Engine != nullptr)
	{
		ReleaseContextsForScriptEngine(GAngelscriptContextPool.FreeContexts, Engine);
	}

	for (asCContext* Context : GlobalContextPool)
	{
		if (Context != nullptr)
		{
			Context->Release();
		}
	}
	GlobalContextPool.Empty();
	InterfaceMethodSignatures.Empty();

	if (bShouldReleaseOwnedEngine && Engine != nullptr)
	{
		for (UClass* ClassObj : TObjectRange<UClass>())
		{
			UASClass* ASClass = Cast<UASClass>(ClassObj);
			if (ASClass == nullptr || ASClass->OwnerScriptEngine != Engine)
				continue;
			ASClass->ScriptTypePtr = nullptr;
			ASClass->OwnerScriptEngine = nullptr;
			ASClass->ConstructFunction = nullptr;
			ASClass->DefaultsFunction = nullptr;

			for (TFieldIterator<UFunction> FunctionIt(ASClass, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
			{
				if (UASFunction* ScriptFunction = Cast<UASFunction>(*FunctionIt))
				{
					ScriptFunction->ScriptFunction = nullptr;
					ScriptFunction->ValidateFunction = nullptr;
				}
			}

			if (ASClass->IsRooted())
			{
				ASClass->RemoveFromRoot();
			}
			ASClass->ClearFlags(RF_Standalone);
		}
	}

	if (bShouldReleaseOwnedEngine && AngelscriptPackage != nullptr)
	{
		ForEachObjectWithPackage(AngelscriptPackage, [](UObject* Obj)
		{
			if (UASStruct* Struct = Cast<UASStruct>(Obj))
			{
				Struct->ScriptType = nullptr;
				Struct->UpdateScriptType();
				if (Struct->IsRooted())
				{
					Struct->RemoveFromRoot();
				}
				Struct->ClearFlags(RF_Standalone);
			}
			else if (UDelegateFunction* DelegateFunc = Cast<UDelegateFunction>(Obj))
			{
				if (DelegateFunc->IsRooted())
				{
					DelegateFunc->RemoveFromRoot();
				}
				DelegateFunc->ClearFlags(RF_Standalone);
			}
			else if (UUserDefinedEnum* ScriptEnum = Cast<UUserDefinedEnum>(Obj))
			{
				if (ScriptEnum->IsRooted())
				{
					ScriptEnum->RemoveFromRoot();
				}
				ScriptEnum->ClearFlags(RF_Standalone);
			}
			return true;
		}, false);
	}

	// Engine teardown: the engine releases its own fields directly here.
	if (bShouldReleaseOwnedEngine && Engine != nullptr)
	{
		// If we are releasing during process exit, the UObject system may still purge
		// script-backed structs after this point (PurgeAllUObjectsOnExit). Mark engines
		// as released so UASStruct destruction skips running script destructors against
		// the now-freed engine instead of crashing.
		if (IsEngineExitRequested())
		{
			GAngelscriptEnginesReleasedForExit = true;
		}

		FAngelscriptStaticTypeInfoRegistry::ClearForEngine(Engine);
		// Drop script-enum -> asITypeInfo* entries before AS engine release so
		// any late access in the teardown window cannot read a dangling pointer.
		ScriptEnumTypeLookupByName.Reset();
		Engine->ShutDownAndRelease();
	}
	Engine = nullptr;
	StaticJIT = nullptr;
	PrecompiledData = nullptr;
#if WITH_AS_DEBUGSERVER
	DebugServer = nullptr;
#endif

	// Clear the engine-owned type / bind / registry databases.
	// ScriptEngine->ShutDownAndRelease() above
	// has already destroyed every script function that held a userData pointer
	// to a heap-allocated FBlueprintEventSignature, so the registry can be
	// safely cleared here. Optional extension-owned process caches are not
	// owned by the core runtime shutdown path.
	if (bShouldReleaseOwnedEngine)
	{
		TypeDatabase.Reset();
		BindState.Reset();
		ToStringList.Reset();
		BindDatabase.Reset();
		StaticNames.Reset();
		StaticNamesByIndex.Reset();

		{
			extern TMap<UClass*, TMap<FString, UFunction*>> GBlueprintEventsByScriptName;
			GBlueprintEventsByScriptName.Empty();
		}
		BlueprintEventSignatureRegistry.Reset();
		DocumentationState.Reset();
#if AS_CAN_GENERATE_JIT
		NativeFormState.Reset();
#endif

#if WITH_EDITOR
		{
			extern void ResetCachedEditorClasses();
			ResetCachedEditorClasses();
		}
#endif

	}

	ActiveModules.Empty();
	ModulesByScriptModule.Empty();
	AllRootPaths.Empty();
	QueuedFullReloadFiles.Empty();
	PreviouslyFailedReloadFiles.Empty();

	if (bShouldReleaseOwnedEngine && bHadInitializedEngine)
	{
		SyncAmbientWorldContextFromCurrentEngine();
	}

	// Release process-wide AS packages via the refcount-protected helper.
	// Phase 4 of clone-removal eliminated SourceEngine / SourceLifetimeToken
	// / SharedState fields, so the original Clone-aware reset block is gone.
	if (bShouldReleaseOwnedEngine)
	{
		ReleaseProcessPackages();
	}
	AngelscriptPackage = nullptr;
	AssetsPackage = nullptr;
	LifetimeToken.Reset();
	WorldContextObject = nullptr;
	CacheService.Reset();
}

FInterfaceMethodSignature* FAngelscriptEngine::RegisterInterfaceMethodSignature(FName FunctionName)
{
	TUniquePtr<FInterfaceMethodSignature> Signature = MakeUnique<FInterfaceMethodSignature>();
	Signature->FunctionName = FunctionName;
	FInterfaceMethodSignature* RawSignature = Signature.Get();
	InterfaceMethodSignatures.Add(MoveTemp(Signature));
	return RawSignature;
}

void FAngelscriptEngine::ReleaseInterfaceMethodSignature(FInterfaceMethodSignature* Signature)
{
	if (Signature == nullptr)
	{
		return;
	}

	for (int32 Index = 0; Index < InterfaceMethodSignatures.Num(); ++Index)
	{
		if (InterfaceMethodSignatures[Index].Get() == Signature)
		{
			InterfaceMethodSignatures.RemoveAt(Index);
			return;
		}
	}
}

void FAngelscriptEngine::PreInitialize_GameThread()
{
	/**
	 * Tell angelscript to use the appropriate allocators.
	 */
	asSetAllocScriptObjectFunction(&UASClass::AllocScriptObject, &UASClass::FinishConstructObject);

	// A new full engine starts a fresh script-engine epoch. Dropping thread-local free
	// contexts here prevents later engine allocations from aliasing stale pooled entries.
	ReleaseAllContextsInPool(GAngelscriptContextPool.FreeContexts);

	ConfigSettings = GetMutableDefault<UAngelscriptSettings>();
	bUseAutomaticImportMethod = ConfigSettings->bAutomaticImports;

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
	// In editor, we need to be able to resolve object pointers to make
	// unreal's access tracking and lazy resolving work.
	asSetResolveObjectPtrFunction(&AngelscriptResolveObjectPtr);
#endif

	/**
	 * Set up angelscript engine, used to bind c++ functions
	 * and compile script modules.
	 */
	Engine = (asCScriptEngine*)asCreateScriptEngine(ANGELSCRIPT_VERSION);

	// Set up thread local data for game thread
	GameThreadTLD = asCThreadManager::GetLocalData();

	AcquireProcessPackages();
}

TArray<FString> FAngelscriptEngine::DiscoverScriptRoots(bool bOnlyProjectRoot) const
{
	TArray<FString> RootPaths;
	for (const FAngelscriptSourceRoot& ScriptRoot : DiscoverScriptRootDescriptors(bOnlyProjectRoot))
	{
		RootPaths.Add(ScriptRoot.AbsolutePath);
	}
	return RootPaths;
}

namespace AngelscriptEngineScriptRoots_Private
{
	FString NormalizeScriptRootPathForCompare(const FString& InPath)
	{
		FString Path = FPaths::ConvertRelativePathToFull(InPath);
		FPaths::NormalizeDirectoryName(Path);
		return Path;
	}

	bool AreScriptRootDescriptorsInSyncWithRootPaths(
		const TArray<FAngelscriptSourceRoot>& ScriptRoots,
		const TArray<FString>& RootPaths)
	{
		if (ScriptRoots.Num() != RootPaths.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < ScriptRoots.Num(); ++Index)
		{
			if (NormalizeScriptRootPathForCompare(ScriptRoots[Index].AbsolutePath)
				!= NormalizeScriptRootPathForCompare(RootPaths[Index]))
			{
				return false;
			}
		}

		return true;
	}

	TArray<FAngelscriptSourceRoot> MakeGameRootDescriptorsFromRootPaths(const TArray<FString>& RootPaths)
	{
		TArray<FAngelscriptSourceRoot> Roots;
		Roots.Reserve(RootPaths.Num());
		for (const FString& Path : RootPaths)
		{
			Roots.Add(FAngelscriptSourceRoot::FromGameRoot(Path));
		}
		return Roots;
	}
}

TArray<FAngelscriptSourceRoot> FAngelscriptEngine::DiscoverScriptRootDescriptors(bool bOnlyProjectRoot) const
{
	check(Dependencies.GetProjectDir);
	check(Dependencies.ConvertRelativePathToFull);
	check(Dependencies.DirectoryExists);
	check(Dependencies.MakeDirectory);
	check(Dependencies.GetEnabledPluginScriptRoots || Dependencies.GetEnabledPluginScriptRootDescriptors);

	FString RootPath = Dependencies.ConvertRelativePathToFull(Dependencies.GetProjectDir() / TEXT("Script"));

	// Create the script root folder if it doesn't exist
	if (RuntimeConfig.bIsEditor && !Dependencies.DirectoryExists(RootPath))
	{
		Dependencies.MakeDirectory(RootPath, true);
	}

	// Find all plugin script roots
	TArray<FAngelscriptSourceRoot> DiscoveredRootPaths;

	if (!bOnlyProjectRoot)
	{
		if (Dependencies.GetEnabledPluginScriptRootDescriptors)
		{
			for (const FAngelscriptPluginScriptRoot& PluginScriptRoot : Dependencies.GetEnabledPluginScriptRootDescriptors())
			{
				const FString ScriptPath = Dependencies.ConvertRelativePathToFull(PluginScriptRoot.ScriptRoot);
				if (Dependencies.DirectoryExists(ScriptPath) && ScriptPath != RootPath)
				{
					DiscoveredRootPaths.Add(FAngelscriptSourceRoot::FromPluginRoot(PluginScriptRoot.PluginName, ScriptPath));
				}
			}
		}
		else
		{
			for (const FString& PluginScriptRoot : Dependencies.GetEnabledPluginScriptRoots())
			{
				const FString ScriptPath = Dependencies.ConvertRelativePathToFull(PluginScriptRoot);
				if (Dependencies.DirectoryExists(ScriptPath) && ScriptPath != RootPath)
				{
					DiscoveredRootPaths.Add(FAngelscriptSourceRoot::FromGameRoot(ScriptPath));
				}
			}
		}

		// Make the search order somewhat deterministic
		DiscoveredRootPaths.Sort([](const FAngelscriptSourceRoot& A, const FAngelscriptSourceRoot& B)
		{
			return A.AbsolutePath < B.AbsolutePath;
		});
	}

	// Inject the project root first in the list so GetModuleByFilename looks there first.
	DiscoveredRootPaths.Insert(FAngelscriptSourceRoot::FromGameRoot(RootPath), 0);

	return DiscoveredRootPaths;
}

TArray<FAngelscriptSourceRoot> FAngelscriptEngine::GetEffectiveScriptRootDescriptors() const
{
	using namespace AngelscriptEngineScriptRoots_Private;

	if (AllScriptRoots.Num() != 0)
	{
		if (AllRootPaths.Num() == 0 || AreScriptRootDescriptorsInSyncWithRootPaths(AllScriptRoots, AllRootPaths))
		{
			return AllScriptRoots;
		}
	}

	return MakeGameRootDescriptorsFromRootPaths(AllRootPaths);
}

TArray<FString> FAngelscriptEngine::MakeAllScriptRoots(bool bOnlyProjectRoot)
{
	FAngelscriptEngine TemporaryEngine;
	return TemporaryEngine.DiscoverScriptRoots(bOnlyProjectRoot);
}

bool FAngelscriptEngine::Initialize_AnyThread()
{
	bSimulateCooked = RuntimeConfig.bSimulateCooked;
	bTestErrors = RuntimeConfig.bTestErrors;
	bForcePreprocessEditorCode = RuntimeConfig.bForcePreprocessEditorCode;
	bUseEditorScripts = WITH_EDITOR
		&& ((RuntimeConfig.bIsEditor && !RuntimeConfig.bRunningCommandlet) || bForcePreprocessEditorCode)
		&& !bSimulateCooked;

	Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);
	Engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, 1);
	Engine->SetEngineProperty(asEP_ALLOW_MULTILINE_STRINGS, 1);
	Engine->SetEngineProperty(asEP_SCRIPT_SCANNER, 1);

	Engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 1);

	Engine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, 0);
	Engine->SetEngineProperty(asEP_ALTER_SYNTAX_NAMED_ARGS, 1);

	Engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, 1);
	Engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, 1);
	Engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, 1);
	Engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, 1);
	Engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY, 1);
	Engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT, 1);
	Engine->SetEngineProperty(asEP_MEMBER_INIT_MODE, 0);

	Engine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 0);

	Engine->SetEngineProperty(asEP_TYPECHECK_SWITCH_ENUMS, 1);

	Engine->SetEngineProperty(asEP_FLOAT_IS_FLOAT64, ConfigSettings->bScriptFloatIsFloat64 ? 1 : 0);
	Engine->SetEngineProperty(asEP_ALLOW_DOUBLE_TYPE, ConfigSettings->bDeprecateDoubleType ? 0 : 1);
	Engine->SetEngineProperty(asEP_WARN_ON_FLOAT_CONSTANTS_FOR_DOUBLES, ConfigSettings->bWarnOnFloatConstantsForDoubleValues ? 1 : 0);
	Engine->SetEngineProperty(asEP_WARN_INTEGER_DIVISION, ConfigSettings->bWarnIntegerDivision ? 1 : 0);

	if (ShouldUseAutomaticImportMethod())
		Engine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 1);

#if !WITH_AS_DEBUGSERVER && !WITH_AS_DEBUGVALUES
	Engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, 1);
#endif

	Engine->SetMessageCallback(asFUNCTION(LogAngelscriptError), 0, asCALL_CDECL);
	Engine->SetContextCallbacks(&AngelscriptRequestContext, &AngelscriptReturnContext, nullptr);

	bCollectStaticJITCompatibilityBinds =
		RuntimeConfig.bCollectStaticJITCompatibilityBinds;
	bScriptDevelopmentMode = RuntimeConfig.bIsEditor || RuntimeConfig.bDevelopmentMode;
	bUseStaticJITCompatibilityData = false;

	// Wait with the plugin script roots until we know we need them
	AllScriptRoots = DiscoverScriptRootDescriptors(/*bOnlyProjectRoot =*/ true);
	AllRootPaths.Reset(AllScriptRoots.Num());
	for (const FAngelscriptSourceRoot& ScriptRoot : AllScriptRoots)
	{
		AllRootPaths.Add(ScriptRoot.AbsolutePath);
	}

	/*
	Start the debug server that external tools can connect to.
	*/
#if WITH_AS_DEBUGSERVER
	if (FApp::HasProjectName())
	{
		DebugServer = new FAngelscriptDebugServer(this, RuntimeConfig.DebugServerPort);
	}
#endif

#if WITH_AS_COVERAGE
	FAngelscriptCodeCoverageExtension::EnsureAttached(*this);
#endif

	// The bind database must exist on this engine before Binds.Cache is loaded. The lifecycle
	// mutates this owned instance directly so initialization never depends on ambient engine
	// resolution or accidentally writes a process fallback database.
	if (!BindDatabase.IsValid())
	{
		LLM_SCOPE_BYTAG(Angelscript);
		BindDatabase = MakeUnique<FAngelscriptBindDatabase>();
	}

#if AS_USE_BIND_DB
	{
		AS_PERF_SCOPE_STARTUP_BIND_DATABASE();
		FAngelscriptScopeTimer Timer(TEXT("load bind database"));
		BindDatabase->Load(
			GetScriptRootDirectory() / TEXT("Binds.Cache"),
			bCollectStaticJITCompatibilityBinds);
	}
#endif	
	// Construct the engine's owned databases. See Initialize() /
	// InitializeWithoutInitialCompile() for the matching short path.
	if (!TypeDatabase.IsValid())
	{
		TypeDatabase = MakeUnique<FAngelscriptTypeDatabase>();
	}
	if (!BindState.IsValid())
	{
		BindState = MakeUnique<FAngelscriptBindState>();
	}
	if (!ToStringList.IsValid())
	{
		ToStringList = MakeUnique<TArray<FToStringType>>();
	}
	if (!BindDatabase.IsValid())
	{
		LLM_SCOPE_BYTAG(Angelscript);
		BindDatabase = MakeUnique<FAngelscriptBindDatabase>();
	}
	if (!BlueprintEventSignatureRegistry.IsValid())
	{
		BlueprintEventSignatureRegistry = MakeUnique<FBlueprintEventSignatureRegistry>();
	}
	if (!DocumentationState.IsValid())
	{
		DocumentationState = MakeUnique<FAngelscriptDocumentationState>();
	}
#if AS_CAN_GENERATE_JIT
	if (!NativeFormState.IsValid())
	{
		NativeFormState = MakeUnique<FAngelscriptNativeFormState>();
	}
#endif
	//Set everything up for angelscript usage.
	{
		FAngelscriptScopeTimer Timer(TEXT("== bindings total =="));
		if (!BindScriptTypes())
		{
			return false;
		}
	}
	
#if !AS_USE_BIND_DB
	// If we aren't using the database, write it during cook
	const bool bSkipWriteBindDB = RuntimeConfig.bSkipWriteBindDB;
	const bool bForceWriteBindDB = RuntimeConfig.bWriteBindDB;
	if ((RuntimeConfig.bRunningCommandlet && !bSkipWriteBindDB) || bForceWriteBindDB)
	{
		UE_LOG(Angelscript, Log, TEXT("Writing angelscript bind database to Binds.Cache file"));
		BindDatabase->Save(GetScriptRootDirectory() / TEXT("Binds.Cache"));
	}

#elif AS_USE_BIND_DB
	BindDatabase->Clear();
#endif

	// Cache V2 always starts from authoritative source or a validated Cache V2
	// generation. Legacy PrecompiledScript*.Cache files are never opened here.
	ReserveStaticNames(7000);

	// Setup thread local data
	GameThreadTLD->primaryContext = CreateContext();

	// Perform the initial compile of all script files
	InitialCompile();

#if AS_CAN_HOTRELOAD
	ScriptTestHotReloadRunner =
		new FAngelscriptScriptTestHotReloadRunner();
#endif

	// Use the checker thread if we want to detect hot reloads,
	// but we don't have access to the editor. In editor, the AngelscriptEditor
	// module will use the directory watcher system to detect reloads instead.
	bUseHotReloadCheckerThread = bScriptDevelopmentMode
		&& !RuntimeConfig.bIsEditor
		&& PackagedRuntimeReloadMode ==
			EAngelscriptPackagedRuntimeReloadMode::Disabled;
	if (bUseHotReloadCheckerThread)
		StartHotReloadThread();

#if !UE_BUILD_SHIPPING
	FCoreDelegates::OnGetOnScreenMessages.AddRaw(this, &FAngelscriptEngine::GetOnScreenMessages);
#endif
	UpdateLineCallbackState();
	return true;
}

bool FAngelscriptEngine::IsCollectingStaticJITCompatibilityBinds()
{
	if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
	{
		return CurrentEngine->bCollectStaticJITCompatibilityBinds;
	}

	return false;
}

void FAngelscriptEngine::PostInitialize_GameThread()
{
	check(CacheService.IsValid());
	CacheService->TransitionToRuntimeGameThread();
	PrimePackagedRuntimeReloadState();
	GetOnInitialCompileFinished().Broadcast();

	if (ShouldRequestAngelscriptCachePackageSmokeExit(
		RuntimeConfig, bDidInitialCompileSucceed))
	{
		UE_LOG(Angelscript, Display,
			TEXT("[CacheV2][PackageSmoke] Startup completed; requesting normal Engine shutdown so production Cache flush/report can run."));
		FPlatformMisc::RequestExit(
			false,
			TEXT("AngelScript Cache package smoke startup completed."));
	}
}

void FAngelscriptEngine::StartHotReloadThread()
{
	if (!bUseHotReloadCheckerThread)
		return;
	if (bHotReloadThreadStarted)
		return;
	bHotReloadThreadStarted = true;

	// Do a check to start with before starting the thread,
	// this will pre-fill all the timestamps. Discard the actual file change events.
	CheckForFileChanges();
	FileChangesDetectedForReload.Empty();

#if AS_CAN_HOTRELOAD
	struct FAngelscriptHotReloadThread : public FRunnable
	{
		bool bRunning = true;

		uint32 Run() override
		{
			auto& Manager = FAngelscriptEngine::Get();
			while(bRunning)
			{
				if (!Manager.bUseHotReloadCheckerThread)
					break;

				if (Manager.bWaitingForHotReloadResults)
				{
					Manager.CheckForFileChanges();
					Manager.bWaitingForHotReloadResults = false;
				}
				FPlatformProcess::Sleep(0.001f);
			}
			return 0;
		}

		void Stop() override { bRunning = false; }
		void Exit() override { bRunning = false; }
	};

	FRunnableThread::Create(new FAngelscriptHotReloadThread(), TEXT("AngelscriptHotReload"), 0, EThreadPriority::TPri_Lowest);
#endif
}

asCContext* FAngelscriptEngine::CreateContext()
{
	// Create a new context
	auto* Context = (asCContext*)Engine->CreateContext();
	Context->SetExceptionCallback(asFUNCTION(LogAngelscriptException), 0, asCALL_CDECL);
#if WITH_AS_DEBUGVALUES || WITH_AS_DEBUGSERVER
	Context->SetLineCallback(AngelscriptLineCallback);
	Context->SetStackPopCallback(AngelscriptStackPopCallback);
#endif
#if WITH_EDITOR
	if (!IsRunningCommandlet())
		Context->SetLoopDetectionCallback(AngelscriptLoopDetectionCallback);
#elif !UE_BUILD_TEST && !UE_BUILD_SHIPPING
	Context->SetLoopDetectionCallback(AngelscriptLoopDetectionCallback);
#endif
	return Context;
}

asIScriptContext* AngelscriptRequestContext(asIScriptEngine* Engine, void* Data)
{
	// Take a context from the thread-local pool if we have one
	auto& LocalPool = GAngelscriptContextPool;
	if (asCContext* Context = TryTakeContextFromPool(LocalPool.FreeContexts, Engine))
	{
		check(Context->GetState() != asEXECUTION_ACTIVE);
		check(Context->GetState() != asEXECUTION_SUSPENDED);
		return Context;
	}

	return CreateConfiguredContext(Engine);
}

void AngelscriptReturnContext(asIScriptEngine* Engine, asIScriptContext* Context, void* Data)
{
	asCContext* ConcreteContext = static_cast<asCContext*>(Context);
	ResetContextForPooling(ConcreteContext);

	// Return context to the thread local context poos
	auto& LocalPool = GAngelscriptContextPool;
	if (LocalPool.FreeContexts.Num() < AS_MAX_POOLED_CONTEXTS)
	{
		LocalPool.FreeContexts.Push(ConcreteContext);
		return;
	}

	// Can't add to global pool, just deallocate
	Context->Release();
}

FAngelscriptPooledContextBase::FAngelscriptPooledContextBase()
{
	Init(asCThreadManager::GetLocalData(), nullptr);
}

FAngelscriptPooledContextBase::FAngelscriptPooledContextBase(asIScriptEngine* DesiredScriptEngine)
{
	Init(asCThreadManager::GetLocalData(), DesiredScriptEngine);
}

FAngelscriptContext::FAngelscriptContext(UObject* WorldContext, asIScriptEngine* DesiredScriptEngine)
	: FAngelscriptPooledContextBase(asCThreadManager::GetLocalData(), DesiredScriptEngine)
{
	if (FAngelscriptEngine::CanUseGameThreadData())
	{
		PreviousWorldContext = GAmbientWorldContext;
		FAngelscriptEngine::AssignWorldContext(WorldContext);
		bChangedWorldContext = true;
	}
	else
	{
		bChangedWorldContext = false;
	}
}

FAngelscriptGameThreadContext::FAngelscriptGameThreadContext(UObject* WorldContext, asIScriptEngine* DesiredScriptEngine)
	: FAngelscriptPooledContextBase(FAngelscriptEngine::GameThreadTLD, DesiredScriptEngine)
{
	PreviousWorldContext = GAmbientWorldContext;
	FAngelscriptEngine::AssignWorldContext(WorldContext);
}

void FAngelscriptPooledContextBase::PrepareExternal(asIScriptFunction* Function)
{
	(*this)->Prepare(Function);
}

void FAngelscriptPooledContextBase::ExecuteExternal()
{
	(*this)->Execute();
}

void FAngelscriptPooledContextBase::Init(asCThreadLocalData* tld, asIScriptEngine* DesiredScriptEngine)
{
	asCContext* ActiveContext = tld->activeContext;
	if (ActiveContext != nullptr)
	{
		auto State = ActiveContext->m_status;
		if (State == asEXECUTION_ACTIVE
			&& (DesiredScriptEngine == nullptr || ActiveContext->GetEngine() == DesiredScriptEngine))
		{
			Context = ActiveContext;
			Context->PushState();
			bWasNested = true;
			return;
		}
	}

	// Take a context from the thread-local pool if we have one
	auto& LocalPool = GAngelscriptContextPool;
	if (DesiredScriptEngine == nullptr)
	{
		if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
		{
			DesiredScriptEngine = CurrentEngine->GetScriptEngine();
		}
	}

	if (asCContext* MatchingContext = TryTakeContextFromPool(LocalPool.FreeContexts, DesiredScriptEngine))
	{
		Context = MatchingContext;
		check(Context->GetState() != asEXECUTION_ACTIVE);
		check(Context->GetState() != asEXECUTION_SUSPENDED);
		bWasNested = false;
		return;
	}

	// Take a context from the global pool if we have one
	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		if (DesiredScriptEngine == nullptr || CurrentEngine->GetScriptEngine() == DesiredScriptEngine)
		{
			FScopeLock Lock(&CurrentEngine->GlobalContextPoolLock);
			if (asCContext* MatchingContext = TryTakeContextFromPool(CurrentEngine->GlobalContextPool, DesiredScriptEngine))
			{
				Context = MatchingContext;
				Context->MovedToNewThread();
				check(Context->GetState() != asEXECUTION_ACTIVE);
				check(Context->GetState() != asEXECUTION_SUSPENDED);
				bWasNested = false;
				return;
			}
		}
	}

	// Create a new context if none was found
	if (DesiredScriptEngine != nullptr)
	{
		Context = CreateConfiguredContext(DesiredScriptEngine);
	}
	else
	{
		auto& Manager = FAngelscriptEngine::Get();
		Context = Manager.CreateContext();
	}
	bWasNested = false;
}

FAngelscriptPooledContextBase::FAngelscriptPooledContextBase(FAngelscriptPooledContextBase&& Other)
{
	Context = Other.Context;
	bWasNested = Other.bWasNested;
	Other.Context = nullptr;
}

FAngelscriptPooledContextBase::~FAngelscriptPooledContextBase()
{
	if (Context == nullptr)
		return;

	if (bWasNested)
	{
		Context->PopState();
		return;
	}

	ResetContextForPooling(Context);

	// Return context to the thread local context poos
	auto& LocalPool = GAngelscriptContextPool;
	if (LocalPool.FreeContexts.Num() < AS_MAX_POOLED_CONTEXTS)
	{
		LocalPool.FreeContexts.Push(Context);
		return;
	}

	// Local context pool is full, return it to the global one
	FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
	if (CurrentEngine != nullptr && CurrentEngine->GetScriptEngine() == Context->GetEngine())
	{
		FScopeLock Lock(&CurrentEngine->GlobalContextPoolLock);
		if (CurrentEngine->GlobalContextPool.Num() < AS_MAX_POOLED_CONTEXTS)
		{
			CurrentEngine->GlobalContextPool.Push(Context);
			return;
		}
	}

	// Global context pool is also full, just deallocate the context
	Context->Release();
}

FAngelscriptContextPool::~FAngelscriptContextPool()
{
	// NOTE: Don't access FAngelscriptEngine here since this destructor is being called at the destruction of every thread (it's a thread_local)
	// and there's no guarantee AngelscriptManager is still around (e.g. if another global static destroys a thread in the destructor).
	if (IsEngineExitRequested())
		return;
	for (auto* Context : FreeContexts)
		Context->Release();
}

bool FAngelscriptEngine::BindScriptTypes()
{
	AS_PERF_SCOPE_STARTUP_BIND_SCRIPT_TYPES();
	LLM_SCOPE_BYTAG(Angelscript);
	MALLOCLEAK_SCOPED_CONTEXT(TEXT("Angelscript/BindScriptTypes"));

	#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptBindExecutionObservation::BeginBindScriptTypesTiming();
	FAngelscriptEnumTableBaselineProbe::Reset();
	ON_SCOPE_EXIT
	{
		FAngelscriptBindExecutionObservation::EndBindScriptTypesTiming();
		FAngelscriptEnumTableBaselineProbe::MaybeAutoDump();
	};
	#endif

	FAngelscriptBinds DirectBinds(*this);
	FString DirectBindDiagnostic;
	if (!FAngelscriptBind::ExecuteRegisteredBinds(DirectBinds, DirectBindDiagnostic))
	{
		UE_LOG(
			Angelscript,
			Error,
			TEXT("Direct AngelScript binding execution failed: %s"),
			*DirectBindDiagnostic);
		return false;
	}
	return true;
}

void FAngelscriptEngine::FindScriptFiles(
	IFileManager& FileManager,
	const FString& RelativeRoot,
	const FString& SearchDirectory,
	const TCHAR* Pattern,
	TArray<FFilenamePair>& OutFilenames,
	bool bSkipDevelopmentScripts,
	bool bSkipEditorScripts)
{
	FString CurrentSearch = SearchDirectory / Pattern;

	TArray<FString> LocalFiles;
	FileManager.FindFiles(LocalFiles, *CurrentSearch, true, false);

	for (const FString& FoundFile : LocalFiles)
	{
		OutFilenames.Add(FFilenamePair{
			SearchDirectory / FoundFile,
			RelativeRoot / FoundFile
			});
	}

	TArray<FString> LocalDirs;
	FString RecursiveDirSearch = SearchDirectory / TEXT("*");
	FileManager.FindFiles(LocalDirs, *RecursiveDirSearch, false, true);

	// FindFiles can return the same dir twice on Linux sometimes so eliminate dupes.
	for (const FString& FoundDirectory : TSet<FString>(LocalDirs))
	{
		if (bSkipDevelopmentScripts)
		{
			if (FoundDirectory == TEXT("Examples"))
				continue;
			if (FoundDirectory == TEXT("Dev"))
				continue;
		}

		if (bSkipEditorScripts)
		{
			if (FoundDirectory == TEXT("Editor"))
				continue;
		}

		FindScriptFiles(
			FileManager,
			RelativeRoot / FoundDirectory,
			SearchDirectory / FoundDirectory,
			Pattern,
			OutFilenames,
			bSkipDevelopmentScripts,
			bSkipEditorScripts
		);
	}
}

void FAngelscriptEngine::FindAllScriptFilenames(TArray<FFilenamePair>& OutFilenames)
{
	TArray<FAngelscriptSource> Sources;
	FindAllScriptSources(Sources);

	OutFilenames.Reserve(OutFilenames.Num() + Sources.Num());
	for (const FAngelscriptSource& Source : Sources)
	{
		OutFilenames.Add(FFilenamePair{
			Source.AbsoluteFilename,
			Source.RelativeFilename,
			Source.VirtualPath.ToString()
		});
	}
}

void FAngelscriptEngine::FindAllScriptSources(TArray<FAngelscriptSource>& OutSources)
{
	const bool bSkipDevelopmentScripts = !ShouldUseEditorScripts();
	const bool bSkipEditorScripts = bSkipDevelopmentScripts;

	check(Dependencies.SourceProvider.IsValid());
	Dependencies.SourceProvider->FindSources(
		GetEffectiveScriptRootDescriptors(),
		bSkipDevelopmentScripts,
		bSkipEditorScripts,
		OutSources);
}

namespace AngelscriptEngineSourceProvider_Private
{
	FAngelscriptSource MakeSourceFromFilenamePair(const FAngelscriptEngine::FFilenamePair& Filename)
	{
		FAngelscriptVirtualPath VirtualPath;
		if (FAngelscriptVirtualPath::TryParse(Filename.VirtualPath, VirtualPath))
		{
			if (VirtualPath.GetSourceKind() == EAngelscriptSourceKind::Plugin)
			{
				return FAngelscriptSource::FromPluginFile(
					VirtualPath.GetMountName(),
					Filename.RelativePath,
					Filename.AbsolutePath);
			}

			if (VirtualPath.GetSourceKind() == EAngelscriptSourceKind::Game)
			{
				return FAngelscriptSource::FromGameFile(
					Filename.RelativePath,
					Filename.AbsolutePath);
			}

			FAngelscriptSource Source;
			Source.VirtualPath = VirtualPath;
			Source.ModuleName = VirtualPath.ToModuleName();
			Source.RelativeFilename = VirtualPath.GetRelativePath();
			Source.AbsoluteFilename = Filename.AbsolutePath;
			Source.SourceKind = VirtualPath.GetSourceKind();
			return Source;
		}

		return FAngelscriptSource::FromGameFile(Filename.RelativePath, Filename.AbsolutePath);
	}
}

FString FAngelscriptEngine::MakeSourceStateKey(const FFilenamePair& Filename) const
{
	return Filename.VirtualPath.IsEmpty() ? Filename.RelativePath : Filename.VirtualPath;
}

bool FAngelscriptEngine::HasAnyDebugServerClients()
{
#if WITH_AS_DEBUGSERVER
	if (DebugServer == nullptr)
		return false;
	if (DebugServer->HasAnyClients())
		return true;
#endif
	return false;
}

void FAngelscriptEngine::ReplaceScriptAssetContent(FString AssetName, TArray<FString> AssetContent)
{
#if WITH_AS_DEBUGSERVER
	FAngelscriptReplaceAssetDefinition Message;
	Message.AssetName = AssetName;
	Message.Lines = AssetContent;
	DebugServer->SendMessageToAll(EDebugMessageType::ReplaceAssetDefinition, Message);
#endif
}

namespace AngelscriptEngineExactStartup_Private
{
	enum class EAttemptResult : uint8
	{
		Miss = 0,
		Restored = 1,
		FatalPartialRestore = 2,
	};

	static void RecordSelection(
		FAngelscriptEngine& Engine,
		const EAngelscriptCacheDecisionOutcome Outcome,
		const EAngelscriptCacheDecisionReasonDomain ReasonDomain,
		const uint32 ReasonCode,
		const FAngelscriptArtifactProfileKey& Profile,
		const FAngelscriptHash256* GenerationId,
		const uint64 ElapsedMicroseconds)
	{
		FAngelscriptCacheService* Service = Engine.GetCacheService();
		if (Service == nullptr)
		{
			return;
		}
		FAngelscriptCacheDecisionEvent Event;
		Event.Stage = EAngelscriptCacheDecisionStage::StartupSelection;
		Event.Outcome = Outcome;
		Event.ReasonDomain = ReasonDomain;
		Event.ReasonCode = ReasonCode;
		Event.Profile = Profile;
		if (GenerationId != nullptr)
		{
			Event.ExpectedCoordinate = *GenerationId;
		}
		Event.ElapsedMicroseconds = ElapsedMicroseconds;
		Service->RecordDecisionEvent(MoveTemp(Event));
	}

	static EAttemptResult TryRestore(
		FAngelscriptEngine& Engine,
		const FAngelscriptPreprocessorContext& PreprocessorContext,
		const TConstArrayView<FAngelscriptSourceRoot> ScriptRoots,
		const bool bSkipDevelopmentScripts,
		const bool bSkipEditorScripts,
		TUniquePtr<FAngelscriptCacheCompileReuseContext>& OutReuseContext)
	{
		OutReuseContext.Reset();
		const UAngelscriptCacheSettings* Settings =
			GetDefault<UAngelscriptCacheSettings>();
		if (Settings == nullptr || !Settings->bEnableCacheV2
			|| Engine.GetRuntimeConfig().bDisableCacheV2Persistence)
		{
			return EAttemptResult::Miss;
		}

		const double StartedSeconds = FPlatformTime::Seconds();
		FAngelscriptCacheEnvironmentProfile Environment;
		const FAngelscriptCacheEnvironmentProfileResult EnvironmentResult =
			BuildAngelscriptCacheEnvironmentProfile(
				Engine, PreprocessorContext, ScriptRoots, Environment);
		if (!EnvironmentResult.IsSuccess())
		{
			RecordSelection(
				Engine,
				EAngelscriptCacheDecisionOutcome::Rejected,
				EAngelscriptCacheDecisionReasonDomain::Validation,
				static_cast<uint32>(EnvironmentResult.Error),
				{}, nullptr,
				static_cast<uint64>((FPlatformTime::Seconds()
					- StartedSeconds) * 1000000.0));
			return EAttemptResult::Miss;
		}

		IAngelscriptSourceProvider* SourceProvider = Engine.GetSourceProvider();
		if (SourceProvider == nullptr)
		{
			RecordSelection(
				Engine,
				EAngelscriptCacheDecisionOutcome::Rejected,
				EAngelscriptCacheDecisionReasonDomain::Validation,
				static_cast<uint32>(
					EAngelscriptCacheSourceDiscoveryError::InvalidRequest),
				Environment.CaptureOptions.Profile,
				nullptr,
				static_cast<uint64>((FPlatformTime::Seconds()
					- StartedSeconds) * 1000000.0));
			return EAttemptResult::Miss;
		}
		FAngelscriptCacheProductionSourceDiscoveryResult Discovery;
		const FAngelscriptCacheSourceDiscoveryStatus DiscoveryStatus =
			FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
				*SourceProvider,
				ScriptRoots,
				bSkipDevelopmentScripts,
				bSkipEditorScripts,
				Environment.DiscoveryConfig,
				{},
				Discovery);
		if (!DiscoveryStatus.IsSuccess())
		{
			RecordSelection(
				Engine,
				EAngelscriptCacheDecisionOutcome::Rejected,
				EAngelscriptCacheDecisionReasonDomain::Validation,
				static_cast<uint32>(DiscoveryStatus.Error),
				Environment.CaptureOptions.Profile,
				nullptr,
				static_cast<uint64>((FPlatformTime::Seconds()
					- StartedSeconds) * 1000000.0));
			return EAttemptResult::Miss;
		}

		FString RequestedBaseRoot;
		const FAngelscriptCacheStoreResult RootResult =
			ResolveAngelscriptCacheRequestedBaseRootForEngine(
				Engine, RequestedBaseRoot);
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		FAngelscriptCacheStorePaths Paths;
		FAngelscriptCacheStoreResult PathsResult;
		if (RootResult.IsSuccess() && FileOps.IsValid()
			&& LockOps.IsValid())
		{
			PathsResult = BuildAngelscriptCacheStorePaths(
				RequestedBaseRoot,
				Environment.CaptureOptions.Compatibility,
				Environment.CaptureOptions.Context,
				*FileOps,
				Paths);
		}
		else
		{
			PathsResult = RootResult.IsSuccess()
				? FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
					EAngelscriptCacheStoreStage::RootValidation)
				: RootResult;
		}
		if (!PathsResult.IsSuccess())
		{
			RecordSelection(
				Engine,
				EAngelscriptCacheDecisionOutcome::Rejected,
				EAngelscriptCacheDecisionReasonDomain::Store,
				static_cast<uint32>(PathsResult.Error),
				Environment.CaptureOptions.Profile,
				nullptr,
				static_cast<uint64>((FPlatformTime::Seconds()
					- StartedSeconds) * 1000000.0));
			return EAttemptResult::Miss;
		}

		FAngelscriptCacheReadSelection Selection;
		Selection.Compatibility = Environment.CaptureOptions.Compatibility;
		Selection.Context = Environment.CaptureOptions.Context;
		Selection.Profile = Environment.CaptureOptions.Profile;
		Selection.bRequireSourceSnapshotMatch = false;
		Selection.bAllowPendingColdStart = false;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		const FAngelscriptCacheStoreResult Open =
			OpenBestAngelscriptCacheReadSession(
				Paths,
				Selection,
				{},
				FPlatformTime::Seconds()
					+ FMath::Max(0.1, static_cast<double>(
						Settings->ShutdownFlushTimeoutSeconds)),
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps,
				Session);
		if (!Open.IsSuccess() || !Session.IsValid())
		{
			RecordSelection(
				Engine,
				Open.IsSuccess()
					? EAngelscriptCacheDecisionOutcome::Miss
					: EAngelscriptCacheDecisionOutcome::Rejected,
				EAngelscriptCacheDecisionReasonDomain::Store,
				static_cast<uint32>(Open.Error),
				Environment.CaptureOptions.Profile,
				nullptr,
				static_cast<uint64>((FPlatformTime::Seconds()
					- StartedSeconds) * 1000000.0));
			return EAttemptResult::Miss;
		}

		const FAngelscriptHash256 GenerationId = Session->GetGenerationId();
		RecordSelection(
			Engine,
			EAngelscriptCacheDecisionOutcome::Reused,
			EAngelscriptCacheDecisionReasonDomain::Store,
			static_cast<uint32>(EAngelscriptCacheStoreError::None),
			Environment.CaptureOptions.Profile,
			&GenerationId,
			static_cast<uint64>((FPlatformTime::Seconds()
				- StartedSeconds) * 1000000.0));

		const double RestoreStartedSeconds = FPlatformTime::Seconds();
		const FAngelscriptCacheExactStartupResult Restore =
			RestoreAngelscriptCacheExactStartup(
				Engine,
				Session->GetGeneration(),
				Environment.CaptureOptions.Profile,
				Discovery,
				{},
				{},
				&GenerationId
#if WITH_ANGELSCRIPT_UNITTESTS
				, Engine.GetCacheRestoreFaultInjectorForTests()
#endif
				);
		FAngelscriptCacheDecisionEvent RestoreEvent;
		RestoreEvent.Stage = EAngelscriptCacheDecisionStage::StartupRestore;
		RestoreEvent.Outcome = Restore.IsRestored()
			? EAngelscriptCacheDecisionOutcome::Restored
			: Restore.Disposition ==
				EAngelscriptCacheExactStartupDisposition::Miss
					? EAngelscriptCacheDecisionOutcome::Miss
					: EAngelscriptCacheDecisionOutcome::Rejected;
		RestoreEvent.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::ExactStartup;
		RestoreEvent.ReasonCode = static_cast<uint32>(Restore.Reason);
		RestoreEvent.Validation = Restore.Validation;
		RestoreEvent.Detail = Restore.Detail;
		RestoreEvent.Profile = Environment.CaptureOptions.Profile;
		RestoreEvent.SourceSnapshot =
			Session->GetGeneration().Manifest.SourceSnapshot;
		RestoreEvent.ExpectedCoordinate = GenerationId;
		RestoreEvent.PrimaryCount = Restore.RestoredModuleCount;
		RestoreEvent.SecondaryCount = Restore.RestoredFunctionCount;
		RestoreEvent.ElapsedMicroseconds = static_cast<uint64>(
			(FPlatformTime::Seconds() - RestoreStartedSeconds) * 1000000.0);
		for (const FAngelscriptCacheModuleSnapshotLink& Link
			: Session->GetGeneration().Manifest.ModuleSnapshots)
		{
			RestoreEvent.ModuleKeys.Add(Link.ModuleKey);
		}
		if (FAngelscriptCacheService* Service = Engine.GetCacheService())
		{
			Service->RecordDecisionEvent(MoveTemp(RestoreEvent));
		}

		if (Restore.IsRestored())
		{
			UE_LOG(Angelscript, Display,
				TEXT("[CacheV2][ExactStartup] Restored Generation=%s Modules=%u Functions=%u Detail=%s"),
				*GenerationId.ToHexString(),
				Restore.RestoredModuleCount,
				Restore.RestoredFunctionCount,
				*Restore.Detail);
			return EAttemptResult::Restored;
		}

		UE_LOG(Angelscript, Verbose,
			TEXT("[CacheV2][ExactStartup] Candidate miss Generation=%s Disposition=%u Reason=%u Restored=%u Detail=%s"),
			*GenerationId.ToHexString(),
			static_cast<uint32>(Restore.Disposition),
			static_cast<uint32>(Restore.Reason),
			Restore.RestoredModuleCount,
			*Restore.Detail);
		if (Restore.Disposition
				== EAngelscriptCacheExactStartupDisposition::Miss
			&& Restore.RestoredModuleCount == 0)
		{
			OutReuseContext = FAngelscriptCacheCompileReuseContext::Create(
				MoveTemp(Session), GenerationId, Environment.CaptureOptions);
		}
		return Restore.RestoredModuleCount == 0
			? EAttemptResult::Miss
			: EAttemptResult::FatalPartialRestore;
	}
}

void FAngelscriptEngine::InitialCompile()
{
	AS_PERF_SCOPE_COMPILE_INITIAL();

	bool bSuccess = true;
	TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile;
	TArray<FFilenamePair> Filenames;
	TArray<FAngelscriptSource> Sources;
	TOptional<FAngelscriptCacheCompileCaptureContext> CacheCaptureContext;
	TUniquePtr<FAngelscriptCacheCompileReuseContext> CacheReuseContext;
	bool bRestoredExactStartup = false;

	ResetDiagnostics();
	if (CacheService.IsValid())
	{
		CacheService->ClearFunctionReuseSummary();
	}

	// Make sure we scan all plugins for script roots as well. Cache V2 either
	// restores a validated generation through its own coordinator or compiles these
	// authoritative sources; the legacy archive is never a module source.
		AllScriptRoots = DiscoverScriptRootDescriptors();
		AllRootPaths.Reset(AllScriptRoots.Num());
		for (const FAngelscriptSourceRoot& ScriptRoot : AllScriptRoots)
		{
			AllRootPaths.Add(ScriptRoot.AbsolutePath);
		}
		for (const FString& Path : AllRootPaths)
		{
			UE_LOG(Angelscript, Display, TEXT("Angelscript root path: %s"), *Path);
		}
		const FAngelscriptLegacyCacheInspection LegacyInspection =
			InspectAngelscriptLegacyCacheArtifactsFromDisk(AllRootPaths);
		if (LegacyInspection.HasRejectedLegacyScriptCache())
		{
			UE_LOG(Angelscript, Warning,
				TEXT("[CacheV2][LegacyRejected] %s"),
				*LegacyInspection.FormatDiagnostic());
		}

		const FAngelscriptPreprocessorContext PreprocessorContext =
			FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		const bool bSkipDevelopmentScripts = !ShouldUseEditorScripts();
		const AngelscriptEngineExactStartup_Private::EAttemptResult
			ExactStartup = AngelscriptEngineExactStartup_Private::TryRestore(
				*this,
				PreprocessorContext,
				GetEffectiveScriptRootDescriptors(),
				bSkipDevelopmentScripts,
				bSkipDevelopmentScripts,
				CacheReuseContext);
		bRestoredExactStartup = ExactStartup ==
			AngelscriptEngineExactStartup_Private::EAttemptResult::Restored;
		if (ExactStartup == AngelscriptEngineExactStartup_Private::
			EAttemptResult::FatalPartialRestore)
		{
			bSuccess = false;
			UE_LOG(Angelscript, Error,
				TEXT("[CacheV2][ExactStartup] A candidate partially mutated the fresh Engine; refusing unsafe compile fallback"));
		}
		else if (!bRestoredExactStartup)
		{
			// Use preprocessor to read authoritative script files on a safe miss.
			FAngelscriptPreprocessor Preprocessor(PreprocessorContext);
			Preprocessor.SetSourceProvider(Dependencies.SourceProvider.Get());

			{
				FAngelscriptScopeTimer Timer(TEXT("load script files from disk"));

				/* Add all files from the script root recursively.*/
				FindAllScriptSources(Sources);

				for (const FAngelscriptSource& Source : Sources)
				{
					Filenames.Add(FFilenamePair{
						Source.AbsoluteFilename,
						Source.RelativeFilename,
						Source.VirtualPath.ToString()
					});
					Preprocessor.AddSource(Source);
				}
			}

			bSuccess = Preprocessor.Preprocess();
			ModulesToCompile = Preprocessor.GetModulesToCompile();
			if (bSuccess && !ModulesToCompile.IsEmpty())
			{
				FAngelscriptCacheCompileCaptureContext PreparedContext;
				const FAngelscriptCacheCompileCapturePreparationResult Preparation =
					PrepareAngelscriptCacheCompileCaptureContext(
						*this,
						PreprocessorContext,
						*Dependencies.SourceProvider,
						GetEffectiveScriptRootDescriptors(),
						bSkipDevelopmentScripts,
						bSkipDevelopmentScripts,
						PreparedContext);
				if (Preparation.IsSuccess())
				{
					CacheCaptureContext.Emplace(MoveTemp(PreparedContext));
				}
				else
				{
					UE_LOG(Angelscript, Warning,
						TEXT("[CacheV2] Initial compile capture preparation skipped: Error=%u Environment=%u Discovery=%u Validation=%u Detail=%s"),
						static_cast<uint32>(Preparation.Error),
						static_cast<uint32>(Preparation.EnvironmentError),
						static_cast<uint32>(Preparation.SourceDiscoveryError),
						static_cast<uint32>(Preparation.Validation.Error),
						*Preparation.Detail);
				}
			}
		}

	if (bSuccess && !bRestoredExactStartup)
	{
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		ECompileResult Result = CompileModules(
			ECompileType::Initial,
			ModulesToCompile,
			CompiledModules,
			{},
			CacheCaptureContext.IsSet() ? &CacheCaptureContext.GetValue() : nullptr,
			CacheReuseContext.Get());
		if (Result == ECompileResult::Error)
		{
			bSuccess = false;
		}
		if (CacheReuseContext.IsValid() && CacheService.IsValid())
		{
			CacheService->PublishFunctionReuseSummary(
				CacheReuseContext->CaptureSummary());
		}
	}
	else if (!bSuccess)
	{
		UE_LOG(Angelscript, Error, TEXT("Angelscript preprocessing failed!"));
	}

	bool bInteractiveStartupRetryAvailable = false;
#if PLATFORM_DESKTOP
	bInteractiveStartupRetryAvailable = FSlateApplication::IsInitialized();
#endif

	const EAngelscriptStartupCompileFailureResponse StartupFailureResponse =
		ResolveAngelscriptStartupCompileFailureResponse(
			RuntimeConfig,
			bInteractiveStartupRetryAvailable);

	// Noninteractive hosts must never wait on a Slate modal. UE's Windows launch
	// loop does not propagate a graceful RequestExitWithStatus code through
	// GuardedMain, so close the Cache service lifecycle and emit the explicitly
	// requested report synchronously before a deterministic forced failure exit.
	// BeginEngineShutdown only changes the mutation phase; it does not flush or
	// publish. The previously persisted Store remains authoritative because a
	// failed startup never publishes a replacement Generation.
	if (!bSuccess
		&& StartupFailureResponse
			== EAngelscriptStartupCompileFailureResponse::RequestExit)
	{
		const FAngelscriptStartupCompileFailureExitRequest ExitRequest =
			ResolveAngelscriptStartupCompileFailureExitRequest(RuntimeConfig);
		if (ExitRequest.bBeginCacheShutdownBeforeDiagnosticReport
			&& CacheService.IsValid())
		{
			CacheService->BeginEngineShutdown();
		}
		if (ExitRequest.bWriteRequestedDiagnosticReportBeforeExit)
		{
			WriteRequestedCacheV2ProcessReport();
		}
		UE_LOG(Angelscript, Error,
			TEXT("[StartupCompileFailure] Cannot run after startup compile failure. Requesting %s exit with status %u (commandlet=%s exitOnError=%s unattended=%s interactiveRetryAvailable=%s)."),
			ExitRequest.bForce ? TEXT("immediate") : TEXT("graceful"),
			static_cast<uint32>(ExitRequest.Status),
			RuntimeConfig.bRunningCommandlet ? TEXT("true") : TEXT("false"),
			RuntimeConfig.bExitOnError ? TEXT("true") : TEXT("false"),
			RuntimeConfig.bIsUnattended ? TEXT("true") : TEXT("false"),
			bInteractiveStartupRetryAvailable ? TEXT("true") : TEXT("false"));

		GIsCriticalError = true;
		FPlatformMisc::RequestExitWithStatus(
			ExitRequest.bForce,
			ExitRequest.Status);
	}
	else if (!bSuccess)
	{
		bool bPreviousUseHotReloadCheckerThread = bUseHotReloadCheckerThread;

		// Next hot-reload compiles everything, since it's hard to tell what needs reloading.
		for (const FFilenamePair& Filename : Filenames)
			PreviouslyFailedReloadFiles.Add(Filename);

		FThreadSafeBool bErrorResponseDone(false);
		auto ShowErrorDialog = [&]()
		{
			ON_SCOPE_EXIT
			{
				bErrorResponseDone = true;
			};

#if !PLATFORM_DESKTOP
			const FString CompileDiagnostics = FormatDiagnostics();
			const FString Message = FString::Printf(
				TEXT("Angelscript code failed to compile at engine startup:")
				TEXT("\n\n%s"),
				*CompileDiagnostics);

			UE_LOG(Angelscript, Error, TEXT("[StartupCompileFailure] Cannot display reload dialog on non-desktop platform. Requesting exit.\n%s"), *CompileDiagnostics);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
			GIsCriticalError = true;
			FPlatformMisc::RequestExit(true);
			return;
#else
			if (!FSlateApplication::IsInitialized())
			{
				const FString CompileDiagnostics = FormatDiagnostics();
				const FString Message = FString::Printf(
					TEXT("Angelscript code failed to compile at engine startup, but Slate is not initialized so the retry window cannot be shown.")
					TEXT("\n\n%s"),
					*CompileDiagnostics);

				UE_LOG(Angelscript, Error, TEXT("[StartupCompileFailure] Slate is not initialized; cannot display compile-error retry dialog. Requesting exit.\n%s"), *CompileDiagnostics);
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
				GIsCriticalError = true;
				FPlatformMisc::RequestExit(true);
				return;
			}

			// Prematurely start the hot reload thread since we will be using it in our modal window
			bUseHotReloadCheckerThread = true;
			StartHotReloadThread();
			UE_LOG(Angelscript, Warning, TEXT("[StartupCompileFailure] Initial compile failed. Opening modal retry dialog for %d failed file(s)."), PreviouslyFailedReloadFiles.Num());

			auto OpenScriptButton = SNew(SButton)
				.Text(FText::FromString("Open Angelscript workspace (VS Code)"))
				.OnClicked_Lambda([this]() -> FReply
				{
					const UAngelscriptSettings* Settings = ConfigSettings != nullptr ? ConfigSettings : GetDefault<UAngelscriptSettings>();
					FString WorkspacePath;
					if (Settings != nullptr && !Settings->VSCodeWorkspacePath.IsEmpty())
					{
						WorkspacePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Settings->VSCodeWorkspacePath);
					}
					else
					{
						WorkspacePath = AllRootPaths.IsEmpty() ? FPaths::ProjectDir() / TEXT("Script") : AllRootPaths[0];
					}

					UE_LOG(Angelscript, Display, TEXT("[StartupCompileFailure] Opening Angelscript workspace in VS Code: %s"), *WorkspacePath);
					FPlatformMisc::OsExecute(nullptr, TEXT("code"), *FString::Printf(TEXT("\"%s\""), *WorkspacePath));
					return FReply::Handled();
				});

			auto PromptWindow = SNew(SWindow)
				.Title(FText::FromString("Angelscript Compile Errors"))
				.ClientSize(FVector2D(800, 600))
				.SizingRule(ESizingRule::UserSized);

			TSharedPtr<SMultiLineEditableTextBox> PromptText;
			PromptWindow->SetContent(
				SNew(SBorder)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					.Padding(10.f)
					[
						SAssignNew(PromptText, SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 10.f, 10.f)
					.HAlign(HAlign_Right)
					[
						OpenScriptButton
					]
				]);

			auto TickHandle = FSlateApplication::Get().GetOnModalLoopTickEvent().AddLambda([&](float DeltaTime)
			{
				if (PreviouslyFailedReloadFiles.Num() == 0)
				{
					// We succesfully compiled! Close the prompt.
					UE_LOG(Angelscript, Display, TEXT("[StartupCompileFailure] Startup compile errors resolved by hot reload. Closing retry dialog."));
					FSlateApplication::Get().RequestDestroyWindow(PromptWindow);
					bSuccess = true;
				}

				// Show an error and prompt for retry
				const FString CompileDiagnostics = FormatDiagnostics();
				const FText Message = FText::FromString(FString::Printf(
					TEXT("Angelscript code failed to compile at engine startup.\n")
					TEXT("Various assets will not load correctly without working angelscript code.\n")
					TEXT("\n\nPlease fix the errors and save the script files to proceed to open the editor.")
					TEXT("\n\n%s"),
					*CompileDiagnostics));
				if (!PromptText->GetText().EqualTo(Message))
				{
					PromptText->SetText(Message);
				}

				// Make sure we detect hot reloads when we need them
				CheckForHotReload(ECompileType::FullReload);

				// Run the debug server if we have one to send diagnostics through
	#if WITH_AS_DEBUGSERVER
				if (DebugServer != nullptr)
					DebugServer->ProcessMessages();
#endif
			});

			UE_LOG(Angelscript, Display, TEXT("[StartupCompileFailure] Showing startup compile-error modal dialog."));
			FSlateApplication::Get().AddModalWindow(PromptWindow, nullptr);
			FSlateApplication::Get().GetOnModalLoopTickEvent().Remove(TickHandle);
			UE_LOG(Angelscript, Display, TEXT("[StartupCompileFailure] Startup compile-error modal dialog closed. success=%s"), bSuccess ? TEXT("true") : TEXT("false"));

			if (!bSuccess)
			{
				UE_LOG(Angelscript, Error, TEXT("[StartupCompileFailure] Startup compile errors were not resolved before dialog close. Requesting exit."));
				GIsCriticalError = true;
				FPlatformMisc::RequestExit(true);
				return;
			}
#endif
		};

		if (IsInGameThread())
		{
			ShowErrorDialog();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [&]() { ShowErrorDialog(); });

			while (!bErrorResponseDone)
				FPlatformProcess::Sleep(0.01f);
		}

		// Reset the hot reload detection mmethod we were using
		bUseHotReloadCheckerThread = bPreviousUseHotReloadCheckerThread;
	}

	// In order to provide proper support for tests that need the AssetManager
	// and UPrimaryDataAsset already created, we need to delay the test discovery
	// until the initial scan is finished.
	FCoreDelegates::GetOnPostEngineInit().AddLambda([&]()
	{
		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		if (AssetManager != nullptr)
		{
			AssetManager->CallOrRegister_OnCompletedInitialScan(
				FSimpleMulticastDelegate::FDelegate::CreateLambda([&]() {
					DiscoverTests();
					bCompletedAssetScan = true;
				})
			);
		}
		else
		{
			UE_LOG(Angelscript, Warning, TEXT("Asset Manager was not ready in PostEngineInit. Tests are discovered without completing an initial asset scan."));
			DiscoverTests();
		}
	});

	bDidInitialCompileSucceed = bSuccess;
	bIsInitialCompileFinished = true;

#if WITH_EDITOR
	if (RuntimeConfig.bDumpDocumentation)
	{
		FAngelscriptDocs::DumpDocumentation(Engine);
		FPlatformMisc::RequestExit(false);
	}
#endif
}

void FAngelscriptEngine::DiscoverTests()
{
	const bool bDiscoveryEnabled =
		WITH_DEV_AUTOMATION_TESTS
		&& GetDefault<UAngelscriptTestSettings>()->bEnableTestDiscovery
		&& !bSimulateCooked
#if WITH_EDITOR
		&& !IsRunningCookCommandlet();
#else
		;
#endif
	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModuleList =
		GetActiveModules();
	const FAngelscriptScriptTestRegistryBuildResult ScriptSuiteResult =
		FAngelscriptScriptTestRegistry::Get().Rebuild(
			ActiveModuleList,
			bDiscoveryEnabled);
	for (const FAngelscriptScriptTestDiagnostic& Diagnostic :
		ScriptSuiteResult.Diagnostics)
	{
		FAngelscriptEngine::FDiagnostic EngineDiagnostic;
		EngineDiagnostic.Row = Diagnostic.SourceLine;
		EngineDiagnostic.Column = 1;
		EngineDiagnostic.bIsError = true;
		EngineDiagnostic.bIsInfo = false;
		EngineDiagnostic.Message = Diagnostic.Message;
		ScriptCompileError(Diagnostic.SourceFile, EngineDiagnostic);
	}

	if (!bDiscoveryEnabled)
	{
		return;
	}
}

bool FAngelscriptEngine::PerformHotReload(
	ECompileType CompileType,
	const TArray<FFilenamePair>& InReloadFiles,
	ECompileResult* OutCompileResult,
	const bool bRejectStructuralChanges)
{
	AS_PERF_SCOPE_RELOAD_HOT_RELOAD();
	if (OutCompileResult != nullptr)
	{
		*OutCompileResult = ECompileResult::Error;
	}

	TGuardValue<bool> ScopeHotReloading(bIsHotReloading, true);
	FAngelscriptScopeTimer Timer(TEXT("==script reload total =="));

	// Create progress indicator
	FScopedSlowTask SlowTask(3.f, FText::FromString(TEXT("Angelscript Hot Reload")));
	if (CompileType == ECompileType::FullReload && bIsInitialCompileFinished)
		SlowTask.MakeDialogDelayed(0.5f);
	SlowTask.EnterProgressFrame(0.5f);

	const FAngelscriptPreprocessorContext PreprocessorContext =
		FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
	FAngelscriptPreprocessor Preprocessor(PreprocessorContext);
	Preprocessor.SetSourceProvider(Dependencies.SourceProvider.Get());

	TSet<FFilenamePair> AlreadyDeletedFiles;
	TArray<FFilenamePair> FileList;
	FileList.Append(InReloadFiles);

	// Any files we tried to reload before but failed to should also be reload now.
	auto& FileManager = IFileManager::Get();
	for (auto& FailedFile : PreviouslyFailedReloadFiles)
	{
		// If it was already deleted before, remember that
		if (!FileManager.FileExists(*FailedFile.AbsolutePath))
			AlreadyDeletedFiles.Add(FailedFile);

		FileList.AddUnique(FailedFile);
	}
	PreviouslyFailedReloadFiles.Empty();

	// Build a set of all files which are dependent on any of the modified files,
	// such that we can hot reload all of them.
	TSet<FFilenamePair> FilesToHotReload;
	if (FileList.Num() > 0)
	{
		if (GAngelscriptRecompileAvoidance && ShouldUseAutomaticImportMethod())
		{
			// When using recompile avoidance, dependency handling is done by the compile step,
			// so we don't track it here, we only reload actually changed files.
			FilesToHotReload.Append(FileList);
		}
		else
		{
			FAngelscriptScopeTimer DependencyCheckTimer(TEXT("reload dependency check"));
			FilesToHotReload.Reserve(ActiveModules.Num() * 2);

			// Build a lookup table from relative file path -> module
			TMap<FString, FAngelscriptModuleDesc*> RelativeFileToModule;
			RelativeFileToModule.Reserve(ActiveModules.Num() * 2);

			TMap<asCModule*, FAngelscriptModuleDesc*> ScriptModuleToModule;
			ScriptModuleToModule.Reserve(ActiveModules.Num() * 2);

			for (auto& Module : ActiveModules)
			{
				auto ModulePtr = &(Module.Value.Get());
				for (const auto& Section : ModulePtr->Code)
					RelativeFileToModule.Add(Section.RelativeFilename, ModulePtr);

				if (ModulePtr->ScriptModule != nullptr)
					ScriptModuleToModule.Add((asCModule*)ModulePtr->ScriptModule, ModulePtr);
			}

			if (ShouldUseAutomaticImportMethod())
			{
				// We will need to progressively mark all modules that depend on one of the files that should be reloaded
				TSet<asCModule*> MarkedModules;
				MarkedModules.Reserve(ActiveModules.Num());

				// Push the modules for all changed files on the module job stack
				for (auto& File : FileList)
				{
					if (auto* ModulePtr = RelativeFileToModule.Find(File.RelativePath))
					{
						if ((*ModulePtr)->ScriptModule != nullptr)
							MarkedModules.Add((asCModule*)((*ModulePtr)->ScriptModule));
					}
					else
					{
						FilesToHotReload.Add(File);
					}
				}

				// Keep marking modules until we settle down
				bool bDidMarkModules = true;
				while (bDidMarkModules)
				{
					bDidMarkModules = false;

					for (auto& Module : ActiveModules)
					{
						auto* ScriptModule = (asCModule*)Module.Value->ScriptModule;
						if (ScriptModule == nullptr)
							continue;
						if (MarkedModules.Contains(ScriptModule))
							continue;

						bool bIsDependent = false;
						for (const auto& DependencyElem : ScriptModule->moduleDependencies)
						{
							if (MarkedModules.Contains(DependencyElem.Key))
							{
								bIsDependent = true;
								break;
							}
						}

						if (bIsDependent)
						{
							MarkedModules.Add(ScriptModule);
							bDidMarkModules = true;
						}
					}
				}

				// Queue up reloads for all marked modules
				for (asCModule* ReloadModule : MarkedModules)
				{
					if (auto* ModulePtr = ScriptModuleToModule.Find(ReloadModule))
					{
						for (const auto& Section : (*ModulePtr)->Code)
							FilesToHotReload.Add(FFilenamePair{ Section.AbsoluteFilename, Section.RelativeFilename, Section.VirtualPath });
					}
				}
			}
			else
			{
				// Book-keeping over the modules which have been visited when
				// traversing the module dependencies.
				TSet<FAngelscriptModuleDesc*> VisitedModules;
				VisitedModules.Reserve(ActiveModules.Num());

				// A job stack of modules which should be visited
				TArray<FAngelscriptModuleDesc*> ModuleJobs;
				ModuleJobs.Reserve(ActiveModules.Num());

				// Push the modules for all changed files on the module job stack
				for (auto& File : FileList)
				{
					FilesToHotReload.Add(File);
					if (auto ModulePtr = RelativeFileToModule.Find(File.RelativePath))
					{
						ModuleJobs.AddUnique(*ModulePtr);
						VisitedModules.Add(*ModulePtr);
					}
				}


				// Build a reverse dependency map for module->dependent modules (non-recursive)
				TMap<FAngelscriptModuleDesc*, TArray<FAngelscriptModuleDesc*>> ReverseDeps;
				if (ModuleJobs.Num() > 0)
				{
					ReverseDeps.Reserve(ActiveModules.Num());
					for (auto& Module : ActiveModules)
					{
						auto ModulePtr = &(Module.Value.Get());
						for (const FString& ImportedModule : Module.Value->ImportedModules)
						{
							auto ImportedModuleDesc = GetModuleByModuleName(ImportedModule);
							if (ImportedModuleDesc.IsValid())
							{
								auto ImportedModulePtr = &(ImportedModuleDesc.ToSharedRef().Get());
								ReverseDeps.FindOrAdd(ImportedModulePtr).Add(ModulePtr);
							}
						}
					}
				}


				// Add all files associated with the visited modules and recurse through
				// the dependent modules.
				while (ModuleJobs.Num() > 0)
				{
					auto ModulePtr = ModuleJobs.Pop(EAllowShrinking::No);

					for (const auto& Section : ModulePtr->Code)
					{
						FilesToHotReload.Add(FFilenamePair{ Section.AbsoluteFilename, Section.RelativeFilename, Section.VirtualPath });
					}

					if (auto DependentModulesPtr = ReverseDeps.Find(ModulePtr))
					{
						for (auto ModuleDepPtr : *DependentModulesPtr)
						{
							if (!VisitedModules.Contains(ModuleDepPtr))
							{
								VisitedModules.Add(ModuleDepPtr);
								ModuleJobs.Push(ModuleDepPtr);
							}
						}
					}
				}
			}
		}
	}

	// Mark all needed files for preprocessing
	for (const auto& PathPair : FilesToHotReload)
	{
		const bool bTreatAsDeleted = AlreadyDeletedFiles.Num() != 0 && AlreadyDeletedFiles.Contains(PathPair);
		Preprocessor.AddSource(
			AngelscriptEngineSourceProvider_Private::MakeSourceFromFilenamePair(PathPair),
			false,
			bTreatAsDeleted);
	}

	bool bPreprocessSuccess = Preprocessor.Preprocess();
	if (!bPreprocessSuccess)
	{
		UE_LOG(Angelscript, Error, TEXT("Hot reload failed in preprocessing. Keeping all old angelscript code."));
		PreviouslyFailedReloadFiles.Append(FileList);
		EmitDiagnostics();
		return false;
	}

	// Notify for progress after preprocessor
	SlowTask.EnterProgressFrame(2.5f);

	// Preprocessing is side-effect free for the active VM. Once it has
	// succeeded, retire any leaf that still owns objects from a module which
	// is about to be replaced. This deliberately happens before class
	// generation swaps the module so AfterEach/teardown callbacks still
	// execute against the generation that created the leaf.
	TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile =
		Preprocessor.GetModulesToCompile();
	TSet<FString> ReloadedModuleNames;
	ReloadedModuleNames.Reserve(ModulesToCompile.Num());
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ModulesToCompile)
	{
		ReloadedModuleNames.Add(Module->ModuleName);
	}
	if (!ReloadedModuleNames.IsEmpty())
	{
		FAngelscriptScriptTestRunner::CancelModules(
			ReloadedModuleNames,
			TEXT("the owning AngelScript module is being hot reloaded"));
#if WITH_DEV_AUTOMATION_TESTS
		FAngelscriptScriptTestAutomation::Get().
			CancelModulesBeforeReload(ReloadedModuleNames);
#endif
		if (ScriptTestHotReloadRunner != nullptr)
		{
			ScriptTestHotReloadRunner->CancelModulesBeforeReload(
				ReloadedModuleNames);
		}
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
	FAngelscriptCompileOptions CompileOptions;
	CompileOptions.CachePolicy =
		EAngelscriptCompileCachePolicy::ForceClean;
	CompileOptions.bRejectStructuralChanges = bRejectStructuralChanges;
	TOptional<FAngelscriptCacheCompileCaptureContext> CacheCaptureContext;
	if (!ModulesToCompile.IsEmpty())
	{
		FAngelscriptCacheCompileCaptureContext PreparedContext;
		const bool bSkipDevelopmentScripts = !ShouldUseEditorScripts();
		const FAngelscriptCacheCompileCapturePreparationResult Preparation =
			PrepareAngelscriptCacheCompileCaptureContext(
				*this,
				PreprocessorContext,
				*Dependencies.SourceProvider,
				GetEffectiveScriptRootDescriptors(),
				bSkipDevelopmentScripts,
				bSkipDevelopmentScripts,
				PreparedContext);
		if (Preparation.IsSuccess())
		{
			CacheCaptureContext.Emplace(MoveTemp(PreparedContext));
		}
		else
		{
			UE_LOG(Angelscript, Warning,
				TEXT("[CacheV2] Hot reload capture preparation skipped: Error=%u Environment=%u Discovery=%u Validation=%u Detail=%s"),
				static_cast<uint32>(Preparation.Error),
				static_cast<uint32>(Preparation.EnvironmentError),
				static_cast<uint32>(Preparation.SourceDiscoveryError),
				static_cast<uint32>(Preparation.Validation.Error),
				*Preparation.Detail);
		}
	}
	ECompileResult Result =
		CompileModules(
			CompileType,
			ModulesToCompile,
			CompiledModules,
			CompileOptions,
			CacheCaptureContext.IsSet() ? &CacheCaptureContext.GetValue() : nullptr);
	if (OutCompileResult != nullptr)
	{
		*OutCompileResult = Result;
	}
	if (Result == ECompileResult::ErrorNeedFullReload)
	{
		return false;
	}
	else if (Result == ECompileResult::Error)
	{
		return false;
	}

	// In the scenario where the initial compile fails and the user presses "Try Again" GEngine is nullptr.
	// Since the unit tests assumes an existing GEngine we need to skip the unit testing in that case.
	// Asset Manager initial scan should be completed also to queue tests for executing after hot reload finishes.
	if(Result == ECompileResult::FullyHandled || Result == ECompileResult::PartiallyHandled)
	{
		FAngelscriptPostCompileClassCollection& PostCompileDelegate = GetPostCompileClassCollection();
			if (PostCompileDelegate.IsBound())
				PostCompileDelegate.Broadcast(CompiledModules);

#if WITH_DEV_AUTOMATION_TESTS
		if (bIsInitialCompileFinished && bCompletedAssetScan)
		{
			// Publish exactly once at the successful hot-reload transaction
			// boundary. Failed preprocess/compile paths retain the previous
			// immutable registry generation.
			DiscoverTests();
			if (GIsEditor
				&& GEngine != nullptr
				&& ScriptTestHotReloadRunner != nullptr
				&& ScriptTestHotReloadRunner->
					ShouldRunTestsOnHotReload())
			{
				ScriptTestHotReloadRunner->PrepareTests(
					CompiledModules);
			}
		}
#endif
	}

#if WITH_AS_DEBUGSERVER
	// Make sure all our breakpoints are applied to modules that might be newly compiled now
	if (DebugServer != nullptr)
		DebugServer->ReapplyBreakpoints();
#endif

	return true;
}

bool FAngelscriptEngine::VerifyPropertySpecifiers(const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules)
{
	bool bPassedVerification = true;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
	{
		for (const TSharedRef<FAngelscriptClassDesc>& Class : Module->Classes)
		{
			for (const TSharedRef<FAngelscriptPropertyDesc>& Property : Class->Properties)
			{
				FString* RepNotifyFunc = Property->Meta.Find(NAME_ReplicatedUsing);
				bPassedVerification &= VerifyRepFunc(RepNotifyFunc, Property, Class, Module);

				FString* BlueprintSetFunc = Property->Meta.Find(NAME_BlueprintSetter);
				bPassedVerification &= VerifyBlueprintSetFunc(BlueprintSetFunc, Property, Class, Module);

				FString* BlueprintGetFunc = Property->Meta.Find(NAME_BlueprintGetter);
				bPassedVerification &= VerifyBlueprintGetFunc(BlueprintGetFunc, Property, Class, Module);
			}
		}
	}

	return bPassedVerification;
}

bool FAngelscriptEngine::VerifyRepFunc(FString* FuncName, const TSharedRef<FAngelscriptPropertyDesc>& Property,
	const TSharedRef<FAngelscriptClassDesc>& Class, const TSharedRef<FAngelscriptModuleDesc>& Module)
{
	if (FuncName != nullptr)
	{
		auto FuncDesc = Class->GetMethod(*FuncName);
		// First make sure we can find the method
		if (!FuncDesc.IsValid())
		{
			ScriptCompileError(Module, Property->LineNumber,
				FString::Printf(TEXT("The function '%s' which is specified for %s on property %s::%s "
									 "could not be found within the script class. (It also has to be UFUNCTION())"),
					**FuncName,
					*Property->PropertyName,
					*Class->ClassName,
					*Property->PropertyName
				)
			);
			return false;
		}

		// Having an ReplicatedUsing callback with 0 arguments is OK, we only need to verify the argument if we actually
		// have one!
		if (FuncDesc->Arguments.Num() > 0)
		{
			if (FuncDesc->Arguments.Num() > 1)
			{
				ScriptCompileError(Module, FuncDesc->LineNumber,
					FString::Printf(TEXT("The function '%s' which is specified for ReplicatedUsing on property %s::%s "
										 "can not have more than 1 argument."),
						**FuncName,
						*Class->ClassName,
						*Property->PropertyName
					)
				);
				return false;
			}

			const FAngelscriptTypeUsage& FuncArgType = FuncDesc->Arguments[0].Type;
			const FAngelscriptTypeUsage& PropType = Property->PropertyType;

			// The type of the argument in the function has to be the same as the type of the variable we're
			// replicating!
			if (!FuncArgType.EqualsUnqualified(PropType))
			{
				ScriptCompileError(Module, FuncDesc->LineNumber,
					FString::Printf(TEXT("The function '%s' which is specified for ReplicatedUsing on property %s::%s "
										 "takes an argument of type '%s', but the value replicated is of type '%s'."),
						**FuncName,
						*Class->ClassName,
						*Property->PropertyName,
						*FuncArgType.GetFriendlyTypeName(),
						*PropType.GetFriendlyTypeName()
					)
				);
				return false;
			}
		}
	}
	return true;
}

bool FAngelscriptEngine::VerifyBlueprintSetFunc(FString* FuncName,
	const TSharedRef<FAngelscriptPropertyDesc>& Property, const TSharedRef<FAngelscriptClassDesc>& Class,
	const TSharedRef<FAngelscriptModuleDesc>& Module)
{
	if (FuncName != nullptr)
	{
		auto FuncDesc = Class->GetMethod(*FuncName);
		// First make sure we can find the method
		if (!FuncDesc.IsValid())
		{
			ScriptCompileError(Module, Property->LineNumber,
				FString::Printf(TEXT("The function '%s' which is specified for BlueprintSetter on property %s::%s "
									 "could not be found within the script class. (It also has to be UFUNCTION())"),
					**FuncName,
					*Class->ClassName,
					*Property->PropertyName
				)
			);
			return false;
		}

		// Having an BlueprintSetter callback requires a func with one argument matching the property type
		if (FuncDesc->Arguments.Num() == 1)
		{
			const FAngelscriptTypeUsage& FuncArgType = FuncDesc->Arguments[0].Type;
			const FAngelscriptTypeUsage& PropType = Property->PropertyType;

			// The type of the argument in the function has to be the same as the type of the variable we're
			// replicating!
			if (!FuncArgType.EqualsUnqualified(PropType))
			{
				ScriptCompileError(Module, FuncDesc->LineNumber,
					FString::Printf(TEXT("The function '%s' which is specified for BlueprintSetter on property %s::%s "
										 "takes an argument of type '%s', but the value written is of type '%s'."),
						**FuncName,
						*Class->ClassName,
						*Property->PropertyName,
						*FuncArgType.GetFriendlyTypeName(),
						*PropType.GetFriendlyTypeName()
					)
				);
				return false;
			}
		}
		else
		{
			ScriptCompileError(Module, Property->LineNumber,
				FString::Printf(TEXT("The function '%s' which is specified for BlueprintSetter on property %s::%s "
									 "should take exactly 1 argument."),
					**FuncName,
					*Class->ClassName,
					*Property->PropertyName
				)
			);
			return false;
		}
	}
	return true;
}

bool FAngelscriptEngine::VerifyBlueprintGetFunc(FString* FuncName,
	const TSharedRef<FAngelscriptPropertyDesc>& Property, const TSharedRef<FAngelscriptClassDesc>& Class,
	const TSharedRef<FAngelscriptModuleDesc>& Module)
{
	if (FuncName != nullptr)
	{
		auto FuncDesc = Class->GetMethod(*FuncName);
		// First make sure we can find the method
		if (!FuncDesc.IsValid())
		{
			ScriptCompileError(Module, Property->LineNumber,
				FString::Printf(TEXT("The function '%s' which is specified for BlueprintGetter on property %s::%s "
									 "could not be found within the script class. (It also has to be UFUNCTION())"),
					**FuncName,
					*Class->ClassName,
					*Property->PropertyName
				)
			);
			return false;
		}

		// BlueprintGetters need to be BlueprintPure
		if (!FuncDesc->bBlueprintPure)
		{
			ScriptCompileError(Module, Property->LineNumber,
				FString::Printf(TEXT("The function '%s' which is specified for BlueprintGetter on property %s::%s "
									 "needs to be marked as BlueprintPure."),
					**FuncName,
					*Class->ClassName,
					*Property->PropertyName
				)
			);
			return false;
		}

		// Having an BlueprintGetter callback requires a function with 0 arguments
		if (FuncDesc->Arguments.Num() == 0)
		{
			const FAngelscriptTypeUsage& FuncRetType = FuncDesc->ReturnType;
			const FAngelscriptTypeUsage& PropType = Property->PropertyType;

			// The type of the argument in the function has to be the same as the type of the variable we're
			// replicating!
			if (!FuncRetType.EqualsUnqualified(PropType))
			{
				FString FriendlyTypeName = FuncRetType.Type == NULL ? FString("void") : *FuncRetType.GetFriendlyTypeName();
				ScriptCompileError(Module, FuncDesc->LineNumber,
					FString::Printf(TEXT("The function '%s' which is specified for BlueprintGetter on property %s::%s "
										 "returns type '%s', but the value read is of type '%s'."),
						**FuncName,
						*Class->ClassName,
						*Property->PropertyName,
						*FriendlyTypeName,
						*PropType.GetFriendlyTypeName()
					)
				);
				return false;
			}
		}
		else
		{
			ScriptCompileError(Module, Property->LineNumber,
				FString::Printf(TEXT("The function '%s' which is specified for BlueprintGetter on property %s::%s "
									 "should not take any arguments."),
					**FuncName,
					*Class->ClassName,
					*Property->PropertyName
				)
			);
			return false;
		}
	}
	return true;
}

void FAngelscriptEngine::CheckForHotReload(ECompileType CompileType)
{
	ProcessQueuedHotReload(CompileType, nullptr, nullptr);
}

bool FAngelscriptEngine::ForceCleanCacheModules(
	const TConstArrayView<FString> CanonicalModuleNames,
	ECompileResult& OutCompileResult)
{
	OutCompileResult = ECompileResult::Error;
#if !AS_CAN_HOTRELOAD
	return false;
#else
	if (bIsHotReloading)
	{
		return false;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> SelectedModules;
	if (CanonicalModuleNames.IsEmpty())
	{
		SelectedModules = GetActiveModules();
	}
	else
	{
		for (const FString& ModuleName : CanonicalModuleNames)
		{
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				GetModuleByModuleName(ModuleName);
			if (Module.IsValid())
			{
				SelectedModules.AddUnique(Module.ToSharedRef());
			}
		}
	}
	SelectedModules.Sort([](
		const TSharedRef<FAngelscriptModuleDesc>& Left,
		const TSharedRef<FAngelscriptModuleDesc>& Right)
	{
		return Left->ModuleName < Right->ModuleName;
	});

	TArray<FFilenamePair> Files;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : SelectedModules)
	{
		for (const FAngelscriptModuleDesc::FCodeSection& Section : Module->Code)
		{
			Files.AddUnique(FFilenamePair{
				Section.AbsoluteFilename,
				Section.RelativeFilename,
				Section.VirtualPath});
		}
	}
	if (Files.IsEmpty())
	{
		return false;
	}
	for (const FFilenamePair& File : Files)
	{
		FileChangesDetectedForReload.AddUnique(File);
	}
	return ProcessQueuedHotReload(
		ECompileType::FullReload,
		&OutCompileResult,
		nullptr);
#endif
}

bool FAngelscriptEngine::ProcessQueuedHotReload(
	ECompileType CompileType,
	ECompileResult* OutCompileResult,
	TArray<FFilenamePair>* OutConsumedFiles,
	const bool bRejectStructuralChanges)
{
	// A test callback can itself modify a script file (for example through a
	// helper command). Never consume the queued changes while script code is
	// still on the stack; the next engine tick will process the same queue.
	if (FAngelscriptScriptTestRunner::IsExecutingScriptCallback())
		return false;

	if (bUseHotReloadCheckerThread)
	{
		// Still waiting for hot reload results to come back,
		// so don't do anything for now.
		if (bWaitingForHotReloadResults)
			return false;
	}

	// Check if anything is queued for hot reload
	TArray<FFilenamePair> FileList;

	FileList.Append(FileChangesDetectedForReload);
	FileChangesDetectedForReload.Empty();

	// If any files were deleted, this should also cause a hotreload
	// We delay hotreloads from deletions slightly so if this was a rename instead of a delete we will see the new file right away
	if (FileList.Num() != 0 || FPlatformTime::Seconds() - LastFileChangeDetectedTime > 0.2)
	{
		for (const auto& DeletedFile : FileDeletionsDetectedForReload)
			FileList.AddUnique(DeletedFile);
		FileDeletionsDetectedForReload.Empty();
	}

	if (CompileType != ECompileType::SoftReloadOnly)
	{
		for (const auto& QueuedFile : QueuedFullReloadFiles)
			FileList.AddUnique(QueuedFile);
		QueuedFullReloadFiles.Empty();
	}

	if (FileList.Num() != 0)
	{
		UE_LOG(Angelscript, Log, TEXT("Primary engine consuming %d queued script file change(s) for hot reload."), FileList.Num());

		if (OutConsumedFiles != nullptr)
		{
			*OutConsumedFiles = FileList;
		}

		// The background scanner or explicit packaged request gave us work;
		// compilation and activation still happen on this game-thread safe point.
		PerformHotReload(
			CompileType,
			FileList,
			OutCompileResult,
			bRejectStructuralChanges);
		if (bUseHotReloadCheckerThread)
		{
			bWaitingForHotReloadResults = true;
		}
		return true;
	}

	if (OutConsumedFiles != nullptr)
	{
		OutConsumedFiles->Reset();
	}
	if (bUseHotReloadCheckerThread)
	{
		bWaitingForHotReloadResults = true;
	}
	return false;
}

EAngelscriptRuntimeReloadRequestStatus
FAngelscriptEngine::RequestPackagedRuntimeReload()
{
#if !AS_CAN_HOTRELOAD
	return EAngelscriptRuntimeReloadRequestStatus::Disabled;
#else
	if (RuntimeConfig.bIsEditor
		|| PackagedRuntimeReloadMode ==
			EAngelscriptPackagedRuntimeReloadMode::Disabled)
	{
		return EAngelscriptRuntimeReloadRequestStatus::Disabled;
	}

	if (Engine == nullptr
		|| (CacheService.IsValid()
			&& CacheService->GetMutationPhase() ==
				EAngelscriptCacheMutationPhase::ShuttingDown))
	{
		return EAngelscriptRuntimeReloadRequestStatus::ShuttingDown;
	}

	if (bPackagedRuntimeReloadQueued
		|| bIsHotReloading
		|| CompletedPackagedRuntimeReloadResult.IsSet())
	{
		return EAngelscriptRuntimeReloadRequestStatus::Busy;
	}

	bPackagedRuntimeReloadQueued = true;
	return EAngelscriptRuntimeReloadRequestStatus::Queued;
#endif
}

bool FAngelscriptEngine::ConsumePackagedRuntimeReloadResult(
	FAngelscriptRuntimeReloadResult& OutResult)
{
	if (!CompletedPackagedRuntimeReloadResult.IsSet())
	{
		return false;
	}

	OutResult = MoveTemp(CompletedPackagedRuntimeReloadResult.GetValue());
	CompletedPackagedRuntimeReloadResult.Reset();
	return true;
}

void FAngelscriptEngine::PrimePackagedRuntimeReloadState()
{
#if AS_CAN_HOTRELOAD
	if (RuntimeConfig.bIsEditor
		|| PackagedRuntimeReloadMode ==
			EAngelscriptPackagedRuntimeReloadMode::Disabled
		|| bPackagedRuntimeReloadPrimed)
	{
		return;
	}

	// The first content scan establishes the baseline and must never present
	// every startup source as a post-startup edit.
	CheckForFileChanges();
	FileChangesDetectedForReload.Reset();
	FileDeletionsDetectedForReload.Reset();
	bPackagedRuntimeReloadPrimed = true;
	NextPackagedRuntimeReloadScan =
		FPlatformTime::Seconds() + PackagedRuntimeReloadScanIntervalSeconds;
#endif
}

void FAngelscriptEngine::CollectChangedModuleNames(
	const TArray<FFilenamePair>& Files,
	TArray<FString>& OutModuleNames) const
{
	OutModuleNames.Reset();
	for (const FFilenamePair& File : Files)
	{
		bool bMatchedActiveModule = false;
		for (const TPair<FString, TSharedRef<FAngelscriptModuleDesc>>& Pair :
			ActiveModules)
		{
			for (const auto& Section : Pair.Value->Code)
			{
				if (Section.RelativeFilename == File.RelativePath
					|| Section.AbsoluteFilename == File.AbsolutePath)
				{
					OutModuleNames.AddUnique(Pair.Value->ModuleName);
					bMatchedActiveModule = true;
					break;
				}
			}
		}

		// New files have no active descriptor yet. Their established module
		// convention is the relative filename without the .as suffix.
		if (!bMatchedActiveModule)
		{
			OutModuleNames.AddUnique(
				FPaths::GetBaseFilename(File.RelativePath));
		}
	}
	OutModuleNames.Sort();
}

void FAngelscriptEngine::TickPackagedRuntimeReload()
{
#if AS_CAN_HOTRELOAD
	if (RuntimeConfig.bIsEditor
		|| PackagedRuntimeReloadMode ==
			EAngelscriptPackagedRuntimeReloadMode::Disabled)
	{
		return;
	}

	PrimePackagedRuntimeReloadState();
	const double CurrentTime = FPlatformTime::Seconds();
	if (PackagedRuntimeReloadMode ==
			EAngelscriptPackagedRuntimeReloadMode::Automatic
		&& !bPackagedRuntimeReloadQueued
		&& !CompletedPackagedRuntimeReloadResult.IsSet()
		&& CurrentTime >= NextPackagedRuntimeReloadScan)
	{
		bPackagedRuntimeReloadQueued = true;
	}

	if (!bPackagedRuntimeReloadQueued
		|| bIsHotReloading
		|| FAngelscriptScriptTestRunner::IsExecutingScriptCallback())
	{
		return;
	}

	bPackagedRuntimeReloadQueued = false;
	NextPackagedRuntimeReloadScan =
		CurrentTime + PackagedRuntimeReloadScanIntervalSeconds;
	const double ReloadStartTime = FPlatformTime::Seconds();
	CheckForFileChanges();

	ECompileResult CompileResult = ECompileResult::Error;
	TArray<FFilenamePair> ConsumedFiles;
	bool bAttempted = false;
	if (!FileDeletionsDetectedForReload.IsEmpty())
	{
		// Removing a live module is structural by definition. Packaged Runtime
		// rejects the whole observed transaction without asking the preprocessor
		// to synthesize an unload against live UObject/class state.
		ConsumedFiles = FileChangesDetectedForReload;
		for (const FFilenamePair& Deleted : FileDeletionsDetectedForReload)
		{
			ConsumedFiles.AddUnique(Deleted);
		}
		FileChangesDetectedForReload.Reset();
		FileDeletionsDetectedForReload.Reset();
		CompileResult = ECompileResult::ErrorNeedFullReload;
		bAttempted = true;
	}
	else
	{
		bAttempted = ProcessQueuedHotReload(
			ECompileType::SoftReloadOnly,
			&CompileResult,
			&ConsumedFiles,
			true);
	}

	FAngelscriptRuntimeReloadResult Result;
	CollectChangedModuleNames(ConsumedFiles, Result.ChangedModuleNames);
	Result.RecompiledModuleCount = Result.ChangedModuleNames.Num();
	Result.CacheMissCount = bAttempted
		? Result.RecompiledModuleCount
		: 0;
	if (!bAttempted)
	{
		Result.Outcome = EAngelscriptRuntimeReloadOutcome::NoChanges;
		Result.Diagnostics = TEXT("No loose AngelScript source changes were detected.");
	}
	else if (CompileResult == ECompileResult::FullyHandled
		|| CompileResult == ECompileResult::PartiallyHandled)
	{
		Result.Outcome = EAngelscriptRuntimeReloadOutcome::AppliedCodeOnly;
		Result.Diagnostics = TEXT("The code-only generation was activated at the game-thread safe point.");
	}
	else if (CompileResult == ECompileResult::ErrorNeedFullReload)
	{
		Result.Outcome = EAngelscriptRuntimeReloadOutcome::RequiresRestart;
		Result.Diagnostics = TEXT("A structural change requires restart; the last good active generation was retained.");
	}
	else
	{
		Result.Outcome = EAngelscriptRuntimeReloadOutcome::CompileFailed;
		Result.Diagnostics = TEXT("Compilation failed; the last good active generation was retained.");
	}

	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> CoordinatePublication;
	TSharedPtr<const FAngelscriptCacheSuccessfulPublicationDto,
		ESPMode::ThreadSafe> IdentityPublication;
	if (CacheService.IsValid())
	{
		const FAngelscriptCacheLifecyclePublications Publications =
			CacheService->GetLifecyclePublications();
		if (Result.Outcome ==
			EAngelscriptRuntimeReloadOutcome::AppliedCodeOnly)
		{
			CoordinatePublication = Publications.Current;
		}
		else if (Result.Outcome ==
			EAngelscriptRuntimeReloadOutcome::RequiresRestart)
		{
			CoordinatePublication = Publications.PendingColdStart;
		}
		IdentityPublication = CoordinatePublication.IsValid()
			? CoordinatePublication
			: Publications.Current;
	}

	if (IdentityPublication.IsValid())
	{
		for (const FAngelscriptCacheCleanModuleArtifacts& Module :
			IdentityPublication->Modules)
		{
			if (Result.ChangedModuleNames.Contains(
				Module.CanonicalModuleName))
			{
				Result.ChangedModuleKeys.AddUnique(
					Module.ModuleKey.Hash.ToHexString());
			}
		}
		Result.ChangedModuleKeys.Sort();
	}

	if (CacheService.IsValid())
	{
		FAngelscriptCacheDecisionEvent Event;
		Event.Stage = EAngelscriptCacheDecisionStage::RuntimeReload;
		Event.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::RuntimeReload;
		Event.ReasonCode = static_cast<uint32>(Result.Outcome);
		switch (Result.Outcome)
		{
		case EAngelscriptRuntimeReloadOutcome::AppliedCodeOnly:
		case EAngelscriptRuntimeReloadOutcome::NoChanges:
			Event.Outcome = EAngelscriptCacheDecisionOutcome::Completed;
			break;
		case EAngelscriptRuntimeReloadOutcome::RequiresRestart:
			Event.Outcome = EAngelscriptCacheDecisionOutcome::Deferred;
			break;
		case EAngelscriptRuntimeReloadOutcome::CompileFailed:
			Event.Outcome = EAngelscriptCacheDecisionOutcome::Rejected;
			break;
		case EAngelscriptRuntimeReloadOutcome::Cancelled:
			Event.Outcome = EAngelscriptCacheDecisionOutcome::RolledBack;
			break;
		default:
			Event.Outcome = EAngelscriptCacheDecisionOutcome::Invalid;
			break;
		}
		if (IdentityPublication.IsValid())
		{
			for (const FAngelscriptCacheCleanModuleArtifacts& Module :
				IdentityPublication->Modules)
			{
				if (Result.ChangedModuleNames.Contains(
					Module.CanonicalModuleName))
				{
					Event.ModuleKeys.AddUnique(Module.ModuleKey);
				}
			}
		}
		if (CoordinatePublication.IsValid())
		{
			Event.TransactionOrdinal =
				CoordinatePublication->TransactionOrdinal;
			Event.Profile = CoordinatePublication->Profile;
			Event.SourceSnapshot = CoordinatePublication->SourceSnapshot;
			Event.CurrentCoordinate = CoordinatePublication->SourceSnapshot;
		}
		Event.PrimaryCount = Result.ChangedModuleNames.Num();
		Event.SecondaryCount = Result.RecompiledModuleCount;
		Event.ElapsedMicroseconds = static_cast<uint64>(FMath::Max(
			0.0,
			(FPlatformTime::Seconds() - ReloadStartTime) * 1000000.0));
		CacheService->RecordDecisionEvent(MoveTemp(Event));
	}

	UE_LOG(Angelscript, Display,
		TEXT("[RuntimeReload] Outcome=%u ChangedModules=%s ChangedKeys=%s Detail=%s"),
		static_cast<uint32>(Result.Outcome),
		*FString::Join(Result.ChangedModuleNames, TEXT(",")),
		*FString::Join(Result.ChangedModuleKeys, TEXT(",")),
		*Result.Diagnostics);
	CompletedPackagedRuntimeReloadResult.Emplace(MoveTemp(Result));
#endif
}

static bool HasGameWorld()
{
	for(const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		// Context.World() can be null when running with auto login in PIE
		if(Context.World() != nullptr && Context.World()->IsGameWorld())
		{
			return true;
		}
	}
	return false;
}

void FAngelscriptEngine::Tick(float DeltaTime)
{
	TickPackagedRuntimeReload();

#if AS_CAN_HOTRELOAD
	if (bScriptDevelopmentMode)
	{
		// In the scenario where the initial compile fails and the user press "Try Again" GEngine is nullptr.
		// Since the unit tests assumes an existing GEngine we need to skip the unit testing in that case.
		if (GEngine != nullptr
			&& ScriptTestHotReloadRunner != nullptr)
		{
			const bool bAllScriptTestsPass =
				ScriptTestHotReloadRunner->RunTests(this);
			if (!bAllScriptTestsPass)
			{
				EmitDiagnostics();
			}
		}

		// Only check for hot reloads periodically instead of every tick
		if (bUseHotReloadCheckerThread)
		{
			double CurrentTime = FPlatformTime::Seconds();
			if (NextHotReloadCheck > CurrentTime && !bWaitingForHotReloadResults)
				return;
			NextHotReloadCheck = CurrentTime + 0.1;
		}

		// If we're in PIE or cooked, only soft reloads are allowed,
		// otherwise we can do a full reload and reinstantiate all
		// editor objects using unreal's hot reload mechanisms.
		if (!GIsEditor || HasGameWorld())
		{
			CheckForHotReload(ECompileType::SoftReloadOnly);
		}
		else
		{
			CheckForHotReload(ECompileType::FullReload);
		}
	}
#endif

#if WITH_AS_DEBUGSERVER
	if(DebugServer != nullptr)
		DebugServer->Tick();
#endif

	// If this is ever not null during this tick, something changed
	// the world context without resetting it. Very bad
	UE_CLOG(GAmbientWorldContext != nullptr, Angelscript, Fatal, TEXT("Angelscript world context was improperly restored after use!"));
}

bool FAngelscriptEngine::ShouldTick() const
{
	return Engine != nullptr;
}

void FAngelscriptEngine::CheckForFileChanges()
{
	ensure(bUseHotReloadCheckerThread
		|| PackagedRuntimeReloadMode !=
			EAngelscriptPackagedRuntimeReloadMode::Disabled);

#if AS_PRINT_STATS
	double StartCompute = FPlatformTime::Seconds();
#endif

	// Clear any previous data we had
	FileChangesDetectedForReload.Empty();

	// Check all files in script directory for hot reload need
	auto& FileManager = IFileManager::Get();

	TArray<FFilenamePair> Filenames;
	FindAllScriptFilenames(Filenames);
	TSet<FString> SeenSourceStateKeys;
	SeenSourceStateKeys.Reserve(Filenames.Num());

	for (FFilenamePair& Filename : Filenames)
	{
		const FAngelscriptSource Source = AngelscriptEngineSourceProvider_Private::MakeSourceFromFilenamePair(Filename);

		FAngelscriptSourceState SourceState;
		const bool bHasSourceState = Dependencies.SourceProvider.IsValid()
			&& Dependencies.SourceProvider->QuerySourceState(Source, SourceState);
		const FString SourceStateKey = MakeSourceStateKey(Filename);
		SeenSourceStateKeys.Add(SourceStateKey);

		FHotReloadState* FileState = FileHotReloadState.Find(SourceStateKey);
		if (FileState == nullptr)
		{
			// File didn't exist before, so definitely hot reload it
			FileChangesDetectedForReload.Add(Filename);

			FHotReloadState NewState;
			if (bHasSourceState)
			{
				NewState.LastChange = SourceState.Timestamp;
				NewState.ContentHash = SourceState.ContentHash;
				NewState.bHasContentHash = SourceState.bHasContentHash;
			}
			else
			{
				NewState.LastChange = FileManager.GetTimeStamp(*Filename.AbsolutePath);
			}
			NewState.Filename = Filename;
			FileHotReloadState.Add(SourceStateKey, NewState);
		}
		else if (bHasSourceState)
		{
			const bool bHasComparableHash = FileState->bHasContentHash && SourceState.bHasContentHash;
			const bool bShouldReload = bHasComparableHash
				? FileState->ContentHash != SourceState.ContentHash
				: FileState->LastChange != SourceState.Timestamp;
			if (bShouldReload)
			{
				FileChangesDetectedForReload.Add(Filename);
			}

			FileState->LastChange = SourceState.Timestamp;
			FileState->ContentHash = SourceState.ContentHash;
			FileState->bHasContentHash = SourceState.bHasContentHash;
			FileState->Filename = Filename;
		}
		else
		{
			const FDateTime FileTime = FileManager.GetTimeStamp(*Filename.AbsolutePath);
			if (FileTime != FileState->LastChange)
			{
				// File on disk is newer, queue reload
				FileChangesDetectedForReload.Add(Filename);
				FileState->LastChange = FileTime;
			}
			FileState->Filename = Filename;
		}
	}

	TArray<FString> MissingSourceStateKeys;
	for (const TPair<FString, FHotReloadState>& Pair : FileHotReloadState)
	{
		if (!SeenSourceStateKeys.Contains(Pair.Key))
		{
			FileDeletionsDetectedForReload.AddUnique(Pair.Value.Filename);
			MissingSourceStateKeys.Add(Pair.Key);
		}
	}
	for (const FString& MissingKey : MissingSourceStateKeys)
	{
		FileHotReloadState.Remove(MissingKey);
	}

	if (!FileChangesDetectedForReload.IsEmpty()
		|| !FileDeletionsDetectedForReload.IsEmpty())
	{
		LastFileChangeDetectedTime = FPlatformTime::Seconds();
	}

#if AS_PRINT_STATS
	double EndCompute = FPlatformTime::Seconds();
	if (FileChangesDetectedForReload.Num() != 0
		|| FileDeletionsDetectedForReload.Num() != 0)
	{
		UE_LOG(Angelscript, Log, TEXT("scanning for changed files took %.3f ms"), (EndCompute - StartCompute) * 1000);
	}
#endif
}

void FAngelscriptEngine::SwapInModules(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules, TArray<TSharedRef<FAngelscriptModuleDesc>>& DiscardedModules)
{
	FAngelscriptScopeTimer PostTimer(TEXT("new module swap-in"));

	for (auto Module : Modules)
	{
		// Mark old modules as outdated
		const FString InternalModuleName = MakeModuleName(Module->ModuleName);
		auto* OldModule = ActiveModules.Find(InternalModuleName);
		if (OldModule != nullptr)
		{
			DiscardedModules.Add(*OldModule);

			// Need to rename the angelscript module to something so
			// we don't look it up later.
			FString TrashName = FString::Printf(TEXT("%s_OLD_%d"), *InternalModuleName, TempNameIndex);
			TempNameIndex += 1;

			if ((*OldModule)->ScriptModule)
			{
				SetOutdated((*OldModule)->ScriptModule);
				(*OldModule)->ScriptModule->SetName(TCHAR_TO_ANSI(*TrashName));
			}
		}

		if (Module->ScriptModule != nullptr)
		{
			// Rename the new module to the right name
			Module->ScriptModule->SetName(TCHAR_TO_ANSI(*InternalModuleName));
		}

		ActiveModules.Add(InternalModuleName, Module);
	}

	// Update dependencies for discarded modules
	for (auto OldModule : DiscardedModules)
	{
		if (OldModule->ScriptModule)
			ModulesByScriptModule.Remove(OldModule->ScriptModule);
	}

#if AS_CAN_HOTRELOAD
	// Rebuild the full list of all active types
	ActiveClassesByName.Reset();
	ActiveDelegatesByName.Reset();
	ActiveEnumsByName.Reset();

	for (auto ModuleElem : ActiveModules)
	{
		auto Module = ModuleElem.Value;
		for (auto Class : Module->Classes)
		{
			ActiveClassesByName.Add(Class->ClassName,
				TPair<TSharedPtr<FAngelscriptModuleDesc>,TSharedPtr<FAngelscriptClassDesc>>{
					Module, Class
			});
		}

		for (auto Delegate : Module->Delegates)
		{
			ActiveDelegatesByName.Add(Delegate->DelegateName,
				TPair<TSharedPtr<FAngelscriptModuleDesc>,TSharedPtr<FAngelscriptDelegateDesc>>{
					Module, Delegate
			});
		}

		for (auto Enum : Module->Enums)
		{
			ActiveEnumsByName.Add(Enum->EnumName,
				TPair<TSharedPtr<FAngelscriptModuleDesc>,TSharedPtr<FAngelscriptEnumDesc>>{
					Module, Enum
			});
		}
	}

#endif
}

#if !UE_BUILD_SHIPPING
void FAngelscriptEngine::GetOnScreenMessages(TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText>& OutMessages)
{
#if AS_CAN_HOTRELOAD
	// This shouldn't be translated so... :) FromString it is.
	const static FText CompileErrorText = FText::FromString(TEXT("ANGELSCRIPT HOT-RELOAD FAILED -- KEEPING OLD CODE"));

	// If the previous hot-reload failed, display a useful message bejbi
	if (PreviouslyFailedReloadFiles.Num())
		OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Error, CompileErrorText);
#endif
}
#endif

TSharedPtr<struct FAngelscriptModuleDesc> FAngelscriptEngine::GetModule(asIScriptModule* Module)
{
	TSharedPtr<struct FAngelscriptModuleDesc>* FoundModule = ModulesByScriptModule.Find(Module);
	if (FoundModule == nullptr)
		return nullptr;
	return *FoundModule;
}

TSharedPtr<FAngelscriptModuleDesc> FAngelscriptEngine::GetModuleByModuleName(const FString& ModuleName)
{
	auto ModulePtr = GetModule(ModuleName);
	if (ModulePtr.IsValid())
	{
		return ModulePtr;
	}

	return TSharedPtr<struct FAngelscriptModuleDesc>();
}

TSharedPtr<struct FAngelscriptModuleDesc> FAngelscriptEngine::GetModuleByFilenameOrModuleName(const FString& Filename, const FString& ModuleName)
{
	auto Module = GetModuleByFilename(Filename);
	if (Module.IsValid())
	{
		return Module;
	}

	return GetModuleByModuleName(ModuleName);
}

TSharedPtr<struct FAngelscriptModuleDesc> FAngelscriptEngine::GetModuleByFilename(const FString& Filename)
{
	for (const TPair<FString, TSharedRef<FAngelscriptModuleDesc>>& It : ActiveModules)
	{
		const TSharedRef<FAngelscriptModuleDesc>& Module = It.Value;
		for (const FAngelscriptModuleDesc::FCodeSection& Section : Module->Code)
		{
			if (Section.AbsoluteFilename.Equals(Filename, ESearchCase::IgnoreCase))
			{
				return Module;
			}
		}
	}

	for (auto RootDir : AllRootPaths)
	{
		RootDir += TEXT("/"); // Needed for MakePathRelativeTo to work

		FString ModuleName = Filename;
		MakePathRelativeTo_IgnoreCase(ModuleName, *RootDir);
		ModuleName = ModuleName.Replace(TEXT(".as"), TEXT("")).Replace(TEXT("/"), TEXT("."));

		auto ModulePtr = GetModule(ModuleName);
		if (ModulePtr.IsValid())
		{
			return ModulePtr;
		}
	}

	return TSharedPtr<struct FAngelscriptModuleDesc>();
}

namespace AngelscriptEngineCacheCapture_Private
{
	static TOptional<FAngelscriptStableModuleKey> FindModuleKey(
		const FAngelscriptModuleDesc& Module,
		const FAngelscriptCachedSourceIndex& SourceIndex)
	{
		TOptional<FAngelscriptStableModuleKey> Result;
		for (const FAngelscriptModuleDesc::FCodeSection& Section : Module.Code)
		{
			for (const FAngelscriptCachedSourceFile& SourceFile
				: SourceIndex.Files)
			{
				const FAngelscriptCachedSourceMount* SourceMount = nullptr;
				for (const FAngelscriptCachedSourceMount& CandidateMount
					: SourceIndex.Mounts)
				{
					if (CandidateMount.MountKey.Hash
						== SourceFile.MountKey.Hash)
					{
						SourceMount = &CandidateMount;
						break;
					}
				}
				if (SourceMount == nullptr)
				{
					continue;
				}

				FString ExpectedVirtualPath = SourceMount->LogicalMount;
				if (!ExpectedVirtualPath.EndsWith(TEXT("/")))
				{
					ExpectedVirtualPath += TEXT("/");
				}
				ExpectedVirtualPath += SourceFile.RelativeLogicalPath;
				if (!ExpectedVirtualPath.Equals(
					Section.VirtualPath, ESearchCase::CaseSensitive))
				{
					continue;
				}

				if (Result.IsSet()
					&& Result.GetValue() != SourceFile.ModuleKey)
				{
					return {};
				}
				Result = SourceFile.ModuleKey;
				break;
			}
		}
		return Result;
	}
}

ECompileResult FAngelscriptEngine::CompileModules(
	ECompileType CompileType,
	const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& InModules,
	TArray<TSharedRef<FAngelscriptModuleDesc>>& OutCompiledModules,
	FAngelscriptCompileOptions CompileOptions,
	const FAngelscriptCacheCompileCaptureContext* CacheCaptureContext,
	FAngelscriptCacheCompileReuseContext* CacheReuseContext)
{
	AS_PERF_SCOPE_COMPILE_MODULES();
	LLM_SCOPE_BYTAG(Angelscript);
	FAngelscriptCacheMutationGuard CacheMutationGuard;
	if (CacheCaptureContext != nullptr && CacheService.IsValid())
	{
		CacheMutationGuard = CacheService->EnterMutation(
			CompileType == ECompileType::Initial
				? EAngelscriptCacheMutationKind::InitialCompile
				: EAngelscriptCacheMutationKind::RuntimeReload);
		if (!CacheMutationGuard.IsEntered())
		{
			UE_LOG(Angelscript, Warning,
				TEXT("[CacheV2] Compile capture skipped because the per-Engine mutation gate was unavailable"));
		}
	}
	if (CompileOptions.IsForcedClean())
	{
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : InModules)
		{
			Module->PrecompiledData = nullptr;
			Module->bLoadedPrecompiledCode = false;
			Module->bLoadedIncrementalCache = false;
		}
	}
	FAngelscriptCompilationContext CompilationContext(
		CompileType,
		CompileOptions,
		InModules);

	if (FAngelscriptCompilationEvents::HasListeners())
	{
		FAngelscriptCompilationEvent BeginEvent;
		BeginEvent.Type = EAngelscriptCompilationEventType::CompileBegin;
		BeginEvent.Phase = TEXT("Compile.Begin");
		BeginEvent.CompilationRunId = CompilationContext.GetRunId();
		BeginEvent.CompileType = CompilationContext.GetCompileType();
		CompilationContext.PopulateInputSummary(BeginEvent);
		FAngelscriptCompilationEvents::Broadcast(BeginEvent);
	}

	// We allocate from the memstack in the script compiler, so use a MemMark to deallocate everything at the end
	FMemMark MemoryMark(FMemStack::Get());

	FAngelscriptCompilationDelegate& PreCompileDelegate = GetPreCompile();
	if (PreCompileDelegate.IsBound())
		PreCompileDelegate.Broadcast();

	// Create progress indicator
	FScopedSlowTask SlowTask(3.f, FText::FromString(TEXT("Script Module Compilation")));
	if (CompileType == ECompileType::FullReload && bIsInitialCompileFinished)
		SlowTask.MakeDialogDelayed(0.5f);

	bool bWasFullyHandled = true;
	bool bHadCompileErrors = false;

	TMap<FString, TSharedRef<struct FAngelscriptModuleDesc>> CompilingModulesByName;
	TMap<FString, TSharedRef<struct FAngelscriptClassDesc>> CompilingClassesByName;

	TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
	TSet<TSharedRef<FAngelscriptModuleDesc>> ModulesToUpdateReferences;
	asModuleReferenceUpdateMap ScriptUpdateMap;

	const bool bUseRecompileAvoidance = (GAngelscriptRecompileAvoidance != 0)
		&& bIsInitialCompileFinished && ShouldUseAutomaticImportMethod();

	auto* ScriptEngine = (asCScriptEngine*)Engine;

	ScriptEngine->deferValidationOfTemplateTypes = true;
	ScriptEngine->deferCalculatingTemplateSize = true;
	ScriptEngine->RequestBuild();
	ScriptEngine->PrepareEngine();

	// Always compile every module in the list
	{
		FAngelscriptScopeTimer Timer(TEXT("script compilation total"));
		int32 ProgressUpdateCounter = 0;
		int32 ProgressUpdatesDone = 0;

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompilationQueue;
		bool bRecompiledAnyDependencies = false;

		// Queue up all changed modules for compilation
		CompilationQueue.Append(InModules);

		// Reset diagnostics for each module
		for (auto& Elem : Diagnostics)
			Elem.Value.bIsCompiling = false;

		// Recursively resolve all dependencies
		{
			FAngelscriptScopeTimer StageTimer(TEXT("script compilation stage1 and stage2"));
			while (CompilationQueue.Num() != 0)
			{
				TArray<TSharedRef<FAngelscriptModuleDesc>> CurrentCompileList = MoveTemp(CompilationQueue);
				CompilationQueue.Reset();

				// Initial setup for compilation
				for (int i = 0, Count = CurrentCompileList.Num(); i < Count; ++i)
				{
					auto Module = CurrentCompileList[i];

					// Setup diagnostic capture
					for (auto Section : Module->Code)
					{
						auto& Diag = Diagnostics.FindOrAdd(Section.AbsoluteFilename);
						Diag.Diagnostics.Reset();
						Diag.Filename = Section.AbsoluteFilename;
						Diag.bIsCompiling = true;
					}

					// Add it to a lookup table so imports can do fast lookups later
					ensureMsgf(!CompilingModulesByName.Contains(Module->ModuleName), TEXT("Duplicate module %s"), *Module->ModuleName);
					CompilingModulesByName.Add(Module->ModuleName, Module);
					CompiledModules.Add(Module);

					// Remove the old module so the script engine can no longer see it
					if (CompileType != ECompileType::Initial)
					{
						auto* OldModule = ActiveModules.Find(MakeModuleName(Module->ModuleName));
						if (OldModule != nullptr)
						{
							auto* OldScriptModule = (asCModule*)(*OldModule)->ScriptModule;
							if (OldScriptModule != nullptr)
								OldScriptModule->RemoveTypesAndGlobalsFromEngineAvailability();
						}
					}

					// Mark which classes we're compiling
					for (auto Class : Module->Classes)
						CompilingClassesByName.Add(Class->ClassName, Class);
				}


				// Stage 1
				for (int i = 0, Count = CurrentCompileList.Num(); i < Count; ++i)
				{
					auto Module = CurrentCompileList[i];

					// Update progress indicator
					if (--ProgressUpdateCounter <= 0 && ProgressUpdatesDone < 10)
					{
						SlowTask.EnterProgressFrame(0.025f);
						ProgressUpdateCounter = FMath::Max(CurrentCompileList.Num() / 10, 10);
						ProgressUpdatesDone += 1;
					}

					// Find all modules, either old or currently compiling, that we should import.
					bool bImportErrors = false;
					TArray<TSharedRef<FAngelscriptModuleDesc>> ImportedModules;

					if (!ShouldUseAutomaticImportMethod())
					{
						for (FString& ImportName : Module->ImportedModules)
						{
							TSharedPtr<FAngelscriptModuleDesc> FoundModule;
							auto* CompilingModule = CompilingModulesByName.Find(ImportName);
							if (CompilingModule != nullptr)
								FoundModule = *CompilingModule;
							if (!FoundModule.IsValid())
							{
								FoundModule = GetModuleByModuleName(ImportName);
							}

							if (FoundModule.IsValid())
							{
								ImportedModules.Add(FoundModule.ToSharedRef());
							}
							else
							{
								ScriptCompileError(Module, 1, FString::Printf(
									TEXT("Could not compile module %s: could not find module %s to import."),
									*Module->ModuleName, *ImportName));
								bImportErrors = true;
							}
						}
					}

					if (bImportErrors)
					{
						// Don't even try to compile if we couldn't import everything
						Module->bCompileError = true;
						bHadCompileErrors = true;
					}
					else
					{
						CompileModule_Types_Stage1(
							CompileType,
							Module,
							ImportedModules,
							CompileOptions);
					}

					AngelscriptEngineCompilationEvents_Private::BroadcastModuleEvent(
						EAngelscriptCompilationEventType::CompileModuleAssembly,
						TEXT("Compile.ModuleAssembly"),
						CompilationContext.GetRunId(),
						CompileType,
						CompilationContext.GetCachePolicy(),
						Module,
						!Module->bCompileError);
				}

				// In parallel, parse the script code
				int ModulesPerTask = 100;
				int TaskCount = 1 + CurrentCompileList.Num() / ModulesPerTask;
				ParallelFor(TaskCount, [&](int TaskIndex)
				{
					int StartIndex = (TaskIndex * ModulesPerTask);
					int EndIndex = FMath::Min(StartIndex + ModulesPerTask, CurrentCompileList.Num());
					for (int i = StartIndex; i < EndIndex; ++i)
					{
						auto Module = CurrentCompileList[i];
						asCModule* ScriptModule = Module->ScriptModule;
						if (ScriptModule == nullptr)
							continue;
						if (Module->bLoadedPrecompiledCode)
							continue;
						if (Module->bCompileError)
							continue;

						// Add types from the module into the engine
						auto Result = ScriptModule->builder->BuildParallelParseScripts();
						if (Result != asSUCCESS)
						{
							Module->bCompileError = true;
							bHadCompileErrors = true;
						}
					}
				});

				for (int i = 0, Count = CurrentCompileList.Num(); i < Count; ++i)
				{
					auto Module = CurrentCompileList[i];
					asCModule* ScriptModule = Module->ScriptModule;
					if (ScriptModule == nullptr)
						continue;
					if (Module->bLoadedPrecompiledCode)
						continue;

					AngelscriptEngineCompilationEvents_Private::BroadcastModuleEvent(
						EAngelscriptCompilationEventType::CompileModuleParse,
						TEXT("Compile.ModuleParse"),
						CompilationContext.GetRunId(),
						CompileType,
						CompilationContext.GetCachePolicy(),
						Module,
						!Module->bCompileError);
				}

				// Now that everything is parsed, generate the actual types
				for (auto Module : CompiledModules)
				{
					asCModule* ScriptModule = Module->ScriptModule;
					if (ScriptModule == nullptr)
						continue;
					if (Module->bLoadedPrecompiledCode)
						continue;
					if (Module->bCompileError)
						continue;

					// Add types from the module into the engine
					auto Result = ScriptModule->builder->BuildGenerateTypes();
					if (Result != asSUCCESS)
					{
						Module->bCompileError = true;
						bHadCompileErrors = true;
					}

					AngelscriptEngineCompilationEvents_Private::BroadcastModuleEvent(
						EAngelscriptCompilationEventType::CompileModuleGenerateTypes,
						TEXT("Compile.ModuleGenerateTypes"),
						CompilationContext.GetRunId(),
						CompileType,
						CompilationContext.GetCachePolicy(),
						Module,
						!Module->bCompileError);
				}

				// If parsing failed on one of our modules during a hotreload,
				// we want to ignore any non-parsing-related errors afterward.
				// This is so we don't spam errors in dependencies.
				if (bHadCompileErrors && CompileType != ECompileType::Initial)
					bIgnoreCompileErrorDiagnostics = true;

				// Stage 2
				for (int i = 0, Count = CurrentCompileList.Num(); i < Count; ++i)
				{
					auto Module = CurrentCompileList[i];

					// Update progress indicator
					if (--ProgressUpdateCounter <= 0 && ProgressUpdatesDone < 10)
					{
						SlowTask.EnterProgressFrame(0.025f);
						ProgressUpdateCounter = FMath::Max(CurrentCompileList.Num() / 10, 10);
						ProgressUpdatesDone += 1;
					}

					// Perform stage 2 of compilation
					CompileModule_Functions_Stage2(CompileType, Module);

					AngelscriptEngineCompilationEvents_Private::BroadcastModuleEvent(
						EAngelscriptCompilationEventType::CompileModuleGenerateFunctions,
						TEXT("Compile.ModuleGenerateFunctions"),
						CompilationContext.GetRunId(),
						CompileType,
						CompilationContext.GetCachePolicy(),
						Module,
						!Module->bCompileError);

					// Cancel out on a compile error if we're
					// doing a hot-reload compile.
					if (Module->bCompileError)
						bHadCompileErrors = true;
				}

				if (bUseRecompileAvoidance)
				{
					// Collect which types are updated to which for each module we compiled
					for (int i = 0, Count = CurrentCompileList.Num(); i < Count; ++i)
					{
						auto Module = CurrentCompileList[i];
						
						asCModule* ScriptModule = Module->ScriptModule;
						if (ScriptModule == nullptr)
							continue;

						// Link up the old and new script modules
						auto OldModule = ActiveModules.Find(MakeModuleName(Module->ModuleName));
						if (OldModule != nullptr)
						{
							auto* OldScriptModule = (*OldModule)->ScriptModule;
							check(OldScriptModule != nullptr);

							ScriptModule->CollectUpdatedTypeReferences(
								OldScriptModule,
								OUT ScriptUpdateMap);

							ScriptModule->ReloadOldModule = OldScriptModule;
							OldScriptModule->ReloadNewModule = ScriptModule;
						}
					}

					// Each module we just compiled should check whether there were structural changes
					for (int i = 0, Count = CurrentCompileList.Num(); i < Count; ++i)
					{
						auto Module = CurrentCompileList[i];
						
						asCModule* ScriptModule = Module->ScriptModule;
						if (ScriptModule == nullptr)
							continue;

						bool bHasStructuralChanges = false;

						// Link up the old and new script modules
						if (ScriptModule->ReloadOldModule != nullptr)
						{
							ScriptModule->DiffForReferenceUpdate(
								ScriptModule->ReloadOldModule,
								OUT ScriptUpdateMap,
								OUT bHasStructuralChanges);
						}
						else
						{
							bHasStructuralChanges = true;
						}

						if (bHasStructuralChanges)
							ScriptModule->ReloadState = asCModule::EReloadState::RecompiledWithStructuralChanges;
						else
							ScriptModule->ReloadState = asCModule::EReloadState::RecompiledOnlyCodeChanges;

						// Link up the old and new script modules
						if (ScriptModule->ReloadOldModule != nullptr)
							ScriptModule->ReloadOldModule->ReloadState = ScriptModule->ReloadState;
					}

					// Go through all existing modules that we aren't already recompiling, and mark them
					// appropriately based on what the dependencies are doing.
					bool bMarkedNewStructuralChanges = true;
					while (bMarkedNewStructuralChanges)
					{
						bMarkedNewStructuralChanges = false;

						for (auto ModuleElem : ActiveModules)
						{
							auto OldModule = ModuleElem.Value;
							asCModule* OldScriptModule = OldModule->ScriptModule;
							if (OldScriptModule == nullptr)
								continue;
							if (OldScriptModule->ReloadState == asCModule::EReloadState::RecompiledWithStructuralChanges)
								continue;
							if (OldScriptModule->ReloadState == asCModule::EReloadState::QueuedForCompilation)
								continue;

							// Code changes only compiles can get upgraded to structural compiles if any of our structural dependencies changed
							if (OldScriptModule->ReloadState == asCModule::EReloadState::RecompiledOnlyCodeChanges)
							{
								for (const auto& DependencyElem : OldScriptModule->moduleDependencies)
								{
									if (DependencyElem.Value.bIsStructuralDependency)
									{
										if (DependencyElem.Key->ReloadState == asCModule::EReloadState::RecompiledWithStructuralChanges)
										{
											// One of our structural dependencies ended up with a structural change, so we
											// definitely need to have a structural change on our end as well!
											OldScriptModule->ReloadState = asCModule::EReloadState::RecompiledWithStructuralChanges;
											OldScriptModule->ReloadNewModule->ReloadState = asCModule::EReloadState::RecompiledWithStructuralChanges;

											// We need to re-check all moduls after upgrading something to a structural change
											bMarkedNewStructuralChanges = true;

											break;
										}
									}
								}

								continue;
							}

							// If we haven't decided to compile it yet, we might choose to do so now
							check(OldScriptModule->ReloadState == asCModule::EReloadState::None 
								|| OldScriptModule->ReloadState == asCModule::EReloadState::UpdateReferences);

							bool bWantUpdateReferences = false;
							bool bTriggeredCompile = false;

							for (const auto& DependencyElem : OldScriptModule->moduleDependencies)
							{
								if (DependencyElem.Key->ReloadState == asCModule::EReloadState::RecompiledOnlyCodeChanges)
								{
									// If this is a hard value dependency we need to recompile anyway, even if it was only a code change
									if (DependencyElem.Value.bIsHardValueDependency || DependencyElem.Value.bIsStructuralDependency)
									{
										bTriggeredCompile = true;
										break;
									}
									else
									{
										// Otherwise we only update references and don't recompile
										bWantUpdateReferences = true;
									}
								}
								else if (DependencyElem.Key->ReloadState == asCModule::EReloadState::RecompiledWithStructuralChanges)
								{
									bTriggeredCompile = true;
									break;
								}
							}

							if (bTriggeredCompile)
							{
								// We no longer want to update references in the old module
								ModulesToUpdateReferences.Remove(OldModule);

								// Create a copy of the old module's preprocessor data so we can re-compile it
								TSharedRef<FAngelscriptModuleDesc> NewModule = MakeShared<FAngelscriptModuleDesc>(*OldModule);
								NewModule->ScriptModule = nullptr;
								NewModule->PrecompiledData = nullptr;
								NewModule->bCompileError = false;
								NewModule->bLoadedPrecompiledCode = false;
								NewModule->bLoadedIncrementalCache = false;
								NewModule->Classes.Reset();
								for (int ClassIndex = 0, ClassCount = OldModule->Classes.Num(); ClassIndex < ClassCount; ++ClassIndex)
								{
									TSharedRef<FAngelscriptClassDesc> OldClass = OldModule->Classes[ClassIndex];
									TSharedRef<FAngelscriptClassDesc> NewClass = MakeShared<FAngelscriptClassDesc>(*OldClass);
									NewClass->ScriptType = nullptr;
									NewClass->Class = nullptr;
									NewClass->Struct = nullptr;

									// This can have changed! Make sure we look this up again using the descriptors we are currently recompiling!
									// We need to check the whole inheritance tree, not just one step, in case we haven't propagated stuff yet!
									TSharedPtr<FAngelscriptClassDesc> Supermost = NewClass;
									while (!Supermost->bSuperIsCodeClass)
									{
										TSharedPtr<FAngelscriptClassDesc> CheckSuper;
										auto* CompilingSuper = CompilingClassesByName.Find(Supermost->SuperClass);
										if (CompilingSuper != nullptr)
											CheckSuper = *CompilingSuper;

										if (!CheckSuper.IsValid())
											CheckSuper = FAngelscriptEngine::Get().GetClass(Supermost->SuperClass);
										if (!CheckSuper.IsValid())
											break;

										Supermost = CheckSuper;
									}
									NewClass->CodeSuperClass = Supermost->CodeSuperClass;

									NewClass->Properties.Reset();
									for (int PropertyIndex = 0, PropertyCount = OldClass->Properties.Num(); PropertyIndex < PropertyCount; ++PropertyIndex)
									{
										TSharedRef<FAngelscriptPropertyDesc> OldProperty = OldClass->Properties[PropertyIndex];
										TSharedRef<FAngelscriptPropertyDesc> NewProperty = MakeShared<FAngelscriptPropertyDesc>(*OldProperty);
										NewProperty->ScriptPropertyIndex = -1;
										NewProperty->ScriptPropertyOffset = 0;
										NewProperty->bIsPrivate = false;
										NewProperty->bIsProtected = false;
										NewProperty->PropertyType = FAngelscriptTypeUsage();

										NewClass->Properties.Add(NewProperty);
									}

									NewClass->Methods.Reset();
									for (int MethodIndex = 0, MethodCount = OldClass->Methods.Num(); MethodIndex < MethodCount; ++MethodIndex)
									{
										TSharedRef<FAngelscriptFunctionDesc> OldMethod = OldClass->Methods[MethodIndex];
										TSharedRef<FAngelscriptFunctionDesc> NewMethod = MakeShared<FAngelscriptFunctionDesc>(*OldMethod);
										NewMethod->ScriptFunction = nullptr;
										NewMethod->bIsNoOp = false;
										NewMethod->bIsConstMethod = false;
										if (!NewMethod->OriginalFunctionName.IsEmpty())
										{
											NewMethod->FunctionName = MoveTemp(NewMethod->OriginalFunctionName);
											NewMethod->OriginalFunctionName.Reset();
										}
										NewMethod->bIsPrivate = false;
										NewMethod->bIsProtected = false;
										NewMethod->Arguments.Reset();
										NewMethod->ReturnType = FAngelscriptTypeUsage();

										NewClass->Methods.Add(NewMethod);
									}

									NewModule->Classes.Add(NewClass);
								}

								NewModule->Enums.Reset();
								for (int EnumIndex = 0, EnumCount = OldModule->Enums.Num(); EnumIndex < EnumCount; ++EnumIndex)
								{
									TSharedRef<FAngelscriptEnumDesc> OldEnum = OldModule->Enums[EnumIndex];
									TSharedRef<FAngelscriptEnumDesc> NewEnum = MakeShared<FAngelscriptEnumDesc>(*OldEnum);
									NewEnum->Enum = nullptr;
									NewEnum->ScriptType = nullptr;
									NewEnum->ValueNames.Reset();
									NewEnum->EnumValues.Reset();

									NewModule->Enums.Add(NewEnum);
								}

								NewModule->Delegates.Reset();
								for (int DelegateIndex = 0, DelegateCount = OldModule->Delegates.Num(); DelegateIndex < DelegateCount; ++DelegateIndex)
								{
									TSharedRef<FAngelscriptDelegateDesc> OldDelegate = OldModule->Delegates[DelegateIndex];
									TSharedRef<FAngelscriptDelegateDesc> NewDelegate = MakeShared<FAngelscriptDelegateDesc>(*OldDelegate);
									NewDelegate->ScriptType = nullptr;
									NewDelegate->Signature = nullptr;
									NewDelegate->Function = nullptr;

									NewModule->Delegates.Add(NewDelegate);
								}

								bRecompiledAnyDependencies = true;
								CompilationQueue.Add(NewModule);

								OldScriptModule->ReloadState = asCModule::EReloadState::QueuedForCompilation;
							}
							else if (bWantUpdateReferences)
							{
								// Add the module to have its references updated. We don't need to recompile at this point.
								ModulesToUpdateReferences.Add(OldModule);
								OldScriptModule->ReloadState = asCModule::EReloadState::UpdateReferences;
							}
						}
					}
				}
			}
		}

		// If any dependencies were compiled _later_ than the original batch, we need to do type reference replacement
		// The modules that were compiled first could be referencing the *old* versions of the types declared in
		// the modules that were compiled later, so we just do a big pass on all the type data.
		if (bRecompiledAnyDependencies || ModulesToUpdateReferences.Num() != 0)
		{
			FAngelscriptScopeTimer StageTimer(TEXT("script reload reference replacement"));

			// For each class that was recompiled, we need to check which template instances it had,
			// and then add all the functions and properties to the replacement list
			struct FTemplateReplacementHelper
			{
				static asCTypeInfo* CreateReplacementTemplateType(asCScriptEngine* Engine, asModuleReferenceUpdateMap& ScriptUpdateMap, asCObjectType* OldInstance)
				{
					asCTypeInfo* ReplacedInstance = ScriptUpdateMap.Types.FindRef(OldInstance);
					if (ReplacedInstance != nullptr)
					{
						check(ReplacedInstance->module != nullptr && ReplacedInstance->module->ReloadNewModule == nullptr);
						return ReplacedInstance;
					}

					asCArray<asCDataType> subTypes = OldInstance->templateSubTypes;

					bool bRequiresReplacement = false;
					for (int i = 0, Count = subTypes.GetLength(); i < Count; ++i)
					{
						if (auto* TypeInfo = (asCTypeInfo*)subTypes[i].GetTypeInfo())
						{
							if (auto* Replacement = ScriptUpdateMap.Types.FindRef(TypeInfo))
							{
								subTypes[i].SetTypeInfo(Replacement);
								bRequiresReplacement = true;

								check(Replacement->module != nullptr && Replacement->module->ReloadNewModule == nullptr);
							}
							else if (TypeInfo->flags & asOBJ_TEMPLATE)
							{
								// Need to recursively create subtypes that are also templates, because we
								// might not actually have gotten to this type yet.
								asCTypeInfo* NewSubTemplateInstance = CreateReplacementTemplateType(
									Engine, ScriptUpdateMap, (asCObjectType*)TypeInfo
								);

								if (NewSubTemplateInstance != nullptr)
								{
									bRequiresReplacement = true;
									subTypes[i].SetTypeInfo(NewSubTemplateInstance);

									check(NewSubTemplateInstance->module != nullptr && NewSubTemplateInstance->module->ReloadNewModule == nullptr);
								}
							}
							else
							{
								if (TypeInfo->module != nullptr && TypeInfo->module->ReloadNewModule != nullptr)
								{
									// This is an old type that no longer exists in the new module (compile errors?)
									// We should avoid replacement on this type entirely.
									return nullptr;
								}
							}
						}
					}

					if (!bRequiresReplacement)
						return nullptr;

					asCObjectType* NewInstance = Engine->GetTemplateInstanceType(OldInstance->templateBaseType, subTypes, nullptr);

					// Add the template type and all its functions and properties to the replacement map
					ScriptUpdateMap.Types.Add(OldInstance, NewInstance);

					// Keep the old instance alive, because we might want to replace it back if there's a compile error later
					OldInstance->AddRefInternal();
					ScriptUpdateMap.TemplateInstances.Add(OldInstance, NewInstance);

					check(OldInstance->properties.GetLength() == NewInstance->properties.GetLength());
					for (int i = 0, Count = OldInstance->properties.GetLength(); i < Count; ++i)
						ScriptUpdateMap.ObjectProperties.Add(OldInstance->properties[i], NewInstance->properties[i]);

					check(OldInstance->methods.GetLength() == NewInstance->methods.GetLength());
					for (int i = 0, Count = OldInstance->methods.GetLength(); i < Count; ++i)
					{
						asCScriptFunction* OldFunction = Engine->scriptFunctions[OldInstance->methods[i]];
						if (!OldFunction->traits.GetTrait(asTRAIT_GENERIC_TEMPLATE_FUNCTION))
						{
							asCScriptFunction* NewFunction = Engine->GenerateTemplateFunction(NewInstance, Engine->scriptFunctions[NewInstance->methods[i]]);
							ScriptUpdateMap.Functions.Add(OldFunction, NewFunction);
						}
					}

					check(OldInstance->beh.constructors.GetLength() == NewInstance->beh.constructors.GetLength());
					for (int i = 0, Count = OldInstance->beh.constructors.GetLength(); i < Count; ++i)
					{
						asCScriptFunction* OldFunction = Engine->scriptFunctions[OldInstance->beh.constructors[i]];
						if (!OldFunction->traits.GetTrait(asTRAIT_GENERIC_TEMPLATE_FUNCTION))
						{
							asCScriptFunction* NewFunction = Engine->GenerateTemplateFunction(NewInstance, Engine->scriptFunctions[NewInstance->beh.constructors[i]]);
							ScriptUpdateMap.Functions.Add(OldFunction, NewFunction);
						}
					}

					if (OldInstance->beh.destruct != 0)
					{
						asCScriptFunction* OldDestructor = Engine->scriptFunctions[OldInstance->beh.destruct];
						if (!OldDestructor->traits.GetTrait(asTRAIT_GENERIC_TEMPLATE_FUNCTION))
						{
							asCScriptFunction* NewDestructor = Engine->GenerateTemplateFunction(NewInstance, Engine->scriptFunctions[NewInstance->beh.destruct]);
							ScriptUpdateMap.Functions.Add(OldDestructor, NewDestructor);
						}
					}

					return NewInstance;
				};
			};

			// Generate new template instances
			for (auto Module : CompiledModules)
			{
				asCModule* ScriptModule = Module->ScriptModule;
				if (ScriptModule == nullptr)
					continue;
				if (ScriptModule->ReloadOldModule == nullptr)
					continue;

				for (int n = 0, Count = ScriptModule->ReloadOldModule->templateInstances.Num(); n < Count; ++n)
				{
					asCObjectType* OldInstance = ScriptModule->ReloadOldModule->templateInstances[n];
					FTemplateReplacementHelper::CreateReplacementTemplateType(Engine, ScriptUpdateMap, OldInstance);
				}
			}

			// If we decided to do reference replacement on any modules that were previously compiled, do so
			for (auto Module : ModulesToUpdateReferences)
			{
				asCModule* ScriptModule = Module->ScriptModule;
				if (ScriptModule == nullptr)
					continue;

				ScriptModule->UpdateReferencesInReflectionDataOnly(ScriptUpdateMap);
				UpdateScriptReferencesInUnrealData(ScriptUpdateMap, Module);
			}

			if (bRecompiledAnyDependencies)
			{
				// Replace all the old pointers with the new pointers
				for (auto Module : CompiledModules)
				{
					asCModule* ScriptModule = Module->ScriptModule;
					if (ScriptModule == nullptr)
						continue;

					ScriptModule->UpdateReferencesInReflectionDataOnly(ScriptUpdateMap);
				}
			}
		}

		{
			FAngelscriptScopeTimer StageTimer(TEXT("script compilation class layouting"));

			// Now that we have properly stage1&stage2'd the modules, and all modules have references to the newly generated types,
			// we make sure anything we've actually recompiled is properly layouted
			for (auto Module : CompiledModules)
			{
				asCModule* ScriptModule = Module->ScriptModule;
				if (ScriptModule == nullptr)
					continue;
				if (Module->bLoadedPrecompiledCode)
					continue;

				auto Result = ScriptModule->builder->BuildLayoutClasses();
				if (Result != asSUCCESS)
					Module->bCompileError = true;
			}

			// Now that class layouting is done, make sure all template types are layouted as well
			ScriptEngine->deferCalculatingTemplateSize = false;
			for (asCObjectType* tmpl : ScriptEngine->unvalidatedTemplateInstances)
				tmpl->CalculateTemplateSize();

			if (ModulesToUpdateReferences.Num() != 0)
			{
				// Create allocations for global variables after all classes are layouted
				for (auto Module : CompiledModules)
				{
					asCModule* ScriptModule = Module->ScriptModule;
					if (ScriptModule == nullptr)
						continue;
					if (Module->bLoadedPrecompiledCode)
						continue;

					ScriptModule->builder->BuildAllocateGlobalVariables();
				}

				// Add the actual memory for the global variables to the replacement list
				for (const auto& GlobalPropertyElement : ScriptUpdateMap.GlobalProperties)
				{
					asCGlobalProperty* OldProperty = GlobalPropertyElement.Key;
					asCGlobalProperty* NewProperty = GlobalPropertyElement.Value;

					ScriptUpdateMap.GlobalVariablePointers.Add(
						OldProperty->GetAddressOfValue(),
						NewProperty->GetAddressOfValue()
					);
				}

				// Now that all classes are layouted (and global variables have been allocated),
				// we can update references in old modules. We do this before we layout the functions,
				// because that will initialize global variables and could call into old functions.
				for (auto Module : ModulesToUpdateReferences)
				{
					asCModule* ScriptModule = Module->ScriptModule;
					if (ScriptModule == nullptr)
						continue;

					ScriptModule->UpdateReferencesInScriptBytecode(ScriptUpdateMap);
				}
			}

			// Functions also need to be layouted
			for (auto Module : CompiledModules)
			{
				asCModule* ScriptModule = Module->ScriptModule;
				if (ScriptModule == nullptr)
					continue;
				if (Module->bLoadedPrecompiledCode)
					continue;
				if (Module->bCompileError)
					continue;

				auto Result = ScriptModule->builder->BuildLayoutFunctions();
				if (Result != asSUCCESS)
					Module->bCompileError = true;
			}

			if (CacheReuseContext != nullptr
				&& CacheReuseContext->IsValid()
				&& CacheCaptureContext != nullptr
				&& !CompileOptions.IsForcedClean())
			{
				for (const TSharedRef<FAngelscriptModuleDesc>& Module
					: CompiledModules)
				{
					if (Module->bCompileError || Module->bLoadedPrecompiledCode
						|| Module->ScriptModule == nullptr
						|| Module->ScriptModule->builder == nullptr)
					{
						continue;
					}
					const TOptional<FAngelscriptStableModuleKey> ModuleKey =
						AngelscriptEngineCacheCapture_Private::FindModuleKey(
							*Module,
							CacheCaptureContext->AuthoritativeSourceIndex);
					if (!ModuleKey.IsSet())
					{
						continue;
					}
					const FAngelscriptCacheCompileReusePrepareResult Prepare =
						CacheReuseContext->PrepareModule(
							Module,
							ModuleKey.GetValue(),
							CacheCaptureContext->AuthoritativeSourceIndex.
								SourceSnapshot,
							CacheService.Get());
					if (Prepare.IsSuccess())
					{
						UE_LOG(Angelscript, Verbose,
							TEXT("[CacheV2][FunctionReuse] %s"),
							*Prepare.Detail);
					}
					else if (Prepare.Error
						!= EAngelscriptCacheCompileReusePrepareError::
							CandidateModuleMissing)
					{
						UE_LOG(Angelscript, Warning,
							TEXT("[CacheV2][FunctionReuse] Module=%s Error=%u Detail=%s"),
							*Module->ModuleName,
							static_cast<uint32>(Prepare.Error),
							*Prepare.Detail);
					}
				}
			}

			for (auto Module : CompiledModules)
			{
				asCModule* ScriptModule = Module->ScriptModule;
				if (ScriptModule == nullptr)
					continue;
				if (Module->bLoadedPrecompiledCode)
					continue;

				AngelscriptEngineCompilationEvents_Private::BroadcastModuleEvent(
					EAngelscriptCompilationEventType::CompileModuleLayout,
					TEXT("Compile.ModuleLayout"),
					CompilationContext.GetRunId(),
					CompileType,
					CompilationContext.GetCachePolicy(),
					Module,
					!Module->bCompileError);
			}
		}

		// It should be visible which modules are compiling during a hotreload
		if (CompileType != ECompileType::Initial && bIsInitialCompileFinished)
		{
			for (auto Module : CompiledModules)
			{
				asCModule* ScriptModule = Module->ScriptModule;
				if (ScriptModule == nullptr)
					continue;
				if (ScriptModule->ReloadState == asCModule::EReloadState::RecompiledWithStructuralChanges)
				{
					UE_LOG(Angelscript, Log, TEXT("Compiling (structural): %s"), *Module->ModuleName);
				}
				else
				{
					UE_LOG(Angelscript, Log, TEXT("Compiling (code only): %s"), *Module->ModuleName);
				}
			}
		}

		{
			FAngelscriptScopeTimer StageTimer(TEXT("script compilation stage3"));

			for (auto Module : CompiledModules)
			{
				// Update progress indicator
				if (--ProgressUpdateCounter <= 0 && ProgressUpdatesDone < 10)
				{
					SlowTask.EnterProgressFrame(0.025f);
					ProgressUpdateCounter = FMath::Max(CompiledModules.Num() / 10, 10);
					ProgressUpdatesDone += 1;
				}

				const bool bJitAvailable = ScriptEngine != nullptr && ScriptEngine->GetJITCompiler() != nullptr;

				// Perform stage 3 of compilation
				CompileModule_Code_Stage3(CompileType, Module);

				AngelscriptEngineCompilationEvents_Private::BroadcastModuleEvent(
					EAngelscriptCompilationEventType::CompileModuleCompileCode,
					TEXT("Compile.ModuleCompileCode"),
					CompilationContext.GetRunId(),
					CompileType,
					CompilationContext.GetCachePolicy(),
					Module,
					!Module->bCompileError,
					bJitAvailable,
					bJitAvailable && !Module->bCompileError && !Module->bLoadedPrecompiledCode);

				// Cancel out on a compile error if we're
				// doing a hot-reload compile.
				if (Module->bCompileError)
					bHadCompileErrors = true;
			}
		}

		// If we added any precompiled modules, finalize them now
		if (!CompileOptions.IsForcedClean()
			&& PrecompiledData != nullptr
			&& bUseStaticJITCompatibilityData)
		{
			PrecompiledData->PrepareToFinalizePrecompiledModules();
		}

		{
			FAngelscriptScopeTimer StageTimer(TEXT("script compilation stage4"));

			// Validate all template instances we've created
			asCBuilder builder(ScriptEngine, nullptr);
			builder.Reset();
			builder.EvaluateTemplateInstances(false);
			ScriptEngine->deferValidationOfTemplateTypes = false;

			if (builder.numErrors > 0)
				bHadCompileErrors = true;

			if (!bHadCompileErrors)
			{
				for (auto Module : CompiledModules)
				{
					// Update progress indicator
					if (--ProgressUpdateCounter <= 0 && ProgressUpdatesDone < 10)
					{
						SlowTask.EnterProgressFrame(0.025f);
						ProgressUpdateCounter = FMath::Max(CompiledModules.Num() / 10, 10);
						ProgressUpdatesDone += 1;
					}

					// Perform stage 4 of compilation
					CompileModule_Globals_Stage4(CompileType, Module);

					AngelscriptEngineCompilationEvents_Private::BroadcastModuleEvent(
						EAngelscriptCompilationEventType::CompileModuleGlobals,
						TEXT("Compile.ModuleGlobals"),
						CompilationContext.GetRunId(),
						CompileType,
						CompilationContext.GetCachePolicy(),
						Module,
						!Module->bCompileError);

					// Cancel out on a compile error if we're
					// doing a hot-reload compile.
					if (Module->bCompileError)
						bHadCompileErrors = true;
				}
			}
		}
	}

	ScriptEngine->BuildCompleted();
	bIgnoreCompileErrorDiagnostics = false;

	// Check if any function imports would error out later
	if (!ShouldUseAutomaticImportMethod())
	{
		if (!CheckFunctionImportsForNewModules(CompiledModules))
		{
			bHadCompileErrors = true;
		}
	}

	// Decide whether to swap in the new modules or not
	bool bShouldSwapInModules = true;
	bool bFullReloadRequired = false;

	// In script reloads, don't swap in anything unless
	// *everything* compiled without errors, so we don't
	// end up in inconsistent state.
	if (bHadCompileErrors)
	{
		UE_LOG(Angelscript, Error, TEXT("Hot reload failed due to script compile errors. Keeping all old script code."));
		bShouldSwapInModules = false;
	}

#if WITH_EDITOR
	// If any script modules specified usage restrictions we should check those now
	CheckUsageRestrictions(CompiledModules);
#endif

	TArray<TSharedRef<FAngelscriptModuleDesc>> DiscardedModules;

	if (bShouldSwapInModules)
	{
		FAngelscriptClassGenerator ClassGenerator;

		// Run the delegate that the game might hook to provide more errors/warnings
		AngelscriptEngineCompilationEvents_Private::BroadcastCompileEvent(
			EAngelscriptCompilationEventType::CompileClassGenerationHandoff,
			TEXT("Compile.ClassGenerationHandoff"),
			CompilationContext.GetRunId(),
			CompileType,
			CompilationContext.GetCachePolicy(),
			CompiledModules);
		GetPreGenerateClasses().Broadcast(CompiledModules);

		for (auto Module : CompiledModules)
		{
			if (Module->ScriptModule != nullptr)
			{
				// Generate classes for the module based on the preprocessed data and the compiled angelscript data
				ClassGenerator.AddModule(Module);
			}
		}

		// Update progress indicator
		SlowTask.EnterProgressFrame(0.5f, FText::FromString(TEXT("Class Generator Setup")));

		// Perform the actual reload
		auto ReloadReq = ClassGenerator.Setup();

		bool bVerifiedProperties = true;
		if (GIsEditor || bScriptDevelopmentMode)
		{
			// Verify Unreal properties
			bVerifiedProperties = VerifyPropertySpecifiers(CompiledModules);
		}

		if (!bVerifiedProperties)
		{
			bShouldSwapInModules = false;
			bHadCompileErrors = true;
		}
		else
		{
			// Emit diagnostics before we go into potentially very slow unreal reload
			EmitDiagnostics();

			// Update progress indicator
			SlowTask.EnterProgressFrame(1.5f, FText::FromString(TEXT("Class Generation")));

			switch (ReloadReq)
			{
				case FAngelscriptClassGenerator::EReloadRequirement::SoftReload:
					SwapInModules(CompiledModules, DiscardedModules);
					ClassGenerator.PerformSoftReload();
					break;
				case FAngelscriptClassGenerator::EReloadRequirement::FullReloadSuggested:
					if (CompileType == ECompileType::SoftReloadOnly)
					{
						if (CompileOptions.bRejectStructuralChanges)
						{
							const FString Msg =
								TEXT("Packaged Runtime reload rejected a structural UPROPERTY/UFUNCTION change. ")
								TEXT("Keeping the last good AngelScript module active until restart.");
							UE_LOG(Angelscript, Warning, TEXT("%s"), *Msg);
							for (const TSharedRef<FAngelscriptModuleDesc>& Module :
								CompiledModules)
							{
								if (ClassGenerator.WantsFullReload(Module))
								{
									TArray<int32> Lines;
									ClassGenerator.GetFullReloadLines(Module, Lines);
									for (const int32 ReloadLine : Lines)
									{
										ScriptCompileError(
											Module, ReloadLine, Msg, false);
									}
								}
							}
							bShouldSwapInModules = false;
							bFullReloadRequired = true;
							break;
						}
#if WITH_EDITOR
						FString Msg =
							TEXT("Performing a Soft Reload during PIE. New UPROPERTY()s and UFUNCTION()s won't show up")
								TEXT(" until full reload. A Full Reload will be queued for after PIE ends.");
						UE_LOG(Angelscript, Warning, TEXT("%s"), *Msg);

						for (auto Module : CompiledModules)
						{
							if (ClassGenerator.WantsFullReload(Module))
							{
								TArray<int32> Lines;
								ClassGenerator.GetFullReloadLines(Module, Lines);
								for (int32 ReloadLine : Lines)
									ScriptCompileError(Module, ReloadLine, Msg, false);
							}
						}
#endif
						bWasFullyHandled = false;
						SwapInModules(CompiledModules, DiscardedModules);
						ClassGenerator.PerformSoftReload();
					}
					else
					{
						SwapInModules(CompiledModules, DiscardedModules);
						ClassGenerator.PerformFullReload();
					}
					break;
				case FAngelscriptClassGenerator::EReloadRequirement::FullReloadRequired:
					if (CompileType == ECompileType::SoftReloadOnly)
					{
						FString Msg =
							TEXT("Full Reload is required due to UPROPERTY() or UFUNCTION() changes, but cannot")
								TEXT(" perform a full reload right now. Keeping old angelscript code active.");
						UE_LOG(Angelscript, Error, TEXT("%s"), *Msg);

						for (auto Module : CompiledModules)
						{
							if (ClassGenerator.NeedsFullReload(Module))
							{
								TArray<int32> Lines;
								ClassGenerator.GetFullReloadLines(Module, Lines);
								for (int32 ReloadLine : Lines)
									ScriptCompileError(Module, ReloadLine, Msg, true);
							}
						}
						bShouldSwapInModules = false;
						bFullReloadRequired = true;
					}
					else
					{
						SwapInModules(CompiledModules, DiscardedModules);
						ClassGenerator.PerformFullReload();
					}
					break;
				case FAngelscriptClassGenerator::EReloadRequirement::Error:
					UE_LOG(Angelscript, Error,
						TEXT("An error was encountered during angelscript hot reload. Keeping old angelscript code "
							 "active."));
					bShouldSwapInModules = false;
					bHadCompileErrors = true;
					break;
			}
		}
	}

	TArray<FAngelscriptFunctionArtifactIdentity>
		ValidatedCompileFunctionIdentities;

	// Freeze only after the final ClassGenerator/reinstancing decision, while
	// every compiled candidate still owns its complete VM state. This is a
	// fail-soft observer: no capture failure changes the established compile,
	// activation or last-good behavior.
	if (CacheCaptureContext != nullptr
		&& CacheMutationGuard.IsEntered()
		&& !bHadCompileErrors)
	{
		const bool bPendingColdStart =
			CompileType == ECompileType::SoftReloadOnly
			&& (!bWasFullyHandled || bFullReloadRequired);
		const bool bCanFreezeCurrent = bShouldSwapInModules;
		const bool bCanFreezePending = bPendingColdStart
			&& (bShouldSwapInModules || bFullReloadRequired);
		if (bCanFreezeCurrent || bCanFreezePending)
		{
			TMap<FString, TSharedRef<FAngelscriptModuleDesc>> CandidateByName;
			for (const TSharedRef<FAngelscriptModuleDesc>& Active :
				GetActiveModules())
			{
				CandidateByName.Add(Active->ModuleName, Active);
			}
			if (!bShouldSwapInModules)
			{
				for (const TSharedRef<FAngelscriptModuleDesc>& Candidate :
					CompiledModules)
				{
					CandidateByName.Add(Candidate->ModuleName, Candidate);
				}
			}

			TArray<TSharedRef<FAngelscriptModuleDesc>> CaptureModules;
			CandidateByName.GenerateValueArray(CaptureModules);
			CaptureModules.Sort([](
				const TSharedRef<FAngelscriptModuleDesc>& Left,
				const TSharedRef<FAngelscriptModuleDesc>& Right)
			{
				return FAngelscriptArtifactCanonicalWriter::
					CompareCanonicalUtf8Strings(
						Left->ModuleName, Right->ModuleName) < 0;
			});

			FAngelscriptCacheSuccessfulPublicationInput PublicationInput;
			PublicationInput.Kind = CompileType == ECompileType::Initial
				? EAngelscriptCacheSuccessfulCompileKind::Initial
				: CompileType == ECompileType::FullReload
					? EAngelscriptCacheSuccessfulCompileKind::FullReload
					: EAngelscriptCacheSuccessfulCompileKind::SoftReload;
			PublicationInput.Disposition = bPendingColdStart
				? EAngelscriptCachePublicationDisposition::PendingColdStart
				: EAngelscriptCachePublicationDisposition::Current;
			PublicationInput.Compatibility = CacheCaptureContext->
				Environment.CaptureOptions.Compatibility;
			PublicationInput.Context = CacheCaptureContext->
				Environment.CaptureOptions.Context;
			PublicationInput.Profile = CacheCaptureContext->
				Environment.CaptureOptions.Profile;

			int32 SkippedCaptureCount = 0;
			uint32 GraphCarriedDependencyFunctionCount = 0;
			for (const TSharedRef<FAngelscriptModuleDesc>& Candidate :
				CaptureModules)
			{
				FAngelscriptCacheCleanModuleArtifacts Artifacts;
				const FAngelscriptCacheCleanCaptureResult CaptureResult =
					CaptureAngelscriptCleanCompiledModule(
					Candidate,
					CacheCaptureContext->Environment.CaptureOptions,
					CacheCaptureContext->AuthoritativeSourceIndex,
					CacheReuseContext,
					Artifacts);
				if (!CaptureResult.IsSuccess())
				{
					++SkippedCaptureCount;
					FAngelscriptCacheDecisionEvent Decision;
					Decision.Stage = CompileType == ECompileType::Initial
						? EAngelscriptCacheDecisionStage::SuccessfulPublication
						: EAngelscriptCacheDecisionStage::RuntimeReload;
					Decision.Outcome = CaptureResult.Error
						== EAngelscriptCacheCleanCaptureError::NotCacheable
						? EAngelscriptCacheDecisionOutcome::NotCacheable
						: EAngelscriptCacheDecisionOutcome::Rejected;
					Decision.ReasonDomain =
						EAngelscriptCacheDecisionReasonDomain::CleanCapture;
					Decision.ReasonCode =
						static_cast<uint32>(CaptureResult.Error);
					Decision.Profile = CacheCaptureContext->
						Environment.CaptureOptions.Profile;
					Decision.SourceSnapshot = CacheCaptureContext->
						AuthoritativeSourceIndex.SourceSnapshot;
					Decision.PrimaryCount = CaptureResult.ValidatedGraphRecordCount;
					Decision.Detail = CaptureResult.Detail;
					const TOptional<FAngelscriptStableModuleKey> ModuleKey =
						AngelscriptEngineCacheCapture_Private::FindModuleKey(
							*Candidate,
							CacheCaptureContext->AuthoritativeSourceIndex);
					if (ModuleKey.IsSet())
					{
						Decision.ModuleKeys.Add(ModuleKey.GetValue());
					}
					CacheService->RecordDecisionEvent(MoveTemp(Decision));
					const FString ModuleKeyText = ModuleKey.IsSet()
						? ModuleKey->Hash.ToHexString() : TEXT("unknown");
					if (CaptureResult.Error
						== EAngelscriptCacheCleanCaptureError::NotCacheable)
					{
						UE_LOG(Angelscript, Verbose,
							TEXT("[CacheV2] Compile capture skipped module: Module=%s ModuleKey=%s Error=%u GraphRecords=%u CapturedSoFar=%d SkippedSoFar=%d Detail=%s"),
							*Candidate->ModuleName, *ModuleKeyText,
							static_cast<uint32>(CaptureResult.Error),
							CaptureResult.ValidatedGraphRecordCount,
							PublicationInput.Modules.Num(),
							SkippedCaptureCount,
							*CaptureResult.Detail);
					}
					else
					{
						UE_LOG(Angelscript, Warning,
							TEXT("[CacheV2] Compile capture rejected module: Module=%s ModuleKey=%s Error=%u GraphRecords=%u CapturedSoFar=%d SkippedSoFar=%d Detail=%s"),
							*Candidate->ModuleName, *ModuleKeyText,
							static_cast<uint32>(CaptureResult.Error),
							CaptureResult.ValidatedGraphRecordCount,
							PublicationInput.Modules.Num(),
							SkippedCaptureCount,
							*CaptureResult.Detail);
					}
					continue;
				}
				GraphCarriedDependencyFunctionCount +=
					CaptureResult.GraphCarriedDependencyFunctionCount;
				PublicationInput.Modules.Add(MoveTemp(Artifacts));
			}

			UE_LOG(Angelscript, Display,
				TEXT("[CacheV2] Compile capture batch: Candidates=%d Captured=%d Skipped=%d GraphCarriedDependencyFunctions=%u"),
				CaptureModules.Num(), PublicationInput.Modules.Num(),
				SkippedCaptureCount,
				GraphCarriedDependencyFunctionCount);
			if (!PublicationInput.Modules.IsEmpty())
			{
				for (const FAngelscriptCacheCleanModuleArtifacts& Module
					: PublicationInput.Modules)
				{
					ValidatedCompileFunctionIdentities.Append(
						Module.ValidatedFunctionArtifactIdentities);
				}
				const FAngelscriptCacheFreezePublicationResult Freeze =
					CacheService->FreezeSuccessfulCompileArtifacts(
						CacheMutationGuard.GetToken(),
						MoveTemp(PublicationInput));
				if (Freeze.IsSuccess())
				{
					UE_LOG(Angelscript, Display,
						TEXT("[CacheV2] Published compile transaction: Tx=%llu Kind=%u Disposition=%u Modules=%d SourceSnapshot=%s"),
						Freeze.Publication->TransactionOrdinal,
						static_cast<uint32>(Freeze.Publication->Kind),
						static_cast<uint32>(Freeze.Publication->Disposition),
						Freeze.Publication->Modules.Num(),
						*Freeze.Publication->SourceSnapshot.ToHexString());
				}
				else
				{
					UE_LOG(Angelscript, Warning,
						TEXT("[CacheV2] Compile capture freeze rejected: Error=%u"),
						static_cast<uint32>(Freeze.Error));
				}
			}
		}
	}

	if (bShouldSwapInModules)
	{
		// Actually delete old modules
		{
			FAngelscriptScopeTimer PostTimer(TEXT("old module cleanup"));
			for (auto OldModule : DiscardedModules)
			{
				if (OldModule->ScriptModule != nullptr)
				{
					// Discard it from the engine as well
					Engine->DiscardModule(OldModule->ScriptModule->GetName());
					OldModule->ScriptModule = nullptr;
				}
			}

			Engine->DeleteDiscardedModules();
		}
		
		// Remove all old template instances we are no longer using
		for (auto TemplateElem : ScriptUpdateMap.TemplateInstances)
		{
			// Key contains the old instance, which we discard
			Engine->DiscardTemplateInstance(TemplateElem.Key);
			// We've held a reference to the old instance just in case, which we drop now
			TemplateElem.Key->ReleaseInternal();
		}

		{
			FAngelscriptScopeTimer PostTimer(TEXT("update module cache"));
			for (auto Module : CompiledModules)
			{
				// Update the cache of script module to module desc
				if (Module->ScriptModule != nullptr)
				{
					ModulesByScriptModule.Add(Module->ScriptModule, Module);
				}

				// If the module has received any synthetic errors from the class generator,
				// make sure it gets recompiled until those go away
				if (Module->bModuleSwapInError)
				{
					for (auto& Section : Module->Code)
						PreviouslyFailedReloadFiles.Add(FFilenamePair{ Section.AbsoluteFilename, Section.RelativeFilename, Section.VirtualPath });
				}
			}
		}

		// Publish one immutable current-Engine StableFunctionKey route snapshot
		// only after the accepted modules and reverse module index agree.
		TArray<asIScriptModule*> RebuiltRouteModules;
		RebuiltRouteModules.Reserve(CompiledModules.Num());
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : CompiledModules)
		{
			if (Module->ScriptModule != nullptr)
			{
				RebuiltRouteModules.AddUnique(Module->ScriptModule);
			}
		}
		TArray<asIScriptModule*> ArtifactInvalidatedRouteModules;
		ArtifactInvalidatedRouteModules.Reserve(
			ModulesToUpdateReferences.Num());
		for (const TSharedRef<FAngelscriptModuleDesc>& Module
			: ModulesToUpdateReferences)
		{
			if (Module->ScriptModule != nullptr)
			{
				ArtifactInvalidatedRouteModules.AddUnique(
					Module->ScriptModule);
			}
		}
		RebuildFunctionRouteSnapshot(
			TConstArrayView<FAngelscriptCacheLiveFunctionRoute>(),
			ValidatedCompileFunctionIdentities,
			RebuiltRouteModules,
			ArtifactInvalidatedRouteModules);

		// We changed some modules, so we should re-resolve all declared imports in all modules
		//  Technically we could store dependencies for these as well and only re-resolve as needed,
		//  but it's not expensive at all so not worth doing.
		if (!ShouldUseAutomaticImportMethod())
		{
			FAngelscriptScopeTimer PostTimer(TEXT("resolve declared imports"));
			ResolveAllDeclaredImports();
		}
	}
	else
	{
		// Any existing modules that we replaced references in,
		// well, we need to replace the references back again to the old ones, yay!
		if (ModulesToUpdateReferences.Num() != 0)
		{
			FAngelscriptScopeTimer PostTimer(TEXT("undo script reference update"));

			asModuleReferenceUpdateMap ReverseUpdateMap;
			ScriptUpdateMap.BuildReverseMap(OUT ReverseUpdateMap);

			for (auto Module : ModulesToUpdateReferences)
			{
				asCModule* ScriptModule = Module->ScriptModule;
				if (ScriptModule == nullptr)
					continue;

				ScriptModule->UpdateReferencesInReflectionDataOnly(ReverseUpdateMap);
				UpdateScriptReferencesInUnrealData(ReverseUpdateMap, Module);
				ScriptModule->UpdateReferencesInScriptBytecode(ReverseUpdateMap);
			}
		}

		// Remove all new template instances we won't actually be using
		for (auto TemplateElem : ScriptUpdateMap.TemplateInstances)
		{
			// Value contains the new instance, which we discard
			Engine->DiscardTemplateInstance(TemplateElem.Value);
			// We previously held a reference to the old instance (in Key), to make sure
			// it didn't get deleted. We drop that reference now, it's been replaced back into the old modules.
			TemplateElem.Key->ReleaseInternal();
		}

		// Discard script modules for stuff we haven't decided to swap in
		for (auto Module : CompiledModules)
		{
			if (Module->ScriptModule != nullptr)
			{
				auto* OldScriptModule = (asCModule*)Module->ScriptModule;
				OldScriptModule->RemoveTypesAndGlobalsFromEngineAvailability();
				OldScriptModule->InternalReset();
				Engine->DiscardModule(OldScriptModule->GetName());

				Module->ScriptModule = nullptr;
			}
		}
	}

	// If we have any new diagnostics, emit them again
	if (bDiagnosticsDirty)
		EmitDiagnostics();

	// Reset reload state on all modules
	for (int i = 0, Count = Engine->scriptModules.GetLength(); i < Count; ++i)
	{
		asCModule* ScriptModule = Engine->scriptModules[i];
		if (ScriptModule == nullptr)
			continue;

		ScriptModule->ReloadState = asCModule::EReloadState::None;
		ScriptModule->ReloadOldModule = nullptr;
		ScriptModule->ReloadNewModule = nullptr;
	}

	ECompileResult Result = ECompileResult::FullyHandled;
	if (!bShouldSwapInModules || bHadCompileErrors)
		Result = bFullReloadRequired ? ECompileResult::ErrorNeedFullReload : ECompileResult::Error;
	else if (!bWasFullyHandled)
		Result = ECompileResult::PartiallyHandled;

	if (bShouldSwapInModules && !bHadCompileErrors)
	{
		FAngelscriptCompilationDelegate& PostCompileDelegate = GetPostCompile();
		if (PostCompileDelegate.IsBound())
			PostCompileDelegate.Broadcast();

	}

	if (CompileType != ECompileType::Initial
		&& Result != ECompileResult::FullyHandled)
	{
		TArray<FFilenamePair> AllCompiledFiles;
		for (auto Module : CompiledModules)
		{
			for (auto& Section : Module->Code)
				AllCompiledFiles.Add(FFilenamePair{ Section.AbsoluteFilename, Section.RelativeFilename, Section.VirtualPath });
		}

		if (Result == ECompileResult::ErrorNeedFullReload)
		{
			// An error was caused because we need a full reload, so queue that up
			for (const auto& RepeatFile : AllCompiledFiles)
				QueuedFullReloadFiles.Add(RepeatFile);

			PreviouslyFailedReloadFiles.Append(AllCompiledFiles);
		}
		else if (Result == ECompileResult::Error)
		{
			// Store failed files so we retry them next reload automatically
			PreviouslyFailedReloadFiles.Append(AllCompiledFiles);
		}
		else if (Result == ECompileResult::PartiallyHandled)
		{
			// If the compilation wasn't fully handled, queue up the files we soft reloaded
			// for a full reload later when it is possible.
			for (const auto& RepeatFile : AllCompiledFiles)
				QueuedFullReloadFiles.Add(RepeatFile);
		}
	}

	CompilationContext.CaptureCompiledModules(CompiledModules);
	CompilationContext.SetResult(Result);
	OutCompiledModules = MoveTemp(CompiledModules);

	if (FAngelscriptCompilationEvents::HasListeners())
	{
		FAngelscriptCompilationEvent EndEvent;
		EndEvent.Type = EAngelscriptCompilationEventType::CompileEnd;
		EndEvent.Phase = TEXT("Compile.End");
		CompilationContext.PopulateResult(EndEvent);
		CompilationContext.PopulateInputSummary(EndEvent);
		AngelscriptEngineCompilationEvents_Private::AddDiagnosticSummary(EndEvent, Diagnostics);

		FAngelscriptCompilationEvents::Broadcast(EndEvent);
	}

	return Result;
}

void FAngelscriptEngine::UpdateScriptReferencesInUnrealData(struct asModuleReferenceUpdateMap& UpdateMap, TSharedRef<FAngelscriptModuleDesc> Module)
{
	auto UpdateTypeUsage = [&](FAngelscriptTypeUsage& Type)
	{
		if (Type.ScriptClass != nullptr)
		{
			auto* NewTypeInfo = UpdateMap.Types.FindRef((asCTypeInfo*)Type.ScriptClass);
			if (NewTypeInfo != nullptr)
				Type.ScriptClass = NewTypeInfo;
		}
	};

	auto UpdateFunctionDesc = [&](FAngelscriptFunctionDesc& Function)
	{
		UpdateTypeUsage(Function.ReturnType);

		for (auto& Argument : Function.Arguments)
		{
			UpdateTypeUsage(Argument.Type);
		}
	};

	auto UpdateUnrealFunction = [&](UASFunction* Function)
	{
		UpdateTypeUsage(Function->ReturnArgument.Type);
		for (auto& Argument : Function->Arguments)
			UpdateTypeUsage(Argument.Type);
		for (auto& Argument : Function->DestroyArguments)
			UpdateTypeUsage(Argument.Type);
	};

	for (auto Class : Module->Classes)
	{
		for (auto Property : Class->Properties)
		{
			UpdateTypeUsage(Property->PropertyType);
		}

		for (auto Method : Class->Methods)
		{
			UpdateFunctionDesc(*Method);
			if (Method->Function != nullptr)
				UpdateUnrealFunction((UASFunction*)Method->Function);
		}
	}

	for (auto Delegate : Module->Delegates)
	{
		UpdateFunctionDesc(*Delegate->Signature);
	}
}

void FAngelscriptEngine::CompileModule_Types_Stage1(
	ECompileType CompileType,
	TSharedRef<struct FAngelscriptModuleDesc> Module,
	const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& ImportedModules,
	const FAngelscriptCompileOptions& CompileOptions)
{
	// Modules always compile with a temporary name, the code
	// then later decides whether to use them and rename them.
	FString TempName = MakeModuleName(Module->ModuleName);
	if (CompileType != ECompileType::Initial)
	{
		TempName = FString::Printf(TEXT("%s_NEW_%d"), *TempName, TempNameIndex);
		TempNameIndex += 1;
	}

	// Generate the angelscript module
	auto* ScriptModule = (asCModule*)Engine->GetModule(TCHAR_TO_ANSI(*TempName), asGM_ALWAYS_CREATE);
	ScriptModule->baseModuleName = TCHAR_TO_ANSI(*Module->ModuleName);

	Module->CombinedDependencyHash = Module->CodeHash;

	// Add stuff from all imported modules into the newly generated module
	bool bAllImportsPreCompiled = true;
	for (auto ImportModule : ImportedModules)
	{
		if (ensure(ImportModule->ScriptModule != nullptr))
		{
			ImportIntoModule(ScriptModule, ImportModule->ScriptModule);
		}

		// If any of our imports are not precompiled, we should not use precompiled code either
		if (!ImportModule->bLoadedPrecompiledCode)
		{
			bAllImportsPreCompiled = false;
		}

		// Combine the hash of the imported module into our own dependency hash
		Module->CombinedDependencyHash ^= ImportModule->CombinedDependencyHash;
	}

	// Check if we have precompiled data for this module and use it if we can
	if (!CompileOptions.IsForcedClean()
		&& PrecompiledData != nullptr
		&& bAllImportsPreCompiled
		&& bUseStaticJITCompatibilityData)
	{
		const FAngelscriptPrecompiledModule* CompiledModule = PrecompiledData->Modules.Find(Module->ModuleName);
		if (CompiledModule != nullptr)
		{
			// Check if file content hashes are the same or not
			if (CompiledModule->CodeHash == Module->CodeHash)
			{
				CompiledModule->ApplyToModule_Stage1(*PrecompiledData, ScriptModule);

				Module->PrecompiledData = CompiledModule;
				Module->bCompileError = false;
				Module->ScriptModule = ScriptModule;
				Module->bLoadedPrecompiledCode = true;

				return;
			}
			else
			{
				UE_LOG(Angelscript, Warning, TEXT("Angelscript precompiled data for module '%s' did not match script as loaded from file. Discarding precompiled data."), *Module->ModuleName);
			}
		}
		else
		{
			UE_LOG(Angelscript, Warning, TEXT("Angelscript precompiled data did not include any code for module '%s'."), *Module->ModuleName);
		}
	}

	// Set up proper pre-class data, this tells angelscript how
	// to treat compilation for classes derived from code classes.
	for (auto ClassDesc : Module->Classes)
	{
		asPreClassData Data;
		bool bHasPreClassData = false;

		if (ClassDesc->bIsStruct)
		{
			Data.PropertyOffset = UASStruct::ScriptValueOffset;
			bHasPreClassData = true;
		}

		if (ClassDesc->CodeSuperClass != nullptr)
		{
			Data.PropertyOffset = ClassDesc->CodeSuperClass->GetPropertiesSize();

			FString SuperClassName = FAngelscriptType::GetBoundClassName(ClassDesc->CodeSuperClass);
			Data.ShadowType = Engine->allRegisteredTypesByName.FindFirst_CaseInsensitive(TCHAR_TO_ANSI(*SuperClassName));

			checkf(Data.ShadowType != nullptr, TEXT("Unable to find C++ class %s to inherit from"), *SuperClassName);
			bHasPreClassData = true;
		}

		if (bHasPreClassData)
			ScriptModule->AddPreClassData(TCHAR_TO_ANSI(*ClassDesc->ClassName), Data);
	}

	// Delegates need to be tagged with a userdata tag so they can be detected correctly during compilation
	for (auto DelegateDesc : Module->Delegates)
	{
		asPreClassData Data;
		if (DelegateDesc->bIsMulticast)
			Data.InitialUserData = FAngelscriptType::TAG_UserData_Multicast_Delegate;
		else
			Data.InitialUserData = FAngelscriptType::TAG_UserData_Delegate;

		ScriptModule->AddPreClassData(TCHAR_TO_ANSI(*DelegateDesc->DelegateName), Data);
	}

	// Add all code we need
	for (auto& Section : Module->Code)
	{
		const FString SectionName = Section.AbsoluteFilename.IsEmpty() ? Section.VirtualPath : Section.AbsoluteFilename;
		ScriptModule->AddScriptSection(TCHAR_TO_ANSI(*SectionName), TCHAR_TO_UTF8(*Section.Code), 0, 0);
	}
	
	// Set the code hash as userdata so we can find it later
#if AS_CAN_GENERATE_JIT
	ScriptModule->SetUserData((void*)(size_t)Module->CombinedDependencyHash, 0);
#endif

#if WITH_EDITOR
	// Allow the script compiler to see which lines are editor-only so it can emit warnings
	ScriptModule->builder->SetEditorOnlyBlockLinePositions(Module->EditorOnlyBlockLines);
	ScriptModule->builder->isEditorOnlyModule = Module->ModuleName.StartsWith(TEXT("Editor.")) || Module->ModuleName.Contains(TEXT(".Editor."));
#endif

	Module->ScriptModule = ScriptModule;
}

void FAngelscriptEngine::CompileModule_Functions_Stage2(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module)
{
	auto* ScriptModule = (asCModule*)Module->ScriptModule;
	if (Module->bCompileError)
		return;
	if (ScriptModule == nullptr)
		return;

	if (Module->bLoadedPrecompiledCode)
	{
		Module->PrecompiledData->ApplyToModule_Stage2(*PrecompiledData, ScriptModule);
		return;
	}

	auto Result = ScriptModule->builder->BuildGenerateFunctions();
	if (Result != asSUCCESS)
		Module->bCompileError = true;
}

void FAngelscriptEngine::CompileModule_Code_Stage3(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module)
{
	auto* ScriptModule = (asCModule*)Module->ScriptModule;
	if (ScriptModule == nullptr)
		return;

	if (Module->bLoadedPrecompiledCode)
	{
		Module->PrecompiledData->ApplyToModule_Stage3(*PrecompiledData, ScriptModule);
		return;
	}

	auto Result = ScriptModule->builder->BuildCompileCode();
	if (Result != asSUCCESS)
		Module->bCompileError = true;

	asDELETE(ScriptModule->builder, asCBuilder);
	ScriptModule->builder = nullptr;

	ScriptModule->JITCompile();
}

void FAngelscriptEngine::CompileModule_Globals_Stage4(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module)
{
	auto* ScriptModule = (asCModule*)Module->ScriptModule;
	if (ScriptModule == nullptr)
		return;

	check(!Module->bCompileError);
	ScriptModule->ResetGlobalVars(0);

#if WITH_AS_COVERAGE
	if (FAngelscriptCodeCoverage* CodeCoverage = FAngelscriptCodeCoverageExtension::GetForEngine(*this))
	{
		CodeCoverage->MapExecutableLines(*Module);
	}
#endif
}

void FAngelscriptEngine::ImportIntoModule(class asIScriptModule* IntoModule, class asIScriptModule* FromModuleIntf)
{
	asCModule* FromModule = (asCModule*)FromModuleIntf;
	IntoModule->ImportModule(FromModule);
}

void FAngelscriptEngine::ResolveAllDeclaredImports()
{
	for (auto& Elem : ActiveModules)
		ResolveDeclaredImports(Elem.Value->ScriptModule);
}

#if WITH_AS_DEBUGSERVER
bool FAngelscriptEngine::IsEvaluatingDebuggerWatch()
{
	if (DebugServer == nullptr)
		return false;
	if (DebugServer->bIsEvaluatingDebuggerWatch)
		return true;
	return false;
}
#endif

FString FAngelscriptEngine::FormatDiagnostics()
{
	FString Str;
	for (auto& FileDiagElem : Diagnostics)
	{
		if (FileDiagElem.Value.Diagnostics.Num() == 0)
			continue;
		Str += TEXT("\n");
		Str += FileDiagElem.Value.Filename;
		Str += TEXT(":\n");
		for (auto& Diag : FileDiagElem.Value.Diagnostics)
		{
			if (Diag.Row || Diag.Column)
				Str += FString::Printf(TEXT("(%d:%d) "), Diag.Row, Diag.Column);
			Str += Diag.Message;
			Str += TEXT("\n");
		}
	}
	return Str;
}

void FAngelscriptEngine::ResetDiagnostics()
{
	Diagnostics.Empty();
}

void FAngelscriptEngine::EmitDiagnostics(class FSocket* Client)
{
	// Output captured diagnostic messages to debugger
	for (auto Iterator = Diagnostics.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator->Value.Diagnostics.Num() == 0)
		{
			if (Iterator->Value.bHasEmittedAny || Iterator->Value.bIsCompiling)
				EmitDiagnostics(Iterator->Value, Client);

#if WITH_AS_DEBUGSERVER
			if (Client == nullptr && (DebugServer == nullptr || DebugServer->HasAnyClients()))
				Iterator.RemoveCurrent();
#else
			Iterator.RemoveCurrent();
#endif
		}
		else
		{
			EmitDiagnostics(Iterator->Value, Client);
			Iterator->Value.bHasEmittedAny = true;
		}
	}

	bDiagnosticsDirty = false;
}

void FAngelscriptEngine::EmitDiagnostics(FDiagnostics& Diag, class FSocket* Client)
{
#if WITH_AS_DEBUGSERVER
	if (DebugServer == nullptr)
		return;

	FAngelscriptDiagnostics Message;
	Message.Filename = Diag.Filename;
	for (auto& Ms : Diag.Diagnostics)
	{
		FAngelscriptDiagnostic New;
		New.Message = Ms.Message;
		New.Line = Ms.Row;
		New.Character = Ms.Column;
		New.bIsError = Ms.bIsError;
		New.bIsInfo = Ms.bIsInfo;
		Message.Diagnostics.Add(New);
	}

	if (Client == nullptr)
		DebugServer->SendMessageToAll(EDebugMessageType::Diagnostics, Message);
	else
		DebugServer->SendMessageToClient(Client, EDebugMessageType::Diagnostics, Message);
#endif
}

#if WITH_EDITOR
void FAngelscriptEngine::CheckUsageRestrictions(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules)
{
	// Figure out which modules have restrictions
	// We do this for both modules that are compiling right now, and modules that were previously compiled
	TMap<asCModule*, TSharedRef<FAngelscriptModuleDesc>> ModulesWithRestrictions;
	for (auto Module : Modules)
	{
		auto* ScriptModule = (asCModule*)Module->ScriptModule;
		if (ScriptModule == nullptr)
			return;
		if (Module->Code.Num() == 0)
			return;

		if (Module->UsageRestrictions.Num() != 0)
			ModulesWithRestrictions.Add(ScriptModule, Module);
	}

	for (auto ModuleElem : ActiveModules)
	{
		auto Module = ModuleElem.Value;
		auto* ScriptModule = (asCModule*)Module->ScriptModule;
		if (ScriptModule == nullptr)
			return;
		if (Module->Code.Num() == 0)
			return;

		if (Module->UsageRestrictions.Num() != 0)
			ModulesWithRestrictions.Add(ScriptModule, Module);
	}

	// Early out if we don't have any restrictions at all
	if (ModulesWithRestrictions.Num() == 0)
		return;

	// Check each module we're compiling if it violates any restrictions
	for (auto Module : Modules)
	{
		auto* ScriptModule = (asCModule*)Module->ScriptModule;
		if (ScriptModule == nullptr)
			return;
		if (Module->Code.Num() == 0)
			return;

		for (const auto& DependencyElement : ScriptModule->moduleDependencies)
		{
			const auto& DependencyInfo = DependencyElement.Value;
			asCModule* Dependency = DependencyElement.Key;

			auto* DependencyModuleDescPtr = ModulesWithRestrictions.Find(Dependency);
			if (DependencyModuleDescPtr == nullptr)
				continue;

			auto DependencyModuleDesc = *DependencyModuleDescPtr;
			bool bMatchesAllow = false;
			bool bMatchesDisallow = false;
			bool bHasAnyDisallow = false;

			for (auto& Restriction : DependencyModuleDesc->UsageRestrictions)
			{
				if (Restriction.bIsAllow)
				{
					if (Module->ModuleName.MatchesWildcard(Restriction.Pattern))
						bMatchesAllow = true;
				}
				else
				{
					bHasAnyDisallow = true;
					if (Module->ModuleName.MatchesWildcard(Restriction.Pattern))
						bMatchesDisallow = true;
				}
			}

			if (!bMatchesAllow && (bMatchesDisallow || !bHasAnyDisallow))
			{
				ScriptCompileError(
					Module, DependencyInfo.FirstLineNumber,
					FString::Printf(
						TEXT("Restricted usage of module %s within module %s is disallowed."),
						*DependencyModuleDesc->ModuleName,
						*Module->ModuleName
					)
				);
			}
		}
	}
}
#endif

void FAngelscriptEngine::ResolveDeclaredImports(class asIScriptModule* Module)
{
	if (Module == nullptr)
		return;

	int32 ImportCount = Module->GetImportedFunctionCount();
	if (ImportCount == 0)
		return;

	auto ToModuleDesc = GetModule(ANSI_TO_TCHAR(Module->GetName()));
	for (int32 ImportIndex = 0; ImportIndex < ImportCount; ++ImportIndex)
	{
		const char* Decl = Module->GetImportedFunctionDeclaration(ImportIndex);

		FString FromModuleName = ANSI_TO_TCHAR(Module->GetImportedFunctionSourceModule(ImportIndex));
		auto FromModule = GetModule(FromModuleName);
		if (!FromModule.IsValid() || FromModule->ScriptModule == nullptr)
		{
			// Errors already presented by CheckFunctionImportsForNewModules
			Module->UnbindImportedFunction(ImportIndex);
			continue;
		}

		asIScriptFunction* Function = FromModule->ScriptModule->GetFunctionByDecl(Decl);
		if (Function == nullptr)
		{
			// Errors already presented by CheckFunctionImportsForNewModules
			Module->UnbindImportedFunction(ImportIndex);
			continue;
		}

		Module->BindImportedFunction(ImportIndex, Function);
	}
}

bool FAngelscriptEngine::CheckFunctionImportsForNewModules(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules)
{
	bool bValid = true;

	TMap<FString, TSharedRef<struct FAngelscriptModuleDesc>> SwappingModules;
	for (auto Module : Modules)
		SwappingModules.Add(Module->ModuleName, Module);

	auto FindModule = [&](const FString& Name) -> TSharedPtr<FAngelscriptModuleDesc>
	{
		auto* SwapModule = SwappingModules.Find(Name);
		if (SwapModule != nullptr)
			return *SwapModule;
		return GetModule(Name);
	};

	auto CheckModule = [&](TSharedRef<struct FAngelscriptModuleDesc> Module)
	{	
		auto* ScriptModule = Module->ScriptModule;
		bool bModuleValid = ScriptModule != nullptr;

		if (bModuleValid)
		{
			for (int32 ImportIndex = 0, ImportCount = ScriptModule->GetImportedFunctionCount(); ImportIndex < ImportCount; ++ImportIndex)
			{
				const char* Decl = ScriptModule->GetImportedFunctionDeclaration(ImportIndex);

				FString FromModuleName = ANSI_TO_TCHAR(ScriptModule->GetImportedFunctionSourceModule(ImportIndex));
				auto FromModule = FindModule(FromModuleName);
				if (!FromModule.IsValid() || FromModule->ScriptModule == nullptr)
				{
					// Don't show error if we had a compile error in that module, we need
					// to fix that first so this error isn't helpful.
					if (!FromModule.IsValid() || !FromModule->bCompileError)
					{
						ScriptCompileError(Module, 1, FString::Printf(
							TEXT("Error resolving import in module %s of function %s: could not find module %s to import from."),
							ANSI_TO_TCHAR(ScriptModule->GetName()), ANSI_TO_TCHAR(Decl), *FromModuleName));
					}
					bModuleValid = false;
					continue;
				}

				asIScriptFunction* Function = FromModule->ScriptModule->GetFunctionByDecl(Decl);
				if (Function == nullptr)
				{
					ScriptCompileError(Module, 1, FString::Printf(
						TEXT("Error resolving import in module %s of function %s: could not find function with this signature in module %s."),
						ANSI_TO_TCHAR(ScriptModule->GetName()), ANSI_TO_TCHAR(Decl), *FromModuleName));
					bModuleValid = false;
					continue;
				}
			}
		}
	
		if (!bModuleValid)
		{
			bValid = false;

			// Make sure this module is added to the next reload
			for (auto& Section : Module->Code)
				PreviouslyFailedReloadFiles.Add(FFilenamePair{ Section.AbsoluteFilename, Section.RelativeFilename, Section.VirtualPath });
		}
	};

	// Check new modules
	for (auto Module : Modules)
		CheckModule(Module);

	// Check any old modules we aren't swapping in
	for (auto OldElem : ActiveModules)
	{
		if (SwappingModules.Contains(OldElem.Value->ModuleName))
			continue;
		CheckModule(OldElem.Value);
	}

	return bValid;
}

void* FAngelscriptEngine::GetCurrentFunctionUserDataPtr()
{
	auto* Function = (asCScriptFunction*)asGetActiveFunction();
	if (Function == nullptr)
		return nullptr;
	return Function->userData;
}

asITypeInfo* FAngelscriptEngine::GetCurrentFunctionObjectType()
{
	auto* Function = (asCScriptFunction*)asGetActiveFunction();
	if (Function == nullptr)
		return nullptr;
	return Function->GetObjectType();
}

asCContext* FAngelscriptEngine::GetCurrentScriptContext()
{
	return (asCContext*)asGetActiveContext();
}

asCContext* FAngelscriptEngine::GetPreviousScriptContext()
{
	auto* tld = asCThreadManager::GetLocalData();
	if (tld->activeContext != nullptr)
		return tld->activeContext;

	auto* Execution = tld->activeExecution;
	while (Execution != nullptr)
	{
		if (Execution->prevContext != nullptr)
			return Execution->prevContext;
		Execution = Execution->prevExecution;
	}

	return nullptr;
}

bool FAngelscriptEngine::IsOutdated(asIScriptFunction* Function)
{
	// Outdated functions will have a null module set,
	// since the module has been discarded.
	return Function->GetModule() == nullptr;
}

void FAngelscriptEngine::SetOutdated(asIScriptModule* OldModule)
{
}

TSharedPtr<FAngelscriptClassDesc> FAngelscriptEngine::GetClass(const FString& ClassName, TSharedPtr<FAngelscriptModuleDesc>* FoundInModule)
{
#if AS_CAN_HOTRELOAD
	if (ActiveClassesByName.Num() != 0)
	{
		auto* FoundEntry = ActiveClassesByName.Find(ClassName);
		if (FoundEntry != nullptr)
		{
			if (FoundInModule != nullptr)
				*FoundInModule = FoundEntry->Key;
			return FoundEntry->Value;
		}
		else
		{
			return nullptr;
		}
	}
#endif

	for (auto ModulePair : ActiveModules)
	{
		auto Module = ModulePair.Value;
		for (auto Class : Module->Classes)
		{
			if(Class->ClassName == ClassName)
			{
				if (FoundInModule != nullptr)
					*FoundInModule = Module;
				return Class;
			}
		}
	}

	if (FoundInModule != nullptr)
		*FoundInModule = nullptr;
	return nullptr;
}

TSharedPtr<FAngelscriptEnumDesc> FAngelscriptEngine::GetEnum(const FString& EnumName, TSharedPtr<FAngelscriptModuleDesc>* FoundInModule)
{
#if AS_CAN_HOTRELOAD
	if (ActiveEnumsByName.Num() != 0)
	{
		auto* FoundEntry = ActiveEnumsByName.Find(EnumName);
		if (FoundEntry != nullptr)
		{
			if (FoundInModule != nullptr)
				*FoundInModule = FoundEntry->Key;
			return FoundEntry->Value;
		}
		else
		{
			return nullptr;
		}
	}
#endif

	for (auto ModulePair : ActiveModules)
	{
		auto Module = ModulePair.Value;
		for (auto EnumDesc : Module->Enums)
		{
			if(EnumDesc->EnumName == EnumName)
			{
				if (FoundInModule != nullptr)
					*FoundInModule = Module;
				return EnumDesc;
			}
		}
	}

	if (FoundInModule != nullptr)
		*FoundInModule = nullptr;
	return nullptr;
}

TSharedPtr<FAngelscriptDelegateDesc> FAngelscriptEngine::GetDelegate(const FString& DelegateName, TSharedPtr<FAngelscriptModuleDesc>* FoundInModule)
{
#if AS_CAN_HOTRELOAD
	if (ActiveDelegatesByName.Num() != 0)
	{
		auto* FoundEntry = ActiveDelegatesByName.Find(DelegateName);
		if (FoundEntry != nullptr)
		{
			if (FoundInModule != nullptr)
				*FoundInModule = FoundEntry->Key;
			return FoundEntry->Value;
		}
		else
		{
			return nullptr;
		}
	}
#endif

	for (auto ModulePair : ActiveModules)
	{
		auto Module = ModulePair.Value;
		for (auto DelegateDesc : Module->Delegates)
		{
			if(DelegateDesc->DelegateName == DelegateName)
			{
				if (FoundInModule != nullptr)
					*FoundInModule = Module;
				return DelegateDesc;
			}
		}
	}

	if (FoundInModule != nullptr)
		*FoundInModule = nullptr;
	return nullptr;
}

bool FAngelscriptFunctionDesc::SignatureMatches(TSharedPtr<FAngelscriptFunctionDesc> OtherFunction, bool bCheckNames) const
{
	if (ReturnType != OtherFunction->ReturnType)
		return false;
	
	return ParametersMatches(OtherFunction, bCheckNames);
}
bool FAngelscriptFunctionDesc::ParametersMatches(TSharedPtr<FAngelscriptFunctionDesc> OtherFunction, bool bCheckNames) const
{
	if (Arguments.Num() != OtherFunction->Arguments.Num())
		return false;

	for (int32 i = 0, ArgCount = Arguments.Num(); i < ArgCount; ++i)
	{
		if (!Arguments[i].IsDefinitionEquivalent(OtherFunction->Arguments[i]))
			return false;
		if (bCheckNames && Arguments[i].ArgumentName != OtherFunction->Arguments[i].ArgumentName)
			return false;
	}

	return true;
}

void FAngelscriptEngine::Throw(const ANSICHAR* Exception)
{
	auto* tld = asCThreadManager::GetLocalData();
	if (tld->activeExecution != nullptr)
	{
		tld->activeExecution->bExceptionThrown = true;
		HandleExceptionFromJIT(Exception);
	}
	else if (tld->activeContext != nullptr)
	{
		tld->activeContext->SetException(Exception);
	}
}

void FAngelscriptEngine::ScriptCompileError(const FString& AbsoluteFilename, const FDiagnostic& Diagnostic)
{
	bDiagnosticsDirty = true;

	auto& FileDiagnostics = Diagnostics.FindOrAdd(AbsoluteFilename);
	FileDiagnostics.Filename = AbsoluteFilename;
	FileDiagnostics.Diagnostics.Add(Diagnostic);

	if (Diagnostic.bIsError)
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Diagnostic.Message);
	}
	else
	{
		UE_LOG(Angelscript, Warning, TEXT("%s"), *Diagnostic.Message);
	}
}

void FAngelscriptEngine::ScriptCompileError(TSharedPtr<FAngelscriptModuleDesc> Module, int32 LineNumber, const FString& Message, bool bIsError)
{
	FAngelscriptEngine::FDiagnostic Diagnostic;
	Diagnostic.Message = Message;
	Diagnostic.Row = LineNumber;
	Diagnostic.Column = 1;
	Diagnostic.bIsError = bIsError;
	Diagnostic.bIsInfo = false;

	if (Module->Code.Num() != 0)
		ScriptCompileError(Module->Code[0].AbsoluteFilename, Diagnostic);
	else
		ScriptCompileError(Module->ModuleName, Diagnostic);
}

void FAngelscriptEngine::ScriptCompileError(UClass* InsideClass, const FString& FunctionName, const FString& Message, bool bIsError)
{
	UASClass* asClass = Cast<UASClass>(InsideClass);
	if (asClass == nullptr)
	{
		//UE_LOG(Angelscript, Warning, TEXT("Failed Cast to UASClass"))
		GLog->Log(TEXT("Failed Cast to UASClass"));
		return;
	}
	//auto* ScriptTypePtr = (asITypeInfo*)InsideClass->ScriptTypePtr;
	auto* ScriptTypePtr = (asITypeInfo*)asClass->ScriptTypePtr;
	if (ScriptTypePtr == nullptr)
	{
		ensureMsgf(false, TEXT("Not a script class."));
		return;
	}

	//auto* ScriptModule = ScriptTypePtr->GetModule();
	asIScriptModule* ScriptModule = ScriptTypePtr->GetModule();
	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc;
	for (auto Elem : ActiveModules)
	{
		if (Elem.Value->ScriptModule == ScriptModule)
		{
			ModuleDesc = Elem.Value;
			break;
		}
	}
	if (!ModuleDesc.IsValid())
	{
		ensureMsgf(false, TEXT("Could not find compiled module."));
		return;
	}

	int32 LineNumber = 1;
	auto ClassDesc = ModuleDesc->GetClass(ScriptTypePtr);
	if (ClassDesc.IsValid())
	{
		LineNumber = ClassDesc->LineNumber;

		if (FunctionName.Len() != 0)
		{
			auto MethodDesc = ClassDesc->GetMethod(FunctionName);
			if (!MethodDesc.IsValid())
				MethodDesc = ClassDesc->GetMethodByScriptName(FunctionName);
			if (MethodDesc.IsValid())
				LineNumber = MethodDesc->LineNumber;
		}
	}

	ScriptCompileError(ModuleDesc, LineNumber, Message, bIsError);
}

void LogAngelscriptError(asSMessageInfo* Message, void* DataPtr)
{
	static FString PreviousSection;
	static int32 PreviousType;

	auto& Manager = FAngelscriptEngine::Get();
	if (Manager.bIgnoreCompileErrorDiagnostics)
		return;

	// Some compilation steps can happen on different threads, so we need to lock sending messages
	FScopeLock MessageLock(&Manager.CompilationLock);

	const FString Section = ANSI_TO_TCHAR(Message->section);
	const bool bHasSection = !Section.IsEmpty();

	bool bPrintSection = false;
	if (bHasSection)
	{
		if (PreviousSection != Section || PreviousType != Message->type)
		{
			PreviousSection = Section;
			PreviousType = Message->type;

			bPrintSection = true;
		}
	}

	FString ErrorMessage;
	if (Message->col || Message->row)
	{
		ErrorMessage = FString::Printf(TEXT("(%d:%d): %s"),
			Message->row, Message->col,
			ANSI_TO_TCHAR(Message->message)
		);
	}
	else
	{
		ErrorMessage = Message->message;
	}

	if (Message->type == asMSGTYPE_INFORMATION)
	{
		if (bPrintSection)
		{
			UE_LOG(Angelscript, Log, TEXT("%s:"), *Section);
		}
		UE_LOG(Angelscript, Log, TEXT(" %s"), *ErrorMessage);
	}
	else if (Message->type == asMSGTYPE_ERROR)
	{
		if (bPrintSection)
		{
			UE_LOG(Angelscript, Error, TEXT("%s:"), *Section);
		}
		UE_LOG(Angelscript, Error, TEXT(" %s"), *ErrorMessage);
	}
	else
	{
		if (bPrintSection)
		{
			UE_LOG(Angelscript, Warning, TEXT("%s:"), *Section);
		}
		UE_LOG(Angelscript, Warning, TEXT(" %s"), *ErrorMessage);
	}

	// Check if this message should be captured as a diagnostic
	auto* FileDiagnostics = Manager.Diagnostics.Find(Section);
	if (FileDiagnostics != nullptr)
	{
		FileDiagnostics->Diagnostics.Add({ ANSI_TO_TCHAR(Message->message), Message->row, Message->col,
			Message->type == asMSGTYPE_ERROR, Message->type == asMSGTYPE_INFORMATION });
		Manager.bDiagnosticsDirty = true;
	}
}

void GetStackTrace(TArray<FString>& OutTrace)
{
	auto* tld = asCThreadManager::GetLocalData();
	asCContext* Context = nullptr;

	struct FStackFrameDescription
	{
		FString Frame;
		FString Module;
		UObject* ThisObject;
	};

	TArray<FStackFrameDescription, TInlineAllocator<16>> Stack;

	if (tld->activeExecution != nullptr)
	{
		FScriptExecution* Execution = tld->activeExecution;
		while (Execution != nullptr)
		{
#if AS_JIT_DEBUG_CALLSTACKS
			auto* DebugStack = (FScopeJITDebugCallstack*)Execution->debugCallStack;
			while (DebugStack != nullptr)
			{
				auto* ThisObject = (UObject*)DebugStack->ThisObject;

				Stack.Add({
					FString::Printf(TEXT("  %s | Line %d"),
									ANSI_TO_TCHAR(DebugStack->FunctionName),
									DebugStack->LineNumber),
					ANSI_TO_TCHAR(DebugStack->Filename),
					ThisObject
				});

				DebugStack = DebugStack->PrevFrame;
			}
#endif

			Context = Execution->prevContext;
			Execution = Execution->prevExecution;
		}
	}
	else
	{
		Context = tld->activeContext;
		if (Context == nullptr)
		{
			OutTrace.Add(TEXT("No Angelscript Context"));
			return;
		}
	}

	if (Context != nullptr)
	{
		int32 FrameCount = FMath::Min((int32)Context->GetCallstackSize(), 64);
		for (int32 i = 0; i < FrameCount; ++i)
		{
			asIScriptFunction* ScriptFunction = Context->GetFunction(i);
			if (ScriptFunction != nullptr)
			{
				int32 Line, Column;
				Line = Context->GetLineNumber(i, &Column, nullptr);

				FStackFrameDescription& Desc = Stack.Emplace_GetRef();
				Desc.Frame = FString::Printf(TEXT("  %s | Line %d | Col %d"),
					ANSI_TO_TCHAR(ScriptFunction->GetDeclaration(true, false, false, true)),
					Line, Column);
				Desc.Module = ANSI_TO_TCHAR(ScriptFunction->GetModuleName());
				Desc.ThisObject = nullptr;

				int ThisTypeId = Context->GetThisTypeId(i);
				if (ThisTypeId != 0)
				{
					auto* ThisType = Context->GetEngine()->GetTypeInfoById(ThisTypeId);
					if (ThisType != nullptr && (ThisType->GetFlags() & asOBJ_REF) != 0)
					{
						// All ref objects are UObjects
						UObject* ThisPtr = (UObject*)Context->GetThisPointer(i);
						Desc.ThisObject = ThisPtr;
					};
				}
			}
		}
	}

	AActor* PreviousThisActor = nullptr;
	OutTrace.Reserve(Stack.Num());

	for (int32 i = Stack.Num() - 1; i >= 0; --i)
	{
		// All ref objects are UObjects
		UObject* ThisPtr = Stack[i].ThisObject;
		AActor* ThisActor = nullptr;

		// Find the actor that contains the object we're in
		while (ThisPtr != nullptr)
		{
			if (Cast<UPackage>(ThisPtr) != nullptr)
			{
				break;
			}
			if (Cast<AActor>(ThisPtr) != nullptr)
			{
				ThisActor = CastChecked<AActor>(ThisPtr);
				break;
			}

			ThisPtr = ThisPtr->GetOuter();
		}

		// Display the found actor unless we already displayed it earlier
		if (ThisActor != nullptr && ThisActor != PreviousThisActor)
		{
			FString OuterStr;
			if (auto* InLevel = ThisActor->GetLevel())
			{
				if (auto* LevelWorld = InLevel->GetOuter())
				{
					OuterStr = FString::Printf(TEXT(" in %s"), *LevelWorld->GetName());
				}
			}

#if WITH_EDITOR
			OutTrace.Insert(FString::Printf(TEXT("    (Actor: %s (Label: %s)%s)"),
				*ThisActor->GetName(), *ThisActor->GetActorLabel(), *OuterStr), 0);
#else
			OutTrace.Insert(FString::Printf(TEXT("    (Actor: %s%s)"),
				*ThisActor->GetName(), *OuterStr), 0);
#endif
			PreviousThisActor = ThisActor;
		}

		OutTrace.Insert(MoveTemp(Stack[i].Frame), 0);

		// Show the module name on top of the stack
		if (i == 0)
			OutTrace.Insert(MoveTemp(Stack[i].Module), 0);
	}
}

void LogScriptStack()
{
	TArray<FString> Trace;
	GetStackTrace(Trace);

	for (FString& Line : Trace)
		UE_LOG(Angelscript, Warning, TEXT("%s"),*Line);
}

FString GetScriptStack()
{
	TArray<FString> Trace;
	GetStackTrace(Trace);
	return FString::Join(Trace, TEXT("\n"));
}

void LogAngelscriptException(const ANSICHAR* ExceptionString)
{
	if (FAngelscriptScriptTestRunner::IsControlledException(ExceptionString)
		|| FAngelscriptScriptTestRunner::
			ShouldSuppressScriptExceptionLogging())
	{
		return;
	}

#if WITH_AS_DEBUGSERVER
	if (FAngelscriptEngine::Get().IsEvaluatingDebuggerWatch())
		return;
#endif

	TGuardValue<bool> LineReentry(GAngelscriptLineReentry, true);

	if (ExceptionString == nullptr)
		ExceptionString = "NO EXCEPTION";

	UE_LOG(Angelscript, Error, TEXT("%s"), ANSI_TO_TCHAR(ExceptionString));

	TArray<FString> Trace;
	GetStackTrace(Trace);

	for (FString& Line : Trace)
		UE_LOG(Angelscript, Error, TEXT("%s"),*Line);

	// Print angelscript exceptions on screen
	if (GEngine != nullptr)
	{
		UKismetSystemLibrary::PrintString(
			GAmbientWorldContext,
			FString::Printf(
				TEXT("Angelscript Exception: %s\n%s"),
				ANSI_TO_TCHAR(ExceptionString),
				Trace.Num() >= 2 ? *Trace[1] : TEXT("")),
			true, false,
			FLinearColor::Red, 30.f);
	}

}

void LogAngelscriptException(asIScriptContext* Context)
{
	const ANSICHAR* ExceptionString = Context->GetExceptionString();
	if (FAngelscriptScriptTestRunner::IsControlledException(ExceptionString)
		|| FAngelscriptScriptTestRunner::
			ShouldSuppressScriptExceptionLogging())
	{
		return;
	}
	LogAngelscriptException(ExceptionString);

#if WITH_AS_DEBUGSERVER
	if (IsInGameThread())
	{
		if (auto* DebugServer = FAngelscriptEngine::Get().DebugServer)
			DebugServer->ProcessException(Context);
	}
#endif
}

void FAngelscriptEngine::HandleExceptionFromJIT(const ANSICHAR* ExceptionString)
{
	LogAngelscriptException(ExceptionString);
}

void FAngelscriptEngine::TraceError(const ANSICHAR* Error)
{
	UE_LOG(Angelscript, Error, TEXT("%s"), ANSI_TO_TCHAR(Error));

	TArray<FString> Trace;
	GetStackTrace(Trace);

	for (FString& Line : Trace)
		UE_LOG(Angelscript, Error, TEXT("%s"),*Line);
}

FAngelscriptEngine::FAngelscriptDebugStack& GetStack(asIScriptContext* Context)
{
	asCContext* Ctx = (asCContext*)Context;
	if (Ctx->DebugFramePtr == nullptr)
	{
		Ctx->DebugFramePtr = new FAngelscriptEngine::FAngelscriptDebugStack;
	}
	return *(FAngelscriptEngine::FAngelscriptDebugStack*)Ctx->DebugFramePtr;
}

FDebugValuePrototype* GetDebugPrototype(asIScriptFunction* Function)
{
	asCScriptFunction* Func = (asCScriptFunction*)Function;
	if (Func->DebugPrototypePtr != nullptr)
		return (FDebugValuePrototype*)Func->DebugPrototypePtr;
	if (Func->scriptData == nullptr)
		return nullptr;

	FDebugValuePrototype* Proto = new FDebugValuePrototype;

	int32 VarCount = Function->GetVarCount();
	for (int32 i = 0; i < VarCount; ++i)
	{
		const char* VarName;
		int VarTypeId;

		Function->GetVar(i, &VarName, &VarTypeId);

		FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromTypeId(VarTypeId);
		if (!Type.IsValid())
			continue;

		int32 Offset = Func->scriptData->variables[i]->stackOffset;
		if( (Func->scriptData->variables[i]->type.IsObject() && !Func->scriptData->variables[i]->type.IsObjectHandle()) || (Offset <= 0) )
		{
			// Determine if the object is really on the heap
			bool onHeap = false;
			if( Func->scriptData->variables[i]->type.IsObject() &&
				!Func->scriptData->variables[i]->type.IsObjectHandle() )
			{
				onHeap = true;
				if( Func->scriptData->variables[i]->type.GetTypeInfo()->GetFlags() & asOBJ_VALUE )
				{
					for( asUINT n = 0; n < Func->scriptData->objVariablePos.GetLength(); n++ )
					{
						if( Func->scriptData->objVariablePos[n] == Offset )
						{
							onHeap = n < Func->scriptData->objVariablesOnHeap;
							break;
						}
					}
				}
			}

			// If it wasn't an object on the heap, then check if it is a reference parameter
			if( !onHeap && Offset <= 0 )
			{
				// Determine what function argument this position matches
				int stackPos = 0;
				if( Func->objectType )
					stackPos -= AS_PTR_SIZE;

				if( Func->DoesReturnOnStack() )
					stackPos -= AS_PTR_SIZE;

				for( asUINT n = 0; n < Func->parameterTypes.GetLength(); n++ )
				{
					if( stackPos == Offset )
					{
						// The right argument was found. Is this a reference parameter?
						if( Func->inOutFlags[n] != asTM_NONE )
							onHeap = true;
						break;
					}

					stackPos -= Func->parameterTypes[n].GetSizeOnStackDWords();
				}
			}

			// Heap variables are references on the stack
			if (onHeap)
				Type.bIsReference = true;
		}

		FASDebugValue* DebugValue = Type.CreateDebugValue(*Proto, -Offset * 4);
		if (DebugValue != nullptr)
			DebugValue->Name = FName(ANSI_TO_TCHAR(VarName));
	}

	Func->DebugPrototypePtr = Proto;
	return Proto;
}

FAngelscriptEngine::FAngelscriptDebugFrame::~FAngelscriptDebugFrame()
{
#if WITH_AS_DEBUGVALUES
	if (Variables != nullptr)
		Prototype->Free(Variables);
#endif
}

void FAngelscriptEngine::UpdateLineCallbackState()
{
	bool bEverRunLineCallback = false;
	bool bAlwaysRunLineCallback = false;

#if WITH_AS_DEBUGSERVER
	if (DebugServer != nullptr)
	{
		if (DebugServer->bIsDebugging)
			bEverRunLineCallback = true;
		if (DebugServer->DataBreakpoints.Num() != 0)
			bEverRunLineCallback = true;
		if (DebugServer->bBreakNextScriptLine)
			bAlwaysRunLineCallback = true;
	}
#endif

#if WITH_AS_COVERAGE
	if (FAngelscriptCodeCoverageExtension::GetForEngine(*this) != nullptr)
	{
		bEverRunLineCallback = true;
		bAlwaysRunLineCallback = true;
	}
#endif

#if WITH_AS_DEBUGVALUES
	bEverRunLineCallback = true;
	bAlwaysRunLineCallback = true;
#endif

	asCContext::CanEverRunLineCallback = bEverRunLineCallback;
	asCContext::ShouldAlwaysRunLineCallback = bAlwaysRunLineCallback;
}

void AngelscriptLineCallback(asCContext* Context)
{
	// Only do this for things running on the game thread
	if (!IsInGameThread())
		return;

	// Guard for reentry on this function. Script called
	// inside of a line callback is not considered for line callbacks.
	if (GAngelscriptLineReentry)
		return;
	GAngelscriptLineReentry = true;

	FAngelscriptEngine& AngelscriptManager = FAngelscriptEngine::Get();

#if WITH_AS_DEBUGVALUES
	auto& Stack = GetStack(Context);
	GAngelscriptStack = &Stack;

	int32 StackSize = Context->GetCallstackSize();
	if (StackSize != Stack.Frames.Num()
		|| (StackSize != 0 && Stack.Frames[0].ScriptFunction != Context->GetFunction(0)))
	{
		Stack.Frames.SetNum(StackSize, false);

		for (int32 i = 0; i < StackSize; ++i)
		{
			auto& Frame = Stack.Frames[i];
			auto* ScriptFunction = Context->GetFunction(i);
			if (ScriptFunction == Frame.ScriptFunction)
				continue;

			Frame.ScriptFunction = Context->GetFunction(i);
			Frame.LineNumber = Context->GetLineNumber(i, nullptr, &Frame.File);

			if (Frame.Prototype && Frame.Variables)
			{
				Frame.Prototype->Free(Frame.Variables);
				Frame.Variables = nullptr;
			}

			if (Frame.ScriptFunction != nullptr)
			{
				Frame.Function = Frame.ScriptFunction->GetName();
				auto* ScriptClass = Frame.ScriptFunction->GetObjectType();
				Frame.Class = ScriptClass ? ScriptClass->GetName() : nullptr;

				Frame.Prototype = GetDebugPrototype(Frame.ScriptFunction);
				if (Frame.Prototype != nullptr)
				{
					Frame.Variables = (FDebugValues*)Frame.Prototype->Instantiate(
						((asCContext*)Context)->GetStackFrame(i)
					);
				}
			}
			else
			{
				Frame.Function = nullptr;
				Frame.Class = nullptr;
				Frame.Prototype = nullptr;
			}

			Frame.This = (UObject*)Context->GetThisPointer(i);
		}
	}
	else if(StackSize != 0)
	{
		auto& Frame = Stack.Frames[0];
		Frame.LineNumber = Context->GetLineNumber(0, nullptr, nullptr);
	}
#endif

#if WITH_AS_DEBUGSERVER
	if (auto* DebugServer = AngelscriptManager.DebugServer)
		DebugServer->ProcessScriptLine(Context);
#endif

#if WITH_AS_COVERAGE
	if (FAngelscriptCodeCoverage* CodeCoverage = FAngelscriptCodeCoverageExtension::GetForEngine(AngelscriptManager))
	{
		int Column;
		int Line = Context->GetLineNumber(0, &Column, nullptr);
		asIScriptFunction* CurrentFunction = Context->GetFunction(0);
		FString ModuleName = ANSI_TO_TCHAR(CurrentFunction->GetModuleName());
		TSharedPtr<struct FAngelscriptModuleDesc> Module = AngelscriptManager.GetModule(ModuleName);
		if (Module != nullptr)
		{
			CodeCoverage->HitLine(*Module, Line);
		}
	}
#endif

	GAngelscriptLineReentry = false;
}

void AngelscriptStackPopCallback(asCContext* Context, void* OldStackFrameStart, void* OldStackFrameEnd)
{
#if WITH_AS_DEBUGSERVER
	FAngelscriptEngine& AngelscriptManager = FAngelscriptEngine::Get();
	if (auto* DebugServer = AngelscriptManager.DebugServer)
		DebugServer->ProcessScriptStackPop(Context, OldStackFrameStart, OldStackFrameEnd);
#endif
}

void AngelscriptLoopDetectionCallback(asCContext* Context)
{
	float MaximumScriptExecutionTime = UAngelscriptSettings::Get().EditorMaximumScriptExecutionTime;
	if (MaximumScriptExecutionTime > 0)
	{
		if (Context->m_loopDetectionExclusionCounter != 0)
			return;

		// Loop detection triggers every 100,000 executed lines of script code or so,
		// and should kill script functions that run for too long.
		// Note that loop detection won't happen in release builds.
		double CurrentTime = FPlatformTime::Seconds();
		if (Context->m_loopDetectionTimer == -1.0)
		{
			// No time has been established for this context yet, so set it and see if we time out later
			Context->m_loopDetectionTimer = CurrentTime;
			return;
		}

		if (Context->m_loopDetectionTimer < CurrentTime - MaximumScriptExecutionTime)
		{
			Context->SetException("Script function took too long to execute. Potentially an infinite loop? (timeout controlled by EditorMaximumScriptExecutionTime setting)");
			return;
		}
	}
}

#if WITH_EDITOR
FAngelscriptExcludeScopeFromLoopTimeout::FAngelscriptExcludeScopeFromLoopTimeout()
{
	Context = (asCContext*)asGetActiveContext();
	if (Context != nullptr)
	{
		Context->m_loopDetectionExclusionCounter += 1;
		StartTime = FPlatformTime::Seconds();
	}
}

FAngelscriptExcludeScopeFromLoopTimeout::~FAngelscriptExcludeScopeFromLoopTimeout()
{
	if (Context != nullptr)
	{
		Context->m_loopDetectionExclusionCounter -= 1;

		// If the scope took 1 second we remove 1 second from the timeout
		if (Context->m_loopDetectionExclusionCounter == 0 && Context->m_loopDetectionTimer != -1.0)
		{
			double NowTime = FPlatformTime::Seconds();
			Context->m_loopDetectionTimer = FMath::Min(Context->m_loopDetectionTimer + (NowTime - StartTime), NowTime);
		}
	}
}
#endif

TArray<FString> FAngelscriptEngine::GetAngelscriptCallstack()
{
	TArray<FString> Trace;
	GetStackTrace(Trace);
	return Trace;
}

FString FAngelscriptEngine::FormatAngelscriptCallstack()
{
	return GetScriptStack();
}

FString FAngelscriptEngine::GetAngelscriptExecutionPosition()
{
	auto* tld = asCThreadManager::GetLocalData();
	if (tld->activeExecution != nullptr)
	{
#if AS_JIT_DEBUG_CALLSTACKS
		auto* DebugStack = (FScopeJITDebugCallstack*)tld->activeExecution->debugCallStack;
		if (DebugStack == nullptr)
			return TEXT("");

		return FString::Printf(TEXT("%s::%d"),
			ANSI_TO_TCHAR(DebugStack->Filename),
			DebugStack->LineNumber);
#else
		return TEXT("");
#endif
	}
	else
	{
		auto* Context = asGetActiveContext();
		if (Context == nullptr)
			return TEXT("");

		if (Context->GetCallstackSize() == 0)
			return TEXT("");

		const char* Filename;
		int32 LineNumber = Context->GetLineNumber(0, nullptr, &Filename);

		return FString::Printf(TEXT("%s::%d"), ANSI_TO_TCHAR(Filename), LineNumber);
	}
}

void FAngelscriptEngine::GetAngelscriptExecutionFileAndLine(FString& OutFilename, int& OutLineNumber)
{
	auto* tld = asCThreadManager::GetLocalData();
	if (tld->activeExecution != nullptr)
	{
#if AS_JIT_DEBUG_CALLSTACKS
		auto* DebugStack = (FScopeJITDebugCallstack*)tld->activeExecution->debugCallStack;
		if (DebugStack == nullptr)
		{
			OutFilename = ANSI_TO_TCHAR(DebugStack->Filename);
			OutLineNumber = DebugStack->LineNumber;
			return;
		}
#endif

		OutFilename = TEXT("");
		OutLineNumber = -1;
	}
	else
	{
		auto* Context = asGetActiveContext();
		if (Context == nullptr)
			return;

		if (Context->GetCallstackSize() == 0)
			return;

		const char* Filename;
		int32 LineNumber = Context->GetLineNumber(0, nullptr, &Filename);

		OutFilename = ANSI_TO_TCHAR(Filename);
		OutLineNumber = LineNumber;
	}
}

UObject* FAngelscriptEngine::GetAngelscriptExecutionThisObject(int32 StackFrame)
{
	auto* Context = asGetActiveContext();
	if (Context == nullptr)
		return nullptr;

	void* ThisPtr = Context->GetThisPointer(StackFrame);
	if (ThisPtr != nullptr)
	{
		asITypeInfo* ThisType = Context->GetEngine()->GetTypeInfoById(Context->GetThisTypeId(StackFrame));
		if (ThisType != nullptr && (ThisType->GetFlags() & asOBJ_REF) != 0)
		{
			return (UObject*)ThisPtr;
		}
	}
	return nullptr;
}

bool FAngelscriptEngine::TryBreakpointAngelscriptDebugging(const TCHAR* Message)
{
#if WITH_AS_DEBUGSERVER
	auto& Manager = FAngelscriptEngine::Get();
	if (Manager.DebugServer == nullptr)
		return false;
	if (!Manager.DebugServer->bIsDebugging)
		return false;
	if (Manager.DebugServer->bIsPaused)
		return false;

	auto* Context = asGetActiveContext();
	if (Context == nullptr)
		return false;

	FStoppedMessage StopMessage;
	if (Message != nullptr)
	{
		StopMessage.Reason = TEXT("exception");
		StopMessage.Text = Message;
	}
	else
	{
		StopMessage.Reason = TEXT("breakpoint");
	}

	Manager.DebugServer->PauseExecution(&StopMessage);
	return true;
#else
	return false;
#endif
}


UStruct* FAngelscriptEngine::GetUnrealStructFromAngelscriptTypeId(int TypeId)
{
	auto* TypeInfo = (asCTypeInfo*)Engine->GetTypeInfoById(TypeId);
	if (TypeInfo == nullptr)
		return nullptr;
	if (TypeInfo->GetSubTypeCount() != 0)
		return nullptr;
	void* UserData = (void*)TypeInfo->plainUserData;
	if (UserData == FAngelscriptType::TAG_UserData_Delegate)
		return nullptr;
	if (UserData == FAngelscriptType::TAG_UserData_Multicast_Delegate)
		return nullptr;
	if (UserData != nullptr && Cast<UDelegateFunction>((UObject*)UserData) != nullptr)
		return nullptr;
	if ((TypeInfo->flags & asOBJ_ENUM) != 0)
		return nullptr;
	return (UStruct*)UserData;
}

#if AS_PRINT_STATS
FAngelscriptScopeTimer::FAngelscriptScopeTimer(const TCHAR* InName)
	: StartTime(FPlatformTime::Seconds())
	, Name(InName)
{
}

FAngelscriptScopeTimer::~FAngelscriptScopeTimer()
{
	double EndTime = FPlatformTime::Seconds();
	OutputTime(*Name, EndTime - StartTime);
}

void FAngelscriptScopeTimer::OutputTime(const TCHAR* Name, double Time)
{
	UE_LOG(Angelscript, Log, TEXT("%s took %.3f ms"), Name, Time * 1000);
}
#endif
#if AS_PRINT_STATS && AS_PRECOMPILED_STATS
FAngelscriptScopeTotalTimer::FAngelscriptScopeTotalTimer(double& TotalTime)
	: Timer(&TotalTime)
	, StartTime(FPlatformTime::Seconds())
{
}

FAngelscriptScopeTotalTimer::~FAngelscriptScopeTotalTimer()
{
	double EndTime = FPlatformTime::Seconds();
	*Timer += (EndTime - StartTime);
}
#endif

// Copied from FPaths::MakePathRelativeTo, but with a case-insensitive equals
bool MakePathRelativeTo_IgnoreCase( FString& InPath, const TCHAR* InRelativeTo )
{
	FString Target = FPaths::ConvertRelativePathToFull(InPath);
	FString Source = FPaths::ConvertRelativePathToFull(InRelativeTo);
	
	Source = FPaths::GetPath(Source);
	Source.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	Target.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	TArray<FString> TargetArray;
	Target.ParseIntoArray(TargetArray, TEXT("/"), true);
	TArray<FString> SourceArray;
	Source.ParseIntoArray(SourceArray, TEXT("/"), true);

	if (TargetArray.Num() && SourceArray.Num())
	{
		// Check for being on different drives
		if ((TargetArray[0][1] == TEXT(':')) && (SourceArray[0][1] == TEXT(':')))
		{
			if (FChar::ToUpper(TargetArray[0][0]) != FChar::ToUpper(SourceArray[0][0]))
			{
				// The Target and Source are on different drives... No relative path available.
				return false;
			}
		}
	}

	while (TargetArray.Num() && SourceArray.Num() && TargetArray[0].Equals(SourceArray[0], ESearchCase::IgnoreCase))
	{
		TargetArray.RemoveAt(0);
		SourceArray.RemoveAt(0);
	}
	FString Result;
	for (int32 Index = 0; Index < SourceArray.Num(); Index++)
	{
		Result += TEXT("../");
	}
	for (int32 Index = 0; Index < TargetArray.Num(); Index++)
	{
		Result += TargetArray[Index];
		if (Index + 1 < TargetArray.Num())
		{
			Result += TEXT("/");
		}
	}
	
	InPath = Result;
	return true;
}

ANGELSCRIPTRUNTIME_API double asStringScanDouble(const char *string)
{
	return FCStringAnsi::Atod(string);
}

ANGELSCRIPTRUNTIME_API float asStringScanFloat(const char *string)
{
	return FCStringAnsi::Atof(string);
}

static bool asStringEquals(const asCString& ASString, const FString& UnrealString)
{
	int32 Length = UnrealString.Len();
	if (Length != ASString.GetLength())
		return false;

	const auto* APtr = ASString.AddressOf();
	const auto* BPtr = *UnrealString;

	for (int32 i = 0; i < Length; ++i)
	{
		if (APtr[i] != BPtr[i])
			return false;
	}

	return true;
}

TSharedPtr<FAngelscriptPropertyDesc> FAngelscriptClassDesc::GetProperty(asCString& PropName)
{
	int32 Length = PropName.GetLength();
	for (auto PropDesc : Properties)
	{
		if (asStringEquals(PropName, PropDesc->PropertyName))
			return PropDesc;
	}

	return nullptr;
}
