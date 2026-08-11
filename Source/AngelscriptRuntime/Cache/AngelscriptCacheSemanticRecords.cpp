#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "Cache/Private/AngelscriptCacheCanonicalCodec.h"
#include "Cache/Private/AngelscriptCacheMemoryView.h"
#include "Cache/Private/AngelscriptCacheSemanticRecordCodec.h"
#include "Internationalization/TextChar.h"

struct FAngelscriptCacheSemanticCandidateAccess
{
	using FTransaction = FAngelscriptCacheReadBudget::FDecodedCandidateTransaction;

	static FTransaction Begin(
		FAngelscriptCacheReadBudget& Budget,
		const FAngelscriptCacheReadLimits& Limits)
	{
		return Budget.BeginDecodedCandidateTransaction(Limits);
	}

	static AngelscriptCacheCanonicalCodec_Private::EDecodedChargeResult TryCharge(
		void* RawContext,
		const uint64 Bytes)
	{
		FTransaction& Transaction = *static_cast<FTransaction*>(RawContext);
		switch (Transaction.TryExtend(Bytes))
		{
		case FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult::Success:
			return AngelscriptCacheCanonicalCodec_Private::EDecodedChargeResult::Accepted;
		case FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult::BudgetExceeded:
			return AngelscriptCacheCanonicalCodec_Private::EDecodedChargeResult::BudgetExceeded;
		case FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult::Overflow:
		case FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult::InvalidState:
		default:
			return AngelscriptCacheCanonicalCodec_Private::EDecodedChargeResult::Overflow;
		}
	}

	static bool Promote(FTransaction& Transaction)
	{
		return Transaction.GetAggregateTemporaryBytes() == 0
			|| Transaction.PromoteToRetained();
	}
};

namespace
{
	using EDecodedChargeResult =
		AngelscriptCacheCanonicalCodec_Private::EDecodedChargeResult;
	using FDecodedChargeSink =
		AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink;
#if WITH_ANGELSCRIPT_UNITTESTS
	using FDecodedAllocationObserverForTests =
		AngelscriptCacheCanonicalCodec_Private::FDecodedAllocationObserverForTests;
#endif

#if WITH_ANGELSCRIPT_UNITTESTS
	static void ObserveExplicitCanonicalAllocationForTests(
		void* RawContext,
		const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event)
	{
		using namespace AngelscriptCacheCanonicalCodecTestHooks;
		FAllocationEventCaptureView& Capture =
			*static_cast<FAllocationEventCaptureView*>(RawContext);
		const int32 EventIndex = Capture.EventCount;
		if (EventIndex < 0
			|| EventIndex >= Capture.EventStorage.Num()
			|| Capture.EventStorage.GetData() == nullptr)
		{
			Capture.bOverflowed = true;
			return;
		}
		Capture.EventStorage[EventIndex] = Event;
		Capture.EventCount = EventIndex + 1;
	}

	static FDecodedAllocationObserverForTests MakeExplicitAllocationObserverForTests(
		AngelscriptCacheCanonicalCodecTestHooks::FAllocationEventCaptureView& Capture)
	{
		Capture.EventCount = 0;
		Capture.bOverflowed = false;
		if (Capture.EventStorage.Num() > 0
			&& Capture.EventStorage.GetData() == nullptr)
		{
			Capture.bOverflowed = true;
			return {};
		}
		return {&Capture, &ObserveExplicitCanonicalAllocationForTests};
	}
#endif

	static FDecodedChargeSink MakeCandidateDecodedChargeSink(
		FAngelscriptCacheSemanticCandidateAccess::FTransaction& Transaction
#if WITH_ANGELSCRIPT_UNITTESTS
		, const FDecodedAllocationObserverForTests Observer = {}
#endif
		)
	{
		return FDecodedChargeSink(
			&Transaction,
			&FAngelscriptCacheSemanticCandidateAccess::TryCharge
#if WITH_ANGELSCRIPT_UNITTESTS
			, Observer
#endif
			);
	}
}

#if WITH_ANGELSCRIPT_UNITTESTS
namespace
{
	static bool PrepareEligibilityAllocationCaptureForTests(
		AngelscriptCacheEligibilityTestHooks::FAllocationEventCaptureView* Capture)
	{
		if (Capture == nullptr)
		{
			return true;
		}
		Capture->EventCount = 0;
		Capture->bOverflowed = false;
		if (Capture->EventStorage.Num() > 0
			&& Capture->EventStorage.GetData() == nullptr)
		{
			Capture->bOverflowed = true;
			return false;
		}
		return true;
	}

	static void ObserveEligibilityAllocationForTests(
		AngelscriptCacheEligibilityTestHooks::FAllocationEventCaptureView* Capture,
		const AngelscriptCacheEligibilityTestHooks::FAllocationEvent& Event)
	{
		if (Capture == nullptr)
		{
			return;
		}
		const int32 EventIndex = Capture->EventCount;
		if (EventIndex < 0
			|| EventIndex >= Capture->EventStorage.Num()
			|| Capture->EventStorage.GetData() == nullptr)
		{
			Capture->bOverflowed = true;
			return;
		}
		Capture->EventStorage[EventIndex] = Event;
		Capture->EventCount = EventIndex + 1;
	}
}
#endif

namespace AngelscriptCacheSemanticRecords_Private
{
	static FAngelscriptCacheValidationResult Failure(
		const EAngelscriptCacheValidationError Error,
		const EAngelscriptCacheRecordKind Kind = static_cast<EAngelscriptCacheRecordKind>(0),
		const uint64 Offset = 0)
	{
		return FAngelscriptCacheValidationResult(Error, Kind, Offset);
	}

	static int32 CompareHash(const FAngelscriptHash256& A, const FAngelscriptHash256& B)
	{
		if (A == B)
		{
			return 0;
		}
		return A < B ? -1 : 1;
	}

	static int32 CompareString(const FString& A, const FString& B)
	{
		return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(A, B);
	}

	static int32 CompareUnicodeSimpleFold(const FString& A, const FString& B)
	{
		int32 AIndex = 0;
		int32 BIndex = 0;
		while (AIndex < A.Len() && BIndex < B.Len())
		{
			int32 ATCharCount = 0;
			int32 BTCharCount = 0;
			const UTF32CHAR AFolded = FTextChar::ToLower(
				FTextChar::GetCodepoint(*A + AIndex, &ATCharCount));
			const UTF32CHAR BFolded = FTextChar::ToLower(
				FTextChar::GetCodepoint(*B + BIndex, &BTCharCount));
			if (AFolded != BFolded)
			{
				return AFolded < BFolded ? -1 : 1;
			}
			AIndex += ATCharCount;
			BIndex += BTCharCount;
		}
		if (AIndex == A.Len() && BIndex == B.Len())
		{
			return 0;
		}
		return AIndex == A.Len() ? -1 : 1;
	}

