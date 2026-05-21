#include "Binds/Bind_Console.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

#include "AngelscriptEngine.h"
#include "AngelscriptBinds.h"
#include "AngelscriptSharedPtr.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_ConsoleVariables((int32)FAngelscriptBinds::EOrder::Late - 1, []
{
	FBindFlags Flags;
	auto FConsoleVariable_ = FAngelscriptBinds::ValueClass<FScriptConsoleVariable<int32>>("FConsoleVariable", Flags);

	FConsoleVariable_.Constructor("void f(const FString& Name, int DefaultValue, const FString& Help = \"\")",
	[](void* Memory, const FString& Name, int32 DefaultValue, const FString& Help)
	{
		new(Memory) FScriptConsoleVariable<int32>(Name, DefaultValue, Help);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	FConsoleVariable_.Constructor("void f(const FString& Name, bool DefaultValue, const FString& Help = \"\")",
	[](void* Memory, const FString& Name, bool DefaultValue, const FString& Help)
	{
		new(Memory) FScriptConsoleVariable<bool>(Name, DefaultValue, Help);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	FConsoleVariable_.Constructor("void f(const FString& Name, float32 DefaultValue, const FString& Help = \"\")",
	[](void* Memory, const FString& Name, float DefaultValue, const FString& Help)
	{
		new(Memory) FScriptConsoleVariable<float>(Name, DefaultValue, Help);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	FConsoleVariable_.Constructor("void f(const FString& Name, const FString& DefaultValue, const FString& Help = \"\")",
	[](void* Memory, const FString& Name, const FString& DefaultValue, const FString& Help)
	{
		new(Memory) FScriptConsoleVariable<FString>(Name, DefaultValue, Help);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	FConsoleVariable_.Destructor("void f()",
	[](FScriptConsoleVariable<int32>* Memory)
	{
		Memory->~FScriptConsoleVariable<int32>();
	});


	FConsoleVariable_.Method("bool GetBool() const", &FScriptConsoleVariable<bool>::GetBool);
	FConsoleVariable_.Method("float32 GetFloat() const", &FScriptConsoleVariable<float>::GetFloat);
	FConsoleVariable_.Method("int GetInt() const", &FScriptConsoleVariable<int>::GetInt);
	FConsoleVariable_.Method("FString GetString() const", &FScriptConsoleVariable<FString>::GetString);
	
	FConsoleVariable_.Method("void SetBool(bool InValue) const", &FScriptConsoleVariable<bool>::SetBool);
	FConsoleVariable_.Method("void SetFloat(float32 InValue) const", &FScriptConsoleVariable<float>::SetFloat);
	FConsoleVariable_.Method("void SetInt(int InValue) const", &FScriptConsoleVariable<int32>::SetInt);
	FConsoleVariable_.Method("void SetString(const FString& InValue) const", &FScriptConsoleVariable<FString>::SetString);
});

namespace
{
	TMap<FString, const void*> GScriptConsoleCommandOwners;
}

struct FScriptConsoleCommand
{
	IConsoleCommand* Command = nullptr;
	FString CommandName;

	FScriptConsoleCommand(const FString& Name, const FString& FunctionName)
	{
#if !UE_BUILD_SHIPPING
		auto* Context = FAngelscriptEngine::Get().GetPreviousScriptContext();
		asIScriptFunction* Function = Context->GetFunction(0);
		if (!ensure(Function != nullptr))
			return;

		asIScriptModule* Module = Function->GetModule();
		if (!ensure(Module != nullptr))
			return;

		FString Decl = FString::Printf(TEXT("void %s(const TArray<FString>& Args)"), *FunctionName);
		auto* CallFunction = Module->GetFunctionByDecl(TCHAR_TO_ANSI(*Decl));
		if (CallFunction == nullptr)
		{
			auto* NamedFunction = Module->GetFunctionByName(TCHAR_TO_ANSI(*FunctionName));

			FString Message;
			if (NamedFunction == nullptr)
				Message = FString::Printf(TEXT("Could not find global function '%s' to bind as console command."), *FunctionName);
			else
				Message = FString::Printf(TEXT("Global function for console command must have signature `void %s(TArray<FString> Arguments)`"), *FunctionName);

			FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Message));
			return;
		}

		TAngelscriptSharedPtr<asIScriptFunction> FunPtr = CallFunction;

		auto* Existing = IConsoleManager::Get().FindConsoleObject(*Name);
		if (Existing != nullptr)
			IConsoleManager::Get().UnregisterConsoleObject(Existing);

		CommandName = Name;
		Command = IConsoleManager::Get().RegisterConsoleCommand(*Name, TEXT(""),

		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[FunPtr](const TArray<FString>& Args, UWorld* World)
		{
			if (!FunPtr.IsValid())
				return;
			auto* Module = FunPtr->GetModule();
			if (Module == nullptr)
				return;

			FAngelscriptContext Context(World, FunPtr->GetEngine());
			if (!PrepareAngelscriptContextWithLog(Context, FunPtr.Get(), TEXT("FScriptConsoleCommand")))
			{
				return;
			}
			Context->SetArgAddress(0, (void*)&Args);
			Context->Execute();
		}));
		GScriptConsoleCommandOwners.Add(CommandName, this);
#endif
	}

	~FScriptConsoleCommand()
	{
#if !UE_BUILD_SHIPPING
		const void* const* CurrentOwner = GScriptConsoleCommandOwners.Find(CommandName);
		if (CurrentOwner != nullptr && *CurrentOwner == this)
		{
			// Pointer equality against the console object is not enough: if this command was
			// replaced, the console manager can reuse the old allocation address for the new
			// command. Track script-side ownership separately so an old module cannot unregister
			// a newer replacement.
			auto* RegisteredCommand = IConsoleManager::Get().FindConsoleObject(*CommandName);
			if (RegisteredCommand == Command)
				IConsoleManager::Get().UnregisterConsoleObject(Command);
			GScriptConsoleCommandOwners.Remove(CommandName);
		}
#endif
	}
};

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_ConsoleCommands((int32)FAngelscriptBinds::EOrder::Late - 1, []
{
	FBindFlags Flags;
	auto FConsoleCommand_ = FAngelscriptBinds::ValueClass<FScriptConsoleCommand>("FConsoleCommand", Flags);

	FConsoleCommand_.Constructor("void f(const FString& Name, const FName& FunctionName)",
	[](void* Memory, const FString& Name, const FName& FunctionName)
	{
		new(Memory) FScriptConsoleCommand(Name, FunctionName.ToString());
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	FConsoleCommand_.Destructor("void f()",
	[](FScriptConsoleCommand* Memory)
	{
		Memory->~FScriptConsoleCommand();
	});
});
