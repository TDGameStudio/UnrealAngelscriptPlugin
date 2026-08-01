#include "Testing/AngelscriptScriptTestRunner.h"

#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASFunction.h"
#include "Core/AngelscriptEngine.h"
#include "Testing/AngelscriptTestSuite.h"
#include "Testing/LatentAutomationCommand.h"
#include "Testing/AngelscriptScriptTestCommands.h"
#include "Testing/AngelscriptScriptTestWorld.h"

#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Logging/StructuredLog.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/StringBuilder.h"
#include "Testing/LatentAutomationCommandClientExecutor.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	constexpr ANSICHAR ControlledAssertionException[] =
		"__ANGELSCRIPT_TEST_CONTROLLED_ASSERTION__";

	TMap<const UAngelscriptTestSuite*,
		TWeakPtr<FAngelscriptScriptTestExecutionContext>> ContextsBySuite;
	TArray<TWeakPtr<FAngelscriptScriptTestExecutionContext>> ActiveContexts;
	TArray<FAngelscriptScriptTestExecutionContext*> ActiveCallbackContexts;
	int32 ScriptTestCallbackDepth = 0;
	int32 ScriptTestExceptionLoggingSuppressionDepth = 0;
	const UAngelscriptTestSuite* ActiveLifecycleSuite = nullptr;
	FAutomationTestBase* ActiveLifecycleResult = nullptr;
	EAngelscriptScriptTestPhase ActiveLifecyclePhase =
		EAngelscriptScriptTestPhase::None;

	int32 GetAutomationErrorCount(const FAutomationTestBase& Result)
	{
		FAutomationTestExecutionInfo Info;
		Result.GetExecutionInfo(Info);
		return Info.GetErrorTotal();
	}

	const TCHAR* DescribeLifecyclePhase(
		EAngelscriptScriptTestPhase Phase)
	{
		switch (Phase)
		{
		case EAngelscriptScriptTestPhase::BeforeAll:
			return TEXT("BeforeAll");
		case EAngelscriptScriptTestPhase::AfterAll:
			return TEXT("AfterAll");
		default:
			return TEXT("suite lifecycle");
		}
	}

	const TCHAR* DescribeClientCommandPhase(
		EAngelscriptScriptTestClientCommandPhase Phase)
	{
		switch (Phase)
		{
		case EAngelscriptScriptTestClientCommandPhase::CreateExecutor:
			return TEXT("CreateExecutor");
		case EAngelscriptScriptTestClientCommandPhase::Setup:
			return TEXT("Setup");
		case EAngelscriptScriptTestClientCommandPhase::SetupClient:
			return TEXT("SetupClient");
		case EAngelscriptScriptTestClientCommandPhase::Execute:
			return TEXT("Execute");
		case EAngelscriptScriptTestClientCommandPhase::Finish:
			return TEXT("Finish");
		case EAngelscriptScriptTestClientCommandPhase::FinishClient:
			return TEXT("FinishClient");
		case EAngelscriptScriptTestClientCommandPhase::Done:
			return TEXT("Done");
		default:
			return TEXT("<unknown>");
		}
	}

	FString FormatActiveScriptError(const FString& Message)
	{
		if (asIScriptContext* ActiveContext = asGetActiveContext())
		{
			if (ActiveContext->GetCallstackSize() > 0)
			{
				const char* ActiveFile = nullptr;
				const int32 ActiveLine =
					ActiveContext->GetLineNumber(
						0,
						nullptr,
						&ActiveFile);
				if (ActiveFile != nullptr
					&& ActiveFile[0] != '\0'
					&& ActiveLine > 0)
				{
					return FString::Printf(
						TEXT("%s:%d: %s"),
						ANSI_TO_TCHAR(ActiveFile),
						ActiveLine,
						*Message);
				}
			}
		}
		return Message;
	}

	class FScopedDetachedAutomationLogCapture final
		: public FFeedbackContext
	{
	public:
		explicit FScopedDetachedAutomationLogCapture(
			FAutomationTestBase* InResult)
			: Result(InResult)
		{
			if (Result != nullptr
				&& Result
					!= FAutomationTestFramework::Get().GetCurrentTest())
			{
				if (GWarn != nullptr)
				{
					// Error, Warning, and Display logs are routed through
					// GWarn rather than GLog. Interpose a forwarding feedback
					// context so a detached result receives the same raw event
					// that UE's current Automation test receives.
					PreviousWarn = GWarn;
					GWarn = this;
					bInterceptsWarn = true;
					bActive = true;
				}
				else if (GLog != nullptr)
				{
					GLog->AddOutputDevice(this);
					bInterceptsLog = true;
					bActive = true;
				}
			}
		}

		~FScopedDetachedAutomationLogCapture() override
		{
			if (bInterceptsWarn)
			{
				if (GWarn == this)
				{
					GWarn = PreviousWarn;
				}
				else
				{
					ensureMsgf(
						false,
						TEXT("GWarn changed while a reflected script-test "
							"log capture was active."));
				}
			}
			if (bInterceptsLog && GLog != nullptr)
			{
				GLog->RemoveOutputDevice(this);
			}
			if (bCommit)
			{
				for (const FAutomationEvent& Event : Events)
				{
					Result->AddEvent(Event);
				}
			}
		}

		void Serialize(
			const TCHAR* Message,
			ELogVerbosity::Type Verbosity,
			const FName& Category) override
		{
			Capture(Message, Verbosity, Category);
			if (PreviousWarn != nullptr)
			{
				PreviousWarn->Serialize(
					Message,
					Verbosity,
					Category);
			}
		}

		void Serialize(
			const TCHAR* Message,
			ELogVerbosity::Type Verbosity,
			const FName& Category,
			double Time) override
		{
			Capture(Message, Verbosity, Category);
			if (PreviousWarn != nullptr)
			{
				PreviousWarn->Serialize(
					Message,
					Verbosity,
					Category,
					Time);
			}
		}

		void SerializeRecord(
			const UE::FLogRecord& Record) override
		{
			TStringBuilder<256> Message;
			Record.FormatMessageTo(Message);
			Capture(
				*Message,
				Record.GetVerbosity(),
				Record.GetCategory());
			if (PreviousWarn != nullptr)
			{
				PreviousWarn->SerializeRecord(Record);
			}
		}

		bool IsActive() const
		{
			return bActive;
		}

		void Discard()
		{
			bCommit = false;
			Events.Reset();
		}

	private:
		void Capture(
			const TCHAR* Message,
			ELogVerbosity::Type Verbosity,
			const FName& Category)
		{
			if (!bActive
				|| Result == nullptr
				|| Result->SuppressLogs()
				|| !Result->ShouldCaptureLogCategory(Category))
			{
				return;
			}

			EAutomationEventType EventType = EAutomationEventType::Info;
			if (Verbosity == ELogVerbosity::Error
				|| Verbosity == ELogVerbosity::Fatal)
			{
				EventType = EAutomationEventType::Error;
			}
			else if (Verbosity == ELogVerbosity::Warning)
			{
				EventType = Result->ElevateLogWarningsToErrors()
					? EAutomationEventType::Error
					: EAutomationEventType::Warning;
			}
			else if (Verbosity != ELogVerbosity::Display)
			{
				return;
			}

			Events.Emplace(
				EventType,
				FString::Printf(
					TEXT("%s: %s"),
					*Category.ToString(),
					Message),
				TEXT("log"));
		}

		FAutomationTestBase* Result = nullptr;
		FFeedbackContext* PreviousWarn = nullptr;
		TArray<FAutomationEvent> Events;
		bool bActive = false;
		bool bInterceptsWarn = false;
		bool bInterceptsLog = false;
		bool bCommit = true;
	};

	int ExecuteScriptContext(
		FAngelscriptContext& Context,
		FScopedDetachedAutomationLogCapture& Capture,
		FAngelscriptScriptTestExecutionContext* TestContext = nullptr)
	{
		auto Execute = [&]()
		{
			if (Capture.IsActive())
			{
				// The framework's global output device targets the currently
				// running UE test. Keep detached script logs out of that result;
				// Capture commits them to the explicitly supplied result instead.
				TGuardValue<bool> SuppressLogErrors(
					FAutomationTestBase::bSuppressLogErrors,
					true);
				TGuardValue<bool> SuppressLogWarnings(
					FAutomationTestBase::bSuppressLogWarnings,
					true);
				return Context->Execute();
			}
			return Context->Execute();
		};

		FAngelscriptScriptTestCallbackScope CallbackScope(
			TestContext,
			true);
		return Execute();
	}

	FString FormatScriptException(
		FAngelscriptContext& Context,
		const FString& Operation)
	{
		const ANSICHAR* Exception = Context->GetExceptionString();
		const char* ExceptionSection = nullptr;
		const int32 ExceptionLine =
			Context->GetExceptionLineNumber(
				nullptr,
				&ExceptionSection);
		const FString Error = FString::Printf(
			TEXT("%s threw an AngelScript exception: %s"),
			*Operation,
			Exception != nullptr
				? ANSI_TO_TCHAR(Exception)
				: TEXT("<no exception text>"));
		return ExceptionSection != nullptr
			&& ExceptionSection[0] != '\0'
			&& ExceptionLine > 0
			? FString::Printf(
				TEXT("%s:%d: %s"),
				ANSI_TO_TCHAR(ExceptionSection),
				ExceptionLine,
				*Error)
			: Error;
	}

	template <typename CallableType>
	decltype(auto) InvokeTestScript(
		FAutomationTestBase* Result,
		FAngelscriptScriptTestExecutionContext* TestContext,
		CallableType&& Callable)
	{
		FScopedDetachedAutomationLogCapture Capture(Result);
		FAngelscriptScriptTestCallbackScope CallbackScope(TestContext);
		if (Capture.IsActive())
		{
			TGuardValue<bool> SuppressLogErrors(
				FAutomationTestBase::bSuppressLogErrors,
				true);
			TGuardValue<bool> SuppressLogWarnings(
				FAutomationTestBase::bSuppressLogWarnings,
				true);
			return Forward<CallableType>(Callable)();
		}
		return Forward<CallableType>(Callable)();
	}

	template <typename CallableType>
	decltype(auto) InvokeTestScript(
		FAutomationTestBase* Result,
		CallableType&& Callable)
	{
		return InvokeTestScript(
			Result,
			nullptr,
			Forward<CallableType>(Callable));
	}

	class FAngelscriptScriptTestLatentSequence final
		: public IAutomationLatentCommand
	{
	public:
		explicit FAngelscriptScriptTestLatentSequence(
			TSharedRef<FAngelscriptScriptTestExecutionContext> InContext)
			: Context(MoveTemp(InContext))
		{
		}

		bool Update() override
		{
			return Context->Update();
		}

	private:
		TSharedRef<FAngelscriptScriptTestExecutionContext> Context;
	};

}

