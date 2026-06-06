#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "ClassGenerator/ASClass.h"
#include "GameFramework/Actor.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

#include "Helper_AngelscriptArguments.h"
#include "Binds/Helper_PODType.h"
#include "Helper_CppType.h"
#include "Helper_ToString.h"
#include "Helper_PropertyBind.h"
#include "Helper_FunctionSignature.h"
#include "Binds/Bind_Helpers.h"
#include "Binds/Bind_TSubclassOf.h"
#include "Binds/BlueprintCallableReflectiveFallback.h"
#include "Binds/Bind_BlueprintTypePrep.h"
//#include "UObject/GarbageCollectionSchema.h"
//#include "GarbageCollectionSchema.h"
#include "UObject/GarbageCollection.h"

#include "AngelscriptDocs.h"
#include "AngelscriptSettings.h"

#include "Testing/AngelscriptEnumTableBaselineProbe.h"
#include "Async/ParallelFor.h"

#if WITH_EDITOR
#include "HAL/FileManager.h"
#include "SourceCodeNavigation.h"
#endif

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

#if !AS_USE_BIND_DB && WITH_EDITOR
// Toggle for the Bind_Defaults Late+100 Phase 2A ParallelFor (Step 3.1).
// Default true: parallel prepare across BindOrder indices.
// Set 0 to fall back to single-threaded prepare for diagnosis or rollback.
// Read once at the entry of Phase 2A to keep behavior stable for the run.
static TAutoConsoleVariable<bool> CVarBindParallelPrepare(
	TEXT("as.Bind.ParallelPrepare"),
	true,
	TEXT("If true (default), Bind_Defaults Late+100 Phase 2A runs the per-BindOrder prepare loop in ParallelFor. ")
	TEXT("Set 0 to fall back to single-threaded prepare. Phase 2B (commit) is always single-threaded on GameThread."),
	ECVF_Default);

// Implemented in Bind_BlueprintCallable.cpp / Bind_BlueprintEvent.cpp.
// Used by the Bind_Defaults Late+100 Phase 2A/2B split (single-threaded prepare in
// Step 2.4; ParallelFor prepare in Step 3.3).
extern void BindBlueprintCallable_Prepare(
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	struct FUFunctionBindPrep& Prep);
extern void BindBlueprintCallable_FromPrep(
	TSharedRef<FAngelscriptType> InType,
	struct FUFunctionBindPrep& Prep,
	FAngelscriptMethodBind& DBBind);
extern void BindBlueprintEvent_Prepare(
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	struct FUFunctionBindPrep& Prep);
extern void BindBlueprintEvent_FromPrep(
	TSharedRef<FAngelscriptType> InType,
	struct FUFunctionBindPrep& Prep,
	FAngelscriptMethodBind& DBBind);
#endif // !AS_USE_BIND_DB && WITH_EDITOR

static const FName NAME_BlueprintType("BlueprintType");
static const FName NAME_NotBlueprintType("NotBlueprintType");
static const FName NAME_NotInAngelscript("NotInAngelscript");
static const FName NAME_ScriptCallable("ScriptCallable");
static const FName NAME_Func_Tooltip("ToolTip");
static const FName NAME_META_DisallowInstantiation("AngelscriptDisallowInstantiation");

static void BindUClassLookup();

/**
 * Type operations for an UObject type.
 */
struct FUObjectType : TAngelscriptPODType<UObject*>
{
	UClass* Class = nullptr;
	FString ClassName;
	asITypeInfo* ClassScriptType = nullptr;

	FUObjectType(UClass* InClass, const FString& InClassName, asITypeInfo* InScriptType = nullptr)
		: Class(InClass), ClassName(InClassName), ClassScriptType(InScriptType)
	{
	}

	FUObjectType() {}

	virtual FString GetAngelscriptTypeName() const override
	{
		ensure(Class != nullptr);
		return ClassName;
	}

	virtual class asITypeInfo* GetAngelscriptTypeInfo(const FAngelscriptTypeUsage& Usage) const override
	{
		return ClassScriptType;
	}

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override
	{
		// C++ classes have individual type instances, so we don't need to check this
		if (Class != nullptr)
			return true;

		// If the scriptclass is identical we don't need to check it
		if (Usage.ScriptClass == Other.ScriptClass)
			return true;

		// Shouldn't happen, safety check
		if (Usage.ScriptClass == nullptr || Other.ScriptClass == nullptr)
			return false;

		// Compare script classes by name, because we are likely comparing for changes during a compile
		if (((asCObjectType*)Usage.ScriptClass)->name == ((asCObjectType*)Other.ScriptClass)->name)
			return true;

		return false;
	}

