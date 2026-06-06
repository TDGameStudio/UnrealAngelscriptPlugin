#include "ScriptEditorMenuExtension.h"
#include "AngelscriptEngine.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"
#include "AngelscriptRuntimeModule.h"
#include "IStructureDetailsView.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "ScriptEditorPrompts.h"
#include "LevelEditor.h"
#include "SourceCodeNavigation.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	TFunction<bool()> GShiftModifierOverrideForTesting;
	TFunction<bool(const UFunction*)> GNavigateToFunctionOverrideForTesting;
	TFunction<void(const UClass*)> GNavigateToClassOverrideForTesting;
}
#endif

namespace
{
	bool IsShiftModifierDown()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GShiftModifierOverrideForTesting)
		{
			return GShiftModifierOverrideForTesting();
		}
#endif
		return FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	}

	bool NavigateToFunctionForMenuExtension(const UFunction* Function)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GNavigateToFunctionOverrideForTesting)
		{
			return GNavigateToFunctionOverrideForTesting(Function);
		}
#endif
		return FSourceCodeNavigation::NavigateToFunction(Function);
	}

	void NavigateToClassForMenuExtension(const UClass* Class)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GNavigateToClassOverrideForTesting)
		{
			GNavigateToClassOverrideForTesting(Class);
			return;
		}
#endif
		FSourceCodeNavigation::NavigateToClass(Class);
	}
}

void UScriptEditorMenuExtension::InitializeExtensions()
{
	// PostReload hook used to be a process-wide static
	// (FAngelscriptClassGenerator::OnPostReload). After the runtime
	// de-globalization (refactor-as-runtime-deglobalize-completion / Section 4)
	// it lives on per-engine FAngelscriptEngineHooks, so subscribe through the
	// extension registry to catch every engine's lifecycle.
	class FScriptEditorMenuPostReloadExtension : public IAngelscriptExtension
	{
	public:
		virtual void OnEngineAttached(FAngelscriptEngine& Engine) override
		{
			Handle = Engine.GetHooks().GetOnPostReload().AddLambda([](bool bFullReload)
			{
				if (bFullReload)
				{
					UScriptEditorMenuExtension::UnregisterExtensions();
					UScriptEditorMenuExtension::RegisterExtensions();
				}
			});
		}

		virtual void OnEngineDetached(FAngelscriptEngine& Engine) override
		{
			if (Handle.IsValid())
			{
				Engine.GetHooks().GetOnPostReload().Remove(Handle);
			}
			Handle.Reset();
		}

	private:
		FDelegateHandle Handle;
	};

	FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(
		MakeShared<FScriptEditorMenuPostReloadExtension>());
	FAngelscriptEngineExtensionRegistry::Get().ReplayCurrentEngine();

	FCoreDelegates::OnEnginePreExit.AddLambda([]()
	{
		UScriptEditorMenuExtension::UnregisterExtensions();
	});

	if (FAngelscriptEngine::IsInitialized() && FAngelscriptEngine::Get().IsInitialCompileFinished())
	{
		UScriptEditorMenuExtension::RegisterExtensions();
	}
}

TArray<UScriptEditorMenuExtension::FRegisteredExtensionSnapshot> UScriptEditorMenuExtension::GetRegisteredExtensionSnapshots()
{
	TArray<FRegisteredExtensionSnapshot> Result;
	Result.Reserve(RegisteredExtensions.Num());
	for (const FRegisteredExtender& RegisteredExtension : RegisteredExtensions)
	{
		Result.Add({
			RegisteredExtension.Location,
			RegisteredExtension.ExtensionPoint,
			RegisteredExtension.SectionName,
		});
	}

	return Result;
}

