#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheModuleSnapshotGraphTests_Private
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
		OutBytes = static_cast<uint64>(OutReservedCapacity) * sizeof(ElementType);
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
		const FAngelscriptStableModuleKey& ModuleKey,
		FAngelscriptCachedSourceFileKey& OutSourceFileKey,
		const bool bChooseExistingFileMissedByKeyOrderedSearch = false)
	{
		FAngelscriptCachedSourceIndex Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Value.DiscoveryPolicy.PolicyVersion = 1;

		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity = TEXT("GraphFixture.Disk");
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

		const int32 FileCount = bChooseExistingFileMissedByKeyOrderedSearch ? 16 : 1;
		for (int32 FileIndex = 0; FileIndex < FileCount; ++FileIndex)
		{
			FAngelscriptCachedSourceFile File;
			File.SourceKind = EAngelscriptCachedSourceKind::Game;
			File.MountKey = Mount.MountKey;
			File.ProviderKey = Provider.ProviderKey;
			File.RelativeLogicalPath = FileCount == 1
				? TEXT("GraphFixture.as")
				: FString::Printf(TEXT("GraphFixture/%02d.as"), FileIndex);
			File.RawContentHash = MakeHash(static_cast<uint8>(0x76 + FileIndex));
			File.ModuleKey = ModuleKey;
			File.SourceFileKey = BuildSourceFileKey(File);
			Value.Files.Add(MoveTemp(File));
		}

		if (!bChooseExistingFileMissedByKeyOrderedSearch)
		{
			OutSourceFileKey = Value.Files[0].SourceFileKey;
		}
		else
		{
			// Files already have their canonical order because all authority fields
			// before RelativeLogicalPath are equal and the paths are zero-padded.
			// Select an existing key for which a key-only binary search over that
			// canonical order fails. This is the exact multi-file shape that exposed
			// the production graph validator's incompatible lookup order.
			bool bSelected = false;
			for (const FAngelscriptCachedSourceFile& Candidate : Value.Files)
			{
				int32 First = 0;
				int32 Last = Value.Files.Num();
				while (First < Last)
				{
					const int32 Middle = First + (Last - First) / 2;
					if (Value.Files[Middle].SourceFileKey.Hash
						< Candidate.SourceFileKey.Hash)
					{
						First = Middle + 1;
					}
					else
					{
						Last = Middle;
					}
				}
				if (!Value.Files.IsValidIndex(First)
					|| !(Value.Files[First].SourceFileKey.Hash
						== Candidate.SourceFileKey.Hash))
				{
					OutSourceFileKey = Candidate.SourceFileKey;
					bSelected = true;
					break;
				}
			}
			checkf(bSelected,
				TEXT("The deterministic multi-file fixture must distinguish canonical SourceIndex order from SourceFileKey order"));
		}

		const FAngelscriptCacheValidationResult SnapshotResult =
			FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
				Value, Value.SourceSnapshot);
		checkf(SnapshotResult.IsSuccess(),
			TEXT("Graph SourceIndex fixture rejected: Error=%u Class=%u Kind=%u Stage=%u Offset=%llu"),
			static_cast<uint32>(SnapshotResult.Error),
			static_cast<uint32>(SnapshotResult.Class),
			static_cast<uint32>(SnapshotResult.RecordKind),
			static_cast<uint32>(SnapshotResult.Stage),
			SnapshotResult.ByteOffset);
		return Value;
	}

	static FAngelscriptCachedModuleInterface MakeModuleInterface(
		const FAngelscriptStableModuleKey& ModuleKey,
		const TOptional<FAngelscriptCachedDeclaration>& TypeDeclaration = {},
		const TOptional<FAngelscriptCachedDeclaration>& FunctionDeclaration = {},
		const TOptional<FAngelscriptCachedDeclaration>& GlobalDeclaration = {})
	{
		FAngelscriptCachedModuleInterface Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		Value.ModuleKey = ModuleKey;
		Value.CanonicalModuleName = TEXT("GraphFixture");
		if (TypeDeclaration.IsSet() || FunctionDeclaration.IsSet()
			|| GlobalDeclaration.IsSet())
		{
			Value.CanonicalNamespaces.Add(TEXT("Gameplay"));
		}
		if (TypeDeclaration.IsSet())
		{
			Value.Declarations.Add(TypeDeclaration.GetValue());
		}
		if (FunctionDeclaration.IsSet())
		{
			Value.Declarations.Add(FunctionDeclaration.GetValue());
		}
		if (GlobalDeclaration.IsSet())
		{
			Value.Declarations.Add(GlobalDeclaration.GetValue());
		}
		check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			Value, Value.InterfaceAbi).IsSuccess());
		return Value;
	}

	static FAngelscriptCachedDeclaration MakeTypeDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Type;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::Class;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = ModuleKey.Hash;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = TEXT("Minimal");
		Declaration.CanonicalDeclaration = TEXT("type Minimal");
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration, 0});

		FAngelscriptTypeIdentityDescriptor Identity;
		Identity.ModuleKey = ModuleKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakeFunctionDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FString& CanonicalName = TEXT("Tick"),
		const FString& CanonicalDeclaration = TEXT("void Tick()"))
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
		Declaration.CanonicalName = CanonicalName;
		Declaration.CanonicalDeclaration = CanonicalDeclaration;
		Declaration.DeclaredType = FAngelscriptCachedDataType{
			EAngelscriptCachedDataTypeKind::Primitive,
			EAngelscriptCachedPrimitiveType::Void};
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Function, 0});

		FAngelscriptFunctionIdentityDescriptor Identity;
		Identity.OwnerKind = Declaration.OwnerKind;
		Identity.OwnerKey = Declaration.OwnerKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakeModuleInitializerDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Function;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::ModuleInitializer;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = ModuleKey.Hash;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = TEXT("ModuleInitialize");
		Declaration.CanonicalDeclaration = TEXT("void ModuleInitialize()");
		Declaration.DeclaredType = FAngelscriptCachedDataType{
			EAngelscriptCachedDataTypeKind::Primitive,
			EAngelscriptCachedPrimitiveType::Void};
		Declaration.TraitFlags = static_cast<uint32>(
			EAngelscriptCachedDeclarationTraitFlags::Generated);

		FAngelscriptFunctionIdentityDescriptor Identity;
		Identity.OwnerKind = Declaration.OwnerKind;
		Identity.OwnerKey = Declaration.OwnerKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakeGlobalDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey)
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
		Declaration.CanonicalName = TEXT("Counter");
		Declaration.CanonicalDeclaration = TEXT("int Counter");
		Declaration.CanonicalTypeSpelling = TEXT("int");
		Declaration.DeclaredType = FAngelscriptCachedDataType{
			EAngelscriptCachedDataTypeKind::Primitive,
			EAngelscriptCachedPrimitiveType::Int32};
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration, 0});

		FAngelscriptGlobalIdentityDescriptor Identity;
		Identity.ModuleKey = ModuleKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.Name = Declaration.CanonicalName;
		Identity.CanonicalType = Declaration.CanonicalTypeSpelling.GetValue();
		Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedTypeSchema MakeMinimalClassSchema(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptStableTypeKey& TypeKey)
	{
		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = ModuleKey;
		Schema.TypeKey = TypeKey;
		Schema.TypeKind = EAngelscriptCachedTypeKind::Class;
		Schema.CanonicalNamespace = TEXT("Gameplay");
		Schema.CanonicalName = TEXT("Minimal");
		Schema.CanonicalDeclaration = TEXT("type Minimal");
		Schema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::ReferenceType);
		Schema.Layout.SemanticSize = 0;
		Schema.Layout.SemanticAlignment = 8;
		Schema.Layout.BasePropertyBoundary = 0;
		Schema.Reflection.ReflectionKind =
			EAngelscriptCachedReflectionKind::None;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
		return Schema;
	}

	static FAngelscriptCachedFunctionBody MakeFunctionBody(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptArtifactProfileKey& Profile,
		const FAngelscriptCachedDeclaration& Declaration,
		const bool bWrongDeclarationAbi = false,
		const EAngelscriptCachedFunctionInvocationKind InvocationKind =
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
		const TOptional<FAngelscriptCacheRecordId>& DebugSidecar = {},
		const TOptional<FAngelscriptHash256>& DebugHash = {})
	{
		FAngelscriptCachedFunctionBody Body;
		Body.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion;
		Body.ModuleKey = ModuleKey;
		Body.Identity.FunctionKey =
			FAngelscriptStableFunctionKey{Declaration.StableKey};
		Body.Identity.Profile = Profile;
		Body.ExpectedDeclarationAbi = bWrongDeclarationAbi
			? MakeHash(0xe1) : Declaration.SignatureHash;
		Body.FunctionSourceDigest.Hash = MakeHash(0xe2);
		Body.FunctionInputDigest.Hash = MakeHash(0xe3);
		Body.InvocationKind = InvocationKind;
		Body.VmExecutionCodecVersion = 1;
		Body.CanonicalExecutionPayload = {0x01, 0x02, 0x03};
		Body.Identity.Content.Execution =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				Body.CanonicalExecutionPayload, {}).Execution;
		if (DebugSidecar.IsSet() && DebugHash.IsSet())
		{
			Body.DebugSidecar = DebugSidecar;
			Body.Identity.Content.Debug = DebugHash.GetValue();
		}
		else
		{
			Body.Identity.Content.Debug =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(Profile);
		}
		return Body;
	}

	static FAngelscriptCachedDebugSidecar MakeDebugSidecar(
		const FAngelscriptArtifactProfileKey& Profile,
		const FAngelscriptStableFunctionKey& FunctionKey,
		const bool bWrongFunctionKey,
		const bool bWrongProfile,
		const TOptional<FAngelscriptCachedSourceFileKey>& SourceFileKey = {})
	{
		FAngelscriptCachedDebugSidecar Debug;
		Debug.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::DebugSidecarPayloadSchemaVersion;
		Debug.FunctionKey = bWrongFunctionKey
			? FAngelscriptStableFunctionKey{MakeHash(0xd1)} : FunctionKey;
		Debug.Profile = bWrongProfile
			? FAngelscriptArtifactProfileKey{MakeHash(0xd2)} : Profile;
		Debug.VmDebugCodecVersion = 1;
		Debug.CanonicalDebugPayload = {0x0d, 0x0e, 0x0f};
		if (SourceFileKey.IsSet())
		{
			FAngelscriptCachedDebugSourceReference& Source =
				Debug.Sources.AddDefaulted_GetRef();
			Source.SourceFileKey = SourceFileKey.GetValue();
			Source.CanonicalLogicalSection = TEXT("");
			check(FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
				Source.SourceFileKey, Source.CanonicalLogicalSection,
				Source.LogicalSectionKey).IsSuccess());
		}
		Debug.DebugHash =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				{}, Debug.CanonicalDebugPayload).Debug;
		return Debug;
	}

	static FAngelscriptCachedModuleState MakeModuleState(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptArtifactProfileKey& Profile,
		const TOptional<FAngelscriptCachedDeclaration>& GlobalDeclaration = {},
		const bool bWrongGlobalShape = false,
		const TOptional<FAngelscriptCachedDeclaration>&
			ModuleInitializerDeclaration = {},
		const bool bWrongInitializerAbi = false)
	{
		FAngelscriptCachedModuleState Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
		Value.ModuleKey = ModuleKey;
		Value.Profile = Profile;
		if (GlobalDeclaration.IsSet())
		{
			FAngelscriptCachedGlobalSchema& Global =
				Value.OrderedGlobals.AddDefaulted_GetRef();
			Global.StorageOrdinal = 0;
			Global.GlobalKey = FAngelscriptStableGlobalKey{
				GlobalDeclaration->StableKey};
			Global.CanonicalNamespace = GlobalDeclaration->CanonicalNamespace;
			Global.CanonicalName = bWrongGlobalShape
				? TEXT("OtherCounter") : GlobalDeclaration->CanonicalName;
			Global.Type = GlobalDeclaration->DeclaredType.GetValue();
			Global.GlobalTraitFlags = GlobalDeclaration->TraitFlags;
			Global.InitializationKind =
				EAngelscriptCachedGlobalInitializationKind::Default;
			Global.CleanupPolicy = EAngelscriptCachedGlobalCleanupPolicy::None;
			check(FAngelscriptCacheRemainingRecordArchive::
				ComputeGlobalStorageLayoutFingerprint(
					ModuleKey, Global,
					Global.StorageLayoutFingerprint).IsSuccess());
		}
		if (ModuleInitializerDeclaration.IsSet())
		{
			FAngelscriptCachedInitializerUnit& Initializer =
				Value.Initializers.AddDefaulted_GetRef();
			Initializer.InitializerKind =
				EAngelscriptCachedInitializerKind::Module;
			Initializer.InitializerKey = FAngelscriptStableFunctionKey{
				ModuleInitializerDeclaration->StableKey};
			Initializer.VmInitializerCodecVersion = 1;
			Initializer.CanonicalExecutionPayload = {0x21, 0x22, 0x23};
			check(FAngelscriptCacheRemainingRecordArchive::
				ComputeInitializerExecutionHash(
					ModuleKey, Profile, Initializer,
					Initializer.InitializerExecutionHash).IsSuccess());

			FAngelscriptCachedInitializationAction& Action =
				Value.OrderedInitializationActions.AddDefaulted_GetRef();
			Action.ActionOrdinal = 0;
			Action.ActionKind =
				EAngelscriptCachedInitializationActionKind::ExecuteInitializer;
			Action.Target.Kind = EAngelscriptCacheReferenceKind::ScriptFunction;
			Action.Target.StableKey = Initializer.InitializerKey.Hash;
			Action.Target.ExpectedAbi = bWrongInitializerAbi
				? MakeHash(0xe4)
				: ModuleInitializerDeclaration->SignatureHash;
		}
		check(FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
			Value, Value.StateInputHash).IsSuccess());
		return Value;
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
			RecordId, Payload, FAngelscriptCacheReadLimits{}, Budget, Output).IsSuccess());
		return Output;
	}

	struct FFixture
	{
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptStableModuleKey ModuleKey{MakeHash(0x10)};
		FAngelscriptArtifactProfileKey Profile{MakeHash(0x30)};
		FAngelscriptCachedSourceFileKey SourceFileKey;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Source;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Interface;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Type;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Function;
		TOptional<FAngelscriptDecodedCacheRecordHandle> SecondFunction;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Debug;
		TOptional<FAngelscriptDecodedCacheRecordHandle> State;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Snapshot;

		enum class EGlobalFixtureKind : uint8
		{
			Valid,
			MissingState,
			UndeclaredState,
			WrongShape,
		};

		enum class EModuleInitializerFixtureKind : uint8
		{
			Valid,
			MissingState,
			UndeclaredState,
			WrongActionAbi,
		};

		enum class ESourceFixtureKind : uint8
		{
			ExistingFileInCanonicalNonKeyOrder,
		};

		explicit FFixture(
			const bool bDeclareType = false,
			const bool bLinkType = false,
			const bool bDeclareFunction = false,
			const bool bLinkFunction = false,
			const bool bWrongFunctionAbi = false,
			const EAngelscriptCachedFunctionInvocationKind FunctionInvocationKind =
				EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			const bool bLinkDebug = false,
			const bool bWrongDebugFunctionKey = false,
			const bool bWrongDebugProfile = false,
			const bool bWrongBodyDebugHash = false,
			const bool bLinkDebugSource = false,
			const bool bMissingDebugSource = false,
			const bool bDuplicateDebugOwner = false,
			const bool bDeclareGlobal = false,
			const bool bStateGlobal = false,
			const bool bWrongGlobalShape = false,
			const bool bDeclareModuleInitializer = false,
			const bool bStateModuleInitializer = false,
			const bool bWrongInitializerAbi = false,
			const bool bCanonicalSourceOrderDiffersFromKeyOrder = false)
		{
			check(!bLinkDebug || bLinkFunction);
			check(!bDuplicateDebugOwner
				|| (bDeclareFunction && bLinkFunction && bLinkDebug));
			check(!(bDeclareFunction || bLinkFunction)
				|| !(bDeclareModuleInitializer || bStateModuleInitializer));
			TOptional<FAngelscriptCachedDeclaration> TypeDeclaration;
			if (bDeclareType || bLinkType)
			{
				TypeDeclaration = MakeTypeDeclaration(ModuleKey);
			}
			TOptional<FAngelscriptCachedDeclaration> FunctionDeclaration;
			TOptional<FAngelscriptCachedDeclaration> SecondFunctionDeclaration;
			if (bDeclareFunction || bLinkFunction)
			{
				FunctionDeclaration = MakeFunctionDeclaration(ModuleKey);
				if (bDuplicateDebugOwner)
				{
					SecondFunctionDeclaration = MakeFunctionDeclaration(
						ModuleKey, TEXT("Update"), TEXT("void Update()"));
					if (SecondFunctionDeclaration->StableKey
						< FunctionDeclaration->StableKey)
					{
						Swap(FunctionDeclaration, SecondFunctionDeclaration);
					}
					FunctionDeclaration->Slots[0].Ordinal = 0;
					SecondFunctionDeclaration->Slots[0].Ordinal = 1;
					check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
						FunctionDeclaration.GetValue(),
						FunctionDeclaration->SignatureHash,
						FunctionDeclaration->TraitsHash).IsSuccess());
					check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
						SecondFunctionDeclaration.GetValue(),
						SecondFunctionDeclaration->SignatureHash,
						SecondFunctionDeclaration->TraitsHash).IsSuccess());
				}
			}
			if (bDeclareModuleInitializer || bStateModuleInitializer)
			{
				FunctionDeclaration =
					MakeModuleInitializerDeclaration(ModuleKey);
			}
			TOptional<FAngelscriptCachedDeclaration> GlobalDeclaration;
			if (bDeclareGlobal || bStateGlobal)
			{
				GlobalDeclaration = MakeGlobalDeclaration(ModuleKey);
			}

			TArray<uint8> Payload;
			check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				MakeSourceIndex(ModuleKey, SourceFileKey,
					bCanonicalSourceOrderDiffersFromKeyOrder), Payload).IsSuccess());
			Source = Decode(EAngelscriptCacheRecordKind::SourceIndex, Payload, Budget);

			Payload.Reset();
			FAngelscriptCachedModuleInterface InterfaceValue =
				MakeModuleInterface(ModuleKey,
					bDeclareType ? TypeDeclaration : TOptional<FAngelscriptCachedDeclaration>{},
					(bDeclareFunction || bDeclareModuleInitializer)
						? FunctionDeclaration
						: TOptional<FAngelscriptCachedDeclaration>{},
					bDeclareGlobal ? GlobalDeclaration
						: TOptional<FAngelscriptCachedDeclaration>{});
			if (bDeclareFunction && SecondFunctionDeclaration.IsSet())
			{
				InterfaceValue.Declarations.Add(
					SecondFunctionDeclaration.GetValue());
				check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
					InterfaceValue, InterfaceValue.InterfaceAbi).IsSuccess());
			}
			check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				InterfaceValue, Payload).IsSuccess());
			Interface = Decode(
				EAngelscriptCacheRecordKind::ModuleInterface, Payload, Budget);

			if (bLinkType)
			{
				const FAngelscriptStableTypeKey TypeKey{
					TypeDeclaration->StableKey};
				Payload.Reset();
				check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
					MakeMinimalClassSchema(ModuleKey, TypeKey), Payload).IsSuccess());
				Type = Decode(
					EAngelscriptCacheRecordKind::TypeSchema, Payload, Budget);
			}

			if (bLinkFunction)
			{
				TOptional<FAngelscriptCacheRecordId> DebugRecordId;
				TOptional<FAngelscriptHash256> DebugHash;
				if (bLinkDebug)
				{
					const TOptional<FAngelscriptCachedSourceFileKey> DebugSourceFileKey =
						bLinkDebugSource
							? TOptional<FAngelscriptCachedSourceFileKey>(
								bMissingDebugSource
									? FAngelscriptCachedSourceFileKey{MakeHash(0xd4)}
									: SourceFileKey)
							: TOptional<FAngelscriptCachedSourceFileKey>{};
					const FAngelscriptCachedDebugSidecar DebugValue = MakeDebugSidecar(
						Profile,
						FAngelscriptStableFunctionKey{FunctionDeclaration->StableKey},
						bWrongDebugFunctionKey, bWrongDebugProfile,
						DebugSourceFileKey);
					Payload.Reset();
					check(FAngelscriptCacheRemainingRecordArchive::SerializeDebugSidecar(
						DebugValue, Payload).IsSuccess());
					Debug = Decode(
						EAngelscriptCacheRecordKind::DebugSidecar, Payload, Budget);
					DebugRecordId = Debug.GetValue()->GetRecordId();
					DebugHash = bWrongBodyDebugHash
						? MakeHash(0xd3) : DebugValue.DebugHash;
				}
				Payload.Reset();
				check(FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
					MakeFunctionBody(ModuleKey, Profile,
						FunctionDeclaration.GetValue(), bWrongFunctionAbi,
						FunctionInvocationKind, DebugRecordId, DebugHash),
					Payload).IsSuccess());
				Function = Decode(
					EAngelscriptCacheRecordKind::FunctionBody, Payload, Budget);
				if (SecondFunctionDeclaration.IsSet())
				{
					Payload.Reset();
					check(FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
						MakeFunctionBody(ModuleKey, Profile,
							SecondFunctionDeclaration.GetValue(), false,
							FunctionInvocationKind, DebugRecordId, DebugHash),
						Payload).IsSuccess());
					SecondFunction = Decode(
						EAngelscriptCacheRecordKind::FunctionBody, Payload, Budget);
				}
			}

			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
				MakeModuleState(ModuleKey, Profile,
					bStateGlobal ? GlobalDeclaration
						: TOptional<FAngelscriptCachedDeclaration>{},
					bWrongGlobalShape,
					bStateModuleInitializer ? FunctionDeclaration
						: TOptional<FAngelscriptCachedDeclaration>{},
					bWrongInitializerAbi), Payload).IsSuccess());
			State = Decode(EAngelscriptCacheRecordKind::ModuleState, Payload, Budget);

			FAngelscriptCachedModuleSnapshot SnapshotValue;
			SnapshotValue.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion;
			SnapshotValue.ModuleKey = ModuleKey;
			SnapshotValue.ModuleInterface = {
				ModuleKey, Interface.GetValue()->GetRecordId()};
			if (bLinkType)
			{
				SnapshotValue.TypeSchemas.Add({
					FAngelscriptStableTypeKey{TypeDeclaration->StableKey},
					Type.GetValue()->GetRecordId()});
			}
			SnapshotValue.ModuleState = {ModuleKey, State.GetValue()->GetRecordId()};
			if (bLinkFunction)
			{
				SnapshotValue.FunctionBodies.Add({
					FAngelscriptStableFunctionKey{FunctionDeclaration->StableKey},
					Function.GetValue()->GetRecordId()});
				if (SecondFunctionDeclaration.IsSet())
				{
					SnapshotValue.FunctionBodies.Add({
						FAngelscriptStableFunctionKey{
							SecondFunctionDeclaration->StableKey},
						SecondFunction.GetValue()->GetRecordId()});
				}
			}
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
				SnapshotValue, Payload).IsSuccess());
			Snapshot = Decode(
				EAngelscriptCacheRecordKind::ModuleSnapshot, Payload, Budget);
		}

		explicit FFixture(const EGlobalFixtureKind Kind)
			: FFixture(false, false, false, false, false,
				EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
				false, false, false, false, false, false, false,
				Kind != EGlobalFixtureKind::UndeclaredState,
				Kind != EGlobalFixtureKind::MissingState,
				Kind == EGlobalFixtureKind::WrongShape)
		{
		}

		explicit FFixture(const EModuleInitializerFixtureKind Kind)
			: FFixture(false, false, false, false, false,
				EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
				false, false, false, false, false, false, false,
				false, false, false,
				Kind != EModuleInitializerFixtureKind::UndeclaredState,
				Kind != EModuleInitializerFixtureKind::MissingState,
				Kind == EModuleInitializerFixtureKind::WrongActionAbi)
		{
		}

		explicit FFixture(const ESourceFixtureKind)
			: FFixture(false, false, true, true, false,
				EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
				true, false, false, false, true, false, false,
				false, false, false, false, false, false, true)
		{
		}
	};

	class FNoCurrentSymbols final : public IAngelscriptCacheCurrentSymbolResolver
	{
	public:
		virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
			EAngelscriptCacheReferenceKind,
			const FAngelscriptHash256&) const override
		{
			return {};
		}
	};

	class FNoCurrentLayouts final : public IAngelscriptCacheCurrentLayoutResolver
	{
	public:
		virtual TOptional<FAngelscriptCacheResolvedDataTypeLayout> ResolveDataTypeLayout(
			const FAngelscriptCachedDataType&,
			const IAngelscriptCacheProspectiveTypeLayoutView&) const override
		{
			return {};
		}

		virtual TOptional<FAngelscriptCacheResolvedTypeLayoutInput> ResolveTypeLayoutInput(
			EAngelscriptCachedTypeLayoutInputKind,
			EAngelscriptCacheReferenceKind,
			const FAngelscriptHash256&) const override
		{
			return {};
		}
	};

	class FDeterministicOpaquePayloads final
		: public IAngelscriptCacheOpaquePayloadValidator
	{
	public:
		mutable uint32 CallCount = 0;
		mutable uint64 LastOwnedSlackBytes = 0;
		bool bReturnWrongHash = false;
		bool bReturnWrongDebugHash = false;
		bool bFail = false;
		bool bReserveOwnedSlack = false;
		bool bOmitDebugSources = false;
		TOptional<FAngelscriptHash256> InitializerHashToReturn;
		TArray<FAngelscriptCachedDebugSourceReference> DebugSourcesToReturn;

		virtual FAngelscriptCacheValidationResult Validate(
			const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
			const FAngelscriptCacheReadLimits&,
			FAngelscriptCacheReadBudget&,
			IAngelscriptCacheCandidateChargeSink& GraphCandidate,
			FAngelscriptCacheOpaquePayloadSummary& OutSummary) const override
		{
			OutSummary = {};
			++CallCount;
			if (bFail)
			{
				return FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::OpaquePayloadMalformed,
					EAngelscriptCacheRecordKind::FunctionBody,
					EAngelscriptCacheValidationStage::OpaqueCodec, 0);
			}
			if (Request.Kind == EAngelscriptCacheOpaquePayloadKind::Debug)
			{
				OutSummary.ValidatedPayloadHash = bReturnWrongDebugHash
					? MakeHash(0xf2)
					: FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
						{}, Request.CanonicalPayload).Debug;
				if (!bOmitDebugSources && !DebugSourcesToReturn.IsEmpty())
				{
					int32 ReservedCapacity = 0;
					uint64 RetainedBytes = 0;
					if (!TryCalculateReserveBytes<
						FAngelscriptCachedDebugSourceReference>(
							DebugSourcesToReturn.Num(), ReservedCapacity,
							RetainedBytes))
					{
						return FAngelscriptCacheValidationResult::AtStage(
							EAngelscriptCacheValidationError::Overflow,
							EAngelscriptCacheRecordKind::DebugSidecar,
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
							EAngelscriptCacheRecordKind::DebugSidecar,
							EAngelscriptCacheValidationStage::OpaqueCodec, 0);
					}
					OutSummary.ExactDebugSources.Reserve(ReservedCapacity);
					for (const FAngelscriptCachedDebugSourceReference& Source
						: DebugSourcesToReturn)
					{
						OutSummary.ExactDebugSources.Add(Source);
					}
					if (static_cast<uint64>(
						OutSummary.ExactDebugSources.GetAllocatedSize()) != RetainedBytes)
					{
						return FAngelscriptCacheValidationResult::AtStage(
							EAngelscriptCacheValidationError::Overflow,
							EAngelscriptCacheRecordKind::DebugSidecar,
							EAngelscriptCacheValidationStage::OpaqueCodec, 0);
					}
				}
			}
			else if (Request.Kind
				== EAngelscriptCacheOpaquePayloadKind::InitializerExecution)
			{
				check(InitializerHashToReturn.IsSet());
				OutSummary.ValidatedPayloadHash = bReturnWrongHash
					? MakeHash(0xf1) : InitializerHashToReturn.GetValue();
			}
			else
			{
				OutSummary.ValidatedPayloadHash = bReturnWrongHash
					? MakeHash(0xf1)
					: FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
						Request.CanonicalPayload, {}).Execution;
			}
			LastOwnedSlackBytes = 0;
			if (bReserveOwnedSlack)
			{
				int32 ReservedCapacity = 0;
				if (!TryCalculateReserveBytes<FAngelscriptCacheOwnedOpaqueBytes>(
					1, ReservedCapacity, LastOwnedSlackBytes))
				{
					return FAngelscriptCacheValidationResult::AtStage(
						EAngelscriptCacheValidationError::Overflow,
						EAngelscriptCacheRecordKind::FunctionBody,
						EAngelscriptCacheValidationStage::OpaqueCodec, 0);
				}
				const EAngelscriptCacheCandidateChargeResult Charge =
					GraphCandidate.TryExtend(LastOwnedSlackBytes);
				if (Charge != EAngelscriptCacheCandidateChargeResult::Success)
				{
					return FAngelscriptCacheValidationResult::AtStage(
						Charge == EAngelscriptCacheCandidateChargeResult::BudgetExceeded
							? EAngelscriptCacheValidationError::BudgetExceeded
							: EAngelscriptCacheValidationError::Overflow,
						EAngelscriptCacheRecordKind::FunctionBody,
						EAngelscriptCacheValidationStage::OpaqueCodec, 0);
				}
				OutSummary.OwnedCanonicalBytes.Reserve(ReservedCapacity);
				if (static_cast<uint64>(
					OutSummary.OwnedCanonicalBytes.GetAllocatedSize())
					!= LastOwnedSlackBytes)
				{
					return FAngelscriptCacheValidationResult::AtStage(
						EAngelscriptCacheValidationError::Overflow,
						EAngelscriptCacheRecordKind::FunctionBody,
						EAngelscriptCacheValidationStage::OpaqueCodec, 0);
				}
			}
			return {};
		}
	};

	struct FContextFixture
	{
		FNoCurrentSymbols Symbols;
		FNoCurrentLayouts Layouts;
		FDeterministicOpaquePayloads Opaque;

		FAngelscriptCacheModuleGraphValidationContext Make(const FFixture& Fixture)
		{
			Opaque.DebugSourcesToReturn.Reset();
			Opaque.InitializerHashToReturn.Reset();
			if (Fixture.Debug.IsSet())
			{
				Opaque.DebugSourcesToReturn =
					Fixture.Debug.GetValue()->TryGetDebugSidecar()->Sources;
			}
			if (Fixture.State.IsSet()
				&& !Fixture.State.GetValue()->TryGetModuleState()
					->Initializers.IsEmpty())
			{
				Opaque.InitializerHashToReturn = Fixture.State.GetValue()
					->TryGetModuleState()->Initializers[0]
					.InitializerExecutionHash;
			}
			FAngelscriptCacheModuleGraphValidationContext Context;
			Context.SelectedProfile = Fixture.Profile;
			Context.SelectedSourceSnapshot =
				Fixture.Source.GetValue()->TryGetSourceIndex()->SourceSnapshot;
			Context.SourceIndex = &Fixture.Source.GetValue().Get();
			Context.CurrentSymbols = &Symbols;
			Context.CurrentLayouts = &Layouts;
			Context.OpaquePayloads = &Opaque;
			return Context;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheModuleSnapshotGraphTests,
	"Angelscript.TestModule.Cache.Archive.ModuleSnapshotGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MinimalReachableGraphPublishesOnlyRootInterfaceAndState)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Fixture;
		FContextFixture ContextFixture;
		const FAngelscriptCacheModuleGraphValidationContext Context =
			ContextFixture.Make(Fixture);
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Fixture.Source.GetValue(),
			Fixture.State.GetValue(),
			Fixture.Snapshot.GetValue(),
			Fixture.Interface.GetValue()};

		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Fixture.Snapshot.GetValue()->GetRecordId(), Pool, Context,
			FAngelscriptCacheReadLimits{}, Fixture.Budget, Graph).IsSuccess()));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(int32(3), Graph.GetReachableRecords().Num()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleSnapshot,
			Graph.GetReachableRecords()[0]->GetRecordId().Kind));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface,
			Graph.GetReachableRecords()[1]->GetRecordId().Kind));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleState,
			Graph.GetReachableRecords()[2]->GetRecordId().Kind));
		ASSERT_THAT(IsTrue(Graph.FindRecordOrdinal(
			Fixture.Source.GetValue()->GetRecordId()).IsSet() == false));
		ASSERT_THAT(AreEqual(uint32(0), Graph.FindRecordOrdinal(
			Fixture.Snapshot.GetValue()->GetRecordId()).GetValue()));
		ASSERT_THAT(AreEqual(uint64(0),
			Fixture.Budget.GetTemporaryResidentDecodedBytes()));

		TestRunner->AddInfo(TEXT(
			"Cache V2 minimal ModuleSnapshot graph retained exactly root/interface/state; SourceIndex remained context-only."));
	}

	TEST_METHOD(ContextAndMissingChildFailuresClearTheWholePublishedGraph)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Fixture;
		FContextFixture ContextFixture;
		FAngelscriptCacheModuleGraphValidationContext Context =
			ContextFixture.Make(Fixture);
		TArray<FAngelscriptDecodedCacheRecordHandle> CompletePool = {
			Fixture.Snapshot.GetValue(), Fixture.Interface.GetValue(),
			Fixture.State.GetValue()};
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Fixture.Snapshot.GetValue()->GetRecordId(), CompletePool, Context,
			FAngelscriptCacheReadLimits{}, Fixture.Budget, Graph).IsSuccess()));

		Context.SourceIndex = nullptr;
		FAngelscriptCacheValidationResult Result = ValidateModuleSnapshotGraph(
			Fixture.Snapshot.GetValue()->GetRecordId(), CompletePool, Context,
			FAngelscriptCacheReadLimits{}, Fixture.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ContextMismatch,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ModuleGraph,
			Result.Stage));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		Context = ContextFixture.Make(Fixture);
		TArray<FAngelscriptDecodedCacheRecordHandle> MissingStatePool = {
			Fixture.Snapshot.GetValue(), Fixture.Interface.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			Fixture.Snapshot.GetValue()->GetRecordId(), MissingStatePool, Context,
			FAngelscriptCacheReadLimits{}, Fixture.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingRecord,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ModuleGraph,
			Result.Stage));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			Fixture.Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(RequiredTypeSchemaPublishesAStableTypeOrdinal)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Fixture(true, true);
		FContextFixture ContextFixture;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Fixture.Type.GetValue(), Fixture.Snapshot.GetValue(),
			Fixture.State.GetValue(), Fixture.Interface.GetValue()};
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Fixture.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
			Fixture.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(4), Graph.GetReachableRecords().Num()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::TypeSchema,
			Graph.GetReachableRecords()[3]->GetRecordId().Kind));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetTypeOrdinals().Num()));
		ASSERT_THAT(IsTrue(Graph.GetTypeOrdinals()[0].TypeKey
			== Fixture.Type.GetValue()->TryGetTypeSchema()->TypeKey));
		ASSERT_THAT(AreEqual(uint32(3),
			Graph.GetTypeOrdinals()[0].TypeSchemaRecordOrdinal));
		ASSERT_THAT(AreEqual(uint64(0),
			Fixture.Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(TypeSchemaLinksHaveExactDeclarationCoverageAndReachability)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FContextFixture ContextFixture;
		FAngelscriptValidatedModuleGraph Graph;

		FFixture MissingRecord(true, true);
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			MissingRecord.Snapshot.GetValue(), MissingRecord.Interface.GetValue(),
			MissingRecord.State.GetValue()};
		FAngelscriptCacheValidationResult Result = ValidateModuleSnapshotGraph(
			MissingRecord.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(MissingRecord), FAngelscriptCacheReadLimits{},
			MissingRecord.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingRecord,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture MissingCoverage(true, false);
		Pool = {MissingCoverage.Snapshot.GetValue(),
			MissingCoverage.Interface.GetValue(), MissingCoverage.State.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			MissingCoverage.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(MissingCoverage), FAngelscriptCacheReadLimits{},
			MissingCoverage.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingCoverage,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture UndeclaredType(false, true);
		Pool = {UndeclaredType.Snapshot.GetValue(),
			UndeclaredType.Interface.GetValue(), UndeclaredType.State.GetValue(),
			UndeclaredType.Type.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			UndeclaredType.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(UndeclaredType), FAngelscriptCacheReadLimits{},
			UndeclaredType.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UndeclaredEntity,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			UndeclaredType.Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(RequiredFunctionBodyPublishesStableFunctionAndOpaqueOrdinals)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Fixture(false, false, true, true);
		FContextFixture ContextFixture;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Fixture.Function.GetValue(), Fixture.State.GetValue(),
			Fixture.Snapshot.GetValue(), Fixture.Interface.GetValue()};
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Fixture.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
			Fixture.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), ContextFixture.Opaque.CallCount));
		ASSERT_THAT(AreEqual(int32(4), Graph.GetReachableRecords().Num()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::FunctionBody,
			Graph.GetReachableRecords()[3]->GetRecordId().Kind));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetFunctionOrdinals().Num()));
		const FAngelscriptCacheValidatedFunctionOrdinal& Function =
			Graph.GetFunctionOrdinals()[0];
		ASSERT_THAT(IsTrue(Function.FunctionKey
			== Fixture.Function.GetValue()->TryGetFunctionBody()->Identity.FunctionKey));
		ASSERT_THAT(AreEqual(uint32(0), Function.DeclarationOrdinal));
		ASSERT_THAT(IsTrue(Function.BodyRecordOrdinal.IsSet()));
		ASSERT_THAT(AreEqual(uint32(3), Function.BodyRecordOrdinal.GetValue()));
		ASSERT_THAT(IsFalse(Function.DebugRecordOrdinal.IsSet()));
		ASSERT_THAT(IsTrue(Function.BodySummaryOrdinal.IsSet()));
		ASSERT_THAT(AreEqual(uint32(0), Function.BodySummaryOrdinal.GetValue()));
		ASSERT_THAT(IsFalse(Function.DebugSummaryOrdinal.IsSet()));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetOpaqueSummaries().Num()));
		ASSERT_THAT(IsTrue(Graph.GetOpaqueSummaries()[0].ValidatedPayloadHash
			== Fixture.Function.GetValue()->TryGetFunctionBody()
				->Identity.Content.Execution));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetOpaqueOwnerOrdinals().Num()));
		ASSERT_THAT(AreEqual(uint64(0),
			Fixture.Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(PresentDebugPublishesBodyOwnedSidecarAndSummary)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Fixture(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction, true);
		FContextFixture ContextFixture;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Fixture.Debug.GetValue(), Fixture.Function.GetValue(),
			Fixture.State.GetValue(), Fixture.Snapshot.GetValue(),
			Fixture.Interface.GetValue()};
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Fixture.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
			Fixture.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(2), ContextFixture.Opaque.CallCount));
		ASSERT_THAT(AreEqual(int32(5), Graph.GetReachableRecords().Num()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::DebugSidecar,
			Graph.GetReachableRecords()[4]->GetRecordId().Kind));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetFunctionOrdinals().Num()));
		const FAngelscriptCacheValidatedFunctionOrdinal& Function =
			Graph.GetFunctionOrdinals()[0];
		ASSERT_THAT(IsTrue(Function.DebugRecordOrdinal.IsSet()));
		ASSERT_THAT(AreEqual(uint32(4), Function.DebugRecordOrdinal.GetValue()));
		ASSERT_THAT(IsTrue(Function.DebugSummaryOrdinal.IsSet()));
		ASSERT_THAT(AreEqual(uint32(1), Function.DebugSummaryOrdinal.GetValue()));
		ASSERT_THAT(AreEqual(int32(2), Graph.GetOpaqueSummaries().Num()));
		ASSERT_THAT(IsTrue(Graph.GetOpaqueSummaries()[1].ValidatedPayloadHash
			== Fixture.Debug.GetValue()->TryGetDebugSidecar()->DebugHash));
		ASSERT_THAT(AreEqual(int32(2), Graph.GetOpaqueOwnerOrdinals().Num()));
		ASSERT_THAT(AreEqual(uint64(0),
			Fixture.Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(DebugReachabilityOwnershipProfileHashAndCodecFailuresAreAtomic)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		FFixture Missing(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction, true);
		FContextFixture MissingContext;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Missing.Function.GetValue(), Missing.State.GetValue(),
			Missing.Snapshot.GetValue(), Missing.Interface.GetValue()};
		FAngelscriptCacheValidationResult Result = ValidateModuleSnapshotGraph(
			Missing.Snapshot.GetValue()->GetRecordId(), Pool,
			MissingContext.Make(Missing), FAngelscriptCacheReadLimits{},
			Missing.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingRecord,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(0), MissingContext.Opaque.CallCount));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture WrongOwner(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			true, true);
		FContextFixture WrongOwnerContext;
		Pool = {WrongOwner.Debug.GetValue(), WrongOwner.Function.GetValue(),
			WrongOwner.State.GetValue(), WrongOwner.Snapshot.GetValue(),
			WrongOwner.Interface.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			WrongOwner.Snapshot.GetValue()->GetRecordId(), Pool,
			WrongOwnerContext.Make(WrongOwner), FAngelscriptCacheReadLimits{},
			WrongOwner.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DebugLinkMismatch,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(2), WrongOwnerContext.Opaque.CallCount));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture WrongProfile(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			true, false, true);
		FContextFixture WrongProfileContext;
		Pool = {WrongProfile.Debug.GetValue(), WrongProfile.Function.GetValue(),
			WrongProfile.State.GetValue(), WrongProfile.Snapshot.GetValue(),
			WrongProfile.Interface.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			WrongProfile.Snapshot.GetValue()->GetRecordId(), Pool,
			WrongProfileContext.Make(WrongProfile), FAngelscriptCacheReadLimits{},
			WrongProfile.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::ProfileGraphMismatch,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture WrongHash(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			true, false, false, true);
		FContextFixture WrongHashContext;
		Pool = {WrongHash.Debug.GetValue(), WrongHash.Function.GetValue(),
			WrongHash.State.GetValue(), WrongHash.Snapshot.GetValue(),
			WrongHash.Interface.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			WrongHash.Snapshot.GetValue()->GetRecordId(), Pool,
			WrongHashContext.Make(WrongHash), FAngelscriptCacheReadLimits{},
			WrongHash.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DebugLinkMismatch,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture WrongCodecHash(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction, true);
		FContextFixture WrongCodecContext;
		WrongCodecContext.Opaque.bReturnWrongDebugHash = true;
		Pool = {WrongCodecHash.Debug.GetValue(), WrongCodecHash.Function.GetValue(),
			WrongCodecHash.State.GetValue(), WrongCodecHash.Snapshot.GetValue(),
			WrongCodecHash.Interface.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			WrongCodecHash.Snapshot.GetValue()->GetRecordId(), Pool,
			WrongCodecContext.Make(WrongCodecHash), FAngelscriptCacheReadLimits{},
			WrongCodecHash.Budget, Graph);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			WrongCodecHash.Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(DebugSourcesRequireExactCodecRowsAndSourceIndexMembership)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Valid(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			true, false, false, false, true);
		FContextFixture ValidContext;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Valid.Debug.GetValue(), Valid.Function.GetValue(), Valid.State.GetValue(),
			Valid.Snapshot.GetValue(), Valid.Interface.GetValue()};
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Valid.Snapshot.GetValue()->GetRecordId(), Pool,
			ValidContext.Make(Valid), FAngelscriptCacheReadLimits{},
			Valid.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(1),
			Graph.GetOpaqueSummaries()[1].ExactDebugSources.Num()));
		ASSERT_THAT(IsTrue(Graph.GetOpaqueSummaries()[1].ExactDebugSources[0]
			.SourceFileKey.Hash == Valid.SourceFileKey.Hash));

		FFixture ExistingFileInCanonicalNonKeyOrder(
			FFixture::ESourceFixtureKind::ExistingFileInCanonicalNonKeyOrder);
		FContextFixture ExistingFileContext;
		Pool = {ExistingFileInCanonicalNonKeyOrder.Debug.GetValue(),
			ExistingFileInCanonicalNonKeyOrder.Function.GetValue(),
			ExistingFileInCanonicalNonKeyOrder.State.GetValue(),
			ExistingFileInCanonicalNonKeyOrder.Snapshot.GetValue(),
			ExistingFileInCanonicalNonKeyOrder.Interface.GetValue()};
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			ExistingFileInCanonicalNonKeyOrder.Snapshot.GetValue()->GetRecordId(),
			Pool, ExistingFileContext.Make(ExistingFileInCanonicalNonKeyOrder),
			FAngelscriptCacheReadLimits{},
			ExistingFileInCanonicalNonKeyOrder.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(16),
			ExistingFileInCanonicalNonKeyOrder.Source.GetValue()
				->TryGetSourceIndex()->Files.Num()));

		FFixture MissingFile(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			true, false, false, false, true, true);
		FContextFixture MissingFileContext;
		Pool = {MissingFile.Debug.GetValue(), MissingFile.Function.GetValue(),
			MissingFile.State.GetValue(), MissingFile.Snapshot.GetValue(),
			MissingFile.Interface.GetValue()};
		FAngelscriptCacheValidationResult Result = ValidateModuleSnapshotGraph(
			MissingFile.Snapshot.GetValue()->GetRecordId(), Pool,
			MissingFileContext.Make(MissingFile), FAngelscriptCacheReadLimits{},
			MissingFile.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DebugSourceMismatch,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture MissingCodecRow(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			true, false, false, false, true);
		FContextFixture MissingCodecRowContext;
		MissingCodecRowContext.Opaque.bOmitDebugSources = true;
		Pool = {MissingCodecRow.Debug.GetValue(),
			MissingCodecRow.Function.GetValue(), MissingCodecRow.State.GetValue(),
			MissingCodecRow.Snapshot.GetValue(),
			MissingCodecRow.Interface.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			MissingCodecRow.Snapshot.GetValue()->GetRecordId(), Pool,
			MissingCodecRowContext.Make(MissingCodecRow),
			FAngelscriptCacheReadLimits{}, MissingCodecRow.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DebugSourceMismatch,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			MissingCodecRow.Budget.GetTemporaryResidentDecodedBytes()));

		TestRunner->AddInfo(TEXT(
			"Cache V2 Debug sources: exact codec rows resolve through single-file and canonical multi-file SourceIndexes even when canonical file order differs from SourceFileKey order; missing file membership and missing codec rows remain atomically rejected."));
	}

	TEST_METHOD(OneDebugSidecarRecordHasExactlyOneFunctionBodyOwner)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Published(false, false, true, true, false);
		FContextFixture PublishedContext;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Published.Function.GetValue(), Published.State.GetValue(),
			Published.Snapshot.GetValue(), Published.Interface.GetValue()};
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Published.Snapshot.GetValue()->GetRecordId(), Pool,
			PublishedContext.Make(Published), FAngelscriptCacheReadLimits{},
			Published.Budget, Graph).IsSuccess()));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));

		FFixture Duplicate(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			true, false, false, false, false, false, true);
		FContextFixture DuplicateContext;
		Pool = {Duplicate.Debug.GetValue(), Duplicate.Function.GetValue(),
			Duplicate.SecondFunction.GetValue(), Duplicate.State.GetValue(),
			Duplicate.Snapshot.GetValue(), Duplicate.Interface.GetValue()};
		const FAngelscriptCacheValidationResult Result = ValidateModuleSnapshotGraph(
			Duplicate.Snapshot.GetValue()->GetRecordId(), Pool,
			DuplicateContext.Make(Duplicate), FAngelscriptCacheReadLimits{},
			Duplicate.Budget, Graph);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::DuplicateDebugOwner, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ModuleGraph,
			Result.Stage));
		ASSERT_THAT(AreEqual(uint32(3), DuplicateContext.Opaque.CallCount));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			Duplicate.Budget.GetTemporaryResidentDecodedBytes()));

		TestRunner->AddInfo(TEXT(
			"Cache V2 Debug ownership: two FunctionBodies sharing one sidecar decoded two execution payloads and the unique Debug payload once, then rejected DuplicateDebugOwner atomically."));
	}

	TEST_METHOD(DefaultGlobalStateHasExactInterfaceCoverageAndStableOrdinal)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Valid(FFixture::EGlobalFixtureKind::Valid);
		FContextFixture ValidContext;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Valid.State.GetValue(), Valid.Snapshot.GetValue(),
			Valid.Interface.GetValue()};
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Valid.Snapshot.GetValue()->GetRecordId(), Pool,
			ValidContext.Make(Valid), FAngelscriptCacheReadLimits{},
			Valid.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetGlobalOrdinals().Num()));
		ASSERT_THAT(IsTrue(Graph.GetGlobalOrdinals()[0].GlobalKey
			== Valid.State.GetValue()->TryGetModuleState()
				->OrderedGlobals[0].GlobalKey));
		ASSERT_THAT(AreEqual(uint32(0),
			Graph.GetGlobalOrdinals()[0].ModuleStateGlobalOrdinal));

		const auto RequireAtomicGlobalFailure = [&](FFixture& Fixture)
		{
			FContextFixture ContextFixture;
			Pool = {Fixture.State.GetValue(), Fixture.Snapshot.GetValue(),
				Fixture.Interface.GetValue()};
			const FAngelscriptCacheValidationResult Result =
				ValidateModuleSnapshotGraph(
					Fixture.Snapshot.GetValue()->GetRecordId(), Pool,
					ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
					Fixture.Budget, Graph);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				Result.Error));
			ASSERT_THAT(IsTrue(Graph.IsEmpty()));
			ASSERT_THAT(AreEqual(uint64(0),
				Fixture.Budget.GetTemporaryResidentDecodedBytes()));
		};

		FFixture Missing(FFixture::EGlobalFixtureKind::MissingState);
		RequireAtomicGlobalFailure(Missing);
		FFixture Undeclared(FFixture::EGlobalFixtureKind::UndeclaredState);
		RequireAtomicGlobalFailure(Undeclared);
		FFixture WrongShape(FFixture::EGlobalFixtureKind::WrongShape);
		RequireAtomicGlobalFailure(WrongShape);

		TestRunner->AddInfo(TEXT(
			"Cache V2 ModuleState global coverage: one default int global published a stable GlobalKey->StorageOrdinal; missing, undeclared and shape-mismatched state all rolled back."));
	}

	TEST_METHOD(ModuleInitializerHasExactDeclarationActionAndOpaqueOwnership)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Valid(FFixture::EModuleInitializerFixtureKind::Valid);
		FContextFixture ValidContext;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Valid.State.GetValue(), Valid.Snapshot.GetValue(),
			Valid.Interface.GetValue()};
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Valid.Snapshot.GetValue()->GetRecordId(), Pool,
			ValidContext.Make(Valid), FAngelscriptCacheReadLimits{},
			Valid.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), ValidContext.Opaque.CallCount));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetFunctionOrdinals().Num()));
		ASSERT_THAT(IsFalse(
			Graph.GetFunctionOrdinals()[0].BodyRecordOrdinal.IsSet()));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetInitializerOrdinals().Num()));
		const FAngelscriptCacheValidatedInitializerOrdinal& Initializer =
			Graph.GetInitializerOrdinals()[0];
		const FAngelscriptCachedModuleState& State =
			*Valid.State.GetValue()->TryGetModuleState();
		ASSERT_THAT(IsTrue(Initializer.InitializerKey
			== State.Initializers[0].InitializerKey));
		ASSERT_THAT(AreEqual(uint32(0), Initializer.UnitOrdinal));
		ASSERT_THAT(AreEqual(uint32(0), Initializer.ExecuteActionOrdinal));
		ASSERT_THAT(AreEqual(uint32(0), Initializer.SummaryOrdinal));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetOpaqueSummaries().Num()));
		ASSERT_THAT(IsTrue(Graph.GetOpaqueSummaries()[0].ValidatedPayloadHash
			== State.Initializers[0].InitializerExecutionHash));
		ASSERT_THAT(AreEqual(int32(1), Graph.GetOpaqueOwnerOrdinals().Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheOpaquePayloadKind::InitializerExecution,
			Graph.GetOpaqueOwnerOrdinals()[0].Owner.Kind));
		ASSERT_THAT(IsTrue(Graph.GetOpaqueOwnerOrdinals()[0].Owner.OwnerKey
			== State.Initializers[0].InitializerKey.Hash));

		const auto RequireAtomicInitializerFailure =
			[&](FFixture& Fixture,
				const EAngelscriptCacheValidationError ExpectedError,
				const bool bWrongCodecHash = false)
		{
			FContextFixture ContextFixture;
			ContextFixture.Opaque.bReturnWrongHash = bWrongCodecHash;
			Pool = {Fixture.State.GetValue(), Fixture.Snapshot.GetValue(),
				Fixture.Interface.GetValue()};
			const FAngelscriptCacheValidationResult Result =
				ValidateModuleSnapshotGraph(
					Fixture.Snapshot.GetValue()->GetRecordId(), Pool,
					ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
					Fixture.Budget, Graph);
			ASSERT_THAT(AreEqual(ExpectedError, Result.Error));
			ASSERT_THAT(IsTrue(Graph.IsEmpty()));
			ASSERT_THAT(AreEqual(uint64(0),
				Fixture.Budget.GetTemporaryResidentDecodedBytes()));
		};

		FFixture Missing(
			FFixture::EModuleInitializerFixtureKind::MissingState);
		RequireAtomicInitializerFailure(Missing,
			EAngelscriptCacheValidationError::InitializerOwnershipMismatch);
		FFixture Undeclared(
			FFixture::EModuleInitializerFixtureKind::UndeclaredState);
		RequireAtomicInitializerFailure(Undeclared,
			EAngelscriptCacheValidationError::InitializerOwnershipMismatch);
		FFixture WrongAbi(
			FFixture::EModuleInitializerFixtureKind::WrongActionAbi);
		RequireAtomicInitializerFailure(WrongAbi,
			EAngelscriptCacheValidationError::InitializerOwnershipMismatch);
		FFixture WrongCodec(
			FFixture::EModuleInitializerFixtureKind::Valid);
		RequireAtomicInitializerFailure(WrongCodec,
			EAngelscriptCacheValidationError::OpaquePayloadHashMismatch, true);

		TestRunner->AddInfo(TEXT(
			"Cache V2 module initializer: declaration, unit and ExecuteInitializer action had exact stable-key/ABI ownership; its opaque payload ran first and all mismatches rolled back atomically."));
	}

	TEST_METHOD(OpaqueSummaryCandidateBudgetIsExactAndLateFailureRollsBack)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FFixture Measurement(false, false, true, true);
		FContextFixture MeasurementContext;
		MeasurementContext.Opaque.bReserveOwnedSlack = true;
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			Measurement.Function.GetValue(), Measurement.State.GetValue(),
			Measurement.Snapshot.GetValue(), Measurement.Interface.GetValue()};
		const uint64 BaselineDecoded = Measurement.Budget.GetDecodedBytes();
		const uint64 BaselineResident =
			Measurement.Budget.GetResidentDecodedBytes();
		FAngelscriptValidatedModuleGraph MeasurementGraph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Measurement.Snapshot.GetValue()->GetRecordId(), Pool,
			MeasurementContext.Make(Measurement), FAngelscriptCacheReadLimits{},
			Measurement.Budget, MeasurementGraph).IsSuccess()));
		const uint64 ExactTotalLimit = Measurement.Budget.GetDecodedBytes();
		const uint64 ExactPeakResidentLimit =
			Measurement.Budget.GetPeakLiveResidentDecodedBytes();
		const uint64 GraphDecodedDelta = ExactTotalLimit - BaselineDecoded;
		const uint64 GraphRetainedDelta =
			Measurement.Budget.GetResidentDecodedBytes() - BaselineResident;
		ASSERT_THAT(IsTrue(MeasurementContext.Opaque.LastOwnedSlackBytes > 0));
		ASSERT_THAT(AreEqual(
			MeasurementContext.Opaque.LastOwnedSlackBytes,
			static_cast<uint64>(MeasurementGraph.GetOpaqueSummaries()[0]
				.OwnedCanonicalBytes.GetAllocatedSize())));
		ASSERT_THAT(AreEqual(int32(0), MeasurementGraph.GetOpaqueSummaries()[0]
			.OwnedCanonicalBytes.Num()));

		FFixture Exact(false, false, true, true);
		FContextFixture ExactContext;
		ExactContext.Opaque.bReserveOwnedSlack = true;
		ASSERT_THAT(AreEqual(BaselineDecoded, Exact.Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(BaselineResident,
			Exact.Budget.GetResidentDecodedBytes()));
		Pool = {Exact.Function.GetValue(), Exact.State.GetValue(),
			Exact.Snapshot.GetValue(), Exact.Interface.GetValue()};
		FAngelscriptCacheReadLimits ExactLimits;
		ExactLimits.MaxTotalDecodedBytes = ExactTotalLimit;
		ExactLimits.MaxResidentDecodedBytes = ExactPeakResidentLimit;
		FAngelscriptValidatedModuleGraph ExactGraph;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Exact.Snapshot.GetValue()->GetRecordId(), Pool,
			ExactContext.Make(Exact), ExactLimits, Exact.Budget,
			ExactGraph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint64(0),
			Exact.Budget.GetTemporaryResidentDecodedBytes()));

		FFixture ShortTotal(false, false, true, true);
		FContextFixture ShortTotalContext;
		ShortTotalContext.Opaque.bReserveOwnedSlack = true;
		Pool = {ShortTotal.Function.GetValue(), ShortTotal.State.GetValue(),
			ShortTotal.Snapshot.GetValue(), ShortTotal.Interface.GetValue()};
		FAngelscriptCacheReadLimits ShortTotalLimits;
		ShortTotalLimits.MaxTotalDecodedBytes = ExactTotalLimit - 1;
		FAngelscriptValidatedModuleGraph FailedGraph;
		FAngelscriptCacheValidationResult Result = ValidateModuleSnapshotGraph(
			ShortTotal.Snapshot.GetValue()->GetRecordId(), Pool,
			ShortTotalContext.Make(ShortTotal), ShortTotalLimits,
			ShortTotal.Budget, FailedGraph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
			Result.Error));
		ASSERT_THAT(IsTrue(FailedGraph.IsEmpty()));
		ASSERT_THAT(AreEqual(BaselineResident,
			ShortTotal.Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0),
			ShortTotal.Budget.GetTemporaryResidentDecodedBytes()));

		FFixture ShortResident(false, false, true, true);
		FContextFixture ShortResidentContext;
		ShortResidentContext.Opaque.bReserveOwnedSlack = true;
		Pool = {ShortResident.Function.GetValue(), ShortResident.State.GetValue(),
			ShortResident.Snapshot.GetValue(), ShortResident.Interface.GetValue()};
		FAngelscriptCacheReadLimits ShortResidentLimits;
		ShortResidentLimits.MaxTotalDecodedBytes = ExactTotalLimit;
		ShortResidentLimits.MaxResidentDecodedBytes = ExactPeakResidentLimit - 1;
		Result = ValidateModuleSnapshotGraph(
			ShortResident.Snapshot.GetValue()->GetRecordId(), Pool,
			ShortResidentContext.Make(ShortResident), ShortResidentLimits,
			ShortResident.Budget, FailedGraph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
			Result.Error));
		ASSERT_THAT(IsTrue(FailedGraph.IsEmpty()));
		ASSERT_THAT(AreEqual(BaselineResident,
			ShortResident.Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0),
			ShortResident.Budget.GetTemporaryResidentDecodedBytes()));

		FFixture LateFailure(false, false, true, true, true);
		FContextFixture LateFailureContext;
		LateFailureContext.Opaque.bReserveOwnedSlack = true;
		Pool = {LateFailure.Function.GetValue(), LateFailure.State.GetValue(),
			LateFailure.Snapshot.GetValue(), LateFailure.Interface.GetValue()};
		const uint64 LateBaselineResident =
			LateFailure.Budget.GetResidentDecodedBytes();
		Result = ValidateModuleSnapshotGraph(
			LateFailure.Snapshot.GetValue()->GetRecordId(), Pool,
			LateFailureContext.Make(LateFailure), FAngelscriptCacheReadLimits{},
			LateFailure.Budget, FailedGraph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::GraphAbiMismatch,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(1), LateFailureContext.Opaque.CallCount));
		ASSERT_THAT(IsTrue(FailedGraph.IsEmpty()));
		ASSERT_THAT(AreEqual(LateBaselineResident,
			LateFailure.Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0),
			LateFailure.Budget.GetTemporaryResidentDecodedBytes()));

		TestRunner->AddInfo(FString::Printf(TEXT(
			"Cache V2 graph candidate exact bytes: decoded-delta=%llu retained-delta=%llu peak-live=%llu opaque-owned-slack=%llu; exact limits passed, both one-byte-short limits and late ABI failure rolled back."),
			GraphDecodedDelta, GraphRetainedDelta, ExactPeakResidentLimit,
			MeasurementContext.Opaque.LastOwnedSlackBytes));
	}

	TEST_METHOD(FunctionBodyCoverageAbiInvocationAndOpaqueFailuresAreTypedAndAtomic)
	{
		using namespace AngelscriptCacheModuleSnapshotGraphTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		FContextFixture ContextFixture;

		FFixture MissingRecord(false, false, true, true);
		TArray<FAngelscriptDecodedCacheRecordHandle> Pool = {
			MissingRecord.Snapshot.GetValue(), MissingRecord.Interface.GetValue(),
			MissingRecord.State.GetValue()};
		FAngelscriptCacheValidationResult Result = ValidateModuleSnapshotGraph(
			MissingRecord.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(MissingRecord), FAngelscriptCacheReadLimits{},
			MissingRecord.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingRecord,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Opaque.CallCount));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture MissingCoverage(false, false, true, false);
		Pool = {MissingCoverage.Snapshot.GetValue(),
			MissingCoverage.Interface.GetValue(), MissingCoverage.State.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			MissingCoverage.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(MissingCoverage), FAngelscriptCacheReadLimits{},
			MissingCoverage.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingCoverage,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture Undeclared(false, false, false, true);
		Pool = {Undeclared.Snapshot.GetValue(), Undeclared.Interface.GetValue(),
			Undeclared.State.GetValue(), Undeclared.Function.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			Undeclared.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(Undeclared), FAngelscriptCacheReadLimits{},
			Undeclared.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UndeclaredEntity,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture WrongAbi(false, false, true, true, true);
		Pool = {WrongAbi.Snapshot.GetValue(), WrongAbi.Interface.GetValue(),
			WrongAbi.State.GetValue(), WrongAbi.Function.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			WrongAbi.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(WrongAbi), FAngelscriptCacheReadLimits{},
			WrongAbi.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::GraphAbiMismatch,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture WrongInvocation(false, false, true, true, false,
			EAngelscriptCachedFunctionInvocationKind::Method);
		Pool = {WrongInvocation.Snapshot.GetValue(),
			WrongInvocation.Interface.GetValue(), WrongInvocation.State.GetValue(),
			WrongInvocation.Function.GetValue()};
		Result = ValidateModuleSnapshotGraph(
			WrongInvocation.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(WrongInvocation), FAngelscriptCacheReadLimits{},
			WrongInvocation.Budget, Graph);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvocationKindMismatch,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));

		FFixture OpaqueFailure(false, false, true, true);
		Pool = {OpaqueFailure.Snapshot.GetValue(),
			OpaqueFailure.Interface.GetValue(), OpaqueFailure.State.GetValue(),
			OpaqueFailure.Function.GetValue()};
		ContextFixture.Opaque.bReturnWrongHash = true;
		Result = ValidateModuleSnapshotGraph(
			OpaqueFailure.Snapshot.GetValue()->GetRecordId(), Pool,
			ContextFixture.Make(OpaqueFailure), FAngelscriptCacheReadLimits{},
			OpaqueFailure.Budget, Graph);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
			Result.Error));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			OpaqueFailure.Budget.GetTemporaryResidentDecodedBytes()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
