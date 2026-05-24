#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "Internationalization/Text.h"
#include "Misc/AutomationTest.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingPath.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "UObject/PropertyOptional.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

/**
 * AngelscriptReflectiveAccess - reusable helpers that let C++ tests read, write,
 * and invoke Angelscript-defined members by *string path / string name*, instead
 * of hand-rolled `FindFProperty<FIntProperty>` / hand-aligned parameter buffers
 * or casts to private AS types.
 *
 * Two complementary building blocks are exposed:
 *
 *   1. Property access by path  (FPropertyBindingPath-based)
 *      ---------------------------------------------------
 *      Supports nested member access like "NestedData.Counter" or
 *      "Inventory[2].Quantity" against any `UObject*` (or `UStruct` + raw ptr).
 *      Any AS-declared UPROPERTY (incl. ones inside USTRUCT payloads and TArray
 *      elements) is reachable the same way.
 *
 *   2. Function invocation by name  (FindFunction + ProcessEvent-based)
 *      ---------------------------------------------------
 *      Builds a typed parameter packet via a small `TFunctionInvoker` helper so
 *      callers don't manually allocate/initialize/destroy the parameter buffer.
 *      Works for every AS-declared UFUNCTION (incl. those returning a value).
 *
 * The helpers intentionally take an `FAutomationTestBase&` so failure cases are
 * routed through `AddError`/`TestX` and never silent - tests that use them read
 * more like specifications than like reflection plumbing.
 */
/** Resolve a `"A.B[idx].C"`-style path against `Object` and return the leaf indirection. */
inline bool ResolvePathOnObject(
	FAutomationTestBase& Test,
	UObject* Object,
	FStringView Path,
	FPropertyBindingPathIndirection& OutLeaf,
	FString* OutError = nullptr)
{
	if (!Test.TestNotNull(TEXT("Reflective access requires a valid UObject"), Object))
	{
		return false;
	}

	FPropertyBindingPath BindingPath;
	if (!BindingPath.FromString(Path))
	{
		Test.AddError(FString::Printf(TEXT("Failed to parse property path '%.*s'"), Path.Len(), Path.GetData()));
		return false;
	}

	TArray<FPropertyBindingPathIndirection> Indirections;
	FString LocalError;
	const bool bResolved = BindingPath.ResolveIndirectionsWithValue(
		FPropertyBindingDataView(Object->GetClass(), Object),
		Indirections,
		OutError != nullptr ? OutError : &LocalError);

	if (!bResolved || Indirections.Num() == 0)
	{
		const FString& ErrorText = OutError != nullptr ? *OutError : LocalError;
		Test.AddError(FString::Printf(
			TEXT("Failed to resolve property path '%.*s' on %s: %s"),
			Path.Len(), Path.GetData(),
			*Object->GetClass()->GetName(),
			ErrorText.IsEmpty() ? TEXT("<no detail>") : *ErrorText));
		return false;
	}

	OutLeaf = Indirections.Last();
	return true;
}

/**
 * Read a property value by string path (e.g. "Health", "Stats.Score", "Items[1].Quantity").
 * Validates both the FProperty subtype and the object identity before touching memory.
 */
template <typename PropertyType, typename ValueType>
bool GetByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, ValueType& OutValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const PropertyType* Typed = CastField<const PropertyType>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be %s"), Path.Len(), Path.GetData(), *PropertyType::StaticClass()->GetName()),
			Typed))
	{
		return false;
	}

	OutValue = Typed->GetPropertyValue(Leaf.GetPropertyAddress());
	return true;
}

/** Write a property value by string path. */
template <typename PropertyType, typename ValueType>
bool SetByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, const ValueType& InValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const PropertyType* Typed = CastField<const PropertyType>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be %s"), Path.Len(), Path.GetData(), *PropertyType::StaticClass()->GetName()),
			Typed))
	{
		return false;
	}

	Typed->SetPropertyValue(Leaf.GetMutablePropertyAddress(), InValue);
	return true;
}

