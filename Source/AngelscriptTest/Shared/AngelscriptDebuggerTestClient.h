#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Sockets.h"

class IDebuggerTestSocket
{
public:
	virtual ~IDebuggerTestSocket() = default;

	virtual bool SetNonBlocking(bool bIsNonBlocking) = 0;
	virtual bool SetNoDelay(bool bIsNoDelay) = 0;
	virtual bool Connect(const FIPv4Address& Address, int32 Port) = 0;
	virtual ESocketErrors GetLastErrorCode() const = 0;
	virtual bool Close() = 0;
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) = 0;
	virtual bool Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) = 0;
	virtual bool HasPendingData(uint32& PendingDataSize) = 0;
	virtual ESocketConnectionState GetConnectionState() const = 0;
	virtual FString GetDescription() const = 0;
};

class IDebuggerTestSocketFactory
{
public:
	virtual ~IDebuggerTestSocketFactory() = default;
	virtual TSharedPtr<IDebuggerTestSocket> CreateSocket(const FString& Description) = 0;
};

class FFakeDebuggerClientSocket final : public IDebuggerTestSocket
{
public:
	explicit FFakeDebuggerClientSocket(const FString& InDescription = TEXT("FakeDebuggerClientSocket"));

	void SetConnectResult(bool bInReturnValue, ESocketErrors InErrorCode);
	void SetConnectionStates(const TArray<ESocketConnectionState>& InConnectionStates);
	void SetSteadyConnectionState(ESocketConnectionState InConnectionState);

	bool WasClosed() const
	{
		return bWasClosed;
	}

	virtual bool SetNonBlocking(bool bIsNonBlocking) override;
	virtual bool SetNoDelay(bool bIsNoDelay) override;
	virtual bool Connect(const FIPv4Address& Address, int32 Port) override;
	virtual ESocketErrors GetLastErrorCode() const override;
	virtual bool Close() override;
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) override;
	virtual bool Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) override;
	virtual bool HasPendingData(uint32& PendingDataSize) override;
	virtual ESocketConnectionState GetConnectionState() const override;
	virtual FString GetDescription() const override;

private:
	FString Description;
	bool bConnectReturnValue = false;
	ESocketErrors ConnectErrorCode = SE_NO_ERROR;
	mutable int32 ConnectionStateReadIndex = 0;
	TArray<ESocketConnectionState> ConnectionStates;
	ESocketConnectionState SteadyConnectionState = ESocketConnectionState::SCS_NotConnected;
	bool bWasClosed = false;
};

class FSingleDebuggerTestSocketFactory final : public IDebuggerTestSocketFactory
{
public:
	template <typename SocketType>
	explicit FSingleDebuggerTestSocketFactory(const TSharedRef<SocketType>& InSocket)
		: Socket(StaticCastSharedRef<IDebuggerTestSocket>(InSocket))
	{
	}

	virtual TSharedPtr<IDebuggerTestSocket> CreateSocket(const FString& Description) override;

private:
	TSharedPtr<IDebuggerTestSocket> Socket;
};

class FAngelscriptDebuggerTestClient
{
public:
	FAngelscriptDebuggerTestClient() = default;
	explicit FAngelscriptDebuggerTestClient(TSharedRef<IDebuggerTestSocketFactory> InSocketFactory)
		: SocketFactory(InSocketFactory)
	{
	}

	~FAngelscriptDebuggerTestClient();

	bool Connect(const FString& Host, int32 Port);
	bool Connect(const FString& Host, int32 Port, float TimeoutSeconds);
	void Disconnect();

	bool IsConnected() const;
	const FString& GetLastError() const
	{
		return LastError;
	}

	bool SendRawEnvelope(EDebugMessageType MessageType, const TArray<uint8>& Body);

	template <typename T>
	bool SendTypedMessage(EDebugMessageType MessageType, T Message)
	{
		TArray<uint8> Body;
		FMemoryWriter Writer(Body);
		Writer << Message;
		return SendRawEnvelope(MessageType, Body);
	}

	TOptional<FAngelscriptDebugMessageEnvelope> ReceiveEnvelope();
	TOptional<FAngelscriptDebugMessageEnvelope> WaitForMessage(float TimeoutSeconds);
	TOptional<FAngelscriptDebugMessageEnvelope> WaitForMessageType(EDebugMessageType ExpectedType, float TimeoutSeconds);
	bool CollectMessagesUntil(EDebugMessageType TerminalType, float TimeoutSeconds, TArray<FAngelscriptDebugMessageEnvelope>& OutMessages);
	TArray<FAngelscriptDebugMessageEnvelope> DrainPendingMessages();

	template <typename T>
	static TOptional<T> DeserializeMessage(const FAngelscriptDebugMessageEnvelope& Envelope)
	{
		T Value;
		TArray<uint8> Body = Envelope.Body;
		FMemoryReader Reader(Body);
		Reader << Value;
		if (Reader.IsError())
		{
			return {};
		}
		return Value;
	}

	template <typename T>
	TOptional<T> WaitForTypedMessage(EDebugMessageType ExpectedType, float TimeoutSeconds)
	{
		TOptional<FAngelscriptDebugMessageEnvelope> Envelope = WaitForMessageType(ExpectedType, TimeoutSeconds);
		if (!Envelope.IsSet())
		{
			return {};
		}
		return DeserializeMessage<T>(Envelope.GetValue());
	}

	bool SendStartDebugging(int32 AdapterVersion);
	bool SendPause();
	bool SendContinue();
	bool SendStopDebugging();
	bool SendDisconnect();
	bool SendStepIn();
	bool SendStepOver();
	bool SendStepOut();
	bool SendRequestCallStack();
	bool SendBreakOptions(const TArray<FString>& Filters);
	bool SendRequestBreakFilters();
	bool SendRequestDebugDatabase();
	bool SendRequestVariables(const FString& ScopePath);
	bool SendRequestEvaluate(const FString& Path, int32 DefaultFrame = 0);
	bool SendSetBreakpoint(const FAngelscriptBreakpoint& Breakpoint);
	bool SendClearBreakpoints(const FAngelscriptClearBreakpoints& Breakpoints);

private:
	bool AppendReceivedData();
	bool ConsumeEnvelope(FAngelscriptDebugMessageEnvelope& OutEnvelope, bool& bOutHasEnvelope);
	bool SetError(const FString& ErrorMessage);

	TSharedPtr<IDebuggerTestSocketFactory> SocketFactory;
	TSharedPtr<IDebuggerTestSocket> Socket;
	TArray<uint8> ReceiveBuffer;
	TArray<FAngelscriptDebugMessageEnvelope> PendingMessages;
	FString LastError;
};

enum class ESingleClientDebuggerCommand : uint8
{
	Continue,
	StepIn,
	StepOver,
	StepOut
};

struct FSingleClientDebuggerStopAction
{
	bool bRequestCallStack = true;
	ESingleClientDebuggerCommand Command = ESingleClientDebuggerCommand::Continue;
};

struct FSingleClientDebuggerWorkerConfig
{
	int32 AdapterVersion = 2;
	float TimeoutSeconds = 45.0f;
	float PostTerminalDrainSeconds = 0.05f;
	TArray<FAngelscriptBreakpoint> InitialBreakpoints;
	TArray<FSingleClientDebuggerStopAction> StopActions;
};

struct FSingleClientDebuggerTranscript
{
	TArray<FAngelscriptDebugMessageEnvelope> ReceivedMessages;
	TOptional<FDebugServerVersionMessage> DebugServerVersion;
	TArray<FStoppedMessage> StopMessages;
	TArray<FAngelscriptCallStack> CallStacks;
	int32 HasContinuedCount = 0;
	bool bTimedOut = false;
	FString Error;
};

TFuture<FSingleClientDebuggerTranscript> RunSingleClientDebuggerWorker(
	int32 Port,
	TAtomic<bool>& bWorkerReady,
	TAtomic<bool>& bShouldStop,
	const FSingleClientDebuggerWorkerConfig& Config = FSingleClientDebuggerWorkerConfig());

