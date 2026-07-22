#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderPropertyTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderProperty", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	void LogBuilderState(const FString& Stage, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true, bool bIncludeDiagnosticCounters = true) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(TEXT("[Builder][%s] %s | %s"), *Stage, *DescribeBuilderCounts(Builder, bIncludeDiagnosticCounters), *DescribeModuleCounts(Module)));
		if (bExpandBuilderDescriptions)
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] classDecls: %s"), *Stage, *DescribeClassDeclarations(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] namedTypes: %s"), *Stage, *DescribeNamedTypeDeclarations(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] functionDescs: %s"), *Stage, *DescribeFunctionDescriptions(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] globalDescs: %s"), *Stage, *DescribeGlobalDescriptions(Builder)));
		}
		if (Module != nullptr)
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleTypes: %s"), *Stage, *DescribeModuleTypes(Module)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleFunctions: %s"), *Stage, *DescribeModuleFunctions(Module)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleGlobals: %s"), *Stage, *DescribeModuleGlobals(Module)));
		}
	}

	void LogBuilderSectionInput(const FString& Stage, const char* SectionName, const char* Source) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(
			TEXT("[Builder][%s] add section name=%s bytes=%d lines=%d"),
			*Stage,
			*ToTestString(SectionName),
			Source != nullptr ? static_cast<int32>(std::strlen(Source)) : 0,
			CountSourceLines(Source)));
	}

	void LogBuilderStageResult(const FString& Stage, int Result, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(TEXT("[Builder][%s] result=%d"), *Stage, Result));
		LogBuilderState(Stage, Builder, Module, bExpandBuilderDescriptions);
	}

	void LogScriptExecutionResult(const FString& Stage, const char* Declaration, int32 Result) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(TEXT("[Builder][%s] executed %s => %d"), *Stage, *ToTestString(Declaration), Result));
	}

	void ReportBuilderFailureDiagnostics() const
	{
		const FString Messages = Engine.GetMessagesText();
		if (!Messages.IsEmpty())
		{
			AddInfo(Messages);
		}
	}

	bool AddBuilderSectionWithLog(asCModule& Module, const char* SectionName, const char* Source, const FString& Stage) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		LogBuilderSectionInput(Stage, SectionName, Source);
		const bool bAdded = AddBuilderSection(Module, SectionName, Source);
		AddInfo(FString::Printf(TEXT("[Builder][%s] AddScriptSection result=%s"), *Stage, BoolText(bAdded)));
		if (Module.builder != nullptr)
		{
			LogBuilderState(Stage, *Module.builder, &Module, true, false);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] %s"), *Stage, *DescribeModuleCounts(&Module)));
		}
		return bAdded;
	}

	bool RunBuilderStage(asCBuilder& Builder, const FString& Stage, int (asCBuilder::*StageMethod)(), const asCModule* Module = nullptr) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		LogBuilderState(FString::Printf(TEXT("%s.before"), *Stage), Builder, Module, true, false);
		const int Result = (Builder.*StageMethod)();
		LogBuilderStageResult(FString::Printf(TEXT("%s.after"), *Stage), Result, Builder, Module);
		return Result == asSUCCESS;
	}

	bool RunBuilderPipelineThroughLayout(asCBuilder& Builder, const asCModule* Module = nullptr) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		if (!RunBuilderStage(Builder, TEXT("BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildLayoutClasses"), &asCBuilder::BuildLayoutClasses, Module))
		{
			return false;
		}
		LogBuilderState(TEXT("BuildAllocateGlobalVariables.before"), Builder, Module, true, false);
		Builder.BuildAllocateGlobalVariables();
		LogBuilderState(TEXT("BuildAllocateGlobalVariables.after"), Builder, Module);
		return RunBuilderStage(Builder, TEXT("BuildLayoutFunctions"), &asCBuilder::BuildLayoutFunctions, Module);
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

	TEST_METHOD(PropertyInitializersAndMethodOverloadsCompile)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder initializer and overload test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderInitializersAndOverloads");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder initializer and overload test should create a backing module")));

		const std::string InitializersAndOverloadsSource = ASTEST_AS_ANSI(R"AS(
			class Accumulator
			{
				int Base = 40;
				int Delta = 2;

				int Add()
				{
					return Base + Delta;
				}

				int Add(int Extra)
				{
					return Base + Delta + Extra;
				}
			}

			int Entry()
			{
				return 40 + 2;
			}

			int EntryWithArg()
			{
				return 40 + 2 + 5;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderInitializersAndOverloads", InitializersAndOverloadsSource.c_str(), TEXT("PropertyInitializersAndMethodOverloadsCompile.AddInitializersAndOverloads")),
			TEXT("Builder initializer and overload test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder initializer and overload test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder initializer and overload test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("PropertyInitializersAndMethodOverloadsCompile.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder initializer and overload test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		sClassDeclaration* AccumulatorDeclaration = FindClassDeclarationByName(*Builder, "Accumulator");
		ASSERT_THAT(IsNotNull(AccumulatorDeclaration,
			TEXT("Builder initializer and overload test should retain the Accumulator declaration")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(AccumulatorDeclaration != nullptr ? AccumulatorDeclaration->propInits.GetLength() : 0),
			TEXT("Builder initializer and overload test should retain both property initializers")));

		asITypeInfo* AccumulatorType = Module->GetTypeInfoByDecl("Accumulator");
		ASSERT_THAT(IsNotNull(AccumulatorType, TEXT("Builder initializer and overload test should expose Accumulator type metadata")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(AccumulatorType != nullptr ? AccumulatorType->GetPropertyCount() : 0),
			TEXT("Builder initializer and overload test should layout both properties")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(AccumulatorType != nullptr ? AccumulatorType->GetMethodCount() : 0),
			TEXT("Builder initializer and overload test should expose both overload methods")));
		asIScriptFunction* AddNoArg = FindTypeMethodByNameAndParamCount(AccumulatorType, "Add", 0);
		asIScriptFunction* AddWithArg = FindTypeMethodByNameAndParamCount(AccumulatorType, "Add", 1);
		ASSERT_THAT(IsNotNull(AddNoArg, TEXT("Builder initializer and overload test should expose Add()")));
		ASSERT_THAT(IsNotNull(AddWithArg, TEXT("Builder initializer and overload test should expose Add(int)")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(AddNoArg != nullptr ? AddNoArg->GetParamCount() : 0),
			TEXT("Builder initializer and overload test should preserve Add() parameter count")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(AddWithArg != nullptr ? AddWithArg->GetParamCount() : 0),
			TEXT("Builder initializer and overload test should preserve Add(int) parameter count")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("PropertyInitializersAndMethodOverloadsCompile.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder initializer and overload test should execute property initializers through Add()")));

		int32 ResultWithArg = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int EntryWithArg()", ResultWithArg))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("PropertyInitializersAndMethodOverloadsCompile.EntryWithArg"), "int EntryWithArg()", ResultWithArg);
		ASSERT_THAT(AreEqual(47, ResultWithArg, TEXT("Builder initializer and overload test should execute Add(int) overload")));
	}
};

#endif
