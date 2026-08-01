#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleSectionTests, "Angelscript.TestModule.AngelScriptSDK.Module.Sections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

	TEST_METHOD(SyntaxErrorReportsSectionAndOffset)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-SECTION-BUILD-DIAGNOSTIC",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Section diagnostic test should create a standalone SDK engine")));
		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleSectionDiagnostic");
		asIScriptModule* Module = CreateScriptModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Section diagnostic test should create a module")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Broken()
			{
				return ;
			}
			)AS");
		ASSERT_THAT(IsTrue(Module->AddScriptSection("SectionDiagnostic.as", Source.c_str(), Source.length(), 17) >= 0, TEXT("Section diagnostic test should add its source")));
		ASSERT_THAT(IsTrue(Module->Build() < 0, TEXT("Malformed section should fail to build")));
		bool bExactLocationReported = false;
		for (const FNativeMessageEntry& Entry : Engine.GetMessages().Entries)
		{
			bExactLocationReported |= Entry.Section == TEXT("SectionDiagnostic.as") && Entry.Row == 20;
		}
		ASSERT_THAT(IsTrue(bExactLocationReported,
			TEXT("Syntax diagnostics should report SectionDiagnostic.as line 20 (source line 3 plus the supplied offset 17)")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("A failed section build should publish no functions")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("A failed section build should publish no globals")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetObjectTypeCount()),
			TEXT("A failed section build should publish no object types")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetEnumCount()),
			TEXT("A failed section build should publish no enums")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetTypedefCount()),
			TEXT("A failed section build should publish no typedefs")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetImportedFunctionCount()),
			TEXT("A failed section build should publish no imported functions")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("Section diagnostic test should explicitly discard the failed module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Section diagnostic test should remove the failed module from name lookup")));

		AngelscriptNativeTestSupport::FNativeTestEngine IsolatedEngine;
		IsolatedEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IsolatedEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(IsolatedEngine.Get(), TEXT("Section diagnostic test should create an independent engine")));
		if (IsolatedEngine.Get() == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsolatedEngine.Get() != ScriptEngine, TEXT("Section diagnostic test should isolate diagnostics and module state by engine")));
		ASSERT_THAT(IsNull(IsolatedEngine.Get()->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Section diagnostic test should not publish the failed module into an independent engine")));
		ASSERT_THAT(AreEqual(0, IsolatedEngine.GetMessages().Entries.Num(),
			TEXT("Section diagnostic test should not leak diagnostics into an independent engine")));
	}

	TEST_METHOD(CrossSectionFunctionKeepsDeclaringSection)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-SECTION-BUILD-DIAGNOSTIC", "declaring_section_metadata");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Declaring-section test should create a standalone SDK engine")));
		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleDeclaringSection");
		asIScriptModule* Module = CreateScriptModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Declaring-section test should create a module")));
		const std::string Helpers = ASTEST_AS_ANSI(R"AS(
			int Helper()
			{
				return 40;
			}
		)AS");
		const std::string Entry = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return Helper() + 2;
			}
		)AS");
		ASSERT_THAT(IsTrue(Module->AddScriptSection("Helpers.as", Helpers.c_str(), Helpers.length(), 0) >= 0, TEXT("Declaring-section test should add helper section")));
		ASSERT_THAT(IsTrue(Module->AddScriptSection("Entry.as", Entry.c_str(), Entry.length(), 0) >= 0, TEXT("Declaring-section test should add entry section")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->Build(), TEXT("Declaring-section test should build both sections")));
		asIScriptFunction* Helper = GetNativeFunctionByDecl(Module, "int Helper()");
		asIScriptFunction* EntryFunction = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Helper, TEXT("Helper function should resolve")));
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Entry function should resolve")));
		ASSERT_THAT(AreEqual(FString(TEXT("Helpers.as")), FString(UTF8_TO_TCHAR(Helper->GetScriptSectionName())), TEXT("Helper should retain its own declaring section")));
		ASSERT_THAT(AreEqual(FString(TEXT("Entry.as")), FString(UTF8_TO_TCHAR(EntryFunction->GetScriptSectionName())), TEXT("Entry should retain its own declaring section")));
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, Result, TEXT("Declaring-section test should execute the cross-section call after metadata lookup")));
	}

	TEST_METHOD(SingleModulePipeline)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-SECTION-BUILD-DIAGNOSTIC", "single_section_pipeline");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

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
	TEST_METHOD(MultiSectionBuild)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-SECTION-BUILD-DIAGNOSTIC", "multi_section_call");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

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

		AS_NATIVE_PRODUCT_PART("MOD-SECTION-BUILD-DIAGNOSTIC", "cross_section_symbol");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

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
};

#endif
