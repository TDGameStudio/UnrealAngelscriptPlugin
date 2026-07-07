#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/CoreNetTypes.h"
#include "ClassGenerator/AngelscriptAdditionalCompileChecks.h"

#include "AngelscriptSource.h"
#include "AngelscriptSourceProvider.h"
#include "AngelscriptType.h"
#include "AngelscriptMemoryTags.h"

#include "AngelscriptEngine.generated.h"

#define AS_CAN_HOTRELOAD (PLATFORM_DESKTOP)
#define AS_MAX_POOLED_CONTEXTS 10
#define AS_PRINT_STATS (!UE_BUILD_SHIPPING)
#define AS_PRECOMPILED_STATS 1
#define WITH_AS_DEBUGSERVER (!UE_BUILD_TEST && !UE_BUILD_SHIPPING)
#define AS_USE_BIND_DB (!WITH_EDITOR)
#define AS_ITERATOR_DEBUGGING (WITH_EDITOR)
#define AS_REFERENCE_DEBUGGING (WITH_EDITOR)
#define WITH_AS_COVERAGE WITH_AS_DEBUGSERVER

#ifndef AS_ENFORCE_SERVER_RPC_VALIDATION
#define AS_ENFORCE_SERVER_RPC_VALIDATION 0
#endif

ANGELSCRIPTRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(Angelscript, Log, All);

class asIScriptEngine;
class asIScriptContext;
class asIScriptModule;
class asIScriptFunction;
class asIScriptObject;
class asITypeInfo;
class asCContext;
class FAngelscriptBindDatabase;
struct FAngelscriptTypeDatabase;
struct FAngelscriptBindState;
struct FToStringType;

class FHotReloadTestRunner;
class FBlueprintEventSignatureRegistry;
struct FAngelscriptEngineLifetimeToken;
struct FAngelscriptEngineContextStack;
struct FAngelscriptEngineScope;

struct FStaticJITDiagnostics;
struct FAngelscriptStateSnapshotBuilder;

// Hook-surface forward decls. Previously these lived in a separate
// hooks header along with a small container struct. The struct was
// inlined onto FAngelscriptEngine so that hook subscribers can write
// `Engine.GetOnPostReload()` directly. The container existed only as a
// stylistic wrapper -- there were never any consumers that used it
// beyond the accessor -- so removing it eliminated 133 indirect call
// sites without losing any encapsulation property.
class UActorComponent;
class ULevel;
class UASClass;
class UClass;
class UEnum;
class UScriptStruct;
class UDelegateFunction;

struct FAngelscriptClassDesc;
struct FAngelscriptModuleDesc;

typedef TArray<FName> FAngelscriptDebugBreakOptions;
typedef TMap<FName, FString> FAngelscriptDebugBreakFilters;
typedef const TArray<TPair<FName, int64>>& EnumNameList;

DECLARE_DELEGATE_RetVal(class ULevel*, FAngelscriptGetDynamicSpawnLevel);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FAngelscriptDebugCheckBreakOptions, const FAngelscriptDebugBreakOptions&, UObject*);
DECLARE_DELEGATE_OneParam(FAngelscriptGetDebugBreakFilters, FAngelscriptDebugBreakFilters&);
DECLARE_DELEGATE_TwoParams(FAngelscriptDebugObjectSuffix, UObject*, FString&);
DECLARE_DELEGATE_OneParam(FAngelscriptComponentCreated, class UActorComponent*);
DECLARE_DELEGATE_ThreeParams(FAngelscriptClassAnalyzeDelegate, FString&, TSharedPtr<struct FAngelscriptClassDesc>, bool&);
DECLARE_MULTICAST_DELEGATE_OneParam(FAngelscriptPostCompileClassCollection, const TArray<TSharedRef<struct FAngelscriptModuleDesc>>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FAngelscriptPreGenerateClasses, const TArray<TSharedRef<struct FAngelscriptModuleDesc>>&);
DECLARE_MULTICAST_DELEGATE(FAngelscriptCompilationDelegate);
DECLARE_MULTICAST_DELEGATE_TwoParams(FAngelscriptLiteralAssetCreated, UObject*, const FString&);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAngelscriptPostReload, bool);
DECLARE_MULTICAST_DELEGATE(FOnAngelscriptFullReload);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptLiteralAssetReload, UObject*, UObject*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptClassReload, UClass*, UClass*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAngelscriptEnumCreated, UEnum*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptEnumChanged, UEnum*, EnumNameList);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptStructReload, UScriptStruct*, UScriptStruct*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptDelegateReload, UDelegateFunction*, UDelegateFunction*);

ANGELSCRIPTRUNTIME_API bool PrepareAngelscriptContextWithLog(class asIScriptContext* Context, class asIScriptFunction* ScriptFunction, const TCHAR* Callsite);

struct FInterfaceMethodSignature
{
	FName FunctionName;
};

