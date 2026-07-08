#pragma once

#include "ClassGenerator/ASClass.h"

#include "AngelscriptEngine.h"
#include "AngelscriptPerformanceStats.h"

#include "UObject/ScriptMacros.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_config.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptobject.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

#ifdef _MSC_VER
#pragma warning(disable : 4191)
#endif

#ifndef AS_ENSURE
#define AS_ENSURE ensureMsgf
#endif

FORCEINLINE bool CheckGameThreadExecution(UASFunction* Function)
{
#if !UE_BUILD_TEST && !UE_BUILD_SHIPPING
	// During initial compile we are allowed to do gamethread stuff in other threads
	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		if (!CurrentEngine->IsInitialCompileFinished())
		{
			return true;
		}
	}

#if WITH_EDITOR
	auto* ConstructingObject = UASClass::GetConstructingASObject();
	if (!IsInGameThread() || ConstructingObject != nullptr)
#else
	if (!IsInGameThread())
#endif
	{
		AS_ENSURE(false,
			TEXT("BlueprintEvent/BlueprintOverride %s is being called from a `default` statement or on a different thread.\n")
			TEXT("This is not allowed unless declared as thread safe. (default statements can run in the async loading thread)"),
			*Function->GetPathName());
		return false;
	}
#endif

	return true;
}

FORCEINLINE static void VerifyScriptVirtualResolved(UASFunction* Function, UObject* Object)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	asCScriptFunction* VirtualScriptFunction = (asCScriptFunction*)Function->ScriptFunction;
	if (VirtualScriptFunction->vfTableIdx == -1)
		return;

	//asCObjectType* ObjectType = (asCObjectType*)Object->GetClass()->ScriptTypePtr;
	UASClass* asClass = UASClass::GetFirstASClass(Object);
	if (asClass == nullptr) return;

	asCObjectType* ObjectType = (asCObjectType*)asClass->ScriptTypePtr;
	checkSlow(ObjectType != nullptr);
	checkSlow(VirtualScriptFunction->vfTableIdx >= 0);
	checkSlow(VirtualScriptFunction->vfTableIdx < (int)ObjectType->virtualFunctionTable.GetLength());

	asCScriptFunction* RealScriptFunction = ObjectType->virtualFunctionTable[VirtualScriptFunction->vfTableIdx];
	check(RealScriptFunction == VirtualScriptFunction);
#endif
}

FORCEINLINE static asCScriptFunction* ResolveScriptVirtual(UASFunction* Function, UObject* Object)
{
	asCScriptFunction* VirtualScriptFunction = (asCScriptFunction*)Function->ScriptFunction;
	if (VirtualScriptFunction->vfTableIdx == -1)
		return VirtualScriptFunction;

	UASClass* asClass = UASClass::GetFirstASClass(Object);
	if (asClass == nullptr) return VirtualScriptFunction;

	asCObjectType* ObjectType = (asCObjectType*)asClass->ScriptTypePtr;
	if (ObjectType == nullptr)
		return VirtualScriptFunction;
	checkSlow(VirtualScriptFunction->vfTableIdx >= 0);
	checkSlow(VirtualScriptFunction->vfTableIdx < (int)ObjectType->virtualFunctionTable.GetLength());

	asCScriptFunction* RealScriptFunction = ObjectType->virtualFunctionTable[VirtualScriptFunction->vfTableIdx];
	return RealScriptFunction ? RealScriptFunction : VirtualScriptFunction;
}

template<typename TContext>
FORCEINLINE static bool PrepareAngelscriptContext(TContext& Context, asIScriptFunction* ScriptFunction, const TCHAR* Callsite)
{
	return PrepareAngelscriptContextWithLog(Context, ScriptFunction, Callsite);
}

#define AS_PREPARE_CONTEXT_OR_RETURN(Context, Function) \
	if (!PrepareAngelscriptContext(Context, Function, *GetPathName())) \
	{ \
		return; \
	}

#define AS_PREPARE_CONTEXT_OR_RETURN_VALUE(Context, Function, Value) \
	if (!PrepareAngelscriptContext(Context, Function, *GetPathName())) \
	{ \
		return Value; \
	}

#define AS_PREPARE_CONTEXT_OR_SET_RESULT(Context, Function, Address, Value) \
	if (!PrepareAngelscriptContext(Context, Function, *GetPathName())) \
	{ \
		*(Address) = Value; \
		return; \
	}

