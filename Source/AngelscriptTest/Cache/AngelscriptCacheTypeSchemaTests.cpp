#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"
#include "Cache/AngelscriptCacheTypeSchema.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_config.h"
#include "EndAngelscriptHeaders.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

// CQTest's stock ASSERT_THAT accepts one argument. This declaration-first TU also
// carries source-authority context on selected assertions; keep that information
// and the identical early-return behavior without weakening the matcher.
#pragma push_macro("ASSERT_THAT")
#undef ASSERT_THAT
#define UEAS_TYPESchema_ASSERT_THAT_1(_assertion) \
	do { if (!this->Assert._assertion) { return; } } while (false)
#define UEAS_TYPESchema_ASSERT_THAT_2(_assertion, _context) \
	do { if (!this->Assert._assertion) { TestRunner->AddError(_context); return; } } while (false)
#define UEAS_TYPESchema_SELECT_ASSERT(_1, _2, NAME, ...) NAME
#define ASSERT_THAT(...) \
	UEAS_TYPESchema_SELECT_ASSERT(__VA_ARGS__, \
		UEAS_TYPESchema_ASSERT_THAT_2, UEAS_TYPESchema_ASSERT_THAT_1)(__VA_ARGS__)

static constexpr bool IsExpectedTsScrReferenceCaseForTests(
	const uint8 Family, const uint32 Variant)
{
	switch (Family)
	{
	case 1: return Variant <= 4;
	case 2: return Variant <= 3;
	case 3: return Variant <= 4;
	case 4: return Variant == 0;
	case 5: return Variant <= 5;
	case 6: return Variant == 0;
	case 7: return Variant == 0;
	case 8:
		switch (Variant)
		{
		case 1: case 5: case 9: case 13: case 17: case 21: case 25:
		case 29: case 37: case 41: case 45: case 49: case 53: case 57:
		case 61: case 65: case 10: case 22: case 26: case 30: case 38:
		case 42: case 46: case 50: case 54: case 58: case 62: case 66:
			return true;
		default:
			return false;
		}
	case 9: return Variant <= 3;
	case 10: return Variant <= 8;
	case 11:
		return Variant == 1 || Variant == 2 || Variant == 3
			|| Variant == 4 || Variant == 10;
	case 12: return Variant <= 5;
	case 13: return Variant == 0;
	case 14: return Variant <= 4;
	default: return false;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheTypeSchemaTests,
	"Angelscript.TestModule.Cache.Archive.TypeSchema",
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
		return FAngelscriptCacheStableReference{Kind, MakeHash(KeyFill), MakeHash(AbiFill)};
	}

	static FString Hex(const TConstArrayView<uint8> Bytes)
	{
		return BytesToHexLower(Bytes.GetData(), Bytes.Num());
	}

	static FAngelscriptCachedDataType MakePrimitive(
		const EAngelscriptCachedPrimitiveType Primitive)
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = Primitive;
		return Type;
	}

	static FAngelscriptCachedDataType MakeScriptType(
		const uint8 KeyFill,
		const uint8 AbiFill,
		const uint32 QualifierFlags = 0)
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::ScriptType;
		Type.TypeReference = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, KeyFill, AbiFill);
		Type.QualifierFlags = QualifierFlags;
		return Type;
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

	template <typename LeftType, typename RightType, typename = void>
	struct THasTypedEquality : std::false_type
	{
	};

	template <typename LeftType, typename RightType>
	struct THasTypedEquality<LeftType, RightType,
		std::void_t<decltype(std::declval<const LeftType&>()
			== std::declval<const RightType&>()),
			decltype(std::declval<const LeftType&>()
				!= std::declval<const RightType&>())>> : std::true_type
	{
	};

	template <typename ElementType>
	static int32 CalculateIndependentArrayReserveCapacityForTests(
		const int32 RequestedCapacity)
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

	template <typename ElementType>
	static uint64 CalculateIndependentArrayReserveBytesForTests(
		const int32 RequestedCapacity)
	{
		const int32 Capacity =
			CalculateIndependentArrayReserveCapacityForTests<ElementType>(RequestedCapacity);
		return uint64(Capacity) * sizeof(ElementType);
	}

	static FAngelscriptHash256 MakeLateByteHash(
		const uint8 CommonFill,
		const uint8 LastByte)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, CommonFill, sizeof(Bytes));
		Bytes[sizeof(Bytes) - 1] = LastByte;
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static void FinalizeValidFixtureHashes(FAngelscriptCachedTypeSchema& Schema)
	{
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
				Property.SemanticStorageAlignment, Property.StorageLayoutHash).IsSuccess());
			Property.PropertyLayoutFingerprint = {};
			check(FAngelscriptCacheTypeSchemaArchive::ComputePropertyLayoutFingerprint(
				Schema.TypeKey, Property, Property.PropertyLayoutFingerprint).IsSuccess());
		}

		if (Schema.KindPayload.Enum.IsSet())
		{
			Schema.KindPayload.Enum->EnumAuthorityHash = {};
			check(FAngelscriptCacheTypeSchemaArchive::ComputeEnumAuthorityHash(
				Schema.TypeKey, *Schema.KindPayload.Enum,
				Schema.KindPayload.Enum->EnumAuthorityHash).IsSuccess());
		}

		Schema.Layout.TypeLayoutHash = {};
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
	}

	static FAngelscriptCacheValidationResult
	RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
		FAngelscriptCachedTypeSchema& Schema)
	{
		// This deliberately does not validate ordinal replay. Malformed-ordinal rows
		// need physically self-consistent hashes so the common decoder, rather than
		// a valid-fixture setup check or an earlier hash mismatch, owns the failure.
		for (FAngelscriptCachedTypeLayoutInput& Input : Schema.LayoutInputs)
		{
			Input.LayoutInputHash = {};
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
					Input, Input.LayoutInputHash);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}

		for (FAngelscriptCachedPropertySchema& Property : Schema.OrderedProperties)
		{
			Property.StorageLayoutHash = {};
			FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
					Property.Type, Property.StorageKind, Property.SemanticStorageSize,
					Property.SemanticStorageAlignment, Property.StorageLayoutHash);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			Property.PropertyLayoutFingerprint = {};
			Result = FAngelscriptCacheTypeSchemaArchive::ComputePropertyLayoutFingerprint(
				Schema.TypeKey, Property, Property.PropertyLayoutFingerprint);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}

		if (Schema.KindPayload.Enum.IsSet())
		{
			Schema.KindPayload.Enum->EnumAuthorityHash = {};
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::ComputeEnumAuthorityHash(
					Schema.TypeKey, *Schema.KindPayload.Enum,
					Schema.KindPayload.Enum->EnumAuthorityHash);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}

		Schema.Layout.TypeLayoutHash = {};
		return FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash);
	}

	template <typename CoordinateType>
	static CoordinateType MakeWrongRecordKindPayloadVersionCoordinateForTests()
	{
		CoordinateType Coordinate;
		using FieldType = std::remove_cv_t<decltype(Coordinate.Field)>;
		Coordinate.Field = FieldType::PayloadSchemaVersion;
		return Coordinate;
	}

	static void RehashSelfConsistentWrongPropertyOffset(
		FAngelscriptCachedTypeSchema& Schema,
		const int32 PropertyIndex)
	{
		check(Schema.OrderedProperties.IsValidIndex(PropertyIndex));
		FAngelscriptCachedPropertySchema& Property =
			Schema.OrderedProperties[PropertyIndex];
		Property.PropertyLayoutFingerprint = {};
		check(FAngelscriptCacheTypeSchemaArchive::ComputePropertyLayoutFingerprint(
			Schema.TypeKey, Property, Property.PropertyLayoutFingerprint).IsSuccess());
		Schema.Layout.TypeLayoutHash = {};
		check(FAngelscriptCacheTypeSchemaArchive::ComputeTypeLayoutHash(
			Schema, Schema.Layout.TypeLayoutHash).IsSuccess());
	}

	static FAngelscriptCachedTypeSchema MakeCompleteDelegateSchema()
	{
		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = FAngelscriptStableModuleKey{MakeHash(0x10)};
		Schema.TypeKey = FAngelscriptStableTypeKey{MakeHash(0x11)};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Delegate;
		Schema.CanonicalNamespace = TEXT("Gameplay");
		Schema.CanonicalName = TEXT("FSignal");
		Schema.CanonicalDeclaration = TEXT("delegate void FSignal(int Value)");
		Schema.TypeSemanticFlags =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Final)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Generated)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ValueType);
		Schema.Metadata.Add({TEXT("Category"), TEXT("Events")});

		Schema.Layout.SemanticSize = 8;
		Schema.Layout.SemanticAlignment = 8;
		Schema.Layout.BasePropertyBoundary = 0;

		FAngelscriptCachedPropertySchema Property;
		Property.LayoutOrdinal = 0;
		Property.SemanticByteOffset = 0;
		Property.PropertyKey = FAngelscriptStablePropertyKey{MakeHash(0x20)};
		Property.CanonicalName = TEXT("_Inner");
		Property.Type = MakePrimitive(EAngelscriptCachedPrimitiveType::Int32);
		Property.StorageKind = EAngelscriptCachedPropertyStorageKind::InlineValue;
		Property.SemanticStorageSize = 4;
		Property.SemanticStorageAlignment = 4;
		Property.Access = EAngelscriptCachedMemberAccess::Private;
		Property.PropertySemanticFlags = 0;
		Property.ReplicationCondition = EAngelscriptCachedReplicationCondition::None;
		Property.Metadata.Add({TEXT("Generated"), TEXT("1")});
		Schema.OrderedProperties.Add(MoveTemp(Property));

		FAngelscriptCachedMethodEntry Method;
		Method.EntryKind = EAngelscriptCachedMethodSlotKind::LocalMethod;
		Method.MethodOrdinal = 0;
		Method.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(0x30)};
		Method.DeclaringOwner = Schema.TypeKey;
		Method.ExpectedDeclarationAbi = MakeHash(0x31);
		Schema.OrderedMethods.Add(Method);

		FAngelscriptCachedBehaviorSlot Constructor;
		Constructor.BehaviorKind = EAngelscriptCachedBehaviorKind::Construct;
		Constructor.SlotOrdinal = 0;
		Constructor.Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptFunction, 0x32, 0x33);
		Constructor.DeclaringOwner = Schema.TypeKey;
		Schema.OrderedBehaviorSlots.Add(Constructor);

		FAngelscriptCachedBehaviorSlot Copy;
		Copy.BehaviorKind = EAngelscriptCachedBehaviorKind::Copy;
		Copy.SlotOrdinal = 0;
		Copy.Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptFunction, 0x30, 0x31);
		Copy.DeclaringOwner = Schema.TypeKey;
		Schema.OrderedBehaviorSlots.Add(Copy);

		FAngelscriptCachedCallableTypePayload Callable;
		Callable.SignatureFunctionKey = FAngelscriptStableFunctionKey{MakeHash(0x34)};
		Callable.ExpectedSignatureAbi = MakeHash(0x35);
		Callable.bMulticast = true;
		Schema.KindPayload.Callable = Callable;

		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UDelegate;
		Schema.Dependencies = {
			MakeDependency(EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction, 0x30, 0x31)),
			MakeDependency(EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction, 0x32, 0x33)),
			MakeDependency(EAngelscriptCacheSemanticDependencyKind::Signature,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction, 0x34, 0x35)),
		};

		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeEnumSchema()
	{
		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = FAngelscriptStableModuleKey{MakeHash(0x40)};
		Schema.TypeKey = FAngelscriptStableTypeKey{MakeHash(0x41)};
		Schema.TypeKind = EAngelscriptCachedTypeKind::Enum;
		Schema.CanonicalNamespace = TEXT("Gameplay");
		Schema.CanonicalName = TEXT("EState");
		Schema.CanonicalDeclaration = TEXT("enum EState");
		Schema.TypeSemanticFlags =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Final)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ValueType);
		Schema.Layout.SemanticSize = 1;
		Schema.Layout.SemanticAlignment = 1;
		Schema.Layout.BasePropertyBoundary = 0;
		FAngelscriptCachedEnumTypePayload EnumPayload;
		EnumPayload.OrderedEnumerators = {
			{0, TEXT("Idle"), -1, {{TEXT("DisplayName"), TEXT("Idle")}}},
			{1, TEXT("Waiting"), -1, {{TEXT("DisplayName"), TEXT("Waiting")}}},
			{2, TEXT("Ready"), MAX_int32, {}},
		};
		Schema.KindPayload.Enum = MoveTemp(EnumPayload);
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UEnum;
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeMinimalSchema(
		const EAngelscriptCachedTypeKind TypeKind)
	{
		if (TypeKind == EAngelscriptCachedTypeKind::Delegate)
		{
			return MakeCompleteDelegateSchema();
		}
		if (TypeKind == EAngelscriptCachedTypeKind::Enum)
		{
			return MakeEnumSchema();
		}

		FAngelscriptCachedTypeSchema Schema;
		Schema.PayloadSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		Schema.ModuleKey = FAngelscriptStableModuleKey{MakeHash(0x50)};
		Schema.TypeKey = FAngelscriptStableTypeKey{MakeHash(
			static_cast<uint8>(0x50 + static_cast<uint8>(TypeKind)))};
		Schema.TypeKind = TypeKind;
		Schema.CanonicalNamespace = TEXT("Gameplay");
		Schema.CanonicalName = TEXT("Minimal");
		Schema.CanonicalDeclaration = TEXT("type Minimal");
		Schema.Layout.BasePropertyBoundary = 0;

		switch (TypeKind)
		{
		case EAngelscriptCachedTypeKind::Class:
			Schema.TypeSemanticFlags =
				static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ReferenceType);
			Schema.Layout.SemanticSize = 0;
			Schema.Layout.SemanticAlignment = 8;
			Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;
			break;
		case EAngelscriptCachedTypeKind::Struct:
			Schema.TypeSemanticFlags =
				static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Final)
				| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ValueType);
			Schema.Layout.SemanticSize = 0;
			Schema.Layout.SemanticAlignment = 8;
			Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;
			break;
		case EAngelscriptCachedTypeKind::Interface:
			Schema.TypeSemanticFlags =
				static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Abstract)
				| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ReferenceType);
			Schema.Layout.SemanticSize = 0;
			Schema.Layout.SemanticAlignment = 8;
			Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;
			break;
		case EAngelscriptCachedTypeKind::Typedef:
			Schema.Layout.SemanticSize = 4;
			Schema.Layout.SemanticAlignment = 4;
			Schema.KindPayload.Typedef = FAngelscriptCachedTypedefTypePayload{
				MakePrimitive(EAngelscriptCachedPrimitiveType::Int32)};
			Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;
			break;
		case EAngelscriptCachedTypeKind::Funcdef:
			Schema.TypeSemanticFlags =
				static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ReferenceType);
			Schema.Layout.SemanticSize = 0;
			Schema.Layout.SemanticAlignment = 4;
			Schema.KindPayload.Callable = FAngelscriptCachedCallableTypePayload{
				FAngelscriptStableFunctionKey{MakeHash(0x61)}, MakeHash(0x62), false};
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Signature,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction, 0x61, 0x62)));
			Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::None;
			break;
		default:
			checkNoEntry();
		}

		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedReflectedFunctionMember MakeReflectedMember(
		const uint32 Ordinal,
		const uint8 KeyFill,
		const uint8 AbiFill)
	{
		FAngelscriptCachedReflectedFunctionMember Member;
		Member.ReflectionOrdinal = Ordinal;
		Member.CanonicalFunctionName = FString::Printf(
			TEXT("ReflectedFunction%u"), Ordinal);
		Member.CanonicalOriginalFunctionName = Member.CanonicalFunctionName;
		Member.CanonicalScriptFunctionName = Member.CanonicalFunctionName;
		Member.Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptFunction, KeyFill, AbiFill);
		return Member;
	}

	static FAngelscriptCachedTypeSchema MakeOrdinaryUClassSchema(
		const bool bHasScriptBase)
	{
		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		Schema.CanonicalName = bHasScriptBase
			? TEXT("UDerivedScriptClass") : TEXT("URootScriptClass");
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UClass;
		Schema.Reflection.ClassReflectionFlags = bHasScriptBase ? 0
			: static_cast<uint32>(
				EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass);
		Schema.Reflection.StaticClassGlobalName = TEXT("UASClass_Gameplay_Ordinary");

		if (bHasScriptBase)
		{
			FAngelscriptCachedTypeRelation Base;
			Base.RelationKind = EAngelscriptCachedTypeRelationKind::Base;
			Base.Target = MakeReference(
				EAngelscriptCacheReferenceKind::ScriptType, 0xa2, 0xa3);
			Schema.Relations.Add(Base);
			FAngelscriptCachedTypeLayoutInput BaseInput;
			BaseInput.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
			BaseInput.Target = Base.Target;
			BaseInput.BoundaryContribution = 0;
			BaseInput.AlignmentContribution = 8;
			Schema.LayoutInputs.Add(BaseInput);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Inheritance, Base.Target));
		}

		const FAngelscriptCacheStableReference CodeRoot = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xa4, 0xa5);
		FAngelscriptCachedTypeRelation Shadow;
		Shadow.RelationKind = EAngelscriptCachedTypeRelationKind::ShadowSuper;
		Shadow.Target = CodeRoot;
		Schema.Relations.Add(Shadow);
		FAngelscriptCachedTypeRelation Code;
		Code.RelationKind = EAngelscriptCachedTypeRelationKind::CodeSuper;
		Code.Target = CodeRoot;
		Schema.Relations.Add(Code);
		FAngelscriptCachedTypeLayoutInput CodeInput;
		CodeInput.InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
		CodeInput.Target = CodeRoot;
		if (!bHasScriptBase)
		{
			CodeInput.BoundaryContribution = 0;
		}
		CodeInput.AlignmentContribution = 8;
		Schema.LayoutInputs.Add(CodeInput);
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, CodeRoot));

		Schema.Reflection.OrderedUFunctionMembers.Add(
			MakeReflectedMember(0, 0xa6, 0xa7));
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Declaration,
			Schema.Reflection.OrderedUFunctionMembers[0].Target));
		Schema.Dependencies.Sort([](
			const FAngelscriptCacheSemanticDependency& A,
			const FAngelscriptCacheSemanticDependency& B)
		{
			const uint8 AKind = static_cast<uint8>(A.Kind);
			const uint8 BKind = static_cast<uint8>(B.Kind);
			return AKind != BKind ? AKind < BKind
				: A.Target.StableKey < B.Target.StableKey;
		});
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeNonZeroBaseBoundaryUClassSchema()
	{
		FAngelscriptCachedTypeSchema Schema = MakeOrdinaryUClassSchema(true);
		bool bFoundBaseInput = false;
		for (FAngelscriptCachedTypeLayoutInput& Input : Schema.LayoutInputs)
		{
			if (Input.InputKind == EAngelscriptCachedTypeLayoutInputKind::BaseType)
			{
				Input.BoundaryContribution = 16;
				Input.AlignmentContribution = 8;
				bFoundBaseInput = true;
			}
		}
		check(bFoundBaseInput);
		Schema.Layout.BasePropertyBoundary = 16;
		Schema.Layout.SemanticSize = 16;
		Schema.Layout.SemanticAlignment = 8;
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeStaticsUClassSchema(
		const int32 MemberCount = 2)
	{
		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		Schema.CanonicalName = TEXT("UGeneratedModuleStatics");
		Schema.TypeSemanticFlags =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Generated)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ReferenceType);
		Schema.Layout.SemanticSize = 0;
		Schema.Layout.SemanticAlignment = 1;
		Schema.Layout.BasePropertyBoundary = 0;
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UClass;
		Schema.Reflection.ClassReflectionFlags =
			static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass)
			| static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::StaticsClass);
		FAngelscriptCachedTypeRelation Code;
		Code.RelationKind = EAngelscriptCachedTypeRelationKind::CodeSuper;
		Code.Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xb0, 0xb1);
		Schema.Relations.Add(Code);
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, Code.Target));
		for (int32 Index = 0; Index < MemberCount; ++Index)
		{
			const FAngelscriptCachedReflectedFunctionMember Member = MakeReflectedMember(
				Index, static_cast<uint8>(0xb2 + Index * 2),
				static_cast<uint8>(0xb3 + Index * 2));
			Schema.Reflection.OrderedUFunctionMembers.Add(Member);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration, Member.Target));
		}
		Schema.Dependencies.Sort([](
			const FAngelscriptCacheSemanticDependency& A,
			const FAngelscriptCacheSemanticDependency& B)
		{
			const uint8 AKind = static_cast<uint8>(A.Kind);
			const uint8 BKind = static_cast<uint8>(B.Kind);
			return AKind != BKind ? AKind < BKind
				: A.Target.StableKey < B.Target.StableKey;
		});
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeReflectedUStructSchema()
	{
		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Struct);
		Schema.CanonicalName = TEXT("FReflectedValue");
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UStruct;
		Schema.Reflection.ClassReflectionFlags = static_cast<uint32>(
			EAngelscriptCachedClassReflectionFlags::IsStruct);
		FAngelscriptCachedTypeLayoutInput Header;
		Header.InputKind = EAngelscriptCachedTypeLayoutInputKind::StructHeader;
		Header.Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xc0, 0xc1);
		Header.BoundaryContribution = 16;
		Schema.LayoutInputs.Add(Header);
		Schema.Layout.BasePropertyBoundary = 16;
		Schema.Layout.SemanticSize = 16;
		Schema.Layout.SemanticAlignment = 8;
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, Header.Target));
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeReflectionFormSchema(
		const EAngelscriptCachedTypeKind TypeKind,
		const EAngelscriptCachedReflectionKind ReflectionKind)
	{
		if (TypeKind == EAngelscriptCachedTypeKind::Class
			&& ReflectionKind == EAngelscriptCachedReflectionKind::UClass)
		{
			return MakeOrdinaryUClassSchema(false);
		}
		if (TypeKind == EAngelscriptCachedTypeKind::Struct
			&& ReflectionKind == EAngelscriptCachedReflectionKind::UStruct)
		{
			return MakeReflectedUStructSchema();
		}
		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(TypeKind);
		Schema.Reflection.ReflectionKind = ReflectionKind;
		if (TypeKind == EAngelscriptCachedTypeKind::Enum
			&& ReflectionKind == EAngelscriptCachedReflectionKind::None)
		{
			Schema.Reflection.ClassReflectionFlags = 0;
		}
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCacheValidationResult DecodeWithMatchingRecordId(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
	{
		FAngelscriptCacheRecordId DeclaredRecordId;
		const FAngelscriptCacheValidationResult IdResult =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				EAngelscriptCacheRecordKind::TypeSchema, Payload, DeclaredRecordId);
		if (!IdResult.IsSuccess())
		{
			OutRecord.Reset();
			return IdResult;
		}
		return FAngelscriptDecodedCacheRecord::TryDecode(
			DeclaredRecordId, Payload, Limits, Budget, OutRecord);
	}

	static FAngelscriptCacheValidationResult DecodeWithMatchingRecordIdAndProbe(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheTypeSchemaAllocationProbeForTests& Probe,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
	{
		FAngelscriptCacheRecordId DeclaredRecordId;
		const FAngelscriptCacheValidationResult IdResult =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				EAngelscriptCacheRecordKind::TypeSchema, Payload, DeclaredRecordId);
		if (!IdResult.IsSuccess())
		{
			OutRecord.Reset();
			return IdResult;
		}
		return FAngelscriptDecodedCacheRecordTestAccess::TryDecodeWithProbe(
			DeclaredRecordId, Payload, Limits, Budget, Probe, OutRecord);
	}

	static const FAngelscriptCachedTypeSchema& RequireTypeSchema(
		const FAngelscriptDecodedCacheRecordHandle& Record)
	{
		check(Record->GetRecordId().Kind == EAngelscriptCacheRecordKind::TypeSchema);
		const FAngelscriptCachedTypeSchema* Schema = Record->TryGetTypeSchema();
		check(Schema != nullptr);
		return *Schema;
	}

	static FAngelscriptDecodedCacheRecordHandle MakeSentinelRecord()
	{
		TArray<uint8> Payload;
		check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			MakeCompleteDelegateSchema(), Payload).IsSuccess());
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
		check(DecodeWithMatchingRecordId(Payload, Limits, Budget, Record).IsSuccess());
		check(Record.IsSet());
		return Record.GetValue();
	}

	struct FMalformedDecodeOutcome
	{
		FAngelscriptCacheValidationResult Result;
		FAngelscriptCacheTypeSchemaTestWireTrace Trace;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		TArray<uint8> Payload;
	};

	static bool ExpectExactFailureAndReset(
		FAutomationTestBase& Test,
		const FAngelscriptCacheValidationResult& Result,
		const TOptional<FAngelscriptDecodedCacheRecordHandle>& Output,
		const EAngelscriptCacheValidationError ExpectedError,
		const EAngelscriptCacheValidationStage ExpectedStage,
		const uint64 ExpectedByteOffset,
		const TCHAR* Context,
		const EAngelscriptCacheRecordKind ExpectedRecordKind =
			EAngelscriptCacheRecordKind::TypeSchema)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = LocalAssert.AreEqual(ExpectedError, Result.Error,
			*FString::Printf(TEXT("%s: Error expected=%u actual=%u"), Context,
				static_cast<uint32>(ExpectedError),
				static_cast<uint32>(Result.Error)));
		bPassed &= LocalAssert.AreEqual(
			FAngelscriptCacheValidationResult::Classify(ExpectedError),
			Result.Class,
			*FString::Printf(TEXT("%s: Class"), Context));
		bPassed &= LocalAssert.AreEqual(ExpectedStage, Result.Stage,
			*FString::Printf(TEXT("%s: Stage expected=%u actual=%u"), Context,
				static_cast<uint32>(ExpectedStage),
				static_cast<uint32>(Result.Stage)));
		bPassed &= LocalAssert.AreEqual(ExpectedRecordKind, Result.RecordKind,
			*FString::Printf(TEXT("%s: RecordKind"), Context));
		bPassed &= LocalAssert.AreEqual(ExpectedByteOffset, Result.ByteOffset,
			*FString::Printf(TEXT("%s: ByteOffset expected=%llu actual=%llu"),
				Context, ExpectedByteOffset, Result.ByteOffset));
		bPassed &= LocalAssert.IsFalse(Output.IsSet(),
			*FString::Printf(TEXT("%s: failure must clear the sentinel output"), Context));
		return bPassed;
	}

	static bool ExpectExactInactiveArmProducerFailure(
		FAutomationTestBase& Test,
		const FAngelscriptCacheValidationResult& Result,
		const TConstArrayView<uint8> ProducedBytes,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = LocalAssert.AreEqual(
			EAngelscriptCacheValidationError::InvalidPresence, Result.Error,
			*FString::Printf(TEXT("%s: Error"), Context));
		bPassed &= LocalAssert.AreEqual(
			EAngelscriptCacheValidationClass::CanonicalSemantic, Result.Class,
			*FString::Printf(TEXT("%s: Class"), Context));
		bPassed &= LocalAssert.AreEqual(
			EAngelscriptCacheRecordKind::TypeSchema, Result.RecordKind,
			*FString::Printf(TEXT("%s: RecordKind"), Context));
		bPassed &= LocalAssert.AreEqual(
			EAngelscriptCacheValidationStage::None, Result.Stage,
			*FString::Printf(TEXT("%s: Stage"), Context));
		bPassed &= LocalAssert.AreEqual(uint64(0), Result.ByteOffset,
			*FString::Printf(TEXT("%s: ByteOffset"), Context));
		bPassed &= LocalAssert.AreEqual(0, ProducedBytes.Num(),
			*FString::Printf(TEXT("%s: producer emits no bytes"), Context));
		return bPassed;
	}

	static bool ExpectExactProducerFailureAndInputUnchanged(
		FAutomationTestBase& Test,
		const FAngelscriptCachedTypeSchema& InvalidSchema,
		const EAngelscriptCacheValidationError ExpectedError,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		TArray<uint8> BeforeBytes;
		FAngelscriptCacheTypeSchemaTestWireTrace BeforeTrace;
		bool bPassed = LocalAssert.IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				InvalidSchema, BeforeBytes, BeforeTrace).IsSuccess(),
			*FString::Printf(TEXT("%s: physical before snapshot"), Context));

		TArray<uint8> ProducedBytes = {0xaa, 0xbb, 0xcc};
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				InvalidSchema, ProducedBytes);
		bPassed &= LocalAssert.AreEqual(ExpectedError, Result.Error,
			*FString::Printf(TEXT("%s: Error expected=%u actual=%u"), Context,
				static_cast<uint32>(ExpectedError),
				static_cast<uint32>(Result.Error)));
		bPassed &= LocalAssert.AreEqual(
			FAngelscriptCacheValidationResult::Classify(ExpectedError), Result.Class,
			*FString::Printf(TEXT("%s: Class"), Context));
		bPassed &= LocalAssert.AreEqual(
			EAngelscriptCacheRecordKind::TypeSchema, Result.RecordKind,
			*FString::Printf(TEXT("%s: RecordKind"), Context));
		bPassed &= LocalAssert.AreEqual(
			EAngelscriptCacheValidationStage::None, Result.Stage,
			*FString::Printf(TEXT("%s: Stage expected=%u actual=%u"), Context,
				static_cast<uint32>(EAngelscriptCacheValidationStage::None),
				static_cast<uint32>(Result.Stage)));
		bPassed &= LocalAssert.AreEqual(uint64(0), Result.ByteOffset,
			*FString::Printf(TEXT("%s: ByteOffset"), Context));
		bPassed &= LocalAssert.AreEqual(0, ProducedBytes.Num(),
			*FString::Printf(TEXT("%s: producer atomically clears output"), Context));

		TArray<uint8> AfterBytes;
		FAngelscriptCacheTypeSchemaTestWireTrace AfterTrace;
		bPassed &= LocalAssert.IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				InvalidSchema, AfterBytes, AfterTrace).IsSuccess(),
			*FString::Printf(TEXT("%s: physical after snapshot"), Context));
		bPassed &= LocalAssert.AreEqual(Hex(BeforeBytes), Hex(AfterBytes),
			*FString::Printf(TEXT("%s: input DTO wire semantics unchanged"), Context));
		return bPassed;
	}

	static bool ExpectExactNormalProducerSuccessAndInputUnchanged(
		FAutomationTestBase& Test,
		const FAngelscriptCachedTypeSchema& InputSchema,
		const TCHAR* Context,
		const TConstArrayView<uint8> ExpectedCanonicalPayload = {},
		TArray<uint8>* OutProducedPayload = nullptr)
	{
		FNoDiscardAsserter LocalAssert(Test);
		TArray<uint8> BeforeBytes;
		FAngelscriptCacheTypeSchemaTestWireTrace BeforeTrace;
		bool bPassed = LocalAssert.IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				InputSchema, BeforeBytes, BeforeTrace).IsSuccess(),
			*FString::Printf(TEXT("%s: physical before snapshot"), Context));

		TArray<uint8> ProducedBytes = {0xaa, 0xbb, 0xcc};
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				InputSchema, ProducedBytes);
		bPassed &= LocalAssert.IsTrue(Result.IsSuccess(),
			*FString::Printf(TEXT(
				"%s: normal producer succeeds error=%u stage=%u offset=%llu"),
				Context, static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Stage), Result.ByteOffset));
		bPassed &= LocalAssert.IsTrue(!ProducedBytes.IsEmpty(),
			*FString::Printf(TEXT("%s: normal producer emits payload"), Context));
		if (!ExpectedCanonicalPayload.IsEmpty())
		{
			bPassed &= LocalAssert.AreEqual(Hex(ExpectedCanonicalPayload), Hex(ProducedBytes),
				*FString::Printf(TEXT("%s: canonical payload"), Context));
		}
		if (OutProducedPayload != nullptr)
		{
			*OutProducedPayload = ProducedBytes;
		}

		TArray<uint8> AfterBytes;
		FAngelscriptCacheTypeSchemaTestWireTrace AfterTrace;
		bPassed &= LocalAssert.IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				InputSchema, AfterBytes, AfterTrace).IsSuccess(),
			*FString::Printf(TEXT("%s: physical after snapshot"), Context));
		bPassed &= LocalAssert.AreEqual(Hex(BeforeBytes), Hex(AfterBytes),
			*FString::Printf(TEXT("%s: input DTO wire semantics unchanged"), Context));
		return bPassed;
	}

	static FMalformedDecodeOutcome DecodePhysicalOnlyFixture(
		const FAngelscriptCachedTypeSchema& Schema,
		const FAngelscriptCacheReadLimits& Limits = {})
	{
		FMalformedDecodeOutcome Outcome;
		check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
			Schema, Outcome.Payload, Outcome.Trace).IsSuccess());
		Outcome.Output = MakeSentinelRecord();
		FAngelscriptCacheReadBudget Budget;
		Outcome.Result = DecodeWithMatchingRecordId(
			Outcome.Payload, Limits, Budget, Outcome.Output);
		return Outcome;
	}

	static void PatchRawByte(
		TArray<uint8>& Payload,
		const FAngelscriptCacheTypeSchemaTestWireTrace& Trace,
		const EAngelscriptCacheTypeSchemaTestField Field,
		const uint8 RawValue,
		const int32 PrimaryIndex = INDEX_NONE,
		const int32 SecondaryIndex = INDEX_NONE)
	{
		const TOptional<FAngelscriptCacheTypeSchemaTestWireSpan> Span =
			Trace.FindUnique(Field, PrimaryIndex, SecondaryIndex);
		check(Span.IsSet() && Span->Size >= 1 && Span->Offset < uint64(Payload.Num()));
		Payload[static_cast<int32>(Span->Offset)] = RawValue;
	}

	static void PatchRawUInt32(
		TArray<uint8>& Payload,
		const FAngelscriptCacheTypeSchemaTestWireTrace& Trace,
		const EAngelscriptCacheTypeSchemaTestField Field,
		const uint32 RawValue,
		const int32 PrimaryIndex = INDEX_NONE,
		const int32 SecondaryIndex = INDEX_NONE)
	{
		const TOptional<FAngelscriptCacheTypeSchemaTestWireSpan> Span =
			Trace.FindUnique(Field, PrimaryIndex, SecondaryIndex);
		check(Span.IsSet() && Span->Size == sizeof(uint32)
			&& Span->Offset + Span->Size <= uint64(Payload.Num()));
		FMemory::Memcpy(Payload.GetData() + Span->Offset, &RawValue, sizeof(RawValue));
	}

	static void PatchHashBytes(
		TArray<uint8>& Payload,
		const FAngelscriptCacheTypeSchemaTestWireTrace& Trace,
		const EAngelscriptCacheTypeSchemaTestField Field,
		const FAngelscriptHash256& Hash,
		const int32 PrimaryIndex = INDEX_NONE)
	{
		const TOptional<FAngelscriptCacheTypeSchemaTestWireSpan> Span =
			Trace.FindUnique(Field, PrimaryIndex, INDEX_NONE);
		check(Span.IsSet() && Span->Size == sizeof(FBlake3Hash::ByteArray)
			&& Span->Offset + Span->Size <= uint64(Payload.Num()));
		FMemory::Memcpy(Payload.GetData() + Span->Offset,
			Hash.Value.GetBytes(), sizeof(FBlake3Hash::ByteArray));
	}

	static bool IsExpectedTypeSemanticFlagMaskValid(
		const EAngelscriptCachedTypeKind Kind,
		const uint32 Flags)
	{
		constexpr uint32 Abstract = 0x01;
		constexpr uint32 Final = 0x02;
		constexpr uint32 Shared = 0x04;
		constexpr uint32 Generated = 0x08;
		constexpr uint32 HasDefaultConstructor = 0x10;
		constexpr uint32 HasDestructor = 0x20;
		constexpr uint32 ValueType = 0x40;
		constexpr uint32 ReferenceType = 0x80;
		if ((Flags & ~0xffu) != 0
			|| (Flags & (Abstract | Final)) == (Abstract | Final)
			|| (Flags & (ValueType | ReferenceType)) == (ValueType | ReferenceType))
		{
			return false;
		}

		uint32 Required = 0;
		uint32 Allowed = 0;
		switch (Kind)
		{
		case EAngelscriptCachedTypeKind::Class:
			Required = ReferenceType;
			Allowed = Abstract | Final | Shared | Generated
				| HasDefaultConstructor | HasDestructor | ReferenceType;
			break;
		case EAngelscriptCachedTypeKind::Struct:
			Required = Final | ValueType;
			Allowed = Final | Shared | Generated | HasDefaultConstructor
				| HasDestructor | ValueType;
			break;
		case EAngelscriptCachedTypeKind::Interface:
			Required = Abstract | ReferenceType;
			Allowed = Abstract | Shared | ReferenceType;
			break;
		case EAngelscriptCachedTypeKind::Enum:
			Required = Final | ValueType;
			Allowed = Final | Shared | ValueType;
			break;
		case EAngelscriptCachedTypeKind::Delegate:
			Required = Final | Generated | ValueType;
			Allowed = Final | Generated | HasDefaultConstructor | HasDestructor | ValueType;
			break;
		case EAngelscriptCachedTypeKind::Typedef:
			break;
		case EAngelscriptCachedTypeKind::Funcdef:
			Required = ReferenceType;
			Allowed = Shared | ReferenceType;
			break;
		default:
			return false;
		}
		return (Flags & Required) == Required && (Flags & ~Allowed) == 0;
	}

	static void ConfigureFlagCoupledBehaviors(
		FAngelscriptCachedTypeSchema& Schema,
		const uint32 Flags)
	{
		const bool bNeedsConstruct = (Flags
			& static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor)) != 0;
		const bool bNeedsDestruct = (Flags
			& static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::HasDestructor)) != 0;
		Schema.OrderedBehaviorSlots.RemoveAll([](const FAngelscriptCachedBehaviorSlot& Slot)
		{
			return Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Construct
				|| Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Factory
				|| Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Destruct;
		});
		Schema.Dependencies.RemoveAll([](const FAngelscriptCacheSemanticDependency& Dependency)
		{
			return Dependency.Target.StableKey == MakeHash(0x32)
				|| Dependency.Target.StableKey == MakeHash(0x72)
				|| Dependency.Target.StableKey == MakeHash(0x74)
				|| Dependency.Target.StableKey == MakeHash(0x76);
		});

		const auto AddScriptBehavior = [&](const EAngelscriptCachedBehaviorKind Kind,
			const uint8 KeyFill, const uint8 AbiFill)
		{
			FAngelscriptCachedBehaviorSlot Slot;
			Slot.BehaviorKind = Kind;
			Slot.SlotOrdinal = 0;
			Slot.Target = MakeReference(
				EAngelscriptCacheReferenceKind::ScriptFunction, KeyFill, AbiFill);
			Slot.DeclaringOwner = Schema.TypeKey;
			Schema.OrderedBehaviorSlots.Add(Slot);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration, Slot.Target));
		};
		if (bNeedsConstruct)
		{
			AddScriptBehavior(EAngelscriptCachedBehaviorKind::Construct, 0x72, 0x73);
			if (Schema.TypeKind == EAngelscriptCachedTypeKind::Class)
			{
				AddScriptBehavior(EAngelscriptCachedBehaviorKind::Factory, 0x74, 0x75);
			}
		}
		if (bNeedsDestruct)
		{
			AddScriptBehavior(EAngelscriptCachedBehaviorKind::Destruct, 0x76, 0x77);
		}
		Schema.OrderedBehaviorSlots.Sort([](
			const FAngelscriptCachedBehaviorSlot& A,
			const FAngelscriptCachedBehaviorSlot& B)
		{
			const uint8 AKind = static_cast<uint8>(A.BehaviorKind);
			const uint8 BKind = static_cast<uint8>(B.BehaviorKind);
			return AKind != BKind ? AKind < BKind : A.SlotOrdinal < B.SlotOrdinal;
		});
		Schema.Dependencies.Sort([](
			const FAngelscriptCacheSemanticDependency& A,
			const FAngelscriptCacheSemanticDependency& B)
		{
			const uint8 AKind = static_cast<uint8>(A.Kind);
			const uint8 BKind = static_cast<uint8>(B.Kind);
			return AKind != BKind ? AKind < BKind
				: A.Target.StableKey < B.Target.StableKey;
		});
	}

	static bool DataTypeReferencesTargetForTests(
		const FAngelscriptCachedDataType& DataType,
		const FAngelscriptCacheStableReference& Target)
	{
		if (DataType.TypeReference.IsSet()
			&& DataType.TypeReference.GetValue() == Target)
		{
			return true;
		}
		for (const FAngelscriptCachedDataType& SubType : DataType.OrderedSubTypes)
		{
			if (DataTypeReferencesTargetForTests(SubType, Target))
			{
				return true;
			}
		}
		return false;
	}

	static bool SchemaReferencesTargetOutsideDependenciesForTests(
		const FAngelscriptCachedTypeSchema& Schema,
		const FAngelscriptCacheStableReference& Target)
	{
		for (const FAngelscriptCachedTypeRelation& Relation : Schema.Relations)
		{
			if (Relation.Target == Target)
			{
				return true;
			}
		}
		for (const FAngelscriptCachedTypeLayoutInput& Input : Schema.LayoutInputs)
		{
			if (Input.Target == Target)
			{
				return true;
			}
		}
		for (const FAngelscriptCachedPropertySchema& Property : Schema.OrderedProperties)
		{
			if (DataTypeReferencesTargetForTests(Property.Type, Target))
			{
				return true;
			}
		}
		if (Target.Kind == EAngelscriptCacheReferenceKind::ScriptFunction)
		{
			for (const FAngelscriptCachedMethodEntry& Method : Schema.OrderedMethods)
			{
				if (Method.FunctionKey.Hash == Target.StableKey
					&& Method.ExpectedDeclarationAbi == Target.ExpectedAbi)
				{
					return true;
				}
			}
			for (const FAngelscriptCachedVirtualFunctionSlot& Slot :
				Schema.VirtualFunctionTable)
			{
				if (Slot.FunctionKey.Hash == Target.StableKey
					&& Slot.ExpectedDeclarationAbi == Target.ExpectedAbi)
				{
					return true;
				}
			}
			if (Schema.KindPayload.Callable.IsSet()
				&& Schema.KindPayload.Callable->SignatureFunctionKey.Hash
					== Target.StableKey
				&& Schema.KindPayload.Callable->ExpectedSignatureAbi
					== Target.ExpectedAbi)
			{
				return true;
			}
		}
		for (const FAngelscriptCachedBehaviorSlot& Slot : Schema.OrderedBehaviorSlots)
		{
			if (Slot.Target == Target)
			{
				return true;
			}
		}
		if (Schema.KindPayload.Typedef.IsSet()
			&& DataTypeReferencesTargetForTests(
				Schema.KindPayload.Typedef->AliasedType, Target))
		{
			return true;
		}
		for (const FAngelscriptCachedReflectedFunctionMember& Member :
			Schema.Reflection.OrderedUFunctionMembers)
		{
			if (Member.Target == Target)
			{
				return true;
			}
		}
		return false;
	}

	static void ClearBehaviorOwnedStateForTests(
		FAngelscriptCachedTypeSchema& Schema)
	{
		TArray<FAngelscriptCacheStableReference> RemovedBehaviorTargets;
		for (const FAngelscriptCachedBehaviorSlot& Slot : Schema.OrderedBehaviorSlots)
		{
			RemovedBehaviorTargets.AddUnique(Slot.Target);
		}
		Schema.OrderedBehaviorSlots.Reset();
		Schema.TypeSemanticFlags &= ~(
			static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor)
			| static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::HasDestructor));
		Schema.Dependencies.RemoveAll([&](
			const FAngelscriptCacheSemanticDependency& Dependency)
		{
			return RemovedBehaviorTargets.Contains(Dependency.Target)
				&& !SchemaReferencesTargetOutsideDependenciesForTests(
					Schema, Dependency.Target);
		});
	}

	static void ClearMethodAndVftOwnedStateForTests(
		FAngelscriptCachedTypeSchema& Schema)
	{
		TArray<FAngelscriptCacheStableReference> RemovedTargets;
		for (const FAngelscriptCachedMethodEntry& Method : Schema.OrderedMethods)
		{
			RemovedTargets.AddUnique(FAngelscriptCacheStableReference{
				EAngelscriptCacheReferenceKind::ScriptFunction,
				Method.FunctionKey.Hash, Method.ExpectedDeclarationAbi});
		}
		for (const FAngelscriptCachedVirtualFunctionSlot& Slot :
			Schema.VirtualFunctionTable)
		{
			RemovedTargets.AddUnique(FAngelscriptCacheStableReference{
				EAngelscriptCacheReferenceKind::ScriptFunction,
				Slot.FunctionKey.Hash, Slot.ExpectedDeclarationAbi});
		}
		Schema.OrderedMethods.Reset();
		Schema.VirtualFunctionTable.Reset();
		Schema.Dependencies.RemoveAll([&](
			const FAngelscriptCacheSemanticDependency& Dependency)
		{
			return RemovedTargets.Contains(Dependency.Target)
				&& !SchemaReferencesTargetOutsideDependenciesForTests(
					Schema, Dependency.Target);
		});
	}

	static void RebuildMethodAndVftDependenciesForTests(
		FAngelscriptCachedTypeSchema& Schema)
	{
		Schema.Dependencies.RemoveAll([&](
			const FAngelscriptCacheSemanticDependency& Dependency)
		{
			return Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::Declaration
				&& !SchemaReferencesTargetOutsideDependenciesForTests(
					Schema, Dependency.Target);
		});
		for (const FAngelscriptCachedMethodEntry& Method : Schema.OrderedMethods)
		{
			Schema.Dependencies.AddUnique(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				FAngelscriptCacheStableReference{
					EAngelscriptCacheReferenceKind::ScriptFunction,
					Method.FunctionKey.Hash, Method.ExpectedDeclarationAbi}));
		}
		for (const FAngelscriptCachedVirtualFunctionSlot& Slot :
			Schema.VirtualFunctionTable)
		{
			Schema.Dependencies.AddUnique(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				FAngelscriptCacheStableReference{
					EAngelscriptCacheReferenceKind::ScriptFunction,
					Slot.FunctionKey.Hash, Slot.ExpectedDeclarationAbi}));
		}
	}

	static void CanonicalizeDependenciesAndFinalizeForTests(
		FAngelscriptCachedTypeSchema& Schema)
	{
		TArray<FAngelscriptCacheSemanticDependency> MergedDependencies;
		MergedDependencies.Reserve(Schema.Dependencies.Num());
		for (const FAngelscriptCacheSemanticDependency& Dependency : Schema.Dependencies)
		{
			MergedDependencies.AddUnique(Dependency);
		}
		MergedDependencies.Sort([](
			const FAngelscriptCacheSemanticDependency& A,
			const FAngelscriptCacheSemanticDependency& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		Schema.Dependencies = MoveTemp(MergedDependencies);
		FinalizeValidFixtureHashes(Schema);
	}

	enum class EBehaviorCompanionRecipeForTests : uint8
	{
		None,
		ClassConstructBalanceFactory,
		ClassFactoryBalanceConstruct,
		SetDestructorFlag,
		ScriptCopyConstructExactPeer,
		ScriptCopyFactoryExactPeers,
	};

	struct FBehaviorProductCellForTests
	{
		EAngelscriptCachedTypeKind TypeKind;
		EAngelscriptCachedBehaviorKind BehaviorKind;
		uint32 Cardinality;
		EAngelscriptCacheReferenceKind TargetKind;
		uint8 SuccessOwnerMask;
	};

	static TConstArrayView<FBehaviorProductCellForTests>
	GetBehaviorProductCellsForTests()
	{
		using T = EAngelscriptCachedTypeKind;
		using B = EAngelscriptCachedBehaviorKind;
		using R = EAngelscriptCacheReferenceKind;
		constexpr uint8 ScriptOwners = 0x0e;
		constexpr uint8 EnvironmentOwnerAbsent = 0x01;
		static const FBehaviorProductCellForTests Cells[] = {
			{T::Class, B::Construct, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::Construct, 2, R::ScriptFunction, ScriptOwners},
			{T::Class, B::Factory, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::Factory, 2, R::ScriptFunction, ScriptOwners},
			{T::Class, B::Destruct, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::ListFactory, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::AddRef, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::Release, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::GetWeakRefFlag, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::GetRefCount, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::SetGcFlag, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::GetGcFlag, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::EnumRefs, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::ReleaseRefs, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::Copy, 1, R::ScriptFunction, ScriptOwners},
			{T::Class, B::CopyFactory, 1, R::ScriptFunction, ScriptOwners},

			{T::Class, B::Destruct, 1, R::EnvironmentSymbol, EnvironmentOwnerAbsent},
			{T::Class, B::AddRef, 1, R::EnvironmentSymbol, EnvironmentOwnerAbsent},
			{T::Class, B::Release, 1, R::EnvironmentSymbol, EnvironmentOwnerAbsent},
			{T::Class, B::GetWeakRefFlag, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
			{T::Class, B::GetRefCount, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
			{T::Class, B::SetGcFlag, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
			{T::Class, B::GetGcFlag, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
			{T::Class, B::EnumRefs, 1, R::EnvironmentSymbol, EnvironmentOwnerAbsent},
			{T::Class, B::ReleaseRefs, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
			{T::Class, B::Copy, 1, R::EnvironmentSymbol, EnvironmentOwnerAbsent},
			{T::Class, B::CopyFactory, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},

			{T::Struct, B::Construct, 1, R::ScriptFunction, ScriptOwners},
			{T::Struct, B::Construct, 2, R::ScriptFunction, ScriptOwners},
			{T::Struct, B::ListConstruct, 1, R::ScriptFunction, ScriptOwners},
			{T::Struct, B::Destruct, 1, R::ScriptFunction, ScriptOwners},
			{T::Struct, B::Copy, 1, R::ScriptFunction, ScriptOwners},
			{T::Struct, B::CopyConstruct, 1, R::ScriptFunction, ScriptOwners},
			{T::Struct, B::Destruct, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
			{T::Struct, B::Copy, 1, R::EnvironmentSymbol, EnvironmentOwnerAbsent},
			{T::Struct, B::CopyConstruct, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},

			{T::Delegate, B::Construct, 1, R::ScriptFunction, ScriptOwners},
			{T::Delegate, B::Construct, 2, R::ScriptFunction, ScriptOwners},
			{T::Delegate, B::ListConstruct, 1, R::ScriptFunction, ScriptOwners},
			{T::Delegate, B::Destruct, 1, R::ScriptFunction, ScriptOwners},
			{T::Delegate, B::Copy, 1, R::ScriptFunction, ScriptOwners},
			{T::Delegate, B::CopyConstruct, 1, R::ScriptFunction, ScriptOwners},
			{T::Delegate, B::Destruct, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
			{T::Delegate, B::Copy, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
			{T::Delegate, B::CopyConstruct, 1, R::EnvironmentSymbol,
				EnvironmentOwnerAbsent},
		};
		return MakeArrayView(Cells);
	}

	static const FBehaviorProductCellForTests* FindBehaviorProductCellForTests(
		const EAngelscriptCachedTypeKind TypeKind,
		const EAngelscriptCachedBehaviorKind BehaviorKind,
		const uint32 Cardinality,
		const EAngelscriptCacheReferenceKind TargetKind)
	{
		for (const FBehaviorProductCellForTests& Cell :
			GetBehaviorProductCellsForTests())
		{
			if (Cell.TypeKind == TypeKind
				&& Cell.BehaviorKind == BehaviorKind
				&& Cell.Cardinality == Cardinality
				&& Cell.TargetKind == TargetKind)
			{
				return &Cell;
			}
		}
		return nullptr;
	}

	struct FBehaviorCompanionRuleForTests
	{
		EAngelscriptCachedTypeKind TypeKind;
		EAngelscriptCachedBehaviorKind BehaviorKind;
		EAngelscriptCacheReferenceKind TargetKind;
		uint8 OwnerMask;
		EBehaviorCompanionRecipeForTests Recipe;
	};

	static EBehaviorCompanionRecipeForTests FindBehaviorCompanionRecipeForTests(
		const EAngelscriptCachedTypeKind TypeKind,
		const EAngelscriptCachedBehaviorKind BehaviorKind,
		const EAngelscriptCacheReferenceKind TargetKind,
		const uint8 OwnerCase)
	{
		using T = EAngelscriptCachedTypeKind;
		using B = EAngelscriptCachedBehaviorKind;
		using R = EAngelscriptCacheReferenceKind;
		using C = EBehaviorCompanionRecipeForTests;
		static const FBehaviorCompanionRuleForTests Rules[] = {
			{T::Class, B::Construct, R::Invalid, 0x0f,
				C::ClassConstructBalanceFactory},
			{T::Class, B::Factory, R::Invalid, 0x0f,
				C::ClassFactoryBalanceConstruct},
			{T::Class, B::Destruct, R::Invalid, 0x0f, C::SetDestructorFlag},
			{T::Struct, B::Destruct, R::Invalid, 0x0f, C::SetDestructorFlag},
			{T::Delegate, B::Destruct, R::Invalid, 0x0f, C::SetDestructorFlag},
			{T::Struct, B::CopyConstruct, R::ScriptFunction, 0x0e,
				C::ScriptCopyConstructExactPeer},
			{T::Delegate, B::CopyConstruct, R::ScriptFunction, 0x0e,
				C::ScriptCopyConstructExactPeer},
			{T::Class, B::CopyFactory, R::ScriptFunction, 0x0e,
				C::ScriptCopyFactoryExactPeers},
		};
		const uint8 OwnerBit = 1u << OwnerCase;
		for (const FBehaviorCompanionRuleForTests& Rule : Rules)
		{
			if (Rule.TypeKind == TypeKind
				&& Rule.BehaviorKind == BehaviorKind
				&& (Rule.TargetKind == R::Invalid || Rule.TargetKind == TargetKind)
				&& (Rule.OwnerMask & OwnerBit) != 0)
			{
				return Rule.Recipe;
			}
		}
		return C::None;
	}

	static void AddBehaviorSlotForTests(
		FAngelscriptCachedTypeSchema& Schema,
		const EAngelscriptCachedBehaviorKind BehaviorKind,
		const uint32 Ordinal,
		const FAngelscriptCacheStableReference& Target,
		const TOptional<FAngelscriptStableTypeKey>& DeclaringOwner)
	{
		FAngelscriptCachedBehaviorSlot Slot;
		Slot.BehaviorKind = BehaviorKind;
		Slot.SlotOrdinal = Ordinal;
		Slot.Target = Target;
		Slot.DeclaringOwner = DeclaringOwner;
		Schema.OrderedBehaviorSlots.Add(Slot);
		Schema.Dependencies.AddUnique(MakeDependency(
			Target.Kind == EAngelscriptCacheReferenceKind::ScriptFunction
				? EAngelscriptCacheSemanticDependencyKind::Declaration
				: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
			Target));
	}

	static void RebuildBehaviorDependenciesForTests(
		FAngelscriptCachedTypeSchema& Schema)
	{
		Schema.Dependencies.RemoveAll([&](
			const FAngelscriptCacheSemanticDependency& Dependency)
		{
			return (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::Declaration
				|| Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi)
				&& !SchemaReferencesTargetOutsideDependenciesForTests(
					Schema, Dependency.Target);
		});
		for (const FAngelscriptCachedBehaviorSlot& Slot :
			Schema.OrderedBehaviorSlots)
		{
			Schema.Dependencies.AddUnique(MakeDependency(
				Slot.Target.Kind
					== EAngelscriptCacheReferenceKind::ScriptFunction
					? EAngelscriptCacheSemanticDependencyKind::Declaration
					: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
				Slot.Target));
		}
	}

	static void CanonicalSortBehaviorSlotsForTests(
		FAngelscriptCachedTypeSchema& Schema)
	{
		Schema.OrderedBehaviorSlots.Sort([](
			const FAngelscriptCachedBehaviorSlot& A,
			const FAngelscriptCachedBehaviorSlot& B)
		{
			const uint8 AKind = static_cast<uint8>(A.BehaviorKind);
			const uint8 BKind = static_cast<uint8>(B.BehaviorKind);
			return AKind != BKind ? AKind < BKind
				: A.SlotOrdinal < B.SlotOrdinal;
		});
	}

	static FAngelscriptCachedTypeSchema BuildBehaviorProductFixtureForTests(
		const EAngelscriptCachedTypeKind TypeKind,
		const EAngelscriptCachedBehaviorKind BehaviorKind,
		const uint32 Cardinality,
		const EAngelscriptCacheReferenceKind TargetKind,
		const uint8 OwnerCase)
	{
		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(TypeKind);
		ClearBehaviorOwnedStateForTests(Schema);
		const FAngelscriptStableTypeKey OtherOwnerA{MakeHash(0xe6)};
		const FAngelscriptStableTypeKey OtherOwnerB{MakeHash(0xe7)};
		TOptional<FAngelscriptStableTypeKey> Owner;
		if (OwnerCase != 0)
		{
			Owner = OwnerCase == 1 ? Schema.TypeKey
				: OwnerCase == 2 ? OtherOwnerA : OtherOwnerB;
		}

		TArray<FAngelscriptCachedBehaviorSlot> PrimarySlots;
		for (uint32 Index = 0; Index < Cardinality; ++Index)
		{
			const FAngelscriptCacheStableReference Target = MakeReference(
				TargetKind, static_cast<uint8>(0xa0 + Index * 2),
				static_cast<uint8>(0xa1 + Index * 2));
			AddBehaviorSlotForTests(
				Schema, BehaviorKind, Index, Target, Owner);
			PrimarySlots.Add(Schema.OrderedBehaviorSlots.Last());
		}

		const EBehaviorCompanionRecipeForTests Recipe =
			FindBehaviorCompanionRecipeForTests(
				TypeKind, BehaviorKind, TargetKind, OwnerCase);
		switch (Recipe)
		{
		case EBehaviorCompanionRecipeForTests::ClassConstructBalanceFactory:
			for (uint32 Index = 0; Index < Cardinality; ++Index)
			{
				AddBehaviorSlotForTests(
					Schema, EAngelscriptCachedBehaviorKind::Factory, Index,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
						static_cast<uint8>(0xb0 + Index * 2),
						static_cast<uint8>(0xb1 + Index * 2)),
					Schema.TypeKey);
			}
			break;
		case EBehaviorCompanionRecipeForTests::ClassFactoryBalanceConstruct:
			for (uint32 Index = 0; Index < Cardinality; ++Index)
			{
				AddBehaviorSlotForTests(
					Schema, EAngelscriptCachedBehaviorKind::Construct, Index,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
						static_cast<uint8>(0xb0 + Index * 2),
						static_cast<uint8>(0xb1 + Index * 2)),
					Schema.TypeKey);
			}
			break;
		case EBehaviorCompanionRecipeForTests::SetDestructorFlag:
			Schema.TypeSemanticFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::HasDestructor);
			break;
		case EBehaviorCompanionRecipeForTests::ScriptCopyConstructExactPeer:
			for (const FAngelscriptCachedBehaviorSlot& Primary : PrimarySlots)
			{
				AddBehaviorSlotForTests(
					Schema, EAngelscriptCachedBehaviorKind::Construct,
					Primary.SlotOrdinal, Primary.Target, Primary.DeclaringOwner);
			}
			break;
		case EBehaviorCompanionRecipeForTests::ScriptCopyFactoryExactPeers:
			for (const FAngelscriptCachedBehaviorSlot& Primary : PrimarySlots)
			{
				AddBehaviorSlotForTests(
					Schema, EAngelscriptCachedBehaviorKind::Factory,
					Primary.SlotOrdinal, Primary.Target, Primary.DeclaringOwner);
				AddBehaviorSlotForTests(
					Schema, EAngelscriptCachedBehaviorKind::Construct,
					Primary.SlotOrdinal,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
						static_cast<uint8>(0xc0 + Primary.SlotOrdinal * 2),
						static_cast<uint8>(0xc1 + Primary.SlotOrdinal * 2)),
					Schema.TypeKey);
			}
			break;
		case EBehaviorCompanionRecipeForTests::None:
			break;
		}

		CanonicalSortBehaviorSlotsForTests(Schema);
		CanonicalizeDependenciesAndFinalizeForTests(Schema);
		return Schema;
	}

	static int32 FindBehaviorPhysicalIndexForTests(
		const FAngelscriptCachedTypeSchema& Schema,
		const EAngelscriptCachedBehaviorKind BehaviorKind,
		const uint32 Ordinal,
		const FAngelscriptCacheStableReference& Target)
	{
		for (int32 Index = 0; Index < Schema.OrderedBehaviorSlots.Num(); ++Index)
		{
			const FAngelscriptCachedBehaviorSlot& Slot =
				Schema.OrderedBehaviorSlots[Index];
			if (Slot.BehaviorKind == BehaviorKind
				&& Slot.SlotOrdinal == Ordinal
				&& Slot.Target == Target)
			{
				return Index;
			}
		}
		checkNoEntry();
		return INDEX_NONE;
	}

	enum class EPropertyOwnerFormForTests : uint8
	{
		PlainClass,
		OrdinaryUClass,
		PlainStruct,
		UStruct,
		Delegate,
	};

	static bool IsExpectedClassReflectionMaskValid(
		const bool bStatics,
		const bool bHasScriptBase,
		const bool bAbstractType,
		const uint32 Mask)
	{
		constexpr uint32 SuperIsCode = 0x001;
		constexpr uint32 Statics = 0x002;
		constexpr uint32 Abstract = 0x004;
		constexpr uint32 IsStruct = 0x200;
		constexpr uint32 OrdinaryAllowed = 0x1fdu;
		if ((Mask & ~0x3ffu) != 0)
		{
			return false;
		}
		if (bStatics)
		{
			return (Mask & (SuperIsCode | Statics)) == (SuperIsCode | Statics)
				&& (Mask & ~(SuperIsCode | Statics | 0x100u)) == 0;
		}
		return (Mask & (Statics | IsStruct)) == 0
			&& (Mask & ~OrdinaryAllowed) == 0
			&& ((Mask & SuperIsCode) != 0) == !bHasScriptBase
			&& ((Mask & Abstract) != 0) == bAbstractType;
	}

	static bool IsExpectedPropertyMaskValid(
		const EPropertyOwnerFormForTests Owner,
		const uint32 Mask,
		const EAngelscriptCachedReplicationCondition ReplicationCondition)
	{
		constexpr uint32 HasUnreal = 0x00001;
		constexpr uint32 BlueprintReadable = 0x00002;
		constexpr uint32 BlueprintWritable = 0x00004;
		constexpr uint32 Replicated = 0x00400;
		constexpr uint32 SkipReplication = 0x00800;
		constexpr uint32 RepNotify = 0x04000;
		constexpr uint32 Config = 0x08000;
		if ((Mask & ~0x7ffffu) != 0)
		{
			return false;
		}
		if (Mask == 0)
		{
			return ReplicationCondition
				== EAngelscriptCachedReplicationCondition::None;
		}
		if ((Mask & HasUnreal) == 0
			|| ((Mask & BlueprintWritable) != 0 && (Mask & BlueprintReadable) == 0)
			|| ((Mask & RepNotify) != 0 && (Mask & Replicated) == 0)
			|| (ReplicationCondition != EAngelscriptCachedReplicationCondition::None
				&& (Mask & Replicated) == 0)
			|| ((Mask & Replicated) == 0
				&& ReplicationCondition != EAngelscriptCachedReplicationCondition::None)
			|| ((Mask & SkipReplication) != 0
				&& ((Mask & (Replicated | RepNotify)) != 0
					|| ReplicationCondition != EAngelscriptCachedReplicationCondition::None))
			|| ReplicationCondition == EAngelscriptCachedReplicationCondition::NetGroup)
		{
			return false;
		}

		switch (Owner)
		{
		case EPropertyOwnerFormForTests::OrdinaryUClass:
			return (Mask & SkipReplication) == 0;
		case EPropertyOwnerFormForTests::UStruct:
			return (Mask & (Replicated | RepNotify | Config)) == 0;
		case EPropertyOwnerFormForTests::PlainClass:
		case EPropertyOwnerFormForTests::PlainStruct:
		case EPropertyOwnerFormForTests::Delegate:
			return false;
		default:
			return false;
		}
	}

	static EAngelscriptCacheValidationError ExpectedPropertyMaskError(
		const EPropertyOwnerFormForTests Owner,
		const uint32 Mask,
		const EAngelscriptCachedReplicationCondition ReplicationCondition)
	{
		if (IsExpectedPropertyMaskValid(Owner, Mask, ReplicationCondition))
		{
			return EAngelscriptCacheValidationError::None;
		}
		constexpr uint32 HasUnreal = 0x00001;
		constexpr uint32 BlueprintReadable = 0x00002;
		constexpr uint32 BlueprintWritable = 0x00004;
		constexpr uint32 Replicated = 0x00400;
		constexpr uint32 SkipReplication = 0x00800;
		constexpr uint32 RepNotify = 0x04000;
		const bool bContradictory =
			(Mask != 0 && (Mask & HasUnreal) == 0)
			|| ((Mask & BlueprintWritable) != 0 && (Mask & BlueprintReadable) == 0)
			|| ((Mask & RepNotify) != 0 && (Mask & Replicated) == 0)
			|| (ReplicationCondition != EAngelscriptCachedReplicationCondition::None
				&& (Mask & Replicated) == 0)
			|| ((Mask & SkipReplication) != 0
				&& ((Mask & (Replicated | RepNotify)) != 0
					|| ReplicationCondition != EAngelscriptCachedReplicationCondition::None))
			|| ReplicationCondition == EAngelscriptCachedReplicationCondition::NetGroup;
		return bContradictory
			? EAngelscriptCacheValidationError::InvalidQualifierCombination
			: EAngelscriptCacheValidationError::InvalidPresence;
	}

	static FAngelscriptCachedTypeSchema MakePropertyOwnerFixture(
		const EPropertyOwnerFormForTests Owner)
	{
		FAngelscriptCachedTypeSchema Schema;
		switch (Owner)
		{
		case EPropertyOwnerFormForTests::PlainClass:
			Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
			break;
		case EPropertyOwnerFormForTests::OrdinaryUClass:
			Schema = MakeOrdinaryUClassSchema(false);
			break;
		case EPropertyOwnerFormForTests::PlainStruct:
			Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
			break;
		case EPropertyOwnerFormForTests::UStruct:
			Schema = MakeReflectedUStructSchema();
			break;
		case EPropertyOwnerFormForTests::Delegate:
			return MakeCompleteDelegateSchema();
		default:
			checkNoEntry();
		}
		FAngelscriptCachedPropertySchema Property =
			MakeCompleteDelegateSchema().OrderedProperties[0];
		Property.SemanticByteOffset = Schema.Layout.BasePropertyBoundary;
		Property.PropertySemanticFlags = 0;
		Property.ReplicationCondition = EAngelscriptCachedReplicationCondition::None;
		Property.Metadata.Reset();
		Schema.OrderedProperties.Add(MoveTemp(Property));
		Schema.Layout.SemanticAlignment = 8;
		Schema.Layout.SemanticSize = Align(
			Schema.Layout.BasePropertyBoundary + uint64(4), uint64(8));
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeThreePropertyLayoutSchema()
	{
		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Struct);
		Schema.CanonicalName = TEXT("FThreePropertyLayout");
		Schema.OrderedProperties.Reset();
		const EAngelscriptCachedPrimitiveType PrimitiveKinds[] = {
			EAngelscriptCachedPrimitiveType::Int8,
			EAngelscriptCachedPrimitiveType::Int32,
			EAngelscriptCachedPrimitiveType::Int16,
		};
		const uint32 Offsets[] = {0, 4, 8};
		const uint32 Sizes[] = {1, 4, 2};
		const uint32 Alignments[] = {1, 4, 2};
		for (uint32 Index = 0; Index < 3; ++Index)
		{
			FAngelscriptCachedPropertySchema Property;
			Property.LayoutOrdinal = Index;
			Property.SemanticByteOffset = Offsets[Index];
			Property.PropertyKey = FAngelscriptStablePropertyKey{
				MakeHash(static_cast<uint8>(0x20 + Index))};
			Property.CanonicalName = FString::Printf(TEXT("Field%u"), Index);
			Property.Type = MakePrimitive(PrimitiveKinds[Index]);
			Property.StorageKind = EAngelscriptCachedPropertyStorageKind::InlineValue;
			Property.SemanticStorageSize = Sizes[Index];
			Property.SemanticStorageAlignment = Alignments[Index];
			Property.Access = EAngelscriptCachedMemberAccess::Public;
			Property.ReplicationCondition = EAngelscriptCachedReplicationCondition::None;
			Property.Metadata.Add({TEXT("Ordinal"), FString::FromInt(Index)});
			Schema.OrderedProperties.Add(MoveTemp(Property));
		}
		Schema.Layout.SemanticAlignment = 8;
		Schema.Layout.SemanticSize = 16;
		Schema.Layout.BasePropertyBoundary = 0;
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static FAngelscriptCachedTypeSchema MakeRecursivePropertyTypeSchema()
	{
		FAngelscriptCachedTypeSchema Schema = MakeThreePropertyLayoutSchema();
		FAngelscriptCachedPropertySchema& Property = Schema.OrderedProperties[1];
		FAngelscriptCachedDataType NestedEnvironment;
		NestedEnvironment.Kind = EAngelscriptCachedDataTypeKind::EnvironmentType;
		NestedEnvironment.TypeReference = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0x90, 0x91);
		NestedEnvironment.OrderedSubTypes.Add(
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int16));
		FAngelscriptCachedDataType RootEnvironment;
		RootEnvironment.Kind = EAngelscriptCachedDataTypeKind::EnvironmentType;
		RootEnvironment.TypeReference = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0x92, 0x93);
		RootEnvironment.OrderedSubTypes.Add(MoveTemp(NestedEnvironment));
		RootEnvironment.OrderedSubTypes.Add(
			MakePrimitive(EAngelscriptCachedPrimitiveType::UInt8));
		Property.Type = MoveTemp(RootEnvironment);
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::ValueLayout,
			MakeReference(EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0x90, 0x91)));
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::ValueLayout,
			MakeReference(EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0x92, 0x93)));
		Schema.Dependencies.Sort([](const auto& A, const auto& B)
		{
			const uint8 AKind = static_cast<uint8>(A.Kind);
			const uint8 BKind = static_cast<uint8>(B.Kind);
			return AKind != BKind ? AKind < BKind
				: A.Target.StableKey < B.Target.StableKey;
		});
		FinalizeValidFixtureHashes(Schema);
		return Schema;
	}

	static bool IsRequiredTypeSchemaDependencyKindForTests(
		const EAngelscriptCacheSemanticDependencyKind Kind)
	{
		return Kind == EAngelscriptCacheSemanticDependencyKind::Declaration
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Signature
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Inheritance
			|| Kind == EAngelscriptCacheSemanticDependencyKind::ValueLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
	}

	static FAngelscriptCachedTypeSchema MakeTypeSchemaAllocationFixture(
		const uint8 TsScrFamily,
		const uint32 Cardinality,
		const uint32 Variant,
		bool& bExpectedLocalSuccess)
	{
		bExpectedLocalSuccess = true;
		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Struct);
		const auto AddMetadata = [](TArray<FAngelscriptCachedMetadataEntry>& Metadata,
			const uint32 Count,
			const TCHAR* Prefix)
		{
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				const FString KeyPadding = FString::ChrN(Count, TEXT('K'));
				const FString ValuePadding = FString::ChrN(Count, TEXT('V'));
				Metadata.Add({
					FString::Printf(TEXT("%sKey%04u%s"), Prefix, Index, *KeyPadding),
					FString::Printf(TEXT("%sValue%04u%s"), Prefix, Index, *ValuePadding),
				});
			}
		};
		const auto AddProperties = [&](const uint32 Count, const uint32 MetadataCount)
		{
			Schema.OrderedProperties.Reset();
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedPropertySchema Property =
					MakeCompleteDelegateSchema().OrderedProperties[0];
				Property.LayoutOrdinal = Index;
				Property.SemanticByteOffset = Index * 4;
				Property.PropertyKey = FAngelscriptStablePropertyKey{
					MakeHash(static_cast<uint8>(0x20 + Index))};
				Property.CanonicalName = FString::Printf(TEXT("Field%04u"), Index);
				Property.Metadata.Reset();
				AddMetadata(Property.Metadata, MetadataCount,
					*FString::Printf(TEXT("P%04u"), Index));
				Schema.OrderedProperties.Add(MoveTemp(Property));
			}
			Schema.Layout.SemanticAlignment = 8;
			Schema.Layout.SemanticSize = Align(uint64(Count) * 4, uint64(8));
		};
		const auto AddMethods = [&](const uint32 Count)
		{
			Schema.OrderedMethods.Reset();
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FAngelscriptCachedMethodEntry Method;
				Method.EntryKind = EAngelscriptCachedMethodSlotKind::LocalMethod;
				Method.MethodOrdinal = Index;
				Method.FunctionKey = FAngelscriptStableFunctionKey{
					MakeHash(static_cast<uint8>(0x40 + Index))};
				Method.DeclaringOwner = Schema.TypeKey;
				Method.ExpectedDeclarationAbi = MakeHash(
					static_cast<uint8>(0x80 + Index));
				Schema.OrderedMethods.Add(Method);
				Schema.Dependencies.AddUnique(MakeDependency(
					EAngelscriptCacheSemanticDependencyKind::Declaration,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
						static_cast<uint8>(0x40 + Index),
						static_cast<uint8>(0x80 + Index))));
			}
		};

		switch (TsScrFamily)
		{
		case 1:
			if (Variant == 0 || Variant == 4)
			{
				Schema.CanonicalNamespace = FString(TEXT("N"))
					+ FString::ChrN(Cardinality, TEXT('N'));
			}
			if (Variant == 1 || Variant == 4)
			{
				Schema.CanonicalName = FString(TEXT("T"))
					+ FString::ChrN(Cardinality, TEXT('T'));
			}
			if (Variant == 2 || Variant == 4)
			{
				Schema.CanonicalDeclaration = FString(TEXT("D"))
					+ FString::ChrN(Cardinality, TEXT('D'));
			}
			if (Variant == 3 || Variant == 4)
			{
				AddMetadata(Schema.Metadata, Cardinality, TEXT("Top"));
			}
			break;
		case 2:
		{
			Schema = Variant == 1 || Variant == 2
				? MakeOrdinaryUClassSchema(false)
				: MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
			const EAngelscriptCachedTypeRelationKind RelationKind =
				static_cast<EAngelscriptCachedTypeRelationKind>(Variant + 1);
			Schema.Relations.RemoveAll([RelationKind](const auto& Relation)
			{
				return Relation.RelationKind == RelationKind;
			});
			for (uint32 Index = 0; Index < Cardinality; ++Index)
			{
				FAngelscriptCachedTypeRelation Relation;
				Relation.RelationKind = RelationKind;
				if (RelationKind == EAngelscriptCachedTypeRelationKind::ImplementedInterface)
				{
					Relation.SemanticOrdinal = Index;
				}
				const bool bEnvironment = RelationKind
					== EAngelscriptCachedTypeRelationKind::ShadowSuper
					|| RelationKind == EAngelscriptCachedTypeRelationKind::CodeSuper;
				Relation.Target = MakeReference(bEnvironment
					? EAngelscriptCacheReferenceKind::EnvironmentSymbol
					: EAngelscriptCacheReferenceKind::ScriptType,
					static_cast<uint8>(0x20 + Index), static_cast<uint8>(0x80 + Index));
				if (Index == 0 && (Variant == 1 || Variant == 2))
				{
					const EAngelscriptCachedTypeRelationKind PeerKind = Variant == 1
						? EAngelscriptCachedTypeRelationKind::CodeSuper
						: EAngelscriptCachedTypeRelationKind::ShadowSuper;
					for (const FAngelscriptCachedTypeRelation& Peer : Schema.Relations)
					{
						if (Peer.RelationKind == PeerKind)
						{
							Relation.Target = Peer.Target;
							break;
						}
					}
				}
				Schema.Relations.Add(Relation);
				Schema.Dependencies.AddUnique(MakeDependency(
					bEnvironment
						? EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi
						: EAngelscriptCacheSemanticDependencyKind::Inheritance,
					Relation.Target));
				if (Variant == 0)
				{
					FAngelscriptCachedTypeLayoutInput Input;
					Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
					Input.Target = Relation.Target;
					Input.BoundaryContribution = 0;
					Input.AlignmentContribution = 8;
					Schema.LayoutInputs.Add(Input);
				}
				if (Variant == 2 && Index == 0)
				{
					for (FAngelscriptCachedTypeLayoutInput& Input : Schema.LayoutInputs)
					{
						if (Input.InputKind
							== EAngelscriptCachedTypeLayoutInputKind::CodeRoot)
						{
							Input.Target = Relation.Target;
						}
					}
				}
			}
			bExpectedLocalSuccess =
				(Variant == 0 && Cardinality <= 1)
				|| ((Variant == 1 || Variant == 2) && Cardinality == 1)
				|| (Variant == 3)
				|| (Variant == 4 && Cardinality == 0);
			break;
		}
		case 3:
			switch (Variant)
			{
			case 0:
				Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
				break;
			case 1:
				Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
				{
					FAngelscriptCachedTypeRelation Base;
					Base.RelationKind = EAngelscriptCachedTypeRelationKind::Base;
					Base.Target = MakeReference(
						EAngelscriptCacheReferenceKind::ScriptType, 0x20, 0x21);
					Schema.Relations.Add(Base);
					FAngelscriptCachedTypeLayoutInput Input;
					Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
					Input.Target = Base.Target;
					Input.BoundaryContribution = 0;
					Input.AlignmentContribution = 8;
					Schema.LayoutInputs.Add(Input);
					Schema.Dependencies.Add(MakeDependency(
						EAngelscriptCacheSemanticDependencyKind::Inheritance, Base.Target));
				}
				break;
			case 2:
				Schema = MakeOrdinaryUClassSchema(false);
				break;
			case 3:
				Schema = MakeReflectedUStructSchema();
				break;
			case 4:
				Schema = MakeOrdinaryUClassSchema(true);
				break;
			default:
				checkNoEntry();
			}
			if (Variant != 0 && Cardinality != 1)
			{
				const TArray<FAngelscriptCachedTypeLayoutInput> SeedInputs =
					Schema.LayoutInputs;
				Schema.LayoutInputs.Reset();
				for (uint32 Index = 0; Index < Cardinality; ++Index)
				{
					for (FAngelscriptCachedTypeLayoutInput Input : SeedInputs)
					{
						Input.Target.StableKey = MakeHash(
							static_cast<uint8>(0x20 + Index));
						Schema.LayoutInputs.Add(MoveTemp(Input));
					}
				}
				bExpectedLocalSuccess = Cardinality <= 1;
			}
			break;
		case 4:
			AddProperties(Cardinality, 0);
			break;
		case 5:
			AddProperties(Variant == 5 ? FMath::Max(uint32(1), Cardinality) : 1,
				Variant >= 3 ? Cardinality : 0);
			if (Variant == 0 || Variant == 5)
			{
				Schema.OrderedProperties[0].CanonicalName = FString(TEXT("P"))
					+ FString::ChrN(Cardinality, TEXT('P'));
			}
			if (Variant == 1)
			{
				FAngelscriptCachedDataType Nested =
					MakePrimitive(EAngelscriptCachedPrimitiveType::Int32);
				for (uint32 Depth = 0; Depth < Cardinality; ++Depth)
				{
					const FAngelscriptCacheStableReference Target = MakeReference(
						EAngelscriptCacheReferenceKind::EnvironmentSymbol,
						static_cast<uint8>(0x30 + Depth),
						static_cast<uint8>(0x70 + Depth));
					FAngelscriptCachedDataType Parent;
					Parent.Kind = EAngelscriptCachedDataTypeKind::EnvironmentType;
					Parent.TypeReference = Target;
					Parent.OrderedSubTypes.Add(MoveTemp(Nested));
					Nested = MoveTemp(Parent);
					Schema.Dependencies.AddUnique(MakeDependency(
						EAngelscriptCacheSemanticDependencyKind::ValueLayout, Target));
				}
				Schema.OrderedProperties[0].Type = MoveTemp(Nested);
			}
			if (Variant == 2 || Variant == 5)
			{
				FAngelscriptCachedDataType Wide;
				Wide.Kind = EAngelscriptCachedDataTypeKind::EnvironmentType;
				Wide.TypeReference = MakeReference(
					EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0x30, 0x70);
				Schema.Dependencies.AddUnique(MakeDependency(
					EAngelscriptCacheSemanticDependencyKind::ValueLayout,
					Wide.TypeReference.GetValue()));
				for (uint32 Index = 0; Index < Cardinality; ++Index)
				{
					Wide.OrderedSubTypes.Add(MakePrimitive(
						EAngelscriptCachedPrimitiveType::Int32));
				}
				Schema.OrderedProperties[0].Type = MoveTemp(Wide);
			}
			if (Variant == 3 && Cardinality > 2)
			{
				Swap(Schema.OrderedProperties[0].Metadata[0],
					Schema.OrderedProperties[0].Metadata[Cardinality / 2]);
				Schema.OrderedProperties[0].Metadata.Sort([](const auto& A, const auto& B)
				{
					return A.CanonicalKey < B.CanonicalKey;
				});
			}
			break;
		case 6:
			AddMethods(Cardinality);
			break;
		case 7:
			Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
			for (uint32 Index = 0; Index < Cardinality; ++Index)
			{
				FAngelscriptCachedVirtualFunctionSlot Slot;
				Slot.SlotKind = EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
				Slot.VftOrdinal = Index;
				Slot.FunctionKey = FAngelscriptStableFunctionKey{
					MakeHash(static_cast<uint8>(0x40 + Index))};
				Slot.DeclaringOwner = Schema.TypeKey;
				Slot.ImplementingOwner = Schema.TypeKey;
				Slot.ExpectedDeclarationAbi = MakeHash(
					static_cast<uint8>(0x80 + Index));
				Schema.VirtualFunctionTable.Add(Slot);
				Schema.Dependencies.AddUnique(MakeDependency(
					EAngelscriptCacheSemanticDependencyKind::Declaration,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
						static_cast<uint8>(0x40 + Index),
						static_cast<uint8>(0x80 + Index))));
			}
			break;
		case 8:
			{
				const EAngelscriptCachedBehaviorKind BehaviorKind =
					static_cast<EAngelscriptCachedBehaviorKind>(Variant / 4 + 1);
				const uint32 Shape = Variant % 4;
				const bool bEnvironment = Shape >= 2;
				const bool bOwnerPresent = Shape == 1 || Shape == 3;
				const bool bClassOnly =
					BehaviorKind == EAngelscriptCachedBehaviorKind::Factory
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::ListFactory
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::AddRef
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::Release
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::GetWeakRefFlag
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::GetRefCount
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::SetGcFlag
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::GetGcFlag
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::EnumRefs
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::ReleaseRefs
					|| BehaviorKind == EAngelscriptCachedBehaviorKind::CopyFactory;
				Schema = MakeMinimalSchema(bClassOnly
					? EAngelscriptCachedTypeKind::Class
					: EAngelscriptCachedTypeKind::Struct);
				Schema.OrderedBehaviorSlots.Reset();
				const auto AddBehavior = [&](const EAngelscriptCachedBehaviorKind Kind,
					const uint32 Ordinal,
					const FAngelscriptCacheStableReference& Target)
				{
					FAngelscriptCachedBehaviorSlot Slot;
					Slot.BehaviorKind = Kind;
					Slot.SlotOrdinal = Ordinal;
					Slot.Target = Target;
					if (bOwnerPresent)
					{
						Slot.DeclaringOwner = Schema.TypeKey;
					}
					Schema.OrderedBehaviorSlots.Add(Slot);
					Schema.Dependencies.AddUnique(MakeDependency(
						bEnvironment
							? EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi
							: EAngelscriptCacheSemanticDependencyKind::Declaration,
						Target));
				};
			for (uint32 Index = 0; Index < Cardinality; ++Index)
			{
				AddBehavior(BehaviorKind, Index, MakeReference(bEnvironment
					? EAngelscriptCacheReferenceKind::EnvironmentSymbol
					: EAngelscriptCacheReferenceKind::ScriptFunction,
					static_cast<uint8>(0x40 + Index), static_cast<uint8>(0x80 + Index)));
			}
				const bool bSingleton = BehaviorKind
					!= EAngelscriptCachedBehaviorKind::Construct
					&& BehaviorKind != EAngelscriptCachedBehaviorKind::Factory;
				const bool bOwnerValid = Cardinality == 0
					|| (bEnvironment ? !bOwnerPresent : bOwnerPresent);
				const bool bTargetValid = !bEnvironment
					? BehaviorKind != EAngelscriptCachedBehaviorKind::TemplateCallback
					: BehaviorKind == EAngelscriptCachedBehaviorKind::Destruct
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::AddRef
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::Release
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::GetWeakRefFlag
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::GetRefCount
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::SetGcFlag
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::GetGcFlag
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::EnumRefs
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::ReleaseRefs
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::Copy
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::CopyConstruct
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::CopyFactory;
				bool bKindValid = BehaviorKind != EAngelscriptCachedBehaviorKind::TemplateCallback;
				if (!bClassOnly)
				{
					bKindValid = BehaviorKind == EAngelscriptCachedBehaviorKind::Construct
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::ListConstruct
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::Destruct
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::Copy
						|| BehaviorKind == EAngelscriptCachedBehaviorKind::CopyConstruct;
				}
				bExpectedLocalSuccess = Cardinality == 0
					|| (bKindValid && bTargetValid && bOwnerValid
						&& (!bSingleton || Cardinality <= 1));
				if (bExpectedLocalSuccess && Cardinality == 1
					&& BehaviorKind == EAngelscriptCachedBehaviorKind::Destruct)
				{
					Schema.TypeSemanticFlags |= static_cast<uint32>(
						EAngelscriptCachedTypeSemanticFlags::HasDestructor);
				}
				if (bExpectedLocalSuccess && Cardinality > 0 && !bEnvironment
					&& BehaviorKind == EAngelscriptCachedBehaviorKind::Factory)
				{
					for (uint32 Index = 0; Index < Cardinality; ++Index)
					{
						AddBehavior(EAngelscriptCachedBehaviorKind::Construct, Index,
							MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
								static_cast<uint8>(0x60 + Index),
								static_cast<uint8>(0xa0 + Index)));
					}
				}
				if (bExpectedLocalSuccess && Cardinality == 1 && !bEnvironment
					&& BehaviorKind == EAngelscriptCachedBehaviorKind::CopyConstruct)
				{
					AddBehavior(EAngelscriptCachedBehaviorKind::Construct, 0,
						Schema.OrderedBehaviorSlots[0].Target);
				}
				if (bExpectedLocalSuccess && Cardinality == 1 && !bEnvironment
					&& BehaviorKind == EAngelscriptCachedBehaviorKind::CopyFactory)
				{
					const FAngelscriptCacheStableReference AliasTarget =
						Schema.OrderedBehaviorSlots[0].Target;
					AddBehavior(EAngelscriptCachedBehaviorKind::Construct, 0, AliasTarget);
					AddBehavior(EAngelscriptCachedBehaviorKind::Factory, 0, AliasTarget);
				}
				Schema.OrderedBehaviorSlots.Sort([](const auto& A, const auto& B)
				{
					const uint8 AKind = static_cast<uint8>(A.BehaviorKind);
					const uint8 BKind = static_cast<uint8>(B.BehaviorKind);
					return AKind != BKind ? AKind < BKind : A.SlotOrdinal < B.SlotOrdinal;
				});
			}
			break;
		case 9:
			if (Variant == 0)
			{
				Schema = MakeEnumSchema();
				Schema.KindPayload.Enum->OrderedEnumerators.Reset();
				for (uint32 Index = 0; Index < Cardinality; ++Index)
				{
					FAngelscriptCachedEnumEnumerator Enumerator;
					Enumerator.DeclarationOrdinal = Index;
					Enumerator.CanonicalName = FString::Printf(TEXT("Value%04u"), Index);
					Enumerator.Value = int32(Index);
					AddMetadata(Enumerator.Metadata, Cardinality,
						*FString::Printf(TEXT("Enum%04u"), Index));
					Schema.KindPayload.Enum->OrderedEnumerators.Add(MoveTemp(Enumerator));
				}
			}
			else if (Variant == 1)
			{
				Schema = MakeCompleteDelegateSchema();
			}
			else if (Variant == 2)
			{
				Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
			}
			else
			{
				Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
				// Cardinality intentionally does not manufacture an illegal subtype.
				// A V1 Typedef aliases exactly one unqualified, non-void primitive;
				// the hostile non-zero subtype storage has its own physical-only row.
				Schema.KindPayload.Typedef->AliasedType = MakePrimitive(
					EAngelscriptCachedPrimitiveType::Int32);
			}
			break;
		case 10:
			if (Variant == 4)
			{
				Schema = MakeStaticsUClassSchema(Cardinality);
				bExpectedLocalSuccess = Cardinality > 0;
			}
			else
			{
				Schema = MakeOrdinaryUClassSchema(false);
				Schema.Reflection.OrderedUFunctionMembers.Reset();
				Schema.Dependencies.RemoveAll([](const auto& Dependency)
				{
					return Dependency.Kind
						== EAngelscriptCacheSemanticDependencyKind::Declaration;
				});
				if (Variant == 1 || Variant == 5)
				{
					Schema.Reflection.ConfigName = FString::ChrN(Cardinality, TEXT('C'));
				}
				if (Variant == 2 || Variant == 5)
				{
					Schema.Reflection.StaticClassGlobalName =
						FString::ChrN(Cardinality, TEXT('S'));
				}
				if (Variant == 3 || Variant == 5 || Variant >= 6)
				{
					for (uint32 Index = 0; Index < Cardinality; ++Index)
					{
						const FAngelscriptCachedReflectedFunctionMember Member =
							MakeReflectedMember(Index,
								static_cast<uint8>(0x40 + Index),
								static_cast<uint8>(0x80 + Index));
						Schema.Reflection.OrderedUFunctionMembers.Add(Member);
						Schema.Dependencies.Add(MakeDependency(
							EAngelscriptCacheSemanticDependencyKind::Declaration,
							Member.Target));
					}
				}
				if (Cardinality == 0 && Variant != 0 && Variant != 3)
				{
					bExpectedLocalSuccess = false;
				}
			}
			break;
		case 11:
			{
				const EAngelscriptCacheSemanticDependencyKind DependencyKind =
					static_cast<EAngelscriptCacheSemanticDependencyKind>(Variant + 1);
				const EAngelscriptCacheReferenceKind TargetKinds[] = {
					EAngelscriptCacheReferenceKind::Invalid,
					EAngelscriptCacheReferenceKind::ScriptImport,
					EAngelscriptCacheReferenceKind::ScriptFunction,
					EAngelscriptCacheReferenceKind::ScriptFunction,
					EAngelscriptCacheReferenceKind::ScriptType,
					EAngelscriptCacheReferenceKind::ScriptType,
					EAngelscriptCacheReferenceKind::ScriptProperty,
					EAngelscriptCacheReferenceKind::ScriptGlobal,
					EAngelscriptCacheReferenceKind::EnvironmentSymbol,
					EAngelscriptCacheReferenceKind::ScriptFunction,
					EAngelscriptCacheReferenceKind::EnvironmentSymbol,
					EAngelscriptCacheReferenceKind::EnvironmentSymbol,
				};
				if (DependencyKind == EAngelscriptCacheSemanticDependencyKind::Declaration)
				{
					AddMethods(Cardinality);
				}
				else if (DependencyKind == EAngelscriptCacheSemanticDependencyKind::Signature)
				{
					Schema = Cardinality == 0
						? MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct)
						: MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
					bExpectedLocalSuccess = Cardinality <= 1;
				}
				else if (DependencyKind == EAngelscriptCacheSemanticDependencyKind::Inheritance)
				{
					Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
					for (uint32 Index = 0; Index < Cardinality; ++Index)
					{
						FAngelscriptCachedTypeRelation Relation;
						Relation.RelationKind =
							EAngelscriptCachedTypeRelationKind::ImplementedInterface;
						Relation.SemanticOrdinal = Index;
						// Keep the high-cardinality allocation fixture disjoint from
						// the owning Class TypeKey (0x51). The former 0x40 range
						// reached 0x51 at index 17 and accidentally modeled
						// self-inheritance instead of allocator slack.
						Relation.Target = MakeReference(
							EAngelscriptCacheReferenceKind::ScriptType,
							static_cast<uint8>(0x80 + Index),
							static_cast<uint8>(0xa0 + Index));
						Schema.Relations.Add(Relation);
						Schema.Dependencies.Add(MakeDependency(
							DependencyKind, Relation.Target));
					}
				}
				else if (DependencyKind == EAngelscriptCacheSemanticDependencyKind::ValueLayout)
				{
					AddProperties(Cardinality, 0);
					for (uint32 Index = 0; Index < Cardinality; ++Index)
					{
						const FAngelscriptCacheStableReference Target = MakeReference(
							EAngelscriptCacheReferenceKind::ScriptType,
							static_cast<uint8>(0x40 + Index),
							static_cast<uint8>(0x80 + Index));
						Schema.OrderedProperties[Index].Type = MakeScriptType(
							static_cast<uint8>(0x40 + Index),
							static_cast<uint8>(0x80 + Index));
						Schema.Dependencies.Add(MakeDependency(DependencyKind, Target));
					}
				}
				else if (DependencyKind
					== EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi)
				{
					Schema = Cardinality == 0
						? MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct)
						: MakeReflectedUStructSchema();
					bExpectedLocalSuccess = Cardinality <= 1;
				}
				else
				{
					for (uint32 Index = 0; Index < Cardinality; ++Index)
					{
						FAngelscriptCacheSemanticDependency Dependency =
							MakeDependency(DependencyKind,
								MakeReference(TargetKinds[Variant + 1],
									static_cast<uint8>(0x40 + Index),
									static_cast<uint8>(0x80 + Index)));
						if (DependencyKind
								== EAngelscriptCacheSemanticDependencyKind::GlobalStorage
							|| DependencyKind == EAngelscriptCacheSemanticDependencyKind::HardValue
							|| DependencyKind
								== EAngelscriptCacheSemanticDependencyKind::Initializer
							|| DependencyKind
								== EAngelscriptCacheSemanticDependencyKind::CompileOption)
						{
							Dependency.ExpectedContentOrValue = MakeHash(
								static_cast<uint8>(0xc0 + Index));
						}
						Schema.Dependencies.Add(MoveTemp(Dependency));
					}
					// These are wire-valid dependency records, but they are not
					// locally derived authorities of a TypeSchema record.
					bExpectedLocalSuccess = false;
				}
			}
			break;
		case 12:
			if (Variant == 0)
			{
				AddMetadata(Schema.Metadata, Cardinality, TEXT("Scratch"));
			}
			else if (Variant == 1)
			{
				Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
				for (uint32 Index = 0; Index < Cardinality; ++Index)
				{
					FAngelscriptCachedTypeRelation Relation;
					Relation.RelationKind =
						EAngelscriptCachedTypeRelationKind::ImplementedInterface;
					Relation.SemanticOrdinal = Index;
					Relation.Target = MakeReference(
						EAngelscriptCacheReferenceKind::ScriptType,
						static_cast<uint8>(0x40 + Index),
						static_cast<uint8>(0x80 + Index));
					Schema.Relations.Add(Relation);
					Schema.Dependencies.Add(MakeDependency(
						EAngelscriptCacheSemanticDependencyKind::Inheritance,
						Relation.Target));
				}
			}
			else if (Variant == 2)
			{
				Schema = MakeOrdinaryUClassSchema(true);
			}
			else if (Variant == 3)
			{
				AddMethods(Cardinality);
			}
			else if (Variant == 4)
			{
				Schema = MakeEnumSchema();
				Schema.KindPayload.Enum->OrderedEnumerators.Reset();
				for (uint32 Index = 0; Index < Cardinality; ++Index)
				{
					FAngelscriptCachedEnumEnumerator Enumerator;
					Enumerator.DeclarationOrdinal = Index;
					Enumerator.CanonicalName = FString::Printf(TEXT("E%04u"), Index);
					Enumerator.Value = int32(Index);
					AddMetadata(Enumerator.Metadata, Cardinality, TEXT("Enum"));
					Schema.KindPayload.Enum->OrderedEnumerators.Add(MoveTemp(Enumerator));
				}
			}
			else
			{
				AddProperties(Cardinality, Cardinality);
				AddMetadata(Schema.Metadata, Cardinality, TEXT("Combined"));
				AddMethods(Cardinality);
			}
			break;
		case 13:
			AddProperties(Cardinality, 0);
			break;
		case 14:
			Schema = Variant == 1
				? MakeMinimalSchema(EAngelscriptCachedTypeKind::Class)
				: Variant == 3 || Variant == 4
					? MakeOrdinaryUClassSchema(false)
					: MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
			if (Variant == 3 || Variant == 4)
			{
				Schema.Dependencies.RemoveAll([](const auto& Dependency)
				{
					return Dependency.Kind
						== EAngelscriptCacheSemanticDependencyKind::Declaration;
				});
			}
			if (Variant == 0 || Variant == 4)
			{
				AddMethods(Cardinality);
			}
			if (Variant == 1 || Variant == 4)
			{
				for (uint32 Index = 0; Index < Cardinality; ++Index)
				{
					FAngelscriptCachedVirtualFunctionSlot Slot;
					Slot.SlotKind = EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
					Slot.VftOrdinal = Index;
					Slot.FunctionKey = FAngelscriptStableFunctionKey{
						MakeHash(static_cast<uint8>(0x40 + Index))};
					Slot.DeclaringOwner = Schema.TypeKey;
					Slot.ImplementingOwner = Schema.TypeKey;
					Slot.ExpectedDeclarationAbi = MakeHash(
						static_cast<uint8>(0x80 + Index));
					Schema.VirtualFunctionTable.Add(Slot);
					Schema.Dependencies.AddUnique(MakeDependency(
						EAngelscriptCacheSemanticDependencyKind::Declaration,
						MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
							static_cast<uint8>(0x40 + Index),
							static_cast<uint8>(0x80 + Index))));
				}
			}
			if (Variant == 2 || Variant == 4)
			{
				for (uint32 Index = 0; Index < Cardinality; ++Index)
				{
					FAngelscriptCachedBehaviorSlot Slot;
					Slot.BehaviorKind = EAngelscriptCachedBehaviorKind::Construct;
					Slot.SlotOrdinal = Index;
					Slot.Target = MakeReference(
						EAngelscriptCacheReferenceKind::ScriptFunction,
						static_cast<uint8>(0x50 + Index),
						static_cast<uint8>(0x90 + Index));
					Slot.DeclaringOwner = Schema.TypeKey;
					Schema.OrderedBehaviorSlots.Add(Slot);
					Schema.Dependencies.AddUnique(MakeDependency(
						EAngelscriptCacheSemanticDependencyKind::Declaration,
						Slot.Target));
					if (Variant == 4)
					{
						FAngelscriptCachedBehaviorSlot Factory = Slot;
						Factory.BehaviorKind = EAngelscriptCachedBehaviorKind::Factory;
						Factory.Target = MakeReference(
							EAngelscriptCacheReferenceKind::ScriptFunction,
							static_cast<uint8>(0x70 + Index),
							static_cast<uint8>(0xb0 + Index));
						Schema.OrderedBehaviorSlots.Add(Factory);
						Schema.Dependencies.AddUnique(MakeDependency(
							EAngelscriptCacheSemanticDependencyKind::Declaration,
							Factory.Target));
					}
				}
			}
			if (Variant == 3 || Variant == 4)
			{
				Schema.Reflection.OrderedUFunctionMembers.Reset();
				for (uint32 Index = 0; Index < Cardinality; ++Index)
				{
					const FAngelscriptCachedReflectedFunctionMember Member =
						MakeReflectedMember(Index,
							static_cast<uint8>(0x60 + Index),
							static_cast<uint8>(0xa0 + Index));
					Schema.Reflection.OrderedUFunctionMembers.Add(Member);
					Schema.Dependencies.AddUnique(MakeDependency(
						EAngelscriptCacheSemanticDependencyKind::Declaration,
						Member.Target));
				}
			}
			break;
		default:
			checkNoEntry();
		}

		Schema.Relations.StableSort([](const auto& A, const auto& B)
		{
			return static_cast<uint8>(A.RelationKind)
				< static_cast<uint8>(B.RelationKind);
		});
		Schema.LayoutInputs.StableSort([](const auto& A, const auto& B)
		{
			return static_cast<uint8>(A.InputKind)
				< static_cast<uint8>(B.InputKind);
		});
		Schema.OrderedBehaviorSlots.StableSort([](const auto& A, const auto& B)
		{
			const uint8 AKind = static_cast<uint8>(A.BehaviorKind);
			const uint8 BKind = static_cast<uint8>(B.BehaviorKind);
			return AKind != BKind ? AKind < BKind : A.SlotOrdinal < B.SlotOrdinal;
		});
		Schema.Dependencies.Sort([](const auto& A, const auto& B)
		{
			const uint8 AKind = static_cast<uint8>(A.Kind);
			const uint8 BKind = static_cast<uint8>(B.Kind);
			return AKind != BKind ? AKind < BKind
				: A.Target.StableKey < B.Target.StableKey;
		});
		if (bExpectedLocalSuccess)
		{
			FinalizeValidFixtureHashes(Schema);
		}
		return Schema;
	}

	static uint32 GetTypeSchemaAllocationVariantCount(const uint8 TsScrFamily)
	{
		const uint32 Counts[] = {
			0,
			5,  // TS-SCR-01: three strings, metadata, combined
			5,  // TS-SCR-02: all five relation kinds
			5,  // TS-SCR-03: empty, three roles, Base+Code combination
			1,  // TS-SCR-04: property top-level array
			6,  // TS-SCR-05: name, depth, width, metadata positions, combined
			1,  // TS-SCR-06: methods
			1,  // TS-SCR-07: VFT
			68, // TS-SCR-08: 17 groups x four target/owner-presence shapes
			4,  // TS-SCR-09: Enum metadata, Delegate, Funcdef, Typedef subtype storage
			9,  // TS-SCR-10: forms plus three reflected-name allocation fixtures
			11, // TS-SCR-11: every dependency kind
			6,  // TS-SCR-12: actual canonical indexes plus combined
			1,  // TS-SCR-13: property replay scratch
			5,  // TS-SCR-14: method/VFT/behavior/UFunction/combined
		};
		check(TsScrFamily > 0 && TsScrFamily < UE_ARRAY_COUNT(Counts));
		return Counts[TsScrFamily];
	}

	struct FIndependentTypeSchemaWireSpanForTests
	{
		EAngelscriptCacheTypeSchemaTestField Field;
		int32 PrimaryIndex = INDEX_NONE;
		int32 SecondaryIndex = INDEX_NONE;
		int32 TertiaryIndex = INDEX_NONE;
		uint64 ExactOffset = 0;
		uint64 ExactSize = 0;
	};

	struct FIndependentTypeSchemaWireBoundaryForTests
	{
		uint64 ExactOffset = 0;
		uint64 ExactSize = 0;
	};

	struct FIndependentTypeSchemaCapturedOffsetForTests
	{
		FAngelscriptTypeSchemaFieldCoordinate Coordinate;
		uint64 ExactOffset = 0;
	};

	struct FIndependentTypeSchemaWireInventoryForTests
	{
		bool bComplete = false;
		uint64 ConsumedBytes = 0;
		TArray<FIndependentTypeSchemaWireSpanForTests> WireSpans;
		TArray<FIndependentTypeSchemaWireBoundaryForTests> PhysicalBoundaries;
		TArray<FIndependentTypeSchemaCapturedOffsetForTests> CapturedOffsets;
		TArray<uint64> StableReferenceOffsets;

		TOptional<uint64> FindCapturedOffset(
			const FAngelscriptTypeSchemaFieldCoordinate& Coordinate) const
		{
			for (const FIndependentTypeSchemaCapturedOffsetForTests& Entry : CapturedOffsets)
			{
				if (Entry.Coordinate.Field == Coordinate.Field
					&& Entry.Coordinate.PrimaryIndex == Coordinate.PrimaryIndex
					&& Entry.Coordinate.SecondaryIndex == Coordinate.SecondaryIndex
					&& Entry.Coordinate.TertiaryIndex == Coordinate.TertiaryIndex)
				{
					return Entry.ExactOffset;
				}
			}
			return {};
		}
	};

	class FIndependentTypeSchemaWireScannerForTests
	{
	public:
		explicit FIndependentTypeSchemaWireScannerForTests(
			const TConstArrayView<uint8> InPayload)
			: Payload(InPayload)
		{
		}

		FIndependentTypeSchemaWireInventoryForTests Scan()
		{
			Capture(EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion);
			ReadU32(EAngelscriptCacheTypeSchemaTestField::PayloadSchemaVersion);
			Capture(EAngelscriptTypeSchemaCapturedField::ModuleKey);
			ReadHash(EAngelscriptCacheTypeSchemaTestField::ModuleKey);
			Capture(EAngelscriptTypeSchemaCapturedField::TypeKey);
			ReadHash(EAngelscriptCacheTypeSchemaTestField::TypeKey);
			Capture(EAngelscriptTypeSchemaCapturedField::TypeKind);
			const uint8 TypeKind = ReadU8(EAngelscriptCacheTypeSchemaTestField::TypeKind);
			ReadCapturedString(EAngelscriptTypeSchemaCapturedField::CanonicalNamespace,
				EAngelscriptCacheTypeSchemaTestField::CanonicalNamespace);
			ReadCapturedString(EAngelscriptTypeSchemaCapturedField::CanonicalName,
				EAngelscriptCacheTypeSchemaTestField::CanonicalNameBytes);
			ReadCapturedString(EAngelscriptTypeSchemaCapturedField::CanonicalDeclaration,
				EAngelscriptCacheTypeSchemaTestField::CanonicalDeclaration);
			Capture(EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags);
			ReadU32(EAngelscriptCacheTypeSchemaTestField::TypeSemanticFlags);
			ReadTopLevelMetadata();
			ReadRelations();
			ReadLayoutInputs();
			Capture(EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
			Mark(EAngelscriptCacheTypeSchemaTestField::Layout);
			ReadU64(EAngelscriptCacheTypeSchemaTestField::LayoutSemanticSize);
			ReadU32(EAngelscriptCacheTypeSchemaTestField::LayoutSemanticAlignment);
			ReadU32(EAngelscriptCacheTypeSchemaTestField::LayoutBasePropertyBoundary);
			ReadHash(EAngelscriptCacheTypeSchemaTestField::TypeLayoutHash);
			ReadProperties();
			ReadMethods();
			ReadVirtualFunctionTable();
			ReadBehaviors();
			ReadKindPayload(TypeKind);
			ReadReflection();
			ReadDependencies();
			Inventory.ConsumedBytes = Offset;
			Inventory.bComplete = bValid && Offset == uint64(Payload.Num());
			return MoveTemp(Inventory);
		}

	private:
		static constexpr uint32 UnusedIndex = MAX_uint32;

		bool CanRead(const uint64 Size) const
		{
			return Size <= uint64(Payload.Num()) && Offset <= uint64(Payload.Num()) - Size;
		}

		uint64 TakeRaw(const uint64 Size)
		{
			const uint64 Start = Offset;
			if (!CanRead(Size))
			{
				bValid = false;
				Offset = uint64(Payload.Num());
				return Start;
			}
			if (Size > 0)
			{
				Inventory.PhysicalBoundaries.Add({Start, Size});
			}
			Offset += Size;
			return Start;
		}

		void AddSpan(const EAngelscriptCacheTypeSchemaTestField Field,
			const uint64 Start, const uint64 Size,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			Inventory.WireSpans.Add({
				Field, Primary, Secondary, Tertiary, Start, Size});
		}

		void Mark(const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			AddSpan(Field, Offset, 0, Primary, Secondary, Tertiary);
		}

		uint8 ReadRawU8()
		{
			const uint64 Start = TakeRaw(sizeof(uint8));
			return bValid ? Payload[static_cast<int32>(Start)] : 0;
		}

		uint32 ReadRawU32()
		{
			const uint64 Start = TakeRaw(sizeof(uint32));
			uint32 Value = 0;
			if (bValid)
			{
				FMemory::Memcpy(&Value, Payload.GetData() + Start, sizeof(Value));
			}
			return Value;
		}

		uint8 ReadU8(const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			const uint64 Start = Offset;
			const uint8 Value = ReadRawU8();
			AddSpan(Field, Start, sizeof(uint8), Primary, Secondary, Tertiary);
			return Value;
		}

		uint32 ReadU32(const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			const uint64 Start = Offset;
			const uint32 Value = ReadRawU32();
			AddSpan(Field, Start, sizeof(uint32), Primary, Secondary, Tertiary);
			return Value;
		}

		void ReadU64(const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 Primary = INDEX_NONE)
		{
			const uint64 Start = TakeRaw(sizeof(uint64));
			AddSpan(Field, Start, sizeof(uint64), Primary);
		}

		void ReadHash(const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			const uint64 Start = TakeRaw(sizeof(FBlake3Hash::ByteArray));
			AddSpan(Field, Start, sizeof(FBlake3Hash::ByteArray),
				Primary, Secondary, Tertiary);
		}

		uint64 ReadString(const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			const uint64 StringStart = Offset;
			const uint32 ByteCount = ReadRawU32();
			const uint64 ByteStart = TakeRaw(ByteCount);
			AddSpan(Field, ByteStart, ByteCount, Primary, Secondary, Tertiary);
			return StringStart;
		}

		void ReadCapturedString(const EAngelscriptTypeSchemaCapturedField CapturedField,
			const EAngelscriptCacheTypeSchemaTestField WireField)
		{
			const uint64 Start = ReadString(WireField);
			CaptureAt(CapturedField, Start);
		}

		void Capture(const EAngelscriptTypeSchemaCapturedField Field,
			const uint32 Primary = UnusedIndex,
			const uint32 Secondary = UnusedIndex,
			const uint32 Tertiary = UnusedIndex)
		{
			CaptureAt(Field, Offset, Primary, Secondary, Tertiary);
		}

		void CaptureAt(const EAngelscriptTypeSchemaCapturedField Field,
			const uint64 ExactOffset,
			const uint32 Primary = UnusedIndex,
			const uint32 Secondary = UnusedIndex,
			const uint32 Tertiary = UnusedIndex)
		{
			Inventory.CapturedOffsets.Add({
				FAngelscriptTypeSchemaFieldCoordinate{Field, Primary, Secondary, Tertiary},
				ExactOffset});
		}

		void ReadStableReference(const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE,
			const int32 Tertiary = INDEX_NONE)
		{
			const uint64 Start = Offset;
			Inventory.StableReferenceOffsets.Add(Start);
			TakeRaw(sizeof(uint8));
			TakeRaw(sizeof(FBlake3Hash::ByteArray));
			TakeRaw(sizeof(FBlake3Hash::ByteArray));
			AddSpan(Field, Start, sizeof(uint8) + 2 * sizeof(FBlake3Hash::ByteArray),
				Primary, Secondary, Tertiary);
		}

		void ReadStableTypeKey(const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE)
		{
			const uint64 Start = TakeRaw(sizeof(FBlake3Hash::ByteArray));
			AddSpan(Field, Start, sizeof(FBlake3Hash::ByteArray), Primary, Secondary);
		}

		void ReadTopLevelMetadata()
		{
			Capture(EAngelscriptTypeSchemaCapturedField::Metadata);
			const uint32 Count = ReadU32(EAngelscriptCacheTypeSchemaTestField::Metadata);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::MetadataEntry, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::MetadataEntry,
					static_cast<int32>(Index));
				ReadString(EAngelscriptCacheTypeSchemaTestField::MetadataEntry,
					static_cast<int32>(Index), INDEX_NONE, 0);
				ReadString(EAngelscriptCacheTypeSchemaTestField::MetadataEntry,
					static_cast<int32>(Index), INDEX_NONE, 1);
			}
		}

		void ReadNestedMetadata(const int32 OwnerIndex,
			const EAngelscriptTypeSchemaCapturedField PublicEntryField,
			const EAngelscriptCacheTypeSchemaTestField ContainerField,
			const EAngelscriptCacheTypeSchemaTestField EntryField)
		{
			// The nested count is intentionally decoder-internal. Public fields 19/32
			// name concrete entries and never alias this count offset.
			const uint32 Count = ReadU32(ContainerField, OwnerIndex);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(PublicEntryField, static_cast<uint32>(OwnerIndex), Index);
				Mark(EntryField, OwnerIndex, static_cast<int32>(Index));
				ReadString(EntryField, OwnerIndex, static_cast<int32>(Index), 0);
				ReadString(EntryField, OwnerIndex, static_cast<int32>(Index), 1);
			}
		}

		// Scanner sections are split only for readability. Every read above is from
		// raw payload bytes; no DTO, writer trace, captured table, or decoder event
		// supplies an expected offset or cardinality.
		void ReadRelations()
		{
			const uint32 Count = ReadU32(EAngelscriptCacheTypeSchemaTestField::Relations);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::Relation, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::Relations,
					static_cast<int32>(Index));
				ReadU8(EAngelscriptCacheTypeSchemaTestField::RelationKind,
					static_cast<int32>(Index));
				const uint8 HasOrdinal = ReadU8(
					EAngelscriptCacheTypeSchemaTestField::RelationSemanticOrdinalOptionalTag,
					static_cast<int32>(Index));
				if (HasOrdinal == 1)
				{
					TakeRaw(sizeof(uint32));
				}
				Capture(EAngelscriptTypeSchemaCapturedField::RelationTarget, Index);
				ReadStableReference(EAngelscriptCacheTypeSchemaTestField::Relations,
					static_cast<int32>(Index), 0);
			}
		}

		void ReadLayoutInputs()
		{
			const uint32 Count = ReadU32(EAngelscriptCacheTypeSchemaTestField::LayoutInputs);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::LayoutInput, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::LayoutInput,
					static_cast<int32>(Index));
				ReadU8(EAngelscriptCacheTypeSchemaTestField::LayoutInputKind,
					static_cast<int32>(Index));
				Capture(EAngelscriptTypeSchemaCapturedField::LayoutInputTarget, Index);
				ReadStableReference(EAngelscriptCacheTypeSchemaTestField::LayoutInput,
					static_cast<int32>(Index), 0);
				const uint8 HasBoundary = ReadU8(
					EAngelscriptCacheTypeSchemaTestField::LayoutInputBoundaryOptionalTag,
					static_cast<int32>(Index));
				if (HasBoundary == 1)
				{
					TakeRaw(sizeof(uint32));
				}
				const uint8 HasAlignment = ReadU8(
					EAngelscriptCacheTypeSchemaTestField::LayoutInputAlignmentOptionalTag,
					static_cast<int32>(Index));
				if (HasAlignment == 1)
				{
					TakeRaw(sizeof(uint32));
				}
				ReadHash(EAngelscriptCacheTypeSchemaTestField::LayoutInput,
					static_cast<int32>(Index), 1);
			}
		}

		void ReadCanonicalDataType(const int32 PropertyIndex, int32& PreOrder,
			const bool bPublicPropertyCoordinate)
		{
			const int32 ThisPreOrder = PreOrder++;
			if (bPublicPropertyCoordinate)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::PropertyType,
					static_cast<uint32>(PropertyIndex), static_cast<uint32>(ThisPreOrder));
			}
			Mark(EAngelscriptCacheTypeSchemaTestField::PropertyType,
				PropertyIndex, ThisPreOrder);
			ReadU8(EAngelscriptCacheTypeSchemaTestField::DataTypeKind,
				PropertyIndex, ThisPreOrder);
			ReadU8(EAngelscriptCacheTypeSchemaTestField::DataTypePrimitive,
				PropertyIndex, ThisPreOrder);
			const uint8 HasReference = ReadU8(
				EAngelscriptCacheTypeSchemaTestField::DataTypeTypeReferenceOptionalTag,
				PropertyIndex, ThisPreOrder);
			if (HasReference == 1)
			{
				ReadStableReference(EAngelscriptCacheTypeSchemaTestField::PropertyType,
					PropertyIndex, ThisPreOrder, 0);
			}
			ReadU32(EAngelscriptCacheTypeSchemaTestField::DataTypeQualifierFlags,
				PropertyIndex, ThisPreOrder);
			const uint32 Count = ReadU32(
				EAngelscriptCacheTypeSchemaTestField::DataTypeOrderedSubTypes,
				PropertyIndex, ThisPreOrder);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				ReadCanonicalDataType(PropertyIndex, PreOrder, bPublicPropertyCoordinate);
			}
		}

		void ReadProperties()
		{
			const uint32 Count = ReadU32(
				EAngelscriptCacheTypeSchemaTestField::OrderedProperties);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::OrderedProperty, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
					static_cast<int32>(Index));
				ReadU32(EAngelscriptCacheTypeSchemaTestField::PropertyLayoutOrdinal,
					static_cast<int32>(Index));
				ReadU32(EAngelscriptCacheTypeSchemaTestField::PropertySemanticByteOffset,
					static_cast<int32>(Index));
				Capture(EAngelscriptTypeSchemaCapturedField::PropertyKey, Index);
				ReadHash(EAngelscriptCacheTypeSchemaTestField::PropertyKey,
					static_cast<int32>(Index));
				ReadString(EAngelscriptCacheTypeSchemaTestField::PropertyCanonicalName,
					static_cast<int32>(Index));
				int32 PreOrder = 0;
				ReadCanonicalDataType(static_cast<int32>(Index), PreOrder, true);
				ReadU8(EAngelscriptCacheTypeSchemaTestField::PropertyStorageKind,
					static_cast<int32>(Index));
				ReadU32(EAngelscriptCacheTypeSchemaTestField::PropertySemanticStorageSize,
					static_cast<int32>(Index));
				ReadU32(EAngelscriptCacheTypeSchemaTestField::PropertySemanticStorageAlignment,
					static_cast<int32>(Index));
				ReadHash(EAngelscriptCacheTypeSchemaTestField::PropertyStorageLayoutHash,
					static_cast<int32>(Index));
				ReadU8(EAngelscriptCacheTypeSchemaTestField::PropertyMemberAccess,
					static_cast<int32>(Index));
				ReadU32(EAngelscriptCacheTypeSchemaTestField::PropertySemanticFlags,
					static_cast<int32>(Index));
				ReadU8(EAngelscriptCacheTypeSchemaTestField::PropertyReplicationCondition,
					static_cast<int32>(Index));
				ReadNestedMetadata(static_cast<int32>(Index),
					EAngelscriptTypeSchemaCapturedField::PropertyMetadata,
					EAngelscriptCacheTypeSchemaTestField::PropertyMetadata,
					EAngelscriptCacheTypeSchemaTestField::PropertyMetadataEntry);
				ReadHash(EAngelscriptCacheTypeSchemaTestField::PropertyLayoutFingerprint,
					static_cast<int32>(Index));
			}
		}

		void ReadMethods()
		{
			const uint32 Count = ReadU32(EAngelscriptCacheTypeSchemaTestField::OrderedMethods);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::OrderedMethod, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::OrderedMethod, static_cast<int32>(Index));
				ReadU8(EAngelscriptCacheTypeSchemaTestField::MethodSlotKind,
					static_cast<int32>(Index));
				ReadU32(EAngelscriptCacheTypeSchemaTestField::MethodOrdinal,
					static_cast<int32>(Index));
				Capture(EAngelscriptTypeSchemaCapturedField::MethodFunction, Index);
				ReadHash(EAngelscriptCacheTypeSchemaTestField::MethodFunctionKey,
					static_cast<int32>(Index));
				Capture(EAngelscriptTypeSchemaCapturedField::MethodDeclaringOwner, Index);
				ReadStableTypeKey(EAngelscriptCacheTypeSchemaTestField::MethodDeclaringOwner,
					static_cast<int32>(Index));
				ReadHash(EAngelscriptCacheTypeSchemaTestField::MethodExpectedDeclarationAbi,
					static_cast<int32>(Index));
			}
		}

		void ReadVirtualFunctionTable()
		{
			const uint32 Count = ReadU32(
				EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable,
					static_cast<int32>(Index));
				ReadU8(EAngelscriptCacheTypeSchemaTestField::MethodSlotKind,
					static_cast<int32>(Index), 1);
				ReadU32(EAngelscriptCacheTypeSchemaTestField::MethodOrdinal,
					static_cast<int32>(Index), 1);
				Capture(EAngelscriptTypeSchemaCapturedField::VirtualFunction, Index);
				ReadHash(EAngelscriptCacheTypeSchemaTestField::MethodFunctionKey,
					static_cast<int32>(Index), 1);
				Capture(EAngelscriptTypeSchemaCapturedField::VirtualDeclaringOwner, Index);
				ReadStableTypeKey(EAngelscriptCacheTypeSchemaTestField::MethodDeclaringOwner,
					static_cast<int32>(Index), 1);
				Capture(EAngelscriptTypeSchemaCapturedField::VirtualImplementingOwner, Index);
				ReadStableTypeKey(EAngelscriptCacheTypeSchemaTestField::MethodDeclaringOwner,
					static_cast<int32>(Index), 2);
				ReadHash(EAngelscriptCacheTypeSchemaTestField::MethodExpectedDeclarationAbi,
					static_cast<int32>(Index), 1);
			}
		}

		void ReadBehaviors()
		{
			const uint32 Count = ReadU32(EAngelscriptCacheTypeSchemaTestField::BehaviorSlots);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::BehaviorSlot, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, static_cast<int32>(Index));
				ReadU8(EAngelscriptCacheTypeSchemaTestField::BehaviorKind,
					static_cast<int32>(Index));
				ReadU32(EAngelscriptCacheTypeSchemaTestField::BehaviorOrdinal,
					static_cast<int32>(Index));
				Capture(EAngelscriptTypeSchemaCapturedField::BehaviorTarget, Index);
				ReadStableReference(EAngelscriptCacheTypeSchemaTestField::BehaviorTarget,
					static_cast<int32>(Index));
				const uint8 HasOwner = ReadU8(
					EAngelscriptCacheTypeSchemaTestField::BehaviorDeclaringOwnerOptionalTag,
					static_cast<int32>(Index));
				if (HasOwner == 1)
				{
					Capture(EAngelscriptTypeSchemaCapturedField::BehaviorDeclaringOwner, Index);
					ReadStableTypeKey(
						EAngelscriptCacheTypeSchemaTestField::BehaviorDeclaringOwnerOptionalTag,
						static_cast<int32>(Index), 1);
				}
			}
		}

		void ReadKindPayload(const uint8 TypeKind)
		{
			Capture(EAngelscriptTypeSchemaCapturedField::KindPayload);
			Mark(EAngelscriptCacheTypeSchemaTestField::KindPayload);
			switch (TypeKind)
			{
			case 1: // Class
			case 2: // Struct
			case 3: // Interface
				break;
			case 4: // Enum
				{
					const uint32 Count = ReadU32(
						EAngelscriptCacheTypeSchemaTestField::KindPayload, 0);
					for (uint32 Index = 0; Index < Count && bValid; ++Index)
					{
						Capture(EAngelscriptTypeSchemaCapturedField::EnumEnumerator, Index);
						Mark(EAngelscriptCacheTypeSchemaTestField::EnumEnumerator,
							static_cast<int32>(Index));
						ReadU32(EAngelscriptCacheTypeSchemaTestField::EnumDeclarationOrdinal,
							static_cast<int32>(Index));
						ReadString(EAngelscriptCacheTypeSchemaTestField::EnumCanonicalName,
							static_cast<int32>(Index));
						ReadU32(EAngelscriptCacheTypeSchemaTestField::EnumSignedValue,
							static_cast<int32>(Index));
						ReadNestedMetadata(static_cast<int32>(Index),
							EAngelscriptTypeSchemaCapturedField::EnumEnumeratorMetadata,
							EAngelscriptCacheTypeSchemaTestField::EnumEnumeratorMetadata,
							EAngelscriptCacheTypeSchemaTestField::EnumEnumeratorMetadataEntry);
					}
					ReadHash(EAngelscriptCacheTypeSchemaTestField::EnumAuthorityHash);
				}
				break;
			case 5: // Delegate
			case 7: // Funcdef
				Capture(EAngelscriptTypeSchemaCapturedField::CallableSignature);
				ReadHash(EAngelscriptCacheTypeSchemaTestField::CallableSignatureFunctionKey);
				ReadHash(EAngelscriptCacheTypeSchemaTestField::CallableExpectedSignatureAbi);
				ReadU8(EAngelscriptCacheTypeSchemaTestField::CallableMulticastBoolean);
				break;
			case 6: // Typedef
				{
					int32 PreOrder = 0;
					ReadCanonicalDataType(INDEX_NONE, PreOrder, false);
				}
				break;
			default:
				// The normal producer rejects an unknown TypeKind before writing.
				// Test-only physical snapshots still need to observe the malformed
				// DTO without inventing a payload arm for that unknown discriminator.
				break;
			}
		}

		void ReadReflection()
		{
			Capture(EAngelscriptTypeSchemaCapturedField::Reflection);
			Mark(EAngelscriptCacheTypeSchemaTestField::Reflection);
			Capture(EAngelscriptTypeSchemaCapturedField::ReflectionKind);
			ReadU8(EAngelscriptCacheTypeSchemaTestField::ReflectionKind);
			Capture(EAngelscriptTypeSchemaCapturedField::ClassReflectionFlags);
			ReadU32(EAngelscriptCacheTypeSchemaTestField::ClassReflectionFlags);
			const uint8 HasConfigName = ReadU8(
				EAngelscriptCacheTypeSchemaTestField::ReflectionConfigNameOptionalTag);
			if (HasConfigName == 1)
			{
				ReadString(EAngelscriptCacheTypeSchemaTestField::ReflectionConfigNameOptionalTag,
					INDEX_NONE, INDEX_NONE, 1);
			}
			const uint8 HasStaticClassName = ReadU8(
				EAngelscriptCacheTypeSchemaTestField::ReflectionStaticClassGlobalNameOptionalTag);
			if (HasStaticClassName == 1)
			{
				ReadString(
					EAngelscriptCacheTypeSchemaTestField::ReflectionStaticClassGlobalNameOptionalTag,
					INDEX_NONE, INDEX_NONE, 1);
			}
			const uint32 Count = ReadU32(
				EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::ReflectedFunctionMember, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers,
					static_cast<int32>(Index));
				TakeRaw(sizeof(uint32));
				Capture(EAngelscriptTypeSchemaCapturedField::ReflectedFunctionName,
					Index);
				ReadString(
					EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionNameBytes,
					static_cast<int32>(Index));
				Capture(
					EAngelscriptTypeSchemaCapturedField::ReflectedOriginalFunctionName,
					Index);
				ReadString(EAngelscriptCacheTypeSchemaTestField::
					ReflectedOriginalFunctionNameBytes,
					static_cast<int32>(Index));
				Capture(
					EAngelscriptTypeSchemaCapturedField::ReflectedScriptFunctionName,
					Index);
				ReadString(EAngelscriptCacheTypeSchemaTestField::
					ReflectedScriptFunctionNameBytes,
					static_cast<int32>(Index));
				Capture(EAngelscriptTypeSchemaCapturedField::ReflectedFunctionTarget, Index);
				ReadStableReference(EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers,
					static_cast<int32>(Index), 0);
			}
		}

		void ReadDependencies()
		{
			const uint32 Count = ReadU32(EAngelscriptCacheTypeSchemaTestField::Dependencies);
			for (uint32 Index = 0; Index < Count && bValid; ++Index)
			{
				Capture(EAngelscriptTypeSchemaCapturedField::Dependency, Index);
				Mark(EAngelscriptCacheTypeSchemaTestField::Dependency, static_cast<int32>(Index));
				ReadU8(EAngelscriptCacheTypeSchemaTestField::Dependency,
					static_cast<int32>(Index), 1);
				Capture(EAngelscriptTypeSchemaCapturedField::DependencyTarget, Index);
				ReadStableReference(EAngelscriptCacheTypeSchemaTestField::Dependency,
					static_cast<int32>(Index), 2);
				const uint8 HasValue = ReadU8(
					EAngelscriptCacheTypeSchemaTestField::DependencyExpectedContentOrValueOptionalTag,
					static_cast<int32>(Index));
				if (HasValue == 1)
				{
					TakeRaw(sizeof(FBlake3Hash::ByteArray));
				}
			}
		}

		TConstArrayView<uint8> Payload;
		uint64 Offset = 0;
		bool bValid = true;
		FIndependentTypeSchemaWireInventoryForTests Inventory;
	};

	static FIndependentTypeSchemaWireInventoryForTests
	ScanIndependentTypeSchemaWireForTests(const TConstArrayView<uint8> Payload)
	{
		return FIndependentTypeSchemaWireScannerForTests(Payload).Scan();
	}

	enum class ETsScrSiteDispositionForTests : uint8
	{
		Required,
		StreamingZero,
		InvalidFixtureOnly,
	};

	enum class ETsScrSiteLifetimeForTests : uint8
	{
		CandidateTemporary,
		StreamingTemporary,
	};

	enum class ETsScrAllocationSiteKindForTests : uint16
	{
		TokenController,
		CanonicalPayloadOwnedBytes,
		FlatHeaderOffsets,
		CanonicalNamespaceString,
		CanonicalNameString,
		CanonicalDeclarationString,
		MetadataArray,
		MetadataKeyString,
		MetadataValueString,
		ParallelMetadataOffsets,
		RelationsArray,
		ParallelRelationOffsets,
		LayoutInputsArray,
		ParallelLayoutInputOffsets,
		PropertiesArray,
		ParallelPropertyOffsets,
		PropertyCanonicalNameString,
		DataTypeOrderedSubTypesArray,
		PropertyMetadataArray,
		PropertyMetadataKeyString,
		PropertyMetadataValueString,
		ParallelNestedPropertyOffsets,
		MethodsArray,
		ParallelMethodOffsets,
		VftArray,
		ParallelVftOffsets,
		BehaviorSlotsArray,
		ParallelBehaviorOffsets,
		EnumEnumeratorsArray,
		EnumNameString,
		EnumMetadataArray,
		EnumMetadataKeyString,
		EnumMetadataValueString,
		TypedefDataTypeOrderedSubTypesArray,
		FlatSelectedArmOffsets,
		ReflectionConfigNameString,
		ReflectionStaticClassGlobalNameString,
		UFunctionMembersArray,
		ReflectedFunctionNameString,
		ReflectedOriginalFunctionNameString,
		ReflectedScriptFunctionNameString,
		ParallelReflectionOffsets,
		DependenciesArray,
		ParallelDependencyOffsets,
		MetadataCanonicalIndexScratch,
		RelationsCanonicalIndexScratch,
		LayoutInputsCanonicalIndexScratch,
		DependenciesCanonicalIndexScratch,
		EnumMetadataCanonicalIndexScratch,
		PropertyMetadataCanonicalIndexScratch,
		CombinedCanonicalIndexScratch,
		PropertyReplayScratch,
		MethodOrdinalScratch,
		VftOrdinalScratch,
		BehaviorOrdinalScratch,
		UFunctionOrdinalScratch,
		Count,
	};
	static constexpr uint16 TsScrExpectedSiteCountForTests = 56;
	static_assert(static_cast<uint16>(ETsScrAllocationSiteKindForTests::Count)
		== TsScrExpectedSiteCountForTests);
	static constexpr int32 TsScrExpectedRepresentativeFixtureCountForTests = 48;
	static constexpr int32 TsScrExpectedRepresentativeTargetCountForTests = 76;
	struct FTsScrSiteRepresentativeAuthorityForTests
	{
		ETsScrAllocationSiteKindForTests SiteKind;
		ETsScrSiteDispositionForTests Disposition;
		int32 ExpectedTargetCount;
	};
	static constexpr FTsScrSiteRepresentativeAuthorityForTests
	TsScrSiteRepresentativeAuthoritiesForTests[] = {
		{ETsScrAllocationSiteKindForTests::TokenController,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::CanonicalPayloadOwnedBytes,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::FlatHeaderOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::CanonicalNamespaceString,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::CanonicalNameString,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::CanonicalDeclarationString,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::MetadataArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::MetadataKeyString,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::MetadataValueString,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::ParallelMetadataOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::RelationsArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ParallelRelationOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::LayoutInputsArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ParallelLayoutInputOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::PropertiesArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ParallelPropertyOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::PropertyCanonicalNameString,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::DataTypeOrderedSubTypesArray,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::PropertyMetadataArray,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::PropertyMetadataKeyString,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::PropertyMetadataValueString,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::ParallelNestedPropertyOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::MethodsArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ParallelMethodOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::VftArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ParallelVftOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::BehaviorSlotsArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ParallelBehaviorOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::EnumEnumeratorsArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::EnumNameString,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::EnumMetadataArray,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::EnumMetadataKeyString,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::EnumMetadataValueString,
			ETsScrSiteDispositionForTests::Required, 4},
		{ETsScrAllocationSiteKindForTests::TypedefDataTypeOrderedSubTypesArray,
			ETsScrSiteDispositionForTests::InvalidFixtureOnly, 0},
		{ETsScrAllocationSiteKindForTests::FlatSelectedArmOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ReflectionConfigNameString,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ReflectionStaticClassGlobalNameString,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::UFunctionMembersArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ReflectedFunctionNameString,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ReflectedOriginalFunctionNameString,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ReflectedScriptFunctionNameString,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ParallelReflectionOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::DependenciesArray,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::ParallelDependencyOffsets,
			ETsScrSiteDispositionForTests::Required, 1},
		{ETsScrAllocationSiteKindForTests::MetadataCanonicalIndexScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::RelationsCanonicalIndexScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::LayoutInputsCanonicalIndexScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::DependenciesCanonicalIndexScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::EnumMetadataCanonicalIndexScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::PropertyMetadataCanonicalIndexScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::CombinedCanonicalIndexScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::PropertyReplayScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::MethodOrdinalScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::VftOrdinalScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::BehaviorOrdinalScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
		{ETsScrAllocationSiteKindForTests::UFunctionOrdinalScratch,
			ETsScrSiteDispositionForTests::StreamingZero, 0},
	};
	static_assert(UE_ARRAY_COUNT(TsScrSiteRepresentativeAuthoritiesForTests)
		== TsScrExpectedSiteCountForTests);
	static_assert([]
	{
		bool Seen[TsScrExpectedSiteCountForTests] = {};
		int32 RequiredCount = 0;
		int32 StreamingZeroCount = 0;
		int32 InvalidFixtureOnlyCount = 0;
		int32 TargetCount = 0;
		int32 AuthorityIndex = 0;
		for (const FTsScrSiteRepresentativeAuthorityForTests& Authority :
			TsScrSiteRepresentativeAuthoritiesForTests)
		{
			const int32 SiteIndex = static_cast<int32>(Authority.SiteKind);
			if (SiteIndex < 0 || SiteIndex >= TsScrExpectedSiteCountForTests
				|| SiteIndex != AuthorityIndex++ || Seen[SiteIndex])
			{
				return false;
			}
			Seen[SiteIndex] = true;
			RequiredCount += Authority.Disposition
				== ETsScrSiteDispositionForTests::Required;
			StreamingZeroCount += Authority.Disposition
				== ETsScrSiteDispositionForTests::StreamingZero;
			InvalidFixtureOnlyCount += Authority.Disposition
				== ETsScrSiteDispositionForTests::InvalidFixtureOnly;
			if ((Authority.Disposition == ETsScrSiteDispositionForTests::Required)
				!= (Authority.ExpectedTargetCount > 0))
			{
				return false;
			}
			TargetCount += Authority.ExpectedTargetCount;
		}
		return RequiredCount == 43 && StreamingZeroCount == 12
			&& InvalidFixtureOnlyCount == 1
			&& TargetCount == TsScrExpectedRepresentativeTargetCountForTests;
	}(), "the named 56-site authority freezes 43/12/1 and 76 targets");

	struct FTsScrAllocationSiteTemplateForTests
	{
		uint8 Family = 0;
		ETsScrAllocationSiteKindForTests SiteKind;
		ETsScrSiteDispositionForTests Disposition;
		ETsScrSiteLifetimeForTests Lifetime;
		EAngelscriptCacheValidationStage ExpectedStage;
	};

	static TConstArrayView<FTsScrAllocationSiteTemplateForTests>
	GetIndependentTsScrSiteTemplatesForTests()
	{
		using K = ETsScrAllocationSiteKindForTests;
		using D = ETsScrSiteDispositionForTests;
		using L = ETsScrSiteLifetimeForTests;
		const EAngelscriptCacheValidationStage P =
			EAngelscriptCacheValidationStage::PayloadDecode;
		const EAngelscriptCacheValidationStage S =
			EAngelscriptCacheValidationStage::LocalSemantic;
		static const FTsScrAllocationSiteTemplateForTests Templates[] = {
			{1, K::TokenController, D::Required, L::CandidateTemporary, P},
			{1, K::CanonicalPayloadOwnedBytes, D::Required, L::CandidateTemporary, P},
			{1, K::FlatHeaderOffsets, D::Required, L::CandidateTemporary, P},
			{1, K::CanonicalNamespaceString, D::Required, L::CandidateTemporary, P},
			{1, K::CanonicalNameString, D::Required, L::CandidateTemporary, P},
			{1, K::CanonicalDeclarationString, D::Required, L::CandidateTemporary, P},
			{1, K::MetadataArray, D::Required, L::CandidateTemporary, P},
			{1, K::MetadataKeyString, D::Required, L::CandidateTemporary, P},
			{1, K::MetadataValueString, D::Required, L::CandidateTemporary, P},
			{1, K::ParallelMetadataOffsets, D::Required, L::CandidateTemporary, P},
			{2, K::RelationsArray, D::Required, L::CandidateTemporary, P},
			{2, K::ParallelRelationOffsets, D::Required, L::CandidateTemporary, P},
			{3, K::LayoutInputsArray, D::Required, L::CandidateTemporary, P},
			{3, K::ParallelLayoutInputOffsets, D::Required, L::CandidateTemporary, P},
			{4, K::PropertiesArray, D::Required, L::CandidateTemporary, P},
			{4, K::ParallelPropertyOffsets, D::Required, L::CandidateTemporary, P},
			{5, K::PropertyCanonicalNameString, D::Required, L::CandidateTemporary, P},
			{5, K::DataTypeOrderedSubTypesArray, D::Required, L::CandidateTemporary, P},
			{5, K::PropertyMetadataArray, D::Required, L::CandidateTemporary, P},
			{5, K::PropertyMetadataKeyString, D::Required, L::CandidateTemporary, P},
			{5, K::PropertyMetadataValueString, D::Required, L::CandidateTemporary, P},
			{5, K::ParallelNestedPropertyOffsets, D::Required, L::CandidateTemporary, P},
			{6, K::MethodsArray, D::Required, L::CandidateTemporary, P},
			{6, K::ParallelMethodOffsets, D::Required, L::CandidateTemporary, P},
			{7, K::VftArray, D::Required, L::CandidateTemporary, P},
			{7, K::ParallelVftOffsets, D::Required, L::CandidateTemporary, P},
			{8, K::BehaviorSlotsArray, D::Required, L::CandidateTemporary, P},
			{8, K::ParallelBehaviorOffsets, D::Required, L::CandidateTemporary, P},
			{9, K::EnumEnumeratorsArray, D::Required, L::CandidateTemporary, P},
			{9, K::EnumNameString, D::Required, L::CandidateTemporary, P},
			{9, K::EnumMetadataArray, D::Required, L::CandidateTemporary, P},
			{9, K::EnumMetadataKeyString, D::Required, L::CandidateTemporary, P},
			{9, K::EnumMetadataValueString, D::Required, L::CandidateTemporary, P},
			{9, K::TypedefDataTypeOrderedSubTypesArray, D::InvalidFixtureOnly,
				L::CandidateTemporary, P},
			{9, K::FlatSelectedArmOffsets, D::Required, L::CandidateTemporary, P},
			{10, K::ReflectionConfigNameString, D::Required, L::CandidateTemporary, P},
			{10, K::ReflectionStaticClassGlobalNameString, D::Required, L::CandidateTemporary, P},
			{10, K::UFunctionMembersArray, D::Required, L::CandidateTemporary, P},
			{10, K::ReflectedFunctionNameString, D::Required,
				L::CandidateTemporary, P},
			{10, K::ReflectedOriginalFunctionNameString, D::Required,
				L::CandidateTemporary, P},
			{10, K::ReflectedScriptFunctionNameString, D::Required,
				L::CandidateTemporary, P},
			{10, K::ParallelReflectionOffsets, D::Required, L::CandidateTemporary, P},
			{11, K::DependenciesArray, D::Required, L::CandidateTemporary, P},
			{11, K::ParallelDependencyOffsets, D::Required, L::CandidateTemporary, P},
			{12, K::MetadataCanonicalIndexScratch, D::StreamingZero, L::StreamingTemporary, S},
			{12, K::RelationsCanonicalIndexScratch, D::StreamingZero, L::StreamingTemporary, S},
			{12, K::LayoutInputsCanonicalIndexScratch, D::StreamingZero, L::StreamingTemporary, S},
			{12, K::DependenciesCanonicalIndexScratch, D::StreamingZero, L::StreamingTemporary, S},
			{12, K::EnumMetadataCanonicalIndexScratch, D::StreamingZero, L::StreamingTemporary, S},
			{12, K::PropertyMetadataCanonicalIndexScratch, D::StreamingZero, L::StreamingTemporary, S},
			{12, K::CombinedCanonicalIndexScratch, D::StreamingZero, L::StreamingTemporary, S},
			{13, K::PropertyReplayScratch, D::StreamingZero, L::StreamingTemporary, S},
			{14, K::MethodOrdinalScratch, D::StreamingZero, L::StreamingTemporary, S},
			{14, K::VftOrdinalScratch, D::StreamingZero, L::StreamingTemporary, S},
			{14, K::BehaviorOrdinalScratch, D::StreamingZero, L::StreamingTemporary, S},
			{14, K::UFunctionOrdinalScratch, D::StreamingZero, L::StreamingTemporary, S},
		};
		static_assert(UE_ARRAY_COUNT(Templates) == TsScrExpectedSiteCountForTests);
		return MakeArrayView(Templates);
	}

	struct FTsScrExpectedAllocationEventForTests
	{
		ETsScrAllocationSiteKindForTests SiteKind;
		uint8 Family = 0;
		int32 PrimaryIndex = INDEX_NONE;
		int32 SecondaryIndex = INDEX_NONE;
		int32 TertiaryIndex = INDEX_NONE;
		uint64 ExpectedByteOffset = 0;
		uint64 ReferenceVisibilityByteOffset = 0;
		int32 RequestedElementCount = 0;
		uint64 ElementSize = 0;
		uint64 ElementAlignment = 0;
		uint64 ReservedCapacity = 0;
		uint64 ExactChargeBytes = 0;
		bool bTouchesActualAllocatorSlackBoundary = false;
		uint64 TotalPrefixBefore = 0;
		uint64 ResidentPrefixBefore = 0;
		uint64 TemporaryPrefixBefore = 0;
		uint64 PeakLiveAfter = 0;
		ETsScrSiteLifetimeForTests Lifetime =
			ETsScrSiteLifetimeForTests::CandidateTemporary;
		EAngelscriptCacheValidationStage ExpectedStage =
			EAngelscriptCacheValidationStage::PayloadDecode;
	};

	struct FIndependentTsScrExpectedPlanForTests
	{
		TArray<FTsScrExpectedAllocationEventForTests> Events;
		uint64 ExactTotalBytes = 0;
		uint64 ExactResidentBytes = 0;
		uint64 ExactTemporaryBytes = 0;
		uint64 PrePromotionTemporaryBytes = 0;
		uint64 PromotionCheckpointCount = 0;
		uint64 PeakLiveBytes = 0;
	};

	static const FTsScrAllocationSiteTemplateForTests& FindTsScrTemplateForTests(
		const ETsScrAllocationSiteKindForTests SiteKind)
	{
		for (const FTsScrAllocationSiteTemplateForTests& Template :
			GetIndependentTsScrSiteTemplatesForTests())
		{
			if (Template.SiteKind == SiteKind)
			{
				return Template;
			}
		}
		checkNoEntry();
		return GetIndependentTsScrSiteTemplatesForTests()[0];
	}

	static uint64 FindIndependentWireOffsetForTests(
		const FIndependentTypeSchemaWireInventoryForTests& Inventory,
		const EAngelscriptCacheTypeSchemaTestField Field,
		const int32 Primary = INDEX_NONE,
		const int32 Secondary = INDEX_NONE,
		const int32 Tertiary = INDEX_NONE)
	{
		for (const FIndependentTypeSchemaWireSpanForTests& Span : Inventory.WireSpans)
		{
			if (Span.Field == Field
				&& Span.PrimaryIndex == Primary
				&& Span.SecondaryIndex == Secondary
				&& Span.TertiaryIndex == Tertiary)
			{
				return Span.ExactOffset;
			}
		}
		checkNoEntry();
		return 0;
	}

	static uint64 RequireIndependentSpanOffsetForTests(
		const TConstArrayView<uint8> Payload,
		const EAngelscriptCacheTypeSchemaTestField Field,
		const int32 Primary = INDEX_NONE,
		const int32 Secondary = INDEX_NONE,
		const int32 Tertiary = INDEX_NONE)
	{
		const FIndependentTypeSchemaWireInventoryForTests Inventory =
			ScanIndependentTypeSchemaWireForTests(Payload);
		check(Inventory.bComplete);
		return FindIndependentWireOffsetForTests(
			Inventory, Field, Primary, Secondary, Tertiary);
	}

	static bool ExpectExactFailureAndReset(
		FAutomationTestBase& Test,
		const FMalformedDecodeOutcome& Outcome,
		const EAngelscriptCacheValidationError ExpectedError,
		const EAngelscriptCacheValidationStage ExpectedStage,
		const EAngelscriptCacheTypeSchemaTestField ExpectedField,
		const TCHAR* Context,
		const int32 PrimaryIndex = INDEX_NONE,
		const int32 SecondaryIndex = INDEX_NONE,
		const int32 TertiaryIndex = INDEX_NONE)
	{
		return ExpectExactFailureAndReset(
			Test,
			Outcome.Result,
			Outcome.Output,
			ExpectedError,
			ExpectedStage,
			RequireIndependentSpanOffsetForTests(Outcome.Payload, ExpectedField,
				PrimaryIndex, SecondaryIndex, TertiaryIndex),
			Context);
	}

	static uint64 FindIndependentStringStartForTests(
		const FIndependentTypeSchemaWireInventoryForTests& Inventory,
		const EAngelscriptCacheTypeSchemaTestField Field,
		const int32 Primary = INDEX_NONE,
		const int32 Secondary = INDEX_NONE,
		const int32 Tertiary = INDEX_NONE)
	{
		const uint64 Utf8BytesOffset = FindIndependentWireOffsetForTests(
			Inventory, Field, Primary, Secondary, Tertiary);
		check(Utf8BytesOffset >= sizeof(uint32));
		return Utf8BytesOffset - sizeof(uint32);
	}

	template <typename ElementType>
	static void AppendIndependentTsScrSiteForTests(
		FIndependentTsScrExpectedPlanForTests& Plan,
		const ETsScrAllocationSiteKindForTests SiteKind,
		const int32 RequestedElementCount,
		const uint64 ExpectedByteOffset,
		const int32 Primary = INDEX_NONE,
		const int32 Secondary = INDEX_NONE,
		const int32 Tertiary = INDEX_NONE)
	{
		if (RequestedElementCount <= 0)
		{
			return;
		}
		const FTsScrAllocationSiteTemplateForTests& Template =
			FindTsScrTemplateForTests(SiteKind);
		check(Template.Disposition == ETsScrSiteDispositionForTests::Required);
		const int32 ReservedCapacity =
			CalculateIndependentArrayReserveCapacityForTests<ElementType>(RequestedElementCount);
		const uint64 ExactCharge = uint64(ReservedCapacity) * sizeof(ElementType);
		check(ExactCharge > 0);
		FTsScrExpectedAllocationEventForTests Event;
		Event.SiteKind = SiteKind;
		Event.Family = Template.Family;
		Event.PrimaryIndex = Primary;
		Event.SecondaryIndex = Secondary;
		Event.TertiaryIndex = Tertiary;
		Event.ExpectedByteOffset = ExpectedByteOffset;
		Event.ReferenceVisibilityByteOffset = ExpectedByteOffset;
		Event.RequestedElementCount = RequestedElementCount;
		Event.ElementSize = sizeof(ElementType);
		Event.ElementAlignment = alignof(ElementType);
		Event.ReservedCapacity = ReservedCapacity;
		Event.ExactChargeBytes = ExactCharge;
		const int32 PreviousCapacity = RequestedElementCount > 1
			? CalculateIndependentArrayReserveCapacityForTests<ElementType>(
				RequestedElementCount - 1)
			: 0;
		const int32 NextCapacity =
			CalculateIndependentArrayReserveCapacityForTests<ElementType>(
				RequestedElementCount + 1);
		Event.bTouchesActualAllocatorSlackBoundary =
			ReservedCapacity != PreviousCapacity || ReservedCapacity != NextCapacity;
		Event.TotalPrefixBefore = Plan.ExactTotalBytes;
		Event.ResidentPrefixBefore = Plan.ExactResidentBytes;
		Event.TemporaryPrefixBefore = Plan.ExactTemporaryBytes;
		Event.Lifetime = Template.Lifetime;
		Event.ExpectedStage = Template.ExpectedStage;
		Plan.ExactTotalBytes += ExactCharge;
		check(Template.Lifetime
			== ETsScrSiteLifetimeForTests::CandidateTemporary);
		Plan.ExactTemporaryBytes += ExactCharge;
		Event.PeakLiveAfter = Plan.ExactResidentBytes + Plan.ExactTemporaryBytes;
		Plan.PeakLiveBytes = FMath::Max(Plan.PeakLiveBytes, Event.PeakLiveAfter);
		Plan.Events.Add(Event);
	}

	static void AppendIndependentTsScrControllerForTests(
		FIndependentTsScrExpectedPlanForTests& Plan)
	{
		using FController = SharedPointerInternals::TIntrusiveReferenceController<
			FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>;
		constexpr uint32 EffectiveAlignment = alignof(FController)
			> __STDCPP_DEFAULT_NEW_ALIGNMENT__
			? alignof(FController)
			: (sizeof(FController) <= 8
				? uint32(8) : uint32(__STDCPP_DEFAULT_NEW_ALIGNMENT__));
		void* Allocation = FMemory::Malloc(sizeof(FController), EffectiveAlignment);
		check(Allocation != nullptr);
		const uint64 ActualControllerBytes = static_cast<uint64>(
			FMemory::GetAllocSize(Allocation));
		FMemory::Free(Allocation);
		const uint64 QuantizedControllerBytes = static_cast<uint64>(
			FMemory::QuantizeSize(sizeof(FController), EffectiveAlignment));
		const uint64 IndependentControllerBytes =
			ActualControllerBytes >= sizeof(FController)
				? ActualControllerBytes
				: QuantizedControllerBytes;
		const FTsScrAllocationSiteTemplateForTests& Template = FindTsScrTemplateForTests(
			ETsScrAllocationSiteKindForTests::TokenController);
		FTsScrExpectedAllocationEventForTests Event;
		Event.SiteKind = ETsScrAllocationSiteKindForTests::TokenController;
		Event.Family = Template.Family;
		Event.ExpectedByteOffset = 0;
		Event.ReferenceVisibilityByteOffset = 0;
		Event.RequestedElementCount = 1;
		Event.ElementSize = sizeof(FController);
		Event.ElementAlignment = alignof(FController);
		Event.ReservedCapacity = 1;
		Event.ExactChargeBytes = IndependentControllerBytes;
		Event.TotalPrefixBefore = Plan.ExactTotalBytes;
		Event.ResidentPrefixBefore = Plan.ExactResidentBytes;
		Event.TemporaryPrefixBefore = Plan.ExactTemporaryBytes;
		Event.Lifetime = Template.Lifetime;
		Event.ExpectedStage = Template.ExpectedStage;
		Plan.ExactTotalBytes += Event.ExactChargeBytes;
		check(Template.Lifetime
			== ETsScrSiteLifetimeForTests::CandidateTemporary);
		Plan.ExactTemporaryBytes += Event.ExactChargeBytes;
		Event.PeakLiveAfter = Plan.ExactTemporaryBytes;
		Plan.PeakLiveBytes = FMath::Max(Plan.PeakLiveBytes, Event.PeakLiveAfter);
		Plan.Events.Add(Event);
	}

	static uint64 MeasureIndependentControllerBaseAllocationForTests()
	{
		// The WITH_ANGELSCRIPT_UNITTESTS hook constructs the exact private-token
		// FController via NewIntrusiveReferenceController, measures its allocation
		// base, destroys it, and never exposes an interior TSharedRef pointer.
		return FAngelscriptDecodedCacheRecord::
			MeasureExactControllerBaseAllocationForTests();
	}

	static int32 CountCapturedFieldsInRangeForTests(
		const FIndependentTypeSchemaWireInventoryForTests& Inventory,
		const EAngelscriptTypeSchemaCapturedField First,
		const EAngelscriptTypeSchemaCapturedField Last)
	{
		int32 Count = 0;
		for (const FIndependentTypeSchemaCapturedOffsetForTests& Entry :
			Inventory.CapturedOffsets)
		{
			const uint16 Raw = static_cast<uint16>(Entry.Coordinate.Field);
			Count += Raw >= static_cast<uint16>(First)
				&& Raw <= static_cast<uint16>(Last);
		}
		return Count;
	}

	static FIndependentTsScrExpectedPlanForTests BuildIndependentTsScrPlanForTests(
		const FAngelscriptCachedTypeSchema& Schema,
		const TConstArrayView<uint8> CanonicalPayload,
		const FIndependentTypeSchemaWireInventoryForTests& Inventory)
	{
		using K = ETsScrAllocationSiteKindForTests;
		using O = FAngelscriptTypeSchemaCapturedOffsetEntryForTests;
		FIndependentTsScrExpectedPlanForTests Plan;
		AppendIndependentTsScrControllerForTests(Plan);
		AppendIndependentTsScrSiteForTests<uint8>(Plan, K::CanonicalPayloadOwnedBytes,
			CanonicalPayload.Num(), 0);
		const int32 FlatHeaderCount = CountCapturedFieldsInRangeForTests(Inventory,
			EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion,
			EAngelscriptTypeSchemaCapturedField::Metadata)
			+ CountCapturedFieldsInRangeForTests(Inventory,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation,
				EAngelscriptTypeSchemaCapturedField::LayoutExpectation);
		AppendIndependentTsScrSiteForTests<O>(Plan, K::FlatHeaderOffsets,
			FlatHeaderCount, 0);
		AppendIndependentTsScrSiteForTests<TCHAR>(Plan, K::CanonicalNamespaceString,
			Schema.CanonicalNamespace.Len() + 1,
			Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::CanonicalNamespace}).Get(0));
		AppendIndependentTsScrSiteForTests<TCHAR>(Plan, K::CanonicalNameString,
			Schema.CanonicalName.Len() + 1,
			Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::CanonicalName}).Get(0));
		AppendIndependentTsScrSiteForTests<TCHAR>(Plan, K::CanonicalDeclarationString,
			Schema.CanonicalDeclaration.Len() + 1,
			Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::CanonicalDeclaration}).Get(0));
		AppendIndependentTsScrSiteForTests<FAngelscriptCachedMetadataEntry>(Plan,
			K::MetadataArray, Schema.Metadata.Num(),
			Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::Metadata}).Get(0));
		for (int32 Index = 0; Index < Schema.Metadata.Num(); ++Index)
		{
			AppendIndependentTsScrSiteForTests<TCHAR>(Plan, K::MetadataKeyString,
				Schema.Metadata[Index].CanonicalKey.Len() + 1,
				FindIndependentStringStartForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::MetadataEntry,
					Index, INDEX_NONE, 0), Index,
				INDEX_NONE, 0);
			AppendIndependentTsScrSiteForTests<TCHAR>(Plan, K::MetadataValueString,
				Schema.Metadata[Index].CanonicalValue.Len() + 1,
				FindIndependentStringStartForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::MetadataEntry,
					Index, INDEX_NONE, 1), Index,
				INDEX_NONE, 1);
		}
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelMetadataOffsets,
			Schema.Metadata.Num(),
			Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::Metadata}).Get(0));

		AppendIndependentTsScrSiteForTests<FAngelscriptCachedTypeRelation>(Plan,
			K::RelationsArray, Schema.Relations.Num(),
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::Relations));
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelRelationOffsets,
			Schema.Relations.Num() * 2,
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::Relations));
		AppendIndependentTsScrSiteForTests<FAngelscriptCachedTypeLayoutInput>(Plan,
			K::LayoutInputsArray, Schema.LayoutInputs.Num(),
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::LayoutInputs));
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelLayoutInputOffsets,
			Schema.LayoutInputs.Num() * 2,
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::LayoutInputs));
		AppendIndependentTsScrSiteForTests<FAngelscriptCachedPropertySchema>(Plan,
			K::PropertiesArray, Schema.OrderedProperties.Num(),
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::OrderedProperties));
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelPropertyOffsets,
			Schema.OrderedProperties.Num() * 2,
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::OrderedProperties));
		int32 NestedPropertyOffsetCount = 0;
		for (int32 PropertyIndex = 0;
			PropertyIndex < Schema.OrderedProperties.Num(); ++PropertyIndex)
		{
			const FAngelscriptCachedPropertySchema& Property =
				Schema.OrderedProperties[PropertyIndex];
			AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
				K::PropertyCanonicalNameString, Property.CanonicalName.Len() + 1,
				FindIndependentStringStartForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::PropertyCanonicalName,
					PropertyIndex), PropertyIndex);
			int32 PreOrder = 0;
			const auto VisitPropertyType = [&](const auto& Self,
				const FAngelscriptCachedDataType& Type) -> void
			{
				const int32 ThisPreOrder = PreOrder++;
				++NestedPropertyOffsetCount;
				if (!Type.OrderedSubTypes.IsEmpty())
				{
					AppendIndependentTsScrSiteForTests<FAngelscriptCachedDataType>(Plan,
						K::DataTypeOrderedSubTypesArray, Type.OrderedSubTypes.Num(),
						FindIndependentWireOffsetForTests(Inventory,
							EAngelscriptCacheTypeSchemaTestField::DataTypeOrderedSubTypes,
							PropertyIndex, ThisPreOrder),
						PropertyIndex, ThisPreOrder);
				}
				for (const FAngelscriptCachedDataType& Child : Type.OrderedSubTypes)
				{
					Self(Self, Child);
				}
			};
			VisitPropertyType(VisitPropertyType, Property.Type);
			AppendIndependentTsScrSiteForTests<FAngelscriptCachedMetadataEntry>(Plan,
				K::PropertyMetadataArray, Property.Metadata.Num(),
				FindIndependentWireOffsetForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::PropertyMetadata,
					PropertyIndex),
				PropertyIndex);
			for (int32 MetadataIndex = 0;
				MetadataIndex < Property.Metadata.Num(); ++MetadataIndex)
			{
				++NestedPropertyOffsetCount;
				AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
					K::PropertyMetadataKeyString,
					Property.Metadata[MetadataIndex].CanonicalKey.Len() + 1,
					FindIndependentStringStartForTests(Inventory,
						EAngelscriptCacheTypeSchemaTestField::PropertyMetadataEntry,
						PropertyIndex, MetadataIndex, 0),
					PropertyIndex, MetadataIndex, 0);
				AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
					K::PropertyMetadataValueString,
					Property.Metadata[MetadataIndex].CanonicalValue.Len() + 1,
					FindIndependentStringStartForTests(Inventory,
						EAngelscriptCacheTypeSchemaTestField::PropertyMetadataEntry,
						PropertyIndex, MetadataIndex, 1),
					PropertyIndex, MetadataIndex, 1);
			}
		}
		const int32 NestedPropertyEventIndex = Plan.Events.Num();
		AppendIndependentTsScrSiteForTests<O>(Plan,
			K::ParallelNestedPropertyOffsets, NestedPropertyOffsetCount,
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::OrderedProperties));
		if (Plan.Events.IsValidIndex(NestedPropertyEventIndex))
		{
			Plan.Events[NestedPropertyEventIndex].ReferenceVisibilityByteOffset =
				FindIndependentWireOffsetForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::OrderedMethods);
		}

		AppendIndependentTsScrSiteForTests<FAngelscriptCachedMethodEntry>(Plan,
			K::MethodsArray, Schema.OrderedMethods.Num(),
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::OrderedMethods));
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelMethodOffsets,
			Schema.OrderedMethods.Num() * 3,
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::OrderedMethods));
		AppendIndependentTsScrSiteForTests<FAngelscriptCachedVirtualFunctionSlot>(Plan,
			K::VftArray, Schema.VirtualFunctionTable.Num(),
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable));
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelVftOffsets,
			Schema.VirtualFunctionTable.Num() * 4,
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable));
		AppendIndependentTsScrSiteForTests<FAngelscriptCachedBehaviorSlot>(Plan,
			K::BehaviorSlotsArray, Schema.OrderedBehaviorSlots.Num(),
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::BehaviorSlots));
		const int32 BehaviorOffsets = Schema.OrderedBehaviorSlots.Num() * 3;
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelBehaviorOffsets,
			BehaviorOffsets, FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::BehaviorSlots));

		if (Schema.KindPayload.Enum.IsSet())
		{
			const TArray<FAngelscriptCachedEnumEnumerator>& Enumerators =
				Schema.KindPayload.Enum->OrderedEnumerators;
			const uint64 KindOffset = Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::KindPayload}).Get(0);
			AppendIndependentTsScrSiteForTests<FAngelscriptCachedEnumEnumerator>(Plan,
				K::EnumEnumeratorsArray, Enumerators.Num(), KindOffset);
			for (int32 EnumIndex = 0; EnumIndex < Enumerators.Num(); ++EnumIndex)
			{
				AppendIndependentTsScrSiteForTests<TCHAR>(Plan, K::EnumNameString,
					Enumerators[EnumIndex].CanonicalName.Len() + 1,
					FindIndependentStringStartForTests(Inventory,
						EAngelscriptCacheTypeSchemaTestField::EnumCanonicalName,
						EnumIndex), EnumIndex);
				AppendIndependentTsScrSiteForTests<FAngelscriptCachedMetadataEntry>(Plan,
					K::EnumMetadataArray, Enumerators[EnumIndex].Metadata.Num(),
					FindIndependentWireOffsetForTests(Inventory,
						EAngelscriptCacheTypeSchemaTestField::EnumEnumeratorMetadata,
						EnumIndex), EnumIndex);
				for (int32 MetadataIndex = 0;
					MetadataIndex < Enumerators[EnumIndex].Metadata.Num(); ++MetadataIndex)
				{
					AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
						K::EnumMetadataKeyString,
						Enumerators[EnumIndex].Metadata[MetadataIndex].CanonicalKey.Len() + 1,
						FindIndependentStringStartForTests(Inventory,
							EAngelscriptCacheTypeSchemaTestField::EnumEnumeratorMetadataEntry,
							EnumIndex, MetadataIndex, 0),
						EnumIndex, MetadataIndex, 0);
					AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
						K::EnumMetadataValueString,
						Enumerators[EnumIndex].Metadata[MetadataIndex].CanonicalValue.Len() + 1,
						FindIndependentStringStartForTests(Inventory,
							EAngelscriptCacheTypeSchemaTestField::EnumEnumeratorMetadataEntry,
							EnumIndex, MetadataIndex, 1),
						EnumIndex, MetadataIndex, 1);
				}
			}
		}
		const int32 SelectedArmOffsets = CountCapturedFieldsInRangeForTests(Inventory,
			EAngelscriptTypeSchemaCapturedField::KindPayload,
			EAngelscriptTypeSchemaCapturedField::CallableSignature);
		const int32 SelectedArmEventIndex = Plan.Events.Num();
		AppendIndependentTsScrSiteForTests<O>(Plan, K::FlatSelectedArmOffsets,
			SelectedArmOffsets, Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::KindPayload}).Get(0));
		check(Plan.Events.IsValidIndex(SelectedArmEventIndex));
		Plan.Events[SelectedArmEventIndex].ReferenceVisibilityByteOffset =
			Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::Reflection}).Get(0);

		if (Schema.Reflection.ConfigName.IsSet())
		{
			AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
				K::ReflectionConfigNameString,
				Schema.Reflection.ConfigName->Len() + 1,
				FindIndependentStringStartForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::ReflectionConfigNameOptionalTag,
					INDEX_NONE, INDEX_NONE, 1));
		}
		if (Schema.Reflection.StaticClassGlobalName.IsSet())
		{
			AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
				K::ReflectionStaticClassGlobalNameString,
				Schema.Reflection.StaticClassGlobalName->Len() + 1,
				FindIndependentStringStartForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::ReflectionStaticClassGlobalNameOptionalTag,
					INDEX_NONE, INDEX_NONE, 1));
		}
		AppendIndependentTsScrSiteForTests<FAngelscriptCachedReflectedFunctionMember>(Plan,
			K::UFunctionMembersArray, Schema.Reflection.OrderedUFunctionMembers.Num(),
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers));
		for (int32 Index = 0;
			Index < Schema.Reflection.OrderedUFunctionMembers.Num(); ++Index)
		{
			const FAngelscriptCachedReflectedFunctionMember& Member =
				Schema.Reflection.OrderedUFunctionMembers[Index];
			AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
				K::ReflectedFunctionNameString,
				Member.CanonicalFunctionName.Len() + 1,
				FindIndependentStringStartForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::
						ReflectedFunctionNameBytes,
					Index), Index);
			if (!Member.CanonicalOriginalFunctionName.IsEmpty())
			{
				AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
					K::ReflectedOriginalFunctionNameString,
					Member.CanonicalOriginalFunctionName.Len() + 1,
					FindIndependentStringStartForTests(Inventory,
						EAngelscriptCacheTypeSchemaTestField::
							ReflectedOriginalFunctionNameBytes,
						Index), Index);
			}
			AppendIndependentTsScrSiteForTests<TCHAR>(Plan,
				K::ReflectedScriptFunctionNameString,
				Member.CanonicalScriptFunctionName.Len() + 1,
				FindIndependentStringStartForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::
						ReflectedScriptFunctionNameBytes,
					Index), Index);
		}
		const int32 ReflectionOffsetEventIndex = Plan.Events.Num();
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelReflectionOffsets,
			Schema.Reflection.OrderedUFunctionMembers.Num() * 5,
			Inventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::Reflection}).Get(0));
		if (Plan.Events.IsValidIndex(ReflectionOffsetEventIndex))
		{
			Plan.Events[ReflectionOffsetEventIndex].ReferenceVisibilityByteOffset =
				FindIndependentWireOffsetForTests(Inventory,
					EAngelscriptCacheTypeSchemaTestField::Dependencies);
		}
		AppendIndependentTsScrSiteForTests<FAngelscriptCacheSemanticDependency>(Plan,
			K::DependenciesArray, Schema.Dependencies.Num(),
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::Dependencies));
		AppendIndependentTsScrSiteForTests<O>(Plan, K::ParallelDependencyOffsets,
			Schema.Dependencies.Num() * 2,
			FindIndependentWireOffsetForTests(Inventory,
				EAngelscriptCacheTypeSchemaTestField::Dependencies));
		Plan.PrePromotionTemporaryBytes = Plan.ExactTemporaryBytes;
		if (Plan.PrePromotionTemporaryBytes > 0)
		{
			Plan.ExactResidentBytes = Plan.PrePromotionTemporaryBytes;
			Plan.ExactTemporaryBytes = 0;
			Plan.PromotionCheckpointCount = 1;
		}
		return Plan;
	}

	struct FTsScrExpandedAllocationCaseForTests
	{
		int32 RepresentativeFixtureIndex = INDEX_NONE;
		uint8 Family = 0;
		uint32 Variant = 0;
		uint32 Cardinality = 0;
		ETsScrAllocationSiteKindForTests SiteKind =
			ETsScrAllocationSiteKindForTests::TokenController;
		int32 PrimaryIndex = INDEX_NONE;
		int32 SecondaryIndex = INDEX_NONE;
		int32 TertiaryIndex = INDEX_NONE;
		int32 RequestedElementCount = 0;
		uint64 ReservedCapacity = 0;
		uint64 ExactChargeBytes = 0;
		bool bTouchesActualAllocatorSlackBoundary = false;
	};

	enum class ETsScrRepresentativeFixturePurposeForTests : uint8
	{
		Shape,
		Occurrence,
		Slack,
	};

	enum class ETsScrRepresentativeFixtureIdForTests : uint8
	{
		ShapeEmpty,
		ShapeOne,
		ShapeMany,
		OccurrenceTopMetadata,
		OccurrencePropertyMetadata,
		OccurrenceNestedDataType,
		OccurrenceEnumMetadata,
		SlackCanonicalPayload,
		SlackCanonicalNamespace,
		SlackCanonicalName,
		SlackCanonicalDeclaration,
		SlackMetadataArray,
		SlackMetadataKey,
		SlackMetadataValue,
		SlackParallelMetadata,
		SlackRelationsArray,
		SlackParallelRelations,
		SlackLayoutInputs,
		SlackParallelLayoutInputs,
		SlackPropertiesArray,
		SlackParallelProperties,
		SlackPropertyName,
		SlackDataTypeSubTypes,
		SlackPropertyMetadataArray,
		SlackPropertyMetadataKey,
		SlackPropertyMetadataValue,
		SlackParallelNestedPropertyOffsets,
		SlackMethodsArray,
		SlackParallelMethods,
		SlackVftArray,
		SlackParallelVft,
		SlackBehaviorSlots,
		SlackParallelBehavior,
		SlackEnumEnumerators,
		SlackEnumName,
		SlackEnumMetadataArray,
		SlackEnumMetadataKey,
		SlackEnumMetadataValue,
		SlackFlatSelectedArmOffsets,
		SlackReflectionConfigName,
		SlackReflectionStaticClassGlobalName,
		SlackUFunctionMembers,
		SlackReflectedFunctionName,
		SlackReflectedOriginalFunctionName,
		SlackReflectedScriptFunctionName,
		SlackParallelReflection,
		SlackDependencies,
		SlackParallelDependencies,
		Count,
	};

	struct FTsScrRepresentativeFixtureAuthorityForTests
	{
		ETsScrRepresentativeFixtureIdForTests FixtureId;
		ETsScrRepresentativeFixturePurposeForTests Purpose;
		uint8 Family;
		uint32 Variant;
		uint32 Cardinality;
		ETsScrAllocationSiteKindForTests SlackSiteKind;
		int32 SlackPrimaryIndex = INDEX_NONE;
		int32 SlackSecondaryIndex = INDEX_NONE;
		int32 SlackTertiaryIndex = INDEX_NONE;
		int32 ExpectedSlackRequestedElementCount = 0;
		uint32 CustomizationPaddingCharacters = 0;
	};

	// Cardinality, expected allocator request, and any fixture padding are frozen
	// source literals. Runtime code may verify them, but never searches or selects
	// a replacement fixture when the platform allocator contract drifts.
	static constexpr FTsScrRepresentativeFixtureAuthorityForTests
	TsScrRepresentativeFixtureAuthoritiesForTests[] = {
		{ETsScrRepresentativeFixtureIdForTests::ShapeEmpty,
			ETsScrRepresentativeFixturePurposeForTests::Shape, 1, 4, 0,
			ETsScrAllocationSiteKindForTests::TokenController},
		{ETsScrRepresentativeFixtureIdForTests::ShapeOne,
			ETsScrRepresentativeFixturePurposeForTests::Shape, 1, 4, 1,
			ETsScrAllocationSiteKindForTests::TokenController},
		{ETsScrRepresentativeFixtureIdForTests::ShapeMany,
			ETsScrRepresentativeFixturePurposeForTests::Shape, 1, 4, 32,
			ETsScrAllocationSiteKindForTests::TokenController},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrRepresentativeFixturePurposeForTests::Occurrence, 1, 3, 17,
			ETsScrAllocationSiteKindForTests::TokenController},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrRepresentativeFixturePurposeForTests::Occurrence, 5, 5, 17,
			ETsScrAllocationSiteKindForTests::TokenController},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceNestedDataType,
			ETsScrRepresentativeFixturePurposeForTests::Occurrence, 5, 1, 17,
			ETsScrAllocationSiteKindForTests::TokenController},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrRepresentativeFixturePurposeForTests::Occurrence, 9, 0, 17,
			ETsScrAllocationSiteKindForTests::TokenController},
		// F1/V0/C0 is 196 wire bytes before the fixed 28-character ASCII padding.
		{ETsScrRepresentativeFixtureIdForTests::SlackCanonicalPayload,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 1, 0, 0,
			ETsScrAllocationSiteKindForTests::CanonicalPayloadOwnedBytes,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 224, 28},
		{ETsScrRepresentativeFixtureIdForTests::SlackCanonicalNamespace,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 1, 0, 2,
			ETsScrAllocationSiteKindForTests::CanonicalNamespaceString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 4},
		{ETsScrRepresentativeFixtureIdForTests::SlackCanonicalName,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 1, 1, 2,
			ETsScrAllocationSiteKindForTests::CanonicalNameString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 4},
		{ETsScrRepresentativeFixtureIdForTests::SlackCanonicalDeclaration,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 1, 2, 2,
			ETsScrAllocationSiteKindForTests::CanonicalDeclarationString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 4},
		{ETsScrRepresentativeFixtureIdForTests::SlackMetadataArray,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 1, 3, 33,
			ETsScrAllocationSiteKindForTests::MetadataArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 33},
		{ETsScrRepresentativeFixtureIdForTests::SlackMetadataKey,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 1, 3, 53,
			ETsScrAllocationSiteKindForTests::MetadataKeyString,
			0, INDEX_NONE, 0, 64},
		{ETsScrRepresentativeFixtureIdForTests::SlackMetadataValue,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 1, 3, 67,
			ETsScrAllocationSiteKindForTests::MetadataValueString,
			0, INDEX_NONE, 1, 80},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelMetadata,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 1, 3, 85,
			ETsScrAllocationSiteKindForTests::ParallelMetadataOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 85},
		{ETsScrRepresentativeFixtureIdForTests::SlackRelationsArray,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 2, 3, 1,
			ETsScrAllocationSiteKindForTests::RelationsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelRelations,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 2, 3, 21,
			ETsScrAllocationSiteKindForTests::ParallelRelationOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 42},
		{ETsScrRepresentativeFixtureIdForTests::SlackLayoutInputs,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 3, 1, 1,
			ETsScrAllocationSiteKindForTests::LayoutInputsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelLayoutInputs,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 3, 2, 1,
			ETsScrAllocationSiteKindForTests::ParallelLayoutInputOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 2},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertiesArray,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 4, 0, 1,
			ETsScrAllocationSiteKindForTests::PropertiesArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelProperties,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 4, 0, 21,
			ETsScrAllocationSiteKindForTests::ParallelPropertyOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 42},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertyName,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 5, 0, 2,
			ETsScrAllocationSiteKindForTests::PropertyCanonicalNameString,
			0, INDEX_NONE, INDEX_NONE, 4},
		{ETsScrRepresentativeFixtureIdForTests::SlackDataTypeSubTypes,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 5, 2, 1,
			ETsScrAllocationSiteKindForTests::DataTypeOrderedSubTypesArray,
			0, 0, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertyMetadataArray,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 5, 3, 1,
			ETsScrAllocationSiteKindForTests::PropertyMetadataArray,
			0, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertyMetadataKey,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 5, 3, 19,
			ETsScrAllocationSiteKindForTests::PropertyMetadataKeyString,
			0, 0, 0, 32},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertyMetadataValue,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 5, 3, 33,
			ETsScrAllocationSiteKindForTests::PropertyMetadataValueString,
			0, 0, 1, 48},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelNestedPropertyOffsets,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 5, 4, 55,
			ETsScrAllocationSiteKindForTests::ParallelNestedPropertyOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 56},
		{ETsScrRepresentativeFixtureIdForTests::SlackMethodsArray,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 6, 0, 1,
			ETsScrAllocationSiteKindForTests::MethodsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelMethods,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 6, 0, 19,
			ETsScrAllocationSiteKindForTests::ParallelMethodOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 57},
		{ETsScrRepresentativeFixtureIdForTests::SlackVftArray,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 7, 0, 1,
			ETsScrAllocationSiteKindForTests::VftArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelVft,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 7, 0, 18,
			ETsScrAllocationSiteKindForTests::ParallelVftOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 72},
		{ETsScrRepresentativeFixtureIdForTests::SlackBehaviorSlots,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 8, 1, 1,
			ETsScrAllocationSiteKindForTests::BehaviorSlotsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelBehavior,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 8, 1, 19,
			ETsScrAllocationSiteKindForTests::ParallelBehaviorOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 57},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumEnumerators,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 9, 0, 1,
			ETsScrAllocationSiteKindForTests::EnumEnumeratorsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumName,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 9, 0, 6,
			ETsScrAllocationSiteKindForTests::EnumNameString,
			0, INDEX_NONE, INDEX_NONE, 16, 6},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumMetadataArray,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 9, 0, 10,
			ETsScrAllocationSiteKindForTests::EnumMetadataArray,
			0, INDEX_NONE, INDEX_NONE, 10},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumMetadataKey,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 9, 0, 24,
			ETsScrAllocationSiteKindForTests::EnumMetadataKeyString,
			0, 0, 0, 40},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumMetadataValue,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 9, 0, 30,
			ETsScrAllocationSiteKindForTests::EnumMetadataValueString,
			0, 0, 1, 48},
		{ETsScrRepresentativeFixtureIdForTests::SlackFlatSelectedArmOffsets,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 9, 1, 0,
			ETsScrAllocationSiteKindForTests::FlatSelectedArmOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 2},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectionConfigName,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 10, 1, 3,
			ETsScrAllocationSiteKindForTests::ReflectionConfigNameString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 4},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectionStaticClassGlobalName,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 10, 2, 3,
			ETsScrAllocationSiteKindForTests::ReflectionStaticClassGlobalNameString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 4},
		{ETsScrRepresentativeFixtureIdForTests::SlackUFunctionMembers,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 10, 3, 1,
			ETsScrAllocationSiteKindForTests::UFunctionMembersArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectedFunctionName,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 10, 6, 1,
			ETsScrAllocationSiteKindForTests::ReflectedFunctionNameString,
			0, INDEX_NONE, INDEX_NONE, 19},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectedOriginalFunctionName,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 10, 7, 1,
			ETsScrAllocationSiteKindForTests::ReflectedOriginalFunctionNameString,
			0, INDEX_NONE, INDEX_NONE, 19},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectedScriptFunctionName,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 10, 8, 1,
			ETsScrAllocationSiteKindForTests::ReflectedScriptFunctionNameString,
			0, INDEX_NONE, INDEX_NONE, 19},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelReflection,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 10, 3, 21,
			ETsScrAllocationSiteKindForTests::ParallelReflectionOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 105},
		{ETsScrRepresentativeFixtureIdForTests::SlackDependencies,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 11, 2, 1,
			ETsScrAllocationSiteKindForTests::DependenciesArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelDependencies,
			ETsScrRepresentativeFixturePurposeForTests::Slack, 11, 3, 21,
			ETsScrAllocationSiteKindForTests::ParallelDependencyOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, 42},
	};
	static_assert(UE_ARRAY_COUNT(TsScrRepresentativeFixtureAuthoritiesForTests)
		== TsScrExpectedRepresentativeFixtureCountForTests);
	static_assert(static_cast<int32>(ETsScrRepresentativeFixtureIdForTests::Count)
		== TsScrExpectedRepresentativeFixtureCountForTests);
	static_assert([]
	{
		const auto Matches = [](const int32 FixtureIndex,
			const ETsScrRepresentativeFixturePurposeForTests Purpose,
			const uint8 Family, const uint32 Variant, const uint32 Cardinality)
		{
			const FTsScrRepresentativeFixtureAuthorityForTests& Row =
				TsScrRepresentativeFixtureAuthoritiesForTests[FixtureIndex];
			return Row.Purpose == Purpose && Row.Family == Family
				&& Row.Variant == Variant
				&& Row.Cardinality == Cardinality;
		};
		using P = ETsScrRepresentativeFixturePurposeForTests;
		return Matches(0, P::Shape, 1, 4, 0)
			&& Matches(1, P::Shape, 1, 4, 1)
			&& Matches(2, P::Shape, 1, 4, 32)
			&& Matches(3, P::Occurrence, 1, 3, 17)
			&& Matches(4, P::Occurrence, 5, 5, 17)
			&& Matches(5, P::Occurrence, 5, 1, 17)
			&& Matches(6, P::Occurrence, 9, 0, 17);
	}(), "shape and first/middle/last occurrence fixtures are exact");
	static_assert([]
	{
		bool SeenFixtureIds[TsScrExpectedRepresentativeFixtureCountForTests] = {};
		bool SeenSlackSites[TsScrExpectedSiteCountForTests] = {};
		int32 ShapeCount = 0;
		int32 OccurrenceCount = 0;
		int32 SlackCount = 0;
		for (int32 RowIndex = 0;
			RowIndex < TsScrExpectedRepresentativeFixtureCountForTests; ++RowIndex)
		{
			const FTsScrRepresentativeFixtureAuthorityForTests& Row =
				TsScrRepresentativeFixtureAuthoritiesForTests[RowIndex];
			const int32 FixtureIndex = static_cast<int32>(Row.FixtureId);
			if (FixtureIndex != RowIndex || SeenFixtureIds[FixtureIndex])
			{
				return false;
			}
			SeenFixtureIds[FixtureIndex] = true;
			for (int32 PreviousIndex = 0; PreviousIndex < RowIndex; ++PreviousIndex)
			{
				const FTsScrRepresentativeFixtureAuthorityForTests& Previous =
					TsScrRepresentativeFixtureAuthoritiesForTests[PreviousIndex];
				if (Previous.Family == Row.Family
					&& Previous.Variant == Row.Variant
					&& Previous.Cardinality == Row.Cardinality)
				{
					return false;
				}
			}
			ShapeCount += Row.Purpose
				== ETsScrRepresentativeFixturePurposeForTests::Shape;
			OccurrenceCount += Row.Purpose
				== ETsScrRepresentativeFixturePurposeForTests::Occurrence;
			SlackCount += Row.Purpose
				== ETsScrRepresentativeFixturePurposeForTests::Slack;
			if (Row.Purpose == ETsScrRepresentativeFixturePurposeForTests::Slack)
			{
				const int32 SiteIndex = static_cast<int32>(Row.SlackSiteKind);
				if (Row.ExpectedSlackRequestedElementCount <= 0
					|| SeenSlackSites[SiteIndex]
					|| Row.SlackSiteKind
						== ETsScrAllocationSiteKindForTests::TokenController
					|| Row.SlackSiteKind
						== ETsScrAllocationSiteKindForTests::FlatHeaderOffsets)
				{
					return false;
				}
				SeenSlackSites[SiteIndex] = true;
				const bool bHasCustomizationPadding =
					Row.CustomizationPaddingCharacters > 0;
				const bool bPaddingFixture = Row.FixtureId
						== ETsScrRepresentativeFixtureIdForTests::SlackCanonicalPayload
					|| Row.FixtureId
						== ETsScrRepresentativeFixtureIdForTests::SlackEnumName;
				if (bHasCustomizationPadding != bPaddingFixture)
				{
					return false;
				}
			}
			else if (Row.ExpectedSlackRequestedElementCount != 0
				|| Row.CustomizationPaddingCharacters != 0)
			{
				return false;
			}
		}
		for (const FTsScrSiteRepresentativeAuthorityForTests& Site :
			TsScrSiteRepresentativeAuthoritiesForTests)
		{
			const bool bRequiresSlackFixture = Site.Disposition
				== ETsScrSiteDispositionForTests::Required
				&& Site.SiteKind != ETsScrAllocationSiteKindForTests::TokenController
				&& Site.SiteKind != ETsScrAllocationSiteKindForTests::FlatHeaderOffsets;
			if (SeenSlackSites[static_cast<int32>(Site.SiteKind)]
				!= bRequiresSlackFixture)
			{
				return false;
			}
		}
		const FTsScrRepresentativeFixtureAuthorityForTests& Payload =
			TsScrRepresentativeFixtureAuthoritiesForTests[static_cast<int32>(
				ETsScrRepresentativeFixtureIdForTests::SlackCanonicalPayload)];
		const FTsScrRepresentativeFixtureAuthorityForTests& EnumName =
			TsScrRepresentativeFixtureAuthoritiesForTests[static_cast<int32>(
				ETsScrRepresentativeFixtureIdForTests::SlackEnumName)];
		return ShapeCount == 3 && OccurrenceCount == 4 && SlackCount == 41
			&& Payload.Cardinality == 0
			&& Payload.ExpectedSlackRequestedElementCount == 224
			&& Payload.CustomizationPaddingCharacters == 28
			&& EnumName.Cardinality == 6
			&& EnumName.ExpectedSlackRequestedElementCount == 16
			&& EnumName.CustomizationPaddingCharacters == 6;
	}(), "fixture authority is exactly 3 shape + 4 occurrence + 41 slack rows");

	struct FTsScrRepresentativeTargetAuthorityForTests
	{
		ETsScrRepresentativeFixtureIdForTests FixtureId;
		ETsScrAllocationSiteKindForTests SiteKind;
		int32 PrimaryIndex = INDEX_NONE;
		int32 SecondaryIndex = INDEX_NONE;
		int32 TertiaryIndex = INDEX_NONE;
		bool bMustTouchActualAllocatorSlack = false;
	};

	static constexpr FTsScrRepresentativeTargetAuthorityForTests
	TsScrRepresentativeTargetAuthoritiesForTests[] = {
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrAllocationSiteKindForTests::TokenController},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrAllocationSiteKindForTests::FlatHeaderOffsets},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrAllocationSiteKindForTests::MetadataKeyString, 0, INDEX_NONE, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrAllocationSiteKindForTests::MetadataKeyString, 8, INDEX_NONE, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrAllocationSiteKindForTests::MetadataKeyString, 16, INDEX_NONE, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrAllocationSiteKindForTests::MetadataValueString, 0, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrAllocationSiteKindForTests::MetadataValueString, 8, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata,
			ETsScrAllocationSiteKindForTests::MetadataValueString, 16, INDEX_NONE, 1},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyCanonicalNameString, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyCanonicalNameString, 8},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyCanonicalNameString, 16},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataArray, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataArray, 8},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataArray, 16},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataKeyString, 0, 0, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataKeyString, 8, 8, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataKeyString, 16, 16, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataValueString, 0, 0, 1},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataValueString, 8, 8, 1},
		{ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata,
			ETsScrAllocationSiteKindForTests::PropertyMetadataValueString, 16, 16, 1},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceNestedDataType,
			ETsScrAllocationSiteKindForTests::DataTypeOrderedSubTypesArray, 0, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceNestedDataType,
			ETsScrAllocationSiteKindForTests::DataTypeOrderedSubTypesArray, 0, 8},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceNestedDataType,
			ETsScrAllocationSiteKindForTests::DataTypeOrderedSubTypesArray, 0, 16},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumNameString, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumNameString, 8},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumNameString, 16},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataArray, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataArray, 8},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataArray, 16},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataKeyString, 0, 0, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataKeyString, 8, 8, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataKeyString, 16, 16, 0},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataValueString, 0, 0, 1},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataValueString, 8, 8, 1},
		{ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata,
			ETsScrAllocationSiteKindForTests::EnumMetadataValueString, 16, 16, 1},
		{ETsScrRepresentativeFixtureIdForTests::SlackCanonicalPayload,
			ETsScrAllocationSiteKindForTests::CanonicalPayloadOwnedBytes,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackCanonicalNamespace,
			ETsScrAllocationSiteKindForTests::CanonicalNamespaceString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackCanonicalName,
			ETsScrAllocationSiteKindForTests::CanonicalNameString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackCanonicalDeclaration,
			ETsScrAllocationSiteKindForTests::CanonicalDeclarationString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackMetadataArray,
			ETsScrAllocationSiteKindForTests::MetadataArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackMetadataKey,
			ETsScrAllocationSiteKindForTests::MetadataKeyString,
			0, INDEX_NONE, 0, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackMetadataValue,
			ETsScrAllocationSiteKindForTests::MetadataValueString,
			0, INDEX_NONE, 1, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelMetadata,
			ETsScrAllocationSiteKindForTests::ParallelMetadataOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackRelationsArray,
			ETsScrAllocationSiteKindForTests::RelationsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelRelations,
			ETsScrAllocationSiteKindForTests::ParallelRelationOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackLayoutInputs,
			ETsScrAllocationSiteKindForTests::LayoutInputsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelLayoutInputs,
			ETsScrAllocationSiteKindForTests::ParallelLayoutInputOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertiesArray,
			ETsScrAllocationSiteKindForTests::PropertiesArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelProperties,
			ETsScrAllocationSiteKindForTests::ParallelPropertyOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertyName,
			ETsScrAllocationSiteKindForTests::PropertyCanonicalNameString,
			0, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackDataTypeSubTypes,
			ETsScrAllocationSiteKindForTests::DataTypeOrderedSubTypesArray,
			0, 0, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertyMetadataArray,
			ETsScrAllocationSiteKindForTests::PropertyMetadataArray,
			0, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertyMetadataKey,
			ETsScrAllocationSiteKindForTests::PropertyMetadataKeyString,
			0, 0, 0, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackPropertyMetadataValue,
			ETsScrAllocationSiteKindForTests::PropertyMetadataValueString,
			0, 0, 1, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelNestedPropertyOffsets,
			ETsScrAllocationSiteKindForTests::ParallelNestedPropertyOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackMethodsArray,
			ETsScrAllocationSiteKindForTests::MethodsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelMethods,
			ETsScrAllocationSiteKindForTests::ParallelMethodOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackVftArray,
			ETsScrAllocationSiteKindForTests::VftArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelVft,
			ETsScrAllocationSiteKindForTests::ParallelVftOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackBehaviorSlots,
			ETsScrAllocationSiteKindForTests::BehaviorSlotsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelBehavior,
			ETsScrAllocationSiteKindForTests::ParallelBehaviorOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumEnumerators,
			ETsScrAllocationSiteKindForTests::EnumEnumeratorsArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumName,
			ETsScrAllocationSiteKindForTests::EnumNameString,
			0, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumMetadataArray,
			ETsScrAllocationSiteKindForTests::EnumMetadataArray,
			0, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumMetadataKey,
			ETsScrAllocationSiteKindForTests::EnumMetadataKeyString,
			0, 0, 0, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackEnumMetadataValue,
			ETsScrAllocationSiteKindForTests::EnumMetadataValueString,
			0, 0, 1, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackFlatSelectedArmOffsets,
			ETsScrAllocationSiteKindForTests::FlatSelectedArmOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectionConfigName,
			ETsScrAllocationSiteKindForTests::ReflectionConfigNameString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectionStaticClassGlobalName,
			ETsScrAllocationSiteKindForTests::ReflectionStaticClassGlobalNameString,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackUFunctionMembers,
			ETsScrAllocationSiteKindForTests::UFunctionMembersArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectedFunctionName,
			ETsScrAllocationSiteKindForTests::ReflectedFunctionNameString,
			0, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectedOriginalFunctionName,
			ETsScrAllocationSiteKindForTests::ReflectedOriginalFunctionNameString,
			0, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackReflectedScriptFunctionName,
			ETsScrAllocationSiteKindForTests::ReflectedScriptFunctionNameString,
			0, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelReflection,
			ETsScrAllocationSiteKindForTests::ParallelReflectionOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackDependencies,
			ETsScrAllocationSiteKindForTests::DependenciesArray,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
		{ETsScrRepresentativeFixtureIdForTests::SlackParallelDependencies,
			ETsScrAllocationSiteKindForTests::ParallelDependencyOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE, true},
	};
	static_assert(UE_ARRAY_COUNT(TsScrRepresentativeTargetAuthoritiesForTests)
		== TsScrExpectedRepresentativeTargetCountForTests);
	static_assert([]
	{
		int32 CountsBySite[TsScrExpectedSiteCountForTests] = {};
		int32 CountsByFixture[TsScrExpectedRepresentativeFixtureCountForTests] = {};
		for (int32 TargetIndex = 0;
			TargetIndex < TsScrExpectedRepresentativeTargetCountForTests;
			++TargetIndex)
		{
			const FTsScrRepresentativeTargetAuthorityForTests& Target =
				TsScrRepresentativeTargetAuthoritiesForTests[TargetIndex];
			const int32 FixtureIndex = static_cast<int32>(Target.FixtureId);
			if (FixtureIndex < 0
				|| FixtureIndex >= TsScrExpectedRepresentativeFixtureCountForTests
				|| TsScrRepresentativeFixtureAuthoritiesForTests[FixtureIndex].FixtureId
					!= Target.FixtureId
				|| Target.bMustTouchActualAllocatorSlack
					!= (TsScrRepresentativeFixtureAuthoritiesForTests[
						FixtureIndex].Purpose
						== ETsScrRepresentativeFixturePurposeForTests::Slack))
			{
				return false;
			}
			++CountsByFixture[FixtureIndex];
			++CountsBySite[static_cast<int32>(Target.SiteKind)];
			for (int32 PreviousIndex = 0;
				PreviousIndex < TargetIndex; ++PreviousIndex)
			{
				const FTsScrRepresentativeTargetAuthorityForTests& Previous =
					TsScrRepresentativeTargetAuthoritiesForTests[PreviousIndex];
				if (Previous.FixtureId == Target.FixtureId
					&& Previous.SiteKind == Target.SiteKind
					&& Previous.PrimaryIndex == Target.PrimaryIndex
					&& Previous.SecondaryIndex == Target.SecondaryIndex
					&& Previous.TertiaryIndex == Target.TertiaryIndex)
				{
					return false;
				}
			}
		}
		for (int32 SiteIndex = 0;
			SiteIndex < TsScrExpectedSiteCountForTests; ++SiteIndex)
		{
			if (CountsBySite[SiteIndex]
				!= TsScrSiteRepresentativeAuthoritiesForTests[SiteIndex].ExpectedTargetCount)
			{
				return false;
			}
		}
		for (int32 FixtureIndex = 0;
			FixtureIndex < TsScrExpectedRepresentativeFixtureCountForTests;
			++FixtureIndex)
		{
			const FTsScrRepresentativeFixtureAuthorityForTests& Fixture =
				TsScrRepresentativeFixtureAuthoritiesForTests[FixtureIndex];
			if (Fixture.Purpose == ETsScrRepresentativeFixturePurposeForTests::Shape
				&& CountsByFixture[FixtureIndex] != 0)
			{
				return false;
			}
			if (Fixture.Purpose == ETsScrRepresentativeFixturePurposeForTests::Slack
				&& CountsByFixture[FixtureIndex] != 1)
			{
				return false;
			}
			if (Fixture.Purpose == ETsScrRepresentativeFixturePurposeForTests::Slack)
			{
				int32 SlackTargetIndex = INDEX_NONE;
				for (int32 TargetIndex = 0;
					TargetIndex < TsScrExpectedRepresentativeTargetCountForTests;
					++TargetIndex)
				{
					const FTsScrRepresentativeTargetAuthorityForTests& Target =
						TsScrRepresentativeTargetAuthoritiesForTests[TargetIndex];
					if (Target.FixtureId == Fixture.FixtureId)
					{
						if (SlackTargetIndex != INDEX_NONE)
						{
							return false;
						}
						SlackTargetIndex = TargetIndex;
					}
				}
				if (SlackTargetIndex == INDEX_NONE)
				{
					return false;
				}
				const FTsScrRepresentativeTargetAuthorityForTests& SlackTarget =
					TsScrRepresentativeTargetAuthoritiesForTests[SlackTargetIndex];
				if (!SlackTarget.bMustTouchActualAllocatorSlack
					|| SlackTarget.SiteKind != Fixture.SlackSiteKind
					|| SlackTarget.PrimaryIndex != Fixture.SlackPrimaryIndex
					|| SlackTarget.SecondaryIndex != Fixture.SlackSecondaryIndex
					|| SlackTarget.TertiaryIndex != Fixture.SlackTertiaryIndex)
				{
					return false;
				}
			}
		}
		return CountsByFixture[static_cast<int32>(
			ETsScrRepresentativeFixtureIdForTests::OccurrenceTopMetadata)] == 8
			&& CountsByFixture[static_cast<int32>(
				ETsScrRepresentativeFixtureIdForTests::OccurrencePropertyMetadata)] == 12
			&& CountsByFixture[static_cast<int32>(
				ETsScrRepresentativeFixtureIdForTests::OccurrenceNestedDataType)] == 3
			&& CountsByFixture[static_cast<int32>(
				ETsScrRepresentativeFixtureIdForTests::OccurrenceEnumMetadata)] == 12;
	}(), "76 explicit target tuples reference the 48 fixtures exactly");

	static void ApplyTsScrRepresentativeFixtureCustomizationForTests(
		const FTsScrRepresentativeFixtureAuthorityForTests& Authority,
		FAngelscriptCachedTypeSchema& Schema)
	{
		if (Authority.CustomizationPaddingCharacters == 0)
		{
			return;
		}
		if (Authority.FixtureId
			== ETsScrRepresentativeFixtureIdForTests::SlackCanonicalPayload)
		{
			Schema.CanonicalNamespace += FString::ChrN(
				int32(Authority.CustomizationPaddingCharacters), TEXT('B'));
		}
		else if (Authority.FixtureId
			== ETsScrRepresentativeFixtureIdForTests::SlackEnumName)
		{
			check(Schema.KindPayload.Enum.IsSet());
			for (FAngelscriptCachedEnumEnumerator& Enumerator :
				Schema.KindPayload.Enum->OrderedEnumerators)
			{
				Enumerator.CanonicalName += FString::ChrN(
					int32(Authority.CustomizationPaddingCharacters), TEXT('N'));
			}
		}
		else
		{
			checkNoEntry();
		}
		FinalizeValidFixtureHashes(Schema);
	}

	struct FTsScrPlanCoordinateKeyForTests
	{
		ETsScrAllocationSiteKindForTests SiteKind;
		int32 PrimaryIndex = INDEX_NONE;
		int32 SecondaryIndex = INDEX_NONE;
		int32 TertiaryIndex = INDEX_NONE;

		bool operator==(const FTsScrPlanCoordinateKeyForTests& Other) const
		{
			return SiteKind == Other.SiteKind
				&& PrimaryIndex == Other.PrimaryIndex
				&& SecondaryIndex == Other.SecondaryIndex
				&& TertiaryIndex == Other.TertiaryIndex;
		}

		friend uint32 GetTypeHash(const FTsScrPlanCoordinateKeyForTests& Key)
		{
			uint32 Hash = ::GetTypeHash(static_cast<uint16>(Key.SiteKind));
			Hash = HashCombine(Hash, ::GetTypeHash(Key.PrimaryIndex));
			Hash = HashCombine(Hash, ::GetTypeHash(Key.SecondaryIndex));
			return HashCombine(Hash, ::GetTypeHash(Key.TertiaryIndex));
		}
	};

	struct FTsScrHostileTypedefAuthorityForTests
	{
		int32 RequestedSubTypes;
		int32 TargetProbeOccurrenceIndex;
		FTsScrPlanCoordinateKeyForTests TargetCoordinate;
		FTsScrPlanCoordinateKeyForTests FollowingPlanCoordinate;
	};

	static constexpr FTsScrHostileTypedefAuthorityForTests
	TsScrHostileTypedefAuthorityForTests{
		2,
		6,
		{ETsScrAllocationSiteKindForTests::TypedefDataTypeOrderedSubTypesArray,
			INDEX_NONE, 0, INDEX_NONE},
		{ETsScrAllocationSiteKindForTests::FlatSelectedArmOffsets,
			INDEX_NONE, INDEX_NONE, INDEX_NONE},
	};
	static_assert(TsScrHostileTypedefAuthorityForTests.RequestedSubTypes == 2);
	static_assert(TsScrHostileTypedefAuthorityForTests.TargetProbeOccurrenceIndex == 6);
	static_assert(TsScrHostileTypedefAuthorityForTests.TargetCoordinate.SiteKind
		== ETsScrAllocationSiteKindForTests::TypedefDataTypeOrderedSubTypesArray);
	static_assert(TsScrHostileTypedefAuthorityForTests.TargetCoordinate.PrimaryIndex
		== INDEX_NONE);
	static_assert(TsScrHostileTypedefAuthorityForTests.TargetCoordinate.SecondaryIndex
		== 0);
	static_assert(TsScrHostileTypedefAuthorityForTests.TargetCoordinate.TertiaryIndex
		== INDEX_NONE);

	static TMap<FTsScrPlanCoordinateKeyForTests, int32>
	BuildUniqueTsScrPlanCoordinateIndexForTests(
		const FIndependentTsScrExpectedPlanForTests& Plan)
	{
		TMap<FTsScrPlanCoordinateKeyForTests, int32> Result;
		Result.Reserve(Plan.Events.Num());
		for (int32 EventIndex = 0; EventIndex < Plan.Events.Num(); ++EventIndex)
		{
			const FTsScrExpectedAllocationEventForTests& Event =
				Plan.Events[EventIndex];
			const FTsScrPlanCoordinateKeyForTests Coordinate{
				Event.SiteKind, Event.PrimaryIndex,
				Event.SecondaryIndex, Event.TertiaryIndex};
			check(!Result.Contains(Coordinate));
			Result.Add(Coordinate, EventIndex);
		}
		return Result;
	}

	struct FTsScrResolvedRepresentativeFixtureForTests
	{
		FTsScrRepresentativeFixtureAuthorityForTests Authority;
		FAngelscriptCachedTypeSchema Schema;
		TArray<uint8> Payload;
		FIndependentTypeSchemaWireInventoryForTests Inventory;
		FIndependentTsScrExpectedPlanForTests Plan;
		TMap<FTsScrPlanCoordinateKeyForTests, int32> EventIndexByCoordinate;
		TArray<FTsScrExpandedAllocationCaseForTests> Targets;
	};

	static const TArray<FTsScrResolvedRepresentativeFixtureForTests>&
	GetResolvedTsScrRepresentativeFixturesForTests()
	{
		static const TArray<FTsScrResolvedRepresentativeFixtureForTests> Fixtures = []
		{
			TArray<FTsScrResolvedRepresentativeFixtureForTests> Result;
			Result.Reserve(TsScrExpectedRepresentativeFixtureCountForTests);
			for (int32 FixtureIndex = 0;
				FixtureIndex < TsScrExpectedRepresentativeFixtureCountForTests;
				++FixtureIndex)
			{
				const FTsScrRepresentativeFixtureAuthorityForTests& Authority =
					TsScrRepresentativeFixtureAuthoritiesForTests[FixtureIndex];
				check(static_cast<int32>(Authority.FixtureId) == FixtureIndex);
				check(Authority.Family > 0 && Authority.Family <= 11);
				check(Authority.Variant
					< GetTypeSchemaAllocationVariantCount(Authority.Family));
				FTsScrResolvedRepresentativeFixtureForTests Fixture;
				Fixture.Authority = Authority;
				bool bExpectedLocalSuccess = false;
				Fixture.Schema = MakeTypeSchemaAllocationFixture(
					Authority.Family, Authority.Cardinality, Authority.Variant,
					bExpectedLocalSuccess);
				check(bExpectedLocalSuccess);
				ApplyTsScrRepresentativeFixtureCustomizationForTests(
					Authority, Fixture.Schema);
				check(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
					Fixture.Schema, Fixture.Payload).IsSuccess());
				Fixture.Inventory =
					ScanIndependentTypeSchemaWireForTests(Fixture.Payload);
				check(Fixture.Inventory.bComplete);
				Fixture.Plan = BuildIndependentTsScrPlanForTests(
					Fixture.Schema, Fixture.Payload, Fixture.Inventory);
				Fixture.EventIndexByCoordinate =
					BuildUniqueTsScrPlanCoordinateIndexForTests(Fixture.Plan);
				for (const FTsScrResolvedRepresentativeFixtureForTests& Previous : Result)
				{
					check(Previous.Authority.Family != Authority.Family
						|| Previous.Authority.Variant != Authority.Variant
						|| Previous.Authority.Cardinality != Authority.Cardinality);
				}
				Result.Add(MoveTemp(Fixture));
			}
			check(Result.Num()
				== TsScrExpectedRepresentativeFixtureCountForTests);

			// The independent plan is indexed once per fixture. Explicit target rows
			// then resolve in one pass without scanning a plan once per target.
			int32 ResolvedTargetCount = 0;
			for (const FTsScrRepresentativeTargetAuthorityForTests& TargetAuthority :
				TsScrRepresentativeTargetAuthoritiesForTests)
			{
				const int32 FixtureIndex =
					static_cast<int32>(TargetAuthority.FixtureId);
				FTsScrResolvedRepresentativeFixtureForTests& Fixture =
					Result[FixtureIndex];
				const FTsScrPlanCoordinateKeyForTests Coordinate{
					TargetAuthority.SiteKind, TargetAuthority.PrimaryIndex,
					TargetAuthority.SecondaryIndex, TargetAuthority.TertiaryIndex};
				const int32* EventIndex =
					Fixture.EventIndexByCoordinate.Find(Coordinate);
				check(EventIndex != nullptr);
				const FTsScrExpectedAllocationEventForTests& Event =
					Fixture.Plan.Events[*EventIndex];
				if (TargetAuthority.bMustTouchActualAllocatorSlack)
				{
					check(Event.RequestedElementCount
						== Fixture.Authority.ExpectedSlackRequestedElementCount);
					check(Event.bTouchesActualAllocatorSlackBoundary);
				}
				FTsScrExpandedAllocationCaseForTests Case;
				Case.RepresentativeFixtureIndex = FixtureIndex;
				Case.Family = Fixture.Authority.Family;
				Case.Variant = Fixture.Authority.Variant;
				Case.Cardinality = Fixture.Authority.Cardinality;
				Case.SiteKind = Event.SiteKind;
				Case.PrimaryIndex = Event.PrimaryIndex;
				Case.SecondaryIndex = Event.SecondaryIndex;
				Case.TertiaryIndex = Event.TertiaryIndex;
				Case.RequestedElementCount = Event.RequestedElementCount;
				Case.ReservedCapacity = Event.ReservedCapacity;
				Case.ExactChargeBytes = Event.ExactChargeBytes;
				Case.bTouchesActualAllocatorSlackBoundary =
					Event.bTouchesActualAllocatorSlackBoundary;
				Fixture.Targets.Add(Case);
				++ResolvedTargetCount;
			}
			check(ResolvedTargetCount
				== TsScrExpectedRepresentativeTargetCountForTests);
			return Result;
		}();
		return Fixtures;
	}

	static const TArray<FTsScrExpandedAllocationCaseForTests>&
	GetIndependentTsScrExpandedCasesForTests()
	{
		static const TArray<FTsScrExpandedAllocationCaseForTests> Cases = []
		{
			TArray<FTsScrExpandedAllocationCaseForTests> Result;
			Result.Reserve(TsScrExpectedRepresentativeTargetCountForTests);
			for (const FTsScrResolvedRepresentativeFixtureForTests& Fixture :
				GetResolvedTsScrRepresentativeFixturesForTests())
			{
				Result.Append(Fixture.Targets);
			}
			check(Result.Num()
				== TsScrExpectedRepresentativeTargetCountForTests);
			return Result;
		}();
		return Cases;
	}

	enum class ETsScrReferenceAuthorityShapeForTests : uint8
	{
		None,
		RelationLayoutDependency,
		Relations2LayoutReflectionDependencies2,
		Relations3Layouts2ReflectionDependencies3,
		RelationDependency,
		LayoutDependency,
		PropertyTypeDependency,
		Dependency,
		BehaviorDependency,
		Behaviors2Dependencies2,
		Behaviors2Dependency,
		Behaviors3Dependency,
		Delegate,
		Relations2LayoutDependency,
		RelationReflectionDependencies2,
		CombinedFamily14,
	};

	struct FTsScrReferenceCoordinateAuthorityForTests
	{
		EAngelscriptCacheTypeSchemaTestField Field;
		int32 PrimaryIndex = INDEX_NONE;
		int32 SecondaryIndex = INDEX_NONE;
		int32 TertiaryIndex = INDEX_NONE;
	};

	struct FTsScrReferenceCaseAuthorityForTests
	{
		uint8 Family = 0;
		uint32 Variant = 0;
		ETsScrReferenceAuthorityShapeForTests Shape =
			ETsScrReferenceAuthorityShapeForTests::None;
	};

	static TConstArrayView<FTsScrReferenceCoordinateAuthorityForTests>
	GetTsScrReferenceCoordinatesForTests(
		const ETsScrReferenceAuthorityShapeForTests Shape)
	{
		using F = EAngelscriptCacheTypeSchemaTestField;
		using C = FTsScrReferenceCoordinateAuthorityForTests;
		static const C RelationLayoutDependency[] = {
			{F::Relations, 0, 0}, {F::LayoutInput, 0, 0}, {F::Dependency, 0, 2}};
		static const C Relations2LayoutReflectionDependencies2[] = {
			{F::Relations, 0, 0}, {F::Relations, 1, 0},
			{F::LayoutInput, 0, 0}, {F::ReflectedFunctionMembers, 0, 0},
			{F::Dependency, 0, 2}, {F::Dependency, 1, 2}};
		static const C Relations3Layouts2ReflectionDependencies3[] = {
			{F::Relations, 0, 0}, {F::Relations, 1, 0}, {F::Relations, 2, 0},
			{F::LayoutInput, 0, 0}, {F::LayoutInput, 1, 0},
			{F::ReflectedFunctionMembers, 0, 0},
			{F::Dependency, 0, 2}, {F::Dependency, 1, 2}, {F::Dependency, 2, 2}};
		static const C RelationDependency[] = {
			{F::Relations, 0, 0}, {F::Dependency, 0, 2}};
		static const C LayoutDependency[] = {
			{F::LayoutInput, 0, 0}, {F::Dependency, 0, 2}};
		static const C PropertyTypeDependency[] = {
			{F::PropertyType, 0, 0, 0}, {F::Dependency, 0, 2}};
		static const C Dependency[] = {{F::Dependency, 0, 2}};
		static const C BehaviorDependency[] = {
			{F::BehaviorTarget, 0}, {F::Dependency, 0, 2}};
		static const C Behaviors2Dependencies2[] = {
			{F::BehaviorTarget, 0}, {F::BehaviorTarget, 1},
			{F::Dependency, 0, 2}, {F::Dependency, 1, 2}};
		static const C Behaviors2Dependency[] = {
			{F::BehaviorTarget, 0}, {F::BehaviorTarget, 1},
			{F::Dependency, 0, 2}};
		static const C Behaviors3Dependency[] = {
			{F::BehaviorTarget, 0}, {F::BehaviorTarget, 1},
			{F::BehaviorTarget, 2}, {F::Dependency, 0, 2}};
		static const C Delegate[] = {
			{F::BehaviorTarget, 0}, {F::BehaviorTarget, 1},
			{F::Dependency, 0, 2}, {F::Dependency, 1, 2}, {F::Dependency, 2, 2}};
		static const C Relations2LayoutDependency[] = {
			{F::Relations, 0, 0}, {F::Relations, 1, 0},
			{F::LayoutInput, 0, 0}, {F::Dependency, 0, 2}};
		static const C RelationReflectionDependencies2[] = {
			{F::Relations, 0, 0}, {F::ReflectedFunctionMembers, 0, 0},
			{F::Dependency, 0, 2}, {F::Dependency, 1, 2}};
		static const C CombinedFamily14[] = {
			{F::Relations, 0, 0}, {F::Relations, 1, 0},
			{F::LayoutInput, 0, 0},
			{F::BehaviorTarget, 0}, {F::BehaviorTarget, 1},
			{F::ReflectedFunctionMembers, 0, 0},
			{F::Dependency, 0, 2}, {F::Dependency, 1, 2},
			{F::Dependency, 2, 2}, {F::Dependency, 3, 2}, {F::Dependency, 4, 2}};

		switch (Shape)
		{
		case ETsScrReferenceAuthorityShapeForTests::None: return {};
		case ETsScrReferenceAuthorityShapeForTests::RelationLayoutDependency:
			return MakeArrayView(RelationLayoutDependency);
		case ETsScrReferenceAuthorityShapeForTests::Relations2LayoutReflectionDependencies2:
			return MakeArrayView(Relations2LayoutReflectionDependencies2);
		case ETsScrReferenceAuthorityShapeForTests::Relations3Layouts2ReflectionDependencies3:
			return MakeArrayView(Relations3Layouts2ReflectionDependencies3);
		case ETsScrReferenceAuthorityShapeForTests::RelationDependency:
			return MakeArrayView(RelationDependency);
		case ETsScrReferenceAuthorityShapeForTests::LayoutDependency:
			return MakeArrayView(LayoutDependency);
		case ETsScrReferenceAuthorityShapeForTests::PropertyTypeDependency:
			return MakeArrayView(PropertyTypeDependency);
		case ETsScrReferenceAuthorityShapeForTests::Dependency: return MakeArrayView(Dependency);
		case ETsScrReferenceAuthorityShapeForTests::BehaviorDependency:
			return MakeArrayView(BehaviorDependency);
		case ETsScrReferenceAuthorityShapeForTests::Behaviors2Dependencies2:
			return MakeArrayView(Behaviors2Dependencies2);
		case ETsScrReferenceAuthorityShapeForTests::Behaviors2Dependency:
			return MakeArrayView(Behaviors2Dependency);
		case ETsScrReferenceAuthorityShapeForTests::Behaviors3Dependency:
			return MakeArrayView(Behaviors3Dependency);
		case ETsScrReferenceAuthorityShapeForTests::Delegate: return MakeArrayView(Delegate);
		case ETsScrReferenceAuthorityShapeForTests::Relations2LayoutDependency:
			return MakeArrayView(Relations2LayoutDependency);
		case ETsScrReferenceAuthorityShapeForTests::RelationReflectionDependencies2:
			return MakeArrayView(RelationReflectionDependencies2);
		case ETsScrReferenceAuthorityShapeForTests::CombinedFamily14:
			return MakeArrayView(CombinedFamily14);
		default: checkNoEntry(); return {};
		}
	}

	using FTsScrReferenceShapeForTests = ETsScrReferenceAuthorityShapeForTests;
	static constexpr int32 TsScrExpectedReferenceCaseCountForTests = 78;
	static constexpr FTsScrReferenceCaseAuthorityForTests
	TsScrReferenceCaseAuthoritiesForTests[] = {
		{1,0,FTsScrReferenceShapeForTests::None},
		{1,1,FTsScrReferenceShapeForTests::None},
		{1,2,FTsScrReferenceShapeForTests::None},
		{1,3,FTsScrReferenceShapeForTests::None},
		{1,4,FTsScrReferenceShapeForTests::None},
		{2,0,FTsScrReferenceShapeForTests::RelationLayoutDependency},
		{2,1,FTsScrReferenceShapeForTests::Relations2LayoutReflectionDependencies2},
		{2,2,FTsScrReferenceShapeForTests::Relations2LayoutReflectionDependencies2},
		{2,3,FTsScrReferenceShapeForTests::RelationDependency},
		{3,0,FTsScrReferenceShapeForTests::None},
		{3,1,FTsScrReferenceShapeForTests::RelationLayoutDependency},
		{3,2,FTsScrReferenceShapeForTests::Relations2LayoutReflectionDependencies2},
		{3,3,FTsScrReferenceShapeForTests::LayoutDependency},
		{3,4,FTsScrReferenceShapeForTests::Relations3Layouts2ReflectionDependencies3},
		{4,0,FTsScrReferenceShapeForTests::None},
		{5,0,FTsScrReferenceShapeForTests::None},
		{5,1,FTsScrReferenceShapeForTests::PropertyTypeDependency},
		{5,2,FTsScrReferenceShapeForTests::PropertyTypeDependency},
		{5,3,FTsScrReferenceShapeForTests::None},
		{5,4,FTsScrReferenceShapeForTests::None},
		{5,5,FTsScrReferenceShapeForTests::PropertyTypeDependency},
		{6,0,FTsScrReferenceShapeForTests::Dependency},
		{7,0,FTsScrReferenceShapeForTests::Dependency},
		{8,1,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,5,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,9,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,13,FTsScrReferenceShapeForTests::Behaviors2Dependencies2},
		{8,17,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,21,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,25,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,29,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,37,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,41,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,45,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,49,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,53,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,57,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,61,FTsScrReferenceShapeForTests::Behaviors2Dependency},
		{8,65,FTsScrReferenceShapeForTests::Behaviors3Dependency},
		{8,10,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,22,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,26,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,30,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,38,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,42,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,46,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,50,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,54,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,58,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,62,FTsScrReferenceShapeForTests::BehaviorDependency},
		{8,66,FTsScrReferenceShapeForTests::BehaviorDependency},
		{9,0,FTsScrReferenceShapeForTests::None},
		{9,1,FTsScrReferenceShapeForTests::Delegate},
		{9,2,FTsScrReferenceShapeForTests::Dependency},
		{9,3,FTsScrReferenceShapeForTests::None},
		{10,0,FTsScrReferenceShapeForTests::Relations2LayoutDependency},
		{10,1,FTsScrReferenceShapeForTests::Relations2LayoutDependency},
		{10,2,FTsScrReferenceShapeForTests::Relations2LayoutDependency},
		{10,3,FTsScrReferenceShapeForTests::Relations2LayoutReflectionDependencies2},
		{10,4,FTsScrReferenceShapeForTests::RelationReflectionDependencies2},
		{10,5,FTsScrReferenceShapeForTests::Relations2LayoutReflectionDependencies2},
		{11,1,FTsScrReferenceShapeForTests::Dependency},
		{11,2,FTsScrReferenceShapeForTests::Dependency},
		{11,3,FTsScrReferenceShapeForTests::RelationDependency},
		{11,4,FTsScrReferenceShapeForTests::PropertyTypeDependency},
		{11,10,FTsScrReferenceShapeForTests::LayoutDependency},
		{12,0,FTsScrReferenceShapeForTests::None},
		{12,1,FTsScrReferenceShapeForTests::RelationDependency},
		{12,2,FTsScrReferenceShapeForTests::Relations3Layouts2ReflectionDependencies3},
		{12,3,FTsScrReferenceShapeForTests::Dependency},
		{12,4,FTsScrReferenceShapeForTests::None},
		{12,5,FTsScrReferenceShapeForTests::Dependency},
		{13,0,FTsScrReferenceShapeForTests::None},
		{14,0,FTsScrReferenceShapeForTests::Dependency},
		{14,1,FTsScrReferenceShapeForTests::Dependency},
		{14,2,FTsScrReferenceShapeForTests::BehaviorDependency},
		{14,3,FTsScrReferenceShapeForTests::Relations2LayoutReflectionDependencies2},
		{14,4,FTsScrReferenceShapeForTests::CombinedFamily14},
	};
	static_assert(UE_ARRAY_COUNT(TsScrReferenceCaseAuthoritiesForTests)
		== TsScrExpectedReferenceCaseCountForTests);

	static_assert([]
	{
		constexpr int32 ExpectedCountsByFamily[] = {
			0, 5, 4, 5, 1, 6, 1, 1, 28, 4, 6, 5, 6, 1, 5};
		int32 CountsByFamily[UE_ARRAY_COUNT(ExpectedCountsByFamily)] = {};
		int32 NoneCount = 0;
		for (int32 RowIndex = 0;
			RowIndex < TsScrExpectedReferenceCaseCountForTests; ++RowIndex)
		{
			const FTsScrReferenceCaseAuthorityForTests& Row =
				TsScrReferenceCaseAuthoritiesForTests[RowIndex];
			if (!IsExpectedTsScrReferenceCaseForTests(Row.Family, Row.Variant)
				|| static_cast<uint8>(Row.Shape)
					> static_cast<uint8>(
						ETsScrReferenceAuthorityShapeForTests::CombinedFamily14))
			{
				return false;
			}
			++CountsByFamily[Row.Family];
			NoneCount += Row.Shape == ETsScrReferenceAuthorityShapeForTests::None;
			for (int32 PreviousIndex = 0; PreviousIndex < RowIndex; ++PreviousIndex)
			{
				const FTsScrReferenceCaseAuthorityForTests& Previous =
					TsScrReferenceCaseAuthoritiesForTests[PreviousIndex];
				if (Previous.Family == Row.Family
					&& Previous.Variant == Row.Variant)
				{
					return false;
				}
			}
		}
		for (int32 Family = 1; Family <= 14; ++Family)
		{
			if (CountsByFamily[Family] != ExpectedCountsByFamily[Family])
			{
				return false;
			}
		}
		return NoneCount == 15;
	}(), "the 78 named reference rows are unique and exactly cover the valid source whitelist");

	static TConstArrayView<FTsScrReferenceCaseAuthorityForTests>
	GetTsScrReferenceCaseAuthorityForTests()
	{
		return MakeArrayView(TsScrReferenceCaseAuthoritiesForTests);
	}

	static TArray<uint64> BuildExpectedStableReferenceOffsetsForTests(
		const FTsScrReferenceCaseAuthorityForTests& Authority,
		const FIndependentTypeSchemaWireInventoryForTests& Inventory)
	{
		TArray<uint64> Result;
		for (const FTsScrReferenceCoordinateAuthorityForTests& Coordinate :
			GetTsScrReferenceCoordinatesForTests(Authority.Shape))
		{
			Result.Add(FindIndependentWireOffsetForTests(Inventory,
				Coordinate.Field, Coordinate.PrimaryIndex,
				Coordinate.SecondaryIndex, Coordinate.TertiaryIndex));
		}
		return Result;
	}

	struct FTsScrStreamingCheckpointCaseForTests
	{
		uint8 Family = 0;
		uint32 Variant = 0;
		ETsScrAllocationSiteKindForTests SiteKind =
			ETsScrAllocationSiteKindForTests::MetadataCanonicalIndexScratch;
		uint32 CheckpointOrdinal = 0;
		EAngelscriptCacheTypeSchemaTestField ExpectedField =
			EAngelscriptCacheTypeSchemaTestField::Metadata;
		int32 PrimaryIndex = INDEX_NONE;
	};

	static TConstArrayView<FTsScrStreamingCheckpointCaseForTests>
	GetTsScrStreamingCheckpointCasesForTests()
	{
		using K = ETsScrAllocationSiteKindForTests;
		using F = EAngelscriptCacheTypeSchemaTestField;
		static const FTsScrStreamingCheckpointCaseForTests Cases[] = {
			{12, 0, K::MetadataCanonicalIndexScratch, 0, F::Metadata},
			{12, 1, K::RelationsCanonicalIndexScratch, 1, F::Relations},
			{12, 2, K::LayoutInputsCanonicalIndexScratch, 2, F::LayoutInputs},
			{12, 3, K::DependenciesCanonicalIndexScratch, 3, F::Dependencies},
			{12, 4, K::EnumMetadataCanonicalIndexScratch, 4, F::KindPayload},
			{12, 5, K::PropertyMetadataCanonicalIndexScratch, 5,
				F::PropertyMetadata, 0},
			{12, 5, K::CombinedCanonicalIndexScratch, 6, F::Dependencies},
			{13, 0, K::PropertyReplayScratch, 7, F::OrderedProperty, 0},
			{14, 0, K::MethodOrdinalScratch, 8, F::OrderedMethods},
			{14, 1, K::VftOrdinalScratch, 9, F::VirtualFunctionTable},
			{14, 2, K::BehaviorOrdinalScratch, 10, F::BehaviorSlots},
			{14, 3, K::UFunctionOrdinalScratch, 11, F::Reflection},
		};
		static_assert(UE_ARRAY_COUNT(Cases) == 12);
		return MakeArrayView(Cases);
	}

	struct FTsScrFilteredProbeEventViewForTests
	{
		TConstArrayView<FAngelscriptCacheTypeSchemaProbeEventForTests> Chronology;
		EAngelscriptCacheTypeSchemaProbeEventKindForTests Kind;

		int32 Num() const
		{
			int32 Count = 0;
			for (const FAngelscriptCacheTypeSchemaProbeEventForTests& Event : Chronology)
			{
				Count += Event.Kind == Kind;
			}
			return Count;
		}

		bool IsValidIndex(const int32 Index) const
		{
			return Index >= 0 && Index < Num();
		}

		const FAngelscriptCacheTypeSchemaProbeEventForTests& FindAtForConstantLookup(
			const int32 Index) const
		{
			int32 MatchingIndex = 0;
			for (const FAngelscriptCacheTypeSchemaProbeEventForTests& Event : Chronology)
			{
				if (Event.Kind != Kind)
				{
					continue;
				}
				if (MatchingIndex++ == Index)
				{
					return Event;
				}
			}
			checkNoEntry();
			return Chronology[0];
		}
	};

	template <int32 Capacity = 4096>
	struct FTsScrCallerOwnedProbeCaptureForTests
	{
		static_assert(Capacity > 0);
		static_assert(std::is_constructible_v<
			FAngelscriptCacheTypeSchemaAllocationProbeForTests,
			TArrayView<FAngelscriptCacheTypeSchemaProbeEventForTests>, int32&, bool&>);
		static_assert(!std::is_default_constructible_v<
			FAngelscriptCacheTypeSchemaAllocationProbeForTests>,
			"the test seam cannot fall back to Runtime-owned growing storage");
		FAngelscriptCacheTypeSchemaProbeEventForTests EventStorage[Capacity];
		int32 EventCount = 0;
		bool bOverflowed = false;
		FAngelscriptCacheTypeSchemaAllocationProbeForTests Probe;

		FTsScrCallerOwnedProbeCaptureForTests()
			: Probe(MakeArrayView(EventStorage), EventCount, bOverflowed)
		{
		}

		TConstArrayView<FAngelscriptCacheTypeSchemaProbeEventForTests>
		GetChronology() const
		{
			return MakeArrayView(EventStorage, EventCount);
		}

		FTsScrFilteredProbeEventViewForTests GetEvents(
			const EAngelscriptCacheTypeSchemaProbeEventKindForTests Kind) const
		{
			return {GetChronology(), Kind};
		}

		FTsScrFilteredProbeEventViewForTests GetAllocationEvents() const
		{
			return GetEvents(
				EAngelscriptCacheTypeSchemaProbeEventKindForTests::Allocation);
		}

		FTsScrFilteredProbeEventViewForTests GetPromotionCheckpoints() const
		{
			return GetEvents(
				EAngelscriptCacheTypeSchemaProbeEventKindForTests::CandidatePromotion);
		}

		FTsScrFilteredProbeEventViewForTests GetValidationCheckpoints() const
		{
			return GetEvents(
				EAngelscriptCacheTypeSchemaProbeEventKindForTests::ValidationCheckpoint);
		}

		FTsScrFilteredProbeEventViewForTests GetInjectedOverflowCheckpoints() const
		{
			return GetEvents(
				EAngelscriptCacheTypeSchemaProbeEventKindForTests::InjectedOverflowCheckpoint);
		}
	};
public:
	TEST_METHOD(StableWireEnumsFlagsAndPayloadVersionAreExplicit)
	{
		static_assert(FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeKind::Class) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeKind::Struct) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeKind::Interface) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeKind::Enum) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeKind::Delegate) == 5);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeKind::Typedef) == 6);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeKind::Funcdef) == 7);

		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::Copy) == 15);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::CopyConstruct) == 16);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::CopyFactory) == 17);
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::KnownMask) == 0xff);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::KnownMask) == 0x3ff);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::HasUnrealProperty) == 0x00001);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::BlueprintReadable) == 0x00002);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::BlueprintWritable) == 0x00004);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::EditableOnDefaults) == 0x00008);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::EditableOnInstance) == 0x00010);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::EditConst) == 0x00020);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::InstancedReference) == 0x00040);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::PersistentInstance) == 0x00080);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::AdvancedDisplay) == 0x00100);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::Transient) == 0x00200);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::Replicated) == 0x00400);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::SkipReplication) == 0x00800);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::SkipSerialization) == 0x01000);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::SaveGame) == 0x02000);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::RepNotify) == 0x04000);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::Config) == 0x08000);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::Interp) == 0x10000);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::AssetRegistrySearchable) == 0x20000);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::NoClear) == 0x40000);
		static_assert(static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::KnownMask) == 0x7ffff);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::None) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::InitialOnly) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::OwnerOnly) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::SkipOwner) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::SimulatedOnly) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::AutonomousOnly) == 5);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::SimulatedOrPhysics) == 6);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::InitialOrOwner) == 7);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::Custom) == 8);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::ReplayOrOwner) == 9);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::ReplayOnly) == 10);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::SimulatedOnlyNoReplay) == 11);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::SimulatedOrPhysicsNoReplay) == 12);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::SkipReplay) == 13);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::Dynamic) == 14);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::Never) == 15);
		static_assert(static_cast<uint8>(EAngelscriptCachedReplicationCondition::NetGroup) == 16);
		static_assert(static_cast<uint8>(EAngelscriptCacheValidationStage::None) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCacheValidationStage::EnvelopeDecode) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCacheValidationStage::PayloadDecode) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCacheValidationStage::LocalSemantic) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCacheValidationStage::OpaqueCodec) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCacheValidationStage::ModuleGraph) == 5);
		static_assert(static_cast<uint8>(EAngelscriptCacheValidationStage::CurrentResolver) == 6);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::Invalid) == 0);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion) == 1);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::ModuleKey) == 2);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::TypeKey) == 3);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::TypeKind) == 4);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::CanonicalNamespace) == 5);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::CanonicalName) == 6);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::CanonicalDeclaration) == 7);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags) == 8);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::Metadata) == 9);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::MetadataEntry) == 10);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::Relation) == 11);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::RelationTarget) == 12);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::LayoutInput) == 13);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::LayoutInputTarget) == 14);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::LayoutExpectation) == 15);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::OrderedProperty) == 16);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::PropertyKey) == 17);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::PropertyType) == 18);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::PropertyMetadata) == 19);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::OrderedMethod) == 20);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::MethodFunction) == 21);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::MethodDeclaringOwner) == 22);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot) == 23);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::VirtualFunction) == 24);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::VirtualDeclaringOwner) == 25);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::VirtualImplementingOwner) == 26);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::BehaviorSlot) == 27);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::BehaviorTarget) == 28);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::BehaviorDeclaringOwner) == 29);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::KindPayload) == 30);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::EnumEnumerator) == 31);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::EnumEnumeratorMetadata) == 32);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::CallableSignature) == 33);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::Reflection) == 34);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::ReflectedFunctionMember) == 35);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::ReflectedFunctionTarget) == 36);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::Dependency) == 37);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::DependencyTarget) == 38);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::ReflectionKind) == 39);
		static_assert(static_cast<uint16>(EAngelscriptTypeSchemaCapturedField::ClassReflectionFlags) == 40);
		static_assert(std::is_same_v<decltype(FAngelscriptTypeSchemaFieldCoordinate{}.PrimaryIndex), uint32>);
		static_assert(FAngelscriptTypeSchemaFieldCoordinate{}.PrimaryIndex == MAX_uint32);
		static_assert(FAngelscriptTypeSchemaFieldCoordinate{}.SecondaryIndex == MAX_uint32);
		static_assert(FAngelscriptTypeSchemaFieldCoordinate{}.TertiaryIndex == MAX_uint32);
	}

	TEST_METHOD(PublicRecordFactoryIsTheOnlyOwningValidatedBoundary)
	{
		using FDecoder = decltype(&FAngelscriptDecodedCacheRecord::TryDecode);
		using FProbeDecoder = decltype(
			&FAngelscriptDecodedCacheRecordTestAccess::TryDecodeWithProbe);
		static_assert(std::is_invocable_r_v<FAngelscriptCacheValidationResult,
			FDecoder, const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			TOptional<FAngelscriptDecodedCacheRecordHandle>&>);
		static_assert(!std::is_invocable_r_v<FAngelscriptCacheValidationResult,
			FDecoder, const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			FAngelscriptCachedTypeSchema&>);
		static_assert(std::is_invocable_r_v<FAngelscriptCacheValidationResult,
			FProbeDecoder, const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			FAngelscriptCacheTypeSchemaAllocationProbeForTests&,
			TOptional<FAngelscriptDecodedCacheRecordHandle>&>);
		static_assert(!std::is_invocable_r_v<FAngelscriptCacheValidationResult,
			FProbeDecoder, const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			TOptional<FAngelscriptDecodedCacheRecordHandle>&>,
			"the test facade requires an explicit caller-owned probe");
		static_assert(std::is_same_v<FAngelscriptDecodedCacheRecordHandle,
			TSharedRef<const FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>>);
		static_assert(!std::is_default_constructible_v<FAngelscriptDecodedCacheRecord>);
		static_assert(!std::is_aggregate_v<FAngelscriptDecodedCacheRecord>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			const FAngelscriptCacheRecordId&>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			const FAngelscriptCachedSourceIndex&>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			const FAngelscriptCachedModuleInterface&>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			const FAngelscriptCachedTypeSchema&>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			const FAngelscriptCachedModuleState&>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			const FAngelscriptCachedFunctionBody&>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			const FAngelscriptCachedDebugSidecar&>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			const FAngelscriptCachedModuleSnapshot&>);
		static_assert(!std::is_constructible_v<FAngelscriptDecodedCacheRecord,
			FAngelscriptCacheRecordId, TArray<uint8>, FAngelscriptCachedTypeSchema>);
		static_assert(!std::is_copy_constructible_v<FAngelscriptDecodedCacheRecord>);
		static_assert(!std::is_move_constructible_v<FAngelscriptDecodedCacheRecord>);
		static_assert(!std::is_copy_assignable_v<FAngelscriptDecodedCacheRecord>);
		static_assert(!std::is_move_assignable_v<FAngelscriptDecodedCacheRecord>);
		static_assert(std::is_destructible_v<FAngelscriptDecodedCacheRecord>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>().GetRecordId()),
			const FAngelscriptCacheRecordId&>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>()
				.GetCanonicalPayload()),
			TConstArrayView<uint8>>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>().TryGetSourceIndex()),
			const FAngelscriptCachedSourceIndex*>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>()
				.TryGetModuleInterface()),
			const FAngelscriptCachedModuleInterface*>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>().TryGetTypeSchema()),
			const FAngelscriptCachedTypeSchema*>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>().TryGetModuleState()),
			const FAngelscriptCachedModuleState*>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>().TryGetFunctionBody()),
			const FAngelscriptCachedFunctionBody*>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>().TryGetDebugSidecar()),
			const FAngelscriptCachedDebugSidecar*>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>()
				.TryGetModuleSnapshot()),
			const FAngelscriptCachedModuleSnapshot*>);
		static_assert(std::is_same_v<
			decltype(std::declval<const FAngelscriptDecodedCacheRecord&>()
				.FindCapturedOffset(std::declval<const FAngelscriptTypeSchemaFieldCoordinate&>())),
			TOptional<uint64>>);
		static_assert(std::is_same_v<decltype(std::declval<const
			FAngelscriptDecodedCacheRecord&>().FindCapturedOffset(std::declval<const
			FAngelscriptSourceIndexFieldCoordinate&>())), TOptional<uint64>>);
		static_assert(std::is_same_v<decltype(std::declval<const
			FAngelscriptDecodedCacheRecord&>().FindCapturedOffset(std::declval<const
			FAngelscriptModuleInterfaceFieldCoordinate&>())), TOptional<uint64>>);
		static_assert(std::is_same_v<decltype(std::declval<const
			FAngelscriptDecodedCacheRecord&>().FindCapturedOffset(std::declval<const
			FAngelscriptModuleStateFieldCoordinate&>())), TOptional<uint64>>);
		static_assert(std::is_same_v<decltype(std::declval<const
			FAngelscriptDecodedCacheRecord&>().FindCapturedOffset(std::declval<const
			FAngelscriptFunctionBodyFieldCoordinate&>())), TOptional<uint64>>);
		static_assert(std::is_same_v<decltype(std::declval<const
			FAngelscriptDecodedCacheRecord&>().FindCapturedOffset(std::declval<const
			FAngelscriptDebugSidecarFieldCoordinate&>())), TOptional<uint64>>);
		static_assert(std::is_same_v<decltype(std::declval<const
			FAngelscriptDecodedCacheRecord&>().FindCapturedOffset(std::declval<const
			FAngelscriptModuleSnapshotFieldCoordinate&>())), TOptional<uint64>>);

		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			MakeCompleteDelegateSchema(), Payload).IsSuccess()));
		FTsScrCallerOwnedProbeCaptureForTests<> AllocationCapture;
		FAngelscriptCacheTypeSchemaAllocationProbeForTests& AllocationProbe =
			AllocationCapture.Probe;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordIdAndProbe(
			Payload, FAngelscriptCacheReadLimits{}, Budget, AllocationProbe,
			Output).IsSuccess()));
		ASSERT_THAT(IsTrue(Output.IsSet()));
		ASSERT_THAT(IsNotNull(Output.GetValue()->TryGetTypeSchema()));
		ASSERT_THAT(IsNull(Output.GetValue()->TryGetSourceIndex()));
		ASSERT_THAT(IsNull(Output.GetValue()->TryGetModuleInterface()));
		ASSERT_THAT(IsNull(Output.GetValue()->TryGetModuleState()));
		ASSERT_THAT(IsNull(Output.GetValue()->TryGetFunctionBody()));
		ASSERT_THAT(IsNull(Output.GetValue()->TryGetDebugSidecar()));
		ASSERT_THAT(IsNull(Output.GetValue()->TryGetModuleSnapshot()));

		FTsScrCallerOwnedProbeCaptureForTests<> PublicUnrelatedCapture;
		FAngelscriptCacheReadBudget PublicBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> PublicOutput;
		const FAngelscriptCacheValidationResult PublicResult =
			DecodeWithMatchingRecordId(Payload, FAngelscriptCacheReadLimits{},
				PublicBudget, PublicOutput);
		ASSERT_THAT(IsTrue(PublicResult.IsSuccess()));
		ASSERT_THAT(IsTrue(PublicOutput.IsSet()));
		ASSERT_THAT(AreEqual(0, PublicUnrelatedCapture.EventCount),
			TEXT("public TryDecode cannot discover or mutate an unrelated caller probe"));
		ASSERT_THAT(IsFalse(PublicUnrelatedCapture.bOverflowed));
		ASSERT_THAT(AreEqual(Hex(Output.GetValue()->GetCanonicalPayload()),
			Hex(PublicOutput.GetValue()->GetCanonicalPayload())));
		ASSERT_THAT(AreEqual(Budget.GetDecodedBytes(), PublicBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetResidentDecodedBytes(),
			PublicBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetTemporaryResidentDecodedBytes(),
			PublicBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetPeakLiveResidentDecodedBytes(),
			PublicBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(Budget.GetReferencesAndRelocations(),
			PublicBudget.GetReferencesAndRelocations()));

		const int32 FirstCaptureEventCount = AllocationCapture.EventCount;
		FTsScrCallerOwnedProbeCaptureForTests<> IndependentCapture;
		FAngelscriptCacheReadBudget IndependentBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> IndependentOutput;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordIdAndProbe(Payload,
			FAngelscriptCacheReadLimits{}, IndependentBudget, IndependentCapture.Probe,
			IndependentOutput).IsSuccess()));
		ASSERT_THAT(IsTrue(IndependentOutput.IsSet()));
		ASSERT_THAT(AreEqual(FirstCaptureEventCount, AllocationCapture.EventCount,
			TEXT("a second explicit probe cannot route events into the first caller view")));
		ASSERT_THAT(AreEqual(FirstCaptureEventCount, IndependentCapture.EventCount));
		ASSERT_THAT(IsFalse(IndependentCapture.bOverflowed));

		using FController = SharedPointerInternals::TIntrusiveReferenceController<
			FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>;
		constexpr uint32 EffectiveControllerAlignment = alignof(FController)
			> __STDCPP_DEFAULT_NEW_ALIGNMENT__
			? alignof(FController)
			: (sizeof(FController) <= 8
				? uint32(8) : uint32(__STDCPP_DEFAULT_NEW_ALIGNMENT__));
		void* IndependentControllerAllocation = FMemory::Malloc(
			sizeof(FController), EffectiveControllerAlignment);
		ASSERT_THAT(IsNotNull(IndependentControllerAllocation));
		const uint64 ActualControllerBytes = static_cast<uint64>(
			FMemory::GetAllocSize(IndependentControllerAllocation));
		FMemory::Free(IndependentControllerAllocation);
		const uint64 QuantizedControllerBytes = static_cast<uint64>(
			FMemory::QuantizeSize(sizeof(FController), EffectiveControllerAlignment));
		const uint64 IndependentControllerBytes =
			ActualControllerBytes >= sizeof(FController)
				? ActualControllerBytes
				: QuantizedControllerBytes;
		ASSERT_THAT(AreEqual(uint64(1),
			AllocationProbe.GetDecodedRecordControllerAllocationCount()));
		ASSERT_THAT(AreEqual(IndependentControllerBytes,
			AllocationProbe.GetDecodedRecordControllerAllocatedBytes()),
			TEXT("the sole handle must use one actual allocator-charged MakeShared controller/object allocation"));
		ASSERT_THAT(IsTrue(AllocationProbe.UsedSingleMakeSharedAllocation()));
		ASSERT_THAT(IsFalse(AllocationCapture.bOverflowed));

		const uint64 AttemptsBeforeCopy = AllocationProbe.GetTotalAllocationAttempts();
		const uint64 AllocatedBytesBeforeCopy = AllocationProbe.GetTotalAllocatedBytes();
		const uint64 DecodedBeforeCopy = Budget.GetDecodedBytes();
		const uint64 ResidentBeforeCopy = Budget.GetResidentDecodedBytes();
		const uint64 ReferencesBeforeCopy = Budget.GetReferencesAndRelocations();
		const FAngelscriptDecodedCacheRecordHandle& Original = Output.GetValue();
		const FAngelscriptDecodedCacheRecord* OriginalAddress = &Original.Get();
		const FAngelscriptCachedTypeSchema* OriginalDtoAddress =
			Original->TryGetTypeSchema();
		const TOptional<uint64> OriginalOffset = Original->FindCapturedOffset({
			EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion});
		const uint8* OwnedPayloadAddress = Original->GetCanonicalPayload().GetData();
		ASSERT_THAT(IsTrue(OwnedPayloadAddress != Payload.GetData()),
			TEXT("the immutable token owns one non-aliasing canonical payload allocation"));
		FAngelscriptDecodedCacheRecordHandle Copy = Original;
		ASSERT_THAT(IsTrue(&Copy.Get() == &Original.Get()));
		ASSERT_THAT(IsTrue(Copy->TryGetTypeSchema() == OriginalDtoAddress));
		ASSERT_THAT(AreEqual(OriginalOffset.GetValue(),
			Copy->FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion}).GetValue()));
		ASSERT_THAT(AreEqual(AttemptsBeforeCopy,
			AllocationProbe.GetTotalAllocationAttempts()));
		ASSERT_THAT(AreEqual(AllocatedBytesBeforeCopy,
			AllocationProbe.GetTotalAllocatedBytes()));
		ASSERT_THAT(AreEqual(DecodedBeforeCopy, Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(ResidentBeforeCopy, Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(ReferencesBeforeCopy,
			Budget.GetReferencesAndRelocations()),
			TEXT("a shared handle copy is reference-count-only and must not charge any budget"));
		Output.Reset();
		ASSERT_THAT(IsTrue(&Copy.Get() == OriginalAddress));
		ASSERT_THAT(IsTrue(Copy->GetCanonicalPayload().GetData() == OwnedPayloadAddress));
		ASSERT_THAT(AreEqual(Hex(Payload), Hex(Copy->GetCanonicalPayload())),
			TEXT("the copied handle keeps the token-owned canonical payload alive after the original optional resets"));
		ASSERT_THAT(AreEqual(AttemptsBeforeCopy,
			AllocationProbe.GetTotalAllocationAttempts()));
		ASSERT_THAT(AreEqual(AllocatedBytesBeforeCopy,
			AllocationProbe.GetTotalAllocatedBytes()));
	}

	TEST_METHOD(StableKeyEqualityIsFullWidthAndDomainIsolated)
	{
		using FModuleKey = FAngelscriptStableModuleKey;
		using FTypeKey = FAngelscriptStableTypeKey;
		using FFunctionKey = FAngelscriptStableFunctionKey;
		using FPropertyKey = FAngelscriptStablePropertyKey;
		static_assert(THasTypedEquality<FModuleKey, FModuleKey>::value);
		static_assert(THasTypedEquality<FTypeKey, FTypeKey>::value);
		static_assert(THasTypedEquality<FFunctionKey, FFunctionKey>::value);
		static_assert(THasTypedEquality<FPropertyKey, FPropertyKey>::value);
		static_assert(!THasTypedEquality<FModuleKey, FTypeKey>::value);
		static_assert(!THasTypedEquality<FTypeKey, FModuleKey>::value);
		static_assert(!THasTypedEquality<FModuleKey, FFunctionKey>::value);
		static_assert(!THasTypedEquality<FFunctionKey, FModuleKey>::value);
		static_assert(!THasTypedEquality<FModuleKey, FPropertyKey>::value);
		static_assert(!THasTypedEquality<FPropertyKey, FModuleKey>::value);
		static_assert(!THasTypedEquality<FTypeKey, FFunctionKey>::value);
		static_assert(!THasTypedEquality<FFunctionKey, FTypeKey>::value);
		static_assert(!THasTypedEquality<FTypeKey, FPropertyKey>::value);
		static_assert(!THasTypedEquality<FPropertyKey, FTypeKey>::value);
		static_assert(!THasTypedEquality<FFunctionKey, FPropertyKey>::value);
		static_assert(!THasTypedEquality<FPropertyKey, FFunctionKey>::value);
		static_assert(!std::is_convertible_v<FModuleKey, FTypeKey>);
		static_assert(!std::is_convertible_v<FTypeKey, FFunctionKey>);
		static_assert(!std::is_convertible_v<FFunctionKey, FPropertyKey>);
		static_assert(!std::is_convertible_v<FPropertyKey, FModuleKey>);

		const FAngelscriptHash256 EqualHash = MakeLateByteHash(0xa5, 0x01);
		const FAngelscriptHash256 LateDifferentHash = MakeLateByteHash(0xa5, 0x02);
		const FModuleKey ModuleA{EqualHash};
		const FModuleKey ModuleAEqual{EqualHash};
		const FModuleKey ModuleLateDifferent{LateDifferentHash};
		const FTypeKey TypeA{EqualHash};
		const FTypeKey TypeAEqual{EqualHash};
		const FTypeKey TypeLateDifferent{LateDifferentHash};
		const FFunctionKey FunctionA{EqualHash};
		const FFunctionKey FunctionAEqual{EqualHash};
		const FFunctionKey FunctionLateDifferent{LateDifferentHash};
		const FPropertyKey PropertyA{EqualHash};
		const FPropertyKey PropertyAEqual{EqualHash};
		const FPropertyKey PropertyLateDifferent{LateDifferentHash};
		ASSERT_THAT(IsTrue(ModuleA == ModuleAEqual));
		ASSERT_THAT(IsTrue(ModuleA != ModuleLateDifferent));
		ASSERT_THAT(IsTrue(TypeA == TypeAEqual));
		ASSERT_THAT(IsTrue(TypeA != TypeLateDifferent));
		ASSERT_THAT(IsTrue(FunctionA == FunctionAEqual));
		ASSERT_THAT(IsTrue(FunctionA != FunctionLateDifferent));
		ASSERT_THAT(IsTrue(PropertyA == PropertyAEqual));
		ASSERT_THAT(IsTrue(PropertyA != PropertyLateDifferent));
	}

	TEST_METHOD(AllTypeSchemaRawEnumsBooleansAndOptionalTagsAreExhausted)
	{
		const auto AssertEnumDomain = [&](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCacheTypeSchemaTestField Field,
			const uint8 FirstLegal,
			const uint8 LastLegal,
			const int32 PrimaryIndex = INDEX_NONE,
			const int32 SecondaryIndex = INDEX_NONE)
		{
			TArray<uint8> BasePayload;
			FAngelscriptCacheTypeSchemaTestWireTrace Trace;
			ASSERT_THAT(IsTrue(
				FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
					Schema, BasePayload, Trace).IsSuccess()));
			const uint64 Offset = RequireIndependentSpanOffsetForTests(
				BasePayload, Field, PrimaryIndex, SecondaryIndex);
			for (uint32 Raw = FirstLegal; Raw <= LastLegal; ++Raw)
			{
				TArray<uint8> Payload = BasePayload;
				PatchRawByte(Payload, Trace, Field, static_cast<uint8>(Raw),
					PrimaryIndex, SecondaryIndex);
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
				const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
					Payload, Limits, Budget, Output);
				ASSERT_THAT(IsFalse(Result.Error
					== EAngelscriptCacheValidationError::UnknownEnumValue
					&& Result.Stage == EAngelscriptCacheValidationStage::PayloadDecode
					&& Result.ByteOffset == Offset),
					*FString::Printf(TEXT("raw enum value %u was rejected by physical decode"), Raw));
			}
			const uint8 InvalidValues[] = {
				static_cast<uint8>(FirstLegal == 0 ? LastLegal + 1 : 0),
				static_cast<uint8>(LastLegal + 1),
				0xff,
			};
			for (const uint8 Raw : InvalidValues)
			{
				if (Raw >= FirstLegal && Raw <= LastLegal)
				{
					continue;
				}
				TArray<uint8> Payload = BasePayload;
				PatchRawByte(Payload, Trace, Field, Raw, PrimaryIndex, SecondaryIndex);
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
				const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
					Payload, Limits, Budget, Output);
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
					Result, Output, EAngelscriptCacheValidationError::UnknownEnumValue,
					EAngelscriptCacheValidationStage::PayloadDecode, Offset,
					TEXT("raw enum outside frozen domain"))));
			}
		};

		const FAngelscriptCachedTypeSchema Delegate = MakeCompleteDelegateSchema();
		AssertEnumDomain(Delegate,
			EAngelscriptCacheTypeSchemaTestField::TypeKind, 1, 7);
		AssertEnumDomain(MakeOrdinaryUClassSchema(true),
			EAngelscriptCacheTypeSchemaTestField::RelationKind, 1, 5, 0);
		AssertEnumDomain(Delegate,
			EAngelscriptCacheTypeSchemaTestField::PropertyMemberAccess, 1, 3, 0);
		AssertEnumDomain(Delegate,
			EAngelscriptCacheTypeSchemaTestField::MethodSlotKind, 1, 4, 0);
		AssertEnumDomain(Delegate,
			EAngelscriptCacheTypeSchemaTestField::BehaviorKind, 1, 17, 0);
		AssertEnumDomain(Delegate,
			EAngelscriptCacheTypeSchemaTestField::ReflectionKind, 1, 5);
		AssertEnumDomain(Delegate,
			EAngelscriptCacheTypeSchemaTestField::PropertyStorageKind, 1, 2, 0);
		AssertEnumDomain(MakeOrdinaryUClassSchema(true),
			EAngelscriptCacheTypeSchemaTestField::LayoutInputKind, 1, 3, 0);
		AssertEnumDomain(Delegate,
			EAngelscriptCacheTypeSchemaTestField::PropertyReplicationCondition,
			0, 16, 0);

		const auto AssertRawTag = [&](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCacheTypeSchemaTestField Field,
			const EAngelscriptCacheValidationError Expected,
			const int32 PrimaryIndex = INDEX_NONE,
			const int32 SecondaryIndex = INDEX_NONE)
		{
			TArray<uint8> BasePayload;
			FAngelscriptCacheTypeSchemaTestWireTrace Trace;
			ASSERT_THAT(IsTrue(
				FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
					Schema, BasePayload, Trace).IsSuccess()));
			for (const uint8 Raw : {uint8(2), uint8(0xff)})
			{
				TArray<uint8> Payload = BasePayload;
				PatchRawByte(Payload, Trace, Field, Raw,
					PrimaryIndex, SecondaryIndex);
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
				const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
					Payload, Limits, Budget, Output);
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
					Result, Output, Expected,
					EAngelscriptCacheValidationStage::PayloadDecode,
					RequireIndependentSpanOffsetForTests(
						BasePayload, Field, PrimaryIndex, SecondaryIndex),
					TEXT("raw boolean/optional tag outside canonical 0/1"))));
			}
		};
		AssertRawTag(Delegate,
			EAngelscriptCacheTypeSchemaTestField::CallableMulticastBoolean,
			EAngelscriptCacheValidationError::InvalidBoolean);
		AssertRawTag(MakeOrdinaryUClassSchema(true),
			EAngelscriptCacheTypeSchemaTestField::RelationSemanticOrdinalOptionalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag, 0);
		AssertRawTag(MakeOrdinaryUClassSchema(true),
			EAngelscriptCacheTypeSchemaTestField::LayoutInputBoundaryOptionalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag, 0);
		AssertRawTag(MakeOrdinaryUClassSchema(true),
			EAngelscriptCacheTypeSchemaTestField::LayoutInputAlignmentOptionalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag, 0);
		FAngelscriptCachedTypeSchema ConfigPresent = MakeOrdinaryUClassSchema(false);
		ConfigPresent.Reflection.ConfigName = TEXT("Game");
		FinalizeValidFixtureHashes(ConfigPresent);
		AssertRawTag(ConfigPresent,
			EAngelscriptCacheTypeSchemaTestField::ReflectionConfigNameOptionalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag);
		AssertRawTag(MakeOrdinaryUClassSchema(false),
			EAngelscriptCacheTypeSchemaTestField::ReflectionStaticClassGlobalNameOptionalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag);
		AssertRawTag(Delegate,
			EAngelscriptCacheTypeSchemaTestField::DataTypeTypeReferenceOptionalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag, 0, 0);
		AssertRawTag(MakeOrdinaryUClassSchema(false),
			EAngelscriptCacheTypeSchemaTestField::DependencyExpectedContentOrValueOptionalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag, 0);
		AssertRawTag(Delegate,
			EAngelscriptCacheTypeSchemaTestField::BehaviorDeclaringOwnerOptionalTag,
			EAngelscriptCacheValidationError::InvalidOptionalTag, 0);

		const auto AssertCanonicalOptionalFixture = [&](const FAngelscriptCachedTypeSchema& Schema,
			const TCHAR* Context)
		{
			TArray<uint8> Payload;
			ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, Payload).IsSuccess()), Context);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
			ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(Payload,
				FAngelscriptCacheReadLimits{}, Budget, Output).IsSuccess()), Context);
			ASSERT_THAT(IsTrue(Output.IsSet()), Context);
		};
		AssertCanonicalOptionalFixture(MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Struct), TEXT("raw optional 0: all absent"));
		AssertCanonicalOptionalFixture(ConfigPresent,
			TEXT("raw optional 1: reflection ConfigName present"));
		AssertCanonicalOptionalFixture(MakeOrdinaryUClassSchema(false),
			TEXT("raw optional 1: required StaticClassGlobalName present"));
		AssertCanonicalOptionalFixture(MakeRecursivePropertyTypeSchema(),
			TEXT("raw optional 0/1: recursive TypeReference absent and present nodes"));
	}

	TEST_METHOD(EveryUnknownFlagBitFailsAtItsOwningField)
	{
		const auto AssertUnknownBit = [&](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCacheTypeSchemaTestField Field,
			const uint32 Bit,
			const int32 Primary = INDEX_NONE,
			const int32 Secondary = INDEX_NONE)
		{
			TArray<uint8> Payload;
			FAngelscriptCacheTypeSchemaTestWireTrace Trace;
			ASSERT_THAT(IsTrue(
				FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
					Schema, Payload, Trace).IsSuccess()));
			PatchRawUInt32(Payload, Trace, Field, Bit, Primary, Secondary);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
			const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
				Payload, FAngelscriptCacheReadLimits{}, Budget, Output);
			const bool bNestedPropertyField =
				Field == EAngelscriptCacheTypeSchemaTestField::DataTypeQualifierFlags;
			const bool bPropertyField = bNestedPropertyField
				|| Field == EAngelscriptCacheTypeSchemaTestField::PropertySemanticFlags;
			const EAngelscriptCacheTypeSchemaTestField ExpectedField = bPropertyField
				? EAngelscriptCacheTypeSchemaTestField::OrderedProperty : Field;
			const int32 ExpectedPrimary = Primary;
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Output,
				EAngelscriptCacheValidationError::UnknownFlags,
				EAngelscriptCacheValidationStage::LocalSemantic,
				RequireIndependentSpanOffsetForTests(
					Payload, ExpectedField, ExpectedPrimary),
				TEXT("individual unknown high flag bit"))));
		};

		for (uint32 BitIndex = 8; BitIndex < 32; ++BitIndex)
		{
			AssertUnknownBit(MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct),
				EAngelscriptCacheTypeSchemaTestField::TypeSemanticFlags,
				uint32(1) << BitIndex);
		}
		for (uint32 BitIndex = 10; BitIndex < 32; ++BitIndex)
		{
			AssertUnknownBit(MakeOrdinaryUClassSchema(false),
				EAngelscriptCacheTypeSchemaTestField::ClassReflectionFlags,
				uint32(1) << BitIndex);
		}
		for (uint32 BitIndex = 19; BitIndex < 32; ++BitIndex)
		{
			AssertUnknownBit(MakePropertyOwnerFixture(
				EPropertyOwnerFormForTests::OrdinaryUClass),
				EAngelscriptCacheTypeSchemaTestField::PropertySemanticFlags,
				uint32(1) << BitIndex, 0);
		}
		for (uint32 BitIndex = 6; BitIndex < 32; ++BitIndex)
		{
			AssertUnknownBit(MakeRecursivePropertyTypeSchema(),
				EAngelscriptCacheTypeSchemaTestField::DataTypeQualifierFlags,
				uint32(1) << BitIndex, 1, 0);
		}
	}

	TEST_METHOD(IndependentRawWireScannerCoversAllFieldsAndEveryUnionArm)
	{
		bool bAllFieldsSuccess = false;
		bool bTypedefSuccess = false;
		TArray<FAngelscriptCachedTypeSchema> Fixtures = {
			MakeTypeSchemaAllocationFixture(14, 3, 4, bAllFieldsSuccess),
			MakeRecursivePropertyTypeSchema(),
			MakeEnumSchema(),
			MakeCompleteDelegateSchema(),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef),
			MakeTypeSchemaAllocationFixture(9, 3, 3, bTypedefSuccess),
			MakeReflectedUStructSchema(),
		};
		ASSERT_THAT(IsTrue(bAllFieldsSuccess));
		ASSERT_THAT(IsTrue(bTypedefSuccess));
		for (const FAngelscriptCachedTypeSchema& Schema : Fixtures)
		{
			TArray<uint8> Payload;
			FAngelscriptCacheTypeSchemaTestWireTrace Trace;
			ASSERT_THAT(IsTrue(
				FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
					Schema, Payload, Trace).IsSuccess()));
			const FIndependentTypeSchemaWireInventoryForTests Expected =
				ScanIndependentTypeSchemaWireForTests(Payload);
			ASSERT_THAT(IsTrue(Expected.bComplete));
			ASSERT_THAT(AreEqual(uint64(Payload.Num()), Expected.ConsumedBytes));
			ASSERT_THAT(AreEqual(Expected.WireSpans.Num(), Trace.GetAllV1Spans().Num()),
				TEXT("raw scanner and writer trace cannot co-omit or add a V1 field"));
			for (const FIndependentTypeSchemaWireSpanForTests& ExpectedSpan :
				Expected.WireSpans)
			{
				const TOptional<FAngelscriptCacheTypeSchemaTestWireSpan> Actual =
					Trace.FindUnique(ExpectedSpan.Field, ExpectedSpan.PrimaryIndex,
						ExpectedSpan.SecondaryIndex, ExpectedSpan.TertiaryIndex);
				ASSERT_THAT(IsTrue(Actual.IsSet()),
					TEXT("every independently scanned Field/P/S/T span exists exactly once"));
				if (Actual.IsSet())
				{
					ASSERT_THAT(AreEqual(ExpectedSpan.ExactOffset, Actual->Offset));
					ASSERT_THAT(AreEqual(ExpectedSpan.ExactSize, Actual->Size));
				}
			}
		}
	}

	TEST_METHOD(TypeSchemaCapturedCoordinateMatrixZeroThroughFortyThreeIsExactAndFree)
	{
		struct FDecodedFixture
		{
			TArray<uint8> Payload;
			FIndependentTypeSchemaWireInventoryForTests Inventory;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Handle;
		};
		const auto DecodeFixture = [&](const FAngelscriptCachedTypeSchema& Schema)
			-> FDecodedFixture
		{
			FDecodedFixture Fixture;
			if (!FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, Fixture.Payload).IsSuccess())
			{
				TestRunner->AddError(TEXT("DecodeFixture serializer setup failed"));
			}
			Fixture.Inventory = ScanIndependentTypeSchemaWireForTests(Fixture.Payload);
			if (!Fixture.Inventory.bComplete)
			{
				TestRunner->AddError(TEXT("DecodeFixture raw inventory setup failed"));
			}
			FAngelscriptCacheReadBudget Budget;
			if (!DecodeWithMatchingRecordId(Fixture.Payload,
				FAngelscriptCacheReadLimits{}, Budget, Fixture.Handle).IsSuccess())
			{
				TestRunner->AddError(TEXT("DecodeFixture production decode setup failed"));
			}
			if (!Fixture.Handle.IsSet())
			{
				TestRunner->AddError(TEXT("DecodeFixture did not publish a handle"));
			}
			return Fixture;
		};

		bool bAllFieldsSuccess = false;
		const FDecodedFixture AllFields = DecodeFixture(
			MakeTypeSchemaAllocationFixture(14, 3, 4, bAllFieldsSuccess));
		ASSERT_THAT(IsTrue(bAllFieldsSuccess));
		const FDecodedFixture Properties = DecodeFixture(MakeRecursivePropertyTypeSchema());
		const FDecodedFixture Enum = DecodeFixture(MakeEnumSchema());
		const FDecodedFixture Callable = DecodeFixture(MakeCompleteDelegateSchema());
		const FDecodedFixture Empty = DecodeFixture(
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Class));
		const FDecodedFixture* Fixtures[] = {
			&AllFields, &Properties, &Enum, &Callable, &Empty,
		};

		struct FMatrixRow
		{
			EAngelscriptTypeSchemaCapturedField Field;
			uint8 FixtureIndex;
			uint32 Primary = MAX_uint32;
			uint32 Secondary = MAX_uint32;
			uint32 Tertiary = MAX_uint32;
		};
		const FMatrixRow Rows[] = {
			{EAngelscriptTypeSchemaCapturedField::Invalid, 0},
			{EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion, 0},
			{EAngelscriptTypeSchemaCapturedField::ModuleKey, 0},
			{EAngelscriptTypeSchemaCapturedField::TypeKey, 0},
			{EAngelscriptTypeSchemaCapturedField::TypeKind, 0},
			{EAngelscriptTypeSchemaCapturedField::CanonicalNamespace, 0},
			{EAngelscriptTypeSchemaCapturedField::CanonicalName, 0},
			{EAngelscriptTypeSchemaCapturedField::CanonicalDeclaration, 0},
			{EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags, 0},
			{EAngelscriptTypeSchemaCapturedField::Metadata, 3},
			{EAngelscriptTypeSchemaCapturedField::MetadataEntry, 3, 0},
			{EAngelscriptTypeSchemaCapturedField::Relation, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::RelationTarget, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::LayoutInput, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::LayoutInputTarget, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::LayoutExpectation, 0},
			{EAngelscriptTypeSchemaCapturedField::OrderedProperty, 1, 0},
			{EAngelscriptTypeSchemaCapturedField::PropertyKey, 1, 0},
			{EAngelscriptTypeSchemaCapturedField::PropertyType, 1, 1, 0},
			{EAngelscriptTypeSchemaCapturedField::PropertyMetadata, 1, 1, 0},
			{EAngelscriptTypeSchemaCapturedField::OrderedMethod, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::MethodFunction, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::MethodDeclaringOwner, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::VirtualFunctionSlot, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::VirtualFunction, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::VirtualDeclaringOwner, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::VirtualImplementingOwner, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::BehaviorSlot, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::BehaviorTarget, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::BehaviorDeclaringOwner, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::KindPayload, 2},
			{EAngelscriptTypeSchemaCapturedField::EnumEnumerator, 2, 0},
			{EAngelscriptTypeSchemaCapturedField::EnumEnumeratorMetadata, 2, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::CallableSignature, 3},
			{EAngelscriptTypeSchemaCapturedField::Reflection, 0},
			{EAngelscriptTypeSchemaCapturedField::ReflectedFunctionMember, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::ReflectedFunctionTarget, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::Dependency, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::DependencyTarget, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::ReflectionKind, 0},
			{EAngelscriptTypeSchemaCapturedField::ClassReflectionFlags, 0},
			{EAngelscriptTypeSchemaCapturedField::ReflectedFunctionName, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::ReflectedOriginalFunctionName, 0, 0},
			{EAngelscriptTypeSchemaCapturedField::ReflectedScriptFunctionName, 0, 0},
		};
		static_assert(UE_ARRAY_COUNT(Rows) == 44);

		for (uint32 RawField = 0; RawField < UE_ARRAY_COUNT(Rows); ++RawField)
		{
			const FMatrixRow& Row = Rows[RawField];
			ASSERT_THAT(AreEqual(RawField, uint32(Row.Field)),
				TEXT("matrix remains exact, append-only and numerically contiguous"));
			const FDecodedFixture& Fixture = *Fixtures[Row.FixtureIndex];
			const FAngelscriptTypeSchemaFieldCoordinate Coordinate{
				Row.Field, Row.Primary, Row.Secondary, Row.Tertiary};
			const TOptional<uint64> Expected =
				Fixture.Inventory.FindCapturedOffset(Coordinate);
			FAngelscriptCacheReadLimits LookupLimits;
			FAngelscriptCacheReadBudget LookupBudget;
			ASSERT_THAT(IsTrue(LookupBudget.TryConsumeRetainedDecoded(7, LookupLimits)));
			FAngelscriptCacheTemporaryResidentReservation LookupReservation;
			ASSERT_THAT(IsTrue(LookupBudget.TryReserveTemporaryDecoded(
				3, LookupLimits, LookupReservation)));
			ASSERT_THAT(IsTrue(LookupBudget.TryConsumeReferencesAndRelocations(
				5, LookupLimits)));
			const uint64 DecodedBefore = LookupBudget.GetDecodedBytes();
			const uint64 ResidentBefore = LookupBudget.GetResidentDecodedBytes();
			const uint64 TemporaryBefore =
				LookupBudget.GetTemporaryResidentDecodedBytes();
			const uint64 PeakBefore =
				LookupBudget.GetPeakLiveResidentDecodedBytes();
			const uint64 ReferencesBefore =
				LookupBudget.GetReferencesAndRelocations();
			FTsScrCallerOwnedProbeCaptureForTests<> LookupCapture;
			FAngelscriptCacheTypeSchemaAllocationProbeForTests& LookupProbe =
				LookupCapture.Probe;
			const uint64 AttemptsBefore = LookupProbe.GetTotalAllocationAttempts();
			const uint64 BytesBefore = LookupProbe.GetTotalAllocatedBytes();
			const TOptional<uint64> Actual =
				Fixture.Handle.GetValue()->FindCapturedOffset(Coordinate);
			ASSERT_THAT(AreEqual(Expected.IsSet(), Actual.IsSet()));
			if (RawField == 0)
			{
				ASSERT_THAT(IsFalse(Actual.IsSet()));
			}
			else
			{
				ASSERT_THAT(IsTrue(Expected.IsSet()));
				ASSERT_THAT(AreEqual(Expected.GetValue(), Actual.GetValue()));
			}
			ASSERT_THAT(AreEqual(AttemptsBefore,
				LookupProbe.GetTotalAllocationAttempts()));
			ASSERT_THAT(AreEqual(BytesBefore, LookupProbe.GetTotalAllocatedBytes()),
				TEXT("public captured lookup is allocation-free and Budget-free"));
			ASSERT_THAT(AreEqual(DecodedBefore, LookupBudget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(ResidentBefore,
				LookupBudget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(TemporaryBefore,
				LookupBudget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(PeakBefore,
				LookupBudget.GetPeakLiveResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(ReferencesBefore,
				LookupBudget.GetReferencesAndRelocations()));
			ASSERT_THAT(AreEqual(0, LookupCapture.EventCount),
				TEXT("public lookup cannot route events into an unrelated caller probe"));
			ASSERT_THAT(IsFalse(LookupCapture.bOverflowed));

			if (RawField == 0)
			{
				continue;
			}
			FAngelscriptTypeSchemaFieldCoordinate Surplus = Coordinate;
			if (Row.Primary == MAX_uint32)
			{
				Surplus.PrimaryIndex = 0;
			}
			else if (Row.Secondary == MAX_uint32)
			{
				Surplus.SecondaryIndex = 0;
			}
			else
			{
				Surplus.TertiaryIndex = 0;
			}
			ASSERT_THAT(IsFalse(Fixture.Handle.GetValue()->FindCapturedOffset(
				Surplus).IsSet()), TEXT("every unconsumed index must remain U"));
			if (Row.Primary != MAX_uint32)
			{
				FAngelscriptTypeSchemaFieldCoordinate Missing = Coordinate;
				Missing.PrimaryIndex = MAX_uint32;
				ASSERT_THAT(IsFalse(Fixture.Handle.GetValue()->FindCapturedOffset(
					Missing).IsSet()));
				Missing.PrimaryIndex = MAX_uint32 - 1;
				ASSERT_THAT(IsFalse(Fixture.Handle.GetValue()->FindCapturedOffset(
					Missing).IsSet()));
			}
			if (Row.Secondary != MAX_uint32)
			{
				FAngelscriptTypeSchemaFieldCoordinate Missing = Coordinate;
				Missing.SecondaryIndex = MAX_uint32;
				ASSERT_THAT(IsFalse(Fixture.Handle.GetValue()->FindCapturedOffset(
					Missing).IsSet()));
				Missing.SecondaryIndex = MAX_uint32 - 1;
				ASSERT_THAT(IsFalse(Fixture.Handle.GetValue()->FindCapturedOffset(
					Missing).IsSet()));
			}
			const bool bDynamicOrSelectedField =
				(RawField >= 10 && RawField <= 14)
				|| (RawField >= 16 && RawField <= 29)
				|| (RawField >= 31 && RawField <= 33)
				|| (RawField >= 35 && RawField <= 38)
				|| (RawField >= 41 && RawField <= 43);
			if (bDynamicOrSelectedField)
			{
				ASSERT_THAT(IsFalse(Empty.Handle.GetValue()->FindCapturedOffset(
					Coordinate).IsSet()),
					TEXT("every dynamic/selected coordinate has an inapplicable empty-row control"));
			}
			ASSERT_THAT(AreEqual(AttemptsBefore,
				LookupProbe.GetTotalAllocationAttempts()));
			ASSERT_THAT(AreEqual(BytesBefore, LookupProbe.GetTotalAllocatedBytes()));
			ASSERT_THAT(AreEqual(DecodedBefore, LookupBudget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(ResidentBefore,
				LookupBudget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(TemporaryBefore,
				LookupBudget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(PeakBefore,
				LookupBudget.GetPeakLiveResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(ReferencesBefore,
				LookupBudget.GetReferencesAndRelocations()));
		}

		ASSERT_THAT(AreEqual(uint64(0), AllFields.Handle.GetValue()->FindCapturedOffset(
			{EAngelscriptTypeSchemaCapturedField::PayloadSchemaVersion}).GetValue()),
			TEXT("captured offset zero is a set value"));
		bool bFoundValidAbsentOwner = false;
		for (uint32 Variant = 0;
			Variant < GetTypeSchemaAllocationVariantCount(8); ++Variant)
		{
			bool bExpectedLocalSuccess = false;
			const FAngelscriptCachedTypeSchema AbsentOwnerSchema =
				MakeTypeSchemaAllocationFixture(8, 1, Variant, bExpectedLocalSuccess);
			if (!bExpectedLocalSuccess || AbsentOwnerSchema.OrderedBehaviorSlots.IsEmpty()
				|| AbsentOwnerSchema.OrderedBehaviorSlots[0].DeclaringOwner.IsSet())
			{
				continue;
			}
			const FDecodedFixture AbsentOwner = DecodeFixture(AbsentOwnerSchema);
			ASSERT_THAT(IsFalse(AbsentOwner.Handle.GetValue()->FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::BehaviorDeclaringOwner, 0}).IsSet()),
				TEXT("field 29 consumes BehaviorOrdinal/U/U and absent optional is unset"));
			bFoundValidAbsentOwner = true;
			break;
		}
		ASSERT_THAT(IsTrue(bFoundValidAbsentOwner));
		ASSERT_THAT(IsFalse(Callable.Handle.GetValue()->FindCapturedOffset({
			EAngelscriptTypeSchemaCapturedField::EnumEnumerator, 0}).IsSet()));
		ASSERT_THAT(IsFalse(Enum.Handle.GetValue()->FindCapturedOffset({
			EAngelscriptTypeSchemaCapturedField::CallableSignature}).IsSet()));
		ASSERT_THAT(IsFalse(AllFields.Handle.GetValue()->FindCapturedOffset({
			static_cast<EAngelscriptTypeSchemaCapturedField>(41)}).IsSet()));
		ASSERT_THAT(IsFalse(AllFields.Handle.GetValue()->FindCapturedOffset(
			MakeWrongRecordKindPayloadVersionCoordinateForTests<
				FAngelscriptSourceIndexFieldCoordinate>()).IsSet()));
		ASSERT_THAT(IsFalse(AllFields.Handle.GetValue()->FindCapturedOffset(
			MakeWrongRecordKindPayloadVersionCoordinateForTests<
				FAngelscriptModuleInterfaceFieldCoordinate>()).IsSet()));
		// The remaining four overload shapes are frozen by the compile-time
		// invocability assertions above. Their concrete coordinate enums are not
		// yet authoritative here, so this test does not invent raw-value aliases.
	}

	TEST_METHOD(EveryIndependentRawWireBoundaryIsOneByteTruncatedThroughFactory)
	{
		bool bAllFieldsSuccess = false;
		bool bTypedefSuccess = false;
		const TArray<FAngelscriptCachedTypeSchema> Fixtures = {
			MakeTypeSchemaAllocationFixture(14, 3, 4, bAllFieldsSuccess),
			MakeRecursivePropertyTypeSchema(), MakeEnumSchema(),
			MakeCompleteDelegateSchema(),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef),
			MakeTypeSchemaAllocationFixture(9, 3, 3, bTypedefSuccess),
			MakeReflectedUStructSchema(),
		};
		ASSERT_THAT(IsTrue(bAllFieldsSuccess));
		ASSERT_THAT(IsTrue(bTypedefSuccess));
		for (const FAngelscriptCachedTypeSchema& Schema : Fixtures)
		{
			TArray<uint8> Payload;
			ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, Payload).IsSuccess()));
			const FIndependentTypeSchemaWireInventoryForTests Inventory =
				ScanIndependentTypeSchemaWireForTests(Payload);
			ASSERT_THAT(IsTrue(Inventory.bComplete));
			for (const FIndependentTypeSchemaWireBoundaryForTests& Boundary :
				Inventory.PhysicalBoundaries)
			{
				const uint64 Cut = Boundary.ExactOffset + Boundary.ExactSize - 1;
				TArray<uint8> Truncated = Payload;
				Truncated.SetNum(static_cast<int32>(Cut), EAllowShrinking::No);
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
				const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
					Truncated, FAngelscriptCacheReadLimits{}, Budget, Output);
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Output,
					EAngelscriptCacheValidationError::OutOfBounds,
					EAngelscriptCacheValidationStage::PayloadDecode, Cut,
					TEXT("one-byte truncation from independent raw scanner boundary"))));
			}
		}
	}
	TEST_METHOD(DeclaredRecordIdMismatchRejectsBeforeDispatchAndClearsHandle)
	{
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			MakeCompleteDelegateSchema(), Payload).IsSuccess()));
		FAngelscriptCacheRecordId WrongId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::TypeSchema, Payload, WrongId).IsSuccess()));
		WrongId.ContentHash = MakeHash(0xee);
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptDecodedCacheRecord::TryDecode(
				WrongId, Payload, Limits, Budget, Output);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Output,
			EAngelscriptCacheValidationError::RecordIdMismatch,
			EAngelscriptCacheValidationStage::PayloadDecode,
			uint64(0), TEXT("declared RecordId mismatch before dispatch"))));
	}

	TEST_METHOD(AliasedPayloadViewIsKeptAliveWhileAtomicOutputIsCleared)
	{
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			MakeCompleteDelegateSchema(), Payload).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget SeedBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Payload, Limits, SeedBudget, Output).IsSuccess()));
		ASSERT_THAT(IsTrue(Output.IsSet()));

		FAngelscriptCacheRecordId DeclaredId = Output.GetValue()->GetRecordId();
		const TConstArrayView<uint8> AliasedFailurePayload =
			Output.GetValue()->GetCanonicalPayload();
		DeclaredId.ContentHash = MakeHash(0xee);
		FAngelscriptCacheReadBudget FailureBudget;
		const FAngelscriptCacheValidationResult Failure =
			FAngelscriptDecodedCacheRecord::TryDecode(
				DeclaredId, AliasedFailurePayload, Limits, FailureBudget, Output);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Failure, Output,
			EAngelscriptCacheValidationError::RecordIdMismatch,
			EAngelscriptCacheValidationStage::PayloadDecode,
			uint64(0), TEXT("aliased old output payload RecordId mismatch"))));
		ASSERT_THAT(AreEqual(uint64(0), FailureBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0), FailureBudget.GetResidentDecodedBytes()),
			TEXT("the input lifetime guard is only a ThreadSafe refcount copy"));

		FAngelscriptCacheReadBudget SecondSeedBudget;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Payload, Limits, SecondSeedBudget, Output).IsSuccess()));
		const FAngelscriptCacheRecordId MatchingId = Output.GetValue()->GetRecordId();
		const TConstArrayView<uint8> AliasedSuccessPayload =
			Output.GetValue()->GetCanonicalPayload();
		FAngelscriptCacheReadBudget SuccessBudget;
		ASSERT_THAT(IsTrue(FAngelscriptDecodedCacheRecord::TryDecode(
			MatchingId, AliasedSuccessPayload, Limits, SuccessBudget, Output).IsSuccess()));
		ASSERT_THAT(IsTrue(Output.IsSet()));
		ASSERT_THAT(IsTrue(RequireTypeSchema(Output.GetValue()).TypeKey
			== MakeCompleteDelegateSchema().TypeKey));
	}

	TEST_METHOD(CompletePayloadRecordIdAndEnvelopeHaveFrozenGoldens)
	{
		const FAngelscriptCachedTypeSchema Schema = MakeCompleteDelegateSchema();
		ASSERT_THAT(AreEqual(FString(TEXT("d5fde2db0fdce72701c7e7a331a2372147f8949b39850ff48869df21af371230")),
			Schema.OrderedProperties[0].StorageLayoutHash.ToHexString()));
		ASSERT_THAT(AreEqual(FString(TEXT("0899ec027b91760050ef10914713ea16ecf5fa5d13f54af8d876a50df8416af7")),
			Schema.OrderedProperties[0].PropertyLayoutFingerprint.ToHexString()));
		ASSERT_THAT(AreEqual(FString(TEXT("31930115c02e5c552a685c7bafefea27b93c664b3fb8e18e65eccb820315a456")),
			Schema.Layout.TypeLayoutHash.ToHexString()));

		FAngelscriptCachedTypeLayoutInput LayoutInputVector;
		LayoutInputVector.InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
		LayoutInputVector.Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xa4, 0xa5);
		LayoutInputVector.BoundaryContribution = 0;
		LayoutInputVector.AlignmentContribution = 8;
		FAngelscriptHash256 LayoutInputHash;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
			LayoutInputVector, LayoutInputHash).IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT(
			"19903c25b6a2d207614125561a1285221a021062c8219ba41c85a84b89abd04c")),
			LayoutInputHash.ToHexString()));
		LayoutInputVector.AlignmentContribution = 16;
		FAngelscriptHash256 MutatedLayoutInputHash;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
			LayoutInputVector, MutatedLayoutInputHash).IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT(
			"c36d242e8b167abedfad69a23577d0651e9e0edab2118d95dcccd448b67a9ac7")),
			MutatedLayoutInputHash.ToHexString()));
		ASSERT_THAT(IsTrue(!(LayoutInputHash == MutatedLayoutInputHash),
			TEXT("one LayoutInput field mutation must change its independent hash")));

		FAngelscriptCachedEnumTypePayload EnumAuthorityVector;
		EnumAuthorityVector.OrderedEnumerators = {
			{0, TEXT("Idle"), -1, {{TEXT("DisplayName"), TEXT("Idle")}}},
			{1, TEXT("Waiting"), -1, {{TEXT("DisplayName"), TEXT("Waiting")}}},
			{2, TEXT("Ready"), MAX_int32, {}},
		};
		FAngelscriptHash256 EnumAuthorityHash;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::ComputeEnumAuthorityHash(
			FAngelscriptStableTypeKey{MakeHash(0x41)}, EnumAuthorityVector,
			EnumAuthorityHash).IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT(
			"bc379827084ce82a8635f56600fae4de208534bf92a6d229ab0fa3688fce58e1")),
			EnumAuthorityHash.ToHexString()));
		EnumAuthorityVector.OrderedEnumerators[2].Value = MIN_int32;
		FAngelscriptHash256 MutatedEnumAuthorityHash;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::ComputeEnumAuthorityHash(
			FAngelscriptStableTypeKey{MakeHash(0x41)}, EnumAuthorityVector,
			MutatedEnumAuthorityHash).IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT(
			"ef456bfeaf07d858e493bd128c2f90534a225bf716854b62113ff14d3d5053ad")),
			MutatedEnumAuthorityHash.ToHexString()));
		ASSERT_THAT(IsTrue(!(EnumAuthorityHash == MutatedEnumAuthorityHash),
			TEXT("one Enum value mutation must change its independent authority hash")));

		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Payload).IsSuccess()));
		const FString ExpectedPayload = TEXT("0200000010101010101010101010101010101010101010101010101010101010101010101111111111111111111111111111111111111111111111111111111111111111050800000047616d65706c617907000000465369676e616c2000000064656c656761746520766f696420465369676e616c28696e742056616c7565295a000000010000000800000043617465676f7279060000004576656e747300000000000000000800000000000000080000000000000031930115c02e5c552a685c7bafefea27b93c664b3fb8e18e65eccb820315a4560100000000000000000000002020202020202020202020202020202020202020202020202020202020202020060000005f496e6e65720105000000000000000000010400000004000000d5fde2db0fdce72701c7e7a331a2372147f8949b39850ff48869df21af371230030000000000010000000900000047656e65726174656401000000310899ec027b91760050ef10914713ea16ecf5fa5d13f54af8d876a50df8416af70100000001000000003030303030303030303030303030303030303030303030303030303030303030111111111111111111111111111111111111111111111111111111111111111131313131313131313131313131313131313131313131313131313131313131310000000002000000010000000003323232323232323232323232323232323232323232323232323232323232323233333333333333333333333333333333333333333333333333333333333333330111111111111111111111111111111111111111111111111111111111111111110f0000000003303030303030303030303030303030303030303030303030303030303030303031313131313131313131313131313131313131313131313131313131313131310111111111111111111111111111111111111111111111111111111111111111113434343434343434343434343434343434343434343434343434343434343434353535353535353535353535353535353535353535353535353535353535353501050000000000000000000003000000020330303030303030303030303030303030303030303030303030303030303030303131313131313131313131313131313131313131313131313131313131313131000203323232323232323232323232323232323232323232323232323232323232323233333333333333333333333333333333333333333333333333333333333333330003033434343434343434343434343434343434343434343434343434343434343434353535353535353535353535353535353535353535353535353535353535353500");
		ASSERT_THAT(AreEqual(ExpectedPayload, Hex(Payload),
			*FString::Printf(TEXT("TypeSchema payload golden actual=%s"), *Hex(Payload))));

		FAngelscriptCacheRecordId RecordId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::TypeSchema, Payload, RecordId).IsSuccess()));
		ASSERT_THAT(AreEqual(FString(TEXT("37c9e115155085b364c0c312bea72cedd7077d6f5b4fed4f10bb99569e1f566e")),
			RecordId.ContentHash.ToHexString()));

		TArray<uint8> Envelope;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
			EAngelscriptCacheRecordKind::TypeSchema, Payload, Envelope).IsSuccess()));
		const FString ExpectedEnvelope = FString(TEXT("55454153435632520200000003000000d40300000000000037c9e115155085b364c0c312bea72cedd7077d6f5b4fed4f10bb99569e1f566e")) + ExpectedPayload;
		ASSERT_THAT(AreEqual(ExpectedEnvelope, Hex(Envelope),
			*FString::Printf(TEXT("TypeSchema envelope golden actual=%s"), *Hex(Envelope))));
	}

	TEST_METHOD(RoundTripReserializesIdenticallyAndCanonicalizesOnlySetLikeFields)
	{
		FAngelscriptCachedTypeSchema Schema = MakeCompleteDelegateSchema();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Payload).IsSuccess()));

		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Payload, Limits, Budget, Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		ASSERT_THAT(AreEqual(Hex(Payload),
			Hex(Decoded.GetValue()->GetCanonicalPayload())));
		ASSERT_THAT(IsTrue(Schema.TypeKey
			== RequireTypeSchema(Decoded.GetValue()).TypeKey));

		Algo::Reverse(Schema.Dependencies);
		Schema.Metadata.Add({TEXT("Alpha"), TEXT("First")});
		FinalizeValidFixtureHashes(Schema);
		TArray<uint8> CanonicalFromReorderedSets;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, CanonicalFromReorderedSets).IsSuccess()));
		FAngelscriptCachedTypeSchema CanonicalSchema = Schema;
		CanonicalSchema.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		CanonicalSchema.Metadata.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareMetadata(A, B) < 0;
		});
		FinalizeValidFixtureHashes(CanonicalSchema);
		TArray<uint8> ExpectedCanonical;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			CanonicalSchema, ExpectedCanonical).IsSuccess()));
		ASSERT_THAT(AreEqual(Hex(ExpectedCanonical), Hex(CanonicalFromReorderedSets)));
	}

	TEST_METHOD(NormalProducerCanonicalizesEverySetLikeFieldWithoutMutatingInput)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		bool bPassed = true;
		const auto SerializeCanonicalEquivalent = [&](
			const FAngelscriptCachedTypeSchema& CanonicalSchema,
			const TCHAR* Context)
		{
			TArray<uint8> ExpectedPayload;
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
					CanonicalSchema, ExpectedPayload);
			bPassed &= LocalAssert.IsTrue(Result.IsSuccess(),
				*FString::Printf(TEXT("%s: explicit canonical equivalent serializes"), Context));
			bPassed &= LocalAssert.IsTrue(!ExpectedPayload.IsEmpty(),
				*FString::Printf(TEXT("%s: explicit canonical equivalent emits payload"), Context));
			return ExpectedPayload;
		};

		FAngelscriptCachedTypeSchema Canonical = MakeCompleteDelegateSchema();
		Canonical.Metadata.Add({TEXT("Alpha"), TEXT("First")});
		Canonical.Metadata.Add({TEXT("Zebra"), TEXT("Last")});
		Canonical.Metadata.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareMetadata(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Canonical);
		const TArray<uint8> TopMetadataExpected = SerializeCanonicalEquivalent(
			Canonical, TEXT("top-level metadata canonical baseline"));
		FAngelscriptCachedTypeSchema Unordered = Canonical;
		Algo::Reverse(Unordered.Metadata);
		FinalizeValidFixtureHashes(Unordered);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			Unordered, TEXT("unique unordered top-level Metadata"), TopMetadataExpected);

		Canonical = MakeCompleteDelegateSchema();
		Canonical.OrderedProperties[0].Metadata.Add({TEXT("Alpha"), TEXT("First")});
		Canonical.OrderedProperties[0].Metadata.Add({TEXT("Zebra"), TEXT("Last")});
		Canonical.OrderedProperties[0].Metadata.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareMetadata(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Canonical);
		const TArray<uint8> PropertyMetadataExpected = SerializeCanonicalEquivalent(
			Canonical, TEXT("property metadata canonical baseline"));
		Unordered = Canonical;
		Algo::Reverse(Unordered.OrderedProperties[0].Metadata);
		FinalizeValidFixtureHashes(Unordered);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			Unordered, TEXT("unique unordered property Metadata"), PropertyMetadataExpected);

		Canonical = MakeEnumSchema();
		Canonical.KindPayload.Enum->OrderedEnumerators[0].Metadata.Add(
			{TEXT("Alpha"), TEXT("First")});
		Canonical.KindPayload.Enum->OrderedEnumerators[0].Metadata.Add(
			{TEXT("Zebra"), TEXT("Last")});
		Canonical.KindPayload.Enum->OrderedEnumerators[0].Metadata.Sort([](const auto& A,
			const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareMetadata(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Canonical);
		const TArray<uint8> EnumMetadataExpected = SerializeCanonicalEquivalent(
			Canonical, TEXT("enum metadata canonical baseline"));
		Unordered = Canonical;
		Algo::Reverse(Unordered.KindPayload.Enum->OrderedEnumerators[0].Metadata);
		FinalizeValidFixtureHashes(Unordered);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			Unordered, TEXT("unique unordered enum-enumerator Metadata"), EnumMetadataExpected);

		Canonical = MakeOrdinaryUClassSchema(true);
		const TArray<uint8> RelationExpected = SerializeCanonicalEquivalent(
			Canonical, TEXT("relation-kind sections canonical baseline"));
		Unordered = Canonical;
		Algo::Reverse(Unordered.Relations);
		FinalizeValidFixtureHashes(Unordered);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			Unordered, TEXT("unique unordered Relation kind sections"), RelationExpected);

		FAngelscriptCachedTypeSchema DirectInterfaces = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Interface);
		FAngelscriptCachedTypeRelation FirstDirectInterface;
		FirstDirectInterface.RelationKind =
			EAngelscriptCachedTypeRelationKind::ImplementedInterface;
		FirstDirectInterface.SemanticOrdinal = 0;
		FirstDirectInterface.Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, 0xf2, 0xf3);
		FAngelscriptCachedTypeRelation SecondDirectInterface = FirstDirectInterface;
		SecondDirectInterface.SemanticOrdinal = 1;
		SecondDirectInterface.Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, 0xe2, 0xe3);
		DirectInterfaces.Relations = {FirstDirectInterface, SecondDirectInterface};
		DirectInterfaces.Dependencies = {
			MakeDependency(EAngelscriptCacheSemanticDependencyKind::Inheritance,
				FirstDirectInterface.Target),
			MakeDependency(EAngelscriptCacheSemanticDependencyKind::Inheritance,
				SecondDirectInterface.Target),
		};
		DirectInterfaces.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		FinalizeValidFixtureHashes(DirectInterfaces);
		bPassed &= LocalAssert.AreEqual(FString(TEXT(
			"dd58ac98186a58c34b2decc1d15c7842f5c4e61e512515d205caba9c63e04dfd")),
			DirectInterfaces.Layout.TypeLayoutHash.ToHexString(),
			TEXT("direct-interface ordinal fixture TypeLayoutHash"));
		TArray<uint8> FirstDirectInterfacePayload;
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			DirectInterfaces,
			TEXT("direct-interface semantic sequence remains ordinal ordered"),
			{}, &FirstDirectInterfacePayload);
		bPassed &= LocalAssert.AreEqual(479, FirstDirectInterfacePayload.Num(),
			TEXT("direct-interface ordinal payload byte count"));
		FAngelscriptCacheRecordId DirectInterfaceRecordId;
		const FAngelscriptCacheValidationResult DirectInterfaceRecordIdResult =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				EAngelscriptCacheRecordKind::TypeSchema,
				FirstDirectInterfacePayload, DirectInterfaceRecordId);
		bPassed &= LocalAssert.IsTrue(DirectInterfaceRecordIdResult.IsSuccess(),
			TEXT("direct-interface ordinal payload builds TypeSchema RecordId"));
		bPassed &= LocalAssert.AreEqual(EAngelscriptCacheRecordKind::TypeSchema,
			DirectInterfaceRecordId.Kind,
			TEXT("direct-interface ordinal RecordId kind"));
		TestRunner->AddInfo(FString::Printf(
			TEXT("TypeSchema v2 direct-interface RecordId=%s Bytes=%d"),
			*DirectInterfaceRecordId.ContentHash.ToHexString(),
			FirstDirectInterfacePayload.Num()));
		bPassed &= LocalAssert.AreEqual(FString(TEXT(
			"011ee5ba2e93da4102cd83e7a86fa96b64f64ce6c5c52f9a633481aba18f75ca")),
			DirectInterfaceRecordId.ContentHash.ToHexString(),
			TEXT("direct-interface ordinal RecordId content hash"));

		Canonical = MakeOrdinaryUClassSchema(true);
		const TArray<uint8> LayoutInputExpected = SerializeCanonicalEquivalent(
			Canonical, TEXT("LayoutInputs canonical baseline"));
		Unordered = Canonical;
		Algo::Reverse(Unordered.LayoutInputs);
		FinalizeValidFixtureHashes(Unordered);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			Unordered, TEXT("unique unordered LayoutInputs by InputKind"), LayoutInputExpected);

		Canonical = MakeCompleteDelegateSchema();
		Canonical.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Canonical);
		const TArray<uint8> DependencyExpected = SerializeCanonicalEquivalent(
			Canonical, TEXT("Dependencies canonical baseline"));
		Unordered = Canonical;
		Algo::Reverse(Unordered.Dependencies);
		FinalizeValidFixtureHashes(Unordered);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			Unordered, TEXT("unique unordered Dependencies production compare contract"),
			DependencyExpected);

		FAngelscriptCachedTypeSchema Invalid = MakeCompleteDelegateSchema();
		Invalid.Metadata.Add({TEXT("Duplicate"), TEXT("Value")});
		Invalid.Metadata.Add({TEXT("Duplicate"), TEXT("Value")});
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate top-level Metadata key"));
		Invalid.Metadata.Last().CanonicalValue = TEXT("OtherValue");
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("conflicting top-level Metadata key"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.OrderedProperties[0].Metadata.Add({TEXT("Duplicate"), TEXT("Value")});
		Invalid.OrderedProperties[0].Metadata.Add({TEXT("Duplicate"), TEXT("Value")});
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate property Metadata key"));
		Invalid.OrderedProperties[0].Metadata.Last().CanonicalValue = TEXT("OtherValue");
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("conflicting property Metadata key"));

		Invalid = MakeEnumSchema();
		Invalid.KindPayload.Enum->OrderedEnumerators[0].Metadata.Add(
			{TEXT("Duplicate"), TEXT("Value")});
		Invalid.KindPayload.Enum->OrderedEnumerators[0].Metadata.Add(
			{TEXT("Duplicate"), TEXT("Value")});
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate enum-enumerator Metadata key"));
		Invalid.KindPayload.Enum->OrderedEnumerators[0].Metadata.Last().CanonicalValue =
			TEXT("OtherValue");
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("conflicting enum-enumerator Metadata key"));

		Invalid = MakeOrdinaryUClassSchema(true);
		Invalid.Relations.Add(FAngelscriptCachedTypeRelation(Invalid.Relations[0]));
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate singleton Relation kind"));
		Invalid = MakeOrdinaryUClassSchema(true);
		FAngelscriptCachedTypeRelation ConflictingRelation = Invalid.Relations[0];
		ConflictingRelation.Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, 0xd0, 0xd1);
		Invalid.Relations.Add(ConflictingRelation);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("conflicting singleton Relation kind"));

		Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.LayoutInputs.Add(FAngelscriptCachedTypeLayoutInput(Invalid.LayoutInputs[0]));
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate LayoutInput coordinate"));
		Invalid = MakeOrdinaryUClassSchema(false);
		FAngelscriptCachedTypeLayoutInput ConflictingInput = Invalid.LayoutInputs[0];
		ConflictingInput.Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xd2, 0xd3);
		Invalid.LayoutInputs.Add(ConflictingInput);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("conflicting LayoutInput coordinate"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.Dependencies.Add(FAngelscriptCacheSemanticDependency(Invalid.Dependencies[0]));
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate Dependency coordinate"));
		Invalid = MakeCompleteDelegateSchema();
		FAngelscriptCacheSemanticDependency ConflictingDependency = Invalid.Dependencies[0];
		ConflictingDependency.Target.ExpectedAbi = MakeHash(0xd4);
		Invalid.Dependencies.Add(ConflictingDependency);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("conflicting Dependency coordinate"));

		ASSERT_THAT(IsTrue(bPassed,
			TEXT("normal producer set-like canonicalization and duplicate handling")));
	}

	TEST_METHOD(NormalProducerRejectsHeaderStringsAndTypeFlagRulesAtomically)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		bool bPassed = true;
		const uint32 FrozenSchemaVersion =
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion;
		constexpr uint32 Abstract =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Abstract);
		constexpr uint32 Final =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Final);
		constexpr uint32 Shared =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Shared);
		constexpr uint32 Generated =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Generated);
		constexpr uint32 HasDefaultConstructor =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor);
		constexpr uint32 HasDestructor =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::HasDestructor);
		constexpr uint32 ValueType =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ValueType);
		constexpr uint32 ReferenceType =
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ReferenceType);

		const auto MakeFlagFixture = [&](const EAngelscriptCachedTypeKind Kind,
			const uint32 Flags, const bool bConfigureBehaviors = true)
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(Kind);
			Schema.TypeSemanticFlags = Flags;
			if (bConfigureBehaviors)
			{
				ConfigureFlagCoupledBehaviors(Schema, Flags);
			}
			else
			{
				// Delegate's complete minimal fixture owns Construct/Copy behavior
				// rows already. Negative flag-coupling cases require a genuinely
				// behavior-free graph, including its now-unreachable dependencies.
				ClearBehaviorOwnedStateForTests(Schema);
				Schema.TypeSemanticFlags = Flags;
			}
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};
		const auto InsertEmbeddedNul = [](FString& Value)
		{
			const int32 EmbeddedNulIndex = Value.Len();
			Value.Append(TEXT("AfterNul"));
			Value.GetCharArray().Insert(static_cast<TCHAR>(0), EmbeddedNulIndex);
		};
		const auto ExpectFlagSuccess = [&](const EAngelscriptCachedTypeKind Kind,
			const uint32 Flags, const TCHAR* Context)
		{
			return ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
				MakeFlagFixture(Kind, Flags), Context);
		};
		const auto ExpectFlagFailure = [&](const EAngelscriptCachedTypeKind Kind,
			const uint32 Flags, const EAngelscriptCacheValidationError ExpectedError,
			const TCHAR* Context)
		{
			return ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				MakeFlagFixture(Kind, Flags), ExpectedError, Context);
		};
		const auto ExpectBehaviorFlagFailure = [&](const EAngelscriptCachedTypeKind Kind,
			const uint32 Flags, const TCHAR* Context)
		{
			return ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				MakeFlagFixture(Kind, Flags, false),
				EAngelscriptCacheValidationError::InvalidQualifierCombination, Context);
		};

		FAngelscriptCachedTypeSchema Invalid = MakeCompleteDelegateSchema();
		Invalid.PayloadSchemaVersion = FrozenSchemaVersion - 1;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
			TEXT("payload schema version below frozen version"));
		Invalid = MakeCompleteDelegateSchema();
		Invalid.PayloadSchemaVersion = FrozenSchemaVersion + 1;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
			TEXT("payload schema version above frozen version"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.ModuleKey = {};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ZeroStableKey,
			TEXT("zero ModuleKey"));
		Invalid = MakeCompleteDelegateSchema();
		Invalid.TypeKey = {};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ZeroStableKey,
			TEXT("zero TypeKey"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.TypeKind = static_cast<EAngelscriptCachedTypeKind>(0);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::UnknownEnumValue,
			TEXT("TypeKind raw 0"));
		Invalid = MakeCompleteDelegateSchema();
		Invalid.TypeKind = static_cast<EAngelscriptCachedTypeKind>(8);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::UnknownEnumValue,
			TEXT("TypeKind raw 8"));
		Invalid = MakeCompleteDelegateSchema();
		Invalid.TypeKind = static_cast<EAngelscriptCachedTypeKind>(0xff);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::UnknownEnumValue,
			TEXT("TypeKind raw 0xff"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.CanonicalName.Reset();
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("empty required CanonicalName"));
		Invalid = MakeCompleteDelegateSchema();
		Invalid.CanonicalDeclaration.Reset();
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("empty required CanonicalDeclaration"));

		Invalid = MakeCompleteDelegateSchema();
		InsertEmbeddedNul(Invalid.CanonicalNamespace);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::EmbeddedNul,
			TEXT("embedded NUL in CanonicalNamespace"));
		Invalid = MakeCompleteDelegateSchema();
		InsertEmbeddedNul(Invalid.CanonicalName);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::EmbeddedNul,
			TEXT("embedded NUL in CanonicalName"));
		Invalid = MakeCompleteDelegateSchema();
		InsertEmbeddedNul(Invalid.CanonicalDeclaration);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::EmbeddedNul,
			TEXT("embedded NUL in CanonicalDeclaration"));
		if constexpr (sizeof(TCHAR) == 2)
		{
			Invalid = MakeCompleteDelegateSchema();
			Invalid.CanonicalNamespace.AppendChar(static_cast<TCHAR>(0xd800));
			FinalizeValidFixtureHashes(Invalid);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
				EAngelscriptCacheValidationError::InvalidUtf8,
				TEXT("unpaired UTF-16 surrogate in CanonicalNamespace"));
		}

		// Retained B1 producer rows already witness Class missing ReferenceType,
		// Class Abstract|Final, and the unknown high bit. The rows below are the
		// remaining literal witnesses for each frozen known bit.
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Class,
			ReferenceType | Abstract, TEXT("Class allows Abstract"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Class,
			ReferenceType | Final, TEXT("Class allows Final"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Class,
			ReferenceType | Shared, TEXT("Class allows Shared"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Class,
			ReferenceType | Generated, TEXT("Class allows Generated"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Class,
			ReferenceType | HasDefaultConstructor,
			TEXT("Class allows HasDefaultConstructor with Construct and Factory"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Class,
			ReferenceType | HasDestructor, TEXT("Class allows HasDestructor with Destruct"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Class,
			ReferenceType | ValueType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Class forbids ValueType and ValueType|ReferenceType"));

		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Struct, ValueType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Struct requires Final"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Struct, Final,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Struct requires ValueType"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | Shared, TEXT("Struct allows Shared"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | Generated, TEXT("Struct allows Generated"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | HasDefaultConstructor,
			TEXT("Struct allows HasDefaultConstructor with Construct"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | HasDestructor,
			TEXT("Struct allows HasDestructor with Destruct"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | Abstract,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Struct forbids Abstract"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | ReferenceType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Struct forbids ReferenceType"));

		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Interface, ReferenceType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Interface requires Abstract"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Interface, Abstract,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Interface requires ReferenceType"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Interface,
			Abstract | ReferenceType | Shared, TEXT("Interface allows Shared"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Interface,
			Abstract | ReferenceType | Final,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Interface forbids Final"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Interface,
			Abstract | ReferenceType | Generated,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Interface forbids Generated"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Interface,
			Abstract | ReferenceType | HasDefaultConstructor,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Interface forbids HasDefaultConstructor"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Interface,
			Abstract | ReferenceType | HasDestructor,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Interface forbids HasDestructor"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Interface,
			Abstract | ReferenceType | ValueType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Interface forbids ValueType"));

		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Enum, ValueType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Enum requires Final"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Enum, Final,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Enum requires ValueType"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Enum,
			Final | ValueType | Shared, TEXT("Enum allows Shared"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Enum,
			Final | ValueType | Abstract,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Enum forbids Abstract"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Enum,
			Final | ValueType | Generated,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Enum forbids Generated"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Enum,
			Final | ValueType | HasDefaultConstructor,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Enum forbids HasDefaultConstructor"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Enum,
			Final | ValueType | HasDestructor,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Enum forbids HasDestructor"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Enum,
			Final | ValueType | ReferenceType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Enum forbids ReferenceType"));

		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Delegate,
			Generated | ValueType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Delegate requires Final"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Delegate,
			Final | ValueType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Delegate requires Generated"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Delegate requires ValueType"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | HasDefaultConstructor,
			TEXT("Delegate allows HasDefaultConstructor with Construct"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | HasDestructor,
			TEXT("Delegate allows HasDestructor with Destruct"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | Abstract,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Delegate forbids Abstract"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | Shared,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Delegate forbids Shared"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | ReferenceType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Delegate forbids ReferenceType"));

		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Typedef, 0,
			TEXT("Typedef accepts no TypeSemanticFlags"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Typedef, Abstract,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef forbids Abstract"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Typedef, Final,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef forbids Final"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Typedef, Shared,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef forbids Shared"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Typedef, Generated,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef forbids Generated"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Typedef,
			HasDefaultConstructor,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef forbids HasDefaultConstructor"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Typedef, HasDestructor,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef forbids HasDestructor"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Typedef, ValueType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef forbids ValueType"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Typedef, ReferenceType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef forbids ReferenceType"));

		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Funcdef, 0,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Funcdef requires ReferenceType"));
		bPassed &= ExpectFlagSuccess(EAngelscriptCachedTypeKind::Funcdef,
			ReferenceType | Shared, TEXT("Funcdef allows Shared"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Funcdef,
			ReferenceType | Abstract,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Funcdef forbids Abstract"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Funcdef,
			ReferenceType | Final,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Funcdef forbids Final"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Funcdef,
			ReferenceType | Generated,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Funcdef forbids Generated"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Funcdef,
			ReferenceType | HasDefaultConstructor,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Funcdef forbids HasDefaultConstructor"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Funcdef,
			ReferenceType | HasDestructor,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Funcdef forbids HasDestructor"));
		bPassed &= ExpectFlagFailure(EAngelscriptCachedTypeKind::Funcdef,
			ReferenceType | ValueType,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Funcdef forbids ValueType"));

		bPassed &= ExpectBehaviorFlagFailure(EAngelscriptCachedTypeKind::Class,
			ReferenceType | HasDefaultConstructor,
			TEXT("Class HasDefaultConstructor requires Construct and Factory"));
		bPassed &= ExpectBehaviorFlagFailure(EAngelscriptCachedTypeKind::Class,
			ReferenceType | HasDestructor,
			TEXT("Class HasDestructor requires Destruct"));
		bPassed &= ExpectBehaviorFlagFailure(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | HasDefaultConstructor,
			TEXT("Struct HasDefaultConstructor requires Construct"));
		bPassed &= ExpectBehaviorFlagFailure(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | HasDestructor,
			TEXT("Struct HasDestructor requires Destruct"));
		bPassed &= ExpectBehaviorFlagFailure(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | HasDefaultConstructor,
			TEXT("Delegate HasDefaultConstructor requires Construct"));
		bPassed &= ExpectBehaviorFlagFailure(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | HasDestructor,
			TEXT("Delegate HasDestructor requires Destruct"));

		Invalid = MakeFlagFixture(EAngelscriptCachedTypeKind::Class,
			ReferenceType | HasDefaultConstructor);
		Invalid.TypeSemanticFlags = ReferenceType;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner, Invalid,
			TEXT("ModuleSnapshot graph owns Class zero-parameter Construct and Factory parity"));
		Invalid = MakeFlagFixture(EAngelscriptCachedTypeKind::Class,
			ReferenceType | HasDestructor);
		Invalid.TypeSemanticFlags = ReferenceType;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Class Destruct requires HasDestructor"));
		Invalid = MakeFlagFixture(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | HasDefaultConstructor);
		Invalid.TypeSemanticFlags = Final | ValueType;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner, Invalid,
			TEXT("ModuleSnapshot graph owns Struct zero-parameter Construct parity"));
		Invalid = MakeFlagFixture(EAngelscriptCachedTypeKind::Struct,
			Final | ValueType | HasDestructor);
		Invalid.TypeSemanticFlags = Final | ValueType;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Struct Destruct requires HasDestructor"));
		Invalid = MakeFlagFixture(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | HasDefaultConstructor);
		Invalid.TypeSemanticFlags = Final | Generated | ValueType;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner, Invalid,
			TEXT("ModuleSnapshot graph owns Delegate zero-parameter Construct parity"));
		Invalid = MakeFlagFixture(EAngelscriptCachedTypeKind::Delegate,
			Final | Generated | ValueType | HasDestructor);
		Invalid.TypeSemanticFlags = Final | Generated | ValueType;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Delegate Destruct requires HasDestructor"));

		ASSERT_THAT(IsTrue(bPassed,
			TEXT("normal producer header, string, and TypeSemanticFlags rules")));
	}

	TEST_METHOD(SevenTypeKindsSelectOnlyTheirLegalKindPayload)
	{
		for (uint8 RawKind = 1; RawKind <= 7; ++RawKind)
		{
			const EAngelscriptCachedTypeKind Kind =
				static_cast<EAngelscriptCachedTypeKind>(RawKind);
			const FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(Kind);
			TArray<uint8> Payload;
			ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, Payload).IsSuccess(),
				*FString::Printf(TEXT("TypeKind %u should serialize"), RawKind)));
			FAngelscriptCacheReadLimits Limits;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
				Payload, Limits, Budget, Decoded).IsSuccess()));
			ASSERT_THAT(IsTrue(Decoded.IsSet()));
			ASSERT_THAT(AreEqual(Kind,
				RequireTypeSchema(Decoded.GetValue()).TypeKind));
		}

		FAngelscriptCachedTypeSchema WrongArm = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		WrongArm.KindPayload.Enum = MakeEnumSchema().KindPayload.Enum;
		TArray<uint8> RejectedProducerBytes = {0xaa};
		const FAngelscriptCacheValidationResult ProducerResult =
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				WrongArm, RejectedProducerBytes);
		ASSERT_THAT(IsTrue(ExpectExactInactiveArmProducerFailure(*TestRunner,
			ProducerResult, RejectedProducerBytes,
			TEXT("Class with inactive Enum producer arm"))));
		WrongArm = MakeEnumSchema();
		WrongArm.KindPayload.Callable = FAngelscriptCachedCallableTypePayload{
			FAngelscriptStableFunctionKey{MakeHash(0x91)}, MakeHash(0x92), false};
		RejectedProducerBytes = {0xbb};
		const FAngelscriptCacheValidationResult SecondProducerResult =
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				WrongArm, RejectedProducerBytes);
		ASSERT_THAT(IsTrue(ExpectExactInactiveArmProducerFailure(*TestRunner,
			SecondProducerResult, RejectedProducerBytes,
			TEXT("Enum with inactive Callable producer arm"))));
		WrongArm = MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
		WrongArm.KindPayload.Callable->bMulticast = true;
		const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(WrongArm);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::KindPayload,
			TEXT("Funcdef selected callable arm cannot be multicast"))));
	}

	TEST_METHOD(NormalProducerRejectsKindPayloadEnumCallableAndTypedefRulesAtomically)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		bool bPassed = true;
		FAngelscriptCachedTypeSchema Invalid = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		Invalid.KindPayload.Callable = FAngelscriptCachedCallableTypePayload{
			FAngelscriptStableFunctionKey{MakeHash(0x70)}, MakeHash(0x71), false};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Class with inactive Callable producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		Invalid.KindPayload.Typedef = FAngelscriptCachedTypedefTypePayload{
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int32)};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Class with inactive Typedef producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		Invalid.KindPayload.Enum = MakeEnumSchema().KindPayload.Enum;
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Struct with inactive Enum producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		Invalid.KindPayload.Callable = FAngelscriptCachedCallableTypePayload{
			FAngelscriptStableFunctionKey{MakeHash(0x72)}, MakeHash(0x73), false};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Struct with inactive Callable producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		Invalid.KindPayload.Typedef = FAngelscriptCachedTypedefTypePayload{
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int32)};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Struct with inactive Typedef producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface);
		Invalid.KindPayload.Enum = MakeEnumSchema().KindPayload.Enum;
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Interface with inactive Enum producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface);
		Invalid.KindPayload.Callable = FAngelscriptCachedCallableTypePayload{
			FAngelscriptStableFunctionKey{MakeHash(0x74)}, MakeHash(0x75), false};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Interface with inactive Callable producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface);
		Invalid.KindPayload.Typedef = FAngelscriptCachedTypedefTypePayload{
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int32)};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Interface with inactive Typedef producer arm"));

		Invalid = MakeEnumSchema();
		Invalid.KindPayload.Enum.Reset();
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Enum without required Enum producer arm"));

		Invalid = MakeEnumSchema();
		Invalid.KindPayload.Typedef = FAngelscriptCachedTypedefTypePayload{
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int32)};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Enum with inactive Typedef producer arm"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.KindPayload.Callable.Reset();
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Delegate without required Callable producer arm"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.KindPayload.Enum = MakeEnumSchema().KindPayload.Enum;
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Delegate with inactive Enum producer arm"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.KindPayload.Typedef = FAngelscriptCachedTypedefTypePayload{
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int32)};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Delegate with inactive Typedef producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Typedef.Reset();
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Typedef without required Typedef producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Enum = MakeEnumSchema().KindPayload.Enum;
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Typedef with inactive Enum producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Callable = FAngelscriptCachedCallableTypePayload{
			FAngelscriptStableFunctionKey{MakeHash(0x76)}, MakeHash(0x77), false};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Typedef with inactive Callable producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
		Invalid.KindPayload.Callable.Reset();
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Funcdef without required Callable producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
		Invalid.KindPayload.Enum = MakeEnumSchema().KindPayload.Enum;
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Funcdef with inactive Enum producer arm"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
		Invalid.KindPayload.Typedef = FAngelscriptCachedTypedefTypePayload{
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int32)};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Funcdef with inactive Typedef producer arm"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.KindPayload.Callable->SignatureFunctionKey = {};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ZeroStableKey,
			TEXT("Delegate Callable zero SignatureFunctionKey"));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.KindPayload.Callable->ExpectedSignatureAbi = {};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::MissingExpectedAbi,
			TEXT("Delegate Callable zero ExpectedSignatureAbi"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
		Invalid.KindPayload.Callable->SignatureFunctionKey = {};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::ZeroStableKey,
			TEXT("Funcdef Callable zero SignatureFunctionKey"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
		Invalid.KindPayload.Callable->ExpectedSignatureAbi = {};
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::MissingExpectedAbi,
			TEXT("Funcdef Callable zero ExpectedSignatureAbi"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
		Invalid.KindPayload.Callable->bMulticast = true;
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Funcdef Callable multicast is forbidden"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Typedef->AliasedType =
			MakePrimitive(EAngelscriptCachedPrimitiveType::Void);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef Void alias"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Typedef->AliasedType = {};
		Invalid.KindPayload.Typedef->AliasedType.Kind =
			EAngelscriptCachedDataTypeKind::Auto;
		Invalid.KindPayload.Typedef->AliasedType.QualifierFlags =
			static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::Auto);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef Auto alias"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Typedef->AliasedType = MakeScriptType(0x78, 0x79);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef ScriptType object alias"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Typedef->AliasedType.QualifierFlags =
			static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef ObjectHandle-qualified primitive alias"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Typedef->AliasedType.QualifierFlags =
			static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::Reference);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef Reference-qualified primitive alias"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
		Invalid.KindPayload.Typedef->AliasedType.OrderedSubTypes.Add(
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int32));
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef primitive alias with one OrderedSubTypes entry"));

		FAngelscriptCachedTypeSchema MinMaxEnum = MakeEnumSchema();
		MinMaxEnum.KindPayload.Enum->OrderedEnumerators[0].Value = MIN_int32;
		MinMaxEnum.KindPayload.Enum->OrderedEnumerators[1].Value = MIN_int32;
		MinMaxEnum.KindPayload.Enum->OrderedEnumerators[2].Value = MAX_int32;
		FinalizeValidFixtureHashes(MinMaxEnum);
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			MinMaxEnum, TEXT("Enum MIN_int32 alias and MAX_int32 producer witness"));

		Invalid = MakeEnumSchema();
		Invalid.KindPayload.Enum->OrderedEnumerators[1].CanonicalName = TEXT("");
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Enum CanonicalName is required"));

		Invalid = MakeEnumSchema();
		Invalid.KindPayload.Enum->EnumAuthorityHash = MakeHash(0x7a);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			TEXT("EnumAuthorityHash is derived and stale hash is rejected"));

		ASSERT_THAT(IsTrue(bPassed,
			TEXT("normal producer KindPayload, callable, Typedef, and Enum rules")));
	}

	TEST_METHOD(NormalProducerRejectsRelationRulesAtomically)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		bool bPassed = true;
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][Relations][Producer] begin "
			"matrix=11 forms x 5 kinds x 3 cardinalities; focused=30; expected-total=195"));
		using FError = EAngelscriptCacheValidationError;
		constexpr FError S = FError::None;
		constexpr FError P = FError::InvalidPresence;
		constexpr FError C = FError::ConflictingKey;
		constexpr FError Q = FError::InvalidQualifierCombination;

		enum class ERelationForm : uint8
		{
			ClassNone,
			OrdinaryUClass,
			StaticsUClass,
			StructNone,
			UStruct,
			InterfaceNone,
			EnumNone,
			UEnum,
			Delegate,
			Typedef,
			Funcdef,
		};

		struct FRelationFormRow
		{
			ERelationForm Form;
			const TCHAR* Name;
			EAngelscriptCacheReferenceKind TargetKinds[5];
			FError Expected[5][3];
		};

		constexpr EAngelscriptCacheReferenceKind Script =
			EAngelscriptCacheReferenceKind::ScriptType;
		constexpr EAngelscriptCacheReferenceKind Environment =
			EAngelscriptCacheReferenceKind::EnvironmentSymbol;
		const FRelationFormRow FormRows[] = {
			{ERelationForm::ClassNone, TEXT("Class+None"),
				{Script, Script, Script, Script, Script},
				{{S, S, C}, {S, P, P}, {S, P, P}, {S, S, S}, {S, P, P}}},
			{ERelationForm::OrdinaryUClass, TEXT("ordinary UClass"),
				{Script, Environment, Environment, Environment, Script},
				{{S, S, C}, {P, S, C}, {P, S, C}, {S, S, S}, {S, P, P}}},
			{ERelationForm::StaticsUClass, TEXT("statics UClass"),
				{Script, Script, Environment, Script, Script},
				{{S, Q, Q}, {S, P, P}, {P, S, C}, {S, P, P}, {S, P, P}}},
			{ERelationForm::StructNone, TEXT("Struct+None"),
				{Script, Script, Script, Script, Script},
				{{S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}}},
			{ERelationForm::UStruct, TEXT("Struct+UStruct"),
				{Script, Script, Script, Script, Script},
				{{S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}}},
			{ERelationForm::InterfaceNone, TEXT("Interface+None"),
				{Script, Script, Script, Script, Script},
				{{S, P, P}, {S, P, P}, {S, P, P}, {S, S, S}, {S, P, P}}},
			{ERelationForm::EnumNone, TEXT("Enum+None"),
				{Script, Script, Script, Script, Script},
				{{S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}}},
			{ERelationForm::UEnum, TEXT("Enum+UEnum"),
				{Script, Script, Script, Script, Script},
				{{S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}}},
			{ERelationForm::Delegate, TEXT("Delegate+UDelegate"),
				{Script, Script, Script, Script, Script},
				{{S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}}},
			{ERelationForm::Typedef, TEXT("Typedef+None"),
				{Script, Script, Script, Script, Script},
				{{S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}}},
			{ERelationForm::Funcdef, TEXT("Funcdef+None"),
				{Script, Script, Script, Script, Script},
				{{S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}, {S, P, P}}},
		};

		const TCHAR* RelationKindNames[] = {
			TEXT("Base"),
			TEXT("ShadowSuper"),
			TEXT("CodeSuper"),
			TEXT("ImplementedInterface"),
			TEXT("Compose"),
		};

		const auto MakeFormSchema = [](const ERelationForm Form)
		{
			switch (Form)
			{
			case ERelationForm::ClassNone:
				return MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
			case ERelationForm::OrdinaryUClass:
				return MakeOrdinaryUClassSchema(false);
			case ERelationForm::StaticsUClass:
				return MakeStaticsUClassSchema();
			case ERelationForm::StructNone:
				return MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
			case ERelationForm::UStruct:
				return MakeReflectedUStructSchema();
			case ERelationForm::InterfaceNone:
				return MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface);
			case ERelationForm::EnumNone:
				return MakeReflectionFormSchema(EAngelscriptCachedTypeKind::Enum,
					EAngelscriptCachedReflectionKind::None);
			case ERelationForm::UEnum:
				return MakeEnumSchema();
			case ERelationForm::Delegate:
				return MakeCompleteDelegateSchema();
			case ERelationForm::Typedef:
				return MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef);
			case ERelationForm::Funcdef:
				return MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef);
			}
			checkNoEntry();
			return MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		};

		const auto MakeRelationFixture = [&](const ERelationForm Form,
			const EAngelscriptCachedTypeRelationKind Kind,
			const uint32 Cardinality,
			const EAngelscriptCacheReferenceKind TargetKind)
		{
			FAngelscriptCachedTypeSchema Schema = MakeFormSchema(Form);
			TArray<FAngelscriptCacheStableReference> RemovedTargets;
			for (const FAngelscriptCachedTypeRelation& Relation : Schema.Relations)
			{
				if (Relation.RelationKind == Kind)
				{
					RemovedTargets.Add(Relation.Target);
				}
			}
			Schema.Relations.RemoveAll([Kind](const FAngelscriptCachedTypeRelation& Relation)
			{
				return Relation.RelationKind == Kind;
			});
			Schema.Dependencies.RemoveAll([&](
				const FAngelscriptCacheSemanticDependency& Dependency)
			{
				return RemovedTargets.Contains(Dependency.Target);
			});
			if (Kind == EAngelscriptCachedTypeRelationKind::Base)
			{
				Schema.LayoutInputs.RemoveAll([](
					const FAngelscriptCachedTypeLayoutInput& Input)
				{
					return Input.InputKind
						== EAngelscriptCachedTypeLayoutInputKind::BaseType;
				});
			}
			if (Kind == EAngelscriptCachedTypeRelationKind::CodeSuper
				&& Form == ERelationForm::OrdinaryUClass)
			{
				Schema.LayoutInputs.RemoveAll([](
					const FAngelscriptCachedTypeLayoutInput& Input)
				{
					return Input.InputKind
						== EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
				});
			}

			for (uint32 Index = 0; Index < Cardinality; ++Index)
			{
				FAngelscriptCacheStableReference Target = MakeReference(
					TargetKind, static_cast<uint8>(0xd0 + Index * 2),
					static_cast<uint8>(0xd1 + Index * 2));
				if (Index == 0
					&& TargetKind == EAngelscriptCacheReferenceKind::EnvironmentSymbol
					&& (Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
						|| Kind == EAngelscriptCachedTypeRelationKind::CodeSuper))
				{
					const EAngelscriptCachedTypeRelationKind PeerKind =
						Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
							? EAngelscriptCachedTypeRelationKind::CodeSuper
							: EAngelscriptCachedTypeRelationKind::ShadowSuper;
					for (const FAngelscriptCachedTypeRelation& Peer : Schema.Relations)
					{
						if (Peer.RelationKind == PeerKind)
						{
							Target = Peer.Target;
							break;
						}
					}
				}

				FAngelscriptCachedTypeRelation Relation;
				Relation.RelationKind = Kind;
				Relation.Target = Target;
				if (Kind == EAngelscriptCachedTypeRelationKind::ImplementedInterface)
				{
					Relation.SemanticOrdinal = Index;
				}
				Schema.Relations.Add(Relation);
				Schema.Dependencies.Add(MakeDependency(
					Target.Kind == EAngelscriptCacheReferenceKind::ScriptType
						? EAngelscriptCacheSemanticDependencyKind::Inheritance
						: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
					Target));

				if (Index == 0 && Kind == EAngelscriptCachedTypeRelationKind::Base)
				{
					FAngelscriptCachedTypeLayoutInput Input;
					Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
					Input.Target = Target;
					Input.BoundaryContribution = 0;
					Input.AlignmentContribution = 8;
					Schema.LayoutInputs.Add(Input);
				}
				if (Index == 0
					&& Kind == EAngelscriptCachedTypeRelationKind::CodeSuper
					&& Form == ERelationForm::OrdinaryUClass)
				{
					FAngelscriptCachedTypeLayoutInput Input;
					Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
					Input.Target = Target;
					Input.BoundaryContribution = 0;
					Input.AlignmentContribution = 8;
					Schema.LayoutInputs.Add(Input);
				}
			}

			if (Form == ERelationForm::OrdinaryUClass
				&& Kind == EAngelscriptCachedTypeRelationKind::Base)
			{
				const uint32 SuperBit = static_cast<uint32>(
					EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass);
				Schema.Reflection.ClassReflectionFlags = Cardinality == 0
					? Schema.Reflection.ClassReflectionFlags | SuperBit
					: Schema.Reflection.ClassReflectionFlags & ~SuperBit;
				for (FAngelscriptCachedTypeLayoutInput& Input : Schema.LayoutInputs)
				{
					if (Input.InputKind == EAngelscriptCachedTypeLayoutInputKind::CodeRoot)
					{
						if (Cardinality == 0)
						{
							Input.BoundaryContribution = 0;
						}
						else
						{
							Input.BoundaryContribution.Reset();
						}
					}
				}
			}

			Schema.Relations.StableSort([](const auto& A, const auto& B)
			{
				return static_cast<uint8>(A.RelationKind)
					< static_cast<uint8>(B.RelationKind);
			});
			Schema.LayoutInputs.Sort([](const auto& A, const auto& B)
			{
				return static_cast<uint8>(A.InputKind)
					< static_cast<uint8>(B.InputKind);
			});
			Schema.Dependencies.Sort([](const auto& A, const auto& B)
			{
				return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
			});
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};

		int32 MatrixCalls = 0;
		for (const FRelationFormRow& FormRow : FormRows)
		{
			for (int32 KindIndex = 0; KindIndex < 5; ++KindIndex)
			{
				const EAngelscriptCachedTypeRelationKind Kind =
					static_cast<EAngelscriptCachedTypeRelationKind>(KindIndex + 1);
				for (uint32 Cardinality = 0; Cardinality < 3; ++Cardinality)
				{
					const FError Expected = FormRow.Expected[KindIndex][Cardinality];
					const FAngelscriptCachedTypeSchema Schema = MakeRelationFixture(
						FormRow.Form, Kind, Cardinality, FormRow.TargetKinds[KindIndex]);
					const FString Context = FString::Printf(TEXT("%s %s cardinality %u"),
						FormRow.Name, RelationKindNames[KindIndex], Cardinality);
					if (Expected == FError::None)
					{
						bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(
							*TestRunner, Schema, *Context);
					}
					else
					{
						bPassed &= ExpectExactProducerFailureAndInputUnchanged(
							*TestRunner, Schema, Expected, *Context);
					}
					++MatrixCalls;
				}
			}
		}

		const auto MakeDirectInterfaces = [&](const uint32 Count)
		{
			return MakeRelationFixture(ERelationForm::ClassNone,
				EAngelscriptCachedTypeRelationKind::ImplementedInterface,
				Count, EAngelscriptCacheReferenceKind::ScriptType);
		};

		int32 AdditionalCalls = 0;
		const uint8 RawRelationKinds[] = {0, 6, 255};
		for (const uint8 RawKind : RawRelationKinds)
		{
			FAngelscriptCachedTypeSchema Invalid = MakeDirectInterfaces(1);
			Invalid.Relations[0].RelationKind =
				static_cast<EAngelscriptCachedTypeRelationKind>(RawKind);
			FinalizeValidFixtureHashes(Invalid);
			const FString Context = FString::Printf(
				TEXT("raw RelationKind %u"), RawKind);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				Invalid, FError::UnknownEnumValue, *Context);
			++AdditionalCalls;
		}

		FAngelscriptCachedTypeSchema Invalid = MakeRelationFixture(
			ERelationForm::ClassNone, EAngelscriptCachedTypeRelationKind::Base,
			1, EAngelscriptCacheReferenceKind::ScriptType);
		Invalid.Relations[0].SemanticOrdinal = 0;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::InvalidPresence, TEXT("Base relation forbids SemanticOrdinal"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(1);
		Invalid.Relations[0].SemanticOrdinal.Reset();
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::InvalidPresence,
			TEXT("ImplementedInterface requires SemanticOrdinal"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(3);
		Invalid.Relations[0].SemanticOrdinal = 1;
		Invalid.Relations[1].SemanticOrdinal = 2;
		Invalid.Relations[2].SemanticOrdinal = 3;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::OrdinalGap, TEXT("direct-interface first ordinal gap 1,2,3"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(3);
		Invalid.Relations[2].SemanticOrdinal = 3;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::OrdinalGap, TEXT("direct-interface last ordinal gap 0,1,3"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(3);
		Invalid.Relations[1].SemanticOrdinal = 0;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::DuplicateOrdinal,
			TEXT("direct-interface middle duplicate ordinal 0,0,2"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(3);
		Invalid.Relations[2].SemanticOrdinal = 1;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::DuplicateOrdinal,
			TEXT("direct-interface last duplicate ordinal 0,1,1"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(2);
		Swap(Invalid.Relations[0], Invalid.Relations[1]);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::NonCanonicalOrder,
			TEXT("direct-interface complete-row stored ordinal order 1,0"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(1);
		Invalid.Relations.Add(FAngelscriptCachedTypeRelation(Invalid.Relations[0]));
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::DuplicateKey,
			TEXT("identical repeated legal direct-interface row"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(1);
		Invalid.Relations[0].Target.StableKey = Invalid.TypeKey.Hash;
		Invalid.Dependencies[0].Target = Invalid.Relations[0].Target;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::ConflictingKey,
			TEXT("direct-interface target self-references enclosing TypeKey"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(1);
		Invalid.Relations[0].Target.StableKey = {};
		Invalid.Dependencies[0].Target = Invalid.Relations[0].Target;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::ZeroStableKey, TEXT("direct-interface target zero StableKey"));
		++AdditionalCalls;

		Invalid = MakeDirectInterfaces(1);
		Invalid.Relations[0].Target.ExpectedAbi = {};
		Invalid.Dependencies[0].Target = Invalid.Relations[0].Target;
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::MissingExpectedAbi,
			TEXT("direct-interface target missing ExpectedAbi"));
		++AdditionalCalls;

		struct FWrongRelationReferenceRow
		{
			ERelationForm Form;
			EAngelscriptCachedTypeRelationKind Kind;
			EAngelscriptCacheReferenceKind WrongKinds[2];
			const TCHAR* Name;
		};
		const FWrongRelationReferenceRow WrongReferenceRows[] = {
			{ERelationForm::ClassNone, EAngelscriptCachedTypeRelationKind::Base,
				{Environment, EAngelscriptCacheReferenceKind::ScriptFunction},
				TEXT("Class+None Base")},
			{ERelationForm::ClassNone,
				EAngelscriptCachedTypeRelationKind::ImplementedInterface,
				{Environment, EAngelscriptCacheReferenceKind::ScriptFunction},
				TEXT("Class+None ImplementedInterface")},
			{ERelationForm::OrdinaryUClass, EAngelscriptCachedTypeRelationKind::Base,
				{Environment, EAngelscriptCacheReferenceKind::ScriptFunction},
				TEXT("ordinary UClass Base")},
			{ERelationForm::OrdinaryUClass,
				EAngelscriptCachedTypeRelationKind::ShadowSuper,
				{Script, EAngelscriptCacheReferenceKind::ScriptFunction},
				TEXT("ordinary UClass ShadowSuper")},
			{ERelationForm::OrdinaryUClass,
				EAngelscriptCachedTypeRelationKind::CodeSuper,
				{Script, EAngelscriptCacheReferenceKind::ScriptFunction},
				TEXT("ordinary UClass CodeSuper")},
			{ERelationForm::OrdinaryUClass,
				EAngelscriptCachedTypeRelationKind::ImplementedInterface,
				{Script, EAngelscriptCacheReferenceKind::ScriptFunction},
				TEXT("ordinary UClass ImplementedInterface")},
			{ERelationForm::StaticsUClass, EAngelscriptCachedTypeRelationKind::CodeSuper,
				{Script, EAngelscriptCacheReferenceKind::ScriptFunction},
				TEXT("statics UClass CodeSuper")},
			{ERelationForm::InterfaceNone,
				EAngelscriptCachedTypeRelationKind::ImplementedInterface,
				{Environment, EAngelscriptCacheReferenceKind::ScriptFunction},
				TEXT("Interface+None ImplementedInterface")},
		};
		int32 WrongReferenceCalls = 0;
		for (const FWrongRelationReferenceRow& Row : WrongReferenceRows)
		{
			for (const EAngelscriptCacheReferenceKind WrongKind : Row.WrongKinds)
			{
				const FAngelscriptCachedTypeSchema Wrong = MakeRelationFixture(
					Row.Form, Row.Kind, 1, WrongKind);
				const FString Context = FString::Printf(TEXT("%s wrong ReferenceKind %u"),
					Row.Name, static_cast<uint8>(WrongKind));
				bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
					Wrong, FError::WrongReferenceKind, *Context);
				++WrongReferenceCalls;
				++AdditionalCalls;
			}
		}

		bPassed &= LocalAssert.AreEqual(165, MatrixCalls,
			TEXT("Relations literal form/kind/cardinality call count"));
		bPassed &= LocalAssert.AreEqual(16, WrongReferenceCalls,
			TEXT("Relations legal-coordinate wrong-reference call count"));
		bPassed &= LocalAssert.AreEqual(30, AdditionalCalls,
			TEXT("Relations additional literal call count"));
		bPassed &= LocalAssert.AreEqual(195, MatrixCalls + AdditionalCalls,
			TEXT("Relations total normal-producer call count"));
		TestRunner->AddInfo(FString::Printf(TEXT(
			"[CacheV2][TypeSchema][Relations][Producer] complete "
			"matrix=%d focused=%d wrong-reference=%d total=%d"),
			MatrixCalls, AdditionalCalls, WrongReferenceCalls,
			MatrixCalls + AdditionalCalls));

		ASSERT_THAT(IsTrue(bPassed,
			TEXT("normal producer Relations local rules and cardinality matrix")));
	}

	TEST_METHOD(NormalProducerRejectsLayoutInputRolesAndPairingAtomically)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);
		bool bPassed = true;
		using FError = EAngelscriptCacheValidationError;
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][LayoutInputs][Producer] begin "
			"legal=13 raw-kind=3 missing=5 extra=34 wrong-role=3 pairing=6 "
			"dependency-precedence=2 "
			"optional-mask=12 reference=12 range=9 stale-hash=1 expected-total=100"));

		const auto MakePlainClassWithBase = []()
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			FAngelscriptCachedTypeRelation Base;
			Base.RelationKind = EAngelscriptCachedTypeRelationKind::Base;
			Base.Target = MakeReference(
				EAngelscriptCacheReferenceKind::ScriptType, 0xe0, 0xe1);
			Schema.Relations.Add(Base);
			FAngelscriptCachedTypeLayoutInput Input;
			Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
			Input.Target = Base.Target;
			Input.BoundaryContribution = 0;
			Input.AlignmentContribution = 8;
			Schema.LayoutInputs.Add(Input);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Inheritance, Base.Target));
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};

		const FAngelscriptCachedTypeSchema PlainClassWithBase =
			MakePlainClassWithBase();
		const FAngelscriptCachedTypeSchema OrdinaryRoot =
			MakeOrdinaryUClassSchema(false);
		const FAngelscriptCachedTypeSchema OrdinaryDerived =
			MakeOrdinaryUClassSchema(true);
		const FAngelscriptCachedTypeSchema ReflectedUStruct =
			MakeReflectedUStructSchema();

		constexpr uint8 BaseRoleBit = 1u << 0;
		constexpr uint8 CodeRoleBit = 1u << 1;
		constexpr uint8 HeaderRoleBit = 1u << 2;
		struct FLayoutBaselineRow
		{
			const TCHAR* Name;
			FAngelscriptCachedTypeSchema Schema;
			uint8 ForbiddenRoleMask;
		};
		const FLayoutBaselineRow Baselines[] = {
			{TEXT("Class+None no Base"),
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Class),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
			{TEXT("Class+None with Base"), PlainClassWithBase,
				CodeRoleBit | HeaderRoleBit},
			{TEXT("ordinary root UClass"), OrdinaryRoot,
				BaseRoleBit | HeaderRoleBit},
			{TEXT("ordinary derived UClass"), OrdinaryDerived, HeaderRoleBit},
			{TEXT("statics UClass"), MakeStaticsUClassSchema(),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
			{TEXT("Struct+None"),
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
			{TEXT("Struct+UStruct"), ReflectedUStruct,
				BaseRoleBit | CodeRoleBit},
			{TEXT("Interface+None"),
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
			{TEXT("Enum+None"), MakeReflectionFormSchema(
				EAngelscriptCachedTypeKind::Enum,
				EAngelscriptCachedReflectionKind::None),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
			{TEXT("Enum+UEnum"), MakeEnumSchema(),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
			{TEXT("Delegate+UDelegate"), MakeCompleteDelegateSchema(),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
			{TEXT("Typedef+None"),
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
			{TEXT("Funcdef+None"),
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef),
				BaseRoleBit | CodeRoleBit | HeaderRoleBit},
		};

		const EAngelscriptCachedTypeLayoutInputKind RoleKinds[] = {
			EAngelscriptCachedTypeLayoutInputKind::BaseType,
			EAngelscriptCachedTypeLayoutInputKind::CodeRoot,
			EAngelscriptCachedTypeLayoutInputKind::StructHeader,
		};
		const TCHAR* RoleNames[] = {
			TEXT("BaseType"), TEXT("CodeRoot"), TEXT("StructHeader"),
		};

		const auto MakeLayoutInput = [](const EAngelscriptCachedTypeLayoutInputKind Kind)
		{
			FAngelscriptCachedTypeLayoutInput Input;
			Input.InputKind = Kind;
			switch (Kind)
			{
			case EAngelscriptCachedTypeLayoutInputKind::BaseType:
				Input.Target = MakeReference(
					EAngelscriptCacheReferenceKind::ScriptType, 0xe2, 0xe3);
				Input.BoundaryContribution = 0;
				Input.AlignmentContribution = 8;
				break;
			case EAngelscriptCachedTypeLayoutInputKind::CodeRoot:
				Input.Target = MakeReference(
					EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xe4, 0xe5);
				Input.BoundaryContribution = 0;
				Input.AlignmentContribution = 8;
				break;
			case EAngelscriptCachedTypeLayoutInputKind::StructHeader:
				Input.Target = MakeReference(
					EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xe6, 0xe7);
				Input.BoundaryContribution = 16;
				break;
			default:
				checkNoEntry();
			}
			return Input;
		};

		const auto FindInputIndex = [](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedTypeLayoutInputKind Kind) -> int32
		{
			for (int32 Index = 0; Index < Schema.LayoutInputs.Num(); ++Index)
			{
				if (Schema.LayoutInputs[Index].InputKind == Kind)
				{
					return Index;
				}
			}
			checkNoEntry();
			return INDEX_NONE;
		};

		const auto FindRelationIndex = [](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedTypeRelationKind Kind) -> int32
		{
			for (int32 Index = 0; Index < Schema.Relations.Num(); ++Index)
			{
				if (Schema.Relations[Index].RelationKind == Kind)
				{
					return Index;
				}
			}
			checkNoEntry();
			return INDEX_NONE;
		};

		int32 PositiveCalls = 0;
		for (const FLayoutBaselineRow& Baseline : Baselines)
		{
			bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(
				*TestRunner, Baseline.Schema, Baseline.Name);
			++PositiveCalls;
		}

		int32 RawKindCalls = 0;
		const uint8 RawInputKinds[] = {0, 4, 255};
		for (const uint8 RawKind : RawInputKinds)
		{
			FAngelscriptCachedTypeSchema Invalid = OrdinaryRoot;
			Invalid.LayoutInputs[0].InputKind =
				static_cast<EAngelscriptCachedTypeLayoutInputKind>(RawKind);
			const FString Context = FString::Printf(TEXT("raw InputKind %u"), RawKind);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				Invalid, FError::UnknownEnumValue, *Context);
			++RawKindCalls;
		}

		struct FMissingInputRow
		{
			FAngelscriptCachedTypeSchema Schema;
			EAngelscriptCachedTypeLayoutInputKind Kind;
			const TCHAR* Context;
		};
		const FMissingInputRow MissingRows[] = {
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType,
				TEXT("Class+None BaseType missing")},
			{OrdinaryDerived, EAngelscriptCachedTypeLayoutInputKind::BaseType,
				TEXT("ordinary derived UClass BaseType missing")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot,
				TEXT("ordinary root UClass CodeRoot missing")},
			{OrdinaryDerived, EAngelscriptCachedTypeLayoutInputKind::CodeRoot,
				TEXT("ordinary derived UClass CodeRoot missing")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::StructHeader,
				TEXT("Struct+UStruct StructHeader missing")},
		};
		int32 MissingCalls = 0;
		for (const FMissingInputRow& Row : MissingRows)
		{
			FAngelscriptCachedTypeSchema Invalid = Row.Schema;
			Invalid.LayoutInputs.RemoveAt(FindInputIndex(Invalid, Row.Kind));
			FinalizeValidFixtureHashes(Invalid);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				Invalid, FError::InvalidPresence, Row.Context);
			++MissingCalls;
		}

		int32 ExtraCalls = 0;
		for (const FLayoutBaselineRow& Baseline : Baselines)
		{
			for (int32 RoleIndex = 0; RoleIndex < 3; ++RoleIndex)
			{
				const uint8 RoleBit = 1u << RoleIndex;
				if ((Baseline.ForbiddenRoleMask & RoleBit) == 0)
				{
					continue;
				}
				FAngelscriptCachedTypeSchema Invalid = Baseline.Schema;
				Invalid.LayoutInputs.Add(MakeLayoutInput(RoleKinds[RoleIndex]));
				Invalid.LayoutInputs.Sort([](const auto& A, const auto& B)
				{
					return static_cast<uint8>(A.InputKind)
						< static_cast<uint8>(B.InputKind);
				});
				FinalizeValidFixtureHashes(Invalid);
				const FString Context = FString::Printf(TEXT("%s forbids extra %s"),
					Baseline.Name, RoleNames[RoleIndex]);
				bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
					Invalid, FError::InvalidPresence, *Context);
				++ExtraCalls;
			}
		}

		struct FWrongRoleRow
		{
			FAngelscriptCachedTypeSchema Schema;
			EAngelscriptCachedTypeLayoutInputKind ReplacementKind;
			const TCHAR* Context;
		};
		const FWrongRoleRow WrongRoleRows[] = {
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::CodeRoot,
				TEXT("Class+None BaseType replaced by CodeRoot")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::StructHeader,
				TEXT("ordinary root CodeRoot replaced by StructHeader")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::BaseType,
				TEXT("UStruct StructHeader replaced by BaseType")},
		};
		int32 WrongRoleCalls = 0;
		for (const FWrongRoleRow& Row : WrongRoleRows)
		{
			FAngelscriptCachedTypeSchema Invalid = Row.Schema;
			Invalid.LayoutInputs[0] = MakeLayoutInput(Row.ReplacementKind);
			FinalizeValidFixtureHashes(Invalid);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				Invalid, FError::InvalidPresence, Row.Context);
			++WrongRoleCalls;
		}

		int32 PairingCalls = 0;
		int32 DependencyPrecedenceCalls = 0;
		FAngelscriptCachedTypeSchema Invalid = PlainClassWithBase;
		Invalid.LayoutInputs[FindInputIndex(Invalid,
			EAngelscriptCachedTypeLayoutInputKind::BaseType)].Target.StableKey =
			MakeHash(0xe8);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::InvalidQualifierCombination,
			TEXT("BaseType differs from Base by StableKey only"));
		++PairingCalls;

		Invalid = PlainClassWithBase;
		Invalid.LayoutInputs[FindInputIndex(Invalid,
			EAngelscriptCachedTypeLayoutInputKind::BaseType)].Target.ExpectedAbi =
			MakeHash(0xe9);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::InvalidQualifierCombination,
			TEXT("BaseType differs from Base by ExpectedAbi only"));
		++PairingCalls;

		Invalid = OrdinaryRoot;
		Invalid.LayoutInputs[FindInputIndex(Invalid,
			EAngelscriptCachedTypeLayoutInputKind::CodeRoot)].Target.StableKey =
			MakeHash(0xea);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::InvalidQualifierCombination,
			TEXT("CodeRoot differs from equal Shadow and Code peers by StableKey"));
		++PairingCalls;

		Invalid = OrdinaryRoot;
		Invalid.LayoutInputs[FindInputIndex(Invalid,
			EAngelscriptCachedTypeLayoutInputKind::CodeRoot)].Target.ExpectedAbi =
			MakeHash(0xeb);
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::InvalidQualifierCombination,
			TEXT("CodeRoot differs from equal Shadow and Code peers by ExpectedAbi"));
		++PairingCalls;

		Invalid = OrdinaryRoot;
		{
			FAngelscriptCachedTypeRelation& Code = Invalid.Relations[
				FindRelationIndex(Invalid, EAngelscriptCachedTypeRelationKind::CodeSuper)];
			Code.Target.StableKey = MakeHash(0xec);
			Invalid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, Code.Target));
		}
		Invalid.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::InvalidQualifierCombination,
			TEXT("CodeRoot equals Shadow while CodeSuper differs by StableKey"));
		++PairingCalls;

		Invalid = OrdinaryRoot;
		{
			FAngelscriptCachedTypeRelation& Code = Invalid.Relations[
				FindRelationIndex(Invalid, EAngelscriptCachedTypeRelationKind::CodeSuper)];
			Code.Target.ExpectedAbi = MakeHash(0xed);
			Invalid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, Code.Target));
		}
		Invalid.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::ConflictingKey,
			TEXT("CodeSuper ABI conflict wins in Dependencies before LayoutInput pairing"));
		++DependencyPrecedenceCalls;

		Invalid = OrdinaryRoot;
		{
			FAngelscriptCachedTypeRelation& Shadow = Invalid.Relations[
				FindRelationIndex(Invalid, EAngelscriptCachedTypeRelationKind::ShadowSuper)];
			Shadow.Target.StableKey = MakeHash(0xee);
			Invalid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, Shadow.Target));
		}
		Invalid.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::InvalidQualifierCombination,
			TEXT("CodeRoot equals CodeSuper while Shadow differs by StableKey"));
		++PairingCalls;

		Invalid = OrdinaryRoot;
		{
			FAngelscriptCachedTypeRelation& Shadow = Invalid.Relations[
				FindRelationIndex(Invalid, EAngelscriptCachedTypeRelationKind::ShadowSuper)];
			Shadow.Target.ExpectedAbi = MakeHash(0xef);
			Invalid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, Shadow.Target));
		}
		Invalid.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Invalid);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::ConflictingKey,
			TEXT("ShadowSuper ABI conflict wins in Dependencies before LayoutInput pairing"));
		++DependencyPrecedenceCalls;

		struct FOptionalMaskRow
		{
			FAngelscriptCachedTypeSchema Schema;
			EAngelscriptCachedTypeLayoutInputKind Kind;
			uint8 Mask;
			const TCHAR* Context;
		};
		const FOptionalMaskRow OptionalMaskRows[] = {
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType, 0,
				TEXT("BaseType optional mask 0")},
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType, 1,
				TEXT("BaseType optional mask 1")},
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType, 2,
				TEXT("BaseType optional mask 2")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 0,
				TEXT("root CodeRoot optional mask 0")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 1,
				TEXT("root CodeRoot optional mask 1")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 2,
				TEXT("root CodeRoot optional mask 2")},
			{OrdinaryDerived, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 0,
				TEXT("derived CodeRoot optional mask 0")},
			{OrdinaryDerived, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 1,
				TEXT("derived CodeRoot optional mask 1")},
			{OrdinaryDerived, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 3,
				TEXT("derived CodeRoot optional mask 3")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::StructHeader, 0,
				TEXT("StructHeader optional mask 0")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::StructHeader, 2,
				TEXT("StructHeader optional mask 2")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::StructHeader, 3,
				TEXT("StructHeader optional mask 3")},
		};
		int32 OptionalMaskCalls = 0;
		for (const FOptionalMaskRow& Row : OptionalMaskRows)
		{
			FAngelscriptCachedTypeSchema Masked = Row.Schema;
			FAngelscriptCachedTypeLayoutInput& Input =
				Masked.LayoutInputs[FindInputIndex(Masked, Row.Kind)];
			if ((Row.Mask & 0x1) != 0)
			{
				if (!Input.BoundaryContribution.IsSet())
				{
					Input.BoundaryContribution = 0;
				}
			}
			else
			{
				Input.BoundaryContribution.Reset();
			}
			if ((Row.Mask & 0x2) != 0)
			{
				if (!Input.AlignmentContribution.IsSet())
				{
					Input.AlignmentContribution = 8;
				}
			}
			else
			{
				Input.AlignmentContribution.Reset();
			}
			FinalizeValidFixtureHashes(Masked);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				Masked, FError::InvalidPresence, Row.Context);
			++OptionalMaskCalls;
		}

		struct FRoleBaselineRow
		{
			FAngelscriptCachedTypeSchema Schema;
			EAngelscriptCachedTypeLayoutInputKind Kind;
			const TCHAR* Name;
		};
		const FRoleBaselineRow RoleBaselines[] = {
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType,
				TEXT("BaseType")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot,
				TEXT("CodeRoot")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::StructHeader,
				TEXT("StructHeader")},
		};

		int32 ZeroKeyCalls = 0;
		for (const FRoleBaselineRow& Row : RoleBaselines)
		{
			FAngelscriptCachedTypeSchema ZeroKey = Row.Schema;
			ZeroKey.LayoutInputs[FindInputIndex(ZeroKey, Row.Kind)].Target.StableKey = {};
			const FString Context = FString::Printf(TEXT("%s target zero StableKey"),
				Row.Name);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				ZeroKey, FError::ZeroStableKey, *Context);
			++ZeroKeyCalls;
		}

		int32 MissingAbiCalls = 0;
		for (const FRoleBaselineRow& Row : RoleBaselines)
		{
			FAngelscriptCachedTypeSchema MissingAbi = Row.Schema;
			MissingAbi.LayoutInputs[
				FindInputIndex(MissingAbi, Row.Kind)].Target.ExpectedAbi = {};
			const FString Context = FString::Printf(TEXT("%s target missing ExpectedAbi"),
				Row.Name);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				MissingAbi, FError::MissingExpectedAbi, *Context);
			++MissingAbiCalls;
		}

		struct FWrongInputReferenceRow
		{
			FAngelscriptCachedTypeSchema Schema;
			EAngelscriptCachedTypeLayoutInputKind Kind;
			EAngelscriptCacheReferenceKind WrongKind;
			const TCHAR* Context;
		};
		const FWrongInputReferenceRow WrongReferenceRows[] = {
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol,
				TEXT("BaseType EnvironmentSymbol target")},
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType,
				EAngelscriptCacheReferenceKind::ScriptFunction,
				TEXT("BaseType ScriptFunction target")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot,
				EAngelscriptCacheReferenceKind::ScriptType,
				TEXT("CodeRoot ScriptType target")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot,
				EAngelscriptCacheReferenceKind::ScriptFunction,
				TEXT("CodeRoot ScriptFunction target")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::StructHeader,
				EAngelscriptCacheReferenceKind::ScriptType,
				TEXT("StructHeader ScriptType target")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::StructHeader,
				EAngelscriptCacheReferenceKind::ScriptFunction,
				TEXT("StructHeader ScriptFunction target")},
		};
		int32 WrongReferenceCalls = 0;
		for (const FWrongInputReferenceRow& Row : WrongReferenceRows)
		{
			FAngelscriptCachedTypeSchema Wrong = Row.Schema;
			Wrong.LayoutInputs[FindInputIndex(Wrong, Row.Kind)].Target.Kind = Row.WrongKind;
			FinalizeValidFixtureHashes(Wrong);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				Wrong, FError::WrongReferenceKind, Row.Context);
			++WrongReferenceCalls;
		}

		struct FAlignmentRow
		{
			FAngelscriptCachedTypeSchema Schema;
			EAngelscriptCachedTypeLayoutInputKind Kind;
			uint32 Value;
			const TCHAR* Context;
		};
		const FAlignmentRow AlignmentRows[] = {
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType, 0,
				TEXT("BaseType alignment zero")},
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType, 3,
				TEXT("BaseType alignment non-power-of-two three")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 0,
				TEXT("CodeRoot alignment zero")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 3,
				TEXT("CodeRoot alignment non-power-of-two three")},
		};
		int32 InvalidAlignmentCalls = 0;
		for (const FAlignmentRow& Row : AlignmentRows)
		{
			FAngelscriptCachedTypeSchema Misaligned = Row.Schema;
			Misaligned.LayoutInputs[
				FindInputIndex(Misaligned, Row.Kind)].AlignmentContribution = Row.Value;
			FinalizeValidFixtureHashes(Misaligned);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				Misaligned, FError::InvalidQualifierCombination, Row.Context);
			++InvalidAlignmentCalls;
		}

		struct FOverflowRow
		{
			FAngelscriptCachedTypeSchema Schema;
			EAngelscriptCachedTypeLayoutInputKind Kind;
			bool bBoundary;
			const TCHAR* Context;
		};
		const FOverflowRow OverflowRows[] = {
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType, true,
				TEXT("BaseType boundary above int32")},
			{PlainClassWithBase, EAngelscriptCachedTypeLayoutInputKind::BaseType, false,
				TEXT("BaseType alignment above int32")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, true,
				TEXT("root CodeRoot boundary above int32")},
			{OrdinaryRoot, EAngelscriptCachedTypeLayoutInputKind::CodeRoot, false,
				TEXT("CodeRoot alignment above int32")},
			{ReflectedUStruct, EAngelscriptCachedTypeLayoutInputKind::StructHeader, true,
				TEXT("StructHeader boundary above int32")},
		};
		constexpr uint32 OverflowValue = static_cast<uint32>(MAX_int32) + 1u;
		int32 OverflowCalls = 0;
		for (const FOverflowRow& Row : OverflowRows)
		{
			FAngelscriptCachedTypeSchema Overflowed = Row.Schema;
			FAngelscriptCachedTypeLayoutInput& Input =
				Overflowed.LayoutInputs[FindInputIndex(Overflowed, Row.Kind)];
			if (Row.bBoundary)
			{
				Input.BoundaryContribution = OverflowValue;
			}
			else
			{
				Input.AlignmentContribution = OverflowValue;
			}
			FinalizeValidFixtureHashes(Overflowed);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
				Overflowed, FError::Overflow, Row.Context);
			++OverflowCalls;
		}

		Invalid = OrdinaryRoot;
		Invalid.LayoutInputs[0].LayoutInputHash = MakeHash(0xf0);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			FError::DerivedHashMismatch,
			TEXT("stale LayoutInputHash after legal finalization"));
		const int32 StaleHashCalls = 1;

		bPassed &= LocalAssert.AreEqual(13, PositiveCalls,
			TEXT("LayoutInputs legal baseline success call count"));
		bPassed &= LocalAssert.AreEqual(3, RawKindCalls,
			TEXT("LayoutInputs raw-kind call count"));
		bPassed &= LocalAssert.AreEqual(5, MissingCalls,
			TEXT("LayoutInputs required-absence call count"));
		bPassed &= LocalAssert.AreEqual(34, ExtraCalls,
			TEXT("LayoutInputs forbidden-extra coordinate count"));
		bPassed &= LocalAssert.AreEqual(3, WrongRoleCalls,
			TEXT("LayoutInputs wrong-role call count"));
		bPassed &= LocalAssert.AreEqual(6, PairingCalls,
			TEXT("LayoutInputs relation-pairing call count"));
		bPassed &= LocalAssert.AreEqual(2, DependencyPrecedenceCalls,
			TEXT("LayoutInputs dependency-precedence call count"));
		bPassed &= LocalAssert.AreEqual(12, OptionalMaskCalls,
			TEXT("LayoutInputs optional-mask call count"));
		bPassed &= LocalAssert.AreEqual(3, ZeroKeyCalls,
			TEXT("LayoutInputs zero-key call count"));
		bPassed &= LocalAssert.AreEqual(3, MissingAbiCalls,
			TEXT("LayoutInputs missing-ABI call count"));
		bPassed &= LocalAssert.AreEqual(6, WrongReferenceCalls,
			TEXT("LayoutInputs wrong-reference call count"));
		bPassed &= LocalAssert.AreEqual(4, InvalidAlignmentCalls,
			TEXT("LayoutInputs invalid-alignment call count"));
		bPassed &= LocalAssert.AreEqual(5, OverflowCalls,
			TEXT("LayoutInputs overflow call count"));
		bPassed &= LocalAssert.AreEqual(1, StaleHashCalls,
			TEXT("LayoutInputs stale-hash call count"));
		bPassed &= LocalAssert.AreEqual(100,
			PositiveCalls + RawKindCalls + MissingCalls + ExtraCalls
				+ WrongRoleCalls + PairingCalls + DependencyPrecedenceCalls
				+ OptionalMaskCalls
				+ ZeroKeyCalls + MissingAbiCalls + WrongReferenceCalls
				+ InvalidAlignmentCalls + OverflowCalls + StaleHashCalls,
			TEXT("LayoutInputs total normal-producer call count"));
		TestRunner->AddInfo(FString::Printf(TEXT(
			"[CacheV2][TypeSchema][LayoutInputs][Producer] complete "
			"legal=%d raw-kind=%d missing=%d extra=%d wrong-role=%d pairing=%d "
			"dependency-precedence=%d "
			"optional-mask=%d zero-key=%d missing-abi=%d wrong-reference=%d "
			"invalid-alignment=%d overflow=%d stale-hash=%d total=%d"),
			PositiveCalls, RawKindCalls, MissingCalls, ExtraCalls, WrongRoleCalls,
			PairingCalls, DependencyPrecedenceCalls, OptionalMaskCalls,
			ZeroKeyCalls, MissingAbiCalls,
			WrongReferenceCalls, InvalidAlignmentCalls, OverflowCalls, StaleHashCalls,
			PositiveCalls + RawKindCalls + MissingCalls + ExtraCalls
				+ WrongRoleCalls + PairingCalls + DependencyPrecedenceCalls
				+ OptionalMaskCalls
				+ ZeroKeyCalls + MissingAbiCalls + WrongReferenceCalls
				+ InvalidAlignmentCalls + OverflowCalls + StaleHashCalls));

		ASSERT_THAT(IsTrue(bPassed,
			TEXT("normal producer LayoutInput roles, masks, pairing, and ranges")));
	}

	TEST_METHOD(NormalProducerRejectsPropertyStorageFlagsAndLayoutReplayAtomically)
	{
		using FError = EAngelscriptCacheValidationError;
		FNoDiscardAsserter LocalAssert(*TestRunner);
		bool bPassed = true;
		int32 RegionASuccessCalls = 0;
		int32 RegionANegativeCalls = 0;
		int32 RegionACalls = 0;

		const auto ExpectSuccess = [&](const FAngelscriptCachedTypeSchema& Schema,
			const TCHAR* Context)
		{
			bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(
				*TestRunner, Schema, Context);
			++RegionASuccessCalls;
			++RegionACalls;
		};
		const auto ExpectFailure = [&](const FAngelscriptCachedTypeSchema& Schema,
			const FError ExpectedError, const TCHAR* Context)
		{
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(
				*TestRunner, Schema, ExpectedError, Context);
			++RegionANegativeCalls;
			++RegionACalls;
		};
		const auto MakeOwnerWithProperty = [](FAngelscriptCachedTypeSchema Schema)
		{
			FAngelscriptCachedPropertySchema Property =
				MakeCompleteDelegateSchema().OrderedProperties[0];
			Property.SemanticByteOffset = Schema.Layout.BasePropertyBoundary;
			Property.PropertySemanticFlags = 0;
			Property.ReplicationCondition =
				EAngelscriptCachedReplicationCondition::None;
			Property.Metadata.Reset();
			Schema.OrderedProperties.Add(MoveTemp(Property));
			Schema.Layout.SemanticAlignment = 8;
			Schema.Layout.SemanticSize = Align(
				Schema.Layout.BasePropertyBoundary + uint64(4), uint64(8));
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};
		const auto MakeFixedLayoutOwnerWithForbiddenProperty =
			[&MakeOwnerWithProperty](FAngelscriptCachedTypeSchema Schema)
		{
			const auto FixedLayout = Schema.Layout;
			Schema = MakeOwnerWithProperty(MoveTemp(Schema));
			Schema.Layout = FixedLayout;
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};

		FAngelscriptCachedTypeSchema EmptyDelegate = MakeCompleteDelegateSchema();
		EmptyDelegate.OrderedProperties.Reset();
		EmptyDelegate.Layout.SemanticSize = 0;
		EmptyDelegate.Layout.SemanticAlignment = 8;
		EmptyDelegate.Layout.BasePropertyBoundary = 0;
		FinalizeValidFixtureHashes(EmptyDelegate);
		ExpectSuccess(EmptyDelegate,
			TEXT("empty Delegate normalized layout (0,8,0)"));

		const FAngelscriptCachedTypeSchema PlainClass =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainClass);
		const FAngelscriptCachedTypeSchema OrdinaryRoot =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::OrdinaryUClass);
		const FAngelscriptCachedTypeSchema OrdinaryDerived =
			MakeOwnerWithProperty(MakeOrdinaryUClassSchema(true));
		const FAngelscriptCachedTypeSchema PlainStruct =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		const FAngelscriptCachedTypeSchema ReflectedUStruct =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::UStruct);
		ExpectSuccess(PlainClass, TEXT("Class+None property owner baseline"));
		ExpectSuccess(OrdinaryRoot, TEXT("ordinary root UClass property owner baseline"));
		ExpectSuccess(OrdinaryDerived,
			TEXT("ordinary script-derived UClass property owner baseline"));
		ExpectSuccess(PlainStruct, TEXT("Struct+None property owner baseline"));
		ExpectSuccess(ReflectedUStruct, TEXT("Struct+UStruct property owner baseline"));

		FAngelscriptCachedTypeSchema ProtectedAccess = PlainStruct;
		ProtectedAccess.OrderedProperties[0].Access =
			EAngelscriptCachedMemberAccess::Protected;
		FinalizeValidFixtureHashes(ProtectedAccess);
		ExpectSuccess(ProtectedAccess, TEXT("Protected property access"));

		struct FForbiddenOwnerRow
		{
			FAngelscriptCachedTypeSchema Schema;
			const TCHAR* Context;
		};
		const FForbiddenOwnerRow ForbiddenOwnerRows[] = {
			{MakeOwnerWithProperty(MakeStaticsUClassSchema()),
				TEXT("StaticsClass forbids properties")},
			{MakeFixedLayoutOwnerWithForbiddenProperty(
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface)),
				TEXT("Interface+None forbids properties")},
			{MakeFixedLayoutOwnerWithForbiddenProperty(MakeReflectionFormSchema(
				EAngelscriptCachedTypeKind::Enum,
				EAngelscriptCachedReflectionKind::None)),
				TEXT("Enum+None forbids properties")},
			{MakeFixedLayoutOwnerWithForbiddenProperty(MakeEnumSchema()),
				TEXT("Enum+UEnum forbids properties")},
			{MakeOwnerWithProperty(MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef)),
				TEXT("Typedef forbids properties")},
			{MakeOwnerWithProperty(MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef)),
				TEXT("Funcdef forbids properties")},
		};
		for (const FForbiddenOwnerRow& Row : ForbiddenOwnerRows)
		{
			ExpectFailure(Row.Schema, FError::InvalidPresence, Row.Context);
		}

		struct FOrdinalValueRow
		{
			uint32 Ordinals[3];
			FError ExpectedError;
			const TCHAR* Context;
		};
		const FOrdinalValueRow OrdinalValueRows[] = {
			{{0, 2, 3}, FError::OrdinalGap, TEXT("property ordinal gap [0,2,3]")},
			{{0, 1, 3}, FError::OrdinalGap, TEXT("property ordinal gap [0,1,3]")},
			{{0, 0, 2}, FError::DuplicateOrdinal,
				TEXT("property duplicate ordinal [0,0,2]")},
			{{0, 1, 1}, FError::DuplicateOrdinal,
				TEXT("property duplicate ordinal [0,1,1]")},
		};
		for (const FOrdinalValueRow& Row : OrdinalValueRows)
		{
			FAngelscriptCachedTypeSchema Invalid = MakeThreePropertyLayoutSchema();
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Invalid.OrderedProperties[Index].LayoutOrdinal = Row.Ordinals[Index];
			}
			FinalizeValidFixtureHashes(Invalid);
			ExpectFailure(Invalid, Row.ExpectedError, Row.Context);
		}

		FAngelscriptCachedTypeSchema Invalid = MakeThreePropertyLayoutSchema();
		Invalid.OrderedProperties.Swap(0, 1);
		FinalizeValidFixtureHashes(Invalid);
		ExpectFailure(Invalid, FError::NonCanonicalOrder,
			TEXT("property stored-row order [1,0,2]"));

		Invalid = MakeThreePropertyLayoutSchema();
		Invalid.OrderedProperties.Swap(1, 2);
		FinalizeValidFixtureHashes(Invalid);
		ExpectFailure(Invalid, FError::NonCanonicalOrder,
			TEXT("property stored-row order [0,2,1]"));

		Invalid = PlainStruct;
		Invalid.OrderedProperties[0].PropertyKey = {};
		ExpectFailure(Invalid, FError::ZeroStableKey, TEXT("zero PropertyKey"));

		Invalid = PlainStruct;
		Invalid.OrderedProperties[0].CanonicalName.Reset();
		ExpectFailure(Invalid, FError::InvalidPresence, TEXT("empty property name"));

		for (const uint8 RawAccess : {uint8(0), uint8(255)})
		{
			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Access =
				static_cast<EAngelscriptCachedMemberAccess>(RawAccess);
			const FString Context = FString::Printf(
				TEXT("property Access raw %u"), RawAccess);
			ExpectFailure(Invalid, FError::UnknownEnumValue, *Context);
		}

		for (const uint8 RawStorageKind : {uint8(0), uint8(255)})
		{
			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].StorageKind =
				static_cast<EAngelscriptCachedPropertyStorageKind>(RawStorageKind);
			const FString Context = FString::Printf(
				TEXT("property StorageKind raw %u"), RawStorageKind);
			ExpectFailure(Invalid, FError::UnknownEnumValue, *Context);
		}

		for (const uint8 RawKind : {uint8(0), uint8(5), uint8(255)})
		{
			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Type.Kind =
				static_cast<EAngelscriptCachedDataTypeKind>(RawKind);
			const FString Context = FString::Printf(
				TEXT("property DataTypeKind raw %u"), RawKind);
			ExpectFailure(Invalid, FError::UnknownEnumValue, *Context);
		}

		for (const uint32 UnknownQualifier : {uint32(0x40), uint32(0xffffffff)})
		{
			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Type.QualifierFlags = UnknownQualifier;
			const FString Context = FString::Printf(
				TEXT("property datatype unknown qualifier 0x%08x"), UnknownQualifier);
			ExpectFailure(Invalid, FError::UnknownFlags, *Context);
		}

		Invalid = PlainStruct;
		Invalid.OrderedProperties[0].Type.Primitive =
			EAngelscriptCachedPrimitiveType::Invalid;
		ExpectFailure(Invalid, FError::InvalidPresence,
			TEXT("Primitive datatype uses Invalid sentinel"));

		Invalid = PlainStruct;
		Invalid.OrderedProperties[0].Type.TypeReference = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, 0xe0, 0xe1);
		ExpectFailure(Invalid, FError::InvalidPresence,
			TEXT("Primitive datatype carries TypeReference"));

		struct FReferenceKindRow
		{
			EAngelscriptCachedDataTypeKind DataTypeKind;
			EAngelscriptCacheReferenceKind ExpectedReferenceKind;
			EAngelscriptCacheReferenceKind WrongReferenceKind;
			const TCHAR* Name;
		};
		const FReferenceKindRow ReferenceKindRows[] = {
			{EAngelscriptCachedDataTypeKind::ScriptType,
				EAngelscriptCacheReferenceKind::ScriptType,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, TEXT("ScriptType")},
			{EAngelscriptCachedDataTypeKind::EnvironmentType,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol,
				EAngelscriptCacheReferenceKind::ScriptType, TEXT("EnvironmentType")},
		};
		for (const FReferenceKindRow& Row : ReferenceKindRows)
		{
			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Type.Kind = Row.DataTypeKind;
			Invalid.OrderedProperties[0].Type.TypeReference = MakeReference(
				Row.ExpectedReferenceKind, 0xe2, 0xe3);
			const FString NonInvalidPrimitiveContext = FString::Printf(
				TEXT("%s datatype carries non-Invalid Primitive"), Row.Name);
			ExpectFailure(Invalid, FError::InvalidPresence,
				*NonInvalidPrimitiveContext);

			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Type.Kind = Row.DataTypeKind;
			Invalid.OrderedProperties[0].Type.Primitive =
				EAngelscriptCachedPrimitiveType::Invalid;
			Invalid.OrderedProperties[0].Type.TypeReference.Reset();
			const FString MissingReferenceContext = FString::Printf(
				TEXT("%s datatype missing TypeReference"), Row.Name);
			ExpectFailure(Invalid, FError::InvalidPresence, *MissingReferenceContext);

			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Type.Kind = Row.DataTypeKind;
			Invalid.OrderedProperties[0].Type.Primitive =
				EAngelscriptCachedPrimitiveType::Invalid;
			Invalid.OrderedProperties[0].Type.TypeReference = MakeReference(
				Row.WrongReferenceKind, 0xe4, 0xe5);
			const FString WrongReferenceContext = FString::Printf(
				TEXT("%s datatype carries wrong reference kind"), Row.Name);
			ExpectFailure(Invalid, FError::WrongReferenceKind, *WrongReferenceContext);

			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Type.Kind = Row.DataTypeKind;
			Invalid.OrderedProperties[0].Type.Primitive =
				EAngelscriptCachedPrimitiveType::Invalid;
			Invalid.OrderedProperties[0].Type.TypeReference = MakeReference(
				Row.ExpectedReferenceKind, 0xe6, 0xe7);
			Invalid.OrderedProperties[0].Type.TypeReference->StableKey = {};
			const FString ZeroKeyContext = FString::Printf(
				TEXT("%s datatype reference zero key"), Row.Name);
			ExpectFailure(Invalid, FError::ZeroStableKey, *ZeroKeyContext);

			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Type.Kind = Row.DataTypeKind;
			Invalid.OrderedProperties[0].Type.Primitive =
				EAngelscriptCachedPrimitiveType::Invalid;
			Invalid.OrderedProperties[0].Type.TypeReference = MakeReference(
				Row.ExpectedReferenceKind, 0xe8, 0xe9);
			Invalid.OrderedProperties[0].Type.TypeReference->ExpectedAbi = {};
			const FString MissingAbiContext = FString::Printf(
				TEXT("%s datatype reference missing ABI"), Row.Name);
			ExpectFailure(Invalid, FError::MissingExpectedAbi, *MissingAbiContext);
		}

		Invalid = PlainStruct;
		Invalid.OrderedProperties[0].Type.Kind = EAngelscriptCachedDataTypeKind::Auto;
		Invalid.OrderedProperties[0].Type.QualifierFlags = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::Auto);
		Invalid.OrderedProperties[0].Type.TypeReference.Reset();
		ExpectFailure(Invalid, FError::InvalidQualifierCombination,
			TEXT("Auto datatype carries non-Invalid Primitive"));

		Invalid = PlainStruct;
		Invalid.OrderedProperties[0].Type.Kind = EAngelscriptCachedDataTypeKind::Auto;
		Invalid.OrderedProperties[0].Type.QualifierFlags = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::Auto);
		Invalid.OrderedProperties[0].Type.Primitive =
			EAngelscriptCachedPrimitiveType::Invalid;
		Invalid.OrderedProperties[0].Type.TypeReference = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, 0xea, 0xeb);
		ExpectFailure(Invalid, FError::InvalidQualifierCombination,
			TEXT("Auto datatype carries TypeReference"));

		Invalid = PlainStruct;
		Invalid.OrderedProperties[0].Type =
			MakePrimitive(EAngelscriptCachedPrimitiveType::Void);
		FinalizeValidFixtureHashes(Invalid);
		ExpectFailure(Invalid, FError::InvalidQualifierCombination,
			TEXT("Void primitive cannot back property storage"));

		for (const uint8 RawPrimitive : {uint8(13), uint8(255)})
		{
			Invalid = PlainStruct;
			Invalid.OrderedProperties[0].Type.Primitive =
				static_cast<EAngelscriptCachedPrimitiveType>(RawPrimitive);
			const FString Context = FString::Printf(
				TEXT("property PrimitiveType raw %u"), RawPrimitive);
			ExpectFailure(Invalid, FError::UnknownEnumValue, *Context);
		}

		bPassed &= LocalAssert.AreEqual(7, RegionASuccessCalls,
			TEXT("Region A success normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(40, RegionANegativeCalls,
			TEXT("Region A negative normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(47, RegionACalls,
			TEXT("Region A total normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(47,
			RegionASuccessCalls + RegionANegativeCalls,
			TEXT("Slice 4 checkpoint A grand total normal-producer call count"));

		int32 RegionBSuccessCalls = 0;
		int32 RegionBNegativeCalls = 0;
		int32 RegionBCalls = 0;
		int32 RegionBSuccessGroupCalls[2][4] = {};
		int32 RegionBFailureGroupCalls[2][4] = {};
		bool RegionBSeen[2][4][64] = {};
		int32 RegionBLedgerMarkedCalls = 0;
		int32 RegionBLedgerDuplicateCalls = 0;

		const auto MarkRegionBCoordinate = [&](const EAngelscriptCachedPropertyStorageKind StorageKind,
			const EAngelscriptCachedDataTypeKind DataTypeKind, const uint32 QualifierMask,
			const TCHAR* Context)
		{
			const int32 StorageIndex = static_cast<int32>(StorageKind) - 1;
			const int32 DataTypeIndex = static_cast<int32>(DataTypeKind) - 1;
			const bool bCoordinateInRange = StorageIndex >= 0 && StorageIndex < 2
				&& DataTypeIndex >= 0 && DataTypeIndex < 4 && QualifierMask < 64;
			bPassed &= LocalAssert.IsTrue(bCoordinateInRange,
				*FString::Printf(TEXT("%s: Region B ledger coordinate range"), Context));
			if (!bCoordinateInRange)
			{
				return;
			}
			if (RegionBSeen[StorageIndex][DataTypeIndex][QualifierMask])
			{
				++RegionBLedgerDuplicateCalls;
			}
			else
			{
				RegionBSeen[StorageIndex][DataTypeIndex][QualifierMask] = true;
				++RegionBLedgerMarkedCalls;
			}
		};

		const FAngelscriptCacheV1StorageLayout RegionBObjectHandleLayout =
			FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
				.GetObjectHandleStorageLayout();
		const uint32 RegionBCanonicalQualifierMasks[2][4] = {
			{0x00, 0x00, 0x00, 0x10},
			{0x00, 0x04, 0x04, 0x10},
		};
		const auto MakeRegionBFinalizedBaseline = [&](const EAngelscriptCachedPropertyStorageKind StorageKind,
			const EAngelscriptCachedDataTypeKind DataTypeKind)
		{
			FAngelscriptCachedTypeSchema Schema = PlainStruct;
			FAngelscriptCachedPropertySchema& Property = Schema.OrderedProperties[0];
			Property.StorageKind = StorageKind;
			Property.Type = {};
			Property.Type.Kind = DataTypeKind;
			Property.Type.QualifierFlags = RegionBCanonicalQualifierMasks[
				static_cast<int32>(StorageKind) - 1][static_cast<int32>(DataTypeKind) - 1];
			if (DataTypeKind == EAngelscriptCachedDataTypeKind::Primitive)
			{
				Property.Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
			}
			if (DataTypeKind == EAngelscriptCachedDataTypeKind::ScriptType)
			{
				Property.Type.TypeReference = MakeReference(
					EAngelscriptCacheReferenceKind::ScriptType, 0xf0, 0xf1);
			}
			if (DataTypeKind == EAngelscriptCachedDataTypeKind::EnvironmentType)
			{
				Property.Type.TypeReference = MakeReference(
					EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xf2, 0xf3);
			}
			if (Property.Type.TypeReference.IsSet())
			{
				const EAngelscriptCacheSemanticDependencyKind DependencyKind =
					StorageKind == EAngelscriptCachedPropertyStorageKind::ObjectHandle
						? DataTypeKind == EAngelscriptCachedDataTypeKind::ScriptType
							? EAngelscriptCacheSemanticDependencyKind::Declaration
							: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi
						: EAngelscriptCacheSemanticDependencyKind::ValueLayout;
				Schema.Dependencies.AddUnique(MakeDependency(
					DependencyKind,
					Property.Type.TypeReference.GetValue()));
				Schema.Dependencies.Sort([](
					const FAngelscriptCacheSemanticDependency& A,
					const FAngelscriptCacheSemanticDependency& B)
				{
					return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
				});
			}
			if (StorageKind == EAngelscriptCachedPropertyStorageKind::InlineValue)
			{
				Property.SemanticStorageSize = 4;
				Property.SemanticStorageAlignment = 4;
			}
			if (StorageKind == EAngelscriptCachedPropertyStorageKind::ObjectHandle)
			{
				Property.SemanticStorageSize =
					RegionBObjectHandleLayout.SemanticStorageSize;
				Property.SemanticStorageAlignment =
					RegionBObjectHandleLayout.SemanticStorageAlignment;
			}
			Schema.Layout.SemanticAlignment = FMath::Max(
				uint32(8), Property.SemanticStorageAlignment);
			const uint64 PropertyEnd = uint64(Property.SemanticByteOffset)
				+ uint64(Property.SemanticStorageSize);
			Schema.Layout.SemanticSize = Align(
				PropertyEnd, uint64(Schema.Layout.SemanticAlignment));
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};

		FAngelscriptCachedTypeSchema RegionBBaselines[2][4];
		for (uint8 RawStorageKind = 1; RawStorageKind <= 2; ++RawStorageKind)
		{
			for (uint8 RawDataTypeKind = 1; RawDataTypeKind <= 4; ++RawDataTypeKind)
			{
				RegionBBaselines[RawStorageKind - 1][RawDataTypeKind - 1] =
					MakeRegionBFinalizedBaseline(
						static_cast<EAngelscriptCachedPropertyStorageKind>(RawStorageKind),
						static_cast<EAngelscriptCachedDataTypeKind>(RawDataTypeKind));
			}
		}
		const auto MakeRegionBCoordinate = [&](const EAngelscriptCachedPropertyStorageKind StorageKind,
			const EAngelscriptCachedDataTypeKind DataTypeKind, const uint32 QualifierMask)
		{
			FAngelscriptCachedTypeSchema Schema = RegionBBaselines[
				static_cast<int32>(StorageKind) - 1][static_cast<int32>(DataTypeKind) - 1];
			Schema.OrderedProperties[0].Type.QualifierFlags = QualifierMask;
			return Schema;
		};
		const auto ExpectRegionBSuccess = [&](FAngelscriptCachedTypeSchema Schema,
			const EAngelscriptCachedPropertyStorageKind StorageKind,
			const EAngelscriptCachedDataTypeKind DataTypeKind, const uint32 QualifierMask,
			const TCHAR* Context)
		{
			FinalizeValidFixtureHashes(Schema);
			MarkRegionBCoordinate(StorageKind, DataTypeKind, QualifierMask, Context);
			bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(
				*TestRunner, Schema, Context);
			++RegionBSuccessCalls;
			++RegionBCalls;
			++RegionBSuccessGroupCalls[static_cast<int32>(StorageKind) - 1]
				[static_cast<int32>(DataTypeKind) - 1];
		};
		const auto ExpectRegionBFailure = [&](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedPropertyStorageKind StorageKind,
			const EAngelscriptCachedDataTypeKind DataTypeKind, const uint32 QualifierMask,
			const TCHAR* Context)
		{
			MarkRegionBCoordinate(StorageKind, DataTypeKind, QualifierMask, Context);
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Schema,
				FError::InvalidQualifierCombination, Context);
			++RegionBNegativeCalls;
			++RegionBCalls;
			++RegionBFailureGroupCalls[static_cast<int32>(StorageKind) - 1]
				[static_cast<int32>(DataTypeKind) - 1];
		};

		struct FRegionBSuccessRow
		{
			EAngelscriptCachedPropertyStorageKind StorageKind;
			EAngelscriptCachedDataTypeKind DataTypeKind;
			uint32 QualifierMask;
			const TCHAR* Context;
		};
		const FRegionBSuccessRow RegionBSuccessRows[] = {
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::Primitive, 0x00,
				TEXT("Region B Inline Primitive mask 00 success")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x00,
				TEXT("Region B Inline ScriptType mask 00 success")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x02,
				TEXT("Region B Inline ScriptType mask 02 success")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x00,
				TEXT("Region B Inline EnvironmentType mask 00 success")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x02,
				TEXT("Region B Inline EnvironmentType mask 02 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x04,
				TEXT("Region B ObjectHandle ScriptType mask 04 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x06,
				TEXT("Region B ObjectHandle ScriptType mask 06 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x0c,
				TEXT("Region B ObjectHandle ScriptType mask 0c success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x0e,
				TEXT("Region B ObjectHandle ScriptType mask 0e success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x24,
				TEXT("Region B ObjectHandle ScriptType mask 24 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x26,
				TEXT("Region B ObjectHandle ScriptType mask 26 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x2c,
				TEXT("Region B ObjectHandle ScriptType mask 2c success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x2e,
				TEXT("Region B ObjectHandle ScriptType mask 2e success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x04,
				TEXT("Region B ObjectHandle EnvironmentType mask 04 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x06,
				TEXT("Region B ObjectHandle EnvironmentType mask 06 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x0c,
				TEXT("Region B ObjectHandle EnvironmentType mask 0c success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x0e,
				TEXT("Region B ObjectHandle EnvironmentType mask 0e success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x24,
				TEXT("Region B ObjectHandle EnvironmentType mask 24 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x26,
				TEXT("Region B ObjectHandle EnvironmentType mask 26 success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x2c,
				TEXT("Region B ObjectHandle EnvironmentType mask 2c success")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x2e,
				TEXT("Region B ObjectHandle EnvironmentType mask 2e success")},
		};
		for (const FRegionBSuccessRow& Row : RegionBSuccessRows)
		{
			ExpectRegionBSuccess(MakeRegionBCoordinate(
				Row.StorageKind, Row.DataTypeKind, Row.QualifierMask),
				Row.StorageKind, Row.DataTypeKind, Row.QualifierMask, Row.Context);
		}

		struct FRegionBFailurePartition
		{
			EAngelscriptCachedPropertyStorageKind StorageKind;
			EAngelscriptCachedDataTypeKind DataTypeKind;
			uint32 FixedSetMask;
			uint8 FreeBitPositions[6];
			uint8 FreeBitCount;
			int32 ExpectedCalls;
			const TCHAR* Context;
		};
		const FRegionBFailurePartition RegionBFailurePartitions[] = {
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::Primitive, 0x01, {1, 2, 3, 4, 5}, 5, 32,
				TEXT("Inline Primitive Reference-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::Primitive, 0x10, {1, 2, 3, 5}, 4, 16,
				TEXT("Inline Primitive Auto-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::Primitive, 0x04, {1, 3, 5}, 3, 8,
				TEXT("Inline Primitive ObjectHandle-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::Primitive, 0x08, {1, 5}, 2, 4,
				TEXT("Inline Primitive ConstHandle-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::Primitive, 0x20, {1}, 1, 2,
				TEXT("Inline Primitive IfHandleThenConst-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::Primitive, 0x02, {}, 0, 1,
				TEXT("Inline Primitive ObjectConst-only partition")},

			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x01, {1, 2, 3, 4, 5}, 5, 32,
				TEXT("Inline ScriptType Reference-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x10, {1, 2, 3, 5}, 4, 16,
				TEXT("Inline ScriptType Auto-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x04, {1, 3, 5}, 3, 8,
				TEXT("Inline ScriptType ObjectHandle-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x08, {1, 5}, 2, 4,
				TEXT("Inline ScriptType ConstHandle-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x20, {1}, 1, 2,
				TEXT("Inline ScriptType IfHandleThenConst-set partition")},

			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x01, {1, 2, 3, 4, 5}, 5, 32,
				TEXT("Inline EnvironmentType Reference-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x10, {1, 2, 3, 5}, 4, 16,
				TEXT("Inline EnvironmentType Auto-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x04, {1, 3, 5}, 3, 8,
				TEXT("Inline EnvironmentType ObjectHandle-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x08, {1, 5}, 2, 4,
				TEXT("Inline EnvironmentType ConstHandle-set partition")},
			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x20, {1}, 1, 2,
				TEXT("Inline EnvironmentType IfHandleThenConst-set partition")},

			{EAngelscriptCachedPropertyStorageKind::InlineValue,
				EAngelscriptCachedDataTypeKind::Auto, 0x00, {0, 1, 2, 3, 4, 5}, 6, 64,
				TEXT("Inline Auto all-mask partition")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::Primitive, 0x00, {0, 1, 2, 3, 4, 5}, 6, 64,
				TEXT("ObjectHandle Primitive all-mask partition")},

			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x01, {1, 2, 3, 4, 5}, 5, 32,
				TEXT("ObjectHandle ScriptType Reference-set partition")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x10, {1, 2, 3, 5}, 4, 16,
				TEXT("ObjectHandle ScriptType Auto-set partition")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::ScriptType, 0x00, {1, 3, 5}, 3, 8,
				TEXT("ObjectHandle ScriptType ObjectHandle-clear partition")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x01, {1, 2, 3, 4, 5}, 5, 32,
				TEXT("ObjectHandle EnvironmentType Reference-set partition")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x10, {1, 2, 3, 5}, 4, 16,
				TEXT("ObjectHandle EnvironmentType Auto-set partition")},
			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::EnvironmentType, 0x00, {1, 3, 5}, 3, 8,
				TEXT("ObjectHandle EnvironmentType ObjectHandle-clear partition")},

			{EAngelscriptCachedPropertyStorageKind::ObjectHandle,
				EAngelscriptCachedDataTypeKind::Auto, 0x00, {0, 1, 2, 3, 4, 5}, 6, 64,
				TEXT("ObjectHandle Auto all-mask partition")},
		};
		for (const FRegionBFailurePartition& Partition : RegionBFailurePartitions)
		{
			int32 PartitionCalls = 0;
			const uint32 DenseCount = uint32(1) << Partition.FreeBitCount;
			for (uint32 DenseMask = 0; DenseMask < DenseCount; ++DenseMask)
			{
				uint32 QualifierMask = Partition.FixedSetMask;
				for (uint8 FreeBitIndex = 0;
					FreeBitIndex < Partition.FreeBitCount; ++FreeBitIndex)
				{
					if ((DenseMask & (uint32(1) << FreeBitIndex)) != 0)
					{
						QualifierMask |= uint32(1)
							<< Partition.FreeBitPositions[FreeBitIndex];
					}
				}
				const FString Context = FString::Printf(TEXT("Region B %s mask %02x"),
					Partition.Context, QualifierMask);
				ExpectRegionBFailure(MakeRegionBCoordinate(Partition.StorageKind,
					Partition.DataTypeKind, QualifierMask), Partition.StorageKind,
					Partition.DataTypeKind, QualifierMask, *Context);
				++PartitionCalls;
			}
			bPassed &= LocalAssert.AreEqual(Partition.ExpectedCalls, PartitionCalls,
				*FString::Printf(TEXT("%s exact fixed partition count"), Partition.Context));
		}

		int32 RegionBLedgerSeenCalls = 0;
		int32 RegionBLedgerMissingCalls = 0;
		for (int32 StorageIndex = 0; StorageIndex < 2; ++StorageIndex)
		{
			for (int32 DataTypeIndex = 0; DataTypeIndex < 4; ++DataTypeIndex)
			{
				for (int32 QualifierMask = 0; QualifierMask < 64; ++QualifierMask)
				{
					if (RegionBSeen[StorageIndex][DataTypeIndex][QualifierMask])
					{
						++RegionBLedgerSeenCalls;
					}
					else
					{
						++RegionBLedgerMissingCalls;
					}
				}
			}
		}

		const int32 ExpectedRegionBSuccessGroupCalls[2][4] = {
			{1, 2, 2, 0},
			{0, 8, 8, 0},
		};
		const int32 ExpectedRegionBFailureGroupCalls[2][4] = {
			{63, 62, 62, 64},
			{64, 56, 56, 64},
		};
		for (int32 StorageIndex = 0; StorageIndex < 2; ++StorageIndex)
		{
			for (int32 DataTypeIndex = 0; DataTypeIndex < 4; ++DataTypeIndex)
			{
				bPassed &= LocalAssert.AreEqual(
					ExpectedRegionBSuccessGroupCalls[StorageIndex][DataTypeIndex],
					RegionBSuccessGroupCalls[StorageIndex][DataTypeIndex],
					*FString::Printf(TEXT("Region B success group [%d][%d]"),
						StorageIndex, DataTypeIndex));
				bPassed &= LocalAssert.AreEqual(
					ExpectedRegionBFailureGroupCalls[StorageIndex][DataTypeIndex],
					RegionBFailureGroupCalls[StorageIndex][DataTypeIndex],
					*FString::Printf(TEXT("Region B failure group [%d][%d]"),
						StorageIndex, DataTypeIndex));
			}
		}

		bPassed &= LocalAssert.AreEqual(21, RegionBSuccessCalls,
			TEXT("Region B success normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(491, RegionBNegativeCalls,
			TEXT("Region B negative normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(512, RegionBCalls,
			TEXT("Region B total normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(512,
			RegionBSuccessCalls + RegionBNegativeCalls,
			TEXT("Region B independent success plus negative call count"));
		bPassed &= LocalAssert.AreEqual(512, RegionBLedgerMarkedCalls,
			TEXT("Region B ledger marked coordinate count"));
		bPassed &= LocalAssert.AreEqual(512, RegionBLedgerSeenCalls,
			TEXT("Region B ledger seen coordinate count"));
		bPassed &= LocalAssert.AreEqual(0, RegionBLedgerDuplicateCalls,
			TEXT("Region B ledger duplicate coordinate count"));
		bPassed &= LocalAssert.AreEqual(0, RegionBLedgerMissingCalls,
			TEXT("Region B ledger missing coordinate count"));
		bPassed &= LocalAssert.AreEqual(28,
			RegionASuccessCalls + RegionBSuccessCalls,
			TEXT("Slice 4 checkpoint B success normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(531,
			RegionANegativeCalls + RegionBNegativeCalls,
			TEXT("Slice 4 checkpoint B negative normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(559, RegionACalls + RegionBCalls,
			TEXT("Slice 4 checkpoint B grand total normal-producer call count"));

		int32 RegionCSuccessCalls = 0;
		int32 RegionCNegativeCalls = 0;
		int32 RegionCCalls = 0;
		const auto ExpectRegionCSuccess = [&](const FAngelscriptCachedTypeSchema& Schema,
			const TCHAR* Context)
		{
			bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(
				*TestRunner, Schema, Context);
			++RegionCSuccessCalls;
			++RegionCCalls;
		};
		const auto ExpectRegionCFailure = [&](const FAngelscriptCachedTypeSchema& Schema,
			const FError ExpectedError, const TCHAR* Context)
		{
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(
				*TestRunner, Schema, ExpectedError, Context);
			++RegionCNegativeCalls;
			++RegionCCalls;
		};

		const uint32 RegionCOrdinaryUClassSuccessMasks[] = {
			0x00001, 0x00003, 0x00007, 0x00009, 0x00011, 0x00021,
			0x00041, 0x00081, 0x00101, 0x00201, 0x00401, 0x01001,
			0x02001, 0x04401, 0x08001, 0x10001, 0x20001, 0x40001,
		};
		for (const uint32 PropertyFlags : RegionCOrdinaryUClassSuccessMasks)
		{
			FAngelscriptCachedTypeSchema Schema = OrdinaryRoot;
			Schema.OrderedProperties[0].PropertySemanticFlags = PropertyFlags;
			if (PropertyFlags == 0x04401)
			{
				Schema.OrderedProperties[0].Metadata.Add(
					{TEXT("ReplicatedUsing"), TEXT("OnRep_Value")});
			}
			FinalizeValidFixtureHashes(Schema);
			const FString Context = FString::Printf(
				TEXT("Region C ordinary UClass property flags 0x%05x success"),
				PropertyFlags);
			ExpectRegionCSuccess(Schema, *Context);
		}

		for (uint8 RawCondition = 1; RawCondition <= 15; ++RawCondition)
		{
			FAngelscriptCachedTypeSchema Schema = OrdinaryRoot;
			Schema.OrderedProperties[0].PropertySemanticFlags = 0x00401;
			Schema.OrderedProperties[0].ReplicationCondition =
				static_cast<EAngelscriptCachedReplicationCondition>(RawCondition);
			FinalizeValidFixtureHashes(Schema);
			const FString Context = FString::Printf(
				TEXT("Region C ordinary UClass replicated condition %u success"),
				RawCondition);
			ExpectRegionCSuccess(Schema, *Context);
		}

		const uint32 RegionCUStructSuccessMasks[] = {
			0x00001, 0x00003, 0x00007, 0x00009, 0x00011, 0x00021,
			0x00041, 0x00081, 0x00101, 0x00201, 0x00801, 0x01001,
			0x02001, 0x10001, 0x20001, 0x40001,
		};
		for (const uint32 PropertyFlags : RegionCUStructSuccessMasks)
		{
			FAngelscriptCachedTypeSchema Schema = ReflectedUStruct;
			Schema.OrderedProperties[0].PropertySemanticFlags = PropertyFlags;
			FinalizeValidFixtureHashes(Schema);
			const FString Context = FString::Printf(
				TEXT("Region C UStruct property flags 0x%05x success"),
				PropertyFlags);
			ExpectRegionCSuccess(Schema, *Context);
		}

		FAngelscriptCachedTypeSchema RegionCMetadataWithoutRepNotify = OrdinaryRoot;
		RegionCMetadataWithoutRepNotify.OrderedProperties[0].PropertySemanticFlags =
			0x00001;
		RegionCMetadataWithoutRepNotify.OrderedProperties[0].Metadata.Add(
			{TEXT("ReplicatedUsing"), TEXT("OnRep_OrdinaryMetadata")});
		FinalizeValidFixtureHashes(RegionCMetadataWithoutRepNotify);
		ExpectRegionCSuccess(RegionCMetadataWithoutRepNotify,
			TEXT("Region C ReplicatedUsing metadata without RepNotify success"));

		for (const uint32 UnknownPropertyFlags : {uint32(0x80000), uint32(0xffffffff)})
		{
			FAngelscriptCachedTypeSchema UnknownFlags = OrdinaryRoot;
			UnknownFlags.OrderedProperties[0].PropertySemanticFlags =
				UnknownPropertyFlags;
			const FString Context = FString::Printf(
				TEXT("Region C property flags unknown mask 0x%08x"),
				UnknownPropertyFlags);
			ExpectRegionCFailure(UnknownFlags, FError::UnknownFlags, *Context);
		}

		FAngelscriptCachedTypeSchema RegionCInvalidFlags = OrdinaryRoot;
		RegionCInvalidFlags.OrderedProperties[0].PropertySemanticFlags = 0x00005;
		ExpectRegionCFailure(RegionCInvalidFlags,
			FError::InvalidQualifierCombination,
			TEXT("Region C BlueprintWritable without BlueprintReadable"));

		RegionCInvalidFlags = OrdinaryRoot;
		RegionCInvalidFlags.OrderedProperties[0].PropertySemanticFlags = 0x04001;
		RegionCInvalidFlags.OrderedProperties[0].Metadata.Add(
			{TEXT("ReplicatedUsing"), TEXT("OnRep_Value")});
		ExpectRegionCFailure(RegionCInvalidFlags,
			FError::InvalidQualifierCombination,
			TEXT("Region C RepNotify without Replicated"));

		RegionCInvalidFlags = OrdinaryRoot;
		RegionCInvalidFlags.OrderedProperties[0].PropertySemanticFlags = 0x04401;
		ExpectRegionCFailure(RegionCInvalidFlags,
			FError::InvalidQualifierCombination,
			TEXT("Region C RepNotify missing ReplicatedUsing metadata"));

		RegionCInvalidFlags = OrdinaryRoot;
		RegionCInvalidFlags.OrderedProperties[0].PropertySemanticFlags = 0x04401;
		RegionCInvalidFlags.OrderedProperties[0].Metadata.Add(
			{TEXT("ReplicatedUsing"), TEXT("")});
		ExpectRegionCFailure(RegionCInvalidFlags,
			FError::InvalidQualifierCombination,
			TEXT("Region C RepNotify has empty ReplicatedUsing metadata"));

		RegionCInvalidFlags = OrdinaryRoot;
		RegionCInvalidFlags.OrderedProperties[0].PropertySemanticFlags = 0x00001;
		RegionCInvalidFlags.OrderedProperties[0].ReplicationCondition =
			EAngelscriptCachedReplicationCondition::InitialOnly;
		ExpectRegionCFailure(RegionCInvalidFlags,
			FError::InvalidQualifierCombination,
			TEXT("Region C nonreplicated property has InitialOnly condition"));

		RegionCInvalidFlags = OrdinaryRoot;
		RegionCInvalidFlags.OrderedProperties[0].PropertySemanticFlags = 0x00c01;
		ExpectRegionCFailure(RegionCInvalidFlags,
			FError::InvalidQualifierCombination,
			TEXT("Region C SkipReplication conflicts with Replicated"));

		RegionCInvalidFlags = OrdinaryRoot;
		RegionCInvalidFlags.OrderedProperties[0].PropertySemanticFlags = 0x04c01;
		RegionCInvalidFlags.OrderedProperties[0].Metadata.Add(
			{TEXT("ReplicatedUsing"), TEXT("OnRep_Value")});
		ExpectRegionCFailure(RegionCInvalidFlags,
			FError::InvalidQualifierCombination,
			TEXT("Region C SkipReplication conflicts with RepNotify"));

		RegionCInvalidFlags = OrdinaryRoot;
		RegionCInvalidFlags.OrderedProperties[0].PropertySemanticFlags = 0x00c01;
		RegionCInvalidFlags.OrderedProperties[0].ReplicationCondition =
			EAngelscriptCachedReplicationCondition::InitialOnly;
		ExpectRegionCFailure(RegionCInvalidFlags,
			FError::InvalidQualifierCombination,
			TEXT("Region C SkipReplication conflicts with InitialOnly condition"));

		for (const uint8 RawCondition : {uint8(17), uint8(255)})
		{
			FAngelscriptCachedTypeSchema UnknownCondition = OrdinaryRoot;
			UnknownCondition.OrderedProperties[0].PropertySemanticFlags = 0x00401;
			UnknownCondition.OrderedProperties[0].ReplicationCondition =
				static_cast<EAngelscriptCachedReplicationCondition>(RawCondition);
			const FString Context = FString::Printf(
				TEXT("Region C property replication condition raw %u"), RawCondition);
			ExpectRegionCFailure(UnknownCondition,
				FError::UnknownEnumValue, *Context);
		}

		FAngelscriptCachedTypeSchema RegionCOwnerForbidden = OrdinaryRoot;
		RegionCOwnerForbidden.OrderedProperties[0].PropertySemanticFlags = 0x00801;
		FinalizeValidFixtureHashes(RegionCOwnerForbidden);
		ExpectRegionCFailure(RegionCOwnerForbidden, FError::InvalidPresence,
			TEXT("Region C ordinary UClass forbids SkipReplication"));

		RegionCOwnerForbidden = ReflectedUStruct;
		RegionCOwnerForbidden.OrderedProperties[0].PropertySemanticFlags = 0x00401;
		FinalizeValidFixtureHashes(RegionCOwnerForbidden);
		ExpectRegionCFailure(RegionCOwnerForbidden, FError::InvalidPresence,
			TEXT("Region C UStruct forbids Replicated"));

		RegionCOwnerForbidden = ReflectedUStruct;
		RegionCOwnerForbidden.OrderedProperties[0].PropertySemanticFlags = 0x04401;
		RegionCOwnerForbidden.OrderedProperties[0].Metadata.Add(
			{TEXT("ReplicatedUsing"), TEXT("OnRep_Value")});
		FinalizeValidFixtureHashes(RegionCOwnerForbidden);
		ExpectRegionCFailure(RegionCOwnerForbidden, FError::InvalidPresence,
			TEXT("Region C UStruct forbids RepNotify"));

		RegionCOwnerForbidden = ReflectedUStruct;
		RegionCOwnerForbidden.OrderedProperties[0].PropertySemanticFlags = 0x08001;
		FinalizeValidFixtureHashes(RegionCOwnerForbidden);
		ExpectRegionCFailure(RegionCOwnerForbidden, FError::InvalidPresence,
			TEXT("Region C UStruct forbids Config"));

		struct FRegionCPlainOwnerFailureRow
		{
			EPropertyOwnerFormForTests Owner;
			const TCHAR* Context;
		};
		const FRegionCPlainOwnerFailureRow RegionCPlainOwnerFailureRows[] = {
			{EPropertyOwnerFormForTests::PlainClass,
				TEXT("Region C plain Class forbids HasUnrealProperty")},
			{EPropertyOwnerFormForTests::PlainStruct,
				TEXT("Region C plain Struct forbids HasUnrealProperty")},
			{EPropertyOwnerFormForTests::Delegate,
				TEXT("Region C Delegate forbids HasUnrealProperty")},
		};
		for (const FRegionCPlainOwnerFailureRow& Row : RegionCPlainOwnerFailureRows)
		{
			RegionCOwnerForbidden = MakePropertyOwnerFixture(Row.Owner);
			RegionCOwnerForbidden.OrderedProperties[0].PropertySemanticFlags = 0x00001;
			FinalizeValidFixtureHashes(RegionCOwnerForbidden);
			ExpectRegionCFailure(RegionCOwnerForbidden,
				FError::InvalidPresence, Row.Context);
		}

		bPassed &= LocalAssert.AreEqual(50, RegionCSuccessCalls,
			TEXT("Region C success normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(19, RegionCNegativeCalls,
			TEXT("Region C negative normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(69, RegionCCalls,
			TEXT("Region C total normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(69,
			RegionCSuccessCalls + RegionCNegativeCalls,
			TEXT("Region C independent success plus negative call count"));
		bPassed &= LocalAssert.AreEqual(78,
			RegionASuccessCalls + RegionBSuccessCalls + RegionCSuccessCalls,
			TEXT("Slice 4 checkpoint C success normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(550,
			RegionANegativeCalls + RegionBNegativeCalls + RegionCNegativeCalls,
			TEXT("Slice 4 checkpoint C negative normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(628,
			RegionACalls + RegionBCalls + RegionCCalls,
			TEXT("Slice 4 checkpoint C grand total normal-producer call count"));

		int32 RegionDSuccessCalls = 0;
		int32 RegionDNegativeCalls = 0;
		int32 RegionDCalls = 0;
		int32 RegionDInvalidQualifierCalls = 0;
		int32 RegionDOverflowCalls = 0;
		int32 RegionDAggregateAlignmentCalls = 0;
		int32 RegionDLayoutBoundaryCalls = 0;
		int32 RegionDCursorTailCalls = 0;
		int32 RegionDStorageScalarCalls = 0;
		int32 RegionDCheckedArithmeticCalls = 0;

		const auto ExpectRegionDSuccess = [&](const FAngelscriptCachedTypeSchema& Schema,
			const TCHAR* Context)
		{
			bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(
				*TestRunner, Schema, Context);
			++RegionDSuccessCalls;
			++RegionDCalls;
		};
		const auto ExpectRegionDInvalidQualifier = [&](
			const FAngelscriptCachedTypeSchema& Schema, int32& FamilyCalls,
			const TCHAR* Context)
		{
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(
				*TestRunner, Schema, FError::InvalidQualifierCombination, Context);
			++FamilyCalls;
			++RegionDInvalidQualifierCalls;
			++RegionDNegativeCalls;
			++RegionDCalls;
		};
		const auto ExpectRegionDOverflow = [&](const FAngelscriptCachedTypeSchema& Schema,
			int32& FamilyCalls, const TCHAR* Context)
		{
			bPassed &= ExpectExactProducerFailureAndInputUnchanged(
				*TestRunner, Schema, FError::Overflow, Context);
			++FamilyCalls;
			++RegionDOverflowCalls;
			++RegionDNegativeCalls;
			++RegionDCalls;
		};
		const auto SetRegionDLayoutInput = [&](FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedTypeLayoutInputKind InputKind,
			const uint32 BoundaryContribution, const uint32 AlignmentContribution,
			const TCHAR* Context)
		{
			int32 MatchedInputs = 0;
			for (FAngelscriptCachedTypeLayoutInput& Input : Schema.LayoutInputs)
			{
				if (Input.InputKind == InputKind)
				{
					Input.BoundaryContribution = BoundaryContribution;
					Input.AlignmentContribution = AlignmentContribution;
					++MatchedInputs;
				}
			}
			bPassed &= LocalAssert.AreEqual(1, MatchedInputs, Context);
		};

		const FAngelscriptCachedTypeSchema RegionDThreeProperty =
			MakeThreePropertyLayoutSchema();
		ExpectRegionDSuccess(RegionDThreeProperty,
			TEXT("Region D exact three-property padding and tail layout success"));

		FAngelscriptCachedTypeSchema RegionDPropertyAlignment16 =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDPropertyAlignment16.OrderedProperties[0].SemanticStorageAlignment = 16;
		RegionDPropertyAlignment16.Layout.SemanticSize = 16;
		RegionDPropertyAlignment16.Layout.SemanticAlignment = 16;
		FinalizeValidFixtureHashes(RegionDPropertyAlignment16);
		ExpectRegionDSuccess(RegionDPropertyAlignment16,
			TEXT("Region D property raises exact aggregate alignment to 16 success"));

		FAngelscriptCachedTypeSchema RegionDBaseContribution16 =
			MakeNonZeroBaseBoundaryUClassSchema();
		SetRegionDLayoutInput(RegionDBaseContribution16,
			EAngelscriptCachedTypeLayoutInputKind::BaseType, 16, 16,
			TEXT("Region D exact BaseType input singleton"));
		RegionDBaseContribution16.Layout.SemanticSize = 16;
		RegionDBaseContribution16.Layout.SemanticAlignment = 16;
		RegionDBaseContribution16.Layout.BasePropertyBoundary = 16;
		FinalizeValidFixtureHashes(RegionDBaseContribution16);
		ExpectRegionDSuccess(RegionDBaseContribution16,
			TEXT("Region D BaseType boundary and maximum alignment 16 success"));

		FAngelscriptCachedTypeSchema RegionDCodeRootContribution16 =
			MakeOrdinaryUClassSchema(false);
		SetRegionDLayoutInput(RegionDCodeRootContribution16,
			EAngelscriptCachedTypeLayoutInputKind::CodeRoot, 16, 16,
			TEXT("Region D exact CodeRoot input singleton"));
		RegionDCodeRootContribution16.Layout.SemanticSize = 16;
		RegionDCodeRootContribution16.Layout.SemanticAlignment = 16;
		RegionDCodeRootContribution16.Layout.BasePropertyBoundary = 16;
		FinalizeValidFixtureHashes(RegionDCodeRootContribution16);
		ExpectRegionDSuccess(RegionDCodeRootContribution16,
			TEXT("Region D CodeRoot boundary and shadow alignment 16 success"));

		const FAngelscriptCachedTypeSchema RegionDStructHeaderBoundary =
			MakeReflectedUStructSchema();
		ExpectRegionDSuccess(RegionDStructHeaderBoundary,
			TEXT("Region D UStruct StructHeader boundary-only layout 16 8 16 success"));

		FAngelscriptCachedTypeSchema RegionDInvalidLayout = RegionDThreeProperty;
		RegionDInvalidLayout.Layout.SemanticAlignment = 0;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDAggregateAlignmentCalls,
			TEXT("Region D aggregate alignment zero"));

		RegionDInvalidLayout = RegionDThreeProperty;
		RegionDInvalidLayout.Layout.SemanticAlignment = 4;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDAggregateAlignmentCalls,
			TEXT("Region D aggregate alignment below exact initial 8"));

		RegionDInvalidLayout = RegionDThreeProperty;
		RegionDInvalidLayout.Layout.SemanticAlignment = 16;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDAggregateAlignmentCalls,
			TEXT("Region D aggregate alignment surplus above exact maximum 8"));

		RegionDInvalidLayout = RegionDPropertyAlignment16;
		RegionDInvalidLayout.Layout.SemanticAlignment = 8;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDAggregateAlignmentCalls,
			TEXT("Region D aggregate alignment below property contribution 16"));

		RegionDInvalidLayout = RegionDBaseContribution16;
		RegionDInvalidLayout.Layout.SemanticAlignment = 8;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDAggregateAlignmentCalls,
			TEXT("Region D aggregate alignment below BaseType contribution 16"));

		RegionDInvalidLayout = RegionDCodeRootContribution16;
		RegionDInvalidLayout.Layout.SemanticAlignment = 8;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDAggregateAlignmentCalls,
			TEXT("Region D aggregate alignment below CodeRoot contribution 16"));

		RegionDInvalidLayout = RegionDBaseContribution16;
		RegionDInvalidLayout.Layout.SemanticSize = 8;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDLayoutBoundaryCalls,
			TEXT("Region D BasePropertyBoundary 16 above SemanticSize 8"));

		RegionDInvalidLayout = RegionDThreeProperty;
		RegionDInvalidLayout.Layout.SemanticSize = uint64(MAX_int32) + 1;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDOverflow(RegionDInvalidLayout, RegionDLayoutBoundaryCalls,
			TEXT("Region D layout SemanticSize above INT32_MAX"));

		RegionDInvalidLayout = RegionDThreeProperty;
		RegionDInvalidLayout.Layout.SemanticAlignment = uint32(MAX_int32) + 1u;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDOverflow(RegionDInvalidLayout, RegionDLayoutBoundaryCalls,
			TEXT("Region D layout SemanticAlignment above INT32_MAX"));

		RegionDInvalidLayout = RegionDBaseContribution16;
		RegionDInvalidLayout.Layout.BasePropertyBoundary = uint32(MAX_int32) + 1u;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDOverflow(RegionDInvalidLayout, RegionDLayoutBoundaryCalls,
			TEXT("Region D layout BasePropertyBoundary above INT32_MAX"));

		RegionDInvalidLayout = RegionDBaseContribution16;
		RegionDInvalidLayout.Layout.BasePropertyBoundary = 8;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDLayoutBoundaryCalls,
			TEXT("Region D layout boundary disagrees with BaseType contribution 16"));

		RegionDInvalidLayout = RegionDCodeRootContribution16;
		RegionDInvalidLayout.Layout.BasePropertyBoundary = 8;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDLayoutBoundaryCalls,
			TEXT("Region D layout boundary disagrees with CodeRoot contribution 16"));

		RegionDInvalidLayout = RegionDStructHeaderBoundary;
		RegionDInvalidLayout.Layout.BasePropertyBoundary = 8;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDLayoutBoundaryCalls,
			TEXT("Region D layout boundary disagrees with StructHeader contribution 16"));

		const FAngelscriptCachedTypeSchema RegionDNonZeroBoundaryProperty =
			MakeOwnerWithProperty(MakeNonZeroBaseBoundaryUClassSchema());
		RegionDInvalidLayout = RegionDNonZeroBoundaryProperty;
		RegionDInvalidLayout.OrderedProperties[0].SemanticByteOffset = 0;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDCursorTailCalls,
			TEXT("Region D property offset zero below inherited boundary 16"));

		RegionDInvalidLayout = RegionDThreeProperty;
		RegionDInvalidLayout.OrderedProperties[1].SemanticByteOffset = 0;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDCursorTailCalls,
			TEXT("Region D middle property overlaps predecessor"));

		RegionDInvalidLayout = RegionDThreeProperty;
		RegionDInvalidLayout.OrderedProperties[2].SemanticByteOffset = 12;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		bPassed &= LocalAssert.IsFalse(
			RegionDThreeProperty.OrderedProperties[2].PropertyLayoutFingerprint
				== RegionDInvalidLayout.OrderedProperties[2].PropertyLayoutFingerprint,
			TEXT("Region D legal and aligned-wrong final property fingerprints differ"));
		bPassed &= LocalAssert.IsFalse(
			RegionDThreeProperty.Layout.TypeLayoutHash
				== RegionDInvalidLayout.Layout.TypeLayoutHash,
			TEXT("Region D legal and aligned-wrong type layout hashes differ"));
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDCursorTailCalls,
			TEXT("Region D aligned final offset 12 differs from replay cursor 8 with terminal size 16"));

		RegionDInvalidLayout = RegionDThreeProperty;
		RegionDInvalidLayout.Layout.SemanticSize = 10;
		FinalizeValidFixtureHashes(RegionDInvalidLayout);
		ExpectRegionDInvalidQualifier(RegionDInvalidLayout,
			RegionDCursorTailCalls,
			TEXT("Region D terminal size 10 omits mandatory tail padding to 16"));

		FAngelscriptCachedTypeSchema RegionDInvalidStorage =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDInvalidStorage.OrderedProperties[0].SemanticStorageSize = 0;
		FinalizeValidFixtureHashes(RegionDInvalidStorage);
		ExpectRegionDInvalidQualifier(RegionDInvalidStorage,
			RegionDStorageScalarCalls,
			TEXT("Region D property storage size zero"));

		RegionDInvalidStorage =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDInvalidStorage.OrderedProperties[0].SemanticStorageAlignment = 0;
		FinalizeValidFixtureHashes(RegionDInvalidStorage);
		ExpectRegionDInvalidQualifier(RegionDInvalidStorage,
			RegionDStorageScalarCalls,
			TEXT("Region D property storage alignment zero"));

		RegionDInvalidStorage =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDInvalidStorage.OrderedProperties[0].SemanticStorageAlignment = 3;
		FinalizeValidFixtureHashes(RegionDInvalidStorage);
		ExpectRegionDInvalidQualifier(RegionDInvalidStorage,
			RegionDStorageScalarCalls,
			TEXT("Region D property storage alignment three is not a power of two"));

		RegionDInvalidStorage =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDInvalidStorage.OrderedProperties[0].SemanticStorageSize =
			uint32(MAX_int32) + 1u;
		FinalizeValidFixtureHashes(RegionDInvalidStorage);
		ExpectRegionDOverflow(RegionDInvalidStorage, RegionDStorageScalarCalls,
			TEXT("Region D property storage size above INT32_MAX"));

		RegionDInvalidStorage =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDInvalidStorage.OrderedProperties[0].SemanticStorageAlignment =
			uint32(MAX_int32) + 1u;
		FinalizeValidFixtureHashes(RegionDInvalidStorage);
		ExpectRegionDOverflow(RegionDInvalidStorage, RegionDStorageScalarCalls,
			TEXT("Region D property storage alignment above INT32_MAX"));

		FAngelscriptCachedTypeSchema RegionDInvalidArithmetic =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticByteOffset =
			uint32(MAX_int32) + 1u;
		FinalizeValidFixtureHashes(RegionDInvalidArithmetic);
		ExpectRegionDOverflow(RegionDInvalidArithmetic,
			RegionDCheckedArithmeticCalls,
			TEXT("Region D property byte offset above INT32_MAX"));

		RegionDInvalidArithmetic =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticByteOffset =
			MAX_uint32 - 1;
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticStorageSize = 4;
		FinalizeValidFixtureHashes(RegionDInvalidArithmetic);
		ExpectRegionDOverflow(RegionDInvalidArithmetic,
			RegionDCheckedArithmeticCalls,
			TEXT("Region D checked uint32 property end wrap"));

		RegionDInvalidArithmetic =
			MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticByteOffset =
			uint32(MAX_int32) - 3u;
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticStorageSize = 4;
		FinalizeValidFixtureHashes(RegionDInvalidArithmetic);
		ExpectRegionDOverflow(RegionDInvalidArithmetic,
			RegionDCheckedArithmeticCalls,
			TEXT("Region D property end crosses INT32_MAX"));

		RegionDInvalidArithmetic = RegionDThreeProperty;
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticStorageSize = MAX_int32;
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticStorageAlignment = 1;
		RegionDInvalidArithmetic.OrderedProperties[1].SemanticByteOffset = MAX_int32;
		RegionDInvalidArithmetic.Layout.SemanticSize = MAX_int32;
		FinalizeValidFixtureHashes(RegionDInvalidArithmetic);
		ExpectRegionDOverflow(RegionDInvalidArithmetic,
			RegionDCheckedArithmeticCalls,
			TEXT("Region D pre-property AlignUp crosses INT32_MAX"));

		RegionDInvalidArithmetic = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		RegionDInvalidArithmetic.OrderedProperties.Add(
			RegionDThreeProperty.OrderedProperties[0]);
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticByteOffset = 0;
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticStorageSize = MAX_int32;
		RegionDInvalidArithmetic.OrderedProperties[0].SemanticStorageAlignment = 1;
		RegionDInvalidArithmetic.Layout.SemanticAlignment = 8;
		RegionDInvalidArithmetic.Layout.SemanticSize = MAX_int32;
		FinalizeValidFixtureHashes(RegionDInvalidArithmetic);
		ExpectRegionDOverflow(RegionDInvalidArithmetic,
			RegionDCheckedArithmeticCalls,
			TEXT("Region D terminal AlignUp crosses INT32_MAX"));

		bPassed &= LocalAssert.AreEqual(6, RegionDAggregateAlignmentCalls,
			TEXT("Region D aggregate-alignment failure call count"));
		bPassed &= LocalAssert.AreEqual(7, RegionDLayoutBoundaryCalls,
			TEXT("Region D layout-boundary failure call count"));
		bPassed &= LocalAssert.AreEqual(4, RegionDCursorTailCalls,
			TEXT("Region D cursor-tail failure call count"));
		bPassed &= LocalAssert.AreEqual(5, RegionDStorageScalarCalls,
			TEXT("Region D storage-scalar failure call count"));
		bPassed &= LocalAssert.AreEqual(5, RegionDCheckedArithmeticCalls,
			TEXT("Region D checked-arithmetic failure call count"));
		bPassed &= LocalAssert.AreEqual(17, RegionDInvalidQualifierCalls,
			TEXT("Region D InvalidQualifierCombination literal call count"));
		bPassed &= LocalAssert.AreEqual(10, RegionDOverflowCalls,
			TEXT("Region D Overflow literal call count"));
		bPassed &= LocalAssert.AreEqual(5, RegionDSuccessCalls,
			TEXT("Region D success normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(27, RegionDNegativeCalls,
			TEXT("Region D negative normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(32, RegionDCalls,
			TEXT("Region D total normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(32,
			RegionDSuccessCalls + RegionDNegativeCalls,
			TEXT("Region D independent success plus negative call count"));
		bPassed &= LocalAssert.AreEqual(83,
			RegionASuccessCalls + RegionBSuccessCalls + RegionCSuccessCalls
				+ RegionDSuccessCalls,
			TEXT("Slice 4 final success normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(577,
			RegionANegativeCalls + RegionBNegativeCalls + RegionCNegativeCalls
				+ RegionDNegativeCalls,
			TEXT("Slice 4 final negative normal-producer call count"));
		bPassed &= LocalAssert.AreEqual(660,
			RegionACalls + RegionBCalls + RegionCCalls + RegionDCalls,
			TEXT("Slice 4 final grand total normal-producer call count"));

		ASSERT_THAT(IsTrue(bPassed,
			TEXT("normal producer property Regions A, B, C, and D owner, datatype, storage, qualifier, flags, replication, and layout matrix")));
	}

	TEST_METHOD(NormalProducerRejectsReflectionFormsNamesAndMembersAtomically)
	{
		FAngelscriptCachedTypeSchema Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.LayoutInputs.Add(FAngelscriptCachedTypeLayoutInput(Invalid.LayoutInputs[0]));
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate LayoutInput singleton"))));

		Invalid = MakeOrdinaryUClassSchema(false);
		FAngelscriptCachedTypeLayoutInput ConflictingInput = Invalid.LayoutInputs[0];
		ConflictingInput.Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xd0, 0xd1);
		Invalid.LayoutInputs.Add(MoveTemp(ConflictingInput));
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("conflicting LayoutInput singleton"))));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface);
		for (uint32 Ordinal : {0u, 2u})
		{
			FAngelscriptCachedTypeRelation Relation;
			Relation.RelationKind =
				EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			Relation.SemanticOrdinal = Ordinal;
			Relation.Target = MakeReference(
				EAngelscriptCacheReferenceKind::ScriptType,
				static_cast<uint8>(0xd2 + Ordinal),
				static_cast<uint8>(0xe2 + Ordinal));
			Invalid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Inheritance,
				Relation.Target));
			Invalid.Relations.Add(MoveTemp(Relation));
		}
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("implemented-interface ordinal gap"))));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		FAngelscriptCachedTypeRelation ForbiddenBase;
		ForbiddenBase.RelationKind = EAngelscriptCachedTypeRelationKind::Base;
		ForbiddenBase.Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptType, 0xd4, 0xe4);
		Invalid.Relations.Add(ForbiddenBase);
		Invalid.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Inheritance,
			ForbiddenBase.Target));
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Struct forbids Base relation"))));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		Invalid.Layout.SemanticSize = 8;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("terminal layout size replay"))));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		Invalid.Layout.SemanticAlignment = 3;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("layout alignment is a power of two"))));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		Invalid.Layout.BasePropertyBoundary = 1;
		Invalid.Layout.SemanticSize = 8;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("layout boundary requires an authority"))));

		Invalid = MakeThreePropertyLayoutSchema();
		Invalid.OrderedProperties[1].SemanticByteOffset = 2;
		ASSERT_THAT(IsTrue(
			RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
				Invalid).IsSuccess()));
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("property byte offset replay"))));

		Invalid = MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		Invalid.OrderedProperties[0].StorageKind =
			static_cast<EAngelscriptCachedPropertyStorageKind>(3);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::UnknownEnumValue,
			TEXT("property storage kind"))));

		Invalid = MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		Invalid.OrderedProperties[0].Access =
			static_cast<EAngelscriptCachedMemberAccess>(4);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::UnknownEnumValue,
			TEXT("property access"))));

		Invalid = MakePropertyOwnerFixture(EPropertyOwnerFormForTests::OrdinaryUClass);
		Invalid.OrderedProperties[0].PropertySemanticFlags = static_cast<uint32>(
			EAngelscriptCachedPropertySemanticFlags::BlueprintReadable);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("property flags require HasUnrealProperty"))));

		Invalid = MakePropertyOwnerFixture(EPropertyOwnerFormForTests::OrdinaryUClass);
		Invalid.OrderedProperties[0].PropertySemanticFlags =
			static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::HasUnrealProperty)
			| static_cast<uint32>(EAngelscriptCachedPropertySemanticFlags::Replicated);
		Invalid.OrderedProperties[0].ReplicationCondition =
			EAngelscriptCachedReplicationCondition::NetGroup;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("NetGroup replication is forbidden in V1"))));

		Invalid = MakeThreePropertyLayoutSchema();
		Invalid.OrderedProperties[0].LayoutOrdinal = 1;
		ASSERT_THAT(IsTrue(
			RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
				Invalid).IsSuccess()));
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("property ordinal gap"))));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.OrderedMethods[0].EntryKind =
			EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("method entry kind"))));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.OrderedMethods[0].MethodOrdinal = 1;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("method ordinal gap"))));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.OrderedMethods[0].DeclaringOwner =
			FAngelscriptStableTypeKey{MakeHash(0xd5)};
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("local method owner"))));

		const auto MakeVftProducerSchema = [&]()
		{
			FAngelscriptCachedTypeSchema Schema =
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
			FAngelscriptCachedVirtualFunctionSlot Slot;
			Slot.SlotKind = EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
			Slot.VftOrdinal = 0;
			Slot.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(0xd6)};
			Slot.DeclaringOwner = Schema.TypeKey;
			Slot.ImplementingOwner = Schema.TypeKey;
			Slot.ExpectedDeclarationAbi = MakeHash(0xe6);
			Schema.VirtualFunctionTable.Add(Slot);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xd6, 0xe6)));
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};

		Invalid = MakeVftProducerSchema();
		Invalid.VirtualFunctionTable[0].SlotKind =
			EAngelscriptCachedMethodSlotKind::LocalMethod;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("VFT slot kind"))));

		Invalid = MakeVftProducerSchema();
		Invalid.VirtualFunctionTable[0].VftOrdinal = 1;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("VFT ordinal gap"))));

		Invalid = MakeVftProducerSchema();
		Invalid.VirtualFunctionTable[0].DeclaringOwner =
			FAngelscriptStableTypeKey{MakeHash(0xd7)};
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("VFT declaration owner"))));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.OrderedBehaviorSlots[0].SlotOrdinal = 1;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("behavior ordinal gap"))));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.OrderedBehaviorSlots[0].DeclaringOwner.Reset();
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("script behavior owner"))));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.OrderedBehaviorSlots[0].BehaviorKind =
			EAngelscriptCachedBehaviorKind::TemplateCallback;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("forbidden behavior kind"))));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		Invalid.TypeSemanticFlags |= 0x100u;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::UnknownFlags,
			TEXT("unknown TypeSemanticFlags bit"))));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		Invalid.TypeSemanticFlags = 0;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Class requires ReferenceType"))));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		Invalid.TypeSemanticFlags |=
			static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Abstract)
			| static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Final);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Abstract and Final are incompatible"))));

		Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.Reflection.ConfigName = FString();
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("present ConfigName is nonempty"))));

		Invalid = MakeStaticsUClassSchema(0);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("StaticsClass requires reflected members"))));

		Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.Reflection.OrderedUFunctionMembers[0].ReflectionOrdinal = 1;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("reflected UFunction ordinal gap"))));

		Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.Reflection.OrderedUFunctionMembers[0].Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xd8, 0xe8);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::WrongReferenceKind,
			TEXT("reflected UFunction target kind"))));

		Invalid = MakeEnumSchema();
		Invalid.KindPayload.Enum->OrderedEnumerators[1].DeclarationOrdinal = 2;
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("enum declaration ordinal gap"))));

		Invalid = MakeEnumSchema();
		Invalid.KindPayload.Enum->OrderedEnumerators[1].CanonicalName = TEXT("Idle");
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("duplicate enum name"))));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.Dependencies.RemoveAll([](const auto& Dependency)
		{
			return Dependency.Kind == EAngelscriptCacheSemanticDependencyKind::Signature;
		});
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::MissingCoverage,
			TEXT("missing derived dependency"))));

		Invalid = MakeCompleteDelegateSchema();
		Invalid.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::ValueLayout,
			MakeReference(EAngelscriptCacheReferenceKind::ScriptType, 0xd9, 0xe9)));
		ASSERT_THAT(IsTrue(ExpectExactProducerFailureAndInputUnchanged(*TestRunner,
			Invalid, EAngelscriptCacheValidationError::UnexpectedRecord,
			TEXT("extra derived dependency"))));
	}

	TEST_METHOD(NormalProducerUsesFrozenHashesAndNeverAcceptsResolvers)
	{
		using FExpectedSerializeSignature = FAngelscriptCacheValidationResult(*)(
			const FAngelscriptCachedTypeSchema&, TArray<uint8>&);
		static_assert(std::is_same_v<decltype(
			&FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema),
			FExpectedSerializeSignature>);

		FNoDiscardAsserter LocalAssert(*TestRunner);
		bool bPassed = true;
		const FAngelscriptCachedTypeSchema Delegate = MakeCompleteDelegateSchema();
		bPassed &= LocalAssert.AreEqual(FString(TEXT(
			"d5fde2db0fdce72701c7e7a331a2372147f8949b39850ff48869df21af371230")),
			Delegate.OrderedProperties[0].StorageLayoutHash.ToHexString(),
			TEXT("frozen StorageLayoutHash embedded in legal schema"));
		bPassed &= LocalAssert.AreEqual(FString(TEXT(
			"0899ec027b91760050ef10914713ea16ecf5fa5d13f54af8d876a50df8416af7")),
			Delegate.OrderedProperties[0].PropertyLayoutFingerprint.ToHexString(),
			TEXT("frozen PropertyLayoutFingerprint embedded in legal schema"));
		bPassed &= LocalAssert.AreEqual(FString(TEXT(
			"31930115c02e5c552a685c7bafefea27b93c664b3fb8e18e65eccb820315a456")),
			Delegate.Layout.TypeLayoutHash.ToHexString(),
			TEXT("frozen TypeLayoutHash embedded in legal schema"));
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			Delegate, TEXT("legal unresolved Method Behavior and callable targets"));

		const FAngelscriptCachedTypeSchema Enum = MakeEnumSchema();
		bPassed &= LocalAssert.AreEqual(FString(TEXT(
			"bc379827084ce82a8635f56600fae4de208534bf92a6d229ab0fa3688fce58e1")),
			Enum.KindPayload.Enum->EnumAuthorityHash.ToHexString(),
			TEXT("frozen EnumAuthorityHash embedded in legal schema"));
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			Enum, TEXT("legal unresolved enum serializes without resolver"));

		const FAngelscriptCachedTypeSchema RootClass = MakeOrdinaryUClassSchema(false);
		bPassed &= LocalAssert.AreEqual(FString(TEXT(
			"19903c25b6a2d207614125561a1285221a021062c8219ba41c85a84b89abd04c")),
			RootClass.LayoutInputs[0].LayoutInputHash.ToHexString(),
			TEXT("frozen LayoutInputHash embedded in legal CodeRoot schema"));
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			RootClass, TEXT("legal unresolved CodeRoot and reflected target"));
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			MakeOrdinaryUClassSchema(true),
			TEXT("legal unresolved ScriptType Base and Environment CodeRoot"));
		bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(*TestRunner,
			MakeRecursivePropertyTypeSchema(),
			TEXT("legal nested EnvironmentType serializes without resolver"));

		FAngelscriptCachedTypeSchema Invalid = Delegate;
		Invalid.OrderedProperties[0].StorageLayoutHash = MakeHash(0xf1);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			TEXT("stale StorageLayoutHash"));
		Invalid = Delegate;
		Invalid.OrderedProperties[0].PropertyLayoutFingerprint = MakeHash(0xf2);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			TEXT("stale PropertyLayoutFingerprint"));
		Invalid = Delegate;
		Invalid.Layout.TypeLayoutHash = MakeHash(0xf3);
		bPassed &= ExpectExactProducerFailureAndInputUnchanged(*TestRunner, Invalid,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			TEXT("stale TypeLayoutHash"));

		ASSERT_THAT(IsTrue(bPassed,
			TEXT("normal producer frozen hashes and resolver-independent signature")));
	}

	TEST_METHOD(TypeSemanticFlagPredicateIsExhaustiveForAllSevenKinds)
	{
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Abstract) == 0x01);
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Final) == 0x02);
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Shared) == 0x04);
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::Generated) == 0x08);
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor) == 0x10);
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::HasDestructor) == 0x20);
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ValueType) == 0x40);
		static_assert(static_cast<uint32>(EAngelscriptCachedTypeSemanticFlags::ReferenceType) == 0x80);

		for (uint8 RawKind = 1; RawKind <= 7; ++RawKind)
		{
			const EAngelscriptCachedTypeKind Kind =
				static_cast<EAngelscriptCachedTypeKind>(RawKind);
			for (uint32 Mask = 0; Mask <= 0xff; ++Mask)
			{
				const bool bExpectedValid = IsExpectedTypeSemanticFlagMaskValid(Kind, Mask);
				FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(Kind);
				Schema.TypeSemanticFlags = Mask;
				ConfigureFlagCoupledBehaviors(Schema, Mask);
				TArray<uint8> Payload;
				FAngelscriptCacheTypeSchemaTestWireTrace Trace;
				if (bExpectedValid)
				{
					FinalizeValidFixtureHashes(Schema);
					ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
						Schema, Payload).IsSuccess()));
				}
				else
				{
					ASSERT_THAT(IsTrue(
						FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
							Schema, Payload, Trace).IsSuccess()));
				}
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
				const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
					Payload, Limits, Budget, Output);
				if (bExpectedValid)
				{
					ASSERT_THAT(IsTrue(Result.IsSuccess()),
						*FString::Printf(TEXT("TypeKind %u mask 0x%02x"), RawKind, Mask));
					ASSERT_THAT(IsTrue(Output.IsSet()));
				}
				else
				{
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
						Result, Output,
						(Mask & ~0xffu) != 0
							? EAngelscriptCacheValidationError::UnknownFlags
							: EAngelscriptCacheValidationError::InvalidQualifierCombination,
						EAngelscriptCacheValidationStage::LocalSemantic,
						RequireIndependentSpanOffsetForTests(Payload,
							EAngelscriptCacheTypeSchemaTestField::TypeSemanticFlags),
						TEXT("TypeKind semantic flag exhaustive matrix"))));
				}
			}

			FAngelscriptCachedTypeSchema Unknown = MakeMinimalSchema(Kind);
			Unknown.TypeSemanticFlags = 0x100;
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Unknown);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				EAngelscriptCacheValidationError::UnknownFlags,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::TypeSemanticFlags,
				TEXT("individual unknown TypeSemanticFlags bit"))));
		}
	}
	TEST_METHOD(ReflectionStaticsAndOptionalNameMatrixIsClosed)
	{
		static_assert(static_cast<uint8>(EAngelscriptCachedReflectionKind::None) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedReflectionKind::UClass) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedReflectionKind::UStruct) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedReflectionKind::UEnum) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCachedReflectionKind::UDelegate) == 5);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass) == 0x001);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::StaticsClass) == 0x002);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::Abstract) == 0x004);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::Transient) == 0x008);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::HideDropdown) == 0x010);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::DefaultToInstanced) == 0x020);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::EditInlineNew) == 0x040);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::Deprecated) == 0x080);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::Placeable) == 0x100);
		static_assert(static_cast<uint32>(EAngelscriptCachedClassReflectionFlags::IsStruct) == 0x200);

		for (uint8 RawType = 1; RawType <= 7; ++RawType)
		{
			for (uint8 RawReflection = 1; RawReflection <= 5; ++RawReflection)
			{
				const EAngelscriptCachedTypeKind TypeKind =
					static_cast<EAngelscriptCachedTypeKind>(RawType);
				const EAngelscriptCachedReflectionKind ReflectionKind =
					static_cast<EAngelscriptCachedReflectionKind>(RawReflection);
				const bool bExpectedLegal =
					(TypeKind == EAngelscriptCachedTypeKind::Class
						&& (ReflectionKind == EAngelscriptCachedReflectionKind::None
							|| ReflectionKind == EAngelscriptCachedReflectionKind::UClass))
					|| (TypeKind == EAngelscriptCachedTypeKind::Struct
						&& (ReflectionKind == EAngelscriptCachedReflectionKind::None
							|| ReflectionKind == EAngelscriptCachedReflectionKind::UStruct))
					|| (TypeKind == EAngelscriptCachedTypeKind::Interface
						&& ReflectionKind == EAngelscriptCachedReflectionKind::None)
					|| (TypeKind == EAngelscriptCachedTypeKind::Enum
						&& (ReflectionKind == EAngelscriptCachedReflectionKind::None
							|| ReflectionKind == EAngelscriptCachedReflectionKind::UEnum))
					|| (TypeKind == EAngelscriptCachedTypeKind::Delegate
						&& ReflectionKind == EAngelscriptCachedReflectionKind::UDelegate)
					|| ((TypeKind == EAngelscriptCachedTypeKind::Typedef
						|| TypeKind == EAngelscriptCachedTypeKind::Funcdef)
						&& ReflectionKind == EAngelscriptCachedReflectionKind::None);
				FAngelscriptCachedTypeSchema Schema = bExpectedLegal
					? MakeReflectionFormSchema(TypeKind, ReflectionKind)
					: MakeMinimalSchema(TypeKind);
				Schema.Reflection.ReflectionKind = ReflectionKind;
				TArray<uint8> Payload;
				FAngelscriptCacheTypeSchemaTestWireTrace Trace;
				if (bExpectedLegal)
				{
					ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
						Schema, Payload).IsSuccess()));
				}
				else
				{
					ASSERT_THAT(IsTrue(
						FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
							Schema, Payload, Trace).IsSuccess()));
				}
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
				const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
					Payload, Limits, Budget, Output);
				ASSERT_THAT(AreEqual(bExpectedLegal, Output.IsSet()));
				if (bExpectedLegal)
				{
					ASSERT_THAT(IsTrue(Result.IsSuccess()));
				}
				if (!bExpectedLegal)
				{
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
						Result, Output, EAngelscriptCacheValidationError::InvalidPresence,
						EAngelscriptCacheValidationStage::LocalSemantic,
						RequireIndependentSpanOffsetForTests(Payload,
							EAngelscriptCacheTypeSchemaTestField::Reflection),
						TEXT("TypeKind/reflection closed allowlist"))));
				}
			}
		}

		FAngelscriptCachedTypeSchema Schema = MakeCompleteDelegateSchema();
		Schema.Reflection.ClassReflectionFlags = 0x400;
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::UnknownFlags,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::ClassReflectionFlags,
			TEXT("reflection unknown flags"))));
		Schema = MakeCompleteDelegateSchema();
		Schema.Reflection.ConfigName = TEXT("");
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Reflection,
			TEXT("forbidden present-empty ConfigName"))));
		Schema = MakeCompleteDelegateSchema();
		Schema.Reflection.StaticClassGlobalName = TEXT("");
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Reflection,
			TEXT("forbidden present-empty StaticClassGlobalName"))));
	}

	TEST_METHOD(ReflectionOptionalStringsAndZeroMemberUClassAreFocused)
	{
		const auto AssertValid = [&](FAngelscriptCachedTypeSchema Schema,
			const TCHAR* Context)
		{
			FinalizeValidFixtureHashes(Schema);
			TArray<uint8> Payload;
			ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, Payload).IsSuccess()), Context);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
			ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(Payload,
				FAngelscriptCacheReadLimits{}, Budget, Output).IsSuccess()), Context);
			ASSERT_THAT(IsTrue(Output.IsSet()), Context);
		};
		const auto AssertInvalid = [&](const FAngelscriptCachedTypeSchema& Schema,
			const TCHAR* Context)
		{
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::Reflection, Context)));
		};

		FAngelscriptCachedTypeSchema Ordinary = MakeOrdinaryUClassSchema(false);
		Ordinary.Reflection.OrderedUFunctionMembers.Reset();
		Ordinary.Dependencies.RemoveAll([](const auto& Dependency)
		{
			return Dependency.Kind
				== EAngelscriptCacheSemanticDependencyKind::Declaration;
		});
		AssertValid(Ordinary, TEXT("ordinary UClass permits zero reflected members"));
		Ordinary.Reflection.ConfigName = TEXT("Game");
		AssertValid(Ordinary,
			TEXT("ordinary UClass permits nonempty ConfigName with required static global"));
		Ordinary.Reflection.ConfigName = TEXT("");
		AssertInvalid(Ordinary, TEXT("ordinary UClass rejects present-empty ConfigName"));
		Ordinary = MakeOrdinaryUClassSchema(false);
		Ordinary.Reflection.StaticClassGlobalName.Reset();
		AssertInvalid(Ordinary, TEXT("ordinary UClass requires StaticClassGlobalName"));
		Ordinary.Reflection.StaticClassGlobalName = TEXT("");
		AssertInvalid(Ordinary,
			TEXT("ordinary UClass rejects present-empty StaticClassGlobalName"));

		FAngelscriptCachedTypeSchema Statics = MakeStaticsUClassSchema(1);
		AssertValid(Statics, TEXT("statics UClass requires both optional strings absent"));
		Statics.Reflection.ConfigName = TEXT("Game");
		AssertInvalid(Statics, TEXT("statics UClass forbids ConfigName"));
		Statics = MakeStaticsUClassSchema(1);
		Statics.Reflection.StaticClassGlobalName = TEXT("Unexpected");
		AssertInvalid(Statics, TEXT("statics UClass forbids StaticClassGlobalName"));
		Statics = MakeStaticsUClassSchema(0);
		AssertInvalid(Statics, TEXT("statics UClass forbids zero reflected members"));

		TArray<FAngelscriptCachedTypeSchema> OtherForms = {
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Class),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct),
			MakeReflectedUStructSchema(),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface),
			MakeEnumSchema(),
			MakeCompleteDelegateSchema(),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef),
		};
		for (FAngelscriptCachedTypeSchema Schema : OtherForms)
		{
			Schema.Reflection.ConfigName = TEXT("Forbidden");
			AssertInvalid(Schema, TEXT("non-ordinary-UClass form forbids ConfigName"));
			Schema.Reflection.ConfigName.Reset();
			Schema.Reflection.StaticClassGlobalName = TEXT("Forbidden");
			AssertInvalid(Schema,
				TEXT("non-ordinary-UClass form forbids StaticClassGlobalName"));
		}
	}

	TEST_METHOD(ClassReflectionMasksAreExhaustiveForEveryLegalForm)
	{
		for (const bool bHasScriptBase : {false, true})
		{
			for (const bool bAbstractType : {false, true})
			{
				for (uint32 Mask = 0; Mask <= 0x3ff; ++Mask)
				{
					FAngelscriptCachedTypeSchema Schema =
						MakeOrdinaryUClassSchema(bHasScriptBase);
					Schema.Reflection.ClassReflectionFlags = Mask;
					Schema.TypeSemanticFlags &= ~static_cast<uint32>(
						EAngelscriptCachedTypeSemanticFlags::Abstract);
					if (bAbstractType)
					{
						Schema.TypeSemanticFlags |= static_cast<uint32>(
							EAngelscriptCachedTypeSemanticFlags::Abstract);
					}
					const bool bExpected = IsExpectedClassReflectionMaskValid(
						false, bHasScriptBase, bAbstractType, Mask);
					TArray<uint8> Payload;
					FAngelscriptCacheTypeSchemaTestWireTrace Trace;
					if (bExpected)
					{
						FinalizeValidFixtureHashes(Schema);
						ASSERT_THAT(IsTrue(
							FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
								Schema, Payload).IsSuccess()));
					}
					else
					{
						ASSERT_THAT(IsTrue(
							FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
								Schema, Payload, Trace).IsSuccess()));
					}
					FAngelscriptCacheReadLimits Limits;
					FAngelscriptCacheReadBudget Budget;
					TOptional<FAngelscriptDecodedCacheRecordHandle> Output = MakeSentinelRecord();
					const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
						Payload, Limits, Budget, Output);
					ASSERT_THAT(AreEqual(bExpected, Output.IsSet()));
					if (!bExpected)
					{
						const FString Context = FString::Printf(
							TEXT("ordinary UClass exhaustive reflection mask base=%d abstract=%d mask=0x%03x"),
							bHasScriptBase ? 1 : 0, bAbstractType ? 1 : 0, Mask);
						const bool bParityMismatch =
							((Mask & 0x001u) != 0) != !bHasScriptBase
							|| ((Mask & 0x004u) != 0) != bAbstractType;
						ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
							Result, Output, bParityMismatch
								? EAngelscriptCacheValidationError::InvalidQualifierCombination
								: EAngelscriptCacheValidationError::InvalidPresence,
							EAngelscriptCacheValidationStage::LocalSemantic,
							RequireIndependentSpanOffsetForTests(Payload,
								EAngelscriptCacheTypeSchemaTestField::Reflection),
							*Context)));
					}
				}
			}
		}

		for (uint32 Mask = 0; Mask <= 0x3ff; ++Mask)
		{
			FAngelscriptCachedTypeSchema Schema = MakeStaticsUClassSchema();
			Schema.Reflection.ClassReflectionFlags = Mask;
			const bool bExpected = IsExpectedClassReflectionMaskValid(
				true, false, false, Mask);
			if (bExpected)
			{
				FinalizeValidFixtureHashes(Schema);
			}
			FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			if (bExpected)
			{
				if (!Outcome.Result.IsSuccess())
				{
					TestRunner->AddError(FString::Printf(
						TEXT("statics valid mask=0x%03x unexpectedly failed error=%u stage=%u offset=%llu"),
						Mask, static_cast<uint32>(Outcome.Result.Error),
						static_cast<uint32>(Outcome.Result.Stage), Outcome.Result.ByteOffset));
				}
				ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()));
			}
			ASSERT_THAT(AreEqual(bExpected, Outcome.Output.IsSet()));
			if (!bExpected)
			{
				const FString Context = FString::Printf(
					TEXT("statics UClass exhaustive reflection mask=0x%03x"), Mask);
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
					EAngelscriptCacheValidationError::InvalidPresence,
					EAngelscriptCacheValidationStage::LocalSemantic,
					EAngelscriptCacheTypeSchemaTestField::Reflection,
					*Context)));
			}
		}

		for (uint32 Mask = 0; Mask <= 0x3ff; ++Mask)
		{
			FAngelscriptCachedTypeSchema Schema = MakeReflectedUStructSchema();
			Schema.Reflection.ClassReflectionFlags = Mask;
			FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			const bool bExpected = Mask == 0x200u;
			if (bExpected)
			{
				if (!Outcome.Result.IsSuccess())
				{
					TestRunner->AddError(FString::Printf(
						TEXT("UStruct valid mask=0x%03x unexpectedly failed error=%u stage=%u offset=%llu"),
						Mask, static_cast<uint32>(Outcome.Result.Error),
						static_cast<uint32>(Outcome.Result.Stage), Outcome.Result.ByteOffset));
				}
				ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()));
			}
			ASSERT_THAT(AreEqual(bExpected, Outcome.Output.IsSet()));
			if (!bExpected)
			{
				const FString Context = FString::Printf(
					TEXT("UStruct exhaustive reflection mask=0x%03x"), Mask);
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
					EAngelscriptCacheValidationError::InvalidPresence,
					EAngelscriptCacheValidationStage::LocalSemantic,
					EAngelscriptCacheTypeSchemaTestField::Reflection,
					*Context)));
			}
		}

		TArray<FAngelscriptCachedTypeSchema> ZeroFlagForms = {
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Class),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface),
			MakeReflectionFormSchema(EAngelscriptCachedTypeKind::Enum,
				EAngelscriptCachedReflectionKind::None),
			MakeReflectionFormSchema(EAngelscriptCachedTypeKind::Enum,
				EAngelscriptCachedReflectionKind::UEnum),
			MakeCompleteDelegateSchema(),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef),
			MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef),
		};
		for (const FAngelscriptCachedTypeSchema& Base : ZeroFlagForms)
		{
			for (uint32 Mask = 0; Mask <= 0x3ff; ++Mask)
			{
				FAngelscriptCachedTypeSchema Schema = Base;
				Schema.Reflection.ClassReflectionFlags = Mask;
				FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
				if (Mask == 0)
				{
					ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()));
				}
				ASSERT_THAT(AreEqual(Mask == 0, Outcome.Output.IsSet()));
				if (Mask != 0)
				{
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
						EAngelscriptCacheValidationError::InvalidPresence,
						EAngelscriptCacheValidationStage::LocalSemantic,
						EAngelscriptCacheTypeSchemaTestField::Reflection,
						TEXT("non-class reflection form requires zero class mask"))));
				}
			}
		}

		for (uint32 Bit = 10; Bit < 32; ++Bit)
		{
			FAngelscriptCachedTypeSchema Schema = MakeOrdinaryUClassSchema(false);
			Schema.Reflection.ClassReflectionFlags |= 1u << Bit;
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				EAngelscriptCacheValidationError::UnknownFlags,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::ClassReflectionFlags,
				TEXT("individual unknown class-reflection bit"))));
		}
	}

	TEST_METHOD(OrdinaryStaticsAndUStructReflectionShapesAreIndependent)
	{
		for (const bool bHasScriptBase : {false, true})
		{
			const FAngelscriptCachedTypeSchema Schema =
				MakeOrdinaryUClassSchema(bHasScriptBase);
			TArray<uint8> Payload;
			ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, Payload).IsSuccess()));
			FAngelscriptCacheReadLimits Limits;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
			ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
				Payload, Limits, Budget, Output).IsSuccess()));
			const FAngelscriptCachedTypeSchema& Decoded =
				RequireTypeSchema(Output.GetValue());
			ASSERT_THAT(AreEqual(int32(1), Decoded.Reflection.OrderedUFunctionMembers.Num()));
			ASSERT_THAT(AreEqual(
				Schema.Reflection.OrderedUFunctionMembers[0].CanonicalFunctionName,
				Decoded.Reflection.OrderedUFunctionMembers[0].CanonicalFunctionName));
			ASSERT_THAT(AreEqual(
				Schema.Reflection.OrderedUFunctionMembers[0].CanonicalScriptFunctionName,
				Decoded.Reflection.OrderedUFunctionMembers[0].CanonicalScriptFunctionName));
			ASSERT_THAT(AreEqual(bHasScriptBase,
				Decoded.LayoutInputs[0].InputKind
					== EAngelscriptCachedTypeLayoutInputKind::BaseType));
		}

		const FAngelscriptCachedTypeSchema Statics = MakeStaticsUClassSchema();
		ASSERT_THAT(AreEqual(int32(0), Statics.OrderedProperties.Num()));
		ASSERT_THAT(AreEqual(int32(0), Statics.OrderedMethods.Num()));
		ASSERT_THAT(AreEqual(int32(0), Statics.VirtualFunctionTable.Num()));
		ASSERT_THAT(AreEqual(int32(0), Statics.OrderedBehaviorSlots.Num()));
		ASSERT_THAT(AreEqual(int32(2),
			Statics.Reflection.OrderedUFunctionMembers.Num()));
		FAngelscriptCachedTypeSchema Invalid = Statics;
		Invalid.Reflection.OrderedUFunctionMembers.Reset();
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Reflection,
			TEXT("statics UClass requires one reflected member"))));
		Invalid = Statics;
		Invalid.OrderedMethods.Add(MakeCompleteDelegateSchema().OrderedMethods[0]);
		Invalid.OrderedMethods[0].DeclaringOwner = Invalid.TypeKey;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedMethod,
			TEXT("statics UClass forbids VM methods"), 0)));

		const FAngelscriptCachedTypeSchema UStruct = MakeReflectedUStructSchema();
		ASSERT_THAT(IsTrue(UStruct.LayoutInputs[0].BoundaryContribution.IsSet()));
		ASSERT_THAT(IsFalse(UStruct.LayoutInputs[0].AlignmentContribution.IsSet()));
		Invalid = UStruct;
		Invalid.LayoutInputs[0].AlignmentContribution = 8;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::LayoutInput,
			TEXT("UStruct header forbids alignment contribution"), 0)));
	}

	TEST_METHOD(ReflectedUFunctionOrdinalsAndReferenceShapeAreExhaustiveLocally)
	{
		for (const bool bStatics : {false, true})
		{
			for (const uint32 Count : {uint32(1), uint32(3)})
			{
				FAngelscriptCachedTypeSchema Schema = bStatics
					? MakeStaticsUClassSchema(Count)
					: MakeOrdinaryUClassSchema(false);
				if (!bStatics)
				{
					Schema.Reflection.OrderedUFunctionMembers.Reset();
					Schema.Dependencies.RemoveAll([](const auto& Dependency)
					{
						return Dependency.Kind
							== EAngelscriptCacheSemanticDependencyKind::Declaration;
					});
					for (uint32 Index = 0; Index < Count; ++Index)
					{
						const FAngelscriptCachedReflectedFunctionMember Member =
							MakeReflectedMember(Index,
								static_cast<uint8>(0xd0 + Index * 2),
								static_cast<uint8>(0xd1 + Index * 2));
						Schema.Reflection.OrderedUFunctionMembers.Add(Member);
						Schema.Dependencies.Add(MakeDependency(
							EAngelscriptCacheSemanticDependencyKind::Declaration,
							Member.Target));
					}
					FinalizeValidFixtureHashes(Schema);
				}
				TArray<uint8> Payload;
				ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
					Schema, Payload).IsSuccess()));
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
				ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
					Payload, Limits, Budget, Output).IsSuccess()));
				ASSERT_THAT(AreEqual(int32(Count), RequireTypeSchema(
					Output.GetValue()).Reflection.OrderedUFunctionMembers.Num()));

				if (Count == 3)
				{
					for (uint32 Position = 0; Position < Count; ++Position)
					{
						FAngelscriptCachedTypeSchema Gap = Schema;
						for (uint32 Index = Position; Index < Count; ++Index)
						{
							++Gap.Reflection.OrderedUFunctionMembers[Index].ReflectionOrdinal;
						}
						ASSERT_THAT(IsTrue(
							RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
								Gap).IsSuccess()));
						const FMalformedDecodeOutcome GapOutcome =
							DecodePhysicalOnlyFixture(Gap);
						ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, GapOutcome,
							EAngelscriptCacheValidationError::OrdinalGap,
							EAngelscriptCacheValidationStage::LocalSemantic,
							EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers,
							TEXT("independent UFunction ordinal gap"), Position)));

						if (Position > 0)
						{
							FAngelscriptCachedTypeSchema Duplicate = Schema;
							Duplicate.Reflection.OrderedUFunctionMembers[Position].ReflectionOrdinal =
								Position - 1;
							ASSERT_THAT(IsTrue(
								RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
									Duplicate).IsSuccess()));
							const FMalformedDecodeOutcome DuplicateOutcome =
								DecodePhysicalOnlyFixture(Duplicate);
							ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
								DuplicateOutcome,
								EAngelscriptCacheValidationError::DuplicateOrdinal,
								EAngelscriptCacheValidationStage::LocalSemantic,
								EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers,
								TEXT("isolated UFunction duplicate ordinal"), Position)));
						}

						FAngelscriptCachedTypeSchema Reordered = Schema;
						const uint32 Peer = Position == 2 ? 1 : Position + 1;
						Swap(Reordered.Reflection.OrderedUFunctionMembers[Position],
							Reordered.Reflection.OrderedUFunctionMembers[Peer]);
						ASSERT_THAT(IsTrue(
							RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
								Reordered).IsSuccess()));
						const FMalformedDecodeOutcome ReorderedOutcome =
							DecodePhysicalOnlyFixture(Reordered);
						ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, ReorderedOutcome,
							EAngelscriptCacheValidationError::NonCanonicalOrder,
							EAngelscriptCacheValidationStage::LocalSemantic,
							EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers,
							TEXT("independent UFunction complete-row reorder"),
							FMath::Min(Position, Peer))));
					}
				}
			}
		}

		FAngelscriptCachedTypeSchema Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.Reflection.OrderedUFunctionMembers[0].Target.Kind =
			EAngelscriptCacheReferenceKind::EnvironmentSymbol;
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::WrongReferenceKind,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers,
			TEXT("UFunction target reference kind"), 0, 0)));
		Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.Reflection.OrderedUFunctionMembers[0].Target.ExpectedAbi = {};
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::MissingExpectedAbi,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers,
			TEXT("UFunction target expected ABI"), 0, 0)));
		Invalid = MakeEnumSchema();
		Invalid.Reflection.OrderedUFunctionMembers.Add(MakeReflectedMember(0, 0xe0, 0xe1));
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::ReflectedFunctionMembers,
			TEXT("non-UClass forbids reflected UFunction members"), 0)));
	}

	TEST_METHOD(ReflectedFunctionNamesRoundTripExactlyAndRequiredNamesFailClosed)
	{
		FAngelscriptCachedTypeSchema Schema = MakeOrdinaryUClassSchema(false);
		FAngelscriptCachedReflectedFunctionMember& Member =
			Schema.Reflection.OrderedUFunctionMembers[0];
		Member.CanonicalFunctionName = TEXT("NativeDisplayName");
		Member.CanonicalOriginalFunctionName = TEXT("ScriptVisibleName");
		Member.CanonicalScriptFunctionName = TEXT("ScriptVisibleName_Implementation");
		FinalizeValidFixtureHashes(Schema);

		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Payload).IsSuccess()));
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(Payload,
			FAngelscriptCacheReadLimits{}, Budget, Output).IsSuccess()));
		const FAngelscriptCachedReflectedFunctionMember& Decoded =
			RequireTypeSchema(Output.GetValue()).Reflection.OrderedUFunctionMembers[0];
		ASSERT_THAT(AreEqual(Member.CanonicalFunctionName,
			Decoded.CanonicalFunctionName));
		ASSERT_THAT(AreEqual(Member.CanonicalOriginalFunctionName,
			Decoded.CanonicalOriginalFunctionName));
		ASSERT_THAT(AreEqual(Member.CanonicalScriptFunctionName,
			Decoded.CanonicalScriptFunctionName));

		FAngelscriptCachedTypeSchema Invalid = Schema;
		Invalid.Reflection.OrderedUFunctionMembers[0].CanonicalFunctionName.Reset();
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		const FIndependentTypeSchemaWireInventoryForTests FunctionNameInventory =
			ScanIndependentTypeSchemaWireForTests(Outcome.Payload);
		ASSERT_THAT(IsTrue(FunctionNameInventory.bComplete));
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
			Outcome.Result, Outcome.Output,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			FunctionNameInventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::ReflectedFunctionName, 0}).GetValue(),
			TEXT("reflected Unreal function name is required"))));

		Invalid = Schema;
		Invalid.Reflection.OrderedUFunctionMembers[0].CanonicalScriptFunctionName.Reset();
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		const FIndependentTypeSchemaWireInventoryForTests ScriptNameInventory =
			ScanIndependentTypeSchemaWireForTests(Outcome.Payload);
		ASSERT_THAT(IsTrue(ScriptNameInventory.bComplete));
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
			Outcome.Result, Outcome.Output,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			ScriptNameInventory.FindCapturedOffset({
				EAngelscriptTypeSchemaCapturedField::ReflectedScriptFunctionName, 0}).GetValue(),
			TEXT("reflected script implementation name is required"))));

		FAngelscriptCachedTypeSchema EmptyOriginal = Schema;
		EmptyOriginal.Reflection.OrderedUFunctionMembers[0].
			CanonicalOriginalFunctionName.Reset();
		FinalizeValidFixtureHashes(EmptyOriginal);
		Payload.Reset();
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			EmptyOriginal, Payload).IsSuccess()));
	}
	TEST_METHOD(RelationsAndDirectInterfaceOrdinalsRemainCanonical)
	{
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeRelationKind::Base) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeRelationKind::ShadowSuper) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeRelationKind::CodeSuper) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeRelationKind::ImplementedInterface) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeRelationKind::Compose) == 5);

		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		for (uint32 Ordinal = 0; Ordinal < 2; ++Ordinal)
		{
			FAngelscriptCachedTypeRelation Relation;
			Relation.RelationKind = EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			Relation.SemanticOrdinal = Ordinal;
			Relation.Target = MakeReference(EAngelscriptCacheReferenceKind::ScriptType,
				static_cast<uint8>(0x70 + Ordinal), static_cast<uint8>(0x80 + Ordinal));
			Schema.Relations.Add(Relation);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Inheritance, Relation.Target));
		}
		FinalizeValidFixtureHashes(Schema);
		TArray<uint8> Bytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Bytes).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Bytes, Limits, Budget, Record).IsSuccess()));
		ASSERT_THAT(IsTrue(Record.IsSet()));

		FAngelscriptCachedTypeSchema ReorderedTargets = Schema;
		Swap(ReorderedTargets.Relations[0].Target, ReorderedTargets.Relations[1].Target);
		FinalizeValidFixtureHashes(ReorderedTargets);
		ASSERT_THAT(IsFalse(Schema.Layout.TypeLayoutHash
			== ReorderedTargets.Layout.TypeLayoutHash),
			TEXT("direct interface order must contribute to TypeLayoutHash"));
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			ReorderedTargets, Bytes).IsSuccess()));
		FAngelscriptCacheReadLimits ReorderedLimits;
		FAngelscriptCacheReadBudget ReorderedBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> ReorderedRecord;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Bytes, ReorderedLimits, ReorderedBudget, ReorderedRecord).IsSuccess()));
		ASSERT_THAT(IsTrue(ReorderedRecord.IsSet()));

		FAngelscriptCachedTypeSchema Invalid = Schema;
		Invalid.Relations[1].SemanticOrdinal = 0;
		ASSERT_THAT(IsTrue(
			RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
				Invalid).IsSuccess()));
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DuplicateOrdinal,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("direct-interface duplicate ordinal"), 1)));
		Invalid = Schema;
		Invalid.Relations[1].SemanticOrdinal = 2;
		ASSERT_THAT(IsTrue(
			RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
				Invalid).IsSuccess()));
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::OrdinalGap,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("direct-interface ordinal gap"), 1)));
		Invalid = Schema;
		Invalid.Relations[0].SemanticOrdinal.Reset();
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("direct-interface SemanticOrdinal is required"), 0)));
		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		FAngelscriptCachedTypeRelation Compose;
		Compose.RelationKind = EAngelscriptCachedTypeRelationKind::Compose;
		Compose.Target = MakeReference(EAngelscriptCacheReferenceKind::ScriptType, 0x75, 0x85);
		Invalid.Relations.Add(Compose);
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("Compose relation is forbidden on Class"), 0)));
	}

	TEST_METHOD(DirectInterfaceOrdinalGapDuplicateAndReorderAreIndependent)
	{
		FAngelscriptCachedTypeSchema Valid = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		for (uint32 Index = 0; Index < 3; ++Index)
		{
			FAngelscriptCachedTypeRelation Relation;
			Relation.RelationKind =
				EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			Relation.SemanticOrdinal = Index;
			Relation.Target = MakeReference(EAngelscriptCacheReferenceKind::ScriptType,
				static_cast<uint8>(0x70 + Index), static_cast<uint8>(0x80 + Index));
			Valid.Relations.Add(Relation);
			Valid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Inheritance, Relation.Target));
		}
		Valid.Dependencies.Sort([](const auto& A, const auto& B)
		{
			const uint8 AKind = static_cast<uint8>(A.Kind);
			const uint8 BKind = static_cast<uint8>(B.Kind);
			return AKind != BKind ? AKind < BKind
				: A.Target.StableKey < B.Target.StableKey;
		});
		FinalizeValidFixtureHashes(Valid);
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Valid, Payload).IsSuccess()));

		for (uint32 Position = 0; Position < 3; ++Position)
		{
			FAngelscriptCachedTypeSchema Gap = Valid;
			for (uint32 Index = Position; Index < 3; ++Index)
			{
				Gap.Relations[Index].SemanticOrdinal =
					Gap.Relations[Index].SemanticOrdinal.GetValue() + 1;
			}
			ASSERT_THAT(IsTrue(
				RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
					Gap).IsSuccess()));
			FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Gap);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				EAngelscriptCacheValidationError::OrdinalGap,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::Relations,
				TEXT("independent direct-interface ordinal gap"), Position)));

			if (Position > 0)
			{
				FAngelscriptCachedTypeSchema Duplicate = Valid;
				// Mutate only the ordinal. Copying the complete peer row would also
				// duplicate Target identity and turn this into a multi-fault fixture.
				Duplicate.Relations[Position].SemanticOrdinal = Position - 1;
				ASSERT_THAT(IsTrue(
					RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
						Duplicate).IsSuccess()));
				Outcome = DecodePhysicalOnlyFixture(Duplicate);
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
					EAngelscriptCacheValidationError::DuplicateOrdinal,
					EAngelscriptCacheValidationStage::LocalSemantic,
					EAngelscriptCacheTypeSchemaTestField::Relations,
					TEXT("isolated direct-interface duplicate ordinal"), Position)));
			}

			FAngelscriptCachedTypeSchema Reordered = Valid;
			const uint32 Peer = Position == 2 ? 1 : Position + 1;
			Swap(Reordered.Relations[Position], Reordered.Relations[Peer]);
			ASSERT_THAT(IsTrue(
				RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
					Reordered).IsSuccess()));
			Outcome = DecodePhysicalOnlyFixture(Reordered);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				EAngelscriptCacheValidationError::NonCanonicalOrder,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::Relations,
				TEXT("independent direct-interface complete-row reorder"),
				FMath::Min(Position, Peer))));
		}
	}

	TEST_METHOD(RelationStructuralFailuresAreSingleFaultAndExact)
	{
		const auto MakeOneInterface = [&]()
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			FAngelscriptCachedTypeRelation Relation;
			Relation.RelationKind =
				EAngelscriptCachedTypeRelationKind::ImplementedInterface;
			Relation.SemanticOrdinal = 0;
			Relation.Target = MakeReference(
				EAngelscriptCacheReferenceKind::ScriptType, 0x71, 0x81);
			Schema.Relations.Add(Relation);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Inheritance,
				Relation.Target));
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};

		FAngelscriptCachedTypeSchema Invalid = MakeOneInterface();
		Invalid.Relations[0].Target.StableKey = Invalid.TypeKey.Hash;
		Invalid.Dependencies[0].Target = Invalid.Relations[0].Target;
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::ConflictingKey,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("relation target cannot self-reference enclosing TypeKey"), 0)));

		Invalid = MakeOneInterface();
		Invalid.Relations[0].Target.ExpectedAbi = {};
		Invalid.Dependencies[0].Target = Invalid.Relations[0].Target;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::MissingExpectedAbi,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("relation missing required ExpectedAbi"), 0)));

		Invalid = MakeOneInterface();
		Invalid.Relations[0].Target.StableKey = {};
		Invalid.Dependencies[0].Target = Invalid.Relations[0].Target;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("relation zero StableKey"), 0)));

		Invalid = MakeOneInterface();
		Invalid.Relations.Add(FAngelscriptCachedTypeRelation(Invalid.Relations[0]));
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DuplicateKey,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("duplicate-identical direct interface row"), 1)));
	}

	TEST_METHOD(RelationKindsFormsCardinalitiesAndReferenceKindsAreCartesian)
	{
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][Relations][Decoder] begin "
			"matrix=11 forms x 5 kinds x 3 cardinalities x 3 references; "
			"expected-total=495"));
		enum class EForm : uint8
		{
			ClassNone,
			OrdinaryUClass,
			StaticsUClass,
			StructNone,
			UStruct,
			InterfaceNone,
			EnumNone,
			UEnum,
			Delegate,
			Typedef,
			Funcdef,
		};
		struct FFormFixture
		{
			EForm Form;
			FAngelscriptCachedTypeSchema Schema;
		};
		const TArray<FFormFixture> Forms = {
			{EForm::ClassNone, MakeMinimalSchema(EAngelscriptCachedTypeKind::Class)},
			{EForm::OrdinaryUClass, MakeOrdinaryUClassSchema(false)},
			{EForm::StaticsUClass, MakeStaticsUClassSchema()},
			{EForm::StructNone, MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct)},
			{EForm::UStruct, MakeReflectedUStructSchema()},
			{EForm::InterfaceNone, MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface)},
			{EForm::EnumNone, MakeReflectionFormSchema(EAngelscriptCachedTypeKind::Enum,
				EAngelscriptCachedReflectionKind::None)},
			{EForm::UEnum, MakeEnumSchema()},
			{EForm::Delegate, MakeCompleteDelegateSchema()},
			{EForm::Typedef, MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef)},
			{EForm::Funcdef, MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef)},
		};
		int32 MatrixCalls = 0;
		int32 ExpectedSuccessCalls = 0;
		int32 ExpectedFailureCalls = 0;
		for (const FFormFixture& FormFixture : Forms)
		{
			for (uint8 RawKind = 1; RawKind <= 5; ++RawKind)
			{
				const EAngelscriptCachedTypeRelationKind Kind =
					static_cast<EAngelscriptCachedTypeRelationKind>(RawKind);
				const bool bAllowedRelationCoordinate = [&]()
				{
					switch (FormFixture.Form)
					{
					case EForm::ClassNone:
						return Kind == EAngelscriptCachedTypeRelationKind::Base
							|| Kind == EAngelscriptCachedTypeRelationKind::ImplementedInterface;
					case EForm::OrdinaryUClass:
						return Kind == EAngelscriptCachedTypeRelationKind::Base
							|| Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
							|| Kind == EAngelscriptCachedTypeRelationKind::CodeSuper
							|| Kind == EAngelscriptCachedTypeRelationKind::ImplementedInterface;
					case EForm::StaticsUClass:
						return Kind == EAngelscriptCachedTypeRelationKind::CodeSuper;
					case EForm::InterfaceNone:
						return Kind == EAngelscriptCachedTypeRelationKind::ImplementedInterface;
					case EForm::StructNone:
					case EForm::UStruct:
					case EForm::EnumNone:
					case EForm::UEnum:
					case EForm::Delegate:
					case EForm::Typedef:
					case EForm::Funcdef:
						return false;
					}
					return false;
				}();
				const bool bRequiredRelationCoordinate =
					(FormFixture.Form == EForm::OrdinaryUClass
						&& (Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
							|| Kind == EAngelscriptCachedTypeRelationKind::CodeSuper))
					|| (FormFixture.Form == EForm::StaticsUClass
						&& Kind == EAngelscriptCachedTypeRelationKind::CodeSuper);
				const auto IsExpectedReferenceCase = [&]() -> uint8
				{
					if (!bAllowedRelationCoordinate)
					{
						return MAX_uint8;
					}
					if (Kind == EAngelscriptCachedTypeRelationKind::Base)
					{
						return uint8(0);
					}
					if (Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
						|| Kind == EAngelscriptCachedTypeRelationKind::CodeSuper)
					{
						return uint8(1);
					}
					return FormFixture.Form == EForm::OrdinaryUClass
						? uint8(1) : uint8(0);
				};
				for (uint32 Cardinality = 0; Cardinality <= 2; ++Cardinality)
				{
					for (uint8 ReferenceCase = 0; ReferenceCase < 3; ++ReferenceCase)
					{
						FAngelscriptCachedTypeSchema Schema = FormFixture.Schema;
						TArray<FAngelscriptCacheStableReference> RemovedTargets;
						for (const FAngelscriptCachedTypeRelation& Relation : Schema.Relations)
						{
							if (Relation.RelationKind == Kind)
							{
								RemovedTargets.Add(Relation.Target);
							}
						}
						Schema.Relations.RemoveAll([Kind](const FAngelscriptCachedTypeRelation& Relation)
						{
							return Relation.RelationKind == Kind;
						});
						Schema.Dependencies.RemoveAll([&](
							const FAngelscriptCacheSemanticDependency& Dependency)
						{
							return RemovedTargets.Contains(Dependency.Target);
						});
						if (Kind == EAngelscriptCachedTypeRelationKind::Base)
						{
							Schema.LayoutInputs.RemoveAll([](
								const FAngelscriptCachedTypeLayoutInput& Input)
							{
								return Input.InputKind
									== EAngelscriptCachedTypeLayoutInputKind::BaseType;
							});
						}
						if (Kind == EAngelscriptCachedTypeRelationKind::CodeSuper
							&& FormFixture.Form == EForm::OrdinaryUClass)
						{
							Schema.LayoutInputs.RemoveAll([](
								const FAngelscriptCachedTypeLayoutInput& Input)
							{
								return Input.InputKind
									== EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
							});
						}

						for (uint32 Index = 0; Index < Cardinality; ++Index)
						{
							EAngelscriptCacheReferenceKind ReferenceKind =
								ReferenceCase == 0
								? EAngelscriptCacheReferenceKind::ScriptType
								: ReferenceCase == 1
									? EAngelscriptCacheReferenceKind::EnvironmentSymbol
									: EAngelscriptCacheReferenceKind::ScriptFunction;
							FAngelscriptCacheStableReference Target = MakeReference(
								ReferenceKind, static_cast<uint8>(0xd0 + Index * 2),
								static_cast<uint8>(0xd1 + Index * 2));
							if (FormFixture.Form == EForm::OrdinaryUClass
								&& ReferenceKind == EAngelscriptCacheReferenceKind::EnvironmentSymbol
								&& Index == 0
								&& (Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
									|| Kind == EAngelscriptCachedTypeRelationKind::CodeSuper))
							{
								const EAngelscriptCachedTypeRelationKind PeerKind =
									Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
									? EAngelscriptCachedTypeRelationKind::CodeSuper
									: EAngelscriptCachedTypeRelationKind::ShadowSuper;
								for (const FAngelscriptCachedTypeRelation& Peer : Schema.Relations)
								{
									if (Peer.RelationKind == PeerKind)
									{
										Target = Peer.Target;
										break;
									}
								}
							}
							FAngelscriptCachedTypeRelation Relation;
							Relation.RelationKind = Kind;
							Relation.Target = Target;
							if (Kind == EAngelscriptCachedTypeRelationKind::ImplementedInterface)
							{
								Relation.SemanticOrdinal = Index;
							}
							Schema.Relations.Add(Relation);
							Schema.Dependencies.Add(MakeDependency(
								Target.Kind == EAngelscriptCacheReferenceKind::ScriptType
									? EAngelscriptCacheSemanticDependencyKind::Inheritance
									: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
								Target));
							if (Kind == EAngelscriptCachedTypeRelationKind::Base
								&& Index == 0)
							{
								FAngelscriptCachedTypeLayoutInput Input;
								Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
								Input.Target = ReferenceKind
									== EAngelscriptCacheReferenceKind::ScriptType
									? Target
									: MakeReference(EAngelscriptCacheReferenceKind::ScriptType,
										0xe8, 0xe9);
								Input.BoundaryContribution = 0;
								Input.AlignmentContribution = 8;
								Schema.LayoutInputs.Add(Input);
							}
							if (Kind == EAngelscriptCachedTypeRelationKind::CodeSuper
								&& FormFixture.Form == EForm::OrdinaryUClass
								&& Index == 0)
							{
								FAngelscriptCachedTypeLayoutInput Input;
								Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
								Input.Target = ReferenceKind
									== EAngelscriptCacheReferenceKind::EnvironmentSymbol
									? Target
									: MakeReference(
										EAngelscriptCacheReferenceKind::EnvironmentSymbol,
										0xea, 0xeb);
								Input.BoundaryContribution = 0;
								Input.AlignmentContribution = 8;
								Schema.LayoutInputs.Add(Input);
							}
						}

						const bool bScript = ReferenceCase == 0;
						const bool bEnvironment = ReferenceCase == 1;
						bool bExpected = false;
						if (Cardinality == 0)
						{
							bExpected = !((FormFixture.Form == EForm::OrdinaryUClass
								&& (Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper
									|| Kind == EAngelscriptCachedTypeRelationKind::CodeSuper))
								|| (FormFixture.Form == EForm::StaticsUClass
									&& Kind == EAngelscriptCachedTypeRelationKind::CodeSuper));
						}
						else if (Kind == EAngelscriptCachedTypeRelationKind::Base)
						{
							bExpected = Cardinality == 1 && bScript
								&& (FormFixture.Form == EForm::ClassNone
									|| FormFixture.Form == EForm::OrdinaryUClass);
						}
						else if (Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper)
						{
							bExpected = Cardinality == 1 && bEnvironment
								&& FormFixture.Form == EForm::OrdinaryUClass;
						}
						else if (Kind == EAngelscriptCachedTypeRelationKind::CodeSuper)
						{
							bExpected = Cardinality == 1 && bEnvironment
								&& (FormFixture.Form == EForm::OrdinaryUClass
									|| FormFixture.Form == EForm::StaticsUClass);
						}
						else if (Kind == EAngelscriptCachedTypeRelationKind::ImplementedInterface)
						{
							bExpected = (FormFixture.Form == EForm::ClassNone && bScript)
								|| (FormFixture.Form == EForm::OrdinaryUClass && bEnvironment)
								|| (FormFixture.Form == EForm::InterfaceNone && bScript);
						}
						++MatrixCalls;
						bExpected ? ++ExpectedSuccessCalls : ++ExpectedFailureCalls;

						if (FormFixture.Form == EForm::OrdinaryUClass
							&& Kind == EAngelscriptCachedTypeRelationKind::Base)
						{
							const uint32 SuperBit = static_cast<uint32>(
								EAngelscriptCachedClassReflectionFlags::SuperIsCodeClass);
							Schema.Reflection.ClassReflectionFlags = Cardinality == 0
								? Schema.Reflection.ClassReflectionFlags | SuperBit
								: Schema.Reflection.ClassReflectionFlags & ~SuperBit;
							for (FAngelscriptCachedTypeLayoutInput& Input : Schema.LayoutInputs)
							{
								if (Input.InputKind == EAngelscriptCachedTypeLayoutInputKind::CodeRoot)
								{
									if (Cardinality == 0)
									{
										Input.BoundaryContribution = 0;
									}
									else
									{
										Input.BoundaryContribution.Reset();
									}
								}
							}
						}
						Schema.Relations.StableSort([](const auto& A, const auto& B)
						{
							return static_cast<uint8>(A.RelationKind)
								< static_cast<uint8>(B.RelationKind);
						});
						Schema.Dependencies.Sort([](const auto& A, const auto& B)
						{
							return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
						});
						Schema.LayoutInputs.StableSort([](const auto& A, const auto& B)
						{
							return static_cast<uint8>(A.InputKind)
								< static_cast<uint8>(B.InputKind);
						});
						ASSERT_THAT(IsTrue(
							RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
								Schema).IsSuccess()));
						int32 FirstRelationIndex = INDEX_NONE;
						for (int32 RelationIndex = 0;
							RelationIndex < Schema.Relations.Num(); ++RelationIndex)
						{
							if (Schema.Relations[RelationIndex].RelationKind == Kind)
							{
								FirstRelationIndex = RelationIndex;
								break;
							}
						}
						const FMalformedDecodeOutcome Outcome =
							DecodePhysicalOnlyFixture(Schema);
						const FString Context = FString::Printf(
							TEXT("relation Cartesian form=%u kind=%u cardinality=%u reference=%u "
								"actual-error=%u actual-stage=%u actual-offset=%llu"),
							static_cast<uint8>(FormFixture.Form), RawKind,
							Cardinality, ReferenceCase,
							static_cast<uint8>(Outcome.Result.Error),
							static_cast<uint8>(Outcome.Result.Stage),
							Outcome.Result.ByteOffset);
						const bool bWrongReferenceKind = Cardinality > 0
							&& (ReferenceCase == 2
								|| (bAllowedRelationCoordinate
									&& ReferenceCase != IsExpectedReferenceCase()));
						const bool bStaticsBaseParityFailure = Cardinality > 0
							&& ReferenceCase != 2
							&& FormFixture.Form == EForm::StaticsUClass
							&& Kind == EAngelscriptCachedTypeRelationKind::Base;
						const bool bStaticsShadowWitnessFailure = Cardinality > 0
							&& ReferenceCase != 2
							&& FormFixture.Form == EForm::StaticsUClass
							&& Kind == EAngelscriptCachedTypeRelationKind::ShadowSuper;
						const EAngelscriptCacheValidationError Expected = bExpected
							? EAngelscriptCacheValidationError::None
							: bStaticsBaseParityFailure
								? EAngelscriptCacheValidationError::InvalidQualifierCombination
							: bWrongReferenceKind
								? EAngelscriptCacheValidationError::WrongReferenceKind
								: Cardinality == 2
									&& bAllowedRelationCoordinate
									&& Kind != EAngelscriptCachedTypeRelationKind::ImplementedInterface
									? EAngelscriptCacheValidationError::ConflictingKey
									: EAngelscriptCacheValidationError::InvalidPresence;
						if (bExpected)
						{
							ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()));
						}
						ASSERT_THAT(AreEqual(bExpected, Outcome.Output.IsSet()));
						if (!bExpected)
						{
							if ((Cardinality == 0 && bRequiredRelationCoordinate)
								|| bStaticsBaseParityFailure || bStaticsShadowWitnessFailure)
							{
								ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
									Outcome, Expected,
									EAngelscriptCacheValidationStage::LocalSemantic,
									EAngelscriptCacheTypeSchemaTestField::Reflection,
									*Context)));
							}
							else
							{
								const int32 ProvingRelationIndex = Expected
									== EAngelscriptCacheValidationError::ConflictingKey
									? FirstRelationIndex + 1 : FirstRelationIndex;
								ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
									Outcome, Expected,
									EAngelscriptCacheValidationStage::LocalSemantic,
									EAngelscriptCacheTypeSchemaTestField::Relations,
									*Context,
									ProvingRelationIndex)));
							}
						}
					}
				}
			}
		}
		ASSERT_THAT(AreEqual(495, MatrixCalls,
			TEXT("Relations decoder Cartesian matrix must visit every frozen cell")));
		TestRunner->AddInfo(FString::Printf(TEXT(
			"[CacheV2][TypeSchema][Relations][Decoder] complete "
			"total=%d expected-success=%d expected-failure=%d"),
			MatrixCalls, ExpectedSuccessCalls, ExpectedFailureCalls));
	}
	TEST_METHOD(LayoutInputsPreservePresentZeroAndExactRolePresence)
	{
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][LayoutInputs][DecoderPresence] begin "
			"present-zero=1 role-mask-negatives=2 wrong-reference=1 duplicate=1"));
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeLayoutInputKind::BaseType) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeLayoutInputKind::CodeRoot) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedTypeLayoutInputKind::StructHeader) == 3);

		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		FAngelscriptCachedTypeRelation Base;
		Base.RelationKind = EAngelscriptCachedTypeRelationKind::Base;
		Base.Target = MakeReference(EAngelscriptCacheReferenceKind::ScriptType, 0x70, 0x71);
		Schema.Relations.Add(Base);
		FAngelscriptCachedTypeLayoutInput Input;
		Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
		Input.Target = Base.Target;
		Input.BoundaryContribution = 0;
		Input.AlignmentContribution = 8;
		Schema.LayoutInputs.Add(Input);
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::Inheritance, Base.Target));
		FinalizeValidFixtureHashes(Schema);

		TArray<uint8> Bytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Bytes).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Bytes, Limits, Budget, Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		const FAngelscriptCachedTypeSchema& DecodedSchema =
			RequireTypeSchema(Decoded.GetValue());
		ASSERT_THAT(IsTrue(DecodedSchema.LayoutInputs[0].BoundaryContribution.IsSet()));
		ASSERT_THAT(AreEqual(uint32(0),
			DecodedSchema.LayoutInputs[0].BoundaryContribution.GetValue()));

		FAngelscriptCachedTypeSchema Invalid = Schema;
		Invalid.LayoutInputs[0].BoundaryContribution.Reset();
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::LayoutInput,
			TEXT("BaseType boundary required"), 0)));
		Invalid = Schema;
		Invalid.LayoutInputs[0].AlignmentContribution.Reset();
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::LayoutInput,
			TEXT("BaseType alignment required"), 0)));
		Invalid = Schema;
		Invalid.LayoutInputs[0].InputKind = EAngelscriptCachedTypeLayoutInputKind::CodeRoot;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::WrongReferenceKind,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::LayoutInput,
			TEXT("wrong role retains the old role's target ReferenceKind"), 0)));
		Invalid = Schema;
		Invalid.LayoutInputs.Add(FAngelscriptCachedTypeLayoutInput(Invalid.LayoutInputs[0]));
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DuplicateKey,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::LayoutInput,
			TEXT("duplicate LayoutInput role selects the second physical row"), 1)));
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][LayoutInputs][DecoderPresence] complete "
			"present-zero=1 role-mask-negatives=2 wrong-reference=1 duplicate=1 total=5"));
	}
	TEST_METHOD(LayoutInputRolePresenceAndTargetMatrixIsComplete)
	{
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][LayoutInputs][DecoderMatrix] begin "
			"roles=4 contribution-masks=4 expected-mask-cells=16 "
			"target-checks=16 missing-checks=4 expected-total=36"));
		static_assert(std::is_same_v<
			decltype(FAngelscriptCachedTypeLayoutInput::Target),
			FAngelscriptCacheStableReference>,
			"LayoutInput Target is a required direct stable reference");
		struct FRoleFixture
		{
			FAngelscriptCachedTypeSchema Schema;
			int32 InputIndex;
			uint8 RequiredContributionMask;
			const TCHAR* Context;
		};
		const TArray<FRoleFixture> Fixtures = {
			{[]
			{
				FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
					EAngelscriptCachedTypeKind::Class);
				FAngelscriptCachedTypeRelation Base;
				Base.RelationKind = EAngelscriptCachedTypeRelationKind::Base;
				Base.Target = MakeReference(EAngelscriptCacheReferenceKind::ScriptType,
					0x70, 0x71);
				Schema.Relations.Add(Base);
				FAngelscriptCachedTypeLayoutInput Input;
				Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
				Input.Target = Base.Target;
				Input.BoundaryContribution = 0;
				Input.AlignmentContribution = 8;
				Schema.LayoutInputs.Add(Input);
				Schema.Dependencies.Add(MakeDependency(
					EAngelscriptCacheSemanticDependencyKind::Inheritance, Base.Target));
				FinalizeValidFixtureHashes(Schema);
				return Schema;
			}(), 0, 0x3, TEXT("BaseType")},
			{MakeOrdinaryUClassSchema(false), 0, 0x3, TEXT("root CodeRoot")},
			{MakeOrdinaryUClassSchema(true), 1, 0x2, TEXT("derived CodeRoot")},
			{MakeReflectedUStructSchema(), 0, 0x1, TEXT("StructHeader")},
		};
		int32 ContributionMaskCells = 0;
		int32 ExpectedMaskSuccesses = 0;
		int32 ExpectedMaskFailures = 0;
		int32 TargetChecks = 0;
		int32 MissingChecks = 0;
		for (const FRoleFixture& Fixture : Fixtures)
		{
			for (uint8 ContributionMask = 0; ContributionMask < 4; ++ContributionMask)
			{
				++ContributionMaskCells;
				FAngelscriptCachedTypeSchema Schema = Fixture.Schema;
				FAngelscriptCachedTypeLayoutInput& Input =
					Schema.LayoutInputs[Fixture.InputIndex];
				if ((ContributionMask & 0x1) == 0)
				{
					Input.BoundaryContribution.Reset();
				}
				else if (!Input.BoundaryContribution.IsSet())
				{
					Input.BoundaryContribution = 0;
				}
				if ((ContributionMask & 0x2) == 0)
				{
					Input.AlignmentContribution.Reset();
				}
				else if (!Input.AlignmentContribution.IsSet())
				{
					Input.AlignmentContribution = 8;
				}
				if (ContributionMask == Fixture.RequiredContributionMask)
				{
					++ExpectedMaskSuccesses;
					FinalizeValidFixtureHashes(Schema);
					TArray<uint8> Payload;
					ASSERT_THAT(IsTrue(
						FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
							Schema, Payload).IsSuccess()), Fixture.Context);
				}
				else
				{
					++ExpectedMaskFailures;
					const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
						EAngelscriptCacheValidationError::InvalidPresence,
						EAngelscriptCacheValidationStage::LocalSemantic,
						EAngelscriptCacheTypeSchemaTestField::LayoutInput,
						TEXT("LayoutInput boundary/alignment contribution presence matrix"),
						Fixture.InputIndex)));
				}
			}

			FAngelscriptCachedTypeSchema InvalidTarget = Fixture.Schema;
			InvalidTarget.LayoutInputs[Fixture.InputIndex].Target.StableKey = {};
			FMalformedDecodeOutcome TargetOutcome =
				DecodePhysicalOnlyFixture(InvalidTarget);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, TargetOutcome,
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::LayoutInput,
				TEXT("direct LayoutInput target zero StableKey"), Fixture.InputIndex)));
			++TargetChecks;

			InvalidTarget = Fixture.Schema;
			InvalidTarget.LayoutInputs[Fixture.InputIndex].Target.Kind =
				EAngelscriptCacheReferenceKind::Invalid;
			TargetOutcome = DecodePhysicalOnlyFixture(InvalidTarget);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, TargetOutcome,
				EAngelscriptCacheValidationError::UnknownEnumValue,
				EAngelscriptCacheValidationStage::PayloadDecode,
				EAngelscriptCacheTypeSchemaTestField::LayoutInput,
				TEXT("direct LayoutInput target invalid ReferenceKind"),
				Fixture.InputIndex, 0)));
			++TargetChecks;

			InvalidTarget = Fixture.Schema;
			InvalidTarget.LayoutInputs[Fixture.InputIndex].Target.Kind =
				EAngelscriptCacheReferenceKind::ScriptFunction;
			TargetOutcome = DecodePhysicalOnlyFixture(InvalidTarget);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, TargetOutcome,
				EAngelscriptCacheValidationError::WrongReferenceKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::LayoutInput,
				TEXT("direct LayoutInput target wrong ReferenceKind"), Fixture.InputIndex)));
			++TargetChecks;

			InvalidTarget = Fixture.Schema;
			InvalidTarget.LayoutInputs[Fixture.InputIndex].Target.ExpectedAbi = {};
			TargetOutcome = DecodePhysicalOnlyFixture(InvalidTarget);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, TargetOutcome,
				EAngelscriptCacheValidationError::MissingExpectedAbi,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::LayoutInput,
				TEXT("direct LayoutInput target requires ExpectedAbi"), Fixture.InputIndex)));
			++TargetChecks;

			FAngelscriptCachedTypeSchema Missing = Fixture.Schema;
			const EAngelscriptCachedTypeLayoutInputKind MissingKind =
				Missing.LayoutInputs[Fixture.InputIndex].InputKind;
			Missing.LayoutInputs.RemoveAt(Fixture.InputIndex);
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Missing);
			if (MissingKind == EAngelscriptCachedTypeLayoutInputKind::BaseType)
			{
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
					EAngelscriptCacheValidationError::InvalidPresence,
					EAngelscriptCacheValidationStage::LocalSemantic,
					EAngelscriptCacheTypeSchemaTestField::Relations,
					TEXT("missing BaseType uses the requiring Base Relation row"), 0)));
			}
			else
			{
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
					EAngelscriptCacheValidationError::InvalidPresence,
					EAngelscriptCacheValidationStage::LocalSemantic,
					EAngelscriptCacheTypeSchemaTestField::Reflection,
					TEXT("missing form-required CodeRoot/StructHeader uses Reflection"))));
			}
			++MissingChecks;
		}
		ASSERT_THAT(AreEqual(16, ContributionMaskCells));
		ASSERT_THAT(AreEqual(4, ExpectedMaskSuccesses));
		ASSERT_THAT(AreEqual(12, ExpectedMaskFailures));
		ASSERT_THAT(AreEqual(16, TargetChecks));
		ASSERT_THAT(AreEqual(4, MissingChecks));
		TestRunner->AddInfo(FString::Printf(TEXT(
			"[CacheV2][TypeSchema][LayoutInputs][DecoderMatrix] complete "
			"mask-cells=%d expected-success=%d expected-failure=%d "
			"target-checks=%d missing-checks=%d total=%d"),
			ContributionMaskCells, ExpectedMaskSuccesses, ExpectedMaskFailures,
			TargetChecks, MissingChecks,
			ContributionMaskCells + TargetChecks + MissingChecks));
	}

	TEST_METHOD(LayoutInputFormPairingAndSingletonFailuresUseExactRows)
	{
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][LayoutInputs][DecoderClosure] begin "
			"pairing=6 form=1 order=1 duplicate=1 conflict=1 stale-hash=1 "
			"expected-total=11"));

		const auto MakePlainClassWithBase = []()
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			FAngelscriptCachedTypeRelation Base;
			Base.RelationKind = EAngelscriptCachedTypeRelationKind::Base;
			Base.Target = MakeReference(
				EAngelscriptCacheReferenceKind::ScriptType, 0x70, 0x71);
			Schema.Relations.Add(Base);
			FAngelscriptCachedTypeLayoutInput Input;
			Input.InputKind = EAngelscriptCachedTypeLayoutInputKind::BaseType;
			Input.Target = Base.Target;
			Input.BoundaryContribution = 0;
			Input.AlignmentContribution = 8;
			Schema.LayoutInputs.Add(Input);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Inheritance, Base.Target));
			FinalizeValidFixtureHashes(Schema);
			return Schema;
		};
		const auto FindRelationIndex = [](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedTypeRelationKind Kind) -> int32
		{
			for (int32 Index = 0; Index < Schema.Relations.Num(); ++Index)
			{
				if (Schema.Relations[Index].RelationKind == Kind)
				{
					return Index;
				}
			}
			checkNoEntry();
			return INDEX_NONE;
		};
		const auto ExpectLayoutRow = [this](
			const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCacheValidationError Error,
			const TCHAR* Context,
			const int32 InputIndex)
		{
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			return ExpectExactFailureAndReset(*TestRunner, Outcome, Error,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::LayoutInput,
				Context, InputIndex);
		};

		int32 PairingChecks = 0;
		FAngelscriptCachedTypeSchema Invalid = MakePlainClassWithBase();
		Invalid.LayoutInputs[0].Target.StableKey = MakeHash(0x72);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("BaseType stable key differs from Base relation"), 0)));
		++PairingChecks;

		Invalid = MakePlainClassWithBase();
		Invalid.LayoutInputs[0].Target.ExpectedAbi = MakeHash(0x73);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("BaseType ABI differs from Base relation"), 0)));
		++PairingChecks;

		Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.LayoutInputs[0].Target.StableKey = MakeHash(0x74);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("CodeRoot stable key differs from Shadow/Code relations"), 0)));
		++PairingChecks;

		Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.LayoutInputs[0].Target.ExpectedAbi = MakeHash(0x75);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("CodeRoot ABI differs from Shadow/Code relations"), 0)));
		++PairingChecks;

		Invalid = MakeOrdinaryUClassSchema(false);
		{
			FAngelscriptCachedTypeRelation& Code = Invalid.Relations[
				FindRelationIndex(Invalid, EAngelscriptCachedTypeRelationKind::CodeSuper)];
			Code.Target.StableKey = MakeHash(0x76);
			Invalid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, Code.Target));
		}
		Invalid.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("CodeSuper stable key differs from LayoutInput/ShadowSuper"), 0)));
		++PairingChecks;

		Invalid = MakeOrdinaryUClassSchema(false);
		{
			FAngelscriptCachedTypeRelation& Shadow = Invalid.Relations[
				FindRelationIndex(Invalid, EAngelscriptCachedTypeRelationKind::ShadowSuper)];
			Shadow.Target.StableKey = MakeHash(0x77);
			Invalid.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, Shadow.Target));
		}
		Invalid.Dependencies.Sort([](const auto& A, const auto& B)
		{
			return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
		});
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("ShadowSuper stable key differs from LayoutInput/CodeSuper"), 0)));
		++PairingChecks;

		int32 FormChecks = 0;
		Invalid = MakeEnumSchema();
		Invalid.LayoutInputs.Add(MakePlainClassWithBase().LayoutInputs[0]);
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Enum forbids every LayoutInput role"), 0)));
		++FormChecks;

		int32 OrderChecks = 0;
		Invalid = MakeOrdinaryUClassSchema(true);
		Algo::Reverse(Invalid.LayoutInputs);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::NonCanonicalOrder,
			TEXT("CodeRoot before BaseType is noncanonical at the proving first row"), 0)));
		++OrderChecks;

		int32 DuplicateChecks = 0;
		Invalid = MakeOrdinaryUClassSchema(false);
		FAngelscriptCachedTypeLayoutInput DuplicateInput = Invalid.LayoutInputs[0];
		Invalid.LayoutInputs.Add(MoveTemp(DuplicateInput));
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::DuplicateKey,
			TEXT("identical LayoutInput singleton repeats at the second row"), 1)));
		++DuplicateChecks;

		int32 ConflictChecks = 0;
		Invalid = MakeOrdinaryUClassSchema(false);
		FAngelscriptCachedTypeLayoutInput Conflicting = Invalid.LayoutInputs[0];
		Conflicting.Target.StableKey = MakeHash(0x78);
		Invalid.LayoutInputs.Add(MoveTemp(Conflicting));
		FinalizeValidFixtureHashes(Invalid);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::ConflictingKey,
			TEXT("different LayoutInput singleton coordinate conflicts at second row"), 1)));
		++ConflictChecks;

		int32 HashChecks = 0;
		Invalid = MakeOrdinaryUClassSchema(false);
		Invalid.LayoutInputs[0].LayoutInputHash = MakeHash(0x79);
		ASSERT_THAT(IsTrue(ExpectLayoutRow(Invalid,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			TEXT("stale LayoutInputHash selects its row"), 0)));
		++HashChecks;

		ASSERT_THAT(AreEqual(6, PairingChecks));
		ASSERT_THAT(AreEqual(1, FormChecks));
		ASSERT_THAT(AreEqual(1, OrderChecks));
		ASSERT_THAT(AreEqual(1, DuplicateChecks));
		ASSERT_THAT(AreEqual(1, ConflictChecks));
		ASSERT_THAT(AreEqual(1, HashChecks));
		TestRunner->AddInfo(FString::Printf(TEXT(
			"[CacheV2][TypeSchema][LayoutInputs][DecoderClosure] complete "
			"pairing=%d form=%d order=%d duplicate=%d conflict=%d stale-hash=%d total=%d"),
			PairingChecks, FormChecks, OrderChecks, DuplicateChecks, ConflictChecks,
			HashChecks, PairingChecks + FormChecks + OrderChecks + DuplicateChecks
				+ ConflictChecks + HashChecks));
	}

	TEST_METHOD(PropertyStorageWitnessAndImmutableLayoutReplayAreExact)
	{
		static_assert(static_cast<uint8>(EAngelscriptCachedPropertyStorageKind::InlineValue) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedPropertyStorageKind::ObjectHandle) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedMemberAccess::Public) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedMemberAccess::Protected) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedMemberAccess::Private) == 3);

		const FAngelscriptCachedTypeSchema AtZero = MakeCompleteDelegateSchema();
		FAngelscriptCachedTypeSchema AtFour = AtZero;
		AtFour.OrderedProperties[0].SemanticByteOffset = 4;
		RehashSelfConsistentWrongPropertyOffset(AtFour, 0);
		TArray<uint8> Bytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			AtZero, Bytes).IsSuccess()));
		FMalformedDecodeOutcome AtFourOutcome = DecodePhysicalOnlyFixture(AtFour);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, AtFourOutcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("aligned-looking gap differs from exact replay cursor"), 0)));
		ASSERT_THAT(IsFalse(AtZero.OrderedProperties[0].PropertyLayoutFingerprint
			== AtFour.OrderedProperties[0].PropertyLayoutFingerprint));
		ASSERT_THAT(IsFalse(AtZero.Layout.TypeLayoutHash == AtFour.Layout.TypeLayoutHash),
			TEXT("an otherwise self-consistent wrong offset still contributes to both hashes"));

		FAngelscriptCachedTypeSchema Invalid = AtZero;
		Invalid.OrderedProperties[0].SemanticStorageSize = 0;
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("zero property storage size"), 0)));
		Invalid = AtZero;
		Invalid.OrderedProperties[0].SemanticStorageAlignment = 3;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("non-power-of-two property storage alignment"), 0)));
		Invalid = AtZero;
		Invalid.OrderedProperties[0].SemanticByteOffset = 3;
		RehashSelfConsistentWrongPropertyOffset(Invalid, 0);
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("unaligned property byte offset"), 0)));
		Invalid = AtZero;
		Invalid.Layout.SemanticSize = 4;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Layout,
			TEXT("terminal size omits tail alignment"))));
		Invalid = AtZero;
		Invalid.Layout.BasePropertyBoundary = 4;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Layout,
			TEXT("BasePropertyBoundary conflicts with property replay"))));
	}

	TEST_METHOD(PropertyOrdinalReplayPaddingRangeAndForbiddenFormsAreClosed)
	{
		const auto AssertLocal = [&](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCacheValidationError Error,
			const EAngelscriptCacheTypeSchemaTestField Field,
			const TCHAR* Context,
			const int32 Primary = INDEX_NONE)
		{
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				Error, EAngelscriptCacheValidationStage::LocalSemantic,
				Field, Context, Primary)));
		};

		const FAngelscriptCachedTypeSchema Valid = MakeThreePropertyLayoutSchema();
		TArray<uint8> ValidPayload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Valid, ValidPayload).IsSuccess()),
			TEXT("three-property control freezes internal padding and terminal tail padding"));

		for (uint32 Position = 0; Position < 3; ++Position)
		{
			FAngelscriptCachedTypeSchema Gap = Valid;
			for (uint32 Index = Position; Index < 3; ++Index)
			{
				++Gap.OrderedProperties[Index].LayoutOrdinal;
			}
			ASSERT_THAT(IsTrue(
				RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
					Gap).IsSuccess()));
			AssertLocal(Gap, EAngelscriptCacheValidationError::OrdinalGap,
				EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
				TEXT("independent property ordinal gap"), Position);

			if (Position > 0)
			{
				FAngelscriptCachedTypeSchema Duplicate = Valid;
				Duplicate.OrderedProperties[Position].LayoutOrdinal = Position - 1;
				ASSERT_THAT(IsTrue(
					RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
						Duplicate).IsSuccess()));
				AssertLocal(Duplicate,
					EAngelscriptCacheValidationError::DuplicateOrdinal,
					EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
					TEXT("isolated property duplicate ordinal"), Position);
			}

			FAngelscriptCachedTypeSchema Reordered = Valid;
			const uint32 Peer = Position == 2 ? 1 : Position + 1;
			Swap(Reordered.OrderedProperties[Position], Reordered.OrderedProperties[Peer]);
			ASSERT_THAT(IsTrue(
				RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
					Reordered).IsSuccess()));
			AssertLocal(Reordered, EAngelscriptCacheValidationError::NonCanonicalOrder,
				EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
				TEXT("independent property complete-row reorder"),
				FMath::Min(Position, Peer));
		}

		FAngelscriptCachedTypeSchema Invalid = Valid;
		Invalid.Layout.BasePropertyBoundary = 1;
		AssertLocal(Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::Layout,
			TEXT("property offset below BasePropertyBoundary"));
		Invalid = Valid;
		Invalid.OrderedProperties[1].SemanticByteOffset = 2;
		RehashSelfConsistentWrongPropertyOffset(Invalid, 1);
		AssertLocal(Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("property offset is not aligned"), 1);
		Invalid = Valid;
		Invalid.OrderedProperties[1].SemanticByteOffset = 0;
		RehashSelfConsistentWrongPropertyOffset(Invalid, 1);
		AssertLocal(Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("property overlaps predecessor"), 1);
		Invalid = Valid;
		Invalid.OrderedProperties[2].SemanticByteOffset = 10;
		RehashSelfConsistentWrongPropertyOffset(Invalid, 2);
		AssertLocal(Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("aligned property offset differs from checked replay cursor"), 2);

		Invalid = Valid;
		Invalid.Layout.SemanticSize = 10;
		AssertLocal(Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::Layout,
			TEXT("terminal SemanticSize omits mandatory tail padding"));
		Invalid = Valid;
		Invalid.Layout.SemanticSize = 24;
		AssertLocal(Invalid, EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::Layout,
			TEXT("terminal SemanticSize includes surplus tail padding"));

		for (const uint32 Alignment : {uint32(0), uint32(3), uint32(2), uint32(16)})
		{
			Invalid = Valid;
			Invalid.Layout.SemanticAlignment = Alignment;
			AssertLocal(Invalid,
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				EAngelscriptCacheTypeSchemaTestField::Layout,
				TEXT("aggregate alignment must be nonzero power-of-two exact maximum >= 8"));
		}

		Invalid = Valid;
		Invalid.OrderedProperties[2].SemanticByteOffset = MAX_uint32 - 1;
		Invalid.OrderedProperties[2].SemanticStorageSize = 4;
		AssertLocal(Invalid, EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("checked property end overflow"), 2);
		Invalid = Valid;
		Invalid.Layout.SemanticSize = uint64(MAX_int32) + 1;
		AssertLocal(Invalid, EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheTypeSchemaTestField::Layout,
			TEXT("maintained VM layout range exceeds INT32_MAX"));
		Invalid = Valid;
		Invalid.OrderedProperties[0].SemanticStorageSize = uint32(MAX_int32) + 1u;
		AssertLocal(Invalid, EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("property storage size exceeds INT32_MAX"), 0);
		Invalid = Valid;
		Invalid.OrderedProperties[0].SemanticStorageAlignment = uint32(MAX_int32) + 1u;
		AssertLocal(Invalid, EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("property storage alignment exceeds INT32_MAX"), 0);
		Invalid = Valid;
		Invalid.Layout.BasePropertyBoundary = 24;
		AssertLocal(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::Layout,
			TEXT("BasePropertyBoundary cannot exceed SemanticSize"));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		Invalid.OrderedProperties.Add(Valid.OrderedProperties[0]);
		Invalid.OrderedProperties[0].SemanticByteOffset = 0;
		Invalid.OrderedProperties[0].SemanticStorageSize = MAX_int32;
		Invalid.OrderedProperties[0].SemanticStorageAlignment = 1;
		Invalid.Layout.SemanticAlignment = 8;
		Invalid.Layout.SemanticSize = uint64(MAX_int32) + 1;
		FinalizeValidFixtureHashes(Invalid);
		AssertLocal(Invalid, EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheTypeSchemaTestField::Layout,
			TEXT("terminal AlignUp crosses maintained INT32 layout range"));

		const FAngelscriptCachedTypeSchema NonZeroBase =
			MakeNonZeroBaseBoundaryUClassSchema();
		TArray<uint8> NonZeroBasePayload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			NonZeroBase, NonZeroBasePayload).IsSuccess()));
		FAngelscriptCacheReadBudget NonZeroBaseBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> NonZeroBaseOutput;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(NonZeroBasePayload,
			FAngelscriptCacheReadLimits{}, NonZeroBaseBudget,
			NonZeroBaseOutput).IsSuccess()),
			TEXT("the dedicated nonzero-base fixture is locally valid before mutation"));
		ASSERT_THAT(IsTrue(NonZeroBaseOutput.IsSet()));

		Invalid = NonZeroBase;
		Invalid.OrderedProperties.Add(Valid.OrderedProperties[0]);
		Invalid.OrderedProperties[0].SemanticByteOffset = 0;
		Invalid.OrderedProperties[0].LayoutOrdinal = 0;
		Invalid.Layout.SemanticSize = 24;
		FinalizeValidFixtureHashes(Invalid);
		AssertLocal(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("inherited storage cannot be inserted into derived local property list"), 0);

		Invalid = Valid;
		Invalid.OrderedProperties[0].PropertyKey = {};
		AssertLocal(Invalid, EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("zero structural PropertyKey"), 0);
		Invalid = Valid;
		Invalid.OrderedProperties[0].CanonicalName.Reset();
		AssertLocal(Invalid, EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("empty structural property name"), 0);

		for (const EAngelscriptCachedTypeKind ForbiddenKind : {
			EAngelscriptCachedTypeKind::Interface,
			EAngelscriptCachedTypeKind::Enum,
			EAngelscriptCachedTypeKind::Typedef,
			EAngelscriptCachedTypeKind::Funcdef})
		{
			Invalid = MakeMinimalSchema(ForbiddenKind);
			Invalid.OrderedProperties.Add(Valid.OrderedProperties[0]);
			FinalizeValidFixtureHashes(Invalid);
			AssertLocal(Invalid, EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
				TEXT("property forbidden on this TypeKind/form"), 0);
		}
	}

	TEST_METHOD(PropertySemanticFlagsAndReplicationAreExhaustivePerOwnerForm)
	{
		const FAngelscriptDecodedCacheRecordHandle Sentinel = MakeSentinelRecord();
		const EPropertyOwnerFormForTests Owners[] = {
			EPropertyOwnerFormForTests::PlainClass,
			EPropertyOwnerFormForTests::OrdinaryUClass,
			EPropertyOwnerFormForTests::PlainStruct,
			EPropertyOwnerFormForTests::UStruct,
			EPropertyOwnerFormForTests::Delegate,
		};
		for (const EPropertyOwnerFormForTests Owner : Owners)
		{
			struct FPayloadTemplate
			{
				FAngelscriptCachedTypeSchema Schema;
				TArray<uint8> Payload;
				FAngelscriptCacheTypeSchemaTestWireTrace Trace;
			};
			FPayloadTemplate Templates[4];
			for (uint32 TemplateIndex = 0; TemplateIndex < UE_ARRAY_COUNT(Templates);
				++TemplateIndex)
			{
				FPayloadTemplate& Template = Templates[TemplateIndex];
				Template.Schema = MakePropertyOwnerFixture(Owner);
				FAngelscriptCachedPropertySchema& Property =
					Template.Schema.OrderedProperties[0];
				Property.Metadata.Reset();
				if ((TemplateIndex & 2) != 0)
				{
					Property.Metadata.Add({TEXT("ReplicatedUsing"), TEXT("OnRep_Value")});
				}
				Property.ReplicationCondition = (TemplateIndex & 1) != 0
					? EAngelscriptCachedReplicationCondition::InitialOnly
					: EAngelscriptCachedReplicationCondition::None;
				FinalizeValidFixtureHashes(Template.Schema);
				ASSERT_THAT(IsTrue(
					FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
						Template.Schema, Template.Payload, Template.Trace).IsSuccess()));
			}
			for (uint32 Mask = 0; Mask <= 0x7ffff; ++Mask)
			{
				const bool bInitialCondition = (Mask & 0x00400u) != 0
					&& (Mask & 0x00800u) == 0;
				const uint32 TemplateIndex = ((Mask & 0x04000u) != 0 ? 2u : 0u)
					| (bInitialCondition ? 1u : 0u);
				FPayloadTemplate& Template = Templates[TemplateIndex];
				const EAngelscriptCacheValidationError Expected =
					ExpectedPropertyMaskError(Owner, Mask,
						Template.Schema.OrderedProperties[0].ReplicationCondition);
				PatchRawUInt32(Template.Payload, Template.Trace,
					EAngelscriptCacheTypeSchemaTestField::PropertySemanticFlags, Mask, 0);
				if (Expected == EAngelscriptCacheValidationError::None
					|| Expected == EAngelscriptCacheValidationError::InvalidPresence)
				{
					FAngelscriptCachedTypeSchema Schema = Template.Schema;
					Schema.OrderedProperties[0].PropertySemanticFlags = Mask;
					FinalizeValidFixtureHashes(Schema);
					PatchHashBytes(Template.Payload, Template.Trace,
						EAngelscriptCacheTypeSchemaTestField::PropertyLayoutFingerprint,
						Schema.OrderedProperties[0].PropertyLayoutFingerprint, 0);
					PatchHashBytes(Template.Payload, Template.Trace,
						EAngelscriptCacheTypeSchemaTestField::TypeLayoutHash,
						Schema.Layout.TypeLayoutHash);
				}
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Sentinel;
				const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
					Template.Payload, Limits, Budget, Output);
				ASSERT_THAT(AreEqual(Expected == EAngelscriptCacheValidationError::None,
					Output.IsSet()));
				if (Expected == EAngelscriptCacheValidationError::None)
				{
					ASSERT_THAT(IsTrue(Result.IsSuccess()));
				}
				if (Expected != EAngelscriptCacheValidationError::None)
				{
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
						Result, Output, Expected,
						EAngelscriptCacheValidationStage::LocalSemantic,
						RequireIndependentSpanOffsetForTests(Template.Payload,
							EAngelscriptCacheTypeSchemaTestField::OrderedProperty, 0),
						TEXT("property semantic mask exhaustive decoder negative"))));
				}
			}
		}

		for (const EPropertyOwnerFormForTests Owner : Owners)
		{
			FAngelscriptCachedTypeSchema Schema = MakePropertyOwnerFixture(Owner);
			Schema.OrderedProperties[0].PropertySemanticFlags = 0x00401u;
			Schema.OrderedProperties[0].ReplicationCondition =
				EAngelscriptCachedReplicationCondition::InitialOnly;
			FinalizeValidFixtureHashes(Schema);
			FAngelscriptCacheTypeSchemaTestWireTrace Trace;
			TArray<uint8> Payload;
			ASSERT_THAT(IsTrue(
				FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
					Schema, Payload, Trace).IsSuccess()));
			for (uint32 RawCondition = 0; RawCondition <= 0xff; ++RawCondition)
			{
				PatchRawByte(Payload, Trace,
					EAngelscriptCacheTypeSchemaTestField::PropertyReplicationCondition,
					static_cast<uint8>(RawCondition), 0);
				FAngelscriptCacheReadLimits Limits;
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output = Sentinel;
				const bool bKnown = RawCondition <= 16;
				const EAngelscriptCachedReplicationCondition Condition =
					static_cast<EAngelscriptCachedReplicationCondition>(RawCondition);
				const EAngelscriptCacheValidationError Expected = !bKnown
					? EAngelscriptCacheValidationError::UnknownEnumValue
					: ExpectedPropertyMaskError(Owner, 0x00401u, Condition);
				FAngelscriptCachedTypeSchema Rehashed = Schema;
				Rehashed.OrderedProperties[0].ReplicationCondition = Condition;
				FinalizeValidFixtureHashes(Rehashed);
				PatchHashBytes(Payload, Trace,
					EAngelscriptCacheTypeSchemaTestField::PropertyLayoutFingerprint,
					Rehashed.OrderedProperties[0].PropertyLayoutFingerprint, 0);
				PatchHashBytes(Payload, Trace,
					EAngelscriptCacheTypeSchemaTestField::TypeLayoutHash,
					Rehashed.Layout.TypeLayoutHash);
				const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
					Payload, Limits, Budget, Output);
				ASSERT_THAT(AreEqual(Expected == EAngelscriptCacheValidationError::None,
					Output.IsSet()));
				if (Expected == EAngelscriptCacheValidationError::None)
				{
					ASSERT_THAT(IsTrue(Result.IsSuccess()));
				}
				if (Expected != EAngelscriptCacheValidationError::None)
				{
					const EAngelscriptCacheValidationStage ExpectedStage = bKnown
						? EAngelscriptCacheValidationStage::LocalSemantic
						: EAngelscriptCacheValidationStage::PayloadDecode;
					const EAngelscriptCacheTypeSchemaTestField ExpectedField = bKnown
						? EAngelscriptCacheTypeSchemaTestField::OrderedProperty
						: EAngelscriptCacheTypeSchemaTestField::PropertyReplicationCondition;
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
						Result, Output, Expected, ExpectedStage,
						RequireIndependentSpanOffsetForTests(Payload, ExpectedField, 0),
						TEXT("replication condition exhaustive decoder negative"))));
				}
			}
		}
	}

	TEST_METHOD(PropertyStorageKindDataTypeAndQualifierMatrixIsExhaustive)
	{
		const FAngelscriptCacheV1StorageLayout HandleLayout =
			FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants()
				.GetObjectHandleStorageLayout();
		for (uint8 RawStorage = 1; RawStorage <= 2; ++RawStorage)
		{
			const EAngelscriptCachedPropertyStorageKind StorageKind =
				static_cast<EAngelscriptCachedPropertyStorageKind>(RawStorage);
			for (uint8 RawTypeKind = 1; RawTypeKind <= 4; ++RawTypeKind)
			{
				for (uint32 Qualifiers = 0; Qualifiers <= 0x3f; ++Qualifiers)
				{
					FAngelscriptCachedTypeSchema Schema = MakePropertyOwnerFixture(
						EPropertyOwnerFormForTests::PlainStruct);
					FAngelscriptCachedPropertySchema& Property = Schema.OrderedProperties[0];
					Property.StorageKind = StorageKind;
					Property.Type = {};
					Property.Type.Kind = static_cast<EAngelscriptCachedDataTypeKind>(RawTypeKind);
					Property.Type.QualifierFlags = Qualifiers;
					if (Property.Type.Kind == EAngelscriptCachedDataTypeKind::Primitive)
					{
						Property.Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
					}
					if (Property.Type.Kind == EAngelscriptCachedDataTypeKind::ScriptType)
					{
						Property.Type.TypeReference = MakeReference(
							EAngelscriptCacheReferenceKind::ScriptType, 0xd0, 0xd1);
					}
					if (Property.Type.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType)
					{
						Property.Type.TypeReference = MakeReference(
							EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xd2, 0xd3);
					}
					if (Property.Type.TypeReference.IsSet())
					{
						const EAngelscriptCacheSemanticDependencyKind DependencyKind =
							StorageKind == EAngelscriptCachedPropertyStorageKind::ObjectHandle
								? Property.Type.Kind == EAngelscriptCachedDataTypeKind::ScriptType
									? EAngelscriptCacheSemanticDependencyKind::Declaration
									: EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi
								: EAngelscriptCacheSemanticDependencyKind::ValueLayout;
						Schema.Dependencies.AddUnique(MakeDependency(
							DependencyKind,
							Property.Type.TypeReference.GetValue()));
						Schema.Dependencies.Sort([](
							const FAngelscriptCacheSemanticDependency& A,
							const FAngelscriptCacheSemanticDependency& B)
						{
							return FAngelscriptCacheTypeSchemaArchive::CompareDependencies(A, B) < 0;
						});
					}
					const bool bReference = (Qualifiers & 0x01u) != 0;
					const bool bObjectConst = (Qualifiers & 0x02u) != 0;
					const bool bObjectHandle = (Qualifiers & 0x04u) != 0;
					const bool bConstHandle = (Qualifiers & 0x08u) != 0;
					const bool bAuto = (Qualifiers & 0x10u) != 0;
					const bool bIfHandleThenConst = (Qualifiers & 0x20u) != 0;
					const bool bObjectType = RawTypeKind == 2 || RawTypeKind == 3;
					const bool bExpected = RawTypeKind != 4 && !bReference && !bAuto
						&& (StorageKind == EAngelscriptCachedPropertyStorageKind::InlineValue
							? !bObjectHandle && !bConstHandle && !bIfHandleThenConst
								&& (bObjectType || !bObjectConst)
							: bObjectType && bObjectHandle);
					if (StorageKind == EAngelscriptCachedPropertyStorageKind::ObjectHandle)
					{
						Property.SemanticStorageSize = HandleLayout.SemanticStorageSize;
						Property.SemanticStorageAlignment =
							HandleLayout.SemanticStorageAlignment;
					}
					if (bExpected)
					{
						FinalizeValidFixtureHashes(Schema);
					}
					const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
					if (bExpected)
					{
						ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()));
					}
					ASSERT_THAT(AreEqual(bExpected, Outcome.Output.IsSet()));
					if (!bExpected)
					{
						ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
							EAngelscriptCacheValidationError::InvalidQualifierCombination,
							EAngelscriptCacheValidationStage::LocalSemantic,
							EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
							TEXT("StorageKind/DataType/qualifier matrix"), 0)));
					}
				}
			}
		}

		FAngelscriptCachedTypeSchema Unknown = MakePropertyOwnerFixture(
			EPropertyOwnerFormForTests::PlainStruct);
		Unknown.OrderedProperties[0].Type.QualifierFlags = 0x40;
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Unknown);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::UnknownFlags,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("CanonicalDataType unknown qualifier bit"), 0)));
		Unknown = MakePropertyOwnerFixture(EPropertyOwnerFormForTests::PlainStruct);
		Unknown.OrderedProperties[0].Type.Primitive = EAngelscriptCachedPrimitiveType::Void;
		Outcome = DecodePhysicalOnlyFixture(Unknown);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("Void property storage is not cacheable"), 0)));
	}
	TEST_METHOD(MethodVftBehaviorAndReflectedMemberSequencesStayIndependent)
	{
		static_assert(static_cast<uint8>(EAngelscriptCachedMethodSlotKind::LocalMethod) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedMethodSlotKind::VirtualDeclaration) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedMethodSlotKind::VirtualOverride) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedMethodSlotKind::Inherited) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::Construct) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::ListConstruct) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::Destruct) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::Factory) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::ListFactory) == 5);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::AddRef) == 6);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::Release) == 7);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::GetWeakRefFlag) == 8);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::TemplateCallback) == 9);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::GetRefCount) == 10);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::SetGcFlag) == 11);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::GetGcFlag) == 12);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::EnumRefs) == 13);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::ReleaseRefs) == 14);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::Copy) == 15);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::CopyConstruct) == 16);
		static_assert(static_cast<uint8>(EAngelscriptCachedBehaviorKind::CopyFactory) == 17);

		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		for (uint32 Ordinal = 0; Ordinal < 2; ++Ordinal)
		{
			FAngelscriptCachedMethodEntry Method;
			Method.EntryKind = EAngelscriptCachedMethodSlotKind::LocalMethod;
			Method.MethodOrdinal = Ordinal;
			Method.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(
				static_cast<uint8>(0x90 + Ordinal * 2))};
			Method.DeclaringOwner = Schema.TypeKey;
			Method.ExpectedDeclarationAbi = MakeHash(
				static_cast<uint8>(0x91 + Ordinal * 2));
			Schema.OrderedMethods.Add(Method);
			Schema.Dependencies.Add(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					static_cast<uint8>(0x90 + Ordinal * 2),
					static_cast<uint8>(0x91 + Ordinal * 2))));
		}
		FAngelscriptCachedVirtualFunctionSlot Vft;
		Vft.SlotKind = EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
		Vft.VftOrdinal = 0;
		Vft.FunctionKey = Schema.OrderedMethods[1].FunctionKey;
		Vft.DeclaringOwner = Schema.TypeKey;
		Vft.ImplementingOwner = Schema.TypeKey;
		Vft.ExpectedDeclarationAbi = Schema.OrderedMethods[1].ExpectedDeclarationAbi;
		Schema.VirtualFunctionTable.Add(Vft);
		FinalizeValidFixtureHashes(Schema);
		TArray<uint8> Bytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Bytes).IsSuccess()));
		FAngelscriptCacheReadLimits MethodLimits;
		FAngelscriptCacheReadBudget MethodBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> MethodRecord;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Bytes, MethodLimits, MethodBudget, MethodRecord).IsSuccess()));
		ASSERT_THAT(IsTrue(MethodRecord.IsSet()));

		FAngelscriptCachedTypeSchema ReorderedMethods = Schema;
		Swap(ReorderedMethods.OrderedMethods[0].FunctionKey,
			ReorderedMethods.OrderedMethods[1].FunctionKey);
		Swap(ReorderedMethods.OrderedMethods[0].ExpectedDeclarationAbi,
			ReorderedMethods.OrderedMethods[1].ExpectedDeclarationAbi);
		FinalizeValidFixtureHashes(ReorderedMethods);
		ASSERT_THAT(IsFalse(Schema.Layout.TypeLayoutHash
			== ReorderedMethods.Layout.TypeLayoutHash));
		ASSERT_THAT(IsTrue(Schema.VirtualFunctionTable[0].FunctionKey
			== ReorderedMethods.VirtualFunctionTable[0].FunctionKey));

		FAngelscriptCachedTypeSchema Invalid = Schema;
		Invalid.OrderedMethods[0].EntryKind =
			EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedMethod,
			TEXT("OrderedMethods forbids virtual-declaration kind"), 0)));
		Invalid = Schema;
		Invalid.VirtualFunctionTable[0].SlotKind =
			EAngelscriptCachedMethodSlotKind::LocalMethod;
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable,
			TEXT("VFT forbids local-method kind"), 0)));
		Invalid = Schema;
		Invalid.OrderedMethods[1].MethodOrdinal = 2;
		ASSERT_THAT(IsTrue(
			RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
				Invalid).IsSuccess()));
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::OrdinalGap,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedMethod,
			TEXT("method ordinal gap"), 1)));

		Invalid = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		FAngelscriptCachedBehaviorSlot EngineCopy;
		EngineCopy.BehaviorKind = EAngelscriptCachedBehaviorKind::Copy;
		EngineCopy.Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xa0, 0xa1);
		EngineCopy.DeclaringOwner = Invalid.TypeKey;
		Invalid.OrderedBehaviorSlots.Add(EngineCopy);
		Invalid.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi, EngineCopy.Target));
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::
				BehaviorDeclaringOwnerOptionalTag,
			TEXT("environment behavior targets must not carry a script declaring owner"),
			0, 1)));
	}

	TEST_METHOD(NormalProducerRejectsMethodAndVftSequencesIndependently)
	{
		using FError = EAngelscriptCacheValidationError;
		constexpr uint8 LocalMask = 1u << 0;
		constexpr uint8 VirtualDeclarationMask = 1u << 1;
		constexpr uint8 VirtualOverrideMask = 1u << 2;
		constexpr uint8 InheritedMask = 1u << 3;
		const FAngelscriptStableTypeKey OtherOwner{MakeHash(0xe0)};

		const auto AddMethod = [](FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedMethodSlotKind Kind,
			const uint32 Ordinal,
			const uint8 KeyFill,
			const uint8 AbiFill,
			const FAngelscriptStableTypeKey Owner)
		{
			FAngelscriptCachedMethodEntry Method;
			Method.EntryKind = Kind;
			Method.MethodOrdinal = Ordinal;
			Method.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(KeyFill)};
			Method.DeclaringOwner = Owner;
			Method.ExpectedDeclarationAbi = MakeHash(AbiFill);
			Schema.OrderedMethods.Add(Method);
		};
		const auto AddVft = [](FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedMethodSlotKind Kind,
			const uint32 Ordinal,
			const uint8 KeyFill,
			const uint8 AbiFill,
			const FAngelscriptStableTypeKey DeclaringOwner,
			const FAngelscriptStableTypeKey ImplementingOwner)
		{
			FAngelscriptCachedVirtualFunctionSlot Slot;
			Slot.SlotKind = Kind;
			Slot.VftOrdinal = Ordinal;
			Slot.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(KeyFill)};
			Slot.DeclaringOwner = DeclaringOwner;
			Slot.ImplementingOwner = ImplementingOwner;
			Slot.ExpectedDeclarationAbi = MakeHash(AbiFill);
			Schema.VirtualFunctionTable.Add(Slot);
		};
		const auto CanonicalMethodOwner = [&](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedMethodSlotKind Kind)
		{
			return Kind == EAngelscriptCachedMethodSlotKind::LocalMethod
				|| Kind == EAngelscriptCachedMethodSlotKind::VirtualDeclaration
				? Schema.TypeKey : OtherOwner;
		};
		const auto CanonicalVftDeclaringOwner = [&](
			const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedMethodSlotKind Kind)
		{
			return Kind == EAngelscriptCachedMethodSlotKind::LocalMethod
				|| Kind == EAngelscriptCachedMethodSlotKind::VirtualDeclaration
				? Schema.TypeKey : OtherOwner;
		};
		const auto CanonicalVftImplementingOwner = [&](
			const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedMethodSlotKind Kind)
		{
			return Kind == EAngelscriptCachedMethodSlotKind::Inherited
				? OtherOwner : Schema.TypeKey;
		};

		int32 TotalCalls = 0;
		int32 SuccessCalls = 0;
		int32 FailureCalls = 0;
		bool bPassed = true;
		const auto RunProducerCase = [&](FAngelscriptCachedTypeSchema Schema,
			const FError ExpectedError,
			const TCHAR* Context)
		{
			RebuildMethodAndVftDependenciesForTests(Schema);
			CanonicalizeDependenciesAndFinalizeForTests(Schema);
			++TotalCalls;
			if (ExpectedError == FError::None)
			{
				++SuccessCalls;
				bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(
					*TestRunner, Schema, Context);
			}
			else
			{
				++FailureCalls;
				bPassed &= ExpectExactProducerFailureAndInputUnchanged(
					*TestRunner, Schema, ExpectedError, Context);
			}
		};

		enum class EMethodVftForm : uint8
		{
			ClassNone,
			OrdinaryUClass,
			StaticsUClass,
			StructNone,
			UStruct,
			InterfaceNone,
			EnumNone,
			UEnum,
			Delegate,
			Typedef,
			Funcdef,
		};
		struct FMethodVftFormRow
		{
			EMethodVftForm Form;
			FAngelscriptCachedTypeSchema Schema;
			uint8 MethodSuccessMask;
			FError MethodFailure;
			uint8 VftSuccessMask;
			FError VftFailure;
		};
		const TArray<FMethodVftFormRow> FormRows = {
			{EMethodVftForm::ClassNone,
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Class),
				LocalMask | InheritedMask, FError::InvalidQualifierCombination,
				VirtualDeclarationMask | VirtualOverrideMask | InheritedMask,
				FError::InvalidQualifierCombination},
			{EMethodVftForm::OrdinaryUClass, MakeOrdinaryUClassSchema(false),
				LocalMask | InheritedMask, FError::InvalidQualifierCombination,
				VirtualDeclarationMask | VirtualOverrideMask | InheritedMask,
				FError::InvalidQualifierCombination},
			{EMethodVftForm::StaticsUClass, MakeStaticsUClassSchema(),
				0, FError::InvalidPresence, 0, FError::InvalidPresence},
			{EMethodVftForm::StructNone,
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct),
				LocalMask, FError::InvalidQualifierCombination,
				0, FError::InvalidPresence},
			{EMethodVftForm::UStruct, MakeReflectedUStructSchema(),
				LocalMask, FError::InvalidQualifierCombination,
				0, FError::InvalidPresence},
			{EMethodVftForm::InterfaceNone,
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface),
				LocalMask | InheritedMask, FError::InvalidQualifierCombination,
				VirtualDeclarationMask | VirtualOverrideMask | InheritedMask,
				FError::InvalidQualifierCombination},
			{EMethodVftForm::EnumNone,
				MakeReflectionFormSchema(EAngelscriptCachedTypeKind::Enum,
					EAngelscriptCachedReflectionKind::None),
				0, FError::InvalidPresence, 0, FError::InvalidPresence},
			{EMethodVftForm::UEnum, MakeEnumSchema(),
				0, FError::InvalidPresence, 0, FError::InvalidPresence},
			{EMethodVftForm::Delegate, MakeCompleteDelegateSchema(),
				LocalMask, FError::InvalidQualifierCombination,
				0, FError::InvalidPresence},
			{EMethodVftForm::Typedef,
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef),
				0, FError::InvalidPresence, 0, FError::InvalidPresence},
			{EMethodVftForm::Funcdef,
				MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef),
				0, FError::InvalidPresence, 0, FError::InvalidPresence},
		};

		int32 PartitionCalls = TotalCalls;
		int32 PartitionSuccess = SuccessCalls;
		int32 PartitionFailure = FailureCalls;
		for (const FMethodVftFormRow& FormRow : FormRows)
		{
			for (uint8 RawKind = 1; RawKind <= 4; ++RawKind)
			{
				const EAngelscriptCachedMethodSlotKind Kind =
					static_cast<EAngelscriptCachedMethodSlotKind>(RawKind);
				const uint8 KindMask = 1u << (RawKind - 1);
				for (const bool bVftRole : {false, true})
				{
					if ((!bVftRole
							&& FormRow.Form == EMethodVftForm::Delegate
							&& Kind == EAngelscriptCachedMethodSlotKind::VirtualDeclaration)
						|| (bVftRole
							&& FormRow.Form == EMethodVftForm::ClassNone
							&& Kind == EAngelscriptCachedMethodSlotKind::LocalMethod))
					{
						continue;
					}
					FAngelscriptCachedTypeSchema Schema = FormRow.Schema;
					ClearMethodAndVftOwnedStateForTests(Schema);
					if (bVftRole)
					{
						AddVft(Schema, Kind, 0, 0xe3, 0xe4,
							CanonicalVftDeclaringOwner(Schema, Kind),
							CanonicalVftImplementingOwner(Schema, Kind));
					}
					else
					{
						AddMethod(Schema, Kind, 0, 0xe1, 0xe2,
							CanonicalMethodOwner(Schema, Kind));
					}
					const uint8 SuccessMask = bVftRole
						? FormRow.VftSuccessMask : FormRow.MethodSuccessMask;
					const FString Context = FString::Printf(TEXT(
						"Method/VFT form-role literal row form=%u role=%s kind=%u"),
						static_cast<uint32>(FormRow.Form),
						bVftRole ? TEXT("vft") : TEXT("method"),
						static_cast<uint32>(RawKind));
					const bool bStaticsRoleMismatch =
						FormRow.Form == EMethodVftForm::StaticsUClass
						&& ((!bVftRole
								&& (Kind
									== EAngelscriptCachedMethodSlotKind::VirtualDeclaration
									|| Kind
										== EAngelscriptCachedMethodSlotKind::VirtualOverride))
							|| (bVftRole && Kind
								== EAngelscriptCachedMethodSlotKind::LocalMethod));
					RunProducerCase(MoveTemp(Schema),
						(SuccessMask & KindMask) != 0 ? FError::None
							: bStaticsRoleMismatch
								? FError::InvalidQualifierCombination
							: bVftRole ? FormRow.VftFailure : FormRow.MethodFailure,
						*Context);
				}
			}
		}
		ASSERT_THAT(AreEqual(86, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(18, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(68, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const uint8 RawKind : {uint8(0), uint8(5), uint8(255)})
		{
			FAngelscriptCachedTypeSchema MethodSchema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			AddMethod(MethodSchema,
				static_cast<EAngelscriptCachedMethodSlotKind>(RawKind),
				0, 0xe1, 0xe2, MethodSchema.TypeKey);
			RunProducerCase(MoveTemp(MethodSchema), FError::UnknownEnumValue,
				TEXT("raw MethodSlotKind in Method"));

			FAngelscriptCachedTypeSchema VftSchema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			AddVft(VftSchema,
				static_cast<EAngelscriptCachedMethodSlotKind>(RawKind),
				0, 0xe3, 0xe4, VftSchema.TypeKey, VftSchema.TypeKey);
			RunProducerCase(MoveTemp(VftSchema), FError::UnknownEnumValue,
				TEXT("raw MethodSlotKind in VFT"));
		}
		ASSERT_THAT(AreEqual(6, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(6, FailureCalls - PartitionFailure));

		const auto MakeOrdinalFixture = [&](const bool bVftRole)
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			for (uint32 Ordinal = 0; Ordinal < 3; ++Ordinal)
			{
				if (bVftRole)
				{
					AddVft(Schema,
						EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
						Ordinal, static_cast<uint8>(0xf0 + Ordinal),
						static_cast<uint8>(0xf4 + Ordinal),
						Schema.TypeKey, Schema.TypeKey);
				}
				else
				{
					AddMethod(Schema, EAngelscriptCachedMethodSlotKind::LocalMethod,
						Ordinal, static_cast<uint8>(0xf0 + Ordinal),
						static_cast<uint8>(0xf4 + Ordinal), Schema.TypeKey);
				}
			}
			return Schema;
		};
		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const bool bVftRole : {false, true})
		{
			FAngelscriptCachedTypeSchema GapMiddle = MakeOrdinalFixture(bVftRole);
			FAngelscriptCachedTypeSchema GapLast = GapMiddle;
			FAngelscriptCachedTypeSchema DuplicateMiddle = GapMiddle;
			FAngelscriptCachedTypeSchema DuplicateLast = GapMiddle;
			FAngelscriptCachedTypeSchema ReorderFirst = GapMiddle;
			FAngelscriptCachedTypeSchema ReorderLast = GapMiddle;
			if (bVftRole)
			{
				++GapMiddle.VirtualFunctionTable[1].VftOrdinal;
				++GapMiddle.VirtualFunctionTable[2].VftOrdinal;
				++GapLast.VirtualFunctionTable[2].VftOrdinal;
				DuplicateMiddle.VirtualFunctionTable[1].VftOrdinal = 0;
				DuplicateLast.VirtualFunctionTable[2].VftOrdinal = 1;
				Swap(ReorderFirst.VirtualFunctionTable[0],
					ReorderFirst.VirtualFunctionTable[1]);
				Swap(ReorderLast.VirtualFunctionTable[1],
					ReorderLast.VirtualFunctionTable[2]);
			}
			else
			{
				++GapMiddle.OrderedMethods[1].MethodOrdinal;
				++GapMiddle.OrderedMethods[2].MethodOrdinal;
				++GapLast.OrderedMethods[2].MethodOrdinal;
				DuplicateMiddle.OrderedMethods[1].MethodOrdinal = 0;
				DuplicateLast.OrderedMethods[2].MethodOrdinal = 1;
				Swap(ReorderFirst.OrderedMethods[0], ReorderFirst.OrderedMethods[1]);
				Swap(ReorderLast.OrderedMethods[1], ReorderLast.OrderedMethods[2]);
			}
			RunProducerCase(MoveTemp(GapMiddle), FError::OrdinalGap,
				TEXT("Method/VFT middle ordinal gap"));
			RunProducerCase(MoveTemp(GapLast), FError::OrdinalGap,
				TEXT("Method/VFT last ordinal gap"));
			RunProducerCase(MoveTemp(DuplicateMiddle), FError::DuplicateOrdinal,
				TEXT("Method/VFT middle duplicate ordinal"));
			RunProducerCase(MoveTemp(DuplicateLast), FError::DuplicateOrdinal,
				TEXT("Method/VFT last duplicate ordinal"));
			RunProducerCase(MoveTemp(ReorderFirst), FError::NonCanonicalOrder,
				TEXT("Method/VFT first complete-row reorder"));
			RunProducerCase(MoveTemp(ReorderLast), FError::NonCanonicalOrder,
				TEXT("Method/VFT last complete-row reorder"));
		}
		ASSERT_THAT(AreEqual(12, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(12, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const bool bVftRole : {false, true})
		{
			FAngelscriptCachedTypeSchema DuplicateKey = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			if (bVftRole)
			{
				AddVft(DuplicateKey,
					EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
					0, 0xd0, 0xd1, DuplicateKey.TypeKey, DuplicateKey.TypeKey);
				AddVft(DuplicateKey,
					EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
					1, 0xd0, 0xd2, DuplicateKey.TypeKey, DuplicateKey.TypeKey);
			}
			else
			{
				AddMethod(DuplicateKey, EAngelscriptCachedMethodSlotKind::LocalMethod,
					0, 0xd0, 0xd1, DuplicateKey.TypeKey);
				AddMethod(DuplicateKey, EAngelscriptCachedMethodSlotKind::LocalMethod,
					1, 0xd0, 0xd2, DuplicateKey.TypeKey);
			}
			RunProducerCase(MoveTemp(DuplicateKey), FError::DuplicateKey,
				TEXT("same FunctionKey at distinct ordinals inside one array"));
		}
		ASSERT_THAT(AreEqual(2, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(2, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const bool bVftRole : {false, true})
		{
			FAngelscriptCachedTypeSchema ZeroKey = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			FAngelscriptCachedTypeSchema MissingAbi = ZeroKey;
			if (bVftRole)
			{
				AddVft(ZeroKey, EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
					0, 0xd3, 0xd4, ZeroKey.TypeKey, ZeroKey.TypeKey);
				MissingAbi = ZeroKey;
				ZeroKey.VirtualFunctionTable[0].FunctionKey = {};
				MissingAbi.VirtualFunctionTable[0].ExpectedDeclarationAbi = {};
			}
			else
			{
				AddMethod(ZeroKey, EAngelscriptCachedMethodSlotKind::LocalMethod,
					0, 0xd3, 0xd4, ZeroKey.TypeKey);
				MissingAbi = ZeroKey;
				ZeroKey.OrderedMethods[0].FunctionKey = {};
				MissingAbi.OrderedMethods[0].ExpectedDeclarationAbi = {};
			}
			RunProducerCase(MoveTemp(ZeroKey), FError::ZeroStableKey,
				TEXT("Method/VFT zero FunctionKey"));
			RunProducerCase(MoveTemp(MissingAbi), FError::MissingExpectedAbi,
				TEXT("Method/VFT missing ExpectedDeclarationAbi"));
		}
		ASSERT_THAT(AreEqual(4, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(4, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		const FAngelscriptStableTypeKey ZeroOwner{};
		const auto MakeSingleMethodOwnerFixture = [&](const EAngelscriptCachedMethodSlotKind Kind,
			const FAngelscriptStableTypeKey Owner)
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			AddMethod(Schema, Kind, 0, 0xc0, 0xc1, Owner);
			return Schema;
		};
		const auto MakeSingleVftOwnerFixture = [&](
			const EAngelscriptCachedMethodSlotKind Kind,
			const FAngelscriptStableTypeKey DeclaringOwner,
			const FAngelscriptStableTypeKey ImplementingOwner)
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			AddVft(Schema, Kind, 0, 0xc2, 0xc3,
				DeclaringOwner, ImplementingOwner);
			return Schema;
		};
		FAngelscriptCachedTypeSchema OwnerBase = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		RunProducerCase(MakeSingleVftOwnerFixture(
			EAngelscriptCachedMethodSlotKind::Inherited, OtherOwner, OtherOwner),
			FError::None, TEXT("explicit nonself inherited VFT owner control"));
		RunProducerCase(MakeSingleMethodOwnerFixture(
			EAngelscriptCachedMethodSlotKind::Inherited, OwnerBase.TypeKey),
			FError::InvalidQualifierCombination,
			TEXT("Inherited Method forbids self owner"));
		RunProducerCase(MakeSingleVftOwnerFixture(
			EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
			OwnerBase.TypeKey, OtherOwner), FError::InvalidQualifierCombination,
			TEXT("VirtualDeclaration forbids nonself implementing owner"));
		RunProducerCase(MakeSingleVftOwnerFixture(
			EAngelscriptCachedMethodSlotKind::VirtualOverride,
			OwnerBase.TypeKey, OwnerBase.TypeKey), FError::InvalidQualifierCombination,
			TEXT("VirtualOverride forbids self declaring owner"));
		RunProducerCase(MakeSingleVftOwnerFixture(
			EAngelscriptCachedMethodSlotKind::VirtualOverride,
			OtherOwner, OtherOwner), FError::InvalidQualifierCombination,
			TEXT("VirtualOverride forbids nonself implementing owner"));
		RunProducerCase(MakeSingleVftOwnerFixture(
			EAngelscriptCachedMethodSlotKind::Inherited,
			OwnerBase.TypeKey, OtherOwner), FError::InvalidQualifierCombination,
			TEXT("Inherited VFT forbids self declaring owner"));
		RunProducerCase(MakeSingleVftOwnerFixture(
			EAngelscriptCachedMethodSlotKind::Inherited,
			OtherOwner, OwnerBase.TypeKey), FError::InvalidQualifierCombination,
			TEXT("Inherited VFT forbids self implementing owner"));
		RunProducerCase(MakeSingleMethodOwnerFixture(
			EAngelscriptCachedMethodSlotKind::LocalMethod, ZeroOwner),
			FError::ZeroStableKey, TEXT("Method required owner is zero"));
		RunProducerCase(MakeSingleVftOwnerFixture(
			EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
			ZeroOwner, OwnerBase.TypeKey), FError::ZeroStableKey,
			TEXT("VFT declaring owner is zero"));
		RunProducerCase(MakeSingleVftOwnerFixture(
			EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
			OwnerBase.TypeKey, ZeroOwner), FError::ZeroStableKey,
			TEXT("VFT implementing owner is zero"));
		ASSERT_THAT(AreEqual(10, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(1, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(9, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		FAngelscriptCachedTypeSchema CrossArray = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		AddMethod(CrossArray, EAngelscriptCachedMethodSlotKind::LocalMethod,
			0, 0xc8, 0xc9, CrossArray.TypeKey);
		AddVft(CrossArray, EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
			0, 0xc8, 0xc9, CrossArray.TypeKey, CrossArray.TypeKey);
		RunProducerCase(MoveTemp(CrossArray), FError::None,
			TEXT("same FunctionKey once in Method and once in VFT is locally valid"));
		ASSERT_THAT(AreEqual(1, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(1, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(0, FailureCalls - PartitionFailure));

		ASSERT_THAT(AreEqual(121, TotalCalls));
		ASSERT_THAT(AreEqual(20, SuccessCalls));
		ASSERT_THAT(AreEqual(101, FailureCalls));
		ASSERT_THAT(IsTrue(bPassed,
			TEXT("Method/VFT producer-local 121-call literal authority")));
	}

	TEST_METHOD(NormalProducerRejectsBehaviorGroupsOwnersFlagsAndAliasesAtomically)
	{
		using FError = EAngelscriptCacheValidationError;
		int32 TotalCalls = 0;
		int32 SuccessCalls = 0;
		int32 FailureCalls = 0;
		bool bPassed = true;
		const auto RunProducerCase = [&](FAngelscriptCachedTypeSchema Schema,
			const FError ExpectedError,
			const TCHAR* Context)
		{
			CanonicalizeDependenciesAndFinalizeForTests(Schema);
			++TotalCalls;
			if (ExpectedError == FError::None)
			{
				++SuccessCalls;
				bPassed &= ExpectExactNormalProducerSuccessAndInputUnchanged(
					*TestRunner, Schema, Context);
			}
			else
			{
				++FailureCalls;
				bPassed &= ExpectExactProducerFailureAndInputUnchanged(
					*TestRunner, Schema, ExpectedError, Context);
			}
		};

		int32 PartitionCalls = TotalCalls;
		int32 PartitionSuccess = SuccessCalls;
		int32 PartitionFailure = FailureCalls;
		for (uint8 RawKind = 1; RawKind <= 17; ++RawKind)
		{
			const EAngelscriptCachedBehaviorKind BehaviorKind =
				static_cast<EAngelscriptCachedBehaviorKind>(RawKind);
			for (uint8 RawTypeKind = 1; RawTypeKind <= 7; ++RawTypeKind)
			{
				const EAngelscriptCachedTypeKind TypeKind =
					static_cast<EAngelscriptCachedTypeKind>(RawTypeKind);
				for (const uint32 Cardinality : {uint32(1), uint32(2)})
				{
					for (const EAngelscriptCacheReferenceKind TargetKind : {
						EAngelscriptCacheReferenceKind::ScriptFunction,
						EAngelscriptCacheReferenceKind::EnvironmentSymbol})
					{
						for (uint8 OwnerCase = 0; OwnerCase < 4; ++OwnerCase)
						{
							const bool bRetainedB1DelegateConstructAbsent =
								TypeKind == EAngelscriptCachedTypeKind::Delegate
								&& BehaviorKind
									== EAngelscriptCachedBehaviorKind::Construct
								&& Cardinality == 1
								&& TargetKind
									== EAngelscriptCacheReferenceKind::ScriptFunction
								&& OwnerCase == 0;
							const bool bRetainedB1DelegateTemplateCallbackSelf =
								TypeKind == EAngelscriptCachedTypeKind::Delegate
								&& BehaviorKind
									== EAngelscriptCachedBehaviorKind::TemplateCallback
								&& Cardinality == 1
								&& TargetKind
									== EAngelscriptCacheReferenceKind::ScriptFunction
								&& OwnerCase == 1;
							if (bRetainedB1DelegateConstructAbsent
								|| bRetainedB1DelegateTemplateCallbackSelf)
							{
								continue;
							}

							const FBehaviorProductCellForTests* Cell =
								FindBehaviorProductCellForTests(
									TypeKind, BehaviorKind, Cardinality, TargetKind);
							const uint8 OwnerBit = 1u << OwnerCase;
							const FError ExpectedError = Cell != nullptr
								&& (Cell->SuccessOwnerMask & OwnerBit) != 0
								? FError::None : FError::InvalidPresence;
							RunProducerCase(
								BuildBehaviorProductFixtureForTests(
									TypeKind, BehaviorKind, Cardinality,
									TargetKind, OwnerCase),
								ExpectedError,
								TEXT("represented nonempty Behavior product literal"));
						}
					}
				}
			}
		}
		ASSERT_THAT(AreEqual(1902, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(101, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(1801, FailureCalls - PartitionFailure));

		struct FEmptyBehaviorFixture
		{
			FAngelscriptCachedTypeSchema Schema;
			const TCHAR* Context;
		};
		const TArray<FEmptyBehaviorFixture> EmptyBehaviorFixtures = {
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Class),
				TEXT("empty Class None Behavior producer form")},
			{MakeOrdinaryUClassSchema(false),
				TEXT("empty ordinary Class UClass Behavior producer form")},
			{MakeStaticsUClassSchema(),
				TEXT("empty statics Class UClass Behavior producer form")},
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct),
				TEXT("empty Struct None Behavior producer form")},
			{MakeReflectedUStructSchema(),
				TEXT("empty Struct UStruct Behavior producer form")},
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface),
				TEXT("empty Interface None Behavior producer form")},
			{MakeReflectionFormSchema(EAngelscriptCachedTypeKind::Enum,
				EAngelscriptCachedReflectionKind::None),
				TEXT("empty Enum None Behavior producer form")},
			{MakeEnumSchema(), TEXT("empty Enum UEnum Behavior producer form")},
			{MakeCompleteDelegateSchema(),
				TEXT("empty Delegate UDelegate Behavior producer form")},
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef),
				TEXT("empty Typedef None Behavior producer form")},
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef),
				TEXT("empty Funcdef None Behavior producer form")},
		};
		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const FEmptyBehaviorFixture& Fixture : EmptyBehaviorFixtures)
		{
			FAngelscriptCachedTypeSchema Schema = Fixture.Schema;
			ClearBehaviorOwnedStateForTests(Schema);
			RunProducerCase(MoveTemp(Schema), FError::None, Fixture.Context);
		}
		ASSERT_THAT(AreEqual(11, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(11, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(0, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (uint8 RawKind = 1; RawKind <= 17; ++RawKind)
		{
			const EAngelscriptCachedBehaviorKind BehaviorKind =
				static_cast<EAngelscriptCachedBehaviorKind>(RawKind);
			const FAngelscriptCachedTypeSchema OrdinaryClass =
				BuildBehaviorProductFixtureForTests(
					EAngelscriptCachedTypeKind::Class, BehaviorKind, 1,
					EAngelscriptCacheReferenceKind::ScriptFunction, 1);
			FAngelscriptCachedTypeSchema Statics = MakeStaticsUClassSchema();
			ClearBehaviorOwnedStateForTests(Statics);
			// Statics UClass forbids both the behavior rows and their
			// constructor/destructor semantic flags. Keep its valid form flags so
			// the intended nonempty-behavior rule is the first contradiction.
			for (const FAngelscriptCachedBehaviorSlot& Slot :
				OrdinaryClass.OrderedBehaviorSlots)
			{
				AddBehaviorSlotForTests(Statics, Slot.BehaviorKind,
					Slot.SlotOrdinal, Slot.Target, Slot.DeclaringOwner);
			}
			const FString Context = FString::Printf(TEXT(
				"statics UClass rejects canonical nonempty Behavior row kind=%u"),
				static_cast<uint32>(RawKind));
			RunProducerCase(MoveTemp(Statics), FError::InvalidPresence,
				*Context);
		}
		ASSERT_THAT(AreEqual(17, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(17, FailureCalls - PartitionFailure));

		ASSERT_THAT(AreEqual(1930, TotalCalls));
		ASSERT_THAT(AreEqual(112, SuccessCalls));
		ASSERT_THAT(AreEqual(1818, FailureCalls));

		const int32 FocusedStartCalls = TotalCalls;
		const int32 FocusedStartSuccess = SuccessCalls;
		const int32 FocusedStartFailure = FailureCalls;
		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const uint8 RawKind : {uint8(0), uint8(18), uint8(255)})
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Struct);
			ClearBehaviorOwnedStateForTests(Schema);
			AddBehaviorSlotForTests(Schema,
				static_cast<EAngelscriptCachedBehaviorKind>(RawKind), 0,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xd0, 0xd1),
				Schema.TypeKey);
			RunProducerCase(MoveTemp(Schema), FError::UnknownEnumValue,
				TEXT("raw BehaviorKind"));
		}
		ASSERT_THAT(AreEqual(3, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(3, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const uint8 RawReferenceKind : {uint8(0), uint8(10), uint8(255)})
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Struct);
			ClearBehaviorOwnedStateForTests(Schema);
			FAngelscriptCacheStableReference Target =
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xd2, 0xd3);
			Target.Kind =
				static_cast<EAngelscriptCacheReferenceKind>(RawReferenceKind);
			AddBehaviorSlotForTests(Schema,
				EAngelscriptCachedBehaviorKind::Construct, 0,
				Target, Schema.TypeKey);
			RunProducerCase(MoveTemp(Schema), FError::UnknownEnumValue,
				TEXT("raw Behavior target ReferenceKind"));
		}
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Struct);
			ClearBehaviorOwnedStateForTests(Schema);
			AddBehaviorSlotForTests(Schema,
				EAngelscriptCachedBehaviorKind::Construct, 0,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptType,
					0xd2, 0xd3),
				Schema.TypeKey);
			RunProducerCase(MoveTemp(Schema), FError::WrongReferenceKind,
				TEXT("known wrong Behavior target ReferenceKind"));
		}
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Struct);
			ClearBehaviorOwnedStateForTests(Schema);
			FAngelscriptCacheStableReference Target =
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xd2, 0xd3);
			Target.StableKey = {};
			AddBehaviorSlotForTests(Schema,
				EAngelscriptCachedBehaviorKind::Construct, 0,
				Target, Schema.TypeKey);
			RunProducerCase(MoveTemp(Schema), FError::ZeroStableKey,
				TEXT("zero Behavior target key"));
		}
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Struct);
			ClearBehaviorOwnedStateForTests(Schema);
			FAngelscriptCacheStableReference Target =
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xd2, 0xd3);
			Target.ExpectedAbi = {};
			AddBehaviorSlotForTests(Schema,
				EAngelscriptCachedBehaviorKind::Construct, 0,
				Target, Schema.TypeKey);
			RunProducerCase(MoveTemp(Schema), FError::MissingExpectedAbi,
				TEXT("missing Behavior target ABI"));
		}
		ASSERT_THAT(AreEqual(6, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(6, FailureCalls - PartitionFailure));

		struct FBehaviorGapRow
		{
			EAngelscriptCachedTypeKind TypeKind;
			EAngelscriptCachedBehaviorKind BehaviorKind;
			EAngelscriptCacheReferenceKind TargetKind;
			uint8 OwnerCase;
			bool bPresentZeroOwner;
		};
		const FBehaviorGapRow GapRows[] = {
			{EAngelscriptCachedTypeKind::Struct,
				EAngelscriptCachedBehaviorKind::ListConstruct,
				EAngelscriptCacheReferenceKind::ScriptFunction, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::Destruct,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::Factory,
				EAngelscriptCacheReferenceKind::ScriptFunction, 1, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::ListFactory,
				EAngelscriptCacheReferenceKind::ScriptFunction, 1, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::AddRef,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 1, true},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::Release,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 1, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::GetWeakRefFlag,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::TemplateCallback,
				EAngelscriptCacheReferenceKind::ScriptFunction, 1, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::GetRefCount,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::SetGcFlag,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::GetGcFlag,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::EnumRefs,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::ReleaseRefs,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::Copy,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Struct,
				EAngelscriptCachedBehaviorKind::CopyConstruct,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
			{EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::CopyFactory,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0, false},
		};
		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const FBehaviorGapRow& Row : GapRows)
		{
			FAngelscriptCachedTypeSchema Schema =
				BuildBehaviorProductFixtureForTests(
					Row.TypeKind, Row.BehaviorKind, 2,
					Row.TargetKind, Row.OwnerCase);
			const FAngelscriptCacheStableReference SecondTarget =
				MakeReference(Row.TargetKind, 0xa2, 0xa3);
			const int32 SecondIndex = FindBehaviorPhysicalIndexForTests(
				Schema, Row.BehaviorKind, 1, SecondTarget);
			Schema.OrderedBehaviorSlots[SecondIndex].SlotOrdinal = 2;
			if (Row.bPresentZeroOwner)
			{
				const FAngelscriptStableTypeKey ZeroOwner{};
				for (FAngelscriptCachedBehaviorSlot& Slot :
					Schema.OrderedBehaviorSlots)
				{
					if (Slot.BehaviorKind == Row.BehaviorKind)
					{
						Slot.DeclaringOwner = ZeroOwner;
					}
				}
			}
			RunProducerCase(MoveTemp(Schema), FError::OrdinalGap,
				TEXT("non-Construct per-kind 0,2 ordinal gap"));
		}
		for (uint8 RawKind = 1; RawKind <= 17; ++RawKind)
		{
			const EAngelscriptCachedBehaviorKind BehaviorKind =
				static_cast<EAngelscriptCachedBehaviorKind>(RawKind);
			FAngelscriptCachedTypeSchema Schema =
				BuildBehaviorProductFixtureForTests(
					EAngelscriptCachedTypeKind::Class, BehaviorKind, 2,
					EAngelscriptCacheReferenceKind::ScriptFunction, 1);
			const FAngelscriptCacheStableReference SecondTarget =
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xa2, 0xa3);
			const int32 SecondIndex = FindBehaviorPhysicalIndexForTests(
				Schema, BehaviorKind, 1, SecondTarget);
			Schema.OrderedBehaviorSlots[SecondIndex].SlotOrdinal = 0;
			RunProducerCase(MoveTemp(Schema), FError::DuplicateOrdinal,
				TEXT("per-kind duplicate Behavior ordinal"));
		}
		{
			FAngelscriptCachedTypeSchema Schema =
				BuildBehaviorProductFixtureForTests(
					EAngelscriptCachedTypeKind::Class,
					EAngelscriptCachedBehaviorKind::Construct, 2,
					EAngelscriptCacheReferenceKind::ScriptFunction, 1);
			const int32 FirstIndex = FindBehaviorPhysicalIndexForTests(
				Schema, EAngelscriptCachedBehaviorKind::Construct, 0,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xa0, 0xa1));
			const int32 SecondIndex = FindBehaviorPhysicalIndexForTests(
				Schema, EAngelscriptCachedBehaviorKind::Construct, 1,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xa2, 0xa3));
			Swap(Schema.OrderedBehaviorSlots[FirstIndex],
				Schema.OrderedBehaviorSlots[SecondIndex]);
			RunProducerCase(MoveTemp(Schema), FError::NonCanonicalOrder,
				TEXT("Behavior complete-row within-group reorder"));
		}
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Struct);
			ClearBehaviorOwnedStateForTests(Schema);
			AddBehaviorSlotForTests(Schema,
				EAngelscriptCachedBehaviorKind::Copy, 0,
				MakeReference(EAngelscriptCacheReferenceKind::EnvironmentSymbol,
					0xd4, 0xd5),
				{});
			AddBehaviorSlotForTests(Schema,
				EAngelscriptCachedBehaviorKind::Construct, 0,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xd6, 0xd7),
				Schema.TypeKey);
			RunProducerCase(MoveTemp(Schema), FError::NonCanonicalOrder,
				TEXT("BehaviorKind group disorder"));
		}
		ASSERT_THAT(AreEqual(35, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(35, FailureCalls - PartitionFailure));

		const auto MakeClassCountFixture = [&](
			const uint32 ConstructCount,
			const uint32 FactoryCount)
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			ClearBehaviorOwnedStateForTests(Schema);
			for (uint32 Index = 0; Index < ConstructCount; ++Index)
			{
				AddBehaviorSlotForTests(Schema,
					EAngelscriptCachedBehaviorKind::Construct, Index,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
						static_cast<uint8>(0xb0 + Index * 2),
						static_cast<uint8>(0xb1 + Index * 2)),
					Schema.TypeKey);
			}
			for (uint32 Index = 0; Index < FactoryCount; ++Index)
			{
				AddBehaviorSlotForTests(Schema,
					EAngelscriptCachedBehaviorKind::Factory, Index,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
						static_cast<uint8>(0xc0 + Index * 2),
						static_cast<uint8>(0xc1 + Index * 2)),
					Schema.TypeKey);
			}
			return Schema;
		};
		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		RunProducerCase(MakeClassCountFixture(1, 0), FError::InvalidPresence,
			TEXT("Class Construct Factory count 1/0"));
		RunProducerCase(MakeClassCountFixture(0, 1), FError::InvalidPresence,
			TEXT("Class Construct Factory count 0/1"));
		RunProducerCase(MakeClassCountFixture(2, 1), FError::InvalidPresence,
			TEXT("Class Construct Factory count 2/1"));
		RunProducerCase(MakeClassCountFixture(1, 2), FError::InvalidPresence,
			TEXT("Class Construct Factory count 1/2"));
		ASSERT_THAT(AreEqual(4, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(4, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const EAngelscriptCachedBehaviorKind CopyKind : {
			EAngelscriptCachedBehaviorKind::CopyConstruct,
			EAngelscriptCachedBehaviorKind::CopyFactory})
		{
			const EAngelscriptCachedTypeKind TypeKind =
				CopyKind == EAngelscriptCachedBehaviorKind::CopyConstruct
				? EAngelscriptCachedTypeKind::Struct
				: EAngelscriptCachedTypeKind::Class;
			const auto MakeCopyFixture = [&]()
			{
				return BuildBehaviorProductFixtureForTests(
					TypeKind, CopyKind, 1,
					EAngelscriptCacheReferenceKind::ScriptFunction, 1);
			};
			FAngelscriptCachedTypeSchema KeyMismatch = MakeCopyFixture();
			FAngelscriptCachedTypeSchema AbiMismatch = KeyMismatch;
			FAngelscriptCachedTypeSchema OwnerMismatch = KeyMismatch;
			FAngelscriptCachedTypeSchema NoPeer = KeyMismatch;
			const FAngelscriptCacheStableReference PrimaryTarget =
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xa0, 0xa1);
			const int32 CopyIndex = FindBehaviorPhysicalIndexForTests(
				KeyMismatch, CopyKind, 0, PrimaryTarget);
			KeyMismatch.OrderedBehaviorSlots[CopyIndex].Target.StableKey =
				MakeHash(0xda);
			RebuildBehaviorDependenciesForTests(KeyMismatch);
			AbiMismatch.OrderedBehaviorSlots[CopyIndex].Target.ExpectedAbi =
				MakeHash(0xdb);
			RebuildBehaviorDependenciesForTests(AbiMismatch);
			OwnerMismatch.OrderedBehaviorSlots[CopyIndex].DeclaringOwner =
				FAngelscriptStableTypeKey{MakeHash(0xdc)};
			if (CopyKind == EAngelscriptCachedBehaviorKind::CopyConstruct)
			{
				NoPeer.OrderedBehaviorSlots.RemoveAll([](
					const FAngelscriptCachedBehaviorSlot& Slot)
				{
					return Slot.BehaviorKind
						== EAngelscriptCachedBehaviorKind::Construct;
				});
			}
			else
			{
				NoPeer.OrderedBehaviorSlots.RemoveAll([](
					const FAngelscriptCachedBehaviorSlot& Slot)
				{
					return Slot.BehaviorKind
							== EAngelscriptCachedBehaviorKind::Factory
						|| Slot.BehaviorKind
							== EAngelscriptCachedBehaviorKind::Construct;
				});
			}
			RebuildBehaviorDependenciesForTests(NoPeer);
			RunProducerCase(MoveTemp(KeyMismatch),
				FError::InvalidQualifierCombination,
				TEXT("script Copy alias key mismatch"));
			RunProducerCase(MoveTemp(AbiMismatch),
				FError::ConflictingKey,
				TEXT("script Copy ABI mismatch is a Dependency conflict first"));
			RunProducerCase(MoveTemp(OwnerMismatch),
				FError::InvalidQualifierCombination,
				TEXT("script Copy alias owner mismatch"));
			RunProducerCase(MoveTemp(NoPeer),
				FError::InvalidQualifierCombination,
				TEXT("script Copy alias has no exact peer"));
		}
		ASSERT_THAT(AreEqual(8, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(8, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		{
			FAngelscriptCachedTypeSchema EnvironmentPresentZero =
				BuildBehaviorProductFixtureForTests(
					EAngelscriptCachedTypeKind::Class,
					EAngelscriptCachedBehaviorKind::AddRef, 1,
					EAngelscriptCacheReferenceKind::EnvironmentSymbol, 1);
			const int32 PrimaryIndex = FindBehaviorPhysicalIndexForTests(
				EnvironmentPresentZero,
				EAngelscriptCachedBehaviorKind::AddRef, 0,
				MakeReference(EAngelscriptCacheReferenceKind::EnvironmentSymbol,
					0xa0, 0xa1));
			EnvironmentPresentZero.OrderedBehaviorSlots[PrimaryIndex].
				DeclaringOwner = FAngelscriptStableTypeKey{};
			RunProducerCase(MoveTemp(EnvironmentPresentZero),
				FError::InvalidPresence,
				TEXT("Environment present-zero owner validates tag before inactive value"));
		}
		{
			FAngelscriptCachedTypeSchema ScriptPresentZeroGap =
				BuildBehaviorProductFixtureForTests(
					EAngelscriptCachedTypeKind::Class,
					EAngelscriptCachedBehaviorKind::GetRefCount, 2,
					EAngelscriptCacheReferenceKind::ScriptFunction, 1);
			const FAngelscriptStableTypeKey ZeroOwner{};
			for (FAngelscriptCachedBehaviorSlot& Slot :
				ScriptPresentZeroGap.OrderedBehaviorSlots)
			{
				if (Slot.BehaviorKind
					== EAngelscriptCachedBehaviorKind::GetRefCount)
				{
					Slot.DeclaringOwner = ZeroOwner;
				}
			}
			const int32 SecondIndex = FindBehaviorPhysicalIndexForTests(
				ScriptPresentZeroGap,
				EAngelscriptCachedBehaviorKind::GetRefCount, 1,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xa2, 0xa3));
			ScriptPresentZeroGap.OrderedBehaviorSlots[SecondIndex].SlotOrdinal = 2;
			RunProducerCase(MoveTemp(ScriptPresentZeroGap),
				FError::ZeroStableKey,
				TEXT("Script present-zero owner wins before later ordinal gap"));
		}
		ASSERT_THAT(AreEqual(2, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(0, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(2, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		for (const bool bEqualStableBytes : {false, true})
		{
			FAngelscriptCachedTypeSchema CopyConstruct =
				BuildBehaviorProductFixtureForTests(
					EAngelscriptCachedTypeKind::Struct,
					EAngelscriptCachedBehaviorKind::CopyConstruct, 1,
					EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0);
			const FAngelscriptCacheStableReference EnvironmentTarget =
				MakeReference(EAngelscriptCacheReferenceKind::EnvironmentSymbol,
					0xa0, 0xa1);
			const FAngelscriptCacheStableReference ScriptPeer =
				bEqualStableBytes
				? FAngelscriptCacheStableReference{
					EAngelscriptCacheReferenceKind::ScriptFunction,
					EnvironmentTarget.StableKey, EnvironmentTarget.ExpectedAbi}
				: MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xd8, 0xd9);
			AddBehaviorSlotForTests(CopyConstruct,
				EAngelscriptCachedBehaviorKind::Construct, 0,
				ScriptPeer, CopyConstruct.TypeKey);
			CanonicalSortBehaviorSlotsForTests(CopyConstruct);
			RunProducerCase(MoveTemp(CopyConstruct), FError::None,
				TEXT("Environment CopyConstruct is independent of Script peer"));

			FAngelscriptCachedTypeSchema CopyFactory =
				BuildBehaviorProductFixtureForTests(
					EAngelscriptCachedTypeKind::Class,
					EAngelscriptCachedBehaviorKind::CopyFactory, 1,
					EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0);
			AddBehaviorSlotForTests(CopyFactory,
				EAngelscriptCachedBehaviorKind::Factory, 0,
				ScriptPeer, CopyFactory.TypeKey);
			AddBehaviorSlotForTests(CopyFactory,
				EAngelscriptCachedBehaviorKind::Construct, 0,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xde, 0xdf),
				CopyFactory.TypeKey);
			CanonicalSortBehaviorSlotsForTests(CopyFactory);
			RunProducerCase(MoveTemp(CopyFactory), FError::None,
				TEXT("Environment CopyFactory is independent of Script peer"));
		}
		ASSERT_THAT(AreEqual(4, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(4, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(0, FailureCalls - PartitionFailure));

		PartitionCalls = TotalCalls;
		PartitionSuccess = SuccessCalls;
		PartitionFailure = FailureCalls;
		{
			FAngelscriptCachedTypeSchema AbstractClass =
				MakeOrdinaryUClassSchema(false);
			ClearBehaviorOwnedStateForTests(AbstractClass);
			AbstractClass.TypeSemanticFlags |= static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::Abstract);
			AbstractClass.Reflection.ClassReflectionFlags |= static_cast<uint32>(
				EAngelscriptCachedClassReflectionFlags::Abstract);
			AddBehaviorSlotForTests(AbstractClass,
				EAngelscriptCachedBehaviorKind::Construct, 0,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xe8, 0xe9),
				AbstractClass.TypeKey);
			AddBehaviorSlotForTests(AbstractClass,
				EAngelscriptCachedBehaviorKind::Factory, 0,
				MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
					0xea, 0xeb),
				AbstractClass.TypeKey);
			RunProducerCase(MoveTemp(AbstractClass), FError::None,
				TEXT("abstract ordinary Class retains Construct and Factory groups"));
		}
		{
			FAngelscriptCachedTypeSchema CrossRole = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			const FAngelscriptStableFunctionKey SharedKey{MakeHash(0xec)};
			const FAngelscriptHash256 SharedAbi = MakeHash(0xed);
			FAngelscriptCachedMethodEntry Method;
			Method.EntryKind = EAngelscriptCachedMethodSlotKind::LocalMethod;
			Method.MethodOrdinal = 0;
			Method.FunctionKey = SharedKey;
			Method.DeclaringOwner = CrossRole.TypeKey;
			Method.ExpectedDeclarationAbi = SharedAbi;
			CrossRole.OrderedMethods.Add(Method);
			FAngelscriptCachedVirtualFunctionSlot Vft;
			Vft.SlotKind =
				EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
			Vft.VftOrdinal = 0;
			Vft.FunctionKey = SharedKey;
			Vft.DeclaringOwner = CrossRole.TypeKey;
			Vft.ImplementingOwner = CrossRole.TypeKey;
			Vft.ExpectedDeclarationAbi = SharedAbi;
			CrossRole.VirtualFunctionTable.Add(Vft);
			AddBehaviorSlotForTests(CrossRole,
				EAngelscriptCachedBehaviorKind::Copy, 0,
				FAngelscriptCacheStableReference{
					EAngelscriptCacheReferenceKind::ScriptFunction,
					SharedKey.Hash, SharedAbi},
				CrossRole.TypeKey);
			RebuildMethodAndVftDependenciesForTests(CrossRole);
			RunProducerCase(MoveTemp(CrossRole), FError::None,
				TEXT("same opAssign FunctionKey across Method VFT and Behavior"));
		}
		ASSERT_THAT(AreEqual(2, TotalCalls - PartitionCalls));
		ASSERT_THAT(AreEqual(2, SuccessCalls - PartitionSuccess));
		ASSERT_THAT(AreEqual(0, FailureCalls - PartitionFailure));

		ASSERT_THAT(AreEqual(64, TotalCalls - FocusedStartCalls));
		ASSERT_THAT(AreEqual(6, SuccessCalls - FocusedStartSuccess));
		ASSERT_THAT(AreEqual(58, FailureCalls - FocusedStartFailure));
		ASSERT_THAT(AreEqual(1994, TotalCalls));
		ASSERT_THAT(AreEqual(118, SuccessCalls));
		ASSERT_THAT(AreEqual(1876, FailureCalls));
		ASSERT_THAT(IsTrue(bPassed,
			TEXT("Behavior producer 1,994-call literal authority")));
	}

	TEST_METHOD(MethodAndVftKindsAreExhaustiveInBothIndependentRoles)
	{
		enum class EMethodForm : uint8
		{
			ClassNone,
			OrdinaryUClass,
			StaticsUClass,
			StructNone,
			UStruct,
			InterfaceNone,
			EnumNone,
			UEnum,
			Delegate,
			Typedef,
			Funcdef,
		};
		struct FFormFixture
		{
			EMethodForm Form;
			FAngelscriptCachedTypeSchema Schema;
		};
		const TArray<FFormFixture> Forms = {
			{EMethodForm::ClassNone, MakeMinimalSchema(EAngelscriptCachedTypeKind::Class)},
			{EMethodForm::OrdinaryUClass, MakeOrdinaryUClassSchema(false)},
			{EMethodForm::StaticsUClass, MakeStaticsUClassSchema()},
			{EMethodForm::StructNone, MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct)},
			{EMethodForm::UStruct, MakeReflectedUStructSchema()},
			{EMethodForm::InterfaceNone, MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface)},
			{EMethodForm::EnumNone, MakeReflectionFormSchema(EAngelscriptCachedTypeKind::Enum,
				EAngelscriptCachedReflectionKind::None)},
			{EMethodForm::UEnum, MakeEnumSchema()},
			{EMethodForm::Delegate, MakeCompleteDelegateSchema()},
			{EMethodForm::Typedef, MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef)},
			{EMethodForm::Funcdef, MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef)},
		};
		for (const FFormFixture& FormFixture : Forms)
		{
			for (uint8 RawKind = 1; RawKind <= 4; ++RawKind)
			{
				const EAngelscriptCachedMethodSlotKind Kind =
					static_cast<EAngelscriptCachedMethodSlotKind>(RawKind);
				for (const bool bVftRole : {false, true})
				{
					FAngelscriptCachedTypeSchema Schema = FormFixture.Schema;
					Schema.OrderedMethods.Reset();
					Schema.VirtualFunctionTable.Reset();
					const FAngelscriptStableTypeKey OtherOwner{MakeHash(0xe0)};
					if (!bVftRole)
					{
						FAngelscriptCachedMethodEntry Method;
						Method.EntryKind = Kind;
						Method.MethodOrdinal = 0;
						Method.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(0xe1)};
						Method.DeclaringOwner = Kind == EAngelscriptCachedMethodSlotKind::LocalMethod
							? Schema.TypeKey : OtherOwner;
						Method.ExpectedDeclarationAbi = MakeHash(0xe2);
						Schema.OrderedMethods.Add(Method);
					}
					else
					{
						FAngelscriptCachedVirtualFunctionSlot Slot;
						Slot.SlotKind = Kind;
						Slot.VftOrdinal = 0;
						Slot.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(0xe3)};
						Slot.DeclaringOwner = Kind == EAngelscriptCachedMethodSlotKind::VirtualDeclaration
							? Schema.TypeKey : OtherOwner;
						Slot.ImplementingOwner =
							Kind == EAngelscriptCachedMethodSlotKind::Inherited
							? OtherOwner : Schema.TypeKey;
						Slot.ExpectedDeclarationAbi = MakeHash(0xe4);
						Schema.VirtualFunctionTable.Add(Slot);
					}

					const bool bClassOrInterface =
						FormFixture.Form == EMethodForm::ClassNone
						|| FormFixture.Form == EMethodForm::OrdinaryUClass
						|| FormFixture.Form == EMethodForm::InterfaceNone;
					const bool bLocalOnlyForm =
						FormFixture.Form == EMethodForm::StructNone
						|| FormFixture.Form == EMethodForm::UStruct
						|| FormFixture.Form == EMethodForm::Delegate;
					const bool bExpected = !bVftRole
						? (Kind == EAngelscriptCachedMethodSlotKind::LocalMethod
								&& (bClassOrInterface || bLocalOnlyForm))
							|| (Kind == EAngelscriptCachedMethodSlotKind::Inherited
								&& bClassOrInterface)
						: bClassOrInterface
							&& (Kind == EAngelscriptCachedMethodSlotKind::VirtualDeclaration
								|| Kind == EAngelscriptCachedMethodSlotKind::VirtualOverride
								|| Kind == EAngelscriptCachedMethodSlotKind::Inherited);
					const FString Context = FString::Printf(TEXT(
						"Method/VFT independent role matrix form=%u role=%s kind=%u"),
						static_cast<uint32>(FormFixture.Form),
						bVftRole ? TEXT("vft") : TEXT("method"),
						static_cast<uint32>(RawKind));
					RebuildMethodAndVftDependenciesForTests(Schema);
					CanonicalizeDependenciesAndFinalizeForTests(Schema);
					const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
					if (bExpected)
					{
						ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()),
							*FString::Printf(TEXT(
								"%s expected success error=%u stage=%u offset=%llu"),
								*Context, static_cast<uint32>(Outcome.Result.Error),
								static_cast<uint32>(Outcome.Result.Stage),
								Outcome.Result.ByteOffset));
					}
					ASSERT_THAT(AreEqual(bExpected, Outcome.Output.IsSet()));
					if (!bExpected)
					{
						const bool bStaticsRoleMismatch =
							FormFixture.Form == EMethodForm::StaticsUClass
							&& ((!bVftRole
									&& (Kind
										== EAngelscriptCachedMethodSlotKind::VirtualDeclaration
										|| Kind
											== EAngelscriptCachedMethodSlotKind::VirtualOverride))
								|| (bVftRole && Kind
									== EAngelscriptCachedMethodSlotKind::LocalMethod));
						ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
							Outcome,
								(!bVftRole && (bClassOrInterface || bLocalOnlyForm))
								|| (bVftRole && bClassOrInterface)
								|| bStaticsRoleMismatch
								? EAngelscriptCacheValidationError::InvalidQualifierCombination
								: EAngelscriptCacheValidationError::InvalidPresence,
							EAngelscriptCacheValidationStage::LocalSemantic,
							bVftRole
								? EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable
								: EAngelscriptCacheTypeSchemaTestField::OrderedMethod,
							*Context, 0)));
					}
				}
			}
		}

		for (const bool bVftRole : {false, true})
		{
			FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
				EAngelscriptCachedTypeKind::Class);
			for (uint32 Ordinal = 0; Ordinal < 3; ++Ordinal)
			{
				const uint8 KeyFill = static_cast<uint8>(0xf0 + Ordinal);
				const uint8 AbiFill = static_cast<uint8>(0xf4 + Ordinal);
				if (bVftRole)
				{
					FAngelscriptCachedVirtualFunctionSlot Slot;
					Slot.SlotKind = EAngelscriptCachedMethodSlotKind::VirtualDeclaration;
					Slot.VftOrdinal = Ordinal;
					Slot.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(KeyFill)};
					Slot.DeclaringOwner = Schema.TypeKey;
					Slot.ImplementingOwner = Schema.TypeKey;
					Slot.ExpectedDeclarationAbi = MakeHash(AbiFill);
					Schema.VirtualFunctionTable.Add(Slot);
				}
				else
				{
					FAngelscriptCachedMethodEntry Method;
					Method.EntryKind = EAngelscriptCachedMethodSlotKind::LocalMethod;
					Method.MethodOrdinal = Ordinal;
					Method.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(KeyFill)};
					Method.DeclaringOwner = Schema.TypeKey;
					Method.ExpectedDeclarationAbi = MakeHash(AbiFill);
					Schema.OrderedMethods.Add(Method);
				}
				Schema.Dependencies.Add(MakeDependency(
					EAngelscriptCacheSemanticDependencyKind::Declaration,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
						KeyFill, AbiFill)));
			}
			FinalizeValidFixtureHashes(Schema);
			for (uint32 Position = 0; Position < 3; ++Position)
			{
				FAngelscriptCachedTypeSchema Gap = Schema;
				if (bVftRole)
				{
					for (uint32 Index = Position; Index < 3; ++Index)
					{
						++Gap.VirtualFunctionTable[Index].VftOrdinal;
					}
				}
				else
				{
					for (uint32 Index = Position; Index < 3; ++Index)
					{
						++Gap.OrderedMethods[Index].MethodOrdinal;
					}
				}
				ASSERT_THAT(IsTrue(
					RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
						Gap).IsSuccess()));
				const FMalformedDecodeOutcome GapOutcome = DecodePhysicalOnlyFixture(Gap);
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, GapOutcome,
					EAngelscriptCacheValidationError::OrdinalGap,
					EAngelscriptCacheValidationStage::LocalSemantic,
					bVftRole
						? EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable
						: EAngelscriptCacheTypeSchemaTestField::OrderedMethod,
					TEXT("independent Method/VFT ordinal gap"), Position)));

				if (Position > 0)
				{
					FAngelscriptCachedTypeSchema Duplicate = Schema;
					if (bVftRole)
					{
						Duplicate.VirtualFunctionTable[Position].VftOrdinal = Position - 1;
					}
					else
					{
						Duplicate.OrderedMethods[Position].MethodOrdinal = Position - 1;
					}
					ASSERT_THAT(IsTrue(
						RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
							Duplicate).IsSuccess()));
					const FMalformedDecodeOutcome DuplicateOutcome =
						DecodePhysicalOnlyFixture(Duplicate);
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
						DuplicateOutcome,
						EAngelscriptCacheValidationError::DuplicateOrdinal,
						EAngelscriptCacheValidationStage::LocalSemantic,
						bVftRole
							? EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable
							: EAngelscriptCacheTypeSchemaTestField::OrderedMethod,
						TEXT("isolated Method/VFT duplicate ordinal"), Position)));
				}

				FAngelscriptCachedTypeSchema Reordered = Schema;
				const uint32 Peer = Position == 2 ? 1 : Position + 1;
				if (bVftRole)
				{
					Swap(Reordered.VirtualFunctionTable[Position],
						Reordered.VirtualFunctionTable[Peer]);
				}
				else
				{
					Swap(Reordered.OrderedMethods[Position],
						Reordered.OrderedMethods[Peer]);
				}
				ASSERT_THAT(IsTrue(
					RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
						Reordered).IsSuccess()));
				const FMalformedDecodeOutcome ReorderedOutcome =
					DecodePhysicalOnlyFixture(Reordered);
				ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, ReorderedOutcome,
					EAngelscriptCacheValidationError::NonCanonicalOrder,
					EAngelscriptCacheValidationStage::LocalSemantic,
					bVftRole
						? EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable
						: EAngelscriptCacheTypeSchemaTestField::OrderedMethod,
					TEXT("independent Method/VFT complete-row reorder"),
					FMath::Min(Position, Peer))));
			}
		}
	}

	TEST_METHOD(MethodAndVftOwnerPermutationsAreFocusedPerRole)
	{
		const FAngelscriptStableTypeKey OtherOwner{MakeHash(0xe0)};
		const EAngelscriptCachedMethodSlotKind Kinds[] = {
			EAngelscriptCachedMethodSlotKind::LocalMethod,
			EAngelscriptCachedMethodSlotKind::VirtualDeclaration,
			EAngelscriptCachedMethodSlotKind::VirtualOverride,
			EAngelscriptCachedMethodSlotKind::Inherited,
		};
		for (const EAngelscriptCachedMethodSlotKind Kind : Kinds)
		{
			for (const bool bDeclaringSelf : {false, true})
			{
				FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
					EAngelscriptCachedTypeKind::Class);
				FAngelscriptCachedMethodEntry Method;
				Method.EntryKind = Kind;
				Method.MethodOrdinal = 0;
				Method.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(0xe1)};
				Method.DeclaringOwner = bDeclaringSelf ? Schema.TypeKey : OtherOwner;
				Method.ExpectedDeclarationAbi = MakeHash(0xe2);
				Schema.OrderedMethods.Add(Method);
				Schema.Dependencies.Add(MakeDependency(
					EAngelscriptCacheSemanticDependencyKind::Declaration,
					MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction, 0xe1, 0xe2)));
				const bool bExpected =
					(Kind == EAngelscriptCachedMethodSlotKind::LocalMethod && bDeclaringSelf)
					|| (Kind == EAngelscriptCachedMethodSlotKind::Inherited && !bDeclaringSelf);
				if (bExpected)
				{
					FinalizeValidFixtureHashes(Schema);
					TArray<uint8> Payload;
					ASSERT_THAT(IsTrue(
						FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
							Schema, Payload).IsSuccess()));
				}
				else
				{
					const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
						EAngelscriptCacheValidationError::InvalidQualifierCombination,
						EAngelscriptCacheValidationStage::LocalSemantic,
						EAngelscriptCacheTypeSchemaTestField::OrderedMethod,
						TEXT("OrderedMethods kind/declaring-owner permutation"), 0)));
				}
			}

			for (const bool bDeclaringSelf : {false, true})
			{
				for (const bool bImplementingSelf : {false, true})
				{
					FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
						EAngelscriptCachedTypeKind::Class);
					FAngelscriptCachedVirtualFunctionSlot Slot;
					Slot.SlotKind = Kind;
					Slot.VftOrdinal = 0;
					Slot.FunctionKey = FAngelscriptStableFunctionKey{MakeHash(0xe3)};
					Slot.DeclaringOwner = bDeclaringSelf ? Schema.TypeKey : OtherOwner;
					Slot.ImplementingOwner = bImplementingSelf ? Schema.TypeKey : OtherOwner;
					Slot.ExpectedDeclarationAbi = MakeHash(0xe4);
					Schema.VirtualFunctionTable.Add(Slot);
					Schema.Dependencies.Add(MakeDependency(
						EAngelscriptCacheSemanticDependencyKind::Declaration,
						MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
							0xe3, 0xe4)));
					const bool bExpected =
						(Kind == EAngelscriptCachedMethodSlotKind::VirtualDeclaration
							&& bDeclaringSelf && bImplementingSelf)
						|| (Kind == EAngelscriptCachedMethodSlotKind::VirtualOverride
							&& !bDeclaringSelf && bImplementingSelf)
						|| (Kind == EAngelscriptCachedMethodSlotKind::Inherited
							&& !bDeclaringSelf && !bImplementingSelf);
					if (bExpected)
					{
						FinalizeValidFixtureHashes(Schema);
						TArray<uint8> Payload;
						ASSERT_THAT(IsTrue(
							FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
								Schema, Payload).IsSuccess()));
					}
					else
					{
						const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
						ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
							EAngelscriptCacheValidationError::InvalidQualifierCombination,
							EAngelscriptCacheValidationStage::LocalSemantic,
							EAngelscriptCacheTypeSchemaTestField::VirtualFunctionTable,
							TEXT("VFT kind/declaring/implementing-owner permutation"), 0)));
					}
				}
			}
		}
	}

	TEST_METHOD(AllSeventeenBehaviorKindsUseRepresentedNonemptyRowsAndElevenEmptyForms)
	{
		struct FEmptyBehaviorFixture
		{
			FAngelscriptCachedTypeSchema Schema;
			const TCHAR* Context;
		};
		const TArray<FEmptyBehaviorFixture> EmptyBehaviorFixtures = {
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Class),
				TEXT("empty Class None Behavior form")},
			{MakeOrdinaryUClassSchema(false),
				TEXT("empty ordinary Class UClass Behavior form")},
			{MakeStaticsUClassSchema(),
				TEXT("empty statics Class UClass Behavior form")},
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct),
				TEXT("empty Struct None Behavior form")},
			{MakeReflectedUStructSchema(),
				TEXT("empty Struct UStruct Behavior form")},
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Interface),
				TEXT("empty Interface None Behavior form")},
			{MakeReflectionFormSchema(EAngelscriptCachedTypeKind::Enum,
				EAngelscriptCachedReflectionKind::None),
				TEXT("empty Enum None Behavior form")},
			{MakeEnumSchema(), TEXT("empty Enum UEnum Behavior form")},
			{MakeCompleteDelegateSchema(),
				TEXT("empty Delegate UDelegate Behavior form")},
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Typedef),
				TEXT("empty Typedef None Behavior form")},
			{MakeMinimalSchema(EAngelscriptCachedTypeKind::Funcdef),
				TEXT("empty Funcdef None Behavior form")},
		};
		for (const FEmptyBehaviorFixture& Fixture : EmptyBehaviorFixtures)
		{
			FAngelscriptCachedTypeSchema Schema = Fixture.Schema;
			ClearBehaviorOwnedStateForTests(Schema);
			CanonicalizeDependenciesAndFinalizeForTests(Schema);
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()), Fixture.Context);
			ASSERT_THAT(IsTrue(Outcome.Output.IsSet()), Fixture.Context);
		}

		for (uint8 RawKind = 1; RawKind <= 17; ++RawKind)
		{
			const EAngelscriptCachedBehaviorKind Kind =
				static_cast<EAngelscriptCachedBehaviorKind>(RawKind);
			for (uint8 RawType = 1; RawType <= 7; ++RawType)
			{
				const EAngelscriptCachedTypeKind TypeKind =
					static_cast<EAngelscriptCachedTypeKind>(RawType);
				for (uint32 Cardinality = 1; Cardinality <= 2; ++Cardinality)
				{
					for (const bool bEnvironmentTarget : {false, true})
					{
						for (uint8 OwnerCase = 0; OwnerCase < 4; ++OwnerCase)
						{
							const EAngelscriptCacheReferenceKind TargetKind =
								bEnvironmentTarget
								? EAngelscriptCacheReferenceKind::EnvironmentSymbol
								: EAngelscriptCacheReferenceKind::ScriptFunction;
							FAngelscriptCachedTypeSchema Schema =
								BuildBehaviorProductFixtureForTests(
									TypeKind, Kind, Cardinality, TargetKind, OwnerCase);

							const FBehaviorProductCellForTests* Cell =
								FindBehaviorProductCellForTests(
									TypeKind, Kind, Cardinality, TargetKind);
							const uint8 OwnerBit = 1u << OwnerCase;
							const EAngelscriptCacheValidationError ExpectedError =
								Cell != nullptr
									&& (Cell->SuccessOwnerMask & OwnerBit) != 0
								? EAngelscriptCacheValidationError::None
								: EAngelscriptCacheValidationError::InvalidPresence;
							const FMalformedDecodeOutcome Outcome =
								DecodePhysicalOnlyFixture(Schema);
							if (ExpectedError
								== EAngelscriptCacheValidationError::None)
							{
								ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()));
							}
							ASSERT_THAT(AreEqual(
								ExpectedError == EAngelscriptCacheValidationError::None,
								Outcome.Output.IsSet()));
							if (ExpectedError
								!= EAngelscriptCacheValidationError::None)
							{
								uint32 ProvingOrdinal = 0;
								const FBehaviorProductCellForTests* SingletonCell =
									FindBehaviorProductCellForTests(
										TypeKind, Kind, 1, TargetKind);
								if (Cardinality == 2
									&& SingletonCell != nullptr
									&& (SingletonCell->SuccessOwnerMask & OwnerBit) != 0)
								{
									ProvingOrdinal = 1;
								}
								const FAngelscriptCacheStableReference ProvingTarget =
									MakeReference(TargetKind,
										static_cast<uint8>(0xa0 + ProvingOrdinal * 2),
										static_cast<uint8>(0xa1 + ProvingOrdinal * 2));
								const int32 PhysicalIndex =
									FindBehaviorPhysicalIndexForTests(
										Schema, Kind, ProvingOrdinal, ProvingTarget);
								const EAngelscriptCacheTypeSchemaTestField FailureField =
									TargetKind
										== EAngelscriptCacheReferenceKind::EnvironmentSymbol
										&& OwnerCase != 0
										? EAngelscriptCacheTypeSchemaTestField::
											BehaviorDeclaringOwnerOptionalTag
										: EAngelscriptCacheTypeSchemaTestField::BehaviorSlot;
								const int32 FailureSecondaryIndex =
									FailureField
										== EAngelscriptCacheTypeSchemaTestField::
											BehaviorDeclaringOwnerOptionalTag
										? 1 : INDEX_NONE;
								ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
									Outcome, EAngelscriptCacheValidationError::InvalidPresence,
									EAngelscriptCacheValidationStage::LocalSemantic,
									FailureField,
									TEXT("Behavior represented product literal cell"),
									PhysicalIndex, FailureSecondaryIndex)));
							}
						}
					}
				}
			}
		}

		for (uint8 RawKind = 1; RawKind <= 17; ++RawKind)
		{
			const EAngelscriptCachedBehaviorKind BehaviorKind =
				static_cast<EAngelscriptCachedBehaviorKind>(RawKind);
			const FAngelscriptCachedTypeSchema OrdinaryClass =
				BuildBehaviorProductFixtureForTests(
					EAngelscriptCachedTypeKind::Class, BehaviorKind, 1,
					EAngelscriptCacheReferenceKind::ScriptFunction, 1);
			FAngelscriptCachedTypeSchema Statics = MakeStaticsUClassSchema();
			ClearBehaviorOwnedStateForTests(Statics);
			// Keep the valid Statics form flags. Copying HasDestructor here
			// makes ReflectionFormClosure fail before the nonempty Behavior row
			// that this matrix is intended to exercise.
			for (const FAngelscriptCachedBehaviorSlot& Slot :
				OrdinaryClass.OrderedBehaviorSlots)
			{
				AddBehaviorSlotForTests(Statics, Slot.BehaviorKind,
					Slot.SlotOrdinal, Slot.Target, Slot.DeclaringOwner);
			}
			CanonicalizeDependenciesAndFinalizeForTests(Statics);
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Statics);
			const FString Context = FString::Printf(TEXT(
				"statics UClass forbids every BehaviorKind kind=%u"),
				static_cast<uint32>(RawKind));
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::BehaviorSlot,
				*Context,
				0)));
		}
	}

	TEST_METHOD(BehaviorOrdinalsFlagsCardinalityAndCopyAliasesHaveFocusedRows)
	{
		const auto AddScriptSlot = [](FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedBehaviorKind Kind,
			const uint32 Ordinal,
			const uint8 KeyFill,
			const uint8 AbiFill)
		{
			FAngelscriptCachedBehaviorSlot Slot;
			Slot.BehaviorKind = Kind;
			Slot.SlotOrdinal = Ordinal;
			Slot.Target = MakeReference(
				EAngelscriptCacheReferenceKind::ScriptFunction, KeyFill, AbiFill);
			Slot.DeclaringOwner = Schema.TypeKey;
			Schema.OrderedBehaviorSlots.Add(Slot);
			Schema.Dependencies.AddUnique(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration, Slot.Target));
		};
		const auto AssertBehaviorFailure = [&](const FAngelscriptCachedTypeSchema& Value,
			const EAngelscriptCacheValidationError Error,
			const TCHAR* Context,
			const EAngelscriptCacheTypeSchemaTestField Field,
			const int32 PrimaryIndex = INDEX_NONE,
			const int32 SecondaryIndex = INDEX_NONE)
		{
			const FMalformedDecodeOutcome Failure = DecodePhysicalOnlyFixture(Value);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Failure,
				Error, EAngelscriptCacheValidationStage::LocalSemantic,
				Field, Context, PrimaryIndex, SecondaryIndex)));
		};

		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Struct);
		for (uint32 Ordinal = 0; Ordinal < 3; ++Ordinal)
		{
			AddScriptSlot(Schema, EAngelscriptCachedBehaviorKind::Construct,
				Ordinal, static_cast<uint8>(0xa0 + Ordinal * 2),
				static_cast<uint8>(0xa1 + Ordinal * 2));
		}
		FAngelscriptCachedTypeSchema Invalid = Schema;
		Invalid.OrderedBehaviorSlots[0].SlotOrdinal = 1;
		CanonicalizeDependenciesAndFinalizeForTests(Invalid);
		AssertBehaviorFailure(Invalid, EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("Behavior first ordinal gap"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 0);
		Invalid = Schema;
		Invalid.OrderedBehaviorSlots[1].SlotOrdinal = 0;
		CanonicalizeDependenciesAndFinalizeForTests(Invalid);
		AssertBehaviorFailure(Invalid,
			EAngelscriptCacheValidationError::DuplicateOrdinal,
			TEXT("Behavior duplicate ordinal"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 1);
		Invalid = Schema;
		FAngelscriptCachedBehaviorSlot Copy = Invalid.OrderedBehaviorSlots[0];
		Copy.BehaviorKind = EAngelscriptCachedBehaviorKind::Copy;
		Copy.SlotOrdinal = 0;
		Invalid.OrderedBehaviorSlots.Insert(Copy, 0);
		CanonicalizeDependenciesAndFinalizeForTests(Invalid);
		AssertBehaviorFailure(Invalid,
			EAngelscriptCacheValidationError::NonCanonicalOrder,
			TEXT("Behavior group noncanonical order"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 0);

		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		AddScriptSlot(Schema, EAngelscriptCachedBehaviorKind::Construct, 0, 0xb0, 0xb1);
		AssertBehaviorFailure(Schema,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Class Construct and Factory counts are bidirectional"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 0);
		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		AddScriptSlot(Schema, EAngelscriptCachedBehaviorKind::Factory, 0, 0xb2, 0xb3);
		AssertBehaviorFailure(Schema,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("Class Factory requires matching Construct"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 0);

		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		Schema.TypeSemanticFlags |= static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::HasDestructor);
		AssertBehaviorFailure(Schema,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("HasDestructor requires Destruct behavior"),
			EAngelscriptCacheTypeSchemaTestField::TypeSemanticFlags);
		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		AddScriptSlot(Schema, EAngelscriptCachedBehaviorKind::Destruct, 0, 0xb4, 0xb5);
		AssertBehaviorFailure(Schema,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("HasDestructor and Destruct are bidirectional"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 0);

		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		AddScriptSlot(Schema, EAngelscriptCachedBehaviorKind::Construct, 0, 0xc0, 0xc1);
		AddScriptSlot(Schema, EAngelscriptCachedBehaviorKind::CopyConstruct, 0, 0xc0, 0xc1);
		FinalizeValidFixtureHashes(Schema);
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Payload).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Payload, Limits, Budget, Output).IsSuccess()));
		Invalid = Schema;
		Invalid.OrderedBehaviorSlots[1].Target = MakeReference(
			EAngelscriptCacheReferenceKind::ScriptFunction, 0xc2, 0xc3);
		RebuildBehaviorDependenciesForTests(Invalid);
		CanonicalizeDependenciesAndFinalizeForTests(Invalid);
		AssertBehaviorFailure(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("CopyConstruct script alias must equal Construct target"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 1);

		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		FAngelscriptCachedBehaviorSlot EnvironmentCopyConstruct;
		EnvironmentCopyConstruct.BehaviorKind =
			EAngelscriptCachedBehaviorKind::CopyConstruct;
		EnvironmentCopyConstruct.Target = MakeReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, 0xc4, 0xc5);
		Schema.OrderedBehaviorSlots.Add(EnvironmentCopyConstruct);
		Schema.Dependencies.Add(MakeDependency(
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
			EnvironmentCopyConstruct.Target));
		FinalizeValidFixtureHashes(Schema);
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Payload).IsSuccess()));
		Output.Reset();
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Payload, Limits, Budget, Output).IsSuccess()),
			TEXT("an EnvironmentSymbol CopyConstruct is independent and has no script alias"));

		Invalid = Schema;
		Invalid.OrderedBehaviorSlots[0].Target.Kind =
			EAngelscriptCacheReferenceKind::ScriptType;
		RebuildBehaviorDependenciesForTests(Invalid);
		CanonicalizeDependenciesAndFinalizeForTests(Invalid);
		AssertBehaviorFailure(Invalid,
			EAngelscriptCacheValidationError::WrongReferenceKind,
			TEXT("Behavior target reference kind"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorTarget, 0);
		Invalid = Schema;
		Invalid.OrderedBehaviorSlots[0].Target.ExpectedAbi = {};
		RebuildBehaviorDependenciesForTests(Invalid);
		CanonicalizeDependenciesAndFinalizeForTests(Invalid);
		AssertBehaviorFailure(Invalid,
			EAngelscriptCacheValidationError::MissingExpectedAbi,
			TEXT("Behavior target expected ABI"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorTarget, 0);

		FAngelscriptCachedTypeSchema EnvironmentPresentZero =
			BuildBehaviorProductFixtureForTests(
				EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::AddRef, 1,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 1);
		EnvironmentPresentZero.OrderedBehaviorSlots[0].DeclaringOwner =
			FAngelscriptStableTypeKey{};
		CanonicalizeDependenciesAndFinalizeForTests(EnvironmentPresentZero);
		AssertBehaviorFailure(EnvironmentPresentZero,
			EAngelscriptCacheValidationError::InvalidPresence,
			TEXT("canonical Environment owner present-zero"),
			EAngelscriptCacheTypeSchemaTestField::
				BehaviorDeclaringOwnerOptionalTag, 0, 1);

		FAngelscriptCachedTypeSchema ScriptPresentZeroGap =
			BuildBehaviorProductFixtureForTests(
				EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::GetRefCount, 2,
				EAngelscriptCacheReferenceKind::ScriptFunction, 1);
		for (FAngelscriptCachedBehaviorSlot& Slot :
			ScriptPresentZeroGap.OrderedBehaviorSlots)
		{
			Slot.DeclaringOwner = FAngelscriptStableTypeKey{};
		}
		ScriptPresentZeroGap.OrderedBehaviorSlots[1].SlotOrdinal = 2;
		CanonicalizeDependenciesAndFinalizeForTests(ScriptPresentZeroGap);
		AssertBehaviorFailure(ScriptPresentZeroGap,
			EAngelscriptCacheValidationError::ZeroStableKey,
			TEXT("Script present-zero owner precedes later 0,2 gap"),
			EAngelscriptCacheTypeSchemaTestField::
				BehaviorDeclaringOwnerOptionalTag, 0, 1);

		FAngelscriptCachedTypeSchema ScriptAbsentGap =
			BuildBehaviorProductFixtureForTests(
				EAngelscriptCachedTypeKind::Struct,
				EAngelscriptCachedBehaviorKind::ListConstruct, 2,
				EAngelscriptCacheReferenceKind::ScriptFunction, 0);
		ScriptAbsentGap.OrderedBehaviorSlots[1].SlotOrdinal = 2;
		CanonicalizeDependenciesAndFinalizeForTests(ScriptAbsentGap);
		AssertBehaviorFailure(ScriptAbsentGap,
			EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("Script owner absence is later than the proving 0,2 gap"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 1);

		FAngelscriptCachedTypeSchema EnvironmentPresentZeroGap =
			BuildBehaviorProductFixtureForTests(
				EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::AddRef, 2,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 1);
		for (FAngelscriptCachedBehaviorSlot& Slot :
			EnvironmentPresentZeroGap.OrderedBehaviorSlots)
		{
			Slot.DeclaringOwner = FAngelscriptStableTypeKey{};
		}
		EnvironmentPresentZeroGap.OrderedBehaviorSlots[1].SlotOrdinal = 2;
		CanonicalizeDependenciesAndFinalizeForTests(EnvironmentPresentZeroGap);
		AssertBehaviorFailure(EnvironmentPresentZeroGap,
			EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("Environment present-zero tag is later than the proving 0,2 gap"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 1);

		FAngelscriptCachedTypeSchema EnvironmentPresentNonzeroGap =
			BuildBehaviorProductFixtureForTests(
				EAngelscriptCachedTypeKind::Class,
				EAngelscriptCachedBehaviorKind::Release, 2,
				EAngelscriptCacheReferenceKind::EnvironmentSymbol, 1);
		EnvironmentPresentNonzeroGap.OrderedBehaviorSlots[1].SlotOrdinal = 2;
		CanonicalizeDependenciesAndFinalizeForTests(EnvironmentPresentNonzeroGap);
		AssertBehaviorFailure(EnvironmentPresentNonzeroGap,
			EAngelscriptCacheValidationError::OrdinalGap,
			TEXT("Environment present-nonzero tag is later than the proving 0,2 gap"),
			EAngelscriptCacheTypeSchemaTestField::BehaviorSlot, 1);
	}

	TEST_METHOD(DefaultConstructorNecessaryConditionsAndOpaqueConstructRowsAreLocal)
	{
		const auto AddSlot = [&](FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCachedBehaviorKind Kind,
			const uint8 KeyFill)
		{
			FAngelscriptCachedBehaviorSlot Slot;
			Slot.BehaviorKind = Kind;
			Slot.SlotOrdinal = 0;
			Slot.Target = MakeReference(EAngelscriptCacheReferenceKind::ScriptFunction,
				KeyFill, static_cast<uint8>(KeyFill + 1));
			Slot.DeclaringOwner = Schema.TypeKey;
			Schema.OrderedBehaviorSlots.Add(Slot);
			Schema.Dependencies.AddUnique(MakeDependency(
				EAngelscriptCacheSemanticDependencyKind::Declaration, Slot.Target));
		};
		const auto AssertInvalid = [&](const FAngelscriptCachedTypeSchema& Schema,
			const EAngelscriptCacheValidationError Error,
			const EAngelscriptCacheTypeSchemaTestField Field,
			const TCHAR* Context)
		{
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				Error, EAngelscriptCacheValidationStage::LocalSemantic, Field, Context)));
		};
		const auto AssertSuccess = [&](const FAngelscriptCachedTypeSchema& Schema,
			const TCHAR* Context)
		{
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			ASSERT_THAT(IsTrue(Outcome.Result.IsSuccess()), Context);
			ASSERT_THAT(IsTrue(Outcome.Output.IsSet()), Context);
		};

		FAngelscriptCachedTypeSchema Schema = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Class);
		Schema.TypeSemanticFlags |= static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor);
		AssertInvalid(Schema,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::TypeSemanticFlags,
			TEXT("HasDefaultConstructor requires both Class Construct and Factory aliases"));
		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		AddSlot(Schema, EAngelscriptCachedBehaviorKind::Construct, 0xb0);
		AddSlot(Schema, EAngelscriptCachedBehaviorKind::Factory, 0xb2);
		Schema.OrderedBehaviorSlots.Sort([](const auto& A, const auto& B)
		{
			return static_cast<uint8>(A.BehaviorKind)
				< static_cast<uint8>(B.BehaviorKind);
		});
		FinalizeValidFixtureHashes(Schema);
		AssertSuccess(Schema,
			TEXT("opaque Class Construct and Factory rows are locally valid without the flag"));
		Schema.TypeSemanticFlags |= static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor);
		FinalizeValidFixtureHashes(Schema);
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Payload).IsSuccess()));

		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		Schema.TypeSemanticFlags |= static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor);
		AssertInvalid(Schema,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheTypeSchemaTestField::TypeSemanticFlags,
			TEXT("Struct HasDefaultConstructor requires Construct"));
		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		AddSlot(Schema, EAngelscriptCachedBehaviorKind::Construct, 0xb4);
		FinalizeValidFixtureHashes(Schema);
		AssertSuccess(Schema,
			TEXT("opaque Struct Construct row is locally valid without the flag"));
		Schema.TypeSemanticFlags |= static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor);
		FinalizeValidFixtureHashes(Schema);
		Payload.Reset();
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Payload).IsSuccess()));
	}

	TEST_METHOD(BehaviorEntityParameterTargetAndAbiDriftBelongAfterLocalFactory)
	{
		using FDecoder = decltype(&FAngelscriptDecodedCacheRecord::TryDecode);
		static_assert(std::is_invocable_r_v<FAngelscriptCacheValidationResult,
			FDecoder, const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			TOptional<FAngelscriptDecodedCacheRecordHandle>&>);
		using FGraphValidator = decltype(&ValidateModuleSnapshotGraph);
		static_assert(std::is_invocable_r_v<FAngelscriptCacheValidationResult,
			FGraphValidator, const FAngelscriptCacheRecordId&,
			TConstArrayView<FAngelscriptDecodedCacheRecordHandle>,
			const FAngelscriptCacheModuleGraphValidationContext&,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			FAngelscriptValidatedModuleGraph&>);
		static_assert(!std::is_same_v<FDecoder, FGraphValidator>);
		// ModuleSnapshot graph RED owns wrong entity, missing target,
		// constructor parameter/default mismatch, and declaration ABI drift.
		// This local file freezes the public stage split without duplicating graph fixtures.
	}
	TEST_METHOD(EnumAliasesMetadataAndSignedInt32AuthorityAreStable)
	{
		const FAngelscriptCachedTypeSchema Schema = MakeEnumSchema();
		TArray<uint8> Bytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Bytes).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Bytes, Limits, Budget, Decoded).IsSuccess()));
		ASSERT_THAT(IsTrue(Decoded.IsSet()));
		const TArray<FAngelscriptCachedEnumEnumerator>& Enumerators =
			RequireTypeSchema(Decoded.GetValue()).KindPayload.Enum->OrderedEnumerators;
		ASSERT_THAT(AreEqual(int32(-1), Enumerators[0].Value));
		ASSERT_THAT(AreEqual(int32(-1), Enumerators[1].Value),
			TEXT("distinct enum names may deliberately alias the same signed value"));
		ASSERT_THAT(AreEqual(MAX_int32, Enumerators[2].Value));
		ASSERT_THAT(AreEqual(FString(TEXT("DisplayName")),
			Enumerators[0].Metadata[0].CanonicalKey));

		FAngelscriptCachedTypeSchema Invalid = Schema;
		Invalid.KindPayload.Enum->OrderedEnumerators[1].DeclarationOrdinal = 0;
		ASSERT_THAT(IsTrue(
			RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
				Invalid).IsSuccess()));
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DuplicateOrdinal,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::KindPayload,
			TEXT("enum duplicate declaration ordinal"))));
		Invalid = Schema;
		Invalid.KindPayload.Enum->OrderedEnumerators[1].DeclarationOrdinal = 2;
		ASSERT_THAT(IsTrue(
			RecomputeDerivedHashesForMalformedOrdinalPhysicalFixtureForTests(
				Invalid).IsSuccess()));
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::OrdinalGap,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::KindPayload,
			TEXT("enum declaration ordinal gap"))));
		Invalid = Schema;
		Invalid.KindPayload.Enum->OrderedEnumerators[1].CanonicalName = TEXT("Idle");
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DuplicateKey,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::KindPayload,
			TEXT("enum duplicate canonical name"))));

		Invalid = Schema;
		Invalid.KindPayload.Enum->EnumAuthorityHash = MakeHash(0xee);
		Outcome = DecodePhysicalOnlyFixture(Invalid);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::KindPayload,
			TEXT("enum authority hash"))));
	}
	TEST_METHOD(TypedefDescriptorAndFuncdefPropertyRulesAreFailClosed)
	{
		TArray<uint8> Bytes;
		FAngelscriptCachedTypeSchema Typedef = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Typedef);
		ASSERT_THAT(AreEqual(uint64(4), Typedef.Layout.SemanticSize));
		ASSERT_THAT(AreEqual(uint32(4), Typedef.Layout.SemanticAlignment),
			TEXT("typedef descriptor alignment stays V1TypeInfoInitialAlignment"));
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Typedef, Bytes).IsSuccess()));

		FAngelscriptCachedTypeSchema Invalid = Typedef;
		const auto AssertKindPayloadFailure = [&](const FAngelscriptCachedTypeSchema& Value,
			const EAngelscriptCacheValidationError Error,
			const TCHAR* Context)
		{
			const FMalformedDecodeOutcome Failure = DecodePhysicalOnlyFixture(Value);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Failure,
				Error, EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::KindPayload, Context)));
		};
		Invalid.KindPayload.Typedef->AliasedType =
			MakePrimitive(EAngelscriptCachedPrimitiveType::Void);
		AssertKindPayloadFailure(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef Void alias"));
		Invalid = Typedef;
		Invalid.KindPayload.Typedef->AliasedType.Kind =
			EAngelscriptCachedDataTypeKind::Auto;
		AssertKindPayloadFailure(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef Auto alias"));
		Invalid = Typedef;
		Invalid.KindPayload.Typedef->AliasedType.QualifierFlags =
			static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		AssertKindPayloadFailure(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef qualified alias"));
		Invalid = Typedef;
		Invalid.KindPayload.Typedef->AliasedType.OrderedSubTypes.Add(
			MakePrimitive(EAngelscriptCachedPrimitiveType::Int32));
		AssertKindPayloadFailure(Invalid,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			TEXT("Typedef non-zero subtype array is physically representable hostile input"));

		bool bPrimitiveTraversalSuccess = false;
		const FAngelscriptCachedTypeSchema PrimitiveTraversal =
			MakeTypeSchemaAllocationFixture(9, 17, 3, bPrimitiveTraversalSuccess);
		ASSERT_THAT(IsTrue(bPrimitiveTraversalSuccess));
		ASSERT_THAT(IsTrue(
			PrimitiveTraversal.KindPayload.Typedef.IsSet()));
		ASSERT_THAT(AreEqual(EAngelscriptCachedDataTypeKind::Primitive,
			PrimitiveTraversal.KindPayload.Typedef->AliasedType.Kind));
		ASSERT_THAT(IsTrue(
			PrimitiveTraversal.KindPayload.Typedef->AliasedType.OrderedSubTypes.IsEmpty()),
			TEXT("legal primitive alias traversal emits no subtype allocation event"));
		TArray<uint8> PrimitiveTraversalPayload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			PrimitiveTraversal, PrimitiveTraversalPayload).IsSuccess()));
		FAngelscriptCacheReadBudget PrimitiveTraversalBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> PrimitiveTraversalOutput;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(PrimitiveTraversalPayload,
			FAngelscriptCacheReadLimits{}, PrimitiveTraversalBudget,
			PrimitiveTraversalOutput).IsSuccess()),
			TEXT("legal Typedef primitive traversal remains a common-factory success"));
		ASSERT_THAT(IsTrue(PrimitiveTraversalOutput.IsSet()));

		FAngelscriptCachedTypeSchema Funcdef = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Funcdef);
		ASSERT_THAT(AreEqual(uint64(0), Funcdef.Layout.SemanticSize));
		ASSERT_THAT(AreEqual(uint32(4), Funcdef.Layout.SemanticAlignment));
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Funcdef, Bytes).IsSuccess()));
		Invalid = Funcdef;
		Invalid.Layout.SemanticSize = 8;
		{
			const FMalformedDecodeOutcome Failure = DecodePhysicalOnlyFixture(Invalid);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Failure,
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::Layout,
				TEXT("Funcdef descriptor layout"))));
		}
		Invalid = Funcdef;
		Invalid.KindPayload.Callable->SignatureFunctionKey = {};
		AssertKindPayloadFailure(Invalid,
			EAngelscriptCacheValidationError::ZeroStableKey,
			TEXT("Funcdef signature key"));
		Invalid = Funcdef;
		Invalid.KindPayload.Callable->ExpectedSignatureAbi = {};
		AssertKindPayloadFailure(Invalid,
			EAngelscriptCacheValidationError::MissingExpectedAbi,
			TEXT("Funcdef signature expected ABI"));
		Invalid = Funcdef;
		Invalid.OrderedProperties.Add(MakeCompleteDelegateSchema().OrderedProperties[0]);
		FinalizeValidFixtureHashes(Invalid);
		{
			const FMalformedDecodeOutcome Failure = DecodePhysicalOnlyFixture(Invalid);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Failure,
				EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
				TEXT("zero-sized Funcdef storage is never a V1 property owner"), 0)));
		}
	}
	TEST_METHOD(PhysicalExhaustionAndTrailingDataPrecedeSemanticHashes)
	{
		const FAngelscriptCachedTypeSchema Schema = MakeCompleteDelegateSchema();
		TArray<uint8> Payload;
		FAngelscriptCacheTypeSchemaTestWireTrace Trace;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				Schema, Payload, Trace).IsSuccess()));
		const uint64 LayoutFieldOffset = RequireIndependentSpanOffsetForTests(
			Payload, EAngelscriptCacheTypeSchemaTestField::Layout);

		TArray<uint8> Corrupt = Payload;
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::TypeLayoutHash, 0xee);
		Corrupt.Add(0xfe);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Out = MakeSentinelRecord();
		FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
			Corrupt, Limits, Budget, Out);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Out,
			EAngelscriptCacheValidationError::TrailingData,
			EAngelscriptCacheValidationStage::PayloadDecode,
			static_cast<uint64>(Payload.Num()),
			TEXT("trailing data wins before TypeLayoutHash mismatch"))));

		Corrupt.Pop();
		Result = DecodeWithMatchingRecordId(Corrupt, Limits, Budget, Out);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Out,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::LocalSemantic,
			LayoutFieldOffset, TEXT("TypeLayoutHash mismatch after exhaustion"))));

		Corrupt = Payload;
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::PayloadSchemaVersion, 3);
		Result = DecodeWithMatchingRecordId(Corrupt, Limits, Budget, Out);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Out,
			EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
			EAngelscriptCacheValidationStage::PayloadDecode,
			uint64(0), TEXT("payload schema version"))));
		Corrupt = Payload;
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::TypeKind, 0);
		Result = DecodeWithMatchingRecordId(Corrupt, Limits, Budget, Out);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Out,
			EAngelscriptCacheValidationError::UnknownEnumValue,
			EAngelscriptCacheValidationStage::PayloadDecode,
			RequireIndependentSpanOffsetForTests(
				Payload, EAngelscriptCacheTypeSchemaTestField::TypeKind),
			TEXT("unknown TypeKind physical scalar"))));

		FAngelscriptCachedTypeSchema Enum = MakeEnumSchema();
		Enum.KindPayload.Enum->EnumAuthorityHash = MakeHash(0xea);
		TArray<uint8> EnumPayload;
		FAngelscriptCacheTypeSchemaTestWireTrace EnumTrace;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				Enum, EnumPayload, EnumTrace).IsSuccess()));
		const uint64 EnumEnd = EnumPayload.Num();
		EnumPayload.Add(0xef);
		FAngelscriptCacheReadBudget EnumBudget;
		Out = MakeSentinelRecord();
		Result = DecodeWithMatchingRecordId(EnumPayload,
			FAngelscriptCacheReadLimits{}, EnumBudget, Out);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Out,
			EAngelscriptCacheValidationError::TrailingData,
			EAngelscriptCacheValidationStage::PayloadDecode,
			EnumEnd, TEXT("trailing data wins before EnumAuthorityHash mismatch"))));
	}
	TEST_METHOD(DerivedHashAndFieldLocalValidationOrderIsDeterministic)
	{
		const FAngelscriptCachedTypeSchema Schema = MakeCompleteDelegateSchema();
		TArray<uint8> Payload;
		FAngelscriptCacheTypeSchemaTestWireTrace Trace;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				Schema, Payload, Trace).IsSuccess()));
		const uint64 PropertyFieldOffset = RequireIndependentSpanOffsetForTests(
			Payload, EAngelscriptCacheTypeSchemaTestField::OrderedProperty, 0);

		TArray<uint8> Corrupt = Payload;
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::PropertyStorageLayoutHash, 0xee, 0);
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::PropertyLayoutFingerprint, 0xed, 0);
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::TypeLayoutHash, 0xec);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Out = MakeSentinelRecord();
		FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
			Corrupt, Limits, Budget, Out);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Out,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::LocalSemantic,
			PropertyFieldOffset,
			TEXT("StorageLayoutHash wins before property fingerprint/type hash"))));

		Corrupt = Payload;
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::PropertyLayoutFingerprint, 0xed, 0);
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::TypeLayoutHash, 0xec);
		Out = MakeSentinelRecord();
		Result = DecodeWithMatchingRecordId(Corrupt, Limits, Budget, Out);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Out,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::LocalSemantic,
			PropertyFieldOffset,
			TEXT("PropertyLayoutFingerprint wins before TypeLayoutHash"))));

		const FAngelscriptCachedTypeSchema EnumSchema = MakeEnumSchema();
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				EnumSchema, Payload, Trace).IsSuccess()));
		Corrupt = Payload;
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::EnumAuthorityHash, 0xee);
		PatchRawByte(Corrupt, Trace,
			EAngelscriptCacheTypeSchemaTestField::TypeLayoutHash, 0xed);
		Out = MakeSentinelRecord();
		Result = DecodeWithMatchingRecordId(Corrupt, Limits, Budget, Out);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Out,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::LocalSemantic,
			RequireIndependentSpanOffsetForTests(
				Payload, EAngelscriptCacheTypeSchemaTestField::KindPayload),
			TEXT("EnumAuthorityHash wins before TypeLayoutHash"))));
	}

	TEST_METHOD(FieldLocalAndLayoutReplayPrecedencePairsHaveExactCoordinates)
	{
		FAngelscriptCachedTypeSchema Schema = MakePropertyOwnerFixture(
			EPropertyOwnerFormForTests::OrdinaryUClass);
		Schema.LayoutInputs[0].LayoutInputHash = MakeHash(0xee);
		Schema.OrderedProperties[0].PropertyLayoutFingerprint = MakeHash(0xed);
		FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::LayoutInput,
			TEXT("LayoutInputHash mismatch wins before PropertyLayoutFingerprint"), 0)));

		Schema = MakePropertyOwnerFixture(EPropertyOwnerFormForTests::OrdinaryUClass);
		Schema.OrderedProperties[0].PropertyLayoutFingerprint = MakeHash(0xee);
		Algo::Reverse(Schema.Dependencies);
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("PropertyLayoutFingerprint mismatch wins before dependencies"), 0)));

		Schema = MakePropertyOwnerFixture(EPropertyOwnerFormForTests::OrdinaryUClass);
		Schema.OrderedProperties[0].SemanticByteOffset = 4;
		RehashSelfConsistentWrongPropertyOffset(Schema, 0);
		Algo::Reverse(Schema.Dependencies);
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::NonCanonicalOrder,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Dependency,
			TEXT("field-locally-valid wrong property offset loses to dependencies"), 1)));

		Schema = MakeEnumSchema();
		Schema.Metadata = {
			{TEXT("Zulu"), TEXT("Later")},
			{TEXT("Alpha"), TEXT("Earlier")},
		};
		Schema.KindPayload.Enum->OrderedEnumerators[1].CanonicalName =
			Schema.KindPayload.Enum->OrderedEnumerators[0].CanonicalName;
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::NonCanonicalOrder,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::MetadataEntry,
			TEXT("noncanonical Metadata wins before representable selected Enum arm fault"), 1)));

		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Class);
		Schema.TypeSemanticFlags |= 0x100u;
		Schema.TypeSemanticFlags &= ~static_cast<uint32>(
			EAngelscriptCachedTypeSemanticFlags::ReferenceType);
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::UnknownFlags,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::TypeSemanticFlags,
			TEXT("unknown type flag wins before missing required ReferenceType flag"))));

		Schema = MakeEnumSchema();
		Schema.Reflection.ConfigName = TEXT("ForbiddenForEnum");
		FinalizeValidFixtureHashes(Schema);
		const FMalformedDecodeOutcome LaterPresenceOnly =
			DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
			LaterPresenceOnly, EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Reflection,
			TEXT("complete Enum+UEnum tuple with representable forbidden ConfigName"))));

		TArray<uint8> InvalidUtf8Payload;
		FAngelscriptCacheTypeSchemaTestWireTrace InvalidUtf8Trace;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				Schema, InvalidUtf8Payload, InvalidUtf8Trace).IsSuccess()));
		const FIndependentTypeSchemaWireInventoryForTests InvalidUtf8Inventory =
			ScanIndependentTypeSchemaWireForTests(InvalidUtf8Payload);
		ASSERT_THAT(IsTrue(InvalidUtf8Inventory.bComplete));
		const uint64 IndependentNameFieldOffset = InvalidUtf8Inventory.FindCapturedOffset({
			EAngelscriptTypeSchemaCapturedField::CanonicalName}).GetValue();
		PatchRawByte(InvalidUtf8Payload, InvalidUtf8Trace,
			EAngelscriptCacheTypeSchemaTestField::CanonicalNameBytes, 0xff);
		FAngelscriptCacheReadBudget InvalidUtf8Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> InvalidUtf8Output =
			MakeSentinelRecord();
		const FAngelscriptCacheValidationResult InvalidUtf8Result =
			DecodeWithMatchingRecordId(InvalidUtf8Payload,
				FAngelscriptCacheReadLimits{}, InvalidUtf8Budget, InvalidUtf8Output);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
			InvalidUtf8Result, InvalidUtf8Output,
			EAngelscriptCacheValidationError::InvalidUtf8,
			EAngelscriptCacheValidationStage::PayloadDecode,
			IndependentNameFieldOffset,
			TEXT("invalid UTF-8 wins before later representable Reflection presence fault"))));

		Schema = MakeOrdinaryUClassSchema(true);
		Algo::Reverse(Schema.Relations);
		Schema.Layout.TypeLayoutHash = MakeHash(0xac);
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::NonCanonicalOrder,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Relations,
			TEXT("relation noncanonical order wins before TypeLayoutHash mismatch"), 0)));

		Schema = MakeThreePropertyLayoutSchema();
		for (FAngelscriptCachedPropertySchema& Property : Schema.OrderedProperties)
		{
			++Property.LayoutOrdinal;
		}
		Schema.OrderedProperties[0].PropertyLayoutFingerprint = MakeHash(0xad);
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::OrdinalGap,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::OrderedProperty,
			TEXT("property ordinal gap wins before PropertyLayoutFingerprint mismatch"), 0)));

		Schema = MakeMinimalSchema(EAngelscriptCachedTypeKind::Struct);
		Schema.Reflection.ReflectionKind = EAngelscriptCachedReflectionKind::UClass;
		Schema.Layout.TypeLayoutHash = MakeHash(0xae);
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::Reflection,
			TEXT("wrong reflection form wins before TypeLayoutHash mismatch"))));

		Schema = MakeEnumSchema();
		Schema.KindPayload.Enum->EnumAuthorityHash = MakeHash(0xaf);
		Schema.Layout.TypeLayoutHash = MakeHash(0xb0);
		Outcome = DecodePhysicalOnlyFixture(Schema);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::LocalSemantic,
			EAngelscriptCacheTypeSchemaTestField::KindPayload,
			TEXT("EnumAuthorityHash mismatch wins before TypeLayoutHash mismatch"))));
	}
	TEST_METHOD(PrimitiveAndObjectHandleBuildLayoutTableIsFrozen)
	{
		const FAngelscriptCacheV1BuildLayoutConstants& Constants =
			FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants();
		ASSERT_THAT(AreEqual(uint32(AS_SIZEOF_BOOL), Constants.AsSizeOfBool));
		ASSERT_THAT(AreEqual(uint32(4 * AS_PTR_SIZE), Constants.PointerByteWidth));
		ASSERT_THAT(AreEqual(uint32(alignof(asINT64)), Constants.Int64Alignment));
		ASSERT_THAT(AreEqual(uint32(alignof(double)), Constants.DoubleAlignment));
		ASSERT_THAT(AreEqual(uint32(8), Constants.ObjectHandleAlignment));
		ASSERT_THAT(AreEqual(uint32(8), Constants.ObjectInitialAlignment));
		ASSERT_THAT(AreEqual(uint32(4), Constants.TypeInfoInitialAlignment));

		struct FExpected
		{
			EAngelscriptCachedPrimitiveType Primitive;
			uint32 Size;
			uint32 Alignment;
		};
		const FExpected Expected[] = {
			{EAngelscriptCachedPrimitiveType::Bool, AS_SIZEOF_BOOL, 1},
			{EAngelscriptCachedPrimitiveType::Int8, 1, 1},
			{EAngelscriptCachedPrimitiveType::UInt8, 1, 1},
			{EAngelscriptCachedPrimitiveType::Int16, 2, 2},
			{EAngelscriptCachedPrimitiveType::UInt16, 2, 2},
			{EAngelscriptCachedPrimitiveType::Int32, 4, 4},
			{EAngelscriptCachedPrimitiveType::UInt32, 4, 4},
			{EAngelscriptCachedPrimitiveType::Float32, 4, 4},
			{EAngelscriptCachedPrimitiveType::Int64, 8, alignof(asINT64)},
			{EAngelscriptCachedPrimitiveType::UInt64, 8, alignof(asINT64)},
			{EAngelscriptCachedPrimitiveType::Float64, 8, alignof(double)},
		};
		for (const FExpected& Row : Expected)
		{
			FAngelscriptCacheV1StorageLayout Actual;
			ASSERT_THAT(IsTrue(Constants.TryGetPrimitiveStorageLayout(
				Row.Primitive, Actual)));
			ASSERT_THAT(AreEqual(Row.Size, Actual.SemanticStorageSize));
			ASSERT_THAT(AreEqual(Row.Alignment, Actual.SemanticStorageAlignment));
		}
		FAngelscriptCacheV1StorageLayout Invalid;
		ASSERT_THAT(IsFalse(Constants.TryGetPrimitiveStorageLayout(
			EAngelscriptCachedPrimitiveType::Void, Invalid)));
		const FAngelscriptCacheV1StorageLayout Handle =
			Constants.GetObjectHandleStorageLayout();
		ASSERT_THAT(AreEqual(uint32(4 * AS_PTR_SIZE), Handle.SemanticStorageSize));
		ASSERT_THAT(AreEqual(uint32(8), Handle.SemanticStorageAlignment));
	}
	TEST_METHOD(CommonRecordFactoryLocalDecodeNeverAcceptsCurrentResolvers)
	{
		using FDecoder = decltype(&FAngelscriptDecodedCacheRecord::TryDecode);
		static_assert(std::is_invocable_r_v<FAngelscriptCacheValidationResult,
			FDecoder, const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			TOptional<FAngelscriptDecodedCacheRecordHandle>&>);
		static_assert(!std::is_invocable_v<FDecoder,
			const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			const IAngelscriptCacheCurrentLayoutResolver&,
			TOptional<FAngelscriptDecodedCacheRecordHandle>&>);
		static_assert(!std::is_invocable_v<FDecoder,
			const FAngelscriptCacheRecordId&, TConstArrayView<uint8>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			const IAngelscriptCacheProspectiveTypeLayoutView&,
			TOptional<FAngelscriptDecodedCacheRecordHandle>&>);
		static_assert(std::is_trivially_copyable_v<FAngelscriptCacheProspectiveTypeLayout>);
		static_assert(std::is_abstract_v<IAngelscriptCacheProspectiveTypeLayoutView>);
		static_assert(std::is_abstract_v<IAngelscriptCacheCurrentLayoutResolver>);

		const FAngelscriptCachedTypeSchema Schema = MakeCompleteDelegateSchema();
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			Schema, Payload).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Out;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Payload, Limits, Budget, Out).IsSuccess()));
		ASSERT_THAT(IsTrue(Out.IsSet()));
		ASSERT_THAT(IsTrue(Schema.TypeKey
			== RequireTypeSchema(Out.GetValue()).TypeKey));
	}

	TEST_METHOD(TsScrSiteTemplateTableIsFixedUniqueAndDispositionComplete)
	{
		static_assert(TsScrExpectedSiteCountForTests == 56);
		TArray<uint8> Frequencies;
		Frequencies.Init(0, TsScrExpectedSiteCountForTests);
		int32 RequiredCount = 0;
		int32 StreamingZeroCount = 0;
		int32 InvalidFixtureOnlyCount = 0;
		for (const FTsScrAllocationSiteTemplateForTests& Template :
			GetIndependentTsScrSiteTemplatesForTests())
		{
			const int32 SiteIndex = static_cast<int32>(Template.SiteKind);
			ASSERT_THAT(IsTrue(SiteIndex >= 0
				&& SiteIndex < TsScrExpectedSiteCountForTests));
			++Frequencies[SiteIndex];
			ASSERT_THAT(AreEqual(Template.SiteKind,
				TsScrSiteRepresentativeAuthoritiesForTests[SiteIndex].SiteKind));
			ASSERT_THAT(AreEqual(Template.Disposition,
				TsScrSiteRepresentativeAuthoritiesForTests[SiteIndex].Disposition),
				TEXT("named representative authority and site template share disposition"));
			switch (Template.Disposition)
			{
			case ETsScrSiteDispositionForTests::Required:
				++RequiredCount;
				ASSERT_THAT(AreEqual(
					ETsScrSiteLifetimeForTests::CandidateTemporary,
					Template.Lifetime));
				break;
			case ETsScrSiteDispositionForTests::StreamingZero:
				++StreamingZeroCount;
				ASSERT_THAT(AreEqual(
					ETsScrSiteLifetimeForTests::StreamingTemporary,
					Template.Lifetime));
				break;
			case ETsScrSiteDispositionForTests::InvalidFixtureOnly:
				++InvalidFixtureOnlyCount;
				break;
			default:
				ASSERT_THAT(IsTrue(false, TEXT("unknown TS-SCR disposition")));
				break;
			}
		}
		for (const uint8 Frequency : Frequencies)
		{
			ASSERT_THAT(AreEqual(uint8(1), Frequency,
				TEXT("every SiteKind has exactly one fixed template")));
		}
		ASSERT_THAT(AreEqual(43, RequiredCount));
		ASSERT_THAT(AreEqual(12, StreamingZeroCount));
		ASSERT_THAT(AreEqual(1, InvalidFixtureOnlyCount));
		ASSERT_THAT(AreEqual(ETsScrSiteDispositionForTests::InvalidFixtureOnly,
			FindTsScrTemplateForTests(
				ETsScrAllocationSiteKindForTests::TypedefDataTypeOrderedSubTypesArray)
				.Disposition),
			TEXT("a non-zero Typedef subtype array is hostile-input storage, not Required"));
	}

	TEST_METHOD(TsScrHostileTypedefSubtypeAllocationIsExactAndNeverRequired)
	{
		const FTsScrAllocationSiteTemplateForTests& TargetTemplate =
			FindTsScrTemplateForTests(
				TsScrHostileTypedefAuthorityForTests.TargetCoordinate.SiteKind);
		ASSERT_THAT(AreEqual(ETsScrSiteDispositionForTests::InvalidFixtureOnly,
			TargetTemplate.Disposition));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PayloadDecode,
			TargetTemplate.ExpectedStage));

		constexpr int32 RequestedSubTypes =
			TsScrHostileTypedefAuthorityForTests.RequestedSubTypes;
		constexpr int32 TargetIndex =
			TsScrHostileTypedefAuthorityForTests.TargetProbeOccurrenceIndex;
		const int32 PreviousCapacity =
			CalculateIndependentArrayReserveCapacityForTests<
				FAngelscriptCachedDataType>(RequestedSubTypes - 1);
		const int32 ReservedCapacity =
			CalculateIndependentArrayReserveCapacityForTests<
				FAngelscriptCachedDataType>(RequestedSubTypes);
		ASSERT_THAT(IsTrue(ReservedCapacity != PreviousCapacity),
			TEXT("literal hostile Typedef cardinality remains an allocator transition"));

		FAngelscriptCachedTypeSchema Hostile = MakeMinimalSchema(
			EAngelscriptCachedTypeKind::Typedef);
		for (int32 Index = 0; Index < RequestedSubTypes; ++Index)
		{
			Hostile.KindPayload.Typedef->AliasedType.OrderedSubTypes.Add(
				MakePrimitive(EAngelscriptCachedPrimitiveType::Int32));
		}
		TArray<uint8> Payload;
		FAngelscriptCacheTypeSchemaTestWireTrace WriterTrace;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchemaPhysicalForTests(
				Hostile, Payload, WriterTrace).IsSuccess()));
		const FIndependentTypeSchemaWireInventoryForTests Inventory =
			ScanIndependentTypeSchemaWireForTests(Payload);
		ASSERT_THAT(IsTrue(Inventory.bComplete));
		const FIndependentTsScrExpectedPlanForTests PlanWithoutHostileTarget =
			BuildIndependentTsScrPlanForTests(Hostile, Payload, Inventory);

		const TMap<FTsScrPlanCoordinateKeyForTests, int32> EventIndexByCoordinate =
			BuildUniqueTsScrPlanCoordinateIndexForTests(PlanWithoutHostileTarget);
		const int32* FollowingOccurrence = EventIndexByCoordinate.Find(
			TsScrHostileTypedefAuthorityForTests.FollowingPlanCoordinate);
		ASSERT_THAT(IsNotNull(FollowingOccurrence));
		if (FollowingOccurrence == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TargetIndex, *FollowingOccurrence));
		ASSERT_THAT(IsTrue(PlanWithoutHostileTarget.Events.IsValidIndex(TargetIndex)));
		if (!PlanWithoutHostileTarget.Events.IsValidIndex(TargetIndex))
		{
			return;
		}

		const uint64 ExactTargetBytes =
			uint64(ReservedCapacity) * sizeof(FAngelscriptCachedDataType);
		ASSERT_THAT(IsTrue(ReservedCapacity >= RequestedSubTypes));
		ASSERT_THAT(IsTrue(ExactTargetBytes > 0));
		const uint64 TargetOffset = FindIndependentWireOffsetForTests(
			Inventory, EAngelscriptCacheTypeSchemaTestField::DataTypeOrderedSubTypes,
			TsScrHostileTypedefAuthorityForTests.TargetCoordinate.PrimaryIndex,
			TsScrHostileTypedefAuthorityForTests.TargetCoordinate.SecondaryIndex,
			TsScrHostileTypedefAuthorityForTests.TargetCoordinate.TertiaryIndex);
		const uint64 KindPayloadOffset = FindIndependentWireOffsetForTests(
			Inventory, EAngelscriptCacheTypeSchemaTestField::KindPayload);
		const FTsScrExpectedAllocationEventForTests& FollowingEvent =
			PlanWithoutHostileTarget.Events[TargetIndex];
		ASSERT_THAT(AreEqual(
			TsScrHostileTypedefAuthorityForTests.FollowingPlanCoordinate.SiteKind,
			FollowingEvent.SiteKind));
		ASSERT_THAT(AreEqual(
			TsScrHostileTypedefAuthorityForTests.FollowingPlanCoordinate.PrimaryIndex,
			FollowingEvent.PrimaryIndex));
		ASSERT_THAT(AreEqual(
			TsScrHostileTypedefAuthorityForTests.FollowingPlanCoordinate.SecondaryIndex,
			FollowingEvent.SecondaryIndex));
		ASSERT_THAT(AreEqual(
			TsScrHostileTypedefAuthorityForTests.FollowingPlanCoordinate.TertiaryIndex,
			FollowingEvent.TertiaryIndex));
		const uint64 TargetTotalPrefix = FollowingEvent.TotalPrefixBefore;
		const uint64 TargetTemporaryPrefix = FollowingEvent.TemporaryPrefixBefore;
		const uint64 ExactTotalBytes =
			PlanWithoutHostileTarget.ExactTotalBytes + ExactTargetBytes;
		const uint64 ExactPeakLiveBytes =
			PlanWithoutHostileTarget.PeakLiveBytes + ExactTargetBytes;

		FAngelscriptCacheReadLimits ExactLimits;
		ExactLimits.MaxTotalDecodedBytes = ExactTotalBytes;
		ExactLimits.MaxResidentDecodedBytes = ExactPeakLiveBytes;
		FTsScrCallerOwnedProbeCaptureForTests<> ExactCapture;
		FAngelscriptCacheReadBudget ExactBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> ExactOutput =
			MakeSentinelRecord();
		const FAngelscriptCacheValidationResult ExactResult =
			DecodeWithMatchingRecordIdAndProbe(Payload, ExactLimits, ExactBudget,
				ExactCapture.Probe, ExactOutput);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
			ExactResult, ExactOutput,
			EAngelscriptCacheValidationError::InvalidQualifierCombination,
			EAngelscriptCacheValidationStage::LocalSemantic, KindPayloadOffset,
			TEXT("hostile primitive Typedef subtype exact allocation then semantic rejection"))));
		const FTsScrFilteredProbeEventViewForTests ExactEvents =
			ExactCapture.GetAllocationEvents();
		ASSERT_THAT(AreEqual(PlanWithoutHostileTarget.Events.Num() + 1,
			ExactEvents.Num()));
		const FAngelscriptCacheTypeSchemaProbeEventForTests& ExactTarget =
			ExactEvents.FindAtForConstantLookup(TargetIndex);
		ASSERT_THAT(AreEqual(RequestedSubTypes, ExactTarget.RequestedElementCount));
		ASSERT_THAT(AreEqual(uint64(sizeof(FAngelscriptCachedDataType)),
			ExactTarget.ElementSize));
		ASSERT_THAT(AreEqual(uint64(alignof(FAngelscriptCachedDataType)),
			ExactTarget.ElementAlignment));
		ASSERT_THAT(AreEqual(ReservedCapacity, ExactTarget.ReservedCapacity));
		ASSERT_THAT(AreEqual(ExactTargetBytes, ExactTarget.AllocatedBytes));
		ASSERT_THAT(AreEqual(ExactTargetBytes, ExactTarget.TotalChargeBytes));
		ASSERT_THAT(AreEqual(uint64(0), ExactTarget.ResidentChargeBytes));
		ASSERT_THAT(AreEqual(ExactTargetBytes, ExactTarget.TemporaryChargeBytes));
		ASSERT_THAT(AreEqual(0, ExactCapture.GetPromotionCheckpoints().Num()));
		ASSERT_THAT(AreEqual(ExactTotalBytes, ExactBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0), ExactBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0),
			ExactBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(ExactPeakLiveBytes,
			ExactBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(uint64(0), ExactCapture.Probe.GetLiveAllocatedBytes()));
		ASSERT_THAT(AreEqual(int64(0), ExactCapture.Probe.GetAllocationBalance()));
		ASSERT_THAT(IsFalse(ExactCapture.bOverflowed));

		const auto RunOneShort = [&](const bool bCombinedLiveDimension)
		{
			FAngelscriptCacheReadLimits Limits;
			Limits.MaxTotalDecodedBytes = bCombinedLiveDimension
				? MAX_uint64
				: TargetTotalPrefix + ExactTargetBytes - 1;
			Limits.MaxResidentDecodedBytes = bCombinedLiveDimension
				? TargetTemporaryPrefix + ExactTargetBytes - 1
				: MAX_uint64;
			FTsScrCallerOwnedProbeCaptureForTests<> Capture;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output =
				MakeSentinelRecord();
			const FAngelscriptCacheValidationResult Result =
				DecodeWithMatchingRecordIdAndProbe(
					Payload, Limits, Budget, Capture.Probe, Output);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
				Result, Output, EAngelscriptCacheValidationError::BudgetExceeded,
				EAngelscriptCacheValidationStage::PayloadDecode, TargetOffset,
				bCombinedLiveDimension
					? TEXT("hostile Typedef subtype combined-live one-byte-short")
					: TEXT("hostile Typedef subtype Total one-byte-short"))));
			ASSERT_THAT(AreEqual(TargetIndex, Capture.GetAllocationEvents().Num(),
				TEXT("hostile target has no accepted allocation event")));
			ASSERT_THAT(AreEqual(uint64(TargetIndex),
				Capture.Probe.GetTotalAllocationAttempts(),
				TEXT("hostile target reservation rejects before allocator entry")));
			ASSERT_THAT(AreEqual(uint64(1),
				Capture.Probe.GetRejectedReservationCount()));
			ASSERT_THAT(AreEqual(0, Capture.GetPromotionCheckpoints().Num()));
			ASSERT_THAT(AreEqual(TargetTotalPrefix, Budget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(uint64(0), Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(uint64(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(TargetTemporaryPrefix,
				Budget.GetPeakLiveResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(uint64(0), Capture.Probe.GetLiveAllocatedBytes()));
			ASSERT_THAT(AreEqual(int64(0), Capture.Probe.GetAllocationBalance()));
			ASSERT_THAT(IsFalse(Capture.bOverflowed));
		};
		RunOneShort(false);
		RunOneShort(true);
	}

	TEST_METHOD(TsScrDependencyRequiredClosureIsExactlyTheFiveLocalAuthorities)
	{
		int32 RequiredSuccessCount = 0;
		int32 ExcludedFailureCount = 0;
		TestRunner->AddInfo(TEXT(
			"[CacheV2][TypeSchema][Dependencies][Decoder] begin "
			"raw-kinds=11 required=5 excluded=6"));
		for (uint32 Variant = 0;
			Variant < GetTypeSchemaAllocationVariantCount(11); ++Variant)
		{
			const EAngelscriptCacheSemanticDependencyKind Kind =
				static_cast<EAngelscriptCacheSemanticDependencyKind>(Variant + 1);
			const bool bRequired = IsRequiredTypeSchemaDependencyKindForTests(Kind);
			bool bExpectedLocalSuccess = false;
			FAngelscriptCachedTypeSchema Schema = MakeTypeSchemaAllocationFixture(
				11, 1, Variant, bExpectedLocalSuccess);
			ASSERT_THAT(AreEqual(bRequired, bExpectedLocalSuccess),
				TEXT("only Declaration/Signature/Inheritance/ValueLayout/EnvironmentAbi enter Required"));

			if (bRequired)
			{
				TArray<uint8> Payload;
				ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
					Schema, Payload).IsSuccess()));
				FAngelscriptCacheReadBudget Budget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
				ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(Payload,
					FAngelscriptCacheReadLimits{}, Budget, Output).IsSuccess()));
				ASSERT_THAT(IsTrue(Output.IsSet()));
				++RequiredSuccessCount;
				continue;
			}

			ASSERT_THAT(IsFalse(Schema.Dependencies.IsEmpty()),
				TEXT("every excluded common kind retains a physical semantic-rejection row"));
			FinalizeValidFixtureHashes(Schema);
			const FMalformedDecodeOutcome Outcome = DecodePhysicalOnlyFixture(Schema);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Outcome,
				EAngelscriptCacheValidationError::UnexpectedRecord,
				EAngelscriptCacheValidationStage::LocalSemantic,
				EAngelscriptCacheTypeSchemaTestField::Dependency,
				TEXT("wire-valid non-local dependency is outside TypeSchema authority"),
				0)));
			++ExcludedFailureCount;
		}
		ASSERT_THAT(AreEqual(5, RequiredSuccessCount,
			TEXT("TypeSchema required dependency-kind count")));
		ASSERT_THAT(AreEqual(6, ExcludedFailureCount,
			TEXT("TypeSchema excluded dependency-kind count")));
		TestRunner->AddInfo(FString::Printf(TEXT(
			"[CacheV2][TypeSchema][Dependencies][Decoder] complete "
			"required=%d excluded=%d total=%d"), RequiredSuccessCount,
			ExcludedFailureCount, RequiredSuccessCount + ExcludedFailureCount));
	}

	TEST_METHOD(TsScrRepresentativeFixturesRemainLocallySerializable)
	{
		int32 SuccessfulFixtureCount = 0;
		for (int32 FixtureIndex = 0;
			FixtureIndex < TsScrExpectedRepresentativeFixtureCountForTests;
			++FixtureIndex)
		{
			const FTsScrRepresentativeFixtureAuthorityForTests& Authority =
				TsScrRepresentativeFixtureAuthoritiesForTests[FixtureIndex];
			bool bExpectedLocalSuccess = false;
			FAngelscriptCachedTypeSchema Schema = MakeTypeSchemaAllocationFixture(
				Authority.Family, Authority.Cardinality, Authority.Variant,
				bExpectedLocalSuccess);
			ASSERT_THAT(IsTrue(bExpectedLocalSuccess),
				*FString::Printf(TEXT(
					"fixture=%d family=%u variant=%u cardinality=%u base fixture is valid"),
					FixtureIndex, static_cast<uint32>(Authority.Family),
					Authority.Variant, Authority.Cardinality));
			ApplyTsScrRepresentativeFixtureCustomizationForTests(Authority, Schema);
			TArray<uint8> Payload;
			const FAngelscriptCacheValidationResult Result =
				FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
					Schema, Payload);
			ASSERT_THAT(IsTrue(Result.IsSuccess()),
				*FString::Printf(TEXT(
					"fixture=%d family=%u variant=%u cardinality=%u "
					"error=%u stage=%u offset=%llu remains locally serializable"),
					FixtureIndex, static_cast<uint32>(Authority.Family),
					Authority.Variant, Authority.Cardinality,
					static_cast<uint32>(Result.Error),
					static_cast<uint32>(Result.Stage), Result.ByteOffset));
			if (Result.IsSuccess())
			{
				++SuccessfulFixtureCount;
			}
		}
		ASSERT_THAT(AreEqual(TsScrExpectedRepresentativeFixtureCountForTests,
			SuccessfulFixtureCount,
			TEXT("Every frozen TS-SCR representative fixture serializes")));
	}

	TEST_METHOD(TsScrExpandedCasesFreezeActualSlackOccurrencesAndRecursion)
	{
		const TArray<FTsScrExpandedAllocationCaseForTests>& Cases =
			GetIndependentTsScrExpandedCasesForTests();
		const TArray<FTsScrResolvedRepresentativeFixtureForTests>& Fixtures =
			GetResolvedTsScrRepresentativeFixturesForTests();
		ASSERT_THAT(AreEqual(TsScrExpectedRepresentativeFixtureCountForTests,
			Fixtures.Num()));
		ASSERT_THAT(AreEqual(TsScrExpectedRepresentativeTargetCountForTests,
			Cases.Num()));
		ASSERT_THAT(AreEqual(uint32(0), Fixtures[static_cast<int32>(
			ETsScrRepresentativeFixtureIdForTests::ShapeEmpty)].Authority.Cardinality));
		ASSERT_THAT(AreEqual(uint32(1), Fixtures[static_cast<int32>(
			ETsScrRepresentativeFixtureIdForTests::ShapeOne)].Authority.Cardinality));
		ASSERT_THAT(AreEqual(uint32(32), Fixtures[static_cast<int32>(
			ETsScrRepresentativeFixtureIdForTests::ShapeMany)].Authority.Cardinality));
		int32 CountsBySite[TsScrExpectedSiteCountForTests] = {};
		TArray<bool> SiteHasActualSlackBoundary;
		SiteHasActualSlackBoundary.Init(false, TsScrExpectedSiteCountForTests);
		int32 ResolvedFixtureTargetCount = 0;
		for (int32 FixtureIndex = 0; FixtureIndex < Fixtures.Num(); ++FixtureIndex)
		{
			const FTsScrResolvedRepresentativeFixtureForTests& Fixture =
				Fixtures[FixtureIndex];
			ASSERT_THAT(AreEqual(FixtureIndex,
				static_cast<int32>(Fixture.Authority.FixtureId)));
			for (int32 PreviousIndex = 0;
				PreviousIndex < FixtureIndex; ++PreviousIndex)
			{
				const FTsScrResolvedRepresentativeFixtureForTests& Previous =
					Fixtures[PreviousIndex];
				ASSERT_THAT(IsFalse(
					Previous.Authority.Family == Fixture.Authority.Family
					&& Previous.Authority.Variant == Fixture.Authority.Variant
					&& Previous.Authority.Cardinality
						== Fixture.Authority.Cardinality),
					TEXT("all 48 resolved physical fixture keys are unique"));
			}
			ResolvedFixtureTargetCount += Fixture.Targets.Num();
			if (Fixture.Authority.Purpose
				== ETsScrRepresentativeFixturePurposeForTests::Shape)
			{
				ASSERT_THAT(AreEqual(0, Fixture.Targets.Num()));
			}
			if (Fixture.Authority.Purpose
				== ETsScrRepresentativeFixturePurposeForTests::Slack)
			{
				ASSERT_THAT(AreEqual(1, Fixture.Targets.Num()));
				ASSERT_THAT(AreEqual(
					Fixture.Authority.ExpectedSlackRequestedElementCount,
					Fixture.Targets[0].RequestedElementCount),
					TEXT("each frozen slack literal preserves its exact allocator request"));
				ASSERT_THAT(IsTrue(
					Fixture.Targets[0].bTouchesActualAllocatorSlackBoundary));
			}
		}
		ASSERT_THAT(AreEqual(TsScrExpectedRepresentativeTargetCountForTests,
			ResolvedFixtureTargetCount));
		for (const FTsScrExpandedAllocationCaseForTests& Case : Cases)
		{
			ASSERT_THAT(IsTrue(Case.RepresentativeFixtureIndex >= 0
				&& Case.RepresentativeFixtureIndex < Fixtures.Num()));
			++CountsBySite[static_cast<int32>(Case.SiteKind)];
			if (Case.bTouchesActualAllocatorSlackBoundary)
			{
				SiteHasActualSlackBoundary[static_cast<int32>(Case.SiteKind)] = true;
			}
		}
		for (int32 SiteIndex = 0;
			SiteIndex < TsScrExpectedSiteCountForTests; ++SiteIndex)
		{
			ASSERT_THAT(AreEqual(
				TsScrSiteRepresentativeAuthoritiesForTests[
					SiteIndex].ExpectedTargetCount,
				CountsBySite[SiteIndex]),
				TEXT("every SiteKind keeps its explicit bounded representative count"));
		}
		for (const FTsScrAllocationSiteTemplateForTests& Template :
			GetIndependentTsScrSiteTemplatesForTests())
		{
			if (Template.Disposition != ETsScrSiteDispositionForTests::Required
				|| Template.SiteKind == ETsScrAllocationSiteKindForTests::TokenController
				|| Template.SiteKind == ETsScrAllocationSiteKindForTests::FlatHeaderOffsets)
			{
				continue;
			}
			ASSERT_THAT(IsTrue(SiteHasActualSlackBoundary[
				static_cast<int32>(Template.SiteKind)]),
				TEXT("every variable Required site reaches its own allocator transition"));
		}

		const auto HasPrimaryOccurrence = [&Cases](
			const ETsScrAllocationSiteKindForTests SiteKind,
			const int32 Primary)
		{
			return Cases.ContainsByPredicate([=](
				const FTsScrExpandedAllocationCaseForTests& Case)
			{
				return Case.SiteKind == SiteKind && Case.PrimaryIndex == Primary;
			});
		};
		const auto HasSecondaryOccurrence = [&Cases](
			const ETsScrAllocationSiteKindForTests SiteKind,
			const int32 Secondary)
		{
			return Cases.ContainsByPredicate([=](
				const FTsScrExpandedAllocationCaseForTests& Case)
			{
				return Case.SiteKind == SiteKind && Case.SecondaryIndex == Secondary;
			});
		};
		using K = ETsScrAllocationSiteKindForTests;
		using F = ETsScrRepresentativeFixtureIdForTests;
		const auto HasAuthorityTarget = [&Cases](const F FixtureId,
			const K SiteKind,
			const int32 Primary, const int32 Secondary)
		{
			return Cases.ContainsByPredicate([=](
				const FTsScrExpandedAllocationCaseForTests& Case)
			{
				return Case.RepresentativeFixtureIndex
						== static_cast<int32>(FixtureId)
					&& Case.SiteKind == SiteKind
					&& Case.PrimaryIndex == Primary
					&& Case.SecondaryIndex == Secondary;
			});
		};
		ASSERT_THAT(IsTrue(HasAuthorityTarget(
			F::OccurrenceTopMetadata,
			K::TokenController, INDEX_NONE, INDEX_NONE)));
		ASSERT_THAT(IsTrue(HasAuthorityTarget(
			F::OccurrenceTopMetadata,
			K::FlatHeaderOffsets, INDEX_NONE, INDEX_NONE)));
		for (const int32 Index : {0, 8, 16})
		{
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrenceTopMetadata,
				K::MetadataKeyString, Index, INDEX_NONE)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrenceTopMetadata,
				K::MetadataValueString, Index, INDEX_NONE)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrencePropertyMetadata,
				K::PropertyCanonicalNameString, Index, INDEX_NONE)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrenceNestedDataType,
				K::DataTypeOrderedSubTypesArray, 0, Index)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrencePropertyMetadata,
				K::PropertyMetadataArray, Index, INDEX_NONE)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrencePropertyMetadata,
				K::PropertyMetadataKeyString, Index, Index)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrencePropertyMetadata,
				K::PropertyMetadataValueString, Index, Index)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrenceEnumMetadata,
				K::EnumNameString, Index, INDEX_NONE)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrenceEnumMetadata,
				K::EnumMetadataArray, Index, INDEX_NONE)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrenceEnumMetadata,
				K::EnumMetadataKeyString, Index, Index)));
			ASSERT_THAT(IsTrue(HasAuthorityTarget(
				F::OccurrenceEnumMetadata,
				K::EnumMetadataValueString, Index, Index)));
		}
		for (const K SiteKind : {
			K::MetadataKeyString,
			K::MetadataValueString,
			K::PropertyCanonicalNameString,
			K::PropertyMetadataArray,
			K::PropertyMetadataKeyString,
			K::PropertyMetadataValueString,
			K::EnumNameString,
			K::EnumMetadataArray,
			K::EnumMetadataKeyString,
			K::EnumMetadataValueString})
		{
			ASSERT_THAT(IsTrue(HasPrimaryOccurrence(SiteKind, 0)));
			ASSERT_THAT(IsTrue(HasPrimaryOccurrence(SiteKind, 8)));
			ASSERT_THAT(IsTrue(HasPrimaryOccurrence(SiteKind, 16)));
		}
		for (const K SiteKind : {
			K::PropertyMetadataKeyString,
			K::PropertyMetadataValueString,
			K::EnumMetadataKeyString,
			K::EnumMetadataValueString})
		{
			ASSERT_THAT(IsTrue(HasSecondaryOccurrence(SiteKind, 0)));
			ASSERT_THAT(IsTrue(HasSecondaryOccurrence(SiteKind, 8)));
			ASSERT_THAT(IsTrue(HasSecondaryOccurrence(SiteKind, 16)));
		}
		ASSERT_THAT(IsTrue(HasSecondaryOccurrence(
			K::DataTypeOrderedSubTypesArray, 0)));
		ASSERT_THAT(IsTrue(HasSecondaryOccurrence(
			K::DataTypeOrderedSubTypesArray, 8)));
		ASSERT_THAT(IsTrue(HasSecondaryOccurrence(
			K::DataTypeOrderedSubTypesArray, 16)));
	}

	TEST_METHOD(TsScrRequiredTemplatesExpandAtActualAllocatorBoundariesAndMatchChronology)
	{
		using FController = SharedPointerInternals::TIntrusiveReferenceController<
			FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>;
		constexpr uint32 EffectiveControllerAlignment = alignof(FController)
			> __STDCPP_DEFAULT_NEW_ALIGNMENT__
			? alignof(FController)
			: (sizeof(FController) <= 8
				? uint32(8) : uint32(__STDCPP_DEFAULT_NEW_ALIGNMENT__));
		void* IndependentControllerAllocation = FMemory::Malloc(
			sizeof(FController), EffectiveControllerAlignment);
		ASSERT_THAT(IsNotNull(IndependentControllerAllocation));
		const uint64 IndependentControllerBytes = static_cast<uint64>(
			FMemory::GetAllocSize(IndependentControllerAllocation));
		FMemory::Free(IndependentControllerAllocation);
		ASSERT_THAT(IsTrue(IndependentControllerBytes >= sizeof(FController)));
		ASSERT_THAT(AreEqual(IndependentControllerBytes,
			MeasureIndependentControllerBaseAllocationForTests()),
			TEXT("test-only controller base measurement matches the independent charge"));

		const TArray<FTsScrResolvedRepresentativeFixtureForTests>& Fixtures =
			GetResolvedTsScrRepresentativeFixturesForTests();
		ASSERT_THAT(AreEqual(TsScrExpectedRepresentativeFixtureCountForTests,
			Fixtures.Num()));
		TArray<bool> RequiredSiteWasExpandedByValidFixture;
		RequiredSiteWasExpandedByValidFixture.Init(false,
			TsScrExpectedSiteCountForTests);
		constexpr uint64 SeedRetained = 23;
		constexpr uint64 SeedTemporary = 11;
		int32 ExactFixtureCount = 0;

		for (const FTsScrResolvedRepresentativeFixtureForTests& Fixture : Fixtures)
		{
			const TArray<uint8>& Payload = Fixture.Payload;
			const FIndependentTsScrExpectedPlanForTests& Expected =
				Fixture.Plan;
			ASSERT_THAT(IsTrue(Fixture.Inventory.bComplete));

			FAngelscriptCacheReadBudget ValidityBudget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> ValidityOutput;
			const FAngelscriptCacheValidationResult ValidityResult =
				DecodeWithMatchingRecordId(Payload, FAngelscriptCacheReadLimits{},
					ValidityBudget, ValidityOutput);
			ASSERT_THAT(IsTrue(ValidityResult.IsSuccess()),
				TEXT("every explicit representative fixture first passes the production common factory"));
			ASSERT_THAT(IsTrue(ValidityOutput.IsSet()));
			if (!ValidityResult.IsSuccess() || !ValidityOutput.IsSet())
			{
				continue;
			}
			ValidityOutput.Reset();

			FAngelscriptCacheReadLimits ExactLimits;
			ExactLimits.MaxTotalDecodedBytes =
				SeedRetained + SeedTemporary + Expected.ExactTotalBytes;
			ExactLimits.MaxResidentDecodedBytes =
				SeedRetained + SeedTemporary + Expected.PeakLiveBytes;
			FTsScrCallerOwnedProbeCaptureForTests<> Capture;
			FAngelscriptCacheTypeSchemaAllocationProbeForTests& Probe = Capture.Probe;
			FAngelscriptCacheReadBudget Budget;
			ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(
				SeedRetained, ExactLimits)));
			FAngelscriptCacheTemporaryResidentReservation SeedReservation;
			ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(
				SeedTemporary, ExactLimits, SeedReservation)));
			TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
			FAngelscriptCacheValidationResult Result;
			{
				Result = DecodeWithMatchingRecordIdAndProbe(Payload,
					ExactLimits, Budget, Probe, Output);
				ASSERT_THAT(IsTrue(Result.IsSuccess()));
				ASSERT_THAT(IsTrue(Output.IsSet()));

				const FTsScrFilteredProbeEventViewForTests Observed =
					Capture.GetAllocationEvents();
				ASSERT_THAT(AreEqual(Expected.Events.Num(), Observed.Num()),
					TEXT("semantic-blind events match the independent chronology"));
				int32 AllocationEventIndex = 0;
				for (const FAngelscriptCacheTypeSchemaProbeEventForTests& A :
					Capture.GetChronology())
				{
					if (A.Kind !=
						EAngelscriptCacheTypeSchemaProbeEventKindForTests::Allocation)
					{
						continue;
					}
					ASSERT_THAT(IsTrue(AllocationEventIndex < Expected.Events.Num()));
					const FTsScrExpectedAllocationEventForTests& E =
						Expected.Events[AllocationEventIndex++];
					ASSERT_THAT(AreEqual(E.RequestedElementCount,
						A.RequestedElementCount));
					ASSERT_THAT(AreEqual(E.ElementSize, A.ElementSize));
					ASSERT_THAT(AreEqual(E.ElementAlignment, A.ElementAlignment));
					ASSERT_THAT(AreEqual(E.ReservedCapacity, A.ReservedCapacity));
					ASSERT_THAT(AreEqual(E.ExactChargeBytes, A.AllocatedBytes));
					ASSERT_THAT(AreEqual(E.ExactChargeBytes, A.TotalChargeBytes));
					ASSERT_THAT(AreEqual(uint64(0), A.ResidentChargeBytes));
					ASSERT_THAT(AreEqual(E.ExactChargeBytes,
						A.TemporaryChargeBytes));
					ASSERT_THAT(AreEqual(
						ETsScrSiteLifetimeForTests::CandidateTemporary, E.Lifetime));
					RequiredSiteWasExpandedByValidFixture[
						static_cast<int32>(E.SiteKind)] = true;
				}
				ASSERT_THAT(AreEqual(Expected.Events.Num(), AllocationEventIndex));

				const FTsScrFilteredProbeEventViewForTests Promotions =
					Capture.GetPromotionCheckpoints();
				ASSERT_THAT(AreEqual(
					int32(Expected.PromotionCheckpointCount), Promotions.Num()));
				ASSERT_THAT(AreEqual(uint64(1), Expected.PromotionCheckpointCount));
				const auto& Promotion = Promotions.FindAtForConstantLookup(0);
				ASSERT_THAT(AreEqual(SeedTemporary + Expected.PrePromotionTemporaryBytes,
					Promotion.TemporaryBytesBefore));
				ASSERT_THAT(AreEqual(SeedRetained + Expected.ExactResidentBytes,
					Promotion.ResidentBytesAfter));
				ASSERT_THAT(AreEqual(
					SeedRetained + SeedTemporary + Expected.ExactTotalBytes,
					Promotion.TotalDecodedBytes));
				ASSERT_THAT(AreEqual(Expected.Events.Num(),
					Promotion.AcceptedAllocationEventCount));
				ASSERT_THAT(AreEqual(uint64(Expected.Events.Num()),
					Promotion.AllocationAttemptCount));
				ASSERT_THAT(AreEqual(Expected.ExactTotalBytes,
					Promotion.AllocatedBytes));
				ASSERT_THAT(IsFalse(Promotion.bHandleWasObservable,
					TEXT("promotion precedes handle publication")));

				Output.Reset();
				ASSERT_THAT(AreEqual(uint64(0), Probe.GetLiveAllocatedBytes()));
				ASSERT_THAT(AreEqual(int64(0), Probe.GetAllocationBalance()));
			}
			ASSERT_THAT(IsFalse(Output.IsSet()));
			ASSERT_THAT(AreEqual(
				SeedRetained + SeedTemporary + Expected.ExactTotalBytes,
				Budget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(SeedRetained + Expected.ExactResidentBytes,
				Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(SeedTemporary,
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(
				SeedRetained + SeedTemporary + Expected.PeakLiveBytes,
				Budget.GetPeakLiveResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(Expected.PeakLiveBytes,
				Probe.GetPeakLiveAllocatedBytes()));
			ASSERT_THAT(IsFalse(Capture.bOverflowed));
			++ExactFixtureCount;
		}
		ASSERT_THAT(AreEqual(TsScrExpectedRepresentativeFixtureCountForTests,
			ExactFixtureCount),
			TEXT("full exact chronology runs once for each frozen representative fixture"));

		for (const FTsScrAllocationSiteTemplateForTests& Template :
			GetIndependentTsScrSiteTemplatesForTests())
		{
			if (Template.Disposition == ETsScrSiteDispositionForTests::Required)
			{
				ASSERT_THAT(IsTrue(RequiredSiteWasExpandedByValidFixture[
					static_cast<int32>(Template.SiteKind)]),
					TEXT("only exact Total/live common-factory success closes Required coverage"));
			}
		}
	}
	TEST_METHOD(TsScrEveryRequiredSiteHasIndependentTotalAndResidentOneShortLimits)
	{
		constexpr uint64 SeedRetained = 29;
		constexpr uint64 SeedTemporary = 17;
		const TArray<FTsScrResolvedRepresentativeFixtureForTests>& Fixtures =
			GetResolvedTsScrRepresentativeFixturesForTests();
		ASSERT_THAT(AreEqual(TsScrExpectedRepresentativeFixtureCountForTests,
			Fixtures.Num()));
		int32 OneShortTargetCount = 0;

		for (const FTsScrResolvedRepresentativeFixtureForTests& Fixture : Fixtures)
		{
			if (Fixture.Targets.IsEmpty())
			{
				ASSERT_THAT(AreEqual(
					ETsScrRepresentativeFixturePurposeForTests::Shape,
					Fixture.Authority.Purpose));
			}
			else
			{
				const TArray<uint8>& Payload = Fixture.Payload;
				FAngelscriptCacheReadBudget ValidityBudget;
				TOptional<FAngelscriptDecodedCacheRecordHandle> ValidityOutput;
				const FAngelscriptCacheValidationResult ValidityResult =
					DecodeWithMatchingRecordId(Payload, FAngelscriptCacheReadLimits{},
						ValidityBudget, ValidityOutput);
				ASSERT_THAT(IsTrue(ValidityResult.IsSuccess()),
					TEXT("one-short rows are gated once by their explicit production fixture"));
				ASSERT_THAT(IsTrue(ValidityOutput.IsSet()));
				if (ValidityResult.IsSuccess() && ValidityOutput.IsSet())
				{
					ValidityOutput.Reset();
					for (const FTsScrExpandedAllocationCaseForTests& Case :
						Fixture.Targets)
					{
						const FTsScrPlanCoordinateKeyForTests Coordinate{
							Case.SiteKind, Case.PrimaryIndex,
							Case.SecondaryIndex, Case.TertiaryIndex};
						const int32* TargetIndex =
							Fixture.EventIndexByCoordinate.Find(Coordinate);
						ASSERT_THAT(IsNotNull(TargetIndex));
						if (TargetIndex == nullptr)
						{
							return;
						}
						const FTsScrExpectedAllocationEventForTests& Target =
							Fixture.Plan.Events[*TargetIndex];

						const auto RunOneShort = [&](const bool bCombinedLiveDimension)
						{
							FAngelscriptCacheReadLimits Limits;
							Limits.MaxTotalDecodedBytes = bCombinedLiveDimension
								? MAX_uint64
								: SeedRetained + SeedTemporary + Target.TotalPrefixBefore
									+ Target.ExactChargeBytes - 1;
							Limits.MaxResidentDecodedBytes = bCombinedLiveDimension
								? SeedRetained + SeedTemporary
									+ Target.TemporaryPrefixBefore
									+ Target.ExactChargeBytes - 1
								: MAX_uint64;
							FAngelscriptCacheReadBudget Budget;
							ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(
								SeedRetained, Limits)));
							FAngelscriptCacheTemporaryResidentReservation SeedReservation;
							ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(
								SeedTemporary, Limits, SeedReservation)));

							FTsScrCallerOwnedProbeCaptureForTests<> Capture;
							FAngelscriptCacheTypeSchemaAllocationProbeForTests& Probe =
								Capture.Probe;
							TOptional<FAngelscriptDecodedCacheRecordHandle> Output =
								MakeSentinelRecord();
							const FAngelscriptCacheValidationResult Result =
								DecodeWithMatchingRecordIdAndProbe(
									Payload, Limits, Budget, Probe, Output);
							ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
								Result, Output,
								EAngelscriptCacheValidationError::BudgetExceeded,
								Target.ExpectedStage, Target.ExpectedByteOffset,
								bCombinedLiveDimension
									? TEXT("independent combined-live one-byte-short")
									: TEXT("independent Total one-byte-short"))));
							ASSERT_THAT(AreEqual(*TargetIndex,
								Capture.GetAllocationEvents().Num()),
								TEXT("the rejected target and all later events remain absent"));
							ASSERT_THAT(AreEqual(uint64(1),
								Probe.GetRejectedReservationCount()));
							ASSERT_THAT(AreEqual(uint64(*TargetIndex),
								Probe.GetTotalAllocationAttempts()),
								TEXT("reservation rejects before allocator entry"));
							ASSERT_THAT(AreEqual(0,
								Capture.GetPromotionCheckpoints().Num()));
							ASSERT_THAT(AreEqual(uint64(0),
								Probe.GetLiveAllocatedBytes()));
							ASSERT_THAT(AreEqual(int64(0),
								Probe.GetAllocationBalance()));
							ASSERT_THAT(AreEqual(Target.TemporaryPrefixBefore,
								Probe.GetPeakLiveAllocatedBytes()));
							ASSERT_THAT(AreEqual(SeedRetained + SeedTemporary
								+ Target.TotalPrefixBefore, Budget.GetDecodedBytes()));
							ASSERT_THAT(AreEqual(SeedRetained,
								Budget.GetResidentDecodedBytes()));
							ASSERT_THAT(AreEqual(SeedTemporary,
								Budget.GetTemporaryResidentDecodedBytes()));
							ASSERT_THAT(AreEqual(SeedRetained + SeedTemporary
								+ Target.TemporaryPrefixBefore,
								Budget.GetPeakLiveResidentDecodedBytes()));
							ASSERT_THAT(IsFalse(Capture.bOverflowed));
						};
						RunOneShort(false);
						RunOneShort(true);
						++OneShortTargetCount;
					}
				}
			}
		}
		ASSERT_THAT(AreEqual(TsScrExpectedRepresentativeTargetCountForTests,
			OneShortTargetCount));
	}
	TEST_METHOD(TsScrReferenceRelocationLimitFreezesExactVariantOrderAndEveryPrefix)
	{
		TArray<bool> ExpectedReferenceFamilies;
		ExpectedReferenceFamilies.Init(false, 15);
		TArray<bool> ObservedReferenceFamilies;
		ObservedReferenceFamilies.Init(false, 15);
		int32 ProcessedAuthorityCount = 0;

		for (const FTsScrReferenceCaseAuthorityForTests& Authority :
			GetTsScrReferenceCaseAuthorityForTests())
		{
			++ProcessedAuthorityCount;
			bool bExpectedLocalSuccess = false;
			const FAngelscriptCachedTypeSchema Schema =
				MakeTypeSchemaAllocationFixture(
					Authority.Family, 1, Authority.Variant, bExpectedLocalSuccess);
			ASSERT_THAT(IsTrue(bExpectedLocalSuccess),
				*FString::Printf(TEXT(
					"every named reference authority is a valid fixture: "
					"family=%u variant=%u shape=%u"),
					Authority.Family, Authority.Variant,
					static_cast<uint32>(Authority.Shape)));
			if (!bExpectedLocalSuccess)
			{
				return;
			}
			TArray<uint8> Payload;
			ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, Payload).IsSuccess()));
			const FIndependentTypeSchemaWireInventoryForTests Inventory =
				ScanIndependentTypeSchemaWireForTests(Payload);
			ASSERT_THAT(IsTrue(Inventory.bComplete));
			const TArray<uint64> ExpectedReferenceOffsets =
				BuildExpectedStableReferenceOffsetsForTests(Authority, Inventory);
			const bool bExpectedReferenceBearing = !ExpectedReferenceOffsets.IsEmpty();
			ASSERT_THAT(AreEqual(ExpectedReferenceOffsets.Num(),
				Inventory.StableReferenceOffsets.Num()),
				TEXT("fixed coordinate authority and raw scanner agree on exact count"));
			for (int32 ReferenceIndex = 0;
				ReferenceIndex < ExpectedReferenceOffsets.Num(); ++ReferenceIndex)
			{
				ASSERT_THAT(AreEqual(ExpectedReferenceOffsets[ReferenceIndex],
					Inventory.StableReferenceOffsets[ReferenceIndex]),
					TEXT("stable-reference occurrence order is frozen exactly"));
			}
			ExpectedReferenceFamilies[Authority.Family] |= bExpectedReferenceBearing;
			ObservedReferenceFamilies[Authority.Family] |=
				!Inventory.StableReferenceOffsets.IsEmpty();

			if (bExpectedReferenceBearing)
			{
				const FIndependentTsScrExpectedPlanForTests ExpectedPlan =
					BuildIndependentTsScrPlanForTests(Schema, Payload, Inventory);
				FAngelscriptCacheReadLimits ExactLimits;
				ExactLimits.MaxReferencesAndRelocations =
					ExpectedReferenceOffsets.Num();
				FAngelscriptCacheReadBudget ExactBudget;
				FTsScrCallerOwnedProbeCaptureForTests<> ExactCapture;
				FAngelscriptCacheTypeSchemaAllocationProbeForTests& ExactProbe =
					ExactCapture.Probe;
				TOptional<FAngelscriptDecodedCacheRecordHandle> ExactOutput;
				{
					ASSERT_THAT(IsTrue(DecodeWithMatchingRecordIdAndProbe(
						Payload, ExactLimits, ExactBudget, ExactProbe,
						ExactOutput).IsSuccess()));
					ASSERT_THAT(IsTrue(ExactOutput.IsSet()));
					ASSERT_THAT(AreEqual(1,
						ExactCapture.GetPromotionCheckpoints().Num()));
					ExactOutput.Reset();
					ASSERT_THAT(AreEqual(int64(0), ExactProbe.GetAllocationBalance()));
				}
				ASSERT_THAT(AreEqual(uint64(ExpectedReferenceOffsets.Num()),
					ExactBudget.GetReferencesAndRelocations()));
				ASSERT_THAT(AreEqual(ExpectedPlan.ExactTotalBytes,
					ExactBudget.GetDecodedBytes()));
				ASSERT_THAT(AreEqual(ExpectedPlan.ExactResidentBytes,
					ExactBudget.GetResidentDecodedBytes()));
				ASSERT_THAT(AreEqual(uint64(0),
					ExactBudget.GetTemporaryResidentDecodedBytes()));
				ASSERT_THAT(IsFalse(ExactCapture.bOverflowed));

				for (int32 ReferenceIndex = 0;
					ReferenceIndex < ExpectedReferenceOffsets.Num(); ++ReferenceIndex)
				{
					const uint64 ReferenceOffset =
						ExpectedReferenceOffsets[ReferenceIndex];
					int32 AcceptedAllocationEvents = 0;
					uint64 AcceptedAllocationBytes = 0;
					for (const FTsScrExpectedAllocationEventForTests& Event :
						ExpectedPlan.Events)
					{
						if (Event.ReferenceVisibilityByteOffset >= ReferenceOffset)
						{
							break;
						}
						++AcceptedAllocationEvents;
						AcceptedAllocationBytes += Event.ExactChargeBytes;
					}

					FAngelscriptCacheReadLimits ShortLimits;
					ShortLimits.MaxReferencesAndRelocations = ReferenceIndex;
					FAngelscriptCacheReadBudget ShortBudget;
					FTsScrCallerOwnedProbeCaptureForTests<> ShortCapture;
					FAngelscriptCacheTypeSchemaAllocationProbeForTests& ShortProbe =
						ShortCapture.Probe;
					TOptional<FAngelscriptDecodedCacheRecordHandle> ShortOutput =
						MakeSentinelRecord();
					const FAngelscriptCacheValidationResult Result =
						DecodeWithMatchingRecordIdAndProbe(
							Payload, ShortLimits, ShortBudget, ShortProbe,
							ShortOutput);
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
						Result, ShortOutput,
						EAngelscriptCacheValidationError::BudgetExceeded,
						EAngelscriptCacheValidationStage::PayloadDecode,
						ReferenceOffset,
						TEXT("each exact reference occurrence has its own short row"))));
					ASSERT_THAT(AreEqual(uint64(ReferenceIndex),
						ShortBudget.GetReferencesAndRelocations()));
					ASSERT_THAT(AreEqual(AcceptedAllocationEvents,
						ShortCapture.GetAllocationEvents().Num()),
						*[&]
						{
							FString Context = FString::Printf(TEXT(
								"family=%u variant=%u reference=%d offset=%llu "
								"expected-events=%d actual-events=%d"),
								Authority.Family, Authority.Variant, ReferenceIndex,
								ReferenceOffset, AcceptedAllocationEvents,
								ShortCapture.GetAllocationEvents().Num());
							const int32 EventCount = FMath::Max(
								AcceptedAllocationEvents,
								ShortCapture.GetAllocationEvents().Num());
							for (int32 EventIndex = 0; EventIndex < EventCount; ++EventIndex)
							{
								Context += FString::Printf(TEXT(" | %d"), EventIndex);
								if (ExpectedPlan.Events.IsValidIndex(EventIndex))
								{
									const auto& ExpectedEvent = ExpectedPlan.Events[EventIndex];
									Context += FString::Printf(TEXT(" E(%u,%llu,%llu,%llu)"),
										static_cast<uint32>(ExpectedEvent.SiteKind),
										ExpectedEvent.ExpectedByteOffset,
										ExpectedEvent.ReferenceVisibilityByteOffset,
										ExpectedEvent.ExactChargeBytes);
								}
								if (EventIndex < ShortCapture.GetAllocationEvents().Num())
								{
									const auto& ActualEvent = ShortCapture.GetAllocationEvents().
										FindAtForConstantLookup(EventIndex);
									Context += FString::Printf(TEXT(" A(%llu,%llu)"),
										ActualEvent.FieldOffset, ActualEvent.AllocatedBytes);
								}
							}
							return Context;
						}());
					ASSERT_THAT(AreEqual(uint64(AcceptedAllocationEvents),
						ShortProbe.GetTotalAllocationAttempts()));
					ASSERT_THAT(AreEqual(0,
						ShortCapture.GetPromotionCheckpoints().Num()));
					ASSERT_THAT(AreEqual(AcceptedAllocationBytes,
						ShortBudget.GetDecodedBytes()));
					ASSERT_THAT(AreEqual(uint64(0),
						ShortBudget.GetResidentDecodedBytes()));
					ASSERT_THAT(AreEqual(uint64(0),
						ShortBudget.GetTemporaryResidentDecodedBytes()));
					ASSERT_THAT(AreEqual(AcceptedAllocationBytes,
						ShortBudget.GetPeakLiveResidentDecodedBytes()));
					ASSERT_THAT(AreEqual(uint64(0),
						ShortProbe.GetLiveAllocatedBytes()));
					ASSERT_THAT(AreEqual(int64(0),
						ShortProbe.GetAllocationBalance()));
					ASSERT_THAT(IsFalse(ShortCapture.bOverflowed));
				}
			}
		}
		ASSERT_THAT(AreEqual(TsScrExpectedReferenceCaseCountForTests,
			ProcessedAuthorityCount));

		for (int32 Family = 1; Family <= 14; ++Family)
		{
			ASSERT_THAT(AreEqual(ExpectedReferenceFamilies[Family],
				ObservedReferenceFamilies[Family]),
				TEXT("the exact reference-bearing family set cannot shrink or grow"));
		}
	}
	TEST_METHOD(TsScrStreamingScratchCheckpointsAreReachedWithoutAllocating)
	{
		for (const FTsScrStreamingCheckpointCaseForTests& Case :
			GetTsScrStreamingCheckpointCasesForTests())
		{
			const FTsScrAllocationSiteTemplateForTests& Template =
				FindTsScrTemplateForTests(Case.SiteKind);
			ASSERT_THAT(AreEqual(ETsScrSiteDispositionForTests::StreamingZero,
				Template.Disposition));
			ASSERT_THAT(AreEqual(
				ETsScrSiteLifetimeForTests::StreamingTemporary,
				Template.Lifetime));

			bool bExpectedLocalSuccess = false;
			const FAngelscriptCachedTypeSchema Schema =
				MakeTypeSchemaAllocationFixture(
					Case.Family, 17, Case.Variant, bExpectedLocalSuccess);
			ASSERT_THAT(IsTrue(bExpectedLocalSuccess));
			TArray<uint8> Payload;
			ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
				Schema, Payload).IsSuccess()));
			const FIndependentTypeSchemaWireInventoryForTests Inventory =
				ScanIndependentTypeSchemaWireForTests(Payload);
			ASSERT_THAT(IsTrue(Inventory.bComplete));
			const FIndependentTsScrExpectedPlanForTests Expected =
				BuildIndependentTsScrPlanForTests(Schema, Payload, Inventory);

			FTsScrCallerOwnedProbeCaptureForTests<> SuccessCapture;
			FAngelscriptCacheTypeSchemaAllocationProbeForTests& SuccessProbe =
				SuccessCapture.Probe;
			FAngelscriptCacheReadBudget SuccessBudget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> SuccessOutput;
			{
				ASSERT_THAT(IsTrue(DecodeWithMatchingRecordIdAndProbe(Payload,
					FAngelscriptCacheReadLimits{}, SuccessBudget,
					SuccessProbe, SuccessOutput).IsSuccess()));
				ASSERT_THAT(IsTrue(SuccessOutput.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Events.Num(),
					SuccessCapture.GetAllocationEvents().Num()));
				ASSERT_THAT(AreEqual(uint64(Expected.Events.Num()),
					SuccessProbe.GetTotalAllocationAttempts()));
				ASSERT_THAT(AreEqual(Expected.ExactTotalBytes,
					SuccessProbe.GetTotalAllocatedBytes()));
				const FTsScrFilteredProbeEventViewForTests Checkpoints =
					SuccessCapture.GetValidationCheckpoints();
				ASSERT_THAT(AreEqual(12, Checkpoints.Num()));
				for (int32 CheckpointIndex = 0;
					CheckpointIndex < Checkpoints.Num(); ++CheckpointIndex)
				{
					ASSERT_THAT(AreEqual(uint32(CheckpointIndex),
						Checkpoints.FindAtForConstantLookup(
							CheckpointIndex).SequenceOrdinal));
					ASSERT_THAT(AreEqual(uint64(Expected.Events.Num()),
						Checkpoints.FindAtForConstantLookup(
							CheckpointIndex).AllocationAttemptCount));
					ASSERT_THAT(AreEqual(Expected.ExactTotalBytes,
						Checkpoints.FindAtForConstantLookup(
							CheckpointIndex).AllocatedBytes));
				}
				ASSERT_THAT(AreEqual(1,
					SuccessCapture.GetPromotionCheckpoints().Num()));
				SuccessOutput.Reset();
				ASSERT_THAT(AreEqual(uint64(0),
					SuccessProbe.GetLiveAllocatedBytes()));
				ASSERT_THAT(AreEqual(int64(0),
					SuccessProbe.GetAllocationBalance()));
			}
			ASSERT_THAT(AreEqual(Expected.ExactTotalBytes,
				SuccessBudget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(Expected.ExactResidentBytes,
				SuccessBudget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(uint64(0),
				SuccessBudget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(IsFalse(SuccessCapture.bOverflowed));

			FTsScrCallerOwnedProbeCaptureForTests<> FailureCapture;
			FAngelscriptCacheTypeSchemaAllocationProbeForTests& FailureProbe =
				FailureCapture.Probe;
			FailureProbe.InjectOverflowAfterValidationCheckpointForTests(
				Case.CheckpointOrdinal);
			FAngelscriptCacheReadBudget FailureBudget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> FailureOutput =
				MakeSentinelRecord();
			const FAngelscriptCacheValidationResult FailureResult =
				DecodeWithMatchingRecordIdAndProbe(Payload,
					FAngelscriptCacheReadLimits{}, FailureBudget, FailureProbe,
					FailureOutput);
			ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
				FailureResult, FailureOutput,
				EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheValidationStage::LocalSemantic,
				FindIndependentWireOffsetForTests(Inventory,
					Case.ExpectedField, Case.PrimaryIndex),
				TEXT("later fault proves the intended StreamingZero checkpoint was reached"))));
			ASSERT_THAT(AreEqual(int32(Case.CheckpointOrdinal + 1),
				FailureCapture.GetValidationCheckpoints().Num()));
			ASSERT_THAT(AreEqual(1,
				FailureCapture.GetInjectedOverflowCheckpoints().Num()),
				TEXT("the private test-only overflow injection fired exactly once"));
			ASSERT_THAT(AreEqual(Case.CheckpointOrdinal,
				FailureCapture.GetInjectedOverflowCheckpoints()
					.FindAtForConstantLookup(0).SequenceOrdinal));
			ASSERT_THAT(AreEqual(0,
				FailureCapture.GetPromotionCheckpoints().Num()));
			ASSERT_THAT(AreEqual(Expected.Events.Num(),
				FailureCapture.GetAllocationEvents().Num()));
			ASSERT_THAT(AreEqual(uint64(Expected.Events.Num()),
				FailureProbe.GetTotalAllocationAttempts()));
			ASSERT_THAT(AreEqual(Expected.ExactTotalBytes,
				FailureProbe.GetTotalAllocatedBytes()));
			ASSERT_THAT(AreEqual(uint64(0),
				FailureProbe.GetLiveAllocatedBytes()));
			ASSERT_THAT(AreEqual(int64(0),
				FailureProbe.GetAllocationBalance()));
			ASSERT_THAT(AreEqual(Expected.ExactTotalBytes,
				FailureBudget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(uint64(0),
				FailureBudget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(uint64(0),
				FailureBudget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(Expected.PeakLiveBytes,
				FailureBudget.GetPeakLiveResidentDecodedBytes()));
			ASSERT_THAT(IsFalse(FailureCapture.bOverflowed));
		}
	}

	TEST_METHOD(TsScrCallerOwnedProbeReportsUndersizedChronologyOverflowWithoutGrowing)
	{
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			MakeCompleteDelegateSchema(), Payload).IsSuccess()));
		FTsScrCallerOwnedProbeCaptureForTests<1> Capture;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordIdAndProbe(Payload,
			FAngelscriptCacheReadLimits{}, Budget, Capture.Probe, Output).IsSuccess()));
		ASSERT_THAT(IsTrue(Output.IsSet()));
		ASSERT_THAT(AreEqual(1, Capture.EventCount));
		ASSERT_THAT(IsTrue(Capture.bOverflowed,
			TEXT("a full caller-owned view reports overflow and never grows")));
	}
	TEST_METHOD(TsScrEveryFamilyLateFaultProvesTargetAndRestoresFullState)
	{
		constexpr uint64 SeedRetained = 13;
		constexpr uint64 SeedTemporary = 17;
		const EAngelscriptCacheTypeSchemaInjectedFailureForTests FaultModes[] = {
			EAngelscriptCacheTypeSchemaInjectedFailureForTests::PhysicalAfterTarget,
			EAngelscriptCacheTypeSchemaInjectedFailureForTests::LocalAfterTarget,
			EAngelscriptCacheTypeSchemaInjectedFailureForTests::HashAfterTarget,
		};
		TArray<bool> FamilyWasReached;
		FamilyWasReached.Init(false, 15);
		for (uint8 Family = 1; Family <= 11; ++Family)
		{
			for (uint32 Variant = 0;
				Variant < GetTypeSchemaAllocationVariantCount(Family); ++Variant)
			{
				bool bExpectedLocalSuccess = false;
				const FAngelscriptCachedTypeSchema Schema = MakeTypeSchemaAllocationFixture(
					Family, 1, Variant, bExpectedLocalSuccess);
				if (!bExpectedLocalSuccess)
				{
					continue;
				}
				TArray<uint8> Payload;
				ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
					Schema, Payload).IsSuccess()));
				const FIndependentTypeSchemaWireInventoryForTests Inventory =
					ScanIndependentTypeSchemaWireForTests(Payload);
				const FIndependentTsScrExpectedPlanForTests Expected =
					BuildIndependentTsScrPlanForTests(Schema, Payload, Inventory);
				int32 TargetIndex = INDEX_NONE;
				// Family 1 has no references, so use its latest site. For later
				// families use the first family site: its independent wire anchor is
				// reached before that family's stable-reference rows, making the
				// physical-fault reference prefix exact rather than offset-guessed
				// across a post-row parallel-offset allocation.
				const int32 FirstEventIndex = Family == 1
					? Expected.Events.Num() - 1 : 0;
				const int32 EndEventIndex = Family == 1
					? -1 : Expected.Events.Num();
				const int32 EventStep = Family == 1 ? -1 : 1;
				for (int32 EventIndex = FirstEventIndex;
					EventIndex != EndEventIndex; EventIndex += EventStep)
				{
					if (Expected.Events[EventIndex].Family == Family)
					{
						TargetIndex = EventIndex;
						break;
					}
				}
				if (TargetIndex == INDEX_NONE)
				{
					continue;
				}
				FamilyWasReached[Family] = true;
				const FTsScrExpectedAllocationEventForTests& Target =
					Expected.Events[TargetIndex];
				for (const EAngelscriptCacheTypeSchemaInjectedFailureForTests FaultMode :
					FaultModes)
				{
					const bool bPhysical = FaultMode
						== EAngelscriptCacheTypeSchemaInjectedFailureForTests::PhysicalAfterTarget;
					const int32 ExpectedAcceptedEvents = bPhysical
						? TargetIndex + 1 : Expected.Events.Num();
					uint64 ExpectedReferences = 0;
					for (const uint64 ReferenceOffset : Inventory.StableReferenceOffsets)
					{
						ExpectedReferences += !bPhysical
							|| ReferenceOffset < Target.ExpectedByteOffset;
					}
					const uint64 ExpectedTotal = SeedRetained + SeedTemporary
						+ (bPhysical
							? Target.TotalPrefixBefore + Target.ExactChargeBytes
							: Expected.ExactTotalBytes);
					const uint64 ExpectedCandidatePeak = bPhysical
						? Target.ResidentPrefixBefore + Target.TemporaryPrefixBefore
							+ Target.ExactChargeBytes
						: Expected.PeakLiveBytes;

					FAngelscriptCacheReadLimits Limits;
					FAngelscriptCacheReadBudget Budget;
					ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(
						SeedRetained, Limits)));
					FAngelscriptCacheTemporaryResidentReservation SeedReservation;
					ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(
						SeedTemporary, Limits, SeedReservation)));
					const FAngelscriptDecodedCacheRecordHandle PriorHandle =
						MakeSentinelRecord();
					const FAngelscriptDecodedCacheRecord* PriorAddress = &PriorHandle.Get();
					const FString PriorPayload = Hex(PriorHandle->GetCanonicalPayload());
					FTsScrCallerOwnedProbeCaptureForTests<> Capture;
					FAngelscriptCacheTypeSchemaAllocationProbeForTests& Probe = Capture.Probe;
					Probe.InjectOverflowAfterAcceptedEventForTests(TargetIndex, FaultMode);
					TOptional<FAngelscriptDecodedCacheRecordHandle> Output =
						MakeSentinelRecord();
					const FAngelscriptCacheValidationResult Result =
						DecodeWithMatchingRecordIdAndProbe(
							Payload, Limits, Budget, Probe, Output);
					const EAngelscriptCacheValidationStage ExpectedStage = bPhysical
						? EAngelscriptCacheValidationStage::PayloadDecode
						: EAngelscriptCacheValidationStage::LocalSemantic;
					ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner,
						Result, Output, EAngelscriptCacheValidationError::Overflow,
						ExpectedStage, Target.ExpectedByteOffset,
						TEXT("per-family injected late failure cleanup"))));
					ASSERT_THAT(AreEqual(ExpectedAcceptedEvents,
						Capture.GetAllocationEvents().Num()),
						TEXT("target is reached and no event after the selected fault occurs"));
					ASSERT_THAT(IsTrue(
						Capture.GetAllocationEvents().IsValidIndex(TargetIndex)));
					const auto& ObservedTarget = Capture.GetAllocationEvents().
						FindAtForConstantLookup(TargetIndex);
					ASSERT_THAT(AreEqual(Target.RequestedElementCount,
						ObservedTarget.RequestedElementCount));
					ASSERT_THAT(AreEqual(Target.ExactChargeBytes,
						ObservedTarget.AllocatedBytes));
					ASSERT_THAT(AreEqual(uint64(ExpectedAcceptedEvents),
						Probe.GetTotalAllocationAttempts()));
					ASSERT_THAT(AreEqual(0,
						Capture.GetPromotionCheckpoints().Num()));
					ASSERT_THAT(AreEqual(1,
						Capture.GetInjectedOverflowCheckpoints().Num()),
						TEXT("the independently tagged injection checkpoint proves the fault fired"));
					ASSERT_THAT(AreEqual(uint32(TargetIndex),
						Capture.GetInjectedOverflowCheckpoints()
							.FindAtForConstantLookup(0).SequenceOrdinal));
					ASSERT_THAT(AreEqual(uint64(0), Probe.GetLiveAllocatedBytes()));
					ASSERT_THAT(AreEqual(int64(0), Probe.GetAllocationBalance()));
					ASSERT_THAT(AreEqual(ExpectedCandidatePeak,
						Probe.GetPeakLiveAllocatedBytes()),
						*[&]
						{
							FString Context = FString::Printf(TEXT(
							"family=%d variant=%u target=%d site=%u fault=%u "
							"expected-peak=%llu actual-peak=%llu"),
							Family, Variant, TargetIndex,
							static_cast<uint32>(Target.SiteKind),
							static_cast<uint32>(FaultMode),
							ExpectedCandidatePeak,
							Probe.GetPeakLiveAllocatedBytes());
							for (int32 EventIndex = 0;
								EventIndex < ExpectedAcceptedEvents; ++EventIndex)
							{
								const auto& ExpectedEvent = Expected.Events[EventIndex];
								const auto& ActualEvent = Capture.GetAllocationEvents().
									FindAtForConstantLookup(EventIndex);
								Context += FString::Printf(TEXT(
									" | %d E(%u,%llu,%llu) A(%llu,%llu)"),
									EventIndex,
									static_cast<uint32>(ExpectedEvent.SiteKind),
									ExpectedEvent.ExpectedByteOffset,
									ExpectedEvent.ExactChargeBytes,
									ActualEvent.FieldOffset,
									ActualEvent.AllocatedBytes);
							}
							return Context;
						}());
					ASSERT_THAT(AreEqual(ExpectedTotal, Budget.GetDecodedBytes()));
					ASSERT_THAT(AreEqual(SeedRetained,
						Budget.GetResidentDecodedBytes()));
					ASSERT_THAT(AreEqual(SeedTemporary,
						Budget.GetTemporaryResidentDecodedBytes()));
					ASSERT_THAT(AreEqual(
						SeedRetained + SeedTemporary + ExpectedCandidatePeak,
						Budget.GetPeakLiveResidentDecodedBytes()));
					ASSERT_THAT(AreEqual(ExpectedReferences,
						Budget.GetReferencesAndRelocations()));
					ASSERT_THAT(IsFalse(Capture.bOverflowed));
					ASSERT_THAT(IsTrue(&PriorHandle.Get() == PriorAddress));
					ASSERT_THAT(AreEqual(PriorPayload,
						Hex(PriorHandle->GetCanonicalPayload())),
						TEXT("unrelated prior retained handle remains readable"));
				}
			}
			ASSERT_THAT(IsTrue(FamilyWasReached[Family]),
				TEXT("every TS-SCR family reaches its independently selected target"));
		}
	}

	TEST_METHOD(SharedBudgetCountersStayMonotonicAcrossLateFailureWithoutReset)
	{
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(FAngelscriptCacheTypeSchemaArchive::SerializeTypeSchema(
			MakeCompleteDelegateSchema(), Payload).IsSuccess()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget SharedBudget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> Output;
		ASSERT_THAT(IsTrue(DecodeWithMatchingRecordId(
			Payload, Limits, SharedBudget, Output).IsSuccess()));
		ASSERT_THAT(IsTrue(Output.IsSet()));
		const uint64 DecodedBeforeFailure = SharedBudget.GetDecodedBytes();
		const uint64 ReferencesBeforeFailure =
			SharedBudget.GetReferencesAndRelocations();
		const uint64 ResidentBeforeFailure = SharedBudget.GetResidentDecodedBytes();

		TArray<uint8> Trailing = Payload;
		Trailing.Add(0x7f);
		const FAngelscriptCacheValidationResult Result = DecodeWithMatchingRecordId(
			Trailing, Limits, SharedBudget, Output);
		ASSERT_THAT(IsTrue(ExpectExactFailureAndReset(*TestRunner, Result, Output,
			EAngelscriptCacheValidationError::TrailingData,
			EAngelscriptCacheValidationStage::PayloadDecode,
			uint64(Payload.Num()), TEXT("late failure on shared monotonic budget"))));
		ASSERT_THAT(IsTrue(SharedBudget.GetDecodedBytes() >= DecodedBeforeFailure));
		ASSERT_THAT(IsTrue(SharedBudget.GetReferencesAndRelocations()
			>= ReferencesBeforeFailure));
		ASSERT_THAT(AreEqual(uint64(0),
			SharedBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(SharedBudget.GetResidentDecodedBytes()
			>= ResidentBeforeFailure),
			TEXT("the shared Budget is conservative and monotonic; it is never refunded/reset"));
	}
};

#undef ASSERT_THAT
#undef UEAS_TYPESchema_SELECT_ASSERT
#undef UEAS_TYPESchema_ASSERT_THAT_2
#undef UEAS_TYPESchema_ASSERT_THAT_1
#pragma pop_macro("ASSERT_THAT")

#ifndef ASSERT_THAT
#error "TypeSchema test-local assertion dispatcher did not restore CQTest ASSERT_THAT"
#endif

#endif // WITH_ANGELSCRIPT_UNITTESTS
