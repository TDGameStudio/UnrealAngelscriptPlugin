#include "AngelscriptBytecodes.h"
#if AS_CAN_GENERATE_JIT
#include "StaticJIT/StaticJITBinds.h"
#include "AngelscriptType.h"
#include "AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptfunction.h"
//#include "as_callfunc.h"
//#include "as_objecttype.h"
//#include "as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_callfunc.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

static FAngelscriptBytecode UnimplementedBytecode;

TMap<asEBCInstr, FAngelscriptBytecode*>& FAngelscriptBytecode::GetBytecodeMap()
{
	static TMap<asEBCInstr, FAngelscriptBytecode*> InstructionToBytecodeMap;
	return InstructionToBytecodeMap;
}

FAngelscriptBytecode& FAngelscriptBytecode::GetBytecode(asEBCInstr Instr)
{
	FAngelscriptBytecode** Bytecode = GetBytecodeMap().Find(Instr);
	if (Bytecode == nullptr)
		return UnimplementedBytecode;
	check(*Bytecode != nullptr);
	return **Bytecode;
}

struct FDefaultConstructCall
{
	asCObjectType* ObjectType;

	FDefaultConstructCall(asITypeInfo* InObjectType)
		: ObjectType((asCObjectType*)InObjectType)
	{
	}

	void CallDefaultConstruct(FStaticJITContext& Context, const FString& AddressExpression);
};

struct FCopyConstructCall
{
	asCObjectType* ObjectType;

	FCopyConstructCall(asITypeInfo* InObjectType)
		: ObjectType((asCObjectType*)InObjectType)
	{
	}

	void CallCopyConstruct(FStaticJITContext& Context, const FString& AddressExpression, const FString& SourceExpression);
};

struct FAssignCall
{
	asCObjectType* ObjectType;

	FAssignCall(asITypeInfo* InObjectType)
		: ObjectType((asCObjectType*)InObjectType)
	{
	}

	void CallAssign(FStaticJITContext& Context, const FString& AddressExpression, const FString& SourceExpression);
};


struct FScriptSystemCall
{
	asCScriptFunction* ScriptFunction;
	asSSystemFunctionInterface* SysFunc;

	bool bHaveNativeFunction = false;
	bool bUsingTemplateObjectForm = false;
	bool bAllTypesHaveNatives = false;
	bool bAllTypesHaveGenerics = false;
	bool bIsTrivialFunction = false;

	FScriptFunctionNativeForm* NativeForm = nullptr;
	
	enum class ELookupType
	{
		PtrArg,
		IdArg,
		IdArg_AfterPtr,
	};

	FResolvedNativeType ObjectType;
	FResolvedNativeType ReturnType;
	TArray<FResolvedNativeType> ArgumentTypes;
	ELookupType Type;

	FScriptSystemCall(asIScriptFunction* InFunction, ELookupType InType)
		: ScriptFunction((asCScriptFunction*)InFunction)
		, Type(InType)
	{
		SysFunc = ScriptFunction->sysFuncIntf;
	}

	int32 GetUsageFlags()
	{
		return BU_l_objectRegister | BU_l_objectRegister;
	}

	void MakeCall(FStaticJITContext& Context)
	{
		FNativeFunctionContext NativeContext;
		NativeContext.LastPushedTypeInfo = Context.LastPushedTypeInfo;
		NativeContext.LastPushedTypeId = Context.LastPushedTypeId;
		NativeContext.CallingFunction = ScriptFunction;

		// Comment the actual call we're doing
		Context.Line("// {0}", ScriptFunction->GetDeclaration(true, true, true, false));

		// See if we have a native form for this function
		FindNativeForm();

		// Analyze arguments and determine what to do
		AnalyzeArgumentTypes();

		// Fill native context types
		NativeContext.ArgumentTypes = ArgumentTypes;
		NativeContext.ReturnType = ReturnType;
		NativeContext.ObjectType = ObjectType;

		// Check if we satisfy all condition to make a fully native function call
		bool bCanMakeNativeCall = bHaveNativeFunction && bAllTypesHaveNatives && NativeForm->CanCallNative(NativeContext);
		bool bCanMakeCustomCall = NativeForm != nullptr && NativeForm->CanCallCustom(NativeContext);
		if (FCStringAnsi::Strcmp(ScriptFunction->GetName(), "opCast") == 0)
		{
			UE_LOG(
				Angelscript,
				Display,
				TEXT("StaticJIT dispatch decl=%s nativeForm=%s native=%s custom=%s allNative=%s allGeneric=%s callConv=%d"),
				ANSI_TO_TCHAR(ScriptFunction->GetDeclaration()),
				NativeForm != nullptr ? TEXT("true") : TEXT("false"),
				bCanMakeNativeCall ? TEXT("true") : TEXT("false"),
				bCanMakeCustomCall ? TEXT("true") : TEXT("false"),
				bAllTypesHaveNatives ? TEXT("true") : TEXT("false"),
				bAllTypesHaveGenerics ? TEXT("true") : TEXT("false"),
				(int32)SysFunc->callConv);
		}

		// Proceed to the actual call method to use
		if (bCanMakeCustomCall)
		{
			bIsTrivialFunction = NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::CustomCall);
			MakeCustomCall(Context, NativeContext);
		}
		else if (bCanMakeNativeCall)
		{
			bIsTrivialFunction = NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::NativeCall);
			MakeNativeCall(Context, NativeContext);
		}
		else if (bAllTypesHaveGenerics && SupportsCallingConventionForGeneric())
		{
			bIsTrivialFunction = bHaveNativeFunction && NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::PointerCall);
			MakePointerCall(Context);
		}
		else
		{
			bIsTrivialFunction = false;
			MakeDynamicCall(Context);
		}
	}

	bool SupportsCallingConventionForGeneric()
	{
		switch (SysFunc->callConv)
		{
		case ICC_CDECL:
		case ICC_CDECL_RETURNINMEM:
		case ICC_CDECL_OBJFIRST:
		case ICC_CDECL_OBJFIRST_RETURNINMEM:
		case ICC_CDECL_OBJLAST:
		case ICC_CDECL_OBJLAST_RETURNINMEM:
		case ICC_THISCALL:
		case ICC_THISCALL_RETURNINMEM:
		case ICC_VIRTUAL_THISCALL:
		case ICC_VIRTUAL_THISCALL_RETURNINMEM:
			return true;
		default:
			return false;
		}
	}

	bool HasObjectPointer()
	{
		switch (SysFunc->callConv)
		{
		case ICC_CDECL_OBJFIRST:
		case ICC_CDECL_OBJFIRST_RETURNINMEM:
		case ICC_CDECL_OBJLAST:
		case ICC_CDECL_OBJLAST_RETURNINMEM:
		case ICC_THISCALL:
		case ICC_THISCALL_RETURNINMEM:
		case ICC_VIRTUAL_THISCALL:
		case ICC_VIRTUAL_THISCALL_RETURNINMEM:
		case ICC_GENERIC_METHOD:
		case ICC_GENERIC_METHOD_RETURNINMEM:
			return true;
		default:
			return false;
		}
	}

	bool IsMethod()
	{
		switch (SysFunc->callConv)
		{
		case ICC_THISCALL:
		case ICC_THISCALL_RETURNINMEM:
		case ICC_VIRTUAL_THISCALL:
		case ICC_VIRTUAL_THISCALL_RETURNINMEM:
			return true;
		default:
			return false;
		}
	}

	bool HasObjectPointerFirst()
	{
		switch (SysFunc->callConv)
		{
		case ICC_CDECL_OBJFIRST:
		case ICC_CDECL_OBJFIRST_RETURNINMEM:
			return true;
		default:
			return false;
		}
	}

	bool HasObjectPointerLast()
	{
		switch (SysFunc->callConv)
		{
		case ICC_CDECL_OBJLAST:
		case ICC_CDECL_OBJLAST_RETURNINMEM:
			return true;
		default:
			return false;
		}
	}

	bool HasScriptFunctionPointerFirstParam()
	{
		return SysFunc->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction;
	}

	bool HasScriptObjectTypeFirstParam()
	{
		return SysFunc->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType;
	}

	bool ShouldInformSystemFunction()
	{
		if (bIsTrivialFunction)
			return false;
		if (SysFunc->passFirstParamMetaData != asEFirstParamMetaData::None)
			return false;
		if (bUsingTemplateObjectForm)
			return false;
		if (NativeForm != nullptr && NativeForm->CanSkipInformSystemFunction())
			return false;
		if (ScriptFunction->userData != 0)
			return true;
		if (ScriptFunction->GetObjectType() != nullptr)
		{
			if (ScriptFunction->GetObjectType()->GetSubTypeCount() != 0)
				return true;
		}
		return false;
	}

	void LookupScriptFunc(FStaticJITContext& Context)
	{
		Context.Line("  asCScriptFunction* ScriptFunc = {0};", Context.ReferenceFunction(ScriptFunction));
	}

	bool NeedNativeScriptFuncLookup(FNativeFunctionContext& FunctionContext)
	{
		if (NativeForm != nullptr && NativeForm->CanSkipScriptFunctionLookup(FunctionContext))
			return false;
		if (ShouldInformSystemFunction())
			return true;
		if (HasScriptFunctionPointerFirstParam())
			return true;
		//if (ScriptFunction->returnType.IsValid() && ScriptFunction->returnType.GetTokenType() != ttVoid)
		//{
		//	if ((ScriptFunction->returnType.IsObject() || ScriptFunction->returnType.IsFuncdef()) && !ScriptFunction->returnType.IsReference())
		//	{
		//		if (ScriptFunction->returnType.IsObjectHandle())
		//		{
		//			return true;
		//		}
		//	}
		//}
		return false;
	}

	void MakeNativeCall(FStaticJITContext& Context, FNativeFunctionContext& NativeContext)
	{
		AddNativeHeaders(Context);

		Context.Line("{");
		if (NeedNativeScriptFuncLookup(NativeContext))
			LookupScriptFunc(Context);

		// Determine type for return value
		bool bHaveReturnValue = ReturnType.HasNativeForm();
		FString ReturnNativeForm = ReturnType.GetNativeForm();

		int32 StackOffset = 0;
		int32 BaseOffset = 0;
		int32 ReturnValueOffset = 0;

		// Find the object pointer to call object functions on
		FString NativeObjectType;
		FString NativeObjectName;

		bool bCheckedForObject = false;
		if (HasObjectPointer())
		{
			NativeObjectType = ObjectType.GetNativeForm();
			NativeObjectName = NativeObjectType;

			if (!NativeObjectType.EndsWith(TEXT("*")))
				NativeObjectType += TEXT("*");
			else
				NativeObjectName.RemoveFromEnd(TEXT("*"));

			Context.Line("  {0} Object = ({0}){1};", NativeObjectType, Context.StackValue_At(StackOffset, 2));

			if (!Context.IsStackValueNonNull(StackOffset) && !NativeForm->CanSkipObjectNullCheck(EScriptFunctionCallMethod::NativeCall))
			{
				Context.DebugLineNumber();
				Context.Line("  if(Object != nullptr)");
				Context.Line("  {");
				bCheckedForObject = true;
			}
			else if (!bIsTrivialFunction)
			{
				Context.DebugLineNumber();
			}

			StackOffset += 2;
			BaseOffset += 2;

			NativeContext.ObjectAddress = TEXT("Object");
		}
		else if (!bIsTrivialFunction)
		{
			Context.DebugLineNumber();
		}

		if (bHaveReturnValue)
		{
			if (ScriptFunction->DoesReturnOnStack())
			{
				NativeContext.ReturnValueAddress = FString::Printf(TEXT("((void*)%s)"), *Context.StackValue_At(StackOffset, 2));

				ReturnValueOffset = StackOffset;
				StackOffset += 2;
				BaseOffset += 2;
			}
		}

		// Handle arguments
		{
			if (HasObjectPointerFirst() && !NativeForm->ShouldIgnoreObjectArgument())
				NativeContext.ArgumentValues.Add(FString::Printf(TEXT("TAutoDereference<%s>(Object)"), *NativeObjectName));

			if (HasScriptFunctionPointerFirstParam())
				NativeContext.ArgumentValues.Add(TEXT("ScriptFunc"));
			else if (HasScriptObjectTypeFirstParam())
				NativeContext.ArgumentValues.Add(Context.ReferenceObjectType(ScriptFunction->objectType));

			for (int32 i = 0, Count = ArgumentTypes.Num(); i < Count; ++i)
			{
				bool bRefToValueOnStack = false;

				auto& ScriptParam = ScriptFunction->parameterTypes[i];
				if (ScriptParam.IsReference())
					bRefToValueOnStack = true;
				else if (ScriptParam.IsObject() && !ScriptParam.IsObjectHandle())
					bRefToValueOnStack = true;

				if (ScriptParam.IsEnumType() && !bRefToValueOnStack)
				{
					NativeContext.ArgumentValues.Add(
						FString::Printf(
							TEXT("(%s)(value_as<uint8>(%s))"),
							*ArgumentTypes[i].GetNativeForm(),
							*Context.StackValue_At(StackOffset, 1)
						)
					);
				}
				else if (ScriptParam.GetTokenType() == ttQuestion)
				{
					NativeContext.ArgumentValues.Add(
						FString::Printf(
							TEXT("(void*)%s"),
							*Context.StackValue_At(StackOffset, 2)
						)
					);

					NativeContext.ArgumentValues.Add(
						FString::Printf(
							TEXT("value_as<int>(%s)"),
							*Context.StackValue_At(StackOffset + 2, 1)
						)
					);
				}
				else if (bRefToValueOnStack)
				{
					if (bUsingTemplateObjectForm)
					{
						if (ArgumentTypes[i].GetNativeForm() == TEXT("void*"))
						{
							NativeContext.ArgumentValues.Add(
								FString::Printf(
									TEXT("(void*)%s"),
									*Context.StackValue_At(StackOffset, 2)
								)
							);
						}
						else
						{
							NativeContext.ArgumentValues.Add(
								FString::Printf(
									TEXT("TAutoDereference<%s>((void*)%s)"),
									*ArgumentTypes[i].GetNativeForm(),
									*Context.StackValue_At(StackOffset, 2)
								)
							);
						}
					}
					else
					{
						NativeContext.ArgumentValues.Add(
							FString::Printf(
								TEXT("(*(%s*)%s)"),
								*ArgumentTypes[i].GetNativeForm(),
								*Context.StackValue_At(StackOffset, 2)
							)
						);
					}
				}
				else
				{
					NativeContext.ArgumentValues.Add(
						FString::Printf(
							TEXT("value_as<%s>(%s)"),
							*ArgumentTypes[i].GetNativeForm(),
							*Context.StackValue_At(StackOffset, 
								ScriptParam.GetSizeOnStackDWords())
						)
					);
				}

				StackOffset += ScriptFunction->parameterTypes[i].GetSizeOnStackDWords();
			}

			if (HasObjectPointerLast() && !NativeForm->ShouldIgnoreObjectArgument())
				NativeContext.ArgumentValues.Add(FString::Printf(TEXT("TAutoDereference<%s>(Object)"), *NativeObjectName));
		}

		// Call the native form
		FNativeFunctionCall Call = NativeForm->GenerateCall(NativeContext);
		if (Call.Header.Len() != 0)
			Context.AddHeader(Call.Header);

		FString CallPrefix;
		FString CallSuffix;

		// Handle return value if needed
		bool bReturnIsNativeReference = true;
		if (!Call.bHandledReturnValue)
		{
			if (bHaveReturnValue)
			{
				if (ScriptFunction->DoesReturnOnStack())
				{
					CallPrefix = FString::Printf(TEXT("new(%s) %s("), *NativeContext.ReturnValueAddress, *ReturnType.GetNativeForm());
					CallSuffix = TEXT(")");
				}
				else
				{
					FString ReturnNative = ReturnType.GetNativeForm();
					if ((bUsingTemplateObjectForm || ScriptFunction->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None) && ReturnType.Usage.bIsReference)
					{
						if (ReturnNative == TEXT("void*"))
						{
							CallPrefix = TEXT("void* FunctionReturnValue = ");
							bReturnIsNativeReference = false;
						}
						else
						{
							CallPrefix = FString::Printf(TEXT("%s& FunctionReturnValue = TAutoDereference<%s>("), *ReturnNative, *ReturnNative);
							CallSuffix = TEXT(")");
						}
					}
					else
					{
						if (ReturnType.Usage.bIsReference)
						{
							ReturnNative = FString::Printf(TEXT("const %s&"), *ReturnNative);
						}
					
						if (ReturnType.Usage.IsObjectPointer())
						{
							// Cast away any potential const-ness for pointers here as it has already been enforced earlier.
							CallPrefix = FString::Printf(TEXT("%s FunctionReturnValue = (%s)"), *ReturnNative, *ReturnNative);
						}
						else
						{
							CallPrefix = FString::Printf(TEXT("%s FunctionReturnValue = "), *ReturnNative);
						}
					}
				}
			}
		}

		// Inform context of what we're calling
		if (ShouldInformSystemFunction())
			Context.Line("  FScopeInformSystemFunction Inform(Execution, ScriptFunc);");

		// Output the function call
		Context.Line("{0}{1}{2};", CallPrefix, Call.CallCode, CallSuffix);

		// Handle copying return value if needed
		CopyReturnValue(Context, Call.bHandledReturnValue ? Call.ReturnValue : TEXT("FunctionReturnValue"), bReturnIsNativeReference);

		Context.PopMultiple(StackOffset);

		// Check for thrown angelscript exceptions
		if (bCheckedForObject)
		{
			Context.Line("} else [[unlikely]] {");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("}");
		}

		if (!bIsTrivialFunction)
		{
			Context.Line("if (Execution.bExceptionThrown) [[unlikely]]");
			Context.Line("{");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/true, /*bSetNullptrException=*/false);
			Context.Line("}");
		}

		Context.Line("}");
	}

	void MakeCustomCall(FStaticJITContext& Context, FNativeFunctionContext& NativeContext)
	{
		AddNativeHeaders(Context);

		bool bHaveReturnValue = ScriptFunction->returnType.IsValid() && ScriptFunction->returnType.GetTokenType() != ttVoid;

		int32 StackOffset = 0;
		int32 BaseOffset = 0;
		int32 ReturnValueOffset = 0;

		Context.Line("{");

		if (NativeForm->ShouldCustomLookupScriptFunction(NativeContext))
			LookupScriptFunc(Context);

		bool bCheckedForObject = false;
		if (HasObjectPointer())
		{
			Context.Line("  void* Object = (void*){0};", Context.StackValue_At(StackOffset, 2));

			if (!Context.IsStackValueNonNull(StackOffset) && !NativeForm->CanSkipObjectNullCheck(EScriptFunctionCallMethod::CustomCall))
			{
				Context.DebugLineNumber();
				Context.Line("  if(Object != nullptr)");
				Context.Line("  {");
				bCheckedForObject = true;
			}
			else if (!bIsTrivialFunction)
			{
				Context.DebugLineNumber();
			}

			StackOffset += 2;
			BaseOffset += 2;

			NativeContext.ObjectAddress = TEXT("Object");
		}
		else if (!bIsTrivialFunction)
		{
			Context.DebugLineNumber();
		}

		if (bHaveReturnValue && ScriptFunction->DoesReturnOnStack())
		{
			NativeContext.ReturnValueAddress = FString::Printf(TEXT("((void*)%s)"), *Context.StackValue_At(StackOffset, 2));

			ReturnValueOffset = StackOffset;
			StackOffset += 2;
			BaseOffset += 2;
		}

		for (int32 i = 0, Count = ArgumentTypes.Num(); i < Count; ++i)
		{
			auto& ScriptParam = ScriptFunction->parameterTypes[i];
			if (ScriptParam.GetTokenType() == ttQuestion)
			{
				NativeContext.ArgumentValues.Add(
					Context.StackValue_At(StackOffset, 2)
				);
				NativeContext.ArgumentValues.Add(
					Context.StackValue_At(StackOffset + 2, 1)
				);
			}
			else
			{
				NativeContext.ArgumentValues.Add(
					Context.StackValue_At(StackOffset, ScriptParam.GetSizeOnStackDWords())
				);
			}

			StackOffset += ScriptParam.GetSizeOnStackDWords();
		}

		// Call the native form
		FNativeFunctionCall Call = NativeForm->GenerateCustomCall(NativeContext, Context);
		if (Call.Header.Len() != 0)
			Context.AddHeader(Call.Header);

		// Output the function call
		if (Call.CallCode.Len() != 0)
			Context.Line("{0};", Call.CallCode);

		if (!Call.bHandledReturnValue && Call.ReturnValue.Len() != 0)
			CopyReturnValue(Context, Call.ReturnValue, false);

		Context.PopMultiple(StackOffset);

		// Check for thrown angelscript exceptions
		if (bCheckedForObject)
		{
			Context.Line("} else [[unlikely]] {");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("}");
		}

		if (!bIsTrivialFunction)
		{
			Context.Line("if (Execution.bExceptionThrown) [[unlikely]]");
			Context.Line("{");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/true, /*bSetNullptrException=*/false);
			Context.Line("}");
		}

		Context.Line("}");
	}

	void MakePointerCall(FStaticJITContext& Context)
	{
		Context.Line("{");

		if (ShouldInformSystemFunction() || HasScriptFunctionPointerFirstParam())
			LookupScriptFunc(Context);

		if (IsMethod())
			Context.Line("  asMETHOD_t RawFuncPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(ScriptFunction));
		else
			Context.Line("  asFUNCTION_t RawFuncPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(ScriptFunction));

		// We still add native headers, since some of our generic forms
		// might actually be native forms!
		AddNativeHeaders(Context);

		// Determine type for return value
		bool bHaveReturnValue = ReturnType.HasGenericForm();
		FString ReturnGenericForm = ReturnType.GetGenericForm();

		// Cast the function pointer to something we can use
		FString PtrType;
		if (bHaveReturnValue)
			PtrType += ReturnGenericForm;
		else
			PtrType += TEXT("void");

		if (IsMethod())
			PtrType += TEXT("(asCUnknownClass::*)(");
		else
			PtrType += TEXT("(*)(");
		{
			int32 ArgCount = 0;
			if (HasObjectPointerFirst())
			{
				if (ArgCount != 0)
					PtrType += TEXT(",");
				PtrType += TEXT("void*");
				ArgCount += 1;
			}

			if (HasScriptFunctionPointerFirstParam())
			{
				if (ArgCount != 0)
					PtrType += TEXT(",");
				PtrType += TEXT("void*");
				ArgCount += 1;
			}
			else if (HasScriptObjectTypeFirstParam())
			{
				if (ArgCount != 0)
					PtrType += TEXT(",");
				PtrType += TEXT("void*");
				ArgCount += 1;
			}

			for (int32 i = 0, Count = ArgumentTypes.Num(); i < Count; ++i)
			{
				if (ArgCount != 0)
					PtrType += TEXT(",");
				ArgCount += 1;

				FString ArgForm = ArgumentTypes[i].GetFuncCastForm();
				ensure(ArgForm.Len() != 0);
				PtrType += ArgForm;

				if (ScriptFunction->parameterTypes[i].GetTokenType() == ttQuestion)
					PtrType += TEXT(",int");
			}

			if (HasObjectPointerLast())
			{
				if (ArgCount != 0)
					PtrType += TEXT(",");
				PtrType += TEXT("void*");
				ArgCount += 1;
			}
		}
		PtrType += TEXT(")");

		Context.Line("  auto CastedFuncPtr = ({0})RawFuncPtr;", PtrType);

		// Inform context of what we're calling
		if (ShouldInformSystemFunction())
			Context.Line("  FScopeInformSystemFunction Inform(Execution, ScriptFunc);");

		int32 StackOffset = 0;
		int32 BaseOffset = 0;

		// Find the object pointer to call object functions on
		bool bCheckedForObject = false;
		if (HasObjectPointer())
		{
			Context.Line("  void* Object = (void*){0};", Context.StackValue_At(StackOffset, 2));

			if (!Context.IsStackValueNonNull(StackOffset))
			{
				Context.DebugLineNumber();
				Context.Line("  if(Object != nullptr)");
				Context.Line("  {");
				bCheckedForObject = true;
			}
			else if (!bIsTrivialFunction)
			{
				Context.DebugLineNumber();
			}

			StackOffset += 2;
			BaseOffset += 2;
		}
		else if (!bIsTrivialFunction)
		{
			Context.DebugLineNumber();
		}

		// Call the actual function from its pointer
		FString Call;
		FString CallPrefix;
		FString CallSuffix;

		if (bHaveReturnValue)
		{
			if (ScriptFunction->DoesReturnOnStack())
			{
				CallPrefix = FString::Printf(TEXT("new((void*)%s) %s("),
					*Context.StackValue_At(StackOffset, 2), *ReturnType.GetGenericForm());
				CallSuffix = TEXT(")");

				StackOffset += 2;
				BaseOffset += 2;
			}
			else
			{
				CallPrefix = FString::Printf(TEXT("%s FunctionReturnValue = "), *ReturnType.GetGenericForm());
			}
		}

		if (IsMethod())
			Call += TEXT("(((asCUnknownClass*)Object)->*CastedFuncPtr)(");
		else
			Call += TEXT("CastedFuncPtr(");

		{
			int32 ArgCount = 0;

			if (HasObjectPointerFirst())
			{
				if (ArgCount != 0)
					Call += TEXT(",");
				Call += TEXT("Object");
				ArgCount += 1;
			}

			if (HasScriptFunctionPointerFirstParam())
			{
				if (ArgCount != 0)
					Call += TEXT(",");
				Call += TEXT("ScriptFunc");
				ArgCount += 1;
			}
			else if (HasScriptObjectTypeFirstParam())
			{
				if (ArgCount != 0)
					Call += TEXT(",");
				Call += Context.ReferenceObjectType(ScriptFunction->objectType);
				ArgCount += 1;
			}

			for (int32 i = 0, Count = ArgumentTypes.Num(); i < Count; ++i)
			{
				if (ArgCount != 0)
					Call += TEXT(",");
				ArgCount += 1;

				bool bRefToValueOnStack = false;
				bool bValueAsCast = false;
				if (!ArgumentTypes[i].Usage.bIsReference)
				{
					auto& ScriptParam = ScriptFunction->parameterTypes[i];
					check(!ScriptParam.IsReference());

					if (ScriptParam.IsObject() && !ScriptParam.IsObjectHandle())
						bRefToValueOnStack = true;
				}

				if (ArgumentTypes[i].Usage.bIsReference)
				{
					Call += TEXT("(");
					Call += ArgumentTypes[i].GetGenericForm();
					Call += TEXT("*)");
				}
				else if (bRefToValueOnStack)
				{
					Call += TEXT("*(");
					Call += ArgumentTypes[i].GetGenericForm();
					Call += TEXT("*)");
				}
				else
				{
					bValueAsCast = true;
					Call += TEXT("value_as<");
					Call += ArgumentTypes[i].GetGenericForm();
					Call += TEXT(">(");
				}

				if (ScriptFunction->parameterTypes[i].GetTokenType() == ttQuestion)
				{
					Call += Context.StackValue_At(StackOffset, 2);
					Call += TEXT(",value_as<int>(");
					Call += Context.StackValue_At(StackOffset + 2, 1);
					Call += TEXT(")");
				}
				else
				{
					Call += Context.StackValue_At(StackOffset, ScriptFunction->parameterTypes[i].GetSizeOnStackDWords());
				}

				if (bValueAsCast)
					Call += TEXT(")");

				StackOffset += ScriptFunction->parameterTypes[i].GetSizeOnStackDWords();
			}

			if (HasObjectPointerLast())
			{
				if (ArgCount != 0)
					Call += TEXT(",");
				Call += TEXT("Object");
				ArgCount += 1;
			}
		}
		Call += TEXT(")");

		Context.Line("{0}{1}{2};", CallPrefix, Call, CallSuffix);

		CopyReturnValue(Context, TEXT("FunctionReturnValue"), false);

		Context.PopMultiple(StackOffset);

		// Check for thrown angelscript exceptions
		if (bCheckedForObject)
		{
			Context.Line("} else [[unlikely]] {");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("}");
		}

		if (!bIsTrivialFunction)
		{
			Context.Line("if (Execution.bExceptionThrown) [[unlikely]]");
			Context.Line("{");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/true, /*bSetNullptrException=*/false);
			Context.Line("}");
		}

		Context.Line("}");
	}

	void CopyReturnValue(FStaticJITContext& Context, const FString& ReturnValue, bool bNativeReference)
	{
		if (ScriptFunction->returnType.IsValid() && ScriptFunction->returnType.GetTokenType() != ttVoid)
		{
			// Store the returned value into the script vm
			if ((ScriptFunction->returnType.IsObject() || ScriptFunction->returnType.IsFuncdef()) && !ScriptFunction->returnType.IsReference())
			{
				if (ScriptFunction->returnType.IsObjectHandle())
				{
					Context.Line("l_objectRegister = (void*){0};", ReturnValue);
				}
				else
				{
					// We already did the setting of the return value when we called the function above
				}
			}
			else
			{
				if (ScriptFunction->returnType.IsReference())
				{
					if (bNativeReference)
						Context.Line("l_valueRegister = (asQWORD)&{0};", ReturnValue);
					else
						Context.Line("l_valueRegister = (asQWORD){0};", ReturnValue);
					Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
				}
				else if (ScriptFunction->returnType.GetTokenType() == ttFloat32)
				{
					Context.Line("l_floatRegister = {0};", ReturnValue);
					Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::FloatRegister;
				}
				else if (ScriptFunction->returnType.GetTokenType() == ttFloat64)
				{
					Context.Line("l_doubleRegister = {0};", ReturnValue);
					Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DoubleRegister;
				}
				else
				{
					switch (ScriptFunction->returnType.GetSizeInMemoryBytes())
					{
					case 1:
						Context.Line("l_byteRegister = (asBYTE&){0};", ReturnValue);
						Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
						break;
					case 2:
						Context.Line("l_dwordRegister = (asDWORD)(asWORD&){0};", ReturnValue);
						Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DWordRegister;
						break;
					case 4:
						Context.Line("l_dwordRegister = (asDWORD&){0};", ReturnValue);
						Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DWordRegister;
						break;
					case 8:
						Context.Line("l_valueRegister = (asQWORD&){0};", ReturnValue);
						Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
						break;
					default:
						check(false);
						break;
					}
				}
			}
		}
	}

	void MakeDynamicCall(FStaticJITContext& Context)
	{
		// If all else fails, fall back to calling the script VM for the system call
		Context.MaterializeWholeStack();

		Context.Line("{");
		Context.DebugLineNumber();

		LookupScriptFunc(Context);

		int32 BaseOffset = 0;
		if (HasObjectPointer())
			BaseOffset += 2;
		if (ScriptFunction->DoesReturnOnStack())
			BaseOffset += 2;

		Context.Line("SCRIPT_CALL_NATIVE(ScriptFunc, {0});", Context.LocalStackOffset * 4);

		Context.Line("if (Execution.bExceptionThrown)");
		Context.Line("{");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/true, /*bSetNullptrException=*/false);
		Context.Line("}");

		Context.Line("}");

		Context.PopMultiple(ScriptFunction->totalSpaceBeforeFunction);
	}

	void AddNativeHeaders(FStaticJITContext& Context)
	{
		if (ObjectType.CppForm.CppHeader.Len() != 0)
			Context.AddHeader(ObjectType.CppForm.CppHeader);
		if (ReturnType.CppForm.CppHeader.Len() != 0)
			Context.AddHeader(ReturnType.CppForm.CppHeader);
		for (auto& ArgType : ArgumentTypes)
		{
			if (ArgType.CppForm.CppHeader.Len() != 0)
				Context.AddHeader(ArgType.CppForm.CppHeader);
		}
	}

	void FindNativeForm()
	{
		bHaveNativeFunction = true;

		if (HasObjectPointer())
		{
			ObjectType.Usage = FAngelscriptTypeUsage::FromTypeId(ScriptFunction->GetObjectType()->GetTypeId());
			if (ObjectType.Usage.IsValid())
			{
				ObjectType.Usage.GetCppForm(ObjectType.CppForm);
				if (ObjectType.CppForm.TemplateObjectForm.Len() != 0)
				{
					ObjectType.CppForm.CppType = ObjectType.CppForm.TemplateObjectForm;
					bUsingTemplateObjectForm = true;
				}
				else if (!ObjectType.HasNativeForm())
				{
					bHaveNativeFunction = false;
				}
			}

			if (!ObjectType.HasGenericForm())
			{
				// The object pointer itself is *always* a pointer,
				// so we can safely force a void* generic type to it
				ObjectType.CppForm.CppGenericType = TEXT("void*");
			}
		}

		NativeForm = FScriptFunctionNativeForm::GetNativeForm(ScriptFunction);
		if (NativeForm == nullptr)
			bHaveNativeFunction = false;
	}

	void AnalyzeArgumentTypes()
	{
		if (ScriptFunction == nullptr)
			return;

		bAllTypesHaveGenerics = true;
		bAllTypesHaveNatives = true;

		// Resolve return type
		ReturnType.Usage = FAngelscriptTypeUsage::FromReturn(ScriptFunction);
		if (ReturnType.Usage.IsValid())
		{
			ReturnType.Usage.GetCppForm(ReturnType.CppForm);
			if (bUsingTemplateObjectForm && ReturnType.CppForm.TemplateObjectForm.Len() != 0)
				ReturnType.CppForm.CppType = ReturnType.CppForm.TemplateObjectForm;
			else if (bUsingTemplateObjectForm && !ReturnType.HasNativeForm() && ReturnType.Usage.bIsReference)
				ReturnType.CppForm.CppType = "void*";
			if (!ReturnType.HasGenericForm())
				bAllTypesHaveGenerics = false;
			if (!ReturnType.HasNativeForm())
				bAllTypesHaveNatives = false;
		}
		else if(ScriptFunction->returnType.IsObjectHandle())
		{
			ReturnType.CppForm.CppGenericType = TEXT("void*");
			bAllTypesHaveNatives = false;
		}
		else if (ScriptFunction->returnType.IsValid() && ScriptFunction->returnType.GetTokenType() != ttVoid)
		{
			if (!ReturnType.Usage.bIsReference)
				bAllTypesHaveGenerics = false;
			bAllTypesHaveNatives = false;
		}

		// Resolve parameter types
		// TODO: Read these from the UFunction where appropriate instead of the script function
		int32 ArgCount = ScriptFunction->GetParamCount();
		for (int32 i = 0; i < ArgCount; ++i)
		{
			auto& ArgType = ArgumentTypes.Emplace_GetRef();

			ArgType.Usage = FAngelscriptTypeUsage::FromParam(ScriptFunction, i);
			if (ArgType.Usage.IsValid())
			{
				ArgType.Usage.GetCppForm(ArgType.CppForm);
				if (bUsingTemplateObjectForm && ArgType.CppForm.TemplateObjectForm.Len() != 0)
					ArgType.CppForm.CppType = ArgType.CppForm.TemplateObjectForm;
				else if (bUsingTemplateObjectForm && !ArgType.HasNativeForm() && ArgType.Usage.bIsReference)
					ArgType.CppForm.CppType = "void*";
				if (!ArgType.HasGenericForm())
					bAllTypesHaveGenerics = false;
				if (!ArgType.HasNativeForm())
					bAllTypesHaveNatives = false;
			}
			else if (ScriptFunction->parameterTypes[i].GetTokenType() == ttQuestion)
			{
				ArgType.CppForm.CppType = TEXT("void*");
				ArgType.CppForm.CppGenericType = TEXT("void*");
			}
			else if(ScriptFunction->parameterTypes[i].IsObjectHandle())
			{
				ArgType.CppForm.CppGenericType = TEXT("void*");
				bAllTypesHaveNatives = false;
			}
			else
			{
				if (!ArgType.Usage.bIsReference)
					bAllTypesHaveGenerics = false;
				bAllTypesHaveNatives = false;
			}
		}
	}
};