UWorld* UScriptEditorMenuExtension::GetWorld() const
{
	return (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
}

TArray<UFunction*> UScriptEditorMenuExtension::GatherExtensionFunctions() const
{
	TArray<UFunction*> Functions;

	const static FName NAME_CallInEditor(TEXT("CallInEditor"));
	for (TFieldIterator<UFunction> FunctionIt(GetClass(), EFieldIterationFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* Function = *FunctionIt;
		if (Function == nullptr)
			continue;
		if (!Function->HasMetaData(NAME_CallInEditor))
			continue;
		if (Function->GetReturnProperty() != nullptr)
			continue;

		Functions.Add(Function);
	}

	return Functions;
}

FName UScriptEditorMenuExtension::GetExtensionPointOrDefault() const
{
	if (!ExtensionPoint.IsNone())
		return ExtensionPoint;

	switch (ExtensionMenu)
	{
		case EScriptEditorMenuExtensionLocation::LevelViewport_ContextMenu:
			return "ActorOptions";
		case EScriptEditorMenuExtensionLocation::ContentBrowser_AssetViewContextMenu:
			return "CommonAssetActions";
	}

	return NAME_None;
}

UObject* UScriptEditorMenuExtension::GetToolMenuContext(TSubclassOf<UObject> ContextClass) const
{
	if (ActiveToolMenuContext != nullptr && ContextClass.Get() != nullptr)
	{
		return ActiveToolMenuContext->FindByClass(ContextClass.Get());
	}

	return nullptr;
}

TSharedRef<FExtender> UScriptEditorMenuExtension::Extend(const TSharedRef<FUICommandList> CommandList, FExtenderSelection Selection) const
{
	FEditorScriptExecutionGuard ScriptGuard;
	TGuardValue<FExtenderSelection> ScopeSelection(CurrentSelection, Selection);

	TSharedRef<FExtender> Extender(new FExtender());

	if (!ShouldExtend())
		return Extender;

	TArray<UFunction*> Functions = GatherExtensionFunctions();
	if (Functions.Num() == 0)
		return Extender;

	Extender->AddMenuExtension(
		GetExtensionPointOrDefault(),
		(EExtensionHook::Position)ExtensionOrder,
		CommandList,
		FMenuExtensionDelegate::CreateLambda(
			[Extension=this, Functions, Selection](FMenuBuilder& MenuBuilder)
			{
				Extension->BuildMenu(MenuBuilder, Functions, Selection);
			}
		));

	return Extender;
}

void UScriptEditorMenuExtension::CallFunctionOnSelection(UFunction* Function, FExtenderSelection Selection) const
{
	FScriptEditorPromptOptions Options;
	FScriptEditorPrompts::ShowPromptToCallFunction(
		this,
		Function->GetFName(),
		Options,
		TArray<UObject*>()
	);
}

const static FName NAME_EXT_EditorIcon(TEXT("EditorIcon"));
const static FName NAME_EXT_ActionCanExecute(TEXT("ActionCanExecute"));
const static FName NAME_EXT_ActionIsVisible(TEXT("ActionIsVisible"));
const static FName NAME_EXT_ActionIsChecked(TEXT("ActionIsChecked"));
const static FName NAME_EXT_ActionType(TEXT("ActionType"));
const static FName NAME_EXT_EditorButtonStyle(TEXT("EditorButtonStyle"));
const static FName NAME_EXT_ToolbarLabel(TEXT("ToolbarLabel"));

static EUserInterfaceActionType GetExtensionActionType(UFunction* Function)
{
	if (Function->HasMetaData(NAME_EXT_ActionType))
	{
		const FString& ActionType = Function->GetMetaData(NAME_EXT_ActionType);
		if (ActionType == TEXT("None"))
			return EUserInterfaceActionType::None;
		if (ActionType == TEXT("Button"))
			return EUserInterfaceActionType::Button;
		if (ActionType == TEXT("ToggleButton"))
			return EUserInterfaceActionType::ToggleButton;
		if (ActionType == TEXT("RadioButton"))
			return EUserInterfaceActionType::RadioButton;
		if (ActionType == TEXT("Check"))
			return EUserInterfaceActionType::Check;
		if (ActionType == TEXT("CollapsedButton"))
			return EUserInterfaceActionType::CollapsedButton;
	}

	return EUserInterfaceActionType::Button;
}

void UScriptEditorMenuExtension::AddMenuEntry(class FMenuBuilder& MenuBuilder, UFunction* Function, FExtenderSelection Selection) const
{
	FString IconName = Function->GetMetaData(NAME_EXT_EditorIcon);
	if (IconName.IsEmpty())
		IconName = TEXT("GraphEditor.Event_16x");

	FText DisplayName = Function->GetDisplayNameText();
	if (Function->HasMetaData(NAME_EXT_ToolbarLabel))
	{
		DisplayName = FText::FromString(Function->GetMetaData(NAME_EXT_ToolbarLabel));
	}

	MenuBuilder.AddMenuEntry(
		DisplayName,
		Function->GetToolTipText(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), FName(*IconName)),
		CreateUIAction(Function, Selection)
	);
}

void UScriptEditorMenuExtension::AddToolMenuEntry(FToolMenuSection& MenuSection, UFunction* Function, FExtenderSelection Selection) const
{
	FString IconName = Function->GetMetaData(NAME_EXT_EditorIcon);
	if (IconName.IsEmpty())
		IconName = TEXT("GraphEditor.Event_16x");

	FText DisplayName = Function->GetDisplayNameText();
	if (Function->HasMetaData(NAME_EXT_ToolbarLabel))
	{
		DisplayName = FText::FromString(Function->GetMetaData(NAME_EXT_ToolbarLabel));
	}

	MenuSection.AddMenuEntry(
		Function->GetFName(),
		DisplayName,
		Function->GetToolTipText(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), FName(*IconName)),
		CreateToolUIAction(Function, Selection),
		GetExtensionActionType(Function)
	);
}

