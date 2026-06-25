#include "ClassGenerator/ASClass.h"

#include "AngelscriptEngine.h"
#include "AngelscriptPerformanceStats.h"

#include "UObject/Package.h"
#include "UObject/CoreNet.h"
#include "UObject/ScriptMacros.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectThreadContext.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"

//WILL-EDIT
#include "StartAngelscriptHeaders.h"
//#include "as_config.h"
//#include "as_scriptengine.h"
//#include "as_scriptobject.h"
//#include "as_context.h"
#include "source/as_config.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptobject.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

#ifdef _MSC_VER
#pragma warning(disable : 4191)
#endif

#if !WITH_ANGELSCRIPT_HAZE
#define AS_ENSURE ensureMsgf
#else
#define AS_ENSURE devEnsure
#endif

UObject* UASClass::OverrideConstructingObject = nullptr;

#if WITH_EDITOR
thread_local bool GIsInAngelscriptThreadSafeFunction = false;
thread_local bool GIsAngelscriptWorldContextAvailable = false;

ANGELSCRIPTRUNTIME_API void SetAngelscriptWorldContextAvailable(bool bAvailable)
{
	GIsAngelscriptWorldContextAvailable = bAvailable;
}
#endif

UASClass::UASClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

namespace
{
	FString GLastAngelscriptValidateFailureReason;
}

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

