#include "Bind_JsonObjectConverter.h"

#include "AngelscriptBinds.h"

/**
 * FJsonObjectConverter wildcard struct serialization helpers.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObjectConverter::UStructToJsonObjectString(const ?&in MaybeStruct,                         | Serializes a struct value to a new JSON object string.                                                           |
 * |     FString&out Result,                                                                              | @param MaybeStruct Struct value and runtime type to serialize.                                                   |
 * |     int CheckFlags = 0,                                                                              | @param Result Receives the JSON text.                                                                            |
 * |     int SkipFlags = 0,                                                                               | @param CheckFlags Property flags that must be present.                                                           |
 * |     int Indent = 0,                                                                                  | @param SkipFlags Property flags to omit.                                                                         |
 * |     bool PrettyPrint = true);                                                                        | @param Indent Initial indentation depth.                                                                         |
 * |                                                                                                      | @param PrettyPrint Enables formatted output.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObjectConverter::AppendUStructToJsonObjectString(const ?&in MaybeStruct,                   | Appends a serialized struct to an existing JSON string.                                                          |
 * |     FString& InOutString,                                                                            | @param MaybeStruct Struct value and runtime type to serialize.                                                   |
 * |     int CheckFlags = 0,                                                                              | @param InOutString Existing text to extend.                                                                      |
 * |     int SkipFlags = 0,                                                                               | @param CheckFlags Property flags that must be present.                                                           |
 * |     int Indent = 0,                                                                                  | @param SkipFlags Property flags to omit.                                                                         |
 * |     bool PrettyPrint = true);                                                                        | @param Indent Initial indentation depth.                                                                         |
 * |                                                                                                      | @param PrettyPrint Enables formatted output.                                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool FJsonObjectConverter::JsonObjectStringToUStruct(const FString&in JsonString,                    | Deserializes JSON into a concrete struct output.                                                                 |
 * |     ?&out MaybeStruct,                                                                               | @param JsonString JSON object text.                                                                              |
 * |     int CheckFlags = 0,                                                                              | @param MaybeStruct Destination value whose type selects the target struct.                                       |
 * |     int SkipFlags = 0);                                                                              | @param CheckFlags Property flags that must be present.                                                           |
 * |                                                                                                      | @param SkipFlags Property flags to omit.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindJsonObjectConverterFunctions(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FJsonObjectConverter");

		const FString StructToStringSignature =
			"bool UStructToJsonObjectString(const ?&in MaybeStruct, "
			"FString &out Result, "
			"int CheckFlags = 0, "
			"int SkipFlags = 0, "
			"int Indent = 0, "
			"bool PrettyPrint = true)";
		Binds.BindGlobalFunctionForTarget(StructToStringSignature, &FAngelscriptJsonObjectConverterBinds::UStructToJsonObjectString);

		const FString AppendStructToStringSignature =
			"bool AppendUStructToJsonObjectString(const ?&in MaybeStruct, "
			"FString& InOutString, "
			"int CheckFlags = 0, "
			"int SkipFlags = 0, "
			"int Indent = 0, "
			"bool PrettyPrint = true)";
		Binds.BindGlobalFunctionForTarget(AppendStructToStringSignature, &FAngelscriptJsonObjectConverterBinds::AppendUStructToJsonObjectString);

		const FString StringToStructSignature =
			"bool JsonObjectStringToUStruct(const FString &in JsonString, "
			"?&out MaybeStruct, "
			"int CheckFlags = 0, "
			"int SkipFlags = 0)";
		Binds.BindGlobalFunctionForTarget(StringToStructSignature, &FAngelscriptJsonObjectConverterBinds::JsonObjectStringToUStruct);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_JsonObjectConverter(
	TEXT("JsonObjectConverter.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindJsonObjectConverterFunctions);
