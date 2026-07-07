#include "AngelscriptDebugServer.h"
#include "AngelscriptEngine.h"
#include "AngelscriptEditorDebugBridge.h"
#include "AngelscriptDocs.h"
#include "AngelscriptPerformanceStats.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h" 

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AngelscriptSettings.h"
#include "ClassGenerator/ASClass.h"
#include "SocketSubsystem.h"

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
//#include "as_module.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "source/as_module.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_EDITOR
#include "SourceCodeNavigation.h"

const FName NAME_ScriptKeywords("ScriptKeywords");
const TArray<FName> NAMES_InformedMeta = {
	"DelegateBindType",
	"DelegateFunctionParam",
	"DelegateObjectParam",
};
#endif

#if PLATFORM_WINDOWS && WITH_AS_DEBUGSERVER
THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/MinWindows.h"
#include <DbgHelp.h>
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#endif

int AngelscriptDebugServer::DebugAdapterVersion = 0;

namespace
{
	// RequestDebugDatabase can legitimately emit multi-megabyte envelopes on mature projects.
	constexpr int32 MaxDebuggerEnvelopeSizeBytes = 16 * 1024 * 1024;
}

bool SerializeDebugMessageEnvelope(EDebugMessageType MessageType, const TArray<uint8>& Body, TArray<uint8>& OutBuffer)
{
	OutBuffer.Reset();
	FMemoryWriter Writer(OutBuffer);
	const int32 MessageLength = static_cast<int32>(sizeof(uint8)) + Body.Num();
	const uint8 MessageTypeByte = static_cast<uint8>(MessageType);
	Writer << const_cast<int32&>(MessageLength);
	Writer << const_cast<uint8&>(MessageTypeByte);
	OutBuffer.Append(Body);
	return true;
}

bool TryDeserializeDebugMessageEnvelope(TArray<uint8>& InOutBuffer, FAngelscriptDebugMessageEnvelope& OutEnvelope, bool& bOutHasEnvelope, FString* OutError)
{
	bOutHasEnvelope = false;
	if (InOutBuffer.Num() < static_cast<int32>(sizeof(int32)))
	{
		return true;
	}

	TArray<uint8> HeaderBytes;
	HeaderBytes.Append(InOutBuffer.GetData(), sizeof(int32));
	FMemoryReader HeaderReader(HeaderBytes);
	int32 MessageLength = 0;
	HeaderReader << MessageLength;

	if (MessageLength <= 0 || MessageLength > MaxDebuggerEnvelopeSizeBytes)
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT("Received debugger envelope with invalid message length %d."), MessageLength);
		}
		return false;
	}

	const int32 TotalEnvelopeSize = static_cast<int32>(sizeof(int32)) + MessageLength;
	if (InOutBuffer.Num() < TotalEnvelopeSize)
	{
		return true;
	}

	TArray<uint8> PayloadBytes;
	PayloadBytes.Append(InOutBuffer.GetData() + sizeof(int32), MessageLength);
	FMemoryReader PayloadReader(PayloadBytes);
	uint8 MessageTypeByte = static_cast<uint8>(EDebugMessageType::Disconnect);
	PayloadReader << MessageTypeByte;

	OutEnvelope.MessageType = static_cast<EDebugMessageType>(MessageTypeByte);
	OutEnvelope.Body.Reset();
	if (MessageLength > static_cast<int32>(sizeof(uint8)))
	{
		OutEnvelope.Body.Append(PayloadBytes.GetData() + sizeof(uint8), MessageLength - static_cast<int32>(sizeof(uint8)));
	}

	InOutBuffer.RemoveAt(0, TotalEnvelopeSize, EAllowShrinking::No);
	bOutHasEnvelope = true;
	return true;
}

namespace
{
	enum class EConditionalBreakpointValueKind : uint8
	{
		Invalid,
		Integer,
		Boolean,
		String,
	};

	struct FConditionalBreakpointValue
	{
		EConditionalBreakpointValueKind Kind = EConditionalBreakpointValueKind::Invalid;
		int64 IntegerValue = 0;
		bool BoolValue = false;
		FString StringValue;
	};

	bool ParseConditionalBreakpointOperator(const FString& Condition, FString& OutLeftOperand, FString& OutOperator, FString& OutRightOperand)
	{
		static const TCHAR* Operators[] = { TEXT("=="), TEXT("!="), TEXT(">="), TEXT("<="), TEXT(">"), TEXT("<") };
		for (const TCHAR* CandidateOperator : Operators)
		{
			const int32 OperatorIndex = Condition.Find(CandidateOperator);
			if (OperatorIndex == INDEX_NONE)
			{
				continue;
			}

			OutLeftOperand = Condition.Left(OperatorIndex).TrimStartAndEnd();
			OutOperator = CandidateOperator;
			OutRightOperand = Condition.RightChop(OperatorIndex + FCString::Strlen(CandidateOperator)).TrimStartAndEnd();
			return true;
		}

		return false;
	}

	bool TryParseConditionalBreakpointLiteral(const FString& Token, FConditionalBreakpointValue& OutValue)
	{
		const FString TrimmedToken = Token.TrimStartAndEnd();
		if (TrimmedToken.IsEmpty())
		{
			return false;
		}

		if (TrimmedToken.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			OutValue.Kind = EConditionalBreakpointValueKind::Boolean;
			OutValue.BoolValue = true;
			OutValue.StringValue = TEXT("true");
			return true;
		}

		if (TrimmedToken.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			OutValue.Kind = EConditionalBreakpointValueKind::Boolean;
			OutValue.BoolValue = false;
			OutValue.StringValue = TEXT("false");
			return true;
		}

		int64 IntegerValue = 0;
		if (LexTryParseString(IntegerValue, *TrimmedToken))
		{
			OutValue.Kind = EConditionalBreakpointValueKind::Integer;
			OutValue.IntegerValue = IntegerValue;
			OutValue.StringValue = TrimmedToken;
			return true;
		}

		return false;
	}

	FString NormalizeConditionalBreakpointPath(const FString& Operand)
	{
		const FString TrimmedOperand = Operand.TrimStartAndEnd();
		if (TrimmedOperand.IsEmpty() || TrimmedOperand.Contains(TEXT(":")))
		{
			return TrimmedOperand;
		}

		return FString::Printf(TEXT("0:%s"), *TrimmedOperand);
	}

	bool TryResolveConditionalBreakpointValue(FAngelscriptDebugServer& DebugServer, asCContext* Context, const FString& Operand, FConditionalBreakpointValue& OutValue)
	{
		if (TryParseConditionalBreakpointLiteral(Operand, OutValue))
		{
			return true;
		}

		if (Context == nullptr)
		{
			return false;
		}

		FDebuggerValue DebuggerValue;
		int32 Frame = 0;
		if (!DebugServer.GetDebuggerValue(NormalizeConditionalBreakpointPath(Operand), DebuggerValue, &Frame))
		{
			return false;
		}

		OutValue.StringValue = DebuggerValue.Value;
		if (DebuggerValue.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			OutValue.Kind = EConditionalBreakpointValueKind::Boolean;
			OutValue.BoolValue = true;
			return true;
		}

		if (DebuggerValue.Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			OutValue.Kind = EConditionalBreakpointValueKind::Boolean;
			OutValue.BoolValue = false;
			return true;
		}

		int64 IntegerValue = 0;
		if (LexTryParseString(IntegerValue, *DebuggerValue.Value))
		{
			OutValue.Kind = EConditionalBreakpointValueKind::Integer;
			OutValue.IntegerValue = IntegerValue;
			return true;
		}

		OutValue.Kind = EConditionalBreakpointValueKind::String;
		return true;
	}

	TOptional<bool> EvaluateConditionalBreakpoint(FAngelscriptDebugServer& DebugServer, asCContext* Context, const FString& Condition, FString* OutFailureReason)
	{
		const FString TrimmedCondition = Condition.TrimStartAndEnd();
		if (TrimmedCondition.IsEmpty())
		{
			return true;
		}

		auto SetFailureReason = [OutFailureReason](const FString& Message)
		{
			if (OutFailureReason != nullptr)
			{
				*OutFailureReason = Message;
			}
		};

		FString LeftOperand;
		FString Operator;
		FString RightOperand;
		if (!ParseConditionalBreakpointOperator(TrimmedCondition, LeftOperand, Operator, RightOperand))
		{
			FConditionalBreakpointValue Value;
			if (!TryResolveConditionalBreakpointValue(DebugServer, Context, TrimmedCondition, Value))
			{
				SetFailureReason(FString::Printf(TEXT("Could not resolve breakpoint condition operand '%s'."), *TrimmedCondition));
				return {};
			}

			switch (Value.Kind)
			{
			case EConditionalBreakpointValueKind::Boolean:
				return Value.BoolValue;

			case EConditionalBreakpointValueKind::Integer:
				return Value.IntegerValue != 0;

			case EConditionalBreakpointValueKind::String:
				return !Value.StringValue.IsEmpty();

			default:
				SetFailureReason(FString::Printf(TEXT("Breakpoint condition '%s' evaluated to an unsupported value kind."), *TrimmedCondition));
				return {};
			}
		}

		FConditionalBreakpointValue LeftValue;
		FConditionalBreakpointValue RightValue;
		if (!TryResolveConditionalBreakpointValue(DebugServer, Context, LeftOperand, LeftValue))
		{
			SetFailureReason(FString::Printf(TEXT("Could not resolve left operand '%s' for conditional breakpoint '%s'."), *LeftOperand, *TrimmedCondition));
			return {};
		}

		if (!TryResolveConditionalBreakpointValue(DebugServer, Context, RightOperand, RightValue))
		{
			SetFailureReason(FString::Printf(TEXT("Could not resolve right operand '%s' for conditional breakpoint '%s'."), *RightOperand, *TrimmedCondition));
			return {};
		}

		const bool bLeftIsNumeric = LeftValue.Kind == EConditionalBreakpointValueKind::Integer || LeftValue.Kind == EConditionalBreakpointValueKind::Boolean;
		const bool bRightIsNumeric = RightValue.Kind == EConditionalBreakpointValueKind::Integer || RightValue.Kind == EConditionalBreakpointValueKind::Boolean;
		if (bLeftIsNumeric && bRightIsNumeric)
		{
			const int64 LeftNumericValue = LeftValue.Kind == EConditionalBreakpointValueKind::Boolean ? (LeftValue.BoolValue ? 1 : 0) : LeftValue.IntegerValue;
			const int64 RightNumericValue = RightValue.Kind == EConditionalBreakpointValueKind::Boolean ? (RightValue.BoolValue ? 1 : 0) : RightValue.IntegerValue;

			if (Operator == TEXT("=="))
			{
				return LeftNumericValue == RightNumericValue;
			}
			if (Operator == TEXT("!="))
			{
				return LeftNumericValue != RightNumericValue;
			}
			if (Operator == TEXT(">"))
			{
				return LeftNumericValue > RightNumericValue;
			}
			if (Operator == TEXT(">="))
			{
				return LeftNumericValue >= RightNumericValue;
			}
			if (Operator == TEXT("<"))
			{
				return LeftNumericValue < RightNumericValue;
			}
			if (Operator == TEXT("<="))
			{
				return LeftNumericValue <= RightNumericValue;
			}
		}

		if (Operator == TEXT("=="))
		{
			return LeftValue.StringValue == RightValue.StringValue;
		}
		if (Operator == TEXT("!="))
		{
			return LeftValue.StringValue != RightValue.StringValue;
		}

		SetFailureReason(FString::Printf(TEXT("Conditional breakpoint '%s' uses unsupported operator '%s' for non-numeric values."), *TrimmedCondition, *Operator));
		return {};
	}
}

namespace DataBreakpoint_Windows
{
#if PLATFORM_WINDOWS && WITH_AS_DEBUGSERVER
	static TAtomic<FAngelscriptDebugServer*> GActiveDebugServer { nullptr };

	static PVOID DegbugRegisterExceptionHandlerHandle;
	static TAtomic<bool> GClearDataBreakpoints { false };

	struct FThreadContextGuard
	{
		HANDLE Thread;
		FThreadContextGuard(HANDLE InThread)
		{
			Thread = InThread;
			SuspendThread(Thread);
		}

		~FThreadContextGuard()
		{
			ResumeThread(Thread);
		}
	};

	void ApplyBreakpointsToThreadContext(const TArray<FAngelscriptDataBreakpoint>& Breakpoints, CONTEXT& Context)
	{
		// Disable all registers initially
		Context.Dr7 &= 0xFFFFFFFFFFFFFF00;

		TArray<uint8> ModifiedRegisters;
		for (uint8 RegisterToModify = 0; RegisterToModify < Breakpoints.Num(); RegisterToModify++)
		{
			if (Breakpoints[RegisterToModify].Status != EAngelscriptDataBreakpointStatus::Keep)
			{
				continue;
			}

			switch (RegisterToModify)
			{
			case 0: Context.Dr0 = Breakpoints[RegisterToModify].Address; break;
			case 1: Context.Dr1 = Breakpoints[RegisterToModify].Address; break;
			case 2: Context.Dr2 = Breakpoints[RegisterToModify].Address; break;
			case 3: Context.Dr3 = Breakpoints[RegisterToModify].Address; break;
			}

			Context.Dr7 |= static_cast<DWORD64>(0x1) << (RegisterToModify * 2); // Local enable
			// For now we dont need global breakpoints
			//Context.Dr7 |= static_cast<DWORD64>(0x1) << (RegisterToUse * 2 + 1); // Global enable

			DWORD64 BreakpointSettings = 0x1; // Writes only
			switch (Breakpoints[RegisterToModify].AddressSize)
			{
			case 1: break;
			case 2: BreakpointSettings |= 0b0100; break;
			case 4: BreakpointSettings |= 0b1100; break;
			case 8: BreakpointSettings |= 0b1000; break;
			}

			Context.Dr7 |= static_cast<DWORD64>(BreakpointSettings) << (16 + RegisterToModify * 4);
		}
	}

	void ApplyBreakpointsToThreadContext(const FAngelscriptActiveDataBreakpoint* Breakpoints, int32 BreakpointCount, CONTEXT& Context)
	{
		Context.Dr7 &= 0xFFFFFFFFFFFFFF00;

		for (uint8 RegisterToModify = 0; RegisterToModify < BreakpointCount; RegisterToModify++)
		{
			if (Breakpoints[RegisterToModify].GetStatus() != EAngelscriptDataBreakpointStatus::Keep)
			{
				continue;
			}

			switch (RegisterToModify)
			{
			case 0: Context.Dr0 = Breakpoints[RegisterToModify].Address; break;
			case 1: Context.Dr1 = Breakpoints[RegisterToModify].Address; break;
			case 2: Context.Dr2 = Breakpoints[RegisterToModify].Address; break;
			case 3: Context.Dr3 = Breakpoints[RegisterToModify].Address; break;
			}

			Context.Dr7 |= static_cast<DWORD64>(0x1) << (RegisterToModify * 2);

			DWORD64 BreakpointSettings = 0x1;
			switch (Breakpoints[RegisterToModify].AddressSize)
			{
			case 1: break;
			case 2: BreakpointSettings |= 0b0100; break;
			case 4: BreakpointSettings |= 0b1100; break;
			case 8: BreakpointSettings |= 0b1000; break;
			}

			Context.Dr7 |= static_cast<DWORD64>(BreakpointSettings) << (16 + RegisterToModify * 4);
		}
	}

