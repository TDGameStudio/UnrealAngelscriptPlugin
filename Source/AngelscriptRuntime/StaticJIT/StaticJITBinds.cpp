#include "StaticJIT/StaticJITBinds.h"
#include "StaticJIT/AngelscriptBytecodes.h"
#if AS_CAN_GENERATE_JIT
#include "ClassGenerator/ASStruct.h"
#include "Core/AngelscriptBindDatabase.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "AngelscriptEngine.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "Misc/App.h"
#include "UObject/Package.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptfunction.h"
//#include "as_scriptengine.h"
//#include "as_callfunc.h"
//#include "as_objecttype.h"
#include "source/as_scriptfunction.h"
#include "source/as_scriptengine.h"
#include "source/as_callfunc.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

#include "HAL/FileManager.h"
#include <atomic>

namespace
{
	std::atomic<int32> GNativeFormCount{0};
	uint8 GNativeFormUserDataKey = 0;

	asPWORD GetNativeFormUserDataType()
	{
		return reinterpret_cast<asPWORD>(&GNativeFormUserDataKey);
	}

#if WITH_DEV_AUTOMATION_TESTS
	FString MakeNativeFormDebugString(const ANSICHAR* Value)
	{
		return Value != nullptr ? FString(ANSI_TO_TCHAR(Value)) : FString();
	}
#endif

	void AddNativeForm(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, FScriptFunctionNativeForm* NativeForm)
	{
		if (ScriptFunction == nullptr || NativeForm == nullptr || !Engine.bCollectStaticJITCompatibilityBinds)
		{
			delete NativeForm;
			return;
		}

		FAngelscriptNativeFormState* State = Engine.GetNativeFormState();
		check(State != nullptr);
		Engine.GetScriptEngine()->SetUserData(State, GetNativeFormUserDataType());
		if (FScriptFunctionNativeForm** Existing = State->Forms.Find(ScriptFunction))
		{
			delete *Existing;
			*Existing = NativeForm;
		}
		else
		{
			State->Forms.Add(ScriptFunction, NativeForm);
			GNativeFormCount.fetch_add(1, std::memory_order_relaxed);
		}
	}

}

FAngelscriptNativeFormState::~FAngelscriptNativeFormState()
{
	GNativeFormCount.fetch_sub(Forms.Num(), std::memory_order_relaxed);
	for (const TPair<asIScriptFunction*, FScriptFunctionNativeForm*>& Pair : Forms)
	{
		delete Pair.Value;
	}
}

FScriptFunctionNativeForm* FScriptFunctionNativeForm::GetNativeForm(class asIScriptFunction* InScriptFunction)
{
	auto* ScriptFunction = (asCScriptFunction*)InScriptFunction;
	auto* ObjectType = ScriptFunction->objectType;
	if (ObjectType != nullptr && ObjectType->templateBaseType != nullptr)
	{
		// Look up the template base type's function for the native call, since that's the
		// one we will have registered the native form for.
		// Note that this has the assumption that the methods are always in the same order!
		if (ScriptFunction->GetId() == ObjectType->beh.construct)
			return GetNativeForm(ObjectType->engine->scriptFunctions[ObjectType->templateBaseType->beh.construct]);
		if (ScriptFunction->GetId() == ObjectType->beh.destruct)
			return GetNativeForm(ObjectType->engine->scriptFunctions[ObjectType->templateBaseType->beh.destruct]);

		if (ScriptFunction->traits.GetTrait(asTRAIT_CONSTRUCTOR))
		{
			int32 ConstructorIndex = ObjectType->beh.constructors.IndexOf(ScriptFunction->GetId());
			if (ConstructorIndex != -1)
				return GetNativeForm(ObjectType->engine->scriptFunctions[ObjectType->templateBaseType->beh.constructors[ConstructorIndex]]);
		}

		int32 MethodIndex = ObjectType->methods.IndexOf(ScriptFunction->GetId());
		if (MethodIndex != -1)
			return GetNativeForm(ObjectType->engine->scriptFunctions[ObjectType->templateBaseType->methods[MethodIndex]]);
	}

	FAngelscriptNativeFormState* State = static_cast<FAngelscriptNativeFormState*>(ScriptFunction->engine->GetUserData(GetNativeFormUserDataType()));
	if (State == nullptr)
	{
		return nullptr;
	}
	if (FScriptFunctionNativeForm** Found = State->Forms.Find(ScriptFunction))
	{
		return *Found;
	}
	return nullptr;
}

