#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "HAL/IConsoleManager.h"
#include "Misc/Guid.h"
#include "Misc/OutputDeviceNull.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageCVarTests
// -----------------------------------------------------------------------------
// Coverage for Documents/Coverage/Coverage_CVar.md.
//
// The AngelScript-facing API is FConsoleVariable/FConsoleCommand, not direct
// IConsoleManager access. These cases verify script-side registration, reads,
// writes, common safe access patterns, command arguments, and command unload.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

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

		IConsoleVariable* RegisterIntVariable(const FString& Name, int32 Value, const TCHAR* Help)
		{
			TrackName(Name);
			return IConsoleManager::Get().RegisterConsoleVariable(*Name, Value, Help);
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

	TEST_METHOD(ConsoleCommandExecutionApisUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ExecuteConsoleCommandSource = ASTEST_AS(R"AS(
			void TryExecuteConsoleCommand()
			{
				ConsoleCommand("stat fps");
			}
			)AS");
		const FString PlayerControllerConsoleCommandSource = ASTEST_AS(R"AS(
			void TryPlayerControllerConsoleCommand(APlayerController Controller)
			{
				Controller.ConsoleCommand("stat fps");
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
};

#endif // WITH_DEV_AUTOMATION_TESTS
