#include "CoreMinimal.h"
#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "AngelscriptBindDatabase.h"
#include "UObject/UnrealType.h"
#include "Binds/Helper_StructType.h"

#include "Bind_TSoftObjectPtr_Functions.h"

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
	explicit FBaseSoftReferenceType(const FAngelscriptBindDatabase& InBindDatabase)
		: BindDatabase(&InBindDatabase)
	{
	}

	const FAngelscriptBindDatabase* BindDatabase;

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
	explicit FSoftObjectPtrType(const FAngelscriptBindDatabase& InBindDatabase)
		: FBaseSoftReferenceType(InBindDatabase)
	{
	}

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
			const FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(MetaClass, *BindDatabase);
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
	explicit FSoftClassPtrType(const FAngelscriptBindDatabase& InBindDatabase)
		: FBaseSoftReferenceType(InBindDatabase)
	{
	}

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
			const FString ClassHeaderPath = FAngelscriptBindDatabase::GetSourceHeader(MetaClass, *BindDatabase);
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

namespace
{
	void BindSoftReferenceTypeDeclarations(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bTemplate = true;
		Flags.TemplateType = "<T>";
		Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;

		Binds.ValueClassForTarget<FSoftObjectPtr>("TSoftObjectPtr<class T>", Flags);
		Binds.ValueClassForTarget<FSoftObjectPtr>("TSoftClassPtr<class T>", Flags);
	}

	void BindSoftReferenceTypeInfrastructure(FAngelscriptBinds& Binds)
	{
		TSharedRef<FSoftObjectPtrType> SoftObjectPtrType = MakeShared<FSoftObjectPtrType>(Binds.GetTargetBindDatabase());
		Binds.RegisterTypeForTarget(SoftObjectPtrType);

		TSharedRef<FSoftClassPtrType> SoftClassPtrType = MakeShared<FSoftClassPtrType>(Binds.GetTargetBindDatabase());
		Binds.RegisterTypeForTarget(SoftClassPtrType);

		auto TSoftObjectPtr_ = Binds.ExistingClassForTarget("TSoftObjectPtr<T>");
		TSoftObjectPtr_.TemplateCallback(
			"bool f(int&in Type, int&out ErrorMessage)",
			&FAngelscriptTSoftObjectPtrBinds::ValidateTemplate);

		auto TSoftClassPtr_ = Binds.ExistingClassForTarget("TSoftClassPtr<T>");
		TSoftClassPtr_.TemplateCallback(
			"bool f(int&in Type, int&out ErrorMessage)",
			&FAngelscriptTSoftObjectPtrBinds::ValidateTemplate);

		FAngelscriptTypeDatabase* TargetTypeDatabase = &Binds.GetTargetTypeDatabase();
		Binds.RegisterTypeFinderForTarget([SoftObjectPtrType, SoftClassPtrType, TargetTypeDatabase](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			FSoftObjectProperty* ObjectProperty = CastField<FSoftObjectProperty>(Property);
			if (ObjectProperty == nullptr)
				return false;

			// Soft object pointer could be a soft class pointer.
			FSoftClassProperty* ClassProperty = CastField<FSoftClassProperty>(Property);
			if (ClassProperty != nullptr)
			{
				const TSharedRef<FAngelscriptType>* RegisteredSubType = TargetTypeDatabase->TypesByClass.Find(ClassProperty->MetaClass);
				if (RegisteredSubType == nullptr)
					return false;

				Usage.Type = SoftClassPtrType;
				FAngelscriptTypeUsage& SubType = Usage.SubTypes.Emplace_GetRef();
				SubType.Type = RegisteredSubType->ToSharedPtr();
				return SubType.IsValid();
			}

			const TSharedRef<FAngelscriptType>* RegisteredSubType = TargetTypeDatabase->TypesByClass.Find(ObjectProperty->PropertyClass);
			if (RegisteredSubType == nullptr)
				return false;

			Usage.Type = SoftObjectPtrType;
			FAngelscriptTypeUsage& SubType = Usage.SubTypes.Emplace_GetRef();
			SubType.Type = RegisteredSubType->ToSharedPtr();
			return true;
		});
	}

	void BindSoftPtrBaseMethods(FAngelscriptBinds& SoftPtr_)
	{
		SoftPtr_.Constructor("void f()", &FAngelscriptTSoftObjectPtrBinds::ConstructDefault);
		SoftPtr_.Constructor("void f(const FSoftObjectPath& Path)", &FAngelscriptTSoftObjectPtrBinds::ConstructFromPath);
		SoftPtr_.Destructor("void f()", &FAngelscriptTSoftObjectPtrBinds::Destruct);
		SoftPtr_.Method("FSoftObjectPath ToSoftObjectPath() const", &FAngelscriptTSoftObjectPtrBinds::ToSoftObjectPath);
		SoftPtr_.Method("FString ToString() const", &FAngelscriptTSoftObjectPtrBinds::ToString);
		SoftPtr_.Method("FString GetLongPackageName() const", &FAngelscriptTSoftObjectPtrBinds::GetLongPackageName);
		SoftPtr_.Method("FString GetAssetName() const", &FAngelscriptTSoftObjectPtrBinds::GetAssetName);
		SoftPtr_.Method("bool IsValid() const", &FAngelscriptTSoftObjectPtrBinds::IsValid);
		SoftPtr_.Method("bool IsPending() const", &FAngelscriptTSoftObjectPtrBinds::IsPending);
		SoftPtr_.Method("bool IsNull() const", &FAngelscriptTSoftObjectPtrBinds::IsNull);
		SoftPtr_.Method("void Reset()", &FAngelscriptTSoftObjectPtrBinds::Reset);
		SoftPtr_.Method("TSoftObjectPtr<T>& opAssign(const FSoftObjectPath& Path)", &FAngelscriptTSoftObjectPtrBinds::AssignPath);
	}

