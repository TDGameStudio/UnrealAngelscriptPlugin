#include "AngelscriptEngine.h"
#include "CQTest.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_FileSystem_AngelscriptScriptRootDiscoveryTests_Private
{
	FString NormalizeDiscoveryPath(const FString& InPath)
	{
		FString Normalized = InPath;
		FPaths::NormalizeFilename(Normalized);
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Normalized;
	}

	TArray<FString> MakeScriptRootsForTest(
		const FAngelscriptEngineConfig& Config,
		const FAngelscriptEngineDependencies& Dependencies,
		const bool bOnlyProjectRoot)
	{
		FAngelscriptEngine TemporaryEngine(Config, Dependencies);
		return TemporaryEngine.DiscoverScriptRoots(bOnlyProjectRoot);
	}

	bool TestRootSequence(
		FAutomationTestBase& Test,
		const FString& Context,
		const TArray<FString>& Actual,
		const TArray<FString>& Expected)
	{
		bool bPassed = Test.TestEqual(
			*FString::Printf(TEXT("%s should keep the expected root count"), *Context),
			Actual.Num(),
			Expected.Num());
		if (Actual.Num() != Expected.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should keep root index %d stable"), *Context, Index),
				Actual[Index],
				Expected[Index]);
		}

		return bPassed;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptRootDiscoveryTest,
	"Angelscript.TestModule.FileSystem.RootDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ProjectRootFirstAndPluginRootsDeduped)
	{
		using namespace AngelscriptTest_FileSystem_AngelscriptScriptRootDiscoveryTests_Private;
		const FString ProjectDir = NormalizeDiscoveryPath(TEXT("J:/VirtualProject"));
		const FString ProjectScriptRoot = NormalizeDiscoveryPath(ProjectDir / TEXT("Script"));
		const FString PluginBetaRoot = NormalizeDiscoveryPath(TEXT("J:/VirtualProject/Plugins/Beta/Script"));
		const FString PluginAlphaRoot = NormalizeDiscoveryPath(TEXT("J:/VirtualProject/Plugins/Alpha/Script"));
		const FString MissingPluginRoot = NormalizeDiscoveryPath(TEXT("J:/VirtualProject/Plugins/Missing/Script"));

		int32 PluginRootQueryCount = 0;
		TArray<FString> DirectoryExistQueries;
		TArray<FString> MadeDirectories;
		const TSet<FString> ExistingRoots = {ProjectScriptRoot, PluginAlphaRoot, PluginBetaRoot};

		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;

		FAngelscriptEngineDependencies Dependencies;
		Dependencies.GetProjectDir = [ProjectDir]()
		{
			return ProjectDir;
		};
		Dependencies.ConvertRelativePathToFull = [](const FString& Path)
		{
			return NormalizeDiscoveryPath(Path);
		};
		Dependencies.DirectoryExists = [&DirectoryExistQueries, ExistingRoots](const FString& Path) mutable
		{
			const FString Normalized = NormalizeDiscoveryPath(Path);
			DirectoryExistQueries.Add(Normalized);
			return ExistingRoots.Contains(Normalized);
		};
		Dependencies.MakeDirectory = [&MadeDirectories](const FString& Path, const bool bTree) mutable
		{
			MadeDirectories.Add(NormalizeDiscoveryPath(Path));
			return true;
		};
		Dependencies.GetEnabledPluginScriptRoots = [&PluginRootQueryCount, PluginBetaRoot, ProjectScriptRoot, MissingPluginRoot, PluginAlphaRoot]() mutable
		{
			++PluginRootQueryCount;
			return TArray<FString>{PluginBetaRoot, ProjectScriptRoot, MissingPluginRoot, PluginAlphaRoot};
		};

		const TArray<FString> DiscoveredRoots = MakeScriptRootsForTest(Config, Dependencies, false);
		const TArray<FString> ProjectOnlyRoots = MakeScriptRootsForTest(Config, Dependencies, true);
		const TArray<FString> WrappedRoots = MakeScriptRootsForTest(Config, Dependencies, false);
		const TArray<FString> ExpectedDiscoveredRoots = {ProjectScriptRoot, PluginAlphaRoot, PluginBetaRoot};

		ASSERT_THAT(IsTrue(TestRootSequence(*TestRunner, TEXT("DiscoverScriptRoots(false)"), DiscoveredRoots, ExpectedDiscoveredRoots), TEXT("DiscoverScriptRoots(false) should match expected sequence")));
		ASSERT_THAT(IsTrue(TestRootSequence(*TestRunner, TEXT("DiscoverScriptRoots(true)"), ProjectOnlyRoots, {ProjectScriptRoot}), TEXT("DiscoverScriptRoots(true) should match expected sequence")));
		ASSERT_THAT(IsTrue(TestRootSequence(*TestRunner, TEXT("Equivalent wrapper root discovery"), WrappedRoots, ExpectedDiscoveredRoots), TEXT("Equivalent wrapper root discovery should match expected sequence")));
		ASSERT_THAT(AreEqual(1, DiscoveredRoots.FilterByPredicate([&ProjectScriptRoot](const FString& Root) { return Root == ProjectScriptRoot; }).Num(), TEXT("Project root should only appear once even if plugins report the same path")));
		ASSERT_THAT(IsFalse(DiscoveredRoots.Contains(MissingPluginRoot), TEXT("Missing plugin roots should be filtered out of discovery")));
		ASSERT_THAT(IsTrue(MadeDirectories.IsEmpty(), TEXT("Existing project root should not trigger directory creation in editor mode")));
		ASSERT_THAT(AreEqual(2, PluginRootQueryCount, TEXT("Plugin root provider should only be queried for the two non-project-only discovery calls")));
		ASSERT_THAT(IsTrue(
			DirectoryExistQueries.Contains(ProjectScriptRoot)
				&& DirectoryExistQueries.Contains(PluginAlphaRoot)
				&& DirectoryExistQueries.Contains(PluginBetaRoot)
				&& DirectoryExistQueries.Contains(MissingPluginRoot),
			TEXT("DirectoryExists should consult project and plugin roots during discovery")));
	}
};

#endif
