#include "Binds/Bind_Delegates.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#include "Helper_CppType.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

inline static FString CreateCppNameForDelegate(UDelegateFunction* Function)
{
	FString Decl = TEXT("F");
	Decl += Function->GetName();
	Decl.RemoveFromEnd(TEXT("__DelegateSignature"));

	// Delegates declared inside classes get suffixed with the class they're in,
	// so we don't run into conflicts binding them globally.
	UClass* OuterClass = Cast<UClass>(Function->GetOuter());
	if (OuterClass)
	{
		Decl = FString::Printf(TEXT("%s%s::%s"), OuterClass->GetPrefixCPP(), *OuterClass->GetName(), *Decl);
	}

	return Decl;
}

inline static FString CreateAngelscriptNameForDelegate(UDelegateFunction* Function)
{
	FString Decl = TEXT("F");
	Decl += Function->GetName();
	Decl.RemoveFromEnd(TEXT("__DelegateSignature"));

	// Delegates declared inside classes get suffixed with the class they're in,
	// so we don't run into conflicts binding them globally.
	if (Function->GetOuter()->IsA<UClass>())
		Decl += TEXT("__") + Function->GetOuter()->GetName();

	return Decl;
}

struct FDelegateOps
{
	UDelegateFunction* SignatureFunction;
};

// From Bind_BlueprintEvent.cpp
extern void BindDelegateEvent(FAngelscriptBinds& Delegate_, UFunction* Function, bool bIsMulticast, bool bIsSparse);

struct FScriptDelegateType : TAngelscriptCppType<FScriptDelegate>
{
	FString Name;
	UDelegateFunction* Function;

	FScriptDelegateType(const FString& InName, UDelegateFunction* InFunction)
		: Name(InName)
		, Function(InFunction)
	{
	}

	FScriptDelegateType()
		: Name(TEXT("_FScriptDelegate"))
		, Function(nullptr)
	{}

	FORCEINLINE UDelegateFunction* GetSignature(const FAngelscriptTypeUsage& Usage) const
	{
		if (Function != nullptr)
			return Function;
		check(Usage.ScriptClass != nullptr);
		void* UserData = Usage.ScriptClass->GetUserData();
		check(UserData != FAngelscriptType::TAG_UserData_Delegate);
		check(UserData != FAngelscriptType::TAG_UserData_Multicast_Delegate);
		return (UDelegateFunction*)UserData;
	}

	FORCEINLINE UDelegateFunction* GetSignatureMaybeTagged(const FAngelscriptTypeUsage& Usage) const
	{
		if (Function != nullptr)
			return Function;
		check(Usage.ScriptClass != nullptr);
		void* UserData = Usage.ScriptClass->GetUserData();
		if (UserData == FAngelscriptType::TAG_UserData_Delegate)
			return nullptr;
		if (UserData == FAngelscriptType::TAG_UserData_Multicast_Delegate)
			return nullptr;
		return (UDelegateFunction*)UserData;
	}

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override
	{
		// C++ delegates have individual type instances, so we don't need to check this
		if (Function != nullptr)
			return true;

		// If the scriptclass is identical we don't need to check it
		if (Usage.ScriptClass == Other.ScriptClass)
			return true;

		// Shouldn't happen, safety check
		if (Usage.ScriptClass == nullptr || Other.ScriptClass == nullptr)
			return false;

		// Compare script delegates by name, because we are likely comparing for changes during a compile
		if (((asCObjectType*)Usage.ScriptClass)->name == ((asCObjectType*)Other.ScriptClass)->name)
			return true;

		return false;
	}

	void* GetData() const override
	{
		return Function;
	}

	FString GetAngelscriptTypeName() const override
	{
		return Name;
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		auto* Prop = new FDelegateProperty(Params.Outer, Params.PropertyName);
		Prop->SignatureFunction = GetSignature(Usage);
		return Prop;
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		FScriptDelegate* ValuePtr = (FScriptDelegate*)Data.StackPtr;
		new(ValuePtr) FScriptDelegate();

		if (Usage.bIsReference)
		{
			FScriptDelegate& ObjRef = Stack.StepCompiledInRef<FDelegateProperty, FScriptDelegate>(ValuePtr);
			Context->SetArgAddress(ArgumentIndex, &ObjRef);
		}
		else
		{
			Stack.StepCompiledIn<FDelegateProperty>(ValuePtr);
			Context->SetArgObject(ArgumentIndex, ValuePtr);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return !Usage.bIsReference;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		void* ReturnedObject = Context->GetReturnObject();
		if (ReturnedObject == nullptr)
			return;
		*(FScriptDelegate*)Destination = *(FScriptDelegate*)ReturnedObject;
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		auto* DelegateProp = CastField<FDelegateProperty>(Property);
		if (DelegateProp == nullptr)
			return false;
		auto* Signature = GetSignatureMaybeTagged(Usage);
		if (Signature != nullptr)
		{
			return DelegateProp->SignatureFunction == Signature;
		}
		else
		{
			check(Usage.ScriptClass != nullptr);
			FString CheckName = ANSI_TO_TCHAR(Usage.ScriptClass->GetName());
			return DelegateProp->SignatureFunction->GetFName().GetPlainNameString() == CheckName;
		}
	}

	bool DefaultValue_AngelscriptFallback(const FAngelscriptTypeUsage& Usage, FString& OutAngelscriptValue) const
	{
		OutAngelscriptValue = Name + TEXT("()");
		return true;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		if (Function == nullptr)
		{
			OutCppForm.bNativeCannotBeGeneric = true;
			OutCppForm.TemplateObjectForm = TEXT("FScriptDelegate");
			return true;
		}

		OutCppForm.CppType = CreateCppNameForDelegate(Function);
		FString HeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Function);
		if (HeaderPath.Len() != 0 && !HeaderPath.Contains(TEXT("NoExportTypes.h")))
		{
			OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *HeaderPath);
		}
		OutCppForm.bNativeCannotBeGeneric = true;
		OutCppForm.TemplateObjectForm = TEXT("FScriptDelegate");
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		FScriptDelegate& Delegate = Usage.ResolvePrimitive<FScriptDelegate>(Address);