	static bool IsKnownEntityKind(const EAngelscriptArtifactEntityKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptArtifactEntityKind::Class:
		case EAngelscriptArtifactEntityKind::Struct:
		case EAngelscriptArtifactEntityKind::Interface:
		case EAngelscriptArtifactEntityKind::Enum:
		case EAngelscriptArtifactEntityKind::Delegate:
		case EAngelscriptArtifactEntityKind::Typedef:
		case EAngelscriptArtifactEntityKind::Funcdef:
		case EAngelscriptArtifactEntityKind::GlobalVariable:
		case EAngelscriptArtifactEntityKind::Property:
		case EAngelscriptArtifactEntityKind::GlobalFunction:
		case EAngelscriptArtifactEntityKind::Method:
		case EAngelscriptArtifactEntityKind::Constructor:
		case EAngelscriptArtifactEntityKind::Destructor:
		case EAngelscriptArtifactEntityKind::Factory:
		case EAngelscriptArtifactEntityKind::DelegateSignature:
		case EAngelscriptArtifactEntityKind::ModuleInitializer:
		case EAngelscriptArtifactEntityKind::GlobalInitializer:
		case EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor:
		case EAngelscriptArtifactEntityKind::GeneratedDefaultDestructor:
		case EAngelscriptArtifactEntityKind::InitDefaults:
			return true;
		default:
			return false;
		}
	}

	static FAngelscriptCacheValidationResult ValidateString(const FStringView Value)
	{
		if (Value.Len() < 0 || (Value.Len() > 0 && Value.GetData() == nullptr))
		{
			return Failure(EAngelscriptCacheValidationError::InvalidArrayView);
		}
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const uint32 CodeUnit = static_cast<uint32>(Value[Index]);
			if (CodeUnit == 0)
			{
				return Failure(EAngelscriptCacheValidationError::EmbeddedNul);
			}
			if constexpr (sizeof(TCHAR) == 2)
			{
				if (CodeUnit >= 0xd800 && CodeUnit <= 0xdbff)
				{
					if (Index + 1 >= Value.Len())
					{
						return Failure(EAngelscriptCacheValidationError::InvalidUtf8);
					}
					const uint32 Low = static_cast<uint32>(Value[++Index]);
					if (Low < 0xdc00 || Low > 0xdfff)
					{
						return Failure(EAngelscriptCacheValidationError::InvalidUtf8);
					}
				}
				else if (CodeUnit >= 0xdc00 && CodeUnit <= 0xdfff)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidUtf8);
				}
			}
			else if (CodeUnit > 0x10ffff || (CodeUnit >= 0xd800 && CodeUnit <= 0xdfff))
			{
				return Failure(EAngelscriptCacheValidationError::InvalidUtf8);
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateString(const FString& Value)
	{
		return ValidateString(FStringView(Value.GetCharArray().GetData(), Value.Len()));
	}

	static FAngelscriptCacheValidationResult ValidateRequiredString(const FString& Value)
	{
		const FAngelscriptCacheValidationResult Result = ValidateString(Value);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		return Value.IsEmpty()
			? Failure(EAngelscriptCacheValidationError::InvalidPresence)
			: FAngelscriptCacheValidationResult{};
	}

	using AngelscriptCacheCanonicalCodec_Private::BeginRead;
	using AngelscriptCacheCanonicalCodec_Private::FReader;
	using AngelscriptCacheCanonicalCodec_Private::FWriter;
	using AngelscriptCacheCanonicalCodec_Private::ReadEnum;

	static FAngelscriptCacheValidationResult ValidateStableReference(
		const FAngelscriptCacheStableReference& Value)
	{
		const uint8 RawKind = static_cast<uint8>(Value.Kind);
		if (RawKind < 1 || RawKind > 9)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		if (Value.StableKey.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		const bool bRequiresAbi = RawKind <= static_cast<uint8>(EAngelscriptCacheReferenceKind::EnvironmentSymbol);
		if (bRequiresAbi && Value.ExpectedAbi.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::MissingExpectedAbi);
		}
		if (!bRequiresAbi && !Value.ExpectedAbi.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ForbiddenExpectedAbi);
		}
		return {};
	}

	static void WriteStableReference(FWriter& Writer, const FAngelscriptCacheStableReference& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteHash(Value.StableKey);
		Writer.WriteHash(Value.ExpectedAbi);
	}

	static bool ReadStableReference(FReader& Reader, FAngelscriptCacheStableReference& OutValue)
	{
		if (!Reader.ConsumeReference()
			|| !ReadEnum(Reader, OutValue.Kind, 1, 9)
			|| !Reader.ReadHash(OutValue.StableKey)
			|| !Reader.ReadHash(OutValue.ExpectedAbi))
		{
			return false;
		}
		const FAngelscriptCacheValidationResult Result = ValidateStableReference(OutValue);
		if (!Result.IsSuccess())
		{
			Reader.FailSemantic(Result.Error);
			return false;
		}
		return true;
	}

	static bool DependencyRequiresContent(const EAngelscriptCacheSemanticDependencyKind Kind)
	{
		return Kind == EAngelscriptCacheSemanticDependencyKind::ValueLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::PropertyLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::GlobalStorage
			|| Kind == EAngelscriptCacheSemanticDependencyKind::HardValue
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Initializer
			|| Kind == EAngelscriptCacheSemanticDependencyKind::CompileOption
			|| Kind == EAngelscriptCacheSemanticDependencyKind::FunctionContent;
	}

	static bool DependencyContributesToInterfaceAbi(
		const EAngelscriptCacheSemanticDependencyKind Kind)
	{
		return Kind == EAngelscriptCacheSemanticDependencyKind::Import
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Declaration
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Signature
			|| Kind == EAngelscriptCacheSemanticDependencyKind::Inheritance
			|| Kind == EAngelscriptCacheSemanticDependencyKind::ValueLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::PropertyLayout
			|| Kind == EAngelscriptCacheSemanticDependencyKind::GlobalStorage
			|| Kind == EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
	}

	static FAngelscriptCacheValidationResult ValidateSemanticDependency(
		const FAngelscriptCacheSemanticDependency& Value)
	{
		const uint8 RawKind = static_cast<uint8>(Value.Kind);
		if (RawKind < 1 || RawKind > 12)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		const FAngelscriptCacheValidationResult ReferenceResult = ValidateStableReference(Value.Target);
		if (!ReferenceResult.IsSuccess())
		{
			return ReferenceResult;
		}
		if (DependencyRequiresContent(Value.Kind) != Value.ExpectedContentOrValue.IsSet())
		{
			return Failure(EAngelscriptCacheValidationError::InvalidPresence);
		}
		if (Value.ExpectedContentOrValue.IsSet() && Value.ExpectedContentOrValue->IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		if (Value.Kind == EAngelscriptCacheSemanticDependencyKind::FunctionContent
			&& Value.Target.Kind
				!= EAngelscriptCacheReferenceKind::ScriptFunction)
		{
			return Failure(EAngelscriptCacheValidationError::WrongReferenceKind);
		}
		return {};
	}

	static void WriteSemanticDependency(FWriter& Writer, const FAngelscriptCacheSemanticDependency& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		WriteStableReference(Writer, Value.Target);
		Writer.WriteOptionalHash(Value.ExpectedContentOrValue);
	}

	static bool ReadSemanticDependency(FReader& Reader, FAngelscriptCacheSemanticDependency& OutValue)
	{
		if (!ReadEnum(Reader, OutValue.Kind, 1, 12)
			|| !ReadStableReference(Reader, OutValue.Target)
			|| !Reader.ReadOptionalHash(OutValue.ExpectedContentOrValue))
		{
			return false;
		}
		const FAngelscriptCacheValidationResult Result = ValidateSemanticDependency(OutValue);
		if (!Result.IsSuccess())
		{
			Reader.FailSemantic(Result.Error);
			return false;
		}
		return true;
	}

	static FAngelscriptCacheValidationResult ValidateDataType(
		const FAngelscriptCachedDataType& Value,
		const uint64 Depth)
	{
		if (Depth > FAngelscriptCacheReadLimits::DefaultMaxNestingDepth)
		{
			return Failure(EAngelscriptCacheValidationError::NestingDepthExceeded);
		}
		const uint8 RawKind = static_cast<uint8>(Value.Kind);
		if (RawKind < 1 || RawKind > 4)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		if ((Value.QualifierFlags
			& ~static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::KnownMask)) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags);
		}
		const uint32 AutoFlag = static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::Auto);
		const bool bAutoKind = Value.Kind == EAngelscriptCachedDataTypeKind::Auto;
		if (bAutoKind != ((Value.QualifierFlags & AutoFlag) != 0))
		{
			return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
		}
		const uint32 ObjectHandle = static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
		const uint32 ConstHandle = static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::ConstHandle);
		if ((Value.QualifierFlags & ConstHandle) != 0 && (Value.QualifierFlags & ObjectHandle) == 0)
		{
			return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
		}

		switch (Value.Kind)
		{
		case EAngelscriptCachedDataTypeKind::Primitive:
		{
			if (static_cast<uint8>(Value.Primitive) < 1 || static_cast<uint8>(Value.Primitive) > 12
				|| Value.TypeReference.IsSet())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			const uint32 PrimitiveQualifiers = static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::Reference)
				| static_cast<uint32>(
					EAngelscriptCachedTypeQualifierFlags::ObjectConst);
			if ((Value.QualifierFlags & ~PrimitiveQualifiers) != 0)
			{
				return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
			}
			break;
		}

		case EAngelscriptCachedDataTypeKind::ScriptType:
		case EAngelscriptCachedDataTypeKind::EnvironmentType:
			if (Value.Primitive != EAngelscriptCachedPrimitiveType::Invalid || !Value.TypeReference.IsSet())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			if ((Value.Kind == EAngelscriptCachedDataTypeKind::ScriptType
					&& Value.TypeReference->Kind != EAngelscriptCacheReferenceKind::ScriptType)
				|| (Value.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType
					&& Value.TypeReference->Kind != EAngelscriptCacheReferenceKind::EnvironmentSymbol))
			{
				return Failure(EAngelscriptCacheValidationError::WrongReferenceKind);
			}
			if (const FAngelscriptCacheValidationResult ReferenceResult =
				ValidateStableReference(Value.TypeReference.GetValue()); !ReferenceResult.IsSuccess())
			{
				return ReferenceResult;
			}
			break;

		case EAngelscriptCachedDataTypeKind::Auto:
			if (Value.Primitive != EAngelscriptCachedPrimitiveType::Invalid
				|| Value.TypeReference.IsSet() || Value.QualifierFlags != AutoFlag)
			{
				return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
			}
			break;

		default:
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}

		for (const FAngelscriptCachedDataType& SubType : Value.OrderedSubTypes)
		{
			const FAngelscriptCacheValidationResult Result = ValidateDataType(SubType, Depth + 1);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
		return {};
	}

	static void WriteDataType(FWriter& Writer, const FAngelscriptCachedDataType& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteUInt8(static_cast<uint8>(Value.Primitive));
		Writer.WriteUInt8(Value.TypeReference.IsSet() ? 1 : 0);
		if (Value.TypeReference.IsSet())
		{
			WriteStableReference(Writer, Value.TypeReference.GetValue());
		}
		Writer.WriteUInt32(Value.QualifierFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedSubTypes.Num()));
		for (const FAngelscriptCachedDataType& SubType : Value.OrderedSubTypes)
		{
			WriteDataType(Writer, SubType);
		}
	}

	static bool ReadDataType(FReader& Reader, FAngelscriptCachedDataType& OutValue, const uint64 Depth)
	{
		if (Depth > Reader.GetLimits().MaxNestingDepth)
		{
			Reader.Fail(EAngelscriptCacheValidationError::NestingDepthExceeded);
			return false;
		}
		if (!ReadEnum(Reader, OutValue.Kind, 1, 4))
		{
			return false;
		}
		uint8 PrimitiveRaw = 0;
		const uint64 PrimitiveOffset = Reader.GetOffset();
		if (!Reader.ReadUInt8(PrimitiveRaw))
		{
			return false;
		}
		if (PrimitiveRaw > 12)
		{
			Reader.Fail(EAngelscriptCacheValidationError::UnknownEnumValue, PrimitiveOffset);
			return false;
		}
		OutValue.Primitive = static_cast<EAngelscriptCachedPrimitiveType>(PrimitiveRaw);
		uint8 OptionalTag = 0;
		const uint64 OptionalTagOffset = Reader.GetOffset();
		if (!Reader.ReadUInt8(OptionalTag))
		{
			return false;
		}
		if (OptionalTag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, OptionalTagOffset);
			return false;
		}
		if (OptionalTag == 1)
		{
			FAngelscriptCacheStableReference Reference;
			if (!ReadStableReference(Reader, Reference))
			{
				return false;
			}
			OutValue.TypeReference = MoveTemp(Reference);
		}
		if (!Reader.ReadUInt32(OutValue.QualifierFlags))
		{
			return false;
		}
		uint32 Count = 0;
		if (!Reader.ReadArrayCountAndReserve(11, OutValue.OrderedSubTypes, Count))
		{
			return false;
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FAngelscriptCachedDataType SubType;
			if (!ReadDataType(Reader, SubType, Depth + 1))
			{
				return false;
			}
			OutValue.OrderedSubTypes.Add(MoveTemp(SubType));
		}
		const FAngelscriptCacheValidationResult Result = ValidateDataType(OutValue, Depth);
		if (!Result.IsSuccess())
		{
			Reader.FailSemantic(Result.Error);
			return false;
		}
		return true;
	}

	static FAngelscriptCacheValidationResult ValidateMetadata(const FAngelscriptCachedMetadataEntry& Value)
	{
		if (const FAngelscriptCacheValidationResult KeyResult = ValidateRequiredString(Value.CanonicalKey);
			!KeyResult.IsSuccess())
		{
			return KeyResult;
		}
		return ValidateString(Value.CanonicalValue);
	}

	static void WriteMetadata(FWriter& Writer, const FAngelscriptCachedMetadataEntry& Value)
	{
		Writer.WriteString(Value.CanonicalKey);
		Writer.WriteString(Value.CanonicalValue);
	}

	static bool ReadMetadata(FReader& Reader, FAngelscriptCachedMetadataEntry& OutValue)
	{
		return Reader.ReadString(OutValue.CanonicalKey)
			&& Reader.ReadString(OutValue.CanonicalValue);
	}

	static FAngelscriptCacheValidationResult ValidateOption(const FAngelscriptCachedCanonicalOption& Value)
	{
		if (const FAngelscriptCacheValidationResult KeyResult = ValidateRequiredString(Value.CanonicalKey);
			!KeyResult.IsSuccess())
		{
			return KeyResult;
		}
		return Value.ValueFingerprint.IsZero()
			? Failure(EAngelscriptCacheValidationError::ZeroStableKey)
			: FAngelscriptCacheValidationResult{};
	}

	static void WriteOption(FWriter& Writer, const FAngelscriptCachedCanonicalOption& Value)
	{
		Writer.WriteString(Value.CanonicalKey);
		Writer.WriteHash(Value.ValueFingerprint);
	}

	static bool ReadOption(FReader& Reader, FAngelscriptCachedCanonicalOption& OutValue)
	{
		return Reader.ReadString(OutValue.CanonicalKey)
			&& Reader.ReadHash(OutValue.ValueFingerprint);
	}

	static FAngelscriptCacheValidationResult ValidateParameter(const FAngelscriptCachedParameter& Value)
	{
		if (const FAngelscriptCacheValidationResult NameResult = ValidateRequiredString(Value.CanonicalName);
			!NameResult.IsSuccess())
		{
			return NameResult;
		}
		if (const FAngelscriptCacheValidationResult TypeResult = ValidateDataType(Value.Type, 1);
			!TypeResult.IsSuccess())
		{
			return TypeResult;
		}
		if (static_cast<uint8>(Value.Passing) < 1 || static_cast<uint8>(Value.Passing) > 4)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		if (Value.CanonicalDefaultExpression.IsSet())
		{
			const FAngelscriptCacheValidationResult DefaultResult =
				ValidateString(Value.CanonicalDefaultExpression.GetValue());
			if (!DefaultResult.IsSuccess())
			{
				return DefaultResult;
			}
		}
		if ((Value.TraitFlags
			& ~static_cast<uint32>(EAngelscriptCachedParameterTraitFlags::KnownMask)) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags);
		}
		return {};
	}

	static void WriteParameter(FWriter& Writer, const FAngelscriptCachedParameter& Value)
	{
		Writer.WriteUInt32(Value.Ordinal);
		Writer.WriteString(Value.CanonicalName);
		WriteDataType(Writer, Value.Type);
		Writer.WriteUInt8(static_cast<uint8>(Value.Passing));
		Writer.WriteOptionalString(Value.CanonicalDefaultExpression);
		Writer.WriteUInt32(Value.TraitFlags);
	}

	static bool ReadParameter(FReader& Reader, FAngelscriptCachedParameter& OutValue)
	{
		if (!Reader.ReadUInt32(OutValue.Ordinal)
			|| !Reader.ReadString(OutValue.CanonicalName)
			|| !ReadDataType(Reader, OutValue.Type, 1)
			|| !ReadEnum(Reader, OutValue.Passing, 1, 4)
			|| !Reader.ReadOptionalString(OutValue.CanonicalDefaultExpression)
			|| !Reader.ReadUInt32(OutValue.TraitFlags))
		{
			return false;
		}
		const FAngelscriptCacheValidationResult Result = ValidateParameter(OutValue);
		if (!Result.IsSuccess())
		{
			Reader.FailSemantic(Result.Error);
			return false;
		}
		return true;
	}

	static FAngelscriptCacheValidationResult ValidateSlot(const FAngelscriptCachedDeclarationSlot& Value)
	{
		return static_cast<uint8>(Value.SlotKind) >= 1 && static_cast<uint8>(Value.SlotKind) <= 4
			? FAngelscriptCacheValidationResult{}
			: Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}

	static void WriteSlot(FWriter& Writer, const FAngelscriptCachedDeclarationSlot& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.SlotKind));
		Writer.WriteUInt32(Value.Ordinal);
	}

	static bool ReadSlot(FReader& Reader, FAngelscriptCachedDeclarationSlot& OutValue)
	{
		return ReadEnum(Reader, OutValue.SlotKind, 1, 4)
			&& Reader.ReadUInt32(OutValue.Ordinal);
	}

	template <typename ElementType, typename CompareType>
	static FAngelscriptCacheValidationResult ValidateOrder(
		const TArray<ElementType>& Values,
		CompareType&& Compare,
		int32* OutFailureIndex = nullptr)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		for (int32 Index = 1; Index < Values.Num(); ++Index)
		{
			if (Compare(Values[Index - 1], Values[Index]) >= 0)
			{
				if (OutFailureIndex != nullptr)
				{
					*OutFailureIndex = Index;
				}
				return Failure(EAngelscriptCacheValidationError::NonCanonicalOrder);
			}
		}
		return {};
	}

	template <typename ElementType, typename AuthorityCompareType, typename EqualContentType>
	static FAngelscriptCacheValidationResult ValidateSortedAuthorityDuplicates(
		const TArray<ElementType>& Values,
		AuthorityCompareType&& CompareAuthority,
		EqualContentType&& EqualContent,
		int32* OutFailureIndex = nullptr,
		const EAngelscriptCacheValidationError DuplicateError = EAngelscriptCacheValidationError::DuplicateKey,
		const EAngelscriptCacheValidationError ConflictError = EAngelscriptCacheValidationError::ConflictingKey)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		TArray<int32> Index;
		Index.Reserve(Values.Num());
		for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex) { Index.Add(ValueIndex); }
		Index.Sort([&](const int32 AIndex, const int32 BIndex)
		{
			const int32 Compare = CompareAuthority(Values[AIndex], Values[BIndex]);
			return Compare != 0 ? Compare < 0 : AIndex < BIndex;
		});
		int32 FirstDuplicateValueIndex = INDEX_NONE;
		int32 SecondDuplicateValueIndex = INDEX_NONE;
		for (int32 GroupBegin = 0; GroupBegin < Index.Num();)
		{
			int32 GroupEnd = GroupBegin + 1;
			while (GroupEnd < Index.Num()
				&& CompareAuthority(Values[Index[GroupBegin]], Values[Index[GroupEnd]]) == 0)
			{
				++GroupEnd;
			}
			if (GroupEnd - GroupBegin >= 2
				&& (SecondDuplicateValueIndex == INDEX_NONE || Index[GroupBegin + 1] < SecondDuplicateValueIndex))
			{
				FirstDuplicateValueIndex = Index[GroupBegin];
				SecondDuplicateValueIndex = Index[GroupBegin + 1];
			}
			GroupBegin = GroupEnd;
		}
		if (SecondDuplicateValueIndex == INDEX_NONE) { return {}; }
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = SecondDuplicateValueIndex;
		}
		return Failure(EqualContent(
			Values[FirstDuplicateValueIndex], Values[SecondDuplicateValueIndex])
			? DuplicateError : ConflictError);
	}

	static FAngelscriptCacheValidationResult PrepareOptions(
		TArray<FAngelscriptCachedCanonicalOption>& Values,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		int32* OutFailureIndex = nullptr)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			const FAngelscriptCachedCanonicalOption& Value = Values[Index];
			const FAngelscriptCacheValidationResult Result = ValidateOption(Value);
			if (!Result.IsSuccess())
			{
				if (OutFailureIndex != nullptr)
				{
					*OutFailureIndex = Index;
				}
				return Result;
			}
		}
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateSortedAuthorityDuplicates(
			Values,
			[](const auto& A, const auto& B) { return CompareString(A.CanonicalKey, B.CanonicalKey); },
			[](const auto& A, const auto& B) { return A.ValueFingerprint == B.ValueFingerprint; },
			OutFailureIndex);
			!DuplicateResult.IsSuccess()) { return DuplicateResult; }
		auto Compare = [](const FAngelscriptCachedCanonicalOption& A, const FAngelscriptCachedCanonicalOption& B)
		{
			const int32 KeyCompare = CompareString(A.CanonicalKey, B.CanonicalKey);
			return KeyCompare != 0 ? KeyCompare : CompareHash(A.ValueFingerprint, B.ValueFingerprint);
		};
		if (bCanonicalize)
		{
			Values.Sort([&](const auto& A, const auto& B) { return Compare(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			return ValidateOrder(Values, Compare, OutFailureIndex);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult PrepareMetadata(
		TArray<FAngelscriptCachedMetadataEntry>& Values,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		int32* OutFailureIndex = nullptr)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			const FAngelscriptCachedMetadataEntry& Value = Values[Index];
			const FAngelscriptCacheValidationResult Result = ValidateMetadata(Value);
			if (!Result.IsSuccess())
			{
				if (OutFailureIndex != nullptr)
				{
					*OutFailureIndex = Index;
				}
				return Result;
			}
		}
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateSortedAuthorityDuplicates(
			Values,
			[](const auto& A, const auto& B) { return CompareString(A.CanonicalKey, B.CanonicalKey); },
			[](const auto& A, const auto& B) { return A.CanonicalValue == B.CanonicalValue; },
			OutFailureIndex);
			!DuplicateResult.IsSuccess()) { return DuplicateResult; }
		auto Compare = [](const FAngelscriptCachedMetadataEntry& A, const FAngelscriptCachedMetadataEntry& B)
		{
			const int32 KeyCompare = CompareString(A.CanonicalKey, B.CanonicalKey);
			return KeyCompare != 0 ? KeyCompare : CompareString(A.CanonicalValue, B.CanonicalValue);
		};
		if (bCanonicalize)
		{
			Values.Sort([&](const auto& A, const auto& B) { return Compare(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			return ValidateOrder(Values, Compare, OutFailureIndex);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult PrepareStringSet(
		TArray<FString>& Values,
		const bool bAllowEmpty,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		int32* OutFailureIndex = nullptr)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			const FString& Value = Values[Index];
			const FAngelscriptCacheValidationResult Result =
				bAllowEmpty ? ValidateString(Value) : ValidateRequiredString(Value);
			if (!Result.IsSuccess())
			{
				if (OutFailureIndex != nullptr)
				{
					*OutFailureIndex = Index;
				}
				return Result;
			}
		}
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateSortedAuthorityDuplicates(
			Values, [](const FString& A, const FString& B) { return CompareString(A, B); },
			[](const FString&, const FString&) { return true; }, OutFailureIndex);
			!DuplicateResult.IsSuccess())
		{
			return DuplicateResult;
		}
		auto Compare = [](const FString& A, const FString& B) { return CompareString(A, B); };
		if (bCanonicalize)
		{
			Values.Sort([](const FString& A, const FString& B)
			{
				return CompareString(A, B) < 0;
			});
		}
		else if (bRequireCanonicalOrder)
		{
			return ValidateOrder(Values, Compare, OutFailureIndex);
		}
		return {};
	}

	static int32 CompareDependency(
		const FAngelscriptCacheSemanticDependency& A,
		const FAngelscriptCacheSemanticDependency& B)
	{
		if (A.Kind != B.Kind)
		{
			return static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind) ? -1 : 1;
		}
		if (A.Target.Kind != B.Target.Kind)
		{
			return static_cast<uint8>(A.Target.Kind) < static_cast<uint8>(B.Target.Kind) ? -1 : 1;
		}
		if (const int32 KeyCompare = CompareHash(A.Target.StableKey, B.Target.StableKey); KeyCompare != 0)
		{
			return KeyCompare;
		}
		if (const int32 AbiCompare = CompareHash(A.Target.ExpectedAbi, B.Target.ExpectedAbi); AbiCompare != 0)
		{
			return AbiCompare;
		}
		if (A.ExpectedContentOrValue.IsSet() != B.ExpectedContentOrValue.IsSet())
		{
			return A.ExpectedContentOrValue.IsSet() ? 1 : -1;
		}
		return A.ExpectedContentOrValue.IsSet()
			? CompareHash(A.ExpectedContentOrValue.GetValue(), B.ExpectedContentOrValue.GetValue())
			: 0;
	}

	static FAngelscriptCacheValidationResult PrepareDependencies(
		TArray<FAngelscriptCacheSemanticDependency>& Values,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		int32* OutFailureIndex = nullptr)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			const FAngelscriptCacheSemanticDependency& Value = Values[Index];
			const FAngelscriptCacheValidationResult Result = ValidateSemanticDependency(Value);
			if (!Result.IsSuccess())
			{
				if (OutFailureIndex != nullptr)
				{
					*OutFailureIndex = Index;
				}
				return Result;
			}
		}
		auto CompareAuthority = [](const FAngelscriptCacheSemanticDependency& A,
			const FAngelscriptCacheSemanticDependency& B)
		{
			if (A.Kind != B.Kind) { return static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind) ? -1 : 1; }
			if (A.Target.Kind != B.Target.Kind)
			{
				return static_cast<uint8>(A.Target.Kind) < static_cast<uint8>(B.Target.Kind) ? -1 : 1;
			}
			return CompareHash(A.Target.StableKey, B.Target.StableKey);
		};
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateSortedAuthorityDuplicates(
			Values, CompareAuthority,
			[](const auto& A, const auto& B) { return CompareDependency(A, B) == 0; },
			OutFailureIndex);
			!DuplicateResult.IsSuccess()) { return DuplicateResult; }
		if (bCanonicalize)
		{
			Values.Sort([](const auto& A, const auto& B) { return CompareDependency(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			return ValidateOrder(Values, CompareDependency, OutFailureIndex);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult PrepareSlots(
		TArray<FAngelscriptCachedDeclarationSlot>& Values,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		int32* OutFailureIndex = nullptr)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			const FAngelscriptCachedDeclarationSlot& Value = Values[Index];
			const FAngelscriptCacheValidationResult Result = ValidateSlot(Value);
			if (!Result.IsSuccess())
			{
				if (OutFailureIndex != nullptr)
				{
					*OutFailureIndex = Index;
				}
				return Result;
			}
		}
		auto CompareAuthority = [](const FAngelscriptCachedDeclarationSlot& A,
			const FAngelscriptCachedDeclarationSlot& B)
		{
			if (A.SlotKind != B.SlotKind)
			{
				return static_cast<uint8>(A.SlotKind) < static_cast<uint8>(B.SlotKind) ? -1 : 1;
			}
			if (A.Ordinal == B.Ordinal) { return 0; }
			return A.Ordinal < B.Ordinal ? -1 : 1;
		};
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateSortedAuthorityDuplicates(
			Values, CompareAuthority, [](const auto&, const auto&) { return true; },
			OutFailureIndex,
			EAngelscriptCacheValidationError::DuplicateOrdinal,
			EAngelscriptCacheValidationError::DuplicateOrdinal); !DuplicateResult.IsSuccess())
		{
			return DuplicateResult;
		}
		auto Compare = [](const FAngelscriptCachedDeclarationSlot& A, const FAngelscriptCachedDeclarationSlot& B)
		{
			if (A.SlotKind != B.SlotKind)
			{
				return static_cast<uint8>(A.SlotKind) < static_cast<uint8>(B.SlotKind) ? -1 : 1;
			}
			if (A.Ordinal == B.Ordinal)
			{
				return 0;
			}
			return A.Ordinal < B.Ordinal ? -1 : 1;
		};
		if (bCanonicalize)
		{
			Values.Sort([&](const auto& A, const auto& B) { return Compare(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			return ValidateOrder(Values, Compare, OutFailureIndex);
		}
		return {};
	}

	static FAngelscriptCachedSourceProviderKey ComputeProviderKey(
		const FAngelscriptSourceProviderIdentityInput& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-source-provider"));
		Writer.WriteUInt8(static_cast<uint8>(Value.ProviderKind));
		Writer.WriteString(Value.CanonicalImplementationIdentity);
		Writer.WriteHash(Value.IdentityFingerprint);
		return FAngelscriptCachedSourceProviderKey{Writer.FinalizeHash()};
	}

	static FAngelscriptCachedSourceMountKey ComputeMountKey(const FAngelscriptSourceMountIdentityInput& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-source-mount"));
		Writer.WriteUInt8(static_cast<uint8>(Value.SourceKind));
		Writer.WriteString(Value.LogicalMount);
		Writer.WriteHash(Value.ProviderKey.Hash);
		return FAngelscriptCachedSourceMountKey{Writer.FinalizeHash()};
	}

	static FAngelscriptCachedPreprocessHookKey ComputeHookKey(const FAngelscriptPreprocessHookIdentityInput& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-preprocess-hook"));
		Writer.WriteUInt8(static_cast<uint8>(Value.Phase));
		Writer.WriteString(Value.CanonicalImplementationIdentity);
		Writer.WriteUInt8(static_cast<uint8>(Value.AffectedScopeKind));
		Writer.WriteHash(Value.AffectedScopeStableKey);
		return FAngelscriptCachedPreprocessHookKey{Writer.FinalizeHash()};
	}

	static FAngelscriptCachedSourceFileKey ComputeSourceFileKey(const FAngelscriptSourceFileIdentityInput& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-source-file"));
		Writer.WriteUInt8(static_cast<uint8>(Value.SourceKind));
		Writer.WriteHash(Value.MountKey.Hash);
		Writer.WriteHash(Value.ProviderKey.Hash);
		Writer.WriteString(Value.RelativeLogicalPath);
		Writer.WriteBool(Value.GeneratedSourceKey.IsSet());
		if (Value.GeneratedSourceKey.IsSet())
		{
			Writer.WriteHash(Value.GeneratedSourceKey.GetValue());
		}
		return FAngelscriptCachedSourceFileKey{Writer.FinalizeHash()};
	}

	static FAngelscriptCachedPreprocessorInputKey ComputeInputKey(
		const FAngelscriptPreprocessorInputIdentityInput& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-preprocessor-input"));
		Writer.WriteHash(Value.OwnerScopeStableKey);
		Writer.WriteUInt8(static_cast<uint8>(Value.InputKind));
		Writer.WriteString(Value.CanonicalName);
		Writer.WriteBool(Value.TargetStableKey.IsSet());
		if (Value.TargetStableKey.IsSet())
		{
			Writer.WriteHash(Value.TargetStableKey.GetValue());
		}
		return FAngelscriptCachedPreprocessorInputKey{Writer.FinalizeHash()};
	}

	static FAngelscriptCachedSourceEdgeKey ComputeEdgeKey(const FAngelscriptSourceEdgeIdentityInput& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-source-edge"));
		Writer.WriteUInt8(static_cast<uint8>(Value.EdgeKind));
		Writer.WriteHash(Value.FromSourceFileKey.Hash);
		Writer.WriteHash(Value.ToSourceOrGeneratedKey);
		Writer.WriteString(Value.CanonicalIncludeOrGeneratorIdentity);
		return FAngelscriptCachedSourceEdgeKey{Writer.FinalizeHash()};
	}

	static FAngelscriptSourceProviderIdentityInput GetIdentityInput(
		const FAngelscriptCachedSourceProvider& Value)
	{
		return {Value.ProviderKind, Value.CanonicalImplementationIdentity, Value.IdentityFingerprint};
	}

	static FAngelscriptSourceMountIdentityInput GetIdentityInput(const FAngelscriptCachedSourceMount& Value)
	{
		return {Value.SourceKind, Value.LogicalMount, Value.ProviderKey};
	}

	static FAngelscriptPreprocessHookIdentityInput GetIdentityInput(
		const FAngelscriptCachedPreprocessHook& Value)
	{
		return {Value.Phase, Value.CanonicalImplementationIdentity,
			Value.AffectedScopeKind, Value.AffectedScopeStableKey};
	}

	static FAngelscriptSourceFileIdentityInput GetIdentityInput(const FAngelscriptCachedSourceFile& Value)
	{
		return {Value.SourceKind, Value.MountKey, Value.ProviderKey,
			Value.RelativeLogicalPath, Value.GeneratedSourceKey};
	}

	static FAngelscriptPreprocessorInputIdentityInput GetIdentityInput(
		const FAngelscriptCachedPreprocessorInput& Value)
	{
		return {Value.OwnerScopeStableKey, Value.InputKind, Value.CanonicalName, Value.TargetStableKey};
	}

	static FAngelscriptSourceEdgeIdentityInput GetIdentityInput(const FAngelscriptCachedSourceEdge& Value)
	{
		return {Value.EdgeKind, Value.FromSourceFileKey, Value.ToSourceOrGeneratedKey,
			Value.CanonicalIncludeOrGeneratorIdentity};
	}

	static FAngelscriptImportIdentityInput GetIdentityInput(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedImportDeclaration& Value)
	{
		return {ModuleKey, Value.CanonicalNamespace, Value.CanonicalName, Value.CanonicalSignature,
			Value.TargetModuleKey, FAngelscriptStableFunctionKey{Value.TargetDeclaration.StableKey}};
	}

	static FAngelscriptCacheValidationResult NormalizeRelativeLogicalPath(FString& InOutPath)
	{
		if (const FAngelscriptCacheValidationResult StringResult = ValidateString(InOutPath);
			!StringResult.IsSuccess())
		{
			return StringResult;
		}
		FString Normalized = InOutPath;
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
		if (Normalized.IsEmpty()
			|| Normalized.StartsWith(TEXT("/"), ESearchCase::CaseSensitive)
			|| (Normalized.Len() >= 2 && Normalized[1] == TEXT(':')))
		{
			return Failure(EAngelscriptCacheValidationError::InvalidLogicalPath);
		}

		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), true);
		TArray<FString> CanonicalSegments;
		for (FString& Segment : Segments)
		{
			if (Segment == TEXT("."))
			{
				continue;
			}
			if (Segment == TEXT(".."))
			{
				if (CanonicalSegments.IsEmpty())
				{
					return Failure(EAngelscriptCacheValidationError::InvalidLogicalPath);
				}
				CanonicalSegments.Pop(EAllowShrinking::No);
				continue;
			}
			if (!Segment.IsEmpty())
			{
				CanonicalSegments.Add(MoveTemp(Segment));
			}
		}
		Normalized = FString::Join(CanonicalSegments, TEXT("/"));
		if (Normalized.IsEmpty())
		{
			return Failure(EAngelscriptCacheValidationError::InvalidLogicalPath);
		}
		InOutPath = MoveTemp(Normalized);
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateCanonicalRelativeLogicalPath(const FString& Path)
	{
		if (const FAngelscriptCacheValidationResult StringResult = ValidateString(Path);
			!StringResult.IsSuccess())
		{
			return StringResult;
		}
		if (Path.IsEmpty() || Path[0] == TEXT('/') || Path[0] == TEXT('\\')
			|| (Path.Len() >= 2 && Path[1] == TEXT(':')))
		{
			return Failure(EAngelscriptCacheValidationError::InvalidLogicalPath);
		}

		int32 CanonicalDepth = 0;
		bool bNeedsNormalization = false;
		int32 SegmentBegin = 0;
		for (int32 Index = 0; Index <= Path.Len(); ++Index)
		{
			const bool bAtEnd = Index == Path.Len();
			const bool bBackslash = !bAtEnd && Path[Index] == TEXT('\\');
			if (!bAtEnd && Path[Index] != TEXT('/') && !bBackslash)
			{
				continue;
			}
			bNeedsNormalization |= bBackslash;
			const int32 SegmentLength = Index - SegmentBegin;
			if (SegmentLength == 0)
			{
				bNeedsNormalization = true;
			}
			else if (SegmentLength == 1 && Path[SegmentBegin] == TEXT('.'))
			{
				bNeedsNormalization = true;
			}
			else if (SegmentLength == 2 && Path[SegmentBegin] == TEXT('.')
				&& Path[SegmentBegin + 1] == TEXT('.'))
			{
				if (CanonicalDepth == 0)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidLogicalPath);
				}
				--CanonicalDepth;
				bNeedsNormalization = true;
			}
			else
			{
				++CanonicalDepth;
			}
			SegmentBegin = Index + 1;
		}
		if (CanonicalDepth == 0)
		{
			return Failure(EAngelscriptCacheValidationError::InvalidLogicalPath);
		}
		return bNeedsNormalization
			? Failure(EAngelscriptCacheValidationError::NonCanonicalOrder)
			: FAngelscriptCacheValidationResult{};
	}

	static FAngelscriptCacheValidationResult ValidateCapabilities(
		const uint32 CapabilityFlags,
		const FAngelscriptHash256& Identity,
		const TOptional<FAngelscriptHash256>& Version,
		const TOptional<FAngelscriptHash256>& Configuration,
		const TOptional<FAngelscriptHash256>& Content)
	{
		const uint32 KnownMask = static_cast<uint32>(EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		if ((CapabilityFlags & ~KnownMask) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags);
		}
		const auto ValidateRequiredOptional = [&](const EAngelscriptCachedFingerprintCapabilityFlags Flag,
			const TOptional<FAngelscriptHash256>& Optional)
		{
			const bool bFlagSet = (CapabilityFlags & static_cast<uint32>(Flag)) != 0;
			return bFlagSet == Optional.IsSet()
				&& (!Optional.IsSet() || !Optional->IsZero());
		};
		const bool bStableIdentity = (CapabilityFlags
			& static_cast<uint32>(EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity)) != 0;
		if (bStableIdentity == Identity.IsZero()
			|| !ValidateRequiredOptional(EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint, Version)
			|| !ValidateRequiredOptional(EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint, Configuration)
			|| !ValidateRequiredOptional(EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint, Content))
		{
			return Failure(EAngelscriptCacheValidationError::InvalidPresence);
		}
		return {};
	}

	static void WriteDiscoveryPolicy(FWriter& Writer, const FAngelscriptCachedSourceDiscoveryPolicy& Value)
	{
		Writer.WriteUInt32(Value.PolicyVersion);
		Writer.WriteUInt32(Value.FilterFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.Options.Num()));
		for (const FAngelscriptCachedCanonicalOption& Option : Value.Options)
		{
			WriteOption(Writer, Option);
		}
	}

	static void WriteSourceMount(FWriter& Writer, const FAngelscriptCachedSourceMount& Value)
	{
		Writer.WriteHash(Value.MountKey.Hash);
		Writer.WriteUInt8(static_cast<uint8>(Value.SourceKind));
		Writer.WriteString(Value.LogicalMount);
		Writer.WriteHash(Value.ProviderKey.Hash);
		Writer.WriteHash(Value.RootConfigurationFingerprint);
		Writer.WriteUInt32(static_cast<uint32>(Value.Options.Num()));
		for (const FAngelscriptCachedCanonicalOption& Option : Value.Options)
		{
			WriteOption(Writer, Option);
		}
	}

	static void WriteSourceProvider(FWriter& Writer, const FAngelscriptCachedSourceProvider& Value)
	{
		Writer.WriteHash(Value.ProviderKey.Hash);
		Writer.WriteUInt8(static_cast<uint8>(Value.ProviderKind));
		Writer.WriteString(Value.CanonicalImplementationIdentity);
		Writer.WriteHash(Value.IdentityFingerprint);
		Writer.WriteOptionalHash(Value.VersionFingerprint);
		Writer.WriteOptionalHash(Value.ConfigurationFingerprint);
		Writer.WriteOptionalHash(Value.ContentFingerprint);
		Writer.WriteUInt32(Value.CapabilityFlags);
	}

	static void WriteHook(FWriter& Writer, const FAngelscriptCachedPreprocessHook& Value)
	{
		Writer.WriteHash(Value.HookKey.Hash);
		Writer.WriteUInt8(static_cast<uint8>(Value.Phase));
		Writer.WriteString(Value.CanonicalImplementationIdentity);
		Writer.WriteUInt8(static_cast<uint8>(Value.AffectedScopeKind));
		Writer.WriteHash(Value.AffectedScopeStableKey);
		Writer.WriteHash(Value.IdentityFingerprint);
		Writer.WriteOptionalHash(Value.VersionFingerprint);
		Writer.WriteOptionalHash(Value.ConfigurationFingerprint);
		Writer.WriteOptionalHash(Value.ContentFingerprint);
		Writer.WriteUInt32(Value.CapabilityFlags);
	}

	static void WriteSourceFile(FWriter& Writer, const FAngelscriptCachedSourceFile& Value)
	{
		Writer.WriteHash(Value.SourceFileKey.Hash);
		Writer.WriteUInt8(static_cast<uint8>(Value.SourceKind));
		Writer.WriteHash(Value.MountKey.Hash);
		Writer.WriteHash(Value.ProviderKey.Hash);
		Writer.WriteString(Value.RelativeLogicalPath);
		Writer.WriteHash(Value.RawContentHash);
		Writer.WriteOptionalHash(Value.GeneratedSourceKey);
		Writer.WriteOptionalHash(Value.GeneratedConfigurationFingerprint);
		Writer.WriteHash(Value.ModuleKey.Hash);
	}

	static void WriteInput(FWriter& Writer, const FAngelscriptCachedPreprocessorInput& Value)
	{
		Writer.WriteHash(Value.InputKey.Hash);
		Writer.WriteUInt8(static_cast<uint8>(Value.OwnerScopeKind));
		Writer.WriteHash(Value.OwnerScopeStableKey);
		Writer.WriteUInt8(static_cast<uint8>(Value.InputKind));
		Writer.WriteString(Value.CanonicalName);
		Writer.WriteUInt8(static_cast<uint8>(Value.TargetKind));
		Writer.WriteOptionalHash(Value.TargetStableKey);
		Writer.WriteHash(Value.EffectiveValueOrContentHash);
	}

	static void WriteEdge(FWriter& Writer, const FAngelscriptCachedSourceEdge& Value)
	{
		Writer.WriteHash(Value.EdgeKey.Hash);
		Writer.WriteUInt8(static_cast<uint8>(Value.EdgeKind));
		Writer.WriteHash(Value.FromSourceFileKey.Hash);
		Writer.WriteHash(Value.ToSourceOrGeneratedKey);
		Writer.WriteString(Value.CanonicalIncludeOrGeneratorIdentity);
		Writer.WriteUInt8(Value.SemanticOrdinal.IsSet() ? 1 : 0);
		if (Value.SemanticOrdinal.IsSet())
		{
			Writer.WriteUInt32(Value.SemanticOrdinal.GetValue());
		}
	}

	static void WriteIneligible(FWriter& Writer, const FAngelscriptCachedFastPathIneligibleScope& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.ScopeKind));
		Writer.WriteHash(Value.ScopeStableKey);
		Writer.WriteUInt8(static_cast<uint8>(Value.Reason));
		Writer.WriteString(Value.CanonicalDiagnosticIdentity);
		Writer.WriteOptionalHash(Value.ObservedFingerprint);
	}

	static int32 CompareMount(const FAngelscriptCachedSourceMount& A, const FAngelscriptCachedSourceMount& B)
	{
		if (A.SourceKind != B.SourceKind)
		{
			return static_cast<uint8>(A.SourceKind) < static_cast<uint8>(B.SourceKind) ? -1 : 1;
		}
		if (const int32 LogicalCompare = CompareString(A.LogicalMount, B.LogicalMount); LogicalCompare != 0)
		{
			return LogicalCompare;
		}
		if (const int32 ProviderCompare = CompareHash(A.ProviderKey.Hash, B.ProviderKey.Hash); ProviderCompare != 0)
		{
			return ProviderCompare;
		}
		return CompareHash(A.MountKey.Hash, B.MountKey.Hash);
	}

	static int32 CompareProvider(
		const FAngelscriptCachedSourceProvider& A,
		const FAngelscriptCachedSourceProvider& B)
	{
		if (A.ProviderKind != B.ProviderKind)
		{
			return static_cast<uint8>(A.ProviderKind) < static_cast<uint8>(B.ProviderKind) ? -1 : 1;
		}
		if (const int32 IdentityCompare = CompareString(
			A.CanonicalImplementationIdentity, B.CanonicalImplementationIdentity); IdentityCompare != 0)
		{
			return IdentityCompare;
		}
		return CompareHash(A.ProviderKey.Hash, B.ProviderKey.Hash);
	}

	static int32 CompareHook(const FAngelscriptCachedPreprocessHook& A, const FAngelscriptCachedPreprocessHook& B)
	{
		if (A.Phase != B.Phase)
		{
			return static_cast<uint8>(A.Phase) < static_cast<uint8>(B.Phase) ? -1 : 1;
		}
		if (const int32 IdentityCompare = CompareString(
			A.CanonicalImplementationIdentity, B.CanonicalImplementationIdentity); IdentityCompare != 0)
		{
			return IdentityCompare;
		}
		return CompareHash(A.HookKey.Hash, B.HookKey.Hash);
	}

	static int32 CompareSourceFile(const FAngelscriptCachedSourceFile& A, const FAngelscriptCachedSourceFile& B)
	{
		if (A.SourceKind != B.SourceKind)
		{
			return static_cast<uint8>(A.SourceKind) < static_cast<uint8>(B.SourceKind) ? -1 : 1;
		}
		if (const int32 MountCompare = CompareHash(A.MountKey.Hash, B.MountKey.Hash); MountCompare != 0)
		{
			return MountCompare;
		}
		if (const int32 ProviderCompare = CompareHash(A.ProviderKey.Hash, B.ProviderKey.Hash); ProviderCompare != 0)
		{
			return ProviderCompare;
		}
		if (const int32 PathCompare = CompareString(A.RelativeLogicalPath, B.RelativeLogicalPath); PathCompare != 0)
		{
			return PathCompare;
		}
		return CompareHash(A.SourceFileKey.Hash, B.SourceFileKey.Hash);
	}

	static int32 CompareInput(
		const FAngelscriptCachedPreprocessorInput& A,
		const FAngelscriptCachedPreprocessorInput& B)
	{
		if (const int32 OwnerCompare = CompareHash(A.OwnerScopeStableKey, B.OwnerScopeStableKey); OwnerCompare != 0)
		{
			return OwnerCompare;
		}
		if (A.InputKind != B.InputKind)
		{
			return static_cast<uint8>(A.InputKind) < static_cast<uint8>(B.InputKind) ? -1 : 1;
		}
		if (const int32 NameCompare = CompareString(A.CanonicalName, B.CanonicalName); NameCompare != 0)
		{
			return NameCompare;
		}
		const FAngelscriptHash256 ATarget = A.TargetStableKey.IsSet() ? A.TargetStableKey.GetValue() : FAngelscriptHash256{};
		const FAngelscriptHash256 BTarget = B.TargetStableKey.IsSet() ? B.TargetStableKey.GetValue() : FAngelscriptHash256{};
		if (const int32 TargetCompare = CompareHash(ATarget, BTarget); TargetCompare != 0)
		{
			return TargetCompare;
		}
		return CompareHash(A.InputKey.Hash, B.InputKey.Hash);
	}

	static int32 CompareEdge(const FAngelscriptCachedSourceEdge& A, const FAngelscriptCachedSourceEdge& B)
	{
		if (A.EdgeKind != B.EdgeKind)
		{
			return static_cast<uint8>(A.EdgeKind) < static_cast<uint8>(B.EdgeKind) ? -1 : 1;
		}
		if (const int32 FromCompare = CompareHash(A.FromSourceFileKey.Hash, B.FromSourceFileKey.Hash); FromCompare != 0)
		{
			return FromCompare;
		}
		if (const int32 ToCompare = CompareHash(A.ToSourceOrGeneratedKey, B.ToSourceOrGeneratedKey); ToCompare != 0)
		{
			return ToCompare;
		}
		return CompareHash(A.EdgeKey.Hash, B.EdgeKey.Hash);
	}

	static int32 CompareIneligible(
		const FAngelscriptCachedFastPathIneligibleScope& A,
		const FAngelscriptCachedFastPathIneligibleScope& B)
	{
		if (A.ScopeKind != B.ScopeKind)
		{
			return static_cast<uint8>(A.ScopeKind) < static_cast<uint8>(B.ScopeKind) ? -1 : 1;
		}
		if (const int32 ScopeCompare = CompareHash(A.ScopeStableKey, B.ScopeStableKey); ScopeCompare != 0)
		{
			return ScopeCompare;
		}
		if (A.Reason == B.Reason)
		{
			return 0;
		}
		return static_cast<uint8>(A.Reason) < static_cast<uint8>(B.Reason) ? -1 : 1;
	}

	struct FIndexedStableHash
	{
		FAngelscriptHash256 Hash;
		int32 ValueIndex = INDEX_NONE;
	};

	struct FPreparedEligibilityIndexes
	{
		TArray<FIndexedStableHash> HookKeyIndex;
		TArray<FIndexedStableHash> ReverseHookDependencies;
	};

	static bool TryAddScratchBytes(const uint64 Bytes, uint64& InOutTotal)
	{
		if (InOutTotal > MAX_uint64 - Bytes)
		{
			return false;
		}
		InOutTotal += Bytes;
		return true;
	}

	static bool TryAddScratchElements(
		const uint64 Count,
		const uint64 ElementBytes,
		uint64& InOutTotal)
	{
		return (Count == 0 || ElementBytes <= MAX_uint64 / Count)
			&& TryAddScratchBytes(Count * ElementBytes, InOutTotal);
	}

	static bool TryComputeSourceValidationScratchBytes(
		const uint64 PayloadBytes,
		const FAngelscriptCachedSourceIndex& Value,
		uint64& OutBytes)
	{
		OutBytes = 256;
		if (!TryAddScratchElements(PayloadBytes, 2, OutBytes))
		{
			return false;
		}

		uint64 MaxSortCount = static_cast<uint64>(Value.DiscoveryPolicy.Options.Num());
		const auto ConsiderCount = [&MaxSortCount](const int32 Count)
		{
			MaxSortCount = FMath::Max(MaxSortCount, static_cast<uint64>(Count));
		};
		ConsiderCount(Value.Mounts.Num());
		ConsiderCount(Value.Providers.Num());
		ConsiderCount(Value.PreprocessHooks.Num());
		ConsiderCount(Value.Files.Num());
		ConsiderCount(Value.PreprocessorInputs.Num());
		ConsiderCount(Value.Edges.Num());
		ConsiderCount(Value.IneligibleScopes.Num());
		for (const FAngelscriptCachedSourceMount& Mount : Value.Mounts)
		{
			ConsiderCount(Mount.Options.Num());
		}

		return TryAddScratchElements(
			static_cast<uint64>(Value.IneligibleScopes.Num()), sizeof(int32), OutBytes)
			&& TryAddScratchElements(
				static_cast<uint64>(Value.Providers.Num()), sizeof(FIndexedStableHash), OutBytes)
			&& TryAddScratchElements(
				static_cast<uint64>(Value.Mounts.Num()), sizeof(FIndexedStableHash), OutBytes)
			&& TryAddScratchElements(
				static_cast<uint64>(Value.PreprocessHooks.Num()), sizeof(FIndexedStableHash), OutBytes)
			&& TryAddScratchElements(
				static_cast<uint64>(Value.Files.Num()) * 3, sizeof(FIndexedStableHash), OutBytes)
			&& TryAddScratchElements(MaxSortCount, sizeof(FIndexedStableHash), OutBytes);
	}

	static bool TryComputeModuleValidationScratchBytes(
		const uint64 PayloadBytes,
		const FAngelscriptCachedModuleInterface& Value,
		uint64& OutBytes)
	{
		OutBytes = 256;
		if (!TryAddScratchElements(PayloadBytes, 2, OutBytes))
		{
			return false;
		}

		uint64 MaxSortCount = static_cast<uint64>(Value.CanonicalNamespaces.Num());
		uint64 TotalSlotCount = 0;
		const auto ConsiderCount = [&MaxSortCount](const int32 Count)
		{
			MaxSortCount = FMath::Max(MaxSortCount, static_cast<uint64>(Count));
		};
		ConsiderCount(Value.Declarations.Num());
		ConsiderCount(Value.Imports.Num());
		ConsiderCount(Value.Dependencies.Num());
		for (const FAngelscriptCachedDeclaration& Declaration : Value.Declarations)
		{
			ConsiderCount(Declaration.Metadata.Num());
			ConsiderCount(Declaration.OrderedParameters.Num());
			ConsiderCount(Declaration.Slots.Num());
			if (!TryAddScratchBytes(static_cast<uint64>(Declaration.Slots.Num()), TotalSlotCount))
			{
				return false;
			}
		}
		for (const FAngelscriptCachedImportDeclaration& Import : Value.Imports)
		{
			ConsiderCount(Import.Slots.Num());
			if (!TryAddScratchBytes(static_cast<uint64>(Import.Slots.Num()), TotalSlotCount))
			{
				return false;
			}
		}

		return TryAddScratchElements(
			static_cast<uint64>(Value.Declarations.Num()), sizeof(FIndexedStableHash), OutBytes)
			&& TryAddScratchElements(
				static_cast<uint64>(Value.CanonicalNamespaces.Num()), sizeof(uint8), OutBytes)
			&& TryAddScratchElements(TotalSlotCount, sizeof(uint32), OutBytes)
			&& TryAddScratchElements(MaxSortCount, sizeof(FIndexedStableHash), OutBytes);
	}

	template <typename ElementType>
	static bool TryAddArrayReserveScratchBytes(
		const int32 RequestedCapacity,
		uint64& OutBytes)
	{
		if (RequestedCapacity < 0)
		{
			return false;
		}
		if (RequestedCapacity == 0)
		{
			return true;
		}

		using FArrayType = TArray<ElementType>;
		typename FArrayType::ElementAllocatorType Allocator;
		int32 ReservedCapacity = 0;
		if constexpr (TAllocatorTraits<typename FArrayType::AllocatorType>::SupportsElementAlignment)
		{
			ReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			ReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType));
		}
		if (ReservedCapacity < RequestedCapacity)
		{
			return false;
		}

		return TryAddScratchElements(
			static_cast<uint64>(ReservedCapacity), sizeof(ElementType), OutBytes);
	}

	static bool TryComputeEligibilityScratchBytes(
		const FAngelscriptCachedSourceIndex& Value,
		const bool bIndexesAlreadyPrepared,
		uint64& OutBytes)
	{
		OutBytes = 0;
		if (!(TryAddArrayReserveScratchBytes<FAngelscriptHash256>(
				Value.Files.Num(), OutBytes)
			&& TryAddArrayReserveScratchBytes<FAngelscriptHash256>(Value.Files.Num(), OutBytes)
			&& TryAddArrayReserveScratchBytes<FAngelscriptHash256>(Value.Files.Num(), OutBytes)
			&& TryAddArrayReserveScratchBytes<uint8>(Value.PreprocessHooks.Num(), OutBytes)
			&& TryAddArrayReserveScratchBytes<int32>(Value.PreprocessHooks.Num(), OutBytes)))
		{
			return false;
		}
		return bIndexesAlreadyPrepared
			|| (TryAddArrayReserveScratchBytes<FIndexedStableHash>(
					Value.PreprocessHooks.Num(), OutBytes)
				&& TryAddArrayReserveScratchBytes<FIndexedStableHash>(
					Value.PreprocessHooks.Num(), OutBytes));
	}

	static bool TryComputeEligibilityBatchPreparationScratchBytes(
		const FAngelscriptCachedSourceIndex& Value,
		const int32 ModuleCount,
		uint64& OutBytes)
	{
		OutBytes = 0;
		return TryAddArrayReserveScratchBytes<FAngelscriptStableModuleKey>(
				ModuleCount, OutBytes)
			&& TryAddArrayReserveScratchBytes<FIndexedStableHash>(
				Value.PreprocessHooks.Num(), OutBytes)
			&& TryAddArrayReserveScratchBytes<FIndexedStableHash>(
				Value.PreprocessHooks.Num(), OutBytes)
			&& TryAddArrayReserveScratchBytes<
				FAngelscriptCacheTemporaryResidentReservation>(
					ModuleCount, OutBytes);
	}

	template <typename ElementType, typename KeyFunctionType>
	static void BuildStableHashIndex(
		const TArray<ElementType>& Values,
		KeyFunctionType&& GetKey,
		TArray<FIndexedStableHash>& OutIndex)
	{
		OutIndex.Reset();
		OutIndex.Reserve(Values.Num());
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			OutIndex.Add({GetKey(Values[Index]), Index});
		}
		OutIndex.Sort([](const FIndexedStableHash& A, const FIndexedStableHash& B)
		{
			const int32 HashCompare = CompareHash(A.Hash, B.Hash);
			return HashCompare != 0 ? HashCompare < 0 : A.ValueIndex < B.ValueIndex;
		});
	}

	static int32 FindStableHashValueIndex(
		const TArray<FIndexedStableHash>& Index,
		const FAngelscriptHash256& Key)
	{
		int32 Lower = 0;
		int32 Upper = Index.Num();
		while (Lower < Upper)
		{
			const int32 Middle = Lower + (Upper - Lower) / 2;
			if (CompareHash(Index[Middle].Hash, Key) < 0)
			{
				Lower = Middle + 1;
			}
			else
			{
				Upper = Middle;
			}
		}
		return Lower < Index.Num() && Index[Lower].Hash == Key
			? Index[Lower].ValueIndex
			: INDEX_NONE;
	}

	template <typename ElementType, typename KeyFunctionType, typename EqualFunctionType>
	static FAngelscriptCacheValidationResult ValidateStableHashAuthorityDuplicates(
		const TArray<ElementType>& Values,
		KeyFunctionType&& GetKey,
		EqualFunctionType&& EqualContent,
		int32* OutFailureIndex = nullptr)
	{
		if (OutFailureIndex != nullptr)
		{
			*OutFailureIndex = INDEX_NONE;
		}
		TArray<FIndexedStableHash> Index;
		BuildStableHashIndex(Values, Forward<KeyFunctionType>(GetKey), Index);
		int32 FirstDuplicateValueIndex = INDEX_NONE;
		int32 SecondDuplicateValueIndex = INDEX_NONE;
		for (int32 GroupBegin = 0; GroupBegin < Index.Num();)
		{
			int32 GroupEnd = GroupBegin + 1;
			while (GroupEnd < Index.Num() && Index[GroupEnd].Hash == Index[GroupBegin].Hash)
			{
				++GroupEnd;
			}
			if (GroupEnd - GroupBegin >= 2
				&& (SecondDuplicateValueIndex == INDEX_NONE
					|| Index[GroupBegin + 1].ValueIndex < SecondDuplicateValueIndex))
			{
				FirstDuplicateValueIndex = Index[GroupBegin].ValueIndex;
				SecondDuplicateValueIndex = Index[GroupBegin + 1].ValueIndex;
			}
			GroupBegin = GroupEnd;
		}
		if (SecondDuplicateValueIndex != INDEX_NONE)
		{
			if (OutFailureIndex != nullptr)
			{
				*OutFailureIndex = SecondDuplicateValueIndex;
			}
			return Failure(EqualContent(
				Values[FirstDuplicateValueIndex], Values[SecondDuplicateValueIndex])
				? EAngelscriptCacheValidationError::DuplicateKey
				: EAngelscriptCacheValidationError::ConflictingKey);
		}
		return {};
	}

	static bool SameOptionalHash(
		const TOptional<FAngelscriptHash256>& A,
		const TOptional<FAngelscriptHash256>& B)
	{
		return A.IsSet() == B.IsSet() && (!A.IsSet() || A.GetValue() == B.GetValue());
	}

	static bool EqualOptions(
		const TArray<FAngelscriptCachedCanonicalOption>& A,
		const TArray<FAngelscriptCachedCanonicalOption>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].CanonicalKey != B[Index].CanonicalKey
				|| !(A[Index].ValueFingerprint == B[Index].ValueFingerprint))
			{
				return false;
			}
		}
		return true;
	}

	static FAngelscriptHash256 HashSourceIndex(const FAngelscriptCachedSourceIndex& Value);

	// Factory semantic validation borrows the immutable candidate's captured
	// coordinate store through these allocation-free views. The view deliberately
	// knows neither the private storage layout nor the canonical payload bytes, so
	// semantic code cannot grow a second scanner or offset model.
	struct FSourceIndexSemanticOffsetView final
	{
		const void* Context = nullptr;
		TOptional<uint64> (*Find)(
			const void*, const FAngelscriptSourceIndexFieldCoordinate&) = nullptr;

		TOptional<uint64> FindOffset(
			const FAngelscriptSourceIndexFieldCoordinate& Coordinate) const
		{
			return Context != nullptr && Find != nullptr
				? Find(Context, Coordinate)
				: TOptional<uint64>{};
		}
	};

	struct FModuleInterfaceSemanticOffsetView final
	{
		const void* Context = nullptr;
		TOptional<uint64> (*Find)(
			const void*, const FAngelscriptModuleInterfaceFieldCoordinate&) = nullptr;

		TOptional<uint64> FindOffset(
			const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate) const
		{
			return Context != nullptr && Find != nullptr
				? Find(Context, Coordinate)
				: TOptional<uint64>{};
		}
	};

	static FAngelscriptCacheValidationResult PrepareSourceIndex(
		FAngelscriptCachedSourceIndex& Value,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		const bool bValidateStoredSnapshot,
		const FSourceIndexSemanticOffsetView* ExactOffsets = nullptr)
	{
		uint64 SemanticFieldOffset = 0;
		const auto SetExactOffset = [&](const FAngelscriptSourceIndexFieldCoordinate& Coordinate)
		{
			if (ExactOffsets == nullptr)
			{
				return;
			}
			const TOptional<uint64> Offset = ExactOffsets->FindOffset(Coordinate);
			check(Offset.IsSet());
			SemanticFieldOffset = Offset.GetValue();
		};
		const auto Prepare = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptSourceIndexCapturedField::PayloadSchemaVersion});
		if (Value.PayloadSchemaVersion != FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion)
		{
			return Failure(EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
		SetExactOffset({EAngelscriptSourceIndexCapturedField::DiscoveryPolicyVersion});
		if (Value.DiscoveryPolicy.PolicyVersion == 0)
		{
			return Failure(EAngelscriptCacheValidationError::InvalidPresence,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
		SetExactOffset({EAngelscriptSourceIndexCapturedField::DiscoveryPolicyFilterFlags});
		if ((Value.DiscoveryPolicy.FilterFlags
			& ~static_cast<uint32>(EAngelscriptCachedSourceDiscoveryFilterFlags::KnownMask)) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
		int32 DiscoveryOptionFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult OptionsResult = PrepareOptions(
			Value.DiscoveryPolicy.Options,
			bCanonicalize,
			bRequireCanonicalOrder,
			&DiscoveryOptionFailureIndex);
			!OptionsResult.IsSuccess())
		{
			if (DiscoveryOptionFailureIndex != INDEX_NONE)
			{
				SetExactOffset({
					EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOption,
					static_cast<uint32>(DiscoveryOptionFailureIndex)});
			}
			return OptionsResult;
		}
		TArray<FIndexedStableHash> ProviderScalarIndex;
		TArray<int32> IneligibleAuthorityIndex;
		const auto BuildIneligibleAuthorityIndex = [&]()
		{
			IneligibleAuthorityIndex.Reset();
			IneligibleAuthorityIndex.Reserve(Value.IneligibleScopes.Num());
			for (int32 Index = 0; Index < Value.IneligibleScopes.Num(); ++Index)
			{
				IneligibleAuthorityIndex.Add(Index);
			}
			IneligibleAuthorityIndex.Sort([&](const int32 AIndex, const int32 BIndex)
			{
				const int32 Compare = CompareIneligible(
					Value.IneligibleScopes[AIndex], Value.IneligibleScopes[BIndex]);
				return Compare != 0 ? Compare < 0 : AIndex < BIndex;
			});
		};
		const auto HasIndexedIneligibleReason = [&] (
			const EAngelscriptCachedFastPathScopeKind ScopeKind,
			const FAngelscriptHash256& ScopeKey,
			const EAngelscriptCachedFastPathIneligibleReason Reason)
		{
			int32 Lower = 0;
			int32 Upper = IneligibleAuthorityIndex.Num();
			while (Lower < Upper)
			{
				const int32 Middle = Lower + (Upper - Lower) / 2;
				const FAngelscriptCachedFastPathIneligibleScope& Candidate =
					Value.IneligibleScopes[IneligibleAuthorityIndex[Middle]];
				int32 Compare = static_cast<uint8>(Candidate.ScopeKind) - static_cast<uint8>(ScopeKind);
				if (Compare == 0) { Compare = CompareHash(Candidate.ScopeStableKey, ScopeKey); }
				if (Compare == 0)
				{
					Compare = static_cast<uint8>(Candidate.Reason) - static_cast<uint8>(Reason);
				}
				if (Compare < 0) { Lower = Middle + 1; }
				else { Upper = Middle; }
			}
			if (Lower >= IneligibleAuthorityIndex.Num()) { return false; }
			const FAngelscriptCachedFastPathIneligibleScope& Candidate =
				Value.IneligibleScopes[IneligibleAuthorityIndex[Lower]];
			return Candidate.ScopeKind == ScopeKind
				&& Candidate.ScopeStableKey == ScopeKey
				&& Candidate.Reason == Reason;
		};
		struct FMissingCapability
		{
			EAngelscriptCachedFingerprintCapabilityFlags Flag;
			EAngelscriptCachedFastPathIneligibleReason Reason;
		};
		const FMissingCapability ProviderCapabilities[] = {
			{EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity,
				EAngelscriptCachedFastPathIneligibleReason::MissingStableIdentity},
			{EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint},
			{EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingConfigurationFingerprint},
			{EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::UnstableGeneratedSource}};
		const FMissingCapability HookCapabilities[] = {
			{EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity,
				EAngelscriptCachedFastPathIneligibleReason::MissingStableIdentity},
			{EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingVersionFingerprint},
			{EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::MissingConfigurationFingerprint},
			{EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint,
				EAngelscriptCachedFastPathIneligibleReason::UnknownHookBehavior}};

		const auto PrepareIneligibleScopes = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptSourceIndexCapturedField::IneligibleScopes});
		for (int32 ScopeIndex = 0; ScopeIndex < Value.IneligibleScopes.Num(); ++ScopeIndex)
		{
			FAngelscriptCachedFastPathIneligibleScope& Scope =
				Value.IneligibleScopes[ScopeIndex];
			SetExactOffset({EAngelscriptSourceIndexCapturedField::IneligibleScopeKind,
				static_cast<uint32>(ScopeIndex)});
			if (static_cast<uint8>(Scope.ScopeKind) < 1 || static_cast<uint8>(Scope.ScopeKind) > 5
				|| static_cast<uint8>(Scope.Reason) < 1 || static_cast<uint8>(Scope.Reason) > 5)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::IneligibleScopeStableKey,
				static_cast<uint32>(ScopeIndex)});
			if (Scope.ScopeStableKey.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset({
				EAngelscriptSourceIndexCapturedField::IneligibleScopeCanonicalDiagnosticIdentity,
				static_cast<uint32>(ScopeIndex)});
			if (const FAngelscriptCacheValidationResult StringResult =
				ValidateRequiredString(Scope.CanonicalDiagnosticIdentity); !StringResult.IsSuccess())
			{
				return StringResult;
			}
			if (Scope.ObservedFingerprint.IsSet())
			{
				SetExactOffset({
					EAngelscriptSourceIndexCapturedField::IneligibleScopeObservedFingerprint,
					static_cast<uint32>(ScopeIndex)});
			}
			if (Scope.ObservedFingerprint.IsSet() && Scope.ObservedFingerprint->IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
		}
		int32 IneligibleFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateSortedAuthorityDuplicates(
			Value.IneligibleScopes,
			[](const auto& A, const auto& B) { return CompareIneligible(A, B); },
			[](const auto& A, const auto& B)
			{
				return A.CanonicalDiagnosticIdentity == B.CanonicalDiagnosticIdentity
					&& SameOptionalHash(A.ObservedFingerprint, B.ObservedFingerprint);
			}, &IneligibleFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptSourceIndexCapturedField::IneligibleScope,
				static_cast<uint32>(IneligibleFailureIndex)});
			return DuplicateResult;
		}
		if (bCanonicalize)
		{
			Value.IneligibleScopes.Sort([](const auto& A, const auto& B) { return CompareIneligible(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			if (const FAngelscriptCacheValidationResult OrderResult =
				ValidateOrder(Value.IneligibleScopes, CompareIneligible,
					&IneligibleFailureIndex); !OrderResult.IsSuccess())
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::IneligibleScope,
					static_cast<uint32>(IneligibleFailureIndex)});
				return OrderResult;
			}
		}
		return {};
		};

		const auto PrepareProviders = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptSourceIndexCapturedField::Providers});
		for (int32 ProviderIndex = 0; ProviderIndex < Value.Providers.Num(); ++ProviderIndex)
		{
			FAngelscriptCachedSourceProvider& Provider = Value.Providers[ProviderIndex];
			if (static_cast<uint8>(Provider.ProviderKind) < 1 || static_cast<uint8>(Provider.ProviderKind) > 4)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			if (const FAngelscriptCacheValidationResult StringResult =
				ValidateRequiredString(Provider.CanonicalImplementationIdentity); !StringResult.IsSuccess())
			{
				return StringResult;
			}
			if (ExactOffsets != nullptr)
			{
				const TOptional<uint64> Offset = ExactOffsets->FindOffset({
					EAngelscriptSourceIndexCapturedField::ProviderCapabilityFlags,
					static_cast<uint32>(ProviderIndex)});
				check(Offset.IsSet());
				SemanticFieldOffset = Offset.GetValue();
			}
			if (const FAngelscriptCacheValidationResult CapabilityResult = ValidateCapabilities(
				Provider.CapabilityFlags, Provider.IdentityFingerprint, Provider.VersionFingerprint,
				Provider.ConfigurationFingerprint, Provider.ContentFingerprint); !CapabilityResult.IsSuccess())
			{
				return CapabilityResult;
			}
			FAngelscriptCachedSourceProviderKey ComputedProviderKey;
			SetExactOffset({EAngelscriptSourceIndexCapturedField::ProviderKey,
				static_cast<uint32>(ProviderIndex)});
			const FAngelscriptCacheValidationResult ProviderKeyResult =
				FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
					GetIdentityInput(Provider), ComputedProviderKey);
			if (!ProviderKeyResult.IsSuccess())
			{
				return ProviderKeyResult;
			}
			if (!(Provider.ProviderKey.Hash == ComputedProviderKey.Hash))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
			}
			for (const FMissingCapability& Capability : ProviderCapabilities)
			{
				const bool bCapabilitySet =
					(Provider.CapabilityFlags & static_cast<uint32>(Capability.Flag)) != 0;
				const bool bReasonPresent = HasIndexedIneligibleReason(
					EAngelscriptCachedFastPathScopeKind::Provider,
					Provider.ProviderKey.Hash, Capability.Reason);
				if (bCapabilitySet == bReasonPresent)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidPresence);
				}
			}
		}
		int32 ProviderFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateStableHashAuthorityDuplicates(
			Value.Providers,
			[](const auto& Item) { return Item.ProviderKey.Hash; },
			[](const auto& A, const auto& B)
			{
				return A.ProviderKind == B.ProviderKind
					&& A.CanonicalImplementationIdentity == B.CanonicalImplementationIdentity
					&& A.IdentityFingerprint == B.IdentityFingerprint
					&& SameOptionalHash(A.VersionFingerprint, B.VersionFingerprint)
					&& SameOptionalHash(A.ConfigurationFingerprint, B.ConfigurationFingerprint)
					&& SameOptionalHash(A.ContentFingerprint, B.ContentFingerprint)
					&& A.CapabilityFlags == B.CapabilityFlags;
			}, &ProviderFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptSourceIndexCapturedField::Provider,
				static_cast<uint32>(ProviderFailureIndex)});
			return DuplicateResult;
		}
		if (bCanonicalize)
		{
			Value.Providers.Sort([](const auto& A, const auto& B) { return CompareProvider(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			if (const FAngelscriptCacheValidationResult OrderResult = ValidateOrder(
				Value.Providers, CompareProvider, &ProviderFailureIndex);
				!OrderResult.IsSuccess())
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::Provider,
					static_cast<uint32>(ProviderFailureIndex)});
				return OrderResult;
			}
		}
		return {};
		};

		const auto PrepareMounts = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptSourceIndexCapturedField::Mounts});
		for (int32 MountIndex = 0; MountIndex < Value.Mounts.Num(); ++MountIndex)
		{
			FAngelscriptCachedSourceMount& Mount = Value.Mounts[MountIndex];
			SetExactOffset({EAngelscriptSourceIndexCapturedField::MountSourceKind,
				static_cast<uint32>(MountIndex)});
			if (static_cast<uint8>(Mount.SourceKind) < 1 || static_cast<uint8>(Mount.SourceKind) > 3)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::MountProviderKey,
				static_cast<uint32>(MountIndex)});
			if (Mount.ProviderKey.Hash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset({
				EAngelscriptSourceIndexCapturedField::MountRootConfigurationFingerprint,
				static_cast<uint32>(MountIndex)});
			if (Mount.RootConfigurationFingerprint.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::MountLogicalMount,
				static_cast<uint32>(MountIndex)});
			if (const FAngelscriptCacheValidationResult StringResult = ValidateRequiredString(Mount.LogicalMount);
				!StringResult.IsSuccess())
			{
				return StringResult;
			}
			int32 MountOptionFailureIndex = INDEX_NONE;
			if (const FAngelscriptCacheValidationResult OptionsResult = PrepareOptions(
				Mount.Options,
				bCanonicalize,
				bRequireCanonicalOrder,
				&MountOptionFailureIndex);
				!OptionsResult.IsSuccess())
			{
				if (MountOptionFailureIndex != INDEX_NONE)
				{
					SetExactOffset({
						EAngelscriptSourceIndexCapturedField::MountOption,
						static_cast<uint32>(MountIndex),
						static_cast<uint32>(MountOptionFailureIndex)});
				}
				return OptionsResult;
			}
			FAngelscriptCachedSourceMountKey ComputedMountKey;
			SetExactOffset({EAngelscriptSourceIndexCapturedField::MountKey,
				static_cast<uint32>(MountIndex)});
			const FAngelscriptCacheValidationResult MountKeyResult =
				FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
					GetIdentityInput(Mount), ComputedMountKey);
			if (!MountKeyResult.IsSuccess())
			{
				return MountKeyResult;
			}
			if (!(Mount.MountKey.Hash == ComputedMountKey.Hash))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
			}
		}
		int32 MountFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateStableHashAuthorityDuplicates(
			Value.Mounts,
			[](const auto& Item) { return Item.MountKey.Hash; },
			[](const auto& A, const auto& B)
			{
				return A.SourceKind == B.SourceKind && A.LogicalMount == B.LogicalMount
					&& A.ProviderKey.Hash == B.ProviderKey.Hash
					&& A.RootConfigurationFingerprint == B.RootConfigurationFingerprint
					&& EqualOptions(A.Options, B.Options);
			}, &MountFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptSourceIndexCapturedField::Mount,
				static_cast<uint32>(MountFailureIndex)});
			return DuplicateResult;
		}
		if (bCanonicalize)
		{
			Value.Mounts.Sort([](const auto& A, const auto& B) { return CompareMount(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			if (const FAngelscriptCacheValidationResult OrderResult = ValidateOrder(
				Value.Mounts, CompareMount, &MountFailureIndex);
				!OrderResult.IsSuccess())
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::Mount,
					static_cast<uint32>(MountFailureIndex)});
				return OrderResult;
			}
		}
		for (int32 Index = 1; Index < Value.Mounts.Num(); ++Index)
		{
			const FAngelscriptCachedSourceMount& A = Value.Mounts[Index - 1];
			const FAngelscriptCachedSourceMount& B = Value.Mounts[Index];
			if (A.SourceKind == B.SourceKind
				&& A.LogicalMount == B.LogicalMount
				&& A.ProviderKey.Hash == B.ProviderKey.Hash
				&& !(A.MountKey.Hash == B.MountKey.Hash))
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::Mount,
					static_cast<uint32>(Index)});
				return Failure(EAngelscriptCacheValidationError::ConflictingKey);
			}
		}
		return {};
		};

		const auto PrepareHooks = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptSourceIndexCapturedField::PreprocessHooks});
		for (int32 HookIndex = 0; HookIndex < Value.PreprocessHooks.Num(); ++HookIndex)
		{
			FAngelscriptCachedPreprocessHook& Hook = Value.PreprocessHooks[HookIndex];
			SetExactOffset({EAngelscriptSourceIndexCapturedField::HookPhase,
				static_cast<uint32>(HookIndex)});
			if (static_cast<uint8>(Hook.Phase) < 1 || static_cast<uint8>(Hook.Phase) > 3
				|| static_cast<uint8>(Hook.AffectedScopeKind) < 1
				|| static_cast<uint8>(Hook.AffectedScopeKind) > 5)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::HookAffectedScopeStableKey,
				static_cast<uint32>(HookIndex)});
			if (Hook.AffectedScopeStableKey.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			if (const FAngelscriptCacheValidationResult StringResult =
				ValidateRequiredString(Hook.CanonicalImplementationIdentity); !StringResult.IsSuccess())
			{
				return StringResult;
			}
			if (const FAngelscriptCacheValidationResult CapabilityResult = ValidateCapabilities(
				Hook.CapabilityFlags, Hook.IdentityFingerprint, Hook.VersionFingerprint,
				Hook.ConfigurationFingerprint, Hook.ContentFingerprint); !CapabilityResult.IsSuccess())
			{
				return CapabilityResult;
			}
			FAngelscriptCachedPreprocessHookKey ComputedHookKey;
			SetExactOffset({EAngelscriptSourceIndexCapturedField::HookKey,
				static_cast<uint32>(HookIndex)});
			const FAngelscriptCacheValidationResult HookKeyResult =
				FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(
					GetIdentityInput(Hook), ComputedHookKey);
			if (!HookKeyResult.IsSuccess())
			{
				return HookKeyResult;
			}
			if (!(Hook.HookKey.Hash == ComputedHookKey.Hash))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
			}
			for (const FMissingCapability& Capability : HookCapabilities)
			{
				const bool bCapabilitySet =
					(Hook.CapabilityFlags & static_cast<uint32>(Capability.Flag)) != 0;
				const bool bReasonPresent = HasIndexedIneligibleReason(
					EAngelscriptCachedFastPathScopeKind::Hook,
					Hook.HookKey.Hash, Capability.Reason);
				if (bCapabilitySet == bReasonPresent)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidPresence);
				}
			}
		}
		int32 HookFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateStableHashAuthorityDuplicates(
			Value.PreprocessHooks,
			[](const auto& Item) { return Item.HookKey.Hash; },
			[](const auto& A, const auto& B)
			{
				return A.Phase == B.Phase
					&& A.CanonicalImplementationIdentity == B.CanonicalImplementationIdentity
					&& A.AffectedScopeKind == B.AffectedScopeKind
					&& A.AffectedScopeStableKey == B.AffectedScopeStableKey
					&& A.IdentityFingerprint == B.IdentityFingerprint
					&& SameOptionalHash(A.VersionFingerprint, B.VersionFingerprint)
					&& SameOptionalHash(A.ConfigurationFingerprint, B.ConfigurationFingerprint)
					&& SameOptionalHash(A.ContentFingerprint, B.ContentFingerprint)
					&& A.CapabilityFlags == B.CapabilityFlags;
			}, &HookFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptSourceIndexCapturedField::PreprocessHook,
				static_cast<uint32>(HookFailureIndex)});
			return DuplicateResult;
		}
		if (bCanonicalize)
		{
			Value.PreprocessHooks.Sort([](const auto& A, const auto& B) { return CompareHook(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			if (const FAngelscriptCacheValidationResult OrderResult =
				ValidateOrder(Value.PreprocessHooks, CompareHook, &HookFailureIndex);
				!OrderResult.IsSuccess())
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::PreprocessHook,
					static_cast<uint32>(HookFailureIndex)});
				return OrderResult;
			}
		}
		return {};
		};

		const auto PrepareFiles = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptSourceIndexCapturedField::Files});
		for (int32 FileIndex = 0; FileIndex < Value.Files.Num(); ++FileIndex)
		{
			FAngelscriptCachedSourceFile& File = Value.Files[FileIndex];
			SetExactOffset({EAngelscriptSourceIndexCapturedField::FileSourceKind,
				static_cast<uint32>(FileIndex)});
			if (static_cast<uint8>(File.SourceKind) < 1 || static_cast<uint8>(File.SourceKind) > 3)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::FileMountKey,
				static_cast<uint32>(FileIndex)});
			if (File.MountKey.Hash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::FileProviderKey,
				static_cast<uint32>(FileIndex)});
			if (File.ProviderKey.Hash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::FileRawContentHash,
				static_cast<uint32>(FileIndex)});
			if (File.RawContentHash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::FileModuleKey,
				static_cast<uint32>(FileIndex)});
			if (File.ModuleKey.Hash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			const FAngelscriptCacheValidationResult PathResult = bCanonicalize
				? NormalizeRelativeLogicalPath(File.RelativeLogicalPath)
				: ValidateCanonicalRelativeLogicalPath(File.RelativeLogicalPath);
			if (!PathResult.IsSuccess())
			{
				return PathResult;
			}
			if (File.GeneratedSourceKey.IsSet() != File.GeneratedConfigurationFingerprint.IsSet()
				|| (File.GeneratedSourceKey.IsSet()
					&& (File.GeneratedSourceKey->IsZero() || File.GeneratedConfigurationFingerprint->IsZero())))
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			const int32 ProviderIndex = FindStableHashValueIndex(
				ProviderScalarIndex, File.ProviderKey.Hash);
			if (ProviderIndex != INDEX_NONE
				&& (Value.Providers[ProviderIndex].ProviderKind
					== EAngelscriptCachedSourceProviderKind::Generated) != File.GeneratedSourceKey.IsSet())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			FAngelscriptCachedSourceFileKey ComputedSourceFileKey;
			SetExactOffset({EAngelscriptSourceIndexCapturedField::FileSourceFileKey,
				static_cast<uint32>(FileIndex)});
			const FAngelscriptCacheValidationResult SourceFileKeyResult =
				FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(
					GetIdentityInput(File), ComputedSourceFileKey);
			if (!SourceFileKeyResult.IsSuccess())
			{
				return SourceFileKeyResult;
			}
			if (!(File.SourceFileKey.Hash == ComputedSourceFileKey.Hash))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
			}
		}
		int32 FileFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateStableHashAuthorityDuplicates(
			Value.Files,
			[](const auto& Item) { return Item.SourceFileKey.Hash; },
			[](const auto& A, const auto& B)
			{
				return A.SourceKind == B.SourceKind
					&& A.MountKey.Hash == B.MountKey.Hash
					&& A.ProviderKey.Hash == B.ProviderKey.Hash
					&& A.RelativeLogicalPath == B.RelativeLogicalPath
					&& A.RawContentHash == B.RawContentHash
					&& SameOptionalHash(A.GeneratedSourceKey, B.GeneratedSourceKey)
					&& SameOptionalHash(
						A.GeneratedConfigurationFingerprint, B.GeneratedConfigurationFingerprint)
					&& A.ModuleKey.Hash == B.ModuleKey.Hash;
			}, &FileFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptSourceIndexCapturedField::File,
				static_cast<uint32>(FileFailureIndex)});
			return DuplicateResult;
		}

		TArray<FIndexedStableHash> GeneratedIndex;
		GeneratedIndex.Reserve(Value.Files.Num());
		for (int32 Index = 0; Index < Value.Files.Num(); ++Index)
		{
			if (Value.Files[Index].GeneratedSourceKey.IsSet())
			{
				GeneratedIndex.Add({Value.Files[Index].GeneratedSourceKey.GetValue(), Index});
			}
		}
		GeneratedIndex.Sort([](const FIndexedStableHash& A, const FIndexedStableHash& B)
		{
			const int32 HashCompare = CompareHash(A.Hash, B.Hash);
			return HashCompare != 0 ? HashCompare < 0 : A.ValueIndex < B.ValueIndex;
		});
		for (int32 Index = 1; Index < GeneratedIndex.Num(); ++Index)
		{
			if (GeneratedIndex[Index - 1].Hash == GeneratedIndex[Index].Hash)
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::File,
					static_cast<uint32>(GeneratedIndex[Index].ValueIndex)});
				return Failure(EAngelscriptCacheValidationError::ConflictingKey);
			}
		}

		TArray<int32> FoldedPathIndex;
		FoldedPathIndex.Reserve(Value.Files.Num());
		for (int32 Index = 0; Index < Value.Files.Num(); ++Index) { FoldedPathIndex.Add(Index); }
		FoldedPathIndex.Sort([&Value](const int32 AIndex, const int32 BIndex)
		{
			const FAngelscriptCachedSourceFile& A = Value.Files[AIndex];
			const FAngelscriptCachedSourceFile& B = Value.Files[BIndex];
			const int32 MountCompare = CompareHash(A.MountKey.Hash, B.MountKey.Hash);
			if (MountCompare != 0) { return MountCompare < 0; }
			const int32 PathCompare = CompareUnicodeSimpleFold(A.RelativeLogicalPath, B.RelativeLogicalPath);
			if (PathCompare != 0) { return PathCompare < 0; }
			return CompareHash(A.SourceFileKey.Hash, B.SourceFileKey.Hash) < 0;
		});
		for (int32 Index = 1; Index < FoldedPathIndex.Num(); ++Index)
		{
			const FAngelscriptCachedSourceFile& A = Value.Files[FoldedPathIndex[Index - 1]];
			const FAngelscriptCachedSourceFile& B = Value.Files[FoldedPathIndex[Index]];
			if (A.MountKey.Hash == B.MountKey.Hash
				&& CompareUnicodeSimpleFold(A.RelativeLogicalPath, B.RelativeLogicalPath) == 0)
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::File,
					static_cast<uint32>(FoldedPathIndex[Index])});
				return Failure(EAngelscriptCacheValidationError::CaseCollision);
			}
		}
		if (bCanonicalize)
		{
			Value.Files.Sort([](const auto& A, const auto& B) { return CompareSourceFile(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			if (const FAngelscriptCacheValidationResult OrderResult =
				ValidateOrder(Value.Files, CompareSourceFile, &FileFailureIndex);
				!OrderResult.IsSuccess())
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::File,
					static_cast<uint32>(FileFailureIndex)});
				return OrderResult;
			}
		}
		return {};
		};

		const auto PrepareInputs = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptSourceIndexCapturedField::PreprocessorInputs});
		for (int32 InputIndex = 0;
			InputIndex < Value.PreprocessorInputs.Num(); ++InputIndex)
		{
			FAngelscriptCachedPreprocessorInput& Input =
				Value.PreprocessorInputs[InputIndex];
			SetExactOffset({EAngelscriptSourceIndexCapturedField::InputOwnerScopeKind,
				static_cast<uint32>(InputIndex)});
			if (static_cast<uint8>(Input.OwnerScopeKind) < 1 || static_cast<uint8>(Input.OwnerScopeKind) > 5
				|| static_cast<uint8>(Input.InputKind) < 1 || static_cast<uint8>(Input.InputKind) > 4
				|| static_cast<uint8>(Input.TargetKind) > 5)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::InputOwnerScopeStableKey,
				static_cast<uint32>(InputIndex)});
			if (Input.OwnerScopeStableKey.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset({
				EAngelscriptSourceIndexCapturedField::InputEffectiveValueOrContentHash,
				static_cast<uint32>(InputIndex)});
			if (Input.EffectiveValueOrContentHash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			if (const FAngelscriptCacheValidationResult StringResult = ValidateRequiredString(Input.CanonicalName);
				!StringResult.IsSuccess())
			{
				return StringResult;
			}
			const bool bTargetRequired = Input.TargetKind != EAngelscriptCachePreprocessorInputTargetKind::None;
			SetExactOffset({
				Input.TargetStableKey.IsSet()
					? EAngelscriptSourceIndexCapturedField::InputTargetStableKey
					: EAngelscriptSourceIndexCapturedField::InputTargetStableKeyPresence,
				static_cast<uint32>(InputIndex)});
			if (bTargetRequired != Input.TargetStableKey.IsSet()
				|| (Input.TargetStableKey.IsSet() && Input.TargetStableKey->IsZero()))
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			FAngelscriptCachedPreprocessorInputKey ComputedInputKey;
			SetExactOffset({EAngelscriptSourceIndexCapturedField::InputKey,
				static_cast<uint32>(InputIndex)});
			const FAngelscriptCacheValidationResult InputKeyResult =
				FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(
					GetIdentityInput(Input), ComputedInputKey);
			if (!InputKeyResult.IsSuccess())
			{
				return InputKeyResult;
			}
			if (!(Input.InputKey.Hash == ComputedInputKey.Hash))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
			}
		}
		int32 InputFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateStableHashAuthorityDuplicates(
			Value.PreprocessorInputs,
			[](const auto& Item) { return Item.InputKey.Hash; },
			[](const auto& A, const auto& B)
			{
				return A.OwnerScopeKind == B.OwnerScopeKind
					&& A.OwnerScopeStableKey == B.OwnerScopeStableKey
					&& A.InputKind == B.InputKind && A.CanonicalName == B.CanonicalName
					&& A.TargetKind == B.TargetKind
					&& SameOptionalHash(A.TargetStableKey, B.TargetStableKey)
					&& A.EffectiveValueOrContentHash == B.EffectiveValueOrContentHash;
			}, &InputFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptSourceIndexCapturedField::PreprocessorInput,
				static_cast<uint32>(InputFailureIndex)});
			return DuplicateResult;
		}
		if (bCanonicalize)
		{
			Value.PreprocessorInputs.Sort([](const auto& A, const auto& B) { return CompareInput(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			if (const FAngelscriptCacheValidationResult OrderResult =
				ValidateOrder(Value.PreprocessorInputs, CompareInput, &InputFailureIndex);
				!OrderResult.IsSuccess())
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::PreprocessorInput,
					static_cast<uint32>(InputFailureIndex)});
				return OrderResult;
			}
		}
		return {};
		};

		const auto PrepareEdges = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptSourceIndexCapturedField::Edges});
		for (int32 EdgeIndex = 0; EdgeIndex < Value.Edges.Num(); ++EdgeIndex)
		{
			FAngelscriptCachedSourceEdge& Edge = Value.Edges[EdgeIndex];
			SetExactOffset({EAngelscriptSourceIndexCapturedField::EdgeKind,
				static_cast<uint32>(EdgeIndex)});
			if (static_cast<uint8>(Edge.EdgeKind) < 1 || static_cast<uint8>(Edge.EdgeKind) > 2)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::EdgeFromSourceFileKey,
				static_cast<uint32>(EdgeIndex)});
			if (Edge.FromSourceFileKey.Hash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset({EAngelscriptSourceIndexCapturedField::EdgeToSourceOrGeneratedKey,
				static_cast<uint32>(EdgeIndex)});
			if (Edge.ToSourceOrGeneratedKey.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			if (const FAngelscriptCacheValidationResult StringResult =
				ValidateRequiredString(Edge.CanonicalIncludeOrGeneratorIdentity); !StringResult.IsSuccess())
			{
				return StringResult;
			}
			FAngelscriptCachedSourceEdgeKey ComputedEdgeKey;
			SetExactOffset({EAngelscriptSourceIndexCapturedField::EdgeKey,
				static_cast<uint32>(EdgeIndex)});
			const FAngelscriptCacheValidationResult EdgeKeyResult =
				FAngelscriptCacheSemanticArchive::TryBuildSourceEdgeKey(
					GetIdentityInput(Edge), ComputedEdgeKey);
			if (!EdgeKeyResult.IsSuccess())
			{
				return EdgeKeyResult;
			}
			if (!(Edge.EdgeKey.Hash == ComputedEdgeKey.Hash))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
			}
		}
		int32 EdgeFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateStableHashAuthorityDuplicates(
			Value.Edges,
			[](const auto& Item) { return Item.EdgeKey.Hash; },
			[](const auto& A, const auto& B)
			{
				return A.EdgeKind == B.EdgeKind
					&& A.FromSourceFileKey.Hash == B.FromSourceFileKey.Hash
					&& A.ToSourceOrGeneratedKey == B.ToSourceOrGeneratedKey
					&& A.CanonicalIncludeOrGeneratorIdentity == B.CanonicalIncludeOrGeneratorIdentity
					&& A.SemanticOrdinal.IsSet() == B.SemanticOrdinal.IsSet()
					&& (!A.SemanticOrdinal.IsSet() || A.SemanticOrdinal.GetValue() == B.SemanticOrdinal.GetValue());
			}, &EdgeFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptSourceIndexCapturedField::Edge,
				static_cast<uint32>(EdgeFailureIndex)});
			return DuplicateResult;
		}
		if (bCanonicalize)
		{
			Value.Edges.Sort([](const auto& A, const auto& B) { return CompareEdge(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			if (const FAngelscriptCacheValidationResult OrderResult = ValidateOrder(
				Value.Edges, CompareEdge, &EdgeFailureIndex);
				!OrderResult.IsSuccess())
			{
				SetExactOffset({EAngelscriptSourceIndexCapturedField::Edge,
					static_cast<uint32>(EdgeFailureIndex)});
				return OrderResult;
			}
		}
		for (int32 GroupBegin = 0; GroupBegin < Value.Edges.Num();)
		{
			int32 GroupEnd = GroupBegin + 1;
			while (GroupEnd < Value.Edges.Num()
				&& Value.Edges[GroupEnd].EdgeKind == Value.Edges[GroupBegin].EdgeKind
				&& Value.Edges[GroupEnd].FromSourceFileKey.Hash
					== Value.Edges[GroupBegin].FromSourceFileKey.Hash)
			{
				++GroupEnd;
			}
			struct FEdgeOrdinalOccurrence final
			{
				uint32 Ordinal = 0;
				int32 EdgeIndex = INDEX_NONE;
			};
			TArray<FEdgeOrdinalOccurrence> GroupOrdinals;
			GroupOrdinals.Reserve(GroupEnd - GroupBegin);
			const bool bExpectedPresence =
				Value.Edges[GroupBegin].SemanticOrdinal.IsSet();
			for (int32 Index = GroupBegin; Index < GroupEnd; ++Index)
			{
				const bool bPresent = Value.Edges[Index].SemanticOrdinal.IsSet();
				if (bPresent != bExpectedPresence)
				{
					SetExactOffset({
						EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinalPresence,
						static_cast<uint32>(Index)});
					return Failure(EAngelscriptCacheValidationError::InvalidPresence);
				}
				if (bPresent)
				{
					GroupOrdinals.Add({
						Value.Edges[Index].SemanticOrdinal.GetValue(), Index});
				}
			}
			GroupOrdinals.Sort([](const auto& A, const auto& B)
			{
				return A.Ordinal != B.Ordinal
					? A.Ordinal < B.Ordinal
					: A.EdgeIndex < B.EdgeIndex;
			});
			for (int32 OrdinalIndex = 0; OrdinalIndex < GroupOrdinals.Num(); ++OrdinalIndex)
			{
				const FEdgeOrdinalOccurrence& Occurrence = GroupOrdinals[OrdinalIndex];
				if (OrdinalIndex > 0
					&& Occurrence.Ordinal == GroupOrdinals[OrdinalIndex - 1].Ordinal)
				{
					SetExactOffset({EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinal,
						static_cast<uint32>(Occurrence.EdgeIndex)});
					return Failure(EAngelscriptCacheValidationError::DuplicateOrdinal);
				}
				if (Occurrence.Ordinal != static_cast<uint32>(OrdinalIndex))
				{
					SetExactOffset({EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinal,
						static_cast<uint32>(Occurrence.EdgeIndex)});
					return Failure(EAngelscriptCacheValidationError::OrdinalGap);
				}
			}
			GroupBegin = GroupEnd;
		}
		return {};
		};

		FAngelscriptCacheValidationResult CollectionResult = PrepareMounts();
		if (!CollectionResult.IsSuccess()) { return CollectionResult; }
		BuildIneligibleAuthorityIndex();
		CollectionResult = PrepareProviders();
		if (!CollectionResult.IsSuccess()) { return CollectionResult; }
		BuildStableHashIndex(Value.Providers,
			[](const auto& Item) { return Item.ProviderKey.Hash; }, ProviderScalarIndex);
		CollectionResult = PrepareHooks();
		if (!CollectionResult.IsSuccess()) { return CollectionResult; }
		CollectionResult = PrepareFiles();
		if (!CollectionResult.IsSuccess()) { return CollectionResult; }
		CollectionResult = PrepareInputs();
		if (!CollectionResult.IsSuccess()) { return CollectionResult; }
		CollectionResult = PrepareEdges();
		if (!CollectionResult.IsSuccess()) { return CollectionResult; }
		CollectionResult = PrepareIneligibleScopes();
		if (!CollectionResult.IsSuccess()) { return CollectionResult; }

		TArray<FIndexedStableHash> MountIndex;
		TArray<FIndexedStableHash> HookIndex;
		TArray<FIndexedStableHash> FileIndex;
		TArray<FIndexedStableHash> GeneratedSourceIndex;
		TArray<FIndexedStableHash> ModuleIndex;
		BuildStableHashIndex(Value.Mounts,
			[](const auto& Item) { return Item.MountKey.Hash; }, MountIndex);
		BuildStableHashIndex(Value.PreprocessHooks,
			[](const auto& Item) { return Item.HookKey.Hash; }, HookIndex);
		BuildStableHashIndex(Value.Files,
			[](const auto& Item) { return Item.SourceFileKey.Hash; }, FileIndex);
		BuildStableHashIndex(Value.Files,
			[](const auto& Item) { return Item.ModuleKey.Hash; }, ModuleIndex);
		GeneratedSourceIndex.Reserve(Value.Files.Num());
		for (int32 Index = 0; Index < Value.Files.Num(); ++Index)
		{
			if (Value.Files[Index].GeneratedSourceKey.IsSet())
			{
				GeneratedSourceIndex.Add({Value.Files[Index].GeneratedSourceKey.GetValue(), Index});
			}
		}
		GeneratedSourceIndex.Sort([](const FIndexedStableHash& A, const FIndexedStableHash& B)
		{
			const int32 HashCompare = CompareHash(A.Hash, B.Hash);
			return HashCompare != 0 ? HashCompare < 0 : A.ValueIndex < B.ValueIndex;
		});

		auto ContainsIndexKey = [](const TArray<FIndexedStableHash>& Index, const FAngelscriptHash256& Key)
		{
			return FindStableHashValueIndex(Index, Key) != INDEX_NONE;
		};
		auto ContainsAnyTypedKey = [&](const FAngelscriptHash256& Key)
		{
			return ContainsIndexKey(MountIndex, Key)
				|| ContainsIndexKey(ProviderScalarIndex, Key)
				|| ContainsIndexKey(HookIndex, Key)
				|| ContainsIndexKey(FileIndex, Key)
				|| ContainsIndexKey(GeneratedSourceIndex, Key)
				|| ContainsIndexKey(ModuleIndex, Key);
		};
		auto ValidateRequiredIndex = [&](
			const TArray<FIndexedStableHash>& RequiredIndex,
			const FAngelscriptHash256& Key)
		{
			if (ContainsIndexKey(RequiredIndex, Key)) { return FAngelscriptCacheValidationResult{}; }
			return Failure(ContainsAnyTypedKey(Key)
				? EAngelscriptCacheValidationError::WrongReferenceKind
				: EAngelscriptCacheValidationError::MissingGraphTarget,
				EAngelscriptCacheRecordKind::SourceIndex);
		};
		auto ValidateScope = [&](
			const EAngelscriptCachedFastPathScopeKind Kind,
			const FAngelscriptHash256& Key)
		{
			switch (Kind)
			{
			case EAngelscriptCachedFastPathScopeKind::Mount: return ValidateRequiredIndex(MountIndex, Key);
			case EAngelscriptCachedFastPathScopeKind::Provider: return ValidateRequiredIndex(ProviderScalarIndex, Key);
			case EAngelscriptCachedFastPathScopeKind::Hook: return ValidateRequiredIndex(HookIndex, Key);
			case EAngelscriptCachedFastPathScopeKind::SourceFile: return ValidateRequiredIndex(FileIndex, Key);
			case EAngelscriptCachedFastPathScopeKind::Module: return ValidateRequiredIndex(ModuleIndex, Key);
			default: return Failure(EAngelscriptCacheValidationError::UnknownEnumValue,
				EAngelscriptCacheRecordKind::SourceIndex);
			}
		};

		SetExactOffset({EAngelscriptSourceIndexCapturedField::Mounts});
		for (const FAngelscriptCachedSourceMount& Mount : Value.Mounts)
		{
			const FAngelscriptCacheValidationResult ReferenceResult =
				ValidateRequiredIndex(ProviderScalarIndex, Mount.ProviderKey.Hash);
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
		}
		SetExactOffset({EAngelscriptSourceIndexCapturedField::PreprocessHooks});
		for (const FAngelscriptCachedPreprocessHook& Hook : Value.PreprocessHooks)
		{
			const FAngelscriptCacheValidationResult ReferenceResult =
				ValidateScope(Hook.AffectedScopeKind, Hook.AffectedScopeStableKey);
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
		}
		SetExactOffset({EAngelscriptSourceIndexCapturedField::Files});
		for (const FAngelscriptCachedSourceFile& File : Value.Files)
		{
			FAngelscriptCacheValidationResult ReferenceResult =
				ValidateRequiredIndex(MountIndex, File.MountKey.Hash);
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
			ReferenceResult = ValidateRequiredIndex(ProviderScalarIndex, File.ProviderKey.Hash);
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
		}
		SetExactOffset({EAngelscriptSourceIndexCapturedField::PreprocessorInputs});
		for (const FAngelscriptCachedPreprocessorInput& Input : Value.PreprocessorInputs)
		{
			FAngelscriptCacheValidationResult ReferenceResult =
				ValidateScope(Input.OwnerScopeKind, Input.OwnerScopeStableKey);
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
			if (Input.TargetKind == EAngelscriptCachePreprocessorInputTargetKind::None) { continue; }
			const TArray<FIndexedStableHash>* RequiredIndex = nullptr;
			switch (Input.TargetKind)
			{
			case EAngelscriptCachePreprocessorInputTargetKind::SourceFile: RequiredIndex = &FileIndex; break;
			case EAngelscriptCachePreprocessorInputTargetKind::Provider: RequiredIndex = &ProviderScalarIndex; break;
			case EAngelscriptCachePreprocessorInputTargetKind::Hook: RequiredIndex = &HookIndex; break;
			case EAngelscriptCachePreprocessorInputTargetKind::Module: RequiredIndex = &ModuleIndex; break;
			case EAngelscriptCachePreprocessorInputTargetKind::GeneratedSource:
				RequiredIndex = &GeneratedSourceIndex; break;
			default: break;
			}
			ReferenceResult = ValidateRequiredIndex(*RequiredIndex, Input.TargetStableKey.GetValue());
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
		}
		SetExactOffset({EAngelscriptSourceIndexCapturedField::Edges});
		for (const FAngelscriptCachedSourceEdge& Edge : Value.Edges)
		{
			FAngelscriptCacheValidationResult ReferenceResult =
				ValidateRequiredIndex(FileIndex, Edge.FromSourceFileKey.Hash);
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
			ReferenceResult = ValidateRequiredIndex(
				Edge.EdgeKind == EAngelscriptCachedSourceEdgeKind::Include
					? FileIndex : GeneratedSourceIndex,
				Edge.ToSourceOrGeneratedKey);
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
		}
		SetExactOffset({EAngelscriptSourceIndexCapturedField::IneligibleScopes});
		for (const FAngelscriptCachedFastPathIneligibleScope& Scope : Value.IneligibleScopes)
		{
			const FAngelscriptCacheValidationResult ReferenceResult =
				ValidateScope(Scope.ScopeKind, Scope.ScopeStableKey);
			if (!ReferenceResult.IsSuccess()) { return ReferenceResult; }
		}
		SetExactOffset({EAngelscriptSourceIndexCapturedField::Files});
		for (const FAngelscriptCachedSourceFile& File : Value.Files)
		{
			const int32 MountValueIndex = FindStableHashValueIndex(MountIndex, File.MountKey.Hash);
			const FAngelscriptCachedSourceMount& ResolvedMount = Value.Mounts[MountValueIndex];
			if (File.SourceKind != ResolvedMount.SourceKind
				|| !(File.ProviderKey.Hash == ResolvedMount.ProviderKey.Hash))
			{
				return Failure(EAngelscriptCacheValidationError::ConflictingKey,
					EAngelscriptCacheRecordKind::SourceIndex);
			}
		}

		if (bValidateStoredSnapshot)
		{
			SetExactOffset({EAngelscriptSourceIndexCapturedField::SourceSnapshot});
			const FAngelscriptHash256 Computed = HashSourceIndex(Value);
			if (!(Computed == Value.SourceSnapshot))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch,
					EAngelscriptCacheRecordKind::SourceIndex);
			}
		}
		return {};
		};
		const FAngelscriptCacheValidationResult Result = Prepare();
		if (!Result.IsSuccess())
		{
			return Failure(Result.Error, EAngelscriptCacheRecordKind::SourceIndex,
				ExactOffsets != nullptr ? SemanticFieldOffset : 0);
		}
		return Result;
	}

	static void WriteSourceIndexPayload(FWriter& Writer, const FAngelscriptCachedSourceIndex& Value)
	{
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteHash(Value.SourceSnapshot);
		WriteDiscoveryPolicy(Writer, Value.DiscoveryPolicy);
		Writer.WriteUInt32(static_cast<uint32>(Value.Mounts.Num()));
		for (const auto& Item : Value.Mounts) { WriteSourceMount(Writer, Item); }
		Writer.WriteUInt32(static_cast<uint32>(Value.Providers.Num()));
		for (const auto& Item : Value.Providers) { WriteSourceProvider(Writer, Item); }
		Writer.WriteUInt32(static_cast<uint32>(Value.PreprocessHooks.Num()));
		for (const auto& Item : Value.PreprocessHooks) { WriteHook(Writer, Item); }
		Writer.WriteUInt32(static_cast<uint32>(Value.Files.Num()));
		for (const auto& Item : Value.Files) { WriteSourceFile(Writer, Item); }
		Writer.WriteUInt32(static_cast<uint32>(Value.PreprocessorInputs.Num()));
		for (const auto& Item : Value.PreprocessorInputs) { WriteInput(Writer, Item); }
		Writer.WriteUInt32(static_cast<uint32>(Value.Edges.Num()));
		for (const auto& Item : Value.Edges) { WriteEdge(Writer, Item); }
		Writer.WriteUInt32(static_cast<uint32>(Value.IneligibleScopes.Num()));
		for (const auto& Item : Value.IneligibleScopes) { WriteIneligible(Writer, Item); }
	}

	static void WriteOptionalHash(FAngelscriptArtifactCanonicalWriter& Writer, const TOptional<FAngelscriptHash256>& Value)
	{
		Writer.WriteBool(Value.IsSet());
		if (Value.IsSet()) { Writer.WriteHash(Value.GetValue()); }
	}

	static FAngelscriptHash256 HashSourceIndex(const FAngelscriptCachedSourceIndex& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-source-snapshot-v1"));
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteUInt32(Value.DiscoveryPolicy.PolicyVersion);
		Writer.WriteUInt32(Value.DiscoveryPolicy.FilterFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.DiscoveryPolicy.Options.Num()));
		for (const auto& Option : Value.DiscoveryPolicy.Options)
		{
			Writer.WriteString(Option.CanonicalKey); Writer.WriteHash(Option.ValueFingerprint);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Mounts.Num()));
		for (const auto& Mount : Value.Mounts)
		{
			Writer.WriteHash(Mount.MountKey.Hash); Writer.WriteUInt8(static_cast<uint8>(Mount.SourceKind));
			Writer.WriteString(Mount.LogicalMount); Writer.WriteHash(Mount.ProviderKey.Hash);
			Writer.WriteHash(Mount.RootConfigurationFingerprint);
			Writer.WriteUInt32(static_cast<uint32>(Mount.Options.Num()));
			for (const auto& Option : Mount.Options)
			{
				Writer.WriteString(Option.CanonicalKey); Writer.WriteHash(Option.ValueFingerprint);
			}
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Providers.Num()));
		for (const auto& Provider : Value.Providers)
		{
			Writer.WriteHash(Provider.ProviderKey.Hash); Writer.WriteUInt8(static_cast<uint8>(Provider.ProviderKind));
			Writer.WriteString(Provider.CanonicalImplementationIdentity); Writer.WriteHash(Provider.IdentityFingerprint);
			WriteOptionalHash(Writer, Provider.VersionFingerprint);
			WriteOptionalHash(Writer, Provider.ConfigurationFingerprint);
			WriteOptionalHash(Writer, Provider.ContentFingerprint);
			Writer.WriteUInt32(Provider.CapabilityFlags);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.PreprocessHooks.Num()));
		for (const auto& Hook : Value.PreprocessHooks)
		{
			Writer.WriteHash(Hook.HookKey.Hash); Writer.WriteUInt8(static_cast<uint8>(Hook.Phase));
			Writer.WriteString(Hook.CanonicalImplementationIdentity);
			Writer.WriteUInt8(static_cast<uint8>(Hook.AffectedScopeKind)); Writer.WriteHash(Hook.AffectedScopeStableKey);
			Writer.WriteHash(Hook.IdentityFingerprint); WriteOptionalHash(Writer, Hook.VersionFingerprint);
			WriteOptionalHash(Writer, Hook.ConfigurationFingerprint); WriteOptionalHash(Writer, Hook.ContentFingerprint);
			Writer.WriteUInt32(Hook.CapabilityFlags);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Files.Num()));
		for (const auto& File : Value.Files)
		{
			Writer.WriteHash(File.SourceFileKey.Hash); Writer.WriteUInt8(static_cast<uint8>(File.SourceKind));
			Writer.WriteHash(File.MountKey.Hash); Writer.WriteHash(File.ProviderKey.Hash);
			Writer.WriteString(File.RelativeLogicalPath); Writer.WriteHash(File.RawContentHash);
			WriteOptionalHash(Writer, File.GeneratedSourceKey);
			WriteOptionalHash(Writer, File.GeneratedConfigurationFingerprint);
			Writer.WriteHash(File.ModuleKey.Hash);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.PreprocessorInputs.Num()));
		for (const auto& Input : Value.PreprocessorInputs)
		{
			Writer.WriteHash(Input.InputKey.Hash); Writer.WriteUInt8(static_cast<uint8>(Input.OwnerScopeKind));
			Writer.WriteHash(Input.OwnerScopeStableKey); Writer.WriteUInt8(static_cast<uint8>(Input.InputKind));
			Writer.WriteString(Input.CanonicalName); Writer.WriteUInt8(static_cast<uint8>(Input.TargetKind));
			WriteOptionalHash(Writer, Input.TargetStableKey); Writer.WriteHash(Input.EffectiveValueOrContentHash);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Edges.Num()));
		for (const auto& Edge : Value.Edges)
		{
			Writer.WriteHash(Edge.EdgeKey.Hash); Writer.WriteUInt8(static_cast<uint8>(Edge.EdgeKind));
			Writer.WriteHash(Edge.FromSourceFileKey.Hash); Writer.WriteHash(Edge.ToSourceOrGeneratedKey);
			Writer.WriteString(Edge.CanonicalIncludeOrGeneratorIdentity); Writer.WriteBool(Edge.SemanticOrdinal.IsSet());
			if (Edge.SemanticOrdinal.IsSet()) { Writer.WriteUInt32(Edge.SemanticOrdinal.GetValue()); }
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.IneligibleScopes.Num()));
		for (const auto& Scope : Value.IneligibleScopes)
		{
			Writer.WriteUInt8(static_cast<uint8>(Scope.ScopeKind)); Writer.WriteHash(Scope.ScopeStableKey);
			Writer.WriteUInt8(static_cast<uint8>(Scope.Reason)); Writer.WriteString(Scope.CanonicalDiagnosticIdentity);
			WriteOptionalHash(Writer, Scope.ObservedFingerprint);
		}
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 HashDirectSourceInputs(
		const FAngelscriptCachedSourceIndex& Value,
		const FAngelscriptArtifactProfileKey& Profile)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-direct-source-inputs-v1"));
		Writer.WriteUInt32(1);
		Writer.WriteHash(Profile.Hash);
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteUInt32(Value.DiscoveryPolicy.PolicyVersion);
		Writer.WriteUInt32(Value.DiscoveryPolicy.FilterFlags);
		Writer.WriteUInt32(static_cast<uint32>(
			Value.DiscoveryPolicy.Options.Num()));
		for (const auto& Option : Value.DiscoveryPolicy.Options)
		{
			Writer.WriteString(Option.CanonicalKey);
			Writer.WriteHash(Option.ValueFingerprint);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Mounts.Num()));
		for (const auto& Mount : Value.Mounts)
		{
			Writer.WriteHash(Mount.MountKey.Hash);
			Writer.WriteUInt8(static_cast<uint8>(Mount.SourceKind));
			Writer.WriteString(Mount.LogicalMount);
			Writer.WriteHash(Mount.ProviderKey.Hash);
			Writer.WriteHash(Mount.RootConfigurationFingerprint);
			Writer.WriteUInt32(static_cast<uint32>(Mount.Options.Num()));
			for (const auto& Option : Mount.Options)
			{
				Writer.WriteString(Option.CanonicalKey);
				Writer.WriteHash(Option.ValueFingerprint);
			}
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Providers.Num()));
		for (const auto& Provider : Value.Providers)
		{
			Writer.WriteHash(Provider.ProviderKey.Hash);
			Writer.WriteUInt8(static_cast<uint8>(Provider.ProviderKind));
			Writer.WriteString(Provider.CanonicalImplementationIdentity);
			Writer.WriteHash(Provider.IdentityFingerprint);
			WriteOptionalHash(Writer, Provider.VersionFingerprint);
			WriteOptionalHash(Writer, Provider.ConfigurationFingerprint);
			WriteOptionalHash(Writer, Provider.ContentFingerprint);
			Writer.WriteUInt32(Provider.CapabilityFlags);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.PreprocessHooks.Num()));
		for (const auto& Hook : Value.PreprocessHooks)
		{
			Writer.WriteHash(Hook.HookKey.Hash);
			Writer.WriteUInt8(static_cast<uint8>(Hook.Phase));
			Writer.WriteString(Hook.CanonicalImplementationIdentity);
			Writer.WriteUInt8(static_cast<uint8>(Hook.AffectedScopeKind));
			Writer.WriteHash(Hook.AffectedScopeStableKey);
			Writer.WriteHash(Hook.IdentityFingerprint);
			WriteOptionalHash(Writer, Hook.VersionFingerprint);
			WriteOptionalHash(Writer, Hook.ConfigurationFingerprint);
			WriteOptionalHash(Writer, Hook.ContentFingerprint);
			Writer.WriteUInt32(Hook.CapabilityFlags);
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Files.Num()));
		for (const auto& File : Value.Files)
		{
			Writer.WriteHash(File.SourceFileKey.Hash);
			Writer.WriteUInt8(static_cast<uint8>(File.SourceKind));
			Writer.WriteHash(File.MountKey.Hash);
			Writer.WriteHash(File.ProviderKey.Hash);
			Writer.WriteString(File.RelativeLogicalPath);
			Writer.WriteHash(File.RawContentHash);
			WriteOptionalHash(Writer, File.GeneratedSourceKey);
			WriteOptionalHash(
				Writer, File.GeneratedConfigurationFingerprint);
			Writer.WriteHash(File.ModuleKey.Hash);
		}
		return Writer.FinalizeHash();
	}

	template <typename ElementType, typename ReadElementType>
	static bool ReadArray(
		FReader& Reader,
		TArray<ElementType>& OutValues,
		const uint64 MinimumElementBytes,
		ReadElementType&& ReadElement)
	{
		uint32 Count = 0;
		if (!Reader.ReadArrayCountAndReserve(MinimumElementBytes, OutValues, Count))
		{
			return false;
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			ElementType Value;
			if (!ReadElement(Reader, Value))
			{
				return false;
			}
			OutValues.Add(MoveTemp(Value));
		}
		return true;
	}

	static bool ReadDiscoveryPolicy(FReader& Reader, FAngelscriptCachedSourceDiscoveryPolicy& OutValue)
	{
		return Reader.ReadUInt32(OutValue.PolicyVersion)
			&& Reader.ReadUInt32(OutValue.FilterFlags)
			&& ReadArray(Reader, OutValue.Options, 40, ReadOption);
	}

	static bool ReadSourceMount(FReader& Reader, FAngelscriptCachedSourceMount& OutValue)
	{
		return Reader.ReadHash(OutValue.MountKey.Hash)
			&& ReadEnum(Reader, OutValue.SourceKind, 1, 3)
			&& Reader.ReadString(OutValue.LogicalMount)
			&& Reader.ReadHash(OutValue.ProviderKey.Hash)
			&& Reader.ReadHash(OutValue.RootConfigurationFingerprint)
			&& ReadArray(Reader, OutValue.Options, 40, ReadOption);
	}

	static bool ReadSourceProvider(FReader& Reader, FAngelscriptCachedSourceProvider& OutValue)
	{
		return Reader.ReadHash(OutValue.ProviderKey.Hash)
			&& ReadEnum(Reader, OutValue.ProviderKind, 1, 4)
			&& Reader.ReadString(OutValue.CanonicalImplementationIdentity)
			&& Reader.ReadHash(OutValue.IdentityFingerprint)
			&& Reader.ReadOptionalHash(OutValue.VersionFingerprint)
			&& Reader.ReadOptionalHash(OutValue.ConfigurationFingerprint)
			&& Reader.ReadOptionalHash(OutValue.ContentFingerprint)
			&& Reader.ReadUInt32(OutValue.CapabilityFlags);
	}

	static bool ReadHook(FReader& Reader, FAngelscriptCachedPreprocessHook& OutValue)
	{
		return Reader.ReadHash(OutValue.HookKey.Hash)
			&& ReadEnum(Reader, OutValue.Phase, 1, 3)
			&& Reader.ReadString(OutValue.CanonicalImplementationIdentity)
			&& ReadEnum(Reader, OutValue.AffectedScopeKind, 1, 5)
			&& Reader.ReadHash(OutValue.AffectedScopeStableKey)
			&& Reader.ReadHash(OutValue.IdentityFingerprint)
			&& Reader.ReadOptionalHash(OutValue.VersionFingerprint)
			&& Reader.ReadOptionalHash(OutValue.ConfigurationFingerprint)
			&& Reader.ReadOptionalHash(OutValue.ContentFingerprint)
			&& Reader.ReadUInt32(OutValue.CapabilityFlags);
	}

	static bool ReadSourceFile(FReader& Reader, FAngelscriptCachedSourceFile& OutValue)
	{
		return Reader.ReadHash(OutValue.SourceFileKey.Hash)
			&& ReadEnum(Reader, OutValue.SourceKind, 1, 3)
			&& Reader.ReadHash(OutValue.MountKey.Hash)
			&& Reader.ReadHash(OutValue.ProviderKey.Hash)
			&& Reader.ReadString(OutValue.RelativeLogicalPath)
			&& Reader.ReadHash(OutValue.RawContentHash)
			&& Reader.ReadOptionalHash(OutValue.GeneratedSourceKey)
			&& Reader.ReadOptionalHash(OutValue.GeneratedConfigurationFingerprint)
			&& Reader.ReadHash(OutValue.ModuleKey.Hash);
	}

	static bool ReadInput(FReader& Reader, FAngelscriptCachedPreprocessorInput& OutValue)
	{
		if (!Reader.ReadHash(OutValue.InputKey.Hash)
			|| !ReadEnum(Reader, OutValue.OwnerScopeKind, 1, 5)
			|| !Reader.ReadHash(OutValue.OwnerScopeStableKey)
			|| !ReadEnum(Reader, OutValue.InputKind, 1, 4)
			|| !Reader.ReadString(OutValue.CanonicalName))
		{
			return false;
		}
		uint8 TargetKind = 0;
		const uint64 TargetOffset = Reader.GetOffset();
		if (!Reader.ReadUInt8(TargetKind))
		{
			return false;
		}
		if (TargetKind > 5)
		{
			Reader.Fail(EAngelscriptCacheValidationError::UnknownEnumValue, TargetOffset);
			return false;
		}
		OutValue.TargetKind = static_cast<EAngelscriptCachePreprocessorInputTargetKind>(TargetKind);
		return Reader.ReadOptionalHash(OutValue.TargetStableKey)
			&& Reader.ReadHash(OutValue.EffectiveValueOrContentHash);
	}

	static bool ReadOptionalOrdinal(FReader& Reader, TOptional<uint32>& OutValue)
	{
		OutValue.Reset();
		uint8 Tag = 0;
		const uint64 Offset = Reader.GetOffset();
		if (!Reader.ReadUInt8(Tag))
		{
			return false;
		}
		if (Tag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, Offset);
			return false;
		}
		if (Tag == 1)
		{
			uint32 Ordinal = 0;
			if (!Reader.ReadUInt32(Ordinal))
			{
				return false;
			}
			OutValue = Ordinal;
		}
		return true;
	}

	static bool ReadEdge(FReader& Reader, FAngelscriptCachedSourceEdge& OutValue)
	{
		return Reader.ReadHash(OutValue.EdgeKey.Hash)
			&& ReadEnum(Reader, OutValue.EdgeKind, 1, 2)
			&& Reader.ReadHash(OutValue.FromSourceFileKey.Hash)
			&& Reader.ReadHash(OutValue.ToSourceOrGeneratedKey)
			&& Reader.ReadString(OutValue.CanonicalIncludeOrGeneratorIdentity)
			&& ReadOptionalOrdinal(Reader, OutValue.SemanticOrdinal);
	}

	static bool ReadIneligible(FReader& Reader, FAngelscriptCachedFastPathIneligibleScope& OutValue)
	{
		return ReadEnum(Reader, OutValue.ScopeKind, 1, 5)
			&& Reader.ReadHash(OutValue.ScopeStableKey)
			&& ReadEnum(Reader, OutValue.Reason, 1, 5)
			&& Reader.ReadString(OutValue.CanonicalDiagnosticIdentity)
			&& Reader.ReadOptionalHash(OutValue.ObservedFingerprint);
	}

	template <typename EntryType, typename CoordinateType>
	static void CaptureOffset(
		EntryType& OutEntry,
		const CoordinateType& Coordinate,
		const uint64 Offset)
	{
		OutEntry.Coordinate = Coordinate;
		OutEntry.Offset = Offset;
	}

	template <typename EntryType>
	static void CaptureOffset(
		EntryType& OutEntry,
		const FAngelscriptSourceIndexFieldCoordinate& Coordinate,
		const uint64 Offset)
	{
		OutEntry.Coordinate = Coordinate;
		OutEntry.Offset = Offset;
	}

	template <typename EntryType>
	static void CaptureOffset(
		EntryType& OutEntry,
		const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate,
		const uint64 Offset)
	{
		OutEntry.Coordinate = Coordinate;
		OutEntry.Offset = Offset;
	}

	template <typename ElementType, typename OffsetType, typename ReadElementType>
	static bool ReadCapturedArray(
		FReader& Reader,
		TArray<ElementType>& OutValues,
		TArray<OffsetType>& OutOffsets,
		const uint64 MinimumElementBytes,
		const uint64 CountOffset,
		ReadElementType&& ReadElement)
	{
		uint32 Count = 0;
		if (!Reader.ReadArrayCountAndReserve(MinimumElementBytes, OutValues, Count)
			|| !Reader.ReserveDecodedArrayAtOffset(CountOffset, Count, OutOffsets))
		{
			return false;
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			ElementType Value;
			OffsetType& Offsets = OutOffsets.AddDefaulted_GetRef();
			if (!ReadElement(Reader, Value, Offsets, Index))
			{
				return false;
			}
			OutValues.Add(MoveTemp(Value));
		}
		return true;
	}

	template <typename EntryType>
	static bool ReadCapturedOptionalHash(
		FReader& Reader,
		TOptional<FAngelscriptHash256>& OutValue,
		EntryType& PresenceOffset,
		TOptional<EntryType>& ValueOffset,
		const FAngelscriptSourceIndexFieldCoordinate& PresenceCoordinate,
		const FAngelscriptSourceIndexFieldCoordinate& ValueCoordinate)
	{
		OutValue.Reset();
		ValueOffset.Reset();
		const uint64 TagOffset = Reader.GetOffset();
		CaptureOffset(PresenceOffset, PresenceCoordinate, TagOffset);
		uint8 Tag = 0;
		if (!Reader.ReadUInt8(Tag))
		{
			return false;
		}
		if (Tag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TagOffset);
			return false;
		}
		if (Tag == 0)
		{
			return true;
		}
		const uint64 HashOffset = Reader.GetOffset();
		EntryType Captured;
		CaptureOffset(Captured, ValueCoordinate, HashOffset);
		ValueOffset = Captured;
		FAngelscriptHash256 Value;
		if (!Reader.ReadHash(Value))
		{
			return false;
		}
		OutValue = Value;
		return true;
	}

	template <typename OptionOffsetsType>
	static bool ReadCapturedSourceOption(
		FReader& Reader,
		FAngelscriptCachedCanonicalOption& OutValue,
		OptionOffsetsType& OutOffsets,
		const EAngelscriptSourceIndexCapturedField RowField,
		const EAngelscriptSourceIndexCapturedField KeyField,
		const EAngelscriptSourceIndexCapturedField ValueField,
		const uint32 PrimaryIndex,
		const uint32 SecondaryIndex = MAX_uint32)
	{
		const uint64 KeyOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0],
			FAngelscriptSourceIndexFieldCoordinate{RowField, PrimaryIndex, SecondaryIndex},
			KeyOffset);
		CaptureOffset(OutOffsets.Fields[1],
			FAngelscriptSourceIndexFieldCoordinate{KeyField, PrimaryIndex, SecondaryIndex},
			KeyOffset);
		if (!Reader.ReadString(OutValue.CanonicalKey))
		{
			return false;
		}
		const uint64 ValueOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[2],
			FAngelscriptSourceIndexFieldCoordinate{ValueField, PrimaryIndex, SecondaryIndex},
			ValueOffset);
		return Reader.ReadHash(OutValue.ValueFingerprint);
	}

	template <typename DiscoveryOffsetsType>
	static bool ReadCapturedDiscoveryPolicy(
		FReader& Reader,
		FAngelscriptCachedSourceDiscoveryPolicy& OutValue,
		DiscoveryOffsetsType& OutOffsets)
	{
		const uint64 PolicyOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {
			EAngelscriptSourceIndexCapturedField::DiscoveryPolicy}, PolicyOffset);
		CaptureOffset(OutOffsets.Fields[1], {
			EAngelscriptSourceIndexCapturedField::DiscoveryPolicyVersion}, PolicyOffset);
		if (!Reader.ReadUInt32(OutValue.PolicyVersion))
		{
			return false;
		}
		const uint64 FlagsOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[2], {
			EAngelscriptSourceIndexCapturedField::DiscoveryPolicyFilterFlags}, FlagsOffset);
		if (!Reader.ReadUInt32(OutValue.FilterFlags))
		{
			return false;
		}
		const uint64 OptionsOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[3], {
			EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOptions}, OptionsOffset);
		return ReadCapturedArray(
			Reader, OutValue.Options, OutOffsets.Options, 40, OptionsOffset,
			[](FReader& InReader, FAngelscriptCachedCanonicalOption& OutOption,
				auto& OptionOffsets, const uint32 OptionIndex)
			{
				return ReadCapturedSourceOption(
					InReader, OutOption, OptionOffsets,
					EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOption,
					EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOptionCanonicalKey,
					EAngelscriptSourceIndexCapturedField::DiscoveryPolicyOptionValueFingerprint,
					OptionIndex);
			});
	}

	template <typename MountOffsetsType>
	static bool ReadCapturedSourceMount(
		FReader& Reader,
		FAngelscriptCachedSourceMount& OutValue,
		MountOffsetsType& OutOffsets,
		const uint32 Index)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {
			EAngelscriptSourceIndexCapturedField::Mount, Index}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {
			EAngelscriptSourceIndexCapturedField::MountKey, Index}, RowOffset);
		if (!Reader.ReadHash(OutValue.MountKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {
			EAngelscriptSourceIndexCapturedField::MountSourceKind, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.SourceKind, 1, 3)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {
			EAngelscriptSourceIndexCapturedField::MountLogicalMount, Index}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.LogicalMount)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {
			EAngelscriptSourceIndexCapturedField::MountProviderKey, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.ProviderKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {
			EAngelscriptSourceIndexCapturedField::MountRootConfigurationFingerprint, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.RootConfigurationFingerprint)) { return false; }
		const uint64 OptionsOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[6], {
			EAngelscriptSourceIndexCapturedField::MountOptions, Index}, OptionsOffset);
		return ReadCapturedArray(
			Reader, OutValue.Options, OutOffsets.Options, 40, OptionsOffset,
			[Index](FReader& InReader, FAngelscriptCachedCanonicalOption& OutOption,
				auto& OptionOffsets, const uint32 OptionIndex)
			{
				return ReadCapturedSourceOption(
					InReader, OutOption, OptionOffsets,
					EAngelscriptSourceIndexCapturedField::MountOption,
					EAngelscriptSourceIndexCapturedField::MountOptionCanonicalKey,
					EAngelscriptSourceIndexCapturedField::MountOptionValueFingerprint,
					Index, OptionIndex);
			});
	}

	template <typename ProviderOffsetsType>
	static bool ReadCapturedSourceProvider(
		FReader& Reader,
		FAngelscriptCachedSourceProvider& OutValue,
		ProviderOffsetsType& OutOffsets,
		const uint32 Index)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {EAngelscriptSourceIndexCapturedField::Provider, Index}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {EAngelscriptSourceIndexCapturedField::ProviderKey, Index}, RowOffset);
		if (!Reader.ReadHash(OutValue.ProviderKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {EAngelscriptSourceIndexCapturedField::ProviderKind, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.ProviderKind, 1, 4)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {EAngelscriptSourceIndexCapturedField::ProviderCanonicalImplementationIdentity, Index}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalImplementationIdentity)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {EAngelscriptSourceIndexCapturedField::ProviderIdentityFingerprint, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.IdentityFingerprint)) { return false; }
		if (!ReadCapturedOptionalHash(Reader, OutValue.VersionFingerprint,
			OutOffsets.Fields[5], OutOffsets.VersionFingerprint,
			{EAngelscriptSourceIndexCapturedField::ProviderVersionFingerprintPresence, Index},
			{EAngelscriptSourceIndexCapturedField::ProviderVersionFingerprint, Index})) { return false; }
		if (!ReadCapturedOptionalHash(Reader, OutValue.ConfigurationFingerprint,
			OutOffsets.Fields[6], OutOffsets.ConfigurationFingerprint,
			{EAngelscriptSourceIndexCapturedField::ProviderConfigurationFingerprintPresence, Index},
			{EAngelscriptSourceIndexCapturedField::ProviderConfigurationFingerprint, Index})) { return false; }
		if (!ReadCapturedOptionalHash(Reader, OutValue.ContentFingerprint,
			OutOffsets.Fields[7], OutOffsets.ContentFingerprint,
			{EAngelscriptSourceIndexCapturedField::ProviderContentFingerprintPresence, Index},
			{EAngelscriptSourceIndexCapturedField::ProviderContentFingerprint, Index})) { return false; }
		CaptureOffset(OutOffsets.Fields[8], {EAngelscriptSourceIndexCapturedField::ProviderCapabilityFlags, Index}, Reader.GetOffset());
		return Reader.ReadUInt32(OutValue.CapabilityFlags);
	}

	template <typename HookOffsetsType>
	static bool ReadCapturedHook(
		FReader& Reader,
		FAngelscriptCachedPreprocessHook& OutValue,
		HookOffsetsType& OutOffsets,
		const uint32 Index)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {EAngelscriptSourceIndexCapturedField::PreprocessHook, Index}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {EAngelscriptSourceIndexCapturedField::HookKey, Index}, RowOffset);
		if (!Reader.ReadHash(OutValue.HookKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {EAngelscriptSourceIndexCapturedField::HookPhase, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.Phase, 1, 3)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {EAngelscriptSourceIndexCapturedField::HookCanonicalImplementationIdentity, Index}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalImplementationIdentity)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {EAngelscriptSourceIndexCapturedField::HookAffectedScopeKind, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.AffectedScopeKind, 1, 5)) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {EAngelscriptSourceIndexCapturedField::HookAffectedScopeStableKey, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.AffectedScopeStableKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[6], {EAngelscriptSourceIndexCapturedField::HookIdentityFingerprint, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.IdentityFingerprint)) { return false; }
		if (!ReadCapturedOptionalHash(Reader, OutValue.VersionFingerprint,
			OutOffsets.Fields[7], OutOffsets.VersionFingerprint,
			{EAngelscriptSourceIndexCapturedField::HookVersionFingerprintPresence, Index},
			{EAngelscriptSourceIndexCapturedField::HookVersionFingerprint, Index})) { return false; }
		if (!ReadCapturedOptionalHash(Reader, OutValue.ConfigurationFingerprint,
			OutOffsets.Fields[8], OutOffsets.ConfigurationFingerprint,
			{EAngelscriptSourceIndexCapturedField::HookConfigurationFingerprintPresence, Index},
			{EAngelscriptSourceIndexCapturedField::HookConfigurationFingerprint, Index})) { return false; }
		if (!ReadCapturedOptionalHash(Reader, OutValue.ContentFingerprint,
			OutOffsets.Fields[9], OutOffsets.ContentFingerprint,
			{EAngelscriptSourceIndexCapturedField::HookContentFingerprintPresence, Index},
			{EAngelscriptSourceIndexCapturedField::HookContentFingerprint, Index})) { return false; }
		CaptureOffset(OutOffsets.Fields[10], {EAngelscriptSourceIndexCapturedField::HookCapabilityFlags, Index}, Reader.GetOffset());
		return Reader.ReadUInt32(OutValue.CapabilityFlags);
	}

	template <typename FileOffsetsType>
	static bool ReadCapturedSourceFile(
		FReader& Reader,
		FAngelscriptCachedSourceFile& OutValue,
		FileOffsetsType& OutOffsets,
		const uint32 Index)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {EAngelscriptSourceIndexCapturedField::File, Index}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {EAngelscriptSourceIndexCapturedField::FileSourceFileKey, Index}, RowOffset);
		if (!Reader.ReadHash(OutValue.SourceFileKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {EAngelscriptSourceIndexCapturedField::FileSourceKind, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.SourceKind, 1, 3)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {EAngelscriptSourceIndexCapturedField::FileMountKey, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.MountKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {EAngelscriptSourceIndexCapturedField::FileProviderKey, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.ProviderKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {EAngelscriptSourceIndexCapturedField::FileRelativeLogicalPath, Index}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.RelativeLogicalPath)) { return false; }
		CaptureOffset(OutOffsets.Fields[6], {EAngelscriptSourceIndexCapturedField::FileRawContentHash, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.RawContentHash)) { return false; }
		if (!ReadCapturedOptionalHash(Reader, OutValue.GeneratedSourceKey,
			OutOffsets.Fields[7], OutOffsets.GeneratedSourceKey,
			{EAngelscriptSourceIndexCapturedField::FileGeneratedSourceKeyPresence, Index},
			{EAngelscriptSourceIndexCapturedField::FileGeneratedSourceKey, Index})) { return false; }
		if (!ReadCapturedOptionalHash(Reader, OutValue.GeneratedConfigurationFingerprint,
			OutOffsets.Fields[8], OutOffsets.GeneratedConfigurationFingerprint,
			{EAngelscriptSourceIndexCapturedField::FileGeneratedConfigurationFingerprintPresence, Index},
			{EAngelscriptSourceIndexCapturedField::FileGeneratedConfigurationFingerprint, Index})) { return false; }
		CaptureOffset(OutOffsets.Fields[9], {EAngelscriptSourceIndexCapturedField::FileModuleKey, Index}, Reader.GetOffset());
		return Reader.ReadHash(OutValue.ModuleKey.Hash);
	}

	template <typename InputOffsetsType>
	static bool ReadCapturedInput(
		FReader& Reader,
		FAngelscriptCachedPreprocessorInput& OutValue,
		InputOffsetsType& OutOffsets,
		const uint32 Index)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {EAngelscriptSourceIndexCapturedField::PreprocessorInput, Index}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {EAngelscriptSourceIndexCapturedField::InputKey, Index}, RowOffset);
		if (!Reader.ReadHash(OutValue.InputKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {EAngelscriptSourceIndexCapturedField::InputOwnerScopeKind, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.OwnerScopeKind, 1, 5)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {EAngelscriptSourceIndexCapturedField::InputOwnerScopeStableKey, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.OwnerScopeStableKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {EAngelscriptSourceIndexCapturedField::InputKind, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.InputKind, 1, 4)) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {EAngelscriptSourceIndexCapturedField::InputCanonicalName, Index}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalName)) { return false; }
		const uint64 TargetKindOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[6], {EAngelscriptSourceIndexCapturedField::InputTargetKind, Index}, TargetKindOffset);
		uint8 TargetKind = 0;
		if (!Reader.ReadUInt8(TargetKind)) { return false; }
		if (TargetKind > 5)
		{
			Reader.Fail(EAngelscriptCacheValidationError::UnknownEnumValue, TargetKindOffset);
			return false;
		}
		OutValue.TargetKind = static_cast<EAngelscriptCachePreprocessorInputTargetKind>(TargetKind);
		if (!ReadCapturedOptionalHash(Reader, OutValue.TargetStableKey,
			OutOffsets.Fields[7], OutOffsets.TargetStableKey,
			{EAngelscriptSourceIndexCapturedField::InputTargetStableKeyPresence, Index},
			{EAngelscriptSourceIndexCapturedField::InputTargetStableKey, Index})) { return false; }
		CaptureOffset(OutOffsets.Fields[8], {EAngelscriptSourceIndexCapturedField::InputEffectiveValueOrContentHash, Index}, Reader.GetOffset());
		return Reader.ReadHash(OutValue.EffectiveValueOrContentHash);
	}

	template <typename EdgeOffsetsType>
	static bool ReadCapturedEdge(
		FReader& Reader,
		FAngelscriptCachedSourceEdge& OutValue,
		EdgeOffsetsType& OutOffsets,
		const uint32 Index)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {EAngelscriptSourceIndexCapturedField::Edge, Index}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {EAngelscriptSourceIndexCapturedField::EdgeKey, Index}, RowOffset);
		if (!Reader.ReadHash(OutValue.EdgeKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {EAngelscriptSourceIndexCapturedField::EdgeKind, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.EdgeKind, 1, 2)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {EAngelscriptSourceIndexCapturedField::EdgeFromSourceFileKey, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.FromSourceFileKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {EAngelscriptSourceIndexCapturedField::EdgeToSourceOrGeneratedKey, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.ToSourceOrGeneratedKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {EAngelscriptSourceIndexCapturedField::EdgeCanonicalIncludeOrGeneratorIdentity, Index}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalIncludeOrGeneratorIdentity)) { return false; }
		const uint64 TagOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[6], {EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinalPresence, Index}, TagOffset);
		uint8 Tag = 0;
		if (!Reader.ReadUInt8(Tag)) { return false; }
		if (Tag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TagOffset);
			return false;
		}
		if (Tag == 1)
		{
			auto Entry = OutOffsets.Fields[0];
			CaptureOffset(Entry, {EAngelscriptSourceIndexCapturedField::EdgeSemanticOrdinal, Index}, Reader.GetOffset());
			OutOffsets.SemanticOrdinal = Entry;
			uint32 Ordinal = 0;
			if (!Reader.ReadUInt32(Ordinal)) { return false; }
			OutValue.SemanticOrdinal = Ordinal;
		}
		return true;
	}

	template <typename IneligibleOffsetsType>
	static bool ReadCapturedIneligible(
		FReader& Reader,
		FAngelscriptCachedFastPathIneligibleScope& OutValue,
		IneligibleOffsetsType& OutOffsets,
		const uint32 Index)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {EAngelscriptSourceIndexCapturedField::IneligibleScope, Index}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {EAngelscriptSourceIndexCapturedField::IneligibleScopeKind, Index}, RowOffset);
		if (!ReadEnum(Reader, OutValue.ScopeKind, 1, 5)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {EAngelscriptSourceIndexCapturedField::IneligibleScopeStableKey, Index}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.ScopeStableKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {EAngelscriptSourceIndexCapturedField::IneligibleScopeReason, Index}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.Reason, 1, 5)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {EAngelscriptSourceIndexCapturedField::IneligibleScopeCanonicalDiagnosticIdentity, Index}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalDiagnosticIdentity)) { return false; }
		return ReadCapturedOptionalHash(Reader, OutValue.ObservedFingerprint,
			OutOffsets.Fields[5], OutOffsets.ObservedFingerprint,
			{EAngelscriptSourceIndexCapturedField::IneligibleScopeObservedFingerprintPresence, Index},
			{EAngelscriptSourceIndexCapturedField::IneligibleScopeObservedFingerprint, Index});
	}

	template <typename SourceOffsetsType>
	static bool ReadCapturedSourceIndexPayload(
		FReader& Reader,
		FAngelscriptCachedSourceIndex& OutValue,
		SourceOffsetsType& OutOffsets)
	{
		const uint64 SchemaOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(SchemaOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[0], {EAngelscriptSourceIndexCapturedField::PayloadSchemaVersion}, SchemaOffset);
		if (!Reader.ReadUInt32(OutValue.PayloadSchemaVersion)) { return false; }
		const uint64 SnapshotOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(SnapshotOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[1], {EAngelscriptSourceIndexCapturedField::SourceSnapshot}, SnapshotOffset);
		if (!Reader.ReadHash(OutValue.SourceSnapshot)) { return false; }
		const uint64 DiscoveryOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(DiscoveryOffset);
		if (!ReadCapturedDiscoveryPolicy(Reader, OutValue.DiscoveryPolicy, OutOffsets.Discovery)) { return false; }

		const auto ReadTopArray = [&](const int32 HeaderIndex,
			const EAngelscriptSourceIndexCapturedField Field,
			auto& Values,
			auto& Offsets,
			const uint64 MinimumBytes,
			auto&& ReadElement)
		{
			const uint64 TopOffset = Reader.GetOffset();
			Reader.SetSemanticEnclosingFieldOffset(TopOffset);
			CaptureOffset(OutOffsets.HeaderOffsets[HeaderIndex], {Field}, TopOffset);
			return ReadCapturedArray(
				Reader, Values, Offsets, MinimumBytes, TopOffset,
				Forward<decltype(ReadElement)>(ReadElement));
		};

		if (!ReadTopArray(2, EAngelscriptSourceIndexCapturedField::Mounts,
			OutValue.Mounts, OutOffsets.Mounts, 105,
			ReadCapturedSourceMount<
				std::remove_reference_t<decltype(OutOffsets.Mounts[0])>>)) { return false; }
		if (!ReadTopArray(3, EAngelscriptSourceIndexCapturedField::Providers,
			OutValue.Providers, OutOffsets.Providers, 76,
			ReadCapturedSourceProvider<
				std::remove_reference_t<decltype(OutOffsets.Providers[0])>>)) { return false; }
		if (!ReadTopArray(4, EAngelscriptSourceIndexCapturedField::PreprocessHooks,
			OutValue.PreprocessHooks, OutOffsets.Hooks, 109,
			ReadCapturedHook<
				std::remove_reference_t<decltype(OutOffsets.Hooks[0])>>)) { return false; }
		if (!ReadTopArray(5, EAngelscriptSourceIndexCapturedField::Files,
			OutValue.Files, OutOffsets.Files, 167,
			ReadCapturedSourceFile<
				std::remove_reference_t<decltype(OutOffsets.Files[0])>>)) { return false; }
		if (!ReadTopArray(6, EAngelscriptSourceIndexCapturedField::PreprocessorInputs,
			OutValue.PreprocessorInputs, OutOffsets.Inputs, 104,
			ReadCapturedInput<
				std::remove_reference_t<decltype(OutOffsets.Inputs[0])>>)) { return false; }
		if (!ReadTopArray(7, EAngelscriptSourceIndexCapturedField::Edges,
			OutValue.Edges, OutOffsets.Edges, 102,
			ReadCapturedEdge<
				std::remove_reference_t<decltype(OutOffsets.Edges[0])>>)) { return false; }
		return ReadTopArray(8, EAngelscriptSourceIndexCapturedField::IneligibleScopes,
			OutValue.IneligibleScopes,
			OutOffsets.IneligibleScopes, 39,
			ReadCapturedIneligible<
				std::remove_reference_t<decltype(OutOffsets.IneligibleScopes[0])>>);
	}

	static void WriteArtifactStableReference(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const FAngelscriptCacheStableReference& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteHash(Value.StableKey);
		Writer.WriteHash(Value.ExpectedAbi);
	}

	static void WriteArtifactDataType(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const FAngelscriptCachedDataType& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.Kind));
		Writer.WriteUInt8(static_cast<uint8>(Value.Primitive));
		Writer.WriteBool(Value.TypeReference.IsSet());
		if (Value.TypeReference.IsSet())
		{
			WriteArtifactStableReference(Writer, Value.TypeReference.GetValue());
		}
		Writer.WriteUInt32(Value.QualifierFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedSubTypes.Num()));
		for (const FAngelscriptCachedDataType& SubType : Value.OrderedSubTypes)
		{
			WriteArtifactDataType(Writer, SubType);
		}
	}

	static FAngelscriptHash256 HashDeclarationSignature(const FAngelscriptCachedDeclaration& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-declaration-signature-v1"));
		Writer.WriteUInt8(static_cast<uint8>(Value.DeclarationKind));
		Writer.WriteUInt8(static_cast<uint8>(Value.EntityKind));
		Writer.WriteUInt8(static_cast<uint8>(Value.SchemaCoverage));
		Writer.WriteUInt8(static_cast<uint8>(Value.BodyCoverage));
		Writer.WriteUInt8(static_cast<uint8>(Value.OwnerKind));
		Writer.WriteHash(Value.OwnerKey);
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteString(Value.CanonicalNamespace);
		Writer.WriteString(Value.CanonicalName);
		Writer.WriteString(Value.CanonicalDeclaration);
		Writer.WriteBool(Value.CanonicalTypeSpelling.IsSet());
		if (Value.CanonicalTypeSpelling.IsSet())
		{
			Writer.WriteString(Value.CanonicalTypeSpelling.GetValue());
		}
		Writer.WriteBool(Value.DeclaredType.IsSet());
		if (Value.DeclaredType.IsSet())
		{
			WriteArtifactDataType(Writer, Value.DeclaredType.GetValue());
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedParameters.Num()));
		for (const FAngelscriptCachedParameter& Parameter : Value.OrderedParameters)
		{
			Writer.WriteUInt32(Parameter.Ordinal);
			Writer.WriteString(Parameter.CanonicalName);
			WriteArtifactDataType(Writer, Parameter.Type);
			Writer.WriteUInt8(static_cast<uint8>(Parameter.Passing));
			Writer.WriteBool(Parameter.CanonicalDefaultExpression.IsSet());
			if (Parameter.CanonicalDefaultExpression.IsSet())
			{
				Writer.WriteString(Parameter.CanonicalDefaultExpression.GetValue());
			}
			Writer.WriteUInt32(Parameter.TraitFlags);
		}
		Writer.WriteUInt32(Value.TraitFlags);
		Writer.WriteUInt32(Value.ReflectionFlags);
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 HashDeclarationTraits(const FAngelscriptCachedDeclaration& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-declaration-traits-v1"));
		Writer.WriteUInt32(static_cast<uint32>(Value.CanonicalIdentityTraits.Num()));
		for (const FString& Trait : Value.CanonicalIdentityTraits)
		{
			Writer.WriteString(Trait);
		}
		Writer.WriteUInt32(Value.TraitFlags);
		Writer.WriteUInt32(Value.ReflectionFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.Metadata.Num()));
		for (const FAngelscriptCachedMetadataEntry& Metadata : Value.Metadata)
		{
			Writer.WriteString(Metadata.CanonicalKey);
			Writer.WriteString(Metadata.CanonicalValue);
		}
		return Writer.FinalizeHash();
	}

	static bool IsTypeEntity(const EAngelscriptArtifactEntityKind Kind)
	{
		return Kind == EAngelscriptArtifactEntityKind::Class
			|| Kind == EAngelscriptArtifactEntityKind::Struct
			|| Kind == EAngelscriptArtifactEntityKind::Interface
			|| Kind == EAngelscriptArtifactEntityKind::Enum
			|| Kind == EAngelscriptArtifactEntityKind::Delegate
			|| Kind == EAngelscriptArtifactEntityKind::Typedef
			|| Kind == EAngelscriptArtifactEntityKind::Funcdef;
	}

	static bool IsFunctionEntity(const EAngelscriptArtifactEntityKind Kind)
	{
		return Kind == EAngelscriptArtifactEntityKind::GlobalFunction
			|| Kind == EAngelscriptArtifactEntityKind::Method
			|| Kind == EAngelscriptArtifactEntityKind::Constructor
			|| Kind == EAngelscriptArtifactEntityKind::Destructor
			|| Kind == EAngelscriptArtifactEntityKind::Factory
			|| Kind == EAngelscriptArtifactEntityKind::DelegateSignature
			|| Kind == EAngelscriptArtifactEntityKind::ModuleInitializer
			|| Kind == EAngelscriptArtifactEntityKind::GlobalInitializer
			|| Kind == EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor
			|| Kind == EAngelscriptArtifactEntityKind::GeneratedDefaultDestructor
			|| Kind == EAngelscriptArtifactEntityKind::InitDefaults;
	}

	static FAngelscriptCacheValidationResult ComputeDeclarationStableKey(
		const FAngelscriptCachedDeclaration& Value,
		FAngelscriptHash256& OutKey)
	{
		OutKey = {};
		switch (Value.DeclarationKind)
		{
		case EAngelscriptCacheDeclarationKind::Type:
		{
			FAngelscriptTypeIdentityDescriptor Descriptor;
			Descriptor.ModuleKey = Value.ModuleKey;
			Descriptor.Namespace = Value.CanonicalNamespace;
			Descriptor.Kind = Value.EntityKind;
			Descriptor.CanonicalDeclaration = Value.CanonicalDeclaration;
			Descriptor.CanonicalTraits = Value.CanonicalIdentityTraits;
			OutKey = FAngelscriptArtifactIdentityBuilder::BuildTypeKey(Descriptor).Hash;
			return {};
		}
		case EAngelscriptCacheDeclarationKind::Function:
		{
			FAngelscriptFunctionIdentityDescriptor Descriptor;
			Descriptor.OwnerKind = Value.OwnerKind;
			Descriptor.OwnerKey = Value.OwnerKey;
			Descriptor.Namespace = Value.CanonicalNamespace;
			Descriptor.Kind = Value.EntityKind;
			Descriptor.CanonicalDeclaration = Value.CanonicalDeclaration;
			Descriptor.CanonicalTraits = Value.CanonicalIdentityTraits;
			OutKey = FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Descriptor).Hash;
			return {};
		}
		case EAngelscriptCacheDeclarationKind::Global:
		{
			FAngelscriptGlobalIdentityDescriptor Descriptor;
			Descriptor.ModuleKey = Value.ModuleKey;
			Descriptor.Namespace = Value.CanonicalNamespace;
			Descriptor.Kind = Value.EntityKind;
			Descriptor.Name = Value.CanonicalName;
			Descriptor.CanonicalType = Value.CanonicalTypeSpelling.GetValue();
			Descriptor.CanonicalTraits = Value.CanonicalIdentityTraits;
			OutKey = FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Descriptor).Hash;
			return {};
		}
		case EAngelscriptCacheDeclarationKind::Property:
		{
			FAngelscriptPropertyIdentityDescriptor Descriptor;
			Descriptor.OwnerTypeKey = FAngelscriptStableTypeKey{Value.OwnerKey};
			Descriptor.Kind = Value.EntityKind;
			Descriptor.Name = Value.CanonicalName;
			Descriptor.CanonicalType = Value.CanonicalTypeSpelling.GetValue();
			Descriptor.CanonicalTraits = Value.CanonicalIdentityTraits;
			OutKey = FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Descriptor).Hash;
			return {};
		}
		default:
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
	}

	static FAngelscriptCacheValidationResult PrepareDeclaration(
		FAngelscriptCachedDeclaration& Value,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		const bool bValidateStoredHashes,
		const FModuleInterfaceSemanticOffsetView* ExactOffsets = nullptr,
		const uint32 DeclarationIndex = MAX_uint32,
		uint64* InOutSemanticFieldOffset = nullptr)
	{
		const auto SetExactOffset = [&](const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate)
		{
			if (ExactOffsets == nullptr)
			{
				return;
			}
			check(InOutSemanticFieldOffset != nullptr);
			const TOptional<uint64> Offset = ExactOffsets->FindOffset(Coordinate);
			check(Offset.IsSet());
			*InOutSemanticFieldOffset = Offset.GetValue();
		};
		const auto ValidateExactStableReference = [&SetExactOffset](
			const FAngelscriptCacheStableReference& Reference,
			const FAngelscriptModuleInterfaceFieldCoordinate& KindCoordinate,
			const FAngelscriptModuleInterfaceFieldCoordinate& StableKeyCoordinate,
			const FAngelscriptModuleInterfaceFieldCoordinate& ExpectedAbiCoordinate)
			-> FAngelscriptCacheValidationResult
		{
			SetExactOffset(KindCoordinate);
			const uint8 RawKind = static_cast<uint8>(Reference.Kind);
			if (RawKind < 1 || RawKind > 9)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			SetExactOffset(StableKeyCoordinate);
			if (Reference.StableKey.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
			}
			SetExactOffset(ExpectedAbiCoordinate);
			const bool bRequiresAbi = RawKind <= static_cast<uint8>(
				EAngelscriptCacheReferenceKind::EnvironmentSymbol);
			if (bRequiresAbi && Reference.ExpectedAbi.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::MissingExpectedAbi);
			}
			if (!bRequiresAbi && !Reference.ExpectedAbi.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ForbiddenExpectedAbi);
			}
			return {};
		};
		const auto ValidateExactDataType = [
			&SetExactOffset,
			&ValidateExactStableReference,
			DeclarationIndex](
				auto&& Self,
				const FAngelscriptCachedDataType& Type,
				const uint32 ParameterIndex,
				uint32& InOutNodeOrdinal,
				const uint64 Depth) -> FAngelscriptCacheValidationResult
		{
			const bool bParameter = ParameterIndex != MAX_uint32;
			const uint32 NodeOrdinal = InOutNodeOrdinal++;
			const auto Coordinate = [=](
				const EAngelscriptModuleInterfaceCapturedField DeclaredField,
				const EAngelscriptModuleInterfaceCapturedField ParameterField)
			{
				return bParameter
					? FAngelscriptModuleInterfaceFieldCoordinate{
						ParameterField, DeclarationIndex, ParameterIndex, NodeOrdinal}
					: FAngelscriptModuleInterfaceFieldCoordinate{
						DeclaredField, DeclarationIndex, NodeOrdinal, MAX_uint32};
			};
			const auto Select = [&SetExactOffset, &Coordinate](
				const EAngelscriptModuleInterfaceCapturedField DeclaredField,
				const EAngelscriptModuleInterfaceCapturedField ParameterField)
			{
				SetExactOffset(Coordinate(DeclaredField, ParameterField));
			};

			Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeNode,
				EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeNode);
			if (Depth > FAngelscriptCacheReadLimits::DefaultMaxNestingDepth)
			{
				return Failure(EAngelscriptCacheValidationError::NestingDepthExceeded);
			}
			Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeKind,
				EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeKind);
			const uint8 RawKind = static_cast<uint8>(Type.Kind);
			if (RawKind < 1 || RawKind > 4)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}
			Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeQualifierFlags,
				EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeQualifierFlags);
			if ((Type.QualifierFlags
				& ~static_cast<uint32>(EAngelscriptCachedTypeQualifierFlags::KnownMask)) != 0)
			{
				return Failure(EAngelscriptCacheValidationError::UnknownFlags);
			}
			const uint32 AutoFlag = static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::Auto);
			const bool bAutoKind = Type.Kind == EAngelscriptCachedDataTypeKind::Auto;
			if (bAutoKind != ((Type.QualifierFlags & AutoFlag) != 0))
			{
				return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
			}
			const uint32 ObjectHandle = static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
			const uint32 ConstHandle = static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ConstHandle);
			if ((Type.QualifierFlags & ConstHandle) != 0
				&& (Type.QualifierFlags & ObjectHandle) == 0)
			{
				return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
			}

			switch (Type.Kind)
			{
			case EAngelscriptCachedDataTypeKind::Primitive:
			{
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypePrimitive,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypePrimitive);
				if (static_cast<uint8>(Type.Primitive) < 1
					|| static_cast<uint8>(Type.Primitive) > 12)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidPresence);
				}
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferencePresence,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferencePresence);
				if (Type.TypeReference.IsSet())
				{
					return Failure(EAngelscriptCacheValidationError::InvalidPresence);
				}
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeQualifierFlags,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeQualifierFlags);
				const uint32 PrimitiveQualifiers = static_cast<uint32>(
					EAngelscriptCachedTypeQualifierFlags::Reference)
					| static_cast<uint32>(
						EAngelscriptCachedTypeQualifierFlags::ObjectConst);
				if ((Type.QualifierFlags & ~PrimitiveQualifiers) != 0)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
				}
				break;
			}

			case EAngelscriptCachedDataTypeKind::ScriptType:
			case EAngelscriptCachedDataTypeKind::EnvironmentType:
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypePrimitive,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypePrimitive);
				if (Type.Primitive != EAngelscriptCachedPrimitiveType::Invalid)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidPresence);
				}
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferencePresence,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferencePresence);
				if (!Type.TypeReference.IsSet())
				{
					return Failure(EAngelscriptCacheValidationError::InvalidPresence);
				}
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceKind,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceKind);
				if ((Type.Kind == EAngelscriptCachedDataTypeKind::ScriptType
						&& Type.TypeReference->Kind != EAngelscriptCacheReferenceKind::ScriptType)
					|| (Type.Kind == EAngelscriptCachedDataTypeKind::EnvironmentType
						&& Type.TypeReference->Kind != EAngelscriptCacheReferenceKind::EnvironmentSymbol))
				{
					return Failure(EAngelscriptCacheValidationError::WrongReferenceKind);
				}
				if (const FAngelscriptCacheValidationResult ReferenceResult =
					ValidateExactStableReference(
						Type.TypeReference.GetValue(),
						Coordinate(
							EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceKind,
							EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceKind),
						Coordinate(
							EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceStableKey,
							EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceStableKey),
						Coordinate(
							EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceExpectedAbi,
							EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceExpectedAbi));
					!ReferenceResult.IsSuccess())
				{
					return ReferenceResult;
				}
				break;

			case EAngelscriptCachedDataTypeKind::Auto:
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypePrimitive,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypePrimitive);
				if (Type.Primitive != EAngelscriptCachedPrimitiveType::Invalid)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
				}
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferencePresence,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferencePresence);
				if (Type.TypeReference.IsSet())
				{
					return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
				}
				Select(EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeQualifierFlags,
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeQualifierFlags);
				if (Type.QualifierFlags != AutoFlag)
				{
					return Failure(EAngelscriptCacheValidationError::InvalidQualifierCombination);
				}
				break;

			default:
				return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
			}

			for (const FAngelscriptCachedDataType& SubType : Type.OrderedSubTypes)
			{
				if (const FAngelscriptCacheValidationResult SubTypeResult = Self(
					Self, SubType, ParameterIndex, InOutNodeOrdinal, Depth + 1);
					!SubTypeResult.IsSuccess())
				{
					return SubTypeResult;
				}
			}
			return {};
		};

		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationKind,
			DeclarationIndex});
		if (static_cast<uint8>(Value.DeclarationKind) < 1
			|| static_cast<uint8>(Value.DeclarationKind) > 4)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationEntityKind,
			DeclarationIndex});
		if (!IsKnownEntityKind(Value.EntityKind))
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationSchemaCoverage,
			DeclarationIndex});
		if (static_cast<uint8>(Value.SchemaCoverage) < 1
			|| static_cast<uint8>(Value.SchemaCoverage) > 2)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationBodyCoverage,
			DeclarationIndex});
		if (static_cast<uint8>(Value.BodyCoverage) < 1
			|| static_cast<uint8>(Value.BodyCoverage) > 2)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationOwnerKind,
			DeclarationIndex});
		if (static_cast<uint8>(Value.OwnerKind) < 1
			|| static_cast<uint8>(Value.OwnerKind) > 4)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalNamespace,
			DeclarationIndex});
		if (const FAngelscriptCacheValidationResult NamespaceResult = ValidateString(Value.CanonicalNamespace);
			!NamespaceResult.IsSuccess())
		{
			return NamespaceResult;
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalName,
			DeclarationIndex});
		if (const FAngelscriptCacheValidationResult NameResult = ValidateRequiredString(Value.CanonicalName);
			!NameResult.IsSuccess())
		{
			return NameResult;
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalDeclaration,
			DeclarationIndex});
		if (const FAngelscriptCacheValidationResult DeclarationResult =
			ValidateRequiredString(Value.CanonicalDeclaration); !DeclarationResult.IsSuccess())
		{
			return DeclarationResult;
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationTraitFlags,
			DeclarationIndex});
		if ((Value.TraitFlags
			& ~static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::KnownMask)) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::DeclarationReflectionFlags,
			DeclarationIndex});
		if ((Value.ReflectionFlags
			& ~static_cast<uint32>(EAngelscriptCachedReflectionFlags::KnownMask)) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags);
		}
		int32 TraitFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult TraitsResult = PrepareStringSet(
			Value.CanonicalIdentityTraits,
			false,
			bCanonicalize,
			bRequireCanonicalOrder,
			&TraitFailureIndex);
			!TraitsResult.IsSuccess())
		{
			if (TraitFailureIndex != INDEX_NONE)
			{
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalIdentityTrait,
					DeclarationIndex,
					static_cast<uint32>(TraitFailureIndex)});
			}
			return TraitsResult;
		}
		int32 MetadataFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult MetadataResult = PrepareMetadata(
			Value.Metadata,
			bCanonicalize,
			bRequireCanonicalOrder,
			&MetadataFailureIndex);
			!MetadataResult.IsSuccess())
		{
			if (MetadataFailureIndex != INDEX_NONE)
			{
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DeclarationMetadataEntry,
					DeclarationIndex,
					static_cast<uint32>(MetadataFailureIndex)});
			}
			return MetadataResult;
		}
		int32 SlotFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult SlotResult = PrepareSlots(
			Value.Slots,
			bCanonicalize,
			bRequireCanonicalOrder,
			&SlotFailureIndex);
			!SlotResult.IsSuccess())
		{
			if (SlotFailureIndex != INDEX_NONE)
			{
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DeclarationSlot,
					DeclarationIndex,
					static_cast<uint32>(SlotFailureIndex)});
			}
			return SlotResult;
		}

		for (int32 Index = 0; Index < Value.OrderedParameters.Num(); ++Index)
		{
			const FAngelscriptCachedParameter& Parameter = Value.OrderedParameters[Index];
			SetExactOffset({
				EAngelscriptModuleInterfaceCapturedField::DeclarationParameterOrdinal,
				DeclarationIndex,
				static_cast<uint32>(Index)});
			if (Parameter.Ordinal < static_cast<uint32>(Index))
			{
				return Failure(EAngelscriptCacheValidationError::DuplicateOrdinal);
			}
			if (Parameter.Ordinal != static_cast<uint32>(Index))
			{
				return Failure(EAngelscriptCacheValidationError::OrdinalGap);
			}
			FAngelscriptCacheValidationResult ParameterResult;
			if (ExactOffsets == nullptr)
			{
				ParameterResult = ValidateParameter(Parameter);
			}
			else
			{
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DeclarationParameterCanonicalName,
					DeclarationIndex,
					static_cast<uint32>(Index)});
				ParameterResult = ValidateRequiredString(Parameter.CanonicalName);
				if (ParameterResult.IsSuccess())
				{
					uint32 NodeOrdinal = 0;
					ParameterResult = ValidateExactDataType(
						ValidateExactDataType,
						Parameter.Type,
						static_cast<uint32>(Index),
						NodeOrdinal,
						1);
				}
				if (ParameterResult.IsSuccess())
				{
					SetExactOffset({
						EAngelscriptModuleInterfaceCapturedField::DeclarationParameterPassing,
						DeclarationIndex,
						static_cast<uint32>(Index)});
					if (static_cast<uint8>(Parameter.Passing) < 1
						|| static_cast<uint8>(Parameter.Passing) > 4)
					{
						ParameterResult = Failure(
							EAngelscriptCacheValidationError::UnknownEnumValue);
					}
				}
				if (ParameterResult.IsSuccess()
					&& Parameter.CanonicalDefaultExpression.IsSet())
				{
					SetExactOffset({
						EAngelscriptModuleInterfaceCapturedField::DeclarationParameterDefaultExpression,
						DeclarationIndex,
						static_cast<uint32>(Index)});
					ParameterResult = ValidateString(
						Parameter.CanonicalDefaultExpression.GetValue());
				}
				if (ParameterResult.IsSuccess())
				{
					SetExactOffset({
						EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTraitFlags,
						DeclarationIndex,
						static_cast<uint32>(Index)});
					if ((Parameter.TraitFlags & ~static_cast<uint32>(
						EAngelscriptCachedParameterTraitFlags::KnownMask)) != 0)
					{
						ParameterResult = Failure(
							EAngelscriptCacheValidationError::UnknownFlags);
					}
				}
			}
			if (!ParameterResult.IsSuccess())
			{
				return ParameterResult;
			}
		}

		if (Value.CanonicalTypeSpelling.IsSet())
		{
			SetExactOffset({
				EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalTypeSpelling,
				DeclarationIndex});
			if (const FAngelscriptCacheValidationResult TypeSpellingResult =
				ValidateRequiredString(Value.CanonicalTypeSpelling.GetValue()); !TypeSpellingResult.IsSuccess())
			{
				return TypeSpellingResult;
			}
		}
		if (Value.DeclaredType.IsSet())
		{
			FAngelscriptCacheValidationResult TypeResult;
			if (ExactOffsets == nullptr)
			{
				TypeResult = ValidateDataType(Value.DeclaredType.GetValue(), 1);
			}
			else
			{
				uint32 NodeOrdinal = 0;
				TypeResult = ValidateExactDataType(
					ValidateExactDataType,
					Value.DeclaredType.GetValue(),
					MAX_uint32,
					NodeOrdinal,
					1);
			}
			if (!TypeResult.IsSuccess())
			{
				return TypeResult;
			}
		}

		switch (Value.DeclarationKind)
		{
		case EAngelscriptCacheDeclarationKind::Type:
			if (!IsTypeEntity(Value.EntityKind)
				|| Value.SchemaCoverage != EAngelscriptCacheSchemaCoverage::Required
				|| Value.BodyCoverage != EAngelscriptCacheBodyCoverage::Forbidden
				|| Value.CanonicalTypeSpelling.IsSet() || Value.DeclaredType.IsSet()
				|| !Value.OrderedParameters.IsEmpty())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			break;

		case EAngelscriptCacheDeclarationKind::Function:
		{
			if (!IsFunctionEntity(Value.EntityKind) || Value.CanonicalTypeSpelling.IsSet()
				|| !Value.DeclaredType.IsSet()
				|| Value.SchemaCoverage != EAngelscriptCacheSchemaCoverage::Forbidden)
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			const bool bMustForbidBody =
				(Value.TraitFlags & static_cast<uint32>(EAngelscriptCachedDeclarationTraitFlags::Abstract)) != 0
				|| Value.EntityKind == EAngelscriptArtifactEntityKind::DelegateSignature
				|| Value.EntityKind == EAngelscriptArtifactEntityKind::ModuleInitializer
				|| Value.EntityKind == EAngelscriptArtifactEntityKind::GlobalInitializer;
			const EAngelscriptCacheBodyCoverage RequiredBody = bMustForbidBody
				? EAngelscriptCacheBodyCoverage::Forbidden
				: EAngelscriptCacheBodyCoverage::Required;
			if (Value.BodyCoverage != RequiredBody)
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			break;
		}

		case EAngelscriptCacheDeclarationKind::Global:
			if (Value.EntityKind != EAngelscriptArtifactEntityKind::GlobalVariable
				|| Value.SchemaCoverage != EAngelscriptCacheSchemaCoverage::Forbidden
				|| Value.BodyCoverage != EAngelscriptCacheBodyCoverage::Forbidden
				|| !Value.CanonicalTypeSpelling.IsSet() || !Value.DeclaredType.IsSet()
				|| !Value.OrderedParameters.IsEmpty())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			break;

		case EAngelscriptCacheDeclarationKind::Property:
			if (Value.EntityKind != EAngelscriptArtifactEntityKind::Property
				|| Value.SchemaCoverage != EAngelscriptCacheSchemaCoverage::Forbidden
				|| Value.BodyCoverage != EAngelscriptCacheBodyCoverage::Forbidden
				|| !Value.CanonicalTypeSpelling.IsSet() || !Value.DeclaredType.IsSet()
				|| !Value.OrderedParameters.IsEmpty())
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			break;

		default:
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		FAngelscriptHash256 ComputedStableKey;
		SetExactOffset({
			EAngelscriptModuleInterfaceCapturedField::DeclarationStableKey,
			DeclarationIndex});
		const FAngelscriptCacheValidationResult KeyResult = ComputeDeclarationStableKey(Value, ComputedStableKey);
		if (!KeyResult.IsSuccess())
		{
			return KeyResult;
		}
		if (!(ComputedStableKey == Value.StableKey))
		{
			return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
		}

		if (bValidateStoredHashes)
		{
			const FAngelscriptHash256 ComputedSignatureHash = HashDeclarationSignature(Value);
			const FAngelscriptHash256 ComputedTraitsHash = HashDeclarationTraits(Value);
			if (!(ComputedSignatureHash == Value.SignatureHash))
			{
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DeclarationSignatureHash,
					DeclarationIndex});
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
			}
			if (!(ComputedTraitsHash == Value.TraitsHash))
			{
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DeclarationTraitsHash,
					DeclarationIndex});
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
			}
		}
		return {};
	}

	static void WriteDeclaration(FWriter& Writer, const FAngelscriptCachedDeclaration& Value)
	{
		Writer.WriteUInt8(static_cast<uint8>(Value.DeclarationKind));
		Writer.WriteUInt8(static_cast<uint8>(Value.EntityKind));
		Writer.WriteUInt8(static_cast<uint8>(Value.SchemaCoverage));
		Writer.WriteUInt8(static_cast<uint8>(Value.BodyCoverage));
		Writer.WriteHash(Value.StableKey);
		Writer.WriteUInt8(static_cast<uint8>(Value.OwnerKind));
		Writer.WriteHash(Value.OwnerKey);
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteString(Value.CanonicalNamespace);
		Writer.WriteString(Value.CanonicalName);
		Writer.WriteString(Value.CanonicalDeclaration);
		Writer.WriteUInt32(static_cast<uint32>(Value.CanonicalIdentityTraits.Num()));
		for (const FString& Trait : Value.CanonicalIdentityTraits) { Writer.WriteString(Trait); }
		Writer.WriteOptionalString(Value.CanonicalTypeSpelling);
		Writer.WriteUInt8(Value.DeclaredType.IsSet() ? 1 : 0);
		if (Value.DeclaredType.IsSet()) { WriteDataType(Writer, Value.DeclaredType.GetValue()); }
		Writer.WriteUInt32(static_cast<uint32>(Value.OrderedParameters.Num()));
		for (const auto& Parameter : Value.OrderedParameters) { WriteParameter(Writer, Parameter); }
		Writer.WriteUInt32(Value.TraitFlags);
		Writer.WriteUInt32(Value.ReflectionFlags);
		Writer.WriteUInt32(static_cast<uint32>(Value.Metadata.Num()));
		for (const auto& Metadata : Value.Metadata) { WriteMetadata(Writer, Metadata); }
		Writer.WriteUInt32(static_cast<uint32>(Value.Slots.Num()));
		for (const auto& Slot : Value.Slots) { WriteSlot(Writer, Slot); }
		Writer.WriteHash(Value.SignatureHash);
		Writer.WriteHash(Value.TraitsHash);
	}

	static bool ReadDeclaration(FReader& Reader, FAngelscriptCachedDeclaration& OutValue)
	{
		if (!ReadEnum(Reader, OutValue.DeclarationKind, 1, 4)) { return false; }
		uint8 EntityRaw = 0;
		const uint64 EntityOffset = Reader.GetOffset();
		if (!Reader.ReadUInt8(EntityRaw)) { return false; }
		OutValue.EntityKind = static_cast<EAngelscriptArtifactEntityKind>(EntityRaw);
		if (!IsKnownEntityKind(OutValue.EntityKind))
		{
			Reader.Fail(EAngelscriptCacheValidationError::UnknownEnumValue, EntityOffset);
			return false;
		}
		if (!ReadEnum(Reader, OutValue.SchemaCoverage, 1, 2)
			|| !ReadEnum(Reader, OutValue.BodyCoverage, 1, 2)
			|| !Reader.ReadHash(OutValue.StableKey)
			|| !ReadEnum(Reader, OutValue.OwnerKind, 1, 4)
			|| !Reader.ReadHash(OutValue.OwnerKey)
			|| !Reader.ReadHash(OutValue.ModuleKey.Hash)
			|| !Reader.ReadString(OutValue.CanonicalNamespace)
			|| !Reader.ReadString(OutValue.CanonicalName)
			|| !Reader.ReadString(OutValue.CanonicalDeclaration)
			|| !ReadArray(Reader, OutValue.CanonicalIdentityTraits, 4,
				[](FReader& InReader, FString& Out) { return InReader.ReadString(Out); })
			|| !Reader.ReadOptionalString(OutValue.CanonicalTypeSpelling))
		{
			return false;
		}
		uint8 TypeTag = 0;
		const uint64 TypeTagOffset = Reader.GetOffset();
		if (!Reader.ReadUInt8(TypeTag)) { return false; }
		if (TypeTag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TypeTagOffset);
			return false;
		}
		if (TypeTag == 1)
		{
			FAngelscriptCachedDataType Type;
			if (!ReadDataType(Reader, Type, 1)) { return false; }
			OutValue.DeclaredType = MoveTemp(Type);
		}
		return ReadArray(Reader, OutValue.OrderedParameters, 25, ReadParameter)
			&& Reader.ReadUInt32(OutValue.TraitFlags)
			&& Reader.ReadUInt32(OutValue.ReflectionFlags)
			&& ReadArray(Reader, OutValue.Metadata, 8, ReadMetadata)
			&& ReadArray(Reader, OutValue.Slots, 5, ReadSlot)
			&& Reader.ReadHash(OutValue.SignatureHash)
			&& Reader.ReadHash(OutValue.TraitsHash);
	}

	static FAngelscriptStableImportKey HashImportKey(const FAngelscriptImportIdentityInput& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-script-import"));
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteString(Value.CanonicalNamespace);
		Writer.WriteString(Value.CanonicalName);
		Writer.WriteString(Value.CanonicalSignature);
		Writer.WriteHash(Value.TargetModuleKey.Hash);
		Writer.WriteHash(Value.TargetFunctionKey.Hash);
		return FAngelscriptStableImportKey{Writer.FinalizeHash()};
	}

	static FAngelscriptCacheValidationResult PrepareImport(
		const FAngelscriptStableModuleKey& OwningModuleKey,
		FAngelscriptCachedImportDeclaration& Value,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		const FModuleInterfaceSemanticOffsetView* ExactOffsets = nullptr,
		const uint32 ImportIndex = MAX_uint32,
		uint64* InOutSemanticFieldOffset = nullptr)
	{
		const auto SetExactOffset = [&](const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate)
		{
			if (ExactOffsets == nullptr)
			{
				return;
			}
			check(InOutSemanticFieldOffset != nullptr);
			const TOptional<uint64> Offset = ExactOffsets->FindOffset(Coordinate);
			check(Offset.IsSet());
			*InOutSemanticFieldOffset = Offset.GetValue();
		};
		if (OwningModuleKey.Hash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::ImportTargetModuleKey,
			ImportIndex});
		if (Value.TargetModuleKey.Hash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::ImportCanonicalNamespace,
			ImportIndex});
		if (const FAngelscriptCacheValidationResult NamespaceResult =
			ValidateString(Value.CanonicalNamespace); !NamespaceResult.IsSuccess())
		{
			return NamespaceResult;
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::ImportCanonicalName,
			ImportIndex});
		if (const FAngelscriptCacheValidationResult NameResult =
			ValidateRequiredString(Value.CanonicalName); !NameResult.IsSuccess())
		{
			return NameResult;
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::ImportCanonicalSignature,
			ImportIndex});
		if (const FAngelscriptCacheValidationResult SignatureResult =
			ValidateRequiredString(Value.CanonicalSignature); !SignatureResult.IsSuccess())
		{
			return SignatureResult;
		}
		SetExactOffset({
			EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationReferenceKind,
			ImportIndex});
		const uint8 TargetReferenceKind = static_cast<uint8>(Value.TargetDeclaration.Kind);
		if (TargetReferenceKind < 1 || TargetReferenceKind > 9)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		SetExactOffset({
			EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationStableKey,
			ImportIndex});
		if (Value.TargetDeclaration.StableKey.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
		}
		SetExactOffset({
			EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationExpectedAbi,
			ImportIndex});
		const bool bRequiresAbi = TargetReferenceKind <= static_cast<uint8>(
			EAngelscriptCacheReferenceKind::EnvironmentSymbol);
		if (bRequiresAbi && Value.TargetDeclaration.ExpectedAbi.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::MissingExpectedAbi);
		}
		if (!bRequiresAbi && !Value.TargetDeclaration.ExpectedAbi.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ForbiddenExpectedAbi);
		}
		SetExactOffset({
			EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationReferenceKind,
			ImportIndex});
		if (Value.TargetDeclaration.Kind != EAngelscriptCacheReferenceKind::ScriptFunction)
		{
			return Failure(EAngelscriptCacheValidationError::WrongReferenceKind);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::ImportSlots,
			ImportIndex});
		int32 SlotFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult SlotResult = PrepareSlots(
			Value.Slots,
			bCanonicalize,
			bRequireCanonicalOrder,
			&SlotFailureIndex);
			!SlotResult.IsSuccess())
		{
			if (SlotFailureIndex != INDEX_NONE)
			{
				SetExactOffset({EAngelscriptModuleInterfaceCapturedField::ImportSlot,
					ImportIndex, static_cast<uint32>(SlotFailureIndex)});
			}
			return SlotResult;
		}
		if (Value.Slots.Num() != 1 || Value.Slots[0].SlotKind != EAngelscriptCacheDeclarationSlotKind::Import)
		{
			return Failure(EAngelscriptCacheValidationError::InvalidPresence);
		}
		FAngelscriptStableImportKey ComputedImportKey;
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::ImportKey,
			ImportIndex});
		const FAngelscriptCacheValidationResult KeyResult =
			FAngelscriptCacheSemanticArchive::TryBuildImportKey(
				GetIdentityInput(OwningModuleKey, Value), ComputedImportKey);
		if (!KeyResult.IsSuccess())
		{
			return KeyResult;
		}
		if (!(ComputedImportKey.Hash == Value.ImportKey.Hash))
		{
			return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch);
		}
		return {};
	}

	static void WriteImport(FWriter& Writer, const FAngelscriptCachedImportDeclaration& Value)
	{
		Writer.WriteHash(Value.ImportKey.Hash);
		Writer.WriteString(Value.CanonicalNamespace);
		Writer.WriteString(Value.CanonicalName);
		Writer.WriteString(Value.CanonicalSignature);
		Writer.WriteHash(Value.TargetModuleKey.Hash);
		WriteStableReference(Writer, Value.TargetDeclaration);
		Writer.WriteUInt32(static_cast<uint32>(Value.Slots.Num()));
		for (const auto& Slot : Value.Slots) { WriteSlot(Writer, Slot); }
	}

	static bool ReadImport(FReader& Reader, FAngelscriptCachedImportDeclaration& OutValue)
	{
		return Reader.ReadHash(OutValue.ImportKey.Hash)
			&& Reader.ReadString(OutValue.CanonicalNamespace)
			&& Reader.ReadString(OutValue.CanonicalName)
			&& Reader.ReadString(OutValue.CanonicalSignature)
			&& Reader.ReadHash(OutValue.TargetModuleKey.Hash)
			&& ReadStableReference(Reader, OutValue.TargetDeclaration)
			&& ReadArray(Reader, OutValue.Slots, 5, ReadSlot);
	}

	static int32 CompareDeclaration(const FAngelscriptCachedDeclaration& A, const FAngelscriptCachedDeclaration& B)
	{
		if (A.DeclarationKind != B.DeclarationKind)
		{
			return static_cast<uint8>(A.DeclarationKind) < static_cast<uint8>(B.DeclarationKind) ? -1 : 1;
		}
		if (A.EntityKind != B.EntityKind)
		{
			return static_cast<uint8>(A.EntityKind) < static_cast<uint8>(B.EntityKind) ? -1 : 1;
		}
		return CompareHash(A.StableKey, B.StableKey);
	}

	static int32 CompareImport(const FAngelscriptCachedImportDeclaration& A, const FAngelscriptCachedImportDeclaration& B)
	{
		return CompareHash(A.ImportKey.Hash, B.ImportKey.Hash);
	}

	static bool EqualSlots(
		const TArray<FAngelscriptCachedDeclarationSlot>& A,
		const TArray<FAngelscriptCachedDeclarationSlot>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].SlotKind != B[Index].SlotKind || A[Index].Ordinal != B[Index].Ordinal)
			{
				return false;
			}
		}
		return true;
	}

	static FAngelscriptHash256 HashModuleInterfaceAbi(const FAngelscriptCachedModuleInterface& Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-module-interface-abi-v1"));
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteString(Value.CanonicalModuleName);
		Writer.WriteUInt32(static_cast<uint32>(Value.CanonicalNamespaces.Num()));
		for (const FString& Namespace : Value.CanonicalNamespaces) { Writer.WriteString(Namespace); }
		Writer.WriteUInt32(static_cast<uint32>(Value.Declarations.Num()));
		for (const FAngelscriptCachedDeclaration& Declaration : Value.Declarations)
		{
			Writer.WriteUInt8(static_cast<uint8>(Declaration.DeclarationKind));
			Writer.WriteUInt8(static_cast<uint8>(Declaration.EntityKind));
			Writer.WriteUInt8(static_cast<uint8>(Declaration.SchemaCoverage));
			Writer.WriteUInt8(static_cast<uint8>(Declaration.BodyCoverage));
			Writer.WriteHash(Declaration.StableKey);
			Writer.WriteHash(Declaration.SignatureHash);
			Writer.WriteHash(Declaration.TraitsHash);
			Writer.WriteUInt32(static_cast<uint32>(Declaration.Slots.Num()));
			for (const auto& Slot : Declaration.Slots)
			{
				Writer.WriteUInt8(static_cast<uint8>(Slot.SlotKind)); Writer.WriteUInt32(Slot.Ordinal);
			}
		}
		Writer.WriteUInt32(static_cast<uint32>(Value.Imports.Num()));
		for (const FAngelscriptCachedImportDeclaration& Import : Value.Imports)
		{
			Writer.WriteHash(Import.ImportKey.Hash); Writer.WriteString(Import.CanonicalSignature);
			Writer.WriteHash(Import.TargetModuleKey.Hash);
			WriteArtifactStableReference(Writer, Import.TargetDeclaration);
			Writer.WriteUInt32(static_cast<uint32>(Import.Slots.Num()));
			for (const auto& Slot : Import.Slots)
			{
				Writer.WriteUInt8(static_cast<uint8>(Slot.SlotKind)); Writer.WriteUInt32(Slot.Ordinal);
			}
		}
		uint32 AbiDependencyCount = 0;
		for (const auto& Dependency : Value.Dependencies)
		{
			if (DependencyContributesToInterfaceAbi(Dependency.Kind)) { ++AbiDependencyCount; }
		}
		Writer.WriteUInt32(AbiDependencyCount);
		for (const auto& Dependency : Value.Dependencies)
		{
			if (DependencyContributesToInterfaceAbi(Dependency.Kind))
			{
				Writer.WriteUInt8(static_cast<uint8>(Dependency.Kind));
				WriteArtifactStableReference(Writer, Dependency.Target);
			}
		}
		return Writer.FinalizeHash();
	}

	static FAngelscriptCacheValidationResult ValidateGlobalSlotOrdinals(
		const FAngelscriptCachedModuleInterface& Value,
		const FModuleInterfaceSemanticOffsetView* ExactOffsets,
		uint64* InOutSemanticFieldOffset,
		const uint8 FirstKindValue,
		const uint8 LastKindValue)
	{
		struct FSlotOccurrence final
		{
			uint32 Ordinal = 0;
			uint32 WireSequence = 0;
			uint32 OwnerIndex = 0;
			uint32 SlotIndex = 0;
			bool bImport = false;
		};
		const auto SetExactOffset = [&](const FSlotOccurrence& Occurrence)
		{
			if (ExactOffsets == nullptr)
			{
				return;
			}
			check(InOutSemanticFieldOffset != nullptr);
			const TOptional<uint64> Offset = ExactOffsets->FindOffset({
				Occurrence.bImport
					? EAngelscriptModuleInterfaceCapturedField::ImportSlotOrdinal
					: EAngelscriptModuleInterfaceCapturedField::DeclarationSlotOrdinal,
				Occurrence.OwnerIndex,
				Occurrence.SlotIndex});
			check(Offset.IsSet());
			*InOutSemanticFieldOffset = Offset.GetValue();
		};
		check(FirstKindValue >= 1 && LastKindValue <= 4
			&& FirstKindValue <= LastKindValue);
		for (uint8 KindValue = FirstKindValue;
			KindValue <= LastKindValue; ++KindValue)
		{
			TArray<FSlotOccurrence> Occurrences;
			uint32 WireSequence = 0;
			for (int32 DeclarationOrdinal = 0;
				DeclarationOrdinal < Value.Declarations.Num(); ++DeclarationOrdinal)
			{
				const FAngelscriptCachedDeclaration& Declaration =
					Value.Declarations[DeclarationOrdinal];
				for (int32 SlotIndex = 0; SlotIndex < Declaration.Slots.Num(); ++SlotIndex)
				{
					const FAngelscriptCachedDeclarationSlot& Slot = Declaration.Slots[SlotIndex];
					if (static_cast<uint8>(Slot.SlotKind) == KindValue)
					{
						Occurrences.Add({Slot.Ordinal, WireSequence,
							static_cast<uint32>(DeclarationOrdinal),
							static_cast<uint32>(SlotIndex), false});
					}
					++WireSequence;
				}
			}
			for (int32 ImportIndex = 0; ImportIndex < Value.Imports.Num(); ++ImportIndex)
			{
				const FAngelscriptCachedImportDeclaration& Import = Value.Imports[ImportIndex];
				for (int32 SlotIndex = 0; SlotIndex < Import.Slots.Num(); ++SlotIndex)
				{
					const FAngelscriptCachedDeclarationSlot& Slot = Import.Slots[SlotIndex];
					if (static_cast<uint8>(Slot.SlotKind) == KindValue)
					{
						Occurrences.Add({Slot.Ordinal, WireSequence,
							static_cast<uint32>(ImportIndex),
							static_cast<uint32>(SlotIndex), true});
					}
					++WireSequence;
				}
			}
			Occurrences.Sort([](const FSlotOccurrence& A, const FSlotOccurrence& B)
			{
				return A.Ordinal != B.Ordinal
					? A.Ordinal < B.Ordinal
					: A.WireSequence < B.WireSequence;
			});
			for (int32 Index = 0; Index < Occurrences.Num(); ++Index)
			{
				if (Index > 0 && Occurrences[Index].Ordinal == Occurrences[Index - 1].Ordinal)
				{
					SetExactOffset(Occurrences[Index]);
					return Failure(EAngelscriptCacheValidationError::DuplicateOrdinal);
				}
				if (Occurrences[Index].Ordinal != static_cast<uint32>(Index))
				{
					SetExactOffset(Occurrences[Index]);
					return Failure(EAngelscriptCacheValidationError::OrdinalGap);
				}
			}
		}
		return {};
	}

	static FAngelscriptCacheValidationResult PrepareModuleInterface(
		FAngelscriptCachedModuleInterface& Value,
		const bool bCanonicalize,
		const bool bRequireCanonicalOrder,
		const bool bValidateStoredAbi,
		const bool bValidateStoredDeclarationHashes = true,
		const FModuleInterfaceSemanticOffsetView* ExactOffsets = nullptr)
	{
		uint64 SemanticFieldOffset = 0;
		const auto SetExactOffset = [&](const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate)
		{
			if (ExactOffsets == nullptr)
			{
				return;
			}
			const TOptional<uint64> Offset = ExactOffsets->FindOffset(Coordinate);
			check(Offset.IsSet());
			SemanticFieldOffset = Offset.GetValue();
		};
		const auto Prepare = [&]() -> FAngelscriptCacheValidationResult
		{
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::PayloadSchemaVersion});
		if (Value.PayloadSchemaVersion != FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion)
		{
			return Failure(EAngelscriptCacheValidationError::UnsupportedPayloadSchema,
				EAngelscriptCacheRecordKind::ModuleInterface);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::ModuleKey});
		if (Value.ModuleKey.Hash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptCacheRecordKind::ModuleInterface);
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::CanonicalModuleName});
		if (const FAngelscriptCacheValidationResult NameResult = ValidateRequiredString(Value.CanonicalModuleName);
			!NameResult.IsSuccess())
		{
			return NameResult;
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::CanonicalNamespaces});
		int32 NamespaceFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult NamespaceResult = PrepareStringSet(
			Value.CanonicalNamespaces,
			false,
			bCanonicalize,
			bRequireCanonicalOrder,
			&NamespaceFailureIndex);
			!NamespaceResult.IsSuccess())
		{
			if (NamespaceFailureIndex != INDEX_NONE)
			{
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::CanonicalNamespace,
					static_cast<uint32>(NamespaceFailureIndex)});
			}
			return NamespaceResult;
		}

		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::Declarations});
		for (int32 DeclarationIndex = 0;
			DeclarationIndex < Value.Declarations.Num(); ++DeclarationIndex)
		{
			FAngelscriptCachedDeclaration& Declaration =
				Value.Declarations[DeclarationIndex];
			const FAngelscriptCacheValidationResult DeclarationResult = PrepareDeclaration(
				Declaration,
				bCanonicalize,
				bRequireCanonicalOrder,
				bValidateStoredDeclarationHashes,
				ExactOffsets,
				static_cast<uint32>(DeclarationIndex),
				&SemanticFieldOffset);
			if (!DeclarationResult.IsSuccess())
			{
				return DeclarationResult;
			}
		}
		int32 DeclarationFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateStableHashAuthorityDuplicates(
			Value.Declarations,
			[](const auto& Item) { return Item.StableKey; },
			[](const auto& A, const auto& B)
			{
				return A.DeclarationKind == B.DeclarationKind
					&& A.EntityKind == B.EntityKind
					&& A.SignatureHash == B.SignatureHash
					&& A.TraitsHash == B.TraitsHash
					&& EqualSlots(A.Slots, B.Slots);
			}, &DeclarationFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptModuleInterfaceCapturedField::Declaration,
				static_cast<uint32>(DeclarationFailureIndex)});
			return DuplicateResult;
		}
		if (bCanonicalize)
		{
			Value.Declarations.Sort([](const auto& A, const auto& B) { return CompareDeclaration(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			for (int32 DeclarationIndex = 1;
				DeclarationIndex < Value.Declarations.Num(); ++DeclarationIndex)
			{
				if (CompareDeclaration(
					Value.Declarations[DeclarationIndex - 1],
					Value.Declarations[DeclarationIndex]) >= 0)
				{
					SetExactOffset({
						EAngelscriptModuleInterfaceCapturedField::Declaration,
						static_cast<uint32>(DeclarationIndex)});
					return Failure(EAngelscriptCacheValidationError::NonCanonicalOrder);
				}
			}
		}
		if (const FAngelscriptCacheValidationResult SlotResult = ValidateGlobalSlotOrdinals(
			Value, ExactOffsets, &SemanticFieldOffset, 1, 3);
			!SlotResult.IsSuccess())
		{
			return SlotResult;
		}
		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::Declarations});
		TArray<FIndexedStableHash> DeclarationIndex;
		BuildStableHashIndex(Value.Declarations,
			[](const auto& Item) { return Item.StableKey; }, DeclarationIndex);
		auto RequiredOwnerKind = [](const FAngelscriptCachedDeclaration& Declaration)
		{
			if (Declaration.DeclarationKind == EAngelscriptCacheDeclarationKind::Type
				|| Declaration.DeclarationKind == EAngelscriptCacheDeclarationKind::Global)
			{
				return EAngelscriptFunctionOwnerKind::Module;
			}
			if (Declaration.DeclarationKind == EAngelscriptCacheDeclarationKind::Property)
			{
				return EAngelscriptFunctionOwnerKind::Type;
			}
			if (Declaration.EntityKind == EAngelscriptArtifactEntityKind::GlobalFunction
				|| Declaration.EntityKind == EAngelscriptArtifactEntityKind::ModuleInitializer)
			{
				return EAngelscriptFunctionOwnerKind::Module;
			}
			if (Declaration.EntityKind == EAngelscriptArtifactEntityKind::GlobalInitializer)
			{
				return EAngelscriptFunctionOwnerKind::Global;
			}
			return EAngelscriptFunctionOwnerKind::Type;
		};

		// Owner phase 1: interface/module authority disagreements win globally.
		for (const FAngelscriptCachedDeclaration& Declaration : Value.Declarations)
		{
			if (!(Declaration.ModuleKey.Hash == Value.ModuleKey.Hash)
				|| (Declaration.OwnerKind == EAngelscriptFunctionOwnerKind::Module
					&& !(Declaration.OwnerKey == Value.ModuleKey.Hash)))
			{
				return Failure(EAngelscriptCacheValidationError::CrossModuleOwner,
					EAngelscriptCacheRecordKind::ModuleInterface);
			}
		}
		// Owner phase 2: the tagged declaration/callable matrix selects one owner kind.
		for (const FAngelscriptCachedDeclaration& Declaration : Value.Declarations)
		{
			if (Declaration.OwnerKind != RequiredOwnerKind(Declaration))
			{
				return Failure(EAngelscriptCacheValidationError::WrongReferenceKind,
					EAngelscriptCacheRecordKind::ModuleInterface);
			}
		}
		// Owner phase 3: every required owner key is explicit and nonzero.
		for (const FAngelscriptCachedDeclaration& Declaration : Value.Declarations)
		{
			if (Declaration.OwnerKey.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::MissingOwner,
					EAngelscriptCacheRecordKind::ModuleInterface);
			}
		}
		// Owner phase 4: a present key under the wrong declaration kind is typed failure.
		for (const FAngelscriptCachedDeclaration& Declaration : Value.Declarations)
		{
			if (Declaration.OwnerKind == EAngelscriptFunctionOwnerKind::Module) { continue; }
			const int32 OwnerIndex = FindStableHashValueIndex(DeclarationIndex, Declaration.OwnerKey);
			if (OwnerIndex != INDEX_NONE)
			{
				const EAngelscriptCacheDeclarationKind RequiredKind =
					Declaration.OwnerKind == EAngelscriptFunctionOwnerKind::Type
						? EAngelscriptCacheDeclarationKind::Type
						: EAngelscriptCacheDeclarationKind::Global;
				if (Value.Declarations[OwnerIndex].DeclarationKind != RequiredKind)
				{
					return Failure(EAngelscriptCacheValidationError::WrongReferenceKind,
						EAngelscriptCacheRecordKind::ModuleInterface);
				}
			}
		}
		// Owner phase 5: an absent local owner is not publishable.
		for (const FAngelscriptCachedDeclaration& Declaration : Value.Declarations)
		{
			if (Declaration.OwnerKind != EAngelscriptFunctionOwnerKind::Module
				&& FindStableHashValueIndex(DeclarationIndex, Declaration.OwnerKey) == INDEX_NONE)
			{
				return Failure(EAngelscriptCacheValidationError::MissingOwner,
					EAngelscriptCacheRecordKind::ModuleInterface);
			}
		}
		// Owner phase 6: the resolved type/global entity kind must satisfy the exact matrix.
		for (const FAngelscriptCachedDeclaration& Declaration : Value.Declarations)
		{
			if (Declaration.OwnerKind == EAngelscriptFunctionOwnerKind::Module) { continue; }
			const FAngelscriptCachedDeclaration& Owner = Value.Declarations[
				FindStableHashValueIndex(DeclarationIndex, Declaration.OwnerKey)];
			if (Declaration.OwnerKind == EAngelscriptFunctionOwnerKind::Global
				&& Owner.EntityKind != EAngelscriptArtifactEntityKind::GlobalVariable)
			{
				return Failure(EAngelscriptCacheValidationError::WrongReferenceKind,
					EAngelscriptCacheRecordKind::ModuleInterface);
			}
			if (Declaration.OwnerKind == EAngelscriptFunctionOwnerKind::Type)
			{
				const bool bClassOrStruct = Owner.EntityKind == EAngelscriptArtifactEntityKind::Class
					|| Owner.EntityKind == EAngelscriptArtifactEntityKind::Struct;
				const bool bClassStructOrInterface = bClassOrStruct
					|| Owner.EntityKind == EAngelscriptArtifactEntityKind::Interface;
				const uint32 GeneratedFlag = static_cast<uint32>(
					EAngelscriptCachedDeclarationTraitFlags::Generated);
				const bool bDelegateOwner = Owner.EntityKind == EAngelscriptArtifactEntityKind::Delegate;
				const bool bOwnerGenerated = (Owner.TraitFlags & GeneratedFlag) != 0;
				const bool bDeclarationGenerated = (Declaration.TraitFlags & GeneratedFlag) != 0;
				const bool bGeneratedDelegatePair = bDelegateOwner
					&& bOwnerGenerated && bDeclarationGenerated;
				bool bAllowedOwner = false;
				if (Declaration.DeclarationKind == EAngelscriptCacheDeclarationKind::Property)
				{
					bAllowedOwner = bClassOrStruct || bGeneratedDelegatePair;
				}
				else
				{
					switch (Declaration.EntityKind)
					{
					case EAngelscriptArtifactEntityKind::Method:
						bAllowedOwner = bClassStructOrInterface || bGeneratedDelegatePair;
						break;
					case EAngelscriptArtifactEntityKind::Constructor:
					case EAngelscriptArtifactEntityKind::Destructor:
						bAllowedOwner = bClassOrStruct || bGeneratedDelegatePair;
						break;
					case EAngelscriptArtifactEntityKind::Factory:
						bAllowedOwner = bClassOrStruct;
						break;
					case EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor:
					case EAngelscriptArtifactEntityKind::GeneratedDefaultDestructor:
					case EAngelscriptArtifactEntityKind::InitDefaults:
						bAllowedOwner = bClassOrStruct
							|| (bDelegateOwner && bDeclarationGenerated);
						break;
					case EAngelscriptArtifactEntityKind::DelegateSignature:
						bAllowedOwner = Owner.EntityKind == EAngelscriptArtifactEntityKind::Delegate
							|| Owner.EntityKind == EAngelscriptArtifactEntityKind::Funcdef;
						break;
					default:
						break;
					}
				}
				if (!bAllowedOwner)
				{
					return Failure(EAngelscriptCacheValidationError::WrongReferenceKind,
						EAngelscriptCacheRecordKind::ModuleInterface);
				}
			}
		}

		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::Imports});
		for (int32 ImportIndex = 0; ImportIndex < Value.Imports.Num(); ++ImportIndex)
		{
			FAngelscriptCachedImportDeclaration& Import = Value.Imports[ImportIndex];
			const FAngelscriptCacheValidationResult ImportResult = PrepareImport(
				Value.ModuleKey,
				Import,
				bCanonicalize,
				bRequireCanonicalOrder,
				ExactOffsets,
				static_cast<uint32>(ImportIndex),
				&SemanticFieldOffset);
			if (!ImportResult.IsSuccess())
			{
				return ImportResult;
			}
		}
		int32 ImportFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DuplicateResult = ValidateStableHashAuthorityDuplicates(
			Value.Imports,
			[](const auto& Item) { return Item.ImportKey.Hash; },
			[](const auto& A, const auto& B)
			{
				return A.CanonicalNamespace == B.CanonicalNamespace
					&& A.CanonicalName == B.CanonicalName
					&& A.CanonicalSignature == B.CanonicalSignature
					&& A.TargetModuleKey.Hash == B.TargetModuleKey.Hash
					&& A.TargetDeclaration.StableKey == B.TargetDeclaration.StableKey
					&& A.TargetDeclaration.ExpectedAbi == B.TargetDeclaration.ExpectedAbi
					&& EqualSlots(A.Slots, B.Slots);
			}, &ImportFailureIndex); !DuplicateResult.IsSuccess())
		{
			SetExactOffset({EAngelscriptModuleInterfaceCapturedField::Import,
				static_cast<uint32>(ImportFailureIndex)});
			return DuplicateResult;
		}
		if (bCanonicalize)
		{
			Value.Imports.Sort([](const auto& A, const auto& B) { return CompareImport(A, B) < 0; });
		}
		else if (bRequireCanonicalOrder)
		{
			if (const FAngelscriptCacheValidationResult OrderResult = ValidateOrder(
				Value.Imports, CompareImport, &ImportFailureIndex);
				!OrderResult.IsSuccess())
			{
				SetExactOffset({EAngelscriptModuleInterfaceCapturedField::Import,
					static_cast<uint32>(ImportFailureIndex)});
				return OrderResult;
			}
		}
		if (const FAngelscriptCacheValidationResult SlotResult = ValidateGlobalSlotOrdinals(
			Value, ExactOffsets, &SemanticFieldOffset, 4, 4);
			!SlotResult.IsSuccess())
		{
			return SlotResult;
		}

		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::Dependencies});
		if (ExactOffsets != nullptr)
		{
			for (int32 DependencyIndex = 0;
				DependencyIndex < Value.Dependencies.Num(); ++DependencyIndex)
			{
				const FAngelscriptCacheSemanticDependency& Dependency =
					Value.Dependencies[DependencyIndex];
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DependencyKind,
					static_cast<uint32>(DependencyIndex)});
				const uint8 RawKind = static_cast<uint8>(Dependency.Kind);
				if (RawKind < 1 || RawKind > 12)
				{
					return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
				}

				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DependencyTargetReferenceKind,
					static_cast<uint32>(DependencyIndex)});
				const uint8 RawReferenceKind = static_cast<uint8>(Dependency.Target.Kind);
				if (RawReferenceKind < 1 || RawReferenceKind > 9)
				{
					return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
				}
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DependencyTargetStableKey,
					static_cast<uint32>(DependencyIndex)});
				if (Dependency.Target.StableKey.IsZero())
				{
					return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
				}
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DependencyTargetExpectedAbi,
					static_cast<uint32>(DependencyIndex)});
				const bool bRequiresAbi = RawReferenceKind <= static_cast<uint8>(
					EAngelscriptCacheReferenceKind::EnvironmentSymbol);
				if (bRequiresAbi && Dependency.Target.ExpectedAbi.IsZero())
				{
					return Failure(EAngelscriptCacheValidationError::MissingExpectedAbi);
				}
				if (!bRequiresAbi && !Dependency.Target.ExpectedAbi.IsZero())
				{
					return Failure(EAngelscriptCacheValidationError::ForbiddenExpectedAbi);
				}
				SetExactOffset({
					EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValuePresence,
					static_cast<uint32>(DependencyIndex)});
				if (DependencyRequiresContent(Dependency.Kind)
					!= Dependency.ExpectedContentOrValue.IsSet())
				{
					return Failure(EAngelscriptCacheValidationError::InvalidPresence);
				}
				if (Dependency.ExpectedContentOrValue.IsSet())
				{
					SetExactOffset({
						EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValue,
						static_cast<uint32>(DependencyIndex)});
					if (Dependency.ExpectedContentOrValue->IsZero())
					{
						return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
					}
				}
			}
		}
		int32 DependencyFailureIndex = INDEX_NONE;
		if (const FAngelscriptCacheValidationResult DependencyResult = PrepareDependencies(
			Value.Dependencies,
			bCanonicalize,
			bRequireCanonicalOrder,
			&DependencyFailureIndex);
			!DependencyResult.IsSuccess())
		{
			if (DependencyFailureIndex != INDEX_NONE)
			{
				SetExactOffset({EAngelscriptModuleInterfaceCapturedField::Dependency,
					static_cast<uint32>(DependencyFailureIndex)});
			}
			return DependencyResult;
		}
		for (int32 DependencyIndex = 0;
			DependencyIndex < Value.Dependencies.Num(); ++DependencyIndex)
		{
			const FAngelscriptCacheSemanticDependency& Dependency =
				Value.Dependencies[DependencyIndex];
			SetExactOffset({
				EAngelscriptModuleInterfaceCapturedField::DependencyTargetReferenceKind,
				static_cast<uint32>(DependencyIndex)});
			if (Dependency.Target.Kind == EAngelscriptCacheReferenceKind::CanonicalName
				|| Dependency.Target.Kind == EAngelscriptCacheReferenceKind::StringLiteral)
			{
				return Failure(EAngelscriptCacheValidationError::WrongReferenceKind);
			}
		}

		SetExactOffset({EAngelscriptModuleInterfaceCapturedField::CanonicalNamespaces});
		TArray<uint8> UsedNamespaces;
		UsedNamespaces.SetNumZeroed(Value.CanonicalNamespaces.Num());
		const auto FindNamespace = [&](const FString& Candidate)
		{
			int32 Lower = 0;
			int32 Upper = Value.CanonicalNamespaces.Num();
			while (Lower < Upper)
			{
				const int32 Middle = Lower + (Upper - Lower) / 2;
				if (CompareString(Value.CanonicalNamespaces[Middle], Candidate) < 0)
				{
					Lower = Middle + 1;
				}
				else
				{
					Upper = Middle;
				}
			}
			return Lower < Value.CanonicalNamespaces.Num()
				&& Value.CanonicalNamespaces[Lower] == Candidate ? Lower : INDEX_NONE;
		};
		for (const auto& Declaration : Value.Declarations)
		{
			if (Declaration.CanonicalNamespace.IsEmpty()) { continue; }
			const int32 NamespaceIndex = FindNamespace(Declaration.CanonicalNamespace);
			if (NamespaceIndex == INDEX_NONE)
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			UsedNamespaces[NamespaceIndex] = 1;
		}
		for (const auto& Import : Value.Imports)
		{
			if (Import.CanonicalNamespace.IsEmpty()) { continue; }
			const int32 NamespaceIndex = FindNamespace(Import.CanonicalNamespace);
			if (NamespaceIndex == INDEX_NONE)
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
			UsedNamespaces[NamespaceIndex] = 1;
		}
		for (const uint8 bUsed : UsedNamespaces)
		{
			if (bUsed == 0)
			{
				return Failure(EAngelscriptCacheValidationError::InvalidPresence);
			}
		}
		if (bValidateStoredAbi)
		{
			SetExactOffset({EAngelscriptModuleInterfaceCapturedField::InterfaceAbi});
			const FAngelscriptHash256 ComputedAbi = HashModuleInterfaceAbi(Value);
			if (!(ComputedAbi == Value.InterfaceAbi))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch,
					EAngelscriptCacheRecordKind::ModuleInterface);
			}
		}
		return {};
		};
		const FAngelscriptCacheValidationResult Result = Prepare();
		if (!Result.IsSuccess())
		{
			return Failure(Result.Error, EAngelscriptCacheRecordKind::ModuleInterface,
				ExactOffsets != nullptr ? SemanticFieldOffset : 0);
		}
		return Result;
	}

	static void WriteModuleInterfacePayload(FWriter& Writer, const FAngelscriptCachedModuleInterface& Value)
	{
		Writer.WriteUInt32(Value.PayloadSchemaVersion);
		Writer.WriteHash(Value.ModuleKey.Hash);
		Writer.WriteString(Value.CanonicalModuleName);
		Writer.WriteHash(Value.InterfaceAbi);
		Writer.WriteUInt32(static_cast<uint32>(Value.CanonicalNamespaces.Num()));
		for (const FString& Namespace : Value.CanonicalNamespaces) { Writer.WriteString(Namespace); }
		Writer.WriteUInt32(static_cast<uint32>(Value.Declarations.Num()));
		for (const auto& Declaration : Value.Declarations) { WriteDeclaration(Writer, Declaration); }
		Writer.WriteUInt32(static_cast<uint32>(Value.Imports.Num()));
		for (const auto& Import : Value.Imports) { WriteImport(Writer, Import); }
		Writer.WriteUInt32(static_cast<uint32>(Value.Dependencies.Num()));
		for (const auto& Dependency : Value.Dependencies) { WriteSemanticDependency(Writer, Dependency); }
	}

	template <typename EntryType>
	static bool ReadCapturedModuleOptionalString(
		FReader& Reader,
		TOptional<FString>& OutValue,
		EntryType& PresenceOffset,
		TOptional<EntryType>& ValueOffset,
		const FAngelscriptModuleInterfaceFieldCoordinate& PresenceCoordinate,
		const FAngelscriptModuleInterfaceFieldCoordinate& ValueCoordinate)
	{
		OutValue.Reset();
		ValueOffset.Reset();
		const uint64 TagOffset = Reader.GetOffset();
		CaptureOffset(PresenceOffset, PresenceCoordinate, TagOffset);
		uint8 Tag = 0;
		if (!Reader.ReadUInt8(Tag))
		{
			return false;
		}
		if (Tag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TagOffset);
			return false;
		}
		if (Tag == 0)
		{
			return true;
		}
		auto Entry = PresenceOffset;
		CaptureOffset(Entry, ValueCoordinate, Reader.GetOffset());
		ValueOffset = Entry;
		FString Value;
		if (!Reader.ReadString(Value))
		{
			return false;
		}
		OutValue = MoveTemp(Value);
		return true;
	}

	template <typename DataTypeOffsetsType>
	static bool ReadCapturedModuleDataType(
		FReader& Reader,
		FAngelscriptCachedDataType& OutValue,
		DataTypeOffsetsType& OutOffsets,
		const uint32 DeclarationIndex,
		const uint32 ParameterIndex,
		uint32& InOutNodeOrdinal,
		const uint64 Depth)
	{
		if (Depth > Reader.GetLimits().MaxNestingDepth)
		{
			Reader.Fail(EAngelscriptCacheValidationError::NestingDepthExceeded);
			return false;
		}
		const bool bParameter = ParameterIndex != MAX_uint32;
		const uint32 NodeOrdinal = InOutNodeOrdinal++;
		const auto Coordinate = [&](
			const EAngelscriptModuleInterfaceCapturedField DeclaredField,
			const EAngelscriptModuleInterfaceCapturedField ParameterField)
		{
			return bParameter
				? FAngelscriptModuleInterfaceFieldCoordinate{
					ParameterField, DeclarationIndex, ParameterIndex, NodeOrdinal}
				: FAngelscriptModuleInterfaceFieldCoordinate{
					DeclaredField, DeclarationIndex, NodeOrdinal, MAX_uint32};
		};
		const uint64 KindOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], Coordinate(
			EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeNode,
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeNode), KindOffset);
		CaptureOffset(OutOffsets.Fields[1], Coordinate(
			EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeKind,
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeKind), KindOffset);
		if (!ReadEnum(Reader, OutValue.Kind, 1, 4))
		{
			return false;
		}
		const uint64 PrimitiveOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[2], Coordinate(
			EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypePrimitive,
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypePrimitive),
			PrimitiveOffset);
		uint8 PrimitiveRaw = 0;
		if (!Reader.ReadUInt8(PrimitiveRaw))
		{
			return false;
		}
		if (PrimitiveRaw > 12)
		{
			Reader.Fail(EAngelscriptCacheValidationError::UnknownEnumValue, PrimitiveOffset);
			return false;
		}
		OutValue.Primitive = static_cast<EAngelscriptCachedPrimitiveType>(PrimitiveRaw);

		const uint64 PresenceOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[3], Coordinate(
			EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferencePresence,
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferencePresence),
			PresenceOffset);
		uint8 ReferenceTag = 0;
		if (!Reader.ReadUInt8(ReferenceTag))
		{
			return false;
		}
		if (ReferenceTag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, PresenceOffset);
			return false;
		}
		if (ReferenceTag == 1)
		{
			if (!Reader.ConsumeReference())
			{
				return false;
			}
			const uint64 ReferenceOffset = Reader.GetOffset();
			auto Entry = OutOffsets.Fields[0];
			CaptureOffset(Entry, Coordinate(
				EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReference,
				EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReference),
				ReferenceOffset);
			OutOffsets.Reference = Entry;
			CaptureOffset(Entry, Coordinate(
				EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceKind,
				EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceKind),
				ReferenceOffset);
			OutOffsets.ReferenceKind = Entry;
			FAngelscriptCacheStableReference Reference;
			if (!ReadEnum(Reader, Reference.Kind, 1, 9))
			{
				return false;
			}
			CaptureOffset(Entry, Coordinate(
				EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceStableKey,
				EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceStableKey),
				Reader.GetOffset());
			OutOffsets.ReferenceStableKey = Entry;
			if (!Reader.ReadHash(Reference.StableKey))
			{
				return false;
			}
			CaptureOffset(Entry, Coordinate(
				EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeReferenceExpectedAbi,
				EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeReferenceExpectedAbi),
				Reader.GetOffset());
			OutOffsets.ReferenceExpectedAbi = Entry;
			if (!Reader.ReadHash(Reference.ExpectedAbi))
			{
				return false;
			}
			OutValue.TypeReference = MoveTemp(Reference);
		}

		const uint64 QualifierOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[4], Coordinate(
			EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeQualifierFlags,
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeQualifierFlags),
			QualifierOffset);
		if (!Reader.ReadUInt32(OutValue.QualifierFlags))
		{
			return false;
		}
		const uint64 SubTypesOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[5], Coordinate(
			EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypeOrderedSubTypes,
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTypeOrderedSubTypes),
			SubTypesOffset);
		uint32 Count = 0;
		if (!Reader.ReadArrayCountAndReserve(11, OutValue.OrderedSubTypes, Count)
			|| !Reader.ReserveDecodedArrayAtOffset(
				SubTypesOffset, Count, OutOffsets.SubTypes))
		{
			return false;
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FAngelscriptCachedDataType SubType;
			auto& SubTypeOffsets = OutOffsets.SubTypes.AddDefaulted_GetRef();
			if (!ReadCapturedModuleDataType(
				Reader,
				SubType,
				SubTypeOffsets,
				DeclarationIndex,
				ParameterIndex,
				InOutNodeOrdinal,
				Depth + 1))
			{
				return false;
			}
			OutValue.OrderedSubTypes.Add(MoveTemp(SubType));
		}
		return true;
	}

	template <typename ParameterOffsetsType>
	static bool ReadCapturedModuleParameter(
		FReader& Reader,
		FAngelscriptCachedParameter& OutValue,
		ParameterOffsetsType& OutOffsets,
		const uint32 DeclarationIndex,
		const uint32 ParameterIndex)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameter,
			DeclarationIndex, ParameterIndex}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterOrdinal,
			DeclarationIndex, ParameterIndex}, RowOffset);
		if (!Reader.ReadUInt32(OutValue.Ordinal)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterCanonicalName,
			DeclarationIndex, ParameterIndex}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalName)) { return false; }
		uint32 NodeOrdinal = 0;
		if (!ReadCapturedModuleDataType(
			Reader,
			OutValue.Type,
			OutOffsets.Type,
			DeclarationIndex,
			ParameterIndex,
			NodeOrdinal,
			1)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterPassing,
			DeclarationIndex, ParameterIndex}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.Passing, 1, 4)) { return false; }
		if (!ReadCapturedModuleOptionalString(
			Reader,
			OutValue.CanonicalDefaultExpression,
			OutOffsets.Fields[4],
			OutOffsets.DefaultExpression,
			{EAngelscriptModuleInterfaceCapturedField::DeclarationParameterDefaultExpressionPresence,
				DeclarationIndex, ParameterIndex},
			{EAngelscriptModuleInterfaceCapturedField::DeclarationParameterDefaultExpression,
				DeclarationIndex, ParameterIndex})) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationParameterTraitFlags,
			DeclarationIndex, ParameterIndex}, Reader.GetOffset());
		if (!Reader.ReadUInt32(OutValue.TraitFlags)) { return false; }
		return true;
	}

	template <typename MetadataOffsetsType>
	static bool ReadCapturedModuleMetadata(
		FReader& Reader,
		FAngelscriptCachedMetadataEntry& OutValue,
		MetadataOffsetsType& OutOffsets,
		const uint32 DeclarationIndex,
		const uint32 MetadataIndex)
	{
		const uint64 KeyOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationMetadataEntry,
			DeclarationIndex, MetadataIndex}, KeyOffset);
		CaptureOffset(OutOffsets.Fields[1], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationMetadataCanonicalKey,
			DeclarationIndex, MetadataIndex}, KeyOffset);
		if (!Reader.ReadString(OutValue.CanonicalKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationMetadataCanonicalValue,
			DeclarationIndex, MetadataIndex}, Reader.GetOffset());
		return Reader.ReadString(OutValue.CanonicalValue);
	}

	template <typename SlotOffsetsType>
	static bool ReadCapturedModuleSlot(
		FReader& Reader,
		FAngelscriptCachedDeclarationSlot& OutValue,
		SlotOffsetsType& OutOffsets,
		const EAngelscriptModuleInterfaceCapturedField RowField,
		const EAngelscriptModuleInterfaceCapturedField KindField,
		const EAngelscriptModuleInterfaceCapturedField OrdinalField,
		const uint32 PrimaryIndex,
		const uint32 SlotIndex)
	{
		const uint64 KindOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {
			RowField, PrimaryIndex, SlotIndex}, KindOffset);
		CaptureOffset(OutOffsets.Fields[1], {
			KindField, PrimaryIndex, SlotIndex}, KindOffset);
		if (!ReadEnum(Reader, OutValue.SlotKind, 1, 4)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {
			OrdinalField, PrimaryIndex, SlotIndex}, Reader.GetOffset());
		return Reader.ReadUInt32(OutValue.Ordinal);
	}

	template <typename DeclarationOffsetsType>
	static bool ReadCapturedModuleDeclaration(
		FReader& Reader,
		FAngelscriptCachedDeclaration& OutValue,
		DeclarationOffsetsType& OutOffsets,
		const uint32 DeclarationIndex)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {
			EAngelscriptModuleInterfaceCapturedField::Declaration, DeclarationIndex},
			RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationKind, DeclarationIndex},
			RowOffset);
		if (!ReadEnum(Reader, OutValue.DeclarationKind, 1, 4)) { return false; }
		const uint64 EntityOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[2], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationEntityKind,
			DeclarationIndex}, EntityOffset);
		uint8 EntityRaw = 0;
		if (!Reader.ReadUInt8(EntityRaw)) { return false; }
		OutValue.EntityKind = static_cast<EAngelscriptArtifactEntityKind>(EntityRaw);
		if (!IsKnownEntityKind(OutValue.EntityKind))
		{
			Reader.Fail(EAngelscriptCacheValidationError::UnknownEnumValue, EntityOffset);
			return false;
		}
		CaptureOffset(OutOffsets.Fields[3], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationSchemaCoverage,
			DeclarationIndex}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.SchemaCoverage, 1, 2)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationBodyCoverage,
			DeclarationIndex}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.BodyCoverage, 1, 2)) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationStableKey,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.StableKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[6], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationOwnerKind,
			DeclarationIndex}, Reader.GetOffset());
		if (!ReadEnum(Reader, OutValue.OwnerKind, 1, 4)) { return false; }
		CaptureOffset(OutOffsets.Fields[7], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationOwnerKey,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.OwnerKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[8], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationModuleKey,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.ModuleKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[9], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalNamespace,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalNamespace)) { return false; }
		CaptureOffset(OutOffsets.Fields[10], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalName,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalName)) { return false; }
		CaptureOffset(OutOffsets.Fields[11], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalDeclaration,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalDeclaration)) { return false; }

		const uint64 TraitsOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[12], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalIdentityTraits,
			DeclarationIndex}, TraitsOffset);
		uint32 TraitCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			4, OutValue.CanonicalIdentityTraits, TraitCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				TraitsOffset, TraitCount, OutOffsets.IdentityTraits))
		{
			return false;
		}
		for (uint32 TraitIndex = 0; TraitIndex < TraitCount; ++TraitIndex)
		{
			auto& Entry = OutOffsets.IdentityTraits.AddDefaulted_GetRef();
			CaptureOffset(Entry, {
				EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalIdentityTrait,
				DeclarationIndex, TraitIndex}, Reader.GetOffset());
			FString Trait;
			if (!Reader.ReadString(Trait)) { return false; }
			OutValue.CanonicalIdentityTraits.Add(MoveTemp(Trait));
		}

		if (!ReadCapturedModuleOptionalString(
			Reader,
			OutValue.CanonicalTypeSpelling,
			OutOffsets.Fields[13],
			OutOffsets.CanonicalTypeSpelling,
			{EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalTypeSpellingPresence,
				DeclarationIndex},
			{EAngelscriptModuleInterfaceCapturedField::DeclarationCanonicalTypeSpelling,
				DeclarationIndex})) { return false; }

		const uint64 TypeTagOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[14], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationDeclaredTypePresence,
			DeclarationIndex}, TypeTagOffset);
		uint8 TypeTag = 0;
		if (!Reader.ReadUInt8(TypeTag)) { return false; }
		if (TypeTag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TypeTagOffset);
			return false;
		}
		if (TypeTag == 1)
		{
			OutOffsets.DeclaredType.Emplace();
			FAngelscriptCachedDataType Type;
			uint32 NodeOrdinal = 0;
			if (!ReadCapturedModuleDataType(
				Reader,
				Type,
				OutOffsets.DeclaredType.GetValue(),
				DeclarationIndex,
				MAX_uint32,
				NodeOrdinal,
				1)) { return false; }
			OutValue.DeclaredType = MoveTemp(Type);
		}

		const uint64 ParametersOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[15], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationOrderedParameters,
			DeclarationIndex}, ParametersOffset);
		if (!ReadCapturedArray(
			Reader,
			OutValue.OrderedParameters,
			OutOffsets.Parameters,
			25,
			ParametersOffset,
			[DeclarationIndex](FReader& InReader,
				FAngelscriptCachedParameter& OutParameter,
				auto& ParameterOffsets,
				const uint32 ParameterIndex)
			{
				return ReadCapturedModuleParameter(
					InReader,
					OutParameter,
					ParameterOffsets,
					DeclarationIndex,
					ParameterIndex);
			})) { return false; }

		CaptureOffset(OutOffsets.Fields[16], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationTraitFlags,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadUInt32(OutValue.TraitFlags)) { return false; }
		CaptureOffset(OutOffsets.Fields[17], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationReflectionFlags,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadUInt32(OutValue.ReflectionFlags)) { return false; }

		const uint64 MetadataOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[18], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationMetadata,
			DeclarationIndex}, MetadataOffset);
		if (!ReadCapturedArray(
			Reader,
			OutValue.Metadata,
			OutOffsets.Metadata,
			8,
			MetadataOffset,
			[DeclarationIndex](FReader& InReader,
				FAngelscriptCachedMetadataEntry& OutMetadata,
				auto& MetadataOffsets,
				const uint32 MetadataIndex)
			{
				return ReadCapturedModuleMetadata(
					InReader,
					OutMetadata,
					MetadataOffsets,
					DeclarationIndex,
					MetadataIndex);
			})) { return false; }

		const uint64 SlotsOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[19], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationSlots,
			DeclarationIndex}, SlotsOffset);
		if (!ReadCapturedArray(
			Reader,
			OutValue.Slots,
			OutOffsets.Slots,
			5,
			SlotsOffset,
			[DeclarationIndex](FReader& InReader,
				FAngelscriptCachedDeclarationSlot& OutSlot,
				auto& SlotOffsets,
				const uint32 SlotIndex)
			{
				return ReadCapturedModuleSlot(
					InReader,
					OutSlot,
					SlotOffsets,
					EAngelscriptModuleInterfaceCapturedField::DeclarationSlot,
					EAngelscriptModuleInterfaceCapturedField::DeclarationSlotKind,
					EAngelscriptModuleInterfaceCapturedField::DeclarationSlotOrdinal,
					DeclarationIndex,
					SlotIndex);
			})) { return false; }
		CaptureOffset(OutOffsets.Fields[20], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationSignatureHash,
			DeclarationIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.SignatureHash)) { return false; }
		CaptureOffset(OutOffsets.Fields[21], {
			EAngelscriptModuleInterfaceCapturedField::DeclarationTraitsHash,
			DeclarationIndex}, Reader.GetOffset());
		return Reader.ReadHash(OutValue.TraitsHash);
	}

	template <typename ImportOffsetsType>
	static bool ReadCapturedModuleImport(
		FReader& Reader,
		FAngelscriptCachedImportDeclaration& OutValue,
		ImportOffsetsType& OutOffsets,
		const uint32 ImportIndex)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {
			EAngelscriptModuleInterfaceCapturedField::Import, ImportIndex}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {
			EAngelscriptModuleInterfaceCapturedField::ImportKey, ImportIndex}, RowOffset);
		if (!Reader.ReadHash(OutValue.ImportKey.Hash)) { return false; }
		CaptureOffset(OutOffsets.Fields[2], {
			EAngelscriptModuleInterfaceCapturedField::ImportCanonicalNamespace,
			ImportIndex}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalNamespace)) { return false; }
		CaptureOffset(OutOffsets.Fields[3], {
			EAngelscriptModuleInterfaceCapturedField::ImportCanonicalName,
			ImportIndex}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalName)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {
			EAngelscriptModuleInterfaceCapturedField::ImportCanonicalSignature,
			ImportIndex}, Reader.GetOffset());
		if (!Reader.ReadString(OutValue.CanonicalSignature)) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {
			EAngelscriptModuleInterfaceCapturedField::ImportTargetModuleKey,
			ImportIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.TargetModuleKey.Hash)) { return false; }
		if (!Reader.ConsumeReference()) { return false; }
		const uint64 ReferenceOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[6], {
			EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclaration,
			ImportIndex}, ReferenceOffset);
		CaptureOffset(OutOffsets.Fields[7], {
			EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationReferenceKind,
			ImportIndex}, ReferenceOffset);
		if (!ReadEnum(Reader, OutValue.TargetDeclaration.Kind, 1, 9)) { return false; }
		CaptureOffset(OutOffsets.Fields[8], {
			EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationStableKey,
			ImportIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.TargetDeclaration.StableKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[9], {
			EAngelscriptModuleInterfaceCapturedField::ImportTargetDeclarationExpectedAbi,
			ImportIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.TargetDeclaration.ExpectedAbi)) { return false; }
		const uint64 SlotsOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[10], {
			EAngelscriptModuleInterfaceCapturedField::ImportSlots,
			ImportIndex}, SlotsOffset);
		return ReadCapturedArray(
			Reader,
			OutValue.Slots,
			OutOffsets.Slots,
			5,
			SlotsOffset,
			[ImportIndex](FReader& InReader,
				FAngelscriptCachedDeclarationSlot& OutSlot,
				auto& SlotOffsets,
				const uint32 SlotIndex)
			{
				return ReadCapturedModuleSlot(
					InReader,
					OutSlot,
					SlotOffsets,
					EAngelscriptModuleInterfaceCapturedField::ImportSlot,
					EAngelscriptModuleInterfaceCapturedField::ImportSlotKind,
					EAngelscriptModuleInterfaceCapturedField::ImportSlotOrdinal,
					ImportIndex,
					SlotIndex);
			});
	}

	template <typename DependencyOffsetsType>
	static bool ReadCapturedModuleDependency(
		FReader& Reader,
		FAngelscriptCacheSemanticDependency& OutValue,
		DependencyOffsetsType& OutOffsets,
		const uint32 DependencyIndex)
	{
		const uint64 RowOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[0], {
			EAngelscriptModuleInterfaceCapturedField::Dependency,
			DependencyIndex}, RowOffset);
		CaptureOffset(OutOffsets.Fields[1], {
			EAngelscriptModuleInterfaceCapturedField::DependencyKind,
			DependencyIndex}, RowOffset);
		if (!ReadEnum(Reader, OutValue.Kind, 1, 12)) { return false; }
		if (!Reader.ConsumeReference()) { return false; }
		const uint64 ReferenceOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[2], {
			EAngelscriptModuleInterfaceCapturedField::DependencyTarget,
			DependencyIndex}, ReferenceOffset);
		CaptureOffset(OutOffsets.Fields[3], {
			EAngelscriptModuleInterfaceCapturedField::DependencyTargetReferenceKind,
			DependencyIndex}, ReferenceOffset);
		if (!ReadEnum(Reader, OutValue.Target.Kind, 1, 9)) { return false; }
		CaptureOffset(OutOffsets.Fields[4], {
			EAngelscriptModuleInterfaceCapturedField::DependencyTargetStableKey,
			DependencyIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.Target.StableKey)) { return false; }
		CaptureOffset(OutOffsets.Fields[5], {
			EAngelscriptModuleInterfaceCapturedField::DependencyTargetExpectedAbi,
			DependencyIndex}, Reader.GetOffset());
		if (!Reader.ReadHash(OutValue.Target.ExpectedAbi)) { return false; }
		const uint64 PresenceOffset = Reader.GetOffset();
		CaptureOffset(OutOffsets.Fields[6], {
			EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValuePresence,
			DependencyIndex}, PresenceOffset);
		uint8 Tag = 0;
		if (!Reader.ReadUInt8(Tag)) { return false; }
		if (Tag > 1)
		{
			Reader.Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, PresenceOffset);
			return false;
		}
		if (Tag == 1)
		{
			auto Entry = OutOffsets.Fields[0];
			CaptureOffset(Entry, {
				EAngelscriptModuleInterfaceCapturedField::DependencyExpectedContentOrValue,
				DependencyIndex}, Reader.GetOffset());
			OutOffsets.ExpectedContentOrValue = Entry;
			FAngelscriptHash256 Value;
			if (!Reader.ReadHash(Value)) { return false; }
			OutValue.ExpectedContentOrValue = Value;
		}
		return true;
	}

	template <typename ModuleOffsetsType>
	static bool ReadCapturedModuleInterfacePayload(
		FReader& Reader,
		FAngelscriptCachedModuleInterface& OutValue,
		ModuleOffsetsType& OutOffsets)
	{
		const uint64 SchemaOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(SchemaOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[0], {
			EAngelscriptModuleInterfaceCapturedField::PayloadSchemaVersion},
			SchemaOffset);
		if (!Reader.ReadUInt32(OutValue.PayloadSchemaVersion)) { return false; }

		const uint64 ModuleKeyOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(ModuleKeyOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[1], {
			EAngelscriptModuleInterfaceCapturedField::ModuleKey}, ModuleKeyOffset);
		if (!Reader.ReadHash(OutValue.ModuleKey.Hash)) { return false; }

		const uint64 ModuleNameOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(ModuleNameOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[2], {
			EAngelscriptModuleInterfaceCapturedField::CanonicalModuleName}, ModuleNameOffset);
		if (!Reader.ReadString(OutValue.CanonicalModuleName)) { return false; }

		const uint64 InterfaceAbiOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(InterfaceAbiOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[3], {
			EAngelscriptModuleInterfaceCapturedField::InterfaceAbi}, InterfaceAbiOffset);
		if (!Reader.ReadHash(OutValue.InterfaceAbi)) { return false; }

		const uint64 NamespacesOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(NamespacesOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[4], {
			EAngelscriptModuleInterfaceCapturedField::CanonicalNamespaces},
			NamespacesOffset);
		uint32 NamespaceCount = 0;
		if (!Reader.ReadArrayCountAndReserve(
			4, OutValue.CanonicalNamespaces, NamespaceCount)
			|| !Reader.ReserveDecodedArrayAtOffset(
				NamespacesOffset, NamespaceCount, OutOffsets.CanonicalNamespaces))
		{
			return false;
		}
		for (uint32 NamespaceIndex = 0; NamespaceIndex < NamespaceCount; ++NamespaceIndex)
		{
			auto& Entry = OutOffsets.CanonicalNamespaces.AddDefaulted_GetRef();
			CaptureOffset(Entry, {
				EAngelscriptModuleInterfaceCapturedField::CanonicalNamespace,
				NamespaceIndex}, Reader.GetOffset());
			FString Namespace;
			if (!Reader.ReadString(Namespace)) { return false; }
			OutValue.CanonicalNamespaces.Add(MoveTemp(Namespace));
		}

		const uint64 DeclarationsOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(DeclarationsOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[5], {
			EAngelscriptModuleInterfaceCapturedField::Declarations}, DeclarationsOffset);
		if (!ReadCapturedArray(
			Reader,
			OutValue.Declarations,
			OutOffsets.Declarations,
			203,
			DeclarationsOffset,
			[](FReader& InReader,
				FAngelscriptCachedDeclaration& OutDeclaration,
				auto& DeclarationOffsets,
				const uint32 DeclarationIndex)
			{
				return ReadCapturedModuleDeclaration(
					InReader, OutDeclaration, DeclarationOffsets, DeclarationIndex);
			})) { return false; }

		const uint64 ImportsOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(ImportsOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[6], {
			EAngelscriptModuleInterfaceCapturedField::Imports}, ImportsOffset);
		if (!ReadCapturedArray(
			Reader,
			OutValue.Imports,
			OutOffsets.Imports,
			145,
			ImportsOffset,
			[](FReader& InReader,
				FAngelscriptCachedImportDeclaration& OutImport,
				auto& ImportOffsets,
				const uint32 ImportIndex)
			{
				return ReadCapturedModuleImport(
					InReader, OutImport, ImportOffsets, ImportIndex);
			})) { return false; }

		const uint64 DependenciesOffset = Reader.GetOffset();
		Reader.SetSemanticEnclosingFieldOffset(DependenciesOffset);
		CaptureOffset(OutOffsets.HeaderOffsets[7], {
			EAngelscriptModuleInterfaceCapturedField::Dependencies}, DependenciesOffset);
		return ReadCapturedArray(
			Reader,
			OutValue.Dependencies,
			OutOffsets.Dependencies,
			67,
			DependenciesOffset,
			[](FReader& InReader,
				FAngelscriptCacheSemanticDependency& OutDependency,
				auto& DependencyOffsets,
				const uint32 DependencyIndex)
			{
				return ReadCapturedModuleDependency(
					InReader, OutDependency, DependencyOffsets, DependencyIndex);
			});
	}
}

