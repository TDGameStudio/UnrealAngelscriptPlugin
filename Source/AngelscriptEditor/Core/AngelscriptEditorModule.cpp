#include "Core/AngelscriptEditorModule.h"
#include "HotReload/AngelscriptDirectoryWatcherInternal.h"
#include "HotReload/ClassReloadHelper.h"
#include "AngelscriptSettings.h"

#include "AngelscriptEditorDebugBridge.h"
#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "EditorMenuExtensions/ScriptEditorMenuExtension.h"
#include "Snippet/AngelscriptSnippetRunnerWindow.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompilerModule.h"
#include "FileHelpers.h"
#include "SPositiveActionButton.h"

#include "ISettingsModule.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "IDirectoryWatcher.h"
#include "DirectoryWatcherModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Misc/ScopedSlowTask.h"

#include "ContentBrowser/AngelscriptContentBrowserDataSource.h"
#include "IContentBrowserDataModule.h"
#include "ContentBrowserDataSubsystem.h"

#include "Editor.h"
#include "SourceCodeNavigation.h"
#include "AngelscriptBindDatabase.h"
#include "AngelscriptBinds.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_MODULE(FAngelscriptEditorModule, AngelscriptEditor);

namespace AngelscriptEditor::Private
{
	void RegisterStateDumpExtension(FDelegateHandle& OutHandle);
	void UnregisterStateDumpExtension(FDelegateHandle& InOutHandle);
}

extern void RegisterAngelscriptSourceNavigation();

static FDelegateHandle GLiteralAssetPreSaveHandle;

//WILL-EDIT
void FunctionTests();
void ForceEditorWindowToFront();
void OnEngineInitDone();
void OnLiteralAssetSaved(UObject* Object);

namespace
{
#if WITH_DEV_AUTOMATION_TESTS
	TFunction<IDirectoryWatcher*()> GDirectoryWatcherResolverForTesting;
	FAngelscriptEditorModuleAssetListPopupTestHooks GAssetListPopupTestHooks;
	FAngelscriptEditorModuleCreateBlueprintPopupTestHooks GCreateBlueprintPopupTestHooks;
	FAngelscriptEditorModuleLiteralAssetSaveTestHooks GLiteralAssetSaveTestHooks;
	TFunction<bool(const TCHAR*, const TCHAR*, const TCHAR*)> GPlatformExecuteOverrideForTesting;
	TFunction<void()> GSnippetRunnerOpenOverrideForTesting;
	TFunction<void()> GOnEngineInitDoneOverrideForTesting;
	int32 GOnPostEngineInitRegistrationCountForTesting = 0;
#endif

	FDelegateHandle GOnPostEngineInitHandle;

	IDirectoryWatcher* ResolveDirectoryWatcher()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GDirectoryWatcherResolverForTesting)
		{
			return GDirectoryWatcherResolverForTesting();
		}
#endif

		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
		return DirectoryWatcherModule.Get();
	}

	void UnregisterDirectoryWatchers(TArray<TPair<FString, FDelegateHandle>>& InOutWatchHandles, IDirectoryWatcher* DirectoryWatcher)
	{
		if (DirectoryWatcher == nullptr)
		{
			return;
		}

		for (const TPair<FString, FDelegateHandle>& WatchHandle : InOutWatchHandles)
		{
			if (WatchHandle.Value.IsValid())
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(WatchHandle.Key, WatchHandle.Value);
			}
		}

		InOutWatchHandles.Reset();
	}

	bool ShouldShowAssetListPopupCreateButton(UASClass* BaseClass)
	{
		return BaseClass != nullptr;
	}

	void ForceEditorWindowToFrontForAssetListPopup()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GAssetListPopupTestHooks.ForceEditorWindowToFront)
		{
			GAssetListPopupTestHooks.ForceEditorWindowToFront();
			return;
		}
#endif

		ForceEditorWindowToFront();
	}

	void OpenAssetEditorForAssetListPopup(const FString& AssetPath)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GAssetListPopupTestHooks.OpenAssetByPath)
		{
			GAssetListPopupTestHooks.OpenAssetByPath(AssetPath);
			return;
		}
#endif

		FScopedSlowTask ProgressBar(2.f, FText::FromString(FString::Printf(TEXT("Opening %s"), *AssetPath)));
		ProgressBar.EnterProgressFrame(0.5f);
		ProgressBar.MakeDialog(false, true);

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetPath);
	}

	void OpenAssetEditorForAssetListPopup(UObject* AssetObject)
	{
		if (AssetObject == nullptr)
		{
			return;
		}

#if WITH_DEV_AUTOMATION_TESTS
		if (GAssetListPopupTestHooks.OpenAssetByObject)
		{
			GAssetListPopupTestHooks.OpenAssetByObject(AssetObject);
			return;
		}
#endif

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetObject);
	}

	bool HasAnyDebugServerClientsForLiteralAssetSave()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GLiteralAssetSaveTestHooks.HasAnyDebugServerClients)
		{
			return GLiteralAssetSaveTestHooks.HasAnyDebugServerClients();
		}
#endif

		return FAngelscriptEngine::Get().HasAnyDebugServerClients();
	}

	void OpenMessageDialogForLiteralAssetSave(const FText& Message)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GLiteralAssetSaveTestHooks.OpenMessageDialog)
		{
			GLiteralAssetSaveTestHooks.OpenMessageDialog(Message);
			return;
		}
#endif

		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}

	void ReplaceScriptAssetContentForLiteralAssetSave(const FString& AssetName, const TArray<FString>& NewContent)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GLiteralAssetSaveTestHooks.ReplaceScriptAssetContent)
		{
			GLiteralAssetSaveTestHooks.ReplaceScriptAssetContent(AssetName, NewContent);
			return;
		}
