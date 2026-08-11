#include "Cache/AngelscriptCacheSourceDiscovery.h"

#include "Algo/Reverse.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Hash/Blake3.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheProductionSourceDiscoveryTests_Private
{
	static FAngelscriptHash256 HashString(
		const FStringView Domain,
		const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static TArray<uint8> Utf8Bytes(const FStringView Value)
	{
		const FTCHARToUTF8 Utf8(Value.GetData(), Value.Len());
		TArray<uint8> Bytes;
		Bytes.Append(
			reinterpret_cast<const uint8*>(Utf8.Get()),
			Utf8.Length());
		return Bytes;
	}

	static FAngelscriptSourceProviderDescriptor MakeKnownDescriptor(
		const FString& StableInstance,
		const EAngelscriptSourceProviderDescriptorKind Kind =
			EAngelscriptSourceProviderDescriptorKind::External)
	{
		FAngelscriptSourceProviderDescriptor Descriptor;
		Descriptor.Kind = Kind;
		Descriptor.CanonicalImplementationIdentity =
			TEXT("Angelscript.Test.DiscoveryProvider.V1");
		Descriptor.StableInstanceIdentity = StableInstance;
		Descriptor.Version = FString(TEXT("1"));
		Descriptor.Configuration = FString(TEXT("deterministic-test-v1"));
		return Descriptor;
	}

	struct FDiscoveryProvider final : IAngelscriptSourceProvider
	{
		TArray<FAngelscriptSource> Sources;
		TMap<FString, TArray<uint8>> BytesByVirtualPath;
		TMap<FString, FAngelscriptSourceProviderDescriptor> DescriptorByVirtualPath;
		TSet<FString> FailedLoads;
		int32 FindCallCount = 0;
		int32 ByteLoadCallCount = 0;

		virtual void FindSources(
			const TArray<FAngelscriptSourceRoot>& ScriptRoots,
			bool bSkipDevelopmentScripts,
			bool bSkipEditorScripts,
			TArray<FAngelscriptSource>& OutSources) override
		{
			++FindCallCount;
			OutSources.Append(Sources);
		}

		virtual bool LoadSourceText(
			const FAngelscriptSource& Source,
			FString& OutSourceText) override
		{
			const TArray<uint8>* Bytes =
				BytesByVirtualPath.Find(Source.VirtualPath.ToString());
			if (Bytes == nullptr)
			{
				return false;
			}
			FUTF8ToTCHAR Text(
				reinterpret_cast<const ANSICHAR*>(Bytes->GetData()),
				Bytes->Num());
			OutSourceText = FString(Text.Length(), Text.Get());
			return true;
		}

		virtual bool LoadSourceBytes(
			const FAngelscriptSource& Source,
			TArray<uint8>& OutSourceBytes) override
		{
			++ByteLoadCallCount;
			OutSourceBytes.Reset();
			if (FailedLoads.Contains(Source.VirtualPath.ToString()))
			{
				return false;
			}
			if (Source.bHasSourceText)
			{
				OutSourceBytes = Utf8Bytes(Source.SourceText);
				return true;
			}
			const TArray<uint8>* Bytes =
				BytesByVirtualPath.Find(Source.VirtualPath.ToString());
			if (Bytes == nullptr)
			{
				return false;
			}
			OutSourceBytes = *Bytes;
			return true;
		}

		virtual bool QuerySourceState(
			const FAngelscriptSource& Source,
			FAngelscriptSourceState& OutState) override
		{
			OutState.Timestamp = FDateTime(2026, 8, 10);
			OutState.ContentHash = GetTypeHash(Source.VirtualPath.ToString());
			OutState.bHasContentHash = true;
			return true;
		}

		virtual bool QuerySourceDescriptor(
			const FAngelscriptSource& Source,
			FAngelscriptSourceProviderDescriptor& OutDescriptor) override
		{
			const FAngelscriptSourceProviderDescriptor* Descriptor =
				DescriptorByVirtualPath.Find(Source.VirtualPath.ToString());
			if (Descriptor == nullptr)
			{
				OutDescriptor = {};
				return false;
			}
			OutDescriptor = *Descriptor;
			return true;
		}
	};

	static FAngelscriptCacheProductionSourceDiscoveryConfig MakeConfig()
	{
		FAngelscriptCacheProductionSourceDiscoveryConfig Config;
		Config.Profile.Hash = HashString(
			TEXT("production-discovery-profile"),
			TEXT("EditorDevelopment"));
		Config.DiscoveryPolicyVersion = 2;
		Config.Options.Add({
			EAngelscriptCacheDirectOptionKind::Preprocessor,
			TEXT("WITH_EDITOR"), TEXT("1")});
		Config.bObserveLegacyGlobalPreprocessHooks = false;
		return Config;
	}

	static void AddDescribedSource(
		FDiscoveryProvider& Provider,
		const FAngelscriptSource& Source,
		const FStringView Text,
		const FAngelscriptSourceProviderDescriptor& Descriptor)
	{
		Provider.Sources.Add(Source);
		Provider.BytesByVirtualPath.Add(
			Source.VirtualPath.ToString(), Utf8Bytes(Text));
		Provider.DescriptorByVirtualPath.Add(
			Source.VirtualPath.ToString(), Descriptor);
	}

	static const FAngelscriptCacheModuleSourcePlan* FindModule(
		const FAngelscriptCacheProductionSourceDiscoveryResult& Result,
		const FStringView ModuleName)
	{
		for (const FAngelscriptCacheModuleSourcePlan& Module : Result.Modules)
		{
			if (Module.CanonicalModuleName == ModuleName)
			{
				return &Module;
			}
		}
		return nullptr;
	}

	static FAngelscriptCacheSourceDiscoveryStatus Discover(
		FDiscoveryProvider& Provider,
		const FAngelscriptCacheProductionSourceDiscoveryConfig& Config,
		FAngelscriptCacheProductionSourceDiscoveryResult& OutResult,
		const FAngelscriptCacheDirectSourceLimits& Limits = {})
	{
		const TArray<FAngelscriptSourceRoot> Roots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("Q:/RelocatableProject/Script")),
			FAngelscriptSourceRoot::FromPluginRoot(
				TEXT("Inventory"),
				TEXT("Q:/RelocatableProject/Plugins/Inventory/Script")),
		};
		return FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
			Provider,
			Roots,
			false,
			false,
			Config,
			Limits,
			OutResult);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheProductionSourceDiscoveryTests,
	"Angelscript.TestModule.Cache.ProductionSourceDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(GamePluginAndMemoryDiscoveryIsDeterministicAndRelocatable)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		const FAngelscriptSourceProviderDescriptor DiskDescriptor =
			MakeKnownDescriptor(TEXT("primary-disk"));
		AddDescribedSource(
			Provider,
			FAngelscriptSource::FromGameFile(
				TEXT("Gameplay/Player.as"),
				TEXT("Q:/RelocatableProject/Script/Gameplay/Player.as")),
			TEXT("int PlayerEntry() { return 1; }"),
			DiskDescriptor);
		AddDescribedSource(
			Provider,
			FAngelscriptSource::FromPluginFile(
				TEXT("Inventory"),
				TEXT("Items/Item.as"),
				TEXT("Q:/RelocatableProject/Plugins/Inventory/Script/Items/Item.as")),
			TEXT("int ItemEntry() { return 2; }"),
			DiskDescriptor);
		Provider.Sources.Add(FAngelscriptSource::FromMemorySource(
			TEXT("/Angelscript/Memory/Generated/Runtime.as"),
			TEXT("int RuntimeEntry() { return 3; }")));

		const FAngelscriptCacheProductionSourceDiscoveryConfig Config =
			MakeConfig();
		FAngelscriptCacheProductionSourceDiscoveryResult First;
		const FAngelscriptCacheSourceDiscoveryStatus FirstStatus =
			Discover(Provider, Config, First);
		ASSERT_THAT(IsTrue(FirstStatus.IsSuccess(), *FirstStatus.Detail));
		ASSERT_THAT(AreEqual(3, First.Modules.Num()));
		ASSERT_THAT(AreEqual(3, First.DirectPlan.DirectProjection.Files.Num()));
		ASSERT_THAT(AreEqual(3, First.DirectPlan.DirectProjection.Mounts.Num()));
		ASSERT_THAT(AreEqual(2, First.DirectPlan.DirectProjection.Providers.Num()));

		for (const FAngelscriptCachedSourceMount& Mount :
			First.DirectPlan.DirectProjection.Mounts)
		{
			ASSERT_THAT(IsFalse(Mount.LogicalMount.Contains(TEXT(":"))));
			ASSERT_THAT(IsFalse(Mount.LogicalMount.Contains(TEXT("RelocatableProject"))));
		}
		for (const FAngelscriptCachedSourceFile& File :
			First.DirectPlan.DirectProjection.Files)
		{
			ASSERT_THAT(IsFalse(File.RelativeLogicalPath.Contains(TEXT(":"))));
			ASSERT_THAT(IsFalse(File.RelativeLogicalPath.StartsWith(TEXT("/"))));
		}

		Algo::Reverse(Provider.Sources);
		FAngelscriptCacheProductionSourceDiscoveryResult Reordered;
		const FAngelscriptCacheSourceDiscoveryStatus ReorderedStatus =
			Discover(Provider, Config, Reordered);
		ASSERT_THAT(IsTrue(ReorderedStatus.IsSuccess(), *ReorderedStatus.Detail));
		ASSERT_THAT(AreEqual(
			First.DirectPlan.DirectInputDigest.ToHexString(),
			Reordered.DirectPlan.DirectInputDigest.ToHexString(),
			TEXT("provider enumeration order must not change the direct digest")));
		ASSERT_THAT(AreEqual(
			First.Modules[0].ModuleKey.Hash.ToHexString(),
			Reordered.Modules[0].ModuleKey.Hash.ToHexString(),
			TEXT("module plans must have deterministic key order")));
	}

	TEST_METHOD(AddDeleteAndRenameRejectTheOldDirectCandidate)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		const FAngelscriptSourceProviderDescriptor Descriptor =
			MakeKnownDescriptor(TEXT("inventory"));
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(TEXT("A.as"), TEXT("Q:/Script/A.as")),
			TEXT("int A() { return 1; }"), Descriptor);
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(TEXT("B.as"), TEXT("Q:/Script/B.as")),
			TEXT("int B() { return 2; }"), Descriptor);
		const FAngelscriptCacheProductionSourceDiscoveryConfig Config = MakeConfig();

		FAngelscriptCacheProductionSourceDiscoveryResult Baseline;
		ASSERT_THAT(IsTrue(Discover(Provider, Config, Baseline).IsSuccess()));

		auto ExpectDirectMiss = [&](const TCHAR* Label)
		{
			FAngelscriptCacheProductionSourceDiscoveryResult Current;
			ASSERT_THAT(IsTrue(Discover(Provider, Config, Current).IsSuccess()));
			FAngelscriptCacheDependencyCandidateResult CandidateResult;
			const FAngelscriptCacheValidationResult Validation =
				FAngelscriptCacheSourcePlanner::ValidatePersistedDependencyCandidate(
					Current.DirectPlan,
					Config.Profile,
					Baseline.DirectPlan.DirectProjection,
					{},
					{},
					CandidateResult);
			ASSERT_THAT(IsTrue(Validation.IsSuccess(), Label));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheDependencyCandidateMatch::DirectInputMismatch,
				CandidateResult.Match,
				Label));
		};

		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(TEXT("C.as"), TEXT("Q:/Script/C.as")),
			TEXT("int C() { return 3; }"), Descriptor);
		ExpectDirectMiss(TEXT("an added file must reject the old candidate"));

		Provider.Sources.RemoveAt(2);
		Provider.Sources.RemoveAt(1);
		ExpectDirectMiss(TEXT("a deleted file must reject the old candidate"));

		Provider.Sources.Add(FAngelscriptSource::FromGameFile(
			TEXT("RenamedB.as"), TEXT("Q:/Script/RenamedB.as")));
		Provider.BytesByVirtualPath.Add(
			TEXT("/Angelscript/Game/RenamedB.as"),
			Utf8Bytes(TEXT("int B() { return 2; }")));
		Provider.DescriptorByVirtualPath.Add(
			TEXT("/Angelscript/Game/RenamedB.as"), Descriptor);
		ExpectDirectMiss(TEXT("a renamed file must reject the old candidate"));
	}

	TEST_METHOD(CrossProviderSimpleFoldPathCollisionFailsAtomically)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(
				TEXT("Actors/Hero.as"), TEXT("Q:/One/Actors/Hero.as")),
			TEXT("int Hero() { return 1; }"),
			MakeKnownDescriptor(TEXT("one")));
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(
				TEXT("actors/hero.as"), TEXT("Q:/Two/actors/hero.as")),
			TEXT("int OtherHero() { return 2; }"),
			MakeKnownDescriptor(TEXT("two")));

		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		Result.Modules.AddDefaulted();
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, MakeConfig(), Result);
		ASSERT_THAT(IsFalse(Status.IsSuccess()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::CaseCollision,
			Status.Validation.Error));
		ASSERT_THAT(IsTrue(Result.Modules.IsEmpty()));
		ASSERT_THAT(IsTrue(Result.DirectPlan.DirectProjection.Files.IsEmpty()));
	}

	TEST_METHOD(MismatchedDerivedModuleNameFailsBeforeReading)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		FAngelscriptSource Source = FAngelscriptSource::FromGameFile(
			TEXT("Expected.as"), TEXT("Q:/Script/Expected.as"));
		Source.ModuleName = TEXT("Injected.OtherModule");
		AddDescribedSource(
			Provider,
			Source,
			TEXT("int Expected() { return 1; }"),
			MakeKnownDescriptor(TEXT("module-coordinate")));

		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		Result.Modules.AddDefaulted();
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, MakeConfig(), Result);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
			Status.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::InvalidLogicalPath,
			Status.Validation.Error));
		ASSERT_THAT(AreEqual(
			0,
			Provider.ByteLoadCallCount,
			TEXT("an invalid stable module coordinate must be rejected before source I/O")));
		ASSERT_THAT(IsTrue(Result.Modules.IsEmpty()));
		ASSERT_THAT(IsTrue(Result.DirectPlan.DirectProjection.Files.IsEmpty()));
	}

	TEST_METHOD(ModuleNameBudgetIsTypedBeforeReading)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		AddDescribedSource(
			Provider,
			FAngelscriptSource::FromGameFile(
				TEXT("LongModuleName.as"), TEXT("Q:/Script/LongModuleName.as")),
			TEXT("int LongModuleName() { return 1; }"),
			MakeKnownDescriptor(TEXT("module-budget")));
		FAngelscriptCacheDirectSourceLimits Limits;
		Limits.MaxCanonicalStringCharacters = 4;

		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		Result.Modules.AddDefaulted();
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, MakeConfig(), Result, Limits);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSourceDiscoveryError::DirectPlanRejected,
			Status.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Status.Validation.Error));
		ASSERT_THAT(AreEqual(
			0,
			Provider.ByteLoadCallCount,
			TEXT("an oversized module coordinate must fail before source I/O")));
		ASSERT_THAT(IsTrue(Result.Modules.IsEmpty()));
		ASSERT_THAT(IsTrue(Result.DirectPlan.DirectProjection.Files.IsEmpty()));
	}

	TEST_METHOD(SingleRawSourceBudgetFailsAtomically)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		AddDescribedSource(
			Provider,
			FAngelscriptSource::FromGameFile(TEXT("A.as"), TEXT("Q:/Script/A.as")),
			TEXT("abcd"),
			MakeKnownDescriptor(TEXT("single-byte-budget")));
		FAngelscriptCacheDirectSourceLimits Limits;
		Limits.MaxSingleRawSourceBytes = 3;
		Limits.MaxTotalRawSourceBytes = 16;

		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		Result.Modules.AddDefaulted();
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, MakeConfig(), Result, Limits);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSourceDiscoveryError::DirectPlanRejected,
			Status.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Status.Validation.Error));
		ASSERT_THAT(AreEqual(1, Provider.ByteLoadCallCount));
		ASSERT_THAT(IsTrue(Result.Modules.IsEmpty()));
		ASSERT_THAT(IsTrue(Result.DirectPlan.DirectProjection.Files.IsEmpty()));
	}

	TEST_METHOD(TotalRawSourceBudgetStopsAtFirstExcess)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		const FAngelscriptSourceProviderDescriptor Descriptor =
			MakeKnownDescriptor(TEXT("total-byte-budget"));
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(TEXT("A.as"), TEXT("Q:/Script/A.as")),
			TEXT("abcd"), Descriptor);
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(TEXT("B.as"), TEXT("Q:/Script/B.as")),
			TEXT("efgh"), Descriptor);
		FAngelscriptCacheDirectSourceLimits Limits;
		Limits.MaxSingleRawSourceBytes = 4;
		Limits.MaxTotalRawSourceBytes = 7;

		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		Result.Modules.AddDefaulted();
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, MakeConfig(), Result, Limits);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSourceDiscoveryError::DirectPlanRejected,
			Status.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Status.Validation.Error));
		ASSERT_THAT(AreEqual(2, Provider.ByteLoadCallCount));
		ASSERT_THAT(IsTrue(Result.Modules.IsEmpty()));
		ASSERT_THAT(IsTrue(Result.DirectPlan.DirectProjection.Files.IsEmpty()));
	}

	TEST_METHOD(UnidentifiedProviderDoesNotPoisonIntrinsicMemoryModule)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		const FAngelscriptSource Unknown = FAngelscriptSource::FromGameFile(
			TEXT("Unknown/External.as"), TEXT("Q:/Unknown/External.as"));
		Provider.Sources.Add(Unknown);
		Provider.BytesByVirtualPath.Add(
			Unknown.VirtualPath.ToString(),
			Utf8Bytes(TEXT("int External() { return 1; }")));
		Provider.Sources.Add(FAngelscriptSource::FromMemorySource(
			TEXT("/Angelscript/Memory/Safe/Safe.as"),
			TEXT("int Safe() { return 2; }")));

		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, MakeConfig(), Result);
		ASSERT_THAT(IsTrue(Status.IsSuccess(), *Status.Detail));
		const FAngelscriptCacheModuleSourcePlan* UnknownModule =
			FindModule(Result, TEXT("Unknown.External"));
		const FAngelscriptCacheModuleSourcePlan* MemoryModule =
			FindModule(Result, TEXT("Angelscript.Memory.Safe.Safe"));
		ASSERT_THAT(IsNotNull(UnknownModule));
		ASSERT_THAT(IsNotNull(MemoryModule));
		ASSERT_THAT(IsFalse(UnknownModule->bExactFastPathEligible));
		ASSERT_THAT(AreEqual(3, UnknownModule->MatchingIneligibleScopes.Num()));
		ASSERT_THAT(IsTrue(MemoryModule->bExactFastPathEligible));
		ASSERT_THAT(IsTrue(MemoryModule->MatchingIneligibleScopes.IsEmpty()));
	}

	TEST_METHOD(BuiltInDiskProviderHashesExactStrictUtf8Bytes)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		const FString FixtureRoot = FPaths::ProjectSavedDir()
			/ TEXT("Automation/CacheV33DiskDiscovery");
		IFileManager::Get().DeleteDirectory(*FixtureRoot, false, true);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*FixtureRoot, false, true);
		};
		IFileManager::Get().MakeDirectory(*FixtureRoot, true);
		const FString SourcePath = FixtureRoot / TEXT("Exact.as");
		TArray<uint8> ExactBytes{0xef, 0xbb, 0xbf};
		ExactBytes.Append(Utf8Bytes(
			TEXT("int Exact() { return 7; }\r\n")));
		ASSERT_THAT(IsTrue(FFileHelper::SaveArrayToFile(
			ExactBytes, *SourcePath)));

		FAngelscriptDiskSourceProvider Provider;
		const TArray<FAngelscriptSourceRoot> Roots{
			FAngelscriptSourceRoot::FromGameRoot(FixtureRoot)};
		const FAngelscriptCacheProductionSourceDiscoveryConfig Config =
			MakeConfig();
		FAngelscriptCacheProductionSourceDiscoveryResult First;
		FAngelscriptCacheSourceDiscoveryStatus Status =
			FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
				Provider, Roots, false, false, Config, {}, First);
		ASSERT_THAT(IsTrue(Status.IsSuccess(), *Status.Detail));
		ASSERT_THAT(AreEqual(1, First.DirectPlan.DirectProjection.Files.Num()));
		const FAngelscriptHash256 ExpectedRawHash{
			FBlake3::HashBuffer(
				ExactBytes.GetData(),
				static_cast<uint64>(ExactBytes.Num()))};
		ASSERT_THAT(AreEqual(
			ExpectedRawHash.ToHexString(),
			First.DirectPlan.DirectProjection.Files[0].RawContentHash.ToHexString(),
			TEXT("disk discovery must hash BOM and CRLF from exact file bytes")));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachedSourceProviderKind::BuiltInDisk,
			First.DirectPlan.DirectProjection.Providers[0].ProviderKind));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(
				EAngelscriptCachedFingerprintCapabilityFlags::KnownMask),
			First.DirectPlan.DirectProjection.Providers[0].CapabilityFlags));
		ASSERT_THAT(IsTrue(First.Modules[0].bExactFastPathEligible));

		TArray<uint8> WithoutBom = Utf8Bytes(
			TEXT("int Exact() { return 7; }\r\n"));
		ASSERT_THAT(IsTrue(FFileHelper::SaveArrayToFile(
			WithoutBom, *SourcePath)));
		FAngelscriptCacheProductionSourceDiscoveryResult Changed;
		Status = FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
			Provider, Roots, false, false, Config, {}, Changed);
		ASSERT_THAT(IsTrue(Status.IsSuccess(), *Status.Detail));
		ASSERT_THAT(IsFalse(
			First.DirectPlan.DirectInputDigest
				== Changed.DirectPlan.DirectInputDigest,
			TEXT("removing only the physical UTF-8 BOM is still a raw input change")));
	}

	TEST_METHOD(ExplicitUnstableHookAffectsOnlyItsModule)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		const FAngelscriptSourceProviderDescriptor Descriptor =
			MakeKnownDescriptor(TEXT("hook-scope"));
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(TEXT("A.as"), TEXT("Q:/Script/A.as")),
			TEXT("int A() { return 1; }"), Descriptor);
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(TEXT("B.as"), TEXT("Q:/Script/B.as")),
			TEXT("int B() { return 2; }"), Descriptor);

		const TOptional<FAngelscriptStableModuleKey> AKey =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				FAngelscriptVirtualPath::GameRoot, TEXT("A.as"), TEXT("A"));
		ASSERT_THAT(IsTrue(AKey.IsSet()));
		FAngelscriptCachedPreprocessHook Hook;
		Hook.Phase = EAngelscriptCachedPreprocessHookPhase::ProcessChunks;
		Hook.CanonicalImplementationIdentity =
			TEXT("Angelscript.Test.PartiallyFingerprintedHook");
		Hook.AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Module;
		Hook.AffectedScopeStableKey = AKey->Hash;
		Hook.IdentityFingerprint = HashString(
			TEXT("production-hook-identity"),
			Hook.CanonicalImplementationIdentity);
		Hook.ConfigurationFingerprint = HashString(
			TEXT("production-hook-config"), TEXT("default"));
		Hook.ContentFingerprint = HashString(
			TEXT("production-hook-content"), TEXT("v1"));
		Hook.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity)
			| static_cast<uint32>(
				EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint)
			| static_cast<uint32>(
				EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint);
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(
				{Hook.Phase,
					Hook.CanonicalImplementationIdentity,
					Hook.AffectedScopeKind,
					Hook.AffectedScopeStableKey},
				Hook.HookKey).IsSuccess()));

		FAngelscriptCacheProductionSourceDiscoveryConfig Config = MakeConfig();
		Config.PreprocessHooks.Add(Hook);
		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, Config, Result);
		ASSERT_THAT(IsTrue(Status.IsSuccess(), *Status.Detail));
		const FAngelscriptCacheModuleSourcePlan* A = FindModule(Result, TEXT("A"));
		const FAngelscriptCacheModuleSourcePlan* B = FindModule(Result, TEXT("B"));
		ASSERT_THAT(IsNotNull(A));
		ASSERT_THAT(IsNotNull(B));
		ASSERT_THAT(IsFalse(A->bExactFastPathEligible));
		ASSERT_THAT(AreEqual(1, A->MatchingIneligibleScopes.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint,
			A->MatchingIneligibleScopes[0].Reason));
		ASSERT_THAT(IsTrue(B->bExactFastPathEligible));
		ASSERT_THAT(IsTrue(B->MatchingIneligibleScopes.IsEmpty()));
	}

	TEST_METHOD(BoundLegacyGlobalHookFailsClosedForEveryCurrentModule)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		AddDescribedSource(Provider,
			FAngelscriptSource::FromGameFile(TEXT("Hooked.as"), TEXT("Q:/Script/Hooked.as")),
			TEXT("int Hooked() { return 1; }"),
			MakeKnownDescriptor(TEXT("legacy-hook")));
		const FDelegateHandle Handle =
			FAngelscriptPreprocessor::OnProcessChunks.AddLambda(
				[](FAngelscriptPreprocessor&) {});
		ON_SCOPE_EXIT
		{
			FAngelscriptPreprocessor::OnProcessChunks.Remove(Handle);
		};

		FAngelscriptCacheProductionSourceDiscoveryConfig Config = MakeConfig();
		Config.bObserveLegacyGlobalPreprocessHooks = true;
		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, Config, Result);
		ASSERT_THAT(IsTrue(Status.IsSuccess(), *Status.Detail));
		ASSERT_THAT(AreEqual(1, Result.Modules.Num()));
		ASSERT_THAT(IsFalse(Result.Modules[0].bExactFastPathEligible));
		ASSERT_THAT(AreEqual(
			4, Result.Modules[0].MatchingIneligibleScopes.Num(),
			TEXT("an opaque legacy delegate must expose every missing hook capability")));
	}

	TEST_METHOD(ReadFailureAndInvalidUtf8ResetTheWholeResult)
	{
		using namespace AngelscriptCacheProductionSourceDiscoveryTests_Private;
		FDiscoveryProvider Provider;
		const FAngelscriptSource Source = FAngelscriptSource::FromGameFile(
			TEXT("Broken.as"), TEXT("Q:/Script/Broken.as"));
		AddDescribedSource(
			Provider, Source, TEXT("int Broken() { return 1; }"),
			MakeKnownDescriptor(TEXT("broken")));
		Provider.FailedLoads.Add(Source.VirtualPath.ToString());

		FAngelscriptCacheProductionSourceDiscoveryResult Result;
		Result.Modules.AddDefaulted();
		FAngelscriptCacheSourceDiscoveryStatus Status =
			Discover(Provider, MakeConfig(), Result);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSourceDiscoveryError::SourceReadFailed,
			Status.Error));
		ASSERT_THAT(IsTrue(Result.Modules.IsEmpty()));

		Provider.FailedLoads.Reset();
		Provider.BytesByVirtualPath[Source.VirtualPath.ToString()] = {
			0xf0, 0x28, 0x8c, 0x28};
		Result.Modules.AddDefaulted();
		Status = Discover(Provider, MakeConfig(), Result);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSourceDiscoveryError::InvalidSourceEncoding,
			Status.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::InvalidUtf8,
			Status.Validation.Error));
		ASSERT_THAT(IsTrue(Result.Modules.IsEmpty()));
	}
};

#endif
