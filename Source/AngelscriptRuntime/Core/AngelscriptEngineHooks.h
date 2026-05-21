#pragma once

#include "CoreMinimal.h"

class UActorComponent;
class ULevel;
class UASClass;

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
};
