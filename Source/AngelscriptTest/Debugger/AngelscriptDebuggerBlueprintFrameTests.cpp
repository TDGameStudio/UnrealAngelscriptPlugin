#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"
#include "AngelscriptDebuggerTestMonitor.h"
#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptDebuggerTestHelpers.h"
#include "AngelscriptTestEngineHelper.h"

#include "Async/Async.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptDebuggerBlueprintFrameTests_Private
{
	static const FName BlueprintInvocationFunctionName(TEXT("CallIntoScript"));
	static const FName ScriptBreakpointFunctionName(TEXT("BreakInScript"));
	static const FName ScriptValuePropertyName(TEXT("ScriptValue"));
	static const FName LastBreakResultPropertyName(TEXT("LastBreakResult"));

	bool WaitForVoidInvocationCompletion(
		FAutomationTestBase& Test,
		FAngelscriptDebuggerTestSession& Session,
		const TSharedRef<FAsyncGeneratedVoidInvocationState>& InvocationState,
		const TCHAR* Context)
	{
		return Test.TestTrue(
			Context,
			Session.PumpUntil(
				[&InvocationState]()
				{
					return InvocationState->bCompleted.Load();
				},
				Session.GetDefaultTimeoutSeconds()));
	}

	template <typename T>
	bool WaitForTypedMessage(
		FAngelscriptDebuggerTestClient& Client,
		EDebugMessageType ExpectedType,
		float TimeoutSeconds,
		TOptional<T>& OutValue,
		FString& OutError,
		const TCHAR* Context)
	{
		OutValue = Client.WaitForTypedMessage<T>(ExpectedType, TimeoutSeconds);
		if (!OutValue.IsSet())
		{
			OutError = FString::Printf(TEXT("%s: %s"), Context, *Client.GetLastError());
			return false;
		}

		return true;
	}

	void CleanupBlueprint(UBlueprint*& Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	UBlueprint* CreateTransientBlueprintChild(
		FAutomationTestBase& Test,
		UClass* ParentClass,
		FStringView Suffix,
		const TCHAR* CallingContext = TEXT("AngelscriptDebuggerBlueprintFrameTests"))
	{
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should resolve the script parent class before creating a transient child blueprint"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptDebuggerBlueprintFrame_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should create a transient blueprint package"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			CallingContext);
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should create a transient child blueprint"), Blueprint))
		{
			return nullptr;
		}

		return Blueprint;
	}

	UK2Node_CallFunction* AddCallFunctionNode(
		UEdGraph& FunctionGraph,
		UFunction& ScriptFunction,
		UEdGraphPin* ConnectPin)
	{
		UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(&FunctionGraph);
		if (CallFunctionNode == nullptr)
		{
			return nullptr;
		}

		FunctionGraph.AddNode(CallFunctionNode, false, false);
		CallFunctionNode->CreateNewGuid();
		CallFunctionNode->PostPlacedNewNode();
		CallFunctionNode->SetFromFunction(&ScriptFunction);
		CallFunctionNode->AllocateDefaultPins();
		CallFunctionNode->NodePosX = 320;
		CallFunctionNode->NodePosY = 0;

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (Schema != nullptr && ConnectPin != nullptr && CallFunctionNode->GetExecPin() != nullptr)
		{
			Schema->TryCreateConnection(ConnectPin, CallFunctionNode->GetExecPin());
		}

		return CallFunctionNode;
	}

	bool CreateTransientBlueprintCallingScriptFunction(
		FAutomationTestBase& Test,
		UClass& ParentClass,
		UBlueprint& Blueprint,
		FName FunctionName,
		FName ScriptFunctionName)
	{
		UFunction* ScriptFunction = ParentClass.FindFunctionByName(ScriptFunctionName);
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should expose the script function that the transient blueprint calls"), ScriptFunction))
		{
			return false;
		}

		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(&Blueprint, FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should create a Blueprint function graph"), FunctionGraph))
		{
			return false;
		}

		FBlueprintEditorUtils::AddFunctionGraph<UClass>(&Blueprint, FunctionGraph, true, nullptr);

		TArray<UK2Node_FunctionEntry*> EntryNodes;
		FunctionGraph->GetNodesOfClass(EntryNodes);
		UK2Node_FunctionEntry* EntryNode = EntryNodes.Num() > 0 ? EntryNodes[0] : nullptr;
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should create a function entry node for the Blueprint caller graph"), EntryNode))
		{
			return false;
		}

		UEdGraphPin* EntryThenPin = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should expose the function entry exec pin"), EntryThenPin))
		{
			return false;
		}

		UK2Node_CallFunction* CallNode = AddCallFunctionNode(*FunctionGraph, *ScriptFunction, EntryThenPin);
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should place a call-function node in the transient Blueprint graph"), CallNode))
		{
			return false;
		}

		UEdGraphPin* InputPin = CallNode->FindPin(TEXT("Input"));
		if (!Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should expose the Input pin on the Blueprint call node"), InputPin))
		{
			return false;
		}

		InputPin->DefaultValue = TEXT("7");
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		return Test.TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should compile the transient Blueprint caller into a generated class"), Blueprint.GeneratedClass.Get());
	}

	int32 FindCallstackFrameIndexByPrefix(const FAngelscriptCallStack& Callstack, const FString& Prefix)
	{
		for (int32 FrameIndex = 0; FrameIndex < Callstack.Frames.Num(); ++FrameIndex)
		{
			if (Callstack.Frames[FrameIndex].Name.StartsWith(Prefix))
			{
				return FrameIndex;
			}
		}

		return INDEX_NONE;
	}

	bool ReadIntProperty(
		FAutomationTestBase& Test,
		const UObject& Object,
		FName PropertyName,
		int32& OutValue,
		const TCHAR* Context)
	{
		const FIntProperty* Property = FindFProperty<FIntProperty>(Object.GetClass(), PropertyName);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should expose int property '%s'"), Context, *PropertyName.ToString()), Property))
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(&Object);
		return true;
	}

	struct FBlueprintFrameMonitorResult
	{
		TOptional<FStoppedMessage> StopMessage;
		TOptional<FAngelscriptCallStack> Callstack;
		TOptional<FAngelscriptVariable> BlueprintScriptValue;
		TOptional<FAngelscriptVariables> BlueprintThisScope;
		int32 BlueprintFrameIndex = INDEX_NONE;
		int32 ContinuedCount = 0;
		bool bTimedOut = false;
		FString Error;
	};

	TFuture<FBlueprintFrameMonitorResult> StartBlueprintFrameMonitor(
		int32 Port,
		TAtomic<bool>& bMonitorReady,
		TAtomic<bool>& bShouldStop,
		float TimeoutSeconds)
	{
		return Async(EAsyncExecution::ThreadPool,
			[Port, &bMonitorReady, &bShouldStop, TimeoutSeconds]() -> FBlueprintFrameMonitorResult
			{
				FBlueprintFrameMonitorResult Result;
				FAngelscriptDebuggerTestClient MonitorClient;
				ON_SCOPE_EXIT
				{
					MonitorClient.SendStopDebugging();
					MonitorClient.SendDisconnect();
					MonitorClient.Disconnect();
				};

				if (!MonitorClient.Connect(TEXT("127.0.0.1"), Port))
				{
					Result.Error = FString::Printf(TEXT("Blueprint-frame monitor failed to connect: %s"), *MonitorClient.GetLastError());
					bMonitorReady = true;
					return Result;
				}

				if (!HandshakeMonitorClient(MonitorClient, bShouldStop, TimeoutSeconds, Result.Error))
				{
					Result.bTimedOut = !bShouldStop.Load();
					bMonitorReady = true;
					return Result;
				}

				bMonitorReady = true;

				Result.StopMessage = MonitorClient.WaitForTypedMessage<FStoppedMessage>(EDebugMessageType::HasStopped, TimeoutSeconds);
				if (!Result.StopMessage.IsSet())
				{
					Result.Error = FString::Printf(TEXT("Blueprint-frame monitor timed out waiting for HasStopped: %s"), *MonitorClient.GetLastError());
					Result.bTimedOut = true;
					return Result;
				}

				if (!MonitorClient.SendRequestCallStack() ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::CallStack, TimeoutSeconds, Result.Callstack, Result.Error, TEXT("Blueprint-frame monitor failed to receive CallStack")))
				{
					return Result;
				}

				Result.BlueprintFrameIndex = FindCallstackFrameIndexByPrefix(Result.Callstack.GetValue(), TEXT("(BP)"));
				if (Result.BlueprintFrameIndex == INDEX_NONE)
				{
					Result.Error = TEXT("Blueprint-frame monitor failed to locate a Blueprint frame inside the returned callstack.");
					return Result;
				}

				if (!MonitorClient.SendRequestEvaluate(TEXT("ScriptValue"), Result.BlueprintFrameIndex) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Evaluate, TimeoutSeconds, Result.BlueprintScriptValue, Result.Error, TEXT("Blueprint-frame monitor failed to receive Blueprint-frame ScriptValue evaluate reply")))
				{
					return Result;
				}

				const FString ThisScopePath = FString::Printf(TEXT("%d:%s"), Result.BlueprintFrameIndex, TEXT("%this%"));
				if (!MonitorClient.SendRequestVariables(ThisScopePath) ||
					!WaitForTypedMessage(MonitorClient, EDebugMessageType::Variables, TimeoutSeconds, Result.BlueprintThisScope, Result.Error, TEXT("Blueprint-frame monitor failed to receive Blueprint-frame %this% scope variables")))
				{
					return Result;
				}

				if (!MonitorClient.SendContinue())
				{
					Result.Error = FString::Printf(TEXT("Blueprint-frame monitor failed to send Continue: %s"), *MonitorClient.GetLastError());
					return Result;
				}

				if (!MonitorClient.WaitForMessageType(EDebugMessageType::HasContinued, TimeoutSeconds).IsSet())
				{
					Result.Error = FString::Printf(TEXT("Blueprint-frame monitor timed out waiting for HasContinued: %s"), *MonitorClient.GetLastError());
					Result.bTimedOut = true;
					return Result;
				}

				Result.ContinuedCount = 1;
				return Result;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerBlueprintFrameTests,
	"Angelscript.TestModule.Debugger.BlueprintFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	FDebuggerTestContext Ctx;

	BEFORE_EACH()
	{
		ASSERT_THAT(IsTrue(Ctx.SetUp(*TestRunner)));
	}

	AFTER_EACH()
	{
		Ctx.TearDown();
	}

	TEST_METHOD(BlueprintMixedCallstackAndThisScope)
	{
		using namespace AngelscriptDebuggerBlueprintFrameTests_Private;

		const FAngelscriptDebuggerScriptFixture Fixture = FAngelscriptDebuggerScriptFixture::CreateBlueprintFrameFixture();
		FAngelscriptEngine& Engine = Ctx.GetEngine();
		UBlueprint* Blueprint = nullptr;
		UObject* BlueprintObject = nullptr;

		ON_SCOPE_EXIT
		{
			FAngelscriptClearBreakpoints ClearBreakpoints;
			ClearBreakpoints.Filename = Fixture.Filename;
			ClearBreakpoints.ModuleName = Fixture.ModuleName.ToString();
			Ctx.Client.SendClearBreakpoints(ClearBreakpoints);

			if (BlueprintObject != nullptr)
			{
				BlueprintObject->MarkAsGarbage();
				BlueprintObject = nullptr;
			}

			CleanupBlueprint(Blueprint);
			Engine.DiscardModule(*Fixture.ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should compile the Blueprint-frame script fixture"), Fixture.Compile(Engine))));

		UClass* ScriptParentClass = Fixture.FindGeneratedClass(Engine);
		ASSERT_THAT(IsTrue(TestRunner->TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should publish the script parent class"), ScriptParentClass)));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, ScriptParentClass, TEXT("MixedCallstack"));
		ASSERT_THAT(IsTrue(Blueprint != nullptr));

		ASSERT_THAT(IsTrue(CreateTransientBlueprintCallingScriptFunction(*TestRunner, *ScriptParentClass, *Blueprint, BlueprintInvocationFunctionName, ScriptBreakpointFunctionName)));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsTrue(TestRunner->TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should expose the transient Blueprint generated class"), BlueprintClass)));

		BlueprintObject = NewObject<UObject>(GetTransientPackage(), BlueprintClass);
		ASSERT_THAT(IsTrue(TestRunner->TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should instantiate the transient Blueprint caller"), BlueprintObject)));

		UFunction* BlueprintFunction = BlueprintObject->FindFunction(BlueprintInvocationFunctionName);
		ASSERT_THAT(IsTrue(TestRunner->TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should expose the Blueprint caller function on the transient object"), BlueprintFunction)));

		TAtomic<bool> bMonitorReady(false);
		TAtomic<bool> bShouldStop(false);
		TFuture<FBlueprintFrameMonitorResult> MonitorFuture =
			StartBlueprintFrameMonitor(Ctx.GetPort(), bMonitorReady, bShouldStop, Ctx.GetDefaultTimeoutSeconds());
		ON_SCOPE_EXIT
		{
			bShouldStop = true;
			if (MonitorFuture.IsValid())
			{
				MonitorFuture.Wait();
			}
		};

		ASSERT_THAT(IsTrue(WaitForMonitorReady(*TestRunner, Ctx.Session, bMonitorReady, TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should bring the monitor client up before execution"))));

		FAngelscriptBreakpoint Breakpoint;
		Breakpoint.Id = 46;
		Breakpoint.Filename = Fixture.Filename;
		Breakpoint.ModuleName = Fixture.ModuleName.ToString();
		Breakpoint.LineNumber = Fixture.GetLine(TEXT("BlueprintScriptBreakLine"));

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should send the script breakpoint"), Ctx.Client.SendSetBreakpoint(Breakpoint))));

		ASSERT_THAT(IsTrue(WaitForBreakpointCount(*TestRunner, Ctx.Session, 1, TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should observe the breakpoint registration after both debugger clients finish handshaking"))));

		const TSharedRef<FAsyncGeneratedVoidInvocationState> InvocationState =
			DispatchGeneratedVoidInvocation(Engine, BlueprintObject, BlueprintFunction);
		ASSERT_THAT(IsTrue(WaitForVoidInvocationCompletion(*TestRunner, Ctx.Session, InvocationState, TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should finish the Blueprint invocation after the monitor resumes execution"))));

		bShouldStop = true;
		const FBlueprintFrameMonitorResult MonitorResult = MonitorFuture.Get();

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should finish without monitor errors"), MonitorResult.Error.IsEmpty())));
		if (!MonitorResult.Error.IsEmpty())
		{
			TestRunner->AddError(MonitorResult.Error);
		}

		ASSERT_THAT(IsTrue(TestRunner->TestFalse(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should not time out while collecting debugger payloads"), MonitorResult.bTimedOut)));
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should finish the Blueprint caller invocation successfully"), InvocationState->bSucceeded)));
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should deserialize the stop payload"), MonitorResult.StopMessage.IsSet())));
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should capture a mixed callstack"), MonitorResult.Callstack.IsSet())));
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should capture the Blueprint-frame ScriptValue evaluation"), MonitorResult.BlueprintScriptValue.IsSet())));
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should capture the Blueprint-frame %this% scope"), MonitorResult.BlueprintThisScope.IsSet())));

		const FAngelscriptCallStack& Callstack = MonitorResult.Callstack.GetValue();
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should return at least two frames"), Callstack.Frames.Num() >= 2)));
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should locate a Blueprint frame inside the mixed callstack"), Callstack.Frames.IsValidIndex(MonitorResult.BlueprintFrameIndex))));

		const FAngelscriptCallFrame& ScriptFrame = Callstack.Frames[0];
		const FAngelscriptCallFrame& BlueprintFrame = Callstack.Frames[MonitorResult.BlueprintFrameIndex];

		TestRunner->TestEqual(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should stop because of a breakpoint"), MonitorResult.StopMessage->Reason, FString(TEXT("breakpoint")));
		TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should keep the script frame on top of the mixed callstack"), ScriptFrame.Source.EndsWith(Fixture.Filename));
		TestRunner->TestEqual(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should report the script breakpoint line on the top frame"), ScriptFrame.LineNumber, Fixture.GetLine(TEXT("BlueprintScriptBreakLine")));
		TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should tag the Blueprint caller frame with the (BP) prefix"), BlueprintFrame.Name.StartsWith(TEXT("(BP)")));
		TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should report the Blueprint caller source as a transient outer/class path"), BlueprintFrame.Source.StartsWith(TEXT("::")));
		TestRunner->TestEqual(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should evaluate Blueprint-frame ScriptValue to 5"), MonitorResult.BlueprintScriptValue->Value, FString(TEXT("5")));
		TestRunner->TestEqual(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should observe a single HasContinued after resuming"), MonitorResult.ContinuedCount, 1);

		const TArray<FAngelscriptVariable>& BlueprintThisVariables = MonitorResult.BlueprintThisScope->Variables;
		const FAngelscriptVariable* ScriptValueVariable = BlueprintThisVariables.FindByPredicate(
			[](const FAngelscriptVariable& Variable)
			{
				return Variable.Name == TEXT("ScriptValue");
			});
		ASSERT_THAT(IsTrue(TestRunner->TestNotNull(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should expose ScriptValue inside the Blueprint-frame %this% scope"), ScriptValueVariable)));

		TestRunner->TestEqual(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should report ScriptValue = 5 inside the Blueprint-frame %this% scope"), ScriptValueVariable->Value, FString(TEXT("5")));
		TestRunner->TestTrue(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should expose a non-zero ValueAddress for ScriptValue inside the Blueprint-frame %this% scope"), ScriptValueVariable->ValueAddress != 0);

		int32 LastBreakResult = INDEX_NONE;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, *BlueprintObject, LastBreakResultPropertyName, LastBreakResult, TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope"))));

		TestRunner->TestEqual(TEXT("Debugger.BlueprintFrame.BlueprintMixedCallstackAndThisScope should preserve the expected script-side result after the Blueprint caller resumes"), LastBreakResult, 12);
	}
};

#endif
