#include "CQTest.h"

#include "Components/TimelineComponent.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "Features/IModularFeatures.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UHT/AngelscriptCrossModuleBindings.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/UObjectGlobals.h"

#include <cstddef>
#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptCrossModulePublicHeaderTests;
struct FAngelscriptCrossModuleLayoutVersionTests;
struct FAngelscriptCrossModuleResolverTests;
struct FAngelscriptCrossModuleDirectBindProbeTests;
struct FAngelscriptCrossModuleGenerationProfileTests;
struct FAngelscriptCrossModuleDefaultOffTests;

TEST_CLASS_WITH_FLAGS(FAngelscriptCrossModuleLinkProbeTests,
	"Angelscript.CppTests.UHTToolResolver.LinkProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	friend struct FAngelscriptCrossModulePublicHeaderTests;
	friend struct FAngelscriptCrossModuleLayoutVersionTests;
	friend struct FAngelscriptCrossModuleResolverTests;
	friend struct FAngelscriptCrossModuleDirectBindProbeTests;
	friend struct FAngelscriptCrossModuleGenerationProfileTests;
	friend struct FAngelscriptCrossModuleDefaultOffTests;

	static constexpr uint32 ProbeLayoutVersion = 0xA5C0DE02u;
	static FAutomationTestBase* GActiveTest;

	static bool TestTrue(const TCHAR* What, bool bValue) { return GActiveTest->TestTrue(What, bValue); }
	static bool TestTrue(const FString& What, bool bValue) { return GActiveTest->TestTrue(*What, bValue); }
	static bool TestFalse(const TCHAR* What, bool bValue) { return GActiveTest->TestFalse(What, bValue); }
	static bool TestFalse(const FString& What, bool bValue) { return GActiveTest->TestFalse(*What, bValue); }
	template <typename ExpectedType, typename ActualType>
	static bool TestEqual(const TCHAR* What, const ExpectedType& Expected, const ActualType& Actual) { return GActiveTest->TestEqual(What, Expected, Actual); }
	template <typename ExpectedType, typename ActualType>
	static bool TestEqual(const FString& What, const ExpectedType& Expected, const ActualType& Actual) { return GActiveTest->TestEqual(*What, Expected, Actual); }
	template <typename ValueType>
	static bool TestNotNull(const TCHAR* What, ValueType* Value) { return GActiveTest->TestNotNull(What, Value); }
	template <typename ValueType>
	static bool TestNull(const TCHAR* What, ValueType* Value) { return GActiveTest->TestNull(What, Value); }

	static_assert(std::is_empty<IModularFeature>::value, "IModularFeature must stay empty for the probe reader layout.");
	static_assert(!std::is_polymorphic<IModularFeature>::value, "IModularFeature must stay non-polymorphic for the probe reader layout.");
	static_assert(std::is_standard_layout<FAngelscriptCrossModuleEntry>::value, "Cross-module entry must stay standard-layout.");
	static_assert(std::is_standard_layout<FAngelscriptCrossModuleFeatureReader>::value, "Cross-module reader must stay standard-layout.");
	static_assert(FAngelscriptCrossModuleBindings::LayoutVersionExpected == ProbeLayoutVersion, "Cross-module layout token drifted.");
	static_assert(FAngelscriptCrossModuleBindings::FlagStatic == (1u << 0), "Cross-module static flag drifted.");
	static_assert(FAngelscriptCrossModuleBindings::FlagConst == (1u << 1), "Cross-module const flag drifted.");
	static_assert(FAngelscriptCrossModuleBindings::FlagWorldContext == (1u << 2), "Cross-module world-context flag drifted.");
	static_assert(FAngelscriptCrossModuleBindings::FlagHasOutParams == (1u << 3), "Cross-module out-param flag drifted.");
	static_assert(FAngelscriptCrossModuleBindings::FlagReturnByRef == (1u << 4), "Cross-module return-by-ref flag drifted.");

	struct FProbeEntryReader
	{
		const TCHAR* Tag;
		uint32 Magic;
	};

	struct FProbeFeatureReader
	{
		const FProbeEntryReader* Entries;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;
	};

	struct FProbeFeatureLayoutProbe : public IModularFeature
	{
		const FProbeEntryReader* Entries;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;
	};

	struct FCrossModuleFeatureLayoutProbe : public IModularFeature
	{
		const FAngelscriptCrossModuleEntry* Table;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;
	};

	static_assert(std::is_standard_layout<FProbeFeatureLayoutProbe>::value, "Probe feature layout must stay standard-layout.");
	static_assert(offsetof(FProbeFeatureLayoutProbe, Entries) == offsetof(FProbeFeatureReader, Entries), "Probe Entries offset drifted.");
	static_assert(offsetof(FProbeFeatureLayoutProbe, Count) == offsetof(FProbeFeatureReader, Count), "Probe Count offset drifted.");
	static_assert(offsetof(FProbeFeatureLayoutProbe, ModuleName) == offsetof(FProbeFeatureReader, ModuleName), "Probe ModuleName offset drifted.");
	static_assert(offsetof(FProbeFeatureLayoutProbe, LayoutVersion) == offsetof(FProbeFeatureReader, LayoutVersion), "Probe LayoutVersion offset drifted.");
	static_assert(std::is_standard_layout<FCrossModuleFeatureLayoutProbe>::value, "Cross-module feature layout must stay standard-layout.");
	static_assert(offsetof(FCrossModuleFeatureLayoutProbe, Table) == offsetof(FAngelscriptCrossModuleFeatureReader, Table), "Cross-module Table offset drifted.");
	static_assert(offsetof(FCrossModuleFeatureLayoutProbe, Count) == offsetof(FAngelscriptCrossModuleFeatureReader, Count), "Cross-module Count offset drifted.");
	static_assert(offsetof(FCrossModuleFeatureLayoutProbe, ModuleName) == offsetof(FAngelscriptCrossModuleFeatureReader, ModuleName), "Cross-module ModuleName offset drifted.");
	static_assert(offsetof(FCrossModuleFeatureLayoutProbe, LayoutVersion) == offsetof(FAngelscriptCrossModuleFeatureReader, LayoutVersion), "Cross-module LayoutVersion offset drifted.");

	static FString GetAngelscriptPluginDirectory()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Angelscript"));
	}

	static FString GetCrossModuleLayoutVersionFilePath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptUHTTool/cross-module-layout-version.txt"));
	}

	static FString GetCrossModuleGenerationModulesFilePath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptUHTTool/cross-module-generation-modules.json"));
	}

	static FString GetCrossModulePublicHeaderPath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptRuntime/Public/UHT/AngelscriptCrossModuleBindings.h"));
	}

	static FString GetRuntimeBuildCsPath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs"));
	}

	static FString GetUhtCodeGeneratorPath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionTableCodeGenerator.cs"));
	}

	static FString GetGeneratedUhtOutputDirectory()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
	}

	static FString GetGeneratedUhtFilePath(const TCHAR* FileName)
	{
		return FPaths::Combine(GetGeneratedUhtOutputDirectory(), FileName);
	}

	static UClass* ResolveCrossModuleClassByName(const TCHAR* ModuleName, const TCHAR* ClassName)
	{
		if (ClassName == nullptr)
		{
			return nullptr;
		}

		if (ModuleName != nullptr)
		{
			const FString FullPath = FString::Printf(TEXT("/Script/%s.%s"), ModuleName, ClassName);
			if (UClass* Class = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *FullPath, EFindObjectFlags::ExactClass)))
			{
				return Class;
			}
		}

		return Cast<UClass>(StaticFindFirstObject(UClass::StaticClass(), ClassName, EFindFirstObjectOptions::ExactClass | EFindFirstObjectOptions::NativeFirst));
	}

	static UClass* ResolveCrossModuleClass(const FAngelscriptCrossModuleEntry& Entry, const FAngelscriptCrossModuleFeatureReader& Reader)
	{
		if (UClass* Class = ResolveCrossModuleClassByName(Reader.ModuleName, Entry.ClassName))
		{
			return Class;
		}

		if (Entry.ClassName != nullptr && (Entry.ClassName[0] == TEXT('U') || Entry.ClassName[0] == TEXT('A')) && Entry.ClassName[1] != TEXT('\0'))
		{
			return ResolveCrossModuleClassByName(Reader.ModuleName, Entry.ClassName + 1);
		}

		return nullptr;
	}

	static FString GetEngineManualCrossModuleShardPath()
	{
		return FPaths::Combine(FPaths::EngineDir(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/Engine/UHT/AS_FunctionTable_Engine_CrossModule_Manual.cpp"));
	}

	static FString GetEngineAutomaticCrossModuleShardPath()
	{
		return FPaths::Combine(FPaths::EngineDir(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/Engine/UHT/AS_FunctionTable_Engine_CrossModule_000.cpp"));
	}

	static bool LoadEngineCrossModuleShardContents(FString& OutContents)
	{
		if (FFileHelper::LoadFileToString(OutContents, *GetEngineAutomaticCrossModuleShardPath()))
		{
			return true;
		}

		return FFileHelper::LoadFileToString(OutContents, *GetEngineManualCrossModuleShardPath());
	}

	static bool ContainsSafeReturnValueCrossModuleThunk(const FString& ShardContents)
	{
		return ShardContents.Contains(TEXT("FCrossModuleCallFrame* Frame")) &&
			ShardContents.Contains(TEXT("Frame->ReturnSlot")) &&
			ShardContents.Contains(TEXT("*static_cast<")) &&
			ShardContents.Contains(TEXT("*>(Frame->ReturnSlot) = ")) &&
			ShardContents.Contains(TEXT(", sizeof("));
	}

	static bool ContainsArgumentMarshallingCrossModuleThunk(const FString& ShardContents)
	{
		return !ShardContents.Contains(TEXT("FCrossModuleCallFrame* /*Frame*/")) &&
			ShardContents.Contains(TEXT("PassCrossModuleArg<")) &&
			ShardContents.Contains(TEXT("(Frame, 0)")) &&
			ShardContents.Contains(TEXT("Frame == nullptr")) &&
			ShardContents.Contains(TEXT("Frame->ArgSlots == nullptr")) &&
			ShardContents.Contains(TEXT("TIsReferenceType")) &&
			ShardContents.Contains(TEXT("TIsPointer"));
	}

	static bool ContainsNonTrivialReturnConstruction(const FString& ShardContents)
	{
		return ShardContents.Contains(TEXT("new (Frame->ReturnSlot)")) &&
			ShardContents.Contains(TEXT("BuildCrossModuleReturn")) &&
			ShardContents.Contains(TEXT("BuildCrossModuleReturn<FVector>"));
	}

	static bool ContainsNonZeroArgCrossModuleTableEntry(const FString& ShardContents)
	{
		TArray<FString> Lines;
		ShardContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			if (!Line.Contains(TEXT("&Call_")))
			{
				continue;
			}

			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT(","), false);
			if (Fields.Num() < 5)
			{
				continue;
			}

			const int32 ArgCount = FCString::Atoi(*Fields[3].TrimStartAndEnd());
			if (ArgCount > 0)
			{
				return true;
			}
		}

		return false;
	}

	static FString FormatLayoutVersionToken(uint32 Version)
	{
		return FString::Printf(TEXT("0x%08X"), Version);
	}

	static bool LoadLayoutVersionToken(FString& OutToken)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *GetCrossModuleLayoutVersionFilePath()))
		{
			return false;
		}

		TArray<FString> Lines;
		Contents.ParseIntoArrayLines(Lines);
		for (FString Line : Lines)
		{
			Line = Line.TrimStartAndEnd();
			if (!Line.IsEmpty() && !Line.StartsWith(TEXT("#")))
			{
				OutToken = Line;
				return true;
			}
		}

		return false;
	}

	static TSet<FString> ExtractDependencyModuleNames(const FString& BuildCsContents)
	{
		TArray<FString> Lines;
		BuildCsContents.ParseIntoArrayLines(Lines);

		TSet<FString> ModuleNames;
		bool bInDependencyBlock = false;
		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			if (Line.Contains(TEXT("DependencyModuleNames.AddRange")))
			{
				bInDependencyBlock = true;
			}
			if (!bInDependencyBlock)
			{
				continue;
			}

			int32 SearchIndex = 0;
			while (SearchIndex < Line.Len())
			{
				const int32 OpenQuote = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchIndex);
				if (OpenQuote == INDEX_NONE)
				{
					break;
				}

				const int32 CloseQuote = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenQuote + 1);
				if (CloseQuote == INDEX_NONE)
				{
					break;
				}

				ModuleNames.Add(Line.Mid(OpenQuote + 1, CloseQuote - OpenQuote - 1));
				SearchIndex = CloseQuote + 1;
			}

			if (Line.Contains(TEXT("});")))
			{
				bInDependencyBlock = false;
			}
		}

		return ModuleNames;
	}

	static bool CsvContainsRowWithPrefix(const FString& CsvContents, const FString& Prefix)
	{
		TArray<FString> Lines;
		CsvContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(Prefix))
			{
				return true;
			}
		}

		return false;
	}

	static bool TryParseCsvLine(const FString& Line, TArray<FString>& OutFields)
	{
		OutFields.Reset();
		FString Field;
		bool bInQuotes = false;
		for (int32 Index = 0; Index < Line.Len(); ++Index)
		{
			const TCHAR Character = Line[Index];
			if (Character == TEXT('"'))
			{
				if (bInQuotes && Index + 1 < Line.Len() && Line[Index + 1] == TEXT('"'))
				{
					Field.AppendChar(TEXT('"'));
					++Index;
				}
				else
				{
					bInQuotes = !bInQuotes;
				}
			}
			else if (Character == TEXT(',') && !bInQuotes)
			{
				OutFields.Add(Field);
				Field.Reset();
			}
			else
			{
				Field.AppendChar(Character);
			}
		}

		if (bInQuotes)
		{
			OutFields.Reset();
			return false;
		}

		OutFields.Add(Field);
		return true;
	}

	static bool TryExtractFirstCrossModuleCsvIdentity(const FString& EntriesContents, FString& OutModuleName, FString& OutClassName, FString& OutFunctionName)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			if (!TryParseCsvLine(Line, Fields))
			{
				continue;
			}

			if (Fields.Num() >= 5 && Fields[4] == TEXT("CrossModule"))
			{
				OutModuleName = Fields[0];
				OutClassName = Fields[2];
				OutFunctionName = Fields[3];
				return true;
			}
		}

		return false;
	}

	static bool CsvContainsEntryKindForIdentity(const FString& EntriesContents, const FString& ModuleName, const FString& ClassName, const FString& FunctionName, const FString& EntryKind)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			if (!TryParseCsvLine(Line, Fields))
			{
				continue;
			}

			if (Fields.Num() >= 5 && Fields[0] == ModuleName && Fields[2] == ClassName && Fields[3] == FunctionName && Fields[4] == EntryKind)
			{
				return true;
			}
		}

		return false;
	}

	static bool CsvContainsThunkStyleForIdentity(const FString& EntriesContents, const FString& ModuleName, const FString& ClassName, const FString& FunctionName, const FString& ThunkStyle)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			if (!TryParseCsvLine(Line, Fields))
			{
				continue;
			}

			if (Fields.Num() >= 8 && Fields[0] == ModuleName && Fields[2] == ClassName && Fields[3] == FunctionName && Fields[7] == ThunkStyle)
			{
				return true;
			}
		}

		return false;
	}

	static bool CsvContainsModuleEntryKind(const FString& EntriesContents, const FString& ModuleName, const FString& EntryKind)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			if (!TryParseCsvLine(Line, Fields))
			{
				continue;
			}

			if (Fields.Num() >= 5 && Fields[0] == ModuleName && Fields[4] == EntryKind)
			{
				return true;
			}
		}

		return false;
	}

	static bool CsvContainsModuleThunkStyle(const FString& EntriesContents, const FString& ModuleName, const FString& ThunkStyle)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			if (!TryParseCsvLine(Line, Fields))
			{
				continue;
			}

			if (Fields.Num() >= 8 && Fields[0] == ModuleName && Fields[7] == ThunkStyle)
			{
				return true;
			}
		}

		return false;
	}

	static bool CsvContainsSkippedReasonForModule(const FString& SkippedEntriesContents, const FString& ModuleName, const FString& FailureReason)
	{
		TArray<FString> Lines;
		SkippedEntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			if (!TryParseCsvLine(Line, Fields))
			{
				continue;
			}

			if (Fields.Num() >= 4 && Fields[0] == ModuleName && Fields[3] == FailureReason)
			{
				return true;
			}
		}

		return false;
	}

	static bool TryExtractFirstRpcStubCsvIdentity(const FString& EntriesContents, FString& OutModuleName, FString& OutClassName, FString& OutFunctionName)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			if (!TryParseCsvLine(Line, Fields))
			{
				continue;
			}

			if (Fields.Num() >= 6 && Fields[4] == TEXT("Stub") && Fields[5].Contains(TEXT("ERASE_NO_FUNCTION")) &&
				(Fields[3].StartsWith(TEXT("Client")) || Fields[3].Contains(TEXT("Multicast"))))
			{
				OutModuleName = Fields[0];
				OutClassName = Fields[2];
				OutFunctionName = Fields[3];
				return true;
			}
		}

		return false;
	}

	static bool RunLinkProbeRoundtrip(FAutomationTestBase& Test);
	static bool RunPublicHeader(FAutomationTestBase& Test);
	static bool RunLayoutVersionSingleSource(FAutomationTestBase& Test);
	static bool RunSkippedStatistics(FAutomationTestBase& Test);
	static bool RunNoLongerUnexportedSymbol(FAutomationTestBase& Test);
	static bool RunStaticAssertSizeofConsistency(FAutomationTestBase& Test);
	static bool RunStaleCleanupBoundaries(FAutomationTestBase& Test);
	static bool RunScriptProjectionSafetyGate(FAutomationTestBase& Test);
	static bool RunAutomaticEntryVisible(FAutomationTestBase& Test);
	static bool RunBuildCsDependencyBoundary(FAutomationTestBase& Test);
	static bool RunGenerationProfilesPolicy(FAutomationTestBase& Test);
	static bool RunGenerationProfilesEntries(FAutomationTestBase& Test);
	static bool RunDefaultOffDiagnostics(FAutomationTestBase& Test);
	static bool RunDefaultOffGeneratedOutput(FAutomationTestBase& Test);