int32 FScriptFunctionNativeForm::NumNativeForms()
{
	return GNativeFormCount.load(std::memory_order_relaxed);
}

int32 FNativeFunctionContext::AppendArgumentsTo(FString& CallCode, int32 ArgumentStart)
{
	for (const FString& Arg : ArgumentValues)
	{
		if (ArgumentStart != 0)
			CallCode += TEXT(", ");
		CallCode += Arg;
		ArgumentStart += 1;
	}

	return ArgumentValues.Num();
}

struct FScriptNativeConstructor : public FScriptFunctionNativeForm
{
	const ANSICHAR* Name;
	bool bTrivial;
	const ANSICHAR* CustomForm;
	
	FScriptNativeConstructor(const ANSICHAR* InName, bool InTrivial, const ANSICHAR* InCustomForm)
		: Name(InName), bTrivial(InTrivial), CustomForm(InCustomForm)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::Constructor;
		Info.Name = MakeNativeFormDebugString(Name);
		Info.CustomForm = MakeNativeFormDebugString(CustomForm);
		Info.bTrivial = bTrivial;
		return Info;
	}
#endif

	bool ShouldIgnoreObjectArgument() const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return bTrivial && (Method != EScriptFunctionCallMethod::PointerCall);
	}

	FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const override
	{
		check(Context.ObjectAddress.Len() != 0);
		check(Context.ReturnValueAddress.Len() == 0);

		FNativeFunctionCall Call;
		Call.CallCode = FString::Printf(TEXT("new (%s) %s("), *Context.ObjectAddress, ANSI_TO_TCHAR(Name));
		Context.AppendArgumentsTo(Call.CallCode);
		if (CustomForm != nullptr)
			Call.CallCode += ANSI_TO_TCHAR(CustomForm);
		Call.CallCode += TEXT(")");

		return Call;
	}
};

void FScriptFunctionNativeForm::BindNativeConstructor(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial, const ANSICHAR* CustomForm)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeConstructor(Name, bTrivial, CustomForm));
}

struct FScriptNativeDestructor : public FScriptFunctionNativeForm
{
	const ANSICHAR* Name;
	bool bTrivial;
	
	FScriptNativeDestructor(const ANSICHAR* InName, bool InTrivial)
		: Name(InName), bTrivial(InTrivial)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::Destructor;
		Info.Name = MakeNativeFormDebugString(Name);
		Info.bTrivial = bTrivial;
		return Info;
	}
#endif

	bool ShouldIgnoreObjectArgument() const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return bTrivial && (Method != EScriptFunctionCallMethod::PointerCall);
	}

	FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const override
	{
		check(Context.ObjectAddress.Len() != 0);
		check(Context.ReturnValueAddress.Len() == 0);
		check(Context.ArgumentValues.Num() == 0);

		FNativeFunctionCall Call;
		Call.CallCode = FString::Printf(TEXT("((%s*)%s)->~%s()"),
			ANSI_TO_TCHAR(Name),
			*Context.ObjectAddress,
			ANSI_TO_TCHAR(Name)
		);

		return Call;
	}
};

void FScriptFunctionNativeForm::BindNativeDestructor(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeDestructor(Name, bTrivial));
}

struct FScriptNativeAssignment : public FScriptFunctionNativeForm
{
	const ANSICHAR* Name;
	bool bTrivial;
	
	FScriptNativeAssignment(const ANSICHAR* InName, bool InTrivial)
		: Name(InName), bTrivial(InTrivial)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::Assignment;
		Info.Name = MakeNativeFormDebugString(Name);
		Info.bTrivial = bTrivial;
		return Info;
	}
#endif

	bool ShouldIgnoreObjectArgument() const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return bTrivial && (Method != EScriptFunctionCallMethod::PointerCall);
	}

	FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const override
	{
		check(Context.ObjectAddress.Len() != 0);
		check(Context.ArgumentValues.Num() == 1);

		FNativeFunctionCall Call;
		Call.CallCode = FString::Printf(TEXT("(*((%s*)%s) = %s)"),
			ANSI_TO_TCHAR(Name),
			*Context.ObjectAddress,
			*Context.ArgumentValues[0]
		);

		return Call;
	}
};

