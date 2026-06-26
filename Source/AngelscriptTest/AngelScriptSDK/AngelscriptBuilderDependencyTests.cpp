#include "AngelscriptBuilderTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderDependencyTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder.Dependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool BuildProviderModule(FAutomationTestBase& Test, AngelscriptNativeTestSupport::FNativeTestEngine& TestEngine, asCModule& Module, const char* SectionName, const char* Source)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		if (!AddBuilderSectionWithLog(Test, Module, SectionName, Source, FString::Printf(TEXT("%s.AddSection"), UTF8_TO_TCHAR(SectionName))))
		{
			return false;
		}

		const int BuildResult = Module.Build();
		ReportBuilderFailureDiagnostics(Test, TestEngine);
		Test.AddInfo(FString::Printf(TEXT("[Builder][Dependencies][%s] provider Build result=%d"), UTF8_TO_TCHAR(SectionName), BuildResult));
		return BuildResult == asSUCCESS;
	}

	static bool CompileConsumerModule(FAutomationTestBase& Test, AngelscriptNativeTestSupport::FNativeTestEngine& TestEngine, asCModule& Module, const FString& Stage)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asCBuilder* Builder = Module.builder;
		if (Builder == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("[Builder][%s] missing builder after consumer section was added"), *Stage));
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

	static const asCModule::FModuleDependencyInfo* FindDependencyInfo(const asCModule& Consumer, const asCModule& Provider)
	{
		return Consumer.moduleDependencies.Find(const_cast<asCModule*>(&Provider));
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

	TEST_METHOD(DirectMarkDependencyRecordsModuleAndSourceLocation)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder direct dependency test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ProviderScope(Engine, "BuilderDependencyDirectProvider");
		asCModule* ProviderModule = CreateBuilderModule(ScriptEngine, ProviderScope.Get());
		ASSERT_THAT(IsNotNull(ProviderModule, TEXT("Builder direct dependency test should create provider module")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ConsumerScope(Engine, "BuilderDependencyDirectConsumer");
		asCModule* ConsumerModule = CreateBuilderModule(ScriptEngine, ConsumerScope.Get());
		ASSERT_THAT(IsNotNull(ConsumerModule, TEXT("Builder direct dependency test should create consumer module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
class Marker
{
}
)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *ConsumerModule, "BuilderDependencyDirectConsumer.as", Source.c_str(), TEXT("DependencyDirect.AddSection")),
			TEXT("Builder direct dependency test should add consumer section")));
		asCBuilder* Builder = ConsumerModule->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder direct dependency test should create consumer builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DependencyDirect.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, ConsumerModule),
			TEXT("Builder direct dependency test should parse consumer section")));
		ASSERT_THAT(IsTrue(Builder->parsers.GetLength() > 0, TEXT("Builder direct dependency test should keep parser node")));

		asCScriptCode* Script = Builder->scripts[0];
		asCScriptNode* Node = Builder->parsers[0]->GetScriptNode()->firstChild;
		ASSERT_THAT(IsNotNull(Script, TEXT("Builder direct dependency test should expose script code")));
		ASSERT_THAT(IsNotNull(Node, TEXT("Builder direct dependency test should expose a node for source location")));

		Builder->MarkDependency(ProviderModule, Node, Script);
		const asCModule::FModuleDependencyInfo* Info = FindDependencyInfo(*ConsumerModule, *ProviderModule);
		ASSERT_THAT(IsNotNull(Info, TEXT("Builder direct dependency test should record provider module dependency")));
		ASSERT_THAT(IsFalse(Info != nullptr && Info->bIsStructuralDependency,
			TEXT("Builder direct dependency test should not mark plain dependency as structural")));
		ASSERT_THAT(IsFalse(Info != nullptr && Info->bIsHardValueDependency,
			TEXT("Builder direct dependency test should not mark plain dependency as hard-value")));
		ASSERT_THAT(AreEqual(1, Info != nullptr ? Info->FirstLineNumber : INDEX_NONE,
			TEXT("Builder direct dependency test should record first dependency line")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->FirstColumn > 0,
			TEXT("Builder direct dependency test should record a positive dependency column")));
	}

	TEST_METHOD(ExplicitMarkStructuralDependencyRecordsStructuralFlag)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder structural dependency test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ProviderScope(Engine, "BuilderDependencyStructuralProvider");
		asCModule* ProviderModule = CreateBuilderModule(ScriptEngine, ProviderScope.Get());
		ASSERT_THAT(IsNotNull(ProviderModule, TEXT("Builder structural dependency test should create provider module")));

		const std::string ProviderSource = ASTEST_AS_ANSI(R"AS(
class ProviderValue
{
	int Stored = 17;
}
)AS");
		ASSERT_THAT(IsTrue(BuildProviderModule(*TestRunner, Engine, *ProviderModule, "BuilderDependencyStructuralProvider.as", ProviderSource.c_str()),
			TEXT("Builder structural dependency test should build provider module")));
		asCTypeInfo* ProviderType = static_cast<asCTypeInfo*>(ProviderModule->GetTypeInfoByDecl("ProviderValue"));
		ASSERT_THAT(IsNotNull(ProviderType, TEXT("Builder structural dependency test should expose provider type metadata")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ConsumerScope(Engine, "BuilderDependencyStructuralConsumer");
		asCModule* ConsumerModule = CreateBuilderModule(ScriptEngine, ConsumerScope.Get());
		ASSERT_THAT(IsNotNull(ConsumerModule, TEXT("Builder structural dependency test should create consumer module")));

		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
class ConsumerValue
{
}
)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *ConsumerModule, "BuilderDependencyStructuralConsumer.as", ConsumerSource.c_str(), TEXT("DependencyStructural.AddSection")),
			TEXT("Builder structural dependency test should add consumer section")));
		asCBuilder* Builder = ConsumerModule->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder structural dependency test should create consumer builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DependencyStructural.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, ConsumerModule),
			TEXT("Builder structural dependency test should parse consumer section")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DependencyStructural.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, ConsumerModule),
			TEXT("Builder structural dependency test should generate consumer type")));

		asCTypeInfo* ConsumerType = static_cast<asCTypeInfo*>(ConsumerModule->GetTypeInfoByDecl("ConsumerValue"));
		ASSERT_THAT(IsNotNull(ConsumerType, TEXT("Builder structural dependency test should expose consumer type metadata")));
		ASSERT_THAT(IsTrue(Builder->parsers.GetLength() > 0, TEXT("Builder structural dependency test should keep parser node")));
		asCScriptCode* Script = Builder->scripts[0];
		asCScriptNode* Node = Builder->parsers[0]->GetScriptNode();
		ASSERT_THAT(IsNotNull(Script, TEXT("Builder structural dependency test should expose script code")));
		ASSERT_THAT(IsNotNull(Node, TEXT("Builder structural dependency test should expose a node for source location")));

		Builder->MarkStructuralDependency(ConsumerType, ProviderType, Node, Script);

		const asCModule::FModuleDependencyInfo* Info = FindDependencyInfo(*ConsumerModule, *ProviderModule);
		ASSERT_THAT(IsNotNull(Info, TEXT("Builder structural dependency test should record provider dependency")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->bIsStructuralDependency,
			TEXT("Builder structural dependency test should mark value-type property dependency as structural")));
		ASSERT_THAT(IsFalse(Info != nullptr && Info->bIsHardValueDependency,
			TEXT("Builder structural dependency test should not mark pure layout dependency as hard-value")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->FirstLineNumber > 0,
			TEXT("Builder structural dependency test should record a dependency source line")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->FirstColumn > 0,
			TEXT("Builder structural dependency test should record a dependency source column")));
	}

	TEST_METHOD(DefaultConstructorCallMarksHardValueDependency)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder hard dependency test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ProviderScope(Engine, "BuilderDependencyHardProvider");
		asCModule* ProviderModule = CreateBuilderModule(ScriptEngine, ProviderScope.Get());
		ASSERT_THAT(IsNotNull(ProviderModule, TEXT("Builder hard dependency test should create provider module")));

		const std::string ProviderSource = ASTEST_AS_ANSI(R"AS(
int ProviderValue()
{
	return 21;
}
)AS");
		ASSERT_THAT(IsTrue(BuildProviderModule(*TestRunner, Engine, *ProviderModule, "BuilderDependencyHardProvider.as", ProviderSource.c_str()),
			TEXT("Builder hard dependency test should build provider module")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ConsumerScope(Engine, "BuilderDependencyHardConsumer");
		asCModule* ConsumerModule = CreateBuilderModule(ScriptEngine, ConsumerScope.Get());
		ASSERT_THAT(IsNotNull(ConsumerModule, TEXT("Builder hard dependency test should create consumer module")));
		ConsumerModule->ImportModule(ProviderModule);

		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
class ConsumerValue
{
	int Stored = ProviderValue();
}

int Entry()
{
	ConsumerValue Value;
	return Value.Stored + ProviderValue();
}
)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *ConsumerModule, "BuilderDependencyHardConsumer.as", ConsumerSource.c_str(), TEXT("DependencyHard.AddSection")),
			TEXT("Builder hard dependency test should add consumer section")));
		ASSERT_THAT(IsTrue(CompileConsumerModule(*TestRunner, Engine, *ConsumerModule, TEXT("DependencyHard")),
			TEXT("Builder hard dependency test should compile consumer module")));

		const asCModule::FModuleDependencyInfo* Info = FindDependencyInfo(*ConsumerModule, *ProviderModule);
		ASSERT_THAT(IsNotNull(Info, TEXT("Builder hard dependency test should record provider dependency")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->bIsHardValueDependency,
			TEXT("Builder hard dependency test should mark default-initializer function call dependency as hard-value")));
		ASSERT_THAT(IsFalse(Info != nullptr && Info->bIsStructuralDependency,
			TEXT("Builder hard dependency test should not mark function-only dependency as structural")));
		asIScriptFunction* EntryFunction = ConsumerModule->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsTrue(HasBytecode(EntryFunction),
			TEXT("Builder hard dependency test should compile consumer entry bytecode")));
	}

	TEST_METHOD(GlobalInitializerRejectsCrossModuleFunctionDependency)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder global dependency test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ProviderScope(Engine, "BuilderDependencyGlobalProvider");
		asCModule* ProviderModule = CreateBuilderModule(ScriptEngine, ProviderScope.Get());
		ASSERT_THAT(IsNotNull(ProviderModule, TEXT("Builder global dependency test should create provider module")));

		const std::string ProviderSource = ASTEST_AS_ANSI(R"AS(
int ProviderValue()
{
	return 40;
}
)AS");
		ASSERT_THAT(IsTrue(BuildProviderModule(*TestRunner, Engine, *ProviderModule, "BuilderDependencyGlobalProvider.as", ProviderSource.c_str()),
			TEXT("Builder global dependency test should build provider module")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ConsumerScope(Engine, "BuilderDependencyGlobalConsumer");
		asCModule* ConsumerModule = CreateBuilderModule(ScriptEngine, ConsumerScope.Get());
		ASSERT_THAT(IsNotNull(ConsumerModule, TEXT("Builder global dependency test should create consumer module")));
		ConsumerModule->ImportModule(ProviderModule);

		const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
const int GlobalAnswer = ProviderValue() + 2;

int Entry()
{
	return GlobalAnswer;
}
)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *ConsumerModule, "BuilderDependencyGlobalConsumer.as", ConsumerSource.c_str(), TEXT("DependencyGlobal.AddSection")),
			TEXT("Builder global dependency test should add consumer section")));
		asCBuilder* Builder = ConsumerModule->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder global dependency test should create consumer builder")));

		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DependencyGlobal.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, ConsumerModule),
			TEXT("Builder global dependency test should parse consumer section")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DependencyGlobal.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, ConsumerModule),
			TEXT("Builder global dependency test should generate consumer types")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DependencyGlobal.BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, ConsumerModule),
			TEXT("Builder global dependency test should generate consumer functions")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Builder, TEXT("DependencyGlobal.BuildLayoutClasses"), &asCBuilder::BuildLayoutClasses, ConsumerModule),
			TEXT("Builder global dependency test should layout consumer classes")));
		LogBuilderState(*TestRunner, TEXT("DependencyGlobal.BuildAllocateGlobalVariables.before"), *Builder, ConsumerModule, true, false);
		Builder->BuildAllocateGlobalVariables();
		LogBuilderState(*TestRunner, TEXT("DependencyGlobal.BuildAllocateGlobalVariables.after"), *Builder, ConsumerModule);

		ASSERT_THAT(IsFalse(RunBuilderStage(*TestRunner, *Builder, TEXT("DependencyGlobal.BuildLayoutFunctions"), &asCBuilder::BuildLayoutFunctions, ConsumerModule),
			TEXT("Builder global dependency test should reject cross-module global initializer calls")));
		ReportBuilderFailureDiagnostics(*TestRunner, Engine);

		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(
			*TestRunner,
			Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("BuilderDependencyGlobalConsumer.as"), 1, TEXT("Global variable initialization cannot call global function ProviderValue")),
			TEXT("Builder global dependency test should report cross-module initializer diagnostic"))));
		asIScriptFunction* EntryFunction = ConsumerModule->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(EntryFunction,
			TEXT("Builder global dependency test should retain function metadata for diagnostics after layout failure")));
		ASSERT_THAT(IsFalse(HasBytecode(EntryFunction),
			TEXT("Builder global dependency test should not compile executable bytecode after layout failure")));
	}
};

#endif
