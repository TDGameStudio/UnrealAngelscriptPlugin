#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "AngelscriptEngine.h"

struct FJsonValueContainer;
struct FJsonValueArrayContainer;
struct FJsonObjectContainer;
struct FJsonObjectFieldIterator;

struct FAngelscriptJsonBinds
{
	static FString ValueTypeToString(EJson Type);

#if AS_ITERATOR_DEBUGGING
	static bool CheckObjectIteratorDebug(void* MapPtr);
#endif

	static void ConstructValue(FJsonValueContainer* Address);
	static void DestructValue(FJsonValueContainer& Object);
	static void ConstructArray(FJsonValueArrayContainer* Address);
	static void DestructArray(FJsonValueArrayContainer& Object);
	static void ConstructObjectCopy(FJsonObjectContainer* Address, const FJsonObjectContainer& InObject);
	static void ConstructObject(FJsonObjectContainer* Address);
	static void DestructObject(FJsonObjectContainer& Object);
	static bool TryGetObjectField(const FJsonObjectContainer& Object, const FString& FieldName, FJsonObjectContainer& OutObject);
	static bool TryGetArrayField(const FJsonObjectContainer& Object, const FString& FieldName, FJsonValueArrayContainer& OutArray);
	static bool LoadFromString(FJsonObjectContainer* Object, const FString& JsonString);
	static FString SaveToString(FJsonObjectContainer* Object, bool bPrettyPrint);
	static void DestructIterator(FJsonObjectFieldIterator& Iterator);
	static FJsonObjectFieldIterator& Proceed(FJsonObjectFieldIterator& Iterator);
	static FJsonObjectFieldIterator Iterator(FJsonObjectContainer* Object);
	static FJsonObjectContainer ParseString(const FString& JsonString);
};

struct FJsonValueContainer
{
	TSharedPtr<FJsonValue> Value;

	FJsonValueContainer() = default;
	FJsonValueContainer(TSharedPtr<FJsonValue> InValue) : Value(InValue) {}

	EJson GetType() const
	{
		if (Value.IsValid())
			return Value->Type;
		else
			return EJson::None;
	}

	bool TryGetNumber(double& OutNumber) const
	{
		if (Value.IsValid())
			return Value->TryGetNumber(OutNumber);
		else
			return false;
	}

	bool TryGetNumber(float& OutNumber) const
	{
		if (Value.IsValid())
			return Value->TryGetNumber(OutNumber);
		else
			return false;
	}

	bool TryGetNumber(int32& OutNumber) const
	{
		if (Value.IsValid())
			return Value->TryGetNumber(OutNumber);
		else
			return false;
	}

	bool TryGetNumber(int64& OutNumber) const
	{
		if (Value.IsValid())
			return Value->TryGetNumber(OutNumber);
		else
			return false;
	}

	bool TryGetString(FString& OutString) const
	{
		if (Value.IsValid())
			return Value->TryGetString(OutString);
		else
			return false;
	}

	bool TryGetBool(bool& OutBool) const
	{
		if (Value.IsValid())
			return Value->TryGetBool(OutBool);
		else
			return false;
	}

	bool TryGetArray(FJsonValueArrayContainer& OutArray) const;
	bool TryGetObject(FJsonObjectContainer& Object) const;

	bool IsNull() const
	{
		if (Value.IsValid())
			return Value->Type == EJson::Null || Value->Type == EJson::None;
		else
			return false;
	}
};

struct FJsonValueArrayContainer
{
	TArray<TSharedPtr<FJsonValue>> Array;

	void Empty()
	{
		Array.Empty();
	}

	void AddNull()
	{
		Array.Add(MakeShared<FJsonValueNull>());
	}

	void AddString(const FString& Str)
	{
		Array.Add(MakeShared<FJsonValueString>(Str));
	}

	void AddNumberInt(int32 I)
	{
		Array.Add(MakeShared<FJsonValueNumber>(I));
	}

	void AddNumberDouble(double D)
	{
		Array.Add(MakeShared<FJsonValueNumber>(D));
	}

	void AddBoolean(bool B)
	{
		Array.Add(MakeShared<FJsonValueBoolean>(B));
	}

	void AddArray(const FJsonValueArrayContainer& ArrayValue)
	{
		Array.Add(MakeShared<FJsonValueArray>(ArrayValue.Array));
	}

	void AddObject(const FJsonObjectContainer& ObjectValue);

	void AddValue(const FJsonValueContainer& Value)
	{
		Array.Add(Value.Value);
	}

	int32 Num() const
	{
		return Array.Num();
	}

