#include "CQTest.h"
#include "Core/AngelscriptEditorModule.h"
#include "AngelscriptCompileOptions.h"
#include "AngelscriptSettings.h"

#include "IDirectoryWatcher.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestNull(...) Test.TestNull(__VA_ARGS__)

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleSettingsTests_Private
{
	static const FName ProjectSettingsContainerName(TEXT("Project"));
	static const FName PluginsCategoryName(TEXT("Plugins"));
	static const FName AngelscriptSectionName(TEXT("Angelscript"));
	static const FName AngelscriptCompileOptionsSectionName(TEXT("AngelscriptCompileOptions"));

	class FMockDirectoryWatcher final : public IDirectoryWatcher
	{
	public:
		bool RegisterDirectoryChangedCallback_Handle(const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& OutHandle, uint32 Flags) override
		{
			OutHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
			return true;
		}

		bool UnregisterDirectoryChangedCallback_Handle(const FString& Directory, FDelegateHandle InHandle) override
		{
			return true;
		}
	};

	TSharedPtr<ISettingsCategory> FindAngelscriptPluginsCategory(ISettingsModule& SettingsModule)
	{
		const TSharedPtr<ISettingsContainer> ProjectContainer = SettingsModule.GetContainer(ProjectSettingsContainerName);
		if (!ProjectContainer.IsValid())
		{
			return nullptr;
		}

		return ProjectContainer->GetCategory(PluginsCategoryName);
	}

	ISettingsSectionPtr FindAngelscriptProjectSettingsSection(ISettingsModule& SettingsModule)
	{
		const TSharedPtr<ISettingsCategory> PluginsCategory = FindAngelscriptPluginsCategory(SettingsModule);
		if (!PluginsCategory.IsValid())
		{
			return nullptr;
		}

		return PluginsCategory->GetSection(AngelscriptSectionName, true);
	}

	ISettingsSectionPtr FindAngelscriptCompileOptionsSettingsSection(ISettingsModule& SettingsModule)
	{
		const TSharedPtr<ISettingsCategory> PluginsCategory = FindAngelscriptPluginsCategory(SettingsModule);
		if (!PluginsCategory.IsValid())
		{
			return nullptr;
		}

		return PluginsCategory->GetSection(AngelscriptCompileOptionsSectionName, true);
	}

	int32 CountAngelscriptProjectSettingsSections(ISettingsModule& SettingsModule)
	{
		const TSharedPtr<ISettingsCategory> PluginsCategory = FindAngelscriptPluginsCategory(SettingsModule);
		if (!PluginsCategory.IsValid())
		{
			return 0;
		}

		TArray<TSharedPtr<ISettingsSection>> Sections;
		PluginsCategory->GetSections(Sections, true);

		int32 MatchCount = 0;
		for (const TSharedPtr<ISettingsSection>& Section : Sections)
		{
			if (Section.IsValid() && Section->GetName() == AngelscriptSectionName)
			{
				++MatchCount;
			}
		}

		return MatchCount;
	}

	void RegisterAngelscriptProjectSettingsSection(ISettingsModule& SettingsModule)
	{
		SettingsModule.RegisterSettings(
			ProjectSettingsContainerName,
			PluginsCategoryName,
			AngelscriptSectionName,
			NSLOCTEXT("Angelscript", "AngelscriptSettingsTitle", "Angelscript"),
			NSLOCTEXT("Angelscript", "AngelscriptSettingsDescription", "Configuration for behavior of the angelscript compiler and script engine."),
			GetMutableDefault<UAngelscriptSettings>());
	}

	void UnregisterAngelscriptProjectSettingsSection(ISettingsModule& SettingsModule)
	{
		SettingsModule.UnregisterSettings(
			ProjectSettingsContainerName,
			PluginsCategoryName,
			AngelscriptSectionName);
	}
}


