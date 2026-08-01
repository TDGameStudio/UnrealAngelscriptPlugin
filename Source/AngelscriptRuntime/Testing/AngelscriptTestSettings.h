#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "AngelscriptTestSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Angelscript Test User Settings"))
class UAngelscriptTestUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Begin UDeveloperSettings interface
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Editor"); }
	// End UDeveloperSettings interface

	/** Whether tests on hot reload is enabled. */
	UPROPERTY(EditAnywhere, config, Category = UnitTests)
	bool bRunUnitTestsOnHotReload = true;

	/**
	 * When hot reloading, all tests depending on the newly recompiled files will be executed.
	 * Use this setting to limit the number of modules to be tested upon hot reload.
	 * Set to 0 to disable limit.
	 */
	UPROPERTY(EditAnywhere, config, Category = UnitTests)
	int LimitNModulesToTestOnHotReload = 0;
};

UCLASS(config=Editor, defaultconfig, meta=(DisplayName="Angelscript Test Settings"))
class UAngelscriptTestSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Begin UDeveloperSettings interface
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Editor"); }
	// End UDeveloperSettings interface

	/** Whether to enable test discovery from script files. */
	UPROPERTY(EditAnywhere, config, Category = Tests)
	bool bEnableTestDiscovery = true;

	/**
	 * Force garbage collection every N tests.
	 * This will also perform test execution in batches when hot reloading.
	 * All the tests of an AS module are added or not, so the number of test in a batch can
	 * be greater that this. Understand this setting as a minimum number of tests per batch but
	 * don't continue adding modules once this number has been reached.
	 * This allows the editor to refresh between batches and prevents a lock because of PhysX issues.
	 * Set to 0 to disable.
	*/
	UPROPERTY(EditAnywhere, config, Category = Tests)
	int GarbageCollectEveryNTests;

	/** Turn on code coverage measurements. Reports are written to Saved/CodeCoverage/. */
	UPROPERTY(EditAnywhere, config, Category = CodeCoverage, Meta = (ConfigRestartRequired = true))
	bool bEnableCodeCoverage;

	/** 
	 * Don't measure coverage in paths that match these wildcards.
	 * Paths start at the Angelscript root (by convention this dir is named Script/). If you want
	 * to exclude a directory from which modules are imported as Network.MyFile, you want to add
	 * Network/_* to this list (NOTE: remove the _, it's there to avoid -Wcomment in clang).
	 * If you just want to exclude MyFile.as, add Network/MyFile.as. If you have the convention
	 * that unit tests be in files named _Test.as, add the pattern *_Test.as, and so on.
	 */
	UPROPERTY(EditAnywhere, config, Category = CodeCoverage)
	TArray<FString> CoverageExcludePatterns;

	/**
	 * Debug-break on ensures in tests. This is generally off in tests because ensures
	 * are intentionally triggered by tests all the time, which is really annoying.
	 */
	UPROPERTY(EditAnywhere, config, Category = Debugging)
	bool bEnableDebugBreaksInTests;

};