struct FCallScriptFunction
{
	enum class ELookupType
	{
		IdArg,
		IdArg_AfterPtr,
		BindIdArg,
		NoLookup,
	};

	asCScriptFunction* ScriptFunction;
	ELookupType LookupType;
	bool bIgnoreExceptions = false;
	bool bResolveVirtualFunction = false;
	bool bIsDestructor = false;
	FString OverrideObjectAddress;
	TArray<FString> OverrideArguments;

	FCallScriptFunction(asCScriptFunction* Function, ELookupType Type, bool InIgnoreExceptions = false, bool InResolveVirtualFunction = true)
		: ScriptFunction(Function)
		, LookupType(Type)
		, bIgnoreExceptions(InIgnoreExceptions)
		, bResolveVirtualFunction(InResolveVirtualFunction)
	{
	}

	void MakeCall(FStaticJITContext& Context)
	{
		Context.Line("// {0}", ScriptFunction->GetDeclaration(true, true, true, false));
		Context.DebugLineNumber();
		Context.Line("{");

		bIsDestructor = ScriptFunction->traits.GetTrait(asTRAIT_DESTRUCTOR);

		asCScriptFunction* RealFunction;
		if (bResolveVirtualFunction)
			RealFunction = Context.JIT->DevirtualizeFunction(ScriptFunction);
		else
			RealFunction = ScriptFunction;

		bool bDoDynamicCall = true;
		if (RealFunction != nullptr)
		{
			auto* GenerateData = Context.JIT->FunctionsToGenerate.Find(RealFunction);
			if (GenerateData != nullptr)
			{
				WriteDirectCall(Context, RealFunction, GenerateData);
				bDoDynamicCall = false;
			}
		}
		else if (Context.JIT->IsFunctionAlwaysJIT(ScriptFunction))
		{
			WriteDirectCall(Context, nullptr, nullptr);
			bDoDynamicCall = false;
		}

		if (bDoDynamicCall)
			WriteDynamicCall(Context);

		Context.Line("}");

		if (OverrideObjectAddress.Len() == 0)
			Context.PopMultiple(ScriptFunction->totalSpaceBeforeFunction);
	}

	void WriteDirectCall(FStaticJITContext& Context, asCScriptFunction* RealFunction, FAngelscriptStaticJIT::FGenerateFunction* GenerateData)
	{
		FString ArgumentString;
		FString ArgumentTypeString;
		FString ReturnTypeString = TEXT("void");
		FString HeadCode;
		FString FootCode;

		bool bCheckedObject = false;
		if (ScriptFunction->objectType != nullptr)
		{
			FAngelscriptTypeUsage ObjectType = FAngelscriptTypeUsage::FromTypeId(ScriptFunction->objectType->GetTypeId());
			FString LiteralType;

			if (ObjectType.IsValid())
			{
				FAngelscriptType::FCppForm CppForm;
				ObjectType.GetCppForm(CppForm);

				if (CppForm.CppHeader.Len() != 0)
					Context.AddHeader(CppForm.CppHeader);

				if (CppForm.CppType.Len() != 0)
					LiteralType = CppForm.CppType;
				else if (CppForm.CppGenericType.Len() != 0)
					LiteralType = CppForm.CppGenericType;
			}

			if (LiteralType.Len() == 0)
				LiteralType = TEXT("void*");
			else if (!LiteralType.EndsWith(TEXT("*")))
				LiteralType += TEXT("*");

			FString ObjectAddress;
			if (OverrideObjectAddress.Len() != 0)
			{
				ArgumentString += FString::Printf(TEXT(",\n\t\t(%s)(%s)"), *LiteralType, *OverrideObjectAddress);
				ArgumentTypeString += FString::Printf(TEXT(",%s"), *LiteralType);
			}
			else
			{
				Context.Line("{0} CallObject = ({0}){1};", LiteralType, Context.StackValue_At(0, 2));
				ArgumentString += FString::Printf(TEXT(",\n\t\t(%s)CallObject"), *LiteralType);
				ArgumentTypeString += FString::Printf(TEXT(",%s"), *LiteralType);

				if (OverrideObjectAddress.Len() == 0 && !Context.IsStackValueNonNull(0))
				{
					Context.Line("if (CallObject != nullptr)");
					Context.Line("{");
					bCheckedObject = true;
				}
			}
		}

		for (int i = 0, Count = ScriptFunction->parameterTypes.GetLength(); i < Count; ++i)
		{
			int32 Offset = ScriptFunction->parameterOffsets[i];
			auto DataType = ScriptFunction->parameterTypes[i];
			auto ParamType = FAngelscriptTypeUsage::FromParam(ScriptFunction, i);

			FString LiteralType;
			if (ParamType.IsValid())
			{
				FAngelscriptType::FCppForm CppForm;
				ParamType.GetCppForm(CppForm);

				if (CppForm.CppHeader.Len() != 0)
					Context.AddHeader(CppForm.CppHeader);

				if (CppForm.CppType.Len() != 0)
					LiteralType = CppForm.CppType;
				else if (CppForm.CppGenericType.Len() != 0)
					LiteralType = CppForm.CppGenericType;
			}

			FString ArgValue;
			if (OverrideArguments.Num() > i)
				ArgValue = OverrideArguments[i];
			else
				ArgValue = Context.StackValue_At(Offset, DataType.GetSizeOnStackDWords());

			if (DataType.IsReference() && DataType.IsReferenceType())
			{
				if (LiteralType.Len() == 0)
					LiteralType = TEXT("UObject*");

				ArgumentString += FString::Printf(TEXT(",\n\t\t(%s*)%s"), *LiteralType, *ArgValue);
				ArgumentTypeString += FString::Printf(TEXT(",%s*"), *LiteralType);
			}
			else if (DataType.IsReferenceType())
			{
				if (LiteralType.Len() == 0)
					LiteralType = TEXT("UObject*");

				ArgumentString += FString::Printf(TEXT(",\n\t\t(%s)%s"), *LiteralType, *ArgValue);
				ArgumentTypeString += FString::Printf(TEXT(",%s"), *LiteralType);
			}
			else if (DataType.IsObject() || DataType.IsReference())
			{
				if (LiteralType.Len() == 0)
					LiteralType = TEXT("FUnknownValueType");

				if (DataType.GetSizeInMemoryBytes() < 4 && !DataType.IsObject())
					LiteralType = TEXT("asDWORD");

				ArgumentString += FString::Printf(TEXT(",\n\t\t(%s*)%s"), *LiteralType, *ArgValue);
				ArgumentTypeString += FString::Printf(TEXT(",%s*"), *LiteralType);
			}
			else
			{
				FString VMType;
				switch (DataType.GetSizeInMemoryDWords())
				{
				case 1: VMType = DataType.GetTokenType() == ttFloat32 ? TEXT("float") : TEXT("asDWORD"); break;
				case 2: VMType = DataType.GetTokenType() == ttFloat64 ? TEXT("double") : TEXT("asQWORD"); break;
				default: check(false); break;
				}

				ArgumentString += FString::Printf(TEXT(",\n\t\tvalue_as<%s>(%s)"), *VMType, *ArgValue);
				ArgumentTypeString += FString::Printf(TEXT(",%s"), *VMType);
			}
		}

		if (ScriptFunction->returnType.GetTokenType() != ttVoid)
		{
			FAngelscriptTypeUsage ReturnType = FAngelscriptTypeUsage::FromReturn(ScriptFunction);
			FString LiteralType;

			int32 VMOffset = (ScriptFunction->objectType != nullptr) ? 2 : 0;
			bool bStoreReturnValueInVM = OverrideObjectAddress.IsEmpty();

			if (ReturnType.IsValid())
			{
				FAngelscriptType::FCppForm CppForm;
				ReturnType.GetCppForm(CppForm);

				if (CppForm.CppHeader.Len() != 0)
					Context.AddHeader(CppForm.CppHeader);

				if (CppForm.CppType.Len() != 0)
					LiteralType = CppForm.CppType;
				else if (CppForm.CppGenericType.Len() != 0)
					LiteralType = CppForm.CppGenericType;
			}

			if (ScriptFunction->returnType.IsReference() && ScriptFunction->returnType.IsReferenceType())
			{
				if (LiteralType.Len() == 0)
					LiteralType = TEXT("UObject*");

				ReturnTypeString = FString::Printf(TEXT("%s*"), *LiteralType);
				if (bStoreReturnValueInVM)
				{
					HeadCode = TEXT("l_valueRegister = (asQWORD)");
					Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
				}
			}
			else if (ScriptFunction->returnType.IsReferenceType())
			{
				if (LiteralType.Len() == 0)
					LiteralType = TEXT("UObject*");

				ReturnTypeString = FString::Printf(TEXT("%s"), *LiteralType);
				if (bStoreReturnValueInVM)
					HeadCode = TEXT("l_objectRegister = (void*)");
			}
			else if (ScriptFunction->returnType.IsReference())
			{
				if (LiteralType.Len() == 0)
					LiteralType = TEXT("FUnknownValueType");

				ReturnTypeString = FString::Printf(TEXT("%s*"), *LiteralType);
				if (bStoreReturnValueInVM)
				{
					HeadCode = TEXT("l_valueRegister = (asQWORD)");
					Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
				}
			}
			else if (ScriptFunction->returnType.IsObject())
			{
				check(ScriptFunction->DoesReturnOnStack());
				check(bStoreReturnValueInVM);

				if (LiteralType.Len() == 0)
					LiteralType = TEXT("FUnknownValueType");

				ArgumentString = FString::Printf(TEXT(",\n\t\t(%s*)(%s)"), *LiteralType, *Context.StackValue_At(VMOffset, 2)) + ArgumentString;
				ArgumentTypeString = FString::Printf(TEXT(",%s*"), *LiteralType) + ArgumentTypeString;
			}
			else
			{
				if (LiteralType.Len() == 0)
				{
					switch (ScriptFunction->returnType.GetSizeInMemoryBytes())
					{
					case 1: LiteralType = TEXT("asBYTE"); break;
					case 2: LiteralType = TEXT("asWORD"); break;
					case 4: LiteralType = ScriptFunction->returnType.GetTokenType() == ttFloat32 ? TEXT("float") : TEXT("asDWORD"); break;
					case 8: LiteralType = ScriptFunction->returnType.GetTokenType() == ttFloat64 ? TEXT("double") : TEXT("asQWORD"); break;
					default: check(false); break;
					}
				}

				ReturnTypeString = LiteralType;
				if (bStoreReturnValueInVM)
				{
					switch (ScriptFunction->returnType.GetSizeInMemoryBytes())
					{
					case 1:
						HeadCode = TEXT("l_byteRegister = (asBYTE)");
						Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
					break;
					case 2:
						HeadCode = TEXT("l_dwordRegister = (asDWORD)");
						Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DWordRegister;
					break;
					case 4:
						if (ScriptFunction->returnType.GetTokenType() == ttFloat32)
						{
							HeadCode = TEXT("l_floatRegister = ");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::FloatRegister;
						}
						else
						{
							HeadCode = TEXT("l_dwordRegister = (asDWORD)");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DWordRegister;
						}
					break;
					case 8:
						if (ScriptFunction->returnType.GetTokenType() == ttFloat64)
						{
							HeadCode = TEXT("l_doubleRegister = ");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DoubleRegister;
						}
						else
						{
							HeadCode = TEXT("l_valueRegister = (asQWORD)");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
						}
					break;
					default: check(false); break;
					}
				}
			}
		}

		if (RealFunction != nullptr && GenerateData != nullptr)
		{
			Context.File->ExternFunctions.Add(RealFunction);
			Context.Line("{2} {0}(Execution{1});{3}", GenerateData->FunctionSymbolName, ArgumentString, HeadCode, FootCode);
		}
		else
		{
			Context.Line("asCObjectType* CallObjectType = (asCObjectType*)(((UObject*)CallObject)->GetClass()->ScriptTypePtr);");
			Context.Line("asCScriptFunction* CallRealFunction = CallObjectType->virtualFunctionTable[{0}];", ScriptFunction->vfTableIdx);
			Context.Line("auto CastedCallFunction = ({0}(*)(FScriptExecution&{1}))CallRealFunction->jitFunction_Raw;", ReturnTypeString, ArgumentTypeString);
			Context.Line("{1} CastedCallFunction(Execution{0});{2}", ArgumentString, HeadCode, FootCode);
		}

		// Check for thrown angelscript exceptions
		if (bIgnoreExceptions)
		{
			if (bCheckedObject)
			{
				Context.Line("}");
			}
		}
		else
		{
			if (bCheckedObject)
			{
				Context.Line("} else [[unlikely]] {");
				Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/bIsDestructor, /*bSetNullptrException=*/true);
				Context.Line("}");
			}

			Context.Line("if (Execution.bExceptionThrown) [[unlikely]]");
			Context.Line("{");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/bIsDestructor, /*bSetNullptrException=*/false);
			Context.Line("}");
		}
	}

	void WriteDynamicCall(FStaticJITContext& Context)
	{
		if (LookupType == ELookupType::BindIdArg)
		{
			Context.Line("  asCScriptFunction* SignatureFunc = {0};", Context.ReferenceFunction(ScriptFunction));
			Context.Line("  int FunctionId = SCRIPT_ENGINE->importedFunctions[SignatureFunc->GetId() & ~FUNC_IMPORTED]->boundFunctionId;");
			Context.Line("  if (FunctionId == -1) [[unlikely]]");
			Context.Line("  {");
			Context.Line("     SCRIPT_UNBOUND_EXCEPTION();");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/bIsDestructor, /*bSetNullptrException=*/false);
			Context.Line("  }");
			Context.Line("  asCScriptFunction* CallFunction = SCRIPT_ENGINE->scriptFunctions[FunctionId];");
		}
		else
		{
			Context.Line("  asCScriptFunction* CallFunction = {0};", Context.ReferenceFunction(ScriptFunction));
		}

		Context.Line("  FAngelscriptContext CallContext(CallFunction->GetEngine());");
		Context.Line("  CallContext->Prepare(CallFunction);");

		int32 StackOffset = 0;
		if (ScriptFunction->objectType != nullptr)
		{
			if (OverrideObjectAddress.Len() != 0)
				Context.Line("  CallContext->SetObject((void*){0});", OverrideObjectAddress);
			else
				Context.Line("  CallContext->SetObject((void*){0});", Context.StackValue_At(StackOffset, 2));
			StackOffset += 2;
		}

		if (ScriptFunction->DoesReturnOnStack())
		{
			check(OverrideObjectAddress.Len() == 0);
			Context.Line("  CallContext->SetReturnIntoValue((void*){0});", Context.StackValue_At(StackOffset, 2));
			StackOffset += 2;
		}

		for (int32 i = 0, Count = ScriptFunction->parameterTypes.GetLength(); i < Count; ++i)
		{
			auto& paramType = ScriptFunction->parameterTypes[i];

			FString ArgValue;
			if (OverrideArguments.Num() > i)
				ArgValue = OverrideArguments[i];
			else
				ArgValue = Context.StackValue_At(StackOffset, paramType.GetSizeOnStackDWords());

			if (paramType.IsObject() || paramType.IsReference())
			{
				Context.Line("  CallContext->SetArgAddress({0}, (void*){1});", i, ArgValue);
				StackOffset += 2;
			}
			else
			{
				switch (paramType.GetSizeInMemoryBytes())
				{
				case 1:
					Context.Line("  CallContext->SetArgByte({0}, value_as<asBYTE>({1}));", i, ArgValue);
					StackOffset += 1;
				break;
				case 2:
					Context.Line("  CallContext->SetArgWord({0}, value_as<asWORD>({1}));", i, ArgValue);
					StackOffset += 1;
				break;
				case 4:
					Context.Line("  CallContext->SetArgDWord({0}, value_as<asDWORD>({1}));", i, ArgValue);
					StackOffset += 1;
				break;
				case 8:
					Context.Line("  CallContext->SetArgQWord({0}, value_as<asQWORD>({1}));", i, ArgValue);
					StackOffset += 2;
				break;
				default: check(false); break;
				}
			}
		}

		Context.Line("  CallContext->Execute();");

		if (!bIgnoreExceptions)
		{
			Context.Line("  if (CallContext->m_status != asEXECUTION_FINISHED)");
			Context.Line("  {");
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/bIsDestructor, /*bSetNullptrException=*/false);
			Context.Line("  }");
		}

		if (!ScriptFunction->DoesReturnOnStack() && OverrideObjectAddress.Len() == 0)
		{
			if (ScriptFunction->returnType.IsReference())
			{
				Context.Line("  l_valueRegister = CallContext->m_regs.valueRegister;");
				Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
			}
			else if (ScriptFunction->returnType.IsObjectHandle())
			{
				Context.Line("  l_objectRegister = (void*)CallContext->m_regs.objectRegister;");
			}
			else if(ScriptFunction->returnType.GetTokenType() != ttVoid)
			{
				Context.Line("  l_valueRegister = CallContext->m_regs.valueRegister;");
				Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
			}
		}
	}
};

void FDestructorCall::CallDestroy(FStaticJITContext& Context, const FString& AddressExpression)
{
	// Ignore object types that don't have any destructor
	if (ObjectType->beh.destruct == 0)
		return;

	if ((ObjectType->flags & asOBJ_LIST_PATTERN) != 0)
	{
		Context.Line("    SCRIPT_ENGINE->DestroyList((asBYTE*){0}, {1});",
			AddressExpression, Context.ReferenceTypeInfo(ObjectType));
		return;
	}

	check((ObjectType->GetFlags() & asOBJ_VALUE) != 0);

	// Three possibilities:
	// 1. We have native access to the type, so we can place the destructor call inline.
	// 2. The destructor is a system function. We know the signature so it's easy to call.
	// 3. We are destroying a script struct, we have to go through the engine for this at the moment.

	// Native access:
	asCScriptFunction* DestroyFunc = Context.GetEngine()->scriptFunctions[ObjectType->beh.destruct];
	FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());
	auto DestructNativeForm = FScriptFunctionNativeForm::GetNativeForm(DestroyFunc);

	if (Usage.IsValid())
	{
		FResolvedNativeType DestructNativeType;
		DestructNativeType.Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());

		if (DestructNativeType.Usage.IsValid())
		{
			DestructNativeType.Usage.GetCppForm(DestructNativeType.CppForm);
			if (DestructNativeType.CppForm.TemplateObjectForm.Len() != 0)
				DestructNativeType.CppForm.CppType = DestructNativeType.CppForm.TemplateObjectForm;
		}

		if (DestructNativeForm != nullptr && DestructNativeType.HasNativeForm() && (DestructNativeForm->IsTrivialFunction(EScriptFunctionCallMethod::NativeCall) || DestroyFunc->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None))
		{
			FNativeFunctionContext DestructContext;
			DestructContext.CallingFunction = DestroyFunc;
			DestructContext.ObjectAddress = FString::Printf(TEXT("((%s*)%s)"), *DestructNativeType.CppForm.CppType, *AddressExpression);
			DestructContext.ObjectType = DestructNativeType;

			if (DestructNativeForm->CanCallNative(DestructContext))
			{
				if (DestroyFunc->sysFuncIntf->callConv == ICC_CDECL_OBJFIRST && !DestructNativeForm->ShouldIgnoreObjectArgument())
				{
					DestructContext.ArgumentValues.Add(FString::Printf(TEXT("TAutoDereference<%s>(%s)"), *DestructNativeType.CppForm.CppType, *DestructContext.ObjectAddress));
				}

				if (DestroyFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
				{
					DestructContext.ArgumentValues.Add(Context.ReferenceFunction(DestroyFunc));
				}
				else if (DestroyFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType)
				{
					DestructContext.ArgumentValues.Add(Context.ReferenceObjectType(DestroyFunc->objectType));
				}

				FNativeFunctionCall Call = DestructNativeForm->GenerateCall(DestructContext);
				Context.Line("{0};", Call.CallCode);

				if (Call.Header.Len() != 0)
					Context.AddHeader(Call.Header);
				if (DestructNativeType.CppForm.CppHeader.Len() != 0)
					Context.AddHeader(DestructNativeType.CppForm.CppHeader);

				return;
			}
		}
	}

	// System function:
	if (DestroyFunc->sysFuncIntf != nullptr)
	{
		check((ObjectType->GetFlags() & asOBJ_SCRIPT_OBJECT) == 0);

		if (DestroyFunc->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None)
		{
			Context.Line("{");

			FString MetaDataParam;
			if (DestroyFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
			{
				MetaDataParam = Context.ReferenceFunction(DestroyFunc);
			}
			else if (DestroyFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType)
			{
				MetaDataParam = Context.ReferenceObjectType(DestroyFunc->objectType);
			}

			switch (DestroyFunc->sysFuncIntf->callConv)
			{
				case ICC_CDECL_OBJFIRST:
				case ICC_CDECL_OBJLAST:
					Context.Line("  asFUNCTION_t RawDestructorPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(DestroyFunc));
					Context.Line("  auto CastedDestructorPtr = (void(*)(void*,void*))RawDestructorPtr;");
					Context.Line("  CastedDestructorPtr((void*){0}, {1});", AddressExpression, MetaDataParam);
				break;
				case ICC_THISCALL:
					Context.Line("  asMETHOD_t RawDestructorPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(DestroyFunc));
					Context.Line("  auto CastedDestructorPtr = (void(asCUnknownClass::*)(void*))RawDestructorPtr;");
					Context.Line("  (((asCUnknownClass*){0})->*CastedDestructorPtr)({1});", AddressExpression, MetaDataParam);
				break;
				default:
					check(false);
					break;
			}

			Context.Line("}");
			return;
		}
		else
		{
			Context.Line("{");

			if (DestructNativeForm == nullptr || !DestructNativeForm->IsTrivialFunction(EScriptFunctionCallMethod::PointerCall))
			{
				if (DestroyFunc->userData != 0
					|| (DestroyFunc->objectType != nullptr && DestroyFunc->objectType->templateSubTypes.GetLength() != 0))
				{
					Context.Line("  asCScriptFunction* ScriptDestructor = {0};", Context.ReferenceFunction(DestroyFunc));
					Context.Line("  FScopeInformSystemFunction InformDestructor(Execution, ScriptDestructor);");
				}
			}

			switch (DestroyFunc->sysFuncIntf->callConv)
			{
				case ICC_CDECL_OBJFIRST:
				case ICC_CDECL_OBJLAST:
					Context.Line("  asFUNCTION_t RawDestructorPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(DestroyFunc));
					Context.Line("  auto CastedDestructorPtr = (void(*)(void*))RawDestructorPtr;");
					Context.Line("  CastedDestructorPtr((void*){0});", AddressExpression);
				break;
				case ICC_THISCALL:
					Context.Line("  asMETHOD_t RawDestructorPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(DestroyFunc));
					Context.Line("  auto CastedDestructorPtr = (void(asCUnknownClass::*)())RawDestructorPtr;");
					Context.Line("  (((asCUnknownClass*){0})->*CastedDestructorPtr)();", AddressExpression);
				break;
				default:
					check(false);
					break;
			}

			Context.Line("}");
			return;
		}
	}

	// Script struct type
	FCallScriptFunction ScriptCall(DestroyFunc, FCallScriptFunction::ELookupType::NoLookup, /*bIgnoreExceptions=*/true);
	ScriptCall.OverrideObjectAddress = AddressExpression;
	ScriptCall.MakeCall(Context);
}

void FDefaultConstructCall::CallDefaultConstruct(FStaticJITContext& Context, const FString& AddressExpression)
{
	check((ObjectType->GetFlags() & asOBJ_VALUE) != 0)
	check(ObjectType->beh.construct != 0);

	// Three possibilities:
	// 1. We have native access to the type, so we can place the construct call inline.
	// 2. The constructor is a system function. We know the signature so it's easy to call.
	// 3. We are constructing a script struct, we call the script function that is the constructor.

	// Native access:
	asCScriptFunction* BehFunc = Context.GetEngine()->scriptFunctions[ObjectType->beh.construct];
	FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());
	auto NativeForm = FScriptFunctionNativeForm::GetNativeForm(BehFunc);

	if (Usage.IsValid())
	{
		FResolvedNativeType NativeType;
		NativeType.Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());

		if (NativeType.Usage.IsValid())
		{
			NativeType.Usage.GetCppForm(NativeType.CppForm);
			if (NativeType.CppForm.TemplateObjectForm.Len() != 0)
				NativeType.CppForm.CppType = NativeType.CppForm.TemplateObjectForm;
		}

		if (NativeForm != nullptr && NativeType.HasNativeForm() && (NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::NativeCall) || BehFunc->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None))
		{
			FNativeFunctionContext CallContext;
			CallContext.CallingFunction = BehFunc;
			CallContext.ObjectAddress = FString::Printf(TEXT("((%s*)%s)"), *NativeType.CppForm.CppType, *AddressExpression);
			CallContext.ObjectType = NativeType;

			if (NativeForm->CanCallNative(CallContext))
			{
				if (BehFunc->sysFuncIntf->callConv == ICC_CDECL_OBJFIRST && !NativeForm->ShouldIgnoreObjectArgument())
				{
					CallContext.ArgumentValues.Add(FString::Printf(TEXT("TAutoDereference<%s>(%s)"), *NativeType.CppForm.CppType, *CallContext.ObjectAddress));
				}

				if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
				{
					CallContext.ArgumentValues.Add(Context.ReferenceFunction(BehFunc));
				}
				else if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType)
				{
					CallContext.ArgumentValues.Add(Context.ReferenceObjectType(BehFunc->objectType));
				}

				FNativeFunctionCall Call = NativeForm->GenerateCall(CallContext);
				Context.Line("{0};", Call.CallCode);

				if (Call.Header.Len() != 0)
					Context.AddHeader(Call.Header);
				if (NativeType.CppForm.CppHeader.Len() != 0)
					Context.AddHeader(NativeType.CppForm.CppHeader);

				return;
			}
		}
	}

	// System function:
	if (BehFunc->sysFuncIntf != nullptr)
	{
		check((ObjectType->GetFlags() & asOBJ_SCRIPT_OBJECT) == 0);
		if (BehFunc->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None)
		{
			Context.Line("{");

			FString MetaDataParam;
			if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
			{
				MetaDataParam = Context.ReferenceFunction(BehFunc);
			}
			else if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType)
			{
				MetaDataParam = Context.ReferenceObjectType(BehFunc->objectType);
			}

			switch (BehFunc->sysFuncIntf->callConv)
			{
				case ICC_CDECL_OBJFIRST:
				case ICC_CDECL_OBJLAST:
					Context.Line("  asFUNCTION_t RawBehPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(*)(void*,void*))RawBehPtr;");
					Context.Line("  CastedBehPtr((void*){0},{1});", AddressExpression, MetaDataParam);
				break;
				case ICC_THISCALL:
					Context.Line("  asMETHOD_t RawBehPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(asCUnknownClass::*)(void*))RawBehPtr;");
					Context.Line("  (((asCUnknownClass*){0})->*CastedBehPtr)({1});", AddressExpression, MetaDataParam);
				break;
				default:
					check(false);
					break;
			}

			Context.Line("}");
			return;
		}
		else
		{
			Context.Line("{");

			if (NativeForm == nullptr || !NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::PointerCall))
			{
				if (BehFunc->userData != 0
					|| (BehFunc->objectType != nullptr && BehFunc->objectType->templateSubTypes.GetLength() != 0))
				{
					Context.Line("  asCScriptFunction* ScriptBehavior = {0};", Context.ReferenceFunction(BehFunc));
					Context.Line("  FScopeInformSystemFunction InformBehavior(Execution, ScriptBehavior);");
				}
			}

			switch (BehFunc->sysFuncIntf->callConv)
			{
				case ICC_CDECL_OBJFIRST:
				case ICC_CDECL_OBJLAST:
					Context.Line("  asFUNCTION_t RawBehPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(*)(void*))RawBehPtr;");
					Context.Line("  CastedBehPtr((void*){0});", AddressExpression);
				break;
				case ICC_THISCALL:
					Context.Line("  asMETHOD_t RawBehPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(asCUnknownClass::*)())RawBehPtr;");
					Context.Line("  (((asCUnknownClass*){0})->*CastedBehPtr)();", AddressExpression);
				break;
				default:
					check(false);
					break;
			}

			Context.Line("}");
			return;
		}
	}

	// Script struct type
	FCallScriptFunction ScriptCall(BehFunc, FCallScriptFunction::ELookupType::NoLookup);
	ScriptCall.OverrideObjectAddress = AddressExpression;
	ScriptCall.MakeCall(Context);
}