static bool RunProjectSettingsEntryMatchesModuleLifetime(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleSettingsTests_Private;
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	const bool bHadInitialSettingsSection = FindAngelscriptProjectSettingsSection(SettingsModule).IsValid();
	FMockDirectoryWatcher DirectoryWatcher;
	FAngelscriptEditorModule Module;
	bool bModuleStarted = false;

	UnregisterAngelscriptProjectSettingsSection(SettingsModule);
	FAngelscriptEditorModuleTestAccess::SetDirectoryWatcherResolver([&DirectoryWatcher]()
	{
		return &DirectoryWatcher;
	});

	ON_SCOPE_EXIT
	{
		if (bModuleStarted)
		{
			Module.ShutdownModule();
		}

		FAngelscriptEditorModuleTestAccess::ResetDirectoryWatcherResolver();
		UnregisterAngelscriptProjectSettingsSection(SettingsModule);
		if (bHadInitialSettingsSection)
		{
			RegisterAngelscriptProjectSettingsSection(SettingsModule);
		}
	};

	if (!TestEqual(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should start from zero Angelscript project settings sections after fixture cleanup"),
			CountAngelscriptProjectSettingsSections(SettingsModule),
			0)
		|| !TestNull(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should not find an Angelscript project settings section before startup"),
			FindAngelscriptProjectSettingsSection(SettingsModule).Get()))
	{
		return false;
	}

	Module.StartupModule();
	bModuleStarted = true;

	const ISettingsSectionPtr StartupSection = FindAngelscriptProjectSettingsSection(SettingsModule);
	UObject* const StartupSettingsObject = StartupSection.IsValid() ? StartupSection->GetSettingsObject().Get() : nullptr;
	if (!TestEqual(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should register exactly one Angelscript project settings section during startup"),
			CountAngelscriptProjectSettingsSections(SettingsModule),
			1)
		|| !TestNotNull(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should find the Angelscript project settings section after startup"),
			StartupSection.Get())
		|| !TestNotNull(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should expose a settings object after startup"),
			StartupSettingsObject)
		|| !TestTrue(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should expose UAngelscriptSettings as the registered settings object"),
			StartupSettingsObject->IsA<UAngelscriptSettings>())
		|| !TestEqual(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should register the mutable Angelscript settings default object"),
			StartupSettingsObject,
			static_cast<UObject*>(GetMutableDefault<UAngelscriptSettings>())))
	{
		return false;
	}

	Module.ShutdownModule();
	bModuleStarted = false;

	if (!TestEqual(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should unregister the Angelscript project settings section during shutdown"),
			CountAngelscriptProjectSettingsSections(SettingsModule),
			0)
		|| !TestNull(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should remove the Angelscript project settings section during shutdown"),
			FindAngelscriptProjectSettingsSection(SettingsModule).Get()))
	{
		return false;
	}

	Module.StartupModule();
	bModuleStarted = true;

	const ISettingsSectionPtr RestartSection = FindAngelscriptProjectSettingsSection(SettingsModule);
	UObject* const RestartSettingsObject = RestartSection.IsValid() ? RestartSection->GetSettingsObject().Get() : nullptr;
	if (!TestEqual(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should recreate exactly one Angelscript project settings section after restart"),
			CountAngelscriptProjectSettingsSections(SettingsModule),
			1)
		|| !TestNotNull(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should recreate the Angelscript project settings section after restart"),
			RestartSection.Get())
		|| !TestNotNull(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should recreate a settings object after restart"),
			RestartSettingsObject)
		|| !TestEqual(
			TEXT("Editor.Module.ProjectSettingsEntryMatchesModuleLifetime should keep the same mutable Angelscript settings default object after restart"),
			RestartSettingsObject,
			static_cast<UObject*>(GetMutableDefault<UAngelscriptSettings>())))
	{
		return false;
	}

	return true;
}

static bool RunCompileOptionsValidationRegistration(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleSettingsTests_Private;
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	FMockDirectoryWatcher DirectoryWatcher;
	FAngelscriptEditorModule Module;
	bool bModuleStarted = false;

	// Other lifecycle tests deliberately exercise StartupModule/ShutdownModule on
	// temporary module instances. Settings registrations are global, so this
	// assertion must own the registration it observes instead of relying on the
	// editor module instance that was loaded during process startup.
	SettingsModule.UnregisterSettings(
		ProjectSettingsContainerName,
		PluginsCategoryName,
		AngelscriptCompileOptionsSectionName);
	FAngelscriptEditorModuleTestAccess::SetDirectoryWatcherResolver([&DirectoryWatcher]()
	{
		return &DirectoryWatcher;
	});

	ON_SCOPE_EXIT
	{
		if (bModuleStarted)
		{
			Module.ShutdownModule();
		}
		FAngelscriptEditorModuleTestAccess::ResetDirectoryWatcherResolver();
	};

	Module.StartupModule();
	bModuleStarted = true;

	const ISettingsSectionPtr CompileOptionsSection = FindAngelscriptCompileOptionsSettingsSection(SettingsModule);
	if (!TestNotNull(TEXT("Editor.Module.CompileOptionsValidation should register the compile options section"), CompileOptionsSection.Get()))
	{
		return false;
	}

	return TestTrue(
		TEXT("Editor.Module.CompileOptionsValidation should bind an OnModified validator"),
		CompileOptionsSection->OnModified().IsBound());
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull
#undef TestNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorModuleSettingsTests,
	"Angelscript.Editor.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ProjectSettingsEntryMatchesModuleLifetime)
	{
		ASSERT_THAT(IsTrue(RunProjectSettingsEntryMatchesModuleLifetime(*TestRunner)));
	}

	TEST_METHOD(CompileOptionsValidationIsRegistered)
	{
		ASSERT_THAT(IsTrue(RunCompileOptionsValidationRegistration(*TestRunner)));
	}
};

#endif
