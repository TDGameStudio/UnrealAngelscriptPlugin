#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Builder template application-interface coverage.
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

template <typename TDerived, typename TAsserter>
class TBuilderTemplateApplicationTestSupport : public TTest<TDerived, TAsserter>
{
protected:
	struct FIdentifierStyleCase
	{
		const TCHAR* Id;
		const ANSICHAR* Prefix;
		bool bNumbered;
	};

	struct FTemplateArityCase
	{
		const TCHAR* Id;
		int32 Arity;
	};

	struct FWhitespaceCase
	{
		const TCHAR* Id;
		const ANSICHAR* AfterOpen;
		const ANSICHAR* BeforeComma;
		const ANSICHAR* AfterComma;
		const ANSICHAR* BeforeClose;
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

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderTemplateApplicationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.AppInterface",
	TBuilderTemplateApplicationTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TemplateDeclarationsByArityIdentifierAndWhitespace)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-TEMPLATE-DECLARATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		const FTemplateArityCase Arities[] =
		{
			{ TEXT("one"), 1 },
			{ TEXT("two"), 2 },
			{ TEXT("three"), 3 },
		};
		const FIdentifierStyleCase IdentifierStyles[] =
		{
			{ TEXT("plain"), "Type", false },
			{ TEXT("numbered"), "T", true },
		};
		const FWhitespaceCase WhitespaceStyles[] =
		{
			{ TEXT("tight"), "", "", "", "" },
			{ TEXT("spaced"), " ", " ", " ", " " },
		};

		FNativeTestEngine CaseEngine;
		CaseEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaseEngine.Destroy();
		};
		int32 ObservedCaseCount = 0;
		for (const FTemplateArityCase& Arity : Arities)
		{
			for (const FIdentifierStyleCase& IdentifierStyle : IdentifierStyles)
			{
				for (const FWhitespaceCase& Whitespace : WhitespaceStyles)
				{
					TArray<FString> ExpectedSubtypeNames;
					FString SubtypeList;
					for (int32 SubtypeIndex = 0; SubtypeIndex < Arity.Arity; ++SubtypeIndex)
					{
						const FString SubtypeName = IdentifierStyle.bNumbered
							? FString::Printf(TEXT("%hs%d"), IdentifierStyle.Prefix, SubtypeIndex + 1)
							: FString::Printf(TEXT("%hs%c"), IdentifierStyle.Prefix, TCHAR('A' + SubtypeIndex));
						ExpectedSubtypeNames.Add(SubtypeName);
						if (SubtypeIndex > 0)
						{
							SubtypeList += FString::Printf(
								TEXT("%hs,%hs"),
								Whitespace.BeforeComma,
								Whitespace.AfterComma);
						}
						SubtypeList += SubtypeName;
					}

					const FString CaseId = MakeNativeCaseId(
						"COMPILER-BUILDER-TEMPLATE-DECLARATION",
						{ Arity.Id, IdentifierStyle.Id, Whitespace.Id });
					const FString Declaration = FString::Printf(
						TEXT("map<%hs%s%hs>"),
						Whitespace.AfterOpen,
						*SubtypeList,
						Whitespace.BeforeClose);
					FString ReviewSource;
					AppendGeneratedAsLine(
						ReviewSource,
						FString::Printf(TEXT("// template declaration input: %s"), *Declaration));
					PrintGeneratedAsSource(
						*TestRunner,
						CaseId,
						TEXT("CompilerBuilderTemplateDeclarationDepth"),
						ReviewSource);

					const bool bCellExecuted = RunIsolatedBuilderModuleCell(
						*TestRunner,
						CaseEngine,
						ControlEngine,
						CaseId,
						"CompilerBuilderTemplateDeclarationDepth",
						[&](asIScriptEngine* ScriptEngine, asCModule* Module)
						{
							asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
							asCString TemplateName;
							asCArray<asCString> ParsedSubtypeNames;
							const FTCHARToUTF8 DeclarationUtf8(*Declaration);
							ASSERT_THAT(AreEqual(
								static_cast<int32>(asSUCCESS),
								Builder.ParseTemplateDecl(
									DeclarationUtf8.Get(),
									&TemplateName,
									ParsedSubtypeNames),
								*FString::Printf(TEXT("%s should parse its template declaration"), *CaseId)));
							ASSERT_THAT(AreEqual(
								FString(TEXT("map")),
								FString(UTF8_TO_TCHAR(TemplateName.AddressOf())),
								*FString::Printf(TEXT("%s should preserve the template name"), *CaseId)));
							ASSERT_THAT(AreEqual(
								Arity.Arity,
								static_cast<int32>(ParsedSubtypeNames.GetLength()),
								*FString::Printf(TEXT("%s should preserve subtype arity"), *CaseId)));
							for (int32 SubtypeIndex = 0;
								SubtypeIndex < Arity.Arity
									&& SubtypeIndex < static_cast<int32>(ParsedSubtypeNames.GetLength());
								++SubtypeIndex)
							{
								ASSERT_THAT(AreEqual(
									ExpectedSubtypeNames[SubtypeIndex],
									FString(UTF8_TO_TCHAR(ParsedSubtypeNames[SubtypeIndex].AddressOf())),
									*FString::Printf(
										TEXT("%s should preserve subtype %d"),
										*CaseId,
										SubtypeIndex)));
							}
						});
					ObservedCaseCount += bCellExecuted ? 1 : 0;
				}
			}
		}

		ASSERT_THAT(AreEqual(
			12,
			ObservedCaseCount,
			TEXT("Template arity × identifier style × whitespace should execute every declaration cell")));
	}

	TEST_METHOD(ParseTemplateDeclSplitsNameAndSubtypeIdentifiers)
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
			"Retained two-subtype template smoke; COMPILER-BUILDER-TEMPLATE-DECLARATION owns arity, identifier, and whitespace combinations.");

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder app-interface template test should create a standalone SDK engine")));

		FScopedNativeModuleName ModuleScope(Engine, "BuilderAppInterfaceTemplate");
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
};

#endif
