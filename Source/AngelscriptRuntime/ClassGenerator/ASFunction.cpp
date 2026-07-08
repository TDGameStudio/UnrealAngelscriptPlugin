#include "ClassGenerator/ASFunction.h"
#include "ClassGenerator/ASFunction_CallHelpers.h"

#include "UObject/CoreNet.h"
#include "UObject/StructOnScope.h"

namespace
{
	FString GLastAngelscriptValidateFailureReason;
}

#if WITH_EDITOR
extern thread_local bool GIsInAngelscriptThreadSafeFunction;
#endif

void UASFunction::FinalizeArguments()
{
	ArgStackSize = 0;
	for (int32 i = 0, Num = Arguments.Num(); i < Num; ++i)
	{
		auto& Arg = Arguments[i];

		int32 ArgSize = Arg.Type.GetValueSize();
		int32 ArgAlign = Arg.Type.GetValueAlignment();

		int32 AlignOffset = (Align(ArgStackSize, ArgAlign) - ArgStackSize);
		ArgStackSize += AlignOffset;

		Arg.ValueBytes = ArgSize;
		Arg.StackOffset = ArgStackSize;
		Arg.PosInParmStruct = Arg.Property->GetOffset_ForUFunction();

		if (Arg.Type.bIsReference)
		{
			Arg.ParmBehavior = EArgumentParmBehavior::Reference;
			if (Arg.Type.IsObjectPointer())
				Arg.VMBehavior = EArgumentVMBehavior::ReferencePOD;
			else if (Arg.Type.NeedConstruct())
				Arg.VMBehavior = EArgumentVMBehavior::Reference;
			else
				Arg.VMBehavior = EArgumentVMBehavior::ReferencePOD;
		}
		else if (Arg.Type.IsObjectPointer())
		{
			Arg.ParmBehavior = EArgumentParmBehavior::Value8Byte;

			if (WorldContextIndex == i)
				Arg.VMBehavior = EArgumentVMBehavior::WorldContextObject;
			else
				Arg.VMBehavior = EArgumentVMBehavior::ObjectPointer;
		}
		else if (Arg.Type.IsPrimitive())
		{
			// Special case: a float property that is represented by a double argument should
			// be converted as part of the call thunk.
			if (Arg.Type.Type == FAngelscriptType::ScriptFloatParamExtendedToDoubleType())
			{
				Arg.ParmBehavior = EArgumentParmBehavior::FloatExtendedToDouble;
				Arg.VMBehavior = EArgumentVMBehavior::FloatExtendedToDouble;
			}
			else
			{
				switch (Arg.Type.GetValueSize())
				{
				default:
					check(false);
				case 1:
					Arg.ParmBehavior = EArgumentParmBehavior::Value1Byte;
					Arg.VMBehavior = EArgumentVMBehavior::Value1Byte;
				break;
				case 2:
					Arg.ParmBehavior = EArgumentParmBehavior::Value2Byte;
					Arg.VMBehavior = EArgumentVMBehavior::Value2Byte;
				break;
				case 4:
					Arg.ParmBehavior = EArgumentParmBehavior::Value4Byte;
					Arg.VMBehavior = EArgumentVMBehavior::Value4Byte;
				break;
				case 8:
					Arg.ParmBehavior = EArgumentParmBehavior::Value8Byte;
					Arg.VMBehavior = EArgumentVMBehavior::Value8Byte;
				break;
				}
			}
		}
		else
		{
			if (Arg.Type.NeedCopy())
				Arg.ParmBehavior = EArgumentParmBehavior::Reference;
			else
				Arg.ParmBehavior = EArgumentParmBehavior::ReferencePOD;

			if (Arg.Type.NeedConstruct())
				Arg.VMBehavior = EArgumentVMBehavior::Reference;
			else
				Arg.VMBehavior = EArgumentVMBehavior::ReferencePOD;
		}

		if (Arg.Type.CanDestruct() && Arg.Type.NeedDestruct())
		{
			check(Arg.VMBehavior == EArgumentVMBehavior::Reference);
			DestroyArguments.Add(Arg);
		}

		ArgStackSize += ArgSize;
	}

	if (ReturnArgument.Property != nullptr)
	{
		ReturnArgument.PosInParmStruct = ReturnArgument.Property->GetOffset_ForUFunction();
		ReturnArgument.ValueBytes = ReturnArgument.Type.GetValueSize();

		if (ReturnArgument.Type.bIsReference)
		{
			if (ReturnArgument.Type.NeedCopy())
			{
				ReturnArgument.ParmBehavior = EArgumentParmBehavior::Reference;
				ReturnArgument.VMBehavior = EArgumentVMBehavior::Reference;
			}
			else
			{
				ReturnArgument.ParmBehavior = EArgumentParmBehavior::ReferencePOD;
				ReturnArgument.VMBehavior = EArgumentVMBehavior::ReferencePOD;
			}
		}
		else if (ReturnArgument.Type.IsObjectPointer())
		{
			ReturnArgument.ParmBehavior = EArgumentParmBehavior::ReturnObjectPointer;
			ReturnArgument.VMBehavior = EArgumentVMBehavior::Value8Byte;
		}
		else if (ReturnArgument.Type.IsPrimitive())
		{
			// Special case: a float property that is represented by a double argument should
			// be converted as part of the call thunk.
			if (ReturnArgument.Type.Type == FAngelscriptType::ScriptFloatParamExtendedToDoubleType())
			{
				ReturnArgument.ParmBehavior = EArgumentParmBehavior::FloatExtendedToDouble;
				ReturnArgument.VMBehavior = EArgumentVMBehavior::FloatExtendedToDouble;
			}
			else
			{
				switch (ReturnArgument.Type.GetValueSize())
				{
				default:
					check(false);
				case 1:
					ReturnArgument.ParmBehavior = EArgumentParmBehavior::Value1Byte;
					ReturnArgument.VMBehavior = EArgumentVMBehavior::Value1Byte;
				break;
				case 2:
					ReturnArgument.ParmBehavior = EArgumentParmBehavior::Value2Byte;
					ReturnArgument.VMBehavior = EArgumentVMBehavior::Value2Byte;
				break;
				case 4:
					ReturnArgument.ParmBehavior = EArgumentParmBehavior::Value4Byte;
					ReturnArgument.VMBehavior = EArgumentVMBehavior::Value4Byte;
				break;
				case 8:
					ReturnArgument.ParmBehavior = EArgumentParmBehavior::Value8Byte;
					ReturnArgument.VMBehavior = EArgumentVMBehavior::Value8Byte;
				break;
				}
			}
		}
		else
		{
			if (ReturnArgument.Type.NeedCopy())
				ReturnArgument.ParmBehavior = EArgumentParmBehavior::Reference;
			else
				ReturnArgument.ParmBehavior = EArgumentParmBehavior::ReferencePOD;

			if (ReturnArgument.Type.NeedDestruct())
				ReturnArgument.VMBehavior = EArgumentVMBehavior::ReturnObjectValue;
			else
				ReturnArgument.VMBehavior = EArgumentVMBehavior::ReturnObjectPOD;
		}
	}
	else
	{
		ReturnArgument.VMBehavior = EArgumentVMBehavior::None;
	}
}

