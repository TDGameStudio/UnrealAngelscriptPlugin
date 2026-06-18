#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

#if AS_ITERATOR_DEBUGGING
thread_local static TArray<void*, TInlineAllocator<16>> GFieldMapsBeingIterated;

static bool CheckObjectIteratorDebug(void* MapPtr)
{
	if (GFieldMapsBeingIterated.Contains(MapPtr))
	{
		FAngelscriptEngine::Throw("FJsonObject is being modified during for loop iteration");
		return false;
	}

	return true;
}
#endif

static FString ValueTypeToString(EJson T)
{
	switch (T)
	{
	case EJson::None:
		return TEXT("None");
	case EJson::Null:
		return TEXT("Null");
	case EJson::String:
		return TEXT("String");
	case EJson::Number:
		return TEXT("Number");
	case EJson::Boolean:
		return TEXT("Boolean");
	case EJson::Array:
		return TEXT("Array");
	case EJson::Object:
		return TEXT("Object");
	default:
		return TEXT("<Invalid Type>");
	}
}

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

	bool TryGetArray(struct FJsonValueArrayContainer& OutArray) const;

	bool TryGetObject(struct FJsonObjectContainer& Object) const;

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
		auto Message = FString::Printf(TEXT("Json Value of type '%s' used as a '%s'."), *ValueTypeToString(FieldType), *InType);
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
	}

#if AS_ITERATOR_DEBUGGING
	bool CheckIteratorForNewField(const FString& FieldName) const
	{
		if (JsonObject.IsValid())
		{
			// adding a new field during iteration might reallocate/rehash the map and cause issues
			if (!JsonObject->HasField(FieldName))
				return CheckObjectIteratorDebug(&JsonObject->Values);
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
			if (!CheckObjectIteratorDebug(&JsonObject->Values))
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
			if (!CheckObjectIteratorDebug(&JsonObject->Values))
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
			if (!CheckObjectIteratorDebug(&JsonObject->Values))
				return;
#endif
			JsonObject->Values.Empty();
		}
	}

	// TODO : expand FJsonObjectContainer to support every field type create/access method
};

void FJsonValueArrayContainer::AddObject(const FJsonObjectContainer& ObjectValue)
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

bool FJsonValueContainer::TryGetArray(FJsonValueArrayContainer& OutArray) const
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

