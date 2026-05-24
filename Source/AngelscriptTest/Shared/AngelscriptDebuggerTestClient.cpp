#include "Shared/AngelscriptDebuggerTestClient.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "IPAddress.h"
#include "Misc/ScopeExit.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace
{
	constexpr float DefaultDebuggerConnectTimeoutSeconds = 5.0f;
	constexpr int32 DebuggerEnvelopeHeaderSize = sizeof(int32) + sizeof(uint8);

	bool ParseHostAddress(const FString& Host, FIPv4Address& OutAddress)
	{
		if (Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase))
		{
			OutAddress = FIPv4Address(127, 0, 0, 1);
			return true;
		}

		return FIPv4Address::Parse(Host, OutAddress);
	}

	const TCHAR* SocketConnectionStateToString(const ESocketConnectionState ConnectionState)
	{
		switch (ConnectionState)
		{
		case ESocketConnectionState::SCS_NotConnected:
			return TEXT("SCS_NotConnected");
		case ESocketConnectionState::SCS_Connected:
			return TEXT("SCS_Connected");
		case ESocketConnectionState::SCS_ConnectionError:
			return TEXT("SCS_ConnectionError");
		default:
			return TEXT("SCS_Unknown");
		}
	}

	bool RecordSingleClientEnvelope(
		FSingleClientDebuggerTranscript& Transcript,
		const FAngelscriptDebugMessageEnvelope& Envelope,
		FString& OutError)
	{
		Transcript.ReceivedMessages.Add(Envelope);

		switch (Envelope.MessageType)
		{
		case EDebugMessageType::DebugServerVersion:
			if (!Transcript.DebugServerVersion.IsSet())
			{
				const TOptional<FDebugServerVersionMessage> Version = FAngelscriptDebuggerTestClient::DeserializeMessage<FDebugServerVersionMessage>(Envelope);
				if (!Version.IsSet())
				{
					OutError = TEXT("Single-client debugger worker failed to deserialize DebugServerVersion.");
					return false;
				}

				Transcript.DebugServerVersion = Version;
			}
			break;

		case EDebugMessageType::HasStopped:
			{
				const TOptional<FStoppedMessage> StopMessage = FAngelscriptDebuggerTestClient::DeserializeMessage<FStoppedMessage>(Envelope);
				if (!StopMessage.IsSet())
				{
					OutError = TEXT("Single-client debugger worker failed to deserialize HasStopped.");
					return false;
				}

				Transcript.StopMessages.Add(StopMessage.GetValue());
			}
			break;

		case EDebugMessageType::CallStack:
			{
				const TOptional<FAngelscriptCallStack> CallStack = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptCallStack>(Envelope);
				if (!CallStack.IsSet())
				{
					OutError = TEXT("Single-client debugger worker failed to deserialize CallStack.");
					return false;
				}

				Transcript.CallStacks.Add(CallStack.GetValue());
			}
			break;

		case EDebugMessageType::HasContinued:
			++Transcript.HasContinuedCount;
			break;

		default:
			break;
		}

		return true;
	}

	bool WaitForSingleClientMessageType(
		FAngelscriptDebuggerTestClient& Client,
		TAtomic<bool>& bShouldStop,
		const EDebugMessageType ExpectedType,
		const float TimeoutSeconds,
		FSingleClientDebuggerTranscript& Transcript,
		FString& OutError)
	{
		const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
		while (FPlatformTime::Seconds() < EndTime)
		{
			if (bShouldStop.Load())
			{
				OutError = FString::Printf(TEXT("Single-client debugger worker aborted while waiting for message type %d."), static_cast<int32>(ExpectedType));
				return false;
			}

			const TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
			if (Envelope.IsSet())
			{
				if (!RecordSingleClientEnvelope(Transcript, Envelope.GetValue(), OutError))
				{
					return false;
				}

				if (Envelope->MessageType == ExpectedType)
				{
					return true;
				}
			}
			else if (!Client.GetLastError().IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("Single-client debugger worker failed while waiting for message type %d: %s"),
					static_cast<int32>(ExpectedType),
					*Client.GetLastError());
				return false;
			}

			FPlatformProcess::Sleep(0.001f);
		}

		Transcript.bTimedOut = true;
		OutError = FString::Printf(
			TEXT("Single-client debugger worker timed out after %.3f seconds waiting for message type %d."),
			TimeoutSeconds,
			static_cast<int32>(ExpectedType));
		return false;
	}

	bool DrainSingleClientMessages(
		FAngelscriptDebuggerTestClient& Client,
		TAtomic<bool>& bShouldStop,
		const float DurationSeconds,
		FSingleClientDebuggerTranscript& Transcript,
		FString& OutError)
	{
		if (DurationSeconds <= 0.0f)
		{
			return true;
		}

		const double EndTime = FPlatformTime::Seconds() + DurationSeconds;
		while (FPlatformTime::Seconds() < EndTime)
		{
			if (bShouldStop.Load())
			{
				return true;
			}

			const TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
			if (Envelope.IsSet())
			{
				if (!RecordSingleClientEnvelope(Transcript, Envelope.GetValue(), OutError))
				{
					return false;
				}
			}
			else if (!Client.GetLastError().IsEmpty())
			{
				OutError = FString::Printf(TEXT("Single-client debugger worker failed while draining post-terminal messages: %s"), *Client.GetLastError());
				return false;
			}
			else
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}

		return true;
	}

	bool SendSingleClientCommand(
		FAngelscriptDebuggerTestClient& Client,
		const ESingleClientDebuggerCommand Command,
		FString& OutError)
	{
		bool bSent = false;
		switch (Command)
		{
		case ESingleClientDebuggerCommand::Continue:
			bSent = Client.SendContinue();
			break;

		case ESingleClientDebuggerCommand::StepIn:
			bSent = Client.SendStepIn();
			break;

		case ESingleClientDebuggerCommand::StepOver:
			bSent = Client.SendStepOver();
			break;

		case ESingleClientDebuggerCommand::StepOut:
			bSent = Client.SendStepOut();
			break;

		default:
			OutError = TEXT("Single-client debugger worker received an unknown command.");
			return false;
		}

		if (!bSent)
		{
			OutError = FString::Printf(TEXT("Single-client debugger worker failed to send command: %s"), *Client.GetLastError());
		}
		return bSent;
	}

	class FPlatformDebuggerTestSocket final : public IDebuggerTestSocket
	{
	public:
		FPlatformDebuggerTestSocket(FSocket* InSocket, ISocketSubsystem* InSocketSubsystem)
			: Socket(InSocket)
			, SocketSubsystem(InSocketSubsystem)
		{
		}

		virtual ~FPlatformDebuggerTestSocket() override
		{
			Close();
		}

		virtual bool SetNonBlocking(bool bIsNonBlocking) override
		{
			return Socket != nullptr && Socket->SetNonBlocking(bIsNonBlocking);
		}

		virtual bool SetNoDelay(bool bIsNoDelay) override
		{
			return Socket != nullptr && Socket->SetNoDelay(bIsNoDelay);
		}

		virtual bool Connect(const FIPv4Address& Address, int32 Port) override
		{
			if (Socket == nullptr || SocketSubsystem == nullptr)
			{
				LastErrorCode = SE_NO_ERROR;
				return false;
			}

			const TSharedRef<FInternetAddr> InternetAddr = SocketSubsystem->CreateInternetAddr();
			InternetAddr->SetIp(Address.Value);
			InternetAddr->SetPort(Port);

			const bool bConnected = Socket->Connect(*InternetAddr);
			LastErrorCode = SocketSubsystem->GetLastErrorCode();
			return bConnected;
		}

		virtual ESocketErrors GetLastErrorCode() const override
		{
			return LastErrorCode;
		}

		virtual bool Close() override
		{
			if (Socket == nullptr)
			{
				return true;
			}

			Socket->Close();
			if (SocketSubsystem != nullptr)
			{
				SocketSubsystem->DestroySocket(Socket);
			}

			Socket = nullptr;
			return true;
		}

		virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) override
		{
			return Socket != nullptr && Socket->Send(Data, Count, BytesSent);
		}

		virtual bool Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) override
		{
			return Socket != nullptr && Socket->Recv(Data, BufferSize, BytesRead, Flags);
		}

		virtual bool HasPendingData(uint32& PendingDataSize) override
		{
			return Socket != nullptr && Socket->HasPendingData(PendingDataSize);
		}

		virtual ESocketConnectionState GetConnectionState() const override
		{
			return Socket != nullptr ? Socket->GetConnectionState() : ESocketConnectionState::SCS_NotConnected;
		}

		virtual FString GetDescription() const override
		{
			return Socket != nullptr ? Socket->GetDescription() : TEXT("DestroyedDebuggerTestSocket");
		}

	private:
		FSocket* Socket = nullptr;
		ISocketSubsystem* SocketSubsystem = nullptr;
		ESocketErrors LastErrorCode = SE_NO_ERROR;
	};

	class FPlatformDebuggerTestSocketFactory final : public IDebuggerTestSocketFactory
	{
	public:
		virtual TSharedPtr<IDebuggerTestSocket> CreateSocket(const FString& Description) override
		{
			ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (SocketSubsystem == nullptr)
			{
				return nullptr;
			}

			FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, Description, false);
			if (Socket == nullptr)
			{
				return nullptr;
			}

			return MakeShared<FPlatformDebuggerTestSocket>(Socket, SocketSubsystem);
		}
	};
}

