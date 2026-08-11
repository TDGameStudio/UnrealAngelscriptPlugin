#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheModuleGraphDependencyTests_Private
{
	static FAngelscriptHash256 MakeHash(const uint8 Seed)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (int32 Index = 0; Index < static_cast<int32>(sizeof(Bytes)); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Seed + Index);
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	template <typename ElementType>
	static bool TryCalculateReserveBytes(
		const int32 RequestedCapacity,
		int32& OutReservedCapacity,
		uint64& OutBytes)
	{
		OutReservedCapacity = 0;
		OutBytes = 0;
		if (RequestedCapacity <= 0)
		{
			return RequestedCapacity == 0;
		}
		using FArrayType = TArray<ElementType>;
		typename FArrayType::ElementAllocatorType Allocator;
		if constexpr (TAllocatorTraits<typename FArrayType::AllocatorType>::
			SupportsElementAlignment)
		{
			OutReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			OutReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType));
		}
		if (OutReservedCapacity < RequestedCapacity
			|| static_cast<uint64>(OutReservedCapacity)
				> MAX_uint64 / sizeof(ElementType))
		{
			return false;
		}
		OutBytes = static_cast<uint64>(OutReservedCapacity)
			* sizeof(ElementType);
		return true;
	}

	static FAngelscriptCachedSourceProviderKey BuildProviderKey(
		const FAngelscriptCachedSourceProvider& Provider)
	{
		FAngelscriptCachedSourceProviderKey Key;
		const FAngelscriptSourceProviderIdentityInput Input{
			Provider.ProviderKind, Provider.CanonicalImplementationIdentity,
			Provider.IdentityFingerprint};
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
			Input, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedSourceMountKey BuildMountKey(
		const FAngelscriptCachedSourceMount& Mount)
	{
		FAngelscriptCachedSourceMountKey Key;
		const FAngelscriptSourceMountIdentityInput Input{
			Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey};
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
			Input, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedSourceFileKey BuildSourceFileKey(
		const FAngelscriptCachedSourceFile& File)
	{
		FAngelscriptCachedSourceFileKey Key;
		const FAngelscriptSourceFileIdentityInput Input{
			File.SourceKind, File.MountKey, File.ProviderKey,
			File.RelativeLogicalPath, File.GeneratedSourceKey};
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(
			Input, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedSourceIndex MakeSourceIndex(
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		FAngelscriptCachedSourceIndex Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Value.DiscoveryPolicy.PolicyVersion = 1;

		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity = TEXT("DependencyFixture.Disk");
		Provider.IdentityFingerprint = MakeHash(0x71);
		Provider.VersionFingerprint = MakeHash(0x72);
		Provider.ConfigurationFingerprint = MakeHash(0x73);
		Provider.ContentFingerprint = MakeHash(0x74);
		Provider.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		Provider.ProviderKey = BuildProviderKey(Provider);
		Value.Providers.Add(Provider);

		FAngelscriptCachedSourceMount Mount;
		Mount.SourceKind = EAngelscriptCachedSourceKind::Game;
		Mount.LogicalMount = TEXT("Game");
		Mount.ProviderKey = Provider.ProviderKey;
		Mount.RootConfigurationFingerprint = MakeHash(0x75);
		Mount.MountKey = BuildMountKey(Mount);
		Value.Mounts.Add(Mount);

		FAngelscriptCachedSourceFile File;
		File.SourceKind = EAngelscriptCachedSourceKind::Game;
		File.MountKey = Mount.MountKey;
		File.ProviderKey = Provider.ProviderKey;
		File.RelativeLogicalPath = TEXT("DependencyFixture.as");
		File.RawContentHash = MakeHash(0x76);
		File.ModuleKey = ModuleKey;
		File.SourceFileKey = BuildSourceFileKey(File);
		Value.Files.Add(File);

		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Value, Value.SourceSnapshot).IsSuccess());
		return Value;
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
		Declaration.CanonicalName = TEXT("ResolveExternal");
		Declaration.CanonicalDeclaration = TEXT("void ResolveExternal()");
		Declaration.DeclaredType = FAngelscriptCachedDataType{
			EAngelscriptCachedDataTypeKind::Primitive,
			EAngelscriptCachedPrimitiveType::Void};
		Declaration.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Function, 0});

		FAngelscriptFunctionIdentityDescriptor Identity;
		Identity.OwnerKind = Declaration.OwnerKind;
		Identity.OwnerKey = Declaration.OwnerKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCacheSemanticDependency MakeDependency(
		const bool bRequireContent)
	{
		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = bRequireContent
			? EAngelscriptCacheSemanticDependencyKind::CompileOption
			: EAngelscriptCacheSemanticDependencyKind::Declaration;
		Dependency.Target.Kind = EAngelscriptCacheReferenceKind::EnvironmentSymbol;
		Dependency.Target.StableKey = MakeHash(0x90);
		Dependency.Target.ExpectedAbi = MakeHash(0x91);
		if (bRequireContent)
		{
			Dependency.ExpectedContentOrValue = MakeHash(0x92);
		}
		return Dependency;
	}

	enum class EDependencyRoute : uint8
	{
		ExternalEnvironment,
		SelectedModuleFunction,
		SelectedModuleFunctionWrongAbi,
	};

	enum class EDependencyOwner : uint8
	{
		FunctionBody,
		ModuleInterface,
		ModuleInterfaceAndFunctionBody,
	};

	static FAngelscriptCacheSemanticDependency MakeSelectedModuleFunctionDependency(
		const FAngelscriptCachedDeclaration& Declaration,
		const bool bWrongAbi)
	{
		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::Declaration;
		Dependency.Target.Kind = EAngelscriptCacheReferenceKind::ScriptFunction;
		Dependency.Target.StableKey = Declaration.StableKey;
		Dependency.Target.ExpectedAbi = bWrongAbi
			? MakeHash(0x96) : Declaration.SignatureHash;
		return Dependency;
	}

	static FAngelscriptCachedFunctionBody MakeFunctionBody(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptArtifactProfileKey& Profile,
		const FAngelscriptCachedDeclaration& Declaration,
		const FAngelscriptCacheSemanticDependency& Dependency,
		const bool bIncludeDependency)
	{
		FAngelscriptCachedFunctionBody Body;
		Body.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion;
		Body.ModuleKey = ModuleKey;
		Body.Identity.FunctionKey =
			FAngelscriptStableFunctionKey{Declaration.StableKey};
		Body.Identity.Profile = Profile;
		Body.ExpectedDeclarationAbi = Declaration.SignatureHash;
		Body.FunctionSourceDigest.Hash = MakeHash(0xa1);
		Body.FunctionInputDigest.Hash = MakeHash(0xa2);
		Body.InvocationKind =
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction;
		Body.VmExecutionCodecVersion = 1;
		Body.CanonicalExecutionPayload = {0x31, 0x32, 0x33};
		Body.Identity.Content.Execution =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				Body.CanonicalExecutionPayload, {}).Execution;
		Body.Identity.Content.Debug =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(Profile);
		if (bIncludeDependency)
		{
			Body.ActualDependencies.Add(Dependency);
		}
		return Body;
	}

	static TOptional<FAngelscriptDecodedCacheRecordHandle> Decode(
		const EAngelscriptCacheRecordKind Kind,
		const TArray<uint8>& Payload,
		FAngelscriptCacheReadBudget& Budget)
	{
		FAngelscriptCacheRecordId RecordId;
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			Kind, Payload, RecordId).IsSuccess());
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		check(FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId, Payload, FAngelscriptCacheReadLimits{},
			Budget, Output).IsSuccess());
		return Output;
	}

	struct FFixture
	{
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptStableModuleKey ModuleKey{MakeHash(0x10)};
		FAngelscriptArtifactProfileKey Profile{MakeHash(0x30)};
		FAngelscriptCacheSemanticDependency Dependency;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Source;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Interface;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Function;
		TOptional<FAngelscriptDecodedCacheRecordHandle> State;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Snapshot;

		explicit FFixture(
			const bool bRequireContent = false,
			const EDependencyRoute DependencyRoute =
				EDependencyRoute::ExternalEnvironment,
			const EDependencyOwner DependencyOwner =
				EDependencyOwner::FunctionBody)
			: Dependency(MakeDependency(bRequireContent))
		{
			TArray<uint8> Payload;
			check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				MakeSourceIndex(ModuleKey), Payload).IsSuccess());
			Source = Decode(EAngelscriptCacheRecordKind::SourceIndex,
				Payload, Budget);

			const FAngelscriptCachedDeclaration Declaration =
				MakeFunctionDeclaration(ModuleKey);
			if (DependencyRoute != EDependencyRoute::ExternalEnvironment)
			{
				check(!bRequireContent);
				Dependency = MakeSelectedModuleFunctionDependency(
					Declaration,
					DependencyRoute
						== EDependencyRoute::SelectedModuleFunctionWrongAbi);
			}
			FAngelscriptCachedModuleInterface InterfaceValue;
			InterfaceValue.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			InterfaceValue.ModuleKey = ModuleKey;
			InterfaceValue.CanonicalModuleName = TEXT("DependencyFixture");
			InterfaceValue.CanonicalNamespaces.Add(TEXT("Gameplay"));
			InterfaceValue.Declarations.Add(Declaration);
			if (DependencyOwner == EDependencyOwner::ModuleInterface
				|| DependencyOwner
					== EDependencyOwner::ModuleInterfaceAndFunctionBody)
			{
				InterfaceValue.Dependencies.Add(Dependency);
			}
			check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
				InterfaceValue, InterfaceValue.InterfaceAbi).IsSuccess());
			Payload.Reset();
			check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				InterfaceValue, Payload).IsSuccess());
			Interface = Decode(EAngelscriptCacheRecordKind::ModuleInterface,
				Payload, Budget);

			const FAngelscriptCachedFunctionBody FunctionValue = MakeFunctionBody(
				ModuleKey, Profile, Declaration, Dependency,
				DependencyOwner == EDependencyOwner::FunctionBody
					|| DependencyOwner
						== EDependencyOwner::ModuleInterfaceAndFunctionBody);
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
				FunctionValue, Payload).IsSuccess());
			Function = Decode(EAngelscriptCacheRecordKind::FunctionBody,
				Payload, Budget);

			FAngelscriptCachedModuleState StateValue;
			StateValue.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
			StateValue.ModuleKey = ModuleKey;
			StateValue.Profile = Profile;
			check(FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
				StateValue, StateValue.StateInputHash).IsSuccess());
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
				StateValue, Payload).IsSuccess());
			State = Decode(EAngelscriptCacheRecordKind::ModuleState, Payload, Budget);

			FAngelscriptCachedModuleSnapshot SnapshotValue;
			SnapshotValue.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion;
			SnapshotValue.ModuleKey = ModuleKey;
			SnapshotValue.ModuleInterface = {
				ModuleKey, Interface.GetValue()->GetRecordId()};
			SnapshotValue.ModuleState = {
				ModuleKey, State.GetValue()->GetRecordId()};
			SnapshotValue.FunctionBodies.Add({
				FAngelscriptStableFunctionKey{Declaration.StableKey},
				Function.GetValue()->GetRecordId()});
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
				SnapshotValue, Payload).IsSuccess());
			Snapshot = Decode(EAngelscriptCacheRecordKind::ModuleSnapshot,
				Payload, Budget);
		}

		TArray<FAngelscriptDecodedCacheRecordHandle> MakePool() const
		{
			return {Function.GetValue(), State.GetValue(), Snapshot.GetValue(),
				Interface.GetValue()};
		}
	};

	class FCurrentSymbols final : public IAngelscriptCacheCurrentSymbolResolver
	{
	public:
		mutable uint32 CallCount = 0;
		bool bMissing = false;
		FAngelscriptHash256 Abi = MakeHash(0x91);
		TOptional<FAngelscriptHash256> Content;

		virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
			const EAngelscriptCacheReferenceKind ReferenceKind,
			const FAngelscriptHash256& StableKey) const override
		{
			++CallCount;
			check(ReferenceKind
				== EAngelscriptCacheReferenceKind::EnvironmentSymbol);
			check(StableKey == MakeHash(0x90));
			if (bMissing)
			{
				return {};
			}
			FAngelscriptCacheCurrentSymbol Symbol;
			Symbol.CurrentAbi = Abi;
			Symbol.CurrentContentOrValue = Content;
			return Symbol;
		}
	};

	class FNoCurrentLayouts final : public IAngelscriptCacheCurrentLayoutResolver
	{
	public:
		virtual TOptional<FAngelscriptCacheResolvedDataTypeLayout>
		ResolveDataTypeLayout(
			const FAngelscriptCachedDataType&,
			const IAngelscriptCacheProspectiveTypeLayoutView&) const override
		{
			return {};
		}

		virtual TOptional<FAngelscriptCacheResolvedTypeLayoutInput>
		ResolveTypeLayoutInput(
			EAngelscriptCachedTypeLayoutInputKind,
			EAngelscriptCacheReferenceKind,
			const FAngelscriptHash256&) const override
		{
			return {};
		}
	};

	class FDependencyOpaquePayloads final
		: public IAngelscriptCacheOpaquePayloadValidator
	{
	public:
		FAngelscriptCacheSemanticDependency Dependency;
		bool bReturnRelocation = true;
		bool bWrongRelocationKey = false;
		mutable uint32 CallCount = 0;

		virtual FAngelscriptCacheValidationResult Validate(
			const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
			const FAngelscriptCacheReadLimits&,
			FAngelscriptCacheReadBudget&,
			IAngelscriptCacheCandidateChargeSink& GraphCandidate,
			FAngelscriptCacheOpaquePayloadSummary& OutSummary) const override
		{
			OutSummary = {};
			++CallCount;
			check(Request.Kind
				== EAngelscriptCacheOpaquePayloadKind::FunctionExecution);
			OutSummary.ValidatedPayloadHash =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					Request.CanonicalPayload, {}).Execution;
			if (!bReturnRelocation)
			{
				return {};
			}

			int32 ReservedCapacity = 0;
			uint64 RetainedBytes = 0;
			if (!TryCalculateReserveBytes<FAngelscriptCacheRelocationUse>(
				1, ReservedCapacity, RetainedBytes))
			{
				return FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::Overflow,
					EAngelscriptCacheRecordKind::FunctionBody,
					EAngelscriptCacheValidationStage::OpaqueCodec, 0);
			}
			const EAngelscriptCacheCandidateChargeResult Charge =
				GraphCandidate.TryExtend(RetainedBytes);
			if (Charge != EAngelscriptCacheCandidateChargeResult::Success)
			{
				return FAngelscriptCacheValidationResult::AtStage(
					Charge == EAngelscriptCacheCandidateChargeResult::BudgetExceeded
						? EAngelscriptCacheValidationError::BudgetExceeded
						: EAngelscriptCacheValidationError::Overflow,
					EAngelscriptCacheRecordKind::FunctionBody,
					EAngelscriptCacheValidationStage::OpaqueCodec, 0);
			}
			OutSummary.OrderedRelocations.Reserve(ReservedCapacity);
			FAngelscriptCacheRelocationUse& Relocation =
				OutSummary.OrderedRelocations.AddDefaulted_GetRef();
			Relocation.InstructionOrdinal = 4;
			Relocation.OperandSlot = 1;
			Relocation.DependencyKind = Dependency.Kind;
			Relocation.ReferenceKind = Dependency.Target.Kind;
			Relocation.StableKey = bWrongRelocationKey
				? MakeHash(0x93) : Dependency.Target.StableKey;
			Relocation.ExpectedAbi = Dependency.Target.ExpectedAbi;
			Relocation.ExpectedContentOrValue =
				Dependency.ExpectedContentOrValue;
			return {};
		}
	};

	struct FContextFixture
	{
		FCurrentSymbols Symbols;
		FNoCurrentLayouts Layouts;
		FDependencyOpaquePayloads Opaque;

		FAngelscriptCacheModuleGraphValidationContext Make(
			const FFixture& Fixture)
		{
			Opaque.Dependency = Fixture.Dependency;
			FAngelscriptCacheModuleGraphValidationContext Context;
			Context.SelectedProfile = Fixture.Profile;
			Context.SelectedSourceSnapshot = Fixture.Source.GetValue()
				->TryGetSourceIndex()->SourceSnapshot;
			Context.SourceIndex = &Fixture.Source.GetValue().Get();
			Context.CurrentSymbols = &Symbols;
			Context.CurrentLayouts = &Layouts;
			Context.OpaquePayloads = &Opaque;
			return Context;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheModuleGraphDependencyTests,
	"Angelscript.TestModule.Cache.Archive.ModuleGraphDependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RelocationsAreAValidatedSubsetBeforeCurrentResolution)
	{
		using namespace AngelscriptCacheModuleGraphDependencyTests_Private;

		FFixture Valid;
		FContextFixture ValidContext;
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Valid.Snapshot.GetValue()->GetRecordId(), Valid.MakePool(),
			ValidContext.Make(Valid), FAngelscriptCacheReadLimits{},
			Valid.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), ValidContext.Opaque.CallCount));
		ASSERT_THAT(AreEqual(uint32(1), ValidContext.Symbols.CallCount));
		ASSERT_THAT(AreEqual(int32(1),
			Graph.GetOpaqueSummaries()[0].OrderedRelocations.Num()));

		FFixture DependencyWithoutOperand;
		FContextFixture NoOperandContext;
		NoOperandContext.Opaque.bReturnRelocation = false;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			DependencyWithoutOperand.Snapshot.GetValue()->GetRecordId(),
			DependencyWithoutOperand.MakePool(),
			NoOperandContext.Make(DependencyWithoutOperand),
			FAngelscriptCacheReadLimits{}, DependencyWithoutOperand.Budget,
			Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), NoOperandContext.Symbols.CallCount));

		FFixture WrongRelocation;
		FContextFixture WrongRelocationContext;
		WrongRelocationContext.Opaque.bWrongRelocationKey = true;
		const FAngelscriptCacheValidationResult Result =
			ValidateModuleSnapshotGraph(
				WrongRelocation.Snapshot.GetValue()->GetRecordId(),
				WrongRelocation.MakePool(),
				WrongRelocationContext.Make(WrongRelocation),
				FAngelscriptCacheReadLimits{}, WrongRelocation.Budget, Graph);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::RelocationDependencyMismatch,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(0),
			WrongRelocationContext.Symbols.CallCount));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			WrongRelocation.Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(CurrentSymbolFailuresAreTypedAndAtomic)
	{
		using namespace AngelscriptCacheModuleGraphDependencyTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		const auto RequireFailure = [this, &Graph](
			FFixture& Fixture,
			FContextFixture& ContextFixture,
			const EAngelscriptCacheValidationError Expected)
		{
			const FAngelscriptCacheValidationResult Result =
				ValidateModuleSnapshotGraph(
					Fixture.Snapshot.GetValue()->GetRecordId(), Fixture.MakePool(),
					ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
					Fixture.Budget, Graph);
			ASSERT_THAT(AreEqual(Expected, Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::CurrentResolver,
				Result.Stage));
			ASSERT_THAT(AreEqual(uint32(1), ContextFixture.Symbols.CallCount));
			ASSERT_THAT(IsTrue(Graph.IsEmpty()));
			ASSERT_THAT(AreEqual(uint64(0),
				Fixture.Budget.GetTemporaryResidentDecodedBytes()));
		};

		FFixture Missing;
		FContextFixture MissingContext;
		MissingContext.Symbols.bMissing = true;
		RequireFailure(Missing, MissingContext,
			EAngelscriptCacheValidationError::CurrentSymbolMissing);

		FFixture WrongAbi;
		FContextFixture WrongAbiContext;
		WrongAbiContext.Symbols.Abi = MakeHash(0x94);
		RequireFailure(WrongAbi, WrongAbiContext,
			EAngelscriptCacheValidationError::CurrentAbiMismatch);

		FFixture WrongContent(true);
		FContextFixture WrongContentContext;
		WrongContentContext.Symbols.Content = MakeHash(0x95);
		RequireFailure(WrongContent, WrongContentContext,
			EAngelscriptCacheValidationError::CurrentContentMismatch);
	}

	TEST_METHOD(SelectedModuleFunctionDependencyIsGraphClosedBeforeCurrentEligibility)
	{
		using namespace AngelscriptCacheModuleGraphDependencyTests_Private;

		FFixture Valid(false, EDependencyRoute::SelectedModuleFunction);
		FContextFixture ValidContext;
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Valid.Snapshot.GetValue()->GetRecordId(), Valid.MakePool(),
			ValidContext.Make(Valid), FAngelscriptCacheReadLimits{},
			Valid.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), ValidContext.Opaque.CallCount));
		ASSERT_THAT(AreEqual(uint32(0), ValidContext.Symbols.CallCount));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));

		FFixture WrongAbi(
			false, EDependencyRoute::SelectedModuleFunctionWrongAbi);
		FContextFixture WrongAbiContext;
		FAngelscriptCacheModuleGraphValidationContext Context =
			WrongAbiContext.Make(WrongAbi);
		Context.SelectedSourceSnapshot = MakeHash(0x97);
		Context.SelectedProfile.Hash = MakeHash(0x98);
		const FAngelscriptCacheValidationResult Result =
			ValidateModuleSnapshotGraph(
				WrongAbi.Snapshot.GetValue()->GetRecordId(), WrongAbi.MakePool(),
				Context, FAngelscriptCacheReadLimits{}, WrongAbi.Budget, Graph);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::GraphAbiMismatch, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationStage::ModuleGraph, Result.Stage));
		ASSERT_THAT(AreEqual(uint32(0), WrongAbiContext.Symbols.CallCount));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			WrongAbi.Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(ModuleInterfaceDependenciesShareTheOrderedCurrentMemo)
	{
		using namespace AngelscriptCacheModuleGraphDependencyTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		FFixture InterfaceOnly(false, EDependencyRoute::ExternalEnvironment,
			EDependencyOwner::ModuleInterface);
		FContextFixture InterfaceOnlyContext;
		InterfaceOnlyContext.Opaque.bReturnRelocation = false;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			InterfaceOnly.Snapshot.GetValue()->GetRecordId(),
			InterfaceOnly.MakePool(), InterfaceOnlyContext.Make(InterfaceOnly),
			FAngelscriptCacheReadLimits{}, InterfaceOnly.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), InterfaceOnlyContext.Symbols.CallCount));

		FFixture DuplicateAcrossOwners(false,
			EDependencyRoute::ExternalEnvironment,
			EDependencyOwner::ModuleInterfaceAndFunctionBody);
		FContextFixture DuplicateContext;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			DuplicateAcrossOwners.Snapshot.GetValue()->GetRecordId(),
			DuplicateAcrossOwners.MakePool(),
			DuplicateContext.Make(DuplicateAcrossOwners),
			FAngelscriptCacheReadLimits{}, DuplicateAcrossOwners.Budget,
			Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), DuplicateContext.Symbols.CallCount));

		FFixture WrongAbi(false, EDependencyRoute::ExternalEnvironment,
			EDependencyOwner::ModuleInterface);
		FContextFixture WrongAbiContext;
		WrongAbiContext.Opaque.bReturnRelocation = false;
		WrongAbiContext.Symbols.Abi = MakeHash(0x99);
		const FAngelscriptCacheValidationResult Result =
			ValidateModuleSnapshotGraph(
				WrongAbi.Snapshot.GetValue()->GetRecordId(), WrongAbi.MakePool(),
				WrongAbiContext.Make(WrongAbi), FAngelscriptCacheReadLimits{},
				WrongAbi.Budget, Graph);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::CurrentAbiMismatch, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationStage::CurrentResolver, Result.Stage));
		ASSERT_THAT(AreEqual(uint32(1), WrongAbiContext.Symbols.CallCount));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			WrongAbi.Budget.GetTemporaryResidentDecodedBytes()));
	}
};

#endif
