#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace ObjectRegistrationTest
{
	inline static double LastAssignedValue = 0.0;
	inline static double LastAddedValue = 0.0;
	inline static int32 MissingCallerInvocationCount = 0;

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

	struct FCallerRequirementCounter
	{
		int32 Value = 0;

		int32 Add(const int32 Delta) const
		{
			MissingCallerInvocationCount++;
			return Value + Delta;
		}
	};

	static void ConstructFloatWrapper(FFloatWrapper* Address)
	{
		new (Address) FFloatWrapper();
	}

	static void ConstructCallerRequirementCounter(FCallerRequirementCounter* Address)
	{
		new (Address) FCallerRequirementCounter();
	}

	static int32 InvalidThiscallFreeFunction(const int32 Delta)
	{
		MissingCallerInvocationCount++;
		return Delta;
	}
}

TEST_CLASS_WITH_FLAGS(FObjectRegistrationTests, "Angelscript.TestModule.AngelScriptSDK.Embedding.ObjectRegistration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	struct FCounter
	{
		int32 Value;
	};

	static void ConstructCounter(FCounter* Address)
	{
		new (Address) FCounter{0};
	}

	TEST_METHOD(ObjectRegistrationSimpleValueType)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("EMBED-OBJECT-REGISTRATION-CONTRACTS",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Object registration should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType("Counter", sizeof(FCounter), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FCounter>() | asOBJ_APP_CLASS_ALLINTS) >= 0, TEXT("Object registration should register the value type")));
		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructCounter);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour("Counter", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructCounter), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ConstructorCaller) >= 0, TEXT("Object registration should register the constructor")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty("Counter", "int Value", asOFFSET(FCounter, Value)) >= 0, TEXT("Object registration should register the property")));
		asITypeInfo* const CounterType = ScriptEngine->GetTypeInfoByDecl("Counter");
		ASSERT_THAT(IsNotNull(CounterType, TEXT("Object registration should publish value-type metadata")));
		if (CounterType != nullptr)
		{
			const char* PropertyName = nullptr;
			int PropertyTypeId = asTYPEID_VOID;
			int PropertyOffset = INDEX_NONE;
			ASSERT_THAT(AreEqual(
				static_cast<int32>(sizeof(FCounter)),
				CounterType->GetSize(),
				TEXT("Object registration should preserve the native value size")));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(1),
				CounterType->GetPropertyCount(),
				TEXT("Object registration should publish the native property")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				CounterType->GetProperty(
					0,
					&PropertyName,
					&PropertyTypeId,
					nullptr,
					nullptr,
					&PropertyOffset),
				TEXT("Object registration should expose indexed property metadata")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Value")),
				FString(UTF8_TO_TCHAR(PropertyName != nullptr ? PropertyName : "")),
				TEXT("Object registration should preserve the property name")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asTYPEID_INT32),
				PropertyTypeId,
				TEXT("Object registration should preserve the property type")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asOFFSET(FCounter, Value)),
				PropertyOffset,
				TEXT("Object registration should preserve the native property offset")));
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				Counter Value;
				Value.Value = 19;
				return Value.Value + 23;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("EMBED-OBJECT-REGISTRATION-CONTRACTS-SIMPLE-VALUE"),
			TEXT("ObjectRegistration"),
			UTF8_TO_TCHAR(Source.c_str()));
		FScopedNativeModule Module(*TestRunner, Engine, "ObjectRegistration", Source);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
		if (!this->Assert.IsNotNull(Function, TEXT("Object registration should resolve the script entry")))
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			if (!this->Assert.IsNotNull(Context, TEXT("Object registration should create a context")))
			{
				return;
			}

			ON_SCOPE_EXIT
			{
				Context->Release();
			};
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Registered object should execute from script")));
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Registered object constructor and property should preserve state")));
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("Object registration should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("ObjectRegistration", asGM_ONLY_IF_EXISTS),
			TEXT("Object registration module should be absent after discard")));

		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Object registration should create an independent raw SDK engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(IsNull(
				IndependentEngine.Get()->GetTypeInfoByDecl("Counter"),
				TEXT("Object registration should not leak into an independent engine")));
		}
	}

	TEST_METHOD(NativeFloatWrapper)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-OBJECT-REGISTRATION-CONTRACTS",
			"double_value");

		ObjectRegistrationTest::LastAssignedValue = 0.0;
		ObjectRegistrationTest::LastAddedValue = 0.0;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native float wrapper test should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ObjectRegistrationTest::ConstructFloatWrapper);
		const ASAutoCaller::FunctionCaller SetValueCaller = ASAutoCaller::MakeFunctionCaller(&ObjectRegistrationTest::FFloatWrapper::SetValue);
		const ASAutoCaller::FunctionCaller AddCaller = ASAutoCaller::MakeFunctionCaller(&ObjectRegistrationTest::FFloatWrapper::Add);
		const int TypeResult = ScriptEngine->RegisterObjectType(
			"FloatWrapper",
			sizeof(ObjectRegistrationTest::FFloatWrapper),
			asOBJ_VALUE
				| asOBJ_POD
				| asGetTypeTraits<ObjectRegistrationTest::FFloatWrapper>()
				| asOBJ_APP_CLASS_ALLFLOATS);
		const int ConstructorId = ScriptEngine->RegisterObjectBehaviour(
			"FloatWrapper",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ObjectRegistrationTest::ConstructFloatWrapper),
			asCALL_CDECL_OBJLAST,
			*(asFunctionCaller*)&ConstructorCaller);
		const int SetterId = ScriptEngine->RegisterObjectMethod(
			"FloatWrapper",
			"void SetValue(double InValue)",
			asMETHODPR(
				ObjectRegistrationTest::FFloatWrapper,
				SetValue,
				(double),
				void),
			asCALL_THISCALL,
			*(asFunctionCaller*)&SetValueCaller);
		const int AddId = ScriptEngine->RegisterObjectMethod(
			"FloatWrapper",
			"double Add(const FloatWrapper &in Other) const",
			asMETHODPR(
				ObjectRegistrationTest::FFloatWrapper,
				Add,
				(const ObjectRegistrationTest::FFloatWrapper&) const,
				double),
			asCALL_THISCALL,
			*(asFunctionCaller*)&AddCaller);
		ASSERT_THAT(IsTrue(TypeResult >= 0, TEXT("Native float wrapper test should register its value type")));
		ASSERT_THAT(IsTrue(ConstructorId >= 0, TEXT("Native float wrapper test should register construction")));
		ASSERT_THAT(IsTrue(SetterId >= 0, TEXT("Native float wrapper test should register the double-backed value method with its automatic caller")));
		ASSERT_THAT(IsTrue(AddId >= 0, TEXT("Native float wrapper test should register the double-backed addition method with its automatic caller")));
		asITypeInfo* const WrapperType = ScriptEngine->GetTypeInfoByDecl("FloatWrapper");
		ASSERT_THAT(IsNotNull(
			WrapperType,
			TEXT("Native float wrapper test should publish its value-type metadata")));
		if (WrapperType != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(sizeof(ObjectRegistrationTest::FFloatWrapper)),
				WrapperType->GetSize(),
				TEXT("Native float wrapper test should preserve its native size")));
		}
		asIScriptFunction* const Setter = ScriptEngine->GetFunctionById(SetterId);
		asIScriptFunction* const Add = ScriptEngine->GetFunctionById(AddId);
		ASSERT_THAT(IsNotNull(
			Setter,
			TEXT("Native float wrapper test should publish its returned setter ID")));
		ASSERT_THAT(IsNotNull(
			Add,
			TEXT("Native float wrapper test should publish its returned addition ID")));
		if (Setter != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("void SetValue(float)")),
				FString(UTF8_TO_TCHAR(Setter->GetDeclaration(false, false, false))),
				TEXT("Native float wrapper test should publish the fork-canonical setter declaration")));
		}
		if (Add != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("float Add(const FloatWrapper&in) const")),
				FString(UTF8_TO_TCHAR(Add->GetDeclaration(false, false, false))),
				TEXT("Native float wrapper test should publish the fork-canonical addition declaration")));
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			double Entry()
			{
				FloatWrapper Left;
				FloatWrapper Right;
				Left.SetValue(10.0);
				Right.SetValue(2.5);
				return Left.Add(Right);
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("EMBED-OBJECT-REGISTRATION-CONTRACTS-DOUBLE-VALUE"),
			TEXT("NativeFloatWrapper"),
			UTF8_TO_TCHAR(Source.c_str()));
		FScopedNativeModule Module(*TestRunner, Engine, "NativeFloatWrapper", Source);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByExactDecl(Module, "double Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Native float wrapper test should resolve its exact entry")));
		if (Function == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("Native float wrapper test should create a context")));
			if (Context == nullptr)
			{
				return;
			}

			ON_SCOPE_EXIT
			{
				Context->Release();
			};
			const int ExecuteResult = PrepareAndExecute(Context, Function);
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecuteResult, TEXT("Native float wrapper test should execute double-backed value methods when their automatic callers are provided")));
			ASSERT_THAT(IsNear(12.5, Context->GetReturnDouble(), 0.0001, TEXT("Native float wrapper test should preserve the double-backed method return")));
		}
		ASSERT_THAT(IsNear(2.5, ObjectRegistrationTest::LastAssignedValue, 0.0001, TEXT("Native float wrapper test should retain the last registered setter argument")));
		ASSERT_THAT(IsNear(12.5, ObjectRegistrationTest::LastAddedValue, 0.0001, TEXT("Native float wrapper test should invoke the registered addition method")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("Native float wrapper test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeFloatWrapper", asGM_ONLY_IF_EXISTS),
			TEXT("Native float wrapper module should be absent after discard")));

		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Native float wrapper test should create an independent engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(IsNull(
				IndependentEngine.Get()->GetTypeInfoByDecl("FloatWrapper"),
				TEXT("Native float wrapper registration should remain engine-local")));
		}
	}

	TEST_METHOD(NativeMethodWithoutAutomaticCaller)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-OBJECT-REGISTRATION-CONTRACTS",
			"missing_caller");

		ObjectRegistrationTest::MissingCallerInvocationCount = 0;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Native caller-requirement test should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ObjectRegistrationTest::ConstructCallerRequirementCounter);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType("CallerRequirementCounter", sizeof(ObjectRegistrationTest::FCallerRequirementCounter), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ObjectRegistrationTest::FCallerRequirementCounter>() | asOBJ_APP_CLASS_ALLINTS) >= 0, TEXT("Native caller-requirement test should register its value type")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour("CallerRequirementCounter", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ObjectRegistrationTest::ConstructCallerRequirementCounter), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ConstructorCaller) >= 0, TEXT("Native caller-requirement test should register its constructor with an automatic caller")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty("CallerRequirementCounter", "int Value", asOFFSET(ObjectRegistrationTest::FCallerRequirementCounter, Value)) >= 0, TEXT("Native caller-requirement test should register the state property")));
		const int AddMethodId = ScriptEngine->RegisterObjectMethod(
			"CallerRequirementCounter",
			"int Add(int Delta) const",
			asMETHODPR(
				ObjectRegistrationTest::FCallerRequirementCounter,
				Add,
				(int32) const,
				int32),
			asCALL_THISCALL);
		ASSERT_THAT(IsTrue(
			AddMethodId >= 0,
			TEXT("Native caller-requirement test should permit registering a method without an automatic caller")));
		asITypeInfo* const CounterType =
			ScriptEngine->GetTypeInfoByDecl("CallerRequirementCounter");
		ASSERT_THAT(IsNotNull(
			CounterType,
			TEXT("Native caller-requirement test should publish its type metadata")));
		if (CounterType != nullptr)
		{
			asIScriptFunction* const AddMethod =
				ScriptEngine->GetFunctionById(AddMethodId);
			ASSERT_THAT(IsNotNull(
				AddMethod,
				TEXT("Native caller-requirement test should publish the returned missing-caller method ID")));
			if (AddMethod != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("int Add(int) const")),
					FString(UTF8_TO_TCHAR(AddMethod->GetDeclaration(false, false, false))),
					TEXT("Native caller-requirement test should preserve the exact missing-caller method declaration")));
				ASSERT_THAT(AreEqual(
					static_cast<asUINT>(1),
					AddMethod->GetParamCount(),
					TEXT("Native caller-requirement method should preserve its single integer parameter")));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asTYPEID_INT32),
					AddMethod->GetReturnTypeId(),
					TEXT("Native caller-requirement method should preserve its integer return type")));
			}
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				CallerRequirementCounter Value;
				Value.Value = 19;
				return Value.Add(23);
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("EMBED-OBJECT-REGISTRATION-CONTRACTS-MISSING-CALLER"),
			TEXT("NativeMethodWithoutAutomaticCaller"),
			UTF8_TO_TCHAR(Source.c_str()));
		FScopedNativeModule Module(*TestRunner, Engine, "NativeMethodWithoutAutomaticCaller", Source);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Native caller-requirement test should resolve its entry")));
		if (Function == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("Native caller-requirement test should create a context")));
			if (Context == nullptr)
			{
				return;
			}

			ON_SCOPE_EXIT
			{
				Context->Release();
			};
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, PrepareAndExecute(Context, Function), TEXT("Native caller-requirement test should reject a non-generic native method that lacks an automatic caller")));
			ASSERT_THAT(AreEqual(FString(TEXT("Native calling convention support is disabled. Make sure you're passing a correct Caller.")), FString(UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "")), TEXT("Native caller-requirement test should preserve the fork's missing-caller diagnostic")));
			ASSERT_THAT(IsNotNull(
				Context->GetExceptionFunction(),
				TEXT("Native caller-requirement test should retain the throwing script function")));
		}
		ASSERT_THAT(AreEqual(0, ObjectRegistrationTest::MissingCallerInvocationCount, TEXT("Native caller-requirement test should reject before the native method executes")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("Native caller-requirement test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeMethodWithoutAutomaticCaller", asGM_ONLY_IF_EXISTS),
			TEXT("Native caller-requirement module should be absent after discard")));

		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Native caller-requirement test should create an independent engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(IsNull(
				IndependentEngine.Get()->GetTypeInfoByDecl("CallerRequirementCounter"),
				TEXT("Native caller-requirement registration should remain engine-local")));
		}
	}

	TEST_METHOD(InvalidCallerConventionCombinationIsRejectedAtRegistration)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-OBJECT-REGISTRATION-CONTRACTS",
			"invalid_caller_convention");

		ObjectRegistrationTest::MissingCallerInvocationCount = 0;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Invalid caller-convention test should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"InvalidCallerOwner",
				sizeof(ObjectRegistrationTest::FCallerRequirementCounter),
				asOBJ_VALUE
					| asOBJ_POD
					| asGetTypeTraits<ObjectRegistrationTest::FCallerRequirementCounter>()
					| asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("Invalid caller-convention test should register its owner type")));

		const ASAutoCaller::FunctionCaller FreeFunctionCaller =
			ASAutoCaller::MakeFunctionCaller(ObjectRegistrationTest::InvalidThiscallFreeFunction);
		const int32 RegistrationResult = ScriptEngine->RegisterObjectMethod(
			"InvalidCallerOwner",
			"int Invalid(int Delta) const",
			asFUNCTION(ObjectRegistrationTest::InvalidThiscallFreeFunction),
			asCALL_THISCALL,
			*(asFunctionCaller*)&FreeFunctionCaller);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asWRONG_CALLING_CONV),
			RegistrationResult,
			TEXT("A free-function pointer paired with thiscall should be rejected with the stable fork result")));
		ASSERT_THAT(AreEqual(
			0,
			ObjectRegistrationTest::MissingCallerInvocationCount,
			TEXT("Rejected caller/convention combinations should never execute the native target")));
		asITypeInfo* const OwnerType = ScriptEngine->GetTypeInfoByDecl("InvalidCallerOwner");
		ASSERT_THAT(IsNotNull(
			OwnerType,
			TEXT("Invalid caller-convention test should retain its valid owner type")));
		if (OwnerType != nullptr)
		{
			ASSERT_THAT(IsNull(
				OwnerType->GetMethodByDecl("int Invalid(int) const"),
				TEXT("Rejected caller/convention combinations should not publish a method")));
		}
	}
};

#endif
