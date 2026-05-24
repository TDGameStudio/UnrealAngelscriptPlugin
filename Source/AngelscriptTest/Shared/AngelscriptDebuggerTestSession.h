#pragma once

#include "Binds/Bind_Debugging.h"
#include "CoreMinimal.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

#include "AngelscriptMockDebugServer.h"
#include "AngelscriptTestUtilities.h"

class FAngelscriptDebugServer;
class UObject;
class UFunction;

constexpr float kDefaultDebuggerTestTimeoutSeconds = 45.0f;
struct FAngelscriptDebuggerSessionConfig
{
	FAngelscriptEngine* ExistingEngine = nullptr;
	int32 DebugServerPort = 0;
	float DefaultTimeoutSeconds = 5.0f;
	bool bDisableDebugBreaks = false;
	bool bResetSeenEnsuresOnInitialize = true;
	bool bResetSeenEnsuresOnShutdown = true;

	// If set, Initialize() skips real engine/DebugServer creation and puts the
	// session into mock mode: PumpOneTick() ticks the mock, and GetMockServer()
	// becomes the primary handle tests use. ExistingEngine / DebugServerPort
	// are ignored in this mode. See AngelscriptMockDebugServer.h.
	TSharedPtr<IAngelscriptDebugServerTestInterface> MockServer;
};

struct FScopedDebugAdapterVersionSentinel
{
	explicit FScopedDebugAdapterVersionSentinel(int32 InSentinelValue)
		: OriginalValue(AngelscriptDebugServer::DebugAdapterVersion)
	{
		SetSentinel(InSentinelValue);
	}

	~FScopedDebugAdapterVersionSentinel()
	{
		AngelscriptDebugServer::DebugAdapterVersion = OriginalValue;
	}

	void SetSentinel(int32 InSentinelValue)
	{
		SentinelValue = InSentinelValue;
		AngelscriptDebugServer::DebugAdapterVersion = SentinelValue;
	}

	int32 GetCurrent() const
	{
		return AngelscriptDebugServer::DebugAdapterVersion;
	}

	int32 GetOriginal() const
	{
		return OriginalValue;
	}

	int32 GetSentinel() const
	{
		return SentinelValue;
	}

private:
	int32 OriginalValue = 0;
	int32 SentinelValue = 0;
};

struct FScopedDebugBreakStateSentinel
{
	FScopedDebugBreakStateSentinel()
		: bOriginalValueEnabled(AreAngelscriptDebugBreaksEnabledForTesting())
	{
	}

	~FScopedDebugBreakStateSentinel()
	{
		SetEnabled(bOriginalValueEnabled);
	}

	void SetEnabled(const bool bEnabled) const
	{
		if (bEnabled)
		{
			AngelscriptEnableDebugBreaks();
		}
		else
		{
			AngelscriptDisableDebugBreaks();
		}
	}

	bool IsEnabled() const
	{
		return AreAngelscriptDebugBreaksEnabledForTesting();
	}

	bool GetOriginalValue() const
	{
		return bOriginalValueEnabled;
	}

private:
	bool bOriginalValueEnabled = true;
};

class FAngelscriptDebuggerTestSession
{
public:
	FAngelscriptDebuggerTestSession() = default;
	~FAngelscriptDebuggerTestSession();

	bool Initialize(const FAngelscriptDebuggerSessionConfig& Config = FAngelscriptDebuggerSessionConfig());
	void Shutdown();

	bool IsInitialized() const
	{
		return (Engine != nullptr && DebugServer != nullptr) || MockServer.IsValid();
	}

	// True when the session was initialized with a MockServer config. In this
	// mode Engine/DebugServer are null and GetDebugServer() must NOT be called.
	bool IsMockMode() const
	{
		return MockServer.IsValid();
	}

	bool PumpOneTick();
	bool PumpUntil(TFunctionRef<bool()> Predicate, float TimeoutSeconds = 0.0f);

	FAngelscriptEngine& GetEngine() const;
	FAngelscriptDebugServer& GetDebugServer() const;

	// Mock-mode accessors. Return null in real-engine mode.
	IAngelscriptDebugServerTestInterface* GetMockServer() const
	{
		return MockServer.Get();
	}
	TSharedPtr<IAngelscriptDebugServerTestInterface> GetMockServerShared() const
	{
		return MockServer;
	}

	int32 GetPort() const
	{
		if (MockServer.IsValid())
		{
			return MockServer->GetPort();
		}
		return Port;
	}

	float GetDefaultTimeoutSeconds() const
	{
		return DefaultTimeoutSeconds;
	}

private:
	TUniquePtr<FAngelscriptEngine> OwnedEngine;
	FAngelscriptEngine* Engine = nullptr;
	FAngelscriptDebugServer* DebugServer = nullptr;
	TUniquePtr<FAngelscriptEngineScope> GlobalScope;

	TSharedPtr<IAngelscriptDebugServerTestInterface> MockServer;

	int32 Port = 0;
	float DefaultTimeoutSeconds = 5.0f;
	bool bResetSeenEnsuresOnShutdown = true;
	bool bHasCapturedDebugBreakState = false;
	bool bPreviousDebugBreakStateEnabled = true;
	bool bHasCapturedDebugAdapterVersion = false;
	int32 PreviousDebugAdapterVersion = 0;
};

struct FAsyncGeneratedVoidInvocationState : public TSharedFromThis<FAsyncGeneratedVoidInvocationState>
{
	TAtomic<bool> bCompleted = false;
	bool bSucceeded = false;
};

struct FAsyncGeneratedBoolInvocationState : public TSharedFromThis<FAsyncGeneratedBoolInvocationState>
{
	TAtomic<bool> bCompleted = false;
	bool bSucceeded = false;
	bool bReturnValue = false;
};

TSharedRef<FAsyncGeneratedVoidInvocationState> DispatchGeneratedVoidInvocation(
	FAngelscriptEngine& Engine,
	UObject* Object,
	UFunction* Function);

TSharedRef<FAsyncGeneratedVoidInvocationState> DispatchGeneratedVoidInvocation(
	FAngelscriptEngine& Engine,
	UObject* Object,
	UFunction* Function,
	bool Condition,
	const FString& Message);

TSharedRef<FAsyncGeneratedBoolInvocationState> DispatchGeneratedBoolInvocation(
	FAngelscriptEngine& Engine,
	UObject* Object,
	UFunction* Function,
	bool Condition,
	const FString& Message);

bool WaitForDebugServerIdle(
	FAngelscriptDebuggerTestSession& Session,
	float TimeoutSeconds = 0.0f);

// Build a ready-to-use mock-mode session plus a fresh FAngelscriptMockDebugServer.
// Returns nullptr on initialize failure. Caller receives a unique_ptr that owns
// the session; the mock is accessible via Session->GetMockServer() and is also
// returned via OutMock for direct introspection.
//
// Keeping this helper in the session header (not AngelscriptTestUtilities.h)
// avoids circular includes: the utilities header is included by the session
// header, and the mock types already require the session type for full wiring.
TUniquePtr<FAngelscriptDebuggerTestSession> CreateMockDebuggerSession(
	TSharedPtr<FAngelscriptMockDebugServer>& OutMock,
	const FAngelscriptDebuggerSessionConfig& BaseConfig = FAngelscriptDebuggerSessionConfig());

