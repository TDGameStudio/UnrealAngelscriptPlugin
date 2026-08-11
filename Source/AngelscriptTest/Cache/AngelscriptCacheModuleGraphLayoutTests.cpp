#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheModuleGraphLayoutTests_Private
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
		Provider.CanonicalImplementationIdentity = TEXT("LayoutFixture.Disk");
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
		File.RelativeLogicalPath = TEXT("LayoutFixture.as");
		File.RawContentHash = MakeHash(0x76);
		File.ModuleKey = ModuleKey;
		File.SourceFileKey = BuildSourceFileKey(File);
		Value.Files.Add(File);

		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Value, Value.SourceSnapshot).IsSuccess());
		return Value;
	}

	static FAngelscriptCachedDeclaration MakeTypeDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FStringView CanonicalName = TEXTVIEW("ULayoutCacheRoot"),
		const FStringView CanonicalDeclaration =
			TEXTVIEW("class ULayoutCacheRoot"),
		const uint32 DeclarationSlotOrdinal = 0,
		const EAngelscriptArtifactEntityKind EntityKind =
			EAngelscriptArtifactEntityKind::Class)
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
		Declaration.CanonicalName = FString(CanonicalName);
		Declaration.CanonicalDeclaration = FString(CanonicalDeclaration);
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration,
			DeclarationSlotOrdinal});

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

	enum class EPropertyFixtureKind : uint8
	{
		None,
		Primitive,
		Environment,
	};

	static FAngelscriptCachedDataType MakePrimitiveIntType()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
		return Type;
	}

	static FAngelscriptCachedDataType MakeEnvironmentValueType()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::EnvironmentType;
		Type.TypeReference = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::EnvironmentSymbol,
			MakeHash(0xa0), MakeHash(0xa1)};
		return Type;
	}

	static FAngelscriptCachedDeclaration MakePropertyDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedDeclaration& Owner,
		const EPropertyFixtureKind Kind,
		const uint32 DeclarationSlotOrdinal = 1)
	{
		check(Kind != EPropertyFixtureKind::None);
		const bool bEnvironment = Kind == EPropertyFixtureKind::Environment;
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Property;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::Property;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
		Declaration.OwnerKey = Owner.StableKey;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = Owner.CanonicalNamespace;
		Declaration.CanonicalName = bEnvironment ? TEXT("Value") : TEXT("Counter");
		Declaration.CanonicalTypeSpelling =
			bEnvironment ? TEXT("FExternalValue") : TEXT("int");
		Declaration.CanonicalDeclaration = FString::Printf(TEXT("%s %s"),
			*Declaration.CanonicalTypeSpelling.GetValue(),
			*Declaration.CanonicalName);
		Declaration.DeclaredType = bEnvironment
			? MakeEnvironmentValueType() : MakePrimitiveIntType();
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration,
			DeclarationSlotOrdinal});

		FAngelscriptPropertyIdentityDescriptor Identity;
		Identity.OwnerTypeKey = FAngelscriptStableTypeKey{Owner.StableKey};
		Identity.Kind = Declaration.EntityKind;
		Identity.Name = Declaration.CanonicalName;
		Identity.CanonicalType = Declaration.CanonicalTypeSpelling.GetValue();
		Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedDataType MakeSelectedScriptValueType(
		const FAngelscriptCachedDeclaration& Target,
		const bool bWrongTargetAbi = false)
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::ScriptType;
		Type.TypeReference = FAngelscriptCacheStableReference{
			EAngelscriptCacheReferenceKind::ScriptType,
			Target.StableKey,
			bWrongTargetAbi ? MakeHash(0xb2) : Target.SignatureHash};
		return Type;
	}

	static FAngelscriptCachedDeclaration MakeScriptValuePropertyDeclaration(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedDeclaration& Owner,
		const FAngelscriptCachedDeclaration& Target,
		const FStringView CanonicalName,
		const uint32 DeclarationSlotOrdinal,
		const bool bWrongTargetAbi = false)
	{
		FAngelscriptCachedDeclaration Declaration;
		Declaration.DeclarationKind = EAngelscriptCacheDeclarationKind::Property;
		Declaration.EntityKind = EAngelscriptArtifactEntityKind::Property;
		Declaration.SchemaCoverage = EAngelscriptCacheSchemaCoverage::Forbidden;
		Declaration.BodyCoverage = EAngelscriptCacheBodyCoverage::Forbidden;
		Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
		Declaration.OwnerKey = Owner.StableKey;
		Declaration.ModuleKey = ModuleKey;
		Declaration.CanonicalNamespace = Owner.CanonicalNamespace;
		Declaration.CanonicalName = FString(CanonicalName);
		Declaration.CanonicalTypeSpelling = Target.CanonicalName;
		Declaration.CanonicalDeclaration = FString::Printf(
			TEXT("%s %s"), *Target.CanonicalName,
			*Declaration.CanonicalName);
		Declaration.DeclaredType = MakeSelectedScriptValueType(
			Target, bWrongTargetAbi);
		Declaration.Slots.Add({
			EAngelscriptCacheDeclarationSlotKind::Declaration,
			DeclarationSlotOrdinal});

		FAngelscriptPropertyIdentityDescriptor Identity;
		Identity.OwnerTypeKey = FAngelscriptStableTypeKey{Owner.StableKey};
		Identity.Kind = Declaration.EntityKind;
		Identity.Name = Declaration.CanonicalName;
		Identity.CanonicalType = Declaration.CanonicalTypeSpelling.GetValue();
		Identity.CanonicalTraits = Declaration.CanonicalIdentityTraits;
		Declaration.StableKey =
			FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Identity).Hash;
		check(FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
			Declaration, Declaration.SignatureHash,
			Declaration.TraitsHash).IsSuccess());
		return Declaration;
	}

	static FAngelscriptCachedTypeSchema MakeRootUClassSchema(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedDeclaration& Declaration,
		const TOptional<FAngelscriptCachedDeclaration>& PropertyDeclaration,
		const bool bWrongPrimitiveLayout)
	{
		const FAngelscriptCacheStableReference CodeRoot{
			EAngelscriptCacheReferenceKind::EnvironmentSymbol,
			MakeHash(0x90), MakeHash(0x91)};

		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = ModuleKey;
		Schema.TypeKey = FAngelscriptStableTypeKey{Declaration.StableKey};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Class;
		Schema.CanonicalNamespace = Declaration.CanonicalNamespace;
		Schema.CanonicalName = Declaration.CanonicalName;
		Schema.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Schema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::ReferenceType);

		FAngelscriptCachedTypeRelation Shadow;
		Shadow.RelationKind = EAngelscriptCachedTypeRelationKind::ShadowSuper;
		Shadow.Target = CodeRoot;
		Schema.Relations.Add(Shadow);
		FAngelscriptCachedTypeRelation Code;
		Code.RelationKind = EAngelscriptCachedTypeRelationKind::CodeSuper;
		Code.Target = CodeRoot;
		Schema.Relations.Add(Code);

		FAngelscriptCachedTypeLayoutInput Input;
		Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
		Input.Target = CodeRoot;
		Input.BoundaryContribution = 0;
		Input.AlignmentContribution = 8;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
			Input, Input.LayoutInputHash).IsSuccess());
		Schema.LayoutInputs.Add(Input);

		Schema.Layout.BasePropertyBoundary = 0;
		Schema.Layout.SemanticSize = 0;
		Schema.Layout.SemanticAlignment = 8;
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UClass;
		Schema.Reflection.ClassReflectionFlags = static_cast<uint32>(
			EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass);
		Schema.Reflection.StaticClassGlobalName =
			TEXT("UASClass_Gameplay_LayoutCacheRoot");

		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
		Dependency.Target = CodeRoot;
		Schema.Dependencies.Add(Dependency);

		if (PropertyDeclaration.IsSet())
		{
			const FAngelscriptCachedDeclaration& PropertyDeclarationValue =
				PropertyDeclaration.GetValue();
			check(PropertyDeclarationValue.DeclaredType.IsSet());
			FAngelscriptCachedPropertySchema Property;
			Property.LayoutOrdinal = 0;
			Property.SemanticByteOffset = 0;
			Property.PropertyKey = FAngelscriptStablePropertyKey{
				PropertyDeclarationValue.StableKey};
			Property.CanonicalName = PropertyDeclarationValue.CanonicalName;
			Property.Type = PropertyDeclarationValue.DeclaredType.GetValue();
			Property.StorageKind =
				EAngelscriptCachedPropertyStorageKind::InlineValue;
			const bool bEnvironment = Property.Type.Kind
				== EAngelscriptCachedDataTypeKind::EnvironmentType;
			Property.SemanticStorageSize = bEnvironment
				? 16u : (bWrongPrimitiveLayout ? 8u : 4u);
			Property.SemanticStorageAlignment = bEnvironment
				? 8u : (bWrongPrimitiveLayout ? 8u : 4u);
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
			Schema.OrderedProperties.Add(Property);
			Schema.Layout.SemanticSize = bEnvironment
				? 16u : (bWrongPrimitiveLayout ? 8u : 8u);

			if (bEnvironment)
			{
				FAngelscriptCacheSemanticDependency ValueLayout;
				ValueLayout.Kind =
					EAngelscriptCacheSemanticDependencyKind::ValueLayout;
				ValueLayout.Target = Property.Type.TypeReference.GetValue();
				ValueLayout.ExpectedContentOrValue = MakeHash(0xa2);
				Schema.Dependencies.Add(ValueLayout);
				Schema.Dependencies.Sort([](
					const FAngelscriptCacheSemanticDependency& Left,
					const FAngelscriptCacheSemanticDependency& Right)
				{
					if (Left.Kind != Right.Kind)
					{
						return static_cast<uint8>(Left.Kind)
							< static_cast<uint8>(Right.Kind);
					}
					return Left.Target.StableKey < Right.Target.StableKey;
				});
			}
		}

		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeSimpleClassSchema(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedDeclaration& Declaration)
	{
		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = ModuleKey;
		Schema.TypeKey = FAngelscriptStableTypeKey{Declaration.StableKey};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Class;
		Schema.CanonicalNamespace = Declaration.CanonicalNamespace;
		Schema.CanonicalName = Declaration.CanonicalName;
		Schema.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Schema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::ReferenceType);
		Schema.Layout.SemanticSize = 0;
		Schema.Layout.SemanticAlignment = 8;
		Schema.Layout.BasePropertyBoundary = 0;
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakePrimitiveValueStructSchema(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedDeclaration& Declaration,
		const FAngelscriptCachedDeclaration& PropertyDeclaration)
	{
		check(PropertyDeclaration.DeclaredType.IsSet());
		check(PropertyDeclaration.DeclaredType->Kind
			== EAngelscriptCachedDataTypeKind::Primitive);
		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = ModuleKey;
		Schema.TypeKey = FAngelscriptStableTypeKey{Declaration.StableKey};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Struct;
		Schema.CanonicalNamespace = Declaration.CanonicalNamespace;
		Schema.CanonicalName = Declaration.CanonicalName;
		Schema.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Schema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::Final)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ValueType);
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;

		FAngelscriptCachedPropertySchema Property;
		Property.LayoutOrdinal = 0;
		Property.SemanticByteOffset = 0;
		Property.PropertyKey = FAngelscriptStablePropertyKey{
			PropertyDeclaration.StableKey};
		Property.CanonicalName = PropertyDeclaration.CanonicalName;
		Property.Type = PropertyDeclaration.DeclaredType.GetValue();
		Property.StorageKind = EAngelscriptCachedPropertyStorageKind::InlineValue;
		Property.SemanticStorageSize = 4;
		Property.SemanticStorageAlignment = 4;
		Property.Access = EAngelscriptCachedMemberAccess::Public;
		Property.ReplicationCondition = EAngelscriptCachedReplicationCondition::None;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
			Property.Type, Property.StorageKind, Property.SemanticStorageSize,
			Property.SemanticStorageAlignment,
			Property.StorageLayoutHash).IsSuccess());
		check(FAngelscriptCacheTypeSchemaArchive::ComputePropertyLayoutFingerprint(
			Schema.TypeKey, Property,
			Property.PropertyLayoutFingerprint).IsSuccess());
		Schema.OrderedProperties.Add(Property);
		Schema.Layout.SemanticSize = 8;
		Schema.Layout.SemanticAlignment = 8;
		Schema.Layout.BasePropertyBoundary = 0;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeInlineScriptValueStructSchema(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedDeclaration& Declaration,
		const FAngelscriptCachedDeclaration& PropertyDeclaration,
		const bool bWrongStorageLayout,
		const FAngelscriptHash256& ExpectedTargetLayout)
	{
		check(PropertyDeclaration.DeclaredType.IsSet());
		check(PropertyDeclaration.DeclaredType->Kind
			== EAngelscriptCachedDataTypeKind::ScriptType);
		check(PropertyDeclaration.DeclaredType->TypeReference.IsSet());
		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = ModuleKey;
		Schema.TypeKey = FAngelscriptStableTypeKey{Declaration.StableKey};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Struct;
		Schema.CanonicalNamespace = Declaration.CanonicalNamespace;
		Schema.CanonicalName = Declaration.CanonicalName;
		Schema.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Schema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::Final)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ValueType);
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;

		FAngelscriptCachedPropertySchema Property;
		Property.LayoutOrdinal = 0;
		Property.SemanticByteOffset = 0;
		Property.PropertyKey = FAngelscriptStablePropertyKey{
			PropertyDeclaration.StableKey};
		Property.CanonicalName = PropertyDeclaration.CanonicalName;
		Property.Type = PropertyDeclaration.DeclaredType.GetValue();
		Property.StorageKind = EAngelscriptCachedPropertyStorageKind::InlineValue;
		Property.SemanticStorageSize = bWrongStorageLayout ? 16u : 8u;
		Property.SemanticStorageAlignment = 8;
		Property.Access = EAngelscriptCachedMemberAccess::Public;
		Property.ReplicationCondition = EAngelscriptCachedReplicationCondition::None;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
			Property.Type, Property.StorageKind, Property.SemanticStorageSize,
			Property.SemanticStorageAlignment,
			Property.StorageLayoutHash).IsSuccess());
		check(FAngelscriptCacheTypeSchemaArchive::ComputePropertyLayoutFingerprint(
			Schema.TypeKey, Property,
			Property.PropertyLayoutFingerprint).IsSuccess());
		Schema.OrderedProperties.Add(Property);
		Schema.Layout.SemanticSize = bWrongStorageLayout ? 16u : 8u;
		Schema.Layout.SemanticAlignment = 8;
		Schema.Layout.BasePropertyBoundary = 0;

		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::ValueLayout;
		Dependency.Target = Property.Type.TypeReference.GetValue();
		Dependency.ExpectedContentOrValue = ExpectedTargetLayout;
		Schema.Dependencies.Add(Dependency);
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeDerivedUClassSchema(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedDeclaration& Declaration,
		const FAngelscriptCachedDeclaration& BaseDeclaration,
		const bool bWrongBaseAbi,
		const bool bWrongBaseLayout)
	{
		const FAngelscriptCacheStableReference BaseReference{
			EAngelscriptCacheReferenceKind::ScriptType,
			BaseDeclaration.StableKey,
			bWrongBaseAbi ? MakeHash(0xb0) : BaseDeclaration.SignatureHash};
		const FAngelscriptCacheStableReference CodeRoot{
			EAngelscriptCacheReferenceKind::EnvironmentSymbol,
			MakeHash(0x90), MakeHash(0x91)};

		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = ModuleKey;
		Schema.TypeKey = FAngelscriptStableTypeKey{Declaration.StableKey};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Class;
		Schema.CanonicalNamespace = Declaration.CanonicalNamespace;
		Schema.CanonicalName = Declaration.CanonicalName;
		Schema.CanonicalDeclaration = Declaration.CanonicalDeclaration;
		Schema.TypeSemanticFlags = static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::ReferenceType);

		FAngelscriptCachedTypeRelation Base;
		Base.RelationKind = EAngelscriptCachedTypeRelationKind::Base;
		Base.Target = BaseReference;
		Schema.Relations.Add(Base);
		FAngelscriptCachedTypeRelation Shadow;
		Shadow.RelationKind = EAngelscriptCachedTypeRelationKind::ShadowSuper;
		Shadow.Target = CodeRoot;
		Schema.Relations.Add(Shadow);
		FAngelscriptCachedTypeRelation Code;
		Code.RelationKind = EAngelscriptCachedTypeRelationKind::CodeSuper;
		Code.Target = CodeRoot;
		Schema.Relations.Add(Code);

		FAngelscriptCachedTypeLayoutInput BaseInput;
		BaseInput.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
		BaseInput.Target = BaseReference;
		BaseInput.BoundaryContribution = bWrongBaseLayout ? 8u : 0u;
		BaseInput.AlignmentContribution = 8;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
			BaseInput, BaseInput.LayoutInputHash).IsSuccess());
		Schema.LayoutInputs.Add(BaseInput);
		FAngelscriptCachedTypeLayoutInput CodeInput;
		CodeInput.InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
		CodeInput.Target = CodeRoot;
		CodeInput.AlignmentContribution = 8;
		check(FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
			CodeInput, CodeInput.LayoutInputHash).IsSuccess());
		Schema.LayoutInputs.Add(CodeInput);

		Schema.Layout.BasePropertyBoundary = bWrongBaseLayout ? 8u : 0u;
		Schema.Layout.SemanticSize = bWrongBaseLayout ? 8u : 0u;
		Schema.Layout.SemanticAlignment = 8;
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UClass;
		Schema.Reflection.ClassReflectionFlags = 0;
		Schema.Reflection.StaticClassGlobalName =
			TEXT("UASClass_Gameplay_DerivedLayoutClass");

		FAngelscriptCacheSemanticDependency Inheritance;
		Inheritance.Kind = EAngelscriptCacheSemanticDependencyKind::Inheritance;
		Inheritance.Target = BaseReference;
		Schema.Dependencies.Add(Inheritance);
		FAngelscriptCacheSemanticDependency Environment;
		Environment.Kind = EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
		Environment.Target = CodeRoot;
		Schema.Dependencies.Add(Environment);
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
		return Schema;
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
		FAngelscriptCachedDeclaration Declaration;
		TOptional<FAngelscriptCachedDeclaration> PropertyDeclaration;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Source;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Interface;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Type;
		TOptional<FAngelscriptDecodedCacheRecordHandle> State;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Snapshot;

		explicit FFixture(
			const EPropertyFixtureKind PropertyKind = EPropertyFixtureKind::None,
			const bool bWrongPrimitiveLayout = false)
			: Declaration(MakeTypeDeclaration(ModuleKey))
		{
			check(!bWrongPrimitiveLayout
				|| PropertyKind == EPropertyFixtureKind::Primitive);
			if (PropertyKind != EPropertyFixtureKind::None)
			{
				PropertyDeclaration = MakePropertyDeclaration(
					ModuleKey, Declaration, PropertyKind);
			}
			TArray<uint8> Payload;
			check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				MakeSourceIndex(ModuleKey), Payload).IsSuccess());
			Source = Decode(EAngelscriptCacheRecordKind::SourceIndex,
				Payload, Budget);

			FAngelscriptCachedModuleInterface InterfaceValue;
			InterfaceValue.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			InterfaceValue.ModuleKey = ModuleKey;
			InterfaceValue.CanonicalModuleName = TEXT("LayoutFixture");
			InterfaceValue.CanonicalNamespaces.Add(TEXT("Gameplay"));
			InterfaceValue.Declarations.Add(Declaration);
			if (PropertyDeclaration.IsSet())
			{
				InterfaceValue.Declarations.Add(
					PropertyDeclaration.GetValue());
			}
			check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
				InterfaceValue, InterfaceValue.InterfaceAbi).IsSuccess());
			Payload.Reset();
			check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				InterfaceValue, Payload).IsSuccess());
			Interface = Decode(EAngelscriptCacheRecordKind::ModuleInterface,
				Payload, Budget);

			Payload.Reset();
			check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				MakeRootUClassSchema(ModuleKey, Declaration,
					PropertyDeclaration, bWrongPrimitiveLayout),
				Payload).IsSuccess());
			Type = Decode(EAngelscriptCacheRecordKind::TypeSchema,
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
			SnapshotValue.TypeSchemas.Add({
				FAngelscriptStableTypeKey{Declaration.StableKey},
				Type.GetValue()->GetRecordId()});
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
				SnapshotValue, Payload).IsSuccess());
			Snapshot = Decode(EAngelscriptCacheRecordKind::ModuleSnapshot,
				Payload, Budget);
		}

		TArray<FAngelscriptDecodedCacheRecordHandle> MakePool() const
		{
			return {Type.GetValue(), State.GetValue(), Snapshot.GetValue(),
				Interface.GetValue()};
		}
	};

	struct FLocalBaseFixture
	{
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptStableModuleKey ModuleKey{MakeHash(0x10)};
		FAngelscriptArtifactProfileKey Profile{MakeHash(0x30)};
		FAngelscriptCachedDeclaration BaseDeclaration;
		FAngelscriptCachedDeclaration DerivedDeclaration;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Source;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Interface;
		TOptional<FAngelscriptDecodedCacheRecordHandle> BaseType;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DerivedType;
		TOptional<FAngelscriptDecodedCacheRecordHandle> State;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Snapshot;

		explicit FLocalBaseFixture(
			const bool bWrongBaseAbi = false,
			const bool bWrongBaseLayout = false,
			const bool bBaseCycle = false)
			: BaseDeclaration(MakeTypeDeclaration(
				ModuleKey, TEXTVIEW("UBaseLayoutClass"),
				TEXTVIEW("class UBaseLayoutClass"), 0))
			, DerivedDeclaration(MakeTypeDeclaration(
				ModuleKey, TEXTVIEW("UDerivedLayoutClass"),
				TEXTVIEW("class UDerivedLayoutClass"), 1))
		{
			TArray<uint8> Payload;
			check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				MakeSourceIndex(ModuleKey), Payload).IsSuccess());
			Source = Decode(EAngelscriptCacheRecordKind::SourceIndex,
				Payload, Budget);

			FAngelscriptCachedModuleInterface InterfaceValue;
			InterfaceValue.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			InterfaceValue.ModuleKey = ModuleKey;
			InterfaceValue.CanonicalModuleName = TEXT("LayoutFixture");
			InterfaceValue.CanonicalNamespaces.Add(TEXT("Gameplay"));
			InterfaceValue.Declarations.Add(BaseDeclaration);
			InterfaceValue.Declarations.Add(DerivedDeclaration);
			check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
				InterfaceValue, InterfaceValue.InterfaceAbi).IsSuccess());
			Payload.Reset();
			check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				InterfaceValue, Payload).IsSuccess());
			Interface = Decode(EAngelscriptCacheRecordKind::ModuleInterface,
				Payload, Budget);

			Payload.Reset();
			check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				bBaseCycle
					? MakeDerivedUClassSchema(ModuleKey, BaseDeclaration,
						DerivedDeclaration, false, false)
					: MakeSimpleClassSchema(ModuleKey, BaseDeclaration),
				Payload).IsSuccess());
			BaseType = Decode(EAngelscriptCacheRecordKind::TypeSchema,
				Payload, Budget);

			Payload.Reset();
			check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				MakeDerivedUClassSchema(ModuleKey, DerivedDeclaration,
					BaseDeclaration, bWrongBaseAbi, bWrongBaseLayout),
				Payload).IsSuccess());
			DerivedType = Decode(EAngelscriptCacheRecordKind::TypeSchema,
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
			SnapshotValue.TypeSchemas.Add({
				FAngelscriptStableTypeKey{BaseDeclaration.StableKey},
				BaseType.GetValue()->GetRecordId()});
			SnapshotValue.TypeSchemas.Add({
				FAngelscriptStableTypeKey{DerivedDeclaration.StableKey},
				DerivedType.GetValue()->GetRecordId()});
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
				SnapshotValue, Payload).IsSuccess());
			Snapshot = Decode(EAngelscriptCacheRecordKind::ModuleSnapshot,
				Payload, Budget);
		}

		TArray<FAngelscriptDecodedCacheRecordHandle> MakePool() const
		{
			return {BaseType.GetValue(), DerivedType.GetValue(), State.GetValue(),
				Snapshot.GetValue(), Interface.GetValue()};
		}
	};

	struct FLocalValueFixture
	{
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptStableModuleKey ModuleKey{MakeHash(0x10)};
		FAngelscriptArtifactProfileKey Profile{MakeHash(0x30)};
		FAngelscriptCachedDeclaration LeafDeclaration;
		FAngelscriptCachedDeclaration ContainerDeclaration;
		FAngelscriptCachedDeclaration LeafPropertyDeclaration;
		FAngelscriptCachedDeclaration ContainerPropertyDeclaration;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Source;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Interface;
		TOptional<FAngelscriptDecodedCacheRecordHandle> LeafType;
		TOptional<FAngelscriptDecodedCacheRecordHandle> ContainerType;
		TOptional<FAngelscriptDecodedCacheRecordHandle> State;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Snapshot;

		explicit FLocalValueFixture(
			const bool bWrongTargetAbi = false,
			const bool bWrongStorageLayout = false,
			const bool bValueCycle = false,
			const bool bWrongPropertyLayout = false)
			: LeafDeclaration(MakeTypeDeclaration(
				ModuleKey, TEXTVIEW("FLeafValue"),
				TEXTVIEW("struct FLeafValue"), 0,
				EAngelscriptArtifactEntityKind::Struct))
			, ContainerDeclaration(MakeTypeDeclaration(
				ModuleKey, TEXTVIEW("FContainerValue"),
				TEXTVIEW("struct FContainerValue"), 1,
				EAngelscriptArtifactEntityKind::Struct))
			, LeafPropertyDeclaration(bValueCycle
				? MakeScriptValuePropertyDeclaration(
					ModuleKey, LeafDeclaration, ContainerDeclaration,
					TEXTVIEW("BackReference"), 2)
				: MakePropertyDeclaration(
					ModuleKey, LeafDeclaration,
					EPropertyFixtureKind::Primitive, 2))
			, ContainerPropertyDeclaration(MakeScriptValuePropertyDeclaration(
				ModuleKey, ContainerDeclaration, LeafDeclaration,
				TEXTVIEW("Leaf"), 3, bWrongTargetAbi))
		{
			TArray<uint8> Payload;
			check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
				MakeSourceIndex(ModuleKey), Payload).IsSuccess());
			Source = Decode(EAngelscriptCacheRecordKind::SourceIndex,
				Payload, Budget);

			FAngelscriptCachedTypeSchema LeafSchema = bValueCycle
				? MakeInlineScriptValueStructSchema(
					ModuleKey, LeafDeclaration, LeafPropertyDeclaration,
					false, MakeHash(0xfe))
				: MakePrimitiveValueStructSchema(
					ModuleKey, LeafDeclaration, LeafPropertyDeclaration);
			FAngelscriptCachedTypeSchema ContainerSchema =
				MakeInlineScriptValueStructSchema(
					ModuleKey, ContainerDeclaration,
					ContainerPropertyDeclaration, bWrongStorageLayout,
					LeafSchema.Layout.TypeLayoutHash);
			if (bValueCycle)
			{
				check(LeafSchema.Dependencies.Num() == 1);
				LeafSchema.Dependencies[0].ExpectedContentOrValue =
					ContainerSchema.Layout.TypeLayoutHash;
			}

			FAngelscriptCachedModuleInterface InterfaceValue;
			InterfaceValue.PayloadSchemaVersion =
				FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
			InterfaceValue.ModuleKey = ModuleKey;
			InterfaceValue.CanonicalModuleName = TEXT("LayoutFixture");
			InterfaceValue.CanonicalNamespaces.Add(TEXT("Gameplay"));
			InterfaceValue.Declarations = {
				LeafDeclaration, ContainerDeclaration,
				LeafPropertyDeclaration, ContainerPropertyDeclaration};
			FAngelscriptCacheSemanticDependency PropertyLayout;
			PropertyLayout.Kind =
				EAngelscriptCacheSemanticDependencyKind::PropertyLayout;
			PropertyLayout.Target.Kind =
				EAngelscriptCacheReferenceKind::ScriptProperty;
			PropertyLayout.Target.StableKey = LeafPropertyDeclaration.StableKey;
			PropertyLayout.Target.ExpectedAbi =
				LeafPropertyDeclaration.SignatureHash;
			PropertyLayout.ExpectedContentOrValue = bWrongPropertyLayout
				? MakeHash(0xfd)
				: LeafSchema.OrderedProperties[0].PropertyLayoutFingerprint;
			InterfaceValue.Dependencies.Add(PropertyLayout);
			check(FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
				InterfaceValue, InterfaceValue.InterfaceAbi).IsSuccess());
			Payload.Reset();
			check(FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
				InterfaceValue, Payload).IsSuccess());
			Interface = Decode(EAngelscriptCacheRecordKind::ModuleInterface,
				Payload, Budget);

			Payload.Reset();
			check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				LeafSchema, Payload).IsSuccess());
			LeafType = Decode(EAngelscriptCacheRecordKind::TypeSchema,
				Payload, Budget);

			Payload.Reset();
			check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				ContainerSchema,
				Payload).IsSuccess());
			ContainerType = Decode(EAngelscriptCacheRecordKind::TypeSchema,
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
			SnapshotValue.TypeSchemas = {
				{FAngelscriptStableTypeKey{LeafDeclaration.StableKey},
					LeafType.GetValue()->GetRecordId()},
				{FAngelscriptStableTypeKey{ContainerDeclaration.StableKey},
					ContainerType.GetValue()->GetRecordId()}};
			Payload.Reset();
			check(FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
				SnapshotValue, Payload).IsSuccess());
			Snapshot = Decode(EAngelscriptCacheRecordKind::ModuleSnapshot,
				Payload, Budget);
		}

		TArray<FAngelscriptDecodedCacheRecordHandle> MakePool() const
		{
			return {LeafType.GetValue(), ContainerType.GetValue(), State.GetValue(),
				Snapshot.GetValue(), Interface.GetValue()};
		}
	};

	class FCurrentSymbols final : public IAngelscriptCacheCurrentSymbolResolver
	{
	public:
		mutable uint32 CallCount = 0;

		virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
			const EAngelscriptCacheReferenceKind ReferenceKind,
			const FAngelscriptHash256& StableKey) const override
		{
			++CallCount;
			check(ReferenceKind
				== EAngelscriptCacheReferenceKind::EnvironmentSymbol);
			check(StableKey == MakeHash(0x90)
				|| StableKey == MakeHash(0xa0));
			FAngelscriptCacheCurrentSymbol Symbol;
			Symbol.CurrentAbi = StableKey == MakeHash(0x90)
				? MakeHash(0x91) : MakeHash(0xa1);
			if (StableKey == MakeHash(0xa0))
			{
				Symbol.CurrentContentOrValue = MakeHash(0xa2);
			}
			return Symbol;
		}
	};

	enum class ELayoutResult : uint8
	{
		Valid,
		Missing,
		WrongBoundary,
		WrongAlignment,
		MissingAlignment,
	};

	enum class EDataLayoutResult : uint8
	{
		Valid,
		Missing,
		WrongStorageKind,
		WrongSize,
		WrongAlignment,
	};

	class FCurrentLayouts final : public IAngelscriptCacheCurrentLayoutResolver
	{
	public:
		ELayoutResult Result = ELayoutResult::Valid;
		EDataLayoutResult DataResult = EDataLayoutResult::Valid;
		mutable uint32 DataTypeCallCount = 0;
		mutable uint32 InputCallCount = 0;

		virtual TOptional<FAngelscriptCacheResolvedDataTypeLayout>
		ResolveDataTypeLayout(
			const FAngelscriptCachedDataType& DataType,
			const IAngelscriptCacheProspectiveTypeLayoutView&) const override
		{
			++DataTypeCallCount;
			check(DataType.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType);
			check(DataType.TypeReference.IsSet());
			check(DataType.TypeReference->Kind
				== EAngelscriptCacheReferenceKind::EnvironmentSymbol);
			check(DataType.TypeReference->StableKey == MakeHash(0xa0));
			if (DataResult == EDataLayoutResult::Missing)
			{
				return {};
			}
			FAngelscriptCacheResolvedDataTypeLayout Layout;
			Layout.StorageKind = DataResult
				== EDataLayoutResult::WrongStorageKind
				? EAngelscriptCachedPropertyStorageKind::ObjectHandle
				: EAngelscriptCachedPropertyStorageKind::InlineValue;
			Layout.SemanticStorageSize = DataResult
				== EDataLayoutResult::WrongSize ? 24u : 16u;
			Layout.SemanticStorageAlignment = DataResult
				== EDataLayoutResult::WrongAlignment ? 16u : 8u;
			return Layout;
		}

		virtual TOptional<FAngelscriptCacheResolvedTypeLayoutInput>
		ResolveTypeLayoutInput(
			const EAngelscriptCachedTypeLayoutInputKind InputKind,
			const EAngelscriptCacheReferenceKind ReferenceKind,
			const FAngelscriptHash256& StableKey) const override
		{
			++InputCallCount;
			check(InputKind == EAngelscriptCachedTypeLayoutInputKind::CodeRoot);
			check(ReferenceKind
				== EAngelscriptCacheReferenceKind::EnvironmentSymbol);
			check(StableKey == MakeHash(0x90));
			if (Result == ELayoutResult::Missing)
			{
				return {};
			}

			FAngelscriptCacheResolvedTypeLayoutInput Layout;
			Layout.BoundaryContribution = Result == ELayoutResult::WrongBoundary
				? 16u : 0u;
			if (Result != ELayoutResult::MissingAlignment)
			{
				Layout.AlignmentContribution =
					Result == ELayoutResult::WrongAlignment ? 16u : 8u;
			}
			return Layout;
		}
	};

	class FNoOpaquePayloads final : public IAngelscriptCacheOpaquePayloadValidator
	{
	public:
		mutable uint32 CallCount = 0;

		virtual FAngelscriptCacheValidationResult Validate(
			const FAngelscriptCacheOpaquePayloadValidationRequest&,
			const FAngelscriptCacheReadLimits&,
			FAngelscriptCacheReadBudget&,
			IAngelscriptCacheCandidateChargeSink&,
			FAngelscriptCacheOpaquePayloadSummary&) const override
		{
			++CallCount;
			return FAngelscriptCacheValidationResult::AtStage(
				EAngelscriptCacheValidationError::OpaquePayloadMalformed,
				EAngelscriptCacheRecordKind::ModuleSnapshot,
				EAngelscriptCacheValidationStage::OpaqueCodec, 0);
		}
	};

	struct FContextFixture
	{
		FCurrentSymbols Symbols;
		FCurrentLayouts Layouts;
		FNoOpaquePayloads Opaque;

		template <typename FixtureType>
		FAngelscriptCacheModuleGraphValidationContext Make(
			const FixtureType& Fixture)
		{
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

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheModuleGraphLayoutTests,
	"Angelscript.TestModule.Cache.Archive.ModuleGraphLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CodeRootCurrentLayoutIsRequiredAndMatched)
	{
		using namespace AngelscriptCacheModuleGraphLayoutTests_Private;

		FFixture Fixture;
		FContextFixture ContextFixture;
		FAngelscriptValidatedModuleGraph Graph;
		const FAngelscriptCacheValidationResult Result =
			ValidateModuleSnapshotGraph(
				Fixture.Snapshot.GetValue()->GetRecordId(), Fixture.MakePool(),
				ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
				Fixture.Budget, Graph);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), ContextFixture.Symbols.CallCount));
		ASSERT_THAT(AreEqual(uint32(1), ContextFixture.Layouts.InputCallCount));
		ASSERT_THAT(AreEqual(uint32(0),
			ContextFixture.Layouts.DataTypeCallCount));
		ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Opaque.CallCount));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			Fixture.Budget.GetTemporaryResidentDecodedBytes()));
		UE_LOG(LogTemp, Display, TEXT(
			"Cache V2 CurrentLayouts CodeRoot: symbol=1 input=1 datatype=0 opaque=0; graph published."));
	}

	TEST_METHOD(CodeRootMissingAndMismatchAreTypedAndAtomic)
	{
		using namespace AngelscriptCacheModuleGraphLayoutTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		FFixture Seed;
		FContextFixture SeedContext;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Seed.Snapshot.GetValue()->GetRecordId(), Seed.MakePool(),
			SeedContext.Make(Seed), FAngelscriptCacheReadLimits{},
			Seed.Budget, Graph).IsSuccess()));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));

		const auto RequireFailure = [this, &Graph](
			const ELayoutResult LayoutResult,
			const EAngelscriptCacheValidationError Expected)
		{
			FFixture Fixture;
			FContextFixture ContextFixture;
			ContextFixture.Layouts.Result = LayoutResult;
			const FAngelscriptCacheValidationResult Result =
				ValidateModuleSnapshotGraph(
					Fixture.Snapshot.GetValue()->GetRecordId(), Fixture.MakePool(),
					ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
					Fixture.Budget, Graph);
			ASSERT_THAT(AreEqual(Expected, Result.Error));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationStage::CurrentResolver, Result.Stage));
			ASSERT_THAT(AreEqual(uint32(1), ContextFixture.Symbols.CallCount));
			ASSERT_THAT(AreEqual(uint32(1),
				ContextFixture.Layouts.InputCallCount));
			ASSERT_THAT(AreEqual(uint32(0),
				ContextFixture.Layouts.DataTypeCallCount));
			ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Opaque.CallCount));
			ASSERT_THAT(IsTrue(Graph.IsEmpty()));
			ASSERT_THAT(AreEqual(uint64(0),
				Fixture.Budget.GetTemporaryResidentDecodedBytes()));
			UE_LOG(LogTemp, Display, TEXT(
				"Cache V2 CurrentLayouts rejection: mode=%u error=%u stage=%u symbol=1 input=1 graph-empty=1"),
				static_cast<uint32>(LayoutResult), static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Stage));
		};

		RequireFailure(ELayoutResult::Missing,
			EAngelscriptCacheValidationError::CurrentSymbolMissing);
		RequireFailure(ELayoutResult::WrongBoundary,
			EAngelscriptCacheValidationError::CurrentAbiMismatch);
		RequireFailure(ELayoutResult::WrongAlignment,
			EAngelscriptCacheValidationError::CurrentAbiMismatch);
		RequireFailure(ELayoutResult::MissingAlignment,
			EAngelscriptCacheValidationError::CurrentAbiMismatch);
	}

	TEST_METHOD(PrimitivePropertyUsesBuildConstantsWithoutDataLayoutLookup)
	{
		using namespace AngelscriptCacheModuleGraphLayoutTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		FFixture Valid(EPropertyFixtureKind::Primitive);
		FContextFixture ValidContext;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Valid.Snapshot.GetValue()->GetRecordId(), Valid.MakePool(),
			ValidContext.Make(Valid), FAngelscriptCacheReadLimits{},
			Valid.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), ValidContext.Symbols.CallCount));
		ASSERT_THAT(AreEqual(uint32(1), ValidContext.Layouts.InputCallCount));
		ASSERT_THAT(AreEqual(uint32(0),
			ValidContext.Layouts.DataTypeCallCount));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));

		FFixture Wrong(EPropertyFixtureKind::Primitive, true);
		FContextFixture WrongContext;
		const FAngelscriptCacheValidationResult WrongResult =
			ValidateModuleSnapshotGraph(
				Wrong.Snapshot.GetValue()->GetRecordId(), Wrong.MakePool(),
				WrongContext.Make(Wrong), FAngelscriptCacheReadLimits{},
				Wrong.Budget, Graph);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::CurrentAbiMismatch,
			WrongResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationStage::CurrentResolver,
			WrongResult.Stage));
		ASSERT_THAT(AreEqual(uint32(0),
			WrongContext.Layouts.DataTypeCallCount));
		ASSERT_THAT(IsTrue(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			Wrong.Budget.GetTemporaryResidentDecodedBytes()));
		UE_LOG(LogTemp, Display, TEXT(
			"Cache V2 primitive property: valid int32 used build constants with datatype=0; stored size/alignment mismatch rejected as CurrentAbiMismatch."));
	}

	TEST_METHOD(ExternalPropertyDataLayoutFailuresAreTypedAndAtomic)
	{
		using namespace AngelscriptCacheModuleGraphLayoutTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		FFixture Seed(EPropertyFixtureKind::Environment);
		FContextFixture SeedContext;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Seed.Snapshot.GetValue()->GetRecordId(), Seed.MakePool(),
			SeedContext.Make(Seed), FAngelscriptCacheReadLimits{},
			Seed.Budget, Graph).IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(2), SeedContext.Symbols.CallCount));
		ASSERT_THAT(AreEqual(uint32(1), SeedContext.Layouts.InputCallCount));
		ASSERT_THAT(AreEqual(uint32(1),
			SeedContext.Layouts.DataTypeCallCount));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));

		const auto RequireFailure = [this, &Graph](
			const EDataLayoutResult DataResult,
			const EAngelscriptCacheValidationError Expected)
		{
			FFixture Fixture(EPropertyFixtureKind::Environment);
			FContextFixture ContextFixture;
			ContextFixture.Layouts.DataResult = DataResult;
			const FAngelscriptCacheValidationResult Result =
				ValidateModuleSnapshotGraph(
					Fixture.Snapshot.GetValue()->GetRecordId(), Fixture.MakePool(),
					ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
					Fixture.Budget, Graph);
			ASSERT_THAT(AreEqual(Expected, Result.Error));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationStage::CurrentResolver, Result.Stage));
			ASSERT_THAT(AreEqual(uint32(2), ContextFixture.Symbols.CallCount));
			ASSERT_THAT(AreEqual(uint32(1),
				ContextFixture.Layouts.InputCallCount));
			ASSERT_THAT(AreEqual(uint32(1),
				ContextFixture.Layouts.DataTypeCallCount));
			ASSERT_THAT(IsTrue(Graph.IsEmpty()));
			ASSERT_THAT(AreEqual(uint64(0),
				Fixture.Budget.GetTemporaryResidentDecodedBytes()));
			UE_LOG(LogTemp, Display, TEXT(
				"Cache V2 DataTypeLayout rejection: mode=%u error=%u symbols=2 input=1 datatype=1 graph-empty=1"),
				static_cast<uint32>(DataResult),
				static_cast<uint32>(Result.Error));
		};

		RequireFailure(EDataLayoutResult::Missing,
			EAngelscriptCacheValidationError::CurrentSymbolMissing);
		RequireFailure(EDataLayoutResult::WrongStorageKind,
			EAngelscriptCacheValidationError::CurrentAbiMismatch);
		RequireFailure(EDataLayoutResult::WrongSize,
			EAngelscriptCacheValidationError::CurrentAbiMismatch);
		RequireFailure(EDataLayoutResult::WrongAlignment,
			EAngelscriptCacheValidationError::CurrentAbiMismatch);
	}

	TEST_METHOD(SelectedModuleBaseTypeIsGraphClosedBeforeCurrentEligibility)
	{
		using namespace AngelscriptCacheModuleGraphLayoutTests_Private;

		FLocalBaseFixture Fixture;
		FContextFixture ContextFixture;
		FAngelscriptValidatedModuleGraph Graph;
		const FAngelscriptCacheValidationResult Result =
			ValidateModuleSnapshotGraph(
				Fixture.Snapshot.GetValue()->GetRecordId(), Fixture.MakePool(),
				ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
				Fixture.Budget, Graph);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(1), ContextFixture.Symbols.CallCount));
		ASSERT_THAT(AreEqual(uint32(1), ContextFixture.Layouts.InputCallCount));
		ASSERT_THAT(AreEqual(uint32(0),
			ContextFixture.Layouts.DataTypeCallCount));
		ASSERT_THAT(AreEqual(uint32(2),
			static_cast<uint32>(Graph.GetTypeOrdinals().Num())));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			Fixture.Budget.GetTemporaryResidentDecodedBytes()));
		UE_LOG(LogTemp, Display, TEXT(
			"Cache V2 local BaseType: two selected-module schemas validated with only CodeRoot current calls symbol=1 input=1 datatype=0."));
	}

	TEST_METHOD(SelectedModuleBaseAbiAndLayoutMismatchPrecedeCurrentEligibility)
	{
		using namespace AngelscriptCacheModuleGraphLayoutTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		const auto RequireFailure = [this, &Graph](
			const bool bWrongBaseAbi, const bool bWrongBaseLayout,
			const bool bBaseCycle)
		{
			FLocalBaseFixture Fixture(
				bWrongBaseAbi, bWrongBaseLayout, bBaseCycle);
			FContextFixture ContextFixture;
			FAngelscriptCacheModuleGraphValidationContext Context =
				ContextFixture.Make(Fixture);
			Context.SelectedProfile = FAngelscriptArtifactProfileKey{MakeHash(0xf0)};
			Context.SelectedSourceSnapshot = MakeHash(0xf1);
			const FAngelscriptCacheValidationResult Result =
				ValidateModuleSnapshotGraph(
					Fixture.Snapshot.GetValue()->GetRecordId(), Fixture.MakePool(),
					Context, FAngelscriptCacheReadLimits{}, Fixture.Budget, Graph);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::GraphAbiMismatch, Result.Error));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationStage::ModuleGraph, Result.Stage));
			ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Symbols.CallCount));
			ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Layouts.InputCallCount));
			ASSERT_THAT(AreEqual(uint32(0),
				ContextFixture.Layouts.DataTypeCallCount));
			ASSERT_THAT(IsTrue(Graph.IsEmpty()));
			ASSERT_THAT(AreEqual(uint64(0),
				Fixture.Budget.GetTemporaryResidentDecodedBytes()));
			UE_LOG(LogTemp, Display, TEXT(
				"Cache V2 local BaseType rejection: wrong-abi=%u wrong-layout=%u cycle=%u error=%u stage=%u current-calls=0 graph-empty=1"),
				bWrongBaseAbi ? 1u : 0u, bWrongBaseLayout ? 1u : 0u,
				bBaseCycle ? 1u : 0u,
				static_cast<uint32>(Result.Error), static_cast<uint32>(Result.Stage));
		};

		RequireFailure(true, false, false);
		RequireFailure(false, true, false);
		RequireFailure(false, false, true);
	}

	TEST_METHOD(SelectedModuleInlineValueUsesLinkedTypeLayoutWithoutCurrentCalls)
	{
		using namespace AngelscriptCacheModuleGraphLayoutTests_Private;

		FLocalValueFixture Fixture;
		FContextFixture ContextFixture;
		FAngelscriptValidatedModuleGraph Graph;
		const FAngelscriptCacheValidationResult Result =
			ValidateModuleSnapshotGraph(
				Fixture.Snapshot.GetValue()->GetRecordId(), Fixture.MakePool(),
				ContextFixture.Make(Fixture), FAngelscriptCacheReadLimits{},
				Fixture.Budget, Graph);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Symbols.CallCount));
		ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Layouts.InputCallCount));
		ASSERT_THAT(AreEqual(uint32(0),
			ContextFixture.Layouts.DataTypeCallCount));
		ASSERT_THAT(AreEqual(uint32(2),
			static_cast<uint32>(Graph.GetTypeOrdinals().Num())));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));
		ASSERT_THAT(AreEqual(uint64(0),
			Fixture.Budget.GetTemporaryResidentDecodedBytes()));
		UE_LOG(LogTemp, Display, TEXT(
			"Cache V2 local inline ScriptType: leaf/container layouts validated with symbol=0 input=0 datatype=0 and two published TypeSchemas."));
	}

	TEST_METHOD(SelectedModuleInlineValueAbiLayoutAndCyclePrecedeCurrentEligibility)
	{
		using namespace AngelscriptCacheModuleGraphLayoutTests_Private;

		FAngelscriptValidatedModuleGraph Graph;
		FLocalValueFixture Seed;
		FContextFixture SeedContext;
		ASSERT_THAT(IsTrue(ValidateModuleSnapshotGraph(
			Seed.Snapshot.GetValue()->GetRecordId(), Seed.MakePool(),
			SeedContext.Make(Seed), FAngelscriptCacheReadLimits{},
			Seed.Budget, Graph).IsSuccess()));
		ASSERT_THAT(IsFalse(Graph.IsEmpty()));

		const auto RequireFailure = [this, &Graph](
			const bool bWrongTargetAbi, const bool bWrongStorageLayout,
			const bool bValueCycle, const bool bWrongPropertyLayout)
		{
			FLocalValueFixture Fixture(
				bWrongTargetAbi, bWrongStorageLayout, bValueCycle,
				bWrongPropertyLayout);
			FContextFixture ContextFixture;
			FAngelscriptCacheModuleGraphValidationContext Context =
				ContextFixture.Make(Fixture);
			Context.SelectedProfile = FAngelscriptArtifactProfileKey{MakeHash(0xf2)};
			Context.SelectedSourceSnapshot = MakeHash(0xf3);
			const FAngelscriptCacheValidationResult Result =
				ValidateModuleSnapshotGraph(
					Fixture.Snapshot.GetValue()->GetRecordId(), Fixture.MakePool(),
					Context, FAngelscriptCacheReadLimits{}, Fixture.Budget, Graph);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::GraphAbiMismatch, Result.Error));
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationStage::ModuleGraph, Result.Stage));
			ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Symbols.CallCount));
			ASSERT_THAT(AreEqual(uint32(0), ContextFixture.Layouts.InputCallCount));
			ASSERT_THAT(AreEqual(uint32(0),
				ContextFixture.Layouts.DataTypeCallCount));
			ASSERT_THAT(IsTrue(Graph.IsEmpty()));
			ASSERT_THAT(AreEqual(uint64(0),
				Fixture.Budget.GetTemporaryResidentDecodedBytes()));
			UE_LOG(LogTemp, Display, TEXT(
				"Cache V2 local inline ScriptType/PropertyLayout rejection: wrong-abi=%u wrong-layout=%u cycle=%u wrong-property-layout=%u error=%u stage=%u current-calls=0 graph-empty=1"),
				bWrongTargetAbi ? 1u : 0u,
				bWrongStorageLayout ? 1u : 0u,
				bValueCycle ? 1u : 0u,
				bWrongPropertyLayout ? 1u : 0u,
				static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Stage));
		};

		RequireFailure(true, false, false, false);
		RequireFailure(false, true, false, false);
		RequireFailure(false, false, true, false);
		RequireFailure(false, false, false, true);
	}
};

#endif
