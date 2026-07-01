#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "StaticJIT/StaticJITHeader.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_JIT_DEBUG_CALLSTACKS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITDebugCallstackTests,
	"Angelscript.TestModule.StaticJIT.DebugCallstack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedDebugFrame
	{
		const ANSICHAR* Filename = "";
		const ANSICHAR* FunctionName = "";
		int32 LineNumber = 0;
		void* ThisObject = nullptr;
		FScopeJITDebugCallstack* PrevFrame = nullptr;
	};

	static FString DescribeObservedFrame(const FScopeJITDebugCallstack* Frame)
	{
		if (Frame == nullptr)
		{
			return TEXT("<null>");
		}

		const FString Filename = ANSI_TO_TCHAR(Frame->Filename != nullptr ? Frame->Filename : "");
		const FString FunctionName = ANSI_TO_TCHAR(Frame->FunctionName != nullptr ? Frame->FunctionName : "");
		return FString::Printf(
			TEXT("Filename=%s Function=%s Line=%d This=%p Prev=%p"),
			*Filename,
			*FunctionName,
			Frame->LineNumber,
			Frame->ThisObject,
			Frame->PrevFrame);
	}

	static bool VerifyTopFrame(
		FAutomationTestBase& Test,
		const FScopeJITDebugCallstack* ActualFrame,
		const FExpectedDebugFrame& ExpectedFrame,
		const TCHAR* StageLabel)
	{
		const FString FrameExistsMessage = FString::Printf(TEXT("%s should leave a live debug-callstack frame"), StageLabel);
		const bool bFrameExists = Test.TestNotNull(
			*FrameExistsMessage,
			ActualFrame);
		if (!bFrameExists)
		{
			return false;
		}

		const FString FilenameMessage = FString::Printf(TEXT("%s should record the expected filename"), StageLabel);
		const bool bFilenameMatches = Test.TestTrue(
			*FilenameMessage,
			FCStringAnsi::Strcmp(ActualFrame->Filename, ExpectedFrame.Filename) == 0);
		const FString FunctionMessage = FString::Printf(TEXT("%s should record the expected function name"), StageLabel);
		const bool bFunctionMatches = Test.TestTrue(
			*FunctionMessage,
			FCStringAnsi::Strcmp(ActualFrame->FunctionName, ExpectedFrame.FunctionName) == 0);
		const FString LineMessage = FString::Printf(TEXT("%s should record the expected line number"), StageLabel);
		const bool bLineMatches = Test.TestEqual(
			*LineMessage,
			ActualFrame->LineNumber,
			ExpectedFrame.LineNumber);
		const FString ThisMessage = FString::Printf(TEXT("%s should keep the expected ThisObject pointer"), StageLabel);
		const bool bThisMatches = Test.TestTrue(
			*ThisMessage,
			ActualFrame->ThisObject == ExpectedFrame.ThisObject);
		const FString PrevFrameMessage = FString::Printf(TEXT("%s should keep the expected previous frame link"), StageLabel);
		const bool bPrevMatches = Test.TestTrue(
			*PrevFrameMessage,
			ActualFrame->PrevFrame == ExpectedFrame.PrevFrame);

		const bool bAllMatched = bFilenameMatches && bFunctionMatches && bLineMatches && bThisMatches && bPrevMatches;
		if (!bAllMatched)
		{
			Test.AddInfo(FString::Printf(TEXT("%s observed frame: %s"), StageLabel, *DescribeObservedFrame(ActualFrame)));
		}

		return bAllMatched;
	}

	static bool RunDebugCallstackScopePushPop(FAutomationTestBase& Test);

public:
	TEST_METHOD(ScopePushPop)
	{
		ASSERT_THAT(IsTrue(RunDebugCallstackScopePushPop(*TestRunner)));
	}
};

