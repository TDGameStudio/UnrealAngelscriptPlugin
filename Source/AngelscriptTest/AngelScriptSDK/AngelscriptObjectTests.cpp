#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

namespace
{
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

	void ConstructObject(CObject* Address)
	{
		new (Address) CObject();
	}

	void DestructObject(CObject* Address)
	{
		Address->~CObject();
	}

	CObject GReturnedObject;

	CObject ReturnObjectValue()
	{
		CObject Result;
		Result.Value = 12;
		return Result;
	}

	CObject* ReturnObjectRef()
	{
		return &GReturnedObject;
	}

	void ConstructDefaultMyObj(class CMyObj& Address);
	void ConstructCopyMyObj(class CMyObj& Address, const class CMyObj& Other);
	void DestructMyObj(class CMyObj& Address);
	class CMyObj
	{
public:
		CMyObj() = default;
		CMyObj(const CMyObj&) = default;
	};

	void ConstructDefaultMyObj(CMyObj& Address)
	{
		new (&Address) CMyObj();
	}

	void ConstructCopyMyObj(CMyObj& Address, const CMyObj& Other)
	{
		new (&Address) CMyObj(Other);
	}

	void DestructMyObj(CMyObj& Address)
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

	void ConstructFloatWrapper(CFloatWrapper* Address)
	{
		new (Address) CFloatWrapper();
	}

	CFloatWrapper& AssignFloatToWrapper(float InValue, CFloatWrapper& Target)
	{
		Target.Value = InValue;
		return Target;
	}

	float AddWrapperToWrapper(CFloatWrapper* Self, CFloatWrapper* Other)
	{
		return Self->Value + Other->Value;
	}

	float MultiplyWrapperByFloat(CFloatWrapper* Self, float Other)
	{
		return Self->Value * Other;
	}

	CFloatWrapper& AccessWrapperSlot(int32 Index)
	{
		static CFloatWrapper Slots[8];
		return Slots[Index];
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKObjectTests, "Angelscript.TestModule.AngelScriptSDK.Object", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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
		if (!TestRunner->TestNotNull(TEXT("SDK object value-type test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		const int RegisterObjectResult = ScriptEngine->RegisterObjectType("Object", sizeof(CObject), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CObject>() | asOBJ_APP_CLASS_ALLINTS);
		const ASAutoCaller::FunctionCaller ObjectConstructCaller = ASAutoCaller::MakeFunctionCaller(ConstructObject);
		const int RegisterConstructResult = ScriptEngine->RegisterObjectBehaviour("Object", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructObject), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ObjectConstructCaller);
		const int RegisterDestructResult = ScriptEngine->RegisterObjectBehaviour("Object", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DestructObject), asCALL_CDECL_OBJLAST);
		const int RegisterPropertyResult = ScriptEngine->RegisterObjectProperty("Object", "int Value", asOFFSET(CObject, Value));

		if (!TestRunner->TestTrue(TEXT("SDK object value-type test should register all object APIs"),
			RegisterObjectResult >= 0 &&
			RegisterConstructResult >= 0 &&
			RegisterDestructResult >= 0 &&
			RegisterPropertyResult >= 0))
		{
			return;
		}

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

		TestRunner->TestNotNull(TEXT("SDK object value-type test should expose the named object-copy function"), GetNativeFunctionByDecl(Module, "bool CopyObjectValue()"));
	}

	TEST_METHOD(ConstructorChain)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK object constructor-chain test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

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

		TestRunner->TestNotNull(TEXT("SDK object constructor-chain test should expose the named constructor-chain function"), GetNativeFunctionByDecl(Module, "bool ConstructNestedMember()"));
	}

	TEST_METHOD(NativeFloatWrapper)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK object native-float wrapper test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

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

		TestRunner->TestNotNull(TEXT("SDK object native-float wrapper test should expose the named float-value function"), GetNativeFunctionByDecl(Module, "bool StoreNativeFloat()"));
	}
};

#endif
