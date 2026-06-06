#pragma once

#include "CoreMinimal.h"

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

// Reload-lifecycle delegates moved here from AngelscriptClassGenerator.h as part
// of refactor-as-runtime-deglobalize-completion: they used to live as
// process-wide static fields on FAngelscriptClassGenerator but are now
// engine-owned (held inside FAngelscriptEngineHooks below). Subscribers should
// register through `FAngelscriptEngine::Get().GetHooks().GetOnXxx()` instead of
// through the class-generator statics.
typedef const TArray<TPair<FName, int64>>& EnumNameList;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAngelscriptPostReload, bool);
DECLARE_MULTICAST_DELEGATE(FOnAngelscriptFullReload);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptLiteralAssetReload, UObject*, UObject*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptClassReload, UClass*, UClass*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAngelscriptEnumCreated, UEnum*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptEnumChanged, UEnum*, EnumNameList);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptStructReload, UScriptStruct*, UScriptStruct*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptDelegateReload, UDelegateFunction*, UDelegateFunction*);

struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineHooks
{
public:
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

	// Reload-lifecycle hooks (engine-owned; previously static on FAngelscriptClassGenerator).
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

private:
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