/** Verify a property value by string path (thin wrapper around GetByPath + TestEqual). */
template <typename PropertyType, typename ValueType>
bool VerifyByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, const ValueType& Expected, const TCHAR* What = nullptr)
{
	ValueType Actual{};
	if (!GetByPath<PropertyType, ValueType>(Test, Object, Path, Actual))
	{
		return false;
	}

	const FString Description = What != nullptr
		? FString(What)
		: FString::Printf(TEXT("Property '%.*s' should equal expected value"), Path.Len(), Path.GetData());
	return Test.TestEqual(*Description, Actual, Expected);
}

// -------------------------------------------------------------------------
// Specialized helpers - the generic GetByPath/SetByPath assume a scalar
// `GetPropertyValue(void*)` overload, which doesn't exist for struct /
// object / enum / array properties. These helpers bridge those cases so
// callers never have to hand-roll the access code.
// -------------------------------------------------------------------------

/** Read an entire USTRUCT value at the given path (e.g. "LastVectorValue" -> FVector). */
template <typename StructType>
bool GetStructByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, StructType& OutValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FStructProperty* StructProp = CastField<const FStructProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a USTRUCT"), Path.Len(), Path.GetData()),
			StructProp))
	{
		return false;
	}

	// Use TBaseStructure<T>::Get() so core types like FVector (which don't
	// declare a StaticStruct() member) are still supported.
	const UScriptStruct* ExpectedStruct = TBaseStructure<StructType>::Get();
	if (!Test.TestTrue(
			*FString::Printf(TEXT("USTRUCT property '%.*s' should match expected struct type"),
				Path.Len(), Path.GetData()),
			StructProp->Struct != nullptr && ExpectedStruct != nullptr
			&& StructProp->Struct->IsChildOf(ExpectedStruct)))
	{
		return false;
	}

	OutValue = *static_cast<const StructType*>(Leaf.GetPropertyAddress());
	return true;
}

/** Verify an entire USTRUCT value at the given path. */
template <typename StructType>
bool VerifyStructByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, const StructType& Expected, const TCHAR* What = nullptr)
{
	StructType Actual{};
	if (!GetStructByPath<StructType>(Test, Object, Path, Actual))
	{
		return false;
	}

	const bool bEqual = Actual == Expected;
	const FString Description = What != nullptr
		? FString(What)
		: FString::Printf(TEXT("USTRUCT '%.*s' should equal expected value"), Path.Len(), Path.GetData());
	return Test.TestTrue(*Description, bEqual);
}

/** Read a UObject reference (TObjectPtr / raw pointer / TSubclassOf-backing) at the given path. */
inline bool GetObjectByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, UObject*& OutValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FObjectPropertyBase* ObjProp = CastField<const FObjectPropertyBase>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a UObject reference"), Path.Len(), Path.GetData()),
			ObjProp))
	{
		return false;
	}

	OutValue = ObjProp->GetObjectPropertyValue(Leaf.GetPropertyAddress());
	return true;
}

/** Write a UObject reference (TObjectPtr / raw pointer) at the given path. */
inline bool SetObjectByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, UObject* InValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FObjectPropertyBase* ObjProp = CastField<const FObjectPropertyBase>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a UObject reference"), Path.Len(), Path.GetData()),
			ObjProp))
	{
		return false;
	}

	if (!Test.TestTrue(
			*FString::Printf(TEXT("Object assigned to '%.*s' should match the reflected property class"), Path.Len(), Path.GetData()),
			InValue == nullptr || ObjProp->PropertyClass == nullptr || InValue->IsA(ObjProp->PropertyClass)))
	{
		return false;
	}

	ObjProp->SetObjectPropertyValue(Leaf.GetMutablePropertyAddress(), InValue);
	return true;
}

/**
 * Read an enum value (either FEnumProperty or FByteProperty-with-UEnum) at the
 * given path. Works for AS-declared enums bound as UENUMs as well as raw uint8
 * enum properties declared in C++.
 */