	static HANDLE GetThreadAgnosticCurrentThreadHandle()
	{
		// GetCurrentThread() returns a psuedo-handle that refers to the current thread wherever it is used, so we need a "real" 
		// handle that actually refers to the current thread, even if used from another thread.
		HANDLE RealHandle;
		DuplicateHandle(GetCurrentProcess(),
			GetCurrentThread(),
			GetCurrentProcess(),
			&RealHandle,
			0,
			1,
			DUPLICATE_SAME_ACCESS);
		return RealHandle;
	}

	struct FUpdateDebugRegisterThread : FRunnable
	{
		explicit FUpdateDebugRegisterThread(HANDLE InThreadToDebug, const TArray<FAngelscriptDataBreakpoint>& InBreakpoints)
			: ThreadToDebug(InThreadToDebug), Breakpoints(InBreakpoints)
		{
			ThreadToDebug = InThreadToDebug;
			Thread.Reset(FRunnableThread::Create(this, TEXT("Angelscript Update Debug Register Thread"), 0, TPri_Highest));
		}
		~FUpdateDebugRegisterThread()
		{
			Thread->WaitForCompletion();
		}

		virtual uint32 Run() override
		{
			CONTEXT Context;
			memset(&Context, 0, sizeof(Context));
			FThreadContextGuard ContextGuard(ThreadToDebug);

			// Doesn't seem like the data returned here is the same as the one we set, which I find somewhat strange. But we overwrite it anyways here..
			if (!GetThreadContext(ThreadToDebug, &Context))
			{
				UE_LOG(Angelscript, Error, TEXT("FUpdateDebugRegisterThread: Failed to get ThreadContext!"));
				return INDEX_NONE;
			}

			Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			// Initialize Dr7 according to https://en.wikipedia.org/wiki/X86_debug_register
			// I.e Bit 10 should be 1, Bit 14,15 and 32-63 should be 0, everything else are values we set, so we zero them as well
			Context.Dr7 &= 0x0000000000003F00;
			Context.Dr7 |= 0x001000000000;

			ApplyBreakpointsToThreadContext(Breakpoints, Context);

			// TODO: can we use __readdr and __writedr on the game thread instead?, without suspending?

			if (!SetThreadContext(ThreadToDebug, &Context))
			{
				UE_LOG(Angelscript, Error, TEXT("FUpdateDebugRegisterThread: Failed to set ThreadContext!"));
				return INDEX_NONE;
			}

			return 0;
		}

	private:
		HANDLE ThreadToDebug;
		TArray<FAngelscriptDataBreakpoint> Breakpoints;
		TUniquePtr<FRunnableThread> Thread;
	};

	LONG WINAPI DebugRegisterExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo)
	{
		// Good to know things about the context of this function call
		// * Uses same stack as calling context
		// * Don't do any form of heap allocation, as if the function is called from a malloc
		// context we might "steal" memory they were about to use, resulting in a crash after returning program flow.

		if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP && (ExceptionInfo->ContextRecord->Dr6 & 0xF) > 0)
		{
			FAngelscriptDebugServer* DebugServer = GActiveDebugServer.Load();
			if (DebugServer == nullptr)
			{
				return EXCEPTION_CONTINUE_SEARCH;
			}
			const int32 ActiveBreakpointCount = FMath::Min(DebugServer->ActiveDataBreakpointCount.Load(), FAngelscriptDebugServer::DATA_BREAKPOINT_HARDWARE_LIMIT);

			bool bCppBreakpointTriggered = false;
			bool bBreakpointTriggered = false;
			for (uint8 i = 0; i < FAngelscriptDebugServer::DATA_BREAKPOINT_HARDWARE_LIMIT; i++)
			{
				if (i >= ActiveBreakpointCount)
				{
					continue;
				}

				if ((ExceptionInfo->ContextRecord->Dr6 & (static_cast<DWORD64>(0x1) << i)) == 0)
				{
					continue;
				}

				auto& Breakpoint = DebugServer->ActiveDataBreakpoints[i];
				if (Breakpoint.GetStatus() != EAngelscriptDataBreakpointStatus::Keep)
				{
					continue;
				}

				const int32 PreviousHitCount = Breakpoint.HitCount.Load();
				bool bTriggeredThisIteration = false;
				if (PreviousHitCount > 0)
				{
					const int32 NewHitCount = PreviousHitCount - 1;
					Breakpoint.HitCount.Store(NewHitCount);

					// Breakpoints with a HitCount only trigger once they reach 0 hits left
					if (NewHitCount == 0)
					{
						Breakpoint.SetStatus(EAngelscriptDataBreakpointStatus::Remove_ReachedHitCount);
						Breakpoint.bTriggered.Store(true);
						bTriggeredThisIteration = true;
					}
				}
				else
				{
					Breakpoint.bTriggered.Store(true);
					bTriggeredThisIteration = true;
				}

				asCContext* Context = (asCContext*)asGetActiveContext();
				Breakpoint.SetContext(Context);
				bCppBreakpointTriggered |= (bTriggeredThisIteration && Breakpoint.bCppBreakpoint);
				bBreakpointTriggered |= bTriggeredThisIteration || Breakpoint.bTriggered.Load();
			}

			// AS breakpoints are deferred to the next script line (hence why we cache line number here), since we need to use heap memory to properly handle it
			if (bBreakpointTriggered)
			{
				DebugServer->bBreakNextScriptLine.Store(true);

				ApplyBreakpointsToThreadContext(DebugServer->ActiveDataBreakpoints, ActiveBreakpointCount, *ExceptionInfo->ContextRecord);
			}

			// Still unsure how the RF (Resume Flags) of the EFlags register works, I'm guessing it is handled by VS, or 
			// automatically handled by the hardware (but I see posts that mention you should set it to avoid an infinite breakpoint
			// loop, but I've yet to see that)

			// Zero out breakpoint detection flags, if not we will get multiple triggers for each interrupt
			ExceptionInfo->ContextRecord->Dr6 &= (~static_cast<DWORD>(0)) & 0x0;
			ExceptionInfo->ContextRecord->ContextFlags |= CONTEXT_DEBUG_REGISTERS;

			if (bCppBreakpointTriggered && FPlatformMisc::IsDebuggerPresent())
			{
				UE_DEBUG_BREAK();
			}

			// Call ClearAllAngelscriptDataBreakpointsFromHandler() from the debugger immediate window to exit a spamming data breakpoint
			if (GClearDataBreakpoints.Load())
			{
				for (int i = 0; i < ActiveBreakpointCount; i++)
				{
					DebugServer->ActiveDataBreakpoints[i].bTriggered.Store(false);
					DebugServer->ActiveDataBreakpoints[i].SetStatus(EAngelscriptDataBreakpointStatus::Remove_ReachedHitCount);
				}

				GClearDataBreakpoints.Store(false);
				DebugServer->bBreakNextScriptLine.Store(false);
			}
			else if (bBreakpointTriggered)
			{
				DebugServer->bBreakNextScriptLine.Store(true);
			}

			if (FAngelscriptEngine* OwnerEngine = DebugServer->GetOwnerEngine())
			{
				OwnerEngine->UpdateLineCallbackState();
			}

			// Prevent the exception from propagating further
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		// Let the OS deal with the exception as normal
		return EXCEPTION_CONTINUE_SEARCH;
	}
#endif
}

void ClearAllAngelscriptDataBreakpointsFromHandler()
{
#if PLATFORM_WINDOWS && WITH_AS_DEBUGSERVER
	DataBreakpoint_Windows::GClearDataBreakpoints = true;
#endif
}

bool FAngelscriptDebugServer::HandleConnectionAccepted(class FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	UE_LOG(Angelscript, Log, TEXT("Received angelscript debug connection from %s"), *ClientEndpoint.ToText().ToString());
	PendingClients.Enqueue(ClientSocket);
	return true;
}

void FAngelscriptDebugServer::RemoveClientState(FSocket* Client, bool bResetDebugStateIfLastDebuggingClient)
{
	if (Client == nullptr)
	{
		return;
	}

	const int32 RemovedDebuggingClients = ClientsThatAreDebugging.RemoveSwap(Client);
	ClientsThatWantDebugDatabase.RemoveSwap(Client);
	CallstackRequests.RemoveSwap(Client);
	QueuedSends.Remove(Client);

	if (bResetDebugStateIfLastDebuggingClient && RemovedDebuggingClients > 0 && ClientsThatAreDebugging.Num() == 0)
	{
		bIsDebugging = false;
		bPauseRequested = false;
		bIsPaused = false;
		bBreakNextScriptLine = false;
		ClearAllBreakpoints();

		if (OwnerEngine != nullptr)
		{
			OwnerEngine->UpdateLineCallbackState();
		}
		else if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
		{
			CurrentEngine->UpdateLineCallbackState();
		}
	}
}

void FAngelscriptDebugServer::CloseAndDestroyClient(FSocket*& Client)
{
	if (Client == nullptr)
	{
		return;
	}

	Client->Close();
	if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		SocketSubsystem->DestroySocket(Client);
	}

	Client = nullptr;
}

void FAngelscriptDebugServer::ResetClientStateForShutdown()
{
	FSocket* PendingClient = nullptr;
	while (PendingClients.Dequeue(PendingClient))
	{
		CloseAndDestroyClient(PendingClient);
	}

	for (FSocket*& Client : Clients)
	{
		RemoveClientState(Client, false);
		CloseAndDestroyClient(Client);
	}

	Clients.Empty();
	ClientsThatWantDebugDatabase.Empty();
	ClientsThatAreDebugging.Empty();
	QueuedSends.Empty();
	CallstackRequests.Empty();

	bIsDebugging = false;
	bPauseRequested = false;
	bIsPaused = false;
	bIsEvaluatingDebuggerWatch = false;
	bBreakNextScriptLine = false;
	NextPingDebuggerAliveTime = 0.0;

	Breakpoints.Empty();
	SectionBreakpoints.Empty();
	BreakpointCount = 0;
	DataBreakpoints.Reset();
	RebuildActiveDataBreakpoints();

	IgnoreBreakSection = nullptr;
	IgnoreBreakLine = -1;
	ConditionBreakFunction = nullptr;
	ConditionBreakFrame = -1;
	StackFrameThis.Empty();
}

	FAngelscriptDebugServer::FAngelscriptDebugServer(FAngelscriptEngine* InOwnerEngine, int Port)
{
	LLM_SCOPE_BYTAG(Angelscript);
	OwnerEngine = InOwnerEngine;
	Listener = new FTcpListener(FIPv4Endpoint(FIPv4Address::Any, Port));
	Listener->OnConnectionAccepted().BindRaw(this, &FAngelscriptDebugServer::HandleConnectionAccepted);

	UE_LOG(Angelscript, Log, TEXT("Angelscript debug server listening on %s"), *Listener->GetLocalEndpoint().ToText().ToString());
#if PLATFORM_WINDOWS && WITH_AS_DEBUGSERVER
	DataBreakpoint_Windows::GActiveDebugServer.Store(this);
	if (DataBreakpoint_Windows::DegbugRegisterExceptionHandlerHandle)
	{
		::RemoveVectoredExceptionHandler(DataBreakpoint_Windows::DegbugRegisterExceptionHandlerHandle);
	}
	DataBreakpoint_Windows::DegbugRegisterExceptionHandlerHandle = ::AddVectoredExceptionHandler(0, DataBreakpoint_Windows::DebugRegisterExceptionHandler);
#endif
}

FAngelscriptDebugServer::~FAngelscriptDebugServer()
{
	UnbindAssetRegistry();

#if PLATFORM_WINDOWS && WITH_AS_DEBUGSERVER
	DataBreakpoint_Windows::GActiveDebugServer.Store(nullptr);
	::RemoveVectoredExceptionHandler(DataBreakpoint_Windows::DegbugRegisterExceptionHandlerHandle);
	DataBreakpoint_Windows::DegbugRegisterExceptionHandlerHandle = nullptr;
#endif

	if (Listener)
	{
		Listener->Stop();
		delete Listener;
		Listener = NULL;
	}

	ResetClientStateForShutdown();
}

void FAngelscriptDebugServer::Tick()
{
	AS_PERF_SCOPE_DEBUG_SERVER_TICK();
	ProcessMessages();
}

void FAngelscriptDebugServer::ProcessException(class asIScriptContext* Context)
{
	if (bIsPaused)
	{
		// Maybe we need to do something if an exception occurs while paused?
		return;
	}

	ProcessMessages();

	if (!bIsDebugging)
	{
		return;
	}

	FStoppedMessage StopMsg;
	StopMsg.Reason = TEXT("exception");

	const ANSICHAR* ExceptionString = Context->GetExceptionString();
	if (ExceptionString)
		StopMsg.Text = ANSI_TO_TCHAR(ExceptionString);

	PauseExecution(&StopMsg);
}

void FAngelscriptDebugServer::ProcessScriptLine(class asCContext* Context)
{
	// Early out from the line callback if we're not debugging
	if (!bIsDebugging)
		return;

	// Don't run the line callback if we're already in a breakpoint
	if (bIsPaused)
		return;

	// Don't break if we're stopping the engine
	if (IsEngineExitRequested())
		return;

	// Running scripts still need to poll the debugger socket so manual Pause can
	// arm the next line break without waiting for an outer engine tick.
	const bool bHadPendingLineBreakBeforePolling = bBreakNextScriptLine.Load();
	ProcessMessages();

	if (!bIsDebugging || bIsPaused)
		return;

	// A Pause request that arrived while processing this callback should stop on
	// the *next* script line, not immediately on the current callback.
	if (!bHadPendingLineBreakBeforePolling && bPauseRequested && bBreakNextScriptLine.Load())
	{
		return;
	}

	if (DataBreakpoints.Num() > 0 && bBreakNextScriptLine)
	{
		SyncActiveDataBreakpointsToAuthoritativeState();

		FAngelscriptClearDataBreakpoints ClearMessage;
		TArray<FString> TriggeredBreakpoints;

		for (int i = DataBreakpoints.Num() - 1; i >= 0; i--)
		{
			auto& Breakpoint = DataBreakpoints[i];

			if (Breakpoint.bTriggered)
			{
				if (bBreakNextScriptLine.Load())
				{
					bBreakNextScriptLine.Store(false);
					FAngelscriptEngine::Get().UpdateLineCallbackState();
				}

				FString InfoText = "";
				switch (Breakpoint.Status)
				{
					case EAngelscriptDataBreakpointStatus::Keep:
					{
						InfoText = Breakpoint.Context == nullptr ?
							FString::Printf(TEXT("Data breakpoint (%s) triggered in C++!"), *Breakpoint.Name) :
							FString::Printf(TEXT("Data breakpoint (%s) triggered!"), *Breakpoint.Name);
					}
					break;
					case EAngelscriptDataBreakpointStatus::Remove_OutOfScope:
					{
						InfoText = FString::Printf(TEXT("Variable Out Of Scope (%s), Data Breakpoint disabled!"), *Breakpoint.Name);
					}
					break;
					case EAngelscriptDataBreakpointStatus::Remove_ReachedHitCount:
					{
						InfoText = Breakpoint.Context == nullptr ?
							FString::Printf(TEXT("Data breakpoint (%s) triggered in C++! (HitCount reached, Data Breakpoint disabled!)"), *Breakpoint.Name) :
							FString::Printf(TEXT("Data breakpoint (%s) triggered! (HitCount reached, Data Breakpoint disabled!)"), *Breakpoint.Name);
					}
					break;
				}

				TriggeredBreakpoints.Add(InfoText);
				Breakpoint.bTriggered = false;
			}

			if (Breakpoint.Status != EAngelscriptDataBreakpointStatus::Keep)
			{
				ClearMessage.Ids.Add(Breakpoint.Id);
				DataBreakpoints.RemoveAtSwap(i);
			}
		}

		for (const FString& InfoText : TriggeredBreakpoints)
		{
			FStoppedMessage Msg;
			Msg.Text = InfoText;
			Msg.Reason = TEXT("exception");
			PauseExecution(&Msg);
		}

		if (ClearMessage.Ids.Num() > 0)
		{
			SendMessageToAll(EDebugMessageType::ClearDataBreakpoints, ClearMessage);
			UpdateDataBreakpoints();
		}
	}

	bool bWasPause = false;
	bool bWasStep = false;
	bool bWasIgnored = false;
	if (bBreakNextScriptLine)
	{
		bool bShouldBreak = true;

		if (ConditionBreakFrame != -1 && ConditionBreakFunction != nullptr)
		{
			int32 CallstackSize = Context->GetCallstackSize();
			int32 CheckFrame = CallstackSize - ConditionBreakFrame - 1;
			if (CheckFrame > 0 && Context->GetFunction(CheckFrame) == ConditionBreakFunction)
			{
				bShouldBreak = false;
			}
		}

		if (bShouldBreak)
		{
			bWasPause = bPauseRequested;
			bWasStep = !bWasPause;
			bIsPaused = true;
			bBreakNextScriptLine.Store(false);
			bPauseRequested = false;

			ConditionBreakFrame = -1;
			ConditionBreakFunction = nullptr;

			FAngelscriptEngine::Get().UpdateLineCallbackState();
		}
	}
	else if (BreakpointCount > 0
		&& Context->m_currentFunction != nullptr
		&& Context->m_currentFunction->module != nullptr
		&& Context->m_currentFunction->module->hasBreakPoints)
	{
		const char* Section = nullptr;
		int32 Line = Context->GetLineNumber(0, nullptr, &Section);

		if (Section != nullptr)
		{
			if (Line == IgnoreBreakLine && Section == IgnoreBreakSection)
				bWasIgnored = true;

			TSharedPtr<FFileBreakpoints>& ActiveBreakpoints = SectionBreakpoints.FindOrAdd(Section);
			if (!ActiveBreakpoints.IsValid())
			{
				FString ModuleName = ANSI_TO_TCHAR(Context->m_currentFunction->module->baseModuleName.AddressOf());
				TSharedPtr<FFileBreakpoints>& BreakpointStore = Breakpoints.FindOrAdd(ModuleName);
				if (!BreakpointStore.IsValid())
				{
					BreakpointStore = MakeShared<FFileBreakpoints>();
				}

				ActiveBreakpoints = BreakpointStore;
			}

			if (ActiveBreakpoints->Lines.Contains(Line) && !bWasIgnored)
			{
				bool bConditionAllowsBreak = true;
				if (const FString* ConditionExpression = ActiveBreakpoints->Conditions.Find(Line))
				{
					FString ConditionFailureReason;
					const TOptional<bool> EvaluatedCondition = EvaluateConditionalBreakpoint(*this, Context, *ConditionExpression, &ConditionFailureReason);
					if (EvaluatedCondition.IsSet())
					{
						bConditionAllowsBreak = EvaluatedCondition.GetValue();
					}
					else
					{
						UE_LOG(Angelscript, Warning, TEXT("Failed to evaluate conditional breakpoint '%s' at %s:%d. %s"),
							**ConditionExpression,
							ANSI_TO_TCHAR(Section),
							Line,
							*ConditionFailureReason);
						bConditionAllowsBreak = true;
					}
				}

				if (bConditionAllowsBreak && ShouldBreakOnActiveSide())
				{
					bIsPaused = true;
					IgnoreBreakLine = Line;
					IgnoreBreakSection = Section;
				}
			}
		}

	}

	if (!bWasIgnored && !bIsPaused)
	{
		IgnoreBreakLine = -1;
		IgnoreBreakSection = nullptr;
	}

	if (bIsPaused)
	{
		FStoppedMessage StopMsg;
		StopMsg.Reason = bWasPause ? TEXT("pause") : (bWasStep ? TEXT("step") : TEXT("breakpoint"));

		PauseExecution(&StopMsg);
	}
}

void FAngelscriptDebugServer::ProcessScriptStackPop(class asCContext* Context, void* OldStackFrameStart, void* OldStackFrameEnd)
{
	for (auto& Breakpoint : DataBreakpoints)
	{
		void* BreakpointAddress = reinterpret_cast<void*>(Breakpoint.Address);
		// Stack grows downwards, so start is a higher address and end is a lower address
		if (BreakpointAddress <= OldStackFrameStart && BreakpointAddress >= OldStackFrameEnd)
		{
			Breakpoint.Status = EAngelscriptDataBreakpointStatus::Remove_OutOfScope;
			Breakpoint.bTriggered = true;
			Breakpoint.Context = Context;

			RebuildActiveDataBreakpoints();
			bBreakNextScriptLine.Store(true);
			if (OwnerEngine != nullptr)
			{
				OwnerEngine->UpdateLineCallbackState();
			}
		}
	}
}

bool FAngelscriptDebugServer::ShouldBreakOnActiveSide()
{
	UObject* WorldContext = OwnerEngine != nullptr ? OwnerEngine->GetCurrentWorldContextObject() : nullptr;
	if (WorldContext == nullptr)
		return true;

	auto& Delegate = OwnerEngine != nullptr
		? OwnerEngine->GetDebugCheckBreakOptions()
		: FAngelscriptEngine::Get().GetDebugCheckBreakOptions();
	if (Delegate.IsBound())
	{
		return Delegate.Execute(BreakOptions, WorldContext);
	}

	return true;
}

void FAngelscriptDebugServer::PauseExecution(FStoppedMessage* StopMessage)
{
	bIsPaused = true;

	if (StopMessage != nullptr)
	{
		SendMessageToAll(EDebugMessageType::HasStopped, *StopMessage);
	}
	else
	{
		FStoppedMessage StopMsg;
		StopMsg.Reason = TEXT("pause");

		SendMessageToAll(EDebugMessageType::HasStopped, StopMsg);
	}

	// Reset loop detection on context so we don't trigger timeouts during breakpoints
	auto* Context = (asCContext*)asGetActiveContext();
	if (Context != nullptr)
	{
		Context->m_loopDetectionTimer = -1.0;
	}

	const double PauseDeadline = MaxPauseTimeoutSeconds > 0.0f
		? FPlatformTime::Seconds() + MaxPauseTimeoutSeconds
		: 0.0;

	// Wait for the debugger to unpause
	while (bIsPaused)
	{
		if (PauseDeadline > 0.0 && FPlatformTime::Seconds() >= PauseDeadline)
		{
			UE_LOG(Angelscript, Warning,
				TEXT("PauseExecution auto-resuming after %.1f second safety timeout (no Continue received from debugger client)."),
				MaxPauseTimeoutSeconds);
			bIsPaused = false;
			break;
		}

		ProcessMessages();
		FPlatformProcess::Sleep(0);
	}

	FEmptyMessage ContinueMsg;
	SendMessageToAll(EDebugMessageType::HasContinued, ContinueMsg);

}

void FAngelscriptDebugServer::SleepForCommunicate(float Duration)
{
	double UnSleepTime = FPlatformTime::Seconds() + Duration;
	while (FPlatformTime::Seconds() < UnSleepTime)
	{
		ProcessMessages();
		FPlatformProcess::Sleep(0);
	}
}

void FAngelscriptDebugServer::ProcessMessages()
{
	if (!PendingClients.IsEmpty())
	{
		FSocket* Client = NULL;
		while (PendingClients.Dequeue(Client))
		{
			UE_LOG(Angelscript, Log, TEXT("Added angelscript debug client from %s"), *Client->GetDescription());
			Clients.Add(Client);
		}
	}

	for (int32 ClientIndex = Clients.Num() - 1; ClientIndex >= 0; --ClientIndex)
	{
		FSocket* Client = Clients[ClientIndex];
		if (Client == nullptr)
		{
			QueuedSends.Remove(nullptr);
			Clients.RemoveAtSwap(ClientIndex);
			continue;
		}

		TArray<FQueuedMessage>& Queue = QueuedSends.FindOrAdd(Client);
		if (Client->GetConnectionState() != SCS_Connected
			|| (Queue.Num() > 0 && Queue[0].FirstTry >= 0.0 && Queue[0].FirstTry < FPlatformTime::Seconds() - 10.0))
		{
			UE_LOG(Angelscript, Log, TEXT("Removing angelscript debug client from %s"), *Client->GetDescription());
			RemoveClientState(Client, true);
			CloseAndDestroyClient(Client);
			Clients.RemoveAtSwap(ClientIndex);
		}
	}

	for (TArray<class FSocket*>::TIterator ClientIt(Clients); ClientIt; ++ClientIt)
	{
		FSocket* Client = *ClientIt;
		uint32 DataSize = 0;

		while (Client->HasPendingData(DataSize))
		{
			int32 BytesReceived = 0;
			int32 PacketSize = -1;

			FArrayReaderPtr Datagram = MakeShareable(new FArrayReader(true));
			Datagram->SetNumUninitialized(sizeof(PacketSize));

			// Loop to get packet size
			if (BytesReceived < sizeof(PacketSize))
			{
				while (BytesReceived < sizeof(PacketSize))
				{
					int32 BytesRead = 0;
					Client->Recv(Datagram->GetData(), Datagram->Num() - BytesReceived, BytesRead);
					BytesReceived += BytesRead;
				}

				*Datagram << PacketSize;
			}

			if (PacketSize <= 0 || PacketSize > MaxDebuggerEnvelopeSizeBytes)
				break;

			// Loop until all data received
			BytesReceived = 0;
			Datagram = MakeShareable(new FArrayReader(true));
			Datagram->SetNumUninitialized(PacketSize);
			while (BytesReceived < PacketSize)
			{
				int32 BytesRead = 0;
				Client->Recv(Datagram->GetData(), Datagram->Num() - BytesReceived, BytesRead);
				BytesReceived += BytesRead;
			}

			EDebugMessageType MessageType;
			*Datagram << MessageType;

			HandleMessage(MessageType, Datagram, Client);
		}

		TrySendingMessages(Client);
	}

	// Send pings to actively debugging clients every once in a while to detect disconnections
	if (bIsDebugging && FPlatformTime::Seconds() >= NextPingDebuggerAliveTime)
	{
		for (auto* Client : ClientsThatAreDebugging)
		{
			FEmptyMessage EmptyMessage;
			SendMessageToClient(Client, EDebugMessageType::PingAlive, EmptyMessage);
		}

		NextPingDebuggerAliveTime = FPlatformTime::Seconds() + 5.0;
	}

	if (bIsPaused && CallstackRequests.Num() > 0)
	{
		for(auto* Socket : CallstackRequests)
			SendCallStack(Socket);
		CallstackRequests.Empty();
	}
}

