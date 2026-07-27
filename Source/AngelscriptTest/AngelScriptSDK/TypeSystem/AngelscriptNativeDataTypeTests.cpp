#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_datatype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDataTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.DataType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DataTypePrimitives)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-DATATYPE-QUALIFIER-CARTESIAN supersedes this five-type smoke with eleven primitive types crossed by four qualifier states, exact format, size, alignment, comparison, compilation, execution, discard, and isolation");

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

	TEST_METHOD(DataTypeComparisons)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-DATATYPE-QUALIFIER-CARTESIAN supersedes these three comparison checks for every primitive and mutable/const/reference/const-reference state");

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

	TEST_METHOD(HandleQualifiersPreserveConstAndReferenceFlags)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-DATATYPE-HANDLE-CONTRACT",
			ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Data-type handle qualifier test should create a raw SDK engine")));
		if (ScriptEngine == nullptr || ScriptEngine->RegisterObjectType("TestObject", 0, asOBJ_REF) < 0)
		{
			TestRunner->AddError(TEXT("Data-type handle qualifier test should register a raw reference object type"));
			return;
		}
		asCTypeInfo* ObjectType = static_cast<asCTypeInfo*>(ScriptEngine->GetTypeInfoByName("TestObject"));
		ASSERT_THAT(IsNotNull(ObjectType,
			TEXT("Registered raw object type should exist in the script type system")));
		if (ObjectType == nullptr)
		{
			return;
		}

		asCDataType ObjectValueType = asCDataType::CreateType(ObjectType, false);
		asCDataType ObjectHandleType = asCDataType::CreateObjectHandle(ObjectType, false);
		asCDataType ConstObjectHandleType = asCDataType::CreateObjectHandle(ObjectType, true);
		asCDataType RefConstObjectHandleType = ConstObjectHandleType;
		RefConstObjectHandleType.MakeReference(true);
		asCDataType NullHandleType = asCDataType::CreateNullHandle();

		const struct
		{
			const TCHAR* Id;
			const TCHAR* Declaration;
			const TCHAR* Expected;
		} ReviewCases[] =
		{
			{ TEXT("object-value"), TEXT("TestObject"), TEXT("object kind with handle support") },
			{ TEXT("mutable-handle"), TEXT("TestObject@"), TEXT("typed mutable handle") },
			{ TEXT("const-handle"), TEXT("const TestObject@"), TEXT("typed const handle") },
			{ TEXT("const-reference-handle"), TEXT("const TestObject@&"), TEXT("const and reference flags preserved") },
			{ TEXT("null-handle"), TEXT("<null handle>"), TEXT("untyped null-handle sentinel") },
		};
		for (const auto& ReviewCase : ReviewCases)
		{
			FString Source;
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("// Declaration: %s"), ReviewCase.Declaration));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("// Expected: %s"), ReviewCase.Expected));
			PrintGeneratedAsSource(
				*TestRunner,
				MakeNativeCaseId("TYPE-DATATYPE-HANDLE-CONTRACT", { ReviewCase.Id }),
				TEXT("TypeDataTypeHandleNativeReview"),
				Source);
		}

		ASSERT_THAT(IsTrue(ObjectHandleType.GetTypeInfo() == ObjectType,
			TEXT("Object handle should preserve the target type info")));
		ASSERT_THAT(IsFalse(ObjectHandleType == ConstObjectHandleType,
			TEXT("Exact equality should distinguish mutable and const handles")));
		ASSERT_THAT(IsTrue(ObjectHandleType.IsEqualExceptConst(ConstObjectHandleType),
			TEXT("IsEqualExceptConst should ignore handle constness")));
		ASSERT_THAT(IsTrue(ObjectHandleType.IsEqualExceptRefAndConst(RefConstObjectHandleType),
			TEXT("IsEqualExceptRefAndConst should ignore both reference and const on handles")));
		ASSERT_THAT(IsFalse(NullHandleType == ObjectHandleType,
			TEXT("Null handle should not be exactly equal to a typed object handle")));
		ASSERT_THAT(IsTrue(NullHandleType.IsObjectHandle() && NullHandleType.IsNullHandle(),
			TEXT("Null handle should still report object-handle semantics")));
		ASSERT_THAT(IsTrue(ObjectValueType.IsObject() && ObjectHandleType.IsObjectHandle(),
			TEXT("Value type and object handle should keep different kind semantics")));
		ASSERT_THAT(IsTrue(ObjectValueType.SupportHandles(),
			TEXT("Registered reference object value type should advertise handle support")));
		ASSERT_THAT(IsTrue(ObjectHandleType.CanBeInstantiated(),
			TEXT("Typed object-handle slot should remain instantiable")));
		ASSERT_THAT(AreEqual(
			0,
			ObjectHandleType.GetSizeInMemoryBytes(),
			TEXT("Current fork should report the registered zero-size reference object metadata from GetSizeInMemoryBytes")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(sizeof(void*)),
			ObjectHandleType.GetSizeOfVariableBytes(),
			TEXT("Typed object-handle variable should occupy one native pointer despite zero object metadata size")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(alignof(void*)),
			ObjectHandleType.GetAlignment(),
			TEXT("Typed object handle should retain native pointer alignment")));
	}

	TEST_METHOD(DataTypeObjectHandles)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-DATATYPE-HANDLE-CONTRACT supersedes this two-predicate smoke with exact value/mutable/const/reference/null handle identity, comparison, size, alignment, and instantiation metadata; independent object-type ownership and cleanup belong to the dedicated release products");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Data-type object-handle test should create a raw SDK engine")));
		if (ScriptEngine == nullptr || ScriptEngine->RegisterObjectType("TestObject", 0, asOBJ_REF) < 0)
		{
			TestRunner->AddError(TEXT("Data-type object-handle test should register a raw reference object type"));
			return;
		}
		asCTypeInfo* ObjectType = static_cast<asCTypeInfo*>(ScriptEngine->GetTypeInfoByName("TestObject"));
		ASSERT_THAT(IsNotNull(ObjectType,
			TEXT("Registered raw object type should exist in the script type system")));
		if (ObjectType == nullptr)
		{
			return;
		}

		asCDataType ObjectValueType = asCDataType::CreateType(ObjectType, false);
		ASSERT_THAT(IsTrue(ObjectValueType.IsObject(),
			TEXT("Raw reference object type should be recognized as an object type")));
		ASSERT_THAT(IsTrue(ObjectValueType.SupportHandles(),
			TEXT("Raw reference object type should support handles")));

		asCDataType ObjectHandleType = asCDataType::CreateObjectHandle(ObjectType, false);
		ASSERT_THAT(IsTrue(ObjectHandleType.IsObjectHandle(),
			TEXT("CreateObjectHandle should mark the type as an object handle")));
		ASSERT_THAT(IsTrue(ObjectHandleType.CanBeInstantiated(),
			TEXT("Object handle should still be considered instantiable as a handle slot")));
	}

	TEST_METHOD(SizeAndAlignment)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-DATATYPE-QUALIFIER-CARTESIAN supersedes these three size checks with exact byte, dword, and alignment evidence for every primitive and qualifier state");

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