	virtual FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Class != nullptr)
			return ClassName;
		else if (Usage.ScriptClass != nullptr)
			return ANSI_TO_TCHAR(Usage.ScriptClass->GetName());

		ensure(false);
		return TEXT("");
	}

	virtual UClass* GetClass(const FAngelscriptTypeUsage& Usage) const override
	{
		return Class;
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		return Class != nullptr || Usage.ScriptClass != nullptr;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		// Properties of type UClass* should emit a FClassProperty
		if (Class == UClass::StaticClass())
		{
			auto* Property = new FClassProperty(Params.Outer, Params.PropertyName, RF_Public);
			Property->PropertyClass = Class;
			Property->MetaClass = UObject::StaticClass();
			return Property;
		}

		auto* Property = new FObjectProperty(Params.Outer, Params.PropertyName, RF_Public);
		if (Class != nullptr)
			Property->PropertyClass = Class;
		else
			Property->PropertyClass = (UClass*)Usage.ScriptClass->GetUserData();

		return Property;
	}

	// UObject Types are never directly queried for a property implementation.
	// These are returned by a specialized type finder for performance reasons.
	bool CanQueryPropertyType() const override
	{
		return false;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property);
		if (ObjectProp == nullptr)
			return false;
		if (ObjectProp->HasAnyPropertyFlags(CPF_UObjectWrapper))
			return false;
		if (ObjectProp->HasAnyPropertyFlags(CPF_TObjectPtr))
			return false;

		if (Class != nullptr)
		{
			return ObjectProp->PropertyClass == Class;
		}
		else
		{
			check(Usage.ScriptClass != nullptr);
			UClass* AssociatedClass = (UClass*)Usage.ScriptClass->GetUserData();
			if (AssociatedClass != nullptr)
			{
				return ObjectProp->PropertyClass == AssociatedClass;
			}
			else
			{
				// Workaround: We don't know our actual type yet, so
				// we compare the script types by name.
				FString CheckName = ANSI_TO_TCHAR(Usage.ScriptClass->GetName());
				CheckName.RemoveFromStart(TEXT("U"));
				CheckName.RemoveFromStart(TEXT("A"));

				FString PropClassName = ObjectProp->PropertyClass->GetName();
				return PropClassName == CheckName;
			}
		}
	}

	bool HasReferences(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool IsObjectPointer() const override { return true; }
	void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const override
	{
		Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::Reference));
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		UObject** ArgPtr = (UObject**)Data.StackPtr;
		if (Usage.bIsReference)
		{
			UObject*& ObjRef = Stack.StepCompiledInRef<FObjectPropertyBase, UObject*>(ArgPtr);
			Context->SetArgAddress(ArgumentIndex, &ObjRef);
		}
		else
		{
			Stack.StepCompiledIn<FObjectPropertyBase>(ArgPtr);
			TSetAngelscriptArgument<UObject*>(Context, ArgumentIndex, *ArgPtr);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return !Usage.bIsReference;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		*(UObject**)Destination = TGetAngelscriptReturnValue<UObject*>(Context);
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("null") || InValue == TEXT("nullptr") || InValue == TEXT(""))
		{
			OutValue = TEXT("nullptr");
			return true;
		}
		if (InValue == TEXT("this"))
		{
			OutValue = TEXT("this");
			return true;
		}
		if (InValue == TEXT("__WorldContext") || InValue == TEXT("__WorldContext()"))
		{
			OutValue = TEXT("__WorldContext()");
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("null") || InValue == TEXT("nullptr"))
		{
			OutValue = TEXT("");
			return true;
		}
		return false;
	}

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override { return true; }

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* Address) const override
	{
		*(UObject**)Address = nullptr;
	}

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const
	{
		return alignof(UObject*);
	}

	bool CanHashValue(const FAngelscriptTypeUsage& Usage) const
	{
		return true;
	}

	uint32 GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const
	{
		return GetTypeHash(*(UObject**)Address);
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		UObject*& Object = Usage.ResolvePrimitive<UObject*>(Address);

		if (Class != nullptr)
		{
			UASClass* asClass = Cast<UASClass>(Class);
			//const TCHAR* Prefix = (Class->HasAnyClassFlags(CLASS_Native) || Class->bIsScriptClass) ? Class->GetPrefixCPP() : TEXT("");
			//Value.Type = Prefix + Class->GetName();
			if (asClass != nullptr)
			{
				const TCHAR* Prefix = (asClass->HasAnyClassFlags(CLASS_Native) || asClass->bIsScriptClass) ? asClass->GetPrefixCPP() : TEXT("");
				Value.Type = Prefix + asClass->GetName();
			}			
		}
		else if (Usage.ScriptClass != nullptr)
		{
			Value.Type = Usage.ScriptClass->GetName();
		}

		Value.Usage = Usage;
		Value.Address = Address;

		FillObjectDebuggerValue(Object, Value);
		return true;
	}

	static void FillObjectDebuggerValue(UObject* Object, FDebuggerValue& Value)
	{
		if (Object == nullptr || Object->GetClass() == nullptr)
		{
			Value.Value = TEXT("nullptr");
			Value.bHasMembers = false;
		}
		else
		{
			FString Suffix;
			auto& Delegate = FAngelscriptEngine::Get().GetDebugObjectSuffix();
			if (Delegate.IsBound())
			{
				Delegate.Execute(Object, Suffix);
			}

			UClass* ObjClass = Object->GetClass();
			UASClass* asClass = UASClass::GetFirstASClass(ObjClass);

			if (asClass == nullptr)
				return;

#if WITH_EDITOR
			if (AActor* Actor = Cast<AActor>(Object))
			{
				Value.Value = FString::Printf(TEXT("{ %s %s(%s%s) (ID: %s) }"),
					*Actor->GetActorLabel(),
					*Suffix,
					//(ObjClass->HasAnyClassFlags(CLASS_Native) || ObjClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					(ObjClass->HasAnyClassFlags(CLASS_Native) || asClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					*ObjClass->GetName(),
					*Object->GetName());
			}
			else
#endif
			{

				Value.Value = FString::Printf(TEXT("{ %s %s(%s%s) }"),
					*Object->GetName(),
					*Suffix,
					//(ObjClass->HasAnyClassFlags(CLASS_Native) || ObjClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					(ObjClass->HasAnyClassFlags(CLASS_Native) || asClass->bIsScriptClass) ? ObjClass->GetPrefixCPP() : TEXT(""),
					*ObjClass->GetName());
			}


			//auto* ScriptType = (asITypeInfo*)ObjClass->ScriptTypePtr;
			auto* ScriptType = (asITypeInfo*)asClass->ScriptTypePtr;
			Value.bHasMembers = ObjClass->PropertyLink != nullptr || (ScriptType != nullptr && ScriptType->GetPropertyCount() != 0);
		}
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		UObject*& Object = Usage.ResolvePrimitive<UObject*>(Address);

		if (Object == nullptr)
		{
			return false;
		}

		// Unit tests may force garbage collection in the middle of the tick so check if the object has been destroyed
		// before accessing it.
		if (Object->HasAnyFlags(RF_FinishDestroyed))
		{
			return false;
		}

		FillObjectDebuggerScope(Object, Scope);
		return true;
	}

	static void FillObjectDebuggerScope(UObject* Object, FDebuggerScope& Scope)
	{
		FDebuggerValue NameValue;
		NameValue.Name = TEXT("Name");
		NameValue.Type = TEXT("FName");
		NameValue.Value = TEXT("n\"") + Object->GetName() + TEXT("\"");
		Scope.Values.Add(MoveTemp(NameValue));

		auto* ObjClass = Object->GetClass();
		UASClass* asClass = UASClass::GetFirstASClass(ObjClass);
		if (asClass == nullptr) return;
		//auto* ObjScriptType = (asITypeInfo*)ObjClass->ScriptTypePtr;
		auto* ObjScriptType = (asITypeInfo*)asClass->ScriptTypePtr;
		if (ObjScriptType == nullptr)
		{
			auto ASType = FAngelscriptType::GetByClass(ObjClass);
			if (ASType.IsValid())
				ObjScriptType = ASType->GetAngelscriptTypeInfo(FAngelscriptTypeUsage::DefaultUsage);
		}
		TSet<FString> FoundProperties;

		auto* ScriptType = ObjScriptType;
		while (ScriptType != nullptr)
		{
			int32 PropCount = ScriptType->GetPropertyCount();
			for (int32 i = 0; i < PropCount; ++i)
			{
				const char* PropName;
				int32 Offset;
				ScriptType->GetProperty(i, &PropName, nullptr, nullptr, nullptr, &Offset);

				if (ScriptType->IsPropertyInherited(i))
					continue;

				FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(ScriptType, i);

				FDebuggerValue VarValue;
				if (PropUsage.GetDebuggerValue((void*)((SIZE_T)Object + (SIZE_T)Offset), VarValue))
				{
					VarValue.Name = ANSI_TO_TCHAR(PropName);
					if (!FoundProperties.Contains(VarValue.Name))
					{
						FoundProperties.Add(VarValue.Name);
						Scope.Values.Add(MoveTemp(VarValue));
					}
				}
			}

			int32 FuncCount = ScriptType->GetMethodCount();
			for (int32 i = 0; i < FuncCount; ++i)
			{
				asIScriptFunction* ScriptFunction = ScriptType->GetMethodByIndex(i);
				if (!ScriptFunction->IsReadOnly())
					continue;
				if (ScriptFunction->GetParamCount() != 0)
					continue;
				if (ScriptFunction->GetObjectType()->GetUserData() == UObject::StaticClass())
					continue;
				if (ScriptFunction->GetObjectType() != ScriptType)
					continue;

				FString FuncName = ANSI_TO_TCHAR(ScriptFunction->GetName());
				if (FuncName.StartsWith(TEXT("Get")))
				{
					FString VarName = FuncName.Mid(3);

					FDebuggerValue VarValue;
					if (!FoundProperties.Contains(VarName))
					{
						if (GetDebuggerValueFromFunction(ScriptFunction, Object, VarValue, ObjScriptType, ObjClass, VarName))
						{
							VarValue.Name = VarName;
							VarValue.Name += TEXT("$");
							Scope.Values.Add(MoveTemp(VarValue));
						}
						FoundProperties.Add(VarName);
					}
				}
			}

			if (((asCObjectType*)ScriptType)->derivedFrom != nullptr)
				ScriptType = ((asCObjectType*)ScriptType)->derivedFrom;
			else if (((asCObjectType*)ScriptType)->shadowType != nullptr)
				ScriptType = ((asCObjectType*)ScriptType)->shadowType;
			else
				break;
		}

		for (TFieldIterator<FProperty> It(ObjClass); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && (!Property->HasAnyPropertyFlags(CPF_Edit)
			 || Property->HasAllPropertyFlags(CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate)))
			{
				continue;
			}
			if (FoundProperties.Contains(Property->GetName()))
				continue;

			// Can't bind static arrays. SAD!
			if (Property->ArrayDim != 1)
				continue;

			FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
			if (!PropUsage.IsValid())
				continue;

			FDebuggerValue DbgValue;
			if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(Object), DbgValue, Property))
			{
				DbgValue.Name = Property->GetName();
				FoundProperties.Add(DbgValue.Name);
				Scope.Values.Add(MoveTemp(DbgValue));
			}
		}
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		UObject*& Object = Usage.ResolvePrimitive<UObject*>(Address);

		if (Object == nullptr)
			return false;

		return FillObjectDebuggerMember(Object, Member, Value);
	}

	static bool FillObjectDebuggerMember(UObject* Object, const FString& Member, FDebuggerValue& Value)
	{
		if (Member == TEXT("Name"))
		{
			Value.Name = TEXT("Name");
			Value.Type = TEXT("FName");
			Value.Value = TEXT("n\"") + Object->GetName() + TEXT("\"");
			return true;
		}

		auto* ObjClass = Object->GetClass();
		UASClass* asClass = UASClass::GetFirstASClass(ObjClass);
		if (asClass == nullptr) return false;
		
		//auto* ObjScriptType = (asITypeInfo*)ObjClass->ScriptTypePtr;
		auto* ObjScriptType = (asITypeInfo*)asClass->ScriptTypePtr;
		if (ObjScriptType == nullptr)
		{
			auto ASType = FAngelscriptType::GetByClass(ObjClass);
			if (ASType.IsValid())
				ObjScriptType = ASType->GetAngelscriptTypeInfo(FAngelscriptTypeUsage::DefaultUsage);
		}

		if (Member.EndsWith(TEXT("()")) && ObjScriptType != nullptr)
		{
			FString FunctionName = Member.Mid(0, Member.Len() - 2);
			asIScriptFunction* ScriptFunction = ObjScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
			if (ScriptFunction != nullptr)
			{
				if (GetDebuggerValueFromFunction(ScriptFunction, Object, Value, ObjScriptType, ObjClass, FunctionName.Mid(3)))
				{
					Value.Name = Member;
					return true;
				}
			}
		}

		if (Member.EndsWith(TEXT("$")) && ObjScriptType != nullptr)
		{
			FString FunctionName = TEXT("Get") + Member.Mid(0, Member.Len() - 1);
			asIScriptFunction* ScriptFunction = ObjScriptType->GetMethodByName(TCHAR_TO_ANSI(*FunctionName));
			if (ScriptFunction != nullptr)
			{
				if (GetDebuggerValueFromFunction(ScriptFunction, Object, Value, ObjScriptType, ObjClass, FunctionName.Mid(3)))
				{
					Value.Name = Member;
					return true;
				}
			}
		}

		FString GetFunctionName = TEXT("Get") + Member;

		auto* ScriptType = ObjScriptType;
		while (ScriptType != nullptr)
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
				if (PropUsage.GetDebuggerValue((void*)((SIZE_T)Object + (SIZE_T)Offset), Value))
				{
					Value.Name = Name;
					return true;
				}
			}

			asIScriptFunction* ScriptFunction = ScriptType->GetMethodByName(TCHAR_TO_ANSI(*GetFunctionName));
			if (ScriptFunction != nullptr && ScriptFunction->IsReadOnly())
			{
				if (GetDebuggerValueFromFunction(ScriptFunction, Object, Value, ObjScriptType, ObjClass, Member))
				{
					Value.Name = Member;
					return true;
				}
			}

			if (((asCObjectType*)ScriptType)->derivedFrom != nullptr)
				ScriptType = ((asCObjectType*)ScriptType)->derivedFrom;
			else if (((asCObjectType*)ScriptType)->shadowType != nullptr)
				ScriptType = ((asCObjectType*)ScriptType)->shadowType;
			else
				break;
		}

		for (TFieldIterator<FProperty> It(ObjClass); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit))
				continue;

			if (Property->GetName() != Member)
				continue;

			// Can't bind static arrays. SAD!
			if (Property->ArrayDim != 1)
				continue;

			FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
			if (!PropUsage.IsValid())
				continue;

			if (PropUsage.GetDebuggerValue(Property->ContainerPtrToValuePtr<void>(Object), Value, Property))
			{
				Value.Name = Property->GetName();
				return true;
			}
		}

		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		if (Class == UObject::StaticClass())
		{
			OutCppForm.CppType = TEXT("UObject*");
		}
		else if (Class == UClass::StaticClass())
		{
			OutCppForm.CppType = TEXT("UClass*");
		}
		else if (Class == UDelegateFunction::StaticClass())
		{
			OutCppForm.CppType = TEXT("UDelegateFunction*");
		}
		else if (Class == UFunction::StaticClass())
		{
			OutCppForm.CppType = TEXT("UFunction*");
		}
		else if (Class != nullptr)
		{
			FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(Class);
			if (ClassHeaderPath.Len() != 0)
			{
				OutCppForm.CppType = ClassName + TEXT("*");
				if (!ClassHeaderPath.Contains(TEXT("NoExportTypes.h")))
					OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *ClassHeaderPath);
			}
		}

		OutCppForm.CppGenericType = TEXT("UObject*");
		return true;
	}
};

#if WITH_EDITOR
ANGELSCRIPTRUNTIME_API bool IsEditorOnlyClass(UClass* Class);
ANGELSCRIPTRUNTIME_API bool ShouldDisallowInstantiation(UClass* Class);
#endif

