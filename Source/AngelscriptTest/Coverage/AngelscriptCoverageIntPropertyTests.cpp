#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Math/NumericLimits.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageIntPropertyTests
// -----------------------------------------------------------------------------
// "Übershader-style" coverage for AngelScript integer-family UPROPERTYs, the
// first slice of the AS full-coverage matrix (Documents/AS_FullCoverageMatrix.md
// section 1). Each TEST_METHOD walks one usage axis from the matrix against the
// whole int family:
//
//   int8 / int16 / int (int32) / int64 / uint8 / uint16 / uint (uint32) / uint64
//
// Axes covered here:
//   * IntFamilyDeclarationDefaults - declaration-time default values reflect
//                                    back through the UE property taxonomy.
//   * IntFamilyWriteRoundTrip      - C++ -> property -> C++ round-trip via path.
//   * IntFamilyBoundaryValues      - min / max edge values per width.
//   * IntContainerProperties       - TArray<int>/<int64>/<uint8> + TMap<int,*>.
//   * IntPropertySpecifierFlags    - full UPROPERTY specifier permutation set
//                                    (Edit/Visible/Blueprint access, standalone
//                                    flag specifiers, meta keys, combinations)
//                                    -> CPF_* / meta mapping.
//
// Pattern D (UPROPERTY path read/write) from the Angelscript test guide: spawn
// an AS actor, drive its members, read them back through FPropertyBindingPath
// helpers in Shared/AngelscriptReflectiveAccess.h.
//
// FProperty mapping (authoritative source: Bind_Primitives.cpp):
//   int8   -> FInt8Property    (FInt8Type)
//   int16  -> FInt16Property   (FInt16Type)
//   int    -> FIntProperty     (FIntType,    NativeType int32)
//   int64  -> FInt64Property   (FInt64Type)
//   uint8  -> FByteProperty    (FUInt8Type)
//   uint16 -> FUInt16Property  (FUInt16Type)
//   uint   -> FUInt32Property  (FUIntType,   NativeType uint32)
//   uint64 -> FUInt64Property  (FUInt64Type)
//
// Detailed coverage matrix: Documents/Coverage/Coverage_IntProperty.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageIntPropertyTest,
	"Angelscript.TestModule.Coverage.IntProperty",
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
	// Declaration-time defaults round-trip back through the property layer.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntFamilyDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntDefaultsActor : AActor
			{
				UPROPERTY()
				int8 Int8Value = 100;

				UPROPERTY()
				int16 Int16Value = 30000;

				UPROPERTY()
				int IntValue = 123456;

				UPROPERTY()
				int64 Int64Value = 10000000000;

				UPROPERTY()
				uint8 UInt8Value = 250;

				UPROPERTY()
				uint16 UInt16Value = 60000;

				UPROPERTY()
				uint UInt32Value = 123456;

				UPROPERTY()
				uint64 UInt64Value = 123456;
			}
			)AS"),
			TEXT("ACoverageIntDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-defaults actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Int8Value"),   static_cast<int8>(100),            TEXT("int8 UPROPERTY default should read back"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Int16Value"),  static_cast<int16>(30000),         TEXT("int16 UPROPERTY default should read back"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("IntValue"),    123456,                            TEXT("int UPROPERTY default should read back"));
		VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Int64Value"),  static_cast<int64>(10000000000LL), TEXT("int64 UPROPERTY default should read back"));
		VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  static_cast<uint8>(250),           TEXT("uint8 UPROPERTY default should read back"));
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(60000),        TEXT("uint16 UPROPERTY default should read back"));
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), static_cast<uint32>(123456),       TEXT("uint UPROPERTY default should read back"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), static_cast<uint64>(123456),       TEXT("uint64 UPROPERTY default should read back"));
	}

	// -------------------------------------------------------------------------
	// C++ -> property -> C++ write round-trip across the whole int family.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntFamilyWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_RoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntRoundTripActor : AActor
			{
				UPROPERTY()
				int8 Int8Value = 0;

				UPROPERTY()
				int16 Int16Value = 0;

				UPROPERTY()
				int IntValue = 0;

				UPROPERTY()
				int64 Int64Value = 0;

				UPROPERTY()
				uint8 UInt8Value = 0;

				UPROPERTY()
				uint16 UInt16Value = 0;

				UPROPERTY()
				uint UInt32Value = 0;

				UPROPERTY()
				uint64 UInt64Value = 0;
			}
			)AS"),
			TEXT("ACoverageIntRoundTripActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int-round-trip actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-round-trip actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Int8Value"),   static_cast<int8>(-42)),         TEXT("int8 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Int16Value"),  static_cast<int16>(-12345)),     TEXT("int16 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("IntValue"),    -987654),                        TEXT("int SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Int64Value"),  static_cast<int64>(-9000000000LL)), TEXT("int64 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  static_cast<uint8>(200)),        TEXT("uint8 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(54321)),     TEXT("uint16 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), static_cast<uint32>(3000000000u)), TEXT("uint SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), static_cast<uint64>(12000000000000000000ull)), TEXT("uint64 SetByPath")));

		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Int8Value"),   static_cast<int8>(-42),            TEXT("int8 write should round-trip"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Int16Value"),  static_cast<int16>(-12345),        TEXT("int16 write should round-trip"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("IntValue"),    -987654,                           TEXT("int write should round-trip"));
		VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Int64Value"),  static_cast<int64>(-9000000000LL), TEXT("int64 write should round-trip"));
		VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  static_cast<uint8>(200),           TEXT("uint8 write should round-trip"));
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(54321),        TEXT("uint16 write should round-trip"));
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), static_cast<uint32>(3000000000u),  TEXT("uint write should round-trip"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), static_cast<uint64>(12000000000000000000ull), TEXT("uint64 write should round-trip"));
	}

	// -------------------------------------------------------------------------
	// Min / max boundary values per width, written from C++ (avoids AS literal
	// edge cases) and read back through the property layer.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntFamilyBoundaryValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_Boundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyBoundary.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntBoundaryActor : AActor
			{
				UPROPERTY()
				int8 Int8Value = 0;

				UPROPERTY()
				int16 Int16Value = 0;

				UPROPERTY()
				int IntValue = 0;

				UPROPERTY()
				int64 Int64Value = 0;

				UPROPERTY()
				uint8 UInt8Value = 0;

				UPROPERTY()
				uint16 UInt16Value = 0;

				UPROPERTY()
				uint UInt32Value = 0;

				UPROPERTY()
				uint64 UInt64Value = 0;
			}
			)AS"),
			TEXT("ACoverageIntBoundaryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int-boundary actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-boundary actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// --- Minimums ---
		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,  int8 >(*TestRunner, Actor, TEXT("Int8Value"),  TNumericLimits<int8>::Min()),  TEXT("int8 min SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), TNumericLimits<int16>::Min()), TEXT("int16 min SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,   int32>(*TestRunner, Actor, TEXT("IntValue"),   TNumericLimits<int32>::Min()), TEXT("int min SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), TNumericLimits<int64>::Min()), TEXT("int64 min SetByPath")));

		VerifyByPath<FInt8Property,  int8 >(*TestRunner, Actor, TEXT("Int8Value"),  TNumericLimits<int8>::Min(),  TEXT("int8 should hold its minimum"));
		VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), TNumericLimits<int16>::Min(), TEXT("int16 should hold its minimum"));
		VerifyByPath<FIntProperty,   int32>(*TestRunner, Actor, TEXT("IntValue"),   TNumericLimits<int32>::Min(), TEXT("int should hold its minimum"));
		VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), TNumericLimits<int64>::Min(), TEXT("int64 should hold its minimum"));

		// --- Maximums (signed) ---
		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,  int8 >(*TestRunner, Actor, TEXT("Int8Value"),  TNumericLimits<int8>::Max()),  TEXT("int8 max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), TNumericLimits<int16>::Max()), TEXT("int16 max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,   int32>(*TestRunner, Actor, TEXT("IntValue"),   TNumericLimits<int32>::Max()), TEXT("int max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), TNumericLimits<int64>::Max()), TEXT("int64 max SetByPath")));

		VerifyByPath<FInt8Property,  int8 >(*TestRunner, Actor, TEXT("Int8Value"),  TNumericLimits<int8>::Max(),  TEXT("int8 should hold its maximum"));
		VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), TNumericLimits<int16>::Max(), TEXT("int16 should hold its maximum"));
		VerifyByPath<FIntProperty,   int32>(*TestRunner, Actor, TEXT("IntValue"),   TNumericLimits<int32>::Max(), TEXT("int should hold its maximum"));
		VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), TNumericLimits<int64>::Max(), TEXT("int64 should hold its maximum"));

		// --- Maximums (unsigned) ---
		ASSERT_THAT(IsTrue(SetByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  TNumericLimits<uint8>::Max()),  TEXT("uint8 max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), TNumericLimits<uint16>::Max()), TEXT("uint16 max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), TNumericLimits<uint32>::Max()), TEXT("uint max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), TNumericLimits<uint64>::Max()), TEXT("uint64 max SetByPath")));

		VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  TNumericLimits<uint8>::Max(),  TEXT("uint8 should hold its maximum"));
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), TNumericLimits<uint16>::Max(), TEXT("uint16 should hold its maximum"));
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), TNumericLimits<uint32>::Max(), TEXT("uint should hold its maximum"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), TNumericLimits<uint64>::Max(), TEXT("uint64 should hold its maximum"));
	}

	// -------------------------------------------------------------------------
	// int as container element: TArray<int>/<int64>/<uint8> + TMap<int, *>.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntContainerActor : AActor
			{
				UPROPERTY()
				TArray<int> IntArray;

				UPROPERTY()
				TArray<int64> Int64Array;

				UPROPERTY()
				TArray<uint8> ByteArray;

				UPROPERTY()
				TMap<int, int> IntToIntMap;

				UPROPERTY()
				TMap<int, FString> IntToStringMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					IntArray.Add(10);
					IntArray.Add(20);
					IntArray.Add(30);

					Int64Array.Add(1000000000000);
					Int64Array.Add(2000000000000);

					ByteArray.Add(1);
					ByteArray.Add(255);

					IntToIntMap.Add(1, 100);
					IntToIntMap.Add(2, 200);

					IntToStringMap.Add(7, "Seven");
					IntToStringMap.Add(9, "Nine");
				}
			}
			)AS"),
			TEXT("ACoverageIntContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// --- TArray<int> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("IntArray"), Count), TEXT("TArray<int> length should resolve")));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TArray<int> should report three elements")));
		}
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[0]"), 10, TEXT("TArray<int>[0]"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[2]"), 30, TEXT("TArray<int>[2]"));

		// --- TArray<int64> ---
		VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Array[1]"), static_cast<int64>(2000000000000LL), TEXT("TArray<int64>[1]"));

		// --- TArray<uint8> ---
		VerifyByPath<FByteProperty, uint8>(*TestRunner, Actor, TEXT("ByteArray[1]"), static_cast<uint8>(255), TEXT("TArray<uint8>[1]"));

		// --- TMap<int,int> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToIntMap"), Count), TEXT("TMap<int,int> length should resolve")));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,int> should report two entries")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FIntProperty, int32>(*TestRunner, Actor, TEXT("IntToIntMap"), 2, Value),
				TEXT("TMap<int,int> value lookup should resolve")));
			ASSERT_THAT(AreEqual(200, Value, TEXT("TMap<int,int>[2] should be 200")));
		}

		// --- TMap<int,FString> (int key) ---
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FStrProperty, FString>(*TestRunner, Actor, TEXT("IntToStringMap"), 9, Value),
				TEXT("TMap<int,FString> value lookup should resolve")));
			ASSERT_THAT(AreEqual(FString(TEXT("Nine")), Value, TEXT("TMap<int,FString>[9] should be \"Nine\"")));
		}
	}

	// -------------------------------------------------------------------------
	// Extended container coverage: remaining TArray widths, TMap<FString,int>,
	// TSet<int> (sub-matrix 5 completion).
	// -------------------------------------------------------------------------
	TEST_METHOD(IntContainerPropertiesExtended)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_ContainerExt"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyContainerExt.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntContainerExtActor : AActor
			{
				UPROPERTY()
				TArray<int8> Int8Array;

				UPROPERTY()
				TArray<int16> Int16Array;

				UPROPERTY()
				TArray<uint16> UInt16Array;

				UPROPERTY()
				TArray<uint> UIntArray;

				UPROPERTY()
				TArray<uint64> UInt64Array;

				UPROPERTY()
				TMap<FString, int> StringToIntMap;

				UPROPERTY()
				TMap<int8, FString> Int8ToStringMap;

				UPROPERTY()
				TMap<int64, FString> Int64ToStringMap;

				UPROPERTY()
				TMap<uint, FString> UIntToStringMap;

				UPROPERTY()
				TSet<int> IntSet;

				UPROPERTY()
				TSet<int8> Int8Set;

				UPROPERTY()
				TSet<int64> Int64Set;

				UPROPERTY()
				TSet<uint> UIntSet;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Int8Array.Add(-42);
					Int8Array.Add(127);

					Int16Array.Add(-12345);
					Int16Array.Add(30000);

					UInt16Array.Add(60000);
					UInt16Array.Add(65535);

					UIntArray.Add(3000000000);
					UIntArray.Add(4000000000);

					UInt64Array.Add(10000000000000000000);

					StringToIntMap.Add("Alpha", 100);
					StringToIntMap.Add("Beta", 200);

					Int8ToStringMap.Add(-10, "NegTen");
					Int8ToStringMap.Add(127, "MaxInt8");

					Int64ToStringMap.Add(9000000000, "Billion");
					Int64ToStringMap.Add(-9000000000, "NegBillion");

					UIntToStringMap.Add(3000000000, "Three");
					UIntToStringMap.Add(4000000000, "Four");

					IntSet.Add(5);
					IntSet.Add(10);
					IntSet.Add(15);

					Int8Set.Add(-42);
					Int8Set.Add(0);
					Int8Set.Add(127);

					Int64Set.Add(1000000000000);
					Int64Set.Add(2000000000000);

					UIntSet.Add(3000000000);
					UIntSet.Add(4000000000);
				}
			}
			)AS"),
			TEXT("ACoverageIntContainerExtActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int-container-ext actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-container-ext actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// --- TArray<int8> ---
		VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Array[0]"), static_cast<int8>(-42), TEXT("TArray<int8>[0]"));
		VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Array[1]"), static_cast<int8>(127), TEXT("TArray<int8>[1]"));

		// --- TArray<int16> ---
		VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Array[0]"), static_cast<int16>(-12345), TEXT("TArray<int16>[0]"));
		VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Array[1]"), static_cast<int16>(30000), TEXT("TArray<int16>[1]"));

		// --- TArray<uint16> ---
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Array[0]"), static_cast<uint16>(60000), TEXT("TArray<uint16>[0]"));
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Array[1]"), static_cast<uint16>(65535), TEXT("TArray<uint16>[1]"));

		// --- TArray<uint> ---
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UIntArray[0]"), static_cast<uint32>(3000000000u), TEXT("TArray<uint>[0]"));
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UIntArray[1]"), static_cast<uint32>(4000000000u), TEXT("TArray<uint>[1]"));

		// --- TArray<uint64> ---
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Array[0]"), static_cast<uint64>(10000000000000000000ull), TEXT("TArray<uint64>[0]"));

		// --- TMap<FString,int> (int as value) ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToIntMap"), Count), TEXT("TMap<FString,int> length should resolve")));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,int> should report two entries")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FIntProperty, int32>(*TestRunner, Actor, TEXT("StringToIntMap"), FString(TEXT("Beta")), Value),
				TEXT("TMap<FString,int> value lookup should resolve")));
			ASSERT_THAT(AreEqual(200, Value, TEXT("TMap<FString,int>[\"Beta\"] should be 200")));
		}

		// --- TSet<int> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("IntSet"), Count), TEXT("TSet<int> length should resolve")));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TSet<int> should report three elements")));

			bool bContains = SetContainsByPath<int32>(*TestRunner, Actor, TEXT("IntSet"), 10);
			ASSERT_THAT(IsTrue(bContains, TEXT("TSet<int> should contain 10")));

			bContains = SetContainsByPath<int32>(*TestRunner, Actor, TEXT("IntSet"), 99);
			ASSERT_THAT(IsFalse(bContains, TEXT("TSet<int> should not contain 99")));
		}

		// --- TMap<int8,FString> (int8 as key) ---
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int8, FStrProperty, FString>(*TestRunner, Actor, TEXT("Int8ToStringMap"), static_cast<int8>(127), Value),
				TEXT("TMap<int8,FString> value lookup should resolve")));
			ASSERT_THAT(AreEqual(FString(TEXT("MaxInt8")), Value, TEXT("TMap<int8,FString>[127] should be \"MaxInt8\"")));
		}

		// --- TMap<int64,FString> (int64 as key) ---
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int64, FStrProperty, FString>(*TestRunner, Actor, TEXT("Int64ToStringMap"), static_cast<int64>(9000000000LL), Value),
				TEXT("TMap<int64,FString> value lookup should resolve")));
			ASSERT_THAT(AreEqual(FString(TEXT("Billion")), Value, TEXT("TMap<int64,FString>[9000000000] should be \"Billion\"")));
		}

		// --- TMap<uint,FString> (uint as key) ---
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<uint32, FStrProperty, FString>(*TestRunner, Actor, TEXT("UIntToStringMap"), static_cast<uint32>(3000000000u), Value),
				TEXT("TMap<uint,FString> value lookup should resolve")));
			ASSERT_THAT(AreEqual(FString(TEXT("Three")), Value, TEXT("TMap<uint,FString>[3000000000] should be \"Three\"")));
		}

		// --- TSet<int8> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("Int8Set"), Count), TEXT("TSet<int8> length should resolve")));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TSet<int8> should report three elements")));

			bool bContains = SetContainsByPath<int8>(*TestRunner, Actor, TEXT("Int8Set"), static_cast<int8>(-42));
			ASSERT_THAT(IsTrue(bContains, TEXT("TSet<int8> should contain -42")));
		}

		// --- TSet<int64> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("Int64Set"), Count), TEXT("TSet<int64> length should resolve")));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<int64> should report two elements")));

			bool bContains = SetContainsByPath<int64>(*TestRunner, Actor, TEXT("Int64Set"), static_cast<int64>(1000000000000LL));
			ASSERT_THAT(IsTrue(bContains, TEXT("TSet<int64> should contain 1000000000000")));
		}

		// --- TSet<uint> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("UIntSet"), Count), TEXT("TSet<uint> length should resolve")));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<uint> should report two elements")));

			bool bContains = SetContainsByPath<uint32>(*TestRunner, Actor, TEXT("UIntSet"), static_cast<uint32>(3000000000u));
			ASSERT_THAT(IsTrue(bContains, TEXT("TSet<uint> should contain 3000000000")));
		}
	}

	// -------------------------------------------------------------------------
	// Container edge cases: empty, single element, modification operations.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntContainerEdgeCases)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_ContainerEdges"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyContainerEdges.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntContainerEdgeActor : AActor
			{
				UPROPERTY()
				TArray<int> EmptyArray;

				UPROPERTY()
				TArray<int> SingleElementArray;

				UPROPERTY()
				TArray<int> ModifiedArray;

				UPROPERTY()
				TMap<int, int> EmptyMap;

				UPROPERTY()
				TMap<int, int> SingleEntryMap;

				UPROPERTY()
				TMap<int, int> OverwriteMap;

				UPROPERTY()
				TSet<int> EmptySet;

				UPROPERTY()
				TSet<int> SingleElementSet;

				UPROPERTY()
				TSet<int> DuplicateSet;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Empty containers - no action

					// Single element
					SingleElementArray.Add(42);
					SingleEntryMap.Add(1, 100);
					SingleElementSet.Add(99);

					// Modified array - add then remove
					ModifiedArray.Add(1);
					ModifiedArray.Add(2);
					ModifiedArray.Add(3);
					ModifiedArray.RemoveAt(1);  // Remove middle element

					// Map overwrite
					OverwriteMap.Add(10, 100);
					OverwriteMap.Add(10, 200);  // Overwrite existing key

					// Set with duplicates
					DuplicateSet.Add(5);
					DuplicateSet.Add(10);
					DuplicateSet.Add(5);  // Duplicate - should be ignored
				}
			}
			)AS"),
			TEXT("ACoverageIntContainerEdgeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int-container-edge actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-container-edge actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// --- Empty Array ---
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("EmptyArray"), Length)));
			ASSERT_THAT(AreEqual(0, Length, TEXT("Empty TArray should have length 0")));
		}

		// --- Single Element Array ---
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("SingleElementArray"), Length)));
			ASSERT_THAT(AreEqual(1, Length, TEXT("Single element TArray should have length 1")));

			VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SingleElementArray[0]"), 42, TEXT("Single element TArray[0]"));
		}

		// --- Modified Array (after RemoveAt) ---
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ModifiedArray"), Length)));
			ASSERT_THAT(AreEqual(2, Length, TEXT("Modified TArray should have 2 elements after RemoveAt")));

			VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[0]"), 1, TEXT("Modified TArray[0]"));
			VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[1]"), 3, TEXT("Modified TArray[1] after remove"));
		}

		// --- Empty Map ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("EmptyMap"), Count)));
			ASSERT_THAT(AreEqual(0, Count, TEXT("Empty TMap should have 0 entries")));
		}

		// --- Single Entry Map ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("SingleEntryMap"), Count)));
			ASSERT_THAT(AreEqual(1, Count, TEXT("Single entry TMap should have 1 entry")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FIntProperty, int32>(*TestRunner, Actor, TEXT("SingleEntryMap"), 1, Value)));
			ASSERT_THAT(AreEqual(100, Value, TEXT("Single entry TMap[1] should be 100")));
		}

		// --- Overwrite Map (key collision) ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("OverwriteMap"), Count)));
			ASSERT_THAT(AreEqual(1, Count, TEXT("Overwrite TMap should have 1 entry (not 2)")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FIntProperty, int32>(*TestRunner, Actor, TEXT("OverwriteMap"), 10, Value)));
			ASSERT_THAT(AreEqual(200, Value, TEXT("Overwrite TMap[10] should be 200 (overwritten)")));
		}

		// --- Empty Set ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("EmptySet"), Count)));
			ASSERT_THAT(AreEqual(0, Count, TEXT("Empty TSet should have 0 elements")));
		}

		// --- Single Element Set ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("SingleElementSet"), Count)));
			ASSERT_THAT(AreEqual(1, Count, TEXT("Single element TSet should have 1 element")));

			bool bContains = SetContainsByPath<int32>(*TestRunner, Actor, TEXT("SingleElementSet"), 99);
			ASSERT_THAT(IsTrue(bContains, TEXT("Single element TSet should contain 99")));
		}

		// --- Duplicate Set (deduplication) ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("DuplicateSet"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet with duplicate Add should have 2 elements (deduplicated)")));

			bool bContains5 = SetContainsByPath<int32>(*TestRunner, Actor, TEXT("DuplicateSet"), 5);
			bool bContains10 = SetContainsByPath<int32>(*TestRunner, Actor, TEXT("DuplicateSet"), 10);
			ASSERT_THAT(IsTrue(bContains5, TEXT("TSet should contain 5")));
			ASSERT_THAT(IsTrue(bContains10, TEXT("TSet should contain 10")));
		}
	}

	// -------------------------------------------------------------------------
	// UPROPERTY specifier coverage for int members: the full permutation set of
	// Edit / Visible / Blueprint access specifiers, standalone flag specifiers,
	// meta keys, and representative combinations -> CPF_* flags + meta.
	//
	// Baseline behaviour of this fork (Preprocessor ProcessPropertyMacro +
	// ClassGenerator): every UPROPERTY defaults to EditAnywhere + BlueprintRead-
	// Write, so a property only loses CPF_Edit / CPF_BlueprintVisible when an
	// explicit specifier overrides the default. `Replicated` / `ReplicatedUsing`
	// are intentionally excluded here -- under WITH_ANGELSCRIPT_HAZE they are not
	// valid property specifiers and belong in the Networking PIE suite.
	// -------------------------------------------------------------------------
	TEST_METHOD(IntPropertySpecifierFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertySpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntSpecifierActor : AActor
			{
				UPROPERTY(EditAnywhere)
				int EditAnywhereInt = 1;

				UPROPERTY(EditDefaultsOnly)
				int EditDefaultsOnlyInt = 2;

				UPROPERTY(EditInstanceOnly)
				int EditInstanceOnlyInt = 3;

				UPROPERTY(NotEditable)
				int NotEditableInt = 4;

				UPROPERTY(EditConst)
				int EditConstInt = 5;

				UPROPERTY(VisibleAnywhere)
				int VisibleAnywhereInt = 6;

				UPROPERTY(VisibleDefaultsOnly)
				int VisibleDefaultsOnlyInt = 7;

				UPROPERTY(VisibleInstanceOnly)
				int VisibleInstanceOnlyInt = 8;

				UPROPERTY(BlueprintReadWrite)
				int BlueprintReadWriteInt = 9;

				UPROPERTY(BlueprintReadOnly)
				int BlueprintReadOnlyInt = 10;

				UPROPERTY(BlueprintHidden)
				int BlueprintHiddenInt = 11;

				UPROPERTY(Transient)
				int TransientInt = 12;

				UPROPERTY(Config)
				int ConfigInt = 13;

				UPROPERTY(SaveGame)
				int SaveGameInt = 14;

				UPROPERTY(AdvancedDisplay)
				int AdvancedDisplayInt = 15;

				UPROPERTY(Interp)
				int InterpInt = 16;

				UPROPERTY(ExposeOnSpawn)
				int ExposeOnSpawnInt = 17;

				UPROPERTY(meta = (ClampMin = "0", ClampMax = "10"))
				int ClampedInt = 5;

				UPROPERTY(meta = (UIMin = "1", UIMax = "5"))
				int UIRangedInt = 3;

				UPROPERTY()
				bool Gate = true;

				UPROPERTY(meta = (EditCondition = "Gate"))
				int EditConditionInt = 18;

				UPROPERTY(Category = "Stats")
				int CategorizedInt = 19;

				UPROPERTY(EditAnywhere, BlueprintReadOnly)
				int EditableReadOnlyInt = 20;

				UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Transient, Category = "Tuning")
				int ComboInt = 21;
			}
			)AS"),
			TEXT("ACoverageIntSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int-specifier actor class should compile")));

		// Resolve a property and report (non-aborting) if it is missing, so every
		// row in the matrix gets evaluated even if one specifier regresses.
		auto FindProp = [&](const TCHAR* Name) -> const FProperty*
		{
			const FProperty* Found = ScriptClass->FindPropertyByName(FName(Name));
			if (Found == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("Specifier property '%s' should exist"), Name));
			}
			return Found;
		};

		auto CheckFlag = [&](const TCHAR* Name, EPropertyFlags Flag, bool bExpected, const TCHAR* Label)
		{
			const FProperty* Found = FindProp(Name);
			if (Found == nullptr)
			{
				return;
			}
			if (bExpected)
			{
				TestRunner->TestTrue(Label, Found->HasAnyPropertyFlags(Flag));
			}
			else
			{
				TestRunner->TestFalse(Label, Found->HasAnyPropertyFlags(Flag));
			}
		};

		// --- Edit specifiers ---
		CheckFlag(TEXT("EditAnywhereInt"),       CPF_Edit,                  true,  TEXT("EditAnywhere -> CPF_Edit"));
		CheckFlag(TEXT("EditAnywhereInt"),       CPF_DisableEditOnInstance, false, TEXT("EditAnywhere -> editable on instance"));
		CheckFlag(TEXT("EditAnywhereInt"),       CPF_DisableEditOnTemplate, false, TEXT("EditAnywhere -> editable on defaults"));

		CheckFlag(TEXT("EditDefaultsOnlyInt"),   CPF_Edit,                  true,  TEXT("EditDefaultsOnly -> CPF_Edit"));
		CheckFlag(TEXT("EditDefaultsOnlyInt"),   CPF_DisableEditOnInstance, true,  TEXT("EditDefaultsOnly -> disabled on instance"));
		CheckFlag(TEXT("EditDefaultsOnlyInt"),   CPF_DisableEditOnTemplate, false, TEXT("EditDefaultsOnly -> editable on defaults"));

		CheckFlag(TEXT("EditInstanceOnlyInt"),   CPF_Edit,                  true,  TEXT("EditInstanceOnly -> CPF_Edit"));
		CheckFlag(TEXT("EditInstanceOnlyInt"),   CPF_DisableEditOnTemplate, true,  TEXT("EditInstanceOnly -> disabled on defaults"));
		CheckFlag(TEXT("EditInstanceOnlyInt"),   CPF_DisableEditOnInstance, false, TEXT("EditInstanceOnly -> editable on instance"));

		CheckFlag(TEXT("NotEditableInt"),        CPF_Edit,                  false, TEXT("NotEditable -> clears CPF_Edit"));

		CheckFlag(TEXT("EditConstInt"),          CPF_Edit,                  true,  TEXT("EditConst keeps default CPF_Edit"));
		CheckFlag(TEXT("EditConstInt"),          CPF_EditConst,             true,  TEXT("EditConst -> CPF_EditConst"));

		// --- Visible specifiers (edit-visible but read-only) ---
		CheckFlag(TEXT("VisibleAnywhereInt"),    CPF_Edit,                  true,  TEXT("VisibleAnywhere -> CPF_Edit"));
		CheckFlag(TEXT("VisibleAnywhereInt"),    CPF_EditConst,             true,  TEXT("VisibleAnywhere -> CPF_EditConst"));
		CheckFlag(TEXT("VisibleAnywhereInt"),    CPF_DisableEditOnInstance, false, TEXT("VisibleAnywhere -> visible on instance"));
		CheckFlag(TEXT("VisibleAnywhereInt"),    CPF_DisableEditOnTemplate, false, TEXT("VisibleAnywhere -> visible on defaults"));

		CheckFlag(TEXT("VisibleDefaultsOnlyInt"), CPF_EditConst,            true,  TEXT("VisibleDefaultsOnly -> CPF_EditConst"));
		CheckFlag(TEXT("VisibleDefaultsOnlyInt"), CPF_DisableEditOnInstance, true, TEXT("VisibleDefaultsOnly -> disabled on instance"));

		CheckFlag(TEXT("VisibleInstanceOnlyInt"), CPF_EditConst,            true,  TEXT("VisibleInstanceOnly -> CPF_EditConst"));
		CheckFlag(TEXT("VisibleInstanceOnlyInt"), CPF_DisableEditOnTemplate, true, TEXT("VisibleInstanceOnly -> disabled on defaults"));

		// --- Blueprint access specifiers ---
		CheckFlag(TEXT("BlueprintReadWriteInt"), CPF_BlueprintVisible,      true,  TEXT("BlueprintReadWrite -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("BlueprintReadWriteInt"), CPF_BlueprintReadOnly,     false, TEXT("BlueprintReadWrite -> not read-only"));

		CheckFlag(TEXT("BlueprintReadOnlyInt"),  CPF_BlueprintVisible,      true,  TEXT("BlueprintReadOnly -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("BlueprintReadOnlyInt"),  CPF_BlueprintReadOnly,     true,  TEXT("BlueprintReadOnly -> CPF_BlueprintReadOnly"));

		CheckFlag(TEXT("BlueprintHiddenInt"),    CPF_BlueprintVisible,      false, TEXT("BlueprintHidden -> clears CPF_BlueprintVisible"));

		// --- Standalone flag specifiers ---
		CheckFlag(TEXT("TransientInt"),          CPF_Transient,             true,  TEXT("Transient -> CPF_Transient"));
		CheckFlag(TEXT("ConfigInt"),             CPF_Config,                true,  TEXT("Config -> CPF_Config"));
		CheckFlag(TEXT("SaveGameInt"),           CPF_SaveGame,              true,  TEXT("SaveGame -> CPF_SaveGame"));
		CheckFlag(TEXT("AdvancedDisplayInt"),    CPF_AdvancedDisplay,       true,  TEXT("AdvancedDisplay -> CPF_AdvancedDisplay"));
		CheckFlag(TEXT("InterpInt"),             CPF_Interp,                true,  TEXT("Interp -> CPF_Interp"));
		CheckFlag(TEXT("ExposeOnSpawnInt"),      CPF_ExposeOnSpawn,         true,  TEXT("ExposeOnSpawn -> CPF_ExposeOnSpawn"));

		// --- Representative combinations ---
		CheckFlag(TEXT("EditableReadOnlyInt"),   CPF_Edit,                  true,  TEXT("EditAnywhere+BlueprintReadOnly -> CPF_Edit"));
		CheckFlag(TEXT("EditableReadOnlyInt"),   CPF_BlueprintVisible,      true,  TEXT("EditAnywhere+BlueprintReadOnly -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("EditableReadOnlyInt"),   CPF_BlueprintReadOnly,     true,  TEXT("EditAnywhere+BlueprintReadOnly -> CPF_BlueprintReadOnly"));

		CheckFlag(TEXT("ComboInt"),              CPF_Edit,                  true,  TEXT("Combo -> CPF_Edit"));
		CheckFlag(TEXT("ComboInt"),              CPF_DisableEditOnInstance, true,  TEXT("Combo (EditDefaultsOnly) -> disabled on instance"));
		CheckFlag(TEXT("ComboInt"),              CPF_BlueprintReadOnly,     true,  TEXT("Combo -> CPF_BlueprintReadOnly"));
		CheckFlag(TEXT("ComboInt"),              CPF_Transient,             true,  TEXT("Combo -> CPF_Transient"));

#if WITH_EDITOR
		// --- Meta keys round-trip (editor-only metadata store) ---
		if (const FProperty* Clamped = FindProp(TEXT("ClampedInt")))
		{
			TestRunner->TestEqual(TEXT("ClampMin meta should round-trip"),  Clamped->GetMetaData(TEXT("ClampMin")), FString(TEXT("0")));
			TestRunner->TestEqual(TEXT("ClampMax meta should round-trip"),  Clamped->GetMetaData(TEXT("ClampMax")), FString(TEXT("10")));
		}
		if (const FProperty* UIRanged = FindProp(TEXT("UIRangedInt")))
		{
			TestRunner->TestEqual(TEXT("UIMin meta should round-trip"), UIRanged->GetMetaData(TEXT("UIMin")), FString(TEXT("1")));
			TestRunner->TestEqual(TEXT("UIMax meta should round-trip"), UIRanged->GetMetaData(TEXT("UIMax")), FString(TEXT("5")));
		}
		if (const FProperty* EditCond = FindProp(TEXT("EditConditionInt")))
		{
			TestRunner->TestEqual(TEXT("EditCondition meta should round-trip"), EditCond->GetMetaData(TEXT("EditCondition")), FString(TEXT("Gate")));
		}
		if (const FProperty* Categorized = FindProp(TEXT("CategorizedInt")))
		{
			TestRunner->TestEqual(TEXT("Category meta should round-trip"), Categorized->GetMetaData(TEXT("Category")), FString(TEXT("Stats")));
		}
		if (const FProperty* Combo = FindProp(TEXT("ComboInt")))
		{
			TestRunner->TestEqual(TEXT("Combo Category meta should round-trip"), Combo->GetMetaData(TEXT("Category")), FString(TEXT("Tuning")));
		}
#endif
	}
};

#endif