void FScriptFunctionNativeForm::BindNativeAssignment(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeAssignment(Name, bTrivial));
}

struct FScriptNativeUObjectCast : public FScriptFunctionNativeForm
{
	FString TargetType;
	bool bGuaranteed;
	
	FScriptNativeUObjectCast(const FString& InTargetType, bool InGuaranteed)
		: TargetType(InTargetType), bGuaranteed(InGuaranteed)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::UObjectCast;
		Info.TargetType = TargetType;
		Info.bGuaranteed = bGuaranteed;
		return Info;
	}
#endif

	bool ShouldIgnoreObjectArgument() const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return (Method != EScriptFunctionCallMethod::PointerCall);
	}

	bool CanSkipObjectNullCheck(EScriptFunctionCallMethod Method) const override
	{
		return (Method == EScriptFunctionCallMethod::CustomCall);
	}

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		if (bGuaranteed)
			return true;

		if (Context.LastPushedTypeInfo == nullptr)
			return false;
		if ((Context.LastPushedTypeInfo->GetFlags() & asOBJ_REF) == 0)
			return false;

		FAngelscriptTypeUsage Usage;
		Usage = FAngelscriptTypeUsage::FromTypeId(Context.LastPushedTypeInfo->GetTypeId());

		if (Usage.IsValid())
			return true;
		else
			return false;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		check(Context.ObjectAddress.Len() != 0);

		FNativeFunctionCall Call;

		if (bGuaranteed)
		{
			check(Context.ArgumentValues.Num() == 0);

			Call.CallCode = FString::Printf(TEXT("%s* CastedObject = (%s*)%s"),
				*TargetType,
				*TargetType,
				*Context.ObjectAddress
			);
			Call.ReturnValue = TEXT("CastedObject");
		}
		else
		{
			check(Context.ArgumentValues.Num() == 2);

			FAngelscriptTypeUsage Usage;
			Usage = FAngelscriptTypeUsage::FromTypeId(Context.LastPushedTypeInfo->GetTypeId());

			FAngelscriptType::FCppForm CppForm;
			Usage.GetCppForm(CppForm);
			UClass* TargetClass = Context.LastPushedTypeInfo != nullptr
				? reinterpret_cast<UClass*>(Context.LastPushedTypeInfo->GetUserData())
				: nullptr;
			const bool bTargetIsInterface = TargetClass != nullptr && TargetClass->HasAnyClassFlags(CLASS_Interface);

			if (bTargetIsInterface)
			{
				UE_LOG(
					Angelscript,
					Display,
					TEXT("StaticJIT UObject cast targetType=%hs targetClass=%s cppType=%s cppGeneric=%s"),
					Context.LastPushedTypeInfo->GetName(),
					*TargetClass->GetName(),
					CppForm.CppType.IsEmpty() ? TEXT("<empty>") : *CppForm.CppType,
					CppForm.CppGenericType.IsEmpty() ? TEXT("<empty>") : *CppForm.CppGenericType);
			}

			if (!CppForm.CppType.IsEmpty())
			{
				// We're casting to a native class, so we can use a real unreal cast, yay!
				FString NativeClassName = CppForm.CppType;
				NativeClassName.RemoveFromEnd(TEXT("*"));

				Call.Header = CppForm.CppHeader;
				Call.CallCode = FString::Printf(TEXT("*((void**)%s) = Cast<%s>((UObject*)%s)"),
					*Context.ArgumentValues[0],
					*NativeClassName,
					*Context.ObjectAddress
				);

				Call.bHandledReturnValue = true;
				check(Context.ReturnValueAddress.Len() == 0);
			}
			else
			{
				// We're casting to a script type, so retrieve the UClass that it's for and do the cast
				JITContext.Line("UObject* CastObject = (UObject*)({0});", Context.ObjectAddress);
				JITContext.Line("UClass* DestClass = (UClass*)(({0})->plainUserData);", 
					JITContext.ReferenceTypeInfo(Context.LastPushedTypeInfo));

				Call.CallCode = FString::Printf(
					TEXT("*((void**)%s) = (CastObject && CastObject->IsA(DestClass)) ? (void*)(CastObject) : nullptr"),
					*Context.ArgumentValues[0]
				);

				Call.bHandledReturnValue = true;
				check(Context.ReturnValueAddress.Len() == 0);
			}
		}

		return Call;
	}
};