bool FAngelscriptStaticJITDebugCallstackTests::RunDebugCallstackScopePushPop(FAutomationTestBase& Test)
{

	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	do
	{
		if (!Test.TestTrue(
				TEXT("StaticJIT.DebugCallstack.ScopePushPop should run with a current Angelscript engine scope"),
				FAngelscriptEngine::TryGetCurrentEngine() == &Engine))
		{
			break;
		}

		asCThreadLocalData* ThreadLocalData = FAngelscriptEngine::GameThreadTLD;
		if (!Test.TestNotNull(
				TEXT("StaticJIT.DebugCallstack.ScopePushPop should expose a valid game-thread local data pointer"),
				ThreadLocalData))
		{
			break;
		}

		FScriptExecution* PreviousExecution = ThreadLocalData->activeExecution;
		asCContext* PreviousContext = ThreadLocalData->activeContext;

		int32 OuterSentinel = 11;
		int32 InnerSentinel = 27;
		void* OuterThis = &OuterSentinel;
		void* InnerThis = &InnerSentinel;

		{
			FScriptExecution Execution(ThreadLocalData);
			if (!Test.TestTrue(
					TEXT("StaticJIT.DebugCallstack.ScopePushPop should register the local execution on the thread-local data"),
					ThreadLocalData->activeExecution == &Execution))
			{
				break;
			}

			if (!Test.TestNull(
					TEXT("StaticJIT.DebugCallstack.ScopePushPop should start with an empty debug callstack"),
					Execution.debugCallStack))
			{
				break;
			}

			FScopeJITDebugCallstack* OuterFrame = nullptr;
			{
				FScopeJITDebugCallstack OuterScope(Execution, "Outer.as", "Outer", 11, OuterThis);
				OuterFrame = &OuterScope;

				const FExpectedDebugFrame ExpectedOuterFrame
				{
					"Outer.as",
					"Outer",
					11,
					OuterThis,
					nullptr
				};
				if (!VerifyTopFrame(
						Test,
						static_cast<FScopeJITDebugCallstack*>(Execution.debugCallStack),
						ExpectedOuterFrame,
						TEXT("StaticJIT.DebugCallstack.ScopePushPop outer scope")))
				{
					break;
				}

				{
					FScopeJITDebugCallstack InnerScope(Execution, "Inner.as", "Inner", 27, InnerThis);

					const FExpectedDebugFrame ExpectedInnerFrame
					{
						"Inner.as",
						"Inner",
						27,
						InnerThis,
						OuterFrame
					};
					if (!VerifyTopFrame(
							Test,
							static_cast<FScopeJITDebugCallstack*>(Execution.debugCallStack),
							ExpectedInnerFrame,
							TEXT("StaticJIT.DebugCallstack.ScopePushPop inner scope")))
					{
						break;
					}
				}

				if (!VerifyTopFrame(
						Test,
						static_cast<FScopeJITDebugCallstack*>(Execution.debugCallStack),
						ExpectedOuterFrame,
						TEXT("StaticJIT.DebugCallstack.ScopePushPop after inner scope")))
				{
					break;
				}
			}

			if (!Test.TestNull(
					TEXT("StaticJIT.DebugCallstack.ScopePushPop should restore a null debug callstack after the outer scope exits"),
					Execution.debugCallStack)
				|| !Test.TestFalse(
					TEXT("StaticJIT.DebugCallstack.ScopePushPop should not mark the execution as throwing an exception"),
					Execution.bExceptionThrown))
			{
				break;
			}
		}

		if (!Test.TestTrue(
				TEXT("StaticJIT.DebugCallstack.ScopePushPop should restore the previous active execution after FScriptExecution exits"),
				ThreadLocalData->activeExecution == PreviousExecution)
			|| !Test.TestTrue(
				TEXT("StaticJIT.DebugCallstack.ScopePushPop should restore the previous active context after FScriptExecution exits"),
				ThreadLocalData->activeContext == PreviousContext))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

#endif