void UScriptEditorMenuExtension::AddToolMenuButton(FToolMenuSection& MenuSection, UFunction* Function, FExtenderSelection Selection) const
{
	FString IconName = Function->GetMetaData(NAME_EXT_EditorIcon);
	if (IconName.IsEmpty())
		IconName = TEXT("GraphEditor.Event_16x");

	FText DisplayName = Function->GetDisplayNameText();
	if (Function->HasMetaData(NAME_EXT_ToolbarLabel))
	{
		DisplayName = FText::FromString(Function->GetMetaData(NAME_EXT_ToolbarLabel));
	}

	auto Entry = FToolMenuEntry::InitToolBarButton(
		Function->GetFName(),
		CreateToolUIAction(Function, Selection),
		DisplayName,
		Function->GetToolTipText(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), FName(*IconName)),
		GetExtensionActionType(Function)
	);

	FString StyleName = Function->GetMetaData(NAME_EXT_EditorButtonStyle);
	if (!StyleName.IsEmpty())
		Entry.StyleNameOverride = *StyleName;

	MenuSection.AddEntry(Entry);
}

FToolUIAction UScriptEditorMenuExtension::CreateToolUIAction(UFunction* Function, FExtenderSelection Selection) const
{
	FToolUIAction Action;
	Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([Extension = this, Function, Selection](const FToolMenuContext& Context)
	{
		if (IsShiftModifierDown())
		{
			const bool bFoundFunction = NavigateToFunctionForMenuExtension(Function);
			if (!bFoundFunction)
			{
				NavigateToClassForMenuExtension(Function->GetOuterUClass());
			}
		}
		else
		{
			UScriptEditorMenuExtension* TempObject = NewObject<UScriptEditorMenuExtension>(GetTransientPackage(), Extension->GetClass());
			TempObject->AddToRoot();

			TGuardValue<const FToolMenuContext*> ScopeActiveContext(TempObject->ActiveToolMenuContext, &Context);

			FEditorScriptExecutionGuard ScriptGuard;
			TGuardValue<FExtenderSelection> ScopeSelection(TempObject->CurrentSelection, Selection);
			FScopedTransaction Transaction(FText::FromString(Function->GetDisplayNameText().ToString()));

			TempObject->CallFunctionOnSelection(Function, Selection);
			TempObject->RemoveFromRoot();
		}
	});

	FString CanExecute = Function->GetMetaData(NAME_EXT_ActionCanExecute);
	if (!CanExecute.IsEmpty())
	{
		Action.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda(
			[Extension = this, Selection, FuncName = FName(CanExecute)](const FToolMenuContext& Context) -> bool
			{
				FEditorScriptExecutionGuard ScriptGuard;
				TGuardValue<const FToolMenuContext*> ScopeActiveContext(Extension->ActiveToolMenuContext, &Context);
				TGuardValue<FExtenderSelection> ScopeSelection(Extension->CurrentSelection, Selection);

				UFunction* ExecFunction = Extension->FindFunction(FuncName);
				if (ExecFunction == nullptr || ExecFunction->ReturnValueOffset != 0 || CastField<FBoolProperty>(ExecFunction->GetReturnProperty()) == nullptr)
				{
					return false;
				}

				bool bReturnValue = false;
				((UObject*)Extension)->ProcessEvent(ExecFunction, &bReturnValue);
				return bReturnValue;
			});
	}

	FString IsVisible = Function->GetMetaData(NAME_EXT_ActionIsVisible);
	if (!IsVisible.IsEmpty())
	{
		Action.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateLambda(
			[Extension = this, Selection, FuncName = FName(IsVisible)](const FToolMenuContext& Context) -> bool
			{
				FEditorScriptExecutionGuard ScriptGuard;
				TGuardValue<const FToolMenuContext*> ScopeActiveContext(Extension->ActiveToolMenuContext, &Context);
				TGuardValue<FExtenderSelection> ScopeSelection(Extension->CurrentSelection, Selection);

				UFunction* ExecFunction = Extension->FindFunction(FuncName);
				if (ExecFunction == nullptr || ExecFunction->ReturnValueOffset != 0 || CastField<FBoolProperty>(ExecFunction->GetReturnProperty()) == nullptr)
				{
					return false;
				}

				bool bReturnValue = false;
				((UObject*)Extension)->ProcessEvent(ExecFunction, &bReturnValue);
				return bReturnValue;
			});
	}

	FString IsChecked = Function->GetMetaData(NAME_EXT_ActionIsChecked);
	if (!IsChecked.IsEmpty())
	{
		Action.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
			[Extension = this, Selection, FuncName = FName(IsChecked)](const FToolMenuContext& Context) -> ECheckBoxState
			{
				FEditorScriptExecutionGuard ScriptGuard;
				TGuardValue<const FToolMenuContext*> ScopeActiveContext(Extension->ActiveToolMenuContext, &Context);
				TGuardValue<FExtenderSelection> ScopeSelection(Extension->CurrentSelection, Selection);

				UFunction* ExecFunction = Extension->FindFunction(FuncName);
				if (ExecFunction == nullptr || ExecFunction->ReturnValueOffset != 0 || CastField<FBoolProperty>(ExecFunction->GetReturnProperty()) == nullptr)
				{
					return ECheckBoxState::Unchecked;
				}

				bool bReturnValue = false;
				((UObject*)Extension)->ProcessEvent(ExecFunction, &bReturnValue);
				return bReturnValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			});
	}

	return Action;
}

