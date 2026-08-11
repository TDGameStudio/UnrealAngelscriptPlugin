#include "AngelscriptDebuggerTestSession.h"

#include "Binds/Bind_Debugging.h"
#include "ClassGenerator/ASClass.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/PlatformProcess.h"

namespace
{
	struct FGeneratedBoolInvocationParams
	{
		bool Condition = false;
		FString Message;
		bool ReturnValue = false;
	};

	struct FGeneratedVoidInvocationParams
	{
		bool Condition = false;
		FString Message;
	};

	int32 MakeUniqueDebugServerPort()
	{
		static int32 NextOffset = 0;
		constexpr int32 BasePort = 30000;
		constexpr int32 PortWindow = 10000;
		constexpr int32 BucketSize = 100;
		constexpr int32 ProcessBucketCount = PortWindow / BucketSize;
		const int32 ProcessBucket = (FPlatformProcess::GetCurrentProcessId() % ProcessBucketCount) * BucketSize;
		const int32 Port = BasePort + ProcessBucket + NextOffset;
		NextOffset = (NextOffset + 1) % BucketSize;
		return FMath::Clamp(Port, BasePort, BasePort + PortWindow - 1);
	}

	bool InvokeGeneratedFunction(FAngelscriptEngine& Engine, UObject* Object, UFunction* Function, void* Params)
	{
		if (!::IsValid(Object) || Function == nullptr)
		{
			return false;
		}

		FAngelscriptEngineScope FunctionScope(Engine, Object);
		if (UASFunction* ScriptFunction = Cast<UASFunction>(Function))
		{
			ScriptFunction->RuntimeCallEvent(Object, Params);
		}
		else
		{
			Object->ProcessEvent(Function, Params);
		}

		return true;
	}
}

FAngelscriptDebuggerTestSession::~FAngelscriptDebuggerTestSession()
{
	Shutdown();
}

bool FAngelscriptDebuggerTestSession::Initialize(const FAngelscriptDebuggerSessionConfig& Config)
{
	Shutdown();

	DefaultTimeoutSeconds = Config.DefaultTimeoutSeconds > 0.0f ? Config.DefaultTimeoutSeconds : 5.0f;
	bResetSeenEnsuresOnShutdown = Config.bResetSeenEnsuresOnShutdown;

	// Mock mode: skip engine/DebugServer entirely. We still honor debug-break
	// and seen-ensures reset flags so mock tests behave consistently with the
	// real path, but adapter-version capture is unnecessary here (the real
	// DebugAdapterVersion global is not mutated in mock mode).
	if (Config.MockServer.IsValid())
	{
		if (Config.bResetSeenEnsuresOnInitialize)
		{
			AngelscriptForgetSeenEnsures();
		}

		if (Config.bDisableDebugBreaks)
		{
			bPreviousDebugBreakStateEnabled = AreAngelscriptDebugBreaksEnabledForTesting();
			bHasCapturedDebugBreakState = true;
			AngelscriptDisableDebugBreaks();
		}

		MockServer = Config.MockServer;
		Port = MockServer->GetPort();
		return true;
	}

	PreviousDebugAdapterVersion = AngelscriptDebugServer::DebugAdapterVersion;
	bHasCapturedDebugAdapterVersion = true;

	if (Config.bResetSeenEnsuresOnInitialize)
	{
		AngelscriptForgetSeenEnsures();
	}

	if (Config.bDisableDebugBreaks)
	{
		bPreviousDebugBreakStateEnabled = AreAngelscriptDebugBreaksEnabledForTesting();
		bHasCapturedDebugBreakState = true;
		AngelscriptDisableDebugBreaks();
	}

	if (Config.ExistingEngine != nullptr)
	{
		Engine = Config.ExistingEngine;
	}
	else
	{
		const bool bExplicitPort = Config.DebugServerPort > 0;
		constexpr int32 MaxAutomaticPortAttempts = 32;
		const int32 AttemptCount = bExplicitPort ? 1 : MaxAutomaticPortAttempts;

		for (int32 Attempt = 0; Attempt < AttemptCount; ++Attempt)
		{
			FAngelscriptEngineConfig EngineConfig;
			EngineConfig.DebugServerPort = bExplicitPort ? Config.DebugServerPort : MakeUniqueDebugServerPort();
			FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
			TUniquePtr<FAngelscriptEngine> CandidateEngine = CreateScriptScanFreeFullEngineForTesting(EngineConfig, Dependencies);

			if (CandidateEngine.IsValid()
				&& CandidateEngine->DebugServer != nullptr
				&& CandidateEngine->DebugServer->IsListening())
			{
				OwnedEngine = MoveTemp(CandidateEngine);
				Engine = OwnedEngine.Get();
				break;
			}
		}
	}

	if (Engine == nullptr)
	{
		Shutdown();
		return false;
	}

	GlobalScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	DebugServer = Engine->DebugServer;
	Port = Engine->GetRuntimeConfig().DebugServerPort;

	if (DebugServer == nullptr || !DebugServer->IsListening())
	{
		Shutdown();
		return false;
	}

	DebugServer->MaxPauseTimeoutSeconds = DefaultTimeoutSeconds;

	return true;
}

void FAngelscriptDebuggerTestSession::Shutdown()
{
	if (bResetSeenEnsuresOnShutdown)
	{
		AngelscriptForgetSeenEnsures();
	}

	if (bHasCapturedDebugBreakState)
	{
		if (bPreviousDebugBreakStateEnabled)
		{
			AngelscriptEnableDebugBreaks();
		}
		else
		{
			AngelscriptDisableDebugBreaks();
		}
		bHasCapturedDebugBreakState = false;
	}

	bPreviousDebugBreakStateEnabled = true;

	if (bHasCapturedDebugAdapterVersion)
	{
		AngelscriptDebugServer::DebugAdapterVersion = PreviousDebugAdapterVersion;
		bHasCapturedDebugAdapterVersion = false;
	}

	PreviousDebugAdapterVersion = 0;

	MockServer.Reset();
	DebugServer = nullptr;
	Port = 0;
	GlobalScope.Reset();
	Engine = nullptr;
	OwnedEngine.Reset();
}