FString UASFunction::GetSourceFilePath() const
{
	if (ScriptFunction == nullptr)
		return TEXT("");
	auto& Manager = FAngelscriptEngine::Get();
	auto Module = Manager.GetModule(ScriptFunction->GetModule());
	if (!Module.IsValid())
		return TEXT("");
	if (Module->Code.Num() == 0)
		return TEXT("");
	return Module->Code[0].AbsoluteFilename;
}

int UASFunction::GetSourceLineNumber() const
{
	if (ScriptFunction == nullptr)
		return -1;

	auto* RealFunc = ((asCScriptFunction*)ScriptFunction);
	auto* scriptData = RealFunc->scriptData;
	if (scriptData == nullptr)
		return -1;

	return (scriptData->declaredAt & 0xFFFFF) + 1;
}

uint8 UASFunction::OptimizedCall_ByteReturn(UObject* Object)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return 0;
#endif

	if (JitFunction_Raw != nullptr)
	{
		return MakeRawJITCall_ReturnValue<asBYTE>(Object, JitFunction_Raw);
	}

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (RealFunction->jitFunction_Raw != nullptr)
	{
		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value1Byte);

		return MakeRawJITCall_ReturnValue<asBYTE>(Object, RealFunction->jitFunction_Raw);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN_VALUE(Context, RealFunction, 0);
		Context->SetObject(Object);
		Context->Execute();

		checkSlow(ReturnArgument.PosInParmStruct == 0);
		checkSlow(ReturnArgument.ParmBehavior == EArgumentParmBehavior::Value1Byte);

		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			return 0;

		return Context->GetReturnByte();
	}
}

