#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleBuildFailureTests, "Angelscript.TestModule.AngelScriptSDK.Module.BuildFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asIScriptModule* CreateScriptModule(asIScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return ScriptEngine != nullptr
			? ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE)
			: nullptr;
	}

	static FString FormatPointer(const void* Pointer)
	{
		return FString::Printf(TEXT("%p"), Pointer);
	}

	static FString DescribeObjectTypes(asIScriptModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		const asUINT TypeCount = Module->GetObjectTypeCount();
		for (asUINT TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			asITypeInfo* TypeInfo = Module->GetObjectTypeByIndex(TypeIndex);
			if (TypeInfo == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += UTF8_TO_TCHAR(TypeInfo->GetName());
		}

		return Result.IsEmpty() ? TEXT("<no object types>") : Result;
	}

	static FString DescribeGlobals(asIScriptModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		const asUINT GlobalCount = Module->GetGlobalVarCount();
		for (asUINT GlobalIndex = 0; GlobalIndex < GlobalCount; ++GlobalIndex)
		{
			const char* Declaration = Module->GetGlobalVarDeclaration(GlobalIndex, true);
			if (Declaration == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += UTF8_TO_TCHAR(Declaration);
		}

		return Result.IsEmpty() ? TEXT("<no globals>") : Result;
	}

	static FString DescribeTypeInfoList(asIScriptModule* Module, asUINT Count, asITypeInfo* (asIScriptModule::*Getter)(asUINT) const, const TCHAR* EmptyText)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		for (asUINT TypeIndex = 0; TypeIndex < Count; ++TypeIndex)
		{
			asITypeInfo* TypeInfo = (Module->*Getter)(TypeIndex);
			if (TypeInfo == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			const char* Namespace = TypeInfo->GetNamespace();
			if (Namespace != nullptr && Namespace[0] != '\0')
			{
				Result += UTF8_TO_TCHAR(Namespace);
				Result += TEXT("::");
			}
			Result += UTF8_TO_TCHAR(TypeInfo->GetName());
		}

		return Result.IsEmpty() ? EmptyText : Result;
	}

	static int32 FindGlobalVarIndexByName(asIScriptModule* Module, const char* Name)
	{
		if (Module == nullptr || Name == nullptr)
		{
			return INDEX_NONE;
		}

		const asUINT GlobalCount = Module->GetGlobalVarCount();
		for (asUINT GlobalIndex = 0; GlobalIndex < GlobalCount; ++GlobalIndex)
		{
			const char* GlobalName = nullptr;
			if (Module->GetGlobalVar(GlobalIndex, &GlobalName) >= 0 &&
				GlobalName != nullptr &&
				FCStringAnsi::Strcmp(GlobalName, Name) == 0)
			{
				return static_cast<int32>(GlobalIndex);
			}
		}

		return INDEX_NONE;
	}

	static asIScriptFunction* FindFunctionByNameAndNamespace(asIScriptModule* Module, const char* Name, const char* Namespace)
	{
		if (Module == nullptr || Name == nullptr || Namespace == nullptr)
		{
			return nullptr;
		}

		const asUINT FunctionCount = Module->GetFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(FunctionIndex);
			if (Function != nullptr &&
				FCStringAnsi::Strcmp(Function->GetName(), Name) == 0 &&
				FCStringAnsi::Strcmp(Function->GetNamespace(), Namespace) == 0)
			{
				return Function;
			}
		}

		return nullptr;
	}

	static void LogModuleState(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptModule* Module, const TCHAR* Stage)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		Test.AddInfo(FString::Printf(
			TEXT("ScriptModule state [%s]: engineModuleCount=%u module=%s name=%s defaultNamespace=%s functions={%s} globals={%s} objectTypes={%s} enums={%s} typedefs={%s} imports=%u"),
			Stage != nullptr ? Stage : TEXT("<unknown>"),
			ScriptEngine != nullptr ? ScriptEngine->GetModuleCount() : 0,
			*FormatPointer(Module),
			Module != nullptr ? UTF8_TO_TCHAR(Module->GetName()) : TEXT("<null>"),
			Module != nullptr ? UTF8_TO_TCHAR(Module->GetDefaultNamespace()) : TEXT("<null>"),
			*CollectFunctionDeclarations(Module),
			*DescribeGlobals(Module),
			*DescribeObjectTypes(Module),
			*DescribeTypeInfoList(Module, Module != nullptr ? Module->GetEnumCount() : 0, &asIScriptModule::GetEnumByIndex, TEXT("<no enums>")),
			*DescribeTypeInfoList(Module, Module != nullptr ? Module->GetTypedefCount() : 0, &asIScriptModule::GetTypedefByIndex, TEXT("<no typedefs>")),
			Module != nullptr ? Module->GetImportedFunctionCount() : 0));
	}