void FScriptFunctionNativeForm::BindNativeUObjectCast(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const FString& TargetType, bool bGuaranteed)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeUObjectCast(TargetType, bGuaranteed));
}

struct FScriptNativeMethod : public FScriptFunctionNativeForm
{
	const ANSICHAR* Name;
	bool bTrivial;
	
	FScriptNativeMethod(const ANSICHAR* InName, bool InTrivial)
		: Name(InName), bTrivial(InTrivial)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::Method;
		Info.Name = MakeNativeFormDebugString(Name);
		Info.bTrivial = bTrivial;
		return Info;
	}
#endif

	bool ShouldIgnoreObjectArgument() const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return bTrivial;
	}

	FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const override
	{
		FNativeFunctionCall Call;
		Call.CallCode = FString::Printf(TEXT("%s->%s("), *Context.ObjectAddress, ANSI_TO_TCHAR(Name));
		Context.AppendArgumentsTo(Call.CallCode);
		Call.CallCode += TEXT(")");

		return Call;
	}
};

void FScriptFunctionNativeForm::BindNativeMethod(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeMethod(Name, bTrivial));
}

struct FScriptNativeFunction : public FScriptFunctionNativeForm
{
	const ANSICHAR* Name;
	bool bTrivial;
	
	FScriptNativeFunction(const ANSICHAR* InName, bool InTrivial)
		: Name(InName), bTrivial(InTrivial)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::Function;
		Info.Name = MakeNativeFormDebugString(Name);
		Info.bTrivial = bTrivial;
		return Info;
	}
#endif

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return bTrivial;
	}

	FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const override
	{
		FNativeFunctionCall Call;
		Call.CallCode = FString::Printf(TEXT("%s("), ANSI_TO_TCHAR(Name));
		Context.AppendArgumentsTo(Call.CallCode);
		Call.CallCode += TEXT(")");

		return Call;
	}
};

void FScriptFunctionNativeForm::BindNativeFunction(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeFunction(Name, bTrivial));
}

struct FScriptNativeFunctionHeader : public FScriptFunctionNativeForm
{
	const ANSICHAR* Name;
	const ANSICHAR* Header;
	bool bTrivial;
	
	FScriptNativeFunctionHeader(const ANSICHAR* InName, bool InTrivial, const ANSICHAR* InHeader)
		: Name(InName), Header(InHeader), bTrivial(InTrivial)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::FunctionHeader;
		Info.Name = MakeNativeFormDebugString(Name);
		Info.Header = MakeNativeFormDebugString(Header);
		Info.bTrivial = bTrivial;
		return Info;
	}
#endif

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return bTrivial;
	}

	FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const override
	{
		FNativeFunctionCall Call;
		Call.CallCode = FString::Printf(TEXT("%s("), ANSI_TO_TCHAR(Name));
		Context.AppendArgumentsTo(Call.CallCode);
		Call.CallCode += TEXT(")");

		Call.Header = ANSI_TO_TCHAR(Header);
		if (Call.Header.Len() != 0)
			Call.Header = FString::Printf(TEXT("#include \"%s\""), *Call.Header);

		return Call;
	}
};

void FScriptFunctionNativeForm::BindNativeFunctionHeader(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial, const ANSICHAR* Header)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeFunctionHeader(Name, bTrivial, Header));
}

struct FScriptNativeUFunction : public FScriptFunctionNativeForm
{
	UFunction* Function;
	FString Name;
	bool bTrivial;
	
	FScriptNativeUFunction(UFunction* InFunction, const FString& InName, bool InTrivial)
		: Function(InFunction)
		, Name(InName)
		, bTrivial(InTrivial)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::UFunction;
		Info.Name = Name;
		Info.UnrealFunction = Function;
		Info.bTrivial = bTrivial;
		return Info;
	}
