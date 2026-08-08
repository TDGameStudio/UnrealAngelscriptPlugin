#include "AngelscriptBinds.h"

#include "Bind_Json_Functions.h"

static void BindJsonTypeDeclarations(FAngelscriptBinds& Binds)
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
}

static void BindJsonManual(FAngelscriptBinds& Binds)
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
}

AS_FORCE_LINK const FAngelscriptBind Bind_Json_TypeDeclarations(
	TEXT("Json.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindJsonTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_Json(
	TEXT("Json.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindJsonManual);
