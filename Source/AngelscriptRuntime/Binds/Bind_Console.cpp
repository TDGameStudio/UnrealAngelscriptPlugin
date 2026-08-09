#include "Binds/Bind_Console.h"

#include "AngelscriptBinds.h"

/**
 * Console variable and command binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FConsoleVariable;                                                                   | Declares the script-owned console-variable handle type.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FConsoleCommand;                                                                    | Declares the script-owned console-command handle type.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FConsoleVariable Variable(const FString& Name, int DefaultValue,                           | Registers an integer console variable for the lifetime of this handle.                                               |
 * |     const FString& Help = "");                                                             | @param Help Optional text shown by the console help system.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FConsoleVariable Variable(const FString& Name, bool DefaultValue,                          | Registers a Boolean console variable for the lifetime of this handle.                                                |
 * |     const FString& Help = "");                                                             | @param Help Optional text shown by the console help system.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FConsoleVariable Variable(const FString& Name, float32 DefaultValue,                       | Registers a floating-point console variable for the lifetime of this handle.                                         |
 * |     const FString& Help = "");                                                             | @param Help Optional text shown by the console help system.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FConsoleVariable Variable(const FString& Name, const FString& DefaultValue,                | Registers a string console variable for the lifetime of this handle.                                                 |
 * |     const FString& Help = "");                                                             | @param Help Optional text shown by the console help system.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FConsoleVariable.GetBool() const;                                                     | Returns the current value converted to bool.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FConsoleVariable.GetFloat() const;                                                 | Returns the current value converted to float32.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FConsoleVariable.GetInt() const;                                                       | Returns the current value converted to int.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FConsoleVariable.GetString() const;                                                | Returns the current value converted to FString.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FConsoleVariable.SetBool(bool InValue) const;                                         | Sets the console variable from a Boolean value.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FConsoleVariable.SetFloat(float32 InValue) const;                                     | Sets the console variable from a floating-point value.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FConsoleVariable.SetInt(int InValue) const;                                           | Sets the console variable from an integer value.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FConsoleVariable.SetString(const FString& InValue) const;                             | Sets the console variable from a string value.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FConsoleCommand Command(const FString& Name, const FName& FunctionName);                   | Registers a console command that invokes a script function for this handle lifetime.                                 |
 * |                                                                                            | @param FunctionName Name of the compatible script function to invoke.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_ConsoleTypes(
	TEXT("Console.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		const FBindFlags Flags;
		Binds.ValueClassForTarget<FScriptConsoleVariable<int32>>("FConsoleVariable", Flags);
		Binds.ValueClassForTarget<FScriptConsoleCommand>("FConsoleCommand", Flags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_ConsoleVariables(
	TEXT("Console.Variables"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});

AS_FORCE_LINK const FAngelscriptBind Bind_ConsoleCommands(
	TEXT("Console.Commands"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto Command_ = Binds.ExistingClassForTarget("FConsoleCommand");
		Command_
			.Constructor(
				"void f(const FString& Name, const FName& FunctionName)",
				&FAngelscriptConsoleBinds::ConstructCommand)
			.NoDiscard();
		Command_.Destructor("void f()", &FAngelscriptConsoleBinds::DestructCommand);
	});
