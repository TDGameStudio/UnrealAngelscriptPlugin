#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageLoggingTests
// -----------------------------------------------------------------------------
// Comprehensive logging and debug output coverage for AngelScript, following
// the matrix from Documents/Coverage/Coverage_DebugAndLogging.md.
//
// Test axes covered:
//   * PrintFunctions                - Print, PrintString, PrintWarning, PrintError
//   * UELogMacros                   - UE_LOG with different verbosity levels
//   * LogCategories                 - Different log categories (LogTemp, LogScript, etc.)
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

					// PrintString with duration and color (if supported)
					PrintString("Colored message on screen", 5.0f, FLinearColor::Red);

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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Print functions actor should spawn")));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UE_LOG macros actor should spawn")));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Log categories actor should spawn")));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Log formatting actor should spawn")));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Conditional logging actor should spawn")));

		// Verify properties
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDebugMode"), true, TEXT("bDebugMode should be true"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LogLevel"), 2, TEXT("LogLevel should be 2"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Function entry/exit logging actor should spawn")));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Performance conscious logging actor should spawn")));

		// Verify tick counter initialization
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TickCounter"), 0, TEXT("TickCounter should be 0"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LogInterval"), 60, TEXT("LogInterval should be 60"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Context-rich logging actor should spawn")));

		// Verify context properties
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PlayerID"), 12345, TEXT("PlayerID should be 12345"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