void FAssignCall::CallAssign(FStaticJITContext& Context, const FString& AddressExpression, const FString& SourceExpression)
{
	check((ObjectType->GetFlags() & asOBJ_VALUE) != 0)
	check(ObjectType->beh.copy != 0);

	// Three possibilities:
	// 1. We have native access to the type, so we can place the call inline.
	// 2. The function is a system function. We know the signature so it's easy to call.
	// 3. We are using a script struct, we should behave as CopyScript does.

	// Native access:
	asCScriptFunction* BehFunc = Context.GetEngine()->scriptFunctions[ObjectType->beh.copy];
	FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());
	auto NativeForm = FScriptFunctionNativeForm::GetNativeForm(BehFunc);

	if (Usage.IsValid())
	{
		FResolvedNativeType NativeType;
		NativeType.Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());

		if (NativeType.Usage.IsValid())
		{
			NativeType.Usage.GetCppForm(NativeType.CppForm);
			if (NativeType.CppForm.TemplateObjectForm.Len() != 0)
				NativeType.CppForm.CppType = NativeType.CppForm.TemplateObjectForm;
		}

		if (NativeForm != nullptr && NativeType.HasNativeForm() && (NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::NativeCall) || BehFunc->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None))
		{
			FNativeFunctionContext CallContext;

			FResolvedNativeType RefType;
			RefType = NativeType;
			RefType.Usage.bIsReference = true;

			CallContext.CallingFunction = BehFunc;
			CallContext.ArgumentTypes.Add(RefType);
			CallContext.ArgumentValues.Add(FString::Printf(TEXT("*(%s*)%s"), *NativeType.CppForm.CppType, *SourceExpression));
			CallContext.ObjectAddress = FString::Printf(TEXT("((%s*)%s)"), *NativeType.CppForm.CppType, *AddressExpression);
			CallContext.ObjectType = NativeType;

			if (NativeForm->CanCallNative(CallContext))
			{
				if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
				{
					CallContext.ArgumentValues.Insert(Context.ReferenceFunction(BehFunc), 0);
				}
				else if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType)
				{
					CallContext.ArgumentValues.Insert(Context.ReferenceObjectType(BehFunc->objectType), 0);
				}

				if (BehFunc->sysFuncIntf->callConv == ICC_CDECL_OBJFIRST && !NativeForm->ShouldIgnoreObjectArgument())
				{
					CallContext.ArgumentValues.Insert(FString::Printf(TEXT("TAutoDereference<%s>(%s)"), *NativeType.CppForm.CppType, *CallContext.ObjectAddress), 0);
				}

				FNativeFunctionCall Call = NativeForm->GenerateCall(CallContext);
				Context.Line("{0};", Call.CallCode);

				if (Call.Header.Len() != 0)
					Context.AddHeader(Call.Header);
				if (NativeType.CppForm.CppHeader.Len() != 0)
					Context.AddHeader(NativeType.CppForm.CppHeader);

				return;
			}
		}
	}

	// System function:
	if (BehFunc->sysFuncIntf != nullptr)
	{
		if ((ObjectType->GetFlags() & asOBJ_SCRIPT_OBJECT) == 0)
		{
			if (BehFunc->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None)
			{
				Context.Line("{");

				FString MetaDataParam;
				if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
				{
					MetaDataParam = Context.ReferenceFunction(BehFunc);
				}
				else if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType)
				{
					MetaDataParam = Context.ReferenceObjectType(BehFunc->objectType);
				}

				switch (BehFunc->sysFuncIntf->callConv)
				{
				case ICC_CDECL_OBJFIRST:
				case ICC_CDECL_OBJLAST:
					Context.Line("  asFUNCTION_t RawBehPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(*)(void*,void*,void*))RawBehPtr;");
					Context.Line("  CastedBehPtr((void*){0}, {2}, (void*){1});", AddressExpression, SourceExpression, MetaDataParam);
					break;
				case ICC_THISCALL:
					Context.Line("  asMETHOD_t RawBehPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(asCUnknownClass::*)(void*,void*))RawBehPtr;");
					Context.Line("  (((asCUnknownClass*){0})->*CastedBehPtr)((void*){1}, {2});", AddressExpression, SourceExpression, MetaDataParam);
					break;
				default:
					check(false);
					break;
				}

				Context.Line("}");
				return;
			}
			else
			{
				Context.Line("{");

				if (NativeForm == nullptr || !NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::PointerCall))
				{
					if (BehFunc->userData != 0
						|| (BehFunc->objectType != nullptr && BehFunc->objectType->templateSubTypes.GetLength() != 0))
					{
						Context.Line("  asCScriptFunction* ScriptBehavior = {0};", Context.ReferenceFunction(BehFunc));
						Context.Line("  FScopeInformSystemFunction InformBehavior(Execution, ScriptBehavior);");
					}
				}

				switch (BehFunc->sysFuncIntf->callConv)
				{
				case ICC_CDECL_OBJFIRST:
				case ICC_CDECL_OBJLAST:
					Context.Line("  asFUNCTION_t RawBehPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(*)(void*,void*))RawBehPtr;");
					Context.Line("  CastedBehPtr((void*){0}, (void*){1});", AddressExpression, SourceExpression);
					break;
				case ICC_THISCALL:
					Context.Line("  asMETHOD_t RawBehPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(asCUnknownClass::*)(void*))RawBehPtr;");
					Context.Line("  (((asCUnknownClass*){0})->*CastedBehPtr)((void*){1});", AddressExpression, SourceExpression);
					break;
				default:
					check(false);
					break;
				}

				Context.Line("}");
				return;
			}
		}
		else
		{
			// This is the default script struct copy operator
			Context.Line("{");
			Context.Line("  auto* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(ObjectType));
			Context.Line("  auto* d = (asCScriptObject*){0};", AddressExpression);
			Context.Line("  auto* s = (asCScriptObject*){0};", SourceExpression);
			Context.Line("  d->PerformCopy(s, objType, objType);");
			Context.Line("}");
			return;
		}
	}

	// Script assignment operator
	FCallScriptFunction ScriptCall(BehFunc, FCallScriptFunction::ELookupType::NoLookup);
	ScriptCall.OverrideObjectAddress = AddressExpression;
	ScriptCall.OverrideArguments.Add(SourceExpression);
	ScriptCall.MakeCall(Context);
}

void FCopyConstructCall::CallCopyConstruct(FStaticJITContext& Context, const FString& AddressExpression, const FString& SourceExpression)
{
	if (ObjectType->beh.copyconstruct == 0)
	{
		FDefaultConstructCall DefConstr(ObjectType);
		DefConstr.CallDefaultConstruct(Context, AddressExpression);

		FAssignCall Assg(ObjectType);
		Assg.CallAssign(Context, AddressExpression, SourceExpression);

		return;
	}

	check((ObjectType->GetFlags() & asOBJ_VALUE) != 0)
	check(ObjectType->beh.copyconstruct != 0);

	// Three possibilities:
	// 1. We have native access to the type, so we can place the call inline.
	// 2. The function is a system function. We know the signature so it's easy to call.
	// 3. We are using a script struct, we should behave as CopyScript does.

	// Native access:
	asCScriptFunction* BehFunc = Context.GetEngine()->scriptFunctions[ObjectType->beh.copyconstruct];
	FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());
	auto NativeForm = FScriptFunctionNativeForm::GetNativeForm(BehFunc);

	if (Usage.IsValid())
	{
		FResolvedNativeType NativeType;
		NativeType.Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());

		if (NativeType.Usage.IsValid())
		{
			NativeType.Usage.GetCppForm(NativeType.CppForm);
			if (NativeType.CppForm.TemplateObjectForm.Len() != 0)
				NativeType.CppForm.CppType = NativeType.CppForm.TemplateObjectForm;
		}

		if (NativeForm != nullptr && NativeType.HasNativeForm() && (NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::NativeCall) || BehFunc->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None))
		{
			FNativeFunctionContext CallContext;

			FResolvedNativeType RefType;
			RefType = NativeType;
			RefType.Usage.bIsReference = true;

			CallContext.CallingFunction = BehFunc;
			CallContext.ArgumentTypes.Add(RefType);
			CallContext.ArgumentValues.Add(FString::Printf(TEXT("*(%s*)%s"), *NativeType.CppForm.CppType, *SourceExpression));
			CallContext.ObjectAddress = FString::Printf(TEXT("((%s*)%s)"), *NativeType.CppForm.CppType, *AddressExpression);
			CallContext.ObjectType = NativeType;

			if (NativeForm->CanCallNative(CallContext))
			{
				if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
				{
					CallContext.ArgumentValues.Insert(Context.ReferenceFunction(BehFunc), 0);
				}
				else if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType)
				{
					CallContext.ArgumentValues.Insert(Context.ReferenceObjectType(BehFunc->objectType), 0);
				}

				if (BehFunc->sysFuncIntf->callConv == ICC_CDECL_OBJFIRST && !NativeForm->ShouldIgnoreObjectArgument())
				{
					CallContext.ArgumentValues.Insert(FString::Printf(TEXT("TAutoDereference<%s>(%s)"), *NativeType.CppForm.CppType, *CallContext.ObjectAddress), 0);
				}

				FNativeFunctionCall Call = NativeForm->GenerateCall(CallContext);
				Context.Line("{0};", Call.CallCode);

				if (Call.Header.Len() != 0)
					Context.AddHeader(Call.Header);
				if (NativeType.CppForm.CppHeader.Len() != 0)
					Context.AddHeader(NativeType.CppForm.CppHeader);

				return;
			}
		}
	}

	// System function:
	if (BehFunc->sysFuncIntf != nullptr)
	{
		if ((ObjectType->GetFlags() & asOBJ_SCRIPT_OBJECT) == 0)
		{
			if (BehFunc->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None)
			{
				Context.Line("{");

				FString MetaDataParam;
				if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptFunction)
				{
					MetaDataParam = Context.ReferenceFunction(BehFunc);
				}
				else if (BehFunc->sysFuncIntf->passFirstParamMetaData == asEFirstParamMetaData::ScriptObjectType)
				{
					MetaDataParam = Context.ReferenceObjectType(BehFunc->objectType);
				}

				switch (BehFunc->sysFuncIntf->callConv)
				{
				case ICC_CDECL_OBJFIRST:
				case ICC_CDECL_OBJLAST:
					Context.Line("  asFUNCTION_t RawBehPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(*)(void*,void*,void*))RawBehPtr;");
					Context.Line("  CastedBehPtr((void*){0}, {2}, (void*){1});", AddressExpression, SourceExpression, MetaDataParam);
					break;
				case ICC_THISCALL:
					Context.Line("  asMETHOD_t RawBehPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(asCUnknownClass::*)(void*,void*))RawBehPtr;");
					Context.Line("  (((asCUnknownClass*){0})->*CastedBehPtr)((void*){1}, {2});", AddressExpression, SourceExpression, MetaDataParam);
					break;
				default:
					check(false);
					break;
				}

				Context.Line("}");
				return;
			}
			else
			{
				Context.Line("{");

				if (NativeForm == nullptr || !NativeForm->IsTrivialFunction(EScriptFunctionCallMethod::PointerCall))
				{
					if (BehFunc->userData != 0
						|| (BehFunc->objectType != nullptr && BehFunc->objectType->templateSubTypes.GetLength() != 0))
					{
						Context.Line("  asCScriptFunction* ScriptBehavior = {0};", Context.ReferenceFunction(BehFunc));
						Context.Line("  FScopeInformSystemFunction InformBehavior(Execution, ScriptBehavior);");
					}
				}

				switch (BehFunc->sysFuncIntf->callConv)
				{
				case ICC_CDECL_OBJFIRST:
				case ICC_CDECL_OBJLAST:
					Context.Line("  asFUNCTION_t RawBehPtr = {0}.GetFunction();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(*)(void*,void*))RawBehPtr;");
					Context.Line("  CastedBehPtr((void*){0}, (void*){1});", AddressExpression, SourceExpression);
					break;
				case ICC_THISCALL:
					Context.Line("  asMETHOD_t RawBehPtr = {0}.GetMethod();", Context.ReferenceSystemFunctionPointer(BehFunc));
					Context.Line("  auto CastedBehPtr = (void(asCUnknownClass::*)(void*))RawBehPtr;");
					Context.Line("  (((asCUnknownClass*){0})->*CastedBehPtr)((void*){1});", AddressExpression, SourceExpression);
					break;
				default:
					check(false);
					break;
				}

				Context.Line("}");
				return;
			}
		}
		else
		{
			check(false);
			return;
		}
	}

	// Script assignment operator
	FCallScriptFunction ScriptCall(BehFunc, FCallScriptFunction::ELookupType::NoLookup);
	ScriptCall.OverrideObjectAddress = AddressExpression;
	ScriptCall.OverrideArguments.Add(SourceExpression);
	ScriptCall.MakeCall(Context);
}

FNativeFunctionCall GenerateCustomCall_PushArg(FNativeFunctionContext& Context, FStaticJITContext& JITContext, bool bIsRef)
{
	check(Context.ArgumentValues.Num() >= 1);
	check(Context.ObjectAddress.Len() == 0);
	check(!JITContext.bParmIsExecuted);

	int32 TypeId;
	if (Context.ArgumentValues.Num() == 2)
		TypeId = Context.LastPushedTypeId;
	else
		Context.CallingFunction->GetParam(0, &TypeId);

	auto DataType = JITContext.GetEngine()->GetDataTypeFromTypeId(TypeId);

	// Align member into the parm struct
	JITContext.ParmStructSize = Align(JITContext.ParmStructSize, DataType.GetAlignment());
	int32 ParmOffset = JITContext.ParmStructSize;

	FStaticJITContext::FPropertyInParm PropInParm;
	PropInParm.Offset = ParmOffset;
	PropInParm.TypeId = TypeId;
	PropInParm.bIsRef = bIsRef;
	PropInParm.SourceExpr = Context.ArgumentValues[0];
	JITContext.PropertiesInParm.Add(PropInParm);

	if (DataType.IsPrimitive() || DataType.IsEnumType())
	{
		switch (DataType.GetSizeInMemoryBytes())
		{
		case 1:
			JITContext.Line("(asBYTE&)l_ParmStruct[{0}] = *(asBYTE*){1};", ParmOffset, Context.ArgumentValues[0]);
		break;
		case 2:
			JITContext.Line("(asWORD&)l_ParmStruct[{0}] = *(asWORD*){1};", ParmOffset, Context.ArgumentValues[0]);
		break;
		case 4:
			JITContext.Line("(asDWORD&)l_ParmStruct[{0}] = *(asDWORD*){1};", ParmOffset, Context.ArgumentValues[0]);
		break;
		case 8:
			JITContext.Line("(asQWORD&)l_ParmStruct[{0}] = *(asQWORD*){1};", ParmOffset, Context.ArgumentValues[0]);
		break;
		}

		JITContext.ParmStructSize += DataType.GetSizeInMemoryBytes();
		return FNativeFunctionCall();
	}

	check(DataType.IsObject());

	if (DataType.IsReferenceType())
	{
		JITContext.Line("(UObject*&)l_ParmStruct[{0}] = *(UObject**){1};", ParmOffset, Context.ArgumentValues[0]);
		JITContext.ParmStructSize += 8;
		return FNativeFunctionCall();
	}

	FCopyConstructCall CopyConstr((asCObjectType*)DataType.GetTypeInfo());
	CopyConstr.CallCopyConstruct(JITContext,
		FString::Printf(TEXT("(void*)&l_ParmStruct[%d]"), ParmOffset),
		Context.ArgumentValues[0]);
	JITContext.ParmStructSize += DataType.GetSizeInMemoryBytes();

	return FNativeFunctionCall();
}

void CleanupPropertiesInParm(FNativeFunctionContext& Context, FStaticJITContext& JITContext)
{
	JITContext.bParmIsExecuted = true;
	for (auto& PropInParm : JITContext.PropertiesInParm)
	{
		auto DataType = JITContext.GetEngine()->GetDataTypeFromTypeId(PropInParm.TypeId);

		// Copy the value back into the original reference
		if (PropInParm.bIsRef)
		{
			if (DataType.IsPrimitive() || DataType.IsEnumType())
			{
				switch (DataType.GetSizeInMemoryBytes())
				{
				case 1:
					JITContext.Line("*(asBYTE*)({0}) = (asBYTE&)l_ParmStruct[{1}];", PropInParm.SourceExpr, PropInParm.Offset);
				break;
				case 2:
					JITContext.Line("*(asWORD*)({0}) = (asWORD&)l_ParmStruct[{1}];", PropInParm.SourceExpr, PropInParm.Offset);
				break;
				case 4:
					JITContext.Line("*(asDWORD*)({0}) = (asDWORD&)l_ParmStruct[{1}];", PropInParm.SourceExpr, PropInParm.Offset);
				break;
				case 8:
					JITContext.Line("*(asQWORD*)({0}) = (asQWORD&)l_ParmStruct[{1}];", PropInParm.SourceExpr, PropInParm.Offset);
				break;
				}
			}
			else if (DataType.IsReferenceType())
			{
				JITContext.Line("*(UObject**)({0}) = (UObject*&)l_ParmStruct[{1}];", PropInParm.SourceExpr, PropInParm.Offset);
			}
			else
			{
				FAssignCall Assign((asCObjectType*)DataType.GetTypeInfo());
				Assign.CallAssign(JITContext,
					PropInParm.SourceExpr,
					FString::Printf(TEXT("(void*)&l_ParmStruct[%d]"), PropInParm.Offset));
			}
		}

		// Destruct the value in the parm struct if we have to
		if (DataType.IsObject() && !DataType.IsReferenceType())
		{
			FDestructorCall Destructor((asCObjectType*)DataType.GetTypeInfo());
			Destructor.CallDestroy(JITContext,
				FString::Printf(TEXT("(void*)&l_ParmStruct[%d]"), PropInParm.Offset));
		}
	}
}

FNativeFunctionCall GenerateCustomCall_DelegateExecute(FNativeFunctionContext& Context, FStaticJITContext& JITContext)
{
	check(Context.ArgumentValues.Num() == 1);
	check(Context.ObjectAddress.Len() == 0);
	check(!JITContext.bParmIsExecuted);

	JITContext.Line("((FScriptDelegate*){0})->ProcessDelegate<UObject>(&l_ParmStruct[0]);", Context.ArgumentValues[0]);
	CleanupPropertiesInParm(Context, JITContext);
	return FNativeFunctionCall();
}

FNativeFunctionCall GenerateCustomCall_MulticastExecute(FNativeFunctionContext& Context, FStaticJITContext& JITContext)
{
	check(Context.ArgumentValues.Num() == 1);
	check(Context.ObjectAddress.Len() == 0);
	check(!JITContext.bParmIsExecuted);

	JITContext.Line("((FMulticastScriptDelegate*){0})->ProcessMulticastDelegate<UObject>(&l_ParmStruct[0]);", Context.ArgumentValues[0]);
	CleanupPropertiesInParm(Context, JITContext);
	return FNativeFunctionCall();
}

FNativeFunctionCall GenerateCustomCall_EventFunctionExecute(FNativeFunctionContext& Context, FStaticJITContext& JITContext)
{
	check(Context.ArgumentValues.Num() == 2);
	check(Context.ObjectAddress.Len() == 0);
	check(!JITContext.bParmIsExecuted);

	JITContext.Line("UObject* EventObject = ((UObject*){0});", Context.ArgumentValues[0]);
	JITContext.Line("UFunction* EventFunction = EventObject->FindFunctionChecked(*(FName*)({0}));", Context.ArgumentValues[1]);
	JITContext.Line("EventObject->ProcessEvent(EventFunction, &l_ParmStruct[0]);");
	CleanupPropertiesInParm(Context, JITContext);
	return FNativeFunctionCall();
}

IMPL_BYTECODE_BEGIN(asBC_PopPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.Pop(2);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return -2;
	};
};
IMPL_BYTECODE_END(asBC_PopPtr)

IMPL_BYTECODE_BEGIN(asBC_PshGPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.PushVolatilePtr("*(asPWORD*){0}", Context.ReferenceGlobalVariable(
			(void*)asBC_PTRARG(Context.BC)
		));
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PshGPtr)

IMPL_BYTECODE_BEGIN(asBC_PshC4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		auto& Expr = Context.PushStatic(1);
		Expr.ValueType = FStaticJITContext::EVariableType::DWord;
		Expr.Expression = Context.BCConstant_DWord();
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +1;
	};
};
IMPL_BYTECODE_END(asBC_PshC4)

IMPL_BYTECODE_BEGIN(asBC_PshV4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.PushVArgValue(1, 0);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +1;
	};
};
IMPL_BYTECODE_END(asBC_PshV4)

IMPL_BYTECODE_BEGIN(asBC_PSF)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		auto& Expr = Context.PushStatic(2);
		Expr.Expression = Context.VArg_Address(0);
		Expr.ValueType = FStaticJITContext::EVariableType::Pointer;
		Expr.bGuaranteeNonNull = true;
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PSF)

IMPL_BYTECODE_BEGIN(asBC_SwapPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		if (!Context.IsStackOffsetMaterialized(0)
			&& !Context.IsStackOffsetMaterialized(2))
		{
			FStaticJITContext::FStackExpression& Expr1 = Context.GetStaticAtStackOffset(0);
			FStaticJITContext::FStackExpression& Expr2 = Context.GetStaticAtStackOffset(2);

			int32 TempOffset = Expr1.OffsetOnStack;
			Expr1.OffsetOnStack = Expr2.OffsetOnStack;
			Expr2.OffsetOnStack = Expr1.OffsetOnStack;
		}
		else
		{
			Context.MaterializeStackAtOffset(0);
			Context.MaterializeStackAtOffset(2);

			Context.Line("{");
			Context.Line("asQWORD p = (asQWORD){0};", Context.StackValue_At(0, 2));
			Context.Line("*(asQWORD*){0} = (asQWORD){1};", Context.StackAddress_At(0), Context.StackValue_At(2, 2));
			Context.Line("*(asQWORD*){0} = p;", Context.StackAddress_At(2));
			Context.Line("}");
		}

		return true;
	}
};
IMPL_BYTECODE_END(asBC_SwapPtr)

IMPL_BYTECODE_BEGIN(asBC_NOT)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		switch (Context.VArg_Type(0))
		{
			case FStaticJITContext::EVariableType::Byte:
				Context.Line("{0} = !{0};", Context.VArg_Var(0));
			break;
			case FStaticJITContext::EVariableType::Word:
				Context.Line("{0} = (asWORD)(! (*(asBYTE*)&{0}));", Context.VArg_Var(0));
			break;
			case FStaticJITContext::EVariableType::DWord:
				Context.Line("{0} = (asDWORD)(! (*(asBYTE*)&{0}));", Context.VArg_Var(0));
			break;
			case FStaticJITContext::EVariableType::QWord:
				Context.Line("{0} = (asQWORD)(! (*(asBYTE*)&{0}));", Context.VArg_Var(0));
			break;
			default:
				check(false);
			break;
		}
		return true;
	}
};
IMPL_BYTECODE_END(asBC_NOT)

