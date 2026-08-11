#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"

enum class EAngelscriptCacheCodec : uint8
{
	None = 0,
	Zlib = 1,
};

enum class EAngelscriptCacheRecordKind : uint8
{
	SourceIndex = 1,
	ModuleInterface = 2,
	TypeSchema = 3,
	ModuleState = 4,
	FunctionBody = 5,
	DebugSidecar = 6,
	ModuleSnapshot = 7,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheRecordId
{
	EAngelscriptCacheRecordKind Kind = static_cast<EAngelscriptCacheRecordKind>(0);
	FAngelscriptHash256 ContentHash;

	friend bool operator==(const FAngelscriptCacheRecordId& A, const FAngelscriptCacheRecordId& B)
	{
		return A.Kind == B.Kind && A.ContentHash == B.ContentHash;
	}

	friend bool operator<(const FAngelscriptCacheRecordId& A, const FAngelscriptCacheRecordId& B)
	{
		const uint8 AKind = static_cast<uint8>(A.Kind);
		const uint8 BKind = static_cast<uint8>(B.Kind);
		return AKind < BKind || (AKind == BKind && A.ContentHash < B.ContentHash);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheReadLimits
{
	static constexpr uint64 DefaultMaxCanonicalRecordPayloadBytes = UINT64_C(64) * 1024 * 1024;
	static constexpr uint64 DefaultMaxStoredRecordBytes = UINT64_C(64) * 1024 * 1024;
	static constexpr uint64 DefaultMaxManifestBytes = UINT64_C(64) * 1024 * 1024;
	static constexpr uint64 DefaultMaxPackBytes = UINT64_C(128) * 1024 * 1024;
	static constexpr uint64 DefaultMaxPackIndexEntries = UINT64_C(262144);
	static constexpr uint64 DefaultMaxGenerationRecords = UINT64_C(262144);
	static constexpr uint64 DefaultMaxModuleSnapshots = UINT64_C(262144);
	static constexpr uint64 DefaultMaxGenerationPacks = UINT64_C(4096);
	static constexpr uint64 DefaultMaxStringBytes = UINT64_C(1) * 1024 * 1024;
	static constexpr uint64 DefaultMaxArrayElements = UINT64_C(1) * 1024 * 1024;
	static constexpr uint64 DefaultMaxNestingDepth = 64;
	static constexpr uint64 DefaultMaxReferencesAndRelocations = UINT64_C(1) * 1024 * 1024;
	static constexpr uint64 DefaultMaxSessionBytes = UINT64_C(512) * 1024 * 1024;

	uint64 MaxCanonicalRecordPayloadBytes = DefaultMaxCanonicalRecordPayloadBytes;
	uint64 MaxStoredRecordBytes = DefaultMaxStoredRecordBytes;
	uint64 MaxManifestBytes = DefaultMaxManifestBytes;
	uint64 MaxPackBytes = DefaultMaxPackBytes;
	uint64 MaxPackIndexEntries = DefaultMaxPackIndexEntries;
	uint64 MaxGenerationRecords = DefaultMaxGenerationRecords;
	uint64 MaxModuleSnapshots = DefaultMaxModuleSnapshots;
	uint64 MaxGenerationPacks = DefaultMaxGenerationPacks;
	uint64 MaxStringBytes = DefaultMaxStringBytes;
	uint64 MaxArrayElements = DefaultMaxArrayElements;
	uint64 MaxNestingDepth = DefaultMaxNestingDepth;
	uint64 MaxReferencesAndRelocations = DefaultMaxReferencesAndRelocations;
	uint64 MaxTotalStoredBytes = DefaultMaxSessionBytes;
	uint64 MaxTotalDecompressedBytes = DefaultMaxSessionBytes;
	uint64 MaxTotalDecodedBytes = DefaultMaxSessionBytes;
	uint64 MaxResidentDecodedBytes = DefaultMaxSessionBytes;
};

enum class EAngelscriptCacheValidationClass : uint8
{
	Success = 0,
	Malformed = 1,
	ArithmeticOrBudget = 2,
	CodecOrIntegrity = 3,
	CanonicalSemantic = 4,
	GraphOrOwnership = 5,
	Ineligible = 6,
};

enum class EAngelscriptCacheValidationStage : uint8
{
	None = 0,
	EnvelopeDecode = 1,
	PayloadDecode = 2,
	LocalSemantic = 3,
	OpaqueCodec = 4,
	ModuleGraph = 5,
	CurrentResolver = 6,
	PackDecode = 7,
	ManifestDecode = 8,
	ManifestGraph = 9,
};

enum class EAngelscriptCacheValidationError : uint8
{
	None = 0,
	BadMagic = 1,
	UnsupportedSchema = 2,
	UnknownRecordKind = 3,
	NonZeroReserved = 4,
	Overflow = 5,
	BudgetExceeded = 6,
	OutOfBounds = 7,
	ChecksumMismatch = 8,
	TrailingData = 9,
	InvalidArrayView = 10,
	AliasedInputOutput = 11,
	UnsupportedPayloadSchema = 12,
	UnknownEnumValue = 13,
	UnknownFlags = 14,
	InvalidBoolean = 15,
	InvalidOptionalTag = 16,
	InvalidUtf8 = 17,
	EmbeddedNul = 18,
	InvalidLogicalPath = 19,
	ImpossibleCount = 20,
	NestingDepthExceeded = 21,
	RecordIdMismatch = 22,
	NonCanonicalOrder = 23,
	DuplicateKey = 24,
	ConflictingKey = 25,
	CaseCollision = 26,
	ZeroStableKey = 27,
	MissingExpectedAbi = 28,
	ForbiddenExpectedAbi = 29,
	InvalidPresence = 30,
	InvalidQualifierCombination = 31,
	OrdinalGap = 32,
	DuplicateOrdinal = 33,
	DerivedHashMismatch = 34,
	MissingOwner = 35,
	CrossModuleOwner = 36,
	MissingGraphTarget = 37,
	WrongReferenceKind = 38,
	CompatibilityMismatch = 39,
	ContextMismatch = 40,
	ProfileMismatch = 41,
	SourceSnapshotMismatch = 42,
	CurrentAbiMismatch = 43,
	UnsupportedCodecVersion = 44,
	OpaquePayloadMalformed = 45,
	OpaquePayloadHashMismatch = 46,
	RelocationDependencyMismatch = 47,
	WrongRecordKind = 48,
	MissingRecord = 49,
	MissingCoverage = 50,
	UnexpectedRecord = 51,
	UndeclaredEntity = 52,
	DuplicateDebugOwner = 53,
	DebugLinkMismatch = 54,
	EnumAuthorityMismatch = 55,
	InitializerOwnershipMismatch = 56,
	GlobalCoverageMismatch = 57,
	ProfileGraphMismatch = 58,
	SourceGraphMismatch = 59,
	GraphAbiMismatch = 60,
	InvocationKindMismatch = 61,
	DebugSourceMismatch = 62,
	CurrentContentMismatch = 63,
	CurrentSymbolMissing = 64,
	UnsupportedStorageCodec = 65,
	DecompressionFailed = 66,
	DecompressedSizeMismatch = 67,
	PackIdMismatch = 68,
	GenerationIdMismatch = 69,
	OverlappingRange = 70,
	PackIndexMismatch = 71,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult
{
	EAngelscriptCacheValidationError Error = EAngelscriptCacheValidationError::None;
	EAngelscriptCacheValidationClass Class = EAngelscriptCacheValidationClass::Success;
	EAngelscriptCacheRecordKind RecordKind = static_cast<EAngelscriptCacheRecordKind>(0);
	EAngelscriptCacheValidationStage Stage = EAngelscriptCacheValidationStage::None;
	uint64 ByteOffset = 0;

	FAngelscriptCacheValidationResult() = default;

	explicit FAngelscriptCacheValidationResult(
		const EAngelscriptCacheValidationError InError,
		const EAngelscriptCacheRecordKind InRecordKind = static_cast<EAngelscriptCacheRecordKind>(0),
		const uint64 InByteOffset = 0)
		: Error(InError)
		, Class(Classify(InError))
		, RecordKind(InRecordKind)
		, ByteOffset(InByteOffset)
	{
	}

	static FAngelscriptCacheValidationResult AtStage(
		const EAngelscriptCacheValidationError InError,
		const EAngelscriptCacheRecordKind InRecordKind,
		const EAngelscriptCacheValidationStage InStage,
		const uint64 InByteOffset = 0)
	{
		FAngelscriptCacheValidationResult Result(InError, InRecordKind, InByteOffset);
		Result.Stage = InStage;
		return Result;
	}

	static EAngelscriptCacheValidationClass Classify(const EAngelscriptCacheValidationError InError)
	{
		switch (InError)
		{
		case EAngelscriptCacheValidationError::None:
			return EAngelscriptCacheValidationClass::Success;

		case EAngelscriptCacheValidationError::BadMagic:
		case EAngelscriptCacheValidationError::UnsupportedSchema:
		case EAngelscriptCacheValidationError::UnsupportedPayloadSchema:
		case EAngelscriptCacheValidationError::UnknownRecordKind:
		case EAngelscriptCacheValidationError::UnknownEnumValue:
		case EAngelscriptCacheValidationError::UnknownFlags:
		case EAngelscriptCacheValidationError::InvalidBoolean:
		case EAngelscriptCacheValidationError::InvalidOptionalTag:
		case EAngelscriptCacheValidationError::NonZeroReserved:
		case EAngelscriptCacheValidationError::InvalidUtf8:
		case EAngelscriptCacheValidationError::EmbeddedNul:
		case EAngelscriptCacheValidationError::InvalidLogicalPath:
		case EAngelscriptCacheValidationError::TrailingData:
		case EAngelscriptCacheValidationError::InvalidArrayView:
		case EAngelscriptCacheValidationError::AliasedInputOutput:
			return EAngelscriptCacheValidationClass::Malformed;

		case EAngelscriptCacheValidationError::Overflow:
		case EAngelscriptCacheValidationError::BudgetExceeded:
		case EAngelscriptCacheValidationError::OutOfBounds:
		case EAngelscriptCacheValidationError::ImpossibleCount:
		case EAngelscriptCacheValidationError::NestingDepthExceeded:
			return EAngelscriptCacheValidationClass::ArithmeticOrBudget;

		case EAngelscriptCacheValidationError::ChecksumMismatch:
		case EAngelscriptCacheValidationError::RecordIdMismatch:
		case EAngelscriptCacheValidationError::UnsupportedCodecVersion:
		case EAngelscriptCacheValidationError::OpaquePayloadMalformed:
		case EAngelscriptCacheValidationError::OpaquePayloadHashMismatch:
		case EAngelscriptCacheValidationError::UnsupportedStorageCodec:
		case EAngelscriptCacheValidationError::DecompressionFailed:
		case EAngelscriptCacheValidationError::DecompressedSizeMismatch:
		case EAngelscriptCacheValidationError::PackIdMismatch:
		case EAngelscriptCacheValidationError::GenerationIdMismatch:
		case EAngelscriptCacheValidationError::PackIndexMismatch:
			return EAngelscriptCacheValidationClass::CodecOrIntegrity;

		case EAngelscriptCacheValidationError::OverlappingRange:
			return EAngelscriptCacheValidationClass::ArithmeticOrBudget;

		case EAngelscriptCacheValidationError::NonCanonicalOrder:
		case EAngelscriptCacheValidationError::DuplicateKey:
		case EAngelscriptCacheValidationError::ConflictingKey:
		case EAngelscriptCacheValidationError::CaseCollision:
		case EAngelscriptCacheValidationError::ZeroStableKey:
		case EAngelscriptCacheValidationError::MissingExpectedAbi:
		case EAngelscriptCacheValidationError::ForbiddenExpectedAbi:
		case EAngelscriptCacheValidationError::InvalidPresence:
		case EAngelscriptCacheValidationError::InvalidQualifierCombination:
		case EAngelscriptCacheValidationError::OrdinalGap:
		case EAngelscriptCacheValidationError::DuplicateOrdinal:
		case EAngelscriptCacheValidationError::DerivedHashMismatch:
			return EAngelscriptCacheValidationClass::CanonicalSemantic;

		case EAngelscriptCacheValidationError::MissingOwner:
		case EAngelscriptCacheValidationError::CrossModuleOwner:
		case EAngelscriptCacheValidationError::MissingGraphTarget:
		case EAngelscriptCacheValidationError::WrongReferenceKind:
		case EAngelscriptCacheValidationError::RelocationDependencyMismatch:
		case EAngelscriptCacheValidationError::WrongRecordKind:
		case EAngelscriptCacheValidationError::MissingRecord:
		case EAngelscriptCacheValidationError::MissingCoverage:
		case EAngelscriptCacheValidationError::UnexpectedRecord:
		case EAngelscriptCacheValidationError::UndeclaredEntity:
		case EAngelscriptCacheValidationError::DuplicateDebugOwner:
		case EAngelscriptCacheValidationError::DebugLinkMismatch:
		case EAngelscriptCacheValidationError::EnumAuthorityMismatch:
		case EAngelscriptCacheValidationError::InitializerOwnershipMismatch:
		case EAngelscriptCacheValidationError::GlobalCoverageMismatch:
		case EAngelscriptCacheValidationError::ProfileGraphMismatch:
		case EAngelscriptCacheValidationError::SourceGraphMismatch:
		case EAngelscriptCacheValidationError::GraphAbiMismatch:
		case EAngelscriptCacheValidationError::InvocationKindMismatch:
		case EAngelscriptCacheValidationError::DebugSourceMismatch:
			return EAngelscriptCacheValidationClass::GraphOrOwnership;

		case EAngelscriptCacheValidationError::CompatibilityMismatch:
		case EAngelscriptCacheValidationError::ContextMismatch:
		case EAngelscriptCacheValidationError::ProfileMismatch:
		case EAngelscriptCacheValidationError::SourceSnapshotMismatch:
		case EAngelscriptCacheValidationError::CurrentAbiMismatch:
		case EAngelscriptCacheValidationError::CurrentContentMismatch:
		case EAngelscriptCacheValidationError::CurrentSymbolMissing:
			return EAngelscriptCacheValidationClass::Ineligible;
		}

		checkNoEntry();
		return EAngelscriptCacheValidationClass::Malformed;
	}

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheValidationError::None;
	}
};

class FAngelscriptCacheReadBudget;
class FAngelscriptDecodedCacheRecord;
class FAngelscriptDecodedCacheRecordBatch;
struct FAngelscriptCacheSemanticCandidateAccess;
struct FAngelscriptCacheModuleGraphCandidateAccess;
#if WITH_ANGELSCRIPT_UNITTESTS
struct FAngelscriptCacheBudgetTests;
struct FAngelscriptDecodedCacheRecordTestAccess;
#endif

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheTemporaryResidentReservation
{
public:
	FAngelscriptCacheTemporaryResidentReservation() = default;
	~FAngelscriptCacheTemporaryResidentReservation();

	FAngelscriptCacheTemporaryResidentReservation(
		const FAngelscriptCacheTemporaryResidentReservation&) = delete;
	FAngelscriptCacheTemporaryResidentReservation& operator=(
		const FAngelscriptCacheTemporaryResidentReservation&) = delete;

	FAngelscriptCacheTemporaryResidentReservation(
		FAngelscriptCacheTemporaryResidentReservation&& Other) noexcept;
	FAngelscriptCacheTemporaryResidentReservation& operator=(
		FAngelscriptCacheTemporaryResidentReservation&& Other) noexcept;

	void Reset();
	bool PromoteToRetained();
	bool IsActive() const { return Owner != nullptr; }
	uint64 GetReservedBytes() const { return ReservedBytes; }

private:
	friend class FAngelscriptCacheReadBudget;

	FAngelscriptCacheReadBudget* Owner = nullptr;
	uint64 ReservedBytes = 0;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheReadBudget
{
public:
	FAngelscriptCacheReadBudget() = default;
	~FAngelscriptCacheReadBudget();
	FAngelscriptCacheReadBudget(const FAngelscriptCacheReadBudget&) = delete;
	FAngelscriptCacheReadBudget& operator=(const FAngelscriptCacheReadBudget&) = delete;
	FAngelscriptCacheReadBudget(FAngelscriptCacheReadBudget&&) = delete;
	FAngelscriptCacheReadBudget& operator=(FAngelscriptCacheReadBudget&&) = delete;

	bool TryConsumeStored(const uint64 Bytes, const FAngelscriptCacheReadLimits& Limits)
	{
		BindOrCheckThreadAccess();
		return TryConsume(Bytes, Limits.MaxTotalStoredBytes, StoredBytes);
	}

	bool TryConsumeDecompressed(const uint64 Bytes, const FAngelscriptCacheReadLimits& Limits)
	{
		BindOrCheckThreadAccess();
		return TryConsume(Bytes, Limits.MaxTotalDecompressedBytes, DecompressedBytes);
	}

	bool TryConsumeRetainedDecoded(
		const uint64 Bytes,
		const FAngelscriptCacheReadLimits& Limits)
	{
		BindOrCheckThreadAccess();
		if (Bytes == 0)
		{
			return true;
		}
		if (!CanConsume(Bytes, Limits.MaxTotalDecodedBytes, DecodedBytes)
			|| !CanConsumeLiveResident(Bytes, Limits.MaxResidentDecodedBytes))
		{
			return false;
		}
		DecodedBytes += Bytes;
		ResidentDecodedBytes += Bytes;
		UpdatePeakLiveResidentDecoded();
		return true;
	}

	bool TryReserveTemporaryDecoded(
		const uint64 Bytes,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheTemporaryResidentReservation& OutReservation)
	{
		BindOrCheckThreadAccess();
		if (OutReservation.IsActive())
		{
			return false;
		}
		check(OutReservation.ReservedBytes == 0);
		if (Bytes == 0)
		{
			return true;
		}
		if (!CanConsume(Bytes, Limits.MaxTotalDecodedBytes, DecodedBytes)
			|| !CanConsumeLiveResident(Bytes, Limits.MaxResidentDecodedBytes))
		{
			return false;
		}
		DecodedBytes += Bytes;
		TemporaryResidentDecodedBytes += Bytes;
		UpdatePeakLiveResidentDecoded();
		OutReservation.Owner = this;
		OutReservation.ReservedBytes = Bytes;
		return true;
	}

	bool TryConsumeReferencesAndRelocations(
		const uint64 Count,
		const FAngelscriptCacheReadLimits& Limits)
	{
		BindOrCheckThreadAccess();
		return TryConsume(Count, Limits.MaxReferencesAndRelocations, ReferencesAndRelocations);
	}

	uint64 GetStoredBytes() const { CheckThreadAccess(); return StoredBytes; }
	uint64 GetDecompressedBytes() const { CheckThreadAccess(); return DecompressedBytes; }
	uint64 GetDecodedBytes() const { CheckThreadAccess(); return DecodedBytes; }
	uint64 GetResidentDecodedBytes() const { CheckThreadAccess(); return ResidentDecodedBytes; }
	uint64 GetTemporaryResidentDecodedBytes() const
	{
		CheckThreadAccess();
		return TemporaryResidentDecodedBytes;
	}
	uint64 GetPeakLiveResidentDecodedBytes() const
	{
		CheckThreadAccess();
		return PeakLiveResidentDecodedBytes;
	}
	uint64 GetReferencesAndRelocations() const
	{
		CheckThreadAccess();
		return ReferencesAndRelocations;
	}

private:
	friend class FAngelscriptCacheTemporaryResidentReservation;
	friend class FAngelscriptDecodedCacheRecord;
	friend class FAngelscriptDecodedCacheRecordBatch;
	friend struct FAngelscriptCacheSemanticCandidateAccess;
	friend struct FAngelscriptCacheModuleGraphCandidateAccess;
#if WITH_ANGELSCRIPT_UNITTESTS
	friend struct FAngelscriptCacheBudgetTests;
	friend struct FAngelscriptDecodedCacheRecordTestAccess;
#endif

	enum class EDecodedCandidateExtendResult : uint8
	{
		Success,
		BudgetExceeded,
		Overflow,
		InvalidState,
	};

	class FDecodedCandidateTransaction final
	{
	public:
		~FDecodedCandidateTransaction();

		FDecodedCandidateTransaction(const FDecodedCandidateTransaction&) = delete;
		FDecodedCandidateTransaction& operator=(const FDecodedCandidateTransaction&) = delete;
		FDecodedCandidateTransaction(FDecodedCandidateTransaction&& Other) noexcept;
		FDecodedCandidateTransaction& operator=(FDecodedCandidateTransaction&& Other) = delete;

		EDecodedCandidateExtendResult TryExtend(uint64 Bytes);
		bool PromoteToRetained();
		bool IsOpen() const;
		uint64 GetAggregateTemporaryBytes() const { return AggregateTemporaryBytes; }

	private:
		friend class FAngelscriptCacheReadBudget;

		enum class EState : uint8
		{
			Open,
			Promoted,
			Closed,
			MovedFrom,
		};

		FDecodedCandidateTransaction(
			FAngelscriptCacheReadBudget& InOwner,
			const FAngelscriptCacheReadLimits& InLimits);

		void ReleaseWithoutRefundingTotal();
		void MakeMovedFrom();

		FAngelscriptCacheReadBudget* Owner = nullptr;
		uint64 MaxTotalDecodedBytes = 0;
		uint64 MaxResidentDecodedBytes = 0;
		uint64 AggregateTemporaryBytes = 0;
		EState State = EState::Closed;
	};

	FDecodedCandidateTransaction BeginDecodedCandidateTransaction(
		const FAngelscriptCacheReadLimits& Limits);
	EDecodedCandidateExtendResult TryExtendDecodedCandidate(
		FDecodedCandidateTransaction& Transaction,
		uint64 Bytes);
	bool PromoteDecodedCandidate(FDecodedCandidateTransaction& Transaction);
	void ReleaseDecodedCandidate(FDecodedCandidateTransaction& Transaction);

	void ReleaseTemporaryDecoded(const uint64 Bytes)
	{
		CheckThreadAccess();
		if (Bytes > TemporaryResidentDecodedBytes)
		{
			checkf(false,
				TEXT("AngelScript cache temporary resident reservation accounting underflow"));
			return;
		}
		TemporaryResidentDecodedBytes -= Bytes;
		UpdatePeakLiveResidentDecoded();
	}

	bool PromoteTemporaryDecoded(const uint64 Bytes)
	{
		CheckThreadAccess();
		if (Bytes == 0 || Bytes > TemporaryResidentDecodedBytes
			|| ResidentDecodedBytes > MAX_uint64 - Bytes)
		{
			checkf(false,
				TEXT("AngelScript cache temporary reservation promotion invariant failed"));
			return false;
		}
		TemporaryResidentDecodedBytes -= Bytes;
		ResidentDecodedBytes += Bytes;
		UpdatePeakLiveResidentDecoded();
		return true;
	}

	bool CanConsumeLiveResident(const uint64 Bytes, const uint64 Limit) const
	{
		return Bytes <= Limit
			&& ResidentDecodedBytes <= Limit - Bytes
			&& TemporaryResidentDecodedBytes <= Limit - Bytes - ResidentDecodedBytes;
	}

	void UpdatePeakLiveResidentDecoded()
	{
		check(ResidentDecodedBytes <= MAX_uint64 - TemporaryResidentDecodedBytes);
		PeakLiveResidentDecodedBytes = FMath::Max(
			PeakLiveResidentDecodedBytes,
			ResidentDecodedBytes + TemporaryResidentDecodedBytes);
	}

	static bool TryConsume(const uint64 Bytes, const uint64 Limit, uint64& InOutValue)
	{
		if (!CanConsume(Bytes, Limit, InOutValue))
		{
			return false;
		}
		InOutValue += Bytes;
		return true;
	}

	static bool CanConsume(const uint64 Bytes, const uint64 Limit, const uint64 Value)
	{
		return Bytes <= Limit && Value <= Limit - Bytes;
	}

	void BindOrCheckThreadAccess()
	{
#if DO_CHECK
		const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		if (OwnerThreadId == 0)
		{
			OwnerThreadId = CurrentThreadId;
		}
		else
		{
			checkf(OwnerThreadId == CurrentThreadId,
				TEXT("AngelScript cache read Budget is thread-affine"));
		}
#endif
	}

	void CheckThreadAccess() const
	{
#if DO_CHECK
		checkf(OwnerThreadId == 0 || OwnerThreadId == FPlatformTLS::GetCurrentThreadId(),
			TEXT("AngelScript cache read Budget is thread-affine"));
#endif
	}

	uint64 StoredBytes = 0;
	uint64 DecompressedBytes = 0;
	uint64 DecodedBytes = 0;
	uint64 ResidentDecodedBytes = 0;
	uint64 TemporaryResidentDecodedBytes = 0;
	uint64 PeakLiveResidentDecodedBytes = 0;
	uint64 ReferencesAndRelocations = 0;
#if DO_CHECK
	uint32 OwnerThreadId = 0;
	uint32 ActiveDecodedCandidateTransactions = 0;
#endif
};

inline FAngelscriptCacheReadBudget::~FAngelscriptCacheReadBudget()
{
	CheckThreadAccess();
#if DO_CHECK
	checkf(ActiveDecodedCandidateTransactions == 0,
		TEXT("AngelScript cache read Budget destroyed with an active decoded candidate"));
#endif
}

inline FAngelscriptCacheReadBudget::FDecodedCandidateTransaction::FDecodedCandidateTransaction(
	FAngelscriptCacheReadBudget& InOwner,
	const FAngelscriptCacheReadLimits& InLimits)
	: Owner(&InOwner)
	, MaxTotalDecodedBytes(InLimits.MaxTotalDecodedBytes)
	, MaxResidentDecodedBytes(InLimits.MaxResidentDecodedBytes)
	, State(EState::Open)
{
}

inline FAngelscriptCacheReadBudget::FDecodedCandidateTransaction::~FDecodedCandidateTransaction()
{
	ReleaseWithoutRefundingTotal();
}

inline FAngelscriptCacheReadBudget::FDecodedCandidateTransaction::FDecodedCandidateTransaction(
	FDecodedCandidateTransaction&& Other) noexcept
	: Owner(Other.Owner)
	, MaxTotalDecodedBytes(Other.MaxTotalDecodedBytes)
	, MaxResidentDecodedBytes(Other.MaxResidentDecodedBytes)
	, AggregateTemporaryBytes(Other.AggregateTemporaryBytes)
	, State(Other.State)
{
	Other.MakeMovedFrom();
}

inline FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult
FAngelscriptCacheReadBudget::FDecodedCandidateTransaction::TryExtend(const uint64 Bytes)
{
	if (Owner == nullptr || State != EState::Open)
	{
		return EDecodedCandidateExtendResult::InvalidState;
	}
	return Owner->TryExtendDecodedCandidate(*this, Bytes);
}

inline bool FAngelscriptCacheReadBudget::FDecodedCandidateTransaction::PromoteToRetained()
{
	return Owner != nullptr && State == EState::Open
		? Owner->PromoteDecodedCandidate(*this)
		: false;
}

inline bool FAngelscriptCacheReadBudget::FDecodedCandidateTransaction::IsOpen() const
{
	return Owner != nullptr && State == EState::Open;
}

inline void
FAngelscriptCacheReadBudget::FDecodedCandidateTransaction::ReleaseWithoutRefundingTotal()
{
	if (Owner != nullptr && State == EState::Open)
	{
		Owner->ReleaseDecodedCandidate(*this);
	}
}

inline void FAngelscriptCacheReadBudget::FDecodedCandidateTransaction::MakeMovedFrom()
{
	Owner = nullptr;
	MaxTotalDecodedBytes = 0;
	MaxResidentDecodedBytes = 0;
	AggregateTemporaryBytes = 0;
	State = EState::MovedFrom;
}

inline FAngelscriptCacheReadBudget::FDecodedCandidateTransaction
FAngelscriptCacheReadBudget::BeginDecodedCandidateTransaction(
	const FAngelscriptCacheReadLimits& Limits)
{
	BindOrCheckThreadAccess();
#if DO_CHECK
	check(ActiveDecodedCandidateTransactions < MAX_uint32);
	++ActiveDecodedCandidateTransactions;
#endif
	return FDecodedCandidateTransaction(*this, Limits);
}

inline FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult
FAngelscriptCacheReadBudget::TryExtendDecodedCandidate(
	FDecodedCandidateTransaction& Transaction,
	const uint64 Bytes)
{
	CheckThreadAccess();
	if (Transaction.Owner != this
		|| Transaction.State != FDecodedCandidateTransaction::EState::Open)
	{
		return EDecodedCandidateExtendResult::InvalidState;
	}
	if (Bytes == 0)
	{
		return EDecodedCandidateExtendResult::Success;
	}
	if (Transaction.AggregateTemporaryBytes > MAX_uint64 - Bytes)
	{
		return EDecodedCandidateExtendResult::Overflow;
	}

	if (!CanConsume(Bytes, Transaction.MaxTotalDecodedBytes, DecodedBytes)
		|| !CanConsumeLiveResident(Bytes, Transaction.MaxResidentDecodedBytes))
	{
		return EDecodedCandidateExtendResult::BudgetExceeded;
	}

	DecodedBytes += Bytes;
	TemporaryResidentDecodedBytes += Bytes;
	Transaction.AggregateTemporaryBytes += Bytes;
	UpdatePeakLiveResidentDecoded();
	return EDecodedCandidateExtendResult::Success;
}

inline bool FAngelscriptCacheReadBudget::PromoteDecodedCandidate(
	FDecodedCandidateTransaction& Transaction)
{
	CheckThreadAccess();
	if (Transaction.Owner != this
		|| Transaction.State != FDecodedCandidateTransaction::EState::Open
		|| Transaction.AggregateTemporaryBytes == 0)
	{
		return false;
	}

	const uint64 Bytes = Transaction.AggregateTemporaryBytes;
	if (Bytes > TemporaryResidentDecodedBytes
		|| ResidentDecodedBytes > MAX_uint64 - Bytes)
	{
		checkf(false,
			TEXT("AngelScript cache decoded-candidate promotion invariant failed"));
		return false;
	}

	TemporaryResidentDecodedBytes -= Bytes;
	ResidentDecodedBytes += Bytes;
	UpdatePeakLiveResidentDecoded();
#if DO_CHECK
	check(ActiveDecodedCandidateTransactions > 0);
	--ActiveDecodedCandidateTransactions;
#endif
	Transaction.Owner = nullptr;
	Transaction.MaxTotalDecodedBytes = 0;
	Transaction.MaxResidentDecodedBytes = 0;
	Transaction.AggregateTemporaryBytes = 0;
	Transaction.State = FDecodedCandidateTransaction::EState::Promoted;
	return true;
}

inline void FAngelscriptCacheReadBudget::ReleaseDecodedCandidate(
	FDecodedCandidateTransaction& Transaction)
{
	CheckThreadAccess();
	check(Transaction.Owner == this);
	check(Transaction.State == FDecodedCandidateTransaction::EState::Open);
	check(Transaction.AggregateTemporaryBytes <= TemporaryResidentDecodedBytes);

	TemporaryResidentDecodedBytes -= Transaction.AggregateTemporaryBytes;
	UpdatePeakLiveResidentDecoded();
#if DO_CHECK
	check(ActiveDecodedCandidateTransactions > 0);
	--ActiveDecodedCandidateTransactions;
#endif
	Transaction.Owner = nullptr;
	Transaction.MaxTotalDecodedBytes = 0;
	Transaction.MaxResidentDecodedBytes = 0;
	Transaction.AggregateTemporaryBytes = 0;
	Transaction.State = FDecodedCandidateTransaction::EState::Closed;
}

inline FAngelscriptCacheTemporaryResidentReservation::~FAngelscriptCacheTemporaryResidentReservation()
{
	Reset();
}

inline FAngelscriptCacheTemporaryResidentReservation::FAngelscriptCacheTemporaryResidentReservation(
	FAngelscriptCacheTemporaryResidentReservation&& Other) noexcept
	: Owner(Other.Owner)
	, ReservedBytes(Other.ReservedBytes)
{
	Other.Owner = nullptr;
	Other.ReservedBytes = 0;
}

inline FAngelscriptCacheTemporaryResidentReservation&
FAngelscriptCacheTemporaryResidentReservation::operator=(
	FAngelscriptCacheTemporaryResidentReservation&& Other) noexcept
{
	if (this != &Other)
	{
		Reset();
		Owner = Other.Owner;
		ReservedBytes = Other.ReservedBytes;
		Other.Owner = nullptr;
		Other.ReservedBytes = 0;
	}
	return *this;
}

inline void FAngelscriptCacheTemporaryResidentReservation::Reset()
{
	if (Owner != nullptr)
	{
		Owner->ReleaseTemporaryDecoded(ReservedBytes);
		Owner = nullptr;
		ReservedBytes = 0;
	}
}

inline bool FAngelscriptCacheTemporaryResidentReservation::PromoteToRetained()
{
	if (Owner == nullptr || ReservedBytes == 0)
	{
		return false;
	}

	if (!Owner->PromoteTemporaryDecoded(ReservedBytes))
	{
		return false;
	}
	Owner = nullptr;
	ReservedBytes = 0;
	return true;
}

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheRecordEnvelope
{
	FAngelscriptCacheRecordId RecordId;
	TArray<uint8> CanonicalPayload;

	void Reset()
	{
		RecordId = FAngelscriptCacheRecordId{};
		CanonicalPayload.Empty();
	}
};