template<bool TThreadSafe, bool TNonVirtual>
static FORCEINLINE_DEBUGGABLE void AngelscriptCallFromBPVM(UASFunction* ASFunction, UObject* Object, FFrame& Stack, RESULT_DECL)
{
#if AS_CAN_HOTRELOAD
	if (ASFunction->ScriptFunction == nullptr)
		return;
#endif

	if constexpr (!TThreadSafe)
	{
		if (!CheckGameThreadExecution(ASFunction))
			return;
	}

	asCScriptFunction* ScriptFunction = (asCScriptFunction*)ASFunction->ScriptFunction;
	asJITFunction JitFunction = nullptr;
	if constexpr (TNonVirtual)
	{
		JitFunction = ASFunction->JitFunction;
	}
	else
	{
		ScriptFunction = ResolveScriptVirtual(ASFunction, Object);
		JitFunction = ScriptFunction->jitFunction;
	}

	if (!TThreadSafe && JitFunction != nullptr)
	{
		AS_PERF_SCOPE_RUNTIME_CALL_BPVM_JIT();

		UObject* NewWorldContext = nullptr;
		FScriptExecution Execution(FAngelscriptEngine::GameThreadTLD);

		uint8* ArgStack = (uint8*)FMemory_Alloca(ASFunction->ArgStackSize);

		int32 ArgumentCount = ASFunction->Arguments.Num();
		asDWORD* VMArgs = (asDWORD*)FMemory_Alloca(8 * ArgumentCount + 16);
		asDWORD* VMArgStart = VMArgs;

		if (!ASFunction->HasAnyFunctionFlags(FUNC_Static))
		{
			NewWorldContext = Object;
			*(void**)VMArgs = Object;
			VMArgs += 2;
		}

		if (ASFunction->ReturnArgument.VMBehavior == UASFunction::EArgumentVMBehavior::ReturnObjectPOD)
		{
			checkSlow(ScriptFunction->DoesReturnOnStack());

			*(void**)VMArgs = RESULT_PARAM;
			VMArgs += 2;
		}
		else if (ASFunction->ReturnArgument.VMBehavior == UASFunction::EArgumentVMBehavior::ReturnObjectValue)
		{
			checkSlow(ScriptFunction->DoesReturnOnStack());

			// The BP VM already initialized the return value, so we need to destruct it,
			// because it will be re-initialized by the AS VM
			ASFunction->ReturnArgument.Type.DestructValue(RESULT_PARAM);

			*(void**)VMArgs = RESULT_PARAM;
			VMArgs += 2;
		}

		for (int32 i = 0; i < ArgumentCount; ++i)
		{
			auto& Arg = ASFunction->Arguments[i];
			switch (Arg.VMBehavior)
			{
				case UASFunction::EArgumentVMBehavior::FloatExtendedToDouble:
				{
					float Value = 0;
					Stack.StepCompiledIn<FProperty>(&Value);

					*(double*)VMArgs = (double)Value;
					VMArgs += 2;
				}
				break;
				case UASFunction::EArgumentVMBehavior::WorldContextObject:
				{
					void* Ptr = nullptr;
					Stack.StepCompiledIn<FProperty>(&Ptr);

					NewWorldContext = (UObject*)Ptr;
					*(void**)VMArgs = Ptr;
					VMArgs += 2;

					AS_ENSURE(NewWorldContext != nullptr, TEXT("Null WorldContext passed into static function call '%s'"), *ASFunction->GetName());
				}
				break;
				case UASFunction::EArgumentVMBehavior::ObjectPointer:
				{
					void* Ptr = nullptr;
					Stack.StepCompiledIn<FProperty>(&Ptr);

					*(void**)VMArgs = Ptr;
					VMArgs += 2;
				}
				break;
				case UASFunction::EArgumentVMBehavior::ReferencePOD:
				{
					void* StackPtr = ArgStack + Arg.StackOffset;

					uint8& RefValue = Stack.StepCompiledInRef<FProperty, uint8>(StackPtr);

					*(void**)VMArgs = &RefValue;
					VMArgs += 2;
				}
				break;
				case UASFunction::EArgumentVMBehavior::Reference:
				{
					void* StackPtr = ArgStack + Arg.StackOffset;
					Arg.Type.ConstructValue(StackPtr);

					uint8& RefValue = Stack.StepCompiledInRef<FProperty, uint8>(StackPtr);
					*(void**)VMArgs = &RefValue;
					VMArgs += 2;
				}
				break;
				case UASFunction::EArgumentVMBehavior::Value1Byte:
				case UASFunction::EArgumentVMBehavior::Value2Byte:
				case UASFunction::EArgumentVMBehavior::Value4Byte:
				{
					asDWORD Value = 0;
					Stack.StepCompiledIn<FProperty>(&Value);

					*(asDWORD*)VMArgs = Value;
					VMArgs += 1;
				}
				break;
				case UASFunction::EArgumentVMBehavior::Value8Byte:
				{
					asQWORD Value = 0;
					Stack.StepCompiledIn<FProperty>(&Value);

					*(asQWORD*)VMArgs = Value;
					VMArgs += 2;
				}
				break;
				default:
					UE_ASSUME(false);
				break;
			}

		}

		if (ASFunction->bIsWorldContextGenerated)
		{
			P_GET_OBJECT(UObject, ArgWorldContext);
			NewWorldContext = ArgWorldContext;

			AS_ENSURE(NewWorldContext != nullptr, TEXT("Null WorldContext passed into static function call '%s'"), *ASFunction->GetName());
		}

		P_FINISH;

		asQWORD OutValue = 0;
		FAngelscriptGameThreadScopeWorldContext WorldContext(NewWorldContext);
		(JitFunction)(Execution, VMArgStart, &OutValue);

		switch (ASFunction->ReturnArgument.VMBehavior)
		{
		case UASFunction::EArgumentVMBehavior::ReferencePOD:
		{
			// Special case for value types, we need to actually copy these back into the parm struct
			void* RetValue = (void*&)OutValue;

			// We may not have a return address, if the angelscript function threw an
			// exception for example.
			if (RetValue != nullptr)
				FMemory::Memcpy(RESULT_PARAM, RetValue, ASFunction->ReturnArgument.ValueBytes);
		}
		break;
		case UASFunction::EArgumentVMBehavior::Reference:
		{
			// Special case for value types, we need to actually copy these back into the parm struct
			void* RetValue = (void*&)OutValue;

			// We may not have a return address, if the angelscript function threw an
			// exception for example.
			if (RetValue != nullptr)
				ASFunction->ReturnArgument.Type.CopyValue(RetValue, RESULT_PARAM);
		}
		break;
		case UASFunction::EArgumentVMBehavior::FloatExtendedToDouble:
			*(float*)RESULT_PARAM = (float)(double&)OutValue;
		break;
		case UASFunction::EArgumentVMBehavior::ReturnObjectPOD:
		case UASFunction::EArgumentVMBehavior::ReturnObjectValue:
		break;
		case UASFunction::EArgumentVMBehavior::Value1Byte:
			*(asBYTE*)RESULT_PARAM = (asBYTE)(asDWORD&)OutValue;
		break;
		case UASFunction::EArgumentVMBehavior::Value2Byte:
			*(asWORD*)RESULT_PARAM = (asWORD)(asDWORD&)OutValue;
		break;
		case UASFunction::EArgumentVMBehavior::Value4Byte:
			*(asDWORD*)RESULT_PARAM = (asDWORD&)OutValue;
		break;
		case UASFunction::EArgumentVMBehavior::Value8Byte:
			*(asQWORD*)RESULT_PARAM = OutValue;
		break;
		case UASFunction::EArgumentVMBehavior::None:
		break;
		default:
			UE_ASSUME(false);
		break;
		}

		for (int32 i = 0, Num = ASFunction->DestroyArguments.Num(); i < Num; ++i)
		{
			auto& Arg = ASFunction->DestroyArguments[i];
			Arg.Type.DestructValue(ArgStack + Arg.StackOffset);
		}
	}
	else
	{
		uint8* ArgStack = (uint8*)FMemory_Alloca(ASFunction->ArgStackSize);

		const bool bInGameThread = !TThreadSafe || IsInGameThread();
		bool bChangedWorldContext = false;
		UObject* PrevWorldContext = nullptr;

		// Scope because FAngelscriptPooledContextBase needs to be destructed before we reset the world context
		{
			FAngelscriptPooledContextBase Context(ScriptFunction->GetEngine());
			if (!PrepareAngelscriptContext(Context, ScriptFunction, *ASFunction->GetPathName()))
				return;

			for (int32 i = 0, Num = ASFunction->Arguments.Num(); i < Num; ++i)
			{
				auto& Arg = ASFunction->Arguments[i];

				FAngelscriptType::FArgData Data;
				Data.StackPtr = ArgStack + Arg.StackOffset;

				Arg.Type.SetArgument(i, Context, Stack, Data);
			}

			if (!ASFunction->HasAnyFunctionFlags(FUNC_Static))
			{
				if (bInGameThread)
				{
					PrevWorldContext = (UObject*)FAngelscriptEngine::GetAmbientWorldContext();
					FAngelscriptEngine::AssignWorldContext(Object);
					bChangedWorldContext = true;
				}
				Context->SetObject(Object);
			}
			else if (ASFunction->bIsWorldContextGenerated)
			{
				checkSlow(ASFunction->WorldContextIndex == ASFunction->Arguments.Num());

				P_GET_OBJECT(UObject, WorldContext);

				if (bInGameThread)
				{
					PrevWorldContext = (UObject*)FAngelscriptEngine::GetAmbientWorldContext();
					FAngelscriptEngine::AssignWorldContext(WorldContext);
					bChangedWorldContext = true;

					AS_ENSURE(WorldContext != nullptr, TEXT("Null WorldContext passed into static function call '%s'"), *ASFunction->GetName());
				}
			}
			else if (ASFunction->WorldContextIndex >= 0)
			{
				UObject* WorldContext = *(UObject**)Context->GetAddressOfArg(ASFunction->WorldContextIndex);

				if (bInGameThread)
				{
					PrevWorldContext = (UObject*)FAngelscriptEngine::GetAmbientWorldContext();
					FAngelscriptEngine::AssignWorldContext(WorldContext);
					bChangedWorldContext = true;

					AS_ENSURE(WorldContext != nullptr, TEXT("Null WorldContext passed into static function call '%s'"), *ASFunction->GetName());
				}
			}
			else
			{
				// All static functions need a world context pin right now
				check(false);
			}

			P_FINISH;

			Context->Execute();

			if (ASFunction->ReturnArgument.Property != nullptr)
			{
				if (ASFunction->ReturnArgument.VMBehavior == UASFunction::EArgumentVMBehavior::Reference)
				{
					void* RetValue = Context->GetReturnAddress();

					// We may not have a return address, if the angelscript function threw an
					// exception for example.
					if (RetValue != nullptr)
						ASFunction->ReturnArgument.Type.CopyValue(RetValue, RESULT_PARAM);
				}
				else if (ASFunction->ReturnArgument.VMBehavior == UASFunction::EArgumentVMBehavior::ReferencePOD)
				{
					void* RetValue = Context->GetReturnAddress();

					// We may not have a return address, if the angelscript function threw an
					// exception for example.
					if (RetValue != nullptr)
						FMemory::Memcpy(RESULT_PARAM, RetValue, ASFunction->ReturnArgument.ValueBytes);
				}
				else
				{
					ASFunction->ReturnArgument.Type.GetReturnValue(Context, RESULT_PARAM);
				}
			}

			for (int32 i = 0, Num = ASFunction->DestroyArguments.Num(); i < Num; ++i)
			{
				auto& Arg = ASFunction->DestroyArguments[i];
				Arg.Type.DestructValue(ArgStack + Arg.StackOffset);
			}
		}

		if (!TThreadSafe || bChangedWorldContext)
			FAngelscriptEngine::AssignWorldContext(PrevWorldContext);
	}
}

