#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Builder scalar application-interface coverage.
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

template <typename TDerived, typename TAsserter>
class TBuilderScalarApplicationTestSupport : public TTest<TDerived, TAsserter>
{
protected:
	struct FScalarTypeCase
	{
		const TCHAR* Id;
		const ANSICHAR* Declaration;
		int32 TypeId;
	};

	struct FNamespaceCase
	{
		const TCHAR* Id;
		const ANSICHAR* Name;
	};

	struct FQualifierCase
	{
		const TCHAR* Id;
		const ANSICHAR* Prefix;
		bool bReadOnly;
	};

	template <typename FCellBody>
	static bool RunIsolatedBuilderModuleCell(
		FAutomationTestBase& Test,
		AngelscriptNativeTestSupport::FNativeTestEngine& CaseEngine,
		AngelscriptNativeTestSupport::FNativeTestEngine& ControlEngine,
		const FString& CaseId,
		const ANSICHAR* ModuleName,
		FCellBody CellBody)
	{
		using namespace AngelscriptBuilderTestSupport;

		FNoDiscardAsserter Assertions(Test);
		asIScriptEngine* const ScriptEngine = CaseEngine.Get();
		asIScriptEngine* const ControlScriptEngine = ControlEngine.Get();
		if (!Assertions.IsNotNull(
				ScriptEngine,
				*FString::Printf(TEXT("%s should retain its raw SDK engine"), *CaseId))
			|| !Assertions.IsNotNull(
				ControlScriptEngine,
				*FString::Printf(TEXT("%s should retain its independent control engine"), *CaseId)))
		{
			return false;
		}

		(void)Assertions.IsNull(
			ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			*FString::Printf(TEXT("%s should begin without a stale case module"), *CaseId));
		(void)Assertions.IsNull(
			ControlScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			*FString::Printf(TEXT("%s should begin without a stale control module"), *CaseId));

		asCModule* const ControlModule = CreateBuilderModule(ControlScriptEngine, ModuleName);
		asCModule* const Module = CreateBuilderModule(ScriptEngine, ModuleName);
		const bool bControlModuleValid = Assertions.IsNotNull(
			ControlModule,
			*FString::Printf(TEXT("%s should create its independent control module"), *CaseId));
		const bool bModuleValid = Assertions.IsNotNull(
			Module,
			*FString::Printf(TEXT("%s should create its case module"), *CaseId));
		(void)Assertions.IsTrue(
			Module != ControlModule,
			*FString::Printf(TEXT("%s should keep case and control module identities independent"), *CaseId));

		if (bControlModuleValid && bModuleValid)
		{
			CellBody(ScriptEngine, Module);
			(void)Assertions.IsTrue(
				ControlScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS) == ControlModule,
				*FString::Printf(TEXT("%s should not replace its independent control module"), *CaseId));
		}