#endif

		FAngelscriptEngine::Get().ReplaceScriptAssetContent(AssetName, NewContent);
	}

	void ShowAssetPickerMenuForAssetListPopup(const FAssetPickerConfig& AssetPickerConfig, UASClass* BaseClass)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GAssetListPopupTestHooks.ShowAssetPickerMenu)
		{
			GAssetListPopupTestHooks.ShowAssetPickerMenu(AssetPickerConfig, BaseClass);
			return;
		}
#endif

		const FVector2D AssetPickerSize(600.0f, 586.0f);
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		auto ActualWidget = SNew(SBox)
		.WidthOverride(AssetPickerSize.X)
		.HeightOverride(AssetPickerSize.Y)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			]
			+ SVerticalBox::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(SBox)
					.HeightOverride(34.f)
					[
						SNew(SPositiveActionButton)
							.Visibility_Lambda([BaseClass]() {
								return ShouldShowAssetListPopupCreateButton(BaseClass) ? EVisibility::Visible : EVisibility::Collapsed;
							})
							.Text(
								BaseClass != nullptr && BaseClass->IsChildOf(UDataAsset::StaticClass())
									? FText::FromString("Create New Data Asset")
									: FText::FromString("Create New Blueprint")
							)
							.OnClicked_Lambda(
								[BaseClass]()
								{
									if (BaseClass != nullptr)
									{
										FAngelscriptEditorModule::ShowCreateBlueprintPopup(BaseClass);
									}
									return FReply::Handled();
								}
							)
					]
				]
			]
		];

		FMenuBuilder MenuBuilder(/*BShouldCloseAfterSelection=*/ false, /*CommandList=*/ nullptr);
		MenuBuilder.BeginSection("AssetPickerOpenAsset", NSLOCTEXT("GlobalAssetPicker", "WindowTitle", "Open Asset"));
		MenuBuilder.AddWidget(ActualWidget, FText::GetEmpty(), /*bNoIndent=*/ true);
		MenuBuilder.EndSection();

		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		FVector2D WindowPosition = FSlateApplication::Get().GetCursorPos();
		if (!ParentWindow.IsValid())
		{
			TSharedPtr<SDockTab> LevelEditorTab = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTab();
			ParentWindow = LevelEditorTab->GetParentWindow();
			check(ParentWindow.IsValid());
		}

		if (ParentWindow.IsValid())
		{
			FSlateRect ParentMonitorRect = ParentWindow->GetFullScreenInfo();
			const FVector2D MonitorCenter((ParentMonitorRect.Right + ParentMonitorRect.Left) * 0.5f, (ParentMonitorRect.Top + ParentMonitorRect.Bottom) * 0.5f);
			WindowPosition = MonitorCenter - AssetPickerSize * 0.5f;

			FPopupTransitionEffect TransitionEffect(FPopupTransitionEffect::None);
			FSlateApplication::Get().PushMenu(ParentWindow.ToSharedRef(), FWidgetPath(), MenuBuilder.MakeWidget(), WindowPosition, TransitionEffect, /*bFocusImmediately=*/ true);
		}
	}

	void ForceEditorWindowToFrontForCreateBlueprintPopup()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GCreateBlueprintPopupTestHooks.ForceEditorWindowToFront)
		{
			GCreateBlueprintPopupTestHooks.ForceEditorWindowToFront();
			return;
		}
#endif
		ForceEditorWindowToFront();
	}

	FString ShowSaveAssetDialogForCreateBlueprintPopup(const FSaveAssetDialogConfig& SaveAssetDialogConfig)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GCreateBlueprintPopupTestHooks.CreateSaveAssetDialog)
		{
			return GCreateBlueprintPopupTestHooks.CreateSaveAssetDialog(SaveAssetDialogConfig);
		}
#endif
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		return ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	}

	bool HasAssetsForCreateBlueprintPopup(IAssetRegistry& AssetRegistry, const FString& Path, bool bRecursive)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GCreateBlueprintPopupTestHooks.HasAssetsInPath)
		{
			return GCreateBlueprintPopupTestHooks.HasAssetsInPath(Path, bRecursive);
		}
#endif

		return AssetRegistry.HasAssets(*Path, bRecursive);
	}

	void OpenMessageDialogForCreateBlueprintPopup(const FText& Message)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GCreateBlueprintPopupTestHooks.OpenMessageDialog)
		{
			GCreateBlueprintPopupTestHooks.OpenMessageDialog(Message);
			return;
		}
#endif
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}

	UObject* CreateBlueprintAssetForCreateBlueprintPopup(UASClass* Class, UPackage* Package, FName AssetName, UClass* BlueprintClass, UClass* BlueprintGeneratedClass)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GCreateBlueprintPopupTestHooks.CreateBlueprintAsset)
		{
			return GCreateBlueprintPopupTestHooks.CreateBlueprintAsset(Class, Package, AssetName, BlueprintClass, BlueprintGeneratedClass);
		}
#endif
		return FKismetEditorUtilities::CreateBlueprint(
			Class, Package, AssetName, BPTYPE_Normal,
			BlueprintClass, BlueprintGeneratedClass, FName("AngelscriptCreateBlueprint")
		);
	}

	void NotifyAssetCreatedForCreateBlueprintPopup(UObject* Asset)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GCreateBlueprintPopupTestHooks.AssetCreated)
		{
			GCreateBlueprintPopupTestHooks.AssetCreated(Asset);
			return;
		}