IMPL_BYTECODE_BEGIN(asBC_PshG4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.PushVolatileDWord("*(asDWORD*){0}", Context.ReferenceGlobalVariable(
			(void*)asBC_PTRARG(Context.BC)
		));
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +1;
	};
};
IMPL_BYTECODE_END(asBC_PshG4)

IMPL_BYTECODE_BEGIN(asBC_LdGRdR4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// Load the address of a global variable in the register, then
		// copy the value of the global variable into a local variable

		FString GlobalVariable = Context.ReferenceGlobalVariable((void*)asBC_PTRARG(Context.BC));
		Context.Line("l_valueRegister = (asQWORD){0};", GlobalVariable);

		switch (Context.VArg_Type(0))
		{
			case FStaticJITContext::EVariableType::DWord:
				Context.Line("{0} = *(asDWORD*){1};", Context.VArg_Var(0), GlobalVariable);
			break;
			case FStaticJITContext::EVariableType::Float:
				Context.Line("{0} = *(float*){1};", Context.VArg_Var(0), GlobalVariable);
			break;
			default:
				check(false);
			break;
		}

		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		return true;
	}

	virtual int32 GetUsageFlags(FStaticJITContext& Context) const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_LdGRdR4)

IMPL_BYTECODE_BEGIN(asBC_CALL)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int FunctionId = (int)asBC_DWORDARG(Context.BC);
		auto* ScriptFunction = (asCScriptFunction*)FAngelscriptEngine::Get().Engine->GetFunctionById(FunctionId);

		FCallScriptFunction Call(ScriptFunction, FCallScriptFunction::ELookupType::IdArg,
			/*IgnoreExceptions=*/false, /*ResolveVirtualFunction=*/false);
		Call.MakeCall(Context);

		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		int FunctionId = (int)asBC_DWORDARG(Context.BC);
		auto* ScriptFunction = (asCScriptFunction*)FAngelscriptEngine::Get().Engine->GetFunctionById(FunctionId);
		return -ScriptFunction->totalSpaceBeforeFunction;
	};
};
IMPL_BYTECODE_END(asBC_CALL)

IMPL_BYTECODE_BEGIN(asBC_SaveReturnValue)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		if (!Context.bReturnOnStack)
		{
			if (Context.bReturnIsReference)
			{
				Context.MaterializeValueRegister();
				return true;
			}
			else if (Context.bReturnIsPointer)
			{
				return true;
			}
			else if (Context.bReturnIsValue)
			{
				if (Context.bReturnIsFloatingPoint)
				{
					if (Context.ReturnSizeBytes == 4)
					{
						if (Context.ValueRegisterState != FStaticJITContext::EValueRegisterState::FloatRegister)
						{
							Context.MaterializeValueRegister();
							Context.Line("  l_floatRegister = value_as<float>(l_valueRegister);");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::FloatRegister;
						}
					}
					else if (Context.ReturnSizeBytes == 8)
					{
						if (Context.ValueRegisterState != FStaticJITContext::EValueRegisterState::DoubleRegister)
						{
							Context.MaterializeValueRegister();
							Context.Line("  l_doubleRegister = value_as<double>(l_valueRegister);");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DoubleRegister;
						}
					}
					else
					{
						check(false);
					}
				}
				else
				{
					switch (Context.ReturnSizeBytes)
					{
					case 1:
						if (Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::DWordRegister)
						{
							Context.Line("  l_byteRegister = (asBYTE&)l_dwordRegister;");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
						}
						else if (Context.ValueRegisterState != FStaticJITContext::EValueRegisterState::ByteRegister)
						{
							Context.MaterializeValueRegister();
							Context.Line("  l_byteRegister = (asBYTE&)l_valueRegister;");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
						}
					break;
					case 2:
						if (Context.ValueRegisterState != FStaticJITContext::EValueRegisterState::DWordRegister)
						{
							Context.MaterializeValueRegister();
							Context.Line("  l_dwordRegister = (asWORD&)l_valueRegister;");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DWordRegister;
						}
					break;
					case 4:
						if (Context.ValueRegisterState != FStaticJITContext::EValueRegisterState::DWordRegister)
						{
							Context.MaterializeValueRegister();
							Context.Line("  l_dwordRegister = (asDWORD&)l_valueRegister;");
							Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DWordRegister;
						}
					break;
					case 8:
						if (Context.ValueRegisterState != FStaticJITContext::EValueRegisterState::ValueRegister)
						{
							Context.MaterializeValueRegister();
						}
					break;
					default:
						check(false);
					break;
					}
				}

				return true;
			}
		}

		return true;
	}
};
IMPL_BYTECODE_END(asBC_SaveReturnValue)

IMPL_BYTECODE_BEGIN(asBC_RET)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		if (!Context.bReturnOnStack)
		{
			if (Context.bReturnIsReference)
			{
				check(Context.ValueRegisterState != FStaticJITContext::EValueRegisterState::ValueRegister || Context.ValueRegisterState != FStaticJITContext::EValueRegisterState::Indeterminate);
				Context.Line("  return ({0})l_valueRegister;", Context.LiteralReturnType);
				return true;
			}
			else if (Context.bReturnIsPointer)
			{
				Context.Line("  return ({0})l_objectRegister;", Context.LiteralReturnType);
				return true;
			}
			else if (Context.bReturnIsValue)
			{
				if (Context.bReturnIsFloatingPoint)
				{
					if (Context.ReturnSizeBytes == 4)
					{
						check(Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::FloatRegister || Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::Indeterminate);
						Context.Line("  return l_floatRegister;");
					}
					else if (Context.ReturnSizeBytes == 8)
					{
						check(Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::DoubleRegister || Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::Indeterminate);
						Context.Line("  return l_doubleRegister;");
					}
					else
					{
						check(false);
					}
				}
				else
				{
					switch (Context.ReturnSizeBytes)
					{
					case 1:
						check(Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::ByteRegister || Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::Indeterminate);
						Context.Line("  return ({0})l_byteRegister;", Context.LiteralReturnType);
					break;
					case 2:
						check(Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::DWordRegister || Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::Indeterminate);
						Context.Line("  return ({0})l_dwordRegister;", Context.LiteralReturnType);
					break;
					case 4:
						check(Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::DWordRegister || Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::Indeterminate);
						Context.Line("  return ({0})l_dwordRegister;", Context.LiteralReturnType);
					break;
					case 8:
						check(Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::ValueRegister || Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::Indeterminate);
						Context.Line("  return ({0})l_valueRegister;", Context.LiteralReturnType);
					break;
					default:
						check(false);
					break;
					}
				}

				return true;
			}
		}

		Context.Line("  return;");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_RET)

IMPL_BYTECODE_BEGIN(asBC_JMP)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		if (Context.IsPartOfSwitch())
			return;
		Context.AddLabel(asBC_INTARG(Context.BC) + 2, /*bConditional = */false);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		if (Context.IsPartOfSwitch())
			return true;

		// Unconditional Relative Jump
		Context.MaterializeWholeStack();
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_JMP)

IMPL_BYTECODE_BEGIN(asBC_JZ)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		Context.AddLabel(asBC_INTARG(Context.BC) + 2);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeWholeStack();
		FString ValueRegister = Context.GetValueRegisterForUnsignedComparisonMaybeMaterialize();
		Context.Line("if({0} == 0) {", ValueRegister);
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		Context.Line("}");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_JZ)

IMPL_BYTECODE_BEGIN(asBC_JNZ)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		Context.AddLabel(asBC_INTARG(Context.BC) + 2);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeWholeStack();
		FString ValueRegister = Context.GetValueRegisterForUnsignedComparisonMaybeMaterialize();
		Context.Line("if({0} != 0) {", ValueRegister);
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		Context.Line("}");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_JNZ)

IMPL_BYTECODE_BEGIN(asBC_JS)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		Context.AddLabel(asBC_INTARG(Context.BC) + 2);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeWholeStack();
		FString ValueRegister = Context.GetValueRegisterForSignedComparisonMaybeMaterialize();
		Context.Line("if({0} < 0) {", ValueRegister);
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		Context.Line("}");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_JS)

IMPL_BYTECODE_BEGIN(asBC_JNS)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		Context.AddLabel(asBC_INTARG(Context.BC) + 2);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeWholeStack();
		FString ValueRegister = Context.GetValueRegisterForSignedComparisonMaybeMaterialize();
		Context.Line("if({0} >= 0) {", ValueRegister);
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		Context.Line("}");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_JNS)

IMPL_BYTECODE_BEGIN(asBC_JP)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		Context.AddLabel(asBC_INTARG(Context.BC) + 2);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeWholeStack();
		FString ValueRegister = Context.GetValueRegisterForSignedComparisonMaybeMaterialize();
		Context.Line("if({0} > 0) {", ValueRegister);
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		Context.Line("}");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_JP)

IMPL_BYTECODE_BEGIN(asBC_JNP)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		Context.AddLabel(asBC_INTARG(Context.BC) + 2);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeWholeStack();
		FString ValueRegister = Context.GetValueRegisterForSignedComparisonMaybeMaterialize();
		Context.Line("if({0} <= 0) {", ValueRegister);
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		Context.Line("}");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_JNP)

IMPL_BYTECODE_BEGIN(asBC_TZ)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		FString ValueRegister = Context.GetValueRegisterForUnsignedComparisonMaybeMaterialize();
		Context.Line("if({0} == 0)", ValueRegister);
		Context.Line("   l_byteRegister = 1;");
		Context.Line("else");
		Context.Line("   l_byteRegister = 0;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_TZ)

IMPL_BYTECODE_BEGIN(asBC_TNZ)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		FString ValueRegister = Context.GetValueRegisterForUnsignedComparisonMaybeMaterialize();
		Context.Line("if({0} != 0)", ValueRegister);
		Context.Line("   l_byteRegister = 1;");
		Context.Line("else");
		Context.Line("   l_byteRegister = 0;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_TNZ)

IMPL_BYTECODE_BEGIN(asBC_TS)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		FString ValueRegister = Context.GetValueRegisterForSignedComparisonMaybeMaterialize();
		Context.Line("if({0} < 0)", ValueRegister);
		Context.Line("   l_byteRegister = 1;");
		Context.Line("else");
		Context.Line("   l_byteRegister = 0;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_TS)

IMPL_BYTECODE_BEGIN(asBC_TNS)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		FString ValueRegister = Context.GetValueRegisterForSignedComparisonMaybeMaterialize();
		Context.Line("if({0} >= 0)", ValueRegister);
		Context.Line("   l_byteRegister = 1;");
		Context.Line("else");
		Context.Line("   l_byteRegister = 0;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_TNS)

IMPL_BYTECODE_BEGIN(asBC_TP)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		FString ValueRegister = Context.GetValueRegisterForSignedComparisonMaybeMaterialize();
		Context.Line("if({0} > 0)", ValueRegister);
		Context.Line("   l_byteRegister = 1;");
		Context.Line("else");
		Context.Line("   l_byteRegister = 0;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_TP)

IMPL_BYTECODE_BEGIN(asBC_TNP)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		FString ValueRegister = Context.GetValueRegisterForSignedComparisonMaybeMaterialize();
		Context.Line("if({0} <= 0)", ValueRegister);
		Context.Line("   l_byteRegister = 1;");
		Context.Line("else");
		Context.Line("   l_byteRegister = 0;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_TNP)

IMPL_BYTECODE_BEGIN(asBC_NEGi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.Line("{0} = - {0};", Context.VArg_SignedVar(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_NEGi)

IMPL_BYTECODE_BEGIN(asBC_NEGf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		Context.Line("{0} = -{0};", Context.VArg_Var(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_NEGf)

IMPL_BYTECODE_BEGIN(asBC_NEGd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		Context.Line("{0} = -{0};", Context.VArg_Var(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_NEGd)

IMPL_BYTECODE_BEGIN(asBC_INCi16)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(int16**)&l_valueRegister)++;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_INCi16)

IMPL_BYTECODE_BEGIN(asBC_INCi8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(int8**)&l_valueRegister)++;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_INCi8)

IMPL_BYTECODE_BEGIN(asBC_DECi16)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(int16**)&l_valueRegister)--;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_DECi16)

IMPL_BYTECODE_BEGIN(asBC_DECi8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(int8**)&l_valueRegister)--;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_DECi8)

IMPL_BYTECODE_BEGIN(asBC_INCi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(int32**)&l_valueRegister)++;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_INCi)

IMPL_BYTECODE_BEGIN(asBC_DECi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(int32**)&l_valueRegister)--;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_DECi)

IMPL_BYTECODE_BEGIN(asBC_INCf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(float**)&l_valueRegister)++;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_INCf)

IMPL_BYTECODE_BEGIN(asBC_DECf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(float**)&l_valueRegister)--;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_DECf)

IMPL_BYTECODE_BEGIN(asBC_INCd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(double**)&l_valueRegister)++;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_INCd)

IMPL_BYTECODE_BEGIN(asBC_DECd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("(**(double**)&l_valueRegister)--;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_DECd)

IMPL_BYTECODE_BEGIN(asBC_IncVi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		Context.Line("{0}++;", Context.VArg_Var(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_IncVi)

IMPL_BYTECODE_BEGIN(asBC_DecVi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		Context.Line("{0}--;", Context.VArg_Var(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_DecVi)

IMPL_BYTECODE_BEGIN(asBC_BNOT)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		Context.Line("{0} = ~{0};", Context.VArg_Var(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BNOT)

IMPL_BYTECODE_BEGIN(asBC_BAND)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));
		check(Context.VArg_IsNumeric(2));
		Context.Line("{0} = {1} & {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BAND)

IMPL_BYTECODE_BEGIN(asBC_BOR)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));
		check(Context.VArg_IsNumeric(2));
		Context.Line("{0} = {1} | {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BOR)

IMPL_BYTECODE_BEGIN(asBC_BXOR)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));
		check(Context.VArg_IsNumeric(2));
		Context.Line("{0} = {1} ^ {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BXOR)

IMPL_BYTECODE_BEGIN(asBC_BSLL)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));
		check(Context.VArg_IsNumeric(2));
		Context.Line("{0} = {1} << {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BSLL)

IMPL_BYTECODE_BEGIN(asBC_BSRL)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));
		check(Context.VArg_IsNumeric(2));
		Context.Line("{0} = {1} >> {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BSRL)

IMPL_BYTECODE_BEGIN(asBC_BSRA)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));
		check(Context.VArg_IsNumeric(2));
		Context.Line("{0} = {1} >> {2};", Context.VArg_Var(0), Context.VArg_SignedVar(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BSRA)

IMPL_BYTECODE_BEGIN(asBC_CopyScript)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		asCObjectType* TypeInfo = (asCObjectType*)asBC_PTRARG(Context.BC);

		check(TypeInfo->beh.copy == TypeInfo->engine->scriptTypeBehaviours.beh.copy);
		check((TypeInfo->flags & asOBJ_VALUE) != 0);
		check((TypeInfo->flags & asOBJ_POD) == 0);

		Context.Line("{");
		Context.Line("  auto* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(TypeInfo));
		Context.Line("  auto* d = (asCScriptObject*){0};", Context.StackValue_At(0, 2));
		Context.Line("  auto* s = (asCScriptObject*){0};", Context.StackValue_At(2, 2));

		if (!Context.IsStackValueNonNull(0) || !Context.IsStackValueNonNull(2))
		{
			Context.Line("  if (s == nullptr || d == nullptr) [[unlikely]]");
			Context.Line("  {");
			Context.DebugLineNumber();
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("  }");
		}

		Context.PopMultiple(4);

		Context.Line("  d->CopyStruct(s, objType);");
		Context.PushPtr("d");

		Context.Line("}");
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return -2;
	};
};
IMPL_BYTECODE_END(asBC_CopyScript)

IMPL_BYTECODE_BEGIN(asBC_COPY)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.Line("{");
		Context.Line("  auto* d = (void*){0};", Context.StackValue_At(0, 2));
		Context.Line("  auto* s = (void*){0};", Context.StackValue_At(2, 2));

		if (!Context.IsStackValueNonNull(0) || !Context.IsStackValueNonNull(2))
		{
			Context.Line("  if (s == nullptr || d == nullptr) [[unlikely]]");
			Context.Line("  {");
			Context.DebugLineNumber();
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("  }");
		}

		Context.PopMultiple(4);

		int32 TypeId = asBC_DWORDARG(Context.BC);
		auto* TypeInfo = Context.GetEngine()->GetTypeInfoById(TypeId);

		Context.Line("  FMemory::Memcpy(d, s, {0});", Context.ReferenceTypeSize(TypeInfo));

		Context.PushPtr("d");
		Context.Line("}");
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return -2;
	};
};
IMPL_BYTECODE_END(asBC_COPY)

IMPL_BYTECODE_BEGIN(asBC_PshC8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		auto& Expr = Context.PushStatic(2);
		Expr.ValueType = FStaticJITContext::EVariableType::QWord;
		Expr.Expression = Context.BCConstant_QWord();
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PshC8)

IMPL_BYTECODE_BEGIN(asBC_PshVPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.PushVArgValue(2, 0);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PshVPtr)

IMPL_BYTECODE_BEGIN(asBC_RDSPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.Line("{");
		Context.Line("  asPWORD a = (asPWORD){0};", Context.StackValue_At(0, 2));
		if (!Context.IsStackValueNonNull(0))
		{
			Context.Line("  if (a == 0) [[unlikely]]");
			Context.Line("  {");
			Context.DebugLineNumber();
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("  }");
		}
		Context.Pop(2);
		Context.PushPtr("*(asPWORD*)a");
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_RDSPtr)

IMPL_BYTECODE_BEGIN(asBC_CMPd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);

		Context.Line("  if ({0} == {1})     l_byteRegister = 0;", Context.VArg_Var(0), Context.VArg_Var(1));
		Context.Line("  else if ({0} < {1}) l_byteRegister = -1;", Context.VArg_Var(0), Context.VArg_Var(1));
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPd)

IMPL_BYTECODE_BEGIN(asBC_CMPu)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));

		Context.Line("  if ({0} == {1})     l_byteRegister = 0;", Context.VArg_Var(0), Context.VArg_Var(1));
		Context.Line("  else if ({0} < {1}) l_byteRegister = -1;", Context.VArg_Var(0), Context.VArg_Var(1));
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPu)

IMPL_BYTECODE_BEGIN(asBC_CMPf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);

		Context.Line("  if ({0} == {1})     l_byteRegister = 0;", Context.VArg_Var(0), Context.VArg_Var(1));
		Context.Line("  else if ({0} < {1}) l_byteRegister = -1;", Context.VArg_Var(0), Context.VArg_Var(1));
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPf)

IMPL_BYTECODE_BEGIN(asBC_CMPi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));

		Context.Line("  if ({0} == {1})     l_byteRegister = 0;", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1));
		Context.Line("  else if ({0} < {1}) l_byteRegister = -1;", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1));
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPi)

IMPL_BYTECODE_BEGIN(asBC_CMPIi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));

		Context.Line("  if ({0} == value_as<int>({1}))     l_byteRegister = 0;", Context.VArg_SignedVar(0), Context.BCConstant_DWord());
		Context.Line("  else if ({0} < value_as<int>({1})) l_byteRegister = -1;", Context.VArg_SignedVar(0), Context.BCConstant_DWord());
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPIi)

IMPL_BYTECODE_BEGIN(asBC_CMPIf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);

		Context.Line("  if ({0} == value_as<float>({1}))     l_byteRegister = 0;", Context.VArg_Var(0), Context.BCConstant_DWord());
		Context.Line("  else if ({0} < value_as<float>({1})) l_byteRegister = -1;", Context.VArg_Var(0), Context.BCConstant_DWord());
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPIf)

IMPL_BYTECODE_BEGIN(asBC_CMPIu)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));

		Context.Line("  if ({0} == value_as<uint32>({1}))     l_byteRegister = 0;", Context.VArg_Var(0), Context.BCConstant_DWord());
		Context.Line("  else if ({0} < value_as<uint32>({1})) l_byteRegister = -1;", Context.VArg_Var(0), Context.BCConstant_DWord());
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPIu)

IMPL_BYTECODE_BEGIN(asBC_JMPP)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		uint32 LabelIndex = 0;
		uint32 MaxLabel = asBC_DWORDARG(Context.BC);

		for (int32 i = Context.CurrentInstructionIndex+1, Count = Context.Instructions.Num(); i < Count; ++i)
		{
			auto& JmpInstr = Context.Instructions[i];
			if ((asEBCInstr)*(asBYTE*)JmpInstr.BC != asBC_JMP)
				break;
			if (LabelIndex > MaxLabel)
				break;

			JmpInstr.bJumpPartOfSwitch = true;
			Context.AddLabel(
				(JmpInstr.BC - Context.BC) +
				asBC_INTARG(JmpInstr.BC) + 2,
				/*bConditional = */true);

			LabelIndex += 1;
		}
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));

		// Create a new switch statement with jumps inside it
		Context.MaterializeWholeStack();
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;

		Context.Line("switch({0})", Context.VArg_Var(0));
		Context.Line("{");

		uint32 MaxLabel = asBC_DWORDARG(Context.BC);
		uint32 LabelIndex = 0;
		for (int32 i = Context.CurrentInstructionIndex+1, Count = Context.Instructions.Num(); i < Count; ++i)
		{
			auto& JmpInstr = Context.Instructions[i];
			if ((asEBCInstr)*(asBYTE*)JmpInstr.BC != asBC_JMP)
				break;
			if (LabelIndex > MaxLabel)
				break;

			Context.Line("case {0}:", LabelIndex);
			Context.Jump(Context.GetLabelInstruction(
				(JmpInstr.BC - Context.BC) +
				asBC_INTARG(JmpInstr.BC) + 2
			));
			Context.Line("break;");

			LabelIndex += 1;
		}

		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_JMPP)

