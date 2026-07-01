#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Vector2D.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtrTemplates.h"

// -----------------------------------------------------------------------------
// Template_ReflectionAccess
// -----------------------------------------------------------------------------
// Demonstrates how C++ tests should access AS-declared UPROPERTY / UFUNCTION
// members without the historical "hack" patterns (hand-written FindFProperty
// calls, manually packed parameter buffers, or casts to private AS internals).
//
// Ten tests are provided:
//
//   1. PathAndInvoke       - quick smoke test covering the essentials.
//   2. PathAccessAllTypes  - matrix over every AS-reflected UPROPERTY kind
//                            (primitives, name/string, struct, enum, object
//                            ref, TArray, nested paths).
//   3. FunctionInvokeAllTypes - matrix over AS UFUNCTION signatures (void,
//                            primitive, string, struct, object, enum, array,
//                            out-reference, return values).
//   4. PathAccessExtendedTypes - extended struct types (FVector2D, FIntPoint,
//                            FQuat, FTransform, FLinearColor, FColor, FGuid),
//                            TObjectPtr component refs, TSubclassOf, TMap,
//                            TSet, TOptional.
//   5. FunctionInvokeExtendedTypes - UFUNCTIONs that take/return the extended
//                            struct/container/class/component types.
//   6. PathAccessReferenceTypes - FText, TSoftObjectPtr, TSoftClassPtr,
//                            TWeakObjectPtr, and UFUNCTION parameter /
//                            return types. Includes an FInstancedStruct
//                            boundary note.
//   7. PathAccessScriptDefinedTypes - AS-*defined* USTRUCTs and UCLASSes used
//                            as UPROPERTYs and UFUNCTION parameters /
//                            returns. Uses an IsolatedFull engine so the AS
//                            type table outlives the spawned actors.
//   8. PathParserLimits   - pins down parser behaviour for unsupported path
//                            shapes (Map-keyed, double-indexed arrays) so
//                            regressions surface as bound failures, not
//                            mystery silent reads.
//   9. BPLibraryViaASGlobalWrapper - calls UBlueprintFunctionLibrary statics
//                            (`Math::Abs`, `Math::Sqrt`, `Math::Clamp`,
//                            `Math::Lerp`, `Math::IsNearlyEqual`, plus the
//                            project-local EligibleCallable fallback sample)
//                            from an AS module-level wrapper; the C++ test
//                            drives the wrapper through FASGlobalFunctionInvoker.
//                            This is the "BP library reached from AS, called
//                            from C++" round-trip.
//  10. BPLibraryThroughActorUFunction - composes the two paths: an AS-defined
//                            UFUNCTION on a spawned actor calls several
//                            `Math::*` BP statics internally, and the C++
//                            test reaches the actor method via FFunctionInvoker.
//
// IMPORTANT: `FPropertyBindingPath::FromString` understands only "Name" and
// "Name[integer]" segments. TMap, TSet, and multi-index chains
// (e.g. "Matrix[0][1]") are NOT natively navigable via a path string - use
// GetMapValueByPath / SetContainsByPath etc. instead.
//
// The helpers live in Shared/AngelscriptReflectiveAccess.h. Reading/writing
// UPROPERTYs always goes through FPropertyBindingPath::FromString +
// ResolveIndirectionsWithValue; invoking UFUNCTIONs always goes through
// FindFunction + UASFunction::RuntimeCallEvent. Neither helper touches private
// AngelScript types. Template scripts inherit directly from engine-registered
// types such as AActor; do not use test-module native base classes as AS-visible
// test fixtures.
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

// =============================================================================
// Test 1: PathAndInvoke - minimal smoke coverage
// =============================================================================

namespace TemplateReflectionAccessTest
{
#define TestTrue(Message, Expr) Test.TestTrue(Message, Expr)
#define TestFalse(Message, Expr) Test.TestFalse(Message, Expr)
#define TestEqual(Message, Actual, Expected) Test.TestEqual(Message, Actual, Expected)
#define TestNotNull(Message, Ptr) Test.TestNotNull(Message, Ptr)

	bool PathAndInvoke(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TemplateReflectionAccess"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	// --- 1. Compile a script class with a modest mix of AS-declared members so
	//        that Approaches A + B both have something to probe.
	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionAccess.as"),
		TEXT(R"AS(
UCLASS()
class ATemplateReflectionAccessActor : AActor
{
	UPROPERTY()
	int Health = 100;

	UPROPERTY()
	FString DisplayName = "DefaultName";

	UPROPERTY()
	TArray<int> DamageLog;

	UPROPERTY()
	FVector LastVectorValue = FVector::ZeroVector;

	UPROPERTY()
	int LastIntValue = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		LastVectorValue = FVector(1.0, 2.0, 3.0);
		DamageLog.Add(0);
		DamageLog.Add(0);
	}

	UFUNCTION()
	int ApplyDamage(float Amount, int Critical)
	{
		int Applied = int(Amount) * (Critical > 0 ? 2 : 1);
		Health -= Applied;
		DisplayName = "Damaged";
		DamageLog[0] = Applied;
		DamageLog[1] = Critical;
		LastIntValue = Applied;
		return Health;
	}
}
)AS"),
		TEXT("ATemplateReflectionAccessActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("Template actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	// Approach A - path-based Get / Set / Verify.
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("Health"), 100,
		TEXT("Top-level int UPROPERTY should read back its AS default"));
	VerifyByPath<FStrProperty, FString>(Test, Actor, TEXT("DisplayName"), FString(TEXT("DefaultName")),
		TEXT("Top-level FString UPROPERTY should read back its AS default"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("LastVectorValue.X"), 1.0,
		TEXT("Nested USTRUCT sub-field should read through 'LastVectorValue.X'"));
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("DamageLog[1]"), 0,
		TEXT("Array-indexed int should resolve 'DamageLog[1]'"));

	if (!SetByPath<FDoubleProperty, double>(Test, Actor, TEXT("LastVectorValue.Y"), 42.0))
	{
		return false;
	}
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("LastVectorValue.Y"), 42.0,
		TEXT("Writing through a path should round-trip when read back"));

	// Approach B - FindFunction + invoke via the typed invoker.
	// Note: AS `float` parameters are reflected as FDoubleProperty.
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("ApplyDamage")));
		if (!Invoker.IsValid())
		{
			return false;
		}
		Invoker.AddParam<double>(25.0);
		Invoker.AddParam<int32>(1);
		const int32 RemainingHealth = Invoker.CallAndReturn<int32>(/*Fallback=*/INDEX_NONE);
		TestEqual(TEXT("UFUNCTION return value should equal post-call Health"), RemainingHealth, 100 - 50);
	}

	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("Health"), 50,
		TEXT("Health should reflect damage applied by the AS UFUNCTION"));
	VerifyByPath<FStrProperty, FString>(Test, Actor, TEXT("DisplayName"), FString(TEXT("Damaged")),
		TEXT("DisplayName should reflect the UFUNCTION-side assignment"));
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("DamageLog[0]"), 50,
		TEXT("Array-indexed write from AS should be observable via path read"));
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("LastIntValue"), 50,
		TEXT("Inherited UPROPERTY assigned from AS should be observable via path read"));

	}
	return true;
}