	void BindSoftReferenceFunctions(FAngelscriptBinds& Binds)
	{
		auto TSoftObjectPtr_ = Binds.ExistingClassForTarget("TSoftObjectPtr<T>");
		BindSoftPtrBaseMethods(TSoftObjectPtr_);

		TSoftObjectPtr_.ImplicitConstructor("void f(T handle_only Object)", &FAngelscriptTSoftObjectPtrBinds::ConstructFromObject);
		TSoftObjectPtr_.Constructor("void f(const TSoftObjectPtr<T>& Other)", &FAngelscriptTSoftObjectPtrBinds::CopyConstruct);
		TSoftObjectPtr_.Method("TSoftObjectPtr<T>& opAssign(T handle_only Object)", &FAngelscriptTSoftObjectPtrBinds::AssignObject);
		TSoftObjectPtr_.Method("TSoftObjectPtr<T>& opAssign(const TSoftObjectPtr<T>& Other)", &FAngelscriptTSoftObjectPtrBinds::AssignOther);
		TSoftObjectPtr_.Method("bool opEquals(const TSoftObjectPtr<T>& Other) const", &FAngelscriptTSoftObjectPtrBinds::EqualsOther);
		TSoftObjectPtr_.Method("bool opEquals(T handle_only Object) const", &FAngelscriptTSoftObjectPtrBinds::EqualsObject);
		TSoftObjectPtr_.Method("T handle_only Get() const", &FAngelscriptTSoftObjectPtrBinds::GetObject)
			.Documentation(TEXT("Returns the object selected at the specified path.\nIf the object is not loaded, returns nullptr."));

		TSoftObjectPtr_.Method("void LoadAsync(FOnSoftObjectLoaded OnLoaded) const", &FAngelscriptTSoftObjectPtrBinds::LoadObjectAsync)
			.Documentation(TEXT("Asynchronously loads the package that contains the referenced object.\nDelegate may be called immediately if object is already loaded."));

#if WITH_EDITOR
		TSoftObjectPtr_.Method("T handle_only EditorOnlyLoadSynchronous() const", &FAngelscriptTSoftObjectPtrBinds::EditorOnlyLoadSynchronous)
			.Documentation(TEXT("Synchronously load the asset references by the soft pointer. Only available in editor, because it would cause hitches during gameplay."))
			.EditorOnly();
#endif

		auto TSoftClassPtr_ = Binds.ExistingClassForTarget("TSoftClassPtr<T>");
		BindSoftPtrBaseMethods(TSoftClassPtr_);

		TSoftClassPtr_.Constructor("void f(UClass Object)", &FAngelscriptTSoftObjectPtrBinds::ConstructFromClass);
		TSoftClassPtr_.Constructor("void f(const TSoftClassPtr<T>& Other)", &FAngelscriptTSoftObjectPtrBinds::CopyConstruct);
		TSoftClassPtr_.Constructor("void f(const TSubclassOf<T>& Other)", &FAngelscriptTSoftObjectPtrBinds::ConstructFromSubclass);
		TSoftClassPtr_.Method("TSoftClassPtr<T>& opAssign(UClass Object)", &FAngelscriptTSoftObjectPtrBinds::AssignClass);
		TSoftClassPtr_.Method("TSoftClassPtr<T>& opAssign(const TSoftClassPtr<T>& Other)", &FAngelscriptTSoftObjectPtrBinds::AssignOther);
		TSoftClassPtr_.Method("TSoftClassPtr<T>& opAssign(const TSubclassOf<T>& Other)", &FAngelscriptTSoftObjectPtrBinds::AssignSubclass);
		TSoftClassPtr_.Method("bool opEquals(const TSoftClassPtr<T>& Other) const", &FAngelscriptTSoftObjectPtrBinds::EqualsOther);
		TSoftClassPtr_.Method("bool opEquals(const TSubclassOf<T>& Other) const", &FAngelscriptTSoftObjectPtrBinds::EqualsSubclass);
		TSoftClassPtr_.Method("bool opEquals(UClass Object) const", &FAngelscriptTSoftObjectPtrBinds::EqualsClass);
		TSoftClassPtr_.Method("TSubclassOf<T> Get() const", &FAngelscriptTSoftObjectPtrBinds::GetClass)
			.Documentation(TEXT("Returns the class selected at the specified path.\nIf the class is not loaded, returns nullptr."));

		TSoftClassPtr_.Method("void LoadAsync(FOnSoftClassLoaded OnLoaded) const", &FAngelscriptTSoftObjectPtrBinds::LoadClassAsync)
			.Documentation(TEXT("Asynchronously loads the package that contains the referenced class.\nDelegate may be called immediately if class is already loaded."));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_TSoftObjectPtr_TypeDeclarations(
	TEXT("SoftReferences.Declarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindSoftReferenceTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_TSoftObjectPtr_TypeInfrastructure(
	TEXT("SoftReferences.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindSoftReferenceTypeInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_TSoftObjectPtr_Functions(
	TEXT("SoftReferences.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindSoftReferenceFunctions);
