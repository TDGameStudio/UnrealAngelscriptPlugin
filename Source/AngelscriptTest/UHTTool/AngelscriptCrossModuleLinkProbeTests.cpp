#include "Misc/AutomationTest.h"

#include "Components/TimelineComponent.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "Features/IModularFeatures.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UHT/AngelscriptCrossModuleBindings.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/UObjectGlobals.h"

#include <cstddef>
#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private
{
	constexpr uint32 ProbeLayoutVersion = 0xA5C0DE01u;

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

	FString GetAngelscriptPluginDirectory()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Angelscript"));
	}

	FString GetCrossModuleLayoutVersionFilePath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptUHTTool/cross-module-layout-version.txt"));
	}

	FString GetCrossModulePublicHeaderPath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptRuntime/Public/UHT/AngelscriptCrossModuleBindings.h"));
	}

	FString GetRuntimeBuildCsPath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs"));
	}

	FString GetUhtCodeGeneratorPath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionTableCodeGenerator.cs"));
	}

	FString GetGeneratedUhtOutputDirectory()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
	}

	FString GetGeneratedUhtFilePath(const TCHAR* FileName)
	{
		return FPaths::Combine(GetGeneratedUhtOutputDirectory(), FileName);
	}

	UClass* ResolveCrossModuleClassByName(const TCHAR* ModuleName, const TCHAR* ClassName)
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

	UClass* ResolveCrossModuleClass(const FAngelscriptCrossModuleEntry& Entry, const FAngelscriptCrossModuleFeatureReader& Reader)
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

	FString GetEngineManualCrossModuleShardPath()
	{
		return FPaths::Combine(FPaths::EngineDir(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/Engine/UHT/AS_FunctionTable_Engine_CrossModule_Manual.cpp"));
	}

	FString GetEngineAutomaticCrossModuleShardPath()
	{
		return FPaths::Combine(FPaths::EngineDir(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/Engine/UHT/AS_FunctionTable_Engine_CrossModule_000.cpp"));
	}

	bool LoadEngineCrossModuleShardContents(FString& OutContents)
	{
		if (FFileHelper::LoadFileToString(OutContents, *GetEngineAutomaticCrossModuleShardPath()))
		{
			return true;
		}

		return FFileHelper::LoadFileToString(OutContents, *GetEngineManualCrossModuleShardPath());
	}

	bool ContainsSafeReturnValueCrossModuleThunk(const FString& ShardContents)
	{
		return ShardContents.Contains(TEXT("void* Ret")) &&
			ShardContents.Contains(TEXT("if (Ret == nullptr)")) &&
			ShardContents.Contains(TEXT("*static_cast<")) &&
			ShardContents.Contains(TEXT("*>(Ret) = ")) &&
			ShardContents.Contains(TEXT(", sizeof("));
	}

	bool ContainsArgumentMarshallingCrossModuleThunk(const FString& ShardContents)
	{
		return !ShardContents.Contains(TEXT("void** /*Args*/")) &&
			ShardContents.Contains(TEXT("PassCrossModuleArg<")) &&
			ShardContents.Contains(TEXT("(Args, 0)")) &&
			ShardContents.Contains(TEXT("Args == nullptr")) &&
			ShardContents.Contains(TEXT("TIsReferenceType")) &&
			ShardContents.Contains(TEXT("TIsPointer"));
	}

	bool ContainsNonTrivialReturnConstruction(const FString& ShardContents)
	{
		return ShardContents.Contains(TEXT("new (Ret)")) &&
			ShardContents.Contains(TEXT("BuildCrossModuleReturn")) &&
			ShardContents.Contains(TEXT("BuildCrossModuleReturn<FVector>"));
	}

	bool ContainsNonZeroArgCrossModuleTableEntry(const FString& ShardContents)
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

	FString FormatLayoutVersionToken(uint32 Version)
	{
		return FString::Printf(TEXT("0x%08X"), Version);
	}

	bool LoadLayoutVersionToken(FString& OutToken)
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

	TSet<FString> ExtractDependencyModuleNames(const FString& BuildCsContents)
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

	bool CsvContainsRowWithPrefix(const FString& CsvContents, const FString& Prefix)
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

	bool TryExtractFirstCrossModuleCsvIdentity(const FString& EntriesContents, FString& OutModuleName, FString& OutClassName, FString& OutFunctionName)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT(","), false);
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

	bool CsvContainsEntryKindForIdentity(const FString& EntriesContents, const FString& ModuleName, const FString& ClassName, const FString& FunctionName, const FString& EntryKind)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT(","), false);
			if (Fields.Num() >= 5 && Fields[0] == ModuleName && Fields[2] == ClassName && Fields[3] == FunctionName && Fields[4] == EntryKind)
			{
				return true;
			}
		}

		return false;
	}

	bool TryExtractFirstRpcStubCsvIdentity(const FString& EntriesContents, FString& OutModuleName, FString& OutClassName, FString& OutFunctionName)
	{
		TArray<FString> Lines;
		EntriesContents.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT(","), false);
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleLinkProbeRoundtripTest,
	"Angelscript.CppTests.UHTToolResolver.LinkProbe.IModularFeaturesRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleLinkProbeRoundtripTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

	TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FName(TEXT("AngelscriptCrossModuleLinkProbe")));
	if (!TestTrue(TEXT("Engine module link probe feature should be registered"), Features.Num() > 0))
	{
		return false;
	}

	IModularFeature* Feature = Features[0];
	if (!TestNotNull(TEXT("Probe feature should be valid"), Feature))
	{
		return false;
	}

	const FProbeFeatureReader* Reader = reinterpret_cast<const FProbeFeatureReader*>(Feature);
	if (!TestNotNull(TEXT("Probe feature reader should be valid"), Reader))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("Probe layout version should match"), Reader->LayoutVersion, ProbeLayoutVersion);
	bPassed &= TestEqual(TEXT("Probe entry count should match"), Reader->Count, 1);
	bPassed &= TestEqual(TEXT("Probe module name should match"), FString(Reader->ModuleName), FString(TEXT("Engine")));
	bPassed &= TestNotNull(TEXT("Probe entry table should be valid"), Reader->Entries);

	if (Reader->Entries != nullptr)
	{
		bPassed &= TestEqual(TEXT("Probe entry tag should match"), FString(Reader->Entries[0].Tag), FString(TEXT("Engine.Probe")));
		bPassed &= TestEqual(TEXT("Probe entry magic should match"), Reader->Entries[0].Magic, ProbeLayoutVersion);
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModulePublicHeaderTest,
	"Angelscript.CppTests.UHTToolResolver.PublicHeader.NoASRuntimeOrSDKDeps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModulePublicHeaderTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

	FString HeaderContents;
	if (!TestTrue(TEXT("Cross-module public ABI header should be readable"), FFileHelper::LoadFileToString(HeaderContents, *GetCrossModulePublicHeaderPath())))
	{
		return false;
	}

	bool bPassed = true;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleLayoutVersionSingleSourceTest,
	"Angelscript.CppTests.UHTToolResolver.LayoutVersionFile_SingleSource_GeneratorAndHeaderInSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleLayoutVersionSingleSourceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

	FString VersionToken;
	if (!TestTrue(TEXT("Cross-module layout version file should expose a token"), LoadLayoutVersionToken(VersionToken)))
	{
		return false;
	}

	const FString ExpectedHeaderToken = FormatLayoutVersionToken(FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	bool bPassed = TestEqual(TEXT("Layout version file should match public header token"), VersionToken, ExpectedHeaderToken);

	FString CrossModuleShardContents;
	if (!TestTrue(TEXT("Engine cross-module shard should be generated"), LoadEngineCrossModuleShardContents(CrossModuleShardContents)))
	{
		return false;
	}

	bPassed &= TestTrue(TEXT("Cross-module shard should embed the shared layout token"), CrossModuleShardContents.Contains(FString::Printf(TEXT("GCrossModuleLayoutVersion = %su"), *VersionToken)));
	bPassed &= TestTrue(TEXT("Cross-module shard should assert cross-module entry ABI size"), CrossModuleShardContents.Contains(TEXT("static_assert(sizeof(FCrossModuleEntry) == 32")));
	bPassed &= TestTrue(TEXT("Cross-module shard should assert cross-module feature ABI size"), CrossModuleShardContents.Contains(TEXT("static_assert(sizeof(FCrossModuleFeature) == 32")));
	bPassed &= TestTrue(TEXT("Cross-module shard should pass the shared layout token into feature registration"), CrossModuleShardContents.Contains(TEXT("GCrossModuleFeature(GCrossModuleTable, UE_ARRAY_COUNT(GCrossModuleTable), TEXT(\"Engine\"), GCrossModuleLayoutVersion)")));
	bPassed &= TestTrue(TEXT("Cross-module shard should include safe zero-argument return-value thunks when available"), ContainsSafeReturnValueCrossModuleThunk(CrossModuleShardContents));
	bPassed &= TestTrue(TEXT("Cross-module shard should include argument marshalling helpers for non-zero-argument thunks"), ContainsArgumentMarshallingCrossModuleThunk(CrossModuleShardContents));
	bPassed &= TestTrue(TEXT("Cross-module shard should contain at least one non-zero-argument direct-bind entry"), ContainsNonZeroArgCrossModuleTableEntry(CrossModuleShardContents));
	bPassed &= TestTrue(TEXT("Cross-module shard should construct non-trivial return values in the AS return slot"), ContainsNonTrivialReturnConstruction(CrossModuleShardContents));
	bPassed &= TestFalse(TEXT("Cross-module shard should not include plugin ABI header"), CrossModuleShardContents.Contains(TEXT("AngelscriptCrossModuleBindings.h")));
	bPassed &= TestFalse(TEXT("Cross-module shard should not include AngelscriptBinds"), CrossModuleShardContents.Contains(TEXT("AngelscriptBinds.h")));
	bPassed &= TestFalse(TEXT("Cross-module shard should not include angelscript SDK"), CrossModuleShardContents.Contains(TEXT("angelscript.h")));
	bPassed &= TestFalse(TEXT("Cross-module shard should use IModularFeatures registration, not exported getter link path"), CrossModuleShardContents.Contains(TEXT("Get_AS_Bindings_")));
	bPassed &= TestFalse(TEXT("Cross-module shard should not use brace aggregate feature init"), CrossModuleShardContents.Contains(TEXT("= { GCrossModuleTable")));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleSkippedStatisticsTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.SkippedStatisticsClassifyCrossModuleOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleSkippedStatisticsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

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
	bPassed &= TestTrue(TEXT("Cross-module unsupported signatures should have an explicit skipped reason"), SummaryContents.Contains(TEXT("cross-module-unsupported-signature,")));
	bPassed &= TestTrue(TEXT("Disabled target modules should have an explicit skipped reason"), SummaryContents.Contains(TEXT("target-module-disabled,")));
	bPassed &= TestTrue(TEXT("RPC functions should have an explicit skipped reason"), SummaryContents.Contains(TEXT("rpc-net-function,")));
	bPassed &= TestFalse(TEXT("Supported cross-module candidates should no longer be lumped into unexported-symbol"), CsvContainsRowWithPrefix(SummaryContents, TEXT("unexported-symbol,")));

	FString RpcModuleName;
	FString RpcClassName;
	FString RpcFunctionName;
	if (TestTrue(TEXT("Generated entries CSV should keep RPC functions on stub fallback path"), TryExtractFirstRpcStubCsvIdentity(EntriesContents, RpcModuleName, RpcClassName, RpcFunctionName)))
	{
		const FString RpcSkippedIdentity = FString::Printf(TEXT("%s,%s,%s,rpc-net-function"), *RpcModuleName, *RpcClassName, *RpcFunctionName);
		bPassed &= TestTrue(TEXT("RPC stub entry should also appear in skipped diagnostics with rpc-net-function"), SkippedEntriesContents.Contains(RpcSkippedIdentity));
	}
	else
	{
		bPassed = false;
	}

	FString CrossModuleName;
	FString CrossModuleClassName;
	FString CrossModuleFunctionName;
	if (TestTrue(TEXT("Generated entries CSV should expose at least one CrossModule entry kind"), TryExtractFirstCrossModuleCsvIdentity(EntriesContents, CrossModuleName, CrossModuleClassName, CrossModuleFunctionName)))
	{
		const FString SkippedIdentity = FString::Printf(TEXT("%s,%s,%s,"), *CrossModuleName, *CrossModuleClassName, *CrossModuleFunctionName);
		bPassed &= TestFalse(TEXT("Generated cross-module entries should not also appear as skipped"), SkippedEntriesContents.Contains(SkippedIdentity));
	}
	else
	{
		bPassed = false;
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleNoLongerUnexportedSymbolTest,
	"Angelscript.CppTests.UHTToolResolver.NoLongerEmitsUnexportedSymbol_ForCrossModuleCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleNoLongerUnexportedSymbolTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

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

	FString CrossModuleName;
	FString CrossModuleClassName;
	FString CrossModuleFunctionName;
	if (!TestTrue(TEXT("Generated entries CSV should expose a cross-module candidate"), TryExtractFirstCrossModuleCsvIdentity(EntriesContents, CrossModuleName, CrossModuleClassName, CrossModuleFunctionName)))
	{
		return false;
	}

	const FString CrossModuleIdentity = FString::Printf(TEXT("%s,%s,%s,"), *CrossModuleName, *CrossModuleClassName, *CrossModuleFunctionName);
	bool bPassed = true;
	bPassed &= TestFalse(TEXT("Cross-module candidates should not be summarized as unexported-symbol"), CsvContainsRowWithPrefix(SummaryContents, TEXT("unexported-symbol,")));
	bPassed &= TestFalse(TEXT("Cross-module candidate should not remain in skipped diagnostics"), SkippedEntriesContents.Contains(CrossModuleIdentity));
	bPassed &= TestTrue(TEXT("Cross-module candidate should be recorded as EntryKind=CrossModule"), CsvContainsEntryKindForIdentity(EntriesContents, CrossModuleName, CrossModuleClassName, CrossModuleFunctionName, TEXT("CrossModule")));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleStaticAssertSizeofConsistencyTest,
	"Angelscript.CppTests.UHTToolResolver.StaticAssert_SizeofConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleStaticAssertSizeofConsistencyTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

	static_assert(sizeof(FAngelscriptCrossModuleEntry) == 32, "FAngelscriptCrossModuleEntry size must match generator-emitted ABI.");
	static_assert(sizeof(FAngelscriptCrossModuleFeatureReader) == 32, "FAngelscriptCrossModuleFeatureReader size must match generator-emitted ABI.");

	FString HeaderContents;
	if (!TestTrue(TEXT("Cross-module public ABI header should be readable"), FFileHelper::LoadFileToString(HeaderContents, *GetCrossModulePublicHeaderPath())))
	{
		return false;
	}

	FString CrossModuleShardContents;
	if (!TestTrue(TEXT("Engine cross-module shard should be generated"), LoadEngineCrossModuleShardContents(CrossModuleShardContents)))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Public header should assert cross-module entry ABI size"), HeaderContents.Contains(TEXT("static_assert(sizeof(FAngelscriptCrossModuleEntry) == 32")));
	bPassed &= TestTrue(TEXT("Public header should assert cross-module reader ABI size"), HeaderContents.Contains(TEXT("static_assert(sizeof(FAngelscriptCrossModuleFeatureReader) == 32")));
	bPassed &= TestTrue(TEXT("Generated shard should assert cross-module entry ABI size"), CrossModuleShardContents.Contains(TEXT("static_assert(sizeof(FCrossModuleEntry) == 32")));
	bPassed &= TestTrue(TEXT("Generated shard should assert cross-module feature ABI size"), CrossModuleShardContents.Contains(TEXT("static_assert(sizeof(FCrossModuleFeature) == 32")));
	bPassed &= TestFalse(TEXT("Generated shard should not use variable-padding bool fields in ABI payload"), CrossModuleShardContents.Contains(TEXT("\t\tbool ")));
	bPassed &= TestFalse(TEXT("Public header should not use variable-padding bool fields in ABI payload"), HeaderContents.Contains(TEXT("\tbool ")));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleStaleCleanupBoundariesTest,
	"Angelscript.CppTests.UHTToolResolver.StaleCleanup_CrossModuleEnumeratesSupportedModuleDirectories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleStaleCleanupBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Stale cleanup should accept supported module output directories"), GeneratorContents.Contains(TEXT("IReadOnlyDictionary<string, string> supportedModuleOutputDirectories")));
	bPassed &= TestTrue(TEXT("Stale cleanup should enumerate each supported module output directory"), GeneratorContents.Contains(TEXT("foreach ((string moduleName, string outputDirectory) in supportedModuleOutputDirectories)")));
	bPassed &= TestTrue(TEXT("Stale cleanup should delete only matching cross-module shards per module"), GeneratorContents.Contains(TEXT("$\"AS_FunctionTable_{moduleName}_CrossModule_*.cpp\"")));
	bPassed &= TestTrue(TEXT("Stale cleanup should preserve runtime same-module cleanup"), GeneratorContents.Contains(TEXT("DeleteStaleFilesInDirectory(runtimeOutputDirectory, \"AS_FunctionTable_*.cpp\", livePaths)")));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleScriptProjectionSafetyGateTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.ScriptMethodMixinProjection_ExcludedFromAutomaticSafeSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleScriptProjectionSafetyGateTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleAutomaticEntryVisibleTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.AutomaticEntryVisible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleAutomaticEntryVisibleTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

	FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
	if (CurrentEngine == nullptr || CurrentEngine->GetScriptEngine() == nullptr)
	{
		AddWarning(TEXT("AngelScript engine is not initialized; skipping runtime cross-module entry visibility check."));
		return true;
	}

	TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FAngelscriptCrossModuleBindings::FeatureName());
	if (!TestTrue(TEXT("Cross-module binding features should be registered"), Features.Num() > 0))
	{
		return false;
	}

	bool bObservedBoundEntry = false;
	bool bPassed = true;
	for (IModularFeature* Feature : Features)
	{
		const FAngelscriptCrossModuleFeatureReader* Reader = reinterpret_cast<const FAngelscriptCrossModuleFeatureReader*>(Feature);
		if (Reader == nullptr || Reader->LayoutVersion != FAngelscriptCrossModuleBindings::LayoutVersionExpected || Reader->Count <= 0 || Reader->Table == nullptr)
		{
			continue;
		}

		for (int32 EntryIndex = 0; EntryIndex < Reader->Count; ++EntryIndex)
		{
			const FAngelscriptCrossModuleEntry& CrossModuleEntry = Reader->Table[EntryIndex];
			UClass* EntryClass = ResolveCrossModuleClass(CrossModuleEntry, *Reader);
			if (EntryClass == nullptr)
			{
				continue;
			}

			TMap<FString, FFuncEntry>* FunctionMap = FAngelscriptBinds::GetClassFuncMaps().Find(EntryClass);
			if (FunctionMap == nullptr)
			{
				continue;
			}

			FFuncEntry* Entry = FunctionMap->Find(CrossModuleEntry.FunctionName);
			if (Entry == nullptr || Entry->UserData != &CrossModuleEntry)
			{
				continue;
			}

			bObservedBoundEntry = true;
			bPassed &= TestTrue(TEXT("Automatic cross-module entry should have a bound function pointer"), Entry->FuncPtr.IsBound());
			bPassed &= TestTrue(TEXT("Automatic cross-module entry should use generic bridge"), Entry->bGenericCall);
			bPassed &= TestNotNull(TEXT("Automatic cross-module entry should carry user data"), Entry->UserData);
		}
	}

	bPassed &= TestTrue(TEXT("At least one automatic cross-module entry should be injected"), bObservedBoundEntry);
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleBuildCsDependencyBoundaryTest,
	"Angelscript.CppTests.UHTToolResolver.BuildCs_NoEngineModuleAddedAsDependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleBuildCsDependencyBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_UHTToolResolver_LinkProbe_Private;

	FString BuildCsContents;
	if (!TestTrue(TEXT("AngelscriptRuntime.Build.cs should be readable"), FFileHelper::LoadFileToString(BuildCsContents, *GetRuntimeBuildCsPath())))
	{
		return false;
	}

	const TSet<FString> ExpectedDependencyModules = {
		TEXT("ApplicationCore"), TEXT("Core"), TEXT("CoreUObject"), TEXT("Engine"), TEXT("EngineSettings"), TEXT("DeveloperSettings"),
		TEXT("Json"), TEXT("JsonUtilities"), TEXT("GameplayTags"), TEXT("StructUtils"), TEXT("AIModule"), TEXT("NavigationSystem"),
		TEXT("NetCore"), TEXT("Landscape"), TEXT("Networking"), TEXT("Sockets"), TEXT("InputCore"), TEXT("SlateCore"), TEXT("Slate"),
		TEXT("UMG"), TEXT("TraceLog"), TEXT("AssetRegistry"), TEXT("Projects"), TEXT("PhysicsCore"), TEXT("CoreOnline"), TEXT("EnhancedInput"),
		TEXT("UnrealEd"), TEXT("EditorSubsystem"), TEXT("UMGEditor") };

	bool bPassed = true;
	const TSet<FString> ActualDependencyModules = ExtractDependencyModuleNames(BuildCsContents);
	for (const FString& ActualModule : ActualDependencyModules)
	{
		if (!ExpectedDependencyModules.Contains(ActualModule))
		{
			AddError(FString::Printf(TEXT("Unexpected AngelscriptRuntime module dependency introduced: %s"), *ActualModule));
			bPassed = false;
		}
	}

	return bPassed;
}

#endif
