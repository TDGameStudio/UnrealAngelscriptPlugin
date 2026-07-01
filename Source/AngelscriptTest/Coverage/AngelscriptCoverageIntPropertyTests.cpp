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
// Detailed coverage matrix: OpenSpec: test-coverage/coverage-matrix.md
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-defaults actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Int8Value"),   static_cast<int8>(100),            TEXT("int8 UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Int16Value"),  static_cast<int16>(30000),         TEXT("int16 UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("IntValue"),    123456,                            TEXT("int UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Int64Value"),  static_cast<int64>(10000000000LL), TEXT("int64 UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  static_cast<uint8>(250),           TEXT("uint8 UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(60000),        TEXT("uint16 UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), static_cast<uint32>(123456),       TEXT("uint UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), static_cast<uint64>(123456),       TEXT("uint64 UPROPERTY default should read back"))));
	}

	TEST_METHOD(IntFamilyImplicitAndExplicitZeroDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_ZeroDefaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyZeroDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntZeroDefaultsActor : AActor
			{
				UPROPERTY()
				int8 NoDefaultInt8;

				UPROPERTY()
				int16 NoDefaultInt16;

				UPROPERTY()
				int NoDefaultInt;

				UPROPERTY()
				int64 NoDefaultInt64;

				UPROPERTY()
				uint8 NoDefaultUInt8;

				UPROPERTY()
				uint16 NoDefaultUInt16;

				UPROPERTY()
				uint NoDefaultUInt;

				UPROPERTY()
				uint64 NoDefaultUInt64;

				UPROPERTY()
				int ExplicitZero = 0;

				UPROPERTY()
				int8 NegativeDefault = -100;

				UPROPERTY()
				int LargeDefault = 2147483647;

				UPROPERTY()
				uint64 LargeUnsignedDefault = 18000000000000000000;
			}
			)AS"),
			TEXT("ACoverageIntZeroDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int zero-defaults actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int zero-defaults actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("NoDefaultInt8"), static_cast<int8>(0), TEXT("int8 UPROPERTY without initializer should default to zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("NoDefaultInt16"), static_cast<int16>(0), TEXT("int16 UPROPERTY without initializer should default to zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NoDefaultInt"), 0, TEXT("int UPROPERTY without initializer should default to zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("NoDefaultInt64"), static_cast<int64>(0), TEXT("int64 UPROPERTY without initializer should default to zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty, uint8>(*TestRunner, Actor, TEXT("NoDefaultUInt8"), static_cast<uint8>(0), TEXT("uint8 UPROPERTY without initializer should default to zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("NoDefaultUInt16"), static_cast<uint16>(0), TEXT("uint16 UPROPERTY without initializer should default to zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("NoDefaultUInt"), static_cast<uint32>(0), TEXT("uint UPROPERTY without initializer should default to zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("NoDefaultUInt64"), static_cast<uint64>(0), TEXT("uint64 UPROPERTY without initializer should default to zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ExplicitZero"), 0, TEXT("explicit zero int UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("NegativeDefault"), static_cast<int8>(-100), TEXT("negative int8 UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LargeDefault"), TNumericLimits<int32>::Max(), TEXT("large int UPROPERTY default should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("LargeUnsignedDefault"), static_cast<uint64>(18000000000000000000ull), TEXT("large uint64 UPROPERTY default should read back"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-round-trip actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Int8Value"),   static_cast<int8>(-42)),         TEXT("int8 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Int16Value"),  static_cast<int16>(-12345)),     TEXT("int16 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("IntValue"),    -987654),                        TEXT("int SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Int64Value"),  static_cast<int64>(-9000000000LL)), TEXT("int64 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  static_cast<uint8>(200)),        TEXT("uint8 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(54321)),     TEXT("uint16 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), static_cast<uint32>(3000000000u)), TEXT("uint SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), static_cast<uint64>(12000000000000000000ull)), TEXT("uint64 SetByPath")));

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property,   int8  >(*TestRunner, Actor, TEXT("Int8Value"),   static_cast<int8>(-42),            TEXT("int8 write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property,  int16 >(*TestRunner, Actor, TEXT("Int16Value"),  static_cast<int16>(-12345),        TEXT("int16 write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty,    int32 >(*TestRunner, Actor, TEXT("IntValue"),    -987654,                           TEXT("int write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property,  int64 >(*TestRunner, Actor, TEXT("Int64Value"),  static_cast<int64>(-9000000000LL), TEXT("int64 write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  static_cast<uint8>(200),           TEXT("uint8 write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(54321),        TEXT("uint16 write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), static_cast<uint32>(3000000000u),  TEXT("uint write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), static_cast<uint64>(12000000000000000000ull), TEXT("uint64 write should round-trip"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-boundary actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		// --- Minimums ---
		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,  int8 >(*TestRunner, Actor, TEXT("Int8Value"),  TNumericLimits<int8>::Min()),  TEXT("int8 min SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), TNumericLimits<int16>::Min()), TEXT("int16 min SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,   int32>(*TestRunner, Actor, TEXT("IntValue"),   TNumericLimits<int32>::Min()), TEXT("int min SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), TNumericLimits<int64>::Min()), TEXT("int64 min SetByPath")));

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property,  int8 >(*TestRunner, Actor, TEXT("Int8Value"),  TNumericLimits<int8>::Min(),  TEXT("int8 should hold its minimum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), TNumericLimits<int16>::Min(), TEXT("int16 should hold its minimum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty,   int32>(*TestRunner, Actor, TEXT("IntValue"),   TNumericLimits<int32>::Min(), TEXT("int should hold its minimum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), TNumericLimits<int64>::Min(), TEXT("int64 should hold its minimum"))));

		// --- Maximums (signed) ---
		ASSERT_THAT(IsTrue(SetByPath<FInt8Property,  int8 >(*TestRunner, Actor, TEXT("Int8Value"),  TNumericLimits<int8>::Max()),  TEXT("int8 max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), TNumericLimits<int16>::Max()), TEXT("int16 max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty,   int32>(*TestRunner, Actor, TEXT("IntValue"),   TNumericLimits<int32>::Max()), TEXT("int max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), TNumericLimits<int64>::Max()), TEXT("int64 max SetByPath")));

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property,  int8 >(*TestRunner, Actor, TEXT("Int8Value"),  TNumericLimits<int8>::Max(),  TEXT("int8 should hold its maximum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), TNumericLimits<int16>::Max(), TEXT("int16 should hold its maximum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty,   int32>(*TestRunner, Actor, TEXT("IntValue"),   TNumericLimits<int32>::Max(), TEXT("int should hold its maximum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), TNumericLimits<int64>::Max(), TEXT("int64 should hold its maximum"))));

		// --- Maximums (unsigned) ---
		ASSERT_THAT(IsTrue(SetByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  TNumericLimits<uint8>::Max()),  TEXT("uint8 max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), TNumericLimits<uint16>::Max()), TEXT("uint16 max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), TNumericLimits<uint32>::Max()), TEXT("uint max SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), TNumericLimits<uint64>::Max()), TEXT("uint64 max SetByPath")));

		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty,   uint8 >(*TestRunner, Actor, TEXT("UInt8Value"),  TNumericLimits<uint8>::Max(),  TEXT("uint8 should hold its maximum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), TNumericLimits<uint16>::Max(), TEXT("uint16 should hold its maximum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), TNumericLimits<uint32>::Max(), TEXT("uint should hold its maximum"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), TNumericLimits<uint64>::Max(), TEXT("uint64 should hold its maximum"))));
	}

	TEST_METHOD(IntFamilyNearBoundaryValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_NearBoundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyNearBoundary.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntNearBoundaryActor : AActor
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
			TEXT("ACoverageIntNearBoundaryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int near-boundary actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int near-boundary actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		const int8 Int8MinPlusOne = static_cast<int8>(TNumericLimits<int8>::Min() + 1);
		const int16 Int16MinPlusOne = static_cast<int16>(TNumericLimits<int16>::Min() + 1);
		const int32 IntMinPlusOne = TNumericLimits<int32>::Min() + 1;
		const int64 Int64MinPlusOne = TNumericLimits<int64>::Min() + 1;
		const int8 Int8MaxMinusOne = static_cast<int8>(TNumericLimits<int8>::Max() - 1);
		const int16 Int16MaxMinusOne = static_cast<int16>(TNumericLimits<int16>::Max() - 1);
		const int32 IntMaxMinusOne = TNumericLimits<int32>::Max() - 1;
		const int64 Int64MaxMinusOne = TNumericLimits<int64>::Max() - 1;

		ASSERT_THAT(IsTrue(SetByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Value"), Int8MinPlusOne), TEXT("int8 min+1 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), Int16MinPlusOne), TEXT("int16 min+1 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), IntMinPlusOne), TEXT("int min+1 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), Int64MinPlusOne), TEXT("int64 min+1 SetByPath")));

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Value"), Int8MinPlusOne, TEXT("int8 should hold min+1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), Int16MinPlusOne, TEXT("int16 should hold min+1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), IntMinPlusOne, TEXT("int should hold min+1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), Int64MinPlusOne, TEXT("int64 should hold min+1"))));

		ASSERT_THAT(IsTrue(SetByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Value"), Int8MaxMinusOne), TEXT("int8 max-1 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), Int16MaxMinusOne), TEXT("int16 max-1 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), IntMaxMinusOne), TEXT("int max-1 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), Int64MaxMinusOne), TEXT("int64 max-1 SetByPath")));

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Value"), Int8MaxMinusOne, TEXT("int8 should hold max-1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), Int16MaxMinusOne, TEXT("int16 should hold max-1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), IntMaxMinusOne, TEXT("int should hold max-1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), Int64MaxMinusOne, TEXT("int64 should hold max-1"))));

		ASSERT_THAT(IsTrue(SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), -1), TEXT("int -1 SetByPath")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), -1, TEXT("int should hold -1"))));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), 0), TEXT("int 0 SetByPath")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), 0, TEXT("int should hold 0"))));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), 1), TEXT("int 1 SetByPath")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), 1, TEXT("int should hold 1"))));

		ASSERT_THAT(IsTrue(SetByPath<FByteProperty, uint8>(*TestRunner, Actor, TEXT("UInt8Value"), static_cast<uint8>(1)), TEXT("uint8 near-zero SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(TNumericLimits<uint16>::Max() - 1)), TEXT("uint16 max-1 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), TNumericLimits<uint32>::Max() - 1), TEXT("uint max-1 SetByPath")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), TNumericLimits<uint64>::Max() - 1), TEXT("uint64 max-1 SetByPath")));

		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty, uint8>(*TestRunner, Actor, TEXT("UInt8Value"), static_cast<uint8>(1), TEXT("uint8 should hold 1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(TNumericLimits<uint16>::Max() - 1), TEXT("uint16 should hold max-1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UInt32Value"), TNumericLimits<uint32>::Max() - 1, TEXT("uint should hold max-1"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), TNumericLimits<uint64>::Max() - 1, TEXT("uint64 should hold max-1"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		// --- TArray<int> ---
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("IntArray"), Count), TEXT("TArray<int> length should resolve")));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TArray<int> should report three elements")));
		}
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[0]"), 10, TEXT("TArray<int>[0]"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[2]"), 30, TEXT("TArray<int>[2]"))));

		// --- TArray<int64> ---
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Array[1]"), static_cast<int64>(2000000000000LL), TEXT("TArray<int64>[1]"))));

		// --- TArray<uint8> ---
		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty, uint8>(*TestRunner, Actor, TEXT("ByteArray[1]"), static_cast<uint8>(255), TEXT("TArray<uint8>[1]"))));

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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-container-ext actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		// --- TArray<int8> ---
		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Array[0]"), static_cast<int8>(-42), TEXT("TArray<int8>[0]"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Array[1]"), static_cast<int8>(127), TEXT("TArray<int8>[1]"))));

		// --- TArray<int16> ---
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Array[0]"), static_cast<int16>(-12345), TEXT("TArray<int16>[0]"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Array[1]"), static_cast<int16>(30000), TEXT("TArray<int16>[1]"))));

		// --- TArray<uint16> ---
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Array[0]"), static_cast<uint16>(60000), TEXT("TArray<uint16>[0]"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Array[1]"), static_cast<uint16>(65535), TEXT("TArray<uint16>[1]"))));

		// --- TArray<uint> ---
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UIntArray[0]"), static_cast<uint32>(3000000000u), TEXT("TArray<uint>[0]"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UIntArray[1]"), static_cast<uint32>(4000000000u), TEXT("TArray<uint>[1]"))));

		// --- TArray<uint64> ---
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Array[0]"), static_cast<uint64>(10000000000000000000ull), TEXT("TArray<uint64>[0]"))));

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

	TEST_METHOD(IntContainerWidthCompletion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_ContainerWidths"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyContainerWidths.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntContainerWidthsActor : AActor
			{
				UPROPERTY()
				TMap<FString, int8> StringToInt8Map;

				UPROPERTY()
				TMap<FString, int16> StringToInt16Map;

				UPROPERTY()
				TMap<FString, int64> StringToInt64Map;

				UPROPERTY()
				TMap<FString, uint8> StringToUInt8Map;

				UPROPERTY()
				TMap<FString, uint16> StringToUInt16Map;

				UPROPERTY()
				TMap<FString, uint> StringToUIntMap;

				UPROPERTY()
				TMap<FString, uint64> StringToUInt64Map;

				UPROPERTY()
				TMap<int16, FString> Int16ToStringMap;

				UPROPERTY()
				TMap<uint8, FString> UInt8ToStringMap;

				UPROPERTY()
				TMap<uint16, FString> UInt16ToStringMap;

				UPROPERTY()
				TMap<uint64, FString> UInt64ToStringMap;

				UPROPERTY()
				TSet<int16> Int16Set;

				UPROPERTY()
				TSet<uint8> UInt8Set;

				UPROPERTY()
				TSet<uint16> UInt16Set;

				UPROPERTY()
				TSet<uint64> UInt64Set;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StringToInt8Map.Add("Int8", -12);
					StringToInt16Map.Add("Int16", -1234);
					StringToInt64Map.Add("Int64", -9000000000);
					StringToUInt8Map.Add("UInt8", 250);
					StringToUInt16Map.Add("UInt16", 60000);
					StringToUIntMap.Add("UInt", 3000000000);
					StringToUInt64Map.Add("UInt64", 12000000000000000000);

					Int16ToStringMap.Add(-1234, "Int16Key");
					UInt8ToStringMap.Add(250, "UInt8Key");
					UInt16ToStringMap.Add(60000, "UInt16Key");
					UInt64ToStringMap.Add(12000000000000000000, "UInt64Key");

					Int16Set.Add(-1234);
					Int16Set.Add(30000);
					UInt8Set.Add(1);
					UInt8Set.Add(250);
					UInt16Set.Add(60000);
					UInt16Set.Add(65535);
					UInt64Set.Add(10000000000000000000);
					UInt64Set.Add(12000000000000000000);
				}
			}
			)AS"),
			TEXT("ACoverageIntContainerWidthsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int container-width actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int container-width actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		{
			int8 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FInt8Property, int8>(*TestRunner, Actor, TEXT("StringToInt8Map"), FString(TEXT("Int8")), Value),
				TEXT("TMap<FString,int8> value lookup should resolve")));
			ASSERT_THAT(AreEqual(static_cast<int8>(-12), Value, TEXT("TMap<FString,int8> should preserve int8 values")));
		}
		{
			int16 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FInt16Property, int16>(*TestRunner, Actor, TEXT("StringToInt16Map"), FString(TEXT("Int16")), Value),
				TEXT("TMap<FString,int16> value lookup should resolve")));
			ASSERT_THAT(AreEqual(static_cast<int16>(-1234), Value, TEXT("TMap<FString,int16> should preserve int16 values")));
		}
		{
			int64 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FInt64Property, int64>(*TestRunner, Actor, TEXT("StringToInt64Map"), FString(TEXT("Int64")), Value),
				TEXT("TMap<FString,int64> value lookup should resolve")));
			ASSERT_THAT(AreEqual(static_cast<int64>(-9000000000LL), Value, TEXT("TMap<FString,int64> should preserve int64 values")));
		}
		{
			uint8 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FByteProperty, uint8>(*TestRunner, Actor, TEXT("StringToUInt8Map"), FString(TEXT("UInt8")), Value),
				TEXT("TMap<FString,uint8> value lookup should resolve")));
			ASSERT_THAT(AreEqual(static_cast<uint8>(250), Value, TEXT("TMap<FString,uint8> should preserve uint8 values")));
		}
		{
			uint16 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FUInt16Property, uint16>(*TestRunner, Actor, TEXT("StringToUInt16Map"), FString(TEXT("UInt16")), Value),
				TEXT("TMap<FString,uint16> value lookup should resolve")));
			ASSERT_THAT(AreEqual(static_cast<uint16>(60000), Value, TEXT("TMap<FString,uint16> should preserve uint16 values")));
		}
		{
			uint32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FUInt32Property, uint32>(*TestRunner, Actor, TEXT("StringToUIntMap"), FString(TEXT("UInt")), Value),
				TEXT("TMap<FString,uint> value lookup should resolve")));
			ASSERT_THAT(AreEqual(static_cast<uint32>(3000000000u), Value, TEXT("TMap<FString,uint> should preserve uint values")));
		}
		{
			uint64 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FUInt64Property, uint64>(*TestRunner, Actor, TEXT("StringToUInt64Map"), FString(TEXT("UInt64")), Value),
				TEXT("TMap<FString,uint64> value lookup should resolve")));
			ASSERT_THAT(AreEqual(static_cast<uint64>(12000000000000000000ull), Value, TEXT("TMap<FString,uint64> should preserve uint64 values")));
		}
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int16, FStrProperty, FString>(*TestRunner, Actor, TEXT("Int16ToStringMap"), static_cast<int16>(-1234), Value),
				TEXT("TMap<int16,FString> key lookup should resolve")));
			ASSERT_THAT(AreEqual(FString(TEXT("Int16Key")), Value, TEXT("TMap<int16,FString> should preserve int16 keys")));
		}
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<uint8, FStrProperty, FString>(*TestRunner, Actor, TEXT("UInt8ToStringMap"), static_cast<uint8>(250), Value),
				TEXT("TMap<uint8,FString> key lookup should resolve")));
			ASSERT_THAT(AreEqual(FString(TEXT("UInt8Key")), Value, TEXT("TMap<uint8,FString> should preserve uint8 keys")));
		}
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<uint16, FStrProperty, FString>(*TestRunner, Actor, TEXT("UInt16ToStringMap"), static_cast<uint16>(60000), Value),
				TEXT("TMap<uint16,FString> key lookup should resolve")));
			ASSERT_THAT(AreEqual(FString(TEXT("UInt16Key")), Value, TEXT("TMap<uint16,FString> should preserve uint16 keys")));
		}
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<uint64, FStrProperty, FString>(*TestRunner, Actor, TEXT("UInt64ToStringMap"), static_cast<uint64>(12000000000000000000ull), Value),
				TEXT("TMap<uint64,FString> key lookup should resolve")));
			ASSERT_THAT(AreEqual(FString(TEXT("UInt64Key")), Value, TEXT("TMap<uint64,FString> should preserve uint64 keys")));
		}

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("Int16Set"), Count), TEXT("TSet<int16> length should resolve")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<int16> should report two elements")));
		ASSERT_THAT(IsTrue(SetContainsByPath<int16>(*TestRunner, Actor, TEXT("Int16Set"), static_cast<int16>(30000)), TEXT("TSet<int16> should contain 30000")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("UInt8Set"), Count), TEXT("TSet<uint8> length should resolve")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<uint8> should report two elements")));
		ASSERT_THAT(IsTrue(SetContainsByPath<uint8>(*TestRunner, Actor, TEXT("UInt8Set"), static_cast<uint8>(250)), TEXT("TSet<uint8> should contain 250")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("UInt16Set"), Count), TEXT("TSet<uint16> length should resolve")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<uint16> should report two elements")));
		ASSERT_THAT(IsTrue(SetContainsByPath<uint16>(*TestRunner, Actor, TEXT("UInt16Set"), static_cast<uint16>(65535)), TEXT("TSet<uint16> should contain 65535")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("UInt64Set"), Count), TEXT("TSet<uint64> length should resolve")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<uint64> should report two elements")));
		ASSERT_THAT(IsTrue(SetContainsByPath<uint64>(*TestRunner, Actor, TEXT("UInt64Set"), static_cast<uint64>(12000000000000000000ull)), TEXT("TSet<uint64> should contain 12000000000000000000")));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int-container-edge actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

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

			ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SingleElementArray[0]"), 42, TEXT("Single element TArray[0]"))));
		}

		// --- Modified Array (after RemoveAt) ---
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ModifiedArray"), Length)));
			ASSERT_THAT(AreEqual(2, Length, TEXT("Modified TArray should have 2 elements after RemoveAt")));

			ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[0]"), 1, TEXT("Modified TArray[0]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ModifiedArray[1]"), 3, TEXT("Modified TArray[1] after remove"))));
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

	TEST_METHOD(IntNestedArrayContainerBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> ExpectedDiagnostics = { TEXT("Containers cannot be nested in other containers") };

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageIntProperty_NestedArrayUnsupported"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntNestedArrayActor : AActor
			{
				UPROPERTY()
				TArray<TArray<int>> Matrix;
			}
			)AS"),
			TEXT("TArray<TArray<int>> should remain an explicit unsupported boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(IntStructNestedPropertyWidths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_StructNestedWidths"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyStructNestedWidths.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FCoverageIntNestedWidths
			{
				UPROPERTY()
				int8 Int8Value = -12;

				UPROPERTY()
				int16 Int16Value = -1234;

				UPROPERTY()
				int IntValue = 123456;

				UPROPERTY()
				int64 Int64Value = -9000000000;

				UPROPERTY()
				uint8 UInt8Value = 250;

				UPROPERTY()
				uint16 UInt16Value = 60000;

				UPROPERTY()
				uint UIntValue = 3000000000;

				UPROPERTY()
				uint64 UInt64Value = 12000000000000000000;
			}

			UCLASS()
			class ACoverageIntStructNestedWidthsActor : AActor
			{
				UPROPERTY()
				FCoverageIntNestedWidths Stats;
			}
			)AS"),
			TEXT("ACoverageIntStructNestedWidthsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int struct-nested actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FStructProperty* StatsProperty = CastField<FStructProperty>(ScriptClass->FindPropertyByName(FName(TEXT("Stats"))));
		ASSERT_THAT(IsNotNull(StatsProperty, TEXT("Stats should reflect as an FStructProperty")));
		if (StatsProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FInt8Property>(StatsProperty->Struct->FindPropertyByName(FName(TEXT("Int8Value")))), TEXT("nested int8 should reflect as FInt8Property")));
		ASSERT_THAT(IsNotNull(CastField<FInt16Property>(StatsProperty->Struct->FindPropertyByName(FName(TEXT("Int16Value")))), TEXT("nested int16 should reflect as FInt16Property")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(StatsProperty->Struct->FindPropertyByName(FName(TEXT("IntValue")))), TEXT("nested int should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FInt64Property>(StatsProperty->Struct->FindPropertyByName(FName(TEXT("Int64Value")))), TEXT("nested int64 should reflect as FInt64Property")));
		ASSERT_THAT(IsNotNull(CastField<FByteProperty>(StatsProperty->Struct->FindPropertyByName(FName(TEXT("UInt8Value")))), TEXT("nested uint8 should reflect as FByteProperty")));
		ASSERT_THAT(IsNotNull(CastField<FUInt16Property>(StatsProperty->Struct->FindPropertyByName(FName(TEXT("UInt16Value")))), TEXT("nested uint16 should reflect as FUInt16Property")));
		ASSERT_THAT(IsNotNull(CastField<FUInt32Property>(StatsProperty->Struct->FindPropertyByName(FName(TEXT("UIntValue")))), TEXT("nested uint should reflect as FUInt32Property")));
		ASSERT_THAT(IsNotNull(CastField<FUInt64Property>(StatsProperty->Struct->FindPropertyByName(FName(TEXT("UInt64Value")))), TEXT("nested uint64 should reflect as FUInt64Property")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int struct-nested actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Stats.Int8Value"), static_cast<int8>(-12), TEXT("nested int8 USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Stats.Int16Value"), static_cast<int16>(-1234), TEXT("nested int16 USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Stats.IntValue"), 123456, TEXT("nested int USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Stats.Int64Value"), static_cast<int64>(-9000000000LL), TEXT("nested int64 USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty, uint8>(*TestRunner, Actor, TEXT("Stats.UInt8Value"), static_cast<uint8>(250), TEXT("nested uint8 USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("Stats.UInt16Value"), static_cast<uint16>(60000), TEXT("nested uint16 USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("Stats.UIntValue"), static_cast<uint32>(3000000000u), TEXT("nested uint USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Stats.UInt64Value"), static_cast<uint64>(12000000000000000000ull), TEXT("nested uint64 USTRUCT member should read back"))));

		ASSERT_THAT(IsTrue(SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Stats.IntValue"), -777), TEXT("nested int USTRUCT member should be writable by path")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Stats.IntValue"), -777, TEXT("nested int USTRUCT member write should round-trip"))));
	}

	TEST_METHOD(IntStructDeepNestedPropertyPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_StructDeepPaths"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyStructDeepPaths.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FCoverageIntPropertyDeepInner
			{
				UPROPERTY()
				int8 Int8Value = 7;

				UPROPERTY()
				uint64 UInt64Value = 999999999;
			}

			USTRUCT()
			struct FCoverageIntPropertyDeepMiddle
			{
				UPROPERTY()
				FCoverageIntPropertyDeepInner Inner;

				UPROPERTY()
				int16 Int16Value = 777;
			}

			USTRUCT()
			struct FCoverageIntPropertyDeepRoot
			{
				UPROPERTY()
				FCoverageIntPropertyDeepMiddle Middle;

				UPROPERTY()
				int IntValue = 12345;
			}

			UCLASS()
			class ACoverageIntStructDeepPathsActor : AActor
			{
				UPROPERTY()
				FCoverageIntPropertyDeepRoot Root;
			}
			)AS"),
			TEXT("ACoverageIntStructDeepPathsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int deep-struct actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FStructProperty* RootProperty = CastField<FStructProperty>(ScriptClass->FindPropertyByName(FName(TEXT("Root"))));
		ASSERT_THAT(IsNotNull(RootProperty, TEXT("Root should reflect as an FStructProperty")));
		if (RootProperty == nullptr)
		{
			return;
		}

		const FStructProperty* MiddleProperty = CastField<FStructProperty>(RootProperty->Struct->FindPropertyByName(FName(TEXT("Middle"))));
		ASSERT_THAT(IsNotNull(MiddleProperty, TEXT("Root.Middle should reflect as an FStructProperty")));
		if (MiddleProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(MiddleProperty->Struct->FindPropertyByName(FName(TEXT("Inner")))), TEXT("Root.Middle.Inner should reflect as an FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(RootProperty->Struct->FindPropertyByName(FName(TEXT("IntValue")))), TEXT("Root.IntValue should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FInt16Property>(MiddleProperty->Struct->FindPropertyByName(FName(TEXT("Int16Value")))), TEXT("Root.Middle.Int16Value should reflect as FInt16Property")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int deep-struct actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Root.Middle.Inner.Int8Value"), static_cast<int8>(7), TEXT("deep nested int8 USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Root.Middle.Inner.UInt64Value"), static_cast<uint64>(999999999ull), TEXT("deep nested uint64 USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Root.Middle.Int16Value"), static_cast<int16>(777), TEXT("middle nested int16 USTRUCT member should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Root.IntValue"), 12345, TEXT("root nested int USTRUCT member should read back"))));

		ASSERT_THAT(IsTrue(SetByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Root.Middle.Inner.Int8Value"), static_cast<int8>(-100)), TEXT("deep nested int8 USTRUCT member should be writable by path")));
		ASSERT_THAT(IsTrue(SetByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Root.Middle.Inner.UInt64Value"), TNumericLimits<uint64>::Max()), TEXT("deep nested uint64 USTRUCT member should be writable by path")));
		ASSERT_THAT(IsTrue(SetByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Root.Middle.Int16Value"), static_cast<int16>(-32000)), TEXT("middle nested int16 USTRUCT member should be writable by path")));
		ASSERT_THAT(IsTrue(SetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Root.IntValue"), TNumericLimits<int32>::Min()), TEXT("root nested int USTRUCT member should be writable by path")));

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Root.Middle.Inner.Int8Value"), static_cast<int8>(-100), TEXT("deep nested int8 USTRUCT member write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("Root.Middle.Inner.UInt64Value"), TNumericLimits<uint64>::Max(), TEXT("deep nested uint64 USTRUCT member write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Root.Middle.Int16Value"), static_cast<int16>(-32000), TEXT("middle nested int16 USTRUCT member write should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Root.IntValue"), TNumericLimits<int32>::Min(), TEXT("root nested int USTRUCT member write should round-trip"))));
	}

	TEST_METHOD(IntPropertyScriptReadWriteApiSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_ScriptApiSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertyScriptApiSurface.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntScriptApiSurfaceActor : AActor
			{
				UPROPERTY()
				int8 Int8Value = -1;

				UPROPERTY()
				int16 Int16Value = -2;

				UPROPERTY()
				int IntValue = -3;

				UPROPERTY()
				int64 Int64Value = -4;

				UPROPERTY()
				uint8 UInt8Value = 5;

				UPROPERTY()
				uint16 UInt16Value = 6;

				UPROPERTY()
				uint UIntValue = 7;

				UPROPERTY()
				uint64 UInt64Value = 8;

				UFUNCTION()
				int ReadCurrentSum()
				{
					return int(Int8Value) + int(Int16Value) + IntValue + int(Int64Value)
						+ int(UInt8Value) + int(UInt16Value) + int(UIntValue) + int(UInt64Value);
				}

				UFUNCTION()
				int RewriteAndReadCurrentSum()
				{
					Int8Value = -8;
					Int16Value = -16;
					IntValue = -32;
					Int64Value = -64;
					UInt8Value = 8;
					UInt16Value = 16;
					UIntValue = 32;
					UInt64Value = 64;
					return ReadCurrentSum();
				}
			}
			)AS"),
			TEXT("ACoverageIntScriptApiSurfaceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int script API surface actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FInt8Property>(ScriptClass->FindPropertyByName(FName(TEXT("Int8Value")))), TEXT("script int8 UPROPERTY should reflect as FInt8Property")));
		ASSERT_THAT(IsNotNull(CastField<FInt16Property>(ScriptClass->FindPropertyByName(FName(TEXT("Int16Value")))), TEXT("script int16 UPROPERTY should reflect as FInt16Property")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(ScriptClass->FindPropertyByName(FName(TEXT("IntValue")))), TEXT("script int UPROPERTY should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FInt64Property>(ScriptClass->FindPropertyByName(FName(TEXT("Int64Value")))), TEXT("script int64 UPROPERTY should reflect as FInt64Property")));
		ASSERT_THAT(IsNotNull(CastField<FByteProperty>(ScriptClass->FindPropertyByName(FName(TEXT("UInt8Value")))), TEXT("script uint8 UPROPERTY should reflect as FByteProperty")));
		ASSERT_THAT(IsNotNull(CastField<FUInt16Property>(ScriptClass->FindPropertyByName(FName(TEXT("UInt16Value")))), TEXT("script uint16 UPROPERTY should reflect as FUInt16Property")));
		ASSERT_THAT(IsNotNull(CastField<FUInt32Property>(ScriptClass->FindPropertyByName(FName(TEXT("UIntValue")))), TEXT("script uint UPROPERTY should reflect as FUInt32Property")));
		ASSERT_THAT(IsNotNull(CastField<FUInt64Property>(ScriptClass->FindPropertyByName(FName(TEXT("UInt64Value")))), TEXT("script uint64 UPROPERTY should reflect as FUInt64Property")));

		const UFunction* ReadCurrentSumFunction = ScriptClass->FindFunctionByName(FName(TEXT("ReadCurrentSum")));
		ASSERT_THAT(IsNotNull(ReadCurrentSumFunction, TEXT("ReadCurrentSum should be reflected as a UFUNCTION")));
		if (ReadCurrentSumFunction == nullptr)
		{
			return;
		}

		const UFunction* RewriteAndReadCurrentSumFunction = ScriptClass->FindFunctionByName(FName(TEXT("RewriteAndReadCurrentSum")));
		ASSERT_THAT(IsNotNull(RewriteAndReadCurrentSumFunction, TEXT("RewriteAndReadCurrentSum should be reflected as a UFUNCTION")));
		if (RewriteAndReadCurrentSumFunction == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Int script API surface actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker ReadInvoker(*TestRunner, Actor, TEXT("ReadCurrentSum"));
		ASSERT_THAT(IsTrue(ReadInvoker.IsValid(), TEXT("ReadCurrentSum should be invokable through reflection")));
		if (!ReadInvoker.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(16, ReadInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("AS should read all int-family UPROPERTY values")));

		FFunctionInvoker RewriteInvoker(*TestRunner, Actor, TEXT("RewriteAndReadCurrentSum"));
		ASSERT_THAT(IsTrue(RewriteInvoker.IsValid(), TEXT("RewriteAndReadCurrentSum should be invokable through reflection")));
		if (!RewriteInvoker.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, RewriteInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("AS should write and reread all int-family UPROPERTY values")));

		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Value"), static_cast<int8>(-8), TEXT("script-written int8 UPROPERTY should read back in C++"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), static_cast<int16>(-16), TEXT("script-written int16 UPROPERTY should read back in C++"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), -32, TEXT("script-written int UPROPERTY should read back in C++"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), static_cast<int64>(-64), TEXT("script-written int64 UPROPERTY should read back in C++"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty, uint8>(*TestRunner, Actor, TEXT("UInt8Value"), static_cast<uint8>(8), TEXT("script-written uint8 UPROPERTY should read back in C++"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(16), TEXT("script-written uint16 UPROPERTY should read back in C++"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UIntValue"), static_cast<uint32>(32), TEXT("script-written uint UPROPERTY should read back in C++"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), static_cast<uint64>(64), TEXT("script-written uint64 UPROPERTY should read back in C++"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		auto HasFlag = [&](const TCHAR* Name, EPropertyFlags Flag, bool bExpected) -> bool
		{
			const FProperty* Found = ScriptClass->FindPropertyByName(FName(Name));
			if (Found == nullptr)
			{
				return false;
			}
			return Found->HasAnyPropertyFlags(Flag) == bExpected;
		};

		// --- Edit specifiers ---
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditAnywhereInt"), CPF_Edit, true), TEXT("EditAnywhere -> CPF_Edit")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditAnywhereInt"), CPF_DisableEditOnInstance, false), TEXT("EditAnywhere -> editable on instance")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditAnywhereInt"), CPF_DisableEditOnTemplate, false), TEXT("EditAnywhere -> editable on defaults")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditDefaultsOnlyInt"), CPF_Edit, true), TEXT("EditDefaultsOnly -> CPF_Edit")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditDefaultsOnlyInt"), CPF_DisableEditOnInstance, true), TEXT("EditDefaultsOnly -> disabled on instance")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditDefaultsOnlyInt"), CPF_DisableEditOnTemplate, false), TEXT("EditDefaultsOnly -> editable on defaults")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditInstanceOnlyInt"), CPF_Edit, true), TEXT("EditInstanceOnly -> CPF_Edit")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditInstanceOnlyInt"), CPF_DisableEditOnTemplate, true), TEXT("EditInstanceOnly -> disabled on defaults")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditInstanceOnlyInt"), CPF_DisableEditOnInstance, false), TEXT("EditInstanceOnly -> editable on instance")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("NotEditableInt"), CPF_Edit, false), TEXT("NotEditable -> clears CPF_Edit")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditConstInt"), CPF_Edit, true), TEXT("EditConst keeps default CPF_Edit")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditConstInt"), CPF_EditConst, true), TEXT("EditConst -> CPF_EditConst")));

		// --- Visible specifiers (edit-visible but read-only) ---
		ASSERT_THAT(IsTrue(HasFlag(TEXT("VisibleAnywhereInt"), CPF_Edit, true), TEXT("VisibleAnywhere -> CPF_Edit")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("VisibleAnywhereInt"), CPF_EditConst, true), TEXT("VisibleAnywhere -> CPF_EditConst")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("VisibleAnywhereInt"), CPF_DisableEditOnInstance, false), TEXT("VisibleAnywhere -> visible on instance")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("VisibleAnywhereInt"), CPF_DisableEditOnTemplate, false), TEXT("VisibleAnywhere -> visible on defaults")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("VisibleDefaultsOnlyInt"), CPF_EditConst, true), TEXT("VisibleDefaultsOnly -> CPF_EditConst")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("VisibleDefaultsOnlyInt"), CPF_DisableEditOnInstance, true), TEXT("VisibleDefaultsOnly -> disabled on instance")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("VisibleInstanceOnlyInt"), CPF_EditConst, true), TEXT("VisibleInstanceOnly -> CPF_EditConst")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("VisibleInstanceOnlyInt"), CPF_DisableEditOnTemplate, true), TEXT("VisibleInstanceOnly -> disabled on defaults")));

		// --- Blueprint access specifiers ---
		ASSERT_THAT(IsTrue(HasFlag(TEXT("BlueprintReadWriteInt"), CPF_BlueprintVisible, true), TEXT("BlueprintReadWrite -> CPF_BlueprintVisible")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("BlueprintReadWriteInt"), CPF_BlueprintReadOnly, false), TEXT("BlueprintReadWrite -> not read-only")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("BlueprintReadOnlyInt"), CPF_BlueprintVisible, true), TEXT("BlueprintReadOnly -> CPF_BlueprintVisible")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("BlueprintReadOnlyInt"), CPF_BlueprintReadOnly, true), TEXT("BlueprintReadOnly -> CPF_BlueprintReadOnly")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("BlueprintHiddenInt"), CPF_BlueprintVisible, false), TEXT("BlueprintHidden -> clears CPF_BlueprintVisible")));

		// --- Standalone flag specifiers ---
		ASSERT_THAT(IsTrue(HasFlag(TEXT("TransientInt"), CPF_Transient, true), TEXT("Transient -> CPF_Transient")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("ConfigInt"), CPF_Config, true), TEXT("Config -> CPF_Config")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("SaveGameInt"), CPF_SaveGame, true), TEXT("SaveGame -> CPF_SaveGame")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("AdvancedDisplayInt"), CPF_AdvancedDisplay, true), TEXT("AdvancedDisplay -> CPF_AdvancedDisplay")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("InterpInt"), CPF_Interp, true), TEXT("Interp -> CPF_Interp")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("ExposeOnSpawnInt"), CPF_ExposeOnSpawn, true), TEXT("ExposeOnSpawn -> CPF_ExposeOnSpawn")));

		// --- Representative combinations ---
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditableReadOnlyInt"), CPF_Edit, true), TEXT("EditAnywhere+BlueprintReadOnly -> CPF_Edit")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditableReadOnlyInt"), CPF_BlueprintVisible, true), TEXT("EditAnywhere+BlueprintReadOnly -> CPF_BlueprintVisible")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("EditableReadOnlyInt"), CPF_BlueprintReadOnly, true), TEXT("EditAnywhere+BlueprintReadOnly -> CPF_BlueprintReadOnly")));

		ASSERT_THAT(IsTrue(HasFlag(TEXT("ComboInt"), CPF_Edit, true), TEXT("Combo -> CPF_Edit")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("ComboInt"), CPF_DisableEditOnInstance, true), TEXT("Combo (EditDefaultsOnly) -> disabled on instance")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("ComboInt"), CPF_BlueprintReadOnly, true), TEXT("Combo -> CPF_BlueprintReadOnly")));
		ASSERT_THAT(IsTrue(HasFlag(TEXT("ComboInt"), CPF_Transient, true), TEXT("Combo -> CPF_Transient")));

