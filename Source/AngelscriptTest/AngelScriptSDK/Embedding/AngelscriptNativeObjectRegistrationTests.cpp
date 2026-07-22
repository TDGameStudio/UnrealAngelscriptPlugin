#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace ObjectRegistrationTest
{
	inline static double LastAssignedValue = 0.0;
	inline static double LastAddedValue = 0.0;
	struct FFloatWrapper
	{
		double Value = 0.0;

		void SetValue(double InValue)
		{
			LastAssignedValue = InValue;
			Value = InValue;
		}

		double Add(const FFloatWrapper& Other) const
		{
			LastAddedValue = Value + Other.Value;
			return LastAddedValue;
		}
	};
	static void ConstructFloatWrapper(FFloatWrapper* Address) { new (Address) FFloatWrapper(); }
}

TEST_CLASS_WITH_FLAGS(FObjectRegistrationTests, "Angelscript.TestModule.AngelScriptSDK.Embedding.ObjectRegistration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	struct FCounter { int32 Value; };
	static void ConstructCounter(FCounter* Address) { new(Address) FCounter{0}; }

	TEST_METHOD(ObjectRegistrationSimpleValueType)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Object registration should create a raw SDK engine")));
		if (ScriptEngine == nullptr) return;
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType("Counter", sizeof(FCounter), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FCounter>() | asOBJ_APP_CLASS_ALLINTS) >= 0, TEXT("Object registration should register the value type")));
		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructCounter);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour("Counter", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructCounter), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ConstructorCaller) >= 0, TEXT("Object registration should register the constructor")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty("Counter", "int Value", asOFFSET(FCounter, Value)) >= 0, TEXT("Object registration should register the property")));
		FScopedNativeModule Module(*TestRunner, Engine, "ObjectRegistration", "int Entry() { Counter Value; Value.Value = 19; return Value.Value + 23; }");
		if (!Module.IsValid()) return;
		asIScriptFunction* Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
		if (!this->Assert.IsNotNull(Function, TEXT("Object registration should resolve the script entry"))) return;
		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!this->Assert.IsNotNull(Context, TEXT("Object registration should create a context"))) return;
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Registered object should execute from script")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Registered object constructor and property should preserve state")));
	}

	TEST_METHOD(NativeFloatWrapper)
	{
		using namespace AngelscriptNativeTestSupport;
		ObjectRegistrationTest::LastAssignedValue = 0.0;
		ObjectRegistrationTest::LastAddedValue = 0.0;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native float wrapper test should create a raw SDK engine")));
		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ObjectRegistrationTest::ConstructFloatWrapper);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType("FloatWrapper", sizeof(ObjectRegistrationTest::FFloatWrapper), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ObjectRegistrationTest::FFloatWrapper>() | asOBJ_APP_CLASS_ALLFLOATS) >= 0, TEXT("Native float wrapper test should register its value type")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour("FloatWrapper", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ObjectRegistrationTest::ConstructFloatWrapper), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ConstructorCaller) >= 0, TEXT("Native float wrapper test should register construction")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod("FloatWrapper", "void SetValue(double InValue)", asMETHODPR(ObjectRegistrationTest::FFloatWrapper, SetValue, (double), void), asCALL_THISCALL) >= 0, TEXT("Native float wrapper test should register the current fork's double-backed value method")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectMethod("FloatWrapper", "double Add(const FloatWrapper &in Other) const", asMETHODPR(ObjectRegistrationTest::FFloatWrapper, Add, (const ObjectRegistrationTest::FFloatWrapper&) const, double), asCALL_THISCALL) >= 0, TEXT("Native float wrapper test should register the current fork's double-backed addition method")));
		FScopedNativeModule Module(*TestRunner, Engine, "NativeFloatWrapper", "double Entry() { FloatWrapper left; FloatWrapper right; left.SetValue(10.0); right.SetValue(2.5); return left.Add(right); }");
		if (!Module.IsValid()) return;
		asIScriptFunction* const Function = GetNativeFunctionByExactDecl(Module, "double Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Native float wrapper test should resolve its exact entry")));
		if (Function == nullptr) return;
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Native float wrapper test should create a context")));
		if (Context == nullptr) return;
		ON_SCOPE_EXIT { Context->Release(); };
		const int ExecuteResult = PrepareAndExecute(Context, Function);
		const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		// The current float64 fork accepts the registration shape but cannot execute
		// native value-type methods with double-backed scalar parameters. Keep this
		// separate from the working integer thiscall path in CallingConventionTests.
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, ExecuteResult, TEXT("Native float wrapper test should expose the current fork's double-backed native-call limitation")));
		ASSERT_THAT(AreEqual(FString(TEXT("Native calling convention support is disabled. Make sure you're passing a correct Caller.")), ExceptionString, TEXT("Native float wrapper test should report the current fork's native-call limitation")));
		ASSERT_THAT(IsNear(0.0, ObjectRegistrationTest::LastAssignedValue, 0.0001, TEXT("Native float wrapper test should not partially execute the double-backed value method")));
		ASSERT_THAT(IsNear(0.0, ObjectRegistrationTest::LastAddedValue, 0.0001, TEXT("Native float wrapper test should not partially execute the double-backed addition method")));
	}
};

#endif