FAngelscriptScriptTestCallbackScope::
	FAngelscriptScriptTestCallbackScope(
		bool bInSuppressExceptionLogging)
	: bSuppressExceptionLogging(bInSuppressExceptionLogging)
{
	check(IsInGameThread());
	ActiveCallbackContexts.Add(nullptr);
	++ScriptTestCallbackDepth;
	if (bSuppressExceptionLogging)
	{
		++ScriptTestExceptionLoggingSuppressionDepth;
	}
}

FAngelscriptScriptTestCallbackScope::
	FAngelscriptScriptTestCallbackScope(
		FAngelscriptScriptTestExecutionContext* InExecutionContext,
		bool bInSuppressExceptionLogging)
	: bSuppressExceptionLogging(bInSuppressExceptionLogging)
{
	check(IsInGameThread());
	ActiveCallbackContexts.Add(InExecutionContext);
	++ScriptTestCallbackDepth;
	if (bSuppressExceptionLogging)
	{
		++ScriptTestExceptionLoggingSuppressionDepth;
	}
}

FAngelscriptScriptTestCallbackScope::
	~FAngelscriptScriptTestCallbackScope()
{
	check(IsInGameThread());
	check(ScriptTestCallbackDepth > 0);
	check(!ActiveCallbackContexts.IsEmpty());
	if (bSuppressExceptionLogging)
	{
		check(ScriptTestExceptionLoggingSuppressionDepth > 0);
		--ScriptTestExceptionLoggingSuppressionDepth;
	}
	ActiveCallbackContexts.Pop(EAllowShrinking::No);
	--ScriptTestCallbackDepth;
}

FAngelscriptScriptTestExecutionContext::
	FAngelscriptScriptTestExecutionContext(
		const FAngelscriptScriptTestDescriptor& InDescriptor,
		FAutomationTestBase* InAutomationTest)
	: Descriptor(InDescriptor)
	, AutomationTest(InAutomationTest)
{
}

FAngelscriptScriptTestExecutionContext::
	~FAngelscriptScriptTestExecutionContext()
{
	Cleanup();
}

bool FAngelscriptScriptTestExecutionContext::HasFailed() const
{
	return bFailed
		|| bCanceled
		|| (AutomationTest != nullptr && AutomationTest->HasAnyErrors());
}

