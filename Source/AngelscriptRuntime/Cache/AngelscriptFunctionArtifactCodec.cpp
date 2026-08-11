#include "Cache/AngelscriptFunctionArtifactCodec.h"

#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Cache/Private/AngelscriptCacheCanonicalCodec.h"
#include "Containers/StringConv.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"

#include "as_module.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_restore.h"
#include "as_scriptfunction.h"
#include "as_scriptengine.h"
#include "as_tokendef.h"

namespace AngelscriptFunctionArtifactCodec_Private
{
	static constexpr uint64 DebugMagic = UINT64_C(0x3147424453414555);
	static constexpr uint64 ExecutionEnvelopeMagic =
		UINT64_C(0x3158455441534555);

	static FAngelscriptCacheValidationResult Failure(
		const EAngelscriptCacheValidationError Error,
		const EAngelscriptCacheRecordKind Kind,
		const uint64 Offset = 0)
	{
		return FAngelscriptCacheValidationResult::AtStage(
			Error, Kind, EAngelscriptCacheValidationStage::OpaqueCodec, Offset);
	}

	static FAngelscriptCacheValidationResult ChargeFailure(
		const EAngelscriptCacheCandidateChargeResult Result,
		const EAngelscriptCacheRecordKind Kind,
		const uint64 Offset)
	{
		return Failure(
			Result == EAngelscriptCacheCandidateChargeResult::BudgetExceeded
				? EAngelscriptCacheValidationError::BudgetExceeded
				: EAngelscriptCacheValidationError::Overflow,
			Kind,
			Offset);
	}