		if (Function != nullptr)
			Value.Type = Name;
		else if(Usage.ScriptClass != nullptr)
			Value.Type = ANSI_TO_TCHAR(Usage.ScriptClass->GetName());

		Value.Usage = Usage;
		Value.Address = Address;
		Value.SetAddressToMonitor(&Delegate, sizeof(Delegate));

		if (Delegate.IsBound())
		{
			UObject* Object = Delegate.GetUObject();
			FName FunctionName = Delegate.GetFunctionName();

			Value.bHasMembers = true;
			Value.Value = FString::Printf(TEXT("Bound to %s.%s"),
				*GetNameSafe(Object),
				*FunctionName.ToString()
			);
		}
		else
		{
			Value.bHasMembers = false;
			Value.Value = TEXT("Unbound");
		}

		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		FScriptDelegate& Delegate = Usage.ResolvePrimitive<FScriptDelegate>(Address);
		if (!Delegate.IsBound())
			return false;

		UObject* Object = Delegate.GetUObject();
		FName FunctionName = Delegate.GetFunctionName();

		FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(UObject::StaticClass()));
		if (ObjectUsage.IsValid())
		{
			FDebuggerValue ObjectValue;
			const UObject*& ObjectRef = ObjectValue.AllocatePODLiteral<const UObject*>();
			ObjectRef = Object;

			ObjectValue.Name = TEXT("Object");
			ObjectValue.Usage = ObjectUsage;
			ObjectValue.Address = &ObjectRef;
			ObjectUsage.GetDebuggerValue(&ObjectRef, ObjectValue);
			Scope.Values.Add(MoveTemp(ObjectValue));
		}

		FDebuggerValue NameValue;
		NameValue.Name = TEXT("Function");
		NameValue.Type = TEXT("FName");
		NameValue.Value = FString::Printf(TEXT("n\"%s\""), *FunctionName.ToString());
		Scope.Values.Add(MoveTemp(NameValue));

		return true;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		FScriptDelegate& Delegate = Usage.ResolvePrimitive<FScriptDelegate>(Address);
		if (!Delegate.IsBound())
			return false;

		if (Member == TEXT("Object"))
		{
			UObject* Object = Delegate.GetUObject();
			Value.Name = TEXT("Object");

			const UObject*& ObjectRef = Value.AllocatePODLiteral<const UObject*>();
			ObjectRef = Object;

			FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(UObject::StaticClass()));
			if (ObjectUsage.IsValid())
			{
				Value.Usage = ObjectUsage;
				Value.Address = &ObjectRef;
				ObjectUsage.GetDebuggerValue(&ObjectRef, Value);
			}
			else
			{
				Value.Type = TEXT("UObject");
				Value.Value = GetNameSafe(Object);
			}

			return true;
		}
		else if (Member == TEXT("Function"))
		{
			FName FunctionName = Delegate.GetFunctionName();
			Value.Name = TEXT("Function");
			Value.Type = TEXT("FName");
			Value.Value = FString::Printf(TEXT("n\"%s\""), *FunctionName.ToString());
			return true;
		}

		return false;
	}
};

bool CheckAngelscriptPropertyCompatibility(const FProperty* A, const FProperty* B)
{
	if (A == B)
	{
		return true;
	}

	if (!A || !B) //one of properties is null
	{
		return false;
	}

	if (A->GetSize() != B->GetSize())
	{
		return false;
	}

	if (A->GetOffset_ForGC() != B->GetOffset_ForGC())
	{
		return false;
	}

	if (!A->SameType(B))
	{
		if (auto* EnumFieldA = CastField<FEnumProperty>(A))
		{
			if (auto* ByteFieldB = CastField<FByteProperty>(B))
			{
				//if (ByteFieldB->Enum == EnumFieldA->Enum)
				if (ByteFieldB->Enum == EnumFieldA->GetEnum())
					return true;
			}
		}
		else if (auto* EnumFieldB = CastField<FEnumProperty>(B))
		{
			if (auto* ByteFieldA = CastField<FByteProperty>(A))
			{
				//if (ByteFieldA->Enum == EnumFieldB->Enum)
				if (ByteFieldA->Enum == EnumFieldB->GetEnum())
					return true;
			}
		}
		return false;
	}

	return true;
}

// This mirrors UFunction::IsSignatureCompatibleWith except that it treats
// enums properties of the same size as compatible regardless of the property style
bool CheckAngelscriptDelegateCompatibility(UFunction* Signature, UFunction* CheckFunction)
{
	// Early out if they're exactly the same function
	if (Signature == CheckFunction)
	{
		return true;
	}

	const uint64 IgnoreFlags = UFunction::GetDefaultIgnoredSignatureCompatibilityFlags();


	// Run thru the parameter property chains to compare each property
	TFieldIterator<FProperty> IteratorA(Signature);
	TFieldIterator<FProperty> IteratorB(CheckFunction);

	while (IteratorA && (IteratorA->PropertyFlags & CPF_Parm))
	{
		if (IteratorB && (IteratorB->PropertyFlags & CPF_Parm))
		{
			// Compare the two properties to make sure their types are identical
			// Note: currently this requires both to be strictly identical and wouldn't allow functions that differ only by how derived a class is,
			// which might be desirable when binding delegates, assuming there is directionality in the SignatureIsCompatibleWith call
			FProperty* PropA = *IteratorA;
			FProperty* PropB = *IteratorB;

			// Check the flags as well
			uint64 PropertyMash = PropA->PropertyFlags ^ PropB->PropertyFlags;

			// Ignore ReferenceParm if the signature has an OutParm and the implementation has a ReferenceParm
			if ((PropA->PropertyFlags & CPF_OutParm) && (PropB->PropertyFlags & CPF_ReferenceParm))
				PropertyMash &= ~CPF_ReferenceParm;

			if (!CheckAngelscriptPropertyCompatibility(PropA, PropB) || ((PropertyMash & ~IgnoreFlags) != 0))
			{
				// Type mismatch between an argument of A and B
				return false;
			}
		}
		else
		{
			// B ran out of arguments before A did
			return false;
		}
		++IteratorA;
		++IteratorB;
	}

	// They matched all the way thru A's properties, but it could still be a mismatch if B has remaining parameters
	return !(IteratorB && (IteratorB->PropertyFlags & CPF_Parm));
}

