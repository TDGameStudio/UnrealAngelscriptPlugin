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
// AngelscriptCoverageUStructTests
// -----------------------------------------------------------------------------
// Comprehensive USTRUCT coverage for AngelScript, following the matrix from
// Documents/Coverage/Coverage_OtherMacros.md section 1 (USTRUCT).
//
// Test axes covered:
//   * UStructBasicDeclaration  - USTRUCT(), plain struct, nested struct
//   * UStructSpecifiers        - BlueprintType, Atomic specifiers
//   * UStructMembers           - Various member types (int, FString, FVector, etc.)
//   * UStructValueSemantics    - Copy, assignment, comparison, defaults
//   * UStructOperators         - opEquals, opAdd, opCmp overloads
//   * UStructAsParameter       - Value, &in, &out parameters
//   * UStructAsReturn          - Returning struct from functions
//   * UStructInContainers      - TArray<FStruct>, TMap with struct keys/values
//   * UStructNested            - Nested struct within struct
//
// Pattern: spawn AS actor, drive members via AngelScript, validate through
// FProperty reflection (GetByPath/SetByPath/VerifyByPath).
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUStructTest,
	"Angelscript.TestModule.Coverage.UStruct",
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
	// Basic struct declarations: USTRUCT(), plain struct, nested struct
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructBasicDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_BasicDecl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructBasicDecl.as"),
			ASTEST_AS(R"AS(
			// USTRUCT() - minimal declaration
			USTRUCT()
			struct FSimpleStruct
			{
				UPROPERTY()
				int Value = 42;
			}

			// Plain struct without USTRUCT (script-only)
			struct FPlainStruct
			{
				int X = 10;
				int Y = 20;
			}

			// Nested struct
			USTRUCT()
			struct FNestedOuter
			{
				UPROPERTY()
				int OuterValue = 100;

				UPROPERTY()
				FSimpleStruct InnerStruct;
			}

			UCLASS()
			class ACoverageStructBasicActor : AActor
			{
				UPROPERTY()
				FSimpleStruct SimpleData;

				UPROPERTY()
				FNestedOuter NestedData;

				FPlainStruct PlainData;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SimpleData.Value = 99;
					NestedData.OuterValue = 200;
					NestedData.InnerStruct.Value = 300;
					PlainData.X = 50;
					PlainData.Y = 75;
				}
			}
			)AS"),
			TEXT("ACoverageStructBasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct basic declaration actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct basic declaration actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify USTRUCT members
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SimpleData.Value"), 99, TEXT("FSimpleStruct.Value should be set"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.OuterValue"), 200, TEXT("FNestedOuter.OuterValue should be set"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.InnerStruct.Value"), 300, TEXT("Nested FSimpleStruct.Value should be set"));
	}

	// -------------------------------------------------------------------------
	// USTRUCT specifiers: BlueprintType, Atomic
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructSpecifiers.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FBlueprintTypeStruct
			{
				UPROPERTY()
				int Value = 10;
			}

			USTRUCT(Atomic)
			struct FAtomicStruct
			{
				UPROPERTY()
				int X = 5;

				UPROPERTY()
				int Y = 15;
			}

			UCLASS()
			class ACoverageStructSpecifierActor : AActor
			{
				UPROPERTY()
				FBlueprintTypeStruct BPData;

				UPROPERTY()
				FAtomicStruct AtomicData;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BPData.Value = 100;
					AtomicData.X = 50;
					AtomicData.Y = 150;
				}
			}
			)AS"),
			TEXT("ACoverageStructSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct specifier actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct specifier actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BPData.Value"), 100, TEXT("BlueprintType struct should work"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AtomicData.X"), 50, TEXT("Atomic struct X should work"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AtomicData.Y"), 150, TEXT("Atomic struct Y should work"));
	}
	// -------------------------------------------------------------------------
	// USTRUCT members: various types (int, float, bool, FString, FVector, AActor, TArray)
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructMembers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Members"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMembers.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FComplexStruct
			{
				UPROPERTY(EditAnywhere)
				int IntValue = 0;

				UPROPERTY(EditAnywhere)
				float FloatValue = 0.0f;

				UPROPERTY(EditAnywhere)
				bool BoolValue = false;

				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FString StringValue;

				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FName NameValue;

				UPROPERTY(EditAnywhere)
				FVector VectorValue;

				UPROPERTY()
				AActor ActorRef;

				UPROPERTY()
				TArray<int> IntArray;

				UPROPERTY()
				TArray<FString> StringArray;
			}

			UCLASS()
			class ACoverageStructMemberActor : AActor
			{
				UPROPERTY()
				FComplexStruct Data;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Data.IntValue = 42;
					Data.FloatValue = 3.14f;
					Data.BoolValue = true;
					Data.StringValue = "Hello";
					Data.NameValue = n"TestName";
					Data.VectorValue = FVector(1.0f, 2.0f, 3.0f);
					Data.ActorRef = this;
					Data.IntArray.Add(10);
					Data.IntArray.Add(20);
					Data.StringArray.Add("First");
					Data.StringArray.Add("Second");
				}
			}
			)AS"),
			TEXT("ACoverageStructMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct members actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct members actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.IntValue"), 42, TEXT("Struct int member"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("Data.FloatValue"), 3.14f, TEXT("Struct float member"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("Data.BoolValue"), true, TEXT("Struct bool member"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.StringValue"), FString(TEXT("Hello")), TEXT("Struct FString member"));
		VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("Data.NameValue"), FName(TEXT("TestName")), TEXT("Struct FName member"));
		
		// Verify FVector
		FVector VectorResult(0.0f);
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("Data.VectorValue"), VectorResult), TEXT("Get FVector from struct")));
		ASSERT_THAT(IsTrue(VectorResult.Equals(FVector(1.0f, 2.0f, 3.0f)), TEXT("Struct FVector member should match")));

		// Verify AActor reference
		UObject* ActorRefObj = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("Data.ActorRef"), ActorRefObj), TEXT("Get AActor ref from struct")));
		AActor* ActorRef = Cast<AActor>(ActorRefObj);
		ASSERT_THAT(AreEqual(Actor, ActorRef, TEXT("Struct AActor reference should point to self")));

		// Verify TArray<int>
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.IntArray[0]"), 10, TEXT("Struct TArray<int>[0]"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.IntArray[1]"), 20, TEXT("Struct TArray<int>[1]"));

		// Verify TArray<FString>
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.StringArray[0]"), FString(TEXT("First")), TEXT("Struct TArray<FString>[0]"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.StringArray[1]"), FString(TEXT("Second")), TEXT("Struct TArray<FString>[1]"));
	}
	// -------------------------------------------------------------------------
	// USTRUCT value semantics: copy construction, assignment, comparison, defaults
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructValueSemantics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_ValueSemantics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructValueSemantics.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FValueStruct
			{
				UPROPERTY()
				int X = 10;

				UPROPERTY()
				int Y = 20;

				UPROPERTY()
				FString Name = "Default";
			}

			UCLASS()
			class ACoverageStructValueActor : AActor
			{
				UPROPERTY()
				FValueStruct Original;

				UPROPERTY()
				FValueStruct CopyConstructed;

				UPROPERTY()
				FValueStruct Assigned;

				UPROPERTY()
				bool AreEqual = false;

				UPROPERTY()
				bool AreNotEqual = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test default values
					Original.X = 100;
					Original.Y = 200;
					Original.Name = "Original";

					// Copy construction
					CopyConstructed = Original;

					// Assignment
					Assigned.X = 0;
					Assigned.Y = 0;
					Assigned.Name = "Temp";
					Assigned = Original;

					// Comparison
					AreEqual = (CopyConstructed == Original);
					
					FValueStruct Different;
					Different.X = 999;
					AreNotEqual = (Different != Original);
				}
			}
			)AS"),
			TEXT("ACoverageStructValueActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct value semantics actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct value semantics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify original
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Original.X"), 100, TEXT("Original.X"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Original.Y"), 200, TEXT("Original.Y"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Original.Name"), FString(TEXT("Original")), TEXT("Original.Name"));

		// Verify copy construction
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CopyConstructed.X"), 100, TEXT("CopyConstructed.X should match original"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CopyConstructed.Y"), 200, TEXT("CopyConstructed.Y should match original"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("CopyConstructed.Name"), FString(TEXT("Original")), TEXT("CopyConstructed.Name should match original"));

		// Verify assignment
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Assigned.X"), 100, TEXT("Assigned.X should match original after assignment"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Assigned.Y"), 200, TEXT("Assigned.Y should match original after assignment"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Assigned.Name"), FString(TEXT("Original")), TEXT("Assigned.Name should match original after assignment"));

		// Verify comparison operators
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AreEqual"), true, TEXT("Structs with same values should be equal"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AreNotEqual"), true, TEXT("Structs with different values should not be equal"));
	}
	// -------------------------------------------------------------------------
	// USTRUCT operator overloads: opEquals, opAdd, opCmp
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Operators"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructOperators.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FOperatorStruct
			{
				UPROPERTY()
				int X = 0;

				UPROPERTY()
				int Y = 0;

				bool opEquals(const FOperatorStruct& Other) const
				{
					return X == Other.X && Y == Other.Y;
				}

				FOperatorStruct opAdd(const FOperatorStruct& Other) const
				{
					FOperatorStruct Result;
					Result.X = X + Other.X;
					Result.Y = Y + Other.Y;
					return Result;
				}

				int opCmp(const FOperatorStruct& Other) const
				{
					if (X < Other.X) return -1;
					if (X > Other.X) return 1;
					if (Y < Other.Y) return -1;
					if (Y > Other.Y) return 1;
					return 0;
				}
			}

			UCLASS()
			class ACoverageStructOperatorActor : AActor
			{
				UPROPERTY()
				FOperatorStruct A;

				UPROPERTY()
				FOperatorStruct B;

				UPROPERTY()
				FOperatorStruct Sum;

				UPROPERTY()
				bool AreEqual = false;

				UPROPERTY()
				bool ALessThanB = false;

				UPROPERTY()
				bool AGreaterThanB = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					A.X = 10;
					A.Y = 20;

					B.X = 5;
					B.Y = 15;

					// opAdd
					Sum = A + B;

					// opEquals
					FOperatorStruct ACopy;
					ACopy.X = 10;
					ACopy.Y = 20;
					AreEqual = (A == ACopy);

					// opCmp
					ALessThanB = (B < A);
					AGreaterThanB = (A > B);
				}
			}
			)AS"),
			TEXT("ACoverageStructOperatorActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct operators actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct operators actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify opAdd result
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Sum.X"), 15, TEXT("opAdd should sum X values"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Sum.Y"), 35, TEXT("opAdd should sum Y values"));

		// Verify opEquals
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AreEqual"), true, TEXT("opEquals should return true for equal structs"));

		// Verify opCmp
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ALessThanB"), true, TEXT("opCmp should support less-than comparison"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AGreaterThanB"), true, TEXT("opCmp should support greater-than comparison"));
	}
	// -------------------------------------------------------------------------
	// USTRUCT as parameter: value, &in, &out
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructAsParameter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Parameter"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructParameter.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FParamStruct
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				FString Name;
			}

			UCLASS()
			class ACoverageStructParamActor : AActor
			{
				UPROPERTY()
				FParamStruct ValueParam;

				UPROPERTY()
				FParamStruct InParam;

				UPROPERTY()
				FParamStruct OutParam;

				void ModifyByValue(FParamStruct Param)
				{
					Param.Value = 999;  // Should not affect caller
				}

				void ReadByConstRef(const FParamStruct&in Param)
				{
					InParam.Value = Param.Value;
					InParam.Name = Param.Name;
				}

				void WriteByRef(FParamStruct&out Param)
				{
					Param.Value = 777;
					Param.Name = "Modified";
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test value parameter (copy)
					ValueParam.Value = 100;
					ValueParam.Name = "Original";
					ModifyByValue(ValueParam);
					// ValueParam should remain 100

					// Test &in parameter (read-only reference)
					FParamStruct Source;
					Source.Value = 200;
					Source.Name = "Source";
					ReadByConstRef(Source);

					// Test &out parameter (write reference)
					WriteByRef(OutParam);
				}
			}
			)AS"),
			TEXT("ACoverageStructParamActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct parameter actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct parameter actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify value parameter was not modified
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ValueParam.Value"), 100, TEXT("Value parameter should not be modified"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ValueParam.Name"), FString(TEXT("Original")), TEXT("Value parameter name should not be modified"));

		// Verify &in parameter read
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InParam.Value"), 200, TEXT("&in parameter should read value"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("InParam.Name"), FString(TEXT("Source")), TEXT("&in parameter should read name"));

		// Verify &out parameter write
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OutParam.Value"), 777, TEXT("&out parameter should write value"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("OutParam.Name"), FString(TEXT("Modified")), TEXT("&out parameter should write name"));
	}
	// -------------------------------------------------------------------------
	// USTRUCT as return value
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructAsReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Return"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructReturn.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FReturnStruct
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FString Description;

				UPROPERTY()
				FVector Position;
			}

			UCLASS()
			class ACoverageStructReturnActor : AActor
			{
				UPROPERTY()
				FReturnStruct Result;

				FReturnStruct CreateStruct(int InID, FString InDesc)
				{
					FReturnStruct New;
					New.ID = InID;
					New.Description = InDesc;
					New.Position = FVector(InID * 10.0f, InID * 20.0f, InID * 30.0f);
					return New;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Result = CreateStruct(42, "Test Result");
				}
			}
			)AS"),
			TEXT("ACoverageStructReturnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct return actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct return actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify returned struct values
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Result.ID"), 42, TEXT("Returned struct ID should be set"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result.Description"), FString(TEXT("Test Result")), TEXT("Returned struct Description should be set"));

		FVector VectorResult(0.0f);
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("Result.Position"), VectorResult), TEXT("Get returned struct Position")));
		ASSERT_THAT(IsTrue(VectorResult.Equals(FVector(420.0f, 840.0f, 1260.0f)), TEXT("Returned struct Position should be calculated correctly")));
	}
	// -------------------------------------------------------------------------
	// USTRUCT in containers: TArray<FStruct>, TMap with struct keys/values
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructInContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Containers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructContainers.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FItemStruct
			{
				UPROPERTY()
				int ItemID = 0;

				UPROPERTY()
				FString ItemName;

				UPROPERTY()
				float Weight = 0.0f;

				bool opEquals(const FItemStruct& Other) const
				{
					return ItemID == Other.ItemID;
				}

				int opCmp(const FItemStruct& Other) const
				{
					if (ItemID < Other.ItemID) return -1;
					if (ItemID > Other.ItemID) return 1;
					return 0;
				}
			}

			UCLASS()
			class ACoverageStructContainerActor : AActor
			{
				UPROPERTY()
				TArray<FItemStruct> ItemArray;

				UPROPERTY()
				TMap<int, FItemStruct> IDToItemMap;

				UPROPERTY()
				TMap<FString, FItemStruct> NameToItemMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Populate TArray<FStruct>
					FItemStruct Item1;
					Item1.ItemID = 1;
					Item1.ItemName = "Sword";
					Item1.Weight = 5.0f;
					ItemArray.Add(Item1);

					FItemStruct Item2;
					Item2.ItemID = 2;
					Item2.ItemName = "Shield";
					Item2.Weight = 10.0f;
					ItemArray.Add(Item2);

					FItemStruct Item3;
					Item3.ItemID = 3;
					Item3.ItemName = "Potion";
					Item3.Weight = 0.5f;
					ItemArray.Add(Item3);

					// Populate TMap<int, FStruct>
					IDToItemMap.Add(1, Item1);
					IDToItemMap.Add(2, Item2);
					IDToItemMap.Add(3, Item3);

					// Populate TMap<FString, FStruct>
					NameToItemMap.Add("Sword", Item1);
					NameToItemMap.Add("Shield", Item2);
					NameToItemMap.Add("Potion", Item3);
				}
			}
			)AS"),
			TEXT("ACoverageStructContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct container actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify TArray<FStruct>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ItemArray"), Length), TEXT("TArray<FStruct> length should resolve")));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FStruct> should have 3 elements")));
		}
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ItemArray[0].ItemID"), 1, TEXT("TArray<FStruct>[0].ItemID"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ItemArray[0].ItemName"), FString(TEXT("Sword")), TEXT("TArray<FStruct>[0].ItemName"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ItemArray[0].Weight"), 5.0f, TEXT("TArray<FStruct>[0].Weight"));

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ItemArray[1].ItemID"), 2, TEXT("TArray<FStruct>[1].ItemID"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ItemArray[1].ItemName"), FString(TEXT("Shield")), TEXT("TArray<FStruct>[1].ItemName"));

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ItemArray[2].ItemID"), 3, TEXT("TArray<FStruct>[2].ItemID"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("ItemArray[2].Weight"), 0.5f, TEXT("TArray<FStruct>[2].Weight"));

		// Verify TMap<int, FStruct>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IDToItemMap"), Count), TEXT("TMap<int, FStruct> length should resolve")));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int, FStruct> should have 3 entries")));
		}

		// Verify TMap<FString, FStruct>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToItemMap"), Count), TEXT("TMap<FString, FStruct> length should resolve")));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<FString, FStruct> should have 3 entries")));
		}
	}
	// -------------------------------------------------------------------------
	// USTRUCT nested: struct within struct
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructNested)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Nested"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructNested.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FInnerStruct
			{
				UPROPERTY()
				int InnerValue = 0;

				UPROPERTY()
				FString InnerName;
			}

			USTRUCT()
			struct FMiddleStruct
			{
				UPROPERTY()
				int MiddleValue = 0;

				UPROPERTY()
				FInnerStruct InnerData;

				UPROPERTY()
				TArray<FInnerStruct> InnerArray;
			}

			USTRUCT()
			struct FOuterStruct
			{
				UPROPERTY()
				int OuterValue = 0;

				UPROPERTY()
				FMiddleStruct MiddleData;

				UPROPERTY()
				FInnerStruct DirectInner;

				UPROPERTY()
				TArray<FMiddleStruct> MiddleArray;
			}

			UCLASS()
			class ACoverageStructNestedActor : AActor
			{
				UPROPERTY()
				FOuterStruct NestedData;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set outer level
					NestedData.OuterValue = 100;

					// Set middle level
					NestedData.MiddleData.MiddleValue = 200;

					// Set inner level (through middle)
					NestedData.MiddleData.InnerData.InnerValue = 300;
					NestedData.MiddleData.InnerData.InnerName = "DeepInner";

					// Set direct inner
					NestedData.DirectInner.InnerValue = 400;
					NestedData.DirectInner.InnerName = "DirectInner";

					// Set array of inner structs in middle
					FInnerStruct Inner1;
					Inner1.InnerValue = 301;
					Inner1.InnerName = "Inner1";
					NestedData.MiddleData.InnerArray.Add(Inner1);

					FInnerStruct Inner2;
					Inner2.InnerValue = 302;
					Inner2.InnerName = "Inner2";
					NestedData.MiddleData.InnerArray.Add(Inner2);

					// Set array of middle structs
					FMiddleStruct Middle1;
					Middle1.MiddleValue = 201;
					Middle1.InnerData.InnerValue = 311;
					Middle1.InnerData.InnerName = "MiddleArray1Inner";
					NestedData.MiddleArray.Add(Middle1);

					FMiddleStruct Middle2;
					Middle2.MiddleValue = 202;
					Middle2.InnerData.InnerValue = 312;
					Middle2.InnerData.InnerName = "MiddleArray2Inner";
					NestedData.MiddleArray.Add(Middle2);
				}
			}
			)AS"),
			TEXT("ACoverageStructNestedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct nested actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct nested actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify outer level
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.OuterValue"), 100, TEXT("Outer level value"));

		// Verify middle level
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleData.MiddleValue"), 200, TEXT("Middle level value"));

		// Verify inner level (3 levels deep)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerData.InnerValue"), 300, TEXT("Inner level value (3 deep)"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerData.InnerName"), FString(TEXT("DeepInner")), TEXT("Inner level name (3 deep)"));

		// Verify direct inner (2 levels deep)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.DirectInner.InnerValue"), 400, TEXT("Direct inner value"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.DirectInner.InnerName"), FString(TEXT("DirectInner")), TEXT("Direct inner name"));

		// Verify array of inner structs in middle
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerArray[0].InnerValue"), 301, TEXT("InnerArray[0].InnerValue"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerArray[0].InnerName"), FString(TEXT("Inner1")), TEXT("InnerArray[0].InnerName"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerArray[1].InnerValue"), 302, TEXT("InnerArray[1].InnerValue"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerArray[1].InnerName"), FString(TEXT("Inner2")), TEXT("InnerArray[1].InnerName"));

		// Verify array of middle structs with nested inner structs
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[0].MiddleValue"), 201, TEXT("MiddleArray[0].MiddleValue"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[0].InnerData.InnerValue"), 311, TEXT("MiddleArray[0].InnerData.InnerValue"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[0].InnerData.InnerName"), FString(TEXT("MiddleArray1Inner")), TEXT("MiddleArray[0].InnerData.InnerName"));

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[1].MiddleValue"), 202, TEXT("MiddleArray[1].MiddleValue"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[1].InnerData.InnerValue"), 312, TEXT("MiddleArray[1].InnerData.InnerValue"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[1].InnerData.InnerName"), FString(TEXT("MiddleArray2Inner")), TEXT("MiddleArray[1].InnerData.InnerName"));
	}
};

#endif
