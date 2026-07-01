#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "Binds/Bind_Debugging.h"

#include "Components/ActorTestSpawner.h"
#include "CoreGlobals.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/Guid.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeExit.h"

#include <initializer_list>

// -----------------------------------------------------------------------------
// AngelscriptCoverageDebugTests
// -----------------------------------------------------------------------------
// Coverage for the currently exposed AngelScript debug helpers from:
//   * Documents/Coverage/Coverage_DebugAndLogging.md
//
// The runtime binding surface exposes DebugBreak, ensure/check variants,
// callstack helpers, throw, and DrawDebugStringFromObject. It does not expose
// the old System::DrawDebugLine/Sphere/Box/Capsule/Arrow helpers, so this file
// intentionally covers only the supported API surface.
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageDebugTest,
	"Angelscript.TestModule.Coverage.Debug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr TCHAR CoverageDebugConsolePrefix[] = TEXT("as.coverage.debug");

	struct FCapturedLogLine
	{
		FString Text;
		ELogVerbosity::Type Verbosity = ELogVerbosity::NoLogging;
		FName Category;
	};

	struct FCapturedLogDevice : FOutputDevice
	{
		TArray<FCapturedLogLine> Lines;

		void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			Lines.Add({ FString(Data), Verbosity, Category });
		}

		bool Contains(const FString& Text, ELogVerbosity::Type Verbosity, const FName& Category) const
		{
			for (const FCapturedLogLine& Line : Lines)
			{
				if (Line.Verbosity == Verbosity
					&& Line.Category == Category
					&& Line.Text.Contains(Text))
				{
					return true;
				}
			}

			return false;
		}

		bool ContainsText(const FString& Text) const
		{
			for (const FCapturedLogLine& Line : Lines)
			{
				if (Line.Text.Contains(Text))
				{
					return true;
				}
			}

			return false;
		}
	};

	struct FScopedCoverageDebugConsole
	{
		explicit FScopedCoverageDebugConsole(FAutomationTestBase& InTest)
			: Assert(InTest)
		{
		}

		~FScopedCoverageDebugConsole()
		{
			for (const FString& Name : RegisteredNames)
			{
				if (IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(*Name))
				{
					IConsoleManager::Get().UnregisterConsoleObject(ConsoleObject);
				}
			}
		}

		FScopedCoverageDebugConsole(const FScopedCoverageDebugConsole&) = delete;
		FScopedCoverageDebugConsole& operator=(const FScopedCoverageDebugConsole&) = delete;

		FString MakeName(const TCHAR* Kind)
		{
			FString Name = FString::Printf(
				TEXT("%s.%s.%s"),
				CoverageDebugConsolePrefix,
				Kind,
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			RegisteredNames.AddUnique(Name);
			return Name;
		}

		IConsoleVariable* RegisterIntVariable(const FString& Name, int32 Value, const TCHAR* Help)
		{
			RegisteredNames.AddUnique(Name);
			return IConsoleManager::Get().RegisterConsoleVariable(*Name, Value, Help);
		}

		bool ExecuteCommandArgs(const FString& Name, std::initializer_list<const TCHAR*> Args, const TCHAR* ContextLabel)
		{
			IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(*Name);
			IConsoleCommand* Command = ConsoleObject != nullptr ? ConsoleObject->AsCommand() : nullptr;
			if (!Assert.IsNotNull(Command, *FString::Printf(TEXT("%s should find command '%s'"), ContextLabel, *Name)))
			{
				return false;
			}

			TArray<FString> CommandArgs;
			for (const TCHAR* Arg : Args)
			{
				CommandArgs.Add(Arg);
			}

			FOutputDeviceNull OutputDevice;
			return Assert.IsTrue(
				Command->Execute(CommandArgs, nullptr, OutputDevice),
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

	private:
		FNoDiscardAsserter Assert;
		TArray<FString> RegisteredNames;
	};

	struct FScopedAngelscriptDebugBreakOverride
	{
		FScopedAngelscriptDebugBreakOverride()
			: bWasEnabled(AreAngelscriptDebugBreaksEnabledForTesting())
		{
			AngelscriptDisableDebugBreaks();
			AngelscriptForgetSeenEnsures();
		}

		~FScopedAngelscriptDebugBreakOverride()
		{
			AngelscriptForgetSeenEnsures();
			if (bWasEnabled)
			{
				AngelscriptEnableDebugBreaks();
			}
			else
			{
				AngelscriptDisableDebugBreaks();
			}
		}

	private:
		bool bWasEnabled = true;
	};

	static FString MakeDebugConsoleScriptSource(FString Source, const FString& CommandName, const FString& OutputName)
	{
		Source.ReplaceInline(TEXT("$COMMAND$"), *CommandName, ESearchCase::CaseSensitive);
		Source.ReplaceInline(TEXT("$OUTPUT$"), *OutputName, ESearchCase::CaseSensitive);
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

	TEST_METHOD(EnsureAndCheckBindings)
	{
		TestRunner->AddExpectedError(TEXT("Ensure condition failed"), EAutomationExpectedErrorFlags::Contains, 3);

		FScopedAngelscriptDebugBreakOverride DebugBreakOverride;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageDebug_AssertBindings"), ASTEST_AS(R"AS(
			int AssertBindingSmoke()
			{
				int Score = 0;

				if (ensure(true))
				{
					Score += 1;
				}
				if (!ensure(false))
				{
					Score += 2;
				}
				if (ensure(true, "CoverageEnsureMessagePass"))
				{
					Score += 4;
				}
				if (!ensure(false, "CoverageEnsureMessage"))
				{
					Score += 8;
				}
				if (!ensureAlways(false, "CoverageEnsureAlways"))
				{
					Score += 16;
				}

				check(true);
				check(true, "CoverageCheckMessagePass");

				return Score;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("ensure/check module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int AssertBindingSmoke()"),
			TEXT("ensure/check bindings should return condition state and allow passing check calls"), 31)));
	}

	TEST_METHOD(LogSeverityHelpersEmitExpectedVerbosity)
	{
		TestRunner->AddExpectedError(TEXT("CoverageDebugLog_Error"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("CoverageDebugLog_CategoryError"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageDebug_LogSeverityHelpers"), ASTEST_AS(R"AS(
			int EmitDebugLogSeverity()
			{
				Log("CoverageDebugLog_Log");
				LogInfo("CoverageDebugLog_Info");
				LogDisplay("CoverageDebugLog_Display");
				Warning("CoverageDebugLog_Warning");
				Error("CoverageDebugLog_Error");

				Log(n"CoverageDebugLogCategory", "CoverageDebugLog_CategoryLog");
				LogDisplay(n"CoverageDebugLogCategory", "CoverageDebugLog_CategoryDisplay");
				Warning(n"CoverageDebugLogCategory", "CoverageDebugLog_CategoryWarning");
				Error(n"CoverageDebugLogCategory", "CoverageDebugLog_CategoryError");

				return 9;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("debug log severity module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		FCapturedLogDevice LogCapture;
		GLog->AddOutputDevice(&LogCapture);
		ON_SCOPE_EXIT
		{
			GLog->RemoveOutputDevice(&LogCapture);
		};

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int EmitDebugLogSeverity()"),
			TEXT("debug log severity helpers should execute"), 9)));
		GLog->FlushThreadedLogs();

		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageDebugLog_Display")),
			TEXT("LogDisplay should emit its message")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageDebugLog_Warning")),
			TEXT("Warning should emit its message")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageDebugLog_Error")),
			TEXT("Error should emit its message")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageDebugLog_CategoryDisplay")),
			TEXT("category LogDisplay overload should emit its message")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageDebugLog_CategoryWarning")),
			TEXT("category Warning overload should emit its message")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageDebugLog_CategoryError")),
			TEXT("category Error overload should emit its message")));
	}

	TEST_METHOD(FormattedDebugLoggingSurfaceIncludesValuesAndContext)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageDebug_FormattedLoggingSurface"), ASTEST_AS(R"AS(
			int EmitFormattedDebugLogSurface()
			{
				FString FunctionName = "EmitFormattedDebugLogSurface";
				FString ObjectName = "CoverageDebugObject";
				int Value = 42;
				FString Branch = "Low";

				if (Value > 20)
				{
					Branch = "High";
				}

				Log(n"CoverageDebugFormat", "Enter " + FunctionName + " Object=" + ObjectName);
				LogDisplay(n"CoverageDebugFormat", "CoverageDebugFormatted Value=" + Value + " Branch=" + Branch);
				Log(n"CoverageDebugFormat", "Exit " + FunctionName);
				return 3;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("formatted debug logging module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		FCapturedLogDevice LogCapture;
		GLog->AddOutputDevice(&LogCapture);
		ON_SCOPE_EXIT
		{
			GLog->RemoveOutputDevice(&LogCapture);
		};

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int EmitFormattedDebugLogSurface()"),
			TEXT("formatted debug logging helper surface should execute"), 3)));
		GLog->FlushThreadedLogs();

		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageDebugFormatted Value=42 Branch=High")),
			TEXT("formatted debug logging should include numeric and branch values")));
	}

	TEST_METHOD(NativeLogVerbosityEnumsRemainCompileTimeBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ELogVerbositySource = ASTEST_AS(R"AS(
			int TryNativeVerbosityEnum()
			{
				ELogVerbosity Value = ELogVerbosity::VeryVerbose;
				return int(Value);
			}
			)AS");
		const TArray<FString> ELogVerbosityFragments = { TEXT("ELogVerbosity") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_ELogVerbosityUnsupported"),
			*ELogVerbositySource,
			TEXT("native ELogVerbosity enum values are not currently script-facing"),
			MakeArrayView(ELogVerbosityFragments))));

		const FString FatalVerbositySource = ASTEST_AS(R"AS(
			void TryFatalVerbosity()
			{
				Fatal("Coverage fatal");
			}
			)AS");
		const TArray<FString> FatalVerbosityFragments = { TEXT("Fatal") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_FatalUnsupported"),
			*FatalVerbositySource,
			TEXT("Fatal logging is native crash behavior and is not exposed as an AS helper"),
			MakeArrayView(FatalVerbosityFragments))));

		const FString VerboseVerbositySource = ASTEST_AS(R"AS(
			void TryVerboseVerbosity()
			{
				Verbose("Coverage verbose");
				VeryVerbose("Coverage very verbose");
			}
			)AS");
		const TArray<FString> VerboseVerbosityFragments = { TEXT("Verbose") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_VerboseUnsupported"),
			*VerboseVerbositySource,
			TEXT("Verbose and VeryVerbose are native log levels, not direct AS logging helper names"),
			MakeArrayView(VerboseVerbosityFragments))));
	}

	TEST_METHOD(CallstackAndThrowBindings)
	{
		TestRunner->AddExpectedError(TEXT("CoverageThrowMessage"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("ASCoverageDebug_CallstackThrow"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("void ThrowLeaf()"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("int ThrowEntry()"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageDebug_CallstackThrow"), ASTEST_AS(R"AS(
			bool StackContains(const TArray<FString>& Stack, const FString& Needle)
			{
				for (int Index = 0; Index < Stack.Num(); ++Index)
				{
					if (Stack[Index].Contains(Needle))
					{
						return true;
					}
				}
				return false;
			}

			int ProbeCallstack()
			{
				TArray<FString> Stack = GetAngelscriptCallstack();
				FString Formatted = FormatAngelscriptCallstack();
				if (Stack.Num() < 3)
				{
					return 0;
				}
				if (!StackContains(Stack, "ProbeCallstack"))
				{
					return 0;
				}
				if (!StackContains(Stack, "EntryCallstack"))
				{
					return 0;
				}
				if (!Formatted.Contains("ProbeCallstack"))
				{
					return 0;
				}
				if (!Formatted.Contains("EntryCallstack"))
				{
					return 0;
				}
				return 1;
			}

			int EntryCallstack()
			{
				return ProbeCallstack();
			}

			void ThrowLeaf()
			{
				throw("CoverageThrowMessage");
			}

			int ThrowEntry()
			{
				ThrowLeaf();
				return 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("callstack/throw module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int EntryCallstack()"),
			TEXT("GetAngelscriptCallstack and FormatAngelscriptCallstack should include nested AS frames"), 1)));

		asIScriptFunction* ThrowFunction = ScriptModule.GetFunctionByDecl("int ThrowEntry()");
		ASSERT_THAT(IsNotNull(ThrowFunction, TEXT("ThrowEntry should resolve by declaration")));
		if (ThrowFunction == nullptr)
		{
			return;
		}

		asIScriptContext* Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("ThrowEntry should create an execution context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int32 PrepareResult = Context->Prepare(ThrowFunction);
		const int32 ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		const FString ExceptionString = Context->GetExceptionString() != nullptr
			? UTF8_TO_TCHAR(Context->GetExceptionString())
			: TEXT("");

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("ThrowEntry should prepare")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult, TEXT("ThrowEntry should raise a script exception")));
		ASSERT_THAT(IsTrue(ExceptionString.Contains(TEXT("CoverageThrowMessage")), TEXT("throw should surface the supplied message")));
	}

	TEST_METHOD(DrawDebugStringFromObject)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_DrawDebugString"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugDrawDebugString.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ADebugStringCoverageActor : AActor
				{
					UPROPERTY()
					bool bDrewDebugString = false;

					UFUNCTION(BlueprintOverride)
					void BeginPlay()
					{
						DrawDebugStringFromObject(this, GetActorLocation() + FVector(0.0, 0.0, 25.0), "CoverageDebugString", 0.01f, FLinearColor::Green);
						bDrewDebugString = true;
					}
				}
			)AS"),
			TEXT("ADebugStringCoverageActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DrawDebugStringFromObject actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DrawDebugStringFromObject actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDrewDebugString"), true,
			TEXT("DrawDebugStringFromObject should be callable from an actor world context"))));
	}

	TEST_METHOD(DrawDebugStringParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_DrawDebugStringParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugDrawDebugStringParameters.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ADebugStringParameterCoverageActor : AActor
				{
					UPROPERTY()
					bool bDefaultParametersDrew = false;

					UPROPERTY()
					bool bExplicitParametersDrew = false;

					UFUNCTION()
					int DrawDefaultDebugString()
					{
						DrawDebugStringFromObject(this, GetActorLocation(), "CoverageDebugDefaultParameters");
						bDefaultParametersDrew = true;
						return 11;
					}

					UFUNCTION()
					int DrawExplicitDebugString()
					{
						FVector OffsetLocation = GetActorLocation() + FVector(8.0, 16.0, 32.0);
						DrawDebugStringFromObject(this, OffsetLocation, "CoverageDebugExplicitParameters", 0.02f, FLinearColor::Yellow);
						bExplicitParametersDrew = true;
						return 23;
					}
				}
			)AS"),
			TEXT("ADebugStringParameterCoverageActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DrawDebugString parameter actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DrawDebugString parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("DrawDefaultDebugString"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("DrawDefaultDebugString should be invokable through reflection helper")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const int32 ReturnValue = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(11, ReturnValue, TEXT("DrawDebugStringFromObject default duration/color parameters should execute")));
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("DrawExplicitDebugString"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("DrawExplicitDebugString should be invokable through reflection helper")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const int32 ReturnValue = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(23, ReturnValue, TEXT("DrawDebugStringFromObject explicit location, duration, and color parameters should execute")));
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDefaultParametersDrew"), true,
			TEXT("DrawDebugStringFromObject should support default duration and color parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bExplicitParametersDrew"), true,
			TEXT("DrawDebugStringFromObject should support explicit location, duration, and color parameters"))));
	}

	TEST_METHOD(DebugBreakBindingCanBeDisabledForAutomation)
	{
		FScopedAngelscriptDebugBreakOverride DebugBreakOverride;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageDebug_DebugBreak"), ASTEST_AS(R"AS(
			int TriggerDebugBreak()
			{
				DebugBreak();
				return 7;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("DebugBreak module should compile")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int TriggerDebugBreak()"),
			TEXT("DebugBreak should be script-facing and controllable in automation"), 7)));
	}

	TEST_METHOD(ObjectInspectionHelpersExposeNamesAndOuter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_ObjectInspection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugObjectInspection.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class AObjectInspectionCoverageActor : AActor
				{
					UPROPERTY()
					FString ObjectName = "";

					UPROPERTY()
					FString ClassName = "";

					UPROPERTY()
					FString FullObjectName = "";

					UPROPERTY()
					FString OuterName = "";

					UPROPERTY()
					bool bInspectionComplete = false;

					UFUNCTION(BlueprintOverride)
					void BeginPlay()
					{
						ObjectName = GetName().ToString();
						ClassName = GetClass().GetName().ToString();
						FullObjectName = GetFullName();

						UObject Outer = GetOuter();
						if (Outer != nullptr)
						{
							OuterName = Outer.GetName().ToString();
						}

						bInspectionComplete = ObjectName.Len() > 0
							&& ClassName.Contains("ObjectInspectionCoverageActor")
							&& FullObjectName.Contains(ObjectName)
							&& OuterName.Len() > 0;
					}
				}
			)AS"),
			TEXT("AObjectInspectionCoverageActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("object inspection actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("object inspection actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInspectionComplete"), true,
			TEXT("object inspection helpers should populate name, class, full name, and outer"))));

		FString ObjectName;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FStrProperty>(*TestRunner, Actor, TEXT("ObjectName"), ObjectName)));
		ASSERT_THAT(IsFalse(ObjectName.IsEmpty(), TEXT("GetName should provide a non-empty object name")));

		FString ClassName;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FStrProperty>(*TestRunner, Actor, TEXT("ClassName"), ClassName)));
		ASSERT_THAT(IsTrue(ClassName.Contains(TEXT("ObjectInspectionCoverageActor")), TEXT("GetClass().GetName should expose the generated class name")));

		FString FullObjectName;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FStrProperty>(*TestRunner, Actor, TEXT("FullObjectName"), FullObjectName)));
		ASSERT_THAT(IsTrue(FullObjectName.Contains(ObjectName), TEXT("GetFullName should include the object name")));
		ASSERT_THAT(IsTrue(FullObjectName.Contains(ClassName), TEXT("GetFullName should include the class name")));

		FString OuterName;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FStrProperty>(*TestRunner, Actor, TEXT("OuterName"), OuterName)));
		ASSERT_THAT(IsFalse(OuterName.IsEmpty(), TEXT("GetOuter should provide a non-empty outer name")));

		UObject* NativeOuter = Actor->GetOuter();
		ASSERT_THAT(IsNotNull(NativeOuter, TEXT("spawned actor should have a native outer")));
		if (NativeOuter != nullptr)
		{
			ASSERT_THAT(AreEqual(NativeOuter->GetName(), OuterName, TEXT("GetOuter should match the native actor outer")));
		}
	}

	TEST_METHOD(CpuProfilerScopedEventIsScriptFacing)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageDebug_CpuProfilerTrace"), ASTEST_AS(R"AS(
			int ScopedCpuProfilerEvent()
			{
				FCpuProfilerTraceScoped Scope(n"CoverageDebugCpuProfiler");
				return 1;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("FCpuProfilerTraceScoped module should compile")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int ScopedCpuProfilerEvent()"),
			TEXT("FCpuProfilerTraceScoped should provide the AS-facing profiler event boundary"), 1)));
	}

	TEST_METHOD(ProfilerDebugPatternsUseScopedEventsCountersAndScratchMemory)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageDebug_ProfilerDebugPatterns"), ASTEST_AS(R"AS(
			int ProfiledDebugPath()
			{
				int Score = 0;
				int CallCount = 0;
				int ScratchTextLength = 0;

				{
					FCpuProfilerTraceScoped FunctionScope(n"CoverageDebugProfiledFunction");
					TArray<FString> ScratchMessages;

					for (int Index = 0; Index < 4; ++Index)
					{
						FCpuProfilerTraceScoped IterationScope(n"CoverageDebugProfiledIteration");
						FString Message = "Sample=" + Index;
						ScratchMessages.Add(Message);
						ScratchTextLength += Message.Len();
						CallCount++;
					}

					if (ScratchMessages.Num() == 4)
					{
						Score += 1;
					}
				}

				if (CallCount == 4)
				{
					Score += 2;
				}
				if (ScratchTextLength > 0)
				{
					Score += 4;
				}

				return Score;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("profiled debug patterns module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int ProfiledDebugPath()"),
			TEXT("profiled debug patterns should execute scoped events, counters, and scratch memory observations"), 7)));
	}

	TEST_METHOD(UnsupportedDrawDebugShapeParametersFailToCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString DrawLineSource = ASTEST_AS(R"AS(
			void TryLineParameters()
			{
				DrawDebugLine(FVector(0.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0), FLinearColor::Red, true, 5.0f, 3, 2.0f);
			}
			)AS");
		const TArray<FString> DrawLineFragments = { TEXT("DrawDebugLine") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_DrawDebugLineUnsupported"),
			*DrawLineSource,
			TEXT("legacy DrawDebugLine parameters are not AS-facing on this branch"),
			MakeArrayView(DrawLineFragments))));

		const FString DrawSphereSource = ASTEST_AS(R"AS(
			void TrySphereParameters()
			{
				DrawDebugSphere(FVector(0.0, 0.0, 0.0), 50.0f, 12, FLinearColor::Green, true, 5.0f, 3, 2.0f);
			}
			)AS");
		const TArray<FString> DrawSphereFragments = { TEXT("DrawDebugSphere") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_DrawDebugSphereUnsupported"),
			*DrawSphereSource,
			TEXT("legacy DrawDebugSphere size/persistence/depth/thickness parameters are not AS-facing on this branch"),
			MakeArrayView(DrawSphereFragments))));

		const FString DrawBoxSource = ASTEST_AS(R"AS(
			void TryBoxParameters()
			{
				DrawDebugBox(FVector(0.0, 0.0, 0.0), FVector(50.0, 25.0, 10.0), FLinearColor::Yellow, true, 5.0f, 3, 2.0f);
			}
			)AS");
		const TArray<FString> DrawBoxFragments = { TEXT("DrawDebugBox") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_DrawDebugBoxUnsupported"),
			*DrawBoxSource,
			TEXT("legacy DrawDebugBox size/persistence/depth/thickness parameters are not AS-facing on this branch"),
			MakeArrayView(DrawBoxFragments))));

		const FString DrawCapsuleSource = ASTEST_AS(R"AS(
			void TryCapsuleParameters()
			{
				DrawDebugCapsule(FVector(0.0, 0.0, 0.0), 88.0f, 34.0f, FQuat::Identity, FLinearColor::Green, true, 5.0f, 3, 2.0f);
			}
			)AS");
		const TArray<FString> DrawCapsuleFragments = { TEXT("DrawDebugCapsule") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_DrawDebugCapsuleUnsupported"),
			*DrawCapsuleSource,
			TEXT("legacy DrawDebugCapsule radius/size/persistence/depth/thickness parameters are not AS-facing on this branch"),
			MakeArrayView(DrawCapsuleFragments))));

		const FString DrawArrowSource = ASTEST_AS(R"AS(
			void TryArrowParameters()
			{
				DrawDebugArrow(FVector(0.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0), 12.0f, FLinearColor::Red, true, 5.0f, 3, 2.0f);
			}
			)AS");
		const TArray<FString> DrawArrowFragments = { TEXT("DrawDebugArrow") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_DrawDebugArrowUnsupported"),
			*DrawArrowSource,
			TEXT("legacy DrawDebugArrow size/persistence/depth/thickness parameters are not AS-facing on this branch"),
			MakeArrayView(DrawArrowFragments))));

		const FString DrawCoordinateSystemSource = ASTEST_AS(R"AS(
			void TryCoordinateSystemParameters()
			{
				DrawDebugCoordinateSystem(FVector(0.0, 0.0, 0.0), FRotator(0.0, 90.0, 0.0), 25.0f, true, 5.0f, 3, 2.0f);
			}
			)AS");
		const TArray<FString> DrawCoordinateSystemFragments = { TEXT("DrawDebugCoordinateSystem") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_DrawDebugCoordinateSystemUnsupported"),
			*DrawCoordinateSystemSource,
			TEXT("legacy DrawDebugCoordinateSystem size/persistence/depth/thickness parameters are not AS-facing on this branch"),
			MakeArrayView(DrawCoordinateSystemFragments))));

		const FString DrawPointSource = ASTEST_AS(R"AS(
			void TryPointParameters()
			{
				DrawDebugPoint(FVector(0.0, 0.0, 0.0), 8.0f, FLinearColor::White, true, 5.0f, 3);
			}
			)AS");
		const TArray<FString> DrawPointFragments = { TEXT("DrawDebugPoint") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_DrawDebugPointUnsupported"),
			*DrawPointSource,
			TEXT("legacy DrawDebugPoint size/persistence/depth parameters are not AS-facing on this branch"),
			MakeArrayView(DrawPointFragments))));
	}

	TEST_METHOD(StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedCoverageDebugConsole ConsoleScope(*TestRunner);

		const FString CommandName = ConsoleScope.MakeName(TEXT("stat.show.command"));
		const FString OutputName = ConsoleScope.MakeName(TEXT("stat.show.output"));
		IConsoleVariable* OutputVariable = ConsoleScope.RegisterIntVariable(
			OutputName,
			-1,
			TEXT("Coverage debug stat/show command output"));
		ASSERT_THAT(IsNotNull(OutputVariable, TEXT("stat/show command output CVar should be registered")));
		if (OutputVariable == nullptr)
		{
			return;
		}

		const FString ScriptSource = MakeDebugConsoleScriptSource(
			ASTEST_AS(R"AS(
				const FConsoleCommand Command("$COMMAND$", n"OnCoverageDebugCommand");

				void OnCoverageDebugCommand(const TArray<FString>& Args)
				{
					FConsoleVariable Output("$OUTPUT$", 0, "Coverage debug stat/show command output");
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
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "Collision")
					{
						Score = 201;
					}
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "Bones")
					{
						Score = 202;
					}
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "Navmesh")
					{
						Score = 203;
					}
					else if (Args.Num() == 2 && Args[0] == "show" && Args[1] == "Paths")
					{
						Score = 204;
					}

					Output.SetInt(Score);
				}

				int DebugCommandReady()
				{
					return 1;
				}
				)AS"),
			CommandName,
			OutputName);

		TUniquePtr<FScopedAngelscriptModule> Module = MakeUnique<FScopedAngelscriptModule>(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_StatShowCommandRegistration"),
			ScriptSource);
		ASSERT_THAT(IsTrue(Module->IsValid(), TEXT("stat/show FConsoleCommand module should compile")));
		if (!Module->IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module->GetModule(), TEXT("int DebugCommandReady()"),
			TEXT("stat/show FConsoleCommand module should initialize"), 1)));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("fps") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat fps"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 101,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat fps output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("unit") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat unit"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 102,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat unit output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("game") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat game"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 103,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat game output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("gpu") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat gpu"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 104,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat gpu output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("memory") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat memory"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 105,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat memory output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("stat"), TEXT("slow") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat slow"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 106,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand stat slow output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("Collision") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand show Collision"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 201,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand show Collision output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("Bones") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand show Bones"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 202,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand show Bones output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("Navmesh") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand show Navmesh"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 203,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand show Navmesh output"))));

		ASSERT_THAT(IsTrue(ConsoleScope.ExecuteCommandArgs(CommandName, { TEXT("show"), TEXT("Paths") },
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand show Paths"))));
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyInt(OutputName, 204,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand show Paths output"))));

		Module.Reset();
		ASSERT_THAT(IsTrue(ConsoleScope.VerifyCommandMissing(CommandName,
			TEXT("StatAndShowCommandNamesDispatchThroughRegisteredConsoleCommand"))));
	}

	TEST_METHOD(ConsoleProfilerAndDebuggerControlsFailToCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ConsoleCommandSource = ASTEST_AS(R"AS(
			void TryConsoleDebugCommands()
			{
				ConsoleCommand("stat fps");
				ConsoleCommand("stat unit");
				ConsoleCommand("stat game");
				ConsoleCommand("stat gpu");
				ConsoleCommand("stat memory");
				ConsoleCommand("stat slow");
				ConsoleCommand("show Collision");
				ConsoleCommand("show Bones");
				ConsoleCommand("show Navmesh");
				ConsoleCommand("show Paths");
			}
			)AS");
		const TArray<FString> ConsoleCommandFragments = { TEXT("ConsoleCommand") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_ConsoleCommandsUnsupported"),
			*ConsoleCommandSource,
			TEXT("stat/show console command execution is not directly AS-facing"),
			MakeArrayView(ConsoleCommandFragments))));

		const FString ScopeCycleCounterSource = ASTEST_AS(R"AS(
			void TryNativeProfilerMacro()
			{
				SCOPE_CYCLE_COUNTER(STAT_CoverageDebugAndLogging);
			}
			)AS");
		const TArray<FString> ScopeCycleCounterFragments = { TEXT("STAT_CoverageDebugAndLogging") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_ScopeCycleCounterUnsupported"),
			*ScopeCycleCounterSource,
			TEXT("native SCOPE_CYCLE_COUNTER macro is not AS-facing"),
			MakeArrayView(ScopeCycleCounterFragments))));

		const FString DebuggerControlSource = ASTEST_AS(R"AS(
			void TryIdeDebuggerControls()
			{
				SetBreakpoint("Coverage_DebugAndLogging.as", 12);
			}
			)AS");
		const TArray<FString> DebuggerControlFragments = { TEXT("SetBreakpoint") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_IdeBreakpointUnsupported"),
			*DebuggerControlSource,
			TEXT("IDE breakpoint management is debugger-client functionality, not an AS callable API"),
			MakeArrayView(DebuggerControlFragments))));
	}

	TEST_METHOD(DebugErrorHandlingPatterns)
	{
		TestRunner->AddExpectedError(TEXT("CoverageDebugNullGuard"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("CoverageDebugIndexGuard"), EAutomationExpectedErrorFlags::Contains, 1);

		FScopedAngelscriptDebugBreakOverride DebugBreakOverride;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageDebug_ErrorHandlingPatterns"), ASTEST_AS(R"AS(
			enum ECoverageDebugErrorCode
			{
				Success = 0,
				NotFound = 1,
				InvalidParam = 2
			}

			bool TryDivide(int Numerator, int Denominator, int&out OutResult)
			{
				if (Denominator == 0)
				{
					OutResult = 0;
					return false;
				}

				OutResult = Numerator / Denominator;
				return true;
			}

			ECoverageDebugErrorCode ValidateIndex(const TArray<int>&in Values, int Index)
			{
				if (Index < 0)
				{
					return ECoverageDebugErrorCode::InvalidParam;
				}
				if (!Values.IsValidIndex(Index))
				{
					return ECoverageDebugErrorCode::NotFound;
				}
				return ECoverageDebugErrorCode::Success;
			}

			int SafeActorNameLength(AActor Actor)
			{
				if (!ensure(Actor != nullptr, "CoverageDebugNullGuard"))
				{
					return 0;
				}

				return Actor.GetName().ToString().Len();
			}

			int SafeArrayRead(const TArray<int>&in Values, int Index, int DefaultValue)
			{
				if (!ensure(Values.IsValidIndex(Index), "CoverageDebugIndexGuard"))
				{
					return DefaultValue;
				}

				return Values[Index];
			}

			int DefensiveDebugPatterns()
			{
				int Score = 0;

				int DivideResult = 0;
				if (!TryDivide(10, 0, DivideResult) && DivideResult == 0)
				{
					Score += 1;
				}
				if (TryDivide(12, 3, DivideResult) && DivideResult == 4)
				{
					Score += 2;
				}

				TArray<int> Values;
				Values.Add(11);
				Values.Add(22);
				if (ValidateIndex(Values, -1) == ECoverageDebugErrorCode::InvalidParam)
				{
					Score += 4;
				}
				if (ValidateIndex(Values, 5) == ECoverageDebugErrorCode::NotFound)
				{
					Score += 8;
				}
				if (SafeActorNameLength(nullptr) == 0)
				{
					Score += 16;
				}
				if (SafeArrayRead(Values, 5, -7) == -7)
				{
					Score += 32;
				}

				return Score;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("debug error handling patterns module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int DefensiveDebugPatterns()"),
			TEXT("debug error handling should cover bool returns, out error results, ensure guards, null checks, and index checks"), 63)));
	}

	TEST_METHOD(DebuggerClientOnlyFeaturesFailToCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ConditionalBreakpointSource = ASTEST_AS(R"AS(
			void TryConditionalBreakpoint()
			{
				SetConditionalBreakpoint("Coverage_DebugAndLogging.as", 15, "Value > 10");
			}
			)AS");
		const TArray<FString> ConditionalBreakpointFragments = { TEXT("SetConditionalBreakpoint") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_ConditionalBreakpointUnsupported"),
			*ConditionalBreakpointSource,
			TEXT("conditional breakpoint management is debugger-client functionality, not an AS callable API"),
			MakeArrayView(ConditionalBreakpointFragments))));

		const FString LogBreakpointSource = ASTEST_AS(R"AS(
			void TryLogBreakpoint()
			{
				SetLogBreakpoint("Coverage_DebugAndLogging.as", 16, "Value={Value}");
			}
			)AS");
		const TArray<FString> LogBreakpointFragments = { TEXT("SetLogBreakpoint") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_LogBreakpointUnsupported"),
			*LogBreakpointSource,
			TEXT("log breakpoint management is debugger-client functionality, not an AS callable API"),
			MakeArrayView(LogBreakpointFragments))));

		const FString StepControlsSource = ASTEST_AS(R"AS(
			void TryStepControls()
			{
				StepOver();
				StepInto();
				StepOut();
			}
			)AS");
		const TArray<FString> StepControlFragments = { TEXT("StepOver") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_StepControlsUnsupported"),
			*StepControlsSource,
			TEXT("debugger stepping is driven by debugger clients, not AS callable functions"),
			MakeArrayView(StepControlFragments))));

		const FString WatchAndImmediateSource = ASTEST_AS(R"AS(
			void TryWatchAndImmediate()
			{
				Watch("Value");
				EvaluateImmediate("Value + 1");
			}
			)AS");
		const TArray<FString> WatchFragments = { TEXT("Watch") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_WatchImmediateUnsupported"),
			*WatchAndImmediateSource,
			TEXT("watch and immediate-window evaluation are debugger-client operations, not AS callable APIs"),
			MakeArrayView(WatchFragments))));

		const FString ReferenceToolsSource = ASTEST_AS(R"AS(
			void TryReferenceTools(UObject Object)
			{
				OpenReferenceViewer(Object);
				ObjRefs(Object);
			}
			)AS");
		const TArray<FString> ReferenceToolFragments = { TEXT("OpenReferenceViewer") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_ReferenceToolsUnsupported"),
			*ReferenceToolsSource,
			TEXT("editor reference viewer and obj refs commands are not direct AS callable APIs"),
			MakeArrayView(ReferenceToolFragments))));

		const FString InvestigationWorkflowSource = ASTEST_AS(R"AS(
			void TryInvestigationWorkflowTools()
			{
				RecordReproSteps("Coverage repro");
				GitBisect("bad", "good");
			}
			)AS");
		const TArray<FString> InvestigationFragments = { TEXT("RecordReproSteps") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDebug_InvestigationWorkflowUnsupported"),
			*InvestigationWorkflowSource,
			TEXT("repro-step recording and git bisect are workflow practices, not AS callable APIs"),
			MakeArrayView(InvestigationFragments))));
	}

	TEST_METHOD(DebuggingWorkflowPatternsUseCallableScriptHelpers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_WorkflowPatterns"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugWorkflowPatterns.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ADebugWorkflowCoverageActor : AActor
				{
					UPROPERTY()
					int ProbeCount = 0;

					UPROPERTY()
					FString LastProbe = "";

					UPROPERTY()
					bool bRecordedReproContext = false;

					UPROPERTY()
					bool bConditionalProbeHit = false;

					UFUNCTION()
					int TraceBranch(int Value)
					{
						Log(n"CoverageDebugWorkflow", "Enter TraceBranch Value=" + Value);

						if (Value > 10)
						{
							ProbeCount++;
							LastProbe = "High:" + Value;
							bConditionalProbeHit = true;
							Log(n"CoverageDebugWorkflow", "Branch High Value=" + Value);
						}
						else
						{
							ProbeCount++;
							LastProbe = "Low:" + Value;
							Log(n"CoverageDebugWorkflow", "Branch Low Value=" + Value);
						}

						Log(n"CoverageDebugWorkflow", "Exit TraceBranch ProbeCount=" + ProbeCount);
						return ProbeCount;
					}

					UFUNCTION()
					int RecordReproContext(FString StepName)
					{
						Log(n"CoverageDebugWorkflow", "ReproStep=" + StepName + " Actor=" + GetName() + " Class=" + GetClass().GetName());
						bRecordedReproContext = StepName.Len() > 0 && GetName().ToString().Len() > 0;
						return bRecordedReproContext ? 1 : 0;
					}
				}
			)AS"),
			TEXT("ADebugWorkflowCoverageActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("debug workflow actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("debug workflow actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("TraceBranch"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("TraceBranch should be invokable through reflection helper")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<int32>(4);
			const int32 ReturnValue = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(1, ReturnValue, TEXT("low branch trace should increment the probe counter")));
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("TraceBranch"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("TraceBranch should be invokable through reflection helper")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<int32>(42);
			const int32 ReturnValue = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(2, ReturnValue, TEXT("high branch trace should increment the probe counter")));
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("RecordReproContext"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("RecordReproContext should be invokable through reflection helper")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<FString>(TEXT("Open coverage workflow actor"));
			const int32 ReturnValue = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(1, ReturnValue, TEXT("repro context helper should record a non-empty step and object context")));
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ProbeCount"), 2,
			TEXT("debug workflow probes should count low and high branch traces"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bConditionalProbeHit"), true,
			TEXT("conditional debug probe should record the high-value branch"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRecordedReproContext"), true,
			TEXT("debug workflow should record explicit repro context in script state"))));

		FString LastProbe;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FStrProperty>(*TestRunner, Actor, TEXT("LastProbe"), LastProbe)));
		ASSERT_THAT(AreEqual(FString(TEXT("High:42")), LastProbe, TEXT("debug workflow should preserve the latest branch context")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