IMPL_BYTECODE_BEGIN(asBC_PopRPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.Line("l_valueRegister = (asQWORD){0};", Context.StackValue_At(0, 2));
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		Context.Pop(2);
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return -2;
	};
};
IMPL_BYTECODE_END(asBC_PopRPtr)

IMPL_BYTECODE_BEGIN(asBC_PshRPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.PushPtr("l_valueRegister");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PshRPtr)

IMPL_BYTECODE_BEGIN(asBC_STR)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// This instruction is deprecated. Should never occur in bytecode.
		check(false);
		return false;
	}
};
IMPL_BYTECODE_END(asBC_STR)

IMPL_BYTECODE_BEGIN(asBC_CALLSYS)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		auto* ScriptFunction = (asCScriptFunction*)asBC_PTRARG(Context.BC);

		FScriptSystemCall Call(ScriptFunction, FScriptSystemCall::ELookupType::PtrArg);
		Call.MakeCall(Context);
		return true;
	}

	virtual int32 GetUsageFlags(FStaticJITContext& Context) const override
	{
		auto* ScriptFunction = (asCScriptFunction*)asBC_PTRARG(Context.BC);

		FScriptSystemCall Call(ScriptFunction, FScriptSystemCall::ELookupType::PtrArg);
		return Call.GetUsageFlags();
	};

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		auto* ScriptFunction = (asCScriptFunction*)asBC_PTRARG(Context.BC);
		return -ScriptFunction->totalSpaceBeforeFunction;
	};
};
IMPL_BYTECODE_END(asBC_CALLSYS)

IMPL_BYTECODE_BEGIN(asBC_Thiscall1)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		auto* ScriptFunction = (asCScriptFunction*)asBC_PTRARG(Context.BC);

		FScriptSystemCall Call(ScriptFunction, FScriptSystemCall::ELookupType::PtrArg);
		Call.MakeCall(Context);
		return true;
	}

	virtual int32 GetUsageFlags(FStaticJITContext& Context) const override
	{
		auto* ScriptFunction = (asCScriptFunction*)asBC_PTRARG(Context.BC);

		FScriptSystemCall Call(ScriptFunction, FScriptSystemCall::ELookupType::PtrArg);
		return Call.GetUsageFlags();
	};

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		auto* ScriptFunction = (asCScriptFunction*)asBC_PTRARG(Context.BC);
		return -ScriptFunction->totalSpaceBeforeFunction;
	};
};
IMPL_BYTECODE_END(asBC_Thiscall1)

IMPL_BYTECODE_BEGIN(asBC_CALLBND)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int BindId = asBC_INTARG(Context.BC);
		asCScriptFunction* ScriptFunction = ((asCScriptEngine*)FAngelscriptEngine::Get().Engine)->importedFunctions[(int)BindId & ~FUNC_IMPORTED]->importedFunctionSignature;

		FCallScriptFunction Call(ScriptFunction, FCallScriptFunction::ELookupType::BindIdArg);
		Call.MakeCall(Context);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		int BindId = asBC_INTARG(Context.BC);
		asCScriptFunction* f = ((asCScriptEngine*)FAngelscriptEngine::Get().Engine)->importedFunctions[(int)BindId & ~FUNC_IMPORTED]->importedFunctionSignature;
		if (f)
			return -f->totalSpaceBeforeFunction;
		else
			return 0;
	};
};
IMPL_BYTECODE_END(asBC_CALLBND)

IMPL_BYTECODE_BEGIN(asBC_SUSPEND)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SUSPEND)

IMPL_BYTECODE_BEGIN(asBC_FinConstruct)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		asITypeInfo* TypeInfo = (asITypeInfo*)asBC_PTRARG(Context.BC);

		Context.Line("{");
		Context.Line("  asIScriptObject* Object = (asIScriptObject*){0};", Context.StackValue_At(0, 2));
		Context.Line("  asITypeInfo* TypeInfo = (asITypeInfo*){0};", Context.ReferenceTypeInfo(TypeInfo));
		Context.Pop(2);
		Context.Line("  SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo);");
		Context.Line("}");

		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return -2;
	};
};
IMPL_BYTECODE_END(asBC_FinConstruct)

IMPL_BYTECODE_BEGIN(asBC_ALLOC)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		asCObjectType *objType = (asCObjectType*)asBC_PTRARG(Context.BC);
		int func = asBC_INTARG(Context.BC + 2);
		asCScriptFunction* f = (func == 0) ? nullptr : (asCScriptFunction*)FAngelscriptEngine::Get().Engine->GetFunctionById(func);

		if ((objType->flags & asOBJ_SCRIPT_OBJECT) != 0)
		{
			Context.Line("{");

			Context.Line("  asCObjectType* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(objType));
			Context.Line("  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->AllocScriptObject(objType);");
			Context.Line("  ScriptObject_Construct(objType, (asCScriptObject*)mem);");

			Context.Line("  asPWORD* a = (asPWORD*){0};", Context.StackValue_At(f->spaceNeededForArguments, 2));
			Context.Line("  if(a != nullptr) *a = (asPWORD)mem;");

			Context.PushPtr("mem");

			FCallScriptFunction Call(f, FCallScriptFunction::ELookupType::IdArg_AfterPtr);
			Call.MakeCall(Context);

			Context.Line("}");
		}
		else if(f != nullptr)
		{
			Context.AddStateVar("void*", "AllocMemory");

			Context.Line("{");
			Context.Line("  asCObjectType* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(objType));
			Context.Line("  AllocMemory = (void*)SCRIPT_ENGINE->CallAlloc(objType);");
			Context.Line("}");

			Context.PushPtr("AllocMemory");

			FScriptSystemCall Call(f, FScriptSystemCall::ELookupType::IdArg_AfterPtr);
			Call.MakeCall(Context);

			Context.Line("{");
			Context.Line("  asPWORD* a = (asPWORD*){0};", Context.StackValue_At(0, 2));
			Context.Pop(2);
			Context.Line("  if(a != nullptr) *a = (asPWORD)AllocMemory;");
			Context.Line("}");
		}
		else
		{
			Context.Line("{");
			Context.Line("  asCObjectType* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(objType));
			Context.Line("  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->CallAlloc(objType);");
			Context.Line("  asPWORD* a = (asPWORD*){0};", Context.StackValue_At(0, 2));
			Context.Pop(2);
			Context.Line("  if(a != nullptr) *a = (asPWORD)mem;");
			Context.Line("}");
		}

		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		asCObjectType *objType = (asCObjectType*)asBC_PTRARG(Context.BC);
		int func = asBC_INTARG(Context.BC + 2);
		asCScriptFunction* f = (func == 0) ? nullptr : (asCScriptFunction*)FAngelscriptEngine::Get().Engine->GetFunctionById(func);
		if ((objType->flags & asOBJ_SCRIPT_OBJECT) != 0)
			return -(f->totalSpaceBeforeFunction - 2);
		else if (f)
			return -f->totalSpaceBeforeFunction;
		else
			return -2;
	};
};
IMPL_BYTECODE_END(asBC_ALLOC)

IMPL_BYTECODE_BEGIN(asBC_FREE)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);
		asCObjectType *objType = (asCObjectType*)asBC_PTRARG(Context.BC);

		if ((objType->flags & asOBJ_SCRIPT_OBJECT) != 0 && objType->GetUserData() == nullptr)
		{
			Context.Line("{");
			Context.Line("  void* obj = (void*){0};", Context.VArg_Var(0));
			Context.Line("  if (obj != nullptr)");
			Context.Line("  {");
			Context.Line("    asCObjectType* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(objType));
			Context.Line("    SCRIPT_ENGINE->ReleaseRawScriptObject(obj, objType);");
			Context.Line("  }");
			Context.Line("}");
		}
		else if ((objType->flags & asOBJ_REF) != 0)
		{
			Context.Line("{");
			Context.Line("  void* obj = (void*){0};", Context.VArg_Var(0));
			Context.Line("  if (obj != nullptr)");
			Context.Line("  {");
			Context.Line("    asCObjectType* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(objType));
			Context.Line("    if ((objType->flags & asOBJ_NOCOUNT) == 0 && objType->beh.release != 0)");
			Context.Line("      SCRIPT_ENGINE->CallObjectMethod(obj, objType->beh.release);");
			Context.Line("  }");
			Context.Line("}");
		}
		else
		{
			Context.Line("{");
			Context.Line("  void* obj = (void*){0};", Context.VArg_Var(0));
			Context.Line("  if (obj != nullptr)");
			Context.Line("  {");
			Context.Line("    asCObjectType* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(objType));

			FDestructorCall DestructorCall(objType);
			DestructorCall.CallDestroy(Context, TEXT("obj"));

			Context.Line("    SCRIPT_ENGINE->CallFree(obj);");
			Context.Line("  }");
			Context.Line("}");
		}

		Context.Line("{0} = nullptr;", Context.VArg_Var(0));

		return true;
	}
};
IMPL_BYTECODE_END(asBC_FREE)

IMPL_BYTECODE_BEGIN(asBC_DestructScript)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(false);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_DestructScript)

IMPL_BYTECODE_BEGIN(asBC_LOADOBJ)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);
		Context.Line("l_objectRegister = (void*){0};", Context.VArg_Var(0));
		Context.Line("{0} = nullptr;", Context.VArg_Var(0));
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_objectRegister;
	};
};
IMPL_BYTECODE_END(asBC_LOADOBJ)

IMPL_BYTECODE_BEGIN(asBC_STOREOBJ)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);
		Context.Line("{0} = ({1})l_objectRegister;", Context.VArg_Var(0), Context.VArg_LiteralTypename(0));
		Context.Line("l_objectRegister = nullptr;");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_objectRegister;
	};
};
IMPL_BYTECODE_END(asBC_STOREOBJ)

IMPL_BYTECODE_BEGIN(asBC_GETOBJ)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int32 Offset = asBC_WORDARG0(Context.BC);
		int32 VarOffset = asBC_SWORDARG1(Context.BC);

		if (!Context.IsStackOffsetMaterialized(Offset))
		{
			auto& Expr = Context.GetStaticAtStackOffset(Offset);
			check(Expr.bIsVarLiteral);
			int32 VarLiteral = Expr.VarOffsetLiteral;
			Expr.bIsVarLiteral = false;
			check(VarLiteral == VarOffset);

			Context.Line("  asPWORD v = (asPWORD){0};", Context.VAddress(VarLiteral));
			Expr.Expression = TEXT("*v");

			Context.MaterializeStackAtOffset(Offset);
			Context.Line("  *(asPWORD*)v = 0;");
		}
		else
		{
			Context.Line("{");
			Context.Line("  asPWORD v = (asPWORD){0};", Context.VAddress(VarOffset));
			Context.Line("  *(asPWORD*){0} = *(asPWORD*)v;", Context.StackAddress_At(Offset));
			Context.Line("  *(asPWORD*)v = 0;");
			Context.Line("}");
		}
		return true;
	}
};
IMPL_BYTECODE_END(asBC_GETOBJ)

IMPL_BYTECODE_BEGIN(asBC_REFCPY)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		asCObjectType* objType = (asCObjectType*)asBC_PTRARG(Context.BC);
		const bool bRawScriptReference =
			(objType->flags & asOBJ_SCRIPT_OBJECT) != 0
			&& objType->GetUserData() == nullptr;

		Context.Line("{");
		Context.Line("  asPWORD* d = (asPWORD*){0};", Context.StackValue_At(0, 2));
		Context.Pop(2);
		Context.Line("  asPWORD s = (asPWORD){0};", Context.StackValue_At(0, 2));
		Context.Line("  asCObjectType* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(objType));
		Context.Line("  if (*d != s)");
		Context.Line("  {");
		if (bRawScriptReference)
		{
			Context.Line("    if (*d != 0)");
			Context.Line("      SCRIPT_ENGINE->ReleaseRawScriptObject((void*)*d, objType);");
			Context.Line("    if (s != 0)");
			Context.Line("      SCRIPT_ENGINE->AddRawScriptObjectReference((void*)s, objType);");
		}
		else
		{
			Context.Line("    if ((objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE)) == 0)");
			Context.Line("    {");
			Context.Line("      if (*d != 0 && objType->beh.release != 0)");
			Context.Line("        SCRIPT_ENGINE->CallObjectMethod((void*)*d, objType->beh.release);");
			Context.Line("      if (s != 0 && objType->beh.addref != 0)");
			Context.Line("        SCRIPT_ENGINE->CallObjectMethod((void*)s, objType->beh.addref);");
			Context.Line("    }");
		}
		Context.Line("  }");
		Context.Line("  if (*d != s)");
		Context.Line("    *d = s;");
		Context.Line("}");
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return -2;
	};
};
IMPL_BYTECODE_END(asBC_REFCPY)

IMPL_BYTECODE_BEGIN(asBC_CHKREF)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		if (!Context.IsStackValueNonNull(0))
		{
			Context.Line("if ({0} == 0) [[unlikely]]", Context.StackValue_At(0, 2));
			Context.Line("{");
			Context.DebugLineNumber();
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("}");
		}
		return true;
	}
};
IMPL_BYTECODE_END(asBC_CHKREF)

IMPL_BYTECODE_BEGIN(asBC_GETOBJREF)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int32 Offset = asBC_WORDARG0(Context.BC);
		int32 VarOffset = asBC_SWORDARG1(Context.BC);

		if (!Context.IsStackOffsetMaterialized(Offset))
		{
			auto& Expr = Context.GetStaticAtStackOffset(Offset);
			check(Expr.bIsVarLiteral);
			int32 VarLiteral = Expr.VarOffsetLiteral;
			Expr.bIsVarLiteral = false;
			check(VarLiteral == VarOffset);

			Expr.Expression = FString::Printf(TEXT("*(asPWORD*)%s"), *Context.VAddress(VarLiteral));

			Context.MaterializeStackAtOffset(Offset);
		}
		else
		{
			Context.Line("*(asPWORD*){0} = *(asPWORD*){1};", Context.StackAddress_At(Offset), Context.VAddress(VarOffset));
		}

		return true;
	}
};
IMPL_BYTECODE_END(asBC_GETOBJREF)

IMPL_BYTECODE_BEGIN(asBC_GETREF)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int32 Offset = asBC_WORDARG0(Context.BC);
		int32 VarOffset = asBC_SWORDARG1(Context.BC);

		if (!Context.IsStackOffsetMaterialized(Offset))
		{
			auto& Expr = Context.GetStaticAtStackOffset(Offset);
			check(Expr.bIsVarLiteral);
			int32 VarLiteral = Expr.VarOffsetLiteral;
			Expr.bIsVarLiteral = false;
			check(VarLiteral == VarOffset);

			Expr.VarOffsetLiteral = -1;
			Expr.Expression = FString::Printf(TEXT("(asPWORD)%s"), *Context.VAddress(VarLiteral));
		}
		else
		{
			Context.Line("*(asPWORD*){0} = (asPWORD){1};", Context.StackAddress_At(Offset), Context.VAddress(VarOffset));
		}
		return true;
	}
};
IMPL_BYTECODE_END(asBC_GETREF)

IMPL_BYTECODE_BEGIN(asBC_PshNull)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		auto& Expr = Context.PushStatic(2);
		Expr.ValueType = FStaticJITContext::EVariableType::Pointer;
		Expr.Expression = TEXT("nullptr");
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PshNull)

IMPL_BYTECODE_BEGIN(asBC_ClrVPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);
		Context.Line("{0} = nullptr;", Context.VArg_Var(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ClrVPtr)

IMPL_BYTECODE_BEGIN(asBC_OBJTYPE)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		asITypeInfo* TypeInfo = (asITypeInfo*)asBC_PTRARG(Context.BC);

		auto& Expr = Context.PushStatic(2);
		Expr.ValueType = FStaticJITContext::EVariableType::Pointer;
		Expr.Expression = Context.ReferenceTypeInfo(TypeInfo);
		Expr.bGuaranteeNonNull = true;
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_OBJTYPE)

IMPL_BYTECODE_BEGIN(asBC_TYPEID)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int32 TypeId = asBC_DWORDARG(Context.BC);
		auto* TypeInfo = Context.GetEngine()->GetTypeInfoById(TypeId);

		Context.LastPushedTypeInfo = TypeInfo;
		Context.LastPushedTypeId = TypeId;

		auto& Expr = Context.PushStatic(1);
		Expr.ValueType = FStaticJITContext::EVariableType::DWord;
		Expr.Expression = Context.ReferenceTypeId(TypeId);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +1;
	};
};
IMPL_BYTECODE_END(asBC_TYPEID)

void ByteCode_SetVArg_Constant(FStaticJITContext& Context)
{
	switch (Context.VArg_Type(0))
	{
	case FStaticJITContext::EVariableType::Byte:
	{
		asDWORD Arg = asBC_DWORDARG(Context.BC);
		check(Arg <= 0xff);
		Context.Line("{0} = 0x{1}u;", Context.VArg_Var(0), FString::Printf(TEXT("%x"), Arg));
	}
	break;
	case FStaticJITContext::EVariableType::Word:
	{
		asDWORD Arg = asBC_DWORDARG(Context.BC);
		check(Arg <= 0xffff);
		Context.Line("{0} = 0x{1}u;", Context.VArg_Var(0), FString::Printf(TEXT("%x"), Arg));
	}
	break;
	case FStaticJITContext::EVariableType::DWord:
	{
		asDWORD Arg = asBC_DWORDARG(Context.BC);
		Context.Line("{0} = 0x{1}u;", Context.VArg_Var(0), FString::Printf(TEXT("%x"), Arg));
	}
	break;
	case FStaticJITContext::EVariableType::QWord:
	{
		asQWORD Arg = asBC_QWORDARG(Context.BC);
		Context.Line("{0} = 0x{1}ull;", Context.VArg_Var(0), FString::Printf(TEXT("%llx"), Arg));
	}
	break;
	case FStaticJITContext::EVariableType::Float:
	{
		asDWORD Arg = asBC_DWORDARG(Context.BC);
		Context.Line("{0} = value_as<float>(0x{1}u);", Context.VArg_Var(0), FString::Printf(TEXT("%x"), Arg));
	}
	break;
	case FStaticJITContext::EVariableType::Double:
	{
		asQWORD Arg = asBC_QWORDARG(Context.BC);
		Context.Line("{0} = value_as<double>(0x{1}ull);", Context.VArg_Var(0), FString::Printf(TEXT("%llx"), Arg));
	}
	break;
	default:
		check(false);
	break;
	}
}

IMPL_BYTECODE_BEGIN(asBC_SetV4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		ByteCode_SetVArg_Constant(Context);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SetV4)

IMPL_BYTECODE_BEGIN(asBC_SetV8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		ByteCode_SetVArg_Constant(Context);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SetV8)

IMPL_BYTECODE_BEGIN(asBC_FreeNullV8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);
		Context.Line("{0} = nullptr;", Context.VArg_Var(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_FreeNullV8)

// These bytecodes only maintain AS_REFERENCE_DEBUGGING state in the interpreter.
// StaticJIT accepts them so editor-generated bytecode can still be emitted as AOT.
IMPL_BYTECODE_BEGIN(asBC_TrackRef)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		return true;
	}
};
IMPL_BYTECODE_END(asBC_TrackRef)

IMPL_BYTECODE_BEGIN(asBC_UntrackRef)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		return true;
	}
};
IMPL_BYTECODE_END(asBC_UntrackRef)

IMPL_BYTECODE_BEGIN(asBC_ValidateRef)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ValidateRef)

IMPL_BYTECODE_BEGIN(asBC_ADDSi)
{
	FString OffsetExpr(FStaticJITContext& Context) const
	{
		int32 TypeId = asBC_INTARG(Context.BC);
		int32 Offset = asBC_SWORDARG0(Context.BC);

		// Store a reference to the unknown property so we can look it up in the new place
		return Context.ReferencePropertyOffset(Offset, TypeId);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		if (!Context.IsStackOffsetMaterialized(0))
		{
			auto& Expr = Context.GetStaticAtStackOffset(0);
			if (!Expr.bGuaranteeNonNull)
			{
				Context.Line("{");
				Context.Line("  if ((asPWORD){0} == 0) [[unlikely]]", Context.StackValue_At(0, 2));
				Context.Line("  {");
				Context.DebugLineNumber();
				Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
				Context.Line("  }");
				Context.Line("}");
			}

			Expr.Expression = FString::Printf(TEXT("(((asPWORD)%s)+%s)"), *Expr.Expression, *OffsetExpr(Context));
		}
		else
		{
			Context.Line("{");
			Context.Line("  asPWORD a = (asPWORD){0};", Context.StackValue_At(0, 2));
			Context.Line("  if (a == 0) [[unlikely]]");
			Context.Line("  {");
			Context.DebugLineNumber();
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("  }");

			Context.Pop(2);
			Context.PushPtr("a + {0}", OffsetExpr(Context));

			Context.Line("}");
		}

		return true;
	}
};
IMPL_BYTECODE_END(asBC_ADDSi)

void ByteCode_CopyVArg(FStaticJITContext& Context)
{
	check(Context.VArg_Type(1) == Context.VArg_Type(0));
	Context.Line("{0} = {1};", Context.VArg_Var(0), Context.VArg_Var(1));
}

IMPL_BYTECODE_BEGIN(asBC_CpyVtoV4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		ByteCode_CopyVArg(Context);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_CpyVtoV4)

IMPL_BYTECODE_BEGIN(asBC_CpyVtoV8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		ByteCode_CopyVArg(Context);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_CpyVtoV8)

IMPL_BYTECODE_BEGIN(asBC_CpyVtoR1)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Byte);

		Context.Line("l_byteRegister = {0};", Context.VArg_Var(0));
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CpyVtoR1)