void UASFunction::OptimizedCall_FloatArg(UObject* Object, float Argument)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (JitFunction_Raw != nullptr)
	{
		MakeRawJITCall_Arg<float>(Object, JitFunction_Raw, Argument);
		return;
	}

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (RealFunction->jitFunction_Raw != nullptr)
	{
		MakeRawJITCall_Arg<float>(Object, RealFunction->jitFunction_Raw, Argument);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);
		checkSlow(Arguments[0].PosInParmStruct == 0);
		checkSlow(Arguments[0].ParmBehavior == EArgumentParmBehavior::Value4Byte);

		Context->m_regs.stackFramePointer[AS_PTR_SIZE] = (asDWORD&)Argument;

		Context->Execute();
	}
}

void UASFunction::OptimizedCall_DoubleArg(UObject* Object, double Argument)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (JitFunction_Raw != nullptr)
	{
		MakeRawJITCall_Arg<double>(Object, JitFunction_Raw, Argument);
		return;
	}

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		MakeRawJITCall_Arg<double>(Object, JitFunc, Argument);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);

		checkSlow(Context->m_returnValueSize == 0);
		checkSlow(Arguments[0].PosInParmStruct == 0);
		checkSlow(Arguments[0].ParmBehavior == EArgumentParmBehavior::Value8Byte);

		*(asQWORD*)&Context->m_regs.stackFramePointer[AS_PTR_SIZE] = (asQWORD&)Argument;

		Context->Execute();
	}
}

void UASFunction::OptimizedCall(UObject* Object)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (JitFunction_Raw != nullptr)
	{
		MakeRawJITCall_NoParam(Object, JitFunction_Raw);
		return;
	}

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
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

uint8 UASFunction::OptimizedCall_RefArg_ByteReturn(UObject* Object, void* Argument)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return 0;
#endif

	if (JitFunction_Raw != nullptr)
	{
		return MakeRawJITCall_Arg_ReturnValue<void*,uint8>(Object, JitFunction_Raw, Argument);
	}

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		return MakeRawJITCall_Arg_ReturnValue<void*,uint8>(Object, JitFunc, Argument);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN_VALUE(Context, RealFunction, 0);
		Context->SetObject(Object);
		Context->SetArgAddress(0, Argument);

		Context->Execute();
		
		if (Context->m_status == asEContextState::asEXECUTION_EXCEPTION)
			return 0;

		return Context->GetReturnByte();
	}
}

void UASFunction::OptimizedCall_RefArg(UObject* Object, void* Argument)
{
#if AS_CAN_HOTRELOAD
	if (ScriptFunction == nullptr)
		return;
#endif

	if (JitFunction_Raw != nullptr)
	{
		MakeRawJITCall_Arg<void*>(Object, JitFunction_Raw, Argument);
		return;
	}

	asCScriptFunction* RealFunction = ResolveScriptVirtual(this, Object);
	if (auto* JitFunc = RealFunction->jitFunction_Raw)
	{
		MakeRawJITCall_Arg<void*>(Object, JitFunc, Argument);
	}
	else
	{
		FAngelscriptGameThreadContext Context(Object, RealFunction->GetEngine());
		AS_PREPARE_CONTEXT_OR_RETURN(Context, RealFunction);
		Context->SetObject(Object);
		Context->SetArgAddress(0, Argument);

		Context->Execute();
	}
}