#endif

	bool CanSkipInformSystemFunction() const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return bTrivial;
	}

	bool CanCallNative(const FNativeFunctionContext& Context) const override
	{
		if (Function->HasAnyFunctionFlags(FUNC_Static))
		{
			UClass* InClass = CastChecked<UClass>(Function->GetOuter());
			FString Header = FAngelscriptBindDatabase::GetSourceHeader(InClass);
			if (Header.Len() == 0)
				return false;
		}
		if (Function->HasAnyFunctionFlags(FUNC_Private))
			return false;
		if (Function->HasAnyFunctionFlags(FUNC_Protected))
			return false;
#if !IS_MONOLITHIC
		// In non-monolithic builds we can only directly call stuff we've whitelisted,
		// because we may not be linking the game module to them.
		UPackage* InPackage = Function->GetOutermost();
		FString PackageName = InPackage->GetPathName();
		if (PackageName == FString(TEXT("/Script/")) + FApp::GetProjectName())
			return true;
		if (PackageName == TEXT("/Script/Engine"))
			return true;
		return false;
#else
		return true;
#endif
	}

	FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const override
	{
		FNativeFunctionCall Call;

		if (Function->HasAnyFunctionFlags(FUNC_Static))
		{
			UClass* InClass = CastChecked<UClass>(Function->GetOuter());
			Call.Header = FAngelscriptBindDatabase::GetSourceHeader(InClass);
			if (Call.Header.Len() != 0)
				Call.Header = FString::Printf(TEXT("#include \"%s\""), *Call.Header);


			FString ClassName = InClass->GetPrefixCPP();
			ClassName += InClass->GetName();

			Call.CallCode = FString::Printf(TEXT("%s::%s("),
				*ClassName,
				*Name);

			Context.AppendArgumentsTo(Call.CallCode);
			Call.CallCode += TEXT(")");
		}
		else
		{
			Call.CallCode = FString::Printf(TEXT("%s->%s("), *Context.ObjectAddress, *Name);
			Context.AppendArgumentsTo(Call.CallCode);
			Call.CallCode += TEXT(")");
		}

		return Call;
	}
};

void FScriptFunctionNativeForm::BindUFunction(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, UFunction* Function, const FString& Name, bool bTrivial)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeUFunction(Function, Name, bTrivial));
}

struct FScriptNativeTArrayIteratorProceed : public FScriptFunctionNativeForm
{
	FScriptNativeTArrayIteratorProceed()
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::TArrayIteratorProceed;
		return Info;
	}
#endif

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return true;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		check(Context.ObjectAddress.Len() != 0);
		check(Context.ReturnValueAddress.Len() == 0);
		check(Context.ArgumentValues.Num() == 0);
		check(Context.ObjectType.Usage.SubTypes.Num() == 1);

		FString SubTypeStr;

		FAngelscriptType::FCppForm SubTypeNativeForm;
		if (Context.ObjectType.Usage.SubTypes[0].GetCppForm(SubTypeNativeForm))
		{
			if (!SubTypeNativeForm.CppType.IsEmpty())
				SubTypeStr = SubTypeNativeForm.CppType;
			else if (!SubTypeNativeForm.CppGenericType.IsEmpty())
				SubTypeStr = SubTypeNativeForm.CppGenericType;
		}


		JITContext.Line("FArrayIterator* It = (FArrayIterator*){0};", Context.ObjectAddress);
		JITContext.Line("uint32 Num = (uint32)It->Array->Num();");
		JITContext.Line("if ((uint32)It->Index >= Num) [[unlikely]] {");
		JITContext.Line("FArrayOperations::ThrowOutOfBounds();");
		JITContext.ExceptionCleanupAndReturn(true, false);
		JITContext.Line("}");

		if (SubTypeStr.IsEmpty())
			JITContext.Line("void* ElementRef = FArrayOperations::Iterator_Proceed_Unchecked(It, Num);");
		else
			JITContext.Line("void* ElementRef = FArrayOperations::Iterator_Proceed_Template_Unchecked<{0}>(It, Num);", SubTypeStr);

		FNativeFunctionCall Call;
		Call.Header = TEXT("#include \"Binds/Bind_TArray_Structs.h\"");
		Call.ReturnValue = TEXT("ElementRef");

		return Call;
	}
};

void FScriptFunctionNativeForm::BindTArrayIteratorProceed(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeTArrayIteratorProceed());
}

struct FScriptNativeTemplateInstantiation : public FScriptFunctionNativeForm
{
	const ANSICHAR* Name;
	bool bTrivial;
	bool bNeedsCompare;
	bool bNeedsCopy;
	
