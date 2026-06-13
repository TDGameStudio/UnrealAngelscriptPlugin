#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "AngelscriptDelegateWithPayload.generated.h"

/** Internal delegate signature used for binding validation (no parameters). */
DECLARE_DYNAMIC_DELEGATE(FInternalEmptyDelegate);

/** Internal delegate signature used for binding validation (with payload parameter). */
DECLARE_DYNAMIC_DELEGATE_OneParam(FInternalEmptyDelegateWithPayload, int, Payload);

/**
 * A delegate wrapper that supports binding a UFunction with an optional payload.
 *
 * This struct allows Angelscript to bind a UFunction on a UObject and optionally attach a "payload" value (of any type) that will be passed as an additional
 * parameter when the delegate is executed. Primitive types are automatically boxed into corresponding UScriptStruct wrappers (FAngelscriptBoxed*) so they can
 * be stored inside an FInstancedStruct.
 *
 * Example usage in Angelscript:
 * @code
 *     // Bind without payload — target function takes no parameters
 *     FAngelscriptDelegateWithPayload Delegate;
 *     Delegate.BindUFunction(MyObject, n"OnCompleted");
 *     Delegate.ExecuteIfBound();  // calls MyObject.OnCompleted()
 *
 *     // Bind with payload — target function receives the payload as its parameter
 *     int ItemID = 42;
 *     FAngelscriptDelegateWithPayload PayloadDelegate;
 *     PayloadDelegate.BindWithPayload(MyObject, n"OnItemProcessed", ItemID);
 *     PayloadDelegate.ExecuteIfBound();  // calls MyObject.OnItemProcessed(42)
 * @endcode
 */
USTRUCT(BlueprintType)
struct ANGELSCRIPTRUNTIME_API FAngelscriptDelegateWithPayload
{
	GENERATED_BODY()

	/** The boxed payload data to pass to the bound function upon execution. */
	UPROPERTY(BlueprintReadWrite, Category = "Delegate")
	FInstancedStruct Payload;

	/** Weak reference to the target UObject that owns the bound function. */
	UPROPERTY(BlueprintReadWrite, Category = "Delegate")
	TWeakObjectPtr<UObject> Object;

	/** The name of the UFunction to invoke on the target object. */
	UPROPERTY(BlueprintReadWrite, Category = "Delegate")
	FName FunctionName;

	/** Returns true if the delegate is bound to a valid object and function. */
	bool IsBound() const;

	/** Executes the bound function if the delegate is currently bound. */
	void ExecuteIfBound() const;

	/** Binds this delegate to a UFunction on the given object (no payload). */
	void BindUFunction(UObject* Object, FName FunctionName);

	/**
	 * Binds this delegate to a UFunction on the given object with a payload value.
	 *
	 * @param Object            The target UObject that owns the function.
	 * @param FunctionName      The name of the UFunction to bind.
	 * @param PayloadPtr        Pointer to the payload data to attach.
	 * @param PayloadScriptTypeId  The Angelscript type ID of the payload, used to
	 *                             determine the appropriate boxed struct type.
	 */
	void BindUFunctionWithPayload(UObject* Object, FName FunctionName, void* PayloadPtr, int PayloadScriptTypeId);

	/** Resets the delegate, clearing the bound object, function name, and payload. */
	void Reset();

	/**
	 * Maps an Angelscript primitive type ID to the corresponding boxed UScriptStruct.
	 *
	 * @param TypeId  The Angelscript type ID to look up.
	 * @return The UScriptStruct* for the boxed wrapper, or nullptr if not a primitive.
	 */
	static UScriptStruct* GetBoxedPrimitiveStructFromTypeId(int TypeId);
};

//=============================================================================
// Boxed Primitive Structs
//
// These structs wrap primitive value types into UScriptStruct containers so
// they can be stored inside FInstancedStruct. They are internal implementation
// details and are NOT exposed to Angelscript (NoAutoAngelscriptBind).
//
// +---------------------------+-----------+----------------------------------+
// | Struct Name               | Value Type| Description                      |
// +---------------------------+-----------+----------------------------------+
// | FAngelscriptBoxedByte     | uint8     | Boxed 8-bit unsigned integer     |
// | FAngelscriptBoxedShort    | uint16    | Boxed 16-bit unsigned integer    |
// | FAngelscriptBoxedInt32    | uint32    | Boxed 32-bit unsigned integer    |
// | FAngelscriptBoxedInt64    | uint64    | Boxed 64-bit unsigned integer    |
// | FAngelscriptBoxedFloat    | float     | Boxed 32-bit floating point      |
// | FAngelscriptBoxedDouble   | double    | Boxed 64-bit floating point      |
// +---------------------------+-----------+----------------------------------+
//=============================================================================

USTRUCT(BlueprintType, Meta = (NoAutoAngelscriptBind))
struct FAngelscriptBoxedByte
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Value = 0;
};

USTRUCT(BlueprintType, Meta = (NoAutoAngelscriptBind))
struct FAngelscriptBoxedShort
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 Value = 0;
};

USTRUCT(BlueprintType, Meta = (NoAutoAngelscriptBind))
struct FAngelscriptBoxedInt32
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Value = 0;
};

USTRUCT(BlueprintType, Meta = (NoAutoAngelscriptBind))
struct FAngelscriptBoxedInt64
{
	GENERATED_BODY()

	UPROPERTY()
	uint64 Value = 0;
};

USTRUCT(BlueprintType, Meta = (NoAutoAngelscriptBind))
struct FAngelscriptBoxedFloat
{
	GENERATED_BODY()

	UPROPERTY()
	float Value = 0;
};

USTRUCT(BlueprintType, Meta = (NoAutoAngelscriptBind))
struct FAngelscriptBoxedDouble
{
	GENERATED_BODY()

	UPROPERTY()
	double Value = 0;
};