// =============================================================================
// Test 2: PathAccessAllTypes - broad coverage of UPROPERTY types
// =============================================================================
//
// The Angelscript property model maps AS declarations onto UE's FProperty
// taxonomy. This test exercises every FProperty subclass that AS is expected to
// produce for a UPROPERTY, writes every slot from AS (via BeginPlay), then
// reads it back from C++ through a single path-based helper. If any of the
// reflection assumptions (type, offset, TArray layout, enum storage, USTRUCT
// nesting) regress, exactly one VerifyByPath line will flag it.

	bool PathAccessAllTypes(FAutomationTestBase& Test)
	{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TemplateReflectionAccessAllTypes"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionAccessAllTypes.as"),
		TEXT(R"AS(
// AS enum - bound as UENUM, stored as FEnumProperty with a signed underlying int.
enum ETemplateReflectionColor
{
	Red,
	Green,
	Blue,
	White
}

UCLASS()
class ATemplateReflectionAccessAllTypesActor : AActor
{
	// ---- Primitives ----
	UPROPERTY() bool        BoolValue   = true;
	UPROPERTY() uint8       ByteValue   = 250;       // FByteProperty (no UEnum)
	UPROPERTY() int         IntValue    = -123456;   // FIntProperty
	UPROPERTY() int64       Int64Value  = 1099511627783; // FInt64Property
	UPROPERTY() float       FloatValue  = 3.14;      // FFloatProperty (AS float stays float in properties)
	UPROPERTY() double      DoubleValue = 2.718281828459045;

	// ---- Text-like ----
	UPROPERTY() FName   NameValue   = n"HelloName";
	UPROPERTY() FString StringValue = "HelloString";

	// ---- Structs (C++-declared USTRUCTs used from AS) ----
	UPROPERTY() FVector  VectorValue  = FVector(10.0, 20.0, 30.0);
	UPROPERTY() FRotator RotatorValue = FRotator(45.0, 90.0, 180.0);

	// ---- Enum ----
	UPROPERTY() ETemplateReflectionColor ColorValue = ETemplateReflectionColor::Blue;

	// ---- Object reference ----
	UPROPERTY() AActor SelfRef = nullptr;

	// ---- Containers ----
	UPROPERTY() TArray<int>     IntArray;
	UPROPERTY() TArray<FString> StringArray;
	UPROPERTY() TArray<FVector> VectorArray;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		SelfRef = this;

		IntArray.Add(10);
		IntArray.Add(20);
		IntArray.Add(30);

		StringArray.Add("alpha");
		StringArray.Add("beta");

		VectorArray.Add(FVector(1.0, 0.0, 0.0));
		VectorArray.Add(FVector(0.0, 1.0, 0.0));
	}
}
)AS"),
		TEXT("ATemplateReflectionAccessAllTypesActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("All-types actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	// ---- Primitives ----
	VerifyByPath<FBoolProperty,   bool  >(Test, Actor, TEXT("BoolValue"),   true,                     TEXT("bool UPROPERTY should read back its AS default"));
	VerifyByPath<FByteProperty,   uint8 >(Test, Actor, TEXT("ByteValue"),   static_cast<uint8>(250),  TEXT("uint8 UPROPERTY should read back its AS default"));
	VerifyByPath<FIntProperty,    int32 >(Test, Actor, TEXT("IntValue"),    -123456,                  TEXT("int UPROPERTY should read back its AS default"));
	VerifyByPath<FInt64Property,  int64 >(Test, Actor, TEXT("Int64Value"),  static_cast<int64>(1099511627783LL), TEXT("int64 UPROPERTY should read back its AS default"));
	// AS `float` is promoted to FDoubleProperty in the UFunction/UProperty reflection
	// (AS defaults asEP_FLOAT_IS_FLOAT64 to 1). Read the value back as a double.
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("FloatValue"),  3.14,                     TEXT("AS float UPROPERTY is reflected as FDoubleProperty"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("DoubleValue"), 2.718281828459045,        TEXT("double UPROPERTY should read back its AS default"));

	// ---- Text-like ----
	VerifyByPath<FNameProperty, FName  >(Test, Actor, TEXT("NameValue"),   FName(TEXT("HelloName")), TEXT("FName UPROPERTY should read back its AS default"));
	VerifyByPath<FStrProperty,  FString>(Test, Actor, TEXT("StringValue"), FString(TEXT("HelloString")), TEXT("FString UPROPERTY should read back its AS default"));

	// ---- Structs: read whole struct (by value), and sub-field access via nested path ----
	{
		FVector Actual = FVector::ZeroVector;
		if (!GetStructByPath<FVector>(Test, Actor, TEXT("VectorValue"), Actual))
		{
			return false;
		}
		TestTrue(TEXT("FVector UPROPERTY should equal its AS default as a whole struct"),
			Actual.Equals(FVector(10.0, 20.0, 30.0)));
	}
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("VectorValue.X"),  10.0, TEXT("FVector.X should be reachable through nested path"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("VectorValue.Y"),  20.0, TEXT("FVector.Y should be reachable through nested path"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("VectorValue.Z"),  30.0, TEXT("FVector.Z should be reachable through nested path"));

	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("RotatorValue.Pitch"),  45.0, TEXT("FRotator.Pitch should be reachable through nested path"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("RotatorValue.Yaw"),    90.0, TEXT("FRotator.Yaw should be reachable through nested path"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("RotatorValue.Roll"),  180.0, TEXT("FRotator.Roll should be reachable through nested path"));

	// ---- Enum ----
	{
		int64 EnumRaw = -1;
		if (!GetEnumByPath(Test, Actor, TEXT("ColorValue"), EnumRaw))
		{
			return false;
		}
		TestEqual(TEXT("AS enum UPROPERTY should read back its underlying value"),
			EnumRaw, static_cast<int64>(2)); // Blue = 2
	}

	// ---- Object reference ----
	{
		UObject* SelfRef = nullptr;
		if (!GetObjectByPath(Test, Actor, TEXT("SelfRef"), SelfRef))
		{
			return false;
		}
		TestEqual(TEXT("AActor UPROPERTY assigned to `this` in BeginPlay should resolve to the actor"),
			SelfRef, static_cast<UObject*>(Actor));
	}

	// ---- Container lengths + indexed element access ----
	{
		int32 Count = 0;
		if (!GetArrayNumByPath(Test, Actor, TEXT("IntArray"), Count))
		{
			return false;
		}
		TestEqual(TEXT("TArray<int> should report the three elements added in AS BeginPlay"), Count, 3);
	}
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("IntArray[0]"), 10, TEXT("TArray<int>[0]"));
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("IntArray[2]"), 30, TEXT("TArray<int>[2]"));

	VerifyByPath<FStrProperty, FString>(Test, Actor, TEXT("StringArray[0]"), FString(TEXT("alpha")), TEXT("TArray<FString>[0]"));
	VerifyByPath<FStrProperty, FString>(Test, Actor, TEXT("StringArray[1]"), FString(TEXT("beta")),  TEXT("TArray<FString>[1]"));

	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("VectorArray[0].X"), 1.0, TEXT("TArray<FVector>[0].X"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("VectorArray[1].Y"), 1.0, TEXT("TArray<FVector>[1].Y"));

	// ---- Write-through-path round-trips cover the major leaf types ----
	if (!SetByPath<FIntProperty, int32>(Test, Actor, TEXT("IntValue"), 4242)) return false;
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("IntValue"), 4242, TEXT("Int SetByPath should round-trip"));

	if (!SetByPath<FStrProperty, FString>(Test, Actor, TEXT("StringValue"), FString(TEXT("Overwritten")))) return false;
	VerifyByPath<FStrProperty, FString>(Test, Actor, TEXT("StringValue"), FString(TEXT("Overwritten")), TEXT("String SetByPath should round-trip"));

	if (!SetByPath<FDoubleProperty, double>(Test, Actor, TEXT("VectorValue.X"), -1.0)) return false;
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("VectorValue.X"), -1.0, TEXT("Nested-path struct write should round-trip"));

	if (!SetByPath<FStrProperty, FString>(Test, Actor, TEXT("StringArray[1]"), FString(TEXT("beta-updated")))) return false;
	VerifyByPath<FStrProperty, FString>(Test, Actor, TEXT("StringArray[1]"), FString(TEXT("beta-updated")), TEXT("Array-indexed write should round-trip"));

	}
	return true;
}

// =============================================================================
// Test 3: FunctionInvokeAllTypes - broad coverage of UFUNCTION signatures
// =============================================================================
//
// Every AS UFUNCTION shape the invoker must support is exercised here against
// a dedicated "DrivenActor" pattern:
//   - the script object stores a result for each call inside its UPROPERTYs,
//   - the C++ test invokes the function reflectively, then reads the result
//     back through the path helpers (so both halves of the plumbing exercise
//     the reflective layer).
//
// Covered signatures:
//   - void, no params
//   - primitive in (bool, int, float->double, int64)
//   - FString in (non-trivial by-ref parameter)
//   - FVector in (USTRUCT by value)
//   - AActor in (UObject reference)
//   - enum in
//   - TArray<int> in (container-by-value parameter)
//   - FVector &out  (output reference round-trip)
//   - return int
//   - return FString

	bool FunctionInvokeAllTypes(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TemplateReflectionAccessInvokeAllTypes"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionAccessInvokeAllTypes.as"),
		TEXT(R"AS(
enum ETemplateInvokeMode
{
	None,
	Fast,
	Slow
}

UCLASS()
class ATemplateReflectionInvokeAllTypesActor : AActor
{
	// Per-overload result slots that the C++ test reads via path helpers.
	UPROPERTY() bool    LastBool    = false;
	UPROPERTY() int     LastInt     = 0;
	UPROPERTY() float   LastFloat   = 0.0;
	UPROPERTY() int64   LastInt64   = 0;
	UPROPERTY() FString LastFString;
	UPROPERTY() FVector LastStructIn = FVector::ZeroVector;
	UPROPERTY() AActor  LastObjectIn = nullptr;
	UPROPERTY() ETemplateInvokeMode LastEnum = ETemplateInvokeMode::None;
	UPROPERTY() TArray<int> LastArrayIn;
	UPROPERTY() bool    bVoidCalled = false;

	UFUNCTION()
	void NoArgEvent()
	{
		bVoidCalled = true;
	}

	UFUNCTION()
	void TakeBool(bool Flag)
	{
		LastBool = Flag;
	}

	UFUNCTION()
	void TakeInt(int Value)
	{
		LastInt = Value;
	}

	UFUNCTION()
	void TakeFloat(float Value)
	{
		LastFloat = Value;
	}

	UFUNCTION()
	void TakeInt64(int64 Value)
	{
		LastInt64 = Value;
	}

	UFUNCTION()
	void TakeString(FString Value)
	{
		LastFString = Value;
	}

	UFUNCTION()
	void TakeVector(FVector Value)
	{
		LastStructIn = Value;
	}

	UFUNCTION()
	void TakeActor(AActor Value)
	{
		LastObjectIn = Value;
	}

	UFUNCTION()
	void TakeEnum(ETemplateInvokeMode Value)
	{
		LastEnum = Value;
	}

	UFUNCTION()
	void TakeIntArray(TArray<int> Values)
	{
		LastArrayIn = Values;
	}

	// Out-reference round trip: C++ reads `OutValue` back out of the parm buffer.
	UFUNCTION()
	void FillVectorOut(FVector& OutValue)
	{
		OutValue = FVector(7.0, 8.0, 9.0);
	}

	UFUNCTION()
	int Sum(int A, int B)
	{
		return A + B;
	}

	UFUNCTION()
	FString BuildLabel(FString Prefix, int Counter)
	{
		return Prefix + "-" + Counter;
	}
}
)AS"),
		TEXT("ATemplateReflectionInvokeAllTypesActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("Invoke-all-types actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	// --- void, no params ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("NoArgEvent")));
		if (!Invoker.IsValid() || !Invoker.Call())
		{
			return false;
		}
	}
	VerifyByPath<FBoolProperty, bool>(Test, Actor, TEXT("bVoidCalled"), true, TEXT("No-arg UFUNCTION should flip bVoidCalled"));

	// --- bool in ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeBool")));
		Invoker.AddParam<bool>(true);
		if (!Invoker.Call()) return false;
	}
	VerifyByPath<FBoolProperty, bool>(Test, Actor, TEXT("LastBool"), true, TEXT("bool in-param should round-trip"));

	// --- int in ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeInt")));
		Invoker.AddParam<int32>(987654);
		if (!Invoker.Call()) return false;
	}
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("LastInt"), 987654, TEXT("int in-param should round-trip"));

	// --- float in (AS `float` is reflected as FDoubleProperty both for params and UPROPERTYs) ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeFloat")));
		Invoker.AddParam<double>(1.5);
		if (!Invoker.Call()) return false;
	}
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("LastFloat"), 1.5, TEXT("float in-param should round-trip (read as double)"));

	// --- int64 in ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeInt64")));
		Invoker.AddParam<int64>(static_cast<int64>(9223372036854775000LL));
		if (!Invoker.Call()) return false;
	}
	VerifyByPath<FInt64Property, int64>(Test, Actor, TEXT("LastInt64"),
		static_cast<int64>(9223372036854775000LL), TEXT("int64 in-param should round-trip"));

	// --- FString in ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeString")));
		Invoker.AddParam<FString>(FString(TEXT("PassedString")));
		if (!Invoker.Call()) return false;
	}
	VerifyByPath<FStrProperty, FString>(Test, Actor, TEXT("LastFString"), FString(TEXT("PassedString")),
		TEXT("FString in-param should round-trip"));

	// --- FVector in ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeVector")));
		Invoker.AddParam<FVector>(FVector(4.0, 5.0, 6.0));
		if (!Invoker.Call()) return false;
	}
	{
		FVector Actual = FVector::ZeroVector;
		if (!GetStructByPath<FVector>(Test, Actor, TEXT("LastStructIn"), Actual))
		{
			return false;
		}
		TestTrue(TEXT("FVector in-param should round-trip"), Actual.Equals(FVector(4.0, 5.0, 6.0)));
	}

	// --- AActor in (object reference) ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeActor")));
		Invoker.AddParam<AActor*>(Actor);
		if (!Invoker.Call()) return false;
	}
	{
		UObject* Ref = nullptr;
		if (!GetObjectByPath(Test, Actor, TEXT("LastObjectIn"), Ref))
		{
			return false;
		}
		TestEqual(TEXT("AActor in-param should round-trip"), Ref, static_cast<UObject*>(Actor));
	}

	// --- enum in (passed as the UEnum's underlying integer width) ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeEnum")));
		// ETemplateInvokeMode::Slow == 2. AS enums map to FEnumProperty with
		// FByteProperty underlying storage (width sizeof(uint8)).
		Invoker.AddParam<uint8>(2);
		if (!Invoker.Call()) return false;
	}
	{
		int64 EnumRaw = -1;
		if (!GetEnumByPath(Test, Actor, TEXT("LastEnum"), EnumRaw))
		{
			return false;
		}
		TestEqual(TEXT("enum in-param should round-trip"), EnumRaw, static_cast<int64>(2));
	}

	// --- TArray<int> in (container by value) ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeIntArray")));
		TArray<int32> Values = { 11, 22, 33 };
		Invoker.AddParam<TArray<int32>>(Values);
		if (!Invoker.Call()) return false;
	}
	{
		int32 Count = 0;
		if (!GetArrayNumByPath(Test, Actor, TEXT("LastArrayIn"), Count))
		{
			return false;
		}
		TestEqual(TEXT("TArray<int> in-param should report the right length"), Count, 3);
	}
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("LastArrayIn[0]"), 11, TEXT("TArray<int>[0] via in-param"));
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("LastArrayIn[2]"), 33, TEXT("TArray<int>[2] via in-param"));

	// --- FVector &out (out-ref round trip: the invoker exposes the mutated buffer) ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("FillVectorOut")));
		Invoker.AddParam<FVector>(FVector::ZeroVector);
		if (!Invoker.Call()) return false;

		FVector OutValue = FVector::ZeroVector;
		if (!Invoker.ReadParamAfterCall<FVector>(0, OutValue))
		{
			return false;
		}
		TestTrue(TEXT("FVector &out parameter should carry the AS-side assignment back to C++"),
			OutValue.Equals(FVector(7.0, 8.0, 9.0)));
	}

	// --- return int ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("Sum")));
		Invoker.AddParam<int32>(17);
		Invoker.AddParam<int32>(25);
		const int32 Result = Invoker.CallAndReturn<int32>(/*Fallback=*/INDEX_NONE);
		TestEqual(TEXT("int-returning UFUNCTION should expose the return value"), Result, 42);
	}

	// --- return FString ---
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("BuildLabel")));
		Invoker.AddParam<FString>(FString(TEXT("Enemy")));
		Invoker.AddParam<int32>(7);
		const FString Result = Invoker.CallAndReturn<FString>(FString());
		TestEqual(TEXT("FString-returning UFUNCTION should expose the return value"),
			Result, FString(TEXT("Enemy-7")));
	}

	}
	return true;
}

