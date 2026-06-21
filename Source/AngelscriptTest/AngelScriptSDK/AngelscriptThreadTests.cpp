// AngelscriptSDKThreadTests.cpp
// Tests for as_thread.cpp - thread-local storage via asCThreadManager.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.Thread.*

#include "CQTest.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_thread.h"
#include "EndAngelscriptHeaders.h"

// TODO: asCThreadManager symbols not exported from AngelscriptRuntime. Disabled until linkage resolved.
#if 0 // WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKThreadTests, "Angelscript.TestModule.AngelScriptSDK.Thread", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FTLSCaptureRunnable : public FRunnable
	{
	public:
		asCThreadLocalData* CapturedTLS = nullptr;
		FEvent* CompletionEvent = nullptr;

		explicit FTLSCaptureRunnable(FEvent* InEvent) : CompletionEvent(InEvent) {}

		virtual uint32 Run() override
		{
			CapturedTLS = asCThreadManager::GetLocalData();
			if (CompletionEvent) CompletionEvent->Trigger();
			return 0;
		}
	};

public:
	TEST_METHOD(GetLocalDataNonNull)
	{
		asCThreadLocalData* TLS = asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(TLS, TEXT("GetLocalData on main thread should return non-null")));
	}

	TEST_METHOD(GetLocalDataStable)
	{
		asCThreadLocalData* TLS1 = asCThreadManager::GetLocalData();
		asCThreadLocalData* TLS2 = asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(TLS1, TEXT("First GetLocalData should be non-null")));
		ASSERT_THAT(IsNotNull(TLS2, TEXT("Second GetLocalData should be non-null")));
		ASSERT_THAT(AreEqual(TLS1, TLS2, TEXT("Two calls on same thread should return identical pointer")));
	}

	TEST_METHOD(DifferentTLS)
	{

		asCThreadLocalData* MainTLS = asCThreadManager::GetLocalData();
		ASSERT_THAT(IsNotNull(MainTLS, TEXT("Main thread TLS should be non-null")));

		FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool(true);
		FTLSCaptureRunnable Runnable(CompletionEvent);

		FRunnableThread* Thread = FRunnableThread::Create(&Runnable, TEXT("TLSCaptureThread"));
		if (!this->Assert.IsNotNull(Thread, TEXT("Should create worker thread")))
		{
			FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
			return;
		}

		CompletionEvent->Wait();
		Thread->WaitForCompletion();
		delete Thread;
		FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);

		ASSERT_THAT(IsNotNull(Runnable.CapturedTLS, TEXT("Worker thread TLS should be non-null")));
		ASSERT_THAT(AreNotEqual(MainTLS, Runnable.CapturedTLS, TEXT("Worker thread TLS should differ from main thread TLS")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
