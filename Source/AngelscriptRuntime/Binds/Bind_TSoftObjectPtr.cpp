#include "Bind_TSoftObjectPtr.h"

#include "CoreMinimal.h"
#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "AngelscriptBindDatabase.h"
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

/**
 * Soft object and class reference binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftObjectPtr<T> ObjectRef;                                                                                                 | Declares a covariant soft reference to an object of type T.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftClassPtr<T> ClassRef;                                                                                                   | Declares a covariant soft reference to a class derived from T.                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftObjectPtr<T> Value();                                                                                                   | Constructs a null soft object reference.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftObjectPtr<T> Value(const FSoftObjectPath& Path);                                                                        | Constructs a soft object reference from an unloaded asset path.                                                      |
 * |                                                                                                                              | @param Path Asset path retained without forcing the object to load.                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftObjectPtr<T> Value(T Object);                                                                                           | Implicitly constructs a soft reference from a loaded object.                                                         |
 * |                                                                                                                              | @param Object Loaded object used to establish the reference.                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftObjectPtr<T> Value(const TSoftObjectPtr<T>& Other);                                                                     | Copies another soft object reference.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSoftObjectPath TSoftObjectPtr<T>.ToSoftObjectPath() const;                                                                  | Returns the reference as a soft object path.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString TSoftObjectPtr<T>.ToString() const;                                                                                  | Returns the reference's asset path string.                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString TSoftObjectPtr<T>.GetLongPackageName() const;                                                                        | Returns the long package name portion of the asset path.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString TSoftObjectPtr<T>.GetAssetName() const;                                                                              | Returns the asset name portion of the path.                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSoftObjectPtr<T>.IsValid() const;                                                                                      | Reports whether the referenced object is currently loaded and valid.                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSoftObjectPtr<T>.IsPending() const;                                                                                    | Reports whether a non-null reference exists but its object is not loaded.                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSoftObjectPtr<T>.IsNull() const;                                                                                       | Reports whether the reference contains no asset path or object.                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSoftObjectPtr<T>.Reset();                                                                                              | Clears the stored path and object reference.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ObjectRef = Path;                                                                                                            | Replaces the soft object reference with Path.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ObjectRef = Object;                                                                                                          | Replaces the soft reference with a loaded Object of type T.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ObjectRef = Other;                                                                                                           | Copies another soft object reference.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = ObjectRef == Other;                                                                                            | Compares two soft object references by their referenced identity.                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = ObjectRef == Object;                                                                                           | Reports whether Object is the currently referenced loaded object.                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | T TSoftObjectPtr<T>.Get() const;                                                                                             | Returns the loaded object, or null without loading it.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSoftObjectPtr<T>.LoadAsync(FOnSoftObjectLoaded OnLoaded) const;                                                        | Requests asynchronous asset loading; an already-loaded asset may invoke the delegate immediately.                    |
 * |                                                                                                                              | @param OnLoaded Called with the resolved object when loading completes.                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | T TSoftObjectPtr<T>.EditorOnlyLoadSynchronous() const;                                                                       | Synchronously loads and returns the object in editor builds; unavailable for runtime gameplay.                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftClassPtr<T> Value();                                                                                                    | Constructs a null soft class reference.                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftClassPtr<T> Value(const FSoftObjectPath& Path);                                                                         | Constructs a soft class reference from an unloaded class path.                                                       |
 * |                                                                                                                              | @param Path Class asset path retained without forcing the class to load.                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftClassPtr<T> Value(UClass Object);                                                                                       | Constructs a soft class reference from a loaded UClass.                                                              |
 * |                                                                                                                              | @param Object Loaded class constrained to derive from T.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftClassPtr<T> Value(const TSoftClassPtr<T>& Other);                                                                       | Copies another soft class reference.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSoftClassPtr<T> Value(const TSubclassOf<T>& Other);                                                                         | Initializes the soft class reference from a typed class value.                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSoftObjectPath TSoftClassPtr<T>.ToSoftObjectPath() const;                                                                   | Returns the class reference as a soft object path.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString TSoftClassPtr<T>.ToString() const;                                                                                   | Returns the class reference's asset path string.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString TSoftClassPtr<T>.GetLongPackageName() const;                                                                         | Returns the long package name portion of the class asset path.                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString TSoftClassPtr<T>.GetAssetName() const;                                                                               | Returns the class asset name portion of the path.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSoftClassPtr<T>.IsValid() const;                                                                                       | Reports whether the referenced class is currently loaded and valid.                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSoftClassPtr<T>.IsPending() const;                                                                                     | Reports whether a non-null class reference exists but is not loaded.                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool TSoftClassPtr<T>.IsNull() const;                                                                                        | Reports whether the class reference contains no path or class.                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSoftClassPtr<T>.Reset();                                                                                               | Clears the stored class path and reference.                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ClassRef = Path;                                                                                                             | Replaces the soft class reference with Path.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ClassRef = Object;                                                                                                           | Replaces the soft class reference with a loaded UClass derived from T.                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ClassRef = OtherClassRef;                                                                                                    | Copies another soft class reference.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ClassRef = Subclass;                                                                                                         | Copies a TSubclassOf<T> class value.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = ClassRef == OtherClassRef;                                                                                     | Compares against another soft class reference by class identity.                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = ClassRef == Subclass;                                                                                          | Compares against a TSubclassOf<T> value by class identity.                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = ClassRef == Object;                                                                                            | Reports whether Object is the currently referenced loaded class.                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TSubclassOf<T> TSoftClassPtr<T>.Get() const;                                                                                 | Returns the loaded class as TSubclassOf<T>, or null without loading it.                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void TSoftClassPtr<T>.LoadAsync(FOnSoftClassLoaded OnLoaded) const;                                                          | Requests asynchronous class loading; an already-loaded class may invoke the delegate immediately.                    |
 * |                                                                                                                              | @param OnLoaded Called with the resolved class when loading completes.                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

namespace
{


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

}

AS_FORCE_LINK const FAngelscriptBind Bind_TSoftObjectPtr_TypeDeclarations(
	TEXT("SoftReferences.Declarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bTemplate = true;
		Flags.TemplateType = "<T>";
		Flags.ExtraFlags = asOBJ_TEMPLATE_SUBTYPE_COVARIANT;

		Binds.ValueClassForTarget<FSoftObjectPtr>("TSoftObjectPtr<class T>", Flags);
		Binds.ValueClassForTarget<FSoftObjectPtr>("TSoftClassPtr<class T>", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_TSoftObjectPtr_TypeInfrastructure(
	TEXT("SoftReferences.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
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
	});

AS_FORCE_LINK const FAngelscriptBind Bind_TSoftObjectPtr_Functions(
	TEXT("SoftReferences.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
