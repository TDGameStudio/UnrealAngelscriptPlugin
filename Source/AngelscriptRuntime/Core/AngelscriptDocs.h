#pragma once
#include "CoreMinimal.h"

#if WITH_EDITOR
#define AS_DOC(FunctionId, Documentation) FAngelscriptDocs::AddUnrealDocumentation(FunctionId, Documentation);
#define SCRIPT_BIND_DOCUMENTATION(Documentation) FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), TEXT(Documentation), TEXT(""), nullptr);
#define SCRIPT_GLOBAL_DOCUMENTATION(Documentation) FAngelscriptDocs::AddDocumentationForGlobalVariable(FAngelscriptBinds::GetPreviousGlobalVariableId(), TEXT(Documentation));
#else
#define AS_DOC(...) 
#define SCRIPT_BIND_DOCUMENTATION(...)
#define SCRIPT_GLOBAL_DOCUMENTATION(...) 
#endif

class asIScriptEngine;

struct ANGELSCRIPTRUNTIME_API FAngelscriptDocs
{
	static void AddUnrealDocumentation(int FunctionId, FStringView Documentation, FStringView Category, UFunction* Function);
	static void AddUnrealDocumentationForType(int TypeId, FStringView Documentation);
	static void AddUnrealDocumentationForProperty(int TypeId, int PropertyOffset, FStringView Documentation);
	static void AddDocumentationForGlobalVariable(int GlobalVariableId, FStringView Documentation);

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