inline bool GetEnumByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, int64& OutValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FProperty* Prop = Leaf.GetProperty();
	if (const FEnumProperty* EnumProp = CastField<const FEnumProperty>(Prop))
	{
		OutValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(Leaf.GetPropertyAddress());
		return true;
	}
	if (const FByteProperty* ByteProp = CastField<const FByteProperty>(Prop))
	{
		OutValue = static_cast<int64>(ByteProp->GetPropertyValue(Leaf.GetPropertyAddress()));
		return true;
	}

	Test.AddError(FString::Printf(
		TEXT("Property '%.*s' is not an enum-backed property (type=%s)"),
		Path.Len(), Path.GetData(),
		Prop != nullptr ? *Prop->GetClass()->GetName() : TEXT("<null>")));
	return false;
}

/**
 * Return the element count of a TArray-typed property at the given path.
 * Useful for asserting that AS-side code populated an array as expected
 * before drilling into specific indices via GetByPath.
 */
inline bool GetArrayNumByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, int32& OutCount)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FArrayProperty* ArrayProp = CastField<const FArrayProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TArray"), Path.Len(), Path.GetData()),
			ArrayProp))
	{
		return false;
	}

	FScriptArrayHelper Helper(ArrayProp, Leaf.GetPropertyAddress());
	OutCount = Helper.Num();
	return true;
}

/**
 * Return the entry count of a TMap-typed property at the given path. FPropertyBindingPath
 * cannot index into TMap natively, so TMap access is split into "resolve the path to the
 * TMap property, then look up a specific key" - see GetMapValueByPath below.
 */
inline bool GetMapNumByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, int32& OutCount)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FMapProperty* MapProp = CastField<const FMapProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TMap"), Path.Len(), Path.GetData()),
			MapProp))
	{
		return false;
	}

	FScriptMapHelper Helper(MapProp, Leaf.GetPropertyAddress());
	OutCount = Helper.Num();
	return true;
}

/**
 * Look up a TMap entry by key and return its value. The key is passed by reference to
 * the same C++ type the AS side uses (FString, int32, FName, ...). Implementation walks
 * FScriptMapHelper comparing key memory via FProperty::Identical - which handles all
 * common scalar, name, and string keys.
 */
template <typename KeyType, typename ValuePropertyType, typename ValueType>
bool GetMapValueByPath(
	FAutomationTestBase& Test,
	UObject* Object,
	FStringView Path,
	const KeyType& Key,
	ValueType& OutValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FMapProperty* MapProp = CastField<const FMapProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TMap"), Path.Len(), Path.GetData()),
			MapProp))
	{
		return false;
	}

	FScriptMapHelper Helper(MapProp, Leaf.GetPropertyAddress());

	const ValuePropertyType* TypedValueProp = CastField<const ValuePropertyType>(MapProp->ValueProp);
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("TMap value property at '%.*s' should be %s"),
				Path.Len(), Path.GetData(), *ValuePropertyType::StaticClass()->GetName()),
			TypedValueProp))
	{
		return false;
	}

	for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
	{
		if (!Helper.IsValidIndex(SparseIndex))
		{
			continue;
		}

		const void* KeyPtr = Helper.GetKeyPtr(SparseIndex);
		if (*static_cast<const KeyType*>(KeyPtr) == Key)
		{
			const void* ValuePtr = Helper.GetValuePtr(SparseIndex);
			OutValue = TypedValueProp->GetPropertyValue(ValuePtr);
			return true;
		}
	}

	Test.AddError(FString::Printf(
		TEXT("TMap at '%.*s' does not contain the requested key"),
		Path.Len(), Path.GetData()));
	return false;
}

/** Return the element count of a TSet-typed property at the given path. */
inline bool GetSetNumByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, int32& OutCount)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FSetProperty* SetProp = CastField<const FSetProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TSet"), Path.Len(), Path.GetData()),
			SetProp))
	{
		return false;
	}

	FScriptSetHelper Helper(SetProp, Leaf.GetPropertyAddress());
	OutCount = Helper.Num();
	return true;
}

