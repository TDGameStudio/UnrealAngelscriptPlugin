#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "AngelscriptTestEngine.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FAngelscriptDependencyInjectionTestAccess
{
	static void ResetToIsolatedEngineState()
	{
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptEngine::DestroyGlobal();
		}
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineDependencyInjectionTests,
	"Angelscript.TestModule.Engine.DependencyInjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool RunInjectedScriptRootDiscovery(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FAngelscriptDependencyInjectionTestAccess::ResetToIsolatedEngineState();

FAngelscriptEngineConfig Config;
FAngelscriptEngineDependencies Dependencies;

Dependencies.GetProjectDir = []()
{
	return FString(TEXT("C:/InjectedProject"));
};

Dependencies.ConvertRelativePathToFull = [](const FString& Path)
{
	return Path;
};

Dependencies.DirectoryExists = [](const FString& Path)
{
	return Path == TEXT("C:/InjectedProject/Script")
		|| Path == TEXT("C:/Plugins/Beta/Script")
		|| Path == TEXT("C:/Plugins/Alpha/Script");
};

Dependencies.MakeDirectory = [](const FString& Path, bool bTree)
{
	return true;
};

Dependencies.GetEnabledPluginScriptRoots = []()
{
	return TArray<FString>
	{
		TEXT("C:/Plugins/Beta/Script"),
		TEXT("C:/Plugins/Alpha/Script"),
	};
};

TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
TArray<FString> Roots = Engine->DiscoverScriptRoots(false);

bool bOk = true;
bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedProject/Script")), Roots[0], TEXT("Injected project root should be first"));
bOk &= LocalAssert.AreEqual(FString(TEXT("C:/Plugins/Alpha/Script")), Roots[1], TEXT("Injected plugin roots should be sorted deterministically"));
bOk &= LocalAssert.AreEqual(FString(TEXT("C:/Plugins/Beta/Script")), Roots[2], TEXT("Injected plugin roots should keep all entries"));

return bOk;
}

static bool RunInjectedProjectOnlyScriptRootDiscovery(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FAngelscriptDependencyInjectionTestAccess::ResetToIsolatedEngineState();

FAngelscriptEngineConfig Config;
FAngelscriptEngineDependencies Dependencies;

Dependencies.GetProjectDir = []()
{
	return FString(TEXT("C:/InjectedProjectOnly"));
};

Dependencies.ConvertRelativePathToFull = [](const FString& Path)
{
	return Path;
};

Dependencies.DirectoryExists = [](const FString& Path)
{
	return Path == TEXT("C:/InjectedProjectOnly/Script")
		|| Path == TEXT("C:/Plugins/ShouldNotAppear/Script");
};

Dependencies.MakeDirectory = [](const FString& Path, bool bTree)
{
	return true;
};

Dependencies.GetEnabledPluginScriptRoots = []()
{
	return TArray<FString>
	{
		TEXT("C:/Plugins/ShouldNotAppear/Script"),
	};
};

TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
TArray<FString> Roots = Engine->DiscoverScriptRoots(true);

bool bOk = LocalAssert.AreEqual(1, Roots.Num(), TEXT("Project-only discovery should return exactly one root"));
if (Roots.Num() == 1)
{
	bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedProjectOnly/Script")), Roots[0], TEXT("Project-only discovery should keep only the project root"));
}

return bOk;
}

static bool RunInjectedMissingPluginScriptRootSkip(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FAngelscriptDependencyInjectionTestAccess::ResetToIsolatedEngineState();

FAngelscriptEngineConfig Config;
FAngelscriptEngineDependencies Dependencies;

Dependencies.GetProjectDir = []()
{
	return FString(TEXT("C:/InjectedSkipProject"));
};

Dependencies.ConvertRelativePathToFull = [](const FString& Path)
{
	return Path;
};

Dependencies.DirectoryExists = [](const FString& Path)
{
	return Path == TEXT("C:/InjectedSkipProject/Script")
		|| Path == TEXT("C:/Plugins/Alpha/Script");
};

Dependencies.MakeDirectory = [](const FString& Path, bool bTree)
{
	return true;
};

Dependencies.GetEnabledPluginScriptRoots = []()
{
	return TArray<FString>
	{
		TEXT("C:/Plugins/Missing/Script"),
		TEXT("C:/Plugins/Alpha/Script"),
		TEXT("C:/InjectedSkipProject/Script"),
	};
};

TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
TArray<FString> Roots = Engine->DiscoverScriptRoots(false);

bool bOk = LocalAssert.AreEqual(2, Roots.Num(), TEXT("Missing plugin roots should be skipped and project root should not be duplicated"));
if (Roots.Num() == 2)
{
	bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedSkipProject/Script")), Roots[0], TEXT("Project root should remain first when skipping missing plugin roots"));
	bOk &= LocalAssert.AreEqual(FString(TEXT("C:/Plugins/Alpha/Script")), Roots[1], TEXT("Only existing plugin root should remain after skipping missing roots"));
}

return bOk;
}