	static bool TryBuildCurrentTypeKey(
		const asCModule& CurrentModule,
		const FAngelscriptStableModuleKey& ModuleKey,
		const asCTypeInfo* Type,
		FAngelscriptStableTypeKey& OutKey)
	{
		OutKey = {};
		return Type != nullptr && Type->module == &CurrentModule
			&& FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
				ModuleKey, *Type, OutKey);
	}

	static bool TryBuildCurrentPropertyKey(
		const asCModule& CurrentModule,
		const FAngelscriptStableModuleKey& ModuleKey,
		const asCTypeInfo* OwnerType,
		const asCObjectProperty* Property,
		FAngelscriptStablePropertyKey& OutKey)
	{
		OutKey = {};
		FAngelscriptStableTypeKey OwnerTypeKey;
		if (Property == nullptr
			|| !TryBuildCurrentTypeKey(
				CurrentModule, ModuleKey, OwnerType, OwnerTypeKey))
		{
			return false;
		}

		const asCString CanonicalType = Property->type.Format(
			OwnerType->nameSpace, false, false);
		FAngelscriptPropertyIdentityDescriptor Identity;
		Identity.OwnerTypeKey = OwnerTypeKey;
		Identity.Kind = EAngelscriptArtifactEntityKind::Property;
		Identity.Name = UTF8_TO_TCHAR(Property->name.AddressOf());
		Identity.CanonicalType = UTF8_TO_TCHAR(CanonicalType.AddressOf());
		OutKey = FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Identity);
		return !OutKey.Hash.IsZero();
	}

	static const asCTypeInfo* FindDerivedStaticClassOwnerType(
		const asCModule& CurrentModule,
		const asCGlobalProperty* Global)
	{
		if (Global == nullptr || Global->module != &CurrentModule
			|| !Global->type.IsReadOnly()
			|| !Global->name.StartsWith("__StaticType_"))
		{
			return nullptr;
		}

		const FString GlobalName = UTF8_TO_TCHAR(Global->name.AddressOf());
		const FString GlobalNamespace = Global->nameSpace != nullptr
			? UTF8_TO_TCHAR(Global->nameSpace->name.AddressOf()) : FString();
		const asCTypeInfo* Match = nullptr;
		for (asUINT Index = 0; Index < CurrentModule.GetObjectTypeCount(); ++Index)
		{
			const asCTypeInfo* Type = static_cast<const asCTypeInfo*>(
				CurrentModule.GetObjectTypeByIndex(Index));
			if (Type == nullptr || Type->module != &CurrentModule)
			{
				continue;
			}
			const FString ExpectedName = FString::Printf(
				TEXT("__StaticType_%s"), UTF8_TO_TCHAR(Type->GetName()));
			if (GlobalName == ExpectedName
				&& GlobalNamespace == UTF8_TO_TCHAR(Type->GetNamespace()))
			{
				if (Match != nullptr)
				{
					return nullptr;
				}
				Match = Type;
			}
		}
		return Match;
	}

	static const asCTypeInfo* FindDerivedTypeFunctionOwnerType(
		const asCModule& CurrentModule,
		const asCScriptFunction* Function)
	{
		if (Function == nullptr || Function->module != &CurrentModule)
		{
			return nullptr;
		}
		if (Function->artifactInvocationKind
				== asBUILD_ARTIFACT_INVOCATION_FACTORY
			&& Function->GetParamCount() == 0
			&& Function->artifactOwnerType != nullptr
			&& Function->artifactOwnerType->module == &CurrentModule
			&& Function->returnType.GetTypeInfo()
				== Function->artifactOwnerType)
		{
			return Function->artifactOwnerType;
		}
		if (Function->objectType != nullptr
			|| Function->name != "StaticClass"
			|| !Function->traits.GetTrait(asTRAIT_GENERATED_FUNCTION)
			|| FString(UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false)))
				!= TEXT("UClass StaticClass()"))
		{
			return nullptr;
		}

		const FString FunctionNamespace = UTF8_TO_TCHAR(
			Function->GetNamespace());
		const asCTypeInfo* Match = nullptr;
		for (asUINT Index = 0; Index < CurrentModule.GetObjectTypeCount(); ++Index)
		{
			const asCTypeInfo* Type = static_cast<const asCTypeInfo*>(
				CurrentModule.GetObjectTypeByIndex(Index));
			if (Type == nullptr || Type->module != &CurrentModule)
			{
				continue;
			}
			const FString TypeNamespace = UTF8_TO_TCHAR(Type->GetNamespace());
			const FString ExpectedFunctionNamespace = TypeNamespace.IsEmpty()
				? FString(UTF8_TO_TCHAR(Type->GetName()))
				: FString::Printf(TEXT("%s::%s"), *TypeNamespace,
					UTF8_TO_TCHAR(Type->GetName()));
			if (FunctionNamespace == ExpectedFunctionNamespace)
			{
				if (Match != nullptr)
				{
					return nullptr;
				}
				Match = Type;
			}
		}
		return Match;
	}

	static bool TryBuildCurrentGlobalKey(
		const asCModule& CurrentModule,
		const FAngelscriptStableModuleKey& ModuleKey,
		const asCGlobalProperty* Global,
		FAngelscriptStableGlobalKey& OutKey)
	{
		OutKey = {};
		if (Global == nullptr || Global->module != &CurrentModule
			|| ModuleKey.Hash.IsZero())
		{
			return false;
		}

		const asCString CanonicalType = Global->type.Format(
			Global->nameSpace, false, false);
		FAngelscriptGlobalIdentityDescriptor Identity;
		Identity.ModuleKey = ModuleKey;
		Identity.Namespace = Global->nameSpace != nullptr
			? UTF8_TO_TCHAR(Global->nameSpace->name.AddressOf()) : FString();
		Identity.Kind = EAngelscriptArtifactEntityKind::GlobalVariable;
		Identity.Name = UTF8_TO_TCHAR(Global->name.AddressOf());
		Identity.CanonicalType = UTF8_TO_TCHAR(CanonicalType.AddressOf());
		OutKey = FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Identity);
		return !OutKey.Hash.IsZero();
	}

	static bool TryBuildCurrentFunctionKey(
		const asCModule& CurrentModule,
		const FAngelscriptStableModuleKey& ModuleKey,
		const asCScriptFunction* Function,
		FAngelscriptStableFunctionKey& OutKey)
	{
		OutKey = {};
		return Function != nullptr && Function->module == &CurrentModule
			&& FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Function, OutKey);
	}

	class FFunctionArtifactReadStream final : public asIBinaryStream
	{
	public:
		explicit FFunctionArtifactReadStream(const TConstArrayView<uint8> InBytes)
			: Bytes(InBytes)
		{
		}

		virtual int Read(void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| static_cast<uint64>(Size)
					> static_cast<uint64>(Bytes.Num()) - Offset)
			{
				return asERROR;
			}
			if (Size != 0)
			{
				FMemory::Memcpy(Data, Bytes.GetData() + Offset, Size);
				Offset += Size;
			}
			return asSUCCESS;
		}

		virtual int Write(const void*, asUINT) override
		{
			return asERROR;
		}

		uint64 GetOffset() const
		{
			return Offset;
		}

	private:
		TConstArrayView<uint8> Bytes;
		uint64 Offset = 0;
	};

	class FExecutionEnvelopeReader final
	{
	public:
		FExecutionEnvelopeReader(
			const TConstArrayView<uint8> InBytes,
			const FAngelscriptCacheReadLimits& InLimits)
			: Bytes(InBytes)
			, Limits(InLimits)
		{
		}

		bool ReadUInt8(uint8& OutValue)
		{
			const uint8* Data = ReadFixed(1);
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = Data[0];
			return true;
		}

		bool ReadUInt32(uint32& OutValue)
		{
			const uint8* Data = ReadFixed(sizeof(uint32));
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = static_cast<uint32>(Data[0])
				| (static_cast<uint32>(Data[1]) << 8)
				| (static_cast<uint32>(Data[2]) << 16)
				| (static_cast<uint32>(Data[3]) << 24);
			return true;
		}

		bool ReadUInt64(uint64& OutValue)
		{
			const uint8* Data = ReadFixed(sizeof(uint64));
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = 0;
			for (uint32 Shift = 0; Shift < 64; Shift += 8)
			{
				OutValue |= static_cast<uint64>(Data[Shift / 8]) << Shift;
			}
			return true;
		}

		bool ReadHash(FAngelscriptHash256& OutValue)
		{
			const uint8* Data = ReadFixed(sizeof(FBlake3Hash::ByteArray));
			if (Data == nullptr)
			{
				return false;
			}
			FBlake3Hash::ByteArray HashBytes{};
			FMemory::Memcpy(HashBytes, Data, sizeof(HashBytes));
			OutValue = FAngelscriptHash256{FBlake3Hash(HashBytes)};
			return true;
		}

		bool ReadCount(uint32& OutCount)
		{
			const uint64 CountOffset = Offset;
			if (!ReadUInt32(OutCount))
			{
				return false;
			}
			if (OutCount > Limits.MaxArrayElements
				|| OutCount > static_cast<uint32>(MAX_int32))
			{
				SetFailure(EAngelscriptCacheValidationError::BudgetExceeded,
					CountOffset);
				return false;
			}
			return true;
		}

		bool ReadByteArray(TConstArrayView<uint8>& OutValue)
		{
			OutValue = {};
			const uint64 LengthOffset = Offset;
			uint64 Length = 0;
			if (!ReadUInt64(Length))
			{
				return false;
			}
			if (Length == 0 || Length > Limits.MaxCanonicalRecordPayloadBytes
				|| Length > static_cast<uint64>(MAX_int32))
			{
				SetFailure(
					Length == 0
						? EAngelscriptCacheValidationError::ImpossibleCount
						: EAngelscriptCacheValidationError::BudgetExceeded,
					LengthOffset);
				return false;
			}
			const uint8* Data = ReadFixed(Length);
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = TConstArrayView<uint8>(
				Data, static_cast<int32>(Length));
			return true;
		}

		bool IsAtEnd() const
		{
			return Offset == static_cast<uint64>(Bytes.Num());
		}

		uint64 GetOffset() const
		{
			return Offset;
		}

		const FAngelscriptCacheValidationResult& GetResult() const
		{
			return Result;
		}

		void SetFailure(
			const EAngelscriptCacheValidationError Error,
			const uint64 AtOffset)
		{
			if (Result.IsSuccess())
			{
				Result = Failure(
					Error, EAngelscriptCacheRecordKind::FunctionBody, AtOffset);
			}
		}

	private:
		const uint8* ReadFixed(const uint64 Count)
		{
			if (Count > static_cast<uint64>(Bytes.Num()) - Offset)
			{
				SetFailure(EAngelscriptCacheValidationError::OutOfBounds, Offset);
				return nullptr;
			}
			const uint8* Data = Bytes.GetData() + Offset;
			Offset += Count;
			return Data;
		}

		TConstArrayView<uint8> Bytes;
		const FAngelscriptCacheReadLimits& Limits;
		uint64 Offset = 0;
		FAngelscriptCacheValidationResult Result;
	};

	static FAngelscriptCacheValidationResult DecodeExecutionEnvelope(
		const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		IAngelscriptCacheCandidateChargeSink& GraphCandidate,
		FAngelscriptCacheOpaquePayloadSummary& OutSummary,
		TConstArrayView<uint8>& OutRawVmPayload)
	{
		OutSummary = {};
		OutRawVmPayload = {};
		if (Request.Kind != EAngelscriptCacheOpaquePayloadKind::FunctionExecution
			|| Request.CodecVersion
				!= FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion)
		{
			return Failure(EAngelscriptCacheValidationError::UnsupportedSchema,
				EAngelscriptCacheRecordKind::FunctionBody);
		}
		if (Request.CanonicalPayload.IsEmpty()
			|| static_cast<uint64>(Request.CanonicalPayload.Num())
				> Limits.MaxCanonicalRecordPayloadBytes)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				EAngelscriptCacheRecordKind::FunctionBody);
		}

		FAngelscriptCacheTemporaryResidentReservation Scratch;
		if (!Budget.TryReserveTemporaryDecoded(
			static_cast<uint64>(Request.CanonicalPayload.Num()), Limits, Scratch))
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				EAngelscriptCacheRecordKind::FunctionBody);
		}

		FExecutionEnvelopeReader Reader(Request.CanonicalPayload, Limits);
		uint64 Magic = 0;
		uint32 Version = 0;
		uint32 RelocationCount = 0;
		if (!Reader.ReadUInt64(Magic)
			|| !Reader.ReadUInt32(Version)
			|| !Reader.ReadCount(RelocationCount))
		{
			return Reader.GetResult();
		}
		if (Magic != ExecutionEnvelopeMagic
			|| Version != FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion)
		{
			return Failure(EAngelscriptCacheValidationError::UnsupportedSchema,
				EAngelscriptCacheRecordKind::FunctionBody, 0);
		}

		if (RelocationCount != 0)
		{
			int32 ReservedCapacity = 0;
			uint64 ReservedBytes = 0;
			if (!AngelscriptCacheCanonicalCodec_Private::
				TryCalculateArrayReserveBytes<FAngelscriptCacheRelocationUse>(
					static_cast<int32>(RelocationCount),
					ReservedCapacity,
					ReservedBytes))
			{
				return Failure(EAngelscriptCacheValidationError::Overflow,
					EAngelscriptCacheRecordKind::FunctionBody,
					Reader.GetOffset());
			}
			const EAngelscriptCacheCandidateChargeResult Charge =
				GraphCandidate.TryExtend(ReservedBytes);
			if (Charge != EAngelscriptCacheCandidateChargeResult::Success)
			{
				return ChargeFailure(Charge,
					EAngelscriptCacheRecordKind::FunctionBody,
					Reader.GetOffset());
			}
			OutSummary.OrderedRelocations.Reserve(ReservedCapacity);
			if (static_cast<uint64>(
					OutSummary.OrderedRelocations.GetAllocatedSize()) != ReservedBytes)
			{
				return Failure(EAngelscriptCacheValidationError::Overflow,
					EAngelscriptCacheRecordKind::FunctionBody,
					Reader.GetOffset());
			}
		}

		for (uint32 RelocationOrdinal = 0;
			RelocationOrdinal < RelocationCount; ++RelocationOrdinal)
		{
			const uint64 RelocationOffset = Reader.GetOffset();
			FAngelscriptCacheRelocationUse& Relocation =
				OutSummary.OrderedRelocations.AddDefaulted_GetRef();
			uint32 OperandSlot = 0;
			uint8 DependencyKind = 0;
			uint8 ReferenceKind = 0;
			uint8 HasExpectedContent = 0;
			if (!Reader.ReadUInt32(Relocation.InstructionOrdinal)
				|| !Reader.ReadUInt32(OperandSlot)
				|| !Reader.ReadUInt8(DependencyKind)
				|| !Reader.ReadUInt8(ReferenceKind)
				|| !Reader.ReadHash(Relocation.StableKey)
				|| !Reader.ReadHash(Relocation.ExpectedAbi)
				|| !Reader.ReadUInt8(HasExpectedContent))
			{
				return Reader.GetResult();
			}
			if (OperandSlot > MAX_uint16
				|| DependencyKind <= static_cast<uint8>(
					EAngelscriptCacheSemanticDependencyKind::Invalid)
				|| DependencyKind > static_cast<uint8>(
					EAngelscriptCacheSemanticDependencyKind::FunctionContent)
				|| ReferenceKind <= static_cast<uint8>(
					EAngelscriptCacheReferenceKind::Invalid)
				|| ReferenceKind > static_cast<uint8>(
					EAngelscriptCacheReferenceKind::StringLiteral)
				|| HasExpectedContent > 1
				|| Relocation.StableKey.IsZero()
				|| Relocation.ExpectedAbi.IsZero())
			{
				return Failure(
					EAngelscriptCacheValidationError::RelocationDependencyMismatch,
					EAngelscriptCacheRecordKind::FunctionBody,
					RelocationOffset);
			}
			Relocation.OperandSlot = static_cast<uint16>(OperandSlot);
			Relocation.DependencyKind =
				static_cast<EAngelscriptCacheSemanticDependencyKind>(DependencyKind);
			Relocation.ReferenceKind =
				static_cast<EAngelscriptCacheReferenceKind>(ReferenceKind);
			if (HasExpectedContent != 0)
			{
				FAngelscriptHash256 ExpectedContent;
				if (!Reader.ReadHash(ExpectedContent))
				{
					return Reader.GetResult();
				}
				if (ExpectedContent.IsZero())
				{
					return Failure(
						EAngelscriptCacheValidationError::RelocationDependencyMismatch,
						EAngelscriptCacheRecordKind::FunctionBody,
						RelocationOffset);
				}
				Relocation.ExpectedContentOrValue = ExpectedContent;
			}
		}

		if (!Reader.ReadByteArray(OutRawVmPayload))
		{
			return Reader.GetResult();
		}
		if (!Reader.IsAtEnd())
		{
			return Failure(EAngelscriptCacheValidationError::TrailingData,
				EAngelscriptCacheRecordKind::FunctionBody,
				Reader.GetOffset());
		}
		OutSummary.ValidatedPayloadHash =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				OutRawVmPayload, {}).Execution;
		return {};
	}

	static bool RelocationSummariesEqual(
		const TConstArrayView<FAngelscriptCacheRelocationUse> Left,
		const TConstArrayView<FAngelscriptCacheRelocationUse> Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			const FAngelscriptCacheRelocationUse& A = Left[Index];
			const FAngelscriptCacheRelocationUse& B = Right[Index];
			if (A.InstructionOrdinal != B.InstructionOrdinal
				|| A.OperandSlot != B.OperandSlot
				|| A.DependencyKind != B.DependencyKind
				|| A.ReferenceKind != B.ReferenceKind
				|| !(A.StableKey == B.StableKey)
				|| !(A.ExpectedAbi == B.ExpectedAbi)
				|| A.ExpectedContentOrValue.IsSet()
					!= B.ExpectedContentOrValue.IsSet()
				|| (A.ExpectedContentOrValue.IsSet()
					&& !(A.ExpectedContentOrValue.GetValue()
						== B.ExpectedContentOrValue.GetValue())))
			{
				return false;
			}
		}
		return true;
	}

	class FDebugReader final
	{
	public:
		FDebugReader(
			const TConstArrayView<uint8> InBytes,
			const FAngelscriptCacheReadLimits& InLimits,
			IAngelscriptCacheCandidateChargeSink& InGraphCandidate)
			: Bytes(InBytes)
			, Limits(InLimits)
			, GraphCandidate(InGraphCandidate)
		{
		}

		bool ReadUInt32(uint32& OutValue)
		{
			const uint8* Data = ReadFixed(sizeof(uint32));
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = static_cast<uint32>(Data[0])
				| (static_cast<uint32>(Data[1]) << 8)
				| (static_cast<uint32>(Data[2]) << 16)
				| (static_cast<uint32>(Data[3]) << 24);
			return true;
		}

		bool ReadUInt64(uint64& OutValue)
		{
			const uint8* Data = ReadFixed(sizeof(uint64));
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = 0;
			for (uint32 Shift = 0; Shift < 64; Shift += 8)
			{
				OutValue |= static_cast<uint64>(Data[Shift / 8]) << Shift;
			}
			return true;
		}

		bool ReadHash(FAngelscriptHash256& OutValue)
		{
			const uint8* Data = ReadFixed(sizeof(FBlake3Hash::ByteArray));
			if (Data == nullptr)
			{
				return false;
			}
			FBlake3Hash::ByteArray HashBytes{};
			FMemory::Memcpy(HashBytes, Data, sizeof(HashBytes));
			OutValue = FAngelscriptHash256{FBlake3Hash(HashBytes)};
			return true;
		}

		bool ReadCount(uint32& OutCount)
		{
			const uint64 CountOffset = Offset;
			if (!ReadUInt32(OutCount))
			{
				return false;
			}
			if (OutCount > Limits.MaxArrayElements
				|| OutCount > static_cast<uint32>(MAX_int32))
			{
				SetFailure(EAngelscriptCacheValidationError::BudgetExceeded,
					CountOffset);
				return false;
			}
			return true;
		}

		bool ReadRetainedString(FString& OutValue)
		{
			return ReadString(&OutValue);
		}

		bool SkipString()
		{
			return ReadString(nullptr);
		}

		bool IsAtEnd() const
		{
			return Offset == static_cast<uint64>(Bytes.Num());
		}

		uint64 GetOffset() const
		{
			return Offset;
		}

		const FAngelscriptCacheValidationResult& GetResult() const
		{
			return Result;
		}

		void SetFailure(
			const EAngelscriptCacheValidationError Error,
			const uint64 AtOffset)
		{
			if (Result.IsSuccess())
			{
				Result = Failure(
					Error, EAngelscriptCacheRecordKind::DebugSidecar, AtOffset);
			}
		}

	private:
		const uint8* ReadFixed(const uint64 Count)
		{
			if (Count > static_cast<uint64>(Bytes.Num()) - Offset)
			{
				SetFailure(EAngelscriptCacheValidationError::OutOfBounds, Offset);
				return nullptr;
			}
			const uint8* Data = Bytes.GetData() + Offset;
			Offset += Count;
			return Data;
		}

		bool ReadString(FString* OutValue)
		{
			if (OutValue != nullptr)
			{
				OutValue->Reset();
			}
			const uint64 FieldOffset = Offset;
			uint32 Length = 0;
			if (!ReadUInt32(Length))
			{
				return false;
			}
			if (Length > Limits.MaxStringBytes)
			{
				SetFailure(EAngelscriptCacheValidationError::BudgetExceeded,
					FieldOffset);
				return false;
			}
			const uint8* Data = ReadFixed(Length);
			if (Data == nullptr)
			{
				return false;
			}
			bool bEmbeddedNul = false;
			uint64 TCharCount = 0;
			if (!AngelscriptCacheCanonicalCodec_Private::ValidateUtf8(
				Data, Length, bEmbeddedNul, TCharCount))
			{
				SetFailure(
					bEmbeddedNul
						? EAngelscriptCacheValidationError::EmbeddedNul
						: EAngelscriptCacheValidationError::InvalidUtf8,
					FieldOffset);
				return false;
			}
			if (OutValue == nullptr || Length == 0)
			{
				return true;
			}
			if (TCharCount >= static_cast<uint64>(MAX_int32))
			{
				SetFailure(EAngelscriptCacheValidationError::ImpossibleCount,
					FieldOffset);
				return false;
			}
			int32 ReservedCapacity = 0;
			uint64 ReservedBytes = 0;
			if (!AngelscriptCacheCanonicalCodec_Private::
				TryCalculateArrayReserveBytes<TCHAR>(
				static_cast<int32>(TCharCount + 1),
				ReservedCapacity,
				ReservedBytes))
			{
				SetFailure(EAngelscriptCacheValidationError::Overflow, FieldOffset);
				return false;
			}
			const EAngelscriptCacheCandidateChargeResult Charge =
				GraphCandidate.TryExtend(ReservedBytes);
			if (Charge != EAngelscriptCacheCandidateChargeResult::Success)
			{
				Result = ChargeFailure(
					Charge, EAngelscriptCacheRecordKind::DebugSidecar, FieldOffset);
				return false;
			}
			TArray<TCHAR>& Characters = OutValue->GetCharArray();
			Characters.Reserve(ReservedCapacity);
			if (static_cast<uint64>(Characters.GetAllocatedSize()) != ReservedBytes)
			{
				SetFailure(EAngelscriptCacheValidationError::Overflow, FieldOffset);
				return false;
			}
			Characters.SetNumUninitialized(static_cast<int32>(TCharCount + 1));
			FUTF8ToTCHAR_Convert::Convert(
				Characters.GetData(), static_cast<int32>(TCharCount),
				reinterpret_cast<const ANSICHAR*>(Data), static_cast<int32>(Length));
			Characters[static_cast<int32>(TCharCount)] = TEXT('\0');
			return true;
		}

		TConstArrayView<uint8> Bytes;
		const FAngelscriptCacheReadLimits& Limits;
		IAngelscriptCacheCandidateChargeSink& GraphCandidate;
		uint64 Offset = 0;
		FAngelscriptCacheValidationResult Result;
	};

	static FAngelscriptCacheValidationResult DecodeDebugArtifact(
		const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
		const FAngelscriptCacheReadLimits& Limits,
		IAngelscriptCacheCandidateChargeSink& GraphCandidate,
		FAngelscriptCacheOpaquePayloadSummary& OutSummary,
		asCScriptFunction* ApplyToFunction = nullptr)
	{
		FDebugReader Reader(Request.CanonicalPayload, Limits, GraphCandidate);
		uint64 Magic = 0;
		uint32 Version = 0;
		if (!Reader.ReadUInt64(Magic) || Magic != DebugMagic)
		{
			if (Reader.GetResult().IsSuccess())
			{
				Reader.SetFailure(EAngelscriptCacheValidationError::BadMagic, 0);
			}
			return Reader.GetResult();
		}
		if (!Reader.ReadUInt32(Version)
			|| Version != FAngelscriptFunctionArtifactCodec::DebugCodecVersion)
		{
			if (Reader.GetResult().IsSuccess())
			{
				Reader.SetFailure(
					EAngelscriptCacheValidationError::UnsupportedSchema,
					sizeof(uint64));
			}
			return Reader.GetResult();
		}

		uint32 SourceCount = 0;
		if (!Reader.ReadCount(SourceCount) || SourceCount == 0)
		{
			if (Reader.GetResult().IsSuccess())
			{
				Reader.SetFailure(
					EAngelscriptCacheValidationError::ImpossibleCount,
					Reader.GetOffset() - sizeof(uint32));
			}
			return Reader.GetResult();
		}
		int32 ReservedCapacity = 0;
		uint64 ReservedBytes = 0;
		if (!AngelscriptCacheCanonicalCodec_Private::
			TryCalculateArrayReserveBytes<FAngelscriptCachedDebugSourceReference>(
			static_cast<int32>(SourceCount), ReservedCapacity, ReservedBytes))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::DebugSidecar, Reader.GetOffset());
		}
		const EAngelscriptCacheCandidateChargeResult SourceCharge =
			GraphCandidate.TryExtend(ReservedBytes);
		if (SourceCharge != EAngelscriptCacheCandidateChargeResult::Success)
		{
			return ChargeFailure(SourceCharge,
				EAngelscriptCacheRecordKind::DebugSidecar, Reader.GetOffset());
		}
		OutSummary.ExactDebugSources.Reserve(ReservedCapacity);
		if (static_cast<uint64>(OutSummary.ExactDebugSources.GetAllocatedSize())
			!= ReservedBytes)
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::DebugSidecar, Reader.GetOffset());
		}
		for (uint32 SourceOrdinal = 0; SourceOrdinal < SourceCount; ++SourceOrdinal)
		{
			FAngelscriptCachedDebugSourceReference& Source =
				OutSummary.ExactDebugSources.AddDefaulted_GetRef();
			if (!Reader.ReadHash(Source.SourceFileKey.Hash)
				|| !Reader.ReadHash(Source.LogicalSectionKey.Hash)
				|| !Reader.ReadRetainedString(Source.CanonicalLogicalSection))
			{
				return Reader.GetResult();
			}
			FAngelscriptCachedLogicalSectionKey Recomputed;
			const FAngelscriptCacheValidationResult KeyResult =
				FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
					Source.SourceFileKey,
					Source.CanonicalLogicalSection,
					Recomputed);
			if (!KeyResult.IsSuccess()
				|| !(Recomputed.Hash == Source.LogicalSectionKey.Hash))
			{
				return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch,
					EAngelscriptCacheRecordKind::DebugSidecar, Reader.GetOffset());
			}
		}

		uint32 DeclaredAt = 0;
		uint32 Count = 0;
		if (!Reader.ReadUInt32(DeclaredAt) || !Reader.ReadCount(Count))
		{
			return Reader.GetResult();
		}
		if (ApplyToFunction != nullptr)
		{
			if (ApplyToFunction->scriptData == nullptr
				|| OutSummary.ExactDebugSources.IsEmpty()
				|| DeclaredAt > static_cast<uint32>(MAX_int32))
			{
				return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
					EAngelscriptCacheRecordKind::DebugSidecar,
					Reader.GetOffset());
			}
			ApplyToFunction->scriptData->declaredAt = static_cast<int>(DeclaredAt);
			ApplyToFunction->scriptData->scriptSectionIdx =
				ApplyToFunction->engine->GetScriptSectionNameIndex(
					TCHAR_TO_UTF8(*OutSummary.ExactDebugSources[0].CanonicalLogicalSection));
			ApplyToFunction->scriptData->lineNumbers.SetLength(Count);
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			uint32 Line = 0;
			if (!Reader.ReadUInt32(Line))
			{
				return Reader.GetResult();
			}
			if (ApplyToFunction != nullptr)
			{
				ApplyToFunction->scriptData->lineNumbers[Index] =
					static_cast<int>(Line);
			}
		}
		if (!Reader.ReadCount(Count))
		{
			return Reader.GetResult();
		}
		if (Count > static_cast<uint32>(MAX_int32 / 2))
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				EAngelscriptCacheRecordKind::DebugSidecar,
				Reader.GetOffset() - sizeof(uint32));
		}
		if (ApplyToFunction != nullptr)
		{
			ApplyToFunction->scriptData->sectionIdxs.SetLength(Count * 2u);
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			uint32 ProgramPosition = 0;
			uint32 SourceOrdinal = 0;
			if (!Reader.ReadUInt32(ProgramPosition)
				|| !Reader.ReadUInt32(SourceOrdinal))
			{
				return Reader.GetResult();
			}
			if (SourceOrdinal >= SourceCount)
			{
				return Failure(EAngelscriptCacheValidationError::MissingGraphTarget,
					EAngelscriptCacheRecordKind::DebugSidecar,
					Reader.GetOffset() - sizeof(uint32));
			}
			if (ApplyToFunction != nullptr)
			{
				ApplyToFunction->scriptData->sectionIdxs[Index * 2u] =
					static_cast<int>(ProgramPosition);
				ApplyToFunction->scriptData->sectionIdxs[Index * 2u + 1u] =
					ApplyToFunction->engine->GetScriptSectionNameIndex(
						TCHAR_TO_UTF8(*OutSummary.ExactDebugSources[
							SourceOrdinal].CanonicalLogicalSection));
			}
		}
		if (!Reader.ReadCount(Count))
		{
			return Reader.GetResult();
		}
		if (ApplyToFunction != nullptr
			&& Count > ApplyToFunction->parameterTypes.GetLength())
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				EAngelscriptCacheRecordKind::DebugSidecar,
				Reader.GetOffset() - sizeof(uint32));
		}
		if (ApplyToFunction != nullptr)
		{
			ApplyToFunction->parameterNames.SetLength(Count);
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			if (ApplyToFunction != nullptr)
			{
				FString ParameterName;
				if (!Reader.ReadRetainedString(ParameterName))
				{
					return Reader.GetResult();
				}
				ApplyToFunction->parameterNames[Index] =
					TCHAR_TO_UTF8(*ParameterName);
			}
			else if (!Reader.SkipString())
			{
				return Reader.GetResult();
			}
		}
		if (!Reader.ReadCount(Count))
		{
			return Reader.GetResult();
		}
		if (ApplyToFunction != nullptr)
		{
			ApplyToFunction->scriptData->temporaryVariables.SetLength(Count);
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			uint32 StackOffset = 0;
			uint32 Token = 0;
			if (!Reader.ReadUInt32(StackOffset) || !Reader.ReadUInt32(Token))
			{
				return Reader.GetResult();
			}
			if (StackOffset > static_cast<uint32>(MAX_int32)
				|| Token > static_cast<uint32>(ttUnresolvedObject))
			{
				return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
					EAngelscriptCacheRecordKind::DebugSidecar,
					Reader.GetOffset() - sizeof(uint32) * 2u);
			}
			if (ApplyToFunction != nullptr)
			{
				ApplyToFunction->scriptData->temporaryVariables[Index].Offset =
					static_cast<int>(StackOffset);
				ApplyToFunction->scriptData->temporaryVariables[Index].Token =
					static_cast<eTokenType>(Token);
			}
		}
		if (!Reader.ReadCount(Count))
		{
			return Reader.GetResult();
		}
		if (ApplyToFunction != nullptr
			&& Count != ApplyToFunction->scriptData->variables.GetLength())
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				EAngelscriptCacheRecordKind::DebugSidecar,
				Reader.GetOffset() - sizeof(uint32));
		}
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FString Name;
			if (ApplyToFunction != nullptr)
			{
				if (!Reader.ReadRetainedString(Name))
				{
					return Reader.GetResult();
				}
			}
			else if (!Reader.SkipString())
			{
				return Reader.GetResult();
			}
			uint32 DeclaredAtProgramPosition = 0;
			if (!Reader.ReadUInt32(DeclaredAtProgramPosition)
				|| DeclaredAtProgramPosition > static_cast<uint32>(MAX_int32))
			{
				if (Reader.GetResult().IsSuccess())
				{
					Reader.SetFailure(
						EAngelscriptCacheValidationError::ImpossibleCount,
						Reader.GetOffset() - sizeof(uint32));
				}
				return Reader.GetResult();
			}
			if (ApplyToFunction != nullptr)
			{
				if (DeclaredAtProgramPosition
					> ApplyToFunction->scriptData->byteCode.GetLength()
					|| ApplyToFunction->scriptData->variables[Index] == nullptr)
				{
					return Failure(
						EAngelscriptCacheValidationError::ImpossibleCount,
						EAngelscriptCacheRecordKind::DebugSidecar,
						Reader.GetOffset() - sizeof(uint32));
				}
				ApplyToFunction->scriptData->variables[Index]->name =
					TCHAR_TO_UTF8(*Name);
				ApplyToFunction->scriptData->variables[Index]->
					declaredAtProgramPos = DeclaredAtProgramPosition;
			}
		}
		if (!Reader.IsAtEnd())
		{
			return Failure(EAngelscriptCacheValidationError::TrailingData,
				EAngelscriptCacheRecordKind::DebugSidecar, Reader.GetOffset());
		}
		OutSummary.ValidatedPayloadHash =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
				{}, Request.CanonicalPayload).Debug;
		return {};
	}

	class FAlreadyValidatedApplyChargeSink final
		: public IAngelscriptCacheCandidateChargeSink
	{
	public:
		virtual EAngelscriptCacheCandidateChargeResult TryExtend(uint64) override
		{
			return EAngelscriptCacheCandidateChargeResult::Success;
		}
	};
}

