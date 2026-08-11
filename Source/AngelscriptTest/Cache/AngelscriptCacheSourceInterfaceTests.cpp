#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "Async/Async.h"
#include "CQTest.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheSourceInterfaceTests_Private
{
	using FPublicDecodedRecordFactory = decltype(&FAngelscriptDecodedCacheRecord::TryDecode);
	using FPublicEligibilityQuery = FAngelscriptCacheValidationResult (*)(
		const FAngelscriptDecodedCacheRecord&,
		const FAngelscriptStableModuleKey&,
		const FAngelscriptCacheReadLimits&,
		FAngelscriptCacheReadBudget&,
		FAngelscriptCacheExactFastPathEligibility&);
	static_assert(std::is_same_v<decltype(static_cast<FPublicEligibilityQuery>(
		&FAngelscriptCacheSemanticArchive::QueryExactFastPathEligibility)),
		FPublicEligibilityQuery>);

	static_assert(std::is_invocable_r_v<FAngelscriptCacheValidationResult,
		FPublicDecodedRecordFactory,
		const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
		const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
		TOptional<FAngelscriptDecodedCacheRecordHandle>&>,
		"The sole production factory must publish the common immutable handle");
	static_assert(!std::is_invocable_r_v<FAngelscriptCacheValidationResult,
		FPublicEligibilityQuery,
		const FAngelscriptCachedSourceIndex&, const FAngelscriptStableModuleKey&,
		const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
		FAngelscriptCacheExactFastPathEligibility&>,
		"A raw SourceIndex DTO must not be accepted by the eligibility query");
	static_assert(std::is_copy_constructible_v<FAngelscriptDecodedCacheRecordHandle>
		&& std::is_copy_assignable_v<FAngelscriptDecodedCacheRecordHandle>,
		"Common handles share one immutable decoded record without deep copy");
	static_assert(!std::is_copy_constructible_v<FAngelscriptCacheReadBudget>
		&& !std::is_copy_assignable_v<FAngelscriptCacheReadBudget>
		&& !std::is_move_constructible_v<FAngelscriptCacheReadBudget>
		&& !std::is_move_assignable_v<FAngelscriptCacheReadBudget>,
		"A read budget owns active reservation guards and must never be copied or moved");

	static FAngelscriptHash256 MakeHash(const uint8 Fill)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Fill, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCacheValidationResult DecodeRecordForTests(
		const EAngelscriptCacheRecordKind Kind,
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
	{
		FAngelscriptCacheRecordId RecordId;
		const FAngelscriptCacheValidationResult IdResult =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(Kind, Payload, RecordId);
		if (!IdResult.IsSuccess())
		{
			OutRecord.Reset();
			return IdResult;
		}
		return FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId, Payload, Limits, Budget, OutRecord);
	}

	class FDecodedSourceIndexHandleForTests final
	{
	public:
		FDecodedSourceIndexHandleForTests() = default;
		FDecodedSourceIndexHandleForTests(
			const FDecodedSourceIndexHandleForTests&) = delete;
		FDecodedSourceIndexHandleForTests& operator=(
			const FDecodedSourceIndexHandleForTests&) = delete;

		FDecodedSourceIndexHandleForTests(
			FDecodedSourceIndexHandleForTests&& Other) noexcept
			: Record(MoveTemp(Other.Record))
		{
			Other.Record.Reset();
		}

		FDecodedSourceIndexHandleForTests& operator=(
			FDecodedSourceIndexHandleForTests&& Other) noexcept
		{
			if (this != &Other)
			{
				Record = MoveTemp(Other.Record);
				Other.Record.Reset();
			}
			return *this;
		}

		bool IsValid() const { return Record.IsSet(); }

		const FAngelscriptCachedSourceIndex& GetValue() const
		{
			check(Record.IsSet());
			const FAngelscriptCachedSourceIndex* Source =
				Record.GetValue()->TryGetSourceIndex();
			check(Source != nullptr);
			return *Source;
		}

		const FAngelscriptDecodedCacheRecord& GetRecord() const
		{
			check(Record.IsSet());
			return *Record.GetValue();
		}

		void Reset() { Record.Reset(); }

		void Set(const FAngelscriptDecodedCacheRecordHandle& InRecord)
		{
			check(InRecord->TryGetSourceIndex() != nullptr);
			Record = InRecord;
		}

	private:
		TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
	};

	static FAngelscriptCacheValidationResult DecodeSourceIndexHandleForTests(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FDecodedSourceIndexHandleForTests& OutRecord)
	{
		OutRecord.Reset();
		TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
		const FAngelscriptCacheValidationResult Result = DecodeRecordForTests(
			EAngelscriptCacheRecordKind::SourceIndex,
			Payload, Limits, Budget, Record);
		if (Result.IsSuccess())
		{
			check(Record.IsSet());
			OutRecord.Set(Record.GetValue());
		}
		return Result;
	}

	static FAngelscriptCacheValidationResult QueryExactFastPathEligibilityForTests(
		const FDecodedSourceIndexHandleForTests& SourceIndex,
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheExactFastPathEligibility& OutResult)
	{
		if (!SourceIndex.IsValid())
		{
			OutResult.Reset();
			return FAngelscriptCacheValidationResult(
				EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
		return FAngelscriptCacheSemanticArchive::QueryExactFastPathEligibility(
			SourceIndex.GetRecord(), ModuleKey, Limits, Budget, OutResult);
	}

	static FAngelscriptCacheValidationResult
		QueryExactFastPathEligibilityWithAllocationCaptureForTests(
			const FDecodedSourceIndexHandleForTests& SourceIndex,
			const FAngelscriptStableModuleKey& ModuleKey,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			AngelscriptCacheEligibilityTestHooks::FAllocationEventCaptureView Capture,
			FAngelscriptCacheExactFastPathEligibility& OutResult)
	{
		if (!SourceIndex.IsValid())
		{
			OutResult.Reset();
			return FAngelscriptCacheValidationResult(
				EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
		return FAngelscriptCacheSemanticArchive::
			QueryExactFastPathEligibilityWithAllocationCaptureForTests(
				SourceIndex.GetRecord(), ModuleKey, Limits, Budget, Capture, OutResult);
	}

	static FAngelscriptCacheValidationResult DecodeSourceIndexForTests(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCachedSourceIndex& OutValue)
	{
		OutValue = {};
		TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
		const FAngelscriptCacheValidationResult Result = DecodeRecordForTests(
			EAngelscriptCacheRecordKind::SourceIndex,
			Payload, Limits, Budget, Record);
		if (Result.IsSuccess())
		{
			check(Record.IsSet());
			const FAngelscriptCachedSourceIndex* Source =
				Record.GetValue()->TryGetSourceIndex();
			check(Source != nullptr);
			OutValue = *Source;
		}
		return Result;
	}

	static FAngelscriptCacheValidationResult DecodeModuleInterfaceForTests(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCachedModuleInterface& OutValue)
	{
		OutValue = {};
		TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
		const FAngelscriptCacheValidationResult Result = DecodeRecordForTests(
			EAngelscriptCacheRecordKind::ModuleInterface,
			Payload, Limits, Budget, Record);
		if (Result.IsSuccess())
		{
			check(Record.IsSet());
			const FAngelscriptCachedModuleInterface* Interface =
				Record.GetValue()->TryGetModuleInterface();
			check(Interface != nullptr);
			OutValue = *Interface;
		}
		return Result;
	}

	struct FIndexedStableHashScratchProbe
	{
		FAngelscriptHash256 Hash;
		int32 ValueIndex = INDEX_NONE;
	};

	template <typename ElementType>
	static int32 CalculateArrayReserveCapacityForTests(const int32 RequestedCapacity)
	{
		if (RequestedCapacity <= 0)
		{
			return 0;
		}
		using FArrayType = TArray<ElementType>;
		typename FArrayType::ElementAllocatorType Allocator;
		if constexpr (TAllocatorTraits<typename FArrayType::AllocatorType>::SupportsElementAlignment)
		{
			return Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			return Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType));
		}
	}

	static FString Hex(const TConstArrayView<uint8> Bytes)
	{
		return BytesToHexLower(Bytes.GetData(), Bytes.Num());
	}

	static FAngelscriptStableModuleKey MakeModuleKey()
	{
		const TOptional<FAngelscriptStableModuleKey> Key =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("Game"), TEXT("Actors/Hero.as"), TEXT("Hero"));
		check(Key.IsSet());
		return Key.GetValue();
	}

	static FAngelscriptCachedSourceProviderKey BuildProviderKey(
		const FAngelscriptCachedSourceProvider& Provider)
	{
		FAngelscriptCachedSourceProviderKey Key;
		const FAngelscriptSourceProviderIdentityInput Input{
			Provider.ProviderKind, Provider.CanonicalImplementationIdentity, Provider.IdentityFingerprint};
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(Input, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedSourceMountKey BuildMountKey(
		const FAngelscriptCachedSourceMount& Mount)
	{
		FAngelscriptCachedSourceMountKey Key;
		const FAngelscriptSourceMountIdentityInput Input{
			Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey};
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(Input, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedPreprocessHookKey BuildHookKey(
		const FAngelscriptCachedPreprocessHook& Hook)
	{
		FAngelscriptCachedPreprocessHookKey Key;
		const FAngelscriptPreprocessHookIdentityInput Input{
			Hook.Phase, Hook.CanonicalImplementationIdentity,
			Hook.AffectedScopeKind, Hook.AffectedScopeStableKey};
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(Input, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedSourceFileKey BuildSourceFileKey(
		const FAngelscriptCachedSourceFile& File)
	{
		FAngelscriptCachedSourceFileKey Key;
		const FAngelscriptSourceFileIdentityInput Input{
			File.SourceKind, File.MountKey, File.ProviderKey,
			File.RelativeLogicalPath, File.GeneratedSourceKey};
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(Input, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedPreprocessorInputKey BuildInputKey(
		const FAngelscriptCachedPreprocessorInput& Input)
	{
		FAngelscriptCachedPreprocessorInputKey Key;
		const FAngelscriptPreprocessorInputIdentityInput IdentityInput{
			Input.OwnerScopeStableKey, Input.InputKind, Input.CanonicalName, Input.TargetStableKey};
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(
			IdentityInput, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedSourceEdgeKey BuildEdgeKey(const FAngelscriptCachedSourceEdge& Edge)
	{
		FAngelscriptCachedSourceEdgeKey Key;
		const FAngelscriptSourceEdgeIdentityInput Input{
			Edge.EdgeKind, Edge.FromSourceFileKey, Edge.ToSourceOrGeneratedKey,
			Edge.CanonicalIncludeOrGeneratorIdentity};
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceEdgeKey(Input, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedDataType MakePrimitive(const EAngelscriptCachedPrimitiveType Primitive)
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = Primitive;
		return Type;
	}

	static FAngelscriptCachedSourceIndex MakeSourceIndex()
	{
		FAngelscriptCachedSourceIndex Source;
		Source.PayloadSchemaVersion = FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Source.DiscoveryPolicy.PolicyVersion = 1;
		Source.DiscoveryPolicy.FilterFlags =
			static_cast<uint32>(EAngelscriptCachedSourceDiscoveryFilterFlags::SkipEditor);
		Source.DiscoveryPolicy.Options = {
			{TEXT("Defines.MODE"), MakeHash(0x10)},
			{TEXT("Preprocessor.Strict"), MakeHash(0x11)}};

		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity = TEXT("Runtime.DiskSourceProvider");
		Provider.IdentityFingerprint = MakeHash(0x20);
		Provider.VersionFingerprint = MakeHash(0x21);
		Provider.ConfigurationFingerprint = MakeHash(0x22);
		Provider.ContentFingerprint = MakeHash(0x23);
		Provider.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		Provider.ProviderKey = BuildProviderKey(Provider);
		Source.Providers.Add(Provider);

		FAngelscriptCachedSourceMount Mount;
		Mount.SourceKind = EAngelscriptCachedSourceKind::Game;
		Mount.LogicalMount = TEXT("Game");
		Mount.ProviderKey = Provider.ProviderKey;
		Mount.RootConfigurationFingerprint = MakeHash(0x30);
		Mount.Options = {
			{TEXT("Extensions"), MakeHash(0x31)},
			{TEXT("Recursive"), MakeHash(0x32)}};
		Mount.MountKey = BuildMountKey(Mount);
		Source.Mounts.Add(Mount);

		FAngelscriptCachedPreprocessHook Hook;
		Hook.Phase = EAngelscriptCachedPreprocessHookPhase::ProcessChunks;
		Hook.CanonicalImplementationIdentity = TEXT("Runtime.StandardPreprocessor");
		Hook.AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Mount;
		Hook.AffectedScopeStableKey = Mount.MountKey.Hash;
		Hook.IdentityFingerprint = MakeHash(0x40);
		Hook.VersionFingerprint = MakeHash(0x41);
		Hook.ConfigurationFingerprint = MakeHash(0x42);
		Hook.ContentFingerprint = MakeHash(0x43);
		Hook.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		Hook.HookKey = BuildHookKey(Hook);
		Source.PreprocessHooks.Add(Hook);

		FAngelscriptCachedSourceFile File;
		File.SourceKind = EAngelscriptCachedSourceKind::Game;
		File.MountKey = Mount.MountKey;
		File.ProviderKey = Provider.ProviderKey;
		File.RelativeLogicalPath = TEXT("Actors/Hero.as");
		File.RawContentHash = MakeHash(0x50);
		File.ModuleKey = MakeModuleKey();
		File.SourceFileKey = BuildSourceFileKey(File);
		Source.Files.Add(File);

		FAngelscriptCachedPreprocessorInput Input;
		Input.OwnerScopeKind = EAngelscriptCachedFastPathScopeKind::SourceFile;
		Input.OwnerScopeStableKey = File.SourceFileKey.Hash;
		Input.InputKind = EAngelscriptCachePreprocessorInputKind::Define;
		Input.CanonicalName = TEXT("WITH_EDITOR");
		Input.TargetKind = EAngelscriptCachePreprocessorInputTargetKind::None;
		Input.EffectiveValueOrContentHash = MakeHash(0x60);
		Input.InputKey = BuildInputKey(Input);
		Source.PreprocessorInputs.Add(Input);

		FAngelscriptCachedSourceEdge Edge;
		Edge.EdgeKind = EAngelscriptCachedSourceEdgeKind::Include;
		Edge.FromSourceFileKey = File.SourceFileKey;
		Edge.ToSourceOrGeneratedKey = File.SourceFileKey.Hash;
		Edge.CanonicalIncludeOrGeneratorIdentity = TEXT("SelfIncludeFixture");
		Edge.SemanticOrdinal = 0u;
		Edge.EdgeKey = BuildEdgeKey(Edge);
		Source.Edges.Add(Edge);

		const FAngelscriptCacheValidationResult SnapshotResult =
			FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(Source, Source.SourceSnapshot);
		check(SnapshotResult.IsSuccess());
		return Source;
	}

	static FAngelscriptStableModuleKey MakeSecondaryModuleKey()
	{
		const TOptional<FAngelscriptStableModuleKey> Key =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("Game"), TEXT("Actors/Villain.as"), TEXT("Villain"));
		check(Key.IsSet());
		return Key.GetValue();
	}

	static FAngelscriptCacheValidationResult RefreshSourceSnapshot(FAngelscriptCachedSourceIndex& Source)
	{
		Source.SourceSnapshot = {};
		return FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Source, Source.SourceSnapshot);
	}

	static FAngelscriptCacheValidationResult QueryEligibility(
		const FAngelscriptCachedSourceIndex& Source,
		const FAngelscriptStableModuleKey& ModuleKey,
		FAngelscriptCacheExactFastPathEligibility& OutEligibility)
	{
		TArray<uint8> Payload;
		const FAngelscriptCacheValidationResult SerializeResult =
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Payload);
		if (!SerializeResult.IsSuccess())
		{
			OutEligibility.Reset();
			return SerializeResult;
		}
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FDecodedSourceIndexHandleForTests Validated;
		const FAngelscriptCacheValidationResult DecodeResult =
			DecodeSourceIndexHandleForTests(
				Payload, Limits, Budget, Validated);
		if (!DecodeResult.IsSuccess())
		{
			OutEligibility.Reset();
			return DecodeResult;
		}
		return QueryExactFastPathEligibilityForTests(
			Validated, ModuleKey, Limits, Budget, OutEligibility);
	}

	static FAngelscriptCachedFastPathIneligibleScope MakeIneligibleScope(
		const EAngelscriptCachedFastPathScopeKind ScopeKind,
		const FAngelscriptHash256& ScopeKey,
		const EAngelscriptCachedFastPathIneligibleReason Reason,
		const FStringView Diagnostic)
	{
		FAngelscriptCachedFastPathIneligibleScope Scope;
		Scope.ScopeKind = ScopeKind;
		Scope.ScopeStableKey = ScopeKey;
		Scope.Reason = Reason;
		Scope.CanonicalDiagnosticIdentity = FString(Diagnostic);
		return Scope;
	}

	static FAngelscriptCachedPreprocessHook MakeHook(
		const FStringView Identity,
		const EAngelscriptCachedFastPathScopeKind AffectedScopeKind,
		const FAngelscriptHash256& AffectedScopeKey,
		const uint8 FingerprintFill)
	{
		FAngelscriptCachedPreprocessHook Hook;
		Hook.Phase = EAngelscriptCachedPreprocessHookPhase::ProcessChunks;
		Hook.CanonicalImplementationIdentity = FString(Identity);
		Hook.AffectedScopeKind = AffectedScopeKind;
		Hook.AffectedScopeStableKey = AffectedScopeKey;
		Hook.IdentityFingerprint = MakeHash(FingerprintFill);
		Hook.VersionFingerprint = MakeHash(FingerprintFill + 1);
		Hook.ConfigurationFingerprint = MakeHash(FingerprintFill + 2);
		Hook.ContentFingerprint = MakeHash(FingerprintFill + 3);
		Hook.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		Hook.HookKey = BuildHookKey(Hook);
		return Hook;
	}

	static FAngelscriptCachedSourceFile AddSecondaryFile(FAngelscriptCachedSourceIndex& Source)
	{
		FAngelscriptCachedSourceFile File = Source.Files[0];
		File.RelativeLogicalPath = TEXT("Actors/Villain.as");
		File.RawContentHash = MakeHash(0x71);
		File.ModuleKey = MakeSecondaryModuleKey();
		File.SourceFileKey = BuildSourceFileKey(File);
		Source.Files.Add(File);
		return File;
	}

	static void RebuildSingleSourceGraphKeys(FAngelscriptCachedSourceIndex& Source)
	{
		check(Source.Providers.Num() == 1 && Source.Mounts.Num() == 1 && Source.PreprocessHooks.Num() == 1
			&& Source.Files.Num() == 1 && Source.PreprocessorInputs.Num() == 1 && Source.Edges.Num() == 1);
		const FAngelscriptHash256 OldProvider = Source.Providers[0].ProviderKey.Hash;
		const FAngelscriptHash256 OldMount = Source.Mounts[0].MountKey.Hash;
		const FAngelscriptHash256 OldHook = Source.PreprocessHooks[0].HookKey.Hash;
		const FAngelscriptHash256 OldFile = Source.Files[0].SourceFileKey.Hash;

		Source.Providers[0].ProviderKey = BuildProviderKey(Source.Providers[0]);
		Source.Mounts[0].ProviderKey = Source.Providers[0].ProviderKey;
		Source.Mounts[0].MountKey = BuildMountKey(Source.Mounts[0]);
		FAngelscriptCachedPreprocessHook& Hook = Source.PreprocessHooks[0];
		if (Hook.AffectedScopeKind == EAngelscriptCachedFastPathScopeKind::Provider
			&& Hook.AffectedScopeStableKey == OldProvider)
		{
			Hook.AffectedScopeStableKey = Source.Providers[0].ProviderKey.Hash;
		}
		if (Hook.AffectedScopeKind == EAngelscriptCachedFastPathScopeKind::Mount
			&& Hook.AffectedScopeStableKey == OldMount)
		{
			Hook.AffectedScopeStableKey = Source.Mounts[0].MountKey.Hash;
		}
		Hook.HookKey = BuildHookKey(Hook);

		FAngelscriptCachedSourceFile& File = Source.Files[0];
		File.ProviderKey = Source.Providers[0].ProviderKey;
		File.MountKey = Source.Mounts[0].MountKey;
		File.SourceFileKey = BuildSourceFileKey(File);
		FAngelscriptCachedPreprocessorInput& Input = Source.PreprocessorInputs[0];
		if (Input.OwnerScopeStableKey == OldProvider) { Input.OwnerScopeStableKey = Source.Providers[0].ProviderKey.Hash; }
		if (Input.OwnerScopeStableKey == OldMount) { Input.OwnerScopeStableKey = Source.Mounts[0].MountKey.Hash; }
		if (Input.OwnerScopeStableKey == OldHook) { Input.OwnerScopeStableKey = Hook.HookKey.Hash; }
		if (Input.OwnerScopeStableKey == OldFile) { Input.OwnerScopeStableKey = File.SourceFileKey.Hash; }
		if (Input.TargetStableKey.IsSet())
		{
			if (Input.TargetStableKey.GetValue() == OldProvider) { Input.TargetStableKey = Source.Providers[0].ProviderKey.Hash; }
			if (Input.TargetStableKey.GetValue() == OldMount) { Input.TargetStableKey = Source.Mounts[0].MountKey.Hash; }
			if (Input.TargetStableKey.GetValue() == OldHook) { Input.TargetStableKey = Hook.HookKey.Hash; }
			if (Input.TargetStableKey.GetValue() == OldFile) { Input.TargetStableKey = File.SourceFileKey.Hash; }
		}
		Input.InputKey = BuildInputKey(Input);
		FAngelscriptCachedSourceEdge& Edge = Source.Edges[0];
		if (Edge.FromSourceFileKey.Hash == OldFile) { Edge.FromSourceFileKey = File.SourceFileKey; }
		if (Edge.ToSourceOrGeneratedKey == OldFile) { Edge.ToSourceOrGeneratedKey = File.SourceFileKey.Hash; }
		Edge.EdgeKey = BuildEdgeKey(Edge);
		for (FAngelscriptCachedFastPathIneligibleScope& Scope : Source.IneligibleScopes)
		{
			if (Scope.ScopeKind == EAngelscriptCachedFastPathScopeKind::Provider
				&& Scope.ScopeStableKey == OldProvider) { Scope.ScopeStableKey = Source.Providers[0].ProviderKey.Hash; }
			if (Scope.ScopeKind == EAngelscriptCachedFastPathScopeKind::Mount
				&& Scope.ScopeStableKey == OldMount) { Scope.ScopeStableKey = Source.Mounts[0].MountKey.Hash; }
			if (Scope.ScopeKind == EAngelscriptCachedFastPathScopeKind::Hook
				&& Scope.ScopeStableKey == OldHook) { Scope.ScopeStableKey = Hook.HookKey.Hash; }
			if (Scope.ScopeKind == EAngelscriptCachedFastPathScopeKind::SourceFile
				&& Scope.ScopeStableKey == OldFile) { Scope.ScopeStableKey = File.SourceFileKey.Hash; }
		}
	}

	static void RebuildSingleFileKeyAndDependents(FAngelscriptCachedSourceIndex& Source)
	{
		check(Source.Files.Num() == 1 && Source.PreprocessorInputs.Num() == 1
			&& Source.Edges.Num() == 1);
		FAngelscriptCachedSourceFile& File = Source.Files[0];
		const FAngelscriptHash256 OldFileKey = File.SourceFileKey.Hash;
		File.SourceFileKey = BuildSourceFileKey(File);

		FAngelscriptCachedPreprocessorInput& Input = Source.PreprocessorInputs[0];
		if (Input.OwnerScopeKind == EAngelscriptCachedFastPathScopeKind::SourceFile
			&& Input.OwnerScopeStableKey == OldFileKey)
		{
			Input.OwnerScopeStableKey = File.SourceFileKey.Hash;
		}
		if (Input.TargetKind == EAngelscriptCachePreprocessorInputTargetKind::SourceFile
			&& Input.TargetStableKey.IsSet()
			&& Input.TargetStableKey.GetValue() == OldFileKey)
		{
			Input.TargetStableKey = File.SourceFileKey.Hash;
		}
		Input.InputKey = BuildInputKey(Input);

		FAngelscriptCachedSourceEdge& Edge = Source.Edges[0];
		if (Edge.FromSourceFileKey.Hash == OldFileKey)
		{
			Edge.FromSourceFileKey = File.SourceFileKey;
		}
		if (Edge.EdgeKind == EAngelscriptCachedSourceEdgeKind::Include
			&& Edge.ToSourceOrGeneratedKey == OldFileKey)
		{
			Edge.ToSourceOrGeneratedKey = File.SourceFileKey.Hash;
		}
		Edge.EdgeKey = BuildEdgeKey(Edge);

		for (FAngelscriptCachedFastPathIneligibleScope& Scope : Source.IneligibleScopes)
		{
			if (Scope.ScopeKind == EAngelscriptCachedFastPathScopeKind::SourceFile
				&& Scope.ScopeStableKey == OldFileKey)
			{
				Scope.ScopeStableKey = File.SourceFileKey.Hash;
			}
		}
	}

	static FAngelscriptCachedSourceIndex MakeGeneratedSourceIndex()
	{
		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		Source.Providers[0].ProviderKind = EAngelscriptCachedSourceProviderKind::Generated;
		Source.Files[0].GeneratedSourceKey = MakeHash(0x76);
		Source.Files[0].GeneratedConfigurationFingerprint = MakeHash(0x77);
		RebuildSingleSourceGraphKeys(Source);
		return Source;
	}

	static FAngelscriptCachedDeclaration MakeFunctionDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Function;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::GlobalFunction;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Required;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = ModuleKey.Hash;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = TEXT("Tick");
		Declaration.CanonicalDeclaration = TEXT("void Tick(float DeltaSeconds)");
		Declaration.CanonicalIdentityTraits = {TEXT("Callable"), TEXT("Public")};
		Declaration.DeclaredType = MakePrimitive(EAngelscriptCachedPrimitiveType::Void);

		FAngelscriptCachedParameter Parameter;
		Parameter.Ordinal = 0;
		Parameter.CanonicalName = TEXT("DeltaSeconds");
		Parameter.Type = MakePrimitive(EAngelscriptCachedPrimitiveType::Float32);
		Parameter.Passing = EAngelscriptCachedParameterPassing::Value;
		Parameter.TraitFlags = static_cast<uint32>(
			EAngelscriptCachedParameterTraitFlags::BlueprintByValue);
		Declaration.OrderedParameters.Add(Parameter);
		Declaration.ReflectionFlags = static_cast<uint32>(
			EAngelscriptCachedReflectionFlags::BlueprintCallable);
		Declaration.Metadata = {
			{TEXT("Category"), TEXT("Gameplay")},
			{TEXT("ToolTip"), TEXT("Ticks the hero")}};
		Declaration.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Function, 0});

		FAngelscriptFunctionIdentityDescriptor Identity;
		Identity.OwnerKind = Declaration.OwnerKind;
		Identity.OwnerKey = Declaration.OwnerKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
		Declaration.StableKey = FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity).Hash;
		const FAngelscriptCacheValidationResult HashResult =
			FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				Declaration, Declaration.SignatureHash, Declaration.TraitsHash);
		check(HashResult.IsSuccess());
		return Declaration;
	}

	static void FinalizeDeclarationIdentityAndHashes(FAngelscriptCachedDeclaration& Declaration)
	{
		switch (Declaration.DeclarationKind)
		{
		case EAngelscriptCacheDeclarationKind::Type:
		{
			FAngelscriptTypeIdentityDescriptor Identity;
			Identity.ModuleKey = Declaration.ModuleKey;
			Identity.Namespace = Declaration.CanonicalNamespace;
			Identity.Kind = Declaration.EntityKind;
			Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
			Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
			Declaration.StableKey = FAngelscriptArtifactIdentityBuilder::BuildTypeKey(Identity).Hash;
			break;
		}
		case EAngelscriptCacheDeclarationKind::Function:
		{
			FAngelscriptFunctionIdentityDescriptor Identity;
			Identity.OwnerKind = Declaration.OwnerKind;
			Identity.OwnerKey = Declaration.OwnerKey;
			Identity.Namespace = Declaration.CanonicalNamespace;
			Identity.Kind = Declaration.EntityKind;
			Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
			Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
			Declaration.StableKey = FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity).Hash;
			break;
		}
		case EAngelscriptCacheDeclarationKind::Global:
		{
			FAngelscriptGlobalIdentityDescriptor Identity;
			Identity.ModuleKey = Declaration.ModuleKey;
			Identity.Namespace = Declaration.CanonicalNamespace;
			Identity.Kind = Declaration.EntityKind;
			Identity.Name = Declaration.CanonicalName;
			Identity.CanonicalType = Declaration.CanonicalTypeSpelling.GetValue();
			Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
			Declaration.StableKey = FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Identity).Hash;
			break;
		}
		case EAngelscriptCacheDeclarationKind::Property:
		{
			FAngelscriptPropertyIdentityDescriptor Identity;
			Identity.OwnerTypeKey = FAngelscriptStableTypeKey{Declaration.OwnerKey};
			Identity.Kind = Declaration.EntityKind;
			Identity.Name = Declaration.CanonicalName;
			Identity.CanonicalType = Declaration.CanonicalTypeSpelling.GetValue();
			Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
			Declaration.StableKey = FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Identity).Hash;
			break;
		}
		default:
			checkNoEntry();
		}
		const FAngelscriptCacheValidationResult HashResult =
			FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				Declaration, Declaration.SignatureHash, Declaration.TraitsHash);
		check(HashResult.IsSuccess());
	}

	static FAngelscriptCachedDeclaration MakeTypeDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey,
		const EAngelscriptArtifactEntityKind EntityKind,
		const FStringView Name,
		const uint32 DeclarationSlotOrdinal = 0)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Type;
		Declaration.EntityKind = EntityKind;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = ModuleKey.Hash;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = FString(Name);
		Declaration.CanonicalDeclaration = FString::Printf(TEXT("type %s"), *FString(Name));
		Declaration.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Declaration, DeclarationSlotOrdinal});
		FinalizeDeclarationIdentityAndHashes(Declaration);
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakeGlobalDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FStringView Name,
		const uint32 DeclarationSlotOrdinal = 0)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Global;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::GlobalVariable;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = ModuleKey.Hash;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = FString(Name);
		Declaration.CanonicalDeclaration = FString::Printf(TEXT("int %s"), *FString(Name));
		Declaration.CanonicalTypeSpelling = TEXT("int");
		Declaration.DeclaredType = MakePrimitive(EAngelscriptCachedPrimitiveType::Int32);
		Declaration.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Declaration, DeclarationSlotOrdinal});
		FinalizeDeclarationIdentityAndHashes(Declaration);
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakePropertyDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptHash256& OwnerTypeKey,
		const FStringView Name,
		const uint32 DeclarationSlotOrdinal)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Property;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::Property;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
		Declaration.OwnerKey = OwnerTypeKey;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = FString(Name);
		Declaration.CanonicalDeclaration = FString::Printf(TEXT("int %s"), *FString(Name));
		Declaration.CanonicalTypeSpelling = TEXT("int");
		Declaration.DeclaredType = MakePrimitive(EAngelscriptCachedPrimitiveType::Int32);
		Declaration.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Declaration, DeclarationSlotOrdinal});
		FinalizeDeclarationIdentityAndHashes(Declaration);
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakeOwnedFunctionDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey,
		const EAngelscriptArtifactEntityKind EntityKind,
		const EAngelscriptFunctionOwnerKind OwnerKind,
		const FAngelscriptHash256& OwnerKey,
		const FStringView Name,
		const uint32 FunctionSlotOrdinal = 0)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Function;
		Declaration.EntityKind = EntityKind;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EntityKind == EAngelscriptArtifactEntityKind::DelegateSignature
			|| EntityKind == EAngelscriptArtifactEntityKind::ModuleInitializer
			|| EntityKind == EAngelscriptArtifactEntityKind::GlobalInitializer
			? EAngelscriptCacheBodyCoverage::Forbidden
			: EAngelscriptCacheBodyCoverage::Required;
		Declaration.OwnerKind = OwnerKind;
		Declaration.OwnerKey = OwnerKey;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = FString(Name);
		Declaration.CanonicalDeclaration = FString::Printf(TEXT("void %s()"), *FString(Name));
		Declaration.DeclaredType = MakePrimitive(EAngelscriptCachedPrimitiveType::Void);
		Declaration.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Function, FunctionSlotOrdinal});
		FinalizeDeclarationIdentityAndHashes(Declaration);
		return Declaration;
	}

	static FAngelscriptCachedModuleInterface MakeDeclarationInterface(
		TArray<FAngelscriptCachedDeclaration>&& Declarations)
	{
		FAngelscriptCachedModuleInterface Interface;
		Interface.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		Interface.ModuleKey = MakeModuleKey();
		Interface.CanonicalModuleName = TEXT("Hero");
		Interface.CanonicalNamespaces = {TEXT("Gameplay")};
		Interface.Declarations = MoveTemp(Declarations);
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(Interface, Interface.InterfaceAbi);
		check(Result.IsSuccess());
		return Interface;
	}

	static FAngelscriptCachedImportDeclaration MakeImport(
		const FAngelscriptStableModuleKey& OwningModuleKey)
	{
		FAngelscriptCachedImportDeclaration Import;
		Import.CanonicalNamespace = TEXT("Engine");
		Import.CanonicalName = TEXT("Log");
		Import.CanonicalSignature = TEXT("void Log(const string&in Message)");
		Import.TargetModuleKey = FAngelscriptStableModuleKey{MakeHash(0x81)};
		Import.TargetDeclaration = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptFunction, MakeHash(0x82), MakeHash(0x83)};
		Import.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Import, 0});
		const FAngelscriptImportIdentityInput IdentityInput{
			OwningModuleKey, Import.CanonicalNamespace, Import.CanonicalName,
			Import.CanonicalSignature, Import.TargetModuleKey,
			FAngelscriptStableFunctionKey{Import.TargetDeclaration.StableKey}};
		const FAngelscriptCacheValidationResult KeyResult =
			FAngelscriptCacheSemanticArchive::TryBuildImportKey(IdentityInput, Import.ImportKey);
		check(KeyResult.IsSuccess());
		return Import;
	}

	static FAngelscriptCachedModuleInterface MakeModuleInterface()
	{
		FAngelscriptCachedModuleInterface Interface;
		Interface.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		Interface.ModuleKey = MakeModuleKey();
		Interface.CanonicalModuleName = TEXT("Hero");
		Interface.CanonicalNamespaces = {TEXT("Engine"), TEXT("Gameplay")};
		Interface.Declarations.Add(MakeFunctionDeclaration(Interface.ModuleKey));
		Interface.Imports.Add(MakeImport(Interface.ModuleKey));

		FAngelscriptCacheSemanticDependency DeclarationDependency;
		DeclarationDependency.Kind = EAngelscriptCacheSemanticDependencyKind::Declaration;
		DeclarationDependency.Target = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptType, MakeHash(0x91), MakeHash(0x92)};
		Interface.Dependencies.Add(DeclarationDependency);

		FAngelscriptCacheSemanticDependency HardValueDependency;
		HardValueDependency.Kind = EAngelscriptCacheSemanticDependencyKind::HardValue;
		HardValueDependency.Target = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptGlobal, MakeHash(0x93), MakeHash(0x94)};
		HardValueDependency.ExpectedContentOrValue = MakeHash(0x95);
		Interface.Dependencies.Add(HardValueDependency);

		const FAngelscriptCacheValidationResult AbiResult =
			FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
				Interface, Interface.InterfaceAbi);
		check(AbiResult.IsSuccess());
		return Interface;
	}

	template <typename RecordType, typename DecodeFunctionType>
	static bool ExpectDecodeFailureAndReset(
		FAutomationTestBase& Test,
		const TArray<uint8>& Bytes,
		const EAngelscriptCacheValidationError ExpectedError,
		DecodeFunctionType&& Decode)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		RecordType Output;
		Output.PayloadSchemaVersion = 77;
		const FAngelscriptCacheValidationResult Result = Decode(Bytes, Limits, Budget, Output);
		bool bPassed = LocalAssert.AreEqual(ExpectedError, Result.Error);
		bPassed &= LocalAssert.AreEqual(0u, Output.PayloadSchemaVersion,
			TEXT("Every failed semantic decode must reset its complete output record"));
		return bPassed;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheSourceInterfaceTests,
	"Angelscript.TestModule.Cache.Archive.SourceInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SourceIndexFullPayloadRecordIdAndEnvelopeAreFrozen)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		const FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Payload).IsSuccess()));
		const FString ActualPayload = Hex(Payload);
		const FString ExpectedPayload = TEXT("0100000021fffd2a08a5583e1347f60ee4889e98555f73529f01f16969b3182ad527bc960100000002000000020000000c000000446566696e65732e4d4f444510101010101010101010101010101010101010101010101010101010101010101300000050726570726f636573736f722e537472696374111111111111111111111111111111111111111111111111111111111111111101000000ad6fcd11d1c9a3482f6d26033994ad4ec23877008021e965aeb5bd87b57d7dfc010400000047616d65e017033e47d4fbe1517753805b71e2c791bcb84250bf4796eed294a17ccf09773030303030303030303030303030303030303030303030303030303030303030020000000a000000457874656e73696f6e73313131313131313131313131313131313131313131313131313131313131313109000000526563757273697665323232323232323232323232323232323232323232323232323232323232323201000000e017033e47d4fbe1517753805b71e2c791bcb84250bf4796eed294a17ccf0977011a00000052756e74696d652e4469736b536f7572636550726f766964657220202020202020202020202020202020202020202020202020202020202020200121212121212121212121212121212121212121212121212121212121212121210122222222222222222222222222222222222222222222222222222222222222220123232323232323232323232323232323232323232323232323232323232323230f000000010000006336d65bca9e157b5d15df442a3322f79a8331933da3900c48c907a278811acd011c00000052756e74696d652e5374616e6461726450726570726f636573736f7201ad6fcd11d1c9a3482f6d26033994ad4ec23877008021e965aeb5bd87b57d7dfc40404040404040404040404040404040404040404040404040404040404040400141414141414141414141414141414141414141414141414141414141414141410142424242424242424242424242424242424242424242424242424242424242420143434343434343434343434343434343434343434343434343434343434343430f00000001000000680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e74001ad6fcd11d1c9a3482f6d26033994ad4ec23877008021e965aeb5bd87b57d7dfce017033e47d4fbe1517753805b71e2c791bcb84250bf4796eed294a17ccf09770e0000004163746f72732f4865726f2e617350505050505050505050505050505050505050505050505050505050505050500000d446256670f3b8923e27d69e5b2dc249a5e6e3146d64cc2cbdb3ae258cce0c5f01000000f2791c1a0e2d8a3f7866685b9155a5aa1c8193e06d100c22a9898e88ee8772ca04680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e740020b000000574954485f454449544f520000606060606060606060606060606060606060606060606060606060606060606001000000105ee45ca295721e13ffc58c4e48efe88cc2f316497ea7414f6e19bdcbb8f7f801680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e740680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e7401200000053656c66496e636c75646546697874757265010000000000000000");
		ASSERT_THAT(AreEqual(ExpectedPayload, ActualPayload,
			*FString::Printf(TEXT("SourceIndex payload golden actual=%s"), *ActualPayload)));

		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::SourceIndex, Payload, RecordId).IsSuccess()));
		const FString ExpectedSnapshot = TEXT("21fffd2a08a5583e1347f60ee4889e98555f73529f01f16969b3182ad527bc96");
		const FString ExpectedRecordId = TEXT("7ae43a13ca1b6319014f9fa389f46e4ae2b867a93ed57c7516be65dd01df9073");
		ASSERT_THAT(AreEqual(ExpectedSnapshot, Source.SourceSnapshot.ToHexString(),
			*FString::Printf(TEXT("SourceSnapshot golden actual=%s"), *Source.SourceSnapshot.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedRecordId, RecordId.ContentHash.ToHexString(),
			*FString::Printf(TEXT("SourceIndex RecordId golden actual=%s"), *RecordId.ContentHash.ToHexString())));

		TArray<uint8> Envelope;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
			EAngelscriptCacheRecordKind::SourceIndex, Payload, Envelope).IsSuccess()));
		const FString ActualEnvelope = Hex(Envelope);
		const FString ExpectedEnvelope = TEXT("55454153435632520200000001000000ce040000000000007ae43a13ca1b6319014f9fa389f46e4ae2b867a93ed57c7516be65dd01df90730100000021fffd2a08a5583e1347f60ee4889e98555f73529f01f16969b3182ad527bc960100000002000000020000000c000000446566696e65732e4d4f444510101010101010101010101010101010101010101010101010101010101010101300000050726570726f636573736f722e537472696374111111111111111111111111111111111111111111111111111111111111111101000000ad6fcd11d1c9a3482f6d26033994ad4ec23877008021e965aeb5bd87b57d7dfc010400000047616d65e017033e47d4fbe1517753805b71e2c791bcb84250bf4796eed294a17ccf09773030303030303030303030303030303030303030303030303030303030303030020000000a000000457874656e73696f6e73313131313131313131313131313131313131313131313131313131313131313109000000526563757273697665323232323232323232323232323232323232323232323232323232323232323201000000e017033e47d4fbe1517753805b71e2c791bcb84250bf4796eed294a17ccf0977011a00000052756e74696d652e4469736b536f7572636550726f766964657220202020202020202020202020202020202020202020202020202020202020200121212121212121212121212121212121212121212121212121212121212121210122222222222222222222222222222222222222222222222222222222222222220123232323232323232323232323232323232323232323232323232323232323230f000000010000006336d65bca9e157b5d15df442a3322f79a8331933da3900c48c907a278811acd011c00000052756e74696d652e5374616e6461726450726570726f636573736f7201ad6fcd11d1c9a3482f6d26033994ad4ec23877008021e965aeb5bd87b57d7dfc40404040404040404040404040404040404040404040404040404040404040400141414141414141414141414141414141414141414141414141414141414141410142424242424242424242424242424242424242424242424242424242424242420143434343434343434343434343434343434343434343434343434343434343430f00000001000000680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e74001ad6fcd11d1c9a3482f6d26033994ad4ec23877008021e965aeb5bd87b57d7dfce017033e47d4fbe1517753805b71e2c791bcb84250bf4796eed294a17ccf09770e0000004163746f72732f4865726f2e617350505050505050505050505050505050505050505050505050505050505050500000d446256670f3b8923e27d69e5b2dc249a5e6e3146d64cc2cbdb3ae258cce0c5f01000000f2791c1a0e2d8a3f7866685b9155a5aa1c8193e06d100c22a9898e88ee8772ca04680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e740020b000000574954485f454449544f520000606060606060606060606060606060606060606060606060606060606060606001000000105ee45ca295721e13ffc58c4e48efe88cc2f316497ea7414f6e19bdcbb8f7f801680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e740680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e7401200000053656c66496e636c75646546697874757265010000000000000000");
		ASSERT_THAT(AreEqual(ExpectedEnvelope, ActualEnvelope,
			*FString::Printf(TEXT("SourceIndex envelope golden actual=%s"), *ActualEnvelope)));
	}

	TEST_METHOD(ModuleInterfaceFullPayloadHashesRecordIdAndEnvelopeAreFrozen)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		const FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Payload).IsSuccess()));
		const FString ActualPayload = Hex(Payload);
		const FString ExpectedPayload = TEXT("01000000d446256670f3b8923e27d69e5b2dc249a5e6e3146d64cc2cbdb3ae258cce0c5f040000004865726f6a5be3b2aba2420237ae1c6da3c740ea922ee8ca10208a70c72d7c44e154238d0200000006000000456e67696e650800000047616d65706c617901000000022001026f1bd5aba0fcd1112870dc3f91893e61ecd6805ad0fdefff24ad84dc81feec3b01d446256670f3b8923e27d69e5b2dc249a5e6e3146d64cc2cbdb3ae258cce0c5fd446256670f3b8923e27d69e5b2dc249a5e6e3146d64cc2cbdb3ae258cce0c5f0800000047616d65706c6179040000005469636b1d000000766f6964205469636b28666c6f61742044656c74615365636f6e647329020000000800000043616c6c61626c65060000005075626c69630001010100000000000000000001000000000000000c00000044656c74615365636f6e6473010b0000000000000000000100010000000000000001000000020000000800000043617465676f72790800000047616d65706c617907000000546f6f6c5469700e0000005469636b7320746865206865726f0100000002000000009b3ea8cc37f59e7394c0ce01112fe6d67f48776c357a259772151ebd10f231392bfe5f03345f8f6207a3d0ee05f4c89bd5f64294603efcd694ea84b0a0b3346801000000c589bbd149afb8b12650f68d65f428fa631f243c1590ed67d1460b63fcdc213c06000000456e67696e65030000004c6f6721000000766f6964204c6f6728636f6e737420737472696e6726696e204d65737361676529818181818181818181818181818181818181818181818181818181818181818103828282828282828282828282828282828282828282828282828282828282828283838383838383838383838383838383838383838383838383838383838383830100000004000000000200000002029191919191919191919191919191919191919191919191919191919191919191929292929292929292929292929292929292929292929292929292929292929200080493939393939393939393939393939393939393939393939393939393939393939494949494949494949494949494949494949494949494949494949494949494019595959595959595959595959595959595959595959595959595959595959595");
		ASSERT_THAT(AreEqual(ExpectedPayload, ActualPayload,
			*FString::Printf(TEXT("ModuleInterface payload golden actual=%s"), *ActualPayload)));

		const FAngelscriptCachedDeclaration& Declaration = Interface.Declarations[0];
		ASSERT_THAT(AreEqual(FString(TEXT("6f1bd5aba0fcd1112870dc3f91893e61ecd6805ad0fdefff24ad84dc81feec3b")),
			Declaration.StableKey.ToHexString(),
			*FString::Printf(TEXT("Declaration StableKey golden actual=%s"), *Declaration.StableKey.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("9b3ea8cc37f59e7394c0ce01112fe6d67f48776c357a259772151ebd10f23139")),
			Declaration.SignatureHash.ToHexString(),
			*FString::Printf(TEXT("SignatureHash golden actual=%s"), *Declaration.SignatureHash.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("2bfe5f03345f8f6207a3d0ee05f4c89bd5f64294603efcd694ea84b0a0b33468")),
			Declaration.TraitsHash.ToHexString(),
			*FString::Printf(TEXT("TraitsHash golden actual=%s"), *Declaration.TraitsHash.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("6a5be3b2aba2420237ae1c6da3c740ea922ee8ca10208a70c72d7c44e154238d")),
			Interface.InterfaceAbi.ToHexString(),
			*FString::Printf(TEXT("InterfaceAbi golden actual=%s"), *Interface.InterfaceAbi.ToHexString())));

		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::ModuleInterface, Payload, RecordId).IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT("9264cbaf948be5fa35e55652e4e583d221e0d913d53bd64cd74485a420491e55")),
			RecordId.ContentHash.ToHexString(),
			*FString::Printf(TEXT("ModuleInterface RecordId golden actual=%s"), *RecordId.ContentHash.ToHexString())));

		TArray<uint8> Envelope;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
			EAngelscriptCacheRecordKind::ModuleInterface, Payload, Envelope).IsSuccess()));
		const FString ActualEnvelope = Hex(Envelope);
		ASSERT_THAT(AreEqual(FString(TEXT("554541534356325202000000020000004c030000000000009264cbaf948be5fa35e55652e4e583d221e0d913d53bd64cd74485a420491e5501000000d446256670f3b8923e27d69e5b2dc249a5e6e3146d64cc2cbdb3ae258cce0c5f040000004865726f6a5be3b2aba2420237ae1c6da3c740ea922ee8ca10208a70c72d7c44e154238d0200000006000000456e67696e650800000047616d65706c617901000000022001026f1bd5aba0fcd1112870dc3f91893e61ecd6805ad0fdefff24ad84dc81feec3b01d446256670f3b8923e27d69e5b2dc249a5e6e3146d64cc2cbdb3ae258cce0c5fd446256670f3b8923e27d69e5b2dc249a5e6e3146d64cc2cbdb3ae258cce0c5f0800000047616d65706c6179040000005469636b1d000000766f6964205469636b28666c6f61742044656c74615365636f6e647329020000000800000043616c6c61626c65060000005075626c69630001010100000000000000000001000000000000000c00000044656c74615365636f6e6473010b0000000000000000000100010000000000000001000000020000000800000043617465676f72790800000047616d65706c617907000000546f6f6c5469700e0000005469636b7320746865206865726f0100000002000000009b3ea8cc37f59e7394c0ce01112fe6d67f48776c357a259772151ebd10f231392bfe5f03345f8f6207a3d0ee05f4c89bd5f64294603efcd694ea84b0a0b3346801000000c589bbd149afb8b12650f68d65f428fa631f243c1590ed67d1460b63fcdc213c06000000456e67696e65030000004c6f6721000000766f6964204c6f6728636f6e737420737472696e6726696e204d65737361676529818181818181818181818181818181818181818181818181818181818181818103828282828282828282828282828282828282828282828282828282828282828283838383838383838383838383838383838383838383838383838383838383830100000004000000000200000002029191919191919191919191919191919191919191919191919191919191919191929292929292929292929292929292929292929292929292929292929292929200080493939393939393939393939393939393939393939393939393939393939393939494949494949494949494949494949494949494949494949494949494949494019595959595959595959595959595959595959595959595959595959595959595")), ActualEnvelope,
			*FString::Printf(TEXT("ModuleInterface envelope golden actual=%s"), *ActualEnvelope)));
	}

	TEST_METHOD(PublicIdentityOnlyKeyBuildersHaveFrozenFullHashesAndZeroOutputFailures)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		const FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		const FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		ASSERT_THAT(AreEqual(FString(TEXT("e017033e47d4fbe1517753805b71e2c791bcb84250bf4796eed294a17ccf0977")),
			Source.Providers[0].ProviderKey.Hash.ToHexString(),
			*FString::Printf(TEXT("ProviderKey golden actual=%s"),
				*Source.Providers[0].ProviderKey.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("ad6fcd11d1c9a3482f6d26033994ad4ec23877008021e965aeb5bd87b57d7dfc")),
			Source.Mounts[0].MountKey.Hash.ToHexString(),
			*FString::Printf(TEXT("MountKey golden actual=%s"),
				*Source.Mounts[0].MountKey.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("6336d65bca9e157b5d15df442a3322f79a8331933da3900c48c907a278811acd")),
			Source.PreprocessHooks[0].HookKey.Hash.ToHexString(),
			*FString::Printf(TEXT("HookKey golden actual=%s"),
				*Source.PreprocessHooks[0].HookKey.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("680bdd055d1b82a6c186e13062ac4eb3d3e474608176aa545bfdc99ef851e740")),
			Source.Files[0].SourceFileKey.Hash.ToHexString(),
			*FString::Printf(TEXT("SourceFileKey golden actual=%s"),
				*Source.Files[0].SourceFileKey.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("f2791c1a0e2d8a3f7866685b9155a5aa1c8193e06d100c22a9898e88ee8772ca")),
			Source.PreprocessorInputs[0].InputKey.Hash.ToHexString(),
			*FString::Printf(TEXT("InputKey golden actual=%s"),
				*Source.PreprocessorInputs[0].InputKey.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("105ee45ca295721e13ffc58c4e48efe88cc2f316497ea7414f6e19bdcbb8f7f8")),
			Source.Edges[0].EdgeKey.Hash.ToHexString(),
			*FString::Printf(TEXT("EdgeKey golden actual=%s"),
				*Source.Edges[0].EdgeKey.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(FString(TEXT("c589bbd149afb8b12650f68d65f428fa631f243c1590ed67d1460b63fcdc213c")),
			Interface.Imports[0].ImportKey.Hash.ToHexString(),
			*FString::Printf(TEXT("ImportKey golden actual=%s"),
				*Interface.Imports[0].ImportKey.Hash.ToHexString())));

		FAngelscriptSourceProviderIdentityInput ZeroIdentity{
			EAngelscriptCachedSourceProviderKind::External, TEXT("External.Unstable"), {}};
		FAngelscriptCachedSourceProviderKey ProviderKey;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
			ZeroIdentity, ProviderKey).IsSuccess()));
		ASSERT_THAT(IsFalse(ProviderKey.Hash.IsZero()));

		FAngelscriptSourceProviderIdentityInput InvalidProvider = ZeroIdentity;
		InvalidProvider.ProviderKind = EAngelscriptCachedSourceProviderKind::Invalid;
		ProviderKey.Hash = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownEnumValue,
			FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
				InvalidProvider, ProviderKey).Error));
		ASSERT_THAT(IsTrue(ProviderKey.Hash.IsZero()));
		InvalidProvider = ZeroIdentity;
		InvalidProvider.CanonicalImplementationIdentity = TEXT("BadProvider");
		InvalidProvider.CanonicalImplementationIdentity.GetCharArray().Insert(TEXT('\0'), 3);
		ProviderKey.Hash = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::EmbeddedNul,
			FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
				InvalidProvider, ProviderKey).Error));
		ASSERT_THAT(IsTrue(ProviderKey.Hash.IsZero()));

		FAngelscriptSourceMountIdentityInput InvalidMount{
			EAngelscriptCachedSourceKind::Game, TEXT("Game"), {}};
		FAngelscriptCachedSourceMountKey MountKey{MakeHash(0xee)};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(InvalidMount, MountKey).Error));
		ASSERT_THAT(IsTrue(MountKey.Hash.IsZero()));

		FAngelscriptPreprocessHookIdentityInput InvalidHook{
			EAngelscriptCachedPreprocessHookPhase::Invalid, TEXT("Hook"),
			EAngelscriptCachedFastPathScopeKind::Module, Source.Files[0].ModuleKey.Hash};
		FAngelscriptCachedPreprocessHookKey HookKey{MakeHash(0xee)};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownEnumValue,
			FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(InvalidHook, HookKey).Error));
		ASSERT_THAT(IsTrue(HookKey.Hash.IsZero()));

		FAngelscriptSourceFileIdentityInput InvalidFile{
			EAngelscriptCachedSourceKind::Game, Source.Mounts[0].MountKey,
			Source.Providers[0].ProviderKey, TEXT("../Escape.as"), {}};
		FAngelscriptCachedSourceFileKey FileKey{MakeHash(0xee)};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidLogicalPath,
			FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(InvalidFile, FileKey).Error));
		ASSERT_THAT(IsTrue(FileKey.Hash.IsZero()));
		InvalidFile.RelativeLogicalPath = TEXT("Valid.as");
		InvalidFile.GeneratedSourceKey = FAngelscriptHash256{};
		FileKey.Hash = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(InvalidFile, FileKey).Error));
		ASSERT_THAT(IsTrue(FileKey.Hash.IsZero()));

		FAngelscriptPreprocessorInputIdentityInput InvalidInput{
			{}, EAngelscriptCachePreprocessorInputKind::Define, TEXT("DEFINE"), {}};
		FAngelscriptCachedPreprocessorInputKey InputKey{MakeHash(0xee)};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(InvalidInput, InputKey).Error));
		ASSERT_THAT(IsTrue(InputKey.Hash.IsZero()));
		InvalidInput.OwnerScopeStableKey = Source.Files[0].SourceFileKey.Hash;
		InvalidInput.TargetStableKey = FAngelscriptHash256{};
		InputKey.Hash = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(InvalidInput, InputKey).Error));
		ASSERT_THAT(IsTrue(InputKey.Hash.IsZero()));

		FAngelscriptSourceEdgeIdentityInput InvalidEdge{
			EAngelscriptCachedSourceEdgeKind::Include, {}, Source.Files[0].SourceFileKey.Hash,
			TEXT("Include")};
		FAngelscriptCachedSourceEdgeKey EdgeKey{MakeHash(0xee)};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::TryBuildSourceEdgeKey(InvalidEdge, EdgeKey).Error));
		ASSERT_THAT(IsTrue(EdgeKey.Hash.IsZero()));

		const FAngelscriptCachedImportDeclaration& Import = Interface.Imports[0];
		const FAngelscriptImportIdentityInput ImportIdentity{
			Interface.ModuleKey, Import.CanonicalNamespace, Import.CanonicalName,
			Import.CanonicalSignature, Import.TargetModuleKey,
			FAngelscriptStableFunctionKey{Import.TargetDeclaration.StableKey}};
		FAngelscriptStableImportKey FirstImportKey;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::TryBuildImportKey(
			ImportIdentity, FirstImportKey).IsSuccess()));
		FAngelscriptCachedImportDeclaration NonIdentityVariant = Import;
		NonIdentityVariant.TargetDeclaration.ExpectedAbi = MakeHash(0xfa);
		NonIdentityVariant.Slots[0].Ordinal = 99;
		const FAngelscriptImportIdentityInput VariantIdentity{
			Interface.ModuleKey, NonIdentityVariant.CanonicalNamespace, NonIdentityVariant.CanonicalName,
			NonIdentityVariant.CanonicalSignature, NonIdentityVariant.TargetModuleKey,
			FAngelscriptStableFunctionKey{NonIdentityVariant.TargetDeclaration.StableKey}};
		FAngelscriptStableImportKey VariantImportKey;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::TryBuildImportKey(
			VariantIdentity, VariantImportKey).IsSuccess()));
		ASSERT_THAT(AreEqual(FirstImportKey.Hash.ToHexString(), VariantImportKey.Hash.ToHexString()));

		FAngelscriptImportIdentityInput InvalidImport = ImportIdentity;
		InvalidImport.ModuleKey = {};
		VariantImportKey.Hash = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::TryBuildImportKey(InvalidImport, VariantImportKey).Error));
		ASSERT_THAT(IsTrue(VariantImportKey.Hash.IsZero()));
		InvalidImport = ImportIdentity;
		InvalidImport.CanonicalName.Reset();
		VariantImportKey.Hash = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			FAngelscriptCacheSemanticArchive::TryBuildImportKey(InvalidImport, VariantImportKey).Error));
		ASSERT_THAT(IsTrue(VariantImportKey.Hash.IsZero()));
	}

	TEST_METHOD(SourceEdgeSemanticOrdinalGroupsEnforceAllOrNoneAndContiguousSequences)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		TArray<uint8> Bytes;
		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		Source.Edges[0].SemanticOrdinal.Reset();
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes).IsSuccess()));

		Source = MakeSourceIndex();
		FAngelscriptCachedSourceEdge Second = Source.Edges[0];
		Second.CanonicalIncludeOrGeneratorIdentity = TEXT("SecondInclude");
		Second.SemanticOrdinal = 1;
		Second.EdgeKey = BuildEdgeKey(Second);
		Source.Edges.Add(Second);
		FAngelscriptCachedSourceEdge Third = Source.Edges[0];
		Third.CanonicalIncludeOrGeneratorIdentity = TEXT("ThirdInclude");
		Third.SemanticOrdinal = 2;
		Third.EdgeKey = BuildEdgeKey(Third);
		Source.Edges.Add(Third);
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes).IsSuccess()));

		FAngelscriptCachedSourceIndex IndependentGroups = MakeGeneratedSourceIndex();
		FAngelscriptCachedSourceEdge GeneratedEdge = IndependentGroups.Edges[0];
		GeneratedEdge.EdgeKind = EAngelscriptCachedSourceEdgeKind::GeneratedSource;
		GeneratedEdge.ToSourceOrGeneratedKey = IndependentGroups.Files[0].GeneratedSourceKey.GetValue();
		GeneratedEdge.CanonicalIncludeOrGeneratorIdentity = TEXT("GeneratedGroup");
		GeneratedEdge.SemanticOrdinal = 0;
		GeneratedEdge.EdgeKey = BuildEdgeKey(GeneratedEdge);
		IndependentGroups.Edges.Add(GeneratedEdge);
		FAngelscriptCachedSourceFile SecondFile = IndependentGroups.Files[0];
		SecondFile.RelativeLogicalPath = TEXT("Actors/SecondGenerated.as");
		SecondFile.RawContentHash = MakeHash(0x7a);
		SecondFile.GeneratedSourceKey = MakeHash(0x7b);
		SecondFile.GeneratedConfigurationFingerprint = MakeHash(0x7c);
		SecondFile.ModuleKey = MakeSecondaryModuleKey();
		SecondFile.SourceFileKey = BuildSourceFileKey(SecondFile);
		IndependentGroups.Files.Add(SecondFile);
		FAngelscriptCachedSourceEdge SecondFromEdge = IndependentGroups.Edges[0];
		SecondFromEdge.FromSourceFileKey = SecondFile.SourceFileKey;
		SecondFromEdge.CanonicalIncludeOrGeneratorIdentity = TEXT("SecondFromGroup");
		SecondFromEdge.SemanticOrdinal = 0;
		SecondFromEdge.EdgeKey = BuildEdgeKey(SecondFromEdge);
		IndependentGroups.Edges.Add(SecondFromEdge);
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(IndependentGroups).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			IndependentGroups, Bytes).IsSuccess()));

		Source = MakeSourceIndex();
		Second = Source.Edges[0];
		Second.CanonicalIncludeOrGeneratorIdentity = TEXT("MixedInclude");
		Second.SemanticOrdinal.Reset();
		Second.EdgeKey = BuildEdgeKey(Second);
		Source.Edges.Add(Second);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes).Error));

		Source = MakeSourceIndex();
		Second = Source.Edges[0];
		Second.CanonicalIncludeOrGeneratorIdentity = TEXT("GapInclude");
		Second.SemanticOrdinal = 2;
		Second.EdgeKey = BuildEdgeKey(Second);
		Source.Edges.Add(Second);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::OrdinalGap,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes).Error));

		Source = MakeSourceIndex();
		Second = Source.Edges[0];
		Second.CanonicalIncludeOrGeneratorIdentity = TEXT("DuplicateOrdinalInclude");
		Second.SemanticOrdinal = 0;
		Second.EdgeKey = BuildEdgeKey(Second);
		Source.Edges.Add(Second);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DuplicateOrdinal,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes).Error));
	}

	TEST_METHOD(ProviderAndHookCapabilitiesRequireExactlyTheirMappedIneligibleRows)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		struct FPair
		{
			EAngelscriptCachedFingerprintCapabilityFlags Flag;
			EAngelscriptCachedFastPathIneligibleReason ProviderReason;
			EAngelscriptCachedFastPathIneligibleReason HookReason;
		};
		const FPair Pairs[] = {
			{EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity,
				EAngelscriptCachedFastPathIneligibleReason::MissingStableIdentity,
				EAngelscriptCachedFastPathIneligibleReason::MissingStableIdentity},
			{EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint},
			{EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingConfigurationFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingConfigurationFingerprint},
			{EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource,
				EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior},
		};
		TArray<uint8> Bytes;
		for (const FPair& Pair : Pairs)
		{
			FAngelscriptCachedSourceIndex MissingProviderRow = MakeSourceIndex();
			FAngelscriptCachedSourceProvider& Provider = MissingProviderRow.Providers[0];
			Provider.CapabilityFlags &= ~static_cast<uint32>(Pair.Flag);
			switch (Pair.Flag)
			{
			case EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity: Provider.IdentityFingerprint = {}; break;
			case EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint: Provider.VersionFingerprint.Reset(); break;
			case EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint: Provider.ConfigurationFingerprint.Reset(); break;
			case EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint: Provider.ContentFingerprint.Reset(); break;
			default: checkNoEntry();
			}
			if (Pair.Flag == EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity)
			{
				RebuildSingleSourceGraphKeys(MissingProviderRow);
			}
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(MissingProviderRow, Bytes).Error));

			FAngelscriptCachedSourceIndex ValidProviderRow = MissingProviderRow;
			ValidProviderRow.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Provider,
				ValidProviderRow.Providers[0].ProviderKey.Hash, Pair.ProviderReason,
				TEXT("Provider capability unavailable")));
			ASSERT_THAT(IsTrue(RefreshSourceSnapshot(ValidProviderRow).IsSuccess()));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				ValidProviderRow, Bytes).IsSuccess()));

			FAngelscriptCachedSourceIndex WrongProviderScope = MissingProviderRow;
			WrongProviderScope.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Mount,
				WrongProviderScope.Mounts[0].MountKey.Hash, Pair.ProviderReason,
				TEXT("Provider capability wrong scope")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
					WrongProviderScope, Bytes).Error));

			FAngelscriptCachedSourceIndex WrongProviderKey = MissingProviderRow;
			const FAngelscriptHash256 MissingProviderKey = MakeHash(0xe8);
			ASSERT_THAT(IsTrue(MissingProviderKey
				!= WrongProviderKey.Providers[0].ProviderKey.Hash));
			WrongProviderKey.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Provider,
				MissingProviderKey, Pair.ProviderReason,
				TEXT("Provider capability wrong key")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
					WrongProviderKey, Bytes).Error));

			FAngelscriptCachedSourceIndex WrongProviderReason = MissingProviderRow;
			WrongProviderReason.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Provider,
				WrongProviderReason.Providers[0].ProviderKey.Hash,
				EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
				TEXT("Provider capability unrelated reason cannot replace exact row")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
					WrongProviderReason, Bytes).Error));

			FAngelscriptCachedSourceIndex UnrelatedProviderReason = MakeSourceIndex();
			UnrelatedProviderReason.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Provider,
				UnrelatedProviderReason.Providers[0].ProviderKey.Hash,
				EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
				TEXT("Independent provider diagnostic")));
			ASSERT_THAT(IsTrue(RefreshSourceSnapshot(UnrelatedProviderReason).IsSuccess()));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				UnrelatedProviderReason, Bytes).IsSuccess()));

			FAngelscriptCachedSourceIndex ForbiddenProviderRow = MakeSourceIndex();
			ForbiddenProviderRow.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Provider,
				ForbiddenProviderRow.Providers[0].ProviderKey.Hash, Pair.ProviderReason,
				TEXT("Stale provider reason")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ForbiddenProviderRow, Bytes).Error));

			FAngelscriptCachedSourceIndex MissingHookRow = MakeSourceIndex();
			FAngelscriptCachedPreprocessHook& Hook = MissingHookRow.PreprocessHooks[0];
			Hook.CapabilityFlags &= ~static_cast<uint32>(Pair.Flag);
			switch (Pair.Flag)
			{
			case EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity: Hook.IdentityFingerprint = {}; break;
			case EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint: Hook.VersionFingerprint.Reset(); break;
			case EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint: Hook.ConfigurationFingerprint.Reset(); break;
			case EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint: Hook.ContentFingerprint.Reset(); break;
			default: checkNoEntry();
			}
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(MissingHookRow, Bytes).Error));

			FAngelscriptCachedSourceIndex ValidHookRow = MissingHookRow;
			ValidHookRow.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Hook,
				ValidHookRow.PreprocessHooks[0].HookKey.Hash, Pair.HookReason,
				TEXT("Hook capability unavailable")));
			ASSERT_THAT(IsTrue(RefreshSourceSnapshot(ValidHookRow).IsSuccess()));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				ValidHookRow, Bytes).IsSuccess()));

			FAngelscriptCachedSourceIndex WrongHookScope = MissingHookRow;
			WrongHookScope.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Mount,
				WrongHookScope.Mounts[0].MountKey.Hash, Pair.HookReason,
				TEXT("Hook capability wrong scope")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
					WrongHookScope, Bytes).Error));

			FAngelscriptCachedSourceIndex WrongHookKey = MissingHookRow;
			const FAngelscriptHash256 MissingHookKey = MakeHash(0xe9);
			ASSERT_THAT(IsTrue(MissingHookKey
				!= WrongHookKey.PreprocessHooks[0].HookKey.Hash));
			WrongHookKey.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Hook,
				MissingHookKey, Pair.HookReason,
				TEXT("Hook capability wrong key")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
					WrongHookKey, Bytes).Error));

			FAngelscriptCachedSourceIndex WrongHookReason = MissingHookRow;
			WrongHookReason.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Hook,
				WrongHookReason.PreprocessHooks[0].HookKey.Hash,
				EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource,
				TEXT("Hook capability unrelated reason cannot replace exact row")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
					WrongHookReason, Bytes).Error));

			FAngelscriptCachedSourceIndex UnrelatedHookReason = MakeSourceIndex();
			UnrelatedHookReason.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Hook,
				UnrelatedHookReason.PreprocessHooks[0].HookKey.Hash,
				EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource,
				TEXT("Independent hook diagnostic")));
			ASSERT_THAT(IsTrue(RefreshSourceSnapshot(UnrelatedHookReason).IsSuccess()));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				UnrelatedHookReason, Bytes).IsSuccess()));

			FAngelscriptCachedSourceIndex ForbiddenHookRow = MakeSourceIndex();
			ForbiddenHookRow.IneligibleScopes.Add(MakeIneligibleScope(
				EAngelscriptCachedFastPathScopeKind::Hook,
				ForbiddenHookRow.PreprocessHooks[0].HookKey.Hash, Pair.HookReason,
				TEXT("Stale hook reason")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ForbiddenHookRow, Bytes).Error));
		}

		FAngelscriptCachedSourceIndex UnrelatedReasons = MakeSourceIndex();
		UnrelatedReasons.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Provider,
			UnrelatedReasons.Providers[0].ProviderKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("Independent provider diagnostic")));
		UnrelatedReasons.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Hook,
			UnrelatedReasons.PreprocessHooks[0].HookKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource,
			TEXT("Independent hook diagnostic")));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(UnrelatedReasons).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			UnrelatedReasons, Bytes).IsSuccess()));
	}

	TEST_METHOD(SourceTypedReferencesResolveByKindAndRejectMissingWrongOrAmbiguousAuthorities)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		struct FScopeTarget { EAngelscriptCachedFastPathScopeKind Kind; FAngelscriptHash256 Key; };
		TArray<uint8> Bytes;
		FAngelscriptCachedSourceIndex Base = MakeSourceIndex();
		const FScopeTarget ScopeTargets[] = {
			{EAngelscriptCachedFastPathScopeKind::Mount, Base.Mounts[0].MountKey.Hash},
			{EAngelscriptCachedFastPathScopeKind::Provider, Base.Providers[0].ProviderKey.Hash},
			{EAngelscriptCachedFastPathScopeKind::Hook, Base.PreprocessHooks[0].HookKey.Hash},
			{EAngelscriptCachedFastPathScopeKind::SourceFile, Base.Files[0].SourceFileKey.Hash},
			{EAngelscriptCachedFastPathScopeKind::Module, Base.Files[0].ModuleKey.Hash},
		};
		for (const FScopeTarget& Target : ScopeTargets)
		{
			FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
			Source.PreprocessorInputs[0].OwnerScopeKind = Target.Kind;
			Source.PreprocessorInputs[0].OwnerScopeStableKey = Target.Key;
			Source.PreprocessorInputs[0].InputKey = BuildInputKey(Source.PreprocessorInputs[0]);
			ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				Source, Bytes).IsSuccess()));
		}

		struct FInputTarget { EAngelscriptCachePreprocessorInputTargetKind Kind; FAngelscriptHash256 Key; };
		const FInputTarget InputTargets[] = {
			{EAngelscriptCachePreprocessorInputTargetKind::SourceFile, Base.Files[0].SourceFileKey.Hash},
			{EAngelscriptCachePreprocessorInputTargetKind::Provider, Base.Providers[0].ProviderKey.Hash},
			{EAngelscriptCachePreprocessorInputTargetKind::Hook, Base.PreprocessHooks[0].HookKey.Hash},
			{EAngelscriptCachePreprocessorInputTargetKind::Module, Base.Files[0].ModuleKey.Hash},
		};
		for (const FInputTarget& Target : InputTargets)
		{
			FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
			Source.PreprocessorInputs[0].TargetKind = Target.Kind;
			Source.PreprocessorInputs[0].TargetStableKey = Target.Key;
			Source.PreprocessorInputs[0].InputKey = BuildInputKey(Source.PreprocessorInputs[0]);
			ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				Source, Bytes).IsSuccess()));
		}

		FAngelscriptCachedSourceIndex WrongKind = MakeSourceIndex();
		WrongKind.PreprocessorInputs[0].TargetKind =
			EAngelscriptCachePreprocessorInputTargetKind::Provider;
		WrongKind.PreprocessorInputs[0].TargetStableKey = WrongKind.Files[0].SourceFileKey.Hash;
		WrongKind.PreprocessorInputs[0].InputKey = BuildInputKey(WrongKind.PreprocessorInputs[0]);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(WrongKind, Bytes).Error));

		FAngelscriptCachedSourceIndex Missing = MakeSourceIndex();
		Missing.PreprocessorInputs[0].TargetKind =
			EAngelscriptCachePreprocessorInputTargetKind::Provider;
		Missing.PreprocessorInputs[0].TargetStableKey = MakeHash(0xfe);
		Missing.PreprocessorInputs[0].InputKey = BuildInputKey(Missing.PreprocessorInputs[0]);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Missing, Bytes).Error));

		FAngelscriptCachedSourceIndex ResolvedConflict = MakeSourceIndex();
		ResolvedConflict.Files[0].SourceKind = EAngelscriptCachedSourceKind::Plugin;
		RebuildSingleSourceGraphKeys(ResolvedConflict);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ResolvedConflict, Bytes).Error));

		FAngelscriptCachedSourceIndex Generated = MakeGeneratedSourceIndex();
		Generated.PreprocessorInputs[0].TargetKind =
			EAngelscriptCachePreprocessorInputTargetKind::GeneratedSource;
		Generated.PreprocessorInputs[0].TargetStableKey = Generated.Files[0].GeneratedSourceKey;
		Generated.PreprocessorInputs[0].InputKey = BuildInputKey(Generated.PreprocessorInputs[0]);
		Generated.Edges[0].EdgeKind = EAngelscriptCachedSourceEdgeKind::GeneratedSource;
		Generated.Edges[0].ToSourceOrGeneratedKey = Generated.Files[0].GeneratedSourceKey.GetValue();
		Generated.Edges[0].EdgeKey = BuildEdgeKey(Generated.Edges[0]);
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Generated).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Generated, Bytes).IsSuccess()));

		FAngelscriptCachedSourceFile DuplicateGeneratedFile = Generated.Files[0];
		DuplicateGeneratedFile.RelativeLogicalPath = TEXT("Actors/GeneratedCopy.as");
		DuplicateGeneratedFile.RawContentHash = MakeHash(0x79);
		DuplicateGeneratedFile.SourceFileKey = BuildSourceFileKey(DuplicateGeneratedFile);
		Generated.Files.Add(DuplicateGeneratedFile);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Generated, Bytes).Error));

		FAngelscriptCachedSourceIndex ForgedHookCycle = MakeSourceIndex();
		FAngelscriptCachedPreprocessHook HookA = MakeHook(TEXT("Cycle.A"),
			EAngelscriptCachedFastPathScopeKind::Module, ForgedHookCycle.Files[0].ModuleKey.Hash, 0xa0);
		FAngelscriptCachedPreprocessHook HookB = MakeHook(TEXT("Cycle.B"),
			EAngelscriptCachedFastPathScopeKind::Hook, HookA.HookKey.Hash, 0xb0);
		HookA.AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Hook;
		HookA.AffectedScopeStableKey = HookB.HookKey.Hash;
		ForgedHookCycle.PreprocessHooks.Add(HookA);
		ForgedHookCycle.PreprocessHooks.Add(HookB);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ForgedHookCycle, Bytes).Error));

		FAngelscriptCachedSourceIndex ForgedHookSelfCycle = MakeSourceIndex();
		FAngelscriptCachedPreprocessHook SelfHook = MakeHook(TEXT("Cycle.Self"),
			EAngelscriptCachedFastPathScopeKind::Module,
			ForgedHookSelfCycle.Files[0].ModuleKey.Hash, 0xd0);
		SelfHook.AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Hook;
		SelfHook.AffectedScopeStableKey = SelfHook.HookKey.Hash;
		ForgedHookSelfCycle.PreprocessHooks.Add(SelfHook);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ForgedHookSelfCycle, Bytes).Error));
	}

	TEST_METHOD(SourceTypedReferenceMatrixFreezesEveryAuthorityRoute)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		enum class EField : uint8
		{
			MountProvider,
			FileMount,
			FileProvider,
			HookScope,
			InputOwnerScope,
			IneligibleScope,
			InputTarget,
			EdgeFrom,
			EdgeIncludeTo,
			EdgeGeneratedTo,
		};
		enum class EAuthority : uint8
		{
			Mount,
			Provider,
			Hook,
			SourceFile,
			Module,
			GeneratedSource,
		};
		struct FCase
		{
			EField Field;
			EAuthority RequiredAuthority;
		};
		const FCase Cases[] = {
			{EField::MountProvider, EAuthority::Provider},
			{EField::FileMount, EAuthority::Mount},
			{EField::FileProvider, EAuthority::Provider},
			{EField::HookScope, EAuthority::Mount},
			{EField::HookScope, EAuthority::Provider},
			{EField::HookScope, EAuthority::Hook},
			{EField::HookScope, EAuthority::SourceFile},
			{EField::HookScope, EAuthority::Module},
			{EField::InputOwnerScope, EAuthority::Mount},
			{EField::InputOwnerScope, EAuthority::Provider},
			{EField::InputOwnerScope, EAuthority::Hook},
			{EField::InputOwnerScope, EAuthority::SourceFile},
			{EField::InputOwnerScope, EAuthority::Module},
			{EField::IneligibleScope, EAuthority::Mount},
			{EField::IneligibleScope, EAuthority::Provider},
			{EField::IneligibleScope, EAuthority::Hook},
			{EField::IneligibleScope, EAuthority::SourceFile},
			{EField::IneligibleScope, EAuthority::Module},
			{EField::InputTarget, EAuthority::SourceFile},
			{EField::InputTarget, EAuthority::Provider},
			{EField::InputTarget, EAuthority::Hook},
			{EField::InputTarget, EAuthority::Module},
			{EField::InputTarget, EAuthority::GeneratedSource},
			{EField::EdgeFrom, EAuthority::SourceFile},
			{EField::EdgeIncludeTo, EAuthority::SourceFile},
			{EField::EdgeGeneratedTo, EAuthority::GeneratedSource},
		};

		auto ScopeKindFor = [](const EAuthority Authority)
		{
			switch (Authority)
			{
			case EAuthority::Mount: return EAngelscriptCachedFastPathScopeKind::Mount;
			case EAuthority::Provider: return EAngelscriptCachedFastPathScopeKind::Provider;
			case EAuthority::Hook: return EAngelscriptCachedFastPathScopeKind::Hook;
			case EAuthority::SourceFile: return EAngelscriptCachedFastPathScopeKind::SourceFile;
			case EAuthority::Module: return EAngelscriptCachedFastPathScopeKind::Module;
			default: checkNoEntry(); return EAngelscriptCachedFastPathScopeKind::Invalid;
			}
		};
		auto TargetKindFor = [](const EAuthority Authority)
		{
			switch (Authority)
			{
			case EAuthority::SourceFile: return EAngelscriptCachePreprocessorInputTargetKind::SourceFile;
			case EAuthority::Provider: return EAngelscriptCachePreprocessorInputTargetKind::Provider;
			case EAuthority::Hook: return EAngelscriptCachePreprocessorInputTargetKind::Hook;
			case EAuthority::Module: return EAngelscriptCachePreprocessorInputTargetKind::Module;
			case EAuthority::GeneratedSource:
				return EAngelscriptCachePreprocessorInputTargetKind::GeneratedSource;
			default: checkNoEntry(); return EAngelscriptCachePreprocessorInputTargetKind::None;
			}
		};
		auto AuthorityKey = [](const FAngelscriptCachedSourceIndex& Source,
			const EAuthority Authority)
		{
			switch (Authority)
			{
			case EAuthority::Mount: return Source.Mounts[0].MountKey.Hash;
			case EAuthority::Provider: return Source.Providers[0].ProviderKey.Hash;
			case EAuthority::Hook: return Source.PreprocessHooks[0].HookKey.Hash;
			case EAuthority::SourceFile: return Source.Files[0].SourceFileKey.Hash;
			case EAuthority::Module: return Source.Files[0].ModuleKey.Hash;
			case EAuthority::GeneratedSource:
				check(Source.Files[0].GeneratedSourceKey.IsSet());
				return Source.Files[0].GeneratedSourceKey.GetValue();
			default: checkNoEntry(); return FAngelscriptHash256{};
			}
		};
		auto WrongAuthorityFor = [](const EAuthority Authority)
		{
			switch (Authority)
			{
			case EAuthority::Mount: return EAuthority::Provider;
			case EAuthority::Provider: return EAuthority::Hook;
			case EAuthority::Hook: return EAuthority::Mount;
			case EAuthority::SourceFile: return EAuthority::Provider;
			case EAuthority::Module: return EAuthority::Hook;
			case EAuthority::GeneratedSource: return EAuthority::SourceFile;
			default: checkNoEntry(); return EAuthority::Mount;
			}
		};
		auto MakeFixture = [](const FCase& Case)
		{
			return Case.RequiredAuthority == EAuthority::GeneratedSource
				? MakeGeneratedSourceIndex() : MakeSourceIndex();
		};
		auto ApplyReference = [&](FAngelscriptCachedSourceIndex& Source,
			const FCase& Case, const FAngelscriptHash256& Key)
		{
			switch (Case.Field)
			{
			case EField::MountProvider:
				Source.Mounts[0].ProviderKey.Hash = Key;
				Source.Mounts[0].MountKey = BuildMountKey(Source.Mounts[0]);
				break;
			case EField::FileMount:
				Source.Files[0].MountKey.Hash = Key;
				Source.Files[0].SourceFileKey = BuildSourceFileKey(Source.Files[0]);
				break;
			case EField::FileProvider:
				Source.Files[0].ProviderKey.Hash = Key;
				Source.Files[0].SourceFileKey = BuildSourceFileKey(Source.Files[0]);
				break;
			case EField::HookScope:
				Source.PreprocessHooks.Add(MakeHook(TEXT("Matrix.TypedScope"),
					ScopeKindFor(Case.RequiredAuthority), Key, 0xd0));
				break;
			case EField::InputOwnerScope:
				Source.PreprocessorInputs[0].OwnerScopeKind = ScopeKindFor(Case.RequiredAuthority);
				Source.PreprocessorInputs[0].OwnerScopeStableKey = Key;
				Source.PreprocessorInputs[0].InputKey = BuildInputKey(Source.PreprocessorInputs[0]);
				break;
			case EField::IneligibleScope:
				Source.IneligibleScopes.Add(MakeIneligibleScope(
					ScopeKindFor(Case.RequiredAuthority), Key,
					Case.RequiredAuthority == EAuthority::Hook
						? EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource
						: EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
					TEXT("Typed scope matrix")));
				break;
			case EField::InputTarget:
				Source.PreprocessorInputs[0].TargetKind = TargetKindFor(Case.RequiredAuthority);
				Source.PreprocessorInputs[0].TargetStableKey = Key;
				Source.PreprocessorInputs[0].InputKey = BuildInputKey(Source.PreprocessorInputs[0]);
				break;
			case EField::EdgeFrom:
				Source.Edges[0].FromSourceFileKey.Hash = Key;
				Source.Edges[0].EdgeKey = BuildEdgeKey(Source.Edges[0]);
				break;
			case EField::EdgeIncludeTo:
				Source.Edges[0].EdgeKind = EAngelscriptCachedSourceEdgeKind::Include;
				Source.Edges[0].ToSourceOrGeneratedKey = Key;
				Source.Edges[0].EdgeKey = BuildEdgeKey(Source.Edges[0]);
				break;
			case EField::EdgeGeneratedTo:
				Source.Edges[0].EdgeKind = EAngelscriptCachedSourceEdgeKind::GeneratedSource;
				Source.Edges[0].ToSourceOrGeneratedKey = Key;
				Source.Edges[0].EdgeKey = BuildEdgeKey(Source.Edges[0]);
				break;
			default:
				checkNoEntry();
			}
		};

		TArray<uint8> Bytes;
		for (int32 CaseIndex = 0; CaseIndex < static_cast<int32>(UE_ARRAY_COUNT(Cases)); ++CaseIndex)
		{
			const FCase& Case = Cases[CaseIndex];
			FAngelscriptCachedSourceIndex Valid = MakeFixture(Case);
			ApplyReference(Valid, Case, AuthorityKey(Valid, Case.RequiredAuthority));
			ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Valid).IsSuccess()));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				Valid, Bytes).IsSuccess()));

			FAngelscriptCachedSourceIndex Wrong = MakeFixture(Case);
			ApplyReference(Wrong, Case,
				AuthorityKey(Wrong, WrongAuthorityFor(Case.RequiredAuthority)));
			Bytes = {0xaa};
			FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Wrong, Bytes);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
				Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
			ASSERT_THAT(AreEqual(UINT64_C(0), Result.ByteOffset));
			ASSERT_THAT(IsTrue(Bytes.IsEmpty()));

			FAngelscriptCachedSourceIndex Missing = MakeFixture(Case);
			const FAngelscriptHash256 MissingKey = MakeHash(
				static_cast<uint8>(0xe0 + CaseIndex));
			ApplyReference(Missing, Case, MissingKey);
			Bytes = {0xbb};
			Result = FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Missing, Bytes);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget,
				Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
			ASSERT_THAT(AreEqual(UINT64_C(0), Result.ByteOffset));
			ASSERT_THAT(IsTrue(Bytes.IsEmpty()));
		}

		FAngelscriptCachedSourceIndex SourceKindConflict = MakeSourceIndex();
		SourceKindConflict.Files[0].SourceKind = EAngelscriptCachedSourceKind::Plugin;
		RebuildSingleSourceGraphKeys(SourceKindConflict);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				SourceKindConflict, Bytes).Error));

		FAngelscriptCachedSourceIndex ProviderConflict = MakeSourceIndex();
		FAngelscriptCachedSourceProvider SecondProvider = ProviderConflict.Providers[0];
		SecondProvider.CanonicalImplementationIdentity = TEXT("Runtime.SecondProvider");
		SecondProvider.IdentityFingerprint = MakeHash(0xfa);
		SecondProvider.ProviderKey = BuildProviderKey(SecondProvider);
		ProviderConflict.Providers.Add(SecondProvider);
		ProviderConflict.Files[0].ProviderKey = SecondProvider.ProviderKey;
		RebuildSingleFileKeyAndDependents(ProviderConflict);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				ProviderConflict, Bytes).Error));

		FAngelscriptCachedSourceIndex FileInternalOrder = MakeSourceIndex();
		FileInternalOrder.Files[0].SourceKind = EAngelscriptCachedSourceKind::Plugin;
		FileInternalOrder.Files[0].MountKey.Hash = MakeHash(0xc1);
		FileInternalOrder.Files[0].ProviderKey.Hash =
			FileInternalOrder.Mounts[0].MountKey.Hash;
		FileInternalOrder.Files[0].SourceFileKey =
			BuildSourceFileKey(FileInternalOrder.Files[0]);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				FileInternalOrder, Bytes).Error));

		FAngelscriptCachedSourceIndex InputInternalOrder = MakeSourceIndex();
		InputInternalOrder.PreprocessorInputs[0].OwnerScopeKind =
			EAngelscriptCachedFastPathScopeKind::Module;
		InputInternalOrder.PreprocessorInputs[0].OwnerScopeStableKey = MakeHash(0xc2);
		InputInternalOrder.PreprocessorInputs[0].TargetKind =
			EAngelscriptCachePreprocessorInputTargetKind::Provider;
		InputInternalOrder.PreprocessorInputs[0].TargetStableKey =
			InputInternalOrder.Files[0].SourceFileKey.Hash;
		InputInternalOrder.PreprocessorInputs[0].InputKey =
			BuildInputKey(InputInternalOrder.PreprocessorInputs[0]);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				InputInternalOrder, Bytes).Error));

		FAngelscriptCachedSourceIndex EdgeInternalOrder = MakeSourceIndex();
		EdgeInternalOrder.Edges[0].FromSourceFileKey.Hash = MakeHash(0xc3);
		EdgeInternalOrder.Edges[0].ToSourceOrGeneratedKey =
			EdgeInternalOrder.Providers[0].ProviderKey.Hash;
		EdgeInternalOrder.Edges[0].EdgeKey = BuildEdgeKey(EdgeInternalOrder.Edges[0]);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				EdgeInternalOrder, Bytes).Error));
	}

	TEST_METHOD(EligibilityUsesTransitiveHookClosureAndKeepsTwoModulesIndependent)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		FAngelscriptCachedSourceIndex DirectScopes = MakeSourceIndex();
		DirectScopes.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Module, DirectScopes.Files[0].ModuleKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior, TEXT("Module direct")));
		DirectScopes.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::SourceFile, DirectScopes.Files[0].SourceFileKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior, TEXT("File direct")));
		DirectScopes.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Hook, DirectScopes.PreprocessHooks[0].HookKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource, TEXT("Hook direct")));
		DirectScopes.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Provider, DirectScopes.Providers[0].ProviderKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior, TEXT("Provider direct")));
		DirectScopes.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Mount, DirectScopes.Mounts[0].MountKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior, TEXT("Mount direct")));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(DirectScopes).IsSuccess()));
		FAngelscriptCacheExactFastPathEligibility DirectEligibility;
		ASSERT_THAT(IsTrue(QueryEligibility(
			DirectScopes, DirectScopes.Files[0].ModuleKey, DirectEligibility).IsSuccess()));
		ASSERT_THAT(AreEqual(5, DirectEligibility.MatchingScopes.Num()));
		for (int32 Index = 0; Index < DirectEligibility.MatchingScopes.Num(); ++Index)
		{
			ASSERT_THAT(AreEqual(Index + 1,
				static_cast<int32>(DirectEligibility.MatchingScopes[Index].ScopeKind),
				TEXT("MatchingScopes must preserve canonical SourceIndex scope ordering")));
		}

		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		const FAngelscriptCachedSourceFile SecondaryFile = AddSecondaryFile(Source);
		FAngelscriptCachedPreprocessHook RootHook = MakeHook(TEXT("Chain.Root"),
			EAngelscriptCachedFastPathScopeKind::Module, Source.Files[0].ModuleKey.Hash, 0x80);
		FAngelscriptCachedPreprocessHook TransitiveHook = MakeHook(TEXT("Chain.Transitive"),
			EAngelscriptCachedFastPathScopeKind::Hook, RootHook.HookKey.Hash, 0x90);
		TransitiveHook.CapabilityFlags &= ~static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint);
		TransitiveHook.ContentFingerprint.Reset();
		Source.PreprocessHooks.Add(RootHook);
		Source.PreprocessHooks.Add(TransitiveHook);
		Source.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Hook, TransitiveHook.HookKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("Transitive hook lacks content fingerprint")));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));
		FAngelscriptCacheExactFastPathEligibility UnrelatedEligibility;
		ASSERT_THAT(IsTrue(QueryEligibility(
			Source, SecondaryFile.ModuleKey, UnrelatedEligibility).IsSuccess()));
		ASSERT_THAT(IsTrue(UnrelatedEligibility.bExactFastPathEligible));
		ASSERT_THAT(IsTrue(UnrelatedEligibility.MatchingScopes.IsEmpty()));

		FAngelscriptCachedPreprocessHook DetachedRoot = MakeHook(TEXT("Villain.Detached.Root"),
			EAngelscriptCachedFastPathScopeKind::Module, SecondaryFile.ModuleKey.Hash, 0xd0);
		FAngelscriptCachedPreprocessHook DetachedLeaf = MakeHook(TEXT("Villain.Detached.Leaf"),
			EAngelscriptCachedFastPathScopeKind::Hook, DetachedRoot.HookKey.Hash, 0xe0);
		DetachedLeaf.CapabilityFlags &= ~static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint);
		DetachedLeaf.ContentFingerprint.Reset();
		Source.PreprocessHooks.Add(DetachedRoot);
		Source.PreprocessHooks.Add(DetachedLeaf);
		Source.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Hook, DetachedLeaf.HookKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("Detached hook chain belongs only to the secondary module")));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));
		FAngelscriptCacheExactFastPathEligibility DetachedHeroEligibility;
		ASSERT_THAT(IsTrue(QueryEligibility(
			Source, Source.Files[0].ModuleKey, DetachedHeroEligibility).IsSuccess()));
		ASSERT_THAT(IsFalse(DetachedHeroEligibility.bExactFastPathEligible));
		ASSERT_THAT(AreEqual(1, DetachedHeroEligibility.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(TransitiveHook.HookKey.Hash.ToHexString(),
			DetachedHeroEligibility.MatchingScopes[0].ScopeStableKey.ToHexString()));

		FAngelscriptCachedPreprocessHook SecondaryHook = MakeHook(TEXT("Villain.Direct"),
			EAngelscriptCachedFastPathScopeKind::SourceFile, SecondaryFile.SourceFileKey.Hash, 0xc0);
		SecondaryHook.CapabilityFlags &= ~static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint);
		SecondaryHook.ContentFingerprint.Reset();
		Source.PreprocessHooks.Add(SecondaryHook);
		Source.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Hook, SecondaryHook.HookKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("Secondary module hook")));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));

		FAngelscriptCacheExactFastPathEligibility HeroEligibility;
		ASSERT_THAT(IsTrue(QueryEligibility(
			Source, Source.Files[0].ModuleKey, HeroEligibility).IsSuccess()));
		ASSERT_THAT(IsFalse(HeroEligibility.bExactFastPathEligible));
		ASSERT_THAT(AreEqual(1, HeroEligibility.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(TransitiveHook.HookKey.Hash.ToHexString(),
			HeroEligibility.MatchingScopes[0].ScopeStableKey.ToHexString()));

		FAngelscriptCacheExactFastPathEligibility VillainEligibility;
		ASSERT_THAT(IsTrue(QueryEligibility(
			Source, SecondaryFile.ModuleKey, VillainEligibility).IsSuccess()));
		ASSERT_THAT(IsFalse(VillainEligibility.bExactFastPathEligible));
		ASSERT_THAT(AreEqual(2, VillainEligibility.MatchingScopes.Num()));
		bool bFoundDetachedLeaf = false;
		bool bFoundSecondaryHook = false;
		for (const FAngelscriptCachedFastPathIneligibleScope& Scope : VillainEligibility.MatchingScopes)
		{
			bFoundDetachedLeaf |= Scope.ScopeStableKey == DetachedLeaf.HookKey.Hash;
			bFoundSecondaryHook |= Scope.ScopeStableKey == SecondaryHook.HookKey.Hash;
		}
		ASSERT_THAT(IsTrue(bFoundDetachedLeaf));
		ASSERT_THAT(IsTrue(bFoundSecondaryHook));

		FAngelscriptCacheExactFastPathEligibility MissingEligibility;
		MissingEligibility.bExactFastPathEligible = true;
		MissingEligibility.MatchingScopes.Add(Source.IneligibleScopes[0]);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget,
			QueryEligibility(
				Source, FAngelscriptStableModuleKey{MakeHash(0xfd)}, MissingEligibility).Error));
		ASSERT_THAT(IsFalse(MissingEligibility.bExactFastPathEligible));
		ASSERT_THAT(IsTrue(MissingEligibility.MatchingScopes.IsEmpty()));
	}

	TEST_METHOD(ModuleDeclarationKindsAndLocalOwnerMatrixAreValidated)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		TArray<uint8> Bytes;
		const FAngelscriptStableModuleKey ModuleKey = MakeModuleKey();
		const EAngelscriptArtifactEntityKind TypeKinds[] = {
			EAngelscriptArtifactEntityKind::Class,
			EAngelscriptArtifactEntityKind::Struct,
			EAngelscriptArtifactEntityKind::Interface,
			EAngelscriptArtifactEntityKind::Enum,
			EAngelscriptArtifactEntityKind::Delegate,
			EAngelscriptArtifactEntityKind::Typedef,
			EAngelscriptArtifactEntityKind::Funcdef,
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(TypeKinds); ++Index)
		{
			TArray<FAngelscriptCachedDeclaration> Declarations;
			Declarations.Add(MakeTypeDeclaration(ModuleKey, TypeKinds[Index],
				FString::Printf(TEXT("Type%d"), Index)));
			const FAngelscriptCachedModuleInterface Interface =
				MakeDeclarationInterface(MoveTemp(Declarations));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				Interface, Bytes).IsSuccess()));
		}

		const EAngelscriptArtifactEntityKind MethodOwnerKinds[] = {
			EAngelscriptArtifactEntityKind::Class,
			EAngelscriptArtifactEntityKind::Struct,
			EAngelscriptArtifactEntityKind::Interface,
		};
		for (const EAngelscriptArtifactEntityKind OwnerKind : MethodOwnerKinds)
		{
			FAngelscriptCachedDeclaration Owner = MakeTypeDeclaration(ModuleKey, OwnerKind, TEXT("Owner"));
			FAngelscriptCachedDeclaration Method = MakeOwnedFunctionDeclaration(ModuleKey,
				EAngelscriptArtifactEntityKind::Method, EAngelscriptFunctionOwnerKind::Type,
				Owner.StableKey, TEXT("Run"));
			TArray<FAngelscriptCachedDeclaration> Declarations;
			Declarations.Add(Owner);
			Declarations.Add(Method);
			const FAngelscriptCachedModuleInterface Interface =
				MakeDeclarationInterface(MoveTemp(Declarations));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				Interface, Bytes).IsSuccess()));
		}

		const EAngelscriptArtifactEntityKind ClassOnlyCallables[] = {
			EAngelscriptArtifactEntityKind::Constructor,
			EAngelscriptArtifactEntityKind::Destructor,
			EAngelscriptArtifactEntityKind::Factory,
			EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor,
			EAngelscriptArtifactEntityKind::InitDefaults,
		};
		for (const EAngelscriptArtifactEntityKind CallableKind : ClassOnlyCallables)
		{
			for (const EAngelscriptArtifactEntityKind OwnerKind : {
				EAngelscriptArtifactEntityKind::Class, EAngelscriptArtifactEntityKind::Struct})
			{
				FAngelscriptCachedDeclaration Owner = MakeTypeDeclaration(ModuleKey, OwnerKind, TEXT("CallableOwner"));
				FAngelscriptCachedDeclaration Callable = MakeOwnedFunctionDeclaration(ModuleKey,
					CallableKind, EAngelscriptFunctionOwnerKind::Type, Owner.StableKey, TEXT("Callable"));
				TArray<FAngelscriptCachedDeclaration> Declarations{Owner, Callable};
				const FAngelscriptCachedModuleInterface Interface =
					MakeDeclarationInterface(MoveTemp(Declarations));
				ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
					Interface, Bytes).IsSuccess()));
			}
		}

		FAngelscriptCachedDeclaration ModuleInitializer = MakeOwnedFunctionDeclaration(ModuleKey,
			EAngelscriptArtifactEntityKind::ModuleInitializer, EAngelscriptFunctionOwnerKind::Module,
			ModuleKey.Hash, TEXT("ModuleInitializer"));
		TArray<FAngelscriptCachedDeclaration> ModuleInitializerDeclarations{ModuleInitializer};
		const FAngelscriptCachedModuleInterface ModuleInitializerInterface =
			MakeDeclarationInterface(MoveTemp(ModuleInitializerDeclarations));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			ModuleInitializerInterface, Bytes).IsSuccess()));

		for (const EAngelscriptArtifactEntityKind OwnerKind : {
			EAngelscriptArtifactEntityKind::Class, EAngelscriptArtifactEntityKind::Struct})
		{
			FAngelscriptCachedDeclaration Owner = MakeTypeDeclaration(ModuleKey, OwnerKind, TEXT("StorageOwner"));
			FAngelscriptCachedDeclaration Property = MakePropertyDeclaration(
				ModuleKey, Owner.StableKey, TEXT("Health"), 1);
			TArray<FAngelscriptCachedDeclaration> Declarations;
			Declarations.Add(Owner);
			Declarations.Add(Property);
			const FAngelscriptCachedModuleInterface Interface =
				MakeDeclarationInterface(MoveTemp(Declarations));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				Interface, Bytes).IsSuccess()));
		}

		for (const EAngelscriptArtifactEntityKind OwnerKind : {
			EAngelscriptArtifactEntityKind::Delegate, EAngelscriptArtifactEntityKind::Funcdef})
		{
			FAngelscriptCachedDeclaration Owner = MakeTypeDeclaration(ModuleKey, OwnerKind, TEXT("Callback"));
			FAngelscriptCachedDeclaration Signature = MakeOwnedFunctionDeclaration(ModuleKey,
				EAngelscriptArtifactEntityKind::DelegateSignature, EAngelscriptFunctionOwnerKind::Type,
				Owner.StableKey, TEXT("Invoke"));
			TArray<FAngelscriptCachedDeclaration> Declarations;
			Declarations.Add(Owner);
			Declarations.Add(Signature);
			const FAngelscriptCachedModuleInterface Interface =
				MakeDeclarationInterface(MoveTemp(Declarations));
			ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				Interface, Bytes).IsSuccess()));
		}

		FAngelscriptCachedDeclaration Global = MakeGlobalDeclaration(ModuleKey, TEXT("Counter"));
		FAngelscriptCachedDeclaration Initializer = MakeOwnedFunctionDeclaration(ModuleKey,
			EAngelscriptArtifactEntityKind::GlobalInitializer, EAngelscriptFunctionOwnerKind::Global,
			Global.StableKey, TEXT("InitializeCounter"));
		TArray<FAngelscriptCachedDeclaration> GlobalDeclarations;
		GlobalDeclarations.Add(Global);
		GlobalDeclarations.Add(Initializer);
		const FAngelscriptCachedModuleInterface GlobalInterface =
			MakeDeclarationInterface(MoveTemp(GlobalDeclarations));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			GlobalInterface, Bytes).IsSuccess()));

		auto MakeUncheckedInterface = [&](TArray<FAngelscriptCachedDeclaration>&& Declarations)
		{
			FAngelscriptCachedModuleInterface Interface;
			Interface.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			Interface.ModuleKey = ModuleKey;
			Interface.CanonicalModuleName = TEXT("Hero");
			Interface.CanonicalNamespaces = {TEXT("Gameplay")};
			Interface.Declarations = MoveTemp(Declarations);
			return Interface;
		};

		for (const EAngelscriptArtifactEntityKind CallableKind : ClassOnlyCallables)
		{
			FAngelscriptCachedDeclaration InterfaceOwner = MakeTypeDeclaration(
				ModuleKey, EAngelscriptArtifactEntityKind::Interface, TEXT("InterfaceOwner"));
			FAngelscriptCachedDeclaration Callable = MakeOwnedFunctionDeclaration(ModuleKey,
				CallableKind, EAngelscriptFunctionOwnerKind::Type,
				InterfaceOwner.StableKey, TEXT("ForbiddenCallable"));
			TArray<FAngelscriptCachedDeclaration> InvalidCallableDeclarations{InterfaceOwner, Callable};
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
				FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
					MakeUncheckedInterface(MoveTemp(InvalidCallableDeclarations)), Bytes).Error));
		}

		for (const EAngelscriptArtifactEntityKind InvalidDelegateOwnerKind : {
			EAngelscriptArtifactEntityKind::Class,
			EAngelscriptArtifactEntityKind::Struct,
			EAngelscriptArtifactEntityKind::Interface,
			EAngelscriptArtifactEntityKind::Enum,
			EAngelscriptArtifactEntityKind::Typedef})
		{
			FAngelscriptCachedDeclaration InvalidDelegateOwner = MakeTypeDeclaration(
				ModuleKey, InvalidDelegateOwnerKind, TEXT("InvalidDelegateOwner"));
			FAngelscriptCachedDeclaration Signature = MakeOwnedFunctionDeclaration(ModuleKey,
				EAngelscriptArtifactEntityKind::DelegateSignature, EAngelscriptFunctionOwnerKind::Type,
				InvalidDelegateOwner.StableKey, TEXT("ForbiddenSignature"));
			TArray<FAngelscriptCachedDeclaration> InvalidSignatureDeclarations{InvalidDelegateOwner, Signature};
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
				FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
					MakeUncheckedInterface(MoveTemp(InvalidSignatureDeclarations)), Bytes).Error));
		}

		FAngelscriptCachedDeclaration WrongModuleInitializerOwner = ModuleInitializer;
		WrongModuleInitializerOwner.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
		WrongModuleInitializerOwner.OwnerKey = MakeHash(0xab);
		FinalizeDeclarationIdentityAndHashes(WrongModuleInitializerOwner);
		TArray<FAngelscriptCachedDeclaration> InvalidModuleInitializer{WrongModuleInitializerOwner};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				MakeUncheckedInterface(MoveTemp(InvalidModuleInitializer)), Bytes).Error));

		FAngelscriptCachedDeclaration WrongGlobalInitializerOwner = Initializer;
		WrongGlobalInitializerOwner.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
		FinalizeDeclarationIdentityAndHashes(WrongGlobalInitializerOwner);
		TArray<FAngelscriptCachedDeclaration> InvalidGlobalInitializer{Global, WrongGlobalInitializerOwner};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				MakeUncheckedInterface(MoveTemp(InvalidGlobalInitializer)), Bytes).Error));

		FAngelscriptCachedDeclaration EnumOwner = MakeTypeDeclaration(
			ModuleKey, EAngelscriptArtifactEntityKind::Enum, TEXT("WrongOwner"));
		FAngelscriptCachedDeclaration EnumMethod = MakeOwnedFunctionDeclaration(ModuleKey,
			EAngelscriptArtifactEntityKind::Method, EAngelscriptFunctionOwnerKind::Type,
			EnumOwner.StableKey, TEXT("Run"));
		TArray<FAngelscriptCachedDeclaration> InvalidDeclarations;
		InvalidDeclarations.Add(EnumOwner);
		InvalidDeclarations.Add(EnumMethod);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				MakeUncheckedInterface(MoveTemp(InvalidDeclarations)), Bytes).Error));

		FAngelscriptCachedDeclaration DelegateOwner = MakeTypeDeclaration(
			ModuleKey, EAngelscriptArtifactEntityKind::Delegate, TEXT("WrongStorageOwner"));
		FAngelscriptCachedDeclaration DelegateProperty = MakePropertyDeclaration(
			ModuleKey, DelegateOwner.StableKey, TEXT("Value"), 1);
		InvalidDeclarations = {DelegateOwner, DelegateProperty};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				MakeUncheckedInterface(MoveTemp(InvalidDeclarations)), Bytes).Error));

		FAngelscriptCachedDeclaration MissingOwnerMethod = MakeOwnedFunctionDeclaration(ModuleKey,
			EAngelscriptArtifactEntityKind::Method, EAngelscriptFunctionOwnerKind::Type,
			MakeHash(0xfa), TEXT("MissingOwnerMethod"));
		InvalidDeclarations = {MissingOwnerMethod};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingOwner,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				MakeUncheckedInterface(MoveTemp(InvalidDeclarations)), Bytes).Error));

		FAngelscriptCachedDeclaration WrongKindGlobal = MakeGlobalDeclaration(ModuleKey, TEXT("WrongKindGlobal"));
		FAngelscriptCachedDeclaration WrongKindMethod = MakeOwnedFunctionDeclaration(ModuleKey,
			EAngelscriptArtifactEntityKind::Method, EAngelscriptFunctionOwnerKind::Type,
			WrongKindGlobal.StableKey, TEXT("WrongKindMethod"));
		InvalidDeclarations = {WrongKindGlobal, WrongKindMethod};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				MakeUncheckedInterface(MoveTemp(InvalidDeclarations)), Bytes).Error));

		FAngelscriptCachedDeclaration WrongOwnerKind = WrongKindMethod;
		WrongOwnerKind.OwnerKind = EAngelscriptFunctionOwnerKind::Property;
		WrongOwnerKind.OwnerKey = {};
		FinalizeDeclarationIdentityAndHashes(WrongOwnerKind);
		InvalidDeclarations = {WrongOwnerKind};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				MakeUncheckedInterface(MoveTemp(InvalidDeclarations)), Bytes).Error));

		FAngelscriptCachedDeclaration CrossModule = MakeFunctionDeclaration(MakeSecondaryModuleKey());
		InvalidDeclarations = {CrossModule};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::CrossModuleOwner,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				MakeUncheckedInterface(MoveTemp(InvalidDeclarations)), Bytes).Error));
	}

	TEST_METHOD(GeneratedDelegateMembersRequireExactGeneratedTraits)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		const FAngelscriptStableModuleKey ModuleKey = MakeModuleKey();
		const uint32 GeneratedFlag = static_cast<uint32>(
			EAngelscriptCachedDeclarationTraitFlags::Generated);

		auto SetGenerated = [GeneratedFlag](
			FAngelscriptCachedDeclaration& Declaration, const bool bGenerated)
		{
			if (bGenerated)
			{
				Declaration.TraitFlags |= GeneratedFlag;
			}
			else
			{
				Declaration.TraitFlags &= ~GeneratedFlag;
			}
			FinalizeDeclarationIdentityAndHashes(Declaration);
		};
		auto MakeDelegateInterface = [&](
			const EAngelscriptArtifactEntityKind MemberKind,
			const bool bOwnerGenerated,
			const bool bMemberGenerated)
		{
			FAngelscriptCachedDeclaration Owner = MakeTypeDeclaration(
				ModuleKey, EAngelscriptArtifactEntityKind::Delegate, TEXT("GeneratedDelegate"));
			SetGenerated(Owner, bOwnerGenerated);

			FAngelscriptCachedDeclaration Member = MemberKind == EAngelscriptArtifactEntityKind::Property
				? MakePropertyDeclaration(ModuleKey, Owner.StableKey, TEXT("GeneratedMember"), 1)
				: MakeOwnedFunctionDeclaration(ModuleKey, MemberKind,
					EAngelscriptFunctionOwnerKind::Type, Owner.StableKey, TEXT("GeneratedMember"));
			SetGenerated(Member, bMemberGenerated);

			FAngelscriptCachedModuleInterface Interface;
			Interface.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			Interface.ModuleKey = ModuleKey;
			Interface.CanonicalModuleName = TEXT("Hero");
			Interface.CanonicalNamespaces = {TEXT("Gameplay")};
			Interface.Declarations = {MoveTemp(Owner), MoveTemp(Member)};
			return Interface;
		};
		auto ExpectAccepted = [&](
			const EAngelscriptArtifactEntityKind MemberKind,
			const bool bOwnerGenerated,
			const bool bMemberGenerated)
		{
			FAngelscriptCachedModuleInterface Interface = MakeDelegateInterface(
				MemberKind, bOwnerGenerated, bMemberGenerated);
			FAngelscriptHash256 InterfaceAbi;
			const FAngelscriptCacheValidationResult AbiResult =
				FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(Interface, InterfaceAbi);
			if (!AbiResult.IsSuccess())
			{
				return false;
			}
			Interface.InterfaceAbi = InterfaceAbi;
			TArray<uint8> FirstBytes;
			TArray<uint8> SecondBytes;
			return FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
					Interface, FirstBytes).IsSuccess()
				&& FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
					Interface, SecondBytes).IsSuccess()
				&& !FirstBytes.IsEmpty()
				&& FirstBytes == SecondBytes;
		};
		auto Validate = [&](
			const EAngelscriptArtifactEntityKind MemberKind,
			const bool bOwnerGenerated,
			const bool bMemberGenerated)
		{
			FAngelscriptCachedModuleInterface Interface = MakeDelegateInterface(
				MemberKind, bOwnerGenerated, bMemberGenerated);
			FAngelscriptHash256 InterfaceAbi;
			return FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
				Interface, InterfaceAbi);
		};

		const EAngelscriptArtifactEntityKind OwnerAndMemberGeneratedKinds[] = {
			EAngelscriptArtifactEntityKind::Property,
			EAngelscriptArtifactEntityKind::Method,
			EAngelscriptArtifactEntityKind::Constructor,
			EAngelscriptArtifactEntityKind::Destructor,
		};
		for (const EAngelscriptArtifactEntityKind MemberKind : OwnerAndMemberGeneratedKinds)
		{
			ASSERT_THAT(IsTrue(ExpectAccepted(MemberKind, true, true)));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
				Validate(MemberKind, false, true).Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
				Validate(MemberKind, true, false).Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
				Validate(MemberKind, false, false).Error));
		}

		const EAngelscriptArtifactEntityKind MemberGeneratedKinds[] = {
			EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor,
			EAngelscriptArtifactEntityKind::InitDefaults,
		};
		for (const EAngelscriptArtifactEntityKind MemberKind : MemberGeneratedKinds)
		{
			ASSERT_THAT(IsTrue(ExpectAccepted(MemberKind, true, true)));
			ASSERT_THAT(IsTrue(ExpectAccepted(MemberKind, false, true)));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
				Validate(MemberKind, false, false).Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
				Validate(MemberKind, true, false).Error));
		}

		for (const bool bOwnerGenerated : {false, true})
		{
			for (const bool bMemberGenerated : {false, true})
			{
				ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
					Validate(EAngelscriptArtifactEntityKind::Factory,
						bOwnerGenerated, bMemberGenerated).Error));
			}
		}
	}

	TEST_METHOD(DeclarationCoverageAndValueTypePresenceAreFailClosed)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		TArray<uint8> Bytes;
		const FAngelscriptStableModuleKey ModuleKey = MakeModuleKey();
		auto SerializeSingle = [&](FAngelscriptCachedDeclaration Declaration)
		{
			FAngelscriptCachedModuleInterface Interface;
			Interface.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			Interface.ModuleKey = ModuleKey;
			Interface.CanonicalModuleName = TEXT("Hero");
			Interface.CanonicalNamespaces = {TEXT("Gameplay")};
			Interface.Declarations.Add(MoveTemp(Declaration));
			return FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error;
		};

		FAngelscriptCachedDeclaration Type = MakeTypeDeclaration(
			ModuleKey, EAngelscriptArtifactEntityKind::Class, TEXT("HeroType"));
		Type.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeSingle(Type)));

		FAngelscriptCachedDeclaration Function = MakeFunctionDeclaration(ModuleKey);
		Function.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeSingle(Function)));
		Function = MakeFunctionDeclaration(ModuleKey);
		Function.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeSingle(Function)));

		FAngelscriptCachedDeclaration Global = MakeGlobalDeclaration(ModuleKey, TEXT("Counter"));
		Global.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeSingle(Global)));
		Global = MakeGlobalDeclaration(ModuleKey, TEXT("Counter"));
		Global.BodyCoverage = EAngelscriptCacheBodyCoverage::Required;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeSingle(Global)));
		Global = MakeGlobalDeclaration(ModuleKey, TEXT("Counter"));
		Global.CanonicalTypeSpelling.Reset();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeSingle(Global)));
		Global = MakeGlobalDeclaration(ModuleKey, TEXT("Counter"));
		Global.DeclaredType.Reset();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeSingle(Global)));
		Global = MakeGlobalDeclaration(ModuleKey, TEXT("Counter"));
		Global.CanonicalTypeSpelling = TEXT("float");
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch, SerializeSingle(Global)));
		Global = MakeGlobalDeclaration(ModuleKey, TEXT("Counter"));
		Global.DeclaredType = MakePrimitive(EAngelscriptCachedPrimitiveType::Float32);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch, SerializeSingle(Global)));

		FAngelscriptCachedDeclaration Owner = MakeTypeDeclaration(
			ModuleKey, EAngelscriptArtifactEntityKind::Class, TEXT("Owner"));
		auto SerializeProperty = [&](FAngelscriptCachedDeclaration Property)
		{
			FAngelscriptCachedModuleInterface Interface;
			Interface.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			Interface.ModuleKey = ModuleKey;
			Interface.CanonicalModuleName = TEXT("Hero");
			Interface.CanonicalNamespaces = {TEXT("Gameplay")};
			Interface.Declarations = {Owner, MoveTemp(Property)};
			return FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error;
		};
		FAngelscriptCachedDeclaration Property = MakePropertyDeclaration(
			ModuleKey, Owner.StableKey, TEXT("Health"), 1);
		Property.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeProperty(Property)));
		Property = MakePropertyDeclaration(ModuleKey, Owner.StableKey, TEXT("Health"), 1);
		Property.BodyCoverage = EAngelscriptCacheBodyCoverage::Required;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeProperty(Property)));
		Property = MakePropertyDeclaration(ModuleKey, Owner.StableKey, TEXT("Health"), 1);
		Property.CanonicalTypeSpelling.Reset();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeProperty(Property)));
		Property = MakePropertyDeclaration(ModuleKey, Owner.StableKey, TEXT("Health"), 1);
		Property.DeclaredType.Reset();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, SerializeProperty(Property)));
		Property = MakePropertyDeclaration(ModuleKey, Owner.StableKey, TEXT("Health"), 1);
		Property.CanonicalTypeSpelling = TEXT("float");
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch, SerializeProperty(Property)));
		Property = MakePropertyDeclaration(ModuleKey, Owner.StableKey, TEXT("Health"), 1);
		Property.DeclaredType = MakePrimitive(EAngelscriptCachedPrimitiveType::Float32);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch, SerializeProperty(Property)));
	}

	TEST_METHOD(ImportIdentityAuthorityAndSlotConflictsAreIndependent)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		TArray<uint8> Bytes;
		FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		const FAngelscriptCachedImportDeclaration Import = Interface.Imports[0];
		const FAngelscriptImportIdentityInput Identity{
			Interface.ModuleKey, Import.CanonicalNamespace, Import.CanonicalName,
			Import.CanonicalSignature, Import.TargetModuleKey,
			FAngelscriptStableFunctionKey{Import.TargetDeclaration.StableKey}};
		FAngelscriptStableImportKey Key;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::TryBuildImportKey(Identity, Key).IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT("c589bbd149afb8b12650f68d65f428fa631f243c1590ed67d1460b63fcdc213c")), Key.Hash.ToHexString(),
			*FString::Printf(TEXT("Import identity key golden actual=%s"), *Key.Hash.ToHexString())));

		FAngelscriptImportIdentityInput InvalidIdentity = Identity;
		InvalidIdentity.TargetFunctionKey = {};
		Key.Hash = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::TryBuildImportKey(InvalidIdentity, Key).Error));
		ASSERT_THAT(IsTrue(Key.Hash.IsZero()));

		Interface = MakeModuleInterface();
		Interface.Imports[0].TargetDeclaration.Kind = EAngelscriptCacheReferenceKind::ScriptType;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));
		Interface = MakeModuleInterface();
		Interface.Imports[0].TargetDeclaration.ExpectedAbi = {};
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingExpectedAbi,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));
		Interface = MakeModuleInterface();
		Interface.Imports[0].Slots.Reset();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		FAngelscriptCachedImportDeclaration SlotConflict = Interface.Imports[0];
		SlotConflict.Slots[0].Ordinal = 1;
		Interface.Imports.Add(SlotConflict);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		FAngelscriptCachedDeclaration DeclarationSlotConflict = Interface.Declarations[0];
		DeclarationSlotConflict.Slots[0].Ordinal = 1;
		Interface.Declarations.Add(DeclarationSlotConflict);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));
	}

	TEST_METHOD(RecordsRoundTripReserializeAndCanonicalizeInsertionOrder)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		TArray<uint8> SourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, SourceBytes).IsSuccess()));
		Algo::Reverse(Source.DiscoveryPolicy.Options);
		Algo::Reverse(Source.Mounts[0].Options);
		TArray<uint8> ReorderedSourceBytes;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, ReorderedSourceBytes).IsSuccess()));
		ASSERT_THAT(AreEqual(Hex(SourceBytes), Hex(ReorderedSourceBytes)));

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget SourceRoundTripBudget;
		FAngelscriptCachedSourceIndex DecodedSource;
		ASSERT_THAT(IsTrue(DecodeSourceIndexForTests(
			SourceBytes, Limits, SourceRoundTripBudget, DecodedSource).IsSuccess()));
		TArray<uint8> ReserializedSource;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			DecodedSource, ReserializedSource).IsSuccess()));
		ASSERT_THAT(AreEqual(Hex(SourceBytes), Hex(ReserializedSource)));

		FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		TArray<uint8> InterfaceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			Interface, InterfaceBytes).IsSuccess()));
		Algo::Reverse(Interface.Declarations[0].CanonicalIdentityTraits);
		Algo::Reverse(Interface.Declarations[0].Metadata);
		Algo::Reverse(Interface.Dependencies);
		TArray<uint8> ReorderedInterfaceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			Interface, ReorderedInterfaceBytes).IsSuccess()));
		ASSERT_THAT(AreEqual(Hex(InterfaceBytes), Hex(ReorderedInterfaceBytes)));

		FAngelscriptCacheReadBudget InterfaceRoundTripBudget;
		FAngelscriptCachedModuleInterface DecodedInterface;
		ASSERT_THAT(IsTrue(DecodeModuleInterfaceForTests(
			InterfaceBytes, Limits, InterfaceRoundTripBudget, DecodedInterface).IsSuccess()));
		TArray<uint8> ReserializedInterface;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			DecodedInterface, ReserializedInterface).IsSuccess()));
		ASSERT_THAT(AreEqual(Hex(InterfaceBytes), Hex(ReserializedInterface)));
	}

	TEST_METHOD(DecodedSemanticFailuresRetainRecordKindAndExactNestedFieldOffset)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		auto ReadUInt32 = [](const TArray<uint8>& Bytes, int32& InOutOffset)
		{
			const uint32 Value = static_cast<uint32>(Bytes[InOutOffset])
				| (static_cast<uint32>(Bytes[InOutOffset + 1]) << 8)
				| (static_cast<uint32>(Bytes[InOutOffset + 2]) << 16)
				| (static_cast<uint32>(Bytes[InOutOffset + 3]) << 24);
			InOutOffset += 4;
			return Value;
		};
		auto SkipString = [&ReadUInt32](const TArray<uint8>& Bytes, int32& InOutOffset)
		{
			InOutOffset += static_cast<int32>(ReadUInt32(Bytes, InOutOffset));
		};
		auto OverwriteHash = [](TArray<uint8>& Bytes, const int32 Offset, const FAngelscriptHash256& Hash)
		{
			check(Offset >= 0 && Offset + static_cast<int32>(sizeof(FBlake3Hash::ByteArray)) <= Bytes.Num());
			FMemory::Memcpy(Bytes.GetData() + Offset, Hash.Value.GetBytes(), sizeof(FBlake3Hash::ByteArray));
		};
		auto FindUniqueHashOffset = [](const TArray<uint8>& Bytes, const FAngelscriptHash256& Hash)
		{
			const uint8* Needle = Hash.Value.GetBytes();
			int32 MatchOffset = INDEX_NONE;
			for (int32 Offset = 0;
				Offset + static_cast<int32>(sizeof(FBlake3Hash::ByteArray)) <= Bytes.Num();
				++Offset)
			{
				if (FMemory::Memcmp(Bytes.GetData() + Offset, Needle,
					sizeof(FBlake3Hash::ByteArray)) == 0)
				{
					check(MatchOffset == INDEX_NONE);
					MatchOffset = Offset;
				}
			}
			check(MatchOffset != INDEX_NONE);
			return MatchOffset;
		};
		auto FindDeclarationsFieldOffset = [&ReadUInt32, &SkipString](const TArray<uint8>& Bytes)
		{
			int32 Offset = 4 + 32;
			SkipString(Bytes, Offset);
			Offset += 32;
			const uint32 NamespaceCount = ReadUInt32(Bytes, Offset);
			for (uint32 Index = 0; Index < NamespaceCount; ++Index) { SkipString(Bytes, Offset); }
			return Offset;
		};

		TArray<uint8> SourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			MakeSourceIndex(), SourceBytes).IsSuccess()));
		int32 SourceOffset = 4 + 32 + 4 + 4;
		const uint32 PolicyOptionCount = ReadUInt32(SourceBytes, SourceOffset);
		for (uint32 Index = 0; Index < PolicyOptionCount; ++Index)
		{
			SkipString(SourceBytes, SourceOffset);
			SourceOffset += 32;
		}
		const uint64 MountKeyOffset = static_cast<uint64>(SourceOffset + 4);
		SourceBytes[MountKeyOffset] ^= 0x01;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget MountHashBudget;
		FAngelscriptCachedSourceIndex SourceOutput = MakeSourceIndex();
		FAngelscriptCacheValidationResult Result = DecodeSourceIndexForTests(
			SourceBytes, Limits, MountHashBudget, SourceOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
		ASSERT_THAT(AreEqual(MountKeyOffset, Result.ByteOffset));
		ASSERT_THAT(AreEqual(0u, SourceOutput.PayloadSchemaVersion));

		const FString IneligibleDiagnostic = TEXT("OffsetProbe");
		FAngelscriptCachedSourceIndex GraphSource = MakeSourceIndex();
		GraphSource.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Provider,
			GraphSource.Providers[0].ProviderKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			IneligibleDiagnostic));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(GraphSource).IsSuccess()));
		TArray<uint8> GraphSourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			GraphSource, GraphSourceBytes).IsSuccess()));
		const int32 IneligibleFieldOffset = GraphSourceBytes.Num()
			- (4 + 1 + 32 + 1 + 4 + IneligibleDiagnostic.Len() + 1);
		const int32 IneligibleKeyOffset = IneligibleFieldOffset + 4 + 1;
		TArray<uint8> MissingGraphBytes = GraphSourceBytes;
		OverwriteHash(MissingGraphBytes, IneligibleKeyOffset, MakeHash(0xfa));
		FAngelscriptCacheReadBudget MissingGraphBudget;
		Result = DecodeSourceIndexForTests(
			MissingGraphBytes, Limits, MissingGraphBudget, SourceOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
		ASSERT_THAT(AreEqual(static_cast<uint64>(IneligibleFieldOffset), Result.ByteOffset));
		TArray<uint8> WrongGraphBytes = GraphSourceBytes;
		OverwriteHash(WrongGraphBytes, IneligibleKeyOffset, GraphSource.Mounts[0].MountKey.Hash);
		FAngelscriptCacheReadBudget WrongGraphBudget;
		Result = DecodeSourceIndexForTests(
			WrongGraphBytes, Limits, WrongGraphBudget, SourceOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
		ASSERT_THAT(AreEqual(static_cast<uint64>(IneligibleFieldOffset), Result.ByteOffset));

		TArray<uint8> InterfaceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			MakeModuleInterface(), InterfaceBytes).IsSuccess()));
		const int32 InterfaceOffset = FindDeclarationsFieldOffset(InterfaceBytes);
		const uint64 DeclarationStableKeyOffset = static_cast<uint64>(
			InterfaceOffset + 4 + 4);
		InterfaceBytes[DeclarationStableKeyOffset] ^= 0x01;
		FAngelscriptCacheReadBudget InterfaceHashBudget;
		FAngelscriptCachedModuleInterface InterfaceOutput = MakeModuleInterface();
		Result = DecodeModuleInterfaceForTests(
			InterfaceBytes, Limits, InterfaceHashBudget, InterfaceOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface, Result.RecordKind));
		ASSERT_THAT(AreEqual(DeclarationStableKeyOffset, Result.ByteOffset));
		ASSERT_THAT(AreEqual(0u, InterfaceOutput.PayloadSchemaVersion));

		TArray<uint8> CrossOwnerBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			MakeModuleInterface(), CrossOwnerBytes).IsSuccess()));
		const uint64 CrossDeclarationsFieldOffset = static_cast<uint64>(
			FindDeclarationsFieldOffset(CrossOwnerBytes));
		OverwriteHash(CrossOwnerBytes, 4, MakeSecondaryModuleKey().Hash);
		FAngelscriptCacheReadBudget CrossOwnerBudget;
		Result = DecodeModuleInterfaceForTests(
			CrossOwnerBytes, Limits, CrossOwnerBudget, InterfaceOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::CrossModuleOwner, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface, Result.RecordKind));
		ASSERT_THAT(AreEqual(CrossDeclarationsFieldOffset, Result.ByteOffset));

		const FAngelscriptStableModuleKey ModuleKey = MakeModuleKey();
		FAngelscriptCachedModuleInterface NonCanonicalInterface = MakeDeclarationInterface({
			MakeTypeDeclaration(ModuleKey, EAngelscriptArtifactEntityKind::Class, TEXT("Alpha"), 0),
			MakeTypeDeclaration(ModuleKey, EAngelscriptArtifactEntityKind::Struct, TEXT("Beta"), 1)});
		NonCanonicalInterface.Declarations.Sort([](const auto& A, const auto& B)
		{
			return A.StableKey < B.StableKey;
		});
		Algo::Reverse(NonCanonicalInterface.Declarations);
		TArray<uint8> NonCanonicalBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterfacePreservingOrderForTests(
			NonCanonicalInterface, NonCanonicalBytes).IsSuccess()));
		const uint64 NonCanonicalSecondDeclarationOffset = static_cast<uint64>(
			FindUniqueHashOffset(
				NonCanonicalBytes,
				NonCanonicalInterface.Declarations[1].StableKey) - 4);
		OverwriteHash(NonCanonicalBytes, 4, MakeSecondaryModuleKey().Hash);
		FAngelscriptCacheReadBudget NonCanonicalBudget;
		Result = DecodeModuleInterfaceForTests(
			NonCanonicalBytes, Limits, NonCanonicalBudget, InterfaceOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::NonCanonicalOrder, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface, Result.RecordKind));
		ASSERT_THAT(AreEqual(NonCanonicalSecondDeclarationOffset, Result.ByteOffset));
	}

	TEST_METHOD(NestedSemanticAndPhysicalFailuresUseExactCapturedFieldOffsets)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		auto ReadUInt32 = [](const TArray<uint8>& Bytes, int32& InOutOffset)
		{
			const uint32 Value = static_cast<uint32>(Bytes[InOutOffset])
				| (static_cast<uint32>(Bytes[InOutOffset + 1]) << 8)
				| (static_cast<uint32>(Bytes[InOutOffset + 2]) << 16)
				| (static_cast<uint32>(Bytes[InOutOffset + 3]) << 24);
			InOutOffset += 4;
			return Value;
		};
		auto SkipString = [&ReadUInt32](const TArray<uint8>& Bytes, int32& InOutOffset)
		{
			InOutOffset += static_cast<int32>(ReadUInt32(Bytes, InOutOffset));
		};
		auto FindUniqueHashOffset = [](const TArray<uint8>& Bytes, const FAngelscriptHash256& Hash)
		{
			const uint8* Needle = Hash.Value.GetBytes();
			int32 MatchOffset = INDEX_NONE;
			for (int32 Offset = 0;
				Offset + static_cast<int32>(sizeof(FBlake3Hash::ByteArray)) <= Bytes.Num();
				++Offset)
			{
				if (FMemory::Memcmp(Bytes.GetData() + Offset, Needle,
					sizeof(FBlake3Hash::ByteArray)) == 0)
				{
					check(MatchOffset == INDEX_NONE);
					MatchOffset = Offset;
				}
			}
			check(MatchOffset != INDEX_NONE);
			return MatchOffset;
		};
		auto OverwriteWithZeroHash = [](TArray<uint8>& Bytes, const int32 Offset)
		{
			check(Offset >= 0
				&& Offset + static_cast<int32>(sizeof(FBlake3Hash::ByteArray)) <= Bytes.Num());
			FMemory::Memzero(Bytes.GetData() + Offset, sizeof(FBlake3Hash::ByteArray));
		};
		auto ExpectInterfaceDecodeFailure = [](FAutomationTestBase& Test,
			const TArray<uint8>& Bytes,
			const EAngelscriptCacheValidationError ExpectedError,
			const uint64 ExpectedOffset)
		{
			FNoDiscardAsserter LocalAssert(Test);
			FAngelscriptCacheReadLimits Limits;
			FAngelscriptCacheReadBudget Budget;
			FAngelscriptCachedModuleInterface Output;
			Output.PayloadSchemaVersion = 77;
			const FAngelscriptCacheValidationResult Result =
				DecodeModuleInterfaceForTests(
					Bytes, Limits, Budget, Output);
			bool bPassed = LocalAssert.AreEqual(ExpectedError, Result.Error);
			bPassed &= LocalAssert.AreEqual(
				EAngelscriptCacheRecordKind::ModuleInterface, Result.RecordKind);
			bPassed &= LocalAssert.AreEqual(ExpectedOffset, Result.ByteOffset);
			bPassed &= LocalAssert.AreEqual(0u, Output.PayloadSchemaVersion,
				TEXT("A failed nested semantic read must reset the complete output record"));
			return bPassed;
		};

		const FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		TArray<uint8> CanonicalBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			Interface, CanonicalBytes).IsSuccess()));

		int32 Offset = 4 + 32;
		SkipString(CanonicalBytes, Offset);
		Offset += 32;
		const uint32 NamespaceCount = ReadUInt32(CanonicalBytes, Offset);
		for (uint32 Index = 0; Index < NamespaceCount; ++Index)
		{
			SkipString(CanonicalBytes, Offset);
		}
		ASSERT_THAT(AreEqual(1u, ReadUInt32(CanonicalBytes, Offset)));
		Offset += 4 + 32 + 1 + 32 + 32;
		SkipString(CanonicalBytes, Offset);
		SkipString(CanonicalBytes, Offset);
		SkipString(CanonicalBytes, Offset);
		const uint32 IdentityTraitCount = ReadUInt32(CanonicalBytes, Offset);
		for (uint32 Index = 0; Index < IdentityTraitCount; ++Index)
		{
			SkipString(CanonicalBytes, Offset);
		}
		const uint8 TypeSpellingTag = CanonicalBytes[Offset++];
		ASSERT_THAT(IsTrue(TypeSpellingTag <= 1));
		if (TypeSpellingTag == 1)
		{
			SkipString(CanonicalBytes, Offset);
		}
		ASSERT_THAT(AreEqual(static_cast<uint8>(1), CanonicalBytes[Offset++]));
		const int32 DeclaredTypeOffset = Offset;
		const int32 DeclaredTypeOptionalTagOffset = DeclaredTypeOffset + 2;
		const int32 DeclaredTypeQualifierFlagsOffset = DeclaredTypeOffset + 3;
		Offset += 11;
		ASSERT_THAT(AreEqual(1u, ReadUInt32(CanonicalBytes, Offset)));
		Offset += 4;
		SkipString(CanonicalBytes, Offset);
		Offset += 11;
		Offset += 1;
		const uint8 DefaultExpressionTag = CanonicalBytes[Offset++];
		ASSERT_THAT(IsTrue(DefaultExpressionTag <= 1));
		if (DefaultExpressionTag == 1)
		{
			SkipString(CanonicalBytes, Offset);
		}
		const int32 ParameterTraitFlagsOffset = Offset;

		TArray<uint8> InvalidDeclaredType = CanonicalBytes;
		InvalidDeclaredType[DeclaredTypeQualifierFlagsOffset + 3] |= 0x80;
		ASSERT_THAT(IsTrue(ExpectInterfaceDecodeFailure(*TestRunner, InvalidDeclaredType,
			EAngelscriptCacheValidationError::UnknownFlags,
			static_cast<uint64>(DeclaredTypeQualifierFlagsOffset))));

		TArray<uint8> InvalidParameter = CanonicalBytes;
		InvalidParameter[ParameterTraitFlagsOffset + 3] |= 0x80;
		ASSERT_THAT(IsTrue(ExpectInterfaceDecodeFailure(*TestRunner, InvalidParameter,
			EAngelscriptCacheValidationError::UnknownFlags,
			static_cast<uint64>(ParameterTraitFlagsOffset))));

		TArray<uint8> InvalidPhysicalTag = CanonicalBytes;
		InvalidPhysicalTag[DeclaredTypeOptionalTagOffset] = 2;
		ASSERT_THAT(IsTrue(ExpectInterfaceDecodeFailure(*TestRunner, InvalidPhysicalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag,
			static_cast<uint64>(DeclaredTypeOptionalTagOffset))));

		const int32 ImportExpectedAbiOffset = FindUniqueHashOffset(
			CanonicalBytes, Interface.Imports[0].TargetDeclaration.ExpectedAbi);
		TArray<uint8> InvalidImport = CanonicalBytes;
		OverwriteWithZeroHash(InvalidImport, ImportExpectedAbiOffset);
		ASSERT_THAT(IsTrue(ExpectInterfaceDecodeFailure(*TestRunner, InvalidImport,
			EAngelscriptCacheValidationError::MissingExpectedAbi,
			static_cast<uint64>(ImportExpectedAbiOffset))));

		const int32 FirstDependencyStableKeyOffset = FindUniqueHashOffset(
			CanonicalBytes, Interface.Dependencies[0].Target.StableKey);
		const int32 FirstDependencyKindOffset = FirstDependencyStableKeyOffset - 2;
		const int32 FirstDependencyPresenceOffset =
			FirstDependencyStableKeyOffset
			+ 2 * static_cast<int32>(sizeof(FBlake3Hash::ByteArray));
		TArray<uint8> InvalidDependencyPresence = CanonicalBytes;
		InvalidDependencyPresence[FirstDependencyKindOffset] = static_cast<uint8>(
			EAngelscriptCacheSemanticDependencyKind::HardValue);
		ASSERT_THAT(IsTrue(ExpectInterfaceDecodeFailure(*TestRunner, InvalidDependencyPresence,
			EAngelscriptCacheValidationError::InvalidPresence,
			static_cast<uint64>(FirstDependencyPresenceOffset))));

		const int32 FirstDependencyExpectedAbiOffset = FindUniqueHashOffset(
			CanonicalBytes, Interface.Dependencies[0].Target.ExpectedAbi);
		TArray<uint8> InvalidDependencyAbi = CanonicalBytes;
		OverwriteWithZeroHash(InvalidDependencyAbi, FirstDependencyExpectedAbiOffset);
		ASSERT_THAT(IsTrue(ExpectInterfaceDecodeFailure(*TestRunner, InvalidDependencyAbi,
			EAngelscriptCacheValidationError::MissingExpectedAbi,
			static_cast<uint64>(FirstDependencyExpectedAbiOffset))));

		const int32 ExpectedContentOffset = FindUniqueHashOffset(
			CanonicalBytes, Interface.Dependencies[1].ExpectedContentOrValue.GetValue());
		TArray<uint8> InvalidDependency = CanonicalBytes;
		OverwriteWithZeroHash(InvalidDependency, ExpectedContentOffset);
		ASSERT_THAT(IsTrue(ExpectInterfaceDecodeFailure(*TestRunner, InvalidDependency,
			EAngelscriptCacheValidationError::ZeroStableKey,
			static_cast<uint64>(ExpectedContentOffset))));
	}

	TEST_METHOD(SourceWirePhasePrecedenceAndAuthorityContentAreFrozen)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		TArray<uint8> Bytes;

		FAngelscriptCachedSourceIndex MountBeforeProvider = MakeSourceIndex();
		MountBeforeProvider.Mounts[0].ProviderKey = {};
		MountBeforeProvider.Providers[0].ProviderKind = EAngelscriptCachedSourceProviderKind::Invalid;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(MountBeforeProvider, Bytes).Error));

		FAngelscriptCachedSourceIndex MountBeforeIneligible = MakeSourceIndex();
		MountBeforeIneligible.Mounts[0].ProviderKey = {};
		MountBeforeIneligible.IneligibleScopes.Add({});
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ZeroStableKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(MountBeforeIneligible, Bytes).Error));

		FAngelscriptCachedSourceIndex ProviderBeforeHook = MakeSourceIndex();
		ProviderBeforeHook.Providers[0].CapabilityFlags &= ~static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint);
		ProviderBeforeHook.Providers[0].VersionFingerprint.Reset();
		ProviderBeforeHook.PreprocessHooks[0].Phase = EAngelscriptCachedPreprocessHookPhase::Invalid;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ProviderBeforeHook, Bytes).Error));

		FAngelscriptCachedSourceIndex ProviderBeforeIneligible = MakeSourceIndex();
		ProviderBeforeIneligible.Providers[0].CapabilityFlags &= ~static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint);
		ProviderBeforeIneligible.Providers[0].VersionFingerprint.Reset();
		ProviderBeforeIneligible.IneligibleScopes.Add({});
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ProviderBeforeIneligible, Bytes).Error));

		FAngelscriptCachedSourceIndex TypedReferenceBeforeResolvedConflict = MakeSourceIndex();
		TypedReferenceBeforeResolvedConflict.Files[0].SourceKind = EAngelscriptCachedSourceKind::Plugin;
		TypedReferenceBeforeResolvedConflict.Files[0].SourceFileKey =
			BuildSourceFileKey(TypedReferenceBeforeResolvedConflict.Files[0]);
		TypedReferenceBeforeResolvedConflict.PreprocessHooks[0].AffectedScopeStableKey = MakeHash(0xf1);
		TypedReferenceBeforeResolvedConflict.PreprocessHooks[0].HookKey =
			BuildHookKey(TypedReferenceBeforeResolvedConflict.PreprocessHooks[0]);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				TypedReferenceBeforeResolvedConflict, Bytes).Error));

		FAngelscriptCachedSourceIndex MountOptionsConflict = MakeSourceIndex();
		FAngelscriptCachedSourceMount SecondMount = MountOptionsConflict.Mounts[0];
		SecondMount.Options[0].ValueFingerprint = MakeHash(0xf2);
		MountOptionsConflict.Mounts.Add(SecondMount);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(MountOptionsConflict, Bytes).Error));

		FAngelscriptCachedSourceIndex ExactGeneratedDuplicate = MakeGeneratedSourceIndex();
		const FAngelscriptCachedSourceFile ExactGeneratedFile = ExactGeneratedDuplicate.Files[0];
		ExactGeneratedDuplicate.Files.Add(ExactGeneratedFile);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DuplicateKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ExactGeneratedDuplicate, Bytes).Error));

		FAngelscriptCachedSourceIndex GeneratedConfigurationConflict = MakeGeneratedSourceIndex();
		FAngelscriptCachedSourceFile ConfigurationVariant = GeneratedConfigurationConflict.Files[0];
		ConfigurationVariant.GeneratedConfigurationFingerprint = MakeHash(0xf3);
		GeneratedConfigurationConflict.Files.Add(ConfigurationVariant);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				GeneratedConfigurationConflict, Bytes).Error));

		FAngelscriptCachedSourceIndex HookBeforeFile = MakeSourceIndex();
		HookBeforeFile.PreprocessHooks[0].AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Mount;
		HookBeforeFile.PreprocessHooks[0].AffectedScopeStableKey =
			HookBeforeFile.Providers[0].ProviderKey.Hash;
		HookBeforeFile.PreprocessHooks[0].HookKey = BuildHookKey(HookBeforeFile.PreprocessHooks[0]);
		HookBeforeFile.Files[0].MountKey.Hash = MakeHash(0xf4);
		HookBeforeFile.Files[0].SourceFileKey = BuildSourceFileKey(HookBeforeFile.Files[0]);
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(HookBeforeFile, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
		ASSERT_THAT(AreEqual(UINT64_C(0), Result.ByteOffset));

		FAngelscriptCachedSourceIndex MountBeforeHook = MakeSourceIndex();
		MountBeforeHook.Mounts[0].ProviderKey.Hash = MakeHash(0xe1);
		MountBeforeHook.Mounts[0].MountKey = BuildMountKey(MountBeforeHook.Mounts[0]);
		MountBeforeHook.PreprocessHooks[0].AffectedScopeKind =
			EAngelscriptCachedFastPathScopeKind::Mount;
		MountBeforeHook.PreprocessHooks[0].AffectedScopeStableKey =
			MountBeforeHook.Providers[0].ProviderKey.Hash;
		MountBeforeHook.PreprocessHooks[0].HookKey =
			BuildHookKey(MountBeforeHook.PreprocessHooks[0]);
		Result = FAngelscriptCacheSemanticArchive::SerializeSourceIndex(MountBeforeHook, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget, Result.Error));

		FAngelscriptCachedSourceIndex FileBeforeInput = MakeSourceIndex();
		FileBeforeInput.Files[0].MountKey.Hash = MakeHash(0xe2);
		FileBeforeInput.Files[0].SourceFileKey = BuildSourceFileKey(FileBeforeInput.Files[0]);
		FileBeforeInput.PreprocessorInputs[0].OwnerScopeKind =
			EAngelscriptCachedFastPathScopeKind::Mount;
		FileBeforeInput.PreprocessorInputs[0].OwnerScopeStableKey =
			FileBeforeInput.Providers[0].ProviderKey.Hash;
		FileBeforeInput.PreprocessorInputs[0].InputKey =
			BuildInputKey(FileBeforeInput.PreprocessorInputs[0]);
		Result = FAngelscriptCacheSemanticArchive::SerializeSourceIndex(FileBeforeInput, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget, Result.Error));

		FAngelscriptCachedSourceIndex InputBeforeEdge = MakeSourceIndex();
		InputBeforeEdge.PreprocessorInputs[0].OwnerScopeStableKey = MakeHash(0xe3);
		InputBeforeEdge.PreprocessorInputs[0].InputKey =
			BuildInputKey(InputBeforeEdge.PreprocessorInputs[0]);
		InputBeforeEdge.Edges[0].FromSourceFileKey.Hash =
			InputBeforeEdge.Providers[0].ProviderKey.Hash;
		InputBeforeEdge.Edges[0].EdgeKey = BuildEdgeKey(InputBeforeEdge.Edges[0]);
		Result = FAngelscriptCacheSemanticArchive::SerializeSourceIndex(InputBeforeEdge, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget, Result.Error));

		FAngelscriptCachedSourceIndex InputBeforeIneligible = MakeSourceIndex();
		InputBeforeIneligible.PreprocessorInputs[0].OwnerScopeKind =
			EAngelscriptCachedFastPathScopeKind::Mount;
		InputBeforeIneligible.PreprocessorInputs[0].OwnerScopeStableKey =
			InputBeforeIneligible.Providers[0].ProviderKey.Hash;
		InputBeforeIneligible.PreprocessorInputs[0].InputKey =
			BuildInputKey(InputBeforeIneligible.PreprocessorInputs[0]);
		InputBeforeIneligible.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::SourceFile, MakeHash(0xf5),
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("Later missing ineligible")));
		Result = FAngelscriptCacheSemanticArchive::SerializeSourceIndex(InputBeforeIneligible, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));

		FAngelscriptCachedSourceIndex EdgeBeforeIneligible = MakeSourceIndex();
		EdgeBeforeIneligible.Edges[0].FromSourceFileKey.Hash =
			EdgeBeforeIneligible.Providers[0].ProviderKey.Hash;
		EdgeBeforeIneligible.Edges[0].EdgeKey = BuildEdgeKey(EdgeBeforeIneligible.Edges[0]);
		EdgeBeforeIneligible.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::SourceFile, MakeHash(0xf6),
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("Later missing ineligible")));
		Result = FAngelscriptCacheSemanticArchive::SerializeSourceIndex(EdgeBeforeIneligible, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongReferenceKind, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
	}

	TEST_METHOD(ModuleOwnerValidationRunsAfterLocalShapeHashOrderAndDuplicates)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		TArray<uint8> Bytes;

		FAngelscriptCachedModuleInterface CoverageBeforeOwner = MakeModuleInterface();
		CoverageBeforeOwner.Declarations[0].SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		CoverageBeforeOwner.Declarations[0].ModuleKey = MakeSecondaryModuleKey();
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(CoverageBeforeOwner, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface, Result.RecordKind));
		ASSERT_THAT(AreEqual(UINT64_C(0), Result.ByteOffset));

		FAngelscriptCachedModuleInterface HashBeforeOwner = MakeModuleInterface();
		HashBeforeOwner.Declarations[0].StableKey = MakeHash(0xf4);
		HashBeforeOwner.Declarations[0].ModuleKey = MakeSecondaryModuleKey();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(HashBeforeOwner, Bytes).Error));

		FAngelscriptCachedModuleInterface DuplicateBeforeOwner = MakeModuleInterface();
		const FAngelscriptCachedDeclaration ExactDeclaration = DuplicateBeforeOwner.Declarations[0];
		DuplicateBeforeOwner.Declarations.Add(ExactDeclaration);
		DuplicateBeforeOwner.ModuleKey = MakeSecondaryModuleKey();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DuplicateKey,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(DuplicateBeforeOwner, Bytes).Error));

		FAngelscriptCachedModuleInterface ConflictBeforeOwner = MakeModuleInterface();
		FAngelscriptCachedDeclaration ConflictingDeclaration = ConflictBeforeOwner.Declarations[0];
		ConflictingDeclaration.Slots[0].Ordinal = 1;
		ConflictBeforeOwner.Declarations.Add(ConflictingDeclaration);
		ConflictBeforeOwner.ModuleKey = MakeSecondaryModuleKey();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(ConflictBeforeOwner, Bytes).Error));

		FAngelscriptCachedModuleInterface SignatureHashBeforeOwner = MakeModuleInterface();
		SignatureHashBeforeOwner.Declarations[0].SignatureHash = MakeHash(0xf5);
		SignatureHashBeforeOwner.ModuleKey = MakeSecondaryModuleKey();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				SignatureHashBeforeOwner, Bytes).Error));

		FAngelscriptCachedModuleInterface TraitsHashBeforeOwner = MakeModuleInterface();
		TraitsHashBeforeOwner.Declarations[0].TraitsHash = MakeHash(0xf6);
		TraitsHashBeforeOwner.ModuleKey = MakeSecondaryModuleKey();
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				TraitsHashBeforeOwner, Bytes).Error));
	}

	TEST_METHOD(SourcePathsCaseAndFingerprintPresenceFailClosed)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		const TArray<FString> InvalidPaths = {
			TEXT(""), TEXT("C:/Scripts/Hero.as"), TEXT("\\\\server\\share\\Hero.as"),
			TEXT("/Scripts/Hero.as"), TEXT("../../Hero.as")};
		for (const FString& Path : InvalidPaths)
		{
			FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
			Source.Files[0].RelativeLogicalPath = Path;
			TArray<uint8> Bytes = {0xde};
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidLogicalPath, Result.Error));
			ASSERT_THAT(IsTrue(Bytes.IsEmpty()));
		}

		FAngelscriptCachedSourceIndex EmbeddedNul = MakeSourceIndex();
		EmbeddedNul.Files[0].RelativeLogicalPath = TEXT("Actors/Hero.as");
		EmbeddedNul.Files[0].RelativeLogicalPath.GetCharArray().Insert(TEXT('\0'), 7);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::EmbeddedNul,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(EmbeddedNul, Bytes).Error));

		FAngelscriptCachedSourceIndex Collision = MakeSourceIndex();
		FAngelscriptCachedSourceFile Second = Collision.Files[0];
		Second.RelativeLogicalPath = TEXT("actors/hero.AS");
		Second.SourceFileKey = BuildSourceFileKey(Second);
		Collision.Files.Add(Second);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::CaseCollision,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Collision, Bytes).Error));

		FAngelscriptCachedSourceIndex UnicodeCollision = MakeSourceIndex();
		UnicodeCollision.Files[0].RelativeLogicalPath = TEXT("Actors/\u00c5.as");
		UnicodeCollision.Files[0].SourceFileKey = BuildSourceFileKey(UnicodeCollision.Files[0]);
		Second = UnicodeCollision.Files[0];
		Second.RelativeLogicalPath = TEXT("Actors/\u00e5.as");
		Second.SourceFileKey = BuildSourceFileKey(Second);
		UnicodeCollision.Files.Add(Second);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::CaseCollision,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(UnicodeCollision, Bytes).Error));

		FAngelscriptCachedSourceIndex SupplementaryCollision = MakeSourceIndex();
		SupplementaryCollision.Files[0].RelativeLogicalPath = TEXT("Actors/\U00010400.as");
		SupplementaryCollision.Files[0].SourceFileKey =
			BuildSourceFileKey(SupplementaryCollision.Files[0]);
		Second = SupplementaryCollision.Files[0];
		Second.RelativeLogicalPath = TEXT("Actors/\U00010428.as");
		Second.SourceFileKey = BuildSourceFileKey(Second);
		SupplementaryCollision.Files.Add(Second);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::CaseCollision,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				SupplementaryCollision, Bytes).Error));

		FAngelscriptCachedSourceIndex BadPresence = MakeSourceIndex();
		BadPresence.Providers[0].CapabilityFlags &= ~static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(BadPresence, Bytes).Error));
	}

	TEST_METHOD(DerivedHashesStableKeysCoverageAndOrdinalsAreValidated)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		TArray<uint8> Bytes;
		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		Source.SourceSnapshot = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes).Error));

		FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		Interface.InterfaceAbi = MakeHash(0xee);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		Interface.Declarations[0].StableKey = MakeHash(0xef);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		Interface.Declarations[0].SignatureHash = MakeHash(0xef);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		Interface.Declarations[0].SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		Interface.Declarations[0].BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		Interface.Declarations[0].OrderedParameters[0].Ordinal = 1;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::OrdinalGap,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		Interface.Declarations[0].Slots[0].Ordinal = 1;
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::OrdinalGap,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		Interface.Declarations[0].Slots.Add(
			{EAngelscriptCacheDeclarationSlotKind::Function, 0});
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DuplicateOrdinal,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));
	}

	TEST_METHOD(DuplicateConflictAndNonCanonicalWireOrderHaveExactErrors)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		TArray<uint8> Bytes;
		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		const FAngelscriptCachedCanonicalOption DuplicateOption = Source.DiscoveryPolicy.Options[0];
		Source.DiscoveryPolicy.Options.Add(DuplicateOption);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DuplicateKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes).Error));

		Source = MakeSourceIndex();
		FAngelscriptCachedCanonicalOption Conflict = Source.DiscoveryPolicy.Options[0];
		Conflict.ValueFingerprint = MakeHash(0xfe);
		Source.DiscoveryPolicy.Options.Add(Conflict);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, Bytes).Error));

		FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		const FAngelscriptCachedMetadataEntry DuplicateMetadata = Interface.Declarations[0].Metadata[0];
		Interface.Declarations[0].Metadata.Add(DuplicateMetadata);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DuplicateKey,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Interface = MakeModuleInterface();
		FAngelscriptCachedMetadataEntry MetadataConflict = Interface.Declarations[0].Metadata[0];
		MetadataConflict.CanonicalValue = TEXT("Other");
		Interface.Declarations[0].Metadata.Add(MetadataConflict);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey,
			FAngelscriptCacheSemanticArchive::SerializeModuleInterface(Interface, Bytes).Error));

		Source = MakeSourceIndex();
		Algo::Reverse(Source.DiscoveryPolicy.Options);
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndexPreservingOrderForTests(
			Source, Bytes).IsSuccess()));
		ASSERT_THAT(IsTrue(ExpectDecodeFailureAndReset<FAngelscriptCachedSourceIndex>(
			*TestRunner, Bytes, EAngelscriptCacheValidationError::NonCanonicalOrder,
			[](const TArray<uint8>& In, const FAngelscriptCacheReadLimits& Limits,
				FAngelscriptCacheReadBudget& Budget, FAngelscriptCachedSourceIndex& Out)
			{
				return DecodeSourceIndexForTests(In, Limits, Budget, Out);
			})));

		Interface = MakeModuleInterface();
		Algo::Reverse(Interface.Declarations[0].Metadata);
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterfacePreservingOrderForTests(
			Interface, Bytes).IsSuccess()));
		ASSERT_THAT(IsTrue(ExpectDecodeFailureAndReset<FAngelscriptCachedModuleInterface>(
			*TestRunner, Bytes, EAngelscriptCacheValidationError::NonCanonicalOrder,
			[](const TArray<uint8>& In, const FAngelscriptCacheReadLimits& Limits,
				FAngelscriptCacheReadBudget& Budget, FAngelscriptCachedModuleInterface& Out)
			{
				return DecodeModuleInterfaceForTests(In, Limits, Budget, Out);
			})));
	}

	TEST_METHOD(CompetingFullHashDuplicateGroupsChooseSmallestSecondWireOccurrence)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		const FAngelscriptCachedSourceIndex Baseline = MakeSourceIndex();
		const FAngelscriptCachedSourceProvider ProviderA = Baseline.Providers[0];
		FAngelscriptCachedSourceProvider ProviderB = ProviderA;
		ProviderB.CanonicalImplementationIdentity = TEXT("Runtime.CompetingSourceProvider");
		ProviderB.IdentityFingerprint = MakeHash(0xd0);
		ProviderB.ProviderKey = BuildProviderKey(ProviderB);
		ASSERT_THAT(IsTrue(ProviderA.ProviderKey.Hash != ProviderB.ProviderKey.Hash));

		const bool bAIsSortedFirst = ProviderA.ProviderKey.Hash < ProviderB.ProviderKey.Hash;
		const FAngelscriptCachedSourceProvider& SortedFirst = bAIsSortedFirst ? ProviderA : ProviderB;
		const FAngelscriptCachedSourceProvider& SortedSecond = bAIsSortedFirst ? ProviderB : ProviderA;

		FAngelscriptCachedSourceProvider FirstConflict = SortedFirst;
		FirstConflict.VersionFingerprint = MakeHash(0xd1);
		FAngelscriptCachedSourceProvider SecondConflict = SortedSecond;
		SecondConflict.VersionFingerprint = MakeHash(0xd2);

		// The lower full key's group is encountered first by the temporary sorted index,
		// but the higher full key reaches its second wire occurrence first.  The wire-first
		// group is conflicting, so sorted-key order must not incorrectly report DuplicateKey.
		FAngelscriptCachedSourceIndex ConflictWins = Baseline;
		ConflictWins.Providers = {SortedFirst, SortedSecond, SecondConflict, SortedFirst};
		TArray<uint8> Bytes;
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::SerializeSourceIndex(ConflictWins, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ConflictingKey, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
		ASSERT_THAT(AreEqual(UINT64_C(0), Result.ByteOffset));

		// Swap only the duplicate/conflict content classification.  The same second-wire
		// indices now require DuplicateKey even though the sorted-first group conflicts later.
		FAngelscriptCachedSourceIndex DuplicateWins = Baseline;
		DuplicateWins.Providers = {SortedFirst, SortedSecond, SortedSecond, FirstConflict};
		Result = FAngelscriptCacheSemanticArchive::SerializeSourceIndex(DuplicateWins, Bytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DuplicateKey, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
		ASSERT_THAT(AreEqual(UINT64_C(0), Result.ByteOffset));
	}

	TEST_METHOD(RecordReadersApplyFieldRecordArrayAndCumulativeBudgetsAndResetOutputs)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		const FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		TArray<uint8> SourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(Source, SourceBytes).IsSuccess()));

		FAngelscriptCacheReadLimits Limits;
		Limits.MaxCanonicalRecordPayloadBytes = static_cast<uint64>(SourceBytes.Num() - 1);
		FAngelscriptCacheReadBudget PayloadLimitBudget;
		FAngelscriptCachedSourceIndex DecodedSource;
		DecodedSource.PayloadSchemaVersion = 77;
		FAngelscriptCacheValidationResult Result = DecodeSourceIndexForTests(
			SourceBytes, Limits, PayloadLimitBudget, DecodedSource);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(0u, DecodedSource.PayloadSchemaVersion));

		Limits = {};
		Limits.MaxArrayElements = 0;
		FAngelscriptCacheReadBudget ArrayLimitBudget;
		Result = DecodeSourceIndexForTests(
			SourceBytes, Limits, ArrayLimitBudget, DecodedSource);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));

		const FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		TArray<uint8> InterfaceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			Interface, InterfaceBytes).IsSuccess()));
		Limits = {};
		Limits.MaxStringBytes = 3;
		FAngelscriptCacheReadBudget StringLimitBudget;
		FAngelscriptCachedModuleInterface DecodedInterface;
		DecodedInterface.PayloadSchemaVersion = 77;
		Result = DecodeModuleInterfaceForTests(
			InterfaceBytes, Limits, StringLimitBudget, DecodedInterface);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(0u, DecodedInterface.PayloadSchemaVersion));

		Limits = {};
		FAngelscriptCacheReadBudget SingleReadMeasureBudget;
		FAngelscriptCachedSourceIndex SingleReadMeasureOutput;
		ASSERT_THAT(IsTrue(DecodeSourceIndexForTests(
			SourceBytes, Limits, SingleReadMeasureBudget, SingleReadMeasureOutput).IsSuccess()));
		const uint64 SingleReadDecodedBytes = SingleReadMeasureBudget.GetDecodedBytes();
		ASSERT_THAT(IsTrue(SingleReadDecodedBytes > 0));
		Limits.MaxTotalDecodedBytes = SingleReadDecodedBytes;
		FAngelscriptCacheReadBudget CumulativeBudget;
		ASSERT_THAT(IsTrue(DecodeSourceIndexForTests(
			SourceBytes, Limits, CumulativeBudget, DecodedSource).IsSuccess()));
		ASSERT_THAT(AreEqual(SingleReadDecodedBytes, CumulativeBudget.GetDecodedBytes()));
		Result = DecodeSourceIndexForTests(
			SourceBytes, Limits, CumulativeBudget, DecodedSource);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(SingleReadDecodedBytes, CumulativeBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(0u, DecodedSource.PayloadSchemaVersion));

		SourceBytes.Add(0x7f);
		Limits = {};
		FAngelscriptCacheReadBudget TrailingDataBudget;
		Result = DecodeSourceIndexForTests(
			SourceBytes, Limits, TrailingDataBudget, DecodedSource);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::TrailingData, Result.Error));
		ASSERT_THAT(AreEqual(0u, DecodedSource.PayloadSchemaVersion));
	}

	TEST_METHOD(MultiFileEligibilityCapacityMatchesItsReservedScratchBudget)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		const auto RunFixture = [&](const int32 FileCount, const int32 HookCount)
		{
		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		Source.Files.Reserve(FileCount);
		for (int32 FileIndex = 1; FileIndex < FileCount; ++FileIndex)
		{
			FAngelscriptCachedSourceFile File = Source.Files[0];
			File.RelativeLogicalPath = FString::Printf(
				TEXT("Actors/Hero_%03d.as"), FileIndex);
			File.RawContentHash = MakeHash(static_cast<uint8>(0x80 + FileIndex % 0x70));
			File.SourceFileKey = BuildSourceFileKey(File);
			Source.Files.Add(MoveTemp(File));
		}
		Source.PreprocessHooks.Reset();
		Source.PreprocessHooks.Reserve(HookCount);
		for (int32 HookIndex = 0; HookIndex < HookCount; ++HookIndex)
		{
			Source.PreprocessHooks.Add(MakeHook(
				FString::Printf(TEXT("Runtime.CapacityHook.%03d"), HookIndex),
				EAngelscriptCachedFastPathScopeKind::Mount,
				Source.Mounts[0].MountKey.Hash,
				static_cast<uint8>(0x20 + HookIndex % 0x50)));
		}
		Source.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Module,
			Source.Files[0].ModuleKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("MultiFileCapacityProbe")));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));

		TArray<uint8> SourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, SourceBytes).IsSuccess()));

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget MeasureDecodeBudget;
		FDecodedSourceIndexHandleForTests MeasureToken;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			SourceBytes, Limits, MeasureDecodeBudget, MeasureToken).IsSuccess()));
		FAngelscriptCacheReadBudget MeasureQueryBudget;
		FAngelscriptCacheExactFastPathEligibility MeasureResult;
		using namespace AngelscriptCacheEligibilityTestHooks;
		FAllocationEvent AllocationEvents[2];
		int32 AllocationEventCount = 0;
		bool bAllocationEventOverflowed = false;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityWithAllocationCaptureForTests(
				MeasureToken,
				Source.Files[0].ModuleKey,
				Limits,
				MeasureQueryBudget,
				FAllocationEventCaptureView(
					MakeArrayView(AllocationEvents),
					AllocationEventCount,
					bAllocationEventOverflowed),
				MeasureResult).IsSuccess()));
		ASSERT_THAT(AreEqual(1, MeasureResult.MatchingScopes.Num()));
		ASSERT_THAT(IsFalse(bAllocationEventOverflowed));
		ASSERT_THAT(AreEqual(2, AllocationEventCount));
		const FAllocationEvent& PrimaryEvent = AllocationEvents[0];
		ASSERT_THAT(AreEqual(EAllocationPhase::PrimaryScratchArrays,
			PrimaryEvent.Phase));
		const int32 RequestedCapacity = PrimaryEvent.RequestedCapacity;
		const int32 FileCapacityBeforeAdds =
			PrimaryEvent.FirstCapacityBeforePopulation;
		const int32 MountCapacityBeforeAdds =
			PrimaryEvent.SecondCapacityBeforePopulation;
		const int32 ProviderCapacityBeforeAdds =
			PrimaryEvent.ThirdCapacityBeforePopulation;
		const int32 FileCapacityAfterAdds =
			PrimaryEvent.FirstCapacityAfterPopulation;
		const int32 MountCapacityAfterAdds =
			PrimaryEvent.SecondCapacityAfterPopulation;
		const int32 ProviderCapacityAfterAdds =
			PrimaryEvent.ThirdCapacityAfterPopulation;
		const uint64 AllocatedBytesBeforeAdds =
			PrimaryEvent.AllocatedBytesBeforePopulation;
		const uint64 AllocatedBytesAfterAdds =
			PrimaryEvent.AllocatedBytesAfterPopulation;
		ASSERT_THAT(AreEqual(FileCount, RequestedCapacity));
		const int32 ExpectedReservedCapacity =
			CalculateArrayReserveCapacityForTests<FAngelscriptHash256>(FileCount);
		ASSERT_THAT(AreEqual(ExpectedReservedCapacity, FileCapacityBeforeAdds));
		ASSERT_THAT(AreEqual(ExpectedReservedCapacity, MountCapacityBeforeAdds));
		ASSERT_THAT(AreEqual(ExpectedReservedCapacity, ProviderCapacityBeforeAdds));
		const uint64 ExpectedHashArrayBytes =
			static_cast<uint64>(ExpectedReservedCapacity)
				* 3 * sizeof(FAngelscriptHash256);
		ASSERT_THAT(AreEqual(ExpectedHashArrayBytes, AllocatedBytesBeforeAdds));
		ASSERT_THAT(AreEqual(ExpectedHashArrayBytes, AllocatedBytesAfterAdds));
		if (FileCapacityBeforeAdds < FileCount
			|| MountCapacityBeforeAdds < FileCount
			|| ProviderCapacityBeforeAdds < FileCount)
		{
			TestRunner->AddError(FString::Printf(
				TEXT("Eligibility arrays lacked Add-free capacity before population: requested=%d file=%d mount=%d provider=%d"),
				FileCount, FileCapacityBeforeAdds,
				MountCapacityBeforeAdds, ProviderCapacityBeforeAdds));
		}
		if (FileCapacityAfterAdds != FileCapacityBeforeAdds
			|| MountCapacityAfterAdds != MountCapacityBeforeAdds
			|| ProviderCapacityAfterAdds != ProviderCapacityBeforeAdds
			|| AllocatedBytesAfterAdds != AllocatedBytesBeforeAdds)
		{
			TestRunner->AddError(FString::Printf(
				TEXT("Eligibility arrays grew during Add: capacities %d/%d/%d -> %d/%d/%d, bytes %llu -> %llu"),
				FileCapacityBeforeAdds, MountCapacityBeforeAdds, ProviderCapacityBeforeAdds,
				FileCapacityAfterAdds, MountCapacityAfterAdds, ProviderCapacityAfterAdds,
				static_cast<unsigned long long>(AllocatedBytesBeforeAdds),
				static_cast<unsigned long long>(AllocatedBytesAfterAdds)));
		}

		const uint64 QueryResident = MeasureQueryBudget.GetResidentDecodedBytes();
		const uint64 QueryPeakLive = MeasureQueryBudget.GetPeakLiveResidentDecodedBytes();
		ASSERT_THAT(IsTrue(QueryPeakLive >= QueryResident));
		const uint64 QueryScratch = QueryPeakLive - QueryResident;
		ASSERT_THAT(IsTrue(QueryScratch > 0));
		const FAllocationEvent& AuxiliaryEvent = AllocationEvents[1];
		ASSERT_THAT(AreEqual(EAllocationPhase::AuxiliaryScratchArrays,
			AuxiliaryEvent.Phase));
		const int32 ProbedHookCount = AuxiliaryEvent.RequestedCapacity;
		const int32 HookKeyIndexCapacity =
			AuxiliaryEvent.FirstCapacityAfterPopulation;
		const int32 ReverseHookCapacity =
			AuxiliaryEvent.SecondCapacityAfterPopulation;
		const int32 ReachedHookCapacity =
			AuxiliaryEvent.ThirdCapacityAfterPopulation;
		const int32 HookQueueCapacity =
			AuxiliaryEvent.FourthCapacityAfterPopulation;
		const uint64 AuxiliaryAllocatedBytes =
			AuxiliaryEvent.AllocatedBytesAfterPopulation;
		ASSERT_THAT(AreEqual(
			AuxiliaryEvent.FirstCapacityBeforePopulation,
			HookKeyIndexCapacity));
		ASSERT_THAT(AreEqual(
			AuxiliaryEvent.SecondCapacityBeforePopulation,
			ReverseHookCapacity));
		ASSERT_THAT(AreEqual(
			AuxiliaryEvent.ThirdCapacityBeforePopulation,
			ReachedHookCapacity));
		ASSERT_THAT(AreEqual(
			AuxiliaryEvent.FourthCapacityBeforePopulation,
			HookQueueCapacity));
		ASSERT_THAT(AreEqual(
			AuxiliaryEvent.AllocatedBytesBeforePopulation,
			AuxiliaryAllocatedBytes));
		ASSERT_THAT(AreEqual(HookCount, ProbedHookCount));
		const int32 ExpectedIndexedHashCapacity =
			CalculateArrayReserveCapacityForTests<FIndexedStableHashScratchProbe>(HookCount);
		const int32 ExpectedReachedCapacity =
			CalculateArrayReserveCapacityForTests<uint8>(HookCount);
		const int32 ExpectedQueueCapacity =
			CalculateArrayReserveCapacityForTests<int32>(HookCount);
		const uint64 ExpectedAuxiliaryArrayBytes =
			static_cast<uint64>(ExpectedIndexedHashCapacity)
				* 2 * sizeof(FIndexedStableHashScratchProbe)
			+ static_cast<uint64>(ExpectedReachedCapacity) * sizeof(uint8)
			+ static_cast<uint64>(ExpectedQueueCapacity) * sizeof(int32);
		if (HookKeyIndexCapacity != ExpectedIndexedHashCapacity
			|| ReverseHookCapacity != ExpectedIndexedHashCapacity
			|| ReachedHookCapacity != ExpectedReachedCapacity
			|| HookQueueCapacity != ExpectedQueueCapacity
			|| AuxiliaryAllocatedBytes != ExpectedAuxiliaryArrayBytes)
		{
			TestRunner->AddError(FString::Printf(
				TEXT("Eligibility auxiliary arrays diverged from reserve authority for %d hooks: capacities=%d/%d/%d/%d expected=%d/%d/%d, bytes=%llu expected=%llu"),
				HookCount,
				HookKeyIndexCapacity, ReverseHookCapacity,
				ReachedHookCapacity, HookQueueCapacity,
				ExpectedIndexedHashCapacity,
				ExpectedReachedCapacity,
				ExpectedQueueCapacity,
				static_cast<unsigned long long>(AuxiliaryAllocatedBytes),
				static_cast<unsigned long long>(ExpectedAuxiliaryArrayBytes)));
		}
		const uint64 ExpectedQueryScratch = ExpectedHashArrayBytes
			+ ExpectedAuxiliaryArrayBytes;
		if (QueryScratch != ExpectedQueryScratch)
		{
			TestRunner->AddError(FString::Printf(
				TEXT("Eligibility scratch registered %llu bytes for %d files, but allocator-authoritative reserve capacity %d requires %llu bytes"),
				static_cast<unsigned long long>(QueryScratch),
				FileCount,
				ExpectedReservedCapacity,
				static_cast<unsigned long long>(ExpectedQueryScratch)));
		}
		ASSERT_THAT(IsTrue(QueryResident > 0));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			MeasureQueryBudget.GetTemporaryResidentDecodedBytes()));

		Limits = {};
		FAngelscriptCacheReadBudget ExactBudget;
		FDecodedSourceIndexHandleForTests ExactToken;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			SourceBytes, Limits, ExactBudget, ExactToken).IsSuccess()));
		const uint64 ExactTokenResident = ExactBudget.GetResidentDecodedBytes();
		const uint64 ExactEntryTemporary = ExactBudget.GetTemporaryResidentDecodedBytes();
		Limits.MaxResidentDecodedBytes = ExactTokenResident + QueryPeakLive;
		FAngelscriptCacheExactFastPathEligibility ExactResult;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			ExactToken, Source.Files[0].ModuleKey,
			Limits, ExactBudget, ExactResult).IsSuccess()));
		ASSERT_THAT(AreEqual(1, ExactResult.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(ExactTokenResident + QueryResident,
			ExactBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(ExactEntryTemporary,
			ExactBudget.GetTemporaryResidentDecodedBytes()));

		Limits = {};
		FAngelscriptCacheReadBudget ShortBudget;
		FDecodedSourceIndexHandleForTests ShortToken;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			SourceBytes, Limits, ShortBudget, ShortToken).IsSuccess()));
		const uint64 ShortTokenResident = ShortBudget.GetResidentDecodedBytes();
		const uint64 ShortEntryTemporary = ShortBudget.GetTemporaryResidentDecodedBytes();
		Limits.MaxResidentDecodedBytes = ShortTokenResident + QueryPeakLive - 1;
		FAngelscriptCacheExactFastPathEligibility ShortResult;
		ASSERT_THAT(AreEqual(0, ShortResult.MatchingScopes.Max()));
		FAngelscriptCacheValidationResult Result =
			QueryExactFastPathEligibilityForTests(
				ShortToken, Source.Files[0].ModuleKey,
				Limits, ShortBudget, ShortResult);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(0, ShortResult.MatchingScopes.Max()));
		ASSERT_THAT(AreEqual(ShortTokenResident, ShortBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(ShortEntryTemporary,
			ShortBudget.GetTemporaryResidentDecodedBytes()));

		Limits = {};
		FAngelscriptCacheReadBudget ScratchShortBudget;
		FDecodedSourceIndexHandleForTests ScratchShortToken;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			SourceBytes, Limits, ScratchShortBudget, ScratchShortToken).IsSuccess()));
		const uint64 ScratchShortTokenResident =
			ScratchShortBudget.GetResidentDecodedBytes();
		const uint64 ScratchShortEntryTemporary =
			ScratchShortBudget.GetTemporaryResidentDecodedBytes();
		Limits.MaxResidentDecodedBytes = ScratchShortTokenResident + QueryScratch - 1;
		FAngelscriptCacheExactFastPathEligibility ScratchShortResult;
		FAllocationEvent ScratchShortEvents[2];
		int32 ScratchShortEventCount = 0;
		bool bScratchShortEventOverflowed = false;
		Result = QueryExactFastPathEligibilityWithAllocationCaptureForTests(
				ScratchShortToken, Source.Files[0].ModuleKey,
				Limits, ScratchShortBudget,
				FAllocationEventCaptureView(
					MakeArrayView(ScratchShortEvents),
					ScratchShortEventCount,
					bScratchShortEventOverflowed),
				ScratchShortResult);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(0, ScratchShortEventCount));
		ASSERT_THAT(IsFalse(bScratchShortEventOverflowed));
		ASSERT_THAT(AreEqual(0, ScratchShortResult.MatchingScopes.Max()));
		ASSERT_THAT(AreEqual(ScratchShortTokenResident,
			ScratchShortBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(ScratchShortEntryTemporary,
			ScratchShortBudget.GetTemporaryResidentDecodedBytes()));
		};

		int32 QuantizedHookCount = INDEX_NONE;
		for (int32 Candidate = 1; Candidate <= 8192; ++Candidate)
		{
			if (CalculateArrayReserveCapacityForTests<FIndexedStableHashScratchProbe>(Candidate)
					> Candidate
				|| CalculateArrayReserveCapacityForTests<uint8>(Candidate) > Candidate
				|| CalculateArrayReserveCapacityForTests<int32>(Candidate) > Candidate)
			{
				QuantizedHookCount = Candidate;
				break;
			}
		}
		RunFixture(256, 1);
		RunFixture(103, QuantizedHookCount != INDEX_NONE ? QuantizedHookCount : 1);
	}

	TEST_METHOD(EligibilityAllocationCaptureIsCallerOwnedBoundedConcurrentAndObserverInert)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		using namespace AngelscriptCacheEligibilityTestHooks;

		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		Source.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Module,
			Source.Files[0].ModuleKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("CallerOwnedConcurrentCapture")));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));
		const FAngelscriptStableModuleKey ModuleKey = Source.Files[0].ModuleKey;
		TArray<uint8> SourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, SourceBytes).IsSuccess()));

		struct FQuerySummary
		{
			EAngelscriptCacheValidationError Error =
				EAngelscriptCacheValidationError::InvalidPresence;
			EAngelscriptCacheValidationClass Class =
				EAngelscriptCacheValidationClass::Malformed;
			EAngelscriptCacheValidationStage Stage =
				EAngelscriptCacheValidationStage::None;
			uint64 ByteOffset = MAX_uint64;
			bool bExactFastPathEligible = true;
			int32 MatchingScopeCount = INDEX_NONE;
			EAngelscriptCachedFastPathScopeKind ScopeKind =
				EAngelscriptCachedFastPathScopeKind::Invalid;
			FAngelscriptHash256 ScopeStableKey;
			EAngelscriptCachedFastPathIneligibleReason Reason =
				EAngelscriptCachedFastPathIneligibleReason::Invalid;
			FString Diagnostic;
			uint64 DecodedBytes = 0;
			uint64 ResidentDecodedBytes = 0;
			uint64 TemporaryResidentDecodedBytes = 0;
			uint64 PeakLiveResidentDecodedBytes = 0;
			int32 EventCount = INDEX_NONE;
			bool bEventOverflowed = false;
			FAllocationEvent Events[2];
		};

		const auto RunQuery = [SourceBytes, ModuleKey](
			const int32 CaptureCapacity) -> FQuerySummary
		{
			FQuerySummary Summary;
			FAngelscriptCacheReadLimits Limits;
			FAngelscriptCacheReadBudget DecodeBudget;
			FDecodedSourceIndexHandleForTests Token;
			const FAngelscriptCacheValidationResult DecodeResult =
				DecodeSourceIndexHandleForTests(
					SourceBytes, Limits, DecodeBudget, Token);
			if (!DecodeResult.IsSuccess())
			{
				Summary.Error = DecodeResult.Error;
				Summary.Class = DecodeResult.Class;
				Summary.Stage = DecodeResult.Stage;
				Summary.ByteOffset = DecodeResult.ByteOffset;
				return Summary;
			}

			FAngelscriptCacheReadBudget QueryBudget;
			FAngelscriptCacheExactFastPathEligibility QueryResult;
			FAngelscriptCacheValidationResult Result;
			if (CaptureCapacity < 0)
			{
				Result = QueryExactFastPathEligibilityForTests(
					Token, ModuleKey, Limits, QueryBudget, QueryResult);
			}
			else
			{
				Summary.EventCount = 0;
				Result = QueryExactFastPathEligibilityWithAllocationCaptureForTests(
						Token, ModuleKey, Limits, QueryBudget,
						FAllocationEventCaptureView(
							TArrayView<FAllocationEvent>(Summary.Events, CaptureCapacity),
							Summary.EventCount,
							Summary.bEventOverflowed),
						QueryResult);
			}

			Summary.Error = Result.Error;
			Summary.Class = Result.Class;
			Summary.Stage = Result.Stage;
			Summary.ByteOffset = Result.ByteOffset;
			Summary.bExactFastPathEligible = QueryResult.bExactFastPathEligible;
			Summary.MatchingScopeCount = QueryResult.MatchingScopes.Num();
			if (!QueryResult.MatchingScopes.IsEmpty())
			{
				const FAngelscriptCachedFastPathIneligibleScope& Scope =
					QueryResult.MatchingScopes[0];
				Summary.ScopeKind = Scope.ScopeKind;
				Summary.ScopeStableKey = Scope.ScopeStableKey;
				Summary.Reason = Scope.Reason;
				Summary.Diagnostic = Scope.CanonicalDiagnosticIdentity;
			}
			Summary.DecodedBytes = QueryBudget.GetDecodedBytes();
			Summary.ResidentDecodedBytes = QueryBudget.GetResidentDecodedBytes();
			Summary.TemporaryResidentDecodedBytes =
				QueryBudget.GetTemporaryResidentDecodedBytes();
			Summary.PeakLiveResidentDecodedBytes =
				QueryBudget.GetPeakLiveResidentDecodedBytes();
			return Summary;
		};

		const FQuerySummary Baseline = RunQuery(-1);
		TFuture<FQuerySummary> ZeroCapacityFuture = Async(
			EAsyncExecution::ThreadPool,
			[RunQuery]() { return RunQuery(0); });
		TFuture<FQuerySummary> OneCapacityFuture = Async(
			EAsyncExecution::ThreadPool,
			[RunQuery]() { return RunQuery(1); });
		TFuture<FQuerySummary> ExactCapacityFuture = Async(
			EAsyncExecution::ThreadPool,
			[RunQuery]() { return RunQuery(2); });
		const FQuerySummary ZeroCapacity = ZeroCapacityFuture.Get();
		const FQuerySummary OneCapacity = OneCapacityFuture.Get();
		const FQuerySummary ExactCapacity = ExactCapacityFuture.Get();

		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::None, Baseline.Error));
		ASSERT_THAT(IsFalse(Baseline.bExactFastPathEligible));
		ASSERT_THAT(AreEqual(1, Baseline.MatchingScopeCount));
		const FQuerySummary* CapturedSummaries[] = {
			&ZeroCapacity, &OneCapacity, &ExactCapacity};
		for (const FQuerySummary* Captured : CapturedSummaries)
		{
			ASSERT_THAT(AreEqual(Baseline.Error, Captured->Error));
			ASSERT_THAT(AreEqual(Baseline.Class, Captured->Class));
			ASSERT_THAT(AreEqual(Baseline.Stage, Captured->Stage));
			ASSERT_THAT(AreEqual(Baseline.ByteOffset, Captured->ByteOffset));
			ASSERT_THAT(AreEqual(Baseline.bExactFastPathEligible,
				Captured->bExactFastPathEligible));
			ASSERT_THAT(AreEqual(Baseline.MatchingScopeCount,
				Captured->MatchingScopeCount));
			ASSERT_THAT(AreEqual(Baseline.ScopeKind, Captured->ScopeKind));
			ASSERT_THAT(IsTrue(Baseline.ScopeStableKey == Captured->ScopeStableKey));
			ASSERT_THAT(AreEqual(Baseline.Reason, Captured->Reason));
			ASSERT_THAT(AreEqual(Baseline.Diagnostic, Captured->Diagnostic));
			ASSERT_THAT(AreEqual(Baseline.DecodedBytes, Captured->DecodedBytes));
			ASSERT_THAT(AreEqual(Baseline.ResidentDecodedBytes,
				Captured->ResidentDecodedBytes));
			ASSERT_THAT(AreEqual(Baseline.TemporaryResidentDecodedBytes,
				Captured->TemporaryResidentDecodedBytes));
			ASSERT_THAT(AreEqual(Baseline.PeakLiveResidentDecodedBytes,
				Captured->PeakLiveResidentDecodedBytes));
		}

		ASSERT_THAT(AreEqual(0, ZeroCapacity.EventCount));
		ASSERT_THAT(IsTrue(ZeroCapacity.bEventOverflowed));
		ASSERT_THAT(AreEqual(1, OneCapacity.EventCount));
		ASSERT_THAT(IsTrue(OneCapacity.bEventOverflowed));
		ASSERT_THAT(AreEqual(EAllocationPhase::PrimaryScratchArrays,
			OneCapacity.Events[0].Phase));
		ASSERT_THAT(AreEqual(2, ExactCapacity.EventCount));
		ASSERT_THAT(IsFalse(ExactCapacity.bEventOverflowed));
		ASSERT_THAT(AreEqual(EAllocationPhase::PrimaryScratchArrays,
			ExactCapacity.Events[0].Phase));
		ASSERT_THAT(AreEqual(EAllocationPhase::AuxiliaryScratchArrays,
			ExactCapacity.Events[1].Phase));
		ASSERT_THAT(AreEqual(OneCapacity.Events[0].RequestedCapacity,
			ExactCapacity.Events[0].RequestedCapacity));
	}

	TEST_METHOD(EligibilityResultChargesActualOwnedCapacitiesAndRejectsOneByteShort)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;

		constexpr int32 DiagnosticLength = 31;
		const FString Diagnostic = FString::ChrN(DiagnosticLength, TEXT('D'));
		constexpr int32 MatchingCount = 5;

		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		const auto AddMatchingScope = [&](const int32 Ordinal,
			const EAngelscriptCachedFastPathScopeKind ScopeKind,
			const FAngelscriptHash256& ScopeKey,
			const EAngelscriptCachedFastPathIneligibleReason Reason)
		{
			if (Ordinal < MatchingCount)
			{
				Source.IneligibleScopes.Add(MakeIneligibleScope(
					ScopeKind, ScopeKey, Reason, Diagnostic));
			}
		};
		AddMatchingScope(0, EAngelscriptCachedFastPathScopeKind::Mount,
			Source.Mounts[0].MountKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior);
		AddMatchingScope(1, EAngelscriptCachedFastPathScopeKind::Provider,
			Source.Providers[0].ProviderKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior);
		AddMatchingScope(2, EAngelscriptCachedFastPathScopeKind::Hook,
			Source.PreprocessHooks[0].HookKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource);
		AddMatchingScope(3, EAngelscriptCachedFastPathScopeKind::SourceFile,
			Source.Files[0].SourceFileKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior);
		AddMatchingScope(4, EAngelscriptCachedFastPathScopeKind::Module,
			Source.Files[0].ModuleKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior);
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(Source).IsSuccess()));
		const FAngelscriptStableModuleKey ModuleKey = Source.Files[0].ModuleKey;
		TArray<uint8> SourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, SourceBytes).IsSuccess()));

		const int32 ExpectedScopeCapacity =
			CalculateArrayReserveCapacityForTests<
				FAngelscriptCachedFastPathIneligibleScope>(MatchingCount);
		const int32 ExpectedDiagnosticCapacity =
			CalculateArrayReserveCapacityForTests<TCHAR>(DiagnosticLength + 1);
		ASSERT_THAT(IsTrue(ExpectedScopeCapacity >= MatchingCount));
		ASSERT_THAT(IsTrue(ExpectedDiagnosticCapacity >= DiagnosticLength + 1));
		const uint64 ExpectedOwnedBytes =
			static_cast<uint64>(ExpectedScopeCapacity)
				* sizeof(FAngelscriptCachedFastPathIneligibleScope)
			+ static_cast<uint64>(MatchingCount)
				* static_cast<uint64>(ExpectedDiagnosticCapacity) * sizeof(TCHAR);

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget MeasureDecodeBudget;
		FDecodedSourceIndexHandleForTests MeasureToken;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			SourceBytes, Limits, MeasureDecodeBudget, MeasureToken).IsSuccess()));
		FAngelscriptCacheReadBudget MeasureQueryBudget;
		FAngelscriptCacheExactFastPathEligibility MeasureResult;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			MeasureToken, ModuleKey, Limits,
			MeasureQueryBudget, MeasureResult).IsSuccess()));
		ASSERT_THAT(AreEqual(MatchingCount, MeasureResult.MatchingScopes.Num()));
		const uint64 ActualOwnedBytes =
			static_cast<uint64>(MeasureResult.MatchingScopes.GetAllocatedSize());
		uint64 ActualOwnedBytesWithDiagnostics = ActualOwnedBytes;
		for (const FAngelscriptCachedFastPathIneligibleScope& Scope :
			MeasureResult.MatchingScopes)
		{
			ActualOwnedBytesWithDiagnostics += static_cast<uint64>(
				Scope.CanonicalDiagnosticIdentity.GetCharArray().GetAllocatedSize());
		}
		ASSERT_THAT(AreEqual(ExpectedOwnedBytes, ActualOwnedBytesWithDiagnostics));
		ASSERT_THAT(AreEqual(ActualOwnedBytesWithDiagnostics,
			MeasureQueryBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			MeasureQueryBudget.GetTemporaryResidentDecodedBytes()));
		const uint64 ExactTotalDecodedBytes = MeasureQueryBudget.GetDecodedBytes();
		const uint64 ExactPeakLiveBytes =
			MeasureQueryBudget.GetPeakLiveResidentDecodedBytes();

		const auto RunBoundedQuery = [&](const uint64 MaxTotalDecodedBytes,
			const uint64 MaxResidentDecodedBytes,
			FAngelscriptCacheReadBudget& QueryBudget,
			FAngelscriptCacheExactFastPathEligibility& OutResult)
		{
			FAngelscriptCacheReadLimits BoundedLimits;
			BoundedLimits.MaxTotalDecodedBytes = MaxTotalDecodedBytes;
			BoundedLimits.MaxResidentDecodedBytes = MaxResidentDecodedBytes;
			FAngelscriptCacheReadBudget DecodeBudget;
			FDecodedSourceIndexHandleForTests Token;
			const FAngelscriptCacheValidationResult DecodeResult =
				DecodeSourceIndexHandleForTests(
					SourceBytes, Limits, DecodeBudget, Token);
			if (!DecodeResult.IsSuccess())
			{
				return DecodeResult;
			}
			return QueryExactFastPathEligibilityForTests(
				Token, ModuleKey, BoundedLimits, QueryBudget, OutResult);
		};

		FAngelscriptCacheReadBudget ExactQueryBudget;
		FAngelscriptCacheExactFastPathEligibility ExactResult;
		ASSERT_THAT(IsTrue(RunBoundedQuery(
			ExactTotalDecodedBytes, ExactPeakLiveBytes,
			ExactQueryBudget, ExactResult).IsSuccess()));
		ASSERT_THAT(AreEqual(MatchingCount, ExactResult.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(ExpectedOwnedBytes,
			ExactQueryBudget.GetResidentDecodedBytes()));

		FAngelscriptCacheReadBudget TotalShortBudget;
		FAngelscriptCacheExactFastPathEligibility TotalShortResult;
		TotalShortResult.MatchingScopes.AddDefaulted();
		FAngelscriptCacheValidationResult Result = RunBoundedQuery(
			ExactTotalDecodedBytes - 1, MAX_uint64,
			TotalShortBudget, TotalShortResult);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
			Result.Error));
		ASSERT_THAT(AreEqual(0, TotalShortResult.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(0, TotalShortResult.MatchingScopes.Max()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			TotalShortBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			TotalShortBudget.GetTemporaryResidentDecodedBytes()));

		FAngelscriptCacheReadBudget ResidentShortBudget;
		FAngelscriptCacheExactFastPathEligibility ResidentShortResult;
		ResidentShortResult.MatchingScopes.AddDefaulted();
		Result = RunBoundedQuery(
			MAX_uint64, ExactPeakLiveBytes - 1,
			ResidentShortBudget, ResidentShortResult);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
			Result.Error));
		ASSERT_THAT(AreEqual(0, ResidentShortResult.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(0, ResidentShortResult.MatchingScopes.Max()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ResidentShortBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ResidentShortBudget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(ValidatedSourceIndexMoveTransfersAuthorityAndInvalidatesSource)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;

		const FAngelscriptCachedSourceIndex FirstSource = MakeSourceIndex();
		FAngelscriptCachedSourceIndex SecondSource = MakeSourceIndex();
		SecondSource.Providers[0].ContentFingerprint = MakeHash(0xc1);
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(SecondSource).IsSuccess()));
		ASSERT_THAT(IsFalse(
			FirstSource.SourceSnapshot == SecondSource.SourceSnapshot));

		TArray<uint8> FirstBytes;
		TArray<uint8> SecondBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			FirstSource, FirstBytes).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			SecondSource, SecondBytes).IsSuccess()));

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget FirstDecodeBudget;
		FDecodedSourceIndexHandleForTests Original;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			FirstBytes, Limits, FirstDecodeBudget, Original).IsSuccess()));
		ASSERT_THAT(IsTrue(Original.IsValid()));
		ASSERT_THAT(IsTrue(
			Original.GetValue().SourceSnapshot == FirstSource.SourceSnapshot));

		FDecodedSourceIndexHandleForTests MoveConstructed(MoveTemp(Original));
		ASSERT_THAT(IsFalse(Original.IsValid()));
		ASSERT_THAT(IsTrue(MoveConstructed.IsValid()));
		ASSERT_THAT(IsTrue(MoveConstructed.GetValue().SourceSnapshot
			== FirstSource.SourceSnapshot));

		FAngelscriptCacheReadBudget MovedFromQueryBudget;
		FAngelscriptCacheExactFastPathEligibility MovedFromResult;
		MovedFromResult.bExactFastPathEligible = true;
		MovedFromResult.MatchingScopes.AddDefaulted();
		FAngelscriptCacheValidationResult Result =
			QueryExactFastPathEligibilityForTests(
				Original, FirstSource.Files[0].ModuleKey,
				Limits, MovedFromQueryBudget, MovedFromResult);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			Result.Error));
		ASSERT_THAT(IsFalse(MovedFromResult.bExactFastPathEligible));
		ASSERT_THAT(AreEqual(0, MovedFromResult.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(0, MovedFromResult.MatchingScopes.Max()));
		ASSERT_THAT(AreEqual(UINT64_C(0), MovedFromQueryBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			MovedFromQueryBudget.GetResidentDecodedBytes()));

		FAngelscriptCacheReadBudget MoveConstructedQueryBudget;
		FAngelscriptCacheExactFastPathEligibility MoveConstructedResult;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			MoveConstructed, FirstSource.Files[0].ModuleKey,
			Limits, MoveConstructedQueryBudget, MoveConstructedResult).IsSuccess()));
		ASSERT_THAT(IsTrue(MoveConstructedResult.bExactFastPathEligible));

		FAngelscriptCacheReadBudget SecondDecodeBudget;
		FDecodedSourceIndexHandleForTests MoveAssigned;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			SecondBytes, Limits, SecondDecodeBudget, MoveAssigned).IsSuccess()));
		ASSERT_THAT(IsTrue(MoveAssigned.GetValue().SourceSnapshot
			== SecondSource.SourceSnapshot));
		MoveAssigned = MoveTemp(MoveConstructed);
		ASSERT_THAT(IsFalse(MoveConstructed.IsValid()));
		ASSERT_THAT(IsTrue(MoveAssigned.IsValid()));
		ASSERT_THAT(IsTrue(MoveAssigned.GetValue().SourceSnapshot
			== FirstSource.SourceSnapshot));
		ASSERT_THAT(IsTrue(MoveAssigned.GetValue().Files[0].ModuleKey.Hash
			== FirstSource.Files[0].ModuleKey.Hash));

		FAngelscriptCacheReadBudget MoveAssignedQueryBudget;
		FAngelscriptCacheExactFastPathEligibility MoveAssignedResult;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			MoveAssigned, FirstSource.Files[0].ModuleKey,
			Limits, MoveAssignedQueryBudget, MoveAssignedResult).IsSuccess()));
		ASSERT_THAT(IsTrue(MoveAssignedResult.bExactFastPathEligible));
	}

	TEST_METHOD(LateSourceAndModuleSemanticFailureRollsBackOnlyCandidateOwnership)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;

		const FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		TArray<uint8> SourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, SourceBytes).IsSuccess()));
		SourceBytes[4] ^= 0x01;

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget SourceBudget;
		ASSERT_THAT(IsTrue(SourceBudget.TryConsumeRetainedDecoded(7, Limits)));
		FAngelscriptCacheTemporaryResidentReservation SourceSeedTemporary;
		ASSERT_THAT(IsTrue(SourceBudget.TryReserveTemporaryDecoded(
			5, Limits, SourceSeedTemporary)));
		const uint64 SourceSeedTotal = SourceBudget.GetDecodedBytes();
		const uint64 SourceSeedLive = SourceBudget.GetResidentDecodedBytes()
			+ SourceBudget.GetTemporaryResidentDecodedBytes();
		FDecodedSourceIndexHandleForTests SourceOutput;
		FAngelscriptCacheValidationResult Result =
			DecodeSourceIndexHandleForTests(
				SourceBytes, Limits, SourceBudget, SourceOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex,
			Result.RecordKind));
		ASSERT_THAT(IsFalse(SourceOutput.IsValid()));
		ASSERT_THAT(IsTrue(SourceBudget.GetDecodedBytes() > SourceSeedTotal));
		ASSERT_THAT(AreEqual(UINT64_C(7),
			SourceBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(5),
			SourceBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(
			SourceBudget.GetPeakLiveResidentDecodedBytes() > SourceSeedLive));

		const FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		TArray<uint8> InterfaceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			Interface, InterfaceBytes).IsSuccess()));
		const int32 InterfaceAbiOffset =
			4 + 32 + 4 + Interface.CanonicalModuleName.Len();
		InterfaceBytes[InterfaceAbiOffset] ^= 0x01;

		FAngelscriptCacheReadBudget InterfaceBudget;
		ASSERT_THAT(IsTrue(InterfaceBudget.TryConsumeRetainedDecoded(11, Limits)));
		FAngelscriptCacheTemporaryResidentReservation InterfaceSeedTemporary;
		ASSERT_THAT(IsTrue(InterfaceBudget.TryReserveTemporaryDecoded(
			13, Limits, InterfaceSeedTemporary)));
		const uint64 InterfaceSeedTotal = InterfaceBudget.GetDecodedBytes();
		const uint64 InterfaceSeedLive = InterfaceBudget.GetResidentDecodedBytes()
			+ InterfaceBudget.GetTemporaryResidentDecodedBytes();
		FAngelscriptCachedModuleInterface InterfaceOutput = MakeModuleInterface();
		Result = DecodeModuleInterfaceForTests(
			InterfaceBytes, Limits, InterfaceBudget, InterfaceOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface,
			Result.RecordKind));
		ASSERT_THAT(AreEqual(0u, InterfaceOutput.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(InterfaceOutput.Declarations.IsEmpty()));
		ASSERT_THAT(IsTrue(InterfaceOutput.Imports.IsEmpty()));
		ASSERT_THAT(IsTrue(InterfaceBudget.GetDecodedBytes() > InterfaceSeedTotal));
		ASSERT_THAT(AreEqual(UINT64_C(11),
			InterfaceBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(13),
			InterfaceBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(
			InterfaceBudget.GetPeakLiveResidentDecodedBytes() > InterfaceSeedLive));

		SourceSeedTemporary.Reset();
		InterfaceSeedTemporary.Reset();
		ASSERT_THAT(AreEqual(UINT64_C(0),
			SourceBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			InterfaceBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(SourceBudget.GetDecodedBytes() > SourceSeedTotal));
		ASSERT_THAT(IsTrue(InterfaceBudget.GetDecodedBytes() > InterfaceSeedTotal));
	}

	TEST_METHOD(DerivedOutputBuildersClearEveryOutputAtomicallyOnFailure)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;

		FAngelscriptCachedDeclaration Declaration =
			MakeFunctionDeclaration(MakeModuleKey());
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Invalid;
		FAngelscriptHash256 SignatureHash = MakeHash(0xe1);
		FAngelscriptHash256 TraitsHash = MakeHash(0xe2);
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				Declaration, SignatureHash, TraitsHash);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownEnumValue,
			Result.Error));
		ASSERT_THAT(IsTrue(SignatureHash.IsZero()));
		ASSERT_THAT(IsTrue(TraitsHash.IsZero()));

		FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		Source.Providers[0].ProviderKind =
			EAngelscriptCachedSourceProviderKind::Invalid;
		FAngelscriptHash256 SourceSnapshot = MakeHash(0xe3);
		Result = FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Source, SourceSnapshot);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownEnumValue,
			Result.Error));
		ASSERT_THAT(IsTrue(SourceSnapshot.IsZero()));

		FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		Interface.Declarations[0].DeclarationKind =
			EAngelscriptCacheDeclarationKind::Invalid;
		FAngelscriptHash256 InterfaceAbi = MakeHash(0xe4);
		Result = FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			Interface, InterfaceAbi);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownEnumValue,
			Result.Error));
		ASSERT_THAT(IsTrue(InterfaceAbi.IsZero()));
	}

	TEST_METHOD(GlobalStorageContentInvalidatesStateWithoutChangingModuleInterfaceAbi)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;

		FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		FAngelscriptCacheSemanticDependency GlobalStorage;
		GlobalStorage.Kind = EAngelscriptCacheSemanticDependencyKind::GlobalStorage;
		GlobalStorage.Target = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptGlobal, MakeHash(0xa1), MakeHash(0xa2)};
		GlobalStorage.ExpectedContentOrValue = MakeHash(0xa3);
		Interface.Dependencies.Add(GlobalStorage);

		FAngelscriptHash256 BaselineAbi;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			Interface, BaselineAbi).IsSuccess(),
			TEXT("GlobalStorage must admit a non-zero storage-layout fingerprint")));

		Interface.Dependencies.Last().ExpectedContentOrValue = MakeHash(0xa4);
		FAngelscriptHash256 ContentChangedAbi;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			Interface, ContentChangedAbi).IsSuccess()));
		ASSERT_THAT(AreEqual(BaselineAbi, ContentChangedAbi,
			TEXT("Storage-layout content invalidates ModuleState, not ModuleInterface ABI")));

		Interface.Dependencies.Last().Target.ExpectedAbi = MakeHash(0xa5);
		FAngelscriptHash256 TargetAbiChanged;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			Interface, TargetAbiChanged).IsSuccess()));
		ASSERT_THAT(AreNotEqual(BaselineAbi, TargetAbiChanged,
			TEXT("GlobalStorage remains an ABI-bearing stable-reference dependency")));
	}

	TEST_METHOD(ValidationScratchAndValidatedEligibilityAreBudgetedBeforeAllocation)
	{
		using namespace AngelscriptCacheSourceInterfaceTests_Private;
		FAngelscriptCacheReadLimits ReservationLimits;
		ReservationLimits.MaxResidentDecodedBytes = 16;
		FAngelscriptCacheReadBudget ReservationBudget;
		ASSERT_THAT(IsTrue(ReservationBudget.TryConsumeStored(3, ReservationLimits)));
		FAngelscriptCacheTemporaryResidentReservation Reservation;
		ASSERT_THAT(IsTrue(ReservationBudget.TryReserveTemporaryDecoded(
			8, ReservationLimits, Reservation)));
		ASSERT_THAT(AreEqual(UINT64_C(3), ReservationBudget.GetStoredBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8), ReservationBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), ReservationBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8),
			ReservationBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8),
			ReservationBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(Reservation.IsActive()));
		ASSERT_THAT(IsFalse(ReservationBudget.TryReserveTemporaryDecoded(
			1, ReservationLimits, Reservation),
			TEXT("An active output guard rejects an otherwise fitting request")));
		ASSERT_THAT(AreEqual(UINT64_C(8), ReservationBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8),
			ReservationBudget.GetTemporaryResidentDecodedBytes()));
		FAngelscriptCacheTemporaryResidentReservation SecondReservation;
		ASSERT_THAT(IsFalse(ReservationBudget.TryReserveTemporaryDecoded(
			9, ReservationLimits, SecondReservation)));
		ASSERT_THAT(IsFalse(SecondReservation.IsActive()));
		ASSERT_THAT(AreEqual(UINT64_C(8), ReservationBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8),
			ReservationBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(IsFalse(ReservationBudget.TryConsumeRetainedDecoded(
			9, ReservationLimits)));
		ASSERT_THAT(AreEqual(UINT64_C(8), ReservationBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), ReservationBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8),
			ReservationBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8),
			ReservationBudget.GetPeakLiveResidentDecodedBytes()));
		Reservation.Reset();
		ASSERT_THAT(IsFalse(Reservation.IsActive()));
		ASSERT_THAT(AreEqual(UINT64_C(3), ReservationBudget.GetStoredBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8), ReservationBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), ReservationBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ReservationBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8),
			ReservationBudget.GetPeakLiveResidentDecodedBytes()));

		FAngelscriptCacheReadBudget FreshReservationBudget;
		ASSERT_THAT(AreEqual(UINT64_C(0), FreshReservationBudget.GetStoredBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), FreshReservationBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), FreshReservationBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			FreshReservationBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			FreshReservationBudget.GetPeakLiveResidentDecodedBytes()));

		const FAngelscriptCachedSourceIndex Source = MakeSourceIndex();
		TArray<uint8> SourceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, SourceBytes).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget SourceMeasureBudget;
		FDecodedSourceIndexHandleForTests Validated;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			SourceBytes, Limits, SourceMeasureBudget, Validated).IsSuccess()));
		ASSERT_THAT(IsTrue(Validated.IsValid()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			SourceMeasureBudget.GetTemporaryResidentDecodedBytes()));

		const uint64 SourceResident = SourceMeasureBudget.GetResidentDecodedBytes();
		const uint64 SourcePeakLive = SourceMeasureBudget.GetPeakLiveResidentDecodedBytes();
		ASSERT_THAT(IsTrue(SourcePeakLive >= SourceResident));
		const uint64 SourceScratch = SourcePeakLive - SourceResident;
		ASSERT_THAT(IsTrue(SourceScratch > 0));
		Limits.MaxResidentDecodedBytes = SourcePeakLive;
		FAngelscriptCacheReadBudget ExactSourceBudget;
		FDecodedSourceIndexHandleForTests ExactSourceLimit;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			SourceBytes, Limits, ExactSourceBudget, ExactSourceLimit).IsSuccess()));
		ASSERT_THAT(IsTrue(ExactSourceLimit.IsValid()));
		ASSERT_THAT(AreEqual(SourcePeakLive,
			ExactSourceBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ExactSourceBudget.GetTemporaryResidentDecodedBytes()));

		Limits.MaxResidentDecodedBytes = SourcePeakLive - 1;
		FAngelscriptCacheReadBudget ShortSourceBudget;
		FDecodedSourceIndexHandleForTests Rejected;
		FAngelscriptCacheValidationResult Result =
			DecodeSourceIndexHandleForTests(
				SourceBytes, Limits, ShortSourceBudget, Rejected);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex, Result.RecordKind));
		ASSERT_THAT(IsFalse(Rejected.IsValid()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ShortSourceBudget.GetTemporaryResidentDecodedBytes()));

		TArray<uint8> LateSemanticFailure = SourceBytes;
		LateSemanticFailure[4] ^= 0x01;
		Limits = {};
		FAngelscriptCacheReadBudget LateSourceFailureBudget;
		Result = DecodeSourceIndexHandleForTests(
			LateSemanticFailure, Limits, LateSourceFailureBudget, Rejected);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch, Result.Error));
		ASSERT_THAT(IsFalse(Rejected.IsValid()));
		ASSERT_THAT(IsTrue(
			LateSourceFailureBudget.GetPeakLiveResidentDecodedBytes() > 0));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			LateSourceFailureBudget.GetTemporaryResidentDecodedBytes()));

		Limits = {};
		FAngelscriptCacheReadBudget QueryMeasureBudget;
		FAngelscriptCacheExactFastPathEligibility Eligibility;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			Validated, Source.Files[0].ModuleKey,
			Limits, QueryMeasureBudget, Eligibility).IsSuccess()));
		const uint64 QueryResident = QueryMeasureBudget.GetResidentDecodedBytes();
		const uint64 QueryPeakLive = QueryMeasureBudget.GetPeakLiveResidentDecodedBytes();
		ASSERT_THAT(IsTrue(QueryPeakLive >= QueryResident));
		const uint64 QueryScratch = QueryPeakLive - QueryResident;
		ASSERT_THAT(IsTrue(QueryScratch > 0));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			QueryMeasureBudget.GetTemporaryResidentDecodedBytes()));
		Limits.MaxResidentDecodedBytes = QueryPeakLive;
		FAngelscriptCacheReadBudget ExactQueryBudget;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			Validated, Source.Files[0].ModuleKey,
			Limits, ExactQueryBudget, Eligibility).IsSuccess()));
		ASSERT_THAT(IsTrue(Eligibility.bExactFastPathEligible));
		ASSERT_THAT(AreEqual(QueryPeakLive,
			ExactQueryBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ExactQueryBudget.GetTemporaryResidentDecodedBytes()));

		Limits.MaxResidentDecodedBytes = QueryPeakLive - 1;
		FAngelscriptCacheReadBudget ShortQueryBudget;
		Eligibility.bExactFastPathEligible = true;
		Eligibility.MatchingScopes.Add({});
		Result = QueryExactFastPathEligibilityForTests(
			Validated, Source.Files[0].ModuleKey, Limits, ShortQueryBudget, Eligibility);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(IsFalse(Eligibility.bExactFastPathEligible));
		ASSERT_THAT(IsTrue(Eligibility.MatchingScopes.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ShortQueryBudget.GetTemporaryResidentDecodedBytes()));

		Limits = {};
		FAngelscriptCacheReadBudget MissingTargetQueryBudget;
		Result = QueryExactFastPathEligibilityForTests(
			Validated, MakeSecondaryModuleKey(), Limits, MissingTargetQueryBudget, Eligibility);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingGraphTarget, Result.Error));
		ASSERT_THAT(IsTrue(
			MissingTargetQueryBudget.GetPeakLiveResidentDecodedBytes() > 0));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			MissingTargetQueryBudget.GetTemporaryResidentDecodedBytes()));

		FAngelscriptCachedSourceIndex IneligibleSource = MakeSourceIndex();
		IneligibleSource.IneligibleScopes.Add(MakeIneligibleScope(
			EAngelscriptCachedFastPathScopeKind::Module,
			IneligibleSource.Files[0].ModuleKey.Hash,
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior,
			TEXT("ResidentOutputProbe")));
		ASSERT_THAT(IsTrue(RefreshSourceSnapshot(IneligibleSource).IsSuccess()));
		TArray<uint8> IneligibleBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			IneligibleSource, IneligibleBytes).IsSuccess()));
		FAngelscriptCacheReadBudget IneligibleDecodeBudget;
		FDecodedSourceIndexHandleForTests IneligibleToken;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			IneligibleBytes, Limits, IneligibleDecodeBudget, IneligibleToken).IsSuccess()));

		FAngelscriptCacheReadBudget IneligibleMeasureQueryBudget;
		FAngelscriptCacheExactFastPathEligibility IneligibleResult;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			IneligibleToken, IneligibleSource.Files[0].ModuleKey,
			Limits, IneligibleMeasureQueryBudget, IneligibleResult).IsSuccess()));
		ASSERT_THAT(AreEqual(1, IneligibleResult.MatchingScopes.Num()));
		const uint64 IneligibleResident =
			IneligibleMeasureQueryBudget.GetResidentDecodedBytes();
		const uint64 IneligiblePeakLive =
			IneligibleMeasureQueryBudget.GetPeakLiveResidentDecodedBytes();
		ASSERT_THAT(IsTrue(IneligiblePeakLive >= IneligibleResident));
		const uint64 IneligibleScratch = IneligiblePeakLive - IneligibleResident;
		ASSERT_THAT(IsTrue(IneligibleScratch > 0 && IneligibleResident > 1));

		Limits.MaxResidentDecodedBytes = IneligiblePeakLive;
		FAngelscriptCacheReadBudget ExactIneligibleQueryBudget;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			IneligibleToken, IneligibleSource.Files[0].ModuleKey,
			Limits, ExactIneligibleQueryBudget, IneligibleResult).IsSuccess()));
		ASSERT_THAT(AreEqual(1, IneligibleResult.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(IneligiblePeakLive,
			ExactIneligibleQueryBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(IneligibleResident,
			ExactIneligibleQueryBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ExactIneligibleQueryBudget.GetTemporaryResidentDecodedBytes()));

		Limits.MaxResidentDecodedBytes = IneligiblePeakLive - 1;
		FAngelscriptCacheReadBudget ShortIneligibleQueryBudget;
		Result = QueryExactFastPathEligibilityForTests(
			IneligibleToken, IneligibleSource.Files[0].ModuleKey,
			Limits, ShortIneligibleQueryBudget, IneligibleResult);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(IsTrue(IneligibleResult.MatchingScopes.IsEmpty()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ShortIneligibleQueryBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ShortIneligibleQueryBudget.GetTemporaryResidentDecodedBytes()));

		Limits = {};
		FAngelscriptCacheReadBudget CombinedExactBudget;
		FDecodedSourceIndexHandleForTests CombinedExactToken;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			IneligibleBytes, Limits, CombinedExactBudget, CombinedExactToken).IsSuccess()));
		const uint64 CombinedExactTokenResident =
			CombinedExactBudget.GetResidentDecodedBytes();
		Limits.MaxResidentDecodedBytes = CombinedExactTokenResident
			+ IneligiblePeakLive;
		ASSERT_THAT(IsTrue(QueryExactFastPathEligibilityForTests(
			CombinedExactToken, IneligibleSource.Files[0].ModuleKey,
			Limits, CombinedExactBudget, IneligibleResult).IsSuccess()));
		ASSERT_THAT(AreEqual(1, IneligibleResult.MatchingScopes.Num()));
		ASSERT_THAT(AreEqual(CombinedExactTokenResident + IneligibleResident,
			CombinedExactBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			CombinedExactBudget.GetTemporaryResidentDecodedBytes()));

		Limits = {};
		FAngelscriptCacheReadBudget CombinedShortBudget;
		FDecodedSourceIndexHandleForTests CombinedShortToken;
		ASSERT_THAT(IsTrue(DecodeSourceIndexHandleForTests(
			IneligibleBytes, Limits, CombinedShortBudget, CombinedShortToken).IsSuccess()));
		const uint64 CombinedShortTokenResident =
			CombinedShortBudget.GetResidentDecodedBytes();
		Limits.MaxResidentDecodedBytes = CombinedShortTokenResident
			+ IneligiblePeakLive - 1;
		Result = QueryExactFastPathEligibilityForTests(
			CombinedShortToken, IneligibleSource.Files[0].ModuleKey,
			Limits, CombinedShortBudget, IneligibleResult);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(IsTrue(IneligibleResult.MatchingScopes.IsEmpty()));
		ASSERT_THAT(AreEqual(CombinedShortTokenResident,
			CombinedShortBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			CombinedShortBudget.GetTemporaryResidentDecodedBytes()));

		const FAngelscriptCachedModuleInterface Interface = MakeModuleInterface();
		TArray<uint8> InterfaceBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			Interface, InterfaceBytes).IsSuccess()));
		Limits = {};
		FAngelscriptCacheReadBudget InterfaceMeasureBudget;
		FAngelscriptCachedModuleInterface DecodedInterface;
		ASSERT_THAT(IsTrue(DecodeModuleInterfaceForTests(
			InterfaceBytes, Limits, InterfaceMeasureBudget, DecodedInterface).IsSuccess()));
		const uint64 InterfaceResident = InterfaceMeasureBudget.GetResidentDecodedBytes();
		const uint64 InterfacePeakLive =
			InterfaceMeasureBudget.GetPeakLiveResidentDecodedBytes();
		ASSERT_THAT(IsTrue(InterfacePeakLive >= InterfaceResident));
		const uint64 InterfaceScratch = InterfacePeakLive - InterfaceResident;
		ASSERT_THAT(IsTrue(InterfaceScratch > 0));
		Limits.MaxResidentDecodedBytes = InterfacePeakLive;
		FAngelscriptCacheReadBudget ExactInterfaceBudget;
		ASSERT_THAT(IsTrue(DecodeModuleInterfaceForTests(
			InterfaceBytes, Limits, ExactInterfaceBudget, DecodedInterface).IsSuccess()));
		ASSERT_THAT(AreEqual(InterfacePeakLive,
			ExactInterfaceBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ExactInterfaceBudget.GetTemporaryResidentDecodedBytes()));

		Limits.MaxResidentDecodedBytes = InterfacePeakLive - 1;
		FAngelscriptCacheReadBudget ShortInterfaceBudget;
		Result = DecodeModuleInterfaceForTests(
			InterfaceBytes, Limits, ShortInterfaceBudget, DecodedInterface);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface, Result.RecordKind));
		ASSERT_THAT(AreEqual(0u, DecodedInterface.PayloadSchemaVersion));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ShortInterfaceBudget.GetTemporaryResidentDecodedBytes()));

		TArray<uint8> LateInterfaceFailure = InterfaceBytes;
		const int32 InterfaceAbiOffset = 4 + 32 + 4 + Interface.CanonicalModuleName.Len();
		LateInterfaceFailure[InterfaceAbiOffset] ^= 0x01;
		Limits = {};
		FAngelscriptCacheReadBudget LateInterfaceFailureBudget;
		Result = DecodeModuleInterfaceForTests(
			LateInterfaceFailure, Limits, LateInterfaceFailureBudget, DecodedInterface);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch, Result.Error));
		ASSERT_THAT(IsTrue(
			LateInterfaceFailureBudget.GetPeakLiveResidentDecodedBytes() > 0));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			LateInterfaceFailureBudget.GetTemporaryResidentDecodedBytes()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
