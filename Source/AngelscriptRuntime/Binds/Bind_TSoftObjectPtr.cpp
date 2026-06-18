#include "CoreMinimal.h"
#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "AngelscriptEngine.h"
#include "AngelscriptBindDatabase.h"
#include "AngelscriptDocs.h"
#include "FunctionLibraries/SoftReferenceStatics.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#include "Binds/Helper_StructType.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
//#include "as_scriptfunction.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

struct FBaseSoftReferenceType : public TAngelscriptCppType<FSoftObjectPtr>
{
	UClass* GetSubTypeClass(const FAngelscriptTypeUsage& Usage) const
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;
		return Usage.SubTypes[0].GetClass();
	}

	virtual UClass* GetClassOfObject(const FAngelscriptTypeUsage& Usage) const
	{
		return nullptr;
	}

	bool DescribesCompleteType(const FAngelscriptTypeUsage& Usage) const override
	{
		return Usage.SubTypes.Num() >= 1 && Usage.SubTypes[0].IsValid();
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

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override { return true; }
	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const override
	{
		FSoftObjectPtr* ValuePtr = (FSoftObjectPtr*)Data.StackPtr;
		new(ValuePtr) FSoftObjectPtr();

		if (Usage.bIsReference)
		{
			FSoftObjectPtr& ObjRef = Stack.StepCompiledInRef<FSoftObjectProperty, FSoftObjectPtr>(ValuePtr);
			Context->SetArgAddress(ArgumentIndex, &ObjRef);
		}
		else
		{
			Stack.StepCompiledIn<FSoftObjectProperty>(ValuePtr);
			Context->SetArgObject(ArgumentIndex, ValuePtr);
		}
	}

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override
	{
		return true;
	}

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override
	{
		if(Usage.bIsReference)
		{
			*(FSoftObjectPtr**)Destination = (FSoftObjectPtr*)Context->GetReturnAddress();
		}
		else
		{
			void* ReturnedObject = Context->GetReturnObject();
			if (ReturnedObject == nullptr)
				return;
			*(FSoftObjectPtr*)Destination = *(FSoftObjectPtr*)ReturnedObject;
		}
	}

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override
	{
		FSoftObjectPtr& SoftPtr = Usage.ResolvePrimitive<FSoftObjectPtr>(Address);

		UObject* Object = SoftPtr.Get();
		if (Object == nullptr)
		{
			Value.Value = FString::Printf(TEXT("{ Pending %s }"), *SoftPtr.ToString());
			Value.Type = Usage.GetAngelscriptDeclaration();
			Value.Usage = Usage;
			Value.Address = Address;
			Value.bHasMembers = false;
			return true;
		}

		FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(GetClassOfObject(Usage)));
		if (ObjectUsage.IsValid())
		{
			const UObject*& ObjectRef = Value.AllocatePODLiteral<const UObject*>();
			ObjectRef = Object;

			if (ObjectUsage.GetDebuggerValue(&ObjectRef, Value))
			{
				Value.Type = Usage.GetAngelscriptDeclaration();
				return true;
			}
		}

		Value.Value = FString::Printf(TEXT("{ Object %s }"), *SoftPtr.ToString());
		Value.Type = Usage.GetAngelscriptDeclaration();
		Value.Usage = Usage;
		Value.Address = Address;
		Value.bHasMembers = false;
		return true;
	}

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override
	{
		FSoftObjectPtr& SoftPtr = Usage.ResolvePrimitive<FSoftObjectPtr>(Address);

		UObject* Object = SoftPtr.Get();
		if (Object == nullptr)
			return false;
		
		FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(GetClassOfObject(Usage)));
		if (ObjectUsage.IsValid())
		{
			if (ObjectUsage.GetDebuggerScope(&Object, Scope))
				return true;
		}

		return false;
	}

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override
	{
		FSoftObjectPtr& SoftPtr = Usage.ResolvePrimitive<FSoftObjectPtr>(Address);

		UObject* Object = SoftPtr.Get();
		if (Object == nullptr)
			return false;
		
		FAngelscriptTypeUsage ObjectUsage(FAngelscriptType::GetByClass(GetClassOfObject(Usage)));
		if (ObjectUsage.IsValid())
		{
			if (ObjectUsage.GetDebuggerMember(&Object, Member, Value))
				return true;
		}

		return false;
	}
};

