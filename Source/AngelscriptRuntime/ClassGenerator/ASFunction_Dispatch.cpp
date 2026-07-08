#include "ClassGenerator/ASFunction_CallHelpers.h"
void UASFunction_NoParams::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (UNLIKELY(RealFunction == nullptr))
	{
		P_FINISH;
		return;
	}
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		P_FINISH;

		MakeRawJITCall_NoParam(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		P_FINISH;
		Context->Execute();
	}
}

void UASFunction_NoParams::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (UNLIKELY(RealFunction == nullptr))
		return;
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		MakeRawJITCall_NoParam(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);
		Context->Execute();
	}
}

void UASFunction_DWordArg::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		asDWORD ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);

		P_FINISH;

		MakeRawJITCall_Arg<asDWORD>(Object, JitFunc, ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);

		asDWORD ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);
		Context->m_regs.stackFramePointer[AS_PTR_SIZE] = ArgumentValue;

		P_FINISH;
		Context->Execute();
	}
}

void UASFunction_DWordArg::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		asDWORD ArgumentValue = *(asDWORD*)Parms;
		MakeRawJITCall_Arg<asDWORD>(Object, JitFunc, ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);
		checkSlow(Arguments[0].PosInParmStruct == 0);
		checkSlow(Arguments[0].ParmBehavior == EArgumentParmBehavior::Value4Byte);

		Context->m_regs.stackFramePointer[AS_PTR_SIZE] = *(asDWORD*)Parms;

		Context->Execute();
	}
}

void UASFunction_FloatArg::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		float ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);

		P_FINISH;

		MakeRawJITCall_Arg<float>(Object, JitFunc, ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);

		asDWORD ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);
		Context->m_regs.stackFramePointer[AS_PTR_SIZE] = ArgumentValue;

		P_FINISH;
		Context->Execute();
	}
}

void UASFunction_FloatArg::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		float ArgumentValue = *(float*)Parms;
		MakeRawJITCall_Arg<float>(Object, JitFunc, ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);
		checkSlow(Arguments[0].PosInParmStruct == 0);
		checkSlow(Arguments[0].ParmBehavior == EArgumentParmBehavior::Value4Byte);

		Context->m_regs.stackFramePointer[AS_PTR_SIZE] = *(asDWORD*)Parms;

		Context->Execute();
	}
}

void UASFunction_DoubleArg::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		double ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);

		P_FINISH;

		MakeRawJITCall_Arg<double>(Object, JitFunc, ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);

		asQWORD ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);
		*(asQWORD*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = ArgumentValue;

		P_FINISH;
		Context->Execute();
	}
}

void UASFunction_DoubleArg::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		double ArgumentValue = *(double*)Parms;
		MakeRawJITCall_Arg<double>(Object, JitFunc, ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);
		checkSlow(Arguments[0].PosInParmStruct == 0);
		checkSlow(Arguments[0].ParmBehavior == EArgumentParmBehavior::Value8Byte);

		*(asQWORD*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = *(asQWORD*)Parms;

		Context->Execute();
	}
}

void UASFunction_FloatExtendedToDoubleArg::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		float ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);

		P_FINISH;

		MakeRawJITCall_Arg<double>(Object, JitFunc, (double)ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);

		float ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);
		*(double*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = (double)ArgumentValue;

		P_FINISH;
		Context->Execute();
	}
}

void UASFunction_FloatExtendedToDoubleArg::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		float ArgumentValue = *(float*)Parms;
		MakeRawJITCall_Arg<double>(Object, JitFunc, (double)ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);
		checkSlow(Arguments[0].PosInParmStruct == 0);
		checkSlow(Arguments[0].ParmBehavior == EArgumentParmBehavior::FloatExtendedToDouble);

		*(double*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = (double)*(float*)Parms;

		Context->Execute();
	}
}

void UASFunction_QWordArg::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		asQWORD ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);

		P_FINISH;

		MakeRawJITCall_Arg<asQWORD>(Object, JitFunc, ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);

		asQWORD ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);
		*(asQWORD*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = ArgumentValue;

		P_FINISH;
		Context->Execute();
	}
}

