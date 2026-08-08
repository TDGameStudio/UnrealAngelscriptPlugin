#include "Binds/Bind_Delegates.h"

#include "AngelscriptBindDatabase.h"
#include "AngelscriptDebugValue.h"
#include "AngelscriptEngine.h"

#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

static FString CreateCppNameForDelegate(UDelegateFunction* Function)
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

FScriptDelegateType::FScriptDelegateType(
	const FString& InName,
	UDelegateFunction* InFunction,
	const FAngelscriptBindDatabase& InBindDatabase)
	: Name(InName)
	, Function(InFunction)
	, BindDatabase(&InBindDatabase)
{
}

FScriptDelegateType::FScriptDelegateType(const FAngelscriptBindDatabase& InBindDatabase)
	: Name(TEXT("_FScriptDelegate"))
	, Function(nullptr)
	, BindDatabase(&InBindDatabase)
{}

UDelegateFunction* FScriptDelegateType::GetSignature(const FAngelscriptTypeUsage& Usage) const
{
	if (Function != nullptr)
		return Function;
	check(Usage.ScriptClass != nullptr);
	void* UserData = Usage.ScriptClass->GetUserData();
	check(UserData != FAngelscriptType::TAG_UserData_Delegate);
	check(UserData != FAngelscriptType::TAG_UserData_Multicast_Delegate);
	return (UDelegateFunction*)UserData;
}

UDelegateFunction* FScriptDelegateType::GetSignatureMaybeTagged(const FAngelscriptTypeUsage& Usage) const
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

bool FScriptDelegateType::IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const
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

void* FScriptDelegateType::GetData() const
{
	return Function;
}

FString FScriptDelegateType::GetAngelscriptTypeName() const
{
	return Name;
}

bool FScriptDelegateType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

FProperty* FScriptDelegateType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
{
	auto* Prop = new FDelegateProperty(Params.Outer, Params.PropertyName);
	Prop->SignatureFunction = GetSignature(Usage);
	return Prop;
}

bool FScriptDelegateType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

void FScriptDelegateType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
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

bool FScriptDelegateType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return !Usage.bIsReference;
}

void FScriptDelegateType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
{
	void* ReturnedObject = Context->GetReturnObject();
	if (ReturnedObject == nullptr)
		return;
	*(FScriptDelegate*)Destination = *(FScriptDelegate*)ReturnedObject;
}

bool FScriptDelegateType::CanQueryPropertyType() const
{
	return false;
}

bool FScriptDelegateType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
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

bool FScriptDelegateType::DefaultValue_AngelscriptFallback(const FAngelscriptTypeUsage& Usage, FString& OutAngelscriptValue) const
{
	OutAngelscriptValue = Name + TEXT("()");
	return true;
}

bool FScriptDelegateType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	if (Function == nullptr)
	{
		OutCppForm.bNativeCannotBeGeneric = true;
		OutCppForm.TemplateObjectForm = TEXT("FScriptDelegate");
		return true;
	}

	OutCppForm.CppType = CreateCppNameForDelegate(Function);
	FString HeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Function, *BindDatabase);
	if (HeaderPath.Len() != 0 && !HeaderPath.Contains(TEXT("NoExportTypes.h")))
	{
		OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *HeaderPath);
	}
	OutCppForm.bNativeCannotBeGeneric = true;
	OutCppForm.TemplateObjectForm = TEXT("FScriptDelegate");
	return true;
}

bool FScriptDelegateType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
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

bool FScriptDelegateType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
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

bool FScriptDelegateType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
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

FMulticastScriptDelegateType::FMulticastScriptDelegateType(
	const FString& InName,
	UDelegateFunction* InFunction,
	const FAngelscriptBindDatabase& InBindDatabase)
	: Name(InName)
	, Function(InFunction)
	, BindDatabase(&InBindDatabase)
{
}

FMulticastScriptDelegateType::FMulticastScriptDelegateType(const FAngelscriptBindDatabase& InBindDatabase)
	: Name(TEXT("_FMulticastScriptDelegate"))
	, Function(nullptr)
	, BindDatabase(&InBindDatabase)
{}

UDelegateFunction* FMulticastScriptDelegateType::GetSignature(const FAngelscriptTypeUsage& Usage) const
{
	if (Function != nullptr)
		return Function;
	check(Usage.ScriptClass != nullptr);
	void* UserData = Usage.ScriptClass->GetUserData();
	check(UserData != FAngelscriptType::TAG_UserData_Delegate);
	check(UserData != FAngelscriptType::TAG_UserData_Multicast_Delegate);
	return (UDelegateFunction*)UserData;
}

UDelegateFunction* FMulticastScriptDelegateType::GetSignatureMaybeTagged(const FAngelscriptTypeUsage& Usage) const
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

bool FMulticastScriptDelegateType::IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const
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

void* FMulticastScriptDelegateType::GetData() const
{
	return Function;
}

FString FMulticastScriptDelegateType::GetAngelscriptTypeName() const
{
	return Name;
}

bool FMulticastScriptDelegateType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