// =============================================================================
// Test 4: PathAccessExtendedTypes - extended UPROPERTY coverage
// =============================================================================
//
// Goes beyond the primitives tested in Test 2 to prove that the path-based
// helpers behave correctly against the wider UPROPERTY surface an AS project
// actually encounters in production: richer USTRUCTs (FVector2D/FIntPoint/
// FQuat/FTransform/FLinearColor/FColor/FGuid), `TObjectPtr<UActorComponent>`
// back-pointers to spawned components, `TSubclassOf<AActor>` class
// references, `TMap`, and `TSet`.
//
// For TMap/TSet the FPropertyBindingPath parser cannot index into the
// container, so the helpers stop at the *containing* property, and then
// FScriptMapHelper / FScriptSetHelper is used internally by
// GetMapValueByPath / SetContainsByPath / GetMapNumByPath / GetSetNumByPath.

	bool PathAccessExtendedTypes(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TemplateReflectionAccessExtendedTypes"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionAccessExtendedTypes.as"),
		TEXT(R"AS(
UCLASS()
class ATemplateReflectionExtendedTypesActor : AActor
{
	// ---- Richer C++-declared USTRUCTs ----
	UPROPERTY() FVector2D   Vector2DValue  = FVector2D(1.5, 2.5);
	UPROPERTY() FIntPoint   IntPointValue  = FIntPoint(7, 9);
	UPROPERTY() FQuat       QuatValue      = FQuat(0.0, 0.0, 0.0, 1.0);
	UPROPERTY() FTransform  TransformValue;
	UPROPERTY() FLinearColor LinearColorValue = FLinearColor(1.0, 0.5, 0.25, 1.0);
	UPROPERTY() FColor      ColorValue     = FColor(255, 128, 64, 255);
	UPROPERTY() FGuid       GuidValue;

	// ---- Component reference (TObjectPtr<UActorComponent>) and class reference ----
	// DefaultComponent auto-creates a USceneComponent on the actor at construction
	// time, so BeginPlay can rely on it. The SubClass field then captures the actor's
	// UClass via TSubclassOf<AActor>.
	UPROPERTY(DefaultComponent)
	USceneComponent SceneRoot;

	UPROPERTY()
	TSubclassOf<AActor> PawnSubclass;

	// ---- Map / set containers ----
	UPROPERTY() TMap<FString, int>   StringToIntMap;
	UPROPERTY() TMap<int, FString>   IntToStringMap;
	UPROPERTY() TSet<FName>          NameSet;

	// ---- TOptional ----
	// AS TOptional<T> reflects as FOptionalProperty. We keep one set and one unset
	// so the C++ side can exercise both code paths in GetOptionalIsSetByPath /
	// GetOptionalValueByPath.
	UPROPERTY() TOptional<int>     OptionalIntSet;
	UPROPERTY() TOptional<int>     OptionalIntUnset;
	UPROPERTY() TOptional<FString> OptionalStringSet;
	UPROPERTY() TOptional<FVector> OptionalVectorSet;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		// Rotation portion of transform (yaw 90, pitch 0, roll 0) and a non-zero
		// translation so the sub-struct path reads can tell the difference from
		// the default-constructed value.
		TransformValue = FTransform(FRotator(0.0, 90.0, 0.0), FVector(100.0, 200.0, 300.0), FVector::OneVector);

		// Deterministic GUID so the C++ assert can compare against a known value.
		GuidValue = FGuid(0x12345678, 0x9ABCDEF0, 0x11223344, 0x55667788);

		// Capture the actor's own class explicitly; property-style class lookup
		// is removed by refactor-as-remove-autoaccessor.
		PawnSubclass = GetClass();

		StringToIntMap.Add("Apple", 1);
		StringToIntMap.Add("Banana", 2);
		StringToIntMap.Add("Cherry", 3);

		IntToStringMap.Add(10, "Ten");
		IntToStringMap.Add(20, "Twenty");

		NameSet.Add(n"Alpha");
		NameSet.Add(n"Beta");
		NameSet.Add(n"Gamma");

		OptionalIntSet    = 999;
		// OptionalIntUnset is intentionally left unset.
		OptionalStringSet = "HelloOptional";
		OptionalVectorSet = FVector(-1.0, -2.0, -3.0);
	}
}
)AS"),
		TEXT("ATemplateReflectionExtendedTypesActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("Extended-types actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	// ---- FVector2D: both whole-struct read and sub-path read ----
	{
		FVector2D Actual = FVector2D::ZeroVector;
		if (!GetStructByPath<FVector2D>(Test, Actor, TEXT("Vector2DValue"), Actual))
		{
			return false;
		}
		TestTrue(TEXT("FVector2D UPROPERTY should equal its AS default as a whole struct"),
			Actual.Equals(FVector2D(1.5, 2.5)));
	}
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("Vector2DValue.X"), 1.5, TEXT("FVector2D.X via nested path"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("Vector2DValue.Y"), 2.5, TEXT("FVector2D.Y via nested path"));

	// ---- FIntPoint ----
	{
		FIntPoint Actual(0, 0);
		if (!GetStructByPath<FIntPoint>(Test, Actor, TEXT("IntPointValue"), Actual))
		{
			return false;
		}
		TestEqual(TEXT("FIntPoint UPROPERTY should equal its AS default"), Actual, FIntPoint(7, 9));
	}
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("IntPointValue.X"), 7, TEXT("FIntPoint.X via nested path"));
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("IntPointValue.Y"), 9, TEXT("FIntPoint.Y via nested path"));

	// ---- FQuat: identity quaternion ----
	{
		FQuat Actual = FQuat::Identity;
		if (!GetStructByPath<FQuat>(Test, Actor, TEXT("QuatValue"), Actual))
		{
			return false;
		}
		TestTrue(TEXT("FQuat UPROPERTY should equal the identity quaternion"),
			Actual.Equals(FQuat::Identity));
	}
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("QuatValue.W"), 1.0, TEXT("FQuat.W via nested path"));

	// ---- FTransform: sub-struct access through a longer path ("A.B.C") ----
	{
		FTransform Actual;
		if (!GetStructByPath<FTransform>(Test, Actor, TEXT("TransformValue"), Actual))
		{
			return false;
		}
		TestTrue(TEXT("FTransform UPROPERTY translation should match AS BeginPlay"),
			Actual.GetLocation().Equals(FVector(100.0, 200.0, 300.0)));
		TestTrue(TEXT("FTransform UPROPERTY scale should match AS BeginPlay"),
			Actual.GetScale3D().Equals(FVector::OneVector));
	}
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("TransformValue.Translation.X"), 100.0, TEXT("FTransform.Translation.X via nested path"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("TransformValue.Translation.Z"), 300.0, TEXT("FTransform.Translation.Z via nested path"));

	// ---- FLinearColor + FColor ----
	{
		FLinearColor Actual;
		if (!GetStructByPath<FLinearColor>(Test, Actor, TEXT("LinearColorValue"), Actual))
		{
			return false;
		}
		TestTrue(TEXT("FLinearColor UPROPERTY should equal its AS default"),
			Actual.Equals(FLinearColor(1.0f, 0.5f, 0.25f, 1.0f)));
	}
	VerifyByPath<FFloatProperty, float>(Test, Actor, TEXT("LinearColorValue.R"), 1.0f, TEXT("FLinearColor.R via nested path"));
	VerifyByPath<FFloatProperty, float>(Test, Actor, TEXT("LinearColorValue.G"), 0.5f, TEXT("FLinearColor.G via nested path"));

	{
		FColor Actual;
		if (!GetStructByPath<FColor>(Test, Actor, TEXT("ColorValue"), Actual))
		{
			return false;
		}
		TestEqual(TEXT("FColor UPROPERTY should equal its AS default"), Actual, FColor(255, 128, 64, 255));
	}

	// ---- FGuid: a 4-part USTRUCT, read each quarter via a nested path ----
	{
		FGuid Actual;
		if (!GetStructByPath<FGuid>(Test, Actor, TEXT("GuidValue"), Actual))
		{
			return false;
		}
		const FGuid Expected(0x12345678u, 0x9ABCDEF0u, 0x11223344u, 0x55667788u);
		TestEqual(TEXT("FGuid UPROPERTY should equal its AS default as a whole struct"), Actual, Expected);
	}

	// ---- Component reference (DefaultComponent) ----
	{
		UObject* Component = nullptr;
		if (!GetObjectByPath(Test, Actor, TEXT("SceneRoot"), Component))
		{
			return false;
		}
		TestNotNull(TEXT("DefaultComponent USceneComponent UPROPERTY should be spawned on the actor"), Component);
		TestTrue(TEXT("DefaultComponent should be a USceneComponent"), Component != nullptr && Component->IsA<USceneComponent>());
		if (USceneComponent* AsScene = Cast<USceneComponent>(Component))
		{
			TestEqual(TEXT("DefaultComponent outer should be the owning actor"),
				AsScene->GetOwner(), static_cast<AActor*>(Actor));
		}
	}

	// ---- TSubclassOf<AActor> ----
	{
		UClass* AsClass = nullptr;
		if (!GetClassByPath(Test, Actor, TEXT("PawnSubclass"), AsClass))
		{
			return false;
		}
		TestEqual(TEXT("TSubclassOf<AActor> assigned from GetClass() should equal the actor's class"),
			AsClass, Actor->GetClass());
	}

	// ---- TMap<FString, int> ----
	{
		int32 Count = 0;
		if (!GetMapNumByPath(Test, Actor, TEXT("StringToIntMap"), Count))
		{
			return false;
		}
		TestEqual(TEXT("TMap<FString,int> should report three entries"), Count, 3);
	}
	{
		int32 Banana = 0;
		if (!GetMapValueByPath<FString, FIntProperty, int32>(
				Test, Actor, TEXT("StringToIntMap"), FString(TEXT("Banana")), Banana))
		{
			return false;
		}
		TestEqual(TEXT("TMap<FString,int>[\"Banana\"] should be 2"), Banana, 2);
	}
	{
		int32 Cherry = 0;
		if (!GetMapValueByPath<FString, FIntProperty, int32>(
				Test, Actor, TEXT("StringToIntMap"), FString(TEXT("Cherry")), Cherry))
		{
			return false;
		}
		TestEqual(TEXT("TMap<FString,int>[\"Cherry\"] should be 3"), Cherry, 3);
	}

	// ---- TMap<int, FString> (int-keyed map) ----
	{
		FString Twenty;
		if (!GetMapValueByPath<int32, FStrProperty, FString>(
				Test, Actor, TEXT("IntToStringMap"), 20, Twenty))
		{
			return false;
		}
		TestEqual(TEXT("TMap<int,FString>[20] should be \"Twenty\""),
			Twenty, FString(TEXT("Twenty")));
	}

	// ---- TSet<FName> ----
	{
		int32 Count = 0;
		if (!GetSetNumByPath(Test, Actor, TEXT("NameSet"), Count))
		{
			return false;
		}
		TestEqual(TEXT("TSet<FName> should report three entries"), Count, 3);
	}
	TestTrue(TEXT("TSet<FName> should contain Alpha"),
		SetContainsByPath<FName>(Test, Actor, TEXT("NameSet"), FName(TEXT("Alpha"))));
	TestTrue(TEXT("TSet<FName> should contain Gamma"),
		SetContainsByPath<FName>(Test, Actor, TEXT("NameSet"), FName(TEXT("Gamma"))));
	TestFalse(TEXT("TSet<FName> should not contain an unrelated name"),
		SetContainsByPath<FName>(Test, Actor, TEXT("NameSet"), FName(TEXT("DefinitelyNotInTheSet"))));

	// ---- TOptional<int> - set vs. unset state round-trip ----
	{
		bool bIsSet = false;
		if (!GetOptionalIsSetByPath(Test, Actor, TEXT("OptionalIntSet"), bIsSet))
		{
			return false;
		}
		TestTrue(TEXT("TOptional<int> assigned in AS should report IsSet=true"), bIsSet);

		int32 Inner = 0;
		if (!GetOptionalValueByPath<FIntProperty, int32>(Test, Actor, TEXT("OptionalIntSet"), Inner))
		{
			return false;
		}
		TestEqual(TEXT("TOptional<int> inner value should equal the AS-side assignment"), Inner, 999);
	}
	{
		bool bIsSet = true;
		if (!GetOptionalIsSetByPath(Test, Actor, TEXT("OptionalIntUnset"), bIsSet))
		{
			return false;
		}
		TestFalse(TEXT("TOptional<int> left unset in AS should report IsSet=false"), bIsSet);
	}

	// ---- TOptional<FString> ----
	{
		FString Inner;
		if (!GetOptionalValueByPath<FStrProperty, FString>(Test, Actor, TEXT("OptionalStringSet"), Inner))
		{
			return false;
		}
		TestEqual(TEXT("TOptional<FString> inner value should equal the AS-side assignment"),
			Inner, FString(TEXT("HelloOptional")));
	}

	// ---- TOptional<FVector> via dedicated struct helper ----
	{
		FVector Inner = FVector::ZeroVector;
		if (!GetOptionalStructByPath<FVector>(Test, Actor, TEXT("OptionalVectorSet"), Inner))
		{
			return false;
		}
		TestTrue(TEXT("TOptional<FVector> inner value should equal the AS-side assignment"),
			Inner.Equals(FVector(-1.0, -2.0, -3.0)));
	}

	}
	return true;
}

