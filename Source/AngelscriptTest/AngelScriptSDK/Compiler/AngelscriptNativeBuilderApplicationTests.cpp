#include "Support/AngelscriptNativeBuilderTestSupport.h"

// Builder application-interface coverage.
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


namespace AngelscriptBuilderAppInterfaceTest
{
	struct FScopedScriptFunction
	{
		explicit FScopedScriptFunction(asCScriptEngine* InEngine, asCModule* InModule = nullptr, asEFuncType FuncType = asFUNC_SYSTEM)
			: Function(asNEW(asCScriptFunction)(InEngine, InModule, FuncType))
		{
		}

		~FScopedScriptFunction()
		{
			if (Function != nullptr)
			{
				Function->ReleaseInternal();
				Function = nullptr;
			}
		}

		FScopedScriptFunction(const FScopedScriptFunction&) = delete;
		FScopedScriptFunction& operator=(const FScopedScriptFunction&) = delete;

		asCScriptFunction* Get() const
		{
			return Function;
		}

	private:
		asCScriptFunction* Function = nullptr;
	};
}

TEST_CLASS_WITH_FLAGS(FBuilderApplicationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.AppInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(ParseDataTypeResolvesPrimitiveAndScriptClass)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder app-interface data type test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderAppInterfaceDataType");
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

	TEST_METHOD(ParseFunctionDeclarationPreservesParamsDefaultsAndTraits)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder app-interface function test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderAppInterfaceFunction");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder app-interface function test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		AngelscriptBuilderAppInterfaceTest::FScopedScriptFunction Function(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		asSNameSpace* BuilderNamespace = static_cast<asCScriptEngine*>(ScriptEngine)->AddNameSpace("BuilderNS");
		ASSERT_THAT(IsNotNull(BuilderNamespace, TEXT("Builder app-interface function test should create implicit namespace")));

		const int ParseResult = Builder.ParseFunctionDeclaration(
			nullptr,
			"int Blend(const int A, int B = 7) no_discard",
			Function.Get(),
			false,
			nullptr,
			nullptr,
			BuilderNamespace);
		ASSERT_THAT(AreEqual(0, ParseResult, TEXT("Builder app-interface function test should parse namespaced function declaration")));
		ASSERT_THAT(AreEqual(FString(TEXT("Blend")), FString(UTF8_TO_TCHAR(Function.Get()->GetName())),
			TEXT("Builder app-interface function test should preserve function name")));
		ASSERT_THAT(AreEqual(FString(TEXT("BuilderNS")), FString(UTF8_TO_TCHAR(Function.Get()->GetNamespace())),
			TEXT("Builder app-interface function test should preserve namespace")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Function.Get()->GetParamCount()),
			TEXT("Builder app-interface function test should preserve parameter count")));
		ASSERT_THAT(IsTrue(Function.Get()->traits.GetTrait(asTRAIT_NODISCARD),
			TEXT("Builder app-interface function test should parse no_discard trait")));

		int FirstTypeId = asINVALID_TYPE;
		asDWORD FirstFlags = 0;
		const char* FirstName = nullptr;
		const char* FirstDefaultArg = nullptr;
		ASSERT_THAT(AreEqual(0, Function.Get()->GetParam(0, &FirstTypeId, &FirstFlags, &FirstName, &FirstDefaultArg),
			TEXT("Builder app-interface function test should expose first parameter")));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, FirstTypeId, TEXT("Builder app-interface function test should expose first int parameter type")));
		ASSERT_THAT(AreEqual(FString(TEXT("A")), FString(UTF8_TO_TCHAR(FirstName)),
			TEXT("Builder app-interface function test should preserve first parameter name")));
		ASSERT_THAT(IsNull(FirstDefaultArg, TEXT("Builder app-interface function test should not synthesize default arg for first parameter")));

		int SecondTypeId = asINVALID_TYPE;
		asDWORD SecondFlags = 0;
		const char* SecondName = nullptr;
		const char* SecondDefaultArg = nullptr;
		ASSERT_THAT(AreEqual(0, Function.Get()->GetParam(1, &SecondTypeId, &SecondFlags, &SecondName, &SecondDefaultArg),
			TEXT("Builder app-interface function test should expose second parameter")));
		ASSERT_THAT(AreEqual(asTYPEID_INT32, SecondTypeId, TEXT("Builder app-interface function test should expose second int parameter type")));
		ASSERT_THAT(AreEqual(FString(TEXT("B")), FString(UTF8_TO_TCHAR(SecondName)),
			TEXT("Builder app-interface function test should preserve second parameter name")));
		ASSERT_THAT(AreEqual(FString(TEXT("7")), FString(UTF8_TO_TCHAR(SecondDefaultArg)),
			TEXT("Builder app-interface function test should preserve cleaned default argument")));
	}

	TEST_METHOD(ParseVariableDeclarationExtractsNamespaceNameAndType)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder app-interface variable test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderAppInterfaceVariable");
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

	TEST_METHOD(ParseTemplateDeclSplitsNameAndSubtypeIdentifiers)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder app-interface template test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderAppInterfaceTemplate");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder app-interface template test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		asCString TemplateName;
		asCArray<asCString> SubtypeNames;

		const int ParseResult = Builder.ParseTemplateDecl("map<KeyType, ValueType>", &TemplateName, SubtypeNames);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ParseResult, TEXT("Builder app-interface template test should parse template declaration")));
		ASSERT_THAT(AreEqual(FString(TEXT("map")), FString(UTF8_TO_TCHAR(TemplateName.AddressOf())),
			TEXT("Builder app-interface template test should extract template name")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(SubtypeNames.GetLength()),
			TEXT("Builder app-interface template test should extract subtype count")));
		ASSERT_THAT(AreEqual(FString(TEXT("KeyType")), FString(UTF8_TO_TCHAR(SubtypeNames[0].AddressOf())),
			TEXT("Builder app-interface template test should preserve first subtype text")));
		ASSERT_THAT(AreEqual(FString(TEXT("ValueType")), FString(UTF8_TO_TCHAR(SubtypeNames[1].AddressOf())),
			TEXT("Builder app-interface template test should preserve second subtype text")));
	}

	TEST_METHOD(VerifyPropertyAcceptsValidDeclarationAndRejectsNameConflict)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder app-interface property test should create a standalone SDK engine")));

		const int TypeResult = ScriptEngine->RegisterObjectType("BuilderNativeCarrier", 4, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE);
		ASSERT_THAT(IsTrue(TypeResult >= 0 || TypeResult == asALREADY_REGISTERED,
			TEXT("Builder app-interface property test should register native carrier type")));
		const int PropertyResult = ScriptEngine->RegisterObjectProperty("BuilderNativeCarrier", "int Existing", 0);
		ASSERT_THAT(IsTrue(PropertyResult >= 0 || PropertyResult == asALREADY_REGISTERED,
			TEXT("Builder app-interface property test should register existing property")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "BuilderAppInterfaceProperty");
		asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module, TEXT("Builder app-interface property test should create a module")));

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		asCDataType ObjectType;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS),
			Builder.ParseDataType("BuilderNativeCarrier", &ObjectType, Module->defaultNamespace, false, true),
			TEXT("Builder app-interface property test should parse native carrier type")));

		asCString PropertyName;
		asCDataType PropertyType;
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS),
			Builder.VerifyProperty(&ObjectType, "const int Added", PropertyName, PropertyType, nullptr),
			TEXT("Builder app-interface property test should accept a new property declaration")));
		ASSERT_THAT(AreEqual(FString(TEXT("Added")), FString(UTF8_TO_TCHAR(PropertyName.AddressOf())),
			TEXT("Builder app-interface property test should extract property name")));
		ASSERT_THAT(IsTrue(PropertyType.IsIntegerType(), TEXT("Builder app-interface property test should resolve property type")));
		ASSERT_THAT(IsTrue(PropertyType.IsReadOnly(), TEXT("Builder app-interface property test should preserve const property type")));

		asCString ConflictingName;
		asCDataType ConflictingType;
		const int ConflictResult = Builder.VerifyProperty(&ObjectType, "int Existing", ConflictingName, ConflictingType, nullptr);
		ASSERT_THAT(AreEqual(static_cast<int32>(asNAME_TAKEN), ConflictResult,
			TEXT("Builder app-interface property test should reject a property name conflict")));
		ASSERT_THAT(IsTrue(AssertBuilderDiagnostic(*TestRunner, Engine.GetMessages(),
			AngelscriptBuilderTestSupport::FExpectedBuilderDiagnostic::Error(TEXT("Property"), INDEX_NONE, TEXT("Name conflict. 'Existing' is an object property.")),
			TEXT("VerifyProperty.NameConflict")),
			TEXT("Builder app-interface property test should report the property name conflict")));
	}
};

#endif
