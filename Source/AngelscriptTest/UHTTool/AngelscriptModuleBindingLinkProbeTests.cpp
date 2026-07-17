#include "CQTest.h"

#include "Components/TimelineComponent.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "Features/IModularFeatures.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Bindings/AngelscriptModuleBindingProtocol.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/UObjectGlobals.h"

#include <cstddef>
#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptModuleBindingPublicHeaderTests;
struct FAngelscriptModuleBindingLayoutVersionTests;
struct FAngelscriptModuleBindingResolverTests;
struct FAngelscriptModuleBindingDirectBindProbeTests;
struct FAngelscriptModuleBindingGenerationProfileTests;
struct FAngelscriptModuleBindingDefaultOffTests;
struct FAngelscriptModuleLocalCompileGateTests;

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleBindingLinkProbeTests,
	"Angelscript.CppTests.UHTToolResolver.LinkProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	friend struct FAngelscriptModuleBindingPublicHeaderTests;
	friend struct FAngelscriptModuleBindingLayoutVersionTests;
	friend struct FAngelscriptModuleBindingResolverTests;
	friend struct FAngelscriptModuleBindingDirectBindProbeTests;
	friend struct FAngelscriptModuleBindingGenerationProfileTests;
	friend struct FAngelscriptModuleBindingDefaultOffTests;
	friend struct FAngelscriptModuleLocalCompileGateTests;

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
	static_assert(std::is_standard_layout<FAngelscriptModuleBinding>::value, "Module binding binding must stay standard-layout.");
	static_assert(std::is_standard_layout<FAngelscriptModuleBindingFeatureView>::value, "Module binding binding reader must stay standard-layout.");
	static_assert(FAngelscriptModuleBindingProtocol::LayoutVersionExpected == ProbeLayoutVersion, "Module binding layout token drifted.");
	static_assert(FAngelscriptModuleBindingProtocol::FlagStatic == (1u << 0), "Module binding static flag drifted.");
	static_assert(FAngelscriptModuleBindingProtocol::FlagConst == (1u << 1), "Module binding const flag drifted.");
	static_assert(FAngelscriptModuleBindingProtocol::FlagWorldContext == (1u << 2), "Module binding world-context flag drifted.");
	static_assert(FAngelscriptModuleBindingProtocol::FlagHasOutParams == (1u << 3), "Module binding out-param flag drifted.");
	static_assert(FAngelscriptModuleBindingProtocol::FlagReturnByRef == (1u << 4), "Module binding return-by-ref flag drifted.");

	struct FProbeBindingReader
	{
		const TCHAR* Tag;
		uint32 Magic;
	};

	struct FProbeFeatureReader
	{
		const FProbeBindingReader* Entries;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;
	};

	struct FProbeFeatureLayoutProbe : public IModularFeature
	{
		const FProbeBindingReader* Entries;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;
	};

	struct FModuleBindingFeatureLayoutProbe : public IModularFeature
	{
		const FAngelscriptModuleBinding* Table;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;
	};

	static_assert(std::is_standard_layout<FProbeFeatureLayoutProbe>::value, "Probe feature layout must stay standard-layout.");
	static_assert(offsetof(FProbeFeatureLayoutProbe, Entries) == offsetof(FProbeFeatureReader, Entries), "Probe Entries offset drifted.");
	static_assert(offsetof(FProbeFeatureLayoutProbe, Count) == offsetof(FProbeFeatureReader, Count), "Probe Count offset drifted.");
	static_assert(offsetof(FProbeFeatureLayoutProbe, ModuleName) == offsetof(FProbeFeatureReader, ModuleName), "Probe ModuleName offset drifted.");
	static_assert(offsetof(FProbeFeatureLayoutProbe, LayoutVersion) == offsetof(FProbeFeatureReader, LayoutVersion), "Probe LayoutVersion offset drifted.");
	static_assert(std::is_standard_layout<FModuleBindingFeatureLayoutProbe>::value, "Module binding binding feature layout must stay standard-layout.");
	static_assert(offsetof(FModuleBindingFeatureLayoutProbe, Table) == offsetof(FAngelscriptModuleBindingFeatureView, Table), "Module binding Table offset drifted.");
	static_assert(offsetof(FModuleBindingFeatureLayoutProbe, Count) == offsetof(FAngelscriptModuleBindingFeatureView, Count), "Module binding Count offset drifted.");
	static_assert(offsetof(FModuleBindingFeatureLayoutProbe, ModuleName) == offsetof(FAngelscriptModuleBindingFeatureView, ModuleName), "Module binding ModuleName offset drifted.");
	static_assert(offsetof(FModuleBindingFeatureLayoutProbe, LayoutVersion) == offsetof(FAngelscriptModuleBindingFeatureView, LayoutVersion), "Module binding LayoutVersion offset drifted.");

	static FString GetAngelscriptPluginDirectory()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Angelscript"));
	}

	static FString GetModuleBindingLayoutVersionFilePath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptUHTTool/module-binding-layout-version.txt"));
	}

	static FString GetModuleBindingGenerationModulesFilePath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptUHTTool/module-binding-generation-modules.json"));
	}

	static FString GetCompileOptionsFilePath()
	{
		return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultAngelscriptCompileOptions.ini"));
	}

	static FString GetRuntimeModuleBindingBridgePath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptRuntime/Binds/Bind_ModuleBinding.cpp"));
	}

	static FString GetEditorModulePath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptEditor/Core/AngelscriptEditorModule.cpp"));
	}

	static FString GetModuleBindingPublicHeaderPath()
	{
		return FPaths::Combine(GetAngelscriptPluginDirectory(), TEXT("Source/AngelscriptRuntime/Public/Bindings/AngelscriptModuleBindingProtocol.h"));
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

	static UClass* ResolveModuleBindingClassByName(const TCHAR* ModuleName, const TCHAR* ClassName)
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

	static UClass* ResolveModuleBindingClass(const FAngelscriptModuleBinding& Binding, const FAngelscriptModuleBindingFeatureView& Reader)
	{
		if (UClass* Class = ResolveModuleBindingClassByName(Reader.ModuleName, Binding.ClassName))
		{
			return Class;
		}

		if (Binding.ClassName != nullptr && (Binding.ClassName[0] == TEXT('U') || Binding.ClassName[0] == TEXT('A')) && Binding.ClassName[1] != TEXT('\0'))
		{
			return ResolveModuleBindingClassByName(Reader.ModuleName, Binding.ClassName + 1);
		}

		return nullptr;
	}

	static FString GetEngineManualModuleBindingShardPath()
	{
		return FPaths::Combine(FPaths::EngineDir(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/Engine/UHT/AS_FunctionTable_Engine_ModuleBinding_Manual.cpp"));
	}

	static FString GetEngineAutomaticModuleBindingShardPath()
	{
		return FPaths::Combine(FPaths::EngineDir(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/Engine/UHT/AS_FunctionTable_Engine_ModuleBinding_000.cpp"));
	}

	static bool LoadEngineModuleBindingShardContents(FString& OutContents)
	{
		if (FFileHelper::LoadFileToString(OutContents, *GetEngineAutomaticModuleBindingShardPath()))
		{
			return true;
		}

		return FFileHelper::LoadFileToString(OutContents, *GetEngineManualModuleBindingShardPath());
	}

	static bool ContainsSafeReturnValueModuleBindingThunk(const FString& ShardContents)
	{
		return ShardContents.Contains(TEXT("FModuleBindingCallFrame* Frame")) &&
			ShardContents.Contains(TEXT("Frame->ReturnSlot")) &&
			ShardContents.Contains(TEXT("*static_cast<")) &&
			ShardContents.Contains(TEXT("*>(Frame->ReturnSlot) = ")) &&
			ShardContents.Contains(TEXT(", sizeof("));
	}

	static bool ContainsArgumentMarshallingModuleBindingThunk(const FString& ShardContents)
	{
		return !ShardContents.Contains(TEXT("FModuleBindingCallFrame* /*Frame*/")) &&
			ShardContents.Contains(TEXT("PassModuleBindingArg<")) &&
			ShardContents.Contains(TEXT("(Frame, 0)")) &&
			ShardContents.Contains(TEXT("Frame == nullptr")) &&
			ShardContents.Contains(TEXT("Frame->ArgSlots == nullptr")) &&
			ShardContents.Contains(TEXT("TIsReferenceType")) &&
			ShardContents.Contains(TEXT("TIsPointer"));
	}

	static bool ContainsNonTrivialReturnConstruction(const FString& ShardContents)
	{
		return ShardContents.Contains(TEXT("new (Frame->ReturnSlot)")) &&
			ShardContents.Contains(TEXT("BuildModuleBindingReturn")) &&
			ShardContents.Contains(TEXT("BuildModuleBindingReturn<FVector>"));
	}

	static bool ContainsNonZeroArgModuleBindingTableEntry(const FString& ShardContents)
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
		if (!FFileHelper::LoadFileToString(Contents, *GetModuleBindingLayoutVersionFilePath()))
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

	static bool TryExtractFirstModuleBindingCsvIdentity(const FString& EntriesContents, FString& OutModuleName, FString& OutClassName, FString& OutFunctionName)
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

			if (Fields.Num() >= 5 && Fields[4] == TEXT("ModuleBinding"))
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
	static bool RunModuleLocalCompileGate(FAutomationTestBase& Test);

public:
	TEST_METHOD(IModularFeaturesRoundtrip)
	{
		ASSERT_THAT(IsTrue(RunLinkProbeRoundtrip(*TestRunner)));
	}
};

FAutomationTestBase* FAngelscriptModuleBindingLinkProbeTests::GActiveTest = nullptr;

bool FAngelscriptModuleBindingLinkProbeTests::RunLinkProbeRoundtrip(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FName(TEXT("AngelscriptModuleBindingLinkProbe")));
	return TestEqual(TEXT("Engine module link probe feature should not be registered while module-binding generation is disabled by default"), Features.Num(), 0);
}

bool FAngelscriptModuleBindingLinkProbeTests::RunPublicHeader(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString HeaderContents;
	if (!TestTrue(TEXT("Module binding public ABI header should be readable"), FFileHelper::LoadFileToString(HeaderContents, *GetModuleBindingPublicHeaderPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Public ABI header should declare the module-binding call frame"), HeaderContents.Contains(TEXT("struct FAngelscriptModuleBindingCallFrame")));
	bPassed &= TestTrue(TEXT("Public ABI header should expose frame-based thunk pointers"), HeaderContents.Contains(TEXT("FAngelscriptModuleBindingCallFrame* Frame")));
	bPassed &= TestTrue(TEXT("Public ABI header should assert module-binding call-frame ABI size"), HeaderContents.Contains(TEXT("static_assert(sizeof(FAngelscriptModuleBindingCallFrame) == 48")));
	bPassed &= TestFalse(TEXT("Public ABI header should not expose raw Args/Ret thunk pointers"), HeaderContents.Contains(TEXT("void** Args, void* Ret")));
	bPassed &= TestFalse(TEXT("Public ABI header should not include AngelscriptBinds"), HeaderContents.Contains(TEXT("AngelscriptBinds.h")));
	bPassed &= TestFalse(TEXT("Public ABI header should not include FunctionCallers"), HeaderContents.Contains(TEXT("FunctionCallers.h")));
	bPassed &= TestFalse(TEXT("Public ABI header should not include angelscript SDK"), HeaderContents.Contains(TEXT("angelscript.h")));
	bPassed &= TestFalse(TEXT("Public ABI header should not expose FAngelscriptBinds"), HeaderContents.Contains(TEXT("FAngelscriptBinds")));
	bPassed &= TestFalse(TEXT("Public ABI header should not expose ASAutoCaller"), HeaderContents.Contains(TEXT("ASAutoCaller")));
	bPassed &= TestFalse(TEXT("Public ABI header should not expose FGenericFuncPtr"), HeaderContents.Contains(TEXT("FGenericFuncPtr")));
	bPassed &= TestEqual(TEXT("Module binding ABI size should match"), static_cast<int32>(sizeof(FAngelscriptModuleBinding)), 32);
	bPassed &= TestEqual(TEXT("Module binding feature view ABI size should match"), static_cast<int32>(sizeof(FAngelscriptModuleBindingFeatureView)), 32);
	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunLayoutVersionSingleSource(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString VersionToken;
	if (!TestTrue(TEXT("Module binding layout version file should expose a token"), LoadLayoutVersionToken(VersionToken)))
	{
		return false;
	}

	const FString ExpectedHeaderToken = FormatLayoutVersionToken(FAngelscriptModuleBindingProtocol::LayoutVersionExpected);
	bool bPassed = TestEqual(TEXT("Layout version file should match public header token"), VersionToken, ExpectedHeaderToken);

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bPassed &= TestTrue(TEXT("Generator should still build module-binding shards when explicitly enabled"), GeneratorContents.Contains(TEXT("BuildModuleBindingShard")));
	bPassed &= TestTrue(TEXT("Module binding shard template should embed the shared layout token"), GeneratorContents.Contains(TEXT("GModuleBindingLayoutVersion = {layoutVersion}u")));
	bPassed &= TestTrue(TEXT("Module binding shard template should mirror the frame ABI"), GeneratorContents.Contains(TEXT("struct FModuleBindingCallFrame")));
	bPassed &= TestTrue(TEXT("Module binding shard template should use frame-based thunk pointers"), GeneratorContents.Contains(TEXT("FModuleBindingCallFrame* Frame")));
	bPassed &= TestTrue(TEXT("Module binding shard template should read arguments from frame slots"), GeneratorContents.Contains(TEXT("Frame->ArgSlots")));
	bPassed &= TestTrue(TEXT("Module binding shard template should write returns through the frame return slot"), GeneratorContents.Contains(TEXT("Frame->ReturnSlot")));
	bPassed &= TestTrue(TEXT("Module binding shard template should assert module-binding call-frame ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FModuleBindingCallFrame) == 48")));
	bPassed &= TestTrue(TEXT("Module binding shard template should assert module binding ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FModuleBinding) == 32")));
	bPassed &= TestTrue(TEXT("Module binding shard template should assert module binding feature ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FModuleBindingFeature) == 32")));
	bPassed &= TestTrue(TEXT("Module binding shard template should pass the shared layout token into feature registration"), GeneratorContents.Contains(TEXT("GModuleBindingFeature(GModuleBindingTable, UE_ARRAY_COUNT(GModuleBindingTable), TEXT(\\\"")));
	bPassed &= TestTrue(TEXT("Module binding shard template should include return-value thunk support"), GeneratorContents.Contains(TEXT("BuildModuleBindingReturn")));
	bPassed &= TestTrue(TEXT("Module binding shard template should include argument marshalling helpers"), GeneratorContents.Contains(TEXT("PassModuleBindingArg<")));
	bPassed &= TestFalse(TEXT("Module binding shard template should not use exported getter link path"), GeneratorContents.Contains(TEXT("Get_AS_Bindings_")));
	bPassed &= TestFalse(TEXT("Module binding shard template should not expose raw Args/Ret thunk signatures"), GeneratorContents.Contains(TEXT("void** Args, void* Ret")));
	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunSkippedStatistics(FAutomationTestBase& Test)
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
	bPassed &= TestTrue(TEXT("WorldContext module-binding candidates should identify missing policy"), SummaryContents.Contains(TEXT("needs-world-context-policy,")));
	bPassed &= TestTrue(TEXT("Out/ref module-binding candidates should identify missing protocol"), SummaryContents.Contains(TEXT("needs-out-param-protocol,")));
	bPassed &= TestTrue(TEXT("Ref-return module-binding candidates should identify missing protocol"), SummaryContents.Contains(TEXT("needs-ref-return-protocol,")));
	bPassed &= TestTrue(TEXT("Container module-binding candidates should identify missing frame protocol"), SummaryContents.Contains(TEXT("needs-container-frame-protocol,")));
	bPassed &= TestTrue(TEXT("Interface module-binding candidates should identify missing frame protocol"), SummaryContents.Contains(TEXT("needs-interface-frame-protocol,")));
	bPassed &= TestTrue(TEXT("Delegate module-binding candidates should identify missing frame protocol"), SummaryContents.Contains(TEXT("needs-delegate-frame-protocol,")));
	bPassed &= TestTrue(TEXT("FieldPath module-binding candidates should identify missing frame protocol"), SummaryContents.Contains(TEXT("needs-field-path-frame-protocol,")));
	bPassed &= TestTrue(TEXT("ScriptMethod/ScriptMixin projections should identify missing receiver projection"), SummaryContents.Contains(TEXT("needs-script-this-projection,")));
	bPassed &= TestFalse(TEXT("Enabled module-binding unsupported signatures should be split into protocol reasons"), CsvContainsRowWithPrefix(SummaryContents, TEXT("module-binding-unsupported-signature,")));
	bPassed &= TestFalse(TEXT("Disabled target modules should not remain lumped into target-module-disabled when classifiable"), SummaryContents.Contains(TEXT("target-module-disabled,")));
	bPassed &= TestTrue(TEXT("Disabled target modules should preserve protocol diagnostics"), SummaryContents.Contains(TEXT("disabled-needs-out-param-protocol,")));
	bPassed &= TestTrue(TEXT("RPC functions should have an explicit skipped reason"), SummaryContents.Contains(TEXT("rpc-net-function,")));
	bPassed &= TestFalse(TEXT("Supported module-binding candidates should no longer be lumped into unexported-symbol"), CsvContainsRowWithPrefix(SummaryContents, TEXT("unexported-symbol,")));
	bPassed &= TestTrue(TEXT("Entries CSV should expose a thunk style column"), EntriesContents.StartsWith(TEXT("ModuleName,EditorOnly,ClassName,FunctionName,EntryKind,EraseMacro,ShardIndex,ThunkStyle")));
	const FString DiagnosticOnlyDisabledModule = TEXT("ControlRigEditor");
	bPassed &= TestFalse(TEXT("Source profile module should not emit module-binding entries while generation is disabled by default"), CsvContainsModuleEntryKind(EntriesContents, DiagnosticOnlyDisabledModule, TEXT("ModuleBinding")));
	bPassed &= TestTrue(TEXT("Source profile module should remain diagnostic-only disabled-safe-module-binding by default"), CsvContainsSkippedReasonForModule(SkippedEntriesContents, DiagnosticOnlyDisabledModule, TEXT("disabled-safe-module-binding")));

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

	FString ModuleBindingName;
	FString ModuleBindingClassName;
	FString ModuleBindingFunctionName;
	bPassed &= TestFalse(TEXT("Generated entries CSV should not expose ModuleBinding entry kinds while generation is disabled by default"), TryExtractFirstModuleBindingCsvIdentity(EntriesContents, ModuleBindingName, ModuleBindingClassName, ModuleBindingFunctionName));

	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunNoLongerUnexportedSymbol(FAutomationTestBase& Test)
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
	bPassed &= TestFalse(TEXT("Module binding candidates should not be summarized as unexported-symbol"), CsvContainsRowWithPrefix(SummaryContents, TEXT("unexported-symbol,")));
	bPassed &= TestFalse(TEXT("Module binding candidates should not be recorded as EntryKind=ModuleBinding while generation is disabled by default"), EntriesContents.Contains(TEXT(",ModuleBinding,")));
	bPassed &= TestTrue(TEXT("Skipped diagnostics should retain disabled-safe-module-binding candidates by default"), SkippedEntriesContents.Contains(TEXT("disabled-safe-module-binding")));
	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunStaticAssertSizeofConsistency(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	static_assert(sizeof(FAngelscriptModuleBinding) == 32, "FAngelscriptModuleBinding size must match generator-emitted ABI.");
	static_assert(sizeof(FAngelscriptModuleBindingFeatureView) == 32, "FAngelscriptModuleBindingFeatureView size must match generator-emitted ABI.");

	FString HeaderContents;
	if (!TestTrue(TEXT("Module binding public ABI header should be readable"), FFileHelper::LoadFileToString(HeaderContents, *GetModuleBindingPublicHeaderPath())))
	{
		return false;
	}

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Public header should assert module binding ABI size"), HeaderContents.Contains(TEXT("static_assert(sizeof(FAngelscriptModuleBinding) == 32")));
	bPassed &= TestTrue(TEXT("Public header should assert module binding feature view ABI size"), HeaderContents.Contains(TEXT("static_assert(sizeof(FAngelscriptModuleBindingFeatureView) == 32")));
	bPassed &= TestTrue(TEXT("Generated shard template should assert module binding ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FModuleBinding) == 32")));
	bPassed &= TestTrue(TEXT("Generated shard template should assert module binding feature ABI size"), GeneratorContents.Contains(TEXT("static_assert(sizeof(FModuleBindingFeature) == 32")));
	bPassed &= TestFalse(TEXT("Generated shard template should not use variable-padding bool fields in ABI payload"), GeneratorContents.Contains(TEXT("\\t\\tbool ")));
	bPassed &= TestFalse(TEXT("Public header should not use variable-padding bool fields in ABI payload"), HeaderContents.Contains(TEXT("\tbool ")));
	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunStaleCleanupBoundaries(FAutomationTestBase& Test)
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
	bPassed &= TestTrue(TEXT("Stale cleanup should delete only matching module-binding shards per module"), GeneratorContents.Contains(TEXT("$\"AS_FunctionTable_{moduleName}_ModuleBinding_*.cpp\"")));
	bPassed &= TestTrue(TEXT("Stale cleanup should remove legacy module-binding shard names"), GeneratorContents.Contains(TEXT("const string LegacyModuleBindingSegment = \"Cross\" + \"Module\"")));
	bPassed &= TestTrue(TEXT("Stale cleanup should remove the legacy Engine module-binding link probe"), GeneratorContents.Contains(TEXT("AS_FunctionTable_Engine_LinkProbe.cpp")));
	bPassed &= TestTrue(TEXT("Stale cleanup should preserve runtime same-module cleanup"), GeneratorContents.Contains(TEXT("DeleteStaleFilesInDirectory(runtimeOutputDirectory, \"AS_FunctionTable_*.cpp\", livePaths)")));
	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunScriptProjectionSafetyGate(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString GeneratorContents;
	if (!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Safe module-binding gate should call the ScriptMethod/ScriptMixin projection guard"), GeneratorContents.Contains(TEXT("!HasScriptMethodMixinProjection(signature, classObj, function)")));
	bPassed &= TestTrue(TEXT("Generator should define a dedicated ScriptMethod/ScriptMixin projection guard"), GeneratorContents.Contains(TEXT("HasScriptMethodMixinProjection")));
	bPassed &= TestTrue(TEXT("ScriptMethod metadata should be recognized as an implicit script-this projection"), GeneratorContents.Contains(TEXT("function.MetaData.ContainsKey(\"ScriptMethod\")")));
	bPassed &= TestTrue(TEXT("ScriptMixin metadata should be recognized as an implicit script-this projection"), GeneratorContents.Contains(TEXT("classObj.MetaData.ContainsKey(\"ScriptMixin\")")));
	bPassed &= TestTrue(TEXT("Only static projected functions require exclusion from the raw thunk safe set"), GeneratorContents.Contains(TEXT("return signature.IsStatic &&")));
	bPassed &= TestTrue(TEXT("Module binding classification should pass class/function context into the safe-signature gate"), GeneratorContents.Contains(TEXT("IsSafeAutomaticModuleBindingSignature(signature!, classObj, function)")));
	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunAutomaticEntryVisible(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
	if (CurrentEngine == nullptr || CurrentEngine->GetScriptEngine() == nullptr)
	{
		Test.AddWarning(TEXT("AngelScript engine is not initialized; skipping runtime module-binding entry visibility check."));
		return true;
	}

	TArray<IModularFeature*> Features = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FAngelscriptModuleBindingProtocol::FeatureName());
	return TestEqual(TEXT("Automatic module-binding entries should not be injected while generation is disabled by default"), Features.Num(), 0);
}

bool FAngelscriptModuleBindingLinkProbeTests::RunBuildCsDependencyBoundary(FAutomationTestBase& Test)
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

bool FAngelscriptModuleBindingLinkProbeTests::RunGenerationProfilesPolicy(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString ProfileConfigContents;
	if (!TestTrue(TEXT("Module binding generation profile config should be readable"), FFileHelper::LoadFileToString(ProfileConfigContents, *GetModuleBindingGenerationModulesFilePath())))
	{
		return false;
	}

	FString CompileOptionsContents;
	if (!TestTrue(TEXT("Compile options should be readable"), FFileHelper::LoadFileToString(CompileOptionsContents, *GetCompileOptionsFilePath())))
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
	bPassed &= TestTrue(TEXT("Profile config should remain enabled for explicit compile-option control"), ProfileConfigContents.Contains(TEXT("\"enabled\": true")));
	bPassed &= TestTrue(TEXT("Compile options should default ModuleLocal generation to disabled"), CompileOptionsContents.Contains(TEXT("bCompileAngelscriptModuleLocalBindings=false")));
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

bool FAngelscriptModuleBindingLinkProbeTests::RunGenerationProfilesEntries(FAutomationTestBase& Test)
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
	bPassed &= TestTrue(TEXT("Summary should report selected module-binding generation profile"), SummaryContents.Contains(TEXT("\"moduleBindingGenerationProfile\"")));
	bPassed &= TestTrue(TEXT("Summary should report module-binding generation disabled by default"), SummaryContents.Contains(TEXT("\"moduleBindingGenerationEnabled\": false")));
	bPassed &= TestTrue(TEXT("Summary should report module-binding config path"), SummaryContents.Contains(TEXT("\"moduleBindingGenerationConfigPath\"")));
	bPassed &= TestTrue(TEXT("Summary should report configured module-binding modules"), SummaryContents.Contains(TEXT("\"moduleBindingConfiguredModules\"")));
	bPassed &= TestTrue(TEXT("Summary should report effective module-binding modules"), SummaryContents.Contains(TEXT("\"moduleBindingEffectiveModules\"")));
	bPassed &= TestTrue(TEXT("Summary should report no effective module-binding modules while generation is disabled"), SummaryContents.Contains(TEXT("\"moduleBindingEffectiveModules\": []")));
	for (const FString& PilotModule : PilotModules)
	{
		bPassed &= TestFalse(FString::Printf(TEXT("Pilot module %s should not emit ModuleBinding rows by default"), *PilotModule), CsvContainsModuleEntryKind(EntriesContents, PilotModule, TEXT("ModuleBinding")));
		bPassed &= TestTrue(FString::Printf(TEXT("Pilot module %s should remain diagnostic-only disabled-safe-module-binding by default"), *PilotModule), CsvContainsSkippedReasonForModule(SkippedEntriesContents, PilotModule, TEXT("disabled-safe-module-binding")));
	}

	for (const FString& SourceProfileModule : SourceProfileModules)
	{
		bPassed &= TestFalse(FString::Printf(TEXT("Source profile module %s should not emit ModuleBinding rows by default"), *SourceProfileModule), CsvContainsModuleEntryKind(EntriesContents, SourceProfileModule, TEXT("ModuleBinding")));
		bPassed &= TestTrue(FString::Printf(TEXT("Source profile module %s should remain diagnostic-only disabled-safe-module-binding by default"), *SourceProfileModule), CsvContainsSkippedReasonForModule(SkippedEntriesContents, SourceProfileModule, TEXT("disabled-safe-module-binding")));
	}

	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunDefaultOffDiagnostics(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString ProfileConfigContents;
	if (!TestTrue(TEXT("Module binding generation profile config should be readable"), FFileHelper::LoadFileToString(ProfileConfigContents, *GetModuleBindingGenerationModulesFilePath())))
	{
		return false;
	}

	FString CompileOptionsContents;
	if (!TestTrue(TEXT("Compile options should be readable"), FFileHelper::LoadFileToString(CompileOptionsContents, *GetCompileOptionsFilePath())))
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
	bPassed &= TestTrue(TEXT("Config should leave the profile available to the compile option"), ProfileConfigContents.Contains(TEXT("\"enabled\": true")));
	bPassed &= TestTrue(TEXT("Compile options should disable ModuleLocal generation by default"), CompileOptionsContents.Contains(TEXT("bCompileAngelscriptModuleLocalBindings=false")));
	bPassed &= TestTrue(TEXT("Config should keep source profile modules for explicit opt-in"), ProfileConfigContents.Contains(TEXT("\"ControlRigEditor\"")));
	bPassed &= TestTrue(TEXT("Summary should expose disabled ModuleBinding generation state"), SummaryContents.Contains(TEXT("\"moduleBindingGenerationEnabled\": false")));
	bPassed &= TestTrue(TEXT("Summary should keep selected profile visible"), SummaryContents.Contains(TEXT("\"moduleBindingGenerationProfile\"")));
	bPassed &= TestTrue(TEXT("Summary should keep configured module diagnostics visible"), SummaryContents.Contains(TEXT("\"moduleBindingConfiguredModules\"")));
	bPassed &= TestTrue(TEXT("Summary should report no effective modules by default"), SummaryContents.Contains(TEXT("\"moduleBindingEffectiveModules\": []")));
	bPassed &= TestTrue(TEXT("Generator should model ModuleBinding generation as an explicit config gate"), GeneratorContents.Contains(TEXT("ModuleBindingGenerationEnabled")));
	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunDefaultOffGeneratedOutput(FAutomationTestBase& Test)
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
		FName(TEXT("AngelscriptModuleBindingLinkProbe")));
	TArray<IModularFeature*> BindingFeatures = IModularFeatures::Get().GetModularFeatureImplementations<IModularFeature>(
		FAngelscriptModuleBindingProtocol::FeatureName());

	bool bPassed = true;
	bPassed &= TestFalse(TEXT("Entries CSV should not contain ModuleBinding rows by default"), EntriesContents.Contains(TEXT(",ModuleBinding,")));
	bPassed &= TestTrue(TEXT("Skipped diagnostics should retain opt-in opportunities"), SkippedEntriesContents.Contains(TEXT("disabled-safe-module-binding")));
	bPassed &= TestEqual(TEXT("Engine link probe should not be registered by default"), ProbeFeatures.Num(), 0);
	bPassed &= TestEqual(TEXT("ModuleBinding binding features should not be registered by default"), BindingFeatures.Num(), 0);
	return bPassed;
}

bool FAngelscriptModuleBindingLinkProbeTests::RunModuleLocalCompileGate(FAutomationTestBase& Test)
{
	TGuardValue<FAutomationTestBase*> ActiveTestGuard(GActiveTest, &Test);

	FString CompileOptionsContents;
	FString BuildCsContents;
	FString GeneratorContents;
	FString BridgeContents;
	FString EditorModuleContents;
	if (!TestTrue(TEXT("Compile options should be readable"), FFileHelper::LoadFileToString(CompileOptionsContents, *GetCompileOptionsFilePath())) ||
		!TestTrue(TEXT("Runtime Build.cs should be readable"), FFileHelper::LoadFileToString(BuildCsContents, *GetRuntimeBuildCsPath())) ||
		!TestTrue(TEXT("UHT code generator should be readable"), FFileHelper::LoadFileToString(GeneratorContents, *GetUhtCodeGeneratorPath())) ||
		!TestTrue(TEXT("Runtime ModuleLocal bridge should be readable"), FFileHelper::LoadFileToString(BridgeContents, *GetRuntimeModuleBindingBridgePath())) ||
		!TestTrue(TEXT("Editor module should be readable"), FFileHelper::LoadFileToString(EditorModuleContents, *GetEditorModulePath())))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Compile options should default ModuleLocal bindings to disabled"), CompileOptionsContents.Contains(TEXT("bCompileAngelscriptModuleLocalBindings=false")));
	bPassed &= TestTrue(TEXT("Runtime Build.cs should define the ModuleLocal bindings macro"), BuildCsContents.Contains(TEXT("WITH_ANGELSCRIPT_MODULE_LOCAL_BINDINGS")));
	bPassed &= TestTrue(TEXT("Runtime Build.cs should reject ModuleLocal bindings for non-source engines"), BuildCsContents.Contains(TEXT("requires a source engine")));
	bPassed &= TestTrue(TEXT("UHT should read the ModuleLocal compile option"), GeneratorContents.Contains(TEXT("bCompileAngelscriptModuleLocalBindings")));
	bPassed &= TestTrue(TEXT("UHT should reject enabled ModuleLocal bindings for non-source engines"), GeneratorContents.Contains(TEXT("requires a source engine")));
	bPassed &= TestTrue(TEXT("Runtime ModuleLocal bridge should be compile-gated"), BridgeContents.Contains(TEXT("#if WITH_ANGELSCRIPT_MODULE_LOCAL_BINDINGS")));
	bPassed &= TestTrue(TEXT("Editor settings should validate ModuleLocal bindings on modification"), EditorModuleContents.Contains(TEXT("OnModified().Bind")));
	bPassed &= TestTrue(TEXT("Editor settings should reject ModuleLocal bindings for non-source engines"), EditorModuleContents.Contains(TEXT("requires a source engine")));
	return bPassed;
}

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleBindingPublicHeaderTests,
	"Angelscript.CppTests.UHTToolResolver.PublicHeader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NoASRuntimeOrSDKDeps)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunPublicHeader(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleBindingLayoutVersionTests,
	"Angelscript.CppTests.UHTToolResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LayoutVersionFile_SingleSource_GeneratorAndHeaderInSync)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunLayoutVersionSingleSource(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleBindingResolverTests,
	"Angelscript.CppTests.UHTToolResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NoLongerEmitsUnexportedSymbol_ForModuleBindingCandidate)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunNoLongerUnexportedSymbol(*TestRunner)));
	}

	TEST_METHOD(StaticAssert_SizeofConsistency)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunStaticAssertSizeofConsistency(*TestRunner)));
	}

	TEST_METHOD(StaleCleanup_ModuleBindingEnumeratesSupportedModuleDirectories)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunStaleCleanupBoundaries(*TestRunner)));
	}

	TEST_METHOD(BuildCs_NoEngineModuleAddedAsDependency)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunBuildCsDependencyBoundary(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleBindingDirectBindProbeTests,
	"Angelscript.CppTests.UHTToolResolver.ModuleBindingDirectBind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SkippedStatisticsClassifyModuleBindingOutcomes)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunSkippedStatistics(*TestRunner)));
	}

	TEST_METHOD(ScriptMethodMixinProjection_ExcludedFromAutomaticSafeSet)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunScriptProjectionSafetyGate(*TestRunner)));
	}

	TEST_METHOD(AutomaticEntryVisible)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunAutomaticEntryVisible(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleBindingGenerationProfileTests,
	"Angelscript.CppTests.UHTToolResolver.ModuleBindingGenerationProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PolicyFileAndBuildCsBoundary)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunGenerationProfilesPolicy(*TestRunner)));
	}

	TEST_METHOD(GeneratedRowsAreModuleBindingOnly)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunGenerationProfilesEntries(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleBindingDefaultOffTests,
	"Angelscript.CppTests.UHTToolResolver.ModuleBindingDefaultOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DiagnosticsAndProfileOptIn)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunDefaultOffDiagnostics(*TestRunner)));
	}

	TEST_METHOD(GeneratedOutputsSuppressed)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunDefaultOffGeneratedOutput(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleLocalCompileGateTests,
	"Angelscript.CppTests.UHTToolResolver.ModuleLocalCompileGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompileOptionControlsUhtAndRuntimeBridge)
	{
		ASSERT_THAT(IsTrue(FAngelscriptModuleBindingLinkProbeTests::RunModuleLocalCompileGate(*TestRunner)));
	}
};

#endif