template<bool TThreadSafe, bool TNonVirtual>
static FORCEINLINE_DEBUGGABLE void AngelscriptCallFromParms(UASFunction* ASFunction, UObject* Object, void* Parms)
{

#if AS_CAN_HOTRELOAD
	if (ASFunction->ScriptFunction == nullptr)
		return;
#endif

	if constexpr (!TThreadSafe)
	{
		if (!CheckGameThreadExecution(ASFunction))
			return;
	}

	asCScriptFunction* ScriptFunction = (asCScriptFunction*)ASFunction->ScriptFunction;
	asJITFunction_ParmsEntry JitFunction;

	if constexpr (TNonVirtual)
	{
		JitFunction = ASFunction->JitFunction_ParmsEntry;
	}
	else
	{
		ScriptFunction = ResolveScriptVirtual(ASFunction, Object);
		JitFunction = ScriptFunction->jitFunction_ParmsEntry;
	}

	if (!TThreadSafe && JitFunction != nullptr)
	{
		UObject* NewWorldContext = nullptr;
		if (!ASFunction->HasAnyFunctionFlags(FUNC_Static))
		{
			NewWorldContext = Object;
		}
		else
		{
			checkSlow(ASFunction->WorldContextIndex >= 0);
			NewWorldContext = *(UObject**)((SIZE_T)Parms + ASFunction->WorldContextOffsetInParms);
		}

		FAngelscriptGameThreadScopeWorldContext WorldContext(NewWorldContext);
		FScriptExecution Execution(FAngelscriptEngine::GameThreadTLD);

		(JitFunction)(Execution, Object, Parms);
	}
	else
	{
		AS_PERF_SCOPE_RUNTIME_CALL_PARMS_CONTEXT();

		const bool bInGameThread = !TThreadSafe || IsInGameThread();
		bool bChangedWorldContext = false;
		UObject* PrevWorldContext = nullptr;

		// Scope because FAngelscriptPooledContextBase needs to be destructed before we reset the world context
		{
			FAngelscriptPooledContextBase Context(ScriptFunction->GetEngine());
			if (!PrepareAngelscriptContext(Context, ScriptFunction, *ASFunction->GetPathName()))
				return;

			for(int32 i = 0, Num = ASFunction->Arguments.Num(); i < Num; ++i)
			{
				auto& Arg = ASFunction->Arguments[i];
				void* ValuePtr = (void*)((SIZE_T)Parms + Arg.PosInParmStruct);

				switch (Arg.ParmBehavior)
				{
				case UASFunction::EArgumentParmBehavior::Reference:
					// Special case for references to values in the parameter struct
					Context->SetArgAddress(i, ValuePtr);
				break;
				case UASFunction::EArgumentParmBehavior::Value1Byte:
					Context->SetArgByte(i, *(asBYTE*)ValuePtr);
				break;
				case UASFunction::EArgumentParmBehavior::Value2Byte:
					Context->SetArgWord(i, *(asWORD*)ValuePtr);
				break;
				case UASFunction::EArgumentParmBehavior::Value4Byte:
					Context->SetArgDWord(i, *(asDWORD*)ValuePtr);
				break;
				case UASFunction::EArgumentParmBehavior::Value8Byte:
					Context->SetArgQWord(i, *(asQWORD*)ValuePtr);
				break;
				case UASFunction::EArgumentParmBehavior::FloatExtendedToDouble:
					// -4 Indicates an unreal float upgraded to a double in script
					Context->SetArgDouble(i, (double)*(float*)ValuePtr);
				break;
				default:
					UE_ASSUME(false);
				break;
				}
			}

			if (!ASFunction->HasAnyFunctionFlags(FUNC_Static))
			{
				if (bInGameThread)
				{
					PrevWorldContext = (UObject*)FAngelscriptEngine::GetAmbientWorldContext();
					FAngelscriptEngine::AssignWorldContext(Object);
					bChangedWorldContext = true;
				}
				Context->SetObject(Object);
			}
			else if(ASFunction->bIsWorldContextGenerated)
			{
				UObject* WorldContext = *(UObject**)((SIZE_T)Parms + ASFunction->WorldContextOffsetInParms);
				checkSlow(ASFunction->WorldContextIndex == ASFunction->Arguments.Num());

				if (bInGameThread)
				{
					PrevWorldContext = (UObject*)FAngelscriptEngine::GetAmbientWorldContext();
					FAngelscriptEngine::AssignWorldContext(WorldContext);
					bChangedWorldContext = true;

					AS_ENSURE(WorldContext != nullptr, TEXT("Null WorldContext passed into static function call '%s'"), *ASFunction->GetName());
				}
			}
			else if (ASFunction->WorldContextIndex >= 0)
			{
				UObject* WorldContext = *(UObject**)Context->GetAddressOfArg(ASFunction->WorldContextIndex);

				if (bInGameThread)
				{
					PrevWorldContext = (UObject*)FAngelscriptEngine::GetAmbientWorldContext();
					FAngelscriptEngine::AssignWorldContext(WorldContext);
					bChangedWorldContext = true;

					AS_ENSURE(WorldContext != nullptr, TEXT("Null WorldContext passed into static function call '%s'"), *ASFunction->GetName());
				}
			}
			else
			{
				// All static functions need a world context pin right now
				check(false);
			}

			Context->Execute();

			if (ASFunction->ReturnArgument.Property != nullptr)
			{
				void* RetPtr = (void*)((SIZE_T)Parms + ASFunction->ReturnArgument.PosInParmStruct);
				switch (ASFunction->ReturnArgument.ParmBehavior)
				{
				case UASFunction::EArgumentParmBehavior::ReturnObjectPointer:
					// Special case for object pointers, these are returned in a different register
					*(void**)RetPtr = Context->GetReturnObject();
				break;
				case UASFunction::EArgumentParmBehavior::Reference:
				{
					// Special case for value types, we need to actually copy these back into the parm struct
					void* RetValue = Context->GetReturnAddress();

					// We may not have a return address, if the angelscript function threw an
					// exception for example.
					if (RetValue != nullptr)
						ASFunction->ReturnArgument.Type.CopyValue(RetValue, RetPtr);
				}
				break;
				case UASFunction::EArgumentParmBehavior::ReferencePOD:
				{
					// Special case for value types, we need to actually copy these back into the parm struct
					void* RetValue = Context->GetReturnAddress();

					// We may not have a return address, if the angelscript function threw an
					// exception for example.
					if (RetValue != nullptr)
						FMemory::Memcpy(RetPtr, RetValue, ASFunction->ReturnArgument.ValueBytes);
				}
				break;
				case UASFunction::EArgumentParmBehavior::Value1Byte:
					*(uint8*)RetPtr = Context->GetReturnByte();
				break;
				case UASFunction::EArgumentParmBehavior::Value2Byte:
					*(uint16*)RetPtr = Context->GetReturnWord();
				break;
				case UASFunction::EArgumentParmBehavior::Value4Byte:
					*(uint32*)RetPtr = Context->GetReturnDWord();
				break;
				case UASFunction::EArgumentParmBehavior::Value8Byte:
					*(uint64*)RetPtr = Context->GetReturnQWord();
				break;
				case UASFunction::EArgumentParmBehavior::FloatExtendedToDouble:
					// -4 Indicates an unreal float upgraded to a double in script
					*(float*)RetPtr = (float)Context->GetReturnDouble();
				break;
				default:
					UE_ASSUME(false);
				break;
				}
			}
		}

		if (!TThreadSafe || bChangedWorldContext)
		{
			FAngelscriptEngine::AssignWorldContext(PrevWorldContext);
		}
	}
}