	FJsonValueContainer GetValueAt(int32 Index) const
	{
		if (!Array.IsValidIndex(Index))
		{
			FAngelscriptEngine::Throw("Array index is out of bounds");
			return {};
		}

		return { Array[Index] };
	}

	// TODO : expand FJsonValueArrayContainer to support every value create/access method
};

struct FJsonObjectContainer
{
	TSharedPtr<FJsonObject> JsonObject;

	FJsonObjectContainer() = default;
	FJsonObjectContainer(TSharedPtr<FJsonObject> InJsonObject) : JsonObject(InJsonObject) {}

	// this one is not for binding!
	bool CheckValidObject() const
	{
		if (JsonObject.IsValid())
			return true;
		else
		{
			FAngelscriptEngine::Throw("JsonObject is not valid");
			return false;
		}
	}

	void TypeErrorMessage(EJson FieldType, const FString& InType) const
	{
		auto Message = FString::Printf(TEXT("Json Value of type '%s' used as a '%s'."), *FAngelscriptJsonBinds::ValueTypeToString(FieldType), *InType);
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
	}

#if AS_ITERATOR_DEBUGGING
	bool CheckIteratorForNewField(const FString& FieldName) const
	{
		if (JsonObject.IsValid())
		{
			// adding a new field during iteration might reallocate/rehash the map and cause issues
			if (!JsonObject->HasField(FieldName))
				return FAngelscriptJsonBinds::CheckObjectIteratorDebug(&JsonObject->Values);
			return true;
		}

		return false;
	}
#endif

	bool IsValid() const
	{
		return JsonObject.IsValid();
	}

	bool HasField(const FString& FieldName) const
	{
		return CheckValidObject() && JsonObject->HasField(FieldName);
	}

	FString GetStringField(const FString& FieldName) const
	{
		if (CheckValidObject())
		{
			FString String;

			auto Value = JsonObject->GetField<EJson::None>(FieldName);
			if (!Value->TryGetString(String))
			{
				TypeErrorMessage(Value->Type, TEXT("String"));
			}

			return String;
		}
		else
		{
			return TEXT("");
		}
	}

	void SetStringField(const FString& FieldName, const FString& StringValue)
	{
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!CheckIteratorForNewField(FieldName))
				return;
#endif
			JsonObject->SetStringField(FieldName, StringValue);
		}
	}

	double GetNumberField(const FString& FieldName) const
	{
		if (CheckValidObject())
		{
			double Number;

			auto Value = JsonObject->GetField<EJson::None>(FieldName);
			if (!Value->TryGetNumber(Number))
			{
				TypeErrorMessage(Value->Type, TEXT("Number"));
			}

			return Number;
		}
		else
		{
			return 0.;
		}
	}

	void SetNumberField(const FString& FieldName, double Number)
	{
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!CheckIteratorForNewField(FieldName))
				return;
#endif
			JsonObject->SetNumberField(FieldName, Number);
		}
	}

	bool GetBoolField(const FString& FieldName) const
	{
		if (CheckValidObject())
		{
			bool bBool;

			auto Value = JsonObject->GetField<EJson::None>(FieldName);
			if (!Value->TryGetBool(bBool))
			{
				TypeErrorMessage(Value->Type, TEXT("Bool"));
			}

			return bBool;
		}
		else
		{
			return false;
		}
	}

	void SetBoolField(const FString& FieldName, bool InValue)
	{
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!CheckIteratorForNewField(FieldName))
				return;
#endif
			JsonObject->SetBoolField(FieldName, InValue);
		}
	}

	FJsonObjectContainer GetObjectField(const FString& FieldName) const
	{
		if (CheckValidObject())
		{
			auto Value = JsonObject->GetField<EJson::None>(FieldName);
			if (Value->Type == EJson::Object)
			{
				return FJsonObjectContainer(JsonObject->GetObjectField(FieldName));
			}

			TypeErrorMessage(Value->Type, TEXT("Object"));
		}

		return FJsonObjectContainer();
	}

	void SetObjectField(const FString& FieldName, const FJsonObjectContainer& InObject)
	{
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!CheckIteratorForNewField(FieldName))
				return;
#endif
			JsonObject->SetObjectField(FieldName, InObject.JsonObject);
		}
	}

	FJsonObjectContainer CreateObjectField(const FString& FieldName)
	{
		FJsonObjectContainer Result;
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!CheckIteratorForNewField(FieldName))
				return Result;