USTRUCT()
struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineConfig
{
	GENERATED_BODY()

	UPROPERTY()
	bool bForceThreadedInitialize = false;

	UPROPERTY()
	bool bSkipThreadedInitialize = false;

	// When true, FAngelscriptEngine::Create routes to InitializeWithoutInitialCompile()
	// (binds + runtime services, but no on-disk script scan / initial compile).
	// Default false reflects the production path (full Initialize). Test fixtures
	// set this via FAngelscriptTestEngine::Create; AngelscriptEditor tests set it
	// inline before calling FAngelscriptEngine::Create. See OpenSpec
	// `refactor-as-engine-clone-removal` D8 / Section 7.
	UPROPERTY()
	bool bSkipInitialCompile = false;

	UPROPERTY()
	bool bSimulateCooked = false;

	UPROPERTY()
	bool bTestErrors = false;

	UPROPERTY()
	bool bForcePreprocessEditorCode = false;

	UPROPERTY()
	bool bGeneratePrecompiledData = false;

	UPROPERTY()
	bool bDevelopmentMode = false;

	UPROPERTY()
	bool bIgnorePrecompiledData = false;

	// When generating precompiled data, skip the StaticJIT C++ transpilation pass
	// (only emit the PrecompiledScript.Cache bytecode archive). Useful for shipping
	// scripts in a package without StaticJIT, and to avoid transpiler limitations on
	// certain script constructs.
	UPROPERTY()
	bool bSkipStaticJITCodeGen = false;

	UPROPERTY()
	bool bSkipWriteBindDB = false;

	UPROPERTY()
	bool bWriteBindDB = false;

	UPROPERTY()
	bool bExitOnError = false;

	UPROPERTY()
	bool bDumpDocumentation = false;

	UPROPERTY()
	int32 DebugServerPort = 27099;

	UPROPERTY()
	bool bIsEditor = false;

	UPROPERTY()
	bool bRunningCommandlet = false;

	UPROPERTY()
	TSet<FName> DisabledBindNames;

	static FAngelscriptEngineConfig FromCurrentProcess();
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptPluginScriptRoot
{
	FString PluginName;
	FString ScriptRoot;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineDependencies
{
	TFunction<FString()> GetProjectDir;
	TFunction<FString(const FString&)> ConvertRelativePathToFull;
	TFunction<bool(const FString&)> DirectoryExists;
	TFunction<bool(const FString&, bool)> MakeDirectory;
	TFunction<TArray<FString>()> GetEnabledPluginScriptRoots;
	TFunction<TArray<FAngelscriptPluginScriptRoot>()> GetEnabledPluginScriptRootDescriptors;
	TSharedPtr<IAngelscriptSourceProvider> SourceProvider;

	static FAngelscriptEngineDependencies CreateDefault();
};

enum class ECompileType : uint8
{
	Initial,
	SoftReloadOnly,
	FullReload,
};

enum class ECompileResult : uint8
{
	Error,
	ErrorNeedFullReload,
	PartiallyHandled,
	FullyHandled,
};

USTRUCT()
struct ANGELSCRIPTRUNTIME_API FAngelscriptEngine
{
	GENERATED_BODY()

	FAngelscriptEngine();
	explicit FAngelscriptEngine(const FAngelscriptEngineConfig& InConfig, const FAngelscriptEngineDependencies& InDependencies);
	~FAngelscriptEngine();

	static TUniquePtr<FAngelscriptEngine> Create(const FAngelscriptEngineConfig& InConfig, const FAngelscriptEngineDependencies& InDependencies);
	static FAngelscriptEngine* TryGetCurrentEngine();
	static FAngelscriptEngine& Get();
	static bool IsInitialized();
	static FString GetScriptRootDirectory();
	static UPackage* GetPackage();
	static UObject* TryGetCurrentWorldContextObject();
	static UObject* GetAmbientWorldContext();
	static bool ShouldUseEditorScriptsForCurrentContext();
	static bool ShouldUseAutomaticImportMethodForCurrentContext();

	// True once an owned AngelScript engine has been released during process exit.
	// After this point the UObject system may still purge script-backed structs whose
	// owning asITypeInfo/engine is gone; callers (e.g. UASStruct destruction) must skip
	// running script destructors to avoid dereferencing a freed engine.
	static bool AreEnginesReleasedForExit();
	static class asCThreadLocalData* GameThreadTLD;
	static bool bStaticJITTranspiledCodeLoaded;

	// Hook accessors. Previously these lived on a separate hooks
	// container exposed via an accessor; inlined here because the
	// container had no consumers beyond that accessor and its only
	// effect was forcing every subscriber through a redundant indirection.
	//
	// Subscription pattern: most callers should still use the
	// IAngelscriptExtension / FAngelscriptEngineExtensionRegistry pattern so
	// that subscriptions attach/detach with engine lifetime; direct AddLambda
	// calls remain valid for short-lived test fixtures.

	FAngelscriptGetDynamicSpawnLevel& GetDynamicSpawnLevel() { return DynamicSpawnLevel; }
	const FAngelscriptGetDynamicSpawnLevel& GetDynamicSpawnLevel() const { return DynamicSpawnLevel; }

	FAngelscriptDebugCheckBreakOptions& GetDebugCheckBreakOptions() { return DebugCheckBreakOptions; }
	const FAngelscriptDebugCheckBreakOptions& GetDebugCheckBreakOptions() const { return DebugCheckBreakOptions; }

	FAngelscriptGetDebugBreakFilters& GetDebugBreakFilters() { return DebugBreakFilters; }
	const FAngelscriptGetDebugBreakFilters& GetDebugBreakFilters() const { return DebugBreakFilters; }

	FAngelscriptDebugObjectSuffix& GetDebugObjectSuffix() { return DebugObjectSuffix; }
	const FAngelscriptDebugObjectSuffix& GetDebugObjectSuffix() const { return DebugObjectSuffix; }

	FAngelscriptComponentCreated& GetComponentCreated() { return ComponentCreated; }
	const FAngelscriptComponentCreated& GetComponentCreated() const { return ComponentCreated; }

	FAngelscriptCompilationDelegate& GetPreCompile() { return PreCompile; }
	const FAngelscriptCompilationDelegate& GetPreCompile() const { return PreCompile; }

	FAngelscriptCompilationDelegate& GetPostCompile() { return PostCompile; }
	const FAngelscriptCompilationDelegate& GetPostCompile() const { return PostCompile; }

	FAngelscriptCompilationDelegate& GetOnInitialCompileFinished() { return OnInitialCompileFinished; }
	const FAngelscriptCompilationDelegate& GetOnInitialCompileFinished() const { return OnInitialCompileFinished; }

	FAngelscriptClassAnalyzeDelegate& GetClassAnalyze() { return ClassAnalyze; }
	const FAngelscriptClassAnalyzeDelegate& GetClassAnalyze() const { return ClassAnalyze; }

	FAngelscriptPreGenerateClasses& GetPreGenerateClasses() { return PreGenerateClasses; }
	const FAngelscriptPreGenerateClasses& GetPreGenerateClasses() const { return PreGenerateClasses; }

	FAngelscriptPostCompileClassCollection& GetPostCompileClassCollection() { return PostCompileClassCollection; }
	const FAngelscriptPostCompileClassCollection& GetPostCompileClassCollection() const { return PostCompileClassCollection; }

	FAngelscriptLiteralAssetCreated& GetOnLiteralAssetCreated() { return OnLiteralAssetCreated; }
	const FAngelscriptLiteralAssetCreated& GetOnLiteralAssetCreated() const { return OnLiteralAssetCreated; }

	FAngelscriptLiteralAssetCreated& GetPostLiteralAssetSetup() { return PostLiteralAssetSetup; }
	const FAngelscriptLiteralAssetCreated& GetPostLiteralAssetSetup() const { return PostLiteralAssetSetup; }

	FOnAngelscriptClassReload& GetOnClassReload() { return OnClassReload; }
	const FOnAngelscriptClassReload& GetOnClassReload() const { return OnClassReload; }

	FOnAngelscriptEnumCreated& GetOnEnumCreated() { return OnEnumCreated; }
	const FOnAngelscriptEnumCreated& GetOnEnumCreated() const { return OnEnumCreated; }

	FOnAngelscriptEnumChanged& GetOnEnumChanged() { return OnEnumChanged; }
	const FOnAngelscriptEnumChanged& GetOnEnumChanged() const { return OnEnumChanged; }

	FOnAngelscriptStructReload& GetOnStructReload() { return OnStructReload; }
	const FOnAngelscriptStructReload& GetOnStructReload() const { return OnStructReload; }

	FOnAngelscriptDelegateReload& GetOnDelegateReload() { return OnDelegateReload; }
	const FOnAngelscriptDelegateReload& GetOnDelegateReload() const { return OnDelegateReload; }

	FOnAngelscriptFullReload& GetOnFullReload() { return OnFullReload; }
	const FOnAngelscriptFullReload& GetOnFullReload() const { return OnFullReload; }

	FOnAngelscriptPostReload& GetOnPostReload() { return OnPostReload; }
	const FOnAngelscriptPostReload& GetOnPostReload() const { return OnPostReload; }

	FOnAngelscriptLiteralAssetReload& GetOnLiteralAssetReload() { return OnLiteralAssetReload; }
	const FOnAngelscriptLiteralAssetReload& GetOnLiteralAssetReload() const { return OnLiteralAssetReload; }

	bool bSimulateCooked = false;
	bool bTestErrors = false;
	bool bIsHotReloading = false;
	bool bForcePreprocessEditorCode = false;
	bool bUseEditorScripts = false;
	bool bUseAutomaticImportMethod = false;
	bool bGeneratePrecompiledData = false;

	static bool IsSimulatingCookedForCurrentContext();
	static bool IsTestingErrorsForCurrentContext();
	static bool IsHotReloadingForCurrentContext();
	static bool IsForcingPreprocessEditorCodeForCurrentContext();
	static bool IsScriptDevelopmentModeForCurrentContext();

	void Initialize();
	/* Initialize bindings and runtime services without scanning Script roots or compiling disk scripts. */
	void InitializeWithoutInitialCompile();
	void Shutdown();
	FInterfaceMethodSignature* RegisterInterfaceMethodSignature(FName FunctionName);
	void ReleaseInterfaceMethodSignature(FInterfaceMethodSignature* Signature);
	TArray<FString> DiscoverScriptRoots(bool bOnlyProjectRoot = false) const;
	TArray<FAngelscriptSourceRoot> DiscoverScriptRootDescriptors(bool bOnlyProjectRoot = false) const;
	TArray<FAngelscriptSourceRoot> GetEffectiveScriptRootDescriptors() const;

	/** Discard a named script module from the engine. Returns true if the module was found and discarded. */
	bool DiscardModule(const TCHAR* ModuleName);

	/* Initially bind all engine types to angelscript. */
	void BindScriptTypes();

	/* Initially compile all script files in global folders. */
	void InitialCompile();

	/* Add the listed set of modules into the angelscript engine. 
	 * Modules should already be pre-processed.
	 * Modules array should already be sorted in dependency order. */
	ECompileResult CompileModules(ECompileType CompileType, const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules, TArray<TSharedRef<struct FAngelscriptModuleDesc>>& OutCompiledModules);
	void CompileModule_Types_Stage1(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module, const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& ImportedModules);
	void CompileModule_Functions_Stage2(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module);
	void CompileModule_Code_Stage3(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module);
	void CompileModule_Globals_Stage4(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module);

	/* Perform a hot reload of the specified type if necessary. */
	void CheckForHotReload(ECompileType CompileType);

	/* Verify Unreal Property specifiers in a module. */
	bool VerifyPropertySpecifiers(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules);

	bool VerifyRepFunc(FString* FuncDesc, const TSharedRef<struct FAngelscriptPropertyDesc>& Property,
		const TSharedRef<struct FAngelscriptClassDesc>& Class,
		const TSharedRef<struct FAngelscriptModuleDesc>& Module);

	bool VerifyBlueprintSetFunc(FString* FuncDesc, const TSharedRef<struct FAngelscriptPropertyDesc>& Property,
		const TSharedRef<struct FAngelscriptClassDesc>& Class, const TSharedRef<struct FAngelscriptModuleDesc>& Module);

	bool VerifyBlueprintGetFunc(FString* FuncDesc, const TSharedRef<struct FAngelscriptPropertyDesc>& Property,
		const TSharedRef<struct FAngelscriptClassDesc>& Class, const TSharedRef<struct FAngelscriptModuleDesc>& Module);


	void Tick(float DeltaTime);
	bool ShouldTick() const;

	/* Functions can have user data specified at bind-time that can be looked up here. */
	template<typename T>
	static T* GetCurrentFunctionUserData()
	{
		return (T*)GetCurrentFunctionUserDataPtr();
	}

	static void* GetCurrentFunctionUserDataPtr();
	static asITypeInfo* GetCurrentFunctionObjectType();
	static class asCContext* GetCurrentScriptContext();
	static class asCContext* GetPreviousScriptContext();

	/* Outdated flag marking for modules and functions. */
	bool IsOutdated(asIScriptFunction* Function);

	/* Converting from angelscript-visible pointers to uobject pointers. */
	static FORCEINLINE UObject* AngelscriptToUObject(asIScriptObject* Object)
	{
		return (UObject*)Object;
	}

	static FORCEINLINE asIScriptObject* UObjectToAngelscript(UObject* Object)
	{
		return (asIScriptObject*)Object;
	}

	static bool CanCastScriptObjectToUnrealInterface(asITypeInfo* RuntimeType, asITypeInfo* TargetType, void* ObjectPtr);

	static FORCEINLINE bool CanUseGameThreadData()
	{
		if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
		{
			return IsInGameThread() || !CurrentEngine->bIsInitialCompileFinished;
		}

		return IsInGameThread();
	}

	/* Throw an exception to the angelscript VM. */
	static void Throw(const ANSICHAR* Exception);

	/* Show an error with a script stack trace, but continue the script flow without throwing an exception. */
	static void TraceError(const ANSICHAR* Error);

	/* Get string representations of all levels of the current angelscript callstack. */
	static TArray<FString> GetAngelscriptCallstack();

	/* Get a string representation of the current angelscript callstack. */
	static FString FormatAngelscriptCallstack();

	/* Get a string representation of the current location of angelscript execution (ie the top most stack frame's position). */
	static FString GetAngelscriptExecutionPosition();

	/* Get the file and line number of the current location of angelscript execution (ie the top most stack frame's position). */
	static void GetAngelscriptExecutionFileAndLine(FString& OutFilename, int& OutLineNumber);

	/* Get the UObject that the current angelscript callstack is operating on. */
	static UObject* GetAngelscriptExecutionThisObject(int32 StackFrame = 0);

	/* If the angelscript debugger is attached, do an angelscript breakpoint. Returns whether we broke in AS debugging. */
	static bool TryBreakpointAngelscriptDebugging(const TCHAR* Message = nullptr);
	UObject* GetCurrentWorldContextObject() const { return WorldContextObject; }
	bool ShouldUseEditorScripts() const { return bUseEditorScripts; }
	bool ShouldUseAutomaticImportMethod() const { return bUseAutomaticImportMethod; }

	/* Checks if the character is a valid alphanumeric character or an underscore. */
	FORCEINLINE static bool IsValidIdentifierCharacter(TCHAR Character)
	{
		return (Character >= 'A' && Character <= 'Z')
				|| (Character >= 'a' && Character <= 'z')
				|| (Character >= '0' && Character <= '9')
				|| Character == '_';
	}

	/* The root angelscript UPackage everything should belong to. */
	UPROPERTY()
	UPackage* AngelscriptPackage = nullptr;
	/* The package that all literal assets are put into. */
	UPROPERTY()
	UPackage* AssetsPackage = nullptr;

	/* Root paths where all scripts are loaded from. */
	TArray<FString> AllRootPaths;
	TArray<FAngelscriptSourceRoot> AllScriptRoots;

	/* Internal script data. */
	class asCScriptEngine* Engine = nullptr;

	asIScriptEngine* GetScriptEngine() const
	{
		return (asIScriptEngine*)Engine;
	}

	UPackage* GetPackageInstance() const
	{
		return AngelscriptPackage;
	}

	TSharedPtr<struct FAngelscriptModuleDesc> GetModule(const FString& ModuleName)
	{
		auto* ModRef = ActiveModules.Find(MakeModuleName(ModuleName));
		if (ModRef == nullptr)
			return nullptr;
		else
			return *ModRef;
	}

	TArray<TSharedRef<struct FAngelscriptModuleDesc>> GetActiveModules() const
	{
		TArray<TSharedRef<struct FAngelscriptModuleDesc>> Result;
		for (auto It : ActiveModules)
		{
			Result.Add(It.Value);
		}
		return Result;
	}

	TSharedPtr<struct FAngelscriptModuleDesc> GetModule(asIScriptModule* Module);
	TSharedPtr<struct FAngelscriptModuleDesc> GetModuleByFilename(const FString& Filename);

	TSharedPtr<struct FAngelscriptClassDesc> GetClass(const FString& ClassName, TSharedPtr<struct FAngelscriptModuleDesc>* FoundInModule = nullptr);
	TSharedPtr<struct FAngelscriptEnumDesc> GetEnum(const FString& EnumName, TSharedPtr<struct FAngelscriptModuleDesc>* FoundInModule = nullptr);
	TSharedPtr<struct FAngelscriptDelegateDesc> GetDelegate(const FString& DelegateName, TSharedPtr<struct FAngelscriptModuleDesc>* FoundInModule = nullptr);

	TSharedPtr<struct FAngelscriptModuleDesc> GetModuleByModuleName(const FString& ModuleName);

	TSharedPtr<struct FAngelscriptModuleDesc> GetModuleByFilenameOrModuleName(const FString& Filename, const FString& ModuleName);

	// Captured diagnostic messages during compilation
	struct FDiagnostic
	{
		FString Message;
		int32 Row;
		int32 Column;
		bool bIsError;
		bool bIsInfo;
	};

	void ScriptCompileError(const FString& AbsoluteFilename, const FDiagnostic& Diagnostic);
	void ScriptCompileError(TSharedPtr<FAngelscriptModuleDesc> Module, int32 LineNumber, const FString& Message, bool bIsError = true);
	void ScriptCompileError(UClass* InsideClass, const FString& FunctionName, const FString& Message, bool bIsError = true);

	UStruct* GetUnrealStructFromAngelscriptTypeId(int TypeId);

	// Can be filled by the game module to provide additional compile checks in editor
	// depending on what code class is being compiled.
	TMap<UClass*, TSharedPtr<FAngelscriptAdditionalCompileChecks>> AdditionalCompileChecks;

	struct FFilenamePair
	{
		FString AbsolutePath;
		FString RelativePath;
		FString VirtualPath;
	};

private:
	FString MakeModuleName(const FString& ModuleName) const;
	bool ShouldInitializeThreaded();
	TSet<FName> CollectDisabledBindNames() const;
	void AcquireProcessPackages();
	void ReleaseProcessPackages();
	#if WITH_DEV_AUTOMATION_TESTS
	#endif
	void PreInitialize_GameThread();
	void Initialize_AnyThread();
	void PostInitialize_GameThread();

	void SetOutdated(asIScriptModule* OldModule);

	/* Internal state of active modules. */
	TMap<FString, TSharedRef<struct FAngelscriptModuleDesc>> ActiveModules;
	TMap<asIScriptModule*, TSharedPtr<struct FAngelscriptModuleDesc>> ModulesByScriptModule;

#if AS_CAN_HOTRELOAD
	TMap<FStringView, TPair<TSharedPtr<struct FAngelscriptModuleDesc>, TSharedPtr<struct FAngelscriptClassDesc>>> ActiveClassesByName;
	TMap<FStringView, TPair<TSharedPtr<struct FAngelscriptModuleDesc>, TSharedPtr<struct FAngelscriptEnumDesc>>> ActiveEnumsByName;
	TMap<FStringView, TPair<TSharedPtr<struct FAngelscriptModuleDesc>, TSharedPtr<struct FAngelscriptDelegateDesc>>> ActiveDelegatesByName;
#endif

	/* Global context pool of contexts that don't belong to a thread right now. */
	friend struct FAngelscriptPooledContextBase;
	friend struct FAngelscriptContextPool;
	TArray<asCContext*> GlobalContextPool;
	FCriticalSection GlobalContextPoolLock;

	/* Hot reload watch state, maps script files to required data for detecting reloads. */
	struct FHotReloadState
	{
		FDateTime LastChange;
		uint64 ContentHash = 0;
		bool bHasContentHash = false;
	};

	bool bUseHotReloadCheckerThread = false;

	TMap<FString, FHotReloadState> FileHotReloadState;

	volatile bool bWaitingForHotReloadResults = false;

	/* Prepares and holds the execution status of unit tests on hot reload. */
	FHotReloadTestRunner* HotReloadTestRunner = nullptr;

	/* Files that we tried to reload before, but failed to compile, that we should retry later. */
	TSet<FFilenamePair> PreviouslyFailedReloadFiles;

	/* Files that we soft reloaded but that we need to do a full reload on once we are able. */
	TSet<FFilenamePair> QueuedFullReloadFiles;
	TArray<TUniquePtr<FInterfaceMethodSignature>> InterfaceMethodSignatures;

	double NextHotReloadCheck = -1.0;

	void DiscoverTests();
	bool PerformHotReload(ECompileType CompileType, const TArray<FFilenamePair>& FileList);
	void CheckForFileChanges();
	FString MakeSourceStateKey(const FFilenamePair& Filename) const;

	void ImportIntoModule(class asIScriptModule* IntoModule, class asIScriptModule* FromModule);

	bool CheckFunctionImportsForNewModules(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules);
	void UpdateScriptReferencesInUnrealData(struct asModuleReferenceUpdateMap& UpdateMap, TSharedRef<FAngelscriptModuleDesc> Module);

	void ResolveAllDeclaredImports();
	void ResolveDeclaredImports(class asIScriptModule* Module);

#if WITH_EDITOR
	void CheckUsageRestrictions(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules);
#endif

	void SwapInModules(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules, TArray<TSharedRef<struct FAngelscriptModuleDesc>>& DiscardedModules);

#if !UE_BUILD_SHIPPING
	void GetOnScreenMessages(TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText>& OutMessages);
#endif

	// Counter for temporary generated module names
	int32 TempNameIndex = 0;

	// Owned engine state. The engine is the sole owner; each TUniquePtr is
	// empty before Initialize*() runs (Get() returns nullptr) and is
	// MakeUnique-d during initialization. Teardown releases these in
	// Shutdown().
	TUniquePtr<FAngelscriptTypeDatabase> TypeDatabase;
	TUniquePtr<FAngelscriptBindState> BindState;
	TUniquePtr<TArray<FToStringType>> ToStringList;
	TUniquePtr<FAngelscriptBindDatabase> BindDatabase;
	TUniquePtr<FBlueprintEventSignatureRegistry> BlueprintEventSignatureRegistry;
	TArray<FName> StaticNames;
	TMap<FName, int32> StaticNamesByIndex;

	// Per-engine lookup from script-defined UEnum name to its bound asITypeInfo*.
	// Populated during Bind_Enums, consumed by property-type resolution paths in
	// Bind_UEnum. Engine-owned so that two engines bound concurrently never see
	// each other's `asITypeInfo*` for same-named script enums.
	TMap<FName, class asITypeInfo*> ScriptEnumTypeLookupByName;

	TSharedPtr<FAngelscriptEngineLifetimeToken> LifetimeToken;
	bool bHoldsProcessPackageReference = false;
	UPROPERTY()
	UObject* WorldContextObject = nullptr;

	friend class UAngelscriptTestCommandlet;
	friend class UAngelscriptGameInstanceSubsystem;

	static FAngelscriptEngine* TryGetGlobalEngine();
	static void SetGlobalEngine(FAngelscriptEngine* InEngine);
	static FAngelscriptEngine& GetOrCreate();
	static bool DestroyGlobal();
	friend class FAngelscriptRuntimeModule;
	friend struct FAngelscriptBindConfigTestAccess;
	friend struct FAngelscriptDependencyInjectionTestAccess;
	friend struct FAngelscriptEngineIsolationTestAccess;
	friend struct FAngelscriptMultiEngineTestAccess;
	friend struct FAngelscriptSubsystemOwnershipTestAccess;
	friend struct FAngelscriptTickBehaviorTestAccess;
	friend struct FAngelscriptHotReloadTestAccess;
	friend struct FAngelscriptEngineScope;
	friend struct FAngelscriptTestEngineScopeAccess;
	friend struct FStaticJITDiagnostics;
	friend struct FAngelscriptStateSnapshotBuilder;
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FAngelscriptInterfaceSignatureTestAccess;
#endif

public:
	TArray<FFilenamePair> FileChangesDetectedForReload;
	TArray<FFilenamePair> FileDeletionsDetectedForReload;
	double LastFileChangeDetectedTime = -1.0;

	bool bDidInitialCompileSucceed = true;
	bool bIsInitialCompileFinished = false;
	
	bool bCompletedAssetScan = false;

	bool IsInitialCompileFinished() const
	{
		return bIsInitialCompileFinished;
	}

	struct FAngelscriptDebugFrame
	{
		const char* File = nullptr;
		const char* Function = nullptr;
		const char* Class = nullptr;
		int32 LineNumber = -1;
		UObject* This = nullptr;
		struct FDebugValues* Variables = nullptr;
		struct FDebugValuePrototype* Prototype = nullptr;
		asIScriptFunction* ScriptFunction = nullptr;

		~FAngelscriptDebugFrame();
	};

	struct FAngelscriptDebugStack
	{
		TArray<FAngelscriptDebugFrame> Frames;
	};

	struct FDiagnostics
	{
		FString Filename;
		TArray<FDiagnostic> Diagnostics;
		bool bHasEmittedAny = false;
		bool bIsCompiling = false;
	};

#if WITH_AS_DEBUGSERVER
	class FAngelscriptDebugServer* DebugServer = nullptr;
	bool IsEvaluatingDebuggerWatch();
#endif

	FCriticalSection CompilationLock;

	TMap<FString, FDiagnostics> Diagnostics;
	TMap<FString, FDiagnostics> LastEmittedDiagnostics;

	bool bDiagnosticsDirty = false;
	bool bIgnoreCompileErrorDiagnostics = false;

	FString FormatDiagnostics();
	void ResetDiagnostics();
	void EmitDiagnostics(class FSocket* Client = nullptr);
	void EmitDiagnostics(FDiagnostics& Diag, class FSocket* Client = nullptr);

	void FindAllScriptFilenames(TArray<FFilenamePair>& OutFilenames);
	void FindAllScriptSources(TArray<FAngelscriptSource>& OutSources);

	bool HasAnyDebugServerClients();
	void ReplaceScriptAssetContent(FString AssetName, TArray<FString> AssetContent);

	friend struct FAngelscriptPrecompiledData;
	struct FAngelscriptPrecompiledData* PrecompiledData = nullptr;
	struct FAngelscriptStaticJIT* StaticJIT = nullptr;
	bool bUsePrecompiledData = false;
	bool bUsedPrecompiledDataForPreprocessor = false;
	bool bScriptDevelopmentMode = false;

	static bool IsGeneratingPrecompiledData();

	// Argument type specializations that were bound by Bind_BlueprintEvent.cpp,
	// the preprocessor uses this list to look up what push argument function to call
	TSet<FString> BoundBlueprintEventArgumentSpecializations;

	void StartHotReloadThread();
	bool bHotReloadThreadStarted = false;

	UPROPERTY()
	class UAngelscriptSettings* ConfigSettings = nullptr;

	static void HandleExceptionFromJIT(const ANSICHAR* ExceptionString);
	asCContext* CreateContext();
	void UpdateLineCallbackState();

	FAngelscriptTypeDatabase* GetTypeDatabase() const;
	FAngelscriptBindState* GetBindState() const;
	TArray<FToStringType>* GetToStringList() const;
	FAngelscriptBindDatabase* GetBindDatabase() const;
	FBlueprintEventSignatureRegistry* GetBlueprintEventSignatureRegistry() const;

	TMap<FName, class asITypeInfo*>& GetScriptEnumTypeLookup() { return ScriptEnumTypeLookupByName; }
	const TMap<FName, class asITypeInfo*>& GetScriptEnumTypeLookup() const { return ScriptEnumTypeLookupByName; }

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetToStringEntryCountForTesting() const;
	FAngelscriptBindDatabase& GetBindDatabaseForTesting() const;
	static int32 GetLocalPooledContextCountForTesting(asIScriptEngine* ScriptEngine);
	void SetUseEditorScriptsForTesting(bool bEnabled);
	void SetAutomaticImportMethodForTesting(bool bEnabled);
	void SetBlueprintLibraryNamespaceSettingsForTesting(bool bUseScriptName, TArray<FString> PrefixesToStrip, TArray<FString> SuffixesToStrip);
#endif

	static const FName& GetStaticName(int32 Index);
	static bool TryGetStaticName(int32 Index, FName& OutName);
	static int32 GetOrAddStaticName(FName Name);
	static int32 GetStaticNameCount();
	static void ReserveStaticNames(int32 Count);
	static void ResetStaticNames();
	static void AddStaticNameFromPrecompiled(FName Name);
	static const TArray<FName>& GetStaticNames();

	static void AssignWorldContext(UObject* NewWorldContext);

	static void FindScriptFiles(
		IFileManager& FileManager,
		const FString& RelativeRoot,
		const FString& SearchDirectory,
		const TCHAR* Pattern,
		TArray<FFilenamePair>& OutFilenames,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts);

	static TArray<FString> MakeAllScriptRoots(bool bOnlyProjectRoot = false);

	const FAngelscriptEngineConfig& GetRuntimeConfig() const
	{
		return RuntimeConfig;
	}

private:
	FAngelscriptEngineConfig RuntimeConfig;
	FAngelscriptEngineDependencies Dependencies;

	// Hook delegate fields. Field order/grouping kept identical to the
	// previous hooks container for git-blame and diff
	// readability. Categories: runtime helpers, compile lifecycle, class
	// generation, asset lifecycle, reload lifecycle.
	FAngelscriptGetDynamicSpawnLevel DynamicSpawnLevel;
	FAngelscriptDebugCheckBreakOptions DebugCheckBreakOptions;
	FAngelscriptGetDebugBreakFilters DebugBreakFilters;
	FAngelscriptDebugObjectSuffix DebugObjectSuffix;
	FAngelscriptComponentCreated ComponentCreated;
	FAngelscriptCompilationDelegate PreCompile;
	FAngelscriptCompilationDelegate PostCompile;
	FAngelscriptCompilationDelegate OnInitialCompileFinished;
	FAngelscriptClassAnalyzeDelegate ClassAnalyze;
	FAngelscriptPreGenerateClasses PreGenerateClasses;
	FAngelscriptPostCompileClassCollection PostCompileClassCollection;
	FAngelscriptLiteralAssetCreated OnLiteralAssetCreated;
	FAngelscriptLiteralAssetCreated PostLiteralAssetSetup;

	FOnAngelscriptClassReload OnClassReload;
	FOnAngelscriptEnumCreated OnEnumCreated;
	FOnAngelscriptEnumChanged OnEnumChanged;
	FOnAngelscriptStructReload OnStructReload;
	FOnAngelscriptDelegateReload OnDelegateReload;
	FOnAngelscriptFullReload OnFullReload;
	FOnAngelscriptPostReload OnPostReload;
	FOnAngelscriptLiteralAssetReload OnLiteralAssetReload;
};

template<>
struct TStructOpsTypeTraits<FAngelscriptEngine> : public TStructOpsTypeTraitsBase2<FAngelscriptEngine>
{
	enum
	{
		WithCopy = false,
	};
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineContextStack
{
	static void Push(FAngelscriptEngine* Engine);
	static void Pop(FAngelscriptEngine* Engine);
	static FAngelscriptEngine* Peek();
	static bool IsEmpty();

#if WITH_DEV_AUTOMATION_TESTS
	static TArray<FAngelscriptEngine*> SnapshotAndClear();
	static void RestoreSnapshot(TArray<FAngelscriptEngine*>&& SavedStack);
#endif
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineScope
{
	explicit FAngelscriptEngineScope(FAngelscriptEngine& InEngine, UObject* InWorldContext = nullptr);
	~FAngelscriptEngineScope();

	FAngelscriptEngineScope(const FAngelscriptEngineScope&) = delete;
	FAngelscriptEngineScope& operator=(const FAngelscriptEngineScope&) = delete;
	FAngelscriptEngineScope(FAngelscriptEngineScope&& Other) noexcept;
	FAngelscriptEngineScope& operator=(FAngelscriptEngineScope&& Other) noexcept;

private:
	void Reset();

	FAngelscriptEngine* Engine = nullptr;
	UObject* PreviousWorldContext = nullptr;
	UObject* PreviousEngineWorldContext = nullptr;
	bool bChangedWorldContext = false;
};

struct FAngelscriptContextPool
{
	TArray<asCContext*> FreeContexts;

	~FAngelscriptContextPool();
};

extern thread_local FAngelscriptContextPool GAngelscriptContextPool;
extern FAngelscriptEngine::FAngelscriptDebugStack* GAngelscriptStack;

/* Automatically retrieves and manages an angelscript context for the scope of this struct. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptPooledContextBase
{
	FAngelscriptPooledContextBase();
	explicit FAngelscriptPooledContextBase(class asIScriptEngine* DesiredScriptEngine);

	FORCEINLINE FAngelscriptPooledContextBase(class asCThreadLocalData* tld)
	{
		Init(tld, nullptr);
	}

	FORCEINLINE FAngelscriptPooledContextBase(class asCThreadLocalData* tld, class asIScriptEngine* DesiredScriptEngine)
	{
		Init(tld, DesiredScriptEngine);
	}

	~FAngelscriptPooledContextBase();

	void Init(class asCThreadLocalData* tld, class asIScriptEngine* DesiredScriptEngine);

	FAngelscriptPooledContextBase(FAngelscriptPooledContextBase&& Other);

	FAngelscriptPooledContextBase(FAngelscriptPooledContextBase& Other) = delete;
	void operator=(FAngelscriptPooledContextBase& Other) = delete;

	bool operator==(asCContext* Ptr) const { return Context == Ptr; }
	bool operator!=(asCContext* Ptr) const { return Context != Ptr; }

	void PrepareExternal(class asIScriptFunction* Function);
	void ExecuteExternal();

	FORCEINLINE asCContext* operator->() const
	{
		return Context;
	}

	operator asCContext*() const
	{
		return Context;
	}

private:
	asCContext* Context;
	bool bWasNested;
};

struct FAngelscriptGameThreadScopeWorldContext
{
	FAngelscriptGameThreadScopeWorldContext(UObject* WorldContext)
	{
		PreviousWorldContext = FAngelscriptEngine::GetAmbientWorldContext();
		FAngelscriptEngine::AssignWorldContext(WorldContext);
	}

	~FAngelscriptGameThreadScopeWorldContext()
	{
		FAngelscriptEngine::AssignWorldContext(PreviousWorldContext);
	}
private:
	UObject* PreviousWorldContext;
};

struct FAngelscriptContext : public FAngelscriptPooledContextBase
{
	FAngelscriptContext()
	{
		bChangedWorldContext = false;
	}

	explicit FAngelscriptContext(class asIScriptEngine* DesiredScriptEngine)
		: FAngelscriptPooledContextBase(DesiredScriptEngine)
	{
		bChangedWorldContext = false;
	}

	FAngelscriptContext(UObject* WorldContext)
		: FAngelscriptContext(WorldContext, nullptr)
	{
	}

	FAngelscriptContext(UObject* WorldContext, class asIScriptEngine* DesiredScriptEngine = nullptr);

	~FAngelscriptContext()
	{
		if (bChangedWorldContext)
		{
			FAngelscriptEngine::AssignWorldContext(PreviousWorldContext);
		}
	}
private:
	UObject* PreviousWorldContext;
	bool bChangedWorldContext;
};

struct FAngelscriptGameThreadContext : public FAngelscriptPooledContextBase
{
	FAngelscriptGameThreadContext(UObject* WorldContext, class asIScriptEngine* DesiredScriptEngine = nullptr);

	~FAngelscriptGameThreadContext()
	{
		FAngelscriptEngine::AssignWorldContext(PreviousWorldContext);
	}
private:
	UObject* PreviousWorldContext;
};

/**
 * Anything running within this scope is excluded from the angelscript loop detection timeout in editor.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptExcludeScopeFromLoopTimeout
{
#if WITH_EDITOR
	class asCContext* Context;
	double StartTime;

	FAngelscriptExcludeScopeFromLoopTimeout();
	~FAngelscriptExcludeScopeFromLoopTimeout();
#else
	FORCEINLINE FAngelscriptExcludeScopeFromLoopTimeout() {}
	FORCEINLINE ~FAngelscriptExcludeScopeFromLoopTimeout() {}
#endif
};

/**
 * Description of a script property that has been added as an unreal property.
 */
struct FAngelscriptPropertyDesc
{
	/* Name of the property. */
	FString PropertyName;

	/* Literal type in script of the property. */
	FString LiteralType;

	/* Resolved type of the property. */
	FAngelscriptTypeUsage PropertyType;

	/* Metadata for the property. */
	TMap<FName, FString> Meta;

	/* Whether the property can be read in blueprint. */
	bool bBlueprintReadable = false;

	/* Whether the property can be written in blueprint. */
	bool bBlueprintWritable = false;

	/* Whether the property can be edited on defaults. */
	bool bEditableOnDefaults = false;

	/* Whether the property can be edited on instances. */
	bool bEditableOnInstance = false;

	/* Whether the property is shown in details views but cannot be changed. */
	bool bEditConst = false;

	/* Whether the property is considered a component reference. */
	bool bInstancedReference = false;

	/* Whether the property is considered a persistent reference, an object referenced by the property is duplicated like a component. */
	bool bPersistentInstance = false;

	/* Whether the property should be marked as advanced display. */
	bool bAdvancedDisplay = false;

	/* Whether the property is transient. */
	bool bTransient = false;

	/* Whether an FProperty exists in the class for this property. */
	bool bHasUnrealProperty = false;

	/* Whether the property should be replicated. */
	bool bReplicated = false;

	/* Whether the property should be skipped when replicating. */
	bool bSkipReplication = false;

	/* Whether to skip during serialization. */
	bool bSkipSerialization = false;

	/* Whether property should be serialized for save games. */
	bool bSaveGame = false;

	/* Specified replication condition. */
	TEnumAsByte<ELifetimeCondition> ReplicationCondition = COND_None;

	/* Whether we should call a function when this is replicated. */
	bool bRepNotify = false;

	/* Whether this is a config property read from ini files. */
	bool bConfig = false;

	/* Whether the property is exposed for Matinee or Sequencer to modify. */
	bool bInterp = false;

	/* Whether the property should be searchable in the Asset Registry. */
	bool bAssetRegistrySearchable = false;

	/* Whether the property should not be clearable (disallow being set to None). */
	bool bNoClear = false;

	/* Whether the property is private in angelscript. */
	bool bIsPrivate = false;

	/* Whether the property is protected in angelscript. */
	bool bIsProtected = false;

	/* Angelscript internal data for the property. */
	int32 ScriptPropertyIndex = -1;
	SIZE_T ScriptPropertyOffset = 0;

	/* Line number in the file of the property. */
	int32 LineNumber = 1;

	bool IsDefinitionEquivalent(const FAngelscriptPropertyDesc& Other) const
	{
		return Other.bBlueprintReadable == bBlueprintReadable
			&& Other.bBlueprintWritable == bBlueprintWritable
			&& Other.bEditableOnDefaults == bEditableOnDefaults
			&& Other.bEditableOnInstance == bEditableOnInstance
			&& Other.bAdvancedDisplay == bAdvancedDisplay
			&& Other.bEditConst == bEditConst
			&& Other.bInstancedReference == bInstancedReference
			&& Other.bPersistentInstance == bPersistentInstance
			&& Other.bTransient == bTransient
			&& Other.bConfig == bConfig
			&& Other.bInterp == bInterp
			&& Other.bAssetRegistrySearchable == bAssetRegistrySearchable
			&& Other.bNoClear == bNoClear
			&& Other.bReplicated == bReplicated
			&& Other.ReplicationCondition == ReplicationCondition
			&& Other.bSkipReplication == bSkipReplication
			&& Other.bSkipSerialization == bSkipSerialization
			&& Other.bSaveGame == bSaveGame
			&& Other.bRepNotify == bRepNotify
			&& Other.bIsPrivate == bIsPrivate
			&& Other.bIsProtected == bIsProtected
			;
	}
};

/**
 * Description of an argument to an angelscript function.
 */
struct FAngelscriptArgumentDesc
{
	/* Name of the argument. */
	FString ArgumentName;

	/* Stringified default value of the argument. */
	FString DefaultValue;

	/* Angelscript type of the argument. */
	FAngelscriptTypeUsage Type;

	/* What kind of blueprint parameter to generate for this argument. */
	bool bBlueprintByValue = false;
	bool bBlueprintOutRef = false;
	bool bBlueprintInRef = false;

	/* If set, even an InRef parameter will have its value copied back. */
	bool bInRefForceCopyOut = false;

	bool IsDefinitionEquivalent(const FAngelscriptArgumentDesc& Other) const
	{
		return Other.bBlueprintByValue == bBlueprintByValue
			&& Other.bBlueprintOutRef == bBlueprintOutRef
			&& Other.bBlueprintInRef == bBlueprintInRef
			&& Other.bInRefForceCopyOut == bInRefForceCopyOut
			&& Other.Type == Type
			;
	}
};

/**
 * Description of a script function that should be bound as an unreal function.
 */
struct FAngelscriptFunctionDesc
{
	/* Name of the function in unreal. */
	FString FunctionName;

	/* Original name of the function as it was declared if FunctionName has been changed, empty otherwise. */
	FString OriginalFunctionName;

	/* Name of the angelscript function to bind. */
	FString ScriptFunctionName;

	/* Metadata for the function. */
	TMap<FName, FString> Meta;

	/* Return type of the function. */
	FAngelscriptTypeUsage ReturnType;

	/* Types of the arguments to the function. */
	TArray<FAngelscriptArgumentDesc> Arguments;

	/* Whether the function should be marked as blueprint callable. */
	bool bBlueprintCallable = false;

	/* Whether this function is an override for a Blueprint{Implementable,Native}Event. */
	bool bBlueprintOverride = false;

	/* Whether this function can be overridden by blueprint as an event. */
	bool bBlueprintEvent = false;

	/* Whether this function should be marked as pure in blueprint. */
	bool bBlueprintPure = false;

	/* Whether this function is a NetMulticast. */
	bool bNetMulticast = false;

	/* Whether this function is a Client net function. */
	bool bNetClient = false;

	/* Whether this function is a Server net function. */
	bool bNetServer = false;

	/* Whether this function should have a _Validate function. */
	bool bNetValidate = false;

	/* Whether to send as unreliable if a netfunction. */
	bool bUnreliable = false;

	/* Whether to tag the function as blueprint authority only. */
	bool bBlueprintAuthorityOnly = false;

	/* Whether the function is marked as Exec in angelscript. */
	bool bExec = false;

	/* Whether the blueprint event can be overridden or not. */
	bool bCanOverrideEvent = true;

	/* Whether this function is a static global function. */
	bool bIsStatic = false;

	/* Whether this is a const method in angelscript. */
	bool bIsConstMethod = false;

	/* Internal angelscript function this is referencing. */
	class asIScriptFunction* ScriptFunction = nullptr;

	/* Whether this function can be called from other threads in unreal. */
	bool bThreadSafe = false;

	/* Generated UFunction for this script function. */
	UFunction* Function = nullptr;

	/* Whether this function has a completely empty body. */
	bool bIsNoOp = false;

	/* Whether the function is private in angelscript. */
	bool bIsPrivate = false;

	/* Whether the function is protected in angelscript. */
	bool bIsProtected = false;

	/* Line number in the file of the function. */
	int32 LineNumber = 1;

	bool SignatureMatches(TSharedPtr<FAngelscriptFunctionDesc> OtherFunction, bool bCheckNames = false) const;
	bool ParametersMatches(TSharedPtr<FAngelscriptFunctionDesc> OtherFunction, bool bCheckNames = false) const;

	bool IsDefinitionEquivalent(const FAngelscriptFunctionDesc& Other) const
	{
		return Other.bBlueprintCallable == bBlueprintCallable
			&& Other.bBlueprintOverride == bBlueprintOverride
			&& Other.bBlueprintEvent == bBlueprintEvent
			&& Other.bBlueprintPure == bBlueprintPure
			&& Other.bUnreliable == bUnreliable
			&& Other.bNetMulticast == bNetMulticast
			&& Other.bNetClient == bNetClient
			&& Other.bNetServer == bNetServer
			&& Other.bBlueprintAuthorityOnly == bBlueprintAuthorityOnly
			&& Other.bExec == bExec
			&& Other.bCanOverrideEvent == bCanOverrideEvent
			&& Other.bIsStatic == bIsStatic
			&& Other.bIsConstMethod == bIsConstMethod
			&& Other.bThreadSafe == bThreadSafe
			&& Other.bIsPrivate == bIsPrivate
			&& Other.bIsProtected == bIsProtected
			;
	}
};

/**
 * Description of a script class during preprocessing.
 */
struct FAngelscriptClassDesc
{
	/* Name of the class that was compiled. */
	FString ClassName;

	/* Angelscript name of the class that should be the super for this angelscript class. */
	FString SuperClass;

	/* Actual UClass of the native class that backs this script type. */
	UClass* CodeSuperClass = nullptr;

	/* Whether the direct superclass of this is a code class. If false, it is an angelscript class. */
	bool bSuperIsCodeClass = false;

	/* Whether this is a generated statics class that does not actually exist in script. */
	bool bIsStaticsClass = false;

	/* Whether this class is abstract. */
	bool bAbstract = false;

	/* Whether all instances of this class should be transient. */
	bool bTransient = false;

	/* Whether this class is hidden from property combo boxes */
	bool bHideDropdown = false;

	/* Indicates that references to this class default to instanced. Used to be subclasses of UComponent, but now can be any UObject */
	bool bDefaultToInstanced = false;

	/* Class can be constructed from EditInlineNew New button. */
	bool bEditInlineNew = false;

	/* Whether this class is deprecated. */
	bool bIsDeprecatedClass = false;

	/* Whether the class can be placed in levels. */
	bool bPlaceable = true;

	/* Whether this class represents a struct or not. */
	bool bIsStruct = false;

	/* List of interface class names this class implements. */
	TArray<FString> ImplementedInterfaces;

	/* Name of the config file to use. */
	FString ConfigName;

	/* Internal angelscript class this is referencing. */
	asITypeInfo* ScriptType = nullptr;

	/* Generated UClass that this class should be instanced as. */
	UClass* Class = nullptr;

	/* Generated UStruct that this class should be instanced as. */
	UStruct* Struct = nullptr;

	/* Properties we're adding into unreal for this class. */
	TArray<TSharedRef<FAngelscriptPropertyDesc>> Properties;

	/* Functions we're adding into unreal for this class. */
	TArray<TSharedRef<FAngelscriptFunctionDesc>> Methods;

	/* The name of the global variable that should be set in script to this UClass. */
	FString StaticClassGlobalVariableName;

	/* The code used to set default properties for this class. */
	FString DefaultsCode;

	/* Line number in the file of the class. */
	int32 LineNumber = 1;

	/* Metadata for the class. */
	TMap<FName, FString> Meta;

	/* Composable class */
	FString ComposeOntoClass;

	/* This will be set when the class resides in a namespace that is NOT the modules default namespace. */
	TOptional<FString> Namespace;

	// Find the property descriptor by name
	TSharedPtr<FAngelscriptPropertyDesc> GetProperty(const FString& PropName)
	{
		for (auto PropDesc : Properties)
		{
			if (PropName.Equals(PropDesc->PropertyName))
			{
				return PropDesc;
			}
		}
		return nullptr;
	}

	TSharedPtr<FAngelscriptPropertyDesc> GetProperty(class asCString& PropName);

	// Find the function descriptor by name
	TSharedPtr<FAngelscriptFunctionDesc> GetMethod(const FString& FuncName)
	{
		for (auto FuncDesc : Methods)
		{
			if (FuncName.Equals(FuncDesc->FunctionName))
			{
				return FuncDesc;
			}
		}
		return nullptr;
	}

	TSharedPtr<FAngelscriptFunctionDesc> GetMethodByScriptName(const FString& FuncName)
	{
		for (auto FuncDesc : Methods)
		{
			if (FuncName.Equals(FuncDesc->ScriptFunctionName))
			{
				return FuncDesc;
			}
		}
		return nullptr;
	}

	bool AreFlagsEqual(const FAngelscriptClassDesc& Other) const
	{
		return bAbstract == Other.bAbstract
			&& bTransient == Other.bTransient
			&& bHideDropdown == Other.bHideDropdown
			&& bDefaultToInstanced == Other.bDefaultToInstanced
			&& bEditInlineNew == Other.bEditInlineNew
			&& bIsDeprecatedClass == Other.bIsDeprecatedClass
			&& bPlaceable == Other.bPlaceable
			&& ImplementedInterfaces == Other.ImplementedInterfaces
		;
	}
};

/**
 * Description of a script enum during preprocessing.
 */
struct FAngelscriptEnumDesc
{
	/* Name of the enum that was compiled. */
	FString EnumName;
	FString Documentation;

	/* Enum values to add to the enum. */
	TArray<FName> ValueNames;
	TArray<int32> EnumValues;

	/* Actual generated UEnum for this. */
	UEnum* Enum = nullptr;

	/* Internal angelscript type this is referencing. */
	asITypeInfo* ScriptType = nullptr;

	/* Line number in the file of the enum. */
	int32 LineNumber = 1;

	/* Metadata for the enum. */
	TMap<TPair<FName, int32>, FString> Meta;
};

/**
 * Description of a delegate signature declared in angelscript.
 */
struct FAngelscriptDelegateDesc
{
	/* Name of the delegate that was compiled. */
	FString DelegateName;

	/* Whether this is a multicast delegate. */
	bool bIsMulticast = false;

	/* Description of the signature function in angelscript. */
	TSharedPtr<FAngelscriptFunctionDesc> Signature;

	/* Actual generated signature UFunction for this. */
	UDelegateFunction* Function = nullptr;

	/* Internal angelscript type this is referencing. */
	asITypeInfo* ScriptType = nullptr;

	/* Line number in the file of the delegate. */
	int32 LineNumber = 1;
};

// Stores either a :
// - UnitTest_
// - ComplexUnitTest_
// - IntegrationTest_
// - ComplexIntegrationTest_
// 
// UnitTest_ and IntegrationTest_ functions always generate exactly one test case.
// ComplexUnitTest_ and ComplexIntegrationTest_ functions may generate more, in that case you will have multiple
// instances of this struct with different test case names and arguments, but pointing to the same function.
// If bIsComplexTest is true it's a complex test, and in that case the ComplexTestParam should be passed to the function.
struct FAngelscriptTestDesc
{
	asIScriptFunction* Function;
	bool bIsComplexTest;
	FString ComplexTestParam;
};

/**
 * Description of a script module during preprocessing.
 */
struct FAngelscriptModuleDesc
{
	/* Name of the module in angelscript. Usually Dir.SubDir.ModuleName */
	FString ModuleName;

	/* Code sections to add to the module during compilation. Map is filename->processed code. */
	struct FCodeSection
	{
		FString VirtualPath;
		FString RelativeFilename;
		FString AbsoluteFilename;
		FString Code;
		int64 CodeHash;
	};

	TArray<FCodeSection> Code;

	// Hash of the preprocessed code that was added to this module
	int64 CodeHash = 0;
	// Combined hash of all code in the module and other modules that it depends on
	int64 CombinedDependencyHash = 0;

	/* List of classes that will be compiled in this module. */
	TArray<TSharedRef<FAngelscriptClassDesc>> Classes;

	/* List of enums that will be compiled in this module. */
	TArray<TSharedRef<FAngelscriptEnumDesc>> Enums;

	/* List of delegates that will be compiled in this module. */
	TArray<TSharedRef<FAngelscriptDelegateDesc>> Delegates;

	/* Names of modules that should be imported into this module. */
	TArray<FString> ImportedModules;

	/* Names of functions in this module that should be executed after CDOs for classes are created. */
	TArray<FString> PostInitFunctions;

#if WITH_EDITOR
	/* Usage restrictions specified in this file with preprocessor macros. */
	struct FUsageRestriction
	{
		bool bIsAllow;
		FString Pattern;
	};

	TArray<FUsageRestriction> UsageRestrictions;

	// Each editor-only code block has its start and end line recorded here for error checking
	TArray<TPair<int,int>> EditorOnlyBlockLines;
#endif

	/* Internal angelscript data that get set during compilation. */
	class asCModule* ScriptModule = nullptr;
	const struct FAngelscriptPrecompiledModule* PrecompiledData = nullptr;
	bool bCompileError = false;
	bool bLoadedPrecompiledCode = false;
	bool bModuleSwapInError = false;

	/* All unit tests in this module. Maps function name -> function description struct. */
	TMap<FString, FAngelscriptTestDesc> UnitTestFunctions;

	/* All integration tests in this module. Maps function name -> function description struct. */
	TMap<FString, FAngelscriptTestDesc> IntegrationTestFunctions;

	// Find the class descriptor by name in this module
	TSharedPtr<FAngelscriptClassDesc> GetClass(const FString& ClassName)
	{
		for (auto ClassDesc : Classes)
		{
			if (ClassName == ClassDesc->ClassName)
			{
				return ClassDesc;
			}
		}
		return nullptr;
	}

	TSharedPtr<FAngelscriptClassDesc> GetClass(asITypeInfo* Type)
	{
		for (auto ClassDesc : Classes)
		{
			if (ClassDesc->ScriptType == Type)
			{
				return ClassDesc;
			}
		}
		return nullptr;
	}

	// Find the enum descriptor by name in this module
	TSharedPtr<FAngelscriptEnumDesc> GetEnum(const FString& EnumName)
	{
		for (auto EnumDesc : Enums)
		{
			if (EnumName == EnumDesc->EnumName)
			{
				return EnumDesc;
			}
		}
		return nullptr;
	}

	// TestFunctions are all functions in a module which match the signature 
	// void Test_<name>(FUnitTest& t).
	FAngelscriptTestDesc* GetUnitTestFunction(const FString& TestFunctionName)
	{
		FAngelscriptTestDesc* FunctionDesc = UnitTestFunctions.Find(TestFunctionName);
		if (FunctionDesc == nullptr)
		{
			return nullptr;
		}
		return FunctionDesc;
	}

	FAngelscriptTestDesc* GetIntegrationTestFunction(const FString& TestFunctionName)
	{
		FAngelscriptTestDesc* FunctionDesc = IntegrationTestFunctions.Find(TestFunctionName);
		if (FunctionDesc == nullptr)
		{
			return nullptr;
		}
		return FunctionDesc;
	}
};

/* Helper scope struct to print out performance information. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptScopeTimer
{
#if AS_PRINT_STATS
	double StartTime;
	FString Name;
public:
	FAngelscriptScopeTimer(const TCHAR* Name);
	~FAngelscriptScopeTimer();

	static void OutputTime(const TCHAR* Name, double Time);
#else
	FORCEINLINE FAngelscriptScopeTimer(const TCHAR* Name) {}
	FORCEINLINE static void OutputTime(const TCHAR* Name, double Time) {}
#endif
};

struct FAngelscriptScopeTotalTimer
{
#if AS_PRINT_STATS && AS_PRECOMPILED_STATS
	double* Timer;
	double StartTime;
public:
	FAngelscriptScopeTotalTimer(double& TotalTime);
	~FAngelscriptScopeTotalTimer();
#else
	FORCEINLINE FAngelscriptScopeTotalTimer(double& TotalTime) {}
#endif
};

inline uint32 GetTypeHash(const FAngelscriptEngine::FFilenamePair& FilenamePair)
{
	return HashCombine(
		HashCombine(GetTypeHash(FilenamePair.AbsolutePath), GetTypeHash(FilenamePair.RelativePath)),
		GetTypeHash(FilenamePair.VirtualPath));
}

inline bool operator==(const FAngelscriptEngine::FFilenamePair& A, const FAngelscriptEngine::FFilenamePair& B)
{
	return A.AbsolutePath == B.AbsolutePath
		&& A.RelativePath == B.RelativePath
		&& A.VirtualPath == B.VirtualPath;
}
