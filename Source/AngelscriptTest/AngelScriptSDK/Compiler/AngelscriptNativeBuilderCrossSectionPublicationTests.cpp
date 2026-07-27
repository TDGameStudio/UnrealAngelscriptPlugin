#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "Support/AngelscriptNativeBuilderDependencyTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

template <typename TDerived, typename TAsserter>
class TBuilderCrossSectionPublicationTestSupport : public TBuilderDependencySharedTestSupport<TDerived, TAsserter>
{
protected:
	using TBuilderDependencySharedTestSupport<TDerived, TAsserter>::AddInfo;

	static void PrintDependencySource(
		FAutomationTestBase& Test,
		const TCHAR* CaseId,
		const TCHAR* ModuleName,
		const std::string& Source)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			CaseId,
			ModuleName,
			FString(UTF8_TO_TCHAR(Source.c_str())));
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
};

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderCrossSectionPublicationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderDependency",
	TBuilderCrossSectionPublicationTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CrossSectionPublicationPreservesOwnersAndExecution)
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

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-CROSS-SECTION-PUBLICATION",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Runtime
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Cross-section publication owner should create a standalone SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		{
			const std::string ProviderSource = ASTEST_AS_ANSI(R"AS(
				int ProvideValue()
				{
					return 40;
				}
				)AS");
			const std::string ConsumerSource = ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					return ProvideValue() + 2;
				}
				)AS");
			FScopedNativeModuleName ModuleScope(
				Engine,
				"BuilderCrossSectionOwnerProviderConsumer");
			asCModule* const Module =
				CreateBuilderModule(ScriptEngine, ModuleScope.Get());
			ASSERT_THAT(IsNotNull(
				Module,
				TEXT("Provider/consumer shape should create its module")));
			if (Module != nullptr)
			{
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-CROSS-SECTION-PUBLICATION-PROVIDER-CONSUMER-CONSUMER"),
					TEXT("BuilderCrossSectionOwnerProviderConsumer"),
					ConsumerSource);
				ASSERT_THAT(IsTrue(
					AddBuilderSectionWithLog(
						*Module,
						"BuilderCrossSectionOwner_Consumer.as",
						ConsumerSource.c_str(),
						TEXT("CrossSectionOwner.ProviderConsumer.AddConsumer")),
					TEXT("Provider/consumer shape should add its consumer before its provider")));
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-CROSS-SECTION-PUBLICATION-PROVIDER-CONSUMER-PROVIDER"),
					TEXT("BuilderCrossSectionOwnerProviderConsumer"),
					ProviderSource);
				ASSERT_THAT(IsTrue(
					AddBuilderSectionWithLog(
						*Module,
						"BuilderCrossSectionOwner_Provider.as",
						ProviderSource.c_str(),
						TEXT("CrossSectionOwner.ProviderConsumer.AddProvider")),
					TEXT("Provider/consumer shape should add its provider after its consumer")));

				asCBuilder* const Builder = Module->builder;
				ASSERT_THAT(IsNotNull(
					Builder,
					TEXT("Provider/consumer shape should expose its builder")));
				if (Builder != nullptr)
				{
					const bool bLayoutSucceeded =
						RunBuilderPipelineThroughLayout(*Builder, Module);
					ASSERT_THAT(IsTrue(
						bLayoutSucceeded,
						TEXT("Provider/consumer shape should build through layout")));
					bool bCodeSucceeded = false;
					if (bLayoutSucceeded)
					{
						bCodeSucceeded = RunBuilderStage(
							*Builder,
							TEXT("CrossSectionOwner.ProviderConsumer.BuildCompileCode"),
							&asCBuilder::BuildCompileCode,
							Module);
					}
					ASSERT_THAT(IsTrue(
						bCodeSucceeded,
						TEXT("Provider/consumer shape should compile bytecode")));

					asIScriptFunction* const Provider =
						bCodeSucceeded
							? Module->GetFunctionByDecl("int ProvideValue()")
							: nullptr;
					asIScriptFunction* const Entry =
						bCodeSucceeded
							? Module->GetFunctionByDecl("int Entry()")
							: nullptr;
					ASSERT_THAT(IsNotNull(
						Provider,
						TEXT("Provider/consumer shape should publish its provider")));
					ASSERT_THAT(IsNotNull(
						Entry,
						TEXT("Provider/consumer shape should publish its consumer entry")));
					ASSERT_THAT(AreEqual(
						FString(TEXT("BuilderCrossSectionOwner_Provider.as")),
						FString(UTF8_TO_TCHAR(
							Provider != nullptr
								? Provider->GetScriptSectionName()
								: "")),
						TEXT("Provider/consumer shape should preserve exact provider section ownership")));
					ASSERT_THAT(AreEqual(
						FString(TEXT("BuilderCrossSectionOwner_Consumer.as")),
						FString(UTF8_TO_TCHAR(
							Entry != nullptr
								? Entry->GetScriptSectionName()
								: "")),
						TEXT("Provider/consumer shape should preserve exact consumer section ownership")));
					ASSERT_THAT(IsTrue(
						HasBytecode(Provider),
						TEXT("Provider/consumer shape should compile provider bytecode")));
					ASSERT_THAT(IsTrue(
						HasBytecode(Entry),
						TEXT("Provider/consumer shape should compile consumer bytecode")));

					int32 Result = 0;
					bool bExecuted = false;
					if (bCodeSucceeded)
					{
						bExecuted = ExecuteScriptFunction(
							*TestRunner,
							ScriptEngine,
							Module,
							"int Entry()",
							Result);
					}
					ASSERT_THAT(IsTrue(
						bExecuted,
						TEXT("Provider/consumer shape should execute across its section boundary")));
					ASSERT_THAT(AreEqual(
						42,
						Result,
						TEXT("Provider/consumer shape should preserve its cross-section result")));
				}
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderCrossSectionOwnerProviderConsumer",
				asGM_ONLY_IF_EXISTS),
			TEXT("Provider/consumer shape should discard its module before the next shape")));

		{
			const std::string TypesSource = ASTEST_AS_ANSI(R"AS(
				class SharedState
				{
					int Value;

					int Read()
					{
						return Value;
					}
				}
				)AS");
			const std::string HelpersSource = ASTEST_AS_ANSI(R"AS(
				int AddOne(int Value)
				{
					return Value + 1;
				}
				)AS");
			const std::string EntrySource = ASTEST_AS_ANSI(R"AS(
				int Entry(SharedState State)
				{
					return AddOne(State.Read()) + 2;
				}
				)AS");
			FScopedNativeModuleName ModuleScope(
				Engine,
				"BuilderCrossSectionOwnerTypeHelperEntry");
			asCModule* const Module =
				CreateBuilderModule(ScriptEngine, ModuleScope.Get());
			ASSERT_THAT(IsNotNull(
				Module,
				TEXT("Type/helper/entry shape should create its module")));
			if (Module != nullptr)
			{
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-CROSS-SECTION-PUBLICATION-TYPE-HELPER-ENTRY-ENTRY"),
					TEXT("BuilderCrossSectionOwnerTypeHelperEntry"),
					EntrySource);
				ASSERT_THAT(IsTrue(
					AddBuilderSectionWithLog(
						*Module,
						"BuilderCrossSectionOwner_Entry.as",
						EntrySource.c_str(),
						TEXT("CrossSectionOwner.TypeHelperEntry.AddEntry")),
					TEXT("Type/helper/entry shape should add its entry first")));
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-CROSS-SECTION-PUBLICATION-TYPE-HELPER-ENTRY-HELPER"),
					TEXT("BuilderCrossSectionOwnerTypeHelperEntry"),
					HelpersSource);
				ASSERT_THAT(IsTrue(
					AddBuilderSectionWithLog(
						*Module,
						"BuilderCrossSectionOwner_Helpers.as",
						HelpersSource.c_str(),
						TEXT("CrossSectionOwner.TypeHelperEntry.AddHelpers")),
					TEXT("Type/helper/entry shape should add its helper second")));
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-CROSS-SECTION-PUBLICATION-TYPE-HELPER-ENTRY-TYPE"),
					TEXT("BuilderCrossSectionOwnerTypeHelperEntry"),
					TypesSource);
				ASSERT_THAT(IsTrue(
					AddBuilderSectionWithLog(
						*Module,
						"BuilderCrossSectionOwner_Types.as",
						TypesSource.c_str(),
						TEXT("CrossSectionOwner.TypeHelperEntry.AddTypes")),
					TEXT("Type/helper/entry shape should add its type last")));

				asCBuilder* const Builder = Module->builder;
				ASSERT_THAT(IsNotNull(
					Builder,
					TEXT("Type/helper/entry shape should expose its builder")));
				if (Builder != nullptr)
				{
					const bool bLayoutSucceeded =
						RunBuilderPipelineThroughLayout(*Builder, Module);
					ASSERT_THAT(IsTrue(
						bLayoutSucceeded,
						TEXT("Type/helper/entry shape should build through layout")));
					bool bCodeSucceeded = false;
					if (bLayoutSucceeded)
					{
						bCodeSucceeded = RunBuilderStage(
							*Builder,
							TEXT("CrossSectionOwner.TypeHelperEntry.BuildCompileCode"),
							&asCBuilder::BuildCompileCode,
							Module);
					}
					ASSERT_THAT(IsTrue(
						bCodeSucceeded,
						TEXT("Type/helper/entry shape should compile bytecode")));

					asITypeInfo* const SharedState =
						bCodeSucceeded
							? Module->GetTypeInfoByDecl("SharedState")
							: nullptr;
					asIScriptFunction* const Read =
						SharedState != nullptr
							? SharedState->GetMethodByDecl("int Read()")
							: nullptr;
					asIScriptFunction* const Helper =
						bCodeSucceeded
							? GetNativeFunctionByDecl(
								Module,
								"int AddOne(const int)")
							: nullptr;
					asIScriptFunction* const Entry =
						bCodeSucceeded
							? GetNativeFunctionByDecl(
								Module,
								"int Entry(SharedState)")
							: nullptr;
					ASSERT_THAT(IsNotNull(
						SharedState,
						TEXT("Type/helper/entry shape should publish SharedState metadata")));
					ASSERT_THAT(AreEqual(
						1,
						static_cast<int32>(
							SharedState != nullptr
								? SharedState->GetPropertyCount()
								: 0),
						TEXT("Type/helper/entry shape should layout SharedState.Value")));
					ASSERT_THAT(IsNotNull(
						Read,
						TEXT("Type/helper/entry shape should publish SharedState.Read")));
					ASSERT_THAT(IsNotNull(
						Helper,
						TEXT("Type/helper/entry shape should publish AddOne")));
					ASSERT_THAT(IsNotNull(
						Entry,
						TEXT("Type/helper/entry shape should publish Entry")));
					ASSERT_THAT(AreEqual(
						FString(TEXT("BuilderCrossSectionOwner_Types.as")),
						FString(UTF8_TO_TCHAR(
							Read != nullptr
								? Read->GetScriptSectionName()
								: "")),
						TEXT("Type/helper/entry shape should preserve method section ownership")));
					ASSERT_THAT(AreEqual(
						FString(TEXT("BuilderCrossSectionOwner_Helpers.as")),
						FString(UTF8_TO_TCHAR(
							Helper != nullptr
								? Helper->GetScriptSectionName()
								: "")),
						TEXT("Type/helper/entry shape should preserve helper section ownership")));
					ASSERT_THAT(AreEqual(
						FString(TEXT("BuilderCrossSectionOwner_Entry.as")),
						FString(UTF8_TO_TCHAR(
							Entry != nullptr
								? Entry->GetScriptSectionName()
								: "")),
						TEXT("Type/helper/entry shape should preserve entry section ownership")));
					ASSERT_THAT(IsTrue(
						HasBytecode(Read),
						TEXT("Type/helper/entry shape should compile method bytecode")));
					ASSERT_THAT(IsTrue(
						HasBytecode(Helper),
						TEXT("Type/helper/entry shape should compile helper bytecode")));
					ASSERT_THAT(IsTrue(
						HasBytecode(Entry),
						TEXT("Type/helper/entry shape should compile entry bytecode")));

					// Direct builder-stage calls intentionally bypass the complete
					// module build lifecycle. Execute the same three-section source
					// through the public Build() path so script-object factories
					// receive the engine finalization that raw staged inspection
					// does not provide.
					ScriptEngine->DiscardModule(ModuleScope.Get());
					FScopedNativeModuleName RuntimeModuleScope(
						Engine,
						"BuilderCrossSectionOwnerTypeHelperEntryRuntime");
					asIScriptModule* const RuntimeModule =
						ScriptEngine->GetModule(
							RuntimeModuleScope.Get(),
							asGM_ALWAYS_CREATE);
					ASSERT_THAT(IsNotNull(
						RuntimeModule,
						TEXT("Type/helper/entry runtime shape should create its module")));
					bool bRuntimeBuilt = RuntimeModule != nullptr;
					if (RuntimeModule != nullptr)
					{
						bRuntimeBuilt &=
							RuntimeModule->AddScriptSection(
								"BuilderCrossSectionOwner_Entry.as",
								EntrySource.c_str(),
								static_cast<asUINT>(EntrySource.size()))
							>= 0;
						bRuntimeBuilt &=
							RuntimeModule->AddScriptSection(
								"BuilderCrossSectionOwner_Helpers.as",
								HelpersSource.c_str(),
								static_cast<asUINT>(HelpersSource.size()))
							>= 0;
						bRuntimeBuilt &=
							RuntimeModule->AddScriptSection(
								"BuilderCrossSectionOwner_Types.as",
								TypesSource.c_str(),
								static_cast<asUINT>(TypesSource.size()))
							>= 0;
						bRuntimeBuilt &=
							RuntimeModule->Build() >= 0;
					}
					ASSERT_THAT(IsTrue(
						bRuntimeBuilt,
						TEXT("Type/helper/entry runtime shape should complete the public module build lifecycle")));

					asITypeInfo* const RuntimeSharedState =
						bRuntimeBuilt
							? RuntimeModule->GetTypeInfoByDecl("SharedState")
							: nullptr;
					ASSERT_THAT(IsNotNull(
						RuntimeSharedState,
						TEXT("Type/helper/entry runtime shape should expose SharedState")));
					void* const StateObject =
						RuntimeSharedState != nullptr
							? ScriptEngine->CreateScriptObject(
								RuntimeSharedState)
							: nullptr;
					ASSERT_THAT(IsNotNull(
						StateObject,
						TEXT("Type/helper/entry runtime shape should construct SharedState through the public API")));
					if (StateObject == nullptr)
					{
						return;
					}
					ON_SCOPE_EXIT
					{
						ScriptEngine->ReleaseScriptObject(
							StateObject,
							RuntimeSharedState);
					};

					asIScriptObject* const SharedStateObject =
						static_cast<asIScriptObject*>(StateObject);
					void* const ValueAddress =
						SharedStateObject->GetAddressOfProperty(0);
					ASSERT_THAT(IsNotNull(
						ValueAddress,
						TEXT("Type/helper/entry runtime shape should expose SharedState.Value storage")));
					if (ValueAddress == nullptr)
					{
						return;
					}
					*static_cast<int32*>(ValueAddress) = 39;

					int32 Result = 0;
					if (bRuntimeBuilt)
					{
						FSdkFunctionInvoker Invoker(
							*TestRunner,
							ScriptEngine,
							RuntimeModule,
							"int Entry(SharedState)");
						Result = Invoker
							.AddArgObject(StateObject)
							.CallAndReturn<int32>();
					}
					ASSERT_THAT(AreEqual(
						42,
						Result,
						TEXT("Type/helper/entry shape should preserve its cross-section result")));
				}
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderCrossSectionOwnerTypeHelperEntry",
				asGM_ONLY_IF_EXISTS),
			TEXT("Type/helper/entry shape should discard its module after execution")));
	}

	};

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderCrossSectionCompatibilityTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderDependency",
	TBuilderCrossSectionPublicationTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CrossSectionDependenciesCompileAndKeepSections)
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
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained type/helper/entry section smoke; COMPILER-BUILDER-CROSS-SECTION-PUBLICATION owns both section shapes, exact owners, metadata, bytecode, runtime, cleanup, and isolation.");

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder cross-section dependency test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderCrossSectionDependencies");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder cross-section dependency test should create a backing module")));

		const std::string TypesSectionSource = ASTEST_AS_ANSI(R"AS(
			class SharedState
			{
				int Value = 39;

				int Read()
				{
					return Value;
				}
			}
			)AS");
		const std::string HelpersSectionSource = ASTEST_AS_ANSI(R"AS(
			int AddOne(int Value)
			{
				return Value + 1;
			}
			)AS");
		const std::string EntrySectionSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return AddOne(39) + 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderCrossSection_Types", TypesSectionSource.c_str(), TEXT("CrossSectionDependenciesCompileAndKeepSections.AddTypes")),
			TEXT("Builder cross-section dependency test should add the type section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderCrossSection_Helpers", HelpersSectionSource.c_str(), TEXT("CrossSectionDependenciesCompileAndKeepSections.AddHelpers")),
			TEXT("Builder cross-section dependency test should add the helper section")));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderCrossSection_Entry", EntrySectionSource.c_str(), TEXT("CrossSectionDependenciesCompileAndKeepSections.AddEntry")),
			TEXT("Builder cross-section dependency test should add the entry section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder cross-section dependency test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder cross-section dependency test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("CrossSectionDependenciesCompileAndKeepSections.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder cross-section dependency test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics(Engine);
			return;
		}

		asITypeInfo* SharedStateType = Module->GetTypeInfoByDecl("SharedState");
		asIScriptFunction* HelperFunction = FindModuleFunctionByNameAndParamCount(Module, "AddOne", 1);
		asIScriptFunction* EntryFunction = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(SharedStateType, TEXT("Builder cross-section dependency test should expose SharedState type metadata")));
		ASSERT_THAT(IsNotNull(HelperFunction, TEXT("Builder cross-section dependency test should expose AddOne")));
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Builder cross-section dependency test should expose Entry")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderCrossSection_Helpers")), FString(UTF8_TO_TCHAR(HelperFunction != nullptr ? HelperFunction->GetScriptSectionName() : "")),
			TEXT("Builder cross-section dependency test should preserve AddOne section name")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderCrossSection_Entry")), FString(UTF8_TO_TCHAR(EntryFunction != nullptr ? EntryFunction->GetScriptSectionName() : "")),
			TEXT("Builder cross-section dependency test should preserve Entry section name")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(SharedStateType != nullptr ? SharedStateType->GetPropertyCount() : 0),
			TEXT("Builder cross-section dependency test should layout SharedState.Value")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(SharedStateType, "Read", 0),
			TEXT("Builder cross-section dependency test should layout SharedState.Read")));
		ASSERT_THAT(IsTrue(HasBytecode(HelperFunction), TEXT("Builder cross-section dependency test should compile AddOne bytecode")));
		ASSERT_THAT(IsTrue(HasBytecode(EntryFunction), TEXT("Builder cross-section dependency test should compile Entry bytecode")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("CrossSectionDependenciesCompileAndKeepSections.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder cross-section dependency test should execute across section boundaries")));
	}
};

#endif
