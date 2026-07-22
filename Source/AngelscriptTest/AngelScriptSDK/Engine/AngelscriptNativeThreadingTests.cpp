#include "CQTest.h"

// Raw SDK thread-manager and thread-local-data coverage.

#include "HAL/Event.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FThreadingTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.Threading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
		asCThreadLocalData* const First = asCThreadManager::GetLocalData();
		asCThreadLocalData* const Second = asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(First, TEXT("Thread manager should create local data for the main thread")));
		ASSERT_THAT(AreEqual(First, Second, TEXT("Repeated main-thread lookup should return the same local data")));
	}

	TEST_METHOD(WorkerThreadsReceiveDistinctLocalData)
	{
		asCThreadLocalData* const MainThreadData = asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(MainThreadData, TEXT("Main thread should have local data before worker creation")));

		FEvent* const Ready = FPlatformProcess::GetSynchEventFromPool(true);
		FEvent* const Release = FPlatformProcess::GetSynchEventFromPool(true);
		ASSERT_THAT(IsNotNull(Ready, TEXT("Worker-ready event should be allocated")));
		ASSERT_THAT(IsNotNull(Release, TEXT("Worker-release event should be allocated")));
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
		FRunnableThread* const Thread = FRunnableThread::Create(&Worker, TEXT("AngelscriptThreadLocalData"));
		ASSERT_THAT(IsNotNull(Thread, TEXT("Thread-local-data worker should be created")));
		if (Thread == nullptr)
		{
			FPlatformProcess::ReturnSynchEventToPool(Release);
			FPlatformProcess::ReturnSynchEventToPool(Ready);
			return;
		}

		Ready->Wait();
		ASSERT_THAT(IsNotNull(Worker.First, TEXT("Worker should create thread-local data")));
		ASSERT_THAT(AreEqual(Worker.First, Worker.Second,
			TEXT("Repeated worker-thread lookup should return the same local data")));
		ASSERT_THAT(AreNotEqual(MainThreadData, Worker.First,
			TEXT("Worker and main thread should not share thread-local data")));

		Release->Trigger();
		Thread->WaitForCompletion();
		delete Thread;
		FPlatformProcess::ReturnSynchEventToPool(Release);
		FPlatformProcess::ReturnSynchEventToPool(Ready);
	}

	TEST_METHOD(RepeatedLookupReturnsStableLocalData)
	{
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