void UASClass::GetLifetimeScriptReplicationList(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	for (TFieldIterator<FProperty> It(this, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty * Prop = *It;
		if (Prop != NULL && Prop->GetPropertyFlags() & CPF_Net)
		{
			OutLifetimeProps.AddUnique(FLifetimeProperty(Prop->RepIndex, Prop->GetBlueprintReplicationCondition()));
		}
	}

	UASClass* SuperScriptClass = Cast<UASClass>(GetSuperStruct());
	if (SuperScriptClass != NULL)
	{
		SuperScriptClass->GetLifetimeScriptReplicationList(OutLifetimeProps);
	}
}

UClass* UASClass::GetMostUpToDateClass()
{
#if !AS_CAN_HOTRELOAD
	return this;
#else
	if (NewerVersion == nullptr)
		return this;

	UASClass* NewerClass = NewerVersion;
	while (NewerClass->NewerVersion != nullptr)
		NewerClass = NewerClass->NewerVersion;
	return NewerClass;
#endif
}

UASClass* UASClass::GetFirstASClass(UClass* Class)
{
	UClass* Parent = Class;
	while (Parent != nullptr)
	{
		if (Cast<UASClass>(Parent) != nullptr)
			return (UASClass*)Parent;
		Parent = Parent->GetSuperClass();
	}
	return nullptr;
}

UASClass* UASClass::GetFirstASClass(UObject* Object)
{
	UClass* Parent = Object->GetClass();
	while (Parent != nullptr)
	{
		if (Cast<UASClass>(Parent) != nullptr)
			return (UASClass*)Parent;
		Parent = Parent->GetSuperClass();
	}
	return nullptr;
}

UClass* UASClass::GetFirstASOrNativeClass(UClass* Class)
{
	UClass* Parent = Class;
	while (Parent != nullptr)
	{
		if (Cast<UASClass>(Parent) != nullptr)
			return Parent;
		if (Parent->HasAnyClassFlags(CLASS_Native))
			return Parent;
		Parent = Parent->GetSuperClass();
	}
	return nullptr;
}

void UASClass::RuntimeDestroyObject(UObject* Object)
{
#if WITH_AS_DEBUGVALUES
	if (Object->Debug != nullptr)
		DebugValues.Free(Object->Debug);
#endif

	if (ScriptTypePtr == nullptr)
		return;

	auto* ScriptObject = (asCScriptObject*)(Object);
	ScriptObject->CallDestructor((asCObjectType*)ScriptTypePtr);
}

bool UASClass::IsFunctionImplementedInScript(FName InFunctionName) const
{
	UFunction* Function = FindFunctionByName(InFunctionName);
	//return Function && Function->GetOuterUClass() && Function->GetOuterUClass()->bIsScriptClass;
	UASFunction* asFunction = Cast<UASFunction>(Function);
	// UE 5.7+: after DiscardModule, UASFunction objects remain in the FuncMap
	// but with ScriptFunction set to nullptr. Check that the backing script
	// function is still alive before reporting as implemented.
	return asFunction && asFunction->ScriptFunction != nullptr && asFunction->GetOuterUClass();
}

static TArray<FObjectInitializer> CurrentObjectInitializers;
bool GConstructASObjectWithoutDefaults = false;

UObject* UASClass::GetConstructingASObject()
{
	if (UASClass::OverrideConstructingObject != nullptr)
		return UASClass::OverrideConstructingObject;

	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	auto* Initializer = ThreadContext.TopInitializer();
	if (Initializer == nullptr)
		return nullptr;
	UObject* Object = Initializer->GetObj();
	if (Object == nullptr)
		return nullptr;

	//if (Object->GetClass()->ScriptTypePtr == nullptr)
	UASClass* asClass = UASClass::GetFirstASClass(Object);
	
	if (!asClass || asClass->ScriptTypePtr == nullptr)
		return nullptr;
	return Object;
}

static thread_local UObject* GASDefaultConstructorOuter = nullptr;
UASClass::FScopeSetDefaultConstructorOuter::FScopeSetDefaultConstructorOuter(UObject* NewOuter)
{
	PrevOuter = GASDefaultConstructorOuter;
	GASDefaultConstructorOuter = NewOuter;
}

UASClass::FScopeSetDefaultConstructorOuter::~FScopeSetDefaultConstructorOuter()
{
	GASDefaultConstructorOuter = PrevOuter;
}

UObject* UASClass::GetDefaultConstructorOuter()
{
	return GASDefaultConstructorOuter != nullptr ? GASDefaultConstructorOuter : GetTransientPackage();
}

void* UASClass::AllocScriptObject(class asITypeInfo* ScriptType, size_t Size)
{
	if (ScriptType->GetFlags() & asOBJ_VALUE)
	{
		void* Mem = FMemory::Malloc(Size, ScriptType->alignment);
		FMemory::Memzero(Mem, Size);
		return Mem;
	}

	UASClass* Class = (UASClass*)ScriptType->GetUserData();
	/*
	
		This code comes from StaticConstructObject_Internal.

		In order to split it into an allocate and a native construct part
		it has been copied and split into AllocScriptObject and FinishConstructObject.

	*/

	auto* InClass = Class;
	auto* InOuter = GetDefaultConstructorOuter();
	auto InName = NAME_None;
	EObjectFlags InFlags = RF_NoFlags;
	auto InternalSetFlags = EInternalObjectFlags::None;
	UObject* InTemplate = nullptr;
	bool bCopyTransientsFromClassDefaults = false;
	FObjectInstancingGraph* InInstanceGraph = nullptr;
	bool bAssumeTemplateIsArchetype = false;

	UObject* Result = NULL;

	if (InOuter == GetTransientPackage())
		InFlags |= RF_Transient;

	// Subobjects are always created in the constructor, no need to re-create them unless their archetype != CDO or they're blueprint generated.
	// If the existing subobject is to be re-used it can't have BeginDestroy called on it so we need to pass this information to StaticAllocateObject.	
	const bool bIsNativeClass = InClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic);
	const bool bIsNativeFromCDO = bIsNativeClass &&
		(
			!InTemplate || 
			(InName != NAME_None && (bAssumeTemplateIsArchetype || InTemplate == UObject::GetArchetypeFromRequiredInfo(InClass, InOuter, InName, InFlags)))
		);
	const bool bCanRecycleSubObjects = false; // No recycling, we won't know we've done it in FinishConstructObject
	bool bRecycledSubobject = false;	
	Result = StaticAllocateObject(InClass, InOuter, InName, InFlags, InternalSetFlags, bCanRecycleSubObjects, &bRecycledSubobject);
	check(Result != NULL);

	// We delay destroying the initializer until we know our constructor has been called,
	// since it's script calling the constructor instead of our ClassConstructor static function.
	FObjectInitializer& Initializer = CurrentObjectInitializers.Emplace_GetRef(
		Result, nullptr, EObjectInitializerOptions::InitializeProperties, nullptr
	);

	(*Class->ClassConstructor)( Initializer );

	return Result;
}

