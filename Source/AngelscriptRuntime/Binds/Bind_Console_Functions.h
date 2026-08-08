#pragma once

#include "CoreMinimal.h"
#include "Binds/Bind_Console.h"

struct IConsoleCommand;

struct FScriptConsoleCommand
{
	IConsoleCommand* Command = nullptr;
	FString CommandName;

	FScriptConsoleCommand(const FString& Name, const FString& FunctionName);
	~FScriptConsoleCommand();
};

struct FAngelscriptConsoleBinds
{
	static void ConstructIntVariable(void* Address, const FString& Name, int32 DefaultValue, const FString& Help);
	static void ConstructBoolVariable(void* Address, const FString& Name, bool DefaultValue, const FString& Help);
	static void ConstructFloatVariable(void* Address, const FString& Name, float DefaultValue, const FString& Help);
	static void ConstructStringVariable(void* Address, const FString& Name, const FString& DefaultValue, const FString& Help);
	static void DestructVariable(FScriptConsoleVariable<int32>* Address);
	static void ConstructCommand(void* Address, const FString& Name, const FName& FunctionName);
	static void DestructCommand(FScriptConsoleCommand* Address);
};