	FScriptNativeTemplateInstantiation(const ANSICHAR* InName, bool InTrivial, bool InNeedsCompare, bool InNeedsCopy)
		: Name(InName), bTrivial(InTrivial), bNeedsCompare(InNeedsCompare), bNeedsCopy(InNeedsCopy)
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::TemplateInstantiatedCall;
		Info.Name = MakeNativeFormDebugString(Name);
		Info.bTrivial = bTrivial;
		Info.bNeedsCompare = bNeedsCompare;
		Info.bNeedsCopy = bNeedsCopy;
		return Info;
	}
#endif

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return bTrivial;
	}

	bool CanSkipInformSystemFunction() const override
	{
		return true;
	}

	bool CanSkipScriptFunctionLookup(FNativeFunctionContext& Context) const override
	{
		return !GetNativeObjectType(Context).IsEmpty();
	}

	FString GetNativeObjectType(FNativeFunctionContext& Context) const
	{
		FString ObjectTypeStr;

		FAngelscriptType::FCppForm ObjectNativeForm;
		if (Context.ObjectType.Usage.GetCppForm(ObjectNativeForm))
		{
			if (!ObjectNativeForm.CppType.IsEmpty())
				ObjectTypeStr = ObjectNativeForm.CppType;
			else if (!ObjectNativeForm.CppGenericType.IsEmpty())
				ObjectTypeStr = ObjectNativeForm.CppGenericType;
		}

		bool bSubtypesAllowed = true;
		for (auto SubType : Context.ObjectType.Usage.SubTypes)
		{
			UScriptStruct* SubStruct = Cast<UScriptStruct>(SubType.GetUnrealStruct());
			if (SubStruct != nullptr)
			{
				if (bNeedsCompare && ((SubStruct->StructFlags & STRUCT_IdenticalNative) == 0 || SubStruct->IsA<UASStruct>() || !SubType.CanCompare()))
				{
					bSubtypesAllowed = false;
				}

				if (bNeedsCopy && (SubStruct->StructFlags & (STRUCT_CopyNative | STRUCT_IsPlainOldData)) == 0)
				{
					bSubtypesAllowed = false;
				}
			}
		}

		if (bSubtypesAllowed)
		{
			return ObjectTypeStr;
		}
		else
		{
			return FString();
		}
	}

	FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const override
	{
		FString ObjectTypeStr = GetNativeObjectType(Context);

		FNativeFunctionCall Call;
		if (ObjectTypeStr.IsEmpty())
		{
			Call.CallCode = FString::Printf(TEXT("%s("), ANSI_TO_TCHAR(Name));
			Context.AppendArgumentsTo(Call.CallCode);
			Call.CallCode += TEXT(")");
		}
		else
		{
			int32 IgnoreArguments = 1;
			if (Context.CallingFunction->sysFuncIntf->passFirstParamMetaData != asEFirstParamMetaData::None)
				IgnoreArguments += 1;

			Call.CallCode = FString::Printf(TEXT("%s_Template(*(%s*)(%s)"),
				ANSI_TO_TCHAR(Name),
				*ObjectTypeStr,
				*Context.ObjectAddress
			);

			for (int32 i = IgnoreArguments, Count = Context.ArgumentValues.Num(); i < Count; ++i)
			{
				Call.CallCode += TEXT(", ");
				Call.CallCode += Context.ArgumentValues[i];
			}

			Call.CallCode += TEXT(")");
		}

		return Call;
	}
};

void FScriptFunctionNativeForm::BindTemplateInstantiatedCall(
	FAngelscriptEngine& Engine,
	asIScriptFunction* ScriptFunction,
	const ANSICHAR* Name,
	bool bTrivial,
	bool bNeedsCompare,
	bool bNeedsCopy)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeTemplateInstantiation(Name, bTrivial, bNeedsCompare, bNeedsCopy));
}

struct FScriptNativeTArrayIteratorCreate : public FScriptFunctionNativeForm
{
	FScriptNativeTArrayIteratorCreate()
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::TArrayIteratorCreate;
		return Info;
	}