struct FSoftObjectPtrType : public FBaseSoftReferenceType
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("TSoftObjectPtr");
	}

	virtual UClass* GetClassOfObject(const FAngelscriptTypeUsage& Usage) const
	{
		return GetSubTypeClass(Usage);
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;

		auto* ObjectProp = new FSoftObjectProperty(Params.Outer, Params.PropertyName);
		ObjectProp->PropertyClass = GetClassOfObject(Usage);

		return ObjectProp;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		const FSoftObjectProperty* ObjProp = CastField<FSoftObjectProperty>(Property);
		if (ObjProp == nullptr)
			return false;
		return true;
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		UClass* MetaClass = GetClassOfObject(Usage);
		if (MetaClass != nullptr)
		{
			const FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(MetaClass);
			if (!ClassHeaderPath.IsEmpty())
			{
				OutCppForm.CppType = FString::Printf(TEXT("TSoftObjectPtr<%s%s>"), MetaClass->GetPrefixCPP(), *MetaClass->GetName());
				OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *ClassHeaderPath);
			}
		}

		OutCppForm.CppGenericType = TEXT("TSoftObjectPtr<UObject>");
		OutCppForm.TemplateObjectForm = TEXT("TSoftObjectPtr<UObject>");
		return true;
	}
};

struct FSoftClassPtrType : public FBaseSoftReferenceType
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("TSoftClassPtr");
	}

	virtual UClass* GetClassOfObject(const FAngelscriptTypeUsage& Usage) const
	{
		return UClass::StaticClass();
	}

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FAngelscriptType::FPropertyParams& Params) const override
	{
		if (Usage.SubTypes.Num() == 0)
			return nullptr;

		auto* ClassProp = new FSoftClassProperty(Params.Outer, Params.PropertyName);
		ClassProp->PropertyClass = UClass::StaticClass();
		ClassProp->MetaClass = GetSubTypeClass(Usage);

		return ClassProp;
	}

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override
	{
		const FSoftClassProperty* ClassProp = CastField<FSoftClassProperty>(Property);
		if (ClassProp == nullptr)
			return false;
		return true;
	}

	bool CanQueryPropertyType() const override
	{
		return false;
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		UClass* MetaClass = GetSubTypeClass(Usage);
		if (MetaClass != nullptr)
		{
			const FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(MetaClass);
			if (!ClassHeaderPath.IsEmpty())
			{
				OutCppForm.CppType = FString::Printf(TEXT("TSoftClassPtr<%s%s>"), MetaClass->GetPrefixCPP(), *MetaClass->GetName());
				OutCppForm.CppHeader = FString::Printf(TEXT("#include \"%s\""), *ClassHeaderPath);
			}
		}

		OutCppForm.CppGenericType = TEXT("TSoftClassPtr<UObject>");
		OutCppForm.TemplateObjectForm = TEXT("TSoftClassPtr<UObject>");
		return true;
	}
};

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_SoftReferences_Declaration((int32)FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bTemplate = true;
	Flags.TemplateType = "<T>";
	Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;

	TSharedRef<FSoftObjectPtrType> SoftObjectPtrType = MakeShared<FSoftObjectPtrType>();
	FAngelscriptType::Register(SoftObjectPtrType);

	TSharedRef<FSoftClassPtrType> SoftClassPtrType = MakeShared<FSoftClassPtrType>();
	FAngelscriptType::Register(SoftClassPtrType);

	auto TSoftObjectPtr_ = FAngelscriptBinds::ValueClass<FSoftObjectPtr>("TSoftObjectPtr<class T>", Flags);
	TSoftObjectPtr_.TemplateCallback("bool f(int&in Type, int&out ErrorMessage)",
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

	auto TSoftClassPtr_ = FAngelscriptBinds::ValueClass<FSoftObjectPtr>("TSoftClassPtr<class T>", Flags);
	TSoftClassPtr_.TemplateCallback("bool f(int&in Type, int&out ErrorMessage)",
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

	FAngelscriptType::RegisterTypeFinder([SoftObjectPtrType, SoftClassPtrType](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
	{
		FSoftObjectProperty* ObjectProperty = CastField<FSoftObjectProperty>(Property);
		if (ObjectProperty == nullptr)
			return false;

		// Soft object pointer could be a soft class pointer
		FSoftClassProperty* ClassProperty = CastField<FSoftClassProperty>(Property);
		if (ClassProperty != nullptr)
		{
			auto SubClass = FAngelscriptType::GetByClass(ClassProperty->MetaClass);
			if (!SubClass.IsValid())
				return false;
			
			Usage.Type = SoftClassPtrType;

			FAngelscriptTypeUsage& SubType = Usage.SubTypes.Emplace_GetRef();
			SubType.Type = SubClass;
			if (!SubType.IsValid())
				return false;
			return true;
		}

		// Just a regular soft object
		auto SubClass = FAngelscriptType::GetByClass(ObjectProperty->PropertyClass);
		if (!SubClass.IsValid())
			return false;
		
		Usage.Type = SoftObjectPtrType;
		FAngelscriptTypeUsage& SubType = Usage.SubTypes.Emplace_GetRef();
		SubType.Type = SubClass;
		return true;
	});
});

static UClass* GetSoftPtrSubType()
{
	asITypeInfo* TemplateType = FAngelscriptEngine::GetCurrentFunctionObjectType();
	auto* SubType = TemplateType->GetSubType(0);
	return (UClass*)SubType->GetUserData();
}

void BindSoftPtrBaseMethods(FAngelscriptBinds& SoftPtr_)
{
	SoftPtr_.Constructor("void f()", [](FSoftObjectPtr* Ptr)
	{
		new(Ptr) FSoftObjectPtr();
	});

	SoftPtr_.Constructor("void f(const FSoftObjectPath& Path)", [](FSoftObjectPtr* Ptr, FSoftObjectPath& Path)
	{
		new(Ptr) FSoftObjectPtr(Path);
	});

	SoftPtr_.Destructor("void f()", [](FSoftObjectPtr* Self)
	{
		Self->~FSoftObjectPtr();
	});

	SoftPtr_.Method("FSoftObjectPath ToSoftObjectPath() const", [](FSoftObjectPtr* Self) -> FSoftObjectPath
	{
		return Self->ToSoftObjectPath();
	});

	SoftPtr_.Method("FString ToString() const", [](FSoftObjectPtr* Self) -> FString
	{
		return Self->ToSoftObjectPath().ToString();
	});

	SoftPtr_.Method("FString GetLongPackageName() const", [](FSoftObjectPtr* Self) -> FString
	{
		return Self->GetLongPackageName();
	});

	SoftPtr_.Method("FString GetAssetName() const", [](FSoftObjectPtr* Self) -> FString
	{
		return Self->GetAssetName();
	});

	SoftPtr_.Method("bool IsValid() const", [](FSoftObjectPtr* Self) -> bool
	{
		return Self->IsValid();
	});

	SoftPtr_.Method("bool IsPending() const", [](FSoftObjectPtr* Self) -> bool
	{
		return Self->IsPending();
	});

	SoftPtr_.Method("bool IsNull() const", [](FSoftObjectPtr* Self) -> bool
	{
		return Self->IsNull();
	});

	SoftPtr_.Method("void Reset()", [](FSoftObjectPtr* Self)
	{
		Self->Reset();
	});

	SoftPtr_.Method("TSoftObjectPtr<T>& opAssign(const FSoftObjectPath& Path)", [](FSoftObjectPtr* Self, FSoftObjectPath& Path) -> FSoftObjectPtr&
	{
		*Self = Path;
		return *Self;
	});
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_SoftReferences((int32)FAngelscriptBinds::EOrder::Late-5, []
{
	auto TSoftObjectPtr_ = FAngelscriptBinds::ExistingClass("TSoftObjectPtr<T>");
	BindSoftPtrBaseMethods(TSoftObjectPtr_);

	TSoftObjectPtr_.ImplicitConstructor("void f(T handle_only Object)", [](FSoftObjectPtr* Ptr, UObject* Object)
	{
		new(Ptr) FSoftObjectPtr(Object);
	});

	TSoftObjectPtr_.Constructor("void f(const TSoftObjectPtr<T>& Other)", [](FSoftObjectPtr* Ptr, FSoftObjectPtr& Other)
	{
		new(Ptr) FSoftObjectPtr(Other);
	});

	TSoftObjectPtr_.Method("TSoftObjectPtr<T>& opAssign(T handle_only Object)", [](FSoftObjectPtr* Self, UObject* Object) -> FSoftObjectPtr&
	{
		*Self = Object;
		return *Self;
	});

	TSoftObjectPtr_.Method("TSoftObjectPtr<T>& opAssign(const TSoftObjectPtr<T>& Other)", [](FSoftObjectPtr* Self, FSoftObjectPtr& Other) -> FSoftObjectPtr&
	{
		*Self = Other;
		return *Self;
	});

	TSoftObjectPtr_.Method("bool opEquals(const TSoftObjectPtr<T>& Other) const", [](FSoftObjectPtr* Self, const FSoftObjectPtr& Other) -> bool
	{
		return *Self == Other;
	});

	TSoftObjectPtr_.Method("bool opEquals(T handle_only Object) const", [](FSoftObjectPtr* Self, UObject* Object) -> bool
	{
		return Self->Get() == Object;
	});

	TSoftObjectPtr_.Method("T handle_only Get() const", [](FSoftObjectPtr* Self) -> UObject*
	{
		UObject* Object = Self->Get();
		if (Object != nullptr && !Object->IsA(GetSoftPtrSubType()))
			return nullptr;
		return Object;
	});
	SCRIPT_BIND_DOCUMENTATION("Returns the object selected at the specified path.\nIf the object is not loaded, returns nullptr.");

	TSoftObjectPtr_.Method("void LoadAsync(FOnSoftObjectLoaded OnLoaded) const", [](FSoftObjectPtr* Self, FOnSoftObjectLoaded OnLoaded)
	{
		UClass* ObjClass = GetSoftPtrSubType();
		if (ObjClass != nullptr)
		{
			// We don't allow loading references to actors or components,
			// levels are supposed to be streamed in using the streaming system not loaded manually.
			if (ObjClass->IsChildOf(AActor::StaticClass()))
			{
				FAngelscriptEngine::Throw("Actor soft references cannot be loaded, stream the level in instead.");
				return;
			}
			else if (ObjClass->IsChildOf(UActorComponent::StaticClass()))
			{
				FAngelscriptEngine::Throw("Component soft references cannot be loaded, stream the level in instead.");
				return;
			}
		}

		// Check if already loaded first
		if (UObject* Object = Self->Get())
		{
			if (!Object->IsA(ObjClass))
				Object = nullptr;
			OnLoaded.ExecuteIfBound(Object);
			return;
		}

		// Load the package the object is in
		TWeakObjectPtr<UClass> WeakClass = ObjClass;
		FSoftObjectPtr ObjectCopy(*Self);
		const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectCopy.ToString());
		if (PackageName.IsEmpty() || (FindPackage(nullptr, *PackageName) == nullptr && !FPackageName::DoesPackageExist(PackageName)))
		{
			OnLoaded.ExecuteIfBound(nullptr);
			return;
		}

		FLoadPackageAsyncDelegate Delegate;
		Delegate.BindLambda([ObjectCopy, OnLoaded, WeakClass](const FName& PkgName, UPackage* LoadedPkg, EAsyncLoadingResult::Type Result)
		{
			UObject* Object = ObjectCopy.Get();
			if (Object != nullptr)
			{
				if (!WeakClass.IsValid() || !Object->IsA(WeakClass.Get()))
					Object = nullptr;
			}
			OnLoaded.ExecuteIfBound(Object);
		});

		LoadPackageAsync(*PackageName, Delegate, 100);
	});
	SCRIPT_BIND_DOCUMENTATION("Asynchronously loads the package that contains the referenced object.\nDelegate may be called immediately if object is already loaded.");

#if WITH_EDITOR
	TSoftObjectPtr_.Method("T handle_only EditorOnlyLoadSynchronous() const", [](FSoftObjectPtr* Self) -> UObject*
	{
		UClass* ObjClass = GetSoftPtrSubType();
		if (ObjClass != nullptr)
		{
			// We don't allow loading references to actors or components,
			// levels are supposed to be streamed in using the streaming system not loaded manually.
			if (ObjClass->IsChildOf(AActor::StaticClass()))
			{
				FAngelscriptEngine::Throw("Actor soft references cannot be loaded, stream the level in instead.");
				return nullptr;
			}
			else if (ObjClass->IsChildOf(UActorComponent::StaticClass()))
			{
				FAngelscriptEngine::Throw("Component soft references cannot be loaded, stream the level in instead.");
				return nullptr;
			}
		}

		return Self->LoadSynchronous();
	});
	SCRIPT_BIND_DOCUMENTATION("Synchronously load the asset references by the soft pointer. Only available in editor, because it would cause hitches during gameplay.");
	FAngelscriptBinds::SetPreviousBindIsEditorOnly(true);
#endif

	auto TSoftClassPtr_ = FAngelscriptBinds::ExistingClass("TSoftClassPtr<T>");
	BindSoftPtrBaseMethods(TSoftClassPtr_);

	TSoftClassPtr_.Constructor("void f(UClass Object)", [](FSoftObjectPtr* Ptr, UClass* Object)
	{
		new(Ptr) FSoftObjectPtr(Object);
	});

	TSoftClassPtr_.Constructor("void f(const TSoftClassPtr<T>& Other)", [](FSoftObjectPtr* Ptr, FSoftObjectPtr& Other)
	{
		new(Ptr) FSoftObjectPtr(Other);
	});

	TSoftClassPtr_.Constructor("void f(const TSubclassOf<T>& Other)", [](FSoftObjectPtr* Ptr, TSubclassOf<UObject>& Other)
	{
		new(Ptr) FSoftObjectPtr(Other.Get());
	});

	TSoftClassPtr_.Method("TSoftClassPtr<T>& opAssign(UClass Object)", [](FSoftObjectPtr* Self, UClass* NewClass) -> FSoftObjectPtr&
	{
		if (NewClass != nullptr && !NewClass->IsChildOf(GetSoftPtrSubType()))
		{
			FAngelscriptEngine::Throw("Provided class is does not inherit from TSoftClassPtr subtype.");
			return *Self;
		}

		*Self = NewClass;
		return *Self;
	});

	TSoftClassPtr_.Method("TSoftClassPtr<T>& opAssign(const TSoftClassPtr<T>& Other)", [](FSoftObjectPtr* Self, FSoftObjectPtr& Other) -> FSoftObjectPtr&
	{
		*Self = Other;
		return *Self;
	});

	TSoftClassPtr_.Method("TSoftClassPtr<T>& opAssign(const TSubclassOf<T>& Other)", [](FSoftObjectPtr* Self, TSubclassOf<UObject>& Other) -> FSoftObjectPtr&
	{
		*Self = Other.Get();
		return *Self;
	});

	TSoftClassPtr_.Method("bool opEquals(const TSoftClassPtr<T>& Other) const", [](FSoftObjectPtr* Self, const FSoftObjectPtr& Other) -> bool
	{
		return *Self == Other;
	});

	TSoftClassPtr_.Method("bool opEquals(const TSubclassOf<T>& Other) const", [](FSoftObjectPtr* Self, const TSubclassOf<UObject>& Other) -> bool
	{
		return Self->Get() == Other.Get();
	});

	TSoftClassPtr_.Method("bool opEquals(UClass Object) const", [](FSoftObjectPtr* Self, UClass* Object) -> bool
	{
		return Self->Get() == Object;
	});

	TSoftClassPtr_.Method("TSubclassOf<T> Get() const", [](FSoftObjectPtr* Self) -> TSubclassOf<UObject>
	{
		UClass* Object = Cast<UClass>(Self->Get());
		if (Object != nullptr && !Object->IsChildOf(GetSoftPtrSubType()))
			return TSubclassOf<UObject>();
		return TSubclassOf<UObject>(Object);
	});
	SCRIPT_BIND_DOCUMENTATION("Returns the class selected at the specified path.\nIf the class is not loaded, returns nullptr.");

	TSoftClassPtr_.Method("void LoadAsync(FOnSoftClassLoaded OnLoaded) const", [](FSoftObjectPtr* Self, FOnSoftClassLoaded OnLoaded)
	{
		UClass* ObjClass = GetSoftPtrSubType();

		// Check if already loaded first
		if (UClass* Object = Cast<UClass>(Self->Get()))
		{
			if (!Object->IsChildOf(ObjClass))
				Object = nullptr;
			OnLoaded.ExecuteIfBound(Object);
			return;
		}

		// Load the package the class is in
		TWeakObjectPtr<UClass> WeakClass = ObjClass;
		FSoftObjectPtr ObjectCopy(*Self);
		const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectCopy.ToString());
		if (PackageName.IsEmpty() || (FindPackage(nullptr, *PackageName) == nullptr && !FPackageName::DoesPackageExist(PackageName)))
		{
			OnLoaded.ExecuteIfBound(nullptr);
			return;
		}

		FLoadPackageAsyncDelegate Delegate;
		Delegate.BindLambda([ObjectCopy, OnLoaded, WeakClass](const FName& PkgName, UPackage* LoadedPkg, EAsyncLoadingResult::Type Result)
		{
			UClass* Object = Cast<UClass>(ObjectCopy.Get());
			if (Object != nullptr)
			{
				if (!WeakClass.IsValid() || !Object->IsChildOf(WeakClass.Get()))
					Object = nullptr;
			}
			OnLoaded.ExecuteIfBound(Object);
		});

		LoadPackageAsync(*PackageName, Delegate, 100);
	});
	SCRIPT_BIND_DOCUMENTATION("Asynchronously loads the package that contains the referenced class.\nDelegate may be called immediately if class is already loaded.");
});
