#include "AngelscriptBuilderTestSupport.h"
#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderGlobalVariableTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder.GlobalVariables",
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

	TEST_METHOD(ConstGlobalsAllocateDescriptorsAndAddresses)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder global variable test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderGlobalConstDescriptors");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder global variable test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			const int GlobalAnswer = 42;

			namespace BuilderGlobals
			{
				const int NamespacedAnswer = GlobalAnswer + 5;
			}

			int ReadGlobal()
			{
				return GlobalAnswer;
			}

			int ReadNamespaced()
			{
				return BuilderGlobals::NamespacedAnswer;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderGlobalConstDescriptors.as", Source.c_str(), TEXT("GlobalConstDescriptors.AddSection")),
			TEXT("Builder global variable test should add the script section")));
		ASSERT_THAT(IsTrue(CompileBuilderGlobals(*TestRunner, Engine, *Module, TEXT("GlobalConstDescriptors")),
			TEXT("Builder global variable test should compile const globals")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder global variable test should keep builder alive after manual codegen")));

		sGlobalVariableDescription* GlobalAnswer = FindGlobalVariableDescriptionByName(*Builder, "GlobalAnswer");
		ASSERT_THAT(IsNotNull(GlobalAnswer, TEXT("Builder global variable test should track GlobalAnswer descriptor")));
		ASSERT_THAT(IsNotNull(GlobalAnswer->property, TEXT("Builder global variable test should allocate GlobalAnswer property")));
		ASSERT_THAT(IsTrue(GlobalAnswer->isCompiled, TEXT("Builder global variable test should mark GlobalAnswer compiled")));
		ASSERT_THAT(IsTrue(GlobalAnswer->isPureConstant, TEXT("Builder global variable test should mark literal const global as pure constant")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(GlobalAnswer->constantValue),
			TEXT("Builder global variable test should preserve GlobalAnswer constant value")));

		sGlobalVariableDescription* NamespacedAnswer = FindGlobalVariableDescriptionByName(*Builder, "NamespacedAnswer");
		ASSERT_THAT(IsNotNull(NamespacedAnswer, TEXT("Builder global variable test should track NamespacedAnswer descriptor")));
		ASSERT_THAT(IsNotNull(NamespacedAnswer->property, TEXT("Builder global variable test should allocate NamespacedAnswer property")));
		ASSERT_THAT(IsTrue(NamespacedAnswer->isCompiled, TEXT("Builder global variable test should mark NamespacedAnswer compiled")));
		ASSERT_THAT(IsTrue(NamespacedAnswer->isPureConstant, TEXT("Builder global variable test should mark namespaced const global as pure constant")));
		ASSERT_THAT(AreEqual(47, static_cast<int32>(NamespacedAnswer->constantValue),
			TEXT("Builder global variable test should fold namespaced const global expression")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderGlobals")), FString(UTF8_TO_TCHAR(NamespacedAnswer->nameSpace->name.AddressOf())),
			TEXT("Builder global variable test should preserve namespaced global namespace")));

		const int32 GlobalIndex = FindGlobalVarIndexByNameAndNamespace(Module, "GlobalAnswer");
		ASSERT_THAT(IsTrue(GlobalIndex >= 0, TEXT("Builder global variable test should publish GlobalAnswer in module globals")));
		const int32 NamespacedIndex = FindGlobalVarIndexByNameAndNamespace(Module, "NamespacedAnswer", "BuilderGlobals");
		ASSERT_THAT(IsTrue(NamespacedIndex >= 0, TEXT("Builder global variable test should publish NamespacedAnswer in module globals")));

		int32* GlobalAddress = static_cast<int32*>(Module->GetAddressOfGlobalVar(static_cast<asUINT>(GlobalIndex)));
		ASSERT_THAT(IsNotNull(GlobalAddress, TEXT("Builder global variable test should allocate GlobalAnswer memory")));
		ASSERT_THAT(AreEqual(42, *GlobalAddress, TEXT("Builder global variable test should initialize GlobalAnswer memory")));
		int32* NamespacedAddress = static_cast<int32*>(Module->GetAddressOfGlobalVar(static_cast<asUINT>(NamespacedIndex)));
		ASSERT_THAT(IsNotNull(NamespacedAddress, TEXT("Builder global variable test should allocate NamespacedAnswer memory")));
		ASSERT_THAT(AreEqual(47, *NamespacedAddress, TEXT("Builder global variable test should initialize NamespacedAnswer memory")));

		int32 GlobalResult = 0;
		ASSERT_THAT(IsTrue(ExecuteScriptFunction<int32>(*TestRunner, ScriptEngine, Module, "int ReadGlobal()", GlobalResult),
			TEXT("Builder global variable test should execute ReadGlobal")));
		ASSERT_THAT(AreEqual(42, GlobalResult, TEXT("Builder global variable test should read GlobalAnswer through bytecode")));
		int32 NamespacedResult = 0;
		ASSERT_THAT(IsTrue(ExecuteScriptFunction<int32>(*TestRunner, ScriptEngine, Module, "int ReadNamespaced()", NamespacedResult),
			TEXT("Builder global variable test should execute ReadNamespaced")));
		ASSERT_THAT(AreEqual(47, NamespacedResult, TEXT("Builder global variable test should read NamespacedAnswer through bytecode")));
	}

	TEST_METHOD(ConstGlobalsKeepModuleMetadataBeforeRuntimeInitialization)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder global metadata test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderGlobalMetadata");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder global metadata test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			const uint GlobalMask = 15;
			const bool GlobalFlag = true;
			)AS");
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
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

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