#endif
		FAssetRegistryModule::AssetCreated(Asset);
	}

	void PromptForCheckoutAndSaveForCreateBlueprintPopup(const TArray<UPackage*>& Packages)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GCreateBlueprintPopupTestHooks.PromptForCheckoutAndSave)
		{
			GCreateBlueprintPopupTestHooks.PromptForCheckoutAndSave(Packages);
			return;
		}
#endif
		FEditorFileUtils::FPromptForCheckoutAndSaveParams Params;
		FEditorFileUtils::PromptForCheckoutAndSave(Packages, Params);
	}

	void OpenAssetEditorForCreateBlueprintPopup(UObject* Asset)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GCreateBlueprintPopupTestHooks.OpenEditorForAsset)
		{
			GCreateBlueprintPopupTestHooks.OpenEditorForAsset(Asset);
			return;
		}
#endif
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
	}

	bool ExecutePlatformCommandForEditorModule(const TCHAR* CommandType, const TCHAR* Command, const TCHAR* CommandLine)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GPlatformExecuteOverrideForTesting)
		{
			return GPlatformExecuteOverrideForTesting(CommandType, Command, CommandLine);
		}
#endif

		return FPlatformMisc::OsExecute(CommandType, Command, CommandLine);
	}

	void OpenSnippetRunnerWindowForEditorModule()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GSnippetRunnerOpenOverrideForTesting)
		{
			GSnippetRunnerOpenOverrideForTesting();
			return;
		}
#endif

		FAngelscriptSnippetRunnerWindow::OpenWindow();
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FAngelscriptEditorModuleTestAccess::SetDirectoryWatcherResolver(TFunction<IDirectoryWatcher*()> InResolver)
{
	GDirectoryWatcherResolverForTesting = MoveTemp(InResolver);
}

void FAngelscriptEditorModuleTestAccess::ResetDirectoryWatcherResolver()
{
	GDirectoryWatcherResolverForTesting = nullptr;
}

void FAngelscriptEditorModuleTestAccess::SetAssetListPopupTestHooks(FAngelscriptEditorModuleAssetListPopupTestHooks InHooks)
{
	GAssetListPopupTestHooks = MoveTemp(InHooks);
}

void FAngelscriptEditorModuleTestAccess::ResetAssetListPopupTestHooks()
{
	GAssetListPopupTestHooks = FAngelscriptEditorModuleAssetListPopupTestHooks();
}

void FAngelscriptEditorModuleTestAccess::SetCreateBlueprintPopupTestHooks(FAngelscriptEditorModuleCreateBlueprintPopupTestHooks InHooks)
{
	GCreateBlueprintPopupTestHooks = MoveTemp(InHooks);
}

void FAngelscriptEditorModuleTestAccess::ResetCreateBlueprintPopupTestHooks()
{
	GCreateBlueprintPopupTestHooks = FAngelscriptEditorModuleCreateBlueprintPopupTestHooks();
}

void FAngelscriptEditorModuleTestAccess::SetLiteralAssetSaveTestHooks(FAngelscriptEditorModuleLiteralAssetSaveTestHooks InHooks)
{
	GLiteralAssetSaveTestHooks = MoveTemp(InHooks);
}

void FAngelscriptEditorModuleTestAccess::ResetLiteralAssetSaveTestHooks()
{
	GLiteralAssetSaveTestHooks = FAngelscriptEditorModuleLiteralAssetSaveTestHooks();
}

void FAngelscriptEditorModuleTestAccess::SetPlatformExecuteOverride(TFunction<bool(const TCHAR*, const TCHAR*, const TCHAR*)> InOverride)
{
	GPlatformExecuteOverrideForTesting = MoveTemp(InOverride);
}

void FAngelscriptEditorModuleTestAccess::ResetPlatformExecuteOverride()
{
	GPlatformExecuteOverrideForTesting = nullptr;
}

void FAngelscriptEditorModuleTestAccess::SetSnippetRunnerOpenOverride(TFunction<void()> InOverride)
{
	GSnippetRunnerOpenOverrideForTesting = MoveTemp(InOverride);
}

void FAngelscriptEditorModuleTestAccess::ResetSnippetRunnerOpenOverride()
{
	GSnippetRunnerOpenOverrideForTesting = nullptr;
}

void FAngelscriptEditorModuleTestAccess::SetOnEngineInitDoneOverride(TFunction<void()> InOverride)
{
	GOnEngineInitDoneOverrideForTesting = MoveTemp(InOverride);
}

void FAngelscriptEditorModuleTestAccess::ResetOnEngineInitDoneOverride()
{
	GOnEngineInitDoneOverrideForTesting = nullptr;
}

void FAngelscriptEditorModuleTestAccess::InvokeOnLiteralAssetSaved(UObject* Object)
{
	OnLiteralAssetSaved(Object);
}

bool FAngelscriptEditorModuleTestAccess::IsLiteralAssetPreSaveRegistered()
{
	return GLiteralAssetPreSaveHandle.IsValid();
}

bool FAngelscriptEditorModuleTestAccess::HasStateDumpExtensionHandle(const FAngelscriptEditorModule& Module)
{
	return Module.StateDumpExtensionHandle.IsValid();
}

bool FAngelscriptEditorModuleTestAccess::ShouldShowAssetListPopupCreateButton(UASClass* BaseClass)
{
	return ::ShouldShowAssetListPopupCreateButton(BaseClass);
}

void FAngelscriptEditorModuleTestAccess::RegisterToolsMenuEntries(FAngelscriptEditorModule& Module)
{
	Module.RegisterToolsMenuEntries();
}