static void BindUClass(UClass* Class, const FString& TypeName)
{	
	// Bind into angelscript engine
	auto Class_ = FAngelscriptBinds::ReferenceClass(TypeName, Class);
	// Register angelscript type
	auto Type = MakeShared<FUObjectType>(Class, TypeName, Class_.GetTypeInfo());
	FAngelscriptType::Register(Type);

	// Tell the angelscript type what UClass is associated with it
	auto* TypeInfo = (asCTypeInfo*)Class_.GetTypeInfo();
	if (TypeInfo != nullptr)
	{
		TypeInfo->plainUserData = (SIZE_T)Class;

#if WITH_EDITOR
		const FString& Tooltip = Class->GetMetaData(NAME_Func_Tooltip);
		if (Tooltip.Len() != 0)
			FAngelscriptDocs::AddUnrealDocumentationForType(TypeInfo->GetTypeId(), Tooltip);

		if (IsEditorOnlyClass(Class))
			TypeInfo->flags |= asOBJ_EDITOR_ONLY;

		if (ShouldDisallowInstantiation(Class))
			TypeInfo->flags |= asOBJ_DISALLOW_INSTANTIATION;
#endif
	}
}

static void BindStaticClass(FAngelscriptBinds& Binds, const FString& TypeName, UClass* Class)
{
	// Bind the StaticClass() function.
	if (FAngelscriptEngine::Get().ConfigSettings->StaticClassDeprecation != EAngelscriptStaticClassMode::Disallowed)
	{
		FAngelscriptBinds::FNamespace ns(TypeName);
		FAngelscriptBinds::BindGlobalFunction("UClass StaticClass()", FUNC_TRIVIAL(FAngelscriptBindHelpers::GetStaticClassFromClass), Class);
		FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

		if (FAngelscriptEngine::Get().ConfigSettings->StaticClassDeprecation == EAngelscriptStaticClassMode::Deprecated)
			FAngelscriptBinds::DeprecatePreviousBind("Types can now be used as values directly");
	}

	// Bind the static class global variable used for direct type access
	{
		FString Decl = FString::Printf(TEXT("const TSubclassOf<UObject> __StaticType_%s"), *TypeName);
		TSubclassOf<UObject>* ClassValue = new TSubclassOf<UObject>(Class);
		FAngelscriptBinds::BindGlobalVariable(Decl, ClassValue);
	}
}

#if AS_USE_BIND_DB
// From Bind_BlueprintEvent.cpp
extern void BindBlueprintEvent(TSharedRef<FAngelscriptType> InType, UFunction* Function, FAngelscriptMethodBind& DBMethod);

