#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderNamespaceTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderNamespace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

	void ReportBuilderFailureDiagnostics(const AngelscriptNativeTestSupport::FNativeTestEngine& Engine) const
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
	TEST_METHOD(NamespaceResolutionSeparatesTypesFunctionsAndGlobals)
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
			"AggregateSupport",
			"COMPILER-BUILDER-DECLARATION-PUBLICATION owns staged scoped publication; this test retains cross-section namespace-qualified runtime support.");

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder namespace test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderNamespaceResolution");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder namespace test should create a backing module")));

		const std::string NamespaceTypesSource = ASTEST_AS_ANSI(R"AS(
			namespace Inventory
			{
				const int Bonus = 5;

				class Item
				{
					int Count = 37;

					int Read()
					{
						return Count;
					}
				}

				int Score(Item Value)
				{
					return Value.Read() + Bonus;
				}

				int ScoreBase()
				{
					return 37 + Bonus;
				}
			}
			)AS");
		const std::string NamespaceEntrySource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return Inventory::ScoreBase();
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-DECLARATION-PUBLICATION-NAMESPACE-CROSS-SECTION-SUPPORT-TYPES"),
			TEXT("BuilderNamespaceCrossSectionSupportTypes"),
			FString(UTF8_TO_TCHAR(NamespaceTypesSource.c_str())));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("COMPILER-BUILDER-DECLARATION-PUBLICATION-NAMESPACE-CROSS-SECTION-SUPPORT-ENTRY"),
			TEXT("BuilderNamespaceCrossSectionSupportEntry"),
			FString(UTF8_TO_TCHAR(NamespaceEntrySource.c_str())));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderNamespace_Types", NamespaceTypesSource.c_str(), TEXT("NamespaceResolutionSeparatesTypesFunctionsAndGlobals.AddTypes")),
			TEXT("Builder namespace test should add the namespace type section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderNamespace_Entry", NamespaceEntrySource.c_str(), TEXT("NamespaceResolutionSeparatesTypesFunctionsAndGlobals.AddEntry")),
			TEXT("Builder namespace test should add the entry section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder namespace test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder namespace test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("NamespaceResolutionSeparatesTypesFunctionsAndGlobals.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder namespace test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics(Engine);
			return;
		}

		asITypeInfo* ItemType = Module->GetTypeInfoByDecl("Inventory::Item");
		asIScriptFunction* ScoreFunction = FindModuleFunctionByNameAndParamCount(Module, "Score", 1, "Inventory");
		asIScriptFunction* ScoreBaseFunction = FindModuleFunctionByNameAndParamCount(Module, "ScoreBase", 0, "Inventory");
		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(ItemType, TEXT("Builder namespace test should expose the namespaced Item type")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(ItemType != nullptr ? ItemType->GetNamespace() : "")),
			TEXT("Builder namespace test should preserve Item namespace")));
		ASSERT_THAT(IsNotNull(ScoreFunction, TEXT("Builder namespace test should expose Inventory::Score")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(ScoreFunction != nullptr ? ScoreFunction->GetNamespace() : "")),
			TEXT("Builder namespace test should preserve Score namespace")));
		ASSERT_THAT(IsNotNull(ScoreBaseFunction, TEXT("Builder namespace test should expose Inventory::ScoreBase")));
		ASSERT_THAT(IsTrue(HasBytecode(ScoreBaseFunction), TEXT("Builder namespace test should compile ScoreBase bytecode")));
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder namespace test should expose global Entry")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), FString(UTF8_TO_TCHAR(EntryFunction != nullptr ? EntryFunction->GetNamespace() : "")),
			TEXT("Builder namespace test should keep Entry in the global namespace")));

		const int BonusGlobalIndex = FindGlobalVarIndexByNameAndNamespace(Module, "Bonus", "Inventory");
		ASSERT_THAT(IsTrue(BonusGlobalIndex >= 0, TEXT("Builder namespace test should expose Bonus global by name")));
		const char* GlobalName = nullptr;
		const char* GlobalNamespace = nullptr;
		int GlobalTypeId = asINVALID_TYPE;
		bool bIsConst = false;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->GetGlobalVar(static_cast<asUINT>(BonusGlobalIndex), &GlobalName, &GlobalNamespace, &GlobalTypeId, &bIsConst),
			TEXT("Builder namespace test should read Bonus global metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Bonus")), FString(UTF8_TO_TCHAR(GlobalName != nullptr ? GlobalName : "")),
			TEXT("Builder namespace test should preserve Bonus global name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(GlobalNamespace != nullptr ? GlobalNamespace : "")),
			TEXT("Builder namespace test should preserve Bonus namespace")));
		ASSERT_THAT(IsTrue(bIsConst, TEXT("Builder namespace test should preserve Bonus constness")));
		ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl("int"), GlobalTypeId,
			TEXT("Builder namespace test should preserve Bonus int type")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("NamespaceResolutionSeparatesTypesFunctionsAndGlobals.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder namespace test should execute namespace-qualified references")));
	}
};

#endif
