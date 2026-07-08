#include "ClassGenerator/ASFunction_CallHelpers.h"
void UASFunction_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	AngelscriptCallFromBPVM<true, true>(this, Object, Stack, RESULT_PARAM);
}

void UASFunction_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	AngelscriptCallFromParms<true, true>(this, Object, Parms);
}

void UASFunction_NotThreadSafe_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	AngelscriptCallFromBPVM<false, true>(this, Object, Stack, RESULT_PARAM);
}

void UASFunction_NotThreadSafe_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	AngelscriptCallFromParms<false, true>(this, Object, Parms);
}

void UASFunction_NoParams_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	P_FINISH;
	MakeRawJITCall_NoParam(Object, JitFunction_Raw);
}

void UASFunction_NoParams_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	MakeRawJITCall_NoParam(Object, JitFunction_Raw);
}

void UASFunction_DWordArg_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	asDWORD ArgumentValue;
	Stack.StepCompiledIn<FProperty>(&ArgumentValue);

	P_FINISH;

	MakeRawJITCall_Arg<asDWORD>(Object, JitFunction_Raw, ArgumentValue);
}

void UASFunction_DWordArg_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	asDWORD ArgumentValue = *(asDWORD*)Parms;
	MakeRawJITCall_Arg<asDWORD>(Object, JitFunction_Raw, ArgumentValue);
}

void UASFunction_FloatArg_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	float ArgumentValue;
	Stack.StepCompiledIn<FProperty>(&ArgumentValue);

	P_FINISH;

	MakeRawJITCall_Arg<float>(Object, JitFunction_Raw, ArgumentValue);
}

void UASFunction_FloatArg_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	float ArgumentValue = *(float*)Parms;
	MakeRawJITCall_Arg<float>(Object, JitFunction_Raw, ArgumentValue);
}

void UASFunction_DoubleArg_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	double ArgumentValue;
	Stack.StepCompiledIn<FProperty>(&ArgumentValue);

	P_FINISH;

	MakeRawJITCall_Arg<double>(Object, JitFunction_Raw, ArgumentValue);
}

void UASFunction_DoubleArg_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	double ArgumentValue = *(double*)Parms;
	MakeRawJITCall_Arg<double>(Object, JitFunction_Raw, ArgumentValue);
}

void UASFunction_FloatExtendedToDoubleArg_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	float ArgumentValue;
	Stack.StepCompiledIn<FProperty>(&ArgumentValue);

	P_FINISH;

	MakeRawJITCall_Arg<double>(Object, JitFunction_Raw, (double)ArgumentValue);
}

void UASFunction_FloatExtendedToDoubleArg_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	float ArgumentValue = *(float*)Parms;
	MakeRawJITCall_Arg<double>(Object, JitFunction_Raw, (double)ArgumentValue);
}

void UASFunction_QWordArg_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	asQWORD ArgumentValue;
	Stack.StepCompiledIn<FProperty>(&ArgumentValue);

	P_FINISH;

	MakeRawJITCall_Arg<asQWORD>(Object, JitFunction_Raw, ArgumentValue);
}

void UASFunction_QWordArg_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	asQWORD ArgumentValue = *(asQWORD*)Parms;
	MakeRawJITCall_Arg<asQWORD>(Object, JitFunction_Raw, ArgumentValue);
}

void UASFunction_ByteArg_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	asDWORD ArgumentValue;
	Stack.StepCompiledIn<FProperty>(&ArgumentValue);

	P_FINISH;

	MakeRawJITCall_Arg<asBYTE>(Object, JitFunction_Raw, (asBYTE&)ArgumentValue);
}

void UASFunction_ByteArg_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	MakeRawJITCall_Arg<asBYTE>(Object, JitFunction_Raw, *(asBYTE*)Parms);
}

void UASFunction_ReferenceArg_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	AngelscriptCallFromBPVM<false, true>(this, Object, Stack, RESULT_PARAM);
}

void UASFunction_ReferenceArg_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	MakeRawJITCall_Arg<void*>(Object, JitFunction_Raw, Parms);
}

void UASFunction_ObjectReturn_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	P_FINISH;

	*(UObject**)RESULT_PARAM = MakeRawJITCall_ReturnValue<UObject*>(Object, JitFunction_Raw);
}

void UASFunction_ObjectReturn_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	checkSlow(ReturnArgument.PosInParmStruct == 0);
	checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::ReturnObjectPointer);

	*(UObject**)Parms = MakeRawJITCall_ReturnValue<UObject*>(Object, JitFunction_Raw);
}

void UASFunction_DWordReturn_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (Arguments.Num() == 0)
	{
		RuntimeCallEvent(Object, RESULT_PARAM);
		return;
	}

	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	P_FINISH;

	*(asDWORD*)RESULT_PARAM = MakeRawJITCall_ReturnValue<asDWORD>(Object, JitFunction_Raw);
}

void UASFunction_DWordReturn_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	checkSlow(ReturnArgument.PosInParmStruct == 0);
	checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value4Byte);

	*(asDWORD*)Parms = MakeRawJITCall_ReturnValue<asDWORD>(Object, JitFunction_Raw);
}

void UASFunction_FloatExtendedToDoubleReturn_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	P_FINISH;

	*(float*)RESULT_PARAM = (float)MakeRawJITCall_ReturnValue<double>(Object, JitFunction_Raw);
}

void UASFunction_FloatExtendedToDoubleReturn_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	checkSlow(ReturnArgument.PosInParmStruct == 0);
	checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::FloatExtendedToDouble);

	*(float*)Parms = (float)MakeRawJITCall_ReturnValue<double>(Object, JitFunction_Raw);
}

void UASFunction_FloatReturn_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	P_FINISH;

	*(float*)RESULT_PARAM = MakeRawJITCall_ReturnValue<float>(Object, JitFunction_Raw);
}

void UASFunction_FloatReturn_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	checkSlow(ReturnArgument.PosInParmStruct == 0);
	checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value4Byte);

	*(float*)Parms = MakeRawJITCall_ReturnValue<float>(Object, JitFunction_Raw);
}

void UASFunction_DoubleReturn_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	P_FINISH;

	*(double*)RESULT_PARAM = MakeRawJITCall_ReturnValue<double>(Object, JitFunction_Raw);
}

void UASFunction_DoubleReturn_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	checkSlow(ReturnArgument.PosInParmStruct == 0);
	checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value8Byte);

	*(double*)Parms = MakeRawJITCall_ReturnValue<double>(Object, JitFunction_Raw);
}

void UASFunction_ByteReturn_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	P_FINISH;

	*(asBYTE*)RESULT_PARAM = MakeRawJITCall_ReturnValue<asBYTE>(Object, JitFunction_Raw);
}

void UASFunction_ByteReturn_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	if (!CheckGameThreadExecution(this))
		return;
	VerifyScriptVirtualResolved(this, Object);

	checkSlow(ReturnArgument.PosInParmStruct == 0);
	checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value1Byte);

	*(asBYTE*)Parms = MakeRawJITCall_ReturnValue<asBYTE>(Object, JitFunction_Raw);
}
