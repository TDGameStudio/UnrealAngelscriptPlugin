#include "Engine/NetSerialization.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"

#include "Helper_PropertyBind.h"
#include "Helper_StructType.h"
#include "AngelscriptDocs.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/GarbageCollection.h"
//#include "GarbageCollectionSchema.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_objecttype.h"
#include "source/as_context.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

#include "StaticJIT/AngelscriptStaticJIT.h"
#include "Binds/Bind_Helpers.h"

#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"

static const FName NAME_Struct_Tooltip("ToolTip");
static const FName NAME_Struct_MetaDebuggable("ScriptDebuggable");

struct FUStructType : FAngelscriptType
{
	UScriptStruct* Struct = nullptr;
	asITypeInfo* ScriptTypeInfo = nullptr;
	FString StructName;

	FUStructType(UScriptStruct* InStruct, const FString& InStructName)
		: Struct(InStruct), StructName(InStructName)
	{
	}
	
	virtual bool IsUnrealStruct() const override { return true; }

	bool IsValidType(const FAngelscriptTypeUsage& Usage) const
	{
		return Struct != nullptr || Usage.ScriptClass != nullptr;
	}

	UStruct* GetUnrealStruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return GetStruct(Usage);
	}

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override
	{
		// C++ structs have individual type instances, so we don't need to check this
		if (Struct != nullptr)
			return true;

		// If the scriptclass is identical we don't need to check it
		if (Usage.ScriptClass == Other.ScriptClass)
			return true;

		// Shouldn't happen, safety check
		if (Usage.ScriptClass == nullptr || Other.ScriptClass == nullptr)
			return false;

		// Compare script structs by name, because we are likely comparing for changes during a compile
		if (((asCObjectType*)Usage.ScriptClass)->name == ((asCObjectType*)Other.ScriptClass)->name)
			return true;

		return false;
	}

	FORCEINLINE UScriptStruct* GetStruct(const FAngelscriptTypeUsage& Usage) const
	{
		if (Struct != nullptr)
			return Struct;
		if (Usage.ScriptClass == nullptr)
			return nullptr;
		return (UScriptStruct*)Usage.ScriptClass->GetUserData();
	}

	FORCEINLINE asITypeInfo* GetScriptType(const FAngelscriptTypeUsage& Usage) const
	{
		if (Usage.ScriptClass != nullptr)
			return Usage.ScriptClass;
		return ScriptTypeInfo;
	}

	FORCEINLINE UScriptStruct::ICppStructOps* GetOps(const FAngelscriptTypeUsage& Usage) const
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		if (UsedStruct == nullptr)
			return nullptr;
		return UsedStruct->GetCppStructOps();
	}

	virtual FString GetAngelscriptTypeName() const override
	{
		ensure(Struct != nullptr);
		return StructName;
	}

	FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Struct != nullptr)
			return StructName;
		else if (Usage.ScriptClass != nullptr)
			return ANSI_TO_TCHAR(Usage.ScriptClass->GetName());

		ensure(false);
		return TEXT("");
	}

	virtual void* GetData() const override
	{
		return Struct;
	}

	bool HasReferences(const FAngelscriptTypeUsage& Usage) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);

		// We're a script struct, but we haven't been generated yet.
		//  Let's just assume we have references for now.
		if (UsedStruct == nullptr)
			return true;

		if (UsedStruct->StructFlags & STRUCT_AddStructReferencedObjects)
			return true;

		TArray<const FStructProperty*> EncounteredStructProps;

		FProperty* Property = UsedStruct->PropertyLink;
		while( Property )
		{
			if (Property->ContainsObjectReference(EncounteredStructProps))
				return true;
			Property = Property->PropertyLinkNext;
		}

		return false;
	}

	void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		check(UsedStruct);

		if (!HasReferences(Usage))
			return;

		if (UsedStruct->StructFlags & STRUCT_AddStructReferencedObjects)
		{
			UE::GC::StructAROFn StructARO = UsedStruct->GetCppStructOps()->AddStructReferencedObjects();	
			Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::MemberARO, StructARO) );
		}

		TArray<const FStructProperty*> EncounteredStructProps;
		for (FProperty* Property = UsedStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			Property->EmitReferenceInfo(*Params.Schema, Params.AtOffset, EncounteredStructProps, *Params.DebugPath);
		}
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		return IsValidType(Usage);
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);

		auto* StructProp = new FStructProperty(Params.Outer, Params.PropertyName, RF_Public);
		StructProp->Struct = UsedStruct;

		return StructProp;
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		const FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp == nullptr)
			return false;

		UScriptStruct* UsedStruct = GetStruct(Usage);
		if (UsedStruct == nullptr)
		{
			// Workaround: We don't know our actual type yet, so
			// we compare the script types by name.
			check(Usage.ScriptClass != nullptr);
			FString CheckName = ANSI_TO_TCHAR(Usage.ScriptClass->GetName());
			CheckName.RemoveFromStart(TEXT("F"));

			FString PropClassName = StructProp->Struct->GetName();
			return PropClassName == CheckName;
		}
		else
		{
			if (StructProp->Struct != GetStruct(Usage))
				return false;
			return true;
		}
	}

	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override
	{
		return IsValidType(Usage);
	}

	bool NeedCopy(const FAngelscriptTypeUsage& Usage) const override
	{
		auto* Ops = GetOps(Usage);
		return Ops == nullptr || !Ops->IsPlainOldData();
	}

	bool CanHashValue(const FAngelscriptTypeUsage& Usage) const override
	{
		auto* Ops = GetOps(Usage);
		return Ops != nullptr && Ops->HasGetTypeHash();
	}

	uint32 GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const
	{
		auto* Ops = GetOps(Usage);
		if (Ops == nullptr)
			return 0;
		return Ops->GetStructTypeHash(Address);
	}

	void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		return UsedStruct->CopyScriptStruct(DestinationPtr, SourcePtr, 1);
	}

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return IsValidType(Usage);
	}

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		if (UsedStruct == nullptr)
			return true;

		auto* Ops = GetOps(Usage);
		return Ops == nullptr || !Ops->HasNoopConstructor() || UsedStruct->GetPropertiesSize() > Ops->GetSize();
	}

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		UsedStruct->InitializeStruct(DestinationPtr, 1);
	}

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override
	{
		return IsValidType(Usage);
	}

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		if (UsedStruct == nullptr)
			return true;

		return !(UsedStruct->StructFlags & (STRUCT_IsPlainOldData | STRUCT_NoDestructor));
	}

	void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		UsedStruct->DestroyStruct(DestinationPtr, 1);
	}

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		if (UsedStruct == nullptr)
			return Usage.ScriptClass->GetSize();
		return UsedStruct->GetPropertiesSize();
	}

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		return UsedStruct->CompareScriptStruct(SourcePtr, DestinationPtr, 0);
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		uint8* StructMemory = (uint8*)Data.StackPtr;
		UsedStruct->InitializeStruct(StructMemory, 1);

		if (Usage.bIsReference)
		{
			uint8& RefValue = Stack.StepCompiledInRef<FStructProperty, uint8>(StructMemory);
			Context->SetArgAddress(ArgumentIndex, &RefValue);
		}
		else
		{
			Stack.StepCompiledIn<FStructProperty>(StructMemory);
			Context->SetArgObject(ArgumentIndex, StructMemory);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		if (Usage.bIsReference)
		{
			*(void**)Destination = Context->GetReturnAddress();
		}
		else
		{
			void* ReturnedObject = Context->GetReturnObject();
			if (ReturnedObject == nullptr)
				return;

			UScriptStruct* UsedStruct = GetStruct(Usage);
			UsedStruct->CopyScriptStruct(Destination, ReturnedObject, 1);
		}
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
	{
		UScriptStruct* UsedStruct = GetStruct(Usage);
		if (UsedStruct != nullptr)
			return UsedStruct->GetMinAlignment();
		if (Usage.ScriptClass != nullptr)
			return Usage.ScriptClass->alignment;

		checkf(false, TEXT("Attempted to request alignment from an angelscript struct type without type information."));
		return 8;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		void* NativeValue = (void*)&Usage.ResolvePrimitive<int>(Address);
		auto* UsedStruct = GetStruct(Usage);

		if (Struct != nullptr)
			Value.Type = Struct->GetStructCPPName();
		else if(Usage.ScriptClass != nullptr)
			Value.Type = Usage.ScriptClass->GetName();

		bool bHasToString = false;

		auto* ScriptType = GetScriptType(Usage);
		if (ScriptType != nullptr)
		{
			auto* Func = ScriptType->GetMethodByDecl("FString ToString() const");
			if (Func != nullptr)
			{
				FAngelscriptContext Context(Func->GetEngine());
				if (PrepareAngelscriptContextWithLog(Context, Func, TEXT("FUStructType::GetDebuggerValue")))
				{
					Context->SetObject(NativeValue);
					Context->Execute();

					FString* ReturnString = (FString*)Context->GetReturnObject();
					if (ReturnString != nullptr)
					{
						Value.Value = *ReturnString;
						bHasToString = true;
					}
				}
			}
		}

		if(!bHasToString)
			Value.Value = FString::Printf(TEXT("%s{}"), *Value.Type);

		Value.Usage = Usage;
		Value.Address = Address;
		Value.bHasMembers = true;

		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		void* NativeValue = (void*)&Usage.ResolvePrimitive<int>(Address);

		bool bHasMembers = false;
		auto* UsedStruct = GetStruct(Usage);

		TSet<FString> FoundProperties;

		for (TFieldIterator<FProperty> It(UsedStruct); It; ++It)
		{
			FProperty* Property = *It;
#if WITH_EDITOR
			bool bMetaDebuggable = Property->HasMetaData(NAME_Struct_MetaDebuggable);
#else
			bool bMetaDebuggable = false;
#endif

			if (Struct != nullptr)
			{
				if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && (!Property->HasAnyPropertyFlags(CPF_Edit)
				 || Property->HasAllPropertyFlags(CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate))
					&& !bMetaDebuggable)
				{
					continue;
				}
			}

			// Can't bind static arrays. SAD!
			if (Property->ArrayDim != 1)
				continue;

			FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
			if (!PropUsage.IsValid())
				continue;

			FDebuggerValue DbgValue;
			if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(NativeValue), DbgValue, Property))
			{
				DbgValue.Name = Property->GetName();
				if (bMetaDebuggable)
					DbgValue.Name = TEXT("<") + DbgValue.Name + TEXT(">");
				FoundProperties.Add(DbgValue.Name);
				Scope.Values.Add(MoveTemp(DbgValue));
				bHasMembers = true;
			}
		}

		auto* ScriptType = GetScriptType(Usage);
		if (ScriptType != nullptr)
		{
			int32 PropCount = ScriptType->GetPropertyCount();
			for (int32 i = 0; i < PropCount; ++i)
			{
				const char* PropName;
				int32 Offset;
				ScriptType->GetProperty(i, &PropName, nullptr, nullptr, nullptr, &Offset);

				FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(ScriptType, i);

				FDebuggerValue VarValue;
				if (PropUsage.GetDebuggerValue((void*)((SIZE_T)NativeValue + (SIZE_T)Offset), VarValue))
				{
					VarValue.Name = ANSI_TO_TCHAR(PropName);
					if (!FoundProperties.Contains(VarValue.Name))
					{
						FoundProperties.Add(VarValue.Name);
						Scope.Values.Add(MoveTemp(VarValue));
						bHasMembers = true;
					}
				}
			}
		}

		if (ScriptType != nullptr)
		{
			int32 FuncCount = ScriptType->GetMethodCount();
			for (int32 i = 0; i < FuncCount; ++i)
			{
				asIScriptFunction* ScriptFunction = ScriptType->GetMethodByIndex(i);
				if (!ScriptFunction->IsReadOnly())
					continue;
				if (ScriptFunction->GetParamCount() != 0)
					continue;

				FString FuncName = ANSI_TO_TCHAR(ScriptFunction->GetName());
				if (FuncName.StartsWith(TEXT("Get")))
				{
					FString VarName = FuncName.Mid(3);

					FDebuggerValue VarValue;
					if (!FoundProperties.Contains(VarName))
					{
						if (GetDebuggerValueFromFunction(ScriptFunction, NativeValue, VarValue, GetScriptType(Usage), UsedStruct, VarName))
						{
							VarValue.Name = VarName;
							VarValue.Name += TEXT("$");
							Scope.Values.Add(MoveTemp(VarValue));
							bHasMembers = true;
						}
						FoundProperties.Add(VarName);
					}
				}
			}
		}

		return bHasMembers;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		void* NativeValue = (void*)&Usage.ResolvePrimitive<int>(Address);
		auto* UsedStruct = GetStruct(Usage);

		auto* ScriptType = GetScriptType(Usage);
		if (Member.EndsWith(TEXT("()")) && ScriptType != nullptr)
		{
			FString FunctionName = Member.Mid(0, Member.Len() - 2);
			asIScriptFunction* ScriptFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
			if (ScriptFunction != nullptr)
			{
				if (GetDebuggerValueFromFunction(ScriptFunction, NativeValue, Value, ScriptType, UsedStruct, FunctionName.Mid(3)))
				{
					Value.Name = Member;
					return true;
				}
			}
		}

		if (Member.EndsWith(TEXT("$")) && ScriptType != nullptr)
		{
			FString FunctionName = TEXT("Get") + Member.Mid(0, Member.Len() - 1);
			asIScriptFunction* ScriptFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
			if (ScriptFunction != nullptr)
			{
				if (GetDebuggerValueFromFunction(ScriptFunction, NativeValue, Value, ScriptType, UsedStruct, FunctionName.Mid(3)))
				{
					Value.Name = Member;
					return true;
				}
			}
		}

		for (TFieldIterator<FProperty> It(UsedStruct); It; ++It)
		{
			FProperty* Property = *It;
#if WITH_EDITOR
			bool bMetaDebuggable = Property->HasMetaData(NAME_Struct_MetaDebuggable);
#else
			bool bMetaDebuggable = false;
#endif

			if (Struct != nullptr)
			{
				if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && (!Property->HasAnyPropertyFlags(CPF_Edit)
				 || Property->HasAllPropertyFlags(CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate))
					&& !bMetaDebuggable)
				{
					continue;
				}
			}

			FString PropertyName = Property->GetName();
			if (bMetaDebuggable)
				PropertyName = TEXT("<") + PropertyName + TEXT(">");

			if (PropertyName != Member)
				continue;

			if (Property->ArrayDim != 1)
				continue;

			FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
			if (!PropUsage.IsValid())
				continue;

			if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(NativeValue), Value, Property))
				return true;
		}

		if (ScriptType != nullptr)
		{
			int32 PropCount = ScriptType->GetPropertyCount();
			for (int32 i = 0; i < PropCount; ++i)
			{
				const char* PropName;
				int32 Offset;
				ScriptType->GetProperty(i, &PropName, nullptr, nullptr, nullptr, &Offset);

				FString Name = ANSI_TO_TCHAR(PropName);
				if (Name != Member)
					continue;

				FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(ScriptType, i);
				if (PropUsage.GetDebuggerValue((void*)((SIZE_T)NativeValue + (SIZE_T)Offset), Value))
				{
					Value.Name = Name;
					return true;
				}
			}
		}

		if (ScriptType != nullptr)
		{
			FString FunctionName = TEXT("Get") + Member;
			asIScriptFunction* ScriptFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
			if (ScriptFunction != nullptr && ScriptFunction->IsReadOnly())
			{
				if (GetDebuggerValueFromFunction(ScriptFunction, NativeValue, Value, ScriptType, UsedStruct, FunctionName.Mid(3)))
				{
					Value.Name = Member;
					return true;
				}
			}
		}

		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		if (Struct == nullptr)
		{
			// Script types that are POD can still be represented natively. Helps for template containers
#if AS_CAN_GENERATE_JIT
			asCObjectType* ObjectType = (asCObjectType*)Usage.ScriptClass;
			if (ObjectType != nullptr && (ObjectType->flags & asOBJ_POD) != 0
				&& FAngelscriptEngine::Get().StaticJIT != nullptr
				&& !FAngelscriptEngine::Get().StaticJIT->IsTypePotentiallyDifferent(ObjectType)
				&& ObjectType->GetFirstMethod("opAssign") == nullptr
			)
			{
				int Size = GetValueSize(Usage);
				if (Size == 0)
					OutCppForm.CppType = FString::Printf(TEXT("TScriptPODEmptyStruct<%d>"), GetValueAlignment(Usage));
				else
					OutCppForm.CppType = FString::Printf(TEXT("TScriptPODStruct<%d,%d>"), GetValueSize(Usage), GetValueAlignment(Usage));
				return true;
			}
#endif

			return false;
		}
		else
		{
			FString HeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Struct);
			if (HeaderPath.Len() != 0)
			{
				OutCppForm.CppType = StructName;
				if (!HeaderPath.Contains(TEXT("NoExportTypes.h")))
					OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *HeaderPath);
			}

			return true;
		}
	}
};

