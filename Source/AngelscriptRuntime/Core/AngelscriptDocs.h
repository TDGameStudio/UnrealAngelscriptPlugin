#pragma once
#include "CoreMinimal.h"

#if WITH_EDITOR
#define AS_DOC(FunctionId, Documentation) FAngelscriptDocs::AddUnrealDocumentation(FunctionId, Documentation);
#else
#define AS_DOC(...) 
#endif

class asIScriptEngine;
struct FAngelscriptEngine;

struct FPassedDoc
{
	FString Tooltip;
	FString Category;
	UFunction* Function = nullptr;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptDocumentationState
{
	TMap<int32, FPassedDoc> UnrealDocumentation;
	TMap<int32, FString> UnrealTypeDocumentation;
	TMap<int32, FString> GlobalVariableDocumentation;
	TMap<TPair<int32, int32>, FString> UnrealPropertyDocumentation;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptDocs
{
	static void AddUnrealDocumentation(FAngelscriptEngine& Engine, int FunctionId, FStringView Documentation, FStringView Category, UFunction* Function);
	static void AddUnrealDocumentation(int FunctionId, FStringView Documentation, FStringView Category, UFunction* Function);
	static void AddUnrealDocumentationForType(FAngelscriptEngine& Engine, int TypeId, FStringView Documentation);
	static void AddUnrealDocumentationForType(int TypeId, FStringView Documentation);
	static void AddUnrealDocumentationForProperty(FAngelscriptEngine& Engine, int TypeId, int PropertyOffset, FStringView Documentation);
	static void AddUnrealDocumentationForProperty(int TypeId, int PropertyOffset, FStringView Documentation);
	static void AddDocumentationForGlobalVariable(FAngelscriptEngine& Engine, int GlobalVariableId, FStringView Documentation);
	static void AddDocumentationForGlobalVariable(int GlobalVariableId, FStringView Documentation);

	static const FString& GetUnrealDocumentation(const FAngelscriptEngine& Engine, int FunctionId);
	static const FString& GetUnrealDocumentation(int FunctionId);
	static const struct FPassedDoc& GetFullUnrealDocumentation(int FunctionId);
	static void DumpDocumentation(asIScriptEngine* Engine);

	static const FString& GetUnrealDocumentationForType(int TypeId);
	static const FString& GetUnrealDocumentationForProperty(int TypeId, int PropertyOffset);
	static const FString& GetDocumentationForGlobalVariable(int GlobalVariableId);

	static int32 GetUnrealDocumentationCount();
	static int32 GetUnrealTypeDocumentationCount();
	static int32 GetGlobalVariableDocumentationCount();
	static int32 GetUnrealPropertyDocumentationCount();

	static UFunction* LookupAngelscriptFunction(int FunctionId);

	static void ResetAllDocumentation();
};
