// Bindings-local execute helpers for World collision binding tests.

#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "Templates/Function.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

inline bool WorldCollisionSetArgAddressChecked(
	FAutomationTestBase& Test,
	asIScriptContext& Context,
	asUINT ArgumentIndex,
	void* Address,
	const TCHAR* ContextLabel)
{
	return Test.TestEqual(
		*FString::Printf(TEXT("%s should bind address argument %u"), ContextLabel, static_cast<uint32>(ArgumentIndex)),
		Context.SetArgAddress(ArgumentIndex, Address),
		static_cast<int32>(asSUCCESS));
}

inline bool WorldCollisionSetArgObjectChecked(
	FAutomationTestBase& Test,
	asIScriptContext& Context,
	asUINT ArgumentIndex,
	void* Object,
	const TCHAR* ContextLabel)
{
	return Test.TestEqual(
		*FString::Printf(TEXT("%s should bind object argument %u"), ContextLabel, static_cast<uint32>(ArgumentIndex)),
		Context.SetArgObject(ArgumentIndex, Object),
		static_cast<int32>(asSUCCESS));
}

inline bool WorldCollisionExecuteBoolFunction(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const FString& FunctionDecl,
	TFunctionRef<bool(asIScriptContext&)> BindArguments,
	const TCHAR* ContextLabel,
	bool& OutResult)
{
	asIScriptFunction* Function = GetFunctionByDecl(Test, Module, FunctionDecl);
	if (Function == nullptr)
	{
		return false;
	}

	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptContext* Context = Engine.CreateContext();
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create an execution context"), ContextLabel), Context))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	const int PrepareResult = Context->Prepare(Function);
	if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel),
			PrepareResult,
			static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	if (!BindArguments(*Context))
	{
		return false;
	}

	const int ExecuteResult = Context->Execute();
	if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should execute successfully"), ContextLabel),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED)))
	{
		return false;
	}

	OutResult = Context->GetReturnByte() != 0;
	return true;
}

inline bool WorldCollisionExecuteIntFunction(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	TFunctionRef<bool(asIScriptContext&)> BindArguments,
	const TCHAR* ContextLabel,
	int32& OutResult)
{
	asIScriptFunction* Function = GetFunctionByDecl(Test, Module, FunctionDecl);
	if (Function == nullptr)
	{
		return false;
	}

	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptContext* Context = Engine.CreateContext();
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create an execution context"), ContextLabel), Context))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	const int PrepareResult = Context->Prepare(Function);
	if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel),
			PrepareResult,
			static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	if (!BindArguments(*Context))
	{
		return false;
	}

	const int ExecuteResult = Context->Execute();
	if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should execute successfully"), ContextLabel),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED)))
	{
		return false;
	}

	OutResult = static_cast<int32>(Context->GetReturnDWord());
	return true;
}

inline bool WorldCollisionExecuteFunctionExpectingException(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	TFunctionRef<bool(asIScriptContext&)> BindArguments,
	const TCHAR* ContextLabel,
	const FString& ExpectedExceptionContains,
	FString* OutExceptionString = nullptr)
{
	asIScriptFunction* Function = GetFunctionByDecl(Test, Module, FunctionDecl);
	if (Function == nullptr)
	{
		return false;
	}

	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptContext* Context = Engine.CreateContext();
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create an execution context"), ContextLabel), Context))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	const int PrepareResult = Context->Prepare(Function);
	if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel),
			PrepareResult,
			static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	if (!BindArguments(*Context))
	{
		return false;
	}

	const int ExecuteResult = Context->Execute();
	const FString ExceptionString = UTF8_TO_TCHAR(
		Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");

	if (OutExceptionString != nullptr)
	{
		*OutExceptionString = ExceptionString;
	}

	bool bPassed = true;
	bPassed &= Test.TestEqual(
		*FString::Printf(TEXT("%s should fail with a script exception"), ContextLabel),
		ExecuteResult,
		static_cast<int32>(asEXECUTION_EXCEPTION));
	bPassed &= Test.TestTrue(
		*FString::Printf(TEXT("%s should report the expected exception text"), ContextLabel),
		ExceptionString.Contains(ExpectedExceptionContains));
	return bPassed;
}

template <typename TValue>
inline bool WorldCollisionExecuteAddressBoolFunction(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* ContextLabel,
	TValue& OutValue,
	bool& OutResult)
{
	return WorldCollisionExecuteBoolFunction(
		Test,
		Engine,
		Module,
		FunctionDecl,
		[&](asIScriptContext& Context)
		{
			return WorldCollisionSetArgAddressChecked(Test, Context, 0, &OutValue, ContextLabel);
		},
		ContextLabel,
		OutResult);
}

#endif // WITH_ANGELSCRIPT_UNITTESTS