/** Returns true if the TSet at the given path contains the requested element. */
template <typename ElementType>
bool SetContainsByPath(
	FAutomationTestBase& Test,
	UObject* Object,
	FStringView Path,
	const ElementType& Element)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FSetProperty* SetProp = CastField<const FSetProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TSet"), Path.Len(), Path.GetData()),
			SetProp))
	{
		return false;
	}

	FScriptSetHelper Helper(SetProp, Leaf.GetPropertyAddress());
	for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
	{
		if (!Helper.IsValidIndex(SparseIndex))
		{
			continue;
		}

		const void* ElementPtr = Helper.GetElementPtr(SparseIndex);
		if (*static_cast<const ElementType*>(ElementPtr) == Element)
		{
			return true;
		}
	}
	return false;
}

/**
 * Read a TSubclassOf / UClass*-style property at the given path. Both AS
 * `TSubclassOf<UObject>` fields and raw `UClass` fields reflect as FClassProperty.
 */
inline bool GetClassByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, UClass*& OutClass)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FClassProperty* ClassProp = CastField<const FClassProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a UClass reference"), Path.Len(), Path.GetData()),
			ClassProp))
	{
		return false;
	}

	OutClass = Cast<UClass>(ClassProp->GetObjectPropertyValue(Leaf.GetPropertyAddress()));
	return true;
}

/**
 * Return whether a TOptional-typed UPROPERTY at the given path holds a value.
 * AS `TOptional<T>` fields reflect as FOptionalProperty, whose layout can vary
 * between "is-set bool after the value" and "intrusive unset state", depending
 * on the inner property; the FOptionalPropertyLayout base class hides that.
 */
inline bool GetOptionalIsSetByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, bool& OutIsSet)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FOptionalProperty* OptProp = CastField<const FOptionalProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TOptional"), Path.Len(), Path.GetData()),
			OptProp))
	{
		return false;
	}

	OutIsSet = OptProp->IsSet(Leaf.GetPropertyAddress());
	return true;
}

/**
 * Read the inner value of a TOptional UPROPERTY. If the optional is unset the
 * test fails with a diagnostic - the caller should check IsSet via
 * GetOptionalIsSetByPath first if the unset case is expected.
 *
 * The ValuePropertyType template argument is the expected FProperty subclass
 * of the inner (so FIntProperty for TOptional<int>, FStructProperty for
 * TOptional<FVector>, etc.), ValueType is the C++ value type.
 */
template <typename ValuePropertyType, typename ValueType>
bool GetOptionalValueByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, ValueType& OutValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FOptionalProperty* OptProp = CastField<const FOptionalProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TOptional"), Path.Len(), Path.GetData()),
			OptProp))
	{
		return false;
	}

	if (!Test.TestTrue(
			*FString::Printf(TEXT("TOptional at '%.*s' should be set before reading its inner value"),
				Path.Len(), Path.GetData()),
			OptProp->IsSet(Leaf.GetPropertyAddress())))
	{
		return false;
	}

	const ValuePropertyType* InnerProp = CastField<const ValuePropertyType>(OptProp->GetValueProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("TOptional inner type at '%.*s' should be %s"),
				Path.Len(), Path.GetData(), *ValuePropertyType::StaticClass()->GetName()),
			InnerProp))
	{
		return false;
	}

	const void* ValuePtr = OptProp->GetValuePointerForRead(Leaf.GetPropertyAddress());
	OutValue = InnerProp->GetPropertyValue(ValuePtr);
	return true;
}

/**
 * Read an entire USTRUCT value out of a TOptional<FStruct> at the given path.
 * Specialisation of GetOptionalValueByPath for struct-typed inner values,
 * mirroring the split between GetByPath and GetStructByPath for plain UPROPERTYs.
 */
