#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Builder property application-interface coverage.
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS
template <typename TDerived, typename TAsserter>
class TBuilderPropertyApplicationTestSupport : public TTest<TDerived, TAsserter>
{
protected:
	struct FScalarTypeCase
	{
		const TCHAR* Id;
		const ANSICHAR* Declaration;
		int32 TypeId;
	};

	struct FQualifierCase
	{
		const TCHAR* Id;
		const ANSICHAR* Prefix;
		bool bReadOnly;
	};

	struct FConflictCase
	{
		const TCHAR* Id;
		const ANSICHAR* PropertyName;
		bool bExpectedConflict;
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

TEST_CLASS_WITH_BASE_AND_FLAGS(FBuilderPropertyApplicationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.AppInterface",
	TBuilderPropertyApplicationTestSupport,
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(PropertiesByScalarQualifierAndConflictState)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-BUILDER-PROPERTY-VERIFICATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
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
		const FConflictCase ConflictStates[] =
		{
			{ TEXT("new"), "Added", false },
			{ TEXT("existing"), "Existing", true },
		};

		asIScriptEngine* const ControlScriptEngine = ControlEngine.Get();
		ASSERT_THAT(IsNotNull(
			ControlScriptEngine,
			TEXT("Property verification product should create an independent control engine")));
		if (ControlScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ControlScriptEngine->RegisterObjectType(
				"BuilderNativeCarrier",
				16,
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE) >= 0,
			TEXT("Property verification control should register its independent carrier")));
		ASSERT_THAT(IsTrue(
			ControlScriptEngine->RegisterObjectProperty(
				"BuilderNativeCarrier",
				"int Existing",
				0) >= 0,
			TEXT("Property verification control should register its independent property")));
		asITypeInfo* const ControlType =
			ControlScriptEngine->GetTypeInfoByName("BuilderNativeCarrier");
		ASSERT_THAT(IsNotNull(
			ControlType,
			TEXT("Property verification control should expose its registered carrier")));
		if (ControlType == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1),
			ControlType->GetPropertyCount(),
			TEXT("Property verification control should begin with one independent property")));

		for (const FScalarTypeCase& ScalarType : ScalarTypes)
		{
			for (const FQualifierCase& Qualifier : Qualifiers)
			{
				for (const FConflictCase& ConflictState : ConflictStates)
				{
					const FString CaseId = MakeNativeCaseId(
						"COMPILER-BUILDER-PROPERTY-VERIFICATION",
						{ ScalarType.Id, Qualifier.Id, ConflictState.Id });
					const FString PropertyDeclaration = FString::Printf(
						TEXT("%hs%hs %hs"),
						Qualifier.Prefix,
						ScalarType.Declaration,
						ConflictState.PropertyName);
					FString ReviewSource;
					AppendGeneratedAsLine(ReviewSource, TEXT("class BuilderNativeCarrier"));
					AppendGeneratedAsLine(ReviewSource, TEXT("{"));
					AppendGeneratedAsLine(
						ReviewSource,
						FString::Printf(TEXT("\t%s;"), *PropertyDeclaration));
					AppendGeneratedAsLine(ReviewSource, TEXT("}"));
					PrintGeneratedAsSource(
						*TestRunner,
						CaseId,
						TEXT("CompilerBuilderPropertyVerificationDepth"),
						ReviewSource);

					FNativeTestEngine CaseEngine;
					CaseEngine.Create(*TestRunner);
					ON_SCOPE_EXIT
					{
						CaseEngine.Destroy();
					};
					asIScriptEngine* const CaseScriptEngine = CaseEngine.Get();
					ASSERT_THAT(IsNotNull(
						CaseScriptEngine,
						*FString::Printf(TEXT("%s should create a case-owned raw engine"), *CaseId)));
					if (CaseScriptEngine == nullptr)
					{
						continue;
					}

					ASSERT_THAT(IsTrue(
						CaseScriptEngine->RegisterObjectType(
							"BuilderNativeCarrier",
							16,
							asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE) >= 0,
						*FString::Printf(TEXT("%s should register the native carrier"), *CaseId)));
					ASSERT_THAT(IsTrue(
						CaseScriptEngine->RegisterObjectProperty(
							"BuilderNativeCarrier",
							"int Existing",
							0) >= 0,
						*FString::Printf(TEXT("%s should register the existing property"), *CaseId)));

					ASSERT_THAT(IsTrue(
						ControlScriptEngine->GetTypeInfoByName("BuilderNativeCarrier")
							== ControlType,
						*FString::Printf(TEXT("%s should begin with the independent control type"), *CaseId)));
					ASSERT_THAT(AreEqual(
						static_cast<asUINT>(1),
						ControlType->GetPropertyCount(),
						*FString::Printf(TEXT("%s should begin with one independent control property"), *CaseId)));

					const bool bCellExecuted = RunIsolatedBuilderModuleCell(
						*TestRunner,
						CaseEngine,
						ControlEngine,
						CaseId,
						"CompilerBuilderPropertyVerificationDepth",
						[&](asIScriptEngine* ScriptEngine, asCModule* Module)
						{
							asCBuilder Builder(
								static_cast<asCScriptEngine*>(ScriptEngine),
								Module);
							asCDataType ObjectType;
							ASSERT_THAT(AreEqual(
								static_cast<int32>(asSUCCESS),
								Builder.ParseDataType(
									"BuilderNativeCarrier",
									&ObjectType,
									Module->defaultNamespace,
									false,
									true),
								*FString::Printf(TEXT("%s should resolve the carrier type"), *CaseId)));

							asCString ParsedName;
							asCDataType ParsedType;
							const FTCHARToUTF8 PropertyDeclarationUtf8(*PropertyDeclaration);
							const int32 VerifyResult = Builder.VerifyProperty(
								&ObjectType,
								PropertyDeclarationUtf8.Get(),
								ParsedName,
								ParsedType,
								nullptr);
							ASSERT_THAT(AreEqual(
								ConflictState.bExpectedConflict
									? static_cast<int32>(asNAME_TAKEN)
									: static_cast<int32>(asSUCCESS),
								VerifyResult,
								*FString::Printf(TEXT("%s should preserve its conflict disposition"), *CaseId)));
							ASSERT_THAT(AreEqual(
								FString(UTF8_TO_TCHAR(ConflictState.PropertyName)),
								FString(UTF8_TO_TCHAR(ParsedName.AddressOf())),
								*FString::Printf(TEXT("%s should preserve the property name"), *CaseId)));
							if (!ConflictState.bExpectedConflict)
							{
								ASSERT_THAT(AreEqual(
									ScalarType.TypeId,
									static_cast<asCScriptEngine*>(ScriptEngine)->GetTypeIdFromDataType(ParsedType),
									*FString::Printf(TEXT("%s should preserve the property type"), *CaseId)));
								ASSERT_THAT(AreEqual(
									Qualifier.bReadOnly,
									ParsedType.IsReadOnly(),
									*FString::Printf(TEXT("%s should preserve the property qualifier"), *CaseId)));
							}
							else
							{
								ASSERT_THAT(IsTrue(
									ContainsError(CaseEngine.GetMessages(), TEXT("Name conflict")),
									*FString::Printf(TEXT("%s should retain the name-conflict diagnostic"), *CaseId)));
							}
						});
					ASSERT_THAT(IsTrue(
						bCellExecuted,
						*FString::Printf(TEXT("%s should execute against independent case and control modules"), *CaseId)));

					ASSERT_THAT(IsTrue(
						ControlScriptEngine->GetTypeInfoByName("BuilderNativeCarrier")
							== ControlType,
						*FString::Printf(TEXT("%s should preserve independent control type identity"), *CaseId)));
					ASSERT_THAT(AreEqual(
						static_cast<asUINT>(1),
						ControlType->GetPropertyCount(),
						*FString::Printf(TEXT("%s should not mutate the independent control registry"), *CaseId)));
				}
			}
		}
	}

	TEST_METHOD(VerifyPropertyAcceptsValidDeclarationAndRejectsNameConflict)
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
			"Retained representative property conflict smoke; COMPILER-BUILDER-PROPERTY-VERIFICATION owns scalar type, qualifier, and conflict-state combinations.");

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