void FAngelscriptEditorModuleTestAccess::InvokeOnEngineInitDone()
{
	OnEngineInitDone();
}

void FAngelscriptEditorModuleTestAccess::BroadcastRegisteredOnPostEngineInit()
{
	for (int32 InvocationIndex = 0; InvocationIndex < GOnPostEngineInitRegistrationCountForTesting; ++InvocationIndex)
	{
		OnEngineInitDone();
	}
}
#endif

struct FPromptForCheckoutAndSaveParams
{
	FPromptForCheckoutAndSaveParams()
		: Title(NSLOCTEXT("PackagesDialogModule", "PackagesDialogTitle", "Save Content")),
		Message(NSLOCTEXT("PackagesDialogModule", "PackagesDialogMessage", "Select Content to Save"))
	{}

	bool bCheckDirty = false;                       /** If true, only packages that are dirty in PackagesToSave will be saved	*/
	bool bPromptToSave = false;                     /** If true the user will be prompted with a list of packages to save, otherwise all passed in packages are saved */
	bool bAlreadyCheckedOut = false;                /** If true, the user will not be prompted with the source control dialog */
	bool bCanBeDeclined = true;                     /** If true, offer a "Don't Save" option in addition to "Cancel", which will not result in a cancellation return code. */
	bool bIsExplicitSave = false;                   /** If true, marks the save as explicit. Explicit saves are triggered by user facing actions such as Save As, or ctrl + s*/
	FText Title;                                    /** If bPromptToSave true provides a dialog title */
	FText Message;                                  /** If bPromptToSave true provides a dialog message */
	TArray<UPackage*>* OutFailedPackages = nullptr; /** [out] If specified, will be filled in with all of the packages that failed to save successfully */
};

void OnScriptFileChanges(const TArray<FFileChangeData>& Changes)
{
	// Ignore file changes before initialization finishes
	if (!FAngelscriptEngine::IsInitialized())
		return;

	FAngelscriptEngine& AngelscriptManager = FAngelscriptEngine::Get();
	AngelscriptEditor::Private::QueueScriptFileChanges(
		Changes,
		AngelscriptManager.AllRootPaths,
		AngelscriptManager,
		IFileManager::Get(),
		[&AngelscriptManager](const FString& AbsoluteFolderPath)
		{
			return AngelscriptEditor::Private::GatherLoadedScriptsForFolder(AngelscriptManager, AbsoluteFolderPath);
		});
}

void ForceEditorWindowToFront()
{
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!ParentWindow.IsValid())
	{
		TSharedPtr<SDockTab> LevelEditorTab = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTab();
		ParentWindow = LevelEditorTab->GetParentWindow();
		check(ParentWindow.IsValid());
	}
	if (ParentWindow.IsValid())
	{
		ParentWindow->HACK_ForceToFront();
	}
}

void OnEngineInitDone()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GOnEngineInitDoneOverrideForTesting)
	{
		GOnEngineInitDoneOverrideForTesting();
	}
#endif

	// Register the content browser data source
	auto* DataSource = NewObject<UAngelscriptContentBrowserDataSource>(GetTransientPackage(), "AngelscriptData", RF_MarkAsRootSet | RF_Transient);
	DataSource->Initialize();

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	ContentBrowserData->ActivateDataSource("AngelscriptData");
}

