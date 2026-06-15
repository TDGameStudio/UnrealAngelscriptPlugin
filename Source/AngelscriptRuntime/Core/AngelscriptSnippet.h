#pragma once

#include "CoreMinimal.h"

struct FAngelscriptEngine;

enum class EAngelscriptSnippetSourceMode : uint8
{
	Statements,
	FullSource,
};

enum class EAngelscriptSnippetResultCode : uint8
{
	Succeeded,
	DisabledInShipping,
	InvalidRequest,
	PreprocessFailed,
	CompileFailed,
	EntryPointMissing,
	ExecutionException,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSnippetRequest
{
	FString SourceText;
	EAngelscriptSnippetSourceMode SourceMode = EAngelscriptSnippetSourceMode::Statements;
	FString Label;
	UObject* WorldContextObject = nullptr;
	bool bDiscardModuleAfterExecute = true;
	bool bKeepModuleForDebugging = false;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSnippetDiagnostic
{
	FString Section;
	int32 Row = 0;
	int32 Column = 0;
	bool bIsError = false;
	bool bIsInfo = false;
	FString Message;
	int32 UserRow = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSnippetResult
{
	bool bSucceeded = false;
	EAngelscriptSnippetResultCode ResultCode = EAngelscriptSnippetResultCode::InvalidRequest;
	FString VirtualPath;
	FString ModuleName;
	FString EntryPointDeclaration = TEXT("void Main()");
	FString ErrorMessage;
	FString ExceptionMessage;
	FString ExceptionSection;
	int32 ExceptionLine = 0;
	TArray<FAngelscriptSnippetDiagnostic> Diagnostics;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSnippetRunner
{
	static FAngelscriptSnippetResult Execute(FAngelscriptEngine& Engine, const FAngelscriptSnippetRequest& Request);
};
