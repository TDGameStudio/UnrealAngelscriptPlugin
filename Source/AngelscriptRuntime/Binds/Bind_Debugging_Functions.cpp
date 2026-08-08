#include "Bind_Debugging.h"

#include "AngelscriptEngine.h"

namespace
{
	bool GDebugBreaksEnabled = true;
	int32 GEndPlayMapCount = 0;

	TMap<FString, int32>& GetPoppedEnsures()
	{
		static TMap<FString, int32> PoppedEnsures;
		return PoppedEnsures;
	}

	bool ShouldBreakEnsure(const FString& Position)
	{
#if DO_CHECK && !USING_CODE_ANALYSIS
		TMap<FString, int32>& PoppedEnsures = GetPoppedEnsures();
		int32* PreviousPop = PoppedEnsures.Find(Position);
		const bool bShouldBreak = PreviousPop == nullptr || *PreviousPop != GEndPlayMapCount;
		if (bShouldBreak)
			PoppedEnsures.Add(Position, GEndPlayMapCount);
		return bShouldBreak;
#else
		return false;
#endif
	}

	bool IsEvaluatingDebuggerWatch()
	{
#if WITH_AS_DEBUGSERVER
		return FAngelscriptEngine::Get().IsEvaluatingDebuggerWatch();
#else
		return false;
#endif
	}
}

void AngelscriptDisableDebugBreaks()
{
	GDebugBreaksEnabled = false;
}

void AngelscriptEnableDebugBreaks()
{
	GDebugBreaksEnabled = true;
}

bool AreAngelscriptDebugBreaksEnabledForTesting()
{
	return GDebugBreaksEnabled;
}

void AngelscriptForgetSeenEnsures()
{
	GetPoppedEnsures().Empty();
}

void FAngelscriptDebuggingBinds::HandleEndPlayMap()
{
	++GEndPlayMapCount;
}

void FAngelscriptDebuggingBinds::DebugBreak()
{
	if (!GDebugBreaksEnabled || IsEvaluatingDebuggerWatch())
		return;

	volatile TArray<FString> ScriptCallstack = FAngelscriptEngine::GetAngelscriptCallstack();
	if (FAngelscriptEngine::TryBreakpointAngelscriptDebugging())
		return;
	UE_DEBUG_BREAK();
}

bool FAngelscriptDebuggingBinds::Ensure(bool bCondition)
{
	if (IsEvaluatingDebuggerWatch())
		return bCondition;
#if DO_CHECK
	if (!bCondition)
	{
		const FString Position = FAngelscriptEngine::GetAngelscriptExecutionPosition();
		if (ShouldBreakEnsure(Position))
		{
			UE_LOG(Angelscript, Error, TEXT("Ensure condition failed\n%s"), *Position);
			DebugBreak();
		}
	}
#endif
	return bCondition;
}

bool FAngelscriptDebuggingBinds::EnsureWithMessage(bool bCondition, const FString& Message)
{
	if (IsEvaluatingDebuggerWatch())
		return bCondition;
#if DO_CHECK
	if (!bCondition)
	{
		const FString Position = FAngelscriptEngine::GetAngelscriptExecutionPosition();
		if (ShouldBreakEnsure(Position))
		{
			UE_LOG(Angelscript, Error, TEXT("Ensure condition failed: %s\n%s"), *Message, *Position);
			DebugBreak();
		}
	}
#endif
	return bCondition;
}

bool FAngelscriptDebuggingBinds::EnsureAlways(bool bCondition)
{
	if (IsEvaluatingDebuggerWatch())
		return bCondition;
#if DO_CHECK
	if (!bCondition)
	{
		const FString Position = FAngelscriptEngine::GetAngelscriptExecutionPosition();
		UE_LOG(Angelscript, Error, TEXT("Ensure condition failed\n%s"), *Position);
		DebugBreak();
	}
#endif
	return bCondition;
}

bool FAngelscriptDebuggingBinds::EnsureAlwaysWithMessage(bool bCondition, const FString& Message)
{
	if (IsEvaluatingDebuggerWatch())
		return bCondition;
#if DO_CHECK
	if (!bCondition)
	{
		const FString Position = FAngelscriptEngine::GetAngelscriptExecutionPosition();
		UE_LOG(Angelscript, Error, TEXT("Ensure condition failed: %s\n%s"), *Message, *Position);
		DebugBreak();
	}
#endif
	return bCondition;
}

void FAngelscriptDebuggingBinds::Check(bool bCondition)
{
	if (IsEvaluatingDebuggerWatch())
		return;
#if DO_CHECK
	if (!bCondition)
	{
		const FString Position = FAngelscriptEngine::GetAngelscriptExecutionPosition();
		UE_LOG(Angelscript, Error, TEXT("Check condition failed\n%s"), *Position);
		DebugBreak();
	}
#endif
}

void FAngelscriptDebuggingBinds::CheckWithMessage(bool bCondition, const FString& Message)
{
	if (IsEvaluatingDebuggerWatch())
		return;
#if DO_CHECK
	if (!bCondition)
	{
		const FString Position = FAngelscriptEngine::GetAngelscriptExecutionPosition();
		UE_LOG(Angelscript, Error, TEXT("Check condition failed: %s\n%s"), *Message, *Position);
		DebugBreak();
	}
#endif
}

void FAngelscriptDebuggingBinds::Throw(const FString& Message)
{
	FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
}

TArray<FString> FAngelscriptDebuggingBinds::GetCallstack()
{
	return FAngelscriptEngine::Get().GetAngelscriptCallstack();
}

FString FAngelscriptDebuggingBinds::FormatCallstack()
{
	return FAngelscriptEngine::Get().FormatAngelscriptCallstack();
}