UASFunction* UASFunction::AllocateFunctionFor(UClass* InClass, FName ObjectName, TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc)
{
	asCScriptFunction* ScriptFunction = (asCScriptFunction*)FunctionDesc->ScriptFunction;
	const bool bHasNonVirtualJitFunction = ScriptFunction != nullptr
		&& ScriptFunction->jitFunction != nullptr
		&& ScriptFunction->jitFunction_Raw != nullptr
		&& ScriptFunction->jitFunction_ParmsEntry != nullptr
		&& ScriptFunction->traits.GetTrait(asTRAIT_FINAL)
	;

	// Thread safe functions must go through the most generic path for calls
	if (FunctionDesc->bThreadSafe)
	{
		if (bHasNonVirtualJitFunction)
			return NewObject<UASFunction_JIT>(InClass, ObjectName, RF_Public);
		else
			return NewObject<UASFunction>(InClass, ObjectName, RF_Public);
	}

	if (!FunctionDesc->bIsStatic)
	{
		// void ScriptFunction();
		if (!FunctionDesc->ReturnType.IsValid()
			&& FunctionDesc->Arguments.Num() == 0)
		{
			if (bHasNonVirtualJitFunction)
				return NewObject<UASFunction_NoParams_JIT>(InClass, ObjectName, RF_Public);
			else
				return NewObject<UASFunction_NoParams>(InClass, ObjectName, RF_Public);
		}

		// void ScriptFunction({PRIMITIVE} Value);
		if (!FunctionDesc->ReturnType.IsValid()
			&& FunctionDesc->Arguments.Num() == 1
			&& !FunctionDesc->Arguments[0].Type.bIsReference
			&& FunctionDesc->Arguments[0].Type.IsPrimitive())
		{
			int32 ArgSize = FunctionDesc->Arguments[0].Type.GetValueSize();
			if (ArgSize == 1)
			{
				if (bHasNonVirtualJitFunction)
					return NewObject<UASFunction_ByteArg_JIT>(InClass, ObjectName, RF_Public);
				else
					return NewObject<UASFunction_ByteArg>(InClass, ObjectName, RF_Public);
			}
			else if (ArgSize == 4)
			{
				if (FunctionDesc->Arguments[0].Type.Type == FAngelscriptType::ScriptFloatType())
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_FloatArg_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_FloatArg>(InClass, ObjectName, RF_Public);
				}
				else if (FunctionDesc->Arguments[0].Type.Type == FAngelscriptType::ScriptFloatParamExtendedToDoubleType())
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_FloatExtendedToDoubleArg_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_FloatExtendedToDoubleArg>(InClass, ObjectName, RF_Public);
				}
				else
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_DWordArg_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_DWordArg>(InClass, ObjectName, RF_Public);
				}
			}
			else if (ArgSize == 8)
			{
				if (FunctionDesc->Arguments[0].Type.Type == FAngelscriptType::ScriptDoubleType())
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_DoubleArg_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_DoubleArg>(InClass, ObjectName, RF_Public);
				}
				else
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_QWordArg_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_QWordArg>(InClass, ObjectName, RF_Public);
				}
			}
		}

		// void ScriptFunction({TYPE}& Value);
		if (!FunctionDesc->ReturnType.IsValid()
			&& FunctionDesc->Arguments.Num() == 1
			&& FunctionDesc->Arguments[0].Type.bIsReference)
		{
			if (bHasNonVirtualJitFunction)
				return NewObject<UASFunction_ReferenceArg_JIT>(InClass, ObjectName, RF_Public);
			else
				return NewObject<UASFunction_ReferenceArg>(InClass, ObjectName, RF_Public);
		}

		// {PRIMITIVE} ScriptFunction();
		if (FunctionDesc->ReturnType.IsValid()
			&& !FunctionDesc->ReturnType.bIsReference
			&& FunctionDesc->ReturnType.IsPrimitive()
			&& FunctionDesc->Arguments.Num() == 0)
		{
			int32 ReturnSize = FunctionDesc->ReturnType.GetValueSize();
			if (ReturnSize == 1)
			{
				if (bHasNonVirtualJitFunction)
					return NewObject<UASFunction_ByteReturn_JIT>(InClass, ObjectName, RF_Public);
				else
					return NewObject<UASFunction_ByteReturn>(InClass, ObjectName, RF_Public);
			}
			else if (ReturnSize == 4)
			{
				if (FunctionDesc->ReturnType.Type == FAngelscriptType::ScriptFloatType())
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_FloatReturn_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_FloatReturn>(InClass, ObjectName, RF_Public);
				}
				else if (FunctionDesc->ReturnType.Type == FAngelscriptType::ScriptFloatParamExtendedToDoubleType())
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_FloatExtendedToDoubleReturn_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_FloatExtendedToDoubleReturn>(InClass, ObjectName, RF_Public);
				}
				else
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_DWordReturn_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_DWordReturn>(InClass, ObjectName, RF_Public);
				}
			}
			else if (ReturnSize == 8)
			{
				if (FunctionDesc->ReturnType.Type == FAngelscriptType::ScriptDoubleType())
				{
					if (bHasNonVirtualJitFunction)
						return NewObject<UASFunction_DoubleReturn_JIT>(InClass, ObjectName, RF_Public);
					else
						return NewObject<UASFunction_DoubleReturn>(InClass, ObjectName, RF_Public);
				}
			}
		}

		// UObject ScriptFunction();
		if (FunctionDesc->ReturnType.IsValid()
			&& !FunctionDesc->ReturnType.bIsReference
			&& FunctionDesc->ReturnType.IsObjectPointer()
			&& FunctionDesc->Arguments.Num() == 0)
		{
			if (bHasNonVirtualJitFunction)
				return NewObject<UASFunction_ObjectReturn_JIT>(InClass, ObjectName, RF_Public);
			else
				return NewObject<UASFunction_ObjectReturn>(InClass, ObjectName, RF_Public);
		}
	}

	// Fallback generic path for any non-thread-safe functions otherwise
	if (bHasNonVirtualJitFunction)
		return NewObject<UASFunction_NotThreadSafe_JIT>(InClass, ObjectName, RF_Public);
	else
		return NewObject<UASFunction_NotThreadSafe>(InClass, ObjectName, RF_Public);
}