bool FAngelscriptScriptTestExecutionContext::ResolveSuite()
{
	FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
	if (Engine == nullptr)
	{
		Fail(TEXT("No active AngelScript engine is available for the script test."), false);
		return false;
	}
	OwningEngine = Engine;

	const TSharedPtr<FAngelscriptModuleDesc> Module =
		Engine->GetModule(Descriptor.Id.ModuleName);
	if (!Module.IsValid())
	{
		Fail(FString::Printf(
			TEXT("Script test module '%s' is no longer active; refresh and rerun the test."),
			*Descriptor.Id.ModuleName), false);
		return false;
	}

	const TSharedPtr<FAngelscriptClassDesc> ClassDesc =
		Module->GetClass(Descriptor.Id.SuiteName);
	if (!ClassDesc.IsValid() || ClassDesc->Class == nullptr)
	{
		Fail(FString::Printf(
			TEXT("Script test suite '%s' is no longer active; refresh and rerun the test."),
			*Descriptor.Id.SuiteName), false);
		return false;
	}

	UClass* SuiteClass = ClassDesc->Class;
	if (UASClass* ScriptClass = Cast<UASClass>(SuiteClass))
	{
		SuiteClass = ScriptClass->GetMostUpToDateClass();
	}
	if (SuiteClass == nullptr
		|| !SuiteClass->IsChildOf(UAngelscriptTestSuite::StaticClass())
		|| SuiteClass->HasAnyClassFlags(
			CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		Fail(FString::Printf(
			TEXT("Script test suite '%s' does not resolve to a current concrete test class."),
			*Descriptor.Id.SuiteName), false);
		return false;
	}

	UAngelscriptTestSuite* NewSuite =
		NewObject<UAngelscriptTestSuite>(
			GetTransientPackage(),
			SuiteClass,
			NAME_None,
			RF_Transient);
	if (NewSuite == nullptr)
	{
		Fail(TEXT("Failed to construct the script test suite instance."), false);
		return false;
	}

	Suite.Reset(NewSuite);
	FAngelscriptScriptTestRunner::Associate(
		NewSuite,
		AsShared());
	return true;
}

bool FAngelscriptScriptTestExecutionContext::InvokeReflected(FName MethodName)
{
	if (Suite == nullptr)
	{
		Fail(TEXT("The script test suite instance is unavailable."), false);
		return false;
	}

	UFunction* Function = Suite->GetClass()->FindFunctionByName(MethodName);
	if (Function == nullptr)
	{
		Fail(FString::Printf(
			TEXT("Script test method '%s::%s' is no longer reflected; refresh and rerun the test."),
			*Descriptor.Id.SuiteName,
			*MethodName.ToString()), false);
		return false;
	}
	if (Function->NumParms != 0 || Function->GetReturnProperty() != nullptr)
	{
		Fail(FString::Printf(
			TEXT("Script test method '%s::%s' no longer has the required void() signature."),
			*Descriptor.Id.SuiteName,
			*MethodName.ToString()), false);
		return false;
	}

	if (UASFunction* ScriptFunction = Cast<UASFunction>(Function))
	{
		asIScriptFunction* Match = ScriptFunction->ScriptFunction;
		if (Match == nullptr
			|| Match->GetObjectType() == nullptr
			|| Match->GetParamCount() != 0
			|| Match->GetReturnTypeId() != asTYPEID_VOID)
		{
			Fail(FString::Printf(
				TEXT("Script test method '%s::%s' no longer resolves to a non-static void() method."),
				*Descriptor.Id.SuiteName,
				*MethodName.ToString()), false);
			return false;
		}

		FAngelscriptContext Context(
			Suite.Get(),
			Match->GetEngine());
		if (!PrepareAngelscriptContextWithLog(
			Context,
			Match,
			*Descriptor.DisplayName))
		{
			Fail(FString::Printf(
				TEXT("Failed to prepare script test method '%s::%s'."),
				*Descriptor.Id.SuiteName,
				*MethodName.ToString()), false);
			return false;
		}
		Context->SetObject(Suite.Get());

		FScopedDetachedAutomationLogCapture Capture(AutomationTest);
		const int ExecutionResult =
			ExecuteScriptContext(Context, Capture, this);
		if (ExecutionResult != asEXECUTION_FINISHED)
		{
			Capture.Discard();
			const bool bControlledException =
				FAngelscriptScriptTestRunner::IsControlledException(
					Context->GetExceptionString());
			bFailed = true;
			if (!bControlledException)
			{
				const FString Error =
					FormatScriptException(
						Context,
						FString::Printf(
							TEXT("Script test method '%s::%s'"),
							*Descriptor.Id.SuiteName,
							*MethodName.ToString()));
				if (AutomationTest != nullptr)
				{
					AutomationTest->AddError(Error);
				}
				else
				{
					UE_LOG(Angelscript, Error, TEXT("%s"), *Error);
				}
			}
			return false;
		}
		return !HasFailed();
	}

	InvokeTestScript(
		AutomationTest,
		this,
		[this, Function]() { Suite->ProcessEvent(Function, nullptr); });
	return !HasFailed();
}

bool FAngelscriptScriptTestExecutionContext::Start(
	bool bEnqueueAutomation)
{
	check(IsInGameThread());
	if (!ResolveSuite())
	{
		Finish();
		return false;
	}

	FAngelscriptScriptTestRunner::RememberActive(AsShared());

	Phase = EAngelscriptScriptTestPhase::BeforeEach;
	Trace.Add(TEXT("BeforeEach"));
	InvokeReflected(GET_FUNCTION_NAME_CHECKED(
		UAngelscriptTestSuite,
		BeforeEach));

	if (!HasFailed())
	{
		Phase = EAngelscriptScriptTestPhase::TestMethod;
		Trace.Add(*Descriptor.Id.MethodName);
		InvokeReflected(*Descriptor.Id.MethodName);
	}

	if (MainCommands.IsEmpty())
	{
		Finish();
		return !HasFailed();
	}

	if (EnumHasAnyFlags(
		Descriptor.Flags,
		EAutomationTestFlags::SmokeFilter
			| EAutomationTestFlags::SupportsAutoRTFM))
	{
		Fail(TEXT(
			"SmokeFilter and SupportsAutoRTFM script tests must remain synchronous; "
			"they cannot enqueue latent commands."), false);
		Finish();
		return false;
	}

	Phase = EAngelscriptScriptTestPhase::Command;
	if (bEnqueueAutomation)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(
			MakeShared<FAngelscriptScriptTestLatentSequence>(
				AsShared()));
	}
	return true;
}

