#include "Bind_Console_Functions.h"

#include "Binds/Bind_Console.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSharedPtr.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	TMap<FString, const void*> GScriptConsoleCommandOwners;
}

FScriptConsoleCommand::FScriptConsoleCommand(const FString& Name, const FString& FunctionName)
{
#if !UE_BUILD_SHIPPING
	auto* Context = FAngelscriptEngine::Get().GetPreviousScriptContext();
	asIScriptFunction* Function = Context->GetFunction(0);
	if (!ensure(Function != nullptr))
	{
		return;
	}

	asIScriptModule* Module = Function->GetModule();
	if (!ensure(Module != nullptr))
	{
		return;
	}

	const FString Declaration = FString::Printf(TEXT("void %s(const TArray<FString>& Args)"), *FunctionName);
	asIScriptFunction* CallFunction = Module->GetFunctionByDecl(TCHAR_TO_ANSI(*Declaration));
	if (CallFunction == nullptr)
	{
		asIScriptFunction* NamedFunction = Module->GetFunctionByName(TCHAR_TO_ANSI(*FunctionName));
		const FString Message = NamedFunction == nullptr
			? FString::Printf(TEXT("Could not find global function '%s' to bind as console command."), *FunctionName)
			: FString::Printf(
				TEXT("Global function for console command must have signature `void %s(TArray<FString> Arguments)`"),
				*FunctionName);
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	TAngelscriptSharedPtr<asIScriptFunction> FunctionPtr = CallFunction;
	if (IConsoleObject* Existing = IConsoleManager::Get().FindConsoleObject(*Name))
	{
		IConsoleManager::Get().UnregisterConsoleObject(Existing);
	}

	CommandName = Name;
	Command = IConsoleManager::Get().RegisterConsoleCommand(
		*Name,
		TEXT(""),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[FunctionPtr](const TArray<FString>& Arguments, UWorld* World)
			{
				if (!FunctionPtr.IsValid())
				{
					return;
				}

				asIScriptModule* FunctionModule = FunctionPtr->GetModule();
				if (FunctionModule == nullptr)
				{
					return;
				}

				FAngelscriptContext ScriptContext(World, FunctionPtr->GetEngine());
				if (!PrepareAngelscriptContextWithLog(
					ScriptContext,
					FunctionPtr.Get(),
					TEXT("FScriptConsoleCommand")))
				{
					return;
				}
				ScriptContext->SetArgAddress(0, const_cast<TArray<FString>*>(&Arguments));
				ScriptContext->Execute();
			}));
	GScriptConsoleCommandOwners.Add(CommandName, this);
#endif
}

FScriptConsoleCommand::~FScriptConsoleCommand()
{
#if !UE_BUILD_SHIPPING
	const void* const* CurrentOwner = GScriptConsoleCommandOwners.Find(CommandName);
	if (CurrentOwner != nullptr && *CurrentOwner == this)
	{
		IConsoleObject* RegisteredCommand = IConsoleManager::Get().FindConsoleObject(*CommandName);
		if (RegisteredCommand == Command)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Command);
		}
		GScriptConsoleCommandOwners.Remove(CommandName);
	}
#endif
}

void FAngelscriptConsoleBinds::ConstructIntVariable(
	void* Address,
	const FString& Name,
	const int32 DefaultValue,
	const FString& Help)
{
	new (Address) FScriptConsoleVariable<int32>(Name, DefaultValue, Help);
}

void FAngelscriptConsoleBinds::ConstructBoolVariable(
	void* Address,
	const FString& Name,
	const bool DefaultValue,
	const FString& Help)
{
	new (Address) FScriptConsoleVariable<bool>(Name, DefaultValue, Help);
}

void FAngelscriptConsoleBinds::ConstructFloatVariable(
	void* Address,
	const FString& Name,
	const float DefaultValue,
	const FString& Help)
{
	new (Address) FScriptConsoleVariable<float>(Name, DefaultValue, Help);
}

void FAngelscriptConsoleBinds::ConstructStringVariable(
	void* Address,
	const FString& Name,
	const FString& DefaultValue,
	const FString& Help)
{
	new (Address) FScriptConsoleVariable<FString>(Name, DefaultValue, Help);
}

void FAngelscriptConsoleBinds::DestructVariable(FScriptConsoleVariable<int32>* Address)
{
	Address->~FScriptConsoleVariable<int32>();
}

void FAngelscriptConsoleBinds::ConstructCommand(
	void* Address,
	const FString& Name,
	const FName& FunctionName)
{
	new (Address) FScriptConsoleCommand(Name, FunctionName.ToString());
}

void FAngelscriptConsoleBinds::DestructCommand(FScriptConsoleCommand* Address)
{
	Address->~FScriptConsoleCommand();
}
