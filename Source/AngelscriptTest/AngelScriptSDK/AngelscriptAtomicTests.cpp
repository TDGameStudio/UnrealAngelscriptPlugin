// =============================================================================
// AngelscriptSDKAtomicTests.cpp
//
// Tests for as_atomic.cpp — thread-safe reference counting via asCAtomic.
// Validates basic get/set, atomic inc/dec, and concurrent safety.
//
// Automation IDs:
//   Angelscript.TestModule.AngelScriptSDK.Atomic.*
// =============================================================================

#include "CQTest.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_atomic.h"
#include "EndAngelscriptHeaders.h"

// TODO: asCAtomic symbols not exported from AngelscriptRuntime. Disabled until linkage resolved.
#if WITH_ANGELSCRIPT_UNITTESTS && 0

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKAtomicTest,
	"Angelscript.TestModule.AngelScriptSDK.Atomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 GNumThreads = 4;
	static constexpr int32 GIterationsPerThread = 1000;

	class FAtomicIncDecRunnable : public FRunnable
	{
	public:
		explicit FAtomicIncDecRunnable(asCAtomic& InAtomic)
			: Atomic(InAtomic)
		{
		}

		virtual uint32 Run() override
		{
			for (int32 I = 0; I < GIterationsPerThread; ++I)
			{
				Atomic.atomicInc();
			}
			for (int32 I = 0; I < GIterationsPerThread; ++I)
			{
				Atomic.atomicDec();
			}
			return 0;
		}

	private:
		asCAtomic& Atomic;
	};

public:
	// -------------------------------------------------------------------------
	// InitZero — default constructed asCAtomic should read as 0
	// -------------------------------------------------------------------------

	TEST_METHOD(InitZero)
	{
		asCAtomic Atomic;
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(Atomic.get()),
			TEXT("Default-constructed asCAtomic value should be 0")));
	}

	// -------------------------------------------------------------------------
	// SetGet — set(42) then get() should return 42
	// -------------------------------------------------------------------------

	TEST_METHOD(SetGet)
	{
		asCAtomic Atomic;

		Atomic.set(42);
		ASSERT_THAT(AreEqual(
			42,
			static_cast<int32>(Atomic.get()),
			TEXT("set(42) followed by get() should return 42")));

		Atomic.set(0);
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(Atomic.get()),
			TEXT("set(0) followed by get() should return 0")));

		Atomic.set(999);
		ASSERT_THAT(AreEqual(
			999,
			static_cast<int32>(Atomic.get()),
			TEXT("set(999) followed by get() should return 999")));
	}

	// -------------------------------------------------------------------------
	// IncDec — atomicInc/atomicDec should adjust value by 1
	// -------------------------------------------------------------------------

	TEST_METHOD(IncDec)
	{
		asCAtomic Atomic;

		const asDWORD After1 = Atomic.atomicInc();
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(After1),
			TEXT("atomicInc from 0 should return 1")));
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(Atomic.get()),
			TEXT("get() after one atomicInc should be 1")));

		const asDWORD After2 = Atomic.atomicInc();
		ASSERT_THAT(AreEqual(
			2,
			static_cast<int32>(After2),
			TEXT("atomicInc from 1 should return 2")));

		const asDWORD After3 = Atomic.atomicDec();
		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(After3),
			TEXT("atomicDec from 2 should return 1")));

		const asDWORD After4 = Atomic.atomicDec();
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(After4),
			TEXT("atomicDec from 1 should return 0")));
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(Atomic.get()),
			TEXT("get() after balanced inc/dec should be 0")));
	}

	// -------------------------------------------------------------------------
	// ConcurrentIncDec — multiple threads inc then dec, final value must be 0
	// -------------------------------------------------------------------------

	TEST_METHOD(ConcurrentIncDec)
	{

		asCAtomic Atomic;

		TArray<TUniquePtr<FAtomicIncDecRunnable>> Runnables;
		TArray<FRunnableThread*> Threads;

		Runnables.Reserve(GNumThreads);
		Threads.Reserve(GNumThreads);

		for (int32 I = 0; I < GNumThreads; ++I)
		{
			Runnables.Add(MakeUnique<FAtomicIncDecRunnable>(Atomic));
			Threads.Add(FRunnableThread::Create(
				Runnables.Last().Get(),
				*FString::Printf(TEXT("AtomicTest_%d"), I)));
		}

		for (FRunnableThread* Thread : Threads)
		{
			if (Thread != nullptr)
			{
				Thread->WaitForCompletion();
				delete Thread;
			}
		}

		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(Atomic.get()),
			TEXT("After balanced concurrent inc/dec across all threads, value should be 0")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && disabled atomic-linkage coverage
