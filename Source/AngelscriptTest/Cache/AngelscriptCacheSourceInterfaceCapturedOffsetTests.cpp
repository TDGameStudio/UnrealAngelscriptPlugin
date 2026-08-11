#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private
{
	template <typename EnumType, SIZE_T Count>
	consteval bool AreContiguousFromZero(const EnumType (&Values)[Count])
	{
		for (SIZE_T Index = 0; Index < Count; ++Index)
		{
			if (static_cast<uint16>(Values[Index]) != Index)
			{
				return false;
			}
		}
		return true;
	}

	constexpr EAngelscriptSourceIndexCapturedField SourceFields[] = {
		EAngelscriptSourceIndexCapturedField::Invalid,
		EAngelscriptSourceIndexCapturedField::PayloadSchemaVersion,
		EAngelscriptSourceIndexCapturedField::SourceSnapshot,
		EAngelscriptSourceIndexCapturedField::DiscoveryPolicy,
		EAngelscriptSourceIndexCapturedField::DiscoveryPolicyVersion,
		EAngelscriptSourceIndexCapturedField::DiscoveryPolicyFilterFlags,
		EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOptions,
		EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOption,
		EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOptionCanonicalKey,
		EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOptionValueFingerprint,
		EAngelscriptSourceIndexCapturedField::Mounts,
		EAngelscriptSourceIndexCapturedField::Mount,
		EAngelscriptSourceIndexCapturedField::MountKey,
		EAngelscriptSourceIndexCapturedField::MountSourceKind,
		EAngelscriptSourceIndexCapturedField::MountLogicalMount,
		EAngelscriptSourceIndexCapturedField::MountProviderKey,
		EAngelscriptSourceIndexCapturedField::MountRootConfigurationFingerprint,
		EAngelscriptSourceIndexCapturedField::MountOptions,
		EAngelscriptSourceIndexCapturedField::MountOption,
		EAngelscriptSourceIndexCapturedField::MountOptionCanonicalKey,
		EAngelscriptSourceIndexCapturedField::MountOptionValueFingerprint,
		EAngelscriptSourceIndexCapturedField::Providers,
		EAngelscriptSourceIndexCapturedField::Provider,
		EAngelscriptSourceIndexCapturedField::ProviderKey,
		EAngelscriptSourceIndexCapturedField::ProviderKind,
		EAngelscriptSourceIndexCapturedField::ProviderCanonicalImplementationIdentity,
		EAngelscriptSourceIndexCapturedField::ProviderIdentityFingerprint,
		EAngelscriptSourceIndexCapturedField::ProviderVersionFingerprintPresence,
		EAngelscriptSourceIndexCapturedField::ProviderVersionFingerprint,
		EAngelscriptSourceIndexCapturedField::ProviderConfigurationFingerprintPresence,
		EAngelscriptSourceIndexCapturedField::ProviderConfigurationFingerprint,
		EAngelscriptSourceIndexCapturedField::ProviderContentFingerprintPresence,
		EAngelscriptSourceIndexCapturedField::ProviderContentFingerprint,
		EAngelscriptSourceIndexCapturedField::ProviderCapabilityFlags,
		EAngelscriptSourceIndexCapturedField::PreprocessHooks,
		EAngelscriptSourceIndexCapturedField::PreprocessHook,
		EAngelscriptSourceIndexCapturedField::HookKey,
		EAngelscriptSourceIndexCapturedField::HookPhase,
		EAngelscriptSourceIndexCapturedField::HookCanonicalImplementationIdentity,
		EAngelscriptSourceIndexCapturedField::HookAffectedScopeKind,
		EAngelscriptSourceIndexCapturedField::HookAffectedScopeStableKey,
		EAngelscriptSourceIndexCapturedField::HookIdentityFingerprint,
		EAngelscriptSourceIndexCapturedField::HookVersionFingerprintPresence,
		EAngelscriptSourceIndexCapturedField::HookVersionFingerprint,
		EAngelscriptSourceIndexCapturedField::HookConfigurationFingerprintPresence,
		EAngelscriptSourceIndexCapturedField::HookConfigurationFingerprint,
		EAngelscriptSourceIndexCapturedField::HookContentFingerprintPresence,
		EAngelscriptSourceIndexCapturedField::HookContentFingerprint,
		EAngelscriptSourceIndexCapturedField::HookCapabilityFlags,
		EAngelscriptSourceIndexCapturedField::Files,
		EAngelscriptSourceIndexCapturedField::File,
		EAngelscriptSourceIndexCapturedField::FileSourceFileKey,
		EAngelscriptSourceIndexCapturedField::FileSourceKind,
		EAngelscriptSourceIndexCapturedField::FileMountKey,
		EAngelscriptSourceIndexCapturedField::FileProviderKey,
		EAngelscriptSourceIndexCapturedField::FileRelativeLogicalPath,
		EAngelscriptSourceIndexCapturedField::FileRawContentHash,
		EAngelscriptSourceIndexCapturedField::FileGeneratedSourceKeyPresence,
		EAngelscriptSourceIndexCapturedField::FileGeneratedSourceKey,
		EAngelscriptSourceIndexCapturedField::FileGeneratedConfigurationFingerprintPresence,
		EAngelscriptSourceIndexCapturedField::FileGeneratedConfigurationFingerprint,
		EAngelscriptSourceIndexCapturedField::FileModuleKey,
		EAngelscriptSourceIndexCapturedField::PreprocessorInputs,
		EAngelscriptSourceIndexCapturedField::PreprocessorInput,
		EAngelscriptSourceIndexCapturedField::InputKey,
		EAngelscriptSourceIndexCapturedField::InputOwnerScopeKind,
		EAngelscriptSourceIndexCapturedField::InputOwnerScopeStableKey,
		EAngelscriptSourceIndexCapturedField::InputKind,
		EAngelscriptSourceIndexCapturedField::InputCanonicalName,
		EAngelscriptSourceIndexCapturedField::InputTargetKind,
		EAngelscriptSourceIndexCapturedField::InputTargetStableKeyPresence,
		EAngelscriptSourceIndexCapturedField::InputTargetStableKey,
		EAngelscriptSourceIndexCapturedField::InputEffectiveValueOrContentHash,
		EAngelscriptSourceIndexCapturedField::Edges,
		EAngelscriptSourceIndexCapturedField::Edge,
		EAngelscriptSourceIndexCapturedField::EdgeKey,
		EAngelscriptSourceIndexCapturedField::EdgeKind,
		EAngelscriptSourceIndexCapturedField::EdgeFromSourceFileKey,
		EAngelscriptSourceIndexCapturedField::EdgeToSourceOrGeneratedKey,
		EAngelscriptSourceIndexCapturedField::EdgeCanonicalIncludeOrGeneratorIdentity,
		EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinalPresence,
		EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinal,
		EAngelscriptSourceIndexCapturedField::IneligibleScopes,
		EAngelscriptSourceIndexCapturedField::IneligibleScope,
		EAngelscriptSourceIndexCapturedField::IneligibleScopeKind,
		EAngelscriptSourceIndexCapturedField::IneligibleScopeStableKey,
		EAngelscriptSourceIndexCapturedField::IneligibleScopeReason,
		EAngelscriptSourceIndexCapturedField::IneligibleScopeCanonicalDiagnosticIdentity,
		EAngelscriptSourceIndexCapturedField::IneligibleScopeObservedFingerprintPresence,
		EAngelscriptSourceIndexCapturedField::IneligibleScopeObservedFingerprint,
	};
	static_assert(UE_ARRAY_COUNT(SourceFields) == 90);
	static_assert(AreContiguousFromZero(SourceFields));

	constexpr EAngelscriptModuleInterfaceCapturedField ModuleFields[] = {
		EAngelscriptModuleInterfaceCapturedField::Invalid,
		EAngelscriptModuleInterfaceCapturedField::PayloadSchemaVersion,
		EAngelscriptModuleInterfaceCapturedField::ModuleKey,
		EAngelscriptModuleInterfaceCapturedField::CanonicalModuleName,
		EAngelscriptModuleInterfaceCapturedField::InterfaceAbi,
		EAngelscriptModuleInterfaceCapturedField::CanonicalNamespaces,
		EAngelscriptModuleInterfaceCapturedField::CanonicalNamespace,
		EAngelscriptModuleInterfaceCapturedField::Declarations,
		EAngelscriptModuleInterfaceCapturedField::Declaration,
		EAngelscriptModuleInterfaceCapturedField::DeclarationKind,
		EAngelscriptModuleInterfaceCapturedField::DeclarationEntityKind,
		EAngelscriptModuleInterfaceCapturedField::DeclarationSchemaCoverage,
		EAngelscriptModuleInterfaceCapturedField::DeclarationBodyCoverage,
		EAngelscriptModuleInterfaceCapturedField::DeclarationStableKey,
		EAngelscriptModuleInterfaceCapturedField::DeclarationOwnerKind,
		EAngelscriptModuleInterfaceCapturedField::DeclarationOwnerKey,
		EAngelscriptModuleInterfaceCapturedField::DeclarationModuleKey,
		EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalNamespace,
		EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalName,
		EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalDeclaration,
		EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalIdentityTraits,
		EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalIdentityTrait,
		EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalTypeSpellingPresence,
		EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalTypeSpelling,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypePresence,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeNode,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeKind,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypePrimitive,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferencePresence,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReference,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceKind,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceStableKey,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceExpectedAbi,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeQualifierFlags,
		EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeOrderedSubTypes,
		EAngelscriptModuleInterfaceCapturedField::DeclarationOrderedParameters,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameter,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterOrdinal,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterCanonicalName,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeNode,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeKind,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypePrimitive,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferencePresence,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReference,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceKind,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceStableKey,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceExpectedAbi,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeQualifierFlags,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeOrderedSubTypes,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterPassing,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterDefaultExpressionPresence,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterDefaultExpression,
		EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTraitFlags,
		EAngelscriptModuleInterfaceCapturedField::DeclarationTraitFlags,
		EAngelscriptModuleInterfaceCapturedField::DeclarationReflectionFlags,
		EAngelscriptModuleInterfaceCapturedField::DeclarationMetadata,
		EAngelscriptModuleInterfaceCapturedField::DeclarationMetadataEntry,
		EAngelscriptModuleInterfaceCapturedField::DeclarationMetadataCanonicalKey,
		EAngelscriptModuleInterfaceCapturedField::DeclarationMetadataCanonicalValue,
		EAngelscriptModuleInterfaceCapturedField::DeclarationSlots,
		EAngelscriptModuleInterfaceCapturedField::DeclarationSlot,
		EAngelscriptModuleInterfaceCapturedField::DeclarationSlotKind,
		EAngelscriptModuleInterfaceCapturedField::DeclarationSlotOrdinal,
		EAngelscriptModuleInterfaceCapturedField::DeclarationSignatureHash,
		EAngelscriptModuleInterfaceCapturedField::DeclarationTraitsHash,
		EAngelscriptModuleInterfaceCapturedField::Imports,
		EAngelscriptModuleInterfaceCapturedField::Import,
		EAngelscriptModuleInterfaceCapturedField::ImportKey,
		EAngelscriptModuleInterfaceCapturedField::ImportCanonicalNamespace,
		EAngelscriptModuleInterfaceCapturedField::ImportCanonicalName,
		EAngelscriptModuleInterfaceCapturedField::ImportCanonicalSignature,
		EAngelscriptModuleInterfaceCapturedField::ImportTargetModuleKey,
		EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclaration,
		EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationReferenceKind,
		EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationStableKey,
		EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationExpectedAbi,
		EAngelscriptModuleInterfaceCapturedField::ImportSlots,
		EAngelscriptModuleInterfaceCapturedField::ImportSlot,
		EAngelscriptModuleInterfaceCapturedField::ImportSlotKind,
		EAngelscriptModuleInterfaceCapturedField::ImportSlotOrdinal,
		EAngelscriptModuleInterfaceCapturedField::Dependencies,
		EAngelscriptModuleInterfaceCapturedField::Dependency,
		EAngelscriptModuleInterfaceCapturedField::DependencyKind,
		EAngelscriptModuleInterfaceCapturedField::DependencyTarget,
		EAngelscriptModuleInterfaceCapturedField::DependencyTargetReferenceKind,
		EAngelscriptModuleInterfaceCapturedField::DependencyTargetStableKey,
		EAngelscriptModuleInterfaceCapturedField::DependencyTargetExpectedAbi,
		EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValuePresence,
		EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValue,
	};
	static_assert(UE_ARRAY_COUNT(ModuleFields) == 89);
	static_assert(AreContiguousFromZero(ModuleFields));

	static FAngelscriptHash256 MakeHash(const uint8 Fill)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Fill, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptStableModuleKey MakeModuleKey()
	{
		const TOptional<FAngelscriptStableModuleKey> Key =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("Game"), TEXT("Actors/Hero.as"), TEXT("Hero"));
		check(Key.IsSet());
		return Key.GetValue();
	}

	static FAngelscriptCachedDataType MakePrimitive(
		const EAngelscriptCachedPrimitiveType Primitive)
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = Primitive;
		return Type;
	}

	static FAngelscriptCachedSourceIndex MakeSourceIndexWithOneProvider()
	{
		FAngelscriptCachedSourceIndex Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Value.DiscoveryPolicy.PolicyVersion = 1;
		Value.DiscoveryPolicy.Options.Add({TEXT("Defines.MODE"), MakeHash(0x01)});

		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		Provider.CanonicalImplementationIdentity = TEXT("Runtime.DiskSourceProvider");
		Provider.IdentityFingerprint = MakeHash(0x10);
		Provider.VersionFingerprint = MakeHash(0x11);
		Provider.ConfigurationFingerprint = MakeHash(0x12);
		Provider.ContentFingerprint = MakeHash(0x13);
		Provider.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
			{Provider.ProviderKind, Provider.CanonicalImplementationIdentity,
				Provider.IdentityFingerprint}, Provider.ProviderKey).IsSuccess());
		Value.Providers.Add(Provider);

		FAngelscriptCachedSourceMount Mount;
		Mount.SourceKind = EAngelscriptCachedSourceKind::Game;
		Mount.LogicalMount = TEXT("Game");
		Mount.ProviderKey = Provider.ProviderKey;
		Mount.RootConfigurationFingerprint = MakeHash(0x20);
		Mount.Options.Add({TEXT("Recursive"), MakeHash(0x21)});
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
			{Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey},
			Mount.MountKey).IsSuccess());
		Value.Mounts.Add(Mount);

		FAngelscriptCachedPreprocessHook Hook;
		Hook.Phase = EAngelscriptCachedPreprocessHookPhase::ProcessChunks;
		Hook.CanonicalImplementationIdentity = TEXT("Runtime.StandardPreprocessor");
		Hook.AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Mount;
		Hook.AffectedScopeStableKey = Mount.MountKey.Hash;
		Hook.IdentityFingerprint = MakeHash(0x30);
		Hook.VersionFingerprint = MakeHash(0x31);
		Hook.ConfigurationFingerprint = MakeHash(0x32);
		Hook.ContentFingerprint = MakeHash(0x33);
		Hook.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(
			{Hook.Phase, Hook.CanonicalImplementationIdentity,
				Hook.AffectedScopeKind, Hook.AffectedScopeStableKey},
			Hook.HookKey).IsSuccess());
		Value.PreprocessHooks.Add(Hook);

		FAngelscriptCachedSourceFile File;
		File.SourceKind = EAngelscriptCachedSourceKind::Game;
		File.MountKey = Mount.MountKey;
		File.ProviderKey = Provider.ProviderKey;
		File.RelativeLogicalPath = TEXT("Actors/Hero.as");
		File.RawContentHash = MakeHash(0x40);
		File.ModuleKey = MakeModuleKey();
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(
			{File.SourceKind, File.MountKey, File.ProviderKey,
				File.RelativeLogicalPath, File.GeneratedSourceKey},
			File.SourceFileKey).IsSuccess());
		Value.Files.Add(File);

		FAngelscriptCachedPreprocessorInput Input;
		Input.OwnerScopeKind = EAngelscriptCachedFastPathScopeKind::SourceFile;
		Input.OwnerScopeStableKey = File.SourceFileKey.Hash;
		Input.InputKind = EAngelscriptCachePreprocessorInputKind::Define;
		Input.CanonicalName = TEXT("WITH_EDITOR");
		Input.TargetKind = EAngelscriptCachePreprocessorInputTargetKind::SourceFile;
		Input.TargetStableKey = File.SourceFileKey.Hash;
		Input.EffectiveValueOrContentHash = MakeHash(0x50);
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(
			{Input.OwnerScopeStableKey, Input.InputKind, Input.CanonicalName,
				Input.TargetStableKey}, Input.InputKey).IsSuccess());
		Value.PreprocessorInputs.Add(Input);

		FAngelscriptCachedSourceEdge Edge;
		Edge.EdgeKind = EAngelscriptCachedSourceEdgeKind::Include;
		Edge.FromSourceFileKey = File.SourceFileKey;
		Edge.ToSourceOrGeneratedKey = File.SourceFileKey.Hash;
		Edge.CanonicalIncludeOrGeneratorIdentity = TEXT("SelfIncludeFixture");
		Edge.SemanticOrdinal = 0u;
		check(FAngelscriptCacheSemanticArchive::TryBuildSourceEdgeKey(
			{Edge.EdgeKind, Edge.FromSourceFileKey,
				Edge.ToSourceOrGeneratedKey,
				Edge.CanonicalIncludeOrGeneratorIdentity},
			Edge.EdgeKey).IsSuccess());
		Value.Edges.Add(Edge);

		FAngelscriptCachedFastPathIneligibleScope Ineligible;
		Ineligible.ScopeKind = EAngelscriptCachedFastPathScopeKind::Module;
		Ineligible.ScopeStableKey = File.ModuleKey.Hash;
		Ineligible.Reason =
			EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior;
		Ineligible.CanonicalDiagnosticIdentity = TEXT("ModuleUsesDynamicFixture");
		Ineligible.ObservedFingerprint = MakeHash(0x61);
		Value.IneligibleScopes.Add(Ineligible);

		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Value, Value.SourceSnapshot).IsSuccess());
		return Value;
	}

	static FAngelscriptCachedModuleInterface MakeInterfaceWithOneParameter()
	{
		FAngelscriptCachedModuleInterface Value;
		Value.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		Value.ModuleKey = MakeModuleKey();
		Value.CanonicalModuleName = TEXT("Hero");
		Value.CanonicalNamespaces = {TEXT("Engine"), TEXT("Gameplay")};

		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Function;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::GlobalFunction;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Required;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Declaration.OwnerKey = Value.ModuleKey.Hash;
		Declaration.ModuleKey = Value.ModuleKey;
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
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		Value.Declarations.Add(Declaration);

		FAngelscriptCachedImportDeclaration Import;
		Import.CanonicalNamespace = TEXT("Engine");
		Import.CanonicalName = TEXT("Log");
		Import.CanonicalSignature = TEXT("void Log(const string&in Message)");
		Import.TargetModuleKey = FAngelscriptStableModuleKey{MakeHash(0x70)};
		Import.TargetDeclaration = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptFunction,
			MakeHash(0x71), MakeHash(0x72)};
		Import.Slots.Add({EAngelscriptCacheDeclarationSlotKind::Import, 0});
		check(FAngelscriptCacheSemanticArchive::TryBuildImportKey(
			{Value.ModuleKey, Import.CanonicalNamespace, Import.CanonicalName,
				Import.CanonicalSignature, Import.TargetModuleKey,
				FAngelscriptStableFunctionKey{
					Import.TargetDeclaration.StableKey}},
			Import.ImportKey).IsSuccess());
		Value.Imports.Add(Import);

		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::HardValue;
		Dependency.Target = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptGlobal,
			MakeHash(0x73), MakeHash(0x74)};
		Dependency.ExpectedContentOrValue = MakeHash(0x75);
		Value.Dependencies.Add(Dependency);

		check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			Value, Value.InterfaceAbi).IsSuccess());
		return Value;
	}

	static FAngelscriptCacheValidationResult RefreshDeclarationIdentityAndHashes(
		FAngelscriptCachedDeclaration& Declaration)
	{
		FAngelscriptFunctionIdentityDescriptor Identity;
		Identity.OwnerKind = Declaration.OwnerKind;
		Identity.OwnerKey = Declaration.OwnerKey;
		Identity.Namespace = Declaration.CanonicalNamespace;
		Identity.Kind = Declaration.EntityKind;
		Identity.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity).Hash;
		return FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration,
			Declaration.SignatureHash,
			Declaration.TraitsHash);
	}

	static FAngelscriptCacheValidationResult RefreshInterfaceAbi(
		FAngelscriptCachedModuleInterface& Value)
	{
		return FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
			Value, Value.InterfaceAbi);
	}

	static uint32 ReadUInt32(const TArray<uint8>& Bytes, const uint64 Offset)
	{
		check(Offset <= static_cast<uint64>(Bytes.Num()) - 4);
		return static_cast<uint32>(Bytes[Offset])
			| (static_cast<uint32>(Bytes[Offset + 1]) << 8)
			| (static_cast<uint32>(Bytes[Offset + 2]) << 16)
			| (static_cast<uint32>(Bytes[Offset + 3]) << 24);
	}

	static void WriteUInt32(TArray<uint8>& Bytes, const uint64 Offset, const uint32 Value)
	{
		check(Offset <= static_cast<uint64>(Bytes.Num()) - 4);
		Bytes[Offset] = static_cast<uint8>(Value);
		Bytes[Offset + 1] = static_cast<uint8>(Value >> 8);
		Bytes[Offset + 2] = static_cast<uint8>(Value >> 16);
		Bytes[Offset + 3] = static_cast<uint8>(Value >> 24);
	}

	static void WriteZeroHash(TArray<uint8>& Bytes, const uint64 Offset)
	{
		check(Offset <= static_cast<uint64>(Bytes.Num()) - 32);
		FMemory::Memzero(Bytes.GetData() + Offset, 32);
	}

	static FAngelscriptCacheValidationResult Decode(
		const EAngelscriptCacheRecordKind Kind,
		const TArray<uint8>& Payload,
		FAngelscriptCacheReadBudget& Budget,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
	{
		FAngelscriptCacheRecordId RecordId;
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			Kind, Payload, RecordId).IsSuccess());
		return FAngelscriptDecodedCacheRecord::TryDecode(
			RecordId, Payload, FAngelscriptCacheReadLimits{}, Budget, OutRecord);
	}

	struct FAllocationProbeCapture
	{
		TArray<FAngelscriptCacheTypeSchemaProbeEventForTests> Storage;
		int32 EventCount = 0;
		bool bOverflowed = false;
		TUniquePtr<FAngelscriptCacheTypeSchemaAllocationProbeForTests> Probe;

		FAllocationProbeCapture()
		{
			Storage.SetNum(2048);
			Probe = MakeUnique<FAngelscriptCacheTypeSchemaAllocationProbeForTests>(
				MakeArrayView(Storage), EventCount, bOverflowed);
		}

		TConstArrayView<FAngelscriptCacheTypeSchemaProbeEventForTests> Events() const
		{
			return MakeArrayView(Storage).Left(EventCount);
		}
	};

	static FAngelscriptCacheValidationResult DecodeWithProbe(
		const EAngelscriptCacheRecordKind Kind,
		const TArray<uint8>& Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAllocationProbeCapture& Capture,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
	{
		FAngelscriptCacheRecordId RecordId;
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			Kind, Payload, RecordId).IsSuccess());
		return FAngelscriptDecodedCacheRecordTestAccess::TryDecodeWithProbe(
			RecordId, Payload, Limits, Budget, *Capture.Probe, OutRecord);
	}

	static FAngelscriptSourceIndexFieldCoordinate MakeSourceCoordinate(
		const EAngelscriptSourceIndexCapturedField Field)
	{
		FAngelscriptSourceIndexFieldCoordinate Coordinate{Field};
		const uint16 Raw = static_cast<uint16>(Field);
		if ((Raw >= 7 && Raw <= 9)
			|| (Raw >= 11 && Raw <= 17)
			|| (Raw >= 22 && Raw <= 33)
			|| (Raw >= 35 && Raw <= 48)
			|| (Raw >= 50 && Raw <= 61)
			|| (Raw >= 63 && Raw <= 72)
			|| (Raw >= 74 && Raw <= 81)
			|| (Raw >= 83 && Raw <= 89))
		{
			Coordinate.PrimaryIndex = 0;
		}
		else if (Raw >= 18 && Raw <= 20)
		{
			Coordinate.PrimaryIndex = 0;
			Coordinate.SecondaryIndex = 0;
		}
		return Coordinate;
	}

	static FAngelscriptModuleInterfaceFieldCoordinate MakeModuleCoordinate(
		const EAngelscriptModuleInterfaceCapturedField Field)
	{
		FAngelscriptModuleInterfaceFieldCoordinate Coordinate{Field};
		const uint16 Raw = static_cast<uint16>(Field);
		if (Raw == 6 || (Raw >= 8 && Raw <= 20) || (Raw >= 22 && Raw <= 24)
			|| Raw == 35 || (Raw >= 53 && Raw <= 55) || Raw == 59
			|| (Raw >= 63 && Raw <= 64) || (Raw >= 66 && Raw <= 76)
			|| (Raw >= 81 && Raw <= 88))
		{
			Coordinate.PrimaryIndex = 0;
		}
		else if (Raw == 21 || (Raw >= 25 && Raw <= 38)
			|| (Raw >= 49 && Raw <= 52) || (Raw >= 56 && Raw <= 58)
			|| (Raw >= 60 && Raw <= 62) || (Raw >= 77 && Raw <= 79))
		{
			Coordinate.PrimaryIndex = 0;
			Coordinate.SecondaryIndex = 0;
		}
		else if (Raw >= 39 && Raw <= 48)
		{
			Coordinate.PrimaryIndex = 0;
			Coordinate.SecondaryIndex = 0;
			Coordinate.TertiaryIndex = 0;
		}
		return Coordinate;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheSourceInterfaceCapturedOffsetTests,
	"Angelscript.TestModule.Cache.Archive.SourceInterfaceCapturedOffsets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CapturedCoordinateEnumsAndEveryIndexShapeAreFrozen)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		TArray<uint8> SourcePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			MakeSourceIndexWithOneProvider(), SourcePayload).IsSuccess());
		FAngelscriptCacheReadBudget SourceBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Source;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::SourceIndex,
			SourcePayload, SourceBudget, Source).IsSuccess()));

		TArray<uint8> ModulePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			MakeInterfaceWithOneParameter(), ModulePayload).IsSuccess());
		FAngelscriptCacheReadBudget ModuleBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Module;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::ModuleInterface,
			ModulePayload, ModuleBudget, Module).IsSuccess()));

		ASSERT_THAT(IsFalse(Source.GetValue()->FindCapturedOffset(
			{EAngelscriptSourceIndexCapturedField::Invalid}).IsSet()));
		ASSERT_THAT(IsFalse(Source.GetValue()->FindCapturedOffset({
			static_cast<EAngelscriptSourceIndexCapturedField>(90)}).IsSet()));
		for (uint16 Raw = 1; Raw <= 89; ++Raw)
		{
			const EAngelscriptSourceIndexCapturedField Field =
				static_cast<EAngelscriptSourceIndexCapturedField>(Raw);
			const FAngelscriptSourceIndexFieldCoordinate Coordinate =
				MakeSourceCoordinate(Field);
			const bool bOptionalValueAbsent =
				Field == EAngelscriptSourceIndexCapturedField::FileGeneratedSourceKey
				|| Field == EAngelscriptSourceIndexCapturedField::FileGeneratedConfigurationFingerprint;
			ASSERT_THAT(AreEqual(!bOptionalValueAbsent,
				Source.GetValue()->FindCapturedOffset(Coordinate).IsSet()));
			ASSERT_THAT(IsFalse(Module.GetValue()->FindCapturedOffset(Coordinate).IsSet()));

			FAngelscriptSourceIndexFieldCoordinate Unused = Coordinate;
			if (Coordinate.PrimaryIndex == MAX_uint32)
			{
				Unused.PrimaryIndex = 0;
			}
			else if (Coordinate.SecondaryIndex == MAX_uint32)
			{
				Unused.SecondaryIndex = 0;
			}
			else
			{
				Unused.TertiaryIndex = 0;
			}
			ASSERT_THAT(IsFalse(Source.GetValue()->FindCapturedOffset(
				Unused).IsSet()));

			if (Coordinate.PrimaryIndex != MAX_uint32)
			{
				FAngelscriptSourceIndexFieldCoordinate Missing = Coordinate;
				if (Coordinate.SecondaryIndex != MAX_uint32)
				{
					Missing.SecondaryIndex = MAX_uint32;
				}
				else
				{
					Missing.PrimaryIndex = MAX_uint32;
				}
				ASSERT_THAT(IsFalse(Source.GetValue()->FindCapturedOffset(
					Missing).IsSet()));

				FAngelscriptSourceIndexFieldCoordinate OutOfRange = Coordinate;
				OutOfRange.PrimaryIndex = 999;
				ASSERT_THAT(IsFalse(Source.GetValue()->FindCapturedOffset(
					OutOfRange).IsSet()));
			}
		}

		ASSERT_THAT(IsFalse(Module.GetValue()->FindCapturedOffset(
			{EAngelscriptModuleInterfaceCapturedField::Invalid}).IsSet()));
		ASSERT_THAT(IsFalse(Module.GetValue()->FindCapturedOffset({
			static_cast<EAngelscriptModuleInterfaceCapturedField>(89)}).IsSet()));
		for (uint16 Raw = 1; Raw <= 88; ++Raw)
		{
			const EAngelscriptModuleInterfaceCapturedField Field =
				static_cast<EAngelscriptModuleInterfaceCapturedField>(Raw);
			const FAngelscriptModuleInterfaceFieldCoordinate Coordinate =
				MakeModuleCoordinate(Field);
			const bool bOptionalValueAbsent =
				Field == EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalTypeSpelling
				|| (Raw >= 29 && Raw <= 32)
				|| (Raw >= 43 && Raw <= 46)
				|| Field == EAngelscriptModuleInterfaceCapturedField::DeclarationParameterDefaultExpression;
			ASSERT_THAT(AreEqual(!bOptionalValueAbsent,
				Module.GetValue()->FindCapturedOffset(Coordinate).IsSet()));
			ASSERT_THAT(IsFalse(Source.GetValue()->FindCapturedOffset(Coordinate).IsSet()));

			FAngelscriptModuleInterfaceFieldCoordinate Unused = Coordinate;
			bool bHasUnusedIndex = true;
			if (Coordinate.PrimaryIndex == MAX_uint32)
			{
				Unused.PrimaryIndex = 0;
			}
			else if (Coordinate.SecondaryIndex == MAX_uint32)
			{
				Unused.SecondaryIndex = 0;
			}
			else if (Coordinate.TertiaryIndex == MAX_uint32)
			{
				Unused.TertiaryIndex = 0;
			}
			else
			{
				bHasUnusedIndex = false;
			}
			if (bHasUnusedIndex)
			{
				ASSERT_THAT(IsFalse(Module.GetValue()->FindCapturedOffset(
					Unused).IsSet()));
			}

			if (Coordinate.PrimaryIndex != MAX_uint32)
			{
				FAngelscriptModuleInterfaceFieldCoordinate Missing = Coordinate;
				if (Coordinate.TertiaryIndex != MAX_uint32)
				{
					Missing.TertiaryIndex = MAX_uint32;
				}
				else if (Coordinate.SecondaryIndex != MAX_uint32)
				{
					Missing.SecondaryIndex = MAX_uint32;
				}
				else
				{
					Missing.PrimaryIndex = MAX_uint32;
				}
				ASSERT_THAT(IsFalse(Module.GetValue()->FindCapturedOffset(
					Missing).IsSet()));

				FAngelscriptModuleInterfaceFieldCoordinate OutOfRange = Coordinate;
				OutOfRange.PrimaryIndex = 999;
				ASSERT_THAT(IsFalse(Module.GetValue()->FindCapturedOffset(
					OutOfRange).IsSet()));
			}
		}
	}

	TEST_METHOD(SourceProviderCapabilityPresenceFailureUsesTheExactNestedOffset)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		TArray<uint8> Payload;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			MakeSourceIndexWithOneProvider(), Payload).IsSuccess());
		FAngelscriptCacheReadBudget GoodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::SourceIndex,
			Payload, GoodBudget, Output).IsSuccess()));
		ASSERT_THAT(IsTrue(Output.IsSet()));
		const uint64 ProviderCountOffset = Output.GetValue()->FindCapturedOffset({
			EAngelscriptSourceIndexCapturedField::Providers}).GetValue();
		const uint64 CapabilityOffset = Output.GetValue()->FindCapturedOffset({
			EAngelscriptSourceIndexCapturedField::ProviderCapabilityFlags, 0}).GetValue();
		ASSERT_THAT(IsTrue(CapabilityOffset != ProviderCountOffset));
		const uint32 KnownCapabilities = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		ASSERT_THAT(AreEqual(KnownCapabilities, ReadUInt32(Payload, CapabilityOffset)));

		const uint32 MissingVersionCapability = KnownCapabilities
			& ~static_cast<uint32>(
				EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint);
		WriteUInt32(Payload, CapabilityOffset, MissingVersionCapability);
		FAngelscriptCacheReadBudget BadBudget;
		const FAngelscriptCacheValidationResult Result = Decode(
			EAngelscriptCacheRecordKind::SourceIndex, Payload, BadBudget, Output);

		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidPresence,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex,
			Result.RecordKind));
		ASSERT_THAT(AreEqual(CapabilityOffset, Result.ByteOffset));
		ASSERT_THAT(IsFalse(Output.IsSet()));
		ASSERT_THAT(AreEqual(uint64(0),
			BadBudget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(SourceScalarReferenceAndOrdinalFailuresUseTheirExactCoordinates)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		TArray<uint8> BaselinePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			MakeSourceIndexWithOneProvider(), BaselinePayload).IsSuccess());
		FAngelscriptCacheReadBudget GoodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Good;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::SourceIndex,
			BaselinePayload, GoodBudget, Good).IsSuccess()));

		enum class EMutation : uint8
		{
			ZeroHash,
			UnknownFlags,
			OrdinalOne,
		};
		struct FCase final
		{
			FAngelscriptSourceIndexFieldCoordinate Coordinate;
			EAngelscriptCacheValidationError Error;
			EMutation Mutation;
		};
		const FCase Cases[] = {
			{{EAngelscriptSourceIndexCapturedField::DiscoveryPolicyFilterFlags},
				EAngelscriptCacheValidationError::UnknownFlags,
				EMutation::UnknownFlags},
			{{EAngelscriptSourceIndexCapturedField::MountProviderKey, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptSourceIndexCapturedField::MountRootConfigurationFingerprint, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptSourceIndexCapturedField::HookAffectedScopeStableKey, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptSourceIndexCapturedField::FileRawContentHash, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptSourceIndexCapturedField::InputTargetStableKey, 0},
				EAngelscriptCacheValidationError::InvalidPresence,
				EMutation::ZeroHash},
			{{EAngelscriptSourceIndexCapturedField::InputEffectiveValueOrContentHash, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptSourceIndexCapturedField::EdgeFromSourceFileKey, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinal, 0},
				EAngelscriptCacheValidationError::OrdinalGap,
				EMutation::OrdinalOne},
			{{EAngelscriptSourceIndexCapturedField::IneligibleScopeStableKey, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptSourceIndexCapturedField::IneligibleScopeObservedFingerprint, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
		};

		for (const FCase& Case : Cases)
		{
			const uint64 ExactOffset =
				Good.GetValue()->FindCapturedOffset(Case.Coordinate).GetValue();
			TArray<uint8> Mutated = BaselinePayload;
			switch (Case.Mutation)
			{
			case EMutation::ZeroHash:
				WriteZeroHash(Mutated, ExactOffset);
				break;
			case EMutation::UnknownFlags:
				WriteUInt32(Mutated, ExactOffset, 0x80000000u);
				break;
			case EMutation::OrdinalOne:
				WriteUInt32(Mutated, ExactOffset, 1u);
				break;
			default:
				checkNoEntry();
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Good;
			FAngelscriptCacheReadBudget Budget;
			const FAngelscriptCacheValidationResult Result = Decode(
				EAngelscriptCacheRecordKind::SourceIndex,
				Mutated, Budget, Output);
			ASSERT_THAT(AreEqual(Case.Error, Result.Error));
			ASSERT_THAT(AreEqual(ExactOffset, Result.ByteOffset));
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		}
	}

	TEST_METHOD(ModuleParameterOrdinalGapUsesTheExactNestedOffset)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		TArray<uint8> Payload;
		check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			MakeInterfaceWithOneParameter(), Payload).IsSuccess());
		FAngelscriptCacheReadBudget GoodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::ModuleInterface,
			Payload, GoodBudget, Output).IsSuccess()));
		ASSERT_THAT(IsTrue(Output.IsSet()));
		const uint64 DeclarationCountOffset = Output.GetValue()->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::Declarations}).GetValue();
		const uint64 ParameterOrdinalOffset = Output.GetValue()->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterOrdinal,
			0, 0}).GetValue();
		ASSERT_THAT(IsTrue(ParameterOrdinalOffset != DeclarationCountOffset));
		ASSERT_THAT(AreEqual(uint32(0), ReadUInt32(Payload, ParameterOrdinalOffset)));

		WriteUInt32(Payload, ParameterOrdinalOffset, 1);
		FAngelscriptCacheReadBudget BadBudget;
		const FAngelscriptCacheValidationResult Result = Decode(
			EAngelscriptCacheRecordKind::ModuleInterface, Payload, BadBudget, Output);

		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::OrdinalGap,
			Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface,
			Result.RecordKind));
		ASSERT_THAT(AreEqual(ParameterOrdinalOffset, Result.ByteOffset));
		ASSERT_THAT(IsFalse(Output.IsSet()));
		ASSERT_THAT(AreEqual(uint64(0),
			BadBudget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(ModuleScalarReferenceAndDataTypeFailuresUseTheirExactCoordinates)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		TArray<uint8> BaselinePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			MakeInterfaceWithOneParameter(), BaselinePayload).IsSuccess());
		FAngelscriptCacheReadBudget GoodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Good;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::ModuleInterface,
			BaselinePayload, GoodBudget, Good).IsSuccess()));

		enum class EMutation : uint8
		{
			ZeroHash,
			UnknownFlags,
		};
		struct FCase final
		{
			FAngelscriptModuleInterfaceFieldCoordinate Coordinate;
			EAngelscriptCacheValidationError Error;
			EMutation Mutation;
		};
		const FCase Cases[] = {
			{{EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeQualifierFlags,
				0, 0}, EAngelscriptCacheValidationError::UnknownFlags,
				EMutation::UnknownFlags},
			{{EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeQualifierFlags,
				0, 0, 0}, EAngelscriptCacheValidationError::UnknownFlags,
				EMutation::UnknownFlags},
			{{EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTraitFlags,
				0, 0}, EAngelscriptCacheValidationError::UnknownFlags,
				EMutation::UnknownFlags},
			{{EAngelscriptModuleInterfaceCapturedField::DeclarationReflectionFlags, 0},
				EAngelscriptCacheValidationError::UnknownFlags,
				EMutation::UnknownFlags},
			{{EAngelscriptModuleInterfaceCapturedField::ImportTargetModuleKey, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationStableKey, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationExpectedAbi, 0},
				EAngelscriptCacheValidationError::MissingExpectedAbi,
				EMutation::ZeroHash},
			{{EAngelscriptModuleInterfaceCapturedField::DependencyTargetStableKey, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
			{{EAngelscriptModuleInterfaceCapturedField::DependencyTargetExpectedAbi, 0},
				EAngelscriptCacheValidationError::MissingExpectedAbi,
				EMutation::ZeroHash},
			{{EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValue, 0},
				EAngelscriptCacheValidationError::ZeroStableKey,
				EMutation::ZeroHash},
		};

		for (const FCase& Case : Cases)
		{
			const uint64 ExactOffset =
				Good.GetValue()->FindCapturedOffset(Case.Coordinate).GetValue();
			TArray<uint8> Mutated = BaselinePayload;
			if (Case.Mutation == EMutation::ZeroHash)
			{
				WriteZeroHash(Mutated, ExactOffset);
			}
			else
			{
				WriteUInt32(Mutated, ExactOffset, 0x80000000u);
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Good;
			FAngelscriptCacheReadBudget Budget;
			const FAngelscriptCacheValidationResult Result = Decode(
				EAngelscriptCacheRecordKind::ModuleInterface,
				Mutated, Budget, Output);
			ASSERT_THAT(AreEqual(Case.Error, Result.Error));
			ASSERT_THAT(AreEqual(ExactOffset, Result.ByteOffset));
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		}
	}

	TEST_METHOD(SourceDerivedKeysUseEachExactNestedStoredKeyOffset)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		TArray<uint8> BaselinePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			MakeSourceIndexWithOneProvider(), BaselinePayload).IsSuccess());
		FAngelscriptCacheReadBudget GoodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Good;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::SourceIndex,
			BaselinePayload, GoodBudget, Good).IsSuccess()));

		const EAngelscriptSourceIndexCapturedField KeyFields[] = {
			EAngelscriptSourceIndexCapturedField::MountKey,
			EAngelscriptSourceIndexCapturedField::ProviderKey,
			EAngelscriptSourceIndexCapturedField::HookKey,
			EAngelscriptSourceIndexCapturedField::FileSourceFileKey,
			EAngelscriptSourceIndexCapturedField::InputKey,
			EAngelscriptSourceIndexCapturedField::EdgeKey};
		for (const EAngelscriptSourceIndexCapturedField Field : KeyFields)
		{
			const uint64 ExactOffset = Good.GetValue()->FindCapturedOffset({
				Field, 0}).GetValue();
			TArray<uint8> Mutated = BaselinePayload;
			Mutated[ExactOffset] ^= 0x01;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Good;
			FAngelscriptCacheReadBudget Budget;
			const FAngelscriptCacheValidationResult Result = Decode(
				EAngelscriptCacheRecordKind::SourceIndex,
				Mutated, Budget, Output);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
				Result.Error));
			ASSERT_THAT(AreEqual(ExactOffset, Result.ByteOffset));
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		}
	}

	TEST_METHOD(ModuleDerivedHashesImportKeyAndSlotOrdinalUseExactNestedOffsets)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		TArray<uint8> BaselinePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			MakeInterfaceWithOneParameter(), BaselinePayload).IsSuccess());
		FAngelscriptCacheReadBudget GoodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Good;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::ModuleInterface,
			BaselinePayload, GoodBudget, Good).IsSuccess()));

		struct FMutationCase final
		{
			FAngelscriptModuleInterfaceFieldCoordinate Coordinate;
			EAngelscriptCacheValidationError Error;
			bool bWriteOrdinalOne = false;
		};
		const FMutationCase Cases[] = {
			{{EAngelscriptModuleInterfaceCapturedField::DeclarationStableKey, 0},
				EAngelscriptCacheValidationError::DerivedHashMismatch},
			{{EAngelscriptModuleInterfaceCapturedField::DeclarationSignatureHash, 0},
				EAngelscriptCacheValidationError::DerivedHashMismatch},
			{{EAngelscriptModuleInterfaceCapturedField::DeclarationTraitsHash, 0},
				EAngelscriptCacheValidationError::DerivedHashMismatch},
			{{EAngelscriptModuleInterfaceCapturedField::ImportKey, 0},
				EAngelscriptCacheValidationError::DerivedHashMismatch},
			{{EAngelscriptModuleInterfaceCapturedField::DeclarationSlotOrdinal, 0, 0},
				EAngelscriptCacheValidationError::OrdinalGap, true},
			{{EAngelscriptModuleInterfaceCapturedField::ImportSlotOrdinal, 0, 0},
				EAngelscriptCacheValidationError::OrdinalGap, true}};
		for (const FMutationCase& Case : Cases)
		{
			const uint64 ExactOffset =
				Good.GetValue()->FindCapturedOffset(Case.Coordinate).GetValue();
			TArray<uint8> Mutated = BaselinePayload;
			if (Case.bWriteOrdinalOne)
			{
				WriteUInt32(Mutated, ExactOffset, 1);
			}
			else
			{
				Mutated[ExactOffset] ^= 0x01;
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Good;
			FAngelscriptCacheReadBudget Budget;
			const FAngelscriptCacheValidationResult Result = Decode(
				EAngelscriptCacheRecordKind::ModuleInterface,
				Mutated, Budget, Output);
			ASSERT_THAT(AreEqual(Case.Error, Result.Error));
			ASSERT_THAT(AreEqual(ExactOffset, Result.ByteOffset));
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		}
	}

	TEST_METHOD(DuplicateAndOrderFailuresUseTheFirstProvingPhysicalRow)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		const auto DecodeCanonicalSource = [](
			const FAngelscriptCachedSourceIndex& Value)
		{
			TArray<uint8> Payload;
			check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				Value, Payload).IsSuccess());
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
			check(Decode(EAngelscriptCacheRecordKind::SourceIndex,
				Payload, Budget, Record).IsSuccess());
			return Record.GetValue();
		};
		const auto DecodeCanonicalModule = [](
			const FAngelscriptCachedModuleInterface& Value)
		{
			TArray<uint8> Payload;
			check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				Value, Payload).IsSuccess());
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
			check(Decode(EAngelscriptCacheRecordKind::ModuleInterface,
				Payload, Budget, Record).IsSuccess());
			return Record.GetValue();
		};
		const auto ExpectSourceFailure = [this](
			const FAngelscriptCachedSourceIndex& Value,
			const EAngelscriptCacheValidationError Error,
			const uint64 ExpectedOffset,
			const FAngelscriptDecodedCacheRecordHandle& Sentinel)
		{
			TArray<uint8> Payload;
			FAngelscriptCacheSemanticArchive::SerializeSourceIndexPhysicalForTests(
				Value, Payload);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Sentinel;
			const FAngelscriptCacheValidationResult Result = Decode(
				EAngelscriptCacheRecordKind::SourceIndex, Payload, Budget, Output);
			ASSERT_THAT(AreEqual(Error, Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::SourceIndex,
				Result.RecordKind));
			ASSERT_THAT(AreEqual(ExpectedOffset, Result.ByteOffset));
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		};
		const auto ExpectModuleFailure = [this](
			const FAngelscriptCachedModuleInterface& Value,
			const EAngelscriptCacheValidationError Error,
			const uint64 ExpectedOffset,
			const FAngelscriptDecodedCacheRecordHandle& Sentinel)
		{
			TArray<uint8> Payload;
			FAngelscriptCacheSemanticArchive::SerializeModuleInterfacePhysicalForTests(
				Value, Payload);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Sentinel;
			const FAngelscriptCacheValidationResult Result = Decode(
				EAngelscriptCacheRecordKind::ModuleInterface,
				Payload, Budget, Output);
			ASSERT_THAT(AreEqual(Error, Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::ModuleInterface,
				Result.RecordKind));
			ASSERT_THAT(AreEqual(ExpectedOffset, Result.ByteOffset));
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		};

		// SourceIndex: one top-level key array and both option coordinate shapes.
		{
			FAngelscriptCachedSourceIndex Layout = MakeSourceIndexWithOneProvider();
			Layout.DiscoveryPolicy.Options = {
				{TEXT("Order.A"), MakeHash(0x01)},
				{TEXT("Order.B"), MakeHash(0x02)}};
			check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
				Layout, Layout.SourceSnapshot).IsSuccess());
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalSource(Layout);
			const auto* CanonicalValue = Canonical->TryGetSourceIndex();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOption, 1}).GetValue();

			FAngelscriptCachedSourceIndex Duplicate = *CanonicalValue;
			Duplicate.DiscoveryPolicy.Options[1] =
				Duplicate.DiscoveryPolicy.Options[0];
			ExpectSourceFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedSourceIndex Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.DiscoveryPolicy.Options);
			ExpectSourceFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedSourceIndex Layout = MakeSourceIndexWithOneProvider();
			Layout.Mounts[0].Options = {
				{TEXT("Order.A"), MakeHash(0x11)},
				{TEXT("Order.B"), MakeHash(0x12)}};
			check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
				Layout, Layout.SourceSnapshot).IsSuccess());
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalSource(Layout);
			const auto* CanonicalValue = Canonical->TryGetSourceIndex();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptSourceIndexCapturedField::MountOption, 0, 1}).GetValue();

			FAngelscriptCachedSourceIndex Duplicate = *CanonicalValue;
			Duplicate.Mounts[0].Options[1] = Duplicate.Mounts[0].Options[0];
			ExpectSourceFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedSourceIndex Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.Mounts[0].Options);
			ExpectSourceFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedSourceIndex Layout = MakeSourceIndexWithOneProvider();
			FAngelscriptCachedSourceProvider Second = Layout.Providers[0];
			Second.CanonicalImplementationIdentity =
				TEXT("Runtime.TestSourceProvider");
			Second.IdentityFingerprint = MakeHash(0x14);
			check(FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
				{Second.ProviderKind, Second.CanonicalImplementationIdentity,
					Second.IdentityFingerprint}, Second.ProviderKey).IsSuccess());
			Layout.Providers.Add(Second);
			check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
				Layout, Layout.SourceSnapshot).IsSuccess());
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalSource(Layout);
			const auto* CanonicalValue = Canonical->TryGetSourceIndex();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptSourceIndexCapturedField::Provider, 1}).GetValue();

			FAngelscriptCachedSourceIndex Duplicate = *CanonicalValue;
			Duplicate.Providers[1] = Duplicate.Providers[0];
			ExpectSourceFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedSourceIndex Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.Providers);
			ExpectSourceFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedSourceIndex Layout = MakeSourceIndexWithOneProvider();
			FAngelscriptCachedFastPathIneligibleScope Second =
				Layout.IneligibleScopes[0];
			Second.Reason =
				EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource;
			Layout.IneligibleScopes.Add(Second);
			check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
				Layout, Layout.SourceSnapshot).IsSuccess());
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalSource(Layout);
			const auto* CanonicalValue = Canonical->TryGetSourceIndex();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptSourceIndexCapturedField::IneligibleScope, 1}).GetValue();

			FAngelscriptCachedSourceIndex Duplicate = *CanonicalValue;
			Duplicate.IneligibleScopes[1] = Duplicate.IneligibleScopes[0];
			ExpectSourceFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedSourceIndex Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.IneligibleScopes);
			ExpectSourceFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}

		// ModuleInterface: top-level, declaration-nested and stable-key arrays.
		{
			FAngelscriptCachedModuleInterface Layout = MakeInterfaceWithOneParameter();
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalModule(Layout);
			const auto* CanonicalValue = Canonical->TryGetModuleInterface();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptModuleInterfaceCapturedField::CanonicalNamespace, 1}).GetValue();

			FAngelscriptCachedModuleInterface Duplicate = *CanonicalValue;
			Duplicate.CanonicalNamespaces[1] = Duplicate.CanonicalNamespaces[0];
			ExpectModuleFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedModuleInterface Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.CanonicalNamespaces);
			const uint64 ReorderedExpectedOffset = ExpectedOffset
				+ static_cast<uint64>(
					Reordered.CanonicalNamespaces[0].Len()
					- CanonicalValue->CanonicalNamespaces[0].Len());
			ExpectModuleFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ReorderedExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedModuleInterface Layout = MakeInterfaceWithOneParameter();
			Layout.Declarations[0].CanonicalIdentityTraits = {
				TEXT("Alpha"), TEXT("Bravo")};
			ASSERT_THAT(IsTrue(RefreshDeclarationIdentityAndHashes(
				Layout.Declarations[0]).IsSuccess()));
			ASSERT_THAT(IsTrue(RefreshInterfaceAbi(Layout).IsSuccess()));
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalModule(Layout);
			const auto* CanonicalValue = Canonical->TryGetModuleInterface();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalIdentityTrait,
				0, 1}).GetValue();

			FAngelscriptCachedModuleInterface Duplicate = *CanonicalValue;
			Duplicate.Declarations[0].CanonicalIdentityTraits[1] =
				Duplicate.Declarations[0].CanonicalIdentityTraits[0];
			ExpectModuleFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedModuleInterface Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.Declarations[0].CanonicalIdentityTraits);
			ExpectModuleFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedModuleInterface Layout = MakeInterfaceWithOneParameter();
			Layout.Declarations[0].Metadata = {
				{TEXT("First"), TEXT("ValueA")},
				{TEXT("Later"), TEXT("ValueB")}};
			ASSERT_THAT(IsTrue(RefreshDeclarationIdentityAndHashes(
				Layout.Declarations[0]).IsSuccess()));
			ASSERT_THAT(IsTrue(RefreshInterfaceAbi(Layout).IsSuccess()));
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalModule(Layout);
			const auto* CanonicalValue = Canonical->TryGetModuleInterface();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptModuleInterfaceCapturedField::DeclarationMetadataEntry,
				0, 1}).GetValue();

			FAngelscriptCachedModuleInterface Duplicate = *CanonicalValue;
			Duplicate.Declarations[0].Metadata[1] =
				Duplicate.Declarations[0].Metadata[0];
			ExpectModuleFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedModuleInterface Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.Declarations[0].Metadata);
			ExpectModuleFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedModuleInterface Layout = MakeInterfaceWithOneParameter();
			Layout.Declarations[0].Slots = {
				{EAngelscriptCacheDeclarationSlotKind::Function, 0},
				{EAngelscriptCacheDeclarationSlotKind::Function, 1}};
			ASSERT_THAT(IsTrue(RefreshDeclarationIdentityAndHashes(
				Layout.Declarations[0]).IsSuccess()));
			ASSERT_THAT(IsTrue(RefreshInterfaceAbi(Layout).IsSuccess()));
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalModule(Layout);
			const auto* CanonicalValue = Canonical->TryGetModuleInterface();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptModuleInterfaceCapturedField::DeclarationSlot, 0, 1}).GetValue();

			FAngelscriptCachedModuleInterface Duplicate = *CanonicalValue;
			Duplicate.Declarations[0].Slots[1] = Duplicate.Declarations[0].Slots[0];
			ExpectModuleFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateOrdinal,
				ExpectedOffset, Canonical);

			FAngelscriptCachedModuleInterface Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.Declarations[0].Slots);
			ExpectModuleFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedModuleInterface Layout = MakeInterfaceWithOneParameter();
			FAngelscriptCachedDeclaration Second = Layout.Declarations[0];
			Second.CanonicalName = TEXT("Tock");
			Second.CanonicalDeclaration = TEXT("void Tock(float DeltaSeconds)");
			Second.Slots[0].Ordinal = 1;
			ASSERT_THAT(IsTrue(RefreshDeclarationIdentityAndHashes(Second).IsSuccess()));
			Layout.Declarations.Add(Second);
			ASSERT_THAT(IsTrue(RefreshInterfaceAbi(Layout).IsSuccess()));
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalModule(Layout);
			const auto* CanonicalValue = Canonical->TryGetModuleInterface();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptModuleInterfaceCapturedField::Declaration, 1}).GetValue();

			FAngelscriptCachedModuleInterface Duplicate = *CanonicalValue;
			Duplicate.Declarations[1] = Duplicate.Declarations[0];
			ExpectModuleFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedModuleInterface Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.Declarations);
			ExpectModuleFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedModuleInterface Layout = MakeInterfaceWithOneParameter();
			FAngelscriptCachedImportDeclaration Second = Layout.Imports[0];
			Second.CanonicalName = TEXT("Tag");
			Second.CanonicalSignature =
				TEXT("void Tag(const string&in Message)");
			Second.TargetDeclaration.StableKey = MakeHash(0x76);
			Second.Slots[0].Ordinal = 1;
			check(FAngelscriptCacheSemanticArchive::TryBuildImportKey({
				Layout.ModuleKey,
				Second.CanonicalNamespace,
				Second.CanonicalName,
				Second.CanonicalSignature,
				Second.TargetModuleKey,
				FAngelscriptStableFunctionKey{
					Second.TargetDeclaration.StableKey}}, Second.ImportKey).IsSuccess());
			Layout.Imports.Add(Second);
			ASSERT_THAT(IsTrue(RefreshInterfaceAbi(Layout).IsSuccess()));
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalModule(Layout);
			const auto* CanonicalValue = Canonical->TryGetModuleInterface();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptModuleInterfaceCapturedField::Import, 1}).GetValue();

			FAngelscriptCachedModuleInterface Duplicate = *CanonicalValue;
			Duplicate.Imports[1] = Duplicate.Imports[0];
			ExpectModuleFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedModuleInterface Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.Imports);
			ExpectModuleFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
		{
			FAngelscriptCachedModuleInterface Layout = MakeInterfaceWithOneParameter();
			FAngelscriptCacheSemanticDependency Second = Layout.Dependencies[0];
			Second.Target.StableKey = MakeHash(0xf3);
			Second.ExpectedContentOrValue = MakeHash(0xf4);
			Layout.Dependencies.Add(Second);
			ASSERT_THAT(IsTrue(RefreshInterfaceAbi(Layout).IsSuccess()));
			const FAngelscriptDecodedCacheRecordHandle Canonical =
				DecodeCanonicalModule(Layout);
			const auto* CanonicalValue = Canonical->TryGetModuleInterface();
			check(CanonicalValue != nullptr);
			const uint64 ExpectedOffset = Canonical->FindCapturedOffset({
				EAngelscriptModuleInterfaceCapturedField::Dependency, 1}).GetValue();

			FAngelscriptCachedModuleInterface Duplicate = *CanonicalValue;
			Duplicate.Dependencies[1] = Duplicate.Dependencies[0];
			ExpectModuleFailure(Duplicate,
				EAngelscriptCacheValidationError::DuplicateKey,
				ExpectedOffset, Canonical);

			FAngelscriptCachedModuleInterface Reordered = *CanonicalValue;
			Algo::Reverse(Reordered.Dependencies);
			ExpectModuleFailure(Reordered,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				ExpectedOffset, Canonical);
		}
	}

	TEST_METHOD(OptionalTagsAndPhysicalFailuresUseExactWirePrecedence)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		TArray<uint8> SourcePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			MakeSourceIndexWithOneProvider(), SourcePayload).IsSuccess());
		FAngelscriptCacheReadBudget SourceGoodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Source;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::SourceIndex,
			SourcePayload, SourceGoodBudget, Source).IsSuccess()));

		const FAngelscriptSourceIndexFieldCoordinate SourceOptionalTags[] = {
			{EAngelscriptSourceIndexCapturedField::ProviderVersionFingerprintPresence, 0},
			{EAngelscriptSourceIndexCapturedField::ProviderConfigurationFingerprintPresence, 0},
			{EAngelscriptSourceIndexCapturedField::ProviderContentFingerprintPresence, 0},
			{EAngelscriptSourceIndexCapturedField::HookVersionFingerprintPresence, 0},
			{EAngelscriptSourceIndexCapturedField::HookConfigurationFingerprintPresence, 0},
			{EAngelscriptSourceIndexCapturedField::HookContentFingerprintPresence, 0},
			{EAngelscriptSourceIndexCapturedField::FileGeneratedSourceKeyPresence, 0},
			{EAngelscriptSourceIndexCapturedField::FileGeneratedConfigurationFingerprintPresence, 0},
			{EAngelscriptSourceIndexCapturedField::InputTargetStableKeyPresence, 0},
			{EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinalPresence, 0},
			{EAngelscriptSourceIndexCapturedField::IneligibleScopeObservedFingerprintPresence, 0},
		};
		for (const FAngelscriptSourceIndexFieldCoordinate& Coordinate :
			SourceOptionalTags)
		{
			const uint64 ExactOffset =
				Source.GetValue()->FindCapturedOffset(Coordinate).GetValue();
			TArray<uint8> Mutated = SourcePayload;
			Mutated[ExactOffset] = 2;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Source;
			const FAngelscriptCacheValidationResult Result = Decode(
				EAngelscriptCacheRecordKind::SourceIndex, Mutated, Budget, Output);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::InvalidOptionalTag, Result.Error));
			ASSERT_THAT(AreEqual(ExactOffset, Result.ByteOffset));
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		}

		// The first mutation is semantic and occurs earlier in the payload. The later
		// malformed optional tag must still win because the entire physical record is
		// validated before any semantic replay is allowed to publish.
		const uint64 SourceSemanticOffset = Source.GetValue()->FindCapturedOffset({
			EAngelscriptSourceIndexCapturedField::ProviderCapabilityFlags, 0}).GetValue();
		const uint64 SourcePhysicalOffset = Source.GetValue()->FindCapturedOffset({
			EAngelscriptSourceIndexCapturedField::FileGeneratedSourceKeyPresence, 0}).GetValue();
		ASSERT_THAT(IsTrue(SourceSemanticOffset < SourcePhysicalOffset));
		TArray<uint8> SourceCompeting = SourcePayload;
		const uint32 CapabilitiesWithoutVersion = ReadUInt32(
			SourceCompeting, SourceSemanticOffset)
			& ~static_cast<uint32>(
				EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint);
		WriteUInt32(SourceCompeting, SourceSemanticOffset, CapabilitiesWithoutVersion);
		SourceCompeting[SourcePhysicalOffset] = 2;
		FAngelscriptCacheReadBudget SourceCompetingBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> SourceCompetingOutput = Source;
		FAngelscriptCacheValidationResult Result = Decode(
			EAngelscriptCacheRecordKind::SourceIndex,
			SourceCompeting,
			SourceCompetingBudget,
			SourceCompetingOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidOptionalTag,
			Result.Error));
		ASSERT_THAT(AreEqual(SourcePhysicalOffset, Result.ByteOffset));
		ASSERT_THAT(IsFalse(SourceCompetingOutput.IsSet()));

		TArray<uint8> ModulePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			MakeInterfaceWithOneParameter(), ModulePayload).IsSuccess());
		FAngelscriptCacheReadBudget ModuleGoodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Module;
		ASSERT_THAT(IsTrue(Decode(EAngelscriptCacheRecordKind::ModuleInterface,
			ModulePayload, ModuleGoodBudget, Module).IsSuccess()));

		const FAngelscriptModuleInterfaceFieldCoordinate ModuleOptionalTags[] = {
			{EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalTypeSpellingPresence, 0},
			{EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypePresence, 0},
			{EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferencePresence, 0, 0},
			{EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferencePresence, 0, 0, 0},
			{EAngelscriptModuleInterfaceCapturedField::DeclarationParameterDefaultExpressionPresence, 0, 0},
			{EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValuePresence, 0},
		};
		for (const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate :
			ModuleOptionalTags)
		{
			const uint64 ExactOffset =
				Module.GetValue()->FindCapturedOffset(Coordinate).GetValue();
			TArray<uint8> Mutated = ModulePayload;
			Mutated[ExactOffset] = 2;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Module;
			Result = Decode(EAngelscriptCacheRecordKind::ModuleInterface,
				Mutated, Budget, Output);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::InvalidOptionalTag, Result.Error));
			ASSERT_THAT(AreEqual(ExactOffset, Result.ByteOffset));
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		}

		const uint64 ModuleSemanticOffset = Module.GetValue()->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::DeclarationReflectionFlags, 0}).GetValue();
		const uint64 ModulePhysicalOffset = Module.GetValue()->FindCapturedOffset({
			EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValuePresence,
			0}).GetValue();
		ASSERT_THAT(IsTrue(ModuleSemanticOffset < ModulePhysicalOffset));
		TArray<uint8> ModuleCompeting = ModulePayload;
		WriteUInt32(ModuleCompeting, ModuleSemanticOffset, 0x80000000u);
		ModuleCompeting[ModulePhysicalOffset] = 2;
		FAngelscriptCacheReadBudget ModuleCompetingBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> ModuleCompetingOutput = Module;
		Result = Decode(EAngelscriptCacheRecordKind::ModuleInterface,
			ModuleCompeting,
			ModuleCompetingBudget,
			ModuleCompetingOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidOptionalTag,
			Result.Error));
		ASSERT_THAT(AreEqual(ModulePhysicalOffset, Result.ByteOffset));
		ASSERT_THAT(IsFalse(ModuleCompetingOutput.IsSet()));
	}

	TEST_METHOD(RetainedAllocationsHaveCapturedOriginsAndAtomicBudgetRollback)
	{
		using namespace AngelscriptCacheSourceInterfaceCapturedOffsetTests_Private;

		const auto VerifyRecord = [this](
			const EAngelscriptCacheRecordKind Kind,
			const TArray<uint8>& Payload)
		{
			FAllocationProbeCapture BaselineCapture;
			FAngelscriptCacheReadBudget BaselineBudget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> BaselineOutput;
			ASSERT_THAT(IsTrue(DecodeWithProbe(
				Kind,
				Payload,
				FAngelscriptCacheReadLimits{},
				BaselineBudget,
				BaselineCapture,
				BaselineOutput).IsSuccess()));
			ASSERT_THAT(IsTrue(BaselineOutput.IsSet()));
			ASSERT_THAT(IsFalse(BaselineCapture.bOverflowed));

			TSet<uint64> CapturedOffsets;
			if (Kind == EAngelscriptCacheRecordKind::SourceIndex)
			{
				for (uint16 Raw = 1; Raw <= 89; ++Raw)
				{
					FAngelscriptSourceIndexFieldCoordinate Coordinate =
						MakeSourceCoordinate(
							static_cast<EAngelscriptSourceIndexCapturedField>(Raw));
					const uint32 PrimaryEnd = Coordinate.PrimaryIndex == MAX_uint32 ? 1 : 8;
					const uint32 SecondaryEnd = Coordinate.SecondaryIndex == MAX_uint32 ? 1 : 8;
					for (uint32 Primary = 0; Primary < PrimaryEnd; ++Primary)
					{
						for (uint32 Secondary = 0; Secondary < SecondaryEnd; ++Secondary)
						{
							Coordinate.PrimaryIndex = PrimaryEnd == 1
								? MAX_uint32 : Primary;
							Coordinate.SecondaryIndex = SecondaryEnd == 1
								? MAX_uint32 : Secondary;
							const TOptional<uint64> Offset = BaselineOutput.GetValue()->
								FindCapturedOffset(Coordinate);
							if (Offset.IsSet())
							{
								CapturedOffsets.Add(Offset.GetValue());
							}
						}
					}
				}
			}
			else
			{
				check(Kind == EAngelscriptCacheRecordKind::ModuleInterface);
				for (uint16 Raw = 1; Raw <= 88; ++Raw)
				{
					FAngelscriptModuleInterfaceFieldCoordinate Coordinate =
						MakeModuleCoordinate(
							static_cast<EAngelscriptModuleInterfaceCapturedField>(Raw));
					const uint32 PrimaryEnd = Coordinate.PrimaryIndex == MAX_uint32 ? 1 : 8;
					const uint32 SecondaryEnd = Coordinate.SecondaryIndex == MAX_uint32 ? 1 : 8;
					const uint32 TertiaryEnd = Coordinate.TertiaryIndex == MAX_uint32 ? 1 : 8;
					for (uint32 Primary = 0; Primary < PrimaryEnd; ++Primary)
					{
						for (uint32 Secondary = 0; Secondary < SecondaryEnd; ++Secondary)
						{
							for (uint32 Tertiary = 0; Tertiary < TertiaryEnd; ++Tertiary)
							{
								Coordinate.PrimaryIndex = PrimaryEnd == 1
									? MAX_uint32 : Primary;
								Coordinate.SecondaryIndex = SecondaryEnd == 1
									? MAX_uint32 : Secondary;
								Coordinate.TertiaryIndex = TertiaryEnd == 1
									? MAX_uint32 : Tertiary;
								const TOptional<uint64> Offset = BaselineOutput.GetValue()->
									FindCapturedOffset(Coordinate);
								if (Offset.IsSet())
								{
									CapturedOffsets.Add(Offset.GetValue());
								}
							}
						}
					}
				}
			}

			TArray<FAngelscriptCacheTypeSchemaProbeEventForTests> Allocations;
			uint64 ExpectedRetainedBytes = 0;
			uint64 ExpectedOrdinal = 0;
			for (const FAngelscriptCacheTypeSchemaProbeEventForTests& Event :
				BaselineCapture.Events())
			{
				if (Event.Kind !=
					EAngelscriptCacheTypeSchemaProbeEventKindForTests::Allocation)
				{
					continue;
				}
				ASSERT_THAT(AreEqual(ExpectedOrdinal++, Event.SequenceOrdinal));
				ASSERT_THAT(IsTrue(Event.RequestedElementCount > 0));
				ASSERT_THAT(IsTrue(Event.ReservedCapacity >=
					static_cast<uint64>(Event.RequestedElementCount)));
				ASSERT_THAT(IsTrue(Event.ElementSize > 0));
				ASSERT_THAT(IsTrue(Event.ElementAlignment > 0));
				ASSERT_THAT(IsTrue(Event.AllocatedBytes > 0));
				ASSERT_THAT(AreEqual(Event.AllocatedBytes, Event.TotalChargeBytes));
				ASSERT_THAT(AreEqual(Event.AllocatedBytes, Event.TemporaryChargeBytes));
				ASSERT_THAT(AreEqual(uint64(0), Event.ResidentChargeBytes));
				if (!CapturedOffsets.Contains(Event.FieldOffset))
				{
					TestRunner->AddInfo(FString::Printf(
						TEXT("Cache V2 allocation without a published coordinate: kind=%u, event=%llu, offset=%llu, requested=%d, bytes=%llu"),
						static_cast<uint8>(Kind), Event.SequenceOrdinal,
						Event.FieldOffset, Event.RequestedElementCount,
						Event.AllocatedBytes));
				}
				ASSERT_THAT(IsTrue(CapturedOffsets.Contains(Event.FieldOffset)));
				ExpectedRetainedBytes += Event.AllocatedBytes;
				Allocations.Add(Event);
			}

			ASSERT_THAT(IsTrue(!Allocations.IsEmpty()));
			ASSERT_THAT(AreEqual(
				static_cast<uint64>(Allocations.Num()),
				BaselineCapture.Probe->GetTotalAllocationAttempts()));
			ASSERT_THAT(AreEqual(ExpectedRetainedBytes,
				BaselineCapture.Probe->GetTotalAllocatedBytes()));
			const uint64 ExpectedTotalDecodedBytes = BaselineBudget.GetDecodedBytes();
			ASSERT_THAT(IsTrue(ExpectedTotalDecodedBytes >= ExpectedRetainedBytes));
			ASSERT_THAT(AreEqual(ExpectedRetainedBytes,
				BaselineBudget.GetResidentDecodedBytes()));
			const uint64 ExpectedPeakResidentBytes =
				BaselineBudget.GetPeakLiveResidentDecodedBytes();
			ASSERT_THAT(IsTrue(ExpectedPeakResidentBytes >= ExpectedRetainedBytes));
			ASSERT_THAT(AreEqual(uint64(0),
				BaselineBudget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(int64(0),
				BaselineCapture.Probe->GetAllocationBalance()));

			TestRunner->AddInfo(FString::Printf(
				TEXT("Cache V2 %s retained allocation inventory: payload=%d, events=%d, bytes=%llu"),
				Kind == EAngelscriptCacheRecordKind::SourceIndex
					? TEXT("SourceIndex") : TEXT("ModuleInterface"),
				Payload.Num(), Allocations.Num(), ExpectedRetainedBytes));

			FAngelscriptCacheReadLimits ExactLimits;
			ExactLimits.MaxTotalDecodedBytes = ExpectedTotalDecodedBytes;
			ExactLimits.MaxResidentDecodedBytes = ExpectedPeakResidentBytes;
			FAngelscriptCacheReadBudget ExactBudget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> ExactOutput;
			ASSERT_THAT(IsTrue(FAngelscriptDecodedCacheRecord::TryDecode(
				BaselineOutput.GetValue()->GetRecordId(), Payload, ExactLimits,
				ExactBudget, ExactOutput).IsSuccess()));
			ASSERT_THAT(IsTrue(ExactOutput.IsSet()));

			for (const bool bShortTotal : {true, false})
			{
				FAngelscriptCacheReadLimits ShortLimits;
				if (bShortTotal)
				{
					ShortLimits.MaxTotalDecodedBytes = ExpectedTotalDecodedBytes - 1;
				}
				else
				{
					ShortLimits.MaxResidentDecodedBytes = ExpectedPeakResidentBytes - 1;
				}
				FAngelscriptCacheReadBudget ShortBudget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> ShortOutput = BaselineOutput;
				const FAngelscriptCacheValidationResult ShortResult =
					FAngelscriptDecodedCacheRecord::TryDecode(
						BaselineOutput.GetValue()->GetRecordId(), Payload, ShortLimits,
						ShortBudget, ShortOutput);
				ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
					ShortResult.Error));
				ASSERT_THAT(IsFalse(ShortOutput.IsSet()));
				ASSERT_THAT(AreEqual(uint64(0), ShortBudget.GetResidentDecodedBytes()));
				ASSERT_THAT(AreEqual(uint64(0),
					ShortBudget.GetTemporaryResidentDecodedBytes()));
			}

			uint64 AcceptedBytes = 0;
			for (int32 AllocationIndex = 0;
				AllocationIndex < Allocations.Num(); ++AllocationIndex)
			{
				AcceptedBytes += Allocations[AllocationIndex].AllocatedBytes;
				FAllocationProbeCapture FailureCapture;
				FailureCapture.Probe->InjectOverflowAfterAcceptedEventForTests(
					AllocationIndex,
					EAngelscriptCacheTypeSchemaInjectedFailureForTests::PhysicalAfterTarget);
				FAngelscriptCacheReadBudget FailureBudget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> FailureOutput =
					BaselineOutput;
				const FAngelscriptCacheValidationResult FailureResult = DecodeWithProbe(
					Kind,
					Payload,
					FAngelscriptCacheReadLimits{},
					FailureBudget,
					FailureCapture,
					FailureOutput);
				ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::Overflow,
					FailureResult.Error));
				ASSERT_THAT(AreEqual(Allocations[AllocationIndex].FieldOffset,
					FailureResult.ByteOffset));
				ASSERT_THAT(IsFalse(FailureOutput.IsSet()));
				ASSERT_THAT(IsFalse(FailureCapture.bOverflowed));
				ASSERT_THAT(AreEqual(static_cast<uint64>(AllocationIndex + 1),
					FailureCapture.Probe->GetTotalAllocationAttempts()));
				ASSERT_THAT(AreEqual(AcceptedBytes,
					FailureCapture.Probe->GetTotalAllocatedBytes()));
				ASSERT_THAT(AreEqual(AcceptedBytes, FailureBudget.GetDecodedBytes()));
				ASSERT_THAT(AreEqual(uint64(0),
					FailureBudget.GetResidentDecodedBytes()));
				ASSERT_THAT(AreEqual(uint64(0),
					FailureBudget.GetTemporaryResidentDecodedBytes()));
				ASSERT_THAT(AreEqual(int64(0),
					FailureCapture.Probe->GetAllocationBalance()));
			}
		};

		TArray<uint8> SourcePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			MakeSourceIndexWithOneProvider(), SourcePayload).IsSuccess());
		VerifyRecord(EAngelscriptCacheRecordKind::SourceIndex, SourcePayload);

		TArray<uint8> ModulePayload;
		check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
			MakeInterfaceWithOneParameter(), ModulePayload).IsSuccess());
		VerifyRecord(EAngelscriptCacheRecordKind::ModuleInterface, ModulePayload);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