void UASFunction::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if WITH_EDITOR
	TGuardValue ScopeThreadSafe(GIsInAngelscriptThreadSafeFunction, true);
#endif

	AngelscriptCallFromBPVM<true, false>(this, Object, Stack, RESULT_PARAM);
}

static bool ExecuteValidateFunctionForProcessEvent(UASFunction* Function, UObject* Object, void* Parameters)
{
	UFunction* ValidateFunction = Function->GetRuntimeValidateFunction();
	if (ValidateFunction == nullptr)
	{
		return false;
	}

	UASFunction* ASValidate = Cast<UASFunction>(ValidateFunction);
	if (ASValidate == nullptr)
	{
		return false;
	}

	FStructOnScope ValidateFunctionParms(ValidateFunction);
	uint8* ValidateFunctionParmsPtr = ValidateFunctionParms.GetStructMemory();

	TFieldIterator<FProperty> FunctionPropertyIt(Function);
	TFieldIterator<FProperty> ValidateFunctionPropertyIt(ValidateFunction);

	for (int32 ParamIdx = 0; ParamIdx < Function->NumParms; ++ParamIdx)
	{
		if (!FunctionPropertyIt)
		{
			break;
		}

		check(ValidateFunctionPropertyIt);

		FProperty* SourceProp = *FunctionPropertyIt;
		FProperty* TargetProp = *ValidateFunctionPropertyIt;

		if (SourceProp && TargetProp
			&& ((SourceProp->PropertyFlags & CPF_Parm) != 0)
			&& ((SourceProp->PropertyFlags & CPF_ReturnParm) == 0))
		{
			check(SourceProp->SameType(TargetProp));

			const uint8* SrcPtr = SourceProp->ContainerPtrToValuePtr<uint8>(Parameters);
			uint8* DestPtr = TargetProp->ContainerPtrToValuePtr<uint8>(ValidateFunctionParmsPtr);

			SourceProp->CopyCompleteValue(DestPtr, SrcPtr);
		}

		++FunctionPropertyIt;
		++ValidateFunctionPropertyIt;
	}

	ASValidate->RuntimeCallEvent(Object, ValidateFunctionParmsPtr);

	void* RetPtr = reinterpret_cast<void*>(reinterpret_cast<SIZE_T>(ValidateFunctionParmsPtr) + ValidateFunction->ReturnValueOffset);
	if (*reinterpret_cast<uint8*>(RetPtr) != 0)
	{
		return true;
	}

	GLastAngelscriptValidateFailureReason = ValidateFunction->GetName();
	RPC_ValidateFailed(*GLastAngelscriptValidateFailureReason);
	return false;
}

void UASFunctionNativeThunk(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	// Blueprint VM can invoke this thunk through a generated wrapper frame,
	// but CurrentNativeFunction still points at the authoritative native callee.
	UASFunction* Function = Cast<UASFunction>(Stack.CurrentNativeFunction);
	if (Function == nullptr)
	{
		Function = Cast<UASFunction>(Stack.Node);
	}
	check(Function != nullptr);

	if (Function->HasAnyFunctionFlags(FUNC_NetValidate)
		&& Stack.Node == Function
		&& Stack.Locals != nullptr)
	{
		if (ExecuteValidateFunctionForProcessEvent(Function, Object, Stack.Locals))
		{
			Function->RuntimeCallEvent(Object, Stack.Locals);
		}
		return;
	}

	Function->RuntimeCallFunction(Object, Stack, RESULT_PARAM);
}

void UASFunction::RuntimeCallEvent(UObject* Object, void* Parms)
{
#if WITH_EDITOR
	TGuardValue ScopeThreadSafe(GIsInAngelscriptThreadSafeFunction, true);
#endif

	AngelscriptCallFromParms<true, false>(this, Object, Parms);
}

UFunction* UASFunction::GetRuntimeValidateFunction()
{
	return ValidateFunction;
}

void UASFunction_NotThreadSafe::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	AngelscriptCallFromBPVM<false, false>(this, Object, Stack, RESULT_PARAM);
}

void UASFunction_NotThreadSafe::RuntimeCallEvent(UObject* Object, void* Parms)
{
	AngelscriptCallFromParms<false, false>(this, Object, Parms);
}