// =============================================================================
// Test 5: FunctionInvokeExtendedTypes - extended UFUNCTION coverage
// =============================================================================
//
// Ensures the invoker handles UFUNCTIONs that accept and return the extended
// types: FVector2D / FIntPoint / FQuat / FTransform / FLinearColor / FColor /
// FGuid, `USceneComponent*`, `TSubclassOf<AActor>`, TMap / TSet containers, and
// out-reference variants of those types.

	bool FunctionInvokeExtendedTypes(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TemplateReflectionAccessInvokeExtendedTypes"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionAccessInvokeExtendedTypes.as"),
		TEXT(R"AS(
UCLASS()
class ATemplateReflectionInvokeExtendedTypesActor : AActor
{
	UPROPERTY(DefaultComponent)
	USceneComponent SceneRoot;

	// Result slots.
	UPROPERTY() FVector2D   LastVector2D   = FVector2D::ZeroVector;
	UPROPERTY() FIntPoint   LastIntPoint   = FIntPoint(0, 0);
	UPROPERTY() FQuat       LastQuat       = FQuat::Identity;
	UPROPERTY() FTransform  LastTransform;
	UPROPERTY() FLinearColor LastLinearColor = FLinearColor::Black;
	UPROPERTY() FColor      LastColor      = FColor(0, 0, 0, 0);
	UPROPERTY() FGuid       LastGuid;
	UPROPERTY() USceneComponent LastComponent = nullptr;
	UPROPERTY() TSubclassOf<AActor> LastSubclass;
	UPROPERTY() TMap<FString, int>  LastMap;
	UPROPERTY() TSet<FName>         LastSet;

	UFUNCTION() void TakeVector2D(FVector2D Value)   { LastVector2D = Value; }
	UFUNCTION() void TakeIntPoint(FIntPoint Value)   { LastIntPoint = Value; }
	UFUNCTION() void TakeQuat(FQuat Value)           { LastQuat = Value; }
	UFUNCTION() void TakeTransform(FTransform Value) { LastTransform = Value; }
	UFUNCTION() void TakeLinearColor(FLinearColor Value) { LastLinearColor = Value; }
	UFUNCTION() void TakeColor(FColor Value)         { LastColor = Value; }
	UFUNCTION() void TakeGuid(FGuid Value)           { LastGuid = Value; }

	UFUNCTION() void TakeComponent(USceneComponent Value) { LastComponent = Value; }
	UFUNCTION() void TakeSubclass(TSubclassOf<AActor> Value) { LastSubclass = Value; }

	UFUNCTION() void TakeStringIntMap(TMap<FString, int> Value) { LastMap = Value; }
	UFUNCTION() void TakeNameSet(TSet<FName> Value)             { LastSet = Value; }

	// NOTE: AS does not allow TOptional<T> as a UFUNCTION parameter - the
	// ClassGenerator rejects it via
	// "Unknown or invalid parameter type for parameter" during compile.
	// TOptional is therefore covered only at the UPROPERTY layer (see the
	// PathAccessExtendedTypes test).

	// Return-values covering each major category.
	UFUNCTION() FVector2D MakeVector2D(float X, float Y) { return FVector2D(X, Y); }
	UFUNCTION() FTransform MakeTransform(FVector Location) {
		return FTransform(FRotator::ZeroRotator, Location, FVector::OneVector);
	}
	UFUNCTION() USceneComponent GetOwnedScene() { return SceneRoot; }

	// Out-ref over an extended struct.
	UFUNCTION() void FillTransformOut(FTransform& OutValue) {
		OutValue = FTransform(FRotator::ZeroRotator, FVector(11.0, 22.0, 33.0), FVector::OneVector);
	}
}
)AS"),
		TEXT("ATemplateReflectionInvokeExtendedTypesActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("Invoke-extended-types actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	// ---- FVector2D in-param ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeVector2D")));
		Invoker.AddParam<FVector2D>(FVector2D(3.5, 4.5));
		if (!Invoker.Call()) return false;
	}
	{
		FVector2D Actual = FVector2D::ZeroVector;
		if (!GetStructByPath<FVector2D>(Test, Actor, TEXT("LastVector2D"), Actual)) return false;
		TestTrue(TEXT("FVector2D in-param should round-trip"), Actual.Equals(FVector2D(3.5, 4.5)));
	}

	// ---- FIntPoint in-param ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeIntPoint")));
		Invoker.AddParam<FIntPoint>(FIntPoint(-3, 42));
		if (!Invoker.Call()) return false;
	}
	{
		FIntPoint Actual(0, 0);
		if (!GetStructByPath<FIntPoint>(Test, Actor, TEXT("LastIntPoint"), Actual)) return false;
		TestEqual(TEXT("FIntPoint in-param should round-trip"), Actual, FIntPoint(-3, 42));
	}

	// ---- FQuat in-param ----
	{
		const FQuat Expected = FQuat(FVector(0.0, 0.0, 1.0), UE_DOUBLE_HALF_PI).GetNormalized();
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeQuat")));
		Invoker.AddParam<FQuat>(Expected);
		if (!Invoker.Call()) return false;
		FQuat Actual = FQuat::Identity;
		if (!GetStructByPath<FQuat>(Test, Actor, TEXT("LastQuat"), Actual)) return false;
		TestTrue(TEXT("FQuat in-param should round-trip"), Actual.Equals(Expected));
	}

	// ---- FTransform in-param ----
	{
		const FTransform Expected(FRotator::ZeroRotator, FVector(5.0, -5.0, 50.0), FVector::OneVector);
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeTransform")));
		Invoker.AddParam<FTransform>(Expected);
		if (!Invoker.Call()) return false;
		FTransform Actual;
		if (!GetStructByPath<FTransform>(Test, Actor, TEXT("LastTransform"), Actual)) return false;
		TestTrue(TEXT("FTransform in-param translation should round-trip"),
			Actual.GetLocation().Equals(Expected.GetLocation()));
	}

	// ---- FLinearColor / FColor in-params ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeLinearColor")));
		Invoker.AddParam<FLinearColor>(FLinearColor(0.1f, 0.2f, 0.3f, 0.4f));
		if (!Invoker.Call()) return false;
		FLinearColor Actual;
		if (!GetStructByPath<FLinearColor>(Test, Actor, TEXT("LastLinearColor"), Actual)) return false;
		TestTrue(TEXT("FLinearColor in-param should round-trip"),
			Actual.Equals(FLinearColor(0.1f, 0.2f, 0.3f, 0.4f)));
	}
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeColor")));
		Invoker.AddParam<FColor>(FColor(10, 20, 30, 40));
		if (!Invoker.Call()) return false;
		FColor Actual;
		if (!GetStructByPath<FColor>(Test, Actor, TEXT("LastColor"), Actual)) return false;
		TestEqual(TEXT("FColor in-param should round-trip"), Actual, FColor(10, 20, 30, 40));
	}

	// ---- FGuid in-param ----
	{
		const FGuid Expected(0xDEADBEEFu, 0xCAFEBABEu, 0x01234567u, 0x89ABCDEFu);
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeGuid")));
		Invoker.AddParam<FGuid>(Expected);
		if (!Invoker.Call()) return false;
		FGuid Actual;
		if (!GetStructByPath<FGuid>(Test, Actor, TEXT("LastGuid"), Actual)) return false;
		TestEqual(TEXT("FGuid in-param should round-trip"), Actual, Expected);
	}

	// ---- USceneComponent in-param (TObjectPtr-backed component reference) ----
	USceneComponent* SceneRootFromProperty = nullptr;
	{
		UObject* AsObject = nullptr;
		if (!GetObjectByPath(Test, Actor, TEXT("SceneRoot"), AsObject)) return false;
		SceneRootFromProperty = Cast<USceneComponent>(AsObject);
		if (!TestNotNull(TEXT("DefaultComponent SceneRoot should be live after BeginPlay"), SceneRootFromProperty))
		{
			return false;
		}

		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeComponent")));
		Invoker.AddParam<USceneComponent*>(SceneRootFromProperty);
		if (!Invoker.Call()) return false;
	}
	{
		UObject* Actual = nullptr;
		if (!GetObjectByPath(Test, Actor, TEXT("LastComponent"), Actual)) return false;
		TestEqual(TEXT("USceneComponent in-param should round-trip via TObjectPtr UPROPERTY"),
			Actual, static_cast<UObject*>(SceneRootFromProperty));
	}

	// ---- TSubclassOf<AActor> in-param ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeSubclass")));
		Invoker.AddParam<UClass*>(Actor->GetClass());
		if (!Invoker.Call()) return false;

		UClass* Actual = nullptr;
		if (!GetClassByPath(Test, Actor, TEXT("LastSubclass"), Actual)) return false;
		TestEqual(TEXT("TSubclassOf<AActor> in-param should round-trip"),
			Actual, Actor->GetClass());
	}

	// ---- TMap in-param ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeStringIntMap")));
		TMap<FString, int32> Seed;
		Seed.Add(TEXT("One"), 1);
		Seed.Add(TEXT("Two"), 2);
		Invoker.AddParam<TMap<FString, int32>>(Seed);
		if (!Invoker.Call()) return false;

		int32 Count = 0;
		if (!GetMapNumByPath(Test, Actor, TEXT("LastMap"), Count)) return false;
		TestEqual(TEXT("TMap in-param should round-trip the entry count"), Count, 2);

		int32 Value = 0;
		if (!GetMapValueByPath<FString, FIntProperty, int32>(
				Test, Actor, TEXT("LastMap"), FString(TEXT("Two")), Value)) return false;
		TestEqual(TEXT("TMap in-param should round-trip the values"), Value, 2);
	}

	// ---- TSet in-param ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeNameSet")));
		TSet<FName> Seed;
		Seed.Add(FName(TEXT("Delta")));
		Seed.Add(FName(TEXT("Epsilon")));
		Invoker.AddParam<TSet<FName>>(Seed);
		if (!Invoker.Call()) return false;

		int32 Count = 0;
		if (!GetSetNumByPath(Test, Actor, TEXT("LastSet"), Count)) return false;
		TestEqual(TEXT("TSet in-param should round-trip the entry count"), Count, 2);
		TestTrue(TEXT("TSet in-param should round-trip membership"),
			SetContainsByPath<FName>(Test, Actor, TEXT("LastSet"), FName(TEXT("Epsilon"))));
	}

	// ---- Return FVector2D ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("MakeVector2D")));
		// MakeVector2D's parameters are AS `float`s, which the UFunction reflects as
		// FDoubleProperty (same rule as the core primitive matrix test).
		Invoker.AddParam<double>(1.25);
		Invoker.AddParam<double>(-2.75);
		const FVector2D Result = Invoker.CallAndReturn<FVector2D>(FVector2D::ZeroVector);
		TestTrue(TEXT("FVector2D-returning UFUNCTION should expose the return value"),
			Result.Equals(FVector2D(1.25, -2.75)));
	}

	// ---- Return FTransform ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("MakeTransform")));
		Invoker.AddParam<FVector>(FVector(7.0, 8.0, 9.0));
		const FTransform Result = Invoker.CallAndReturn<FTransform>(FTransform::Identity);
		TestTrue(TEXT("FTransform-returning UFUNCTION should carry the assigned translation back to C++"),
			Result.GetLocation().Equals(FVector(7.0, 8.0, 9.0)));
	}

	// ---- Return USceneComponent (component reference as return value) ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("GetOwnedScene")));
		USceneComponent* Result = Invoker.CallAndReturn<USceneComponent*>(nullptr);
		TestEqual(TEXT("USceneComponent-returning UFUNCTION should return the SceneRoot"),
			static_cast<UObject*>(Result), static_cast<UObject*>(SceneRootFromProperty));
	}

	// ---- FTransform &out ----
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("FillTransformOut")));
		Invoker.AddParam<FTransform>(FTransform::Identity);
		if (!Invoker.Call()) return false;

		FTransform Out;
		if (!Invoker.ReadParamAfterCall<FTransform>(0, Out)) return false;
		TestTrue(TEXT("FTransform &out parameter should carry the AS-side translation back"),
			Out.GetLocation().Equals(FVector(11.0, 22.0, 33.0)));
	}

	// (TOptional<T> is intentionally not covered as a UFUNCTION parameter here -
	// AS rejects it with "Unknown or invalid parameter type". See PathAccessExtendedTypes
	// for TOptional UPROPERTY coverage instead.)

	}
	return true;
}

