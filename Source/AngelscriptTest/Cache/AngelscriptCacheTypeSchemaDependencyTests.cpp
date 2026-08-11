#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheTypeSchema.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheTypeSchemaDependencyTests,
	"Angelscript.TestModule.Cache.Archive.TypeSchema.Dependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 MakeHash(const uint8 Fill)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Fill, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCacheStableReference MakeReference(
		const EAngelscriptCacheReferenceKind Kind,
		const uint8 KeyFill,
		const uint8 AbiFill)
	{
		return {Kind, MakeHash(KeyFill), MakeHash(AbiFill)};
	}

	static FAngelscriptCacheSemanticDependency MakeDependency(
		const EAngelscriptCacheSemanticDependencyKind Kind,
		const FAngelscriptCacheStableReference& Target)
	{
		FAngelscriptCacheSemanticDependency Dependency;
		Dependency.Kind = Kind;
		Dependency.Target = Target;
		if (Kind == EAngelscriptCacheSemanticDependencyKind::ValueLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::PropertyLayout)
		{
			Dependency.ExpectedContentOrValue = MakeHash(0x7a);
		}
		return Dependency;
	}

	static void FinalizeFixture(FAngelscriptCachedTypeSchema& Schema)
	{
		Schema.Dependencies.Sort([](
			const FAngelscriptCacheSemanticDependency& A,
			const FAngelscriptCacheSemanticDependency& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		for (FAngelscriptCachedTypeLayoutInput& Input : Schema.LayoutInputs)
		{
			Input.LayoutInputHash = {};
			check(FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
				Input, Input.LayoutInputHash).IsSuccess());
		}
		for (FAngelscriptCachedPropertySchema& Property : Schema.OrderedProperties)
		{
			Property.StorageLayoutHash = {};
			check(FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
				Property.Type, Property.StorageKind, Property.SemanticStorageSize,
				Property.SemanticStorageAlignment,
				Property.StorageLayoutHash).IsSuccess());
			Property.PropertyLayoutFingerprint = {};
			check(FAngelscriptCacheTypeSchemaArchive::ComputePropertyLayoutFingerprint(
				Schema.TypeKey, Property,
				Property.PropertyLayoutFingerprint).IsSuccess());
		}
		Schema.Layout.TypeLayoutHash = {};
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
	}

	static FAngelscriptCachedTypeSchema MakeBaseSchema(
		const EAngelscriptCachedTypeKind TypeKind,
		const uint8 IdentityFill)
	{
		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = FAngelscriptStableModuleKey{MakeHash(0x10)};
		Schema.TypeKey = FAngelscriptStableTypeKey{MakeHash(IdentityFill)};
		Schema.TypeKind = TypeKind;
		Schema.CanonicalNamespace = TEXT("CacheDependency");
		Schema.CanonicalName = FString::Printf(TEXT("Type%u"), IdentityFill);
		Schema.CanonicalDeclaration = FString::Printf(
			TEXT("type Type%u"), IdentityFill);
		Schema.Layout.BasePropertyBoundary = 0;
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;

		switch (TypeKind)
		{
		case EAngelscriptCachedTypeKind::Class:
			Schema.TypeSemanticFlags = static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::ReferenceType);
			Schema.Layout.SemanticAlignment = 8;
			break;
		case EAngelscriptCachedTypeKind::Struct:
			Schema.TypeSemanticFlags = static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::Final)
				| static_cast<uint32>(
					EAngelscriptCachedTypeSemanticFlags::ValueType);
			Schema.Layout.SemanticAlignment = 8;
			break;
		case EAngelscriptCachedTypeKind::Interface:
			Schema.TypeSemanticFlags = static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::Abstract)
				| static_cast<uint32>(
					EAngelscriptCachedTypeSemanticFlags::ReferenceType);
			Schema.Layout.SemanticAlignment = 8;
			break;
		case EAngelscriptCachedTypeKind::Funcdef:
			Schema.TypeSemanticFlags = static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::ReferenceType);
			Schema.Layout.SemanticAlignment = 4;
			break;
		default:
			checkNoEntry();
		}
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeDeclarationSchema()
	{
		FAngelscriptCachedTypeSchema Schema = MakeBaseSchema(
			EAngelscriptCachedTypeKind::Class, 0x20);
		FAngelscriptCachedMethodEntry Method;
		Method.EntryKind = EAngelscriptCachedMethodSlotKind::LocalMethod;
		Method.MethodOrdinal = 0;
		Method.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(0x21)};
		Method.DeclaringOwner = Schema.TypeKey;
		Method.ExpectedDeclarationAbi = MakeHash(0x22);
		Schema.OrderedMethods.Add(Method);
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Declaration,
			MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
				0x21, 0x22)));
		FinalizeFixture(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeSignatureSchema()
	{
		FAngelscriptCachedTypeSchema Schema = MakeBaseSchema(
			EAngelscriptCachedTypeKind::Funcdef, 0x30);
		Schema.KindPayload.Callable = FAngelscriptCachedCallableTypePayload{
			FAngelscriptStableFunctionKey{MakeHash(0x31)}, MakeHash(0x32), false};
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Signature,
			MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
				0x31, 0x32)));
		FinalizeFixture(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeInheritanceSchema()
	{
		FAngelscriptCachedTypeSchema Schema = MakeBaseSchema(
			EAngelscriptCachedTypeKind::Interface, 0x40);
		FAngelscriptCachedTypeRelation Interface;
		Interface.RelationKind =
			EAngelscriptCachedTypeRelationKind::ImplementedInterface;
		Interface.SemanticOrdinal = 0;
		Interface.Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, 0x41, 0x42);
		Schema.Relations.Add(Interface);
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Inheritance,
			Interface.Target));
		FinalizeFixture(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeValueAndEnvironmentSchema()
	{
		FAngelscriptCachedTypeSchema Schema = MakeBaseSchema(
			EAngelscriptCachedTypeKind::Struct, 0x50);
		Schema.Layout.SemanticSize = 8;

		FAngelscriptCachedDataType ScriptElement;
		ScriptElement.Kind = EAngelscriptCachedDataTypeKind::ScriptType;
		ScriptElement.TypeReference = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, 0x51, 0x52);
		FAngelscriptCachedDataType EnvironmentContainer;
		EnvironmentContainer.Kind = EAngelscriptCachedDataTypeKind::EnvironmentType;
		EnvironmentContainer.TypeReference = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0x53, 0x54);
		EnvironmentContainer.OrderedSubTypes.Add(ScriptElement);

		FAngelscriptCachedPropertySchema Property;
		Property.LayoutOrdinal = 0;
		Property.SemanticByteOffset = 0;
		Property.PropertyKey = FAngelscriptStablePropertyKey{MakeHash(0x55)};
		Property.CanonicalName = TEXT("Value");
		Property.Type = EnvironmentContainer;
		Property.StorageKind = EAngelscriptCachedPropertyStorageKind::InlineValue;
		Property.SemanticStorageSize = 8;
		Property.SemanticStorageAlignment = 8;
		Property.Access = EAngelscriptCachedMemberAccess::Public;
		Property.ReplicationCondition = EAngelscriptCachedReplicationCondition::None;
		Schema.OrderedProperties.Add(Property);

		FAngelscriptCachedBehaviorSlot EnvironmentCopyConstruct;
		EnvironmentCopyConstruct.BehaviorKind =
			EAngelscriptCachedBehaviorKind::CopyConstruct;
		EnvironmentCopyConstruct.Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0x56, 0x57);
		Schema.OrderedBehaviorSlots.Add(EnvironmentCopyConstruct);

		Schema.Dependencies = {
			MakeDependency(EAngelscriptCacheSemanticDependencyKind::ValueLayout,
				ScriptElement.TypeReference.GetValue()),
			MakeDependency(EAngelscriptCacheSemanticDependencyKind::ValueLayout,
				EnvironmentContainer.TypeReference.GetValue()),
			MakeDependency(EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
				EnvironmentCopyConstruct.Target),
		};
		FinalizeFixture(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeObjectHandlePropertySchema(
		const EAngelscriptCachedDataTypeKind DataTypeKind)
	{
		check(DataTypeKind == EAngelscriptCachedDataTypeKind::ScriptType
			|| DataTypeKind == EAngelscriptCachedDataTypeKind::EnvironmentType);
		FAngelscriptCachedTypeSchema Schema = MakeBaseSchema(
			EAngelscriptCachedTypeKind::Class,
			DataTypeKind == EAngelscriptCachedDataTypeKind::ScriptType
				? 0x70 : 0x71);
		const FAngelscriptCacheV1StorageLayout HandleLayout =
			FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
				.GetObjectHandleStorageLayout();
		Schema.Layout.SemanticSize = HandleLayout.SemanticStorageSize;
		Schema.Layout.SemanticAlignment = HandleLayout.SemanticStorageAlignment;

		FAngelscriptCachedPropertySchema Property;
		Property.LayoutOrdinal = 0;
		Property.SemanticByteOffset = 0;
		Property.PropertyKey = FAngelscriptStablePropertyKey{MakeHash(0x72)};
		Property.CanonicalName = TEXT("Target");
		Property.Type.Kind = DataTypeKind;
		Property.Type.TypeReference = MakeReference(
			DataTypeKind == EAngelscriptCachedDataTypeKind::ScriptType
				? EAngelscriptCacheReferenceKind::ScriptType
				: EAngelscriptCacheReferenceKind::EnvironmentSymbol,
			0x73, 0x74);
		Property.Type.QualifierFlags = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		Property.StorageKind = EAngelscriptCachedPropertyStorageKind::ObjectHandle;
		Property.SemanticStorageSize = HandleLayout.SemanticStorageSize;
		Property.SemanticStorageAlignment = HandleLayout.SemanticStorageAlignment;
		Property.Access = EAngelscriptCachedMemberAccess::Public;
		Property.ReplicationCondition = EAngelscriptCachedReplicationCondition::None;
		Schema.OrderedProperties.Add(Property);
		Schema.Dependencies.Add(MakeDependency(
			DataTypeKind == EAngelscriptCachedDataTypeKind::ScriptType
				? EAngelscriptCacheSemanticDependencyKind::Declaration
				: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
			Property.Type.TypeReference.GetValue()));
		FinalizeFixture(Schema);
		return Schema;
	}

	static bool ExpectProducerSuccess(
		FAutomationTestBase& Test,
		const FAngelscriptCachedTypeSchema& Schema,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		TArray<uint8> Payload;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(Schema, Payload);
		bool bPassed = LocalAssert.IsTrue(Result.IsSuccess(), Context);
		bPassed &= LocalAssert.IsTrue(!Payload.IsEmpty(),
			*FString::Printf(TEXT("%s: payload"), Context));
		return bPassed;
	}

	static bool ExpectProducerFailure(
		FAutomationTestBase& Test,
		const FAngelscriptCachedTypeSchema& Schema,
		const EAngelscriptCacheValidationError ExpectedError,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		TArray<uint8> BeforeBytes;
		FAngelscriptCacheTypeSchemaTestWireTrace BeforeTrace;
		bool bPassed = LocalAssert.IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				Schema, BeforeBytes, BeforeTrace).IsSuccess(),
			*FString::Printf(TEXT("%s: physical before"), Context));

		TArray<uint8> ProducedBytes = {0xaa, 0xbb};
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, ProducedBytes);
		bPassed &= LocalAssert.AreEqual(ExpectedError, Result.Error,
			*FString::Printf(TEXT("%s: error"), Context));
		bPassed &= LocalAssert.AreEqual(0, ProducedBytes.Num(),
			*FString::Printf(TEXT("%s: atomic output"), Context));

		TArray<uint8> AfterBytes;
		FAngelscriptCacheTypeSchemaTestWireTrace AfterTrace;
		bPassed &= LocalAssert.IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				Schema, AfterBytes, AfterTrace).IsSuccess(),
			*FString::Printf(TEXT("%s: physical after"), Context));
		bPassed &= LocalAssert.IsTrue(BeforeBytes == AfterBytes,
			*FString::Printf(TEXT("%s: input unchanged"), Context));
		return bPassed;
	}