void FAngelscriptDebugServer::HandleMessage(EDebugMessageType MessageType, FArrayReaderPtr Datagram, class FSocket* Client)
{
	if (MessageType == EDebugMessageType::RequestDebugDatabase)
	{
		ClientsThatWantDebugDatabase.Add(Client);
		SendDebugDatabase(Client);
		FAngelscriptEngine::Get().EmitDiagnostics(Client);
	}
	else if (MessageType == EDebugMessageType::Pause)
	{
		bBreakNextScriptLine = true;
		bPauseRequested = true;

		ConditionBreakFrame = -1;
		ConditionBreakFunction = nullptr;

		FAngelscriptEngine::Get().UpdateLineCallbackState();
	}
	else if (MessageType == EDebugMessageType::Continue)
	{
		bIsPaused = false;
		bBreakNextScriptLine = false;
		bPauseRequested = false;

		ConditionBreakFrame = -1;
		ConditionBreakFunction = nullptr;

		FAngelscriptEngine::Get().UpdateLineCallbackState();
	}
	else if (MessageType == EDebugMessageType::StepIn)
	{
		bBreakNextScriptLine = true;
		bPauseRequested = false;
		bIsPaused = false;

		ConditionBreakFrame = -1;
		ConditionBreakFunction = nullptr;

		FAngelscriptEngine::Get().UpdateLineCallbackState();
	}
	else if (MessageType == EDebugMessageType::StepOver)
	{
		bBreakNextScriptLine = true;
		bPauseRequested = false;
		bIsPaused = false;

		auto* Context = asGetActiveContext();
		if (Context)
		{
			int32 CallstackSize = Context->GetCallstackSize();
			ConditionBreakFrame = CallstackSize - 1;
			ConditionBreakFunction = Context->GetFunction(0);
		}
		else
		{
			ConditionBreakFrame = -1;
			ConditionBreakFunction = nullptr;
		}

		FAngelscriptEngine::Get().UpdateLineCallbackState();
	}
	else if (MessageType == EDebugMessageType::StepOut)
	{
		bBreakNextScriptLine = true;
		bPauseRequested = false;
		bIsPaused = false;

		auto* Context = asGetActiveContext();
		if (Context && Context->GetCallstackSize() >= 2)
		{
			int32 CallstackSize = Context->GetCallstackSize();
			ConditionBreakFrame = CallstackSize - 2;
			ConditionBreakFunction = Context->GetFunction(1);
		}
		else
		{
			// StepOut on the top frame should run to completion instead of re-breaking on the next line.
			bBreakNextScriptLine = false;
			ConditionBreakFrame = -1;
			ConditionBreakFunction = nullptr;
		}

		FAngelscriptEngine::Get().UpdateLineCallbackState();
	}
	else if (MessageType == EDebugMessageType::StartDebugging)
	{
		FStartDebuggingMessage Msg;
		*Datagram << Msg;
		
		bIsDebugging = true;
		bPauseRequested = false;
		bBreakNextScriptLine = false;
		AngelscriptDebugServer::DebugAdapterVersion = Msg.DebugAdapterVersion;

		FDebugServerVersionMessage DebugServerVersionMessage;
		DebugServerVersionMessage.DebugServerVersion = DEBUG_SERVER_VERSION;
		SendMessageToClient(Client, EDebugMessageType::DebugServerVersion, DebugServerVersionMessage);
		
		const bool bIsFirstDebuggingClient = ClientsThatAreDebugging.Num() == 0;
		if (bIsFirstDebuggingClient)
		{
			BreakOptions.Empty();
			ClearAllBreakpoints();
		}

		ClientsThatAreDebugging.AddUnique(Client);
		FAngelscriptEngine::Get().UpdateLineCallbackState();
	}
	else if (MessageType == EDebugMessageType::StopDebugging)
	{
		bIsDebugging = false;
		bPauseRequested = false;
		bIsPaused = false;
		bBreakNextScriptLine = false;
		ClearAllBreakpoints();

		ClientsThatAreDebugging.Remove(Client);
		FAngelscriptEngine::Get().UpdateLineCallbackState();
	}
	else if (MessageType == EDebugMessageType::RequestCallStack)
	{
		CallstackRequests.Add(Client);
	}
	else if (MessageType == EDebugMessageType::ClearBreakpoints)
	{
		FAngelscriptClearBreakpoints BP;
		*Datagram << BP;

		BP.Filename = CanonizeFilename(BP.Filename);
		
		auto& Manager = FAngelscriptEngine::Get();
		auto ModuleDesc = Manager.GetModuleByFilenameOrModuleName(BP.Filename, BP.ModuleName);

		const FString& Key = ModuleDesc.IsValid() ? ModuleDesc->ModuleName : BP.Filename;
		TSharedPtr<FFileBreakpoints>& Active = Breakpoints.FindOrAdd(Key);
		if (!Active.IsValid())
		{
			Active = MakeShared<FFileBreakpoints>();
		}

		BreakpointCount -= Active->Lines.Num();
		Active->Lines.Empty();
		Active->Conditions.Empty();

		if (Active->Module.IsValid())
		{
			auto* Module = (asCModule*)Active->Module->ScriptModule;
			if (Module != nullptr)
				Module->hasBreakPoints = false;
		}
	}
	else if (MessageType == EDebugMessageType::SetBreakpoint)
	{
		FAngelscriptBreakpoint BP;
		*Datagram << BP;
		FString OriginalFilename = BP.Filename;
		BP.Filename = CanonizeFilename(BP.Filename);
		
		auto& Manager = FAngelscriptEngine::Get();
		auto ModuleDesc = Manager.GetModuleByFilenameOrModuleName(BP.Filename, BP.ModuleName);

		// If the module wasn't found, use the filename as a fallback so the rest of the
		// breakpoint bookkeeping logic has an FFileBreakpoints struct to use.
		const FString& Key = ModuleDesc.IsValid() ? ModuleDesc->ModuleName : BP.Filename;
		TSharedPtr<FFileBreakpoints>& Active = Breakpoints.FindOrAdd(Key);
		if (!Active.IsValid())
		{
			Active = MakeShared<FFileBreakpoints>();
		}

		int32 WantedLine = BP.LineNumber;
		int32 CodeLine = -1;

		// Find the next code line in the specified file
		if (!ModuleDesc.IsValid())
		{
			UE_LOG(Angelscript, Warning, TEXT("Breakpoint on file '%s' could not find module to detect code from. Filename was canonized to '%s'."),
				*OriginalFilename, *BP.Filename);
			CodeLine = WantedLine;
		}
		else
		{
			Active->Module = ModuleDesc;

			asCModule* FoundModule = (asCModule*)ModuleDesc->ScriptModule;
			if (FoundModule != nullptr)
			{
				FoundModule->hasBreakPoints = true;
				int32 BestLine = -1;

				// Find the script function that is closest after this line
				for (int32 i = 0, Count = FoundModule->scriptFunctions.GetLength(); i < Count; ++i)
				{
					asCScriptFunction* Func = FoundModule->scriptFunctions[i];
					int32 LineInFunc = Func->FindNextLineWithCode(WantedLine);
					if (LineInFunc == -1)
						continue;
					if (LineInFunc < WantedLine)
						continue;

					if (BestLine == -1 || (LineInFunc - WantedLine) < (BestLine - WantedLine))
						BestLine = LineInFunc;
				}

				if (BestLine != -1)
				{
					CodeLine = BestLine;
				}
				else
				{
					// If we can't delete the breakpoint fallback to setting it anyway
					if (BP.Id != -1)
						CodeLine = -1;
					else
						CodeLine = WantedLine;
				}
			}
			else
			{
				// Fall back to trying the breakpoint at the exact line
				CodeLine = WantedLine;
			}
		}

		const bool bDuplicateBreakpoint = CodeLine != -1 && Active->Lines.Contains(CodeLine);

		// Add the breakpoint
		if (CodeLine != -1 && !bDuplicateBreakpoint)
		{
			Active->Lines.Add(CodeLine);
			BreakpointCount += 1;

			if (BP.Condition.IsEmpty())
			{
				Active->Conditions.Remove(CodeLine);
			}
			else
			{
				Active->Conditions.Add(CodeLine, BP.Condition);
			}

			if (CodeLine != WantedLine && BP.Id != -1)
			{
				// We changed the code line that this breakpoint is on so
				// send a changed event back
				FAngelscriptBreakpoint ChangedBP;
				ChangedBP.Filename = OriginalFilename;
				ChangedBP.LineNumber = CodeLine;
				ChangedBP.Id = BP.Id;
				ChangedBP.Condition = BP.Condition;
				SendMessageToClient(Client, EDebugMessageType::SetBreakpoint, ChangedBP);
			}
		}
		else
		{
			// Code line already had a breakpoint or is impossible to find, send a breakpoint removal message back
			if (BP.Id != -1)
			{
				FAngelscriptBreakpoint ChangedBP;
				ChangedBP.Filename = OriginalFilename;
				ChangedBP.LineNumber = -1;
				ChangedBP.Id = BP.Id;
				ChangedBP.Condition = BP.Condition;
				SendMessageToClient(Client, EDebugMessageType::SetBreakpoint, ChangedBP);
			}
		}
	}
	else if (MessageType == EDebugMessageType::SetDataBreakpoints)
	{
		FAngelscriptDataBreakpoints BP;
		*Datagram << BP;
		DataBreakpoints = BP.Breakpoints;

		UpdateDataBreakpoints();
	}
	else if (MessageType == EDebugMessageType::ClearDataBreakpoints)
	{
		ClearAllDataBreakpoints();
	}
	else if (MessageType == EDebugMessageType::EngineBreak)
	{
		if (bIsPaused)
		{
			UE_DEBUG_BREAK();
			bIsPaused = false;
		}
	}
	else if (MessageType == EDebugMessageType::RequestVariables)
	{
		FString Path;
		*Datagram << Path;

		FAngelscriptVariables Vars;
		FDebuggerScope Scope;

		if (GetDebuggerScope(Path, Scope))
		{
			for (auto& Value : Scope.Values)
			{
				FAngelscriptVariable Var;
				Var.Name = Value.Name;
				Var.Value = Value.Value;
				Var.Type = Value.Type;
				Var.ValueAddress = reinterpret_cast<uint64>(Value.GetAddressToMonitor());
				Var.ValueSize = Value.GetAddressToMonitorValueSize();
				Var.bHasMembers = Value.bHasMembers;

				Vars.Variables.Add(Var);
			}
		}

		SendMessageToClient(Client, EDebugMessageType::Variables, Vars);
	}
	else if (MessageType == EDebugMessageType::RequestEvaluate)
	{
		FString Path;
		*Datagram << Path;

		int32 DefaultFrame;
		*Datagram << DefaultFrame;

		FAngelscriptVariable Var;
		FDebuggerValue Value;

		if (GetDebuggerValue(Path, Value, &DefaultFrame))
		{
			Var.Name = Value.Name;
			Var.Value = Value.Value;
			Var.Type = Value.Type;
			Var.ValueAddress = reinterpret_cast<uint64>(Value.GetAddressToMonitor());
			Var.ValueSize = Value.GetAddressToMonitorValueSize();
			Var.bHasMembers = Value.bHasMembers;
		}

		SendMessageToClient(Client, EDebugMessageType::Evaluate, Var);
	}
	else if (MessageType == EDebugMessageType::GoToDefinition)
	{
		FAngelscriptGoToDefinition GoTo;
		*Datagram << GoTo;

		GoToDefinition(GoTo);
	}
	else if (MessageType == EDebugMessageType::BreakOptions)
	{
		FAngelscriptBreakOptions Options;
		*Datagram << Options;

		BreakOptions.Empty();
		for (auto Filter : Options.Filters)
			BreakOptions.Add(FName(*Filter));
		BreakOptions.Add(FName("break:any"));
	}
	else if (MessageType == EDebugMessageType::RequestBreakFilters)
	{
		TMap<FName, FString> FilterList;
		FAngelscriptEngine& Engine = OwnerEngine != nullptr ? *OwnerEngine : FAngelscriptEngine::Get();
		Engine.GetDebugBreakFilters().ExecuteIfBound(FilterList);

		FAngelscriptBreakFilters Filters;
		for (auto& Elem : FilterList)
		{
			Filters.Filters.Add(Elem.Key.ToString());
			Filters.FilterTitles.Add(Elem.Value);
		}

		SendMessageToClient(Client, EDebugMessageType::BreakFilters, Filters);
	}
	else if (MessageType == EDebugMessageType::FindAssets)
	{
		FAngelscriptFindAssets AssetList;
		*Datagram << AssetList;

		UASClass* BaseClass = nullptr;
		auto ClassDesc = FAngelscriptEngine::Get().GetClass(AssetList.ClassName);
		if (ClassDesc.IsValid() && Cast<UASClass>(ClassDesc->Class) != nullptr)
			BaseClass = Cast<UASClass>(ClassDesc->Class);
		FAngelscriptEditorDebugBridge::GetDebugListAssets().Broadcast(AssetList.Assets, BaseClass);
	}
	else if (MessageType == EDebugMessageType::CreateBlueprint)
	{
		FAngelscriptCreateBlueprint CreateBlueprint;
		*Datagram << CreateBlueprint;

		auto ClassDesc = FAngelscriptEngine::Get().GetClass(CreateBlueprint.ClassName);
		if (ClassDesc.IsValid() && Cast<UASClass>(ClassDesc->Class) != nullptr)
		{
			FAngelscriptEditorDebugBridge::GetEditorCreateBlueprint().Broadcast(Cast<UASClass>(ClassDesc->Class));
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				FString::Printf(
					TEXT("Cannot create blueprint/asset: class %s does not exist.\nHas the script file been saved?"),
					*CreateBlueprint.ClassName)
			));
		}
	}
	else if (MessageType == EDebugMessageType::Disconnect)
	{
		Client->Close();
	}
}

void FAngelscriptDebugServer::ReapplyBreakpoints()
{
	auto& Manager = FAngelscriptEngine::Get();
	for (auto& BreakpointElem : Breakpoints)
	{
		auto FileBreakpoints = BreakpointElem.Value;
		if (FileBreakpoints->Lines.Num() == 0)
		{
			FileBreakpoints->Module = nullptr;
		}
		else
		{
			auto ModuleDesc = Manager.GetModuleByModuleName(BreakpointElem.Key);
			FileBreakpoints->Module = ModuleDesc;

			if (ModuleDesc.IsValid())
			{
				auto* ScriptModule = (asCModule*)ModuleDesc->ScriptModule;
				if (ScriptModule != nullptr)
					ScriptModule->hasBreakPoints = true;
			}
		}
	}
}

void FAngelscriptDebugServer::ClearAllBreakpoints()
{
	for (auto& Elem : Breakpoints)
	{
		if (Elem.Value->Module.IsValid())
		{
			auto* Module = (asCModule*)Elem.Value->Module->ScriptModule;
			if (Module != nullptr)
				Module->hasBreakPoints = false;
		}
	}
	Breakpoints.Empty();
	SectionBreakpoints.Empty();
	BreakpointCount = 0;

	// Clear all Data Breakpoints as well
	ClearAllDataBreakpoints();
}

void FAngelscriptDebugServer::ClearAllDataBreakpoints()
{
	DataBreakpoints.Reset();
	RebuildActiveDataBreakpoints();
	UpdateDataBreakpoints();
}

void FAngelscriptDebugServer::RebuildActiveDataBreakpoints()
{
	ActiveDataBreakpointCount.Store(0);

	const int32 BreakpointCountToCopy = FMath::Min(DataBreakpoints.Num(), DATA_BREAKPOINT_HARDWARE_LIMIT);
	for (int32 BreakpointIndex = 0; BreakpointIndex < BreakpointCountToCopy; ++BreakpointIndex)
	{
		ActiveDataBreakpoints[BreakpointIndex].CopyFrom(DataBreakpoints[BreakpointIndex]);
	}

	for (int32 BreakpointIndex = BreakpointCountToCopy; BreakpointIndex < DATA_BREAKPOINT_HARDWARE_LIMIT; ++BreakpointIndex)
	{
		ActiveDataBreakpoints[BreakpointIndex].Reset();
	}

	ActiveDataBreakpointCount.Store(BreakpointCountToCopy);
}

void FAngelscriptDebugServer::SyncActiveDataBreakpointsToAuthoritativeState()
{
	const int32 BreakpointCountToSync = FMath::Min(DataBreakpoints.Num(), ActiveDataBreakpointCount.Load());
	for (int32 BreakpointIndex = 0; BreakpointIndex < BreakpointCountToSync; ++BreakpointIndex)
	{
		ActiveDataBreakpoints[BreakpointIndex].CopyTo(DataBreakpoints[BreakpointIndex]);
	}
}