#if WITH_EDITOR
		// --- Meta keys round-trip (editor-only metadata store) ---
		const FProperty* Clamped = ScriptClass->FindPropertyByName(FName(TEXT("ClampedInt")));
		ASSERT_THAT(IsNotNull(Clamped, TEXT("ClampedInt should exist")));
		if (Clamped != nullptr)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("0")), Clamped->GetMetaData(TEXT("ClampMin")), TEXT("ClampMin meta should round-trip")));
			ASSERT_THAT(AreEqual(FString(TEXT("10")), Clamped->GetMetaData(TEXT("ClampMax")), TEXT("ClampMax meta should round-trip")));
		}

		const FProperty* UIRanged = ScriptClass->FindPropertyByName(FName(TEXT("UIRangedInt")));
		ASSERT_THAT(IsNotNull(UIRanged, TEXT("UIRangedInt should exist")));
		if (UIRanged != nullptr)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("1")), UIRanged->GetMetaData(TEXT("UIMin")), TEXT("UIMin meta should round-trip")));
			ASSERT_THAT(AreEqual(FString(TEXT("5")), UIRanged->GetMetaData(TEXT("UIMax")), TEXT("UIMax meta should round-trip")));
		}

		const FProperty* EditCond = ScriptClass->FindPropertyByName(FName(TEXT("EditConditionInt")));
		ASSERT_THAT(IsNotNull(EditCond, TEXT("EditConditionInt should exist")));
		if (EditCond != nullptr)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("Gate")), EditCond->GetMetaData(TEXT("EditCondition")), TEXT("EditCondition meta should round-trip")));
		}

		const FProperty* Categorized = ScriptClass->FindPropertyByName(FName(TEXT("CategorizedInt")));
		ASSERT_THAT(IsNotNull(Categorized, TEXT("CategorizedInt should exist")));
		if (Categorized != nullptr)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("Stats")), Categorized->GetMetaData(TEXT("Category")), TEXT("Category meta should round-trip")));
		}

		const FProperty* Combo = ScriptClass->FindPropertyByName(FName(TEXT("ComboInt")));
		ASSERT_THAT(IsNotNull(Combo, TEXT("ComboInt should exist")));
		if (Combo != nullptr)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("Tuning")), Combo->GetMetaData(TEXT("Category")), TEXT("Combo Category meta should round-trip")));
		}