void OnLiteralAssetSaved(UObject* Object)
{
	if (UCurveFloat* Curve = Cast<UCurveFloat>(Object))
	{
		if (!HasAnyDebugServerClientsForLiteralAssetSave())
		{
			OpenMessageDialogForLiteralAssetSave(FText::FromString(TEXT("Visual Studio Code extension must be running to save a script literal curve")));
			return;
		}

		TArray<FString> NewContent;

		int GraphWidth = 64;
		int GraphHeight = 16;
		
		float MinTime;
		float MaxTime;
		Curve->FloatCurve.GetTimeRange(MinTime, MaxTime);

		float MinValue;
		float MaxValue;
		Curve->FloatCurve.GetValueRange(MinValue, MaxValue);

		if (MaxTime > MinTime && MaxValue > MinValue)
		{
			FString EmptyLine;
			EmptyLine.Reserve(GraphWidth);
			for (int Char = 0; Char < GraphWidth; ++Char)
				EmptyLine.AppendChar(' ');
			for (int Line = 0; Line < GraphHeight; ++Line)
				NewContent.Add(EmptyLine);

			float ColInterval = (MaxTime - MinTime) / (float)(GraphWidth - 1);
			float RowInterval = (MaxValue - MinValue) / (float)(GraphHeight);
			for (int Column = 0; Column < GraphWidth; ++Column)
			{
				float Time = MinTime + (Column * ColInterval);
				float Value = Curve->FloatCurve.Eval(Time);

				int TargetRow = FMath::Clamp(FMath::FloorToInt32((Value - MinValue) / RowInterval), 0, GraphHeight-1);
				float RowBase = TargetRow * RowInterval;
				float PctInRow = FMath::Clamp((Value - RowBase) / RowInterval, 0.f, 1.f);

				TCHAR CharType = L'·';
				if (PctInRow < 0.33f)
					CharType = '.';
				else if (PctInRow > 0.66f)
					CharType = '\'';

				NewContent[GraphHeight - TargetRow - 1][Column] = CharType;
			}

			for (int Line = 0; Line < GraphHeight; ++Line)
			{
				if (Line == 0)
				{
					FString Value = FString::SanitizeFloat(MaxValue);
					if (Value.Len() >= 4)
						NewContent[Line] = Value.Mid(0, 4) + TEXT("|") + NewContent[Line] + TEXT("|");
					else
						NewContent[Line] = Value.RightPad(4) + TEXT("|") + NewContent[Line] + TEXT("|");
				}
				else if (Line == GraphHeight - 1)
				{
					FString Value = FString::SanitizeFloat(MinValue);
					if (Value.Len() >= 4)
						NewContent[Line] = Value.Mid(0, 4) + TEXT("|") + NewContent[Line] + TEXT("|");
					else
						NewContent[Line] = Value.RightPad(4) + TEXT("|") + NewContent[Line] + TEXT("|");
				}
				else
				{
					NewContent[Line] = TEXT("    |") + NewContent[Line] + TEXT("|");
				}
			}

			FString RuleLine = TEXT("    -");
			for (int Char = 0; Char < GraphWidth; ++Char)
				RuleLine.AppendChar('-');
			RuleLine += TEXT("-");

			NewContent.Insert(TEXT("/*"), 0);
			NewContent.Insert(RuleLine, 1);

			FString BottomLegend = TEXT("    ");

			FString MinTimeStr = FString::SanitizeFloat(MinTime);
			if (MinTimeStr.Len() >= 4)
				BottomLegend += MinTimeStr.Mid(0, 4);
			else
				BottomLegend += MinTimeStr.RightPad(4);

			for (int Char = 0; Char < GraphWidth - 6; ++Char)
				BottomLegend.AppendChar(' ');

			FString MaxTimeStr = FString::SanitizeFloat(MaxTime);
			if (MaxTimeStr.Len() >= 4)
				BottomLegend += MaxTimeStr.Mid(0, 4);
			else
				BottomLegend += MaxTimeStr.LeftPad(4);

			NewContent.Add(RuleLine);
			NewContent.Add(BottomLegend);
			NewContent.Add(TEXT("*/"));
		}

		for (const FRichCurveKey& Key : Curve->FloatCurve.Keys)
		{
			if (Key.InterpMode == ERichCurveInterpMode::RCIM_Constant)
			{
				NewContent.Add(FString::Format(TEXT("AddConstantCurveKey({0}, {1});"), {
					FString::SanitizeFloat(Key.Time),
					FString::SanitizeFloat(Key.Value),
				}));
			}
			else if (Key.InterpMode == ERichCurveInterpMode::RCIM_Linear)
			{
				NewContent.Add(FString::Format(TEXT("AddLinearCurveKey({0}, {1});"), {
					FString::SanitizeFloat(Key.Time),
					FString::SanitizeFloat(Key.Value),
				}));
			}
			else if (Key.InterpMode == ERichCurveInterpMode::RCIM_Cubic)
			{
				if (Key.TangentMode == ERichCurveTangentMode::RCTM_Auto)
				{
					NewContent.Add(FString::Format(TEXT("AddAutoCurveKey({0}, {1});"), {
						FString::SanitizeFloat(Key.Time),
						FString::SanitizeFloat(Key.Value),
					}));
				}
				//else if (Key.TangentMode == ERichCurveTangentMode::RCTM_SmartAuto)				
				//{
				//	NewContent.Add(FString::Format(TEXT("AddSmartAutoCurveKey({0}, {1});"), {
				//		FString::SanitizeFloat(Key.Time),
				//		FString::SanitizeFloat(Key.Value),
				//	}));
				//}
				else if (Key.TangentMode == ERichCurveTangentMode::RCTM_Break || Key.TangentMode == ERichCurveTangentMode::RCTM_User)
				{
					if (Key.TangentWeightMode == ERichCurveTangentWeightMode::RCTWM_WeightedNone)
					{
						if (Key.TangentMode == ERichCurveTangentMode::RCTM_Break)
						{
							NewContent.Add(FString::Format(TEXT("AddCurveKeyBrokenTangent({0}, {1}, {2}, {3});"), {
								FString::SanitizeFloat(Key.Time),
								FString::SanitizeFloat(Key.Value),
								FString::SanitizeFloat(Key.ArriveTangent),
								FString::SanitizeFloat(Key.LeaveTangent),
							}));
						}
						else
						{
							NewContent.Add(FString::Format(TEXT("AddCurveKeyTangent({0}, {1}, {2});"), {
								FString::SanitizeFloat(Key.Time),
								FString::SanitizeFloat(Key.Value),
								FString::SanitizeFloat(Key.ArriveTangent),
							}));
						}
					}
					else
					{
						FString FunctionName;
						if (Key.TangentWeightMode == ERichCurveTangentWeightMode::RCTWM_WeightedArrive)
							FunctionName = TEXT("AddCurveKeyWeightedArriveTangent");
						else if (Key.TangentWeightMode == ERichCurveTangentWeightMode::RCTWM_WeightedLeave)
							FunctionName = TEXT("AddCurveKeyWeightedLeaveTangent");
						else if (Key.TangentWeightMode == ERichCurveTangentWeightMode::RCTWM_WeightedBoth)
							FunctionName = TEXT("AddCurveKeyWeightedBothTangent");

						FString BrokenBool = TEXT("false");
						if (Key.TangentMode == ERichCurveTangentMode::RCTM_Break)
							BrokenBool = TEXT("true");

						NewContent.Add(FString::Format(TEXT("{0}({1}, {2}, {3}, {4}, {5}, {6}, {7});"), {
							FunctionName,
							FString::SanitizeFloat(Key.Time),
							FString::SanitizeFloat(Key.Value),
							BrokenBool,
							FString::SanitizeFloat(Key.ArriveTangent),
							FString::SanitizeFloat(Key.LeaveTangent),
							FString::SanitizeFloat(Key.ArriveTangentWeight),
							FString::SanitizeFloat(Key.LeaveTangentWeight),
						}));
					}
				}
			}
		}

		if (Curve->FloatCurve.DefaultValue != MAX_flt)
			NewContent.Add(FString::Format(TEXT("DefaultValue = {0};"), {FString::SanitizeFloat(Curve->FloatCurve.DefaultValue)}));

		switch (Curve->FloatCurve.PreInfinityExtrap.GetValue())
		{
		case ERichCurveExtrapolation::RCCE_Cycle: NewContent.Add(TEXT("PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Cycle;")); break;
		case ERichCurveExtrapolation::RCCE_CycleWithOffset: NewContent.Add(TEXT("PreInfinityExtrap = ERichCurveExtrapolation::RCCE_CycleWithOffset;")); break;
		case ERichCurveExtrapolation::RCCE_Oscillate: NewContent.Add(TEXT("PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Oscillate;")); break;
		case ERichCurveExtrapolation::RCCE_Linear: NewContent.Add(TEXT("PreInfinityExtrap = ERichCurveExtrapolation::RCCE_Linear;")); break;
		case ERichCurveExtrapolation::RCCE_None: NewContent.Add(TEXT("PreInfinityExtrap = ERichCurveExtrapolation::RCCE_None;")); break;
		}

		switch (Curve->FloatCurve.PostInfinityExtrap.GetValue())
		{
		case ERichCurveExtrapolation::RCCE_Cycle: NewContent.Add(TEXT("PostInfinityExtrap = ERichCurveExtrapolation::RCCE_Cycle;")); break;
		case ERichCurveExtrapolation::RCCE_CycleWithOffset: NewContent.Add(TEXT("PostInfinityExtrap = ERichCurveExtrapolation::RCCE_CycleWithOffset;")); break;
		case ERichCurveExtrapolation::RCCE_Oscillate: NewContent.Add(TEXT("PostInfinityExtrap = ERichCurveExtrapolation::RCCE_Oscillate;")); break;
		case ERichCurveExtrapolation::RCCE_Linear: NewContent.Add(TEXT("PostInfinityExtrap = ERichCurveExtrapolation::RCCE_Linear;")); break;
		case ERichCurveExtrapolation::RCCE_None: NewContent.Add(TEXT("PostInfinityExtrap = ERichCurveExtrapolation::RCCE_None;")); break;
		}

		ReplaceScriptAssetContentForLiteralAssetSave(Curve->GetName(), NewContent);
	}
	else
	{
		OpenMessageDialogForLiteralAssetSave(FText::FromString(TEXT("Cannot save asset declared as an angelscript asset literal")));
	}
}

