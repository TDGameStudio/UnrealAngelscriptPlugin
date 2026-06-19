#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "StaticJIT/StaticJITHeader.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_generic.h"
#include "source/as_texts.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITNativeBridgeTests_Private
{
	constexpr asDWORD ExpectedGenericReturnValue = 0x1234ABCD;

	struct FGenericProbeState
	{
		int32 CallCount = 0;
		asIScriptFunction* ObservedActiveFunction = nullptr;
		int32 SetReturnResult = asERROR;
	};

	thread_local FGenericProbeState* GActiveGenericProbeState = nullptr;

	void GenericActiveFunctionProbe(asIScriptGeneric* Generic)
	{
		check(GActiveGenericProbeState != nullptr);
		++GActiveGenericProbeState->CallCount;
		GActiveGenericProbeState->ObservedActiveFunction = asGetActiveFunction();
		GActiveGenericProbeState->SetReturnResult = Generic->SetReturnDWord(ExpectedGenericReturnValue);
	}

	struct FScopedFakeSystemFunction
	{
		explicit FScopedFakeSystemFunction(asCScriptEngine& ScriptEngine, const ANSICHAR* FunctionName)
		{
			Function = new asCScriptFunction(&ScriptEngine, nullptr, asFUNC_SYSTEM);
			Function->name = FunctionName;
			Function->returnType = asCDataType::CreatePrimitive(ttInt, false);
		}

		~FScopedFakeSystemFunction()
		{
			delete Function;
			Function = nullptr;
		}

		void ConfigureGeneric(void (*Callback)(asIScriptGeneric*), internalCallConv CallConv)
		{
			check(Function != nullptr);
			check(Function->sysFuncIntf == nullptr);

			Function->sysFuncIntf = new asSSystemFunctionInterface();
			Function->sysFuncIntf->func = reinterpret_cast<asFUNCTION_t>(Callback);
			Function->sysFuncIntf->callConv = CallConv;
			Function->sysFuncIntf->baseOffset = 0;
		}

		asCScriptFunction* Get() const
		{
			return Function;
		}

	private:
		asCScriptFunction* Function = nullptr;
	};

	bool VerifyThreadLocalStateAvailable(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asCThreadLocalData*& OutThreadLocalData)
	{
		OutThreadLocalData = FAngelscriptEngine::GameThreadTLD;
		return Test.TestTrue(
				TEXT("StaticJIT native-bridge tests should run with the current engine installed"),
				FAngelscriptEngine::TryGetCurrentEngine() == &Engine)
			&& Test.TestNotNull(
				TEXT("StaticJIT native-bridge tests should expose a valid game-thread local data pointer"),
				OutThreadLocalData);
	}

	bool VerifyGenericFunctionBridge(
		FAutomationTestBase& Test,
		asCScriptEngine& ScriptEngine,
		asCThreadLocalData& ThreadLocalData)
	{
		bool bPassed = false;
		FScriptExecution* PreviousExecution = ThreadLocalData.activeExecution;
		asCContext* PreviousContext = ThreadLocalData.activeContext;
		asIScriptFunction* PreviousActiveFunction = ThreadLocalData.activeFunction;

		FScopedFakeSystemFunction SentinelFunction(ScriptEngine, "SentinelActiveFunction");
		FScopedFakeSystemFunction GenericFunction(ScriptEngine, "GenericBridgeProbe");
		GenericFunction.ConfigureGeneric(&GenericActiveFunctionProbe, ICC_GENERIC_FUNC);

		FGenericProbeState ProbeState;
		TGuardValue<FGenericProbeState*> ProbeGuard(GActiveGenericProbeState, &ProbeState);

		asDWORD StackArguments[1] = {};
		asQWORD ValueRegister = 0;
		void* ObjectRegister = reinterpret_cast<void*>(UPTRINT(0xDEADBEEF));

		{
			FScriptExecution Execution(&ThreadLocalData);
			ThreadLocalData.activeFunction = SentinelFunction.Get();

			if (!Test.TestTrue(
					TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should register the temporary execution on the thread-local data"),
					ThreadLocalData.activeExecution == &Execution)
				|| !Test.TestFalse(
					TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should start with bExceptionThrown cleared"),
					Execution.bExceptionThrown))
			{
				ThreadLocalData.activeFunction = PreviousActiveFunction;
				return false;
			}

			FStaticJITFunction::ScriptCallNative(
				Execution,
				GenericFunction.Get(),
				reinterpret_cast<asBYTE*>(StackArguments),
				&ValueRegister,
				&ObjectRegister);

			const bool bCallbackInvoked = Test.TestEqual(
				TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should invoke the generic probe exactly once"),
				ProbeState.CallCount,
				1);
			const bool bObservedFunctionMatches = Test.TestTrue(
				TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should expose the called descriptor through asGetActiveFunction()"),
				ProbeState.ObservedActiveFunction == GenericFunction.Get());
			const bool bReturnWriteSucceeded = Test.TestEqual(
				TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should let the generic probe publish a dword return value"),
				ProbeState.SetReturnResult,
				static_cast<int32>(asSUCCESS));
			const bool bExecutionStayedClean = Test.TestFalse(
				TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should not mark the execution as throwing"),
				Execution.bExceptionThrown);
			const bool bReturnValueMatches = Test.TestEqual(
				TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should copy the probe return value into valueRegister"),
				static_cast<asDWORD>(ValueRegister),
				ExpectedGenericReturnValue);
			const bool bObjectRegisterCleared = Test.TestNull(
				TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should leave the generic object register null when the probe does not publish one"),
				ObjectRegister);
			const bool bRestoredSentinel = Test.TestTrue(
				TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should restore the previous activeFunction after the generic bridge returns"),
				ThreadLocalData.activeFunction == SentinelFunction.Get());

			ThreadLocalData.activeFunction = PreviousActiveFunction;
			bPassed = bCallbackInvoked
				&& bObservedFunctionMatches
				&& bReturnWriteSucceeded
				&& bExecutionStayedClean
				&& bReturnValueMatches
				&& bObjectRegisterCleared
				&& bRestoredSentinel;
		}

		const bool bRestoredExecution = Test.TestTrue(
			TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should restore the previous active execution after the temporary scope exits"),
			ThreadLocalData.activeExecution == PreviousExecution);
		const bool bRestoredContext = Test.TestTrue(
			TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should restore the previous active context after the temporary scope exits"),
			ThreadLocalData.activeContext == PreviousContext);
		const bool bRestoredOriginalFunction = Test.TestTrue(
			TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should restore the original activeFunction after the test scope exits"),
			ThreadLocalData.activeFunction == PreviousActiveFunction);
		return bPassed && bRestoredExecution && bRestoredContext && bRestoredOriginalFunction;
	}

	bool VerifyGenericMethodNullThisGuard(
		FAutomationTestBase& Test,
		asCScriptEngine& ScriptEngine,
		asCThreadLocalData& ThreadLocalData)
	{
		bool bPassed = false;
		FScriptExecution* PreviousExecution = ThreadLocalData.activeExecution;
		asCContext* PreviousContext = ThreadLocalData.activeContext;
		asIScriptFunction* PreviousActiveFunction = ThreadLocalData.activeFunction;

		FScopedFakeSystemFunction SentinelFunction(ScriptEngine, "SentinelActiveFunction");
		FScopedFakeSystemFunction GenericMethodFunction(ScriptEngine, "GenericMethodNullThis");
		GenericMethodFunction.ConfigureGeneric(&GenericActiveFunctionProbe, ICC_GENERIC_METHOD);

		FGenericProbeState ProbeState;
		TGuardValue<FGenericProbeState*> ProbeGuard(GActiveGenericProbeState, &ProbeState);

		asDWORD StackArguments[AS_PTR_SIZE] = {};
		asQWORD ValueRegister = 0;
		void* ObjectRegister = reinterpret_cast<void*>(UPTRINT(0xFACEB00C));

		{
			FScriptExecution Execution(&ThreadLocalData);
			ThreadLocalData.activeFunction = SentinelFunction.Get();

			if (!Test.TestTrue(
					TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should register the temporary execution on the thread-local data"),
					ThreadLocalData.activeExecution == &Execution)
				|| !Test.TestFalse(
					TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should start with bExceptionThrown cleared"),
					Execution.bExceptionThrown))
			{
				ThreadLocalData.activeFunction = PreviousActiveFunction;
				return false;
			}

			Test.AddExpectedErrorPlain(ANSI_TO_TCHAR(TXT_NULL_POINTER_ACCESS), EAutomationExpectedErrorFlags::Contains, 1);
			FStaticJITFunction::ScriptCallNative(
				Execution,
				GenericMethodFunction.Get(),
				reinterpret_cast<asBYTE*>(StackArguments),
				&ValueRegister,
				&ObjectRegister);

			const bool bCallbackSkipped = Test.TestEqual(
				TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should skip the generic probe when the method this pointer is null"),
				ProbeState.CallCount,
				0);
			const bool bNoObservedFunction = Test.TestNull(
				TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should not expose a callback-side active function when the probe never runs"),
				ProbeState.ObservedActiveFunction);
			const bool bExceptionThrown = Test.TestTrue(
				TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should mark the execution as throwing"),
				Execution.bExceptionThrown);
			const bool bValueUnchanged = Test.TestEqual(
				TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should leave valueRegister untouched when the guard bails out before the callback"),
				ValueRegister,
				static_cast<asQWORD>(0));
			const bool bObjectRegisterUnchanged = Test.TestTrue(
				TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should leave objectRegister untouched when the guard bails out before the callback"),
				ObjectRegister == reinterpret_cast<void*>(UPTRINT(0xFACEB00C)));
			const bool bRestoredSentinel = Test.TestTrue(
				TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should keep the previous activeFunction when the null-this guard triggers"),
				ThreadLocalData.activeFunction == SentinelFunction.Get());

			ThreadLocalData.activeFunction = PreviousActiveFunction;
			bPassed = bCallbackSkipped
				&& bNoObservedFunction
				&& bExceptionThrown
				&& bValueUnchanged
				&& bObjectRegisterUnchanged
				&& bRestoredSentinel;
		}

		const bool bRestoredExecution = Test.TestTrue(
			TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should restore the previous active execution after the temporary scope exits"),
			ThreadLocalData.activeExecution == PreviousExecution);
		const bool bRestoredContext = Test.TestTrue(
			TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should restore the previous active context after the temporary scope exits"),
			ThreadLocalData.activeContext == PreviousContext);
		const bool bRestoredOriginalFunction = Test.TestTrue(
			TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should restore the original activeFunction after the test scope exits"),
			ThreadLocalData.activeFunction == PreviousActiveFunction);
		return bPassed && bRestoredExecution && bRestoredContext && bRestoredOriginalFunction;
	}
}


namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITNativeBridgeTests_Private
{

bool RunNativeBridgeGenericCallRestoresState(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITNativeBridgeTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	do
	{
		asCThreadLocalData* ThreadLocalData = nullptr;
		if (!VerifyThreadLocalStateAvailable(Test, Engine, ThreadLocalData))
		{
			break;
		}

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		if (!Test.TestNotNull(
				TEXT("StaticJIT.NativeBridge.GenericCallRestoresState should expose the underlying AngelScript engine"),
				ScriptEngine))
		{
			break;
		}

		if (!VerifyGenericFunctionBridge(Test, *ScriptEngine, *ThreadLocalData))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

bool RunNativeBridgeGenericMethodNullThisThrows(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITNativeBridgeTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	do
	{
		asCThreadLocalData* ThreadLocalData = nullptr;
		if (!VerifyThreadLocalStateAvailable(Test, Engine, ThreadLocalData))
		{
			break;
		}

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		if (!Test.TestNotNull(
				TEXT("StaticJIT.NativeBridge.GenericMethodNullThisThrows should expose the underlying AngelScript engine"),
				ScriptEngine))
		{
			break;
		}

		if (!VerifyGenericMethodNullThisGuard(Test, *ScriptEngine, *ThreadLocalData))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITNativeBridgeTests,
	"Angelscript.TestModule.StaticJIT.NativeBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GenericCallRestoresState)
	{
		using namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITNativeBridgeTests_Private;
		ASSERT_THAT(IsTrue(RunNativeBridgeGenericCallRestoresState(*TestRunner)));
	}

	TEST_METHOD(GenericMethodNullThisThrows)
	{
		using namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITNativeBridgeTests_Private;
		ASSERT_THAT(IsTrue(RunNativeBridgeGenericMethodNullThisThrows(*TestRunner)));
	}
};

#endif
