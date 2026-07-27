#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Builder application-interface coverage.
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

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

template <typename TDerived, typename TAsserter>
class TBuilderApplicationTestSupport : public TTest<TDerived, TAsserter>
{
protected:
	struct FScalarTypeCase
	{
		const TCHAR* Id;
		const ANSICHAR* Declaration;
		int32 TypeId;
	};

	struct FParameterShapeCase
	{
		const TCHAR* Id;
		const ANSICHAR* Declaration;
		int32 ParameterCount;
		int32 DefaultParameterIndex;
		const ANSICHAR* DefaultArgument;
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
		using namespace AngelscriptNativeTestSupport;

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

		asCModule* const ControlModule = CreateBuilderModule(
			ControlScriptEngine,
			ModuleName);
		asCModule* const Module = CreateBuilderModule(
			ScriptEngine,
			ModuleName);
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
				ControlScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS)
					== ControlModule,
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

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderApplicationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.AppInterface",
	TBuilderApplicationTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FunctionDeclarationsByReturnParameterTraitAndNamespace)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-FUNCTION-DECLARATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		const FScalarTypeCase ReturnTypes[] =
		{
			{ TEXT("int"), "int", asTYPEID_INT32 },
			{ TEXT("bool"), "bool", asTYPEID_BOOL },
			{ TEXT("float64"), "float64", asTYPEID_FLOAT64 },
		};
		const FParameterShapeCase ParameterShapes[] =
		{
			{ TEXT("none"), "()", 0, INDEX_NONE, nullptr },
			{ TEXT("single"), "(int A)", 1, INDEX_NONE, nullptr },
			{ TEXT("default"), "(const int A, int B = 7)", 2, 1, "7" },
			{ TEXT("directions"), "(int& in A, int& out B, int& inout C)", 3, INDEX_NONE, nullptr },
		};
		const FQualifierCase Traits[] =
		{
			{ TEXT("plain"), "", false },
			{ TEXT("no_discard"), " no_discard", true },
		};
		const FNamespaceCase Namespaces[] =
		{
			{ TEXT("global"), "" },
			{ TEXT("named"), "BuilderFunctionDepth" },
		};

		FNativeTestEngine CaseEngine;
		CaseEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaseEngine.Destroy();
		};
		int32 ObservedCaseCount = 0;
		for (const FScalarTypeCase& ReturnType : ReturnTypes)
		{
			for (const FParameterShapeCase& Parameters : ParameterShapes)
			{
				for (const FQualifierCase& Trait : Traits)
				{
					for (const FNamespaceCase& NamespaceCase : Namespaces)
					{
						const FString CaseId = MakeNativeCaseId(
							"COMPILER-BUILDER-FUNCTION-DECLARATION",
							{ ReturnType.Id, Parameters.Id, Trait.Id, NamespaceCase.Id });
						const FString Declaration = FString::Printf(
							TEXT("%hs Probe%hs%hs"),
							ReturnType.Declaration,
							Parameters.Declaration,
							Trait.Prefix);
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
								FString::Printf(TEXT("\t%s;"), *Declaration));
							AppendGeneratedAsLine(ReviewSource, TEXT("}"));
						}
						else
						{
							AppendGeneratedAsLine(
								ReviewSource,
								FString::Printf(TEXT("%s;"), *Declaration));
						}
						PrintGeneratedAsSource(
							*TestRunner,
							CaseId,
							TEXT("CompilerBuilderFunctionDeclarationDepth"),
							ReviewSource);

						bool bCellObserved = false;
						const bool bCellExecuted = RunIsolatedBuilderModuleCell(
							*TestRunner,
							CaseEngine,
							ControlEngine,
							CaseId,
							"CompilerBuilderFunctionDeclarationDepth",
							[&](asIScriptEngine* ScriptEngine, asCModule* Module)
							{
								asCBuilder Builder(
									static_cast<asCScriptEngine*>(ScriptEngine),
									Module);
								asSNameSpace* const Namespace = NamespaceCase.Name[0] != '\0'
									? static_cast<asCScriptEngine*>(ScriptEngine)->AddNameSpace(NamespaceCase.Name)
									: Module->defaultNamespace;
								ASSERT_THAT(IsNotNull(
									Namespace,
									*FString::Printf(TEXT("%s should resolve its declaration namespace"), *CaseId)));

								AngelscriptBuilderAppInterfaceTest::FScopedScriptFunction Function(
									static_cast<asCScriptEngine*>(ScriptEngine),
									Module);
								const FTCHARToUTF8 DeclarationUtf8(*Declaration);
								const int32 ParseResult = Builder.ParseFunctionDeclaration(
									nullptr,
									DeclarationUtf8.Get(),
									Function.Get(),
									false,
									nullptr,
									nullptr,
									Namespace);
								ASSERT_THAT(AreEqual(
									0,
									ParseResult,
									*FString::Printf(TEXT("%s should parse the complete function declaration"), *CaseId)));
								if (ParseResult < 0)
								{
									const FString Diagnostics = CaseEngine.GetMessagesText();
									if (!Diagnostics.IsEmpty())
									{
										TestRunner->AddInfo(FString::Printf(
											TEXT("%s diagnostics:\n%s"),
											*CaseId,
											*Diagnostics));
									}
									return;
								}

								ASSERT_THAT(AreEqual(
									ReturnType.TypeId,
									Function.Get()->GetReturnTypeId(),
									*FString::Printf(TEXT("%s should preserve the return type"), *CaseId)));
								ASSERT_THAT(AreEqual(
									Parameters.ParameterCount,
									static_cast<int32>(Function.Get()->GetParamCount()),
									*FString::Printf(TEXT("%s should preserve the parameter count"), *CaseId)));
								ASSERT_THAT(AreEqual(
									Trait.bReadOnly,
									Function.Get()->traits.GetTrait(asTRAIT_NODISCARD),
									*FString::Printf(TEXT("%s should preserve the no_discard trait state"), *CaseId)));
								ASSERT_THAT(AreEqual(
									FString(UTF8_TO_TCHAR(NamespaceCase.Name)),
									FString(UTF8_TO_TCHAR(Function.Get()->GetNamespace())),
									*FString::Printf(TEXT("%s should preserve the namespace"), *CaseId)));

								for (int32 ParameterIndex = 0;
									ParameterIndex < Parameters.ParameterCount;
									++ParameterIndex)
								{
									int32 TypeId = asINVALID_TYPE;
									asDWORD Flags = 0;
									const char* Name = nullptr;
									const char* DefaultArgument = nullptr;
									ASSERT_THAT(AreEqual(
										0,
										Function.Get()->GetParam(
											static_cast<asUINT>(ParameterIndex),
											&TypeId,
											&Flags,
											&Name,
											&DefaultArgument),
										*FString::Printf(
											TEXT("%s should expose parameter %d"),
											*CaseId,
											ParameterIndex)));
									ASSERT_THAT(AreEqual(
										asTYPEID_INT32,
										TypeId,
										*FString::Printf(
											TEXT("%s should retain int parameter %d"),
											*CaseId,
											ParameterIndex)));
									if (ParameterIndex == Parameters.DefaultParameterIndex)
									{
										ASSERT_THAT(AreEqual(
											FString(UTF8_TO_TCHAR(Parameters.DefaultArgument)),
											FString(UTF8_TO_TCHAR(DefaultArgument)),
											*FString::Printf(
												TEXT("%s should preserve the default argument"),
												*CaseId)));
									}
									else
									{
										ASSERT_THAT(IsNull(
											DefaultArgument,
											*FString::Printf(
												TEXT("%s should not synthesize a default argument for parameter %d"),
												*CaseId,
												ParameterIndex)));
									}
								}
								bCellObserved = true;
							});

						ObservedCaseCount += bCellExecuted && bCellObserved ? 1 : 0;
					}
				}
			}
		}

		ASSERT_THAT(AreEqual(
			48,
			ObservedCaseCount,
			TEXT("Return type × parameter shape × trait × namespace should execute every function-declaration cell")));
	}

	TEST_METHOD(ParseFunctionDeclarationPreservesParamsDefaultsAndTraits)
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
			"Retained representative function-declaration smoke; COMPILER-BUILDER-FUNCTION-DECLARATION owns return, parameter, trait, and namespace combinations.");

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
};

#endif
