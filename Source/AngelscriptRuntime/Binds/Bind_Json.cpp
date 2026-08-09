#include "Bind_Json.h"

#include "AngelscriptBinds.h"

/**
 * Json binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType Value;                                                                                                             | Declares the JSON value-kind enum.                                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType::None;                                                                                                             | No valid JSON value or type information.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType::Null;                                                                                                             | Explicit JSON null value.                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType::String;                                                                                                           | JSON string value.                                                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType::Number;                                                                                                           | JSON numeric value.                                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType::Boolean;                                                                                                          | JSON true or false value.                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType::Array;                                                                                                            | JSON array value.                                                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType::Object;                                                                                                           | JSON object value.                                                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonValue Value;                                                                                                            | Declares the FJsonValue value type.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonArray Value;                                                                                                            | Declares the FJsonArray value type.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObject Value;                                                                                                           | Declares the FJsonObject value type.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObjectFieldIterator Value;                                                                                              | Declares the FJsonObjectFieldIterator value type.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonValue Value();                                                                                                          | Constructs an empty JSON value wrapper.                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType FJsonValue.GetType() const;                                                                                        | Returns the wrapped JSON value's type, or None when empty.                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.TryGetNumber(float64& OutNumber) const;                                                                      | Reads a JSON number as float64; returns false when conversion fails.                                                 |
 * |                                                                                                                              | @param OutNumber Receives the float64 result on success.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.TryGetNumber(float32& OutNumber) const;                                                                      | Reads a JSON number as float32; returns false when conversion fails.                                                 |
 * |                                                                                                                              | @param OutNumber Receives the float32 result on success.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.TryGetNumber(int32& OutNumber) const;                                                                        | Reads a JSON number as int32; returns false when conversion fails.                                                   |
 * |                                                                                                                              | @param OutNumber Receives the int32 result on success.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.TryGetNumber(int64& OutNumber) const;                                                                        | Reads a JSON number as int64; returns false when conversion fails.                                                   |
 * |                                                                                                                              | @param OutNumber Receives the int64 result on success.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.TryGetString(FString& OutString) const;                                                                      | Reads a string value and reports whether the JSON type is compatible.                                                |
 * |                                                                                                                              | @param OutString Receives the string on success.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.TryGetBool(bool& OutBool) const;                                                                             | Reads a Boolean value and reports whether the JSON type is compatible.                                               |
 * |                                                                                                                              | @param OutBool Receives the Boolean on success.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.IsNull() const;                                                                                              | Reports whether the wrapper contains an explicit JSON null value.                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.TryGetArray(FJsonArray& OutArray) const;                                                                     | Reads an array value and reports whether the JSON type is compatible.                                                |
 * |                                                                                                                              | @param OutArray Receives the array on success.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonValue.TryGetObject(FJsonObject& Object) const;                                                                     | Reads an object value and reports whether the JSON type is compatible.                                               |
 * |                                                                                                                              | @param Object Receives the object on success.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonArray Value();                                                                                                          | Constructs an empty JSON array.                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonArray.Empty();                                                                                                     | Removes every element from the array.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonArray.AddString(const FString& Str);                                                                               | Appends Str as a JSON string value.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonArray.AddNumber(int32 I);                                                                                          | Appends I as a JSON number.                                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonArray.AddNumber(float64 D);                                                                                        | Appends D as a JSON number.                                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FJsonArray.Num() const;                                                                                                | Returns the number of array elements.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonValue FJsonArray.GetValueAt(int32 Index) const;                                                                         | Returns the wrapped value at Index, or an empty wrapper when out of bounds.                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObject Value(const FJsonObject& InObject);                                                                              | Constructs a wrapper sharing InObject's underlying JSON object.                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObject Value();                                                                                                         | Constructs a new empty JSON object.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObject.IsValid() const;                                                                                            | Reports whether this wrapper references a valid JSON object.                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObject.HasField(const FString& FieldName) const;                                                                   | Reports whether the object contains FieldName.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonObject.RemoveField(const FString& FieldName);                                                                      | Removes FieldName when present.                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonObject.RemoveAllFields();                                                                                          | Removes every field from the object.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FJsonObject.GetStringField(const FString& FieldName) const;                                                          | Returns the named string field.                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FJsonObject.GetNumberField(const FString& FieldName) const;                                                          | Returns the named numeric field.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObject.GetBoolField(const FString& FieldName) const;                                                               | Returns the named Boolean field.                                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObject FJsonObject.GetObjectField(const FString& FieldName) const;                                                      | Returns a wrapper for the named object field.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonArray FJsonObject.GetArrayField(const FString& FieldName) const;                                                        | Returns a wrapper for the named array field.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonObject.SetStringField(const FString& FieldName, const FString& StringValue);                                       | Creates or replaces FieldName with a JSON string.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonObject.SetNumberField(const FString& FieldName, float64 Number);                                                   | Creates or replaces FieldName with a JSON number.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonObject.SetBoolField(const FString& FieldName, bool InValue);                                                       | Creates or replaces FieldName with a JSON Boolean.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonObject.SetObjectField(const FString& FieldName, const FJsonObject& InObject);                                      | Stores an object value under the named field.                                                                        |
 * |                                                                                                                              | @param FieldName Field to create or replace.                                                                         |
 * |                                                                                                                              | @param InObject Object value to store.                                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FJsonObject.SetArrayField(const FString& FieldName, const FJsonArray& InArray);                                         | Stores an array value under the named field.                                                                         |
 * |                                                                                                                              | @param FieldName Field to create or replace.                                                                         |
 * |                                                                                                                              | @param InArray Array value to store.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObject.TryGetObjectField(const FString& FieldName, FJsonObject& OutObject) const;                                  | Reads a named object field; returns false when absent or of another type.                                            |
 * |                                                                                                                              | @param FieldName Field to inspect.                                                                                   |
 * |                                                                                                                              | @param OutObject Receives the object on success.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObject FJsonObject.CreateObjectField(const FString& FieldName);                                                         | Creates an empty object field and returns its wrapper.                                                               |
 * |                                                                                                                              | @param FieldName Field to create or replace.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObject.TryGetArrayField(const FString& FieldName, FJsonArray& OutArray) const;                                     | Reads a named array field; returns false when absent or of another type.                                             |
 * |                                                                                                                              | @param FieldName Field to inspect.                                                                                   |
 * |                                                                                                                              | @param OutArray Receives the array on success.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObject.LoadFromString(const FString& JsonStr);                                                                     | Deserializes JSON text into this object and reports success.                                                         |
 * |                                                                                                                              | @param JsonStr JSON object text to parse.                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FJsonObject.SaveToString(bool bPrettyPrint = true) const;                                                            | Serializes this object, returning an empty string if serialization fails.                                            |
 * |                                                                                                                              | @param bPrettyPrint Whether to emit indented rather than condensed JSON.                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObjectFieldIterator FJsonObject.Iterator();                                                                             | Creates an iterator over this object's fields.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObjectFieldIterator.CanProceed;                                                                                    | Indicates whether another field can be consumed.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FJsonObjectFieldIterator.GetFieldName() const;                                                                       | Returns the current field's name after Proceed.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EJsonType FJsonObjectFieldIterator.GetType() const;                                                                          | Returns the current field value's JSON type after Proceed.                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonValue FJsonObjectFieldIterator.GetValue() const;                                                                        | Returns the current field's wrapped value after Proceed.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObjectFieldIterator& FJsonObjectFieldIterator.Proceed();                                                                | Consumes the next field, makes it current, and advances the iterator.                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString Json::ValueTypeToString(EJsonType T);                                                                                | Returns the stable display name for a JSON value type.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FJsonObject Json::ParseString(const FString& JsonStr);                                                                       | Parses JSON object text; returns an invalid wrapper when parsing fails.                                              |
 * |                                                                                                                              | @param JsonStr JSON object text to parse.                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */



AS_FORCE_LINK const FAngelscriptBind Bind_Json_TypeDeclarations(
	TEXT("Json.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		auto EJsonValueType = Binds.EnumForTarget("EJsonType");
		EJsonValueType["None"] = EJson::None;
		EJsonValueType["Null"] = EJson::Null;
		EJsonValueType["String"] = EJson::String;
		EJsonValueType["Number"] = EJson::Number;
		EJsonValueType["Boolean"] = EJson::Boolean;
		EJsonValueType["Array"] = EJson::Array;
		EJsonValueType["Object"] = EJson::Object;

		Binds.ValueClassForTarget<FJsonValueContainer>("FJsonValue", {});
		Binds.ValueClassForTarget<FJsonValueArrayContainer>("FJsonArray", {});
		Binds.ValueClassForTarget<FJsonObjectContainer>("FJsonObject", {});
		Binds.ValueClassForTarget<FJsonObjectFieldIterator>("FJsonObjectFieldIterator", {});
	});

AS_FORCE_LINK const FAngelscriptBind Bind_Json(
	TEXT("Json.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto JsonValue = Binds.ExistingClassForTarget("FJsonValue");
		JsonValue.Constructor("void f()", &FAngelscriptJsonBinds::ConstructValue);
		JsonValue.Destructor("void f()", &FAngelscriptJsonBinds::DestructValue);
		JsonValue.Method("EJsonType GetType() const", &FJsonValueContainer::GetType);
		JsonValue.Method("bool TryGetNumber(float64& OutNumber) const", TMemFunPtrType<true, FJsonValueContainer, bool(double&)>::Type(&FJsonValueContainer::TryGetNumber));
		JsonValue.Method("bool TryGetNumber(float32& OutNumber) const", TMemFunPtrType<true, FJsonValueContainer, bool(float&)>::Type(&FJsonValueContainer::TryGetNumber));
		JsonValue.Method("bool TryGetNumber(int32& OutNumber) const", TMemFunPtrType<true, FJsonValueContainer, bool(int32&)>::Type(&FJsonValueContainer::TryGetNumber));
		JsonValue.Method("bool TryGetNumber(int64& OutNumber) const", TMemFunPtrType<true, FJsonValueContainer, bool(int64&)>::Type(&FJsonValueContainer::TryGetNumber));
		JsonValue.Method("bool TryGetString(FString& OutString) const", &FJsonValueContainer::TryGetString);
		JsonValue.Method("bool TryGetBool(bool& OutBool) const", &FJsonValueContainer::TryGetBool);
		JsonValue.Method("bool IsNull() const", &FJsonValueContainer::IsNull);

		auto JsonValueArray = Binds.ExistingClassForTarget("FJsonArray");
		JsonValueArray.Constructor("void f()", &FAngelscriptJsonBinds::ConstructArray);
		JsonValueArray.Destructor("void f()", &FAngelscriptJsonBinds::DestructArray);
		JsonValueArray.Method("void Empty()", &FJsonValueArrayContainer::Empty);
		JsonValueArray.Method("void AddString(const FString& Str)", &FJsonValueArrayContainer::AddString);
		JsonValueArray.Method("void AddNumber(int32 I)", &FJsonValueArrayContainer::AddNumberInt);
		JsonValueArray.Method("void AddNumber(float64 D)", &FJsonValueArrayContainer::AddNumberDouble);
		JsonValueArray.Method("int32 Num() const", &FJsonValueArrayContainer::Num);
		JsonValueArray.Method("FJsonValue GetValueAt(int32 Index) const", &FJsonValueArrayContainer::GetValueAt);

		auto JsonObject = Binds.ExistingClassForTarget("FJsonObject");
		JsonObject.Constructor("void f(const FJsonObject& InObject)", &FAngelscriptJsonBinds::ConstructObjectCopy);
		JsonObject.Constructor("void f()", &FAngelscriptJsonBinds::ConstructObject);
		JsonObject.Destructor("void f()", &FAngelscriptJsonBinds::DestructObject);
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
		JsonObject.Method("bool TryGetObjectField(const FString& FieldName, FJsonObject& OutObject) const", &FAngelscriptJsonBinds::TryGetObjectField);
		JsonObject.Method("FJsonObject CreateObjectField(const FString& FieldName)", &FJsonObjectContainer::CreateObjectField);
		JsonObject.Method("bool TryGetArrayField(const FString& FieldName, FJsonArray& OutArray) const", &FAngelscriptJsonBinds::TryGetArrayField);
		JsonObject.Method("bool LoadFromString(const FString& JsonStr)", &FAngelscriptJsonBinds::LoadFromString);
		JsonObject.Method("FString SaveToString(bool bPrettyPrint = true) const", &FAngelscriptJsonBinds::SaveToString);

		auto JsonObjectFieldIterator = Binds.ExistingClassForTarget("FJsonObjectFieldIterator");
		JsonObjectFieldIterator.Destructor("void f()", &FAngelscriptJsonBinds::DestructIterator);
		JsonObjectFieldIterator.Method("FString GetFieldName() const", &FJsonObjectFieldIterator::GetFieldName);
		JsonObjectFieldIterator.Method("EJsonType GetType() const", &FJsonObjectFieldIterator::GetType);
		JsonObjectFieldIterator.Method("FJsonValue GetValue() const", &FJsonObjectFieldIterator::GetValue);
		JsonObjectFieldIterator.Property("bool CanProceed", &FJsonObjectFieldIterator::bCanProceed);
		JsonObjectFieldIterator.Method("FJsonObjectFieldIterator& Proceed()", &FAngelscriptJsonBinds::Proceed);
		JsonObject.Method("FJsonObjectFieldIterator Iterator()", &FAngelscriptJsonBinds::Iterator);

		// Binds that depend on arrays and objects for values
		JsonValue.Method("bool TryGetArray(FJsonArray& OutArray) const", &FJsonValueContainer::TryGetArray);
		JsonValue.Method("bool TryGetObject(FJsonObject& Object) const", &FJsonValueContainer::TryGetObject);

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "Json");
			Binds.BindGlobalFunctionForTarget("FString ValueTypeToString(EJsonType T)", &FAngelscriptJsonBinds::ValueTypeToString);
			Binds.BindGlobalFunctionForTarget("FJsonObject ParseString(const FString& JsonStr)", &FAngelscriptJsonBinds::ParseString);
		}
	});