FAngelscriptFunctionArtifactCodec::FAngelscriptFunctionArtifactCodec(
	asCModule& InModule,
	asCScriptEngine& InEngine)
	: Module(&InModule)
	, Engine(&InEngine)
{
}

FAngelscriptCacheValidationResult
FAngelscriptFunctionArtifactCodec::EncodeDebugArtifact(
	const FAngelscriptFunctionDebugArtifact& Artifact,
	TArray<uint8>& OutPayload)
{
	using namespace AngelscriptFunctionArtifactCodec_Private;
	using AngelscriptCacheCanonicalCodec_Private::FWriter;

	OutPayload.Reset();
	if (Artifact.Sources.IsEmpty())
	{
		return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
			EAngelscriptCacheRecordKind::DebugSidecar);
	}
	for (const FAngelscriptCachedDebugSourceReference& Source : Artifact.Sources)
	{
		FAngelscriptCachedLogicalSectionKey Recomputed;
		const FAngelscriptCacheValidationResult KeyResult =
			FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
				Source.SourceFileKey,
				Source.CanonicalLogicalSection,
				Recomputed);
		if (!KeyResult.IsSuccess()
			|| !(Recomputed.Hash == Source.LogicalSectionKey.Hash))
		{
			return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch,
				EAngelscriptCacheRecordKind::DebugSidecar);
		}
	}
	for (const FAngelscriptFunctionDebugTransition& Transition
		: Artifact.SectionTransitions)
	{
		if (!Artifact.Sources.IsValidIndex(
			static_cast<int32>(Transition.SourceOrdinal)))
		{
			return Failure(EAngelscriptCacheValidationError::MissingGraphTarget,
				EAngelscriptCacheRecordKind::DebugSidecar);
		}
	}

	FWriter Writer;
	Writer.WriteUInt64(DebugMagic);
	Writer.WriteUInt32(DebugCodecVersion);
	Writer.WriteUInt32(static_cast<uint32>(Artifact.Sources.Num()));
	for (const FAngelscriptCachedDebugSourceReference& Source : Artifact.Sources)
	{
		Writer.WriteHash(Source.SourceFileKey.Hash);
		Writer.WriteHash(Source.LogicalSectionKey.Hash);
		Writer.WriteString(Source.CanonicalLogicalSection);
	}
	Writer.WriteUInt32(Artifact.DeclaredAt);
	Writer.WriteUInt32(static_cast<uint32>(Artifact.LineNumbers.Num()));
	for (const uint32 Line : Artifact.LineNumbers)
	{
		Writer.WriteUInt32(Line);
	}
	Writer.WriteUInt32(static_cast<uint32>(Artifact.SectionTransitions.Num()));
	for (const FAngelscriptFunctionDebugTransition& Transition
		: Artifact.SectionTransitions)
	{
		Writer.WriteUInt32(Transition.ProgramPosition);
		Writer.WriteUInt32(Transition.SourceOrdinal);
	}
	Writer.WriteUInt32(static_cast<uint32>(Artifact.ParameterNames.Num()));
	for (const FString& ParameterName : Artifact.ParameterNames)
	{
		Writer.WriteString(ParameterName);
	}
	Writer.WriteUInt32(static_cast<uint32>(Artifact.TemporaryVariables.Num()));
	for (const FAngelscriptFunctionDebugTemporary& Temporary
		: Artifact.TemporaryVariables)
	{
		Writer.WriteUInt32(Temporary.StackOffset);
		Writer.WriteUInt32(Temporary.Token);
	}
	Writer.WriteUInt32(static_cast<uint32>(Artifact.LocalVariables.Num()));
	for (const FAngelscriptFunctionDebugLocal& Local : Artifact.LocalVariables)
	{
		Writer.WriteString(Local.Name);
		Writer.WriteUInt32(Local.DeclaredAtProgramPosition);
	}
	OutPayload = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptFunctionArtifactCodec::EncodeExecutionArtifact(
	const TConstArrayView<uint8> RawVmPayload,
	const FAngelscriptStableModuleKey& ModuleKey,
	const TConstArrayView<FAngelscriptCacheSemanticDependency>
		DeclaredDependencies,
	TArray<uint8>& OutPayload,
	FAngelscriptHash256& OutExecutionContentHash) const
{
	using namespace AngelscriptFunctionArtifactCodec_Private;
	using AngelscriptCacheCanonicalCodec_Private::FWriter;

	OutPayload.Reset();
	OutExecutionContentHash = {};
	if (Module == nullptr || Engine == nullptr || RawVmPayload.IsEmpty()
		|| ModuleKey.Hash.IsZero())
	{
		return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	FAngelscriptCacheOpaquePayloadValidationRequest RawRequest;
	RawRequest.Kind = EAngelscriptCacheOpaquePayloadKind::FunctionExecution;
	RawRequest.CodecVersion = ExecutionCodecVersion;
	RawRequest.ModuleKey = ModuleKey;
	RawRequest.CanonicalPayload = RawVmPayload;
	RawRequest.DeclaredDependencies = DeclaredDependencies;
	FAngelscriptCacheReadLimits Limits;
	FAngelscriptCacheReadBudget Budget;
	FAlreadyValidatedApplyChargeSink ChargeSink;
	FAngelscriptCacheOpaquePayloadSummary Summary;
	const FAngelscriptCacheValidationResult Validation =
		ValidateRawExecutionArtifact(
			RawRequest, Limits, Budget, ChargeSink, Summary);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (Summary.OrderedRelocations.Num() < 0
		|| static_cast<uint64>(Summary.OrderedRelocations.Num()) > MAX_uint32)
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	FWriter Writer;
	Writer.WriteUInt64(ExecutionEnvelopeMagic);
	Writer.WriteUInt32(ExecutionCodecVersion);
	Writer.WriteUInt32(
		static_cast<uint32>(Summary.OrderedRelocations.Num()));
	for (const FAngelscriptCacheRelocationUse& Relocation
		: Summary.OrderedRelocations)
	{
		Writer.WriteUInt32(Relocation.InstructionOrdinal);
		Writer.WriteUInt32(Relocation.OperandSlot);
		Writer.WriteUInt8(static_cast<uint8>(Relocation.DependencyKind));
		Writer.WriteUInt8(static_cast<uint8>(Relocation.ReferenceKind));
		Writer.WriteHash(Relocation.StableKey);
		Writer.WriteHash(Relocation.ExpectedAbi);
		Writer.WriteOptionalHash(Relocation.ExpectedContentOrValue);
	}
	Writer.WriteByteArray(RawVmPayload);
	if (static_cast<uint64>(Writer.Bytes.Num())
		> Limits.MaxCanonicalRecordPayloadBytes)
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	OutExecutionContentHash = Summary.ValidatedPayloadHash;
	OutPayload = MoveTemp(Writer.Bytes);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptFunctionArtifactCodec::Validate(
	const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheCandidateChargeSink& GraphCandidate,
	FAngelscriptCacheOpaquePayloadSummary& OutSummary) const
{
	using namespace AngelscriptFunctionArtifactCodec_Private;

	OutSummary = {};
	LastExecutionFailureDetail.Reset();
	if (Request.CanonicalPayload.Num() < 0
		|| static_cast<uint64>(Request.CanonicalPayload.Num())
			> Limits.MaxCanonicalRecordPayloadBytes)
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody);
	}
	if (Request.Kind == EAngelscriptCacheOpaquePayloadKind::Debug)
	{
		if (Request.CodecVersion != DebugCodecVersion)
		{
			return Failure(EAngelscriptCacheValidationError::UnsupportedSchema,
				EAngelscriptCacheRecordKind::DebugSidecar);
		}
		return DecodeDebugArtifact(
			Request, Limits, GraphCandidate, OutSummary);
	}

	TConstArrayView<uint8> RawVmPayload;
	return DecodeExecutionEnvelope(
		Request, Limits, Budget, GraphCandidate, OutSummary, RawVmPayload);
}

