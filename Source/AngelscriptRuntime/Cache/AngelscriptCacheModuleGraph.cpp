#include "Cache/AngelscriptCacheDecodedRecord.h"

#include "Algo/Sort.h"

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptCacheModuleGraph, Log, All);

struct FAngelscriptCacheModuleGraphCandidateAccess
{
	using FTransaction = FAngelscriptCacheReadBudget::FDecodedCandidateTransaction;

	static FTransaction Begin(
		FAngelscriptCacheReadBudget& Budget,
		const FAngelscriptCacheReadLimits& Limits)
	{
		return Budget.BeginDecodedCandidateTransaction(Limits);
	}

	static EAngelscriptCacheCandidateChargeResult TryExtend(
		FTransaction& Transaction,
		const uint64 Bytes)
	{
		switch (Transaction.TryExtend(Bytes))
		{
		case FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult::Success:
			return EAngelscriptCacheCandidateChargeResult::Success;
		case FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult::BudgetExceeded:
			return EAngelscriptCacheCandidateChargeResult::BudgetExceeded;
		case FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult::Overflow:
			return EAngelscriptCacheCandidateChargeResult::Overflow;
		case FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult::InvalidState:
		default:
			return EAngelscriptCacheCandidateChargeResult::InvalidState;
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
	class FGraphCandidateChargeSink final
		: public IAngelscriptCacheCandidateChargeSink
	{
	public:
		explicit FGraphCandidateChargeSink(
			FAngelscriptCacheModuleGraphCandidateAccess::FTransaction& InTransaction)
			: Transaction(InTransaction)
		{
		}

		virtual EAngelscriptCacheCandidateChargeResult TryExtend(
			const uint64 RetainedCapacityBytes) override
		{
			return FAngelscriptCacheModuleGraphCandidateAccess::TryExtend(
				Transaction, RetainedCapacityBytes);
		}

	private:
		FAngelscriptCacheModuleGraphCandidateAccess::FTransaction& Transaction;
	};

	struct FRecordLookup
	{
		FAngelscriptCacheRecordId RecordId;
		uint32 InputOrdinal = 0;
	};

	struct FRequiredTypeDeclaration
	{
		FAngelscriptStableTypeKey TypeKey;
		uint32 DeclarationOrdinal = 0;
	};

	struct FFunctionDeclaration
	{
		FAngelscriptStableFunctionKey FunctionKey;
		uint32 DeclarationOrdinal = 0;
		EAngelscriptCacheBodyCoverage BodyCoverage =
			EAngelscriptCacheBodyCoverage::Invalid;
		EAngelscriptArtifactEntityKind EntityKind =
			static_cast<EAngelscriptArtifactEntityKind>(0);
		TOptional<uint32> BodyLinkOrdinal;
	};

	struct FGlobalDeclaration
	{
		FAngelscriptStableGlobalKey GlobalKey;
		uint32 DeclarationOrdinal = 0;
	};

	struct FPropertyDeclaration
	{
		FAngelscriptStablePropertyKey PropertyKey;
		uint32 DeclarationOrdinal = 0;
		TOptional<uint32> TypeSchemaRecordOrdinal;
		TOptional<uint32> PropertySchemaOrdinal;
	};

	struct FInitializerDeclaration
	{
		FAngelscriptStableFunctionKey InitializerKey;
		uint32 DeclarationOrdinal = 0;
		EAngelscriptArtifactEntityKind EntityKind =
			static_cast<EAngelscriptArtifactEntityKind>(0);
	};

	struct FLinkedFunctionBody
	{
		uint32 BodyInputOrdinal = 0;
		TOptional<uint32> DebugInputOrdinal;
		TOptional<uint32> DebugSequenceOrdinal;
		bool bDuplicateDebugOwner = false;
	};

	struct FDebugOwnerLookup
	{
		FAngelscriptCacheRecordId DebugRecordId;
		uint32 BodyLinkOrdinal = 0;
	};

	struct FCurrentSymbolMemo
	{
		EAngelscriptCacheReferenceKind ReferenceKind =
			EAngelscriptCacheReferenceKind::Invalid;
		FAngelscriptHash256 StableKey;
		FAngelscriptCacheCurrentSymbol Symbol;
	};

	struct FCurrentLayoutInputMemo
	{
		EAngelscriptCachedTypeLayoutInputKind InputKind =
			EAngelscriptCachedTypeLayoutInputKind::Invalid;
		EAngelscriptCacheReferenceKind ReferenceKind =
			EAngelscriptCacheReferenceKind::Invalid;
		FAngelscriptHash256 StableKey;
		FAngelscriptCacheResolvedTypeLayoutInput Layout;
	};

	struct FCurrentDataTypeLayoutMemo
	{
		const FAngelscriptCachedDataType* DataType = nullptr;
		EAngelscriptCachedPropertyStorageKind StorageKind =
			EAngelscriptCachedPropertyStorageKind::Invalid;
		FAngelscriptCacheResolvedDataTypeLayout Layout;
	};

	struct FLocalTypeLayoutEdge
	{
		uint32 TargetTypeOrdinal = 0;
		uint64 DiagnosticOffset = 0;
	};

	static FAngelscriptCacheValidationResult GraphFailure(
		const EAngelscriptCacheValidationError Error,
		const uint64 Offset = 0,
		const EAngelscriptCacheValidationStage Stage =
			EAngelscriptCacheValidationStage::ModuleGraph)
	{
		return FAngelscriptCacheValidationResult::AtStage(
			Error, EAngelscriptCacheRecordKind::ModuleSnapshot, Stage, Offset);
	}

	static bool PayloadsEqual(
		const FAngelscriptDecodedCacheRecord& Left,
		const FAngelscriptDecodedCacheRecord& Right)
	{
		const TConstArrayView<uint8> LeftBytes = Left.GetCanonicalPayload();
		const TConstArrayView<uint8> RightBytes = Right.GetCanonicalPayload();
		return LeftBytes.Num() == RightBytes.Num()
			&& (LeftBytes.IsEmpty()
				|| FMemory::Memcmp(
					LeftBytes.GetData(), RightBytes.GetData(), LeftBytes.Num()) == 0);
	}

	template <typename ElementType>
	static bool TryCalculateArrayReserveBytes(
		const int32 RequestedCapacity,
		int32& OutReservedCapacity,
		uint64& OutBytes)
	{
		OutReservedCapacity = 0;
		OutBytes = 0;
		if (RequestedCapacity <= 0)
		{
			return RequestedCapacity == 0;
		}

		using FArrayType = TArray<ElementType>;
		typename FArrayType::ElementAllocatorType Allocator;
		if constexpr (TAllocatorTraits<typename FArrayType::AllocatorType>::
			SupportsElementAlignment)
		{
			OutReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			OutReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType));
		}
		if (OutReservedCapacity < RequestedCapacity
			|| static_cast<uint64>(OutReservedCapacity)
				> MAX_uint64 / sizeof(ElementType))
		{
			OutReservedCapacity = 0;
			return false;
		}
		OutBytes = static_cast<uint64>(OutReservedCapacity) * sizeof(ElementType);
		return true;
	}

	static bool TryAddBytes(
		const uint64 Addend,
		uint64& InOutBytes)
	{
		if (InOutBytes > MAX_uint64 - Addend)
		{
			return false;
		}
		InOutBytes += Addend;
		return true;
	}

	template <typename ElementType>
	static bool TryAddArrayReserveBytes(
		const int32 RequestedCapacity,
		uint64& InOutBytes,
		int32& OutReservedCapacity,
		uint64& OutArrayBytes)
	{
		return TryCalculateArrayReserveBytes<ElementType>(
				RequestedCapacity, OutReservedCapacity, OutArrayBytes)
			&& TryAddBytes(OutArrayBytes, InOutBytes);
	}

	static const FRecordLookup* FindRecordLookup(
		const TArray<FRecordLookup>& Lookup,
		const FAngelscriptCacheRecordId& RecordId)
	{
		int32 First = 0;
		int32 Last = Lookup.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (Lookup[Middle].RecordId < RecordId)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return Lookup.IsValidIndex(First) && Lookup[First].RecordId == RecordId
			? &Lookup[First] : nullptr;
	}

	static uint64 SnapshotOffset(
		const FAngelscriptDecodedCacheRecord& SnapshotRecord,
		const EAngelscriptModuleSnapshotCapturedField Field,
		const uint32 PrimaryIndex = MAX_uint32)
	{
		return SnapshotRecord.FindCapturedOffset({Field, PrimaryIndex}).Get(0);
	}

	static uint64 InterfaceOffset(
		const FAngelscriptDecodedCacheRecord& InterfaceRecord,
		const EAngelscriptModuleInterfaceCapturedField Field,
		const uint32 PrimaryIndex = MAX_uint32)
	{
		return InterfaceRecord.FindCapturedOffset({Field, PrimaryIndex}).Get(0);
	}

	static uint64 TypeSchemaOffset(
		const FAngelscriptDecodedCacheRecord& TypeSchemaRecord,
		const EAngelscriptTypeSchemaCapturedField Field,
		const uint32 PrimaryIndex = MAX_uint32)
	{
		return TypeSchemaRecord.FindCapturedOffset({Field, PrimaryIndex}).Get(0);
	}

	static uint64 FunctionBodyOffset(
		const FAngelscriptDecodedCacheRecord& FunctionBodyRecord,
		const EAngelscriptFunctionBodyCapturedField Field,
		const uint32 PrimaryIndex = MAX_uint32)
	{
		return FunctionBodyRecord.FindCapturedOffset({Field, PrimaryIndex}).Get(0);
	}

	static uint64 ModuleStateOffset(
		const FAngelscriptDecodedCacheRecord& ModuleStateRecord,
		const EAngelscriptModuleStateCapturedField Field,
		const uint32 PrimaryIndex = MAX_uint32,
		const uint32 SecondaryIndex = MAX_uint32,
		const uint32 TertiaryIndex = MAX_uint32)
	{
		return ModuleStateRecord.FindCapturedOffset(
			{Field, PrimaryIndex, SecondaryIndex, TertiaryIndex}).Get(0);
	}

	static uint64 DebugSidecarOffset(
		const FAngelscriptDecodedCacheRecord& DebugSidecarRecord,
		const EAngelscriptDebugSidecarCapturedField Field,
		const uint32 PrimaryIndex = MAX_uint32)
	{
		return DebugSidecarRecord.FindCapturedOffset({Field, PrimaryIndex}).Get(0);
	}

	static bool DebugSourcesEqual(
		const TConstArrayView<FAngelscriptCachedDebugSourceReference> Left,
		const TConstArrayView<FAngelscriptCachedDebugSourceReference> Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!(Left[Index].SourceFileKey.Hash == Right[Index].SourceFileKey.Hash)
				|| !(Left[Index].LogicalSectionKey.Hash
					== Right[Index].LogicalSectionKey.Hash)
				|| Left[Index].CanonicalLogicalSection
					!= Right[Index].CanonicalLogicalSection)
			{
				return false;
			}
		}
		return true;
	}

	static int32 CompareHashes(
		const FAngelscriptHash256& Left,
		const FAngelscriptHash256& Right)
	{
		if (Left == Right)
		{
			return 0;
		}
		return Left < Right ? -1 : 1;
	}

	static int32 CompareRelocationToDependency(
		const FAngelscriptCacheRelocationUse& Relocation,
		const FAngelscriptCacheSemanticDependency& Dependency)
	{
		if (Relocation.DependencyKind != Dependency.Kind)
		{
			return static_cast<uint8>(Relocation.DependencyKind)
				< static_cast<uint8>(Dependency.Kind) ? -1 : 1;
		}
		if (Relocation.ReferenceKind != Dependency.Target.Kind)
		{
			return static_cast<uint8>(Relocation.ReferenceKind)
				< static_cast<uint8>(Dependency.Target.Kind) ? -1 : 1;
		}
		if (const int32 Key = CompareHashes(
			Relocation.StableKey, Dependency.Target.StableKey); Key != 0)
		{
			return Key;
		}
		if (const int32 Abi = CompareHashes(
			Relocation.ExpectedAbi, Dependency.Target.ExpectedAbi); Abi != 0)
		{
			return Abi;
		}
		if (Relocation.ExpectedContentOrValue.IsSet()
			!= Dependency.ExpectedContentOrValue.IsSet())
		{
			return Relocation.ExpectedContentOrValue.IsSet() ? 1 : -1;
		}
		return Relocation.ExpectedContentOrValue.IsSet()
			? CompareHashes(Relocation.ExpectedContentOrValue.GetValue(),
				Dependency.ExpectedContentOrValue.GetValue())
			: 0;
	}

	static bool RelocationsAreDependencySubset(
		const TConstArrayView<FAngelscriptCacheRelocationUse> Relocations,
		const TConstArrayView<FAngelscriptCacheSemanticDependency> Dependencies)
	{
		for (const FAngelscriptCacheRelocationUse& Relocation : Relocations)
		{
			int32 First = 0;
			int32 Last = Dependencies.Num();
			while (First < Last)
			{
				const int32 Middle = First + (Last - First) / 2;
				if (CompareRelocationToDependency(
					Relocation, Dependencies[Middle]) > 0)
				{
					First = Middle + 1;
				}
				else
				{
					Last = Middle;
				}
			}
			if (First >= Dependencies.Num()
				|| CompareRelocationToDependency(
					Relocation, Dependencies[First]) != 0)
			{
				return false;
			}
		}
		return true;
	}

	static int32 CompareCurrentSymbolMemoIdentity(
		const FCurrentSymbolMemo& Memo,
		const EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey)
	{
		if (Memo.ReferenceKind != ReferenceKind)
		{
			return static_cast<uint8>(Memo.ReferenceKind)
				< static_cast<uint8>(ReferenceKind) ? -1 : 1;
		}
		return CompareHashes(Memo.StableKey, StableKey);
	}

	static int32 LowerBoundCurrentSymbolMemo(
		const TArray<FCurrentSymbolMemo>& Memos,
		const EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey)
	{
		int32 First = 0;
		int32 Last = Memos.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (CompareCurrentSymbolMemoIdentity(
				Memos[Middle], ReferenceKind, StableKey) < 0)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return First;
	}

	static int32 CompareCurrentLayoutInputMemoIdentity(
		const FCurrentLayoutInputMemo& Memo,
		const EAngelscriptCachedTypeLayoutInputKind InputKind,
		const EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey)
	{
		if (Memo.InputKind != InputKind)
		{
			return static_cast<uint8>(Memo.InputKind)
				< static_cast<uint8>(InputKind) ? -1 : 1;
		}
		if (Memo.ReferenceKind != ReferenceKind)
		{
			return static_cast<uint8>(Memo.ReferenceKind)
				< static_cast<uint8>(ReferenceKind) ? -1 : 1;
		}
		return CompareHashes(Memo.StableKey, StableKey);
	}

	static int32 LowerBoundCurrentLayoutInputMemo(
		const TArray<FCurrentLayoutInputMemo>& Memos,
		const EAngelscriptCachedTypeLayoutInputKind InputKind,
		const EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey)
	{
		int32 First = 0;
		int32 Last = Memos.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (CompareCurrentLayoutInputMemoIdentity(
				Memos[Middle], InputKind, ReferenceKind, StableKey) < 0)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return First;
	}

	static bool IsPowerOfTwo(const uint32 Value)
	{
		return Value != 0 && (Value & (Value - 1u)) == 0;
	}

	class FProspectiveTypeLayoutView final
		: public IAngelscriptCacheProspectiveTypeLayoutView
	{
	public:
		explicit FProspectiveTypeLayoutView(
			const FAngelscriptValidatedModuleGraph& InGraph)
			: Graph(InGraph)
		{
		}

		virtual TOptional<FAngelscriptCacheProspectiveTypeLayout>
		FindLocalScriptTypeLayout(
			const FAngelscriptStableTypeKey& TypeKey) const override
		{
			const TConstArrayView<FAngelscriptCacheValidatedTypeOrdinal> Types =
				Graph.GetTypeOrdinals();
			int32 First = 0;
			int32 Last = Types.Num();
			while (First < Last)
			{
				const int32 Middle = First + (Last - First) / 2;
				if (Types[Middle].TypeKey.Hash < TypeKey.Hash)
				{
					First = Middle + 1;
				}
				else
				{
					Last = Middle;
				}
			}
			if (!Types.IsValidIndex(First)
				|| !(Types[First].TypeKey == TypeKey))
			{
				return {};
			}

			const TConstArrayView<FAngelscriptDecodedCacheRecordHandle> Records =
				Graph.GetReachableRecords();
			if (!Records.IsValidIndex(
				static_cast<int32>(Types[First].TypeSchemaRecordOrdinal)))
			{
				return {};
			}
			const FAngelscriptCachedTypeSchema* Type = Records[
				Types[First].TypeSchemaRecordOrdinal]->TryGetTypeSchema();
			if (Type == nullptr)
			{
				return {};
			}
			return FAngelscriptCacheProspectiveTypeLayout{
				Type->TypeKind, Type->Layout.SemanticSize,
				Type->Layout.SemanticAlignment};
		}

	private:
		const FAngelscriptValidatedModuleGraph& Graph;
	};

	static bool IsSelectedModuleReference(
		const FAngelscriptCachedModuleInterface& Interface,
		const FAngelscriptCacheStableReference& Reference)
	{
		switch (Reference.Kind)
		{
		case EAngelscriptCacheReferenceKind::ScriptModule:
			return Reference.StableKey == Interface.ModuleKey.Hash;
		case EAngelscriptCacheReferenceKind::ScriptImport:
			for (const FAngelscriptCachedImportDeclaration& Import : Interface.Imports)
			{
				if (Import.ImportKey.Hash == Reference.StableKey)
				{
					return true;
				}
			}
			return false;
		case EAngelscriptCacheReferenceKind::ScriptType:
		case EAngelscriptCacheReferenceKind::ScriptFunction:
		case EAngelscriptCacheReferenceKind::ScriptGlobal:
		case EAngelscriptCacheReferenceKind::ScriptProperty:
			for (const FAngelscriptCachedDeclaration& Declaration :
				Interface.Declarations)
			{
				if (Declaration.StableKey == Reference.StableKey)
				{
					return true;
				}
			}
			return false;
		default:
			return false;
		}
	}

	static const FFunctionDeclaration* FindFunctionDeclaration(
		const TArray<FFunctionDeclaration>& Declarations,
		const FAngelscriptHash256& StableKey)
	{
		int32 First = 0;
		int32 Last = Declarations.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (Declarations[Middle].FunctionKey.Hash < StableKey)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return Declarations.IsValidIndex(First)
			&& Declarations[First].FunctionKey.Hash == StableKey
			? &Declarations[First] : nullptr;
	}

	static const FRequiredTypeDeclaration* FindRequiredTypeDeclaration(
		const TArray<FRequiredTypeDeclaration>& Declarations,
		const FAngelscriptHash256& StableKey)
	{
		int32 First = 0;
		int32 Last = Declarations.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (Declarations[Middle].TypeKey.Hash < StableKey)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return Declarations.IsValidIndex(First)
			&& Declarations[First].TypeKey.Hash == StableKey
			? &Declarations[First] : nullptr;
	}

	static const FGlobalDeclaration* FindGlobalDeclaration(
		const TArray<FGlobalDeclaration>& Declarations,
		const FAngelscriptHash256& StableKey)
	{
		int32 First = 0;
		int32 Last = Declarations.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (Declarations[Middle].GlobalKey.Hash < StableKey)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return Declarations.IsValidIndex(First)
			&& Declarations[First].GlobalKey.Hash == StableKey
			? &Declarations[First] : nullptr;
	}

	static FPropertyDeclaration* FindPropertyDeclaration(
		TArray<FPropertyDeclaration>& Declarations,
		const FAngelscriptStablePropertyKey& PropertyKey)
	{
		int32 First = 0;
		int32 Last = Declarations.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (Declarations[Middle].PropertyKey.Hash < PropertyKey.Hash)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return Declarations.IsValidIndex(First)
			&& Declarations[First].PropertyKey == PropertyKey
			? &Declarations[First] : nullptr;
	}

	static bool SourceIndexContainsFile(
		const FAngelscriptCachedSourceIndex& SourceIndex,
		const FAngelscriptCachedSourceFileKey& SourceFileKey)
	{
		// SourceIndex.Files has its wire-canonical source-authority order
		// (kind/mount/provider/path/key), not a SourceFileKey-primary order.
		// Keep this bounded graph lookup allocation-free and match the actual
		// authority instead of binary-searching with an incompatible comparator.
		for (const FAngelscriptCachedSourceFile& File : SourceIndex.Files)
		{
			if (File.SourceFileKey.Hash == SourceFileKey.Hash)
			{
				return true;
			}
		}
		return false;
	}

	static bool DataTypesEqual(
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
			if (!DataTypesEqual(
				Left.OrderedSubTypes[Index], Right.OrderedSubTypes[Index]))
			{
				return false;
			}
		}
		return true;
	}

	static int32 CompareDataTypes(
		const FAngelscriptCachedDataType& Left,
		const FAngelscriptCachedDataType& Right)
	{
		if (Left.Kind != Right.Kind)
		{
			return static_cast<uint8>(Left.Kind)
				< static_cast<uint8>(Right.Kind) ? -1 : 1;
		}
		if (Left.Primitive != Right.Primitive)
		{
			return static_cast<uint8>(Left.Primitive)
				< static_cast<uint8>(Right.Primitive) ? -1 : 1;
		}
		if (Left.TypeReference.IsSet() != Right.TypeReference.IsSet())
		{
			return Left.TypeReference.IsSet() ? 1 : -1;
		}
		if (Left.TypeReference.IsSet())
		{
			const FAngelscriptCacheStableReference& LeftReference =
				Left.TypeReference.GetValue();
			const FAngelscriptCacheStableReference& RightReference =
				Right.TypeReference.GetValue();
			if (LeftReference.Kind != RightReference.Kind)
			{
				return static_cast<uint8>(LeftReference.Kind)
					< static_cast<uint8>(RightReference.Kind) ? -1 : 1;
			}
			if (const int32 Key = CompareHashes(
				LeftReference.StableKey, RightReference.StableKey); Key != 0)
			{
				return Key;
			}
			if (const int32 Abi = CompareHashes(
				LeftReference.ExpectedAbi, RightReference.ExpectedAbi); Abi != 0)
			{
				return Abi;
			}
		}
		if (Left.QualifierFlags != Right.QualifierFlags)
		{
			return Left.QualifierFlags < Right.QualifierFlags ? -1 : 1;
		}
		const int32 CommonSubTypeCount = FMath::Min(
			Left.OrderedSubTypes.Num(), Right.OrderedSubTypes.Num());
		for (int32 Index = 0; Index < CommonSubTypeCount; ++Index)
		{
			if (const int32 SubType = CompareDataTypes(
				Left.OrderedSubTypes[Index], Right.OrderedSubTypes[Index]);
				SubType != 0)
			{
				return SubType;
			}
		}
		if (Left.OrderedSubTypes.Num() == Right.OrderedSubTypes.Num())
		{
			return 0;
		}
		return Left.OrderedSubTypes.Num() < Right.OrderedSubTypes.Num() ? -1 : 1;
	}

	static int32 CompareCurrentDataTypeLayoutMemoIdentity(
		const FCurrentDataTypeLayoutMemo& Memo,
		const FAngelscriptCachedDataType& DataType,
		const EAngelscriptCachedPropertyStorageKind StorageKind)
	{
		check(Memo.DataType != nullptr);
		if (const int32 TypeOrder = CompareDataTypes(*Memo.DataType, DataType);
			TypeOrder != 0)
		{
			return TypeOrder;
		}
		if (Memo.StorageKind == StorageKind)
		{
			return 0;
		}
		return static_cast<uint8>(Memo.StorageKind)
			< static_cast<uint8>(StorageKind) ? -1 : 1;
	}

	static int32 LowerBoundCurrentDataTypeLayoutMemo(
		const TArray<FCurrentDataTypeLayoutMemo>& Memos,
		const FAngelscriptCachedDataType& DataType,
		const EAngelscriptCachedPropertyStorageKind StorageKind)
	{
		int32 First = 0;
		int32 Last = Memos.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (CompareCurrentDataTypeLayoutMemoIdentity(
				Memos[Middle], DataType, StorageKind) < 0)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return First;
	}

	static bool MetadataEqual(
		const TConstArrayView<FAngelscriptCachedMetadataEntry> Left,
		const TConstArrayView<FAngelscriptCachedMetadataEntry> Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].CanonicalKey != Right[Index].CanonicalKey
				|| Left[Index].CanonicalValue != Right[Index].CanonicalValue)
			{
				return false;
			}
		}
		return true;
	}

	static TOptional<EAngelscriptCachedFunctionInvocationKind>
	InvocationKindForEntity(const EAngelscriptArtifactEntityKind EntityKind)
	{
		switch (EntityKind)
		{
		case EAngelscriptArtifactEntityKind::GlobalFunction:
			return EAngelscriptCachedFunctionInvocationKind::GlobalFunction;
		case EAngelscriptArtifactEntityKind::Method:
			return EAngelscriptCachedFunctionInvocationKind::Method;
		case EAngelscriptArtifactEntityKind::Constructor:
			return EAngelscriptCachedFunctionInvocationKind::Constructor;
		case EAngelscriptArtifactEntityKind::Destructor:
			return EAngelscriptCachedFunctionInvocationKind::Destructor;
		case EAngelscriptArtifactEntityKind::Factory:
			return EAngelscriptCachedFunctionInvocationKind::Factory;
		case EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor:
			return EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultConstructor;
		case EAngelscriptArtifactEntityKind::GeneratedDefaultDestructor:
			return EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultDestructor;
		case EAngelscriptArtifactEntityKind::InitDefaults:
			return EAngelscriptCachedFunctionInvocationKind::InitDefaults;
		default:
			return {};
		}
	}

	static FAngelscriptCacheValidationResult CandidateChargeFailure(
		const EAngelscriptCacheCandidateChargeResult Result)
	{
		return GraphFailure(
			Result == EAngelscriptCacheCandidateChargeResult::BudgetExceeded
				? EAngelscriptCacheValidationError::BudgetExceeded
				: EAngelscriptCacheValidationError::Overflow);
	}

	static TOptional<EAngelscriptCachedTypeKind> TypeKindForEntity(
		const EAngelscriptArtifactEntityKind EntityKind)
	{
		switch (EntityKind)
		{
		case EAngelscriptArtifactEntityKind::Class:
			return EAngelscriptCachedTypeKind::Class;
		case EAngelscriptArtifactEntityKind::Struct:
			return EAngelscriptCachedTypeKind::Struct;
		case EAngelscriptArtifactEntityKind::Interface:
			return EAngelscriptCachedTypeKind::Interface;
		case EAngelscriptArtifactEntityKind::Enum:
			return EAngelscriptCachedTypeKind::Enum;
		case EAngelscriptArtifactEntityKind::Delegate:
			return EAngelscriptCachedTypeKind::Delegate;
		case EAngelscriptArtifactEntityKind::Typedef:
			return EAngelscriptCachedTypeKind::Typedef;
		case EAngelscriptArtifactEntityKind::Funcdef:
			return EAngelscriptCachedTypeKind::Funcdef;
		default:
			return {};
		}
	}
}