bool FAngelscriptScriptTestExecutionContext::Update()
{
	check(IsInGameThread());
	if (IsComplete())
	{
		return true;
	}
	if (bCanceled || HasFailed())
	{
		if (MainCommands.IsValidIndex(MainCommandIndex))
		{
			FinalizeAdvancedCommand(MainCommands[MainCommandIndex]);
		}
		Finish();
		return true;
	}
	if (!MainCommands.IsValidIndex(MainCommandIndex))
	{
		Finish();
		return true;
	}

	FAngelscriptScriptTestCommand& Command =
		MainCommands[MainCommandIndex];
	bool bCompleted = false;
	switch (Command.Type)
	{
	case EAngelscriptScriptTestCommandType::Action:
		{
			bool Ignored = false;
			bCompleted = InvokeScriptCallback(
				Command.Callback,
				false,
				Ignored);
			break;
		}
	case EAngelscriptScriptTestCommandType::Condition:
		{
			if (!Command.bStarted)
			{
				Command.bStarted = true;
				Command.StartSeconds = FPlatformTime::Seconds();
			}
			bool bCondition = false;
			if (!InvokeScriptCallback(
				Command.Callback,
				true,
				bCondition))
			{
				bCompleted = true;
				break;
			}
			if (bCondition)
			{
				bCompleted = true;
			}
			else if (FPlatformTime::Seconds() - Command.StartSeconds
				>= Command.TimeoutSeconds)
			{
				Fail(FString::Printf(
					TEXT("%s timed out after %.3f seconds."),
					*FAngelscriptScriptTestCommands::Describe(
						Command,
						TEXT("Until")),
					Command.TimeoutSeconds), false);
				bCompleted = true;
			}
			break;
		}
	case EAngelscriptScriptTestCommandType::Delay:
		{
			if (!Command.bStarted)
			{
				Command.bStarted = true;
				Command.StartSeconds = FPlatformTime::Seconds();
			}
			bCompleted = FPlatformTime::Seconds() - Command.StartSeconds
				>= Command.TimeoutSeconds;
			break;
		}
	case EAngelscriptScriptTestCommandType::Advanced:
		{
			ULatentAutomationCommand* Advanced =
				Command.AdvancedCommand.Get();
			if (Advanced == nullptr)
			{
				Fail(TEXT(
					"The advanced latent command was released before execution."),
					false);
				bCompleted = true;
				break;
			}

			if (!Command.bStarted)
			{
				Command.bStarted = true;
				Command.StartSeconds = FPlatformTime::Seconds();
				if (!InvokeTestScript(
					AutomationTest,
					this,
					[Advanced]() { return Advanced->RunsOnClient(); }))
				{
					InvokeTestScript(
						AutomationTest,
						this,
						[Advanced]() { Advanced->Before(); });
					if (HasFailed())
					{
						FinalizeAdvancedCommand(Command);
						bCompleted = true;
					}
				}
				// Before is an explicit phase. Poll Update on the next
				// Automation update, matching the client-capable state machine.
				break;
			}

			if (InvokeTestScript(
				AutomationTest,
				this,
				[Advanced]() { return Advanced->RunsOnClient(); }))
			{
				ALatentAutomationCommandClientExecutor* Executor =
					Command.ClientExecutor.Get();
				if (Command.ClientPhase
						!= EAngelscriptScriptTestClientCommandPhase::
							CreateExecutor
					&& Command.ClientPhase
						!= EAngelscriptScriptTestClientCommandPhase::Done
					&& Executor == nullptr)
				{
					Fail(FString::Printf(
						TEXT("The client executor for an advanced command "
							"was lost during phase '%s'."),
						DescribeClientCommandPhase(Command.ClientPhase)),
						false);
					FinalizeAdvancedCommand(Command);
					bCompleted = true;
				}
				else
				{
					switch (Command.ClientPhase)
					{
					case EAngelscriptScriptTestClientCommandPhase::CreateExecutor:
						{
							APlayerController* Controller =
								TestWorld != nullptr
									? TestWorld->GetFirstPlayerController()
									: nullptr;
							if (Controller != nullptr)
							{
								FActorSpawnParameters SpawnParameters;
								SpawnParameters.Owner = Controller;
								Executor = TestWorld->SpawnActor<
									ALatentAutomationCommandClientExecutor>(
										ALatentAutomationCommandClientExecutor::
											StaticClass(),
										SpawnParameters);
								if (Executor != nullptr)
								{
									Executor->SetTest(Advanced);
									Command.ClientExecutor = Executor;
									Command.ClientPhase =
										EAngelscriptScriptTestClientCommandPhase::
											Setup;
								}
							}
							break;
						}
					case EAngelscriptScriptTestClientCommandPhase::Setup:
						InvokeTestScript(
							AutomationTest,
							this,
							[Advanced]() { Advanced->Before(); });
						if (!HasFailed())
						{
							Executor->StartBefore();
							Command.ClientPhase =
								EAngelscriptScriptTestClientCommandPhase::
									SetupClient;
						}
						break;
					case EAngelscriptScriptTestClientCommandPhase::SetupClient:
						if (Executor->Before())
						{
							Executor->StartUpdate();
							Command.ClientPhase =
								EAngelscriptScriptTestClientCommandPhase::
									Execute;
						}
						break;
					case EAngelscriptScriptTestClientCommandPhase::Execute:
						if (!Command.bServerSuccess
							&& InvokeTestScript(
								AutomationTest,
								this,
								[Advanced]() { return Advanced->Update(); }))
						{
							Command.bServerSuccess = true;
						}
						if (!Command.bClientSuccess
							&& Executor->Update())
						{
							Command.bClientSuccess = true;
						}
						if (Command.bServerSuccess
							&& Command.bClientSuccess)
						{
							Executor->StartAfter();
							Command.ClientPhase =
								EAngelscriptScriptTestClientCommandPhase::
									Finish;
						}
						break;
					case EAngelscriptScriptTestClientCommandPhase::Finish:
						if (!Command.bAdvancedAfterRan)
						{
							Command.bAdvancedAfterRan = true;
							InvokeTestScript(
								AutomationTest,
								this,
								[Advanced]() { Advanced->After(); });
						}
						Command.ClientPhase =
							EAngelscriptScriptTestClientCommandPhase::
								FinishClient;
						break;
					case EAngelscriptScriptTestClientCommandPhase::FinishClient:
						if (Executor->After())
						{
							Command.ClientPhase =
								EAngelscriptScriptTestClientCommandPhase::Done;
							FinalizeAdvancedCommand(Command);
							bCompleted = true;
						}
						break;
					case EAngelscriptScriptTestClientCommandPhase::Done:
						FinalizeAdvancedCommand(Command);
						bCompleted = true;
						break;
					default:
						checkNoEntry();
						break;
					}
				}
			}
			else if (InvokeTestScript(
				AutomationTest,
				this,
				[Advanced]() { return Advanced->Update(); }))
			{
				FinalizeAdvancedCommand(Command);
				bCompleted = true;
			}

			if (!bCompleted && HasFailed())
			{
				FinalizeAdvancedCommand(Command);
				bCompleted = true;
			}

			if (!bCompleted
				&& FPlatformTime::Seconds() - Command.StartSeconds
					>= Command.TimeoutSeconds)
			{
				const FString Description = InvokeTestScript(
					AutomationTest,
					this,
					[Advanced]() { return Advanced->Describe(); });
				if (!InvokeTestScript(
					AutomationTest,
					this,
					[Advanced]() { return Advanced->AllowsTimeout(); }))
				{
					Fail(FString::Printf(
						TEXT("Timed out waiting for advanced command '%s' after %.3f seconds."),
						Description.IsEmpty()
							? TEXT("<unnamed>")
							: *Description,
						Command.TimeoutSeconds),
						false);
				}
				// The timeout is an overall deadline, including client setup
				// and teardown. A non-responsive client cannot be allowed to
				// strand this leaf indefinitely; server cleanup still runs.
				FinalizeAdvancedCommand(Command);
				bCompleted = true;
			}
			break;
		}
	default:
		checkNoEntry();
		bCompleted = true;
		break;
	}

	if (bCompleted)
	{
		if (Command.Type
			== EAngelscriptScriptTestCommandType::Advanced)
		{
			Command.AdvancedCommand.Reset();
		}
		++MainCommandIndex;
	}
	if (HasFailed() || MainCommandIndex >= MainCommands.Num())
	{
		Finish();
		return true;
	}
	return false;
}

void FAngelscriptScriptTestExecutionContext::Cancel(
	const FString& Reason)
{
	if (IsComplete())
	{
		return;
	}
	bCanceled = true;
	CancellationReason = Reason;
	Fail(FString::Printf(
		TEXT("Script test was invalidated: %s. Refresh and rerun the leaf."),
		*Reason), false);
	if (MainCommands.IsValidIndex(MainCommandIndex))
	{
		// Match the asynchronous Update cancellation path: an advanced
		// command that has entered Before/Update must run After and release
		// its association before the suite enters AfterEach and teardown.
		FinalizeAdvancedCommand(MainCommands[MainCommandIndex]);
	}
	Finish();
}

void FAngelscriptScriptTestExecutionContext::Fail(
	const FString& Message,
	bool bThrowControlledException)
{
	bFailed = true;
	if (AutomationTest != nullptr)
	{
		FString SourceFile = Descriptor.SourceFile;
		int32 SourceLine = Descriptor.SourceLine;
		if (asIScriptContext* ActiveContext = asGetActiveContext())
		{
			if (ActiveContext->GetCallstackSize() > 0)
			{
				const char* ActiveFile = nullptr;
				const int32 ActiveLine =
					ActiveContext->GetLineNumber(
						0,
						nullptr,
						&ActiveFile);
				if (ActiveFile != nullptr
					&& ActiveFile[0] != '\0'
					&& ActiveLine > 0)
				{
					SourceFile = ANSI_TO_TCHAR(ActiveFile);
					SourceLine = ActiveLine;
				}
			}
		}
		AutomationTest->AddError(FString::Printf(
			TEXT("%s:%d: %s"),
			*SourceFile,
			SourceLine,
			*Message));
	}
	else
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Message);
	}

	if (bThrowControlledException
		&& asGetActiveContext() != nullptr)
	{
		FAngelscriptEngine::Throw(ControlledAssertionException);
	}
}

void FAngelscriptScriptTestExecutionContext::ExpectError(
	const FString& Pattern,
	EAutomationExpectedErrorFlags::MatchType MatchType,
	int32 Occurrences,
	bool bRegex)
{
	if (AutomationTest == nullptr)
	{
		Fail(TEXT("Expected-log helpers require an active Automation leaf."));
		return;
	}
	if (Pattern.IsEmpty() || Occurrences < 0)
	{
		Fail(TEXT("Expected-log pattern must be non-empty and occurrences must be non-negative."));
		return;
	}
	if (bRegex)
	{
		AutomationTest->AddExpectedError(
			Pattern,
			MatchType,
			Occurrences,
			true);
	}
	else
	{
		AutomationTest->AddExpectedErrorPlain(
			Pattern,
			MatchType,
			Occurrences);
	}
}

bool FAngelscriptScriptTestExecutionContext::ValidateTimeout(
	double Seconds,
	const FString& Operation)
{
	if (!FAngelscriptScriptTestCommands::IsValidTimeout(Seconds))
	{
		Fail(FString::Printf(
			TEXT("%s timeout must be finite, greater than zero, and at most %.0f seconds."),
			*Operation,
			FAngelscriptScriptTestCommands::MaximumTimeoutSeconds));
		return false;
	}
	return true;
}

bool FAngelscriptScriptTestExecutionContext::CanBuildMainCommand() const
{
	return Phase == EAngelscriptScriptTestPhase::BeforeEach
		|| Phase == EAngelscriptScriptTestPhase::TestMethod;
}

bool FAngelscriptScriptTestExecutionContext::CanRegisterTeardown() const
{
	return CanBuildMainCommand()
		|| Phase == EAngelscriptScriptTestPhase::AfterEach;
}