template <typename StructType>
bool GetOptionalStructByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, StructType& OutValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FOptionalProperty* OptProp = CastField<const FOptionalProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TOptional"), Path.Len(), Path.GetData()),
			OptProp))
	{
		return false;
	}

	if (!Test.TestTrue(
			*FString::Printf(TEXT("TOptional at '%.*s' should be set before reading its struct value"),
				Path.Len(), Path.GetData()),
			OptProp->IsSet(Leaf.GetPropertyAddress())))
	{
		return false;
	}

	const FStructProperty* InnerStruct = CastField<const FStructProperty>(OptProp->GetValueProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("TOptional inner type at '%.*s' should be a USTRUCT"), Path.Len(), Path.GetData()),
			InnerStruct))
	{
		return false;
	}

	const UScriptStruct* ExpectedStruct = TBaseStructure<StructType>::Get();
	if (!Test.TestTrue(
			*FString::Printf(TEXT("TOptional inner struct at '%.*s' should match expected struct type"),
				Path.Len(), Path.GetData()),
			InnerStruct->Struct != nullptr && ExpectedStruct != nullptr
			&& InnerStruct->Struct->IsChildOf(ExpectedStruct)))
	{
		return false;
	}

	OutValue = *static_cast<const StructType*>(OptProp->GetValuePointerForRead(Leaf.GetPropertyAddress()));
	return true;
}

// -------------------------------------------------------------------------
// Reference-type helpers (FText / soft / weak / FInstancedStruct)
// -------------------------------------------------------------------------

/** Read an FText UPROPERTY (reflected as FTextProperty). */
inline bool GetTextByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, FText& OutValue)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FTextProperty* TextProp = CastField<const FTextProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be FText"), Path.Len(), Path.GetData()),
			TextProp))
	{
		return false;
	}

	OutValue = TextProp->GetPropertyValue(Leaf.GetPropertyAddress());
	return true;
}

/**
 * Read a TSoftObjectPtr UPROPERTY (reflected as FSoftObjectProperty).
 * Returns the underlying FSoftObjectPath so the test can compare against a
 * known path without having to resolve / load the soft reference.
 */
inline bool GetSoftObjectPathByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, FSoftObjectPath& OutPath)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FSoftObjectProperty* SoftProp = CastField<const FSoftObjectProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TSoftObjectPtr"), Path.Len(), Path.GetData()),
			SoftProp))
	{
		return false;
	}

	// FSoftObjectProperty stores an FSoftObjectPtr which embeds an FSoftObjectPath.
	const FSoftObjectPtr* SoftRef = static_cast<const FSoftObjectPtr*>(Leaf.GetPropertyAddress());
	OutPath = SoftRef->ToSoftObjectPath();
	return true;
}

/** Read a TSoftClassPtr UPROPERTY (reflected as FSoftClassProperty). */
inline bool GetSoftClassPathByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, FSoftObjectPath& OutPath)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FSoftClassProperty* SoftClassProp = CastField<const FSoftClassProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TSoftClassPtr"), Path.Len(), Path.GetData()),
			SoftClassProp))
	{
		return false;
	}

	const FSoftObjectPtr* SoftRef = static_cast<const FSoftObjectPtr*>(Leaf.GetPropertyAddress());
	OutPath = SoftRef->ToSoftObjectPath();
	return true;
}

/**
 * Read a TWeakObjectPtr UPROPERTY (reflected as FWeakObjectProperty).
 * Returns the resolved UObject* (or nullptr if stale / unset).
 */
inline bool GetWeakObjectByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, UObject*& OutObject)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FWeakObjectProperty* WeakProp = CastField<const FWeakObjectProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a TWeakObjectPtr"), Path.Len(), Path.GetData()),
			WeakProp))
	{
		return false;
	}

	OutObject = WeakProp->GetObjectPropertyValue(Leaf.GetPropertyAddress());
	return true;
}

/**
 * Read the UScriptStruct wrapped by an FInstancedStruct UPROPERTY and expose the
 * underlying memory via the passed InspectFn callback. The callback signature is
 *   bool (const UScriptStruct* Struct, const void* Memory)
 * and should return `true` if the inspection succeeded.
 */
