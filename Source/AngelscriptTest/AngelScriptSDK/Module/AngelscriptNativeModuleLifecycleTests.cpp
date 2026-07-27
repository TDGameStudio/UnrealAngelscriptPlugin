#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleLifecycleTests, "Angelscript.TestModule.AngelScriptSDK.Module.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

	TEST_METHOD(ModuleLifecycleCreate)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("MOD-LIFECYCLE-REBUILD-ISOLATION",
			ENativeEvidence::Compile
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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule create test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleCreate");
		asIScriptModule* Module = CreateScriptModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("ScriptModule create test should create a module")));
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("create-after-module"));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			const int Value = 42;

			bool Entry()
			{
				return Value == 42;
			}
			)AS");
		const int AddResult = Module->AddScriptSection("ScriptModuleCreate_Entry", ScriptSource.c_str(), ScriptSource.length(), 0);
		ASSERT_THAT(IsTrue(AddResult >= 0, TEXT("ScriptModule create test should add a script section")));

		const int BuildResult = Module->Build();
		if (BuildResult != static_cast<int32>(asSUCCESS))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), BuildResult, TEXT("ScriptModule create test should build successfully")));
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("create-after-build"));

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "bool Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("ScriptModule create test should expose the entry function")));

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}
		TestRunner->AddInfo(FString::Printf(TEXT("ScriptModule create execution: Entry()=%s"), bResult ? TEXT("true") : TEXT("false")));
		ASSERT_THAT(IsTrue(bResult, TEXT("ScriptModule create test should execute the compiled entry function")));

		const asUINT BeforeDiscardCount = ScriptEngine->GetModuleCount();
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("ScriptModule create test should explicitly discard the built module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule create test should remove the discarded module from name lookup")));
		ASSERT_THAT(AreEqual(static_cast<int32>(BeforeDiscardCount), static_cast<int32>(ScriptEngine->GetModuleCount()),
			TEXT("Current fork should retain the discarded module in the indexed engine inventory")));
		bool bFoundDiscardedModuleByIndex = false;
		for (asUINT ModuleIndex = 0; ModuleIndex < ScriptEngine->GetModuleCount(); ++ModuleIndex)
		{
			bFoundDiscardedModuleByIndex |= ScriptEngine->GetModuleByIndex(ModuleIndex) == Module;
		}
		ASSERT_THAT(IsTrue(
			bFoundDiscardedModuleByIndex,
			TEXT("Current fork should retain the discarded module identity in indexed lookup")));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] DiscardModule removes name lookup but retains the module in indexed inventory until engine shutdown"));

		AngelscriptNativeTestSupport::FNativeTestEngine IsolatedEngine;
		IsolatedEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IsolatedEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(IsolatedEngine.Get(), TEXT("ScriptModule create test should create an independent engine")));
		if (IsolatedEngine.Get() == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsolatedEngine.Get() != ScriptEngine, TEXT("ScriptModule create test should isolate module state by engine")));
		ASSERT_THAT(IsNull(IsolatedEngine.Get()->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule create test should not publish its module into an independent engine")));
	}
	TEST_METHOD(ModuleLifecycleDiscard)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-LIFECYCLE-REBUILD-ISOLATION", "discard_existing");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule discard test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			const int Value = 100;

			int Entry()
			{
				return Value;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleDiscard", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("discard-after-build"));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(100, Result, TEXT("ScriptModule discard test should execute the module before discard")));

		const asUINT BeforeDiscardCount = ScriptEngine->GetModuleCount();
		asIScriptModule* const ModuleIdentity = Module;
		const int DiscardResult = ScriptEngine->DiscardModule("ScriptModuleDiscard");
		const asUINT AfterDiscardCount = ScriptEngine->GetModuleCount();

		asIScriptModule* DiscardedModule = ScriptEngine->GetModule("ScriptModuleDiscard", asGM_ONLY_IF_EXISTS);
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule discard result: return=%d beforeCount=%u afterCount=%u lookupAfterDiscard=%s"),
			DiscardResult,
			BeforeDiscardCount,
			AfterDiscardCount,
			*FormatPointer(DiscardedModule)));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), DiscardResult, TEXT("ScriptModule discard test should report success for an existing module")));
		ASSERT_THAT(IsNull(DiscardedModule, TEXT("ScriptModule discard test should remove the module")));
		ASSERT_THAT(AreEqual(static_cast<int32>(BeforeDiscardCount), static_cast<int32>(AfterDiscardCount),
			TEXT("Current fork should retain a discarded module in the indexed engine inventory")));
		bool bFoundDiscardedModuleByIndex = false;
		for (asUINT ModuleIndex = 0; ModuleIndex < AfterDiscardCount; ++ModuleIndex)
		{
			bFoundDiscardedModuleByIndex |= ScriptEngine->GetModuleByIndex(ModuleIndex) == ModuleIdentity;
		}
		ASSERT_THAT(IsTrue(
			bFoundDiscardedModuleByIndex,
			TEXT("Current fork should retain the discarded module identity in indexed lookup")));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] DiscardModule removes name lookup but retains the module in indexed inventory until engine shutdown"));
	}
	TEST_METHOD(DiscardMissingModuleReturnsNoModule)
	{
		AS_NATIVE_PRODUCT_PART("MOD-LIFECYCLE-REBUILD-ISOLATION", "discard_missing");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule missing-discard test should create a standalone SDK engine")));

		const asUINT BeforeDiscardCount = ScriptEngine->GetModuleCount();
		const int DiscardResult = ScriptEngine->DiscardModule("ScriptModuleMissingDiscard");
		const asUINT AfterDiscardCount = ScriptEngine->GetModuleCount();
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule missing-discard result: return=%d beforeCount=%u afterCount=%u"),
			DiscardResult,
			BeforeDiscardCount,
			AfterDiscardCount));
		ASSERT_THAT(AreEqual(static_cast<int32>(asNO_MODULE), DiscardResult, TEXT("Discarding an absent script module should return asNO_MODULE")));
		ASSERT_THAT(AreEqual(BeforeDiscardCount, AfterDiscardCount, TEXT("Discarding an absent script module should not change module count")));
	}
	TEST_METHOD(MultipleModulesKeepDistinctFunctions)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-LIFECYCLE-REBUILD-ISOLATION", "parallel_name_isolation");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule multi-module test should create a standalone SDK engine")));

		const std::string FirstModuleSource = ASTEST_AS_ANSI(R"AS(
			int GetValue()
			{
				return 1;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module1(*TestRunner, Engine, "ScriptModuleMulti1", FirstModuleSource.c_str());
		if (!Module1.IsValid())
		{
			return;
		}

		const std::string SecondModuleSource = ASTEST_AS_ANSI(R"AS(
			int GetValue()
			{
				return 2;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module2(*TestRunner, Engine, "ScriptModuleMulti2", SecondModuleSource.c_str());
		if (!Module2.IsValid())
		{
			return;
		}

		asIScriptFunction* FirstFunction = GetNativeFunctionByDecl(Module1, "int GetValue()");
		asIScriptFunction* SecondFunction = GetNativeFunctionByDecl(Module2, "int GetValue()");

		ASSERT_THAT(IsNotNull(FirstFunction, TEXT("ScriptModule multi-module test should expose the first module function")));
		ASSERT_THAT(IsNotNull(SecondFunction, TEXT("ScriptModule multi-module test should expose the second module function")));

		ASSERT_THAT(AreNotEqual(FirstFunction, SecondFunction, TEXT("ScriptModule multi-module test should keep module functions distinct")));

		int32 FirstResult = 0;
		int32 SecondResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module1, "int GetValue()", FirstResult) ||
			!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module2, "int GetValue()", SecondResult))
		{
			return;
		}

		LogModuleState(*TestRunner, ScriptEngine, Module1, TEXT("multi-module-first"));
		LogModuleState(*TestRunner, ScriptEngine, Module2, TEXT("multi-module-second"));
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule multi-module execution: first=%d second=%d firstFunction=%s secondFunction=%s"),
			FirstResult,
			SecondResult,
			*FormatPointer(FirstFunction),
			*FormatPointer(SecondFunction)));
		ASSERT_THAT(AreEqual(1, FirstResult, TEXT("ScriptModule multi-module test should execute the first module function")));
		ASSERT_THAT(AreEqual(2, SecondResult, TEXT("ScriptModule multi-module test should execute the second module function")));
	}
	TEST_METHOD(ModuleLifecycleRebuildModule)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-LIFECYCLE-REBUILD-ISOLATION", "always_create_replacement");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule rebuild test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleRebuild");
		const std::string ModuleV1Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 1;
			}
			)AS");
		asIScriptModule* ModuleV1 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), ModuleV1Source.c_str());
		if (ModuleV1 == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(ModuleV1, TEXT("ScriptModule rebuild test should create the initial backing module")));
		LogModuleState(*TestRunner, ScriptEngine, ModuleV1, TEXT("rebuild-v1-after-build"));
		asIScriptFunction* FunctionV1 = GetNativeFunctionByDecl(ModuleV1, "int Entry()");
		ASSERT_THAT(IsNotNull(FunctionV1, TEXT("ScriptModule rebuild test should expose the initial entry function")));

		int32 FirstResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV1, "int Entry()", FirstResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, FirstResult, TEXT("Initial script module rebuild function should return the first version")));

		const std::string ModuleV2Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 2;
			}
			)AS");
		asIScriptModule* ModuleV2 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), ModuleV2Source.c_str());
		if (ModuleV2 == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(ModuleV2, TEXT("ScriptModule rebuild test should create the rebuilt module")));
		LogModuleState(*TestRunner, ScriptEngine, ModuleV2, TEXT("rebuild-v2-after-build"));
		asIScriptFunction* FunctionV2 = GetNativeFunctionByDecl(ModuleV2, "int Entry()");
		ASSERT_THAT(IsNotNull(FunctionV2, TEXT("ScriptModule rebuild test should expose the rebuilt entry function")));

		int32 SecondResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV2, "int Entry()", SecondResult))
		{
			return;
		}
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule rebuild execution: v1=%d v2=%d moduleV1=%s moduleV2=%s functionV1=%s functionV2=%s"),
			FirstResult,
			SecondResult,
			*FormatPointer(ModuleV1),
			*FormatPointer(ModuleV2),
			*FormatPointer(FunctionV1),
			*FormatPointer(FunctionV2)));
		ASSERT_THAT(AreNotEqual(ModuleV1, ModuleV2, TEXT("Rebuilt script module should allocate a distinct module object")));
		ASSERT_THAT(AreNotEqual(FunctionV1, FunctionV2, TEXT("Rebuilt script module should allocate a distinct entry function")));
		ASSERT_THAT(AreEqual(2, SecondResult, TEXT("Rebuilt script module should execute the latest function body")));
	}
	TEST_METHOD(RecompileAfterDiscard)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-LIFECYCLE-REBUILD-ISOLATION", "discard_recompile");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule recompile test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleRecompile");
		const std::string FirstModuleSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 1;
			}
			)AS");
		asIScriptModule* First = BuildNativeModule(ScriptEngine, ModuleScope.Get(), FirstModuleSource.c_str());
		if (First == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(First, TEXT("ScriptModule recompile test should build the first module")));
		LogModuleState(*TestRunner, ScriptEngine, First, TEXT("recompile-v1-after-build"));

		asIScriptFunction* FirstFunction = GetNativeFunctionByDecl(First, "int Entry()");
		ASSERT_THAT(IsNotNull(FirstFunction, TEXT("ScriptModule recompile test should expose the first entry function")));

		int32 FirstResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, First, "int Entry()", FirstResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, FirstResult, TEXT("ScriptModule recompile test should run the first function body before discard")));

		const asUINT BeforeDiscardCount = ScriptEngine->GetModuleCount();
		const int DiscardResult = ScriptEngine->DiscardModule(ModuleScope.Get());
		asIScriptModule* LookupAfterDiscard = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS);
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule recompile discard: return=%d beforeCount=%u afterCount=%u lookupAfterDiscard=%s firstModule=%s firstFunction=%s"),
			DiscardResult,
			BeforeDiscardCount,
			ScriptEngine->GetModuleCount(),
			*FormatPointer(LookupAfterDiscard),
			*FormatPointer(First),
			*FormatPointer(FirstFunction)));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), DiscardResult, TEXT("ScriptModule recompile test should discard the first module")));
		ASSERT_THAT(IsNull(LookupAfterDiscard, TEXT("ScriptModule recompile test should remove the discarded module from name lookup")));

		const std::string SecondModuleSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 2;
			}
			)AS");
		asIScriptModule* Second = BuildNativeModule(ScriptEngine, ModuleScope.Get(), SecondModuleSource.c_str());
		if (Second == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(Second, TEXT("ScriptModule recompile test should build the second module")));
		LogModuleState(*TestRunner, ScriptEngine, Second, TEXT("recompile-v2-after-build"));
		asIScriptFunction* SecondFunction = GetNativeFunctionByDecl(Second, "int Entry()");
		ASSERT_THAT(IsNotNull(SecondFunction, TEXT("ScriptModule recompile test should expose the second entry function")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Second, "int Entry()", Result))
		{
			return;
		}

		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule recompile execution: first=%d second=%d secondModule=%s secondFunction=%s"),
			FirstResult,
			Result,
			*FormatPointer(Second),
			*FormatPointer(SecondFunction)));
		ASSERT_THAT(AreNotEqual(First, Second, TEXT("ScriptModule recompile test should create a distinct module after discard")));
		ASSERT_THAT(AreNotEqual(FirstFunction, SecondFunction, TEXT("ScriptModule recompile test should create a distinct function after discard")));
		ASSERT_THAT(AreEqual(2, Result, TEXT("ScriptModule recompile test should run the rebuilt function body")));
	}
	TEST_METHOD(DiscardThenRebuildCreatesDistinctFunctionsAndTypes)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-LIFECYCLE-REBUILD-ISOLATION", "discard_rebuild_function_and_type_identity");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule typed rebuild test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleTypedRebuild");
		const std::string ModuleV1Source = ASTEST_AS_ANSI(R"AS(
			class VersionedState
			{
				int Get()
				{
					return 100;
				}
			}

			int Entry()
			{
				return 101;
			}
			)AS");
		asIScriptModule* ModuleV1 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), ModuleV1Source.c_str());
		if (ModuleV1 == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(ModuleV1, TEXT("ScriptModule typed rebuild test should build version one")));

		asIScriptFunction* FunctionV1 = GetNativeFunctionByDecl(ModuleV1, "int Entry()");
		asITypeInfo* TypeV1 = ModuleV1->GetTypeInfoByDecl("VersionedState");
		if (FunctionV1 == nullptr || TypeV1 == nullptr)
		{
			LogModuleState(*TestRunner, ScriptEngine, ModuleV1, TEXT("typed-rebuild-v1-metadata-missing"));
		}
		ASSERT_THAT(IsNotNull(FunctionV1, TEXT("ScriptModule typed rebuild test should expose version one function")));
		ASSERT_THAT(IsNotNull(TypeV1, TEXT("ScriptModule typed rebuild test should expose version one type")));

		int32 V1Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV1, "int Entry()", V1Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(101, V1Result, TEXT("ScriptModule typed rebuild test should execute version one")));
		LogModuleState(*TestRunner, ScriptEngine, ModuleV1, TEXT("typed-rebuild-v1-after-execute"));

		const int DiscardResult = ScriptEngine->DiscardModule(ModuleScope.Get());
		asIScriptModule* LookupAfterDiscard = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS);
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule typed rebuild discard: return=%d moduleV1=%s functionV1=%s typeV1=%s lookupAfterDiscard=%s"),
			DiscardResult,
			*FormatPointer(ModuleV1),
			*FormatPointer(FunctionV1),
			*FormatPointer(TypeV1),
			*FormatPointer(LookupAfterDiscard)));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), DiscardResult, TEXT("ScriptModule typed rebuild test should discard version one")));
		ASSERT_THAT(IsNull(LookupAfterDiscard, TEXT("ScriptModule typed rebuild test should remove version one from name lookup")));

		const std::string ModuleV2Source = ASTEST_AS_ANSI(R"AS(
			class VersionedState
			{
				int Get()
				{
					return 200;
				}
			}

			class ReplacementOnlyState
			{
				int Get()
				{
					return 2;
				}
			}

			int Entry()
			{
				return 202;
			}
			)AS");
		asIScriptModule* ModuleV2 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), ModuleV2Source.c_str());
		if (ModuleV2 == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(ModuleV2, TEXT("ScriptModule typed rebuild test should build version two")));

		asIScriptFunction* FunctionV2 = GetNativeFunctionByDecl(ModuleV2, "int Entry()");
		asITypeInfo* TypeV2 = ModuleV2->GetTypeInfoByDecl("VersionedState");
		asITypeInfo* ReplacementOnlyType = ModuleV2->GetTypeInfoByDecl("ReplacementOnlyState");
		if (FunctionV2 == nullptr || TypeV2 == nullptr || ReplacementOnlyType == nullptr)
		{
			LogModuleState(*TestRunner, ScriptEngine, ModuleV2, TEXT("typed-rebuild-v2-metadata-missing"));
		}
		ASSERT_THAT(IsNotNull(FunctionV2, TEXT("ScriptModule typed rebuild test should expose version two function")));
		ASSERT_THAT(IsNotNull(TypeV2, TEXT("ScriptModule typed rebuild test should expose version two type")));
		ASSERT_THAT(IsNotNull(ReplacementOnlyType, TEXT("ScriptModule typed rebuild test should expose the replacement-only type")));

		int32 V2Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV2, "int Entry()", V2Result))
		{
			return;
		}

		LogModuleState(*TestRunner, ScriptEngine, ModuleV2, TEXT("typed-rebuild-v2-after-execute"));
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule typed rebuild execution: v1=%d v2=%d moduleV1=%s moduleV2=%s functionV1=%s functionV2=%s typeV1=%s typeV2=%s replacementType=%s"),
			V1Result,
			V2Result,
			*FormatPointer(ModuleV1),
			*FormatPointer(ModuleV2),
			*FormatPointer(FunctionV1),
			*FormatPointer(FunctionV2),
			*FormatPointer(TypeV1),
			*FormatPointer(TypeV2),
			*FormatPointer(ReplacementOnlyType)));
		ASSERT_THAT(AreEqual(202, V2Result, TEXT("ScriptModule typed rebuild test should execute version two")));
		ASSERT_THAT(AreNotEqual(ModuleV1, ModuleV2, TEXT("ScriptModule typed rebuild test should allocate a distinct module after discard")));
		ASSERT_THAT(AreNotEqual(FunctionV1, FunctionV2, TEXT("ScriptModule typed rebuild test should allocate a distinct entry function after discard")));
		ASSERT_THAT(AreNotEqual(TypeV1, TypeV2, TEXT("ScriptModule typed rebuild test should allocate a distinct type after discard")));
	}
};

#endif
