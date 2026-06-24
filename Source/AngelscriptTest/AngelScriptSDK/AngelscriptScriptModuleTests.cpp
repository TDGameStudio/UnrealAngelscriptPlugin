#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptSDKTestExecutionHelpers.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptScriptModuleTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

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

	TEST_METHOD(Create)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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
	}

	TEST_METHOD(SingleModulePipeline)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule single-module test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleSinglePipeline", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("single-pipeline-after-execute"));
		TestRunner->AddInfo(FString::Printf(TEXT("ScriptModule single-module execution: Entry()=%d"), Result));
		ASSERT_THAT(AreEqual(42, Result, TEXT("ScriptModule single-module pipeline should execute the compiled function")));
	}

	TEST_METHOD(Discard)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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
	}

	TEST_METHOD(DiscardMissingModuleReturnsNoModule)
	{
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

	TEST_METHOD(RebuildModule)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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

	TEST_METHOD(MultiSectionBuild)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule multi-section test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleMultiSection");
		asIScriptModule* Module = CreateScriptModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("ScriptModule multi-section test should create a backing module")));

		const std::string HelperSectionSource = ASTEST_AS_ANSI(R"AS(
			int Add(int A, int B)
			{
				return A + B;
			}
			)AS");
		const std::string EntrySectionSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return Add(10, 20);
			}
			)AS");
		const int AddFirstResult = Module->AddScriptSection("ScriptModuleMultiSection_A", HelperSectionSource.c_str(), HelperSectionSource.length(), 0);
		const int AddSecondResult = Module->AddScriptSection("ScriptModuleMultiSection_B", EntrySectionSource.c_str(), EntrySectionSource.length(), 0);
		ASSERT_THAT(IsTrue(AddFirstResult >= 0 && AddSecondResult >= 0, TEXT("ScriptModule multi-section test should add both script sections")));

		const int BuildResult = Module->Build();
		if (BuildResult != static_cast<int32>(asSUCCESS))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), BuildResult, TEXT("ScriptModule multi-section test should compile both sections")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(30, Result, TEXT("ScriptModule multi-section test should execute cross-section call Add(10,20)=30")));
	}

	TEST_METHOD(CrossSectionSymbolResolution)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule cross-section symbol test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleCrossSectionSymbol");
		asIScriptModule* Module = CreateScriptModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("ScriptModule cross-section symbol test should create a module")));

		const std::string HelperSectionSource = ASTEST_AS_ANSI(R"AS(
			int Helper()
			{
				return 40;
			}
			)AS");
		const std::string EntrySectionSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return Helper() + 2;
			}
			)AS");
		const int AddHelperResult = Module->AddScriptSection("ScriptModuleCrossSectionSymbol_Helper", HelperSectionSource.c_str(), HelperSectionSource.length(), 0);
		const int AddEntryResult = Module->AddScriptSection("ScriptModuleCrossSectionSymbol_Entry", EntrySectionSource.c_str(), EntrySectionSource.length(), 0);
		ASSERT_THAT(IsTrue(AddHelperResult >= 0 && AddEntryResult >= 0, TEXT("ScriptModule cross-section symbol test should add both sections")));

		const int BuildResult = Module->Build();
		if (BuildResult != static_cast<int32>(asSUCCESS))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), BuildResult, TEXT("ScriptModule cross-section symbol test should build across sections")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, Result, TEXT("ScriptModule cross-section symbol test should resolve Helper from a sibling section")));
	}

	TEST_METHOD(EnumerateFunctions)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule enumerate test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Alpha()
			{
				return 1;
			}

			int Beta()
			{
				return 2;
			}

			int Gamma()
			{
				return 3;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleEnumerate", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(3, static_cast<int32>(Module->GetFunctionCount()), TEXT("ScriptModule enumerate test should report three functions")));
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("enumerate-after-build"));

		bool bFoundBeta = false;
		for (asUINT Index = 0; Index < Module->GetFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(Index);
			if (Function != nullptr && FString(UTF8_TO_TCHAR(Function->GetName())) == TEXT("Beta"))
			{
				bFoundBeta = true;
			}
		}

		ASSERT_THAT(IsTrue(bFoundBeta, TEXT("ScriptModule enumerate test should find Beta via GetFunctionByIndex")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Gamma()"), TEXT("ScriptModule enumerate test should resolve Gamma by declaration")));

		int32 AlphaResult = 0;
		int32 BetaResult = 0;
		int32 GammaResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Alpha()", AlphaResult) ||
			!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Beta()", BetaResult) ||
			!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Gamma()", GammaResult))
		{
			return;
		}

		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule enumerate execution: Alpha=%d Beta=%d Gamma=%d functions={%s}"),
			AlphaResult,
			BetaResult,
			GammaResult,
			*CollectFunctionDeclarations(Module)));
		ASSERT_THAT(AreEqual(1, AlphaResult, TEXT("ScriptModule enumerate test should execute Alpha")));
		ASSERT_THAT(AreEqual(2, BetaResult, TEXT("ScriptModule enumerate test should execute Beta")));
		ASSERT_THAT(AreEqual(3, GammaResult, TEXT("ScriptModule enumerate test should execute Gamma")));
	}

	TEST_METHOD(RecompileAfterDiscard)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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

	TEST_METHOD(FunctionReturnTypeMatrixExecutesModuleFunctions)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule return matrix test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			bool ReturnBool()
			{
				return true;
			}

			int8 ReturnInt8()
			{
				return -128;
			}

			uint8 ReturnUInt8()
			{
				return 255;
			}

			int16 ReturnInt16()
			{
				return -32768;
			}

			uint16 ReturnUInt16()
			{
				return 65535;
			}

			int ReturnInt()
			{
				return -123456789;
			}

			uint ReturnUInt()
			{
				return 4000000000;
			}

			int64 ReturnInt64()
			{
				return -9000000000;
			}

			uint64 ReturnUInt64()
			{
				return 9000000000;
			}

			float ReturnFloat()
			{
				return 3.5;
			}

			double ReturnDouble()
			{
				return 6.25;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleReturnTypeMatrix", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("return-matrix-after-build"));

		ASSERT_THAT(AreEqual(11, static_cast<int32>(Module->GetFunctionCount()), TEXT("ScriptModule return matrix test should expose every return function")));
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool ReturnBool()");
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("ScriptModule return matrix test should execute bool return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int8 ReturnInt8()");
			ASSERT_THAT(AreEqual(static_cast<int8>(-128), Invoker.CallAndReturn<int8>(0), TEXT("ScriptModule return matrix test should execute int8 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint8 ReturnUInt8()");
			ASSERT_THAT(AreEqual(static_cast<uint8>(255), Invoker.CallAndReturn<uint8>(0), TEXT("ScriptModule return matrix test should execute uint8 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int16 ReturnInt16()");
			ASSERT_THAT(AreEqual(static_cast<int16>(-32768), Invoker.CallAndReturn<int16>(0), TEXT("ScriptModule return matrix test should execute int16 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint16 ReturnUInt16()");
			ASSERT_THAT(AreEqual(static_cast<uint16>(65535), Invoker.CallAndReturn<uint16>(0), TEXT("ScriptModule return matrix test should execute uint16 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int ReturnInt()");
			ASSERT_THAT(AreEqual(-123456789, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("ScriptModule return matrix test should execute int return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint ReturnUInt()");
			ASSERT_THAT(AreEqual(static_cast<uint32>(4000000000u), Invoker.CallAndReturn<uint32>(0), TEXT("ScriptModule return matrix test should execute uint return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int64 ReturnInt64()");
			ASSERT_THAT(AreEqual(static_cast<int64>(-9000000000ll), Invoker.CallAndReturn<int64>(0), TEXT("ScriptModule return matrix test should execute int64 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint64 ReturnUInt64()");
			ASSERT_THAT(AreEqual(static_cast<uint64>(9000000000ull), Invoker.CallAndReturn<uint64>(0), TEXT("ScriptModule return matrix test should execute uint64 return")));
		}

		float FloatResult = 0.0f;
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "float ReturnFloat()");
			FloatResult = Invoker.CallAndReturn<float>(0.0f);
		}
		double DoubleResult = 0.0;
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "double ReturnDouble()");
			DoubleResult = Invoker.CallAndReturn<double>(0.0);
		}
		ASSERT_THAT(IsNear(3.5f, FloatResult, 0.0001f, TEXT("ScriptModule return matrix test should execute float return")));
		ASSERT_THAT(IsNear(6.25, DoubleResult, 0.0001, TEXT("ScriptModule return matrix test should execute double return")));
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule return matrix execution: float=%f double=%f functions={%s}"),
			FloatResult,
			DoubleResult,
			*CollectFunctionDeclarations(Module)));
	}

	TEST_METHOD(FunctionArgumentReturnRoundTripExecutesModuleFunctions)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule argument roundtrip test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Sum(int A, int B)
			{
				return A + B;
			}

			uint64 Mix(uint64 High, uint64 Low)
			{
				return (High << 32) | Low;
			}

			double Scale(double Value, double Multiplier)
			{
				return Value * Multiplier;
			}

			bool IsInside(int Value, int Min, int Max)
			{
				return Value >= Min && Value <= Max;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleArgumentReturnRoundTrip", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("argument-roundtrip-after-build"));

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Sum(int, int)");
			Invoker.AddArg(static_cast<int32>(-10)).AddArg(static_cast<int32>(52));
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("ScriptModule argument roundtrip test should return Sum(-10,52)=42")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint64 Mix(uint64, uint64)");
			Invoker.AddArg(static_cast<uint64>(0x1234ull)).AddArg(static_cast<uint64>(0xABCDull));
			ASSERT_THAT(AreEqual(static_cast<uint64>(0x12340000ABCDull), Invoker.CallAndReturn<uint64>(0), TEXT("ScriptModule argument roundtrip test should preserve uint64 arguments and return")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "double Scale(double, double)");
			Invoker.AddArg(7.5).AddArg(4.0);
			const double Result = Invoker.CallAndReturn<double>(0.0);
			ASSERT_THAT(IsNear(30.0, Result, 0.0001, TEXT("ScriptModule argument roundtrip test should return Scale(7.5,4.0)=30.0")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool IsInside(int, int, int)");
			Invoker.AddArg(static_cast<int32>(5)).AddArg(static_cast<int32>(1)).AddArg(static_cast<int32>(10));
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("ScriptModule argument roundtrip test should return true for an in-range value")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool IsInside(int, int, int)");
			Invoker.AddArg(static_cast<int32>(-5)).AddArg(static_cast<int32>(1)).AddArg(static_cast<int32>(10));
			ASSERT_THAT(IsFalse(Invoker.CallAndReturn<bool>(true), TEXT("ScriptModule argument roundtrip test should return false for an out-of-range value")));
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("argument-roundtrip-after-execute"));
	}

	TEST_METHOD(RichModuleStoresTopLevelTablesAndExecutesEntry)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule rich storage test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			namespace Inventory
			{
				const int BaseScore = 40;
				const int SlotCount = 3;

				enum ESlot
				{
					Head = 1,
					Body = 2,
					Weapon = 4
				}

				class ItemState
				{
					int Id;
					int Quantity;

					int GetScore()
					{
						return Id + Quantity;
					}
				}

				int Add(int A, int B)
				{
					return A + B;
				}

				int Entry()
				{
					return Add(BaseScore, SlotCount) + int(ESlot::Weapon);
				}
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleRichStorage", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("rich-storage-after-build"));

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetFunctionCount()), TEXT("ScriptModule rich storage test should expose two global functions")));
		ASSERT_THAT(IsNotNull(FindFunctionByNameAndNamespace(Module, "Add", "Inventory"), TEXT("ScriptModule rich storage test should store Add in the Inventory namespace")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Inventory::Entry()"), TEXT("ScriptModule rich storage test should resolve the namespaced Entry function")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("ScriptModule rich storage test should expose two const globals")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetObjectTypeCount()), TEXT("ScriptModule rich storage test should expose one object type")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetEnumCount()), TEXT("ScriptModule rich storage test should expose one enum")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetTypedefCount()), TEXT("ScriptModule rich storage test should expose no typedefs")));

		const int32 BaseScoreIndex = FindGlobalVarIndexByName(Module, "BaseScore");
		ASSERT_THAT(IsTrue(BaseScoreIndex >= 0, TEXT("ScriptModule rich storage test should find BaseScore by name")));
		const char* GlobalName = nullptr;
		const char* GlobalNamespace = nullptr;
		int GlobalTypeId = 0;
		bool bIsConst = false;
		ASSERT_THAT(IsTrue(
			Module->GetGlobalVar(static_cast<asUINT>(BaseScoreIndex), &GlobalName, &GlobalNamespace, &GlobalTypeId, &bIsConst) >= 0,
			TEXT("ScriptModule rich storage test should read BaseScore metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("BaseScore")), FString(UTF8_TO_TCHAR(GlobalName != nullptr ? GlobalName : "")), TEXT("ScriptModule rich storage test should keep BaseScore name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(GlobalNamespace != nullptr ? GlobalNamespace : "")), TEXT("ScriptModule rich storage test should keep BaseScore namespace")));
		ASSERT_THAT(IsTrue(bIsConst, TEXT("ScriptModule rich storage test should keep BaseScore const flag")));
		ASSERT_THAT(IsNotNull(Module->GetAddressOfGlobalVar(static_cast<asUINT>(BaseScoreIndex)), TEXT("ScriptModule rich storage test should expose BaseScore storage")));
		ASSERT_THAT(IsTrue(GlobalTypeId > 0, TEXT("ScriptModule rich storage test should expose a valid BaseScore type id")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("const int Inventory::BaseScore")),
			FString(UTF8_TO_TCHAR(Module->GetGlobalVarDeclaration(static_cast<asUINT>(BaseScoreIndex), true))),
			TEXT("ScriptModule rich storage test should include namespaces in global declarations")));

		asITypeInfo* ItemStateType = Module->GetTypeInfoByDecl("Inventory::ItemState");
		asITypeInfo* SlotEnumType = Module->GetEnumByIndex(0);
		ASSERT_THAT(IsNotNull(ItemStateType, TEXT("ScriptModule rich storage test should expose ItemState type")));
		ASSERT_THAT(IsNotNull(SlotEnumType, TEXT("ScriptModule rich storage test should expose ESlot enum")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(ItemStateType->GetNamespace())), TEXT("ScriptModule rich storage test should keep ItemState namespace")));
		ASSERT_THAT(AreEqual(FString(TEXT("ESlot")), FString(UTF8_TO_TCHAR(SlotEnumType->GetName())), TEXT("ScriptModule rich storage test should keep ESlot enum name")));

		int32 EntryResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Inventory::Entry()", EntryResult))
		{
			return;
		}
		TestRunner->AddInfo(FString::Printf(TEXT("ScriptModule rich storage execution: Entry()=%d"), EntryResult));
		ASSERT_THAT(AreEqual(47, EntryResult, TEXT("ScriptModule rich storage test should execute namespaced Entry through stored declarations")));
	}

	TEST_METHOD(RebuildClearsPreviousTopLevelTables)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule table rebuild test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleTableRebuild");
		const std::string ModuleV1Source = ASTEST_AS_ANSI(R"AS(
			const int OldValue = 100;

			enum EOld
			{
				One = 1
			}

			class OldState
			{
				int Value;
			}

			int OldEntry()
			{
				return OldValue + int(EOld::One);
			}
			)AS");
		asIScriptModule* ModuleV1 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), ModuleV1Source.c_str());
		if (ModuleV1 == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(ModuleV1, TEXT("ScriptModule table rebuild test should build version one")));
		LogModuleState(*TestRunner, ScriptEngine, ModuleV1, TEXT("table-rebuild-v1-after-build"));

		int32 V1Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV1, "int OldEntry()", V1Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(101, V1Result, TEXT("ScriptModule table rebuild test should execute version one")));

		const std::string ModuleV2Source = ASTEST_AS_ANSI(R"AS(
			const int NewValue = 200;

			enum ENew
			{
				Two = 2
			}

			class NewState
			{
				uint Value;
			}

			int NewEntry()
			{
				return NewValue + int(ENew::Two);
			}
			)AS");
		asIScriptModule* ModuleV2 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), ModuleV2Source.c_str());
		if (ModuleV2 == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(ModuleV2, TEXT("ScriptModule table rebuild test should build version two")));
		LogModuleState(*TestRunner, ScriptEngine, ModuleV2, TEXT("table-rebuild-v2-after-build"));

		int32 V2Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV2, "int NewEntry()", V2Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(202, V2Result, TEXT("ScriptModule table rebuild test should execute version two")));

		ASSERT_THAT(IsNull(ModuleV2->GetFunctionByDecl("int OldEntry()"), TEXT("ScriptModule table rebuild test should remove the old function declaration")));
		ASSERT_THAT(IsNull(ModuleV2->GetTypeInfoByDecl("OldState"), TEXT("ScriptModule table rebuild test should remove the old class declaration")));
		ASSERT_THAT(IsNull(ModuleV2->GetTypeInfoByDecl("EOld"), TEXT("ScriptModule table rebuild test should remove the old enum declaration")));
		ASSERT_THAT(IsNotNull(ModuleV2->GetFunctionByDecl("int NewEntry()"), TEXT("ScriptModule table rebuild test should expose the new function declaration")));
		ASSERT_THAT(IsNotNull(ModuleV2->GetTypeInfoByDecl("NewState"), TEXT("ScriptModule table rebuild test should expose the new class declaration")));
		ASSERT_THAT(IsNotNull(ModuleV2->GetTypeInfoByDecl("ENew"), TEXT("ScriptModule table rebuild test should expose the new enum declaration")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ModuleV2->GetFunctionCount()), TEXT("ScriptModule table rebuild test should keep one function after rebuild")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ModuleV2->GetGlobalVarCount()), TEXT("ScriptModule table rebuild test should keep one global after rebuild")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ModuleV2->GetObjectTypeCount()), TEXT("ScriptModule table rebuild test should keep one object type after rebuild")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ModuleV2->GetEnumCount()), TEXT("ScriptModule table rebuild test should keep one enum after rebuild")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(ModuleV2->GetTypedefCount()), TEXT("ScriptModule table rebuild test should keep zero typedefs after rebuild")));
	}

	TEST_METHOD(FailedBuildDoesNotPublishPartialModuleTablesAndCanRecover)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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
