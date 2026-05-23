#include "Misc/AutomationTest.h"

#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"
#include "StaticJIT/PrecompiledData.h"
#include "StaticJIT/StaticJITDiagnostics.h"
#include "StaticJIT/StaticJITHeader.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_StaticJIT_AngelscriptPrecompiledDataArchiveTests_Private
{
	constexpr TCHAR SourceFilename[] = TEXT("PrecompiledDataBuildIdentifierValidation.as");
	const FName ModuleName(TEXT("ASPrecompiledDataBuildIdentifierValidation"));

	FString MakeScriptSource()
	{
		return
			TEXT("int Add(int Left, int Right)\n")
			TEXT("{\n")
			TEXT("    return Left + Right;\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("int Entry()\n")
			TEXT("{\n")
			TEXT("    return Add(20, 22);\n")
			TEXT("}\n");
	}

	FString DescribeSavedModuleNames(const FAngelscriptPrecompiledData& Data)
	{
		TArray<FString> ModuleNames;
		Data.Modules.GetKeys(ModuleNames);
		ModuleNames.Sort();
		return FString::Join(ModuleNames, TEXT(", "));
	}

	FString GuidToString(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	int32 MakeInvalidBuildIdentifier(int32 CurrentBuildIdentifier)
	{
		if (CurrentBuildIdentifier == -1)
		{
			return 1;
		}

		return CurrentBuildIdentifier + 100;
	}

	bool ValidateRoundtripSnapshot(
		FAutomationTestBase& Test,
		const FAngelscriptPrecompiledData& Snapshot,
		FAngelscriptPrecompiledData& Loaded,
		const FString& ExpectedModuleName)
	{
		const bool bGuidMatches = Test.TestEqual(
			TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should preserve DataGuid across Save/Load"),
			GuidToString(Loaded.DataGuid),
			GuidToString(Snapshot.DataGuid));
		const bool bBuildIdentifierMatches = Test.TestEqual(
			TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should preserve BuildIdentifier across Save/Load"),
			Loaded.BuildIdentifier,
			Snapshot.BuildIdentifier);
		const bool bModuleCountMatches = Test.TestEqual(
			TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should preserve the module count across Save/Load"),
			Loaded.Modules.Num(),
			Snapshot.Modules.Num());
		const bool bModuleKeyExists = Test.TestTrue(
			TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should retain the compiled module in the loaded archive"),
			Loaded.Modules.Contains(ExpectedModuleName));

		if (!bModuleKeyExists)
		{
			Test.AddInfo(FString::Printf(TEXT("Observed loaded precompiled modules: [%s]"), *DescribeSavedModuleNames(Loaded)));
		}

		const bool bStillValid = Test.TestTrue(
			TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should accept the freshly loaded archive for the current build"),
			Loaded.IsValidForCurrentBuild());
		return bGuidMatches && bBuildIdentifierMatches && bModuleCountMatches && bModuleKeyExists && bStillValid;
	}

	bool SimulateEngineStartupDiscard(
		FAutomationTestBase& Test,
		TUniquePtr<FAngelscriptPrecompiledData>& PrecompiledData)
	{
		const bool bPointerWasLiveBeforeDiscard = Test.TestNotNull(
			TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should still hold a loaded cache pointer before the discard branch"),
			PrecompiledData.Get());
		if (!bPointerWasLiveBeforeDiscard)
		{
			return false;
		}

		if (!PrecompiledData->IsValidForCurrentBuild())
		{
			PrecompiledData.Reset();
		}

		return Test.TestNull(
			TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should discard the stale cache before later precompiled-data use"),
			PrecompiledData.Get());
	}

	asIScriptModule* FindCompiledModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName InModuleName)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(InModuleName.ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("StaticJIT.PrecompiledData should resolve compiled module '%s'."), *InModuleName.ToString()));
			return nullptr;
		}

		return ModuleDesc->ScriptModule;
	}

	void* FindGlobalVariableAddress(FAutomationTestBase& Test, asIScriptModule* Module, const char* GlobalName)
	{
		if (Module == nullptr)
		{
			return nullptr;
		}

		const asUINT GlobalCount = Module->GetGlobalVarCount();
		for (asUINT GlobalIndex = 0; GlobalIndex < GlobalCount; ++GlobalIndex)
		{
			const char* CandidateName = nullptr;
			if (Module->GetGlobalVar(GlobalIndex, &CandidateName) >= 0
				&& CandidateName != nullptr
				&& FCStringAnsi::Strcmp(CandidateName, GlobalName) == 0)
			{
				return Module->GetAddressOfGlobalVar(GlobalIndex);
			}
		}

		Test.AddError(FString::Printf(TEXT("StaticJIT.PrecompiledData should resolve global variable '%s'."), ANSI_TO_TCHAR(GlobalName)));
		return nullptr;
	}

	bool CompileGlobalReferenceFixture(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName InModuleName, const TCHAR* InSourceFilename)
	{
		const FString ScriptSource =
			TEXT("const int ReferencedGlobal = 10;\n")
			TEXT("int Entry()\n")
			TEXT("{\n")
			TEXT("    return ReferencedGlobal + 1;\n")
			TEXT("}\n");

		return Test.TestTrue(
			TEXT("StaticJIT.PrecompiledData global-reference fixture should compile"),
			AngelscriptTestSupport::CompileModuleFromMemory(
				&Engine,
				InModuleName,
				InSourceFilename,
				ScriptSource));
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPrecompiledDataBuildIdentifierValidationTest,
	"Angelscript.TestModule.StaticJIT.PrecompiledData.BuildIdentifierValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPrecompiledDataGlobalReferenceNameReuseTest,
	"Angelscript.TestModule.StaticJIT.PrecompiledData.GlobalReferenceNameReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPrecompiledDataRepeatedLoadClearsRuntimeCacheTest,
	"Angelscript.TestModule.StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptPrecompiledDataBuildIdentifierValidationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptPrecompiledDataArchiveTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	do
	{
		const FString ScriptSource = MakeScriptSource();
		const bool bCompiled = AngelscriptTestSupport::CompileModuleFromMemory(
			&Engine,
			ModuleName,
			SourceFilename,
			ScriptSource);
		if (!TestTrue(TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should compile the archive fixture module"), bCompiled))
		{
			break;
		}

		FAngelscriptPrecompiledData Snapshot(Engine.GetScriptEngine());
		Snapshot.InitFromActiveScript();

		const FString ModuleNameString = ModuleName.ToString();
		if (!TestEqual(
				TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should stamp the snapshot with the current build identifier"),
				Snapshot.BuildIdentifier,
				Snapshot.GetCurrentBuildIdentifier())
			|| !TestTrue(
				TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should serialize the newly compiled module into the snapshot"),
				Snapshot.Modules.Contains(ModuleNameString)))
		{
			AddInfo(FString::Printf(TEXT("Observed saved precompiled modules: [%s]"), *DescribeSavedModuleNames(Snapshot)));
			break;
		}

		AngelscriptTestSupport::FScopedTempPrecompiledCacheFile CacheFile(TEXT("PrecompiledDataBuildIdentifierValidation"));
		TUniquePtr<FAngelscriptPrecompiledData> LoadedData;
		FString SaveAndReloadError;
		const bool bRoundtripped = AngelscriptTestSupport::SaveAndReloadPrecompiledData(
			&Engine,
			Snapshot,
			CacheFile.GetFilename(),
			LoadedData,
			&SaveAndReloadError);
		if (!TestTrue(TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should roundtrip the archive through Save/Load"), bRoundtripped))
		{
			if (!SaveAndReloadError.IsEmpty())
			{
				AddError(SaveAndReloadError);
			}
			break;
		}

		if (!TestNotNull(TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should load a new precompiled data instance from disk"), LoadedData.Get()))
		{
			break;
		}

		if (!ValidateRoundtripSnapshot(*this, Snapshot, *LoadedData, ModuleNameString))
		{
			break;
		}

		const int32 CurrentBuildIdentifier = LoadedData->GetCurrentBuildIdentifier();
		if (!TestTrue(
				TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should run in a known UE build configuration"),
				CurrentBuildIdentifier != -1))
		{
			break;
		}

		LoadedData->BuildIdentifier = MakeInvalidBuildIdentifier(CurrentBuildIdentifier);
		if (!TestFalse(
				TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should reject archives whose BuildIdentifier no longer matches the active build"),
				LoadedData->IsValidForCurrentBuild()))
		{
			break;
		}

		if (!SimulateEngineStartupDiscard(*this, LoadedData))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

bool FAngelscriptPrecompiledDataGlobalReferenceNameReuseTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptPrecompiledDataArchiveTests_Private;

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	FAngelscriptEngineScope EngineScope(Engine);
	ON_SCOPE_EXIT
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			Engine.DiscardModule(*Module->ModuleName);
		}
	};

	const FName FixtureModuleName(TEXT("ASPrecompiledDataGlobalReferenceNameReuse"));
	if (!CompileGlobalReferenceFixture(*this, Engine, FixtureModuleName, TEXT("PrecompiledDataGlobalReferenceNameReuse.as")))
	{
		return false;
	}

	asIScriptModule* Module = FindCompiledModule(*this, Engine, FixtureModuleName);
	void* GlobalAddress = FindGlobalVariableAddress(*this, Module, "ReferencedGlobal");
	if (!TestNotNull(TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should find the global address"), GlobalAddress))
	{
		return false;
	}

	FAngelscriptPrecompiledData Snapshot(Engine.GetScriptEngine());
	Snapshot.InitFromActiveScript();

	int64 FirstReference = 0;
	int64 ReusedReference = 0;
	FString FirstName;
	FString ReusedName;
	if (!TestTrue(
			TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should resolve and reuse a global reference"),
			FStaticJITDiagnostics::ReferenceGlobalVariableTwice(Snapshot, GlobalAddress, FirstReference, ReusedReference, FirstName, ReusedName)))
	{
		return false;
	}

	TestEqual(TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should reuse the same reference id"), ReusedReference, FirstReference);
	TestEqual(TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should return the stable name when an existing global reference is reused"), ReusedName, FirstName);
	TestEqual(TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should keep the script global name stable"), ReusedName, TEXT("ReferencedGlobal"));
	return true;
}

bool FAngelscriptPrecompiledDataRepeatedLoadClearsRuntimeCacheTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptPrecompiledDataArchiveTests_Private;

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	FAngelscriptEngineScope EngineScope(Engine);
	ON_SCOPE_EXIT
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			Engine.DiscardModule(*Module->ModuleName);
		}
	};

	const FName FixtureModuleName(TEXT("ASPrecompiledDataRepeatedLoadClearsRuntimeCache"));
	if (!CompileGlobalReferenceFixture(*this, Engine, FixtureModuleName, TEXT("PrecompiledDataRepeatedLoadClearsRuntimeCache.as")))
	{
		return false;
	}

	asIScriptModule* Module = FindCompiledModule(*this, Engine, FixtureModuleName);
	void* GlobalAddress = FindGlobalVariableAddress(*this, Module, "ReferencedGlobal");
	if (!TestNotNull(TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should find the global address"), GlobalAddress))
	{
		return false;
	}

	FAngelscriptPrecompiledData Snapshot(Engine.GetScriptEngine());
	Snapshot.InitFromActiveScript();
	int64 GlobalReference = 0;
	int64 ReusedReference = 0;
	FString FirstName;
	FString ReusedName;
	if (!TestTrue(
			TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should resolve a stable global reference"),
			FStaticJITDiagnostics::ReferenceGlobalVariableTwice(Snapshot, GlobalAddress, GlobalReference, ReusedReference, FirstName, ReusedName)))
	{
		return false;
	}

	AngelscriptTestSupport::FScopedTempPrecompiledCacheFile CacheFile(TEXT("PrecompiledDataRepeatedLoadClearsRuntimeCache"));
	TUniquePtr<FAngelscriptPrecompiledData> LoadedData;
	FString SaveAndReloadError;
	if (!TestTrue(
			TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should roundtrip the fixture cache"),
			AngelscriptTestSupport::SaveAndReloadPrecompiledData(&Engine, Snapshot, CacheFile.GetFilename(), LoadedData, &SaveAndReloadError)))
	{
		if (!SaveAndReloadError.IsEmpty())
		{
			AddError(SaveAndReloadError);
		}
		return false;
	}

	if (!TestNotNull(TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should load precompiled data"), LoadedData.Get()))
	{
		return false;
	}

	void* FirstResolvedAddress = nullptr;
	void* SecondResolvedAddress = nullptr;
	bool bCacheClearedAfterLoad = false;
	if (!TestTrue(
			TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should re-resolve JIT refs and clear pointer cache after repeated Load"),
			FStaticJITDiagnostics::ExerciseRepeatedGlobalReferenceLoad(*LoadedData, CacheFile.GetFilename(), GlobalReference, FirstResolvedAddress, SecondResolvedAddress, bCacheClearedAfterLoad)))
	{
		return false;
	}

	TestTrue(TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should clear pointer cache on repeated Load"), bCacheClearedAfterLoad);
	TestTrue(
		TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should re-resolve globals after repeated Load"),
		SecondResolvedAddress == FirstResolvedAddress);
	return true;
}

#endif