static FORCEINLINE_DEBUGGABLE void ExecuteDefaultsFunctions(UObject* Object, UASClass* Class)
{
	if (Class->OwnerScriptEngine == nullptr)
		return;

	UASClass* DefaultsClass = Class;
	UASClass* ParentDefaults = Cast<UASClass>(Class->GetSuperClass());

	if (ParentDefaults == nullptr)
	{
		if (Class->DefaultsFunction != nullptr)
		{
			FAngelscriptContext Context(Object, Class->DefaultsFunction->GetEngine());
			if (!PrepareAngelscriptContext(Context, Class->DefaultsFunction, *Class->GetPathName()))
				return;
			Context->m_executeVirtualCall = false;
			Context->SetObject(Object);
			Context->Execute();
		}
	}
	else
	{
		TArray<asIScriptFunction*, TFixedAllocator<32>> DefaultsFunctions;
		while (DefaultsClass != nullptr)
		{
			if (DefaultsClass->DefaultsFunction != nullptr && DefaultsClass->OwnerScriptEngine != nullptr)
				DefaultsFunctions.Add(DefaultsClass->DefaultsFunction);
			DefaultsClass = Cast<UASClass>(DefaultsClass->GetSuperClass());
		}

		for (int32 i = DefaultsFunctions.Num() - 1; i >= 0; --i)
		{
			FAngelscriptContext Context(Object, DefaultsFunctions[i]->GetEngine());
			if (!PrepareAngelscriptContext(Context, DefaultsFunctions[i], *Class->GetPathName()))
				return;
			Context->m_executeVirtualCall = false;
			Context->SetObject(Object);
			Context->Execute();
		}
	}
}

static FORCEINLINE_DEBUGGABLE void ExecuteConstructFunction(UObject* Object, UASClass* Class)
{
	if (Class->ConstructFunction != nullptr && Class->OwnerScriptEngine != nullptr)
	{
		FAngelscriptContext Context(Object, Class->ConstructFunction->GetEngine());
		if (!PrepareAngelscriptContext(Context, Class->ConstructFunction, *Class->GetPathName()))
			return;
		Context->SetObject(Object);
		Context->Execute();
	}
}

void UASClass::FinishConstructObject(class asIScriptObject* ScriptObject, class asITypeInfo* ScriptType)
{
	UObject* Object = FAngelscriptEngine::AngelscriptToUObject(ScriptObject);
	const bool bIsScriptAllocation = CurrentObjectInitializers.Num() != 0 && CurrentObjectInitializers.Last().GetObj() == Object;

	if (bIsScriptAllocation)
	{
		UASClass* TopClass = UASClass::GetFirstASClass(Object);

#if AS_CAN_HOTRELOAD
		bool bIsInTree = false;
		UASClass* CheckClass = TopClass;
		while (CheckClass != nullptr)
		{
			if (CheckClass->ScriptTypePtr == ScriptType)
			{
				bIsInTree = true;
				break;
			}
			CheckClass = Cast<UASClass>(CheckClass->GetSuperClass());
		}

		if (!bIsInTree)
		{
			CurrentObjectInitializers.RemoveAt(CurrentObjectInitializers.Num() - 1, 1, EAllowShrinking::No);
			return;
		}
#endif

		if (TopClass->ScriptTypePtr == ScriptType)
		{
			CurrentObjectInitializers.RemoveAt(CurrentObjectInitializers.Num() - 1, 1, EAllowShrinking::No);

			// Run the defaults function now that we've finished constructing the childmost script class
			ExecuteDefaultsFunctions(Object, TopClass);
		}
	}
}

static FORCEINLINE_DEBUGGABLE void ApplyOverrideComponents(const FObjectInitializer& Initializer, AActor* Actor, UASClass* ScriptClass)
{
	// Child classes should apply override components first, this is the expected order that C++ does it in
	// The object initializer understands that child overrides are applied before parents
	for(int32 i = 0, Count = ScriptClass->OverrideComponents.Num(); i < Count; ++i)
	{
		auto& Override = ScriptClass->OverrideComponents[i];

		UClass* ComponentClass = Override.ComponentClass;
#if AS_CAN_HOTRELOAD
		//ComponentClass = ComponentClass->GetMostUpToDateClass();
		UASClass* asClass = Cast<UASClass>(ComponentClass);
		if (asClass != nullptr)
			ComponentClass = asClass->GetMostUpToDateClass();
#endif

		Initializer.SetDefaultSubobjectClass(Override.OverrideComponentName, ComponentClass);
	}

	// Parent classes afterward
	if (UASClass* ParentClass = Cast<UASClass>(ScriptClass->GetSuperClass()))
	{
		ApplyOverrideComponents(Initializer, Actor, ParentClass);
	}
}
static FORCEINLINE_DEBUGGABLE void FillOverrideComponentVariablesForClass(AActor* Actor, UASClass* ScriptClass)
{
	for(int32 i = 0, Count = ScriptClass->OverrideComponents.Num(); i < Count; ++i)
	{
		auto& Override = ScriptClass->OverrideComponents[i];
		UActorComponent** VariablePtr = (UActorComponent**)((SIZE_T)Actor + Override.VariableOffset);
		UActorComponent* OverrideComponent = Cast<UActorComponent>(Actor->GetDefaultSubobjectByName(Override.OverrideComponentName));

		if (OverrideComponent == nullptr)
		{
			for (auto* CheckComponent : Actor->GetComponents())
			{
				if (CheckComponent->GetFName() == Override.OverrideComponentName)
				{
					OverrideComponent = CheckComponent;
					break;
				}
			}
		}

		if (OverrideComponent != nullptr)
			*VariablePtr = OverrideComponent;
	}
}