static void BindStructBehaviors(FAngelscriptBinds& Binds, const FString& TypeName, UScriptStruct* Struct)
{
	auto* Ops = Struct->GetCppStructOps();

#if AS_CAN_GENERATE_JIT
	const ANSICHAR* AnsiTypeName = FScriptFunctionNativeForm::AllocateAnsiTypeName(TypeName);
#endif

	if (Ops != nullptr)
	{
		// Bind constructor
		if (Ops->HasNoopConstructor())
		{
			// Binding an empty function here is a precaution, in case we are going to be reusing
			// the bytecode between platforms and this function isn't a no-op on other platforms, we still want to generate
			// calls to it even if it does nothing.
			Binds.Constructor("void f()", [](void* Ptr) {});
			SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);
		}
		else if (Ops->HasZeroConstructor())
		{
			Binds.Constructor("void f()", [](void* Ptr)
			{
				UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
				FMemory::Memzero(Ptr, Ops->GetSize());
			}, Ops);
			SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);
		}
		else
		{
			Binds.Constructor("void f()", [](void* Ptr)
			{
				UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
				Ops->Construct(Ptr);
			}, Ops);
			SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);
		}

		// Bind destructor
		if (Ops->HasDestructor())
		{
			Binds.Destructor("void f()", [](void* Ptr)
			{
				UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
				Ops->Destruct(Ptr);
			}, Ops);
			SCRIPT_TRIVIAL_NATIVE_DESTRUCTOR(Binds, AnsiTypeName);
		}
		else
		{
			// Binding an empty function here is a precaution, in case we are going to be reusing
			// the bytecode between platforms and this function isn't a no-op on other platforms, we still want to generate
			// calls to it even if it does nothing.
			Binds.Destructor("void f()", [](void* Ptr) {});
			SCRIPT_TRIVIAL_NATIVE_DESTRUCTOR(Binds, AnsiTypeName);
		}

		// Bind copy operations
		FString CopyConstructDecl = FString::Printf(TEXT("void f(const %s& Other)"), *TypeName);
		FString AssignDecl = FString::Printf(TEXT("%s& opAssign(const %s& Other)"), *TypeName, *TypeName);
		if (Ops->IsPlainOldData())
		{
			Binds.Constructor(CopyConstructDecl, [](void* Destination, void* Source)
			{
				UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
				FMemory::Memcpy(Destination, Source, Ops->GetSize());
			}, Ops);
			SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);

			Binds.Method(AssignDecl, [](void* Destination, void* Source) -> void*
			{
				UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
				FMemory::Memcpy(Destination, Source, Ops->GetSize());
				return Destination;
			}, Ops);
			SCRIPT_TRIVIAL_NATIVE_ASSIGNMENT(Binds, AnsiTypeName);
		}
		else if (Ops->HasCopy())
		{
			if (Ops->HasNoopConstructor())
			{
				Binds.Constructor(CopyConstructDecl, [](void* Destination, void* Source)
				{
					UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
					Ops->Copy(Destination, Source, 1);
				}, Ops);
				SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);
			}
			else if (Ops->HasZeroConstructor())
			{
				Binds.Constructor(CopyConstructDecl, [](void* Destination, void* Source)
				{
					UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
					FMemory::Memzero(Destination, Ops->GetSize());
					Ops->Copy(Destination, Source, 1);
				}, Ops);
				SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);
			}
			else
			{
				Binds.Constructor(CopyConstructDecl, [](void* Destination, void* Source)
				{
					UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
					Ops->Construct(Destination);
					Ops->Copy(Destination, Source, 1);
				}, Ops);
				SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);
			}

			Binds.Method(AssignDecl, [](void* Destination, void* Source) -> void*
			{
				UScriptStruct::ICppStructOps* Ops = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
				Ops->Copy(Destination, Source, 1);
				return Destination;
			}, Ops);
			SCRIPT_TRIVIAL_NATIVE_ASSIGNMENT(Binds, AnsiTypeName);
		}
	}
	else
	{
		// Bind constructor
		Binds.Constructor("void f()", [](void* Ptr)
		{
			UScriptStruct* Struct = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct>();
			Struct->InitializeStruct(Ptr);
		}, Struct);
		SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);

		// Bind destructor
		Binds.Destructor("void f()", [](void* Ptr)
		{
			UScriptStruct* Struct = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct>();
			Struct->DestroyStruct(Ptr);
		}, Struct);
		SCRIPT_TRIVIAL_NATIVE_DESTRUCTOR(Binds, AnsiTypeName);

		// Bind copy operations
		FString CopyConstructDecl = FString::Printf(TEXT("void f(const %s& Other)"), *TypeName);
		Binds.Constructor(CopyConstructDecl, [](void* Destination, void* Source)
		{
			UScriptStruct* Struct = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct>();
			Struct->InitializeStruct(Destination);
			Struct->CopyScriptStruct(Destination, Source);
		}, Struct);
		SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, AnsiTypeName);

		FString AssignDecl = FString::Printf(TEXT("%s& opAssign(const %s& Other)"), *TypeName, *TypeName);
		Binds.Method(AssignDecl, [](void* Destination, void* Source) -> void*
		{
			UScriptStruct* Struct = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct>();
			Struct->CopyScriptStruct(Destination, Source);
			return Destination;
		}, Struct);
		SCRIPT_TRIVIAL_NATIVE_ASSIGNMENT(Binds, AnsiTypeName);
	}
}