#endif
	}

	TEST_METHOD(IntPropertySpecifierRepresentativeWidths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageIntProperty_SpecifierWidths"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageIntPropertySpecifierWidths.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageIntSpecifierWidthsActor : AActor
			{
				UPROPERTY(EditAnywhere)
				int8 EditAnywhereInt8 = 1;

				UPROPERTY(BlueprintReadOnly)
				int16 BlueprintReadOnlyInt16 = 2;

				UPROPERTY(NotEditable)
				int64 NotEditableInt64 = 3;

				UPROPERTY(Transient)
				uint8 TransientUInt8 = 4;

				UPROPERTY(SaveGame)
				uint16 SaveGameUInt16 = 5;

				UPROPERTY(meta = (ClampMin = "0", ClampMax = "4000000000"))
				uint ClampedUInt = 6;

				UPROPERTY(Category = "Coverage")
				uint64 CategorizedUInt64 = 7;
			}
			)AS"),
			TEXT("ACoverageIntSpecifierWidthsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Int specifier-width actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FProperty* EditAnywhereInt8 = ScriptClass->FindPropertyByName(FName(TEXT("EditAnywhereInt8")));
		ASSERT_THAT(IsNotNull(EditAnywhereInt8, TEXT("EditAnywhereInt8 should be reflected")));
		if (EditAnywhereInt8 != nullptr)
		{
			ASSERT_THAT(IsNotNull(CastField<const FInt8Property>(EditAnywhereInt8), TEXT("EditAnywhereInt8 should reflect as FInt8Property")));
			ASSERT_THAT(IsTrue(EditAnywhereInt8->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere int8 should set CPF_Edit")));
		}

		const FProperty* BlueprintReadOnlyInt16 = ScriptClass->FindPropertyByName(FName(TEXT("BlueprintReadOnlyInt16")));
		ASSERT_THAT(IsNotNull(BlueprintReadOnlyInt16, TEXT("BlueprintReadOnlyInt16 should be reflected")));
		if (BlueprintReadOnlyInt16 != nullptr)
		{
			ASSERT_THAT(IsNotNull(CastField<const FInt16Property>(BlueprintReadOnlyInt16), TEXT("BlueprintReadOnlyInt16 should reflect as FInt16Property")));
			ASSERT_THAT(IsTrue(BlueprintReadOnlyInt16->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly int16 should set CPF_BlueprintVisible")));
			ASSERT_THAT(IsTrue(BlueprintReadOnlyInt16->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly int16 should set CPF_BlueprintReadOnly")));
		}

		const FProperty* NotEditableInt64 = ScriptClass->FindPropertyByName(FName(TEXT("NotEditableInt64")));
		ASSERT_THAT(IsNotNull(NotEditableInt64, TEXT("NotEditableInt64 should be reflected")));
		if (NotEditableInt64 != nullptr)
		{
			ASSERT_THAT(IsNotNull(CastField<const FInt64Property>(NotEditableInt64), TEXT("NotEditableInt64 should reflect as FInt64Property")));
			ASSERT_THAT(IsFalse(NotEditableInt64->HasAnyPropertyFlags(CPF_Edit), TEXT("NotEditable int64 should clear CPF_Edit")));
		}

		const FProperty* TransientUInt8 = ScriptClass->FindPropertyByName(FName(TEXT("TransientUInt8")));
		ASSERT_THAT(IsNotNull(TransientUInt8, TEXT("TransientUInt8 should be reflected")));
		if (TransientUInt8 != nullptr)
		{
			ASSERT_THAT(IsNotNull(CastField<const FByteProperty>(TransientUInt8), TEXT("TransientUInt8 should reflect as FByteProperty")));
			ASSERT_THAT(IsTrue(TransientUInt8->HasAnyPropertyFlags(CPF_Transient), TEXT("Transient uint8 should set CPF_Transient")));
		}

		const FProperty* SaveGameUInt16 = ScriptClass->FindPropertyByName(FName(TEXT("SaveGameUInt16")));
		ASSERT_THAT(IsNotNull(SaveGameUInt16, TEXT("SaveGameUInt16 should be reflected")));
		if (SaveGameUInt16 != nullptr)
		{
			ASSERT_THAT(IsNotNull(CastField<const FUInt16Property>(SaveGameUInt16), TEXT("SaveGameUInt16 should reflect as FUInt16Property")));
			ASSERT_THAT(IsTrue(SaveGameUInt16->HasAnyPropertyFlags(CPF_SaveGame), TEXT("SaveGame uint16 should set CPF_SaveGame")));
		}

		const FProperty* ClampedUInt = ScriptClass->FindPropertyByName(FName(TEXT("ClampedUInt")));
		ASSERT_THAT(IsNotNull(ClampedUInt, TEXT("ClampedUInt should be reflected")));
		if (ClampedUInt != nullptr)
		{
			ASSERT_THAT(IsNotNull(CastField<const FUInt32Property>(ClampedUInt), TEXT("ClampedUInt should reflect as FUInt32Property")));
#if WITH_EDITOR
			ASSERT_THAT(AreEqual(FString(TEXT("0")), ClampedUInt->GetMetaData(TEXT("ClampMin")), TEXT("uint ClampMin metadata should round-trip")));
			ASSERT_THAT(AreEqual(FString(TEXT("4000000000")), ClampedUInt->GetMetaData(TEXT("ClampMax")), TEXT("uint ClampMax metadata should round-trip")));
#endif
		}

		const FProperty* CategorizedUInt64 = ScriptClass->FindPropertyByName(FName(TEXT("CategorizedUInt64")));
		ASSERT_THAT(IsNotNull(CategorizedUInt64, TEXT("CategorizedUInt64 should be reflected")));
		if (CategorizedUInt64 != nullptr)
		{
			ASSERT_THAT(IsNotNull(CastField<const FUInt64Property>(CategorizedUInt64), TEXT("CategorizedUInt64 should reflect as FUInt64Property")));
#if WITH_EDITOR
			ASSERT_THAT(AreEqual(FString(TEXT("Coverage")), CategorizedUInt64->GetMetaData(TEXT("Category")), TEXT("uint64 Category metadata should round-trip")));
#endif
		}
	}
};

#endif