void UASFunction_QWordArg::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		asQWORD ArgumentValue = *(asQWORD*)Parms;
		MakeRawJITCall_Arg<asQWORD>(Object, JitFunc, ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);
		checkSlow(Arguments[0].PosInParmStruct == 0);
		checkSlow(Arguments[0].ParmBehavior == EArgumentParmBehavior::Value8Byte);

		*(asQWORD*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = *(asQWORD*)Parms;

		Context->Execute();
	}
}

void UASFunction_ByteArg::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		asDWORD ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);

		P_FINISH;

		MakeRawJITCall_Arg<asBYTE>(Object, JitFunc, (asBYTE&)ArgumentValue);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);

		asDWORD ArgumentValue;
		Stack.StepCompiledIn<FProperty>(&ArgumentValue);
		*(asBYTE*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = *(asBYTE*)&ArgumentValue;

		P_FINISH;
		Context->Execute();
	}
}

void UASFunction_ByteArg::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		MakeRawJITCall_Arg<asBYTE>(Object, JitFunc, *(asBYTE*)Parms);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);
		checkSlow(Arguments[0].PosInParmStruct == 0);
		checkSlow(Arguments[0].ParmBehavior == EArgumentParmBehavior::Value1Byte);

		*(asBYTE*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = *(asBYTE*)Parms;

		Context->Execute();
	}
}

void UASFunction_ReferenceArg::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	AngelscriptCallFromBPVM<false, false>(this, Object, Stack, RESULT_PARAM);
}

void UASFunction_ReferenceArg::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		MakeRawJITCall_Arg<void*>(Object, JitFunc, Parms);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Arguments[0].PosInParmStruct == 0);

		*(asPWORD*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = (asPWORD)Parms;

		Context->Execute();
	}
}

void UASFunction_ObjectReturn::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(UObject**)RESULT_PARAM = nullptr;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		P_FINISH;

		*(UObject**)RESULT_PARAM = MakeRawJITCall_ReturnValue<UObject*>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (UObject**)RESULT_PARAM, nullptr);
		Context->SetObject(Object);

		P_FINISH;
		Context->Execute();

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(UObject**)RESULT_PARAM = nullptr;
		else
			*(UObject**)RESULT_PARAM = (UObject*)Context->GetReturnAddress();
	}
}

void UASFunction_ObjectReturn::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(UObject**)Parms = nullptr;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::ReturnObjectPointer);

		*(UObject**)Parms = MakeRawJITCall_ReturnValue<UObject*>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (UObject**)Parms, nullptr);
		Context->SetObject(Object);
		Context->Execute();

		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::ReturnObjectPointer);

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(UObject**)Parms = nullptr;
		else
			*(UObject**)Parms = (UObject*)Context->GetReturnAddress();
	}
}

void UASFunction_DWordReturn::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (Arguments.Num() == 0)
	{
		RuntimeCallEvent(Object, RESULT_PARAM);
		return;
	}

#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(asDWORD*)RESULT_PARAM = 0;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		P_FINISH;

		*(asDWORD*)RESULT_PARAM = MakeRawJITCall_ReturnValue<asDWORD>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (asDWORD*)RESULT_PARAM, 0);
		Context->SetObject(Object);

		P_FINISH;
		Context->Execute();


		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(asDWORD*)RESULT_PARAM = 0;
		else
			*(asDWORD*)RESULT_PARAM = Context->GetReturnDWord();
	}
}

void UASFunction_DWordReturn::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(asDWORD*)Parms = 0;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value4Byte);

		*(asDWORD*)Parms = MakeRawJITCall_ReturnValue<asDWORD>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (asDWORD*)Parms, 0);
		Context->SetObject(Object);
		Context->Execute();

		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value4Byte);

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
		{
			UE_LOG(Angelscript, Error, TEXT("Angelscript reflected call exception in %s: %s"), *GetPathName(), UTF8_TO_TCHAR(Context->GetExceptionString()));
			*(asDWORD*)Parms = 0;
		}
		else
			*(asDWORD*)Parms = Context->GetReturnDWord();
	}
}