		(void)Assertions.IsTrue(
			ScriptEngine->DiscardModule(ModuleName) == asSUCCESS,
			*FString::Printf(TEXT("%s should explicitly discard its case module"), *CaseId));
		(void)Assertions.IsNull(
			ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			*FString::Printf(TEXT("%s should leave no case module after discard"), *CaseId));
		(void)Assertions.IsTrue(
			ControlScriptEngine->DiscardModule(ModuleName) == asSUCCESS,
			*FString::Printf(TEXT("%s should explicitly discard its control module"), *CaseId));
		(void)Assertions.IsNull(
			ControlScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			*FString::Printf(TEXT("%s should leave no control module after discard"), *CaseId));
		return bControlModuleValid && bModuleValid;
	}
};

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderScalarApplicationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.AppInterface",
	TBuilderScalarApplicationTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ScalarTypesByQualifierAndDeclarationApi)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-SCALAR-DECLARATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		const FScalarTypeCase ScalarTypes[] =
		{
			{ TEXT("int"), "int", asTYPEID_INT32 },
			{ TEXT("bool"), "bool", asTYPEID_BOOL },
			{ TEXT("float64"), "float64", asTYPEID_FLOAT64 },
		};
		const FQualifierCase Qualifiers[] =
		{
			{ TEXT("mutable"), "", false },
			{ TEXT("const"), "const ", true },
		};
		const FNamespaceCase Namespaces[] =
		{
			{ TEXT("global"), "" },
			{ TEXT("named"), "BuilderScalarDepth" },
		};

		FNativeTestEngine CaseEngine;
		CaseEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaseEngine.Destroy();
		};
		int32 ObservedCaseCount = 0;
		for (const FScalarTypeCase& ScalarType : ScalarTypes)
		{
			for (const FQualifierCase& Qualifier : Qualifiers)
			{
				for (const FNamespaceCase& NamespaceCase : Namespaces)
				{
					const FString CaseId = MakeNativeCaseId(
						"COMPILER-BUILDER-SCALAR-DECLARATION",
						{ ScalarType.Id, Qualifier.Id, NamespaceCase.Id });
					const FString TypeDeclaration = FString::Printf(
						TEXT("%hs%hs"),
						Qualifier.Prefix,
						ScalarType.Declaration);
					const FString VariableDeclaration = FString::Printf(
						TEXT("%s Value"),
						*TypeDeclaration);
					FString ReviewSource;
					AppendGeneratedAsLine(
						ReviewSource,
						NamespaceCase.Name[0] != '\0'
							? FString::Printf(TEXT("namespace %hs"), NamespaceCase.Name)
							: TEXT("// global namespace"));
					if (NamespaceCase.Name[0] != '\0')
					{
						AppendGeneratedAsLine(ReviewSource, TEXT("{"));
						AppendGeneratedAsLine(
							ReviewSource,
							FString::Printf(TEXT("\t%s;"), *VariableDeclaration));
						AppendGeneratedAsLine(ReviewSource, TEXT("}"));
					}
					else
					{
						AppendGeneratedAsLine(
							ReviewSource,
							FString::Printf(TEXT("%s;"), *VariableDeclaration));
					}
					PrintGeneratedAsSource(
						*TestRunner,
						CaseId,
						TEXT("CompilerBuilderScalarDeclarationDepth"),
						ReviewSource);

					const bool bCellExecuted = RunIsolatedBuilderModuleCell(
						*TestRunner,
						CaseEngine,
						ControlEngine,
						CaseId,
						"CompilerBuilderScalarDeclarationDepth",
						[&](asIScriptEngine* ScriptEngine, asCModule* Module)
						{
							asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
							asSNameSpace* const Namespace = NamespaceCase.Name[0] != '\0'
								? static_cast<asCScriptEngine*>(ScriptEngine)->AddNameSpace(NamespaceCase.Name)
								: Module->defaultNamespace;
							ASSERT_THAT(IsNotNull(
								Namespace,
								*FString::Printf(TEXT("%s should resolve its declaration namespace"), *CaseId)));

							asCDataType ParsedType;
							const FTCHARToUTF8 TypeDeclarationUtf8(*TypeDeclaration);
							ASSERT_THAT(AreEqual(
								static_cast<int32>(asSUCCESS),
								Builder.ParseDataType(TypeDeclarationUtf8.Get(), &ParsedType, Namespace),
								*FString::Printf(TEXT("%s should parse its data type"), *CaseId)));
							ASSERT_THAT(AreEqual(
								ScalarType.TypeId,
								static_cast<asCScriptEngine*>(ScriptEngine)->GetTypeIdFromDataType(ParsedType),
								*FString::Printf(TEXT("%s should preserve the scalar type id"), *CaseId)));
							ASSERT_THAT(AreEqual(
								Qualifier.bReadOnly,
								ParsedType.IsReadOnly(),
								*FString::Printf(TEXT("%s should preserve the data-type qualifier"), *CaseId)));

							asCString VariableName;
							asSNameSpace* ParsedNamespace = nullptr;
							asCDataType VariableType;
							const FTCHARToUTF8 VariableDeclarationUtf8(*VariableDeclaration);
							ASSERT_THAT(AreEqual(
								static_cast<int32>(asSUCCESS),
								Builder.ParseVariableDeclaration(
									VariableDeclarationUtf8.Get(),
									Namespace,
									VariableName,
									ParsedNamespace,
									VariableType),
								*FString::Printf(TEXT("%s should parse its variable declaration"), *CaseId)));
							ASSERT_THAT(AreEqual(
								FString(TEXT("Value")),
								FString(UTF8_TO_TCHAR(VariableName.AddressOf())),
								*FString::Printf(TEXT("%s should preserve the variable name"), *CaseId)));
							ASSERT_THAT(AreEqual(
								ScalarType.TypeId,
								static_cast<asCScriptEngine*>(ScriptEngine)->GetTypeIdFromDataType(VariableType),
								*FString::Printf(TEXT("%s should preserve the variable type id"), *CaseId)));
							ASSERT_THAT(AreEqual(
								Qualifier.bReadOnly,
								VariableType.IsReadOnly(),
								*FString::Printf(TEXT("%s should preserve the variable qualifier"), *CaseId)));
							ASSERT_THAT(IsTrue(
								ParsedNamespace == Namespace,
								*FString::Printf(TEXT("%s should preserve namespace identity"), *CaseId)));
						});
					ObservedCaseCount += bCellExecuted ? 1 : 0;
				}
			}
		}

		ASSERT_THAT(AreEqual(
			12,
			ObservedCaseCount,
			TEXT("Scalar type × qualifier × namespace should execute every declaration-API cell")));
	}

	TEST_METHOD(ParseDataTypeResolvesPrimitiveAndScriptClass)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained primitive/script-class ParseDataType smoke; COMPILER-BUILDER-SCALAR-DECLARATION owns scalar qualifier and namespace combinations while COMPILER-BUILDER-SHAPE-FAILURE owns script-class publication.");

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder app-interface data type test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderAppInterfaceDataType");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder app-interface data type test should create a module")));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			namespace BuilderApp
			{
				class Carrier
				{
					int Value;
				}
			}
			)AS");
		ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "BuilderAppInterfaceDataType.as", Source.c_str(), TEXT("AppInterfaceDataType.AddSection")),
			TEXT("Builder app-interface data type test should add the script section")));
		ASSERT_THAT(IsNotNull(Module->builder, TEXT("Builder app-interface data type test should create a builder")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Module->builder, TEXT("AppInterfaceDataType.BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module),
			TEXT("Builder app-interface data type test should parse the class section")));
		ASSERT_THAT(IsTrue(RunBuilderStage(*TestRunner, *Module->builder, TEXT("AppInterfaceDataType.BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module),
			TEXT("Builder app-interface data type test should generate class type metadata")));

		asCBuilder StandaloneBuilder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		asCDataType IntType;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS),
			StandaloneBuilder.ParseDataType("const int", &IntType, Module->defaultNamespace),
			TEXT("Builder app-interface data type test should parse const int")));
		ASSERT_THAT(IsTrue(IntType.IsIntegerType(), TEXT("Builder app-interface data type test should resolve int as integer")));
		ASSERT_THAT(IsTrue(IntType.IsReadOnly(), TEXT("Builder app-interface data type test should preserve const on primitive type")));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, static_cast<asCScriptEngine*>(ScriptEngine)->GetTypeIdFromDataType(IntType),
			TEXT("Builder app-interface data type test should map int to the engine int32 type id")));

		asCDataType ClassType;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS),
			StandaloneBuilder.ParseDataType("BuilderApp::Carrier", &ClassType, Module->defaultNamespace),
			TEXT("Builder app-interface data type test should parse namespaced script class")));
		ASSERT_THAT(IsTrue(ClassType.IsObject(), TEXT("Builder app-interface data type test should resolve script class as object type")));
		ASSERT_THAT(IsNotNull(ClassType.GetTypeInfo(), TEXT("Builder app-interface data type test should attach script class type info")));
		ASSERT_THAT(AreEqual(FString(TEXT("Carrier")), FString(UTF8_TO_TCHAR(ClassType.GetTypeInfo()->GetName())),
			TEXT("Builder app-interface data type test should resolve the expected class name")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderApp")), FString(UTF8_TO_TCHAR(ClassType.GetTypeInfo()->GetNamespace())),
			TEXT("Builder app-interface data type test should resolve the expected namespace")));
	}

	TEST_METHOD(ParseVariableDeclarationExtractsNamespaceNameAndType)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"Retained representative variable-declaration smoke; COMPILER-BUILDER-SCALAR-DECLARATION owns scalar type, qualifier, and namespace combinations.");

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder app-interface variable test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderAppInterfaceVariable");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder app-interface variable test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		asSNameSpace* BuilderNamespace = static_cast<asCScriptEngine*>(ScriptEngine)->AddNameSpace("BuilderVars");
		ASSERT_THAT(IsNotNull(BuilderNamespace, TEXT("Builder app-interface variable test should create implicit namespace")));
		asCString Name;
		asSNameSpace* Namespace = nullptr;
		asCDataType Type;

		const int ParseResult = Builder.ParseVariableDeclaration("const int Value", BuilderNamespace, Name, Namespace, Type);
		ASSERT_THAT(AreEqual(0, ParseResult, TEXT("Builder app-interface variable test should parse namespaced variable declaration")));
		ASSERT_THAT(AreEqual(FString(TEXT("Value")), FString(UTF8_TO_TCHAR(Name.AddressOf())),
			TEXT("Builder app-interface variable test should extract variable name")));
		ASSERT_THAT(IsNotNull(Namespace, TEXT("Builder app-interface variable test should resolve variable namespace")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderVars")), FString(UTF8_TO_TCHAR(Namespace->name.AddressOf())),
			TEXT("Builder app-interface variable test should extract namespace")));
		ASSERT_THAT(IsTrue(Type.IsIntegerType(), TEXT("Builder app-interface variable test should resolve int variable type")));
		ASSERT_THAT(IsTrue(Type.IsReadOnly(), TEXT("Builder app-interface variable test should preserve const variable type")));
	}
};

#endif