void OnLiteralAssetPreSave(UObject* Object, FObjectPreSaveContext SaveContext)
{
	if (!FAngelscriptEngine::IsInitialized())
		return;
	if (!GIsEditor)
		return;
	if (Object == nullptr)
		return;
	if (Object->GetOutermost() == FAngelscriptEngine::Get().AssetsPackage)
		OnLiteralAssetSaved(Object);
}

void FAngelscriptEditorModule::StartupModule()
{
	FClassReloadHelper::Init();
	RegisterAngelscriptSourceNavigation();

	if (FAngelscriptEngine::IsInitialized() && FAngelscriptEngine::Get().IsInitialCompileFinished())
		FComponentTypeRegistry::Get().Invalidate();

	if (!GOnPostEngineInitHandle.IsValid())
	{
		GOnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddStatic(&OnEngineInitDone);
#if WITH_DEV_AUTOMATION_TESTS
		GOnPostEngineInitRegistrationCountForTesting = 1;
#endif
	}

	UScriptEditorMenuExtension::InitializeExtensions();
	AngelscriptEditor::Private::RegisterStateDumpExtension(StateDumpExtensionHandle);

	// Register a directory watch on the script directory so we know when to reload
	IDirectoryWatcher* DirectoryWatcher = ResolveDirectoryWatcher();

	if (ensure(DirectoryWatcher != nullptr))
	{
		UnregisterDirectoryWatchers(DirectoryWatchHandles, DirectoryWatcher);

		TArray<FString> AllRootPaths = FAngelscriptEngine::MakeAllScriptRoots();
		for (const auto& RootPath : AllRootPaths)
		{
			FDelegateHandle WatchHandle;
			if (DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				*RootPath,
				IDirectoryWatcher::FDirectoryChanged::CreateStatic(&OnScriptFileChanges),
				WatchHandle,
				IDirectoryWatcher::IncludeDirectoryChanges) && WatchHandle.IsValid())
			{
				DirectoryWatchHandles.Emplace(RootPath, WatchHandle);
			}
		}
	}

	// Register the angelscript settings that can be edited in project settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
	{
		SettingsModule->RegisterSettings(
			"Project", "Plugins", "Angelscript", 
			NSLOCTEXT("Angelscript", "AngelscriptSettingsTitle", "Angelscript"),
			NSLOCTEXT("Angelscript", "AngelscriptSettingsDescription", "Configuration for behavior of the angelscript compiler and script engine."),
			GetMutableDefault<UAngelscriptSettings>()
		);
	}

	// Helper to pop open the content browser or asset editor from the debug server
	if (DebugListAssetsBridgeHandle.IsValid())
	{
		FAngelscriptEditorDebugBridge::GetDebugListAssets().Remove(DebugListAssetsBridgeHandle);
		DebugListAssetsBridgeHandle.Reset();
	}
	DebugListAssetsBridgeHandle = FAngelscriptEditorDebugBridge::GetDebugListAssets().AddLambda(
		[](TArray<FString> AssetPaths, UASClass* BaseClass)
		{
			FAngelscriptEditorModule::ShowAssetListPopup(AssetPaths, BaseClass);
		}
	);

	// Helper to create a new blueprint from a script class
	if (EditorCreateBlueprintBridgeHandle.IsValid())
	{
		FAngelscriptEditorDebugBridge::GetEditorCreateBlueprint().Remove(EditorCreateBlueprintBridgeHandle);
		EditorCreateBlueprintBridgeHandle.Reset();
	}
	EditorCreateBlueprintBridgeHandle = FAngelscriptEditorDebugBridge::GetEditorCreateBlueprint().AddLambda(
		[](UASClass* ScriptClass)
		{
			FAngelscriptEditorModule::ShowCreateBlueprintPopup(ScriptClass);
		}
	);

	GLiteralAssetPreSaveHandle = FCoreUObjectDelegates::OnObjectPreSave.AddStatic(&OnLiteralAssetPreSave);

	// Register a callback that notifies us when the editor is ready for us to register UI extensions
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAngelscriptEditorModule::RegisterToolsMenuEntries));
}