FFakeDebuggerClientSocket::FFakeDebuggerClientSocket(const FString& InDescription)
	: Description(InDescription)
{
}

void FFakeDebuggerClientSocket::SetConnectResult(bool bInReturnValue, ESocketErrors InErrorCode)
{
	bConnectReturnValue = bInReturnValue;
	ConnectErrorCode = InErrorCode;
}

void FFakeDebuggerClientSocket::SetConnectionStates(const TArray<ESocketConnectionState>& InConnectionStates)
{
	ConnectionStates = InConnectionStates;
	ConnectionStateReadIndex = 0;
	if (ConnectionStates.Num() > 0)
	{
		SteadyConnectionState = ConnectionStates.Last();
	}
}

void FFakeDebuggerClientSocket::SetSteadyConnectionState(ESocketConnectionState InConnectionState)
{
	SteadyConnectionState = InConnectionState;
}

bool FFakeDebuggerClientSocket::SetNonBlocking(bool bIsNonBlocking)
{
	return true;
}

bool FFakeDebuggerClientSocket::SetNoDelay(bool bIsNoDelay)
{
	return true;
}

bool FFakeDebuggerClientSocket::Connect(const FIPv4Address& Address, int32 Port)
{
	ConnectionStateReadIndex = 0;
	bWasClosed = false;
	return bConnectReturnValue;
}