static bool RunInjectedEditorCreatesProjectScriptRoot(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FAngelscriptDependencyInjectionTestAccess::ResetToIsolatedEngineState();

FAngelscriptEngineConfig Config;
Config.bIsEditor = true;

bool bMakeDirectoryCalled = false;
FString CreatedPath;

FAngelscriptEngineDependencies Dependencies;
Dependencies.GetProjectDir = []()
{
	return FString(TEXT("C:/InjectedEditorProject"));
};

Dependencies.ConvertRelativePathToFull = [](const FString& Path)
{
	return Path;
};

Dependencies.DirectoryExists = [](const FString& Path)
{
	return false;
};

Dependencies.MakeDirectory = [&bMakeDirectoryCalled, &CreatedPath](const FString& Path, bool bTree)
{
	bMakeDirectoryCalled = true;
	CreatedPath = Path;
	return true;
};

Dependencies.GetEnabledPluginScriptRoots = []()
{
	return TArray<FString>();
};

TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
TArray<FString> Roots = Engine->DiscoverScriptRoots(false);

bool bOk = true;
bOk &= LocalAssert.IsTrue(bMakeDirectoryCalled, TEXT("Editor discovery should create the missing project script root"));
bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedEditorProject/Script")), CreatedPath, TEXT("Editor discovery should create the expected project script root path"));
bOk &= LocalAssert.AreEqual(1, Roots.Num(), TEXT("Editor discovery should still return the project root after creation"));
if (Roots.Num() == 1)
{
	bOk &= LocalAssert.AreEqual(FString(TEXT("C:/InjectedEditorProject/Script")), Roots[0], TEXT("Created project root should be returned by discovery"));
}

return bOk;
}

static bool RunCreateWithSkipInitialCompileSkipsProductionDirectorySetup(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FAngelscriptDependencyInjectionTestAccess::ResetToIsolatedEngineState();

// Verifies the OpenSpec D8 contract for the unified `FAngelscriptEngine::Create`
// factory: setting `Config.bSkipInitialCompile = true` selects the
// InitializeWithoutInitialCompile path, which must NOT touch the project
// script-root directories. The default-flag path (full Initialize) is
// covered by production-side runtime tests; the wrapper path is covered
// by `RunCreateTestingFullEngineSkipsProductionDirectorySetup` below.
FAngelscriptEngineConfig Config;
Config.bIsEditor = true;
Config.bSkipInitialCompile = true;

bool bMakeDirectoryCalled = false;
FString CreatedPath;

FAngelscriptEngineDependencies Dependencies;
Dependencies.GetProjectDir = []()
{
	return FString(TEXT("C:/CreateFactoryProject"));
};
Dependencies.ConvertRelativePathToFull = [](const FString& Path)
{
	return Path;
};
Dependencies.DirectoryExists = [](const FString& Path)
{
	return false;
};
Dependencies.MakeDirectory = [&bMakeDirectoryCalled, &CreatedPath](const FString& Path, bool bTree)
{
	bMakeDirectoryCalled = true;
	CreatedPath = Path;
	return true;
};
Dependencies.GetEnabledPluginScriptRoots = []()
{
	return TArray<FString>();
};

TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(Engine.Get(), TEXT("Create.WithSkipInitialCompileSkipsProductionDirectorySetup should create an engine")))
{
	return false;
}

bool bOk = LocalAssert.IsFalse(bMakeDirectoryCalled, TEXT("Create.WithSkipInitialCompileSkipsProductionDirectorySetup should not run the production script-root setup path"));
bOk &= LocalAssert.AreEqual(FString(), CreatedPath, TEXT("Create.WithSkipInitialCompileSkipsProductionDirectorySetup should keep the production setup path untouched"));
return bOk;
}

static bool RunCreateTestingFullEngineSkipsProductionDirectorySetup(FAutomationTestBase& Test)
{
FNoDiscardAsserter LocalAssert(Test);
FAngelscriptDependencyInjectionTestAccess::ResetToIsolatedEngineState();

FAngelscriptEngineConfig Config;
Config.bIsEditor = true;

bool bMakeDirectoryCalled = false;

FAngelscriptEngineDependencies Dependencies;
Dependencies.GetProjectDir = []()
{
	return FString(TEXT("C:/CreateTestingFullProject"));
};
Dependencies.ConvertRelativePathToFull = [](const FString& Path)
{
	return Path;
};
Dependencies.DirectoryExists = [](const FString& Path)
{
	return false;
};
Dependencies.MakeDirectory = [&bMakeDirectoryCalled](const FString& Path, bool bTree)
{
	bMakeDirectoryCalled = true;
	return true;
};
Dependencies.GetEnabledPluginScriptRoots = []()
{
	return TArray<FString>();
};

TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
if (!LocalAssert.IsNotNull(Engine.Get(), TEXT("CreateTestingFullEngine.SkipsProductionDirectorySetup should create a testing full engine")))
{
	return false;
}

return LocalAssert.IsFalse(bMakeDirectoryCalled, TEXT("CreateTestingFullEngine.SkipsProductionDirectorySetup should not run the production script-root setup path"));
}

public:
	TEST_METHOD(InjectedScriptRootDiscovery)
	{
RunInjectedScriptRootDiscovery(*TestRunner);
	}

	TEST_METHOD(InjectedProjectOnlyScriptRootDiscovery)
	{
RunInjectedProjectOnlyScriptRootDiscovery(*TestRunner);
	}

	TEST_METHOD(InjectedMissingPluginScriptRootSkip)
	{
RunInjectedMissingPluginScriptRootSkip(*TestRunner);
	}

	TEST_METHOD(InjectedEditorCreatesProjectScriptRoot)
	{
RunInjectedEditorCreatesProjectScriptRoot(*TestRunner);
	}

	TEST_METHOD(CreateWithSkipInitialCompileSkipsProductionDirectorySetup)
	{
RunCreateWithSkipInitialCompileSkipsProductionDirectorySetup(*TestRunner);
	}

	TEST_METHOD(CreateTestingFullEngineSkipsProductionDirectorySetup)
	{
RunCreateTestingFullEngineSkipsProductionDirectorySetup(*TestRunner);
	}

};

#endif
