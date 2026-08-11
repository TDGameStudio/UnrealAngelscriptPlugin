#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"

#include "CQTest.h"

#include <tuple>
#include <type_traits>
#include <utility>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheRemainingRecordCoordinateTests,
	"Angelscript.TestModule.Cache.RemainingRecordCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr uint32 U = MAX_uint32;

	static FAngelscriptHash256 MakeSentinelHash(const uint8 Fill)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Fill, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCacheStableReference MakeSentinelReference(
		const EAngelscriptCacheReferenceKind Kind,
		const uint8 KeyFill,
		const uint8 AbiFill)
	{
		return {Kind, MakeSentinelHash(KeyFill), MakeSentinelHash(AbiFill)};
	}

	static FAngelscriptCachedDataType MakeSentinelDataType(
		const uint8 KeyFill,
		const uint32 QualifierFlags,
		const int32 SubTypeCount)
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::EnvironmentType;
		Type.Primitive = EAngelscriptCachedPrimitiveType::UInt16;
		Type.TypeReference = MakeSentinelReference(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol, KeyFill, KeyFill + 1);
		Type.QualifierFlags = QualifierFlags;
		Type.OrderedSubTypes.SetNum(SubTypeCount);
		return Type;
	}

	template <typename ElementType>
	static TArray<ElementType> MakeSentinelArray(const int32 Count)
	{
		TArray<ElementType> Values;
		Values.SetNum(Count);
		return Values;
	}

	struct FIndexRule
	{
		uint16 FirstField;
		uint16 LastField;
		bool bUsesPrimary;
		bool bUsesSecondary;
		bool bUsesTertiary;
	};

	enum class EDataTypeRootOwner : uint8
	{
		Global,
		HardValue,
	};

	struct FDataTypeRootIdentity
	{
		EDataTypeRootOwner Owner = EDataTypeRootOwner::Global;
		uint32 RowIndex = U;
	};

	struct FDataTypeRootCount
	{
		FDataTypeRootIdentity Identity;
		uint32 NodeCount = 0;
	};

	enum class EOptionalOccurrenceFamily : uint8
	{
		GlobalTypeReference,
		HardValueTypeReference,
		HardValueCanonicalValue,
		InitializerOwnerGlobal,
		InitializationActionDependencyExpectedValue,
		ModuleDependencyExpectedValue,
		FunctionDependencyExpectedValue,
		DebugSidecarRecordId,
	};

	struct FOptionalOccurrenceIdentity
	{
		EOptionalOccurrenceFamily Family = EOptionalOccurrenceFamily::GlobalTypeReference;
		uint32 PrimaryIndex = U;
		uint32 SecondaryIndex = U;
	};

	struct FOptionalRuleAuthority
	{
		const FOptionalOccurrenceIdentity* PresentOccurrences = nullptr;
		uint32 PresentOccurrenceCount = 0;
	};

	struct FLookupAuthority
	{
		uint32 PrimaryCount = 0;
		uint32 SecondaryCount = 0;
		uint32 TertiaryCount = 0;
		const FDataTypeRootCount* DataTypeRoots = nullptr;
		uint32 DataTypeRootCount = 0;
		FOptionalRuleAuthority OptionalRules;
	};

	inline static constexpr EAngelscriptModuleStateCapturedField ModuleStateFields[] = {
		EAngelscriptModuleStateCapturedField::Invalid,
		EAngelscriptModuleStateCapturedField::PayloadSchemaVersion,
		EAngelscriptModuleStateCapturedField::ModuleKey,
		EAngelscriptModuleStateCapturedField::Profile,
		EAngelscriptModuleStateCapturedField::StateInputHash,
		EAngelscriptModuleStateCapturedField::OrderedGlobals,
		EAngelscriptModuleStateCapturedField::Global,
		EAngelscriptModuleStateCapturedField::GlobalStorageOrdinal,
		EAngelscriptModuleStateCapturedField::GlobalKey,
		EAngelscriptModuleStateCapturedField::GlobalCanonicalNamespace,
		EAngelscriptModuleStateCapturedField::GlobalCanonicalName,
		EAngelscriptModuleStateCapturedField::GlobalTypeNode,
		EAngelscriptModuleStateCapturedField::GlobalTypeKind,
		EAngelscriptModuleStateCapturedField::GlobalTypePrimitive,
		EAngelscriptModuleStateCapturedField::GlobalTypeReferencePresence,
		EAngelscriptModuleStateCapturedField::GlobalTypeReference,
		EAngelscriptModuleStateCapturedField::GlobalTypeReferenceKind,
		EAngelscriptModuleStateCapturedField::GlobalTypeReferenceStableKey,
		EAngelscriptModuleStateCapturedField::GlobalTypeReferenceExpectedAbi,
		EAngelscriptModuleStateCapturedField::GlobalTypeQualifierFlags,
		EAngelscriptModuleStateCapturedField::GlobalTypeOrderedSubTypes,
		EAngelscriptModuleStateCapturedField::GlobalTraitFlags,
		EAngelscriptModuleStateCapturedField::GlobalInitializationKind,
		EAngelscriptModuleStateCapturedField::GlobalCleanupPolicy,
		EAngelscriptModuleStateCapturedField::GlobalStorageLayoutFingerprint,
		EAngelscriptModuleStateCapturedField::HardValues,
		EAngelscriptModuleStateCapturedField::HardValue,
		EAngelscriptModuleStateCapturedField::HardValueKind,
		EAngelscriptModuleStateCapturedField::HardValueOwner,
		EAngelscriptModuleStateCapturedField::HardValueOwnerReferenceKind,
		EAngelscriptModuleStateCapturedField::HardValueOwnerStableKey,
		EAngelscriptModuleStateCapturedField::HardValueOwnerExpectedAbi,
		EAngelscriptModuleStateCapturedField::HardValueTypeNode,
		EAngelscriptModuleStateCapturedField::HardValueTypeKind,
		EAngelscriptModuleStateCapturedField::HardValueTypePrimitive,
		EAngelscriptModuleStateCapturedField::HardValueTypeReferencePresence,
		EAngelscriptModuleStateCapturedField::HardValueTypeReference,
		EAngelscriptModuleStateCapturedField::HardValueTypeReferenceKind,
		EAngelscriptModuleStateCapturedField::HardValueTypeReferenceStableKey,
		EAngelscriptModuleStateCapturedField::HardValueTypeReferenceExpectedAbi,
		EAngelscriptModuleStateCapturedField::HardValueTypeQualifierFlags,
		EAngelscriptModuleStateCapturedField::HardValueTypeOrderedSubTypes,
		EAngelscriptModuleStateCapturedField::HardValueCanonicalValuePresence,
		EAngelscriptModuleStateCapturedField::HardValueCanonicalValue,
		EAngelscriptModuleStateCapturedField::HardValueCanonicalValueKind,
		EAngelscriptModuleStateCapturedField::HardValueCanonicalValueFixedWidthValueBytes,
		EAngelscriptModuleStateCapturedField::HardValueHash,
		EAngelscriptModuleStateCapturedField::Initializers,
		EAngelscriptModuleStateCapturedField::Initializer,
		EAngelscriptModuleStateCapturedField::InitializerKind,
		EAngelscriptModuleStateCapturedField::InitializerKey,
		EAngelscriptModuleStateCapturedField::InitializerOwnerGlobalPresence,
		EAngelscriptModuleStateCapturedField::InitializerOwnerGlobal,
		EAngelscriptModuleStateCapturedField::InitializerVmInitializerCodecVersion,
		EAngelscriptModuleStateCapturedField::InitializerExecutionHash,
		EAngelscriptModuleStateCapturedField::InitializerCanonicalExecutionPayload,
		EAngelscriptModuleStateCapturedField::OrderedInitializationActions,
		EAngelscriptModuleStateCapturedField::InitializationAction,
		EAngelscriptModuleStateCapturedField::InitializationActionOrdinal,
		EAngelscriptModuleStateCapturedField::InitializationActionKind,
		EAngelscriptModuleStateCapturedField::InitializationActionTarget,
		EAngelscriptModuleStateCapturedField::InitializationActionTargetReferenceKind,
		EAngelscriptModuleStateCapturedField::InitializationActionTargetStableKey,
		EAngelscriptModuleStateCapturedField::InitializationActionTargetExpectedAbi,
		EAngelscriptModuleStateCapturedField::InitializationActionDependencies,
		EAngelscriptModuleStateCapturedField::InitializationActionDependency,
		EAngelscriptModuleStateCapturedField::InitializationActionDependencyKind,
		EAngelscriptModuleStateCapturedField::InitializationActionDependencyTarget,
		EAngelscriptModuleStateCapturedField::InitializationActionDependencyTargetReferenceKind,
		EAngelscriptModuleStateCapturedField::InitializationActionDependencyTargetStableKey,
		EAngelscriptModuleStateCapturedField::InitializationActionDependencyTargetExpectedAbi,
		EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValuePresence,
		EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValue,
		EAngelscriptModuleStateCapturedField::OrderedPostInitFunctions,
		EAngelscriptModuleStateCapturedField::PostInitFunction,
		EAngelscriptModuleStateCapturedField::PostInitOrdinal,
		EAngelscriptModuleStateCapturedField::PostInitFunctionReference,
		EAngelscriptModuleStateCapturedField::PostInitFunctionReferenceKind,
		EAngelscriptModuleStateCapturedField::PostInitFunctionStableKey,
		EAngelscriptModuleStateCapturedField::PostInitFunctionExpectedAbi,
		EAngelscriptModuleStateCapturedField::Dependencies,
		EAngelscriptModuleStateCapturedField::Dependency,
		EAngelscriptModuleStateCapturedField::DependencyKind,
		EAngelscriptModuleStateCapturedField::DependencyTarget,
		EAngelscriptModuleStateCapturedField::DependencyTargetReferenceKind,
		EAngelscriptModuleStateCapturedField::DependencyTargetStableKey,
		EAngelscriptModuleStateCapturedField::DependencyTargetExpectedAbi,
		EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValuePresence,
		EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue,
	};

	inline static constexpr EAngelscriptFunctionBodyCapturedField FunctionBodyFields[] = {
		EAngelscriptFunctionBodyCapturedField::Invalid,
		EAngelscriptFunctionBodyCapturedField::PayloadSchemaVersion,
		EAngelscriptFunctionBodyCapturedField::ModuleKey,
		EAngelscriptFunctionBodyCapturedField::Identity,
		EAngelscriptFunctionBodyCapturedField::IdentityFunctionKey,
		EAngelscriptFunctionBodyCapturedField::IdentityContent,
		EAngelscriptFunctionBodyCapturedField::IdentityContentExecution,
		EAngelscriptFunctionBodyCapturedField::IdentityContentDebug,
		EAngelscriptFunctionBodyCapturedField::IdentityProfile,
		EAngelscriptFunctionBodyCapturedField::ExpectedDeclarationAbi,
		EAngelscriptFunctionBodyCapturedField::FunctionSourceDigest,
		EAngelscriptFunctionBodyCapturedField::FunctionInputDigest,
		EAngelscriptFunctionBodyCapturedField::InvocationKind,
		EAngelscriptFunctionBodyCapturedField::VmExecutionCodecVersion,
		EAngelscriptFunctionBodyCapturedField::CanonicalExecutionPayload,
		EAngelscriptFunctionBodyCapturedField::ActualDependencies,
		EAngelscriptFunctionBodyCapturedField::ActualDependency,
		EAngelscriptFunctionBodyCapturedField::ActualDependencyKind,
		EAngelscriptFunctionBodyCapturedField::ActualDependencyTarget,
		EAngelscriptFunctionBodyCapturedField::ActualDependencyTargetReferenceKind,
		EAngelscriptFunctionBodyCapturedField::ActualDependencyTargetStableKey,
		EAngelscriptFunctionBodyCapturedField::ActualDependencyTargetExpectedAbi,
		EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValuePresence,
		EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValue,
		EAngelscriptFunctionBodyCapturedField::DebugSidecarPresence,
		EAngelscriptFunctionBodyCapturedField::DebugSidecar,
		EAngelscriptFunctionBodyCapturedField::DebugSidecarKind,
		EAngelscriptFunctionBodyCapturedField::DebugSidecarContentHash,
	};

	inline static constexpr EAngelscriptDebugSidecarCapturedField DebugSidecarFields[] = {
		EAngelscriptDebugSidecarCapturedField::Invalid,
		EAngelscriptDebugSidecarCapturedField::PayloadSchemaVersion,
		EAngelscriptDebugSidecarCapturedField::FunctionKey,
		EAngelscriptDebugSidecarCapturedField::Profile,
		EAngelscriptDebugSidecarCapturedField::DebugHash,
		EAngelscriptDebugSidecarCapturedField::VmDebugCodecVersion,
		EAngelscriptDebugSidecarCapturedField::Sources,
		EAngelscriptDebugSidecarCapturedField::Source,
		EAngelscriptDebugSidecarCapturedField::SourceFileKey,
		EAngelscriptDebugSidecarCapturedField::SourceLogicalSectionKey,
		EAngelscriptDebugSidecarCapturedField::SourceCanonicalLogicalSection,
		EAngelscriptDebugSidecarCapturedField::CanonicalDebugPayload,
	};

	inline static constexpr EAngelscriptModuleSnapshotCapturedField ModuleSnapshotFields[] = {
		EAngelscriptModuleSnapshotCapturedField::Invalid,
		EAngelscriptModuleSnapshotCapturedField::PayloadSchemaVersion,
		EAngelscriptModuleSnapshotCapturedField::ModuleKey,
		EAngelscriptModuleSnapshotCapturedField::ModuleInterface,
		EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceModuleKey,
		EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordId,
		EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordIdKind,
		EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordIdContentHash,
		EAngelscriptModuleSnapshotCapturedField::TypeSchemas,
		EAngelscriptModuleSnapshotCapturedField::TypeSchemaLink,
		EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkTypeKey,
		EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordId,
		EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordIdKind,
		EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordIdContentHash,
		EAngelscriptModuleSnapshotCapturedField::ModuleState,
		EAngelscriptModuleSnapshotCapturedField::ModuleStateModuleKey,
		EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordId,
		EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordIdKind,
		EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordIdContentHash,
		EAngelscriptModuleSnapshotCapturedField::FunctionBodies,
		EAngelscriptModuleSnapshotCapturedField::FunctionBodyLink,
		EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkFunctionKey,
		EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordId,
		EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordIdKind,
		EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordIdContentHash,
	};

	inline static constexpr FIndexRule ModuleStateRules[] = {
		{1, 5, false, false, false},
		{6, 10, true, false, false},
		{11, 20, true, true, false},
		{21, 24, true, false, false},
		{25, 25, false, false, false},
		{26, 31, true, false, false},
		{32, 41, true, true, false},
		{42, 46, true, false, false},
		{47, 47, false, false, false},
		{48, 55, true, false, false},
		{56, 56, false, false, false},
		{57, 64, true, false, false},
		{65, 72, true, true, false},
		{73, 73, false, false, false},
		{74, 79, true, false, false},
		{80, 80, false, false, false},
		{81, 88, true, false, false},
	};

	inline static constexpr FIndexRule FunctionBodyRules[] = {
		{1, 15, false, false, false},
		{16, 23, true, false, false},
		{24, 27, false, false, false},
	};

	inline static constexpr FIndexRule DebugSidecarRules[] = {
		{1, 6, false, false, false},
		{7, 10, true, false, false},
		{11, 11, false, false, false},
	};

	inline static constexpr FIndexRule ModuleSnapshotRules[] = {
		{1, 8, false, false, false},
		{9, 13, true, false, false},
		{14, 19, false, false, false},
		{20, 24, true, false, false},
	};

	template <typename EnumType, SIZE_T Count>
	static constexpr bool EnumValuesAreContinuous(const EnumType (&Fields)[Count])
	{
		for (SIZE_T Index = 0; Index < Count; ++Index)
		{
			if (static_cast<uint16>(Fields[Index]) != Index)
			{
				return false;
			}
		}
		return true;
	}

	template <SIZE_T Count>
	static constexpr bool RulesCoverEveryFieldExactlyOnce(
		const FIndexRule (&Rules)[Count], const uint16 LastPublishedField)
	{
		for (const FIndexRule& Rule : Rules)
		{
			if (Rule.FirstField == 0 || Rule.FirstField > Rule.LastField
				|| Rule.LastField > LastPublishedField
				|| Rule.bUsesSecondary && !Rule.bUsesPrimary
				|| Rule.bUsesTertiary && !Rule.bUsesSecondary)
			{
				return false;
			}
		}
		for (uint16 Field = 1; Field <= LastPublishedField; ++Field)
		{
			uint32 Matches = 0;
			for (const FIndexRule& Rule : Rules)
			{
				Matches += Field >= Rule.FirstField && Field <= Rule.LastField;
			}
			if (Matches != 1)
			{
				return false;
			}
		}
		return true;
	}

	static bool IsOptionalOccurrencePresent(
		const FOptionalRuleAuthority& Rules,
		const EOptionalOccurrenceFamily Family,
		const uint32 PrimaryIndex,
		const uint32 SecondaryIndex)
	{
		if (Rules.PresentOccurrenceCount != 0 && Rules.PresentOccurrences == nullptr)
		{
			return false;
		}
		for (uint32 Index = 0; Index < Rules.PresentOccurrenceCount; ++Index)
		{
			const FOptionalOccurrenceIdentity& Occurrence = Rules.PresentOccurrences[Index];
			if (Occurrence.Family == Family
				&& Occurrence.PrimaryIndex == PrimaryIndex
				&& Occurrence.SecondaryIndex == SecondaryIndex)
			{
				return true;
			}
		}
		return false;
	}

	static bool IsFieldApplicable(
		const FAngelscriptModuleStateFieldCoordinate& Coordinate,
		const FOptionalRuleAuthority& Rules)
	{
		switch (Coordinate.Field)
		{
		case EAngelscriptModuleStateCapturedField::GlobalTypeReference:
		case EAngelscriptModuleStateCapturedField::GlobalTypeReferenceKind:
		case EAngelscriptModuleStateCapturedField::GlobalTypeReferenceStableKey:
		case EAngelscriptModuleStateCapturedField::GlobalTypeReferenceExpectedAbi:
			return IsOptionalOccurrencePresent(Rules,
				EOptionalOccurrenceFamily::GlobalTypeReference,
				Coordinate.PrimaryIndex, Coordinate.SecondaryIndex);
		case EAngelscriptModuleStateCapturedField::HardValueTypeReference:
		case EAngelscriptModuleStateCapturedField::HardValueTypeReferenceKind:
		case EAngelscriptModuleStateCapturedField::HardValueTypeReferenceStableKey:
		case EAngelscriptModuleStateCapturedField::HardValueTypeReferenceExpectedAbi:
			return IsOptionalOccurrencePresent(Rules,
				EOptionalOccurrenceFamily::HardValueTypeReference,
				Coordinate.PrimaryIndex, Coordinate.SecondaryIndex);
		case EAngelscriptModuleStateCapturedField::HardValueCanonicalValue:
		case EAngelscriptModuleStateCapturedField::HardValueCanonicalValueKind:
		case EAngelscriptModuleStateCapturedField::HardValueCanonicalValueFixedWidthValueBytes:
			return IsOptionalOccurrencePresent(Rules,
				EOptionalOccurrenceFamily::HardValueCanonicalValue,
				Coordinate.PrimaryIndex, U);
		case EAngelscriptModuleStateCapturedField::InitializerOwnerGlobal:
			return IsOptionalOccurrencePresent(Rules,
				EOptionalOccurrenceFamily::InitializerOwnerGlobal,
				Coordinate.PrimaryIndex, U);
		case EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValue:
			return IsOptionalOccurrencePresent(Rules,
				EOptionalOccurrenceFamily::InitializationActionDependencyExpectedValue,
				Coordinate.PrimaryIndex, Coordinate.SecondaryIndex);
		case EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue:
			return IsOptionalOccurrencePresent(Rules,
				EOptionalOccurrenceFamily::ModuleDependencyExpectedValue,
				Coordinate.PrimaryIndex, U);
		default:
			return true;
		}
	}

	static bool IsFieldApplicable(
		const FAngelscriptFunctionBodyFieldCoordinate& Coordinate,
		const FOptionalRuleAuthority& Rules)
	{
		switch (Coordinate.Field)
		{
		case EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValue:
			return IsOptionalOccurrencePresent(Rules,
				EOptionalOccurrenceFamily::FunctionDependencyExpectedValue,
				Coordinate.PrimaryIndex, U);
		case EAngelscriptFunctionBodyCapturedField::DebugSidecar:
		case EAngelscriptFunctionBodyCapturedField::DebugSidecarKind:
		case EAngelscriptFunctionBodyCapturedField::DebugSidecarContentHash:
			return IsOptionalOccurrencePresent(Rules,
				EOptionalOccurrenceFamily::DebugSidecarRecordId, U, U);
		default:
			return true;
		}
	}

	static bool IsFieldApplicable(
		const FAngelscriptDebugSidecarFieldCoordinate&,
		const FOptionalRuleAuthority&)
	{
		return true;
	}

	static bool IsFieldApplicable(
		const FAngelscriptModuleSnapshotFieldCoordinate&,
		const FOptionalRuleAuthority&)
	{
		return true;
	}

	template <SIZE_T Count>
	static const FIndexRule* FindIndexRule(
		const uint16 Field, const FIndexRule (&Rules)[Count])
	{
		for (const FIndexRule& Rule : Rules)
		{
			if (Field >= Rule.FirstField && Field <= Rule.LastField)
			{
				return &Rule;
			}
		}
		return nullptr;
	}

	static TOptional<uint32> FindDataTypeNodeCount(
		const FLookupAuthority& Authority,
		const EDataTypeRootOwner Owner,
		const uint32 RowIndex)
	{
		if (Authority.DataTypeRootCount != 0 && Authority.DataTypeRoots == nullptr)
		{
			return {};
		}
		for (uint32 Index = 0; Index < Authority.DataTypeRootCount; ++Index)
		{
			const FDataTypeRootCount& Root = Authority.DataTypeRoots[Index];
			if (Root.Identity.Owner == Owner && Root.Identity.RowIndex == RowIndex)
			{
				return Root.NodeCount;
			}
		}
		return {};
	}

	static TOptional<uint32> FindModuleStateSecondaryBound(
		const FAngelscriptModuleStateFieldCoordinate& Coordinate,
		const FLookupAuthority& Authority)
	{
		const uint16 Field = static_cast<uint16>(Coordinate.Field);
		if (Field >= static_cast<uint16>(EAngelscriptModuleStateCapturedField::GlobalTypeNode)
			&& Field <= static_cast<uint16>(
				EAngelscriptModuleStateCapturedField::GlobalTypeOrderedSubTypes))
		{
			return FindDataTypeNodeCount(
				Authority, EDataTypeRootOwner::Global, Coordinate.PrimaryIndex);
		}
		if (Field >= static_cast<uint16>(
				EAngelscriptModuleStateCapturedField::HardValueTypeNode)
			&& Field <= static_cast<uint16>(
				EAngelscriptModuleStateCapturedField::HardValueTypeOrderedSubTypes))
		{
			return FindDataTypeNodeCount(
				Authority, EDataTypeRootOwner::HardValue, Coordinate.PrimaryIndex);
		}
		return Authority.SecondaryCount;
	}

	template <SIZE_T Count, typename FieldType>
	static bool AreAxesAccepted(
		const FieldType Field,
		const uint32 PrimaryIndex,
		const uint32 SecondaryIndex,
		const uint32 TertiaryIndex,
		const FLookupAuthority& Authority,
		const FIndexRule (&Rules)[Count],
		const TOptional<uint32> SecondaryBound)
	{
		const FIndexRule* MatchingRule = FindIndexRule(static_cast<uint16>(Field), Rules);
		if (MatchingRule == nullptr)
		{
			return false;
		}
		if (MatchingRule->bUsesSecondary && !SecondaryBound.IsSet())
		{
			return false;
		}
		const auto AxisIsAccepted = [](const bool bUsed, const uint32 Index,
			const uint32 Bound)
		{
			return bUsed ? Index != MAX_uint32 && Index < Bound : Index == MAX_uint32;
		};
		return AxisIsAccepted(MatchingRule->bUsesPrimary,
			PrimaryIndex, Authority.PrimaryCount)
			&& AxisIsAccepted(MatchingRule->bUsesSecondary,
				SecondaryIndex, SecondaryBound.Get(Authority.SecondaryCount))
			&& AxisIsAccepted(MatchingRule->bUsesTertiary,
				TertiaryIndex, Authority.TertiaryCount);
	}

	static bool IsCoordinateAccepted(
		const FAngelscriptModuleStateFieldCoordinate& Coordinate,
		const FLookupAuthority& Authority)
	{
		return IsFieldApplicable(Coordinate, Authority.OptionalRules)
			&& AreAxesAccepted(Coordinate.Field, Coordinate.PrimaryIndex,
				Coordinate.SecondaryIndex, Coordinate.TertiaryIndex, Authority,
				ModuleStateRules, FindModuleStateSecondaryBound(Coordinate, Authority));
	}

	static bool IsCoordinateAccepted(
		const FAngelscriptFunctionBodyFieldCoordinate& Coordinate,
		const FLookupAuthority& Authority)
	{
		return IsFieldApplicable(Coordinate, Authority.OptionalRules)
			&& AreAxesAccepted(Coordinate.Field, Coordinate.PrimaryIndex,
				Coordinate.SecondaryIndex, Coordinate.TertiaryIndex, Authority,
				FunctionBodyRules, Authority.SecondaryCount);
	}

	static bool IsCoordinateAccepted(
		const FAngelscriptDebugSidecarFieldCoordinate& Coordinate,
		const FLookupAuthority& Authority)
	{
		return IsFieldApplicable(Coordinate, Authority.OptionalRules)
			&& AreAxesAccepted(Coordinate.Field, Coordinate.PrimaryIndex,
				Coordinate.SecondaryIndex, Coordinate.TertiaryIndex, Authority,
				DebugSidecarRules, Authority.SecondaryCount);
	}

	static bool IsCoordinateAccepted(
		const FAngelscriptModuleSnapshotFieldCoordinate& Coordinate,
		const FLookupAuthority& Authority)
	{
		return IsFieldApplicable(Coordinate, Authority.OptionalRules)
			&& AreAxesAccepted(Coordinate.Field, Coordinate.PrimaryIndex,
				Coordinate.SecondaryIndex, Coordinate.TertiaryIndex, Authority,
				ModuleSnapshotRules, Authority.SecondaryCount);
	}

	static EAngelscriptCacheRecordKind ExpectedRecordKind(
		const FAngelscriptModuleStateFieldCoordinate&)
	{
		return EAngelscriptCacheRecordKind::ModuleState;
	}

	static EAngelscriptCacheRecordKind ExpectedRecordKind(
		const FAngelscriptFunctionBodyFieldCoordinate&)
	{
		return EAngelscriptCacheRecordKind::FunctionBody;
	}

	static EAngelscriptCacheRecordKind ExpectedRecordKind(
		const FAngelscriptDebugSidecarFieldCoordinate&)
	{
		return EAngelscriptCacheRecordKind::DebugSidecar;
	}

	static EAngelscriptCacheRecordKind ExpectedRecordKind(
		const FAngelscriptModuleSnapshotFieldCoordinate&)
	{
		return EAngelscriptCacheRecordKind::ModuleSnapshot;
	}

	template <typename CoordinateType>
	static TOptional<uint64> FindIndependentContractOffset(
		const EAngelscriptCacheRecordKind ActiveKind,
		const CoordinateType& Coordinate,
		const FLookupAuthority& Authority,
		const uint64 CapturedOffset)
	{
		if (ActiveKind != ExpectedRecordKind(Coordinate)
			|| !IsCoordinateAccepted(Coordinate, Authority))
		{
			return {};
		}
		return CapturedOffset;
	}

	template <typename ValueType>
	static constexpr bool IsRecursivelyOwnedType(const ValueType*)
	{
		using Type = std::remove_cv_t<ValueType>;
		return !std::is_pointer_v<Type>
			&& (std::is_arithmetic_v<Type>
				|| std::is_enum_v<Type>
				|| std::is_same_v<Type, FString>
				|| std::is_same_v<Type, FAngelscriptHash256>
				|| std::is_same_v<Type, FAngelscriptStableGlobalKey>
				|| std::is_same_v<Type, FAngelscriptCachedDataType>
				|| std::is_same_v<Type, FAngelscriptCacheStableReference>
				|| std::is_same_v<Type, FAngelscriptStableFunctionKey>
				|| std::is_same_v<Type, FAngelscriptCacheSemanticDependency>
				|| std::is_same_v<Type, FAngelscriptStableModuleKey>
				|| std::is_same_v<Type, FAngelscriptArtifactProfileKey>
				|| std::is_same_v<Type, FAngelscriptFunctionArtifactIdentity>
				|| std::is_same_v<Type, FAngelscriptFunctionSourceDigest>
				|| std::is_same_v<Type, FAngelscriptFunctionInputDigest>
				|| std::is_same_v<Type, FAngelscriptCacheRecordId>
				|| std::is_same_v<Type, FAngelscriptCachedSourceFileKey>
				|| std::is_same_v<Type, FAngelscriptStableTypeKey>
				|| std::is_same_v<Type, FAngelscriptCachedGlobalSchema>
				|| std::is_same_v<Type, FAngelscriptCachedCanonicalValue>
				|| std::is_same_v<Type, FAngelscriptCachedHardValue>
				|| std::is_same_v<Type, FAngelscriptCachedInitializerUnit>
				|| std::is_same_v<Type, FAngelscriptCachedInitializationAction>
				|| std::is_same_v<Type, FAngelscriptCachedPostInitFunction>
				|| std::is_same_v<Type, FAngelscriptCachedLogicalSectionKey>
				|| std::is_same_v<Type, FAngelscriptCachedDebugSourceReference>
				|| std::is_same_v<Type, FAngelscriptCachedModuleRecordLink>
				|| std::is_same_v<Type, FAngelscriptCachedTypeSchemaLink>
				|| std::is_same_v<Type, FAngelscriptCachedFunctionBodyLink>);
	}

	template <typename ElementType, typename AllocatorType>
	static constexpr bool IsRecursivelyOwnedType(const TArray<ElementType, AllocatorType>*)
	{
		return IsRecursivelyOwnedType(static_cast<const ElementType*>(nullptr));
	}

	template <typename ElementType>
	static constexpr bool IsRecursivelyOwnedType(const TOptional<ElementType>*)
	{
		return IsRecursivelyOwnedType(static_cast<const ElementType*>(nullptr));
	}

	template <typename... MemberTypes>
	inline static constexpr bool AreRecursivelyOwnedMembers =
		(IsRecursivelyOwnedType(static_cast<const
			std::remove_cvref_t<MemberTypes>*>(nullptr)) && ...);

	template <typename... MemberTypes>
	using TMemberTypeTuple = std::tuple<std::remove_cvref_t<MemberTypes>...>;

	template <typename CoordinateType, typename FieldType>
	static bool DeclarationFirstMatrixIsExact(
		const EAngelscriptCacheRecordKind ActiveKind,
		const EAngelscriptCacheRecordKind WrongKind,
		const FieldType TopLevelField,
		const FieldType PrimaryField,
		const FieldType SecondaryField,
		const bool bHasUsedSecondary,
		const uint16 LastPublishedField,
		const uint32 UsedSecondaryBound,
		const FLookupAuthority& Authority)
	{
		const auto Missing = [&](const EAngelscriptCacheRecordKind Kind,
			const CoordinateType& Coordinate)
		{
			return !FindIndependentContractOffset(
				Kind, Coordinate, Authority, uint64(7)).IsSet();
		};
		const TOptional<uint64> Zero = FindIndependentContractOffset(
			ActiveKind, CoordinateType{TopLevelField}, Authority, uint64(0));
		if (!Zero.IsSet() || Zero.GetValue() != 0
			|| !Missing(WrongKind, CoordinateType{TopLevelField})
			|| !Missing(ActiveKind, CoordinateType{})
			|| !Missing(ActiveKind, CoordinateType{
				static_cast<FieldType>(LastPublishedField + 1)}))
		{
			return false;
		}

		// Every coordinate kind rejects values supplied on each unused axis.
		if (!Missing(ActiveKind, CoordinateType{TopLevelField, 0, U, U})
			|| !Missing(ActiveKind, CoordinateType{TopLevelField, U, 0, U})
			|| !Missing(ActiveKind, CoordinateType{TopLevelField, U, U, 0}))
		{
			return false;
		}

		// Every coordinate kind has a primary-indexed field: missing, OOB and surplus axes.
		if (!Missing(ActiveKind, CoordinateType{PrimaryField})
			|| !Missing(ActiveKind, CoordinateType{
				PrimaryField, Authority.PrimaryCount, U, U})
			|| !Missing(ActiveKind, CoordinateType{PrimaryField, 0, 0, U})
			|| !Missing(ActiveKind, CoordinateType{PrimaryField, 0, U, 0}))
		{
			return false;
		}

		if (bHasUsedSecondary)
		{
			// ModuleState is the only remaining coordinate with a used secondary axis.
			return Missing(ActiveKind, CoordinateType{SecondaryField, 0, U, U})
				&& Missing(ActiveKind, CoordinateType{
					SecondaryField, 0, UsedSecondaryBound, U})
				&& Missing(ActiveKind, CoordinateType{SecondaryField, 0, 0, 0});
		}
		return true;
	}

	static FLookupAuthority WithOnlyPresentOccurrence(
		const FLookupAuthority& Base,
		const FOptionalOccurrenceIdentity& Occurrence)
	{
		FLookupAuthority Result = Base;
		Result.OptionalRules.PresentOccurrences = &Occurrence;
		Result.OptionalRules.PresentOccurrenceCount = 1;
		return Result;
	}

	template <typename CoordinateType, SIZE_T FieldCount, SIZE_T RepresentativeCount>
	static bool RepeatedOptionalOccurrenceMatrixIsExact(
		const FLookupAuthority& Base,
		const FOptionalOccurrenceIdentity& OccurrenceA,
		const FOptionalOccurrenceIdentity& OccurrenceB,
		const CoordinateType& PresenceA,
		const CoordinateType& PresenceB,
		const CoordinateType (&ControlledFieldsA)[FieldCount],
		const CoordinateType (&ControlledFieldsB)[FieldCount],
		const FOptionalOccurrenceIdentity (&Representatives)[RepresentativeCount])
	{
		const FLookupAuthority PresentA = WithOnlyPresentOccurrence(Base, OccurrenceA);
		const FLookupAuthority PresentB = WithOnlyPresentOccurrence(Base, OccurrenceB);
		if (!IsCoordinateAccepted(PresenceA, Base)
			|| !IsCoordinateAccepted(PresenceB, Base)
			|| !IsCoordinateAccepted(PresenceA, PresentA)
			|| !IsCoordinateAccepted(PresenceB, PresentB))
		{
			return false;
		}

		for (const FOptionalOccurrenceIdentity& Representative : Representatives)
		{
			const FLookupAuthority Unrelated =
				WithOnlyPresentOccurrence(Base, Representative);
			if (!IsCoordinateAccepted(PresenceA, Unrelated)
				|| !IsCoordinateAccepted(PresenceB, Unrelated))
			{
				return false;
			}
		}

		for (SIZE_T Index = 0; Index < FieldCount; ++Index)
		{
			if (IsCoordinateAccepted(ControlledFieldsA[Index], Base)
				|| !IsCoordinateAccepted(ControlledFieldsA[Index], PresentA)
				|| IsCoordinateAccepted(ControlledFieldsA[Index], PresentB)
				|| IsCoordinateAccepted(ControlledFieldsB[Index], Base)
				|| !IsCoordinateAccepted(ControlledFieldsB[Index], PresentB)
				|| IsCoordinateAccepted(ControlledFieldsB[Index], PresentA))
			{
				return false;
			}

			for (const FOptionalOccurrenceIdentity& Representative : Representatives)
			{
				if (Representative.Family == OccurrenceA.Family)
				{
					continue;
				}
				const FLookupAuthority Unrelated =
					WithOnlyPresentOccurrence(Base, Representative);
				if (IsCoordinateAccepted(ControlledFieldsA[Index], Unrelated)
					|| IsCoordinateAccepted(ControlledFieldsB[Index], Unrelated))
				{
					return false;
				}
			}
		}
		return true;
	}

	template <typename CoordinateType, SIZE_T FieldCount, SIZE_T RepresentativeCount>
	static bool TwoAxisOptionalOccurrenceMatrixIsExact(
		const FLookupAuthority& Base,
		const FOptionalOccurrenceIdentity& OccurrenceA,
		const FOptionalOccurrenceIdentity& OccurrenceB,
		const FOptionalOccurrenceIdentity& OccurrenceC,
		const CoordinateType& PresenceA,
		const CoordinateType& PresenceB,
		const CoordinateType& PresenceC,
		const CoordinateType (&ControlledFieldsA)[FieldCount],
		const CoordinateType (&ControlledFieldsB)[FieldCount],
		const CoordinateType (&ControlledFieldsC)[FieldCount],
		const FOptionalOccurrenceIdentity (&Representatives)[RepresentativeCount])
	{
		// A={0,0}, B={0,1}, C={1,0}: B changes only Secondary and C only Primary.
		if (OccurrenceA.PrimaryIndex != 0 || OccurrenceA.SecondaryIndex != 0
			|| OccurrenceB.PrimaryIndex != 0 || OccurrenceB.SecondaryIndex != 1
			|| OccurrenceC.PrimaryIndex != 1 || OccurrenceC.SecondaryIndex != 0
			|| OccurrenceA.Family != OccurrenceB.Family
			|| OccurrenceA.Family != OccurrenceC.Family
			|| OccurrenceA.PrimaryIndex != OccurrenceB.PrimaryIndex
			|| OccurrenceA.SecondaryIndex == OccurrenceB.SecondaryIndex
			|| OccurrenceA.PrimaryIndex == OccurrenceC.PrimaryIndex
			|| OccurrenceA.SecondaryIndex != OccurrenceC.SecondaryIndex)
		{
			return false;
		}

		const FLookupAuthority PresentA = WithOnlyPresentOccurrence(Base, OccurrenceA);
		const FLookupAuthority PresentB = WithOnlyPresentOccurrence(Base, OccurrenceB);
		const FLookupAuthority PresentC = WithOnlyPresentOccurrence(Base, OccurrenceC);
		const auto PresenceIsAlwaysQueryable = [&](const CoordinateType& Presence)
		{
			if (!IsCoordinateAccepted(Presence, Base)
				|| !IsCoordinateAccepted(Presence, PresentA)
				|| !IsCoordinateAccepted(Presence, PresentB)
				|| !IsCoordinateAccepted(Presence, PresentC))
			{
				return false;
			}
			for (const FOptionalOccurrenceIdentity& Representative : Representatives)
			{
				if (!IsCoordinateAccepted(
					Presence, WithOnlyPresentOccurrence(Base, Representative)))
				{
					return false;
				}
			}
			return true;
		};
		if (!PresenceIsAlwaysQueryable(PresenceA)
			|| !PresenceIsAlwaysQueryable(PresenceB)
			|| !PresenceIsAlwaysQueryable(PresenceC))
		{
			return false;
		}

		const auto ControlledFieldsAreOneHot = [&](
			const CoordinateType (&ControlledFields)[FieldCount],
			const FLookupAuthority& Own,
			const FLookupAuthority& OtherA,
			const FLookupAuthority& OtherB)
		{
			for (const CoordinateType& ControlledField : ControlledFields)
			{
				if (IsCoordinateAccepted(ControlledField, Base)
					|| !IsCoordinateAccepted(ControlledField, Own)
					|| IsCoordinateAccepted(ControlledField, OtherA)
					|| IsCoordinateAccepted(ControlledField, OtherB))
				{
					return false;
				}
				for (const FOptionalOccurrenceIdentity& Representative : Representatives)
				{
					if (Representative.Family == OccurrenceA.Family)
					{
						continue;
					}
					if (IsCoordinateAccepted(ControlledField,
						WithOnlyPresentOccurrence(Base, Representative)))
					{
						return false;
					}
				}
			}
			return true;
		};

		return ControlledFieldsAreOneHot(ControlledFieldsA, PresentA, PresentB, PresentC)
			&& ControlledFieldsAreOneHot(ControlledFieldsB, PresentB, PresentA, PresentC)
			&& ControlledFieldsAreOneHot(ControlledFieldsC, PresentC, PresentA, PresentB);
	}

	template <typename CoordinateType, SIZE_T FieldCount, SIZE_T RepresentativeCount>
	static bool SingletonOptionalOccurrenceMatrixIsExact(
		const FLookupAuthority& Base,
		const FOptionalOccurrenceIdentity& Occurrence,
		const CoordinateType& Presence,
		const CoordinateType (&ControlledFields)[FieldCount],
		const FOptionalOccurrenceIdentity (&Representatives)[RepresentativeCount])
	{
		const FLookupAuthority Present = WithOnlyPresentOccurrence(Base, Occurrence);
		if (!IsCoordinateAccepted(Presence, Base)
			|| !IsCoordinateAccepted(Presence, Present))
		{
			return false;
		}

		for (const FOptionalOccurrenceIdentity& Representative : Representatives)
		{
			const FLookupAuthority Unrelated =
				WithOnlyPresentOccurrence(Base, Representative);
			if (!IsCoordinateAccepted(Presence, Unrelated))
			{
				return false;
			}
		}

		for (const CoordinateType& ControlledField : ControlledFields)
		{
			if (IsCoordinateAccepted(ControlledField, Base)
				|| !IsCoordinateAccepted(ControlledField, Present))
			{
				return false;
			}
			for (const FOptionalOccurrenceIdentity& Representative : Representatives)
			{
				if (Representative.Family == Occurrence.Family)
				{
					continue;
				}
				const FLookupAuthority Unrelated =
					WithOnlyPresentOccurrence(Base, Representative);
				if (IsCoordinateAccepted(ControlledField, Unrelated))
				{
					return false;
				}
			}
		}
		return true;
	}

	template <typename ValueType>
	inline static constexpr bool IsOwnedAggregateDto =
		std::is_aggregate_v<ValueType>
		&& std::is_default_constructible_v<ValueType>
		&& std::is_copy_constructible_v<ValueType>
		&& std::is_copy_assignable_v<ValueType>
		&& std::is_move_constructible_v<ValueType>
		&& std::is_move_assignable_v<ValueType>
		&& std::is_destructible_v<ValueType>;

	template <typename ValueType>
	inline static constexpr bool HasLegacySourceDigestMember =
		requires(ValueType Value) { Value.SourceDigest; };

	template <typename ValueType>
	inline static constexpr bool HasLegacyInputDigestMember =
		requires(ValueType Value) { Value.InputDigest; };

	template <typename ValueType>
	inline static constexpr bool HasManifestOnlyModuleSnapshotsMember =
		requires(ValueType Value) { Value.ModuleSnapshots; };

	template <typename ReceiverType, typename CoordinateArgumentType>
	inline static constexpr bool HasCallerBooleanFindCapturedOffset =
		requires
		{
			std::declval<ReceiverType>().FindCapturedOffset(
				std::declval<CoordinateArgumentType>(), true);
		};

	template <typename CoordinateType>
	inline static constexpr bool RejectsCallerBooleanFindCapturedOffsetInEveryValueCategory =
		!HasCallerBooleanFindCapturedOffset<
			const FAngelscriptDecodedCacheRecord&, const CoordinateType&>
		&& !HasCallerBooleanFindCapturedOffset<
			const FAngelscriptDecodedCacheRecord&, CoordinateType&>
		&& !HasCallerBooleanFindCapturedOffset<
			const FAngelscriptDecodedCacheRecord&, CoordinateType&&>
		&& !HasCallerBooleanFindCapturedOffset<
			FAngelscriptDecodedCacheRecord&, const CoordinateType&>
		&& !HasCallerBooleanFindCapturedOffset<
			FAngelscriptDecodedCacheRecord&, CoordinateType&>
		&& !HasCallerBooleanFindCapturedOffset<
			FAngelscriptDecodedCacheRecord&, CoordinateType&&>;

