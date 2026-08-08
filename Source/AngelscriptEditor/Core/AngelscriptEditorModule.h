#pragma once

#include "CoreMinimal.h"
#include "HotReload/AngelscriptScriptTestAutomationRefresh.h"
#include "Modules/ModuleManager.h"

class IDirectoryWatcher;
class UObject;
class UASClass;

#if WITH_DEV_AUTOMATION_TESTS
class UPackage;
struct FAssetPickerConfig;
struct FSaveAssetDialogConfig;
struct FAngelscriptEditorModuleTestAccess;

struct FAngelscriptEditorModuleAssetListPopupTestHooks
{
	TFunction<void()> ForceEditorWindowToFront;
	TFunction<void(const FString&)> OpenAssetByPath;
	TFunction<void(UObject*)> OpenAssetByObject;
	TFunction<void(const FAssetPickerConfig&, UASClass*)> ShowAssetPickerMenu;
};

struct FAngelscriptEditorModuleCreateBlueprintPopupTestHooks
{
	TFunction<void()> ForceEditorWindowToFront;
	TFunction<bool(const FString&, bool)> HasAssetsInPath;
	TFunction<FString(const FSaveAssetDialogConfig&)> CreateSaveAssetDialog;
	TFunction<UObject*(UASClass*, UPackage*, FName, UClass*, UClass*)> CreateBlueprintAsset;
	TFunction<void(const FText&)> OpenMessageDialog;
	TFunction<void(UObject*)> AssetCreated;
	TFunction<void(const TArray<UPackage*>&)> PromptForCheckoutAndSave;
	TFunction<void(UObject*)> OpenEditorForAsset;
};

struct FAngelscriptEditorModuleLiteralAssetSaveTestHooks
{
	TFunction<bool()> HasAnyDebugServerClients;
	TFunction<void(const FText&)> OpenMessageDialog;
	TFunction<void(const FString&, const TArray<FString>&)> ReplaceScriptAssetContent;
};
#endif

class ANGELSCRIPTEDITOR_API FAngelscriptEditorModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static void ShowAssetListPopup(const TArray<FString>& AssetPaths, class UASClass* BaseClass);
	static void ShowCreateBlueprintPopup(class UASClass* Class);

	static void GenerateNativeBinds();
	static void GenerateBindDatabases();	
	static void GenerateNewModule(FString ModuleName, TArray<FString> ModuleList, bool bIsEditor);
	
	static FString GetIncludeForModule(UField* Class, FString& HeaderPath);

	static void GenerateBuildFile
	(
		FString ModuleName, 
		TArray<FString>& PublicDependencies, 
		TArray<FString>& PrivateDependencies, 
		TArray<FString>& OutBuildFile, 
		bool removeFirst = true
	);	
			
	static void GenerateSourceFilesV2
	(
		FString NewModuleName, 
		TArray<FString>& ModuleList, 
		bool bIsEditor, 
		TArray<FString>& Header, 
		FString BaseCPPDir
	);

	static void GenerateFunctionEntries
	(
		UClass* Class, 
		TArray<FString>& File, 
		TArray<FString>& CurrentIncludes, 
		TSet<FString>& IncludeSet, 
		FString HeaderPath, 
		FString ModuleName,
		TSet<FString>& ModuleSet
	);
	static void GeneratePluginDirectory(FString PluginName, TArray<FString>& PluginFile, TArray<FString> ModuleNames);
	static bool FindFunctionDefinitionLine(const FString& FunctionSymbolName, const FString& FunctionModuleName, uint32& OutLineNumber, FString& OutSourceFile);
	static void OriginalGenerate();

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FAngelscriptEditorModuleTestAccess;
#endif
	void RegisterToolsMenuEntries();
	TArray<TPair<FString, FDelegateHandle>> DirectoryWatchHandles;
	FDelegateHandle StateDumpExtensionHandle;
	FDelegateHandle DebugListAssetsBridgeHandle;
	FDelegateHandle EditorCreateBlueprintBridgeHandle;
	TUniquePtr<FAngelscriptScriptTestAutomationRefresh>
		ScriptTestAutomationRefresh;
};

#if WITH_DEV_AUTOMATION_TESTS
struct FAngelscriptEditorModuleTestAccess
{
	static void SetDirectoryWatcherResolver(TFunction<IDirectoryWatcher*()> InResolver);
	static void ResetDirectoryWatcherResolver();
	static void SetAssetListPopupTestHooks(FAngelscriptEditorModuleAssetListPopupTestHooks InHooks);
	static void ResetAssetListPopupTestHooks();
	static void SetCreateBlueprintPopupTestHooks(FAngelscriptEditorModuleCreateBlueprintPopupTestHooks InHooks);
	static void ResetCreateBlueprintPopupTestHooks();
	static void SetLiteralAssetSaveTestHooks(FAngelscriptEditorModuleLiteralAssetSaveTestHooks InHooks);
	static void ResetLiteralAssetSaveTestHooks();
	static void SetPlatformExecuteOverride(TFunction<bool(const TCHAR*, const TCHAR*, const TCHAR*)> InOverride);
	static void ResetPlatformExecuteOverride();
	static void SetSnippetRunnerOpenOverride(TFunction<void()> InOverride);
	static void ResetSnippetRunnerOpenOverride();
	static void SetClassViewerRefreshOverride(TFunction<void()> InOverride);
	static void ResetClassViewerRefreshOverride();
	static void SetOnEngineInitDoneOverride(TFunction<void()> InOverride);
	static void ResetOnEngineInitDoneOverride();
	static void InvokeOnLiteralAssetSaved(UObject* Object);
	static bool IsLiteralAssetPreSaveRegistered();
	static bool HasStateDumpExtensionHandle(const FAngelscriptEditorModule& Module);
	static bool ShouldShowAssetListPopupCreateButton(UASClass* BaseClass);
	static void RegisterToolsMenuEntries(FAngelscriptEditorModule& Module);
	static void InvokeOnEngineInitDone();
	static void BroadcastRegisteredOnPostEngineInit();
};
#endif