static FORCEINLINE_DEBUGGABLE void FillOverrideComponentVariables(AActor* Actor, UASClass* ScriptClass)
{
	if (UASClass* ParentClass = Cast<UASClass>(ScriptClass->GetSuperClass()))
		FillOverrideComponentVariables(Actor, ParentClass);

	FillOverrideComponentVariablesForClass(Actor, ScriptClass);
}


static FORCEINLINE_DEBUGGABLE void CreateDefaultComponents(const FObjectInitializer& Initializer, AActor* Actor, UASClass* ScriptClass)
{
	// Parent class should get a chance to create components first
	if (UASClass* ParentClass = Cast<UASClass>(ScriptClass->GetSuperClass()))
	{
		CreateDefaultComponents(Initializer, Actor, ParentClass);
	}

	TArray<TPair<USceneComponent*, int32>, TInlineAllocator<4>> DelayedComponentAttach;
	for(int32 i = 0, Count = ScriptClass->DefaultComponents.Num(); i < Count; ++i)
	{
		auto& DefaultComponent = ScriptClass->DefaultComponents[i];

		UActorComponent* Component;

		UClass* ComponentClass = DefaultComponent.ComponentClass;
#if AS_CAN_HOTRELOAD
		//ComponentClass = ComponentClass->GetMostUpToDateClass();
		UASClass* asClass = Cast<UASClass>(ComponentClass);
		if (asClass != nullptr)
			ComponentClass = asClass->GetMostUpToDateClass();
#endif

		if (WITH_EDITOR && DefaultComponent.bEditorOnly)
		{
			Component = (UActorComponent*)Initializer.CreateEditorOnlyDefaultSubobject(
				Actor,
				DefaultComponent.ComponentName,
				ComponentClass,
				false
			);
		}
		else
		{
			Component = (UActorComponent*)Initializer.CreateDefaultSubobject(
				Actor,
				DefaultComponent.ComponentName,
				ComponentClass,
				ComponentClass,
				true,
				false
			);
		}

		// Handle the case where we tried to create an abstract component on a non-abstract actor class,
		// this should give an error.
		AS_ENSURE(!ComponentClass->HasAnyClassFlags(CLASS_Abstract) || Actor->GetClass()->HasAnyClassFlags(CLASS_Abstract),
			TEXT("Attempted to instantiate abstract component of type %s on non-abstract actor of type %s"),
			*ComponentClass->GetName(), *Actor->GetClass()->GetName()
		);

		// Set the new component on the variable in the script class
		UActorComponent** VariablePtr = (UActorComponent**)((SIZE_T)Actor + DefaultComponent.VariableOffset);
		*VariablePtr = Component;

		// Handle attachments for scene components
		if (auto* SceneComponent = Cast<USceneComponent>(Component))
		{
			if (DefaultComponent.bIsRoot)
			{
				auto* PreviousRoot = Actor->GetRootComponent();

				// Component should become the root component
				SceneComponent->SetupAttachment(nullptr);
				Actor->SetRootComponent(SceneComponent);

				// Attach previous root component to this component
				if (PreviousRoot != nullptr)
					PreviousRoot->SetupAttachment(SceneComponent);
			}
			else if (DefaultComponent.Attach == NAME_None)
			{
				if (Actor->GetRootComponent() == nullptr)
				{
					// Component should become the root component, since we don't have any
					SceneComponent->SetupAttachment(nullptr);
					Actor->SetRootComponent(SceneComponent);
				}
				else
				{
					// Component should automatically be attached to the existing root component
					SceneComponent->SetupAttachment(Actor->GetRootComponent(), DefaultComponent.AttachSocket);
				}
			}
			else
			{
				// Attach the component later, when all components have been created
				DelayedComponentAttach.Add( TPair<USceneComponent*, int32>{ SceneComponent, i } );
			}
		}
	}

	for (auto& DelayedAttach : DelayedComponentAttach)
	{
		auto& DefaultComponent = ScriptClass->DefaultComponents[DelayedAttach.Value];

		// Find the component to attach to
		USceneComponent* AttachTo = nullptr;
		for (auto* CheckComponent : Actor->GetComponents())
		{
			if (CheckComponent->GetFName() == DefaultComponent.Attach)
			{
				if (auto* CheckSceneComponent = Cast<USceneComponent>(CheckComponent))
				{
					AttachTo = CheckSceneComponent;
					break;
				}
			}
		}

		// If we can't find the thing to attach to, attach to the root instead
		if (AttachTo == nullptr)
		{
			if (Actor->GetRootComponent() != nullptr)
			{
				DelayedAttach.Key->SetupAttachment(Actor->GetRootComponent(), DefaultComponent.AttachSocket);
			}
			else
			{
				DelayedAttach.Key->SetupAttachment(nullptr);
				Actor->SetRootComponent(DelayedAttach.Key);
			}
		}
		else
		{
			DelayedAttach.Key->SetupAttachment(AttachTo, DefaultComponent.AttachSocket);
		}
	}

	// Fill any override component variables with the right components
	FillOverrideComponentVariablesForClass(Actor, ScriptClass);
}

