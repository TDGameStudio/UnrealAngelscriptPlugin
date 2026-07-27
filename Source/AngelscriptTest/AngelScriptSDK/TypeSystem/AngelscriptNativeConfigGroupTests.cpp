// Raw SDK configuration-group coverage.
// Tests for as_configgroup.cpp - type registration group management.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.TypeSystem.ConfigGroup.*

#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FConfigGroupTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.ConfigGroup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int ReturnNinetyNine() { return 99; }
	static int ReturnOne() { return 1; }

public:
	TEST_METHOD(ConfigGroupBeginEnd)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("TYPE-CONFIG-GROUP-STORAGE-ONLY",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		const struct
		{
			const TCHAR* Id;
			const TCHAR* Operation;
			const TCHAR* Expected;
		} ReviewCases[] =
		{
			{ TEXT("begin"), TEXT("BeginConfigGroup(\"TestGroup\")"), TEXT("asSUCCESS") },
			{ TEXT("register"), TEXT("RegisterGlobalFunction(\"int TestGroupFunc()\")"), TEXT("published exact function") },
			{ TEXT("end"), TEXT("EndConfigGroup()"), TEXT("asSUCCESS") },
			{ TEXT("nested"), TEXT("BeginConfigGroup(\"Outer\"); BeginConfigGroup(\"Inner\")"), TEXT("both asSUCCESS because current fork stores no active group") },
			{ TEXT("remove"), TEXT("RemoveConfigGroup(\"TestGroup\")"), TEXT("asSUCCESS") },
			{ TEXT("persistence"), TEXT("compile and execute TestGroupFunc after removal"), TEXT("returns 99 because current fork removal is a no-op") },
		};
		for (const auto& ReviewCase : ReviewCases)
		{
			FString ReviewSource;
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// Operation: %s"), ReviewCase.Operation));
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// Expected: %s"), ReviewCase.Expected));
			PrintGeneratedAsSource(
				*TestRunner,
				MakeNativeCaseId("TYPE-CONFIG-GROUP-STORAGE-ONLY", { ReviewCase.Id }),
				TEXT("TypeConfigGroupNativeReview"),
				ReviewSource);
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SE->BeginConfigGroup("TestGroup"),
			TEXT("Current-fork config-group begin should report success")));
		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(ReturnNinetyNine);
		const int FunctionId = SE->RegisterGlobalFunction(
			"int TestGroupFunc()",
			asFUNCTION(ReturnNinetyNine),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		ASSERT_THAT(IsTrue(
			FunctionId >= 0,
			TEXT("Config-group product should register its exact native function")));
		asIScriptFunction* const RegisteredFunction = SE->GetFunctionById(FunctionId);
		ASSERT_THAT(IsNotNull(
			RegisteredFunction,
			TEXT("Config-group product should resolve the registration-returned function")));
		if (RegisteredFunction != nullptr)
		{
			ASSERT_THAT(IsNull(
				RegisteredFunction->GetConfigGroup(),
				TEXT("Current fork should publish no function configuration-group metadata")));
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SE->EndConfigGroup(),
			TEXT("Current-fork config-group end should report success")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SE->BeginConfigGroup("Outer"),
			TEXT("First nested config-group begin should report success")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SE->BeginConfigGroup("Inner"),
			TEXT("Second nested config-group begin should also report success in the storage-only fork")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SE->EndConfigGroup(),
			TEXT("First nested config-group end should report success")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SE->EndConfigGroup(),
			TEXT("Second nested config-group end should report success")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SE->RemoveConfigGroup("TestGroup"),
			TEXT("Current-fork config-group removal should report success")));

		const FString ScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return TestGroupFunc();
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("TYPE-CONFIG-GROUP-STORAGE-ONLY-PERSISTENCE-SOURCE"),
			TEXT("TypeConfigGroupPersistence"),
			ScriptSource);
		const FTCHARToUTF8 ScriptSourceUtf8(*ScriptSource);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypeConfigGroupPersistence",
			ScriptSourceUtf8.Get());
		ASSERT_THAT(IsTrue(
			Module.IsValid(),
			TEXT("Config-group function should remain compilable after no-op removal")));
		if (!Module.IsValid())
		{
			return;
		}

		{
			FSdkFunctionInvoker Invoker(
				*TestRunner,
				SE,
				Module,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("Config-group persistence product should resolve its exact entry")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					99,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Removed current-fork config group should leave the registered function executable")));
			}
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module.Discard(),
			TEXT("Config-group persistence product should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			SE->GetModule(
				"TypeConfigGroupPersistence",
				asGM_ONLY_IF_EXISTS),
			TEXT("Config-group persistence module should be absent after cleanup")));

		FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};
		asIScriptEngine* const ControlScriptEngine = ControlEngine.Get();
		ASSERT_THAT(IsNotNull(
			ControlScriptEngine,
			TEXT("Config-group isolation should create an independent control engine")));
		if (ControlScriptEngine != nullptr)
		{
			ASSERT_THAT(IsNull(
				ControlScriptEngine->GetGlobalFunctionByDecl(
					"int TestGroupFunc()"),
				TEXT("Config-group registration should remain isolated from an independent engine")));
		}

		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] BeginConfigGroup, EndConfigGroup, RemoveConfigGroup, and FindConfigGroupForFunction are storage-only/no-op stubs in the current fork"));
	}

	TEST_METHOD(RemoveCleansTypes)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-CONFIG-GROUP-STORAGE-ONLY supersedes this permissive remove smoke with exact no-op metadata, persistence, execution, and nested-call assertions");

		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		SE->BeginConfigGroup("RemovableGroup");
		SE->RegisterGlobalFunction("int RemovableFunc()", asFUNCTION(ReturnOne), asCALL_CDECL);
		SE->EndConfigGroup();

		int R = SE->RemoveConfigGroup("RemovableGroup");
		ASSERT_THAT(IsTrue(R >= 0, TEXT("RemoveConfigGroup should succeed")));

		// After removal, function should not be available
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "AfterRemove");
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return RemovableFunc();
			}
			)AS");
		asIScriptModule* M = BuildNativeModule(SE, "AfterRemove", ScriptSource);
		// Note: In the current AS 2.33 fork, RemoveConfigGroup may or may not
		// fully clean up — we just verify the call itself succeeds.
		// If the function is still accessible, that's acceptable behavior for this fork.
		if (M != nullptr)
		{
			TestRunner->AddInfo(TEXT("RemoveConfigGroup did not fully clean up function bindings (acceptable in AS 2.33 fork)"));
		}
	}

	TEST_METHOD(ConfigGroupNestedError)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-CONFIG-GROUP-STORAGE-ONLY supersedes this log-only nested probe with an exact enabled storage-only contract");

		asIScriptEngine* const SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		int R1 = SE->BeginConfigGroup("Outer");
		ASSERT_THAT(IsTrue(R1 >= 0, TEXT("First BeginConfigGroup should succeed")));

		// Nested begin — behavior depends on AS engine version.
		// In AS 2.33 fork, nested config groups may be allowed.
		int R2 = SE->BeginConfigGroup("Inner");
		// Just verify it doesn't crash; the return value is engine-specific.
		TestRunner->AddInfo(FString::Printf(TEXT("Nested BeginConfigGroup returned %d"), R2));

		// Clean up: end all opened config groups
		SE->EndConfigGroup();
		if (R2 >= 0)
		{
			SE->EndConfigGroup();
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
