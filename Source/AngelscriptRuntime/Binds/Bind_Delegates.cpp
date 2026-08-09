#include "Binds/Bind_Delegates.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

/**
 * Reflected single-cast, multicast, sparse, and signature-erased delegate surfaces.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <SingleCastDelegate> Delegate();                                                                     | Constructs an unbound reflected single-cast delegate. Expanded for every eligible non-multicast                  |
 * |                                                                                                      | UDelegateFunction.                                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <SingleCastDelegate> Delegate(const <SingleCastDelegate>& Other);                                    | Copy-constructs a single-cast delegate binding.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate = Other;                                                                                    | Copies a single-cast delegate binding.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <SingleCastDelegate> Delegate(UObject Object, const FName& FunctionName);                            | Constructs and binds a single-cast delegate.                                                                     |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bBound = Delegate.IsBound() const;                                                              | Reports whether the delegate has a live target.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | UObject Object = Delegate.GetUObject() const;                                                        | Returns the bound target object, or null when unbound.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Name = Delegate.GetFunctionName() const;                                                       | Returns the bound UFUNCTION name.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Clear();                                                                                    | Removes the current binding.                                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.BindUFunction(UObject Object, const FName& FunctionName);                                   | Binds a compatible reflected function.                                                                           |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <ReturnType> Result = Delegate.Execute(<SignatureParameters>);                                       | Invokes the bound single-cast delegate. Emitted from the reflected signature when all parameter types are        |
 * |                                                                                                      | supported and the argument count fits AS_EVENT_MAX_ARGS.                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <ReturnType> Result = Delegate.ExecuteIfBound(<SignatureParameters>);                                | Invokes the single-cast delegate only when bound, using the reflected return policy when unbound.                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <MulticastDelegate> Delegate();                                                                      | Constructs an empty reflected multicast delegate. Expanded for every eligible multicast UDelegateFunction.       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <MulticastDelegate> Delegate(const <MulticastDelegate>& Other);                                      | Copy-constructs a multicast invocation list.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate = Other;                                                                                    | Copies a multicast invocation list.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bBound = Delegate.IsBound() const;                                                              | Reports whether the multicast delegate has at least one live target.                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Clear();                                                                                    | Removes every multicast binding.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.AddUFunction(const UObject Object, const FName& FunctionName);                              | Adds a compatible reflected function once.                                                                       |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Unbind(UObject Object, const FName& FunctionName);                                          | Removes the matching object/function binding.                                                                    |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.UnbindObject(UObject Object);                                                               | Removes every binding owned by an object.                                                                        |
 * |                                                                                                      | @param Object UObject whose bindings are removed.                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Broadcast(<SignatureParameters>);                                                           | Invokes the multicast invocation list. Emitted from each supported reflected signature.                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <SparseDelegate> Delegate();                                                                         | Constructs an empty reflected sparse delegate. Expanded for every eligible USparseDelegateFunction.              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bBound = Delegate.IsBound() const;                                                              | Reports whether sparse storage contains a live target.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Clear();                                                                                    | Removes every sparse binding from the resolved owner.                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.AddUFunction(UObject Object, const FName& FunctionName);                                    | Adds a compatible reflected function to sparse storage.                                                          |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Unbind(UObject Object, const FName& FunctionName);                                          | Removes a matching sparse object/function binding.                                                               |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.UnbindObject(UObject Object);                                                               | Removes an object's sparse bindings.                                                                             |
 * |                                                                                                      | @param Object UObject whose sparse bindings are removed.                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Broadcast(<SignatureParameters>);                                                           | Invokes the sparse invocation list. Emitted from each supported reflected signature.                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | _FScriptDelegate Delegate();                                                                         | Constructs an unbound signature-erased single-cast delegate.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | _FScriptDelegate Delegate(const _FScriptDelegate& Other);                                            | Copy-constructs a signature-erased single-cast delegate.                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate = Other;                                                                                    | Copies a signature-erased single-cast binding.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bBound = Delegate.IsBound() const;                                                              | Reports whether the signature-erased delegate is bound.                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Clear() const;                                                                              | Removes the signature-erased delegate binding.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | _FScriptDelegate Delegate(UObject Object, const FName& FunctionName, UDelegateFunction Signature);   | Constructs a signature-erased delegate and validates its target.                                                 |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * |                                                                                                      | @param Signature Reflected delegate signature used to validate the target function.                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.BindUFunction(UObject Object, const FName& FunctionName, UDelegateFunction Signature);      | Binds and validates a signature-erased delegate target.                                                          |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * |                                                                                                      | @param Signature Reflected delegate signature used to validate the target function.                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | UObject Object = Delegate.GetUObject() const;                                                        | Returns the signature-erased delegate's target object.                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Name = Delegate.GetFunctionName() const;                                                       | Returns the signature-erased delegate's target UFUNCTION name.                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | _FMulticastScriptDelegate Delegate();                                                                | Constructs an empty signature-erased multicast delegate.                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | _FMulticastScriptDelegate Delegate(const _FMulticastScriptDelegate& Other);                          | Copy-constructs a signature-erased multicast invocation list.                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate = Other;                                                                                    | Copies a signature-erased multicast invocation list.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bBound = Delegate.IsBound() const;                                                              | Reports whether the signature-erased multicast delegate has a live target.                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Clear();                                                                                    | Removes every signature-erased multicast binding.                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.AddUFunction(const UObject Object, const FName& FunctionName, UDelegateFunction Signature); | Adds and validates a signature-erased multicast target.                                                          |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * |                                                                                                      | @param Signature Reflected delegate signature used to validate the target function.                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.Unbind(UObject Object, const FName& FunctionName);                                          | Removes a signature-erased multicast object/function binding.                                                    |
 * |                                                                                                      | @param Object UObject that owns the target UFUNCTION.                                                            |
 * |                                                                                                      | @param FunctionName Reflected UFUNCTION name; its signature must be compatible.                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Delegate.UnbindObject(UObject Object);                                                               | Removes every signature-erased multicast binding for an object.                                                  |
 * |                                                                                                      | @param Object UObject whose bindings are removed.                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | UDelegateFunction Signature = __DelegateSignature(?& Delegate);                                      | Returns the reflected signature for an arbitrary delegate value.                                                 |
 * |                                                                                                      | @param Delegate Single-cast or multicast delegate whose signature is queried.                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

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

void DeclareDelegate(FAngelscriptBinds& Binds, UDelegateFunction* Function)
{
	FString Decl = CreateAngelscriptNameForDelegate(Function);

	FAngelscriptBindDatabase& BindDB = Binds.GetTargetBindDatabase();
	BindDB.BoundDelegateFunctions.Add(Function);

	Binds.RegisterTypeForTarget(MakeShared<FScriptDelegateType>(
		Decl,
		Function,
		Binds.GetTargetBindDatabase()));

	FBindFlags BindFlags;
	auto Delegate_ = Binds.ValueClassForTarget<FScriptDelegate>(Decl, BindFlags);
	Delegate_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Construct)).NoDiscard();
	Delegate_.Destructor("void f()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Destruct));

	FString CopyDecl = FString::Printf(TEXT("void f(const %s& Other)"), *Decl);
	Delegate_.Constructor(CopyDecl, FUNC_TRIVIAL(FAngelscriptDelegateOperations::CopyConstruct)).NoDiscard();

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

void DeclareDelegateOperations(FAngelscriptBinds& Binds, UDelegateFunction* Function)
{
	FDelegateOps* Ops = new FDelegateOps;
	Ops->SignatureFunction = Function;

	FString Decl = CreateAngelscriptNameForDelegate(Function);

	auto Delegate_ = Binds.ExistingClassForTarget(Decl);

	Delegate_.Method("bool IsBound() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::IsBound));
	Delegate_.Method("UObject GetUObject() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetUObject));
	Delegate_.Method("FName GetFunctionName() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetFunctionName));

	Delegate_.Method("void Clear()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Clear));

	Delegate_.Constructor("void f(UObject Object, const FName& FunctionName)", FUNC(FAngelscriptDelegateOperations::ConstructFromFunction), Ops)
		.PassScriptFunctionAsFirstParam()
		.NoDiscard();

	Delegate_.Method("void BindUFunction(UObject Object, const FName& FunctionName)", FUNC(FAngelscriptDelegateOperations::BindUFunction), Ops)
		.PassScriptFunctionAsFirstParam();

	BindDelegateEvent(Delegate_, Function, false, false);
}



void DeclareMulticastDelegate(FAngelscriptBinds& Binds, UDelegateFunction* Function)
{
	FString Decl = CreateAngelscriptNameForDelegate(Function);
	if (Binds.GetTargetTypeDatabase().TypesByAngelscriptName.Contains(Decl))
		return;

	FAngelscriptBindDatabase& BindDB = Binds.GetTargetBindDatabase();
	BindDB.BoundDelegateFunctions.Add(Function);

	Binds.RegisterTypeForTarget(MakeShared<FMulticastScriptDelegateType>(
		Decl,
		Function,
		Binds.GetTargetBindDatabase()));

	auto Delegate_ = Binds.ValueClassForTarget<FMulticastScriptDelegate>(Decl, FBindFlags());
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

void DeclareMulticastDelegateOperations(FAngelscriptBinds& Binds, UDelegateFunction* Function)
{
	const TSharedRef<FAngelscriptType>* RegisteredType = Binds.GetTargetTypeDatabase().TypesByData.Find(Function);
	if (RegisteredType == nullptr)
		return;
	const TSharedPtr<FAngelscriptType> Type = RegisteredType->ToSharedPtr();

	FDelegateOps* Ops = new FDelegateOps;
	Ops->SignatureFunction = Function;

	auto Delegate_ = Binds.ExistingClassForTarget(Type->GetAngelscriptTypeName());

	Delegate_.Method("bool IsBound() const", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::IsBound));
	Delegate_.Method("void Clear()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Clear));

	Delegate_.Method("void AddUFunction(const UObject Object, const FName& FunctionName)", FUNC(FAngelscriptMulticastDelegateOperations::AddUFunction), Ops)
		.PassScriptFunctionAsFirstParam();

	Delegate_.Method("void Unbind(UObject Object, const FName& FunctionName)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Unbind));
	Delegate_.Method("void UnbindObject(UObject Object)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::UnbindObject));

	BindDelegateEvent(Delegate_, Function, true, false);
}



void DeclareSparseDelegate(FAngelscriptBinds& Binds, USparseDelegateFunction* Function)
{
	FString Decl = CreateAngelscriptNameForDelegate(Function);

	Binds.RegisterTypeForTarget(MakeShared<FScriptSparseDelegateType>(Decl, Function));

	FAngelscriptBindDatabase& BindDB = Binds.GetTargetBindDatabase();
	BindDB.BoundDelegateFunctions.Add(Function);

	auto Delegate_ = Binds.ValueClassForTarget<FSparseDelegate>(Decl, FBindFlags());
	Delegate_.Constructor("void f()", &FAngelscriptSparseDelegateOperations::Construct);
	Delegate_.Destructor("void f()", &FAngelscriptSparseDelegateOperations::Destruct);
}

void FAngelscriptSparseDelegateOperations::Construct(FSparseDelegate* Delegate)
{
	new (Delegate) FSparseDelegate();
}

void FAngelscriptSparseDelegateOperations::Destruct(FSparseDelegate* Delegate)
{
	Delegate->~FSparseDelegate();
}

bool FAngelscriptSparseDelegateOperations::IsBound(FSparseDelegate* Delegate)
{
	return Delegate->IsBound();
}

void FAngelscriptSparseDelegateOperations::AddUFunction(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* InObject, const FName& InFunctionName)
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

void FAngelscriptSparseDelegateOperations::Clear(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction)
{
	FDelegateOps* Ops = static_cast<FDelegateOps*>(ScriptFunction->userData);

	USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Ops->SignatureFunction);
	UObject* OwningObject = FSparseDelegateStorage::ResolveSparseOwner(*Delegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

	Delegate->__Internal_Clear(OwningObject, SparseDelegateFunc->DelegateName);
}

void FAngelscriptSparseDelegateOperations::Unbind(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* Object, const FName& InFunctionName)
{
	FDelegateOps* Ops = static_cast<FDelegateOps*>(ScriptFunction->userData);

	FScriptDelegate InnerDelegate;
	InnerDelegate.BindUFunction(Object, InFunctionName);

	USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Ops->SignatureFunction);
	UObject* OwningObject = FSparseDelegateStorage::ResolveSparseOwner(*Delegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

	Delegate->__Internal_Remove(OwningObject, SparseDelegateFunc->DelegateName, InnerDelegate);
}

void FAngelscriptSparseDelegateOperations::UnbindObject(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* Object, const FName&)
{
	FDelegateOps* Ops = static_cast<FDelegateOps*>(ScriptFunction->userData);

	USparseDelegateFunction* SparseDelegateFunc = CastChecked<USparseDelegateFunction>(Ops->SignatureFunction);
	UObject* OwningObject = FSparseDelegateStorage::ResolveSparseOwner(*Delegate, SparseDelegateFunc->OwningClassName, SparseDelegateFunc->DelegateName);

	FSparseDelegateStorage::RemoveAll(OwningObject, SparseDelegateFunc->DelegateName, Object);
}

void DeclareSparseDelegateOperations(FAngelscriptBinds& Binds, USparseDelegateFunction* Function)
{
	FDelegateOps* Ops = new FDelegateOps;
	Ops->SignatureFunction = Function;

	FString Decl = CreateAngelscriptNameForDelegate(Function);

	auto Delegate_ = Binds.ExistingClassForTarget(Decl);

	Delegate_.Method("bool IsBound() const", &FAngelscriptSparseDelegateOperations::IsBound);
	Delegate_.Method("void Clear()", &FAngelscriptSparseDelegateOperations::Clear, Ops).PassScriptFunctionAsFirstParam();
	Delegate_.Method("void AddUFunction(UObject Object, const FName& FunctionName)", &FAngelscriptSparseDelegateOperations::AddUFunction, Ops)
		.PassScriptFunctionAsFirstParam();
	Delegate_.Method("void Unbind(UObject Object, const FName& FunctionName)", &FAngelscriptSparseDelegateOperations::Unbind, Ops)
		.PassScriptFunctionAsFirstParam();
	Delegate_.Method("void UnbindObject(UObject Object)", &FAngelscriptSparseDelegateOperations::UnbindObject, Ops)
		.PassScriptFunctionAsFirstParam();

	BindDelegateEvent(Delegate_, Function, true, true);
}

AS_FORCE_LINK const FAngelscriptBind Bind_Delegate_Declarations(
	TEXT("Delegates.Declarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptScopeTimer Timer(TEXT("delegate declarations"));

		auto DelegateInternal = MakeShared<FScriptDelegateType>(Binds.GetTargetBindDatabase());
		Binds.GetTargetTypeDatabase().ScriptDelegateType = DelegateInternal;
		Binds.RegisterTypeForTarget(DelegateInternal);

		auto MulticastInternal = MakeShared<FMulticastScriptDelegateType>(Binds.GetTargetBindDatabase());
		Binds.GetTargetTypeDatabase().ScriptMulticastDelegateType = MulticastInternal;
		Binds.RegisterTypeForTarget(MulticastInternal);

		for (UDelegateFunction* Function : TObjectRange<UDelegateFunction>())
		{
			if (!Function->HasAnyFunctionFlags(FUNC_Delegate))
				continue;
			if (auto* SparseFunction = Cast<USparseDelegateFunction>(Function))
				DeclareSparseDelegate(Binds, SparseFunction);
			else if (Function->HasAnyFunctionFlags(FUNC_MulticastDelegate))
				DeclareMulticastDelegate(Binds, Function);
			else
				DeclareDelegate(Binds, Function);
		}

		FAngelscriptTypeDatabase* TargetTypeDatabase = &Binds.GetTargetTypeDatabase();
		Binds.RegisterTypeFinderForTarget([TargetTypeDatabase](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(Property);
			if (DelegateProperty == nullptr)
				return false;

			const TSharedRef<FAngelscriptType>* RegisteredType = TargetTypeDatabase->TypesByData.Find(DelegateProperty->SignatureFunction);
			if (RegisteredType != nullptr)
			{
				Usage.Type = RegisteredType->ToSharedPtr();
				return true;
			}

			return false;
		});

		Binds.RegisterTypeFinderForTarget([TargetTypeDatabase](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			FMulticastDelegateProperty* DelegateProperty = CastField<FMulticastDelegateProperty>(Property);
			if (DelegateProperty == nullptr)
				return false;

			const TSharedRef<FAngelscriptType>* RegisteredType = TargetTypeDatabase->TypesByData.Find(DelegateProperty->SignatureFunction);
			if (RegisteredType != nullptr)
			{
				Usage.Type = RegisteredType->ToSharedPtr();
				return true;
			}

			return false;
		});

		FBindFlags BindFlags;

		auto Delegate_ = Binds.ValueClassForTarget<FScriptDelegate>("_FScriptDelegate", BindFlags);
		Delegate_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Construct));
		Delegate_.Destructor("void f()", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Destruct));

		Delegate_.Constructor("void f(const _FScriptDelegate& Other)", FUNC_TRIVIAL(FAngelscriptDelegateOperations::CopyConstruct));
		Delegate_.Method("_FScriptDelegate& opAssign(const _FScriptDelegate& Other)", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Assign));

		Delegate_.Method("bool IsBound() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::IsBound));
		Delegate_.Method("void Clear() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::Clear));

		auto MulticastDelegate_ = Binds.ValueClassForTarget<FMulticastScriptDelegate>("_FMulticastScriptDelegate", FBindFlags());
		MulticastDelegate_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Construct));
		MulticastDelegate_.Destructor("void f()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Destruct));

		MulticastDelegate_.Constructor("void f(const _FMulticastScriptDelegate& Other)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::CopyConstruct));
		MulticastDelegate_.Method("_FMulticastScriptDelegate& opAssign(const _FMulticastScriptDelegate& Other)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Assign));

		MulticastDelegate_.Method("bool IsBound() const", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::IsBound));
		MulticastDelegate_.Method("void Clear()", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Clear));
	});

AS_FORCE_LINK const FAngelscriptBind Bind_Delegates(
	TEXT("Delegates.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptScopeTimer Timer(TEXT("delegate bindings"));

		for (UDelegateFunction* Function : TObjectRange<UDelegateFunction>())
		{
			if (!Function->HasAnyFunctionFlags(FUNC_Delegate))
				continue;
			if (auto* SparseFunction = Cast<USparseDelegateFunction>(Function))
				DeclareSparseDelegateOperations(Binds, SparseFunction);
			else if (Function->HasAnyFunctionFlags(FUNC_MulticastDelegate))
				DeclareMulticastDelegateOperations(Binds, Function);
			else
				DeclareDelegateOperations(Binds, Function);
		}

		// A way to look up the delegate signature from an arbitrary script delegate type,
		// this is used by auto-generated code to bind to generic script delegates.
		Binds.BindGlobalFunctionForTarget("UDelegateFunction __DelegateSignature(?& Delegate)", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetDelegateSignature));

		auto MulticastDelegate_ = Binds.ExistingClassForTarget("_FMulticastScriptDelegate");

		MulticastDelegate_.Method("void AddUFunction(const UObject Object, const FName& FunctionName, UDelegateFunction Signature)", FUNC(FAngelscriptMulticastDelegateOperations::AddUFunction_Signature));
		MulticastDelegate_.Method("void Unbind(UObject Object, const FName& FunctionName)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::Unbind));
		MulticastDelegate_.Method("void UnbindObject(UObject Object)", FUNC_TRIVIAL(FAngelscriptMulticastDelegateOperations::UnbindObject));

		auto Delegate_ = Binds.ExistingClassForTarget("_FScriptDelegate");
		Delegate_.Method("void BindUFunction(UObject Object, const FName& FunctionName, UDelegateFunction Signature)", FUNC(FAngelscriptDelegateOperations::BindUFunction_Signature));

		Delegate_.Constructor("void f(UObject Object, const FName& FunctionName, UDelegateFunction Signature)", FUNC(FAngelscriptDelegateOperations::ConstructFromFunction_Signature));
		Delegate_.Method("UObject GetUObject() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetUObject));
		Delegate_.Method("FName GetFunctionName() const", FUNC_TRIVIAL(FAngelscriptDelegateOperations::GetFunctionName));
	});