void DeclareDelegate(UDelegateFunction* Function)
{
	FString Decl = CreateAngelscriptNameForDelegate(Function);

	auto& BindDB = FAngelscriptBindDatabase::Get();
	BindDB.BoundDelegateFunctions.Add(Function);

	FAngelscriptType::Register(MakeShared<FScriptDelegateType>(Decl, Function));

	FBindFlags BindFlags;
	auto Delegate_ = FAngelscriptBinds::ValueClass<FScriptDelegate>(Decl, BindFlags);
	Delegate_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Construct));
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	Delegate_.Destructor("void f()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Destruct));

	FString CopyDecl = FString::Printf(TEXT("void f(const %s& Other)"), *Decl);
	Delegate_.Constructor(CopyDecl, FUNC_TRIVIAL(FAngelscriptDelegateOperations::CopyConstruct));
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	FString AssignDecl = FString::Printf(TEXT("%s& opAssign(const %s& Other)"), *Decl, *Decl);
	Delegate_.Method(AssignDecl, FUNC_TRIVIAL(FAngelscriptDelegateOperations::Assign));
}

// Build a debug string for a
FString GetSignatureStringForFunction(UFunction* Function)
{
	FString Name;
	if (auto* DelegateFunc = Cast<UDelegateFunction>(Function))
	{
		if (DelegateFunc->GetOutermost() == FAngelscriptEngine::Get().AngelscriptPackage)
		{
			Name = Function->GetName();
			Name.RemoveFromEnd(TEXT("__DelegateSignature"));
		}
		else
		{
			Name = CreateAngelscriptNameForDelegate(DelegateFunc);
		}
	}
	else
	{
		Name = Function->GetName();
	}

	FString ReturnType = TEXT("void");
	FString Arguments;

	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		FProperty* Property = *It;
		FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromProperty(Property);

		if( Property->PropertyFlags & CPF_ReturnParm )
		{
			ReturnType = Type.GetAngelscriptDeclaration();
		}
		else
		{
			if (Arguments.Len() != 0)
				Arguments += TEXT(", ");

			Arguments += Type.GetAngelscriptDeclaration();

			// Hint that we should make this &in to match the signature
			if ((Property->PropertyFlags & CPF_ReferenceParm) != 0 && (Property->PropertyFlags & CPF_ConstParm) != 0)
				Arguments += TEXT("in");

			Arguments += TEXT(" ");
			Arguments += Property->GetName();
		}
	}

	return FString::Printf(TEXT("%s %s(%s)"),
		*ReturnType, *Name, *Arguments);
}

void FAngelscriptDelegateOperations::BindUFunction(FScriptDelegate* Delegate, asCScriptFunction* Function, UObject* InObject, const FName& InFunctionName)
{
	if (InObject == nullptr)
	{
		FAngelscriptEngine::Throw("Null object passed to BindUFunction.");
		return;
	}

	UFunction* CallFunction = InObject->FindFunction(InFunctionName);
	if (CallFunction == nullptr)
	{
		const FString Debug = FString::Printf(TEXT("\nCould not find function %s\nIs it declared UFUNCTION()?"), *InFunctionName.ToString());
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Debug));
		return;
	}

	FDelegateOps* Ops = (FDelegateOps*)Function->userData;
	if (!CheckAngelscriptDelegateCompatibility(Ops->SignatureFunction, CallFunction))
	{
		FString Message = FString::Printf(TEXT("Specified function is not compatible with delegate function.\n\nDelegate: %s\n\nAttempted Bind: %s"),
			*GetSignatureStringForFunction(Ops->SignatureFunction), *GetSignatureStringForFunction(CallFunction));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	Delegate->BindUFunction(InObject, InFunctionName);
}