bool FAngelscriptScriptTestExecutionContext::EnqueueAction(
	FName Callback,
	const FString& Description,
	bool bTeardown)
{
	if ((bTeardown && !CanRegisterTeardown())
		|| (!bTeardown && !CanBuildMainCommand()))
	{
		Fail(TEXT("Commands cannot mutate the active queue during command execution."));
		return false;
	}

	bool Ignored = false;
	if (!InvokeScriptCallback(Callback, false, Ignored))
	{
		return false;
	}
	// Signature validation must not execute the callback at enqueue time.
	// InvokeScriptCallback recognizes None phase as validation-only.
	FAngelscriptScriptTestCommand Command;
	Command.Type = EAngelscriptScriptTestCommandType::Action;
	Command.Callback = Callback;
	Command.Description = Description;
	if (bTeardown)
	{
		TeardownCommands.Add(MoveTemp(Command));
	}
	else
	{
		MainCommands.Add(MoveTemp(Command));
	}
	return true;
}

bool FAngelscriptScriptTestExecutionContext::EnqueueCondition(
	FName Callback,
	double TimeoutSeconds,
	const FString& Description)
{
	if (!CanBuildMainCommand())
	{
		Fail(TEXT("Commands cannot mutate the active queue during command execution."));
		return false;
	}
	if (!ValidateTimeout(TimeoutSeconds, TEXT("Until")))
	{
		return false;
	}
	bool Ignored = false;
	if (!InvokeScriptCallback(Callback, true, Ignored))
	{
		return false;
	}
	FAngelscriptScriptTestCommand& Command =
		MainCommands.AddDefaulted_GetRef();
	Command.Type = EAngelscriptScriptTestCommandType::Condition;
	Command.Callback = Callback;
	Command.TimeoutSeconds = TimeoutSeconds;
	Command.Description = Description;
	return true;
}

bool FAngelscriptScriptTestExecutionContext::EnqueueDelay(
	double Seconds,
	const FString& Description)
{
	if (!CanBuildMainCommand())
	{
		Fail(TEXT("Commands cannot mutate the active queue during command execution."));
		return false;
	}
	if (!ValidateTimeout(Seconds, TEXT("WaitDelay")))
	{
		return false;
	}
	FAngelscriptScriptTestCommand& Command =
		MainCommands.AddDefaulted_GetRef();
	Command.Type = EAngelscriptScriptTestCommandType::Delay;
	Command.TimeoutSeconds = Seconds;
	Command.Description = Description;
	return true;
}

bool FAngelscriptScriptTestExecutionContext::EnqueueAdvanced(
	ULatentAutomationCommand* Command,
	double TimeoutSeconds)
{
	if (!CanBuildMainCommand())
	{
		Fail(TEXT("Advanced commands can only be added while building the main queue."));
		return false;
	}
	if (Command == nullptr
		|| !ValidateTimeout(TimeoutSeconds, TEXT("AddLatentCommand")))
	{
	if (Command == nullptr)
		{
			Fail(TEXT("AddLatentCommand requires a command instance."));
		}
		return false;
	}
	if (Command->RunsOnClient())
	{
		UWorld* World = GetWorld();
		if (World == nullptr
			|| World->GetNetDriver() == nullptr)
		{
			Fail(TEXT(
				"A client-enabled ULatentAutomationCommand requires an "
				"already network-capable test World; the reflected script "
				"test framework does not create PIE or network participants."));
			return false;
		}
	}
	Command->SetExecutionContext(AsShared());
	Command->SetWorld(GetWorld());
	FAngelscriptScriptTestCommand& Entry =
		MainCommands.AddDefaulted_GetRef();
	Entry.Type = EAngelscriptScriptTestCommandType::Advanced;
	Entry.TimeoutSeconds = TimeoutSeconds;
	Entry.AdvancedCommand.Reset(Command);
	return true;
}

void FAngelscriptScriptTestExecutionContext::FinalizeAdvancedCommand(
	FAngelscriptScriptTestCommand& Command)
{
	if (Command.Type != EAngelscriptScriptTestCommandType::Advanced
		|| Command.AdvancedCommand == nullptr)
	{
		return;
	}
	ULatentAutomationCommand* Advanced =
		Command.AdvancedCommand.Get();
	if (Command.bStarted && !Command.bAdvancedAfterRan)
	{
		Command.bAdvancedAfterRan = true;
		InvokeTestScript(
			AutomationTest,
			this,
			[Advanced]() { Advanced->After(); });
	}
	if (ALatentAutomationCommandClientExecutor* Executor =
		Command.ClientExecutor.Get())
	{
		Executor->Destroy();
		Command.ClientExecutor.Reset();
	}
	Advanced->ClearExecutionContext();
}

bool FAngelscriptScriptTestExecutionContext::InvokeScriptCallback(
	FName MethodName,
	bool bCondition,
	bool& OutValue)
{
	OutValue = false;
	if (Suite == nullptr || MethodName.IsNone())
	{
		Fail(TEXT("A command callback name must be specified."));
		return false;
	}

	UASClass* ScriptClass = Cast<UASClass>(Suite->GetClass());
	if (ScriptClass == nullptr || ScriptClass->ScriptTypePtr == nullptr)
	{
		Fail(TEXT("Command callbacks require a current AngelScript suite class."));
		return false;
	}

	asITypeInfo* Type =
		static_cast<asITypeInfo*>(ScriptClass->ScriptTypePtr);
	asIScriptFunction* Match = nullptr;
	int32 MatchCount = 0;
	for (asUINT Index = 0; Index < Type->GetMethodCount(); ++Index)
	{
		asIScriptFunction* Candidate = Type->GetMethodByIndex(Index);
		if (Candidate != nullptr
			&& MethodName.ToString().Equals(
				ANSI_TO_TCHAR(Candidate->GetName()),
				ESearchCase::CaseSensitive))
		{
			Match = Candidate;
			++MatchCount;
		}
	}

	const int ExpectedReturnType =
		bCondition ? asTYPEID_BOOL : asTYPEID_VOID;
	if (MatchCount != 1
		|| Match == nullptr
		|| Match->GetObjectType() == nullptr
		|| Match->GetParamCount() != 0
		|| Match->GetReturnTypeId() != ExpectedReturnType)
	{
		Fail(FString::Printf(
			TEXT("Callback '%s' must resolve uniquely to a non-static %s() method."),
			*MethodName.ToString(),
			bCondition ? TEXT("bool") : TEXT("void")));
		return false;
	}

	// During queue construction this function is validation-only.
	if (Phase != EAngelscriptScriptTestPhase::Command
		&& Phase != EAngelscriptScriptTestPhase::Teardown)
	{
		return true;
	}

	FAngelscriptContext Context(
		Suite.Get(),
		Match->GetEngine());
	if (!PrepareAngelscriptContextWithLog(
		Context,
		Match,
		*Descriptor.DisplayName))
	{
		Fail(FString::Printf(
			TEXT("Failed to prepare callback '%s'."),
			*MethodName.ToString()), false);
		return false;
	}
	Context->SetObject(Suite.Get());
	Trace.Add(MethodName);
	FScopedDetachedAutomationLogCapture Capture(AutomationTest);
	const int ExecutionResult =
		ExecuteScriptContext(Context, Capture, this);
	if (ExecutionResult != asEXECUTION_FINISHED)
	{
		Capture.Discard();
		const bool bControlledException =
			FAngelscriptScriptTestRunner::IsControlledException(
				Context->GetExceptionString());
		bFailed = true;
		if (!bControlledException)
		{
			const FString Error =
				FormatScriptException(
					Context,
					FString::Printf(
						TEXT("Callback '%s'"),
						*MethodName.ToString()));
			if (AutomationTest != nullptr)
			{
				AutomationTest->AddError(Error);
			}
			else
			{
				UE_LOG(Angelscript, Error, TEXT("%s"), *Error);
			}
		}
		return false;
	}
	if (bCondition)
	{
		OutValue = Context->GetReturnByte() != 0;
	}
	return !HasFailed();
}

