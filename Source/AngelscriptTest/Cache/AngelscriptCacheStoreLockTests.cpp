#include "Cache/AngelscriptCacheStore.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreLockTests,
	"Angelscript.TestModule.Cache.StoreLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FTestLockHandle final : public IAngelscriptCacheNamespaceLockHandle
	{
	};

	class FScriptedLockOps final : public IAngelscriptCacheNamespaceLockOps
	{
	public:
		virtual double MonotonicSeconds() const override
		{
			return NowSeconds;
		}

		virtual TUniquePtr<IAngelscriptCacheNamespaceLockHandle> TryAcquire(
			const FString& LockName,
			const FTimespan WaitSlice) override
		{
			ObservedNames.Add(LockName);
			ObservedWaitTicks.Add(WaitSlice.GetTicks());
			if (FailedAttempts < FailuresBeforeSuccess)
			{
				++FailedAttempts;
				NowSeconds += WaitSlice.GetTotalSeconds();
				if (bCancelAfterFirstFailure && FailedAttempts == 1)
				{
					bCancelled = true;
				}
				return nullptr;
			}
			return MakeUnique<FTestLockHandle>();
		}

		double NowSeconds = 0.0;
		int32 FailuresBeforeSuccess = 0;
		int32 FailedAttempts = 0;
		bool bCancelAfterFirstFailure = false;
		bool bCancelled = false;
		TArray<FString> ObservedNames;
		TArray<int64> ObservedWaitTicks;
	};

	static FAngelscriptCacheStorePaths MakePaths(const TCHAR* Leaf)
	{
		FAngelscriptCacheStorePaths Paths;
		Paths.BaseRootIdentity = TEXT("d:/project/saved/angelscript/cachev2");
		Paths.NamespaceIdentity = Paths.BaseRootIdentity / Leaf;
		return Paths;
	}

	void AssertUncommittedLockFailure(
		const FAngelscriptCacheStoreResult& Result,
		const EAngelscriptCacheStoreError ExpectedError)
	{
		ASSERT_THAT(AreEqual(ExpectedError, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::LockAcquisition, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted, Result.CommitState));
	}

public:
	TEST_METHOD(BuildsTheFrozenFullHashLockNameFromTheCanonicalNamespace)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths(
			TEXT("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef/"
				 "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"));
		FString LockName;

		const FAngelscriptCacheStoreResult Result =
			BuildAngelscriptCacheNamespaceLockName(Paths, LockName);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(FString(TEXT(
			"UEASCacheV2-87a1962c00d565775a6366175b58d47d"
			"c803a842df70798ec7e5432d2ce82708")), LockName,
			*FString::Printf(TEXT("Lock-name golden actual=%s"), *LockName)));
		ASSERT_THAT(IsTrue(LockName.StartsWith(TEXT("UEASCacheV2-"))));
		ASSERT_THAT(AreEqual(76, LockName.Len()));

		FString OtherLockName;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheNamespaceLockName(
			MakePaths(TEXT("other")), OtherLockName).IsSuccess()));
		ASSERT_THAT(AreNotEqual(LockName, OtherLockName));
	}

	TEST_METHOD(AcquiresAfterBoundedOneHundredMillisecondSlices)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths(TEXT("namespace"));
		FScriptedLockOps LockOps;
		LockOps.FailuresBeforeSuccess = 2;
		TUniquePtr<IAngelscriptCacheNamespaceLockHandle> Lock;

		const FAngelscriptCacheStoreResult Result = AcquireAngelscriptCacheNamespaceLock(
			Paths,
			0.25,
			[&LockOps]() { return LockOps.bCancelled; },
			LockOps,
			Lock);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsNotNull(Lock.Get()));
		ASSERT_THAT(AreEqual(3, LockOps.ObservedWaitTicks.Num()));
		for (const int64 WaitTicks : LockOps.ObservedWaitTicks)
		{
			ASSERT_THAT(IsTrue(WaitTicks > 0));
			ASSERT_THAT(IsTrue(
				WaitTicks <= FTimespan::FromMilliseconds(100).GetTicks()));
		}
		ASSERT_THAT(AreEqual(
			FTimespan::FromMilliseconds(100).GetTicks(),
			LockOps.ObservedWaitTicks[0]));
		ASSERT_THAT(AreEqual(
			FTimespan::FromMilliseconds(100).GetTicks(),
			LockOps.ObservedWaitTicks[1]));
	}

	TEST_METHOD(CancellationBeforeAndBetweenSlicesNeverCommits)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths(TEXT("namespace"));
		FScriptedLockOps BeforeWait;
		BeforeWait.bCancelled = true;
		TUniquePtr<IAngelscriptCacheNamespaceLockHandle> Lock;
		const FAngelscriptCacheStoreResult BeforeResult =
			AcquireAngelscriptCacheNamespaceLock(
				Paths,
				1.0,
				[&BeforeWait]() { return BeforeWait.bCancelled; },
				BeforeWait,
				Lock);
		AssertUncommittedLockFailure(
			BeforeResult, EAngelscriptCacheStoreError::Cancelled);
		ASSERT_THAT(IsNull(Lock.Get()));
		ASSERT_THAT(AreEqual(0, BeforeWait.ObservedWaitTicks.Num()));

		FScriptedLockOps BetweenSlices;
		BetweenSlices.FailuresBeforeSuccess = MAX_int32;
		BetweenSlices.bCancelAfterFirstFailure = true;
		const FAngelscriptCacheStoreResult BetweenResult =
			AcquireAngelscriptCacheNamespaceLock(
				Paths,
				1.0,
				[&BetweenSlices]() { return BetweenSlices.bCancelled; },
				BetweenSlices,
				Lock);
		AssertUncommittedLockFailure(
			BetweenResult, EAngelscriptCacheStoreError::Cancelled);
		ASSERT_THAT(IsNull(Lock.Get()));
		ASSERT_THAT(AreEqual(1, BetweenSlices.ObservedWaitTicks.Num()));
	}

	TEST_METHOD(DeadlineReturnsLockTimeoutAfterOnlyBoundedSlices)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths(TEXT("namespace"));
		FScriptedLockOps LockOps;
		LockOps.FailuresBeforeSuccess = MAX_int32;
		TUniquePtr<IAngelscriptCacheNamespaceLockHandle> Lock;

		const FAngelscriptCacheStoreResult Result = AcquireAngelscriptCacheNamespaceLock(
			Paths,
			0.25,
			[&LockOps]() { return LockOps.bCancelled; },
			LockOps,
			Lock);

		AssertUncommittedLockFailure(Result, EAngelscriptCacheStoreError::LockTimeout);
		ASSERT_THAT(IsNull(Lock.Get()));
		ASSERT_THAT(AreEqual(3, LockOps.ObservedWaitTicks.Num()));
		ASSERT_THAT(IsTrue(
			LockOps.ObservedWaitTicks.Last()
			<= FTimespan::FromMilliseconds(50).GetTicks() + 1));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