// From Bind_BlueprintCallable.cpp
extern void BindBlueprintCallable(TSharedRef<FAngelscriptType> InType, UFunction* Function, FAngelscriptMethodBind& DBMethod);

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_BlueprintType_Declarations(FAngelscriptBinds::EOrder::Early, []
{
	for (auto& DBBind : FAngelscriptBindDatabase::Get().Classes)
	{
		UClass* Class = FindObject<UClass>(nullptr, *DBBind.UnrealPath);
		if (Class == nullptr)
			continue;

		DBBind.ResolvedClass = Class;
		BindUClass(Class, DBBind.TypeName);
	}

	BindUClassLookup();
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Defaults((int32)FAngelscriptBinds::EOrder::Late + 100, []
{
	FAngelscriptScopeTimer Timer(TEXT("blueprinttype bindings"));
	auto* ScriptEngine = FAngelscriptEngine::Get().Engine;

	for (auto& DBBind : FAngelscriptBindDatabase::Get().Classes)
	{
		UClass* Class = DBBind.ResolvedClass;
		if (Class == nullptr)
			continue;

		auto ClassType = FAngelscriptType::GetByClass(Class);
		auto* SuperClass = Class->GetSuperClass();

		for (auto& DBFunc : DBBind.Methods)
		{
			UFunction* Function = Class->FindFunctionByName(*DBFunc.UnrealPath);
			if (Function == nullptr)
				continue;

			if (Function->HasAnyFunctionFlags(FUNC_BlueprintEvent | FUNC_NetFuncFlags))
				BindBlueprintEvent(ClassType.ToSharedRef(), Function, DBFunc);
#if WITH_ANGELSCRIPT_HAZE
			else if (Function->HasAnyFunctionFlags(FUNC_NetFunction))
				BindBlueprintEvent(ClassType.ToSharedRef(), Function, DBFunc);
#endif
			else
				BindBlueprintCallable(ClassType.ToSharedRef(), Function, DBFunc);
		}
	}

	for (auto& DBBind : FAngelscriptBindDatabase::Get().Classes)
	{
		UClass* Class = DBBind.ResolvedClass;
		if (Class == nullptr)
			continue;
	
		FAngelscriptBinds Binds = FAngelscriptBinds::ExistingClass(DBBind.TypeName);
		auto* ScriptType = Binds.GetTypeInfo();

		// Inherit properties an functions from the parent class
		if (Class->GetSuperClass() != nullptr)
		{
			auto InheritType = FAngelscriptType::GetByClass(Class->GetSuperClass());
			// Check if superclass is bound in angelscript before performing lookup
			if (InheritType != nullptr)
			{
				auto* InheritScriptType = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*InheritType->GetAngelscriptTypeName()));

				if (InheritScriptType != nullptr)
				{
					ScriptType->CopySystemType(InheritScriptType);
				}
			}
			else
			{
				UE_LOG(Angelscript, Warning, TEXT("Cant find angelscript binding for SuperClass: %s of Class: %s"), *Class->GetSuperClass()->GetName(), *Class->GetName());
			}
		}

		// Bind Properties from database
		for (auto& DBProp : DBBind.Properties)
		{
			FProperty* Property = Class->FindPropertyByName(*DBProp.UnrealPath);
			if (Property == nullptr)
				continue;

			if (DBProp.Declaration.Len() != 0)
			{
				Binds.Property(DBProp.Declaration, Property->GetOffset_ForUFunction());
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

		BindStaticClass(Binds, DBBind.TypeName, Class);
	}
});

#elif !AS_USE_BIND_DB

// From Bind_BlueprintEvent.cpp
extern void BindBlueprintEvent(TSharedRef<FAngelscriptType> InType, UFunction* Function, FAngelscriptMethodBind& DBMethod, const TCHAR* OverrideName = nullptr);

// From Bind_BlueprintCallable.cpp
extern void BindBlueprintCallable(TSharedRef<FAngelscriptType> InType, UFunction* Function, FAngelscriptMethodBind& DBMethod, const TCHAR* OverrideName = nullptr);

/**
 * Binds declarations of all BlueprintType UObjects at the earliest possible
 * moment in bind order.
 */

#if WITH_EDITOR
static TMap<UClass*, bool> GCachedEditorClasses;

void ResetCachedEditorClasses()
{
	GCachedEditorClasses.Empty();
}

bool IsEditorOnlyClass(UClass* Class)
{
	TMap<UClass*, bool>& CachedEditorClasses = GCachedEditorClasses;
	bool* CachedValue = CachedEditorClasses.Find(Class);
	if (CachedValue != nullptr)
		return *CachedValue;

	bool bIsEditor = false;

	// Check if the class lives in an editor-only module package
	if (Class->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_UncookedOnly))
	{
		bIsEditor = true;
	}

	// Check if the class is in a package that's specifically marked editor only by the game
	if (!bIsEditor && FAngelscriptEngine::Get().ConfigSettings->AdditionalEditorOnlyScriptPackageNames.Contains(Class->GetOutermost()->GetFName()))
	{
		bIsEditor = true;
	}

	// See if we can find the module that this class is in
	FString ClassHeaderPath;
	if (!bIsEditor && FSourceCodeNavigation::FindClassHeaderPath(Class, ClassHeaderPath))
	{
		if (ClassHeaderPath.Contains(TEXT("/Source/Editor/"))
			|| ClassHeaderPath.Contains(TEXT("\\Source\\Editor\\"))
			|| ClassHeaderPath.Contains(TEXT("/Plugins/Editor/"))
			|| ClassHeaderPath.Contains(TEXT("\\Plugins\\Editor\\"))
			|| ClassHeaderPath.Contains(TEXT("\\Source\\AngelscriptEditor\\"))
			|| ClassHeaderPath.Contains(TEXT("/Source/AngelscriptEditor/"))
			)
		{
			bIsEditor = true;
		}
	}

	if (!bIsEditor && Class->GetSuperClass() != nullptr)
		bIsEditor = IsEditorOnlyClass(Class->GetSuperClass());

	CachedEditorClasses.Add(Class, bIsEditor);
	return bIsEditor;
}

bool ShouldDisallowInstantiation(UClass* Class)
{
	if (!FAngelscriptEngine::Get().ConfigSettings->bAllowRawConstructorsForComponentsAndActors)
	{
		if (Class->IsChildOf(AActor::StaticClass()))
			return true;
		if (Class->IsChildOf(UActorComponent::StaticClass()))
			return true;
	}

	if (Class->HasMetaData(NAME_META_DisallowInstantiation))
		return true;

	return false;
}
#endif

bool ShouldBindEngineType(UClass* Class)
{
	if (Class == nullptr)
		return false;

	// UObject always gets bound
	if (Class == UObject::StaticClass())
		return true;

	// Only bind native classes
	if (!Class->HasAnyClassFlags(CLASS_Native))
		return false;

#if WITH_EDITOR
	// Don't bind classes in editor modules in simulate-cooked mode
	if (!FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext())
	{
		if (IsEditorOnlyClass(Class))
			return false;
	}
#endif

	// Ignore runtime generated types (impossible?)
	//WILL-EDIT
	UASClass* asClass = Cast<UASClass>(Class);
	if (asClass != nullptr && asClass->bIsScriptClass)
		return false;	

	// BlueprintType always gets bound
	if (Class->HasMetaData(NAME_NotInAngelscript))
		return false;
	if (Class->GetBoolMetaData(NAME_BlueprintType))
		return true;

	// Native interface classes with BlueprintCallable methods should be bound
	// even if they don't have BlueprintType metadata, so that scripts can
	// Cast<> to them and call their methods through interface references.
	if (Class->HasAnyClassFlags(CLASS_Interface) && Class != UInterface::StaticClass())
		return true;

	if (Class->HasMetaData(NAME_NotBlueprintType))
		return false;

	// If the class has any BlueprintCallable functions, also bind it
	UClass* CheckClass = Class;
	bool bHasBlueprintCallable = false;

	//WILL-EDIT
	TArray<FName> NameArray;

	while (CheckClass != nullptr && !bHasBlueprintCallable)
	{
		//WILL-EDIT		
		CheckClass->GenerateFunctionList(NameArray);
		
		for (auto& Elem : NameArray)
		{
			//WILL-EDIT
			UFunction* Function = CheckClass->FindFunctionByName(Elem);			
			if (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintEvent))
			{
				bHasBlueprintCallable = true;
				break;
			}
		}
		CheckClass = CheckClass->GetSuperClass();
	}

	if (bHasBlueprintCallable)
		return true;

	return false;
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_BlueprintType_Declarations(FAngelscriptBinds::EOrder::Early, []
{
	// Bind any clasess that are BlueprintType	

	//WILL-EDIT?
	for (UClass* Class : TObjectRange<UClass>()) //Could this be parallel for? Have to create all maps first
	{					
		if (!ShouldBindEngineType(Class))
		{			
			continue;
		}

		// Bind into angelscript engine
		FString ClassName = FAngelscriptType::GetBoundClassName(Class);
		BindUClass(Class, ClassName);
		
	}

	BindUClassLookup();
});

/**
 * Binds everything that was declared as a blueprint accessible UPROPERTY()
 */
static const FName NAME_Property_ScriptName("ScriptName");
static const FName NAME_Property_DeprecatedProperty("DeprecatedProperty");
static const FName NAME_Property_DeprecationMessage("DeprecationMessage");

static FString GetBlueprintAccessorPropertyName(FProperty* Property)
{
	return Property->GetName();
}


void BindProperties(FAngelscriptBinds Binds, TSharedRef<FAngelscriptType> Type, TArray<FAngelscriptPropertyBind>& DBProperties)
{
	UClass* Class = Type->GetClass(FAngelscriptTypeUsage::DefaultUsage);

	for (TFieldIterator<FProperty> It(Class, EFieldIterationFlags::IncludeDeprecated); It; ++It)
	{
		FProperty* Property = *It;

		// Don't bind editor-only stuff in simulate cooked mode
		if (!FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext() && Property->HasAnyPropertyFlags(CPF_EditorOnly))
			continue;

		FAngelscriptType::FBindParams Params = GetPropertyBindParams(Property);
		Params.BindClass = &Binds;

		if(!Params.bCanRead && !Params.bCanWrite && !Params.bCanEdit)
			continue;

		// Bind using angelscript type system otherwise
		FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromProperty(Property);
		if (!Usage.IsValid())
			continue;

		// Don't bind properties that have a Get or Set accessor bound already
		FString PropertyName = GetBlueprintAccessorPropertyName(Property);

#if WITH_EDITOR
		const FString& ScriptName = Property->GetMetaData(NAME_Property_ScriptName);
		if (ScriptName.Len() != 0)
			PropertyName = FAngelscriptFunctionSignature::GetPrimaryScriptName(ScriptName);

		const FString& Tooltip = Property->GetMetaData(NAME_Func_Tooltip);
		if (Tooltip.Len() != 0)
			FAngelscriptDocs::AddUnrealDocumentationForProperty(Binds.GetTypeInfo()->GetTypeId(), Property->GetOffset_ForUFunction(), Tooltip);
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
				DBProperties.Add(DBProp);
			continue;
		}

		FAngelscriptPropertyBind DBProp;
		DBProp.UnrealPath = Property->GetName();

#if WITH_EDITOR
		bool bIsDeprecated = Property->HasMetaData(NAME_Property_DeprecatedProperty);
		FString DeprecationMessage;
		if (bIsDeprecated)
			DeprecationMessage = Property->GetMetaData(NAME_Property_DeprecationMessage);

		bool bIsEditorOnly = false;
		if (Property->HasAnyPropertyFlags(CPF_EditorOnly))
			bIsEditorOnly = true;
#endif

		FString PropertyType = Usage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable);
		FString Declaration = FString::Printf(TEXT("%s %s"), *PropertyType, *PropertyName);
		Binds.Property(Declaration, Property->GetOffset_ForUFunction(), Params);

		// Simple declarations can be stored in the database by declaration
		DBProp.Declaration = Declaration;

		if (!Property->HasAnyPropertyFlags(CPF_EditorOnly))
			DBProperties.Add(DBProp);

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
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Defaults((int32)FAngelscriptBinds::EOrder::Late+100, []
{
	FAngelscriptScopeTimer Timer(TEXT("blueprinttype bindings"));
	auto* ScriptEngine = FAngelscriptEngine::Get().Engine;
	const double T0 = FPlatformTime::Seconds();

	struct FBindOrder
	{
		UClass* Class = nullptr;
		TSharedPtr<FAngelscriptType> Type;
		asITypeInfo* ScriptType = nullptr;
		TSharedPtr<FAngelscriptType> InheritType;
		asITypeInfo* InheritScriptType = nullptr;
		FAngelscriptClassBind DBBind;

		// Phase 2A (prepare) populates this; Phase 2B (commit) consumes it.
		// Empty until Phase 2A runs; cleared after Phase 2B finishes.
		TArray<FUFunctionBindPrep> FunctionPreps;
	};

	TArray<FBindOrder> ClassesToBind;
	TSet<UClass*> VisitedClasses;

	struct FClassVisiter
	{
		static void Visit(asIScriptEngine* ScriptEngine, UClass* Class, TArray<FBindOrder>& ClassesToBind, TSet<UClass*>& VisitedClasses)
		{
			bool bAlreadyVisited = false;
			VisitedClasses.Add(Class, &bAlreadyVisited);

			if (bAlreadyVisited)
				return;

			FBindOrder BindOrder;
			BindOrder.Class = Class;
			BindOrder.Type = FAngelscriptType::GetByClass(Class);
			if (!BindOrder.Type.IsValid())
				return;

			BindOrder.ScriptType = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*BindOrder.Type->GetAngelscriptTypeName()));

			if (auto* SuperClass = Class->GetSuperClass())
			{
				Visit(ScriptEngine, Class->GetSuperClass(), ClassesToBind, VisitedClasses);

				BindOrder.InheritType = FAngelscriptType::GetByClass(Class->GetSuperClass());
				if (BindOrder.InheritType.IsValid())
					BindOrder.InheritScriptType = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*BindOrder.InheritType->GetAngelscriptTypeName()));
			}

			ClassesToBind.Add(BindOrder);
		};
	};

	for (UClass* Class : TObjectRange<UClass>())
		FClassVisiter::Visit(ScriptEngine, Class, ClassesToBind, VisitedClasses);

	const double TCollect = FPlatformTime::Seconds();

	// ---- Phase 2: Function enumeration + Callable/Event binding ----
	int32 TotalFuncsBound = 0;

	// Opt 6: cache the editor-context flag once (stable for the duration of Phase 2).
	const bool bUseEditorScripts = FAngelscriptEngine::ShouldUseEditorScriptsForCurrentContext();

	// ---- Phase 1.5: Prewarm UClass NameArray on the GameThread (Step 3.2) ----
	// UClass::FindFunctionByName triggers a lazy GenerateFunctionList walk that mutates
	// UClass::FuncMap. Phase 2A must be safe to run on worker threads, so we materialize
	// every UClass + SuperClass function list serially here. Cost is small (~5 ms in
	// editor) and only runs once per Late+100 invocation.
	{
		AS_BIND_PHASE_SCOPE(EBindLatePhase::PrewarmNameArray);
		TArray<FName> Tmp;
		for (auto& Order : ClassesToBind)
		{
			if (Order.Class != nullptr)
			{
				Order.Class->GenerateFunctionList(Tmp);
				if (UClass* Super = Order.Class->GetSuperClass())
				{
					Super->GenerateFunctionList(Tmp);
				}
			}
		}
	}

	// ---- Phase 2A: Prepare (read-only AS Engine, parallelizable) ----
	// Walks every UClass + UFunction once, runs all eligibility checks, and builds
	// FAngelscriptFunctionSignature + cached FFuncEntry pointer into BindOrder.FunctionPreps.
	// No AS Engine writes happen here. Each ClassIdx writes only to its own
	// ClassesToBind[ClassIdx].FunctionPreps, so the loop is safely parallelizable.
	// The NameArray prewarm in Phase 1.5 guards against UClass::FindFunctionByName
	// triggering lazy mutation from worker threads.
	const bool bRunParallelPrepare = CVarBindParallelPrepare.GetValueOnGameThread();
	{
		AS_BIND_PHASE_SCOPE(EBindLatePhase::Phase2_Prepare);

		auto PreparePerClass = [&ClassesToBind, bUseEditorScripts](int32 ClassIdx)
		{
#if WITH_DEV_AUTOMATION_TESTS
			const double WorkerStartSeconds = FPlatformTime::Seconds();
#endif

			FBindOrder& BindOrder = ClassesToBind[ClassIdx];
			auto ClassType = BindOrder.Type;
			UClass* SuperClass = BindOrder.Class != nullptr ? BindOrder.Class->GetSuperClass() : nullptr;

			// Opt 4: single-pass TFieldIterator<UFunction>(ExcludeSuper) replaces the
			//        GenerateFunctionList + FindFunctionByName double walk.
			for (TFieldIterator<UFunction> FuncIt(BindOrder.Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
			{
				UFunction* Function = *FuncIt;

				if (Function->GetSuperFunction() != nullptr)
					continue;

				// Opt 5 (phase 1): keep the SuperClass->FindFunctionByName audit to detect
				// rare shadow-UFUNCTION patterns. Phase 1.5 prewarmed Super NameArray so
				// FindFunctionByName is safe to invoke from worker threads here.
				if (SuperClass != nullptr && SuperClass->FindFunctionByName(Function->GetFName()) != nullptr)
				{
					ensureMsgf(false,
						TEXT("[AS] Shadow-UFUNCTION detected: %s::%s — inherits-by-name but no GetSuperFunction() link."),
						*BindOrder.Class->GetName(), *Function->GetName());
					continue;
				}

				if (!bUseEditorScripts && Function->HasAnyFunctionFlags(FUNC_EditorOnly))
					continue;

				FUFunctionBindPrep Prep;
				if (Function->HasAnyFunctionFlags(FUNC_BlueprintEvent | FUNC_NetFuncFlags))
				{
					BindBlueprintEvent_Prepare(ClassType.ToSharedRef(), Function, Prep);
				}
#if WITH_ANGELSCRIPT_HAZE
				else if (Function->HasAnyFunctionFlags(FUNC_NetFunction))
				{
					BindBlueprintEvent_Prepare(ClassType.ToSharedRef(), Function, Prep);
				}
#endif
				else if (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
				{
					BindBlueprintCallable_Prepare(ClassType.ToSharedRef(), Function, Prep);
				}
				else if (Function->HasMetaData(NAME_ScriptCallable))
				{
					BindBlueprintCallable_Prepare(ClassType.ToSharedRef(), Function, Prep);
				}
				else
				{
					continue;
				}

				if (Prep.Kind == FUFunctionBindPrep::EKind::Skip)
				{
					continue;
				}

				BindOrder.FunctionPreps.Add(MoveTemp(Prep));
			}

#if WITH_DEV_AUTOMATION_TESTS
			const double WorkerSeconds = FPlatformTime::Seconds() - WorkerStartSeconds;
			FAngelscriptEnumTableBaselineProbe::RecordLatePhaseSeconds(
				FAngelscriptEnumTableBaselineProbe::EBindLatePhase::Phase2_PerWorker_Sum, WorkerSeconds);
			FAngelscriptEnumTableBaselineProbe::RecordLatePhaseSeconds(
				FAngelscriptEnumTableBaselineProbe::EBindLatePhase::Phase2_PerWorker_Max, WorkerSeconds);
#endif
		};

		// Unbalanced: per-UClass workload varies wildly (some classes have 1 UFunction,
		// some have 100+). Unbalanced reduces tail latency by allowing dynamic work
		// stealing instead of static partitioning.
		const EParallelForFlags Flags = bRunParallelPrepare
			? EParallelForFlags::Unbalanced
			: EParallelForFlags::ForceSingleThread;

		ParallelFor(ClassesToBind.Num(), PreparePerClass, Flags);
	}

	// ---- Phase 2B: Commit (must run on GameThread; AS Engine writes happen here) ----
	// Opt 1 + Opt 3: enable TLS caches for IsScriptDeclarationAlreadyBound global scan
	// and for GetScriptNameForFunction prefix-conflict detection. The cache only covers
	// the commit window (prepare path is already metadata-only and does not look up
	// global script declarations).
	{
		ensureMsgf(IsInGameThread(),
			TEXT("[AS] Bind_Defaults Late+100 Phase 2B commit must run on GameThread (AS Engine writes are not thread-safe)."));
		AS_BIND_PHASE_SCOPE(EBindLatePhase::Phase2_Commit);
		FScopedBindCaches ScopedBindCaches;

		for (auto& BindOrder : ClassesToBind)
		{
			auto ClassType = BindOrder.Type;

			for (auto& Prep : BindOrder.FunctionPreps)
			{
				if (Prep.Kind == FUFunctionBindPrep::EKind::Skip)
					continue;

				FAngelscriptMethodBind DBMethod;

				if (Prep.Kind == FUFunctionBindPrep::EKind::Event)
				{
					BindBlueprintEvent_FromPrep(ClassType.ToSharedRef(), Prep, DBMethod);
				}
				else
				{
					BindBlueprintCallable_FromPrep(ClassType.ToSharedRef(), Prep, DBMethod);
				}

				if (DBMethod.UnrealPath.Len() != 0 && !Prep.Function->HasAnyFunctionFlags(FUNC_EditorOnly))
				{
					BindOrder.DBBind.Methods.Add(DBMethod);
					++TotalFuncsBound;
				}
			}

			// Free transient prep storage now that commit is done.
			BindOrder.FunctionPreps.Empty();
		}
	}

	const double TFuncBind = FPlatformTime::Seconds();
	const double TGetterSetter = FPlatformTime::Seconds();

	// ---- Phase 4: Inherit + BindProperties + DB write ----
	for (auto& BindOrder : ClassesToBind)
	{
		auto ClassType = BindOrder.Type;
		auto* SuperClass = BindOrder.Class->GetSuperClass();

		if (BindOrder.ScriptType == nullptr)
		{
			continue;
		}

		FString TypeName = BindOrder.Type->GetAngelscriptTypeName();
		FAngelscriptBinds Binds = FAngelscriptBinds::ExistingClass(TypeName);

		if (BindOrder.InheritScriptType != nullptr)
		{
			// Inherit everything from superclass
			BindOrder.ScriptType->CopySystemType(BindOrder.InheritScriptType);
		}

		// Bind UObject properties
		BindProperties(Binds, ClassType.ToSharedRef(), BindOrder.DBBind.Properties);

		BindStaticClass(Binds, TypeName, BindOrder.Class);

		BindOrder.DBBind.TypeName = TypeName;
		BindOrder.DBBind.UnrealPath = BindOrder.Class->GetPathName();
		FAngelscriptBindDatabase::Get().Classes.Add(BindOrder.DBBind);
	}

	const double TPropsInherit = FPlatformTime::Seconds();

	// ---- Phase 5: C++ UInterface method auto-registration ----
	// Interface methods are not picked up by the Phase 2 TFieldIterator<UFunction>(ExcludeSuper)
	// loop because interface functions have GetOuter() == InterfaceUClass, not the implementing
	// class. BlueprintCallableReflectiveFallback also explicitly rejects CLASS_Interface.
	// This phase scans all registered interface UClasses and registers their BlueprintCallable
	// methods as AS generic methods using the shared CallInterfaceMethod dispatcher.
	{
		extern ANGELSCRIPTRUNTIME_API void CallInterfaceMethod(class asIScriptGeneric* InGeneric);

		int32 TotalInterfaceMethodsBound = 0;

		// Collect interface classes that have been registered as AS types
		struct FInterfaceBindEntry
		{
			UClass* InterfaceClass = nullptr;
			FString TypeName;
		};
		TArray<FInterfaceBindEntry> InterfacesToBind;

		for (auto& BindOrder : ClassesToBind)
		{
			UClass* Class = BindOrder.Class;
			if (Class == nullptr || Class == UInterface::StaticClass())
				continue;
			if (!Class->HasAnyClassFlags(CLASS_Interface))
				continue;
			if (!Class->HasAnyClassFlags(CLASS_Native))
				continue;
			if (BindOrder.ScriptType == nullptr)
				continue;

			FInterfaceBindEntry Entry;
			Entry.InterfaceClass = Class;
			Entry.TypeName = BindOrder.Type->GetAngelscriptTypeName();
			InterfacesToBind.Add(Entry);
		}

		UE_LOG(Angelscript, Verbose, TEXT("[Interface] Collected %d native interface types for auto-binding"), InterfacesToBind.Num());
		for (auto& Entry : InterfacesToBind)
		{
			UE_LOG(Angelscript, Verbose, TEXT("[Interface]   Type: %s (UClass: %s)"), *Entry.TypeName, *Entry.InterfaceClass->GetName());
		}

		// Round 1: Register each interface's own methods
		for (auto& Entry : InterfacesToBind)
		{
			FAngelscriptBinds Binds = FAngelscriptBinds::ExistingClass(Entry.TypeName);

			for (TFieldIterator<UFunction> FuncIt(Entry.InterfaceClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
			{
				UFunction* Function = *FuncIt;

				// Skip UInterface base methods
				if (Function->GetOuter() == UInterface::StaticClass())
					continue;

				// Only bind BlueprintCallable/Event/Pure methods
				if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_BlueprintPure))
					continue;

				// Skip functions already manually bound or excluded
				if (FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(Function))
					continue;

				// Build AS type info for return type and arguments
				FAngelscriptTypeUsage ReturnType;
				TArray<FAngelscriptTypeUsage> ArgumentTypes;
				TArray<FString> ArgumentNames;
				TArray<FString> ArgumentDefaults;
				bool bAllTypesValid = true;

				for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
				{
					FProperty* Property = *PropIt;
					FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromProperty(Property);
					if (!Type.IsValid())
					{
						bAllTypesValid = false;
						break;
					}

					if (Property->PropertyFlags & CPF_ReturnParm)
					{
						ReturnType = Type;
					}
					else
					{
						ArgumentTypes.Add(Type);
						ArgumentNames.Add(Property->GetName());
						ArgumentDefaults.Add(TEXT("-"));
					}
				}

				if (!bAllTypesValid)
					continue;

				FString FuncName = Function->GetName();
				FString Declaration = FAngelscriptType::BuildFunctionDeclaration(
					ReturnType, FuncName, ArgumentTypes, ArgumentNames, ArgumentDefaults,
					Function->HasAnyFunctionFlags(FUNC_Const));

				// Check if this method is already registered on the type (e.g. by manual binding)
				asITypeInfo* InterfaceScriptType = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*Entry.TypeName));
				if (InterfaceScriptType != nullptr && InterfaceScriptType->GetMethodByName(TCHAR_TO_ANSI(*FuncName)) != nullptr)
					continue;

				FInterfaceMethodSignature* Sig = FAngelscriptEngine::Get().RegisterInterfaceMethodSignature(FName(*FuncName));
				Binds.GenericMethod(Declaration, CallInterfaceMethod, Sig);
				++TotalInterfaceMethodsBound;

				UE_LOG(Angelscript, Verbose,
					TEXT("[Interface]   %s::%s → %s"),
					*Entry.TypeName, *FuncName, *Declaration);
			}
		}

		// Round 2: Link interface inheritance — copy parent interface methods to child interfaces
		for (auto& Entry : InterfacesToBind)
		{
			UClass* SuperInterface = Entry.InterfaceClass->GetSuperClass();
			if (SuperInterface == nullptr || SuperInterface == UInterface::StaticClass())
				continue;
			if (!SuperInterface->HasAnyClassFlags(CLASS_Interface))
				continue;

			auto SuperType = FAngelscriptType::GetByClass(SuperInterface);
			if (!SuperType.IsValid())
				continue;

			asITypeInfo* ChildScriptType = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*Entry.TypeName));
			asITypeInfo* ParentScriptType = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*SuperType->GetAngelscriptTypeName()));
			if (ChildScriptType != nullptr && ParentScriptType != nullptr)
			{
				ChildScriptType->CopySystemType(ParentScriptType);
			}
		}

		if (TotalInterfaceMethodsBound > 0)
		{
			UE_LOG(Angelscript, Log,
				TEXT("[Interface] Auto-registered %d C++ UInterface methods across %d interface types."),
				TotalInterfaceMethodsBound, InterfacesToBind.Num());
		}
	}

	const double TInterfaceBind = FPlatformTime::Seconds();

	UE_LOG(Angelscript, Log,
		TEXT("[Profiling] blueprinttype bindings breakdown: classes=%d funcs_bound=%d | ")
		TEXT("collect=%.1fms func_bind=%.1fms getter_setter=%.1fms props_inherit=%.1fms interface=%.1fms | total=%.1fms"),
		ClassesToBind.Num(), TotalFuncsBound,
		(TCollect - T0) * 1000.0,
		(TFuncBind - TCollect) * 1000.0,
		(TGetterSetter - TFuncBind) * 1000.0,
		(TPropsInherit - TGetterSetter) * 1000.0,
		(TInterfaceBind - TPropsInherit) * 1000.0,
		(TInterfaceBind - T0) * 1000.0);
});