FUIAction UScriptEditorMenuExtension::CreateUIAction(UFunction* Function, FExtenderSelection Selection) const
{
	FUIAction Action;
	Action.ExecuteAction = FExecuteAction::CreateLambda([Extension = this, Function, Selection]()
	{
		if (IsShiftModifierDown())
		{
			const bool bFoundFunction = NavigateToFunctionForMenuExtension(Function);
			if (!bFoundFunction)
			{
				NavigateToClassForMenuExtension(Function->GetOuterUClass());
			}
		}
		else
		{
			UScriptEditorMenuExtension* TempObject = NewObject<UScriptEditorMenuExtension>(GetTransientPackage(), Extension->GetClass());
			TempObject->AddToRoot();

			FEditorScriptExecutionGuard ScriptGuard;
			TGuardValue<FExtenderSelection> ScopeSelection(TempObject->CurrentSelection, Selection);
			FScopedTransaction Transaction(FText::FromString(Function->GetDisplayNameText().ToString()));

			TempObject->CallFunctionOnSelection(Function, Selection);
			TempObject->RemoveFromRoot();
		}
	});

	FString CanExecute = Function->GetMetaData(NAME_EXT_ActionCanExecute);
	if (!CanExecute.IsEmpty())
	{
		Action.CanExecuteAction = FCanExecuteAction::CreateLambda(
			[Extension = this, Selection, FuncName = FName(CanExecute)]() -> bool
			{
				FEditorScriptExecutionGuard ScriptGuard;
				TGuardValue<FExtenderSelection> ScopeSelection(Extension->CurrentSelection, Selection);

				UFunction* ExecFunction = Extension->FindFunction(FuncName);
				if (ExecFunction == nullptr || ExecFunction->ReturnValueOffset != 0 || CastField<FBoolProperty>(ExecFunction->GetReturnProperty()) == nullptr)
				{
					return false;
				}

				bool bReturnValue = false;
				((UObject*)Extension)->ProcessEvent(ExecFunction, &bReturnValue);
				return bReturnValue;
			});
	}

	FString IsVisible = Function->GetMetaData(NAME_EXT_ActionIsVisible);
	if (!IsVisible.IsEmpty())
	{
		Action.IsActionVisibleDelegate = FIsActionButtonVisible::CreateLambda(
			[Extension = this, Selection, FuncName = FName(IsVisible)]() -> bool
			{
				FEditorScriptExecutionGuard ScriptGuard;
				TGuardValue<FExtenderSelection> ScopeSelection(Extension->CurrentSelection, Selection);

				UFunction* ExecFunction = Extension->FindFunction(FuncName);
				if (ExecFunction == nullptr || ExecFunction->ReturnValueOffset != 0 || CastField<FBoolProperty>(ExecFunction->GetReturnProperty()) == nullptr)
				{
					return false;
				}

				bool bReturnValue = false;
				((UObject*)Extension)->ProcessEvent(ExecFunction, &bReturnValue);
				return bReturnValue;
			});
	}

	FString IsChecked = Function->GetMetaData(NAME_EXT_ActionIsChecked);
	if (!IsChecked.IsEmpty())
	{
		Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
			[Extension = this, Selection, FuncName = FName(IsChecked)]() -> ECheckBoxState
			{
				FEditorScriptExecutionGuard ScriptGuard;
				TGuardValue<FExtenderSelection> ScopeSelection(Extension->CurrentSelection, Selection);

				UFunction* ExecFunction = Extension->FindFunction(FuncName);
				if (ExecFunction == nullptr || ExecFunction->ReturnValueOffset != 0 || CastField<FBoolProperty>(ExecFunction->GetReturnProperty()) == nullptr)
				{
					return ECheckBoxState::Unchecked;
				}

				bool bReturnValue = false;
				((UObject*)Extension)->ProcessEvent(ExecFunction, &bReturnValue);
				return bReturnValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			});
	}

	return Action;
}