void FAngelscriptScriptTestExecutionContext::RunAfterEach()
{
	if (bAfterEachRan || Suite == nullptr)
	{
		return;
	}
	bAfterEachRan = true;
	Phase = EAngelscriptScriptTestPhase::AfterEach;
	Trace.Add(TEXT("AfterEach"));
	InvokeReflected(GET_FUNCTION_NAME_CHECKED(
		UAngelscriptTestSuite,
		AfterEach));
}

void FAngelscriptScriptTestExecutionContext::RunNextTeardown()
{
	if (TeardownCommands.IsEmpty())
	{
		return;
	}
	Phase = EAngelscriptScriptTestPhase::Teardown;
	while (!TeardownCommands.IsEmpty())
	{
		FAngelscriptScriptTestCommand Command =
			TeardownCommands.Pop(EAllowShrinking::No);
		bool Ignored = false;
		InvokeScriptCallback(Command.Callback, false, Ignored);
	}
}

void FAngelscriptScriptTestExecutionContext::Finish()
{
	if (IsComplete())
	{
		return;
	}
	RunAfterEach();
	RunNextTeardown();
	if (!bExpectedMessagesFinalized
		&& AutomationTest != nullptr
		&& AutomationTest
			!= FAutomationTestFramework::Get().GetCurrentTest())
	{
		bExpectedMessagesFinalized = true;
		if (!AutomationTest->HasMetExpectedMessages())
		{
			bFailed = true;
		}
	}
	Cleanup();
	Phase = EAngelscriptScriptTestPhase::Complete;
	FAngelscriptScriptTestRunner::ForgetActive(this);
}

bool FAngelscriptScriptTestExecutionContext::CreateTestWorld(
	bool bInitializeGameSubsystems)
{
	if (TestWorld != nullptr)
	{
		Fail(TEXT("CreateTestWorld cannot replace an active test World."));
		return false;
	}
	if (Phase == EAngelscriptScriptTestPhase::BeforeAll
		|| Phase == EAngelscriptScriptTestPhase::AfterAll
		|| IsComplete())
	{
		Fail(TEXT("CreateTestWorld requires an active method leaf."));
		return false;
	}

	const FName UniqueWorldName =
		MakeUniqueObjectName(
			GetTransientPackage(),
			UWorld::StaticClass(),
			TEXT("AngelscriptScriptTestWorld"));
	if (bInitializeGameSubsystems)
	{
		UGameInstance* GameInstance =
			NewObject<UGameInstance>(GEngine, UGameInstance::StaticClass());
		if (GameInstance == nullptr)
		{
			Fail(TEXT("CreateTestWorld failed to create a GameInstance."));
			return false;
		}
		TestGameInstance.Reset(GameInstance);
		GameInstance->InitializeStandalone(
			UniqueWorldName,
			GetTransientPackage());
		TestWorld.Reset(GameInstance->GetWorld());
		if (TestWorld == nullptr)
		{
			GameInstance->Shutdown();
			TestGameInstance.Reset();
		}
	}
	else
	{
		UWorld* World = UWorld::CreateWorld(
			EWorldType::Game,
			false,
			UniqueWorldName,
			GetTransientPackage());
		if (World != nullptr)
		{
			World->AddToRoot();
			bWorldRooted = true;
			FWorldContext& WorldContext =
				GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			TestWorld.Reset(World);
		}
	}

	if (TestWorld == nullptr)
	{
		Fail(TEXT("CreateTestWorld failed to create a World."));
		return false;
	}
	return true;
}

void FAngelscriptScriptTestExecutionContext::DestroyTestWorld()
{
	if (TestWorld == nullptr)
	{
		TrackedComponents.Reset();
		TrackedActors.Reset();
		TrackedObjects.Reset();
		TestGameInstance.Reset();
		bWorldRooted = false;
		return;
	}

	for (int32 Index = TrackedComponents.Num() - 1; Index >= 0; --Index)
	{
		if (UActorComponent* Component = TrackedComponents[Index].Get())
		{
			Component->DestroyComponent();
		}
	}
	TrackedComponents.Reset();

	for (int32 Index = TrackedActors.Num() - 1; Index >= 0; --Index)
	{
		if (AActor* Actor = TrackedActors[Index].Get())
		{
			Actor->Destroy();
		}
	}
	TrackedActors.Reset();
	TrackedObjects.Reset();

	UWorld* World = TestWorld.Get();
	if (World != nullptr)
	{
		World->BeginTearingDown();
	}
	if (TestGameInstance != nullptr)
	{
		TestGameInstance->Shutdown();
	}
	if (World != nullptr)
	{
		GEngine->ShutdownWorldNetDriver(World);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		if (bWorldRooted && World->IsRooted())
		{
			World->RemoveFromRoot();
		}
	}
	bWorldRooted = false;
	TestWorld.Reset();
	TestGameInstance.Reset();
}