#endif
			Result.JsonObject = MakeShared<FJsonObject>();
			JsonObject->SetObjectField(FieldName, Result.JsonObject);
		}
		return Result;
	}

	FJsonValueArrayContainer GetArrayField(const FString& FieldName) const
	{
		if (CheckValidObject())
		{
			auto Value = JsonObject->GetField<EJson::None>(FieldName);
			if (Value->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>>* Array;
				if (JsonObject->TryGetArrayField(FieldName, Array))
				{
					FJsonValueArrayContainer Result;
					Result.Array = *Array;
					return Result;
				}
			}
			else
			{
				TypeErrorMessage(Value->Type, TEXT("Array"));
			}
		}

		return FJsonValueArrayContainer();
	}

	void SetArrayField(const FString& FieldName, const FJsonValueArrayContainer& Array)
	{
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!FAngelscriptJsonBinds::CheckObjectIteratorDebug(&JsonObject->Values))
				return;
#endif
			JsonObject->SetArrayField(FieldName, Array.Array);
		}
	}

	FJsonValueContainer GetField(const FString& FieldName)
	{
		if (CheckValidObject())
		{
			return FJsonValueContainer(JsonObject->TryGetField(FieldName));
		}
		else
			return FJsonValueContainer();
	}

	void SetField(const FString& FieldName, const FJsonValueContainer& Value)
	{
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!CheckIteratorForNewField(FieldName))
				return;
#endif
			JsonObject->SetField(FieldName, Value.Value);
		}
	}

	void RemoveField(const FString& FieldName)
	{
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!FAngelscriptJsonBinds::CheckObjectIteratorDebug(&JsonObject->Values))
				return;
#endif
			JsonObject->RemoveField(FieldName);
		}
	}

	void RemoveAllFields()
	{
		if (CheckValidObject())
		{
#if AS_ITERATOR_DEBUGGING
			if (!FAngelscriptJsonBinds::CheckObjectIteratorDebug(&JsonObject->Values))
				return;
#endif
			JsonObject->Values.Empty();
		}
	}

	// TODO : expand FJsonObjectContainer to support every field type create/access method
};

inline void FJsonValueArrayContainer::AddObject(const FJsonObjectContainer& ObjectValue)
{
	Array.Add(MakeShared<FJsonValueObject>(ObjectValue.JsonObject));
}

struct FJsonObjectFieldIterator
{
	using MapType = decltype(FJsonObject::Values);
	MapType::TIterator Iterator;

	bool bCanProceed;
	MapType::TMapBase::ElementType CurrentValue;
	bool bValidValue;

#if AS_ITERATOR_DEBUGGING
	MapType* MapPtr;
#endif

	FJsonObjectFieldIterator(MapType& JsonValues)
		: Iterator(JsonValues.CreateIterator())
		, bCanProceed(Iterator)
		, bValidValue(false)
#if AS_ITERATOR_DEBUGGING
		, MapPtr(&JsonValues)
#endif
	{}

	FJsonObjectFieldIterator()
		: Iterator(MapType().CreateIterator())
		, bCanProceed(false)
		, bValidValue(false)
#if AS_ITERATOR_DEBUGGING
		, MapPtr(nullptr)
#endif
	{}

	bool CheckValidIterator() const
	{
		if (!bValidValue)
		{
			FAngelscriptEngine::Throw("Iterator out of range");
			return false;
		}
		return true;
	}

	FString GetFieldName() const
	{
		if (!CheckValidIterator())
		{
			return TEXT("");
		}
#if !UE_JSONOBJECT_LEGACY_STRING_KEYS
		return FString(CurrentValue.Key.ToView());
#else
		return CurrentValue.Key;
#endif
	}

	FJsonValueContainer GetValue() const
	{
		if (!CheckValidIterator())
		{
			return FJsonValueContainer();
		}
		return FJsonValueContainer(CurrentValue.Value);
	}

	EJson GetType() const
	{
		if (CheckValidIterator() && CurrentValue.Value.IsValid())
		{
			return CurrentValue.Value->Type;
		}

		return EJson::None;
	}
};

inline bool FJsonValueContainer::TryGetArray(FJsonValueArrayContainer& OutArray) const
{
	if (Value.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
		if (Value->TryGetArray(ArrayValue))
		{
			OutArray.Array = *ArrayValue;
			return true;
		}
	}
	return false;
}

inline bool FJsonValueContainer::TryGetObject(FJsonObjectContainer& Object) const
{
	if (Value.IsValid())
	{
		const TSharedPtr<FJsonObject>* ObjectValue;
		if (Value->TryGetObject(ObjectValue))
		{
			Object.JsonObject = *ObjectValue;
			return true;
		}
	}

	return false;
}
