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
// AngelscriptCoverageUStructMemberTests
// -----------------------------------------------------------------------------
// Coverage for USTRUCT nested member access in AngelScript, covering all
// integer type variants (int8/16/32/64, uint8/16/32/64) as UPROPERTY members
// inside nested USTRUCT definitions.
//
// This test validates:
//   * USTRUCT definition in AngelScript with integer members
//   * Nested USTRUCT (Outer contains Inner, Inner contains int members)
//   * Actor class containing USTRUCT as UPROPERTY
//   * Nested path access: Data.Inner.Value
//   * Read/write operations through nested paths
//   * All 8 integer type variants
//
// Pattern D (UPROPERTY path read/write): spawn an AS actor, drive its nested
// struct members, read them back through FPropertyBindingPath helpers.
//
// Relates to OpenSpec: test-coverage/coverage-matrix.md §3 row:
//   "USTRUCT 内 int 成员（嵌套路径）"
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
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUStructMemberTest,
	"Angelscript.TestModule.Coverage.UStructMember",
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
	// Nested USTRUCT with all 8 integer type variants.
	// Tests nested path access: OuterStruct.InnerStruct.IntMember
	// -------------------------------------------------------------------------
	TEST_METHOD(NestedStructIntegerMembers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStructMember_Nested"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMemberNested.as"),
			ASTEST_AS(R"AS(
			// Inner struct containing all 8 integer type variants
			USTRUCT()
			struct FInnerIntData
			{
				UPROPERTY()
				int8 Int8Value = -42;

				UPROPERTY()
				int16 Int16Value = -12345;

				UPROPERTY()
				int Int32Value = -987654;

				UPROPERTY()
				int64 Int64Value = -9000000000;

				UPROPERTY()
				uint8 UInt8Value = 200;

				UPROPERTY()
				uint16 UInt16Value = 54321;

				UPROPERTY()
				uint UInt32Value = 3000000000;

				UPROPERTY()
				uint64 UInt64Value = 12000000000;
			}

			// Outer struct containing the inner struct
			USTRUCT()
			struct FOuterIntData
			{
				UPROPERTY()
				FInnerIntData Inner;

				UPROPERTY()
				int OuterValue = 999;
			}

			UCLASS()
			class ACoverageNestedStructActor : AActor
			{
				UPROPERTY()
				FOuterIntData Data;
			}
			)AS"),
			TEXT("ACoverageNestedStructActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Nested struct actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Nested struct actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify default values through nested paths
		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Data.Inner.Int8Value"),   static_cast<int8>(-42),            TEXT("Nested int8 default should read back"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Data.Inner.Int16Value"),  static_cast<int16>(-12345),        TEXT("Nested int16 default should read back"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Data.Inner.Int32Value"),  -987654,                           TEXT("Nested int32 default should read back"));
		VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Data.Inner.Int64Value"),  static_cast<int64>(-9000000000LL), TEXT("Nested int64 default should read back"));
		VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("Data.Inner.UInt8Value"),  static_cast<uint8>(200),           TEXT("Nested uint8 default should read back"));
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("Data.Inner.UInt16Value"), static_cast<uint16>(54321),        TEXT("Nested uint16 default should read back"));
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("Data.Inner.UInt32Value"), static_cast<uint32>(3000000000u),  TEXT("Nested uint32 default should read back"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Data.Inner.UInt64Value"), static_cast<uint64>(12000000000ull), TEXT("Nested uint64 default should read back"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Data.OuterValue"),        999,                               TEXT("Outer struct value should read back"));

		// Write new values through nested paths
		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Data.Inner.Int8Value"),   static_cast<int8>(100))));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Data.Inner.Int16Value"),  static_cast<int16>(30000))));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Data.Inner.Int32Value"),  123456)));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Data.Inner.Int64Value"),  static_cast<int64>(10000000000LL))));
		ASSERT_THAT(IsTrue(SetByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("Data.Inner.UInt8Value"),  static_cast<uint8>(250))));
		ASSERT_THAT(IsTrue(SetByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("Data.Inner.UInt16Value"), static_cast<uint16>(60000))));
		ASSERT_THAT(IsTrue(SetByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("Data.Inner.UInt32Value"), static_cast<uint32>(4000000000u))));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Data.Inner.UInt64Value"), static_cast<uint64>(18000000000000000000ull))));

		// Verify round-trip
		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Data.Inner.Int8Value"),   static_cast<int8>(100),            TEXT("Nested int8 write should round-trip"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Data.Inner.Int16Value"),  static_cast<int16>(30000),         TEXT("Nested int16 write should round-trip"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Data.Inner.Int32Value"),  123456,                            TEXT("Nested int32 write should round-trip"));
		VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Data.Inner.Int64Value"),  static_cast<int64>(10000000000LL), TEXT("Nested int64 write should round-trip"));
		VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("Data.Inner.UInt8Value"),  static_cast<uint8>(250),           TEXT("Nested uint8 write should round-trip"));
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("Data.Inner.UInt16Value"), static_cast<uint16>(60000),        TEXT("Nested uint16 write should round-trip"));
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("Data.Inner.UInt32Value"), static_cast<uint32>(4000000000u),  TEXT("Nested uint32 write should round-trip"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Data.Inner.UInt64Value"), static_cast<uint64>(18000000000000000000ull), TEXT("Nested uint64 write should round-trip"));
	}

	// -------------------------------------------------------------------------
	// Flat USTRUCT (single level) with all integer types.
	// Tests simple struct member access: StructData.IntMember
	// -------------------------------------------------------------------------
	TEST_METHOD(FlatStructIntegerMembers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStructMember_Flat"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMemberFlat.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FFlatIntData
			{
				UPROPERTY()
				int8 Int8Value = 10;

				UPROPERTY()
				int16 Int16Value = 1000;

				UPROPERTY()
				int Int32Value = 100000;

				UPROPERTY()
				int64 Int64Value = 10000000000;

				UPROPERTY()
				uint8 UInt8Value = 255;

				UPROPERTY()
				uint16 UInt16Value = 65000;

				UPROPERTY()
				uint UInt32Value = 4000000000;

				UPROPERTY()
				uint64 UInt64Value = 18000000000000000000;
			}

			UCLASS()
			class ACoverageFlatStructActor : AActor
			{
				UPROPERTY()
				FFlatIntData StructData;
			}
			)AS"),
			TEXT("ACoverageFlatStructActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Flat struct actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Flat struct actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify default values
		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("StructData.Int8Value"),   static_cast<int8>(10),              TEXT("Flat int8 default should read back"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("StructData.Int16Value"),  static_cast<int16>(1000),           TEXT("Flat int16 default should read back"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("StructData.Int32Value"),  100000,                             TEXT("Flat int32 default should read back"));
		VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("StructData.Int64Value"),  static_cast<int64>(10000000000LL),  TEXT("Flat int64 default should read back"));
		VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("StructData.UInt8Value"),  static_cast<uint8>(255),            TEXT("Flat uint8 default should read back"));
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("StructData.UInt16Value"), static_cast<uint16>(65000),         TEXT("Flat uint16 default should read back"));
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("StructData.UInt32Value"), static_cast<uint32>(4000000000u),   TEXT("Flat uint32 default should read back"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("StructData.UInt64Value"), static_cast<uint64>(18000000000000000000ull), TEXT("Flat uint64 default should read back"));
	}

	// -------------------------------------------------------------------------
	// Boundary values in USTRUCT members.
	// Tests min/max values for all integer types in nested structs.
	// -------------------------------------------------------------------------
	TEST_METHOD(StructIntegerBoundaryValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStructMember_Boundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMemberBoundary.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FBoundaryIntData
			{
				UPROPERTY()
				int8 Int8Min = 0;

				UPROPERTY()
				int8 Int8Max = 0;

				UPROPERTY()
				int16 Int16Min = 0;

				UPROPERTY()
				int16 Int16Max = 0;

				UPROPERTY()
				int Int32Min = 0;

				UPROPERTY()
				int Int32Max = 0;

				UPROPERTY()
				int64 Int64Min = 0;

				UPROPERTY()
				int64 Int64Max = 0;

				UPROPERTY()
				uint8 UInt8Max = 0;

				UPROPERTY()
				uint16 UInt16Max = 0;

				UPROPERTY()
				uint UInt32Max = 0;

				UPROPERTY()
				uint64 UInt64Max = 0;
			}

			UCLASS()
			class ACoverageBoundaryStructActor : AActor
			{
				UPROPERTY()
				FBoundaryIntData Data;
			}
			)AS"),
			TEXT("ACoverageBoundaryStructActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Boundary struct actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Boundary struct actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Write boundary values
		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Data.Int8Min"),    TNumericLimits<int8>::Min())));
		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Data.Int8Max"),    TNumericLimits<int8>::Max())));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Data.Int16Min"),   TNumericLimits<int16>::Min())));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Data.Int16Max"),   TNumericLimits<int16>::Max())));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Data.Int32Min"),   TNumericLimits<int32>::Min())));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Data.Int32Max"),   TNumericLimits<int32>::Max())));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Data.Int64Min"),   TNumericLimits<int64>::Min())));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Data.Int64Max"),   TNumericLimits<int64>::Max())));
		ASSERT_THAT(IsTrue(SetByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("Data.UInt8Max"),   TNumericLimits<uint8>::Max())));
		ASSERT_THAT(IsTrue(SetByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("Data.UInt16Max"),  TNumericLimits<uint16>::Max())));
		ASSERT_THAT(IsTrue(SetByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("Data.UInt32Max"),  TNumericLimits<uint32>::Max())));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Data.UInt64Max"),  TNumericLimits<uint64>::Max())));

		// Verify boundary values round-trip
		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Data.Int8Min"),   TNumericLimits<int8>::Min(),   TEXT("int8 min boundary should round-trip"));
		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Data.Int8Max"),   TNumericLimits<int8>::Max(),   TEXT("int8 max boundary should round-trip"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Data.Int16Min"),  TNumericLimits<int16>::Min(),  TEXT("int16 min boundary should round-trip"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Data.Int16Max"),  TNumericLimits<int16>::Max(),  TEXT("int16 max boundary should round-trip"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Data.Int32Min"),  TNumericLimits<int32>::Min(),  TEXT("int32 min boundary should round-trip"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Data.Int32Max"),  TNumericLimits<int32>::Max(),  TEXT("int32 max boundary should round-trip"));
		VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Data.Int64Min"),  TNumericLimits<int64>::Min(),  TEXT("int64 min boundary should round-trip"));
		VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Data.Int64Max"),  TNumericLimits<int64>::Max(),  TEXT("int64 max boundary should round-trip"));
		VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("Data.UInt8Max"),  TNumericLimits<uint8>::Max(),  TEXT("uint8 max boundary should round-trip"));
		VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("Data.UInt16Max"), TNumericLimits<uint16>::Max(), TEXT("uint16 max boundary should round-trip"));
		VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("Data.UInt32Max"), TNumericLimits<uint32>::Max(), TEXT("uint32 max boundary should round-trip"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Data.UInt64Max"), TNumericLimits<uint64>::Max(), TEXT("uint64 max boundary should round-trip"));
	}

	// -------------------------------------------------------------------------
	// Deep nesting (3 levels): Outer.Middle.Inner.Value
	// Tests path resolution through multiple struct layers.
	// -------------------------------------------------------------------------
	TEST_METHOD(DeepNestedStructIntegerMembers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStructMember_Deep"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMemberDeep.as"),
			ASTEST_AS(R"AS(
			// Innermost struct
			USTRUCT()
			struct FInnermost
			{
				UPROPERTY()
				int8 Int8Value = 7;

				UPROPERTY()
				uint64 UInt64Value = 999999999;
			}

			// Middle struct
			USTRUCT()
			struct FMiddleLayer
			{
				UPROPERTY()
				FInnermost Inner;

				UPROPERTY()
				int16 Int16Value = 777;
			}

			// Outermost struct
			USTRUCT()
			struct FOutermost
			{
				UPROPERTY()
				FMiddleLayer Middle;

				UPROPERTY()
				int Int32Value = 12345;
			}

			UCLASS()
			class ACoverageDeepNestedActor : AActor
			{
				UPROPERTY()
				FOutermost Root;
			}
			)AS"),
			TEXT("ACoverageDeepNestedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Deep nested struct actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Deep nested struct actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify deep nested default values
		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Root.Middle.Inner.Int8Value"),   static_cast<int8>(7),              TEXT("Deep nested int8 default should read back"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Root.Middle.Inner.UInt64Value"), static_cast<uint64>(999999999ull), TEXT("Deep nested uint64 default should read back"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Root.Middle.Int16Value"),        static_cast<int16>(777),           TEXT("Middle layer int16 default should read back"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Root.Int32Value"),               12345,                             TEXT("Outer layer int32 default should read back"));

		// Write through deep paths
		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Root.Middle.Inner.Int8Value"),   static_cast<int8>(-100))));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Root.Middle.Inner.UInt64Value"), static_cast<uint64>(18446744073709551615ull))));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Root.Middle.Int16Value"),        static_cast<int16>(-32000))));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Root.Int32Value"),               -2147483648)));

		// Verify deep nested round-trip
		VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Root.Middle.Inner.Int8Value"),   static_cast<int8>(-100),           TEXT("Deep nested int8 write should round-trip"));
		VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Root.Middle.Inner.UInt64Value"), static_cast<uint64>(18446744073709551615ull), TEXT("Deep nested uint64 write should round-trip"));
		VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Root.Middle.Int16Value"),        static_cast<int16>(-32000),        TEXT("Middle layer int16 write should round-trip"));
		VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("Root.Int32Value"),               -2147483648,                       TEXT("Outer layer int32 write should round-trip"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