template <typename InspectFn>
bool InspectInstancedStructByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, InspectFn&& Fn)
{
	FPropertyBindingPathIndirection Leaf;
	if (!ResolvePathOnObject(Test, Object, Path, Leaf))
	{
		return false;
	}

	const FStructProperty* StructProp = CastField<const FStructProperty>(Leaf.GetProperty());
	if (!Test.TestNotNull(
			*FString::Printf(TEXT("Property '%.*s' should be a USTRUCT (FInstancedStruct)"),
				Path.Len(), Path.GetData()),
			StructProp))
	{
		return false;
	}

	if (!Test.TestTrue(
			*FString::Printf(TEXT("Property '%.*s' should be backed by FInstancedStruct"),
				Path.Len(), Path.GetData()),
			StructProp->Struct == FInstancedStruct::StaticStruct()))
	{
		return false;
	}

	const FInstancedStruct* Instanced = static_cast<const FInstancedStruct*>(Leaf.GetPropertyAddress());
	return Fn(Instanced->GetScriptStruct(), Instanced->GetMemory());
}

/**
 * Extract a specific typed payload from an FInstancedStruct UPROPERTY, validating
 * that the runtime struct type matches the expected one and copying the value.
 */
template <typename StructType>
bool GetInstancedStructByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, StructType& OutValue)
{
	return InspectInstancedStructByPath(
		Test, Object, Path,
		[&](const UScriptStruct* RuntimeStruct, const void* Memory) -> bool
		{
			const UScriptStruct* Expected = TBaseStructure<StructType>::Get();
			if (!Test.TestTrue(
					*FString::Printf(TEXT("FInstancedStruct at '%.*s' should contain the expected payload struct"),
						Path.Len(), Path.GetData()),
					RuntimeStruct != nullptr && Expected != nullptr && RuntimeStruct->IsChildOf(Expected)))
			{
				return false;
			}
			if (!Test.TestNotNull(
					*FString::Printf(TEXT("FInstancedStruct at '%.*s' should have live memory"),
						Path.Len(), Path.GetData()),
					Memory))
			{
				return false;
			}
			OutValue = *static_cast<const StructType*>(Memory);
			return true;
		});
}

/**
 * TFunctionInvoker - typed builder over FindFunction + ProcessEvent.
 *
 * The invoker walks the UFunction parameter list in declaration order, letting the caller
 * `AddParam<T>(value)` for each argument and finally `Call<R>()` (or `Call()` for void).
 * Parameter memory is allocated once and destroyed in the correct order, including
 * non-trivial types like FString / FName / TArray.
 *
 * Usage:
 *    auto Invoker = MakeFunctionInvoker(Test, Actor, TEXT("ApplyDamage"));
 *    Invoker.AddParam<float>(25.f);
 *    Invoker.AddParam<int32>(3);
 *    int32 Remaining = Invoker.Call<int32>();
 */