#endif // AS_USE_BIND_DB

/*
 * Bind TSubclassOf<> template
 */
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_TSubclassOf_Declaration((int32)FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bTemplate = true;
	Flags.TemplateType = "<T>";
	Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;

	auto TSubclassOf_ = FAngelscriptBinds::ValueClass<TSubclassOf<UObject>>("TSubclassOf<class T>", Flags);
	TSubclassOf_.Constructor("void f()", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::Construct));
	TSubclassOf_.Constructor("void f(const TSubclassOf<T>& Other)", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::CopyConstruct));
	TSubclassOf_.Method("TSubclassOf<T>& opAssign(const TSubclassOf<T>& Other)", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::Assign));

	TSubclassOf_.TemplateCallback("bool f(int&in Type, int&out ErrorMessage)",
	[](asITypeInfo* TemplateType, asCString* ErrorMessage) -> bool
	{
		if (TemplateType->GetSubTypeCount() != 1)
			return false;

		auto* SubType = TemplateType->GetSubType(0);
		if (SubType == nullptr || SubType->GetFlags() & asOBJ_VALUE)
		{
			if (ErrorMessage != nullptr)
				*ErrorMessage = "Subtype must be a class type";
			return false;
		}
		return true;
	});
});

struct FSubclassOfType : TAngelscriptCppType<TSubclassOf<UObject>>
{
	static asITypeInfo* BaseTypeInfo;