using namespace AngelscriptCacheSemanticRecords_Private;

enum class ECanonicalDataTypeOutputScanResult : uint8
{
	NotAliased,
	Aliased,
	InvalidArrayView,
	NestingDepthExceeded,
	BudgetExceeded,
};

static ECanonicalDataTypeOutputScanResult ScanCanonicalDataTypeOutputAllocations(
	const AngelscriptCacheMemoryView_Private::FAddressRange& InputRange,
	const FAngelscriptCachedDataType& Output,
	const uint64 MaxNestingDepth,
	const uint64 MaxArrayElements,
	const uint64 Depth,
	uint64& InOutVisitedElements)
{
	if (Depth > MaxNestingDepth)
	{
		return ECanonicalDataTypeOutputScanResult::NestingDepthExceeded;
	}

	AngelscriptCacheMemoryView_Private::FAddressRange AllocationRange;
	if (!AngelscriptCacheMemoryView_Private::TryGetAllocationRange(
		Output.OrderedSubTypes, AllocationRange))
	{
		return ECanonicalDataTypeOutputScanResult::InvalidArrayView;
	}
	if (AngelscriptCacheMemoryView_Private::DoRangesOverlap(
		InputRange, AllocationRange))
	{
		return ECanonicalDataTypeOutputScanResult::Aliased;
	}

	const uint64 ElementCount = static_cast<uint64>(Output.OrderedSubTypes.Num());
	if (ElementCount > MaxArrayElements - FMath::Min(InOutVisitedElements, MaxArrayElements))
	{
		return ECanonicalDataTypeOutputScanResult::BudgetExceeded;
	}
	InOutVisitedElements += ElementCount;

	for (const FAngelscriptCachedDataType& SubType : Output.OrderedSubTypes)
	{
		const ECanonicalDataTypeOutputScanResult Result =
			ScanCanonicalDataTypeOutputAllocations(
				InputRange,
				SubType,
				MaxNestingDepth,
				MaxArrayElements,
				Depth + 1,
				InOutVisitedElements);
		if (Result != ECanonicalDataTypeOutputScanResult::NotAliased)
		{
			return Result;
		}
	}
	return ECanonicalDataTypeOutputScanResult::NotAliased;
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeCanonicalString(
	const FStringView Value,
	TArray<uint8>& OutBytes)
{
	bool bAliased = false;
	if (!AngelscriptCacheMemoryView_Private::TryIsViewAliasedWithAllocation(
		Value, OutBytes, bAliased))
	{
		OutBytes.Reset();
		return Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	}
	if (bAliased)
	{
		OutBytes.Reset();
		return Failure(EAngelscriptCacheValidationError::AliasedInputOutput);
	}

	OutBytes.Reset();
	const FAngelscriptCacheValidationResult Result = ValidateString(Value);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	Writer.WriteString(FString(Value));
	OutBytes = MoveTemp(Writer.Bytes);
	return {};
}

static FAngelscriptCacheValidationResult DeserializeCanonicalStringInternal(
	const TConstArrayView<uint8> Bytes,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	const FDecodedChargeSink DecodedChargeSink,
	FString& OutValue)
{
	bool bAliased = false;
	if (!AngelscriptCacheMemoryView_Private::TryIsViewAliasedWithAllocation(
		Bytes, OutValue, bAliased))
	{
		OutValue.Empty();
		return Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	}
	if (bAliased)
	{
		OutValue.Empty();
		return Failure(EAngelscriptCacheValidationError::AliasedInputOutput);
	}

	OutValue.Empty();
	const FAngelscriptCacheValidationResult BeginResult = BeginRead(
		Bytes, Limits, Budget, static_cast<EAngelscriptCacheRecordKind>(0));
	if (!BeginResult.IsSuccess())
	{
		return BeginResult;
	}
	FReader Reader(
		Bytes,
		Limits,
		Budget,
		static_cast<EAngelscriptCacheRecordKind>(0),
		DecodedChargeSink);
	FString Local;
	if (!Reader.ReadString(Local))
	{
		return Reader.GetResult();
	}
	if (!Reader.IsAtEnd())
	{
		Reader.Fail(EAngelscriptCacheValidationError::TrailingData, Reader.GetOffset());
		return Reader.GetResult();
	}
	OutValue = MoveTemp(Local);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::DeserializeCanonicalString(
	const TConstArrayView<uint8> Bytes,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FString& OutValue)
{
	auto Candidate = FAngelscriptCacheSemanticCandidateAccess::Begin(Budget, Limits);
	const FDecodedChargeSink ChargeSink =
		MakeCandidateDecodedChargeSink(Candidate);
	const FAngelscriptCacheValidationResult Result =
		DeserializeCanonicalStringInternal(
			Bytes, Limits, Budget, ChargeSink, OutValue);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!FAngelscriptCacheSemanticCandidateAccess::Promote(Candidate))
	{
		OutValue.Empty();
		return Failure(EAngelscriptCacheValidationError::Overflow);
	}
	return {};
}

#if WITH_ANGELSCRIPT_UNITTESTS
FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::
	DeserializeCanonicalStringWithAllocationCaptureForTests(
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		AngelscriptCacheCanonicalCodecTestHooks::FAllocationEventCaptureView Capture,
		FString& OutValue)
{
	const AngelscriptCacheCanonicalCodec_Private::FDecodedAllocationObserverForTests Observer =
		MakeExplicitAllocationObserverForTests(Capture);
	auto Candidate = FAngelscriptCacheSemanticCandidateAccess::Begin(Budget, Limits);
	const FDecodedChargeSink ChargeSink =
		MakeCandidateDecodedChargeSink(Candidate, Observer);
	const FAngelscriptCacheValidationResult Result =
		DeserializeCanonicalStringInternal(
			Bytes, Limits, Budget, ChargeSink, OutValue);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!FAngelscriptCacheSemanticCandidateAccess::Promote(Candidate))
	{
		OutValue.Empty();
		return Failure(EAngelscriptCacheValidationError::Overflow);
	}
	return {};
}
#endif

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeCanonicalDataType(
	const FAngelscriptCachedDataType& Value,
	TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	const FAngelscriptCacheValidationResult Result = ValidateDataType(Value, 1);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	WriteDataType(Writer, Value);
	OutBytes = MoveTemp(Writer.Bytes);
	return {};
}