void FAngelscriptDebugServer::UpdateDataBreakpoints()
{
	RebuildActiveDataBreakpoints();

#if PLATFORM_WINDOWS && WITH_AS_DEBUGSERVER
	{
		DataBreakpoint_Windows::FUpdateDebugRegisterThread UpdateDebugRegisters(DataBreakpoint_Windows::GetThreadAgnosticCurrentThreadHandle(), DataBreakpoints);
	}
#endif

	FAngelscriptEngine::Get().UpdateLineCallbackState();
}

void FAngelscriptDebugServer::GoToDefinition(const FAngelscriptGoToDefinition GoTo)
{
#if WITH_EDITOR
	auto* Engine = FAngelscriptEngine::Get().Engine;
	UE_LOG(Angelscript, Warning, TEXT("GoToDefinition: [%s, %s]"), *GoTo.TypeName, *GoTo.SymbolName);


	asIScriptFunction* ScriptFunction = nullptr;

	// Global functions in a namespace
	if (ScriptFunction == nullptr && GoTo.TypeName.StartsWith(TEXT("__")))
	{
		FString Namespace = GoTo.TypeName.Mid(2);

		auto AnsiNamespace = StringCast<ANSICHAR>(*Namespace);
		auto AnsiSymbol = StringCast<ANSICHAR>(*GoTo.SymbolName);

		int32 FuncCount = Engine->GetGlobalFunctionCount();
		for (int32 i = 0; i < FuncCount; ++i)
		{
			asIScriptFunction* Func = Engine->GetGlobalFunctionByIndex(i);
			if (FCStringAnsi::Strcmp(Func->GetNamespace(), AnsiNamespace.Get()) == 0
				&& FCStringAnsi::Strcmp(Func->GetName(), AnsiSymbol.Get()) == 0)
			{
				ScriptFunction = Func;
				break;
			}
		}
	}

	// Lookups of functions in C++ bound types
	asITypeInfo* TypeInfo = Engine->GetTypeInfoByName(TCHAR_TO_ANSI(*GoTo.TypeName));
	if (TypeInfo != nullptr && ScriptFunction == nullptr)
	{
		auto AnsiSymbol = StringCast<ANSICHAR>(*GoTo.SymbolName);
		int32 Methods = TypeInfo->GetMethodCount();
		for (int32 i = 0; i < Methods; ++i)
		{
			asIScriptFunction* Method = TypeInfo->GetMethodByIndex(i);
			if (FCStringAnsi::Strcmp(Method->GetName(), AnsiSymbol.Get()) == 0)
			{
				ScriptFunction = Method;
				break;
			}
		}
	}

	if (ScriptFunction != nullptr)
	{
		UFunction* UnrealFunction = FAngelscriptDocs::LookupAngelscriptFunction(ScriptFunction->GetId());
		if (UnrealFunction != nullptr)
		{
			FSourceCodeNavigation::NavigateToFunction(UnrealFunction);
			return;
		}
	}

	// Could be a property inside a class
	TSharedPtr<FAngelscriptType> Type = FAngelscriptType::GetByAngelscriptTypeName(GoTo.TypeName);
	if (Type.IsValid())
	{
		UClass* AssociatedClass = Type->GetClass(FAngelscriptTypeUsage::DefaultUsage);
		if (AssociatedClass != nullptr)
		{
			FProperty* Property = AssociatedClass->FindPropertyByName(*GoTo.SymbolName);
			if (Property != nullptr)
			{
				FSourceCodeNavigation::NavigateToProperty(Property);
				return;
			}
		}
	}

	// Direct type lookups
	{
		TSharedPtr<FAngelscriptType> SymbolType = FAngelscriptType::GetByAngelscriptTypeName(GoTo.SymbolName);
		if (SymbolType.IsValid())
		{
			UClass* AssociatedClass = SymbolType->GetClass(FAngelscriptTypeUsage::DefaultUsage);
			if (AssociatedClass != nullptr)
			{
				FSourceCodeNavigation::NavigateToClass(AssociatedClass);
				return;
			}
		}
	}
#endif
}

FString FAngelscriptDebugServer::CanonizeFilename(const FString& Filename)
{
	return Filename.Replace(TEXT("\\"), TEXT("/"));
}

void FAngelscriptDebugServer::SendCallStack(FSocket* Client)
{
	FAngelscriptCallStack Stack;

	auto* Context = (asCContext*)asGetActiveContext();
	if (Context != nullptr)
	{
		int32 StackSize = Context->GetCallstackSize();

#if DO_BLUEPRINT_GUARD
		auto* BPStack = FBlueprintContextTracker::TryGet();
		int BPStackIndex = 0;
#endif

		for (int32 i = StackSize-1; i >= 0; --i)
		{
			auto* ScriptFunction = (asCScriptFunction*)Context->GetFunction(i);

			FAngelscriptCallFrame Frame;

			// Append any blueprint stack frames if we have them
#if DO_BLUEPRINT_GUARD
			int BPFrame = Context->GetBlueprintCallstackFrame(i);
			for (; BPStackIndex < BPFrame; ++BPStackIndex)
			{
				if (BPStack == nullptr)
					continue;

				auto StackView = BPStack->GetCurrentScriptStack();
				if (!StackView.IsValidIndex(BPStackIndex))
					continue;

				UFunction* Function = StackView[BPStackIndex]->Node;
				if (Function == nullptr || IsAngelscriptGenerated(Function))
					continue;

				if (Function->HasAnyFunctionFlags(FUNC_UbergraphFunction))
				{
					if (BPStackIndex > 0)
						continue;

					Frame.Name = FString::Printf(TEXT("(BP) %s Graph"), *Function->GetOuter()->GetName());
				}
				else
				{
#if WITH_EDITOR
					Frame.Name = FString::Printf(TEXT("(BP) %s"), *Function->GetDisplayNameText().ToString());
#else
					Frame.Name = FString::Printf(TEXT("(BP) %s"), *Function->GetName());
#endif
			}

				Frame.Source = FString::Printf(TEXT("::%s"), *Function->GetOuter()->GetName());
				if (AngelscriptDebugServer::DebugAdapterVersion >= 1)
					Frame.ModuleName = TEXT("");
				Stack.Frames.Insert(Frame, 0);
			}
#endif

			// Append the script stack frame
			if (ScriptFunction != nullptr)
			{
				if (ScriptFunction->traits.GetTrait(asTRAIT_GENERATED_FUNCTION))
				{
					continue;
				}

				if (ScriptFunction->GetFuncType() == asEFuncType::asFUNC_SYSTEM)
				{
					FString FunctionName = ANSI_TO_TCHAR(ScriptFunction->GetName());
					if (!FunctionName.StartsWith(TEXT("__")))
					{
						Frame.Name = FString::Printf(TEXT("(C++) %s"), *FunctionName);
						if (ScriptFunction->GetObjectType() != nullptr)
							Frame.Source = FString::Printf(TEXT("::%s"), ANSI_TO_TCHAR(ScriptFunction->GetObjectType()->GetName()));
						else
							Frame.Source = FString::Printf(TEXT("::%s"), ANSI_TO_TCHAR(ScriptFunction->GetNamespace()));
						if (AngelscriptDebugServer::DebugAdapterVersion >= 1)
							Frame.ModuleName = TEXT("");

						Stack.Frames.Insert(Frame, 0);
					}
				}
				else
				{
					Frame.Name = ANSI_TO_TCHAR(ScriptFunction->GetName());

					const char* SectionName = nullptr;
					Frame.LineNumber = Context->GetLineNumber(i, nullptr, &SectionName);
					Frame.Source = SectionName ? ANSI_TO_TCHAR(SectionName) : TEXT("");

					if (AngelscriptDebugServer::DebugAdapterVersion >= 1)
						Frame.ModuleName = ScriptFunction->GetModuleName() ? ANSI_TO_TCHAR(ScriptFunction->GetModuleName()) : TEXT("");

					Stack.Frames.Insert(Frame, 0);
				}
			}
		}
	}

	SendMessageToClient(Client, EDebugMessageType::CallStack, Stack);
}

void FAngelscriptDebugServer::BroadcastDebugDatabase()
{
	for (FSocket* Client : ClientsThatWantDebugDatabase)
	{
		SendDebugDatabase(Client);
	}
}

