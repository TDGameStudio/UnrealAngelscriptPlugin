#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptSDKTestExecutionHelpers.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptModuleImportTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptModule.Import",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

	TEST_METHOD(ImportMetadataBeforeBinding)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import metadata test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportMetadataConsumer", R"(
import int SharedValue() from "ScriptModuleImportMetadataProvider";

int Entry()
{
	return SharedValue();
}
)");
		if (!Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		TestRunner->TestEqual(TEXT("ScriptModule import metadata should expose one imported function"), static_cast<int32>(Consumer->GetImportedFunctionCount()), 1);
		TestRunner->TestEqual(TEXT("ScriptModule import metadata should resolve the import index by declaration"), Consumer->GetImportedFunctionIndexByDecl("int SharedValue()"), 0);
		TestRunner->TestEqual(TEXT("ScriptModule import metadata should preserve the imported declaration"), FString(UTF8_TO_TCHAR(Consumer->GetImportedFunctionDeclaration(0))), FString(TEXT("int SharedValue()")));
		TestRunner->TestEqual(TEXT("ScriptModule import metadata should preserve the source module name"), FString(UTF8_TO_TCHAR(Consumer->GetImportedFunctionSourceModule(0))), FString(TEXT("ScriptModuleImportMetadataProvider")));
	}

	TEST_METHOD(BindImportedFunctionExecutesProvider)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import bind test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Provider(*TestRunner, Engine, "ScriptModuleImportBindProvider", R"(
int SharedValue()
{
	return 77;
}
)");
		FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportBindConsumer", R"(
import int SharedValue() from "ScriptModuleImportBindProvider";

int Entry()
{
	return SharedValue();
}
)");
		if (!Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* SourceFunction = Provider->GetFunctionByDecl("int SharedValue()");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import bind test should expose the provider function"), SourceFunction))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("ScriptModule import bind test should bind the imported function"), Consumer->BindImportedFunction(0, SourceFunction), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Consumer, "int Entry()", Result))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("ScriptModule import bind test should execute the provider function through the consumer"), Result, 77);
	}

	TEST_METHOD(BindImportedFunctionRejectsSignatureMismatch)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import mismatch test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Provider(*TestRunner, Engine, "ScriptModuleImportMismatchProvider", R"(
int SharedValue(int Value)
{
	return Value;
}
)");
		FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportMismatchConsumer", R"(
import int SharedValue() from "ScriptModuleImportMismatchProvider";

int Entry()
{
	return SharedValue();
}
)");
		if (!Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* SourceFunction = GetNativeFunctionByDecl(Provider, "int SharedValue(int)");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import mismatch test should expose the mismatched provider function"), SourceFunction))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("ScriptModule import mismatch test should use a one-parameter provider"), static_cast<int32>(SourceFunction->GetParamCount()), 1))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("ScriptModule import mismatch test should reject incompatible signatures"), Consumer->BindImportedFunction(0, SourceFunction), static_cast<int32>(asINVALID_INTERFACE));
		TestRunner->TestEqual(TEXT("ScriptModule import mismatch test should reject an invalid import index"), Consumer->BindImportedFunction(7, SourceFunction), static_cast<int32>(asINVALID_ARG));
	}

	TEST_METHOD(UnbindImportedFunctionAllowsRebind)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import rebind test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule ProviderA(*TestRunner, Engine, "ScriptModuleImportRebindProviderA", R"(
int SharedValue()
{
	return 11;
}
)");
		FScopedNativeModule ProviderB(*TestRunner, Engine, "ScriptModuleImportRebindProviderB", R"(
int SharedValue()
{
	return 29;
}
)");
		FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportRebindConsumer", R"(
import int SharedValue() from "ScriptModuleImportRebindProviderA";

int Entry()
{
	return SharedValue();
}
)");
		if (!ProviderA.IsValid() || !ProviderB.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* FirstFunction = ProviderA->GetFunctionByDecl("int SharedValue()");
		asIScriptFunction* SecondFunction = ProviderB->GetFunctionByDecl("int SharedValue()");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import rebind test should expose the first provider function"), FirstFunction) ||
			!TestRunner->TestNotNull(TEXT("ScriptModule import rebind test should expose the second provider function"), SecondFunction))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("ScriptModule import rebind test should bind the first provider"), Consumer->BindImportedFunction(0, FirstFunction), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		int32 FirstResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Consumer, "int Entry()", FirstResult))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("ScriptModule import rebind test should execute the first provider"), FirstResult, 11);

		if (!TestRunner->TestEqual(TEXT("ScriptModule import rebind test should unbind the import"), Consumer->UnbindImportedFunction(0), static_cast<int32>(asSUCCESS)) ||
			!TestRunner->TestEqual(TEXT("ScriptModule import rebind test should bind the second provider"), Consumer->BindImportedFunction(0, SecondFunction), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		int32 SecondResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Consumer, "int Entry()", SecondResult))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("ScriptModule import rebind test should execute the rebound provider"), SecondResult, 29);
	}

	TEST_METHOD(BindAllImportedFunctions)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import bind-all test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName MissingScope(Engine, "ScriptModuleImportBindAllMissingConsumer");
		asIScriptModule* MissingConsumer = BuildNativeModule(ScriptEngine, MissingScope.Get(), R"(
import int SharedValue() from "ScriptModuleImportBindAllMissingProvider";

int Entry()
{
	return SharedValue();
}
)");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule import bind-all test should compile the missing-provider consumer"), MissingConsumer))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		TestRunner->TestEqual(TEXT("ScriptModule import bind-all test should fail when provider is missing"), MissingConsumer->BindAllImportedFunctions(), static_cast<int32>(asCANT_BIND_ALL_FUNCTIONS));

		FScopedNativeModule Provider(*TestRunner, Engine, "ScriptModuleImportBindAllProvider", R"(
int SharedValue()
{
	return 42;
}
)");
		FScopedNativeModule Consumer(*TestRunner, Engine, "ScriptModuleImportBindAllConsumer", R"(
import int SharedValue() from "ScriptModuleImportBindAllProvider";

int Entry()
{
	return SharedValue();
}
)");
		if (!Provider.IsValid() || !Consumer.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		if (!TestRunner->TestEqual(TEXT("ScriptModule import bind-all test should bind all matching imports"), Consumer->BindAllImportedFunctions(), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Consumer, "int Entry()", Result))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("ScriptModule import bind-all test should execute after automatic binding"), Result, 42);
	}
};

#endif
