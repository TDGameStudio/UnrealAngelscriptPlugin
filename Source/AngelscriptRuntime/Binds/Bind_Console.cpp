#include "Binds/Bind_Console.h"

#include "AngelscriptBinds.h"

#include "Bind_Console_Functions.h"

namespace
{
	void BindConsoleTypes(FAngelscriptBinds& Binds)
	{
		const FBindFlags Flags;
		Binds.ValueClassForTarget<FScriptConsoleVariable<int32>>("FConsoleVariable", Flags);
		Binds.ValueClassForTarget<FScriptConsoleCommand>("FConsoleCommand", Flags);
	}

	void BindConsoleVariableFunctions(FAngelscriptBinds& Binds)
	{
		auto Variable_ = Binds.ExistingClassForTarget("FConsoleVariable");
		Variable_
			.Constructor(
				"void f(const FString& Name, int DefaultValue, const FString& Help = \"\")",
				&FAngelscriptConsoleBinds::ConstructIntVariable)
			.NoDiscard();
		Variable_
			.Constructor(
				"void f(const FString& Name, bool DefaultValue, const FString& Help = \"\")",
				&FAngelscriptConsoleBinds::ConstructBoolVariable)
			.NoDiscard();
		Variable_
			.Constructor(
				"void f(const FString& Name, float32 DefaultValue, const FString& Help = \"\")",
				&FAngelscriptConsoleBinds::ConstructFloatVariable)
			.NoDiscard();
		Variable_
			.Constructor(
				"void f(const FString& Name, const FString& DefaultValue, const FString& Help = \"\")",
				&FAngelscriptConsoleBinds::ConstructStringVariable)
			.NoDiscard();

		Variable_.Destructor("void f()", &FAngelscriptConsoleBinds::DestructVariable);
		Variable_.Method("bool GetBool() const", &FScriptConsoleVariable<bool>::GetBool);
		Variable_.Method("float32 GetFloat() const", &FScriptConsoleVariable<float>::GetFloat);
		Variable_.Method("int GetInt() const", &FScriptConsoleVariable<int>::GetInt);
		Variable_.Method("FString GetString() const", &FScriptConsoleVariable<FString>::GetString);
		Variable_.Method("void SetBool(bool InValue) const", &FScriptConsoleVariable<bool>::SetBool);
		Variable_.Method("void SetFloat(float32 InValue) const", &FScriptConsoleVariable<float>::SetFloat);
		Variable_.Method("void SetInt(int InValue) const", &FScriptConsoleVariable<int32>::SetInt);
		Variable_.Method("void SetString(const FString& InValue) const", &FScriptConsoleVariable<FString>::SetString);
	}

	void BindConsoleCommandFunctions(FAngelscriptBinds& Binds)
	{
		auto Command_ = Binds.ExistingClassForTarget("FConsoleCommand");
		Command_
			.Constructor(
				"void f(const FString& Name, const FName& FunctionName)",
				&FAngelscriptConsoleBinds::ConstructCommand)
			.NoDiscard();
		Command_.Destructor("void f()", &FAngelscriptConsoleBinds::DestructCommand);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_ConsoleTypes(
	TEXT("Console.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindConsoleTypes);

AS_FORCE_LINK const FAngelscriptBind Bind_ConsoleVariables(
	TEXT("Console.Variables"),
	EAngelscriptBindPhase::ManualBindings,
	&BindConsoleVariableFunctions);

AS_FORCE_LINK const FAngelscriptBind Bind_ConsoleCommands(
	TEXT("Console.Commands"),
	EAngelscriptBindPhase::ManualBindings,
	&BindConsoleCommandFunctions);