	virtual FString GetAngelscriptTypeName() const override
	{
		return TEXT("TSubclassOf");
	}

	virtual FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const override
	{
		return TEXT("TSubclassOf");
	}

	UClass* GetMetaClass(const FAngelscriptTypeUsage& Usage) const
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;
		return Usage.SubTypes[0].GetClass();
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Usage.SubTypes.Num() == 0)
			return false;

		if (!Usage.SubTypes[0].IsValid())
			return false;

		// At analyze-time we don't have an actual UClass yet for script classes, so assume it will be created in time
		if (Usage.SubTypes[0].Type->IsObjectPointer() && Usage.SubTypes[0].Type.Get() != this && Usage.SubTypes[0].ScriptClass != nullptr)
			return true;

		UClass* SubClass = Usage.SubTypes[0].GetClass();
		if (SubClass == nullptr)
			return false;

		return true;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;

		UClass* SubClass = Usage.SubTypes[0].GetClass();
		check(SubClass);

		auto* Property = new FClassProperty(Params.Outer, Params.PropertyName, RF_Public);
		Property->PropertyFlags |= CPF_UObjectWrapper;
		Property->PropertyClass = UClass::StaticClass();
		Property->SetMetaClass(SubClass);

		return Property;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		const FClassProperty* ClassProp = CastField<FClassProperty>(Property);
		if (ClassProp == nullptr)
			return false;
		if ((ClassProp->PropertyFlags & CPF_UObjectWrapper) == 0)
			return false;

		UClass* AssociatedClass = GetMetaClass(Usage);
		if (AssociatedClass != nullptr)
		{
			return ClassProp->MetaClass == AssociatedClass;
		}
		else
		{
			if (Usage.SubTypes.Num() == 0)
				return false;
			if (Usage.SubTypes[0].ScriptClass == nullptr)
				return false;

			// Workaround: We don't know our actual type yet, so
			// we compare the script types by name.
			FString CheckName = ANSI_TO_TCHAR(Usage.SubTypes[0].ScriptClass->GetName());
			CheckName.RemoveFromStart(TEXT("U"));
			CheckName.RemoveFromStart(TEXT("A"));

			FString PropClassName = ClassProp->MetaClass->GetName();
			return PropClassName == CheckName;
		}
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	virtual UClass* GetClass(const FAngelscriptTypeUsage& Usage) const override
	{
		return nullptr;
	}

	bool DescribesCompleteType(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() >= 1 && Usage.SubTypes[0].IsValid();
	}

	bool HasReferences(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const override
	{
		//Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::Reference));
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		TSubclassOf<UObject>* StructMemory = (TSubclassOf<UObject>*)Data.StackPtr;
		new (StructMemory) TSubclassOf<UObject>();

		if (Usage.bIsReference)
		{
			TSubclassOf<UObject>& RefValue = Stack.StepCompiledInRef<FClassProperty, TSubclassOf<UObject>>(StructMemory);
			Context->SetArgAddress(ArgumentIndex, &RefValue);
		}
		else
		{
			Stack.StepCompiledIn<FClassProperty>(StructMemory);
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

			*(TSubclassOf<UObject>*)Destination = *(TSubclassOf<UObject>*)(ReturnedObject);
		}
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("null") || InValue == TEXT("nullptr") || InValue == TEXT(""))
		{
			OutValue = TEXT("nullptr");
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("null") || InValue == TEXT("nullptr"))
		{
			OutValue = TEXT("");
			return true;
		}
		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		UClass* MetaClass = GetMetaClass(Usage);
		if (MetaClass != nullptr)
		{
			FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(MetaClass);
			if (ClassHeaderPath.Len() != 0)
			{
				OutCppForm.CppType = FString::Printf(TEXT("TSubclassOf<%s%s>"), MetaClass->GetPrefixCPP(), *MetaClass->GetName());
				OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *ClassHeaderPath);
			}
		}

		OutCppForm.CppGenericType = TEXT("TSubclassOf<UObject>");
		OutCppForm.TemplateObjectForm = TEXT("TSubclassOf<UObject>");
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		TSubclassOf<UObject>& SubclassOf = Usage.ResolvePrimitive<TSubclassOf<UObject>>(Address);
		UObject* Object = SubclassOf.Get();
		if (Usage.ScriptClass != nullptr)
			Value.Type = Usage.ScriptClass->GetName();

		Value.Usage = Usage;
		Value.Address = Address;
		Value.SetAddressToMonitor(&SubclassOf, sizeof(TSubclassOf<UObject>));

		if (Object == nullptr || Object->GetClass() == nullptr)
		{
			Value.Value = TEXT("nullptr");
			Value.bHasMembers = false;
		}
		else
		{
			Value.Value = FString::Printf(TEXT("{ %s }"), *Object->GetName());
			Value.bHasMembers = false;
		}

		return true;
	}
};

asITypeInfo* FSubclassOfType::BaseTypeInfo = nullptr;

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_TSubclassOf((int32)FAngelscriptBinds::EOrder::Late-10, []
{
	auto TSubclassOf_ = FAngelscriptBinds::ExistingClass("TSubclassOf<T>");
	FSubclassOfType::BaseTypeInfo = FAngelscriptEngine::Get().Engine->GetTypeInfoByName("TSubclassOf");

	TSubclassOf_.ImplicitConstructor("void f(UClass Class)", FUNC(FAngelscriptSubclassOfHelpers::ImplicitConstruct));
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	TSubclassOf_.Method("UClass opImplConv() const", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::GetClass));
	TSubclassOf_.Method("UObject opImplConv() const", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::GetClass));

	TSubclassOf_.Method("void Set(UClass Class) const", FUNC(FAngelscriptSubclassOfHelpers::SetClass));
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();
	TSubclassOf_.Method("void opAssign(UClass Class)", FUNC(FAngelscriptSubclassOfHelpers::SetClass));
	FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam();

	TSubclassOf_.Method("bool opEquals(const TSubclassOf<T>& Other) const", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::OpEquals));
	TSubclassOf_.Method("bool opEquals(UClass Other) const", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::OpEqualsClass));

	TSubclassOf_.Method("UClass Get() const", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::GetClass));
	TSubclassOf_.Method("bool IsValid() const", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::IsValid));
	TSubclassOf_.Method("bool IsChildOf(UClass Other) const", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::IsChildOf));
	TSubclassOf_.Method("T handle_only GetDefaultObject() const", FUNC_TRIVIAL(FAngelscriptSubclassOfHelpers::GetDefaultObject));
});

struct FObjectPtrType : TAngelscriptCppType<TObjectPtr<UObject>>
{
	static asITypeInfo* BaseTypeInfo;

	virtual FString GetAngelscriptTypeName() const override
	{
		return TEXT("TObjectPtr");
	}

	virtual FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const override
	{
		return TEXT("TObjectPtr");
	}

	virtual bool IsUnresolvedObjectPointer() const override
	{
		return true;
	}

