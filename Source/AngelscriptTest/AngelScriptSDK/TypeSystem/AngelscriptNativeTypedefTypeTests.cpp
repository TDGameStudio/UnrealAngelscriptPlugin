#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedefTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.Typedefs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(TypedefTypeTypedefBytecode)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeMessageCollector SaveMessages;
		asIScriptEngine* const SaveEngine = CreateNativeEngine(&SaveMessages);
		ASSERT_THAT(IsNotNull(SaveEngine, TEXT("Typedef bytecode test should create a save engine")));
		if (SaveEngine == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { DestroyNativeEngine(SaveEngine); };

		ASSERT_THAT(IsTrue(SaveEngine->RegisterTypedef("TestType1", "int8") >= 0, TEXT("Typedef bytecode test should register TestType1 on the save engine")));
		ASSERT_THAT(IsTrue(SaveEngine->RegisterTypedef("TestType4", "int64") >= 0, TEXT("Typedef bytecode test should register TestType4 on the save engine")));
		asIScriptModule* const SaveModule = BuildNativeModule(SaveEngine, "TypedefTypeSave", ASTEST_AS_ANSI(R"AS(
			TestType4 Func(TestType1 value)
			{
				return value;
			}

			int ReturnTypedefRoundTrip()
			{
				TestType1 value = 1;
				return int(Func(value));
			}
			)AS"));
		ASSERT_THAT(IsNotNull(SaveModule, TEXT("Typedef bytecode test should compile the save module")));
		if (SaveModule == nullptr)
		{
			TestRunner->AddInfo(CollectMessages(SaveMessages));
			return;
		}

		FSDKBytecodeStream Bytecode;
		ASSERT_THAT(AreEqual(asSUCCESS, SaveModule->SaveByteCode(&Bytecode), TEXT("Typedef bytecode test should save bytecode")));
		Bytecode.ResetReadPosition();

		FNativeMessageCollector LoadMessages;
		asIScriptEngine* const LoadEngine = CreateNativeEngine(&LoadMessages);
		ASSERT_THAT(IsNotNull(LoadEngine, TEXT("Typedef bytecode test should create a load engine")));
		if (LoadEngine == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { DestroyNativeEngine(LoadEngine); };

		ASSERT_THAT(IsTrue(LoadEngine->RegisterTypedef("TestType1", "int8") >= 0, TEXT("Typedef bytecode test should register TestType1 on the load engine")));
		ASSERT_THAT(IsTrue(LoadEngine->RegisterTypedef("TestType4", "int64") >= 0, TEXT("Typedef bytecode test should register TestType4 on the load engine")));
		asIScriptModule* const LoadModule = LoadEngine->GetModule("TypedefTypeLoad", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(LoadModule, TEXT("Typedef bytecode test should create a load module")));
		if (LoadModule == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asSUCCESS, LoadModule->LoadByteCode(&Bytecode), TEXT("Typedef bytecode test should load bytecode")));
		ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(LoadModule, "int ReturnTypedefRoundTrip()"),
			TEXT("Typedef bytecode test should preserve the loaded entry function")));
	}
};

#endif
