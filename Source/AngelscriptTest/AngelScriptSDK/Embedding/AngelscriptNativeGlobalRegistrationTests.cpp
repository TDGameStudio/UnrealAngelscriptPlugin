#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace GlobalRegistrationTest
{
	static int32 DoubleValue(int32 Value)
	{
		return Value * 2;
	}
}

TEST_CLASS_WITH_FLAGS(FGlobalRegistrationTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.GlobalRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GlobalRegistrationGlobalFunction)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("EMBED-GLOBAL-REGISTRATION-SURFACES",
			ENativeEvidence::Compile
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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Global function registration should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(GlobalRegistrationTest::DoubleValue);
		const int FunctionId = ScriptEngine->RegisterGlobalFunction(
			"int DoubleValue(int Value)",
			asFUNCTION(GlobalRegistrationTest::DoubleValue),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(FunctionId >= 0,
			TEXT("Global function registration should accept the native declaration")));

		asIScriptFunction* const RegisteredFunction = ScriptEngine->GetFunctionById(FunctionId);
		ASSERT_THAT(IsNotNull(
			RegisteredFunction,
			TEXT("Global function registration should publish its returned function ID")));
		if (RegisteredFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FunctionId,
				RegisteredFunction->GetId(),
				TEXT("Global function registration should return its published function ID")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("int DoubleValue(int)")),
				FString(UTF8_TO_TCHAR(RegisteredFunction->GetDeclaration())),
				TEXT("Global function registration should preserve its system-function declaration")));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(1),
				RegisteredFunction->GetParamCount(),
				TEXT("Global function registration should preserve its single parameter")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asTYPEID_INT32),
				RegisteredFunction->GetReturnTypeId(),
				TEXT("Global function registration should preserve its integer return type")));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetGlobalFunctionByDecl(RegisteredFunction->GetDeclaration()),
				TEXT("Current-fork global declaration lookup should expose its system/script parameter-normalization mismatch")));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetGlobalFunctionByDecl("int DoubleValue(const int)"),
				TEXT("Current-fork global declaration lookup should not resolve the registered system function through the script-normalized spelling")));
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return DoubleValue(21);
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("EMBED-GLOBAL-REGISTRATION-SURFACES-FUNCTION"),
			TEXT("GlobalFunctionRegistration"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"GlobalFunctionRegistration",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Global function registration should resolve the script entry")));
		if (Function == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("Global function registration should create a context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Registered global function should execute from script")));
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Registered global function should marshal its return value")));
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("Global function registration should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("GlobalFunctionRegistration", asGM_ONLY_IF_EXISTS),
			TEXT("Global function registration module should be absent after discard")));

		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Global function registration should create an independent raw SDK engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(1),
				IndependentEngine.Get()->GetGlobalFunctionCount(),
				TEXT("Independent global-registration engine should retain only the support assert(bool) registration")));
			asIScriptFunction* const SupportFunction =
				IndependentEngine.Get()->GetGlobalFunctionByIndex(0);
			ASSERT_THAT(IsNotNull(
				SupportFunction,
				TEXT("Independent global-registration engine should publish its sole support registration")));
			if (SupportFunction != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("assert")),
					FString(UTF8_TO_TCHAR(SupportFunction->GetName())),
					TEXT("Independent global-registration engine should not contain DoubleValue or another product registration")));
			}
		}
	}

	TEST_METHOD(GlobalRegistrationGlobalProperty)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-GLOBAL-REGISTRATION-SURFACES",
			"property");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		int32 NativeValue = 21;
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Global property registration should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const int PropertyIndex = ScriptEngine->RegisterGlobalProperty("int NativeValue", &NativeValue);
		ASSERT_THAT(IsTrue(
			PropertyIndex >= 0,
			TEXT("Global property registration should accept the native address")));

		const char* PropertyName = nullptr;
		const char* PropertyNamespace = nullptr;
		int TypeId = asTYPEID_VOID;
		bool bIsConst = true;
		void* PropertyAddress = nullptr;
		const int MetadataResult = ScriptEngine->GetGlobalPropertyByIndex(
			static_cast<asUINT>(PropertyIndex),
			&PropertyName,
			&PropertyNamespace,
			&TypeId,
			&bIsConst,
			nullptr,
			&PropertyAddress);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			MetadataResult,
			TEXT("Global property registration should expose indexed metadata")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("NativeValue")),
			FString(UTF8_TO_TCHAR(PropertyName != nullptr ? PropertyName : "")),
			TEXT("Global property registration should preserve its name")));
		ASSERT_THAT(AreEqual(
			FString(),
			FString(UTF8_TO_TCHAR(PropertyNamespace != nullptr ? PropertyNamespace : "")),
			TEXT("Global property registration should preserve the root namespace")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asTYPEID_INT32),
			TypeId,
			TEXT("Global property registration should preserve its int type ID")));
		ASSERT_THAT(IsFalse(bIsConst, TEXT("Global property registration should remain mutable")));
		ASSERT_THAT(AreEqual(
			static_cast<const void*>(&NativeValue),
			static_cast<const void*>(PropertyAddress),
			TEXT("Global property registration should expose the exact native address")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				NativeValue += 1;
				return NativeValue * 2;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("EMBED-GLOBAL-REGISTRATION-SURFACES-PROPERTY"),
			TEXT("GlobalPropertyRegistration"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"GlobalPropertyRegistration",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByExactDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Global property registration should resolve the script entry")));
		if (Function == nullptr)
		{
			return;
		}

		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context, TEXT("Global property registration should create a context")));
			if (Context == nullptr)
			{
				return;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Registered global property should execute from script")));
			ASSERT_THAT(AreEqual(44, static_cast<int32>(Context->GetReturnDWord()), TEXT("Registered global property should preserve native address mutation")));
		}
		ASSERT_THAT(AreEqual(22, NativeValue, TEXT("Script mutation should update the registered native property")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("Global property registration should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("GlobalPropertyRegistration", asGM_ONLY_IF_EXISTS),
			TEXT("Global property registration module should be absent after discard")));

		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Global property registration should create an independent raw SDK engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				IndependentEngine.Get()->GetGlobalPropertyCount(),
				TEXT("Global property registration should not leak into an independent engine")));
		}
	}
};

#endif