	virtual FString GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode) const override
	{
		if (Usage.SubTypes[0].IsValid())
		{
			if (Mode == EAngelscriptDeclarationMode::MemberVariable)
			{
				// If we're binding this as a member variable, we use a special classifier
				// to tell the angelscript compiler that this needs to be resolved as a TObjectPtr
				return Usage.SubTypes[0].GetAngelscriptDeclaration(Mode) + TEXT(" unresolved_object");
			}
			else if (Mode == EAngelscriptDeclarationMode::PreResolvedObject)
			{
				return Usage.SubTypes[0].GetAngelscriptDeclaration(Mode);
			}
		}

		// Expose the default TObjectPtr<UObject> declaration otherwise
		return FAngelscriptType::GetAngelscriptDeclaration(Usage, Mode);
	}

	UClass* GetObjectClass(const FAngelscriptTypeUsage& Usage) const
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;
		return Usage.SubTypes[0].GetClass();
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Usage.SubTypes.Num() == 0)
			return false;

		if (!Usage.SubTypes[0].IsValid())
			return false;

		// At analyze-time we don't have an actual UClass yet for script classes, so assume it will be created in time
		if (Usage.SubTypes[0].Type->IsObjectPointer() && Usage.SubTypes[0].Type.Get() != this && Usage.SubTypes[0].ScriptClass != nullptr)
			return true;

		UClass* SubClass = Usage.SubTypes[0].GetClass();
		if (SubClass == nullptr)
			return false;

		return true;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;

		UClass* ObjectClass = Usage.SubTypes[0].GetClass();
		check(ObjectClass);

		auto* Property = new FObjectProperty(Params.Outer, Params.PropertyName, RF_Public);
		Property->PropertyFlags |= CPF_TObjectPtr;
		Property->PropertyClass = ObjectClass;

		return Property;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		const FObjectProperty* ObjectPtrProp = CastField<FObjectProperty>(Property);
		if (ObjectPtrProp == nullptr)
			return false;
		if ((ObjectPtrProp->PropertyFlags & CPF_TObjectPtr) == 0)
			return false;

		UClass* AssociatedClass = GetObjectClass(Usage);
		if (AssociatedClass != nullptr)
		{
			return ObjectPtrProp->PropertyClass == AssociatedClass;
		}
		else
		{
			if (Usage.SubTypes.Num() == 0)
				return false;
			if (Usage.SubTypes[0].ScriptClass == nullptr)
				return false;

			// Workaround: We don't know our actual type yet, so
			// we compare the script types by name.
			FString CheckName = ANSI_TO_TCHAR(Usage.SubTypes[0].ScriptClass->GetName());
			CheckName.RemoveFromStart(TEXT("U"));
			CheckName.RemoveFromStart(TEXT("A"));

			FString PropClassName = ObjectPtrProp->PropertyClass->GetName();
			return PropClassName == CheckName;
		}
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	virtual UClass* GetClass(const FAngelscriptTypeUsage& Usage) const override
	{
		return nullptr;
	}

	bool DescribesCompleteType(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() >= 1 && Usage.SubTypes[0].IsValid();
	}

	bool HasReferences(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const override
	{
		//Params.Schema->Add(UE::GC::DeclareMember(Params.Names.Top(), Params.AtOffset, UE::GC::EMemberType::Reference));
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		TObjectPtr<UObject>* StructMemory = (TObjectPtr<UObject>*)Data.StackPtr;
		new (StructMemory) TObjectPtr<UObject>();

		if (Usage.bIsReference)
		{
			TObjectPtr<UObject>& RefValue = Stack.StepCompiledInRef<FObjectProperty, TObjectPtr<UObject>>(StructMemory);
			Context->SetArgAddress(ArgumentIndex, &RefValue);
		}
		else
		{
			Stack.StepCompiledIn<FClassProperty>(StructMemory);
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

			*(TObjectPtr<UObject>*)Destination = *(TObjectPtr<UObject>*)(ReturnedObject);
		}
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("null") || InValue == TEXT("nullptr") || InValue == TEXT(""))
		{
			OutValue = TEXT("nullptr");
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("null") || InValue == TEXT("nullptr"))
		{
			OutValue = TEXT("");
			return true;
		}
		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		UClass* ObjectClass = GetObjectClass(Usage);
		if (ObjectClass != nullptr)
		{
			FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(ObjectClass);
			if (ClassHeaderPath.Len() != 0)
			{
				OutCppForm.CppType = FString::Printf(TEXT("TObjectPtr<%s%s>"), ObjectClass->GetPrefixCPP(), *ObjectClass->GetName());
				OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *ClassHeaderPath);
			}
		}

		OutCppForm.CppGenericType = TEXT("TObjectPtr<UObject>");
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		TObjectPtr<UObject>& ObjectPtr = Usage.ResolvePrimitive<TObjectPtr<UObject>>(Address);
		UObject* Object = ObjectPtr.Get();
		if (Usage.ScriptClass != nullptr)
			Value.Type = Usage.ScriptClass->GetName();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.SetAddressToMonitor(&ObjectPtr, sizeof(FObjectHandle));

		FUObjectType::FillObjectDebuggerValue(Object, Value);
		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		UObject* Object = Usage.ResolvePrimitive<TObjectPtr<UObject>>(Address).Get();
		if (Object == nullptr)
			return false;

		FUObjectType::FillObjectDebuggerScope(Object, Scope);
		return true;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		UObject* Object = Usage.ResolvePrimitive<TObjectPtr<UObject>>(Address).Get();
		if (Object == nullptr)
			return false;

		return FUObjectType::FillObjectDebuggerMember(Object, Member, Value);
	}
};

asITypeInfo* FObjectPtrType::BaseTypeInfo = nullptr;

/*
 * Bind TObjectPtr<> template
 */
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_TObjectPtr_Declaration((int32)FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bTemplate = true;
	Flags.TemplateType = "<T>";
	Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;

	auto TObjectPtr_ = FAngelscriptBinds::ValueClass<TObjectPtr<UObject>>("TObjectPtr<class T>", Flags);
	TObjectPtr_.Constructor("void f()", [](TObjectPtr<UObject>* Ptr)
	{
		new (Ptr) TObjectPtr<UObject>(nullptr);
	});

	TObjectPtr_.Constructor("void f(const TObjectPtr<T>& Other)", [](TObjectPtr<UObject>* Ptr, const TObjectPtr<UObject>* Other)
	{
		new (Ptr) TObjectPtr<UObject>(*Other);
	});

	TObjectPtr_.Method("TObjectPtr<T>& opAssign(const TObjectPtr<T>& Other)", [](TObjectPtr<UObject>* Ptr, const TObjectPtr<UObject>* Other) -> TObjectPtr<UObject>&
	{
		*Ptr = *Other;
		return *Ptr;
	});

	TObjectPtr_.TemplateCallback("bool f(int&in Type, int&out ErrorMessage)",
	[](asITypeInfo* TemplateType, asCString* ErrorMessage) -> bool
	{
		if (TemplateType->GetSubTypeCount() != 1)
			return false;

		auto* SubType = TemplateType->GetSubType(0);
		if (SubType == nullptr || SubType->GetFlags() & asOBJ_VALUE)
		{
			if (ErrorMessage != nullptr)
				*ErrorMessage = "Subtype must be a class type";
			return false;
		}
		return true;
	});
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_TObjectPtr((int32)FAngelscriptBinds::EOrder::Late-10, []
{
	auto TObjectPtr_ = FAngelscriptBinds::ExistingClass("TObjectPtr<T>");
	FObjectPtrType::BaseTypeInfo = FAngelscriptEngine::Get().Engine->GetTypeInfoByName("TObjectPtr");

	TObjectPtr_.ImplicitConstructor("void f(T handle_only Object)", [](TObjectPtr<UObject>* Ptr, UObject* Object)
	{
		new (Ptr) TObjectPtr<UObject>(Object);
	});

	TObjectPtr_.Method("T handle_only opImplConv() const", [](const TObjectPtr<UObject>* Ptr)
	{
		return Ptr->Get();
	});

	TObjectPtr_.Method("bool opEquals(const TObjectPtr<T>& Other) const",
	[](const TObjectPtr<UObject>& Self, const TObjectPtr<UObject>& Other) -> bool
	{
		return Self == Other;
	});

	TObjectPtr_.Method("bool opEquals(const T handle_only Other) const",
	[](const TObjectPtr<UObject>& Self, UObject* Other) -> bool
	{
		return Self == Other;
	});

	TObjectPtr_.Method("TObjectPtr<T>& opAssign(T handle_only Other)",
	[](TObjectPtr<UObject>* Self, UObject* Other)
	{
		*Self = Other;
		return Self;
	});

	TObjectPtr_.Method("T handle_only Get() const", [](TObjectPtr<UObject>* Ptr)
	{
		return Ptr->Get();
	});
});

struct FWeakObjectPtrType : TAngelscriptCppType<TWeakObjectPtr<UObject>>
{
	static asITypeInfo* BaseTypeInfo;

	virtual FString GetAngelscriptTypeName() const override
	{
		return TEXT("TWeakObjectPtr");
	}

	virtual FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const override
	{
		return TEXT("TWeakObjectPtr");
	}

	UClass* GetObjectClass(const FAngelscriptTypeUsage& Usage) const
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;
		return Usage.SubTypes[0].GetClass();
	}

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override
	{
		if (Usage.SubTypes.Num() == 0)
			return false;

		if (!Usage.SubTypes[0].IsValid())
			return false;

		// At analyze-time we don't have an actual UClass yet for script classes, so assume it will be created in time
		if (Usage.SubTypes[0].Type->IsObjectPointer() && Usage.SubTypes[0].Type.Get() != this && Usage.SubTypes[0].ScriptClass != nullptr)
			return true;

		UClass* SubClass = Usage.SubTypes[0].GetClass();
		if (SubClass == nullptr)
			return false;

		return true;
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;

		UClass* ObjectClass = Usage.SubTypes[0].GetClass();
		check(ObjectClass);

		auto* Property = new FWeakObjectProperty(Params.Outer, Params.PropertyName, RF_Public);
		Property->PropertyClass = ObjectClass;

		return Property;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		const FWeakObjectProperty* ObjectPtrProp = CastField<FWeakObjectProperty>(Property);
		if (ObjectPtrProp == nullptr)
			return false;
		if ((ObjectPtrProp->PropertyFlags & CPF_UObjectWrapper) == 0)
			return false;

		UClass* AssociatedClass = GetObjectClass(Usage);
		if (AssociatedClass != nullptr)
		{
			return ObjectPtrProp->PropertyClass == AssociatedClass;
		}
		else
		{
			if (Usage.SubTypes.Num() == 0)
				return false;
			if (Usage.SubTypes[0].ScriptClass == nullptr)
				return false;

			// Workaround: We don't know our actual type yet, so
			// we compare the script types by name.
			FString CheckName = ANSI_TO_TCHAR(Usage.SubTypes[0].ScriptClass->GetName());
			CheckName.RemoveFromStart(TEXT("U"));
			CheckName.RemoveFromStart(TEXT("A"));

			FString PropClassName = ObjectPtrProp->PropertyClass->GetName();
			return PropClassName == CheckName;
		}
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	virtual UClass* GetClass(const FAngelscriptTypeUsage& Usage) const override
	{
		return nullptr;
	}

	bool DescribesCompleteType(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() >= 1 && Usage.SubTypes[0].IsValid();
	}

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override
	{
		TWeakObjectPtr<UObject>* StructMemory = (TWeakObjectPtr<UObject>*)Data.StackPtr;
		new (StructMemory) TWeakObjectPtr<UObject>();

		if (Usage.bIsReference)
		{
			TWeakObjectPtr<UObject>& RefValue = Stack.StepCompiledInRef<FWeakObjectProperty, TWeakObjectPtr<UObject>>(StructMemory);
			Context->SetArgAddress(ArgumentIndex, &RefValue);
		}
		else
		{
			Stack.StepCompiledIn<FClassProperty>(StructMemory);
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

			*(TWeakObjectPtr<UObject>*)Destination = *(TWeakObjectPtr<UObject>*)(ReturnedObject);
		}
	}

	bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("null") || InValue == TEXT("nullptr") || InValue == TEXT(""))
		{
			OutValue = TEXT("nullptr");
			return true;
		}
		return false;
	}

	bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& InValue, FString& OutValue) const override
	{
		if (InValue == TEXT("null") || InValue == TEXT("nullptr"))
		{
			OutValue = TEXT("");
			return true;
		}
		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		UClass* ObjectClass = GetObjectClass(Usage);
		if (ObjectClass != nullptr)
		{
			FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(ObjectClass);
			if (ClassHeaderPath.Len() != 0)
			{
				OutCppForm.CppType = FString::Printf(TEXT("TWeakObjectPtr<%s%s>"), ObjectClass->GetPrefixCPP(), *ObjectClass->GetName());
				OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *ClassHeaderPath);
			}
		}

		OutCppForm.CppGenericType = TEXT("TWeakObjectPtr<UObject>");
		return true;
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		TWeakObjectPtr<UObject>& ObjectPtr = Usage.ResolvePrimitive<TWeakObjectPtr<UObject>>(Address);
		UObject* Object = ObjectPtr.Get();
		if (Usage.ScriptClass != nullptr)
			Value.Type = Usage.ScriptClass->GetName();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.SetAddressToMonitor(&ObjectPtr, sizeof(TWeakObjectPtr<UObject>));

		FUObjectType::FillObjectDebuggerValue(Object, Value);
		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		UObject* Object = Usage.ResolvePrimitive<TWeakObjectPtr<UObject>>(Address).Get();
		if (Object == nullptr)
			return false;

		FUObjectType::FillObjectDebuggerScope(Object, Scope);
		return true;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		UObject* Object = Usage.ResolvePrimitive<TWeakObjectPtr<UObject>>(Address).Get();
		if (Object == nullptr)
			return false;

		return FUObjectType::FillObjectDebuggerMember(Object, Member, Value);
	}
};