IMPL_BYTECODE_BEGIN(asBC_CpyVtoR4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		if (Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float)
		{
			Context.Line("l_floatRegister = {0};", Context.VArg_Var(0));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::FloatRegister;
		}
		else if (Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord)
		{
			Context.Line("l_dwordRegister = {0};", Context.VArg_Var(0));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DWordRegister;
		}
		else if (Context.VArg_Type(0) == FStaticJITContext::EVariableType::Word)
		{
			Context.Line("l_dwordRegister = {0};", Context.VArg_Var(0));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DWordRegister;
		}
		else if (Context.VArg_Type(0) == FStaticJITContext::EVariableType::Byte)
		{
			Context.Line("l_byteRegister = {0};", Context.VArg_Var(0));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		}
		else
		{
			check(false);
		}

		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CpyVtoR4)

IMPL_BYTECODE_BEGIN(asBC_CpyVtoR8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		if (Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double)
		{
			Context.Line("l_doubleRegister = {0};", Context.VArg_Var(0));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::DoubleRegister;
		}
		else if (Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord)
		{
			Context.Line("l_valueRegister = {0};", Context.VArg_Var(0));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		}
		else if (Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer)
		{
			Context.Line("l_valueRegister = (asQWORD){0};", Context.VArg_Var(0));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		}
		else
		{
			check(false);
		}

		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CpyVtoR8)

IMPL_BYTECODE_BEGIN(asBC_CpyVtoG4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		void* GlobalVar = (void*)asBC_PTRARG(Context.BC);
		FString GlobalVarRef = Context.ReferenceGlobalVariable(GlobalVar);

		switch (Context.VArg_Type(0))
		{
		case FStaticJITContext::EVariableType::Byte:
		case FStaticJITContext::EVariableType::Word:
		case FStaticJITContext::EVariableType::DWord:
		{
			Context.Line("*(asDWORD*){0} = (asDWORD){1};", GlobalVarRef, Context.VArg_Var(0));
		}
		break;
		case FStaticJITContext::EVariableType::Float:
		{
			Context.Line("*(asDWORD*){0} = value_as<asDWORD>({1});", GlobalVarRef, Context.VArg_Var(0));
		}
		break;
		default:
			check(false);
		break;
		}

		return true;
	}
};
IMPL_BYTECODE_END(asBC_CpyVtoG4)

void ByteCode_CpyRToV(FStaticJITContext& Context)
{
	switch (Context.VArg_Type(0))
	{
	case FStaticJITContext::EVariableType::Byte:
	{
		if (Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::ByteRegister)
		{
			Context.Line("{0} = l_byteRegister;", Context.VArg_Var(0));
		}
		else
		{
			Context.MaterializeValueRegister();
			Context.Line("memcpy({0}, &l_valueRegister, 1);", Context.VArg_Address(0));
		}
	}
	break;
	case FStaticJITContext::EVariableType::Word:
	{
		if (Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::DWordRegister)
		{
			Context.Line("memcpy({0}, &l_dwordRegister, 2);", Context.VArg_Address(0));
		}
		else
		{
			Context.MaterializeValueRegister();
			Context.Line("memcpy({0}, &l_valueRegister, 2);", Context.VArg_Address(0));
		}
	}
	break;
	case FStaticJITContext::EVariableType::DWord:
	{
		if (Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::DWordRegister)
		{
			Context.Line("{0} = l_dwordRegister;", Context.VArg_Var(0));
		}
		else
		{
			Context.MaterializeValueRegister();
			Context.Line("memcpy({0}, &l_dwordRegister, 1);", Context.VArg_Address(0));
		}
	}
	break;
	case FStaticJITContext::EVariableType::QWord:
	{
		Context.MaterializeValueRegister();
		Context.Line("{0} = l_valueRegister;", Context.VArg_Var(0));
	}
	break;
	case FStaticJITContext::EVariableType::Pointer:
	{
		Context.MaterializeValueRegister();
		Context.Line("{0} = ({1})l_valueRegister;", Context.VArg_Var(0), Context.VArg_LiteralTypename(0));
	}
	break;
	case FStaticJITContext::EVariableType::Float:
	{
		if (Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::FloatRegister)
		{
			Context.Line("{0} = l_floatRegister;", Context.VArg_Var(0));
		}
		else if (Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::DWordRegister)
		{
			Context.Line("memcpy({0}, &l_dwordRegister, 4);", Context.VArg_Address(0));
		}
		else
		{
			Context.MaterializeValueRegister();
			Context.Line("memcpy({0}, &l_valueRegister, 4);", Context.VArg_Address(0));
		}
	}
	break;
	case FStaticJITContext::EVariableType::Double:
	{
		if (Context.ValueRegisterState == FStaticJITContext::EValueRegisterState::DoubleRegister)
		{
			Context.Line("{0} = l_doubleRegister;", Context.VArg_Var(0));
		}
		else
		{
			Context.MaterializeValueRegister();
			Context.Line("memcpy({0}, &l_valueRegister, 4);", Context.VArg_Address(0));
		}
	}
	break;
	default:
		check(false);
	break;
	}
}

IMPL_BYTECODE_BEGIN(asBC_CpyRtoV4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		ByteCode_CpyRToV(Context);
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CpyRtoV4)

IMPL_BYTECODE_BEGIN(asBC_CpyRtoV8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		ByteCode_CpyRToV(Context);
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CpyRtoV8)

IMPL_BYTECODE_BEGIN(asBC_CpyGtoV4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		void* GlobalVar = (void*)asBC_PTRARG(Context.BC);
		FString GlobalVarRef = Context.ReferenceGlobalVariable(GlobalVar);

		switch (Context.VArg_Type(0))
		{
		case FStaticJITContext::EVariableType::Byte:
			Context.Line("{0} = *(asBYTE*){1};", Context.VArg_Var(0), GlobalVarRef);
		break;
		case FStaticJITContext::EVariableType::Word:
		case FStaticJITContext::EVariableType::DWord:
			Context.Line("{0} = *(asDWORD*){1};", Context.VArg_Var(0), GlobalVarRef);
		break;
		case FStaticJITContext::EVariableType::Float:
			Context.Line("{0} = value_as<float>(*(asDWORD*){1});", Context.VArg_Var(0), GlobalVarRef);
		break;
		default:
			check(false);
		break;
		}
		return true;
	}
};
IMPL_BYTECODE_END(asBC_CpyGtoV4)

IMPL_BYTECODE_BEGIN(asBC_WRTV1)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Byte);

		// Value register could be pointing to a variable, so we need to materialize any variables we've pushed
		Context.MaterializeAllPushedVariables();

		Context.MaterializeValueRegister();
		Context.Line("memcpy((void*)l_valueRegister, (void*){0}, 1);", Context.VArg_Address(0));
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_WRTV1)

IMPL_BYTECODE_BEGIN(asBC_WRTV2)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Word);

		// Value register could be pointing to a variable, so we need to materialize any variables we've pushed
		Context.MaterializeAllPushedVariables();

		Context.MaterializeValueRegister();
		Context.Line("memcpy((void*)l_valueRegister, (void*){0}, 2);", Context.VArg_Address(0));
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_WRTV2)

IMPL_BYTECODE_BEGIN(asBC_WRTV4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord || Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);

		// Value register could be pointing to a variable, so we need to materialize any variables we've pushed
		Context.MaterializeAllPushedVariables();

		Context.MaterializeValueRegister();
		Context.Line("memcpy((void*)l_valueRegister, (void*){0}, 4);", Context.VArg_Address(0));
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_WRTV4)

IMPL_BYTECODE_BEGIN(asBC_WRTV8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord
			|| Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double
			|| Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer
		);

		// Value register could be pointing to a variable, so we need to materialize any variables we've pushed
		Context.MaterializeAllPushedVariables();

		Context.MaterializeValueRegister();
		Context.Line("memcpy((void*)l_valueRegister, (void*){0}, 8);", Context.VArg_Address(0));
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_WRTV8)

IMPL_BYTECODE_BEGIN(asBC_RDR1)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Byte);

		Context.MaterializeValueRegister();
		Context.Line("{0} = value_read<asBYTE>((void*)l_valueRegister);", Context.VArg_Var(0));
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_RDR1)

IMPL_BYTECODE_BEGIN(asBC_RDR2)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Word);

		Context.MaterializeValueRegister();
		Context.Line("{0} = value_read<asWORD>((void*)l_valueRegister);", Context.VArg_Var(0));
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_RDR2)

IMPL_BYTECODE_BEGIN(asBC_RDR4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		if(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord)
		{
			Context.Line("{0} = value_read<asDWORD>((void*)l_valueRegister);", Context.VArg_Var(0));
		}
		else if(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float)
		{
			Context.Line("{0} = value_read<float>((void*)l_valueRegister);", Context.VArg_Var(0));
		}
		else
		{
			check(false);
		}

		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_RDR4)

IMPL_BYTECODE_BEGIN(asBC_RDR8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		if(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord)
		{
			Context.Line("{0} = value_read<asQWORD>((void*)l_valueRegister);", Context.VArg_Var(0));
		}
		else if(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double)
		{
			Context.Line("{0} = value_read<double>((void*)l_valueRegister);", Context.VArg_Var(0));
		}
		else if(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer)
		{
			Context.Line("*(void**){0} = value_read<void*>((void*)l_valueRegister);", Context.VArg_Address(0));
		}
		else
		{
			check(false);
		}

		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_RDR8)

IMPL_BYTECODE_BEGIN(asBC_LDG)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		void* GlobalVar = (void*)asBC_PTRARG(Context.BC);
		Context.Line("l_valueRegister = (asQWORD){0};", Context.ReferenceGlobalVariable(GlobalVar));
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_LDG)

IMPL_BYTECODE_BEGIN(asBC_LDV)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.Line("l_valueRegister = (asQWORD){0};", Context.VArg_Address(0));
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_LDV)

IMPL_BYTECODE_BEGIN(asBC_PGA)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		void* GlobalVar = (void*)asBC_PTRARG(Context.BC);

		auto& Expr = Context.PushStatic(2);
		Expr.ValueType = FStaticJITContext::EVariableType::Pointer;
		Expr.Expression = Context.ReferenceGlobalVariable(GlobalVar);
		Expr.bGuaranteeNonNull = true;
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PGA)

IMPL_BYTECODE_BEGIN(asBC_CmpPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Pointer);

		// NB: Optimized this to return 0 or 1, no need to ever return the -1
		Context.Line("l_byteRegister = ((void*){0} == (void*){1}) ? 0 : 1;",
			Context.VArg_Var(0), Context.VArg_Var(1));
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CmpPtr)

IMPL_BYTECODE_BEGIN(asBC_CmpPtrNull)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);

		// NB: Optimized this to return 0 or 1, no need to ever return the -1
		Context.Line("l_byteRegister = ({0} == nullptr) ? 0 : 1;", Context.VArg_Var(0));
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CmpPtrNull)

IMPL_BYTECODE_BEGIN(asBC_ThrowException)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int32 Type = asBC_WORDARG0(Context.BC);
		if (Type == 0)
			Context.Line("SCRIPT_SWITCH_VALUE_INVALID_EXCEPTION();");
		else
			Context.Line("SCRIPT_UNKNOWN_EXCEPTION();");

		Context.ExceptionCleanupAndReturn(false, false);
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_ThrowException)

IMPL_BYTECODE_BEGIN(asBC_VAR)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		auto& Expr = Context.PushStatic(2);
		Expr.ValueType = FStaticJITContext::EVariableType::QWord;
		Expr.VarOffsetLiteral = asBC_SWORDARG0(Context.BC);
		Expr.bIsVarLiteral = true;
		Expr.Expression = FString::Printf(TEXT("%d"), Expr.VarOffsetLiteral);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_VAR)

