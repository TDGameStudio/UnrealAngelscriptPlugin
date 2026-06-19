#include "CQTest.h"
#include "Tests/AngelscriptEditorMenuExtensionsTestTypes.h"

#include "EditorMenuExtensions/ScriptEditorMenuExtension.h"

#include "Framework/MultiBox/MultiBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorMenuExtensionCategoryTests_Private
{
	FString GetEntryLabel(const FToolMenuEntry& Entry)
	{
		return Entry.Label.Get().ToString();
	}

	const FToolMenuEntry* FindEntryByName(const FToolMenuSection& Section, const FName EntryName)
	{
		return Section.Blocks.FindByPredicate([EntryName](const FToolMenuEntry& Entry)
		{
			return Entry.Name == EntryName;
		});
	}

	UToolMenu* RegisterTemporaryMenu(FAutomationTestBase& Test, TArray<FName>& RegisteredMenus, const TCHAR* Prefix, EMultiBoxType Type)
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (!Test.TestNotNull(TEXT("MenuExtensions category test should resolve UToolMenus"), ToolMenus))
		{
			return nullptr;
		}

		const FName MenuName(*FString::Printf(TEXT("Automation.%s.%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		UToolMenu* Menu = ToolMenus->RegisterMenu(MenuName, NAME_None, Type, false);
		if (!Test.TestNotNull(TEXT("MenuExtensions category test should register a temporary tool menu"), Menu))
		{
			return nullptr;
		}

		RegisteredMenus.Add(MenuName);
		return Menu;
	}

	const FToolMenuSection* GetOnlySection(FAutomationTestBase& Test, const UToolMenu& Menu, const TCHAR* Context)
	{
		if (!Test.TestEqual(Context, Menu.Sections.Num(), 1))
		{
			return nullptr;
		}

		return &Menu.Sections[0];
	}

	UToolMenu* MaterializeGeneratedSubMenu(
		FAutomationTestBase& Test,
		TArray<FName>& RegisteredMenus,
		const FToolMenuEntry& Entry,
		const TCHAR* Prefix)
	{
		FNewToolMenuChoice Generator;
		if (Entry.IsSubMenu())
		{
			Generator = Entry.SubMenuData.ConstructMenu;
		}
		else if (Entry.Type == EMultiBlockType::ToolBarComboButton)
		{
			Generator = Entry.ToolBarData.ComboButtonContextMenuGenerator;
		}
		else
		{
			Test.AddError(FString::Printf(TEXT("Entry '%s' does not expose a generated submenu"), *Entry.Name.ToString()));
			return nullptr;
		}

		if (!Test.TestTrue(
			FString::Printf(TEXT("Entry '%s' should expose a NewToolMenu generator"), *Entry.Name.ToString()),
			Generator.NewToolMenu.IsBound()))
		{
			return nullptr;
		}

		UToolMenu* SubMenu = RegisterTemporaryMenu(Test, RegisteredMenus, Prefix, EMultiBoxType::Menu);
		if (SubMenu == nullptr)
		{
			return nullptr;
		}

		Generator.NewToolMenu.Execute(SubMenu);
		return SubMenu;
	}

	bool VerifyToolsSubMenuHierarchy(
		FAutomationTestBase& Test,
		TArray<FName>& RegisteredMenus,
		const FToolMenuEntry& ToolsEntry,
		const TCHAR* Prefix)
	{
		UToolMenu* ToolsMenu = MaterializeGeneratedSubMenu(Test, RegisteredMenus, ToolsEntry, Prefix);
		if (ToolsMenu == nullptr)
		{
			return false;
		}

		const FToolMenuSection* ToolsSection = GetOnlySection(
			Test,
			*ToolsMenu,
			TEXT("MenuExtensions category test should create exactly one section inside the Tools submenu"));
		if (ToolsSection == nullptr)
		{
			return false;
		}

		if (!Test.TestEqual(TEXT("Tools submenu should contain Audit and Bake child categories"), ToolsSection->Blocks.Num(), 2))
		{
			return false;
		}

		const FToolMenuEntry& AuditEntry = ToolsSection->Blocks[0];
		const FToolMenuEntry& BakeEntry = ToolsSection->Blocks[1];
		Test.TestEqual(TEXT("Tools submenu should sort child categories alphabetically"), AuditEntry.Name, FName(TEXT("Audit")));
		Test.TestEqual(TEXT("Tools submenu should keep Bake after Audit"), BakeEntry.Name, FName(TEXT("Bake")));
		Test.TestTrue(TEXT("Audit child category should materialize as a submenu"), AuditEntry.IsSubMenu());
		Test.TestTrue(TEXT("Bake child category should materialize as a submenu"), BakeEntry.IsSubMenu());

		UToolMenu* AuditMenu = MaterializeGeneratedSubMenu(Test, RegisteredMenus, AuditEntry, TEXT("MenuExtensionsAudit"));
		if (AuditMenu == nullptr)
		{
			return false;
		}

		const FToolMenuSection* AuditSection = GetOnlySection(
			Test,
			*AuditMenu,
			TEXT("MenuExtensions category test should create exactly one section inside the Audit submenu"));
		if (AuditSection == nullptr)
		{
			return false;
		}

		if (!Test.TestEqual(TEXT("Audit submenu should expose exactly one command"), AuditSection->Blocks.Num(), 1))
		{
			return false;
		}

		Test.TestEqual(TEXT("Audit submenu should keep the audit command inside the child category"), AuditSection->Blocks[0].Name, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionCategoryTestShim, AuditCommand));

		UToolMenu* BakeMenu = MaterializeGeneratedSubMenu(Test, RegisteredMenus, BakeEntry, TEXT("MenuExtensionsBake"));
		if (BakeMenu == nullptr)
		{
			return false;
		}

		const FToolMenuSection* BakeSection = GetOnlySection(
			Test,
			*BakeMenu,
			TEXT("MenuExtensions category test should create exactly one section inside the Bake submenu"));
		if (BakeSection == nullptr)
		{
			return false;
		}

		if (!Test.TestEqual(TEXT("Bake submenu should expose both bake commands"), BakeSection->Blocks.Num(), 2))
		{
			return false;
		}

		Test.TestEqual(TEXT("Bake submenu should sort commands by SortOrder before function name"), BakeSection->Blocks[0].Name, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionCategoryTestShim, BakeSoonerCommand));
		Test.TestEqual(TEXT("Bake submenu should keep the later bake command after the earlier one"), BakeSection->Blocks[1].Name, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionCategoryTestShim, BakeLaterCommand));
		return true;
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunBuildMenuAndToolMenuSectionRespectCategoryHierarchyAndSortOrder(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorMenuExtensionCategoryTests_Private;
	UAngelscriptEditorMenuExtensionCategoryTestShim* Extension = NewObject<UAngelscriptEditorMenuExtensionCategoryTestShim>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions category test should create a category-order extension shim"), Extension))
	{
		return false;
	}

	FMenuBuilder MenuBuilder(
		true,
		TSharedPtr<const FUICommandList>(),
		TSharedPtr<FExtender>(),
		false,
		&FCoreStyle::Get(),
		false);
	FAngelscriptEditorMenuExtensionTestAccess::BuildMenu(Extension, MenuBuilder);

	const TArray<TSharedRef<const FMultiBlock>>& MenuBlocks = MenuBuilder.GetMultiBox()->GetBlocks();
	TestEqual(TEXT("BuildMenu should emit one top-level command plus one top-level Tools submenu"), MenuBlocks.Num(), 2);
	if (MenuBlocks.Num() >= 2)
	{
		TestEqual(TEXT("BuildMenu should emit the categoryless function as a menu entry"), MenuBlocks[0]->GetType(), EMultiBlockType::MenuEntry);
		TestEqual(TEXT("BuildMenu should emit the categorized branch as a submenu block"), MenuBlocks[1]->GetType(), EMultiBlockType::MenuEntry);
	}

	TArray<FName> RegisteredMenus;
	UToolMenus* ToolMenus = UToolMenus::Get();
	ON_SCOPE_EXIT
	{
		if (ToolMenus != nullptr)
		{
			for (const FName MenuName : RegisteredMenus)
			{
				ToolMenus->RemoveMenu(MenuName);
			}
		}
	};

	UToolMenu* MenuModeMenu = RegisterTemporaryMenu(Test, RegisteredMenus, TEXT("MenuExtensionsCategoryMenu"), EMultiBoxType::Menu);
	if (MenuModeMenu == nullptr)
	{
		return false;
	}

	FToolMenuSection& MenuModeSection = MenuModeMenu->AddSection(TEXT("Automation"));
	FAngelscriptEditorMenuExtensionTestAccess::BuildToolMenuSection(Extension, MenuModeSection, true);

	if (!TestEqual(TEXT("Menu-mode tool section should contain the categoryless command and the top-level Tools submenu"), MenuModeSection.Blocks.Num(), 2))
	{
		return false;
	}

	const FToolMenuEntry* TopLevelMenuEntry = FindEntryByName(MenuModeSection, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionCategoryTestShim, TopLevelCommand));
	const FToolMenuEntry* ToolsMenuEntry = FindEntryByName(MenuModeSection, FName(TEXT("Tools")));
	if (!TestNotNull(TEXT("Menu-mode tool section should expose the categoryless command at top level"), TopLevelMenuEntry)
		|| !TestNotNull(TEXT("Menu-mode tool section should expose the top-level Tools submenu"), ToolsMenuEntry))
	{
		return false;
	}

	TestFalse(TEXT("Categoryless command should stay a direct menu entry"), TopLevelMenuEntry->IsSubMenu());
	TestTrue(TEXT("Top-level Tools category should materialize as a submenu in menu mode"), ToolsMenuEntry->IsSubMenu());
	TestEqual(TEXT("Categoryless command should keep its display label"), GetEntryLabel(*TopLevelMenuEntry), FString(TEXT("Top Level Command")));
	TestEqual(TEXT("Top-level Tools submenu should keep its category label"), GetEntryLabel(*ToolsMenuEntry), FString(TEXT("Tools")));

	if (!VerifyToolsSubMenuHierarchy(Test, RegisteredMenus, *ToolsMenuEntry, TEXT("MenuExtensionsMenuModeTools")))
	{
		return false;
	}

	UToolMenu* ToolbarMenu = RegisterTemporaryMenu(Test, RegisteredMenus, TEXT("MenuExtensionsCategoryToolbar"), EMultiBoxType::ToolBar);
	if (ToolbarMenu == nullptr)
	{
		return false;
	}

	FToolMenuSection& ToolbarSection = ToolbarMenu->AddSection(TEXT("Automation"));
	FAngelscriptEditorMenuExtensionTestAccess::BuildToolMenuSection(Extension, ToolbarSection, false);

	if (!TestEqual(TEXT("Toolbar-mode tool section should contain the categoryless button and the Tools combo button"), ToolbarSection.Blocks.Num(), 2))
	{
		return false;
	}

	const FToolMenuEntry* TopLevelToolbarEntry = FindEntryByName(ToolbarSection, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionCategoryTestShim, TopLevelCommand));
	const FToolMenuEntry* ToolsToolbarEntry = FindEntryByName(ToolbarSection, FName(TEXT("Tools")));
	if (!TestNotNull(TEXT("Toolbar mode should expose the categoryless command at top level"), TopLevelToolbarEntry)
		|| !TestNotNull(TEXT("Toolbar mode should expose the top-level Tools combo button"), ToolsToolbarEntry))
	{
		return false;
	}

	TestEqual(TEXT("Categoryless toolbar block should be emitted as a toolbar button"), TopLevelToolbarEntry->Type, EMultiBlockType::ToolBarButton);
	TestEqual(TEXT("Categorized toolbar block should be emitted as a combo button"), ToolsToolbarEntry->Type, EMultiBlockType::ToolBarComboButton);
	TestTrue(
		TEXT("Toolbar Tools combo button should expose a context-menu generator"),
		ToolsToolbarEntry->ToolBarData.ComboButtonContextMenuGenerator.NewToolMenu.IsBound());

	if (!VerifyToolsSubMenuHierarchy(Test, RegisteredMenus, *ToolsToolbarEntry, TEXT("MenuExtensionsToolbarModeTools")))
	{
		return false;
	}

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorMenuExtensionCategoryTests,
	"Angelscript.Editor.MenuExtensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BuildMenuAndToolMenuSectionRespectCategoryHierarchyAndSortOrder)
	{
		ASSERT_THAT(IsTrue(RunBuildMenuAndToolMenuSectionRespectCategoryHierarchyAndSortOrder(*TestRunner)));
	}
};

#endif