void UASFunction_FloatExtendedToDoubleReturn::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(float*)RESULT_PARAM = 0.f;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		P_FINISH;

		*(float*)RESULT_PARAM = (float)MakeRawJITCall_ReturnValue<double>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (float*)RESULT_PARAM, 0.f);
		Context->SetObject(Object);

		P_FINISH;
		Context->Execute();


		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(float*)RESULT_PARAM = 0;
		else
			*(float*)RESULT_PARAM = (float)Context->GetReturnDouble();
	}
}

void UASFunction_FloatExtendedToDoubleReturn::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(float*)Parms = 0.f;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::FloatExtendedToDouble);

		*(float*)Parms = (float)MakeRawJITCall_ReturnValue<double>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (float*)Parms, 0.f);
		Context->SetObject(Object);
		Context->Execute();

		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::FloatExtendedToDouble);

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(float*)Parms = 0.f;
		else
			*(float*)Parms = (float)Context->GetReturnDouble();
	}
}

void UASFunction_FloatReturn::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(float*)RESULT_PARAM = 0.f;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		P_FINISH;

		*(float*)RESULT_PARAM = MakeRawJITCall_ReturnValue<float>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (float*)RESULT_PARAM, 0.f);
		Context->SetObject(Object);

		P_FINISH;
		Context->Execute();

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(float*)RESULT_PARAM = 0.f;
		else
			*(float*)RESULT_PARAM = Context->GetReturnFloat();
	}
}

void UASFunction_FloatReturn::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(float*)Parms = 0.f;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value4Byte);

		*(float*)Parms = MakeRawJITCall_ReturnValue<float>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (float*)Parms, 0.f);
		Context->SetObject(Object);
		Context->Execute();

		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value4Byte);

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(float*)Parms = 0.f;
		else
			*(float*)Parms = Context->GetReturnFloat();
	}
}

void UASFunction_DoubleReturn::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(double*)RESULT_PARAM = 0.0;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		P_FINISH;

		*(double*)RESULT_PARAM = MakeRawJITCall_ReturnValue<double>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (double*)RESULT_PARAM, 0.0);
		Context->SetObject(Object);

		P_FINISH;
		Context->Execute();

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(double*)RESULT_PARAM = 0.0;
		else
			*(double*)RESULT_PARAM = Context->GetReturnDouble();
	}
}

void UASFunction_DoubleReturn::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(double*)Parms = 0.0;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value8Byte);

		*(double*)Parms = MakeRawJITCall_ReturnValue<double>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (double*)Parms, 0.0);
		Context->SetObject(Object);
		Context->Execute();

		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value8Byte);

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(double*)Parms = 0.0;
		else
			*(double*)Parms = Context->GetReturnDouble();
	}
}

void UASFunction_ByteReturn::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(asBYTE*)RESULT_PARAM = 0;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		P_FINISH;

		*(asBYTE*)RESULT_PARAM = MakeRawJITCall_ReturnValue<asBYTE>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (asBYTE*)RESULT_PARAM, 0);
		Context->SetObject(Object);

		P_FINISH;
		Context->Execute();

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(asBYTE*)RESULT_PARAM = 0;
		else
			*(asBYTE*)RESULT_PARAM = Context->GetReturnByte();
	}
}

void UASFunction_ByteReturn::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
	{
		*(asBYTE*)Parms = 0;
		return;
	}
#endif

	if (!CheckGameThreadExecution(this))
		return;

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value1Byte);

		*(asBYTE*)Parms = MakeRawJITCall_ReturnValue<asBYTE>(Object, JitFunc);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, RealFunction, (asBYTE*)Parms, 0);
		Context->SetObject(Object);
		Context->Execute();

		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value1Byte);

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			*(asBYTE*)Parms = 0;
		else
			*(asBYTE*)Parms = Context->GetReturnByte();
	}
}