public:
	TEST_METHOD(ObjectHandleTargetsUseSymbolAbiRatherThanTargetValueLayout)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		bool bPassed = true;
		for (const EAngelscriptCachedDataTypeKind DataTypeKind : {
			EAngelscriptCachedDataTypeKind::ScriptType,
			EAngelscriptCachedDataTypeKind::EnvironmentType})
		{
			const TCHAR* Context = DataTypeKind
				== EAngelscriptCachedDataTypeKind::ScriptType
				? TEXT("script object handle declaration ABI")
				: TEXT("environment object handle ABI");
			const FAngelscriptCachedTypeSchema Valid =
				MakeObjectHandlePropertySchema(DataTypeKind);
			bPassed &= ExpectProducerSuccess(*TestRunner, Valid, Context);

			FAngelscriptCachedTypeSchema Invalid = Valid;
			Invalid.Dependencies.Reset();
			Invalid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::ValueLayout,
				Invalid.OrderedProperties[0].Type.TypeReference.GetValue()));
			FinalizeFixture(Invalid);
			bPassed &= ExpectProducerFailure(*TestRunner, Invalid,
				EAngelscriptCacheValidationError::MissingCoverage,
				*FString::Printf(TEXT("%s rejects target layout edge"), Context));
		}
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][ObjectHandle] fixed slot layout is profile-owned; "
			"ScriptType uses Declaration and EnvironmentType uses EnvironmentAbi"));
		ASSERT_THAT(IsTrue(bPassed));
	}

	TEST_METHOD(ProducerRequiresTheExactFiveKindDependencyClosure)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][Dependencies][Producer] begin "
			"legal=4 structural=4 missing=6 extra=2 expected-total=16"));

		const TArray<FAngelscriptCachedTypeSchema> Baselines = {
			MakeDeclarationSchema(),
			MakeSignatureSchema(),
			MakeInheritanceSchema(),
			MakeValueAndEnvironmentSchema(),
		};
		const TCHAR* BaselineNames[] = {
			TEXT("Declaration"),
			TEXT("Signature"),
			TEXT("Inheritance"),
			TEXT("ValueLayout and EnvironmentAbi"),
		};

		bool bPassed = true;
		for (int32 Index = 0; Index < Baselines.Num(); ++Index)
		{
			bPassed &= ExpectProducerSuccess(*TestRunner, Baselines[Index],
				BaselineNames[Index]);
		}

		FAngelscriptCachedTypeSchema Invalid = Baselines[0];
		const FAngelscriptCacheSemanticDependency Duplicate = Invalid.Dependencies[0];
		Invalid.Dependencies.Add(Duplicate);
		FinalizeFixture(Invalid);
		bPassed &= ExpectProducerFailure(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate dependency"));

		Invalid = Baselines[0];
		FAngelscriptCacheSemanticDependency Conflict = Invalid.Dependencies[0];
		Conflict.Target.ExpectedAbi = MakeHash(0x61);
		Invalid.Dependencies.Add(Conflict);
		FinalizeFixture(Invalid);
		bPassed &= ExpectProducerFailure(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("conflicting dependency ABI"));

		Invalid = Baselines[0];
		Invalid.Dependencies[0].Kind =
			static_cast<EAngelscriptCacheSemanticDependencyKind>(0);
		FinalizeFixture(Invalid);
		bPassed &= ExpectProducerFailure(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::UnknownEnumValue,
			TEXT("unknown dependency kind"));

		Invalid = Baselines[0];
		Invalid.Dependencies[0].ExpectedContentOrValue = MakeHash(0x62);
		FinalizeFixture(Invalid);
		bPassed &= ExpectProducerFailure(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("derived dependency content"));

		int32 MissingCalls = 0;
		for (int32 BaselineIndex = 0;
			BaselineIndex < Baselines.Num(); ++BaselineIndex)
		{
			for (int32 DependencyIndex = 0;
				DependencyIndex < Baselines[BaselineIndex].Dependencies.Num();
				++DependencyIndex)
			{
				Invalid = Baselines[BaselineIndex];
				Invalid.Dependencies.RemoveAt(DependencyIndex);
				FinalizeFixture(Invalid);
				const FString Context = FString::Printf(
					TEXT("%s missing dependency %d"),
					BaselineNames[BaselineIndex], DependencyIndex);
				bPassed &= ExpectProducerFailure(*TestRunner, Invalid,
					EAngelscriptCacheValidationError::MissingCoverage, *Context);
				++MissingCalls;
			}
		}

		Invalid = Baselines[1];
		Invalid.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::ValueLayout,
			MakeReference(EAngelscriptCacheReferenceKind::ScriptType,
				0x63, 0x64)));
		FinalizeFixture(Invalid);
		bPassed &= ExpectProducerFailure(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::UnexpectedRecord,
			TEXT("allowed kind without DTO source"));

		Invalid = Baselines[0];
		FAngelscriptCacheSemanticDependency Unsupported = MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::HardValue,
			MakeReference(EAngelscriptCacheReferenceKind::ScriptType,
				0x65, 0x66));
		Unsupported.ExpectedContentOrValue = MakeHash(0x67);
		Invalid.Dependencies.Add(Unsupported);
		FinalizeFixture(Invalid);
		bPassed &= ExpectProducerFailure(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::UnexpectedRecord,
			TEXT("dependency kind outside TypeSchema authority"));

		bPassed &= LocalAssert.AreEqual(6, MissingCalls,
			TEXT("exact missing dependency count"));
		TestRunner->AddInfo(FString::Printf(TEXT(
			"[CacheV2][TypeSchema][Dependencies][Producer] complete "
			"legal=4 structural=4 missing=%d extra=2 total=%d"),
			MissingCalls, 4 + 4 + MissingCalls + 2));
		ASSERT_THAT(IsTrue(bPassed));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
