#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "Shared/AngelscriptTestEngine.h"

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

namespace AngelscriptTest_Core_AngelscriptEngineDependencyInjectionTests_Private
{

bool RunInjectedScriptRootDiscovery(FAutomationTestBase& Test)
{
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

	Test.TestEqual(TEXT("Injected project root should be first"), Roots[0], FString(TEXT("C:/InjectedProject/Script")));
	Test.TestEqual(TEXT("Injected plugin roots should be sorted deterministically"), Roots[1], FString(TEXT("C:/Plugins/Alpha/Script")));
	Test.TestEqual(TEXT("Injected plugin roots should keep all entries"), Roots[2], FString(TEXT("C:/Plugins/Beta/Script")));

	return true;
}

bool RunInjectedProjectOnlyScriptRootDiscovery(FAutomationTestBase& Test)
{
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

	Test.TestEqual(TEXT("Project-only discovery should return exactly one root"), Roots.Num(), 1);
	if (Roots.Num() == 1)
	{
		Test.TestEqual(TEXT("Project-only discovery should keep only the project root"), Roots[0], FString(TEXT("C:/InjectedProjectOnly/Script")));
	}

	return true;
}

bool RunInjectedMissingPluginScriptRootSkip(FAutomationTestBase& Test)
{
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

	Test.TestEqual(TEXT("Missing plugin roots should be skipped and project root should not be duplicated"), Roots.Num(), 2);
	if (Roots.Num() == 2)
	{
		Test.TestEqual(TEXT("Project root should remain first when skipping missing plugin roots"), Roots[0], FString(TEXT("C:/InjectedSkipProject/Script")));
		Test.TestEqual(TEXT("Only existing plugin root should remain after skipping missing roots"), Roots[1], FString(TEXT("C:/Plugins/Alpha/Script")));
	}

	return true;
}

bool RunInjectedEditorCreatesProjectScriptRoot(FAutomationTestBase& Test)
{
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

	Test.TestTrue(TEXT("Editor discovery should create the missing project script root"), bMakeDirectoryCalled);
	Test.TestEqual(TEXT("Editor discovery should create the expected project script root path"), CreatedPath, FString(TEXT("C:/InjectedEditorProject/Script")));
	Test.TestEqual(TEXT("Editor discovery should still return the project root after creation"), Roots.Num(), 1);
	if (Roots.Num() == 1)
	{
		Test.TestEqual(TEXT("Created project root should be returned by discovery"), Roots[0], FString(TEXT("C:/InjectedEditorProject/Script")));
	}

	return true;
}

bool RunCreateWithSkipInitialCompileSkipsProductionDirectorySetup(FAutomationTestBase& Test)
{
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
	if (!Test.TestNotNull(TEXT("Create.WithSkipInitialCompileSkipsProductionDirectorySetup should create an engine"), Engine.Get()))
	{
		return false;
	}

	Test.TestFalse(TEXT("Create.WithSkipInitialCompileSkipsProductionDirectorySetup should not run the production script-root setup path"), bMakeDirectoryCalled);
	return Test.TestEqual(TEXT("Create.WithSkipInitialCompileSkipsProductionDirectorySetup should keep the production setup path untouched"), CreatedPath, FString());
}

bool RunCreateTestingFullEngineSkipsProductionDirectorySetup(FAutomationTestBase& Test)
{
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
	if (!Test.TestNotNull(TEXT("CreateTestingFullEngine.SkipsProductionDirectorySetup should create a testing full engine"), Engine.Get()))
	{
		return false;
	}

	return Test.TestFalse(TEXT("CreateTestingFullEngine.SkipsProductionDirectorySetup should not run the production script-root setup path"), bMakeDirectoryCalled);
}

}

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineDependencyInjectionTests,
	"Angelscript.TestModule.Engine.DependencyInjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InjectedScriptRootDiscovery)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineDependencyInjectionTests_Private;
		RunInjectedScriptRootDiscovery(*TestRunner);
	}

	TEST_METHOD(InjectedProjectOnlyScriptRootDiscovery)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineDependencyInjectionTests_Private;
		RunInjectedProjectOnlyScriptRootDiscovery(*TestRunner);
	}

	TEST_METHOD(InjectedMissingPluginScriptRootSkip)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineDependencyInjectionTests_Private;
		RunInjectedMissingPluginScriptRootSkip(*TestRunner);
	}

	TEST_METHOD(InjectedEditorCreatesProjectScriptRoot)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineDependencyInjectionTests_Private;
		RunInjectedEditorCreatesProjectScriptRoot(*TestRunner);
	}

	TEST_METHOD(CreateWithSkipInitialCompileSkipsProductionDirectorySetup)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineDependencyInjectionTests_Private;
		RunCreateWithSkipInitialCompileSkipsProductionDirectorySetup(*TestRunner);
	}

	TEST_METHOD(CreateTestingFullEngineSkipsProductionDirectorySetup)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineDependencyInjectionTests_Private;
		RunCreateTestingFullEngineSkipsProductionDirectorySetup(*TestRunner);
	}

};

#endif
