#include "Core/AngelscriptEditorModule.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuOwner.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptEditorModuleRegisterToolsMenuEntriesTest,
	"Angelscript.Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceSnippetAndLegacyBindCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleMenuTests_Private
{
	struct FPlatformExecuteCall
	{
		FString CommandType;
		FString Command;
		FString CommandLine;
	};

	const FToolMenuEntry* FindOwnedEntry(const FToolMenuSection& Section, const FName EntryName, const FToolMenuOwner Owner)
	{
		return Section.Blocks.FindByPredicate([EntryName, Owner](const FToolMenuEntry& Entry)
		{
			return Entry.Name == EntryName && Entry.Owner == Owner;
		});
	}

	int32 CountOwnedEntries(const FToolMenuSection& Section, const FToolMenuOwner Owner)
	{
		int32 Count = 0;
		for (const FToolMenuEntry& Entry : Section.Blocks)
		{
			if (Entry.Owner == Owner)
			{
				++Count;
			}
		}

		return Count;
	}
}


bool FAngelscriptEditorModuleRegisterToolsMenuEntriesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleMenuTests_Private;
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!TestNotNull(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should resolve tool menus"), ToolMenus))
	{
		return false;
	}

	UToolMenu* ToolsMenu = ToolMenus->ExtendMenu("MainFrame.MainMenu.Tools");
	if (!TestNotNull(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should resolve the main Tools menu"), ToolsMenu))
	{
		return false;
	}

	FAngelscriptEditorModule Module;
	const FToolMenuOwner ModuleOwner(&Module);
	TArray<FPlatformExecuteCall> PlatformExecuteCalls;
	bool bSnippetRunnerOpened = false;

	FAngelscriptEditorModuleTestAccess::ResetPlatformExecuteOverride();
	FAngelscriptEditorModuleTestAccess::ResetSnippetRunnerOpenOverride();
	UToolMenus::UnregisterOwner(&Module);

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetPlatformExecuteOverride();
		FAngelscriptEditorModuleTestAccess::ResetSnippetRunnerOpenOverride();
		UToolMenus::UnregisterOwner(&Module);
	};

	FAngelscriptEditorModuleTestAccess::SetPlatformExecuteOverride(
		[&PlatformExecuteCalls](const TCHAR* CommandType, const TCHAR* Command, const TCHAR* CommandLine)
		{
			FPlatformExecuteCall& Call = PlatformExecuteCalls.AddDefaulted_GetRef();
			Call.CommandType = CommandType != nullptr ? CommandType : TEXT("");
			Call.Command = Command != nullptr ? Command : TEXT("");
			Call.CommandLine = CommandLine != nullptr ? CommandLine : TEXT("");
			return true;
		});
	FAngelscriptEditorModuleTestAccess::SetSnippetRunnerOpenOverride(
		[&bSnippetRunnerOpened]()
		{
			bSnippetRunnerOpened = true;
		});

	FAngelscriptEditorModuleTestAccess::RegisterToolsMenuEntries(Module);

	FToolMenuSection* ProgrammingSection = ToolsMenu->FindSection("Programming");
	FToolMenuSection* ProgrammingBindsSection = ToolsMenu->FindSection("Programming Binds");
	if (!TestNotNull(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should create the Programming section"), ProgrammingSection)
		|| !TestNotNull(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should create the Programming Binds section"), ProgrammingBindsSection))
	{
		return false;
	}

	const FToolMenuEntry* OpenCodeEntry = FindOwnedEntry(*ProgrammingSection, "ASOpenCode", ModuleOwner);
	const FToolMenuEntry* RunSnippetEntry = FindOwnedEntry(*ProgrammingSection, "ASRunSnippet", ModuleOwner);
	const FToolMenuEntry* FunctionTestsEntry = FindOwnedEntry(*ProgrammingSection, "Function Tests", ModuleOwner);
	const FToolMenuEntry* GenerateBindingsEntry = FindOwnedEntry(*ProgrammingBindsSection, "ASGenerateBindings", ModuleOwner);
	if (!TestNotNull(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should register ASOpenCode in the Programming section"), OpenCodeEntry)
		|| !TestNotNull(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should register ASRunSnippet in the Programming section"), RunSnippetEntry)
		|| !TestNotNull(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should register Function Tests in the Programming section"), FunctionTestsEntry)
		|| !TestNotNull(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should register ASGenerateBindings in the Programming Binds section"), GenerateBindingsEntry))
	{
		return false;
	}

	if (!TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should contribute exactly three entries to the Programming section"), CountOwnedEntries(*ProgrammingSection, ModuleOwner), 3)
		|| !TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should contribute exactly one entry to the Programming Binds section"), CountOwnedEntries(*ProgrammingBindsSection, ModuleOwner), 1))
	{
		return false;
	}

	if (!TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should preserve the workspace label"), OpenCodeEntry->Label.Get().ToString(), FString(TEXT("Open Angelscript workspace (VS Code)")))
		|| !TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should preserve the snippet runner label"), RunSnippetEntry->Label.Get().ToString(), FString(TEXT("Run Angelscript Snippet")))
		|| !TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should preserve the function-tests label"), FunctionTestsEntry->Label.Get().ToString(), FString(TEXT("Run Function Tests")))
		|| !TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should preserve the legacy-bind label"), GenerateBindingsEntry->Label.Get().ToString(), FString(TEXT("Legacy Native Bind Generator (Debug Only)"))))
	{
		return false;
	}

	FToolMenuEntry* MutableRunSnippetEntry = const_cast<FToolMenuEntry*>(RunSnippetEntry);
	if (!TestTrue(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should execute the ASRunSnippet action"), MutableRunSnippetEntry->TryExecuteToolUIAction(FToolMenuContext()))
		|| !TestTrue(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should open the snippet runner window"), bSnippetRunnerOpened))
	{
		return false;
	}

	FToolMenuEntry* MutableOpenCodeEntry = const_cast<FToolMenuEntry*>(OpenCodeEntry);
	const bool bExecuted = MutableOpenCodeEntry->TryExecuteToolUIAction(FToolMenuContext());
	if (!TestTrue(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should execute the ASOpenCode action"), bExecuted)
		|| !TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should call platform execute exactly once"), PlatformExecuteCalls.Num(), 1))
	{
		return false;
	}

	const FString ExpectedScriptCommandLine = FString::Printf(TEXT("\"%s\""), *(FPaths::ProjectDir() / TEXT("Script")));
	if (!TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should keep the command type empty"), PlatformExecuteCalls[0].CommandType, FString())
		|| !TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should launch VS Code"), PlatformExecuteCalls[0].Command, FString(TEXT("code")))
		|| !TestEqual(TEXT("Editor.Module.RegisterToolsMenuEntriesAddsWorkspaceAndLegacyBindCommands should target the project Script workspace"), PlatformExecuteCalls[0].CommandLine, ExpectedScriptCommandLine))
	{
		return false;
	}

	return true;
}

#endif