bool FJsonValueContainer::TryGetObject(FJsonObjectContainer& Object) const
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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_JsonClasses(FAngelscriptBinds::EOrder::Late, []
	{
		auto EJsonValueType = FAngelscriptBinds::Enum("EJsonType");
		EJsonValueType["None"] = EJson::None;
		EJsonValueType["Null"] = EJson::Null;
		EJsonValueType["String"] = EJson::String;
		EJsonValueType["Number"] = EJson::Number;
		EJsonValueType["Boolean"] = EJson::Boolean;
		EJsonValueType["Array"] = EJson::Array;
		EJsonValueType["Object"] = EJson::Object;

		auto JsonValue = FAngelscriptBinds::ValueClass<FJsonValueContainer>("FJsonValue", {});
		JsonValue.Constructor("void f()", [](FJsonValueContainer* Address)
			{
				new(Address) FJsonValueContainer();
			});
		JsonValue.Destructor("void f()", [](FJsonValueContainer& Obj)
			{
				Obj.~FJsonValueContainer();
			});
		JsonValue.Method("EJsonType GetType() const", &FJsonValueContainer::GetType);
		JsonValue.Method("bool TryGetNumber(float64& OutNumber) const", TMemFunPtrType<true, FJsonValueContainer, bool(double&)>::Type(&FJsonValueContainer::TryGetNumber));
		JsonValue.Method("bool TryGetNumber(float32& OutNumber) const", TMemFunPtrType<true, FJsonValueContainer, bool(float&)>::Type(&FJsonValueContainer::TryGetNumber));
		JsonValue.Method("bool TryGetNumber(int32& OutNumber) const", TMemFunPtrType<true, FJsonValueContainer, bool(int32&)>::Type(&FJsonValueContainer::TryGetNumber));
		JsonValue.Method("bool TryGetNumber(int64& OutNumber) const", TMemFunPtrType<true, FJsonValueContainer, bool(int64&)>::Type(&FJsonValueContainer::TryGetNumber));
		JsonValue.Method("bool TryGetString(FString& OutString) const", &FJsonValueContainer::TryGetString);
		JsonValue.Method("bool TryGetBool(bool& OutBool) const", &FJsonValueContainer::TryGetBool);
		JsonValue.Method("bool IsNull() const", &FJsonValueContainer::IsNull);

		auto JsonValueArray = FAngelscriptBinds::ValueClass<FJsonValueArrayContainer>("FJsonArray", {});
		JsonValueArray.Constructor("void f()", [](FJsonValueArrayContainer* Address)
			{
				new(Address) FJsonValueArrayContainer();
			});
		JsonValueArray.Destructor("void f()", [](FJsonValueArrayContainer& Obj)
			{
				Obj.~FJsonValueArrayContainer();
			});
		JsonValueArray.Method("void Empty()", &FJsonValueArrayContainer::Empty);
		JsonValueArray.Method("void AddString(const FString& Str)", &FJsonValueArrayContainer::AddString);
		JsonValueArray.Method("void AddNumber(int32 I)", &FJsonValueArrayContainer::AddNumberInt);
		JsonValueArray.Method("void AddNumber(float64 D)", &FJsonValueArrayContainer::AddNumberDouble);
		JsonValueArray.Method("int32 Num() const", &FJsonValueArrayContainer::Num);
		JsonValueArray.Method("FJsonValue GetValueAt(int32 Index) const", &FJsonValueArrayContainer::GetValueAt);

		auto JsonObject = FAngelscriptBinds::ValueClass<FJsonObjectContainer>("FJsonObject", {});
		JsonObject.Constructor("void f(FJsonObject InObject)", [](FJsonObjectContainer* Address, const FJsonObjectContainer& InObject)
			{
				new(Address) FJsonObjectContainer();
				Address->JsonObject = InObject.JsonObject;
			});
		JsonObject.Constructor("void f()", [](FJsonObjectContainer* Address)
			{
				new(Address) FJsonObjectContainer();
				Address->JsonObject = MakeShared<FJsonObject>();
			});
		JsonObject.Destructor("void f()", [](FJsonObjectContainer& Obj)
			{
				Obj.~FJsonObjectContainer();
			});
		JsonObject.Method("bool IsValid() const", &FJsonObjectContainer::IsValid);
		JsonObject.Method("bool HasField(const FString& FieldName) const", &FJsonObjectContainer::HasField);
		JsonObject.Method("void RemoveField(const FString& FieldName)", &FJsonObjectContainer::RemoveField);
		JsonObject.Method("void RemoveAllFields()", &FJsonObjectContainer::RemoveAllFields);

		JsonObject.Method("FString GetStringField(const FString& FieldName) const", &FJsonObjectContainer::GetStringField);
		JsonObject.Method("float64 GetNumberField(const FString& FieldName) const", &FJsonObjectContainer::GetNumberField);
		JsonObject.Method("bool GetBoolField(const FString& FieldName) const", &FJsonObjectContainer::GetBoolField);
		JsonObject.Method("FJsonObject GetObjectField(const FString& FieldName) const", &FJsonObjectContainer::GetObjectField);
		JsonObject.Method("FJsonArray GetArrayField(const FString& FieldName) const", &FJsonObjectContainer::GetArrayField);

		JsonObject.Method("void SetStringField(const FString& FieldName, const FString& StringValue)", &FJsonObjectContainer::SetStringField);
		JsonObject.Method("void SetNumberField(const FString& FieldName, float64 Number)", &FJsonObjectContainer::SetNumberField);
		JsonObject.Method("void SetBoolField(const FString& FieldName, bool InValue)", &FJsonObjectContainer::SetBoolField);
		JsonObject.Method("void SetObjectField(const FString& FieldName, const FJsonObject& InObject)", &FJsonObjectContainer::SetObjectField);
		JsonObject.Method("void SetArrayField(const FString& FieldName, const FJsonArray& InArray)", &FJsonObjectContainer::SetArrayField);

		JsonObject.Method("bool TryGetObjectField(const FString& FieldName, FJsonObject& OutObject) const",
			[](const FJsonObjectContainer& Object, const FString& FieldName, FJsonObjectContainer& OutObject)
			{
				const TSharedPtr<FJsonObject> *ResultPtr;
				if (!Object.JsonObject.IsValid() || !Object.JsonObject->TryGetObjectField(FieldName, ResultPtr) || !ResultPtr)
				{
					return false;
				}
				OutObject.JsonObject = *ResultPtr;
				return true;
			}
		);
		JsonObject.Method("FJsonObject CreateObjectField(const FString& FieldName)", &FJsonObjectContainer::CreateObjectField);

		JsonObject.Method("bool TryGetArrayField(const FString& FieldName, FJsonArray& OutArray) const",
			[](const FJsonObjectContainer& Object, const FString&FieldName, FJsonValueArrayContainer& OutArray)
			{
				const TArray<TSharedPtr<FJsonValue>>* Array;
				if (!Object.JsonObject.IsValid() || !Object.JsonObject->TryGetArrayField(FieldName, Array))
					return false;
				OutArray.Array = *Array;
				return true;
			});

		JsonObject.Method("bool LoadFromString(const FString& JsonStr)",
			[](FJsonObjectContainer* Object, const FString& JsonStr) -> bool
			{
				if (Object->CheckValidObject())
				{
#if AS_ITERATOR_DEBUGGING
					if (!CheckObjectIteratorDebug(&Object->JsonObject->Values))
						return false;
#endif
					auto JsonReader = TJsonReaderFactory<>::Create(JsonStr);
					return FJsonSerializer::Deserialize(JsonReader, Object->JsonObject);
				}
				return false;
			});
		JsonObject.Method("FString SaveToString(bool bPrettyPrint = true) const",
			[](FJsonObjectContainer* Object, bool bPrettyPrint) -> FString
			{
				if (Object->CheckValidObject())
				{
					FString Result;
					if (bPrettyPrint)
					{
						auto JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Result, 0);
						if (bool Success = FJsonSerializer::Serialize(Object->JsonObject.ToSharedRef(), JsonWriter, true))
							return Result;
					}
					else
					{
						auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result, 0);
						if (bool Success = FJsonSerializer::Serialize(Object->JsonObject.ToSharedRef(), JsonWriter, true))
							return Result;
					}
				}
				return TEXT("");
			});

		auto JsonObjectFieldIterator = FAngelscriptBinds::ValueClass<FJsonObjectFieldIterator>("FJsonObjectFieldIterator", {});

		JsonObjectFieldIterator.Destructor("void f()", [](FJsonObjectFieldIterator& Iterator)
			{
#if AS_ITERATOR_DEBUGGING
				if (Iterator.MapPtr != nullptr)
				{
					int RemovedCount = GFieldMapsBeingIterated.RemoveSingleSwap(Iterator.MapPtr);
					check(RemovedCount != 0);
				}
#endif
				Iterator.~FJsonObjectFieldIterator();
			});

		JsonObjectFieldIterator.Method("FString GetFieldName() const", &FJsonObjectFieldIterator::GetFieldName);
		JsonObjectFieldIterator.Method("EJsonType GetType() const", &FJsonObjectFieldIterator::GetType);
		JsonObjectFieldIterator.Method("FJsonValue GetValue() const", &FJsonObjectFieldIterator::GetValue);
		JsonObjectFieldIterator.Property("bool CanProceed", &FJsonObjectFieldIterator::bCanProceed);
		JsonObjectFieldIterator.Method("FJsonObjectFieldIterator& Proceed()", [](FJsonObjectFieldIterator& Iterator) -> FJsonObjectFieldIterator&
			{
				Iterator.bValidValue = (bool)Iterator.Iterator;
				if (!Iterator.bValidValue)
				{
					FAngelscriptEngine::Throw("Iterator out of bounds.");
					return Iterator;
				}
				Iterator.CurrentValue = *Iterator.Iterator;
				++Iterator.Iterator;
				Iterator.bCanProceed = (bool)Iterator.Iterator;
				return Iterator;
			});

		JsonObject.Method("FJsonObjectFieldIterator Iterator()", [](FJsonObjectContainer* Object) -> FJsonObjectFieldIterator
			{
				if (Object->CheckValidObject())
				{
#if AS_ITERATOR_DEBUGGING
					GFieldMapsBeingIterated.Add(&Object->JsonObject->Values);
#endif
					return FJsonObjectFieldIterator(Object->JsonObject->Values);
				}
				else
				{
					return FJsonObjectFieldIterator();
				}
			});

		// Binds that depend on arrays and objects for values
		JsonValue.Method("bool TryGetArray(FJsonArray& OutArray) const", &FJsonValueContainer::TryGetArray);
		JsonValue.Method("bool TryGetObject(FJsonObject& Object) const", &FJsonValueContainer::TryGetObject);

		{
			FAngelscriptBinds::FNamespace ns("Json");
			FAngelscriptBinds::BindGlobalFunction("FString ValueTypeToString(EJsonType T)", &ValueTypeToString);

			FAngelscriptBinds::BindGlobalFunction("FJsonObject ParseString(const FString& JsonStr)",
				[](const FString& JsonStr) -> FJsonObjectContainer
				{
					TSharedPtr<FJsonObject> ParsedObject = MakeShared<FJsonObject>();
					auto JsonReader = TJsonReaderFactory<>::Create(JsonStr);
					if (FJsonSerializer::Deserialize(JsonReader, ParsedObject))
						return { ParsedObject };
					else
						return { };
				});
		}
});