ESocketErrors FFakeDebuggerClientSocket::GetLastErrorCode() const
{
	return ConnectErrorCode;
}

bool FFakeDebuggerClientSocket::Close()
{
	bWasClosed = true;
	return true;
}

bool FFakeDebuggerClientSocket::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	BytesSent = 0;
	return false;
}

bool FFakeDebuggerClientSocket::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	BytesRead = 0;
	return false;
}

bool FFakeDebuggerClientSocket::HasPendingData(uint32& PendingDataSize)
{
	PendingDataSize = 0;
	return false;
}

ESocketConnectionState FFakeDebuggerClientSocket::GetConnectionState() const
{
	if (bWasClosed)
	{
		return ESocketConnectionState::SCS_NotConnected;
	}

	if (ConnectionStates.IsValidIndex(ConnectionStateReadIndex))
	{
		return ConnectionStates[ConnectionStateReadIndex++];
	}

	return SteadyConnectionState;
}

FString FFakeDebuggerClientSocket::GetDescription() const
{
	return Description;
}

TSharedPtr<IDebuggerTestSocket> FSingleDebuggerTestSocketFactory::CreateSocket(const FString& Description)
{
	return Socket;
}

FAngelscriptDebuggerTestClient::~FAngelscriptDebuggerTestClient()
{
	Disconnect();
}