// =============================================================================
// Test 6: PathAccessReferenceTypes - FText / soft / weak / FInstancedStruct
// =============================================================================
//
// Covers the reference-flavoured UPROPERTY types an AS project uses to talk to
// editor-facing data: localized text, soft/weak object & class pointers, and
// FInstancedStruct property shape. Every non-empty value is written from AS and
// read back from C++ through its dedicated helper. Where applicable we also
// flow the type through a UFUNCTION parameter to confirm the invoker handles it.

	bool PathAccessReferenceTypes(FAutomationTestBase& Test)
{
	// Reference type tests use IsolatedFull so script-defined classes and their
	// type metadata stay alive for the whole actor lifetime.
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	static const FName ModuleName(TEXT("TemplateReflectionAccessReferenceTypes"));
	// ASTEST_CREATE_ENGINE_FULL auto-discards all modules on scope exit, so no explicit
	// ON_SCOPE_EXIT discard is needed here.

	// Pre-resolve a known FSoftObjectPath target so the AS side can refer to it
	// as a string literal without depending on external assets.
	const FSoftObjectPath KnownSoftPath(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));

	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionAccessReferenceTypes.as"),
		TEXT(R"AS(
// NOTE on FInstancedStruct:
//   Wrapping an AS-declared USTRUCT into FInstancedStruct triggers a crash
//   inside FASStructOps::Construct during BeginPlay (the AS struct's
//   FASStructOps is not live at the point UScriptStruct::InitializeStruct
//   walks into it). Wrapping a raw C++ USTRUCT like FVector throws
//   "Not a valid USTRUCT" because AS registers math types without a
//   UScriptStruct peer for the AS TypeId <-> UScriptStruct lookup.
//   We therefore omit FInstancedStruct from this test matrix - leave it to a
//   targeted test that exercises AS FInstancedStruct through AS-only helpers.
UCLASS()
class ATemplateReflectionReferenceTypesActor : AActor
{
	// ---- Reference / localization-flavoured UPROPERTYs ----
	UPROPERTY() FText                    TextValue;
	UPROPERTY() TSoftObjectPtr<UObject>  SoftObjectRef;
	UPROPERTY() TSoftClassPtr<AActor>    SoftClassRef;
	UPROPERTY() TWeakObjectPtr<AActor>   WeakActorRef;

	// Mirror slots used by the UFUNCTION round-trip at the end.
	UPROPERTY() FText                    LastText;
	UPROPERTY() TSoftObjectPtr<UObject>  LastSoftRef;
	UPROPERTY() TWeakObjectPtr<AActor>   LastWeak;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		TextValue = FText::FromString("Localized via AS");

		// Construct a soft path from a well-known engine asset so the C++ side
		// can compare against a fixed string.
		SoftObjectRef = TSoftObjectPtr<UObject>(FSoftObjectPath("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		SoftClassRef  = TSoftClassPtr<AActor>(FSoftObjectPath("/Script/Engine.Actor"));

		// Weak back-reference to this actor.
		WeakActorRef = this;
	}

	UFUNCTION() void TakeText(FText Value)                        { LastText = Value; }
	UFUNCTION() void TakeSoftObject(TSoftObjectPtr<UObject> Ref)  { LastSoftRef = Ref; }
	UFUNCTION() void TakeWeakActor(TWeakObjectPtr<AActor> Ref)    { LastWeak = Ref; }

	UFUNCTION() FText MakeLabel(FString Prefix, int Counter)
	{
		return FText::FromString(f"{Prefix}:{Counter}");
	}
}
)AS"),
		TEXT("ATemplateReflectionReferenceTypesActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("Reference-types actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	// ---- FText ----
	{
		FText Actual;
		if (!GetTextByPath(Test, Actor, TEXT("TextValue"), Actual))
		{
			return false;
		}
		TestEqual(TEXT("FText UPROPERTY should round-trip its AS assignment"),
			Actual.ToString(), FString(TEXT("Localized via AS")));
	}

	// ---- TSoftObjectPtr ----
	{
		FSoftObjectPath Actual;
		if (!GetSoftObjectPathByPath(Test, Actor, TEXT("SoftObjectRef"), Actual))
		{
			return false;
		}
		TestEqual(TEXT("TSoftObjectPtr<UObject> UPROPERTY should round-trip its AS path"),
			Actual, KnownSoftPath);
	}

	// ---- TSoftClassPtr ----
	{
		FSoftObjectPath Actual;
		if (!GetSoftClassPathByPath(Test, Actor, TEXT("SoftClassRef"), Actual))
		{
			return false;
		}
		TestEqual(TEXT("TSoftClassPtr<AActor> UPROPERTY should round-trip its AS path"),
			Actual, FSoftObjectPath(TEXT("/Script/Engine.Actor")));
	}

	// ---- TWeakObjectPtr ----
	{
		UObject* Actual = nullptr;
		if (!GetWeakObjectByPath(Test, Actor, TEXT("WeakActorRef"), Actual))
		{
			return false;
		}
		TestEqual(TEXT("TWeakObjectPtr<AActor> UPROPERTY should resolve to the actor"),
			Actual, static_cast<UObject*>(Actor));
	}

	// ---- FInstancedStruct is intentionally not covered here - see the note on
	//       the AS class above for why; the AS-engine-side struct lifecycle is
	//       fragile when FInstancedStruct crosses the script/native boundary.

	// ---- UFUNCTION round-trips ----

	// TakeText(FText)
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeText")));
		Invoker.AddParam<FText>(FText::FromString(TEXT("FromCpp")));
		if (!Invoker.Call()) return false;
		FText Actual;
		if (!GetTextByPath(Test, Actor, TEXT("LastText"), Actual)) return false;
		TestEqual(TEXT("FText in-param should round-trip"), Actual.ToString(), FString(TEXT("FromCpp")));
	}

	// TakeSoftObject(TSoftObjectPtr<UObject>) - pass in a soft ptr built from the known path.
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeSoftObject")));
		TSoftObjectPtr<UObject> SoftIn(KnownSoftPath);
		Invoker.AddParam<TSoftObjectPtr<UObject>>(SoftIn);
		if (!Invoker.Call()) return false;

		FSoftObjectPath Actual;
		if (!GetSoftObjectPathByPath(Test, Actor, TEXT("LastSoftRef"), Actual)) return false;
		TestEqual(TEXT("TSoftObjectPtr in-param should round-trip"), Actual, KnownSoftPath);
	}

	// TakeWeakActor(TWeakObjectPtr<AActor>) - pass the actor itself.
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("TakeWeakActor")));
		TWeakObjectPtr<AActor> WeakIn(Actor);
		Invoker.AddParam<TWeakObjectPtr<AActor>>(WeakIn);
		if (!Invoker.Call()) return false;

		UObject* Resolved = nullptr;
		if (!GetWeakObjectByPath(Test, Actor, TEXT("LastWeak"), Resolved)) return false;
		TestEqual(TEXT("TWeakObjectPtr in-param should round-trip"),
			Resolved, static_cast<UObject*>(Actor));
	}

	// Return FText
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("MakeLabel")));
		Invoker.AddParam<FString>(FString(TEXT("Mob")));
		Invoker.AddParam<int32>(11);
		const FText Result = Invoker.CallAndReturn<FText>(FText::GetEmpty());
		TestEqual(TEXT("FText-returning UFUNCTION should expose the return value"),
			Result.ToString(), FString(TEXT("Mob:11")));
	}

	}
	return true;
}

