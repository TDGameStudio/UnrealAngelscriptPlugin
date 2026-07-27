#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestMacros.h"
#include "StaticJIT/PrecompiledData.h"
#include "StaticJIT/StaticJITDiagnostics.h"
#include "StaticJIT/StaticJITHeader.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptPrecompiledDataArchiveTests,
	"Angelscript.TestModule.StaticJIT.PrecompiledData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr TCHAR SourceFilename[] = TEXT("PrecompiledDataBuildIdentifierValidation.as");
	inline static const FName ModuleName = FName(TEXT("ASPrecompiledDataBuildIdentifierValidation"));

	static FString MakeScriptSource()
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

	static FString DescribeSavedModuleNames(const FAngelscriptPrecompiledData& Data)
	{
		TArray<FString> ModuleNames;
		Data.Modules.GetKeys(ModuleNames);
		ModuleNames.Sort();
		return FString::Join(ModuleNames, TEXT(", "));
	}

	static FString GuidToString(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	static int32 MakeInvalidBuildIdentifier(int32 CurrentBuildIdentifier)
	{
		if (CurrentBuildIdentifier == -1)
		{
			return 1;
		}

		return CurrentBuildIdentifier + 100;
	}

	static bool ValidateRoundtripSnapshot(
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

	static bool SimulateEngineStartupDiscard(
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

	static asIScriptModule* FindCompiledModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName InModuleName)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(InModuleName.ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("StaticJIT.PrecompiledData should resolve compiled module '%s'."), *InModuleName.ToString()));
			return nullptr;
		}

		return ModuleDesc->ScriptModule;
	}

	static void* FindGlobalVariableAddress(FAutomationTestBase& Test, asIScriptModule* Module, const char* GlobalName)
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

	static bool CompileGlobalReferenceFixture(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName InModuleName, const TCHAR* InSourceFilename)
	{
		const FString ScriptSource =
			TEXT("const int ReferencedGlobal = 10;\n")
			TEXT("int Entry()\n")
			TEXT("{\n")
			TEXT("    return ReferencedGlobal + 1;\n")
			TEXT("}\n");

		return Test.TestTrue(
			TEXT("StaticJIT.PrecompiledData global-reference fixture should compile"),
			CompileModuleFromMemory(
				&Engine,
				InModuleName,
				InSourceFilename,
				ScriptSource));
	}

	static void DiscardActiveModules(FAngelscriptEngine& Engine)
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			Engine.DiscardModule(*Module->ModuleName);
		}
	}

	static bool FindReferenceCopyTypeOperand(
		asIScriptFunction& Function,
		asPWORD& OutTypeOperand)
	{
		OutTypeOperand = 0;

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				return false;
			}

			if (Opcode == asBC_REFCPY || Opcode == asBC_RefCpyV)
			{
				OutTypeOperand = asBC_PTRARG(&Bytecode[DwordIndex]);
				return true;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

	static const FAngelscriptPrecompiledFunction* FindSerializedGlobalFunction(
		const FAngelscriptPrecompiledData& Data,
		const FName& InModuleName,
		const ANSICHAR* InFunctionName)
	{
		const FAngelscriptPrecompiledModule* const Module = Data.Modules.Find(InModuleName.ToString());
		if (Module == nullptr || InFunctionName == nullptr)
		{
			return nullptr;
		}

		for (const FAngelscriptPrecompiledFunction& Function : Module->Functions)
		{
			if (FCStringAnsi::Strcmp(*Function.FunctionName, InFunctionName) == 0)
			{
				return &Function;
			}
		}

		return nullptr;
	}

	static bool RestoreSerializedBytecode(
		const FAngelscriptPrecompiledFunction& SerializedFunction,
		asIScriptFunction& TargetFunction)
	{
		asCScriptFunction& TargetInternalFunction = static_cast<asCScriptFunction&>(TargetFunction);
		if (TargetInternalFunction.scriptData == nullptr)
		{
			return false;
		}

		TargetInternalFunction.scriptData->byteCode.SetLength(SerializedFunction.ByteCode.Num());
		FMemory::Memcpy(
			TargetInternalFunction.scriptData->byteCode.AddressOf(),
			SerializedFunction.ByteCode.GetData(),
			SerializedFunction.ByteCode.Num() * sizeof(asDWORD));
		return true;
	}

	static bool RunBuildIdentifierValidation(FAutomationTestBase& Test);
	static bool RunGlobalReferenceNameReuse(FAutomationTestBase& Test);
	static bool RunRepeatedLoadClearsRuntimeCache(FAutomationTestBase& Test);

public:
	TEST_METHOD(BuildIdentifierValidation)
	{
		ASSERT_THAT(IsTrue(RunBuildIdentifierValidation(*TestRunner)));
	}

	TEST_METHOD(GlobalReferenceNameReuse)
	{
		ASSERT_THAT(IsTrue(RunGlobalReferenceNameReuse(*TestRunner)));
	}

	TEST_METHOD(RepeatedLoadClearsRuntimeCache)
	{
		ASSERT_THAT(IsTrue(RunRepeatedLoadClearsRuntimeCache(*TestRunner)));
	}

	TEST_METHOD(ReferenceCopyTypeOperandsRemapAcrossPrecompiledLoad)
	{
		const FName FixtureModuleName(TEXT("ASPrecompiledReferenceCopyTypeRemap"));
		const FString ScriptSource = ASTEST_AS(R"AS(
			int Entry(UObject Source)
			{
				UObject Alias = Source;
				return Alias == nullptr ? 1 : 0;
			}
			)AS");
		TestRunner->AddInfo(FString::Printf(
			TEXT("[AS-STATICJIT-PRECOMPILED-SOURCE]\n%s"),
			*ScriptSource));

		TUniquePtr<FAngelscriptEngine> SourceEngine = CreateScriptScanFreeFullEngineForTesting();
		ASSERT_THAT(IsNotNull(SourceEngine.Get(), TEXT("StaticJIT.PrecompiledData reference-copy remap should create the source engine")));
		ON_SCOPE_EXIT
		{
			FAngelscriptEngineScope SourceCleanupScope(*SourceEngine);
			DiscardActiveModules(*SourceEngine);
		};

		FScopedTempPrecompiledCacheFile CacheFile(TEXT("PrecompiledReferenceCopyTypeRemap"));
		asPWORD SourceTypeOperand = 0;
		asPWORD SourceTypePointer = 0;
		{
			FAngelscriptEngineScope SourceScope(*SourceEngine);

			ASSERT_THAT(IsTrue(
				CompileModuleFromMemory(
					SourceEngine.Get(),
					FixtureModuleName,
					TEXT("PrecompiledReferenceCopyTypeRemap.as"),
					ScriptSource),
				TEXT("StaticJIT.PrecompiledData reference-copy remap should compile the source fixture")));

			asIScriptModule* const SourceModule = FindCompiledModule(*TestRunner, *SourceEngine, FixtureModuleName);
			ASSERT_THAT(IsNotNull(SourceModule, TEXT("StaticJIT.PrecompiledData reference-copy remap should publish the source module")));

			asIScriptFunction* const SourceEntry = SourceModule->GetFunctionByName("Entry");
			ASSERT_THAT(IsNotNull(SourceEntry, TEXT("StaticJIT.PrecompiledData reference-copy remap should publish the source entry function")));
			TestRunner->AddInfo(FString::Printf(
				TEXT("[AS-STATICJIT-PRECOMPILED-DECLARATION][source] %s"),
				UTF8_TO_TCHAR(SourceEntry->GetDeclaration())));

			ASSERT_THAT(IsTrue(
				FindReferenceCopyTypeOperand(*SourceEntry, SourceTypeOperand),
				TEXT("StaticJIT.PrecompiledData reference-copy remap source bytecode should contain a typed reference-copy instruction")));

			asITypeInfo* const SourceType = SourceEngine->GetScriptEngine()->GetTypeInfoByDecl("UObject");
			ASSERT_THAT(IsNotNull(SourceType, TEXT("StaticJIT.PrecompiledData reference-copy remap should publish the source type")));
			SourceTypePointer = reinterpret_cast<asPWORD>(SourceType);
			ASSERT_THAT(AreEqual(
				SourceTypePointer,
				SourceTypeOperand,
				TEXT("StaticJIT.PrecompiledData reference-copy source operand should reference its source type")));

			FAngelscriptPrecompiledData Snapshot(SourceEngine->GetScriptEngine());
			Snapshot.InitFromActiveScript();
			Snapshot.Save(CacheFile.GetFilename());
			ASSERT_THAT(IsTrue(
				IFileManager::Get().FileExists(*CacheFile.GetFilename()),
				TEXT("StaticJIT.PrecompiledData reference-copy remap should save a precompiled cache")));
		}

		TUniquePtr<FAngelscriptEngine> RestoredEngine = CreateScriptScanFreeFullEngineForTesting();
		ASSERT_THAT(IsNotNull(RestoredEngine.Get(), TEXT("StaticJIT.PrecompiledData reference-copy remap should create the restored engine")));
		ON_SCOPE_EXIT
		{
			FAngelscriptEngineScope RestoredCleanupScope(*RestoredEngine);
			DiscardActiveModules(*RestoredEngine);
		};
		FAngelscriptEngineScope RestoredScope(*RestoredEngine);
		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(
				RestoredEngine.Get(),
				FixtureModuleName,
				TEXT("PrecompiledReferenceCopyTypeRemap.as"),
				ScriptSource),
			TEXT("StaticJIT.PrecompiledData reference-copy remap should compile the target fixture")));

		asIScriptModule* const RestoredModule = FindCompiledModule(*TestRunner, *RestoredEngine, FixtureModuleName);
		ASSERT_THAT(IsNotNull(RestoredModule, TEXT("StaticJIT.PrecompiledData reference-copy remap should publish the restored module")));

		asIScriptFunction* const RestoredEntry = RestoredModule->GetFunctionByName("Entry");
		ASSERT_THAT(IsNotNull(RestoredEntry, TEXT("StaticJIT.PrecompiledData reference-copy remap should publish the restored entry function")));
		TestRunner->AddInfo(FString::Printf(
			TEXT("[AS-STATICJIT-PRECOMPILED-DECLARATION][target] %s"),
			UTF8_TO_TCHAR(RestoredEntry->GetDeclaration())));

		FAngelscriptPrecompiledData LoadedData(RestoredEngine->GetScriptEngine());
		LoadedData.Load(CacheFile.GetFilename());
		ASSERT_THAT(IsTrue(
			LoadedData.IsValidForCurrentBuild(),
			TEXT("StaticJIT.PrecompiledData reference-copy remap should load a cache for the current build")));

		const FAngelscriptPrecompiledFunction* const SerializedEntry = FindSerializedGlobalFunction(
			LoadedData,
			FixtureModuleName,
			"Entry");
		ASSERT_THAT(IsNotNull(SerializedEntry, TEXT("StaticJIT.PrecompiledData reference-copy remap should retain serialized entry bytecode")));
		ASSERT_THAT(IsTrue(
			RestoreSerializedBytecode(*SerializedEntry, *RestoredEntry),
			TEXT("StaticJIT.PrecompiledData reference-copy remap should restore serialized bytecode into the target function")));

		asPWORD SerializedTypeOperand = 0;
		ASSERT_THAT(IsTrue(
			FindReferenceCopyTypeOperand(*RestoredEntry, SerializedTypeOperand),
			TEXT("StaticJIT.PrecompiledData reference-copy remap restored serialized bytecode should contain a typed reference-copy instruction")));
		ASSERT_THAT(AreEqual(
			SourceTypePointer,
			SerializedTypeOperand,
			TEXT("StaticJIT.PrecompiledData reference-copy serialized operand should retain the source type before reader remapping")));

		asITypeInfo* const RestoredType = RestoredEngine->GetScriptEngine()->GetTypeInfoByDecl("UObject");
		ASSERT_THAT(IsNotNull(RestoredType, TEXT("StaticJIT.PrecompiledData reference-copy remap should publish the restored type")));
		const asPWORD RestoredTypePointer = reinterpret_cast<asPWORD>(RestoredType);
		ASSERT_THAT(AreNotEqual(
			SourceTypePointer,
			RestoredTypePointer,
			TEXT("StaticJIT.PrecompiledData reference-copy source and restored engines should own distinct type objects")));

		SerializedEntry->Process(LoadedData, static_cast<asCScriptFunction*>(RestoredEntry));

		asPWORD RestoredTypeOperand = 0;
		ASSERT_THAT(IsTrue(
			FindReferenceCopyTypeOperand(*RestoredEntry, RestoredTypeOperand),
			TEXT("StaticJIT.PrecompiledData reference-copy remapped bytecode should retain a typed reference-copy instruction")));
		ASSERT_THAT(AreEqual(
			RestoredTypePointer,
			RestoredTypeOperand,
			TEXT("StaticJIT.PrecompiledData reference-copy operand should remap to the restored engine type")));

		asIScriptContext* const Context = RestoredEngine->GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("StaticJIT.PrecompiledData reference-copy remap should create an execution context")));
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		ASSERT_THAT(IsTrue(
			Context->Prepare(RestoredEntry) >= 0,
			TEXT("StaticJIT.PrecompiledData reference-copy remap should prepare the restored entry function")));
		ASSERT_THAT(IsTrue(
			Context->SetArgObject(0, nullptr) >= 0,
			TEXT("StaticJIT.PrecompiledData reference-copy remap should bind a null registered reference argument")));
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED,
			Context->Execute(),
			TEXT("StaticJIT.PrecompiledData reference-copy remap should execute the restored entry function")));
		ASSERT_THAT(AreEqual(
			static_cast<asDWORD>(1),
			Context->GetReturnDWord(),
			TEXT("StaticJIT.PrecompiledData reference-copy remap should preserve the null reference result after load")));
	}
};