FProperty* FMulticastScriptDelegateType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
{
	auto* Prop = new FMulticastInlineDelegateProperty(Params.Outer, Params.PropertyName);
	Prop->SignatureFunction = GetSignature(Usage);
	Prop->SetPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable);
	return Prop;
}

bool FMulticastScriptDelegateType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{
	return !Usage.bIsReference;
}

void FMulticastScriptDelegateType::SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const
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

bool FMulticastScriptDelegateType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return !Usage.bIsReference;
}

void FMulticastScriptDelegateType::GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const
{
	void* ReturnedObject = Context->GetReturnObject();
	if (ReturnedObject == nullptr)
		return;
	*(FMulticastScriptDelegate*)Destination = *(FMulticastScriptDelegate*)ReturnedObject;
}

bool FMulticastScriptDelegateType::CanQueryPropertyType() const
{
	return false;
}

bool FMulticastScriptDelegateType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
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

bool FMulticastScriptDelegateType::DefaultValue_AngelscriptFallback(const FAngelscriptTypeUsage& Usage, FString& OutAngelscriptValue) const
{
	OutAngelscriptValue = Name + TEXT("()");
	return true;
}

bool FMulticastScriptDelegateType::GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const
{
	if (Function == nullptr)
	{
		OutCppForm.bNativeCannotBeGeneric = true;
		OutCppForm.TemplateObjectForm = TEXT("FMulticastScriptDelegate");
		return true;
	}

	OutCppForm.CppType = CreateCppNameForDelegate(Function);
	FString HeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Function, *BindDatabase);
	if (HeaderPath.Len() != 0 && !HeaderPath.Contains(TEXT("NoExportTypes.h")))
	{
		OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *HeaderPath);
	}
	OutCppForm.bNativeCannotBeGeneric = true;
	OutCppForm.TemplateObjectForm = TEXT("FMulticastScriptDelegate");
	return true;
}

bool FMulticastScriptDelegateType::GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const
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

bool FMulticastScriptDelegateType::GetBindings(const FMulticastScriptDelegate& Delegate, TArray<FMulticastScriptDelegateBinding>& OutBindings) const
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

bool FMulticastScriptDelegateType::GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const
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

bool FMulticastScriptDelegateType::GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const
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

FScriptSparseDelegateType::FScriptSparseDelegateType(const FString& InName, USparseDelegateFunction* InFunction)
	: Name(InName)
	, Function(InFunction)
{
}

FScriptSparseDelegateType::FScriptSparseDelegateType()
	: Function(nullptr)
{}

USparseDelegateFunction* FScriptSparseDelegateType::GetSignature(const FAngelscriptTypeUsage& Usage) const
{
	if (Function != nullptr)
		return Function;
	check(Usage.ScriptClass != nullptr);
	void* UserData = Usage.ScriptClass->GetUserData();
	check(UserData != FAngelscriptType::TAG_UserData_Delegate);
	check(UserData != FAngelscriptType::TAG_UserData_Multicast_Delegate);
	return (USparseDelegateFunction*)UserData;
}

void* FScriptSparseDelegateType::GetData() const
{
	return Function;
}

FString FScriptSparseDelegateType::GetAngelscriptTypeName() const
{
	return Name;
}

bool FScriptSparseDelegateType::CanCreateProperty(const FAngelscriptTypeUsage& Usage) const
{
	return true;
}

FProperty* FScriptSparseDelegateType::CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const
{
	auto* Prop = new FMulticastSparseDelegateProperty(Params.Outer, Params.PropertyName);
	Prop->SignatureFunction = GetSignature(Usage);
	Prop->SetPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable);
	return Prop;
}

bool FScriptSparseDelegateType::CanBeArgument(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FScriptSparseDelegateType::CanBeReturned(const FAngelscriptTypeUsage& Usage) const
{
	return false;
}

bool FScriptSparseDelegateType::CanQueryPropertyType() const
{
	return false;
}

bool FScriptSparseDelegateType::MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const
{
	auto* DelegateProp = CastField<FMulticastSparseDelegateProperty>(Property);
	if (DelegateProp == nullptr)
		return false;
	auto* Signature = GetSignature(Usage);
	return DelegateProp->SignatureFunction == Signature;
}

bool FScriptSparseDelegateType::CanCopy(const FAngelscriptTypeUsage& Usage) const
{ return false; }

bool FScriptSparseDelegateType::CanCompare(const FAngelscriptTypeUsage& Usage) const
{ return false; }

bool FScriptSparseDelegateType::CanConstruct(const FAngelscriptTypeUsage& Usage) const
{ return true; }

bool FScriptSparseDelegateType::NeedConstruct(const FAngelscriptTypeUsage& Usage) const
{ return false; }

bool FScriptSparseDelegateType::CanDestruct(const FAngelscriptTypeUsage& Usage) const
{ return true; }

bool FScriptSparseDelegateType::NeedDestruct(const FAngelscriptTypeUsage& Usage) const
{ return false; }

int32 FScriptSparseDelegateType::GetValueSize(const FAngelscriptTypeUsage& Usage) const
{
	return sizeof(FSparseDelegate);
}

int32 FScriptSparseDelegateType::GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
{
	return alignof(FSparseDelegate);
}