void FAngelscriptDelegateOperations::BindUFunction_Signature(FScriptDelegate* Delegate, UObject* InObject, const FName& InFunctionName, UDelegateFunction* Signature)
{
	if (InObject == nullptr)
	{
		FAngelscriptEngine::Throw("Null object passed to BindUFunction.");
		return;
	}

	if (Signature == nullptr)
	{
		FAngelscriptEngine::Throw("Null signature passed to BindUFunction.");
		return;
	}

	UFunction* CallFunction = InObject->FindFunction(InFunctionName);
	if (CallFunction == nullptr)
	{
		FAngelscriptEngine::Throw("Could not find function in object with this name. Is it declared UFUNCTION()?");
		return;
	}

	if (!CheckAngelscriptDelegateCompatibility(Signature, CallFunction))
	{
		FString Message = FString::Printf(TEXT("Specified function is not compatible with delegate function.\n\nDelegate: %s\n\nAttempted Bind: %s"),
			*GetSignatureStringForFunction(Signature), *GetSignatureStringForFunction(CallFunction));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	Delegate->BindUFunction(InObject, InFunctionName);
}

void FAngelscriptDelegateOperations::ConstructFromFunction(FScriptDelegate* Delegate, asCScriptFunction* Function, UObject* Object, const FName& FunctionName)
{
	new(Delegate) FScriptDelegate();
	BindUFunction(Delegate, Function, Object, FunctionName);
}

void FAngelscriptDelegateOperations::ConstructFromFunction_Signature(FScriptDelegate* Delegate, UObject* InObject, const FName& InFunctionName, UDelegateFunction* Signature)
{
	new(Delegate) FScriptDelegate();

	if (InObject == nullptr)
	{
		FAngelscriptEngine::Throw("Null object passed to BindUFunction.");
		return;
	}

	if (Signature == nullptr)
	{
		FAngelscriptEngine::Throw("Null signature passed to BindUFunction.");
		return;
	}

	UFunction* CallFunction = InObject->FindFunction(InFunctionName);
	if (CallFunction == nullptr)
	{
		FAngelscriptEngine::Throw("Could not find function in object with this name. Is it declared UFUNCTION()?");
		return;
	}

	if (!CheckAngelscriptDelegateCompatibility(Signature, CallFunction))
	{
		FString Message = FString::Printf(TEXT("Specified function is not compatible with delegate function.\n\nDelegate: %s\n\nAttempted Bind: %s"),
			*GetSignatureStringForFunction(Signature), *GetSignatureStringForFunction(CallFunction));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	Delegate->BindUFunction(InObject, InFunctionName);
}

void DeclareDelegateOperations(UDelegateFunction* Function)
{
	FDelegateOps* Ops = new FDelegateOps;
	Ops->SignatureFunction = Function;

	FString Decl = CreateAngelscriptNameForDelegate(Function);

	auto Delegate_ = FAngelscriptBinds::ExistingClass(Decl);

	Delegate_.Method("bool IsBound() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::IsBound));
	Delegate_.Method("UObject GetUObject() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetUObject));
	Delegate_.Method("FName GetFunctionName() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetFunctionName)); 

	Delegate_.Method("void Clear()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Clear));

	Delegate_.Constructor("void f(UObject Object, const FName& FunctionName)", FUNC(FAngelscriptDelegateOperations::ConstructFromFunction), Ops);
	FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	Delegate_.Method("void BindUFunction(UObject Object, const FName& FunctionName)", FUNC(FAngelscriptDelegateOperations::BindUFunction), Ops);
	FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

	BindDelegateEvent(Delegate_, Function, false, false);
}

struct FMulticastScriptDelegateType : TAngelscriptCppType<FMulticastScriptDelegate>
{
	FString Name;
	UDelegateFunction* Function;

	FMulticastScriptDelegateType(const FString& InName, UDelegateFunction* InFunction)
		: Name(InName)
		, Function(InFunction)
	{
	}

	FMulticastScriptDelegateType()
		: Name(TEXT("_FMulticastScriptDelegate"))
		, Function(nullptr)
	{}

	FORCEINLINE UDelegateFunction* GetSignature(const FAngelscriptTypeUsage& Usage) const
	{
		if (Function != nullptr)
			return Function;
		check(Usage.ScriptClass != nullptr);
		void* UserData = Usage.ScriptClass->GetUserData();
		check(UserData != FAngelscriptType::TAG_UserData_Delegate);
		check(UserData != FAngelscriptType::TAG_UserData_Multicast_Delegate);
		return (UDelegateFunction*)UserData;
	}

	FORCEINLINE UDelegateFunction* GetSignatureMaybeTagged(const FAngelscriptTypeUsage& Usage) const
	{
		if (Function != nullptr)
			return Function;
		check(Usage.ScriptClass != nullptr);
		void* UserData = Usage.ScriptClass->GetUserData();
		if (UserData == FAngelscriptType::TAG_UserData_Delegate)
			return nullptr;
		if (UserData == FAngelscriptType::TAG_UserData_Multicast_Delegate)
			return nullptr;
		return (UDelegateFunction*)UserData;
	}

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override
	{
		// C++ delegates have individual type instances, so we don't need to check this
		if (Function != nullptr)
			return true;

		// If the scriptclass is identical we don't need to check it
		if (Usage.ScriptClass == Other.ScriptClass)
			return true;

		// Shouldn't happen, safety check
		if (Usage.ScriptClass == nullptr || Other.ScriptClass == nullptr)
			return false;

		// Compare script delegates by name, because we are likely comparing for changes during a compile
		if (((asCObjectType*)Usage.ScriptClass)->name == ((asCObjectType*)Other.ScriptClass)->name)
			return true;

		return false;
	}

	void* GetData() const override
	{
		return Function;
	}

	FString GetAngelscriptTypeName() const override
	{
		return Name;
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		auto* Prop = new FMulticastInlineDelegateProperty(Params.Outer, Params.PropertyName);
		Prop->SignatureFunction = GetSignature(Usage);
		Prop->SetPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable);
		return Prop;
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override
	{
		return !Usage.bIsReference;
	}

	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		FMulticastScriptDelegate* ValuePtr = (FMulticastScriptDelegate*)Data.StackPtr;
		new(ValuePtr) FMulticastScriptDelegate();

		if (Usage.bIsReference)
		{
			FMulticastScriptDelegate& ObjRef = Stack.StepCompiledInRef<FMulticastInlineDelegateProperty, FMulticastScriptDelegate>(ValuePtr);
			Context->SetArgAddress(ArgumentIndex, &ObjRef);
		}
		else
		{
			Stack.StepCompiledIn<FMulticastInlineDelegateProperty>(ValuePtr);
			Context->SetArgObject(ArgumentIndex, ValuePtr);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return !Usage.bIsReference;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		void* ReturnedObject = Context->GetReturnObject();
		if (ReturnedObject == nullptr)
			return;
		*(FMulticastScriptDelegate*)Destination = *(FMulticastScriptDelegate*)ReturnedObject;
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		auto* DelegateProp = CastField<FMulticastInlineDelegateProperty>(Property);
		if (DelegateProp == nullptr)
			return false;
		auto* Signature = GetSignatureMaybeTagged(Usage);
		if (Signature != nullptr)
		{
			return DelegateProp->SignatureFunction == Signature;
		}
		else
		{
			check(Usage.ScriptClass != nullptr);
			FString CheckName = ANSI_TO_TCHAR(Usage.ScriptClass->GetName());
			return DelegateProp->SignatureFunction->GetFName().GetPlainNameString() == CheckName;
		}
	}

	bool DefaultValue_AngelscriptFallback(const FAngelscriptTypeUsage& Usage, FString& OutAngelscriptValue) const
	{
		OutAngelscriptValue = Name + TEXT("()");
		return true;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		if (Function == nullptr)
		{
			OutCppForm.bNativeCannotBeGeneric = true;
			OutCppForm.TemplateObjectForm = TEXT("FMulticastScriptDelegate");
			return true;
		}

		OutCppForm.CppType = CreateCppNameForDelegate(Function);
		FString HeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Function);
		if (HeaderPath.Len() != 0 && !HeaderPath.Contains(TEXT("NoExportTypes.h")))
		{
			OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *HeaderPath);
		}
		OutCppForm.bNativeCannotBeGeneric = true;
		OutCppForm.TemplateObjectForm = TEXT("FMulticastScriptDelegate");
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		FMulticastScriptDelegate& Delegate = Usage.ResolvePrimitive<FMulticastScriptDelegate>(Address);

		if (Function != nullptr)
			Value.Type = Name;
		else if(Usage.ScriptClass != nullptr)
			Value.Type = ANSI_TO_TCHAR(Usage.ScriptClass->GetName());

		Value.Usage = Usage;
		Value.Usage.TypeIndex = 0;
		Value.Address = Address;

		if (Delegate.IsBound())
		{
			Value.bHasMembers = true;
			Value.Value = FString::Printf(TEXT("Bound to %d object(s)"), Delegate.GetAllObjects().Num());
		}
		else
		{
			Value.bHasMembers = false;
			Value.Value = TEXT("Unbound");
		}

		return true;
	}

	struct FMulticastScriptDelegateBinding
	{
		UObject* Object;
		FString FunctionName;
	};

	bool GetBindings(const FMulticastScriptDelegate& Delegate, TArray<FMulticastScriptDelegateBinding>& OutBindings) const
	{
		if (!Delegate.IsBound())
			return false;

		const TArray<UObject*>& BoundObjects = Delegate.GetAllObjects();

		if (BoundObjects.IsEmpty())
			return false;

		OutBindings.Reserve(BoundObjects.Num());

		FString DelegateString = Delegate.ToString<UObject>();
		if (DelegateString.IsEmpty())
			return false;

		// Remove [ and ]
		DelegateString.MidInline(1, DelegateString.Len() - 2);

		TArray<FString> Values;
		DelegateString.ParseIntoArray(Values, TEXT(","));

		if (Values.IsEmpty())
			return false;

		for (int32 i = 0; i < BoundObjects.Num(); i++)
		{
			if (BoundObjects[i] == nullptr)
				continue;

			FMulticastScriptDelegateBinding Binding;
			Binding.Object = BoundObjects[i];

			int32 FunctionNameStart = -1;
			Values[i].FindLastChar('.', FunctionNameStart);

			if (FunctionNameStart >= 0)
			{
				FString FunctionName = Values[i].Mid(FunctionNameStart + 1, Values[i].Len() - 1);
				Binding.FunctionName = MoveTemp(FunctionName);
			}

			OutBindings.Add(Binding);
		}

		if (OutBindings.IsEmpty())
			return false;

		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		FMulticastScriptDelegate& Delegate = Usage.ResolvePrimitive<FMulticastScriptDelegate>(Address);

		if (!Delegate.IsBound())
			return false;

		TArray<FMulticastScriptDelegateBinding> Bindings;
		if (!GetBindings(Delegate, Bindings))
			return false;

		if (Usage.TypeIndex == 0)
		{
			for (int32 i = 0; i < Bindings.Num(); i++)
			{
				if (Bindings[i].Object == nullptr)
					continue;

				FDebuggerValue ElemValue;
				ElemValue.Name = FString::Printf(TEXT("[%d]"), i);
				ElemValue.Usage = Usage;
				ElemValue.Usage.TypeIndex = i + 1;
				ElemValue.Address = Address;
				ElemValue.bHasMembers = true;

				ElemValue.Value = FString::Printf(TEXT("%s.%s"), *Bindings[i].Object->GetName(), *Bindings[i].FunctionName);

				Scope.Values.Add(MoveTemp(ElemValue));
			}
		}
		else
		{
			int32 Index = Usage.TypeIndex - 1;

			if (!Bindings.IsValidIndex(Index))
				return false;

			if (Bindings[Index].Object == nullptr)
				return false;

			FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(UObject::StaticClass()));

			FDebuggerValue ObjectValue;
			const UObject*& ObjectRef = ObjectValue.AllocatePODLiteral<const UObject*>();
			ObjectRef = Bindings[Index].Object;

			ObjectValue.Name = TEXT("Object");
			ObjectValue.Usage = ObjectUsage;
			ObjectValue.Address = &ObjectRef;
			ObjectUsage.GetDebuggerValue(&ObjectRef, ObjectValue);
			Scope.Values.Add(MoveTemp(ObjectValue));

			FDebuggerValue NameValue;
			NameValue.Name = TEXT("Function");
			NameValue.Type = TEXT("FName");
			NameValue.Value = FString::Printf(TEXT("n\"%s\""), *Bindings[Index].FunctionName);
			Scope.Values.Add(MoveTemp(NameValue));
		}

		return true;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		FMulticastScriptDelegate& Delegate = Usage.ResolvePrimitive<FMulticastScriptDelegate>(Address);

		FString DelegateString = Delegate.ToString<UObject>();

		TArray<FMulticastScriptDelegateBinding> Bindings;
		if (!GetBindings(Delegate, Bindings))
			return false;

		if (Usage.TypeIndex == 0)
		{
			if (Member.StartsWith(TEXT("[")) && Member.EndsWith(TEXT("]")))
			{
				int32 Index = -1;
				LexFromString(Index, *Member.Mid(1, Member.Len() - 2));

				if (!Bindings.IsValidIndex(Index))
					return false;

				if (Bindings[Index].Object == nullptr)
					return false;

				Value.Name = FString::Printf(TEXT("[%d]"), Index);
				Value.Usage = Usage;
				Value.Usage.TypeIndex = Index + 1;
				Value.Address = Address;
				Value.bHasMembers = true;
				Value.Value = FString::Printf(TEXT("%s.%s"), *Bindings[Index].Object->GetName(), *Bindings[Index].FunctionName);
				return true;
			}
		}
		else
		{
			int32 Index = Usage.TypeIndex - 1;

			if (!Bindings.IsValidIndex(Index))
				return false;

			if (Member == TEXT("Object"))
			{
				if (Bindings[Index].Object == nullptr)
					return false;

				FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(UObject::StaticClass()));

				const UObject*& ObjectRef = Value.AllocatePODLiteral<const UObject*>();
				ObjectRef = Bindings[Index].Object;

				Value.Name = TEXT("Object");
				Value.Usage = ObjectUsage;
				Value.Address = &ObjectRef;
				ObjectUsage.GetDebuggerValue(&ObjectRef, Value);
				return true;
			}
			else if (Member == TEXT("Function"))
			{
				Value.Name = TEXT("Function");
				Value.Type = TEXT("FName");
				Value.Value = FString::Printf(TEXT("n\"%s\""), *Bindings[Index].FunctionName);
				return true;
			}
		}

		return false;
	}
};

void DeclareMulticastDelegate(UDelegateFunction* Function)
{
	FString Decl = CreateAngelscriptNameForDelegate(Function);
	if (FAngelscriptType::GetByAngelscriptTypeName(Decl).IsValid())
		return;

	auto& BindDB = FAngelscriptBindDatabase::Get();
	BindDB.BoundDelegateFunctions.Add(Function);

	FAngelscriptType::Register(MakeShared<FMulticastScriptDelegateType>(Decl, Function));

	auto Delegate_ = FAngelscriptBinds::ValueClass<FMulticastScriptDelegate>(Decl, FBindFlags());
	Delegate_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Construct));
	Delegate_.Destructor("void f()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Destruct));

	FString CopyDecl = FString::Printf(TEXT("void f(const %s& Other)"), *Decl);
	Delegate_.Constructor(CopyDecl, FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::CopyConstruct));

	FString AssignDecl = FString::Printf(TEXT("%s& opAssign(const %s& Other)"), *Decl, *Decl);
	Delegate_.Method(AssignDecl, FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Assign));
}