static FAngelscriptCacheValidationResult DeserializeCanonicalDataTypeInternal(
	const TConstArrayView<uint8> Bytes,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	const FDecodedChargeSink DecodedChargeSink,
	FAngelscriptCachedDataType& OutValue)
{
	FAngelscriptCachedDataType PreviousOutput = MoveTemp(OutValue);
	OutValue = {};
	const FAngelscriptCacheValidationResult BeginResult = BeginRead(
		Bytes, Limits, Budget, static_cast<EAngelscriptCacheRecordKind>(0));
	if (!BeginResult.IsSuccess())
	{
		return BeginResult;
	}

	AngelscriptCacheMemoryView_Private::FAddressRange InputRange;
	if (!AngelscriptCacheMemoryView_Private::TryGetViewRange(Bytes, InputRange))
	{
		return Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	}
	uint64 VisitedElements = 0;
	const uint64 EffectiveMaxNestingDepth = FMath::Min(
		Limits.MaxNestingDepth,
		FAngelscriptCacheReadLimits::DefaultMaxNestingDepth);
	const ECanonicalDataTypeOutputScanResult OutputScanResult =
		ScanCanonicalDataTypeOutputAllocations(
			InputRange,
			PreviousOutput,
			EffectiveMaxNestingDepth,
			Limits.MaxArrayElements,
			1,
			VisitedElements);
	switch (OutputScanResult)
	{
	case ECanonicalDataTypeOutputScanResult::NotAliased:
		break;
	case ECanonicalDataTypeOutputScanResult::Aliased:
		return Failure(EAngelscriptCacheValidationError::AliasedInputOutput);
	case ECanonicalDataTypeOutputScanResult::InvalidArrayView:
		return Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	case ECanonicalDataTypeOutputScanResult::NestingDepthExceeded:
		return Failure(EAngelscriptCacheValidationError::NestingDepthExceeded);
	case ECanonicalDataTypeOutputScanResult::BudgetExceeded:
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded);
	default:
		return Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	}

	FReader Reader(
		Bytes,
		Limits,
		Budget,
		static_cast<EAngelscriptCacheRecordKind>(0),
		DecodedChargeSink);
	FAngelscriptCachedDataType Local;
	if (!ReadDataType(Reader, Local, 1))
	{
		return Reader.GetResult();
	}
	if (!Reader.IsAtEnd())
	{
		Reader.Fail(EAngelscriptCacheValidationError::TrailingData, Reader.GetOffset());
		return Reader.GetResult();
	}
	OutValue = MoveTemp(Local);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::
	DeserializeCanonicalDataType(
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCachedDataType& OutValue)
{
	auto Candidate = FAngelscriptCacheSemanticCandidateAccess::Begin(Budget, Limits);
	const FDecodedChargeSink ChargeSink =
		MakeCandidateDecodedChargeSink(Candidate);
	const FAngelscriptCacheValidationResult Result =
		DeserializeCanonicalDataTypeInternal(
			Bytes, Limits, Budget, ChargeSink, OutValue);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!FAngelscriptCacheSemanticCandidateAccess::Promote(Candidate))
	{
		OutValue = {};
		return Failure(EAngelscriptCacheValidationError::Overflow);
	}
	return {};
}

