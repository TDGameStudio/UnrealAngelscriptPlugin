#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBuilderTypeTests, "Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderType", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

	TEST_METHOD(GenerateTypesRegistersDeclarations)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder type-generation test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderGenerateTypes");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder type-generation test should create a backing module")));

		const std::string TypeDeclarationsSource = ASTEST_AS_ANSI(R"AS(
			enum EState
			{
				Idle,
				Busy
			}

			class ActorState
			{
				int Value;

				int GetValue()
				{
					return Value;
				}
			}

			namespace Types
			{
				class NestedState
				{
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderTypes", TypeDeclarationsSource.c_str(), TEXT("GenerateTypesRegistersDeclarations.AddTypes")),
			TEXT("Builder test should add a script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder type-generation test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("GenerateTypesRegistersDeclarations.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module), TEXT("Builder type-generation test should parse scripts")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("GenerateTypesRegistersDeclarations.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module), TEXT("Builder type-generation test should generate types")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		ASSERT_THAT(IsTrue(Builder->classDeclarations.GetLength() >= 2,
			TEXT("Builder type-generation should discover class declarations")));
		sClassDeclaration* ActorStateDeclaration = FindClassDeclarationByName(*Builder, "ActorState");
		ASSERT_THAT(IsNotNull(ActorStateDeclaration,
			TEXT("Builder type-generation should retain the ActorState class declaration")));
		ASSERT_THAT(IsNotNull(ActorStateDeclaration != nullptr ? ActorStateDeclaration->node : nullptr,
			TEXT("Builder type-generation should keep the ActorState declaration AST node")));
		ASSERT_THAT(IsNotNull(ActorStateDeclaration != nullptr ? ActorStateDeclaration->typeInfo : nullptr,
			TEXT("Builder type-generation should attach ActorState type info to the declaration")));
		sClassDeclaration* NestedStateDeclaration = FindClassDeclarationByName(*Builder, "NestedState");
		ASSERT_THAT(IsNotNull(NestedStateDeclaration,
			TEXT("Builder type-generation should retain the namespaced NestedState class declaration")));
		ASSERT_THAT(IsNotNull(NestedStateDeclaration != nullptr ? NestedStateDeclaration->typeInfo : nullptr,
			TEXT("Builder type-generation should attach NestedState type info to the declaration")));
		sClassDeclaration* EnumDeclaration = FindNamedTypeDeclarationByName(*Builder, "EState");
		ASSERT_THAT(IsNotNull(EnumDeclaration,
			TEXT("Builder type-generation should retain the EState named type declaration")));
		ASSERT_THAT(IsNotNull(EnumDeclaration != nullptr ? EnumDeclaration->typeInfo : nullptr,
			TEXT("Builder type-generation should attach EState type info to the named declaration")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->interfaceDeclarations.GetLength()),
			TEXT("Builder type-generation should not require rejected interface syntax")));
		asITypeInfo* ActorStateType = Module->GetTypeInfoByDecl("ActorState");
		ASSERT_THAT(IsNotNull(ActorStateType,
			TEXT("Builder type-generation should register the class type on the module")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(ActorStateType != nullptr ? ActorStateType->GetPropertyCount() : 0),
			TEXT("Builder type-generation should not layout class properties before class layout")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(ActorStateType != nullptr ? ActorStateType->GetMethodCount() : 0),
			TEXT("Builder type-generation should not register class methods before function generation")));
		ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("Types::NestedState"),
			TEXT("Builder type-generation should register the namespaced class type on the module")));
		asITypeInfo* StateEnumType = Module->GetTypeInfoByDecl("EState");
		ASSERT_THAT(IsNotNull(StateEnumType,
			TEXT("Builder type-generation should register the enum type on the module")));
		ASSERT_THAT(IsTrue(StateEnumType != nullptr && StateEnumType->GetEnumValueCount() >= 0,
			TEXT("Builder type-generation should expose enum metadata before later layout stages")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder type-generation should not register global functions before function generation")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetGlobalVarCount()),
			TEXT("Builder type-generation should not register script globals before function generation")));
	}
	TEST_METHOD(ClassInheritanceResolvesBaseTypesAndInheritedCalls)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder inheritance test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderClassInheritance");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder inheritance test should create a backing module")));

		const std::string InheritanceSource = ASTEST_AS_ANSI(R"AS(
			class BaseState
			{
				int BaseValue = 40;

				int ReadBase()
				{
					return BaseValue;
				}
			}

			class DerivedState : BaseState
			{
				int Delta = 2;

				int ReadDelta()
				{
					return Delta;
				}
			}

			int UseInheritedMember()
			{
				DerivedState Value;
				return Value.ReadBase() + Value.ReadDelta();
			}

			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderClassInheritance", InheritanceSource.c_str(), TEXT("ClassInheritanceResolvesBaseTypesAndInheritedCalls.AddInheritanceSection")),
			TEXT("Builder inheritance test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder inheritance test should create a builder")));

		if (!this->Assert.IsTrue(RunBuilderPipelineThroughLayout(*Builder, Module), TEXT("Builder inheritance test should build through layout")) ||
			!this->Assert.IsTrue(RunBuilderStage(*Builder, TEXT("ClassInheritanceResolvesBaseTypesAndInheritedCalls.BuildCompileCode"), &asCBuilder::BuildCompileCode, Module), TEXT("Builder inheritance test should compile bytecode")))
		{
			ReportBuilderFailureDiagnostics();
			return;
		}

		sClassDeclaration* BaseDeclaration = FindClassDeclarationByName(*Builder, "BaseState");
		sClassDeclaration* DerivedDeclaration = FindClassDeclarationByName(*Builder, "DerivedState");
		ASSERT_THAT(IsNotNull(BaseDeclaration,
			TEXT("Builder inheritance test should retain the BaseState declaration")));
		ASSERT_THAT(IsNotNull(DerivedDeclaration,
			TEXT("Builder inheritance test should retain the DerivedState declaration")));
		ASSERT_THAT(IsNotNull(BaseDeclaration != nullptr ? BaseDeclaration->typeInfo : nullptr,
			TEXT("Builder inheritance test should attach type info to BaseState")));
		ASSERT_THAT(IsNotNull(DerivedDeclaration != nullptr ? DerivedDeclaration->typeInfo : nullptr,
			TEXT("Builder inheritance test should attach type info to DerivedState")));

		asITypeInfo* BaseType = Module->GetTypeInfoByDecl("BaseState");
		asITypeInfo* DerivedType = Module->GetTypeInfoByDecl("DerivedState");
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Builder inheritance test should expose BaseState type metadata")));
		ASSERT_THAT(IsNotNull(DerivedType, TEXT("Builder inheritance test should expose DerivedState type metadata")));
		ASSERT_THAT(AreEqual(BaseType, DerivedType != nullptr ? DerivedType->GetBaseType() : nullptr,
			TEXT("Builder inheritance test should preserve the direct base type")));
		ASSERT_THAT(IsTrue(DerivedType != nullptr && BaseType != nullptr && DerivedType->DerivesFrom(BaseType),
			TEXT("Builder inheritance test should preserve the derives-from relationship")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(BaseType != nullptr ? BaseType->GetPropertyCount() : 0),
			TEXT("Builder inheritance test should layout BaseState.BaseValue")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(DerivedType != nullptr ? DerivedType->GetPropertyCount() : 0),
			TEXT("Builder inheritance test should layout DerivedState.Delta plus inherited BaseState.BaseValue")));

		asIScriptFunction* ReadBaseFunction = FindTypeMethodByNameAndParamCount(BaseType, "ReadBase", 0);
		asIScriptFunction* ReadDeltaFunction = FindTypeMethodByNameAndParamCount(DerivedType, "ReadDelta", 0);
		asIScriptFunction* InheritedEntryFunction = FindModuleFunctionByNameAndParamCount(Module, "UseInheritedMember", 0);
		ASSERT_THAT(IsNotNull(ReadBaseFunction, TEXT("Builder inheritance test should expose BaseState.ReadBase")));
		ASSERT_THAT(IsNotNull(ReadDeltaFunction, TEXT("Builder inheritance test should expose DerivedState.ReadDelta")));
		ASSERT_THAT(IsNotNull(InheritedEntryFunction, TEXT("Builder inheritance test should compile a function using inherited members")));
		ASSERT_THAT(IsTrue(HasBytecode(ReadBaseFunction), TEXT("Builder inheritance test should compile ReadBase bytecode")));
		ASSERT_THAT(IsTrue(HasBytecode(ReadDeltaFunction), TEXT("Builder inheritance test should compile ReadDelta bytecode")));
		ASSERT_THAT(IsTrue(HasBytecode(InheritedEntryFunction), TEXT("Builder inheritance test should compile inherited-member call bytecode")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "ReadBase", "BaseState"),
			TEXT("Builder inheritance test should retain BaseState.ReadBase function description")));
		ASSERT_THAT(IsNotNull(FindFunctionDescriptionByName(*Builder, "ReadDelta", "DerivedState"),
			TEXT("Builder inheritance test should retain DerivedState.ReadDelta function description")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}
		LogScriptExecutionResult(TEXT("ClassInheritanceResolvesBaseTypesAndInheritedCalls.Entry"), "int Entry()", Result);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Builder inheritance test should execute independent compiled bytecode")));
	}
	TEST_METHOD(ScriptInterfaceDeclarationFailsWithoutLeakingState)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder interface-boundary test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderScriptInterfaceBoundary");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder interface-boundary test should create a backing module")));

		const std::string InterfaceSource = ASTEST_AS_ANSI(R"AS(
			interface IBuilderThing
			{
				void Run();
			}

			class BuilderThing : IBuilderThing
			{
				void Run()
				{
				}
			}

			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderScriptInterfaceBoundary", InterfaceSource.c_str(), TEXT("ScriptInterfaceDeclarationFailsWithoutLeakingState.AddInterfaceSection")),
			TEXT("Builder interface-boundary test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder interface-boundary test should create a builder")));

		LogBuilderState(TEXT("ScriptInterfaceDeclarationFailsWithoutLeakingState.BuildParallelParseScripts.before"), *Builder, Module, true, false);
		const int ParseResult = Builder->BuildParallelParseScripts();
		LogBuilderStageResult(TEXT("ScriptInterfaceDeclarationFailsWithoutLeakingState.BuildParallelParseScripts.after"), ParseResult, *Builder, Module);
		ReportBuilderFailureDiagnostics();
		ASSERT_THAT(IsTrue(ParseResult < 0,
			TEXT("Builder interface-boundary test should reject script-level interface declarations at parse time")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0,
			TEXT("Builder interface-boundary test should record a builder error")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Builder->parsers.GetLength()),
			TEXT("Builder interface-boundary test should retain the failing parser for diagnostics")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->interfaceDeclarations.GetLength()),
			TEXT("Builder interface-boundary test should not register interface declarations after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->classDeclarations.GetLength()),
			TEXT("Builder interface-boundary test should not register class declarations after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Builder interface-boundary test should not produce function descriptions after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->globVariableList.GetLength()),
			TEXT("Builder interface-boundary test should not produce global descriptions after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetObjectTypeCount()),
			TEXT("Builder interface-boundary test should not register object types after parse failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder interface-boundary test should not register functions after parse failure")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"),
			TEXT("Builder interface-boundary test should not expose Entry after parse failure")));

		const AngelscriptNativeTestSupport::FNativeMessageEntry* InterfaceMessage = Engine.GetMessages().Entries.Num() > 0 ? &Engine.GetMessages().Entries[0] : nullptr;
		ASSERT_THAT(IsNotNull(InterfaceMessage,
			TEXT("Builder interface-boundary test should report a diagnostic")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderScriptInterfaceBoundary")), InterfaceMessage != nullptr ? InterfaceMessage->Section : FString(),
			TEXT("Builder interface-boundary test should report the failing section")));
		ASSERT_THAT(IsTrue(InterfaceMessage != nullptr && InterfaceMessage->Row > 0,
			TEXT("Builder interface-boundary test should report a positive diagnostic row")));
		ASSERT_THAT(IsTrue(InterfaceMessage != nullptr && InterfaceMessage->Column > 0,
			TEXT("Builder interface-boundary test should report a positive diagnostic column")));
	}
	TEST_METHOD(DuplicateDeclarationsFailWithoutLeakingModuleState)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder duplicate-declaration test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderDuplicateDeclarations");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder duplicate-declaration test should create a backing module")));

		const std::string DuplicateDeclarationsSource = ASTEST_AS_ANSI(R"AS(
			class DuplicateType
			{
			}

			class DuplicateType
			{
			}

			int Entry()
			{
				return 42;
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*Module, "BuilderDuplicateDeclarations", DuplicateDeclarationsSource.c_str(), TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.AddDuplicateSection")),
			TEXT("Builder duplicate-declaration test should add the script section")));

		asCBuilder* Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("Builder duplicate-declaration test should create a builder")));

		LogBuilderState(TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.BuildParallelParseScripts.before"), *Builder, Module, true, false);
		const int ParseResult = Builder->BuildParallelParseScripts();
		LogBuilderStageResult(TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.BuildParallelParseScripts.after"), ParseResult, *Builder, Module);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ParseResult,
			TEXT("Builder duplicate-declaration test should parse syntactically valid duplicate declarations")));
		LogBuilderState(TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.BuildGenerateTypes.before"), *Builder, Module, true, false);
		const int GenerateTypesResult = Builder->BuildGenerateTypes();
		LogBuilderStageResult(TEXT("DuplicateDeclarationsFailWithoutLeakingModuleState.BuildGenerateTypes.after"), GenerateTypesResult, *Builder, Module);
		ReportBuilderFailureDiagnostics();
		ASSERT_THAT(IsTrue(GenerateTypesResult < 0,
			TEXT("Builder duplicate-declaration test should reject duplicate class declarations during type generation")));
		ASSERT_THAT(IsTrue(Builder->numErrors > 0,
			TEXT("Builder duplicate-declaration test should record a builder error")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetFunctionCount()),
			TEXT("Builder duplicate-declaration test should not register functions after type-generation failure")));
		ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int Entry()"),
			TEXT("Builder duplicate-declaration test should not expose Entry after type-generation failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->functions.GetLength()),
			TEXT("Builder duplicate-declaration test should not produce function descriptions after type-generation failure")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Builder->globVariableList.GetLength()),
			TEXT("Builder duplicate-declaration test should not produce global descriptions after type-generation failure")));

		const AngelscriptNativeTestSupport::FNativeMessageEntry* DuplicateMessage = Engine.GetMessages().Entries.Num() > 0 ? &Engine.GetMessages().Entries[0] : nullptr;
		ASSERT_THAT(IsNotNull(DuplicateMessage,
			TEXT("Builder duplicate-declaration test should report a diagnostic")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderDuplicateDeclarations")), DuplicateMessage != nullptr ? DuplicateMessage->Section : FString(),
			TEXT("Builder duplicate-declaration test should report the failing section")));
		ASSERT_THAT(IsTrue(DuplicateMessage != nullptr && DuplicateMessage->Row > 0,
			TEXT("Builder duplicate-declaration test should report a positive diagnostic row")));
		ASSERT_THAT(IsTrue(DuplicateMessage != nullptr && DuplicateMessage->Column > 0,
			TEXT("Builder duplicate-declaration test should report a positive diagnostic column")));
	}
};

#endif
