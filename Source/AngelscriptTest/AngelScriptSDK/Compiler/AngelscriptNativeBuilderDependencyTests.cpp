#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "Support/AngelscriptNativeBuilderDependencyTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

template <typename TDerived, typename TAsserter>
class TBuilderModuleDependencyTestSupport : public TBuilderDependencySharedTestSupport<TDerived, TAsserter>
{
protected:
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
};

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderDependencyTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderDependency",
	TBuilderModuleDependencyTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ModuleDependenciesPreserveTargetsFlagsAndFailureIsolation)
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

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-MODULE-DEPENDENCY",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Builder dependency owner should create a standalone SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		{
			const std::string DependencySource = ASTEST_AS_ANSI(R"AS(
				int DependencyValue()
				{
					return 7;
				}
				)AS");
			const std::string DependentSource = ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					return 42;
				}
				)AS");
			PrintDependencySource(
				*TestRunner,
				TEXT("COMPILER-BUILDER-MODULE-DEPENDENCY-DIRECT-MODULE-PROVIDER"),
				TEXT("BuilderDependencyOwnerDirectProvider"),
				DependencySource);

			FScopedNativeModule Dependency(
				*TestRunner,
				Engine,
				"BuilderDependencyOwnerDirectProvider",
				DependencySource.c_str());
			ASSERT_THAT(IsTrue(
				Dependency.IsValid(),
				TEXT("Direct-module dependency provider should compile")));
			if (Dependency.IsValid())
			{
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-MODULE-DEPENDENCY-DIRECT-MODULE-DEPENDENT"),
					TEXT("BuilderDependencyOwnerDirectDependent"),
					DependentSource);
				FScopedNativeModuleName ModuleScope(
					Engine,
					"BuilderDependencyOwnerDirectDependent");
				asCModule* const Module =
					CreateBuilderModule(ScriptEngine, ModuleScope.Get());
				ASSERT_THAT(IsNotNull(
					Module,
					TEXT("Direct-module dependency should create its dependent module")));
				if (Module != nullptr)
				{
					ASSERT_THAT(IsTrue(
						AddBuilderSectionWithLog(
							*Module,
							"BuilderDependencyOwnerDirectDependent.as",
							DependentSource.c_str(),
							TEXT("DependencyOwner.Direct.AddSection")),
						TEXT("Direct-module dependency should add its dependent source")));
					asCBuilder* const Builder = Module->builder;
					ASSERT_THAT(IsNotNull(
						Builder,
						TEXT("Direct-module dependency should expose its builder")));
					if (Builder != nullptr)
					{
						asCModule* const DependencyModule =
							static_cast<asCModule*>(Dependency.Get());
						ASSERT_THAT(AreEqual(
							0,
							Module->moduleDependencies.Num(),
							TEXT("Direct-module dependency should start with an empty dependency table")));
						Builder->MarkDependency(
							DependencyModule,
							nullptr,
							nullptr);

						const asCModule::FModuleDependencyInfo* const Info =
							Module->moduleDependencies.Find(DependencyModule);
						ASSERT_THAT(IsNotNull(
							Info,
							TEXT("Direct-module dependency should retain the exact provider module target")));
						ASSERT_THAT(AreEqual(
							0,
							Info != nullptr ? Info->FirstLineNumber : INDEX_NONE,
							TEXT("Direct-module dependency without a node should retain zero source line")));
						ASSERT_THAT(AreEqual(
							0,
							Info != nullptr ? Info->FirstColumn : INDEX_NONE,
							TEXT("Direct-module dependency without a node should retain zero source column")));
						ASSERT_THAT(IsFalse(
							Info != nullptr && Info->bIsStructuralDependency,
							TEXT("Direct-module dependency should not claim the structural flag")));
						ASSERT_THAT(IsFalse(
							Info != nullptr && Info->bIsHardValueDependency,
							TEXT("Direct-module dependency should not claim the hard-value flag")));

						Builder->MarkDependency(
							DependencyModule,
							nullptr,
							nullptr);
						ASSERT_THAT(AreEqual(
							1,
							Module->moduleDependencies.Num(),
							TEXT("Repeated direct-module marking should deduplicate the exact target")));
						const bool bLayoutSucceeded =
							RunBuilderPipelineThroughLayout(*Builder, Module);
						ASSERT_THAT(IsTrue(
							bLayoutSucceeded,
							TEXT("Direct-module dependent should build through layout")));
						bool bCodeSucceeded = false;
						if (bLayoutSucceeded)
						{
							bCodeSucceeded = RunBuilderStage(
								*Builder,
								TEXT("DependencyOwner.Direct.BuildCompileCode"),
								&asCBuilder::BuildCompileCode,
								Module);
						}
						ASSERT_THAT(IsTrue(
							bCodeSucceeded,
							TEXT("Direct-module dependent should compile executable bytecode")));
						ASSERT_THAT(IsTrue(
							bCodeSucceeded
								&& HasBytecode(
									Module->GetFunctionByDecl("int Entry()")),
							TEXT("Direct-module dependent should publish executable Entry bytecode")));
					}
				}
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderDependencyOwnerDirectProvider",
				asGM_ONLY_IF_EXISTS),
			TEXT("Direct-module provider should be discarded after its scenario")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderDependencyOwnerDirectDependent",
				asGM_ONLY_IF_EXISTS),
			TEXT("Direct-module dependent should be discarded after its scenario")));

		{
			const std::string DependencySource = ASTEST_AS_ANSI(R"AS(
				class DependencyType
				{
					int Value;
				}
				)AS");
			const std::string DependentSource = ASTEST_AS_ANSI(R"AS(
				class UserType
				{
					int Value;
				}
				)AS");
			PrintDependencySource(
				*TestRunner,
				TEXT("COMPILER-BUILDER-MODULE-DEPENDENCY-STRUCTURAL-TYPE-PROVIDER"),
				TEXT("BuilderDependencyOwnerStructuralProvider"),
				DependencySource);

			FScopedNativeModule Dependency(
				*TestRunner,
				Engine,
				"BuilderDependencyOwnerStructuralProvider",
				DependencySource.c_str());
			ASSERT_THAT(IsTrue(
				Dependency.IsValid(),
				TEXT("Structural dependency provider should compile")));
			if (Dependency.IsValid())
			{
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-MODULE-DEPENDENCY-STRUCTURAL-TYPE-DEPENDENT"),
					TEXT("BuilderDependencyOwnerStructuralDependent"),
					DependentSource);
				FScopedNativeModuleName ModuleScope(
					Engine,
					"BuilderDependencyOwnerStructuralDependent");
				asCModule* const Module =
					CreateBuilderModule(ScriptEngine, ModuleScope.Get());
				ASSERT_THAT(IsNotNull(
					Module,
					TEXT("Structural dependency should create its dependent module")));
				if (Module != nullptr)
				{
					ASSERT_THAT(IsTrue(
						AddBuilderSectionWithLog(
							*Module,
							"BuilderDependencyOwnerStructuralDependent.as",
							DependentSource.c_str(),
							TEXT("DependencyOwner.Structural.AddSection")),
						TEXT("Structural dependency should add its dependent source")));
					asCBuilder* const Builder = Module->builder;
					ASSERT_THAT(IsNotNull(
						Builder,
						TEXT("Structural dependency should expose its builder")));
					if (Builder != nullptr)
					{
						const bool bParsed = RunBuilderStage(
							*Builder,
							TEXT("DependencyOwner.Structural.Parse"),
							&asCBuilder::BuildParallelParseScripts,
							Module);
						ASSERT_THAT(IsTrue(
							bParsed,
							TEXT("Structural dependency should parse its dependent type")));
						bool bTypesGenerated = false;
						if (bParsed)
						{
							bTypesGenerated = RunBuilderStage(
								*Builder,
								TEXT("DependencyOwner.Structural.GenerateTypes"),
								&asCBuilder::BuildGenerateTypes,
								Module);
						}
						ASSERT_THAT(IsTrue(
							bTypesGenerated,
							TEXT("Structural dependency should generate its dependent type")));

						asCTypeInfo* const UserType =
							bTypesGenerated
								? static_cast<asCTypeInfo*>(
									Module->GetTypeInfoByDecl("UserType"))
								: nullptr;
						asCTypeInfo* const DependencyType =
							static_cast<asCTypeInfo*>(
								Dependency.Get()->GetTypeInfoByDecl(
									"DependencyType"));
						ASSERT_THAT(IsNotNull(
							UserType,
							TEXT("Structural dependency should resolve its user type")));
						ASSERT_THAT(IsNotNull(
							DependencyType,
							TEXT("Structural dependency should resolve its exact provider type")));
						ASSERT_THAT(AreEqual(
							Dependency.Get(),
							DependencyType != nullptr
								? DependencyType->GetModule()
								: nullptr,
							TEXT("Structural dependency target type should belong to the provider module")));
						if (UserType != nullptr && DependencyType != nullptr)
						{
							Builder->MarkStructuralDependency(
								UserType,
								DependencyType,
								nullptr,
								nullptr);
							asCModule* const DependencyModule =
								static_cast<asCModule*>(Dependency.Get());
							const asCModule::FModuleDependencyInfo* const Info =
								Module->moduleDependencies.Find(
									DependencyModule);
							ASSERT_THAT(IsNotNull(
								Info,
								TEXT("Structural dependency should retain the exact provider module target")));
							ASSERT_THAT(IsTrue(
								Info != nullptr && Info->bIsStructuralDependency,
								TEXT("Structural dependency should set only its structural flag")));
							ASSERT_THAT(IsFalse(
								Info != nullptr && Info->bIsHardValueDependency,
								TEXT("Structural dependency should not set the hard-value flag")));
							ASSERT_THAT(AreEqual(
								0,
								Info != nullptr
									? Info->FirstLineNumber
									: INDEX_NONE,
								TEXT("Structural dependency without a node should retain zero source line")));
							ASSERT_THAT(AreEqual(
								0,
								Info != nullptr
									? Info->FirstColumn
									: INDEX_NONE,
								TEXT("Structural dependency without a node should retain zero source column")));
							Builder->MarkStructuralDependency(
								UserType,
								DependencyType,
								nullptr,
								nullptr);
							ASSERT_THAT(AreEqual(
								1,
								Module->moduleDependencies.Num(),
								TEXT("Repeated structural marking should deduplicate the exact target")));
						}
					}
				}
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderDependencyOwnerStructuralProvider",
				asGM_ONLY_IF_EXISTS),
			TEXT("Structural provider should be discarded after its scenario")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderDependencyOwnerStructuralDependent",
				asGM_ONLY_IF_EXISTS),
			TEXT("Structural dependent should be discarded after its scenario")));

		{
			const std::string DependencySource = ASTEST_AS_ANSI(R"AS(
				int ConstructedValue()
				{
					return 42;
				}
				)AS");
			const std::string DependentSource = ASTEST_AS_ANSI(R"AS(
				int Entry()
				{
					return 0;
				}
				)AS");
			PrintDependencySource(
				*TestRunner,
				TEXT("COMPILER-BUILDER-MODULE-DEPENDENCY-HARD-FUNCTION-PROVIDER"),
				TEXT("BuilderDependencyOwnerHardProvider"),
				DependencySource);

			FScopedNativeModule Dependency(
				*TestRunner,
				Engine,
				"BuilderDependencyOwnerHardProvider",
				DependencySource.c_str());
			ASSERT_THAT(IsTrue(
				Dependency.IsValid(),
				TEXT("Hard-function dependency provider should compile")));
			if (Dependency.IsValid())
			{
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-MODULE-DEPENDENCY-HARD-FUNCTION-DEPENDENT"),
					TEXT("BuilderDependencyOwnerHardDependent"),
					DependentSource);
				FScopedNativeModuleName ModuleScope(
					Engine,
					"BuilderDependencyOwnerHardDependent");
				asCModule* const Module =
					CreateBuilderModule(ScriptEngine, ModuleScope.Get());
				ASSERT_THAT(IsNotNull(
					Module,
					TEXT("Hard-function dependency should create its dependent module")));
				if (Module != nullptr)
				{
					ASSERT_THAT(IsTrue(
						AddBuilderSectionWithLog(
							*Module,
							"BuilderDependencyOwnerHardDependent.as",
							DependentSource.c_str(),
							TEXT("DependencyOwner.Hard.AddSection")),
						TEXT("Hard-function dependency should add its dependent source")));
					asCBuilder* const Builder = Module->builder;
					asCScriptFunction* const DependencyFunction =
						static_cast<asCScriptFunction*>(
							Dependency.Get()->GetFunctionByDecl(
								"int ConstructedValue()"));
					ASSERT_THAT(IsNotNull(
						Builder,
						TEXT("Hard-function dependency should expose its builder")));
					ASSERT_THAT(IsNotNull(
						DependencyFunction,
						TEXT("Hard-function dependency should resolve its exact provider function")));
					ASSERT_THAT(AreEqual(
						Dependency.Get(),
						DependencyFunction != nullptr
							? DependencyFunction->GetModule()
							: nullptr,
						TEXT("Hard-function dependency target should belong to the provider module")));
					if (Builder != nullptr && DependencyFunction != nullptr)
					{
						Builder->bValueDependenciesAreHard = true;
						Builder->MarkDependency(
							DependencyFunction,
							nullptr,
							nullptr);
						asCModule* const DependencyModule =
							static_cast<asCModule*>(Dependency.Get());
						const asCModule::FModuleDependencyInfo* const Info =
							Module->moduleDependencies.Find(
								DependencyModule);
						ASSERT_THAT(IsNotNull(
							Info,
							TEXT("Hard-function dependency should retain the exact provider module target")));
						ASSERT_THAT(IsTrue(
							Info != nullptr && Info->bIsHardValueDependency,
							TEXT("Hard-function dependency should set only its hard-value flag")));
						ASSERT_THAT(IsFalse(
							Info != nullptr && Info->bIsStructuralDependency,
							TEXT("Hard-function dependency should not set the structural flag")));
						ASSERT_THAT(AreEqual(
							0,
							Info != nullptr ? Info->FirstLineNumber : INDEX_NONE,
							TEXT("Hard-function dependency without a node should retain zero source line")));
						ASSERT_THAT(AreEqual(
							0,
							Info != nullptr ? Info->FirstColumn : INDEX_NONE,
							TEXT("Hard-function dependency without a node should retain zero source column")));
						Builder->MarkDependency(
							DependencyFunction,
							nullptr,
							nullptr);
						ASSERT_THAT(AreEqual(
							1,
							Module->moduleDependencies.Num(),
							TEXT("Repeated hard-function marking should deduplicate the exact target")));
						const bool bLayoutSucceeded =
							RunBuilderPipelineThroughLayout(*Builder, Module);
						ASSERT_THAT(IsTrue(
							bLayoutSucceeded,
							TEXT("Hard-function dependent should build through layout")));
						bool bCodeSucceeded = false;
						if (bLayoutSucceeded)
						{
							bCodeSucceeded = RunBuilderStage(
								*Builder,
								TEXT("DependencyOwner.Hard.BuildCompileCode"),
								&asCBuilder::BuildCompileCode,
								Module);
						}
						ASSERT_THAT(IsTrue(
							bCodeSucceeded,
							TEXT("Hard-function dependent should compile executable bytecode")));
						ASSERT_THAT(IsTrue(
							bCodeSucceeded
								&& HasBytecode(
									Module->GetFunctionByDecl("int Entry()")),
							TEXT("Hard-function dependent should publish executable Entry bytecode")));
					}
				}
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderDependencyOwnerHardProvider",
				asGM_ONLY_IF_EXISTS),
			TEXT("Hard-function provider should be discarded after its scenario")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderDependencyOwnerHardDependent",
				asGM_ONLY_IF_EXISTS),
			TEXT("Hard-function dependent should be discarded after its scenario")));

		{
			const std::string DependencySource = ASTEST_AS_ANSI(R"AS(
				int InitializerValue()
				{
					return 9;
				}
				)AS");
			const std::string DependentSource = ASTEST_AS_ANSI(R"AS(
				const int Value = InitializerValue();

				int Entry()
				{
					return Value;
				}
				)AS");
			PrintDependencySource(
				*TestRunner,
				TEXT("COMPILER-BUILDER-MODULE-DEPENDENCY-GLOBAL-INITIALIZER-PROVIDER"),
				TEXT("BuilderDependencyOwnerInitializerProvider"),
				DependencySource);

			FScopedNativeModule Dependency(
				*TestRunner,
				Engine,
				"BuilderDependencyOwnerInitializerProvider",
				DependencySource.c_str());
			ASSERT_THAT(IsTrue(
				Dependency.IsValid(),
				TEXT("Global-initializer dependency provider should compile")));
			if (Dependency.IsValid())
			{
				PrintDependencySource(
					*TestRunner,
					TEXT("COMPILER-BUILDER-MODULE-DEPENDENCY-GLOBAL-INITIALIZER-DEPENDENT"),
					TEXT("BuilderDependencyOwnerInitializerDependent"),
					DependentSource);
				FScopedNativeModuleName ModuleScope(
					Engine,
					"BuilderDependencyOwnerInitializerDependent");
				asCModule* const Module =
					CreateBuilderModule(ScriptEngine, ModuleScope.Get());
				ASSERT_THAT(IsNotNull(
					Module,
					TEXT("Global-initializer dependency should create its dependent module")));
				if (Module != nullptr)
				{
					ASSERT_THAT(IsTrue(
						AddBuilderSectionWithLog(
							*Module,
							"BuilderDependencyOwnerInitializerDependent.as",
							DependentSource.c_str(),
							TEXT("DependencyOwner.Initializer.AddSection")),
						TEXT("Global-initializer dependency should add its dependent source")));
					asCBuilder* const Builder = Module->builder;
					asCScriptFunction* const DependencyFunction =
						static_cast<asCScriptFunction*>(
							Dependency.Get()->GetFunctionByDecl(
								"int InitializerValue()"));
					ASSERT_THAT(IsNotNull(
						Builder,
						TEXT("Global-initializer dependency should expose its builder")));
					ASSERT_THAT(IsNotNull(
						DependencyFunction,
						TEXT("Global-initializer dependency should resolve its exact provider function")));
					if (Builder != nullptr && DependencyFunction != nullptr)
					{
						Builder->bValueDependenciesAreHard = true;
						Builder->MarkDependency(
							DependencyFunction,
							nullptr,
							nullptr);
						asCModule* const DependencyModule =
							static_cast<asCModule*>(Dependency.Get());
						const asCModule::FModuleDependencyInfo* const Info =
							Module->moduleDependencies.Find(
								DependencyModule);
						ASSERT_THAT(IsNotNull(
							Info,
							TEXT("Global-initializer dependency should retain the exact provider target before rejection")));
						ASSERT_THAT(IsTrue(
							Info != nullptr && Info->bIsHardValueDependency,
							TEXT("Global-initializer dependency should retain the hard-value flag before rejection")));
						ASSERT_THAT(IsFalse(
							Info != nullptr && Info->bIsStructuralDependency,
							TEXT("Global-initializer dependency should not claim the structural flag")));
						ASSERT_THAT(AreEqual(
							0,
							Info != nullptr ? Info->FirstLineNumber : INDEX_NONE,
							TEXT("Global-initializer dependency without a node should retain zero source line")));
						ASSERT_THAT(AreEqual(
							0,
							Info != nullptr ? Info->FirstColumn : INDEX_NONE,
							TEXT("Global-initializer dependency without a node should retain zero source column")));
						Builder->MarkDependency(
							DependencyFunction,
							nullptr,
							nullptr);
						ASSERT_THAT(AreEqual(
							1,
							Module->moduleDependencies.Num(),
							TEXT("Repeated global-initializer dependency marking should deduplicate the target")));
					}

					Engine.ResetMessages();
					ASSERT_THAT(IsTrue(
						Module->Build() < 0,
						TEXT("Unavailable cross-module global initializer should be rejected")));
					ASSERT_THAT(IsTrue(
						AssertBuilderDiagnostic(
							*TestRunner,
							Engine.GetMessages(),
							FExpectedBuilderDiagnostic::Error(
								TEXT("BuilderDependencyOwnerInitializerDependent.as"),
								INDEX_NONE,
								TEXT("InitializerValue")),
							TEXT("Global-initializer rejection should name its exact section and unavailable target")),
						TEXT("Global-initializer rejection should retain its owning diagnostic")));

					bool bPublishedExecutableBytecode = false;
					for (asUINT FunctionIndex = 0;
						FunctionIndex < Module->GetFunctionCount();
						++FunctionIndex)
					{
						if (HasBytecode(
							Module->GetFunctionByIndex(FunctionIndex)))
						{
							bPublishedExecutableBytecode = true;
							break;
						}
					}
					ASSERT_THAT(IsFalse(
						bPublishedExecutableBytecode,
						TEXT("Rejected global initializer should not leak executable bytecode")));
				}
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderDependencyOwnerInitializerProvider",
				asGM_ONLY_IF_EXISTS),
			TEXT("Global-initializer provider should be discarded after its scenario")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"BuilderDependencyOwnerInitializerDependent",
				asGM_ONLY_IF_EXISTS),
			TEXT("Global-initializer dependent should be discarded after its scenario")));
	}

	};

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderDirectDependencyTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderDependency",
	TBuilderModuleDependencyTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DirectMarkDependencyRecordsModuleAndSourceLocation)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained direct dependency smoke; COMPILER-BUILDER-MODULE-DEPENDENCY owns exact target, location, flags, deduplication, cleanup, and isolation.");
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Direct dependency test should create an engine")));
		const std::string DependencySource = ASTEST_AS_ANSI(R"AS(
			int DependencyValue()
			{
				return 7;
			}
			)AS");
		FScopedNativeModule Dependency(
			*TestRunner,
			Engine,
			"DependencyDirect",
			DependencySource.c_str());
		if (!Dependency.IsValid())
		{
			return;
		}
		FScopedNativeModuleName ModuleScope(Engine, "DependentDirect");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Direct dependency test should create a dependent module")));
		const std::string DependentSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 1;
			}
			)AS");
		ASSERT_THAT(IsTrue(
			AddBuilderSectionWithLog(
				*Module,
				"DependentDirect.as",
				DependentSource.c_str(),
				TEXT("DirectDependency.Add")),
			TEXT("Direct dependency test should create a builder")));
		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Direct dependency test should expose the builder")));
		asCModule* DependencyModule = static_cast<asCModule*>(Dependency.Get());
		Builder->MarkDependency(DependencyModule, nullptr, nullptr);
		const asCModule::FModuleDependencyInfo* Info = Module->moduleDependencies.Find(DependencyModule);
		ASSERT_THAT(IsNotNull(Info, TEXT("Direct dependency should record the dependency module")));
		ASSERT_THAT(AreEqual(0, Info != nullptr ? Info->FirstLineNumber : INDEX_NONE, TEXT("Dependency without a node should record a zero source line")));
		ASSERT_THAT(AreEqual(0, Info != nullptr ? Info->FirstColumn : INDEX_NONE, TEXT("Dependency without a node should record a zero source column")));
		Builder->MarkDependency(DependencyModule, nullptr, nullptr);
		ASSERT_THAT(AreEqual(1, Module->moduleDependencies.Num(), TEXT("Repeated direct dependency marking should not duplicate the record")));
	}

	};

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderStructuralDependencyTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderDependency",
	TBuilderModuleDependencyTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExplicitMarkStructuralDependencyRecordsStructuralFlag)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained structural dependency smoke; COMPILER-BUILDER-MODULE-DEPENDENCY owns exact type target, structural-only flags, location, deduplication, cleanup, and isolation.");
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Structural dependency test should create an engine")));
		const std::string DependencySource = ASTEST_AS_ANSI(R"AS(
			class DependencyType
			{
				int Value;
			}
			)AS");
		FScopedNativeModule Dependency(
			*TestRunner,
			Engine,
			"DependencyStructural",
			DependencySource.c_str());
		if (!Dependency.IsValid())
		{
			return;
		}
		FScopedNativeModuleName ModuleScope(Engine, "DependentStructural");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Structural dependency test should create a dependent module")));
		const std::string DependentSource = ASTEST_AS_ANSI(R"AS(
			class UserType
			{
				int Value;
			}
			)AS");
		ASSERT_THAT(IsTrue(
			AddBuilderSectionWithLog(
				*Module,
				"DependentStructural.as",
				DependentSource.c_str(),
				TEXT("StructuralDependency.Add")),
			TEXT("Structural dependency test should create a builder")));
		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Structural dependency test should expose the builder")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildParallelParseScripts(), TEXT("Structural dependency test should parse the dependent source")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Builder->BuildGenerateTypes(), TEXT("Structural dependency test should register the user type")));
		asCTypeInfo* UserType = static_cast<asCTypeInfo*>(Module->GetTypeInfoByDecl("UserType"));
		asCTypeInfo* DependencyType = static_cast<asCTypeInfo*>(Dependency.Get()->GetTypeInfoByDecl("DependencyType"));
		ASSERT_THAT(IsNotNull(UserType, TEXT("Structural dependency test should register the user type")));
		ASSERT_THAT(IsNotNull(DependencyType, TEXT("Structural dependency test should expose the dependency type")));
		Builder->MarkStructuralDependency(UserType, DependencyType, nullptr, nullptr);
		const asCModule::FModuleDependencyInfo* Info = Module->moduleDependencies.Find(static_cast<asCModule*>(Dependency.Get()));
		ASSERT_THAT(IsNotNull(Info, TEXT("Structural dependency should record the dependency module")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->bIsStructuralDependency, TEXT("Structural dependency should set its structural flag")));
	}

	};

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderHardValueDependencyTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderDependency",
	TBuilderModuleDependencyTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DefaultConstructorCallMarksHardValueDependency)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained hard-function dependency smoke despite its historical method name; COMPILER-BUILDER-MODULE-DEPENDENCY owns the actual ordinary-function target, hard-only flags, deduplication, cleanup, and isolation.");
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Hard-value dependency test should create an engine")));
		const std::string DependencySource = ASTEST_AS_ANSI(R"AS(
			int ConstructedValue()
			{
				return 42;
			}
			)AS");
		FScopedNativeModule Dependency(
			*TestRunner,
			Engine,
			"DependencyHardValue",
			DependencySource.c_str());
		if (!Dependency.IsValid())
		{
			return;
		}
		FScopedNativeModuleName ModuleScope(Engine, "DependentHardValue");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Hard-value dependency test should create a dependent module")));
		const std::string DependentSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 0;
			}
			)AS");
		ASSERT_THAT(IsTrue(
			AddBuilderSectionWithLog(
				*Module,
				"DependentHardValue.as",
				DependentSource.c_str(),
				TEXT("HardValueDependency.Add")),
			TEXT("Hard-value dependency test should create a builder")));
		asCBuilder* Builder = Module->builder;
		asCScriptFunction* DependencyFunction = static_cast<asCScriptFunction*>(Dependency.Get()->GetFunctionByDecl("int ConstructedValue()"));
		ASSERT_THAT(IsNotNull(Builder, TEXT("Hard-value dependency test should expose the builder")));
		ASSERT_THAT(IsNotNull(DependencyFunction, TEXT("Hard-value dependency test should expose the dependency function")));
		Builder->bValueDependenciesAreHard = true;
		Builder->MarkDependency(DependencyFunction, nullptr, nullptr);
		const asCModule::FModuleDependencyInfo* Info = Module->moduleDependencies.Find(static_cast<asCModule*>(Dependency.Get()));
		ASSERT_THAT(IsNotNull(Info, TEXT("Hard-value dependency should record the dependency module")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->bIsHardValueDependency, TEXT("Value dependency under the hard-value policy should set its hard flag")));
	}

	};

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderGlobalInitializerDependencyTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderDependency",
	TBuilderModuleDependencyTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(GlobalInitializerRejectsCrossModuleFunctionDependency)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained cross-module initializer rejection smoke; COMPILER-BUILDER-MODULE-DEPENDENCY owns exact target, hard-only flags, deduplication, diagnostic, executable exclusion, cleanup, and isolation.");
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Cross-module initializer test should create an engine")));
		const std::string DependencySource = ASTEST_AS_ANSI(R"AS(
			int InitializerValue()
			{
				return 9;
			}
			)AS");
		FScopedNativeModule Dependency(
			*TestRunner,
			Engine,
			"DependencyInitializer",
			DependencySource.c_str());
		if (!Dependency.IsValid())
		{
			return;
		}
		FScopedNativeModuleName ModuleScope(Engine, "DependentInitializer");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Cross-module initializer test should create a dependent module")));
		const std::string DependentSource = ASTEST_AS_ANSI(R"AS(
			int Value = InitializerValue();
			)AS");
		ASSERT_THAT(IsTrue(
			AddBuilderSectionWithLog(
				*Module,
				"DependentInitializer.as",
				DependentSource.c_str(),
				TEXT("InitializerDependency.Add")),
			TEXT("Cross-module initializer test should create a builder")));
		asCBuilder* Builder = Module->builder;
		asCScriptFunction* DependencyFunction = static_cast<asCScriptFunction*>(Dependency.Get()->GetFunctionByDecl("int InitializerValue()"));
		ASSERT_THAT(IsNotNull(Builder, TEXT("Cross-module initializer test should expose the builder")));
		ASSERT_THAT(IsNotNull(DependencyFunction, TEXT("Cross-module initializer test should expose the dependency function")));
		Builder->bValueDependenciesAreHard = true;
		Builder->MarkDependency(DependencyFunction, nullptr, nullptr);
		const asCModule::FModuleDependencyInfo* Info = Module->moduleDependencies.Find(static_cast<asCModule*>(Dependency.Get()));
		ASSERT_THAT(IsNotNull(Info, TEXT("Cross-module function use should be tracked before global initialization")));
		ASSERT_THAT(IsTrue(Info != nullptr && Info->bIsHardValueDependency, TEXT("Cross-module initializer function dependencies should be hard value dependencies")));
		ASSERT_THAT(IsTrue(Module->Build() < 0, TEXT("A global initializer must reject an unavailable cross-module function")));
		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0, TEXT("Rejected cross-module global initialization should report a diagnostic")));
	}

	};

#endif