UObject* FAngelscriptScriptTestExecutionContext::SpawnObject(
	UClass* ObjectClass,
	UObject* Outer)
{
	ObjectClass =
		FAngelscriptScriptTestWorld::ResolveCurrentClass(
			ObjectClass);
	if (ObjectClass == nullptr
		|| !ObjectClass->IsChildOf(UObject::StaticClass())
		|| ObjectClass->IsChildOf(AActor::StaticClass())
		|| ObjectClass->IsChildOf(UActorComponent::StaticClass())
		|| ObjectClass->HasAnyClassFlags(
			CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		Fail(TEXT(
			"SpawnObject requires a current concrete non-Actor, "
			"non-ActorComponent UObject class."));
		return nullptr;
	}
	UObject* Object = NewObject<UObject>(
		Outer != nullptr ? Outer : Suite.Get(),
		ObjectClass,
		NAME_None,
		RF_Transient);
	if (Object == nullptr)
	{
		Fail(TEXT("SpawnObject failed to construct the requested object."));
		return nullptr;
	}
	TrackedObjects.Emplace(Object);
	return Object;
}

AActor* FAngelscriptScriptTestExecutionContext::SpawnActor(
	UClass* ActorClass,
	const FVector& Location,
	const FRotator& Rotation)
{
	ActorClass =
		FAngelscriptScriptTestWorld::ResolveCurrentClass(
			ActorClass);
	if (TestWorld == nullptr)
	{
		Fail(TEXT("SpawnActor requires CreateTestWorld first."));
		return nullptr;
	}
	if (ActorClass == nullptr
		|| !ActorClass->IsChildOf(AActor::StaticClass())
		|| ActorClass->HasAnyClassFlags(
			CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		Fail(TEXT("SpawnActor requires a current concrete Actor class."));
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	AActor* Actor = TestWorld->SpawnActor<AActor>(
		ActorClass,
		Location,
		Rotation,
		Params);
	if (Actor == nullptr)
	{
		Fail(TEXT("SpawnActor failed to create the requested Actor."));
		return nullptr;
	}
	TrackedActors.Add(Actor);
	return Actor;
}

UActorComponent* FAngelscriptScriptTestExecutionContext::SpawnComponent(
	UClass* ComponentClass,
	AActor* Owner,
	bool bRegister)
{
	ComponentClass =
		FAngelscriptScriptTestWorld::ResolveCurrentClass(
			ComponentClass);
	if (!OwnsActor(Owner))
	{
		Fail(TEXT("SpawnComponent owner must belong to the active test World."));
		return nullptr;
	}
	if (ComponentClass == nullptr
		|| !ComponentClass->IsChildOf(UActorComponent::StaticClass())
		|| ComponentClass->HasAnyClassFlags(
			CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		Fail(TEXT("SpawnComponent requires a current concrete component class."));
		return nullptr;
	}
	UActorComponent* Component =
		NewObject<UActorComponent>(
			Owner,
			ComponentClass,
			NAME_None,
			RF_Transient);
	if (bRegister)
	{
		Component->RegisterComponent();
		Component->Activate();
	}
	TrackedComponents.Add(Component);
	return Component;
}

bool FAngelscriptScriptTestExecutionContext::OwnsActor(
	const AActor* Actor) const
{
	return Actor != nullptr
		&& TestWorld != nullptr
		&& Actor->GetWorld() == TestWorld.Get();
}

bool FAngelscriptScriptTestExecutionContext::OwnsComponent(
	const UActorComponent* Component) const
{
	return Component != nullptr
		&& Component->GetOwner() != nullptr
		&& OwnsActor(Component->GetOwner());
}

void FAngelscriptScriptTestExecutionContext::BeginPlay(AActor* Actor)
{
	if (!OwnsActor(Actor))
	{
		Fail(TEXT("BeginPlay Actor must belong to the active test World."));
		return;
	}
	Actor->DispatchBeginPlay();
}

void FAngelscriptScriptTestExecutionContext::BeginPlayAll()
{
	for (const TWeakObjectPtr<AActor>& Actor : TrackedActors)
	{
		if (Actor.IsValid())
		{
			Actor->DispatchBeginPlay();
		}
	}
}

void FAngelscriptScriptTestExecutionContext::TickWorld(
	float DeltaSeconds,
	int32 NumTicks)
{
	if (TestWorld == nullptr
		|| !FAngelscriptScriptTestWorld::
			AreTickArgumentsValid(DeltaSeconds, NumTicks))
	{
		Fail(TEXT("TickWorld requires an active World, non-negative delta, and non-negative tick count."));
		return;
	}
	for (int32 Index = 0; Index < NumTicks; ++Index)
	{
		TestWorld->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
	}
}

void FAngelscriptScriptTestExecutionContext::TickActor(
	AActor* Actor,
	float DeltaSeconds,
	int32 NumTicks)
{
	if (!OwnsActor(Actor)
		|| !FAngelscriptScriptTestWorld::
			AreTickArgumentsValid(DeltaSeconds, NumTicks))
	{
		Fail(TEXT("TickActor requires an Actor in the test World and valid tick arguments."));
		return;
	}
	for (int32 Index = 0; Index < NumTicks; ++Index)
	{
		Actor->TickActor(
			DeltaSeconds,
			LEVELTICK_All,
			Actor->PrimaryActorTick);
	}
}

void FAngelscriptScriptTestExecutionContext::TickComponent(
	UActorComponent* Component,
	float DeltaSeconds,
	int32 NumTicks)
{
	if (!OwnsComponent(Component)
		|| !Component->IsRegistered()
		|| !FAngelscriptScriptTestWorld::
			AreTickArgumentsValid(DeltaSeconds, NumTicks))
	{
		Fail(TEXT("TickComponent requires a registered component in the test World and valid tick arguments."));
		return;
	}
	for (int32 Index = 0; Index < NumTicks; ++Index)
	{
		Component->TickComponent(
			DeltaSeconds,
			LEVELTICK_All,
			&Component->PrimaryComponentTick);
	}
}

void FAngelscriptScriptTestExecutionContext::AdvanceTime(
	float DeltaSeconds,
	int32 NumTicks)
{
	TickWorld(DeltaSeconds, NumTicks);
}

void FAngelscriptScriptTestExecutionContext::DestroyActor(
	AActor* Actor,
	bool bDrain)
{
	if (!OwnsActor(Actor))
	{
		Fail(TEXT("DestroyActor requires an Actor in the active test World."));
		return;
	}
	Actor->Destroy();
	if (bDrain && TestWorld != nullptr)
	{
		TestWorld->Tick(ELevelTick::LEVELTICK_All, 0.0f);
	}
}

void FAngelscriptScriptTestExecutionContext::Cleanup()
{
	if (bCleanupRan)
	{
		return;
	}
	bCleanupRan = true;
	Phase = EAngelscriptScriptTestPhase::Cleanup;
	for (FAngelscriptScriptTestCommand& Command : MainCommands)
	{
		FinalizeAdvancedCommand(Command);
		Command.AdvancedCommand.Reset();
	}
	DestroyTestWorld();
	for (FAngelscriptScriptTestCommand& Command : TeardownCommands)
	{
		FinalizeAdvancedCommand(Command);
		Command.AdvancedCommand.Reset();
	}
	MainCommands.Reset();
	TeardownCommands.Reset();
	if (Suite != nullptr)
	{
		FAngelscriptScriptTestRunner::Dissociate(Suite.Get());
	}
	Suite.Reset();
}

bool FAngelscriptScriptTestRunner::Run(
	const FAngelscriptScriptTestId& Id,
	FAutomationTestBase& AutomationTest)
{
	const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
		Start(Id, AutomationTest);
	return Context.IsValid() && !Context->HasFailed();
}

TSharedPtr<FAngelscriptScriptTestExecutionContext>
FAngelscriptScriptTestRunner::Start(
	const FAngelscriptScriptTestId& Id,
	FAutomationTestBase& AutomationTest,
	bool bEnqueueAutomation)
{
	const TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot =
		FAngelscriptScriptTestRegistry::Get().GetSnapshot();
	const FAngelscriptScriptTestDescriptor* Descriptor =
		Snapshot.IsValid() ? Snapshot->Find(Id) : nullptr;
	if (Descriptor == nullptr)
	{
		AutomationTest.AddError(
			TEXT("The cached script test no longer exists. Refresh the Automation test list and rerun."));
		return nullptr;
	}

	TSharedRef<FAngelscriptScriptTestExecutionContext> Context =
		MakeShared<FAngelscriptScriptTestExecutionContext>(
			*Descriptor,
			&AutomationTest);
	Context->Start(bEnqueueAutomation);
	return Context;
}

TSharedPtr<FAngelscriptScriptTestExecutionContext>
FAngelscriptScriptTestRunner::FindContext(
	const UAngelscriptTestSuite* Suite)
{
	if (const TWeakPtr<FAngelscriptScriptTestExecutionContext>* Found =
		ContextsBySuite.Find(Suite))
	{
		return Found->Pin();
	}
	return nullptr;
}

FAngelscriptScriptTestExecutionContext*
FAngelscriptScriptTestRunner::GetActiveContext()
{
	check(IsInGameThread());
	if (ActiveCallbackContexts.IsEmpty())
	{
		return nullptr;
	}
	FAngelscriptScriptTestExecutionContext* Context =
		ActiveCallbackContexts.Last();
	return Context != nullptr && !Context->IsComplete()
		? Context
		: nullptr;
}

UWorld* FAngelscriptScriptTestRunner::FindWorld(
	const UAngelscriptTestSuite* Suite)
{
	const TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
		FindContext(Suite);
	return Context.IsValid() ? Context->GetWorld() : nullptr;
}

bool FAngelscriptScriptTestRunner::InvokeSuiteLifecycle(
	UAngelscriptTestSuite* Suite,
	FName MethodName,
	EAngelscriptScriptTestPhase Phase,
	FAutomationTestBase& Result)
{
	check(IsInGameThread());
	if (Suite == nullptr
		|| (Phase != EAngelscriptScriptTestPhase::BeforeAll
			&& Phase != EAngelscriptScriptTestPhase::AfterAll))
	{
		Result.AddError(TEXT(
			"Suite lifecycle invocation requires a suite instance and "
			"BeforeAll or AfterAll phase."));
		return false;
	}

	UFunction* ReflectedFunction =
		Suite->GetClass()->FindFunctionByName(MethodName);
	UASFunction* ScriptOverride =
		Cast<UASFunction>(ReflectedFunction);
	if (ScriptOverride == nullptr)
	{
		// The native lifecycle implementations are intentional no-ops. A
		// script BlueprintOverride is represented by a UASFunction owned by
		// the generated class; the parent native event remains a UFunction.
		return true;
	}
	asIScriptFunction* Match =
		ScriptOverride->ScriptFunction;
	if (Match == nullptr
		|| Match->GetObjectType() == nullptr
		|| Match->GetParamCount() != 0
		|| Match->GetReturnTypeId() != asTYPEID_VOID)
	{
		Result.AddError(FString::Printf(
			TEXT("%s must resolve to a non-static void() "
				"script lifecycle method."),
			*MethodName.ToString()));
		return false;
	}

	FAngelscriptContext Context(
		Suite,
		Match->GetEngine());
	if (!PrepareAngelscriptContextWithLog(
		Context,
		Match,
		DescribeLifecyclePhase(Phase)))
	{
		Result.AddError(FString::Printf(
			TEXT("Failed to prepare %s for reflected script suite '%s'."),
			DescribeLifecyclePhase(Phase),
			*Suite->GetClass()->GetName()));
		return false;
	}
	Context->SetObject(Suite);

	const int32 ErrorsBefore = GetAutomationErrorCount(Result);
	int ExecutionResult = asEXECUTION_ERROR;
	{
		FScopedDetachedAutomationLogCapture Capture(&Result);
		TGuardValue<const UAngelscriptTestSuite*> SuiteGuard(
			ActiveLifecycleSuite,
			Suite);
		TGuardValue<FAutomationTestBase*> ResultGuard(
			ActiveLifecycleResult,
			&Result);
		TGuardValue<EAngelscriptScriptTestPhase> PhaseGuard(
			ActiveLifecyclePhase,
			Phase);
		ExecutionResult = ExecuteScriptContext(Context, Capture);
		if (ExecutionResult != asEXECUTION_FINISHED)
		{
			// AngelScript's exception logger emits a multi-line error. Keep
			// one authoritative, source-located lifecycle diagnostic below.
			Capture.Discard();
		}
	}

	if (ExecutionResult != asEXECUTION_FINISHED
		&& GetAutomationErrorCount(Result) == ErrorsBefore)
	{
		const ANSICHAR* Exception = Context->GetExceptionString();
		const char* ExceptionSection = nullptr;
		const int32 ExceptionLine =
			Context->GetExceptionLineNumber(
				nullptr,
				&ExceptionSection);
		const FString Error = FString::Printf(
			TEXT("%s threw an AngelScript exception: %s"),
			DescribeLifecyclePhase(Phase),
			Exception != nullptr
				? ANSI_TO_TCHAR(Exception)
				: TEXT("<no exception text>"));
		Result.AddError(
			ExceptionSection != nullptr
				&& ExceptionSection[0] != '\0'
				&& ExceptionLine > 0
				? FString::Printf(
					TEXT("%s:%d: %s"),
					ANSI_TO_TCHAR(ExceptionSection),
					ExceptionLine,
					*Error)
				: Error);
	}
	return ExecutionResult == asEXECUTION_FINISHED
		&& GetAutomationErrorCount(Result) == ErrorsBefore;
}

bool FAngelscriptScriptTestRunner::ReportSuiteLifecycleMisuse(
	const UAngelscriptTestSuite* Suite,
	const FString& Message)
{
	if (Suite == nullptr || Suite != ActiveLifecycleSuite)
	{
		return false;
	}
	return ReportActiveLifecycleMisuse(Message);
}

bool FAngelscriptScriptTestRunner::ReportActiveLifecycleMisuse(
	const FString& Message)
{
	if (ActiveLifecycleResult == nullptr)
	{
		return false;
	}

	ActiveLifecycleResult->AddError(
		FormatActiveScriptError(FString::Printf(
			TEXT("%s cannot use method-local test helpers: %s"),
			DescribeLifecyclePhase(ActiveLifecyclePhase),
			*Message)));
	if (asGetActiveContext() != nullptr)
	{
		FAngelscriptEngine::Throw(
			ControlledAssertionException);
	}
	return true;
}

void FAngelscriptScriptTestRunner::ReportActiveContextMisuse(
	const FString& Message)
{
	if (ReportActiveLifecycleMisuse(Message))
	{
		return;
	}

	if (FAutomationTestBase* Current =
		FAutomationTestFramework::Get().GetCurrentTest())
	{
		Current->AddError(FormatActiveScriptError(Message));
	}
	else
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Message);
	}
	if (asGetActiveContext() != nullptr)
	{
		FAngelscriptEngine::Throw(ControlledAssertionException);
	}
}

void FAngelscriptScriptTestRunner::Associate(
	UAngelscriptTestSuite* Suite,
	const TSharedRef<FAngelscriptScriptTestExecutionContext>& Context)
{
	ContextsBySuite.Add(Suite, Context);
}

void FAngelscriptScriptTestRunner::Dissociate(
	UAngelscriptTestSuite* Suite)
{
	ContextsBySuite.Remove(Suite);
}

void FAngelscriptScriptTestRunner::RememberActive(
	const TSharedRef<FAngelscriptScriptTestExecutionContext>& Context)
{
	ActiveContexts.RemoveAll(
		[](const TWeakPtr<FAngelscriptScriptTestExecutionContext>& Item)
		{
			return !Item.IsValid();
		});
	ActiveContexts.Add(Context);
}

void FAngelscriptScriptTestRunner::ForgetActive(
	const FAngelscriptScriptTestExecutionContext* Context)
{
	ActiveContexts.RemoveAll(
		[Context](
			const TWeakPtr<FAngelscriptScriptTestExecutionContext>& Item)
		{
			const TSharedPtr<FAngelscriptScriptTestExecutionContext> Pinned =
				Item.Pin();
			return !Pinned.IsValid() || Pinned.Get() == Context;
		});
}

void FAngelscriptScriptTestRunner::CancelModules(
	const TSet<FString>& ModuleNames,
	const FString& Reason)
{
	TArray<TSharedPtr<FAngelscriptScriptTestExecutionContext>> ToCancel;
	for (const TWeakPtr<FAngelscriptScriptTestExecutionContext>& Item :
		ActiveContexts)
	{
		if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			Item.Pin();
			Context.IsValid()
			&& ModuleNames.Contains(
				Context->GetDescriptor().Id.ModuleName))
		{
			ToCancel.Add(MoveTemp(Context));
		}
	}
	for (const TSharedPtr<FAngelscriptScriptTestExecutionContext>& Context :
		ToCancel)
	{
		Context->Cancel(Reason);
	}
}

void FAngelscriptScriptTestRunner::CancelEngine(
	FAngelscriptEngine* Engine,
	const FString& Reason)
{
	check(IsInGameThread());
	if (Engine == nullptr)
	{
		return;
	}

	TArray<TSharedPtr<FAngelscriptScriptTestExecutionContext>> ToCancel;
	for (const TWeakPtr<FAngelscriptScriptTestExecutionContext>& Item :
		ActiveContexts)
	{
		if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			Item.Pin();
			Context.IsValid()
			&& Context->OwningEngine == Engine)
		{
			ToCancel.Add(MoveTemp(Context));
		}
	}
	for (const TSharedPtr<FAngelscriptScriptTestExecutionContext>& Context :
		ToCancel)
	{
		Context->Cancel(Reason);
	}
}

void FAngelscriptScriptTestRunner::CancelAll(
	const FString& Reason)
{
	TArray<TSharedPtr<FAngelscriptScriptTestExecutionContext>> ToCancel;
	for (const TWeakPtr<FAngelscriptScriptTestExecutionContext>& Item :
		ActiveContexts)
	{
		if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			Item.Pin())
		{
			ToCancel.Add(MoveTemp(Context));
		}
	}
	for (const TSharedPtr<FAngelscriptScriptTestExecutionContext>& Context :
		ToCancel)
	{
		Context->Cancel(Reason);
	}
}

bool FAngelscriptScriptTestRunner::IsExecutingScriptCallback()
{
	return ScriptTestCallbackDepth > 0;
}

bool FAngelscriptScriptTestRunner::
	ShouldSuppressScriptExceptionLogging()
{
	return ScriptTestExceptionLoggingSuppressionDepth > 0;
}

bool FAngelscriptScriptTestRunner::IsControlledException(
	const ANSICHAR* Exception)
{
	return Exception != nullptr
		&& FCStringAnsi::Strcmp(
			Exception,
			ControlledAssertionException) == 0;
}