#endif

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return true;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		check(Context.ObjectAddress.Len() != 0);
		check(Context.ReturnValueAddress.Len() != 0);
		check(Context.ArgumentValues.Num() == 0);
		check(Context.ObjectType.Usage.SubTypes.Num() == 1);

		FString SubTypeStr;

		FAngelscriptType::FCppForm SubTypeNativeForm;
		if (Context.ObjectType.Usage.SubTypes[0].GetCppForm(SubTypeNativeForm))
		{
			if (!SubTypeNativeForm.CppType.IsEmpty())
				SubTypeStr = SubTypeNativeForm.CppType;
			else if (!SubTypeNativeForm.CppGenericType.IsEmpty())
				SubTypeStr = SubTypeNativeForm.CppGenericType;
		}

		JITContext.Line("FArrayIterator* It = (FArrayIterator*){0};", Context.ReturnValueAddress);
		JITContext.Line("It->Array = (FScriptArray*){0};", Context.ObjectAddress);

		auto* ScriptType = Context.CallingFunction->objectType->templateSubTypes[0].GetTypeInfo();

		if (!SubTypeStr.IsEmpty())
		{
			JITContext.Line("It->Stride = sizeof({0});", SubTypeStr);
		}
		else if ((ScriptType->GetFlags() & (asOBJ_VALUE | asOBJ_REF)) != 0 && !JITContext.JIT->IsTypePotentiallyDifferent((asCObjectType*)ScriptType))
		{
			JITContext.Line("It->Stride = {0};", ScriptType->size);
		}
		else if((ScriptType->GetFlags() & asOBJ_ENUM) != 0)
		{
			JITContext.Line("It->Stride = {0};", ScriptType->size);
		}
		else
		{
			JITContext.Line("It->Stride = {0}->size;", JITContext.ReferenceTypeInfo(ScriptType));
		}

		JITContext.Line("It->Index = 0;");
		JITContext.Line("It->bCanProceed = ((FScriptArray*){0})->Num() > 0;", Context.ObjectAddress);

		FNativeFunctionCall Call;
		Call.Header = TEXT("#include \"Binds/Bind_TArray_Structs.h\"");
		Call.bHandledReturnValue = true;

		return Call;
	}
};

void FScriptFunctionNativeForm::BindTArrayIteratorCreate(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeTArrayIteratorCreate());
}

struct FScriptNativeTArrayIndex : public FScriptFunctionNativeForm
{
#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::TArrayIndex;
		return Info;
	}
#endif

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return true;
	}

	bool CanSkipInformSystemFunction() const override
	{
		return true;
	}

	bool CanSkipScriptFunctionLookup(FNativeFunctionContext& Context) const override
	{
		return true;
	}

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		return true;
	}

	FString GetNativeObjectType(FNativeFunctionContext& Context) const
	{
		FString ObjectTypeStr;

		FAngelscriptType::FCppForm ObjectNativeForm;
		if (Context.ObjectType.Usage.GetCppForm(ObjectNativeForm))
		{
			if (!ObjectNativeForm.CppType.IsEmpty())
				ObjectTypeStr = ObjectNativeForm.CppType;
			else if (!ObjectNativeForm.CppGenericType.IsEmpty())
				ObjectTypeStr = ObjectNativeForm.CppGenericType;
		}

		return ObjectTypeStr;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		FString ObjectTypeStr = GetNativeObjectType(Context);
		auto* ScriptType = Context.CallingFunction->objectType->templateSubTypes[0].GetTypeInfo();

		JITContext.Line("FScriptArray* ArrPtr = (FScriptArray*){0};", Context.ObjectAddress);
		JITContext.Line("int32 ArrIndex = value_as<int32>({0});", Context.ArgumentValues[0]);
		JITContext.Line("if (!FArrayOperations::IsValidIndex(*ArrPtr, ArrIndex)) [[unlikely]] {");
		JITContext.Line("FArrayOperations::ThrowOutOfBounds();");
		JITContext.ExceptionCleanupAndReturn(true, false);
		JITContext.Line("}");

		if (!ObjectTypeStr.IsEmpty())
		{
			JITContext.Line("void* ElementRef = FArrayOperations::OpIndex_Template_Unchecked(*({0}*)ArrPtr, ArrIndex);", ObjectTypeStr);
		}
		else if ((ScriptType->GetFlags() & (asOBJ_VALUE | asOBJ_REF)) != 0 && !JITContext.JIT->IsTypePotentiallyDifferent((asCObjectType*)ScriptType))
		{
			JITContext.Line("void* ElementRef = FArrayOperations::OpIndex_Stride_Unchecked(*ArrPtr, {0}, ArrIndex);", ScriptType->size);
		}
		else if((ScriptType->GetFlags() & asOBJ_ENUM) != 0)
		{
			JITContext.Line("void* ElementRef = FArrayOperations::OpIndex_Stride_Unchecked(*ArrPtr, {0}, ArrIndex);", ScriptType->size);
		}
		else
		{
			JITContext.Line("void* ElementRef = FArrayOperations::OpIndex_Unchecked(*ArrPtr, {0}, ArrIndex);",
				JITContext.ReferenceObjectType(Context.CallingFunction->objectType));
		}

		JITContext.Line("SCRIPT_ASSUME(ElementRef != nullptr);");

		FNativeFunctionCall Call;
		Call.ReturnValue = TEXT("ElementRef");
		return Call;
	}
};

