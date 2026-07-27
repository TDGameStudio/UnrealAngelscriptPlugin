#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"

// Raw SDK thread-manager and thread-local-data coverage.

#include "HAL/Event.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FThreadingTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.Threading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 WorkerCount = 3;

	class FThreadLocalDataWorker final : public FRunnable
	{
	public:
		FThreadLocalDataWorker(FEvent& InReady, FEvent& InRelease)
			: Ready(InReady)
			, Release(InRelease)
		{
		}

		uint32 Run() override
		{
			First = asCThreadManager::GetLocalData();
			Second = asCThreadManager::GetLocalData();
			Ready.Trigger();
			Release.Wait();
			return 0;
		}

		asCThreadLocalData* First = nullptr;
		asCThreadLocalData* Second = nullptr;

	private:
		FEvent& Ready;
		FEvent& Release;
	};

public:
	TEST_METHOD(PreparedThreadReturnsLocalData)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-TLS-MAIN-THREAD-STABILITY",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Isolation);

		asCThreadLocalData* const First = asCThreadManager::GetLocalData();
		asCThreadLocalData* const Second = asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(First, TEXT("Thread manager should create local data for the main thread")));
		ASSERT_THAT(AreEqual(First, Second, TEXT("Repeated main-thread lookup should return the same local data")));

		FEvent* const Ready = FPlatformProcess::GetSynchEventFromPool(true);
		FEvent* const Release = FPlatformProcess::GetSynchEventFromPool(true);
		ASSERT_THAT(IsNotNull(Ready, TEXT("Main-thread TLS isolation should allocate a worker-ready event")));
		ASSERT_THAT(IsNotNull(Release, TEXT("Main-thread TLS isolation should allocate a worker-release event")));
		if (Ready == nullptr || Release == nullptr)
		{
			if (Ready != nullptr)
			{
				FPlatformProcess::ReturnSynchEventToPool(Ready);
			}
			if (Release != nullptr)
			{
				FPlatformProcess::ReturnSynchEventToPool(Release);
			}
			return;
		}

		FThreadLocalDataWorker Worker(*Ready, *Release);
		FRunnableThread* Thread = FRunnableThread::Create(
			&Worker,
			TEXT("AngelscriptMainThreadTlsIsolation"));
		ON_SCOPE_EXIT
		{
			Release->Trigger();
			if (Thread != nullptr)
			{
				Thread->WaitForCompletion();
				delete Thread;
			}
			FPlatformProcess::ReturnSynchEventToPool(Release);
			FPlatformProcess::ReturnSynchEventToPool(Ready);
		};
		ASSERT_THAT(IsNotNull(
			Thread,
			TEXT("Main-thread TLS isolation should create a simultaneous worker")));
		if (Thread == nullptr)
		{
			return;
		}

		Ready->Wait();
		ASSERT_THAT(IsNotNull(
			Worker.First,
			TEXT("Simultaneous TLS worker should publish its local-data identity")));
		ASSERT_THAT(AreEqual(
			Worker.First,
			Worker.Second,
			TEXT("Simultaneous TLS worker should retain a stable repeated identity")));
		ASSERT_THAT(AreNotEqual(
			First,
			Worker.First,
			TEXT("Main-thread TLS identity should remain isolated from a simultaneous worker")));
	}

	TEST_METHOD(WorkerThreadsReceiveDistinctLocalData)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-TLS-WORKER-ISOLATION",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asCThreadLocalData* const MainThreadData = asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(MainThreadData, TEXT("Main thread should have local data before worker creation")));

		FEvent* const Release = FPlatformProcess::GetSynchEventFromPool(true);
		ASSERT_THAT(IsNotNull(Release, TEXT("Worker-release event should be allocated")));
		if (Release == nullptr)
		{
			return;
		}

		TArray<FEvent*> ReadyEvents;
		TArray<TUniquePtr<FThreadLocalDataWorker>> Workers;
		TArray<FRunnableThread*> Threads;
		ReadyEvents.Reserve(WorkerCount);
		Workers.Reserve(WorkerCount);
		Threads.Reserve(WorkerCount);

		for (int32 WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
		{
			FEvent* const Ready = FPlatformProcess::GetSynchEventFromPool(true);
			ASSERT_THAT(IsNotNull(Ready, TEXT("Worker-ready event should be allocated")));
			if (Ready == nullptr)
			{
				continue;
			}

			ReadyEvents.Add(Ready);
			Workers.Add(MakeUnique<FThreadLocalDataWorker>(*Ready, *Release));
			FRunnableThread* const Thread = FRunnableThread::Create(
				Workers.Last().Get(),
				*FString::Printf(TEXT("AngelscriptThreadLocalData_%d"), WorkerIndex));
			ASSERT_THAT(IsNotNull(Thread, TEXT("Thread-local-data worker should be created")));
			if (Thread == nullptr)
			{
				Workers.Pop();
				FPlatformProcess::ReturnSynchEventToPool(ReadyEvents.Pop());
				continue;
			}
			Threads.Add(Thread);
		}

		ASSERT_THAT(AreEqual(
			WorkerCount,
			Workers.Num(),
			TEXT("Every requested TLS worker should be created")));
		for (FEvent* const Ready : ReadyEvents)
		{
			Ready->Wait();
		}

		for (int32 WorkerIndex = 0; WorkerIndex < Workers.Num(); ++WorkerIndex)
		{
			const FThreadLocalDataWorker& Worker = *Workers[WorkerIndex];
			ASSERT_THAT(IsNotNull(Worker.First, TEXT("Worker should create thread-local data")));
			ASSERT_THAT(AreEqual(Worker.First, Worker.Second,
				TEXT("Repeated worker-thread lookup should return the same local data")));
			ASSERT_THAT(AreNotEqual(MainThreadData, Worker.First,
				TEXT("Worker and main thread should not share thread-local data")));

			for (int32 OtherIndex = WorkerIndex + 1; OtherIndex < Workers.Num(); ++OtherIndex)
			{
				ASSERT_THAT(AreNotEqual(
					Worker.First,
					Workers[OtherIndex]->First,
					TEXT("Simultaneously live workers should own pairwise-distinct thread-local data")));
			}
		}

		Release->Trigger();
		for (FRunnableThread* const Thread : Threads)
		{
			Thread->WaitForCompletion();
			delete Thread;
		}
		for (FEvent* const Ready : ReadyEvents)
		{
			FPlatformProcess::ReturnSynchEventToPool(Ready);
		}
		FPlatformProcess::ReturnSynchEventToPool(Release);
	}

	TEST_METHOD(RepeatedLookupReturnsStableLocalData)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-TLS-MAIN-STABILITY-AFTER-WORKER",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asCThreadLocalData* const BeforeWorker = asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(BeforeWorker, TEXT("Main thread should have local data before worker teardown")));

		FEvent* const Ready = FPlatformProcess::GetSynchEventFromPool(true);
		FEvent* const Release = FPlatformProcess::GetSynchEventFromPool(true);
		if (Ready == nullptr || Release == nullptr)
		{
			TestRunner->AddError(TEXT("Thread-local-data lifecycle test should allocate synchronization events"));
			if (Ready != nullptr)
			{
				FPlatformProcess::ReturnSynchEventToPool(Ready);
			}
			if (Release != nullptr)
			{
				FPlatformProcess::ReturnSynchEventToPool(Release);
			}
			return;
		}

		FThreadLocalDataWorker Worker(*Ready, *Release);
		FRunnableThread* const Thread = FRunnableThread::Create(&Worker, TEXT("AngelscriptThreadLocalDataLifecycle"));
		if (Thread == nullptr)
		{
			TestRunner->AddError(TEXT("Thread-local-data lifecycle worker should be created"));
			FPlatformProcess::ReturnSynchEventToPool(Release);
			FPlatformProcess::ReturnSynchEventToPool(Ready);
			return;
		}

		Ready->Wait();
		Release->Trigger();
		Thread->WaitForCompletion();
		delete Thread;

		asCThreadLocalData* const AfterWorker = asCThreadManager::GetLocalData();
		ASSERT_THAT(AreEqual(BeforeWorker, AfterWorker,
			TEXT("Worker teardown should not replace the main thread's local data")));
		FPlatformProcess::ReturnSynchEventToPool(Release);
		FPlatformProcess::ReturnSynchEventToPool(Ready);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