bool FAngelscriptDebuggerTestSession::PumpOneTick()
{
	if (!IsInitialized())
	{
		return false;
	}

	if (IsInGameThread())
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
	}

	if (MockServer.IsValid())
	{
		MockServer->Tick();
	}
	else
	{
		DebugServer->Tick();
	}
	FPlatformProcess::Sleep(0.0f);
	return true;
}

bool FAngelscriptDebuggerTestSession::PumpUntil(TFunctionRef<bool()> Predicate, float TimeoutSeconds)
{
	if (!IsInitialized())
	{
		return false;
	}

	if (Predicate())
	{
		return true;
	}

	const double Timeout = TimeoutSeconds > 0.0f ? TimeoutSeconds : DefaultTimeoutSeconds;
	const double EndTime = FPlatformTime::Seconds() + Timeout;

	while (FPlatformTime::Seconds() < EndTime)
	{
		if (!PumpOneTick())
		{
			return false;
		}

		if (Predicate())
		{
			return true;
		}
	}

	return Predicate();
}

FAngelscriptEngine& FAngelscriptDebuggerTestSession::GetEngine() const
{
	check(Engine != nullptr);
	return *Engine;
}

FAngelscriptDebugServer& FAngelscriptDebuggerTestSession::GetDebugServer() const
{
	check(DebugServer != nullptr);
	return *DebugServer;
}

bool WaitForDebugServerIdle(FAngelscriptDebuggerTestSession& Session, float TimeoutSeconds)
{
	if (!Session.IsInitialized())
	{
		return false;
	}

	// Mock-mode sessions don't have a real FAngelscriptDebugServer to query;
	// treat them as idle when the mock reports no active stop state. Tests
	// driving the mock manage idle/pending state directly via the mock API.
	if (Session.IsMockMode())
	{
		return Session.PumpUntil(
			[&Session]()
			{
				const IAngelscriptDebugServerTestInterface* Mock = Session.GetMockServer();
				return Mock != nullptr && !Mock->IsStopped();
			},
			TimeoutSeconds);
	}

	return Session.PumpUntil(
		[&Session]()
		{
			const FAngelscriptDebugServer& DebugServer = Session.GetDebugServer();
			return !DebugServer.bIsDebugging && DebugServer.BreakpointCount == 0;
		},
		TimeoutSeconds);
}

TSharedRef<FAsyncGeneratedVoidInvocationState> DispatchGeneratedVoidInvocation(
	FAngelscriptEngine& Engine,
	UObject* Object,
	UFunction* Function)
{
	TSharedRef<FAsyncGeneratedVoidInvocationState> State = MakeShared<FAsyncGeneratedVoidInvocationState>();

	AsyncTask(ENamedThreads::GameThread, [&Engine, Object, Function, State]()
	{
		State->bSucceeded = InvokeGeneratedFunction(Engine, Object, Function, nullptr);
		State->bCompleted = true;
	});

	return State;
}

TSharedRef<FAsyncGeneratedVoidInvocationState> DispatchGeneratedVoidInvocation(
	FAngelscriptEngine& Engine,
	UObject* Object,
	UFunction* Function,
	bool Condition,
	const FString& Message)
{
	TSharedRef<FAsyncGeneratedVoidInvocationState> State = MakeShared<FAsyncGeneratedVoidInvocationState>();

	AsyncTask(ENamedThreads::GameThread, [&Engine, Object, Function, Condition, Message, State]()
	{
		FGeneratedVoidInvocationParams Params;
		Params.Condition = Condition;
		Params.Message = Message;
		State->bSucceeded = InvokeGeneratedFunction(Engine, Object, Function, &Params);
		State->bCompleted = true;
	});

	return State;
}

TSharedRef<FAsyncGeneratedBoolInvocationState> DispatchGeneratedBoolInvocation(
	FAngelscriptEngine& Engine,
	UObject* Object,
	UFunction* Function,
	bool Condition,
	const FString& Message)
{
	TSharedRef<FAsyncGeneratedBoolInvocationState> State = MakeShared<FAsyncGeneratedBoolInvocationState>();

	AsyncTask(ENamedThreads::GameThread, [&Engine, Object, Function, Condition, Message, State]()
	{
		FGeneratedBoolInvocationParams Params;
		Params.Condition = Condition;
		Params.Message = Message;
		State->bSucceeded = InvokeGeneratedFunction(Engine, Object, Function, &Params);
		State->bReturnValue = Params.ReturnValue;
		State->bCompleted = true;
	});

	return State;
}

TUniquePtr<FAngelscriptDebuggerTestSession> CreateMockDebuggerSession(
	TSharedPtr<FAngelscriptMockDebugServer>& OutMock,
	const FAngelscriptDebuggerSessionConfig& BaseConfig)
{
	TSharedPtr<FAngelscriptMockDebugServer> Mock = MakeShared<FAngelscriptMockDebugServer>();
	FAngelscriptDebuggerSessionConfig Config = BaseConfig;
	Config.MockServer = Mock;

	TUniquePtr<FAngelscriptDebuggerTestSession> Session = MakeUnique<FAngelscriptDebuggerTestSession>();
	if (!Session->Initialize(Config))
	{
		return nullptr;
	}

	OutMock = Mock;
	return Session;
}