public:

	TEST_METHOD(FailedBuildDoesNotPublishPartialModuleTablesAndCanRecover)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("MOD-BUILD-FAILURE-RECOVERY",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule failed build recovery test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleFailedBuildRecovery");
		asIScriptModule* FailedModule = CreateScriptModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(FailedModule, TEXT("ScriptModule failed build recovery test should create the failed-build module")));

		const std::string BrokenSource = ASTEST_AS_ANSI(R"AS(
			class BrokenState
			{
				int Value;
			}

			int BrokenEntry()
			{
				return MissingSymbol + 1;
			}
			)AS");
		ASSERT_THAT(IsTrue(
			FailedModule->AddScriptSection("ScriptModuleFailedBuildRecovery_Broken", BrokenSource.c_str(), BrokenSource.length(), 0) >= 0,
			TEXT("ScriptModule failed build recovery test should add broken source")));

		const int FailedBuildResult = FailedModule->Build();
		TestRunner->AddInfo(Engine.GetMessagesText());
		LogModuleState(*TestRunner, ScriptEngine, FailedModule, TEXT("failed-build-after-build"));
		ASSERT_THAT(IsTrue(FailedBuildResult < 0, TEXT("ScriptModule failed build recovery test should reject the broken source")));
		ASSERT_THAT(IsNull(FailedModule->GetFunctionByDecl("int BrokenEntry()"), TEXT("ScriptModule failed build recovery test should not expose BrokenEntry after failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(FailedModule->GetFunctionCount()), TEXT("ScriptModule failed build recovery test should not publish functions after failure")));

		const int DiscardResult = ScriptEngine->DiscardModule(ModuleScope.Get());
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), DiscardResult, TEXT("ScriptModule failed build recovery test should discard the failed module")));

		const std::string RecoverySource = ASTEST_AS_ANSI(R"AS(
			const int RecoveryValue = 42;

			class RecoveryState
			{
				int Value;
			}

			int Entry()
			{
				return RecoveryValue;
			}
			)AS");
		asIScriptModule* RecoveryModule = BuildNativeModule(ScriptEngine, ModuleScope.Get(), RecoverySource.c_str());
		if (RecoveryModule == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(RecoveryModule, TEXT("ScriptModule failed build recovery test should build the recovery module")));
		LogModuleState(*TestRunner, ScriptEngine, RecoveryModule, TEXT("failed-build-recovery-after-build"));

		int32 RecoveryResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, RecoveryModule, "int Entry()", RecoveryResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, RecoveryResult, TEXT("ScriptModule failed build recovery test should execute after recovery")));
		ASSERT_THAT(IsNotNull(RecoveryModule->GetFunctionByDecl("int Entry()"), TEXT("ScriptModule failed build recovery test should expose the recovery function")));
		ASSERT_THAT(IsNotNull(RecoveryModule->GetTypeInfoByDecl("RecoveryState"), TEXT("ScriptModule failed build recovery test should expose the recovery type")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(RecoveryModule->GetGlobalVarCount()), TEXT("ScriptModule failed build recovery test should expose one recovery global")));
		ASSERT_THAT(IsNull(RecoveryModule->GetTypeInfoByDecl("BrokenState"), TEXT("ScriptModule failed build recovery test should not expose BrokenState after recovery")));
	}
};

#endif