// =============================================================================
// Test 7: PathAccessScriptDefinedTypes - AS-declared USTRUCT / UCLASS refs
// =============================================================================
//
// Earlier tests only use C++-declared USTRUCTs (FVector, FTransform, ...). This
// test proves that the same helpers work against AS-*defined* USTRUCTs and
// AS-*defined* UCLASS references used as UPROPERTY fields or UFUNCTION params.
//
// This test case is historically fragile in SHARE_CLEAN mode because the
// engine's GC can outlive the shared AS engine's type table when AS structs
// are in play. The test therefore uses an IsolatedFull engine to keep the AS
// engine alive for the duration of the spawned actor's lifetime.

	bool PathAccessScriptDefinedTypes(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	static const FName ModuleName(TEXT("TemplateReflectionAccessScriptDefined"));

	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionAccessScriptDefined.as"),
		TEXT(R"AS(
// AS-defined USTRUCT used as both a UPROPERTY and a UFUNCTION parameter.
struct FTemplateScriptDefinedStats
{
	UPROPERTY() int Power = 10;
	UPROPERTY() FString Label = "Novice";
}

// AS-defined UCLASS referenced from another AS-defined UCLASS.
UCLASS()
class ATemplateScriptDefinedPartner : AActor
{
	UPROPERTY() int PartnerValue = 77;
	UPROPERTY() FString PartnerName = "Sidekick";
}

UCLASS()
class ATemplateScriptDefinedOwner : AActor
{
	UPROPERTY() FTemplateScriptDefinedStats Stats;
	UPROPERTY() ATemplateScriptDefinedPartner Partner = nullptr;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		Stats.Power = 42;
		Stats.Label = "VIP";

		// Spawn a partner AS actor and keep a reference; the C++ test reads both
		// the reference and the partner's UPROPERTYs via path resolution.
		Partner = Cast<ATemplateScriptDefinedPartner>(SpawnActor(ATemplateScriptDefinedPartner::StaticClass()));
		if (Partner != nullptr)
		{
			Partner.PartnerValue = 123;
			Partner.PartnerName = "Bound";
		}
	}

	// AS-declared USTRUCT as a UFUNCTION parameter is intentionally omitted: the
	// FStructOnScope destructor on the C++ side routes into AngelScript's object
	// destructor and crashes when the slot was populated through raw FProperty
	// writes instead of AngelScript's own construction path.
}
)AS"),
		TEXT("ATemplateScriptDefinedOwner"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("Script-defined-types actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	// ---- AS USTRUCT as UPROPERTY: sub-field path access ----
	VerifyByPath<FIntProperty, int32>(Test, Actor, TEXT("Stats.Power"), 42,
		TEXT("AS-USTRUCT UPROPERTY int sub-field should resolve through a nested path"));
	VerifyByPath<FStrProperty, FString>(Test, Actor, TEXT("Stats.Label"), FString(TEXT("VIP")),
		TEXT("AS-USTRUCT UPROPERTY FString sub-field should resolve through a nested path"));

	// ---- AS UCLASS reference as UPROPERTY ----
	UObject* Partner = nullptr;
	if (!GetObjectByPath(Test, Actor, TEXT("Partner"), Partner))
	{
		return false;
	}
	if (!TestNotNull(TEXT("AS-UCLASS UPROPERTY reference should be live after BeginPlay"), Partner))
	{
		return false;
	}

	// Reading a UPROPERTY on the *partner* object is the same helper, just with a
	// different target - no special casing needed because the partner is also a
	// UObject generated by the AS class system.
	VerifyByPath<FIntProperty, int32>(Test, Partner, TEXT("PartnerValue"), 123,
		TEXT("Partner AS-UCLASS UPROPERTY int should read back the AS-side assignment"));
	VerifyByPath<FStrProperty, FString>(Test, Partner, TEXT("PartnerName"), FString(TEXT("Bound")),
		TEXT("Partner AS-UCLASS UPROPERTY FString should read back the AS-side assignment"));

	// ---- AS UFUNCTION taking an AS USTRUCT ----
	//
	// Passing an AS-declared USTRUCT as a UFUNCTION parameter from C++ is not
	// covered here: the struct's destructor calls into the AngelScript object
	// destructor through UScriptStruct::DestroyStruct, and populating the slot
	// without going through AngelScript's own construction path leaves state
	// the AS destructor can't handle (it crashes in asIScriptObject::CallDestructor).
	// Tests that need this flow should construct the struct value inside AS
	// (e.g. via a helper UFUNCTION returning the struct) rather than building
	// one from raw FProperty writes on the C++ side.

	}
	return true;
}

