#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

struct FAngelscriptTypeUsage;
struct FDebugValuePrototype;
struct FASDebugValue;
struct FAngelscriptTypeDatabase;

namespace UE { namespace GC {
	class FPropertyStack; class FSchemaBuilder;
	inline FName ToName(FName PropertyName) { return PropertyName; }
}}

/**
 * FAngelscriptType holds all information necessary to
 * communicate types between their UClass/FProperty variants
 * and the methods angelscript needs to know about them.
 *
 * This will be subclassed by various Binds_*.cpp files
 * in order to implement specific type operations.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptType : TSharedFromThis<FAngelscriptType>
{
	/**
	 * Type database statics
	 */

	static void* TAG_UserData_Delegate;
	static void* TAG_UserData_Multicast_Delegate;

	// Find the angelscript type that implements the given UClass.
	static TSharedPtr<FAngelscriptType> GetByClass(UClass* ForClass);
	static TSharedPtr<FAngelscriptType> GetByClass(FAngelscriptTypeDatabase& Database, UClass* ForClass);

	// Find the angelscript type that specified the given data.
	static TSharedPtr<FAngelscriptType> GetByData(void* ForData);
	static TSharedPtr<FAngelscriptType> GetByData(FAngelscriptTypeDatabase& Database, void* ForData);

	// Find the angelscript type known to angelscript by the given name.
	static TSharedPtr<FAngelscriptType> GetByAngelscriptTypeName(const FString& Name);
	static TSharedPtr<FAngelscriptType> GetByAngelscriptTypeName(FAngelscriptTypeDatabase& Database, const FString& Name);

	// Find the angelscript type that implements the given Property.
	//  This does not take into account any qualifiers or wrappers,
	//  use FAngelscriptTypeUsage::FromProperty() to get the fully qualified type.
	static TSharedPtr<FAngelscriptType> GetByProperty(FProperty* Property, bool bQueryTypeFinders = true);
	static TSharedPtr<FAngelscriptType> GetByProperty(FAngelscriptTypeDatabase& Database, FProperty* Property, bool bQueryTypeFinders = true);

	static TSharedPtr<FAngelscriptType>& GetScriptObject();
	static void SetScriptObject(TSharedPtr<FAngelscriptType> Type);

	static TSharedPtr<FAngelscriptType>& GetScriptEnum();
	static void SetScriptEnum(TSharedPtr<FAngelscriptType> Type);

	static TSharedPtr<FAngelscriptType>& GetScriptStruct();
	static void SetScriptStruct(TSharedPtr<FAngelscriptType> Type);

	static TSharedPtr<FAngelscriptType>& GetScriptDelegate();
	static void SetScriptDelegate(TSharedPtr<FAngelscriptType> Type);

	static TSharedPtr<FAngelscriptType>& GetScriptMulticastDelegate();
	static void SetScriptMulticastDelegate(TSharedPtr<FAngelscriptType> Type);

	static class asITypeInfo* GetArrayTemplateTypeInfo();
	static void SetArrayTemplateTypeInfo(class asITypeInfo* TypeInfo);

	static TSharedPtr<FAngelscriptType>& ScriptFloatType();
	static TSharedPtr<FAngelscriptType>& ScriptDoubleType();
	static TSharedPtr<FAngelscriptType>& ScriptFloatParamExtendedToDoubleType();
	static TSharedPtr<FAngelscriptType>& ScriptBoolType();

	static const TArray<TSharedRef<FAngelscriptType>>& GetTypes();

	// Register a new type to be usable by the system
	static void Register(TSharedRef<FAngelscriptType> Type);
	static void Register(FAngelscriptTypeDatabase& Database, TSharedRef<FAngelscriptType> Type);

	static void ResetTypeDatabase();

	// Register an alias for a specific type
	static void RegisterAlias(const FString& Alias, TSharedRef<FAngelscriptType> Type);
	static void RegisterAlias(FAngelscriptTypeDatabase& Database, const FString& Alias, TSharedRef<FAngelscriptType> Type);

	// Register a 'type finder' function that can find FAngelscriptTypes for UProperties.
	typedef TFunction<bool(FProperty*,FAngelscriptTypeUsage&)> FTypeFinder;
	static void RegisterTypeFinder(FTypeFinder Finder);
	static void RegisterTypeFinder(FAngelscriptTypeDatabase& Database, FTypeFinder Finder);

	/**
	 * Helper statics
	 */

	// Helper to get the fully qualified name including the U/A prefix.
	static FString GetBoundClassName(UClass* Class);

	// Helper to build an angelscript function declaration
	static FString BuildFunctionDeclaration(const FAngelscriptTypeUsage& ReturnType, const FString& FunctionName, const TArray<FAngelscriptTypeUsage>& ArgumentTypes, const TArray<FString>& ArgumentNames, const TArray<FString>& ArgumentDefaults, bool bConstMethod = false);

	// Helper to find the script-side Hash() declaration used by script structs.
	static class asIScriptFunction* FindScriptStructHashFunction(class asITypeInfo* ScriptType);

	/**
	 * Type operations
	 */
	// Get the base name of the type as known to angelscript, used
	// to construct angelscript function declarations.
	virtual FString GetAngelscriptTypeName() const
	{
		return TEXT("");
	}

	virtual FString GetAngelscriptTypeName(const FAngelscriptTypeUsage& Usage) const
	{
		return GetAngelscriptTypeName();
	}

	// Some types want to specify their angelscript declaration differently depending on where it will be used
	enum EAngelscriptDeclarationMode
	{
		Generic,
		MemberVariable,
		PreResolvedObject,
		FunctionReturnValue,
		FunctionArgument,
		MemberVariable_InContainer,
	};

	// Get the declaration for this type based on a type usage
	virtual FString GetAngelscriptDeclaration(const FAngelscriptTypeUsage& Usage, EAngelscriptDeclarationMode Mode = EAngelscriptDeclarationMode::Generic) const;

	// Get the UClass that this angelscript type implements as a reference.
	// Not all FAngelscriptTypes will be associated with a UClass (eg primitives).
	virtual UClass* GetClass(const FAngelscriptTypeUsage& Usage) const { return nullptr; }

	// Get the TypeInfo associated with this type. Not all AS types will have a typeinfo.
	virtual class asITypeInfo* GetAngelscriptTypeInfo(const FAngelscriptTypeUsage& Usage) const { return nullptr; }

	// get the UStruct that this angelscript type implements as a direct member.
	// Not all FAngelscriptTypes will be associated with a UStruct.
	virtual UStruct* GetUnrealStruct(const FAngelscriptTypeUsage& Usage) const { return nullptr; }

	// Get a data pointer that this type is associated with.
	virtual void* GetData() const { return nullptr; }

	// Whether this type can be queried for a FProperty implementation.
	virtual bool CanQueryPropertyType() const { return true; }

	enum EPropertyMatchType
	{
		TypeFinder,
		OverrideArgument,
		OverrideReturnValue,
		InContainer,
	};

	// Whether this type implements the specified FProperty
	virtual bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const { return false; }

	// Whether this supports created a FProperty for the type
	virtual bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Create a new FProperty that contains this type
	struct FPropertyParams
	{
		UStruct* Struct = nullptr;
		FFieldVariant Outer;
		FName PropertyName;
	};

	virtual FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const { ensure(false);  return nullptr; }

	// Whether params of this type in UFUNCTIONS are forced outparam refs
	virtual bool IsParamForcedOutParam() const { return false; }

	// Whether we need to emit any reference info
	virtual bool HasReferences(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Add reference to be collected for this type into the class
	struct FGCReferenceParams
	{
		TArray<FName, TInlineAllocator<2>> Names;
		class UASClass* Class = nullptr;
		SIZE_T AtOffset;
		UE::GC::FSchemaBuilder* Schema;
		UE::GC::FPropertyStack* DebugPath;
	};

	virtual void EmitReferenceInfo(const FAngelscriptTypeUsage& Usage, FGCReferenceParams& Params) const {}

	// Whether this property needs to be known by the garbage collector
	virtual bool NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Whether this must always be created as a FProperty in a class, instead of allowing it to be angelscript-only
	virtual bool RequiresProperty(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Whether this property represents a trivial UObject pointer.
	virtual bool IsObjectPointer() const { return false; }

	// Whether this property represents an unresolved UObject pointer.
	virtual bool IsUnresolvedObjectPointer() const { return false; }

	// Whether this object type can be a template subtype
	virtual bool CanBeTemplateSubType() const { return true; }
	
	// Whether this property represents a struct that is known to unreal
	virtual bool IsUnrealStruct() const { return false; }

	// Wether this property represents a fully primitive value
	virtual bool IsPrimitive() const { return false; }

	// Whether we can copy values of this type from the engine level
	virtual bool CanCopy(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Whether values of this type need to have CopyValue called or if they can be straight POD-copied
	virtual bool NeedCopy(const FAngelscriptTypeUsage& Usage) const { return true; }

	// Copy a value of this type from one place in memory to another
	virtual void CopyValue(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const { ensure(false); }

	// Whether we can compare values of this type from the engine level
	virtual bool CanCompare(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Compare whether the values at the source and destination are equal
	virtual bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const { ensure(false);  return false; }

	// Whether we can construct new values of this type into memory
	virtual bool CanConstruct(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Whether values of this type need to be constructed
	virtual bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const { return true; }

	// Copy a value of this type from one place in memory to another
	virtual void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const {}

	// Get the size of the value that we can construct
	virtual int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const { ensure(false); return 1; }

	// Whether we can construct new values of this type into memory
	virtual bool CanDestruct(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Whether values of this type need to be destructed
	virtual bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const { return true; }

	// Copy a value of this type from one place in memory to another
	virtual void DestructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const {}

	// Whether a value of this type can be used as a context argument
	virtual bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const { return false; }

	struct FArgData
	{
		void* StackPtr;
	};

	// Read a value from the unreal stack and push it to the angelscript context as an argument
	virtual void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const { ensure(false); }

	// Whether a value of this type can be returned from a script function
	virtual bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Read a value from the unreal stack and push it to the angelscript context as an argument
	virtual void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const { ensure(false); }

	// Customized binding of properties to classes instead of standard by-value semantics
	struct FBindParams
	{
		struct FAngelscriptBinds* BindClass = nullptr;
		FString NameOverride;
		bool bCanRead = true;
		bool bCanWrite = true;
		bool bCanEdit = true;
		bool bProtected = false;
	};
	virtual bool BindProperty(const FAngelscriptTypeUsage& Usage, const FBindParams& Params, FProperty* NativeProperty) const { return false; }

	// Makes an angelscript default value from an unreal default value string
	virtual bool DefaultValue_UnrealToAngelscript(const FAngelscriptTypeUsage& Usage, const FString& UnrealValue, FString& OutAngelscriptValue) const { return false; }

	// Makes an unreal default value string from an angelscript default value
	virtual bool DefaultValue_AngelscriptToUnreal(const FAngelscriptTypeUsage& Usage, const FString& AngelscriptValue, FString& OutUnrealValue) const { return false; }

	// If provided, arguments of this type in bound functions will provide this default value if unreal doesn't specify any
	virtual bool DefaultValue_AngelscriptFallback(const FAngelscriptTypeUsage& Usage, FString& OutAngelscriptValue) const { return false; }

	// Whether we can create a hash of the value
	virtual bool CanHashValue(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Get the hash of the value at the passed address
	virtual uint32 GetHash(const FAngelscriptTypeUsage& Usage, const void* Address) const { ensure(false); return 0; }

	// Get the alignment of the value that we can construct
	virtual int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const { return 1; }

	// Create debug values for this property
	virtual FASDebugValue* CreateDebugValue(const FAngelscriptTypeUsage& Usage, FDebugValuePrototype& Values, int32 Offset) const { return nullptr; }

	// Get the type id for the native type that can be used to reify this script type
	virtual int32 GetReifyType(const FAngelscriptTypeUsage& Usage) const { return 0; }

	// Get the debugger value for a variable of this type
	virtual bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const { return false; }

	// Get the debugger value for a variable of this type
	virtual bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value, FProperty* NativeProperty) const
	{
		return GetDebuggerValue(Usage, Address, Value);
	}

	// Get the debugger scope containing all members for a variable of this type
	virtual bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const { return false; }

	// Find the debugger value for a particular member of this scope
	virtual bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const { return false; }

	// Convert to a identifier string, mainly used for debugging purposes
	virtual bool GetStringIdentifier(const FAngelscriptTypeUsage& Usage, void* Address, FString& OutString) const { return false; }

	// Convert from an identifier string, mainly used for debugging purposes
	virtual bool FromStringIdentifier(const FAngelscriptTypeUsage& Usage, const FString& InString, void* BufferPtr) const { return false; }

	// Whether the passed in type usage describes a type that is usable (can be false for template types without subtypes filled in)
	virtual bool DescribesCompleteType(const FAngelscriptTypeUsage& Usage) const { return true; }

	// Helper function for retrieving return values from simple methods
	static bool GetDebuggerValueFromFunction(class asIScriptFunction* ScriptFunction, void* Object, struct FDebuggerValue& OutValue, class asITypeInfo* ContainerScriptType = nullptr, UStruct* ContainerClass = nullptr, const FString & PropertyAddrToSearchFor = "");

	struct FCppForm
	{
		FString CppType;
		FString CppHeader;
		FString CppGenericType;
		FString OverrideFuncCastType;
		FString TemplateObjectForm;
		bool bIsPrimitive = false;
		bool bDisallowNativeNest = false;
		bool bNativeCannotBeGeneric = false;
	};

	// Lookup of C++ types for static jit purposes
	virtual bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const { return false; }

	// Whether we can compare the values for ordering
	virtual bool IsOrdered(const FAngelscriptTypeUsage& Usage) const { return false; }

	// Order compare for two values
	virtual int32 CompareOrder(const FAngelscriptTypeUsage& Usage, void* Value, void* OtherValue) const { return 0; }

	// Check whether the given type usage is equivalent to ours or not. Already guaranteed to be the same base type.
	virtual bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const { return true; }

	virtual ~FAngelscriptType() {}
};

/**
 * Describes a particular usage of an angelscript type,
 * including all qualifiers, wrappers, and other.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypeUsage
{
	TArray<FAngelscriptTypeUsage> SubTypes;
	TSharedPtr<FAngelscriptType> Type;
	bool bIsReference = false;
	bool bIsConst = false;

	union
	{
		class asITypeInfo* ScriptClass;
		class FProperty* UnrealProperty;
		int32 TypeIndex;
	};

	FAngelscriptTypeUsage()
	{
		ScriptClass = nullptr;
	}

	FAngelscriptTypeUsage(TSharedPtr<FAngelscriptType> InType)
		: Type(InType)
	{
	}

	static FAngelscriptTypeUsage DefaultUsage;

	bool IsValid() const
	{
		return Type.IsValid();
	}
	
	void Reset()
	{
		Type = nullptr;
		bIsReference = false;
		bIsConst = false;
		ScriptClass = nullptr;
		SubTypes.Empty();
	}

	bool operator==(const FAngelscriptTypeUsage& Other) const;

	bool operator!=(const FAngelscriptTypeUsage& Other) const
	{
		return !(*this == Other);
	}

	// Match a type usage to a FProperty
	static FAngelscriptTypeUsage FromProperty(FProperty* Property);
	static FAngelscriptTypeUsage FromProperty(FAngelscriptTypeDatabase& Database, FProperty* Property);

	// Match a type usage to an angelscript Property
	static FAngelscriptTypeUsage FromProperty(class asITypeInfo* ScriptType, int32 PropertyIndex);

	// Match a type usage to an angelscript function return type
	static FAngelscriptTypeUsage FromReturn(class asIScriptFunction* Function);

	// Match a type usage to an angelscript function parameter
	static FAngelscriptTypeUsage FromParam(class asIScriptFunction* Function, int32 ParamIndex);

	// Match a type usage to an angelscript TypeId
	static FAngelscriptTypeUsage FromTypeId(int32 TypeId);

	// Match a type usage to an angelscript DataType
	static FAngelscriptTypeUsage FromDataType(const class asCDataType& DataType);

	// Match a type usage to a FProperty
	static FAngelscriptTypeUsage FromClass(UClass* Class);

	// Match a type usage to a FProperty
	static FAngelscriptTypeUsage FromStruct(UScriptStruct* Struct);

	// Get the actual string we should pass to angelscript for this type
	FString GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode Mode = FAngelscriptType::EAngelscriptDeclarationMode::Generic) const;

	// Get the actual UClass* that corresponds to this. Will be nullptr if invalid or not an object handle.
	UClass* GetClass() const;

	// Checks if the unqualified type (doesn't compare const/reference/handle) is the same as 'Other'.
	bool EqualsUnqualified(const FAngelscriptTypeUsage& Other) const;

	FString GetFriendlyTypeName() const;

	// get the UStruct that this angelscript type implements as a direct member.
	// Not all FAngelscriptTypes will be associated with a UStruct.
	FORCEINLINE UStruct* GetUnrealStruct() const { return Type.IsValid() ? Type->GetUnrealStruct(*this) : nullptr; }

	// Whether this supports created a FProperty for the type
	FORCEINLINE bool CanCreateProperty() const { return Type.IsValid() && Type->CanCreateProperty(*this); }

	// Create a new FProperty that contains this type
	FORCEINLINE FProperty* CreateProperty(const FAngelscriptType::FPropertyParams& Params) const { return Type->CreateProperty(*this, Params); }

	// Whether this type implements the specified FProperty
	FORCEINLINE bool MatchesProperty(const FProperty* Property, FAngelscriptType::EPropertyMatchType MatchType) const { return Type.IsValid() && Type->MatchesProperty(*this, Property, MatchType); }

	// Whether we need to emit any reference info
	FORCEINLINE bool HasReferences() const { return Type.IsValid() && Type->HasReferences(*this); }

	// Add reference to be collected for this type into the class
	FORCEINLINE void EmitReferenceInfo(FAngelscriptType::FGCReferenceParams& Params) const { Type->EmitReferenceInfo(*this, Params); }

	// Whether this property needs to be known by the garbage collector
	FORCEINLINE bool NeverRequiresGC() const { return Type.IsValid() && Type->NeverRequiresGC(*this); }

	// Whether this must always be created as a FProperty in a class, instead of allowing it to be angelscript-only
	FORCEINLINE bool RequiresProperty() const { return Type.IsValid() && Type->RequiresProperty(*this); }

	// Whether we can copy values of this type from the engine level
	FORCEINLINE bool CanCopy() const { return Type.IsValid() && Type->CanCopy(*this); }

	// Whether values of this type need to have CopyValue called or if they can be straight POD-copied
	FORCEINLINE bool NeedCopy() const { return Type->NeedCopy(*this); }

	// Copy a value of this type from one place in memory to another
	FORCEINLINE void CopyValue(void* SourcePtr, void* DestinationPtr) const { Type->CopyValue(*this, SourcePtr, DestinationPtr); }

	// Whether we can compare values of this type from the engine level
	FORCEINLINE bool CanCompare() const { return Type.IsValid() && Type->CanCompare(*this); }

	// Compare whether the values at the source and destination are equal
	FORCEINLINE bool IsValueEqual(void* SourcePtr, void* DestinationPtr) const { return Type->IsValueEqual(*this, SourcePtr, DestinationPtr); }

	// Whether we can construct new values of this type into memory
	FORCEINLINE bool CanConstruct() const { return Type.IsValid() && Type->CanConstruct(*this); }

	// Whether values of this type need to be constructed
	FORCEINLINE bool NeedConstruct() const { return Type->NeedConstruct(*this); }

	// Copy a value of this type from one place in memory to another
	FORCEINLINE void ConstructValue(void* DestinationPtr) const { Type->ConstructValue(*this, DestinationPtr); }

	// Whether we can construct new values of this type into memory
	FORCEINLINE bool CanDestruct() const { return Type.IsValid() && Type->CanDestruct(*this); }

	// Whether values of this type need to be destructed
	FORCEINLINE bool NeedDestruct() const { return Type->NeedDestruct(*this); }

	// Copy a value of this type from one place in memory to another
	FORCEINLINE void DestructValue(void* DestinationPtr) const { Type->DestructValue(*this, DestinationPtr); }

	// Get the size of the value contained in the type
	FORCEINLINE int32 GetValueSize() const { return Type->GetValueSize(*this);  }

	// Whether a value of this type can be used as a context argument
	FORCEINLINE bool CanBeArgument() const { return Type.IsValid() && Type->CanBeArgument(*this); }

	// Read a value from the unreal stack and push it to the angelscript context as an argument
	FORCEINLINE void SetArgument(int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FAngelscriptType::FArgData& Data) const { Type->SetArgument(*this, ArgumentIndex, Context, Stack, Data); }

	// Whether a value of this type can be returned from a script function
	FORCEINLINE bool CanBeReturned() const { return Type.IsValid() && Type->CanBeReturned(*this); }

	// Read a value from the unreal stack and push it to the angelscript context as an argument
	FORCEINLINE void GetReturnValue(class asIScriptContext* Context, void* Destination) const { Type->GetReturnValue(*this, Context, Destination); }

	// Makes an angelscript default value from an unreal default value string
	FORCEINLINE bool DefaultValue_UnrealToAngelscript(const FString& UnrealValue, FString& OutAngelscriptValue) const
	{
		if (!Type.IsValid())
			return false;
		return Type->DefaultValue_UnrealToAngelscript(*this, UnrealValue, OutAngelscriptValue);
	}

	// Makes an unreal default value string from an angelscript default value
	FORCEINLINE bool DefaultValue_AngelscriptToUnreal(const FString& AngelscriptValue, FString& OutUnrealValue) const
	{
		if (!Type.IsValid())
			return false;
		return Type->DefaultValue_AngelscriptToUnreal(*this, AngelscriptValue, OutUnrealValue);
	}

	// If provided, arguments of this type in bound functions will provide this default value if unreal doesn't specify any
	FORCEINLINE bool DefaultValue_AngelscriptFallback(FString& OutAngelscriptValue) const
	{
		if (!Type.IsValid())
			return false;
		return Type->DefaultValue_AngelscriptFallback(*this, OutAngelscriptValue);
	}

	// Whether we can create a hash of the value
	FORCEINLINE bool CanHashValue() const { return Type.IsValid() && Type->CanHashValue(*this); }

	// Get the hash of the value at the passed address
	FORCEINLINE uint32 GetHash(const void* Address) const { return Type->GetHash(*this, Address); }

	// Get the alignment of the value that we can construct
	FORCEINLINE int32 GetValueAlignment() const { return Type->GetValueAlignment(*this); }

	// Whether this property represents a trivial UObject pointer.
	FORCEINLINE bool IsObjectPointer() const { return Type.IsValid() && Type->IsObjectPointer(); }

	// Whether this property represents a potentially unresolved UObject pointer.
	FORCEINLINE bool IsUnresolvedObjectPointer() const { return Type.IsValid() && Type->IsUnresolvedObjectPointer(); }

	// Whether this object type can be a template subtype
	FORCEINLINE bool CanBeTemplateSubType() const { return Type.IsValid() && Type->CanBeTemplateSubType(); }

	// Wether this property represents a fully primitive value
	FORCEINLINE bool IsPrimitive() const { return Type.IsValid() && Type->IsPrimitive(); }

	// Create debug values for this property
	FORCEINLINE FASDebugValue* CreateDebugValue(FDebugValuePrototype& Values, int32 Offset) const { if (Type.IsValid()) return Type->CreateDebugValue(*this, Values, Offset); else return nullptr; }

	// Get the type id for the native type that can be used to reify this script type
	FORCEINLINE int32 GetReifyType() const { return Type.IsValid() ? Type->GetReifyType(*this) : 0; }

	// Get the debugger value for a variable of this type
	FORCEINLINE bool GetDebuggerValue(void* Address, struct FDebuggerValue& Value) const { return Type.IsValid() && Type->GetDebuggerValue(*this, Address, Value); }
	FORCEINLINE bool GetDebuggerValue(void* Address, struct FDebuggerValue& Value, FProperty* NativeProperty) const { return Type.IsValid() && Type->GetDebuggerValue(*this, Address, Value, NativeProperty); }

	// Get the debugger scope containing all members for a variable of this type
	FORCEINLINE bool GetDebuggerScope(void* Address, struct FDebuggerScope& Scope) const { return Type.IsValid() && Type->GetDebuggerScope(*this, Address, Scope); }

	// Find the debugger value for a particular member of this scope
	FORCEINLINE bool GetDebuggerMember(void* Address, const FString& Member, struct FDebuggerValue& Value) const { return Type.IsValid() && Type->GetDebuggerMember(*this, Address, Member, Value); }

	// Convert to a identifier string, mainly used for debugging purposes
	FORCEINLINE bool GetStringIdentifier(void* Address, FString& OutString) const { return Type.IsValid() && Type->GetStringIdentifier(*this, Address, OutString); }

	// Convert from an identifier string, mainly used for debugging purposes
	FORCEINLINE bool FromStringIdentifier(const FString& InString, void* BufferPtr) const { return Type.IsValid() && Type->FromStringIdentifier(*this, InString, BufferPtr); }

	// Lookup of C++ types for static jit purposes
	FORCEINLINE bool GetCppForm(FAngelscriptType::FCppForm& OutCppForm) const { return Type.IsValid() && Type->GetCppForm(*this, OutCppForm); }

	// Whether we can compare the values for ordering
	FORCEINLINE bool IsOrdered() const { return Type.IsValid() && Type->IsOrdered(*this); }

	// Order compare for two values
	FORCEINLINE int32 CompareOrder(void* Value, void* OtherValue) const { return Type->CompareOrder(*this, Value, OtherValue); }

	// Resolve the location of the primitive at the address
	template<typename T>
	T& ResolvePrimitive(void* Address) const
	{
		if (bIsReference)
			Address = *(void**)Address;
		return *(T*)Address;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptTypeDatabase
{
	TArray<TSharedRef<FAngelscriptType>> RegisteredTypes;
	TMap<FString, TSharedRef<FAngelscriptType>> TypesByAngelscriptName;
	TMap<UClass*, TSharedRef<FAngelscriptType>> TypesByClass;
	TMap<void*, TSharedRef<FAngelscriptType>> TypesByData;

	TArray<FAngelscriptType::FTypeFinder> TypeFinders;
	TArray<TSharedRef<FAngelscriptType>> TypesImplementingProperties;
	TSharedPtr<FAngelscriptType> ScriptObjectType;
	TSharedPtr<FAngelscriptType> ScriptEnumType;
	TSharedPtr<FAngelscriptType> ScriptStructType;
	TSharedPtr<FAngelscriptType> ScriptDelegateType;
	TSharedPtr<FAngelscriptType> ScriptMulticastDelegateType;
	TSharedPtr<FAngelscriptType> ScriptFloatType;
	TSharedPtr<FAngelscriptType> ScriptDoubleType;
	TSharedPtr<FAngelscriptType> ScriptFloatParamExtendedToDoubleType;
	TSharedPtr<FAngelscriptType> ScriptBoolType;
	class asITypeInfo* ArrayTemplateTypeInfo = nullptr;
};

struct FDebuggerValue
{
	FString Name;
	FString Type;
	FString Value;
	FAngelscriptTypeUsage Usage;
	void* Address = nullptr;
	bool bHasMembers = false;
	
	TArray<uint8> LiteralValue;
	FAngelscriptTypeUsage LiteralType;

	// If this value, or if the object this value is on is temporary
	bool bTemporaryValue = false;

	// For temporary values, this will point to the non-temporary address of the value (if found)
	void* NonTemporaryAddress = nullptr;
	// If set, monitor this address instead of the address given by GetNonTemporaryAddress()
	void* AddressToMonitor = nullptr;
	int AddressToMonitorValueSize = 0;
	

	FDebuggerValue()
	{
	}

	~FDebuggerValue()
	{
		ClearLiteral();
	}

	FDebuggerValue(FDebuggerValue&& Other)
	{
		*this = (FDebuggerValue&&)(Other);
	}

	FDebuggerValue& operator=(const FDebuggerValue& Other) = delete;

	FDebuggerValue& operator=(FDebuggerValue&& Other)
	{
		Name = MoveTemp(Other.Name);
		Type = MoveTemp(Other.Type);
		Value = MoveTemp(Other.Value);
		Usage = Other.Usage;
		Address = Other.Address;
		bHasMembers = Other.bHasMembers;
		bTemporaryValue = Other.bTemporaryValue;
		NonTemporaryAddress = Other.NonTemporaryAddress;
		AddressToMonitor = Other.AddressToMonitor;
		AddressToMonitorValueSize = Other.AddressToMonitorValueSize;

		LiteralType = Other.LiteralType;

		auto* OldData = Other.LiteralValue.GetData();
		LiteralValue = MoveTemp(Other.LiteralValue);
		ensureAlways(OldData == LiteralValue.GetData());

		Other.LiteralValue.Empty();
		Other.LiteralType.Reset();

		return *this;
	}

	void* GetLiteral() const
	{
		return (void*)Align(LiteralValue.GetData(), 16);
	}

	template<typename T>
	T& AllocatePODLiteral()
	{
		ClearLiteral();
		LiteralValue.SetNumUninitialized(sizeof(T) + 32);
		return *(T*)GetLiteral();
	}

	void* AllocateLiteral(const FAngelscriptTypeUsage& InType)
	{
		ensureAlways(!LiteralType.IsValid());
		ensureAlways(LiteralValue.Num() == 0);

		ClearLiteral();
		LiteralType = InType;
		LiteralValue.SetNumUninitialized(InType.GetValueSize() + 32);

		if (InType.NeedConstruct())
			InType.ConstructValue(GetLiteral());

		return GetLiteral();
	}

	void ClearLiteral()
	{
		if (LiteralType.IsValid() && LiteralType.NeedDestruct())
			LiteralType.DestructValue(GetLiteral());

		LiteralValue.Empty();
		LiteralType.Reset();
	}

	void SetAddressToMonitor(void* InAddressToMonitor, int InAddressValueSize)
	{
		AddressToMonitor = InAddressToMonitor;
		AddressToMonitorValueSize = InAddressValueSize;
	}

	void* GetNonTemporaryAddress() const
	{
		if (bTemporaryValue)
		{
			return NonTemporaryAddress;
		}

		return Address;
	}

	void* GetAddressToMonitor() const
	{
		if (AddressToMonitor != nullptr)
		{
			return AddressToMonitor;
		}

		return GetNonTemporaryAddress();
	}

	int GetAddressToMonitorValueSize() const
	{
		if (AddressToMonitor != nullptr)
		{
			return AddressToMonitorValueSize;
		}

		return Usage.IsValid() ? Usage.GetValueSize() : 0;
	}
};

struct FDebuggerScope
{
	TArray<FDebuggerValue> Values;
};