void FAngelscriptMulticastDelegateOperations::AddUFunction(FMulticastScriptDelegate* Delegate, asCScriptFunction* Function, UObject* InObject, const FName& InFunctionName)
{
	if (InObject == nullptr)
	{
		FAngelscriptEngine::Throw("Null object passed to BindUFunction.");
		return;
	}

	UFunction* CallFunction = InObject->FindFunction(InFunctionName);
	if (CallFunction == nullptr)
	{
		FAngelscriptEngine::Throw("Could not find function with this name. Is it declared UFUNCTION()?");
		return;
	}

	FDelegateOps* Ops = (FDelegateOps*)Function->userData;
	if (!CheckAngelscriptDelegateCompatibility(Ops->SignatureFunction, CallFunction))
	{
		FString Message = FString::Printf(TEXT("Specified function is not compatible with delegate function.\n\nDelegate: %s\n\nAttempted Bind: %s"),
			*GetSignatureStringForFunction(Ops->SignatureFunction), *GetSignatureStringForFunction(CallFunction));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	FScriptDelegate InnerDelegate;
	InnerDelegate.BindUFunction(InObject, InFunctionName);

	Delegate->AddUnique(InnerDelegate);
}

void FAngelscriptMulticastDelegateOperations::AddUFunction_Signature(FMulticastScriptDelegate* Delegate, UObject* InObject, const FName& InFunctionName, UDelegateFunction* Signature)
{
	if (InObject == nullptr)
	{
		FAngelscriptEngine::Throw("Null object passed to AddUFunction.");
		return;
	}

	if (Signature == nullptr)
	{
		FAngelscriptEngine::Throw("Null signature passed to AddUFunction.");
		return;
	}

	UFunction* CallFunction = InObject->FindFunction(InFunctionName);
	if (CallFunction == nullptr)
	{
		FAngelscriptEngine::Throw("Could not find function in object with this name. Is it declared UFUNCTION()?");
		return;
	}

	if (!CheckAngelscriptDelegateCompatibility(Signature, CallFunction))
	{
		FString Message = FString::Printf(TEXT("Specified function is not compatible with delegate function.\n\nDelegate: %s\n\nAttempted Bind: %s"),
			*GetSignatureStringForFunction(Signature), *GetSignatureStringForFunction(CallFunction));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	FScriptDelegate InnerDelegate;
	InnerDelegate.BindUFunction(InObject, InFunctionName);

	Delegate->AddUnique(InnerDelegate);
}

void DeclareMulticastDelegateOperations(UDelegateFunction* Function)
{
	auto Type = FAngelscriptType::GetByData(Function);
	if (!Type.IsValid())
		return;

	FDelegateOps* Ops = new FDelegateOps;
	Ops->SignatureFunction = Function;

	auto Delegate_ = FAngelscriptBinds::ExistingClass(Type->GetAngelscriptTypeName());

	Delegate_.Method("bool IsBound() const", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::IsBound));
	Delegate_.Method("void Clear()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Clear));

	Delegate_.Method("void AddUFunction(const UObject Object, const FName& FunctionName)", FUNC(FAngelscriptMulticastDelegateOperations::AddUFunction), Ops);
	FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

	Delegate_.Method("void Unbind(UObject Object, const FName& FunctionName)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Unbind));
	Delegate_.Method("void UnbindObject(UObject Object)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::UnbindObject));

	BindDelegateEvent(Delegate_, Function, true, false);
}

struct FScriptSparseDelegateType : public FAngelscriptType
{
	FString Name;
	USparseDelegateFunction* Function;

	FScriptSparseDelegateType(const FString& InName, USparseDelegateFunction* InFunction)
		: Name(InName)
		, Function(InFunction)
	{
	}

	FScriptSparseDelegateType()
		: Function(nullptr)
	{}

	FORCEINLINE USparseDelegateFunction* GetSignature(const FAngelscriptTypeUsage& Usage) const
	{
		if (Function != nullptr)
			return Function;
		check(Usage.ScriptClass != nullptr);
		void* UserData = Usage.ScriptClass->GetUserData();
		check(UserData != FAngelscriptType::TAG_UserData_Delegate);
		check(UserData != FAngelscriptType::TAG_UserData_Multicast_Delegate);
		return (USparseDelegateFunction*)UserData;
	}

	void* GetData() const override
	{
		return Function;
	}

	FString GetAngelscriptTypeName() const override
	{
		return Name;
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		auto* Prop = new FMulticastSparseDelegateProperty(Params.Outer, Params.PropertyName);
		Prop->SignatureFunction = GetSignature(Usage);
		Prop->SetPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable);
		return Prop;
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override
	{
		return false;
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return false;
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		auto* DelegateProp = CastField<FMulticastSparseDelegateProperty>(Property);
		if (DelegateProp == nullptr)
			return false;
		auto* Signature = GetSignature(Usage);
		return DelegateProp->SignatureFunction == Signature;
	}

	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override { return false; }
	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override { return false; }

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return false; }

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override { return false; }

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		return sizeof(FSparseDelegate);
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
	{
		return alignof(FSparseDelegate);
	}
};

void DeclareSparseDelegate(USparseDelegateFunction* Function)
{
	FString Decl = CreateAngelscriptNameForDelegate(Function);

	FAngelscriptType::Register(MakeShared<FScriptSparseDelegateType>(Decl, Function));

	auto& BindDB = FAngelscriptBindDatabase::Get();
	BindDB.BoundDelegateFunctions.Add(Function);

	auto Delegate_ = FAngelscriptBinds::ValueClass<FSparseDelegate>(Decl, FBindFlags());
	Delegate_.Constructor("void f()", [](FSparseDelegate* Delegate)
	{
		new(Delegate) FSparseDelegate();
	});

	Delegate_.Destructor("void f()", [](FSparseDelegate* Delegate)
	{
		Delegate->~FSparseDelegate();
	});
}

void AddSparseDelegateUFunction(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* InObject, const FName& InFunctionName)
{
	if (InObject == nullptr)
	{
		FAngelscriptEngine::Throw("Null object passed to BindUFunction.");
		return;
	}

	UFunction* CallFunction = InObject->FindFunction(InFunctionName);
	if (CallFunction == nullptr)
	{
		FAngelscriptEngine::Throw("Could not find function with this name. Is it declared UFUNCTION()?");
		return;
	}

	FDelegateOps* Ops = (FDelegateOps*)ScriptFunction->userData;
	if (!CheckAngelscriptDelegateCompatibility(Ops->SignatureFunction, CallFunction))
	{
		FString Message = FString::Printf(TEXT("Specified function is not compatible with delegate function.\n\nDelegate: %s\n\nAttempted Bind: %s"),
			*GetSignatureStringForFunction(Ops->SignatureFunction), *GetSignatureStringForFunction(CallFunction));
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	FScriptDelegate InnerDelegate;
	InnerDelegate.BindUFunction(InObject, InFunctionName);

	USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Ops->SignatureFunction);
	UObject* OwningObject = FSparseDelegateStorage::ResolveSparseOwner(*Delegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

	Delegate->__Internal_AddUnique(OwningObject, SparseDelegateFunc->DelegateName, InnerDelegate);
}

void DeclareSparseDelegateOperations(USparseDelegateFunction* Function)
{
	FDelegateOps* Ops = new FDelegateOps;
	Ops->SignatureFunction = Function;

	FString Decl = CreateAngelscriptNameForDelegate(Function);

	auto Delegate_ = FAngelscriptBinds::ExistingClass(Decl);

	Delegate_.Method("bool IsBound() const", [](FSparseDelegate* Delegate)
	{
		return Delegate->IsBound();
	});

	Delegate_.Method("void Clear()", [](FSparseDelegate* Delegate, asCScriptFunction* Function)
	{
		FDelegateOps* Ops = (FDelegateOps*)Function->userData;

		USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Ops->SignatureFunction);
		UObject* OwningObject = FSparseDelegateStorage::ResolveSparseOwner(*Delegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

		Delegate->__Internal_Clear(OwningObject, SparseDelegateFunc->DelegateName);
	}, Ops);
	FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

	Delegate_.Method("void AddUFunction(UObject Object, const FName& FunctionName)", AddSparseDelegateUFunction, Ops);
	FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

	Delegate_.Method("void Unbind(UObject Object, const FName& FunctionName)",
	[](FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* Object, const FName& InFunctionName)
	{
		FDelegateOps* Ops = (FDelegateOps*)ScriptFunction->userData;

		FScriptDelegate InnerDelegate;
		InnerDelegate.BindUFunction(Object, InFunctionName);

		USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Ops->SignatureFunction);
		UObject* OwningObject = FSparseDelegateStorage::ResolveSparseOwner(*Delegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

		Delegate->__Internal_Remove(OwningObject, SparseDelegateFunc->DelegateName, InnerDelegate);

	}, Ops);
	FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

	Delegate_.Method("void UnbindObject(UObject Object)",
	[](FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* Object, const FName& InFunctionName)
	{
		FDelegateOps* Ops = (FDelegateOps*)ScriptFunction->userData;

		USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Ops->SignatureFunction);
		UObject* OwningObject = FSparseDelegateStorage::ResolveSparseOwner(*Delegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

		FSparseDelegateStorage::RemoveAll(OwningObject, SparseDelegateFunc->DelegateName, Object);
	}, Ops);
	FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

	BindDelegateEvent(Delegate_, Function, true, true);
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Delegate_Declarations(FAngelscriptBinds::EOrder::Early, []
{
	FAngelscriptScopeTimer Timer(TEXT("delegate declarations"));

	auto DelegateInternal = MakeShared<FScriptDelegateType>();
	FAngelscriptType::SetScriptDelegate(DelegateInternal);
	FAngelscriptType::Register(DelegateInternal);

	auto MulticastInternal = MakeShared<FMulticastScriptDelegateType>();
	FAngelscriptType::SetScriptMulticastDelegate(MulticastInternal);
	FAngelscriptType::Register(MulticastInternal);

	for (UDelegateFunction* Function : TObjectRange<UDelegateFunction>())
	{
		if (!Function->HasAnyFunctionFlags(FUNC_Delegate))
			continue;
		if (auto* SparseFunction = Cast<USparseDelegateFunction>(Function))
			DeclareSparseDelegate(SparseFunction);
		else if (Function->HasAnyFunctionFlags(FUNC_MulticastDelegate))
			DeclareMulticastDelegate(Function);
		else
			DeclareDelegate(Function);
	}

	FAngelscriptType::RegisterTypeFinder([](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(Property);
		if (DelegateProperty == nullptr)
			return false;

		auto Type = FAngelscriptType::GetByData(DelegateProperty->SignatureFunction);
		if (Type.IsValid())
		{
			Usage.Type = Type;
			return true;
		}

		return false;
	});

	FAngelscriptType::RegisterTypeFinder([](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FMulticastDelegateProperty* DelegateProperty = CastField<FMulticastDelegateProperty>(Property);
		if (DelegateProperty == nullptr)
			return false;

		auto Type = FAngelscriptType::GetByData(DelegateProperty->SignatureFunction);
		if (Type.IsValid())
		{
			Usage.Type = Type;
			return true;
		}

		return false;
	});

	FBindFlags BindFlags;

	auto Delegate_ = FAngelscriptBinds::ValueClass<FScriptDelegate>("_FScriptDelegate", BindFlags);
	Delegate_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Construct));
	Delegate_.Destructor("void f()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Destruct));

	Delegate_.Constructor("void f(const _FScriptDelegate& Other)", FUNC_TRIVIAL(FAngelscriptDelegateOperations::CopyConstruct));
	Delegate_.Method("_FScriptDelegate& opAssign(const _FScriptDelegate& Other)", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Assign));

	Delegate_.Method("bool IsBound() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::IsBound));
	Delegate_.Method("void Clear() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Clear));

	auto MulticastDelegate_ = FAngelscriptBinds::ValueClass<FMulticastScriptDelegate>("_FMulticastScriptDelegate", FBindFlags());
	MulticastDelegate_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Construct));
	MulticastDelegate_.Destructor("void f()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Destruct));

	MulticastDelegate_.Constructor("void f(const _FMulticastScriptDelegate& Other)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::CopyConstruct));
	MulticastDelegate_.Method("_FMulticastScriptDelegate& opAssign(const _FMulticastScriptDelegate& Other)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Assign));

	MulticastDelegate_.Method("bool IsBound() const", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::IsBound));
	MulticastDelegate_.Method("void Clear()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Clear));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Delegates(FAngelscriptBinds::EOrder::Late, []
{
	FAngelscriptScopeTimer Timer(TEXT("delegate bindings"));

	for (UDelegateFunction* Function : TObjectRange<UDelegateFunction>())
	{
		if (!Function->HasAnyFunctionFlags(FUNC_Delegate))
			continue;
		if (auto* SparseFunction = Cast<USparseDelegateFunction>(Function))
			DeclareSparseDelegateOperations(SparseFunction);
		else if (Function->HasAnyFunctionFlags(FUNC_MulticastDelegate))
			DeclareMulticastDelegateOperations(Function);
		else
			DeclareDelegateOperations(Function);
	}

	// A way to look up the delegate signature from an arbitrary script delegate type,
	// this is used by auto-generated code to bind to generic script delegates.
	FAngelscriptBinds::BindGlobalFunction("UDelegateFunction __DelegateSignature(?& Delegate)", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetDelegateSignature));

	auto MulticastDelegate_ = FAngelscriptBinds::ExistingClass("_FMulticastScriptDelegate");

	MulticastDelegate_.Method("void AddUFunction(const UObject Object, const FName& FunctionName, UDelegateFunction Signature)", FUNC(FAngelscriptMulticastDelegateOperations::AddUFunction_Signature));
	MulticastDelegate_.Method("void Unbind(UObject Object, const FName& FunctionName)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Unbind));
	MulticastDelegate_.Method("void UnbindObject(UObject Object)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::UnbindObject));

	auto Delegate_ = FAngelscriptBinds::ExistingClass("_FScriptDelegate");
	Delegate_.Method("void BindUFunction(UObject Object, const FName& FunctionName, UDelegateFunction Signature)", FUNC(FAngelscriptDelegateOperations::BindUFunction_Signature));

	Delegate_.Constructor("void f(UObject Object, const FName& FunctionName, UDelegateFunction Signature)", FUNC(FAngelscriptDelegateOperations::ConstructFromFunction_Signature));
	Delegate_.Method("UObject GetUObject() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetUObject));
	Delegate_.Method("FName GetFunctionName() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetFunctionName));
});