// =============================================================================
// Test 8: PathParserLimits - document path-parser boundaries
// =============================================================================
//
// `FPropertyBindingPath::FromString` only understands "Name" and "Name[int]"
// segments - TMap keys, TSet lookups, and chained `Name[i][j]` sub-indices
// return a parse failure. We capture that contract here so any future upgrade
// of PropertyBindingUtils that changes this has a red test to signal the
// behaviour change.

	bool PathParserLimits(FAutomationTestBase& Test)
{
	// -----------------------------------------------------------------------
	// Parse-level behaviour
	// -----------------------------------------------------------------------
	//
	// The parser in FPropertyBindingPath::FromString is intentionally permissive:
	//   * `Name[nonsense]` parses (LexFromString(int32, "nonsense") = 0, which is
	//     still a valid-looking array index to the parser).
	//   * `Name[0][1]` parses because it only looks for the first `[` and the
	//     last `]` and lex-parses whatever is between them.
	// The *resolve* step (ResolveIndirectionsWithValue) is what actually rejects
	// these - that's where type-mismatch / index-out-of-range diagnostics surface.
	//
	// This test captures the split: parser is lenient, resolution is strict.

	// Case 1 - trailing dot is still rejected at parse time.
	{
		FPropertyBindingPath Path;
		TestFalse(TEXT("Trailing dot should NOT parse"),
			Path.FromString(TEXT("Foo.Bar.")));
	}

	// Case 2 - only-index segment "[0].Field" is rejected at parse time
	// (because the bracket has no name in front of it).
	{
		FPropertyBindingPath Path;
		TestFalse(TEXT("\"[0].Field\" (no name before the bracket) should NOT parse"),
			Path.FromString(TEXT("[0].Field")));
	}

	// Case 3 - empty brackets are rejected at parse time.
	{
		FPropertyBindingPath Path;
		TestFalse(TEXT("Empty brackets should NOT parse"),
			Path.FromString(TEXT("Foo[]")));
	}

	// Case 4 - happy path anchors: normal single/nested/indexed paths parse.
	{
		FPropertyBindingPath Path;
		TestTrue(TEXT("\"Foo.Bar[2].Baz\" should parse"),
			Path.FromString(TEXT("Foo.Bar[2].Baz")));
		TestEqual(TEXT("\"Foo.Bar[2].Baz\" should produce 3 segments"),
			Path.NumSegments(), 3);
	}

	// -----------------------------------------------------------------------
	// Resolve-level behaviour - the parser is permissive, so these paths
	// parse clean but fail at ResolveIndirectionsWithValue time against an
	// actual object. We build a minimal scratch actor with a TMap and a
	// TArray<TArray<int>>-ish shape to exercise those rejections.
	// -----------------------------------------------------------------------
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TemplateReflectionAccessPathLimits"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionAccessPathLimits.as"),
		TEXT(R"AS(
UCLASS()
class ATemplateReflectionPathLimitsActor : AActor
{
	UPROPERTY() TMap<FString, int> StringMap;
	UPROPERTY() TArray<int>        IntArray;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		StringMap.Add("Apple", 1);
		IntArray.Add(10);
		IntArray.Add(20);
	}
}
)AS"),
		TEXT("ATemplateReflectionPathLimitsActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("Path-limits scratch actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	auto TryResolve = [Actor](FStringView PathString, FString& OutError) -> bool
	{
		FPropertyBindingPath Path;
		if (!Path.FromString(PathString))
		{
			OutError = TEXT("parse failure");
			return false;
		}
		TArray<FPropertyBindingPathIndirection> Indirections;
		return Path.ResolveIndirectionsWithValue(
			FPropertyBindingDataView(Actor->GetClass(), Actor),
			Indirections,
			&OutError);
	};

	// TMap string-keyed "Name[\"Key\"]":
	// The parser lex-converts the content between the first `[` and the last
	// `]` as an int32, yielding 0 for `"Apple"`. Resolution then silently
	// succeeds - FPropertyBindingPath treats TMap as "ok" for index 0 rather
	// than failing outright, so the read returns whatever the first entry of
	// the map happens to be. This is a hazard: the path *parses* and
	// *resolves* cleanly but does NOT actually honour string keys. Use
	// GetMapValueByPath / SetContainsByPath for TMap/TSet access.
	{
		FString ResolveError;
		TestTrue(TEXT("\"StringMap[\\\"Apple\\\"]\" silently resolves to the first TMap entry - string keys are ignored"),
			TryResolve(TEXT("StringMap[\"Apple\"]"), ResolveError));
	}

	// Double-index on a single segment: "IntArray[0][0]" parses into ONE
	// segment with array-index 0 (the inner "0][0" lexes to 0). Because the
	// parser cannot produce a second container indirection, the path silently
	// collapses to "IntArray[0]". This is the "mystery silent read" hazard
	// the test was meant to flag - we capture it here so the behaviour is
	// explicit. If PropertyBindingUtils ever adds multi-index parsing, flip
	// this to TestFalse.
	{
		FString ResolveError;
		TestTrue(TEXT("\"IntArray[0][0]\" silently resolves to IntArray[0] - the trailing brackets are ignored"),
			TryResolve(TEXT("IntArray[0][0]"), ResolveError));
	}

	// Out-of-range index: "IntArray[999]" - parses but must fail to resolve.
	{
		FString ResolveError;
		TestFalse(TEXT("\"IntArray[999]\" should fail to resolve (index out of range)"),
			TryResolve(TEXT("IntArray[999]"), ResolveError));
	}

	// Missing property name: "NotThere.SubField" - parses but must fail to resolve.
	{
		FString ResolveError;
		TestFalse(TEXT("\"NotThere.SubField\" should fail to resolve (unknown root property)"),
			TryResolve(TEXT("NotThere.SubField"), ResolveError));
	}

	}
	return true;
}

