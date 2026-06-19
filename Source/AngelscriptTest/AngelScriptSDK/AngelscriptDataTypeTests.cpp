#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_datatype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptDataTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.DataType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Primitives)
	{
		asCDataType IntType = asCDataType::CreatePrimitive(ttInt, false);
		asCDataType FloatType = asCDataType::CreatePrimitive(ttFloat32, false);
		asCDataType BoolType = asCDataType::CreatePrimitive(ttBool, false);
		asCDataType VoidType = asCDataType::CreatePrimitive(ttVoid, false);
		asCDataType NullHandleType = asCDataType::CreateNullHandle();

		ASSERT_THAT(IsTrue(IntType.IsValid() && IntType.IsPrimitive() && IntType.IsIntegerType(),
			TEXT("int data type should be valid and primitive")));
		ASSERT_THAT(IsTrue(FloatType.IsPrimitive() && FloatType.IsFloat32Type() && FloatType.IsMathType(),
			TEXT("float32 data type should report float semantics")));
		ASSERT_THAT(IsTrue(BoolType.IsPrimitive() && BoolType.IsBooleanType(),
			TEXT("bool data type should report boolean semantics")));
		ASSERT_THAT(IsFalse(VoidType.CanBeInstantiated(),
			TEXT("void data type should not be instantiable")));
		ASSERT_THAT(IsTrue(NullHandleType.IsNullHandle() && NullHandleType.IsObjectHandle(),
			TEXT("null handle data type should report object-handle semantics")));
	}

	TEST_METHOD(Comparisons)
	{
		asCDataType MutableInt = asCDataType::CreatePrimitive(ttInt, false);
		asCDataType ConstInt = asCDataType::CreatePrimitive(ttInt, true);
		asCDataType RefInt = asCDataType::CreatePrimitive(ttInt, false);
		RefInt.MakeReference(true);

		ASSERT_THAT(IsTrue(MutableInt.IsEqualExceptConst(ConstInt),
			TEXT("Constness should be ignored by IsEqualExceptConst")));
		ASSERT_THAT(IsFalse(MutableInt == ConstInt,
			TEXT("Constness should still matter for exact equality")));
		ASSERT_THAT(IsTrue(MutableInt.IsEqualExceptRef(RefInt),
			TEXT("Reference-ness should be ignored by IsEqualExceptRef")));
		ASSERT_THAT(IsFalse(MutableInt == RefInt,
			TEXT("Reference-ness should still matter for exact equality")));
	}

	TEST_METHOD(HandleQualifierMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		asCTypeInfo* ActorType = static_cast<asCTypeInfo*>(ScriptEngine->GetTypeInfoByName("AActor"));
		ASSERT_THAT(IsNotNull(ActorType,
			TEXT("AActor should exist in the script type system for handle qualifier comparisons")));

		asCDataType ActorValueType = asCDataType::CreateType(ActorType, false);
		asCDataType ActorHandleType = asCDataType::CreateObjectHandle(ActorType, false);
		asCDataType ConstActorHandleType = asCDataType::CreateObjectHandle(ActorType, true);
		asCDataType RefConstActorHandleType = ConstActorHandleType;
		RefConstActorHandleType.MakeReference(true);
		asCDataType NullHandleType = asCDataType::CreateNullHandle();

		ASSERT_THAT(IsTrue(ActorHandleType.GetTypeInfo() == ActorType,
			TEXT("Object handle matrix should preserve the target type info")));
		ASSERT_THAT(IsFalse(ActorHandleType == ConstActorHandleType,
			TEXT("Exact equality should distinguish mutable and const handles")));
		ASSERT_THAT(IsTrue(ActorHandleType.IsEqualExceptConst(ConstActorHandleType),
			TEXT("IsEqualExceptConst should ignore handle constness")));
		ASSERT_THAT(IsTrue(ActorHandleType.IsEqualExceptRefAndConst(RefConstActorHandleType),
			TEXT("IsEqualExceptRefAndConst should ignore both reference and const on handles")));
		ASSERT_THAT(IsFalse(NullHandleType == ActorHandleType,
			TEXT("Null handle should not be exactly equal to a typed object handle")));
		ASSERT_THAT(IsTrue(NullHandleType.IsObjectHandle() && NullHandleType.IsNullHandle(),
			TEXT("Null handle should still report object-handle semantics")));
		ASSERT_THAT(IsTrue(ActorValueType.IsObject() && ActorHandleType.IsObjectHandle(),
			TEXT("Value type and object handle should keep different kind semantics")));

		}
	}

	TEST_METHOD(ObjectHandles)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		asCTypeInfo* ActorType = static_cast<asCTypeInfo*>(ScriptEngine->GetTypeInfoByName("AActor"));
		ASSERT_THAT(IsNotNull(ActorType,
			TEXT("AActor should exist in the script type system for data-type handle tests")));

		asCDataType ActorValueType = asCDataType::CreateType(ActorType, false);
		ASSERT_THAT(IsTrue(ActorValueType.IsObject(),
			TEXT("AActor value type should be recognized as an object type")));
		ASSERT_THAT(IsTrue(ActorValueType.SupportHandles(),
			TEXT("AActor value type should support handles")));

		asCDataType ActorHandleType = asCDataType::CreateObjectHandle(ActorType, false);
		ASSERT_THAT(IsTrue(ActorHandleType.IsObjectHandle(),
			TEXT("CreateObjectHandle should mark the type as an object handle")));
		ASSERT_THAT(IsTrue(ActorHandleType.CanBeInstantiated(),
			TEXT("Object handle should still be considered instantiable as a handle slot")));
		}
	}

	TEST_METHOD(SizeAndAlignment)
	{
		asCDataType IntType = asCDataType::CreatePrimitive(ttInt, false);
		asCDataType Float64Type = asCDataType::CreatePrimitive(ttFloat64, false);
		asCDataType BoolType = asCDataType::CreatePrimitive(ttBool, false);

		ASSERT_THAT(AreEqual(1, IntType.GetSizeInMemoryDWords(),
			TEXT("int should occupy one dword in memory")));
		ASSERT_THAT(AreEqual(8, Float64Type.GetSizeInMemoryBytes(),
			TEXT("float64 should occupy eight bytes in memory")));
		ASSERT_THAT(AreEqual(1, BoolType.GetAlignment(),
			TEXT("bool alignment should stay byte-sized")));
	}
};

#endif