bool FAngelscriptDebuggerTestClient::Connect(const FString& Host, int32 Port)
{
	return Connect(Host, Port, DefaultDebuggerConnectTimeoutSeconds);
}

bool FAngelscriptDebuggerTestClient::Connect(const FString& Host, int32 Port, float TimeoutSeconds)
{
	Disconnect();

	FIPv4Address Address;
	if (!ParseHostAddress(Host, Address))
	{
		return SetError(FString::Printf(TEXT("Failed to parse IPv4 host '%s' for debugger test client."), *Host));
	}

	TSharedPtr<IDebuggerTestSocketFactory> ActiveSocketFactory = SocketFactory;
	if (!ActiveSocketFactory.IsValid())
	{
		ActiveSocketFactory = MakeShared<FPlatformDebuggerTestSocketFactory>();
	}

	if (!SocketFactory.IsValid() && ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM) == nullptr)
	{
		return SetError(TEXT("Socket subsystem is unavailable."));
	}

	Socket = ActiveSocketFactory->CreateSocket(TEXT("AngelscriptDebuggerTestClient"));
	if (!Socket.IsValid())
	{
		return SetError(TEXT("Failed to create debugger test client socket."));
	}

	Socket->SetNonBlocking(true);
	Socket->SetNoDelay(true);

	if (!Socket->Connect(Address, Port))
	{
		ESocketErrors ErrorCode = Socket->GetLastErrorCode();
		if (ErrorCode != SE_NO_ERROR && ErrorCode != SE_EWOULDBLOCK && ErrorCode != SE_EINPROGRESS)
		{
			Disconnect();
			return SetError(FString::Printf(TEXT("Failed to connect debugger test client to %s:%d (socket error %d)."), *Host, Port, static_cast<int32>(ErrorCode)));
		}

		const double EffectiveTimeoutSeconds = TimeoutSeconds > 0.0f ? TimeoutSeconds : DefaultDebuggerConnectTimeoutSeconds;
		const double EndTime = FPlatformTime::Seconds() + EffectiveTimeoutSeconds;
		ESocketConnectionState LastConnectionState = Socket->GetConnectionState();
		while (FPlatformTime::Seconds() < EndTime)
		{
			LastConnectionState = Socket->GetConnectionState();
			if (LastConnectionState == ESocketConnectionState::SCS_Connected)
			{
				ReceiveBuffer.Reset();
				PendingMessages.Reset();
				LastError.Reset();
				return true;
			}

			FPlatformProcess::Sleep(0.0f);
		}

		Disconnect();
		return SetError(FString::Printf(
			TEXT("Timed out after %.3f seconds connecting debugger test client to %s:%d (last connection state %s)."),
			EffectiveTimeoutSeconds,
			*Host,
			Port,
			SocketConnectionStateToString(LastConnectionState)));
	}

	ReceiveBuffer.Reset();
	PendingMessages.Reset();
	LastError.Reset();
	return true;
}

void FAngelscriptDebuggerTestClient::Disconnect()
{
	if (Socket.IsValid())
	{
		Socket->Close();
		Socket.Reset();
	}

	ReceiveBuffer.Reset();
	PendingMessages.Reset();
}

bool FAngelscriptDebuggerTestClient::IsConnected() const
{
	return Socket.IsValid() && Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected;
}

