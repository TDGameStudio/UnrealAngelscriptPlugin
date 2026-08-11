#include "Cache/AngelscriptCacheTypes.h"

#include "Async/Async.h"
#include "CQTest.h"

#include <type_traits>
#include <utility>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheBudgetTests_Private
{
	template <typename BudgetType, typename = void>
	struct THasPublicDecodedCandidateBegin : std::false_type
	{
	};

	template <typename BudgetType>
	struct THasPublicDecodedCandidateBegin<BudgetType, std::void_t<decltype(
		std::declval<BudgetType&>().BeginDecodedCandidateTransaction(
			std::declval<const FAngelscriptCacheReadLimits&>()))>> : std::true_type
	{
	};

	static_assert(!THasPublicDecodedCandidateBegin<FAngelscriptCacheReadBudget>::value,
		"Decoded-candidate transactions must not escape the private Budget surface");
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheBudgetTests,
	"Angelscript.TestModule.Cache.Budget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FBudgetCounters
	{
		uint64 Stored = 0;
		uint64 Decompressed = 0;
		uint64 Decoded = 0;
		uint64 Resident = 0;
		uint64 Temporary = 0;
		uint64 PeakLive = 0;
		uint64 ReferencesAndRelocations = 0;
	};

	static FBudgetCounters CaptureCounters(const FAngelscriptCacheReadBudget& Budget)
	{
		return FBudgetCounters{
			Budget.GetStoredBytes(),
			Budget.GetDecompressedBytes(),
			Budget.GetDecodedBytes(),
			Budget.GetResidentDecodedBytes(),
			Budget.GetTemporaryResidentDecodedBytes(),
			Budget.GetPeakLiveResidentDecodedBytes(),
			Budget.GetReferencesAndRelocations()};
	}

	static FAngelscriptCacheReadLimits MakeDecodedLimits(
		const uint64 MaxTotalDecodedBytes,
		const uint64 MaxLiveResidentDecodedBytes)
	{
		FAngelscriptCacheReadLimits Limits;
		Limits.MaxTotalDecodedBytes = MaxTotalDecodedBytes;
		Limits.MaxResidentDecodedBytes = MaxLiveResidentDecodedBytes;
		return Limits;
	}

	template <typename T>
	static auto HasPublicSessionReset(int) -> decltype(std::declval<T&>().Reset(), std::true_type{});

	template <typename T>
	static std::false_type HasPublicSessionReset(...);

	template <typename T>
	static auto HasSplitTryConsumeDecoded(int) -> decltype(
		std::declval<T&>().TryConsumeDecoded(
			std::declval<uint64>(),
			std::declval<const FAngelscriptCacheReadLimits&>()),
		std::true_type{});

	template <typename T>
	static std::false_type HasSplitTryConsumeDecoded(...);

	template <typename T>
	static auto HasSplitTryConsumeResidentDecoded(int) -> decltype(
		std::declval<T&>().TryConsumeResidentDecoded(
			std::declval<uint64>(),
			std::declval<const FAngelscriptCacheReadLimits&>()),
		std::true_type{});

	template <typename T>
	static std::false_type HasSplitTryConsumeResidentDecoded(...);

	template <typename T>
	static auto HasOldTemporaryReservationApi(int) -> decltype(
		std::declval<T&>().TryReserveTemporaryResidentDecoded(
			std::declval<uint64>(),
			std::declval<const FAngelscriptCacheReadLimits&>(),
			std::declval<FAngelscriptCacheTemporaryResidentReservation&>()),
		std::true_type{});

	template <typename T>
	static std::false_type HasOldTemporaryReservationApi(...);

public:
	TEST_METHOD(PublicSurfaceIsSingleOwnerAndHasNoSessionResetOrSplitAcquisitions)
	{
		using FBudget = FAngelscriptCacheReadBudget;
		using FReservation = FAngelscriptCacheTemporaryResidentReservation;
		using FRetainedAcquire = bool (FBudget::*)(
			uint64, const FAngelscriptCacheReadLimits&);
		using FTemporaryAcquire = bool (FBudget::*)(
			uint64, const FAngelscriptCacheReadLimits&, FReservation&);
		using FPeakGetter = uint64 (FBudget::*)() const;
		using FPromotion = bool (FReservation::*)();

		static_assert(std::is_same_v<decltype(&FBudget::TryConsumeRetainedDecoded),
			FRetainedAcquire>);
		static_assert(std::is_same_v<decltype(&FBudget::TryReserveTemporaryDecoded),
			FTemporaryAcquire>);
		static_assert(std::is_same_v<decltype(&FBudget::GetPeakLiveResidentDecodedBytes),
			FPeakGetter>);
		static_assert(std::is_same_v<decltype(&FReservation::PromoteToRetained),
			FPromotion>);

		ASSERT_THAT(IsTrue(std::is_default_constructible_v<FBudget>,
			TEXT("A caller must be able to create its one session Budget")));
		ASSERT_THAT(IsFalse(std::is_copy_constructible_v<FBudget>,
			TEXT("A session Budget must have one owner")));
		ASSERT_THAT(IsFalse(std::is_copy_assignable_v<FBudget>,
			TEXT("A session Budget must not copy accumulated counters")));
		ASSERT_THAT(IsFalse(std::is_move_constructible_v<FBudget>,
			TEXT("Moving a Budget would invalidate live reservation owner pointers")));
		ASSERT_THAT(IsFalse(std::is_move_assignable_v<FBudget>,
			TEXT("Move assignment must not replace a Budget with live reservations")));
		ASSERT_THAT(IsFalse(decltype(HasPublicSessionReset<FBudget>(0))::value,
			TEXT("Cache V2 exposes no public session-counter Reset")));
		ASSERT_THAT(IsFalse(decltype(HasSplitTryConsumeDecoded<FBudget>(0))::value,
			TEXT("The old Total-only decoded acquisition must not remain public")));
		ASSERT_THAT(IsFalse(decltype(HasSplitTryConsumeResidentDecoded<FBudget>(0))::value,
			TEXT("The old Resident-only decoded acquisition must not remain public")));
		ASSERT_THAT(IsFalse(decltype(HasOldTemporaryReservationApi<FBudget>(0))::value,
			TEXT("The old temporary-resident-only acquisition must not remain public")));

		ASSERT_THAT(IsTrue(std::is_default_constructible_v<FReservation>));
		ASSERT_THAT(IsFalse(std::is_copy_constructible_v<FReservation>));
		ASSERT_THAT(IsFalse(std::is_copy_assignable_v<FReservation>));
		ASSERT_THAT(IsTrue(std::is_nothrow_move_constructible_v<FReservation>));
		ASSERT_THAT(IsTrue(std::is_nothrow_move_assignable_v<FReservation>));
	}

	TEST_METHOD(RetainedConsumePreflightsTotalAndCombinedResidentAtomically)
	{
		{
			FAngelscriptCacheReadBudget ExactBudget;
			const FAngelscriptCacheReadLimits ExactLimits = MakeDecodedLimits(9, 9);
			FAngelscriptCacheTemporaryResidentReservation ExistingTemporary;

			ASSERT_THAT(IsTrue(ExactBudget.TryConsumeRetainedDecoded(4, ExactLimits)));
			ASSERT_THAT(IsTrue(ExactBudget.TryReserveTemporaryDecoded(
				3, ExactLimits, ExistingTemporary)));
			ASSERT_THAT(IsTrue(ExactBudget.TryConsumeRetainedDecoded(2, ExactLimits),
				TEXT("The exact Total and combined-live boundary must succeed")));
			ASSERT_THAT(AreEqual(UINT64_C(9), ExactBudget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(6), ExactBudget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(3), ExactBudget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(9), ExactBudget.GetPeakLiveResidentDecodedBytes()));

			ExistingTemporary.Reset();
			ASSERT_THAT(AreEqual(UINT64_C(9), ExactBudget.GetDecodedBytes(),
				TEXT("Releasing scratch never refunds monotonic TotalDecoded")));
			ASSERT_THAT(AreEqual(UINT64_C(6), ExactBudget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), ExactBudget.GetTemporaryResidentDecodedBytes()));
		}

		{
			FAngelscriptCacheReadBudget TotalShortBudget;
			const FAngelscriptCacheReadLimits TotalShortLimits = MakeDecodedLimits(8, 100);
			FAngelscriptCacheTemporaryResidentReservation ExistingTemporary;
			ASSERT_THAT(IsTrue(TotalShortBudget.TryConsumeRetainedDecoded(4, TotalShortLimits)));
			ASSERT_THAT(IsTrue(TotalShortBudget.TryReserveTemporaryDecoded(
				3, TotalShortLimits, ExistingTemporary)));
			ASSERT_THAT(IsTrue(TotalShortBudget.TryConsumeStored(5, TotalShortLimits)));
			ASSERT_THAT(IsTrue(TotalShortBudget.TryConsumeDecompressed(6, TotalShortLimits)));
			ASSERT_THAT(IsTrue(TotalShortBudget.TryConsumeReferencesAndRelocations(
				7, TotalShortLimits)));
			const FBudgetCounters Before = CaptureCounters(TotalShortBudget);

			ASSERT_THAT(IsFalse(TotalShortBudget.TryConsumeRetainedDecoded(2, TotalShortLimits),
				TEXT("A one-byte-short TotalDecoded limit must reject before either charge")));
			const FBudgetCounters After = CaptureCounters(TotalShortBudget);
			ASSERT_THAT(AreEqual(Before.Stored, After.Stored));
			ASSERT_THAT(AreEqual(Before.Decompressed, After.Decompressed));
			ASSERT_THAT(AreEqual(Before.Decoded, After.Decoded));
			ASSERT_THAT(AreEqual(Before.Resident, After.Resident));
			ASSERT_THAT(AreEqual(Before.Temporary, After.Temporary));
			ASSERT_THAT(AreEqual(Before.PeakLive, After.PeakLive));
			ASSERT_THAT(AreEqual(Before.ReferencesAndRelocations, After.ReferencesAndRelocations));
		}

		{
			FAngelscriptCacheReadBudget ResidentShortBudget;
			const FAngelscriptCacheReadLimits ResidentShortLimits = MakeDecodedLimits(100, 8);
			FAngelscriptCacheTemporaryResidentReservation ExistingTemporary;
			ASSERT_THAT(IsTrue(ResidentShortBudget.TryConsumeRetainedDecoded(4, ResidentShortLimits)));
			ASSERT_THAT(IsTrue(ResidentShortBudget.TryReserveTemporaryDecoded(
				3, ResidentShortLimits, ExistingTemporary)));
			const FBudgetCounters Before = CaptureCounters(ResidentShortBudget);

			ASSERT_THAT(IsFalse(ResidentShortBudget.TryConsumeRetainedDecoded(2, ResidentShortLimits),
				TEXT("A one-byte-short combined-live limit must reject before either charge")));
			const FBudgetCounters After = CaptureCounters(ResidentShortBudget);
			ASSERT_THAT(AreEqual(Before.Stored, After.Stored));
			ASSERT_THAT(AreEqual(Before.Decompressed, After.Decompressed));
			ASSERT_THAT(AreEqual(Before.Decoded, After.Decoded));
			ASSERT_THAT(AreEqual(Before.Resident, After.Resident));
			ASSERT_THAT(AreEqual(Before.Temporary, After.Temporary));
			ASSERT_THAT(AreEqual(Before.PeakLive, After.PeakLive));
			ASSERT_THAT(AreEqual(Before.ReferencesAndRelocations, After.ReferencesAndRelocations));
		}

	}

	TEST_METHOD(StoredDecompressedAndReferenceCountersAreIndependentMonotonicBudgets)
	{
		FAngelscriptCacheReadLimits Limits;
		Limits.MaxTotalStoredBytes = 5;
		Limits.MaxTotalDecompressedBytes = 7;
		Limits.MaxReferencesAndRelocations = 9;
		FAngelscriptCacheReadBudget Budget;

		ASSERT_THAT(IsTrue(Budget.TryConsumeStored(2, Limits)));
		ASSERT_THAT(IsTrue(Budget.TryConsumeStored(3, Limits)));
		ASSERT_THAT(IsTrue(Budget.TryConsumeDecompressed(7, Limits)));
		ASSERT_THAT(IsTrue(Budget.TryConsumeReferencesAndRelocations(4, Limits)));
		ASSERT_THAT(IsTrue(Budget.TryConsumeReferencesAndRelocations(5, Limits)));

		const FBudgetCounters AtExactLimits = CaptureCounters(Budget);
		ASSERT_THAT(AreEqual(UINT64_C(5), AtExactLimits.Stored));
		ASSERT_THAT(AreEqual(UINT64_C(7), AtExactLimits.Decompressed));
		ASSERT_THAT(AreEqual(UINT64_C(9), AtExactLimits.ReferencesAndRelocations));
		ASSERT_THAT(AreEqual(UINT64_C(0), AtExactLimits.Decoded));
		ASSERT_THAT(AreEqual(UINT64_C(0), AtExactLimits.Resident));
		ASSERT_THAT(AreEqual(UINT64_C(0), AtExactLimits.Temporary));
		ASSERT_THAT(AreEqual(UINT64_C(0), AtExactLimits.PeakLive));

		ASSERT_THAT(IsTrue(Budget.TryConsumeStored(0, Limits)));
		ASSERT_THAT(IsTrue(Budget.TryConsumeDecompressed(0, Limits)));
		ASSERT_THAT(IsTrue(Budget.TryConsumeReferencesAndRelocations(0, Limits)));
		ASSERT_THAT(IsFalse(Budget.TryConsumeStored(1, Limits)));
		ASSERT_THAT(IsFalse(Budget.TryConsumeDecompressed(1, Limits)));
		ASSERT_THAT(IsFalse(Budget.TryConsumeReferencesAndRelocations(1, Limits)));

		const FBudgetCounters AfterRejectedExtensions = CaptureCounters(Budget);
		ASSERT_THAT(AreEqual(AtExactLimits.Stored, AfterRejectedExtensions.Stored));
		ASSERT_THAT(AreEqual(AtExactLimits.Decompressed,
			AfterRejectedExtensions.Decompressed));
		ASSERT_THAT(AreEqual(AtExactLimits.ReferencesAndRelocations,
			AfterRejectedExtensions.ReferencesAndRelocations));
		ASSERT_THAT(AreEqual(AtExactLimits.Decoded, AfterRejectedExtensions.Decoded));

		FAngelscriptCacheReadLimits Unlimited;
		Unlimited.MaxTotalStoredBytes = MAX_uint64;
		Unlimited.MaxTotalDecompressedBytes = MAX_uint64;
		Unlimited.MaxReferencesAndRelocations = MAX_uint64;

		FAngelscriptCacheReadBudget StoredOverflowBudget;
		ASSERT_THAT(IsTrue(StoredOverflowBudget.TryConsumeStored(
			MAX_uint64 - 4, Unlimited)));
		ASSERT_THAT(IsFalse(StoredOverflowBudget.TryConsumeStored(5, Unlimited)));
		ASSERT_THAT(AreEqual(MAX_uint64 - 4,
			StoredOverflowBudget.GetStoredBytes()));

		FAngelscriptCacheReadBudget DecompressedOverflowBudget;
		ASSERT_THAT(IsTrue(DecompressedOverflowBudget.TryConsumeDecompressed(
			MAX_uint64 - 4, Unlimited)));
		ASSERT_THAT(IsFalse(DecompressedOverflowBudget.TryConsumeDecompressed(
			5, Unlimited)));
		ASSERT_THAT(AreEqual(MAX_uint64 - 4,
			DecompressedOverflowBudget.GetDecompressedBytes()));

		FAngelscriptCacheReadBudget ReferenceOverflowBudget;
		ASSERT_THAT(IsTrue(ReferenceOverflowBudget.TryConsumeReferencesAndRelocations(
			MAX_uint64 - 4, Unlimited)));
		ASSERT_THAT(IsFalse(ReferenceOverflowBudget.TryConsumeReferencesAndRelocations(
			5, Unlimited)));
		ASSERT_THAT(AreEqual(MAX_uint64 - 4,
			ReferenceOverflowBudget.GetReferencesAndRelocations()));
	}

	TEST_METHOD(TemporaryReservePreflightsTotalAndCombinedResidentAtomically)
	{
		{
			FAngelscriptCacheReadBudget ExactBudget;
			const FAngelscriptCacheReadLimits ExactLimits = MakeDecodedLimits(9, 9);
			FAngelscriptCacheTemporaryResidentReservation ExistingTemporary;
			FAngelscriptCacheTemporaryResidentReservation ExactTemporary;
			ASSERT_THAT(IsTrue(ExactBudget.TryConsumeRetainedDecoded(4, ExactLimits)));
			ASSERT_THAT(IsTrue(ExactBudget.TryReserveTemporaryDecoded(
				3, ExactLimits, ExistingTemporary)));

			ASSERT_THAT(IsTrue(ExactBudget.TryReserveTemporaryDecoded(
				2, ExactLimits, ExactTemporary),
				TEXT("The exact Total and combined-live temporary boundary must succeed")));
			ASSERT_THAT(IsTrue(ExactTemporary.IsActive()));
			ASSERT_THAT(AreEqual(UINT64_C(2), ExactTemporary.GetReservedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(9), ExactBudget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(4), ExactBudget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(5), ExactBudget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(9), ExactBudget.GetPeakLiveResidentDecodedBytes()));
		}

		{
			FAngelscriptCacheReadBudget TotalShortBudget;
			const FAngelscriptCacheReadLimits TotalShortLimits = MakeDecodedLimits(8, 100);
			FAngelscriptCacheTemporaryResidentReservation ExistingTemporary;
			FAngelscriptCacheTemporaryResidentReservation RejectedTemporary;
			ASSERT_THAT(IsTrue(TotalShortBudget.TryConsumeRetainedDecoded(4, TotalShortLimits)));
			ASSERT_THAT(IsTrue(TotalShortBudget.TryReserveTemporaryDecoded(
				3, TotalShortLimits, ExistingTemporary)));
			const FBudgetCounters Before = CaptureCounters(TotalShortBudget);

			ASSERT_THAT(IsFalse(TotalShortBudget.TryReserveTemporaryDecoded(
				2, TotalShortLimits, RejectedTemporary),
				TEXT("A one-byte-short TotalDecoded limit must reject before installing a guard")));
			const FBudgetCounters After = CaptureCounters(TotalShortBudget);
			ASSERT_THAT(IsFalse(RejectedTemporary.IsActive()));
			ASSERT_THAT(AreEqual(UINT64_C(0), RejectedTemporary.GetReservedBytes()));
			ASSERT_THAT(IsTrue(ExistingTemporary.IsActive()));
			ASSERT_THAT(AreEqual(UINT64_C(3), ExistingTemporary.GetReservedBytes()));
			ASSERT_THAT(AreEqual(Before.Stored, After.Stored));
			ASSERT_THAT(AreEqual(Before.Decompressed, After.Decompressed));
			ASSERT_THAT(AreEqual(Before.Decoded, After.Decoded));
			ASSERT_THAT(AreEqual(Before.Resident, After.Resident));
			ASSERT_THAT(AreEqual(Before.Temporary, After.Temporary));
			ASSERT_THAT(AreEqual(Before.PeakLive, After.PeakLive));
			ASSERT_THAT(AreEqual(Before.ReferencesAndRelocations, After.ReferencesAndRelocations));
		}

		{
			FAngelscriptCacheReadBudget ResidentShortBudget;
			const FAngelscriptCacheReadLimits ResidentShortLimits = MakeDecodedLimits(100, 8);
			FAngelscriptCacheTemporaryResidentReservation ExistingTemporary;
			FAngelscriptCacheTemporaryResidentReservation RejectedTemporary;
			ASSERT_THAT(IsTrue(ResidentShortBudget.TryConsumeRetainedDecoded(4, ResidentShortLimits)));
			ASSERT_THAT(IsTrue(ResidentShortBudget.TryReserveTemporaryDecoded(
				3, ResidentShortLimits, ExistingTemporary)));
			const FBudgetCounters Before = CaptureCounters(ResidentShortBudget);

			ASSERT_THAT(IsFalse(ResidentShortBudget.TryReserveTemporaryDecoded(
				2, ResidentShortLimits, RejectedTemporary),
				TEXT("A one-byte-short combined-live limit must reject before installing a guard")));
			const FBudgetCounters After = CaptureCounters(ResidentShortBudget);
			ASSERT_THAT(IsFalse(RejectedTemporary.IsActive()));
			ASSERT_THAT(AreEqual(UINT64_C(0), RejectedTemporary.GetReservedBytes()));
			ASSERT_THAT(IsTrue(ExistingTemporary.IsActive()));
			ASSERT_THAT(AreEqual(UINT64_C(3), ExistingTemporary.GetReservedBytes()));
			ASSERT_THAT(AreEqual(Before.Stored, After.Stored));
			ASSERT_THAT(AreEqual(Before.Decompressed, After.Decompressed));
			ASSERT_THAT(AreEqual(Before.Decoded, After.Decoded));
			ASSERT_THAT(AreEqual(Before.Resident, After.Resident));
			ASSERT_THAT(AreEqual(Before.Temporary, After.Temporary));
			ASSERT_THAT(AreEqual(Before.PeakLive, After.PeakLive));
			ASSERT_THAT(AreEqual(Before.ReferencesAndRelocations, After.ReferencesAndRelocations));
		}

		{
			FAngelscriptCacheReadBudget ActiveOutputBudget;
			const FAngelscriptCacheReadLimits ActiveOutputLimits = MakeDecodedLimits(100, 100);
			FAngelscriptCacheTemporaryResidentReservation ActiveOutput;
			ASSERT_THAT(IsTrue(ActiveOutputBudget.TryReserveTemporaryDecoded(
				7, ActiveOutputLimits, ActiveOutput)));
			const FBudgetCounters Before = CaptureCounters(ActiveOutputBudget);

			ASSERT_THAT(IsFalse(ActiveOutputBudget.TryReserveTemporaryDecoded(
				11, ActiveOutputLimits, ActiveOutput),
				TEXT("An active output guard must never be silently replaced")));
			const FBudgetCounters After = CaptureCounters(ActiveOutputBudget);
			ASSERT_THAT(IsTrue(ActiveOutput.IsActive()));
			ASSERT_THAT(AreEqual(UINT64_C(7), ActiveOutput.GetReservedBytes()));
			ASSERT_THAT(AreEqual(Before.Decoded, After.Decoded));
			ASSERT_THAT(AreEqual(Before.Resident, After.Resident));
			ASSERT_THAT(AreEqual(Before.Temporary, After.Temporary));
			ASSERT_THAT(AreEqual(Before.PeakLive, After.PeakLive));
		}
	}

	TEST_METHOD(ZeroByteOperationsAreNoOpsAndDoNotInstallAReservation)
	{
		FAngelscriptCacheReadBudget Budget;
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(0, 0);
		ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(0, Limits)));
		{
			FAngelscriptCacheTemporaryResidentReservation ZeroReservation;
			ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(0, Limits, ZeroReservation)));
			ASSERT_THAT(IsFalse(ZeroReservation.IsActive(),
				TEXT("A zero-byte temporary acquire is a successful no-op, not an active guard")));
			ASSERT_THAT(AreEqual(UINT64_C(0), ZeroReservation.GetReservedBytes()));
			ASSERT_THAT(IsFalse(ZeroReservation.PromoteToRetained(),
				TEXT("An inactive zero guard cannot be promoted")));
			ZeroReservation.Reset();
		}
		{
			FAngelscriptCacheTemporaryResidentReservation DestructorNoOp;
			ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(0, Limits, DestructorNoOp)));
			ASSERT_THAT(IsFalse(DestructorNoOp.IsActive()));
		}

		const FBudgetCounters Counters = CaptureCounters(Budget);
		ASSERT_THAT(AreEqual(UINT64_C(0), Counters.Stored));
		ASSERT_THAT(AreEqual(UINT64_C(0), Counters.Decompressed));
		ASSERT_THAT(AreEqual(UINT64_C(0), Counters.Decoded));
		ASSERT_THAT(AreEqual(UINT64_C(0), Counters.Resident));
		ASSERT_THAT(AreEqual(UINT64_C(0), Counters.Temporary));
		ASSERT_THAT(AreEqual(UINT64_C(0), Counters.PeakLive));
		ASSERT_THAT(AreEqual(UINT64_C(0), Counters.ReferencesAndRelocations));

		FAngelscriptCacheReadBudget ActiveBudget;
		const FAngelscriptCacheReadLimits ActiveLimits = MakeDecodedLimits(5, 5);
		FAngelscriptCacheTemporaryResidentReservation ActiveReservation;
		ASSERT_THAT(IsTrue(ActiveBudget.TryReserveTemporaryDecoded(
			5, ActiveLimits, ActiveReservation)));
		const FBudgetCounters BeforeRejectedZero = CaptureCounters(ActiveBudget);
		ASSERT_THAT(IsTrue(ActiveBudget.TryConsumeRetainedDecoded(0, ActiveLimits),
			TEXT("A zero-byte retained consume remains a no-op at an exact full boundary")));
		ASSERT_THAT(IsFalse(ActiveBudget.TryReserveTemporaryDecoded(
			0, ActiveLimits, ActiveReservation),
			TEXT("An active output reservation rejects even a zero-byte acquire")));
		const FBudgetCounters AfterRejectedZero = CaptureCounters(ActiveBudget);
		ASSERT_THAT(IsTrue(ActiveReservation.IsActive()));
		ASSERT_THAT(AreEqual(UINT64_C(5), ActiveReservation.GetReservedBytes()));
		ASSERT_THAT(AreEqual(BeforeRejectedZero.Decoded, AfterRejectedZero.Decoded));
		ASSERT_THAT(AreEqual(BeforeRejectedZero.Resident, AfterRejectedZero.Resident));
		ASSERT_THAT(AreEqual(BeforeRejectedZero.Temporary, AfterRejectedZero.Temporary));
		ASSERT_THAT(AreEqual(BeforeRejectedZero.PeakLive, AfterRejectedZero.PeakLive));
	}

	TEST_METHOD(OverflowRejectionsLeaveAllDecodedCountersUnchanged)
	{
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(MAX_uint64, MAX_uint64);

		FAngelscriptCacheReadBudget RetainedBudget;
		ASSERT_THAT(IsTrue(RetainedBudget.TryConsumeRetainedDecoded(MAX_uint64 - 4, Limits)));
		const FBudgetCounters RetainedBefore = CaptureCounters(RetainedBudget);
		ASSERT_THAT(IsFalse(RetainedBudget.TryConsumeRetainedDecoded(5, Limits)));
		const FBudgetCounters RetainedAfter = CaptureCounters(RetainedBudget);
		ASSERT_THAT(AreEqual(RetainedBefore.Decoded, RetainedAfter.Decoded));
		ASSERT_THAT(AreEqual(RetainedBefore.Resident, RetainedAfter.Resident));
		ASSERT_THAT(AreEqual(RetainedBefore.Temporary, RetainedAfter.Temporary));
		ASSERT_THAT(AreEqual(RetainedBefore.PeakLive, RetainedAfter.PeakLive));

		FAngelscriptCacheReadBudget TemporaryBudget;
		FAngelscriptCacheTemporaryResidentReservation ExistingTemporary;
		FAngelscriptCacheTemporaryResidentReservation RejectedTemporary;
		ASSERT_THAT(IsTrue(TemporaryBudget.TryReserveTemporaryDecoded(
			MAX_uint64 - 4, Limits, ExistingTemporary)));
		const FBudgetCounters TemporaryBefore = CaptureCounters(TemporaryBudget);
		ASSERT_THAT(IsFalse(TemporaryBudget.TryReserveTemporaryDecoded(
			5, Limits, RejectedTemporary)));
		const FBudgetCounters TemporaryAfter = CaptureCounters(TemporaryBudget);
		ASSERT_THAT(IsFalse(RejectedTemporary.IsActive()));
		ASSERT_THAT(IsTrue(ExistingTemporary.IsActive()));
		ASSERT_THAT(AreEqual(TemporaryBefore.Decoded, TemporaryAfter.Decoded));
		ASSERT_THAT(AreEqual(TemporaryBefore.Resident, TemporaryAfter.Resident));
		ASSERT_THAT(AreEqual(TemporaryBefore.Temporary, TemporaryAfter.Temporary));
		ASSERT_THAT(AreEqual(TemporaryBefore.PeakLive, TemporaryAfter.PeakLive));
	}

	TEST_METHOD(PeakTracksTheObservedCombinedLiveTimeline)
	{
		FAngelscriptCacheReadBudget Budget;
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(200, 100);
		FAngelscriptCacheTemporaryResidentReservation FirstTemporary;
		FAngelscriptCacheTemporaryResidentReservation SecondTemporary;

		ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(40, Limits)));
		ASSERT_THAT(AreEqual(UINT64_C(40), Budget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(30, Limits, FirstTemporary)));
		ASSERT_THAT(AreEqual(UINT64_C(70), Budget.GetPeakLiveResidentDecodedBytes()));

		FirstTemporary.Reset();
		ASSERT_THAT(AreEqual(UINT64_C(40), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(70), Budget.GetPeakLiveResidentDecodedBytes(),
			TEXT("Releasing scratch cannot lower the historical combined-live peak")));

		ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(20, Limits)));
		ASSERT_THAT(AreEqual(UINT64_C(60), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(70), Budget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(25, Limits, SecondTemporary)));
		ASSERT_THAT(AreEqual(UINT64_C(85),
			Budget.GetResidentDecodedBytes() + Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(85), Budget.GetPeakLiveResidentDecodedBytes(),
			TEXT("Peak is sampled from simultaneous retained plus temporary live bytes")));

		SecondTemporary.Reset();
		ASSERT_THAT(AreEqual(UINT64_C(85), Budget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreNotEqual(UINT64_C(90), Budget.GetPeakLiveResidentDecodedBytes(),
			TEXT("Final resident plus an earlier temporary maximum is not the peak oracle")));
	}

	TEST_METHOD(TemporaryReleaseNeverRefundsMonotonicTotal)
	{
		FAngelscriptCacheReadBudget Budget;
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(40, 40);
		FAngelscriptCacheTemporaryResidentReservation Reservation;

		ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(17, Limits, Reservation)));
		ASSERT_THAT(AreEqual(UINT64_C(17), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(17), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(17), Budget.GetPeakLiveResidentDecodedBytes()));

		Reservation.Reset();
		ASSERT_THAT(IsFalse(Reservation.IsActive()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Reservation.GetReservedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(17), Budget.GetDecodedBytes(),
			TEXT("Scratch release refunds no monotonic TotalDecoded charge")));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(17), Budget.GetPeakLiveResidentDecodedBytes()));

		Reservation.Reset();
		ASSERT_THAT(AreEqual(UINT64_C(17), Budget.GetDecodedBytes(),
			TEXT("Reset on an inactive reservation is idempotent")));
	}

	TEST_METHOD(PromotionReclassifiesLiveBytesWithoutASecondCharge)
	{
		FAngelscriptCacheReadBudget Budget;
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(30, 30);
		FAngelscriptCacheTemporaryResidentReservation Reservation;

		ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(10, Limits)));
		ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(20, Limits, Reservation)));
		const uint64 TotalBefore = Budget.GetDecodedBytes();
		const uint64 LiveBefore =
			Budget.GetResidentDecodedBytes() + Budget.GetTemporaryResidentDecodedBytes();
		const uint64 PeakBefore = Budget.GetPeakLiveResidentDecodedBytes();

		ASSERT_THAT(IsTrue(Reservation.PromoteToRetained()));
		ASSERT_THAT(IsFalse(Reservation.IsActive(),
			TEXT("A promoted guard disables destructor scratch release")));
		ASSERT_THAT(AreEqual(UINT64_C(0), Reservation.GetReservedBytes()));
		ASSERT_THAT(AreEqual(TotalBefore, Budget.GetDecodedBytes(),
			TEXT("Promotion consumes no second TotalDecoded charge")));
		ASSERT_THAT(AreEqual(UINT64_C(30), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(LiveBefore,
			Budget.GetResidentDecodedBytes() + Budget.GetTemporaryResidentDecodedBytes(),
			TEXT("Promotion preserves the combined live total")));
		ASSERT_THAT(AreEqual(PeakBefore, Budget.GetPeakLiveResidentDecodedBytes()));

		Reservation.Reset();
		ASSERT_THAT(AreEqual(UINT64_C(30), Budget.GetResidentDecodedBytes(),
			TEXT("A promoted reservation cannot later release retained bytes")));
		ASSERT_THAT(IsFalse(Reservation.PromoteToRetained(),
			TEXT("Promotion is one-shot")));
	}

	TEST_METHOD(ReservationMoveResetAndDestructorReleaseExactlyOnce)
	{
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(100, 100);

		FAngelscriptCacheReadBudget MoveConstructBudget;
		FAngelscriptCacheTemporaryResidentReservation MoveSource;
		ASSERT_THAT(IsTrue(MoveConstructBudget.TryReserveTemporaryDecoded(
			13, Limits, MoveSource)));
		FAngelscriptCacheTemporaryResidentReservation MoveDestination(MoveTemp(MoveSource));
		ASSERT_THAT(IsFalse(MoveSource.IsActive()));
		ASSERT_THAT(AreEqual(UINT64_C(0), MoveSource.GetReservedBytes()));
		ASSERT_THAT(IsTrue(MoveDestination.IsActive()));
		ASSERT_THAT(AreEqual(UINT64_C(13), MoveDestination.GetReservedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(13),
			MoveConstructBudget.GetTemporaryResidentDecodedBytes()));
		MoveDestination.Reset();
		ASSERT_THAT(AreEqual(UINT64_C(0),
			MoveConstructBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(13), MoveConstructBudget.GetDecodedBytes()));

		FAngelscriptCacheReadBudget ReplacedOwnerBudget;
		FAngelscriptCacheReadBudget TransferredOwnerBudget;
		FAngelscriptCacheTemporaryResidentReservation MoveAssignedDestination;
		FAngelscriptCacheTemporaryResidentReservation MoveAssignedSource;
		ASSERT_THAT(IsTrue(ReplacedOwnerBudget.TryReserveTemporaryDecoded(
			11, Limits, MoveAssignedDestination)));
		ASSERT_THAT(IsTrue(TransferredOwnerBudget.TryReserveTemporaryDecoded(
			17, Limits, MoveAssignedSource)));

		MoveAssignedDestination = MoveTemp(MoveAssignedSource);
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ReplacedOwnerBudget.GetTemporaryResidentDecodedBytes(),
			TEXT("Move assignment releases the destination's previous reservation once")));
		ASSERT_THAT(AreEqual(UINT64_C(11), ReplacedOwnerBudget.GetDecodedBytes()));
		ASSERT_THAT(IsFalse(MoveAssignedSource.IsActive()));
		ASSERT_THAT(IsTrue(MoveAssignedDestination.IsActive()));
		ASSERT_THAT(AreEqual(UINT64_C(17), MoveAssignedDestination.GetReservedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(17),
			TransferredOwnerBudget.GetTemporaryResidentDecodedBytes()));
		MoveAssignedDestination.Reset();
		ASSERT_THAT(AreEqual(UINT64_C(0),
			TransferredOwnerBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(17), TransferredOwnerBudget.GetDecodedBytes()));

		FAngelscriptCacheReadBudget DestructorBudget;
		{
			FAngelscriptCacheTemporaryResidentReservation ScopedReservation;
			ASSERT_THAT(IsTrue(DestructorBudget.TryReserveTemporaryDecoded(
				23, Limits, ScopedReservation)));
			ASSERT_THAT(AreEqual(UINT64_C(23),
				DestructorBudget.GetTemporaryResidentDecodedBytes()));
		}
		ASSERT_THAT(AreEqual(UINT64_C(0), DestructorBudget.GetTemporaryResidentDecodedBytes(),
			TEXT("The active guard destructor releases scratch exactly once")));
		ASSERT_THAT(AreEqual(UINT64_C(23), DestructorBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(23), DestructorBudget.GetPeakLiveResidentDecodedBytes()));
	}

	TEST_METHOD(DecodedCandidateExtendsMultipleSitesAndPromotesExactlyOnce)
	{
		using FBudget = FAngelscriptCacheReadBudget;
		using EExtendResult = FBudget::EDecodedCandidateExtendResult;

		FBudget Budget;
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(30, 30);
		FAngelscriptCacheTemporaryResidentReservation ExistingScratch;
		ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(7, Limits)));
		ASSERT_THAT(IsTrue(Budget.TryReserveTemporaryDecoded(5, Limits, ExistingScratch)));
		const FBudgetCounters BeforeBegin = CaptureCounters(Budget);

		auto Candidate = Budget.BeginDecodedCandidateTransaction(Limits);
		const FBudgetCounters AfterBegin = CaptureCounters(Budget);
		ASSERT_THAT(AreEqual(BeforeBegin.Decoded, AfterBegin.Decoded));
		ASSERT_THAT(AreEqual(BeforeBegin.Resident, AfterBegin.Resident));
		ASSERT_THAT(AreEqual(BeforeBegin.Temporary, AfterBegin.Temporary));
		ASSERT_THAT(AreEqual(BeforeBegin.PeakLive, AfterBegin.PeakLive));
		ASSERT_THAT(IsTrue(Candidate.IsOpen()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Candidate.GetAggregateTemporaryBytes()));

		ASSERT_THAT(AreEqual(EExtendResult::Success, Candidate.TryExtend(0)));
		ASSERT_THAT(AreEqual(BeforeBegin.Decoded, Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Candidate.GetAggregateTemporaryBytes()));

		ASSERT_THAT(AreEqual(EExtendResult::Success, Candidate.TryExtend(8)));
		ASSERT_THAT(AreEqual(UINT64_C(20), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(7), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(13), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(8), Candidate.GetAggregateTemporaryBytes()));

		ASSERT_THAT(AreEqual(EExtendResult::Success, Candidate.TryExtend(10)));
		ASSERT_THAT(AreEqual(UINT64_C(30), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(7), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(23), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(18), Candidate.GetAggregateTemporaryBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(30), Budget.GetPeakLiveResidentDecodedBytes()));

		const FBudgetCounters BeforeRejectedExtension = CaptureCounters(Budget);
		ASSERT_THAT(AreEqual(EExtendResult::BudgetExceeded, Candidate.TryExtend(1)));
		const FBudgetCounters AfterRejectedExtension = CaptureCounters(Budget);
		ASSERT_THAT(AreEqual(BeforeRejectedExtension.Decoded, AfterRejectedExtension.Decoded));
		ASSERT_THAT(AreEqual(BeforeRejectedExtension.Resident, AfterRejectedExtension.Resident));
		ASSERT_THAT(AreEqual(BeforeRejectedExtension.Temporary, AfterRejectedExtension.Temporary));
		ASSERT_THAT(AreEqual(BeforeRejectedExtension.PeakLive, AfterRejectedExtension.PeakLive));
		ASSERT_THAT(AreEqual(UINT64_C(18), Candidate.GetAggregateTemporaryBytes()));

		ASSERT_THAT(IsTrue(Candidate.PromoteToRetained()));
		ASSERT_THAT(IsFalse(Candidate.IsOpen()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Candidate.GetAggregateTemporaryBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(30), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(25), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(5), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(30), Budget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(IsFalse(Candidate.PromoteToRetained(),
			TEXT("decoded-candidate promotion is one-shot")));

		ExistingScratch.Reset();
		ASSERT_THAT(AreEqual(UINT64_C(30), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(25), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(DecodedCandidateShortAndOverflowExtensionsAreSideEffectFree)
	{
		using FBudget = FAngelscriptCacheReadBudget;
		using EExtendResult = FBudget::EDecodedCandidateExtendResult;

		FBudget TotalShortBudget;
		const FAngelscriptCacheReadLimits TotalShortLimits = MakeDecodedLimits(20, 100);
		ASSERT_THAT(IsTrue(TotalShortBudget.TryConsumeRetainedDecoded(4, TotalShortLimits)));
		{
			auto Candidate = TotalShortBudget.BeginDecodedCandidateTransaction(TotalShortLimits);
			ASSERT_THAT(AreEqual(EExtendResult::Success, Candidate.TryExtend(10)));
			const FBudgetCounters Before = CaptureCounters(TotalShortBudget);
			ASSERT_THAT(AreEqual(EExtendResult::BudgetExceeded, Candidate.TryExtend(7)));
			const FBudgetCounters After = CaptureCounters(TotalShortBudget);
			ASSERT_THAT(AreEqual(Before.Decoded, After.Decoded));
			ASSERT_THAT(AreEqual(Before.Resident, After.Resident));
			ASSERT_THAT(AreEqual(Before.Temporary, After.Temporary));
			ASSERT_THAT(AreEqual(Before.PeakLive, After.PeakLive));
			ASSERT_THAT(AreEqual(UINT64_C(10), Candidate.GetAggregateTemporaryBytes()));
		}
		ASSERT_THAT(AreEqual(UINT64_C(14), TotalShortBudget.GetDecodedBytes(),
			TEXT("candidate failure release never refunds accepted TotalDecoded")));
		ASSERT_THAT(AreEqual(UINT64_C(4), TotalShortBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), TotalShortBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(14), TotalShortBudget.GetPeakLiveResidentDecodedBytes()));

		FBudget ResidentShortBudget;
		const FAngelscriptCacheReadLimits ResidentShortLimits = MakeDecodedLimits(100, 20);
		FAngelscriptCacheTemporaryResidentReservation ExistingScratch;
		ASSERT_THAT(IsTrue(ResidentShortBudget.TryConsumeRetainedDecoded(4, ResidentShortLimits)));
		ASSERT_THAT(IsTrue(ResidentShortBudget.TryReserveTemporaryDecoded(
			5, ResidentShortLimits, ExistingScratch)));
		{
			auto Candidate = ResidentShortBudget.BeginDecodedCandidateTransaction(ResidentShortLimits);
			ASSERT_THAT(AreEqual(EExtendResult::Success, Candidate.TryExtend(10)));
			const FBudgetCounters Before = CaptureCounters(ResidentShortBudget);
			ASSERT_THAT(AreEqual(EExtendResult::BudgetExceeded, Candidate.TryExtend(2)));
			const FBudgetCounters After = CaptureCounters(ResidentShortBudget);
			ASSERT_THAT(AreEqual(Before.Decoded, After.Decoded));
			ASSERT_THAT(AreEqual(Before.Resident, After.Resident));
			ASSERT_THAT(AreEqual(Before.Temporary, After.Temporary));
			ASSERT_THAT(AreEqual(Before.PeakLive, After.PeakLive));
		}
		ASSERT_THAT(AreEqual(UINT64_C(19), ResidentShortBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(4), ResidentShortBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(5), ResidentShortBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(19), ResidentShortBudget.GetPeakLiveResidentDecodedBytes()));
		ExistingScratch.Reset();

		FBudget OverflowBudget;
		const FAngelscriptCacheReadLimits OverflowLimits =
			MakeDecodedLimits(MAX_uint64, MAX_uint64);
		{
			auto Candidate = OverflowBudget.BeginDecodedCandidateTransaction(OverflowLimits);
			ASSERT_THAT(AreEqual(EExtendResult::Success, Candidate.TryExtend(MAX_uint64 - 4)));
			const FBudgetCounters Before = CaptureCounters(OverflowBudget);
			ASSERT_THAT(AreEqual(EExtendResult::Overflow, Candidate.TryExtend(5)));
			const FBudgetCounters After = CaptureCounters(OverflowBudget);
			ASSERT_THAT(AreEqual(Before.Decoded, After.Decoded));
			ASSERT_THAT(AreEqual(Before.Temporary, After.Temporary));
			ASSERT_THAT(AreEqual(Before.PeakLive, After.PeakLive));
			ASSERT_THAT(AreEqual(MAX_uint64 - 4,
				Candidate.GetAggregateTemporaryBytes()));
		}
		ASSERT_THAT(AreEqual(MAX_uint64 - 4, OverflowBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), OverflowBudget.GetTemporaryResidentDecodedBytes()));
	}

	TEST_METHOD(OverlappingCandidateTransactionsPromoteAndRollbackOnlyTheirOwnBytes)
	{
		using FBudget = FAngelscriptCacheReadBudget;
		using EExtendResult = FBudget::EDecodedCandidateExtendResult;

		FBudget Budget;
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(64, 64);
		auto First = Budget.BeginDecodedCandidateTransaction(Limits);
		auto Second = Budget.BeginDecodedCandidateTransaction(Limits);

		ASSERT_THAT(AreEqual(EExtendResult::Success, First.TryExtend(11)));
		ASSERT_THAT(AreEqual(EExtendResult::Success, Second.TryExtend(7)));
		ASSERT_THAT(AreEqual(EExtendResult::Success, First.TryExtend(5)));
		ASSERT_THAT(AreEqual(UINT64_C(16), First.GetAggregateTemporaryBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(7), Second.GetAggregateTemporaryBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(23), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(23),
			Budget.GetTemporaryResidentDecodedBytes()));

		ASSERT_THAT(IsTrue(Second.PromoteToRetained()));
		ASSERT_THAT(IsTrue(First.IsOpen(),
			TEXT("promoting one transaction cannot close another")));
		ASSERT_THAT(AreEqual(UINT64_C(23), Budget.GetDecodedBytes(),
			TEXT("promotion adds no second TotalDecoded charge")));
		ASSERT_THAT(AreEqual(UINT64_C(7), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(16),
			Budget.GetTemporaryResidentDecodedBytes(),
			TEXT("only the promoted transaction is reclassified")));

		{
			auto RolledBack = MoveTemp(First);
			ASSERT_THAT(IsFalse(First.IsOpen()));
			ASSERT_THAT(IsTrue(RolledBack.IsOpen()));
		}

		ASSERT_THAT(AreEqual(UINT64_C(23), Budget.GetDecodedBytes(),
			TEXT("rollback never refunds monotonic TotalDecoded")));
		ASSERT_THAT(AreEqual(UINT64_C(7), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(23),
			Budget.GetPeakLiveResidentDecodedBytes()));
	}

	TEST_METHOD(DecodedCandidateSnapshotsTemporaryAndMutableLimitsAtBegin)
	{
		using FBudget = FAngelscriptCacheReadBudget;
		using EExtendResult = FBudget::EDecodedCandidateExtendResult;

		FBudget TemporaryLimitsBudget;
		{
			auto Candidate = TemporaryLimitsBudget.BeginDecodedCandidateTransaction(
				MakeDecodedLimits(9, 9));
			ASSERT_THAT(AreEqual(EExtendResult::Success, Candidate.TryExtend(9)));
			ASSERT_THAT(AreEqual(EExtendResult::BudgetExceeded, Candidate.TryExtend(1)));
		}
		ASSERT_THAT(AreEqual(UINT64_C(9), TemporaryLimitsBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			TemporaryLimitsBudget.GetTemporaryResidentDecodedBytes()));

		FBudget MutableLimitsBudget;
		FAngelscriptCacheReadLimits MutableLimits = MakeDecodedLimits(10, 10);
		auto Candidate = MutableLimitsBudget.BeginDecodedCandidateTransaction(MutableLimits);
		MutableLimits.MaxTotalDecodedBytes = 100;
		MutableLimits.MaxResidentDecodedBytes = 100;
		ASSERT_THAT(AreEqual(EExtendResult::BudgetExceeded, Candidate.TryExtend(11),
			TEXT("The transaction must retain its Begin-time limit snapshot")));
		ASSERT_THAT(AreEqual(UINT64_C(0), MutableLimitsBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Candidate.GetAggregateTemporaryBytes()));
	}

	TEST_METHOD(DecodedCandidateMoveConstructionPromotionAndDestructionAreExactlyOnce)
	{
		using FBudget = FAngelscriptCacheReadBudget;
		using FCandidate = FBudget::FDecodedCandidateTransaction;
		using EExtendResult = FBudget::EDecodedCandidateExtendResult;

		static_assert(!std::is_copy_constructible_v<FCandidate>);
		static_assert(!std::is_copy_assignable_v<FCandidate>);
		static_assert(std::is_nothrow_move_constructible_v<FCandidate>);
		static_assert(!std::is_move_assignable_v<FCandidate>);

		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(100, 100);
		FBudget PromotionBudget;
		{
			auto Source = PromotionBudget.BeginDecodedCandidateTransaction(Limits);
			ASSERT_THAT(AreEqual(EExtendResult::Success, Source.TryExtend(13)));
			FCandidate Destination(MoveTemp(Source));
			ASSERT_THAT(IsFalse(Source.IsOpen()));
			ASSERT_THAT(AreEqual(EExtendResult::InvalidState, Source.TryExtend(1)));
			ASSERT_THAT(IsTrue(Destination.IsOpen()));
			ASSERT_THAT(AreEqual(UINT64_C(13), Destination.GetAggregateTemporaryBytes()));
			ASSERT_THAT(IsTrue(Destination.PromoteToRetained()));
			ASSERT_THAT(IsFalse(Destination.PromoteToRetained(),
				TEXT("A moved decoded candidate can be promoted exactly once")));
		}
		ASSERT_THAT(AreEqual(UINT64_C(13), PromotionBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(13), PromotionBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			PromotionBudget.GetTemporaryResidentDecodedBytes()));

		FBudget DestructorBudget;
		{
			auto Candidate = DestructorBudget.BeginDecodedCandidateTransaction(Limits);
			ASSERT_THAT(AreEqual(EExtendResult::Success, Candidate.TryExtend(17)));
			ASSERT_THAT(AreEqual(UINT64_C(17),
				DestructorBudget.GetTemporaryResidentDecodedBytes()));
		}
		ASSERT_THAT(AreEqual(UINT64_C(0),
			DestructorBudget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(17), DestructorBudget.GetDecodedBytes(),
			TEXT("Candidate destruction releases live scratch without refunding Total")));
	}

	TEST_METHOD(ThreadAffinityBindsOnFirstMutationAndIsPerBudget)
	{
#if DO_CHECK
		const uint32 MainThreadId = FPlatformTLS::GetCurrentThreadId();
		FAngelscriptCacheReadBudget MainBudget;
		const FAngelscriptCacheReadLimits Limits = MakeDecodedLimits(64, 64);

		ASSERT_THAT(AreEqual(UINT32_C(0), MainBudget.OwnerThreadId,
			TEXT("A fresh Budget is unbound")));
		ASSERT_THAT(AreEqual(UINT64_C(0), MainBudget.GetStoredBytes()));
		ASSERT_THAT(AreEqual(UINT32_C(0), MainBudget.OwnerThreadId,
			TEXT("A const read does not claim thread ownership")));

		ASSERT_THAT(IsTrue(MainBudget.TryConsumeStored(0, Limits)));
		ASSERT_THAT(AreEqual(MainThreadId, MainBudget.OwnerThreadId,
			TEXT("Even a zero-byte mutation binds the Budget to its caller")));

		const TFuture<TTuple<uint32, uint32, uint64>> Worker = Async(
			EAsyncExecution::ThreadPool,
			[Limits]()
			{
				FAngelscriptCacheReadBudget WorkerBudget;
				const uint32 WorkerThreadId = FPlatformTLS::GetCurrentThreadId();
				const bool bConsumed = WorkerBudget.TryConsumeStored(7, Limits);
				return MakeTuple(
					WorkerThreadId,
					WorkerBudget.OwnerThreadId,
					bConsumed ? WorkerBudget.GetStoredBytes() : MAX_uint64);
			});

		const TTuple<uint32, uint32, uint64> WorkerResult = Worker.Get();
		ASSERT_THAT(IsTrue(MainThreadId != WorkerResult.Get<0>(),
			TEXT("The worker proof must execute on another thread")));
		ASSERT_THAT(AreEqual(WorkerResult.Get<0>(), WorkerResult.Get<1>(),
			TEXT("The worker Budget binds to the worker rather than shared ambient state")));
		ASSERT_THAT(AreEqual(UINT64_C(7), WorkerResult.Get<2>()));
		ASSERT_THAT(AreEqual(MainThreadId, MainBudget.OwnerThreadId,
			TEXT("A second Budget cannot change the first Budget's owner")));
#else
		ASSERT_THAT(IsTrue(true,
			TEXT("Thread-affinity diagnostics are intentionally compiled only with checks")));
#endif
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
