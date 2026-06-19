#include "CQTest.h"
#include "Tests/AngelscriptEditorMenuExtensionsTestTypes.h"

#include "EditorMenuExtensions/ScriptEditorMenuExtension.h"

#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorMenuExtensionToolMenuContextTests_Private
{
	UToolMenu* RegisterTemporaryMenu(FAutomationTestBase& Test, TArray<FName>& RegisteredMenus, const TCHAR* Prefix)
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (!Test.TestNotNull(TEXT("MenuExtensions tool-menu context test should resolve UToolMenus"), ToolMenus))
		{
			return nullptr;
		}

		const FName MenuName(*FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		UToolMenu* Menu = ToolMenus->RegisterMenu(MenuName, NAME_None, EMultiBoxType::Menu, false);
		if (!Test.TestNotNull(TEXT("MenuExtensions tool-menu context test should register a temporary tool menu"), Menu))
		{
			return nullptr;
		}

		RegisteredMenus.Add(MenuName);
		return Menu;
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunGetToolMenuContextExposesActiveContextOnlyDuringToolMenuCallbacks(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorMenuExtensionToolMenuContextTests_Private;
	UAngelscriptEditorMenuExtensionContextTestShim* Extension = NewObject<UAngelscriptEditorMenuExtensionContextTestShim>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions tool-menu context test should create a context-aware extension shim"), Extension))
	{
		return false;
	}

	UFunction* CommandFunction = Extension->FindFunction(GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionContextTestShim, ContextAwareCommand));
	if (!TestNotNull(TEXT("MenuExtensions tool-menu context test should resolve the context-aware command"), CommandFunction))
	{
		return false;
	}

	UAngelscriptEditorMenuExtensionContextTestShim::ResetRecordedState();
	ON_SCOPE_EXIT
	{
		UAngelscriptEditorMenuExtensionContextTestShim::ResetRecordedState();
	};

	TestTrue(
		TEXT("MenuExtensions tool-menu context should stay null before any tool-menu callback"),
		Extension->GetToolMenuContext(UAngelscriptEditorMenuExtensionToolMenuContextObject::StaticClass()) == nullptr);

	TArray<FName> RegisteredMenus;
	ON_SCOPE_EXIT
	{
		if (UToolMenus* ToolMenus = UToolMenus::Get())
		{
			for (const FName MenuName : RegisteredMenus)
			{
				ToolMenus->RemoveMenu(MenuName);
			}
		}
	};

	UToolMenu* ToolMenu = RegisterTemporaryMenu(Test, RegisteredMenus, TEXT("MenuExtensionsContext"));
	if (ToolMenu == nullptr)
	{
		return false;
	}

	FToolMenuSection& Section = ToolMenu->AddSection(TEXT("Automation"));
	UAngelscriptEditorMenuExtensionToolMenuContextObject* ContextObject = NewObject<UAngelscriptEditorMenuExtensionToolMenuContextObject>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions tool-menu context test should create a context object"), ContextObject))
	{
		return false;
	}

	ContextObject->Marker = 77;
	Section.Context.AddObject(ContextObject);

	FAngelscriptEditorMenuExtensionTestAccess::BuildToolMenuSection(Extension, Section, true);
	if (!TestEqual(TEXT("MenuExtensions tool-menu context test should add one menu entry"), Section.Blocks.Num(), 1))
	{
		return false;
	}

	TestEqual(
		TEXT("MenuExtensions tool-menu context test should add the context-aware command entry"),
		Section.Blocks[0].Name,
		GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionContextTestShim, ContextAwareCommand));

	TestTrue(
		TEXT("MenuExtensions tool-menu context should not leak after tool-menu section build"),
		Extension->GetToolMenuContext(UAngelscriptEditorMenuExtensionToolMenuContextObject::StaticClass()) == nullptr);

	const FToolUIAction ToolAction = FAngelscriptEditorMenuExtensionTestAccess::CreateToolUIAction(Extension, CommandFunction);
	if (!TestTrue(TEXT("MenuExtensions tool-menu context test should bind a tool execute delegate"), ToolAction.ExecuteAction.IsBound())
		|| !TestTrue(TEXT("MenuExtensions tool-menu context test should bind a tool can-execute delegate"), ToolAction.CanExecuteAction.IsBound()))
	{
		return false;
	}

	TestTrue(
		TEXT("MenuExtensions tool actions should see the active tool-menu context during can-execute"),
		ToolAction.CanExecuteAction.Execute(Section.Context));
	TestEqual(
		TEXT("MenuExtensions tool can-execute callback should run exactly once"),
		UAngelscriptEditorMenuExtensionContextTestShim::GetCanExecuteInvocationCount(),
		1);
	TestTrue(
		TEXT("MenuExtensions tool can-execute callback should receive the inserted context object"),
		UAngelscriptEditorMenuExtensionContextTestShim::GetLastCanExecuteContextObject() == ContextObject);
	TestTrue(
		TEXT("MenuExtensions tool can-execute callback should return nullptr for unrelated context classes"),
		UAngelscriptEditorMenuExtensionContextTestShim::WasLastCanExecuteMissingContextNull());
	TestTrue(
		TEXT("MenuExtensions tool-menu context should reset after can-execute returns"),
		Extension->GetToolMenuContext(UAngelscriptEditorMenuExtensionToolMenuContextObject::StaticClass()) == nullptr);

	ToolAction.ExecuteAction.Execute(Section.Context);
	TestEqual(
		TEXT("MenuExtensions tool execute path should invoke the context-aware command exactly once"),
		UAngelscriptEditorMenuExtensionContextTestShim::GetCommandInvocationCount(),
		1);
	TestEqual(
		TEXT("MenuExtensions tool execute path should forward the selected function"),
		UAngelscriptEditorMenuExtensionContextTestShim::GetLastCommandFunctionName(),
		GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionContextTestShim, ContextAwareCommand));
	TestTrue(
		TEXT("MenuExtensions tool execute path should receive the inserted context object"),
		UAngelscriptEditorMenuExtensionContextTestShim::GetLastCommandContextObject() == ContextObject);
	TestTrue(
		TEXT("MenuExtensions tool execute path should return nullptr for unrelated context classes"),
		UAngelscriptEditorMenuExtensionContextTestShim::WasLastCommandMissingContextNull());
	TestTrue(
		TEXT("MenuExtensions tool-menu context should stay null outside the tool execute callback"),
		Extension->GetToolMenuContext(UAngelscriptEditorMenuExtensionToolMenuContextObject::StaticClass()) == nullptr);

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorMenuExtensionToolMenuContextTests,
	"Angelscript.Editor.MenuExtensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GetToolMenuContextExposesActiveContextOnlyDuringToolMenuCallbacks)
	{
		ASSERT_THAT(IsTrue(RunGetToolMenuContextExposesActiveContextOnlyDuringToolMenuCallbacks(*TestRunner)));
	}
};

#endif
