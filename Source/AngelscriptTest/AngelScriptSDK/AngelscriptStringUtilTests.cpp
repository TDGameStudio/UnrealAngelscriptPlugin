#include "AngelscriptSDKTestExecutionHelpers.h"
// AngelscriptSDKStringUtilTests.cpp
// Tests for as_string_util.cpp - number/string conversion via script.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.StringUtil.*

#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

// TODO: RegisterStringFactory API differs between AS 2.33 fork and AS 2.38.
//       These tests use the 2.38-style 3-arg RegisterStringFactory which our
//       fork doesn't support. Disabled until the API gap is resolved.
#if 0 // WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKStringUtilTests, "Angelscript.TestModule.AngelScriptSDK.StringUtil", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void RegisterStringFactory(asIScriptEngine* SE)
	{
		SE->RegisterObjectType("string", sizeof(std::string), asOBJ_VALUE | asGetTypeTraits<std::string>());
		SE->RegisterStringFactory("string", asFUNCTION(+[](asUINT Length, const char* Data) -> std::string {
			return std::string(Data, Length);
		}), asCALL_CDECL);
		SE->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT, "void f()",
			asFUNCTION(+[](std::string* P) { new(P) std::string(); }), asCALL_CDECL_OBJFIRST);
		SE->RegisterObjectBehaviour("string", asBEHAVE_DESTRUCT, "void f()",
			asFUNCTION(+[](std::string* P) { using T = std::string; P->~T(); }), asCALL_CDECL_OBJFIRST);
		SE->RegisterObjectMethod("string", "int parseInt(uint Base = 10) const",
			asFUNCTION(+[](const std::string& S, asUINT Base) -> int {
				return (int)std::strtol(S.c_str(), nullptr, (int)Base);
			}), asCALL_CDECL_OBJFIRST);
		SE->RegisterObjectMethod("string", "double parseFloat() const",
			asFUNCTION(+[](const std::string& S) -> double {
				return std::strtod(S.c_str(), nullptr);
			}), asCALL_CDECL_OBJFIRST);
	}

public:
	TEST_METHOD(ParseInt)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };
		RegisterStringFactory(SE);
		asIScriptModule* M = BuildNativeModule(SE, "StrUtilPI", "int Entry() { string s = \"42\"; return s.parseInt(); }\n");
		if (!this->Assert.IsNotNull(M, TEXT("Should compile"))) { AddInfo(CollectMessages(Messages)); return; }
		int32 Result = 0;
		if (!ExecuteScriptFunction(*this, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(42, Result, TEXT("parseInt 42")));
	}

	TEST_METHOD(ParseNegativeInt)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };
		RegisterStringFactory(SE);
		asIScriptModule* M = BuildNativeModule(SE, "StrUtilNI", "int Entry() { string s = \"-100\"; return s.parseInt(); }\n");
		if (!this->Assert.IsNotNull(M, TEXT("Should compile"))) { AddInfo(CollectMessages(Messages)); return; }
		int32 Result = 0;
		if (!ExecuteScriptFunction(*this, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(-100, Result, TEXT("parseInt -100")));
	}

	TEST_METHOD(ParseFloat)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };
		RegisterStringFactory(SE);
		asIScriptModule* M = BuildNativeModule(SE, "StrUtilPF", "double Entry() { string s = \"3.14\"; return s.parseFloat(); }\n");
		if (!this->Assert.IsNotNull(M, TEXT("Should compile"))) { AddInfo(CollectMessages(Messages)); return; }
		double Result = 0.0;
		if (!ExecuteScriptFunction(*this, SE, M, "double Entry()", Result)) return;
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 3.14, 0.001), TEXT("parseFloat 3.14")));
	}

	TEST_METHOD(ParseZero)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };
		RegisterStringFactory(SE);
		asIScriptModule* M = BuildNativeModule(SE, "StrUtilZ", "int Entry() { string s = \"0\"; return s.parseInt(); }\n");
		if (!this->Assert.IsNotNull(M, TEXT("Should compile"))) { AddInfo(CollectMessages(Messages)); return; }
		int32 Result = -1;
		if (!ExecuteScriptFunction(*this, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(0, Result, TEXT("parseInt 0")));
	}

	TEST_METHOD(LargeValue)
	{
		using namespace AngelscriptNativeTestSupport;

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };
		RegisterStringFactory(SE);
		asIScriptModule* M = BuildNativeModule(SE, "StrUtilLV", "int Entry() { string s = \"2147483647\"; return s.parseInt(); }\n");
		if (!this->Assert.IsNotNull(M, TEXT("Should compile"))) { AddInfo(CollectMessages(Messages)); return; }
		int32 Result = 0;
		if (!ExecuteScriptFunction(*this, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(2147483647, Result, TEXT("parseInt INT32_MAX")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
