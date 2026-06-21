#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;



TEST_CLASS_WITH_FLAGS(FAngelscriptSDKObjectTests, "Angelscript.TestModule.AngelScriptSDK.Object", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class CObject
	{
	public:
		CObject()
			: Value(0)
		{
		}

		void Set(int32 InValue)
		{
			Value = InValue;
		}

		int32 Get() const
		{
			return Value;
		}

		int32& GetRef()
		{
			return Value;
		}

		int32 Value;
	};

	static void ConstructObject(CObject* Address)
	{
		new (Address) CObject();
	}

	static void DestructObject(CObject* Address)
	{
		Address->~CObject();
	}

	inline static CObject GReturnedObject;

	static CObject ReturnObjectValue()
	{
		CObject Result;
		Result.Value = 12;
		return Result;
	}

	static CObject* ReturnObjectRef()
	{
		return &GReturnedObject;
	}

	static void ConstructDefaultMyObj(class CMyObj& Address);
	static void ConstructCopyMyObj(class CMyObj& Address, const class CMyObj& Other);
	static void DestructMyObj(class CMyObj& Address);
	class CMyObj
	{
public:
		CMyObj() = default;
		CMyObj(const CMyObj&) = default;
	};

	static void ConstructDefaultMyObj(CMyObj& Address)
	{
		new (&Address) CMyObj();
	}

	static void ConstructCopyMyObj(CMyObj& Address, const CMyObj& Other)
	{
		new (&Address) CMyObj(Other);
	}

	static void DestructMyObj(CMyObj& Address)
	{
		Address.~CMyObj();
	}

	struct CFloatWrapper
	{
		float Value;

		CFloatWrapper()
			: Value(0.0f)
		{
		}
	};

	static void ConstructFloatWrapper(CFloatWrapper* Address)
	{
		new (Address) CFloatWrapper();
	}

	static CFloatWrapper& AssignFloatToWrapper(float InValue, CFloatWrapper& Target)
	{
		Target.Value = InValue;
		return Target;
	}

	static float AddWrapperToWrapper(CFloatWrapper* Self, CFloatWrapper* Other)
	{
		return Self->Value + Other->Value;
	}

	static float MultiplyWrapperByFloat(CFloatWrapper* Self, float Other)
	{
		return Self->Value * Other;
	}

	static CFloatWrapper& AccessWrapperSlot(int32 Index)
	{
		static CFloatWrapper Slots[8];
		return Slots[Index];
	}
public:
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}

	TEST_METHOD(ValueType)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK object value-type test should create a standalone engine")));

		const int RegisterObjectResult = ScriptEngine->RegisterObjectType("Object", sizeof(CObject), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CObject>() | asOBJ_APP_CLASS_ALLINTS);
		const ASAutoCaller::FunctionCaller ObjectConstructCaller = ASAutoCaller::MakeFunctionCaller(ConstructObject);
		const int RegisterConstructResult = ScriptEngine->RegisterObjectBehaviour("Object", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructObject), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ObjectConstructCaller);
		const int RegisterDestructResult = ScriptEngine->RegisterObjectBehaviour("Object", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DestructObject), asCALL_CDECL_OBJLAST);
		const int RegisterPropertyResult = ScriptEngine->RegisterObjectProperty("Object", "int Value", asOFFSET(CObject, Value));

		ASSERT_THAT(IsTrue(
			RegisterObjectResult >= 0 &&
			RegisterConstructResult >= 0 &&
			RegisterDestructResult >= 0 &&
			RegisterPropertyResult >= 0,
			TEXT("SDK object value-type test should register all object APIs")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKObjectValueType", R"(
bool CopyObjectValue()
{
	Object value;
	value.Value = 10;
	Object copy = value;
	return copy.Value == 10;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "bool CopyObjectValue()"),
			TEXT("SDK object value-type test should expose the named object-copy function")));
	}

	TEST_METHOD(ConstructorChain)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK object constructor-chain test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKObjectConstructorChain", R"(
class InternalClass
{
	InternalClass()
	{
		m_x = 3;
		m_y = 773456;
	}
	int8 m_x;
	int  m_y;
}

class MyClass
{
	MyClass()
	{
		m_c = InternalClass();
	}
	bool Test() const
	{
		return m_c.m_x == 3 && m_c.m_y == 773456;
	}
	InternalClass m_c;
}

bool ConstructNestedMember()
{
	MyClass test;
	return test.Test();
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "bool ConstructNestedMember()"),
			TEXT("SDK object constructor-chain test should expose the named constructor-chain function")));
	}

	TEST_METHOD(NativeFloatWrapper)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK object native-float wrapper test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKObjectFloatValue", R"(
class FloatValue
{
	float Value;
}

bool StoreNativeFloat()
{
	FloatValue value;
	value.Value = 10.0f;
	return value.Value > 9.9f && value.Value < 10.1f;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "bool StoreNativeFloat()"),
			TEXT("SDK object native-float wrapper test should expose the named float-value function")));
	}
};

#endif