bool FAngelscriptDebuggerTestClient::SendRawEnvelope(EDebugMessageType MessageType, const TArray<uint8>& Body)
{
	if (!Socket.IsValid())
	{
		return SetError(TEXT("Cannot send debugger message without an active socket connection."));
	}

	TArray<uint8> Buffer;
	if (!SerializeDebugMessageEnvelope(MessageType, Body, Buffer))
	{
		return SetError(FString::Printf(TEXT("Failed to serialize debugger message type %d."), static_cast<int32>(MessageType)));
	}

	int32 TotalSent = 0;
	while (TotalSent < Buffer.Num())
	{
		int32 BytesSent = 0;
		if (!Socket->Send(Buffer.GetData() + TotalSent, Buffer.Num() - TotalSent, BytesSent))
		{
			return SetError(FString::Printf(TEXT("Failed to send debugger message type %d after %d/%d bytes."), static_cast<int32>(MessageType), TotalSent, Buffer.Num()));
		}

		if (BytesSent <= 0)
		{
			return SetError(FString::Printf(TEXT("Debugger message type %d did not send any bytes."), static_cast<int32>(MessageType)));
		}

		TotalSent += BytesSent;
	}

	LastError.Reset();
	return true;
}

TOptional<FAngelscriptDebugMessageEnvelope> FAngelscriptDebuggerTestClient::ReceiveEnvelope()
{
	if (PendingMessages.Num() > 0)
	{
		FAngelscriptDebugMessageEnvelope Envelope = MoveTemp(PendingMessages[0]);
		PendingMessages.RemoveAt(0, 1, EAllowShrinking::No);
		LastError.Reset();
		return Envelope;
	}

	if (!Socket.IsValid())
	{
		SetError(TEXT("Cannot receive debugger messages without an active socket connection."));
		return {};
	}

	if (!AppendReceivedData())
	{
		return {};
	}

	FAngelscriptDebugMessageEnvelope Envelope;
	bool bHasEnvelope = false;
	if (!ConsumeEnvelope(Envelope, bHasEnvelope))
	{
		return {};
	}

	if (!bHasEnvelope)
	{
		LastError.Reset();
		return {};
	}

	LastError.Reset();
	return Envelope;
}

TOptional<FAngelscriptDebugMessageEnvelope> FAngelscriptDebuggerTestClient::WaitForMessage(float TimeoutSeconds)
{
	const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
	while (FPlatformTime::Seconds() < EndTime)
	{
		TOptional<FAngelscriptDebugMessageEnvelope> Envelope = ReceiveEnvelope();
		if (Envelope.IsSet())
		{
			return Envelope;
		}

		if (!LastError.IsEmpty())
		{
			return {};
		}

		FPlatformProcess::Sleep(0.0f);
	}

	SetError(FString::Printf(TEXT("Timed out after %.3f seconds waiting for any debugger message."), TimeoutSeconds));
	return {};
}

TOptional<FAngelscriptDebugMessageEnvelope> FAngelscriptDebuggerTestClient::WaitForMessageType(EDebugMessageType ExpectedType, float TimeoutSeconds)
{
	const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
	EDebugMessageType LastMessageType = EDebugMessageType::Disconnect;
	bool bSawUnexpectedMessage = false;

	while (FPlatformTime::Seconds() < EndTime)
	{
		for (int32 Index = 0; Index < PendingMessages.Num(); ++Index)
		{
			if (PendingMessages[Index].MessageType == ExpectedType)
			{
				FAngelscriptDebugMessageEnvelope Envelope = MoveTemp(PendingMessages[Index]);
				PendingMessages.RemoveAt(Index, 1, EAllowShrinking::No);
				LastError.Reset();
				return Envelope;
			}

			bSawUnexpectedMessage = true;
			LastMessageType = PendingMessages[Index].MessageType;
		}

		if (!AppendReceivedData())
		{
			return {};
		}

		while (true)
		{
			FAngelscriptDebugMessageEnvelope Envelope;
			bool bHasEnvelope = false;
			if (!ConsumeEnvelope(Envelope, bHasEnvelope))
			{
				return {};
			}

			if (!bHasEnvelope)
			{
				break;
			}

			if (Envelope.MessageType == ExpectedType)
			{
				LastError.Reset();
				return Envelope;
			}

			bSawUnexpectedMessage = true;
			LastMessageType = Envelope.MessageType;
			PendingMessages.Add(MoveTemp(Envelope));
		}

		if (!LastError.IsEmpty())
		{
			return {};
		}

		FPlatformProcess::Sleep(0.0f);
	}

	if (bSawUnexpectedMessage)
	{
		SetError(FString::Printf(TEXT("Timed out after %.3f seconds waiting for debugger message type %d. Last received type was %d."), TimeoutSeconds, static_cast<int32>(ExpectedType), static_cast<int32>(LastMessageType)));
	}
	else
	{
		SetError(FString::Printf(TEXT("Timed out after %.3f seconds waiting for debugger message type %d without receiving any complete envelopes."), TimeoutSeconds, static_cast<int32>(ExpectedType)));
	}

	return {};
}

