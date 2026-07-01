#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "CoreGlobals.h"
#include "GameFramework/Actor.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageLoggingTests
// -----------------------------------------------------------------------------
// Comprehensive logging and debug output coverage for AngelScript, following
// the matrix from OpenSpec: test-coverage/coverage-matrix.md.
//
// Test axes covered:
//   * PrintFunctions                - Print, PrintFromObject, PrintToScreen, PrintDirectToScreen, PrintWarning, PrintError
//   * UELogMacros                   - compile-negative boundary for unsupported native UE_LOG syntax
//   * LogCategories                 - AS-facing FName category overloads
//   * LogFormatting                 - String formatting in log messages
//
// Pattern: Compile script modules with logging calls, spawn actors, verify
// that logging APIs are callable and compile correctly. Note: We verify API
// availability and compilation success, not actual log output content.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageLoggingTest,
	"Angelscript.TestModule.Coverage.Logging",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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

		bool ContainsTextInCategory(const FString& Text, const FName& Category) const
		{
			for (const FCapturedLogLine& Line : Lines)
			{
				if (Line.Category == Category
					&& Line.Text.Contains(Text))
				{
					return true;
				}
			}

			return false;
		}

		int32 CountText(const FString& Text) const
		{
			int32 Count = 0;
			for (const FCapturedLogLine& Line : Lines)
			{
				if (Line.Text.Contains(Text))
				{
					++Count;
				}
			}

			return Count;
		}
	};

	static void RegisterExpectedLogErrors(FAutomationTestBase& Test)
	{
		Test.AddExpectedError(TEXT("CoverageLogLevel_Error"), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedError(TEXT("CoverageCategory_Error"), EAutomationExpectedErrorFlags::Contains, 1);
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

	// -------------------------------------------------------------------------
	// Print series: Basic print functions
	// -------------------------------------------------------------------------
	TEST_METHOD(PrintFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_PrintFunctions"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingPrintFunctions.as"),
			ASTEST_AS(R"AS(
			// Actor that exercises Print functions
			UCLASS()
			class APrintTestActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Basic Print
					Print("Basic print message");

					// Print with variable interpolation
					int Value = 42;
					Print("Value is: " + Value);

					// Print with duration and color
					Print("Colored message on screen", 5.0f, FLinearColor::Red);

					// Screen/world-context variants
					PrintFromObject(this, "Print from object context", 0.01f, FLinearColor::Green);
					PrintToScreen("Print to screen only", 0.01f, FLinearColor::Yellow);
					PrintDirectToScreen("Print direct to screen", 0.01f, FLinearColor::Blue);

					// PrintWarning (if supported)
					PrintWarning("This is a warning message");

					// PrintError (if supported)
					PrintError("This is an error message");

					// Print with concatenation
					FString Name = "Test";
					Print("Actor name: " + Name);

					// Print with numbers
					float Health = 100.5f;
					Print("Health: " + Health);

					// Print boolean
					bool IsActive = true;
					Print("Is active: " + IsActive);
				}
			}
			)AS"),
			TEXT("APrintTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Print functions actor class should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Print functions actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
	}

	// -------------------------------------------------------------------------
	// UE_LOG macros with different verbosity levels
	// -------------------------------------------------------------------------
	TEST_METHOD(UELogMacros)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_UELogMacros"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingUELogMacros.as"),
			ASTEST_AS(R"AS(
			// Actor that tests UE_LOG macro equivalents (if available in AS)
			UCLASS()
			class AUELogTestActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Note: AngelScript may not support UE_LOG directly
					// This tests if the API is exposed or has AS equivalents

					// Regular log message
					Print("Regular log message");

					// Warning level
					PrintWarning("Warning level message");

					// Error level
					PrintError("Error level message");

					// Log with formatting
					int Count = 10;
					Print("Item count: " + Count);

					// Multiple variable types
					FString PlayerName = "Hero";
					float Score = 1500.75f;
					bool IsWinner = true;
					Print("Player: " + PlayerName + ", Score: " + Score + ", Winner: " + IsWinner);
				}
			}
			)AS"),
			TEXT("AUELogTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UE_LOG macros actor class should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UE_LOG macros actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
	}

	// -------------------------------------------------------------------------
	// Log categories: Different log categories for organization
	// -------------------------------------------------------------------------
	TEST_METHOD(LogCategories)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_LogCategories"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingLogCategories.as"),
			ASTEST_AS(R"AS(
			// Actor testing different log categories
			// Note: AngelScript may not support custom log categories directly
			UCLASS()
			class ALogCategoryTestActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// LogTemp equivalent (most common)
					Print("Temporary log message");

					// Structured logging with context
					Print("[Script] BeginPlay called");
					Print("[Actor] Initialization complete");
					Print("[Network] Connection established");

					// Prefixed logging for manual categorization
					FString Category = "GameLogic";
					Print("[" + Category + "] Custom category message");
				}
			}
			)AS"),
			TEXT("ALogCategoryTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Log categories actor class should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Log categories actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
	}

	// -------------------------------------------------------------------------
	// Log formatting: String formatting and concatenation
	// -------------------------------------------------------------------------
	TEST_METHOD(LogFormatting)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_LogFormatting"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingLogFormatting.as"),
			ASTEST_AS(R"AS(
			// Actor testing various log formatting patterns
			UCLASS()
			class ALogFormattingTestActor : AActor
			{
				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Basic string concatenation
					FString Message = "Hello" + " " + "World";
					Print(Message);

					// Integer formatting
					int Level = 5;
					Print("Player level: " + Level);

					// Float formatting
					float Distance = 123.456f;
					Print("Distance: " + Distance + " units");

					// Boolean formatting
					bool HasKey = false;
					Print("Has key: " + HasKey);

					// Vector formatting
					FVector Location = FVector(100.0f, 200.0f, 300.0f);
					Print("Location: " + Location.ToString());

					// Rotator formatting
					FRotator Rotation = FRotator(45.0f, 90.0f, 0.0f);
					Print("Rotation: " + Rotation.ToString());

					// Color formatting
					FLinearColor Color = FLinearColor::Red;
					Print("Color: " + Color.ToString());

					// Object name formatting
					Print("Actor name: " + GetName());
					Print("Class name: " + GetClass().GetName());

					// Multi-line context logging
					Print("=== Actor Status ===");
					Print("Name: " + GetName());
					Print("Location: " + GetActorLocation().ToString());
					Print("Rotation: " + GetActorRotation().ToString());
					Print("==================");
				}
			}
			)AS"),
			TEXT("ALogFormattingTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Log formatting actor class should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Log formatting actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
	}

	// -------------------------------------------------------------------------
	// Conditional logging: Logging based on conditions
	// -------------------------------------------------------------------------
	TEST_METHOD(ConditionalLogging)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_ConditionalLogging"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingConditionalLogging.as"),
			ASTEST_AS(R"AS(
			// Actor testing conditional logging patterns
			UCLASS()
			class AConditionalLogTestActor : AActor
			{
				UPROPERTY()
				bool bDebugMode = true;

				UPROPERTY()
				int LogLevel = 2;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Debug mode logging
					if (bDebugMode)
					{
						Print("[DEBUG] Debug mode is enabled");
					}

					// Verbosity level logging
					if (LogLevel >= 1)
					{
						Print("[INFO] Basic information");
					}

					if (LogLevel >= 2)
					{
						Print("[VERBOSE] Detailed information");
					}

					if (LogLevel >= 3)
					{
						Print("[VERY_VERBOSE] Very detailed information");
					}

					// Error condition logging
					int Health = 0;
					if (Health <= 0)
					{
						PrintError("Health is zero or negative!");
					}

					// Warning condition logging
					float Temperature = 95.0f;
					if (Temperature > 90.0f)
					{
						PrintWarning("Temperature is high: " + Temperature);
					}

					// Success logging
					bool bOperationSuccess = true;
					if (bOperationSuccess)
					{
						Print("[SUCCESS] Operation completed successfully");
					}
					else
					{
						PrintError("[FAILURE] Operation failed");
					}
				}
			}
			)AS"),
			TEXT("AConditionalLogTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Conditional logging actor class should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Conditional logging actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify properties
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDebugMode"), true, TEXT("bDebugMode should be true"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LogLevel"), 2, TEXT("LogLevel should be 2"))));
	}

	// -------------------------------------------------------------------------
	// Function entry/exit logging: Common debugging pattern
	// -------------------------------------------------------------------------
	TEST_METHOD(FunctionEntryExitLogging)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_FunctionEntryExit"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingFunctionEntryExit.as"),
			ASTEST_AS(R"AS(
			// Actor demonstrating function entry/exit logging pattern
			UCLASS()
			class AFunctionLogTestActor : AActor
			{
				UFUNCTION()
				void ProcessData(int Value)
				{
					Print(">>> Enter ProcessData, Value=" + Value);

					// Function logic
					int Result = Value * 2;
					Print("    Computed result: " + Result);

					Print("<<< Exit ProcessData");
				}

				UFUNCTION()
				bool ValidateInput(FString Input)
				{
					Print(">>> Enter ValidateInput");

					bool bValid = Input.Len() > 0;
					Print("    Input length: " + Input.Len() + ", Valid: " + bValid);

					Print("<<< Exit ValidateInput, returning: " + bValid);
					return bValid;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("=== BeginPlay Start ===");

					ProcessData(42);

					bool bResult = ValidateInput("TestString");
					Print("Validation result: " + bResult);

					Print("=== BeginPlay End ===");
				}
			}
			)AS"),
			TEXT("AFunctionLogTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Function entry/exit logging actor class should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Function entry/exit logging actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
	}

	// -------------------------------------------------------------------------
	// Performance logging: Avoiding log spam
	// -------------------------------------------------------------------------
	TEST_METHOD(PerformanceConsciousLogging)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_Performance"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingPerformance.as"),
			ASTEST_AS(R"AS(
			// Actor demonstrating performance-conscious logging
			UCLASS()
			class APerformanceLogTestActor : AActor
			{
				UPROPERTY()
				int TickCounter = 0;

				UPROPERTY()
				int LogInterval = 60;  // Log every 60 frames

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("Actor initialized - will log every " + LogInterval + " ticks");
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					TickCounter++;

					// Avoid spamming logs on every tick
					// Only log at intervals
					if (TickCounter % LogInterval == 0)
					{
						Print("[Tick " + TickCounter + "] Still running, DeltaSeconds: " + DeltaSeconds);
					}

					// Log significant events regardless of interval
					if (TickCounter == 100)
					{
						Print("Milestone: Reached 100 ticks");
					}
				}
			}
			)AS"),
			TEXT("APerformanceLogTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Performance conscious logging actor class should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Performance conscious logging actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify tick counter initialization
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TickCounter"), 0, TEXT("TickCounter should be 0"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LogInterval"), 60, TEXT("LogInterval should be 60"))));
	}

	// -------------------------------------------------------------------------
	// Context-rich logging: Including useful debugging context
	// -------------------------------------------------------------------------
	TEST_METHOD(ContextRichLogging)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_ContextRich"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingContextRich.as"),
			ASTEST_AS(R"AS(
			// Actor demonstrating context-rich logging for debugging
			UCLASS()
			class AContextLogTestActor : AActor
			{
				UPROPERTY()
				int PlayerID = 12345;

				UPROPERTY()
				FString SessionID = "SESSION-ABC-123";

				UFUNCTION()
				void LogWithContext(FString Message)
				{
					// Include actor identity
					FString FullMessage = "[" + GetName() + "] ";

					// Include session context
					FullMessage += "[Session:" + SessionID + "] ";

					// Include player context
					FullMessage += "[Player:" + PlayerID + "] ";

					// Add the actual message
					FullMessage += Message;

					Print(FullMessage);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					LogWithContext("Actor initialized");

					// Log with spatial context
					FVector Loc = GetActorLocation();
					Print("[" + GetName() + "] Location: X=" + Loc.X + " Y=" + Loc.Y + " Z=" + Loc.Z);

					// Log with hierarchical context
					AActor Owner = GetOwner();
					if (Owner != nullptr)
					{
						Print("[" + GetName() + "] Owned by: " + Owner.GetName());
					}
					else
					{
						Print("[" + GetName() + "] No owner");
					}

					LogWithContext("Initialization complete");
				}
			}
			)AS"),
			TEXT("AContextLogTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Context-rich logging actor class should compile")));
		if (!ScriptClass)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Context-rich logging actor should spawn")));
		if (!Actor)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify context properties
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PlayerID"), 12345, TEXT("PlayerID should be 12345"))));
	}

	TEST_METHOD(LogVerbosityFunctionsEmitExpectedCategories)
	{
		RegisterExpectedLogErrors(*TestRunner);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLogging_LogVerbosityFunctions"), ASTEST_AS(R"AS(
			int EmitLogVerbosityMessages()
			{
				Log("CoverageLogLevel_Log");
				LogInfo("CoverageLogLevel_Info");
				LogDisplay("CoverageLogLevel_Display");
				Warning("CoverageLogLevel_Warning");
				Error("CoverageLogLevel_Error");

				Log(n"CoverageCustomCategory", "CoverageCategory_Log");
				Warning(n"CoverageCustomCategory", "CoverageCategory_Warning");
				Error(n"CoverageCustomCategory", "CoverageCategory_Error");

				return 1;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("log verbosity module should compile")));
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

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int EmitLogVerbosityMessages()"),
			TEXT("AS logging functions should execute"), 1)));
		GLog->FlushThreadedLogs();

		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageLogLevel_Log"), ELogVerbosity::Log, FName(TEXT("Angelscript"))),
			TEXT("Log should emit Log verbosity in the Angelscript category")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageLogLevel_Info")),
			TEXT("LogInfo should emit its message")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("[Display] CoverageLogLevel_Display"), ELogVerbosity::Display, FName(TEXT("Angelscript"))),
			TEXT("LogDisplay should emit Display verbosity in the Angelscript category")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageLogLevel_Warning"), ELogVerbosity::Warning, FName(TEXT("Angelscript"))),
			TEXT("Warning should emit Warning verbosity in the Angelscript category")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsTextInCategory(TEXT("CoverageLogLevel_Error"), FName(TEXT("Angelscript"))),
			TEXT("Error should emit its message in the Angelscript category")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageCategory_Log"), ELogVerbosity::Log, FName(TEXT("CoverageCustomCategory"))),
			TEXT("category Log overload should preserve category and Log verbosity")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageCategory_Warning"), ELogVerbosity::Warning, FName(TEXT("CoverageCustomCategory"))),
			TEXT("category Warning overload should preserve category and Warning verbosity")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsTextInCategory(TEXT("CoverageCategory_Error"), FName(TEXT("CoverageCustomCategory"))),
			TEXT("category Error overload should preserve category and emit its message")));
	}

	TEST_METHOD(ConditionalLogFunctionsGateOutput)
	{
		TestRunner->AddExpectedError(TEXT("CoverageConditional_Error"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("CoverageConditional_CategoryErrorTrue"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLogging_ConditionalLogFunctions"), ASTEST_AS(R"AS(
			int EmitConditionalLogs()
			{
				LogIf(true, "CoverageConditional_LogTrue");
				LogIf(false, "CoverageConditional_LogFalse");
				LogInfoIf(true, "CoverageConditional_InfoTrue");
				LogInfoIf(false, "CoverageConditional_InfoFalse");
				WarningIf(true, "CoverageConditional_WarningTrue");
				WarningIf(false, "CoverageConditional_WarningFalse");
				ErrorIf(true, "CoverageConditional_Error");
				ErrorIf(false, "CoverageConditional_ErrorFalse");

				LogIf(true, n"CoverageConditionalCategory", "CoverageConditional_CategoryLogTrue");
				LogIf(false, n"CoverageConditionalCategory", "CoverageConditional_CategoryLogFalse");
				LogInfoIf(true, n"CoverageConditionalCategory", "CoverageConditional_CategoryInfoTrue");
				LogInfoIf(false, n"CoverageConditionalCategory", "CoverageConditional_CategoryInfoFalse");
				LogDisplayIf(true, n"CoverageConditionalCategory", "CoverageConditional_DisplayTrue");
				LogDisplayIf(false, n"CoverageConditionalCategory", "CoverageConditional_DisplayFalse");
				WarningIf(true, n"CoverageConditionalCategory", "CoverageConditional_CategoryWarningTrue");
				WarningIf(false, n"CoverageConditionalCategory", "CoverageConditional_CategoryWarningFalse");
				ErrorIf(true, n"CoverageConditionalCategory", "CoverageConditional_CategoryErrorTrue");
				ErrorIf(false, n"CoverageConditionalCategory", "CoverageConditional_CategoryErrorFalse");

				return 1;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("conditional log module should compile")));
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

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int EmitConditionalLogs()"),
			TEXT("conditional logging functions should execute"), 1)));
		GLog->FlushThreadedLogs();

		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageConditional_LogTrue")),
			TEXT("LogIf(true) should emit")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_LogFalse")),
			TEXT("LogIf(false) should not emit")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageConditional_InfoTrue")),
			TEXT("LogInfoIf(true) should emit")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_InfoFalse")),
			TEXT("LogInfoIf(false) should not emit")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageConditional_WarningTrue")),
			TEXT("WarningIf(true) should emit")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_WarningFalse")),
			TEXT("WarningIf(false) should not emit")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsText(TEXT("CoverageConditional_Error")),
			TEXT("ErrorIf(true) should emit")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_ErrorFalse")),
			TEXT("ErrorIf(false) should not emit")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageConditional_CategoryLogTrue"), ELogVerbosity::Log, FName(TEXT("CoverageConditionalCategory"))),
			TEXT("LogIf(true, category) should preserve category and Log verbosity")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_CategoryLogFalse")),
			TEXT("LogIf(false, category) should not emit")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("[Information] CoverageConditional_CategoryInfoTrue"), ELogVerbosity::Log, FName(TEXT("CoverageConditionalCategory"))),
			TEXT("LogInfoIf(true, category) should preserve category and information prefix")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_CategoryInfoFalse")),
			TEXT("LogInfoIf(false, category) should not emit")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("[Display] CoverageConditional_DisplayTrue"), ELogVerbosity::Display, FName(TEXT("CoverageConditionalCategory"))),
			TEXT("LogDisplayIf(true, category) should preserve category and Display verbosity")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_DisplayFalse")),
			TEXT("LogDisplayIf(false, category) should not emit")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageConditional_CategoryWarningTrue"), ELogVerbosity::Warning, FName(TEXT("CoverageConditionalCategory"))),
			TEXT("WarningIf(true, category) should preserve category and Warning verbosity")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_CategoryWarningFalse")),
			TEXT("WarningIf(false, category) should not emit")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsTextInCategory(TEXT("CoverageConditional_CategoryErrorTrue"), FName(TEXT("CoverageConditionalCategory"))),
			TEXT("ErrorIf(true, category) should preserve category and emit its message")));
		ASSERT_THAT(IsFalse(LogCapture.ContainsText(TEXT("CoverageConditional_CategoryErrorFalse")),
			TEXT("ErrorIf(false, category) should not emit")));
	}

	TEST_METHOD(UnsupportedNativeLogMacrosAndVerbosityEnumsFailToCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString UELogSource = ASTEST_AS(R"AS(
			void TryNativeUELogMacro()
			{
				UE_LOG(LogTemp, Verbose, TEXT("Coverage verbose"));
			}
			)AS");
		const TArray<FString> UELogFragments = { TEXT("LogTemp"), TEXT("Verbose"), TEXT("TEXT") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageLogging_UELogMacroUnsupported"),
			*UELogSource,
			TEXT("native UE_LOG macro syntax is not AS-facing"),
			MakeArrayView(UELogFragments))));

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
			TEXT("ASCoverageLogging_ELogVerbosityUnsupported"),
			*ELogVerbositySource,
			TEXT("ELogVerbosity enum values are not currently script-facing"),
			MakeArrayView(ELogVerbosityFragments))));

		const FString NativeFatalVerbositySource = ASTEST_AS(R"AS(
			void TryNativeFatalVerbosity()
			{
				Fatal("Coverage fatal");
			}
			)AS");
		const TArray<FString> NativeFatalVerbosityFragments = { TEXT("Fatal") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageLogging_FatalFunctionUnsupported"),
			*NativeFatalVerbositySource,
			TEXT("Fatal logging is native crash behavior and is not exposed as an AS logging helper"),
			MakeArrayView(NativeFatalVerbosityFragments))));

		const FString NativeVerboseVerbositySource = ASTEST_AS(R"AS(
			void TryNativeVerboseVerbosity()
			{
				Verbose("Coverage verbose");
				VeryVerbose("Coverage very verbose");
			}
			)AS");
		const TArray<FString> NativeVerboseVerbosityFragments = { TEXT("Verbose") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageLogging_VerboseFunctionsUnsupported"),
			*NativeVerboseVerbositySource,
			TEXT("Verbose and VeryVerbose are native log levels, not direct AS logging helper names"),
			MakeArrayView(NativeVerboseVerbosityFragments))));
	}

	TEST_METHOD(SupportedLogLevelsMapToAutomationSafeVerbosity)
	{
		RegisterExpectedLogErrors(*TestRunner);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageLogging_SupportedLogLevels"), ASTEST_AS(R"AS(
			int EmitSupportedLogLevels()
			{
				Log("CoverageSeverity_Log");
				LogInfo("CoverageSeverity_Info");
				LogDisplay("CoverageSeverity_Display");
				Warning("CoverageSeverity_Warning");
				Error("CoverageLogLevel_Error");

				Log(n"CoverageSeverityCategory", "CoverageSeverity_CategoryLog");
				LogInfo(n"CoverageSeverityCategory", "CoverageSeverity_CategoryInfo");
				LogDisplay(n"CoverageSeverityCategory", "CoverageSeverity_CategoryDisplay");
				Warning(n"CoverageSeverityCategory", "CoverageSeverity_CategoryWarning");
				Error(n"CoverageCustomCategory", "CoverageCategory_Error");

				return 1;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("supported log-level module should compile")));
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

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(), TEXT("int EmitSupportedLogLevels()"),
			TEXT("supported log-level wrappers should execute"), 1)));
		GLog->FlushThreadedLogs();

		ASSERT_THAT(AreEqual(1, LogCapture.CountText(TEXT("CoverageSeverity_Log")),
			TEXT("Log wrapper should emit exactly once")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("[Information] CoverageSeverity_Info"), ELogVerbosity::Log, FName(TEXT("Angelscript"))),
			TEXT("LogInfo should use Log verbosity with an information prefix")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("[Display] CoverageSeverity_Display"), ELogVerbosity::Display, FName(TEXT("Angelscript"))),
			TEXT("LogDisplay should use Display verbosity")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageSeverity_Warning"), ELogVerbosity::Warning, FName(TEXT("Angelscript"))),
			TEXT("Warning should use Warning verbosity")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsTextInCategory(TEXT("CoverageLogLevel_Error"), FName(TEXT("Angelscript"))),
			TEXT("Error should emit its message in the Angelscript category")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageSeverity_CategoryLog"), ELogVerbosity::Log, FName(TEXT("CoverageSeverityCategory"))),
			TEXT("category Log should preserve the supplied category")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("[Information] CoverageSeverity_CategoryInfo"), ELogVerbosity::Log, FName(TEXT("CoverageSeverityCategory"))),
			TEXT("category LogInfo should preserve category and information prefix")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("[Display] CoverageSeverity_CategoryDisplay"), ELogVerbosity::Display, FName(TEXT("CoverageSeverityCategory"))),
			TEXT("category LogDisplay should preserve category and Display verbosity")));
		ASSERT_THAT(IsTrue(LogCapture.Contains(TEXT("CoverageSeverity_CategoryWarning"), ELogVerbosity::Warning, FName(TEXT("CoverageSeverityCategory"))),
			TEXT("category Warning should preserve category and Warning verbosity")));
		ASSERT_THAT(IsTrue(LogCapture.ContainsTextInCategory(TEXT("CoverageCategory_Error"), FName(TEXT("CoverageCustomCategory"))),
			TEXT("category Error should preserve category and emit its message")));
	}

	TEST_METHOD(NetworkDebugLoggingPatterns)
	{
		TestRunner->AddExpectedError(TEXT("CoverageNetworkDebug_RPC"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageLogging_NetworkDebug"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageLoggingNetworkDebug.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ANetworkDebugLogTestActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(ReplicatedUsing=OnRep_DebugValue)
				int DebugValue = 0;

				UPROPERTY()
				bool bLoggedAuthority = false;

				UPROPERTY()
				bool bLoggedOnRep = false;

				UPROPERTY()
				bool bLoggedRpc = false;

				void LogRpcDebugValue(int InValue)
				{
					Error("CoverageNetworkDebug_RPC Value=" + InValue);
					bLoggedRpc = true;
				}

				UFUNCTION()
				void OnRep_DebugValue()
				{
					Print("CoverageNetworkDebug_OnRep DebugValue=" + DebugValue);
					bLoggedOnRep = true;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Print("CoverageNetworkDebug_Authority=" + HasAuthority());
					bLoggedAuthority = true;
					DebugValue = 17;
					OnRep_DebugValue();
					LogRpcDebugValue(DebugValue);
				}
			}
			)AS"),
			TEXT("ANetworkDebugLogTestActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("network debug logging actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("network debug logging actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoggedAuthority"), true,
			TEXT("network debug logging should include authority context"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoggedOnRep"), true,
			TEXT("network debug logging should include replication notification context"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bLoggedRpc"), true,
			TEXT("network debug logging should include RPC parameter context"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