void FAngelscriptValidatedModuleGraph::Reset()
{
	ModuleKey = {};
	ReachableRecords.Empty();
	RecordOrdinals.Empty();
	TypeOrdinals.Empty();
	GlobalOrdinals.Empty();
	FunctionOrdinals.Empty();
	InitializerOrdinals.Empty();
	OpaqueSummaries.Empty();
	OpaqueOwnerOrdinals.Empty();
}

TOptional<uint32> FAngelscriptValidatedModuleGraph::FindRecordOrdinal(
	const FAngelscriptCacheRecordId& RecordId) const
{
	int32 First = 0;
	int32 Last = RecordOrdinals.Num();
	while (First < Last)
	{
		const int32 Middle = First + (Last - First) / 2;
		if (RecordOrdinals[Middle].RecordId < RecordId)
		{
			First = Middle + 1;
		}
		else
		{
			Last = Middle;
		}
	}
	if (!RecordOrdinals.IsValidIndex(First)
		|| !(RecordOrdinals[First].RecordId == RecordId))
	{
		return {};
	}
	return RecordOrdinals[First].RecordOrdinal;
}

FAngelscriptCacheValidationResult ValidateModuleSnapshotGraph(
	const FAngelscriptCacheRecordId& ModuleSnapshotRecordId,
	const TConstArrayView<FAngelscriptDecodedCacheRecordHandle> LocallyValidatedRecords,
	const FAngelscriptCacheModuleGraphValidationContext& Context,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FAngelscriptValidatedModuleGraph& OutGraph)
{
	OutGraph.Reset();

	const FAngelscriptCachedSourceIndex* SourceIndex =
		Context.SourceIndex != nullptr
			? Context.SourceIndex->TryGetSourceIndex() : nullptr;
	if (ModuleSnapshotRecordId.Kind != EAngelscriptCacheRecordKind::ModuleSnapshot
		|| ModuleSnapshotRecordId.ContentHash.IsZero()
		|| Context.SelectedProfile.Hash.IsZero()
		|| Context.SelectedSourceSnapshot.IsZero()
		|| SourceIndex == nullptr
		|| Context.CurrentSymbols == nullptr
		|| Context.CurrentLayouts == nullptr
		|| Context.OpaquePayloads == nullptr)
	{
		return GraphFailure(EAngelscriptCacheValidationError::ContextMismatch);
	}
	if (LocallyValidatedRecords.Num() < 0
		|| (LocallyValidatedRecords.Num() != 0
			&& LocallyValidatedRecords.GetData() == nullptr))
	{
		return GraphFailure(EAngelscriptCacheValidationError::InvalidArrayView);
	}
	if (static_cast<uint64>(LocallyValidatedRecords.Num()) > Limits.MaxArrayElements)
	{
		return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
	}

	int32 LookupCapacity = 0;
	uint64 LookupBytes = 0;
	if (!TryCalculateArrayReserveBytes<FRecordLookup>(
		LocallyValidatedRecords.Num(), LookupCapacity, LookupBytes))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	FAngelscriptCacheTemporaryResidentReservation LookupReservation;
	if (!Budget.TryReserveTemporaryDecoded(LookupBytes, Limits, LookupReservation))
	{
		return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
	}

	TArray<FRecordLookup> Lookup;
	Lookup.Reserve(LookupCapacity);
	if (static_cast<uint64>(Lookup.GetAllocatedSize()) != LookupBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	for (int32 InputOrdinal = 0;
		InputOrdinal < LocallyValidatedRecords.Num(); ++InputOrdinal)
	{
		Lookup.Add({
			LocallyValidatedRecords[InputOrdinal]->GetRecordId(),
			static_cast<uint32>(InputOrdinal)});
	}
	Lookup.Sort([](const FRecordLookup& Left, const FRecordLookup& Right)
	{
		return Left.RecordId < Right.RecordId;
	});
	for (int32 Index = 1; Index < Lookup.Num(); ++Index)
	{
		if (!(Lookup[Index - 1].RecordId == Lookup[Index].RecordId))
		{
			continue;
		}
		const FAngelscriptDecodedCacheRecord& Left =
			LocallyValidatedRecords[Lookup[Index - 1].InputOrdinal].Get();
		const FAngelscriptDecodedCacheRecord& Right =
			LocallyValidatedRecords[Lookup[Index].InputOrdinal].Get();
		return GraphFailure(PayloadsEqual(Left, Right)
			? EAngelscriptCacheValidationError::DuplicateKey
			: EAngelscriptCacheValidationError::ConflictingKey);
	}

	const FRecordLookup* RootLookup = FindRecordLookup(Lookup, ModuleSnapshotRecordId);
	if (RootLookup == nullptr)
	{
		return GraphFailure(EAngelscriptCacheValidationError::MissingRecord);
	}
	const FAngelscriptDecodedCacheRecordHandle& RootRecord =
		LocallyValidatedRecords[RootLookup->InputOrdinal];
	const FAngelscriptCachedModuleSnapshot* Snapshot = RootRecord->TryGetModuleSnapshot();
	if (Snapshot == nullptr)
	{
		return GraphFailure(EAngelscriptCacheValidationError::WrongRecordKind);
	}

	const FRecordLookup* InterfaceLookup =
		FindRecordLookup(Lookup, Snapshot->ModuleInterface.RecordId);
	if (InterfaceLookup == nullptr)
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::MissingRecord,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordId));
	}
	const FAngelscriptDecodedCacheRecordHandle& InterfaceRecord =
		LocallyValidatedRecords[InterfaceLookup->InputOrdinal];
	const FAngelscriptCachedModuleInterface* Interface =
		InterfaceRecord->TryGetModuleInterface();
	if (Interface == nullptr)
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::WrongRecordKind,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::ModuleInterfaceRecordId));
	}

	const FRecordLookup* StateLookup =
		FindRecordLookup(Lookup, Snapshot->ModuleState.RecordId);
	if (StateLookup == nullptr)
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::MissingRecord,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordId));
	}
	const FAngelscriptDecodedCacheRecordHandle& StateRecord =
		LocallyValidatedRecords[StateLookup->InputOrdinal];
	const FAngelscriptCachedModuleState* State = StateRecord->TryGetModuleState();
	if (State == nullptr)
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::WrongRecordKind,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::ModuleStateRecordId));
	}

	// Step 1 follows every linked child before declaration/owner coverage so a
	// missing child wins over a later set mismatch. Input ordinals are retained
	// only as budgeted validation scratch; the published graph owns handles.
	int32 RequiredTypeCount = 0;
	int32 FunctionDeclarationCount = 0;
	int32 GlobalDeclarationCount = 0;
	int32 PropertyDeclarationCount = 0;
	int32 InitializerDeclarationCount = 0;
	for (const FAngelscriptCachedDeclaration& Declaration : Interface->Declarations)
	{
		if (Declaration.DeclarationKind == EAngelscriptCacheDeclarationKind::Type
			&& Declaration.SchemaCoverage == EAngelscriptCacheSchemaCoverage::Required)
		{
			++RequiredTypeCount;
		}
		else if (Declaration.DeclarationKind
			== EAngelscriptCacheDeclarationKind::Function)
		{
			++FunctionDeclarationCount;
			if (Declaration.EntityKind
					== EAngelscriptArtifactEntityKind::ModuleInitializer
				|| Declaration.EntityKind
					== EAngelscriptArtifactEntityKind::GlobalInitializer)
			{
				++InitializerDeclarationCount;
			}
		}
		else if (Declaration.DeclarationKind
			== EAngelscriptCacheDeclarationKind::Global)
		{
			++GlobalDeclarationCount;
		}
		else if (Declaration.DeclarationKind
			== EAngelscriptCacheDeclarationKind::Property)
		{
			++PropertyDeclarationCount;
		}
	}
	int32 RequiredTypeCapacity = 0;
	int32 TypeInputOrdinalCapacity = 0;
	int32 FunctionDeclarationCapacity = 0;
	int32 GlobalDeclarationCapacity = 0;
	int32 PropertyDeclarationCapacity = 0;
	int32 InitializerDeclarationCapacity = 0;
	int32 ExecuteActionOrdinalCapacity = 0;
	int32 LinkedFunctionBodyCapacity = 0;
	uint64 RequiredTypeBytes = 0;
	uint64 TypeInputOrdinalBytes = 0;
	uint64 FunctionDeclarationBytes = 0;
	uint64 GlobalDeclarationBytes = 0;
	uint64 PropertyDeclarationBytes = 0;
	uint64 InitializerDeclarationBytes = 0;
	uint64 ExecuteActionOrdinalBytes = 0;
	uint64 LinkedFunctionBodyBytes = 0;
	uint64 GraphScratchBytes = 0;
	if (!TryAddArrayReserveBytes<FRequiredTypeDeclaration>(
			RequiredTypeCount, GraphScratchBytes,
			RequiredTypeCapacity, RequiredTypeBytes)
		|| !TryAddArrayReserveBytes<uint32>(
			Snapshot->TypeSchemas.Num(), GraphScratchBytes,
			TypeInputOrdinalCapacity, TypeInputOrdinalBytes)
		|| !TryAddArrayReserveBytes<FFunctionDeclaration>(
			FunctionDeclarationCount, GraphScratchBytes,
			FunctionDeclarationCapacity, FunctionDeclarationBytes)
		|| !TryAddArrayReserveBytes<FGlobalDeclaration>(
			GlobalDeclarationCount, GraphScratchBytes,
			GlobalDeclarationCapacity, GlobalDeclarationBytes)
		|| !TryAddArrayReserveBytes<FPropertyDeclaration>(
			PropertyDeclarationCount, GraphScratchBytes,
			PropertyDeclarationCapacity, PropertyDeclarationBytes)
		|| !TryAddArrayReserveBytes<FInitializerDeclaration>(
			InitializerDeclarationCount, GraphScratchBytes,
			InitializerDeclarationCapacity, InitializerDeclarationBytes)
		|| !TryAddArrayReserveBytes<uint32>(
			State->Initializers.Num(), GraphScratchBytes,
			ExecuteActionOrdinalCapacity, ExecuteActionOrdinalBytes)
		|| !TryAddArrayReserveBytes<FLinkedFunctionBody>(
			Snapshot->FunctionBodies.Num(), GraphScratchBytes,
			LinkedFunctionBodyCapacity, LinkedFunctionBodyBytes))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	FAngelscriptCacheTemporaryResidentReservation GraphScratchReservation;
	if (!Budget.TryReserveTemporaryDecoded(
		GraphScratchBytes, Limits, GraphScratchReservation))
	{
		return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
	}

	TArray<FRequiredTypeDeclaration> RequiredTypes;
	TArray<uint32> TypeInputOrdinals;
	TArray<FFunctionDeclaration> FunctionDeclarations;
	TArray<FGlobalDeclaration> GlobalDeclarations;
	TArray<FPropertyDeclaration> PropertyDeclarations;
	TArray<FInitializerDeclaration> InitializerDeclarations;
	TArray<uint32> ExecuteActionOrdinalsByUnit;
	TArray<FLinkedFunctionBody> LinkedFunctionBodies;
	RequiredTypes.Reserve(RequiredTypeCapacity);
	TypeInputOrdinals.Reserve(TypeInputOrdinalCapacity);
	FunctionDeclarations.Reserve(FunctionDeclarationCapacity);
	GlobalDeclarations.Reserve(GlobalDeclarationCapacity);
	PropertyDeclarations.Reserve(PropertyDeclarationCapacity);
	InitializerDeclarations.Reserve(InitializerDeclarationCapacity);
	ExecuteActionOrdinalsByUnit.Reserve(ExecuteActionOrdinalCapacity);
	LinkedFunctionBodies.Reserve(LinkedFunctionBodyCapacity);
	if (static_cast<uint64>(RequiredTypes.GetAllocatedSize()) != RequiredTypeBytes
		|| static_cast<uint64>(TypeInputOrdinals.GetAllocatedSize())
			!= TypeInputOrdinalBytes
		|| static_cast<uint64>(FunctionDeclarations.GetAllocatedSize())
			!= FunctionDeclarationBytes
		|| static_cast<uint64>(GlobalDeclarations.GetAllocatedSize())
			!= GlobalDeclarationBytes
		|| static_cast<uint64>(PropertyDeclarations.GetAllocatedSize())
			!= PropertyDeclarationBytes
		|| static_cast<uint64>(InitializerDeclarations.GetAllocatedSize())
			!= InitializerDeclarationBytes
		|| static_cast<uint64>(ExecuteActionOrdinalsByUnit.GetAllocatedSize())
			!= ExecuteActionOrdinalBytes
		|| static_cast<uint64>(LinkedFunctionBodies.GetAllocatedSize())
			!= LinkedFunctionBodyBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	for (int32 DeclarationOrdinal = 0;
		DeclarationOrdinal < Interface->Declarations.Num(); ++DeclarationOrdinal)
	{
		const FAngelscriptCachedDeclaration& Declaration =
			Interface->Declarations[DeclarationOrdinal];
		if (Declaration.DeclarationKind == EAngelscriptCacheDeclarationKind::Type
			&& Declaration.SchemaCoverage == EAngelscriptCacheSchemaCoverage::Required)
		{
			RequiredTypes.Add({FAngelscriptStableTypeKey{Declaration.StableKey},
				static_cast<uint32>(DeclarationOrdinal)});
		}
		else if (Declaration.DeclarationKind
			== EAngelscriptCacheDeclarationKind::Function)
		{
			FunctionDeclarations.Add({
				FAngelscriptStableFunctionKey{Declaration.StableKey},
				static_cast<uint32>(DeclarationOrdinal),
				Declaration.BodyCoverage,
				Declaration.EntityKind,
				{}});
			if (Declaration.EntityKind
					== EAngelscriptArtifactEntityKind::ModuleInitializer
				|| Declaration.EntityKind
					== EAngelscriptArtifactEntityKind::GlobalInitializer)
			{
				InitializerDeclarations.Add({
					FAngelscriptStableFunctionKey{Declaration.StableKey},
					static_cast<uint32>(DeclarationOrdinal),
					Declaration.EntityKind});
			}
		}
		else if (Declaration.DeclarationKind
			== EAngelscriptCacheDeclarationKind::Global)
		{
			GlobalDeclarations.Add({
				FAngelscriptStableGlobalKey{Declaration.StableKey},
				static_cast<uint32>(DeclarationOrdinal)});
		}
		else if (Declaration.DeclarationKind
			== EAngelscriptCacheDeclarationKind::Property)
		{
			PropertyDeclarations.Add({
				FAngelscriptStablePropertyKey{Declaration.StableKey},
				static_cast<uint32>(DeclarationOrdinal)});
		}
	}
	RequiredTypes.Sort([](
		const FRequiredTypeDeclaration& Left,
		const FRequiredTypeDeclaration& Right)
	{
		return Left.TypeKey.Hash < Right.TypeKey.Hash;
	});
	FunctionDeclarations.Sort([](
		const FFunctionDeclaration& Left,
		const FFunctionDeclaration& Right)
	{
		return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
	});
	GlobalDeclarations.Sort([](
		const FGlobalDeclaration& Left, const FGlobalDeclaration& Right)
	{
		return Left.GlobalKey.Hash < Right.GlobalKey.Hash;
	});
	PropertyDeclarations.Sort([](
		const FPropertyDeclaration& Left,
		const FPropertyDeclaration& Right)
	{
		return Left.PropertyKey.Hash < Right.PropertyKey.Hash;
	});
	InitializerDeclarations.Sort([](
		const FInitializerDeclaration& Left,
		const FInitializerDeclaration& Right)
	{
		return Left.InitializerKey.Hash < Right.InitializerKey.Hash;
	});
	for (int32 UnitOrdinal = 0;
		UnitOrdinal < State->Initializers.Num(); ++UnitOrdinal)
	{
		ExecuteActionOrdinalsByUnit.Add(MAX_uint32);
	}

	// Initializer units are a canonical stable-key set, while actions are the
	// sole execution-order authority. Build the one-to-one action map once so
	// codec traversal and all later ownership checks remain indexed.
	const auto FindInitializerUnitOrdinal = [State](
		const FAngelscriptHash256& InitializerKey) -> TOptional<uint32>
	{
		int32 First = 0;
		int32 Last = State->Initializers.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (State->Initializers[Middle].InitializerKey.Hash < InitializerKey)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		if (!State->Initializers.IsValidIndex(First)
			|| !(State->Initializers[First].InitializerKey.Hash
				== InitializerKey))
		{
			return {};
		}
		return static_cast<uint32>(First);
	};
	for (int32 ActionOrdinal = 0;
		ActionOrdinal < State->OrderedInitializationActions.Num();
		++ActionOrdinal)
	{
		const FAngelscriptCachedInitializationAction& Action =
			State->OrderedInitializationActions[ActionOrdinal];
		if (Action.ActionKind
			!= EAngelscriptCachedInitializationActionKind::ExecuteInitializer)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::MissingCoverage,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::InitializationActionKind,
					static_cast<uint32>(ActionOrdinal)));
		}
		if (!Action.Dependencies.IsEmpty())
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::MissingCoverage,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::InitializationActionDependencies,
					static_cast<uint32>(ActionOrdinal)));
		}
		const TOptional<uint32> UnitOrdinal =
			FindInitializerUnitOrdinal(Action.Target.StableKey);
		if (!UnitOrdinal.IsSet()
			|| ExecuteActionOrdinalsByUnit[UnitOrdinal.GetValue()] != MAX_uint32)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::
						InitializationActionTargetStableKey,
					static_cast<uint32>(ActionOrdinal)));
		}
		ExecuteActionOrdinalsByUnit[UnitOrdinal.GetValue()] =
			static_cast<uint32>(ActionOrdinal);
	}
	for (int32 UnitOrdinal = 0;
		UnitOrdinal < ExecuteActionOrdinalsByUnit.Num(); ++UnitOrdinal)
	{
		if (ExecuteActionOrdinalsByUnit[UnitOrdinal] == MAX_uint32)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::Initializer,
					static_cast<uint32>(UnitOrdinal)));
		}
	}

	for (int32 LinkOrdinal = 0;
		LinkOrdinal < Snapshot->TypeSchemas.Num(); ++LinkOrdinal)
	{
		const FAngelscriptCachedTypeSchemaLink& Link =
			Snapshot->TypeSchemas[LinkOrdinal];
		const FRecordLookup* TypeLookup = FindRecordLookup(Lookup, Link.RecordId);
		if (TypeLookup == nullptr)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::MissingRecord,
				SnapshotOffset(*RootRecord,
					EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordId,
					static_cast<uint32>(LinkOrdinal)));
		}
		const FAngelscriptDecodedCacheRecordHandle& TypeRecord =
			LocallyValidatedRecords[TypeLookup->InputOrdinal];
		if (TypeRecord->TryGetTypeSchema() == nullptr)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::WrongRecordKind,
				SnapshotOffset(*RootRecord,
					EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkRecordId,
					static_cast<uint32>(LinkOrdinal)));
		}
		TypeInputOrdinals.Add(TypeLookup->InputOrdinal);
	}

	int32 DebugSidecarCount = 0;
	for (int32 BodyLinkOrdinal = 0;
		BodyLinkOrdinal < Snapshot->FunctionBodies.Num(); ++BodyLinkOrdinal)
	{
		const FAngelscriptCachedFunctionBodyLink& Link =
			Snapshot->FunctionBodies[BodyLinkOrdinal];
		const FRecordLookup* BodyLookup = FindRecordLookup(Lookup, Link.RecordId);
		if (BodyLookup == nullptr)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::MissingRecord,
				SnapshotOffset(*RootRecord,
					EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordId,
					static_cast<uint32>(BodyLinkOrdinal)));
		}
		const FAngelscriptDecodedCacheRecordHandle& BodyRecord =
			LocallyValidatedRecords[BodyLookup->InputOrdinal];
		const FAngelscriptCachedFunctionBody* Body = BodyRecord->TryGetFunctionBody();
		if (Body == nullptr)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::WrongRecordKind,
				SnapshotOffset(*RootRecord,
					EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkRecordId,
					static_cast<uint32>(BodyLinkOrdinal)));
		}

		FLinkedFunctionBody& Linked = LinkedFunctionBodies.AddDefaulted_GetRef();
		Linked.BodyInputOrdinal = BodyLookup->InputOrdinal;
		if (Body->DebugSidecar.IsSet())
		{
			const FRecordLookup* DebugLookup =
				FindRecordLookup(Lookup, Body->DebugSidecar.GetValue());
			if (DebugLookup == nullptr)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::MissingRecord,
					FunctionBodyOffset(*BodyRecord,
						EAngelscriptFunctionBodyCapturedField::DebugSidecar));
			}
			const FAngelscriptDecodedCacheRecordHandle& DebugRecord =
				LocallyValidatedRecords[DebugLookup->InputOrdinal];
			if (DebugRecord->TryGetDebugSidecar() == nullptr)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongRecordKind,
					FunctionBodyOffset(*BodyRecord,
						EAngelscriptFunctionBodyCapturedField::DebugSidecar));
			}
			Linked.DebugInputOrdinal = DebugLookup->InputOrdinal;
			++DebugSidecarCount;
		}
	}

	int32 DebugOwnerLookupCapacity = 0;
	uint64 DebugOwnerLookupBytes = 0;
	if (!TryCalculateArrayReserveBytes<FDebugOwnerLookup>(
		DebugSidecarCount, DebugOwnerLookupCapacity, DebugOwnerLookupBytes))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	FAngelscriptCacheTemporaryResidentReservation DebugOwnerLookupReservation;
	if (!Budget.TryReserveTemporaryDecoded(
		DebugOwnerLookupBytes, Limits, DebugOwnerLookupReservation))
	{
		return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
	}
	TArray<FDebugOwnerLookup> DebugOwnerLookup;
	DebugOwnerLookup.Reserve(DebugOwnerLookupCapacity);
	if (static_cast<uint64>(DebugOwnerLookup.GetAllocatedSize())
		!= DebugOwnerLookupBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	for (int32 BodyLinkOrdinal = 0;
		BodyLinkOrdinal < LinkedFunctionBodies.Num(); ++BodyLinkOrdinal)
	{
		const FLinkedFunctionBody& Linked =
			LinkedFunctionBodies[BodyLinkOrdinal];
		if (!Linked.DebugInputOrdinal.IsSet())
		{
			continue;
		}
		DebugOwnerLookup.Add({
			LocallyValidatedRecords[Linked.DebugInputOrdinal.GetValue()]
				->GetRecordId(),
			static_cast<uint32>(BodyLinkOrdinal)});
	}
	DebugOwnerLookup.Sort([](
		const FDebugOwnerLookup& Left, const FDebugOwnerLookup& Right)
	{
		if (!(Left.DebugRecordId == Right.DebugRecordId))
		{
			return Left.DebugRecordId < Right.DebugRecordId;
		}
		return Left.BodyLinkOrdinal < Right.BodyLinkOrdinal;
	});
	for (int32 LookupOrdinal = 1;
		LookupOrdinal < DebugOwnerLookup.Num(); ++LookupOrdinal)
	{
		if (DebugOwnerLookup[LookupOrdinal - 1].DebugRecordId
			== DebugOwnerLookup[LookupOrdinal].DebugRecordId)
		{
			LinkedFunctionBodies[
				DebugOwnerLookup[LookupOrdinal].BodyLinkOrdinal]
				.bDuplicateDebugOwner = true;
		}
	}
	int32 UniqueDebugSidecarCount = 0;
	for (FLinkedFunctionBody& Linked : LinkedFunctionBodies)
	{
		if (!Linked.DebugInputOrdinal.IsSet() || Linked.bDuplicateDebugOwner)
		{
			Linked.DebugSequenceOrdinal.Reset();
			continue;
		}
		Linked.DebugSequenceOrdinal =
			static_cast<uint32>(UniqueDebugSidecarCount++);
	}

	if (Snapshot->TypeSchemas.Num() > MAX_int32 - 3
		|| Snapshot->FunctionBodies.Num()
			> MAX_int32 - 3 - Snapshot->TypeSchemas.Num()
		|| UniqueDebugSidecarCount
			> MAX_int32 - 3 - Snapshot->TypeSchemas.Num()
				- Snapshot->FunctionBodies.Num()
		|| State->Initializers.Num()
			> MAX_int32 - Snapshot->FunctionBodies.Num()
		|| UniqueDebugSidecarCount
			> MAX_int32 - Snapshot->FunctionBodies.Num()
				- State->Initializers.Num())
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	const int32 ReachableCount = 3 + Snapshot->TypeSchemas.Num()
		+ Snapshot->FunctionBodies.Num() + UniqueDebugSidecarCount;
	const int32 OpaqueSummaryCount =
		State->Initializers.Num() + Snapshot->FunctionBodies.Num()
			+ UniqueDebugSidecarCount;

	int32 ReachableCapacity = 0;
	int32 RecordOrdinalCapacity = 0;
	int32 TypeOrdinalCapacity = 0;
	int32 GlobalOrdinalCapacity = 0;
	int32 FunctionOrdinalCapacity = 0;
	int32 InitializerOrdinalCapacity = 0;
	int32 OpaqueSummaryCapacity = 0;
	int32 OpaqueOwnerOrdinalCapacity = 0;
	uint64 ReachableBytes = 0;
	uint64 RecordOrdinalBytes = 0;
	uint64 TypeOrdinalBytes = 0;
	uint64 GlobalOrdinalBytes = 0;
	uint64 FunctionOrdinalBytes = 0;
	uint64 InitializerOrdinalBytes = 0;
	uint64 OpaqueSummaryBytes = 0;
	uint64 OpaqueOwnerOrdinalBytes = 0;
	if (!TryCalculateArrayReserveBytes<FAngelscriptDecodedCacheRecordHandle>(
			ReachableCount, ReachableCapacity, ReachableBytes)
		|| !TryCalculateArrayReserveBytes<FAngelscriptCacheValidatedRecordOrdinal>(
			ReachableCount, RecordOrdinalCapacity, RecordOrdinalBytes)
		|| !TryCalculateArrayReserveBytes<FAngelscriptCacheValidatedTypeOrdinal>(
			Snapshot->TypeSchemas.Num(), TypeOrdinalCapacity, TypeOrdinalBytes)
		|| !TryCalculateArrayReserveBytes<FAngelscriptCacheValidatedGlobalOrdinal>(
			State->OrderedGlobals.Num(), GlobalOrdinalCapacity, GlobalOrdinalBytes)
		|| !TryCalculateArrayReserveBytes<FAngelscriptCacheValidatedFunctionOrdinal>(
			FunctionDeclarations.Num(), FunctionOrdinalCapacity, FunctionOrdinalBytes)
		|| !TryCalculateArrayReserveBytes<FAngelscriptCacheValidatedInitializerOrdinal>(
			State->Initializers.Num(), InitializerOrdinalCapacity,
			InitializerOrdinalBytes)
		|| !TryCalculateArrayReserveBytes<FAngelscriptCacheOpaquePayloadSummary>(
			OpaqueSummaryCount, OpaqueSummaryCapacity, OpaqueSummaryBytes)
		|| !TryCalculateArrayReserveBytes<FAngelscriptCacheValidatedOpaqueOwnerOrdinal>(
			OpaqueSummaryCount, OpaqueOwnerOrdinalCapacity, OpaqueOwnerOrdinalBytes))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}

	FAngelscriptCacheModuleGraphCandidateAccess::FTransaction CandidateTransaction =
		FAngelscriptCacheModuleGraphCandidateAccess::Begin(Budget, Limits);
	FGraphCandidateChargeSink CandidateChargeSink(CandidateTransaction);
	FAngelscriptValidatedModuleGraph Candidate;
	Candidate.ModuleKey = Snapshot->ModuleKey;

	const auto ChargeCandidate = [&CandidateTransaction](const uint64 Bytes)
	{
		return FAngelscriptCacheModuleGraphCandidateAccess::TryExtend(
			CandidateTransaction, Bytes);
	};
	if (const EAngelscriptCacheCandidateChargeResult Charge =
		ChargeCandidate(ReachableBytes);
		Charge != EAngelscriptCacheCandidateChargeResult::Success)
	{
		return CandidateChargeFailure(Charge);
	}
	Candidate.ReachableRecords.Reserve(ReachableCapacity);
	if (static_cast<uint64>(Candidate.ReachableRecords.GetAllocatedSize())
		!= ReachableBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	if (const EAngelscriptCacheCandidateChargeResult Charge =
		ChargeCandidate(RecordOrdinalBytes);
		Charge != EAngelscriptCacheCandidateChargeResult::Success)
	{
		return CandidateChargeFailure(Charge);
	}
	Candidate.RecordOrdinals.Reserve(RecordOrdinalCapacity);
	if (static_cast<uint64>(Candidate.RecordOrdinals.GetAllocatedSize())
		!= RecordOrdinalBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	if (const EAngelscriptCacheCandidateChargeResult Charge =
		ChargeCandidate(TypeOrdinalBytes);
		Charge != EAngelscriptCacheCandidateChargeResult::Success)
	{
		return CandidateChargeFailure(Charge);
	}
	Candidate.TypeOrdinals.Reserve(TypeOrdinalCapacity);
	if (static_cast<uint64>(Candidate.TypeOrdinals.GetAllocatedSize())
		!= TypeOrdinalBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	if (const EAngelscriptCacheCandidateChargeResult Charge =
		ChargeCandidate(GlobalOrdinalBytes);
		Charge != EAngelscriptCacheCandidateChargeResult::Success)
	{
		return CandidateChargeFailure(Charge);
	}
	Candidate.GlobalOrdinals.Reserve(GlobalOrdinalCapacity);
	if (static_cast<uint64>(Candidate.GlobalOrdinals.GetAllocatedSize())
		!= GlobalOrdinalBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	if (const EAngelscriptCacheCandidateChargeResult Charge =
		ChargeCandidate(FunctionOrdinalBytes);
		Charge != EAngelscriptCacheCandidateChargeResult::Success)
	{
		return CandidateChargeFailure(Charge);
	}
	Candidate.FunctionOrdinals.Reserve(FunctionOrdinalCapacity);
	if (static_cast<uint64>(Candidate.FunctionOrdinals.GetAllocatedSize())
		!= FunctionOrdinalBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	if (const EAngelscriptCacheCandidateChargeResult Charge =
		ChargeCandidate(InitializerOrdinalBytes);
		Charge != EAngelscriptCacheCandidateChargeResult::Success)
	{
		return CandidateChargeFailure(Charge);
	}
	Candidate.InitializerOrdinals.Reserve(InitializerOrdinalCapacity);
	if (static_cast<uint64>(Candidate.InitializerOrdinals.GetAllocatedSize())
		!= InitializerOrdinalBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	if (const EAngelscriptCacheCandidateChargeResult Charge =
		ChargeCandidate(OpaqueSummaryBytes);
		Charge != EAngelscriptCacheCandidateChargeResult::Success)
	{
		return CandidateChargeFailure(Charge);
	}
	Candidate.OpaqueSummaries.Reserve(OpaqueSummaryCapacity);
	if (static_cast<uint64>(Candidate.OpaqueSummaries.GetAllocatedSize())
		!= OpaqueSummaryBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	if (const EAngelscriptCacheCandidateChargeResult Charge =
		ChargeCandidate(OpaqueOwnerOrdinalBytes);
		Charge != EAngelscriptCacheCandidateChargeResult::Success)
	{
		return CandidateChargeFailure(Charge);
	}
	Candidate.OpaqueOwnerOrdinals.Reserve(OpaqueOwnerOrdinalCapacity);
	if (static_cast<uint64>(Candidate.OpaqueOwnerOrdinals.GetAllocatedSize())
		!= OpaqueOwnerOrdinalBytes)
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	for (int32 StateGlobalOrdinal = 0;
		StateGlobalOrdinal < State->OrderedGlobals.Num(); ++StateGlobalOrdinal)
	{
		Candidate.GlobalOrdinals.Add({
			State->OrderedGlobals[StateGlobalOrdinal].GlobalKey,
			static_cast<uint32>(StateGlobalOrdinal)});
	}
	Candidate.GlobalOrdinals.Sort([](
		const FAngelscriptCacheValidatedGlobalOrdinal& Left,
		const FAngelscriptCacheValidatedGlobalOrdinal& Right)
	{
		return Left.GlobalKey.Hash < Right.GlobalKey.Hash;
	});

	Candidate.ReachableRecords.Add(RootRecord);
	Candidate.ReachableRecords.Add(InterfaceRecord);
	Candidate.ReachableRecords.Add(StateRecord);
	int32 MatchedPropertyDeclarationCount = 0;
	for (int32 TypeOrdinal = 0;
		TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
	{
		Candidate.ReachableRecords.Add(
			LocallyValidatedRecords[TypeInputOrdinals[TypeOrdinal]]);
		Candidate.TypeOrdinals.Add({
			Snapshot->TypeSchemas[TypeOrdinal].TypeKey,
			static_cast<uint32>(3 + TypeOrdinal)});
	}
	const uint32 BodyRecordBase = static_cast<uint32>(
		3 + Snapshot->TypeSchemas.Num());
	for (const FLinkedFunctionBody& Linked : LinkedFunctionBodies)
	{
		Candidate.ReachableRecords.Add(
			LocallyValidatedRecords[Linked.BodyInputOrdinal]);
	}
	for (const FLinkedFunctionBody& Linked : LinkedFunctionBodies)
	{
		if (Linked.DebugInputOrdinal.IsSet() && !Linked.bDuplicateDebugOwner)
		{
			Candidate.ReachableRecords.Add(
				LocallyValidatedRecords[Linked.DebugInputOrdinal.GetValue()]);
		}
	}
	for (uint32 RecordOrdinal = 0;
		RecordOrdinal < static_cast<uint32>(Candidate.ReachableRecords.Num());
		++RecordOrdinal)
	{
		Candidate.RecordOrdinals.Add({
			Candidate.ReachableRecords[RecordOrdinal]->GetRecordId(), RecordOrdinal});
	}
	Candidate.RecordOrdinals.Sort(
		[](const FAngelscriptCacheValidatedRecordOrdinal& Left,
			const FAngelscriptCacheValidatedRecordOrdinal& Right)
		{
			return Left.RecordId < Right.RecordId;
		});

	// ModuleState actions are the sole execution-order authority. Validate each
	// reachable initializer payload exactly once in ascending ActionOrdinal,
	// before FunctionBody and Debug payloads, while publishing a separate
	// stable-key-sorted initializer index below.
	for (int32 ActionOrdinal = 0;
		ActionOrdinal < State->OrderedInitializationActions.Num();
		++ActionOrdinal)
	{
		const FAngelscriptCachedInitializationAction& Action =
			State->OrderedInitializationActions[ActionOrdinal];
		const TOptional<uint32> UnitOrdinal =
			FindInitializerUnitOrdinal(Action.Target.StableKey);
		check(UnitOrdinal.IsSet());
		const FAngelscriptCachedInitializerUnit& Initializer =
			State->Initializers[UnitOrdinal.GetValue()];
		const uint32 SummaryOrdinal =
			static_cast<uint32>(Candidate.OpaqueSummaries.Num());
		FAngelscriptCacheOpaquePayloadSummary& Summary =
			Candidate.OpaqueSummaries.AddDefaulted_GetRef();
		FAngelscriptCacheOpaquePayloadValidationRequest Request;
		Request.Kind = EAngelscriptCacheOpaquePayloadKind::InitializerExecution;
		Request.CodecVersion = Initializer.VmInitializerCodecVersion;
		Request.ModuleKey = State->ModuleKey;
		Request.OwnerKey = Initializer.InitializerKey.Hash;
		Request.Profile = State->Profile;
		Request.CanonicalPayload = Initializer.CanonicalExecutionPayload;
		const FAngelscriptCacheValidationResult OpaqueResult =
			Context.OpaquePayloads->Validate(
				Request, Limits, Budget, CandidateChargeSink, Summary);
		if (!OpaqueResult.IsSuccess())
		{
			return OpaqueResult;
		}
		Candidate.InitializerOrdinals.Add({
			Initializer.InitializerKey, UnitOrdinal.GetValue(),
			static_cast<uint32>(ActionOrdinal), SummaryOrdinal});
		Candidate.OpaqueOwnerOrdinals.Add({
			{EAngelscriptCacheOpaquePayloadKind::InitializerExecution,
				Initializer.InitializerKey.Hash},
			SummaryOrdinal});
	}
	Candidate.InitializerOrdinals.Sort([](
		const FAngelscriptCacheValidatedInitializerOrdinal& Left,
		const FAngelscriptCacheValidatedInitializerOrdinal& Right)
	{
		return Left.InitializerKey.Hash < Right.InitializerKey.Hash;
	});

	for (int32 DependencyBodyOrdinal = 0;
		DependencyBodyOrdinal < Snapshot->FunctionBodies.Num();
		++DependencyBodyOrdinal)
	{
		const FAngelscriptCachedFunctionBody& Body =
			*Candidate.ReachableRecords[BodyRecordBase + DependencyBodyOrdinal]
				->TryGetFunctionBody();
		FAngelscriptCacheOpaquePayloadSummary& Summary =
			Candidate.OpaqueSummaries.AddDefaulted_GetRef();
		FAngelscriptCacheOpaquePayloadValidationRequest Request;
		Request.Kind = EAngelscriptCacheOpaquePayloadKind::FunctionExecution;
		Request.CodecVersion = Body.VmExecutionCodecVersion;
		Request.ModuleKey = Body.ModuleKey;
		Request.OwnerKey = Body.Identity.FunctionKey.Hash;
		Request.Profile = Body.Identity.Profile;
		Request.CanonicalPayload = Body.CanonicalExecutionPayload;
		Request.DeclaredDependencies = Body.ActualDependencies;
		const FAngelscriptCacheValidationResult OpaqueResult =
			Context.OpaquePayloads->Validate(
				Request, Limits, Budget, CandidateChargeSink, Summary);
		if (!OpaqueResult.IsSuccess())
		{
			return OpaqueResult;
		}
		Candidate.OpaqueOwnerOrdinals.Add({
			{EAngelscriptCacheOpaquePayloadKind::FunctionExecution,
				Body.Identity.FunctionKey.Hash},
			static_cast<uint32>(Candidate.OpaqueSummaries.Num() - 1)});
	}
	for (const FLinkedFunctionBody& Linked : LinkedFunctionBodies)
	{
		if (!Linked.DebugInputOrdinal.IsSet() || Linked.bDuplicateDebugOwner)
		{
			continue;
		}
		const FAngelscriptCachedDebugSidecar& Debug =
			*LocallyValidatedRecords[Linked.DebugInputOrdinal.GetValue()]
				->TryGetDebugSidecar();
		const FAngelscriptCachedFunctionBody& Body =
			*LocallyValidatedRecords[Linked.BodyInputOrdinal]->TryGetFunctionBody();
		FAngelscriptCacheOpaquePayloadSummary& Summary =
			Candidate.OpaqueSummaries.AddDefaulted_GetRef();
		FAngelscriptCacheOpaquePayloadValidationRequest Request;
		Request.Kind = EAngelscriptCacheOpaquePayloadKind::Debug;
		Request.CodecVersion = Debug.VmDebugCodecVersion;
		Request.ModuleKey = Body.ModuleKey;
		Request.OwnerKey = Debug.FunctionKey.Hash;
		Request.Profile = Debug.Profile;
		Request.CanonicalPayload = Debug.CanonicalDebugPayload;
		const FAngelscriptCacheValidationResult OpaqueResult =
			Context.OpaquePayloads->Validate(
				Request, Limits, Budget, CandidateChargeSink, Summary);
		if (!OpaqueResult.IsSuccess())
		{
			return OpaqueResult;
		}
		Candidate.OpaqueOwnerOrdinals.Add({
			{EAngelscriptCacheOpaquePayloadKind::Debug, Debug.FunctionKey.Hash},
			static_cast<uint32>(Candidate.OpaqueSummaries.Num() - 1)});
	}
	Candidate.OpaqueOwnerOrdinals.Sort([](
		const FAngelscriptCacheValidatedOpaqueOwnerOrdinal& Left,
		const FAngelscriptCacheValidatedOpaqueOwnerOrdinal& Right)
	{
		const uint8 LeftKind = static_cast<uint8>(Left.Owner.Kind);
		const uint8 RightKind = static_cast<uint8>(Right.Owner.Kind);
		return LeftKind != RightKind
			? LeftKind < RightKind
			: Left.Owner.OwnerKey < Right.Owner.OwnerKey;
	});

	if (!(Snapshot->ModuleInterface.ModuleKey == Snapshot->ModuleKey)
		|| !(Snapshot->ModuleState.ModuleKey == Snapshot->ModuleKey)
		|| !(Interface->ModuleKey == Snapshot->ModuleKey)
		|| !(State->ModuleKey == Snapshot->ModuleKey))
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::CrossModuleOwner,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::ModuleKey));
	}

	// Step 4 begins with exact Required Type declaration/link equality. Both
	// arrays are now canonical by complete TypeKey, so the first merge mismatch
	// deterministically identifies either the missing declaration coverage or the
	// undeclared linked record without an O(n^2) scan.
	int32 RequiredOrdinal = 0;
	int32 LinkOrdinal = 0;
	while (RequiredOrdinal < RequiredTypes.Num()
		&& LinkOrdinal < Snapshot->TypeSchemas.Num())
	{
		const FRequiredTypeDeclaration& Required = RequiredTypes[RequiredOrdinal];
		const FAngelscriptCachedTypeSchemaLink& Link =
			Snapshot->TypeSchemas[LinkOrdinal];
		if (Required.TypeKey == Link.TypeKey)
		{
			++RequiredOrdinal;
			++LinkOrdinal;
			continue;
		}
		if (Required.TypeKey.Hash < Link.TypeKey.Hash)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::MissingCoverage,
				InterfaceOffset(*InterfaceRecord,
					EAngelscriptModuleInterfaceCapturedField::Declaration,
					Required.DeclarationOrdinal));
		}
		return GraphFailure(
			EAngelscriptCacheValidationError::UndeclaredEntity,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkTypeKey,
				static_cast<uint32>(LinkOrdinal)));
	}
	if (RequiredOrdinal < RequiredTypes.Num())
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::MissingCoverage,
			InterfaceOffset(*InterfaceRecord,
				EAngelscriptModuleInterfaceCapturedField::Declaration,
				RequiredTypes[RequiredOrdinal].DeclarationOrdinal));
	}
	if (LinkOrdinal < Snapshot->TypeSchemas.Num())
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::UndeclaredEntity,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::TypeSchemaLinkTypeKey,
				static_cast<uint32>(LinkOrdinal)));
	}

	for (int32 TypeOrdinal = 0;
		TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
	{
		const FAngelscriptCachedTypeSchemaLink& Link =
			Snapshot->TypeSchemas[TypeOrdinal];
		const FAngelscriptDecodedCacheRecordHandle& TypeRecord =
			LocallyValidatedRecords[TypeInputOrdinals[TypeOrdinal]];
		const FAngelscriptCachedTypeSchema& Type =
			*TypeRecord->TryGetTypeSchema();
		const FRequiredTypeDeclaration& Required = RequiredTypes[TypeOrdinal];
		const FAngelscriptCachedDeclaration& Declaration =
			Interface->Declarations[Required.DeclarationOrdinal];
		if (!(Type.ModuleKey == Snapshot->ModuleKey))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::CrossModuleOwner,
				TypeSchemaOffset(*TypeRecord,
					EAngelscriptTypeSchemaCapturedField::ModuleKey));
		}
		const TOptional<EAngelscriptCachedTypeKind> ExpectedTypeKind =
			TypeKindForEntity(Declaration.EntityKind);
		if (!(Type.TypeKey == Link.TypeKey)
			|| !ExpectedTypeKind.IsSet()
			|| Type.TypeKind != ExpectedTypeKind.GetValue()
			|| Type.CanonicalNamespace != Declaration.CanonicalNamespace
			|| Type.CanonicalName != Declaration.CanonicalName
			|| Type.CanonicalDeclaration != Declaration.CanonicalDeclaration)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				TypeSchemaOffset(*TypeRecord,
					EAngelscriptTypeSchemaCapturedField::TypeKey));
		}

		for (int32 PropertyOrdinal = 0;
			PropertyOrdinal < Type.OrderedProperties.Num(); ++PropertyOrdinal)
		{
			const FAngelscriptCachedPropertySchema& Property =
				Type.OrderedProperties[PropertyOrdinal];
			FPropertyDeclaration* PropertyAuthority =
				FindPropertyDeclaration(PropertyDeclarations, Property.PropertyKey);
			if (PropertyAuthority == nullptr)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::UndeclaredEntity,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::PropertyKey,
						static_cast<uint32>(PropertyOrdinal)));
			}
			const FAngelscriptCachedDeclaration& PropertyDeclaration =
				Interface->Declarations[PropertyAuthority->DeclarationOrdinal];
			PropertyAuthority->TypeSchemaRecordOrdinal =
				static_cast<uint32>(3 + TypeOrdinal);
			PropertyAuthority->PropertySchemaOrdinal =
				static_cast<uint32>(PropertyOrdinal);
			const uint32 PrivateTrait = static_cast<uint32>(
				EAngelscriptCachedDeclarationTraitFlags::Private);
			const uint32 ProtectedTrait = static_cast<uint32>(
				EAngelscriptCachedDeclarationTraitFlags::Protected);
			const uint32 AccessTraits = PropertyDeclaration.TraitFlags
				& (PrivateTrait | ProtectedTrait);
			const EAngelscriptCachedMemberAccess ExpectedAccess =
				AccessTraits == PrivateTrait
					? EAngelscriptCachedMemberAccess::Private
					: AccessTraits == ProtectedTrait
						? EAngelscriptCachedMemberAccess::Protected
						: EAngelscriptCachedMemberAccess::Public;
			const uint32 ReadableReflection = static_cast<uint32>(
				EAngelscriptCachedReflectionFlags::BlueprintReadable);
			const uint32 WritableReflection = static_cast<uint32>(
				EAngelscriptCachedReflectionFlags::BlueprintWritable);
			uint32 ExpectedPropertyReflection = 0;
			if ((PropertyDeclaration.ReflectionFlags & ReadableReflection) != 0)
			{
				ExpectedPropertyReflection |= static_cast<uint32>(
					EAngelscriptCachedPropertySemanticFlags::BlueprintReadable);
			}
			if ((PropertyDeclaration.ReflectionFlags & WritableReflection) != 0)
			{
				ExpectedPropertyReflection |= static_cast<uint32>(
					EAngelscriptCachedPropertySemanticFlags::BlueprintWritable);
			}
			const uint32 PropertyReflectionMask = static_cast<uint32>(
				EAngelscriptCachedPropertySemanticFlags::BlueprintReadable)
				| static_cast<uint32>(
					EAngelscriptCachedPropertySemanticFlags::BlueprintWritable);
			if (PropertyDeclaration.DeclarationKind
					!= EAngelscriptCacheDeclarationKind::Property
				|| PropertyDeclaration.EntityKind
					!= EAngelscriptArtifactEntityKind::Property
				|| PropertyDeclaration.SchemaCoverage
					!= EAngelscriptCacheSchemaCoverage::Forbidden
				|| PropertyDeclaration.BodyCoverage
					!= EAngelscriptCacheBodyCoverage::Forbidden
				|| PropertyDeclaration.OwnerKind
					!= EAngelscriptFunctionOwnerKind::Type
				|| !(PropertyDeclaration.OwnerKey == Type.TypeKey.Hash)
				|| !(PropertyDeclaration.ModuleKey == Snapshot->ModuleKey)
				|| PropertyDeclaration.CanonicalNamespace
					!= Type.CanonicalNamespace
				|| PropertyDeclaration.CanonicalName != Property.CanonicalName
				|| !PropertyDeclaration.CanonicalTypeSpelling.IsSet()
				|| !PropertyDeclaration.DeclaredType.IsSet()
				|| !DataTypesEqual(PropertyDeclaration.DeclaredType.GetValue(),
					Property.Type)
				|| !PropertyDeclaration.OrderedParameters.IsEmpty()
				|| AccessTraits == (PrivateTrait | ProtectedTrait)
				|| Property.Access != ExpectedAccess
				|| (PropertyDeclaration.ReflectionFlags
					& ~(ReadableReflection | WritableReflection)) != 0
				|| (Property.PropertySemanticFlags & PropertyReflectionMask)
					!= ExpectedPropertyReflection
				|| !MetadataEqual(
					PropertyDeclaration.Metadata, Property.Metadata))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::OrderedProperty,
						static_cast<uint32>(PropertyOrdinal)));
			}
			++MatchedPropertyDeclarationCount;
		}

		const auto ResolveOwnedScriptFunction = [
			&FunctionDeclarations, Interface, Snapshot](
			const FAngelscriptStableFunctionKey& FunctionKey,
			const FAngelscriptHash256& ExpectedAbi,
			const FAngelscriptStableTypeKey& ExpectedOwner,
			const uint64 DiagnosticOffset,
			const FAngelscriptCachedDeclaration*& OutDeclaration)
			-> FAngelscriptCacheValidationResult
		{
			OutDeclaration = nullptr;
			const FFunctionDeclaration* Function = FindFunctionDeclaration(
				FunctionDeclarations, FunctionKey.Hash);
			if (Function == nullptr)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::MissingGraphTarget,
					DiagnosticOffset);
			}
			const FAngelscriptCachedDeclaration& FunctionDeclaration =
				Interface->Declarations[Function->DeclarationOrdinal];
			if (FunctionDeclaration.DeclarationKind
					!= EAngelscriptCacheDeclarationKind::Function)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					DiagnosticOffset);
			}
			if (!(FunctionDeclaration.ModuleKey == Snapshot->ModuleKey))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::CrossModuleOwner,
					DiagnosticOffset);
			}
			if (FunctionDeclaration.OwnerKind
					!= EAngelscriptFunctionOwnerKind::Type
				|| !(FunctionDeclaration.OwnerKey == ExpectedOwner.Hash))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::MissingOwner,
					DiagnosticOffset);
			}
			if (!(FunctionDeclaration.SignatureHash == ExpectedAbi))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					DiagnosticOffset);
			}
			OutDeclaration = &FunctionDeclaration;
			return {};
		};
		const auto IsMethodEntity = [](
			const EAngelscriptArtifactEntityKind EntityKind)
		{
			return EntityKind == EAngelscriptArtifactEntityKind::Method
				|| EntityKind == EAngelscriptArtifactEntityKind::InitDefaults;
		};

		bool bGraphSupportedMethodTopology = true;
		const FAngelscriptCachedTypeSchema* BaseType = nullptr;
		for (const FAngelscriptCachedTypeRelation& Relation : Type.Relations)
		{
			if (Relation.RelationKind
				!= EAngelscriptCachedTypeRelationKind::Base)
			{
				continue;
			}
			int32 First = 0;
			int32 Last = Snapshot->TypeSchemas.Num();
			while (First < Last)
			{
				const int32 Middle = First + (Last - First) / 2;
				if (Snapshot->TypeSchemas[Middle].TypeKey.Hash
					< Relation.Target.StableKey)
				{
					First = Middle + 1;
				}
				else
				{
					Last = Middle;
				}
			}
			if (!Snapshot->TypeSchemas.IsValidIndex(First)
				|| Snapshot->TypeSchemas[First].TypeKey.Hash
					!= Relation.Target.StableKey)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::MissingGraphTarget,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::RelationTarget));
			}
			const FAngelscriptDecodedCacheRecordHandle& BaseRecord =
				LocallyValidatedRecords[TypeInputOrdinals[First]];
			BaseType = BaseRecord->TryGetTypeSchema();
			if (BaseType == nullptr
				|| BaseType->TypeKind != EAngelscriptCachedTypeKind::Class)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::RelationTarget));
			}
		}
		for (int32 MethodOrdinal = 0;
			MethodOrdinal < Type.OrderedMethods.Num(); ++MethodOrdinal)
		{
			const FAngelscriptCachedMethodEntry& Method =
				Type.OrderedMethods[MethodOrdinal];
			const FAngelscriptCachedDeclaration* FunctionDeclaration = nullptr;
			const FAngelscriptCacheValidationResult FunctionResult =
				ResolveOwnedScriptFunction(
					Method.FunctionKey,
					Method.ExpectedDeclarationAbi,
					Method.DeclaringOwner,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::MethodFunction,
						static_cast<uint32>(MethodOrdinal)),
					FunctionDeclaration);
			if (!FunctionResult.IsSuccess())
			{
				return FunctionResult;
			}
			if (!IsMethodEntity(FunctionDeclaration->EntityKind))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::MethodFunction,
						static_cast<uint32>(MethodOrdinal)));
			}
			if (Method.EntryKind
				== EAngelscriptCachedMethodSlotKind::Inherited)
			{
				bool bMatchesDirectBaseMethod = false;
				if (BaseType != nullptr)
				{
					for (const FAngelscriptCachedMethodEntry& BaseMethod
						: BaseType->OrderedMethods)
					{
						bMatchesDirectBaseMethod |=
							Method.FunctionKey == BaseMethod.FunctionKey
							&& Method.DeclaringOwner == BaseMethod.DeclaringOwner
							&& Method.ExpectedDeclarationAbi
								== BaseMethod.ExpectedDeclarationAbi;
					}
				}
				bGraphSupportedMethodTopology &= bMatchesDirectBaseMethod;
			}
			else
			{
				bGraphSupportedMethodTopology &= Method.EntryKind
						== EAngelscriptCachedMethodSlotKind::LocalMethod
					&& Method.DeclaringOwner.Hash == Type.TypeKey.Hash;
			}
		}
		if (BaseType != nullptr)
		{
			for (const FAngelscriptCachedMethodEntry& BaseMethod
				: BaseType->OrderedMethods)
			{
				bool bInheritedExactly = false;
				for (const FAngelscriptCachedMethodEntry& Method
					: Type.OrderedMethods)
				{
					bInheritedExactly |= Method.EntryKind
							== EAngelscriptCachedMethodSlotKind::Inherited
						&& Method.FunctionKey == BaseMethod.FunctionKey
						&& Method.DeclaringOwner == BaseMethod.DeclaringOwner
						&& Method.ExpectedDeclarationAbi
							== BaseMethod.ExpectedDeclarationAbi;
				}
				if (bInheritedExactly)
				{
					continue;
				}

				bool bReplacedByVirtualOverride = false;
				for (int32 BaseVftOrdinal = 0;
					BaseVftOrdinal < BaseType->VirtualFunctionTable.Num();
					++BaseVftOrdinal)
				{
					const FAngelscriptCachedVirtualFunctionSlot& BaseSlot =
						BaseType->VirtualFunctionTable[BaseVftOrdinal];
					if (BaseSlot.FunctionKey == BaseMethod.FunctionKey
						&& BaseSlot.ExpectedDeclarationAbi
							== BaseMethod.ExpectedDeclarationAbi
						&& Type.VirtualFunctionTable.IsValidIndex(BaseVftOrdinal)
						&& Type.VirtualFunctionTable[BaseVftOrdinal].SlotKind
							== EAngelscriptCachedMethodSlotKind::VirtualOverride)
					{
						bReplacedByVirtualOverride = true;
						break;
					}
				}
				bGraphSupportedMethodTopology &= bReplacedByVirtualOverride;
			}
		}

		for (int32 VftOrdinal = 0;
			VftOrdinal < Type.VirtualFunctionTable.Num(); ++VftOrdinal)
		{
			const FAngelscriptCachedVirtualFunctionSlot& Slot =
				Type.VirtualFunctionTable[VftOrdinal];
			const FAngelscriptCachedDeclaration* FunctionDeclaration = nullptr;
			const FAngelscriptCacheValidationResult FunctionResult =
				ResolveOwnedScriptFunction(
					Slot.FunctionKey,
					Slot.ExpectedDeclarationAbi,
					Slot.ImplementingOwner,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::VirtualFunction,
						static_cast<uint32>(VftOrdinal)),
					FunctionDeclaration);
			if (!FunctionResult.IsSuccess())
			{
				return FunctionResult;
			}
			if (!IsMethodEntity(FunctionDeclaration->EntityKind))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::VirtualFunction,
						static_cast<uint32>(VftOrdinal)));
			}
			if (BaseType != nullptr
				&& VftOrdinal < BaseType->VirtualFunctionTable.Num())
			{
				const FAngelscriptCachedVirtualFunctionSlot& BaseSlot =
					BaseType->VirtualFunctionTable[VftOrdinal];
				const bool bExactInherited = Slot.SlotKind
						== EAngelscriptCachedMethodSlotKind::Inherited
					&& Slot.FunctionKey == BaseSlot.FunctionKey
					&& Slot.DeclaringOwner == BaseSlot.DeclaringOwner
					&& Slot.ImplementingOwner == BaseSlot.ImplementingOwner
					&& Slot.ExpectedDeclarationAbi
						== BaseSlot.ExpectedDeclarationAbi;
				const bool bExactOverride = Slot.SlotKind
						== EAngelscriptCachedMethodSlotKind::VirtualOverride
					&& Slot.DeclaringOwner == BaseSlot.DeclaringOwner
					&& Slot.ImplementingOwner.Hash == Type.TypeKey.Hash;
				bGraphSupportedMethodTopology &=
					bExactInherited || bExactOverride;
			}
			else
			{
				bGraphSupportedMethodTopology &= Slot.SlotKind
						== EAngelscriptCachedMethodSlotKind::VirtualDeclaration
					&& Slot.DeclaringOwner.Hash == Type.TypeKey.Hash
					&& Slot.ImplementingOwner.Hash == Type.TypeKey.Hash;
			}
			if (Slot.SlotKind
					== EAngelscriptCachedMethodSlotKind::VirtualDeclaration
				|| Slot.SlotKind
					== EAngelscriptCachedMethodSlotKind::VirtualOverride)
			{
				bool bHasImplementingMethod = false;
				for (const FAngelscriptCachedMethodEntry& Method
					: Type.OrderedMethods)
				{
					bHasImplementingMethod |= Method.EntryKind
							== EAngelscriptCachedMethodSlotKind::LocalMethod
						&& Method.FunctionKey == Slot.FunctionKey
						&& Method.DeclaringOwner == Slot.ImplementingOwner
						&& Method.ExpectedDeclarationAbi
							== Slot.ExpectedDeclarationAbi;
				}
				bGraphSupportedMethodTopology &= bHasImplementingMethod;
			}
		}
		if (BaseType != nullptr
			&& Type.VirtualFunctionTable.Num()
				< BaseType->VirtualFunctionTable.Num())
		{
			bGraphSupportedMethodTopology = false;
		}

		for (int32 ReflectionOrdinal = 0;
			ReflectionOrdinal
				< Type.Reflection.OrderedUFunctionMembers.Num();
			++ReflectionOrdinal)
		{
			const FAngelscriptCachedReflectedFunctionMember& Member =
				Type.Reflection.OrderedUFunctionMembers[ReflectionOrdinal];
			const FAngelscriptCachedDeclaration* FunctionDeclaration = nullptr;
			const FAngelscriptCacheValidationResult FunctionResult =
				ResolveOwnedScriptFunction(
					FAngelscriptStableFunctionKey{Member.Target.StableKey},
					Member.Target.ExpectedAbi,
					Type.TypeKey,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::ReflectedFunctionTarget,
						static_cast<uint32>(ReflectionOrdinal)),
					FunctionDeclaration);
			if (!FunctionResult.IsSuccess())
			{
				return FunctionResult;
			}
			if (FunctionDeclaration->EntityKind
				!= EAngelscriptArtifactEntityKind::Method)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::ReflectedFunctionTarget,
						static_cast<uint32>(ReflectionOrdinal)));
			}
			if (FunctionDeclaration->CanonicalName
				!= Member.CanonicalScriptFunctionName)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::ConflictingKey,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::
							ReflectedScriptFunctionName,
						static_cast<uint32>(ReflectionOrdinal)));
			}
		}

		const auto IsBehaviorEntityAllowed = [](
			const EAngelscriptCachedBehaviorKind BehaviorKind,
			const EAngelscriptArtifactEntityKind EntityKind)
		{
			switch (BehaviorKind)
			{
			case EAngelscriptCachedBehaviorKind::Construct:
				return EntityKind == EAngelscriptArtifactEntityKind::Constructor
					|| EntityKind == EAngelscriptArtifactEntityKind::
						GeneratedDefaultConstructor;
			case EAngelscriptCachedBehaviorKind::ListConstruct:
			case EAngelscriptCachedBehaviorKind::CopyConstruct:
				return EntityKind == EAngelscriptArtifactEntityKind::Constructor;
			case EAngelscriptCachedBehaviorKind::Destruct:
				return EntityKind == EAngelscriptArtifactEntityKind::Destructor
					|| EntityKind == EAngelscriptArtifactEntityKind::
						GeneratedDefaultDestructor;
			case EAngelscriptCachedBehaviorKind::Factory:
			case EAngelscriptCachedBehaviorKind::ListFactory:
			case EAngelscriptCachedBehaviorKind::CopyFactory:
				return EntityKind == EAngelscriptArtifactEntityKind::Factory;
			case EAngelscriptCachedBehaviorKind::AddRef:
			case EAngelscriptCachedBehaviorKind::Release:
			case EAngelscriptCachedBehaviorKind::GetWeakRefFlag:
			case EAngelscriptCachedBehaviorKind::GetRefCount:
			case EAngelscriptCachedBehaviorKind::SetGcFlag:
			case EAngelscriptCachedBehaviorKind::GetGcFlag:
			case EAngelscriptCachedBehaviorKind::EnumRefs:
			case EAngelscriptCachedBehaviorKind::ReleaseRefs:
			case EAngelscriptCachedBehaviorKind::Copy:
				return EntityKind == EAngelscriptArtifactEntityKind::Method;
			default:
				return false;
			}
		};
		const auto ParametersEqual = [](
			const FAngelscriptCachedDeclaration& Left,
			const FAngelscriptCachedDeclaration& Right)
		{
			if (Left.OrderedParameters.Num() != Right.OrderedParameters.Num())
			{
				return false;
			}
			for (int32 Index = 0; Index < Left.OrderedParameters.Num(); ++Index)
			{
				const FAngelscriptCachedParameter& A = Left.OrderedParameters[Index];
				const FAngelscriptCachedParameter& B = Right.OrderedParameters[Index];
				if (!DataTypesEqual(A.Type, B.Type)
					|| A.Passing != B.Passing
					|| A.CanonicalDefaultExpression
						!= B.CanonicalDefaultExpression)
				{
					return false;
				}
			}
			return true;
		};
		uint32 ZeroParameterConstructorCount = 0;
		uint32 ZeroParameterFactoryCount = 0;
		for (int32 BehaviorOrdinal = 0;
			BehaviorOrdinal < Type.OrderedBehaviorSlots.Num(); ++BehaviorOrdinal)
		{
			const FAngelscriptCachedBehaviorSlot& Slot =
				Type.OrderedBehaviorSlots[BehaviorOrdinal];
			if (Slot.Target.Kind
				== EAngelscriptCacheReferenceKind::EnvironmentSymbol)
			{
				continue;
			}
			check(Slot.Target.Kind
				== EAngelscriptCacheReferenceKind::ScriptFunction);
			check(Slot.DeclaringOwner.IsSet());
			const FAngelscriptCachedDeclaration* FunctionDeclaration = nullptr;
			const FAngelscriptCacheValidationResult FunctionResult =
				ResolveOwnedScriptFunction(
					FAngelscriptStableFunctionKey{Slot.Target.StableKey},
					Slot.Target.ExpectedAbi,
					Slot.DeclaringOwner.GetValue(),
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::BehaviorTarget,
						static_cast<uint32>(BehaviorOrdinal)),
					FunctionDeclaration);
			if (!FunctionResult.IsSuccess())
			{
				return FunctionResult;
			}
			if (!IsBehaviorEntityAllowed(
				Slot.BehaviorKind, FunctionDeclaration->EntityKind))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::BehaviorTarget,
						static_cast<uint32>(BehaviorOrdinal)));
			}
			ZeroParameterConstructorCount +=
				Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Construct
				&& FunctionDeclaration->OrderedParameters.IsEmpty();
			ZeroParameterFactoryCount +=
				Slot.BehaviorKind == EAngelscriptCachedBehaviorKind::Factory
				&& FunctionDeclaration->OrderedParameters.IsEmpty();

			if (Type.TypeKind == EAngelscriptCachedTypeKind::Class
				&& Slot.BehaviorKind
					== EAngelscriptCachedBehaviorKind::Construct)
			{
				const FAngelscriptCachedBehaviorSlot* FactorySlot = nullptr;
				for (const FAngelscriptCachedBehaviorSlot& CandidateSlot
					: Type.OrderedBehaviorSlots)
				{
					if (CandidateSlot.BehaviorKind
							== EAngelscriptCachedBehaviorKind::Factory
						&& CandidateSlot.SlotOrdinal == Slot.SlotOrdinal)
					{
						FactorySlot = &CandidateSlot;
						break;
					}
				}
				if (FactorySlot == nullptr
					|| FactorySlot->Target.Kind
						!= EAngelscriptCacheReferenceKind::ScriptFunction
					|| !FactorySlot->DeclaringOwner.IsSet())
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::MissingCoverage,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::BehaviorSlot,
							static_cast<uint32>(BehaviorOrdinal)));
				}
				const FAngelscriptCachedDeclaration* FactoryDeclaration = nullptr;
				const FAngelscriptCacheValidationResult FactoryResult =
					ResolveOwnedScriptFunction(
						FAngelscriptStableFunctionKey{
							FactorySlot->Target.StableKey},
						FactorySlot->Target.ExpectedAbi,
						FactorySlot->DeclaringOwner.GetValue(),
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::BehaviorTarget,
							static_cast<uint32>(
								FactorySlot - Type.OrderedBehaviorSlots.GetData())),
						FactoryDeclaration);
				if (!FactoryResult.IsSuccess())
				{
					return FactoryResult;
				}
				if (!ParametersEqual(*FunctionDeclaration, *FactoryDeclaration))
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::BehaviorSlot,
							static_cast<uint32>(BehaviorOrdinal)));
				}
			}
		}
		const bool bHasDefaultConstructor =
			(Type.TypeSemanticFlags & static_cast<uint32>(
				EAngelscriptCachedTypeSemanticFlags::HasDefaultConstructor)) != 0;
		if (bHasDefaultConstructor != (ZeroParameterConstructorCount == 1)
			|| ZeroParameterConstructorCount > 1
			|| (Type.TypeKind == EAngelscriptCachedTypeKind::Class
				&& (ZeroParameterFactoryCount != ZeroParameterConstructorCount
					|| ZeroParameterFactoryCount > 1)))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				TypeSchemaOffset(*TypeRecord,
					EAngelscriptTypeSchemaCapturedField::TypeSemanticFlags));
		}

		// This vertical accepts graph-simple types plus reflected shells whose
		// graph-bearing state is an external CodeRoot/StructHeader layout recipe or
		// a selected-module BaseType edge. Local BaseType values remain immutable
		// graph authority and are checked as a DAG before any current resolver call.
		// The TypeSchema decoder has already proven exact relation/LayoutInput/
		// dependency derivation. Properties and function authorities have now been
		// resolved above. Derived method topology is matched by stable function and
		// owner identity against the direct base rather than by an invalid positional
		// suffix assumption: a virtual override legitimately replaces one base method
		// while local methods can be interleaved before inherited methods.
		bool bGraphSupportedRelations = true;
		for (const FAngelscriptCachedTypeRelation& Relation : Type.Relations)
		{
			bGraphSupportedRelations &=
				Relation.Target.Kind
					== EAngelscriptCacheReferenceKind::EnvironmentSymbol
				|| (Relation.RelationKind
						== EAngelscriptCachedTypeRelationKind::Base
					&& Relation.Target.Kind
						== EAngelscriptCacheReferenceKind::ScriptType
					&& IsSelectedModuleReference(*Interface, Relation.Target));
		}
		bool bGraphSupportedLayoutInputs = true;
		for (const FAngelscriptCachedTypeLayoutInput& Input : Type.LayoutInputs)
		{
			bGraphSupportedLayoutInputs &=
				((Input.InputKind == EAngelscriptCachedTypeLayoutInputKind::CodeRoot
					|| Input.InputKind
						== EAngelscriptCachedTypeLayoutInputKind::StructHeader)
					&& Input.Target.Kind
						== EAngelscriptCacheReferenceKind::EnvironmentSymbol)
				|| (Input.InputKind
						== EAngelscriptCachedTypeLayoutInputKind::BaseType
					&& Input.Target.Kind
						== EAngelscriptCacheReferenceKind::ScriptType
					&& IsSelectedModuleReference(*Interface, Input.Target));
		}
		bool bGraphSupportedDependencies = true;
		for (const FAngelscriptCacheSemanticDependency& Dependency :
			Type.Dependencies)
		{
			bGraphSupportedDependencies &= Dependency.Target.Kind
					== EAngelscriptCacheReferenceKind::EnvironmentSymbol
				|| Dependency.Target.Kind
					== EAngelscriptCacheReferenceKind::CanonicalName
				|| Dependency.Target.Kind
					== EAngelscriptCacheReferenceKind::StringLiteral
				|| (Dependency.Target.Kind
						== EAngelscriptCacheReferenceKind::ScriptFunction
					&& IsSelectedModuleReference(*Interface, Dependency.Target))
				|| (Dependency.Target.Kind
						== EAngelscriptCacheReferenceKind::ScriptType
					&& IsSelectedModuleReference(*Interface, Dependency.Target));
		}
		const bool bGraphSupportedObjectType =
			(Type.TypeKind == EAngelscriptCachedTypeKind::Class
				|| Type.TypeKind == EAngelscriptCachedTypeKind::Struct
				|| Type.TypeKind == EAngelscriptCachedTypeKind::Interface)
			&& bGraphSupportedRelations
			&& bGraphSupportedLayoutInputs
			&& bGraphSupportedDependencies
			&& bGraphSupportedMethodTopology
			&& !Type.KindPayload.Enum.IsSet()
			&& !Type.KindPayload.Callable.IsSet()
			&& !Type.KindPayload.Typedef.IsSet();
		// Enum payload identity and ordered-enumerator authority were already
		// recomputed by the sole TypeSchema decoder. At graph scope the remaining
		// authority is the exact Required declaration/link match above. A simple
		// enum therefore needs no current-layout resolver and is safe to retain as
		// an immutable selected-module type.
		const bool bGraphSupportedEnum =
			Type.TypeKind == EAngelscriptCachedTypeKind::Enum
			&& Type.KindPayload.Enum.IsSet()
			&& !Type.KindPayload.Enum->OrderedEnumerators.IsEmpty()
			&& !Type.KindPayload.Callable.IsSet()
			&& !Type.KindPayload.Typedef.IsSet()
			&& Type.Relations.IsEmpty()
			&& Type.LayoutInputs.IsEmpty()
			&& Type.OrderedProperties.IsEmpty()
			&& Type.OrderedMethods.IsEmpty()
			&& Type.VirtualFunctionTable.IsEmpty()
			&& Type.OrderedBehaviorSlots.IsEmpty()
			&& Type.Dependencies.IsEmpty()
			&& Type.Reflection.OrderedUFunctionMembers.IsEmpty();
		const bool bGraphSupportedType =
			bGraphSupportedObjectType || bGraphSupportedEnum;
		if (!bGraphSupportedType)
		{
			return GraphFailure(EAngelscriptCacheValidationError::MissingCoverage,
				TypeSchemaOffset(*TypeRecord,
					EAngelscriptTypeSchemaCapturedField::TypeKind));
		}
	}
	if (MatchedPropertyDeclarationCount != PropertyDeclarations.Num())
	{
		return GraphFailure(EAngelscriptCacheValidationError::MissingCoverage,
			PropertyDeclarations.IsEmpty() ? 0
				: InterfaceOffset(*InterfaceRecord,
					EAngelscriptModuleInterfaceCapturedField::Declaration,
					PropertyDeclarations[0].DeclarationOrdinal));
	}

	// Step 5 compares the complete local global declaration set with ModuleState
	// by stable GlobalKey. ModuleState storage order is independent of interface
	// declaration order, so the published stable index retains each storage
	// ordinal while this merge remains O(n log n), never pairwise.
	int32 GlobalDeclarationOrdinal = 0;
	int32 StateGlobalKeyOrdinal = 0;
	while (GlobalDeclarationOrdinal < GlobalDeclarations.Num()
		&& StateGlobalKeyOrdinal < Candidate.GlobalOrdinals.Num())
	{
		const FGlobalDeclaration& Declared =
			GlobalDeclarations[GlobalDeclarationOrdinal];
		const FAngelscriptCacheValidatedGlobalOrdinal& Stored =
			Candidate.GlobalOrdinals[StateGlobalKeyOrdinal];
		if (Declared.GlobalKey == Stored.GlobalKey)
		{
			++GlobalDeclarationOrdinal;
			++StateGlobalKeyOrdinal;
			continue;
		}
		if (Declared.GlobalKey.Hash < Stored.GlobalKey.Hash)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				InterfaceOffset(*InterfaceRecord,
					EAngelscriptModuleInterfaceCapturedField::Declaration,
					Declared.DeclarationOrdinal));
		}
		return GraphFailure(
			EAngelscriptCacheValidationError::GlobalCoverageMismatch,
			ModuleStateOffset(*StateRecord,
				EAngelscriptModuleStateCapturedField::GlobalKey,
				Stored.ModuleStateGlobalOrdinal));
	}
	if (GlobalDeclarationOrdinal < GlobalDeclarations.Num())
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::GlobalCoverageMismatch,
			InterfaceOffset(*InterfaceRecord,
				EAngelscriptModuleInterfaceCapturedField::Declaration,
				GlobalDeclarations[GlobalDeclarationOrdinal].DeclarationOrdinal));
	}
	if (StateGlobalKeyOrdinal < Candidate.GlobalOrdinals.Num())
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::GlobalCoverageMismatch,
			ModuleStateOffset(*StateRecord,
				EAngelscriptModuleStateCapturedField::GlobalKey,
				Candidate.GlobalOrdinals[StateGlobalKeyOrdinal]
					.ModuleStateGlobalOrdinal));
	}
	int32 GlobalConstantHardValueOrdinal = 0;
	const uint32 ConstGlobalTrait = static_cast<uint32>(
		EAngelscriptCachedDeclarationTraitFlags::Const);
	for (int32 StableGlobalOrdinal = 0;
		StableGlobalOrdinal < GlobalDeclarations.Num(); ++StableGlobalOrdinal)
	{
		const FGlobalDeclaration& Declared =
			GlobalDeclarations[StableGlobalOrdinal];
		const FAngelscriptCachedDeclaration& Declaration =
			Interface->Declarations[Declared.DeclarationOrdinal];
		const FAngelscriptCacheValidatedGlobalOrdinal& Published =
			Candidate.GlobalOrdinals[StableGlobalOrdinal];
		const FAngelscriptCachedGlobalSchema& Global =
			State->OrderedGlobals[Published.ModuleStateGlobalOrdinal];
		if (Declaration.EntityKind
				!= EAngelscriptArtifactEntityKind::GlobalVariable
			|| !Declaration.DeclaredType.IsSet()
			|| Global.CanonicalNamespace != Declaration.CanonicalNamespace
			|| Global.CanonicalName != Declaration.CanonicalName
			|| !DataTypesEqual(
				Global.Type, Declaration.DeclaredType.GetValue())
			|| Global.GlobalTraitFlags != Declaration.TraitFlags)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::Global,
					Published.ModuleStateGlobalOrdinal));
		}
		if (Global.InitializationKind
			== EAngelscriptCachedGlobalInitializationKind::Default)
		{
			if (Global.CleanupPolicy
				!= EAngelscriptCachedGlobalCleanupPolicy::None)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::MissingCoverage,
					ModuleStateOffset(*StateRecord,
						EAngelscriptModuleStateCapturedField::GlobalCleanupPolicy,
						Published.ModuleStateGlobalOrdinal));
			}
			continue;
		}
		if (Global.InitializationKind
			!= EAngelscriptCachedGlobalInitializationKind::PureConstant)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::MissingCoverage,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::GlobalInitializationKind,
					Published.ModuleStateGlobalOrdinal));
		}
		if (Global.CleanupPolicy
				!= EAngelscriptCachedGlobalCleanupPolicy::None
			|| (Global.GlobalTraitFlags & ConstGlobalTrait) == 0)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::GlobalInitializationKind,
					Published.ModuleStateGlobalOrdinal));
		}
		if (!State->HardValues.IsValidIndex(GlobalConstantHardValueOrdinal))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::GlobalInitializationKind,
					Published.ModuleStateGlobalOrdinal));
		}

		const uint32 HardValueOrdinal =
			static_cast<uint32>(GlobalConstantHardValueOrdinal);
		const FAngelscriptCachedHardValue& HardValue =
			State->HardValues[GlobalConstantHardValueOrdinal];
		if (HardValue.HardValueKind
			!= EAngelscriptCachedHardValueKind::GlobalConstant)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::HardValueKind,
					HardValueOrdinal));
		}
		if (HardValue.Owner.Kind
			!= EAngelscriptCacheReferenceKind::ScriptGlobal)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::
						HardValueOwnerReferenceKind,
					HardValueOrdinal));
		}
		if (!(HardValue.Owner.StableKey == Declared.GlobalKey.Hash))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::HardValueOwnerStableKey,
					HardValueOrdinal));
		}
		if (!(HardValue.Owner.ExpectedAbi == Declaration.SignatureHash))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::HardValueOwnerExpectedAbi,
					HardValueOrdinal));
		}
		if (!DataTypesEqual(HardValue.Type, Global.Type))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GlobalCoverageMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::HardValueTypeNode,
					HardValueOrdinal, 0));
		}
		++GlobalConstantHardValueOrdinal;
	}
	if (GlobalConstantHardValueOrdinal != State->HardValues.Num())
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::GlobalCoverageMismatch,
			ModuleStateOffset(*StateRecord,
				EAngelscriptModuleStateCapturedField::HardValue,
				static_cast<uint32>(GlobalConstantHardValueOrdinal)));
	}

	// The first ModuleState initializer vertical accepts the compiler-generated
	// module initializer. Declaration, unit and ExecuteInitializer target sets
	// are exact-equal by stable FunctionKey; the action owns execution order and
	// ABI, while the unit owns the opaque payload.
	int32 InitializerDeclarationOrdinal = 0;
	int32 InitializerUnitOrdinal = 0;
	while (InitializerDeclarationOrdinal < InitializerDeclarations.Num()
		&& InitializerUnitOrdinal < Candidate.InitializerOrdinals.Num())
	{
		const FInitializerDeclaration& Declared =
			InitializerDeclarations[InitializerDeclarationOrdinal];
		const FAngelscriptCacheValidatedInitializerOrdinal& Stored =
			Candidate.InitializerOrdinals[InitializerUnitOrdinal];
		if (Declared.InitializerKey == Stored.InitializerKey)
		{
			++InitializerDeclarationOrdinal;
			++InitializerUnitOrdinal;
			continue;
		}
		if (Declared.InitializerKey.Hash < Stored.InitializerKey.Hash)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
				InterfaceOffset(*InterfaceRecord,
					EAngelscriptModuleInterfaceCapturedField::Declaration,
					Declared.DeclarationOrdinal));
		}
		return GraphFailure(
			EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
			ModuleStateOffset(*StateRecord,
				EAngelscriptModuleStateCapturedField::InitializerKey,
				Stored.UnitOrdinal));
	}
	if (InitializerDeclarationOrdinal < InitializerDeclarations.Num())
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
			InterfaceOffset(*InterfaceRecord,
				EAngelscriptModuleInterfaceCapturedField::Declaration,
				InitializerDeclarations[InitializerDeclarationOrdinal]
					.DeclarationOrdinal));
	}
	if (InitializerUnitOrdinal < Candidate.InitializerOrdinals.Num())
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
			ModuleStateOffset(*StateRecord,
				EAngelscriptModuleStateCapturedField::InitializerKey,
				Candidate.InitializerOrdinals[InitializerUnitOrdinal]
					.UnitOrdinal));
	}
	const uint32 GeneratedTrait = static_cast<uint32>(
		EAngelscriptCachedDeclarationTraitFlags::Generated);
	for (int32 StableInitializerOrdinal = 0;
		StableInitializerOrdinal < InitializerDeclarations.Num();
		++StableInitializerOrdinal)
	{
		const FInitializerDeclaration& Declared =
			InitializerDeclarations[StableInitializerOrdinal];
		const FAngelscriptCachedDeclaration& Declaration =
			Interface->Declarations[Declared.DeclarationOrdinal];
		const FAngelscriptCacheValidatedInitializerOrdinal& Published =
			Candidate.InitializerOrdinals[StableInitializerOrdinal];
		const FAngelscriptCachedInitializerUnit& Initializer =
			State->Initializers[Published.UnitOrdinal];
		const FAngelscriptCachedInitializationAction& Action =
			State->OrderedInitializationActions[
				Published.ExecuteActionOrdinal];

		const bool bVoidCallable = Declaration.DeclaredType.IsSet()
			&& Declaration.DeclaredType->Kind
				== EAngelscriptCachedDataTypeKind::Primitive
			&& Declaration.DeclaredType->Primitive
				== EAngelscriptCachedPrimitiveType::Void
			&& Declaration.DeclaredType->QualifierFlags == 0
			&& !Declaration.DeclaredType->TypeReference.IsSet()
			&& Declaration.DeclaredType->OrderedSubTypes.IsEmpty();
		if (Declared.EntityKind
				!= EAngelscriptArtifactEntityKind::ModuleInitializer
			|| Initializer.InitializerKind
				!= EAngelscriptCachedInitializerKind::Module
			|| Initializer.OwnerGlobal.IsSet()
			|| Declaration.OwnerKind != EAngelscriptFunctionOwnerKind::Module
			|| !(Declaration.OwnerKey == Snapshot->ModuleKey.Hash)
			|| Declaration.BodyCoverage
				!= EAngelscriptCacheBodyCoverage::Forbidden
			|| !bVoidCallable
			|| !Declaration.OrderedParameters.IsEmpty()
			|| Declaration.TraitFlags != GeneratedTrait
			|| Declaration.ReflectionFlags != 0
			|| !Declaration.CanonicalIdentityTraits.IsEmpty()
			|| !Declaration.Metadata.IsEmpty()
			|| !Declaration.Slots.IsEmpty()
			|| Action.ActionKind
				!= EAngelscriptCachedInitializationActionKind::ExecuteInitializer
			|| Action.Target.Kind
				!= EAngelscriptCacheReferenceKind::ScriptFunction
			|| !(Action.Target.StableKey == Initializer.InitializerKey.Hash)
			|| !(Action.Target.ExpectedAbi == Declaration.SignatureHash))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::InitializerOwnershipMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::Initializer,
					Published.UnitOrdinal));
		}

		const FAngelscriptCacheOpaquePayloadSummary& Summary =
			Candidate.OpaqueSummaries[Published.SummaryOrdinal];
		if (!(Summary.ValidatedPayloadHash
			== Initializer.InitializerExecutionHash))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::InitializerExecutionHash,
					Published.UnitOrdinal));
		}
		if (!Summary.OrderedRelocations.IsEmpty()
			|| !Summary.ExactDebugSources.IsEmpty()
			|| !Summary.OwnedCanonicalBytes.IsEmpty())
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::RelocationDependencyMismatch,
				ModuleStateOffset(*StateRecord,
					EAngelscriptModuleStateCapturedField::
						InitializerCanonicalExecutionPayload,
					Published.UnitOrdinal));
		}
	}

	// Step 7 compares the complete required-body set with the keyed snapshot
	// links. Forbidden functions remain in the published declaration index but
	// may not own a body.
	int32 FunctionDeclarationOrdinal = 0;
	int32 BodyLinkOrdinal = 0;
	while (FunctionDeclarationOrdinal < FunctionDeclarations.Num()
		&& BodyLinkOrdinal < Snapshot->FunctionBodies.Num())
	{
		FFunctionDeclaration& Declaration =
			FunctionDeclarations[FunctionDeclarationOrdinal];
		const FAngelscriptCachedFunctionBodyLink& Link =
			Snapshot->FunctionBodies[BodyLinkOrdinal];
		if (Declaration.FunctionKey == Link.FunctionKey)
		{
			if (Declaration.BodyCoverage
				!= EAngelscriptCacheBodyCoverage::Required)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::UnexpectedRecord,
					SnapshotOffset(*RootRecord,
						EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkFunctionKey,
						static_cast<uint32>(BodyLinkOrdinal)));
			}
			Declaration.BodyLinkOrdinal = static_cast<uint32>(BodyLinkOrdinal);
			++FunctionDeclarationOrdinal;
			++BodyLinkOrdinal;
			continue;
		}
		if (Declaration.FunctionKey.Hash < Link.FunctionKey.Hash)
		{
			if (Declaration.BodyCoverage
				== EAngelscriptCacheBodyCoverage::Required)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::MissingCoverage,
					InterfaceOffset(*InterfaceRecord,
						EAngelscriptModuleInterfaceCapturedField::Declaration,
						Declaration.DeclarationOrdinal));
			}
			++FunctionDeclarationOrdinal;
			continue;
		}
		return GraphFailure(
			EAngelscriptCacheValidationError::UndeclaredEntity,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkFunctionKey,
				static_cast<uint32>(BodyLinkOrdinal)));
	}
	while (FunctionDeclarationOrdinal < FunctionDeclarations.Num())
	{
		const FFunctionDeclaration& Declaration =
			FunctionDeclarations[FunctionDeclarationOrdinal++];
		if (Declaration.BodyCoverage == EAngelscriptCacheBodyCoverage::Required)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::MissingCoverage,
				InterfaceOffset(*InterfaceRecord,
					EAngelscriptModuleInterfaceCapturedField::Declaration,
					Declaration.DeclarationOrdinal));
		}
	}
	if (BodyLinkOrdinal < Snapshot->FunctionBodies.Num())
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::UndeclaredEntity,
			SnapshotOffset(*RootRecord,
				EAngelscriptModuleSnapshotCapturedField::FunctionBodyLinkFunctionKey,
				static_cast<uint32>(BodyLinkOrdinal)));
	}

	for (const FFunctionDeclaration& FunctionDeclaration : FunctionDeclarations)
	{
		FAngelscriptCacheValidatedFunctionOrdinal& PublishedFunction =
			Candidate.FunctionOrdinals.AddDefaulted_GetRef();
		PublishedFunction.FunctionKey = FunctionDeclaration.FunctionKey;
		PublishedFunction.DeclarationOrdinal = FunctionDeclaration.DeclarationOrdinal;
		if (!FunctionDeclaration.BodyLinkOrdinal.IsSet())
		{
			continue;
		}

		const uint32 FunctionBodyLinkOrdinal =
			FunctionDeclaration.BodyLinkOrdinal.GetValue();
		const FAngelscriptCachedFunctionBodyLink& Link =
			Snapshot->FunctionBodies[FunctionBodyLinkOrdinal];
		const FLinkedFunctionBody& Linked =
			LinkedFunctionBodies[FunctionBodyLinkOrdinal];
		const FAngelscriptDecodedCacheRecordHandle& BodyRecord =
			LocallyValidatedRecords[Linked.BodyInputOrdinal];
		const FAngelscriptCachedFunctionBody& Body =
			*BodyRecord->TryGetFunctionBody();
		const FAngelscriptCachedDeclaration& Declaration =
			Interface->Declarations[FunctionDeclaration.DeclarationOrdinal];

		if (!(Body.ModuleKey == Snapshot->ModuleKey))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::CrossModuleOwner,
				FunctionBodyOffset(*BodyRecord,
					EAngelscriptFunctionBodyCapturedField::ModuleKey));
		}
		if (!(Body.Identity.FunctionKey == Link.FunctionKey)
			|| !(Body.Identity.FunctionKey == FunctionDeclaration.FunctionKey))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				FunctionBodyOffset(*BodyRecord,
					EAngelscriptFunctionBodyCapturedField::IdentityFunctionKey));
		}
		if (!(Body.ExpectedDeclarationAbi == Declaration.SignatureHash))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				FunctionBodyOffset(*BodyRecord,
					EAngelscriptFunctionBodyCapturedField::ExpectedDeclarationAbi));
		}
		const TOptional<EAngelscriptCachedFunctionInvocationKind> ExpectedInvocation =
			InvocationKindForEntity(FunctionDeclaration.EntityKind);
		if (!ExpectedInvocation.IsSet()
			|| Body.InvocationKind != ExpectedInvocation.GetValue())
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::InvocationKindMismatch,
				FunctionBodyOffset(*BodyRecord,
					EAngelscriptFunctionBodyCapturedField::InvocationKind));
		}
		if (!(Body.Identity.Profile.Hash == State->Profile.Hash))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::ProfileGraphMismatch,
				FunctionBodyOffset(*BodyRecord,
					EAngelscriptFunctionBodyCapturedField::IdentityProfile));
		}

		const uint32 BodySummaryOrdinal =
			static_cast<uint32>(State->Initializers.Num())
				+ FunctionBodyLinkOrdinal;
		const FAngelscriptCacheOpaquePayloadSummary& BodySummary =
			Candidate.OpaqueSummaries[BodySummaryOrdinal];
		if (!(BodySummary.ValidatedPayloadHash
			== Body.Identity.Content.Execution))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
				FunctionBodyOffset(*BodyRecord,
					EAngelscriptFunctionBodyCapturedField::IdentityContentExecution));
		}
		if (!RelocationsAreDependencySubset(
				BodySummary.OrderedRelocations, Body.ActualDependencies)
			|| !BodySummary.ExactDebugSources.IsEmpty()
			|| !BodySummary.OwnedCanonicalBytes.IsEmpty())
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::RelocationDependencyMismatch,
				FunctionBodyOffset(*BodyRecord,
					EAngelscriptFunctionBodyCapturedField::ActualDependencies));
		}

		PublishedFunction.BodyRecordOrdinal =
			BodyRecordBase + FunctionBodyLinkOrdinal;
		PublishedFunction.BodySummaryOrdinal = BodySummaryOrdinal;
		if (Linked.DebugInputOrdinal.IsSet())
		{
			const FAngelscriptDecodedCacheRecordHandle& DebugRecord =
				LocallyValidatedRecords[Linked.DebugInputOrdinal.GetValue()];
			if (Linked.bDuplicateDebugOwner)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::DuplicateDebugOwner,
					FunctionBodyOffset(*BodyRecord,
						EAngelscriptFunctionBodyCapturedField::DebugSidecar));
			}
			const FAngelscriptCachedDebugSidecar& Debug =
				*DebugRecord->TryGetDebugSidecar();
			if (!(Debug.FunctionKey == Body.Identity.FunctionKey))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::DebugLinkMismatch,
					DebugSidecarOffset(*DebugRecord,
						EAngelscriptDebugSidecarCapturedField::FunctionKey));
			}
			if (!(Debug.Profile.Hash == Body.Identity.Profile.Hash))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::ProfileGraphMismatch,
					DebugSidecarOffset(*DebugRecord,
						EAngelscriptDebugSidecarCapturedField::Profile));
			}
			if (!(Debug.DebugHash == Body.Identity.Content.Debug))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::DebugLinkMismatch,
					DebugSidecarOffset(*DebugRecord,
						EAngelscriptDebugSidecarCapturedField::DebugHash));
			}

			check(Linked.DebugSequenceOrdinal.IsSet());
			const uint32 DebugSequenceOrdinal =
				Linked.DebugSequenceOrdinal.GetValue();
			const uint32 DebugSummaryOrdinal =
				static_cast<uint32>(State->Initializers.Num())
					+ static_cast<uint32>(Snapshot->FunctionBodies.Num())
					+ DebugSequenceOrdinal;
			const FAngelscriptCacheOpaquePayloadSummary& DebugSummary =
				Candidate.OpaqueSummaries[DebugSummaryOrdinal];
			if (!(DebugSummary.ValidatedPayloadHash == Debug.DebugHash))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::OpaquePayloadHashMismatch,
					DebugSidecarOffset(*DebugRecord,
						EAngelscriptDebugSidecarCapturedField::DebugHash));
			}
			if (!DebugSummary.OrderedRelocations.IsEmpty()
				|| !DebugSummary.OwnedCanonicalBytes.IsEmpty())
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::OpaquePayloadMalformed,
					DebugSidecarOffset(*DebugRecord,
						EAngelscriptDebugSidecarCapturedField::CanonicalDebugPayload),
					EAngelscriptCacheValidationStage::OpaqueCodec);
			}
			if (!DebugSourcesEqual(Debug.Sources, DebugSummary.ExactDebugSources))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::DebugSourceMismatch,
					DebugSidecarOffset(*DebugRecord,
						EAngelscriptDebugSidecarCapturedField::Sources));
			}
			for (int32 SourceOrdinal = 0;
				SourceOrdinal < Debug.Sources.Num(); ++SourceOrdinal)
			{
				if (!SourceIndexContainsFile(
					*SourceIndex, Debug.Sources[SourceOrdinal].SourceFileKey))
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::DebugSourceMismatch,
						DebugSidecarOffset(*DebugRecord,
							EAngelscriptDebugSidecarCapturedField::SourceFileKey,
							static_cast<uint32>(SourceOrdinal)));
				}
			}

			PublishedFunction.DebugRecordOrdinal = BodyRecordBase
				+ static_cast<uint32>(LinkedFunctionBodies.Num())
				+ DebugSequenceOrdinal;
			PublishedFunction.DebugSummaryOrdinal = DebugSummaryOrdinal;
		}
	}
	for (const FAngelscriptCachedDeclaration& Declaration : Interface->Declarations)
	{
		if (Declaration.DeclarationKind != EAngelscriptCacheDeclarationKind::Type
			&& Declaration.DeclarationKind
				!= EAngelscriptCacheDeclarationKind::Function
			&& Declaration.DeclarationKind
				!= EAngelscriptCacheDeclarationKind::Global
			&& Declaration.DeclarationKind
				!= EAngelscriptCacheDeclarationKind::Property)
		{
			return GraphFailure(EAngelscriptCacheValidationError::MissingCoverage);
		}
	}
	if (!Interface->Imports.IsEmpty())
	{
		return GraphFailure(EAngelscriptCacheValidationError::MissingCoverage);
	}
	if (!State->OrderedPostInitFunctions.IsEmpty()
		|| !State->Dependencies.IsEmpty())
	{
		return GraphFailure(EAngelscriptCacheValidationError::GlobalCoverageMismatch);
	}

	// Step 9 resolves every currently admitted owning record through one immutable
	// dependency rule before source/profile/current eligibility. The same callback
	// is reused in canonical owner order so adding an owner cannot silently acquire
	// a different selected-module interpretation.
	const auto ValidateImmutableDependency = [
		&Interface, &State, &RequiredTypes, &FunctionDeclarations,
		&GlobalDeclarations, &PropertyDeclarations, &Candidate, BodyRecordBase](
			const FAngelscriptCacheSemanticDependency& Dependency,
			const uint64 StableKeyOffset,
			const uint64 ExpectedAbiOffset,
			const uint64 ExpectedContentOffset)
		-> FAngelscriptCacheValidationResult
	{
		if (!IsSelectedModuleReference(*Interface, Dependency.Target))
		{
			return {};
		}

		if (Dependency.Target.Kind
			== EAngelscriptCacheReferenceKind::ScriptModule)
		{
			return Dependency.Target.ExpectedAbi == Interface->InterfaceAbi
				&& !Dependency.ExpectedContentOrValue.IsSet()
				? FAngelscriptCacheValidationResult{}
				: GraphFailure(EAngelscriptCacheValidationError::GraphAbiMismatch,
					ExpectedAbiOffset);
		}

		if (Dependency.Target.Kind
			== EAngelscriptCacheReferenceKind::ScriptType)
		{
			const FRequiredTypeDeclaration* TargetType =
				FindRequiredTypeDeclaration(
					RequiredTypes, Dependency.Target.StableKey);
			if (TargetType == nullptr)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					StableKeyOffset);
			}
			const FAngelscriptCachedDeclaration& TargetDeclaration =
				Interface->Declarations[TargetType->DeclarationOrdinal];
			if (!(Dependency.Target.ExpectedAbi
				== TargetDeclaration.SignatureHash))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					ExpectedAbiOffset);
			}
			if (!Dependency.ExpectedContentOrValue.IsSet())
			{
				return {};
			}
			if (Dependency.Kind
				!= EAngelscriptCacheSemanticDependencyKind::ValueLayout)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					ExpectedContentOffset);
			}

			int32 First = 0;
			int32 Last = Candidate.TypeOrdinals.Num();
			while (First < Last)
			{
				const int32 Middle = First + (Last - First) / 2;
				if (Candidate.TypeOrdinals[Middle].TypeKey.Hash
					< Dependency.Target.StableKey)
				{
					First = Middle + 1;
				}
				else
				{
					Last = Middle;
				}
			}
			if (!Candidate.TypeOrdinals.IsValidIndex(First)
				|| !(Candidate.TypeOrdinals[First].TypeKey.Hash
					== Dependency.Target.StableKey))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					StableKeyOffset);
			}
			const FAngelscriptCachedTypeSchema& TargetSchema =
				*Candidate.ReachableRecords[
					Candidate.TypeOrdinals[First].TypeSchemaRecordOrdinal]
					->TryGetTypeSchema();
			return Dependency.ExpectedContentOrValue.GetValue()
				== TargetSchema.Layout.TypeLayoutHash
				? FAngelscriptCacheValidationResult{}
				: GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					ExpectedContentOffset);
		}

		if (Dependency.Target.Kind
			== EAngelscriptCacheReferenceKind::ScriptProperty)
		{
			FPropertyDeclaration* TargetProperty = FindPropertyDeclaration(
				PropertyDeclarations,
				FAngelscriptStablePropertyKey{Dependency.Target.StableKey});
			if (TargetProperty == nullptr)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					StableKeyOffset);
			}
			const FAngelscriptCachedDeclaration& TargetDeclaration =
				Interface->Declarations[TargetProperty->DeclarationOrdinal];
			if (!(Dependency.Target.ExpectedAbi
				== TargetDeclaration.SignatureHash))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					ExpectedAbiOffset);
			}
			if (!Dependency.ExpectedContentOrValue.IsSet())
			{
				return {};
			}
			if (Dependency.Kind
					!= EAngelscriptCacheSemanticDependencyKind::PropertyLayout
				|| !TargetProperty->TypeSchemaRecordOrdinal.IsSet()
				|| !TargetProperty->PropertySchemaOrdinal.IsSet())
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					ExpectedContentOffset);
			}
			const FAngelscriptCachedTypeSchema& OwnerSchema =
				*Candidate.ReachableRecords[
					TargetProperty->TypeSchemaRecordOrdinal.GetValue()]
					->TryGetTypeSchema();
			const uint32 PropertyOrdinal =
				TargetProperty->PropertySchemaOrdinal.GetValue();
			if (!OwnerSchema.OrderedProperties.IsValidIndex(
				static_cast<int32>(PropertyOrdinal)))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					StableKeyOffset);
			}
			return Dependency.ExpectedContentOrValue.GetValue()
				== OwnerSchema.OrderedProperties[PropertyOrdinal]
					.PropertyLayoutFingerprint
				? FAngelscriptCacheValidationResult{}
				: GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					ExpectedContentOffset);
		}

		if (Dependency.Target.Kind
			== EAngelscriptCacheReferenceKind::ScriptGlobal)
		{
			const FGlobalDeclaration* TargetGlobal = FindGlobalDeclaration(
				GlobalDeclarations, Dependency.Target.StableKey);
			if (TargetGlobal == nullptr)
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::WrongReferenceKind,
					StableKeyOffset);
			}
			const FAngelscriptCachedDeclaration& TargetDeclaration =
				Interface->Declarations[TargetGlobal->DeclarationOrdinal];
			if (!(Dependency.Target.ExpectedAbi
				== TargetDeclaration.SignatureHash))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					ExpectedAbiOffset);
			}
			if (!Dependency.ExpectedContentOrValue.IsSet())
			{
				return {};
			}

			int32 First = 0;
			int32 Last = Candidate.GlobalOrdinals.Num();
			while (First < Last)
			{
				const int32 Middle = First + (Last - First) / 2;
				if (Candidate.GlobalOrdinals[Middle].GlobalKey.Hash
					< Dependency.Target.StableKey)
				{
					First = Middle + 1;
				}
				else
				{
					Last = Middle;
				}
			}
			if (!Candidate.GlobalOrdinals.IsValidIndex(First)
				|| !(Candidate.GlobalOrdinals[First].GlobalKey.Hash
					== Dependency.Target.StableKey))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::GraphAbiMismatch,
					StableKeyOffset);
			}

			if (Dependency.Kind
				== EAngelscriptCacheSemanticDependencyKind::GlobalStorage)
			{
				const FAngelscriptCachedGlobalSchema& Global =
					State->OrderedGlobals[
						Candidate.GlobalOrdinals[First].ModuleStateGlobalOrdinal];
				return Dependency.ExpectedContentOrValue.GetValue()
					== Global.StorageLayoutFingerprint
					? FAngelscriptCacheValidationResult{}
					: GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						ExpectedContentOffset);
			}

			if (Dependency.Kind
				== EAngelscriptCacheSemanticDependencyKind::HardValue)
			{
				const FAngelscriptCachedHardValue* MatchingValue = nullptr;
				for (const FAngelscriptCachedHardValue& HardValue
					: State->HardValues)
				{
					if (HardValue.Owner.Kind
							== EAngelscriptCacheReferenceKind::ScriptGlobal
						&& HardValue.Owner.StableKey
							== Dependency.Target.StableKey)
					{
						if (MatchingValue != nullptr)
						{
							return GraphFailure(
								EAngelscriptCacheValidationError::DuplicateKey,
								StableKeyOffset);
						}
						MatchingValue = &HardValue;
					}
				}
				return MatchingValue != nullptr
					&& Dependency.ExpectedContentOrValue.GetValue()
						== MatchingValue->HardValueHash
					? FAngelscriptCacheValidationResult{}
					: GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						ExpectedContentOffset);
			}

			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				ExpectedContentOffset);
		}

		if (Dependency.Target.Kind
			!= EAngelscriptCacheReferenceKind::ScriptFunction)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				StableKeyOffset);
		}

		const FFunctionDeclaration* TargetFunction = FindFunctionDeclaration(
			FunctionDeclarations, Dependency.Target.StableKey);
		if (TargetFunction == nullptr)
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::WrongReferenceKind,
				StableKeyOffset);
		}
		const FAngelscriptCachedDeclaration& TargetDeclaration =
			Interface->Declarations[TargetFunction->DeclarationOrdinal];
		if (!(Dependency.Target.ExpectedAbi == TargetDeclaration.SignatureHash))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				ExpectedAbiOffset);
		}

		if (!Dependency.ExpectedContentOrValue.IsSet())
		{
			return {};
		}
		if (!TargetFunction->BodyLinkOrdinal.IsSet())
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch,
				ExpectedContentOffset);
		}
		const FAngelscriptCachedFunctionBody& TargetBody =
			*Candidate.ReachableRecords[
				BodyRecordBase + TargetFunction->BodyLinkOrdinal.GetValue()]
				->TryGetFunctionBody();
		return Dependency.ExpectedContentOrValue.GetValue()
			== TargetBody.Identity.Content.Execution
			? FAngelscriptCacheValidationResult{}
			: GraphFailure(EAngelscriptCacheValidationError::GraphAbiMismatch,
				ExpectedContentOffset);
	};

	for (int32 DependencyOrdinal = 0;
		DependencyOrdinal < Interface->Dependencies.Num(); ++DependencyOrdinal)
	{
		const FAngelscriptCacheValidationResult Result =
			ValidateImmutableDependency(
				Interface->Dependencies[DependencyOrdinal],
				InterfaceOffset(*InterfaceRecord,
					EAngelscriptModuleInterfaceCapturedField::
						DependencyTargetStableKey,
					static_cast<uint32>(DependencyOrdinal)),
				InterfaceOffset(*InterfaceRecord,
					EAngelscriptModuleInterfaceCapturedField::
						DependencyTargetExpectedAbi,
					static_cast<uint32>(DependencyOrdinal)),
				InterfaceOffset(*InterfaceRecord,
					EAngelscriptModuleInterfaceCapturedField::
						DependencyExpectedContentOrValue,
					static_cast<uint32>(DependencyOrdinal)));
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}

	for (int32 TypeOrdinal = 0;
		TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
	{
		const FAngelscriptDecodedCacheRecordHandle& TypeRecord =
			Candidate.ReachableRecords[3 + TypeOrdinal];
		const FAngelscriptCachedTypeSchema& Type =
			*TypeRecord->TryGetTypeSchema();
		for (int32 DependencyOrdinal = 0;
			DependencyOrdinal < Type.Dependencies.Num(); ++DependencyOrdinal)
		{
			const FAngelscriptCacheValidationResult Result =
				ValidateImmutableDependency(
					Type.Dependencies[DependencyOrdinal],
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::DependencyTarget,
						static_cast<uint32>(DependencyOrdinal)),
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::DependencyTarget,
						static_cast<uint32>(DependencyOrdinal)),
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::Dependency,
						static_cast<uint32>(DependencyOrdinal)));
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
	}

	for (int32 DependencyBodyOrdinal = 0;
		DependencyBodyOrdinal < Snapshot->FunctionBodies.Num();
		++DependencyBodyOrdinal)
	{
		const FAngelscriptDecodedCacheRecordHandle& BodyRecord =
			Candidate.ReachableRecords[BodyRecordBase + DependencyBodyOrdinal];
		const FAngelscriptCachedFunctionBody& Body =
			*BodyRecord->TryGetFunctionBody();
		for (int32 DependencyOrdinal = 0;
			DependencyOrdinal < Body.ActualDependencies.Num();
			++DependencyOrdinal)
		{
			const FAngelscriptCacheValidationResult Result =
				ValidateImmutableDependency(
					Body.ActualDependencies[DependencyOrdinal],
					FunctionBodyOffset(*BodyRecord,
						EAngelscriptFunctionBodyCapturedField::
							ActualDependencyTargetStableKey,
						static_cast<uint32>(DependencyOrdinal)),
					FunctionBodyOffset(*BodyRecord,
						EAngelscriptFunctionBodyCapturedField::
							ActualDependencyTargetExpectedAbi,
						static_cast<uint32>(DependencyOrdinal)),
					FunctionBodyOffset(*BodyRecord,
						EAngelscriptFunctionBodyCapturedField::
							ActualDependencyExpectedContentOrValue,
						static_cast<uint32>(DependencyOrdinal)));
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
	}

	// Selected-module BaseType and direct InlineValue ScriptType edges are closed
	// entirely by the immutable candidate. Resolve their independent numeric
	// witnesses to linked TypeSchemas, then prove the combined graph is a DAG. This
	// must happen before source, profile or current-environment eligibility so a
	// stale/forged local type graph can never be repaired by live engine state.
	const auto FindLocalTypeOrdinal = [&Candidate](
		const FAngelscriptHash256& StableKey) -> int32
	{
		int32 First = 0;
		int32 Last = Candidate.TypeOrdinals.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (Candidate.TypeOrdinals[Middle].TypeKey.Hash < StableKey)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return Candidate.TypeOrdinals.IsValidIndex(First)
			&& Candidate.TypeOrdinals[First].TypeKey.Hash == StableKey
			? First : INDEX_NONE;
	};

	uint64 LocalTypeEdgeCount = 0;
	for (int32 TypeOrdinal = 0;
		TypeOrdinal < Candidate.TypeOrdinals.Num(); ++TypeOrdinal)
	{
		const FAngelscriptCacheValidatedTypeOrdinal& PublishedType =
			Candidate.TypeOrdinals[TypeOrdinal];
		const FAngelscriptCachedTypeSchema& Type =
			*Candidate.ReachableRecords[
				PublishedType.TypeSchemaRecordOrdinal]->TryGetTypeSchema();
		for (const FAngelscriptCachedTypeLayoutInput& Input : Type.LayoutInputs)
		{
			if (Input.InputKind == EAngelscriptCachedTypeLayoutInputKind::BaseType
				&& IsSelectedModuleReference(*Interface, Input.Target))
			{
				++LocalTypeEdgeCount;
			}
		}
		for (const FAngelscriptCachedPropertySchema& Property :
			Type.OrderedProperties)
		{
			if (Property.StorageKind
					== EAngelscriptCachedPropertyStorageKind::InlineValue
				&& Property.Type.Kind
					== EAngelscriptCachedDataTypeKind::ScriptType
				&& Property.Type.TypeReference.IsSet()
				&& IsSelectedModuleReference(
					*Interface, Property.Type.TypeReference.GetValue()))
			{
				++LocalTypeEdgeCount;
			}
		}
	}
	if (LocalTypeEdgeCount > Limits.MaxArrayElements
		|| LocalTypeEdgeCount > Limits.MaxReferencesAndRelocations
		|| LocalTypeEdgeCount > static_cast<uint64>(MAX_int32)
		|| Candidate.TypeOrdinals.Num() == MAX_int32
		|| static_cast<uint64>(Candidate.TypeOrdinals.Num()) + 1u
			> Limits.MaxArrayElements)
	{
		return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
	}

	int32 LocalEdgeCapacity = 0;
	int32 LocalEdgeStartCapacity = 0;
	int32 LocalInDegreeCapacity = 0;
	int32 LocalQueueCapacity = 0;
	uint64 LocalEdgeBytes = 0;
	uint64 LocalEdgeStartBytes = 0;
	uint64 LocalInDegreeBytes = 0;
	uint64 LocalQueueBytes = 0;
	uint64 LocalTypeScratchBytes = 0;
	if (!TryCalculateArrayReserveBytes<FLocalTypeLayoutEdge>(
			static_cast<int32>(LocalTypeEdgeCount),
			LocalEdgeCapacity, LocalEdgeBytes)
		|| !TryCalculateArrayReserveBytes<uint32>(
			Candidate.TypeOrdinals.Num() + 1,
			LocalEdgeStartCapacity, LocalEdgeStartBytes)
		|| !TryCalculateArrayReserveBytes<uint32>(
			Candidate.TypeOrdinals.Num(),
			LocalInDegreeCapacity, LocalInDegreeBytes)
		|| !TryCalculateArrayReserveBytes<uint32>(
			Candidate.TypeOrdinals.Num(),
			LocalQueueCapacity, LocalQueueBytes)
		|| !TryAddBytes(LocalEdgeBytes, LocalTypeScratchBytes)
		|| !TryAddBytes(LocalEdgeStartBytes, LocalTypeScratchBytes)
		|| !TryAddBytes(LocalInDegreeBytes, LocalTypeScratchBytes)
		|| !TryAddBytes(LocalQueueBytes, LocalTypeScratchBytes))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	{
		FAngelscriptCacheTemporaryResidentReservation LocalTypeReservation;
		if (!Budget.TryReserveTemporaryDecoded(
			LocalTypeScratchBytes, Limits, LocalTypeReservation))
		{
			return GraphFailure(
				EAngelscriptCacheValidationError::BudgetExceeded);
		}
		TArray<FLocalTypeLayoutEdge> LocalEdges;
		TArray<uint32> LocalEdgeStarts;
		TArray<uint32> LocalInDegrees;
		TArray<uint32> LocalQueue;
		LocalEdges.Reserve(LocalEdgeCapacity);
		LocalEdgeStarts.Reserve(LocalEdgeStartCapacity);
		LocalInDegrees.Reserve(LocalInDegreeCapacity);
		LocalQueue.Reserve(LocalQueueCapacity);
		if (static_cast<uint64>(LocalEdges.GetAllocatedSize()) != LocalEdgeBytes
			|| static_cast<uint64>(LocalEdgeStarts.GetAllocatedSize())
				!= LocalEdgeStartBytes
			|| static_cast<uint64>(LocalInDegrees.GetAllocatedSize())
				!= LocalInDegreeBytes
			|| static_cast<uint64>(LocalQueue.GetAllocatedSize())
				!= LocalQueueBytes)
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}
		for (int32 TypeOrdinal = 0;
			TypeOrdinal < Candidate.TypeOrdinals.Num(); ++TypeOrdinal)
		{
			LocalInDegrees.Add(0);
		}

		for (int32 TypeOrdinal = 0;
			TypeOrdinal < Candidate.TypeOrdinals.Num(); ++TypeOrdinal)
		{
			LocalEdgeStarts.Add(static_cast<uint32>(LocalEdges.Num()));
			const FAngelscriptCacheValidatedTypeOrdinal& PublishedType =
				Candidate.TypeOrdinals[TypeOrdinal];
			const FAngelscriptDecodedCacheRecordHandle& TypeRecord =
				Candidate.ReachableRecords[PublishedType.TypeSchemaRecordOrdinal];
			const FAngelscriptCachedTypeSchema& Type =
				*TypeRecord->TryGetTypeSchema();
			bool bHasLocalBase = false;
			for (int32 InputOrdinal = 0;
				InputOrdinal < Type.LayoutInputs.Num(); ++InputOrdinal)
			{
				const FAngelscriptCachedTypeLayoutInput& Input =
					Type.LayoutInputs[InputOrdinal];
				if (Input.InputKind
						!= EAngelscriptCachedTypeLayoutInputKind::BaseType
					|| !IsSelectedModuleReference(*Interface, Input.Target))
				{
					continue;
				}
				if (bHasLocalBase)
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::LayoutInput,
							static_cast<uint32>(InputOrdinal)));
				}
				bHasLocalBase = true;

				const int32 TargetOrdinal =
					FindLocalTypeOrdinal(Input.Target.StableKey);
				if (TargetOrdinal == INDEX_NONE)
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::LayoutInputTarget,
							static_cast<uint32>(InputOrdinal)));
				}
				const FAngelscriptCacheValidatedTypeOrdinal& TargetPublished =
					Candidate.TypeOrdinals[TargetOrdinal];
				const FAngelscriptCachedTypeSchema& TargetType =
					*Candidate.ReachableRecords[
						TargetPublished.TypeSchemaRecordOrdinal]->TryGetTypeSchema();
				if (TargetType.TypeKind != EAngelscriptCachedTypeKind::Class
					|| TargetType.Layout.SemanticSize > MAX_uint32
					|| !Input.BoundaryContribution.IsSet()
					|| Input.BoundaryContribution.GetValue()
						!= static_cast<uint32>(TargetType.Layout.SemanticSize)
					|| !Input.AlignmentContribution.IsSet()
					|| Input.AlignmentContribution.GetValue()
						!= TargetType.Layout.SemanticAlignment)
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::LayoutInput,
							static_cast<uint32>(InputOrdinal)));
				}
				LocalEdges.Add({static_cast<uint32>(TargetOrdinal),
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::LayoutInputTarget,
						static_cast<uint32>(InputOrdinal))});
			}

			for (int32 PropertyOrdinal = 0;
				PropertyOrdinal < Type.OrderedProperties.Num(); ++PropertyOrdinal)
			{
				const FAngelscriptCachedPropertySchema& Property =
					Type.OrderedProperties[PropertyOrdinal];
				if (Property.StorageKind
						!= EAngelscriptCachedPropertyStorageKind::InlineValue
					|| Property.Type.Kind
						!= EAngelscriptCachedDataTypeKind::ScriptType
					|| !Property.Type.TypeReference.IsSet()
					|| !IsSelectedModuleReference(
						*Interface, Property.Type.TypeReference.GetValue()))
				{
					continue;
				}
				const int32 TargetOrdinal = FindLocalTypeOrdinal(
					Property.Type.TypeReference->StableKey);
				if (TargetOrdinal == INDEX_NONE)
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::PropertyType,
							static_cast<uint32>(PropertyOrdinal)));
				}
				const FAngelscriptCacheValidatedTypeOrdinal& TargetPublished =
					Candidate.TypeOrdinals[TargetOrdinal];
				const FAngelscriptCachedTypeSchema& TargetType =
					*Candidate.ReachableRecords[
						TargetPublished.TypeSchemaRecordOrdinal]->TryGetTypeSchema();
				const uint32 ValueTypeFlag = static_cast<uint32>(
					EAngelscriptCachedTypeSemanticFlags::ValueType);
				if ((TargetType.TypeSemanticFlags & ValueTypeFlag) == 0
					|| TargetType.Layout.SemanticSize > MAX_uint32
					|| Property.SemanticStorageSize
						!= static_cast<uint32>(TargetType.Layout.SemanticSize)
					|| Property.SemanticStorageAlignment
						!= TargetType.Layout.SemanticAlignment)
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::OrderedProperty,
							static_cast<uint32>(PropertyOrdinal)));
				}
				LocalEdges.Add({static_cast<uint32>(TargetOrdinal),
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::PropertyType,
						static_cast<uint32>(PropertyOrdinal))});
			}
		}
		LocalEdgeStarts.Add(static_cast<uint32>(LocalEdges.Num()));
		if (static_cast<uint64>(LocalEdges.Num()) != LocalTypeEdgeCount)
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}

		for (const FLocalTypeLayoutEdge& Edge : LocalEdges)
		{
			if (!LocalInDegrees.IsValidIndex(
				static_cast<int32>(Edge.TargetTypeOrdinal))
				|| LocalInDegrees[Edge.TargetTypeOrdinal] == MAX_uint32)
			{
				return GraphFailure(EAngelscriptCacheValidationError::Overflow);
			}
			++LocalInDegrees[Edge.TargetTypeOrdinal];
		}
		for (int32 TypeOrdinal = 0;
			TypeOrdinal < LocalInDegrees.Num(); ++TypeOrdinal)
		{
			if (LocalInDegrees[TypeOrdinal] == 0)
			{
				LocalQueue.Add(static_cast<uint32>(TypeOrdinal));
			}
		}

		int32 QueueReadOrdinal = 0;
		uint32 ProcessedTypeCount = 0;
		while (QueueReadOrdinal < LocalQueue.Num())
		{
			const uint32 TypeOrdinal = LocalQueue[QueueReadOrdinal++];
			++ProcessedTypeCount;
			const uint32 EdgeBegin = LocalEdgeStarts[TypeOrdinal];
			const uint32 EdgeEnd = LocalEdgeStarts[TypeOrdinal + 1u];
			for (uint32 EdgeOrdinal = EdgeBegin;
				EdgeOrdinal < EdgeEnd; ++EdgeOrdinal)
			{
				const uint32 TargetOrdinal =
					LocalEdges[EdgeOrdinal].TargetTypeOrdinal;
				if (LocalInDegrees[TargetOrdinal] == 0)
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::GraphAbiMismatch,
						LocalEdges[EdgeOrdinal].DiagnosticOffset);
				}
				--LocalInDegrees[TargetOrdinal];
				if (LocalInDegrees[TargetOrdinal] == 0)
				{
					LocalQueue.Add(TargetOrdinal);
				}
			}
		}
		if (ProcessedTypeCount
			!= static_cast<uint32>(Candidate.TypeOrdinals.Num()))
		{
			for (int32 TypeOrdinal = 0;
				TypeOrdinal < Candidate.TypeOrdinals.Num(); ++TypeOrdinal)
			{
				if (LocalInDegrees[TypeOrdinal] == 0)
				{
					continue;
				}
				for (uint32 EdgeOrdinal = LocalEdgeStarts[TypeOrdinal];
					EdgeOrdinal < LocalEdgeStarts[TypeOrdinal + 1];
					++EdgeOrdinal)
				{
					const FLocalTypeLayoutEdge& Edge = LocalEdges[EdgeOrdinal];
					if (LocalInDegrees[Edge.TargetTypeOrdinal] != 0)
					{
						return GraphFailure(
							EAngelscriptCacheValidationError::GraphAbiMismatch,
							Edge.DiagnosticOffset);
					}
				}
			}
			return GraphFailure(
				EAngelscriptCacheValidationError::GraphAbiMismatch);
		}
	}

	if (!(SourceIndex->SourceSnapshot == Context.SelectedSourceSnapshot))
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::SourceSnapshotMismatch,
			0,
			EAngelscriptCacheValidationStage::CurrentResolver);
	}
	if (!(State->Profile.Hash == Context.SelectedProfile.Hash))
	{
		return GraphFailure(
			EAngelscriptCacheValidationError::ProfileMismatch,
			StateRecord->FindCapturedOffset({
				EAngelscriptModuleStateCapturedField::Profile}).Get(0),
			EAngelscriptCacheValidationStage::CurrentResolver);
	}

	uint64 CurrentDependencyUpperBound =
		static_cast<uint64>(Interface->Dependencies.Num());
	for (int32 TypeOrdinal = 0;
		TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
	{
		const FAngelscriptCachedTypeSchema& Type =
			*Candidate.ReachableRecords[3 + TypeOrdinal]->TryGetTypeSchema();
		if (CurrentDependencyUpperBound > MAX_uint64
			- static_cast<uint64>(Type.Dependencies.Num()))
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}
		CurrentDependencyUpperBound +=
			static_cast<uint64>(Type.Dependencies.Num());
	}
	for (int32 DependencyBodyOrdinal = 0;
		DependencyBodyOrdinal < Snapshot->FunctionBodies.Num();
		++DependencyBodyOrdinal)
	{
		const FAngelscriptCachedFunctionBody& Body =
			*Candidate.ReachableRecords[BodyRecordBase + DependencyBodyOrdinal]
				->TryGetFunctionBody();
		if (CurrentDependencyUpperBound > MAX_uint64
			- static_cast<uint64>(Body.ActualDependencies.Num()))
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}
		CurrentDependencyUpperBound +=
			static_cast<uint64>(Body.ActualDependencies.Num());
	}
	if (CurrentDependencyUpperBound > Limits.MaxReferencesAndRelocations)
	{
		return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
	}
	if (CurrentDependencyUpperBound > static_cast<uint64>(MAX_int32))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}

	int32 CurrentMemoCapacity = 0;
	uint64 CurrentMemoBytes = 0;
	if (!TryCalculateArrayReserveBytes<FCurrentSymbolMemo>(
		static_cast<int32>(CurrentDependencyUpperBound),
		CurrentMemoCapacity, CurrentMemoBytes))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	{
		FAngelscriptCacheTemporaryResidentReservation CurrentMemoReservation;
		if (!Budget.TryReserveTemporaryDecoded(
			CurrentMemoBytes, Limits, CurrentMemoReservation))
		{
			return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
		}
		TArray<FCurrentSymbolMemo> CurrentMemos;
		CurrentMemos.Reserve(CurrentMemoCapacity);
		if (static_cast<uint64>(CurrentMemos.GetAllocatedSize())
			!= CurrentMemoBytes)
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}

		const auto ValidateCurrentDependency = [
			&Context, &CurrentMemos, &Interface](
				const FAngelscriptCacheSemanticDependency& Dependency,
				const uint64 StableKeyOffset,
				const uint64 ExpectedAbiOffset,
				const uint64 ExpectedContentOffset)
			-> FAngelscriptCacheValidationResult
		{
			if (Dependency.Target.Kind
					== EAngelscriptCacheReferenceKind::CanonicalName
				|| Dependency.Target.Kind
					== EAngelscriptCacheReferenceKind::StringLiteral
				|| IsSelectedModuleReference(*Interface, Dependency.Target))
			{
				return {};
			}

			const int32 MemoOrdinal = LowerBoundCurrentSymbolMemo(
				CurrentMemos, Dependency.Target.Kind,
				Dependency.Target.StableKey);
			if (MemoOrdinal >= CurrentMemos.Num()
				|| CompareCurrentSymbolMemoIdentity(CurrentMemos[MemoOrdinal],
					Dependency.Target.Kind,
					Dependency.Target.StableKey) != 0)
			{
				const TOptional<FAngelscriptCacheCurrentSymbol> Resolved =
					Context.CurrentSymbols->Resolve(
						Dependency.Target.Kind,
						Dependency.Target.StableKey);
				if (!Resolved.IsSet())
				{
					UE_LOG(LogAngelscriptCacheModuleGraph, Warning,
						TEXT("Cache V2 current symbol missing: ReferenceKind=%u DependencyKind=%u StableKey=%s ExpectedAbi=%s"),
						static_cast<uint32>(Dependency.Target.Kind),
						static_cast<uint32>(Dependency.Kind),
						*Dependency.Target.StableKey.ToHexString(),
						*Dependency.Target.ExpectedAbi.ToHexString());
					return GraphFailure(
						EAngelscriptCacheValidationError::CurrentSymbolMissing,
						StableKeyOffset,
						EAngelscriptCacheValidationStage::CurrentResolver);
				}
				CurrentMemos.Insert({Dependency.Target.Kind,
					Dependency.Target.StableKey, Resolved.GetValue()}, MemoOrdinal);
			}

			const FAngelscriptCacheCurrentSymbol& Current =
				CurrentMemos[MemoOrdinal].Symbol;
			if (!(Current.CurrentAbi == Dependency.Target.ExpectedAbi)
				|| Current.CurrentValueStorageKind.IsSet())
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::CurrentAbiMismatch,
					ExpectedAbiOffset,
					EAngelscriptCacheValidationStage::CurrentResolver);
			}
			if (Dependency.ExpectedContentOrValue.IsSet()
				&& (!Current.CurrentContentOrValue.IsSet()
					|| !(Current.CurrentContentOrValue.GetValue()
						== Dependency.ExpectedContentOrValue.GetValue())))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::CurrentContentMismatch,
					ExpectedContentOffset,
					EAngelscriptCacheValidationStage::CurrentResolver);
			}
			return {};
		};

		for (int32 DependencyOrdinal = 0;
			DependencyOrdinal < Interface->Dependencies.Num(); ++DependencyOrdinal)
		{
			const FAngelscriptCacheValidationResult Result =
				ValidateCurrentDependency(
					Interface->Dependencies[DependencyOrdinal],
					InterfaceOffset(*InterfaceRecord,
						EAngelscriptModuleInterfaceCapturedField::
							DependencyTargetStableKey,
						static_cast<uint32>(DependencyOrdinal)),
					InterfaceOffset(*InterfaceRecord,
						EAngelscriptModuleInterfaceCapturedField::
							DependencyTargetExpectedAbi,
						static_cast<uint32>(DependencyOrdinal)),
					InterfaceOffset(*InterfaceRecord,
						EAngelscriptModuleInterfaceCapturedField::
							DependencyExpectedContentOrValue,
						static_cast<uint32>(DependencyOrdinal)));
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}

		for (int32 TypeOrdinal = 0;
			TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
		{
			const FAngelscriptDecodedCacheRecordHandle& TypeRecord =
				Candidate.ReachableRecords[3 + TypeOrdinal];
			const FAngelscriptCachedTypeSchema& Type =
				*TypeRecord->TryGetTypeSchema();
			for (int32 DependencyOrdinal = 0;
				DependencyOrdinal < Type.Dependencies.Num();
				++DependencyOrdinal)
			{
				const FAngelscriptCacheValidationResult Result =
					ValidateCurrentDependency(
						Type.Dependencies[DependencyOrdinal],
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::DependencyTarget,
							static_cast<uint32>(DependencyOrdinal)),
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::DependencyTarget,
							static_cast<uint32>(DependencyOrdinal)),
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::Dependency,
							static_cast<uint32>(DependencyOrdinal)));
				if (!Result.IsSuccess())
				{
					return Result;
				}
			}
		}

		for (int32 DependencyBodyOrdinal = 0;
			DependencyBodyOrdinal < Snapshot->FunctionBodies.Num();
			++DependencyBodyOrdinal)
		{
			const FAngelscriptDecodedCacheRecordHandle& BodyRecord =
				Candidate.ReachableRecords[
					BodyRecordBase + DependencyBodyOrdinal];
			const FAngelscriptCachedFunctionBody& Body =
				*BodyRecord->TryGetFunctionBody();
			for (int32 DependencyOrdinal = 0;
				DependencyOrdinal < Body.ActualDependencies.Num();
				++DependencyOrdinal)
			{
				const FAngelscriptCacheValidationResult Result =
					ValidateCurrentDependency(
						Body.ActualDependencies[DependencyOrdinal],
						FunctionBodyOffset(*BodyRecord,
							EAngelscriptFunctionBodyCapturedField::
								ActualDependencyTargetStableKey,
							static_cast<uint32>(DependencyOrdinal)),
						FunctionBodyOffset(*BodyRecord,
							EAngelscriptFunctionBodyCapturedField::
								ActualDependencyTargetExpectedAbi,
							static_cast<uint32>(DependencyOrdinal)),
						FunctionBodyOffset(*BodyRecord,
							EAngelscriptFunctionBodyCapturedField::
								ActualDependencyExpectedContentOrValue,
							static_cast<uint32>(DependencyOrdinal)));
				if (!Result.IsSuccess())
				{
					return Result;
				}
			}
		}
	}

	const auto ValidateCurrentPropertyStorage = [](
		const FAngelscriptCachedPropertySchema& Property,
		const FAngelscriptCacheResolvedDataTypeLayout& Current)
		-> bool
	{
		if (Current.StorageKind != Property.StorageKind
			|| Current.SemanticStorageSize != Property.SemanticStorageSize
			|| Current.SemanticStorageAlignment
				!= Property.SemanticStorageAlignment)
		{
			return false;
		}
		FAngelscriptHash256 CurrentStorageHash;
		return FAngelscriptCacheTypeSchemaArchive::ComputeStorageLayoutHash(
				Property.Type, Current.StorageKind,
				Current.SemanticStorageSize,
				Current.SemanticStorageAlignment,
				CurrentStorageHash).IsSuccess()
			&& CurrentStorageHash == Property.StorageLayoutHash;
	};

	// Primitive and handle slots are profile/build constants, never live current
	// type queries. Compare them before any eligible TypeLayoutInput call so a
	// stored profile-constant contradiction keeps the frozen step-10 precedence.
	const FAngelscriptCacheV1BuildLayoutConstants& LayoutConstants =
		FAngelscriptCacheTypeSchemaArchive::GetV1BuildLayoutConstants();
	for (int32 TypeOrdinal = 0;
		TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
	{
		const FAngelscriptDecodedCacheRecordHandle& TypeRecord =
			Candidate.ReachableRecords[3 + TypeOrdinal];
		const FAngelscriptCachedTypeSchema& Type =
			*TypeRecord->TryGetTypeSchema();
		for (int32 PropertyOrdinal = 0;
			PropertyOrdinal < Type.OrderedProperties.Num(); ++PropertyOrdinal)
		{
			const FAngelscriptCachedPropertySchema& Property =
				Type.OrderedProperties[PropertyOrdinal];
			FAngelscriptCacheV1StorageLayout ConstantLayout;
			bool bUsesConstant = false;
			if (Property.Type.Kind == EAngelscriptCachedDataTypeKind::Primitive)
			{
				bUsesConstant = LayoutConstants.TryGetPrimitiveStorageLayout(
					Property.Type.Primitive, ConstantLayout);
			}
			else if (Property.StorageKind
				== EAngelscriptCachedPropertyStorageKind::ObjectHandle)
			{
				ConstantLayout = LayoutConstants.GetObjectHandleStorageLayout();
				bUsesConstant = true;
			}
			if (!bUsesConstant)
			{
				continue;
			}

			FAngelscriptCacheResolvedDataTypeLayout Current;
			Current.StorageKind = Property.StorageKind;
			Current.SemanticStorageSize = ConstantLayout.SemanticStorageSize;
			Current.SemanticStorageAlignment =
				ConstantLayout.SemanticStorageAlignment;
			if (!ValidateCurrentPropertyStorage(Property, Current))
			{
				return GraphFailure(
					EAngelscriptCacheValidationError::CurrentAbiMismatch,
					TypeSchemaOffset(*TypeRecord,
						EAngelscriptTypeSchemaCapturedField::OrderedProperty,
						static_cast<uint32>(PropertyOrdinal)),
					EAngelscriptCacheValidationStage::CurrentResolver);
			}
		}
	}

	// Current layout inputs are a separate eligibility authority from dependency
	// ABI/content. Resolve only after every stored graph contradiction and every
	// eligible current symbol has succeeded. The memo identity intentionally
	// excludes stored contributions and hashes so repeated consumers share one
	// raw provider result and apply their own already-validated presence masks.
	uint64 CurrentLayoutInputUpperBound = 0;
	for (int32 TypeOrdinal = 0;
		TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
	{
		const FAngelscriptCachedTypeSchema& Type =
			*Candidate.ReachableRecords[3 + TypeOrdinal]->TryGetTypeSchema();
		if (CurrentLayoutInputUpperBound > MAX_uint64
			- static_cast<uint64>(Type.LayoutInputs.Num()))
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}
		CurrentLayoutInputUpperBound +=
			static_cast<uint64>(Type.LayoutInputs.Num());
	}
	if (CurrentLayoutInputUpperBound > Limits.MaxReferencesAndRelocations)
	{
		return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
	}
	if (CurrentLayoutInputUpperBound > static_cast<uint64>(MAX_int32))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}

	int32 CurrentLayoutMemoCapacity = 0;
	uint64 CurrentLayoutMemoBytes = 0;
	if (!TryCalculateArrayReserveBytes<FCurrentLayoutInputMemo>(
		static_cast<int32>(CurrentLayoutInputUpperBound),
		CurrentLayoutMemoCapacity, CurrentLayoutMemoBytes))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	{
		FAngelscriptCacheTemporaryResidentReservation CurrentLayoutReservation;
		if (!Budget.TryReserveTemporaryDecoded(
			CurrentLayoutMemoBytes, Limits, CurrentLayoutReservation))
		{
			return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
		}
		TArray<FCurrentLayoutInputMemo> CurrentLayoutMemos;
		CurrentLayoutMemos.Reserve(CurrentLayoutMemoCapacity);
		if (static_cast<uint64>(CurrentLayoutMemos.GetAllocatedSize())
			!= CurrentLayoutMemoBytes)
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}

		for (int32 TypeOrdinal = 0;
			TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
		{
			const FAngelscriptDecodedCacheRecordHandle& TypeRecord =
				Candidate.ReachableRecords[3 + TypeOrdinal];
			const FAngelscriptCachedTypeSchema& Type =
				*TypeRecord->TryGetTypeSchema();
			for (int32 InputOrdinal = 0;
				InputOrdinal < Type.LayoutInputs.Num(); ++InputOrdinal)
			{
				const FAngelscriptCachedTypeLayoutInput& Input =
					Type.LayoutInputs[InputOrdinal];
				if (Input.InputKind
						== EAngelscriptCachedTypeLayoutInputKind::BaseType
					&& IsSelectedModuleReference(*Interface, Input.Target))
				{
					continue;
				}

				const int32 MemoOrdinal = LowerBoundCurrentLayoutInputMemo(
					CurrentLayoutMemos, Input.InputKind, Input.Target.Kind,
					Input.Target.StableKey);
				if (MemoOrdinal >= CurrentLayoutMemos.Num()
					|| CompareCurrentLayoutInputMemoIdentity(
						CurrentLayoutMemos[MemoOrdinal], Input.InputKind,
						Input.Target.Kind, Input.Target.StableKey) != 0)
				{
					const TOptional<FAngelscriptCacheResolvedTypeLayoutInput>
						Resolved = Context.CurrentLayouts->ResolveTypeLayoutInput(
							Input.InputKind, Input.Target.Kind,
							Input.Target.StableKey);
					if (!Resolved.IsSet())
					{
						UE_LOG(LogAngelscriptCacheModuleGraph, Warning,
							TEXT("Cache V2 current type-layout input missing: InputKind=%u ReferenceKind=%u StableKey=%s"),
							static_cast<uint32>(Input.InputKind),
							static_cast<uint32>(Input.Target.Kind),
							*Input.Target.StableKey.ToHexString());
						return GraphFailure(
							EAngelscriptCacheValidationError::CurrentSymbolMissing,
							TypeSchemaOffset(*TypeRecord,
								EAngelscriptTypeSchemaCapturedField::LayoutInputTarget,
								static_cast<uint32>(InputOrdinal)),
							EAngelscriptCacheValidationStage::CurrentResolver);
					}
					const FAngelscriptCacheResolvedTypeLayoutInput& Raw =
						Resolved.GetValue();
					const bool bRawShapeValid = Raw.BoundaryContribution.IsSet()
						&& (Input.InputKind
							== EAngelscriptCachedTypeLayoutInputKind::StructHeader
							? !Raw.AlignmentContribution.IsSet()
							: Raw.AlignmentContribution.IsSet()
								&& IsPowerOfTwo(
									Raw.AlignmentContribution.GetValue()));
					if (!bRawShapeValid)
					{
						return GraphFailure(
							EAngelscriptCacheValidationError::CurrentAbiMismatch,
							TypeSchemaOffset(*TypeRecord,
								EAngelscriptTypeSchemaCapturedField::LayoutInput,
								static_cast<uint32>(InputOrdinal)),
							EAngelscriptCacheValidationStage::CurrentResolver);
					}
					CurrentLayoutMemos.Insert({Input.InputKind,
						Input.Target.Kind, Input.Target.StableKey, Raw}, MemoOrdinal);
				}

				const FAngelscriptCacheResolvedTypeLayoutInput& Current =
					CurrentLayoutMemos[MemoOrdinal].Layout;
				FAngelscriptCachedTypeLayoutInput CurrentInput = Input;
				if (Input.BoundaryContribution.IsSet())
				{
					if (!Current.BoundaryContribution.IsSet())
					{
						return GraphFailure(
							EAngelscriptCacheValidationError::CurrentAbiMismatch,
							TypeSchemaOffset(*TypeRecord,
								EAngelscriptTypeSchemaCapturedField::LayoutInput,
								static_cast<uint32>(InputOrdinal)),
							EAngelscriptCacheValidationStage::CurrentResolver);
					}
					CurrentInput.BoundaryContribution =
						Current.BoundaryContribution;
				}
				else
				{
					CurrentInput.BoundaryContribution.Reset();
				}
				if (Input.AlignmentContribution.IsSet())
				{
					if (!Current.AlignmentContribution.IsSet())
					{
						return GraphFailure(
							EAngelscriptCacheValidationError::CurrentAbiMismatch,
							TypeSchemaOffset(*TypeRecord,
								EAngelscriptTypeSchemaCapturedField::LayoutInput,
								static_cast<uint32>(InputOrdinal)),
							EAngelscriptCacheValidationStage::CurrentResolver);
					}
					CurrentInput.AlignmentContribution =
						Current.AlignmentContribution;
				}
				else
				{
					CurrentInput.AlignmentContribution.Reset();
				}

				FAngelscriptHash256 CurrentLayoutInputHash;
				const FAngelscriptCacheValidationResult HashResult =
					FAngelscriptCacheTypeSchemaArchive::ComputeLayoutInputHash(
						CurrentInput, CurrentLayoutInputHash);
				if (!HashResult.IsSuccess()
					|| CurrentInput.BoundaryContribution
						!= Input.BoundaryContribution
					|| CurrentInput.AlignmentContribution
						!= Input.AlignmentContribution
					|| !(CurrentLayoutInputHash == Input.LayoutInputHash))
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::CurrentAbiMismatch,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::LayoutInput,
							static_cast<uint32>(InputOrdinal)),
						EAngelscriptCacheValidationStage::CurrentResolver);
				}
			}
		}
	}

	uint64 CurrentDataTypeLayoutUpperBound = 0;
	for (int32 TypeOrdinal = 0;
		TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
	{
		const FAngelscriptCachedTypeSchema& Type =
			*Candidate.ReachableRecords[3 + TypeOrdinal]->TryGetTypeSchema();
		if (CurrentDataTypeLayoutUpperBound > MAX_uint64
			- static_cast<uint64>(Type.OrderedProperties.Num()))
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}
		CurrentDataTypeLayoutUpperBound +=
			static_cast<uint64>(Type.OrderedProperties.Num());
	}
	if (CurrentDataTypeLayoutUpperBound > Limits.MaxReferencesAndRelocations)
	{
		return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
	}
	if (CurrentDataTypeLayoutUpperBound > static_cast<uint64>(MAX_int32))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}

	int32 CurrentDataTypeMemoCapacity = 0;
	uint64 CurrentDataTypeMemoBytes = 0;
	if (!TryCalculateArrayReserveBytes<FCurrentDataTypeLayoutMemo>(
		static_cast<int32>(CurrentDataTypeLayoutUpperBound),
		CurrentDataTypeMemoCapacity, CurrentDataTypeMemoBytes))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	{
		FAngelscriptCacheTemporaryResidentReservation CurrentDataTypeReservation;
		if (!Budget.TryReserveTemporaryDecoded(
			CurrentDataTypeMemoBytes, Limits, CurrentDataTypeReservation))
		{
			return GraphFailure(EAngelscriptCacheValidationError::BudgetExceeded);
		}
		TArray<FCurrentDataTypeLayoutMemo> CurrentDataTypeMemos;
		CurrentDataTypeMemos.Reserve(CurrentDataTypeMemoCapacity);
		if (static_cast<uint64>(CurrentDataTypeMemos.GetAllocatedSize())
			!= CurrentDataTypeMemoBytes)
		{
			return GraphFailure(EAngelscriptCacheValidationError::Overflow);
		}
		const FProspectiveTypeLayoutView LocalLayouts(Candidate);

		for (int32 TypeOrdinal = 0;
			TypeOrdinal < Snapshot->TypeSchemas.Num(); ++TypeOrdinal)
		{
			const FAngelscriptDecodedCacheRecordHandle& TypeRecord =
				Candidate.ReachableRecords[3 + TypeOrdinal];
			const FAngelscriptCachedTypeSchema& Type =
				*TypeRecord->TryGetTypeSchema();
			for (int32 PropertyOrdinal = 0;
				PropertyOrdinal < Type.OrderedProperties.Num(); ++PropertyOrdinal)
			{
				const FAngelscriptCachedPropertySchema& Property =
					Type.OrderedProperties[PropertyOrdinal];
				if (Property.Type.Kind
						== EAngelscriptCachedDataTypeKind::Primitive
					|| Property.StorageKind
						== EAngelscriptCachedPropertyStorageKind::ObjectHandle
					|| (Property.Type.Kind
							== EAngelscriptCachedDataTypeKind::ScriptType
						&& Property.Type.TypeReference.IsSet()
						&& IsSelectedModuleReference(
							*Interface, Property.Type.TypeReference.GetValue())))
				{
					continue;
				}
				if (Property.StorageKind
						!= EAngelscriptCachedPropertyStorageKind::InlineValue
					|| (Property.Type.Kind
							!= EAngelscriptCachedDataTypeKind::ScriptType
						&& Property.Type.Kind
							!= EAngelscriptCachedDataTypeKind::EnvironmentType))
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::MissingCoverage,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::PropertyType,
							static_cast<uint32>(PropertyOrdinal)));
				}

				const int32 MemoOrdinal = LowerBoundCurrentDataTypeLayoutMemo(
					CurrentDataTypeMemos, Property.Type, Property.StorageKind);
				if (MemoOrdinal >= CurrentDataTypeMemos.Num()
					|| CompareCurrentDataTypeLayoutMemoIdentity(
						CurrentDataTypeMemos[MemoOrdinal], Property.Type,
						Property.StorageKind) != 0)
				{
					const TOptional<FAngelscriptCacheResolvedDataTypeLayout> Resolved =
						Context.CurrentLayouts->ResolveDataTypeLayout(
							Property.Type, LocalLayouts);
					if (!Resolved.IsSet())
					{
						UE_LOG(LogAngelscriptCacheModuleGraph, Warning,
							TEXT("Cache V2 current property layout missing: TypeKind=%u StorageKind=%u StableKey=%s"),
							static_cast<uint32>(Property.Type.Kind),
							static_cast<uint32>(Property.StorageKind),
							Property.Type.TypeReference.IsSet()
								? *Property.Type.TypeReference->StableKey.ToHexString()
								: TEXT("<none>"));
						return GraphFailure(
							EAngelscriptCacheValidationError::CurrentSymbolMissing,
							TypeSchemaOffset(*TypeRecord,
								EAngelscriptTypeSchemaCapturedField::PropertyType,
								static_cast<uint32>(PropertyOrdinal)),
							EAngelscriptCacheValidationStage::CurrentResolver);
					}
					const FAngelscriptCacheResolvedDataTypeLayout& Raw =
						Resolved.GetValue();
					if (Raw.StorageKind
							== EAngelscriptCachedPropertyStorageKind::Invalid
						|| !IsPowerOfTwo(Raw.SemanticStorageAlignment))
					{
						return GraphFailure(
							EAngelscriptCacheValidationError::CurrentAbiMismatch,
							TypeSchemaOffset(*TypeRecord,
								EAngelscriptTypeSchemaCapturedField::OrderedProperty,
								static_cast<uint32>(PropertyOrdinal)),
							EAngelscriptCacheValidationStage::CurrentResolver);
					}
					CurrentDataTypeMemos.Insert({
						&Property.Type, Property.StorageKind, Raw}, MemoOrdinal);
				}

				if (!ValidateCurrentPropertyStorage(
					Property, CurrentDataTypeMemos[MemoOrdinal].Layout))
				{
					return GraphFailure(
						EAngelscriptCacheValidationError::CurrentAbiMismatch,
						TypeSchemaOffset(*TypeRecord,
							EAngelscriptTypeSchemaCapturedField::OrderedProperty,
							static_cast<uint32>(PropertyOrdinal)),
						EAngelscriptCacheValidationStage::CurrentResolver);
				}
			}
		}
	}

	if (!FAngelscriptCacheModuleGraphCandidateAccess::Promote(
		CandidateTransaction))
	{
		return GraphFailure(EAngelscriptCacheValidationError::Overflow);
	}
	OutGraph = MoveTemp(Candidate);
	return {};
}
