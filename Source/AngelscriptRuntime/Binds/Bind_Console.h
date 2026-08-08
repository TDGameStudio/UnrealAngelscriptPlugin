#pragma once
#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "HAL/IConsoleManager.h"

template<typename VarType>
struct FScriptConsoleVariable
{
	IConsoleVariable* Variable = nullptr;
	FAngelscriptEngine* HookEngine = nullptr;
	FDelegateHandle LateInitializeDelegateHandle;

	FScriptConsoleVariable(const FString& Name, VarType DefaultValue, const FString& Help)
	{
		Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
		if (Variable == nullptr)
		{
			if (!FAngelscriptEngine::Get().IsInitialCompileFinished())
			{
				// If we're still in the initial compile, we should not initialize the CVar until after compile is finished.
				// The initial compile can happen on a separate thread, so registering it now might end up crashing.
				FString NameCopy = Name;
				FString HelpCopy = Help;
				FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
				HookEngine = &Engine;
				LateInitializeDelegateHandle = Engine.GetOnInitialCompileFinished().AddLambda(
					[this, NameCopy, DefaultValue, HelpCopy]()
					{
						Variable = IConsoleManager::Get().RegisterConsoleVariable(*NameCopy, DefaultValue, *HelpCopy);
						if (HookEngine != nullptr)
						{
							HookEngine->GetOnInitialCompileFinished().Remove(LateInitializeDelegateHandle);
						}
						LateInitializeDelegateHandle.Reset();
						HookEngine = nullptr;
					}
				);
			}
			else
			{
				Variable = IConsoleManager::Get().RegisterConsoleVariable(*Name, DefaultValue, *Help);
			}
		}
	}

	~FScriptConsoleVariable()
	{
		if (LateInitializeDelegateHandle.IsValid() && HookEngine != nullptr)
		{
			HookEngine->GetOnInitialCompileFinished().Remove(LateInitializeDelegateHandle);
			LateInitializeDelegateHandle.Reset();
			HookEngine = nullptr;
		}
	}

	bool GetBool() const
	{
		if(Variable == nullptr)
			return false;
		return Variable->GetBool();
	}
	
	float GetFloat() const
	{
		if (Variable == nullptr)
			return 0.f;
		return Variable->GetFloat();
	}

	FString GetString() const
	{
		if (Variable == nullptr)
			return TEXT("");
		return Variable->GetString();
	}

	int GetInt() const
	{
		if (Variable == nullptr)
			return 0;
		return Variable->GetInt();
	}
	
	void SetBool(const bool InValue) const
	{
		if(Variable == nullptr)
			return;
		Variable->Set(InValue);
	}

	void SetFloat(const float InValue) const
	{
		if(Variable == nullptr)
			return;
		Variable->Set(InValue);
	}

	void SetString(const FString& InValue) const
	{
		if (Variable == nullptr)
			return;
		Variable->Set(*InValue);
	}
	
	void SetInt(const int InValue) const
	{
		if(Variable == nullptr)
			return;
		Variable->Set(InValue);
	}
};

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