//WILL-EDIT



void UASClass::StaticActorConstructor(const FObjectInitializer& Initializer)
{
	UObject* Object = Initializer.GetObj();
	UASClass* Class = GetFirstASClass(Object);
	asCObjectType* ScriptType = (asCObjectType*)Class->ScriptTypePtr;
	AActor* Actor = (AActor*)Object;

#if AS_CAN_HOTRELOAD
	const bool bApplyDefaults = !GConstructASObjectWithoutDefaults;
	GConstructASObjectWithoutDefaults = false;
#else
	const bool bApplyDefaults = true;
#endif

	// Apply override components
	ApplyOverrideComponents(Initializer, Actor, Class);

	// We need to run the C++ constructor first so everything is valid
	Class->CodeSuperClass->ClassConstructor(Initializer);
	Actor->PrimaryActorTick.bCanEverTick = Class->bCanEverTick;
	Actor->PrimaryActorTick.bStartWithTickEnabled = Class->bStartWithTickEnabled;

	// Construct the C++ part of the angelscript scriptobject
	const bool bIsScriptAllocation = CurrentObjectInitializers.Num() != 0 && CurrentObjectInitializers.Last().GetObj() == Object;

	if (!bIsScriptAllocation && ScriptType != nullptr)
		new(Object) asCScriptObject(ScriptType);

#if WITH_AS_DEBUGVALUES
	// Init the object's debug value
	Object->Debug = Class->DebugValues.Instantiate(Object);
#endif

	//WILL-EDIT

	/*
	Right now I have two options it seems:
	1) Ensure all AS classes inherit from our own custom base so that events can be called
	2) Make one Custom component that we add to an actor that will call the actor and component
	events in place of the base.
	However neither of these are particularly good for firing off a BeginPlay with an actor that spawns
	later
	*/

	// Construct any default components we have marked in our hierarchy
	CreateDefaultComponents(Initializer, Actor, Class);

	// Call the script constructor function
	if (!bIsScriptAllocation)
		ExecuteConstructFunction(Object, Class);

	// Apply any default statements to the object
	if (bApplyDefaults && !bIsScriptAllocation)
		ExecuteDefaultsFunctions(Object, Class);

	if (!bIsScriptAllocation)
		const_cast<FObjectInitializer&>(Initializer).AddPropertyPostInitCallback([Actor, Class]() { FillOverrideComponentVariables(Actor, Class); });
}