void FScriptFunctionNativeForm::BindTArrayIndex(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeTArrayIndex());
}

const ANSICHAR* FScriptFunctionNativeForm::AllocateAnsiTypeName(const FString& TypeName)
{
	if (!FAngelscriptEngine::IsCollectingStaticJITCompatibilityBinds())
		return nullptr;

	auto* Buffer = new TArray<ANSICHAR>();
	*Buffer = StringToArray<ANSICHAR, TCHAR>(&TypeName[0], TypeName.Len());
	Buffer->Add('\0');
	return Buffer->GetData();
}

struct FScriptNativePushArg : public FScriptFunctionNativeForm
{
	FScriptNativePushArg()
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::PushArgument;
		return Info;
	}
#endif

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return true;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		extern FNativeFunctionCall GenerateCustomCall_PushArg(FNativeFunctionContext& Context, FStaticJITContext& JITContext, bool bIsRef);
		return GenerateCustomCall_PushArg(Context, JITContext, false);
	}
};

void FScriptFunctionNativeForm::BindPushArg(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativePushArg());
}

struct FScriptNativePushArgRef : public FScriptFunctionNativeForm
{
	FScriptNativePushArgRef()
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::PushArgumentRef;
		return Info;
	}
#endif

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return true;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		extern FNativeFunctionCall GenerateCustomCall_PushArg(FNativeFunctionContext& Context, FStaticJITContext& JITContext, bool bIsRef);
		return GenerateCustomCall_PushArg(Context, JITContext, true);
	}
};

void FScriptFunctionNativeForm::BindPushArgRef(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativePushArgRef());
}

struct FScriptNativeDelegateExecute : public FScriptFunctionNativeForm
{
	FScriptNativeDelegateExecute()
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::DelegateExecute;
		return Info;
	}
#endif

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return true;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		extern FNativeFunctionCall GenerateCustomCall_DelegateExecute(FNativeFunctionContext& Context, FStaticJITContext& JITContext);
		return GenerateCustomCall_DelegateExecute(Context, JITContext);
	}
};

void FScriptFunctionNativeForm::BindDelegateExecute(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeDelegateExecute());
}

struct FScriptNativeMulticastExecute : public FScriptFunctionNativeForm
{
	FScriptNativeMulticastExecute()
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::MulticastExecute;
		return Info;
	}
#endif

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return true;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		extern FNativeFunctionCall GenerateCustomCall_MulticastExecute(FNativeFunctionContext& Context, FStaticJITContext& JITContext);
		return GenerateCustomCall_MulticastExecute(Context, JITContext);
	}
};

void FScriptFunctionNativeForm::BindMulticastExecute(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeMulticastExecute());
}

struct FScriptNativeEventFunctionExecute : public FScriptFunctionNativeForm
{
	FScriptNativeEventFunctionExecute()
	{
	}

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const override
	{
		FAngelscriptNativeFormDebugInfo Info;
		Info.Kind = EAngelscriptNativeFormKind::EventFunctionExecute;
		return Info;
	}
#endif

	bool CanCallCustom(const FNativeFunctionContext& Context) const override
	{
		return true;
	}

	bool IsTrivialFunction(EScriptFunctionCallMethod Method) const override
	{
		return true;
	}

	FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, FStaticJITContext& JITContext) const override
	{
		extern FNativeFunctionCall GenerateCustomCall_EventFunctionExecute(FNativeFunctionContext& Context, FStaticJITContext& JITContext);
		return GenerateCustomCall_EventFunctionExecute(Context, JITContext);
	}
};

void FScriptFunctionNativeForm::BindEventFunctionExecute(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction)
{
	AddNativeForm(Engine, ScriptFunction, new FScriptNativeEventFunctionExecute());
}

#endif // AS_CAN_GENERATE_JIT
