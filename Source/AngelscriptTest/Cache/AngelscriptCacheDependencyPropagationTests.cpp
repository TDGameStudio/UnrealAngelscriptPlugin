#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDependencyPropagation.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheDependencyPropagationTests_Private
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

	static FAngelscriptCachedDataType MakeVoidType()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = EAngelscriptCachedPrimitiveType::Void;
		return Type;
	}

	static FAngelscriptCachedDataType MakeInt32Type()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
		return Type;
	}

	static FAngelscriptCachedSourceProviderKey BuildProviderKey(
		const FAngelscriptCachedSourceProvider& Provider)
	{
		FAngelscriptCachedSourceProviderKey Key;
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey({
			Provider.ProviderKind,
			Provider.CanonicalImplementationIdentity,
			Provider.IdentityFingerprint}, Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedSourceMountKey BuildMountKey(
		const FAngelscriptCachedSourceMount& Mount)
	{
		FAngelscriptCachedSourceMountKey Key;
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey({
			Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey},
			Key).IsSuccess());
		return Key;
	}

	static FAngelscriptCachedSourceFileKey BuildSourceFileKey(
		const FAngelscriptCachedSourceFile& File)
	{
		FAngelscriptCachedSourceFileKey Key;
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey({
			File.SourceKind,
			File.MountKey,
			File.ProviderKey,
			File.RelativeLogicalPath,
			File.GeneratedSourceKey}, Key).IsSuccess());
		return Key;
	}

	struct FModuleSpec
	{
		FAngelscriptStableModuleKey ModuleKey;
		FString ModuleName;
		FString FunctionName;
		uint32 FunctionTraitFlags = 0;
		uint8 ExecutionSeed = 0;
		TArray<FAngelscriptCacheSemanticDependency> Dependencies;
		TArray<FAngelscriptCachedDeclaration> AdditionalDeclarations;
		TArray<FAngelscriptCachedTypeSchema> TypeSchemas;
		TArray<FAngelscriptCachedImportDeclaration> Imports;
		TArray<FAngelscriptCacheSemanticDependency> InterfaceDependencies;
		TOptional<int32> PureConstantValue;
		TOptional<uint8> ModuleInitializerSeed;
		TArray<FAngelscriptCacheSemanticDependency> StateDependencies;
		TArray<FAngelscriptCacheSemanticDependency>
			InitializationActionDependencies;
	};

	static FModuleSpec MakeModule(
		const uint8 ModuleSeed,
		const TCHAR* ModuleName,
		const TCHAR* FunctionName,
		const uint8 ExecutionSeed,
		const uint32 TraitFlags = 0)
	{
		FModuleSpec Spec;
		Spec.ModuleKey = FAngelscriptStableModuleKey{MakeHash(ModuleSeed)};
		Spec.ModuleName = ModuleName;
		Spec.FunctionName = FunctionName;
		Spec.FunctionTraitFlags = TraitFlags;
		Spec.ExecutionSeed = ExecutionSeed;
		return Spec;
	}

	static FAngelscriptCachedDeclaration MakeFunctionDeclaration(
		const FModuleSpec& Spec)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Function;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::GlobalFunction;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Required;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = Spec.ModuleKey.Hash;
		Declaration.ModuleKey = Spec.ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = Spec.FunctionName;
		Declaration.CanonicalDeclaration = FString::Printf(
			TEXT("void %s()"), *Spec.FunctionName);
		Declaration.DeclaredType = MakeVoidType();
		Declaration.TraitFlags = Spec.FunctionTraitFlags;
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
			Declaration,
			Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakeTypeDeclaration(
		const FModuleSpec& Spec,
		const TCHAR* Name)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Type;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::Struct;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Required;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = Spec.ModuleKey.Hash;
		Declaration.ModuleKey = Spec.ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = Name;
		Declaration.CanonicalDeclaration = FString::Printf(TEXT("struct %s"), Name);
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration, 0});
		FAngelscriptTypeIdentityDescriptor Identity;
		Identity.ModuleKey = Spec.ModuleKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakePropertyDeclaration(
		const FModuleSpec& Spec,
		const FAngelscriptCachedDeclaration& Owner,
		const TCHAR* Name)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Property;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::Property;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
		Declaration.OwnerKey = Owner.StableKey;
		Declaration.ModuleKey = Spec.ModuleKey;
		Declaration.CanonicalNamespace = Owner.CanonicalNamespace;
		Declaration.CanonicalName = Name;
		Declaration.CanonicalTypeSpelling = TEXT("int");
		Declaration.CanonicalDeclaration = FString::Printf(TEXT("int %s"), Name);
		Declaration.DeclaredType = MakeInt32Type();
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration, 1});
		FAngelscriptPropertyIdentityDescriptor Identity;
		Identity.OwnerTypeKey = FAngelscriptStableTypeKey{Owner.StableKey};
		Identity.Kind = Declaration.EntityKind;
		Identity.Name = Declaration.CanonicalName;
		Identity.CanonicalType = Declaration.CanonicalTypeSpelling.GetValue();
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedTypeSchema MakeStructSchema(
		const FModuleSpec& Spec,
		const FAngelscriptCachedDeclaration& TypeDeclaration,
		const FAngelscriptCachedDeclaration& PropertyDeclaration,
		const uint32 PropertyStorageSize)
	{
		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = Spec.ModuleKey;
		Schema.TypeKey = FAngelscriptStableTypeKey{TypeDeclaration.StableKey};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Struct;
		Schema.CanonicalNamespace = TypeDeclaration.CanonicalNamespace;
		Schema.CanonicalName = TypeDeclaration.CanonicalName;
		Schema.CanonicalDeclaration = TypeDeclaration.CanonicalDeclaration;
		Schema.TypeSemanticFlags =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Final)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ValueType);
		Schema.Layout.SemanticSize = Align(
			static_cast<uint64>(PropertyStorageSize), uint64(8));
		Schema.Layout.SemanticAlignment = 8;
		Schema.Layout.BasePropertyBoundary = 0;
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;

		FAngelscriptCachedPropertySchema& Property =
			Schema.OrderedProperties.AddDefaulted_GetRef();
		Property.LayoutOrdinal = 0;
		Property.SemanticByteOffset = 0;
		Property.PropertyKey =
			FAngelscriptStablePropertyKey{PropertyDeclaration.StableKey};
		Property.CanonicalName = PropertyDeclaration.CanonicalName;
		Property.Type = MakeInt32Type();
		Property.StorageKind = EAngelscriptCachedPropertyStorageKind::InlineValue;
		Property.SemanticStorageSize = PropertyStorageSize;
		Property.SemanticStorageAlignment =
			PropertyStorageSize >= 8 ? 8 : 4;
		Property.Access = EAngelscriptCachedMemberAccess::Public;
		Property.ReplicationCondition =
			EAngelscriptCachedReplicationCondition::None;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
			Property.Type, Property.StorageKind,
			Property.SemanticStorageSize,
			Property.SemanticStorageAlignment,
			Property.StorageLayoutHash).IsSuccess());
		check(FAngelscriptCacheTypeSchemaArchive::
			ComputePropertyLayoutFingerprint(
				Schema.TypeKey, Property,
				Property.PropertyLayoutFingerprint).IsSuccess());
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
		return Schema;
	}

	static FAngelscriptCachedDeclaration MakeGlobalDeclaration(
		const FModuleSpec& Spec)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Global;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::GlobalVariable;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = Spec.ModuleKey.Hash;
		Declaration.ModuleKey = Spec.ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = TEXT("AnswerConstant");
		Declaration.CanonicalDeclaration = TEXT("int AnswerConstant");
		Declaration.CanonicalTypeSpelling = TEXT("int");
		Declaration.DeclaredType = MakeInt32Type();
		Declaration.TraitFlags = static_cast<uint32>(
			EAngelscriptCachedDeclarationTraitFlags::Const);
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration, 0});
		FAngelscriptGlobalIdentityDescriptor Identity;
		Identity.ModuleKey = Spec.ModuleKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.Name = Declaration.CanonicalName;
		Identity.CanonicalType = Declaration.CanonicalTypeSpelling.GetValue();
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedDeclaration MakeModuleInitializerDeclaration(
		const FModuleSpec& Spec)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Function;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::ModuleInitializer;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = Spec.ModuleKey.Hash;
		Declaration.ModuleKey = Spec.ModuleKey;
		Declaration.CanonicalNamespace = TEXT("Gameplay");
		Declaration.CanonicalName = TEXT("ModuleInitialize");
		Declaration.CanonicalDeclaration = TEXT("void ModuleInitialize()");
		Declaration.DeclaredType = MakeVoidType();
		Declaration.TraitFlags = static_cast<uint32>(
			EAngelscriptCachedDeclarationTraitFlags::Generated);
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

	static FAngelscriptHash256 ExecutionHash(const uint8 Seed)
	{
		const TArray<uint8> Payload{Seed, static_cast<uint8>(Seed + 1),
			static_cast<uint8>(Seed + 2)};
		return FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
			Payload, {}).Execution;
	}

	static FAngelscriptCacheSemanticDependency MakeFunctionDependency(
		const FAngelscriptCachedDeclaration& Target,
		const TOptional<FAngelscriptHash256>& ExpectedContent = {})
	{
		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = ExpectedContent.IsSet()
			? EAngelscriptCacheSemanticDependencyKind::FunctionContent
			: EAngelscriptCacheSemanticDependencyKind::Declaration;
		Dependency.Target.Kind = EAngelscriptCacheReferenceKind::ScriptFunction;
		Dependency.Target.StableKey = Target.StableKey;
		Dependency.Target.ExpectedAbi = Target.SignatureHash;
		Dependency.ExpectedContentOrValue = ExpectedContent;
		return Dependency;
	}

	static FAngelscriptCacheSemanticDependency MakeMissingFunctionDependency()
	{
		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::Declaration;
		Dependency.Target.Kind = EAngelscriptCacheReferenceKind::ScriptFunction;
		Dependency.Target.StableKey = MakeHash(0xe0);
		Dependency.Target.ExpectedAbi = MakeHash(0xe1);
		return Dependency;
	}

	static FAngelscriptCacheSemanticDependency MakeExternalDependency(
		const bool bWithContent)
	{
		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = bWithContent
			? EAngelscriptCacheSemanticDependencyKind::CompileOption
			: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
		Dependency.Target.Kind = EAngelscriptCacheReferenceKind::EnvironmentSymbol;
		Dependency.Target.StableKey = MakeHash(0xd0);
		Dependency.Target.ExpectedAbi = MakeHash(0xd1);
		if (bWithContent)
		{
			Dependency.ExpectedContentOrValue = MakeHash(0xd2);
		}
		return Dependency;
	}

	static FAngelscriptCacheSemanticDependency MakeDependency(
		const EAngelscriptCacheSemanticDependencyKind Kind,
		const EAngelscriptCacheReferenceKind TargetKind,
		const FAngelscriptHash256& TargetKey,
		const FAngelscriptHash256& ExpectedAbi,
		const TOptional<FAngelscriptHash256>& ExpectedContent = {})
	{
		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = Kind;
		Dependency.Target.Kind = TargetKind;
		Dependency.Target.StableKey = TargetKey;
		Dependency.Target.ExpectedAbi = ExpectedAbi;
		Dependency.ExpectedContentOrValue = ExpectedContent;
		return Dependency;
	}

	static FAngelscriptCachedImportDeclaration MakeImport(
		const FModuleSpec& Owner,
		const FModuleSpec& Target,
		const FAngelscriptCachedDeclaration& TargetDeclaration)
	{
		FAngelscriptCachedImportDeclaration Import;
		Import.CanonicalNamespace = TargetDeclaration.CanonicalNamespace;
		Import.CanonicalName = TargetDeclaration.CanonicalName;
		Import.CanonicalSignature = TargetDeclaration.CanonicalDeclaration;
		Import.TargetModuleKey = Target.ModuleKey;
		Import.TargetDeclaration.Kind =
			EAngelscriptCacheReferenceKind::ScriptFunction;
		Import.TargetDeclaration.StableKey = TargetDeclaration.StableKey;
		Import.TargetDeclaration.ExpectedAbi = TargetDeclaration.SignatureHash;
		Import.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Import, 0});
		check(FAngelscriptCacheSemanticArchive::TryBuildImportKey({
			Owner.ModuleKey,
			Import.CanonicalNamespace,
			Import.CanonicalName,
			Import.CanonicalSignature,
			Import.TargetModuleKey,
			FAngelscriptStableFunctionKey{
				Import.TargetDeclaration.StableKey}},
			Import.ImportKey).IsSuccess());
		return Import;
	}

	static FAngelscriptCacheCompatibilityKey MakeCompatibility()
	{
		FAngelscriptCompatibilityDescriptor Descriptor;
		Descriptor.CanonicalInputs = {
			TEXT("DependencyPropagationFixture"),
			TEXT("VmExecutionCodec=2"),
		};
		return FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
			Descriptor);
	}

	static FAngelscriptCacheContextKey MakeContext()
	{
		FAngelscriptContextDescriptor Descriptor;
		Descriptor.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DependencyPropagation=true"),
		};
		return FAngelscriptArtifactIdentityBuilder::BuildContextKey(Descriptor);
	}

	static FAngelscriptArtifactProfileKey MakeProfile()
	{
		return FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
			MakeCompatibility(), MakeContext());
	}

	static FAngelscriptCachedHardValue MakeGlobalHardValue(
		const FAngelscriptCachedDeclaration& Declaration,
		const int32 Value)
	{
		FAngelscriptCachedHardValue HardValue;
		HardValue.HardValueKind =
			EAngelscriptCachedHardValueKind::GlobalConstant;
		HardValue.Owner.Kind = EAngelscriptCacheReferenceKind::ScriptGlobal;
		HardValue.Owner.StableKey = Declaration.StableKey;
		HardValue.Owner.ExpectedAbi = Declaration.SignatureHash;
		HardValue.Type = Declaration.DeclaredType.GetValue();
		HardValue.CanonicalValue.Emplace();
		HardValue.CanonicalValue->ValueKind =
			EAngelscriptCachedCanonicalValueKind::SignedInteger;
		HardValue.CanonicalValue->FixedWidthValueBytes = {
			static_cast<uint8>(Value & 0xff),
			static_cast<uint8>((Value >> 8) & 0xff),
			static_cast<uint8>((Value >> 16) & 0xff),
			static_cast<uint8>((Value >> 24) & 0xff),
		};
		check(FAngelscriptCacheRemainingRecordArchive::
			ComputeGlobalConstantHardValueHash(
				HardValue, HardValue.HardValueHash).IsSuccess());
		return HardValue;
	}

	static FAngelscriptCachedInitializerUnit MakeInitializer(
		const FModuleSpec& Spec,
		const FAngelscriptCachedDeclaration& Declaration,
		const uint8 Seed)
	{
		FAngelscriptCachedInitializerUnit Initializer;
		Initializer.InitializerKind =
			EAngelscriptCachedInitializerKind::Module;
		Initializer.InitializerKey =
			FAngelscriptStableFunctionKey{Declaration.StableKey};
		Initializer.VmInitializerCodecVersion = 1;
		Initializer.CanonicalExecutionPayload = {
			Seed,
			static_cast<uint8>(Seed + 1),
			static_cast<uint8>(Seed + 2),
		};
		check(FAngelscriptCacheRemainingRecordArchive::
			ComputeInitializerExecutionHash(
				Spec.ModuleKey, MakeProfile(), Initializer,
				Initializer.InitializerExecutionHash).IsSuccess());
		return Initializer;
	}

	static FAngelscriptCachedSourceIndex MakeSourceIndex(
		const TConstArrayView<FModuleSpec> Modules)
	{
		FAngelscriptCachedSourceIndex Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Value.DiscoveryPolicy.PolicyVersion = 1;

		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity =
			TEXT("DependencyPropagationFixture.Disk");
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

		for (int32 Index = 0; Index < Modules.Num(); ++Index)
		{
			FAngelscriptCachedSourceFile File;
			File.SourceKind = EAngelscriptCachedSourceKind::Game;
			File.MountKey = Mount.MountKey;
			File.ProviderKey = Provider.ProviderKey;
			File.RelativeLogicalPath = FString::Printf(
				TEXT("%s.as"), *Modules[Index].ModuleName);
			File.RawContentHash = MakeHash(static_cast<uint8>(0x80 + Index));
			File.ModuleKey = Modules[Index].ModuleKey;
			File.SourceFileKey = BuildSourceFileKey(File);
			Value.Files.Add(MoveTemp(File));
		}
		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Value, Value.SourceSnapshot).IsSuccess());
		return Value;
	}

	static FAngelscriptCachedFunctionBody MakeFunctionBody(
		const FModuleSpec& Spec,
		const FAngelscriptCachedDeclaration& Declaration,
		const FAngelscriptArtifactProfileKey& Profile)
	{
		FAngelscriptCachedFunctionBody Body;
		Body.PayloadSchemaVersion =
			FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion;
		Body.ModuleKey = Spec.ModuleKey;
		Body.Identity.FunctionKey =
			FAngelscriptStableFunctionKey{Declaration.StableKey};
		Body.Identity.Profile = Profile;
		Body.ExpectedDeclarationAbi = Declaration.SignatureHash;
		Body.FunctionSourceDigest.Hash = MakeHash(
			static_cast<uint8>(Spec.ExecutionSeed + 0x20));
		Body.FunctionInputDigest.Hash = MakeHash(
			static_cast<uint8>(Spec.ExecutionSeed + 0x40));
		Body.InvocationKind = EAngelscriptCachedFunctionInvocationKind::GlobalFunction;
		Body.VmExecutionCodecVersion = 1;
		Body.CanonicalExecutionPayload = {
			Spec.ExecutionSeed,
			static_cast<uint8>(Spec.ExecutionSeed + 1),
			static_cast<uint8>(Spec.ExecutionSeed + 2),
		};
		Body.Identity.Content.Execution =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				Body.CanonicalExecutionPayload, {}).Execution;
		Body.Identity.Content.Debug =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(Profile);
		Body.ActualDependencies = Spec.Dependencies;
		return Body;
	}

	static FAngelscriptCachePackLocation MakeLocation(
		const FAngelscriptEncodedPack& Pack,
		const FAngelscriptCachePackIndexEntry& Entry)
	{
		return {Pack.PackId, Entry.PackOffset, Entry.StoredSize, Entry.RawSize,
			Entry.Codec, Entry.RawChecksum};
	}

	class FPackSource final : public IAngelscriptCachePackSource
	{
	public:
		explicit FPackSource(const TArray<FAngelscriptEncodedPack>& InPacks)
			: Packs(InPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			OutBytes = {};
			for (const FAngelscriptEncodedPack& Pack : Packs)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			return false;
		}

	private:
		const TArray<FAngelscriptEncodedPack>& Packs;
	};

	static bool BuildGeneration(
		FAutomationTestBase& Test,
		const TConstArrayView<FModuleSpec> Modules,
		FAngelscriptValidatedGeneration& OutGeneration)
	{
		OutGeneration = {};
		const FAngelscriptCacheCompatibilityKey Compatibility = MakeCompatibility();
		const FAngelscriptCacheContextKey Context = MakeContext();
		const FAngelscriptArtifactProfileKey Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Compatibility, Context);

		TArray<FAngelscriptPreparedRecord> Records;
		TArray<FAngelscriptCacheModuleSnapshotLink> SnapshotRoots;
		const auto AddRecord = [&Records](
			const EAngelscriptCacheRecordKind Kind,
			TArray<uint8>& Payload) -> FAngelscriptCacheRecordId
		{
			FAngelscriptPreparedRecord& Record = Records.AddDefaulted_GetRef();
			check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
				Kind, Payload, Record.RecordId).IsSuccess());
			Record.CanonicalPayload = MoveTemp(Payload);
			return Record.RecordId;
		};

		const FAngelscriptCachedSourceIndex SourceIndex = MakeSourceIndex(Modules);
		TArray<uint8> Payload;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			SourceIndex, Payload).IsSuccess());
		const FAngelscriptCacheRecordId SourceIndexRecordId =
			AddRecord(EAngelscriptCacheRecordKind::SourceIndex, Payload);

		for (const FModuleSpec& Spec : Modules)
		{
			const FAngelscriptCachedDeclaration Declaration =
				MakeFunctionDeclaration(Spec);
			FAngelscriptCachedModuleInterface Interface;
			Interface.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			Interface.ModuleKey = Spec.ModuleKey;
			Interface.CanonicalModuleName = Spec.ModuleName;
			Interface.CanonicalNamespaces.Add(TEXT("Gameplay"));
			Interface.Declarations.Add(Declaration);
			Interface.Declarations.Append(Spec.AdditionalDeclarations);
			Interface.Imports = Spec.Imports;
			Interface.Dependencies = Spec.InterfaceDependencies;
			check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
				Interface, Interface.InterfaceAbi).IsSuccess());
			Payload.Reset();
			check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				Interface, Payload).IsSuccess());
			const FAngelscriptCacheRecordId InterfaceRecordId =
				AddRecord(EAngelscriptCacheRecordKind::ModuleInterface, Payload);

			const FAngelscriptCachedFunctionBody Body =
				MakeFunctionBody(Spec, Declaration, Profile);
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
				Body, Payload).IsSuccess());
			const FAngelscriptCacheRecordId BodyRecordId =
				AddRecord(EAngelscriptCacheRecordKind::FunctionBody, Payload);

			TArray<FAngelscriptCachedTypeSchemaLink> TypeSchemaLinks;
			for (const FAngelscriptCachedTypeSchema& TypeSchema : Spec.TypeSchemas)
			{
				Payload.Reset();
				const FAngelscriptCacheValidationResult TypeSchemaResult =
					FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
						TypeSchema, Payload);
				if (!TypeSchemaResult.IsSuccess())
				{
					Test.AddError(FString::Printf(
						TEXT("Dependency propagation fixture TypeSchema serialize failed: Module=%s Error=%u Stage=%u Offset=%llu"),
						*Spec.ModuleName,
						static_cast<uint32>(TypeSchemaResult.Error),
						static_cast<uint32>(TypeSchemaResult.Stage),
						TypeSchemaResult.ByteOffset));
					return false;
				}
				const FAngelscriptCacheRecordId TypeSchemaRecordId =
					AddRecord(EAngelscriptCacheRecordKind::TypeSchema, Payload);
				TypeSchemaLinks.Add({TypeSchema.TypeKey, TypeSchemaRecordId});
			}

			FAngelscriptCachedModuleState State;
			State.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion;
			State.ModuleKey = Spec.ModuleKey;
			State.Profile = Profile;
			if (Spec.PureConstantValue.IsSet())
			{
				const FAngelscriptCachedDeclaration* GlobalDeclaration =
					Spec.AdditionalDeclarations.FindByPredicate([](
						const FAngelscriptCachedDeclaration& Candidate)
					{
						return Candidate.DeclarationKind
							== EAngelscriptCacheDeclarationKind::Global;
					});
				check(GlobalDeclaration != nullptr);
				FAngelscriptCachedGlobalSchema& Global =
					State.OrderedGlobals.AddDefaulted_GetRef();
				Global.StorageOrdinal = 0;
				Global.GlobalKey = FAngelscriptStableGlobalKey{
					GlobalDeclaration->StableKey};
				Global.CanonicalNamespace =
					GlobalDeclaration->CanonicalNamespace;
				Global.CanonicalName = GlobalDeclaration->CanonicalName;
				Global.Type = GlobalDeclaration->DeclaredType.GetValue();
				Global.GlobalTraitFlags = GlobalDeclaration->TraitFlags;
				Global.InitializationKind =
					EAngelscriptCachedGlobalInitializationKind::PureConstant;
				Global.CleanupPolicy =
					EAngelscriptCachedGlobalCleanupPolicy::None;
				check(FAngelscriptCacheRemainingRecordArchive::
					ComputeGlobalStorageLayoutFingerprint(
						Spec.ModuleKey, Global,
						Global.StorageLayoutFingerprint).IsSuccess());
				State.HardValues.Add(MakeGlobalHardValue(
					*GlobalDeclaration, Spec.PureConstantValue.GetValue()));
			}
			if (Spec.ModuleInitializerSeed.IsSet())
			{
				const FAngelscriptCachedDeclaration* InitializerDeclaration =
					Spec.AdditionalDeclarations.FindByPredicate([](
						const FAngelscriptCachedDeclaration& Candidate)
					{
						return Candidate.EntityKind
							== EAngelscriptArtifactEntityKind::ModuleInitializer;
					});
				check(InitializerDeclaration != nullptr);
				const FAngelscriptCachedInitializerUnit Initializer =
					MakeInitializer(
						Spec, *InitializerDeclaration,
						Spec.ModuleInitializerSeed.GetValue());
				State.Initializers.Add(Initializer);
				FAngelscriptCachedInitializationAction& Action =
					State.OrderedInitializationActions.AddDefaulted_GetRef();
				Action.ActionOrdinal = 0;
				Action.ActionKind =
					EAngelscriptCachedInitializationActionKind::ExecuteInitializer;
				Action.Target.Kind =
					EAngelscriptCacheReferenceKind::ScriptFunction;
				Action.Target.StableKey =
					InitializerDeclaration->StableKey;
				Action.Target.ExpectedAbi =
					InitializerDeclaration->SignatureHash;
				Action.Dependencies = Spec.InitializationActionDependencies;
			}
			State.Dependencies = Spec.StateDependencies;
			check(FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
				State, State.StateInputHash).IsSuccess());
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
				State, Payload).IsSuccess());
			const FAngelscriptCacheRecordId StateRecordId =
				AddRecord(EAngelscriptCacheRecordKind::ModuleState, Payload);

			FAngelscriptCachedModuleSnapshot Snapshot;
			Snapshot.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion;
			Snapshot.ModuleKey = Spec.ModuleKey;
			Snapshot.ModuleInterface = {Spec.ModuleKey, InterfaceRecordId};
			Snapshot.ModuleState = {Spec.ModuleKey, StateRecordId};
			Snapshot.TypeSchemas = MoveTemp(TypeSchemaLinks);
			Snapshot.FunctionBodies.Add({
				FAngelscriptStableFunctionKey{Declaration.StableKey}, BodyRecordId});
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
				Snapshot, Payload).IsSuccess());
			const FAngelscriptCacheRecordId SnapshotRecordId =
				AddRecord(EAngelscriptCacheRecordKind::ModuleSnapshot, Payload);
			SnapshotRoots.Add({Spec.ModuleKey, SnapshotRecordId});
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		const FAngelscriptCacheValidationResult PackResult =
			BuildAngelscriptCachePacks(Records, PackPolicy, Codec, Packs);
		if (!PackResult.IsSuccess())
		{
			Test.AddError(TEXT("Dependency propagation fixture Pack build failed"));
			return false;
		}

		FAngelscriptCacheGenerationManifest Manifest;
		Manifest.ManifestSchemaVersion =
			FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion;
		Manifest.Compatibility = Compatibility;
		Manifest.Context = Context;
		Manifest.Profile = Profile;
		Manifest.SourceSnapshot = SourceIndex.SourceSnapshot;
		Manifest.SourceIndexRecordId = SourceIndexRecordId;
		Manifest.ModuleSnapshots = MoveTemp(SnapshotRoots);
		Manifest.ModuleSnapshots.Sort([](
			const FAngelscriptCacheModuleSnapshotLink& Left,
			const FAngelscriptCacheModuleSnapshotLink& Right)
		{
			return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
		});
		for (const FAngelscriptEncodedPack& Pack : Packs)
		{
			for (const FAngelscriptCachePackIndexEntry& Entry : Pack.Index)
			{
				Manifest.Records.Add({Entry.RecordId, MakeLocation(Pack, Entry)});
			}
		}
		Manifest.Records.Sort([](
			const FAngelscriptCacheRecordIndexEntry& Left,
			const FAngelscriptCacheRecordIndexEntry& Right)
		{
			return Left.RecordId < Right.RecordId;
		});

		FAngelscriptEncodedCacheGenerationManifest Encoded;
		const FAngelscriptCacheValidationResult EncodeResult =
			EncodeAngelscriptCacheGenerationManifest(Manifest, Encoded);
		if (!EncodeResult.IsSuccess())
		{
			Test.AddError(TEXT("Dependency propagation fixture Manifest encode failed"));
			return false;
		}
		FPackSource PackSource(Packs);
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			ValidateAngelscriptCacheGeneration(
				Encoded.CompleteBytes,
				Encoded.ComputedGenerationId,
				PackSource,
				FAngelscriptCacheReadLimits{},
				Budget,
				Codec,
				Validated);
		if (!Validation.IsSuccess() || !Validated.IsSet())
		{
			Test.AddError(FString::Printf(
				TEXT("Dependency propagation fixture validation failed: Error=%u Stage=%u"),
				static_cast<uint32>(Validation.Error),
				static_cast<uint32>(Validation.Stage)));
			return false;
		}
		OutGeneration = MoveTemp(Validated.GetValue());
		return true;
	}

	static FAngelscriptCacheDependencyPropagationResult Plan(
		const FAngelscriptValidatedGeneration& Generation,
		const IAngelscriptCacheCurrentSymbolResolver* Resolver,
		FAngelscriptCacheDependentRecompileWave& OutWave)
	{
		return PlanAngelscriptCacheDependentRecompileWave(
			Generation,
			FAngelscriptCacheReadLimits{},
			Resolver,
			OutWave);
	}

	class FExactExternalResolver final
		: public IAngelscriptCacheCurrentSymbolResolver
	{
	public:
		mutable int32 CallCount = 0;
		bool bMissing = false;
		FAngelscriptHash256 Abi = MakeHash(0xd1);
		TOptional<FAngelscriptHash256> Content = MakeHash(0xd2);

		virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
			const EAngelscriptCacheReferenceKind ReferenceKind,
			const FAngelscriptHash256& StableKey) const override
		{
			++CallCount;
			check(ReferenceKind
				== EAngelscriptCacheReferenceKind::EnvironmentSymbol);
			check(StableKey == MakeHash(0xd0));
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

	static void LogWave(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const FAngelscriptCacheDependencyPropagationResult& Result,
		const FAngelscriptCacheDependentRecompileWave& Wave)
	{
		int32 ReasonCount = 0;
		for (const FAngelscriptCacheDependentModule& Module : Wave.Modules)
		{
			ReasonCount += Module.Reasons.Num();
		}
		Test.AddInfo(FString::Printf(
			TEXT("Dependency propagation %s: Error=%u Modules=%d Reasons=%d Detail=%s"),
			Label,
			static_cast<uint32>(Result.Error),
			Wave.Modules.Num(),
			ReasonCount,
			*Result.Detail));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheDependencyPropagationTests,
	"Angelscript.TestModule.Cache.DependencyPropagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(BodyOnlyProviderChangeKeepsAbiOnlyCallerReusable)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Provider = MakeModule(0x10, TEXT("Provider"),
			TEXT("Provide"), 0x31);
		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		Consumer.Dependencies.Add(MakeFunctionDependency(
			MakeFunctionDeclaration(Provider)));

		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(
			*TestRunner, {Provider, Consumer}, Generation)));
		FAngelscriptCacheDependentRecompileWave Wave;
		const FAngelscriptCacheDependencyPropagationResult Result =
			Plan(Generation, nullptr, Wave);
		LogWave(*TestRunner, TEXT("body-abi-only"), Result, Wave);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), *Result.Detail));
		ASSERT_THAT(AreEqual(0, Wave.Modules.Num()));
	}

	TEST_METHOD(EmbeddedOldFunctionContentSelectsOnlyConsumer)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Provider = MakeModule(0x10, TEXT("Provider"),
			TEXT("Provide"), 0x32);
		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		Consumer.Dependencies.Add(MakeFunctionDependency(
			MakeFunctionDeclaration(Provider), ExecutionHash(0x31)));
		FModuleSpec Unrelated = MakeModule(0x30, TEXT("Unrelated"),
			TEXT("UnrelatedWork"), 0x51);

		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(
			*TestRunner, {Unrelated, Consumer, Provider}, Generation)));
		FAngelscriptCacheDependentRecompileWave Wave;
		const FAngelscriptCacheDependencyPropagationResult Result =
			Plan(Generation, nullptr, Wave);
		LogWave(*TestRunner, TEXT("embedded-content"), Result, Wave);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), *Result.Detail));
		ASSERT_THAT(AreEqual(1, Wave.Modules.Num()));
		ASSERT_THAT(IsTrue(Wave.Modules[0].ModuleKey == Consumer.ModuleKey));
		ASSERT_THAT(AreEqual(1, Wave.Modules[0].Reasons.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::ContentMismatch,
			Wave.Modules[0].Reasons[0].Reason));
	}

	TEST_METHOD(RemovedProviderFunctionIsDeterministicSafeMiss)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		Consumer.Dependencies.Add(MakeMissingFunctionDependency());

		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(*TestRunner, {Consumer}, Generation)));
		FAngelscriptCacheDependentRecompileWave Wave;
		const FAngelscriptCacheDependencyPropagationResult Result =
			Plan(Generation, nullptr, Wave);
		LogWave(*TestRunner, TEXT("removed-target"), Result, Wave);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), *Result.Detail));
		ASSERT_THAT(AreEqual(1, Wave.Modules.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::TargetUnresolved,
			Wave.Modules[0].Reasons[0].Reason));
	}

	TEST_METHOD(ExternalResolverExactHitAndUnavailableSafeMiss)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		Consumer.Dependencies.Add(MakeExternalDependency(true));
		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(*TestRunner, {Consumer}, Generation)));

		FAngelscriptCacheDependentRecompileWave MissingWave;
		const FAngelscriptCacheDependencyPropagationResult MissingResult =
			Plan(Generation, nullptr, MissingWave);
		ASSERT_THAT(IsTrue(MissingResult.IsSuccess(), *MissingResult.Detail));
		ASSERT_THAT(AreEqual(1, MissingWave.Modules.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::TargetUnresolved,
			MissingWave.Modules[0].Reasons[0].Reason));

		FExactExternalResolver Resolver;
		FAngelscriptCacheDependentRecompileWave ExactWave;
		const FAngelscriptCacheDependencyPropagationResult ExactResult =
			Plan(Generation, &Resolver, ExactWave);
		LogWave(*TestRunner, TEXT("external-exact"), ExactResult, ExactWave);
		ASSERT_THAT(IsTrue(ExactResult.IsSuccess(), *ExactResult.Detail));
		ASSERT_THAT(AreEqual(0, ExactWave.Modules.Num()));
		ASSERT_THAT(AreEqual(1, Resolver.CallCount));
	}

	TEST_METHOD(DecodedRecordOrderCannotChangeLogicalWave)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Provider = MakeModule(0x10, TEXT("Provider"),
			TEXT("Provide"), 0x32);
		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		Consumer.Dependencies.Add(MakeFunctionDependency(
			MakeFunctionDeclaration(Provider), ExecutionHash(0x31)));
		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(
			*TestRunner, {Provider, Consumer}, Generation)));

		FAngelscriptValidatedGeneration Reordered = Generation;
		Algo::Reverse(Reordered.ReachableRecords);
		FAngelscriptCacheDependentRecompileWave First;
		FAngelscriptCacheDependentRecompileWave Second;
		const FAngelscriptCacheDependencyPropagationResult FirstResult =
			Plan(Generation, nullptr, First);
		const FAngelscriptCacheDependencyPropagationResult SecondResult =
			Plan(Reordered, nullptr, Second);
		ASSERT_THAT(IsTrue(FirstResult.IsSuccess(), *FirstResult.Detail));
		ASSERT_THAT(IsTrue(SecondResult.IsSuccess(), *SecondResult.Detail));
		ASSERT_THAT(AreEqual(1, First.Modules.Num()));
		ASSERT_THAT(AreEqual(1, Second.Modules.Num()));
		ASSERT_THAT(IsTrue(First.Modules[0].ModuleKey
			== Second.Modules[0].ModuleKey));
		ASSERT_THAT(AreEqual(First.Modules[0].Reasons.Num(),
			Second.Modules[0].Reasons.Num()));
		ASSERT_THAT(IsTrue(First.Modules[0].Reasons[0]
			== Second.Modules[0].Reasons[0]));
	}

	TEST_METHOD(ForgedGenerationFailsAtomically)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Module = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(*TestRunner, {Module}, Generation)));
		Generation.ReachableRecords.Pop();

		FAngelscriptCacheDependentRecompileWave Wave;
		Wave.Modules.AddDefaulted();
		const FAngelscriptCacheDependencyPropagationResult Result =
			Plan(Generation, nullptr, Wave);
		LogWave(*TestRunner, TEXT("forged"), Result, Wave);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyPropagationError::InvalidValidatedGeneration,
			Result.Error));
		ASSERT_THAT(AreEqual(0, Wave.Modules.Num()));
	}

	TEST_METHOD(TypeAndPropertyLayoutUseCurrentSchemaAuthorities)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Provider = MakeModule(0x10, TEXT("Provider"),
			TEXT("Provide"), 0x31);
		const FAngelscriptCachedDeclaration ProviderType =
			MakeTypeDeclaration(Provider, TEXT("SharedValue"));
		const FAngelscriptCachedDeclaration ProviderProperty =
			MakePropertyDeclaration(
				Provider, ProviderType, TEXT("Value"));
		const FAngelscriptCachedTypeSchema OldProviderSchema =
			MakeStructSchema(
				Provider, ProviderType, ProviderProperty, 4);
		const FAngelscriptCachedTypeSchema CurrentProviderSchema =
			MakeStructSchema(
				Provider, ProviderType, ProviderProperty, 8);
		Provider.AdditionalDeclarations = {ProviderType, ProviderProperty};
		Provider.TypeSchemas = {CurrentProviderSchema};

		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		const FAngelscriptCachedDeclaration ConsumerType =
			MakeTypeDeclaration(Consumer, TEXT("ConsumerValue"));
		const FAngelscriptCachedDeclaration ConsumerProperty =
			MakePropertyDeclaration(
				Consumer, ConsumerType, TEXT("LocalValue"));
		FAngelscriptCachedTypeSchema ConsumerSchema = MakeStructSchema(
			Consumer, ConsumerType, ConsumerProperty, 4);
		FAngelscriptCachedPropertySchema& EmbeddedProviderValue =
			ConsumerSchema.OrderedProperties[0];
		EmbeddedProviderValue.Type.Kind =
			EAngelscriptCachedDataTypeKind::ScriptType;
		EmbeddedProviderValue.Type.Primitive =
			EAngelscriptCachedPrimitiveType::Invalid;
		EmbeddedProviderValue.Type.TypeReference.Emplace();
		EmbeddedProviderValue.Type.TypeReference->Kind =
			EAngelscriptCacheReferenceKind::ScriptType;
		EmbeddedProviderValue.Type.TypeReference->StableKey =
			ProviderType.StableKey;
		EmbeddedProviderValue.Type.TypeReference->ExpectedAbi =
			ProviderType.SignatureHash;
		EmbeddedProviderValue.SemanticStorageSize =
			OldProviderSchema.Layout.SemanticSize;
		EmbeddedProviderValue.SemanticStorageAlignment =
			OldProviderSchema.Layout.SemanticAlignment;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
			EmbeddedProviderValue.Type,
			EmbeddedProviderValue.StorageKind,
			EmbeddedProviderValue.SemanticStorageSize,
			EmbeddedProviderValue.SemanticStorageAlignment,
			EmbeddedProviderValue.StorageLayoutHash).IsSuccess());
		check(FAngelscriptCacheTypeSchemaArchive::
			ComputePropertyLayoutFingerprint(
				ConsumerSchema.TypeKey, EmbeddedProviderValue,
				EmbeddedProviderValue.PropertyLayoutFingerprint).IsSuccess());
		ConsumerSchema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::ValueLayout,
			EAngelscriptCacheReferenceKind::ScriptType,
			ProviderType.StableKey,
			ProviderType.SignatureHash,
			OldProviderSchema.Layout.TypeLayoutHash));
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			ConsumerSchema, ConsumerSchema.Layout.TypeLayoutHash).IsSuccess());
		Consumer.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::PropertyLayout,
			EAngelscriptCacheReferenceKind::ScriptProperty,
			ProviderProperty.StableKey,
			ProviderProperty.SignatureHash,
			OldProviderSchema.OrderedProperties[0]
				.PropertyLayoutFingerprint));
		Consumer.AdditionalDeclarations = {ConsumerType, ConsumerProperty};
		Consumer.TypeSchemas = {ConsumerSchema};

		FModuleSpec Unrelated = MakeModule(0x30, TEXT("Unrelated"),
			TEXT("UnrelatedWork"), 0x51);
		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(
			*TestRunner, {Unrelated, Consumer, Provider}, Generation)));
		FAngelscriptCacheDependentRecompileWave Wave;
		const FAngelscriptCacheDependencyPropagationResult Result =
			Plan(Generation, nullptr, Wave);
		LogWave(*TestRunner, TEXT("type-property-layout"), Result, Wave);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), *Result.Detail));
		ASSERT_THAT(AreEqual(1, Wave.Modules.Num()));
		ASSERT_THAT(IsTrue(Wave.Modules[0].ModuleKey == Consumer.ModuleKey));
		ASSERT_THAT(AreEqual(2, Wave.Modules[0].Reasons.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::ContentMismatch,
			Wave.Modules[0].Reasons[0].Reason));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::ContentMismatch,
			Wave.Modules[0].Reasons[1].Reason));
	}

	TEST_METHOD(HardValueAndInitializerContentSelectOnlyConsumer)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Provider = MakeModule(0x10, TEXT("Provider"),
			TEXT("Provide"), 0x31);
		const FAngelscriptCachedDeclaration GlobalDeclaration =
			MakeGlobalDeclaration(Provider);
		const FAngelscriptCachedDeclaration InitializerDeclaration =
			MakeModuleInitializerDeclaration(Provider);
		Provider.AdditionalDeclarations = {
			GlobalDeclaration, InitializerDeclaration};
		Provider.PureConstantValue = 43;
		Provider.ModuleInitializerSeed = 0x92;

		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		Consumer.StateDependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::HardValue,
			EAngelscriptCacheReferenceKind::ScriptGlobal,
			GlobalDeclaration.StableKey,
			GlobalDeclaration.SignatureHash,
			MakeGlobalHardValue(GlobalDeclaration, 42).HardValueHash));
		const FAngelscriptCachedDeclaration ConsumerInitializer =
			MakeModuleInitializerDeclaration(Consumer);
		Consumer.AdditionalDeclarations.Add(ConsumerInitializer);
		Consumer.ModuleInitializerSeed = 0x61;
		Consumer.InitializationActionDependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Initializer,
			EAngelscriptCacheReferenceKind::ScriptFunction,
			InitializerDeclaration.StableKey,
			InitializerDeclaration.SignatureHash,
			MakeInitializer(Provider, InitializerDeclaration, 0x91)
				.InitializerExecutionHash));

		FModuleSpec Unrelated = MakeModule(0x30, TEXT("Unrelated"),
			TEXT("UnrelatedWork"), 0x51);
		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(
			*TestRunner, {Provider, Unrelated, Consumer}, Generation)));
		FAngelscriptCacheDependentRecompileWave Wave;
		const FAngelscriptCacheDependencyPropagationResult Result =
			Plan(Generation, nullptr, Wave);
		LogWave(*TestRunner, TEXT("hard-value-initializer"), Result, Wave);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), *Result.Detail));
		ASSERT_THAT(AreEqual(1, Wave.Modules.Num()));
		ASSERT_THAT(IsTrue(Wave.Modules[0].ModuleKey == Consumer.ModuleKey));
		ASSERT_THAT(AreEqual(2, Wave.Modules[0].Reasons.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::ContentMismatch,
			Wave.Modules[0].Reasons[0].Reason));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::ContentMismatch,
			Wave.Modules[0].Reasons[1].Reason));
	}

	TEST_METHOD(ImportRouteFollowsCurrentProviderDeclaration)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		const FModuleSpec OldProvider = MakeModule(0x10, TEXT("Provider"),
			TEXT("Provide"), 0x31, 0);
		FModuleSpec CurrentProvider = OldProvider;
		CurrentProvider.FunctionTraitFlags = 1;
		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		Consumer.Imports.Add(MakeImport(
			Consumer, OldProvider, MakeFunctionDeclaration(OldProvider)));

		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(
			*TestRunner, {Consumer, CurrentProvider}, Generation)));
		FAngelscriptCacheDependentRecompileWave Wave;
		const FAngelscriptCacheDependencyPropagationResult Result =
			Plan(Generation, nullptr, Wave);
		LogWave(*TestRunner, TEXT("import-current-target"), Result, Wave);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), *Result.Detail));
		ASSERT_THAT(AreEqual(1, Wave.Modules.Num()));
		ASSERT_THAT(IsTrue(Wave.Modules[0].ModuleKey == Consumer.ModuleKey));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::AbiMismatch,
			Wave.Modules[0].Reasons[0].Reason));
	}

	TEST_METHOD(DeclaredFunctionWithoutBodyReportsContentUnavailable)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		FModuleSpec Provider = MakeModule(0x10, TEXT("Provider"),
			TEXT("Provide"), 0x31);
		const FAngelscriptCachedDeclaration DeclarationOnly =
			MakeModuleInitializerDeclaration(Provider);
		Provider.AdditionalDeclarations.Add(DeclarationOnly);
		FModuleSpec Consumer = MakeModule(0x20, TEXT("Consumer"),
			TEXT("Consume"), 0x41);
		Consumer.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::FunctionContent,
			EAngelscriptCacheReferenceKind::ScriptFunction,
			DeclarationOnly.StableKey,
			DeclarationOnly.SignatureHash,
			MakeHash(0xa1)));

		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(BuildGeneration(
			*TestRunner, {Provider, Consumer}, Generation)));
		FAngelscriptCacheDependentRecompileWave Wave;
		const FAngelscriptCacheDependencyPropagationResult Result =
			Plan(Generation, nullptr, Wave);
		LogWave(*TestRunner, TEXT("content-unavailable"), Result, Wave);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), *Result.Detail));
		ASSERT_THAT(AreEqual(1, Wave.Modules.Num()));
		ASSERT_THAT(IsTrue(Wave.Modules[0].ModuleKey == Consumer.ModuleKey));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDependencyMissReason::ContentUnavailable,
			Wave.Modules[0].Reasons[0].Reason));
	}

	TEST_METHOD(FixedPointReturnsOnlyTheNextVerifiedWave)
	{
		using namespace AngelscriptCacheDependencyPropagationTests_Private;
		const FModuleSpec AOld = MakeModule(0x10, TEXT("A"), TEXT("AWork"), 0x31, 0);
		FModuleSpec ANew = AOld;
		ANew.FunctionTraitFlags = 1;
		const FModuleSpec BOld = MakeModule(0x20, TEXT("B"), TEXT("BWork"), 0x41, 0);
		FModuleSpec BNew = BOld;
		BNew.FunctionTraitFlags = 1;
		FModuleSpec C = MakeModule(0x30, TEXT("C"), TEXT("CWork"), 0x51, 0);

		FModuleSpec BBeforeCompile = BOld;
		BBeforeCompile.Dependencies.Add(MakeFunctionDependency(
			MakeFunctionDeclaration(AOld)));
		FModuleSpec CBeforeCompile = C;
		CBeforeCompile.Dependencies.Add(MakeFunctionDependency(
			MakeFunctionDeclaration(BOld)));
		FAngelscriptValidatedGeneration FirstGeneration;
		ASSERT_THAT(IsTrue(BuildGeneration(*TestRunner,
			{ANew, BBeforeCompile, CBeforeCompile}, FirstGeneration)));
		FAngelscriptCacheDependentRecompileWave FirstWave;
		const FAngelscriptCacheDependencyPropagationResult FirstResult =
			Plan(FirstGeneration, nullptr, FirstWave);
		ASSERT_THAT(IsTrue(FirstResult.IsSuccess(), *FirstResult.Detail));
		ASSERT_THAT(AreEqual(1, FirstWave.Modules.Num()));
		ASSERT_THAT(IsTrue(FirstWave.Modules[0].ModuleKey == BOld.ModuleKey));

		BNew.Dependencies.Add(MakeFunctionDependency(
			MakeFunctionDeclaration(ANew)));
		FAngelscriptValidatedGeneration SecondGeneration;
		ASSERT_THAT(IsTrue(BuildGeneration(*TestRunner,
			{ANew, BNew, CBeforeCompile}, SecondGeneration)));
		FAngelscriptCacheDependentRecompileWave SecondWave;
		const FAngelscriptCacheDependencyPropagationResult SecondResult =
			Plan(SecondGeneration, nullptr, SecondWave);
		ASSERT_THAT(IsTrue(SecondResult.IsSuccess(), *SecondResult.Detail));
		ASSERT_THAT(AreEqual(1, SecondWave.Modules.Num()));
		ASSERT_THAT(IsTrue(SecondWave.Modules[0].ModuleKey == C.ModuleKey));

		FModuleSpec CAfterCompile = C;
		CAfterCompile.Dependencies.Add(MakeFunctionDependency(
			MakeFunctionDeclaration(BNew)));
		FAngelscriptValidatedGeneration FinalGeneration;
		ASSERT_THAT(IsTrue(BuildGeneration(*TestRunner,
			{ANew, BNew, CAfterCompile}, FinalGeneration)));
		FAngelscriptCacheDependentRecompileWave FinalWave;
		const FAngelscriptCacheDependencyPropagationResult FinalResult =
			Plan(FinalGeneration, nullptr, FinalWave);
		LogWave(*TestRunner, TEXT("fixed-point-final"), FinalResult, FinalWave);
		ASSERT_THAT(IsTrue(FinalResult.IsSuccess(), *FinalResult.Detail));
		ASSERT_THAT(AreEqual(0, FinalWave.Modules.Num()));
	}
};

#endif
