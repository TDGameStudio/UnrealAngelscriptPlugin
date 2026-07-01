#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "HAL/IConsoleManager.h"
#include "Misc/Guid.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/ScopeExit.h"

#include <initializer_list>

// -----------------------------------------------------------------------------
// AngelscriptCoverageCVarTests
// -----------------------------------------------------------------------------
// Coverage for Documents/Coverage/Coverage_CVar.md.
//
// The AngelScript-facing API is FConsoleVariable/FConsoleCommand, not direct
// IConsoleManager access. These cases verify script-side registration, reads,
// writes, common safe access patterns, command arguments, and command unload.
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageCVarTest,
	"Angelscript.TestModule.Coverage.CVar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr TCHAR CoverageCVarPrefix[] = TEXT("as.coverage.cvar");

	struct FCoverageConsoleScope
	{
		explicit FCoverageConsoleScope(FAutomationTestBase& InTest)
			: Assert(InTest)
		{
		}

		~FCoverageConsoleScope()
		{
			Cleanup();
		}

		FCoverageConsoleScope(const FCoverageConsoleScope&) = delete;
		FCoverageConsoleScope& operator=(const FCoverageConsoleScope&) = delete;

		FString MakeName(const TCHAR* Kind)
		{
			FString Name = FString::Printf(
				TEXT("%s.%s.%s"),
				CoverageCVarPrefix,
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

		bool ExecuteCommand(const FString& Name, const TArray<FString>& Args, const TCHAR* ContextLabel)
		{
			IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(*Name);
			IConsoleCommand* Command = ConsoleObject != nullptr ? ConsoleObject->AsCommand() : nullptr;
			if (!Assert.IsNotNull(Command, *FString::Printf(TEXT("%s should find command '%s'"), ContextLabel, *Name)))
			{
				return false;
			}

			FOutputDeviceNull OutputDevice;
			return Assert.IsTrue(
				Command->Execute(Args, nullptr, OutputDevice),
				*FString::Printf(TEXT("%s should execute command '%s'"), ContextLabel, *Name));
		}

		bool ExecuteCommandArgs(const FString& Name, std::initializer_list<const TCHAR*> Args, const TCHAR* ContextLabel)
		{
			TArray<FString> CommandArgs;
			for (const TCHAR* Arg : Args)
			{
				CommandArgs.Add(Arg);
			}
			return ExecuteCommand(Name, CommandArgs, ContextLabel);
		}

		bool VerifyCommandMissing(const FString& Name, const TCHAR* ContextLabel)
		{
			return Assert.IsNull(
				IConsoleManager::Get().FindConsoleObject(*Name),
				*FString::Printf(TEXT("%s should unregister command '%s'"), ContextLabel, *Name));
		}

		bool VerifyInt(const FString& Name, int32 ExpectedValue, const TCHAR* ContextLabel)
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			if (!Assert.IsNotNull(Variable, *FString::Printf(TEXT("%s should find int cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}
			return Assert.AreEqual(ExpectedValue, Variable->GetInt(), *FString::Printf(TEXT("%s should read expected int"), ContextLabel));
		}

		bool VerifyFloat(const FString& Name, float ExpectedValue, const TCHAR* ContextLabel)
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			if (!Assert.IsNotNull(Variable, *FString::Printf(TEXT("%s should find float cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}
			const float ActualValue = Variable->GetFloat();
			return Assert.IsTrue(
				FMath::IsNearlyEqual(ExpectedValue, ActualValue, 0.001f),
				*FString::Printf(TEXT("%s should read expected float (actual=%g expected=%g)"), ContextLabel, ActualValue, ExpectedValue));
		}

		bool VerifyBool(const FString& Name, bool bExpectedValue, const TCHAR* ContextLabel)
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			if (!Assert.IsNotNull(Variable, *FString::Printf(TEXT("%s should find bool cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}
			return Assert.AreEqual(bExpectedValue, Variable->GetBool(), *FString::Printf(TEXT("%s should read expected bool"), ContextLabel));
		}

		bool VerifyString(const FString& Name, const FString& ExpectedValue, const TCHAR* ContextLabel)
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			if (!Assert.IsNotNull(Variable, *FString::Printf(TEXT("%s should find string cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}
			return Assert.AreEqual(
				ExpectedValue,
				FString(Variable->GetString()),
				*FString::Printf(TEXT("%s should read expected string"), ContextLabel));
		}

		bool VerifyIdentity(
			const FString& Name,
			IConsoleVariable* ExpectedVariable,
			const FString& ExpectedHelp,
			EConsoleVariableFlags ExpectedFlags,
			const TCHAR* ContextLabel)
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
			if (!Assert.IsNotNull(Variable, *FString::Printf(TEXT("%s should find existing cvar '%s'"), ContextLabel, *Name)))
			{
				return false;
			}

			const uint32 SetByMaskBits = static_cast<uint32>(ECVF_SetByMask);
			const uint32 ExpectedPersistentFlags = static_cast<uint32>(ExpectedFlags) & ~SetByMaskBits;
			const uint32 ActualPersistentFlags = static_cast<uint32>(Variable->GetFlags()) & ~SetByMaskBits;

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
				ActualPersistentFlags,
				*FString::Printf(TEXT("%s should preserve persistent native flags"), ContextLabel));
			return bPassed;
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

		FNoDiscardAsserter Assert;
		TArray<FString> RegisteredNames;
	};

	static FString MakeScriptSource(FString Source, const TArray<const FString*>& Arguments)
	{
		for (int32 Index = 0; Index < Arguments.Num(); ++Index)
		{
			Source.ReplaceInline(*FString::Printf(TEXT("$ARG%d$"), Index), **Arguments[Index], ESearchCase::CaseSensitive);
		}
		return Source;
	}

	enum class ERestoreCVarType
	{
		Int,
		Float,
		String
	};

	struct FRestoreCVarState
	{
		const TCHAR* Name = nullptr;
		ERestoreCVarType Type = ERestoreCVarType::Int;
		IConsoleVariable* Variable = nullptr;
		EConsoleVariableFlags OriginalFlags = ECVF_Default;
		EConsoleVariableFlags OriginalSetBy = ECVF_SetByConstructor;
		EConsoleVariableFlags OriginalPersistentFlags = ECVF_Default;
		int32 OriginalInt = 0;
		float OriginalFloat = 0.0f;
		FString OriginalString;
	};

	static void CaptureRestoreState(FRestoreCVarState& State)
	{
		State.Variable = IConsoleManager::Get().FindConsoleVariable(State.Name);
		if (State.Variable == nullptr)
		{
			return;
		}

		State.OriginalFlags = State.Variable->GetFlags();
		const uint32 SetByMaskBits = static_cast<uint32>(ECVF_SetByMask);
		State.OriginalSetBy = static_cast<EConsoleVariableFlags>(static_cast<uint32>(State.OriginalFlags) & SetByMaskBits);
		State.OriginalPersistentFlags = static_cast<EConsoleVariableFlags>(static_cast<uint32>(State.OriginalFlags) & ~SetByMaskBits);
		State.OriginalInt = State.Variable->GetInt();
		State.OriginalFloat = State.Variable->GetFloat();
		State.OriginalString = State.Variable->GetString();
	}

	static void RestoreCapturedStates(TArray<FRestoreCVarState>& States)
	{
		for (FRestoreCVarState& State : States)
		{
			if (State.Variable == nullptr)
			{
				continue;
			}

			State.Variable->SetFlags(State.OriginalPersistentFlags);
			switch (State.Type)
			{
			case ERestoreCVarType::Int:
				State.Variable->Set(State.OriginalInt, State.OriginalSetBy);
				break;
			case ERestoreCVarType::Float:
				State.Variable->Set(State.OriginalFloat, State.OriginalSetBy);
				break;
			case ERestoreCVarType::String:
				State.Variable->Set(*State.OriginalString, State.OriginalSetBy);
				break;
			default:
				break;
			}
			State.Variable->SetFlags(State.OriginalFlags);
		}
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CVarGetSetAllTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageConsoleScope ConsoleScope(*TestRunner);

		const FString IntName = ConsoleScope.MakeName(TEXT("int"));
		const FString FloatName = ConsoleScope.MakeName(TEXT("float"));
		const FString BoolName = ConsoleScope.MakeName(TEXT("bool"));
		const FString StringName = ConsoleScope.MakeName(TEXT("string"));

		const FString ScriptSource = MakeScriptSource(
			ASTEST_AS(R"AS(
				int ReadWriteInt()
				{
					FConsoleVariable Var("$ARG0$", 5, "Coverage int cvar");
					if (Var.GetInt() != 5)
					{
						return 0;
					}
					Var.SetInt(42);
					return Var.GetInt();
				}

				float ReadWriteFloat()
				{
					FConsoleVariable Var("$ARG1$", 1.5f, "Coverage float cvar");
					if (Math::Abs(Var.GetFloat() - 1.5f) > 0.001f)
					{
						return 0.0f;
					}
					Var.SetFloat(3.25f);
					return Var.GetFloat();
				}

				int ReadWriteBool()
				{
					FConsoleVariable Var("$ARG2$", true, "Coverage bool cvar");
					if (!Var.GetBool())
					{
						return 0;
					}
					Var.SetBool(false);
					return Var.GetBool() ? 0 : 1;
				}

				int ReadWriteString()
				{
					FConsoleVariable Var("$ARG3$", "DefaultValue", "Coverage string cvar");
					if (Var.GetString() != "DefaultValue")
					{
						return 0;
					}
					Var.SetString("UpdatedValue");
					return Var.GetString() == "UpdatedValue" ? 1 : 0;
				}
				)AS"),
			{ &IntName, &FloatName, &BoolName, &StringName });

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageCVar_GetSetAllTypes"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CVar get/set module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ReadWriteInt()"),
			TEXT("FConsoleVariable should read and write int values"), 42)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float ReadWriteFloat()"),
			TEXT("FConsoleVariable should read and write float values"), 3.25f, 0.001f)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ReadWriteBool()"),
			TEXT("FConsoleVariable should read and write bool values"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ReadWriteString()"),
			TEXT("FConsoleVariable should read and write string values"), 1)));

		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(IntName, 42, TEXT("CVarGetSetAllTypes native int"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyFloat(FloatName, 3.25f, TEXT("CVarGetSetAllTypes native float"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyBool(BoolName, false, TEXT("CVarGetSetAllTypes native bool"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyString(StringName, TEXT("UpdatedValue"), TEXT("CVarGetSetAllTypes native string"))));
	}

	TEST_METHOD(CVarSafeAccessAndExistingVariable)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageConsoleScope ConsoleScope(*TestRunner);

		const FString ExistingName = ConsoleScope.MakeName(TEXT("existing"));
		IConsoleVariable* ExistingVariable = ConsoleScope.RegisterIntVariable(
			ExistingName,
			7,
			TEXT("Coverage existing cvar"));
		ASSERT_THAT(IsNotNull(ExistingVariable, TEXT("Existing native CVar should be registered")));

		const FString GeneratedName = ConsoleScope.MakeName(TEXT("safe.generated"));
		const FString ScriptSource = MakeScriptSource(
			ASTEST_AS(R"AS(
				int ReadExisting()
				{
					FConsoleVariable Var("$ARG0$", 99, "Should reuse existing native variable");
					return Var.GetInt();
				}

				int UpdateExisting()
				{
					FConsoleVariable Var("$ARG0$", 99, "Should reuse existing native variable");
					Var.SetInt(21);
					return Var.GetInt();
				}

				int SafeFallback()
				{
					FConsoleVariable Missing("$ARG1$", 123, "Generated fallback");
					if (Missing.GetInt() != 123)
					{
						return 0;
					}
					Missing.SetInt(456);
					return Missing.GetInt() == 456 ? 1 : 0;
				}
				)AS"),
			{ &ExistingName, &GeneratedName });

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageCVar_SafeExisting"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CVar safe access module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ReadExisting()"),
			TEXT("FConsoleVariable should reuse an existing native CVar"), 7)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int UpdateExisting()"),
			TEXT("FConsoleVariable should update an existing native CVar"), 21)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int SafeFallback()"),
			TEXT("FConsoleVariable should support safe default registration"), 1)));

		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(ExistingName, 21, TEXT("CVarSafeAccessAndExistingVariable native existing"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(GeneratedName, 456, TEXT("CVarSafeAccessAndExistingVariable native generated"))));
	}

	TEST_METHOD(CVarExistingVariablePreservesNativeMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageConsoleScope ConsoleScope(*TestRunner);

		const FString ExistingName = ConsoleScope.MakeName(TEXT("existing.metadata"));
		IConsoleVariable* ExistingVariable = ConsoleScope.RegisterIntVariable(
			ExistingName,
			7,
			TEXT("Coverage existing native cvar metadata"),
			ECVF_Cheat);
		ASSERT_THAT(IsNotNull(ExistingVariable, TEXT("Existing metadata native CVar should be registered")));
		if (ExistingVariable == nullptr)
		{
			return;
		}

		const FString ExistingHelp = ExistingVariable->GetHelp();
		const EConsoleVariableFlags ExistingFlags = ExistingVariable->GetFlags();
		const FString ScriptSource = MakeScriptSource(
			ASTEST_AS(R"AS(
				int ReadExistingWithDifferentDefaults()
				{
					FConsoleVariable Var("$ARG0$", 99, "Script default should not replace native CVar metadata");
					return Var.GetInt();
				}

				int UpdateExistingWithDifferentDefaults()
				{
					FConsoleVariable Var("$ARG0$", 99, "Script default should not replace native CVar metadata");
					Var.SetInt(21);
					return Var.GetInt();
				}
				)AS"),
			{ &ExistingName });

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageCVar_ExistingMetadata"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("CVar existing metadata module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ReadExistingWithDifferentDefaults()"),
			TEXT("FConsoleVariable should reuse the native value instead of the script default"), 7)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int UpdateExistingWithDifferentDefaults()"),
			TEXT("FConsoleVariable should still update the reused native CVar"), 21)));

		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(ExistingName, 21, TEXT("CVarExistingVariablePreservesNativeMetadata native value"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyIdentity(
			ExistingName,
			ExistingVariable,
			ExistingHelp,
			ExistingFlags,
			TEXT("CVarExistingVariablePreservesNativeMetadata native metadata"))));
		ASSERT_THAT(IsTrue(ExistingVariable->TestFlags(ECVF_Cheat),
			TEXT("existing CVar should preserve the native cheat flag after script reuse")));
	}

	TEST_METHOD(ExistingEngineCVarSmokePreservesAndRestoresValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		IConsoleVariable* MaxFPSVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
		ASSERT_THAT(IsNotNull(MaxFPSVariable, TEXT("native engine CVar t.MaxFPS should exist for existing-CVar smoke")));
		if (MaxFPSVariable == nullptr)
		{
			return;
		}

		const float OriginalMaxFPS = MaxFPSVariable->GetFloat();
		const EConsoleVariableFlags OriginalFlags = MaxFPSVariable->GetFlags();
		const uint32 SetByMaskBits = static_cast<uint32>(ECVF_SetByMask);
		const EConsoleVariableFlags OriginalSetBy = static_cast<EConsoleVariableFlags>(static_cast<uint32>(OriginalFlags) & SetByMaskBits);
		const EConsoleVariableFlags OriginalPersistentFlags = static_cast<EConsoleVariableFlags>(static_cast<uint32>(OriginalFlags) & ~SetByMaskBits);
		ON_SCOPE_EXIT
		{
			MaxFPSVariable->SetFlags(OriginalPersistentFlags);
			MaxFPSVariable->Set(OriginalMaxFPS, OriginalSetBy);
			MaxFPSVariable->SetFlags(OriginalFlags);
		};

		const float NativeBaselineMaxFPS = 61.0f;
		MaxFPSVariable->SetFlags(OriginalPersistentFlags);
		MaxFPSVariable->Set(NativeBaselineMaxFPS, ECVF_SetByCode);
		const bool bBaselineApplied = FMath::IsNearlyEqual(NativeBaselineMaxFPS, MaxFPSVariable->GetFloat(), 0.001f);
		ASSERT_THAT(IsTrue(bBaselineApplied, TEXT("native t.MaxFPS baseline should be writable before AS smoke")));
		if (!bBaselineApplied)
		{
			return;
		}

		const FString ScriptSource = ASTEST_AS(R"AS(
			float ReadExistingMaxFPS()
			{
				FConsoleVariable MaxFPS("t.MaxFPS", 12.0f, "Script default should not replace native t.MaxFPS");
				return MaxFPS.GetFloat();
			}

			float WriteExistingMaxFPS()
			{
				FConsoleVariable MaxFPS("t.MaxFPS", 12.0f, "Script default should not replace native t.MaxFPS");
				MaxFPS.SetFloat(83.0f);
				return MaxFPS.GetFloat();
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageCVar_ExistingEngineMaxFPS"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("existing engine CVar smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float ReadExistingMaxFPS()"),
			TEXT("FConsoleVariable should read existing native t.MaxFPS instead of script default"), NativeBaselineMaxFPS, 0.001f)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float WriteExistingMaxFPS()"),
			TEXT("FConsoleVariable should write through to existing native t.MaxFPS"), 83.0f, 0.001f)));

		IConsoleVariable* MaxFPSAfterScript = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
		ASSERT_THAT(IsNotNull(MaxFPSAfterScript, TEXT("native engine CVar t.MaxFPS should still exist after AS smoke")));
		if (MaxFPSAfterScript == nullptr)
		{
			return;
		}

		const bool bPreservedNativePointer = MaxFPSAfterScript == MaxFPSVariable;
		ASSERT_THAT(IsTrue(bPreservedNativePointer,
			TEXT("FConsoleVariable should preserve the native t.MaxFPS IConsoleVariable pointer")));
		if (!bPreservedNativePointer)
		{
			return;
		}

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(83.0f, MaxFPSVariable->GetFloat(), 0.001f),
			TEXT("native t.MaxFPS should contain the script-written smoke value before restore")));
	}

	TEST_METHOD(ExistingEngineRenderAndScalabilityCVarsPreserveAndRestoreValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FRestoreCVarState> RestoreStates = {
			{ TEXT("t.MaxFPS"), ERestoreCVarType::Float },
			{ TEXT("r.ScreenPercentage"), ERestoreCVarType::Float },
			{ TEXT("r.VSync"), ERestoreCVarType::Int },
			{ TEXT("r.ViewDistanceScale"), ERestoreCVarType::Float },
			{ TEXT("r.ShadowQuality"), ERestoreCVarType::Int },
			{ TEXT("sg.ResolutionQuality"), ERestoreCVarType::Float },
			{ TEXT("sg.ViewDistanceQuality"), ERestoreCVarType::Int },
			{ TEXT("sg.AntiAliasingQuality"), ERestoreCVarType::Int },
			{ TEXT("sg.ShadowQuality"), ERestoreCVarType::Int },
			{ TEXT("sg.PostProcessQuality"), ERestoreCVarType::Int },
			{ TEXT("sg.TextureQuality"), ERestoreCVarType::Int },
			{ TEXT("sg.EffectsQuality"), ERestoreCVarType::Int },
			{ TEXT("sg.FoliageQuality"), ERestoreCVarType::Int }
		};

		for (FRestoreCVarState& State : RestoreStates)
		{
			CaptureRestoreState(State);
			ASSERT_THAT(IsNotNull(State.Variable, *FString::Printf(TEXT("native engine CVar '%s' should exist"), State.Name)));
			if (State.Variable == nullptr)
			{
				return;
			}
		}

		ON_SCOPE_EXIT
		{
			RestoreCapturedStates(RestoreStates);
		};

		for (FRestoreCVarState& State : RestoreStates)
		{
			State.Variable->SetFlags(State.OriginalPersistentFlags);
		}

		const FString ScriptSource = ASTEST_AS(R"AS(
			float ApplyExistingFloatCVars()
			{
				FConsoleVariable MaxFPS("t.MaxFPS", 0.0f, "max fps");
				FConsoleVariable ScreenPercentage("r.ScreenPercentage", 100.0f, "screen percentage");
				FConsoleVariable ViewDistanceScale("r.ViewDistanceScale", 1.0f, "view distance scale");
				FConsoleVariable ResolutionQuality("sg.ResolutionQuality", 100.0f, "resolution quality");
				FConsoleVariable RenderShadowQuality("r.ShadowQuality", 3, "render shadow quality");

				MaxFPS.SetFloat(90.0f);
				ScreenPercentage.SetFloat(80.0f);
				ViewDistanceScale.SetFloat(1.25f);
				ResolutionQuality.SetFloat(70.0f);
				RenderShadowQuality.SetInt(2);

				return MaxFPS.GetFloat()
					+ ScreenPercentage.GetFloat()
					+ ViewDistanceScale.GetFloat()
					+ ResolutionQuality.GetFloat()
					+ RenderShadowQuality.GetInt();
			}

			int ApplyExistingIntCVars()
			{
				FConsoleVariable VSync("r.VSync", 0, "vsync");
				FConsoleVariable ViewDistanceQuality("sg.ViewDistanceQuality", 3, "view distance quality");
				FConsoleVariable AntiAliasingQuality("sg.AntiAliasingQuality", 3, "anti aliasing quality");
				FConsoleVariable ShadowQuality("sg.ShadowQuality", 3, "shadow quality");
				FConsoleVariable PostProcessQuality("sg.PostProcessQuality", 3, "post process quality");
				FConsoleVariable TextureQuality("sg.TextureQuality", 3, "texture quality");
				FConsoleVariable EffectsQuality("sg.EffectsQuality", 3, "effects quality");
				FConsoleVariable FoliageQuality("sg.FoliageQuality", 3, "foliage quality");

				VSync.SetInt(1);
				ViewDistanceQuality.SetInt(1);
				AntiAliasingQuality.SetInt(2);
				ShadowQuality.SetInt(3);
				PostProcessQuality.SetInt(2);
				TextureQuality.SetInt(1);
				EffectsQuality.SetInt(2);
				FoliageQuality.SetInt(3);

				return VSync.GetInt()
					+ ViewDistanceQuality.GetInt()
					+ AntiAliasingQuality.GetInt()
					+ ShadowQuality.GetInt()
					+ PostProcessQuality.GetInt()
					+ TextureQuality.GetInt()
					+ EffectsQuality.GetInt()
					+ FoliageQuality.GetInt();
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageCVar_ExistingRenderScalability"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("existing render/scalability CVar module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float ApplyExistingFloatCVars()"),
			TEXT("existing float/render CVars should read/write through FConsoleVariable"), 243.25f, 0.001f)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ApplyExistingIntCVars()"),
			TEXT("existing int scalability CVars should read/write through FConsoleVariable"), 15)));

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(90.0f, IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))->GetFloat(), 0.001f),
			TEXT("native t.MaxFPS should contain the script-written value before restore")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(80.0f, IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->GetFloat(), 0.001f),
			TEXT("native r.ScreenPercentage should contain the script-written value before restore")));
		ASSERT_THAT(AreEqual(2, IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShadowQuality"))->GetInt(),
			TEXT("native r.ShadowQuality should contain the script-written value before restore")));
		ASSERT_THAT(AreEqual(3, IConsoleManager::Get().FindConsoleVariable(TEXT("sg.FoliageQuality"))->GetInt(),
			TEXT("native sg.FoliageQuality should contain the script-written value before restore")));
	}

	TEST_METHOD(RegisteredCVarNameMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageConsoleScope ConsoleScope(*TestRunner);

		const FString ScreenPercentageName = ConsoleScope.MakeName(TEXT("matrix.r.ScreenPercentage"));
		const FString ViewDistanceScaleName = ConsoleScope.MakeName(TEXT("matrix.r.ViewDistanceScale"));
		const FString VSyncName = ConsoleScope.MakeName(TEXT("matrix.r.VSync"));
		const FString ShadowQualityName = ConsoleScope.MakeName(TEXT("matrix.r.ShadowQuality"));
		const FString MaxFPSName = ConsoleScope.MakeName(TEXT("matrix.t.MaxFPS"));
		const FString ResolutionName = ConsoleScope.MakeName(TEXT("matrix.r.SetRes"));
		const FString ResolutionQualityName = ConsoleScope.MakeName(TEXT("matrix.sg.ResolutionQuality"));
		const FString ViewDistanceQualityName = ConsoleScope.MakeName(TEXT("matrix.sg.ViewDistanceQuality"));
		const FString AntiAliasingQualityName = ConsoleScope.MakeName(TEXT("matrix.sg.AntiAliasingQuality"));
		const FString SGShadowQualityName = ConsoleScope.MakeName(TEXT("matrix.sg.ShadowQuality"));
		const FString PostProcessQualityName = ConsoleScope.MakeName(TEXT("matrix.sg.PostProcessQuality"));
		const FString TextureQualityName = ConsoleScope.MakeName(TEXT("matrix.sg.TextureQuality"));
		const FString EffectsQualityName = ConsoleScope.MakeName(TEXT("matrix.sg.EffectsQuality"));
		const FString FoliageQualityName = ConsoleScope.MakeName(TEXT("matrix.sg.FoliageQuality"));

		const FString ScriptSource = MakeScriptSource(
			ASTEST_AS(R"AS(
				float ApplyRenderScaleCVars()
				{
					FConsoleVariable ScreenPercentage("$ARG0$", 100.0f, "screen percentage");
					FConsoleVariable ViewDistanceScale("$ARG1$", 1.0f, "view distance scale");
					ScreenPercentage.SetFloat(77.0f);
					ViewDistanceScale.SetFloat(1.5f);
					return ScreenPercentage.GetFloat() + ViewDistanceScale.GetFloat();
				}

				int ApplyRenderQualityCVars()
				{
					FConsoleVariable VSync("$ARG2$", 0, "vsync");
					FConsoleVariable ShadowQuality("$ARG3$", 3, "shadow quality");
					VSync.SetInt(1);
					ShadowQuality.SetInt(2);
					return VSync.GetInt() * 10 + ShadowQuality.GetInt();
				}

				float ApplyPerformanceCVar()
				{
					FConsoleVariable MaxFPS("$ARG4$", 0.0f, "max fps");
					MaxFPS.SetFloat(144.0f);
					return MaxFPS.GetFloat();
				}

				int ApplyResolutionStringCVar()
				{
					FConsoleVariable Resolution("$ARG5$", "1280x720w", "resolution command");
					Resolution.SetString("1920x1080w");
					return Resolution.GetString() == "1920x1080w" ? 1 : 0;
				}

				float ApplyScalabilityCVars()
				{
					FConsoleVariable ResolutionQuality("$ARG6$", 100.0f, "resolution quality");
					FConsoleVariable ViewDistanceQuality("$ARG7$", 3, "view distance quality");
					FConsoleVariable AntiAliasingQuality("$ARG8$", 3, "anti aliasing quality");
					FConsoleVariable ShadowQuality("$ARG9$", 3, "shadow quality");
					FConsoleVariable PostProcessQuality("$ARG10$", 3, "post process quality");
					FConsoleVariable TextureQuality("$ARG11$", 3, "texture quality");
					FConsoleVariable EffectsQuality("$ARG12$", 3, "effects quality");
					FConsoleVariable FoliageQuality("$ARG13$", 3, "foliage quality");

					ResolutionQuality.SetFloat(85.0f);
					ViewDistanceQuality.SetInt(1);
					AntiAliasingQuality.SetInt(2);
					ShadowQuality.SetInt(3);
					PostProcessQuality.SetInt(4);
					TextureQuality.SetInt(1);
					EffectsQuality.SetInt(2);
					FoliageQuality.SetInt(3);

					return ResolutionQuality.GetFloat()
						+ ViewDistanceQuality.GetInt()
						+ AntiAliasingQuality.GetInt()
						+ ShadowQuality.GetInt()
						+ PostProcessQuality.GetInt()
						+ TextureQuality.GetInt()
						+ EffectsQuality.GetInt()
						+ FoliageQuality.GetInt();
				}
				)AS"),
			{
				&ScreenPercentageName, &ViewDistanceScaleName, &VSyncName, &ShadowQualityName,
				&MaxFPSName, &ResolutionName, &ResolutionQualityName, &ViewDistanceQualityName,
				&AntiAliasingQualityName, &SGShadowQualityName, &PostProcessQualityName,
				&TextureQualityName, &EffectsQualityName, &FoliageQualityName
			});

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageCVar_RegisteredNameMatrix"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("registered CVar name matrix module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float ApplyRenderScaleCVars()"),
			TEXT("registered render float CVars should be script-readable and script-writable"), 78.5f, 0.001f)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ApplyRenderQualityCVars()"),
			TEXT("registered render int CVars should be script-readable and script-writable"), 12)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float ApplyPerformanceCVar()"),
			TEXT("registered performance CVar should be script-readable and script-writable"), 144.0f, 0.001f)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ApplyResolutionStringCVar()"),
			TEXT("registered resolution CVar should read and write string values"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float ApplyScalabilityCVars()"),
			TEXT("registered scalability quality CVars should be script-readable and script-writable"), 101.0f, 0.001f)));

		ASSERT_THAT(IsTrue(ConsoleScope.VerifyFloat(ScreenPercentageName, 77.0f, TEXT("RegisteredCVarNameMatrix screen percentage"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyFloat(ViewDistanceScaleName, 1.5f, TEXT("RegisteredCVarNameMatrix view distance"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(VSyncName, 1, TEXT("RegisteredCVarNameMatrix vsync"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(ShadowQualityName, 2, TEXT("RegisteredCVarNameMatrix shadow quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyFloat(MaxFPSName, 144.0f, TEXT("RegisteredCVarNameMatrix max fps"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyString(ResolutionName, TEXT("1920x1080w"), TEXT("RegisteredCVarNameMatrix resolution"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyFloat(ResolutionQualityName, 85.0f, TEXT("RegisteredCVarNameMatrix resolution quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(FoliageQualityName, 3, TEXT("RegisteredCVarNameMatrix foliage quality"))));
	}

	TEST_METHOD(CommonCVarUsagePatterns)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageConsoleScope ConsoleScope(*TestRunner);

		const FString ScreenPercentageName = ConsoleScope.MakeName(TEXT("r.ScreenPercentage"));
		const FString VSyncName = ConsoleScope.MakeName(TEXT("r.VSync"));
		const FString ViewDistanceScaleName = ConsoleScope.MakeName(TEXT("r.ViewDistanceScale"));
		const FString ShadowQualityName = ConsoleScope.MakeName(TEXT("r.ShadowQuality"));
		const FString PostProcessQualityName = ConsoleScope.MakeName(TEXT("r.PostProcessQuality"));
		const FString TextureQualityName = ConsoleScope.MakeName(TEXT("r.TextureQuality"));
		const FString EffectsQualityName = ConsoleScope.MakeName(TEXT("r.EffectsQuality"));
		const FString FoliageQualityName = ConsoleScope.MakeName(TEXT("r.FoliageQuality"));
		const FString MaxFPSName = ConsoleScope.MakeName(TEXT("t.MaxFPS"));
		const FString ResolutionName = ConsoleScope.MakeName(TEXT("r.SetRes"));
		const FString ShowCollisionName = ConsoleScope.MakeName(TEXT("ShowFlag.Collision"));
		const FString ShowBoundsName = ConsoleScope.MakeName(TEXT("ShowFlag.Bounds"));
		const FString StatFPSName = ConsoleScope.MakeName(TEXT("stat.FPS"));
		const FString StatUnitName = ConsoleScope.MakeName(TEXT("stat.Unit"));
		const FString ResolutionQualityName = ConsoleScope.MakeName(TEXT("sg.ResolutionQuality"));
		const FString ViewDistanceQualityName = ConsoleScope.MakeName(TEXT("sg.ViewDistanceQuality"));
		const FString AntiAliasingQualityName = ConsoleScope.MakeName(TEXT("sg.AntiAliasingQuality"));
		const FString SGShadowQualityName = ConsoleScope.MakeName(TEXT("sg.ShadowQuality"));
		const FString SGPostProcessQualityName = ConsoleScope.MakeName(TEXT("sg.PostProcessQuality"));
		const FString SGTextureQualityName = ConsoleScope.MakeName(TEXT("sg.TextureQuality"));
		const FString SGEffectsQualityName = ConsoleScope.MakeName(TEXT("sg.EffectsQuality"));
		const FString SGFoliageQualityName = ConsoleScope.MakeName(TEXT("sg.FoliageQuality"));

		const FString ScriptSource = MakeScriptSource(
			ASTEST_AS(R"AS(
				float ApplyRenderScale()
				{
					FConsoleVariable ScreenPercentage("$ARG0$", 100.0f, "screen percentage");
					FConsoleVariable ViewDistanceScale("$ARG2$", 1.0f, "view distance scale");
					ScreenPercentage.SetFloat(75.0f);
					ViewDistanceScale.SetFloat(1.25f);
					return ScreenPercentage.GetFloat() + ViewDistanceScale.GetFloat();
				}

				int ApplyRenderQuality()
				{
					FConsoleVariable VSync("$ARG1$", 0, "vsync");
					FConsoleVariable ShadowQuality("$ARG3$", 3, "shadow quality");
					FConsoleVariable PostProcessQuality("$ARG4$", 3, "post process quality");
					FConsoleVariable TextureQuality("$ARG5$", 3, "texture quality");
					FConsoleVariable EffectsQuality("$ARG6$", 3, "effects quality");
					FConsoleVariable FoliageQuality("$ARG7$", 3, "foliage quality");
					VSync.SetInt(1);
					ShadowQuality.SetInt(2);
					PostProcessQuality.SetInt(2);
					TextureQuality.SetInt(2);
					EffectsQuality.SetInt(2);
					FoliageQuality.SetInt(2);
					return VSync.GetInt()
						+ ShadowQuality.GetInt()
						+ PostProcessQuality.GetInt()
						+ TextureQuality.GetInt()
						+ EffectsQuality.GetInt()
						+ FoliageQuality.GetInt();
				}

				float ApplyPerformanceSettings()
				{
					FConsoleVariable MaxFPS("$ARG8$", 0.0f, "max fps");
					FConsoleVariable Resolution("$ARG9$", "1280x720w", "resolution");
					MaxFPS.SetFloat(120.0f);
					Resolution.SetString("1920x1080w");
					return MaxFPS.GetFloat() + (Resolution.GetString() == "1920x1080w" ? 1.0f : 0.0f);
				}

				int ApplyDebugToggles()
				{
					FConsoleVariable ShowCollision("$ARG10$", 0, "show collision");
					FConsoleVariable ShowBounds("$ARG11$", 0, "show bounds");
					FConsoleVariable StatFPS("$ARG12$", 0, "stat fps");
					FConsoleVariable StatUnit("$ARG13$", 0, "stat unit");
					ShowCollision.SetInt(1);
					ShowBounds.SetInt(1);
					StatFPS.SetInt(1);
					StatUnit.SetInt(1);
					return ShowCollision.GetInt() + ShowBounds.GetInt() + StatFPS.GetInt() + StatUnit.GetInt();
				}

				int ApplyScalabilitySettings(int Quality)
				{
					FConsoleVariable ResolutionQuality("$ARG14$", 100, "resolution quality");
					FConsoleVariable ViewDistanceQuality("$ARG15$", 3, "view distance quality");
					FConsoleVariable AntiAliasingQuality("$ARG16$", 3, "anti aliasing quality");
					FConsoleVariable ShadowQuality("$ARG17$", 3, "shadow quality");
					FConsoleVariable PostProcessQuality("$ARG18$", 3, "post process quality");
					FConsoleVariable TextureQuality("$ARG19$", 3, "texture quality");
					FConsoleVariable EffectsQuality("$ARG20$", 3, "effects quality");
					FConsoleVariable FoliageQuality("$ARG21$", 3, "foliage quality");
					ResolutionQuality.SetInt(85);
					ViewDistanceQuality.SetInt(Quality);
					AntiAliasingQuality.SetInt(Quality);
					ShadowQuality.SetInt(Quality);
					PostProcessQuality.SetInt(Quality);
					TextureQuality.SetInt(Quality);
					EffectsQuality.SetInt(Quality);
					FoliageQuality.SetInt(Quality);
					return ResolutionQuality.GetInt()
						+ ViewDistanceQuality.GetInt()
						+ AntiAliasingQuality.GetInt()
						+ ShadowQuality.GetInt()
						+ PostProcessQuality.GetInt()
						+ TextureQuality.GetInt()
						+ EffectsQuality.GetInt()
						+ FoliageQuality.GetInt();
				}

				int ApplyHighQuality()
				{
					return ApplyScalabilitySettings(3);
				}
				)AS"),
			{
				&ScreenPercentageName, &VSyncName, &ViewDistanceScaleName, &ShadowQualityName,
				&PostProcessQualityName, &TextureQualityName, &EffectsQualityName, &FoliageQualityName,
				&MaxFPSName, &ResolutionName, &ShowCollisionName, &ShowBoundsName, &StatFPSName, &StatUnitName,
				&ResolutionQualityName, &ViewDistanceQualityName, &AntiAliasingQualityName, &SGShadowQualityName,
				&SGPostProcessQualityName, &SGTextureQualityName, &SGEffectsQualityName, &SGFoliageQualityName
			});

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageCVar_CommonUsage"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Common CVar usage module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float ApplyRenderScale()"),
			TEXT("render float CVars should read/write"), 76.25f, 0.001f)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ApplyRenderQuality()"),
			TEXT("render quality CVars should read/write"), 11)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectNearFloat(*TestRunner, Engine, ScriptModule, TEXT("float ApplyPerformanceSettings()"),
			TEXT("performance CVars should read/write"), 121.0f, 0.001f)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ApplyDebugToggles()"),
			TEXT("debug toggle CVars should read/write"), 4)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ApplyHighQuality()"),
			TEXT("scalability quality CVars should read/write"), 106)));

		ASSERT_THAT(IsTrue(ConsoleScope.VerifyFloat(ScreenPercentageName, 75.0f, TEXT("Common CVar native screen percentage"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(VSyncName, 1, TEXT("Common CVar native vsync"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyFloat(ViewDistanceScaleName, 1.25f, TEXT("Common CVar native view distance scale"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(ShadowQualityName, 2, TEXT("Common CVar native shadow quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(PostProcessQualityName, 2, TEXT("Common CVar native post process quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(TextureQualityName, 2, TEXT("Common CVar native texture quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(EffectsQualityName, 2, TEXT("Common CVar native effects quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(FoliageQualityName, 2, TEXT("Common CVar native foliage quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyFloat(MaxFPSName, 120.0f, TEXT("Common CVar native max fps"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyString(ResolutionName, TEXT("1920x1080w"), TEXT("Common CVar native resolution"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(ShowCollisionName, 1, TEXT("Common CVar native show collision"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(ShowBoundsName, 1, TEXT("Common CVar native show bounds"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(StatFPSName, 1, TEXT("Common CVar native stat fps"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(StatUnitName, 1, TEXT("Common CVar native stat unit"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(ResolutionQualityName, 85, TEXT("Common CVar native resolution quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(ViewDistanceQualityName, 3, TEXT("Common CVar native view distance quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(AntiAliasingQualityName, 3, TEXT("Common CVar native anti aliasing quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(SGShadowQualityName, 3, TEXT("Common CVar native sg shadow quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(SGPostProcessQualityName, 3, TEXT("Common CVar native sg post process quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(SGTextureQualityName, 3, TEXT("Common CVar native sg texture quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(SGEffectsQualityName, 3, TEXT("Common CVar native sg effects quality"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(SGFoliageQualityName, 3, TEXT("Common CVar native sg foliage quality"))));
	}

	TEST_METHOD(ConsoleCommandRegistrationArgumentsAndUnload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageConsoleScope ConsoleScope(*TestRunner);

		const FString CommandName = ConsoleScope.MakeName(TEXT("command"));
		const FString OutputName = ConsoleScope.MakeName(TEXT("output"));
		IConsoleVariable* OutputVariable = ConsoleScope.RegisterIntVariable(
			OutputName,
			-1,
			TEXT("Coverage command output"));
		ASSERT_THAT(IsNotNull(OutputVariable, TEXT("Output CVar should be registered")));

		const FString ScriptSource = MakeScriptSource(
			ASTEST_AS(R"AS(
				const FConsoleCommand Command("$ARG0$", n"OnCoverageCommand");

				void OnCoverageCommand(const TArray<FString>& Args)
				{
					FConsoleVariable Output("$ARG1$", 0, "Coverage command output");
					int Score = Args.Num();
					if (Args.Num() == 3 && Args[0] == "stat" && Args[1] == "fps" && Args[2] == "show")
					{
						Score += 10;
					}
					Output.SetInt(Score);
				}

				int CommandReady()
				{
					return 1;
				}
				)AS"),
			{ &CommandName, &OutputName });

		TUniquePtr<FScopedAngelscriptModule> Module = MakeUnique<FScopedAngelscriptModule>(
			*TestRunner,
			Engine,
			TEXT("ASCoverageCVar_Command"),
			ScriptSource);
		ASSERT_THAT(IsTrue(Module->IsValid(), TEXT("FConsoleCommand module should compile")));
		if (!Module->IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module->GetModule(), TEXT("int CommandReady()"),
			TEXT("FConsoleCommand module should initialize"), 1)));

		TArray<FString> Args;
		Args.Add(TEXT("stat"));
		Args.Add(TEXT("fps"));
		Args.Add(TEXT("show"));
		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommand(CommandName, Args, TEXT("ConsoleCommandRegistrationArgumentsAndUnload"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 13, TEXT("ConsoleCommandRegistrationArgumentsAndUnload output"))));

		Module.Reset();
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyCommandMissing(CommandName, TEXT("ConsoleCommandRegistrationArgumentsAndUnload"))));
	}

	TEST_METHOD(ConsoleCommandCommonStringMatrixDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageConsoleScope ConsoleScope(*TestRunner);

		const FString CommandName = ConsoleScope.MakeName(TEXT("command.matrix"));
		const FString OutputName = ConsoleScope.MakeName(TEXT("command.matrix.output"));
		IConsoleVariable* OutputVariable = ConsoleScope.RegisterIntVariable(
			OutputName,
			-1,
			TEXT("Coverage command matrix output"));
		ASSERT_THAT(IsNotNull(OutputVariable, TEXT("Command matrix output CVar should be registered")));

		const FString ScriptSource = MakeScriptSource(
			ASTEST_AS(R"AS(
				const FConsoleCommand Command("$ARG0$", n"OnCoverageCommandMatrix");

				void OnCoverageCommandMatrix(const TArray<FString>& Args)
				{
					FConsoleVariable Output("$ARG1$", 0, "Coverage command matrix output");
					int Score = -1;

					if (Args.Num() == 2 && Args[0] == "stat" && Args[1] == "fps")
					{
						Score = 101;
					}
					else if (Args.Num() == 2 && Args[0] == "stat" && Args[1] == "unit")
					{
						Score = 102;
					}
					else if (Args.Num() == 2 && Args[0] == "stat" && Args[1] == "game")
					{
						Score = 103;
					}
					else if (Args.Num() == 2 && Args[0] == "stat" && Args[1] == "gpu")
					{
						Score = 104;
					}
					else if (Args.Num() == 2 && Args[0] == "stat" && Args[1] == "memory")
					{
						Score = 105;
					}
					else if (Args.Num() == 2 && Args[0] == "stat" && Args[1] == "slow")
					{
						Score = 106;
					}
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "collision")
					{
						Score = 201;
					}
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "bounds")
					{
						Score = 202;
					}
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "bones")
					{
						Score = 203;
					}
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "navmesh")
					{
						Score = 204;
					}
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "paths")
					{
						Score = 205;
					}
					else if (Args.Num() == 2 && Args[0] == "viewmode" && Args[1] == "wireframe")
					{
						Score = 301;
					}
					else if (Args.Num() == 2 && Args[0] == "viewmode" && Args[1] == "unlit")
					{
						Score = 302;
					}
					else if (Args.Num() == 2 && Args[0] == "r.SetRes" && Args[1] == "1920x1080w")
					{
						Score = 401;
					}

					Output.SetInt(Score);
				}

				int CommandMatrixReady()
				{
					return 1;
				}
				)AS"),
			{ &CommandName, &OutputName });

		TUniquePtr<FScopedAngelscriptModule> Module = MakeUnique<FScopedAngelscriptModule>(
			*TestRunner,
			Engine,
			TEXT("ASCoverageCVar_CommandMatrix"),
			ScriptSource);
		ASSERT_THAT(IsTrue(Module->IsValid(), TEXT("FConsoleCommand common string matrix module should compile")));
		if (!Module->IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module->GetModule(), TEXT("int CommandMatrixReady()"),
			TEXT("FConsoleCommand common string matrix should initialize"), 1)));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("fps") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch stat fps"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 101, TEXT("ConsoleCommandCommonStringMatrixDispatch stat fps output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("unit") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch stat unit"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 102, TEXT("ConsoleCommandCommonStringMatrixDispatch stat unit output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("game") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch stat game"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 103, TEXT("ConsoleCommandCommonStringMatrixDispatch stat game output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("gpu") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch stat gpu"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 104, TEXT("ConsoleCommandCommonStringMatrixDispatch stat gpu output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("memory") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch stat memory"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 105, TEXT("ConsoleCommandCommonStringMatrixDispatch stat memory output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("slow") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch stat slow"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 106, TEXT("ConsoleCommandCommonStringMatrixDispatch stat slow output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("collision") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch show collision"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 201, TEXT("ConsoleCommandCommonStringMatrixDispatch show collision output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("bounds") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch show bounds"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 202, TEXT("ConsoleCommandCommonStringMatrixDispatch show bounds output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("bones") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch show bones"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 203, TEXT("ConsoleCommandCommonStringMatrixDispatch show bones output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("navmesh") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch show navmesh"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 204, TEXT("ConsoleCommandCommonStringMatrixDispatch show navmesh output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("paths") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch show paths"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 205, TEXT("ConsoleCommandCommonStringMatrixDispatch show paths output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("viewmode"), TEXT("wireframe") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch viewmode wireframe"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 301, TEXT("ConsoleCommandCommonStringMatrixDispatch viewmode wireframe output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("viewmode"), TEXT("unlit") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch viewmode unlit"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 302, TEXT("ConsoleCommandCommonStringMatrixDispatch viewmode unlit output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("r.SetRes"), TEXT("1920x1080w") },
			TEXT("ConsoleCommandCommonStringMatrixDispatch r.SetRes"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 401, TEXT("ConsoleCommandCommonStringMatrixDispatch r.SetRes output"))));

		Module.Reset();
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyCommandMissing(CommandName, TEXT("ConsoleCommandCommonStringMatrixDispatch"))));
	}

	TEST_METHOD(ConsoleCommandExecutionApisUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ExecuteConsoleCommandSource = ASTEST_AS(R"AS(
			void TryExecuteConsoleCommand()
			{
				ConsoleCommand("stat fps");
				ConsoleCommand("stat unit");
				ConsoleCommand("stat game");
				ConsoleCommand("stat gpu");
				ConsoleCommand("r.SetRes 1920x1080w");
				ConsoleCommand("show collision");
				ConsoleCommand("show bounds");
				ConsoleCommand("viewmode wireframe");
				ConsoleCommand("viewmode unlit");
			}
			)AS");
		const FString PlayerControllerConsoleCommandSource = ASTEST_AS(R"AS(
			void TryPlayerControllerConsoleCommand(APlayerController Controller)
			{
				Controller.ConsoleCommand("stat fps");
				Controller.ConsoleCommand("stat unit");
				Controller.ConsoleCommand("r.SetRes 1920x1080w");
				Controller.ConsoleCommand("show collision");
				Controller.ConsoleCommand("viewmode unlit");
			}
			)AS");

		const TArray<FString> GlobalExpectedFragments = { TEXT("ConsoleCommand") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageCVar_UnsupportedGlobalConsoleCommand"),
			*ExecuteConsoleCommandSource,
			TEXT("global ConsoleCommand execution helper is not script-facing"),
			MakeArrayView(GlobalExpectedFragments))));

		const TArray<FString> PlayerControllerExpectedFragments = { TEXT("ConsoleCommand") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageCVar_UnsupportedPlayerControllerConsoleCommand"),
			*PlayerControllerConsoleCommandSource,
			TEXT("APlayerController ConsoleCommand execution helper is not script-facing"),
			MakeArrayView(PlayerControllerExpectedFragments))));
	}

	TEST_METHOD(ConsoleCommandStringConstructionCompileBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int BuildCommonConsoleCommandStrings()
			{
				TArray<FString> Commands;
				Commands.Add("stat fps");
				Commands.Add("stat unit");
				Commands.Add("stat game");
				Commands.Add("stat gpu");
				Commands.Add("r.SetRes 1920x1080w");
				Commands.Add("show collision");
				Commands.Add("show bounds");
				Commands.Add("viewmode wireframe");
				Commands.Add("viewmode unlit");

				FString ResolutionCommand = "1920";
				ResolutionCommand += "x";
				ResolutionCommand += "1080";
				ResolutionCommand += "w";
				Commands.Add("r.SetRes " + ResolutionCommand);

				int Score = 0;
				for (int Index = 0; Index < Commands.Num(); ++Index)
				{
					if (Commands[Index].Len() > 0)
					{
						Score += 1;
					}
				}
				return Score;
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageCVar_CommandStringBoundary"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("common console command string boundary module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int BuildCommonConsoleCommandStrings()"),
			TEXT("common console command strings should be buildable without executing editor/viewport commands"), 10)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
