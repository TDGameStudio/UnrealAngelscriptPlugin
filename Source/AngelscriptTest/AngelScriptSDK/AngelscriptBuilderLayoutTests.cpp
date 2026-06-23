#include "AngelscriptBuilderTestSupport.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptBuilderTestSupport;
using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptBuilderLayoutTests,
	"Angelscript.TestModule.AngelScriptSDK.Builder.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	inline static FNativeTestEngine Engine;

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

	TEST_METHOD(ClassLayoutPreservesPropertiesMethodsAndBaseType)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder layout test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderLayoutClassShape");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder layout test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class BaseState
			{
				int BaseValue;

				int ReadBase()
				{
					return BaseValue;
				}
			}

			class DerivedState : BaseState
			{
				int Delta;

				int ReadDelta()
				{
					return Delta;
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderLayoutClassShape.as", Source.c_str(), TEXT("ClassShape.AddSection")),
			TEXT("Builder layout test should add the layout section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder layout test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module), TEXT("Builder layout test should build through layout")));

		sClassDeclaration* BaseDeclaration = FindClassDeclarationByName(*Builder, "BaseState");
		sClassDeclaration* DerivedDeclaration = FindClassDeclarationByName(*Builder, "DerivedState");
		ASSERT_THAT(IsNotNull(BaseDeclaration, TEXT("Builder layout test should retain BaseState declaration")));
		ASSERT_THAT(IsNotNull(DerivedDeclaration, TEXT("Builder layout test should retain DerivedState declaration")));
		ASSERT_THAT(IsTrue(BaseDeclaration != nullptr && BaseDeclaration->hasResolved, TEXT("Builder layout test should resolve BaseState")));
		ASSERT_THAT(IsTrue(DerivedDeclaration != nullptr && DerivedDeclaration->hasResolved, TEXT("Builder layout test should resolve DerivedState")));
		ASSERT_THAT(IsTrue(BaseDeclaration != nullptr && BaseDeclaration->hasLayouted, TEXT("Builder layout test should layout BaseState")));
		ASSERT_THAT(IsTrue(DerivedDeclaration != nullptr && DerivedDeclaration->hasLayouted, TEXT("Builder layout test should layout DerivedState")));

		asITypeInfo* BaseType = Module->GetTypeInfoByDecl("BaseState");
		asITypeInfo* DerivedType = Module->GetTypeInfoByDecl("DerivedState");
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Builder layout test should expose BaseState")));
		ASSERT_THAT(IsNotNull(DerivedType, TEXT("Builder layout test should expose DerivedState")));
		ASSERT_THAT(AreEqual(BaseType, DerivedType != nullptr ? DerivedType->GetBaseType() : nullptr,
			TEXT("Builder layout test should preserve direct base type")));
		ASSERT_THAT(IsTrue(DerivedType != nullptr && BaseType != nullptr && DerivedType->DerivesFrom(BaseType),
			TEXT("Builder layout test should preserve derives-from relationship")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(BaseType != nullptr ? BaseType->GetPropertyCount() : 0),
			TEXT("Builder layout test should expose BaseState.BaseValue")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(DerivedType != nullptr ? DerivedType->GetPropertyCount() : 0),
			TEXT("Builder layout test should expose inherited BaseValue and DerivedState.Delta")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(BaseType, "ReadBase", 0), TEXT("Builder layout test should expose BaseState.ReadBase")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(DerivedType, "ReadDelta", 0), TEXT("Builder layout test should expose DerivedState.ReadDelta")));
	}

	TEST_METHOD(PropertyInitializersAreTrackedAndDefaultInitFunctionCompiles)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder initializer layout test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderLayoutInitializers");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder initializer layout test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			class Accumulator
			{
				int Base = 40;
				int Delta = 2;

				int Read()
				{
					return Base + Delta;
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderLayoutInitializers.as", Source.c_str(), TEXT("Initializers.AddSection")),
			TEXT("Builder initializer layout test should add the initializer section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder initializer layout test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module), TEXT("Builder initializer layout test should build through layout")));

		sClassDeclaration* AccumulatorDeclaration = FindClassDeclarationByName(*Builder, "Accumulator");
		ASSERT_THAT(IsNotNull(AccumulatorDeclaration, TEXT("Builder initializer layout test should retain Accumulator declaration")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(AccumulatorDeclaration != nullptr ? AccumulatorDeclaration->propInits.GetLength() : 0),
			TEXT("Builder initializer layout test should retain both property initializers")));

		asITypeInfo* AccumulatorType = Module->GetTypeInfoByDecl("Accumulator");
		ASSERT_THAT(IsNotNull(AccumulatorType, TEXT("Builder initializer layout test should expose Accumulator")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(AccumulatorType != nullptr ? AccumulatorType->GetPropertyCount() : 0),
			TEXT("Builder initializer layout test should layout both properties")));
		ASSERT_THAT(IsNotNull(FindTypeMethodByNameAndParamCount(AccumulatorType, "Read", 0),
			TEXT("Builder initializer layout test should expose Read method")));
	}

	TEST_METHOD(GlobalFunctionOverloadsKeepDistinctLayouts)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder overload layout test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderLayoutGlobalOverloads");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder overload layout test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Pick()
			{
				return 40;
			}

			int Pick(int Value)
			{
				return Value + 2;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderLayoutGlobalOverloads.as", Source.c_str(), TEXT("GlobalOverloads.AddSection")),
			TEXT("Builder overload layout test should add the overload section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder overload layout test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderPipelineThroughLayout(*TestRunner, *Builder, Module), TEXT("Builder overload layout test should build through layout")));

		ASSERT_THAT(AreEqual(2, CountGlobalFunctionDescriptions(*Builder, "Pick"), TEXT("Builder overload layout test should keep both Pick descriptions")));
		asIScriptFunction* PickNoArg = FindModuleFunctionByNameAndParamCount(Module, "Pick", 0);
		asIScriptFunction* PickWithArg = FindModuleFunctionByNameAndParamCount(Module, "Pick", 1);
		ASSERT_THAT(IsNotNull(PickNoArg, TEXT("Builder overload layout test should expose Pick()")));
		ASSERT_THAT(IsNotNull(PickWithArg, TEXT("Builder overload layout test should expose Pick(int)")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(PickNoArg != nullptr ? PickNoArg->GetParamCount() : 0), TEXT("Builder overload layout test should preserve Pick() arity")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(PickWithArg != nullptr ? PickWithArg->GetParamCount() : 0), TEXT("Builder overload layout test should preserve Pick(int) arity")));
	}
};

#endif
