#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "Binds/Bind_Debugging.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

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

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageDebugTest,
	"Angelscript.TestModule.Coverage.Debug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
	}

	TEST_METHOD(ConsoleProfilerAndDebuggerControlsFailToCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ConsoleCommandSource = ASTEST_AS(R"AS(
			void TryConsoleDebugCommands()
			{
				ConsoleCommand("stat fps");
				ConsoleCommand("show Collision");
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
		const TArray<FString> ScopeCycleCounterFragments = { TEXT("SCOPE_CYCLE_COUNTER") };
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
};

#endif // WITH_DEV_AUTOMATION_TESTS