bool FAngelscriptPrecompiledDataArchiveTests::RunBuildIdentifierValidation(FAutomationTestBase& Test)
{

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
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			ModuleName,
			SourceFilename,
			ScriptSource);
		if (!Test.TestTrue(TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should compile the archive fixture module"), bCompiled))
		{
			break;
		}

		FAngelscriptPrecompiledData Snapshot(Engine.GetScriptEngine());
		Snapshot.InitFromActiveScript();

		const FString ModuleNameString = ModuleName.ToString();
		if (!Test.TestEqual(
				TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should stamp the snapshot with the current build identifier"),
				Snapshot.BuildIdentifier,
				Snapshot.GetCurrentBuildIdentifier())
			|| !Test.TestTrue(
				TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should serialize the newly compiled module into the snapshot"),
				Snapshot.Modules.Contains(ModuleNameString)))
		{
			Test.AddInfo(FString::Printf(TEXT("Observed saved precompiled modules: [%s]"), *DescribeSavedModuleNames(Snapshot)));
			break;
		}

		FScopedTempPrecompiledCacheFile CacheFile(TEXT("PrecompiledDataBuildIdentifierValidation"));
		TUniquePtr<FAngelscriptPrecompiledData> LoadedData;
		FString SaveAndReloadError;
		const bool bRoundtripped = SaveAndReloadPrecompiledData(
			&Engine,
			Snapshot,
			CacheFile.GetFilename(),
			LoadedData,
			&SaveAndReloadError);
		if (!Test.TestTrue(TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should roundtrip the archive through Save/Load"), bRoundtripped))
		{
			if (!SaveAndReloadError.IsEmpty())
			{
				Test.AddError(SaveAndReloadError);
			}
			break;
		}

		if (!Test.TestNotNull(TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should load a new precompiled data instance from disk"), LoadedData.Get()))
		{
			break;
		}

		if (!ValidateRoundtripSnapshot(Test, Snapshot, *LoadedData, ModuleNameString))
		{
			break;
		}

		const int32 CurrentBuildIdentifier = LoadedData->GetCurrentBuildIdentifier();
		if (!Test.TestTrue(
				TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should run in a known UE build configuration"),
				CurrentBuildIdentifier != -1))
		{
			break;
		}

		LoadedData->BuildIdentifier = MakeInvalidBuildIdentifier(CurrentBuildIdentifier);
		if (!Test.TestFalse(
				TEXT("StaticJIT.PrecompiledData.BuildIdentifierValidation should reject archives whose BuildIdentifier no longer matches the active build"),
				LoadedData->IsValidForCurrentBuild()))
		{
			break;
		}

		if (!SimulateEngineStartupDiscard(Test, LoadedData))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

bool FAngelscriptPrecompiledDataArchiveTests::RunGlobalReferenceNameReuse(FAutomationTestBase& Test)
{
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
	if (!CompileGlobalReferenceFixture(Test, Engine, FixtureModuleName, TEXT("PrecompiledDataGlobalReferenceNameReuse.as")))
	{
		return false;
	}

	asIScriptModule* Module = FindCompiledModule(Test, Engine, FixtureModuleName);
	void* GlobalAddress = FindGlobalVariableAddress(Test, Module, "ReferencedGlobal");
	if (!Test.TestNotNull(TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should find the global address"), GlobalAddress))
	{
		return false;
	}

	FAngelscriptPrecompiledData Snapshot(Engine.GetScriptEngine());
	Snapshot.InitFromActiveScript();

	int64 FirstReference = 0;
	int64 ReusedReference = 0;
	FString FirstName;
	FString ReusedName;
	if (!Test.TestTrue(
			TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should resolve and reuse a global reference"),
			FStaticJITDiagnostics::ReferenceGlobalVariableTwice(Snapshot, GlobalAddress, FirstReference, ReusedReference, FirstName, ReusedName)))
	{
		return false;
	}

	Test.TestEqual(TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should reuse the same reference id"), ReusedReference, FirstReference);
	Test.TestEqual(TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should return the stable name when an existing global reference is reused"), ReusedName, FirstName);
	Test.TestEqual(TEXT("StaticJIT.PrecompiledData.GlobalReferenceNameReuse should keep the script global name stable"), ReusedName, TEXT("ReferencedGlobal"));
	return true;
}

bool FAngelscriptPrecompiledDataArchiveTests::RunRepeatedLoadClearsRuntimeCache(FAutomationTestBase& Test)
{
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
	if (!CompileGlobalReferenceFixture(Test, Engine, FixtureModuleName, TEXT("PrecompiledDataRepeatedLoadClearsRuntimeCache.as")))
	{
		return false;
	}

	asIScriptModule* Module = FindCompiledModule(Test, Engine, FixtureModuleName);
	void* GlobalAddress = FindGlobalVariableAddress(Test, Module, "ReferencedGlobal");
	if (!Test.TestNotNull(TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should find the global address"), GlobalAddress))
	{
		return false;
	}

	FAngelscriptPrecompiledData Snapshot(Engine.GetScriptEngine());
	Snapshot.InitFromActiveScript();
	int64 GlobalReference = 0;
	int64 ReusedReference = 0;
	FString FirstName;
	FString ReusedName;
	if (!Test.TestTrue(
			TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should resolve a stable global reference"),
			FStaticJITDiagnostics::ReferenceGlobalVariableTwice(Snapshot, GlobalAddress, GlobalReference, ReusedReference, FirstName, ReusedName)))
	{
		return false;
	}

	FScopedTempPrecompiledCacheFile CacheFile(TEXT("PrecompiledDataRepeatedLoadClearsRuntimeCache"));
	TUniquePtr<FAngelscriptPrecompiledData> LoadedData;
	FString SaveAndReloadError;
	if (!Test.TestTrue(
			TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should roundtrip the fixture cache"),
			SaveAndReloadPrecompiledData(&Engine, Snapshot, CacheFile.GetFilename(), LoadedData, &SaveAndReloadError)))
	{
		if (!SaveAndReloadError.IsEmpty())
		{
			Test.AddError(SaveAndReloadError);
		}
		return false;
	}

	if (!Test.TestNotNull(TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should load precompiled data"), LoadedData.Get()))
	{
		return false;
	}

	void* FirstResolvedAddress = nullptr;
	void* SecondResolvedAddress = nullptr;
	bool bCacheClearedAfterLoad = false;
	if (!Test.TestTrue(
			TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should re-resolve JIT refs and clear pointer cache after repeated Load"),
			FStaticJITDiagnostics::ExerciseRepeatedGlobalReferenceLoad(*LoadedData, CacheFile.GetFilename(), GlobalReference, FirstResolvedAddress, SecondResolvedAddress, bCacheClearedAfterLoad)))
	{
		return false;
	}

	Test.TestTrue(TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should clear pointer cache on repeated Load"), bCacheClearedAfterLoad);
	Test.TestTrue(
		TEXT("StaticJIT.PrecompiledData.RepeatedLoadClearsRuntimeCache should re-resolve globals after repeated Load"),
		SecondResolvedAddress == FirstResolvedAddress);
	return true;
}

#endif