bool FAngelscriptDebuggerTestClient::CollectMessagesUntil(
	EDebugMessageType TerminalType,
	float TimeoutSeconds,
	TArray<FAngelscriptDebugMessageEnvelope>& OutMessages)
{
	for (const FAngelscriptDebugMessageEnvelope& Envelope : OutMessages)
	{
		if (Envelope.MessageType == TerminalType)
		{
			LastError.Reset();
			return true;
		}
	}

	auto ConsumeAvailableMessages =
		[this, TerminalType, &OutMessages]() -> TOptional<bool>
		{
			while (true)
			{
				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = ReceiveEnvelope();
				if (!Envelope.IsSet())
				{
					break;
				}

				OutMessages.Add(Envelope.GetValue());
				if (Envelope->MessageType == TerminalType)
				{
					LastError.Reset();
					return true;
				}
			}

			if (!LastError.IsEmpty())
			{
				return false;
			}

			return {};
		};

	if (TimeoutSeconds <= 0.0f)
	{
		const TOptional<bool> Result = ConsumeAvailableMessages();
		return Result.IsSet() ? Result.GetValue() : false;
	}

	const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;
	while (FPlatformTime::Seconds() < EndTime)
	{
		const TOptional<bool> Result = ConsumeAvailableMessages();
		if (Result.IsSet())
		{
			return Result.GetValue();
		}

		FPlatformProcess::Sleep(0.0f);
	}

	SetError(FString::Printf(TEXT("Timed out after %.3f seconds collecting debugger messages until terminal type %d."), TimeoutSeconds, static_cast<int32>(TerminalType)));
	return false;
}

TArray<FAngelscriptDebugMessageEnvelope> FAngelscriptDebuggerTestClient::DrainPendingMessages()
{
	TArray<FAngelscriptDebugMessageEnvelope> Messages = MoveTemp(PendingMessages);
	PendingMessages.Reset();
	while (true)
	{
		TOptional<FAngelscriptDebugMessageEnvelope> Envelope = ReceiveEnvelope();
		if (!Envelope.IsSet())
		{
			break;
		}

		Messages.Add(MoveTemp(Envelope.GetValue()));
	}

	LastError.Reset();
	return Messages;
}

bool FAngelscriptDebuggerTestClient::SendStartDebugging(int32 AdapterVersion)
{
	FStartDebuggingMessage Message;
	Message.DebugAdapterVersion = AdapterVersion;
	return SendTypedMessage(EDebugMessageType::StartDebugging, Message);
}

bool FAngelscriptDebuggerTestClient::SendPause()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::Pause, Message);
}

bool FAngelscriptDebuggerTestClient::SendContinue()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::Continue, Message);
}

bool FAngelscriptDebuggerTestClient::SendStopDebugging()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::StopDebugging, Message);
}

bool FAngelscriptDebuggerTestClient::SendDisconnect()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::Disconnect, Message);
}

