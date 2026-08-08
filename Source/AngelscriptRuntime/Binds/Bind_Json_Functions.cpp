#include "Bind_Json_Functions.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if AS_ITERATOR_DEBUGGING
thread_local static TArray<void*, TInlineAllocator<16>> GFieldMapsBeingIterated;

bool FAngelscriptJsonBinds::CheckObjectIteratorDebug(void* MapPtr)
{
	if (GFieldMapsBeingIterated.Contains(MapPtr))
	{
		FAngelscriptEngine::Throw("FJsonObject is being modified during for loop iteration");
		return false;
	}

	return true;
}
#endif

FString FAngelscriptJsonBinds::ValueTypeToString(EJson Type)
{
	switch (Type)
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

void FAngelscriptJsonBinds::ConstructValue(FJsonValueContainer* Address)
{
	new (Address) FJsonValueContainer();
}

void FAngelscriptJsonBinds::DestructValue(FJsonValueContainer& Object)
{
	Object.~FJsonValueContainer();
}

void FAngelscriptJsonBinds::ConstructArray(FJsonValueArrayContainer* Address)
{
	new (Address) FJsonValueArrayContainer();
}

void FAngelscriptJsonBinds::DestructArray(FJsonValueArrayContainer& Object)
{
	Object.~FJsonValueArrayContainer();
}

void FAngelscriptJsonBinds::ConstructObjectCopy(FJsonObjectContainer* Address, const FJsonObjectContainer& InObject)
{
	new (Address) FJsonObjectContainer();
	Address->JsonObject = InObject.JsonObject;
}

void FAngelscriptJsonBinds::ConstructObject(FJsonObjectContainer* Address)
{
	new (Address) FJsonObjectContainer();
	Address->JsonObject = MakeShared<FJsonObject>();
}

void FAngelscriptJsonBinds::DestructObject(FJsonObjectContainer& Object)
{
	Object.~FJsonObjectContainer();
}

bool FAngelscriptJsonBinds::TryGetObjectField(const FJsonObjectContainer& Object, const FString& FieldName, FJsonObjectContainer& OutObject)
{
	const TSharedPtr<FJsonObject>* ResultPtr;
	if (!Object.JsonObject.IsValid() || !Object.JsonObject->TryGetObjectField(FieldName, ResultPtr) || !ResultPtr)
	{
		return false;
	}
	OutObject.JsonObject = *ResultPtr;
	return true;
}

bool FAngelscriptJsonBinds::TryGetArrayField(const FJsonObjectContainer& Object, const FString& FieldName, FJsonValueArrayContainer& OutArray)
{
	const TArray<TSharedPtr<FJsonValue>>* Array;
	if (!Object.JsonObject.IsValid() || !Object.JsonObject->TryGetArrayField(FieldName, Array))
		return false;
	OutArray.Array = *Array;
	return true;
}

bool FAngelscriptJsonBinds::LoadFromString(FJsonObjectContainer* Object, const FString& JsonString)
{
	if (Object->CheckValidObject())
	{
#if AS_ITERATOR_DEBUGGING
		if (!CheckObjectIteratorDebug(&Object->JsonObject->Values))
			return false;
#endif
		auto JsonReader = TJsonReaderFactory<>::Create(JsonString);
		return FJsonSerializer::Deserialize(JsonReader, Object->JsonObject);
	}
	return false;
}

FString FAngelscriptJsonBinds::SaveToString(FJsonObjectContainer* Object, bool bPrettyPrint)
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
}

void FAngelscriptJsonBinds::DestructIterator(FJsonObjectFieldIterator& Iterator)
{
#if AS_ITERATOR_DEBUGGING
	if (Iterator.MapPtr != nullptr)
	{
		int RemovedCount = GFieldMapsBeingIterated.RemoveSingleSwap(Iterator.MapPtr);
		check(RemovedCount != 0);
	}
#endif
	Iterator.~FJsonObjectFieldIterator();
}

FJsonObjectFieldIterator& FAngelscriptJsonBinds::Proceed(FJsonObjectFieldIterator& Iterator)
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
}

FJsonObjectFieldIterator FAngelscriptJsonBinds::Iterator(FJsonObjectContainer* Object)
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
}

FJsonObjectContainer FAngelscriptJsonBinds::ParseString(const FString& JsonString)
{
	TSharedPtr<FJsonObject> ParsedObject = MakeShared<FJsonObject>();
	auto JsonReader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(JsonReader, ParsedObject))
		return { ParsedObject };
	else
		return {};
}
