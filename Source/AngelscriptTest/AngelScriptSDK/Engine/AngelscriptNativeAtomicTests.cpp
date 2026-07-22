#include "CQTest.h"

// Raw SDK atomic implementation coverage.

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_atomic.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAtomicTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.Atomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 WorkerCount = 4;
	static constexpr int32 OperationsPerWorker = 4096;

	class FAtomicWorker final : public FRunnable
	{
	public:
		explicit FAtomicWorker(asCAtomic& InAtomic)
			: Atomic(InAtomic)
		{
		}

		uint32 Run() override
		{
			for (int32 Index = 0; Index < OperationsPerWorker; ++Index)
			{
				Atomic.atomicInc();
			}

			for (int32 Index = 0; Index < OperationsPerWorker; ++Index)
			{
				Atomic.atomicDec();
			}

			return 0;
		}

	private:
		asCAtomic& Atomic;
	};

public:
	TEST_METHOD(DefaultConstructionStartsAtZero)
	{
		asCAtomic Atomic;
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Atomic.get()),
			TEXT("Default-constructed atomic value should start at zero")));
	}

	TEST_METHOD(SetAndGetPreserveValue)
	{
		asCAtomic Atomic;
		Atomic.set(42);
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Atomic.get()),
			TEXT("Atomic get should return the last value set")));

		Atomic.set(0);
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Atomic.get()),
			TEXT("Atomic value should support resetting to zero")));
	}

	TEST_METHOD(IncrementAndDecrementReturnExpectedValues)
	{
		asCAtomic Atomic;
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Atomic.atomicInc()),
			TEXT("Increment should return the incremented value")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Atomic.atomicInc()),
			TEXT("Second increment should observe the previous result")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Atomic.atomicDec()),
			TEXT("Decrement should return the decremented value")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Atomic.get()),
			TEXT("Read should observe the result of increment/decrement operations")));
	}

	TEST_METHOD(ConcurrentIncrementAndDecrementRemainBalanced)
	{
		asCAtomic Atomic;
		TArray<TUniquePtr<FAtomicWorker>> Workers;
		TArray<FRunnableThread*> Threads;
		Workers.Reserve(WorkerCount);
		Threads.Reserve(WorkerCount);

		for (int32 WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
		{
			Workers.Add(MakeUnique<FAtomicWorker>(Atomic));
			FRunnableThread* const Thread = FRunnableThread::Create(
				Workers.Last().Get(),
				*FString::Printf(TEXT("AngelscriptAtomic_%d"), WorkerIndex));
			ASSERT_THAT(IsNotNull(Thread, TEXT("Atomic worker thread should be created")));
			if (Thread == nullptr)
			{
				continue;
			}

			Threads.Add(Thread);
		}

		for (FRunnableThread* const Thread : Threads)
		{
			Thread->WaitForCompletion();
			delete Thread;
		}

		ASSERT_THAT(AreEqual(0, static_cast<int32>(Atomic.get()),
			TEXT("Balanced concurrent increments and decrements should restore zero")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
