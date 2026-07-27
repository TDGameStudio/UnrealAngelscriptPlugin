#include "Support/AngelscriptNativeBuilderTestSupport.h"

// Builder global-variable coverage.
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FBuilderGlobalTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.GlobalVariables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool CompileBuilderGlobals(FAutomationTestBase& Test, AngelscriptNativeTestSupport::FNativeTestEngine& TestEngine, asCModule& Module, const FString& Stage)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asCBuilder* Builder = Module.builder;
		if (Builder == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("[Builder][%s] missing builder after script sections were added"), *Stage));
			return false;
		}

		if (!RunBuilderPipelineThroughLayout(Test, *Builder, &Module))
		{
			ReportBuilderFailureDiagnostics(Test, TestEngine);
			return false;
		}

		if (!RunBuilderStage(Test, *Builder, FString::Printf(TEXT("%s.BuildCompileCode"), *Stage), &asCBuilder::BuildCompileCode, &Module))
		{
			ReportBuilderFailureDiagnostics(Test, TestEngine);
			return false;
		}

		return true;
	}

public:
	TEST_METHOD(ConstGlobalsPreserveDescriptorAddressAndRuntimeState)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-CONST-GLOBAL-STATE",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder global variable test should create a standalone SDK engine")));

		int32 IsolationControlStorage = 73;
		const int IsolationControlIndex = ScriptEngine->RegisterGlobalProperty(
			"int BuilderGlobalIsolationControl",
			&IsolationControlStorage);
		ASSERT_THAT(IsTrue(
			IsolationControlIndex >= 0,
			TEXT("Builder global variable test should register an independent storage control")));
		void* InitialIsolationControlAddress = nullptr;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->GetGlobalPropertyByIndex(
				static_cast<asUINT>(IsolationControlIndex),
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				&InitialIsolationControlAddress),
			TEXT("Builder global variable test should query the initial storage control")));
		ASSERT_THAT(AreEqual(
			static_cast<const void*>(&IsolationControlStorage),
			static_cast<const void*>(InitialIsolationControlAddress),
			TEXT("Builder global variable test should establish the exact control address baseline")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderGlobalConstDescriptors");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder global variable test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			const int LiteralGlobalInt = 10;
			const int64 LiteralGlobalInt64 = 10000000000;
			const double LiteralGlobalDouble = 1.5;
			const bool LiteralGlobalBool = true;
			const int FoldedGlobalInt = 5 + 6;
			const int64 FoldedGlobalInt64 = 10000000000 + 2;
			const double FoldedGlobalDouble = 1.25 + 2.5;
			const bool FoldedGlobalBool = !false;

			namespace BuilderGlobals
			{
				const int LiteralNamespaceInt = 20;
				const int64 LiteralNamespaceInt64 = 20000000000;
				const double LiteralNamespaceDouble = 2.5;
				const bool LiteralNamespaceBool = false;
				const int FoldedNamespaceInt = 10 + 12;
				const int64 FoldedNamespaceInt64 = 20000000000 + 3;
				const double FoldedNamespaceDouble = 2.25 + 2.5;
				const bool FoldedNamespaceBool = !true;
			}

			int ReadLiteralGlobalInt()
			{
				return LiteralGlobalInt;
			}

			int64 ReadLiteralGlobalInt64()
			{
				return LiteralGlobalInt64;
			}

			double ReadLiteralGlobalDouble()
			{
				return LiteralGlobalDouble;
			}

			bool ReadLiteralGlobalBool()
			{
				return LiteralGlobalBool;
			}

			int ReadFoldedGlobalInt()
			{
				return FoldedGlobalInt;
			}

			int64 ReadFoldedGlobalInt64()
			{
				return FoldedGlobalInt64;
			}

			double ReadFoldedGlobalDouble()
			{
				return FoldedGlobalDouble;
			}

			bool ReadFoldedGlobalBool()
			{
				return FoldedGlobalBool;
			}

			int ReadLiteralNamespaceInt()
			{
				return BuilderGlobals::LiteralNamespaceInt;
			}

			int64 ReadLiteralNamespaceInt64()
			{
				return BuilderGlobals::LiteralNamespaceInt64;
			}

			double ReadLiteralNamespaceDouble()
			{
				return BuilderGlobals::LiteralNamespaceDouble;
			}

			bool ReadLiteralNamespaceBool()
			{
				return BuilderGlobals::LiteralNamespaceBool;
			}

			int ReadFoldedNamespaceInt()
			{
				return BuilderGlobals::FoldedNamespaceInt;
			}

			int64 ReadFoldedNamespaceInt64()
			{
				return BuilderGlobals::FoldedNamespaceInt64;
			}

			double ReadFoldedNamespaceDouble()
			{
				return BuilderGlobals::FoldedNamespaceDouble;
			}

			bool ReadFoldedNamespaceBool()
			{
				return BuilderGlobals::FoldedNamespaceBool;
			}
			)AS");
		struct FConstGlobalCase
		{
			const TCHAR* InitializerId;
			const TCHAR* ScopeId;
			const TCHAR* TypeId;
			const char* Name;
			const char* Namespace;
			const char* TypeDeclaration;
			const char* ReaderDeclaration;
			int64 ExpectedInteger;
			double ExpectedDouble;
			bool ExpectedBool;
		};
		const FConstGlobalCase Cases[] =
		{
			{ TEXT("literal"), TEXT("global"), TEXT("int"), "LiteralGlobalInt", "", "int", "int ReadLiteralGlobalInt()", 10, 0.0, false },
			{ TEXT("literal"), TEXT("global"), TEXT("int64"), "LiteralGlobalInt64", "", "int64", "int64 ReadLiteralGlobalInt64()", 10000000000LL, 0.0, false },
			{ TEXT("literal"), TEXT("global"), TEXT("double"), "LiteralGlobalDouble", "", "double", "double ReadLiteralGlobalDouble()", 0, 1.5, false },
			{ TEXT("literal"), TEXT("global"), TEXT("bool"), "LiteralGlobalBool", "", "bool", "bool ReadLiteralGlobalBool()", 0, 0.0, true },
			{ TEXT("literal"), TEXT("namespace"), TEXT("int"), "LiteralNamespaceInt", "BuilderGlobals", "int", "int ReadLiteralNamespaceInt()", 20, 0.0, false },
			{ TEXT("literal"), TEXT("namespace"), TEXT("int64"), "LiteralNamespaceInt64", "BuilderGlobals", "int64", "int64 ReadLiteralNamespaceInt64()", 20000000000LL, 0.0, false },
			{ TEXT("literal"), TEXT("namespace"), TEXT("double"), "LiteralNamespaceDouble", "BuilderGlobals", "double", "double ReadLiteralNamespaceDouble()", 0, 2.5, false },
			{ TEXT("literal"), TEXT("namespace"), TEXT("bool"), "LiteralNamespaceBool", "BuilderGlobals", "bool", "bool ReadLiteralNamespaceBool()", 0, 0.0, false },
			{ TEXT("folded_expression"), TEXT("global"), TEXT("int"), "FoldedGlobalInt", "", "int", "int ReadFoldedGlobalInt()", 11, 0.0, false },
			{ TEXT("folded_expression"), TEXT("global"), TEXT("int64"), "FoldedGlobalInt64", "", "int64", "int64 ReadFoldedGlobalInt64()", 10000000002LL, 0.0, false },
			{ TEXT("folded_expression"), TEXT("global"), TEXT("double"), "FoldedGlobalDouble", "", "double", "double ReadFoldedGlobalDouble()", 0, 3.75, false },
			{ TEXT("folded_expression"), TEXT("global"), TEXT("bool"), "FoldedGlobalBool", "", "bool", "bool ReadFoldedGlobalBool()", 0, 0.0, true },
			{ TEXT("folded_expression"), TEXT("namespace"), TEXT("int"), "FoldedNamespaceInt", "BuilderGlobals", "int", "int ReadFoldedNamespaceInt()", 22, 0.0, false },
			{ TEXT("folded_expression"), TEXT("namespace"), TEXT("int64"), "FoldedNamespaceInt64", "BuilderGlobals", "int64", "int64 ReadFoldedNamespaceInt64()", 20000000003LL, 0.0, false },
			{ TEXT("folded_expression"), TEXT("namespace"), TEXT("double"), "FoldedNamespaceDouble", "BuilderGlobals", "double", "double ReadFoldedNamespaceDouble()", 0, 4.75, false },
			{ TEXT("folded_expression"), TEXT("namespace"), TEXT("bool"), "FoldedNamespaceBool", "BuilderGlobals", "bool", "bool ReadFoldedNamespaceBool()", 0, 0.0, false },
		};
		const FString ReviewSource(UTF8_TO_TCHAR(Source.c_str()));
		for (const FConstGlobalCase& Case : Cases)
		{
			PrintGeneratedAsSource(
				*TestRunner,
				MakeNativeCaseId(
					"COMPILER-BUILDER-CONST-GLOBAL-STATE",
					{ Case.InitializerId, Case.ScopeId, Case.TypeId }),
				TEXT("BuilderConstGlobalState"),
				ReviewSource);
		}
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderGlobalConstDescriptors.as", Source.c_str(), TEXT("GlobalConstDescriptors.AddSection")),
			TEXT("Builder global variable test should add the script section")));
		ASSERT_THAT(IsTrue(CompileBuilderGlobals(*TestRunner, Engine, *Module, TEXT("GlobalConstDescriptors")),
			TEXT("Builder global variable test should compile const globals")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder global variable test should keep builder alive after manual codegen")));

		for (const FConstGlobalCase& Case : Cases)
		{
			sGlobalVariableDescription* const Descriptor =
				FindGlobalVariableDescriptionByName(*Builder, Case.Name);
			ASSERT_THAT(IsNotNull(Descriptor, TEXT("Const-global product should retain each builder descriptor")));
			ASSERT_THAT(IsNotNull(
				Descriptor != nullptr ? Descriptor->property : nullptr,
				TEXT("Const-global product should allocate each property descriptor")));
			ASSERT_THAT(IsTrue(
				Descriptor != nullptr && Descriptor->isCompiled,
				TEXT("Const-global product should mark each descriptor compiled")));
			ASSERT_THAT(IsTrue(
				Descriptor != nullptr && Descriptor->isPureConstant,
				TEXT("Const-global product should fold every literal and constant expression")));
			ASSERT_THAT(AreEqual(
				FString(UTF8_TO_TCHAR(Case.Namespace)),
				FString(UTF8_TO_TCHAR(
					Descriptor != nullptr && Descriptor->nameSpace != nullptr
						? Descriptor->nameSpace->name.AddressOf()
						: "")),
				TEXT("Const-global descriptor should retain its exact namespace")));

			const int32 GlobalIndex =
				FindGlobalVarIndexByNameAndNamespace(Module, Case.Name, Case.Namespace);
			ASSERT_THAT(IsTrue(
				GlobalIndex >= 0,
				TEXT("Const-global product should publish each exact module global")));
			if (GlobalIndex < 0)
			{
				continue;
			}
			const char* GlobalName = nullptr;
			const char* GlobalNamespace = nullptr;
			int TypeId = asINVALID_TYPE;
			bool bIsConst = false;
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Module->GetGlobalVar(
					static_cast<asUINT>(GlobalIndex),
					&GlobalName,
					&GlobalNamespace,
					&TypeId,
					&bIsConst),
				TEXT("Const-global product should expose module metadata")));
			ASSERT_THAT(AreEqual(
				ScriptEngine->GetTypeIdByDecl(Case.TypeDeclaration),
				TypeId,
				TEXT("Const-global module metadata should retain the exact type")));
			ASSERT_THAT(IsTrue(bIsConst, TEXT("Const-global module metadata should retain constness")));

			void* const Address =
				Module->GetAddressOfGlobalVar(static_cast<asUINT>(GlobalIndex));
			ASSERT_THAT(IsNotNull(Address, TEXT("Const-global product should allocate exact storage")));
			asIScriptFunction* const Reader =
				Module->GetFunctionByDecl(Case.ReaderDeclaration);
			ASSERT_THAT(IsNotNull(Reader, TEXT("Const-global product should publish the exact reader")));
			ASSERT_THAT(IsTrue(HasBytecode(Reader), TEXT("Const-global reader should have bytecode")));

			if (FCStringAnsi::Strcmp(Case.TypeDeclaration, "int") == 0)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(Case.ExpectedInteger),
					Address != nullptr ? *static_cast<int32*>(Address) : 0,
					TEXT("Const int storage should preserve its folded value")));
				int32 Result = 0;
				ASSERT_THAT(IsTrue(
					ExecuteScriptFunction<int32>(
						*TestRunner,
						ScriptEngine,
						Module,
						Case.ReaderDeclaration,
						Result),
					TEXT("Const int reader should execute")));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(Case.ExpectedInteger),
					Result,
					TEXT("Const int reader should preserve runtime state")));
			}
			else if (FCStringAnsi::Strcmp(Case.TypeDeclaration, "int64") == 0)
			{
				ASSERT_THAT(AreEqual(
					Case.ExpectedInteger,
					Address != nullptr ? *static_cast<int64*>(Address) : 0,
					TEXT("Const int64 storage should preserve its folded value")));
				FSdkFunctionInvoker Invoker(
					*TestRunner,
					ScriptEngine,
					Module,
					Case.ReaderDeclaration);
				ASSERT_THAT(IsTrue(
					Invoker.IsValid(),
					TEXT("Const int64 reader should resolve exactly")));
				const int64 Result = Invoker.CallAndReturn<int64>();
				ASSERT_THAT(AreEqual(Case.ExpectedInteger, Result, TEXT("Const int64 reader should preserve runtime state")));
			}
			else if (FCStringAnsi::Strcmp(Case.TypeDeclaration, "double") == 0)
			{
				ASSERT_THAT(IsNear(
					Case.ExpectedDouble,
					Address != nullptr ? *static_cast<double*>(Address) : 0.0,
					1e-10,
					TEXT("Const double storage should preserve its folded value")));
				double Result = 0.0;
				ASSERT_THAT(IsTrue(
					ExecuteScriptFunction<double>(
						*TestRunner,
						ScriptEngine,
						Module,
						Case.ReaderDeclaration,
						Result),
					TEXT("Const double reader should execute")));
				ASSERT_THAT(IsNear(
					Case.ExpectedDouble,
					Result,
					1e-10,
					TEXT("Const double reader should preserve runtime state")));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					Case.ExpectedBool,
					Address != nullptr && *static_cast<bool*>(Address),
					TEXT("Const bool storage should preserve its folded value")));
				bool Result = false;
				ASSERT_THAT(IsTrue(
					ExecuteScriptFunction<bool>(
						*TestRunner,
						ScriptEngine,
						Module,
						Case.ReaderDeclaration,
						Result),
					TEXT("Const bool reader should execute")));
				ASSERT_THAT(AreEqual(Case.ExpectedBool, Result, TEXT("Const bool reader should preserve runtime state")));
			}
		}

		if (Module->builder != nullptr)
		{
			asDELETE(Module->builder, asCBuilder);
		}
		Module->builder = nullptr;
		Builder = nullptr;
		ASSERT_THAT(IsNull(
			Module->builder,
			TEXT("Const-global cleanup should release its transient builder and parser trees")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("Const-global cleanup should explicitly discard its exact module and storage")));
		Module = nullptr;
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Const-global module should be absent after explicit discard")));
		void* FinalIsolationControlAddress = nullptr;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->GetGlobalPropertyByIndex(
				static_cast<asUINT>(IsolationControlIndex),
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				&FinalIsolationControlAddress),
			TEXT("Const-global cleanup should preserve the independent storage control")));
		ASSERT_THAT(AreEqual(
			static_cast<const void*>(InitialIsolationControlAddress),
			static_cast<const void*>(FinalIsolationControlAddress),
			TEXT("Const-global cleanup should preserve the control storage address")));
		ASSERT_THAT(AreEqual(
			73,
			IsolationControlStorage,
			TEXT("Const-global execution and cleanup should not mutate the control storage value")));

		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Const-global isolation should create an independent engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				IndependentEngine.Get()->GetModuleCount(),
				TEXT("Const-global publication should not create modules in an independent engine")));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				IndependentEngine.Get()->GetGlobalPropertyCount(),
				TEXT("Const-global storage control should remain local to the owning engine")));
		}
	}

	TEST_METHOD(ConstGlobalsKeepModuleMetadataBeforeRuntimeInitialization)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"COMPILER-BUILDER-CONST-GLOBAL-STATE owns descriptor, module, address, bytecode, and runtime state for the complete primitive/scope/initializer table.");

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder global metadata test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderGlobalMetadata");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder global metadata test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			const uint GlobalMask = 15;
			const bool GlobalFlag = true;
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-CONST-GLOBAL-STATE-LEGACY-METADATA"),
			TEXT("BuilderConstGlobalMetadataCompatibility"),
			FString(UTF8_TO_TCHAR(Source.c_str())));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderGlobalMetadata.as", Source.c_str(), TEXT("GlobalMetadata.AddSection")),
			TEXT("Builder global metadata test should add the script section")));
		ASSERT_THAT(IsTrue(CompileBuilderGlobals(*TestRunner, Engine, *Module, TEXT("GlobalMetadata")),
			TEXT("Builder global metadata test should compile primitive const globals")));

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder global metadata test should publish two globals")));

		const int32 MaskIndex = FindGlobalVarIndexByNameAndNamespace(Module, "GlobalMask");
		ASSERT_THAT(IsTrue(MaskIndex >= 0, TEXT("Builder global metadata test should find GlobalMask")));
		const char* MaskName = nullptr;
		const char* MaskNamespace = nullptr;
		int MaskTypeId = asINVALID_TYPE;
		bool bMaskConst = false;
		ASSERT_THAT(AreEqual(0, Module->GetGlobalVar(static_cast<asUINT>(MaskIndex), &MaskName, &MaskNamespace, &MaskTypeId, &bMaskConst),
			TEXT("Builder global metadata test should query GlobalMask metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("GlobalMask")), FString(UTF8_TO_TCHAR(MaskName)),
			TEXT("Builder global metadata test should preserve GlobalMask name")));
		ASSERT_THAT(AreEqual(asTYPEID_UINT32, MaskTypeId, TEXT("Builder global metadata test should preserve GlobalMask type id")));
		ASSERT_THAT(IsTrue(bMaskConst, TEXT("Builder global metadata test should mark GlobalMask const")));

		const int32 FlagIndex = FindGlobalVarIndexByNameAndNamespace(Module, "GlobalFlag");
		ASSERT_THAT(IsTrue(FlagIndex >= 0, TEXT("Builder global metadata test should find GlobalFlag")));
		const char* FlagName = nullptr;
		const char* FlagNamespace = nullptr;
		int FlagTypeId = asINVALID_TYPE;
		bool bFlagConst = false;
		ASSERT_THAT(AreEqual(0, Module->GetGlobalVar(static_cast<asUINT>(FlagIndex), &FlagName, &FlagNamespace, &FlagTypeId, &bFlagConst),
			TEXT("Builder global metadata test should query GlobalFlag metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("GlobalFlag")), FString(UTF8_TO_TCHAR(FlagName)),
			TEXT("Builder global metadata test should preserve GlobalFlag name")));
		ASSERT_THAT(AreEqual(asTYPEID_BOOL, FlagTypeId, TEXT("Builder global metadata test should preserve GlobalFlag type id")));
		ASSERT_THAT(IsTrue(bFlagConst, TEXT("Builder global metadata test should mark GlobalFlag const")));
	}

	TEST_METHOD(MutableGlobalIsRejectedWithoutExecutableLeak)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"COMPILER-BUILDER-FORK-DECLARATION-REJECTION owns mutable-global rejection, atomicity, and recovery; this method retains detailed rejected-descriptor compatibility evidence.");

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder mutable global test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderGlobalMutableRejected");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder mutable global test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int MutableCounter = 1;

			int Entry()
			{
				return MutableCounter;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-FORK-DECLARATION-REJECTION-LEGACY-MUTABLE-GLOBAL"),
			TEXT("BuilderMutableGlobalRejectionCompatibility"),
			FString(UTF8_TO_TCHAR(Source.c_str())));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderGlobalMutableRejected.as", Source.c_str(), TEXT("GlobalMutableRejected.AddSection")),
			TEXT("Builder mutable global test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder mutable global test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("GlobalMutableRejected.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder mutable global test should parse mutable global section")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("GlobalMutableRejected.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder mutable global test should generate types before mutable global rejection")));

		const bool bGeneratedFunctions = RunBuilderStage(*TestRunner, *Builder, TEXT("GlobalMutableRejected.BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module);
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);
		ASSERT_THAT(IsFalse(bGeneratedFunctions, TEXT("Builder mutable global test should reject mutable global during function/global registration")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(*TestRunner, Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("BuilderGlobalMutableRejected.as"), INDEX_NONE, TEXT("Mutable global variables are not supported")),
			TEXT("MutableGlobal.Rejected")),
			TEXT("Builder mutable global test should report unsupported mutable global")));
		sGlobalVariableDescription* MutableCounter = FindGlobalVariableDescriptionByName(*Builder, "MutableCounter");
		ASSERT_THAT(IsNotNull(MutableCounter, TEXT("Builder mutable global test should retain failed global descriptor for diagnostics")));
		ASSERT_THAT(IsFalse(MutableCounter != nullptr && MutableCounter->isCompiled,
			TEXT("Builder mutable global test should not mark rejected mutable global compiled")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder mutable global test should expose the rejected declaration metadata before cleanup")));
		const int32 MutableIndex = FindGlobalVarIndexByNameAndNamespace(Module, "MutableCounter");
		ASSERT_THAT(IsTrue(MutableIndex >= 0, TEXT("Builder mutable global test should expose the rejected mutable global name")));
		const char* MutableName = nullptr;
		const char* MutableNamespace = nullptr;
		int MutableTypeId = asINVALID_TYPE;
		bool bMutableConst = true;
		ASSERT_THAT(AreEqual(0, Module->GetGlobalVar(static_cast<asUINT>(MutableIndex), &MutableName, &MutableNamespace, &MutableTypeId, &bMutableConst),
			TEXT("Builder mutable global test should allow querying rejected global metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("MutableCounter")), FString(UTF8_TO_TCHAR(MutableName)),
			TEXT("Builder mutable global test should preserve rejected global name")));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, MutableTypeId, TEXT("Builder mutable global test should preserve rejected global type")));
		ASSERT_THAT(IsFalse(bMutableConst, TEXT("Builder mutable global test should preserve rejected global mutability")));
		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder mutable global test should expose declaration metadata before cleanup")));
		ASSERT_THAT(IsFalse(HasBytecode(EntryFunction), TEXT("Builder mutable global test should not emit executable bytecode after rejection")));
	}
};

#endif