public:
	TEST_METHOD(IModularFeaturesRoundtrip)
	{
		ASSERT_THAT(IsTrue(RunLinkProbeRoundtrip(*TestRunner)));
	}
};

FAutomationTestBase* FAngelscriptCrossModuleLinkProbeTests::GActiveTest = nullptr;

bool FAngelscriptCrossModuleLinkProbeTests::RunLinkProbeRoundtrip(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FName(TEXT("AngelscriptCrossModuleLinkProbe")));
	return TestEqual(TEXT("Engine module link probe feature should not be registered while cross-module generation is disabled by default"), Features.Num(), 0);
}

bool FAngelscriptCrossModuleLinkProbeTests::RunPublicHeader(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString HeaderContents;
	if (!TestTrue(TEXT("Cross-module public ABI header should be readable"), FFileHelper::LoadFileToString(HeaderContents, *GetCrossModulePublicHeaderPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Public ABI header should declare the cross-module call frame"), HeaderContents.Contains(TEXT("struct FAngelscriptCrossModuleCallFrame")));
	bPassed &= TestTrue(TEXT("Public ABI header should expose frame-based thunk pointers"), HeaderContents.Contains(TEXT("FAngelscriptCrossModuleCallFrame* Frame")));
	bPassed &= TestTrue(TEXT("Public ABI header should assert cross-module call-frame ABI size"), HeaderContents.Contains(TEXT("static_assert(sizeof(FAngelscriptCrossModuleCallFrame) == 48")));
	bPassed &= TestFalse(TEXT("Public ABI header should not expose raw Args/Ret thunk pointers"), HeaderContents.Contains(TEXT("void** Args, void* Ret")));
	bPassed &= TestFalse(TEXT("Public ABI header should not include AngelscriptBinds"), HeaderContents.Contains(TEXT("AngelscriptBinds.h")));
	bPassed &= TestFalse(TEXT("Public ABI header should not include FunctionCallers"), HeaderContents.Contains(TEXT("FunctionCallers.h")));
	bPassed &= TestFalse(TEXT("Public ABI header should not include angelscript SDK"), HeaderContents.Contains(TEXT("angelscript.h")));
	bPassed &= TestFalse(TEXT("Public ABI header should not expose FAngelscriptBinds"), HeaderContents.Contains(TEXT("FAngelscriptBinds")));
	bPassed &= TestFalse(TEXT("Public ABI header should not expose ASAutoCaller"), HeaderContents.Contains(TEXT("ASAutoCaller")));
	bPassed &= TestFalse(TEXT("Public ABI header should not expose FGenericFuncPtr"), HeaderContents.Contains(TEXT("FGenericFuncPtr")));
	bPassed &= TestEqual(TEXT("Cross-module entry ABI size should match"), static_cast<int32>(sizeof(FAngelscriptCrossModuleEntry)), 32);
	bPassed &= TestEqual(TEXT("Cross-module reader ABI size should match"), static_cast<int32>(sizeof(FAngelscriptCrossModuleFeatureReader)), 32);
	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunLayoutVersionSingleSource(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString VersionToken;
	if (!TestTrue(TEXT("Cross-module layout version file should expose a token"), LoadLayoutVersionToken(VersionToken)))
	{
		return false;
	}

	const FString ExpectedHeaderToken = FormatLayoutVersionToken(FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	bool bPassed = TestEqual(TEXT("Layout version file should match public header token"), VersionToken, ExpectedHeaderToken);

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bPassed &= TestTrue(TEXT("Generator should still build cross-module shards when explicitly enabled"), GeneratorContents.Contains(TEXT("BuildCrossModuleShard")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should embed the shared layout token"), GeneratorContents.Contains(TEXT("GCrossModuleLayoutVersion = {layoutVersion}u")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should mirror the frame ABI"), GeneratorContents.Contains(TEXT("struct FCrossModuleCallFrame")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should use frame-based thunk pointers"), GeneratorContents.Contains(TEXT("FCrossModuleCallFrame* Frame")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should read arguments from frame slots"), GeneratorContents.Contains(TEXT("Frame->ArgSlots")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should write returns through the frame return slot"), GeneratorContents.Contains(TEXT("Frame->ReturnSlot")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should assert cross-module call-frame ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FCrossModuleCallFrame) == 48")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should assert cross-module entry ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FCrossModuleEntry) == 32")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should assert cross-module feature ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FCrossModuleFeature) == 32")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should pass the shared layout token into feature registration"), GeneratorContents.Contains(TEXT("GCrossModuleFeature(GCrossModuleTable, UE_ARRAY_COUNT(GCrossModuleTable), TEXT(\\\"")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should include return-value thunk support"), GeneratorContents.Contains(TEXT("BuildCrossModuleReturn")));
	bPassed &= TestTrue(TEXT("Cross-module shard template should include argument marshalling helpers"), GeneratorContents.Contains(TEXT("PassCrossModuleArg<")));
	bPassed &= TestFalse(TEXT("Cross-module shard template should not use exported getter link path"), GeneratorContents.Contains(TEXT("Get_AS_Bindings_")));
	bPassed &= TestFalse(TEXT("Cross-module shard template should not expose raw Args/Ret thunk signatures"), GeneratorContents.Contains(TEXT("void** Args, void* Ret")));
	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunSkippedStatistics(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString SummaryContents;
	if (!TestTrue(TEXT("Skipped reason summary should be readable"), FFileHelper::LoadFileToString(SummaryContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_SkippedReasonSummary.csv")))))
	{
		return false;
	}

	FString SkippedEntriesContents;
	if (!TestTrue(TEXT("Skipped entries should be readable"), FFileHelper::LoadFileToString(SkippedEntriesContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_SkippedEntries.csv")))))
	{
		return false;
	}

	FString EntriesContents;
	if (!TestTrue(TEXT("Generated entries should be readable"), FFileHelper::LoadFileToString(EntriesContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_Entries.csv")))))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("WorldContext cross-module candidates should identify missing policy"), SummaryContents.Contains(TEXT("needs-world-context-policy,")));
	bPassed &= TestTrue(TEXT("Out/ref cross-module candidates should identify missing protocol"), SummaryContents.Contains(TEXT("needs-out-param-protocol,")));
	bPassed &= TestTrue(TEXT("Ref-return cross-module candidates should identify missing protocol"), SummaryContents.Contains(TEXT("needs-ref-return-protocol,")));
	bPassed &= TestTrue(TEXT("Container cross-module candidates should identify missing frame protocol"), SummaryContents.Contains(TEXT("needs-container-frame-protocol,")));
	bPassed &= TestTrue(TEXT("Interface cross-module candidates should identify missing frame protocol"), SummaryContents.Contains(TEXT("needs-interface-frame-protocol,")));
	bPassed &= TestTrue(TEXT("Delegate cross-module candidates should identify missing frame protocol"), SummaryContents.Contains(TEXT("needs-delegate-frame-protocol,")));
	bPassed &= TestTrue(TEXT("FieldPath cross-module candidates should identify missing frame protocol"), SummaryContents.Contains(TEXT("needs-field-path-frame-protocol,")));
	bPassed &= TestTrue(TEXT("ScriptMethod/ScriptMixin projections should identify missing receiver projection"), SummaryContents.Contains(TEXT("needs-script-this-projection,")));
	bPassed &= TestFalse(TEXT("Enabled cross-module unsupported signatures should be split into protocol reasons"), CsvContainsRowWithPrefix(SummaryContents, TEXT("cross-module-unsupported-signature,")));
	bPassed &= TestFalse(TEXT("Disabled target modules should not remain lumped into target-module-disabled when classifiable"), SummaryContents.Contains(TEXT("target-module-disabled,")));
	bPassed &= TestTrue(TEXT("Disabled target modules should preserve protocol diagnostics"), SummaryContents.Contains(TEXT("disabled-needs-out-param-protocol,")));
	bPassed &= TestTrue(TEXT("RPC functions should have an explicit skipped reason"), SummaryContents.Contains(TEXT("rpc-net-function,")));
	bPassed &= TestFalse(TEXT("Supported cross-module candidates should no longer be lumped into unexported-symbol"), CsvContainsRowWithPrefix(SummaryContents, TEXT("unexported-symbol,")));
	bPassed &= TestTrue(TEXT("Entries CSV should expose a thunk style column"), EntriesContents.StartsWith(TEXT("ModuleName,EditorOnly,ClassName,FunctionName,EntryKind,EraseMacro,ShardIndex,ThunkStyle")));
	const FString DiagnosticOnlyDisabledModule = TEXT("ControlRigEditor");
	bPassed &= TestFalse(TEXT("Source profile module should not emit cross-module entries while generation is disabled by default"), CsvContainsModuleEntryKind(EntriesContents, DiagnosticOnlyDisabledModule, TEXT("CrossModule")));
	bPassed &= TestTrue(TEXT("Source profile module should remain diagnostic-only disabled-safe-cross-module by default"), CsvContainsSkippedReasonForModule(SkippedEntriesContents, DiagnosticOnlyDisabledModule, TEXT("disabled-safe-cross-module")));

	FString RpcModuleName;
	FString RpcClassName;
	FString RpcFunctionName;
	if (TestTrue(TEXT("Generated entries CSV should keep RPC functions on stub fallback path"), TryExtractFirstRpcStubCsvIdentity(EntriesContents, RpcModuleName, RpcClassName, RpcFunctionName)))
	{
		const FString RpcSkippedIdentity = FString::Printf(TEXT("%s,%s,%s,rpc-net-function"), *RpcModuleName, *RpcClassName, *RpcFunctionName);
		bPassed &= TestTrue(TEXT("RPC stub entry should also appear in skipped diagnostics with rpc-net-function"), SkippedEntriesContents.Contains(RpcSkippedIdentity));
		bPassed &= TestTrue(TEXT("RPC entries should report stub thunk style"), CsvContainsThunkStyleForIdentity(EntriesContents, RpcModuleName, RpcClassName, RpcFunctionName, TEXT("Stub")));
		bPassed &= TestFalse(TEXT("RPC entries should not report frame-wrapper thunk style"), CsvContainsThunkStyleForIdentity(EntriesContents, RpcModuleName, RpcClassName, RpcFunctionName, TEXT("FrameWrapper")));
	}
	else
	{
		bPassed = false;
	}

	FString CrossModuleName;
	FString CrossModuleClassName;
	FString CrossModuleFunctionName;
	bPassed &= TestFalse(TEXT("Generated entries CSV should not expose CrossModule entry kinds while generation is disabled by default"), TryExtractFirstCrossModuleCsvIdentity(EntriesContents, CrossModuleName, CrossModuleClassName, CrossModuleFunctionName));

	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunNoLongerUnexportedSymbol(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString SummaryContents;
	if (!TestTrue(TEXT("Skipped reason summary should be readable"), FFileHelper::LoadFileToString(SummaryContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_SkippedReasonSummary.csv")))))
	{
		return false;
	}

	FString SkippedEntriesContents;
	if (!TestTrue(TEXT("Skipped entries should be readable"), FFileHelper::LoadFileToString(SkippedEntriesContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_SkippedEntries.csv")))))
	{
		return false;
	}

	FString EntriesContents;
	if (!TestTrue(TEXT("Generated entries should be readable"), FFileHelper::LoadFileToString(EntriesContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_Entries.csv")))))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestFalse(TEXT("Cross-module candidates should not be summarized as unexported-symbol"), CsvContainsRowWithPrefix(SummaryContents, TEXT("unexported-symbol,")));
	bPassed &= TestFalse(TEXT("Cross-module candidates should not be recorded as EntryKind=CrossModule while generation is disabled by default"), EntriesContents.Contains(TEXT(",CrossModule,")));
	bPassed &= TestTrue(TEXT("Skipped diagnostics should retain disabled-safe-cross-module candidates by default"), SkippedEntriesContents.Contains(TEXT("disabled-safe-cross-module")));
	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunStaticAssertSizeofConsistency(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	static_assert(sizeof(FAngelscriptCrossModuleEntry) == 32, "FAngelscriptCrossModuleEntry size must match generator-emitted ABI.");
	static_assert(sizeof(FAngelscriptCrossModuleFeatureReader) == 32, "FAngelscriptCrossModuleFeatureReader size must match generator-emitted ABI.");

	FString HeaderContents;
	if (!TestTrue(TEXT("Cross-module public ABI header should be readable"), FFileHelper::LoadFileToString(HeaderContents, *GetCrossModulePublicHeaderPath())))
	{
		return false;
	}

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Public header should assert cross-module entry ABI size"), HeaderContents.Contains(TEXT("static_assert(sizeof(FAngelscriptCrossModuleEntry) == 32")));
	bPassed &= TestTrue(TEXT("Public header should assert cross-module reader ABI size"), HeaderContents.Contains(TEXT("static_assert(sizeof(FAngelscriptCrossModuleFeatureReader) == 32")));
	bPassed &= TestTrue(TEXT("Generated shard template should assert cross-module entry ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FCrossModuleEntry) == 32")));
	bPassed &= TestTrue(TEXT("Generated shard template should assert cross-module feature ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FCrossModuleFeature) == 32")));
	bPassed &= TestFalse(TEXT("Generated shard template should not use variable-padding bool fields in ABI payload"), GeneratorContents.Contains(TEXT("\\t\\tbool ")));
	bPassed &= TestFalse(TEXT("Public header should not use variable-padding bool fields in ABI payload"), HeaderContents.Contains(TEXT("\tbool ")));
	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunStaleCleanupBoundaries(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Stale cleanup should accept all UHT session module output directories"), GeneratorContents.Contains(TEXT("IReadOnlyDictionary<string, string> allModuleOutputDirectories")));
	bPassed &= TestTrue(TEXT("Stale cleanup should enumerate each UHT session module output directory"), GeneratorContents.Contains(TEXT("foreach ((string moduleName, string outputDirectory) in allModuleOutputDirectories)")));
	bPassed &= TestTrue(TEXT("Stale cleanup should delete only matching cross-module shards per module"), GeneratorContents.Contains(TEXT("$\"AS_FunctionTable_{moduleName}_CrossModule_*.cpp\"")));
	bPassed &= TestTrue(TEXT("Stale cleanup should preserve runtime same-module cleanup"), GeneratorContents.Contains(TEXT("DeleteStaleFilesInDirectory(runtimeOutputDirectory, \"AS_FunctionTable_*.cpp\", livePaths)")));
	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunScriptProjectionSafetyGate(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Safe cross-module gate should call the ScriptMethod/ScriptMixin projection guard"), GeneratorContents.Contains(TEXT("!HasScriptMethodMixinProjection(signature, classObj, function)")));
	bPassed &= TestTrue(TEXT("Generator should define a dedicated ScriptMethod/ScriptMixin projection guard"), GeneratorContents.Contains(TEXT("HasScriptMethodMixinProjection")));
	bPassed &= TestTrue(TEXT("ScriptMethod metadata should be recognized as an implicit script-this projection"), GeneratorContents.Contains(TEXT("function.MetaData.ContainsKey(\"ScriptMethod\")")));
	bPassed &= TestTrue(TEXT("ScriptMixin metadata should be recognized as an implicit script-this projection"), GeneratorContents.Contains(TEXT("classObj.MetaData.ContainsKey(\"ScriptMixin\")")));
	bPassed &= TestTrue(TEXT("Only static projected functions require exclusion from the raw thunk safe set"), GeneratorContents.Contains(TEXT("return signature.IsStatic &&")));
	bPassed &= TestTrue(TEXT("Cross-module classification should pass class/function context into the safe-signature gate"), GeneratorContents.Contains(TEXT("IsSafeAutomaticCrossModuleSignature(signature!, classObj, function)")));
	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunAutomaticEntryVisible(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
	if (CurrentEngine == nullptr || CurrentEngine->GetScriptEngine() == nullptr)
	{
		Test.AddWarning(TEXT("AngelScript engine is not initialized; skipping runtime cross-module entry visibility check."));
		return true;
	}

	TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FAngelscriptCrossModuleBindings::FeatureName());
	return TestEqual(TEXT("Automatic cross-module entries should not be injected while generation is disabled by default"), Features.Num(), 0);
}

bool FAngelscriptCrossModuleLinkProbeTests::RunBuildCsDependencyBoundary(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString BuildCsContents;
	if (!TestTrue(TEXT("AngelscriptRuntime.Build.cs should be readable"), FFileHelper::LoadFileToString(BuildCsContents, *GetRuntimeBuildCsPath())))
	{
		return false;
	}

	const TSet<FString> ExpectedDependencyModules = {
		TEXT("ApplicationCore"), TEXT("Core"), TEXT("CoreUObject"), TEXT("Engine"), TEXT("EngineSettings"), TEXT("DeveloperSettings"),
		TEXT("Json"), TEXT("JsonUtilities"), TEXT("StructUtils"), TEXT("AIModule"), TEXT("NavigationSystem"),
		TEXT("NetCore"), TEXT("Landscape"), TEXT("Networking"), TEXT("Sockets"), TEXT("InputCore"), TEXT("SlateCore"), TEXT("Slate"),
		TEXT("UMG"), TEXT("TraceLog"), TEXT("AssetRegistry"), TEXT("Projects"), TEXT("PhysicsCore"), TEXT("CoreOnline"), TEXT("EnhancedInput"),
		TEXT("UnrealEd"), TEXT("EditorSubsystem"), TEXT("UMGEditor") };

	bool bPassed = true;
	const TSet<FString> ActualDependencyModules = ExtractDependencyModuleNames(BuildCsContents);
	for (const FString& ActualModule : ActualDependencyModules)
	{
		if (!ExpectedDependencyModules.Contains(ActualModule))
		{
			Test.AddError(FString::Printf(TEXT("Unexpected AngelscriptRuntime module dependency introduced: %s"), *ActualModule));
			bPassed = false;
		}
	}

	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunGenerationProfilesPolicy(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString ProfileConfigContents;
	if (!TestTrue(TEXT("Cross-module generation profile config should be readable"), FFileHelper::LoadFileToString(ProfileConfigContents, *GetCrossModuleGenerationModulesFilePath())))
	{
		return false;
	}

	FString BuildCsContents;
	if (!TestTrue(TEXT("AngelscriptRuntime.Build.cs should be readable"), FFileHelper::LoadFileToString(BuildCsContents, *GetRuntimeBuildCsPath())))
	{
		return false;
	}

	const TSet<FString> DependencyModules = ExtractDependencyModuleNames(BuildCsContents);
	const TArray<FString> CommonModules = {
		TEXT("GameplayCameras"),
		TEXT("IKRig"),
		TEXT("Niagara"),
		TEXT("GameplayAbilities"),
		TEXT("MovieSceneTracks")
	};
	const TArray<FString> SourceProfileModules = {
		TEXT("ControlRigEditor"),
		TEXT("PythonScriptPlugin"),
		TEXT("NiagaraEditor"),
		TEXT("SequencerScripting"),
		TEXT("EditorScriptingUtilities"),
		TEXT("Paper2D"),
		TEXT("EngineCameras")
	};

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Profile config should default cross-module generation to disabled"), ProfileConfigContents.Contains(TEXT("\"enabled\": false")));
	bPassed &= TestTrue(TEXT("Profile config should declare common profile"), ProfileConfigContents.Contains(TEXT("\"common\"")));
	bPassed &= TestTrue(TEXT("Profile config should declare source profile"), ProfileConfigContents.Contains(TEXT("\"source\"")));
	bPassed &= TestTrue(TEXT("Profile config should declare installed profile"), ProfileConfigContents.Contains(TEXT("\"installed\"")));
	for (const FString& CommonModule : CommonModules)
	{
		bPassed &= TestTrue(FString::Printf(TEXT("Common profile should include pilot module %s"), *CommonModule), ProfileConfigContents.Contains(FString::Printf(TEXT("\"%s\""), *CommonModule)));
		bPassed &= TestFalse(FString::Printf(TEXT("Common profile module %s should not be added to AngelscriptRuntime dependencies"), *CommonModule), DependencyModules.Contains(CommonModule));
	}

	for (const FString& SourceProfileModule : SourceProfileModules)
	{
		bPassed &= TestTrue(FString::Printf(TEXT("Source profile should include disabled-safe module %s"), *SourceProfileModule), ProfileConfigContents.Contains(FString::Printf(TEXT("\"%s\""), *SourceProfileModule)));
		bPassed &= TestFalse(FString::Printf(TEXT("Source profile module %s should not be added to AngelscriptRuntime dependencies"), *SourceProfileModule), DependencyModules.Contains(SourceProfileModule));
	}

	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunGenerationProfilesEntries(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString EntriesContents;
	if (!TestTrue(TEXT("Generated entries should be readable"), FFileHelper::LoadFileToString(EntriesContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_Entries.csv")))))
	{
		return false;
	}

	FString SkippedEntriesContents;
	if (!TestTrue(TEXT("Skipped entries should be readable"), FFileHelper::LoadFileToString(SkippedEntriesContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_SkippedEntries.csv")))))
	{
		return false;
	}

	FString SummaryContents;
	if (!TestTrue(TEXT("Generated summary should be readable"), FFileHelper::LoadFileToString(SummaryContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_Summary.json")))))
	{
		return false;
	}

	const TArray<FString> PilotModules = {
		TEXT("GameplayCameras"),
		TEXT("IKRig"),
		TEXT("Niagara"),
		TEXT("GameplayAbilities"),
		TEXT("MovieSceneTracks")
	};
	const TArray<FString> SourceProfileModules = {
		TEXT("ControlRigEditor"),
		TEXT("PythonScriptPlugin"),
		TEXT("NiagaraEditor"),
		TEXT("SequencerScripting"),
		TEXT("EditorScriptingUtilities"),
		TEXT("Paper2D"),
		TEXT("EngineCameras")
	};

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Summary should report selected cross-module generation profile"), SummaryContents.Contains(TEXT("\"crossModuleGenerationProfile\"")));
	bPassed &= TestTrue(TEXT("Summary should report cross-module generation disabled by default"), SummaryContents.Contains(TEXT("\"crossModuleGenerationEnabled\": false")));
	bPassed &= TestTrue(TEXT("Summary should report cross-module config path"), SummaryContents.Contains(TEXT("\"crossModuleGenerationConfigPath\"")));
	bPassed &= TestTrue(TEXT("Summary should report configured cross-module modules"), SummaryContents.Contains(TEXT("\"crossModuleConfiguredModules\"")));
	bPassed &= TestTrue(TEXT("Summary should report effective cross-module modules"), SummaryContents.Contains(TEXT("\"crossModuleEffectiveModules\"")));
	bPassed &= TestTrue(TEXT("Summary should report no effective cross-module modules while generation is disabled"), SummaryContents.Contains(TEXT("\"crossModuleEffectiveModules\": []")));
	for (const FString& PilotModule : PilotModules)
	{
		bPassed &= TestFalse(FString::Printf(TEXT("Pilot module %s should not emit CrossModule rows by default"), *PilotModule), CsvContainsModuleEntryKind(EntriesContents, PilotModule, TEXT("CrossModule")));
		bPassed &= TestTrue(FString::Printf(TEXT("Pilot module %s should remain diagnostic-only disabled-safe-cross-module by default"), *PilotModule), CsvContainsSkippedReasonForModule(SkippedEntriesContents, PilotModule, TEXT("disabled-safe-cross-module")));
	}

	for (const FString& SourceProfileModule : SourceProfileModules)
	{
		bPassed &= TestFalse(FString::Printf(TEXT("Source profile module %s should not emit CrossModule rows by default"), *SourceProfileModule), CsvContainsModuleEntryKind(EntriesContents, SourceProfileModule, TEXT("CrossModule")));
		bPassed &= TestTrue(FString::Printf(TEXT("Source profile module %s should remain diagnostic-only disabled-safe-cross-module by default"), *SourceProfileModule), CsvContainsSkippedReasonForModule(SkippedEntriesContents, SourceProfileModule, TEXT("disabled-safe-cross-module")));
	}

	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunDefaultOffDiagnostics(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString ProfileConfigContents;
	if (!TestTrue(TEXT("Cross-module generation profile config should be readable"), FFileHelper::LoadFileToString(ProfileConfigContents, *GetCrossModuleGenerationModulesFilePath())))
	{
		return false;
	}

	FString SummaryContents;
	if (!TestTrue(TEXT("Generated summary should be readable"), FFileHelper::LoadFileToString(SummaryContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_Summary.json")))))
	{
		return false;
	}

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Config should declare CrossModule generation disabled by default"), ProfileConfigContents.Contains(TEXT("\"enabled\": false")));
	bPassed &= TestTrue(TEXT("Config should keep source profile modules for explicit opt-in"), ProfileConfigContents.Contains(TEXT("\"ControlRigEditor\"")));
	bPassed &= TestTrue(TEXT("Summary should expose disabled CrossModule generation state"), SummaryContents.Contains(TEXT("\"crossModuleGenerationEnabled\": false")));
	bPassed &= TestTrue(TEXT("Summary should keep selected profile visible"), SummaryContents.Contains(TEXT("\"crossModuleGenerationProfile\"")));
	bPassed &= TestTrue(TEXT("Summary should keep configured module diagnostics visible"), SummaryContents.Contains(TEXT("\"crossModuleConfiguredModules\"")));
	bPassed &= TestTrue(TEXT("Summary should report no effective modules by default"), SummaryContents.Contains(TEXT("\"crossModuleEffectiveModules\": []")));
	bPassed &= TestTrue(TEXT("Generator should model CrossModule generation as an explicit config gate"), GeneratorContents.Contains(TEXT("CrossModuleGenerationEnabled")));
	return bPassed;
}

bool FAngelscriptCrossModuleLinkProbeTests::RunDefaultOffGeneratedOutput(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString EntriesContents;
	if (!TestTrue(TEXT("Generated entries should be readable"), FFileHelper::LoadFileToString(EntriesContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_Entries.csv")))))
	{
		return false;
	}

	FString SkippedEntriesContents;
	if (!TestTrue(TEXT("Skipped entries should be readable"), FFileHelper::LoadFileToString(SkippedEntriesContents, *GetGeneratedUhtFilePath(TEXT("AS_FunctionTable_SkippedEntries.csv")))))
	{
		return false;
	}

	TArray<IModularFeature*> ProbeFeatures = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FName(TEXT("AngelscriptCrossModuleLinkProbe")));
	TArray<IModularFeature*> BindingFeatures = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FAngelscriptCrossModuleBindings::FeatureName());

	bool bPassed = true;
	bPassed &= TestFalse(TEXT("Entries CSV should not contain CrossModule rows by default"), EntriesContents.Contains(TEXT(",CrossModule,")));
	bPassed &= TestTrue(TEXT("Skipped diagnostics should retain opt-in opportunities"), SkippedEntriesContents.Contains(TEXT("disabled-safe-cross-module")));
	bPassed &= TestEqual(TEXT("Engine link probe should not be registered by default"), ProbeFeatures.Num(), 0);
	bPassed &= TestEqual(TEXT("CrossModule binding features should not be registered by default"), BindingFeatures.Num(), 0);
	return bPassed;
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCrossModulePublicHeaderTests,
	"Angelscript.CppTests.UHTToolResolver.PublicHeader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NoASRuntimeOrSDKDeps)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunPublicHeader(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptCrossModuleLayoutVersionTests,
	"Angelscript.CppTests.UHTToolResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LayoutVersionFile_SingleSource_GeneratorAndHeaderInSync)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunLayoutVersionSingleSource(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptCrossModuleResolverTests,
	"Angelscript.CppTests.UHTToolResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NoLongerEmitsUnexportedSymbol_ForCrossModuleCandidate)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunNoLongerUnexportedSymbol(*TestRunner)));
	}

	TEST_METHOD(StaticAssert_SizeofConsistency)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunStaticAssertSizeofConsistency(*TestRunner)));
	}

	TEST_METHOD(StaleCleanup_CrossModuleEnumeratesSupportedModuleDirectories)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunStaleCleanupBoundaries(*TestRunner)));
	}

	TEST_METHOD(BuildCs_NoEngineModuleAddedAsDependency)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunBuildCsDependencyBoundary(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptCrossModuleDirectBindProbeTests,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SkippedStatisticsClassifyCrossModuleOutcomes)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunSkippedStatistics(*TestRunner)));
	}

	TEST_METHOD(ScriptMethodMixinProjection_ExcludedFromAutomaticSafeSet)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunScriptProjectionSafetyGate(*TestRunner)));
	}

	TEST_METHOD(AutomaticEntryVisible)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunAutomaticEntryVisible(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptCrossModuleGenerationProfileTests,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleGenerationProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PolicyFileAndBuildCsBoundary)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunGenerationProfilesPolicy(*TestRunner)));
	}

	TEST_METHOD(GeneratedRowsAreCrossModuleOnly)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunGenerationProfilesEntries(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptCrossModuleDefaultOffTests,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDefaultOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DiagnosticsAndProfileOptIn)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunDefaultOffDiagnostics(*TestRunner)));
	}

	TEST_METHOD(GeneratedOutputsSuppressed)
	{
		ASSERT_THAT(IsTrue(FAngelscriptCrossModuleLinkProbeTests::RunDefaultOffGeneratedOutput(*TestRunner)));
	}
};

#endif