struct FFunctionInvoker
{
	FFunctionInvoker(FAutomationTestBase& InTest, UObject* InTarget, FName FunctionName)
		: Test(InTest)
		, Target(InTarget)
	{
		if (!Test.TestNotNull(TEXT("Function invocation requires a valid UObject"), Target))
		{
			return;
		}

		Function = Target->FindFunction(FunctionName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("UFUNCTION '%s' should exist on %s"), *FunctionName.ToString(), *Target->GetClass()->GetName()),
				Function))
		{
			return;
		}

		// FStructOnScope(Function) allocates + value-initializes the whole parameter
		// block (incl. the return value slot). This matches the layout that both
		// UObject::ProcessEvent and UASFunction::RuntimeCallEvent expect.
		if (Function->ParmsSize > 0)
		{
			Params = MakeUnique<FStructOnScope>(Function);
			if (!Test.TestNotNull(
					*FString::Printf(TEXT("Function '%s' should allocate a reflected parameter buffer"), *Function->GetName()),
					Params.IsValid() ? Params->GetStructMemory() : nullptr))
			{
				bValid = false;
				return;
			}
		}

		// Collect input parameters (exclude return value) in declaration order.
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				InputParams.Add(*It);
			}
		}

		bValid = true;
	}

	~FFunctionInvoker() = default;

	FFunctionInvoker(const FFunctionInvoker&) = delete;
	FFunctionInvoker& operator=(const FFunctionInvoker&) = delete;

	bool IsValid() const { return bValid; }

	void* GetParamsMemory() const
	{
		return Params.IsValid() ? Params->GetStructMemory() : nullptr;
	}

	template <typename ValueType>
	FFunctionInvoker& AddParam(const ValueType& Value)
	{
		if (!bValid)
		{
			return *this;
		}

		if (!Test.TestTrue(
				*FString::Printf(TEXT("Parameter index %d should be in range for %s"), NextParamIndex, *Function->GetName()),
				InputParams.IsValidIndex(NextParamIndex)))
		{
			bValid = false;
			return *this;
		}

		FProperty* Prop = InputParams[NextParamIndex];
		void* Slot = Prop->ContainerPtrToValuePtr<void>(GetParamsMemory());
		*reinterpret_cast<ValueType*>(Slot) = Value;
		++NextParamIndex;
		return *this;
	}

	/**
	 * Read a parameter's current value out of the parm buffer after Call(), useful for
	 * capturing the effect of AS `&out` reference parameters on the caller's side.
	 * Pass the SAME index that was used for AddParam<> (0-based, excludes the return slot).
	 */
	template <typename ValueType>
	bool ReadParamAfterCall(int32 ParamIndex, ValueType& OutValue)
	{
		if (!bValid)
		{
			return false;
		}

		if (!Test.TestTrue(
				*FString::Printf(TEXT("ReadParamAfterCall index %d should be in range for %s"), ParamIndex, *Function->GetName()),
				InputParams.IsValidIndex(ParamIndex)))
		{
			return false;
		}

		FProperty* Prop = InputParams[ParamIndex];
		const void* Slot = Prop->ContainerPtrToValuePtr<void>(GetParamsMemory());
		OutValue = *reinterpret_cast<const ValueType*>(Slot);
		return true;
	}

	/**
	 * Advanced escape hatch: return the mutable slot (and FProperty) for the
	 * *next* parameter so callers can populate complex packed layouts
	 * (e.g. an AS-declared USTRUCT parameter built from sub-field reflection)
	 * without going through AddParam<>. The call advances the cursor so subsequent
	 * AddParam / AddParamSlot / Call still target the right slot.
	 */
	bool AddParamSlot(FProperty*& OutProp, void*& OutSlot)
	{
		if (!bValid)
		{
			OutProp = nullptr;
			OutSlot = nullptr;
			return false;
		}

		if (!Test.TestTrue(
				*FString::Printf(TEXT("Parameter index %d should be in range for %s"), NextParamIndex, *Function->GetName()),
				InputParams.IsValidIndex(NextParamIndex)))
		{
			bValid = false;
			return false;
		}

		OutProp = InputParams[NextParamIndex];
		OutSlot = OutProp->ContainerPtrToValuePtr<void>(GetParamsMemory());
		++NextParamIndex;
		return true;
	}

	/**
	 * Assign the next TOptional<T> in-parameter.
	 * Use SetParamOptional<InnerPropertyType, InnerValueType>(value) for set state,
	 * or SetParamOptionalUnset() for the unset state. Advances the next-param cursor.
	 */
	template <typename InnerPropertyType, typename InnerValueType>
	FFunctionInvoker& SetParamOptional(const InnerValueType& InnerValue)
	{
		if (!bValid) return *this;
		FOptionalProperty* OptProp = nullptr;
		void* Slot = nullptr;
		if (!PrepareOptionalSlot(OptProp, Slot)) return *this;

		void* InnerSlot = OptProp->MarkSetAndGetInitializedValuePointerToReplace(Slot);
		if (const InnerPropertyType* InnerProp = CastField<const InnerPropertyType>(OptProp->GetValueProperty()))
		{
			InnerProp->SetPropertyValue(InnerSlot, InnerValue);
		}
		else
		{
			// Fallback for struct types: memcpy via inner property's InitializeValue + CopyValue.
			*reinterpret_cast<InnerValueType*>(InnerSlot) = InnerValue;
		}
		++NextParamIndex;
		return *this;
	}

	/** Advance past the next TOptional in-parameter leaving it in the unset state. */
	FFunctionInvoker& SetParamOptionalUnset()
	{
		if (!bValid) return *this;
		FOptionalProperty* OptProp = nullptr;
		void* Slot = nullptr;
		if (!PrepareOptionalSlot(OptProp, Slot)) return *this;

		OptProp->MarkUnset(Slot);
		++NextParamIndex;
		return *this;
	}

	/** Read a TOptional return value's IsSet + inner-value after CallAndReturn-style invocation. */
	template <typename InnerPropertyType, typename InnerValueType>
	bool ReadOptionalReturn(bool& OutIsSet, InnerValueType& OutInner)
	{
		if (!bValid) return false;
		if (!Call()) return false;

		FProperty* Return = Function->GetReturnProperty();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Function '%s' should declare a return value"), *Function->GetName()),
				Return))
		{
			return false;
		}

		const FOptionalProperty* OptProp = CastField<const FOptionalProperty>(Return);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Function '%s' should return a TOptional"), *Function->GetName()),
				OptProp))
		{
			return false;
		}

		const void* ReturnSlot = OptProp->ContainerPtrToValuePtr<void>(GetParamsMemory());
		OutIsSet = OptProp->IsSet(ReturnSlot);
		if (!OutIsSet)
		{
			return true;
		}

		const InnerPropertyType* InnerProp = CastField<const InnerPropertyType>(OptProp->GetValueProperty());
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("TOptional inner type on '%s' should match expected FProperty"), *Function->GetName()),
				InnerProp))
		{
			return false;
		}
		OutInner = InnerProp->GetPropertyValue(OptProp->GetValuePointerForRead(ReturnSlot));
		return true;
	}

	bool Call()
	{
		if (!bValid)
		{
			return false;
		}

		if (!Test.TestEqual(
				*FString::Printf(TEXT("Function '%s' should receive the declared number of parameters"), *Function->GetName()),
				NextParamIndex,
				InputParams.Num()))
		{
			return false;
		}

		void* Parms = GetParamsMemory();

		// Angelscript-generated UFUNCTIONs don't go through the native ProcessEvent
		// dispatch - they must route through UASFunction::RuntimeCallEvent. We hide
		// that distinction here so callers only ever say `FindFunction + invoke`.
		if (UASFunction* ScriptFunction = Cast<UASFunction>(Function))
		{
			if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
			{
				FAngelscriptEngineScope EngineScope(*CurrentEngine, Target);
				ScriptFunction->RuntimeCallEvent(Target, Parms);
			}
			else
			{
				ScriptFunction->RuntimeCallEvent(Target, Parms);
			}
		}
		else
		{
			Target->ProcessEvent(Function, Parms);
		}
		return true;
	}

	/** Call the function and read its return value. Fails the test if there is no return property. */
	template <typename ReturnType>
	ReturnType CallAndReturn(const ReturnType& Fallback = ReturnType{})
	{
		if (!Call())
		{
			return Fallback;
		}

		FProperty* Return = Function->GetReturnProperty();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Function '%s' should declare a return value"), *Function->GetName()),
				Return))
		{
			return Fallback;
		}

		const void* ReturnSlot = Return->ContainerPtrToValuePtr<void>(GetParamsMemory());
		return *reinterpret_cast<const ReturnType*>(ReturnSlot);
	}

private:
	bool PrepareOptionalSlot(FOptionalProperty*& OutOpt, void*& OutSlot)
	{
		if (!Test.TestTrue(
				*FString::Printf(TEXT("Optional parameter index %d should be in range for %s"),
					NextParamIndex, *Function->GetName()),
				InputParams.IsValidIndex(NextParamIndex)))
		{
			bValid = false;
			return false;
		}

		FProperty* Prop = InputParams[NextParamIndex];
		OutOpt = CastField<FOptionalProperty>(Prop);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Parameter %d of '%s' should be a TOptional"), NextParamIndex, *Function->GetName()),
				OutOpt))
		{
			bValid = false;
			return false;
		}
		OutSlot = Prop->ContainerPtrToValuePtr<void>(GetParamsMemory());
		return true;
	}

	FAutomationTestBase& Test;
	UObject* Target = nullptr;
	UFunction* Function = nullptr;
	TUniquePtr<FStructOnScope> Params;
	TArray<FProperty*> InputParams;
	int32 NextParamIndex = 0;
	bool bValid = false;
};

