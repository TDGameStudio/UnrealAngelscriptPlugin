#include "Bindings/AngelscriptConsoleBindingsSections.h"

#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "HAL/IConsoleManager.h"
#include "Misc/Guid.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace
{
	static constexpr TCHAR ConsoleObjectPrefix[] = TEXT("as.test.console");


	struct FConsoleManagerScope
	{
		FConsoleManagerScope(FAutomationTestBase& InTest, const TCHAR* InSectionName)
			: Test(InTest)
			, SectionName(InSectionName)
		{
		}

		~FConsoleManagerScope()
		{
			Cleanup();
		}

		FConsoleManagerScope(const FConsoleManagerScope&) = delete;
		FConsoleManagerScope& operator=(const FConsoleManagerScope&) = delete;

		FString MakeName(const TCHAR* Kind)
		{
			FString Name = FString::Printf(
				TEXT("%s.%s.%s.%s"),
				ConsoleObjectPrefix,
				*SectionName,
				Kind,
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			TrackName(Name);
			return Name;
		}

		void TrackName(const FString& Name)
		{
			RegisteredNames.AddUnique(Name);
		}

		IConsoleVariable* RegisterIntVariable(const FString& Name, int32 Value, const TCHAR* Help, EConsoleVariableFlags Flags = ECVF_Default)
		{
			TrackName(Name);
			return IConsoleManager::Get().RegisterConsoleVariable(*Name, Value, Help, Flags);
		}

		IConsoleVariable* RegisterStringVariable(const FString& Name, const TCHAR* Value, const TCHAR* Help)
		{
			TrackName(Name);
			return IConsoleManager::Get().RegisterConsoleVariable(*Name, Value, Help);
		}

		IConsoleCommand* FindCommand(const FString& Name) const
		{
			return static_cast<IConsoleCommand*>(IConsoleManager::Get().FindConsoleObject(*Name));
		}

		bool VerifyCommandExists(const FString& Name, const TCHAR* ContextLabel) const
		{
			FNoDiscardAsserter Assert(Test);
			return Assert.IsNotNull(
				FindCommand(Name),
				*FString::Printf(TEXT("%s should register the console command"), ContextLabel));
		}

		bool VerifyCommandMissing(const FString& Name, const TCHAR* ContextLabel) const
		{
			FNoDiscardAsserter Assert(Test);
			return Assert.IsNull(
				FindCommand(Name),
				*FString::Printf(TEXT("%s should not leave a registered console command"), ContextLabel));
		}

		bool ExecuteCommand(const FString& Name, const TArray<FString>& Args, const TCHAR* ContextLabel) const
		{
			IConsoleCommand* Command = FindCommand(Name);
			FNoDiscardAsserter Assert(Test);
			if (!Assert.IsNotNull(
					Command,
					*FString::Printf(TEXT("%s should find the registered command before execution"), ContextLabel)))
			{
				return false;
			}

			FOutputDeviceNull OutputDevice;
			return Assert.IsTrue(
				Command->Execute(Args, nullptr, OutputDevice),
				*FString::Printf(TEXT("%s should execute the registered delegate"), ContextLabel));
		}

		bool VerifyInt(const FString& Name, int32 ExpectedValue, const TCHAR* ContextLabel) const
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			FNoDiscardAsserter Assert(Test);
			if (!Assert.IsNotNull(
					Variable,
					*FString::Printf(TEXT("%s should find int cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}

			return Assert.AreEqual(
				ExpectedValue,
				Variable->GetInt(),
				*FString::Printf(TEXT("%s should preserve expected int value"), ContextLabel));
		}

		bool VerifyFloat(const FString& Name, float ExpectedValue, const TCHAR* ContextLabel) const
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			FNoDiscardAsserter Assert(Test);
			if (!Assert.IsNotNull(
					Variable,
					*FString::Printf(TEXT("%s should find float cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}

			return Assert.IsNear(
				ExpectedValue,
				Variable->GetFloat(),
				0.0001f,
				*FString::Printf(TEXT("%s should preserve expected float value"), ContextLabel));
		}

		bool VerifyBool(const FString& Name, bool bExpectedValue, const TCHAR* ContextLabel) const
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			FNoDiscardAsserter Assert(Test);
			if (!Assert.IsNotNull(
					Variable,
					*FString::Printf(TEXT("%s should find bool cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}

			return Assert.AreEqual(
				bExpectedValue,
				Variable->GetBool(),
				*FString::Printf(TEXT("%s should preserve expected bool value"), ContextLabel));
		}

		bool VerifyString(const FString& Name, const FString& ExpectedValue, const TCHAR* ContextLabel) const
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			FNoDiscardAsserter Assert(Test);
			if (!Assert.IsNotNull(
					Variable,
					*FString::Printf(TEXT("%s should find string cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}

			return Assert.AreEqual(
				ExpectedValue,
				FString(Variable->GetString()),
				*FString::Printf(TEXT("%s should preserve expected string value"), ContextLabel));
		}

		bool VerifyIdentity(
			const FString& Name,
			IConsoleVariable* ExpectedVariable,
			const FString& ExpectedHelp,
			EConsoleVariableFlags ExpectedFlags,
			const TCHAR* ContextLabel) const
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			FNoDiscardAsserter Assert(Test);
			if (!Assert.IsNotNull(
					Variable,
					*FString::Printf(TEXT("%s should find existing cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}

			const uint32 SetByMaskBits = static_cast<uint32>(ECVF_SetByMask);
			const uint32 ExpectedPersistentFlags = static_cast<uint32>(ExpectedFlags) & ~SetByMaskBits;
			const uint32 CurrentPersistentFlags = static_cast<uint32>(Variable->GetFlags()) & ~SetByMaskBits;

			bool bPassed = true;
			bPassed &= Assert.IsTrue(
				Variable == ExpectedVariable,
				*FString::Printf(TEXT("%s should preserve the native IConsoleVariable pointer"), ContextLabel));
			bPassed &= Assert.AreEqual(
				ExpectedHelp,
				FString(Variable->GetHelp()),
				*FString::Printf(TEXT("%s should preserve native help text"), ContextLabel));
			bPassed &= Assert.AreEqual(
				ExpectedPersistentFlags,
				CurrentPersistentFlags,
				*FString::Printf(TEXT("%s should preserve persistent native flags"), ContextLabel));
			return bPassed;
		}

		bool VerifyNoLeaks(const TCHAR* ContextLabel)
		{
			Cleanup();

			TArray<FString> LeakedNames;
			IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
				FConsoleObjectVisitor::CreateLambda(
					[&LeakedNames](const TCHAR* Name, IConsoleObject*)
					{
						LeakedNames.Add(Name);
					}),
				ConsoleObjectPrefix);

			if (!LeakedNames.IsEmpty())
			{
				Test.AddError(FString::Printf(
					TEXT("%s leaked console objects under '%s': %s"),
					ContextLabel,
					ConsoleObjectPrefix,
					*FString::Join(LeakedNames, TEXT(", "))));
				return false;
			}

			FNoDiscardAsserter Assert(Test);
			return Assert.AreEqual(
				0,
				LeakedNames.Num(),
				*FString::Printf(TEXT("%s should leave no '%s' console objects"), ContextLabel, ConsoleObjectPrefix));
		}

	private:
		void Cleanup()
		{
			for (int32 Index = RegisteredNames.Num() - 1; Index >= 0; --Index)
			{
				if (IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(*RegisteredNames[Index]))
				{
					IConsoleManager::Get().UnregisterConsoleObject(ConsoleObject, false);
				}
			}
			RegisteredNames.Reset();
		}

		FAutomationTestBase& Test;
		FString SectionName;
		TArray<FString> RegisteredNames;
	};

	FString MakeCommandSource(
		const FString& CommandName,
		const FString& OutputName,
		const TCHAR* HandlerName,
		int32 OutputMarker)
	{
		return FString::Printf(TEXT(R"(
const FConsoleCommand Command("%s", n"%s");

void %s(const TArray<FString>& Args)
{
	FConsoleVariable Output("%s", 0, "Console command output sink");
	Output.SetInt(%d);
}

int CommandReady()
{
	return 1;
}
)"),
			*CommandName,
			HandlerName,
			HandlerName,
			*OutputName,
			OutputMarker);
	}

	bool RunConsoleCommandArgumentSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* SectionName,
		const TArray<FString>& Args,
		const FString& ExpectedOutput)
	{
		FConsoleManagerScope ConsoleScope(Test, SectionName);
		const FString CommandName = ConsoleScope.MakeName(TEXT("command"));
		const FString OutputName = ConsoleScope.MakeName(TEXT("output"));

		bool bPassed = true;
		IConsoleVariable* OutputVariable = ConsoleScope.RegisterStringVariable(
			OutputName,
			TEXT("__native_unset__"),
			TEXT("Console command argument output sink"));
		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsNotNull(OutputVariable, TEXT("Console command argument section should pre-register output cvar")))
		{
			return false;
		}

		TUniquePtr<FScopedAngelscriptModule> ModuleScope = MakeUnique<FScopedAngelscriptModule>(
			Test,
			Engine, 
			SectionName,
			FString::Printf(TEXT(R"(
const FConsoleCommand Command("%s", n"OnCommand");

void OnCommand(const TArray<FString>& Args)
{
	FConsoleVariable Output("%s", "__unset__", "Console command output sink");
	if (Args.Num() == 0)
	{
		Output.SetString("<empty>");
		return;
	}

	FString Joined = "";
	for (int Index = 0, Count = Args.Num(); Index < Count; ++Index)
	{
		if (Index != 0)
			Joined += "|";
		Joined += Args[Index];
	}

	Output.SetString(Joined);
}

int CommandReady()
{
	return 1;
}
)"), *CommandName, *OutputName));
		if (!ModuleScope->IsValid())
		{
			return false;
		}

		asIScriptModule& Module = ModuleScope->GetModule();
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int CommandReady()"), TEXT("Console command argument module should initialize"), 1);
		bPassed &= ConsoleScope.VerifyCommandExists(CommandName, TEXT("Console command argument setup"));
		bPassed &= ConsoleScope.ExecuteCommand(CommandName, Args, TEXT("Console command argument execution"));
		bPassed &= ConsoleScope.VerifyString(OutputName, ExpectedOutput, TEXT("Console command argument execution"));

		ModuleScope.Reset();
		bPassed &= ConsoleScope.VerifyCommandMissing(CommandName, TEXT("Console command argument unload"));
		bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console command argument section"));
		return bPassed;
	}
}


bool RunConsoleVariableTypesSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("VariableTypes"));
	const FString IntName = ConsoleScope.MakeName(TEXT("int"));
	const FString FloatName = ConsoleScope.MakeName(TEXT("float"));
	const FString BoolName = ConsoleScope.MakeName(TEXT("bool"));
	const FString StringName = ConsoleScope.MakeName(TEXT("string"));

	bool bPassed = true;
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASConsole_VariableTypes"), FString::Printf(TEXT(R"(
int IntDefault()
{
FConsoleVariable IntVar("%s", 5, "Test int cvar");
return IntVar.GetInt();
}
int IntUpdated()
{
FConsoleVariable IntVar("%s", 5, "Test int cvar");
IntVar.SetInt(42);
return IntVar.GetInt();
}
float FloatDefault()
{
FConsoleVariable FloatVar("%s", 1.5f, "Test float cvar");
return FloatVar.GetFloat();
}
float FloatUpdated()
{
FConsoleVariable FloatVar("%s", 1.5f, "Test float cvar");
FloatVar.SetFloat(3.25f);
return FloatVar.GetFloat();
}
int BoolDefault()
{
FConsoleVariable BoolVar("%s", true, "Test bool cvar");
return BoolVar.GetBool() ? 1 : 0;
}
int BoolUpdated()
{
FConsoleVariable BoolVar("%s", true, "Test bool cvar");
BoolVar.SetBool(false);
return BoolVar.GetBool() ? 1 : 0;
}
int StringDefault()
{
FConsoleVariable StringVar("%s", "DefaultValue", "Test string cvar");
return StringVar.GetString() == "DefaultValue" ? 1 : 0;
}
int StringUpdated()
{
FConsoleVariable StringVar("%s", "DefaultValue", "Test string cvar");
StringVar.SetString("UpdatedValue");
return StringVar.GetString() == "UpdatedValue" ? 1 : 0;
}
)"),
			*IntName,
			*IntName,
			*FloatName,
			*FloatName,
			*BoolName,
			*BoolName,
			*StringName,
			*StringName));
		if (!ModuleScope.IsValid())
		{
			return false;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int IntDefault()"), TEXT("FConsoleVariable int default should read back"), 5);
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int IntUpdated()"), TEXT("FConsoleVariable int SetInt should read back"), 42);
		bPassed &= ExpectGlobalReturnFloat(Test, Engine, Module, 
			TEXT("float FloatDefault()"), TEXT("FConsoleVariable float default should read back"), 1.5f, 0.01f);
		bPassed &= ExpectGlobalReturnFloat(Test, Engine, Module, 
			TEXT("float FloatUpdated()"), TEXT("FConsoleVariable float SetFloat should read back"), 3.25f, 0.01f);
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int BoolDefault()"), TEXT("FConsoleVariable bool default should read back"), 1);
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int BoolUpdated()"), TEXT("FConsoleVariable bool SetBool should read back"), 0);
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int StringDefault()"), TEXT("FConsoleVariable string default should read back"), 1);
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int StringUpdated()"), TEXT("FConsoleVariable string SetString should read back"), 1);
	}

	bPassed &= ConsoleScope.VerifyInt(IntName, 42, TEXT("Console variable type native int"));
	bPassed &= ConsoleScope.VerifyFloat(FloatName, 3.25f, TEXT("Console variable type native float"));
	bPassed &= ConsoleScope.VerifyBool(BoolName, false, TEXT("Console variable type native bool"));
	bPassed &= ConsoleScope.VerifyString(StringName, TEXT("UpdatedValue"), TEXT("Console variable type native string"));
	bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console variable type section"));
	return bPassed;
}

bool RunConsoleVariableExistingSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("VariableExisting"));
	const FString ExistingName = ConsoleScope.MakeName(TEXT("existing"));

	IConsoleVariable* ExistingVariable = ConsoleScope.RegisterIntVariable(
		ExistingName,
		7,
		TEXT("Existing native cvar for bindings test"));
	FNoDiscardAsserter Assert(Test);
	if (!Assert.IsNotNull(ExistingVariable, TEXT("Console variable existing section should pre-register native cvar")))
	{
		return false;
	}

	bool bPassed = true;
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASConsole_VariableExisting"), FString::Printf(TEXT(R"(
int ExistingInitial()
{
FConsoleVariable ExistingVar("%s", 99, "Should reuse existing native cvar");
return ExistingVar.GetInt();
}
int ExistingUpdated()
{
FConsoleVariable ExistingVar("%s", 99, "Should reuse existing native cvar");
ExistingVar.SetInt(21);
return ExistingVar.GetInt();
}
)"), *ExistingName, *ExistingName));
		if (!ModuleScope.IsValid())
		{
			return false;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int ExistingInitial()"), TEXT("Existing native CVar initial value should be reused"), 7);
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int ExistingUpdated()"), TEXT("Existing native CVar should accept script SetInt"), 21);
	}

	bPassed &= ConsoleScope.VerifyInt(ExistingName, 21, TEXT("Console variable existing native value"));
	bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console variable existing section"));
	return bPassed;
}

bool RunConsoleVariableIdentitySection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("VariableIdentity"));
	const FString ExistingName = ConsoleScope.MakeName(TEXT("existingIdentity"));

	IConsoleVariable* ExistingVariable = ConsoleScope.RegisterIntVariable(
		ExistingName,
		7,
		TEXT("Existing native cvar identity/help/flags should survive bindings test"),
		ECVF_Cheat);
	FNoDiscardAsserter Assert(Test);
	if (!Assert.IsNotNull(ExistingVariable, TEXT("Console variable identity section should pre-register native cvar")))
	{
		return false;
	}

	const FString ExistingHelp = ExistingVariable->GetHelp();
	const EConsoleVariableFlags ExistingFlags = ExistingVariable->GetFlags();

	bool bPassed = true;
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASConsole_VariableIdentity"), FString::Printf(TEXT(R"(
int ExistingInitial()
{
FConsoleVariable ExistingVar("%s", 99, "Should not replace native cvar");
return ExistingVar.GetInt();
}
int ExistingUpdated()
{
FConsoleVariable ExistingVar("%s", 99, "Should not replace native cvar");
ExistingVar.SetInt(21);
return ExistingVar.GetInt();
}
)"), *ExistingName, *ExistingName));
		if (!ModuleScope.IsValid())
		{
			return false;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int ExistingInitial()"), TEXT("Existing identity CVar initial value should be reused"), 7);
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int ExistingUpdated()"), TEXT("Existing identity CVar should accept script SetInt"), 21);
	}

	bPassed &= ConsoleScope.VerifyInt(ExistingName, 21, TEXT("Console variable identity native value"));
	bPassed &= ConsoleScope.VerifyIdentity(
		ExistingName,
		ExistingVariable,
		ExistingHelp,
		ExistingFlags,
		TEXT("Console variable identity native metadata"));
	bPassed &= Assert.IsTrue(
		ExistingVariable->TestFlags(ECVF_Cheat),
		TEXT("Console variable identity should preserve the native cheat flag"));
	bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console variable identity section"));
	return bPassed;
}

bool RunConsoleCommandBasicSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("CommandBasic"));
	const FString CommandName = ConsoleScope.MakeName(TEXT("command"));
	const FString OutputName = ConsoleScope.MakeName(TEXT("output"));

	IConsoleVariable* OutputVariable = ConsoleScope.RegisterIntVariable(
		OutputName,
		-1,
		TEXT("Console command output sink"));
	FNoDiscardAsserter Assert(Test);
	if (!Assert.IsNotNull(OutputVariable, TEXT("Console command basic section should pre-register output cvar")))
	{
		return false;
	}

	bool bPassed = true;
	TUniquePtr<FScopedAngelscriptModule> ModuleScope = MakeUnique<FScopedAngelscriptModule>(
		Test,
		Engine, 
		TEXT("CommandBasic"),
		FString::Printf(TEXT(R"(
const FConsoleCommand Command("%s", n"OnCommand");

void OnCommand(const TArray<FString>& Args)
{
FConsoleVariable Output("%s", 0, "Command output");
Output.SetInt(Args.Num());
}

int CommandReady()
{
return 1;
}
)"), *CommandName, *OutputName));
	if (!ModuleScope->IsValid())
	{
		return false;
	}

	asIScriptModule& Module = ModuleScope->GetModule();
	bPassed &= ExpectGlobalInt(Test, Engine, Module, 
		TEXT("int CommandReady()"), TEXT("Console command basic module should initialize"), 1);
	bPassed &= ConsoleScope.VerifyCommandExists(CommandName, TEXT("Console command basic setup"));

	TArray<FString> Args;
	Args.Add(TEXT("One"));
	Args.Add(TEXT("Two"));
	Args.Add(TEXT("Three"));
	bPassed &= ConsoleScope.ExecuteCommand(CommandName, Args, TEXT("Console command basic execution"));
	bPassed &= ConsoleScope.VerifyInt(OutputName, 3, TEXT("Console command basic execution"));

	ModuleScope.Reset();
	bPassed &= ConsoleScope.VerifyCommandMissing(CommandName, TEXT("Console command basic unload"));
	bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console command basic section"));
	return bPassed;
}

bool RunConsoleCommandArgumentEmptySection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	const TArray<FString> Args;
	return RunConsoleCommandArgumentSection(
		Test,
		Engine, 
		TEXT("CommandArgumentEmpty"),
		Args,
		TEXT("<empty>"));
}

bool RunConsoleCommandArgumentContentSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	TArray<FString> Args;
	Args.Add(TEXT("One"));
	Args.Add(TEXT("Two Words"));
	Args.Add(TEXT("Three=Value"));
	return RunConsoleCommandArgumentSection(
		Test,
		Engine, 
		TEXT("CommandArgumentContent"),
		Args,
		TEXT("One|Two Words|Three=Value"));
}

bool RunConsoleCommandReplacementSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("CommandReplacement"));
	const FString CommandName = ConsoleScope.MakeName(TEXT("command"));
	const FString OutputName = ConsoleScope.MakeName(TEXT("output"));

	IConsoleVariable* OutputVariable = ConsoleScope.RegisterIntVariable(
		OutputName,
		-1,
		TEXT("Console command replacement output sink"));
	FNoDiscardAsserter Assert(Test);
	if (!Assert.IsNotNull(OutputVariable, TEXT("Console command replacement section should pre-register output cvar")))
	{
		return false;
	}

	bool bPassed = true;
	TUniquePtr<FScopedAngelscriptModule> OriginalScope = MakeUnique<FScopedAngelscriptModule>(
		Test,
		Engine, 
		TEXT("CommandReplacementOriginal"),
		MakeCommandSource(CommandName, OutputName, TEXT("OnOriginalCommand"), 11));
	if (!OriginalScope->IsValid())
	{
		return false;
	}

	bPassed &= ExpectGlobalInt(Test, Engine, OriginalScope->GetModule(), 
		TEXT("int CommandReady()"), TEXT("Console command original replacement module should initialize"), 1);

	{
		FScopedAngelscriptModule ReplacementScope(
			Test,
			Engine,
			TEXT("ASConsole_CommandReplacementActive"),
			MakeCommandSource(CommandName, OutputName, TEXT("OnReplacementCommand"), 22));
		if (!ReplacementScope.IsValid())
		{
			return false;
		}

		bPassed &= ExpectGlobalInt(Test, Engine, ReplacementScope.GetModule(), 
			TEXT("int CommandReady()"), TEXT("Console command replacement module should initialize"), 1);
		bPassed &= ConsoleScope.VerifyCommandExists(CommandName, TEXT("Console command replacement setup"));
		bPassed &= ConsoleScope.ExecuteCommand(CommandName, {}, TEXT("Console command replacement execution"));
		bPassed &= ConsoleScope.VerifyInt(OutputName, 22, TEXT("Console command replacement execution"));
	}

	bPassed &= ConsoleScope.VerifyCommandMissing(CommandName, TEXT("Console command replacement unload"));
	OriginalScope.Reset();
	bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console command replacement section"));
	return bPassed;
}

bool RunConsoleCommandLifecycleSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("CommandLifecycle"));
	const FString CommandName = ConsoleScope.MakeName(TEXT("command"));
	const FString OutputName = ConsoleScope.MakeName(TEXT("output"));

	IConsoleVariable* OutputVariable = ConsoleScope.RegisterIntVariable(
		OutputName,
		-1,
		TEXT("Console command lifecycle output sink"));
	FNoDiscardAsserter Assert(Test);
	if (!Assert.IsNotNull(OutputVariable, TEXT("Console command lifecycle section should pre-register output cvar")))
	{
		return false;
	}

	bool bPassed = true;
	TUniquePtr<FScopedAngelscriptModule> OriginalScope = MakeUnique<FScopedAngelscriptModule>(
		Test,
		Engine, 
		TEXT("CommandLifecycleOriginal"),
		MakeCommandSource(CommandName, OutputName, TEXT("OnOriginalCommand"), 11));
	if (!OriginalScope->IsValid())
	{
		return false;
	}

	bPassed &= ExpectGlobalInt(Test, Engine, OriginalScope->GetModule(), 
		TEXT("int CommandReady()"), TEXT("Console command lifecycle original module should initialize"), 1);
	bPassed &= ConsoleScope.VerifyCommandExists(CommandName, TEXT("Console command lifecycle original setup"));
	bPassed &= ConsoleScope.ExecuteCommand(CommandName, {}, TEXT("Console command lifecycle original execution"));
	bPassed &= ConsoleScope.VerifyInt(OutputName, 11, TEXT("Console command lifecycle original execution"));

	TUniquePtr<FScopedAngelscriptModule> ReplacementScope = MakeUnique<FScopedAngelscriptModule>(
		Test,
		Engine, 
		TEXT("CommandLifecycleReplacement"),
		MakeCommandSource(CommandName, OutputName, TEXT("OnReplacementCommand"), 22));
	if (!ReplacementScope->IsValid())
	{
		return false;
	}

	bPassed &= ExpectGlobalInt(Test, Engine, ReplacementScope->GetModule(), 
		TEXT("int CommandReady()"), TEXT("Console command lifecycle replacement module should initialize"), 1);
	bPassed &= ConsoleScope.VerifyCommandExists(CommandName, TEXT("Console command lifecycle replacement setup"));
	bPassed &= ConsoleScope.ExecuteCommand(CommandName, {}, TEXT("Console command lifecycle replacement execution"));
	bPassed &= ConsoleScope.VerifyInt(OutputName, 22, TEXT("Console command lifecycle replacement execution"));

	OriginalScope.Reset();
	bPassed &= ConsoleScope.VerifyCommandExists(CommandName, TEXT("Console command lifecycle original unload"));
	bPassed &= ConsoleScope.ExecuteCommand(CommandName, {}, TEXT("Console command lifecycle replacement after original unload"));
	bPassed &= ConsoleScope.VerifyInt(OutputName, 22, TEXT("Console command lifecycle replacement after original unload"));

	ReplacementScope.Reset();
	bPassed &= ConsoleScope.VerifyCommandMissing(CommandName, TEXT("Console command lifecycle replacement unload"));
	bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console command lifecycle section"));
	return bPassed;
}

bool RunConsoleCommandMissingHandlerSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("CommandMissingHandler"));
	const FString CommandName = ConsoleScope.MakeName(TEXT("command"));

	bool bPassed = true;
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASConsole_CommandMissingHandler"), FString::Printf(TEXT(R"(
void Trigger()
{
const FConsoleCommand Command("%s", n"MissingHandler");
}
)"), *CommandName));
		if (!ModuleScope.IsValid())
		{
			return false;
		}

		Test.AddExpectedError(
			TEXT("Could not find global function 'MissingHandler' to bind as console command."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		Test.AddExpectedError(ModuleScope.GetModuleName(), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedError(TEXT("void Trigger() | Line"), EAutomationExpectedErrorFlags::Contains, 1);

		asIScriptModule& Module = ModuleScope.GetModule();
		bPassed &= ExecuteFunctionExpectingScriptException(
			Test,
			Engine,
			Module, 
			TEXT("void Trigger()"),
			TEXT("Missing console command handler should throw"),
			TEXT("Could not find global function 'MissingHandler' to bind as console command."));
		bPassed &= ConsoleScope.VerifyCommandMissing(CommandName, TEXT("Console command missing handler failure"));
	}

	bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console command missing handler section"));
	return bPassed;
}

bool RunConsoleCommandWrongSignatureSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("CommandWrongSignature"));
	const FString CommandName = ConsoleScope.MakeName(TEXT("command"));

	bool bPassed = true;
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASConsole_CommandWrongSignature"), FString::Printf(TEXT(R"(
void WrongSignature()
{
}

void Trigger()
{
const FConsoleCommand Command("%s", n"WrongSignature");
}
)"), *CommandName));
		if (!ModuleScope.IsValid())
		{
			return false;
		}

		Test.AddExpectedError(
			TEXT("Global function for console command must have signature"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		Test.AddExpectedError(ModuleScope.GetModuleName(), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedError(TEXT("void Trigger() | Line"), EAutomationExpectedErrorFlags::Contains, 1);

		asIScriptModule& Module = ModuleScope.GetModule();
		bPassed &= ExecuteFunctionExpectingScriptException(
			Test,
			Engine,
			Module, 
			TEXT("void Trigger()"),
			TEXT("Wrong console command signature should throw"),
			TEXT("Global function for console command must have signature"));
		bPassed &= ConsoleScope.VerifyCommandMissing(CommandName, TEXT("Console command wrong signature failure"));
	}

	bPassed &= ConsoleScope.VerifyNoLeaks(TEXT("Console command wrong signature section"));
	return bPassed;
}

bool RunConsoleLeakSelfCheckSection(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine)
{
	FConsoleManagerScope ConsoleScope(Test, TEXT("LeakSelfCheck"));
	return ConsoleScope.VerifyNoLeaks(TEXT("Console leak self-check"));
}


TEST_CLASS_WITH_FLAGS(FAngelscriptConsoleBindingsTest,
	"Angelscript.TestModule.Bindings.Console",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(ConsoleVariableCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunConsoleVariableTypesSection(*TestRunner, Engine);
	}

	TEST_METHOD(ConsoleVariableExistingCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunConsoleVariableExistingSection(*TestRunner, Engine);
	}

	TEST_METHOD(ConsoleCommandCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunConsoleCommandBasicSection(*TestRunner, Engine);
	}

	TEST_METHOD(ConsoleCommandReplacementCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunConsoleCommandReplacementSection(*TestRunner, Engine);
	}

	TEST_METHOD(ConsoleCommandSignatureCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunConsoleCommandWrongSignatureSection(*TestRunner, Engine);
	}

	TEST_METHOD(LeakSelfCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunConsoleLeakSelfCheckSection(*TestRunner, Engine);
	}
};

#endif