asITypeInfo* FWeakObjectPtrType::BaseTypeInfo = nullptr;

/*
 * Bind TWeakObjectPtr<> template
 */
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_TWeakObjectPtr_Declaration((int32)FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bTemplate = true;
	Flags.TemplateType = "<T>";
	Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;

	auto TWeakObjectPtr_ = FAngelscriptBinds::ValueClass<TWeakObjectPtr<UObject>>("TWeakObjectPtr<class T>", Flags);
	TWeakObjectPtr_.Constructor("void f()", [](TWeakObjectPtr<UObject>* Ptr)
	{
		new (Ptr) TWeakObjectPtr<UObject>(nullptr);
	});

	TWeakObjectPtr_.Constructor("void f(const TWeakObjectPtr<T>& Other)", [](TWeakObjectPtr<UObject>* Ptr, const TWeakObjectPtr<UObject>* Other)
	{
		new (Ptr) TWeakObjectPtr<UObject>(*Other);
	});

	TWeakObjectPtr_.Method("TWeakObjectPtr<T>& opAssign(const TWeakObjectPtr<T>& Other)", [](TWeakObjectPtr<UObject>* Ptr, const TWeakObjectPtr<UObject>* Other) -> TWeakObjectPtr<UObject>&
	{
		*Ptr = *Other;
		return *Ptr;
	});

	TWeakObjectPtr_.TemplateCallback("bool f(int&in Type, int&out ErrorMessage)",
	[](asITypeInfo* TemplateType, asCString* ErrorMessage) -> bool
	{
		if (TemplateType->GetSubTypeCount() != 1)
			return false;

		auto* SubType = TemplateType->GetSubType(0);
		if (SubType == nullptr || SubType->GetFlags() & asOBJ_VALUE)
		{
			if (ErrorMessage != nullptr)
				*ErrorMessage = "Subtype must be a class type";
			return false;
		}
		return true;
	});
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_TWeakObjectPtr((int32)FAngelscriptBinds::EOrder::Late-10, []
{
	auto TWeakObjectPtr_ = FAngelscriptBinds::ExistingClass("TWeakObjectPtr<T>");
	FObjectPtrType::BaseTypeInfo = FAngelscriptEngine::Get().Engine->GetTypeInfoByName("TWeakObjectPtr");

	TWeakObjectPtr_.ImplicitConstructor("void f(T handle_only Object)", [](TWeakObjectPtr<UObject>* Ptr, UObject* Object)
	{
		new (Ptr) TWeakObjectPtr<UObject>(Object);
	});

	TWeakObjectPtr_.Method("T handle_only opImplConv() const", [](const TWeakObjectPtr<UObject>* Ptr)
	{
		return Ptr->Get();
	});

	TWeakObjectPtr_.Method("bool opEquals(const TWeakObjectPtr<T>& Other) const",
	[](const TWeakObjectPtr<UObject>& Self, const TWeakObjectPtr<UObject>& Other) -> bool
	{
		return Self == Other;
	});

	TWeakObjectPtr_.Method("bool opEquals(const T handle_only Other) const",
	[](const TWeakObjectPtr<UObject>& Self, UObject* Other) -> bool
	{
		return Self == Other;
	});

	TWeakObjectPtr_.Method("TWeakObjectPtr<T>& opAssign(T handle_only Other)",
	[](TWeakObjectPtr<UObject>* Self, UObject* Other)
	{
		*Self = Other;
		return Self;
	});

	TWeakObjectPtr_.Method("T handle_only Get() const", [](TWeakObjectPtr<UObject>* Ptr)
	{
		return Ptr->Get();
	});

	TWeakObjectPtr_.Method("bool IsValid() const", [](TWeakObjectPtr<UObject>* Ptr)
	{
		return Ptr->IsValid();
	});

	TWeakObjectPtr_.Method("bool IsStale() const", [](TWeakObjectPtr<UObject>* Ptr)
	{
		return Ptr->IsStale();
	});

	TWeakObjectPtr_.Method("bool IsExplicitlyNull() const", [](TWeakObjectPtr<UObject>* Ptr)
	{
		return Ptr->IsExplicitlyNull();
	});
});

static void BindUClassLookup()
{
	// Register the type used by TSubclassOf
	TSharedRef<FSubclassOfType> SubclassOfType = MakeShared<FSubclassOfType>();
	FAngelscriptType::Register(SubclassOfType);

	// Register a type that handles script object types generically.
	// SetScriptObject takes a TSharedPtr (not TSharedRef) so we pass the
	// implicit Ref->Ptr conversion explicitly.
	TSharedRef<FUObjectType> ScriptObjectType = MakeShared<FUObjectType>(nullptr, TEXT("UObject"));
	FAngelscriptType::SetScriptObject(ScriptObjectType);

	// Register the type used by TObjectPtr
	TSharedRef<FObjectPtrType> ObjectPtrType = MakeShared<FObjectPtrType>();
	FAngelscriptType::Register(ObjectPtrType);

	// Register the type used by TWeakObjectPtr
	TSharedRef<FWeakObjectPtrType> WeakObjectPtrType = MakeShared<FWeakObjectPtrType>();
	FAngelscriptType::Register(WeakObjectPtrType);

	// Register a type finder into the type system that
	// can look up an ObjectProperty's inner angelscript type.
	FAngelscriptType::RegisterTypeFinder([=](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);
		if (ObjectProperty == nullptr)
		{
			// Detect TWeakObjectPtr properties
			const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property);
			if (WeakObjectProperty != nullptr)
			{
				FAngelscriptTypeUsage InnerType = FAngelscriptTypeUsage::FromClass(WeakObjectProperty->PropertyClass);
				if (!InnerType.IsValid())
					return false;
					
				//if (WeakObjectProperty->HasAnyPropertyFlags(CPF_ConstTemplateArg))
				//	InnerType.bIsConst = true;

				Usage.Type = WeakObjectPtrType;
				Usage.SubTypes.SetNum(1);
				Usage.SubTypes[0] = InnerType;
				return true;
			}

			return false;
		}

		// Detect TObjectPtr properties
		if ((ObjectProperty->PropertyFlags & CPF_TObjectPtr) != 0)
		{
			FAngelscriptTypeUsage InnerType = FAngelscriptTypeUsage::FromClass(ObjectProperty->PropertyClass);
			if (!InnerType.IsValid())
				return false;
			
			//if (ObjectProperty->HasAnyPropertyFlags(CPF_ConstTemplateArg))
			//	InnerType.bIsConst = true;

			Usage.Type = ObjectPtrType;
			Usage.SubTypes.SetNum(1);
			Usage.SubTypes[0] = InnerType;
			return true;
		}

		const FClassProperty* ClassProperty = CastField<FClassProperty>(Property);

		if (ClassProperty != nullptr && (ClassProperty->PropertyFlags & CPF_TObjectPtr) != 0)
		{
			FAngelscriptTypeUsage InnerType = FAngelscriptTypeUsage::FromClass(ClassProperty->PropertyClass);
			if (!InnerType.IsValid())
				return false;
			
			//if (ClassProperty->HasAnyPropertyFlags(CPF_ConstTemplateArg))
			//	InnerType.bIsConst = true;

			Usage.Type = ObjectPtrType;
			Usage.SubTypes.SetNum(1);
			Usage.SubTypes[0] = InnerType;
			return true;
		}

		// Class properties are sometimes TSubclassOf<>
		if (ClassProperty != nullptr && (ClassProperty->PropertyFlags & CPF_UObjectWrapper) != 0)
		{
			FAngelscriptTypeUsage InnerType = FAngelscriptTypeUsage::FromClass(ClassProperty->MetaClass);
			if (!InnerType.IsValid())
				return false;

			//if (ClassProperty->HasAnyPropertyFlags(CPF_ConstTemplateArg))
			//	InnerType.bIsConst = true;

			Usage.Type = SubclassOfType;
			Usage.SubTypes.SetNum(1);
			Usage.SubTypes[0] = InnerType;
			return true;
		}

		// Look up a regular object property type
		Usage = FAngelscriptTypeUsage::FromClass(ObjectProperty->PropertyClass);
		return Usage.IsValid();
	});
}