FAngelscriptCacheValidationResult
FAngelscriptFunctionArtifactCodec::ValidateRawExecutionArtifact(
	const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheCandidateChargeSink& GraphCandidate,
	FAngelscriptCacheOpaquePayloadSummary& OutSummary) const
{
	using namespace AngelscriptFunctionArtifactCodec_Private;

	OutSummary = {};
	LastExecutionFailureDetail.Reset();
	if (Module == nullptr || Engine == nullptr
		|| Request.Kind
			!= EAngelscriptCacheOpaquePayloadKind::FunctionExecution
		|| Request.CanonicalPayload.Num() < 0
		|| static_cast<uint64>(Request.CanonicalPayload.Num())
			> Limits.MaxCanonicalRecordPayloadBytes)
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody);
	}
	if (static_cast<uint64>(Request.CanonicalPayload.Num()) > MAX_uint32)
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	FAngelscriptCacheTemporaryResidentReservation Scratch;
	if (!Budget.TryReserveTemporaryDecoded(
		static_cast<uint64>(Request.CanonicalPayload.Num()), Limits, Scratch))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody);
	}
	FFunctionArtifactReadStream Stream(Request.CanonicalPayload);
	asCReader Reader(Module, &Stream, Engine);
	asSFunctionArtifactValidationDiagnostics Diagnostics{};
	const int ValidationResult = Reader.ValidateFunctionArtifact(
		static_cast<asUINT>(Request.CanonicalPayload.Num()), &Diagnostics);
	if (ValidationResult < 0)
	{
		LastExecutionFailureDetail = FString::Printf(
			TEXT("FunctionArtifact result=%d expected=%u read=%u stream=%llu stage=%u error=%u new=%u"),
			Diagnostics.result,
			Diagnostics.expectedSize,
			Diagnostics.bytesRead,
			static_cast<unsigned long long>(Stream.GetOffset()),
			Diagnostics.stage,
			Diagnostics.hadError ? 1u : 0u,
			Diagnostics.wasNewFunction ? 1u : 0u);
		return Failure(EAngelscriptCacheValidationError::OpaquePayloadMalformed,
			EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
	}

	const asUINT RawSymbolUseCount =
		Reader.GetFunctionArtifactSymbolUseCount();
	if (RawSymbolUseCount > Limits.MaxArrayElements
		|| RawSymbolUseCount > static_cast<asUINT>(MAX_int32))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
	}
	if (RawSymbolUseCount != 0)
	{
		int32 ReservedCapacity = 0;
		uint64 ReservedBytes = 0;
		if (!AngelscriptCacheCanonicalCodec_Private::
			TryCalculateArrayReserveBytes<FAngelscriptCacheRelocationUse>(
				static_cast<int32>(RawSymbolUseCount),
				ReservedCapacity,
				ReservedBytes))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
		}
		const EAngelscriptCacheCandidateChargeResult Charge =
			GraphCandidate.TryExtend(ReservedBytes);
		if (Charge != EAngelscriptCacheCandidateChargeResult::Success)
		{
			return ChargeFailure(Charge,
				EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
		}
		OutSummary.OrderedRelocations.Reserve(ReservedCapacity);
		if (static_cast<uint64>(
				OutSummary.OrderedRelocations.GetAllocatedSize()) != ReservedBytes)
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
		}
	}

	for (asUINT SymbolUseOrdinal = 0;
		SymbolUseOrdinal < RawSymbolUseCount; ++SymbolUseOrdinal)
	{
		const asSFunctionArtifactSymbolUse* RawUse =
			Reader.GetFunctionArtifactSymbolUse(SymbolUseOrdinal);
		if (RawUse == nullptr || RawUse->operandSlot > MAX_uint16)
		{
			LastExecutionFailureDetail = FString::Printf(
				TEXT("Function artifact symbol use %u has an invalid coordinate"),
				SymbolUseOrdinal);
			return Failure(
				EAngelscriptCacheValidationError::RelocationDependencyMismatch,
				EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
		}

		EAngelscriptCacheSemanticDependencyKind ExpectedDependencyKind =
			EAngelscriptCacheSemanticDependencyKind::Invalid;
		EAngelscriptCacheReferenceKind ExpectedReferenceKind =
			static_cast<EAngelscriptCacheReferenceKind>(0);
		FAngelscriptHash256 CurrentStableKey;
		bool bResolvedStableIdentity = false;
		switch (RawUse->kind)
		{
		case asFUNCTION_ARTIFACT_SYMBOL_TYPE_DECLARATION:
		case asFUNCTION_ARTIFACT_SYMBOL_TYPE_VALUE_LAYOUT:
		{
			const bool bValidShape = RawUse->type != nullptr
				&& RawUse->function == nullptr
				&& RawUse->globalProperty == nullptr
				&& RawUse->propertyOwnerType == nullptr
				&& RawUse->objectProperty == nullptr;
			FAngelscriptStableTypeKey TypeKey;
			if (bValidShape && TryBuildCurrentTypeKey(
				*Module, Request.ModuleKey, RawUse->type, TypeKey))
			{
				bResolvedStableIdentity = true;
				CurrentStableKey = TypeKey.Hash;
				ExpectedDependencyKind = RawUse->kind
					== asFUNCTION_ARTIFACT_SYMBOL_TYPE_VALUE_LAYOUT
					? EAngelscriptCacheSemanticDependencyKind::ValueLayout
					: EAngelscriptCacheSemanticDependencyKind::Declaration;
				ExpectedReferenceKind =
					EAngelscriptCacheReferenceKind::ScriptType;
			}
			else if (bValidShape && RawUse->type->module == nullptr)
			{
				FAngelscriptCacheStableReference EnvironmentType;
				bResolvedStableIdentity =
					FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
						*RawUse->type, EnvironmentType);
				CurrentStableKey = EnvironmentType.StableKey;
				ExpectedDependencyKind =
					EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
				ExpectedReferenceKind =
					EAngelscriptCacheReferenceKind::EnvironmentSymbol;
			}
			break;
		}
		case asFUNCTION_ARTIFACT_SYMBOL_FUNCTION_SIGNATURE:
		{
			const bool bValidShape = RawUse->type == nullptr
				&& RawUse->function != nullptr
				&& RawUse->globalProperty == nullptr
				&& RawUse->propertyOwnerType == nullptr
				&& RawUse->objectProperty == nullptr;
			const asCTypeInfo* DerivedOwner = bValidShape
				? FindDerivedTypeFunctionOwnerType(*Module, RawUse->function)
				: nullptr;
			if (DerivedOwner != nullptr)
			{
				FAngelscriptStableTypeKey TypeKey;
				bResolvedStableIdentity = TryBuildCurrentTypeKey(
					*Module, Request.ModuleKey, DerivedOwner, TypeKey);
				CurrentStableKey = TypeKey.Hash;
				ExpectedDependencyKind =
					EAngelscriptCacheSemanticDependencyKind::Declaration;
				ExpectedReferenceKind =
					EAngelscriptCacheReferenceKind::ScriptType;
			}
			else
			{
				FAngelscriptCacheStableReference EnvironmentFunction;
				if (bValidShape && RawUse->function->module == nullptr
					&& FAngelscriptCacheEnvironmentIdentity::
						TryBuildFunctionReference(
							*RawUse->function, EnvironmentFunction))
				{
					bResolvedStableIdentity = true;
					CurrentStableKey = EnvironmentFunction.StableKey;
					ExpectedDependencyKind =
						EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
					ExpectedReferenceKind =
						EAngelscriptCacheReferenceKind::EnvironmentSymbol;
				}
				else
				{
					FAngelscriptStableFunctionKey FunctionKey;
					bResolvedStableIdentity = bValidShape
						&& TryBuildCurrentFunctionKey(
							*Module, Request.ModuleKey,
							RawUse->function, FunctionKey);
					CurrentStableKey = FunctionKey.Hash;
					ExpectedDependencyKind =
						EAngelscriptCacheSemanticDependencyKind::Signature;
					ExpectedReferenceKind =
						EAngelscriptCacheReferenceKind::ScriptFunction;
				}
			}
			break;
		}
		case asFUNCTION_ARTIFACT_SYMBOL_GLOBAL_STORAGE:
		{
			const bool bValidShape = RawUse->type == nullptr
				&& RawUse->function == nullptr
				&& RawUse->globalProperty != nullptr
				&& RawUse->propertyOwnerType == nullptr
				&& RawUse->objectProperty == nullptr;
			const asCTypeInfo* DerivedOwner = bValidShape
				? FindDerivedStaticClassOwnerType(
					*Module, RawUse->globalProperty)
				: nullptr;
			if (DerivedOwner != nullptr)
			{
				FAngelscriptStableTypeKey TypeKey;
				bResolvedStableIdentity = TryBuildCurrentTypeKey(
					*Module, Request.ModuleKey, DerivedOwner, TypeKey);
				CurrentStableKey = TypeKey.Hash;
				ExpectedDependencyKind =
					EAngelscriptCacheSemanticDependencyKind::Declaration;
				ExpectedReferenceKind =
					EAngelscriptCacheReferenceKind::ScriptType;
			}
			else
			{
				FAngelscriptStableGlobalKey GlobalKey;
				bResolvedStableIdentity = bValidShape
					&& TryBuildCurrentGlobalKey(
						*Module, Request.ModuleKey,
						RawUse->globalProperty, GlobalKey);
				CurrentStableKey = GlobalKey.Hash;
				ExpectedDependencyKind =
					EAngelscriptCacheSemanticDependencyKind::GlobalStorage;
				ExpectedReferenceKind =
					EAngelscriptCacheReferenceKind::ScriptGlobal;
			}
			break;
		}
		case asFUNCTION_ARTIFACT_SYMBOL_PROPERTY_LAYOUT:
		{
			FAngelscriptStablePropertyKey PropertyKey;
			bResolvedStableIdentity = RawUse->type == nullptr
				&& RawUse->function == nullptr
				&& RawUse->globalProperty == nullptr
				&& RawUse->propertyOwnerType != nullptr
				&& RawUse->objectProperty != nullptr
				&& TryBuildCurrentPropertyKey(
					*Module, Request.ModuleKey,
					RawUse->propertyOwnerType,
					RawUse->objectProperty, PropertyKey);
			CurrentStableKey = PropertyKey.Hash;
			ExpectedDependencyKind =
				EAngelscriptCacheSemanticDependencyKind::PropertyLayout;
			ExpectedReferenceKind =
				EAngelscriptCacheReferenceKind::ScriptProperty;
			break;
		}
		default:
			break;
		}
		if (!bResolvedStableIdentity || CurrentStableKey.IsZero())
		{
			FString SymbolDetail = TEXT("shape unavailable");
			if (RawUse->kind
					== asFUNCTION_ARTIFACT_SYMBOL_FUNCTION_SIGNATURE
				&& RawUse->function != nullptr)
			{
				const asCScriptFunction& Function = *RawUse->function;
				const FString ModuleOwner = Function.module == Module
					? TEXT("current")
					: Function.module == nullptr
						? TEXT("environment")
						: FString::Printf(TEXT("other:%s"),
							UTF8_TO_TCHAR(Function.module->name.AddressOf()));
				FString FunctionKeyFailure;
				FAngelscriptStableFunctionKey DiagnosticFunctionKey;
				const bool bFunctionKeyBuilt = Function.module == Module
					&& FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
						Request.ModuleKey, Function, DiagnosticFunctionKey,
						&FunctionKeyFailure);
				SymbolDetail = FString::Printf(
					TEXT("function=%s namespace=%s module=%s invocation=%u "
						"objectType=%s artifactOwner=%s traits=0x%08x "
						"currentKey=%s keyFailure=%s"),
					UTF8_TO_TCHAR(Function.GetDeclaration(false, false, false)),
					UTF8_TO_TCHAR(Function.GetNamespace()),
					*ModuleOwner,
					static_cast<uint32>(Function.artifactInvocationKind),
					Function.objectType != nullptr
						? UTF8_TO_TCHAR(Function.objectType->GetName()) : TEXT("<none>"),
					Function.artifactOwnerType != nullptr
						? UTF8_TO_TCHAR(Function.artifactOwnerType->GetName())
						: TEXT("<none>"),
					static_cast<uint32>(Function.traits.traits),
					bFunctionKeyBuilt
						? *DiagnosticFunctionKey.Hash.ToHexString() : TEXT("<none>"),
					FunctionKeyFailure.IsEmpty()
						? TEXT("<none>") : *FunctionKeyFailure);
			}
			LastExecutionFailureDetail = FString::Printf(
				TEXT("Function artifact symbol use %u kind %u could not resolve to one current stable identity: %s"),
				SymbolUseOrdinal, static_cast<uint32>(RawUse->kind),
				*SymbolDetail);
			return Failure(
				EAngelscriptCacheValidationError::RelocationDependencyMismatch,
				EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
		}

		const FAngelscriptCacheSemanticDependency* MatchingDependency = nullptr;
		for (const FAngelscriptCacheSemanticDependency& Dependency
			: Request.DeclaredDependencies)
		{
			if (Dependency.Kind == ExpectedDependencyKind
				&& Dependency.Target.Kind == ExpectedReferenceKind
				&& Dependency.Target.StableKey == CurrentStableKey)
			{
				if (MatchingDependency != nullptr)
				{
					LastExecutionFailureDetail = FString::Printf(
						TEXT("Function artifact symbol use %u has duplicate stable dependency %s"),
						SymbolUseOrdinal,
						*CurrentStableKey.ToHexString());
					return Failure(
						EAngelscriptCacheValidationError::RelocationDependencyMismatch,
						EAngelscriptCacheRecordKind::FunctionBody,
						Stream.GetOffset());
				}
				MatchingDependency = &Dependency;
			}
		}
		if (MatchingDependency == nullptr)
		{
			FString AvailableDependencies;
			const int32 DiagnosticLimit = FMath::Min(
				Request.DeclaredDependencies.Num(), 8);
			for (int32 DependencyIndex = 0;
				DependencyIndex < DiagnosticLimit; ++DependencyIndex)
			{
				const FAngelscriptCacheSemanticDependency& Dependency =
					Request.DeclaredDependencies[DependencyIndex];
				if (!AvailableDependencies.IsEmpty())
				{
					AvailableDependencies += TEXT(", ");
				}
				AvailableDependencies += FString::Printf(
					TEXT("%u/%u/%s"),
					static_cast<uint32>(Dependency.Kind),
					static_cast<uint32>(Dependency.Target.Kind),
					*Dependency.Target.StableKey.ToHexString());
			}
			if (Request.DeclaredDependencies.Num() > DiagnosticLimit)
			{
				AvailableDependencies += FString::Printf(
					TEXT(", ... +%d"),
					Request.DeclaredDependencies.Num() - DiagnosticLimit);
			}
			LastExecutionFailureDetail = FString::Printf(
				TEXT("Function artifact symbol use %u kind %u expects dependency %u/%u/%s but declared %d [%s]"),
				SymbolUseOrdinal, static_cast<uint32>(RawUse->kind),
				static_cast<uint32>(ExpectedDependencyKind),
				static_cast<uint32>(ExpectedReferenceKind),
				*CurrentStableKey.ToHexString(),
				Request.DeclaredDependencies.Num(),
				AvailableDependencies.IsEmpty()
					? TEXT("none") : *AvailableDependencies);
			return Failure(
				EAngelscriptCacheValidationError::RelocationDependencyMismatch,
				EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
		}

		FAngelscriptCacheRelocationUse& Relocation =
			OutSummary.OrderedRelocations.AddDefaulted_GetRef();
		Relocation.InstructionOrdinal = RawUse->instructionOrdinal;
		Relocation.OperandSlot = static_cast<uint16>(RawUse->operandSlot);
		Relocation.DependencyKind = MatchingDependency->Kind;
		Relocation.ReferenceKind = MatchingDependency->Target.Kind;
		Relocation.StableKey = MatchingDependency->Target.StableKey;
		Relocation.ExpectedAbi = MatchingDependency->Target.ExpectedAbi;
		Relocation.ExpectedContentOrValue =
			MatchingDependency->ExpectedContentOrValue;
	}
	OutSummary.ValidatedPayloadHash =
		FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
			Request.CanonicalPayload, {}).Execution;
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptFunctionArtifactCodec::RestoreGlobalFunction(
	const TConstArrayView<uint8> CanonicalExecutionPayload,
	const TConstArrayView<uint8> CanonicalDebugPayload,
	const FAngelscriptStableModuleKey& ModuleKey,
	const TConstArrayView<FAngelscriptCacheSemanticDependency>
		DeclaredDependencies,
	const FAngelscriptHash256& ExpectedExecutionContentHash,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	asCScriptFunction*& OutFunction) const
{
	using namespace AngelscriptFunctionArtifactCodec_Private;

	OutFunction = nullptr;
	LastExecutionFailureDetail.Reset();
	if (Module == nullptr || Engine == nullptr
		|| ModuleKey.Hash.IsZero()
		|| ExpectedExecutionContentHash.IsZero()
		|| CanonicalExecutionPayload.IsEmpty()
		|| CanonicalDebugPayload.IsEmpty()
		|| static_cast<uint64>(CanonicalExecutionPayload.Num())
			> Limits.MaxCanonicalRecordPayloadBytes
		|| static_cast<uint64>(CanonicalDebugPayload.Num())
			> Limits.MaxCanonicalRecordPayloadBytes)
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody);
	}
	FAngelscriptCacheOpaquePayloadValidationRequest EnvelopeRequest;
	EnvelopeRequest.Kind = EAngelscriptCacheOpaquePayloadKind::FunctionExecution;
	EnvelopeRequest.CodecVersion = ExecutionCodecVersion;
	EnvelopeRequest.ModuleKey = ModuleKey;
	EnvelopeRequest.CanonicalPayload = CanonicalExecutionPayload;
	EnvelopeRequest.DeclaredDependencies = DeclaredDependencies;
	FAlreadyValidatedApplyChargeSink ApplyCharge;
	FAngelscriptCacheOpaquePayloadSummary EnvelopeSummary;
	TConstArrayView<uint8> RawVmPayload;
	const FAngelscriptCacheValidationResult EnvelopeResult =
		DecodeExecutionEnvelope(
			EnvelopeRequest, Limits, Budget, ApplyCharge,
			EnvelopeSummary, RawVmPayload);
	if (!EnvelopeResult.IsSuccess()
		|| !(EnvelopeSummary.ValidatedPayloadHash
			== ExpectedExecutionContentHash))
	{
		return EnvelopeResult.IsSuccess()
			? Failure(EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
				EAngelscriptCacheRecordKind::FunctionBody)
			: EnvelopeResult;
	}

	FAngelscriptCacheOpaquePayloadValidationRequest RawRequest = EnvelopeRequest;
	RawRequest.CanonicalPayload = RawVmPayload;
	FAngelscriptCacheOpaquePayloadSummary RawSummary;
	const FAngelscriptCacheValidationResult RawValidation =
		ValidateRawExecutionArtifact(
			RawRequest, Limits, Budget, ApplyCharge, RawSummary);
	if (!RawValidation.IsSuccess())
	{
		return RawValidation;
	}
	if (!RelocationSummariesEqual(
		EnvelopeSummary.OrderedRelocations,
		RawSummary.OrderedRelocations))
	{
		LastExecutionFailureDetail =
			TEXT("Execution envelope relocation manifest differs from the skeleton-resolved VM artifact");
		return Failure(
			EAngelscriptCacheValidationError::RelocationDependencyMismatch,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	if (static_cast<uint64>(RawVmPayload.Num()) > MAX_uint32)
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	FAngelscriptCacheTemporaryResidentReservation Scratch;
	if (!Budget.TryReserveTemporaryDecoded(
		static_cast<uint64>(RawVmPayload.Num()), Limits, Scratch))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	FFunctionArtifactReadStream Stream(RawVmPayload);
	asCReader Reader(Module, &Stream, Engine);
	asSFunctionArtifactValidationDiagnostics Diagnostics{};
	const int RestoreResult = Reader.RestoreGlobalFunctionArtifact(
		static_cast<asUINT>(RawVmPayload.Num()),
		&OutFunction,
		&Diagnostics);
	if (RestoreResult < 0 || OutFunction == nullptr)
	{
		LastExecutionFailureDetail = FString::Printf(
			TEXT("FunctionArtifact restore=%d expected=%u read=%u stream=%llu stage=%u error=%u new=%u"),
			Diagnostics.result,
			Diagnostics.expectedSize,
			Diagnostics.bytesRead,
			static_cast<unsigned long long>(Stream.GetOffset()),
			Diagnostics.stage,
			Diagnostics.hadError ? 1u : 0u,
			Diagnostics.wasNewFunction ? 1u : 0u);
		return Failure(EAngelscriptCacheValidationError::OpaquePayloadMalformed,
			EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
	}

	FAngelscriptCacheOpaquePayloadValidationRequest DebugRequest;
	DebugRequest.Kind = EAngelscriptCacheOpaquePayloadKind::Debug;
	DebugRequest.CodecVersion = DebugCodecVersion;
	DebugRequest.CanonicalPayload = CanonicalDebugPayload;
	FAngelscriptCacheOpaquePayloadSummary DebugSummary;
	const FAngelscriptCacheValidationResult DebugResult = DecodeDebugArtifact(
		DebugRequest,
		Limits,
		ApplyCharge,
		DebugSummary,
		OutFunction);
	if (!DebugResult.IsSuccess())
	{
		LastExecutionFailureDetail = FString::Printf(
			TEXT("DebugArtifact apply failed: Error=%u Offset=%llu"),
			static_cast<uint32>(DebugResult.Error),
			DebugResult.ByteOffset);
		return DebugResult;
	}

	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptFunctionArtifactCodec::RestoreFunctionIntoExisting(
	const TConstArrayView<uint8> CanonicalExecutionPayload,
	const TConstArrayView<uint8> CanonicalDebugPayload,
	const FAngelscriptStableModuleKey& ModuleKey,
	const TConstArrayView<FAngelscriptCacheSemanticDependency>
		DeclaredDependencies,
	const FAngelscriptHash256& ExpectedExecutionContentHash,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	asCScriptFunction& TargetFunction) const
{
	using namespace AngelscriptFunctionArtifactCodec_Private;

	LastExecutionFailureDetail.Reset();
	if (Module == nullptr || Engine == nullptr
		|| TargetFunction.module != Module
		|| TargetFunction.engine != Engine
		|| ModuleKey.Hash.IsZero()
		|| ExpectedExecutionContentHash.IsZero()
		|| CanonicalExecutionPayload.IsEmpty()
		|| CanonicalDebugPayload.IsEmpty()
		|| static_cast<uint64>(CanonicalExecutionPayload.Num())
			> Limits.MaxCanonicalRecordPayloadBytes
		|| static_cast<uint64>(CanonicalDebugPayload.Num())
			> Limits.MaxCanonicalRecordPayloadBytes)
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody);
	}
	FAngelscriptCacheOpaquePayloadValidationRequest EnvelopeRequest;
	EnvelopeRequest.Kind = EAngelscriptCacheOpaquePayloadKind::FunctionExecution;
	EnvelopeRequest.CodecVersion = ExecutionCodecVersion;
	EnvelopeRequest.ModuleKey = ModuleKey;
	EnvelopeRequest.CanonicalPayload = CanonicalExecutionPayload;
	EnvelopeRequest.DeclaredDependencies = DeclaredDependencies;
	FAlreadyValidatedApplyChargeSink ApplyCharge;
	FAngelscriptCacheOpaquePayloadSummary EnvelopeSummary;
	TConstArrayView<uint8> RawVmPayload;
	const FAngelscriptCacheValidationResult EnvelopeResult =
		DecodeExecutionEnvelope(
			EnvelopeRequest, Limits, Budget, ApplyCharge,
			EnvelopeSummary, RawVmPayload);
	if (!EnvelopeResult.IsSuccess()
		|| !(EnvelopeSummary.ValidatedPayloadHash
			== ExpectedExecutionContentHash))
	{
		return EnvelopeResult.IsSuccess()
			? Failure(EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
				EAngelscriptCacheRecordKind::FunctionBody)
			: EnvelopeResult;
	}

	FAngelscriptCacheOpaquePayloadValidationRequest RawRequest = EnvelopeRequest;
	RawRequest.CanonicalPayload = RawVmPayload;
	FAngelscriptCacheOpaquePayloadSummary RawSummary;
	const FAngelscriptCacheValidationResult RawValidation =
		ValidateRawExecutionArtifact(
			RawRequest, Limits, Budget, ApplyCharge, RawSummary);
	if (!RawValidation.IsSuccess())
	{
		return RawValidation;
	}
	if (!RelocationSummariesEqual(
		EnvelopeSummary.OrderedRelocations,
		RawSummary.OrderedRelocations))
	{
		LastExecutionFailureDetail =
			TEXT("Execution envelope relocation manifest differs from the skeleton-resolved VM artifact");
		return Failure(
			EAngelscriptCacheValidationError::RelocationDependencyMismatch,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	if (static_cast<uint64>(RawVmPayload.Num()) > MAX_uint32)
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	FAngelscriptCacheTemporaryResidentReservation Scratch;
	if (!Budget.TryReserveTemporaryDecoded(
		static_cast<uint64>(RawVmPayload.Num()), Limits, Scratch))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheRecordKind::FunctionBody);
	}

	FFunctionArtifactReadStream Stream(RawVmPayload);
	asCReader Reader(Module, &Stream, Engine);
	asSFunctionArtifactValidationDiagnostics Diagnostics{};
	asCScriptFunction* Donor = nullptr;
	const int RestoreResult = Reader.RestoreFunctionArtifactDetached(
		static_cast<asUINT>(RawVmPayload.Num()),
		&Donor,
		&Diagnostics);
	if (RestoreResult < 0 || Donor == nullptr)
	{
		LastExecutionFailureDetail = FString::Printf(
			TEXT("FunctionArtifact detached-restore=%d expected=%u read=%u stream=%llu stage=%u error=%u new=%u"),
			Diagnostics.result,
			Diagnostics.expectedSize,
			Diagnostics.bytesRead,
			static_cast<unsigned long long>(Stream.GetOffset()),
			Diagnostics.stage,
			Diagnostics.hadError ? 1u : 0u,
			Diagnostics.wasNewFunction ? 1u : 0u);
		return Failure(EAngelscriptCacheValidationError::OpaquePayloadMalformed,
			EAngelscriptCacheRecordKind::FunctionBody, Stream.GetOffset());
	}

	FAngelscriptCacheOpaquePayloadValidationRequest DebugRequest;
	DebugRequest.Kind = EAngelscriptCacheOpaquePayloadKind::Debug;
	DebugRequest.CodecVersion = DebugCodecVersion;
	DebugRequest.CanonicalPayload = CanonicalDebugPayload;
	FAngelscriptCacheOpaquePayloadSummary DebugSummary;
	const FAngelscriptCacheValidationResult DebugResult = DecodeDebugArtifact(
		DebugRequest,
		Limits,
		ApplyCharge,
		DebugSummary,
		Donor);
	if (!DebugResult.IsSuccess())
	{
		LastExecutionFailureDetail = FString::Printf(
			TEXT("DebugArtifact donor apply failed: Error=%u Offset=%llu"),
			static_cast<uint32>(DebugResult.Error),
			DebugResult.ByteOffset);
		Donor->DestroyHalfCreated();
		return DebugResult;
	}

	const int CommitResult = Reader.CommitFunctionArtifactToExisting(
		Donor, &TargetFunction);
	if (CommitResult < 0)
	{
		LastExecutionFailureDetail = FString::Printf(
			TEXT("FunctionArtifact target commit failed: Result=%d TargetId=%d EngineEqual=%d ModuleEqual=%d ArtifactFuncType=%d TargetFuncType=%d ArtifactHasScriptData=%d TargetHasScriptData=%d ArtifactWords=%u TargetWords=%u ObjectTypeEqual=%d NamespaceEqual=%d EncodedRootTraits=0x%08x ArtifactTraits=0x%08x TargetTraits=0x%08x SignatureEqual=%d ArtifactDeclaration=%s TargetDeclaration=%s"),
			CommitResult,
			TargetFunction.id,
			Donor->engine == TargetFunction.engine ? 1 : 0,
			Donor->module == TargetFunction.module ? 1 : 0,
			static_cast<int32>(Donor->funcType),
			static_cast<int32>(TargetFunction.funcType),
			Donor->scriptData != nullptr ? 1 : 0,
			TargetFunction.scriptData != nullptr ? 1 : 0,
			Donor->scriptData != nullptr
				? Donor->scriptData->byteCode.GetLength() : 0,
			TargetFunction.scriptData != nullptr
				? TargetFunction.scriptData->byteCode.GetLength() : 0,
			Donor->objectType == TargetFunction.objectType ? 1 : 0,
			Donor->nameSpace == TargetFunction.nameSpace ? 1 : 0,
			Diagnostics.rootTraits,
			Donor->traits.traits,
			TargetFunction.traits.traits,
			Donor->IsSignatureEqual(&TargetFunction) ? 1 : 0,
			UTF8_TO_TCHAR(Donor->GetDeclaration(true, true, false)),
			UTF8_TO_TCHAR(TargetFunction.GetDeclaration(true, true, false)));
		Donor->DestroyHalfCreated();
		return Failure(EAngelscriptCacheValidationError::OpaquePayloadMalformed,
			EAngelscriptCacheRecordKind::FunctionBody,
			static_cast<uint64>(RawVmPayload.Num()));
	}
	return {};
}

const FString& FAngelscriptFunctionArtifactCodec::
GetLastExecutionFailureDetail() const
{
	return LastExecutionFailureDetail;
}