public:
	TEST_METHOD(AppendOnlyEnumValuesAreContinuous)
	{
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCachedGlobalInitializationKind>, uint8>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCachedGlobalCleanupPolicy>, uint8>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCachedHardValueKind>, uint8>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCachedInitializerKind>, uint8>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCachedInitializationActionKind>, uint8>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCachedCanonicalValueKind>, uint8>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCachedFunctionInvocationKind>, uint8>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCacheValueStorageKind>, uint8>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptCacheOpaquePayloadKind>, uint8>);
		static_assert(static_cast<uint8>(EAngelscriptCachedGlobalInitializationKind::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCachedGlobalInitializationKind::Default) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedGlobalInitializationKind::PureConstant) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedGlobalInitializationKind::VmInitializer) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedGlobalCleanupPolicy::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCachedGlobalCleanupPolicy::None) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedGlobalCleanupPolicy::DestroyValue) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedGlobalCleanupPolicy::ReleaseHandle) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedHardValueKind::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCachedHardValueKind::GlobalConstant) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedHardValueKind::EnumAuthority) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedInitializerKind::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCachedInitializerKind::Global) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedInitializerKind::Module) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedInitializationActionKind::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCachedInitializationActionKind::DefaultConstructGlobal) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedInitializationActionKind::ExecuteInitializer) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedCanonicalValueKind::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCachedCanonicalValueKind::Bool) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedCanonicalValueKind::SignedInteger) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedCanonicalValueKind::UnsignedInteger) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedCanonicalValueKind::Float32) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCachedCanonicalValueKind::Float64) == 5);
		static_assert(static_cast<uint8>(EAngelscriptCachedCanonicalValueKind::EnumInt32) == 6);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::GlobalFunction) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::Method) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::Constructor) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::Destructor) == 4);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::Factory) == 5);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultConstructor) == 6);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultDestructor) == 7);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::InitDefaults) == 8);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::PublicSingleFunction) == 9);
		static_assert(static_cast<uint8>(EAngelscriptCachedFunctionInvocationKind::Lambda) == 10);
		static_assert(static_cast<uint8>(EAngelscriptCacheValueStorageKind::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCacheValueStorageKind::Trivial) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCacheValueStorageKind::OwningValue) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCacheValueStorageKind::ReferenceCounted) == 3);
		static_assert(static_cast<uint8>(EAngelscriptCacheOpaquePayloadKind::Invalid) == 0);
		static_assert(static_cast<uint8>(EAngelscriptCacheOpaquePayloadKind::FunctionExecution) == 1);
		static_assert(static_cast<uint8>(EAngelscriptCacheOpaquePayloadKind::InitializerExecution) == 2);
		static_assert(static_cast<uint8>(EAngelscriptCacheOpaquePayloadKind::Debug) == 3);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptModuleStateCapturedField>, uint16>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptFunctionBodyCapturedField>, uint16>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptDebugSidecarCapturedField>, uint16>);
		static_assert(std::is_same_v<
			std::underlying_type_t<EAngelscriptModuleSnapshotCapturedField>, uint16>);
		static_assert(UE_ARRAY_COUNT(ModuleStateFields) == 89);
		static_assert(UE_ARRAY_COUNT(FunctionBodyFields) == 28);
		static_assert(UE_ARRAY_COUNT(DebugSidecarFields) == 12);
		static_assert(UE_ARRAY_COUNT(ModuleSnapshotFields) == 25);
		static_assert(EnumValuesAreContinuous(ModuleStateFields));
		static_assert(EnumValuesAreContinuous(FunctionBodyFields));
		static_assert(EnumValuesAreContinuous(DebugSidecarFields));
		static_assert(EnumValuesAreContinuous(ModuleSnapshotFields));
	}

	TEST_METHOD(CoordinateDefaultsAndPublicLookupApiAreExact)
	{
		static_assert(std::is_aggregate_v<FAngelscriptModuleStateFieldCoordinate>);
		static_assert(std::is_aggregate_v<FAngelscriptFunctionBodyFieldCoordinate>);
		static_assert(std::is_aggregate_v<FAngelscriptDebugSidecarFieldCoordinate>);
		static_assert(std::is_aggregate_v<FAngelscriptModuleSnapshotFieldCoordinate>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptModuleStateFieldCoordinate{}.Field),
			EAngelscriptModuleStateCapturedField>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptFunctionBodyFieldCoordinate{}.Field),
			EAngelscriptFunctionBodyCapturedField>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptDebugSidecarFieldCoordinate{}.Field),
			EAngelscriptDebugSidecarCapturedField>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptModuleSnapshotFieldCoordinate{}.Field),
			EAngelscriptModuleSnapshotCapturedField>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptModuleStateFieldCoordinate{}.PrimaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptModuleStateFieldCoordinate{}.SecondaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptModuleStateFieldCoordinate{}.TertiaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptFunctionBodyFieldCoordinate{}.PrimaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptFunctionBodyFieldCoordinate{}.SecondaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptFunctionBodyFieldCoordinate{}.TertiaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptDebugSidecarFieldCoordinate{}.PrimaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptDebugSidecarFieldCoordinate{}.SecondaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptDebugSidecarFieldCoordinate{}.TertiaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptModuleSnapshotFieldCoordinate{}.PrimaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptModuleSnapshotFieldCoordinate{}.SecondaryIndex), uint32>);
		static_assert(std::is_same_v<decltype(
			FAngelscriptModuleSnapshotFieldCoordinate{}.TertiaryIndex), uint32>);
		static_assert(FAngelscriptModuleStateFieldCoordinate{}.Field
			== EAngelscriptModuleStateCapturedField::Invalid);
		static_assert(FAngelscriptFunctionBodyFieldCoordinate{}.Field
			== EAngelscriptFunctionBodyCapturedField::Invalid);
		static_assert(FAngelscriptDebugSidecarFieldCoordinate{}.Field
			== EAngelscriptDebugSidecarCapturedField::Invalid);
		static_assert(FAngelscriptModuleSnapshotFieldCoordinate{}.Field
			== EAngelscriptModuleSnapshotCapturedField::Invalid);
		static_assert(FAngelscriptModuleStateFieldCoordinate{}.PrimaryIndex == U);
		static_assert(FAngelscriptModuleStateFieldCoordinate{}.SecondaryIndex == U);
		static_assert(FAngelscriptModuleStateFieldCoordinate{}.TertiaryIndex == U);
		static_assert(FAngelscriptFunctionBodyFieldCoordinate{}.PrimaryIndex == U);
		static_assert(FAngelscriptFunctionBodyFieldCoordinate{}.SecondaryIndex == U);
		static_assert(FAngelscriptFunctionBodyFieldCoordinate{}.TertiaryIndex == U);
		static_assert(FAngelscriptDebugSidecarFieldCoordinate{}.PrimaryIndex == U);
		static_assert(FAngelscriptDebugSidecarFieldCoordinate{}.SecondaryIndex == U);
		static_assert(FAngelscriptDebugSidecarFieldCoordinate{}.TertiaryIndex == U);
		static_assert(FAngelscriptModuleSnapshotFieldCoordinate{}.PrimaryIndex == U);
		static_assert(FAngelscriptModuleSnapshotFieldCoordinate{}.SecondaryIndex == U);
		static_assert(FAngelscriptModuleSnapshotFieldCoordinate{}.TertiaryIndex == U);
		static_assert(!std::is_convertible_v<FAngelscriptModuleStateFieldCoordinate,
			FAngelscriptFunctionBodyFieldCoordinate>);
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
		static_assert(RejectsCallerBooleanFindCapturedOffsetInEveryValueCategory<
			FAngelscriptModuleStateFieldCoordinate>);
		static_assert(RejectsCallerBooleanFindCapturedOffsetInEveryValueCategory<
			FAngelscriptFunctionBodyFieldCoordinate>);
		static_assert(RejectsCallerBooleanFindCapturedOffsetInEveryValueCategory<
			FAngelscriptDebugSidecarFieldCoordinate>);
		static_assert(RejectsCallerBooleanFindCapturedOffsetInEveryValueCategory<
			FAngelscriptModuleSnapshotFieldCoordinate>);
	}

	TEST_METHOD(StructuredBindingsFreezeExactMemberCountOrderAndOwnedTypes)
	{
		static_assert(!AreRecursivelyOwnedMembers<uint8*>);
		static_assert(!AreRecursivelyOwnedMembers<TArray<uint8*>>);
		static_assert(!AreRecursivelyOwnedMembers<TOptional<uint8*>>);
		{
			FAngelscriptModuleStateFieldCoordinate Value;
			auto& [Field, PrimaryIndex, SecondaryIndex, TertiaryIndex] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(Field),
				decltype(PrimaryIndex), decltype(SecondaryIndex), decltype(TertiaryIndex)>,
				std::tuple<EAngelscriptModuleStateCapturedField, uint32, uint32, uint32>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(Field),
				decltype(PrimaryIndex), decltype(SecondaryIndex), decltype(TertiaryIndex)>);
		}
		{
			FAngelscriptFunctionBodyFieldCoordinate Value;
			auto& [Field, PrimaryIndex, SecondaryIndex, TertiaryIndex] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(Field),
				decltype(PrimaryIndex), decltype(SecondaryIndex), decltype(TertiaryIndex)>,
				std::tuple<EAngelscriptFunctionBodyCapturedField, uint32, uint32, uint32>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(Field),
				decltype(PrimaryIndex), decltype(SecondaryIndex), decltype(TertiaryIndex)>);
		}
		{
			FAngelscriptDebugSidecarFieldCoordinate Value;
			auto& [Field, PrimaryIndex, SecondaryIndex, TertiaryIndex] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(Field),
				decltype(PrimaryIndex), decltype(SecondaryIndex), decltype(TertiaryIndex)>,
				std::tuple<EAngelscriptDebugSidecarCapturedField, uint32, uint32, uint32>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(Field),
				decltype(PrimaryIndex), decltype(SecondaryIndex), decltype(TertiaryIndex)>);
		}
		{
			FAngelscriptModuleSnapshotFieldCoordinate Value;
			auto& [Field, PrimaryIndex, SecondaryIndex, TertiaryIndex] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(Field),
				decltype(PrimaryIndex), decltype(SecondaryIndex), decltype(TertiaryIndex)>,
				std::tuple<EAngelscriptModuleSnapshotCapturedField, uint32, uint32, uint32>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(Field),
				decltype(PrimaryIndex), decltype(SecondaryIndex), decltype(TertiaryIndex)>);
		}

		{
			FAngelscriptCachedGlobalSchema Value;
			auto& [StorageOrdinal, GlobalKey, CanonicalNamespace, CanonicalName, Type,
				GlobalTraitFlags, InitializationKind, CleanupPolicy,
				StorageLayoutFingerprint] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(StorageOrdinal),
				decltype(GlobalKey), decltype(CanonicalNamespace), decltype(CanonicalName),
				decltype(Type), decltype(GlobalTraitFlags), decltype(InitializationKind),
				decltype(CleanupPolicy), decltype(StorageLayoutFingerprint)>,
				std::tuple<uint32, FAngelscriptStableGlobalKey, FString, FString,
					FAngelscriptCachedDataType, uint32,
					EAngelscriptCachedGlobalInitializationKind,
					EAngelscriptCachedGlobalCleanupPolicy, FAngelscriptHash256>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(StorageOrdinal),
				decltype(GlobalKey), decltype(CanonicalNamespace), decltype(CanonicalName),
				decltype(Type), decltype(GlobalTraitFlags), decltype(InitializationKind),
				decltype(CleanupPolicy), decltype(StorageLayoutFingerprint)>);
		}
		{
			FAngelscriptCachedCanonicalValue Value;
			auto& [ValueKind, FixedWidthValueBytes] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(ValueKind),
				decltype(FixedWidthValueBytes)>,
				std::tuple<EAngelscriptCachedCanonicalValueKind, TArray<uint8>>>);
			static_assert(AreRecursivelyOwnedMembers<
				decltype(ValueKind), decltype(FixedWidthValueBytes)>);
		}
		{
			FAngelscriptCachedHardValue Value;
			auto& [HardValueKind, Owner, Type, CanonicalValue, HardValueHash] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(HardValueKind),
				decltype(Owner), decltype(Type), decltype(CanonicalValue),
				decltype(HardValueHash)>,
				std::tuple<EAngelscriptCachedHardValueKind,
					FAngelscriptCacheStableReference, FAngelscriptCachedDataType,
					TOptional<FAngelscriptCachedCanonicalValue>, FAngelscriptHash256>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(HardValueKind),
				decltype(Owner), decltype(Type), decltype(CanonicalValue),
				decltype(HardValueHash)>);
		}
		{
			FAngelscriptCachedInitializerUnit Value;
			auto& [InitializerKind, InitializerKey, OwnerGlobal,
				VmInitializerCodecVersion, InitializerExecutionHash,
				CanonicalExecutionPayload] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(InitializerKind),
				decltype(InitializerKey), decltype(OwnerGlobal),
				decltype(VmInitializerCodecVersion), decltype(InitializerExecutionHash),
				decltype(CanonicalExecutionPayload)>,
				std::tuple<EAngelscriptCachedInitializerKind,
					FAngelscriptStableFunctionKey, TOptional<FAngelscriptStableGlobalKey>,
					uint32, FAngelscriptHash256, TArray<uint8>>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(InitializerKind),
				decltype(InitializerKey), decltype(OwnerGlobal),
				decltype(VmInitializerCodecVersion), decltype(InitializerExecutionHash),
				decltype(CanonicalExecutionPayload)>);
		}
		{
			FAngelscriptCachedInitializationAction Value;
			auto& [ActionOrdinal, ActionKind, Target, Dependencies] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(ActionOrdinal),
				decltype(ActionKind), decltype(Target), decltype(Dependencies)>,
				std::tuple<uint32, EAngelscriptCachedInitializationActionKind,
					FAngelscriptCacheStableReference,
					TArray<FAngelscriptCacheSemanticDependency>>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(ActionOrdinal),
				decltype(ActionKind), decltype(Target), decltype(Dependencies)>);
		}
		{
			FAngelscriptCachedPostInitFunction Value;
			auto& [PostInitOrdinal, Function] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<
				decltype(PostInitOrdinal), decltype(Function)>,
				std::tuple<uint32, FAngelscriptCacheStableReference>>);
			static_assert(AreRecursivelyOwnedMembers<
				decltype(PostInitOrdinal), decltype(Function)>);
		}
		{
			FAngelscriptCachedModuleState Value;
			auto& [PayloadSchemaVersion, ModuleKey, Profile, StateInputHash,
				OrderedGlobals, HardValues, Initializers, OrderedInitializationActions,
				OrderedPostInitFunctions, Dependencies] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(PayloadSchemaVersion),
				decltype(ModuleKey), decltype(Profile), decltype(StateInputHash),
				decltype(OrderedGlobals), decltype(HardValues), decltype(Initializers),
				decltype(OrderedInitializationActions),
				decltype(OrderedPostInitFunctions), decltype(Dependencies)>,
				std::tuple<uint32, FAngelscriptStableModuleKey,
					FAngelscriptArtifactProfileKey, FAngelscriptHash256,
					TArray<FAngelscriptCachedGlobalSchema>,
					TArray<FAngelscriptCachedHardValue>,
					TArray<FAngelscriptCachedInitializerUnit>,
					TArray<FAngelscriptCachedInitializationAction>,
					TArray<FAngelscriptCachedPostInitFunction>,
					TArray<FAngelscriptCacheSemanticDependency>>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(PayloadSchemaVersion),
				decltype(ModuleKey), decltype(Profile), decltype(StateInputHash),
				decltype(OrderedGlobals), decltype(HardValues), decltype(Initializers),
				decltype(OrderedInitializationActions),
				decltype(OrderedPostInitFunctions), decltype(Dependencies)>);
		}
		{
			FAngelscriptCachedFunctionBody Value;
			auto& [PayloadSchemaVersion, ModuleKey, Identity, ExpectedDeclarationAbi,
				FunctionSourceDigest, FunctionInputDigest, InvocationKind,
				VmExecutionCodecVersion, CanonicalExecutionPayload, ActualDependencies,
				DebugSidecar] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(PayloadSchemaVersion),
				decltype(ModuleKey), decltype(Identity), decltype(ExpectedDeclarationAbi),
				decltype(FunctionSourceDigest), decltype(FunctionInputDigest),
				decltype(InvocationKind), decltype(VmExecutionCodecVersion),
				decltype(CanonicalExecutionPayload), decltype(ActualDependencies),
				decltype(DebugSidecar)>,
				std::tuple<uint32, FAngelscriptStableModuleKey,
					FAngelscriptFunctionArtifactIdentity, FAngelscriptHash256,
					FAngelscriptFunctionSourceDigest, FAngelscriptFunctionInputDigest,
					EAngelscriptCachedFunctionInvocationKind, uint32, TArray<uint8>,
					TArray<FAngelscriptCacheSemanticDependency>,
					TOptional<FAngelscriptCacheRecordId>>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(PayloadSchemaVersion),
				decltype(ModuleKey), decltype(Identity), decltype(ExpectedDeclarationAbi),
				decltype(FunctionSourceDigest), decltype(FunctionInputDigest),
				decltype(InvocationKind), decltype(VmExecutionCodecVersion),
				decltype(CanonicalExecutionPayload), decltype(ActualDependencies),
				decltype(DebugSidecar)>);
		}
		{
			FAngelscriptCachedLogicalSectionKey Value;
			auto& [Hash] = Value;
			static_assert(std::is_same_v<
				TMemberTypeTuple<decltype(Hash)>, std::tuple<FAngelscriptHash256>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(Hash)>);
		}
		{
			FAngelscriptCachedDebugSourceReference Value;
			auto& [SourceFileKey, LogicalSectionKey, CanonicalLogicalSection] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(SourceFileKey),
				decltype(LogicalSectionKey), decltype(CanonicalLogicalSection)>,
				std::tuple<FAngelscriptCachedSourceFileKey,
					FAngelscriptCachedLogicalSectionKey, FString>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(SourceFileKey),
				decltype(LogicalSectionKey), decltype(CanonicalLogicalSection)>);
		}
		{
			FAngelscriptCachedDebugSidecar Value;
			auto& [PayloadSchemaVersion, FunctionKey, Profile, DebugHash,
				VmDebugCodecVersion, Sources, CanonicalDebugPayload] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(PayloadSchemaVersion),
				decltype(FunctionKey), decltype(Profile), decltype(DebugHash),
				decltype(VmDebugCodecVersion), decltype(Sources),
				decltype(CanonicalDebugPayload)>,
				std::tuple<uint32, FAngelscriptStableFunctionKey,
					FAngelscriptArtifactProfileKey, FAngelscriptHash256, uint32,
					TArray<FAngelscriptCachedDebugSourceReference>, TArray<uint8>>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(PayloadSchemaVersion),
				decltype(FunctionKey), decltype(Profile), decltype(DebugHash),
				decltype(VmDebugCodecVersion), decltype(Sources),
				decltype(CanonicalDebugPayload)>);
		}
		{
			FAngelscriptCachedModuleRecordLink Value;
			auto& [ModuleKey, RecordId] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<
				decltype(ModuleKey), decltype(RecordId)>,
				std::tuple<FAngelscriptStableModuleKey, FAngelscriptCacheRecordId>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(ModuleKey), decltype(RecordId)>);
		}
		{
			FAngelscriptCachedTypeSchemaLink Value;
			auto& [TypeKey, RecordId] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<
				decltype(TypeKey), decltype(RecordId)>,
				std::tuple<FAngelscriptStableTypeKey, FAngelscriptCacheRecordId>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(TypeKey), decltype(RecordId)>);
		}
		{
			FAngelscriptCachedFunctionBodyLink Value;
			auto& [FunctionKey, RecordId] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<
				decltype(FunctionKey), decltype(RecordId)>,
				std::tuple<FAngelscriptStableFunctionKey, FAngelscriptCacheRecordId>>);
			static_assert(AreRecursivelyOwnedMembers<
				decltype(FunctionKey), decltype(RecordId)>);
		}
		{
			FAngelscriptCachedModuleSnapshot Value;
			auto& [PayloadSchemaVersion, ModuleKey, ModuleInterface, TypeSchemas,
				ModuleState, FunctionBodies] = Value;
			static_assert(std::is_same_v<TMemberTypeTuple<decltype(PayloadSchemaVersion),
				decltype(ModuleKey), decltype(ModuleInterface), decltype(TypeSchemas),
				decltype(ModuleState), decltype(FunctionBodies)>,
				std::tuple<uint32, FAngelscriptStableModuleKey,
					FAngelscriptCachedModuleRecordLink,
					TArray<FAngelscriptCachedTypeSchemaLink>,
					FAngelscriptCachedModuleRecordLink,
					TArray<FAngelscriptCachedFunctionBodyLink>>>);
			static_assert(AreRecursivelyOwnedMembers<decltype(PayloadSchemaVersion),
				decltype(ModuleKey), decltype(ModuleInterface), decltype(TypeSchemas),
				decltype(ModuleState), decltype(FunctionBodies)>);
		}
	}

	TEST_METHOD(AggregatePositionSentinelsBindEveryDtoMemberName)
	{
		const FAngelscriptCachedGlobalSchema Global{
			0x101u,
			FAngelscriptStableGlobalKey{MakeSentinelHash(0x11)},
			FString(TEXT("sentinel.namespace.12")),
			FString(TEXT("sentinel-name-13")),
			MakeSentinelDataType(0x14, 0x151u, 1),
			0x161u,
			EAngelscriptCachedGlobalInitializationKind::PureConstant,
			EAngelscriptCachedGlobalCleanupPolicy::ReleaseHandle,
			MakeSentinelHash(0x17),
		};
		ASSERT_THAT(AreEqual(uint32(0x101), Global.StorageOrdinal));
		ASSERT_THAT(IsTrue(Global.GlobalKey.Hash == MakeSentinelHash(0x11)));
		ASSERT_THAT(IsTrue(Global.CanonicalNamespace == TEXT("sentinel.namespace.12")));
		ASSERT_THAT(IsTrue(Global.CanonicalName == TEXT("sentinel-name-13")));
		ASSERT_THAT(IsTrue(Global.Type.TypeReference->StableKey == MakeSentinelHash(0x14)));
		ASSERT_THAT(AreEqual(uint32(0x151), Global.Type.QualifierFlags));
		ASSERT_THAT(AreEqual(int32(1), Global.Type.OrderedSubTypes.Num()));
		ASSERT_THAT(AreEqual(uint32(0x161), Global.GlobalTraitFlags));
		ASSERT_THAT(IsTrue(Global.InitializationKind
			== EAngelscriptCachedGlobalInitializationKind::PureConstant));
		ASSERT_THAT(IsTrue(Global.CleanupPolicy
			== EAngelscriptCachedGlobalCleanupPolicy::ReleaseHandle));
		ASSERT_THAT(IsTrue(Global.StorageLayoutFingerprint == MakeSentinelHash(0x17)));

		const FAngelscriptCachedCanonicalValue CanonicalValue{
			EAngelscriptCachedCanonicalValueKind::UnsignedInteger,
			TArray<uint8>{0x21, 0x22, 0x23},
		};
		ASSERT_THAT(IsTrue(CanonicalValue.ValueKind
			== EAngelscriptCachedCanonicalValueKind::UnsignedInteger));
		ASSERT_THAT(AreEqual(int32(3), CanonicalValue.FixedWidthValueBytes.Num()));
		ASSERT_THAT(AreEqual(uint8(0x21), CanonicalValue.FixedWidthValueBytes[0]));

		const FAngelscriptCachedHardValue HardValue{
			EAngelscriptCachedHardValueKind::EnumAuthority,
			MakeSentinelReference(EAngelscriptCacheReferenceKind::ScriptGlobal, 0x31, 0x32),
			MakeSentinelDataType(0x33, 0x341u, 2),
			TOptional<FAngelscriptCachedCanonicalValue>(CanonicalValue),
			MakeSentinelHash(0x35),
		};
		ASSERT_THAT(IsTrue(HardValue.HardValueKind
			== EAngelscriptCachedHardValueKind::EnumAuthority));
		ASSERT_THAT(IsTrue(HardValue.Owner.StableKey == MakeSentinelHash(0x31)));
		ASSERT_THAT(IsTrue(HardValue.Owner.ExpectedAbi == MakeSentinelHash(0x32)));
		ASSERT_THAT(IsTrue(HardValue.Type.TypeReference->StableKey == MakeSentinelHash(0x33)));
		ASSERT_THAT(AreEqual(uint32(0x341), HardValue.Type.QualifierFlags));
		ASSERT_THAT(AreEqual(int32(2), HardValue.Type.OrderedSubTypes.Num()));
		ASSERT_THAT(IsTrue(HardValue.CanonicalValue.IsSet()));
		ASSERT_THAT(AreEqual(int32(3),
			HardValue.CanonicalValue->FixedWidthValueBytes.Num()));
		ASSERT_THAT(IsTrue(HardValue.HardValueHash == MakeSentinelHash(0x35)));

		const FAngelscriptCachedInitializerUnit Initializer{
			EAngelscriptCachedInitializerKind::Module,
			FAngelscriptStableFunctionKey{MakeSentinelHash(0x41)},
			TOptional<FAngelscriptStableGlobalKey>(
				FAngelscriptStableGlobalKey{MakeSentinelHash(0x42)}),
			0x431u,
			MakeSentinelHash(0x44),
			TArray<uint8>{0x45, 0x46},
		};
		ASSERT_THAT(IsTrue(Initializer.InitializerKind
			== EAngelscriptCachedInitializerKind::Module));
		ASSERT_THAT(IsTrue(Initializer.InitializerKey.Hash == MakeSentinelHash(0x41)));
		ASSERT_THAT(IsTrue(Initializer.OwnerGlobal->Hash == MakeSentinelHash(0x42)));
		ASSERT_THAT(AreEqual(uint32(0x431), Initializer.VmInitializerCodecVersion));
		ASSERT_THAT(IsTrue(Initializer.InitializerExecutionHash == MakeSentinelHash(0x44)));
		ASSERT_THAT(AreEqual(uint8(0x45), Initializer.CanonicalExecutionPayload[0]));

		const FAngelscriptCacheSemanticDependency ActionDependency{
			EAngelscriptCacheSemanticDependencyKind::Initializer,
			MakeSentinelReference(EAngelscriptCacheReferenceKind::ScriptFunction, 0x51, 0x52),
			TOptional<FAngelscriptHash256>(MakeSentinelHash(0x53)),
		};
		const FAngelscriptCachedInitializationAction Action{
			0x541u,
			EAngelscriptCachedInitializationActionKind::ExecuteInitializer,
			MakeSentinelReference(EAngelscriptCacheReferenceKind::ScriptGlobal, 0x55, 0x56),
			TArray<FAngelscriptCacheSemanticDependency>{ActionDependency},
		};
		ASSERT_THAT(AreEqual(uint32(0x541), Action.ActionOrdinal));
		ASSERT_THAT(IsTrue(Action.ActionKind
			== EAngelscriptCachedInitializationActionKind::ExecuteInitializer));
		ASSERT_THAT(IsTrue(Action.Target.StableKey == MakeSentinelHash(0x55)));
		ASSERT_THAT(IsTrue(Action.Target.ExpectedAbi == MakeSentinelHash(0x56)));
		ASSERT_THAT(AreEqual(int32(1), Action.Dependencies.Num()));
		ASSERT_THAT(IsTrue(Action.Dependencies[0].ExpectedContentOrValue.GetValue()
			== MakeSentinelHash(0x53)));

		const FAngelscriptCachedPostInitFunction PostInit{
			0x611u,
			MakeSentinelReference(EAngelscriptCacheReferenceKind::ScriptFunction, 0x62, 0x63),
		};
		ASSERT_THAT(AreEqual(uint32(0x611), PostInit.PostInitOrdinal));
		ASSERT_THAT(IsTrue(PostInit.Function.StableKey == MakeSentinelHash(0x62)));
		ASSERT_THAT(IsTrue(PostInit.Function.ExpectedAbi == MakeSentinelHash(0x63)));

		const FAngelscriptCachedModuleState ModuleState{
			0x701u,
			FAngelscriptStableModuleKey{MakeSentinelHash(0x72)},
			FAngelscriptArtifactProfileKey{MakeSentinelHash(0x73)},
			MakeSentinelHash(0x74),
			MakeSentinelArray<FAngelscriptCachedGlobalSchema>(1),
			MakeSentinelArray<FAngelscriptCachedHardValue>(2),
			MakeSentinelArray<FAngelscriptCachedInitializerUnit>(3),
			MakeSentinelArray<FAngelscriptCachedInitializationAction>(4),
			MakeSentinelArray<FAngelscriptCachedPostInitFunction>(5),
			MakeSentinelArray<FAngelscriptCacheSemanticDependency>(6),
		};
		ASSERT_THAT(AreEqual(uint32(0x701), ModuleState.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(ModuleState.ModuleKey.Hash == MakeSentinelHash(0x72)));
		ASSERT_THAT(IsTrue(ModuleState.Profile.Hash == MakeSentinelHash(0x73)));
		ASSERT_THAT(IsTrue(ModuleState.StateInputHash == MakeSentinelHash(0x74)));
		ASSERT_THAT(AreEqual(int32(1), ModuleState.OrderedGlobals.Num()));
		ASSERT_THAT(AreEqual(int32(2), ModuleState.HardValues.Num()));
		ASSERT_THAT(AreEqual(int32(3), ModuleState.Initializers.Num()));
		ASSERT_THAT(AreEqual(int32(4), ModuleState.OrderedInitializationActions.Num()));
		ASSERT_THAT(AreEqual(int32(5), ModuleState.OrderedPostInitFunctions.Num()));
		ASSERT_THAT(AreEqual(int32(6), ModuleState.Dependencies.Num()));

		const FAngelscriptFunctionArtifactIdentity FunctionIdentity{
			FAngelscriptStableFunctionKey{MakeSentinelHash(0x81)},
			FAngelscriptFunctionContentHash{
				MakeSentinelHash(0x82), MakeSentinelHash(0x83)},
			FAngelscriptArtifactProfileKey{MakeSentinelHash(0x84)},
		};
		const FAngelscriptCacheRecordId DebugRecordId{
			EAngelscriptCacheRecordKind::DebugSidecar, MakeSentinelHash(0x8e)};
		const FAngelscriptCachedFunctionBody FunctionBody{
			0x851u,
			FAngelscriptStableModuleKey{MakeSentinelHash(0x86)},
			FunctionIdentity,
			MakeSentinelHash(0x87),
			FAngelscriptFunctionSourceDigest{MakeSentinelHash(0x88)},
			FAngelscriptFunctionInputDigest{MakeSentinelHash(0x89)},
			EAngelscriptCachedFunctionInvocationKind::Lambda,
			0x8a1u,
			TArray<uint8>{0x8b, 0x8c},
			MakeSentinelArray<FAngelscriptCacheSemanticDependency>(3),
			TOptional<FAngelscriptCacheRecordId>(DebugRecordId),
		};
		ASSERT_THAT(AreEqual(uint32(0x851), FunctionBody.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(FunctionBody.ModuleKey.Hash == MakeSentinelHash(0x86)));
		ASSERT_THAT(IsTrue(FunctionBody.Identity.FunctionKey.Hash == MakeSentinelHash(0x81)));
		ASSERT_THAT(IsTrue(FunctionBody.Identity.Content.Execution == MakeSentinelHash(0x82)));
		ASSERT_THAT(IsTrue(FunctionBody.Identity.Content.Debug == MakeSentinelHash(0x83)));
		ASSERT_THAT(IsTrue(FunctionBody.Identity.Profile.Hash == MakeSentinelHash(0x84)));
		ASSERT_THAT(IsTrue(FunctionBody.ExpectedDeclarationAbi == MakeSentinelHash(0x87)));
		ASSERT_THAT(IsTrue(FunctionBody.FunctionSourceDigest.Hash == MakeSentinelHash(0x88)));
		ASSERT_THAT(IsTrue(FunctionBody.FunctionInputDigest.Hash == MakeSentinelHash(0x89)));
		ASSERT_THAT(IsTrue(FunctionBody.InvocationKind
			== EAngelscriptCachedFunctionInvocationKind::Lambda));
		ASSERT_THAT(AreEqual(uint32(0x8a1), FunctionBody.VmExecutionCodecVersion));
		ASSERT_THAT(AreEqual(uint8(0x8b), FunctionBody.CanonicalExecutionPayload[0]));
		ASSERT_THAT(AreEqual(int32(3), FunctionBody.ActualDependencies.Num()));
		ASSERT_THAT(IsTrue(FunctionBody.DebugSidecar->ContentHash
			== MakeSentinelHash(0x8e)));

		const FAngelscriptCachedLogicalSectionKey LogicalSectionKey{
			MakeSentinelHash(0x91)};
		ASSERT_THAT(IsTrue(LogicalSectionKey.Hash == MakeSentinelHash(0x91)));

		const FAngelscriptCachedDebugSourceReference DebugSource{
			FAngelscriptCachedSourceFileKey{MakeSentinelHash(0x92)},
			FAngelscriptCachedLogicalSectionKey{MakeSentinelHash(0x93)},
			FString(TEXT("sentinel.logical.section.94")),
		};
		ASSERT_THAT(IsTrue(DebugSource.SourceFileKey.Hash == MakeSentinelHash(0x92)));
		ASSERT_THAT(IsTrue(DebugSource.LogicalSectionKey.Hash == MakeSentinelHash(0x93)));
		ASSERT_THAT(IsTrue(DebugSource.CanonicalLogicalSection
			== TEXT("sentinel.logical.section.94")));

		const FAngelscriptCachedDebugSidecar DebugSidecar{
			0xa01u,
			FAngelscriptStableFunctionKey{MakeSentinelHash(0xa2)},
			FAngelscriptArtifactProfileKey{MakeSentinelHash(0xa3)},
			MakeSentinelHash(0xa4),
			0xa51u,
			MakeSentinelArray<FAngelscriptCachedDebugSourceReference>(2),
			TArray<uint8>{0xa6, 0xa7, 0xa8},
		};
		ASSERT_THAT(AreEqual(uint32(0xa01), DebugSidecar.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(DebugSidecar.FunctionKey.Hash == MakeSentinelHash(0xa2)));
		ASSERT_THAT(IsTrue(DebugSidecar.Profile.Hash == MakeSentinelHash(0xa3)));
		ASSERT_THAT(IsTrue(DebugSidecar.DebugHash == MakeSentinelHash(0xa4)));
		ASSERT_THAT(AreEqual(uint32(0xa51), DebugSidecar.VmDebugCodecVersion));
		ASSERT_THAT(AreEqual(int32(2), DebugSidecar.Sources.Num()));
		ASSERT_THAT(AreEqual(int32(3), DebugSidecar.CanonicalDebugPayload.Num()));

		const FAngelscriptCachedModuleRecordLink ModuleInterfaceLink{
			FAngelscriptStableModuleKey{MakeSentinelHash(0xb1)},
			FAngelscriptCacheRecordId{
				EAngelscriptCacheRecordKind::ModuleInterface, MakeSentinelHash(0xb2)},
		};
		ASSERT_THAT(IsTrue(ModuleInterfaceLink.ModuleKey.Hash == MakeSentinelHash(0xb1)));
		ASSERT_THAT(IsTrue(ModuleInterfaceLink.RecordId.ContentHash == MakeSentinelHash(0xb2)));

		const FAngelscriptCachedTypeSchemaLink TypeSchemaLink{
			FAngelscriptStableTypeKey{MakeSentinelHash(0xb3)},
			FAngelscriptCacheRecordId{
				EAngelscriptCacheRecordKind::TypeSchema, MakeSentinelHash(0xb4)},
		};
		ASSERT_THAT(IsTrue(TypeSchemaLink.TypeKey.Hash == MakeSentinelHash(0xb3)));
		ASSERT_THAT(IsTrue(TypeSchemaLink.RecordId.ContentHash == MakeSentinelHash(0xb4)));

		const FAngelscriptCachedFunctionBodyLink FunctionBodyLink{
			FAngelscriptStableFunctionKey{MakeSentinelHash(0xb5)},
			FAngelscriptCacheRecordId{
				EAngelscriptCacheRecordKind::FunctionBody, MakeSentinelHash(0xb6)},
		};
		ASSERT_THAT(IsTrue(FunctionBodyLink.FunctionKey.Hash == MakeSentinelHash(0xb5)));
		ASSERT_THAT(IsTrue(FunctionBodyLink.RecordId.ContentHash == MakeSentinelHash(0xb6)));

		const FAngelscriptCachedModuleRecordLink ModuleStateLink{
			FAngelscriptStableModuleKey{MakeSentinelHash(0xb7)},
			FAngelscriptCacheRecordId{
				EAngelscriptCacheRecordKind::ModuleState, MakeSentinelHash(0xb8)},
		};
		const FAngelscriptCachedModuleSnapshot Snapshot{
			0xc01u,
			FAngelscriptStableModuleKey{MakeSentinelHash(0xc2)},
			ModuleInterfaceLink,
			MakeSentinelArray<FAngelscriptCachedTypeSchemaLink>(2),
			ModuleStateLink,
			MakeSentinelArray<FAngelscriptCachedFunctionBodyLink>(3),
		};
		ASSERT_THAT(AreEqual(uint32(0xc01), Snapshot.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(Snapshot.ModuleKey.Hash == MakeSentinelHash(0xc2)));
		ASSERT_THAT(IsTrue(Snapshot.ModuleInterface.ModuleKey.Hash
			== MakeSentinelHash(0xb1)));
		ASSERT_THAT(IsTrue(Snapshot.ModuleInterface.RecordId.ContentHash
			== MakeSentinelHash(0xb2)));
		ASSERT_THAT(AreEqual(int32(2), Snapshot.TypeSchemas.Num()));
		ASSERT_THAT(IsTrue(Snapshot.ModuleState.ModuleKey.Hash
			== MakeSentinelHash(0xb7)));
		ASSERT_THAT(IsTrue(Snapshot.ModuleState.RecordId.ContentHash
			== MakeSentinelHash(0xb8)));
		ASSERT_THAT(AreEqual(int32(3), Snapshot.FunctionBodies.Num()));
	}

	TEST_METHOD(PstRulesCoverEveryPublishedField)
	{
		static_assert(RulesCoverEveryFieldExactlyOnce(ModuleStateRules, 88));
		static_assert(RulesCoverEveryFieldExactlyOnce(FunctionBodyRules, 27));
		static_assert(RulesCoverEveryFieldExactlyOnce(DebugSidecarRules, 11));
		static_assert(RulesCoverEveryFieldExactlyOnce(ModuleSnapshotRules, 24));

		const FDataTypeRootCount Roots[] = {
			{{EDataTypeRootOwner::Global, 1}, 3},
		};
		FLookupAuthority Authority;
		Authority.PrimaryCount = 3;
		Authority.SecondaryCount = 3;
		Authority.DataTypeRoots = Roots;
		Authority.DataTypeRootCount = 1;

		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::PayloadSchemaVersion}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::Global, 1}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::GlobalTypeKind, 1, 2}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::InitializationActionDependencyTarget,
			1, 2}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptFunctionBodyCapturedField::ActualDependencyTargetStableKey, 1},
			Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptDebugSidecarCapturedField::SourceLogicalSectionKey, 1},
			Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordIdKind, 1},
			Authority)));
	}

	TEST_METHOD(DeclarationFirstFailureMatrixCoversAllFourCoordinates)
	{
		const FDataTypeRootCount Roots[] = {
			{{EDataTypeRootOwner::Global, 0}, 2},
		};
		FLookupAuthority Authority;
		Authority.PrimaryCount = 2;
		Authority.SecondaryCount = 2;
		Authority.TertiaryCount = 2;
		Authority.DataTypeRoots = Roots;
		Authority.DataTypeRootCount = 1;

		ASSERT_THAT(IsTrue(DeclarationFirstMatrixIsExact<
			FAngelscriptModuleStateFieldCoordinate>(
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheRecordKind::FunctionBody,
			EAngelscriptModuleStateCapturedField::PayloadSchemaVersion,
			EAngelscriptModuleStateCapturedField::Global,
			EAngelscriptModuleStateCapturedField::GlobalTypeNode,
			true, 88, 2, Authority)));
		ASSERT_THAT(IsTrue(DeclarationFirstMatrixIsExact<
			FAngelscriptFunctionBodyFieldCoordinate>(
			EAngelscriptCacheRecordKind::FunctionBody,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptFunctionBodyCapturedField::PayloadSchemaVersion,
			EAngelscriptFunctionBodyCapturedField::ActualDependency,
			EAngelscriptFunctionBodyCapturedField::ActualDependency,
			false, 27, 0, Authority)));
		ASSERT_THAT(IsTrue(DeclarationFirstMatrixIsExact<
			FAngelscriptDebugSidecarFieldCoordinate>(
			EAngelscriptCacheRecordKind::DebugSidecar,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptDebugSidecarCapturedField::PayloadSchemaVersion,
			EAngelscriptDebugSidecarCapturedField::Source,
			EAngelscriptDebugSidecarCapturedField::Source,
			false, 11, 0, Authority)));
		ASSERT_THAT(IsTrue(DeclarationFirstMatrixIsExact<
			FAngelscriptModuleSnapshotFieldCoordinate>(
			EAngelscriptCacheRecordKind::ModuleSnapshot,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptModuleSnapshotCapturedField::PayloadSchemaVersion,
			EAngelscriptModuleSnapshotCapturedField::TypeSchemaLink,
			EAngelscriptModuleSnapshotCapturedField::TypeSchemaLink,
			false, 24, 0, Authority)));
	}

	TEST_METHOD(FieldSpecificOptionalRulesAreExact)
	{
		const FDataTypeRootCount Roots[] = {
			{{EDataTypeRootOwner::Global, 0}, 1},
			{{EDataTypeRootOwner::HardValue, 0}, 1},
		};
		FLookupAuthority Absent;
		Absent.PrimaryCount = 1;
		Absent.SecondaryCount = 1;
		Absent.DataTypeRoots = Roots;
		Absent.DataTypeRootCount = 2;
		const FOptionalOccurrenceIdentity PresentOccurrences[] = {
			{EOptionalOccurrenceFamily::GlobalTypeReference, 0, 0},
			{EOptionalOccurrenceFamily::HardValueTypeReference, 0, 0},
			{EOptionalOccurrenceFamily::HardValueCanonicalValue, 0, U},
			{EOptionalOccurrenceFamily::InitializerOwnerGlobal, 0, U},
			{EOptionalOccurrenceFamily::InitializationActionDependencyExpectedValue, 0, 0},
			{EOptionalOccurrenceFamily::ModuleDependencyExpectedValue, 0, U},
			{EOptionalOccurrenceFamily::FunctionDependencyExpectedValue, 0, U},
			{EOptionalOccurrenceFamily::DebugSidecarRecordId, U, U},
		};
		FLookupAuthority AllPresent = Absent;
		AllPresent.OptionalRules.PresentOccurrences = PresentOccurrences;
		AllPresent.OptionalRules.PresentOccurrenceCount = 8;
		const FLookupAuthority GlobalReferencePresent =
			WithOnlyPresentOccurrence(Absent, PresentOccurrences[0]);
		const FLookupAuthority HardValueReferencePresent =
			WithOnlyPresentOccurrence(Absent, PresentOccurrences[1]);
		const FLookupAuthority CanonicalValuePresent =
			WithOnlyPresentOccurrence(Absent, PresentOccurrences[2]);
		const FLookupAuthority OwnerGlobalPresent =
			WithOnlyPresentOccurrence(Absent, PresentOccurrences[3]);
		const FLookupAuthority ActionDependencyValuePresent =
			WithOnlyPresentOccurrence(Absent, PresentOccurrences[4]);
		const FLookupAuthority ModuleDependencyValuePresent =
			WithOnlyPresentOccurrence(Absent, PresentOccurrences[5]);
		const FLookupAuthority FunctionDependencyValuePresent =
			WithOnlyPresentOccurrence(Absent, PresentOccurrences[6]);
		const FLookupAuthority DebugSidecarPresent =
			WithOnlyPresentOccurrence(Absent, PresentOccurrences[7]);

		const EAngelscriptModuleStateCapturedField ModulePresenceFields[] = {
			EAngelscriptModuleStateCapturedField::GlobalTypeReferencePresence,
			EAngelscriptModuleStateCapturedField::HardValueTypeReferencePresence,
			EAngelscriptModuleStateCapturedField::HardValueCanonicalValuePresence,
			EAngelscriptModuleStateCapturedField::InitializerOwnerGlobalPresence,
			EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValuePresence,
			EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValuePresence,
		};
		for (const EAngelscriptModuleStateCapturedField Field : ModulePresenceFields)
		{
			const bool bUsesSecondary = Field
				== EAngelscriptModuleStateCapturedField::GlobalTypeReferencePresence
				|| Field == EAngelscriptModuleStateCapturedField::HardValueTypeReferencePresence
				|| Field == EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValuePresence;
			ASSERT_THAT(IsTrue(IsCoordinateAccepted(
				{Field, 0, bUsesSecondary ? 0u : U}, Absent)));
			ASSERT_THAT(IsTrue(IsCoordinateAccepted(
				{Field, 0, bUsesSecondary ? 0u : U}, AllPresent)));
		}

		const EAngelscriptModuleStateCapturedField GlobalTypeReferenceFields[] = {
			EAngelscriptModuleStateCapturedField::GlobalTypeReference,
			EAngelscriptModuleStateCapturedField::GlobalTypeReferenceKind,
			EAngelscriptModuleStateCapturedField::GlobalTypeReferenceStableKey,
			EAngelscriptModuleStateCapturedField::GlobalTypeReferenceExpectedAbi,
		};
		for (const EAngelscriptModuleStateCapturedField Field : GlobalTypeReferenceFields)
		{
			ASSERT_THAT(IsFalse(IsCoordinateAccepted({Field, 0, 0}, Absent)));
			ASSERT_THAT(IsTrue(IsCoordinateAccepted(
				{Field, 0, 0}, GlobalReferencePresent)));
		}

		const EAngelscriptModuleStateCapturedField HardValueTypeReferenceFields[] = {
			EAngelscriptModuleStateCapturedField::HardValueTypeReference,
			EAngelscriptModuleStateCapturedField::HardValueTypeReferenceKind,
			EAngelscriptModuleStateCapturedField::HardValueTypeReferenceStableKey,
			EAngelscriptModuleStateCapturedField::HardValueTypeReferenceExpectedAbi,
		};
		for (const EAngelscriptModuleStateCapturedField Field : HardValueTypeReferenceFields)
		{
			ASSERT_THAT(IsFalse(IsCoordinateAccepted({Field, 0, 0}, Absent)));
			ASSERT_THAT(IsTrue(IsCoordinateAccepted(
				{Field, 0, 0}, HardValueReferencePresent)));
		}

		const EAngelscriptModuleStateCapturedField CanonicalValueFields[] = {
			EAngelscriptModuleStateCapturedField::HardValueCanonicalValue,
			EAngelscriptModuleStateCapturedField::HardValueCanonicalValueKind,
			EAngelscriptModuleStateCapturedField::HardValueCanonicalValueFixedWidthValueBytes,
		};
		for (const EAngelscriptModuleStateCapturedField Field : CanonicalValueFields)
		{
			ASSERT_THAT(IsFalse(IsCoordinateAccepted({Field, 0}, Absent)));
			ASSERT_THAT(IsTrue(IsCoordinateAccepted({Field, 0}, CanonicalValuePresent)));
		}

		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::InitializerOwnerGlobal, 0}, Absent)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::InitializerOwnerGlobal, 0},
			OwnerGlobalPresent)));
		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValue,
			0, 0}, Absent)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValue,
			0, 0}, ActionDependencyValuePresent)));
		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue, 0},
			Absent)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue, 0},
			ModuleDependencyValuePresent)));

		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValuePresence,
			0}, Absent)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValuePresence,
			0}, FunctionDependencyValuePresent)));
		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValue,
			0}, Absent)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValue,
			0}, FunctionDependencyValuePresent)));

		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptFunctionBodyCapturedField::DebugSidecarPresence}, Absent)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptFunctionBodyCapturedField::DebugSidecarPresence},
			DebugSidecarPresent)));
		const EAngelscriptFunctionBodyCapturedField DebugSidecarRecordIdFields[] = {
			EAngelscriptFunctionBodyCapturedField::DebugSidecar,
			EAngelscriptFunctionBodyCapturedField::DebugSidecarKind,
			EAngelscriptFunctionBodyCapturedField::DebugSidecarContentHash,
		};
		for (const EAngelscriptFunctionBodyCapturedField Field : DebugSidecarRecordIdFields)
		{
			ASSERT_THAT(IsFalse(IsCoordinateAccepted({Field}, Absent)));
			ASSERT_THAT(IsTrue(IsCoordinateAccepted({Field}, DebugSidecarPresent)));
		}
	}

	TEST_METHOD(ExactOptionalOccurrenceIdentityAndOneHotRulesAreExact)
	{
		const FDataTypeRootCount Roots[] = {
			{{EDataTypeRootOwner::Global, 0}, 2},
			{{EDataTypeRootOwner::Global, 1}, 3},
			{{EDataTypeRootOwner::HardValue, 0}, 2},
			{{EDataTypeRootOwner::HardValue, 1}, 1},
		};
		FLookupAuthority Base;
		Base.PrimaryCount = 2;
		Base.SecondaryCount = 2;
		Base.DataTypeRoots = Roots;
		Base.DataTypeRootCount = 4;

		const FOptionalOccurrenceIdentity GlobalA = {
			EOptionalOccurrenceFamily::GlobalTypeReference, 0, 0};
		const FOptionalOccurrenceIdentity GlobalB = {
			EOptionalOccurrenceFamily::GlobalTypeReference, 0, 1};
		const FOptionalOccurrenceIdentity GlobalC = {
			EOptionalOccurrenceFamily::GlobalTypeReference, 1, 0};
		const FOptionalOccurrenceIdentity HardValueA = {
			EOptionalOccurrenceFamily::HardValueTypeReference, 0, 0};
		const FOptionalOccurrenceIdentity HardValueB = {
			EOptionalOccurrenceFamily::HardValueTypeReference, 0, 1};
		const FOptionalOccurrenceIdentity HardValueC = {
			EOptionalOccurrenceFamily::HardValueTypeReference, 1, 0};
		const FOptionalOccurrenceIdentity CanonicalA = {
			EOptionalOccurrenceFamily::HardValueCanonicalValue, 0, U};
		const FOptionalOccurrenceIdentity CanonicalB = {
			EOptionalOccurrenceFamily::HardValueCanonicalValue, 1, U};
		const FOptionalOccurrenceIdentity OwnerGlobalA = {
			EOptionalOccurrenceFamily::InitializerOwnerGlobal, 0, U};
		const FOptionalOccurrenceIdentity OwnerGlobalB = {
			EOptionalOccurrenceFamily::InitializerOwnerGlobal, 1, U};
		const FOptionalOccurrenceIdentity ActionDependencyA = {
			EOptionalOccurrenceFamily::InitializationActionDependencyExpectedValue, 0, 0};
		const FOptionalOccurrenceIdentity ActionDependencyB = {
			EOptionalOccurrenceFamily::InitializationActionDependencyExpectedValue, 0, 1};
		const FOptionalOccurrenceIdentity ActionDependencyC = {
			EOptionalOccurrenceFamily::InitializationActionDependencyExpectedValue, 1, 0};
		const FOptionalOccurrenceIdentity ModuleDependencyA = {
			EOptionalOccurrenceFamily::ModuleDependencyExpectedValue, 0, U};
		const FOptionalOccurrenceIdentity ModuleDependencyB = {
			EOptionalOccurrenceFamily::ModuleDependencyExpectedValue, 1, U};
		const FOptionalOccurrenceIdentity FunctionDependencyA = {
			EOptionalOccurrenceFamily::FunctionDependencyExpectedValue, 0, U};
		const FOptionalOccurrenceIdentity FunctionDependencyB = {
			EOptionalOccurrenceFamily::FunctionDependencyExpectedValue, 1, U};
		const FOptionalOccurrenceIdentity DebugSidecar = {
			EOptionalOccurrenceFamily::DebugSidecarRecordId, U, U};

		const FOptionalOccurrenceIdentity Representatives[] = {
			GlobalA,
			GlobalB,
			GlobalC,
			HardValueA,
			HardValueB,
			HardValueC,
			CanonicalA,
			CanonicalB,
			OwnerGlobalA,
			OwnerGlobalB,
			ActionDependencyA,
			ActionDependencyB,
			ActionDependencyC,
			ModuleDependencyA,
			ModuleDependencyB,
			FunctionDependencyA,
			FunctionDependencyB,
			DebugSidecar,
		};

		const FAngelscriptModuleStateFieldCoordinate GlobalFieldsA[] = {
			{EAngelscriptModuleStateCapturedField::GlobalTypeReference, 0, 0},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceKind, 0, 0},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceStableKey, 0, 0},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceExpectedAbi, 0, 0},
		};
		const FAngelscriptModuleStateFieldCoordinate GlobalFieldsB[] = {
			{EAngelscriptModuleStateCapturedField::GlobalTypeReference, 0, 1},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceKind, 0, 1},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceStableKey, 0, 1},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceExpectedAbi, 0, 1},
		};
		const FAngelscriptModuleStateFieldCoordinate GlobalFieldsC[] = {
			{EAngelscriptModuleStateCapturedField::GlobalTypeReference, 1, 0},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceKind, 1, 0},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceStableKey, 1, 0},
			{EAngelscriptModuleStateCapturedField::GlobalTypeReferenceExpectedAbi, 1, 0},
		};
		ASSERT_THAT(IsTrue(TwoAxisOptionalOccurrenceMatrixIsExact(
			Base, GlobalA, GlobalB, GlobalC,
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::GlobalTypeReferencePresence, 0, 0},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::GlobalTypeReferencePresence, 0, 1},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::GlobalTypeReferencePresence, 1, 0},
			GlobalFieldsA, GlobalFieldsB, GlobalFieldsC, Representatives)));

		const FAngelscriptModuleStateFieldCoordinate HardValueFieldsA[] = {
			{EAngelscriptModuleStateCapturedField::HardValueTypeReference, 0, 0},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceKind, 0, 0},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceStableKey, 0, 0},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceExpectedAbi, 0, 0},
		};
		const FAngelscriptModuleStateFieldCoordinate HardValueFieldsB[] = {
			{EAngelscriptModuleStateCapturedField::HardValueTypeReference, 0, 1},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceKind, 0, 1},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceStableKey, 0, 1},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceExpectedAbi, 0, 1},
		};
		const FAngelscriptModuleStateFieldCoordinate HardValueFieldsC[] = {
			{EAngelscriptModuleStateCapturedField::HardValueTypeReference, 1, 0},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceKind, 1, 0},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceStableKey, 1, 0},
			{EAngelscriptModuleStateCapturedField::HardValueTypeReferenceExpectedAbi, 1, 0},
		};
		ASSERT_THAT(IsTrue(TwoAxisOptionalOccurrenceMatrixIsExact(
			Base, HardValueA, HardValueB, HardValueC,
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::HardValueTypeReferencePresence, 0, 0},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::HardValueTypeReferencePresence, 0, 1},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::HardValueTypeReferencePresence, 1, 0},
			HardValueFieldsA, HardValueFieldsB, HardValueFieldsC, Representatives)));

		const FAngelscriptModuleStateFieldCoordinate CanonicalFieldsA[] = {
			{EAngelscriptModuleStateCapturedField::HardValueCanonicalValue, 0},
			{EAngelscriptModuleStateCapturedField::HardValueCanonicalValueKind, 0},
			{EAngelscriptModuleStateCapturedField::HardValueCanonicalValueFixedWidthValueBytes, 0},
		};
		const FAngelscriptModuleStateFieldCoordinate CanonicalFieldsB[] = {
			{EAngelscriptModuleStateCapturedField::HardValueCanonicalValue, 1},
			{EAngelscriptModuleStateCapturedField::HardValueCanonicalValueKind, 1},
			{EAngelscriptModuleStateCapturedField::HardValueCanonicalValueFixedWidthValueBytes, 1},
		};
		ASSERT_THAT(IsTrue(RepeatedOptionalOccurrenceMatrixIsExact(
			Base, CanonicalA, CanonicalB,
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::HardValueCanonicalValuePresence, 0},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::HardValueCanonicalValuePresence, 1},
			CanonicalFieldsA, CanonicalFieldsB, Representatives)));

		const FAngelscriptModuleStateFieldCoordinate OwnerGlobalFieldsA[] = {
			{EAngelscriptModuleStateCapturedField::InitializerOwnerGlobal, 0},
		};
		const FAngelscriptModuleStateFieldCoordinate OwnerGlobalFieldsB[] = {
			{EAngelscriptModuleStateCapturedField::InitializerOwnerGlobal, 1},
		};
		ASSERT_THAT(IsTrue(RepeatedOptionalOccurrenceMatrixIsExact(
			Base, OwnerGlobalA, OwnerGlobalB,
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::InitializerOwnerGlobalPresence, 0},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::InitializerOwnerGlobalPresence, 1},
			OwnerGlobalFieldsA, OwnerGlobalFieldsB, Representatives)));

		const FAngelscriptModuleStateFieldCoordinate ActionDependencyFieldsA[] = {
			{EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValue,
				0, 0},
		};
		const FAngelscriptModuleStateFieldCoordinate ActionDependencyFieldsB[] = {
			{EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValue,
				0, 1},
		};
		const FAngelscriptModuleStateFieldCoordinate ActionDependencyFieldsC[] = {
			{EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValue,
				1, 0},
		};
		ASSERT_THAT(IsTrue(TwoAxisOptionalOccurrenceMatrixIsExact(
			Base, ActionDependencyA, ActionDependencyB, ActionDependencyC,
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValuePresence,
				0, 0},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValuePresence,
				0, 1},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::InitializationActionDependencyExpectedContentOrValuePresence,
				1, 0},
			ActionDependencyFieldsA, ActionDependencyFieldsB,
			ActionDependencyFieldsC, Representatives)));

		const FAngelscriptModuleStateFieldCoordinate ModuleDependencyFieldsA[] = {
			{EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue, 0},
		};
		const FAngelscriptModuleStateFieldCoordinate ModuleDependencyFieldsB[] = {
			{EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValue, 1},
		};
		ASSERT_THAT(IsTrue(RepeatedOptionalOccurrenceMatrixIsExact(
			Base, ModuleDependencyA, ModuleDependencyB,
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValuePresence, 0},
			FAngelscriptModuleStateFieldCoordinate{
				EAngelscriptModuleStateCapturedField::DependencyExpectedContentOrValuePresence, 1},
			ModuleDependencyFieldsA, ModuleDependencyFieldsB, Representatives)));

		const FAngelscriptFunctionBodyFieldCoordinate FunctionDependencyFieldsA[] = {
			{EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValue, 0},
		};
		const FAngelscriptFunctionBodyFieldCoordinate FunctionDependencyFieldsB[] = {
			{EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValue, 1},
		};
		ASSERT_THAT(IsTrue(RepeatedOptionalOccurrenceMatrixIsExact(
			Base, FunctionDependencyA, FunctionDependencyB,
			FAngelscriptFunctionBodyFieldCoordinate{
				EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValuePresence,
				0},
			FAngelscriptFunctionBodyFieldCoordinate{
				EAngelscriptFunctionBodyCapturedField::ActualDependencyExpectedContentOrValuePresence,
				1},
			FunctionDependencyFieldsA, FunctionDependencyFieldsB, Representatives)));

		const FAngelscriptFunctionBodyFieldCoordinate FunctionDebugSidecarFields[] = {
			{EAngelscriptFunctionBodyCapturedField::DebugSidecar},
			{EAngelscriptFunctionBodyCapturedField::DebugSidecarKind},
			{EAngelscriptFunctionBodyCapturedField::DebugSidecarContentHash},
		};
		ASSERT_THAT(IsTrue(SingletonOptionalOccurrenceMatrixIsExact(
			Base, DebugSidecar,
			FAngelscriptFunctionBodyFieldCoordinate{
				EAngelscriptFunctionBodyCapturedField::DebugSidecarPresence},
			FunctionDebugSidecarFields, Representatives)));
	}

	TEST_METHOD(DataTypePreorderRestartsAndIsIsolatedPerRoot)
	{
		const FDataTypeRootCount Roots[] = {
			{{EDataTypeRootOwner::Global, 0}, 3},
			{{EDataTypeRootOwner::Global, 1}, 1},
			{{EDataTypeRootOwner::HardValue, 0}, 2},
			{{EDataTypeRootOwner::HardValue, 1}, 4},
		};
		FLookupAuthority Authority;
		Authority.PrimaryCount = 3;
		Authority.DataTypeRoots = Roots;
		Authority.DataTypeRootCount = 4;

		// Every row is an independent root: preorder ordinal zero is valid four times.
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::GlobalTypeNode, 0, 0}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::GlobalTypeNode, 1, 0}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::HardValueTypeNode, 0, 0}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::HardValueTypeNode, 1, 0}, Authority)));

		// Distinct roots retain their own bounds.
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::GlobalTypeNode, 0, 2}, Authority)));
		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::GlobalTypeNode, 0, 3}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::GlobalTypeNode, 1, 0}, Authority)));
		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::GlobalTypeNode, 1, 1}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::HardValueTypeNode, 0, 1}, Authority)));
		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::HardValueTypeNode, 0, 2}, Authority)));
		ASSERT_THAT(IsTrue(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::HardValueTypeNode, 1, 3}, Authority)));
		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::HardValueTypeNode, 1, 4}, Authority)));

		// A missing exact root identity is not allowed to borrow any other root's count.
		ASSERT_THAT(IsFalse(IsCoordinateAccepted({
			EAngelscriptModuleStateCapturedField::GlobalTypeNode, 2, 0}, Authority)));
	}

	TEST_METHOD(RemainingDtoMemberApiAndDigestNamesAreExact)
	{
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().StorageOrdinal), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().GlobalKey), FAngelscriptStableGlobalKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().CanonicalNamespace), FString>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().CanonicalName), FString>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().Type), FAngelscriptCachedDataType>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().GlobalTraitFlags), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().InitializationKind),
			EAngelscriptCachedGlobalInitializationKind>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().CleanupPolicy),
			EAngelscriptCachedGlobalCleanupPolicy>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedGlobalSchema&>().StorageLayoutFingerprint),
			FAngelscriptHash256>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedCanonicalValue&>().ValueKind),
			EAngelscriptCachedCanonicalValueKind>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedCanonicalValue&>().FixedWidthValueBytes), TArray<uint8>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedHardValue&>().HardValueKind), EAngelscriptCachedHardValueKind>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedHardValue&>().Owner), FAngelscriptCacheStableReference>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedHardValue&>().Type), FAngelscriptCachedDataType>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedHardValue&>().CanonicalValue),
			TOptional<FAngelscriptCachedCanonicalValue>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedHardValue&>().HardValueHash), FAngelscriptHash256>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializerUnit&>().InitializerKind),
			EAngelscriptCachedInitializerKind>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializerUnit&>().InitializerKey),
			FAngelscriptStableFunctionKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializerUnit&>().OwnerGlobal),
			TOptional<FAngelscriptStableGlobalKey>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializerUnit&>().VmInitializerCodecVersion), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializerUnit&>().InitializerExecutionHash),
			FAngelscriptHash256>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializerUnit&>().CanonicalExecutionPayload), TArray<uint8>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializationAction&>().ActionOrdinal), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializationAction&>().ActionKind),
			EAngelscriptCachedInitializationActionKind>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializationAction&>().Target),
			FAngelscriptCacheStableReference>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedInitializationAction&>().Dependencies),
			TArray<FAngelscriptCacheSemanticDependency>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedPostInitFunction&>().PostInitOrdinal), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedPostInitFunction&>().Function),
			FAngelscriptCacheStableReference>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().PayloadSchemaVersion), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().ModuleKey), FAngelscriptStableModuleKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().Profile), FAngelscriptArtifactProfileKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().StateInputHash), FAngelscriptHash256>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().OrderedGlobals),
			TArray<FAngelscriptCachedGlobalSchema>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().HardValues), TArray<FAngelscriptCachedHardValue>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().Initializers),
			TArray<FAngelscriptCachedInitializerUnit>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().OrderedInitializationActions),
			TArray<FAngelscriptCachedInitializationAction>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().OrderedPostInitFunctions),
			TArray<FAngelscriptCachedPostInitFunction>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleState&>().Dependencies),
			TArray<FAngelscriptCacheSemanticDependency>>);

		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().PayloadSchemaVersion), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().ModuleKey), FAngelscriptStableModuleKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().Identity),
			FAngelscriptFunctionArtifactIdentity>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().ExpectedDeclarationAbi), FAngelscriptHash256>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().FunctionSourceDigest),
			FAngelscriptFunctionSourceDigest>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().FunctionInputDigest),
			FAngelscriptFunctionInputDigest>);
		static_assert(!HasLegacySourceDigestMember<FAngelscriptCachedFunctionBody>);
		static_assert(!HasLegacyInputDigestMember<FAngelscriptCachedFunctionBody>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().InvocationKind),
			EAngelscriptCachedFunctionInvocationKind>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().VmExecutionCodecVersion), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().CanonicalExecutionPayload), TArray<uint8>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().ActualDependencies),
			TArray<FAngelscriptCacheSemanticDependency>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBody&>().DebugSidecar),
			TOptional<FAngelscriptCacheRecordId>>);

		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedLogicalSectionKey&>().Hash), FAngelscriptHash256>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSourceReference&>().SourceFileKey),
			FAngelscriptCachedSourceFileKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSourceReference&>().LogicalSectionKey),
			FAngelscriptCachedLogicalSectionKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSourceReference&>().CanonicalLogicalSection), FString>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSidecar&>().PayloadSchemaVersion), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSidecar&>().FunctionKey), FAngelscriptStableFunctionKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSidecar&>().Profile), FAngelscriptArtifactProfileKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSidecar&>().DebugHash), FAngelscriptHash256>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSidecar&>().VmDebugCodecVersion), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSidecar&>().Sources),
			TArray<FAngelscriptCachedDebugSourceReference>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedDebugSidecar&>().CanonicalDebugPayload), TArray<uint8>>);

		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleRecordLink&>().ModuleKey), FAngelscriptStableModuleKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleRecordLink&>().RecordId), FAngelscriptCacheRecordId>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedTypeSchemaLink&>().TypeKey), FAngelscriptStableTypeKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedTypeSchemaLink&>().RecordId), FAngelscriptCacheRecordId>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBodyLink&>().FunctionKey),
			FAngelscriptStableFunctionKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedFunctionBodyLink&>().RecordId), FAngelscriptCacheRecordId>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleSnapshot&>().PayloadSchemaVersion), uint32>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleSnapshot&>().ModuleKey), FAngelscriptStableModuleKey>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleSnapshot&>().ModuleInterface),
			FAngelscriptCachedModuleRecordLink>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleSnapshot&>().TypeSchemas),
			TArray<FAngelscriptCachedTypeSchemaLink>>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleSnapshot&>().ModuleState),
			FAngelscriptCachedModuleRecordLink>);
		static_assert(std::is_same_v<decltype(std::declval<
			FAngelscriptCachedModuleSnapshot&>().FunctionBodies),
			TArray<FAngelscriptCachedFunctionBodyLink>>);
		static_assert(!HasManifestOnlyModuleSnapshotsMember<FAngelscriptCachedModuleSnapshot>);
	}

	TEST_METHOD(DefaultConstructionAndOwnedValueTraitsAreFailClosed)
	{
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedGlobalSchema>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedCanonicalValue>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedHardValue>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedInitializerUnit>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedInitializationAction>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedPostInitFunction>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedModuleState>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedFunctionBody>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedLogicalSectionKey>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedDebugSourceReference>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedDebugSidecar>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedModuleRecordLink>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedTypeSchemaLink>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedFunctionBodyLink>);
		static_assert(IsOwnedAggregateDto<FAngelscriptCachedModuleSnapshot>);

		const FAngelscriptCachedGlobalSchema Global;
		ASSERT_THAT(AreEqual(uint32(0), Global.StorageOrdinal));
		ASSERT_THAT(IsTrue(Global.GlobalKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(Global.CanonicalNamespace.IsEmpty()));
		ASSERT_THAT(IsTrue(Global.CanonicalName.IsEmpty()));
		ASSERT_THAT(IsTrue(Global.Type.Kind == EAngelscriptCachedDataTypeKind::Invalid));
		ASSERT_THAT(IsTrue(Global.Type.Primitive
			== EAngelscriptCachedPrimitiveType::Invalid));
		ASSERT_THAT(IsFalse(Global.Type.TypeReference.IsSet()));
		ASSERT_THAT(AreEqual(uint32(0), Global.Type.QualifierFlags));
		ASSERT_THAT(AreEqual(int32(0), Global.Type.OrderedSubTypes.Num()));
		ASSERT_THAT(AreEqual(uint32(0), Global.GlobalTraitFlags));
		ASSERT_THAT(IsTrue(Global.InitializationKind
			== EAngelscriptCachedGlobalInitializationKind::Invalid));
		ASSERT_THAT(IsTrue(Global.CleanupPolicy
			== EAngelscriptCachedGlobalCleanupPolicy::Invalid));
		ASSERT_THAT(IsTrue(Global.StorageLayoutFingerprint.IsZero()));

		const FAngelscriptCachedCanonicalValue CanonicalValue;
		ASSERT_THAT(IsTrue(CanonicalValue.ValueKind
			== EAngelscriptCachedCanonicalValueKind::Invalid));
		ASSERT_THAT(AreEqual(int32(0), CanonicalValue.FixedWidthValueBytes.Num()));

		const FAngelscriptCachedHardValue HardValue;
		ASSERT_THAT(IsTrue(HardValue.HardValueKind == EAngelscriptCachedHardValueKind::Invalid));
		ASSERT_THAT(IsTrue(HardValue.Owner.Kind == EAngelscriptCacheReferenceKind::Invalid));
		ASSERT_THAT(IsTrue(HardValue.Owner.StableKey.IsZero()));
		ASSERT_THAT(IsTrue(HardValue.Owner.ExpectedAbi.IsZero()));
		ASSERT_THAT(IsTrue(HardValue.Type.Kind == EAngelscriptCachedDataTypeKind::Invalid));
		ASSERT_THAT(IsFalse(HardValue.CanonicalValue.IsSet()));
		ASSERT_THAT(IsTrue(HardValue.HardValueHash.IsZero()));

		const FAngelscriptCachedInitializerUnit Initializer;
		ASSERT_THAT(IsTrue(Initializer.InitializerKind
			== EAngelscriptCachedInitializerKind::Invalid));
		ASSERT_THAT(IsTrue(Initializer.InitializerKey.Hash.IsZero()));
		ASSERT_THAT(IsFalse(Initializer.OwnerGlobal.IsSet()));
		ASSERT_THAT(AreEqual(uint32(0), Initializer.VmInitializerCodecVersion));
		ASSERT_THAT(IsTrue(Initializer.InitializerExecutionHash.IsZero()));
		ASSERT_THAT(AreEqual(int32(0), Initializer.CanonicalExecutionPayload.Num()));

		const FAngelscriptCachedInitializationAction Action;
		ASSERT_THAT(AreEqual(uint32(0), Action.ActionOrdinal));
		ASSERT_THAT(IsTrue(Action.ActionKind
			== EAngelscriptCachedInitializationActionKind::Invalid));
		ASSERT_THAT(IsTrue(Action.Target.Kind == EAngelscriptCacheReferenceKind::Invalid));
		ASSERT_THAT(AreEqual(int32(0), Action.Dependencies.Num()));

		const FAngelscriptCachedPostInitFunction PostInit;
		ASSERT_THAT(AreEqual(uint32(0), PostInit.PostInitOrdinal));
		ASSERT_THAT(IsTrue(PostInit.Function.Kind == EAngelscriptCacheReferenceKind::Invalid));
		ASSERT_THAT(IsTrue(PostInit.Function.StableKey.IsZero()));
		ASSERT_THAT(IsTrue(PostInit.Function.ExpectedAbi.IsZero()));

		const FAngelscriptCachedModuleState ModuleState;
		ASSERT_THAT(AreEqual(uint32(0), ModuleState.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(ModuleState.ModuleKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(ModuleState.Profile.Hash.IsZero()));
		ASSERT_THAT(IsTrue(ModuleState.StateInputHash.IsZero()));
		ASSERT_THAT(AreEqual(int32(0), ModuleState.OrderedGlobals.Num()));
		ASSERT_THAT(AreEqual(int32(0), ModuleState.HardValues.Num()));
		ASSERT_THAT(AreEqual(int32(0), ModuleState.Initializers.Num()));
		ASSERT_THAT(AreEqual(int32(0), ModuleState.OrderedInitializationActions.Num()));
		ASSERT_THAT(AreEqual(int32(0), ModuleState.OrderedPostInitFunctions.Num()));
		ASSERT_THAT(AreEqual(int32(0), ModuleState.Dependencies.Num()));

		const FAngelscriptCachedFunctionBody FunctionBody;
		ASSERT_THAT(AreEqual(uint32(0), FunctionBody.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(FunctionBody.ModuleKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(FunctionBody.Identity.FunctionKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(FunctionBody.Identity.Content.Execution.IsZero()));
		ASSERT_THAT(IsTrue(FunctionBody.Identity.Content.Debug.IsZero()));
		ASSERT_THAT(IsTrue(FunctionBody.Identity.Profile.Hash.IsZero()));
		ASSERT_THAT(IsTrue(FunctionBody.ExpectedDeclarationAbi.IsZero()));
		ASSERT_THAT(IsTrue(FunctionBody.FunctionSourceDigest.Hash.IsZero()));
		ASSERT_THAT(IsTrue(FunctionBody.FunctionInputDigest.Hash.IsZero()));
		ASSERT_THAT(IsTrue(FunctionBody.InvocationKind
			== EAngelscriptCachedFunctionInvocationKind::Invalid));
		ASSERT_THAT(AreEqual(uint32(0), FunctionBody.VmExecutionCodecVersion));
		ASSERT_THAT(AreEqual(int32(0), FunctionBody.CanonicalExecutionPayload.Num()));
		ASSERT_THAT(AreEqual(int32(0), FunctionBody.ActualDependencies.Num()));
		ASSERT_THAT(IsFalse(FunctionBody.DebugSidecar.IsSet()));

		const FAngelscriptCachedLogicalSectionKey LogicalSectionKey;
		ASSERT_THAT(IsTrue(LogicalSectionKey.Hash.IsZero()));
		const FAngelscriptCachedDebugSourceReference SourceReference;
		ASSERT_THAT(IsTrue(SourceReference.SourceFileKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(SourceReference.LogicalSectionKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(SourceReference.CanonicalLogicalSection.IsEmpty()));

		const FAngelscriptCachedDebugSidecar DebugSidecar;
		ASSERT_THAT(AreEqual(uint32(0), DebugSidecar.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(DebugSidecar.FunctionKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(DebugSidecar.Profile.Hash.IsZero()));
		ASSERT_THAT(IsTrue(DebugSidecar.DebugHash.IsZero()));
		ASSERT_THAT(AreEqual(uint32(0), DebugSidecar.VmDebugCodecVersion));
		ASSERT_THAT(AreEqual(int32(0), DebugSidecar.Sources.Num()));
		ASSERT_THAT(AreEqual(int32(0), DebugSidecar.CanonicalDebugPayload.Num()));

		const FAngelscriptCachedModuleRecordLink ModuleLink;
		ASSERT_THAT(IsTrue(ModuleLink.ModuleKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(static_cast<uint8>(ModuleLink.RecordId.Kind) == 0));
		ASSERT_THAT(IsTrue(ModuleLink.RecordId.ContentHash.IsZero()));
		const FAngelscriptCachedTypeSchemaLink TypeLink;
		ASSERT_THAT(IsTrue(TypeLink.TypeKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(static_cast<uint8>(TypeLink.RecordId.Kind) == 0));
		ASSERT_THAT(IsTrue(TypeLink.RecordId.ContentHash.IsZero()));
		const FAngelscriptCachedFunctionBodyLink FunctionLink;
		ASSERT_THAT(IsTrue(FunctionLink.FunctionKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(static_cast<uint8>(FunctionLink.RecordId.Kind) == 0));
		ASSERT_THAT(IsTrue(FunctionLink.RecordId.ContentHash.IsZero()));

		const FAngelscriptCachedModuleSnapshot Snapshot;
		ASSERT_THAT(AreEqual(uint32(0), Snapshot.PayloadSchemaVersion));
		ASSERT_THAT(IsTrue(Snapshot.ModuleKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(Snapshot.ModuleInterface.ModuleKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(static_cast<uint8>(Snapshot.ModuleInterface.RecordId.Kind) == 0));
		ASSERT_THAT(IsTrue(Snapshot.ModuleInterface.RecordId.ContentHash.IsZero()));
		ASSERT_THAT(AreEqual(int32(0), Snapshot.TypeSchemas.Num()));
		ASSERT_THAT(IsTrue(Snapshot.ModuleState.ModuleKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(static_cast<uint8>(Snapshot.ModuleState.RecordId.Kind) == 0));
		ASSERT_THAT(IsTrue(Snapshot.ModuleState.RecordId.ContentHash.IsZero()));
		ASSERT_THAT(AreEqual(int32(0), Snapshot.FunctionBodies.Num()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