#if WITH_ANGELSCRIPT_UNITTESTS
FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::
	DeserializeCanonicalDataTypeWithAllocationCaptureForTests(
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		AngelscriptCacheCanonicalCodecTestHooks::FAllocationEventCaptureView Capture,
		FAngelscriptCachedDataType& OutValue)
{
	const AngelscriptCacheCanonicalCodec_Private::FDecodedAllocationObserverForTests Observer =
		MakeExplicitAllocationObserverForTests(Capture);
	auto Candidate = FAngelscriptCacheSemanticCandidateAccess::Begin(Budget, Limits);
	const FDecodedChargeSink ChargeSink =
		MakeCandidateDecodedChargeSink(Candidate, Observer);
	const FAngelscriptCacheValidationResult Result =
		DeserializeCanonicalDataTypeInternal(
			Bytes, Limits, Budget, ChargeSink, OutValue);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!FAngelscriptCacheSemanticCandidateAccess::Promote(Candidate))
	{
		OutValue = {};
		return Failure(EAngelscriptCacheValidationError::Overflow);
	}
	return {};
}
#endif

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeStableReference(
	const FAngelscriptCacheStableReference& Value,
	TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	const FAngelscriptCacheValidationResult Result = ValidateStableReference(Value);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	WriteStableReference(Writer, Value);
	OutBytes = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeSemanticDependency(
	const FAngelscriptCacheSemanticDependency& Value,
	TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	const FAngelscriptCacheValidationResult Result = ValidateSemanticDependency(Value);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	WriteSemanticDependency(Writer, Value);
	OutBytes = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeMetadataEntry(
	const FAngelscriptCachedMetadataEntry& Value,
	TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	const FAngelscriptCacheValidationResult Result = ValidateMetadata(Value);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	WriteMetadata(Writer, Value);
	OutBytes = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeParameter(
	const FAngelscriptCachedParameter& Value,
	TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	const FAngelscriptCacheValidationResult Result = ValidateParameter(Value);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	WriteParameter(Writer, Value);
	OutBytes = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeDeclarationSlot(
	const FAngelscriptCachedDeclarationSlot& Value,
	TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	const FAngelscriptCacheValidationResult Result = ValidateSlot(Value);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	WriteSlot(Writer, Value);
	OutBytes = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
	const FAngelscriptCachedDeclaration& Value,
	FAngelscriptHash256& OutSignatureHash,
	FAngelscriptHash256& OutTraitsHash)
{
	OutSignatureHash = {};
	OutTraitsHash = {};
	FAngelscriptCachedDeclaration Canonical = Value;
	const FAngelscriptCacheValidationResult Result = PrepareDeclaration(Canonical, true, false, false);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutSignatureHash = HashDeclarationSignature(Canonical);
	OutTraitsHash = HashDeclarationTraits(Canonical);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::TryBuildImportKey(
	const FAngelscriptImportIdentityInput& Input,
	FAngelscriptStableImportKey& OutKey)
{
	OutKey = {};
	if (Input.ModuleKey.Hash.IsZero() || Input.TargetModuleKey.Hash.IsZero()
		|| Input.TargetFunctionKey.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	if (const FAngelscriptCacheValidationResult NamespaceResult = ValidateString(Input.CanonicalNamespace);
		!NamespaceResult.IsSuccess())
	{
		return NamespaceResult;
	}
	if (const FAngelscriptCacheValidationResult NameResult = ValidateRequiredString(Input.CanonicalName);
		!NameResult.IsSuccess())
	{
		return NameResult;
	}
	if (const FAngelscriptCacheValidationResult SignatureResult = ValidateRequiredString(Input.CanonicalSignature);
		!SignatureResult.IsSuccess())
	{
		return SignatureResult;
	}
	OutKey = HashImportKey(Input);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
	const FAngelscriptSourceProviderIdentityInput& Input,
	FAngelscriptCachedSourceProviderKey& OutKey)
{
	OutKey = {};
	if (static_cast<uint8>(Input.ProviderKind) < 1 || static_cast<uint8>(Input.ProviderKind) > 4)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	if (const FAngelscriptCacheValidationResult StringResult =
		ValidateRequiredString(Input.CanonicalImplementationIdentity); !StringResult.IsSuccess())
	{
		return StringResult;
	}
	OutKey = ComputeProviderKey(Input);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
	const FAngelscriptSourceMountIdentityInput& Input,
	FAngelscriptCachedSourceMountKey& OutKey)
{
	OutKey = {};
	if (static_cast<uint8>(Input.SourceKind) < 1 || static_cast<uint8>(Input.SourceKind) > 3)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	if (Input.ProviderKey.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	if (const FAngelscriptCacheValidationResult StringResult = ValidateRequiredString(Input.LogicalMount);
		!StringResult.IsSuccess())
	{
		return StringResult;
	}
	OutKey = ComputeMountKey(Input);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(
	const FAngelscriptPreprocessHookIdentityInput& Input,
	FAngelscriptCachedPreprocessHookKey& OutKey)
{
	OutKey = {};
	if (static_cast<uint8>(Input.Phase) < 1 || static_cast<uint8>(Input.Phase) > 3
		|| static_cast<uint8>(Input.AffectedScopeKind) < 1
		|| static_cast<uint8>(Input.AffectedScopeKind) > 5)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	if (Input.AffectedScopeStableKey.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	if (const FAngelscriptCacheValidationResult StringResult =
		ValidateRequiredString(Input.CanonicalImplementationIdentity); !StringResult.IsSuccess())
	{
		return StringResult;
	}
	OutKey = ComputeHookKey(Input);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::TryBuildSourceFileKey(
	const FAngelscriptSourceFileIdentityInput& Input,
	FAngelscriptCachedSourceFileKey& OutKey)
{
	OutKey = {};
	if (static_cast<uint8>(Input.SourceKind) < 1 || static_cast<uint8>(Input.SourceKind) > 3)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	if (Input.MountKey.Hash.IsZero() || Input.ProviderKey.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	FAngelscriptSourceFileIdentityInput Canonical = Input;
	if (const FAngelscriptCacheValidationResult PathResult =
		NormalizeRelativeLogicalPath(Canonical.RelativeLogicalPath); !PathResult.IsSuccess())
	{
		return PathResult;
	}
	if (Canonical.GeneratedSourceKey.IsSet() && Canonical.GeneratedSourceKey->IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	OutKey = ComputeSourceFileKey(Canonical);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::TryBuildPreprocessorInputKey(
	const FAngelscriptPreprocessorInputIdentityInput& Input,
	FAngelscriptCachedPreprocessorInputKey& OutKey)
{
	OutKey = {};
	if (static_cast<uint8>(Input.InputKind) < 1 || static_cast<uint8>(Input.InputKind) > 4)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	if (Input.OwnerScopeStableKey.IsZero()
		|| (Input.TargetStableKey.IsSet() && Input.TargetStableKey->IsZero()))
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	if (const FAngelscriptCacheValidationResult NameResult = ValidateRequiredString(Input.CanonicalName);
		!NameResult.IsSuccess())
	{
		return NameResult;
	}
	OutKey = ComputeInputKey(Input);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::TryBuildSourceEdgeKey(
	const FAngelscriptSourceEdgeIdentityInput& Input,
	FAngelscriptCachedSourceEdgeKey& OutKey)
{
	OutKey = {};
	if (static_cast<uint8>(Input.EdgeKind) < 1 || static_cast<uint8>(Input.EdgeKind) > 2)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue);
	}
	if (Input.FromSourceFileKey.Hash.IsZero() || Input.ToSourceOrGeneratedKey.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey);
	}
	if (const FAngelscriptCacheValidationResult IdentityResult =
		ValidateRequiredString(Input.CanonicalIncludeOrGeneratorIdentity); !IdentityResult.IsSuccess())
	{
		return IdentityResult;
	}
	OutKey = ComputeEdgeKey(Input);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
	const FAngelscriptCachedSourceIndex& Value,
	FAngelscriptHash256& OutHash)
{
	OutHash = {};
	FAngelscriptCachedSourceIndex Canonical = Value;
	const FAngelscriptCacheValidationResult Result = PrepareSourceIndex(Canonical, true, false, false);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutHash = HashSourceIndex(Canonical);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheSemanticArchive::CanonicalizeSourceIndex(
	FAngelscriptCachedSourceIndex& InOutValue)
{
	FAngelscriptCachedSourceIndex Canonical = InOutValue;
	const FAngelscriptCacheValidationResult Result = PrepareSourceIndex(
		Canonical, true, false, false);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Canonical.SourceSnapshot = HashSourceIndex(Canonical);
	InOutValue = MoveTemp(Canonical);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheSemanticArchive::ComputeDirectSourceInputDigest(
	const FAngelscriptCachedSourceIndex& Value,
	const FAngelscriptArtifactProfileKey& Profile,
	FAngelscriptHash256& OutHash)
{
	OutHash = {};
	if (Profile.Hash.IsZero())
	{
		return Failure(
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	FAngelscriptCachedSourceIndex Canonical = Value;
	const FAngelscriptCacheValidationResult Result = PrepareSourceIndex(
		Canonical, true, false, true);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutHash = HashDirectSourceInputs(Canonical, Profile);
	return {};
}

static FAngelscriptCacheValidationResult QueryExactFastPathEligibilityInternal(
	const FAngelscriptCachedSourceIndex& Canonical,
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	const FPreparedEligibilityIndexes* PreparedIndexes,
#if WITH_ANGELSCRIPT_UNITTESTS
	AngelscriptCacheEligibilityTestHooks::FAllocationEventCaptureView* AllocationCapture,
#endif
	FAngelscriptCacheTemporaryResidentReservation* OutMatchingReservation,
	FAngelscriptCacheExactFastPathEligibility& OutResult)
{
#if WITH_ANGELSCRIPT_UNITTESTS
	PrepareEligibilityAllocationCaptureForTests(AllocationCapture);
#endif
	OutResult.Reset();
	if (ModuleKey.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	uint64 ScratchBytes = 0;
	if (!TryComputeEligibilityScratchBytes(
		Canonical, PreparedIndexes != nullptr, ScratchBytes))
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	FAngelscriptCacheTemporaryResidentReservation ScratchReservation;
	if (!Budget.TryReserveTemporaryDecoded(ScratchBytes, Limits, ScratchReservation))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::SourceIndex);
	}

	TArray<FAngelscriptHash256> FileKeys;
	TArray<FAngelscriptHash256> MountKeys;
	TArray<FAngelscriptHash256> ProviderKeys;
	FileKeys.Reserve(Canonical.Files.Num());
	MountKeys.Reserve(Canonical.Files.Num());
	ProviderKeys.Reserve(Canonical.Files.Num());
#if WITH_ANGELSCRIPT_UNITTESTS
	AngelscriptCacheEligibilityTestHooks::FAllocationEvent PrimaryAllocationEvent;
	PrimaryAllocationEvent.Phase =
		AngelscriptCacheEligibilityTestHooks::EAllocationPhase::PrimaryScratchArrays;
	PrimaryAllocationEvent.RequestedCapacity = Canonical.Files.Num();
	PrimaryAllocationEvent.FirstCapacityBeforePopulation = FileKeys.Max();
	PrimaryAllocationEvent.SecondCapacityBeforePopulation = MountKeys.Max();
	PrimaryAllocationEvent.ThirdCapacityBeforePopulation = ProviderKeys.Max();
	PrimaryAllocationEvent.AllocatedBytesBeforePopulation =
		static_cast<uint64>(FileKeys.GetAllocatedSize())
		+ static_cast<uint64>(MountKeys.GetAllocatedSize())
		+ static_cast<uint64>(ProviderKeys.GetAllocatedSize());
#endif
	for (const FAngelscriptCachedSourceFile& File : Canonical.Files)
	{
		if (File.ModuleKey.Hash == ModuleKey.Hash)
		{
			FileKeys.Add(File.SourceFileKey.Hash);
			MountKeys.Add(File.MountKey.Hash);
			ProviderKeys.Add(File.ProviderKey.Hash);
		}
	}
#if WITH_ANGELSCRIPT_UNITTESTS
	PrimaryAllocationEvent.FirstCapacityAfterPopulation = FileKeys.Max();
	PrimaryAllocationEvent.SecondCapacityAfterPopulation = MountKeys.Max();
	PrimaryAllocationEvent.ThirdCapacityAfterPopulation = ProviderKeys.Max();
	PrimaryAllocationEvent.AllocatedBytesAfterPopulation =
		static_cast<uint64>(FileKeys.GetAllocatedSize())
			+ static_cast<uint64>(MountKeys.GetAllocatedSize())
			+ static_cast<uint64>(ProviderKeys.GetAllocatedSize());
	ObserveEligibilityAllocationForTests(
		AllocationCapture, PrimaryAllocationEvent);
#endif
	if (FileKeys.IsEmpty())
	{
		return Failure(EAngelscriptCacheValidationError::MissingGraphTarget,
			EAngelscriptCacheRecordKind::SourceIndex);
	}

	const auto SortUniqueHashes = [](TArray<FAngelscriptHash256>& Values)
	{
		Values.Sort([](const FAngelscriptHash256& A, const FAngelscriptHash256& B)
		{
			return CompareHash(A, B) < 0;
		});
		int32 WriteIndex = 0;
		for (int32 ReadIndex = 0; ReadIndex < Values.Num(); ++ReadIndex)
		{
			if (WriteIndex == 0 || !(Values[ReadIndex] == Values[WriteIndex - 1]))
			{
				if (WriteIndex != ReadIndex) { Values[WriteIndex] = Values[ReadIndex]; }
				++WriteIndex;
			}
		}
		Values.SetNum(WriteIndex, EAllowShrinking::No);
	};
	SortUniqueHashes(FileKeys);
	SortUniqueHashes(MountKeys);
	SortUniqueHashes(ProviderKeys);
	const auto ContainsSortedHash = [](const TArray<FAngelscriptHash256>& Values,
		const FAngelscriptHash256& Hash)
	{
		int32 Lower = 0;
		int32 Upper = Values.Num();
		while (Lower < Upper)
		{
			const int32 Middle = Lower + (Upper - Lower) / 2;
			if (CompareHash(Values[Middle], Hash) < 0) { Lower = Middle + 1; }
			else { Upper = Middle; }
		}
		return Lower < Values.Num() && Values[Lower] == Hash;
	};
	const auto ScopeMatchesBaseClosure = [&](const EAngelscriptCachedFastPathScopeKind ScopeKind,
		const FAngelscriptHash256& ScopeKey)
	{
		switch (ScopeKind)
		{
		case EAngelscriptCachedFastPathScopeKind::Module: return ScopeKey == ModuleKey.Hash;
		case EAngelscriptCachedFastPathScopeKind::SourceFile: return ContainsSortedHash(FileKeys, ScopeKey);
		case EAngelscriptCachedFastPathScopeKind::Mount: return ContainsSortedHash(MountKeys, ScopeKey);
		case EAngelscriptCachedFastPathScopeKind::Provider: return ContainsSortedHash(ProviderKeys, ScopeKey);
		default: return false;
		}
	};

	TArray<FIndexedStableHash> LocalHookKeyIndex;
	TArray<FIndexedStableHash> LocalReverseHookDependencies;
	if (PreparedIndexes == nullptr)
	{
		LocalHookKeyIndex.Reserve(Canonical.PreprocessHooks.Num());
		LocalReverseHookDependencies.Reserve(Canonical.PreprocessHooks.Num());
	}
	const TArray<FIndexedStableHash>& HookKeyIndex = PreparedIndexes != nullptr
		? PreparedIndexes->HookKeyIndex
		: LocalHookKeyIndex;
	const TArray<FIndexedStableHash>& ReverseHookDependencies =
		PreparedIndexes != nullptr
			? PreparedIndexes->ReverseHookDependencies
			: LocalReverseHookDependencies;
	TArray<uint8> ReachedHooks;
	ReachedHooks.Reserve(Canonical.PreprocessHooks.Num());
	TArray<int32> HookQueue;
	HookQueue.Reserve(Canonical.PreprocessHooks.Num());
#if WITH_ANGELSCRIPT_UNITTESTS
	AngelscriptCacheEligibilityTestHooks::FAllocationEvent AuxiliaryAllocationEvent;
	AuxiliaryAllocationEvent.Phase =
		AngelscriptCacheEligibilityTestHooks::EAllocationPhase::AuxiliaryScratchArrays;
	AuxiliaryAllocationEvent.RequestedCapacity = Canonical.PreprocessHooks.Num();
	AuxiliaryAllocationEvent.FirstCapacityBeforePopulation = HookKeyIndex.Max();
	AuxiliaryAllocationEvent.SecondCapacityBeforePopulation =
		ReverseHookDependencies.Max();
	AuxiliaryAllocationEvent.ThirdCapacityBeforePopulation = ReachedHooks.Max();
	AuxiliaryAllocationEvent.FourthCapacityBeforePopulation = HookQueue.Max();
	AuxiliaryAllocationEvent.AllocatedBytesBeforePopulation =
		static_cast<uint64>(HookKeyIndex.GetAllocatedSize())
		+ static_cast<uint64>(ReverseHookDependencies.GetAllocatedSize())
		+ static_cast<uint64>(ReachedHooks.GetAllocatedSize())
		+ static_cast<uint64>(HookQueue.GetAllocatedSize());
#endif
	if (PreparedIndexes == nullptr)
	{
		BuildStableHashIndex(Canonical.PreprocessHooks,
			[](const auto& Hook) { return Hook.HookKey.Hash; },
			LocalHookKeyIndex);
	}
	ReachedHooks.SetNumZeroed(Canonical.PreprocessHooks.Num());
	for (int32 HookIndex = 0; HookIndex < Canonical.PreprocessHooks.Num(); ++HookIndex)
	{
		const FAngelscriptCachedPreprocessHook& Hook = Canonical.PreprocessHooks[HookIndex];
		if (Hook.AffectedScopeKind == EAngelscriptCachedFastPathScopeKind::Hook)
		{
			if (PreparedIndexes == nullptr)
			{
				LocalReverseHookDependencies.Add(
					{Hook.AffectedScopeStableKey, HookIndex});
			}
		}
		else if (ScopeMatchesBaseClosure(Hook.AffectedScopeKind, Hook.AffectedScopeStableKey))
		{
			ReachedHooks[HookIndex] = 1;
			HookQueue.Add(HookIndex);
		}
	}
	if (PreparedIndexes == nullptr)
	{
		LocalReverseHookDependencies.Sort([](
			const FIndexedStableHash& A, const FIndexedStableHash& B)
		{
			const int32 KeyCompare = CompareHash(A.Hash, B.Hash);
			return KeyCompare != 0 ? KeyCompare < 0 : A.ValueIndex < B.ValueIndex;
		});
	}
	for (int32 QueueIndex = 0; QueueIndex < HookQueue.Num(); ++QueueIndex)
	{
		const FAngelscriptHash256& ReachedKey =
			Canonical.PreprocessHooks[HookQueue[QueueIndex]].HookKey.Hash;
		int32 Lower = 0;
		int32 Upper = ReverseHookDependencies.Num();
		while (Lower < Upper)
		{
			const int32 Middle = Lower + (Upper - Lower) / 2;
			if (CompareHash(ReverseHookDependencies[Middle].Hash, ReachedKey) < 0)
			{
				Lower = Middle + 1;
			}
			else
			{
				Upper = Middle;
			}
		}
		for (int32 DependencyIndex = Lower;
			DependencyIndex < ReverseHookDependencies.Num()
				&& ReverseHookDependencies[DependencyIndex].Hash == ReachedKey;
			++DependencyIndex)
		{
			const int32 DependentHookIndex = ReverseHookDependencies[DependencyIndex].ValueIndex;
			if (ReachedHooks[DependentHookIndex] == 0)
			{
				ReachedHooks[DependentHookIndex] = 1;
				HookQueue.Add(DependentHookIndex);
			}
		}
	}
#if WITH_ANGELSCRIPT_UNITTESTS
	AuxiliaryAllocationEvent.FirstCapacityAfterPopulation = HookKeyIndex.Max();
	AuxiliaryAllocationEvent.SecondCapacityAfterPopulation =
		ReverseHookDependencies.Max();
	AuxiliaryAllocationEvent.ThirdCapacityAfterPopulation = ReachedHooks.Max();
	AuxiliaryAllocationEvent.FourthCapacityAfterPopulation = HookQueue.Max();
	AuxiliaryAllocationEvent.AllocatedBytesAfterPopulation =
		static_cast<uint64>(HookKeyIndex.GetAllocatedSize())
			+ static_cast<uint64>(ReverseHookDependencies.GetAllocatedSize())
			+ static_cast<uint64>(ReachedHooks.GetAllocatedSize())
			+ static_cast<uint64>(HookQueue.GetAllocatedSize());
	ObserveEligibilityAllocationForTests(
		AllocationCapture, AuxiliaryAllocationEvent);
#endif
	const auto ScopeMatchesClosure = [&](const EAngelscriptCachedFastPathScopeKind ScopeKind,
		const FAngelscriptHash256& ScopeKey)
	{
		if (ScopeKind != EAngelscriptCachedFastPathScopeKind::Hook)
		{
			return ScopeMatchesBaseClosure(ScopeKind, ScopeKey);
		}
		const int32 HookIndex = FindStableHashValueIndex(HookKeyIndex, ScopeKey);
		return HookIndex != INDEX_NONE && ReachedHooks[HookIndex] != 0;
	};

	int32 MatchingCount = 0;
	for (const FAngelscriptCachedFastPathIneligibleScope& Scope : Canonical.IneligibleScopes)
	{
		if (ScopeMatchesClosure(Scope.ScopeKind, Scope.ScopeStableKey))
		{
			++MatchingCount;
		}
	}

	uint64 MatchingResidentBytes = 0;
	int32 MatchingScopeCapacity = 0;
	uint64 MatchingScopeBytes = 0;
	if (!AngelscriptCacheCanonicalCodec_Private::TryCalculateArrayReserveBytes<
			FAngelscriptCachedFastPathIneligibleScope>(
				MatchingCount, MatchingScopeCapacity, MatchingScopeBytes)
		|| !TryAddScratchBytes(MatchingScopeBytes, MatchingResidentBytes))
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	for (const FAngelscriptCachedFastPathIneligibleScope& Scope : Canonical.IneligibleScopes)
	{
		if (!ScopeMatchesClosure(Scope.ScopeKind, Scope.ScopeStableKey))
		{
			continue;
		}
		const int32 DiagnosticLength = Scope.CanonicalDiagnosticIdentity.Len();
		if (DiagnosticLength >= MAX_int32)
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
		int32 DiagnosticCapacity = 0;
		uint64 DiagnosticBytes = 0;
		if (!AngelscriptCacheCanonicalCodec_Private::TryCalculateArrayReserveBytes<TCHAR>(
				DiagnosticLength + 1, DiagnosticCapacity, DiagnosticBytes)
			|| !TryAddScratchBytes(DiagnosticBytes, MatchingResidentBytes))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
	}

	FAngelscriptCacheTemporaryResidentReservation MatchingReservation;
	if (MatchingResidentBytes != 0
		&& !Budget.TryReserveTemporaryDecoded(
			MatchingResidentBytes, Limits, MatchingReservation))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	OutResult.MatchingScopes.Reserve(MatchingCount);
	if (static_cast<uint64>(OutResult.MatchingScopes.GetAllocatedSize())
		!= MatchingScopeBytes)
	{
		OutResult.Reset();
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	for (const FAngelscriptCachedFastPathIneligibleScope& Scope : Canonical.IneligibleScopes)
	{
		if (ScopeMatchesClosure(Scope.ScopeKind, Scope.ScopeStableKey))
		{
			FAngelscriptCachedFastPathIneligibleScope& PublishedScope =
				OutResult.MatchingScopes.AddDefaulted_GetRef();
			PublishedScope.ScopeKind = Scope.ScopeKind;
			PublishedScope.ScopeStableKey = Scope.ScopeStableKey;
			PublishedScope.Reason = Scope.Reason;
			PublishedScope.CanonicalDiagnosticIdentity.Reserve(
				Scope.CanonicalDiagnosticIdentity.Len());
			PublishedScope.CanonicalDiagnosticIdentity.AppendChars(
				*Scope.CanonicalDiagnosticIdentity,
				Scope.CanonicalDiagnosticIdentity.Len());
			PublishedScope.ObservedFingerprint = Scope.ObservedFingerprint;
		}
	}
	uint64 ActualMatchingResidentBytes =
		static_cast<uint64>(OutResult.MatchingScopes.GetAllocatedSize());
	for (const FAngelscriptCachedFastPathIneligibleScope& Scope :
		OutResult.MatchingScopes)
	{
		if (!TryAddScratchBytes(
				static_cast<uint64>(
					Scope.CanonicalDiagnosticIdentity.GetCharArray().GetAllocatedSize()),
				ActualMatchingResidentBytes))
		{
			OutResult.Reset();
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
	}
	if (ActualMatchingResidentBytes != MatchingResidentBytes)
	{
		OutResult.Reset();
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	if (MatchingResidentBytes != 0)
	{
		if (OutMatchingReservation != nullptr)
		{
			if (OutMatchingReservation->IsActive())
			{
				OutResult.Reset();
				return Failure(EAngelscriptCacheValidationError::Overflow,
					EAngelscriptCacheRecordKind::SourceIndex);
			}
			*OutMatchingReservation = MoveTemp(MatchingReservation);
		}
		else if (!MatchingReservation.PromoteToRetained())
		{
			OutResult.Reset();
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
	}
	OutResult.bExactFastPathEligible = OutResult.MatchingScopes.IsEmpty();
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::
	QueryExactFastPathEligibility(
		const FAngelscriptDecodedCacheRecord& SourceIndexRecord,
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheExactFastPathEligibility& OutResult)
{
	OutResult.Reset();
	if (SourceIndexRecord.GetRecordId().Kind
		!= EAngelscriptCacheRecordKind::SourceIndex)
	{
		return FAngelscriptCacheValidationResult::AtStage(
			EAngelscriptCacheValidationError::WrongRecordKind,
			SourceIndexRecord.GetRecordId().Kind,
			EAngelscriptCacheValidationStage::ModuleGraph,
			0);
	}
	const FAngelscriptCachedSourceIndex* SourceIndex =
		SourceIndexRecord.TryGetSourceIndex();
	check(SourceIndex != nullptr);
	return QueryExactFastPathEligibilityInternal(
		*SourceIndex,
		ModuleKey,
		Limits,
		Budget,
		nullptr,
#if WITH_ANGELSCRIPT_UNITTESTS
		nullptr,
#endif
		nullptr,
		OutResult);
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::
	QueryCurrentExactFastPathEligibility(
		const FAngelscriptCachedSourceIndex& CurrentSourceIndex,
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheExactFastPathEligibility& OutResult)
{
	OutResult.Reset();
	FAngelscriptHash256 ComputedSnapshot;
	const FAngelscriptCacheValidationResult SnapshotResult =
		ComputeSourceSnapshot(CurrentSourceIndex, ComputedSnapshot);
	if (!SnapshotResult.IsSuccess())
	{
		return SnapshotResult;
	}
	if (CurrentSourceIndex.SourceSnapshot.IsZero()
		|| ComputedSnapshot != CurrentSourceIndex.SourceSnapshot)
	{
		return Failure(EAngelscriptCacheValidationError::SourceSnapshotMismatch,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	return QueryExactFastPathEligibilityInternal(
		CurrentSourceIndex,
		ModuleKey,
		Limits,
		Budget,
		nullptr,
#if WITH_ANGELSCRIPT_UNITTESTS
		nullptr,
#endif
		nullptr,
		OutResult);
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::
	QueryCurrentExactFastPathEligibilityBatch(
		const FAngelscriptCachedSourceIndex& CurrentSourceIndex,
		const TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheExactFastPathEligibilityBatch& OutResult)
{
	OutResult.Reset();
	if (ModuleKeys.IsEmpty())
	{
		return Failure(EAngelscriptCacheValidationError::MissingGraphTarget,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	for (const FAngelscriptStableModuleKey& ModuleKey : ModuleKeys)
	{
		if (ModuleKey.Hash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
	}

	FAngelscriptHash256 ComputedSnapshot;
	const FAngelscriptCacheValidationResult SnapshotResult =
		ComputeSourceSnapshot(CurrentSourceIndex, ComputedSnapshot);
	if (!SnapshotResult.IsSuccess())
	{
		return SnapshotResult;
	}
	if (CurrentSourceIndex.SourceSnapshot.IsZero()
		|| ComputedSnapshot != CurrentSourceIndex.SourceSnapshot)
	{
		return Failure(EAngelscriptCacheValidationError::SourceSnapshotMismatch,
			EAngelscriptCacheRecordKind::SourceIndex);
	}

	uint64 PreparationScratchBytes = 0;
	if (!TryComputeEligibilityBatchPreparationScratchBytes(
		CurrentSourceIndex, ModuleKeys.Num(), PreparationScratchBytes))
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	FAngelscriptCacheTemporaryResidentReservation PreparationScratchReservation;
	if (!Budget.TryReserveTemporaryDecoded(
		PreparationScratchBytes, Limits, PreparationScratchReservation))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::SourceIndex);
	}

	TArray<FAngelscriptStableModuleKey> SortedModuleKeys;
	SortedModuleKeys.Reserve(ModuleKeys.Num());
	SortedModuleKeys.Append(ModuleKeys.GetData(), ModuleKeys.Num());
	SortedModuleKeys.Sort([](
		const FAngelscriptStableModuleKey& A,
		const FAngelscriptStableModuleKey& B)
	{
		return A.Hash < B.Hash;
	});
	for (int32 Index = 1; Index < SortedModuleKeys.Num(); ++Index)
	{
		if (SortedModuleKeys[Index - 1].Hash == SortedModuleKeys[Index].Hash)
		{
			return Failure(EAngelscriptCacheValidationError::DuplicateKey,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
	}

	FPreparedEligibilityIndexes PreparedIndexes;
	PreparedIndexes.HookKeyIndex.Reserve(
		CurrentSourceIndex.PreprocessHooks.Num());
	PreparedIndexes.ReverseHookDependencies.Reserve(
		CurrentSourceIndex.PreprocessHooks.Num());
	BuildStableHashIndex(CurrentSourceIndex.PreprocessHooks,
		[](const auto& Hook) { return Hook.HookKey.Hash; },
		PreparedIndexes.HookKeyIndex);
	for (int32 HookIndex = 0;
		HookIndex < CurrentSourceIndex.PreprocessHooks.Num(); ++HookIndex)
	{
		const FAngelscriptCachedPreprocessHook& Hook =
			CurrentSourceIndex.PreprocessHooks[HookIndex];
		if (Hook.AffectedScopeKind == EAngelscriptCachedFastPathScopeKind::Hook)
		{
			PreparedIndexes.ReverseHookDependencies.Add(
				{Hook.AffectedScopeStableKey, HookIndex});
		}
	}
	PreparedIndexes.ReverseHookDependencies.Sort([](
		const FIndexedStableHash& A, const FIndexedStableHash& B)
	{
		const int32 KeyCompare = CompareHash(A.Hash, B.Hash);
		return KeyCompare != 0 ? KeyCompare < 0 : A.ValueIndex < B.ValueIndex;
	});

	TArray<FAngelscriptCacheTemporaryResidentReservation> MatchingReservations;
	MatchingReservations.Reserve(SortedModuleKeys.Num());
	const uint64 ActualPreparationScratchBytes =
		static_cast<uint64>(SortedModuleKeys.GetAllocatedSize())
		+ static_cast<uint64>(PreparedIndexes.HookKeyIndex.GetAllocatedSize())
		+ static_cast<uint64>(
			PreparedIndexes.ReverseHookDependencies.GetAllocatedSize())
		+ static_cast<uint64>(MatchingReservations.GetAllocatedSize());
	if (ActualPreparationScratchBytes != PreparationScratchBytes)
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}

	int32 EntryCapacity = 0;
	uint64 EntryBytes = 0;
	if (!AngelscriptCacheCanonicalCodec_Private::TryCalculateArrayReserveBytes<
			FAngelscriptCacheExactFastPathEligibilityBatchEntry>(
				SortedModuleKeys.Num(), EntryCapacity, EntryBytes))
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	FAngelscriptCacheTemporaryResidentReservation EntryReservation;
	if (!Budget.TryReserveTemporaryDecoded(EntryBytes, Limits, EntryReservation))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::SourceIndex);
	}

	FAngelscriptCacheExactFastPathEligibilityBatch Candidate;
	Candidate.Entries.Reserve(SortedModuleKeys.Num());
	if (static_cast<uint64>(Candidate.Entries.GetAllocatedSize()) != EntryBytes)
	{
		Candidate.Reset();
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	for (const FAngelscriptStableModuleKey& ModuleKey : SortedModuleKeys)
	{
		FAngelscriptCacheExactFastPathEligibilityBatchEntry& Entry =
			Candidate.Entries.AddDefaulted_GetRef();
		Entry.ModuleKey = ModuleKey;
		FAngelscriptCacheTemporaryResidentReservation& MatchingReservation =
			MatchingReservations.AddDefaulted_GetRef();
		const FAngelscriptCacheValidationResult EligibilityResult =
			QueryExactFastPathEligibilityInternal(
				CurrentSourceIndex,
				ModuleKey,
				Limits,
				Budget,
				&PreparedIndexes,
#if WITH_ANGELSCRIPT_UNITTESTS
				nullptr,
#endif
				&MatchingReservation,
				Entry.Eligibility);
		if (!EligibilityResult.IsSuccess())
		{
			Candidate.Reset();
			MatchingReservations.Empty();
			return EligibilityResult;
		}
	}

	for (int32 Index = 0; Index < Candidate.Entries.Num(); ++Index)
	{
		const bool bHasPublishedMatchingAllocation =
			!Candidate.Entries[Index].Eligibility.MatchingScopes.IsEmpty();
		if (bHasPublishedMatchingAllocation
			!= MatchingReservations[Index].IsActive())
		{
			Candidate.Reset();
			MatchingReservations.Empty();
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
	}
	for (FAngelscriptCacheTemporaryResidentReservation& Reservation
		: MatchingReservations)
	{
		if (Reservation.IsActive() && !Reservation.PromoteToRetained())
		{
			Candidate.Reset();
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::SourceIndex);
		}
	}
	if (EntryBytes != 0 && !EntryReservation.PromoteToRetained())
	{
		Candidate.Reset();
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	Candidate.SourceValidationPasses = 1;
	Candidate.PreparedIndexBuilds = 1;
	Candidate.ModuleQueries = static_cast<uint32>(Candidate.Entries.Num());
	OutResult = MoveTemp(Candidate);
	return {};
}

#if WITH_ANGELSCRIPT_UNITTESTS
FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::
	QueryExactFastPathEligibilityWithAllocationCaptureForTests(
		const FAngelscriptDecodedCacheRecord& SourceIndexRecord,
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		AngelscriptCacheEligibilityTestHooks::FAllocationEventCaptureView Capture,
		FAngelscriptCacheExactFastPathEligibility& OutResult)
{
	PrepareEligibilityAllocationCaptureForTests(&Capture);
	OutResult.Reset();
	if (SourceIndexRecord.GetRecordId().Kind
		!= EAngelscriptCacheRecordKind::SourceIndex)
	{
		return FAngelscriptCacheValidationResult::AtStage(
			EAngelscriptCacheValidationError::WrongRecordKind,
			SourceIndexRecord.GetRecordId().Kind,
			EAngelscriptCacheValidationStage::ModuleGraph,
			0);
	}
	const FAngelscriptCachedSourceIndex* SourceIndex =
		SourceIndexRecord.TryGetSourceIndex();
	check(SourceIndex != nullptr);
	return QueryExactFastPathEligibilityInternal(
		*SourceIndex, ModuleKey, Limits, Budget, nullptr, &Capture, nullptr,
		OutResult);
}
#endif

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::ComputeModuleInterfaceAbi(
	const FAngelscriptCachedModuleInterface& Value,
	FAngelscriptHash256& OutHash)
{
	OutHash = {};
	FAngelscriptCachedModuleInterface Canonical = Value;
	const FAngelscriptCacheValidationResult Result = PrepareModuleInterface(Canonical, true, false, false);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutHash = HashModuleInterfaceAbi(Canonical);
	return {};
}

static FAngelscriptCacheValidationResult SerializeSourceIndexInternal(
	const FAngelscriptCachedSourceIndex& Value,
	TArray<uint8>& OutPayload,
	const bool bCanonicalize)
{
	OutPayload.Reset();
	FAngelscriptCachedSourceIndex Prepared = Value;
	const FAngelscriptCacheValidationResult Result = PrepareSourceIndex(
		Prepared, bCanonicalize, false, bCanonicalize);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	WriteSourceIndexPayload(Writer, Prepared);
	OutPayload = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
	const FAngelscriptCachedSourceIndex& Value,
	TArray<uint8>& OutPayload)
{
	return SerializeSourceIndexInternal(Value, OutPayload, true);
}

FAngelscriptCacheValidationResult
AngelscriptCacheSemanticRecords_Private::FDecodedRecordCodecBridge::TryDecodeSourceIndex(
	const TConstArrayView<uint8> Payload,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
	FAngelscriptCachedSourceIndex& OutValue,
	FAngelscriptDecodedCacheRecord::FSourceIndexCapturedOffsetStorage& OutOffsets)
{
	OutValue = {};
	OutOffsets = {};
	const FAngelscriptCacheValidationResult BeginResult =
		AngelscriptCacheCanonicalCodec_Private::BeginRead(
			Payload, Limits, Budget, EAngelscriptCacheRecordKind::SourceIndex);
	if (!BeginResult.IsSuccess())
	{
		return BeginResult;
	}
	AngelscriptCacheCanonicalCodec_Private::FReader Reader(
		Payload,
		Limits,
		Budget,
		EAngelscriptCacheRecordKind::SourceIndex,
		ChargeSink);
	if (!ReadCapturedSourceIndexPayload(Reader, OutValue, OutOffsets))
	{
		OutValue = {};
		OutOffsets = {};
		return Reader.GetResult();
	}
	if (!Reader.IsAtEnd())
	{
		OutValue = {};
		OutOffsets = {};
		return Failure(
			EAngelscriptCacheValidationError::TrailingData,
			EAngelscriptCacheRecordKind::SourceIndex,
			Reader.GetOffset());
	}
	uint64 ScratchBytes = 0;
	if (!TryComputeSourceValidationScratchBytes(
		static_cast<uint64>(Payload.Num()), OutValue, ScratchBytes))
	{
		OutValue = {};
		OutOffsets = {};
		return Failure(
			EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	FAngelscriptCacheTemporaryResidentReservation ScratchReservation;
	if (!Budget.TryReserveTemporaryDecoded(ScratchBytes, Limits, ScratchReservation))
	{
		OutValue = {};
		OutOffsets = {};
		return Failure(
			EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	const FSourceIndexSemanticOffsetView ExactOffsets{
		&OutOffsets,
		[](const void* Context,
			const FAngelscriptSourceIndexFieldCoordinate& Coordinate)
		{
			using FStorage =
				FAngelscriptDecodedCacheRecord::FSourceIndexCapturedOffsetStorage;
			return FAngelscriptDecodedCacheRecord::FindSourceIndexOffsetInStorage(
				*static_cast<const FStorage*>(Context), Coordinate);
		}};
	const FAngelscriptCacheValidationResult Result =
		PrepareSourceIndex(
			OutValue, false, true, true, &ExactOffsets);
	if (!Result.IsSuccess())
	{
		OutValue = {};
		OutOffsets = {};
		return Result;
	}
	return {};
}

static FAngelscriptCacheValidationResult SerializeModuleInterfaceInternal(
	const FAngelscriptCachedModuleInterface& Value,
	TArray<uint8>& OutPayload,
	const bool bCanonicalize)
{
	OutPayload.Reset();
	FAngelscriptCachedModuleInterface Prepared = Value;
	const FAngelscriptCacheValidationResult Result = PrepareModuleInterface(
		Prepared, bCanonicalize, false, bCanonicalize, bCanonicalize);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FWriter Writer;
	WriteModuleInterfacePayload(Writer, Prepared);
	OutPayload = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeModuleInterface(
	const FAngelscriptCachedModuleInterface& Value,
	TArray<uint8>& OutPayload)
{
	return SerializeModuleInterfaceInternal(Value, OutPayload, true);
}

FAngelscriptCacheValidationResult
AngelscriptCacheSemanticRecords_Private::FDecodedRecordCodecBridge::TryDecodeModuleInterface(
	const TConstArrayView<uint8> Payload,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
	FAngelscriptCachedModuleInterface& OutValue,
	FAngelscriptDecodedCacheRecord::FModuleInterfaceCapturedOffsetStorage& OutOffsets)
{
	OutValue = {};
	OutOffsets = {};
	const FAngelscriptCacheValidationResult BeginResult =
		AngelscriptCacheCanonicalCodec_Private::BeginRead(
			Payload, Limits, Budget, EAngelscriptCacheRecordKind::ModuleInterface);
	if (!BeginResult.IsSuccess())
	{
		return BeginResult;
	}
	AngelscriptCacheCanonicalCodec_Private::FReader Reader(
		Payload,
		Limits,
		Budget,
		EAngelscriptCacheRecordKind::ModuleInterface,
		ChargeSink);
	if (!ReadCapturedModuleInterfacePayload(Reader, OutValue, OutOffsets))
	{
		OutValue = {};
		OutOffsets = {};
		return Reader.GetResult();
	}
	if (!Reader.IsAtEnd())
	{
		OutValue = {};
		OutOffsets = {};
		return Failure(
			EAngelscriptCacheValidationError::TrailingData,
			EAngelscriptCacheRecordKind::ModuleInterface,
			Reader.GetOffset());
	}
	uint64 ScratchBytes = 0;
	if (!TryComputeModuleValidationScratchBytes(
		static_cast<uint64>(Payload.Num()), OutValue, ScratchBytes))
	{
		OutValue = {};
		OutOffsets = {};
		return Failure(
			EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::ModuleInterface);
	}
	FAngelscriptCacheTemporaryResidentReservation ScratchReservation;
	if (!Budget.TryReserveTemporaryDecoded(ScratchBytes, Limits, ScratchReservation))
	{
		OutValue = {};
		OutOffsets = {};
		return Failure(
			EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::ModuleInterface);
	}
	const FModuleInterfaceSemanticOffsetView ExactOffsets{
		&OutOffsets,
		[](const void* Context,
			const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate)
		{
			using FStorage =
				FAngelscriptDecodedCacheRecord::FModuleInterfaceCapturedOffsetStorage;
			return FAngelscriptDecodedCacheRecord::FindModuleInterfaceOffsetInStorage(
				*static_cast<const FStorage*>(Context), Coordinate);
		}};
	const FAngelscriptCacheValidationResult Result = PrepareModuleInterface(
		OutValue, false, true, true, true, &ExactOffsets);
	if (!Result.IsSuccess())
	{
		OutValue = {};
		OutOffsets = {};
		return Result;
	}
	return {};
}

#if WITH_ANGELSCRIPT_UNITTESTS
FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeSourceIndexPreservingOrderForTests(
	const FAngelscriptCachedSourceIndex& Value,
	TArray<uint8>& OutPayload)
{
	return SerializeSourceIndexInternal(Value, OutPayload, false);
}

FAngelscriptCacheValidationResult FAngelscriptCacheSemanticArchive::SerializeModuleInterfacePreservingOrderForTests(
	const FAngelscriptCachedModuleInterface& Value,
	TArray<uint8>& OutPayload)
{
	return SerializeModuleInterfaceInternal(Value, OutPayload, false);
}

void FAngelscriptCacheSemanticArchive::SerializeSourceIndexPhysicalForTests(
	const FAngelscriptCachedSourceIndex& Value,
	TArray<uint8>& OutPayload)
{
	FWriter Writer;
	WriteSourceIndexPayload(Writer, Value);
	OutPayload = MoveTemp(Writer.Bytes);
}

void FAngelscriptCacheSemanticArchive::SerializeModuleInterfacePhysicalForTests(
	const FAngelscriptCachedModuleInterface& Value,
	TArray<uint8>& OutPayload)
{
	FWriter Writer;
	WriteModuleInterfacePayload(Writer, Value);
	OutPayload = MoveTemp(Writer.Bytes);
}
#endif