#if WITH_DEV_AUTOMATION_TESTS
EUserInterfaceActionType FAngelscriptEditorMenuExtensionTestAccess::GetActionType(UFunction* Function)
{
	return GetExtensionActionType(Function);
}

void FAngelscriptEditorMenuExtensionTestAccess::SetShiftModifierOverride(TFunction<bool()> InOverride)
{
	GShiftModifierOverrideForTesting = MoveTemp(InOverride);
}

void FAngelscriptEditorMenuExtensionTestAccess::ResetShiftModifierOverride()
{
	GShiftModifierOverrideForTesting = nullptr;
}

void FAngelscriptEditorMenuExtensionTestAccess::SetNavigateToFunctionOverride(TFunction<bool(const UFunction*)> InOverride)
{
	GNavigateToFunctionOverrideForTesting = MoveTemp(InOverride);
}

void FAngelscriptEditorMenuExtensionTestAccess::ResetNavigateToFunctionOverride()
{
	GNavigateToFunctionOverrideForTesting = nullptr;
}

void FAngelscriptEditorMenuExtensionTestAccess::SetNavigateToClassOverride(TFunction<void(const UClass*)> InOverride)
{
	GNavigateToClassOverrideForTesting = MoveTemp(InOverride);
}

void FAngelscriptEditorMenuExtensionTestAccess::ResetNavigateToClassOverride()
{
	GNavigateToClassOverrideForTesting = nullptr;
}
#endif

struct FSortedFunction
{
	UFunction* Function;
	int SortOrder;