void FAngelscriptDebugServer::SendDebugDatabase(FSocket* Client)
{
	double StartTime = FPlatformTime::Seconds();

	auto* ScriptEngine = FAngelscriptEngine::Get().Engine;

	FAngelscriptDebugDatabaseSettings DebugSettings;
	DebugSettings.bAutomaticImports = FAngelscriptEngine::Get().ShouldUseAutomaticImportMethod();
	DebugSettings.bFloatIsFloat64 = GetDefault<UAngelscriptSettings>()->bScriptFloatIsFloat64;
	DebugSettings.bDeprecateStaticClass = GetDefault<UAngelscriptSettings>()->StaticClassDeprecation == EAngelscriptStaticClassMode::Deprecated;
	DebugSettings.bDisallowStaticClass = GetDefault<UAngelscriptSettings>()->StaticClassDeprecation == EAngelscriptStaticClassMode::Disallowed;
	SendMessageToClient(Client, EDebugMessageType::DebugDatabaseSettings, DebugSettings);

	auto Root = MakeShared<FJsonObject>();
	auto SendTypes = [&]()
	{
		FAngelscriptDebugDatabase DB;

		auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&DB.Database);
		FJsonSerializer::Serialize(Root, JsonWriter);

		SendMessageToClient(Client, EDebugMessageType::DebugDatabase, DB);

		Root = MakeShared<FJsonObject>();
	};

	auto GetDecl = [&](int TypeId, asDWORD* Flags = nullptr, bool bShowInRef = true) -> FString
	{
		if (TypeId == -1)
			return TEXT("?");

		const char* DeclRaw = ScriptEngine->GetTypeDeclaration(TypeId);
		FString Decl = ANSI_TO_TCHAR(DeclRaw);
		if (Flags != nullptr)
		{
			if ((*Flags & asTM_CONST) == 0)
			{
				if((*Flags & asTM_INOUTREF) == asTM_INOUTREF)
					Decl += TEXT("&");
				else if((*Flags & asTM_INOUTREF) == asTM_OUTREF)
					Decl += TEXT("&out");
				else if((*Flags & asTM_INOUTREF) == asTM_INREF)
					Decl += TEXT("&in");
			}
		}
		return Decl;
	};

	TArray<TSharedPtr<FJsonValueObject>> Constructors;
	int32 UnsentCount = 0;

	auto MakeFuncDesc = [&](asCScriptFunction* ScriptFunction) -> TSharedPtr<FJsonObject>
	{
		auto FuncDesc = MakeShared<FJsonObject>();

		int32 ArgCount = ScriptFunction->GetParamCount();
		int32 HiddenParam = ((asCScriptFunction*)ScriptFunction)->hiddenArgumentIndex;
		asCObjectType* ObjType = ((asCScriptFunction*)ScriptFunction)->objectType;

		asDWORD Flags;
		FString ReturnType = GetDecl(ScriptFunction->GetReturnTypeId(&Flags), &Flags);

		FString Name = ANSI_TO_TCHAR(ScriptFunction->GetName());
		if (Name.StartsWith(TEXT("__")))
			return nullptr;

		FuncDesc->SetStringField(TEXT("name"), Name);
		FuncDesc->SetStringField(TEXT("return"), ReturnType);

		if (ScriptFunction->GetObjectType() != nullptr && ScriptFunction->IsReadOnly())
			FuncDesc->SetBoolField(TEXT("const"), true);

		if (ScriptFunction->GetObjectType() != nullptr && ScriptFunction->IsProtected())
			FuncDesc->SetBoolField(TEXT("protected"), true);

		if (((asCScriptFunction*)ScriptFunction)->IsSetterAndOnlyValidDuringInitDefaults())
			FuncDesc->SetBoolField(TEXT("defaultsonly"), true);

		if (ScriptFunction->traits.GetTrait(asTRAIT_NOT_CALLABLE))
			FuncDesc->SetBoolField(TEXT("callable"), false);

		const FString& Doc = FAngelscriptDocs::GetUnrealDocumentation(ScriptFunction->GetId());
		if (Doc.Len() != 0)
			FuncDesc->SetStringField(TEXT("doc"), Doc);

		bool bParamsValid = true;
		bool bIsBlueprintEvent = false;

		UFunction* UnrealFunction = FAngelscriptDocs::LookupAngelscriptFunction(ScriptFunction->GetId());
		if (UnrealFunction != nullptr)
		{
			if (UnrealFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent))
			{
				FuncDesc->SetBoolField(TEXT("event"), true);
				bIsBlueprintEvent = true;
			}

			if (UnrealFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
			{
				FuncDesc->SetBoolField(TEXT("ufunction"), true);
			}

			if (Name != UnrealFunction->GetName())
				FuncDesc->SetStringField(TEXT("unrealname"), UnrealFunction->GetName());

#if WITH_EDITOR
			const FString& Keywords = UnrealFunction->GetMetaData(NAME_ScriptKeywords);
			if (Keywords.Len() != 0)
				FuncDesc->SetStringField(TEXT("keywords"), Keywords);

			TSharedPtr<FJsonObject> MetaObject;
			for (FName MetaTag : NAMES_InformedMeta)
			{
				const FString& MetaValue = UnrealFunction->GetMetaData(MetaTag);
				if (!MetaValue.IsEmpty())
				{
					if (!MetaObject.IsValid())
						MetaObject = MakeShared<FJsonObject>();
					MetaObject->SetStringField(MetaTag.ToString(), MetaValue);
				}
			}

			FuncDesc->SetObjectField(TEXT("meta"), MetaObject);
#endif
		}

		TArray<TSharedPtr<FJsonValue>> ArgDesc;
		for (int32 ArgIndex = 0; ArgIndex < ArgCount; ++ArgIndex)
		{
			if (ArgIndex == HiddenParam)
				continue;

			const char* ParamNameRaw;
			const char* ParamDefaultRaw;
			asDWORD ParamFlags;
			int ParamType;

			ScriptFunction->GetParam(ArgIndex, &ParamType, &ParamFlags, &ParamNameRaw, &ParamDefaultRaw);

			// Ignore parameters starting with __, these are usually internal only
			//  We don't need to check string length, because there would be a null in the first char
			if (ParamNameRaw != nullptr && ParamNameRaw[0] == '_' && ParamNameRaw[1] == '_')
				continue;

			auto& ScriptParam = ((asCScriptFunction*)ScriptFunction)->parameterTypes[ArgIndex];
			if (ScriptParam.GetTokenType() == ttQuestion)
			{
				auto ParamDesc = MakeShared<FJsonObject>();
				ParamDesc->SetStringField(TEXT("name"), ANSI_TO_TCHAR(ParamNameRaw));
				ParamDesc->SetStringField(TEXT("type"), TEXT("?"));
				ArgDesc.Add(MakeShared<FJsonValueObject>(ParamDesc));
				continue;
			}

			if (ParamType == -1)
			{
				bParamsValid = false;
				break;
			}

			auto ParamDesc = MakeShared<FJsonObject>();

			if (ParamNameRaw != nullptr)
			{
				FString ParamName = ANSI_TO_TCHAR(ParamNameRaw);
				ParamDesc->SetStringField(TEXT("name"), ParamName);
			}

			if (ParamDefaultRaw != nullptr)
			{
				FString ParamDefault = ANSI_TO_TCHAR(ParamDefaultRaw);
				if (ParamDefault.Len() != 0)
					ParamDesc->SetStringField(TEXT("default"), ParamDefault);
			}

			FString Decl = GetDecl(ParamType, &ParamFlags);

			// Check if we should add an &in to the type
			if ((ParamFlags & (asTM_INOUTREF | asTM_CONST)) == (asTM_INREF | asTM_CONST) && ObjType != nullptr && ObjType->templateSubTypes.GetLength() == 0)
				Decl = TEXT("const ") + Decl + TEXT("&in");

			if (bIsBlueprintEvent)
			{
				// Even if float is float64, we still want float32s expressed as 'float' in blueprint events,
				// the calling code will automatically extend unreal's float32 to a float64.
				if (ParamType == asTYPEID_FLOAT32 && (ParamFlags & asTM_INOUTREF) == 0)
					Decl = TEXT("float");
			}

			ParamDesc->SetStringField(TEXT("type"), Decl);

			ArgDesc.Add(MakeShared<FJsonValueObject>(ParamDesc));
		}

		FuncDesc->SetArrayField(TEXT("args"), ArgDesc);

		if (ScriptFunction->determinesOutputTypeArgumentIndex != -1)
		{
			if (ScriptFunction->hiddenArgumentIndex != -1 && ScriptFunction->hiddenArgumentIndex < ScriptFunction->determinesOutputTypeArgumentIndex)
				FuncDesc->SetNumberField(TEXT("outputTypeIndex"), ScriptFunction->determinesOutputTypeArgumentIndex-1);
			else
				FuncDesc->SetNumberField(TEXT("outputTypeIndex"), ScriptFunction->determinesOutputTypeArgumentIndex);
		}

		if (!ScriptFunction->IsProperty())
			FuncDesc->SetBoolField(TEXT("isProperty"), false);

		if (!bParamsValid)
			return nullptr;
		else
			return FuncDesc;
	};

	int32 TypeCount = ScriptEngine->GetObjectTypeCount();
	for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
	{
		auto* ScriptType = ScriptEngine->GetObjectTypeByIndex(TypeIndex);
		FString TypeName = ANSI_TO_TCHAR(ScriptType->GetName());
		int32 ClassTypeId = ScriptType->GetTypeId();
		UnsentCount += 1;

		if (TypeName.IsEmpty() || TypeName[0] == '_')
			continue;

		auto TypeDesc = MakeShared<FJsonObject>();
		auto Properties = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Methods;

		int32 PropertyCount = ScriptType->GetPropertyCount();
		for (int32 PropertyIndex = 0; PropertyIndex < PropertyCount; ++PropertyIndex)
		{
			const char* Name;
			int TypeId;
			int Offset;
			ScriptType->GetProperty(PropertyIndex, &Name, &TypeId, nullptr, nullptr, &Offset);
			asCObjectProperty* ObjProp = ((asCObjectType*)ScriptType)->properties[PropertyIndex];

#if WITH_EDITOR
			if (ObjProp->isDeprecated)
				continue;
#endif

			FString Decl = GetDecl(TypeId);

			TArray<TSharedPtr<FJsonValue>> PropDesc;
			PropDesc.Add(MakeShared<FJsonValueString>(*Decl));

			int Flags = 0;
			if ((ObjProp->Writeable() || ObjProp->Readable()) && !ObjProp->Editable())
				Flags |= 0x1;
			else if (!(ObjProp->Writeable() || ObjProp->Readable()) && ObjProp->Editable())
				Flags |= 0x2;
			if (ObjProp->isProtected)
				Flags |= 0x4;
			if (Flags != 0)
				PropDesc.Add(MakeShared<FJsonValueNumber>(Flags));

			const FString& Doc = FAngelscriptDocs::GetUnrealDocumentationForProperty(ClassTypeId, Offset);

			// TODO: Remove the Flags check here once we can be reasonably certain everyone has updated vscode extension versions
			// It's needed because older versions of the vscode extension assume that the second input is a string
			if (Doc.Len() != 0 || Flags != 0)
				PropDesc.Add(MakeShared<FJsonValueString>(Doc));

			UnsentCount += 1;

			Properties->SetArrayField(ANSI_TO_TCHAR(Name), PropDesc);
		}


		int32 MethodCount = ScriptType->GetMethodCount();
		for (int32 MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
		{
			auto* ScriptFunction = (asCScriptFunction*)ScriptType->GetMethodByIndex(MethodIndex);
			if (ScriptFunction == nullptr)
				continue;
			if (ScriptFunction->traits.GetTrait(asTRAIT_DEPRECATED))
				continue;

			if (ScriptFunction->traits.GetTrait(asTRAIT_GENERATED_FUNCTION))
			{
				FString PropertyName = ANSI_TO_TCHAR(ScriptFunction->GetName());
				int TypeId = -1;

				if (PropertyName.StartsWith(TEXT("Get")))
				{
					PropertyName = PropertyName.Mid(3);
					TypeId = ScriptEngine->GetTypeIdFromDataType(ScriptFunction->returnType);
				}
				else if (PropertyName.StartsWith(TEXT("Set")))
				{
					PropertyName = PropertyName.Mid(3);
					TypeId = ScriptEngine->GetTypeIdFromDataType(ScriptFunction->parameterTypes[0]);
				}

				if (TypeId == -1)
					continue;

				// If the property is already documented, don't add it again
				if (Properties->HasField(PropertyName))
					continue;

				// Document the generated accessor function as if it is a property, to be transparent
				// and match what is in C++
				FString Decl = GetDecl(TypeId);

				TArray<TSharedPtr<FJsonValue>> PropDesc;
				PropDesc.Add(MakeShared<FJsonValueString>(*Decl));

				const FString& Doc = FAngelscriptDocs::GetUnrealDocumentation(ScriptFunction->GetId());
				if (Doc.Len() != 0 )
					PropDesc.Add(MakeShared<FJsonValueString>(Doc));

				UnsentCount += 1;

				Properties->SetArrayField(PropertyName, PropDesc);
			}
			else
			{
				auto FuncDesc = MakeFuncDesc(ScriptFunction);
				if (FuncDesc.IsValid())
				{
					Methods.Add(MakeShared<FJsonValueObject>(FuncDesc));
					UnsentCount += 1;
				}
			}
		}

		int32 BehCount = ScriptType->GetBehaviourCount();
		for (int32 BehIndex = 0; BehIndex < BehCount; ++BehIndex)
		{
			asEBehaviours BehType;
			auto* ScriptFunction = (asCScriptFunction*)ScriptType->GetBehaviourByIndex(BehIndex, &BehType);
			if (ScriptFunction->traits.GetTrait(asTRAIT_DEPRECATED))
				continue;

			if ((ScriptType->GetFlags() & asOBJ_REF) != 0)
			{
				if (BehType != asEBehaviours::asBEHAVE_FACTORY)
					continue;
			}
			else
			{
				if (BehType != asEBehaviours::asBEHAVE_CONSTRUCT)
					continue;
			}

			auto FuncDesc = MakeFuncDesc(ScriptFunction);
			if (FuncDesc.IsValid())
			{
				FuncDesc->SetStringField(TEXT("name"), TypeName);
				FuncDesc->SetStringField(TEXT("return"), TypeName);
				FuncDesc->SetBoolField(TEXT("isConstructor"), true);
				Constructors.Add(MakeShared<FJsonValueObject>(FuncDesc));
			}
		}

		if (ScriptType->GetSubTypeCount() != 0)
		{
			TArray<TSharedPtr<FJsonValue>> SubTypes;
			for (int32 i = 0, Count = ScriptType->GetSubTypeCount(); i < Count; ++i)
			{
				asITypeInfo* SubType = ScriptType->GetSubType(i);
				SubTypes.Add(MakeShared<FJsonValueString>(ANSI_TO_TCHAR(SubType->GetName())));
			}

			TypeDesc->SetArrayField(TEXT("subtypes"), SubTypes);
		}

		auto ASType = FAngelscriptType::GetByAngelscriptTypeName(TypeName);
		if (ASType.IsValid())
		{
			UClass* ClassPtr = ASType->GetClass(FAngelscriptTypeUsage::DefaultUsage);
			if (ClassPtr != nullptr)
			{
				UClass* SuperPtr = ClassPtr->GetSuperClass();
				if (SuperPtr != nullptr)
				{
					FString SuperName = FAngelscriptType::GetBoundClassName(SuperPtr);
					TypeDesc->SetStringField(TEXT("inherits"), SuperName);
				}
			}
		}

		if ((ScriptType->GetFlags() & asOBJ_VALUE) != 0)
			TypeDesc->SetBoolField(TEXT("isStruct"), true);

		const FString& TypeDoc = FAngelscriptDocs::GetUnrealDocumentationForType(ClassTypeId);
		if (TypeDoc.Len() != 0)
			TypeDesc->SetStringField(TEXT("doc"), TypeDoc);

		if (ScriptType->GetFlags() & asOBJ_REF)
		{
			UClass* UnrealClass = (UClass*)ScriptType->GetUserData();
			if (UnrealClass)
			{
#if WITH_EDITOR
				const FString& Keywords = UnrealClass->GetMetaData(NAME_ScriptKeywords);
				if (Keywords.Len() != 0)
					TypeDesc->SetStringField(TEXT("keywords"), Keywords);
#endif
			}
		}

		TypeDesc->SetObjectField(TEXT("properties"), Properties);
		TypeDesc->SetArrayField(TEXT("methods"), Methods);
		Root->SetObjectField(TypeName, TypeDesc);

		const bool bShouldSend = UnsentCount > 10;
		if (bShouldSend)
		{
			SendTypes();
			UnsentCount = 0;
		}
	}

	struct FNS
	{
		TArray<asCScriptFunction*> Functions;
		TArray<asCGlobalProperty*> Variables;
		TArray<TPair<FString,FString>> Enums;
		FString Keywords;
		FString Doc;
	};

	TMap<FString, FNS> NSFunctions;

	int32 GlobalFuncCount = ScriptEngine->GetGlobalFunctionCount();
	for (int32 GlobalFuncIndex = 0; GlobalFuncIndex < GlobalFuncCount; ++GlobalFuncIndex)
	{
		asCScriptFunction* Func = (asCScriptFunction*)ScriptEngine->GetGlobalFunctionByIndex(GlobalFuncIndex);
		if (Func->traits.GetTrait(asTRAIT_DEPRECATED))
			continue;
		NSFunctions.FindOrAdd(ANSI_TO_TCHAR(Func->GetNamespace())).Functions.Add(Func);
	}

	int32 GlobalVarCount = ScriptEngine->registeredGlobalProps.GetLength();
	for (int32 GlobalVarIndex = 0; GlobalVarIndex < GlobalVarCount; ++GlobalVarIndex)
	{
		asCGlobalProperty* Prop = ScriptEngine->registeredGlobalProps[GlobalVarIndex];
		NSFunctions.FindOrAdd(ANSI_TO_TCHAR(Prop->nameSpace->GetName())).Variables.Add(Prop);
	}

	int32 EnumCount = ScriptEngine->GetEnumCount();
	for (int32 EnumIndex = 0; EnumIndex < EnumCount; ++EnumIndex)
	{
		auto* EnumType = (asCTypeInfo*)ScriptEngine->GetEnumByIndex(EnumIndex);
		auto& NS = NSFunctions.FindOrAdd(ANSI_TO_TCHAR(EnumType->GetName()));
		UEnum* UnrealEnum = (UEnum*)EnumType->GetUserData();

		for (int32 i = 0, Count = EnumType->GetEnumValueCount(); i < Count; ++i)
		{
			int32 Value;
			const char* ValueName = EnumType->GetEnumValueByIndex(i, &Value);

			FString Doc;

#if WITH_EDITOR
			if (UnrealEnum != nullptr)
			{
				int32 ValueIndex = UnrealEnum->GetIndexByValue(Value);
				Doc = UnrealEnum->GetMetaData(TEXT("ToolTip"), ValueIndex);
			}
#endif

			NS.Enums.Add(TPair<FString,FString>(ANSI_TO_TCHAR(ValueName), Doc));
		}

		const FString& TypeDoc = FAngelscriptDocs::GetUnrealDocumentationForType(EnumType->GetTypeId());
		if (TypeDoc.Len() != 0)
			NS.Doc = TypeDoc;
	}

	for (auto& NS : NSFunctions)
	{
		auto NSDesc = MakeShared<FJsonObject>();

		auto Properties = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Methods;

		if (NS.Key.Len() == 0)
		{
			for (auto FuncDesc : Constructors)
			{
				Methods.Add(FuncDesc);
				UnsentCount += 1;
			}
		}
		
		for (asCScriptFunction* ScriptFunction : NS.Value.Functions)
		{
			auto FuncDesc = MakeFuncDesc(ScriptFunction);
			if (FuncDesc.IsValid())
			{
				Methods.Add(MakeShared<FJsonValueObject>(FuncDesc));
				UnsentCount += 1;
			}
		}

		for (asCGlobalProperty* GlobalProperty : NS.Value.Variables)
		{
			if (GlobalProperty->name[0] == '_')
				continue;

			FString Decl = GetDecl(ScriptEngine->GetTypeIdFromDataType(GlobalProperty->type));
			if (GlobalProperty->type.IsReadOnly())
				Decl = TEXT("const ") + Decl;

			TArray<TSharedPtr<FJsonValue>> PropDesc;
			PropDesc.Add(MakeShared<FJsonValueString>(Decl));

			const FString& Documentation = FAngelscriptDocs::GetDocumentationForGlobalVariable(GlobalProperty->id);
			if (!Documentation.IsEmpty())
				PropDesc.Add(MakeShared<FJsonValueString>(Documentation));

			UnsentCount += 1;

			Properties->SetArrayField(ANSI_TO_TCHAR(GlobalProperty->name.AddressOf()), PropDesc);
		}

		for (TPair<FString,FString>& EnumValue : NS.Value.Enums)
		{
			TArray<TSharedPtr<FJsonValue>> PropDesc;
			PropDesc.Add(MakeShared<FJsonValueString>(NS.Key));
			if (EnumValue.Value.Len() != 0)
				PropDesc.Add(MakeShared<FJsonValueString>(EnumValue.Value));
			UnsentCount += 1;

			Properties->SetArrayField(EnumValue.Key, PropDesc);
		}

		if (NS.Value.Enums.Num() != 0)
			NSDesc->SetBoolField(TEXT("isEnum"), true);
		if (NS.Value.Doc.Len() != 0)
			NSDesc->SetStringField(TEXT("doc"), NS.Value.Doc);
		if (NS.Value.Keywords.Len() != 0)
			NSDesc->SetStringField(TEXT("keywords"), NS.Value.Keywords);

		NSDesc->SetObjectField(TEXT("properties"), Properties);
		NSDesc->SetArrayField(TEXT("methods"), Methods);
		Root->SetObjectField(TEXT("__") + NS.Key, NSDesc);

		const bool bShouldSend = UnsentCount > 10;
		if (bShouldSend)
		{
			SendTypes();
			UnsentCount = 0;
		}
	}
	
	SendTypes();

	FEmptyMessage Message;
	SendMessageToClient(Client, EDebugMessageType::DebugDatabaseFinished, Message);

	SendAssetDatabase(Client);

	double EndTime = FPlatformTime::Seconds();
	UE_LOG(Angelscript, Log, TEXT("Sending debug database took %.3g seconds"), (EndTime - StartTime));
}

