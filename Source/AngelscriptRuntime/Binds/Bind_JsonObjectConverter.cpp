#include "AngelscriptBinds.h"

#include "Bind_JsonObjectConverter_Functions.h"

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