bool FAngelscriptDebuggerTestClient::SendStepIn()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::StepIn, Message);
}

bool FAngelscriptDebuggerTestClient::SendStepOver()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::StepOver, Message);
}

bool FAngelscriptDebuggerTestClient::SendStepOut()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::StepOut, Message);
}

bool FAngelscriptDebuggerTestClient::SendRequestCallStack()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::RequestCallStack, Message);
}

bool FAngelscriptDebuggerTestClient::SendBreakOptions(const TArray<FString>& Filters)
{
	FAngelscriptBreakOptions Message;
	Message.Filters = Filters;
	return SendTypedMessage(EDebugMessageType::BreakOptions, Message);
}

bool FAngelscriptDebuggerTestClient::SendRequestBreakFilters()
{
	FEmptyMessage Message;
	return SendTypedMessage(EDebugMessageType::RequestBreakFilters, Message);
}

bool FAngelscriptDebuggerTestClient::SendRequestDebugDatabase()
{
	FAngelscriptRequestDebugDatabase Message;
	return SendTypedMessage(EDebugMessageType::RequestDebugDatabase, Message);
}

bool FAngelscriptDebuggerTestClient::SendRequestVariables(const FString& ScopePath)
{
	FString Message = ScopePath;
	return SendTypedMessage(EDebugMessageType::RequestVariables, Message);
}

bool FAngelscriptDebuggerTestClient::SendRequestEvaluate(const FString& Path, int32 DefaultFrame)
{
	TArray<uint8> Body;
	FMemoryWriter Writer(Body);
	FString MessagePath = Path;
	Writer << MessagePath;
	Writer << DefaultFrame;
	return SendRawEnvelope(EDebugMessageType::RequestEvaluate, Body);
}

bool FAngelscriptDebuggerTestClient::SendSetBreakpoint(const FAngelscriptBreakpoint& Breakpoint)
{
	FAngelscriptBreakpoint Message = Breakpoint;
	return SendTypedMessage(EDebugMessageType::SetBreakpoint, Message);
}

bool FAngelscriptDebuggerTestClient::SendClearBreakpoints(const FAngelscriptClearBreakpoints& Breakpoints)
{
	FAngelscriptClearBreakpoints Message = Breakpoints;
	return SendTypedMessage(EDebugMessageType::ClearBreakpoints, Message);
}

bool FAngelscriptDebuggerTestClient::AppendReceivedData()
{
	if (!Socket.IsValid())
	{
		return false;
	}

	uint32 PendingDataSize = 0;
	while (Socket->HasPendingData(PendingDataSize) && PendingDataSize > 0)
	{
		TArray<uint8> Chunk;
		Chunk.SetNumUninitialized(static_cast<int32>(PendingDataSize));
		int32 BytesRead = 0;
		if (!Socket->Recv(Chunk.GetData(), Chunk.Num(), BytesRead))
		{
			return SetError(TEXT("Failed to receive debugger envelope bytes from the test socket."));
		}

		if (BytesRead <= 0)
		{
			return SetError(TEXT("Debugger test socket reported pending data but did not return any bytes."));
		}

		Chunk.SetNum(BytesRead, EAllowShrinking::No);
		ReceiveBuffer.Append(Chunk);
	}

	return true;
}

bool FAngelscriptDebuggerTestClient::ConsumeEnvelope(FAngelscriptDebugMessageEnvelope& OutEnvelope, bool& bOutHasEnvelope)
{
	return TryDeserializeDebugMessageEnvelope(ReceiveBuffer, OutEnvelope, bOutHasEnvelope, &LastError);
}

bool FAngelscriptDebuggerTestClient::SetError(const FString& ErrorMessage)
{
	LastError = ErrorMessage;
	return false;
}