// =============================================================================
// Test 9: BPLibraryViaASGlobalWrapper
// =============================================================================
//
// UBlueprintFunctionLibrary statics (the `Math::*`, `Gameplay::*`, `System::*`
// families, etc.) are bound into AngelScript as *engine-level namespaced
// globals* (see Bind_BlueprintCallable.cpp :: `FAngelscriptBinds::FNamespace`
// + `BindGlobalFunction`). They do NOT appear in any asIScriptModule's function
// table, so `Module->GetFunctionByDecl("float Math::Abs(float)")` returns
// null — they can only be called from *within* an AS script.
//
// The idiomatic pattern is therefore:
//
//   1. Declare an AS module-level wrapper that forwards to the BP static.
//   2. Invoke the wrapper from C++ through FASGlobalFunctionInvoker.
//
// For tests that do NOT need an AS wrapper, drive BlueprintCallable UFUNCTIONs
// directly through FFunctionInvoker against the owning object or CDO.

	bool BPLibraryViaASGlobalWrapper(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		ASTEST_RESET_ENGINE(Engine);
	};

	// BuildModule compiles a bare AS source (no UCLASS / no UPROPERTY) into an
	// asIScriptModule. That's all we need to expose module-level wrapper
	// functions that forward to BP library statics. The wrappers themselves
	// are ordinary AS global functions, so FASGlobalFunctionInvoker reaches
	// them via `GetFunctionByDecl` on this module.
	asIScriptModule* Module = BuildModule(
		Test, Engine, "ASTemplateBPLibraryViaASGlobalWrapper",
		TEXT(R"(
// ---- Math library wrappers ----
// Note: this fork runs with asEP_FLOAT_IS_FLOAT64=1, so `float` below is
// actually a float64. We use explicit `double` in the wrapper signatures to
// make the stack-slot width unambiguous on both the AS and C++ sides.

double WrapMathAbs(double Value)
{
	return Math::Abs(Value);
}

double WrapMathSqrt(double Value)
{
	return Math::Sqrt(Value);
}

double WrapMathClamp(double Value, double MinValue, double MaxValue)
{
	return Math::Clamp(Value, MinValue, MaxValue);
}

double WrapMathLerp(double A, double B, double Alpha)
{
	return Math::Lerp(A, B, Alpha);
}

bool WrapMathIsNearlyEqual(double A, double B, double Tolerance)
{
	return Math::IsNearlyEqual(A, B, Tolerance);
}

// ---- FVector helper routed through Math::* ----
// Returns |V| through Math::Sqrt, demonstrating struct arg + scalar return
// with the BP-library function on the hot path.
double WrapVectorMagnitude(const FVector&in V)
{
	return Math::Sqrt(V.X * V.X + V.Y * V.Y + V.Z * V.Z);
}

// ---- Nested BP-library composition ----
// Exercises `Math::Square` + `Math::Min` + `Math::Max` in one expression to
// prove several BP statics can be stitched together by AS without any
// intermediate UFUNCTION-level indirection.
double WrapScoreBlend(double Value, double Floor, double Ceiling)
{
	return Math::Clamp(Math::Square(Value), Floor, Ceiling);
}
)"));
	if (!TestNotNull(TEXT("BPLibraryViaASGlobalWrapper module should compile"), Module))
	{
		return false;
	}

	// Math::Abs
	{
		FASGlobalFunctionInvoker Invoker(Test, Engine, *Module, TEXT("double WrapMathAbs(double)"));
		Invoker.AddArg(-7.25);
		TestTrue(TEXT("Math::Abs(-7.25) should return 7.25"),
			FMath::IsNearlyEqual(Invoker.CallAndReturn<double>(0.0), 7.25));
	}

	// Math::Sqrt
	{
		FASGlobalFunctionInvoker Invoker(Test, Engine, *Module, TEXT("double WrapMathSqrt(double)"));
		Invoker.AddArg(144.0);
		TestTrue(TEXT("Math::Sqrt(144.0) should return 12.0"),
			FMath::IsNearlyEqual(Invoker.CallAndReturn<double>(0.0), 12.0));
	}

	// Math::Clamp
	{
		FASGlobalFunctionInvoker Invoker(Test, Engine, *Module, TEXT("double WrapMathClamp(double, double, double)"));
		Invoker.AddArg(42.0).AddArg(0.0).AddArg(10.0);
		TestTrue(TEXT("Math::Clamp(42, 0, 10) should clamp to 10"),
			FMath::IsNearlyEqual(Invoker.CallAndReturn<double>(0.0), 10.0));
	}

	// Math::Lerp
	{
		FASGlobalFunctionInvoker Invoker(Test, Engine, *Module, TEXT("double WrapMathLerp(double, double, double)"));
		Invoker.AddArg(0.0).AddArg(100.0).AddArg(0.25);
		TestTrue(TEXT("Math::Lerp(0, 100, 0.25) should be 25"),
			FMath::IsNearlyEqual(Invoker.CallAndReturn<double>(0.0), 25.0));
	}

	// Math::IsNearlyEqual (bool return - exercises GetReturnByte through the bool branch)
	{
		FASGlobalFunctionInvoker Invoker(Test, Engine, *Module, TEXT("bool WrapMathIsNearlyEqual(double, double, double)"));
		Invoker.AddArg(1.00001).AddArg(1.00002).AddArg(0.001);
		TestTrue(TEXT("Math::IsNearlyEqual should report 1.00001 ~ 1.00002 within 0.001"),
			Invoker.CallAndReturn<bool>(false));
	}

	// FVector const-ref in + Math::Sqrt return
	{
		FVector Input(3.0, 4.0, 12.0); // magnitude = 13
		FASGlobalFunctionInvoker Invoker(Test, Engine, *Module, TEXT("double WrapVectorMagnitude(const FVector&in)"));
		Invoker.AddArgRef(Input);
		TestTrue(TEXT("WrapVectorMagnitude should compose FVector passing with Math::Sqrt"),
			FMath::IsNearlyEqual(Invoker.CallAndReturn<double>(0.0), 13.0, 1e-6));
	}

	// Nested BP-library composition: Math::Square(7) = 49, Clamp(49, 0, 100) = 49.
	{
		FASGlobalFunctionInvoker Invoker(Test, Engine, *Module,
			TEXT("double WrapScoreBlend(double, double, double)"));
		Invoker.AddArg(7.0).AddArg(0.0).AddArg(100.0);
		TestTrue(TEXT("WrapScoreBlend should stitch Math::Square + Math::Clamp (49 case)"),
			FMath::IsNearlyEqual(Invoker.CallAndReturn<double>(-1.0), 49.0));
	}

	// Same routine, but clamp clips: Square(7)=49, Clamp(49, 0, 20) = 20.
	{
		FASGlobalFunctionInvoker Invoker(Test, Engine, *Module,
			TEXT("double WrapScoreBlend(double, double, double)"));
		Invoker.AddArg(7.0).AddArg(0.0).AddArg(20.0);
		TestTrue(TEXT("WrapScoreBlend should observe the Math::Clamp ceiling"),
			FMath::IsNearlyEqual(Invoker.CallAndReturn<double>(-1.0), 20.0));
	}

	}
	return true;
}

// =============================================================================
// Test 10: BPLibraryThroughActorUFunction
// =============================================================================
//
// Compose the two reflection paths:
//   - an AS-defined UFUNCTION on a spawned actor calls several `Math::*` BP
//     statics internally, exercising the BP-library bridge INSIDE the script;
//   - the C++ test reaches that UFUNCTION through FFunctionInvoker, which for
//     AS UFUNCTIONs routes through UASFunction::RuntimeCallEvent.
//
// This is the most "production-like" shape: gameplay code sitting behind a
// UFUNCTION, BP library usage inside, C++ test driving the whole path.

	bool BPLibraryThroughActorUFunction(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("TemplateReflectionBPLibraryActor"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	UClass* ScriptClass = CompileScriptModule(
		Test,
		Engine,
		ModuleName,
		TEXT("TemplateReflectionBPLibraryActor.as"),
		TEXT(R"AS(
UCLASS()
class ATemplateReflectionBPLibraryActor : AActor
{
	// Script-side "last computed" mirror of each value so the C++ test can
	// also verify the UFUNCTION's side-effects through the path helpers,
	// not just its return value.
	UPROPERTY() double LastMagnitude   = 0.0;
	UPROPERTY() double LastClampedBlend = 0.0;

	// Compose several BP library calls into one UFUNCTION that returns a
	// single composite value. The C++ test asserts on the return AND on the
	// UPROPERTY mirrors.
	UFUNCTION()
	double ComputeBlend(const FVector&in V, double BlendAlpha, double BlendMin, double BlendMax)
	{
		// Magnitude via Math::Sqrt.
		LastMagnitude = Math::Sqrt(V.X * V.X + V.Y * V.Y + V.Z * V.Z);

		// Lerp 0..Magnitude by BlendAlpha, then clamp to [BlendMin, BlendMax].
		double Blended = Math::Lerp(0.0, LastMagnitude, BlendAlpha);
		LastClampedBlend = Math::Clamp(Blended, BlendMin, BlendMax);
		return LastClampedBlend;
	}
}
)AS"),
		TEXT("ATemplateReflectionBPLibraryActor"));
	if (ScriptClass == nullptr)
	{
		return false;
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(Test, Spawner, ScriptClass);
	if (!TestNotNull(TEXT("BP-library actor should spawn"), Actor))
	{
		return false;
	}
	BeginPlayActor(Engine, *Actor);

	// Invoke the AS UFUNCTION (which calls Math::Sqrt / Math::Lerp /
	// Math::Clamp internally) and assert on its return value.
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("ComputeBlend")));
		if (!Invoker.IsValid())
		{
			return false;
		}
		// FVector(3, 4, 12) has magnitude 13. Lerp(0, 13, 0.5) == 6.5.
		// Clamp(6.5, 0, 10) == 6.5.
		FVector Input(3.0, 4.0, 12.0);
		Invoker.AddParam<FVector>(Input);
		Invoker.AddParam<double>(0.5);
		Invoker.AddParam<double>(0.0);
		Invoker.AddParam<double>(10.0);
		const double Result = Invoker.CallAndReturn<double>(-1.0);
		TestTrue(TEXT("ComputeBlend should round-trip Math::Sqrt + Math::Lerp + Math::Clamp"),
			FMath::IsNearlyEqual(Result, 6.5, 1e-6));
	}

	// Re-confirm the side-effect mirrors through the path helpers. This
	// exercises the "BP library computation observable via FPropertyBindingPath"
	// shape, which is how real tests typically assert on gameplay code.
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("LastMagnitude"), 13.0,
		TEXT("LastMagnitude UPROPERTY should mirror the BP-library sqrt result"));
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("LastClampedBlend"), 6.5,
		TEXT("LastClampedBlend UPROPERTY should mirror the BP-library clamp output"));

	// Second invocation exercises the clamp path — Blended (10) would become
	// 9 via Lerp, but we clamp to [0, 5] so the return should be 5.
	{
		FFunctionInvoker Invoker(Test, Actor, FName(TEXT("ComputeBlend")));
		if (!Invoker.IsValid())
		{
			return false;
		}
		FVector Input(0.0, 0.0, 10.0); // magnitude 10
		Invoker.AddParam<FVector>(Input);
		Invoker.AddParam<double>(0.9);
		Invoker.AddParam<double>(0.0);
		Invoker.AddParam<double>(5.0);
		const double Result = Invoker.CallAndReturn<double>(-1.0);
		TestTrue(TEXT("ComputeBlend should clamp the Lerp output to the supplied max"),
			FMath::IsNearlyEqual(Result, 5.0, 1e-6));
	}
	VerifyByPath<FDoubleProperty, double>(Test, Actor, TEXT("LastClampedBlend"), 5.0,
		TEXT("LastClampedBlend UPROPERTY should reflect the second invocation's clamp output"));

	}
	return true;
}

#undef TestNotNull
#undef TestEqual
#undef TestFalse
#undef TestTrue
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTemplateReflectionAccessTest,
	"Angelscript.Template.ReflectionAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PathAndInvoke)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::PathAndInvoke(*TestRunner)));
	}

	TEST_METHOD(PathAccessAllTypes)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::PathAccessAllTypes(*TestRunner)));
	}

	TEST_METHOD(FunctionInvokeAllTypes)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::FunctionInvokeAllTypes(*TestRunner)));
	}

	TEST_METHOD(PathAccessExtendedTypes)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::PathAccessExtendedTypes(*TestRunner)));
	}

	TEST_METHOD(FunctionInvokeExtendedTypes)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::FunctionInvokeExtendedTypes(*TestRunner)));
	}

	TEST_METHOD(PathAccessReferenceTypes)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::PathAccessReferenceTypes(*TestRunner)));
	}

	TEST_METHOD(PathAccessScriptDefinedTypes)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::PathAccessScriptDefinedTypes(*TestRunner)));
	}

	TEST_METHOD(PathParserLimits)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::PathParserLimits(*TestRunner)));
	}

	TEST_METHOD(BPLibraryViaASGlobalWrapper)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::BPLibraryViaASGlobalWrapper(*TestRunner)));
	}

	TEST_METHOD(BPLibraryThroughActorUFunction)
	{
		ASSERT_THAT(IsTrue(TemplateReflectionAccessTest::BPLibraryThroughActorUFunction(*TestRunner)));
	}
};

#endif
