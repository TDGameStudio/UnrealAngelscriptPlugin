#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMacrosTests
// -----------------------------------------------------------------------------
// Comprehensive coverage for advanced AngelScript macro usage scenarios.
// Based on Documents/Coverage/Coverage_OtherMacros.md.
//
// Coverage matrix:
//   * UEnumAdvancedDeclaration      - UENUM with BlueprintType, DisplayName, ToolTip
//   * UStructAdvancedUsage          - Complex USTRUCT with nested types, operators
//   * UParamModifiers               - UPARAM usage in function parameters
//   * BlueprintImplementableEvent   - Blueprint-only events (C++ calls, BP implements)
//   * BlueprintNativeEvent          - Events with C++ default and BP override capability
//
// Note: AngelScript treats BlueprintImplementableEvent and BlueprintNativeEvent
// differently than C++. In AS, these are used for cross-language boundaries:
// - BlueprintImplementableEvent: AS declares, Blueprint implements
// - BlueprintNativeEvent: AS provides default, Blueprint can override
//
// Pattern: spawn AS actor, test advanced macro scenarios, validate through
// reflection and runtime behavior verification.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMacrosTest,
	"Angelscript.TestModule.Coverage.Macros",
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
	// UENUM advanced declaration with BlueprintType, DisplayName, ToolTip
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumAdvancedDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UEnumAdvanced"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUEnum.as"),
			ASTEST_AS(R"AS(
			// UENUM with BlueprintType specifier
			UENUM(BlueprintType)
			enum EAdvancedEnum
			{
				Option_A UMETA(DisplayName="First Option", ToolTip="This is the first option"),
				Option_B UMETA(DisplayName="Second Option", ToolTip="This is the second option"),
				Option_C UMETA(DisplayName="Third Option", Hidden),
				Option_MAX UMETA(Hidden)
			}

			// UENUM with category and display customization
			UENUM(BlueprintType)
			enum EDisplayEnum
			{
				Low UMETA(DisplayName="Low Priority"),
				Medium UMETA(DisplayName="Medium Priority"),
				High UMETA(DisplayName="High Priority")
			}

			UCLASS()
			class ACoverageMacrosUEnumActor : AActor
			{
				UPROPERTY(BlueprintReadWrite, Category="Enums")
				EAdvancedEnum AdvancedValue = EAdvancedEnum::Option_A;

				UPROPERTY(BlueprintReadWrite, Category="Enums")
				EDisplayEnum DisplayValue = EDisplayEnum::Medium;

				UPROPERTY()
				int TestResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test enum assignment and comparison
					AdvancedValue = EAdvancedEnum::Option_B;
					check(AdvancedValue == EAdvancedEnum::Option_B);

					// Test enum in switch statement
					switch (AdvancedValue)
					{
						case EAdvancedEnum::Option_A:
							TestResult = 1;
							break;
						case EAdvancedEnum::Option_B:
							TestResult = 2;
							break;
						case EAdvancedEnum::Option_C:
							TestResult = 3;
							break;
						default:
							TestResult = 0;
					}

					// Test display enum
					DisplayValue = EDisplayEnum::High;
					check(int(DisplayValue) == 2);
				}
			}
			)AS"),
			TEXT("ACoverageMacrosUEnumActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Advanced UENUM actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Advanced UENUM actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify enum values were set correctly
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TestResult"), 2,
			TEXT("Switch statement should set TestResult to 2 for Option_B"));

		// Verify UENUM property has correct metadata
		FProperty* EnumProp = ScriptClass->FindPropertyByName(TEXT("AdvancedValue"));
		ASSERT_THAT(IsNotNull(EnumProp, TEXT("AdvancedValue property should exist")));

		FEnumProperty* EnumProperty = CastField<FEnumProperty>(EnumProp);
		if (EnumProperty)
		{
			UEnum* EnumType = EnumProperty->GetEnum();
			ASSERT_THAT(IsNotNull(EnumType, TEXT("Enum type should be accessible")));
			ASSERT_THAT(IsTrue(EnumType->HasMetaData(TEXT("BlueprintType")),
				TEXT("Enum should have BlueprintType metadata")));
		}
	}

	// -------------------------------------------------------------------------
	// USTRUCT advanced usage with nested types, constructors, and operators
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructAdvancedUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UStructAdvanced"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUStruct.as"),
			ASTEST_AS(R"AS(
			// Advanced USTRUCT with custom constructor and operators
			USTRUCT(BlueprintType)
			struct FAdvancedStruct
			{
				UPROPERTY(BlueprintReadWrite, Category="Data")
				int Value = 0;

				UPROPERTY(BlueprintReadWrite, Category="Data")
				FString Name = "Default";

				UPROPERTY(BlueprintReadWrite, Category="Data")
				FVector Position;

				// Custom constructor
				FAdvancedStruct()
				{
					Value = 0;
					Name = "Default";
					Position = FVector(0, 0, 0);
				}

				FAdvancedStruct(int InValue, FString InName)
				{
					Value = InValue;
					Name = InName;
					Position = FVector(0, 0, 0);
				}

				// Operator overloads
				bool opEquals(const FAdvancedStruct&in Other) const
				{
					return Value == Other.Value && Name == Other.Name;
				}

				int opCmp(const FAdvancedStruct&in Other) const
				{
					if (Value < Other.Value) return -1;
					if (Value > Other.Value) return 1;
					return 0;
				}

				FAdvancedStruct opAdd(const FAdvancedStruct&in Other) const
				{
					FAdvancedStruct Result;
					Result.Value = Value + Other.Value;
					Result.Name = Name + Other.Name;
					Result.Position = Position + Other.Position;
					return Result;
				}
			}

			// Nested struct within struct
			USTRUCT(BlueprintType)
			struct FComplexStruct
			{
				UPROPERTY(BlueprintReadWrite)
				FAdvancedStruct Inner;

				UPROPERTY(BlueprintReadWrite)
				TArray<FAdvancedStruct> StructArray;

				UPROPERTY(BlueprintReadWrite)
				TMap<int, FAdvancedStruct> StructMap;
			}

			UCLASS()
			class ACoverageMacrosUStructActor : AActor
			{
				UPROPERTY(BlueprintReadWrite)
				FAdvancedStruct Data1;

				UPROPERTY(BlueprintReadWrite)
				FAdvancedStruct Data2;

				UPROPERTY(BlueprintReadWrite)
				FComplexStruct ComplexData;

				UPROPERTY()
				int ComparisonResult = -1;

				UPROPERTY()
				int AdditionValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test constructor
					Data1 = FAdvancedStruct(100, "First");
					Data2 = FAdvancedStruct(200, "Second");

					// Test comparison operators
					ComparisonResult = (Data1 == Data2) ? 0 : 1;
					check(ComparisonResult == 1);

					// Test addition operator
					FAdvancedStruct Sum = Data1 + Data2;
					AdditionValue = Sum.Value;
					check(AdditionValue == 300);
					check(Sum.Name == "FirstSecond");

					// Test nested struct
					ComplexData.Inner = FAdvancedStruct(500, "Inner");
					ComplexData.StructArray.Add(Data1);
					ComplexData.StructArray.Add(Data2);
					ComplexData.StructMap.Add(1, Data1);
					ComplexData.StructMap.Add(2, Data2);

					// Verify array access
					check(ComplexData.StructArray.Num() == 2);
					check(ComplexData.StructArray[0].Value == 100);
					check(ComplexData.StructArray[1].Value == 200);

					// Verify map access
					check(ComplexData.StructMap.Num() == 2);
					check(ComplexData.StructMap[1].Value == 100);
					check(ComplexData.StructMap[2].Value == 200);
				}
			}
			)AS"),
			TEXT("ACoverageMacrosUStructActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Advanced USTRUCT actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Advanced USTRUCT actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify struct operations
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComparisonResult"), 1,
			TEXT("Struct comparison should return 1 for different structs"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AdditionValue"), 300,
			TEXT("Struct addition should sum values"));

		// Verify nested struct
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComplexData.Inner.Value"), 500,
			TEXT("Nested struct value should be set"));
	}

	// -------------------------------------------------------------------------
	// UPARAM parameter modifiers in functions
	// -------------------------------------------------------------------------
	TEST_METHOD(UParamModifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UPARAM"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUPARAM.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMacrosUParamActor : AActor
			{
				UPROPERTY()
				int ResultValue = 0;

				UPROPERTY()
				FString ResultString;

				// UPARAM with DisplayName
				UFUNCTION(BlueprintCallable, Category="Testing")
				void ProcessValue(
					UPARAM(DisplayName="Input Number") int InValue,
					UPARAM(DisplayName="Multiplier") int Multiplier,
					UPARAM(DisplayName="Result", ref) int&out OutResult)
				{
					OutResult = InValue * Multiplier;
				}

				// UPARAM with ref modifier for output parameters
				UFUNCTION(BlueprintCallable, Category="Testing")
				void SplitValue(
					UPARAM(DisplayName="Input") int Value,
					UPARAM(ref) int&out Half1,
					UPARAM(ref) int&out Half2)
				{
					Half1 = Value / 2;
					Half2 = Value - Half1;
				}

				// UPARAM with const ref input
				UFUNCTION(BlueprintCallable, Category="Testing")
				void ProcessString(
					UPARAM(DisplayName="Input Text") const FString&in Input,
					UPARAM(DisplayName="Output Text", ref) FString&out Output)
				{
					Output = Input + " Processed";
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					int Result;
					ProcessValue(10, 5, Result);
					ResultValue = Result;
					check(ResultValue == 50);

					int Half1, Half2;
					SplitValue(100, Half1, Half2);
					check(Half1 == 50);
					check(Half2 == 50);

					FString Output;
					ProcessString("Test", Output);
					ResultString = Output;
					check(ResultString == "Test Processed");
				}
			}
			)AS"),
			TEXT("ACoverageMacrosUParamActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UPARAM actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UPARAM actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify UPARAM functions executed correctly
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultValue"), 50,
			TEXT("UPARAM function should calculate result correctly"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ResultString"),
			FString(TEXT("Test Processed")), TEXT("UPARAM string function should process text"));

		// Verify UFUNCTION metadata
		UFunction* ProcessFunc = ScriptClass->FindFunctionByName(TEXT("ProcessValue"));
		ASSERT_THAT(IsNotNull(ProcessFunc, TEXT("ProcessValue function should exist")));
	}

	// -------------------------------------------------------------------------
	// BlueprintImplementableEvent - Blueprint-only implementation
	// Note: In AngelScript, this is primarily used for cross-language scenarios
	// where AS declares an event that Blueprint classes can implement.
	// For pure AS testing, we verify the declaration compiles correctly.
	// -------------------------------------------------------------------------
	TEST_METHOD(BlueprintImplementableEvent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_BPImplEvent"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragesMacrosBPImplEvent.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragesMacrosBPImplEventActor : AActor
			{
				UPROPERTY()
				int EventCallCount = 0;

				UPROPERTY()
				int EventValue = 0;

				// BlueprintImplementableEvent - can be implemented in Blueprint
				UFUNCTION(BlueprintImplementableEvent, Category="Events")
				void OnCustomEvent(int Value);

				// BlueprintImplementableEvent with return value
				UFUNCTION(BlueprintImplementableEvent, Category="Events")
				int CalculateValue(int Input);

				// BlueprintImplementableEvent with multiple parameters
				UFUNCTION(BlueprintImplementableEvent, Category="Events")
				void OnComplexEvent(int IntParam, FString StringParam, FVector VectorParam);

				// Function that would call the event (in real usage)
				UFUNCTION(BlueprintCallable, Category="Testing")
				void TriggerEvent(int Value)
				{
					// In a real scenario with Blueprint child class, this would call the BP implementation
					// For pure AS testing, we just verify the signature is correct
					EventCallCount++;
					EventValue = Value;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TriggerEvent(42);
				}
			}
			)AS"),
			TEXT("ACoveragesMacrosBPImplEventActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintImplementableEvent actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintImplementableEvent actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify function metadata
		UFunction* EventFunc = ScriptClass->FindFunctionByName(TEXT("OnCustomEvent"));
		ASSERT_THAT(IsNotNull(EventFunc, TEXT("OnCustomEvent should exist")));
		ASSERT_THAT(IsTrue(EventFunc->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("OnCustomEvent should have BlueprintEvent flag")));

		UFunction* CalcFunc = ScriptClass->FindFunctionByName(TEXT("CalculateValue"));
		ASSERT_THAT(IsNotNull(CalcFunc, TEXT("CalculateValue should exist")));

		UFunction* ComplexFunc = ScriptClass->FindFunctionByName(TEXT("OnComplexEvent"));
		ASSERT_THAT(IsNotNull(ComplexFunc, TEXT("OnComplexEvent should exist")));

		// Verify trigger function worked
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("Event trigger function should increment counter"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventValue"), 42,
			TEXT("Event trigger function should set value"));
	}

	// -------------------------------------------------------------------------
	// BlueprintNativeEvent - Event with native implementation and BP override
	// Note: In AngelScript, this allows AS to provide a default implementation
	// that Blueprint child classes can optionally override.
	// -------------------------------------------------------------------------
	TEST_METHOD(BlueprintNativeEvent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_BPNativeEvent"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosBPNativeEvent.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMacrosBPNativeEventActor : AActor
			{
				UPROPERTY()
				int NativeEventResult = 0;

				UPROPERTY()
				FString NativeEventString;

				// BlueprintNativeEvent with default implementation
				UFUNCTION(BlueprintNativeEvent, Category="Events")
				int ProcessNativeEvent(int Input)
				{
					// Default implementation - can be overridden in Blueprint
					return Input * 2;
				}

				// BlueprintNativeEvent with string processing
				UFUNCTION(BlueprintNativeEvent, Category="Events")
				FString FormatNativeEvent(const FString&in Input, int Count)
				{
					FString Result = Input;
					for (int i = 1; i < Count; i++)
					{
						Result = Result + " " + Input;
					}
					return Result;
				}

				// BlueprintNativeEvent with void return
				UFUNCTION(BlueprintNativeEvent, Category="Events")
				void ExecuteNativeEvent(int Value)
				{
					NativeEventResult = Value * 3;
				}

				// Function that calls native events
				UFUNCTION(BlueprintCallable, Category="Testing")
				void TestNativeEvents()
				{
					// Call native event with default implementation
					int Result = ProcessNativeEvent(10);
					NativeEventResult = Result;

					// Call string native event
					NativeEventString = FormatNativeEvent("Test", 3);

					// Call void native event
					ExecuteNativeEvent(5);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TestNativeEvents();
				}
			}
			)AS"),
			TEXT("ACoverageMacrosBPNativeEventActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintNativeEvent actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintNativeEvent actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify native event function metadata
		UFunction* ProcessFunc = ScriptClass->FindFunctionByName(TEXT("ProcessNativeEvent"));
		ASSERT_THAT(IsNotNull(ProcessFunc, TEXT("ProcessNativeEvent should exist")));
		ASSERT_THAT(IsTrue(ProcessFunc->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("ProcessNativeEvent should have BlueprintEvent flag")));

		UFunction* FormatFunc = ScriptClass->FindFunctionByName(TEXT("FormatNativeEvent"));
		ASSERT_THAT(IsNotNull(FormatFunc, TEXT("FormatNativeEvent should exist")));

		UFunction* ExecuteFunc = ScriptClass->FindFunctionByName(TEXT("ExecuteNativeEvent"));
		ASSERT_THAT(IsNotNull(ExecuteFunc, TEXT("ExecuteNativeEvent should exist")));

		// Verify native event default implementations executed
		// Note: ExecuteNativeEvent overwrites NativeEventResult, so we check its result
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NativeEventResult"), 15,
			TEXT("ExecuteNativeEvent should set result to 5 * 3 = 15"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NativeEventString"),
			FString(TEXT("Test Test Test")), TEXT("FormatNativeEvent should repeat string"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