void UASClass::StaticComponentConstructor(const FObjectInitializer& Initializer)
{
	UObject* Object = Initializer.GetObj();
	UASClass* Class = GetFirstASClass(Object);
	asCObjectType* ScriptType = (asCObjectType*)Class->ScriptTypePtr;
	UActorComponent* Component = (UActorComponent*)Object;

#if AS_CAN_HOTRELOAD
	const bool bApplyDefaults = !GConstructASObjectWithoutDefaults;
	GConstructASObjectWithoutDefaults = false;
#else
	const bool bApplyDefaults = true;
#endif

	// We need to run the C++ constructor first so everything is valid
	Class->CodeSuperClass->ClassConstructor(Initializer);
	Component->PrimaryComponentTick.bCanEverTick = Class->bCanEverTick;
	Component->PrimaryComponentTick.bStartWithTickEnabled = Class->bStartWithTickEnabled;

	// Construct the C++ part of the angelscript scriptobject
	const bool bIsScriptAllocation = CurrentObjectInitializers.Num() != 0 && CurrentObjectInitializers.Last().GetObj() == Object;

	if (!bIsScriptAllocation && ScriptType != nullptr)
		new(Object) asCScriptObject(ScriptType);

#if WITH_AS_DEBUGVALUES
	// Init the object's debug value
	Object->Debug = Class->DebugValues.Instantiate(Object);
#endif

	// Call the script constructor function
	if (!bIsScriptAllocation)
		ExecuteConstructFunction(Object, Class);

	// Apply any default statements to the object
	if (bApplyDefaults && !bIsScriptAllocation)
		ExecuteDefaultsFunctions(Object, Class);
}

void UASClass::StaticObjectConstructor(const FObjectInitializer& Initializer)
{
	UObject* Object = Initializer.GetObj();
	UASClass* Class = GetFirstASClass(Object);
	asCObjectType* ScriptType = (asCObjectType*)Class->ScriptTypePtr;

#if AS_CAN_HOTRELOAD
	const bool bApplyDefaults = !GConstructASObjectWithoutDefaults;
	GConstructASObjectWithoutDefaults = false;
#else
	const bool bApplyDefaults = true;
#endif

	// We need to run the C++ constructor first so everything is valid
	Class->CodeSuperClass->ClassConstructor(Initializer);

#if WITH_AS_DEBUGVALUES
	// Init the object's debug value
	Object->Debug = Class->DebugValues.Instantiate(Object);
#endif

	// Construct the C++ part of the angelscript scriptobject
	const bool bIsScriptAllocation = CurrentObjectInitializers.Num() != 0 && CurrentObjectInitializers.Last().GetObj() == Object;

	if (!bIsScriptAllocation && ScriptType != nullptr)
		new(Object) asCScriptObject(ScriptType);

	// Call the script constructor function
	if (!bIsScriptAllocation)
		ExecuteConstructFunction(Object, Class);

	// Apply any default statements to the object
	if (bApplyDefaults && !bIsScriptAllocation)
		ExecuteDefaultsFunctions(Object, Class);
}

FString UASClass::GetSourceFilePath() const
{
	if (ScriptTypePtr == nullptr)
		return TEXT("");
	auto& Manager = FAngelscriptEngine::Get();
	auto Module = Manager.GetModule(((asITypeInfo*)ScriptTypePtr)->GetModule());
	if (!Module.IsValid())
		return TEXT("");
	if (Module->Code.Num() == 0)
		return TEXT("");
	return Module->Code[0].AbsoluteFilename;
}

FString UASClass::GetRelativeSourceFilePath() const
{
	if (ScriptTypePtr == nullptr)
		return TEXT("");
	auto& Manager = FAngelscriptEngine::Get();
	auto Module = Manager.GetModule(((asITypeInfo*)ScriptTypePtr)->GetModule());
	if (!Module.IsValid())
		return TEXT("");
	if (Module->Code.Num() == 0)
		return TEXT("");
	return Module->Code[0].RelativeFilename;
}

bool UASClass::IsDeveloperOnly() const
{
	if (ScriptTypePtr == nullptr)
		return false;
	auto& Manager = FAngelscriptEngine::Get();
	auto Module = Manager.GetModule(((asITypeInfo*)ScriptTypePtr)->GetModule());
	if (!Module.IsValid())
		return false;
	const FString& ModuleName = Module->ModuleName;
	return ModuleName.Equals(TEXT("Dev"))
		|| ModuleName.StartsWith(TEXT("Dev."))
		|| ModuleName.Equals(TEXT("Editor"))
		|| ModuleName.StartsWith(TEXT("Editor."))
		|| ModuleName.EndsWith(TEXT(".Editor"))
		|| ModuleName.Contains(TEXT(".Editor."));
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
