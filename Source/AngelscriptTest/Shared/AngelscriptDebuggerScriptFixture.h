#pragma once

#include "CoreMinimal.h"

#include "Shared/AngelscriptTestEngineHelper.h"

struct FAngelscriptDebuggerScriptFixture
{
	static FAngelscriptDebuggerScriptFixture CreateBreakpointFixture();
	static FAngelscriptDebuggerScriptFixture CreateNamedBreakpointFixture(FName ModuleName, const FString& Filename, int32 StoredValue);
	static FAngelscriptDebuggerScriptFixture CreateSteppingFixture();
	static FAngelscriptDebuggerScriptFixture CreatePauseFixture();
	static FAngelscriptDebuggerScriptFixture CreateCallstackFixture();
	static FAngelscriptDebuggerScriptFixture CreateBindingFixture();
	static FAngelscriptDebuggerScriptFixture CreateBlueprintFrameFixture();
	bool Compile(FAngelscriptEngine& Engine) const;
	int32 GetLine(FName Marker) const;
	FString GetEvalPath(FName Marker) const;
	UClass* FindGeneratedClass(FAngelscriptEngine& Engine) const;
	UFunction* FindGeneratedFunction(FAngelscriptEngine& Engine, FName FunctionName) const;

	FName ModuleName;
	FName GeneratedClassName;
	FName EntryFunctionName;
	FString EntryFunctionDeclaration;
	FString Filename;
	FString ScriptSource;
	TMap<FName, int32> LineMarkers;
	TMap<FName, FString> EvalPaths;
	bool bUseAnnotatedCompilation = true;
};