static void BindStructType(const FString& TypeName, UScriptStruct* Struct, FBindFlags& BindFlags)
{
	auto Binds = FAngelscriptBinds::ValueClass(TypeName, Struct, BindFlags);

	// Register angelscript type
	auto Type = MakeShared<FUStructType>(Struct, TypeName);
	Type->ScriptTypeInfo = Binds.GetTypeInfo();
	Type->ScriptTypeInfo->SetUserData(Struct);
	FAngelscriptType::Register(Type);
}

static void BindStructTypeLookups()
{
	// Script structs should be generically typed
	FAngelscriptType::SetScriptStruct(MakeShared<FUStructType>(nullptr, TEXT("")));

	// Register a type finder into the type system that
	// can look up a StructProperty's inner angelscript type.
	FAngelscriptType::RegisterTypeFinder([](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (StructProperty == nullptr)
			return false;

		auto Type = FAngelscriptType::GetByData(StructProperty->Struct);
		if (Type.IsValid())
		{
			Usage.Type = Type;
			return true;
		}

		auto* ASStruct = Cast<UASStruct>(StructProperty->Struct);
		if (ASStruct != nullptr && ASStruct->ScriptType != nullptr)
		{
			Usage.Type = FAngelscriptType::GetScriptStruct();
			Usage.ScriptClass = ASStruct->ScriptType;
			return true;
		}

		return false;
	});
}

#if AS_USE_BIND_DB
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_StructDeclarations((int32)FAngelscriptBinds::EOrder::Early + 1, []
{
	for (auto& DBBind : FAngelscriptBindDatabase::Get().Structs)
	{
		UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *DBBind.UnrealPath);
		if (Struct == nullptr)
			continue;

		DBBind.ResolvedStruct = Struct;

		FBindFlags BindFlags;
		if (Struct->StructFlags & STRUCT_IsPlainOldData)
			BindFlags.ExtraFlags |= asOBJ_POD;

		BindStructType(DBBind.TypeName, Struct, BindFlags);
	}

	BindStructTypeLookups();
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_StructDetails(FAngelscriptBinds::EOrder::Late, []
{
	for (auto& DBBind : FAngelscriptBindDatabase::Get().Structs)
	{
		UScriptStruct* Struct = DBBind.ResolvedStruct;
		if (Struct == nullptr)
			continue;

		auto Type = FAngelscriptType::GetByData(Struct);
		if (!Type.IsValid())
			continue;

		auto Binds = FAngelscriptBinds::ExistingClass(DBBind.TypeName);
		BindStructBehaviors(Binds, DBBind.TypeName, Struct);

		for (auto& DBProp : DBBind.Properties)
		{
			FProperty* Property = Struct->FindPropertyByName(*DBProp.UnrealPath);
			if (Property == nullptr)
				continue;

			if (DBProp.Declaration.Len() != 0)
			{
				Binds.Property(DBProp.Declaration, (SIZE_T)Property->GetOffset_ForUFunction());
			}
			else
			{
				FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromProperty(Property);
				if (!Usage.IsValid())
					continue;

				FAngelscriptType::FBindParams Params;
				Params.BindClass = &Binds;
				Params.NameOverride = DBProp.UnrealPath;
				Params.bCanRead = DBProp.bCanRead;
				Params.bCanWrite = DBProp.bCanWrite;
				Usage.Type->BindProperty(Usage, Params, Property);
			}
		}
	}
});
#else // if !AS_USE_BIND_DB

static const FName NAME_Meta_ForceAngelscriptBind("ForceAngelscriptBind");

static const FName NAME_STRUCT_BlueprintType("BlueprintType");
static const FName NAME_STRUCT_NotBlueprintType("NotBlueprintType");
static const FName NAME_STRUCT_NoAutoAngelscriptBind("NoAutoAngelscriptBind");
static const FName NAME_STRUCT_NotInAngelscript("NotInAngelscript");

struct FGetBoxSphereBounds3f
{
	static UScriptStruct* Get();
};

struct FGetBox3f
{
	static UScriptStruct* Get();
};

struct FGetSphere
{
	static UScriptStruct* Get();
};

struct FGetSphere3f
{
	static UScriptStruct* Get();
};

struct FGetIntVector2
{
	static UScriptStruct* Get();
};

bool ShouldBindEngineType(UScriptStruct* Struct)
{
	if (Struct == nullptr)
		return false;

	const FString StructCppName = Struct->GetStructCPPName();
	if (StructCppName == TEXT("FBox") || StructCppName == TEXT("FBoxSphereBounds"))
		return false;

	if (Struct == TBaseStructure<FVector>::Get())
		return false;
	if (Struct == TVariantStructure<FVector3f>::Get())
		return false;
	if (Struct == TBaseStructure<FQuat>::Get())
		return false;
	if (Struct == TVariantStructure<FQuat4f>::Get())
		return false;
	if (Struct == TBaseStructure<FTransform>::Get())
		return false;
	if (Struct == TVariantStructure<FTransform3f>::Get())
		return false;
	if (Struct == TBaseStructure<FRotator>::Get())
		return false;
	if (Struct == TVariantStructure<FRotator3f>::Get())
		return false;
	if (Struct == FGetBox::Get())
		return false;
	if (Struct == FGetBox3f::Get())
		return false;
	if (Struct == TBaseStructure<FLinearColor>::Get())
		return false;
	if (Struct == TBaseStructure<FVector2D>::Get())
		return false;
	if (Struct == TVariantStructure<FVector2f>::Get())
		return false;
	if (Struct == TBaseStructure<FVector4>::Get())
		return false;
	if (Struct == TVariantStructure<FVector4f>::Get())
		return false;
	if (Struct == TBaseStructure<FIntPoint>::Get())
		return false;
	if (Struct == TBaseStructure<FIntVector>::Get())
		return false;
	if (Struct == TBaseStructure<FIntVector4>::Get())
		return false;
	if (Struct == FGetIntVector2::Get())
		return false;
	if (Struct == TBaseStructure<FRandomStream>::Get())
		return false;
	if (Struct == FGetSphere::Get())
		return false;
	if (Struct == FGetSphere3f::Get())
		return false;
	if (Struct == FGetBoxSphereBounds::Get())
		return false;
	if (Struct == FGetBoxSphereBounds3f::Get())
		return false;

	if ((Struct->StructFlags & STRUCT_NoExport))
	{
	}
	else
	{
		// Only bind native structs, not checking for NoExport because those are always from C++ (but might not have the native flag)
		if (!(Struct->StructFlags & STRUCT_Native))
			return false;
	}
	
	// Force binds always gets bound
	if (Struct->HasMetaData(NAME_Meta_ForceAngelscriptBind))
		return true;

	// Allowing opting out of automatic bind
	if (Struct->HasMetaData(NAME_STRUCT_NoAutoAngelscriptBind))
		return false;
	if (Struct->HasMetaData(NAME_STRUCT_NotInAngelscript))
		return false;

	// BlueprintType always gets bound
	if (Struct->GetBoolMetaData(NAME_STRUCT_BlueprintType))
		return true;
	if (Struct->GetBoolMetaData(NAME_STRUCT_NotBlueprintType))
		return false;

	// If the class has any BlueprintVisible properties, also bind it
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
			return true;
		if (Property->HasAnyPropertyFlags(CPF_Edit))
			return true;
		if (Property->HasAnyPropertyFlags(CPF_BlueprintAssignable))
			return true;
	}

	return false;
}