void AddAssetToMessage(FAngelscriptAssetDatabase& Message, const FAssetData& AssetData)
{
	// Send blueprints that inherit from a script class
	FString NativeClassName;
	if (AssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeClassName))
	{
		if (NativeClassName.StartsWith("/Script/AngelscriptRuntime.ASClass'/Script/Angelscript."))
		{
			FString AssetPath = AssetData.PackageName.ToString();
			FString ClassName = FPackageName::ObjectPathToObjectName(NativeClassName.Mid(32, NativeClassName.Len() - 33));

			Message.Assets.Emplace(MoveTemp(AssetPath));
			Message.Assets.Emplace(MoveTemp(ClassName));
		}
	}
	else
	{
		// Send any actual instances of a script class (data assets)
		Message.Assets.Emplace(AssetData.PackageName.ToString());
		Message.Assets.Emplace(AssetData.AssetClassPath.GetAssetName().ToString());
	}
}

void FAngelscriptDebugServer::BindAssetRegistry()
{
	if (bAssetRegistryBound)
		return;
	bAssetRegistryBound = true;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TFunction<void()> BindAssetRegistryChanges = [this]()
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// Sign up to sending newly changed assets to the clients
		FDelegateHandle AddedHandle = AssetRegistry.OnAssetAdded().AddLambda([this](const FAssetData& AssetData)
		{
			if (ClientsThatWantDebugDatabase.Num() == 0)
				return;
			// Skip external actor packages 
			if ((AssetData.PackageFlags & PKG_ContainsMapData) != 0)
				return;
			FAngelscriptAssetDatabase UpdateMessage;
			AddAssetToMessage(UpdateMessage, AssetData);

			for (auto* ConnectedClient : ClientsThatWantDebugDatabase)
				SendMessageToClient(ConnectedClient, EDebugMessageType::AssetDatabase, UpdateMessage);
		});
		AssetRegistryDelegateHandles.Add(AddedHandle);

		FDelegateHandle RemovedHandle = AssetRegistry.OnAssetRemoved().AddLambda([this](const FAssetData& AssetData)
		{
			if (ClientsThatWantDebugDatabase.Num() == 0)
				return;
			// Skip external actor packages 
			if ((AssetData.PackageFlags & PKG_ContainsMapData) != 0)
				return;
			FAngelscriptAssetDatabase UpdateMessage;
			UpdateMessage.Assets.Emplace(AssetData.PackageName.ToString());
			UpdateMessage.Assets.Emplace(FString());

			for (auto* ConnectedClient : ClientsThatWantDebugDatabase)
				SendMessageToClient(ConnectedClient, EDebugMessageType::AssetDatabase, UpdateMessage);
		});
		AssetRegistryDelegateHandles.Add(RemovedHandle);

		FDelegateHandle RenamedHandle = AssetRegistry.OnAssetRenamed().AddLambda([this](const FAssetData& AssetData, const FString& OldObjectPath)
		{
			if (ClientsThatWantDebugDatabase.Num() == 0)
				return;
			// Skip external actor packages 
			if ((AssetData.PackageFlags & PKG_ContainsMapData) != 0)
				return;
			FAngelscriptAssetDatabase UpdateMessage;
			UpdateMessage.Assets.Emplace(OldObjectPath);
			UpdateMessage.Assets.Emplace(FString());
			AddAssetToMessage(UpdateMessage, AssetData);

			for (auto* ConnectedClient : ClientsThatWantDebugDatabase)
				SendMessageToClient(ConnectedClient, EDebugMessageType::AssetDatabase, UpdateMessage);
		});
		AssetRegistryDelegateHandles.Add(RenamedHandle);
	};

	if (AssetRegistry.IsLoadingAssets())
	{
		//  Send the registry as soon as it becomes available
		FDelegateHandle FilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddLambda([this, BindAssetRegistryChanges]()
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			// Send asset registry to all clients that want it
			for (auto* ConnectedClient : ClientsThatWantDebugDatabase)
				SendAssetDatabase(ConnectedClient);

			BindAssetRegistryChanges();
		});
		AssetRegistryDelegateHandles.Add(FilesLoadedHandle);
	}
	else
	{
		// Already loaded, sign up to changes now
		BindAssetRegistryChanges();
	}
}

void FAngelscriptDebugServer::UnbindAssetRegistry()
{
	if (!bAssetRegistryBound)
		return;

	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		for (const FDelegateHandle& Handle : AssetRegistryDelegateHandles)
		{
			AssetRegistry.OnAssetAdded().Remove(Handle);
			AssetRegistry.OnAssetRemoved().Remove(Handle);
			AssetRegistry.OnAssetRenamed().Remove(Handle);
			AssetRegistry.OnFilesLoaded().Remove(Handle);
		}
	}

	AssetRegistryDelegateHandles.Empty();
	bAssetRegistryBound = false;
}

void FAngelscriptDebugServer::SendAssetDatabase(FSocket* Client)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if (!bAssetRegistryBound)
		BindAssetRegistry();

	if (AssetRegistry.IsLoadingAssets())
	{
		// Can't send asset registry yet, still loading
		return;
	}

	{
		FEmptyMessage InitMessage;
		SendMessageToClient(Client, EDebugMessageType::AssetDatabaseInit, InitMessage);
	}

	const FName ClassName_Blueprint("Blueprint");

	FAngelscriptAssetDatabase Message;
	AssetRegistry.EnumerateAllAssets([&](const FAssetData& AssetData) -> bool
	{
		// Skip external actor packages
		if ((AssetData.PackageFlags & PKG_ContainsMapData) != 0)
			return true;
		AddAssetToMessage(Message, AssetData);
		if (Message.Assets.Num() > 50)
		{
			SendMessageToClient(Client, EDebugMessageType::AssetDatabase, Message);
			Message.Assets.Reset();
		}
		return true;
	});

	if (Message.Assets.Num() != 0)
	{
		SendMessageToClient(Client, EDebugMessageType::AssetDatabase, Message);
		Message.Assets.Reset();
	}

	{
		FEmptyMessage FinishedMessage;
		SendMessageToClient(Client, EDebugMessageType::AssetDatabaseFinished, FinishedMessage);
	}
}

struct FExprElem
{
	FString Name;
	bool bIsSubscript = false;
};

static void ParseExpression(TArray<FExprElem>& Arr, const FString& Expr)
{
	int32 Pos = 0;
	int32 Len = Expr.Len();
	int32 TermStart = 0;

	int32 Depth = 0;
	while (Pos < Len)
	{
		auto Char = Expr[Pos];
		if (Char == '.')
		{
			if (Depth == 0)
			{
				if (TermStart != Pos)
				{
					FExprElem Elem;
					Elem.Name = Expr.Mid(TermStart, Pos - TermStart);
					Elem.bIsSubscript = false;
					Arr.Add(Elem);
				}

				TermStart = Pos + 1;
			}
		}
		else if (Char == '[')
		{
			if (Depth == 0)
			{
				if (TermStart != Pos)
				{
					FExprElem Elem;
					Elem.Name = Expr.Mid(TermStart, Pos - TermStart);
					Elem.bIsSubscript = false;
					Arr.Add(Elem);
				}

				TermStart = Pos + 1;
			}

			Depth += 1;
		}
		else if (Char == ']')
		{
			Depth -= 1;
			if (Depth == 0 && TermStart != Pos)
			{
				FExprElem Elem;
				Elem.Name = Expr.Mid(TermStart, Pos - TermStart);
				Elem.bIsSubscript = true;
				Arr.Add(Elem);

				TermStart = Pos + 1;
			}
		}

		++Pos;
	}

	if (TermStart != Pos)
	{
		FExprElem Elem;
		Elem.Name = Expr.Mid(TermStart, Pos - TermStart);
		Elem.bIsSubscript = false;
		Arr.Add(Elem);
	}
}

static const int FLAG_BlueprintFrame = 0x10000000;
int FAngelscriptDebugServer::ResolveDebuggerFrame(int DebuggerFrame)
{
	// Resolve the debugger frame number to either a script frame number or a blueprint frame number
	TArray<int, TInlineAllocator<32>> ResolvedFrames;

	auto* Context = (asCContext*)asGetActiveContext();
	if (Context != nullptr)
	{
		int32 StackSize = Context->GetCallstackSize();

#if DO_BLUEPRINT_GUARD
		auto* BPStack = FBlueprintContextTracker::TryGet();
		int BPStackIndex = 0;
#endif

		for (int32 i = StackSize-1; i >= 0; --i)
		{
			auto* ScriptFunction = (asCScriptFunction*)Context->GetFunction(i);
			FAngelscriptCallFrame Frame;

#if DO_BLUEPRINT_GUARD
			// Append any blueprint stack frames if we have them
			int BPFrame = Context->GetBlueprintCallstackFrame(i);
			for (; BPStackIndex < BPFrame; ++BPStackIndex)
			{
				if (BPStack == nullptr)
					continue;

				auto StackView = BPStack->GetCurrentScriptStack();
				if (!StackView.IsValidIndex(BPStackIndex))
					continue;

				UFunction* Function = StackView[BPStackIndex]->Node;
				if (Function == nullptr || IsAngelscriptGenerated(Function))
					continue;
				if (Function->HasAnyFunctionFlags(FUNC_UbergraphFunction) && BPStackIndex > 0)
					continue;

				ResolvedFrames.Insert(FLAG_BlueprintFrame | BPStackIndex, 0);
			}
#endif

			// Append the script stack frame
			if (ScriptFunction != nullptr)
			{
				if (ScriptFunction->GetFuncType() == asEFuncType::asFUNC_SYSTEM)
				{
					FString FunctionName = ANSI_TO_TCHAR(ScriptFunction->GetName());
					if (FunctionName.StartsWith(TEXT("__")))
						continue;
				}
				if (ScriptFunction->traits.GetTrait(asTRAIT_GENERATED_FUNCTION))
				{
					continue;
				}
				ResolvedFrames.Insert(i, 0);
			}
		}
	}

	if (ResolvedFrames.IsValidIndex(DebuggerFrame))
		return ResolvedFrames[DebuggerFrame];
	else
		return 0;
}

