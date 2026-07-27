#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderLayoutTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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
	TEST_METHOD(ClassLayoutPreservesPropertiesMethodsAndBaseType)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained focused inheritance-layout smoke; COMPILER-BUILDER-CLASS-LAYOUT owns inheritance, initializer, and overload evidence at layout and code stages.");

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder class-layout test should create an engine")));
		FScopedNativeModuleName ModuleScope(Engine, "BuilderClassLayout");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder class-layout test should create a module")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class Base
			{
				int BaseValue;
				int ReadBase()
				{
					return BaseValue;
				}
			}

			class Derived : Base
			{
				float Weight;
				int ReadDerived()
				{
					return ReadBase();
				}
			}
		)AS");
		PrintGeneratedAsSource(*TestRunner, TEXT("COMPILER-BUILDER-CLASS-LAYOUT-LEGACY-INHERITANCE"), TEXT("ClassLayout.as"), UTF8_TO_TCHAR(Source.c_str()));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "ClassLayout.as", Source.c_str(), TEXT("ClassLayout.Add")), TEXT("Builder class-layout test should add source")));
		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder class-layout test should create a builder")));
		if (!RunBuilderPipelineThroughLayout(*Builder, Module))
		{
			ReportBuilderFailureDiagnostics(Engine);
			return;
		}
		asITypeInfo* Base = Module->GetTypeInfoByDecl("Base");
		asITypeInfo* Derived = Module->GetTypeInfoByDecl("Derived");
		ASSERT_THAT(IsNotNull(Base, TEXT("Builder class-layout test should expose Base")));
		ASSERT_THAT(IsNotNull(Derived, TEXT("Builder class-layout test should expose Derived")));
		ASSERT_THAT(AreEqual(Base, Derived->GetBaseType(), TEXT("Builder class-layout test should preserve the base-type relationship")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Base->GetPropertyCount()), TEXT("Builder class-layout test should preserve Base properties")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Derived->GetPropertyCount()), TEXT("Builder class-layout test should preserve inherited and declared Derived properties")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(Base, "ReadBase", 0), TEXT("Builder class-layout test should layout Base methods")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(Derived, "ReadDerived", 0), TEXT("Builder class-layout test should layout Derived methods")));
	}

	TEST_METHOD(PropertyInitializersAreTrackedAndDefaultInitFunctionCompiles)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained focused property-initializer smoke; COMPILER-BUILDER-CLASS-LAYOUT owns initializer metadata, bytecode, and runtime evidence at both declared stages.");

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder property-initializer test should create an engine")));
		FScopedNativeModuleName ModuleScope(Engine, "BuilderPropertyInitializers");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder property-initializer test should create a module")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class Values
			{
				int First = 20;
				int Second = 22;
			}

			int Entry()
			{
				Values Value;
				return Value.First + Value.Second;
			}
		)AS");
		PrintGeneratedAsSource(*TestRunner, TEXT("COMPILER-BUILDER-CLASS-LAYOUT-LEGACY-INITIALIZER"), TEXT("PropertyInitializers.as"), UTF8_TO_TCHAR(Source.c_str()));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "PropertyInitializers.as", Source.c_str(), TEXT("PropertyInitializers.Add")), TEXT("Builder property-initializer test should add source")));
		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder property-initializer test should create a builder")));
		if (!RunBuilderPipelineThroughLayout(*Builder, Module)
			|| !RunBuilderStage(
				*Builder,
				TEXT("PropertyInitializers.Compile"),
				&asCBuilder::BuildCompileCode,
				Module))
		{
			ReportBuilderFailureDiagnostics(Engine);
			return;
		}
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Builder->classDeclarations.GetLength()), TEXT("Builder property-initializer test should retain one class description")));
		const sClassDeclaration* Declaration = Builder->classDeclarations.GetLength() > 0 ? Builder->classDeclarations[0] : nullptr;
		ASSERT_THAT(IsNotNull(Declaration, TEXT("Builder property-initializer test should retain its class description")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Declaration != nullptr ? Declaration->propInits.GetLength() : 0), TEXT("Builder property-initializer test should track both initializers")));
		asITypeInfo* Type = Module->GetTypeInfoByDecl("Values");
		ASSERT_THAT(IsNotNull(Type, TEXT("Builder property-initializer test should expose Values")));
		ASSERT_THAT(IsTrue(Type->GetBehaviourCount() > 0, TEXT("Builder property-initializer test should create initialization behaviour")));
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Entry, TEXT("Builder property-initializer test should publish Entry")));
		ASSERT_THAT(IsTrue(HasBytecode(Entry), TEXT("Builder property-initializer test should compile Entry bytecode")));
	}

	TEST_METHOD(GlobalFunctionOverloadsKeepDistinctLayouts)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained focused global-overload smoke; COMPILER-BUILDER-CLASS-LAYOUT owns overload metadata, bytecode, and runtime evidence with the other class-layout shapes.");

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder overload-layout test should create an engine")));
		FScopedNativeModuleName ModuleScope(Engine, "BuilderOverloadLayout");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder overload-layout test should create a module")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Choose(int Value)
			{
				return Value + 1;
			}

			int Choose(float Value)
			{
				return int(Value) + 2;
			}

			int Entry()
			{
				return Choose(20) + Choose(20.0f);
			}
		)AS");
		PrintGeneratedAsSource(*TestRunner, TEXT("COMPILER-BUILDER-CLASS-LAYOUT-LEGACY-OVERLOAD"), TEXT("OverloadLayout.as"), UTF8_TO_TCHAR(Source.c_str()));
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "OverloadLayout.as", Source.c_str(), TEXT("OverloadLayout.Add")), TEXT("Builder overload-layout test should add source")));
		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder overload-layout test should create a builder")));
		if (!RunBuilderPipelineThroughLayout(*Builder, Module)
			|| !RunBuilderStage(
				*Builder,
				TEXT("OverloadLayout.Compile"),
				&asCBuilder::BuildCompileCode,
				Module))
		{
			ReportBuilderFailureDiagnostics(Engine);
			return;
		}
		TArray<asIScriptFunction*> Overloads;
		TArray<int> ParameterTypeIds;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module->GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* Candidate = Module->GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr || FCStringAnsi::Strcmp(Candidate->GetName(), "Choose") != 0 || Candidate->GetParamCount() != 1)
			{
				continue;
			}
			int TypeId = asINVALID_TYPE;
			Candidate->GetParam(0, &TypeId);
			Overloads.Add(Candidate);
			ParameterTypeIds.Add(TypeId);
		}
		ASSERT_THAT(AreEqual(2, Overloads.Num(), TEXT("Builder overload-layout test should expose both Choose overloads")));
		ASSERT_THAT(IsTrue(ParameterTypeIds.Num() == 2 && ParameterTypeIds[0] != ParameterTypeIds[1], TEXT("Builder overload-layout test should preserve distinct parameter type layouts")));
		ASSERT_THAT(IsTrue(Overloads.Num() == 2 && HasBytecode(Overloads[0]) && HasBytecode(Overloads[1]), TEXT("Builder overload-layout test should compile both overloads")));
		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(43, Result, TEXT("Builder overload-layout test should dispatch both overloads")));
	}

	TEST_METHOD(ClassLayoutsPreserveInheritanceInitializersAndOverloads)
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
		AS_NATIVE_PRODUCT("COMPILER-BUILDER-CLASS-LAYOUT",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Bytecode
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder compile test should create a standalone SDK engine")));

		const int IsolationControlResult = ScriptEngine->RegisterObjectType(
			"BuilderLayoutIsolationControl",
			0,
			asOBJ_REF | asOBJ_NOCOUNT);
		ASSERT_THAT(IsTrue(
			IsolationControlResult >= 0,
			TEXT("Builder class-layout test should register an independent type control")));
		asITypeInfo* const InitialIsolationControl =
			ScriptEngine->GetTypeInfoByDecl("BuilderLayoutIsolationControl");
		ASSERT_THAT(IsNotNull(
			InitialIsolationControl,
			TEXT("Builder class-layout test should establish the control type identity baseline")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderCompileCode");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder compile test should create a backing module")));

		const std::string CompilePipelineSource = ASTEST_AS_ANSI(R"AS(
			class BaseCounter
			{
				int BaseValue = 20;

				int ReadBase()
				{
					return BaseValue;
				}
			}

			class Counter : BaseCounter
			{
				int ExtraValue = 2;

				int Choose(int Value)
				{
					return Value;
				}

				int Choose(float Value)
				{
					return int(Value);
				}

				int Read()
				{
					return ReadBase() + ExtraValue;
				}
			}

			int Entry(Counter Value)
			{
				return Value.Read() + Value.Choose(10) + Value.Choose(10.0f);
			}
			)AS");
		const TCHAR* Shapes[] = { TEXT("inheritance"), TEXT("property_initializer"), TEXT("method_overload") };
		const TCHAR* Stages[] = { TEXT("layout"), TEXT("code") };
		for (const TCHAR* Shape : Shapes)
		{
			for (const TCHAR* Stage : Stages)
			{
				const FString CaseId = MakeNativeCaseId("COMPILER-BUILDER-CLASS-LAYOUT", { Shape, Stage });
				PrintGeneratedAsSource(*TestRunner, CaseId, TEXT("BuilderClassLayout.as"), UTF8_TO_TCHAR(CompilePipelineSource.c_str()));
			}
		}
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderCompile", CompilePipelineSource.c_str(), TEXT("ClassLayoutsPreserveInheritanceInitializersAndOverloads.AddCompileSection")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder compile test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder should build through layout")))
		{
			ReportBuilderFailureDiagnostics(Engine);
			return;
		}
		asITypeInfo* CounterType = Module->GetTypeInfoByDecl("Counter");
		asITypeInfo* BaseType = Module->GetTypeInfoByDecl("BaseCounter");
		ASSERT_THAT(IsNotNull(CounterType, TEXT("Layout stage should expose Counter type metadata")));
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Layout stage should expose BaseCounter type metadata")));
		ASSERT_THAT(AreEqual(BaseType, CounterType != nullptr ? CounterType->GetBaseType() : nullptr,
			TEXT("Layout stage should retain the exact base type")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(CounterType != nullptr ? CounterType->GetPropertyCount() : 0),
			TEXT("Layout stage should expose inherited and declared initialized properties")));
		ASSERT_THAT(IsNotNull(GetNativeMethodByDecl(CounterType, "int Choose(const int)"),
			TEXT("Layout stage should retain the integer overload")));
		ASSERT_THAT(IsNotNull(GetNativeMethodByDecl(CounterType, "int Choose(const float)"),
			TEXT("Layout stage should retain the float overload")));
		if (!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("ClassLayoutsPreserveInheritanceInitializersAndOverloads.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder compile test should compile function bytecode")))
		{
			ReportBuilderFailureDiagnostics(Engine);
			return;
		}

		asIScriptFunction* Function =
			GetNativeFunctionByDecl(
				Module,
				"int Entry(Counter)");
		ASSERT_THAT(IsNotNull(Function, TEXT("Builder compile should expose Entry after code generation")));
		if (Function == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "Entry"),
			TEXT("Builder compile should retain Entry function description")));

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		ASSERT_THAT(IsNotNull(Bytecode, TEXT("Builder compile should emit bytecode for Entry")));
		ASSERT_THAT(IsTrue(BytecodeLength > 0, TEXT("Builder compile should emit at least one bytecode dword")));
		ASSERT_THAT(IsNotNull(CounterType, TEXT("Builder compile should expose Counter type metadata")));
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Builder compile should expose BaseCounter type metadata")));
		ASSERT_THAT(AreEqual(BaseType, CounterType != nullptr ? CounterType->GetBaseType() : nullptr,
			TEXT("Builder class layout should retain the exact base type")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(CounterType != nullptr ? CounterType->GetPropertyCount() : 0),
			TEXT("Builder class layout should expose inherited and declared properties")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(CounterType, "Read", 0),
			TEXT("Builder function layout should expose Counter.Read")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "Read", "Counter"),
			TEXT("Builder compile should retain Counter.Read function description")));
		ASSERT_THAT(IsNotNull(GetNativeMethodByDecl(CounterType, "int Choose(const int)"),
			TEXT("Builder class layout should retain the integer overload")));
		ASSERT_THAT(IsNotNull(GetNativeMethodByDecl(CounterType, "int Choose(const float)"),
			TEXT("Builder class layout should retain the float overload")));

		// Direct builder-stage calls expose the intermediate metadata and
		// bytecode under test, but intentionally bypass the complete public
		// module-build lifecycle. Execute the same source through Build() so
		// generated script-class factories receive engine finalization.
		Function = nullptr;
		Bytecode = nullptr;
		CounterType = nullptr;
		BaseType = nullptr;
		if (Module->builder != nullptr)
		{
			asDELETE(Module->builder, asCBuilder);
		}
		Module->builder = nullptr;
		Builder = nullptr;
		ASSERT_THAT(IsNull(
			Module->builder,
			TEXT("Builder class-layout cleanup should release its transient builder and parser trees")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("Builder class-layout cleanup should explicitly discard the staged module")));
		Module = nullptr;
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Builder class-layout staged module should be absent after explicit discard")));

		{
			FScopedNativeModule RuntimeModuleScope(
				*TestRunner,
				Engine,
				"BuilderClassLayoutRuntime",
				CompilePipelineSource);
			ASSERT_THAT(IsTrue(
				RuntimeModuleScope.IsValid(),
				TEXT("Builder class-layout runtime counterpart should build through the public module path")));
			asITypeInfo* const RuntimeCounterType =
				RuntimeModuleScope.IsValid()
					? RuntimeModuleScope->GetTypeInfoByDecl("Counter")
					: nullptr;
			ASSERT_THAT(IsNotNull(
				RuntimeCounterType,
				TEXT("Builder class-layout runtime counterpart should expose Counter")));
			void* const CounterObject =
				RuntimeCounterType != nullptr
					? ScriptEngine->CreateScriptObject(RuntimeCounterType)
					: nullptr;
			ASSERT_THAT(IsNotNull(
				CounterObject,
				TEXT("Builder class-layout runtime counterpart should construct Counter through the public API")));
			if (CounterObject == nullptr)
			{
				return;
			}
			{
				ON_SCOPE_EXIT
				{
					ScriptEngine->ReleaseScriptObject(
						CounterObject,
						RuntimeCounterType);
				};

				FSdkFunctionInvoker Invoker(
					*TestRunner,
					ScriptEngine,
					RuntimeModuleScope.Get(),
					"int Entry(Counter)");
				const int32 Result = Invoker
					.AddArgObject(CounterObject)
					.CallAndReturn<int32>();
				LogScriptExecutionResult(
					TEXT("ClassLayoutsPreserveInheritanceInitializersAndOverloads.Entry"),
					"int Entry(Counter)",
					Result);
				ASSERT_THAT(AreEqual(42, Result, TEXT("Builder compile should execute compiled bytecode")));
			}

			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				RuntimeModuleScope.Discard(),
				TEXT("Builder class-layout cleanup should explicitly discard the runtime module")));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule("BuilderClassLayoutRuntime", asGM_ONLY_IF_EXISTS),
				TEXT("Builder class-layout runtime module should be absent after explicit discard")));
		}

		ASSERT_THAT(AreEqual(
			InitialIsolationControl,
			ScriptEngine->GetTypeInfoByDecl("BuilderLayoutIsolationControl"),
			TEXT("Builder class-layout cleanup should preserve the independent type identity baseline")));

		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("Builder class-layout isolation should create an independent engine")));
		if (IndependentEngine.Get() != nullptr)
		{
			ASSERT_THAT(IsNull(
				IndependentEngine.Get()->GetTypeInfoByDecl("Counter"),
				TEXT("Builder class-layout types should not leak into an independent engine")));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				IndependentEngine.Get()->GetModuleCount(),
				TEXT("Builder class-layout modules should remain local to the owning engine")));
		}
	}
};

#endif