	bool operator<(const FSortedFunction& Other) const
	{
		if (SortOrder < Other.SortOrder)
			return true;
		if (SortOrder > Other.SortOrder)
			return false;
		return Function->GetName() < Other.Function->GetName();
	}
};

struct FFunctionCategory
{
	FString Name;
	TArray<FSortedFunction> Functions;
	TArray<FFunctionCategory> SubCategories;

	bool operator<(const FFunctionCategory& Other) const
	{
		return Name < Other.Name;
	}

	void Sort()
	{
		Functions.Sort();
		SubCategories.Sort();
		for (auto& Category : SubCategories)
			Category.Sort();
	}
};

static TMap<FString, FFunctionCategory> SortFunctionsByCategory(TArray<UFunction*> Functions)
{
	const static FName NAME_Category(TEXT("Category"));
	const static FName NAME_SortOrder(TEXT("SortOrder"));
	TMap<FString, FFunctionCategory> FunctionCategories;

	for (UFunction* Function : Functions)
	{
		FString CategoryString = Function->GetMetaData(NAME_Category);

		TArray<FString> CategoryElements;
		CategoryString.ParseIntoArray(CategoryElements, TEXT("|"), true);

		if (CategoryElements.Num() == 0)
			CategoryElements.Add(TEXT(""));

		FSortedFunction SortFun;
		SortFun.Function = Function;

		FString SortOrder = Function->GetMetaData(NAME_SortOrder);
		if (!SortOrder.IsEmpty())
			LexFromString(SortFun.SortOrder, *SortOrder);
		else
			SortFun.SortOrder = 0;

		FFunctionCategory* CheckCategory = nullptr;
		for (int i = 0, Count = CategoryElements.Num(); i < Count; ++i)
		{
			FString ElementName = CategoryElements[i].TrimStartAndEnd();

			if (CheckCategory == nullptr)
			{
				CheckCategory = &FunctionCategories.FindOrAdd(ElementName);
				CheckCategory->Name = ElementName;
			}
			else
			{
				auto* SubCategory = CheckCategory->SubCategories.FindByPredicate([&](FFunctionCategory& Cat)
				{
					return Cat.Name == ElementName;
				});

				if (SubCategory == nullptr)
				{
					SubCategory = &CheckCategory->SubCategories.Emplace_GetRef();
					SubCategory->Name = ElementName;
				}

				CheckCategory = SubCategory;
			}
		}

		if (CheckCategory != nullptr)
			CheckCategory->Functions.Add(SortFun);
	}

	for (auto& Category : FunctionCategories)
		Category.Value.Sort();
	return FunctionCategories;
}

void UScriptEditorMenuExtension::BuildMenu(class FMenuBuilder& MenuBuilder, TArray<UFunction*> Functions, FExtenderSelection Selection) const
{
	if (bAddSeparatorBeforeOptions)
		MenuBuilder.AddSeparator();
	if (!MenuSectionHeader.IsEmpty())
		MenuBuilder.BeginSection(GetClass()->GetFName(), MenuSectionHeader);

	TMap<FString, FFunctionCategory> FunctionCategories = SortFunctionsByCategory(Functions);

	auto AddEntriesWithCategory = [&](FMenuBuilder& SubMenu, const FFunctionCategory& Category)
	{
		// Add entries for functions with no category
		for (auto FunctionElem : Category.Functions)
			AddMenuEntry(SubMenu, FunctionElem.Function, Selection);
	};

	struct FSubMenuHelper
	{
		static void AddSubMenuForCategory(const UScriptEditorMenuExtension* Extension, FExtenderSelection Selection, FMenuBuilder& SubMenu, const FFunctionCategory& Category)
		{
			SubMenu.AddSubMenu(
				FText::FromString(Category.Name),
				FText(),
				FNewMenuDelegate::CreateLambda([Extension, Category, Selection](FMenuBuilder& SubSubMenu)
				{
					for (auto FunctionElem : Category.Functions)
						Extension->AddMenuEntry(SubSubMenu, FunctionElem.Function, Selection);

					for (const FFunctionCategory& SubCategoryElem : Category.SubCategories)
						AddSubMenuForCategory(Extension, Selection, SubSubMenu, SubCategoryElem);
				}),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x")
			);
		}
	};

	// Add submenus for categories
	for (auto& Category : FunctionCategories)
	{
		if (Category.Key == TEXT(""))
			AddEntriesWithCategory(MenuBuilder, Category.Value);
		else
			FSubMenuHelper::AddSubMenuForCategory(this, Selection, MenuBuilder, Category.Value);
	}

	if (!MenuSectionHeader.IsEmpty())
		MenuBuilder.EndSection();
	if (bAddSeparatorAfterOptions)
		MenuBuilder.AddSeparator();
}