TFuture<FSingleClientDebuggerTranscript> RunSingleClientDebuggerWorker(
	int32 Port,
	TAtomic<bool>& bWorkerReady,
	TAtomic<bool>& bShouldStop,
	const FSingleClientDebuggerWorkerConfig& Config)
{
	return Async(EAsyncExecution::ThreadPool,
		[Port, &bWorkerReady, &bShouldStop, Config]() -> FSingleClientDebuggerTranscript
		{
			FSingleClientDebuggerTranscript Transcript;
			FAngelscriptDebuggerTestClient Client;
			ON_SCOPE_EXIT
			{
				Client.SendStopDebugging();
				Client.SendDisconnect();
				Client.Disconnect();
			};

			bWorkerReady = false;

			if (!Client.Connect(TEXT("127.0.0.1"), Port, Config.TimeoutSeconds))
			{
				Transcript.Error = FString::Printf(TEXT("Single-client debugger worker failed to connect: %s"), *Client.GetLastError());
				bWorkerReady = true;
				return Transcript;
			}

			if (!Client.SendStartDebugging(Config.AdapterVersion))
			{
				Transcript.Error = FString::Printf(TEXT("Single-client debugger worker failed to send StartDebugging: %s"), *Client.GetLastError());
				bWorkerReady = true;
				return Transcript;
			}

			FString Error;
			if (!WaitForSingleClientMessageType(
				Client,
				bShouldStop,
				EDebugMessageType::DebugServerVersion,
				Config.TimeoutSeconds,
				Transcript,
				Error))
			{
				Transcript.Error = Error;
				bWorkerReady = true;
				return Transcript;
			}

			const TArray<FSingleClientDebuggerStopAction> StopActions = Config.StopActions.Num() > 0
				? Config.StopActions
				: TArray<FSingleClientDebuggerStopAction>{FSingleClientDebuggerStopAction()};

			for (const FAngelscriptBreakpoint& Breakpoint : Config.InitialBreakpoints)
			{
				if (!Client.SendSetBreakpoint(Breakpoint))
				{
					Transcript.Error = FString::Printf(TEXT("Single-client debugger worker failed to send SetBreakpoint: %s"), *Client.GetLastError());
					bWorkerReady = true;
					return Transcript;
				}
			}

			bWorkerReady = true;

			for (int32 ActionIndex = 0; ActionIndex < StopActions.Num(); ++ActionIndex)
			{
				const FSingleClientDebuggerStopAction& Action = StopActions[ActionIndex];
				if (!WaitForSingleClientMessageType(
					Client,
					bShouldStop,
					EDebugMessageType::HasStopped,
					Config.TimeoutSeconds,
					Transcript,
					Error))
				{
					Transcript.Error = Error;
					return Transcript;
				}

				if (Action.bRequestCallStack)
				{
					if (!Client.SendRequestCallStack())
					{
						Transcript.Error = FString::Printf(TEXT("Single-client debugger worker failed to request callstack: %s"), *Client.GetLastError());
						return Transcript;
					}

					if (!WaitForSingleClientMessageType(
						Client,
						bShouldStop,
						EDebugMessageType::CallStack,
						Config.TimeoutSeconds,
						Transcript,
						Error))
					{
						Transcript.Error = Error;
						return Transcript;
					}
				}

				if (!SendSingleClientCommand(Client, Action.Command, Error))
				{
					Transcript.Error = Error;
					return Transcript;
				}

				if (Action.Command == ESingleClientDebuggerCommand::Continue)
				{
					if (!WaitForSingleClientMessageType(
						Client,
						bShouldStop,
						EDebugMessageType::HasContinued,
						Config.TimeoutSeconds,
						Transcript,
						Error))
					{
						Transcript.Error = Error;
						return Transcript;
					}

					if (!DrainSingleClientMessages(
						Client,
						bShouldStop,
						Config.PostTerminalDrainSeconds,
						Transcript,
						Error))
					{
						Transcript.Error = Error;
					}

					return Transcript;
				}
			}

			Transcript.Error = TEXT("Single-client debugger worker exhausted stop actions without sending Continue.");
			return Transcript;
		});
}

