#include "Cache/AngelscriptCacheRemainingRecordTypes.h"

#include "Cache/Private/AngelscriptCacheRemainingRecordCodec.h"
#include "Containers/StringConv.h"

namespace AngelscriptCacheRemainingRecords_Private
{
	using AngelscriptCacheCanonicalCodec_Private::BeginRead;
	using AngelscriptCacheCanonicalCodec_Private::FReader;
	using AngelscriptCacheCanonicalCodec_Private::FWriter;
	using AngelscriptCacheCanonicalCodec_Private::ReadEnum;

	static FAngelscriptCacheValidationResult Failure(
		const EAngelscriptCacheValidationError Error,
		const EAngelscriptCacheValidationStage Stage,
		const uint64 Offset = 0)
	{
		return FAngelscriptCacheValidationResult::AtStage(
			Error,
			EAngelscriptCacheRecordKind::DebugSidecar,
			Stage,
			Offset);
	}

	static FAngelscriptCacheValidationResult ValidateString(const FStringView Value)
	{
		if (Value.Len() < 0 || (Value.Len() > 0 && Value.GetData() == nullptr))
		{
			return Failure(
				EAngelscriptCacheValidationError::InvalidArrayView,
				EAngelscriptCacheValidationStage::LocalSemantic);
		}
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const uint32 CodeUnit = static_cast<uint32>(Value[Index]);
			if (CodeUnit == 0)
			{
				return Failure(
					EAngelscriptCacheValidationError::EmbeddedNul,
					EAngelscriptCacheValidationStage::LocalSemantic);
			}
			if constexpr (sizeof(TCHAR) == 2)
			{
				if (CodeUnit >= 0xd800 && CodeUnit <= 0xdbff)
				{
					if (Index + 1 >= Value.Len())
					{
						return Failure(
							EAngelscriptCacheValidationError::InvalidUtf8,
							EAngelscriptCacheValidationStage::LocalSemantic);
					}
					const uint32 Low = static_cast<uint32>(Value[++Index]);
					if (Low < 0xdc00 || Low > 0xdfff)
					{
						return Failure(
							EAngelscriptCacheValidationError::InvalidUtf8,
							EAngelscriptCacheValidationStage::LocalSemantic);
					}
				}
				else if (CodeUnit >= 0xdc00 && CodeUnit <= 0xdfff)
				{
					return Failure(
						EAngelscriptCacheValidationError::InvalidUtf8,
						EAngelscriptCacheValidationStage::LocalSemantic);
				}
			}
			else if (CodeUnit > 0x10ffff || (CodeUnit >= 0xd800 && CodeUnit <= 0xdfff))
			{
				return Failure(
					EAngelscriptCacheValidationError::InvalidUtf8,
					EAngelscriptCacheValidationStage::LocalSemantic);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult RecordFailure(
		EAngelscriptCacheValidationError Error,
		EAngelscriptCacheRecordKind Kind,
		EAngelscriptCacheValidationStage Stage,
		uint64 Offset);
	static int32 CompareSemanticDependency(
		const FAngelscriptCacheSemanticDependency& Left,
		const FAngelscriptCacheSemanticDependency& Right);
	static bool HasSameSemanticDependencyAuthority(
		const FAngelscriptCacheSemanticDependency& Left,
		const FAngelscriptCacheSemanticDependency& Right);
	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateSemanticDependency(
		const FAngelscriptCacheSemanticDependency& Value,
		EAngelscriptCacheRecordKind RecordKind,
		OffsetProviderType&& GetOffset);

	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateModuleStateDependencyArray(
		const TArray<FAngelscriptCacheSemanticDependency>& Dependencies,
		const bool bActionDependencies,
		const int32 ActionIndex,
		OffsetProviderType&& GetOffset)
	{
		using EField = EAngelscriptModuleStateCapturedField;
		for (int32 Index = 0; Index < Dependencies.Num(); ++Index)
		{
			const FAngelscriptCacheSemanticDependency& Dependency = Dependencies[Index];
			const EField RowField = bActionDependencies
				? EField::InitializationActionDependency : EField::Dependency;
			const EField KindField = bActionDependencies
				? EField::InitializationActionDependencyKind : EField::DependencyKind;
			const EField ReferenceKindField = bActionDependencies
				? EField::InitializationActionDependencyTargetReferenceKind
				: EField::DependencyTargetReferenceKind;
			const EField StableKeyField = bActionDependencies
				? EField::InitializationActionDependencyTargetStableKey
				: EField::DependencyTargetStableKey;
			const EField ExpectedAbiField = bActionDependencies
				? EField::InitializationActionDependencyTargetExpectedAbi
				: EField::DependencyTargetExpectedAbi;
			const EField PresenceField = bActionDependencies
				? EField::InitializationActionDependencyExpectedContentOrValuePresence
				: EField::DependencyExpectedContentOrValuePresence;
			const EField ValueField = bActionDependencies
				? EField::InitializationActionDependencyExpectedContentOrValue
				: EField::DependencyExpectedContentOrValue;
			const int32 Primary = bActionDependencies ? ActionIndex : Index;
			const int32 Secondary = bActionDependencies ? Index : INDEX_NONE;
			const FAngelscriptCacheValidationResult Result = ValidateSemanticDependency(
				Dependency,
				EAngelscriptCacheRecordKind::ModuleState,
				[&GetOffset,
					KindField,
					ReferenceKindField,
					StableKeyField,
					ExpectedAbiField,
					PresenceField,
					ValueField,
					Primary,
					Secondary](const int32 Component)
				{
					switch (Component)
					{
					case 0: return GetOffset(KindField, Primary, Secondary);
					case 1: return GetOffset(ReferenceKindField, Primary, Secondary);
					case 2: return GetOffset(StableKeyField, Primary, Secondary);
					case 3: return GetOffset(ExpectedAbiField, Primary, Secondary);
					case 4: return GetOffset(PresenceField, Primary, Secondary);
					case 5: return GetOffset(ValueField, Primary, Secondary);
					default: checkNoEntry(); return UINT64_C(0);
					}
				});
			if (!Result.IsSuccess())
			{
				return Result;
			}
			if (Index > 0)
			{
				const FAngelscriptCacheSemanticDependency& Previous =
					Dependencies[Index - 1];
				if (HasSameSemanticDependencyAuthority(Previous, Dependency))
				{
					return RecordFailure(
						CompareSemanticDependency(Previous, Dependency) == 0
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						EAngelscriptCacheRecordKind::ModuleState,
						EAngelscriptCacheValidationStage::LocalSemantic,
						GetOffset(RowField, Primary, Secondary));
				}
				if (CompareSemanticDependency(Previous, Dependency) >= 0)
				{
					return RecordFailure(
						EAngelscriptCacheValidationError::NonCanonicalOrder,
						EAngelscriptCacheRecordKind::ModuleState,
						EAngelscriptCacheValidationStage::LocalSemantic,
						GetOffset(RowField, Primary, Secondary));
				}
			}
		}
		return {};
	}

	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateCanonicalValue(
		const FAngelscriptCachedHardValue& HardValue,
		const int32 Index,
		OffsetProviderType&& GetOffset)
	{
		using EField = EAngelscriptModuleStateCapturedField;
		if (!HardValue.CanonicalValue.IsSet())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(EField::HardValueCanonicalValuePresence, Index, INDEX_NONE));
		}
		const FAngelscriptCachedCanonicalValue& Value = HardValue.CanonicalValue.GetValue();
		EAngelscriptCachedCanonicalValueKind ExpectedKind =
			EAngelscriptCachedCanonicalValueKind::Invalid;
		int32 ExpectedBytes = 0;
		if (HardValue.Type.Kind == EAngelscriptCachedDataTypeKind::Primitive
			&& HardValue.Type.QualifierFlags == 0
			&& HardValue.Type.OrderedSubTypes.IsEmpty())
		{
			switch (HardValue.Type.Primitive)
			{
			case EAngelscriptCachedPrimitiveType::Bool:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::Bool;
				ExpectedBytes = 1;
				break;
			case EAngelscriptCachedPrimitiveType::Int8:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::SignedInteger;
				ExpectedBytes = 1;
				break;
			case EAngelscriptCachedPrimitiveType::Int16:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::SignedInteger;
				ExpectedBytes = 2;
				break;
			case EAngelscriptCachedPrimitiveType::Int32:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::SignedInteger;
				ExpectedBytes = 4;
				break;
			case EAngelscriptCachedPrimitiveType::Int64:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::SignedInteger;
				ExpectedBytes = 8;
				break;
			case EAngelscriptCachedPrimitiveType::UInt8:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::UnsignedInteger;
				ExpectedBytes = 1;
				break;
			case EAngelscriptCachedPrimitiveType::UInt16:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::UnsignedInteger;
				ExpectedBytes = 2;
				break;
			case EAngelscriptCachedPrimitiveType::UInt32:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::UnsignedInteger;
				ExpectedBytes = 4;
				break;
			case EAngelscriptCachedPrimitiveType::UInt64:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::UnsignedInteger;
				ExpectedBytes = 8;
				break;
			case EAngelscriptCachedPrimitiveType::Float32:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::Float32;
				ExpectedBytes = 4;
				break;
			case EAngelscriptCachedPrimitiveType::Float64:
				ExpectedKind = EAngelscriptCachedCanonicalValueKind::Float64;
				ExpectedBytes = 8;
				break;
			default:
				break;
			}
		}
		else if (HardValue.Type.Kind == EAngelscriptCachedDataTypeKind::ScriptType
			&& HardValue.Type.QualifierFlags == 0
			&& HardValue.Type.OrderedSubTypes.IsEmpty())
		{
			ExpectedKind = EAngelscriptCachedCanonicalValueKind::EnumInt32;
			ExpectedBytes = 4;
		}
		if (ExpectedKind == EAngelscriptCachedCanonicalValueKind::Invalid
			|| Value.ValueKind != ExpectedKind
			|| Value.FixedWidthValueBytes.Num() != ExpectedBytes)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(EField::HardValueCanonicalValue, Index, INDEX_NONE));
		}
		if (Value.ValueKind == EAngelscriptCachedCanonicalValueKind::Bool
			&& Value.FixedWidthValueBytes[0] > 1)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::InvalidBoolean,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(
					EField::HardValueCanonicalValueFixedWidthValueBytes,
					Index,
					INDEX_NONE));
		}
		return {};
	}

	static int32 CompareSource(
		const FAngelscriptCachedDebugSourceReference& Left,
		const FAngelscriptCachedDebugSourceReference& Right)
	{
		if (Left.SourceFileKey.Hash < Right.SourceFileKey.Hash)
		{
			return -1;
		}
		if (Right.SourceFileKey.Hash < Left.SourceFileKey.Hash)
		{
			return 1;
		}
		if (Left.LogicalSectionKey.Hash < Right.LogicalSectionKey.Hash)
		{
			return -1;
		}
		if (Right.LogicalSectionKey.Hash < Left.LogicalSectionKey.Hash)
		{
			return 1;
		}
		return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
			Left.CanonicalLogicalSection,
			Right.CanonicalLogicalSection);
	}

	static FAngelscriptCacheValidationResult ValidateDebugSource(
		const FAngelscriptCachedDebugSourceReference& Source,
		const uint64 SourceFileOffset,
		const uint64 LogicalKeyOffset,
		const uint64 StringOffset)
	{
		if (Source.SourceFileKey.Hash.IsZero())
		{
			return Failure(
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptCacheValidationStage::LocalSemantic,
				SourceFileOffset);
		}
		if (const FAngelscriptCacheValidationResult StringResult =
			ValidateString(Source.CanonicalLogicalSection);
			!StringResult.IsSuccess())
		{
			return Failure(
				StringResult.Error,
				EAngelscriptCacheValidationStage::LocalSemantic,
				StringOffset);
		}
		FAngelscriptCachedLogicalSectionKey Expected;
		const FAngelscriptCacheValidationResult KeyResult =
			FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
				Source.SourceFileKey,
				Source.CanonicalLogicalSection,
				Expected);
		if (!KeyResult.IsSuccess())
		{
			return Failure(
				KeyResult.Error,
				EAngelscriptCacheValidationStage::LocalSemantic,
				LogicalKeyOffset);
		}
		if (!(Expected.Hash == Source.LogicalSectionKey.Hash))
		{
			return Failure(
				EAngelscriptCacheValidationError::DerivedHashMismatch,
				EAngelscriptCacheValidationStage::LocalSemantic,
				LogicalKeyOffset);
		}
		return {};
	}

	template <typename SourceOffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateDebugSidecar(
		const FAngelscriptCachedDebugSidecar& Value,
		SourceOffsetProviderType&& GetSourceOffset,
		const uint64 PayloadSchemaVersionOffset,
		const uint64 FunctionKeyOffset,
		const uint64 ProfileOffset,
		const uint64 DebugHashOffset,
		const uint64 VmDebugCodecVersionOffset)
	{
		if (Value.PayloadSchemaVersion
			!= FAngelscriptCacheRemainingRecordArchive::DebugSidecarPayloadSchemaVersion)
		{
			return Failure(
				EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
				EAngelscriptCacheValidationStage::LocalSemantic,
				PayloadSchemaVersionOffset);
		}
		if (Value.FunctionKey.Hash.IsZero())
		{
			return Failure(
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptCacheValidationStage::LocalSemantic,
				FunctionKeyOffset);
		}
		if (Value.Profile.Hash.IsZero())
		{
			return Failure(
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptCacheValidationStage::LocalSemantic,
				ProfileOffset);
		}
		if (Value.VmDebugCodecVersion == 0)
		{
			return Failure(
				EAngelscriptCacheValidationError::UnsupportedCodecVersion,
				EAngelscriptCacheValidationStage::LocalSemantic,
				VmDebugCodecVersionOffset);
		}
		for (int32 Index = 0; Index < Value.Sources.Num(); ++Index)
		{
			if (const FAngelscriptCacheValidationResult SourceResult = ValidateDebugSource(
				Value.Sources[Index],
				GetSourceOffset(Index, 1),
				GetSourceOffset(Index, 2),
				GetSourceOffset(Index, 3));
				!SourceResult.IsSuccess())
			{
				return SourceResult;
			}
			if (Index > 0)
			{
				const FAngelscriptCachedDebugSourceReference& Previous =
					Value.Sources[Index - 1];
				const FAngelscriptCachedDebugSourceReference& Current = Value.Sources[Index];
				const bool bSameKey = Previous.SourceFileKey.Hash == Current.SourceFileKey.Hash
					&& Previous.LogicalSectionKey.Hash == Current.LogicalSectionKey.Hash;
				if (bSameKey)
				{
					return Failure(
						Previous.CanonicalLogicalSection == Current.CanonicalLogicalSection
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						EAngelscriptCacheValidationStage::LocalSemantic,
						GetSourceOffset(Index, 0));
				}
				if (CompareSource(Previous, Current) >= 0)
				{
					return Failure(
						EAngelscriptCacheValidationError::NonCanonicalOrder,
						EAngelscriptCacheValidationStage::LocalSemantic,
						GetSourceOffset(Index, 0));
				}
			}
		}
		const FAngelscriptHash256 ExpectedDebugHash =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				{}, Value.CanonicalDebugPayload).Debug;
		if (!(ExpectedDebugHash == Value.DebugHash))
		{
			return Failure(
				EAngelscriptCacheValidationError::DerivedHashMismatch,
				EAngelscriptCacheValidationStage::LocalSemantic,
				DebugHashOffset);
		}
		return {};
	}

	static void WriteDebugSidecar(
		const FAngelscriptCachedDebugSidecar& Value,
		TArray<uint8>& OutPayload)
	{
		FWriter Writer;
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteHash(Value.FunctionKey.Hash);
		Writer.WriteHash(Value.Profile.Hash);
		Writer.WriteHash(Value.DebugHash);
		Writer.WriteUInt32(Value.VmDebugCodecVersion);
		Writer.WriteUInt32(static_cast<uint32>(Value.Sources.Num()));
		for (const FAngelscriptCachedDebugSourceReference& Source : Value.Sources)
		{
			Writer.WriteHash(Source.SourceFileKey.Hash);
			Writer.WriteHash(Source.LogicalSectionKey.Hash);
			Writer.WriteString(Source.CanonicalLogicalSection);
		}
		Writer.WriteByteArray(Value.CanonicalDebugPayload);
		OutPayload = MoveTemp(Writer.Bytes);
	}

	static FAngelscriptCacheValidationResult RecordFailure(
		const EAngelscriptCacheValidationError Error,
		const EAngelscriptCacheRecordKind Kind,
		const EAngelscriptCacheValidationStage Stage,
		const uint64 Offset = 0)
	{
		return FAngelscriptCacheValidationResult::AtStage(Error, Kind, Stage, Offset);
	}

	static void WriteRecordId(FWriter& Writer, const FAngelscriptCacheRecordId& RecordId)
	{
		Writer.WriteUInt8(static_cast<uint8>(RecordId.Kind));
		Writer.WriteHash(RecordId.ContentHash);
	}

	static bool ReadRecordId(FReader& Reader, FAngelscriptCacheRecordId& OutRecordId)
	{
		uint8 RawKind = 0;
		if (!Reader.ConsumeReference()
			|| !Reader.ReadUInt8(RawKind)
			|| !Reader.ReadHash(OutRecordId.ContentHash))
		{
			return false;
		}
		OutRecordId.Kind = static_cast<EAngelscriptCacheRecordKind>(RawKind);
		return true;
	}

	static bool DependencyRequiresContent(
		const EAngelscriptCacheSemanticDependencyKind Kind)
	{
		return Kind == EAngelscriptCacheSemanticDependencyKind::ValueLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::PropertyLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::GlobalStorage
			|| Kind == EAngelscriptCacheSemanticDependencyKind::HardValue
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Initializer
			|| Kind == EAngelscriptCacheSemanticDependencyKind::CompileOption
			|| Kind == EAngelscriptCacheSemanticDependencyKind::FunctionContent;
	}

	static int32 CompareHash(
		const FAngelscriptHash256& Left,
		const FAngelscriptHash256& Right)
	{
		if (Left == Right)
		{
			return 0;
		}
		return Left < Right ? -1 : 1;
	}

	static int32 CompareSemanticDependency(
		const FAngelscriptCacheSemanticDependency& Left,
		const FAngelscriptCacheSemanticDependency& Right)
	{
		if (Left.Kind != Right.Kind)
		{
			return static_cast<uint8>(Left.Kind) < static_cast<uint8>(Right.Kind)
				? -1 : 1;
		}
		if (Left.Target.Kind != Right.Target.Kind)
		{
			return static_cast<uint8>(Left.Target.Kind)
				< static_cast<uint8>(Right.Target.Kind) ? -1 : 1;
		}
		if (const int32 Key = CompareHash(
			Left.Target.StableKey, Right.Target.StableKey); Key != 0)
		{
			return Key;
		}
		if (const int32 Abi = CompareHash(
			Left.Target.ExpectedAbi, Right.Target.ExpectedAbi); Abi != 0)
		{
			return Abi;
		}
		if (Left.ExpectedContentOrValue.IsSet()
			!= Right.ExpectedContentOrValue.IsSet())
		{
			return Left.ExpectedContentOrValue.IsSet() ? 1 : -1;
		}
		return Left.ExpectedContentOrValue.IsSet()
			? CompareHash(
				Left.ExpectedContentOrValue.GetValue(),
				Right.ExpectedContentOrValue.GetValue())
			: 0;
	}

	static bool HasSameSemanticDependencyAuthority(
		const FAngelscriptCacheSemanticDependency& Left,
		const FAngelscriptCacheSemanticDependency& Right)
	{
		return Left.Kind == Right.Kind
			&& Left.Target.Kind == Right.Target.Kind
			&& Left.Target.StableKey == Right.Target.StableKey;
	}

	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateSemanticDependency(
		const FAngelscriptCacheSemanticDependency& Value,
		const EAngelscriptCacheRecordKind RecordKind,
		OffsetProviderType&& GetOffset)
	{
		const uint8 RawDependencyKind = static_cast<uint8>(Value.Kind);
		if (RawDependencyKind < 1 || RawDependencyKind > 12)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::UnknownEnumValue,
				RecordKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(0));
		}
		const uint8 RawReferenceKind = static_cast<uint8>(Value.Target.Kind);
		if (RawReferenceKind < 1 || RawReferenceKind > 9)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::UnknownEnumValue,
				RecordKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(1));
		}
		if (Value.Target.StableKey.IsZero())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::ZeroStableKey,
				RecordKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(2));
		}
		const bool bRequiresAbi = RawReferenceKind
			<= static_cast<uint8>(EAngelscriptCacheReferenceKind::EnvironmentSymbol);
		if (bRequiresAbi && Value.Target.ExpectedAbi.IsZero())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::MissingExpectedAbi,
				RecordKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(3));
		}
		if (!bRequiresAbi && !Value.Target.ExpectedAbi.IsZero())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::ForbiddenExpectedAbi,
				RecordKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(3));
		}
		if (DependencyRequiresContent(Value.Kind)
			!= Value.ExpectedContentOrValue.IsSet())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::InvalidPresence,
				RecordKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(4));
		}
		if (Value.ExpectedContentOrValue.IsSet()
			&& Value.ExpectedContentOrValue->IsZero())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::ZeroStableKey,
				RecordKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(5));
		}
		if (Value.Kind == EAngelscriptCacheSemanticDependencyKind::FunctionContent
			&& Value.Target.Kind
				!= EAngelscriptCacheReferenceKind::ScriptFunction)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::WrongReferenceKind,
				RecordKind,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(1));
		}
		return {};
	}

	static void WriteSemanticDependency(
		FWriter& Writer,
		const FAngelscriptCacheSemanticDependency& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteUInt8(static_cast<uint8>(Value.Target.Kind));
		Writer.WriteHash(Value.Target.StableKey);
		Writer.WriteHash(Value.Target.ExpectedAbi);
		Writer.WriteOptionalHash(Value.ExpectedContentOrValue);
	}

	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateFunctionBody(
		const FAngelscriptCachedFunctionBody& Value,
		OffsetProviderType&& GetOffset)
	{
		using EField = EAngelscriptFunctionBodyCapturedField;
		const auto Fail = [&GetOffset](
			const EAngelscriptCacheValidationError Error,
			const EField Field,
			const int32 PrimaryIndex = INDEX_NONE)
		{
			return RecordFailure(
				Error,
				EAngelscriptCacheRecordKind::FunctionBody,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(Field, PrimaryIndex));
		};
		if (Value.PayloadSchemaVersion
			!= FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion)
		{
			return Fail(EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
				EField::PayloadSchemaVersion);
		}
		if (Value.ModuleKey.Hash.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::ModuleKey);
		}
		if (Value.Identity.FunctionKey.Hash.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::IdentityFunctionKey);
		}
		if (Value.Identity.Content.Execution.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::IdentityContentExecution);
		}
		if (Value.Identity.Content.Debug.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::IdentityContentDebug);
		}
		if (Value.Identity.Profile.Hash.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::IdentityProfile);
		}
		if (Value.ExpectedDeclarationAbi.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::MissingExpectedAbi,
				EField::ExpectedDeclarationAbi);
		}
		if (Value.FunctionSourceDigest.Hash.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::FunctionSourceDigest);
		}
		if (Value.FunctionInputDigest.Hash.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::FunctionInputDigest);
		}
		const uint8 RawInvocationKind = static_cast<uint8>(Value.InvocationKind);
		if (RawInvocationKind < 1 || RawInvocationKind > 10)
		{
			return Fail(EAngelscriptCacheValidationError::UnknownEnumValue,
				EField::InvocationKind);
		}
		if (Value.VmExecutionCodecVersion == 0)
		{
			return Fail(EAngelscriptCacheValidationError::UnsupportedCodecVersion,
				EField::VmExecutionCodecVersion);
		}
		// CanonicalExecutionPayload is codec-owned opaque data.  Its semantic
		// execution hash is therefore returned by the selected opaque codec and
		// compared by ModuleGraph validation; the generic record layer must not
		// assume that the identity hashes every envelope byte directly.
		for (int32 Index = 0; Index < Value.ActualDependencies.Num(); ++Index)
		{
			const FAngelscriptCacheSemanticDependency& Dependency =
				Value.ActualDependencies[Index];
			const FAngelscriptCacheValidationResult DependencyResult =
				ValidateSemanticDependency(
					Dependency,
					EAngelscriptCacheRecordKind::FunctionBody,
					[&GetOffset, Index](const int32 Component)
					{
						switch (Component)
						{
						case 0: return GetOffset(EField::ActualDependencyKind, Index);
						case 1: return GetOffset(
							EField::ActualDependencyTargetReferenceKind, Index);
						case 2: return GetOffset(
							EField::ActualDependencyTargetStableKey, Index);
						case 3: return GetOffset(
							EField::ActualDependencyTargetExpectedAbi, Index);
						case 4: return GetOffset(
							EField::ActualDependencyExpectedContentOrValuePresence, Index);
						case 5: return GetOffset(
							EField::ActualDependencyExpectedContentOrValue, Index);
						default: checkNoEntry(); return UINT64_C(0);
						}
					});
			if (!DependencyResult.IsSuccess())
			{
				return DependencyResult;
			}
			if (Index > 0)
			{
				const FAngelscriptCacheSemanticDependency& Previous =
					Value.ActualDependencies[Index - 1];
				if (HasSameSemanticDependencyAuthority(Previous, Dependency))
				{
					return Fail(
						CompareSemanticDependency(Previous, Dependency) == 0
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						EField::ActualDependency,
						Index);
				}
				if (CompareSemanticDependency(Previous, Dependency) >= 0)
				{
					return Fail(EAngelscriptCacheValidationError::NonCanonicalOrder,
						EField::ActualDependency, Index);
				}
			}
		}
		if (Value.DebugSidecar.IsSet())
		{
			if (Value.DebugSidecar->Kind != EAngelscriptCacheRecordKind::DebugSidecar)
			{
				return Fail(EAngelscriptCacheValidationError::WrongRecordKind,
					EField::DebugSidecarKind);
			}
			if (Value.DebugSidecar->ContentHash.IsZero())
			{
				return Fail(EAngelscriptCacheValidationError::InvalidPresence,
					EField::DebugSidecarContentHash);
			}
		}
		else
		{
			const FAngelscriptHash256 ExpectedAbsentDebug =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(
					Value.Identity.Profile);
			if (!(ExpectedAbsentDebug == Value.Identity.Content.Debug))
			{
				return Fail(EAngelscriptCacheValidationError::DerivedHashMismatch,
					EField::IdentityContentDebug);
			}
		}
		return {};
	}

	static void WriteFunctionBody(
		const FAngelscriptCachedFunctionBody& Value,
		TArray<uint8>& OutPayload)
	{
		FWriter Writer;
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteHash(Value.Identity.FunctionKey.Hash);
		Writer.WriteHash(Value.Identity.Content.Execution);
		Writer.WriteHash(Value.Identity.Content.Debug);
		Writer.WriteHash(Value.Identity.Profile.Hash);
		Writer.WriteHash(Value.ExpectedDeclarationAbi);
		Writer.WriteHash(Value.FunctionSourceDigest.Hash);
		Writer.WriteHash(Value.FunctionInputDigest.Hash);
		Writer.WriteUInt8(static_cast<uint8>(Value.InvocationKind));
		Writer.WriteUInt32(Value.VmExecutionCodecVersion);
		Writer.WriteByteArray(Value.CanonicalExecutionPayload);
		Writer.WriteUInt32(static_cast<uint32>(Value.ActualDependencies.Num()));
		for (const FAngelscriptCacheSemanticDependency& Dependency
			: Value.ActualDependencies)
		{
			WriteSemanticDependency(Writer, Dependency);
		}
		Writer.WriteUInt8(Value.DebugSidecar.IsSet() ? 1 : 0);
		if (Value.DebugSidecar.IsSet())
		{
			WriteRecordId(Writer, Value.DebugSidecar.GetValue());
		}
		OutPayload = MoveTemp(Writer.Bytes);
	}

	static void WriteStableReference(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const FAngelscriptCacheStableReference& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteHash(Value.StableKey);
		Writer.WriteHash(Value.ExpectedAbi);
	}

	static void WriteSemanticDependency(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const FAngelscriptCacheSemanticDependency& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		WriteStableReference(Writer, Value.Target);
		Writer.WriteUInt8(Value.ExpectedContentOrValue.IsSet() ? 1 : 0);
		if (Value.ExpectedContentOrValue.IsSet())
		{
			Writer.WriteHash(Value.ExpectedContentOrValue.GetValue());
		}
	}

	template <typename WriterType>
	static void WriteModuleStateDataType(
		WriterType& Writer,
		const FAngelscriptCachedDataType& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteUInt8(static_cast<uint8>(Value.Primitive));
		Writer.WriteUInt8(Value.TypeReference.IsSet() ? 1 : 0);
		if (Value.TypeReference.IsSet())
		{
			if constexpr (std::is_same_v<WriterType, FWriter>)
			{
				Writer.WriteUInt8(static_cast<uint8>(Value.TypeReference->Kind));
				Writer.WriteHash(Value.TypeReference->StableKey);
				Writer.WriteHash(Value.TypeReference->ExpectedAbi);
			}
			else
			{
				WriteStableReference(Writer, Value.TypeReference.GetValue());
			}
		}
		Writer.WriteUInt32(Value.QualifierFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedSubTypes.Num()));
		for (const FAngelscriptCachedDataType& SubType : Value.OrderedSubTypes)
		{
			WriteModuleStateDataType(Writer, SubType);
		}
	}

	static void WriteModuleStateInputStream(
		const FAngelscriptCachedModuleState& Value,
		FAngelscriptArtifactCanonicalWriter& Writer)
	{
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteHash(Value.Profile.Hash);
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedGlobals.Num()));
		for (const FAngelscriptCachedGlobalSchema& Global : Value.OrderedGlobals)
		{
			Writer.WriteUInt32(Global.StorageOrdinal);
			Writer.WriteHash(Global.GlobalKey.Hash);
			Writer.WriteString(Global.CanonicalNamespace);
			Writer.WriteString(Global.CanonicalName);
			WriteModuleStateDataType(Writer, Global.Type);
			Writer.WriteUInt32(Global.GlobalTraitFlags);
			Writer.WriteUInt8(static_cast<uint8>(Global.InitializationKind));
			Writer.WriteUInt8(static_cast<uint8>(Global.CleanupPolicy));
			Writer.WriteHash(Global.StorageLayoutFingerprint);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.HardValues.Num()));
		for (const FAngelscriptCachedHardValue& HardValue : Value.HardValues)
		{
			Writer.WriteUInt8(static_cast<uint8>(HardValue.HardValueKind));
			WriteStableReference(Writer, HardValue.Owner);
			WriteModuleStateDataType(Writer, HardValue.Type);
			Writer.WriteUInt8(HardValue.CanonicalValue.IsSet() ? 1 : 0);
			if (HardValue.CanonicalValue.IsSet())
			{
				Writer.WriteUInt8(static_cast<uint8>(HardValue.CanonicalValue->ValueKind));
				Writer.WriteBytes(HardValue.CanonicalValue->FixedWidthValueBytes);
			}
			Writer.WriteHash(HardValue.HardValueHash);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Initializers.Num()));
		for (const FAngelscriptCachedInitializerUnit& Initializer : Value.Initializers)
		{
			Writer.WriteUInt8(static_cast<uint8>(Initializer.InitializerKind));
			Writer.WriteHash(Initializer.InitializerKey.Hash);
			Writer.WriteUInt8(Initializer.OwnerGlobal.IsSet() ? 1 : 0);
			if (Initializer.OwnerGlobal.IsSet())
			{
				Writer.WriteHash(Initializer.OwnerGlobal->Hash);
			}
			Writer.WriteUInt32(Initializer.VmInitializerCodecVersion);
			Writer.WriteHash(Initializer.InitializerExecutionHash);
			Writer.WriteBytes(Initializer.CanonicalExecutionPayload);
		}
		Writer.WriteUInt32(
			static_cast<uint32>(Value.OrderedInitializationActions.Num()));
		for (const FAngelscriptCachedInitializationAction& Action
			: Value.OrderedInitializationActions)
		{
			Writer.WriteUInt32(Action.ActionOrdinal);
			Writer.WriteUInt8(static_cast<uint8>(Action.ActionKind));
			WriteStableReference(Writer, Action.Target);
			Writer.WriteUInt32(static_cast<uint32>(Action.Dependencies.Num()));
			for (const FAngelscriptCacheSemanticDependency& Dependency
				: Action.Dependencies)
			{
				WriteSemanticDependency(Writer, Dependency);
			}
		}
		Writer.WriteUInt32(
			static_cast<uint32>(Value.OrderedPostInitFunctions.Num()));
		for (const FAngelscriptCachedPostInitFunction& PostInit
			: Value.OrderedPostInitFunctions)
		{
			Writer.WriteUInt32(PostInit.PostInitOrdinal);
			WriteStableReference(Writer, PostInit.Function);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Dependencies.Num()));
		for (const FAngelscriptCacheSemanticDependency& Dependency : Value.Dependencies)
		{
			WriteSemanticDependency(Writer, Dependency);
		}
	}

	static FAngelscriptHash256 BuildModuleStateInputHash(
		const FAngelscriptCachedModuleState& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-module-state-input-v1"));
		WriteModuleStateInputStream(Value, Writer);
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 BuildGlobalStorageLayoutFingerprint(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedGlobalSchema& Global)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-global-storage-layout-v1"));
		Writer.WriteHash(ModuleKey.Hash);
		Writer.WriteHash(Global.GlobalKey.Hash);
		Writer.WriteUInt32(Global.StorageOrdinal);
		Writer.WriteString(Global.CanonicalNamespace);
		Writer.WriteString(Global.CanonicalName);
		WriteModuleStateDataType(Writer, Global.Type);
		Writer.WriteUInt32(Global.GlobalTraitFlags);
		Writer.WriteUInt8(static_cast<uint8>(Global.InitializationKind));
		Writer.WriteUInt8(static_cast<uint8>(Global.CleanupPolicy));
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 BuildGlobalConstantHardValueHash(
		const FAngelscriptCachedHardValue& HardValue)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-hard-value-v1"));
		Writer.WriteUInt8(static_cast<uint8>(HardValue.HardValueKind));
		WriteStableReference(Writer, HardValue.Owner);
		WriteModuleStateDataType(Writer, HardValue.Type);
		check(HardValue.CanonicalValue.IsSet());
		Writer.WriteUInt8(static_cast<uint8>(HardValue.CanonicalValue->ValueKind));
		Writer.WriteBytes(HardValue.CanonicalValue->FixedWidthValueBytes);
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 BuildInitializerExecutionHash(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptArtifactProfileKey& Profile,
		const FAngelscriptCachedInitializerUnit& Initializer)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-initializer-execution-v1"));
		Writer.WriteHash(ModuleKey.Hash);
		Writer.WriteHash(Profile.Hash);
		Writer.WriteHash(Initializer.InitializerKey.Hash);
		Writer.WriteUInt32(Initializer.VmInitializerCodecVersion);
		Writer.WriteBytes(Initializer.CanonicalExecutionPayload);
		return Writer.FinalizeHash();
	}

	static int32 CompareHardValueAuthority(
		const FAngelscriptCachedHardValue& Left,
		const FAngelscriptCachedHardValue& Right)
	{
		if (Left.HardValueKind != Right.HardValueKind)
		{
			return static_cast<uint8>(Left.HardValueKind)
				< static_cast<uint8>(Right.HardValueKind) ? -1 : 1;
		}
		if (Left.Owner.Kind != Right.Owner.Kind)
		{
			return static_cast<uint8>(Left.Owner.Kind)
				< static_cast<uint8>(Right.Owner.Kind) ? -1 : 1;
		}
		return CompareHash(Left.Owner.StableKey, Right.Owner.StableKey);
	}

	static int32 CompareInitializerAuthority(
		const FAngelscriptCachedInitializerUnit& Left,
		const FAngelscriptCachedInitializerUnit& Right)
	{
		return CompareHash(Left.InitializerKey.Hash, Right.InitializerKey.Hash);
	}

	static void CanonicalizeModuleStateSets(FAngelscriptCachedModuleState& Value)
	{
		Value.HardValues.Sort([](
			const FAngelscriptCachedHardValue& Left,
			const FAngelscriptCachedHardValue& Right)
		{
			return CompareHardValueAuthority(Left, Right) < 0;
		});
		Value.Initializers.Sort([](
			const FAngelscriptCachedInitializerUnit& Left,
			const FAngelscriptCachedInitializerUnit& Right)
		{
			return CompareInitializerAuthority(Left, Right) < 0;
		});
		for (FAngelscriptCachedInitializationAction& Action
			: Value.OrderedInitializationActions)
		{
			Action.Dependencies.Sort([](
				const FAngelscriptCacheSemanticDependency& Left,
				const FAngelscriptCacheSemanticDependency& Right)
			{
				return CompareSemanticDependency(Left, Right) < 0;
			});
		}
		Value.Dependencies.Sort([](
			const FAngelscriptCacheSemanticDependency& Left,
			const FAngelscriptCacheSemanticDependency& Right)
		{
			return CompareSemanticDependency(Left, Right) < 0;
		});
	}

	static void WriteModuleStatePayload(
		const FAngelscriptCachedModuleState& Value,
		TArray<uint8>& OutPayload)
	{
		FWriter Writer;
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteHash(Value.Profile.Hash);
		Writer.WriteHash(Value.StateInputHash);
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedGlobals.Num()));
		for (const FAngelscriptCachedGlobalSchema& Global : Value.OrderedGlobals)
		{
			Writer.WriteUInt32(Global.StorageOrdinal);
			Writer.WriteHash(Global.GlobalKey.Hash);
			Writer.WriteString(Global.CanonicalNamespace);
			Writer.WriteString(Global.CanonicalName);
			WriteModuleStateDataType(Writer, Global.Type);
			Writer.WriteUInt32(Global.GlobalTraitFlags);
			Writer.WriteUInt8(static_cast<uint8>(Global.InitializationKind));
			Writer.WriteUInt8(static_cast<uint8>(Global.CleanupPolicy));
			Writer.WriteHash(Global.StorageLayoutFingerprint);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.HardValues.Num()));
		for (const FAngelscriptCachedHardValue& HardValue : Value.HardValues)
		{
			Writer.WriteUInt8(static_cast<uint8>(HardValue.HardValueKind));
			Writer.WriteUInt8(static_cast<uint8>(HardValue.Owner.Kind));
			Writer.WriteHash(HardValue.Owner.StableKey);
			Writer.WriteHash(HardValue.Owner.ExpectedAbi);
			WriteModuleStateDataType(Writer, HardValue.Type);
			Writer.WriteUInt8(HardValue.CanonicalValue.IsSet() ? 1 : 0);
			if (HardValue.CanonicalValue.IsSet())
			{
				Writer.WriteUInt8(static_cast<uint8>(HardValue.CanonicalValue->ValueKind));
				Writer.WriteByteArray(HardValue.CanonicalValue->FixedWidthValueBytes);
			}
			Writer.WriteHash(HardValue.HardValueHash);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Initializers.Num()));
		for (const FAngelscriptCachedInitializerUnit& Initializer : Value.Initializers)
		{
			Writer.WriteUInt8(static_cast<uint8>(Initializer.InitializerKind));
			Writer.WriteHash(Initializer.InitializerKey.Hash);
			Writer.WriteUInt8(Initializer.OwnerGlobal.IsSet() ? 1 : 0);
			if (Initializer.OwnerGlobal.IsSet())
			{
				Writer.WriteHash(Initializer.OwnerGlobal->Hash);
			}
			Writer.WriteUInt32(Initializer.VmInitializerCodecVersion);
			Writer.WriteHash(Initializer.InitializerExecutionHash);
			Writer.WriteByteArray(Initializer.CanonicalExecutionPayload);
		}
		Writer.WriteUInt32(
			static_cast<uint32>(Value.OrderedInitializationActions.Num()));
		for (const FAngelscriptCachedInitializationAction& Action
			: Value.OrderedInitializationActions)
		{
			Writer.WriteUInt32(Action.ActionOrdinal);
			Writer.WriteUInt8(static_cast<uint8>(Action.ActionKind));
			Writer.WriteUInt8(static_cast<uint8>(Action.Target.Kind));
			Writer.WriteHash(Action.Target.StableKey);
			Writer.WriteHash(Action.Target.ExpectedAbi);
			Writer.WriteUInt32(static_cast<uint32>(Action.Dependencies.Num()));
			for (const FAngelscriptCacheSemanticDependency& Dependency
				: Action.Dependencies)
			{
				WriteSemanticDependency(Writer, Dependency);
			}
		}
		Writer.WriteUInt32(
			static_cast<uint32>(Value.OrderedPostInitFunctions.Num()));
		for (const FAngelscriptCachedPostInitFunction& PostInit
			: Value.OrderedPostInitFunctions)
		{
			Writer.WriteUInt32(PostInit.PostInitOrdinal);
			Writer.WriteUInt8(static_cast<uint8>(PostInit.Function.Kind));
			Writer.WriteHash(PostInit.Function.StableKey);
			Writer.WriteHash(PostInit.Function.ExpectedAbi);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Dependencies.Num()));
		for (const FAngelscriptCacheSemanticDependency& Dependency : Value.Dependencies)
		{
			WriteSemanticDependency(Writer, Dependency);
		}
		OutPayload = MoveTemp(Writer.Bytes);
	}

	static bool AreDataTypesEqual(
		const FAngelscriptCachedDataType& Left,
		const FAngelscriptCachedDataType& Right)
	{
		if (Left.Kind != Right.Kind
			|| Left.Primitive != Right.Primitive
			|| Left.QualifierFlags != Right.QualifierFlags
			|| Left.TypeReference.IsSet() != Right.TypeReference.IsSet()
			|| Left.OrderedSubTypes.Num() != Right.OrderedSubTypes.Num())
		{
			return false;
		}
		if (Left.TypeReference.IsSet()
			&& Left.TypeReference.GetValue() != Right.TypeReference.GetValue())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.OrderedSubTypes.Num(); ++Index)
		{
			if (!AreDataTypesEqual(
				Left.OrderedSubTypes[Index], Right.OrderedSubTypes[Index]))
			{
				return false;
			}
		}
		return true;
	}

	static bool AreCanonicalValuesEqual(
		const TOptional<FAngelscriptCachedCanonicalValue>& Left,
		const TOptional<FAngelscriptCachedCanonicalValue>& Right)
	{
		return Left.IsSet() == Right.IsSet()
			&& (!Left.IsSet()
				|| (Left->ValueKind == Right->ValueKind
					&& Left->FixedWidthValueBytes == Right->FixedWidthValueBytes));
	}

	static bool AreHardValueContentsEqual(
		const FAngelscriptCachedHardValue& Left,
		const FAngelscriptCachedHardValue& Right)
	{
		return Left.Owner.ExpectedAbi == Right.Owner.ExpectedAbi
			&& AreDataTypesEqual(Left.Type, Right.Type)
			&& AreCanonicalValuesEqual(Left.CanonicalValue, Right.CanonicalValue)
			&& Left.HardValueHash == Right.HardValueHash;
	}

	static bool AreInitializerContentsEqual(
		const FAngelscriptCachedInitializerUnit& Left,
		const FAngelscriptCachedInitializerUnit& Right)
	{
		return Left.InitializerKind == Right.InitializerKind
			&& Left.OwnerGlobal.IsSet() == Right.OwnerGlobal.IsSet()
			&& (!Left.OwnerGlobal.IsSet()
				|| Left.OwnerGlobal->Hash == Right.OwnerGlobal->Hash)
			&& Left.VmInitializerCodecVersion == Right.VmInitializerCodecVersion
			&& Left.InitializerExecutionHash == Right.InitializerExecutionHash
			&& Left.CanonicalExecutionPayload == Right.CanonicalExecutionPayload;
	}

	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateModuleStateStableReference(
		const FAngelscriptCacheStableReference& Value,
		const EAngelscriptModuleStateCapturedField KindField,
		const EAngelscriptModuleStateCapturedField StableKeyField,
		const EAngelscriptModuleStateCapturedField ExpectedAbiField,
		const int32 PrimaryIndex,
		const int32 SecondaryIndex,
		OffsetProviderType&& GetOffset)
	{
		const uint8 RawKind = static_cast<uint8>(Value.Kind);
		if (RawKind < 1 || RawKind > 9)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::UnknownEnumValue,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(KindField, PrimaryIndex, SecondaryIndex));
		}
		if (Value.StableKey.IsZero())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(StableKeyField, PrimaryIndex, SecondaryIndex));
		}
		const bool bRequiresAbi = RawKind
			<= static_cast<uint8>(EAngelscriptCacheReferenceKind::EnvironmentSymbol);
		if (bRequiresAbi == Value.ExpectedAbi.IsZero())
		{
			return RecordFailure(
				bRequiresAbi
					? EAngelscriptCacheValidationError::MissingExpectedAbi
					: EAngelscriptCacheValidationError::ForbiddenExpectedAbi,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(ExpectedAbiField, PrimaryIndex, SecondaryIndex));
		}
		return {};
	}

	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateModuleStateDataType(
		const FAngelscriptCachedDataType& Value,
		const bool bHardValue,
		const int32 PrimaryIndex,
		uint32& InOutNodeOrdinal,
		const uint64 Depth,
		OffsetProviderType&& GetOffset)
	{
		using EField = EAngelscriptModuleStateCapturedField;
		const int32 NodeOrdinal = static_cast<int32>(InOutNodeOrdinal++);
		const EField KindField = bHardValue
			? EField::HardValueTypeKind : EField::GlobalTypeKind;
		const EField PrimitiveField = bHardValue
			? EField::HardValueTypePrimitive : EField::GlobalTypePrimitive;
		const EField PresenceField = bHardValue
			? EField::HardValueTypeReferencePresence
			: EField::GlobalTypeReferencePresence;
		const EField ReferenceKindField = bHardValue
			? EField::HardValueTypeReferenceKind
			: EField::GlobalTypeReferenceKind;
		const EField ReferenceKeyField = bHardValue
			? EField::HardValueTypeReferenceStableKey
			: EField::GlobalTypeReferenceStableKey;
		const EField ReferenceAbiField = bHardValue
			? EField::HardValueTypeReferenceExpectedAbi
			: EField::GlobalTypeReferenceExpectedAbi;
		const EField QualifierField = bHardValue
			? EField::HardValueTypeQualifierFlags
			: EField::GlobalTypeQualifierFlags;
		if (Depth > FAngelscriptCacheReadLimits::DefaultMaxNestingDepth)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::NestingDepthExceeded,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(KindField, PrimaryIndex, NodeOrdinal));
		}
		const uint8 RawKind = static_cast<uint8>(Value.Kind);
		if (RawKind < 1 || RawKind > 4)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::UnknownEnumValue,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(KindField, PrimaryIndex, NodeOrdinal));
		}
		const uint32 KnownFlags = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::KnownMask);
		if ((Value.QualifierFlags & ~KnownFlags) != 0)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::UnknownFlags,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(QualifierField, PrimaryIndex, NodeOrdinal));
		}
		const uint32 Auto = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::Auto);
		const uint32 Handle = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		const uint32 ConstHandle = static_cast<uint32>(
			EAngelscriptCachedTypeQualifierFlags::ConstHandle);
		if ((Value.Kind == EAngelscriptCachedDataTypeKind::Auto)
			!= ((Value.QualifierFlags & Auto) != 0)
			|| ((Value.QualifierFlags & ConstHandle) != 0
				&& (Value.QualifierFlags & Handle) == 0))
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::InvalidQualifierCombination,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(QualifierField, PrimaryIndex, NodeOrdinal));
		}
		switch (Value.Kind)
		{
		case EAngelscriptCachedDataTypeKind::Primitive:
			if (Value.Primitive == EAngelscriptCachedPrimitiveType::Invalid
				|| static_cast<uint8>(Value.Primitive) > 12)
			{
				return RecordFailure(
					EAngelscriptCacheValidationError::UnknownEnumValue,
					EAngelscriptCacheRecordKind::ModuleState,
					EAngelscriptCacheValidationStage::LocalSemantic,
					GetOffset(PrimitiveField, PrimaryIndex, NodeOrdinal));
			}
			if (Value.TypeReference.IsSet())
			{
				return RecordFailure(
					EAngelscriptCacheValidationError::InvalidPresence,
					EAngelscriptCacheRecordKind::ModuleState,
					EAngelscriptCacheValidationStage::LocalSemantic,
					GetOffset(PresenceField, PrimaryIndex, NodeOrdinal));
			}
			break;
		case EAngelscriptCachedDataTypeKind::ScriptType:
		case EAngelscriptCachedDataTypeKind::EnvironmentType:
			if (Value.Primitive != EAngelscriptCachedPrimitiveType::Invalid
				|| !Value.TypeReference.IsSet())
			{
				return RecordFailure(
					EAngelscriptCacheValidationError::InvalidPresence,
					EAngelscriptCacheRecordKind::ModuleState,
					EAngelscriptCacheValidationStage::LocalSemantic,
					GetOffset(PresenceField, PrimaryIndex, NodeOrdinal));
			}
			if ((Value.Kind == EAngelscriptCachedDataTypeKind::ScriptType
					&& Value.TypeReference->Kind
						!= EAngelscriptCacheReferenceKind::ScriptType)
				|| (Value.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType
					&& Value.TypeReference->Kind
						!= EAngelscriptCacheReferenceKind::EnvironmentSymbol))
			{
				return RecordFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					EAngelscriptCacheRecordKind::ModuleState,
					EAngelscriptCacheValidationStage::LocalSemantic,
					GetOffset(ReferenceKindField, PrimaryIndex, NodeOrdinal));
			}
			if (const FAngelscriptCacheValidationResult ReferenceResult =
				ValidateModuleStateStableReference(
					Value.TypeReference.GetValue(),
					ReferenceKindField,
					ReferenceKeyField,
					ReferenceAbiField,
					PrimaryIndex,
					NodeOrdinal,
					GetOffset);
				!ReferenceResult.IsSuccess())
			{
				return ReferenceResult;
			}
			break;
		case EAngelscriptCachedDataTypeKind::Auto:
			if (Value.Primitive != EAngelscriptCachedPrimitiveType::Invalid
				|| Value.TypeReference.IsSet() || Value.QualifierFlags != Auto)
			{
				return RecordFailure(
					EAngelscriptCacheValidationError::InvalidQualifierCombination,
					EAngelscriptCacheRecordKind::ModuleState,
					EAngelscriptCacheValidationStage::LocalSemantic,
					GetOffset(QualifierField, PrimaryIndex, NodeOrdinal));
			}
			break;
		default:
			break;
		}
		for (const FAngelscriptCachedDataType& SubType : Value.OrderedSubTypes)
		{
			if (const FAngelscriptCacheValidationResult Result =
				ValidateModuleStateDataType(
					SubType,
					bHardValue,
					PrimaryIndex,
					InOutNodeOrdinal,
					Depth + 1,
					GetOffset);
				!Result.IsSuccess())
			{
				return Result;
			}
		}
		return {};
	}

	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateModuleState(
		const FAngelscriptCachedModuleState& Value,
		OffsetProviderType&& GetOffset)
	{
		using EField = EAngelscriptModuleStateCapturedField;
		const auto Fail = [&GetOffset](
			const EAngelscriptCacheValidationError Error,
			const EField Field,
			const int32 PrimaryIndex = INDEX_NONE,
			const int32 SecondaryIndex = INDEX_NONE)
		{
			return RecordFailure(
				Error,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(Field, PrimaryIndex, SecondaryIndex));
		};
		if (Value.PayloadSchemaVersion
			!= FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion)
		{
			return Fail(EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
				EField::PayloadSchemaVersion);
		}
		if (Value.ModuleKey.Hash.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::ModuleKey);
		}
		if (Value.Profile.Hash.IsZero())
		{
			return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
				EField::Profile);
		}

		for (int32 Index = 0; Index < Value.OrderedGlobals.Num(); ++Index)
		{
			const FAngelscriptCachedGlobalSchema& Global = Value.OrderedGlobals[Index];
			if (Global.StorageOrdinal != static_cast<uint32>(Index))
			{
				return Fail(
					Global.StorageOrdinal < static_cast<uint32>(Index)
						? EAngelscriptCacheValidationError::DuplicateOrdinal
						: EAngelscriptCacheValidationError::OrdinalGap,
					EField::GlobalStorageOrdinal,
					Index);
			}
			if (Global.GlobalKey.Hash.IsZero())
			{
				return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
					EField::GlobalKey, Index);
			}
			for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
			{
				if (Value.OrderedGlobals[PreviousIndex].GlobalKey == Global.GlobalKey)
				{
					return Fail(EAngelscriptCacheValidationError::DuplicateKey,
						EField::Global, Index);
				}
			}
			if (const FAngelscriptCacheValidationResult StringResult =
				ValidateString(Global.CanonicalNamespace); !StringResult.IsSuccess())
			{
				return Fail(StringResult.Error, EField::GlobalCanonicalNamespace, Index);
			}
			if (const FAngelscriptCacheValidationResult StringResult =
				ValidateString(Global.CanonicalName); !StringResult.IsSuccess())
			{
				return Fail(StringResult.Error, EField::GlobalCanonicalName, Index);
			}
			uint32 NodeOrdinal = 0;
			if (const FAngelscriptCacheValidationResult TypeResult =
				ValidateModuleStateDataType(
					Global.Type, false, Index, NodeOrdinal, 1, GetOffset);
				!TypeResult.IsSuccess())
			{
				return TypeResult;
			}
			const uint32 KnownTraits = static_cast<uint32>(
				EAngelscriptCachedDeclarationTraitFlags::KnownMask);
			if ((Global.GlobalTraitFlags & ~KnownTraits) != 0)
			{
				return Fail(EAngelscriptCacheValidationError::UnknownFlags,
					EField::GlobalTraitFlags, Index);
			}
			const uint8 InitKind = static_cast<uint8>(Global.InitializationKind);
			if (InitKind < 1 || InitKind > 3)
			{
				return Fail(EAngelscriptCacheValidationError::UnknownEnumValue,
					EField::GlobalInitializationKind, Index);
			}
			const uint8 Cleanup = static_cast<uint8>(Global.CleanupPolicy);
			if (Cleanup < 1 || Cleanup > 3)
			{
				return Fail(EAngelscriptCacheValidationError::UnknownEnumValue,
					EField::GlobalCleanupPolicy, Index);
			}
			const uint32 ReferenceQualifier = static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::Reference);
			if (Global.Type.Kind == EAngelscriptCachedDataTypeKind::Auto
				|| (Global.Type.Kind == EAngelscriptCachedDataTypeKind::Primitive
					&& Global.Type.Primitive == EAngelscriptCachedPrimitiveType::Void)
				|| (Global.Type.QualifierFlags & ReferenceQualifier) != 0
				|| (Global.Type.Kind == EAngelscriptCachedDataTypeKind::Primitive
					&& Global.CleanupPolicy
						!= EAngelscriptCachedGlobalCleanupPolicy::None))
			{
				return Fail(EAngelscriptCacheValidationError::InvalidPresence,
					EField::GlobalCleanupPolicy, Index);
			}
		}

		for (int32 Index = 0; Index < Value.HardValues.Num(); ++Index)
		{
			const FAngelscriptCachedHardValue& HardValue = Value.HardValues[Index];
			const uint8 Kind = static_cast<uint8>(HardValue.HardValueKind);
			if (Kind < 1 || Kind > 2)
			{
				return Fail(EAngelscriptCacheValidationError::UnknownEnumValue,
					EField::HardValueKind, Index);
			}
			if (const FAngelscriptCacheValidationResult ReferenceResult =
				ValidateModuleStateStableReference(
					HardValue.Owner,
					EField::HardValueOwnerReferenceKind,
					EField::HardValueOwnerStableKey,
					EField::HardValueOwnerExpectedAbi,
					Index,
					INDEX_NONE,
					GetOffset);
				!ReferenceResult.IsSuccess())
			{
				return ReferenceResult;
			}
			if ((HardValue.HardValueKind == EAngelscriptCachedHardValueKind::GlobalConstant
					&& HardValue.Owner.Kind
						!= EAngelscriptCacheReferenceKind::ScriptGlobal)
				|| (HardValue.HardValueKind
						== EAngelscriptCachedHardValueKind::EnumAuthority
					&& HardValue.Owner.Kind
						!= EAngelscriptCacheReferenceKind::ScriptType))
			{
				return Fail(EAngelscriptCacheValidationError::WrongReferenceKind,
					EField::HardValueOwnerReferenceKind, Index);
			}
			uint32 NodeOrdinal = 0;
			if (const FAngelscriptCacheValidationResult TypeResult =
				ValidateModuleStateDataType(
					HardValue.Type, true, Index, NodeOrdinal, 1, GetOffset);
				!TypeResult.IsSuccess())
			{
				return TypeResult;
			}
			if (HardValue.HardValueKind
				== EAngelscriptCachedHardValueKind::GlobalConstant)
			{
				if (const FAngelscriptCacheValidationResult ValueResult =
					ValidateCanonicalValue(HardValue, Index, GetOffset);
					!ValueResult.IsSuccess())
				{
					return ValueResult;
				}
			}
			else if (HardValue.CanonicalValue.IsSet())
			{
				return Fail(EAngelscriptCacheValidationError::InvalidPresence,
					EField::HardValueCanonicalValuePresence, Index);
			}
			if (Index > 0)
			{
				const FAngelscriptCachedHardValue& Previous = Value.HardValues[Index - 1];
				const int32 Order = CompareHardValueAuthority(Previous, HardValue);
				if (Order == 0)
				{
					return Fail(
						AreHardValueContentsEqual(Previous, HardValue)
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						EField::HardValue,
						Index);
				}
				if (Order > 0)
				{
					return Fail(EAngelscriptCacheValidationError::NonCanonicalOrder,
						EField::HardValue, Index);
				}
			}
		}

		int32 ModuleInitializerCount = 0;
		for (int32 Index = 0; Index < Value.Initializers.Num(); ++Index)
		{
			const FAngelscriptCachedInitializerUnit& Initializer = Value.Initializers[Index];
			const uint8 Kind = static_cast<uint8>(Initializer.InitializerKind);
			if (Kind < 1 || Kind > 2)
			{
				return Fail(EAngelscriptCacheValidationError::UnknownEnumValue,
					EField::InitializerKind, Index);
			}
			if (Initializer.InitializerKey.Hash.IsZero())
			{
				return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
					EField::InitializerKey, Index);
			}
			if ((Initializer.InitializerKind == EAngelscriptCachedInitializerKind::Global)
				!= Initializer.OwnerGlobal.IsSet())
			{
				return Fail(EAngelscriptCacheValidationError::InvalidPresence,
					EField::InitializerOwnerGlobalPresence, Index);
			}
			if (Initializer.OwnerGlobal.IsSet() && Initializer.OwnerGlobal->Hash.IsZero())
			{
				return Fail(EAngelscriptCacheValidationError::ZeroStableKey,
					EField::InitializerOwnerGlobal, Index);
			}
			if (Initializer.InitializerKind == EAngelscriptCachedInitializerKind::Module
				&& ++ModuleInitializerCount > 1)
			{
				return Fail(EAngelscriptCacheValidationError::ConflictingKey,
					EField::Initializer, Index);
			}
			if (Index > 0)
			{
				const FAngelscriptCachedInitializerUnit& Previous =
					Value.Initializers[Index - 1];
				const int32 Order = CompareInitializerAuthority(Previous, Initializer);
				if (Order == 0)
				{
					return Fail(
						AreInitializerContentsEqual(Previous, Initializer)
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						EField::Initializer,
						Index);
				}
				if (Order > 0)
				{
					return Fail(EAngelscriptCacheValidationError::NonCanonicalOrder,
						EField::Initializer, Index);
				}
			}
		}

		for (int32 Index = 0; Index < Value.OrderedInitializationActions.Num(); ++Index)
		{
			const FAngelscriptCachedInitializationAction& Action =
				Value.OrderedInitializationActions[Index];
			if (Action.ActionOrdinal != static_cast<uint32>(Index))
			{
				return Fail(
					Action.ActionOrdinal < static_cast<uint32>(Index)
						? EAngelscriptCacheValidationError::DuplicateOrdinal
						: EAngelscriptCacheValidationError::OrdinalGap,
					EField::InitializationActionOrdinal,
					Index);
			}
			const uint8 Kind = static_cast<uint8>(Action.ActionKind);
			if (Kind < 1 || Kind > 2)
			{
				return Fail(EAngelscriptCacheValidationError::UnknownEnumValue,
					EField::InitializationActionKind, Index);
			}
			if (const FAngelscriptCacheValidationResult ReferenceResult =
				ValidateModuleStateStableReference(
					Action.Target,
					EField::InitializationActionTargetReferenceKind,
					EField::InitializationActionTargetStableKey,
					EField::InitializationActionTargetExpectedAbi,
					Index,
					INDEX_NONE,
					GetOffset);
				!ReferenceResult.IsSuccess())
			{
				return ReferenceResult;
			}
			const EAngelscriptCacheReferenceKind RequiredKind =
				Action.ActionKind
					== EAngelscriptCachedInitializationActionKind::DefaultConstructGlobal
				? EAngelscriptCacheReferenceKind::ScriptGlobal
				: EAngelscriptCacheReferenceKind::ScriptFunction;
			if (Action.Target.Kind != RequiredKind)
			{
				return Fail(EAngelscriptCacheValidationError::WrongReferenceKind,
					EField::InitializationActionTargetReferenceKind, Index);
			}
			if (const FAngelscriptCacheValidationResult DependencyResult =
				ValidateModuleStateDependencyArray(
					Action.Dependencies, true, Index, GetOffset);
				!DependencyResult.IsSuccess())
			{
				return DependencyResult;
			}
			for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
			{
				const FAngelscriptCachedInitializationAction& Previous =
					Value.OrderedInitializationActions[PreviousIndex];
				if (Previous.Target.Kind == Action.Target.Kind
					&& Previous.Target.StableKey == Action.Target.StableKey)
				{
					return Fail(EAngelscriptCacheValidationError::ConflictingKey,
						EField::InitializationAction, Index);
				}
			}
		}

		for (int32 Index = 0; Index < Value.OrderedPostInitFunctions.Num(); ++Index)
		{
			const FAngelscriptCachedPostInitFunction& PostInit =
				Value.OrderedPostInitFunctions[Index];
			if (PostInit.PostInitOrdinal != static_cast<uint32>(Index))
			{
				return Fail(
					PostInit.PostInitOrdinal < static_cast<uint32>(Index)
						? EAngelscriptCacheValidationError::DuplicateOrdinal
						: EAngelscriptCacheValidationError::OrdinalGap,
					EField::PostInitOrdinal,
					Index);
			}
			if (const FAngelscriptCacheValidationResult ReferenceResult =
				ValidateModuleStateStableReference(
					PostInit.Function,
					EField::PostInitFunctionReferenceKind,
					EField::PostInitFunctionStableKey,
					EField::PostInitFunctionExpectedAbi,
					Index,
					INDEX_NONE,
					GetOffset);
				!ReferenceResult.IsSuccess())
			{
				return ReferenceResult;
			}
			if (PostInit.Function.Kind != EAngelscriptCacheReferenceKind::ScriptFunction)
			{
				return Fail(EAngelscriptCacheValidationError::WrongReferenceKind,
					EField::PostInitFunctionReferenceKind, Index);
			}
			for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
			{
				const FAngelscriptCacheStableReference& Previous =
					Value.OrderedPostInitFunctions[PreviousIndex].Function;
				if (Previous.StableKey == PostInit.Function.StableKey)
				{
					return Fail(
						Previous.ExpectedAbi == PostInit.Function.ExpectedAbi
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						EField::PostInitFunction,
						Index);
				}
			}
		}

		if (const FAngelscriptCacheValidationResult DependencyResult =
			ValidateModuleStateDependencyArray(Value.Dependencies, false, INDEX_NONE, GetOffset);
			!DependencyResult.IsSuccess())
		{
			return DependencyResult;
		}

		for (int32 GlobalIndex = 0; GlobalIndex < Value.OrderedGlobals.Num(); ++GlobalIndex)
		{
			const FAngelscriptCachedGlobalSchema& Global = Value.OrderedGlobals[GlobalIndex];
			int32 ConstantCount = 0;
			int32 GlobalInitializerCount = 0;
			int32 ExecuteCount = 0;
			int32 DefaultConstructCount = 0;
			for (const FAngelscriptCachedHardValue& HardValue : Value.HardValues)
			{
				ConstantCount += HardValue.HardValueKind
						== EAngelscriptCachedHardValueKind::GlobalConstant
					&& HardValue.Owner.StableKey == Global.GlobalKey.Hash ? 1 : 0;
			}
			for (const FAngelscriptCachedInitializerUnit& Initializer : Value.Initializers)
			{
				GlobalInitializerCount += Initializer.InitializerKind
						== EAngelscriptCachedInitializerKind::Global
					&& Initializer.OwnerGlobal.IsSet()
					&& Initializer.OwnerGlobal->Hash == Global.GlobalKey.Hash ? 1 : 0;
			}
			for (const FAngelscriptCachedInitializationAction& Action
				: Value.OrderedInitializationActions)
			{
				DefaultConstructCount += Action.ActionKind
						== EAngelscriptCachedInitializationActionKind::DefaultConstructGlobal
					&& Action.Target.StableKey == Global.GlobalKey.Hash ? 1 : 0;
				if (Action.ActionKind
					== EAngelscriptCachedInitializationActionKind::ExecuteInitializer)
				{
					for (const FAngelscriptCachedInitializerUnit& Initializer
						: Value.Initializers)
					{
						ExecuteCount += Initializer.InitializerKind
								== EAngelscriptCachedInitializerKind::Global
							&& Initializer.OwnerGlobal.IsSet()
							&& Initializer.OwnerGlobal->Hash == Global.GlobalKey.Hash
							&& Initializer.InitializerKey.Hash
								== Action.Target.StableKey ? 1 : 0;
					}
				}
			}
			const bool bValidCoverage =
				(Global.InitializationKind == EAngelscriptCachedGlobalInitializationKind::Default
					&& ConstantCount == 0 && GlobalInitializerCount == 0 && ExecuteCount == 0
					&& DefaultConstructCount
						== (Global.CleanupPolicy
							== EAngelscriptCachedGlobalCleanupPolicy::DestroyValue ? 1 : 0))
				|| (Global.InitializationKind
						== EAngelscriptCachedGlobalInitializationKind::PureConstant
					&& Global.CleanupPolicy == EAngelscriptCachedGlobalCleanupPolicy::None
					&& (Global.GlobalTraitFlags & static_cast<uint32>(
						EAngelscriptCachedDeclarationTraitFlags::Const)) != 0
					&& ConstantCount == 1 && GlobalInitializerCount == 0
					&& ExecuteCount == 0 && DefaultConstructCount == 0)
				|| (Global.InitializationKind
						== EAngelscriptCachedGlobalInitializationKind::VmInitializer
					&& ConstantCount == 0 && GlobalInitializerCount == 1
					&& ExecuteCount == 1 && DefaultConstructCount == 0);
			if (!bValidCoverage)
			{
				return Fail(EAngelscriptCacheValidationError::GlobalCoverageMismatch,
					EField::GlobalInitializationKind, GlobalIndex);
			}
		}
		for (int32 InitializerIndex = 0;
			InitializerIndex < Value.Initializers.Num(); ++InitializerIndex)
		{
			const FAngelscriptCachedInitializerUnit& Initializer =
				Value.Initializers[InitializerIndex];
			int32 ExecuteCount = 0;
			for (const FAngelscriptCachedInitializationAction& Action
				: Value.OrderedInitializationActions)
			{
				ExecuteCount += Action.ActionKind
						== EAngelscriptCachedInitializationActionKind::ExecuteInitializer
					&& Action.Target.StableKey == Initializer.InitializerKey.Hash ? 1 : 0;
			}
			if (ExecuteCount != 1)
			{
				return Fail(
					EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
					EField::Initializer,
					InitializerIndex);
			}
		}

		for (int32 Index = 0; Index < Value.OrderedGlobals.Num(); ++Index)
		{
			const FAngelscriptCachedGlobalSchema& Global = Value.OrderedGlobals[Index];
			if (!(BuildGlobalStorageLayoutFingerprint(Value.ModuleKey, Global)
				== Global.StorageLayoutFingerprint))
			{
				return Fail(EAngelscriptCacheValidationError::DerivedHashMismatch,
					EField::GlobalStorageLayoutFingerprint, Index);
			}
		}
		for (int32 Index = 0; Index < Value.HardValues.Num(); ++Index)
		{
			const FAngelscriptCachedHardValue& HardValue = Value.HardValues[Index];
			if (HardValue.HardValueKind
					== EAngelscriptCachedHardValueKind::GlobalConstant
				&& !(BuildGlobalConstantHardValueHash(HardValue)
					== HardValue.HardValueHash))
			{
				return Fail(EAngelscriptCacheValidationError::DerivedHashMismatch,
					EField::HardValueHash, Index);
			}
		}
		for (int32 Index = 0; Index < Value.Initializers.Num(); ++Index)
		{
			const FAngelscriptCachedInitializerUnit& Initializer = Value.Initializers[Index];
			if (!(BuildInitializerExecutionHash(
				Value.ModuleKey, Value.Profile, Initializer)
				== Initializer.InitializerExecutionHash))
			{
				return Fail(EAngelscriptCacheValidationError::DerivedHashMismatch,
					EField::InitializerExecutionHash, Index);
			}
		}
		if (!(BuildModuleStateInputHash(Value) == Value.StateInputHash))
		{
			return Fail(EAngelscriptCacheValidationError::DerivedHashMismatch,
				EField::StateInputHash);
		}
		return {};
	}

	static int32 CompareTypeSchemaLink(
		const FAngelscriptCachedTypeSchemaLink& Left,
		const FAngelscriptCachedTypeSchemaLink& Right)
	{
		if (Left.TypeKey.Hash < Right.TypeKey.Hash)
		{
			return -1;
		}
		if (Right.TypeKey.Hash < Left.TypeKey.Hash)
		{
			return 1;
		}
		if (Left.RecordId < Right.RecordId)
		{
			return -1;
		}
		if (Right.RecordId < Left.RecordId)
		{
			return 1;
		}
		return 0;
	}

	static int32 CompareFunctionBodyLink(
		const FAngelscriptCachedFunctionBodyLink& Left,
		const FAngelscriptCachedFunctionBodyLink& Right)
	{
		if (Left.FunctionKey.Hash < Right.FunctionKey.Hash)
		{
			return -1;
		}
		if (Right.FunctionKey.Hash < Left.FunctionKey.Hash)
		{
			return 1;
		}
		if (Left.RecordId < Right.RecordId)
		{
			return -1;
		}
		if (Right.RecordId < Left.RecordId)
		{
			return 1;
		}
		return 0;
	}

	template <typename OffsetProviderType>
	static FAngelscriptCacheValidationResult ValidateModuleSnapshot(
		const FAngelscriptCachedModuleSnapshot& Value,
		OffsetProviderType&& GetOffset)
	{
		const auto Fail = [&GetOffset](
			const EAngelscriptCacheValidationError Error,
			const EAngelscriptModuleSnapshotCapturedField Field,
			const int32 PrimaryIndex = INDEX_NONE)
		{
			return RecordFailure(
				Error,
				EAngelscriptCacheRecordKind::ModuleSnapshot,
				EAngelscriptCacheValidationStage::LocalSemantic,
				GetOffset(Field, PrimaryIndex));
		};
		const auto ValidateRecordId = [&Fail](
			const FAngelscriptCacheRecordId& RecordId,
			const EAngelscriptCacheRecordKind ExpectedKind,
			const EAngelscriptModuleSnapshotCapturedField KindField,
			const EAngelscriptModuleSnapshotCapturedField HashField,
			const int32 PrimaryIndex = INDEX_NONE) -> FAngelscriptCacheValidationResult
		{
			if (RecordId.Kind != ExpectedKind)
			{
				return Fail(
					EAngelscriptCacheValidationError::WrongRecordKind,
					KindField,
					PrimaryIndex);
			}
			if (RecordId.ContentHash.IsZero())
			{
				return Fail(
					EAngelscriptCacheValidationError::InvalidPresence,
					HashField,
					PrimaryIndex);
			}
			return {};
		};

		if (Value.PayloadSchemaVersion
			!= FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion)
		{
			return Fail(
				EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
				EAngelscriptModuleSnapshotCapturedField::PayloadSchemaVersion);
		}
		if (Value.ModuleKey.Hash.IsZero())
		{
			return Fail(
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptModuleSnapshotCapturedField::ModuleKey);
		}
		if (Value.ModuleInterface.ModuleKey.Hash.IsZero())
		{
			return Fail(
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceModuleKey);
		}
		if (Value.ModuleInterface.ModuleKey != Value.ModuleKey)
		{
			return Fail(
				EAngelscriptCacheValidationError::CrossModuleOwner,
				EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceModuleKey);
		}
		if (const FAngelscriptCacheValidationResult Result = ValidateRecordId(
			Value.ModuleInterface.RecordId,
			EAngelscriptCacheRecordKind::ModuleInterface,
			EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordIdKind,
			EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordIdContentHash);
			!Result.IsSuccess())
		{
			return Result;
		}

		for (int32 Index = 0; Index < Value.TypeSchemas.Num(); ++Index)
		{
			const FAngelscriptCachedTypeSchemaLink& Link = Value.TypeSchemas[Index];
			if (Link.TypeKey.Hash.IsZero())
			{
				return Fail(
					EAngelscriptCacheValidationError::ZeroStableKey,
					EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkTypeKey,
					Index);
			}
			if (const FAngelscriptCacheValidationResult Result = ValidateRecordId(
				Link.RecordId,
				EAngelscriptCacheRecordKind::TypeSchema,
				EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordIdKind,
				EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordIdContentHash,
				Index);
				!Result.IsSuccess())
			{
				return Result;
			}
			if (Index > 0)
			{
				const FAngelscriptCachedTypeSchemaLink& Previous = Value.TypeSchemas[Index - 1];
				if (Previous.TypeKey == Link.TypeKey)
				{
					return Fail(
						Previous.RecordId == Link.RecordId
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						EAngelscriptModuleSnapshotCapturedField::TypeSchemaLink,
						Index);
				}
				if (CompareTypeSchemaLink(Previous, Link) >= 0)
				{
					return Fail(
						EAngelscriptCacheValidationError::NonCanonicalOrder,
						EAngelscriptModuleSnapshotCapturedField::TypeSchemaLink,
						Index);
				}
			}
		}

		if (Value.ModuleState.ModuleKey.Hash.IsZero())
		{
			return Fail(
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptModuleSnapshotCapturedField::ModuleStateModuleKey);
		}
		if (Value.ModuleState.ModuleKey != Value.ModuleKey)
		{
			return Fail(
				EAngelscriptCacheValidationError::CrossModuleOwner,
				EAngelscriptModuleSnapshotCapturedField::ModuleStateModuleKey);
		}
		if (const FAngelscriptCacheValidationResult Result = ValidateRecordId(
			Value.ModuleState.RecordId,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordIdKind,
			EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordIdContentHash);
			!Result.IsSuccess())
		{
			return Result;
		}

		for (int32 Index = 0; Index < Value.FunctionBodies.Num(); ++Index)
		{
			const FAngelscriptCachedFunctionBodyLink& Link = Value.FunctionBodies[Index];
			if (Link.FunctionKey.Hash.IsZero())
			{
				return Fail(
					EAngelscriptCacheValidationError::ZeroStableKey,
					EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkFunctionKey,
					Index);
			}
			if (const FAngelscriptCacheValidationResult Result = ValidateRecordId(
				Link.RecordId,
				EAngelscriptCacheRecordKind::FunctionBody,
				EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordIdKind,
				EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordIdContentHash,
				Index);
				!Result.IsSuccess())
			{
				return Result;
			}
			if (Index > 0)
			{
				const FAngelscriptCachedFunctionBodyLink& Previous =
					Value.FunctionBodies[Index - 1];
				if (Previous.FunctionKey == Link.FunctionKey)
				{
					return Fail(
						Previous.RecordId == Link.RecordId
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						EAngelscriptModuleSnapshotCapturedField::FunctionBodyLink,
						Index);
				}
				if (CompareFunctionBodyLink(Previous, Link) >= 0)
				{
					return Fail(
						EAngelscriptCacheValidationError::NonCanonicalOrder,
						EAngelscriptModuleSnapshotCapturedField::FunctionBodyLink,
						Index);
				}
			}
		}
		return {};
	}

	static void WriteModuleSnapshot(
		const FAngelscriptCachedModuleSnapshot& Value,
		TArray<uint8>& OutPayload)
	{
		FWriter Writer;
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteHash(Value.ModuleInterface.ModuleKey.Hash);
		WriteRecordId(Writer, Value.ModuleInterface.RecordId);
		Writer.WriteUInt32(static_cast<uint32>(Value.TypeSchemas.Num()));
		for (const FAngelscriptCachedTypeSchemaLink& Link : Value.TypeSchemas)
		{
			Writer.WriteHash(Link.TypeKey.Hash);
			WriteRecordId(Writer, Link.RecordId);
		}
		Writer.WriteHash(Value.ModuleState.ModuleKey.Hash);
		WriteRecordId(Writer, Value.ModuleState.RecordId);
		Writer.WriteUInt32(static_cast<uint32>(Value.FunctionBodies.Num()));
		for (const FAngelscriptCachedFunctionBodyLink& Link : Value.FunctionBodies)
		{
			Writer.WriteHash(Link.FunctionKey.Hash);
			WriteRecordId(Writer, Link.RecordId);
		}
		OutPayload = MoveTemp(Writer.Bytes);
	}

	bool FDecodedRecordCodecBridge::ReadModuleStateDataType(
		FReader& Reader,
		FAngelscriptCachedDataType& OutValue,
		FAngelscriptDecodedCacheRecord::FModuleStateCapturedOffsetStorage::
			FDataTypeOffsets& OutOffsets,
		const uint32 PrimaryIndex,
		const bool bHardValue,
		uint32& InOutNodeOrdinal,
		const uint64 Depth)
	{
		using EField = EAngelscriptModuleStateCapturedField;
		if (Depth > Reader.GetLimits().MaxNestingDepth)
		{
			Reader.Fail(EAngelscriptCacheValidationError::NestingDepthExceeded);
			return false;
		}
		const uint32 NodeOrdinal = InOutNodeOrdinal++;
		const uint64 NodeOffset = Reader.GetOffset();
		if (!ReadEnum(Reader, OutValue.Kind, 1, 4))
		{
			return false;
		}
		const uint64 PrimitiveOffset = Reader.GetOffset();
		uint8 RawPrimitive = 0;
		if (!Reader.ReadUInt8(RawPrimitive))
		{
			return false;
		}
		if (RawPrimitive > 12)
		{
			Reader.Fail(EAngelscriptCacheValidationError::UnknownEnumValue,
				PrimitiveOffset);
			return false;
		}
		OutValue.Primitive = static_cast<EAngelscriptCachedPrimitiveType>(RawPrimitive);
		const uint64 ReferencePresenceOffset = Reader.GetOffset();
		uint8 ReferenceTag = 0;
		if (!Reader.ReadUInt8(ReferenceTag))
		{
			return false;
		}
		if (ReferenceTag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag,
				ReferencePresenceOffset);
			return false;
		}
		uint64 ReferenceOffset = 0;
		if (ReferenceTag == 1)
		{
			ReferenceOffset = Reader.GetOffset();
			FAngelscriptCacheStableReference Reference;
			if (!Reader.ConsumeReference()
				|| !ReadEnum(Reader, Reference.Kind, 1, 9)
				|| !Reader.ReadHash(Reference.StableKey)
				|| !Reader.ReadHash(Reference.ExpectedAbi))
			{
				return false;
			}
			OutValue.TypeReference = Reference;
		}
		const uint64 QualifierOffset = Reader.GetOffset();
		if (!Reader.ReadUInt32(OutValue.QualifierFlags))
		{
			return false;
		}
		const uint64 SubTypesOffset = Reader.GetOffset();
		uint32 SubTypeCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(11), OutValue.OrderedSubTypes, SubTypeCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				SubTypesOffset, SubTypeCount, OutOffsets.SubTypes))
		{
			return false;
		}

		const EField NodeField = bHardValue
			? EField::HardValueTypeNode : EField::GlobalTypeNode;
		const EField KindField = bHardValue
			? EField::HardValueTypeKind : EField::GlobalTypeKind;
		const EField PrimitiveField = bHardValue
			? EField::HardValueTypePrimitive : EField::GlobalTypePrimitive;
		const EField PresenceField = bHardValue
			? EField::HardValueTypeReferencePresence
			: EField::GlobalTypeReferencePresence;
		const EField QualifierField = bHardValue
			? EField::HardValueTypeQualifierFlags
			: EField::GlobalTypeQualifierFlags;
		const EField SubTypesField = bHardValue
			? EField::HardValueTypeOrderedSubTypes
			: EField::GlobalTypeOrderedSubTypes;
		const auto Entry = [PrimaryIndex, NodeOrdinal](
			const EField Field, const uint64 Offset)
		{
			return FAngelscriptDecodedCacheRecord::
				FModuleStateCapturedOffsetStorage::FEntry{
					{Field, PrimaryIndex, NodeOrdinal, MAX_uint32}, Offset};
		};
		OutOffsets.Fields[0] = Entry(NodeField, NodeOffset);
		OutOffsets.Fields[1] = Entry(KindField, NodeOffset);
		OutOffsets.Fields[2] = Entry(PrimitiveField, PrimitiveOffset);
		OutOffsets.Fields[3] = Entry(PresenceField, ReferencePresenceOffset);
		OutOffsets.Fields[4] = Entry(QualifierField, QualifierOffset);
		OutOffsets.Fields[5] = Entry(SubTypesField, SubTypesOffset);
		if (ReferenceTag == 1)
		{
			const EField ReferenceField = bHardValue
				? EField::HardValueTypeReference : EField::GlobalTypeReference;
			const EField ReferenceKindField = bHardValue
				? EField::HardValueTypeReferenceKind : EField::GlobalTypeReferenceKind;
			const EField StableKeyField = bHardValue
				? EField::HardValueTypeReferenceStableKey
				: EField::GlobalTypeReferenceStableKey;
			const EField ExpectedAbiField = bHardValue
				? EField::HardValueTypeReferenceExpectedAbi
				: EField::GlobalTypeReferenceExpectedAbi;
			OutOffsets.Reference = Entry(ReferenceField, ReferenceOffset);
			OutOffsets.ReferenceKind = Entry(ReferenceKindField, ReferenceOffset);
			OutOffsets.ReferenceStableKey = Entry(StableKeyField, ReferenceOffset + 1);
			OutOffsets.ReferenceExpectedAbi = Entry(ExpectedAbiField, ReferenceOffset + 33);
		}
		for (uint32 Index = 0; Index < SubTypeCount; ++Index)
		{
			FAngelscriptCachedDataType& SubType =
				OutValue.OrderedSubTypes.AddDefaulted_GetRef();
			auto& SubTypeOffsets = OutOffsets.SubTypes.AddDefaulted_GetRef();
			if (!ReadModuleStateDataType(
				Reader,
				SubType,
				SubTypeOffsets,
				PrimaryIndex,
				bHardValue,
				InOutNodeOrdinal,
				Depth + 1))
			{
				return false;
			}
		}
		return true;
	}

	bool FDecodedRecordCodecBridge::ReadModuleStateDependency(
		FReader& Reader,
		FAngelscriptCacheSemanticDependency& OutValue,
		FAngelscriptDecodedCacheRecord::FModuleStateCapturedOffsetStorage::
			FDependencyOffsets& OutOffsets,
		const bool bActionDependency,
		const uint32 PrimaryIndex,
		const uint32 SecondaryIndex)
	{
		using EField = EAngelscriptModuleStateCapturedField;
		const uint64 DependencyOffset = Reader.GetOffset();
		if (!ReadEnum(Reader, OutValue.Kind, 1, 12))
		{
			return false;
		}
		const uint64 TargetOffset = Reader.GetOffset();
		if (!Reader.ConsumeReference()
			|| !ReadEnum(Reader, OutValue.Target.Kind, 1, 9)
			|| !Reader.ReadHash(OutValue.Target.StableKey)
			|| !Reader.ReadHash(OutValue.Target.ExpectedAbi))
		{
			return false;
		}
		const uint64 PresenceOffset = Reader.GetOffset();
		uint8 PresenceTag = 0;
		if (!Reader.ReadUInt8(PresenceTag))
		{
			return false;
		}
		if (PresenceTag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag,
				PresenceOffset);
			return false;
		}
		uint64 ValueOffset = 0;
		if (PresenceTag == 1)
		{
			ValueOffset = Reader.GetOffset();
			FAngelscriptHash256 Hash;
			if (!Reader.ReadHash(Hash))
			{
				return false;
			}
			OutValue.ExpectedContentOrValue = Hash;
		}

		const EField RowField = bActionDependency
			? EField::InitializationActionDependency : EField::Dependency;
		const EField KindField = bActionDependency
			? EField::InitializationActionDependencyKind : EField::DependencyKind;
		const EField TargetField = bActionDependency
			? EField::InitializationActionDependencyTarget : EField::DependencyTarget;
		const EField ReferenceKindField = bActionDependency
			? EField::InitializationActionDependencyTargetReferenceKind
			: EField::DependencyTargetReferenceKind;
		const EField StableKeyField = bActionDependency
			? EField::InitializationActionDependencyTargetStableKey
			: EField::DependencyTargetStableKey;
		const EField ExpectedAbiField = bActionDependency
			? EField::InitializationActionDependencyTargetExpectedAbi
			: EField::DependencyTargetExpectedAbi;
		const EField PresenceField = bActionDependency
			? EField::InitializationActionDependencyExpectedContentOrValuePresence
			: EField::DependencyExpectedContentOrValuePresence;
		const EField ValueField = bActionDependency
			? EField::InitializationActionDependencyExpectedContentOrValue
			: EField::DependencyExpectedContentOrValue;
		const uint32 StoredSecondary = bActionDependency ? SecondaryIndex : MAX_uint32;
		const auto Entry = [PrimaryIndex, StoredSecondary](
			const EField Field, const uint64 Offset)
		{
			return FAngelscriptDecodedCacheRecord::
				FModuleStateCapturedOffsetStorage::FEntry{
					{Field, PrimaryIndex, StoredSecondary, MAX_uint32}, Offset};
		};
		OutOffsets.Fields[0] = Entry(RowField, DependencyOffset);
		OutOffsets.Fields[1] = Entry(KindField, DependencyOffset);
		OutOffsets.Fields[2] = Entry(TargetField, TargetOffset);
		OutOffsets.Fields[3] = Entry(ReferenceKindField, TargetOffset);
		OutOffsets.Fields[4] = Entry(StableKeyField, TargetOffset + 1);
		OutOffsets.Fields[5] = Entry(ExpectedAbiField, TargetOffset + 33);
		OutOffsets.Fields[6] = Entry(PresenceField, PresenceOffset);
		if (PresenceTag == 1)
		{
			OutOffsets.ExpectedContentOrValue = Entry(ValueField, ValueOffset);
		}
		return true;
	}

	FAngelscriptCacheValidationResult FDecodedRecordCodecBridge::TryDecodeModuleState(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
		FAngelscriptCachedModuleState& OutValue,
		FAngelscriptDecodedCacheRecord::FModuleStateCapturedOffsetStorage& OutOffsets)
	{
		OutValue = {};
		OutOffsets = {};
		if (const FAngelscriptCacheValidationResult BeginResult = BeginRead(
			Payload, Limits, Budget, EAngelscriptCacheRecordKind::ModuleState);
			!BeginResult.IsSuccess())
		{
			return BeginResult;
		}
		FReader Reader(
			Payload, Limits, Budget, EAngelscriptCacheRecordKind::ModuleState,
			ChargeSink);
		using EField = EAngelscriptModuleStateCapturedField;
		const auto SetHeader = [&OutOffsets](
			const int32 Slot,
			const EField Field,
			const uint64 Offset)
		{
			OutOffsets.HeaderOffsets[Slot] = {
				{Field, MAX_uint32, MAX_uint32, MAX_uint32}, Offset};
		};
		const uint64 PayloadSchemaVersionOffset = Reader.GetOffset();
		if (!Reader.ReadUInt32(OutValue.PayloadSchemaVersion))
		{
			return Reader.GetResult();
		}
		if (OutValue.PayloadSchemaVersion
			!= FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::PayloadDecode,
				PayloadSchemaVersionOffset);
		}
		SetHeader(0, EField::PayloadSchemaVersion, PayloadSchemaVersionOffset);
		const uint64 ModuleKeyOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.ModuleKey.Hash)) return Reader.GetResult();
		SetHeader(1, EField::ModuleKey, ModuleKeyOffset);
		const uint64 ProfileOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.Profile.Hash)) return Reader.GetResult();
		SetHeader(2, EField::Profile, ProfileOffset);
		const uint64 StateInputHashOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.StateInputHash)) return Reader.GetResult();
		SetHeader(3, EField::StateInputHash, StateInputHashOffset);


		const uint64 GlobalsOffset = Reader.GetOffset();
		uint32 GlobalCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(89), OutValue.OrderedGlobals, GlobalCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				GlobalsOffset, GlobalCount, OutOffsets.Globals))
		{
			return Reader.GetResult();
		}
		SetHeader(4, EField::OrderedGlobals, GlobalsOffset);
		for (uint32 Index = 0; Index < GlobalCount; ++Index)
		{
			FAngelscriptCachedGlobalSchema& Global =
				OutValue.OrderedGlobals.AddDefaulted_GetRef();
			auto& Offsets = OutOffsets.Globals.AddDefaulted_GetRef();
			const uint64 RowOffset = Reader.GetOffset();
			const uint64 OrdinalOffset = Reader.GetOffset();
			if (!Reader.ReadUInt32(Global.StorageOrdinal)) return Reader.GetResult();
			const uint64 KeyOffset = Reader.GetOffset();
			if (!Reader.ReadHash(Global.GlobalKey.Hash)) return Reader.GetResult();
			const uint64 NamespaceOffset = Reader.GetOffset();
			if (!Reader.ReadString(Global.CanonicalNamespace)) return Reader.GetResult();
			const uint64 NameOffset = Reader.GetOffset();
			if (!Reader.ReadString(Global.CanonicalName)) return Reader.GetResult();
			uint32 NodeOrdinal = 0;
			if (!ReadModuleStateDataType(
				Reader, Global.Type, Offsets.Type, Index, false, NodeOrdinal, 1))
			{
				return Reader.GetResult();
			}
			const uint64 TraitsOffset = Reader.GetOffset();
			if (!Reader.ReadUInt32(Global.GlobalTraitFlags)) return Reader.GetResult();
			const uint64 InitOffset = Reader.GetOffset();
			if (!ReadEnum(Reader, Global.InitializationKind, 1, 3))
			{
				return Reader.GetResult();
			}
			const uint64 CleanupOffset = Reader.GetOffset();
			if (!ReadEnum(Reader, Global.CleanupPolicy, 1, 3))
			{
				return Reader.GetResult();
			}
			const uint64 FingerprintOffset = Reader.GetOffset();
			if (!Reader.ReadHash(Global.StorageLayoutFingerprint))
			{
				return Reader.GetResult();
			}
			const auto Entry = [Index](const EField Field, const uint64 Offset)
			{
				return FAngelscriptDecodedCacheRecord::
					FModuleStateCapturedOffsetStorage::FEntry{
						{Field, Index, MAX_uint32, MAX_uint32}, Offset};
			};
			Offsets.Fields[0] = Entry(EField::Global, RowOffset);
			Offsets.Fields[1] = Entry(EField::GlobalStorageOrdinal, OrdinalOffset);
			Offsets.Fields[2] = Entry(EField::GlobalKey, KeyOffset);
			Offsets.Fields[3] = Entry(EField::GlobalCanonicalNamespace, NamespaceOffset);
			Offsets.Fields[4] = Entry(EField::GlobalCanonicalName, NameOffset);
			Offsets.Fields[5] = Entry(EField::GlobalTraitFlags, TraitsOffset);
			Offsets.Fields[6] = Entry(EField::GlobalInitializationKind, InitOffset);
			Offsets.Fields[7] = Entry(EField::GlobalCleanupPolicy, CleanupOffset);
			Offsets.Fields[8] = Entry(
				EField::GlobalStorageLayoutFingerprint, FingerprintOffset);
		}

		const uint64 HardValuesOffset = Reader.GetOffset();
		uint32 HardValueCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(110), OutValue.HardValues, HardValueCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				HardValuesOffset, HardValueCount, OutOffsets.HardValues))
		{
			return Reader.GetResult();
		}
		SetHeader(5, EField::HardValues, HardValuesOffset);
		for (uint32 Index = 0; Index < HardValueCount; ++Index)
		{
			FAngelscriptCachedHardValue& HardValue =
				OutValue.HardValues.AddDefaulted_GetRef();
			auto& Offsets = OutOffsets.HardValues.AddDefaulted_GetRef();
			const uint64 RowOffset = Reader.GetOffset();
			if (!ReadEnum(Reader, HardValue.HardValueKind, 1, 2))
			{
				return Reader.GetResult();
			}
			const uint64 OwnerOffset = Reader.GetOffset();
			if (!Reader.ConsumeReference()
				|| !ReadEnum(Reader, HardValue.Owner.Kind, 1, 9)
				|| !Reader.ReadHash(HardValue.Owner.StableKey)
				|| !Reader.ReadHash(HardValue.Owner.ExpectedAbi))
			{
				return Reader.GetResult();
			}
			uint32 NodeOrdinal = 0;
			if (!ReadModuleStateDataType(
				Reader, HardValue.Type, Offsets.Type, Index, true, NodeOrdinal, 1))
			{
				return Reader.GetResult();
			}
			const uint64 CanonicalPresenceOffset = Reader.GetOffset();
			uint8 CanonicalTag = 0;
			if (!Reader.ReadUInt8(CanonicalTag)) return Reader.GetResult();
			if (CanonicalTag > 1)
			{
				Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag,
					CanonicalPresenceOffset);
				return Reader.GetResult();
			}
			if (CanonicalTag == 1)
			{
				const uint64 CanonicalOffset = Reader.GetOffset();
				FAngelscriptCachedCanonicalValue Canonical;
				if (!ReadEnum(Reader, Canonical.ValueKind, 1, 6))
				{
					return Reader.GetResult();
				}
				const uint64 BytesOffset = Reader.GetOffset();
				uint64 ByteCount = 0;
				if (!Reader.ReadByteArrayCountAndReserve(
					Canonical.FixedWidthValueBytes, ByteCount))
				{
					return Reader.GetResult();
				}
				for (uint64 ByteIndex = 0; ByteIndex < ByteCount; ++ByteIndex)
				{
					uint8 Byte = 0;
					if (!Reader.ReadUInt8(Byte)) return Reader.GetResult();
					Canonical.FixedWidthValueBytes.Add(Byte);
				}
				HardValue.CanonicalValue = MoveTemp(Canonical);
				const auto OptionalEntry = [Index](
					const EField Field, const uint64 Offset)
				{
					return FAngelscriptDecodedCacheRecord::
						FModuleStateCapturedOffsetStorage::FEntry{
							{Field, Index, MAX_uint32, MAX_uint32}, Offset};
				};
				Offsets.CanonicalValue = OptionalEntry(
					EField::HardValueCanonicalValue, CanonicalOffset);
				Offsets.CanonicalValueKind = OptionalEntry(
					EField::HardValueCanonicalValueKind, CanonicalOffset);
				Offsets.CanonicalValueBytes = OptionalEntry(
					EField::HardValueCanonicalValueFixedWidthValueBytes, BytesOffset);
			}
			const uint64 HashOffset = Reader.GetOffset();
			if (!Reader.ReadHash(HardValue.HardValueHash)) return Reader.GetResult();
			const auto Entry = [Index](const EField Field, const uint64 Offset)
			{
				return FAngelscriptDecodedCacheRecord::
					FModuleStateCapturedOffsetStorage::FEntry{
						{Field, Index, MAX_uint32, MAX_uint32}, Offset};
			};
			Offsets.Fields[0] = Entry(EField::HardValue, RowOffset);
			Offsets.Fields[1] = Entry(EField::HardValueKind, RowOffset);
			Offsets.Fields[2] = Entry(EField::HardValueOwner, OwnerOffset);
			Offsets.Fields[3] = Entry(EField::HardValueOwnerReferenceKind, OwnerOffset);
			Offsets.Fields[4] = Entry(EField::HardValueOwnerStableKey, OwnerOffset + 1);
			Offsets.Fields[5] = Entry(EField::HardValueOwnerExpectedAbi, OwnerOffset + 33);
			Offsets.Fields[6] = Entry(
				EField::HardValueCanonicalValuePresence, CanonicalPresenceOffset);
			Offsets.Fields[7] = Entry(EField::HardValueHash, HashOffset);
		}

		const uint64 InitializersOffset = Reader.GetOffset();
		uint32 InitializerCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(78), OutValue.Initializers, InitializerCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				InitializersOffset, InitializerCount, OutOffsets.Initializers))
		{
			return Reader.GetResult();
		}
		SetHeader(6, EField::Initializers, InitializersOffset);
		for (uint32 Index = 0; Index < InitializerCount; ++Index)
		{
			FAngelscriptCachedInitializerUnit& Initializer =
				OutValue.Initializers.AddDefaulted_GetRef();
			auto& Offsets = OutOffsets.Initializers.AddDefaulted_GetRef();
			const uint64 RowOffset = Reader.GetOffset();
			if (!ReadEnum(Reader, Initializer.InitializerKind, 1, 2))
			{
				return Reader.GetResult();
			}
			const uint64 KeyOffset = Reader.GetOffset();
			if (!Reader.ReadHash(Initializer.InitializerKey.Hash)) return Reader.GetResult();
			const uint64 OwnerPresenceOffset = Reader.GetOffset();
			uint8 OwnerTag = 0;
			if (!Reader.ReadUInt8(OwnerTag)) return Reader.GetResult();
			if (OwnerTag > 1)
			{
				Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag,
					OwnerPresenceOffset);
				return Reader.GetResult();
			}
			if (OwnerTag == 1)
			{
				const uint64 OwnerGlobalOffset = Reader.GetOffset();
				FAngelscriptStableGlobalKey OwnerGlobal;
				if (!Reader.ReadHash(OwnerGlobal.Hash)) return Reader.GetResult();
				Initializer.OwnerGlobal = OwnerGlobal;
				Offsets.OwnerGlobal = FAngelscriptDecodedCacheRecord::
					FModuleStateCapturedOffsetStorage::FEntry{
						{EField::InitializerOwnerGlobal, Index, MAX_uint32, MAX_uint32},
						OwnerGlobalOffset};
			}
			const uint64 CodecOffset = Reader.GetOffset();
			if (!Reader.ReadUInt32(Initializer.VmInitializerCodecVersion))
			{
				return Reader.GetResult();
			}
			const uint64 HashOffset = Reader.GetOffset();
			if (!Reader.ReadHash(Initializer.InitializerExecutionHash))
			{
				return Reader.GetResult();
			}
			const uint64 PayloadOffset = Reader.GetOffset();
			uint64 ByteCount = 0;
			if (!Reader.ReadByteArrayCountAndReserve(
				Initializer.CanonicalExecutionPayload, ByteCount))
			{
				return Reader.GetResult();
			}
			for (uint64 ByteIndex = 0; ByteIndex < ByteCount; ++ByteIndex)
			{
				uint8 Byte = 0;
				if (!Reader.ReadUInt8(Byte)) return Reader.GetResult();
				Initializer.CanonicalExecutionPayload.Add(Byte);
			}
			const auto Entry = [Index](const EField Field, const uint64 Offset)
			{
				return FAngelscriptDecodedCacheRecord::
					FModuleStateCapturedOffsetStorage::FEntry{
						{Field, Index, MAX_uint32, MAX_uint32}, Offset};
			};
			Offsets.Fields[0] = Entry(EField::Initializer, RowOffset);
			Offsets.Fields[1] = Entry(EField::InitializerKind, RowOffset);
			Offsets.Fields[2] = Entry(EField::InitializerKey, KeyOffset);
			Offsets.Fields[3] = Entry(
				EField::InitializerOwnerGlobalPresence, OwnerPresenceOffset);
			Offsets.Fields[4] = Entry(
				EField::InitializerVmInitializerCodecVersion, CodecOffset);
			Offsets.Fields[5] = Entry(EField::InitializerExecutionHash, HashOffset);
			Offsets.Fields[6] = Entry(
				EField::InitializerCanonicalExecutionPayload, PayloadOffset);
		}

		const uint64 ActionsOffset = Reader.GetOffset();
		uint32 ActionCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(74), OutValue.OrderedInitializationActions, ActionCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				ActionsOffset, ActionCount, OutOffsets.InitializationActions))
		{
			return Reader.GetResult();
		}
		SetHeader(7, EField::OrderedInitializationActions, ActionsOffset);
		for (uint32 Index = 0; Index < ActionCount; ++Index)
		{
			FAngelscriptCachedInitializationAction& Action =
				OutValue.OrderedInitializationActions.AddDefaulted_GetRef();
			auto& Offsets = OutOffsets.InitializationActions.AddDefaulted_GetRef();
			const uint64 RowOffset = Reader.GetOffset();
			const uint64 OrdinalOffset = Reader.GetOffset();
			if (!Reader.ReadUInt32(Action.ActionOrdinal)) return Reader.GetResult();
			const uint64 KindOffset = Reader.GetOffset();
			if (!ReadEnum(Reader, Action.ActionKind, 1, 2)) return Reader.GetResult();
			const uint64 TargetOffset = Reader.GetOffset();
			if (!Reader.ConsumeReference()
				|| !ReadEnum(Reader, Action.Target.Kind, 1, 9)
				|| !Reader.ReadHash(Action.Target.StableKey)
				|| !Reader.ReadHash(Action.Target.ExpectedAbi))
			{
				return Reader.GetResult();
			}
			const uint64 DependenciesOffset = Reader.GetOffset();
			uint32 DependencyCount = 0;
			if (!Reader.ReadArrayCountAndReserve(
				UINT64_C(67), Action.Dependencies, DependencyCount)
				|| !Reader.ReserveDecodedArrayAtOffset(
					DependenciesOffset, DependencyCount, Offsets.Dependencies))
			{
				return Reader.GetResult();
			}
			const auto Entry = [Index](const EField Field, const uint64 Offset)
			{
				return FAngelscriptDecodedCacheRecord::
					FModuleStateCapturedOffsetStorage::FEntry{
						{Field, Index, MAX_uint32, MAX_uint32}, Offset};
			};
			Offsets.Fields[0] = Entry(EField::InitializationAction, RowOffset);
			Offsets.Fields[1] = Entry(EField::InitializationActionOrdinal, OrdinalOffset);
			Offsets.Fields[2] = Entry(EField::InitializationActionKind, KindOffset);
			Offsets.Fields[3] = Entry(EField::InitializationActionTarget, TargetOffset);
			Offsets.Fields[4] = Entry(
				EField::InitializationActionTargetReferenceKind, TargetOffset);
			Offsets.Fields[5] = Entry(
				EField::InitializationActionTargetStableKey, TargetOffset + 1);
			Offsets.Fields[6] = Entry(
				EField::InitializationActionTargetExpectedAbi, TargetOffset + 33);
			Offsets.Fields[7] = Entry(
				EField::InitializationActionDependencies, DependenciesOffset);
			for (uint32 DependencyIndex = 0;
				DependencyIndex < DependencyCount; ++DependencyIndex)
			{
				auto& Dependency = Action.Dependencies.AddDefaulted_GetRef();
				auto& DependencyOffsets = Offsets.Dependencies.AddDefaulted_GetRef();
				if (!ReadModuleStateDependency(
					Reader,
					Dependency,
					DependencyOffsets,
					true,
					Index,
					DependencyIndex))
				{
					return Reader.GetResult();
				}
			}
		}

		const uint64 PostInitOffset = Reader.GetOffset();
		uint32 PostInitCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(69), OutValue.OrderedPostInitFunctions, PostInitCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				PostInitOffset, PostInitCount, OutOffsets.PostInitFunctions))
		{
			return Reader.GetResult();
		}
		SetHeader(8, EField::OrderedPostInitFunctions, PostInitOffset);
		for (uint32 Index = 0; Index < PostInitCount; ++Index)
		{
			FAngelscriptCachedPostInitFunction& PostInit =
				OutValue.OrderedPostInitFunctions.AddDefaulted_GetRef();
			auto& Offsets = OutOffsets.PostInitFunctions.AddDefaulted_GetRef();
			const uint64 RowOffset = Reader.GetOffset();
			const uint64 OrdinalOffset = Reader.GetOffset();
			if (!Reader.ReadUInt32(PostInit.PostInitOrdinal)) return Reader.GetResult();
			const uint64 ReferenceOffset = Reader.GetOffset();
			if (!Reader.ConsumeReference()
				|| !ReadEnum(Reader, PostInit.Function.Kind, 1, 9)
				|| !Reader.ReadHash(PostInit.Function.StableKey)
				|| !Reader.ReadHash(PostInit.Function.ExpectedAbi))
			{
				return Reader.GetResult();
			}
			const auto Entry = [Index](const EField Field, const uint64 Offset)
			{
				return FAngelscriptDecodedCacheRecord::
					FModuleStateCapturedOffsetStorage::FEntry{
						{Field, Index, MAX_uint32, MAX_uint32}, Offset};
			};
			Offsets.Fields[0] = Entry(EField::PostInitFunction, RowOffset);
			Offsets.Fields[1] = Entry(EField::PostInitOrdinal, OrdinalOffset);
			Offsets.Fields[2] = Entry(EField::PostInitFunctionReference, ReferenceOffset);
			Offsets.Fields[3] = Entry(
				EField::PostInitFunctionReferenceKind, ReferenceOffset);
			Offsets.Fields[4] = Entry(EField::PostInitFunctionStableKey, ReferenceOffset + 1);
			Offsets.Fields[5] = Entry(EField::PostInitFunctionExpectedAbi, ReferenceOffset + 33);
		}

		const uint64 DependenciesOffset = Reader.GetOffset();
		uint32 DependencyCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(67), OutValue.Dependencies, DependencyCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				DependenciesOffset, DependencyCount, OutOffsets.Dependencies))
		{
			return Reader.GetResult();
		}
		SetHeader(9, EField::Dependencies, DependenciesOffset);
		for (uint32 Index = 0; Index < DependencyCount; ++Index)
		{
			auto& Dependency = OutValue.Dependencies.AddDefaulted_GetRef();
			auto& DependencyOffsets = OutOffsets.Dependencies.AddDefaulted_GetRef();
			if (!ReadModuleStateDependency(
				Reader,
				Dependency,
				DependencyOffsets,
				false,
				Index,
				MAX_uint32))
			{
				return Reader.GetResult();
			}
		}
		if (!Reader.IsAtEnd())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::TrailingData,
				EAngelscriptCacheRecordKind::ModuleState,
				EAngelscriptCacheValidationStage::PayloadDecode,
				Reader.GetOffset());
		}

		const auto GetOffset = [&OutOffsets](
			const EField Field,
			const int32 PrimaryIndex,
			const int32 SecondaryIndex) -> uint64
		{
			const uint32 Primary = PrimaryIndex == INDEX_NONE
				? MAX_uint32 : static_cast<uint32>(PrimaryIndex);
			const uint32 Secondary = SecondaryIndex == INDEX_NONE
				? MAX_uint32 : static_cast<uint32>(SecondaryIndex);
			const auto Matches = [Field, Primary, Secondary](const auto& Entry)
			{
				return Entry.Coordinate.Field == Field
					&& Entry.Coordinate.PrimaryIndex == Primary
					&& Entry.Coordinate.SecondaryIndex == Secondary;
			};
			const auto FindEntries = [&Matches](const auto& Entries) -> TOptional<uint64>
			{
				for (const auto& Entry : Entries)
				{
					if (Matches(Entry)) return Entry.Offset;
				}
				return {};
			};
			const auto FindOptional = [&Matches](const auto& Entry) -> TOptional<uint64>
			{
				return Entry.IsSet() && Matches(Entry.GetValue())
					? TOptional<uint64>{Entry->Offset} : TOptional<uint64>{};
			};
			const auto FindType = [&FindEntries, &FindOptional](
				const auto& Self, const auto& Type) -> TOptional<uint64>
			{
				if (const auto Found = FindEntries(Type.Fields); Found.IsSet()) return Found;
				const TOptional<FAngelscriptDecodedCacheRecord::
					FModuleStateCapturedOffsetStorage::FEntry> Optionals[] = {
					Type.Reference,
					Type.ReferenceKind,
					Type.ReferenceStableKey,
					Type.ReferenceExpectedAbi,
				};
				for (const auto& Optional : Optionals)
				{
					if (const auto Found = FindOptional(Optional); Found.IsSet()) return Found;
				}
				for (const auto& SubType : Type.SubTypes)
				{
					if (const auto Found = Self(Self, SubType); Found.IsSet()) return Found;
				}
				return {};
			};
			if (const auto Found = FindEntries(OutOffsets.HeaderOffsets); Found.IsSet())
			{
				return Found.GetValue();
			}
			for (const auto& Global : OutOffsets.Globals)
			{
				if (const auto Found = FindEntries(Global.Fields); Found.IsSet())
					return Found.GetValue();
				if (const auto Found = FindType(FindType, Global.Type); Found.IsSet())
					return Found.GetValue();
			}
			for (const auto& HardValue : OutOffsets.HardValues)
			{
				if (const auto Found = FindEntries(HardValue.Fields); Found.IsSet())
					return Found.GetValue();
				if (const auto Found = FindType(FindType, HardValue.Type); Found.IsSet())
					return Found.GetValue();
				const TOptional<FAngelscriptDecodedCacheRecord::
					FModuleStateCapturedOffsetStorage::FEntry> Optionals[] = {
					HardValue.CanonicalValue,
					HardValue.CanonicalValueKind,
					HardValue.CanonicalValueBytes,
				};
				for (const auto& Optional : Optionals)
				{
					if (const auto Found = FindOptional(Optional); Found.IsSet())
						return Found.GetValue();
				}
			}
			for (const auto& Initializer : OutOffsets.Initializers)
			{
				if (const auto Found = FindEntries(Initializer.Fields); Found.IsSet())
					return Found.GetValue();
				if (const auto Found = FindOptional(Initializer.OwnerGlobal); Found.IsSet())
					return Found.GetValue();
			}
			for (const auto& Action : OutOffsets.InitializationActions)
			{
				if (const auto Found = FindEntries(Action.Fields); Found.IsSet())
					return Found.GetValue();
				for (const auto& Dependency : Action.Dependencies)
				{
					if (const auto Found = FindEntries(Dependency.Fields); Found.IsSet())
						return Found.GetValue();
					if (const auto Found = FindOptional(
						Dependency.ExpectedContentOrValue); Found.IsSet())
						return Found.GetValue();
				}
			}
			for (const auto& PostInit : OutOffsets.PostInitFunctions)
			{
				if (const auto Found = FindEntries(PostInit.Fields); Found.IsSet())
					return Found.GetValue();
			}
			for (const auto& Dependency : OutOffsets.Dependencies)
			{
				if (const auto Found = FindEntries(Dependency.Fields); Found.IsSet())
					return Found.GetValue();
				if (const auto Found = FindOptional(
					Dependency.ExpectedContentOrValue); Found.IsSet())
					return Found.GetValue();
			}
			return UINT64_C(0);
		};
		return ValidateModuleState(OutValue, GetOffset);
	}

	FAngelscriptCacheValidationResult FDecodedRecordCodecBridge::TryDecodeDebugSidecar(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
		FAngelscriptCachedDebugSidecar& OutValue,
		FAngelscriptDecodedCacheRecord::TSingleCapturedOffsetStorage<
			FAngelscriptDebugSidecarFieldCoordinate>& OutOffsets)
	{
		OutValue = {};
		OutOffsets.Entries.Reset();
		if (const FAngelscriptCacheValidationResult BeginResult = BeginRead(
			Payload,
			Limits,
			Budget,
			EAngelscriptCacheRecordKind::DebugSidecar);
			!BeginResult.IsSuccess())
		{
			return BeginResult;
		}

		FReader Reader(
			Payload,
			Limits,
			Budget,
			EAngelscriptCacheRecordKind::DebugSidecar,
			ChargeSink);
		const uint64 PayloadSchemaVersionOffset = Reader.GetOffset();
		if (!Reader.ReadUInt32(OutValue.PayloadSchemaVersion))
		{
			return Reader.GetResult();
		}
		const uint64 FunctionKeyOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.FunctionKey.Hash))
		{
			return Reader.GetResult();
		}
		const uint64 ProfileOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.Profile.Hash))
		{
			return Reader.GetResult();
		}
		const uint64 DebugHashOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.DebugHash))
		{
			return Reader.GetResult();
		}
		const uint64 VmDebugCodecVersionOffset = Reader.GetOffset();
		if (!Reader.ReadUInt32(OutValue.VmDebugCodecVersion))
		{
			return Reader.GetResult();
		}
		const uint64 SourcesOffset = Reader.GetOffset();
		uint32 SourceCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(68), OutValue.Sources, SourceCount))
		{
			return Reader.GetResult();
		}
		const uint64 OffsetEntryCount = UINT64_C(7) + UINT64_C(4) * SourceCount;
		if (!Reader.ReserveDecodedArrayAtOffset(
			SourcesOffset, OffsetEntryCount, OutOffsets.Entries))
		{
			return Reader.GetResult();
		}
		const auto AddOffset = [&OutOffsets](
			const EAngelscriptDebugSidecarCapturedField Field,
			const uint64 Offset,
			const uint32 PrimaryIndex = MAX_uint32)
		{
			OutOffsets.Entries.Add({
				{Field, PrimaryIndex, MAX_uint32, MAX_uint32}, Offset});
		};
		AddOffset(EAngelscriptDebugSidecarCapturedField::PayloadSchemaVersion,
			PayloadSchemaVersionOffset);
		AddOffset(EAngelscriptDebugSidecarCapturedField::FunctionKey, FunctionKeyOffset);
		AddOffset(EAngelscriptDebugSidecarCapturedField::Profile, ProfileOffset);
		AddOffset(EAngelscriptDebugSidecarCapturedField::DebugHash, DebugHashOffset);
		AddOffset(EAngelscriptDebugSidecarCapturedField::VmDebugCodecVersion,
			VmDebugCodecVersionOffset);
		AddOffset(EAngelscriptDebugSidecarCapturedField::Sources, SourcesOffset);

		for (uint32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
		{
			FAngelscriptCachedDebugSourceReference& Source = OutValue.Sources.AddDefaulted_GetRef();
			const uint64 SourceOffset = Reader.GetOffset();
			const uint64 SourceFileKeyOffset = Reader.GetOffset();
			if (!Reader.ReadHash(Source.SourceFileKey.Hash))
			{
				return Reader.GetResult();
			}
			const uint64 LogicalSectionKeyOffset = Reader.GetOffset();
			if (!Reader.ReadHash(Source.LogicalSectionKey.Hash))
			{
				return Reader.GetResult();
			}
			const uint64 CanonicalLogicalSectionOffset = Reader.GetOffset();
			if (!Reader.ReadString(Source.CanonicalLogicalSection))
			{
				return Reader.GetResult();
			}
			AddOffset(EAngelscriptDebugSidecarCapturedField::Source,
				SourceOffset, SourceIndex);
			AddOffset(EAngelscriptDebugSidecarCapturedField::SourceFileKey,
				SourceFileKeyOffset, SourceIndex);
			AddOffset(EAngelscriptDebugSidecarCapturedField::SourceLogicalSectionKey,
				LogicalSectionKeyOffset, SourceIndex);
			AddOffset(EAngelscriptDebugSidecarCapturedField::SourceCanonicalLogicalSection,
				CanonicalLogicalSectionOffset, SourceIndex);
		}

		const uint64 CanonicalDebugPayloadOffset = Reader.GetOffset();
		uint64 DebugPayloadCount = 0;
		if (!Reader.ReadByteArrayCountAndReserve(
			OutValue.CanonicalDebugPayload, DebugPayloadCount))
		{
			return Reader.GetResult();
		}
		for (uint64 ByteIndex = 0; ByteIndex < DebugPayloadCount; ++ByteIndex)
		{
			uint8 Byte = 0;
			if (!Reader.ReadUInt8(Byte))
			{
				return Reader.GetResult();
			}
			OutValue.CanonicalDebugPayload.Add(Byte);
		}
		AddOffset(EAngelscriptDebugSidecarCapturedField::CanonicalDebugPayload,
			CanonicalDebugPayloadOffset);
		if (!Reader.IsAtEnd())
		{
			return Failure(
				EAngelscriptCacheValidationError::TrailingData,
				EAngelscriptCacheValidationStage::PayloadDecode,
				Reader.GetOffset());
		}

		return ValidateDebugSidecar(
			OutValue,
			[&OutOffsets](const int32 SourceIndex, const int32 Component)
			{
				return OutOffsets.Entries[6 + SourceIndex * 4 + Component].Offset;
			},
			PayloadSchemaVersionOffset,
			FunctionKeyOffset,
			ProfileOffset,
			DebugHashOffset,
			VmDebugCodecVersionOffset);
	}

	FAngelscriptCacheValidationResult FDecodedRecordCodecBridge::TryDecodeFunctionBody(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
		FAngelscriptCachedFunctionBody& OutValue,
		FAngelscriptDecodedCacheRecord::FFunctionBodyCapturedOffsetStorage& OutOffsets)
	{
		OutValue = {};
		OutOffsets = {};
		if (const FAngelscriptCacheValidationResult BeginResult = BeginRead(
			Payload,
			Limits,
			Budget,
			EAngelscriptCacheRecordKind::FunctionBody);
			!BeginResult.IsSuccess())
		{
			return BeginResult;
		}

		FReader Reader(
			Payload,
			Limits,
			Budget,
			EAngelscriptCacheRecordKind::FunctionBody,
			ChargeSink);
		using EField = EAngelscriptFunctionBodyCapturedField;
		const auto SetHeader = [&OutOffsets](
			const int32 Slot,
			const EField Field,
			const uint64 Offset)
		{
			OutOffsets.HeaderOffsets[Slot] = {
				{Field, MAX_uint32, MAX_uint32, MAX_uint32}, Offset};
		};

		const uint64 PayloadSchemaVersionOffset = Reader.GetOffset();
		if (!Reader.ReadUInt32(OutValue.PayloadSchemaVersion))
		{
			return Reader.GetResult();
		}
		if (OutValue.PayloadSchemaVersion
			!= FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion)
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
				EAngelscriptCacheRecordKind::FunctionBody,
				EAngelscriptCacheValidationStage::PayloadDecode,
				PayloadSchemaVersionOffset);
		}
		SetHeader(0, EField::PayloadSchemaVersion, PayloadSchemaVersionOffset);

		const uint64 ModuleKeyOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.ModuleKey.Hash))
		{
			return Reader.GetResult();
		}
		SetHeader(1, EField::ModuleKey, ModuleKeyOffset);

		const uint64 IdentityOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.Identity.FunctionKey.Hash))
		{
			return Reader.GetResult();
		}
		const uint64 ContentOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.Identity.Content.Execution))
		{
			return Reader.GetResult();
		}
		const uint64 DebugHashOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.Identity.Content.Debug))
		{
			return Reader.GetResult();
		}
		const uint64 ProfileOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.Identity.Profile.Hash))
		{
			return Reader.GetResult();
		}
		SetHeader(2, EField::Identity, IdentityOffset);
		SetHeader(3, EField::IdentityFunctionKey, IdentityOffset);
		SetHeader(4, EField::IdentityContent, ContentOffset);
		SetHeader(5, EField::IdentityContentExecution, ContentOffset);
		SetHeader(6, EField::IdentityContentDebug, DebugHashOffset);
		SetHeader(7, EField::IdentityProfile, ProfileOffset);

		const uint64 ExpectedDeclarationAbiOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.ExpectedDeclarationAbi))
		{
			return Reader.GetResult();
		}
		SetHeader(8, EField::ExpectedDeclarationAbi, ExpectedDeclarationAbiOffset);
		const uint64 FunctionSourceDigestOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.FunctionSourceDigest.Hash))
		{
			return Reader.GetResult();
		}
		SetHeader(9, EField::FunctionSourceDigest, FunctionSourceDigestOffset);
		const uint64 FunctionInputDigestOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.FunctionInputDigest.Hash))
		{
			return Reader.GetResult();
		}
		SetHeader(10, EField::FunctionInputDigest, FunctionInputDigestOffset);

		const uint64 InvocationKindOffset = Reader.GetOffset();
		if (!ReadEnum(Reader, OutValue.InvocationKind, 1, 10))
		{
			return Reader.GetResult();
		}
		SetHeader(11, EField::InvocationKind, InvocationKindOffset);
		const uint64 VmExecutionCodecVersionOffset = Reader.GetOffset();
		if (!Reader.ReadUInt32(OutValue.VmExecutionCodecVersion))
		{
			return Reader.GetResult();
		}
		SetHeader(12, EField::VmExecutionCodecVersion, VmExecutionCodecVersionOffset);

		const uint64 CanonicalExecutionPayloadOffset = Reader.GetOffset();
		uint64 ExecutionPayloadCount = 0;
		if (!Reader.ReadByteArrayCountAndReserve(
			OutValue.CanonicalExecutionPayload, ExecutionPayloadCount))
		{
			return Reader.GetResult();
		}
		for (uint64 Index = 0; Index < ExecutionPayloadCount; ++Index)
		{
			uint8 Byte = 0;
			if (!Reader.ReadUInt8(Byte))
			{
				return Reader.GetResult();
			}
			OutValue.CanonicalExecutionPayload.Add(Byte);
		}
		SetHeader(13, EField::CanonicalExecutionPayload,
			CanonicalExecutionPayloadOffset);

		const uint64 ActualDependenciesOffset = Reader.GetOffset();
		uint32 DependencyCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(67), OutValue.ActualDependencies, DependencyCount))
		{
			return Reader.GetResult();
		}
		SetHeader(14, EField::ActualDependencies, ActualDependenciesOffset);
		if (!Reader.ReserveDecodedArrayAtOffset(
			ActualDependenciesOffset,
			UINT64_C(7) * DependencyCount,
			OutOffsets.DependencyOffsets)
			|| !Reader.ReserveDecodedArrayAtOffset(
				ActualDependenciesOffset,
				DependencyCount,
				OutOffsets.DependencyExpectedValueOffsets))
		{
			return Reader.GetResult();
		}
		for (uint32 Index = 0; Index < DependencyCount; ++Index)
		{
			FAngelscriptCacheSemanticDependency& Dependency =
				OutValue.ActualDependencies.AddDefaulted_GetRef();
			TOptional<FAngelscriptDecodedCacheRecord::
				FFunctionBodyCapturedOffsetStorage::FEntry>& ExpectedValueEntry =
				OutOffsets.DependencyExpectedValueOffsets.AddDefaulted_GetRef();
			const uint64 DependencyOffset = Reader.GetOffset();
			if (!ReadEnum(Reader, Dependency.Kind, 1, 12))
			{
				return Reader.GetResult();
			}
			const uint64 TargetOffset = Reader.GetOffset();
			if (!Reader.ConsumeReference()
				|| !ReadEnum(Reader, Dependency.Target.Kind, 1, 9)
				|| !Reader.ReadHash(Dependency.Target.StableKey)
				|| !Reader.ReadHash(Dependency.Target.ExpectedAbi))
			{
				return Reader.GetResult();
			}
			const uint64 ExpectedValuePresenceOffset = Reader.GetOffset();
			uint8 ExpectedValueTag = 0;
			if (!Reader.ReadUInt8(ExpectedValueTag))
			{
				return Reader.GetResult();
			}
			if (ExpectedValueTag > 1)
			{
				Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag,
					ExpectedValuePresenceOffset);
				return Reader.GetResult();
			}
			if (ExpectedValueTag == 1)
			{
				const uint64 ExpectedValueOffset = Reader.GetOffset();
				FAngelscriptHash256 ExpectedValue;
				if (!Reader.ReadHash(ExpectedValue))
				{
					return Reader.GetResult();
				}
				Dependency.ExpectedContentOrValue = ExpectedValue;
				ExpectedValueEntry = FAngelscriptDecodedCacheRecord::
					FFunctionBodyCapturedOffsetStorage::FEntry{
						{EField::ActualDependencyExpectedContentOrValue,
							Index, MAX_uint32, MAX_uint32},
						ExpectedValueOffset};
			}

			const auto AddDependencyOffset = [
				&OutOffsets, Index](const EField Field, const uint64 Offset)
			{
				OutOffsets.DependencyOffsets.Add({
					{Field, Index, MAX_uint32, MAX_uint32}, Offset});
			};
			AddDependencyOffset(EField::ActualDependency, DependencyOffset);
			AddDependencyOffset(EField::ActualDependencyKind, DependencyOffset);
			AddDependencyOffset(EField::ActualDependencyTarget, TargetOffset);
			AddDependencyOffset(
				EField::ActualDependencyTargetReferenceKind, TargetOffset);
			AddDependencyOffset(
				EField::ActualDependencyTargetStableKey, TargetOffset + 1);
			AddDependencyOffset(
				EField::ActualDependencyTargetExpectedAbi, TargetOffset + 33);
			AddDependencyOffset(
				EField::ActualDependencyExpectedContentOrValuePresence,
				ExpectedValuePresenceOffset);
		}

		const uint64 DebugSidecarPresenceOffset = Reader.GetOffset();
		uint8 DebugSidecarTag = 0;
		if (!Reader.ReadUInt8(DebugSidecarTag))
		{
			return Reader.GetResult();
		}
		if (DebugSidecarTag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag,
				DebugSidecarPresenceOffset);
			return Reader.GetResult();
		}
		SetHeader(15, EField::DebugSidecarPresence, DebugSidecarPresenceOffset);
		if (DebugSidecarTag == 1)
		{
			const uint64 DebugSidecarOffset = Reader.GetOffset();
			FAngelscriptCacheRecordId RecordId;
			if (!ReadRecordId(Reader, RecordId))
			{
				return Reader.GetResult();
			}
			OutValue.DebugSidecar = RecordId;
			OutOffsets.DebugSidecarOffset =
				FAngelscriptDecodedCacheRecord::
					FFunctionBodyCapturedOffsetStorage::FEntry{
						{EField::DebugSidecar, MAX_uint32, MAX_uint32, MAX_uint32},
						DebugSidecarOffset};
			OutOffsets.DebugSidecarKindOffset =
				FAngelscriptDecodedCacheRecord::
					FFunctionBodyCapturedOffsetStorage::FEntry{
						{EField::DebugSidecarKind,
							MAX_uint32, MAX_uint32, MAX_uint32},
						DebugSidecarOffset};
			OutOffsets.DebugSidecarContentHashOffset =
				FAngelscriptDecodedCacheRecord::
					FFunctionBodyCapturedOffsetStorage::FEntry{
						{EField::DebugSidecarContentHash,
							MAX_uint32, MAX_uint32, MAX_uint32},
						DebugSidecarOffset + 1};
		}

		if (!Reader.IsAtEnd())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::TrailingData,
				EAngelscriptCacheRecordKind::FunctionBody,
				EAngelscriptCacheValidationStage::PayloadDecode,
				Reader.GetOffset());
		}

		const auto GetOffset = [&OutOffsets](
			const EField Field,
			const int32 PrimaryIndex)
		{
			const uint32 StoredPrimary = PrimaryIndex == INDEX_NONE
				? MAX_uint32 : static_cast<uint32>(PrimaryIndex);
			const auto Matches = [Field, StoredPrimary](const auto& Entry)
			{
				return Entry.Coordinate.Field == Field
					&& Entry.Coordinate.PrimaryIndex == StoredPrimary;
			};
			for (const auto& Entry : OutOffsets.HeaderOffsets)
			{
				if (Matches(Entry)) return Entry.Offset;
			}
			for (const auto& Entry : OutOffsets.DependencyOffsets)
			{
				if (Matches(Entry)) return Entry.Offset;
			}
			for (const auto& Entry : OutOffsets.DependencyExpectedValueOffsets)
			{
				if (Entry.IsSet() && Matches(Entry.GetValue())) return Entry->Offset;
			}
			const TOptional<FAngelscriptDecodedCacheRecord::
				FFunctionBodyCapturedOffsetStorage::FEntry> OptionalEntries[] = {
				OutOffsets.DebugSidecarOffset,
				OutOffsets.DebugSidecarKindOffset,
				OutOffsets.DebugSidecarContentHashOffset,
			};
			for (const auto& Entry : OptionalEntries)
			{
				if (Entry.IsSet() && Matches(Entry.GetValue())) return Entry->Offset;
			}
			checkNoEntry();
			return UINT64_C(0);
		};
		return ValidateFunctionBody(OutValue, GetOffset);
	}

	FAngelscriptCacheValidationResult FDecodedRecordCodecBridge::TryDecodeModuleSnapshot(
		const TConstArrayView<uint8> Payload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
		FAngelscriptCachedModuleSnapshot& OutValue,
		FAngelscriptDecodedCacheRecord::FModuleSnapshotCapturedOffsetStorage& OutOffsets)
	{
		OutValue = {};
		OutOffsets = {};
		if (const FAngelscriptCacheValidationResult BeginResult = BeginRead(
			Payload,
			Limits,
			Budget,
			EAngelscriptCacheRecordKind::ModuleSnapshot);
			!BeginResult.IsSuccess())
		{
			return BeginResult;
		}
		FReader Reader(
			Payload,
			Limits,
			Budget,
			EAngelscriptCacheRecordKind::ModuleSnapshot,
			ChargeSink);
		using EField = EAngelscriptModuleSnapshotCapturedField;
		const auto SetHeader = [&OutOffsets](
			const int32 Slot,
			const EField Field,
			const uint64 Offset)
		{
			OutOffsets.HeaderOffsets[Slot] = {
				{Field, MAX_uint32, MAX_uint32, MAX_uint32}, Offset};
		};

		const uint64 PayloadSchemaVersionOffset = Reader.GetOffset();
		if (!Reader.ReadUInt32(OutValue.PayloadSchemaVersion))
		{
			return Reader.GetResult();
		}
		SetHeader(0, EField::PayloadSchemaVersion, PayloadSchemaVersionOffset);
		const uint64 ModuleKeyOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.ModuleKey.Hash))
		{
			return Reader.GetResult();
		}
		SetHeader(1, EField::ModuleKey, ModuleKeyOffset);

		const uint64 ModuleInterfaceOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.ModuleInterface.ModuleKey.Hash))
		{
			return Reader.GetResult();
		}
		const uint64 ModuleInterfaceRecordIdOffset = Reader.GetOffset();
		if (!ReadRecordId(Reader, OutValue.ModuleInterface.RecordId))
		{
			return Reader.GetResult();
		}
		SetHeader(2, EField::ModuleInterface, ModuleInterfaceOffset);
		SetHeader(3, EField::ModuleInterfaceModuleKey, ModuleInterfaceOffset);
		SetHeader(4, EField::ModuleInterfaceRecordId, ModuleInterfaceRecordIdOffset);
		SetHeader(5, EField::ModuleInterfaceRecordIdKind, ModuleInterfaceRecordIdOffset);
		SetHeader(6, EField::ModuleInterfaceRecordIdContentHash,
			ModuleInterfaceRecordIdOffset + 1);

		const uint64 TypeSchemasOffset = Reader.GetOffset();
		uint32 TypeSchemaCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(65), OutValue.TypeSchemas, TypeSchemaCount))
		{
			return Reader.GetResult();
		}
		SetHeader(7, EField::TypeSchemas, TypeSchemasOffset);
		if (!Reader.ReserveDecodedArrayAtOffset(
			TypeSchemasOffset,
			UINT64_C(5) * TypeSchemaCount,
			OutOffsets.TypeSchemaLinkOffsets))
		{
			return Reader.GetResult();
		}
		for (uint32 Index = 0; Index < TypeSchemaCount; ++Index)
		{
			FAngelscriptCachedTypeSchemaLink& Link = OutValue.TypeSchemas.AddDefaulted_GetRef();
			const uint64 LinkOffset = Reader.GetOffset();
			if (!Reader.ReadHash(Link.TypeKey.Hash))
			{
				return Reader.GetResult();
			}
			const uint64 RecordIdOffset = Reader.GetOffset();
			if (!ReadRecordId(Reader, Link.RecordId))
			{
				return Reader.GetResult();
			}
			const auto Add = [&OutOffsets, Index](const EField Field, const uint64 Offset)
			{
				OutOffsets.TypeSchemaLinkOffsets.Add({
					{Field, Index, MAX_uint32, MAX_uint32}, Offset});
			};
			Add(EField::TypeSchemaLink, LinkOffset);
			Add(EField::TypeSchemaLinkTypeKey, LinkOffset);
			Add(EField::TypeSchemaLinkRecordId, RecordIdOffset);
			Add(EField::TypeSchemaLinkRecordIdKind, RecordIdOffset);
			Add(EField::TypeSchemaLinkRecordIdContentHash, RecordIdOffset + 1);
		}

		const uint64 ModuleStateOffset = Reader.GetOffset();
		if (!Reader.ReadHash(OutValue.ModuleState.ModuleKey.Hash))
		{
			return Reader.GetResult();
		}
		const uint64 ModuleStateRecordIdOffset = Reader.GetOffset();
		if (!ReadRecordId(Reader, OutValue.ModuleState.RecordId))
		{
			return Reader.GetResult();
		}
		SetHeader(8, EField::ModuleState, ModuleStateOffset);
		SetHeader(9, EField::ModuleStateModuleKey, ModuleStateOffset);
		SetHeader(10, EField::ModuleStateRecordId, ModuleStateRecordIdOffset);
		SetHeader(11, EField::ModuleStateRecordIdKind, ModuleStateRecordIdOffset);
		SetHeader(12, EField::ModuleStateRecordIdContentHash, ModuleStateRecordIdOffset + 1);

		const uint64 FunctionBodiesOffset = Reader.GetOffset();
		uint32 FunctionBodyCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			UINT64_C(65), OutValue.FunctionBodies, FunctionBodyCount))
		{
			return Reader.GetResult();
		}
		SetHeader(13, EField::FunctionBodies, FunctionBodiesOffset);
		if (!Reader.ReserveDecodedArrayAtOffset(
			FunctionBodiesOffset,
			UINT64_C(5) * FunctionBodyCount,
			OutOffsets.FunctionBodyLinkOffsets))
		{
			return Reader.GetResult();
		}
		for (uint32 Index = 0; Index < FunctionBodyCount; ++Index)
		{
			FAngelscriptCachedFunctionBodyLink& Link =
				OutValue.FunctionBodies.AddDefaulted_GetRef();
			const uint64 LinkOffset = Reader.GetOffset();
			if (!Reader.ReadHash(Link.FunctionKey.Hash))
			{
				return Reader.GetResult();
			}
			const uint64 RecordIdOffset = Reader.GetOffset();
			if (!ReadRecordId(Reader, Link.RecordId))
			{
				return Reader.GetResult();
			}
			const auto Add = [&OutOffsets, Index](const EField Field, const uint64 Offset)
			{
				OutOffsets.FunctionBodyLinkOffsets.Add({
					{Field, Index, MAX_uint32, MAX_uint32}, Offset});
			};
			Add(EField::FunctionBodyLink, LinkOffset);
			Add(EField::FunctionBodyLinkFunctionKey, LinkOffset);
			Add(EField::FunctionBodyLinkRecordId, RecordIdOffset);
			Add(EField::FunctionBodyLinkRecordIdKind, RecordIdOffset);
			Add(EField::FunctionBodyLinkRecordIdContentHash, RecordIdOffset + 1);
		}
		if (!Reader.IsAtEnd())
		{
			return RecordFailure(
				EAngelscriptCacheValidationError::TrailingData,
				EAngelscriptCacheRecordKind::ModuleSnapshot,
				EAngelscriptCacheValidationStage::PayloadDecode,
				Reader.GetOffset());
		}

		const auto GetOffset = [&OutOffsets](const EField Field, const int32 PrimaryIndex)
		{
			const uint32 StoredPrimary = PrimaryIndex == INDEX_NONE
				? MAX_uint32
				: static_cast<uint32>(PrimaryIndex);
			const auto Find = [Field, StoredPrimary](const auto& Entries) -> TOptional<uint64>
			{
				for (const auto& Entry : Entries)
				{
					if (Entry.Coordinate.Field == Field
						&& Entry.Coordinate.PrimaryIndex == StoredPrimary)
					{
						return Entry.Offset;
					}
				}
				return {};
			};
			if (const TOptional<uint64> Header = Find(OutOffsets.HeaderOffsets);
				Header.IsSet())
			{
				return Header.GetValue();
			}
			if (const TOptional<uint64> Type = Find(OutOffsets.TypeSchemaLinkOffsets);
				Type.IsSet())
			{
				return Type.GetValue();
			}
			const TOptional<uint64> Function = Find(OutOffsets.FunctionBodyLinkOffsets);
			check(Function.IsSet());
			return Function.GetValue();
		};
		return ValidateModuleSnapshot(OutValue, GetOffset);
	}
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
	const FAngelscriptCachedSourceFileKey& SourceFileKey,
	const FStringView CanonicalLogicalSection,
	FAngelscriptCachedLogicalSectionKey& OutKey)
{
	OutKey = {};
	if (SourceFileKey.Hash.IsZero())
	{
		return AngelscriptCacheRemainingRecords_Private::Failure(
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	if (const FAngelscriptCacheValidationResult StringResult =
		AngelscriptCacheRemainingRecords_Private::ValidateString(CanonicalLogicalSection);
		!StringResult.IsSuccess())
	{
		return StringResult;
	}
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-debug-logical-section"));
	Writer.WriteHash(SourceFileKey.Hash);
	Writer.WriteString(CanonicalLogicalSection);
	OutKey.Hash = Writer.FinalizeHash();
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::ComputeModuleStateInputHash(
	const FAngelscriptCachedModuleState& Value,
	FAngelscriptHash256& OutHash)
{
	FAngelscriptCachedModuleState Canonical = Value;
	AngelscriptCacheRemainingRecords_Private::CanonicalizeModuleStateSets(Canonical);
	OutHash = AngelscriptCacheRemainingRecords_Private::BuildModuleStateInputHash(Canonical);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::ComputeGlobalStorageLayoutFingerprint(
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptCachedGlobalSchema& Global,
	FAngelscriptHash256& OutHash)
{
	using namespace AngelscriptCacheRemainingRecords_Private;
	OutHash = {};
	if (ModuleKey.Hash.IsZero() || Global.GlobalKey.Hash.IsZero())
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	if (const FAngelscriptCacheValidationResult StringResult =
		ValidateString(Global.CanonicalNamespace); !StringResult.IsSuccess())
	{
		return RecordFailure(
			StringResult.Error,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	if (const FAngelscriptCacheValidationResult StringResult =
		ValidateString(Global.CanonicalName); !StringResult.IsSuccess())
	{
		return RecordFailure(
			StringResult.Error,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	uint32 NodeOrdinal = 0;
	if (const FAngelscriptCacheValidationResult TypeResult =
		ValidateModuleStateDataType(
			Global.Type,
			false,
			0,
			NodeOrdinal,
			1,
			[](const EAngelscriptModuleStateCapturedField, const int32, const int32)
			{
				return UINT64_C(0);
			});
		!TypeResult.IsSuccess())
	{
		return TypeResult;
	}
	if ((Global.GlobalTraitFlags & ~static_cast<uint32>(
		EAngelscriptCachedDeclarationTraitFlags::KnownMask)) != 0)
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::UnknownFlags,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	if (static_cast<uint8>(Global.InitializationKind) < 1
		|| static_cast<uint8>(Global.InitializationKind) > 3
		|| static_cast<uint8>(Global.CleanupPolicy) < 1
		|| static_cast<uint8>(Global.CleanupPolicy) > 3)
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::UnknownEnumValue,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	OutHash = BuildGlobalStorageLayoutFingerprint(ModuleKey, Global);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::ComputeGlobalConstantHardValueHash(
	const FAngelscriptCachedHardValue& HardValue,
	FAngelscriptHash256& OutHash)
{
	using namespace AngelscriptCacheRemainingRecords_Private;
	OutHash = {};
	if (HardValue.HardValueKind != EAngelscriptCachedHardValueKind::GlobalConstant
		|| HardValue.Owner.Kind != EAngelscriptCacheReferenceKind::ScriptGlobal)
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::WrongReferenceKind,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	if (!HardValue.CanonicalValue.IsSet())
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	const auto ZeroOffset = [](
		const EAngelscriptModuleStateCapturedField, const int32, const int32)
	{
		return UINT64_C(0);
	};
	if (const FAngelscriptCacheValidationResult ReferenceResult =
		ValidateModuleStateStableReference(
			HardValue.Owner,
			EAngelscriptModuleStateCapturedField::HardValueOwnerReferenceKind,
			EAngelscriptModuleStateCapturedField::HardValueOwnerStableKey,
			EAngelscriptModuleStateCapturedField::HardValueOwnerExpectedAbi,
			0,
			INDEX_NONE,
			ZeroOffset);
		!ReferenceResult.IsSuccess())
	{
		return ReferenceResult;
	}
	uint32 NodeOrdinal = 0;
	if (const FAngelscriptCacheValidationResult TypeResult =
		ValidateModuleStateDataType(
			HardValue.Type, true, 0, NodeOrdinal, 1, ZeroOffset);
		!TypeResult.IsSuccess())
	{
		return TypeResult;
	}
	if (const FAngelscriptCacheValidationResult ValueResult =
		ValidateCanonicalValue(HardValue, 0, ZeroOffset);
		!ValueResult.IsSuccess())
	{
		return ValueResult;
	}
	OutHash = BuildGlobalConstantHardValueHash(HardValue);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::ComputeInitializerExecutionHash(
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptArtifactProfileKey& Profile,
	const FAngelscriptCachedInitializerUnit& Initializer,
	FAngelscriptHash256& OutHash)
{
	using namespace AngelscriptCacheRemainingRecords_Private;
	OutHash = {};
	if (ModuleKey.Hash.IsZero() || Profile.Hash.IsZero()
		|| Initializer.InitializerKey.Hash.IsZero())
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	if (static_cast<uint8>(Initializer.InitializerKind) < 1
		|| static_cast<uint8>(Initializer.InitializerKind) > 2)
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::UnknownEnumValue,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	if ((Initializer.InitializerKind == EAngelscriptCachedInitializerKind::Global)
		!= Initializer.OwnerGlobal.IsSet())
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::InvalidPresence,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	if (Initializer.OwnerGlobal.IsSet() && Initializer.OwnerGlobal->Hash.IsZero())
	{
		return RecordFailure(
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheValidationStage::LocalSemantic);
	}
	OutHash = BuildInitializerExecutionHash(
		ModuleKey, Profile, Initializer);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::SerializeModuleState(
	const FAngelscriptCachedModuleState& Value,
	TArray<uint8>& OutPayload)
{
	OutPayload.Reset();
	FAngelscriptCachedModuleState Canonical = Value;
	AngelscriptCacheRemainingRecords_Private::CanonicalizeModuleStateSets(Canonical);
	if (const FAngelscriptCacheValidationResult ValidationResult =
		AngelscriptCacheRemainingRecords_Private::ValidateModuleState(
			Canonical,
			[](const EAngelscriptModuleStateCapturedField, const int32, const int32)
			{
				return UINT64_C(0);
			});
		!ValidationResult.IsSuccess())
	{
		return ValidationResult;
	}
	AngelscriptCacheRemainingRecords_Private::WriteModuleStatePayload(
		Canonical, OutPayload);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::SerializeDebugSidecar(
	const FAngelscriptCachedDebugSidecar& Value,
	TArray<uint8>& OutPayload)
{
	OutPayload.Reset();
	FAngelscriptCachedDebugSidecar Canonical = Value;
	for (const FAngelscriptCachedDebugSourceReference& Source : Canonical.Sources)
	{
		if (const FAngelscriptCacheValidationResult SourceResult =
			AngelscriptCacheRemainingRecords_Private::ValidateDebugSource(
				Source, 0, 0, 0);
			!SourceResult.IsSuccess())
		{
			return SourceResult;
		}
	}
	Canonical.Sources.Sort([](
		const FAngelscriptCachedDebugSourceReference& Left,
		const FAngelscriptCachedDebugSourceReference& Right)
	{
		return AngelscriptCacheRemainingRecords_Private::CompareSource(Left, Right) < 0;
	});
	if (const FAngelscriptCacheValidationResult ValidationResult =
		AngelscriptCacheRemainingRecords_Private::ValidateDebugSidecar(
			Canonical,
			[](const int32, const int32)
			{
				return UINT64_C(0);
			},
			0,
			0,
			0,
			0,
			0);
		!ValidationResult.IsSuccess())
	{
		return ValidationResult;
	}
	AngelscriptCacheRemainingRecords_Private::WriteDebugSidecar(Canonical, OutPayload);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
	const FAngelscriptCachedFunctionBody& Value,
	TArray<uint8>& OutPayload)
{
	OutPayload.Reset();
	FAngelscriptCachedFunctionBody Canonical = Value;
	Canonical.ActualDependencies.Sort([](
		const FAngelscriptCacheSemanticDependency& Left,
		const FAngelscriptCacheSemanticDependency& Right)
	{
		return AngelscriptCacheRemainingRecords_Private::CompareSemanticDependency(
			Left, Right) < 0;
	});
	if (const FAngelscriptCacheValidationResult ValidationResult =
		AngelscriptCacheRemainingRecords_Private::ValidateFunctionBody(
			Canonical,
			[](const EAngelscriptFunctionBodyCapturedField, const int32)
			{
				return UINT64_C(0);
			});
		!ValidationResult.IsSuccess())
	{
		return ValidationResult;
	}
	AngelscriptCacheRemainingRecords_Private::WriteFunctionBody(
		Canonical, OutPayload);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
	const FAngelscriptCachedModuleSnapshot& Value,
	TArray<uint8>& OutPayload)
{
	OutPayload.Reset();
	FAngelscriptCachedModuleSnapshot Canonical = Value;
	Canonical.TypeSchemas.Sort([](
		const FAngelscriptCachedTypeSchemaLink& Left,
		const FAngelscriptCachedTypeSchemaLink& Right)
	{
		return AngelscriptCacheRemainingRecords_Private::CompareTypeSchemaLink(Left, Right) < 0;
	});
	Canonical.FunctionBodies.Sort([](
		const FAngelscriptCachedFunctionBodyLink& Left,
		const FAngelscriptCachedFunctionBodyLink& Right)
	{
		return AngelscriptCacheRemainingRecords_Private::CompareFunctionBodyLink(Left, Right) < 0;
	});
	if (const FAngelscriptCacheValidationResult ValidationResult =
		AngelscriptCacheRemainingRecords_Private::ValidateModuleSnapshot(
			Canonical,
			[](const EAngelscriptModuleSnapshotCapturedField, const int32)
			{
				return UINT64_C(0);
			});
		!ValidationResult.IsSuccess())
	{
		return ValidationResult;
	}
	AngelscriptCacheRemainingRecords_Private::WriteModuleSnapshot(Canonical, OutPayload);
	return {};
}