IMPL_BYTECODE_BEGIN(asBC_iTOf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		Context.Line("{0} = ConvertPrimitiveValue<float,int>({1});", Context.VArg_Var(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_iTOf)

IMPL_BYTECODE_BEGIN(asBC_fTOi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		Context.Line("{0} = int({1});", Context.VArg_SignedVar(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_fTOi)

IMPL_BYTECODE_BEGIN(asBC_uTOf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		Context.Line("{0} = ConvertPrimitiveValue<float,asDWORD>({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_uTOf)

IMPL_BYTECODE_BEGIN(asBC_fTOu)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		Context.Line("{0} = asDWORD({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_fTOu)

IMPL_BYTECODE_BEGIN(asBC_sbTOi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Byte);
		Context.Line("{0} = int({1});", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_sbTOi)

IMPL_BYTECODE_BEGIN(asBC_swTOi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Word);
		Context.Line("{0} = int({1});", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_swTOi)

IMPL_BYTECODE_BEGIN(asBC_ubTOi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Byte);
		Context.Line("{0} = int({1});", Context.VArg_SignedVar(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ubTOi)

IMPL_BYTECODE_BEGIN(asBC_uwTOi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Word);
		Context.Line("{0} = int({1});", Context.VArg_SignedVar(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_uwTOi)

IMPL_BYTECODE_BEGIN(asBC_dTOi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		Context.Line("{0} = int({1});", Context.VArg_SignedVar(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_dTOi)

IMPL_BYTECODE_BEGIN(asBC_dTOu)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		Context.Line("{0} = int({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_dTOu)

IMPL_BYTECODE_BEGIN(asBC_dTOf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		Context.Line("{0} = float({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_dTOf)

IMPL_BYTECODE_BEGIN(asBC_iTOd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		Context.Line("{0} = ConvertPrimitiveValue<double,int>({1});", Context.VArg_Var(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_iTOd)

IMPL_BYTECODE_BEGIN(asBC_uTOd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		Context.Line("{0} = ConvertPrimitiveValue<double,asDWORD>({1});", Context.VArg_Var(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_uTOd)

IMPL_BYTECODE_BEGIN(asBC_fTOd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		Context.Line("{0} = double({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_fTOd)

IMPL_BYTECODE_BEGIN(asBC_ADDi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{0} = {1} + {2};", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.VArg_SignedVar(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ADDi)

IMPL_BYTECODE_BEGIN(asBC_SUBi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{0} = {1} - {2};", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.VArg_SignedVar(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SUBi)

IMPL_BYTECODE_BEGIN(asBC_MULi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{0} = {1} * {2};", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.VArg_SignedVar(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MULi)

IMPL_BYTECODE_BEGIN(asBC_DIVi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{");
		Context.Line("  int divider = {0};", Context.VArg_SignedVar(2));
		Context.Line("  int operand = {0};", Context.VArg_SignedVar(1));
		Context.Line("  if (divider == 0) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_DIV0_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_OVERFLOW_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("{0} = operand / divider;", Context.VArg_SignedVar(0));
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_DIVi)

IMPL_BYTECODE_BEGIN(asBC_MODi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{");
		Context.Line("  int divider = {0};", Context.VArg_SignedVar(2));
		Context.Line("  int operand = {0};", Context.VArg_SignedVar(1));
		Context.Line("  if (divider == 0) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_DIV0_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_OVERFLOW_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("{0} = operand % divider;", Context.VArg_SignedVar(0));
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MODi)

IMPL_BYTECODE_BEGIN(asBC_ADDf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = {1} + {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ADDf)

IMPL_BYTECODE_BEGIN(asBC_SUBf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = {1} - {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SUBf)

IMPL_BYTECODE_BEGIN(asBC_MULf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = {1} * {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MULf)

IMPL_BYTECODE_BEGIN(asBC_DIVf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// NB: We don't implement division by 0 errors here, no need for it
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = {1} / {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_DIVf)

IMPL_BYTECODE_BEGIN(asBC_MODf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// NB: We don't implement division by 0 errors here, no need for it
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = std::fmodf({1}, {2});", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MODf)

IMPL_BYTECODE_BEGIN(asBC_ADDd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Double);

		Context.Line("{0} = {1} + {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ADDd)

IMPL_BYTECODE_BEGIN(asBC_SUBd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Double);

		Context.Line("{0} = {1} - {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SUBd)

IMPL_BYTECODE_BEGIN(asBC_MULd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Double);

		Context.Line("{0} = {1} * {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MULd)

IMPL_BYTECODE_BEGIN(asBC_DIVd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// NB: We don't implement division by 0 errors here, no need for it
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Double);

		Context.Line("{0} = {1} / {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_DIVd)

IMPL_BYTECODE_BEGIN(asBC_MODd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// NB: We don't implement division by 0 errors here, no need for it
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Double);

		Context.Line("{0} = std::fmod({1}, {2});", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MODd)

IMPL_BYTECODE_BEGIN(asBC_ADDIi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{0} = {1} + value_as<int>({2});", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.BCConstant_DWord(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ADDIi)

IMPL_BYTECODE_BEGIN(asBC_SUBIi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{0} = {1} - value_as<int>({2});", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.BCConstant_DWord(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SUBIi)

IMPL_BYTECODE_BEGIN(asBC_MULIi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{0} = {1} * value_as<int>({2});", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.BCConstant_DWord(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MULIi)

IMPL_BYTECODE_BEGIN(asBC_ADDIf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = {1} + value_as<float>({2});", Context.VArg_Var(0), Context.VArg_Var(1), Context.BCConstant_DWord(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ADDIf)

IMPL_BYTECODE_BEGIN(asBC_SUBIf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = {1} - value_as<float>({2});", Context.VArg_Var(0), Context.VArg_Var(1), Context.BCConstant_DWord(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SUBIf)

IMPL_BYTECODE_BEGIN(asBC_MULIf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = {1} * value_as<float>({2});", Context.VArg_Var(0), Context.VArg_Var(1), Context.BCConstant_DWord(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MULIf)

IMPL_BYTECODE_BEGIN(asBC_SetG4)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.Line("*(asDWORD*){0} = (asDWORD){1};",
			Context.ReferenceGlobalVariable((void*)asBC_PTRARG(Context.BC)),
			FString::Printf(TEXT("0x%xu"), asBC_DWORDARG(Context.BC + 2)));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SetG4)

IMPL_BYTECODE_BEGIN(asBC_ChkRefS)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.Line("{");
		Context.Line("  asPWORD* a = (asPWORD*){0};", Context.StackValue_At(0, 2));
		Context.Line("  if (*a == 0) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
		Context.Line("  }");
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ChkRefS)

IMPL_BYTECODE_BEGIN(asBC_ChkNullV)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);

		Context.Line("if ({0} == nullptr) [[unlikely]]", Context.VArg_Var(0));
		Context.Line("{");
		Context.DebugLineNumber();
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ChkNullV)

IMPL_BYTECODE_BEGIN(asBC_CALLINTF)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int FunctionId = (int)asBC_DWORDARG(Context.BC);
		auto* ScriptFunction = (asCScriptFunction*)FAngelscriptEngine::Get().Engine->GetFunctionById(FunctionId);

		FCallScriptFunction Call(ScriptFunction, FCallScriptFunction::ELookupType::IdArg);
		Call.MakeCall(Context);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		int FunctionId = (int)asBC_DWORDARG(Context.BC);
		auto* ScriptFunction = (asCScriptFunction*)FAngelscriptEngine::Get().Engine->GetFunctionById(FunctionId);
		return -ScriptFunction->totalSpaceBeforeFunction;
	};
};
IMPL_BYTECODE_END(asBC_CALLINTF)

IMPL_BYTECODE_BEGIN(asBC_iTOb)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Byte);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		Context.Line("{0} = value_as<asBYTE>({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_iTOb)

IMPL_BYTECODE_BEGIN(asBC_iTOw)
{
	bool Implement(FStaticJITContext& Context) const override
{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Word);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		Context.Line("{0} = value_as<asWORD>({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_iTOw)

IMPL_BYTECODE_BEGIN(asBC_SetV1)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		//NB: Identical to SetV4, Deprecated
		ByteCode_SetVArg_Constant(Context);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SetV1)

IMPL_BYTECODE_BEGIN(asBC_SetV2)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		//NB: Identical to SetV4, Deprecated
		ByteCode_SetVArg_Constant(Context);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SetV2)

IMPL_BYTECODE_BEGIN(asBC_Cast)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(false); // Unsupported, we use a different way of casting
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_objectRegister;
	};
};
IMPL_BYTECODE_END(asBC_Cast)

IMPL_BYTECODE_BEGIN(asBC_i64TOi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		Context.Line("{0} = value_as<int>({1});", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_i64TOi)

IMPL_BYTECODE_BEGIN(asBC_uTOi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		Context.Line("{0} = int64({1});", Context.VArg_SignedVar(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_uTOi64)

IMPL_BYTECODE_BEGIN(asBC_iTOi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		Context.Line("{0} = int64({1});", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_iTOi64)

IMPL_BYTECODE_BEGIN(asBC_fTOi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		Context.Line("{0} = int64({1});", Context.VArg_SignedVar(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_fTOi64)

IMPL_BYTECODE_BEGIN(asBC_dTOi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		Context.Line("{0} = int64({1});", Context.VArg_SignedVar(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_dTOi64)

IMPL_BYTECODE_BEGIN(asBC_fTOu64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		Context.Line("{0} = uint64({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_fTOu64)

IMPL_BYTECODE_BEGIN(asBC_dTOu64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		Context.Line("{0} = uint64({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_dTOu64)

IMPL_BYTECODE_BEGIN(asBC_i64TOf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		Context.Line("{0} = ConvertPrimitiveValue<float,int64>({1});", Context.VArg_Var(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_i64TOf)

IMPL_BYTECODE_BEGIN(asBC_u64TOf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		Context.Line("{0} = ConvertPrimitiveValue<float,uint64>({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_u64TOf)

IMPL_BYTECODE_BEGIN(asBC_i64TOd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		Context.Line("{0} = ConvertPrimitiveValue<double,int64>({1});", Context.VArg_Var(0), Context.VArg_SignedVar(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_i64TOd)

IMPL_BYTECODE_BEGIN(asBC_u64TOd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		Context.Line("{0} = ConvertPrimitiveValue<double,uint64>({1});", Context.VArg_Var(0), Context.VArg_Var(1));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_u64TOd)

IMPL_BYTECODE_BEGIN(asBC_NEGi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		Context.Line("{0} = -{0};", Context.VArg_SignedVar(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_NEGi64)

IMPL_BYTECODE_BEGIN(asBC_INCi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("++(**(asQWORD**)&l_valueRegister);");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_INCi64)

IMPL_BYTECODE_BEGIN(asBC_DECi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeValueRegister();
		Context.Line("--(**(asQWORD**)&l_valueRegister);");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_DECi64)

IMPL_BYTECODE_BEGIN(asBC_BNOT64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		Context.Line("{0} = ~{0};", Context.VArg_Var(0));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BNOT64)

IMPL_BYTECODE_BEGIN(asBC_ADDi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} + {2};", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.VArg_SignedVar(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ADDi64)

IMPL_BYTECODE_BEGIN(asBC_SUBi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} - {2};", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.VArg_SignedVar(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SUBi64)

IMPL_BYTECODE_BEGIN(asBC_MULi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} * {2};", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1), Context.VArg_SignedVar(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MULi64)

IMPL_BYTECODE_BEGIN(asBC_DIVi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);
		
		Context.Line("{");
		Context.Line("  asINT64 divider = {0};", Context.VArg_SignedVar(2));
		Context.Line("  asINT64 operand = {0};", Context.VArg_SignedVar(1));
		Context.Line("  if (divider == 0) [[unlikely]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_DIV0_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("  else if (divider == -1 && operand == (asINT64(1)<<63)) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_OVERFLOW_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("{0} = operand / divider;", Context.VArg_SignedVar(0));
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_DIVi64)

IMPL_BYTECODE_BEGIN(asBC_MODi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);
		
		Context.Line("{");
		Context.Line("  asINT64 divider = {0};", Context.VArg_SignedVar(2));
		Context.Line("  asINT64 operand = {0};", Context.VArg_SignedVar(1));
		Context.Line("  if (divider == 0) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_DIV0_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("  else if (divider == -1 && operand == (asINT64(1)<<63)) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_OVERFLOW_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("{0} = operand % divider;", Context.VArg_SignedVar(0));
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MODi64)

IMPL_BYTECODE_BEGIN(asBC_BAND64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} & {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BAND64)

IMPL_BYTECODE_BEGIN(asBC_BOR64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} | {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BOR64)

IMPL_BYTECODE_BEGIN(asBC_BXOR64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} ^ {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BXOR64)

IMPL_BYTECODE_BEGIN(asBC_BSLL64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} << {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BSLL64)

IMPL_BYTECODE_BEGIN(asBC_BSRL64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} >> {2};", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BSRL64)

IMPL_BYTECODE_BEGIN(asBC_BSRA64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{0} = {1} >> {2};", Context.VArg_Var(0), Context.VArg_SignedVar(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_BSRA64)

IMPL_BYTECODE_BEGIN(asBC_CMPi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));

		Context.Line("  if ({0} == {1})     l_byteRegister = 0;", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1));
		Context.Line("  else if ({0} < {1}) l_byteRegister = -1;", Context.VArg_SignedVar(0), Context.VArg_SignedVar(1));
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPi64)

IMPL_BYTECODE_BEGIN(asBC_CMPu64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_IsNumeric(0));
		check(Context.VArg_IsNumeric(1));

		Context.Line("  if ({0} == {1})     l_byteRegister = 0;", Context.VArg_Var(0), Context.VArg_Var(1));
		Context.Line("  else if ({0} < {1}) l_byteRegister = -1;", Context.VArg_Var(0), Context.VArg_Var(1));
		Context.Line("  else                l_byteRegister = 1;");
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_CMPu64)

IMPL_BYTECODE_BEGIN(asBC_ChkNullS)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		int32 Offset = asBC_WORDARG0(Context.BC);
		if (!Context.IsStackValueNonNull(Offset))
		{
			Context.Line("{");
			Context.Line("  asPWORD a = (asPWORD){0};", Context.StackValue_At(Offset, 2));
			Context.Line("  if (a == 0) [[unlikely]]");
			Context.Line("  {");
			Context.DebugLineNumber();
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("  }");
			Context.Line("}");
		}
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ChkNullS)

IMPL_BYTECODE_BEGIN(asBC_ClrHi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		switch (Context.ValueRegisterState)
		{
		case FStaticJITContext::EValueRegisterState::ByteRegister:
			// Don't need to do anything, value register is already a byte
		break;
		case FStaticJITContext::EValueRegisterState::DWordRegister:
			Context.Line("l_byteRegister = (asBYTE&)l_dwordRegister;");
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		break;
		case FStaticJITContext::EValueRegisterState::FloatRegister:
			Context.Line("memcpy((void*)&l_byteRegister, (void*)&l_floatRegister, 1)");
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		break;
		case FStaticJITContext:: EValueRegisterState::DoubleRegister:
			Context.Line("memcpy((void*)&l_byteRegister, (void*)&l_doubleRegister, 1)");
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		break;
		case FStaticJITContext::EValueRegisterState::ValueRegister:
			Context.Line("l_byteRegister = (asBYTE&)l_valueRegister;");
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ByteRegister;
		break;
		default:
			check(false);
		break;
		}

		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_ClrHi)

IMPL_BYTECODE_BEGIN(asBC_CallPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// Used for function pointers, we don't really do this at all,
		// would be a pain to implement, so whatever, revert to the VM.
		check(false);
		return false;
	}
};
IMPL_BYTECODE_END(asBC_CallPtr)

IMPL_BYTECODE_BEGIN(asBC_FuncPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(false);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_FuncPtr)

IMPL_BYTECODE_BEGIN(asBC_LoadThisR)
{
	FString OffsetExpr(FStaticJITContext& Context) const
	{
		int32 TypeId = asBC_INTARG(Context.BC);
		int32 Offset = asBC_SWORDARG0(Context.BC);

		// Store a reference to the unknown property so we can look it up in the new place
		return Context.ReferencePropertyOffset(Offset, TypeId);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		if (Context.ScriptFunction->objectType != nullptr)
		{
			Context.Line("l_valueRegister = ((asQWORD)l_This) + {0};",
				OffsetExpr(Context));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		}
		else
		{
			Context.Line("l_valueRegister = ((asQWORD){0}) + {1};",
				Context.VVar(0),
				OffsetExpr(Context));
			Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		}
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_LoadThisR)

IMPL_BYTECODE_BEGIN(asBC_PshV8)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.PushVArgValue(2, 0);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PshV8)

IMPL_BYTECODE_BEGIN(asBC_DIVu)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{");
		Context.Line("  asUINT divider = {0};", Context.VArg_Var(2));
		Context.Line("  asUINT operand = {0};", Context.VArg_Var(1));
		Context.Line("  if (divider == 0) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_DIV0_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("{0} = operand / divider;", Context.VArg_Var(0));
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_DIVu)

IMPL_BYTECODE_BEGIN(asBC_MODu)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::DWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::DWord);

		Context.Line("{");
		Context.Line("  asUINT divider = {0};", Context.VArg_Var(2));
		Context.Line("  asUINT operand = {0};", Context.VArg_Var(1));
		Context.Line("  if (divider == 0) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_DIV0_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("{0} = operand % divider;", Context.VArg_Var(0));
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MODu)

IMPL_BYTECODE_BEGIN(asBC_DIVu64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{");
		Context.Line("  asQWORD divider = {0};", Context.VArg_Var(2));
		Context.Line("  asQWORD operand = {0};", Context.VArg_Var(1));
		Context.Line("  if (divider == 0) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_DIV0_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("{0} = operand / divider;", Context.VArg_Var(0));
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_DIVu64)

IMPL_BYTECODE_BEGIN(asBC_MODu64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::QWord);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::QWord);

		Context.Line("{");
		Context.Line("  asQWORD divider = {0};", Context.VArg_Var(2));
		Context.Line("  asQWORD operand = {0};", Context.VArg_Var(1));
		Context.Line("  if (divider == 0) [[unlikely]]");
		Context.Line("  {");
		Context.DebugLineNumber();
		Context.Line("     SCRIPT_DIV0_EXCEPTION();");
		Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/false);
		Context.Line("  }");
		Context.Line("{0} = operand % divider;", Context.VArg_Var(0));
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_MODu64)

IMPL_BYTECODE_BEGIN(asBC_LoadRObjR)
{
	FString OffsetExpr(FStaticJITContext& Context) const
	{
		int32 TypeId = *(int*)(Context.BC+2);
		int32 Offset = asBC_SWORDARG1(Context.BC);

		// Store a reference to the unknown property so we can look it up in the new place
		return Context.ReferencePropertyOffset(Offset, TypeId);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);

		if (!Context.VArg_ContainsNonNullPtr(0))
		{
			Context.Line("if ({0} == nullptr) [[unlikely]]", Context.VArg_Var(0));
			Context.Line("{");
			Context.DebugLineNumber();
			Context.ExceptionCleanupAndReturn(/*bAfterCurrentOp=*/false, /*bSetNullptrException=*/true);
			Context.Line("}");
		}

		Context.Line("l_valueRegister = (asQWORD)({0}) + {1};", Context.VArg_Var(0), OffsetExpr(Context));
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_LoadRObjR)

IMPL_BYTECODE_BEGIN(asBC_LoadVObjR)
{
	FString OffsetExpr(FStaticJITContext& Context) const
	{
		int32 TypeId = *(int*)(Context.BC+2);
		int32 Offset = asBC_SWORDARG1(Context.BC);

		// Store a reference to the unknown property so we can look it up in the new place
		return Context.ReferencePropertyOffset(Offset, TypeId);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Object);
		Context.Line("l_valueRegister = (asQWORD)({0}) + {1};", Context.VArg_Address(0), OffsetExpr(Context));
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::ValueRegister;
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_LoadVObjR)

IMPL_BYTECODE_BEGIN(asBC_RefCpyV)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		asCObjectType* objType = (asCObjectType*)asBC_PTRARG(Context.BC);
		const bool bRawScriptReference =
			(objType->flags & asOBJ_SCRIPT_OBJECT) != 0
			&& objType->GetUserData() == nullptr;

		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Pointer);
		Context.Line("{");
		Context.Line("  asPWORD* d = (asPWORD*){0};", Context.VArg_Address(0));
		Context.Line("  asPWORD s = (asPWORD){0};", Context.StackValue_At(0, 2));
		Context.Line("  asCObjectType* objType = (asCObjectType*){0};", Context.ReferenceTypeInfo(objType));
		Context.Line("  if (*d != s)");
		Context.Line("  {");
		if (bRawScriptReference)
		{
			Context.Line("    if (*d != 0)");
			Context.Line("      SCRIPT_ENGINE->ReleaseRawScriptObject((void*)*d, objType);");
			Context.Line("    if (s != 0)");
			Context.Line("      SCRIPT_ENGINE->AddRawScriptObjectReference((void*)s, objType);");
		}
		else
		{
			Context.Line("    if ((objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE)) == 0)");
			Context.Line("    {");
			Context.Line("      if (*d != 0 && objType->beh.release != 0)");
			Context.Line("        SCRIPT_ENGINE->CallObjectMethod((void*)*d, objType->beh.release);");
			Context.Line("      if (s != 0 && objType->beh.addref != 0)");
			Context.Line("        SCRIPT_ENGINE->CallObjectMethod((void*)s, objType->beh.addref);");
			Context.Line("    }");
		}
		Context.Line("  }");
		Context.Line("  if (*d != s)");
		Context.Line("    *d = s;");
		Context.Line("}");
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return 0;
	};
};
IMPL_BYTECODE_END(asBC_RefCpyV)

IMPL_BYTECODE_BEGIN(asBC_JLowZ)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		Context.AddLabel(asBC_INTARG(Context.BC) + 2);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeWholeStack();
		switch (Context.ValueRegisterState)
		{
		case FStaticJITContext::EValueRegisterState::ByteRegister:
			Context.Line("if(l_byteRegister == 0) {");
		break;
		case FStaticJITContext::EValueRegisterState::DWordRegister:
			Context.Line("if((asBYTE&)l_dwordRegister == 0) {");
		break;
		case FStaticJITContext::EValueRegisterState::FloatRegister:
		case FStaticJITContext:: EValueRegisterState::DoubleRegister:
		case FStaticJITContext::EValueRegisterState::ValueRegister:
			Context.Line("if((asBYTE&)l_valueRegister == 0) {");
		break;
		default:
			check(false);
		break;
		}

		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		Context.Line("}");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_JLowZ)

IMPL_BYTECODE_BEGIN(asBC_JLowNZ)
{
	void PrePass(FStaticJITContext& Context) const override
	{
		Context.AddLabel(asBC_INTARG(Context.BC) + 2);
	}

	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeWholeStack();
		switch (Context.ValueRegisterState)
		{
		case FStaticJITContext::EValueRegisterState::ByteRegister:
			Context.Line("if(l_byteRegister != 0) {");
		break;
		case FStaticJITContext::EValueRegisterState::DWordRegister:
			Context.Line("if((asBYTE&)l_dwordRegister != 0) {");
		break;
		case FStaticJITContext::EValueRegisterState::FloatRegister:
		case FStaticJITContext:: EValueRegisterState::DoubleRegister:
		case FStaticJITContext::EValueRegisterState::ValueRegister:
			Context.Line("if((asBYTE&)l_valueRegister != 0) {");
		break;
		default:
			check(false);
		break;
		}
		Context.ValueRegisterState = FStaticJITContext::EValueRegisterState::Indeterminate;
		Context.Jump(Context.GetLabelInstruction(asBC_INTARG(Context.BC) + 2));
		Context.Line("}");
		return true;
	}

	virtual int32 GetUsageFlags() const override
	{
		return BU_l_valueRegister;
	};
};
IMPL_BYTECODE_END(asBC_JLowNZ)

IMPL_BYTECODE_BEGIN(asBC_AllocMem)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		Context.MaterializeAllPushedVariables();

		Context.Line("{");
		Context.Line("  asUINT size = (asUINT){0};", Context.BCConstant_DWord());
		Context.Line("  asBYTE** var = (asBYTE**){0};", Context.VArg_Address(0));
		Context.Line("  *var = FMemory::Malloc(size);");
		Context.Line("  FMemory::Memzero(*var, size);");
		Context.Line("}");
		return true;
	}
};
IMPL_BYTECODE_END(asBC_AllocMem)

IMPL_BYTECODE_BEGIN(asBC_SetListSize)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(false);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SetListSize)

IMPL_BYTECODE_BEGIN(asBC_PshListElmnt)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(false);
		return true;
	}

	virtual int32 GetStackSizeChange(FStaticJITContext& Context) const
	{
		return +2;
	};
};
IMPL_BYTECODE_END(asBC_PshListElmnt)

IMPL_BYTECODE_BEGIN(asBC_SetListType)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		check(false);
		return true;
	}
};
IMPL_BYTECODE_END(asBC_SetListType)

IMPL_BYTECODE_BEGIN(asBC_POWi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// Wtf? Why do we even let you Pow() integers?
		check(false);
		return false;
	}
};
IMPL_BYTECODE_END(asBC_POWi)

IMPL_BYTECODE_BEGIN(asBC_POWu)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// Wtf? Why do we even let you Pow() integers?
		check(false);
		return false;
	}
};
IMPL_BYTECODE_END(asBC_POWu)

IMPL_BYTECODE_BEGIN(asBC_POWf)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// NB: We skip checking for HUGE_VAL, no point
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Float);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Float);

		Context.Line("{0} = Math::Pow({1}, {2});", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return true;
	}
};
IMPL_BYTECODE_END(asBC_POWf)

IMPL_BYTECODE_BEGIN(asBC_POWd)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// NB: We skip checking for HUGE_VAL, no point
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Double);

		Context.Line("{0} = Math::Pow({1}, {2});", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_Var(2));
		return false;
	}
};
IMPL_BYTECODE_END(asBC_POWd)

IMPL_BYTECODE_BEGIN(asBC_POWdi)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// NB: We skip checking for HUGE_VAL, no point
		check(Context.VArg_Type(0) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(1) == FStaticJITContext::EVariableType::Double);
		check(Context.VArg_Type(2) == FStaticJITContext::EVariableType::Double);

		Context.Line("{0} = Math::Pow({1}, (double){2});", Context.VArg_Var(0), Context.VArg_Var(1), Context.VArg_SignedVar(2));
		return false;
	}
};
IMPL_BYTECODE_END(asBC_POWdi)

IMPL_BYTECODE_BEGIN(asBC_POWi64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// Don't allow integer Pow()
		check(false);
		return false;
	}
};
IMPL_BYTECODE_END(asBC_POWi64)

IMPL_BYTECODE_BEGIN(asBC_POWu64)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// Don't allow integer Pow()
		check(false);
		return false;
	}
};
IMPL_BYTECODE_END(asBC_POWu64)

IMPL_BYTECODE_BEGIN(asBC_ResolveObjectPtr)
{
	bool Implement(FStaticJITContext& Context) const override
	{
		// Object pointers never need to be resolved in cooked games, it's an editor thing
		return true;
	}
};
IMPL_BYTECODE_END(asBC_ResolveObjectPtr)

FString FAngelscriptBytecode::GetInstrDebugString(asDWORD* BC)
{
	asEBCInstr Instr = (asEBCInstr)*(asBYTE*)BC;
	switch (Instr)
	{
		case asBC_ADDSi:
			return FString::Printf(TEXT("ADDSi +%d"), asBC_SWORDARG0(BC));
		case asBC_LoadThisR:
			return FString::Printf(TEXT("LoadThisR +%d"), asBC_SWORDARG0(BC));
		case asBC_PshGPtr:
		{
			void* GlobalPtr = (void*)asBC_PTRARG(BC);
			auto* Engine = (asCScriptEngine*)FAngelscriptEngine::Get().Engine;

			if (asCGlobalProperty** PropPtr = Engine->varAddressMap.Find(GlobalPtr))
			{
				return FString::Printf(TEXT("PshGPtr %s::%s"),
					ANSI_TO_TCHAR((*PropPtr)->nameSpace->name.AddressOf()),
					ANSI_TO_TCHAR((*PropPtr)->name.AddressOf()));
			}
		}
		case asBC_TYPEID:
		case asBC_OBJTYPE:
		case asBC_LoadRObjR:
		case asBC_LoadVObjR:
		case asBC_COPY:
		case asBC_ALLOC:
		case asBC_Cast:
		case asBC_CALL:
		case asBC_CALLINTF:
		case asBC_CALLBND:
			return ANSI_TO_TCHAR(asBCInfo[Instr].name);
	}

	FString Out = FString::Printf(TEXT("%s "), ANSI_TO_TCHAR(asBCInfo[Instr].name));

	switch( asBCInfo[Instr].type )
	{
	case asBCTYPE_W_ARG:
		Out += FString::Printf(TEXT("%d"), asBC_SWORDARG0(BC));
	break;

	case asBCTYPE_wW_ARG:
	case asBCTYPE_rW_ARG:
		Out += FString::Printf(TEXT("v%d"), asBC_SWORDARG0(BC));
	break;

	case asBCTYPE_wW_rW_ARG:
	case asBCTYPE_rW_rW_ARG:
		Out += FString::Printf(TEXT("v%d, v%d"), asBC_SWORDARG0(BC), asBC_SWORDARG1(BC));
	break;

	case asBCTYPE_wW_W_ARG:
		Out += FString::Printf(TEXT("v%d, %d"), asBC_SWORDARG0(BC), asBC_SWORDARG1(BC));
	break;

	case asBCTYPE_wW_rW_DW_ARG:
	case asBCTYPE_rW_W_DW_ARG:
		switch(Instr)
		{
		case asBC_ADDIf:
		case asBC_SUBIf:
		case asBC_MULIf:
			Out += FString::Printf(TEXT("v%d, v%d, %f"), (int)asBC_SWORDARG0(BC), (int)asBC_SWORDARG1(BC), asBC_FLOATARG(BC+1));
			break;
		default:
			Out += FString::Printf(TEXT("v%d, v%d, %d"), (int)asBC_SWORDARG0(BC), (int)asBC_SWORDARG1(BC), asBC_INTARG(BC+1));
			break;
		}
		break;

	case asBCTYPE_DW_ARG:
		Out += FString::Printf(TEXT("%d"), asBC_INTARG(BC));
	break;

	case asBCTYPE_QW_ARG:
		Out += TEXT("*");
	break;

	case asBCTYPE_wW_QW_ARG:
	case asBCTYPE_rW_QW_ARG:
		Out += FString::Printf(TEXT("v%d, *"), asBC_SWORDARG0(BC));
	break;

	case asBCTYPE_DW_DW_ARG:
		Out += FString::Printf(TEXT("%d, %d"), asBC_DWORDARG(BC), asBC_DWORDARG(BC+1));
	break;

	case asBCTYPE_rW_DW_DW_ARG:
		Out += FString::Printf(TEXT("v%d, %d, %d"), asBC_SWORDARG0(BC), asBC_DWORDARG(BC), asBC_DWORDARG(BC+1));
	break;

	case asBCTYPE_QW_DW_ARG:
		Out += FString::Printf(TEXT("*, %d"), asBC_DWORDARG(BC+AS_PTR_SIZE));
	break;

	case asBCTYPE_rW_DW_ARG:
	case asBCTYPE_wW_DW_ARG:
	case asBCTYPE_W_DW_ARG:
		Out += FString::Printf(TEXT("v%d, %d"), asBC_SWORDARG0(BC), asBC_DWORDARG(BC));
		break;

	case asBCTYPE_wW_rW_rW_ARG:
		Out += FString::Printf(TEXT("v%d, v%d, v%d"), asBC_SWORDARG0(BC), asBC_SWORDARG1(BC), asBC_SWORDARG2(BC));
	break;
	}

	Out.TrimEndInline();
	return Out;
}

#endif