bool FAngelscriptDebugServer::GetDebuggerValue(const FString& Path, FDebuggerValue& CurrentValue, int32* InOutFrame, TArray<FDebuggerValue>* OutInnerValues)
{
	TGuardValue<bool> ScopeEvaluateWatch(bIsEvaluatingDebuggerWatch, true);
	StackFrameThis.Reset();

	int32 Frame = 0;
	if (InOutFrame != nullptr)
		Frame = *InOutFrame;

	FString NamePath = Path;

	int32 ColonIndex;
	if (Path.FindChar(':', ColonIndex))
	{
		if (!Path.IsValidIndex(ColonIndex+1) || Path[ColonIndex+1] != ':')
		{
			LexFromString(Frame, *Path.Left(ColonIndex));
			NamePath = Path.RightChop(ColonIndex+1);
		}
	}

	Frame = ResolveDebuggerFrame(Frame);

	TArray<FExprElem> Expr;
	ParseExpression(Expr, NamePath);

	if (InOutFrame != nullptr)
		*InOutFrame = Frame;

	if (Expr.Num() == 0)
		return false;

	bool bHasPrefix = false;
	bool bPrefixLocal = false;
	bool bPrefixThis = false;
	bool bPrefixModule = false;

	// Prefixes are parsed from the front of the names list
	if (Expr[0].Name == TEXT("%local%"))
	{
		bHasPrefix = true;
		bPrefixLocal = true;
	}
	else if (Expr[0].Name == TEXT("%this%"))
	{
		bHasPrefix = true;
		bPrefixThis = true;
	}
	else if (Expr[0].Name == TEXT("%module%"))
	{
		bHasPrefix = true;
		bPrefixModule = true;
	}

	if (bHasPrefix)
	{
		if (Expr.Num() == 1)
		{
			// This is a scope lookup, so create a dummy value that we can
			// lookup later.
			CurrentValue.Name = Expr[0].Name;
			CurrentValue.bHasMembers = true;
			return true;
		}

		if (bHasPrefix)
			Expr.RemoveAt(0);
	}

	auto* Context = (asCContext*)asGetActiveContext();
	bool bValidValue = false;

	if ((Frame & FLAG_BlueprintFrame) != 0)
	{
#if DO_BLUEPRINT_GUARD
		// If this is a blueprint stack frame, we only support evaluating the 'this' pointer
		int BPFrame = Frame & ~FLAG_BlueprintFrame;
		auto* BPStack = FBlueprintContextTracker::TryGet();

		if ((!bHasPrefix || bPrefixThis) && BPStack != nullptr && BPStack->GetCurrentScriptStack().IsValidIndex(BPFrame))
		{
			UObject* StackFrameObject = BPStack->GetCurrentScriptStack()[BPFrame]->Object;
			if (StackFrameObject != nullptr)
			{
				StackFrameThis.Add(StackFrameObject);
				void* Address = &StackFrameThis.Last();

				auto Usage = FAngelscriptTypeUsage::FromClass(UASClass::GetFirstASOrNativeClass(StackFrameObject->GetClass()));
				if (Expr[0].Name == TEXT("this"))
				{
					if (Usage.GetDebuggerValue(Address, CurrentValue))
						bValidValue = true;
				}
				else
				{
					if (Usage.GetDebuggerMember(Address, Expr[0].Name, CurrentValue))
						bValidValue = true;
				}
			}
		}
#endif
	}
	else
	{
		auto AnsiStartVar = StringCast<ANSICHAR>(*Expr[0].Name);

		// First lookup is special, it goes through scope detection
		// * Local variable Scope
		if (Context != nullptr && Frame < (int32)Context->GetCallstackSize() && (!bHasPrefix || bPrefixLocal))
		{
			int32 VarCount = Context->GetVarCount(Frame);
			for (int32 i = 0; i < VarCount; ++i)
			{
				if (!Context->IsVarInScope(i, Frame))
					continue;

				const char* VarName = Context->GetVarName(i, Frame);
				if (FCStringAnsi::Strcmp(VarName, AnsiStartVar.Get()) == 0)
				{
					auto Usage = FAngelscriptTypeUsage::FromTypeId(Context->GetVarTypeId(i, Frame));
					if (auto* VarAddress = Context->GetAddressOfVar(i, Frame))
					{
						if (Usage.GetDebuggerValue(VarAddress, CurrentValue))
						{
							bValidValue = true;
							break;
						}
					}
				}
			}
		}

		// * this Scope
		if (!bValidValue && Context != nullptr && Frame < (int32)Context->GetCallstackSize() && (!bHasPrefix || bPrefixThis))
		{
			int32 ThisTypeId = Context->GetThisTypeId(Frame);
			StackFrameThis.Add(ThisTypeId != 0 ? Context->GetThisPointer(Frame) : nullptr);
			if (StackFrameThis.Last() != nullptr)
			{
				auto Usage = FAngelscriptTypeUsage::FromTypeId(ThisTypeId);

				bool bIsHandle = true;
				if (Usage.ScriptClass != nullptr && (Usage.ScriptClass->GetFlags() & asOBJ_VALUE) != 0)
					bIsHandle = false;

				void* Address = bIsHandle ? &StackFrameThis.Last() : StackFrameThis.Last();

				if (Expr[0].Name == TEXT("this"))
				{
					if (Usage.GetDebuggerValue(Address, CurrentValue))
						bValidValue = true;
				}
				else
				{
					if (Usage.GetDebuggerMember(Address, Expr[0].Name, CurrentValue))
						bValidValue = true;
				}
			}
		}

		// * Module Scope
		if (!bValidValue && Context != nullptr && Frame < (int32)Context->GetCallstackSize() && (!bHasPrefix || bPrefixModule))
		{
			auto* Function = Context->GetFunction(Frame);
			auto* Module = Function != nullptr ? Function->GetModule() : nullptr;
			if (Module != nullptr)
			{
				int32 VarCount = Module->GetGlobalVarCount();
				for (int32 i = 0; i < VarCount; ++i)
				{
					const char* VarName;
					int32 TypeId;
					Module->GetGlobalVar(i, &VarName, nullptr, &TypeId);

					if (FCStringAnsi::Strcmp(VarName, AnsiStartVar.Get()) == 0)
					{
						auto Usage = FAngelscriptTypeUsage::FromTypeId(TypeId);

						if (Usage.GetDebuggerValue(Module->GetAddressOfGlobalVar(i), CurrentValue))
						{
							bValidValue = true;
							break;
						}
					}
				}
			}
		}
	}

	// * Global Scope
	if (!bValidValue && Context != nullptr)
	{
		auto* Engine = Context->m_engine;
		asSNameSpace* ns = Engine->defaultNamespace;

		FString VariableName = Expr[0].Name;
		FString NamespaceName;

		int32 SepIndex = -1;
		if (Expr[0].Name.FindLastChar(':', SepIndex))
		{
			if (SepIndex > 0 && Expr[0].Name[SepIndex-1] == ':')
			{
				VariableName = Expr[0].Name.Mid(SepIndex+1);
				NamespaceName = Expr[0].Name.Mid(0, SepIndex-1);

				ns = Engine->FindNameSpace(TCHAR_TO_ANSI(*NamespaceName));
			}
		}

		if (ns != nullptr)
		{
			auto AnsiVariableName = StringCast<ANSICHAR>(*VariableName);

			// Is it a registered global variable?
			asCGlobalProperty* GlobalProperty = Engine->registeredGlobalPropTable.FindFirst(AnsiVariableName.Get(), ns);

			// Is it a script global variable?
			if (GlobalProperty == nullptr)
			{
				GlobalProperty = Engine->allScriptGlobalVariables.FindFirst(AnsiVariableName.Get(), ns);
			}

			if (GlobalProperty != nullptr)
			{
				auto Usage = FAngelscriptTypeUsage::FromDataType(GlobalProperty->type);

				auto* GlobalAddress = GlobalProperty->GetAddressOfValue();
				if (GlobalAddress != nullptr)
				{
					// Value types are still stored as a pointer inside the global variable and allocated on the heap
					if (GlobalProperty->type.IsObject() && !GlobalProperty->type.IsObjectHandle())
						GlobalAddress = *(void**)GlobalAddress;

					if (GlobalAddress != nullptr && Usage.GetDebuggerValue(GlobalAddress, CurrentValue))
					{
						bValidValue = true;
					}
				}
			}

			// Is it a registered global property accessor?
			if (!bValidValue)
			{
				auto AnsiGetterName = StringCast<ANSICHAR>(*(TEXT("Get") + VariableName));
				asCScriptFunction* GlobalFunction = Engine->registeredGlobalFuncTable.FindFirst(AnsiGetterName.Get(), ns);

				// Is it a script global property accessor?
				if (GlobalFunction == nullptr)
				{
					GlobalFunction = Engine->allScriptGlobalFunctions.FindFirst(AnsiGetterName.Get(), ns);
				}

				if (GlobalFunction != nullptr)
				{
					if (FAngelscriptType::GetDebuggerValueFromFunction(GlobalFunction, nullptr, CurrentValue))
					{
						bValidValue = true;
					}
				}
			}
		}

		// See if it's an enum
		if (!bValidValue)
		{
			if (!NamespaceName.IsEmpty() && !VariableName.IsEmpty())
			{
				auto* EnumType = CastToEnumType((asCTypeInfo*)Engine->GetTypeInfoByName(TCHAR_TO_ANSI(*NamespaceName)));
				if (EnumType != nullptr)
				{
					for (int i = 0, Count = EnumType->enumValues.GetLength(); i < Count; ++i)
					{
						if (EnumType->enumValues[i]->name == TCHAR_TO_ANSI(*VariableName))
						{
							CurrentValue.Name = Expr[0].Name;
							CurrentValue.Type = NamespaceName;
							CurrentValue.Value = FString::Printf(TEXT("%d"), EnumType->enumValues[i]->value);
							bValidValue = true;
							break;
						}
					}
				}
			}
		}
	}

	if (bValidValue && CurrentValue.Name.IsEmpty())
	{
		CurrentValue.Name = Expr[0].Name;
	}

	// No value was found in scope, nothing to show
	if (!bValidValue)
		return false;

	// Subsequent lookups are done from the existing debug value
	TArray<FDebuggerValue> ValueStack;
	ValueStack.Add(MoveTemp(CurrentValue));

	for (int32 i = 1, Count = Expr.Num(); i < Count; ++i)
	{
		const FDebuggerValue& TopValue = ValueStack.Last();
		if (!TopValue.bHasMembers)
		{
			bValidValue = false;
			break;
		}

		FString MemberName = Expr[i].Name;
		if (Expr[i].bIsSubscript)
		{
			// Subscripts should resolve as values to become members if possible
			FDebuggerValue InnerValue;
			int32 InnerFrame = Frame;
			if (GetDebuggerValue(MemberName, InnerValue, &InnerFrame))
				MemberName = InnerValue.Value;

			MemberName = TEXT("[") + MemberName + TEXT("]");
		}

		FDebuggerValue NewValue;
		if (TopValue.Usage.GetDebuggerMember(TopValue.Address, MemberName, NewValue))
		{
			ValueStack.Add(MoveTemp(NewValue));
		}
		else
		{
			bValidValue = false;
			break;
		}
	}

	CurrentValue = MoveTemp(ValueStack.Last());
	ValueStack.Pop();

	if (OutInnerValues != nullptr)
	{
		*OutInnerValues = MoveTemp(ValueStack);
		return bValidValue;
	}
	else
	{
		if (!CurrentValue.bTemporaryValue
			&& CurrentValue.Address != nullptr
			&& CurrentValue.AddressToMonitor == nullptr)
		{
			CurrentValue.SetAddressToMonitor(CurrentValue.Address, CurrentValue.GetAddressToMonitorValueSize());
		}

		CurrentValue.Address = nullptr;
		CurrentValue.ClearLiteral();
		return bValidValue;
	}
}

bool FAngelscriptDebugServer::GetDebuggerScope(const FString& Path, FDebuggerScope& Scope)
{
	TGuardValue<bool> ScopeEvaluateWatch(bIsEvaluatingDebuggerWatch, true);
	StackFrameThis.Reset();

	int32 Frame;
	FDebuggerValue CurrentValue;
	TArray<FDebuggerValue> InnerValues;
	if (!GetDebuggerValue(Path, CurrentValue, &Frame, &InnerValues))
		return false;

	// Values without members aren't a scope
	if (!CurrentValue.bHasMembers)
		return false;

	auto* Context = asGetActiveContext();

	if ((Frame & FLAG_BlueprintFrame) != 0)
	{
#if DO_BLUEPRINT_GUARD
		// If this is a blueprint stack frame, we only support evaluating the 'this' pointer
		if (CurrentValue.Name == TEXT("%this%"))
		{
			int BPFrame = Frame & ~FLAG_BlueprintFrame;
			auto* BPStack = FBlueprintContextTracker::TryGet();

			if (BPStack != nullptr && BPStack->GetCurrentScriptStack().IsValidIndex(BPFrame))
			{
				UObject* StackFrameObject = BPStack->GetCurrentScriptStack()[BPFrame]->Object;
				if (StackFrameObject != nullptr)
				{
					StackFrameThis.Add(StackFrameObject);

					auto Usage = FAngelscriptTypeUsage::FromClass(UASClass::GetFirstASOrNativeClass(StackFrameObject->GetClass()));
					return Usage.GetDebuggerScope(&StackFrameThis.Last(), Scope);
				}
			}
		}
#endif
	}
	else
	{
		// Special scopes are implemented here
		// * Local variable Scope
		if (CurrentValue.Name == TEXT("%local%"))
		{
			if (Context != nullptr && Frame < (int32)Context->GetCallstackSize())
			{
				int32 VarCount = Context->GetVarCount(Frame);
				for (int32 i = 0; i < VarCount; ++i)
				{
					if (!Context->IsVarInScope(i, Frame))
						continue;

					const char* VarName = Context->GetVarName(i, Frame);
					auto Usage = FAngelscriptTypeUsage::FromTypeId(Context->GetVarTypeId(i, Frame));

					FDebuggerValue VarValue;
					if (void* VarAddress = Context->GetAddressOfVar(i, Frame))
					{
						if (Usage.GetDebuggerValue(VarAddress, VarValue))
						{
							VarValue.Name = ANSI_TO_TCHAR(VarName);
							Scope.Values.Add(MoveTemp(VarValue));
						}
					}
				}
			}

			return true;
		}

		// * this Scope
		if (CurrentValue.Name == TEXT("%this%"))
		{
			if (Context != nullptr && Frame < (int32)Context->GetCallstackSize())
			{
				int32 ThisTypeId = Context->GetThisTypeId(Frame);
				void* ThisPtr = ThisTypeId != 0 ? Context->GetThisPointer(Frame) : nullptr;
				if (ThisPtr != nullptr)
				{
					auto Usage = FAngelscriptTypeUsage::FromTypeId(ThisTypeId);

					bool bIsHandle = true;
					if (Usage.ScriptClass != nullptr && (Usage.ScriptClass->GetFlags() & asOBJ_VALUE) != 0)
						bIsHandle = false;

					void* Address = bIsHandle ? &ThisPtr : ThisPtr;
					return Usage.GetDebuggerScope(Address, Scope);
				}
			}

			return false;
		}

		// * Module Scope
		if (CurrentValue.Name == TEXT("%module%"))
		{
			if (Context != nullptr && Frame < (int32)Context->GetCallstackSize())
			{
				auto* Function = Context->GetFunction(Frame);
				auto* Module = Function != nullptr ? Function->GetModule() : nullptr;
				if (Module != nullptr)
				{
					int32 VarCount = Module->GetGlobalVarCount();
					for (int32 i = 0; i < VarCount; ++i)
					{
						const char* VarName;
						int32 TypeId;
						Module->GetGlobalVar(i, &VarName, nullptr, &TypeId);
						auto Usage = FAngelscriptTypeUsage::FromTypeId(TypeId);

						FDebuggerValue VarValue;
						if (Usage.GetDebuggerValue(Module->GetAddressOfGlobalVar(i), VarValue))
						{
							VarValue.Name = ANSI_TO_TCHAR(VarName);
							if (!VarValue.Name.StartsWith(TEXT("__")))
							{
								Scope.Values.Add(MoveTemp(VarValue));
							}
						}
					}
				}
			}

			return true;
		}
	}

	// This was a regular variable scope, so perform its lookup
	const int32 CurrentScopeCount = Scope.Values.Num();
	bool bSuccess = CurrentValue.Usage.GetDebuggerScope(CurrentValue.Address, Scope);
	const int32 AddedScopeValues = Scope.Values.Num() - CurrentScopeCount;

	// If our current value is a temporary, make sure we fetch the scope from the "real" address and propagate those child property addresses too
	if (CurrentValue.bTemporaryValue)
	{
		FDebuggerScope MonitorableScope;
		if (CurrentValue.NonTemporaryAddress != nullptr)
			CurrentValue.Usage.GetDebuggerScope(CurrentValue.NonTemporaryAddress, MonitorableScope);

		for (int i = 0; i < AddedScopeValues; i++)
		{
			auto& Value = Scope.Values[Scope.Values.Num() - AddedScopeValues + i];
			if (i < MonitorableScope.Values.Num())
				Value.NonTemporaryAddress = MonitorableScope.Values[i].GetNonTemporaryAddress();
			Value.bTemporaryValue = CurrentValue.bTemporaryValue;
		}
	}

	return bSuccess;
}

void FAngelscriptDebugServer::TrySendingMessages(FSocket* Client)
{
	TArray<FQueuedMessage>& Queue = QueuedSends.FindOrAdd(Client);
	while (Queue.Num() != 0)
	{
		FQueuedMessage& Msg = Queue[0];
		if (Msg.FirstTry < 0)
			Msg.FirstTry = FPlatformTime::Seconds();

		int32 BytesSent;
		if (!Client->Send(Msg.Buffer.GetData(), Msg.Buffer.Num(), BytesSent))
			break;

		Queue.RemoveAt(0);
	}
}