void UScriptEditorMenuExtension::BuildToolMenuSection(FToolMenuSection& MenuSection, FExtenderSelection Selection, bool bIsMenu) const
{
	FEditorScriptExecutionGuard ScriptGuard;
	TGuardValue<FExtenderSelection> ScopeSelection(CurrentSelection, Selection);

	{
		TGuardValue<const FToolMenuContext*> ScopeActiveContext(ActiveToolMenuContext, &MenuSection.Context);
		if (!ShouldExtend())
			return;
	}

	TArray<UFunction*> Functions = GatherExtensionFunctions();
	if (Functions.Num() == 0)
		return;

	TMap<FString, FFunctionCategory> FunctionCategories = SortFunctionsByCategory(Functions);

	if (bAddSeparatorBeforeOptions)
		MenuSection.AddSeparator(NAME_None);

	auto AddEntriesWithCategory = [&](FToolMenuSection& SubMenu, const FFunctionCategory& Category)
	{
		// Add entries for functions with no category
		for (auto FunctionElem : Category.Functions)
		{
			if (bIsMenu)
				AddToolMenuEntry(SubMenu, FunctionElem.Function, Selection);
			else
				AddToolMenuButton(SubMenu, FunctionElem.Function, Selection);
		}
	};

	struct FSubMenuHelper
	{
		static void AddSubMenuForCategory(const UScriptEditorMenuExtension* Extension, FExtenderSelection Selection, FToolMenuSection& SubMenu, const FFunctionCategory& Category)
		{
			SubMenu.AddSubMenu(
				*Category.Name,
				FText::FromString(Category.Name),
				FText(),
				FNewToolMenuDelegate::CreateLambda([Extension, Category, Selection](UToolMenu* SubSubMenu)
				{
					FToolMenuSection& SubSection = SubSubMenu->AddSection(NAME_None, FText());

					for (auto FunctionElem : Category.Functions)
						Extension->AddToolMenuEntry(SubSection, FunctionElem.Function, Selection);

					for (const FFunctionCategory& SubCategoryElem : Category.SubCategories)
						AddSubMenuForCategory(Extension, Selection, SubSection, SubCategoryElem);
				}),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x")
			);
		}
	};

	// Add submenus for categories
	for (auto& Category : FunctionCategories)
	{
		if (Category.Key == TEXT(""))
		{
			AddEntriesWithCategory(MenuSection, Category.Value);
		}
		else
		{
			if (bIsMenu)
			{
				FSubMenuHelper::AddSubMenuForCategory(this, Selection, MenuSection, Category.Value);
			}
			else
			{
				MenuSection.AddEntry(FToolMenuEntry::InitComboButton(
					*Category.Value.Name,
					FUIAction(),
					FNewToolMenuDelegate::CreateLambda([Extension=this, Category=Category.Value, Selection](UToolMenu* SubSubMenu)
					{
						FToolMenuSection& SubSection = SubSubMenu->AddSection(NAME_None, FText());

						for (auto FunctionElem : Category.Functions)
							Extension->AddToolMenuEntry(SubSection, FunctionElem.Function, Selection);

						for (const FFunctionCategory& SubCategoryElem : Category.SubCategories)
							FSubMenuHelper::AddSubMenuForCategory(Extension, Selection, SubSection, SubCategoryElem);
					}),
					FText::FromString(Category.Value.Name),
					FText(),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x"),
					false
				));
			}
		}
	}

	if (bAddSeparatorAfterOptions)
		MenuSection.AddSeparator(NAME_None);
}