void HardCodeCallingMetaForUnrealStructs();
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_StructDeclarations((int32)FAngelscriptBinds::EOrder::Early+1, []
{
	HardCodeCallingMetaForUnrealStructs();

	for (UScriptStruct* Struct : TObjectRange<UScriptStruct>())
	{
		if (!ShouldBindEngineType(Struct))
			continue;

		FString TypeName = Struct->GetStructCPPName();

		// Bind into angelscript engine
		FBindFlags BindFlags;
		if (Struct->StructFlags & STRUCT_IsPlainOldData)
			BindFlags.ExtraFlags |= asOBJ_POD;
		BindStructType(TypeName, Struct, BindFlags);
	}

	BindStructTypeLookups();
});

static const FName NAME_Property_Struct_ScriptName("ScriptName");
static const FName NAME_Property_Struct_DeprecatedProperty("DeprecatedProperty");
static const FName NAME_Property_Struct_DeprecationMessage("DeprecationMessage");
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_StructDetails((int32)FAngelscriptBinds::EOrder::Late+105, []
{
	for (UScriptStruct* Struct : TObjectRange<UScriptStruct>())
	{
		if (!ShouldBindEngineType(Struct))
			continue;

		auto Type = FAngelscriptType::GetByData(Struct);
		if (!Type.IsValid())
			continue;

		FString TypeName = Type->GetAngelscriptTypeName();
		auto Binds = FAngelscriptBinds::ExistingClass(TypeName);

		BindStructBehaviors(Binds, TypeName, Struct);

		auto* ScriptType = Binds.GetTypeInfo();
		auto ScriptFlags = ScriptType->GetFlags();

#if WITH_EDITOR
		const FString& Tooltip = Struct->GetMetaData(NAME_Struct_Tooltip);
		if (Tooltip.Len() != 0)
			FAngelscriptDocs::AddUnrealDocumentationForType(ScriptType->GetTypeId(), Tooltip);
#endif

		FAngelscriptStructBind DBBind;
		DBBind.TypeName = TypeName;
		DBBind.UnrealPath = Struct->GetPathName();

		// Bind actual properties
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Property = *It;

			FAngelscriptType::FBindParams Params = GetPropertyBindParams(Property);
			Params.BindClass = &Binds;

			if (!Params.bCanRead && !Params.bCanWrite && !Params.bCanEdit)
				continue;

			// Don't bind editor-only stuff in simulate cooked mode
			if (!FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext() && Property->HasAnyPropertyFlags(CPF_EditorOnly))
				continue;

			// Bind using angelscript type system otherwise
			FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromProperty(Property);
			if (!Usage.IsValid())
				continue;

			// Don't bind properties that have a Get or Set accessor bound already
			FString PropertyName = Property->GetName();

#if WITH_EDITOR
			const FString& ScriptName = Property->GetMetaData(NAME_Property_Struct_ScriptName);
			if (ScriptName.Len() != 0)
				PropertyName = ScriptName;
#endif

			if (Usage.Type->BindProperty(Usage, Params, Property))
			{
				// Need to replicate the BindProperty in the database
				FAngelscriptPropertyBind DBProp;
				DBProp.UnrealPath = Property->GetName();
				DBProp.bCanWrite = Params.bCanWrite;
				DBProp.bCanRead = Params.bCanRead;
				DBProp.bCanEdit = Params.bCanEdit;

				if (!Property->HasAnyPropertyFlags(CPF_EditorOnly))
					DBBind.Properties.Add(DBProp);
				continue;
			}

#if WITH_EDITOR
			bool bIsDeprecated = Property->HasMetaData(NAME_Property_Struct_DeprecatedProperty);
			FString DeprecationMessage;
			if (bIsDeprecated)
				DeprecationMessage = Property->GetMetaData(NAME_Property_Struct_DeprecationMessage);

			const FString& PropertyTooltip = Property->GetMetaData(NAME_Struct_Tooltip);
			if (PropertyTooltip.Len() != 0)
				FAngelscriptDocs::AddUnrealDocumentationForProperty(ScriptType->GetTypeId(), Property->GetOffset_ForUFunction(), PropertyTooltip);

			bool bIsEditorOnly = false;
			if (Property->HasAnyPropertyFlags(CPF_EditorOnly))
				bIsEditorOnly = true;
#endif

			FAngelscriptPropertyBind DBProp;
			DBProp.UnrealPath = Property->GetName();

			FString PropertyType = Usage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable);
			FString Declaration = FString::Printf(TEXT("%s %s"), *PropertyType, *PropertyName);
			Binds.Property(Declaration, Property->GetOffset_ForUFunction(), Params);

			// Simple declarations can be stored in the database by declaration
			DBProp.Declaration = Declaration;

			if (!Property->HasAnyPropertyFlags(CPF_EditorOnly))
				DBBind.Properties.Add(DBProp);

#if WITH_EDITOR
			if (bIsDeprecated || bIsEditorOnly)
			{
				auto* ObjectType = (asCObjectType*)Binds.GetTypeInfo();
				if (ObjectType != nullptr)
				{
					asCObjectProperty* ScriptProperty = ObjectType->GetFirstProperty(TCHAR_TO_ANSI(*PropertyName));
					if (ScriptProperty != nullptr)
					{
						if (bIsDeprecated)
						{
							ScriptProperty->isDeprecated = true;
							ScriptProperty->DeprecationMessage = TCHAR_TO_ANSI(*DeprecationMessage);
						}

						if (bIsEditorOnly)
						{
							ScriptProperty->isEditorOnly = true;
						}
					}
					else
					{
						ensure(false);
					}
				}
			}
#endif
		}

		// TODO: We need some way of determining whether this struct
		// even exists in cooked, but I can't come up with one right now,
		// so we'll just rely on ignoring it in cooked.
		FAngelscriptBindDatabase::Get().Structs.Add(DBBind);
	}
});

void HardCodeCallingMetaForUnrealStructs()
{
	auto ForceBind = [](const TCHAR* Path)
	{
		if (auto* Struct = FindObject<UStruct>(nullptr, Path))
			Struct->SetMetaData(NAME_Meta_ForceAngelscriptBind, TEXT(""));
	};

	ForceBind(TEXT("/Script/Engine.OverlapResult"));
}
#endif // AS_USE_BIND_DB