void FAngelscriptEditorModule::ShowCreateBlueprintPopup(UASClass* Class)
{
	if (Class == nullptr)
	{
		return;
	}

	const bool bIsDataAsset = Class->IsChildOf<UDataAsset>();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	ForceEditorWindowToFrontForCreateBlueprintPopup();

	FString Title;
	if (bIsDataAsset)
		Title = FString::Printf(TEXT("Create Asset of %s%s"), Class->GetPrefixCPP(), *Class->GetName());
	else
		Title = FString::Printf(TEXT("Create Blueprint of %s%s"), Class->GetPrefixCPP(), *Class->GetName());

	FString AssetPath;
	if (FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath().IsBound())
		AssetPath = FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath().Execute(Class);

	// If we don't have a name, try a standard name
	if (AssetPath.Len() == 0)
	{
		if (bIsDataAsset)
			AssetPath = FString::Printf(TEXT("DA_%s"), *Class->GetName());
		else
			AssetPath = FString::Printf(TEXT("BP_%s"), *Class->GetName());
	}

	// Try to find a folder to place it in if we haven't specified one
	if (!AssetPath.StartsWith(TEXT("/")))
	{
		FString ScriptRelativePath = Class->GetRelativeSourceFilePath();

		TArray<FString> Subfolders;
		ScriptRelativePath.ParseIntoArray(Subfolders, TEXT("/"), true);
		
		FString InitialDirectory = TEXT("/Game");
		for (int i = Subfolders.Num() - 2; i >= 0; --i)
		{
			FString TestDirectory = InitialDirectory;
			for (int n = 0; n <= i; ++n)
				TestDirectory = TestDirectory / Subfolders[n];

			if (HasAssetsForCreateBlueprintPopup(AssetRegistry, TestDirectory, true))
			{
				InitialDirectory = TestDirectory;
				break;
			}
		}

		AssetPath = InitialDirectory / AssetPath;
	}

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	{
		SaveAssetDialogConfig.DefaultPath = FPaths::GetPath(AssetPath);
		SaveAssetDialogConfig.DefaultAssetName = FPaths::GetCleanFilename(AssetPath);
		SaveAssetDialogConfig.AssetClassNames.Add(Class->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
		SaveAssetDialogConfig.DialogTitleOverride = FText::FromString(Title);
	}

	FString SaveObjectPath = ShowSaveAssetDialogForCreateBlueprintPopup(SaveAssetDialogConfig);
	
	if (!SaveObjectPath.IsEmpty())
	{
		// Get the full name of where we want to create the physics asset.
		const FString UserPackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		FName AssetName(*FPackageName::GetLongPackageAssetName(UserPackageName));

		// Check if the user inputed a valid asset name, if they did not, give it the generated default name
		if (AssetName == NAME_None)
		{
			OpenMessageDialogForCreateBlueprintPopup(FText::FromString(TEXT("Error: Invalid name for new asset.")));
			return;
		}

		// Create a new package for the asset
		UPackage* Package = CreatePackage(*UserPackageName);
		UObject* Asset = nullptr;
		check(Package);

		if (bIsDataAsset)
		{
			Asset = NewObject<UDataAsset>(Package, Class, AssetName, RF_Public | RF_Transactional | RF_Standalone);
		}
		else
		{
			UClass* BlueprintClass = nullptr;
			UClass* BlueprintGeneratedClass = nullptr;

			IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
			KismetCompilerModule.GetBlueprintTypesForClass(Class, BlueprintClass, BlueprintGeneratedClass);

			Asset = CreateBlueprintAssetForCreateBlueprintPopup(Class, Package, AssetName, BlueprintClass, BlueprintGeneratedClass);
		}

		if (Asset == nullptr)
		{
			return;
		}

		NotifyAssetCreatedForCreateBlueprintPopup(Asset);

		// Mark the package dirty...
		Package->MarkPackageDirty();

		TArray<UPackage*> Packages;
		Packages.Add(Package);

		PromptForCheckoutAndSaveForCreateBlueprintPopup(Packages);

		OpenAssetEditorForCreateBlueprintPopup(Asset);
	}
}

void FAngelscriptEditorModule::ShowAssetListPopup(const TArray<FString>& AssetPaths, UASClass* BaseClass)
{
	// Ignore open blueprint messages until everything is initialized
	if (!FAngelscriptEngine::IsInitialized())
		return;
	if (!FAngelscriptEngine::Get().bIsInitialCompileFinished)
		return;

	ForceEditorWindowToFrontForAssetListPopup();

	if (AssetPaths.Num() == 1)
	{
		OpenAssetEditorForAssetListPopup(AssetPaths[0]);
	}
	else
	{
		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.OnAssetDoubleClicked = FOnAssetSelected::CreateLambda([](const FAssetData& AssetData)
		{
			if (UObject* ObjectToEdit = AssetData.GetAsset())
			{
				OpenAssetEditorForAssetListPopup(ObjectToEdit);
			}
		});

		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateLambda([](const TArray<FAssetData>& SelectedAssets)
		{
			for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
			{
				if (UObject* ObjectToEdit = AssetIt->GetAsset())
				{
					OpenAssetEditorForAssetListPopup(ObjectToEdit);
				}
			}
		});

		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bShowBottomToolbar = true;
		AssetPickerConfig.bAutohideSearchBar = false;
		AssetPickerConfig.bCanShowClasses = false;
		AssetPickerConfig.bAddFilterUI = true;
		AssetPickerConfig.SaveSettingsName = TEXT("GlobalAssetPicker");

		for (const FString& Path : AssetPaths)
		{
			AssetPickerConfig.Filter.PackageNames.Add(*Path);
		}

		ShowAssetPickerMenuForAssetListPopup(AssetPickerConfig, BaseClass);
	}
}

void FAngelscriptEditorModule::ShutdownModule()
{
	if (GOnPostEngineInitHandle.IsValid())
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(GOnPostEngineInitHandle);
		GOnPostEngineInitHandle.Reset();
	}
#if WITH_DEV_AUTOMATION_TESTS
	GOnPostEngineInitRegistrationCountForTesting = 0;
#endif

	if (DebugListAssetsBridgeHandle.IsValid())
	{
		FAngelscriptEditorDebugBridge::GetDebugListAssets().Remove(DebugListAssetsBridgeHandle);
		DebugListAssetsBridgeHandle.Reset();
	}
	if (EditorCreateBlueprintBridgeHandle.IsValid())
	{
		FAngelscriptEditorDebugBridge::GetEditorCreateBlueprint().Remove(EditorCreateBlueprintBridgeHandle);
		EditorCreateBlueprintBridgeHandle.Reset();
	}

	if (GLiteralAssetPreSaveHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPreSave.Remove(GLiteralAssetPreSaveHandle);
		GLiteralAssetPreSaveHandle.Reset();
	}

	AngelscriptEditor::Private::UnregisterStateDumpExtension(StateDumpExtensionHandle);

	UnregisterDirectoryWatchers(DirectoryWatchHandles, ResolveDirectoryWatcher());

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Angelscript");
	}

	// Unregister the tool menu extension
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FAngelscriptEditorModule::RegisterToolsMenuEntries()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Extend the Tools -> Programming menu
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Tools");
	if (!Menu) return;
	FToolMenuSection& Section = Menu->FindOrAddSection("Programming");
	
	//WILL-EDIT
	FToolUIActionChoice Action(FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)
	{
		// Open VS Code to the <Project>/Script directory
		const FString ScriptPath = FPaths::ProjectDir() / TEXT("Script");
		ExecutePlatformCommandForEditorModule(nullptr, TEXT("code"), *FString::Printf(TEXT("\"%s\""), *ScriptPath));
	}));
	
	Section.AddMenuEntry
	(
		"ASOpenCode",
		NSLOCTEXT("Angelscript", "OpenCode.Label", "Open Angelscript workspace (VS Code)"),
		NSLOCTEXT("Angelscript", "OpenCode.ToolTip", "Opens Visual Studio Code in this project's Angelscript workspace"),
		FSourceCodeNavigation::GetOpenSourceCodeIDEIcon(),
		Action
	);

	FToolUIActionChoice SnippetAction(FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)
	{
		OpenSnippetRunnerWindowForEditorModule();
	}));

	Section.AddMenuEntry
	(
		"ASRunSnippet",
		NSLOCTEXT("Angelscript", "RunSnippet.Label", "Run Angelscript Snippet"),
		NSLOCTEXT("Angelscript", "RunSnippet.ToolTip", "Opens an editor window for executing an Angelscript snippet through /Angelscript/Memory/Immediate."),
		FSourceCodeNavigation::GetOpenSourceCodeIDEIcon(),
		SnippetAction
	);

	FToolUIActionChoice GenerateAction(FExecuteAction::CreateLambda([]() { GenerateNativeBinds(); }));

	FToolMenuSection& BindSection = Menu->FindOrAddSection("Programming Binds");

	BindSection.AddMenuEntry
	(
		"ASGenerateBindings",
		NSLOCTEXT("Angelscript", "GenerateBind.Label", "Legacy Native Bind Generator (Debug Only)"),
		NSLOCTEXT("Angelscript", "GenerateBind.ToolTip", "Legacy editor-side generator retained only for debugging old FunctionCallers output. The UHT-based AngelscriptUHTTool pipeline is the primary path."),
		FSourceCodeNavigation::GetOpenSourceCodeIDEIcon(),
		GenerateAction
	);

	FToolUIActionChoice TestAction(FExecuteAction::CreateLambda( []() { FunctionTests(); } ));

	Section.AddMenuEntry
	(
		"Function Tests",
		NSLOCTEXT("Angelscript", "OpenCode.Label", "Run Function Tests"),
		NSLOCTEXT("Angelscript", "OpenCode.ToolTip", "Runs some Tests for debugging purposes"),
		FSourceCodeNavigation::GetOpenSourceCodeIDEIcon(),
		TestAction
	);
}