FORCEINLINE void MakeRawJITCall_NoParam(UObject* Object, asJITFunction_Raw InFunction)
{

	checkSlow(FAngelscriptEngine::GameThreadTLD == asCThreadManager::GetLocalData());

	FAngelscriptGameThreadScopeWorldContext WorldContext(Object);
	FScriptExecution Execution(FAngelscriptEngine::GameThreadTLD);

	using TFuncPtr = void(*)(FScriptExecution&, void*);
	((TFuncPtr)InFunction)(
		Execution, Object
	);
}

template<typename TArgument>
FORCEINLINE void MakeRawJITCall_Arg(UObject* Object, asJITFunction_Raw InFunction, TArgument ArgValue)
{

	checkSlow(FAngelscriptEngine::GameThreadTLD == asCThreadManager::GetLocalData());

	FAngelscriptGameThreadScopeWorldContext WorldContext(Object);
	FScriptExecution Execution(FAngelscriptEngine::GameThreadTLD);

	using TFuncPtr = void(*)(FScriptExecution&, void*, TArgument Arg);
	((TFuncPtr)InFunction)(
		Execution, Object, ArgValue
	);
}

template<typename TReturnValue>
FORCEINLINE TReturnValue MakeRawJITCall_ReturnValue(UObject* Object, asJITFunction_Raw InFunction)
{

	checkSlow(FAngelscriptEngine::GameThreadTLD == asCThreadManager::GetLocalData());

	FAngelscriptGameThreadScopeWorldContext WorldContext(Object);
	FScriptExecution Execution(FAngelscriptEngine::GameThreadTLD);

	using TFuncPtr = TReturnValue(*)(FScriptExecution&, void*);
	return ((TFuncPtr)InFunction)(
		Execution, Object
	);
}

template<typename TArgument, typename TReturnValue>
FORCEINLINE TReturnValue MakeRawJITCall_Arg_ReturnValue(UObject* Object, asJITFunction_Raw InFunction, TArgument ArgValue)
{

	checkSlow(FAngelscriptEngine::GameThreadTLD == asCThreadManager::GetLocalData());

	FAngelscriptGameThreadScopeWorldContext WorldContext(Object);
	FScriptExecution Execution(FAngelscriptEngine::GameThreadTLD);

	using TFuncPtr = TReturnValue(*)(FScriptExecution&, void*, TArgument Arg);
	return ((TFuncPtr)InFunction)(
		Execution, Object, ArgValue
	);
}
