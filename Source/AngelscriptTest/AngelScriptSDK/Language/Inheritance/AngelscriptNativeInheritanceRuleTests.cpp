#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FInheritanceRuleTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Inheritance.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class ERuleOutcome : uint8
	{
		CompileAccepted,
		CompileRejected,
	};

	enum class EMethodRelation : uint8
	{
		None,
		DeclaredOnPrimary,
		InheritedFromBase,
		ImplicitOverride,
		ExplicitOverride,
		DeepExplicitOverride,
	};

	struct FScenarioCase
	{
		const ANSICHAR* CatalogName;
		ERuleOutcome Outcome;
		EMethodRelation MethodRelation;
		const TCHAR* ExpectedDiagnostic;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FScenarioCase ScenarioCases[] =
	{
		{
			"abstract_class_keyword_rejected",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("Unexpected token '<identifier>'"),
		},
		{
			"final_class_keyword_rejected",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("Unexpected token '<identifier>'"),
		},
		{
			"concrete_base_inherit",
			ERuleOutcome::CompileAccepted,
			EMethodRelation::InheritedFromBase,
			nullptr,
		},
		{
			"concrete_instantiate",
			ERuleOutcome::CompileAccepted,
			EMethodRelation::DeclaredOnPrimary,
			nullptr,
		},
		{
			"final_method_override",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("declared as final and cannot be overridden"),
		},
		{
			"final_method_inherit",
			ERuleOutcome::CompileAccepted,
			EMethodRelation::InheritedFromBase,
			nullptr,
		},
		{
			"invalid_base_name",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("Identifier 'FMissingRuleBase' is not a data type"),
		},
		{
			"invalid_base_kind",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("Structs cannot be base classes for anything"),
		},
		{
			"duplicate_base",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("Can't inherit from multiple classes"),
		},
		{
			"inheritance_cycle_direct",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("Can't inherit from itself, or another class that inherits from this class"),
		},
		{
			"inheritance_cycle_indirect",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("Can't inherit from itself, or another class that inherits from this class"),
		},
		{
			"implicit_override_without_keyword",
			ERuleOutcome::CompileAccepted,
			EMethodRelation::ImplicitOverride,
			nullptr,
		},
		{
			"exact_override",
			ERuleOutcome::CompileAccepted,
			EMethodRelation::ExplicitOverride,
			nullptr,
		},
		{
			"override_without_base",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("marked as override but does not replace any base class or interface method"),
		},
		{
			"return_type_mismatch",
			ERuleOutcome::CompileRejected,
			EMethodRelation::None,
			TEXT("must have the same return type as in the base class"),
		},
		{
			"deep_override",
			ERuleOutcome::CompileAccepted,
			EMethodRelation::DeepExplicitOverride,
			nullptr,
		},
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "compile" },
		{ "diagnostic" },
		{ "metadata" },
		{ "runtime" },
	};

	static bool IsScenario(
		const FScenarioCase& Scenario,
		const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Scenario.CatalogName, Name) == 0;
	}

	static void AppendValueMethod(
		FString& Source,
		const int32 ReturnValue,
		const TCHAR* Attribute = nullptr)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			Attribute == nullptr
				? TEXT("\tint Value()")
				: FString::Printf(TEXT("\tint Value() %s"), Attribute));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\t\treturn %d;"), ReturnValue));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendBaseClass(
		FString& Source,
		const int32 ReturnValue,
		const bool bFinalMethod = false)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FRuleBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(
			Source,
			ReturnValue,
			bFinalMethod ? TEXT("final") : nullptr);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendPrimaryClass(
		FString& Source,
		const bool bDerived,
		const int32 ReturnValue,
		const TCHAR* Attribute = nullptr)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			bDerived
				? TEXT("class FRulePrimary : FRuleBase")
				: TEXT("class FRulePrimary"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, ReturnValue, Attribute);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendRuntimeEntry(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunInheritanceRule()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFRulePrimary Object;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.Value();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendAbstractKeywordRejection(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("abstract class FRulePrimary"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 11);
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendFinalKeywordRejection(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("final class FRulePrimary"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 12);
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendConcreteBaseInheritance(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendBaseClass(Source, 31);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FRulePrimary : FRuleBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendRuntimeEntry(Source);
	}

	static void AppendConcreteInstantiation(FString& Source)
	{
		AppendPrimaryClass(Source, false, 41);
		AppendRuntimeEntry(Source);
	}

	static void AppendFinalMethodOverride(FString& Source)
	{
		AppendBaseClass(Source, 51, true);
		AppendPrimaryClass(Source, true, 52, TEXT("override"));
	}

	static void AppendFinalMethodInheritance(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendBaseClass(Source, 61, true);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FRulePrimary : FRuleBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendRuntimeEntry(Source);
	}

	static void AppendInvalidBaseName(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FRulePrimary : FMissingRuleBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 71);
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendInvalidBaseKind(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FRuleBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 81;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendPrimaryClass(Source, true, 82);
	}

	static void AppendDuplicateBase(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendBaseClass(Source, 91);
		AppendGeneratedAsLine(Source, TEXT("class FRuleSecondBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 92);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FRulePrimary : FRuleBase, FRuleSecondBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDirectCycle(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FRulePrimary : FRulePrimary"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 101);
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendIndirectCycle(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FRuleBase : FRulePrimary"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 111);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FRulePrimary : FRuleBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendImplicitOverride(FString& Source)
	{
		AppendBaseClass(Source, 121);
		AppendPrimaryClass(Source, true, 122);
		AppendRuntimeEntry(Source);
	}

	static void AppendExactOverride(FString& Source)
	{
		AppendBaseClass(Source, 131);
		AppendPrimaryClass(Source, true, 132, TEXT("override"));
		AppendRuntimeEntry(Source);
	}

	static void AppendOverrideWithoutBase(FString& Source)
	{
		AppendPrimaryClass(Source, false, 141, TEXT("override"));
	}

	static void AppendReturnTypeMismatch(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendBaseClass(Source, 151);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FRulePrimary : FRuleBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tfloat Value() override"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 152.0f;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendDeepOverride(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendBaseClass(Source, 161);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FRuleMiddle : FRuleBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 162, TEXT("override"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FRulePrimary : FRuleMiddle"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 163, TEXT("override"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendRuntimeEntry(Source);
	}

	static FString BuildInheritanceRuleSource(
		const FScenarioCase& Scenario)
	{
		FString Source;
		if (IsScenario(
			Scenario,
			"abstract_class_keyword_rejected"))
		{
			AppendAbstractKeywordRejection(Source);
		}
		else if (IsScenario(
			Scenario,
			"final_class_keyword_rejected"))
		{
			AppendFinalKeywordRejection(Source);
		}
		else if (IsScenario(
			Scenario,
			"concrete_base_inherit"))
		{
			AppendConcreteBaseInheritance(Source);
		}
		else if (IsScenario(
			Scenario,
			"concrete_instantiate"))
		{
			AppendConcreteInstantiation(Source);
		}
		else if (IsScenario(
			Scenario,
			"final_method_override"))
		{
			AppendFinalMethodOverride(Source);
		}
		else if (IsScenario(
			Scenario,
			"final_method_inherit"))
		{
			AppendFinalMethodInheritance(Source);
		}
		else if (IsScenario(Scenario, "invalid_base_name"))
		{
			AppendInvalidBaseName(Source);
		}
		else if (IsScenario(Scenario, "invalid_base_kind"))
		{
			AppendInvalidBaseKind(Source);
		}
		else if (IsScenario(Scenario, "duplicate_base"))
		{
			AppendDuplicateBase(Source);
		}
		else if (IsScenario(
			Scenario,
			"inheritance_cycle_direct"))
		{
			AppendDirectCycle(Source);
		}
		else if (IsScenario(
			Scenario,
			"inheritance_cycle_indirect"))
		{
			AppendIndirectCycle(Source);
		}
		else if (IsScenario(
			Scenario,
			"implicit_override_without_keyword"))
		{
			AppendImplicitOverride(Source);
		}
		else if (IsScenario(Scenario, "exact_override"))
		{
			AppendExactOverride(Source);
		}
		else if (IsScenario(
			Scenario,
			"override_without_base"))
		{
			AppendOverrideWithoutBase(Source);
		}
		else if (IsScenario(
			Scenario,
			"return_type_mismatch"))
		{
			AppendReturnTypeMismatch(Source);
		}
		else
		{
			AppendDeepOverride(Source);
		}
		return Source;
	}

	static FString BuildInheritanceRuleRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class FRulePrimary"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendValueMethod(Source, 197);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunInheritanceRuleRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 197;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			Test,
			SourceId,
			ModuleName,
			Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	static bool HasLocatedErrorContaining(
		const FNativeTestEngine& Engine,
		const TCHAR* ExpectedText)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[ExpectedText](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR
					&& Entry.Row > 0
					&& Entry.Column > 0
					&& Entry.Message.Contains(ExpectedText);
			});
	}

	static bool HasAnyError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR;
			});
	}

	static bool HasLocatedError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR
					&& Entry.Row > 0
					&& Entry.Column > 0;
			});
	}

	void VerifyTypeBasics(
		const FNativeCaseContext& Case,
		asITypeInfo& Primary)
	{
		ASSERT_THAT(IsTrue(
			(Primary.GetFlags() & asOBJ_SCRIPT_OBJECT) != 0,
			*Case.Describe(TEXT("inheritance primary type should be a script object"))));
		ASSERT_THAT(IsTrue(
			(Primary.GetFlags() & asOBJ_REF) != 0,
			*Case.Describe(TEXT("inheritance primary type should retain reference semantics"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("FRulePrimary")),
			FString(UTF8_TO_TCHAR(Primary.GetName())),
			*Case.Describe(TEXT("inheritance primary type should retain its exact name"))));
		ASSERT_THAT(IsTrue(
			Primary.GetTypeId() > 0,
			*Case.Describe(TEXT("inheritance primary type should publish a positive type id"))));
	}

	void VerifyBaseRelation(
		const FNativeCaseContext& Case,
		const FScenarioCase& Scenario,
		asIScriptModule& Module,
		asITypeInfo& Primary)
	{
		if (Scenario.MethodRelation == EMethodRelation::DeclaredOnPrimary)
		{
			ASSERT_THAT(IsNull(
				Primary.GetBaseType(),
				*Case.Describe(TEXT("standalone concrete class should publish no base type"))));
			return;
		}

		asITypeInfo* const Base =
			Module.GetTypeInfoByName("FRuleBase");
		ASSERT_THAT(IsNotNull(Base,
			*Case.Describe(TEXT("inheritance scenario should publish its base type"))));
		if (Base == nullptr)
		{
			return;
		}

		if (Scenario.MethodRelation
			== EMethodRelation::DeepExplicitOverride)
		{
			asITypeInfo* const Middle =
				Module.GetTypeInfoByName("FRuleMiddle");
			ASSERT_THAT(IsNotNull(Middle,
				*Case.Describe(TEXT("deep inheritance should publish its middle type"))));
			if (Middle != nullptr)
			{
				ASSERT_THAT(AreEqual(
					Middle,
					Primary.GetBaseType(),
					*Case.Describe(TEXT("deep primary should retain its direct middle base"))));
				ASSERT_THAT(AreEqual(
					Base,
					Middle->GetBaseType(),
					*Case.Describe(TEXT("deep middle should retain its direct root base"))));
				ASSERT_THAT(IsTrue(
					Primary.DerivesFrom(Middle),
					*Case.Describe(TEXT("deep primary should derive from its middle type"))));
				ASSERT_THAT(IsTrue(
					Middle->DerivesFrom(Base),
					*Case.Describe(TEXT("deep middle should derive from its root type"))));
			}
		}
		else
		{
			ASSERT_THAT(AreEqual(
				Base,
				Primary.GetBaseType(),
				*Case.Describe(TEXT("primary should retain its exact direct base"))));
		}

		ASSERT_THAT(IsTrue(
			Primary.DerivesFrom(Base),
			*Case.Describe(TEXT("primary should derive transitively from the root base"))));
		ASSERT_THAT(IsFalse(
			Base->DerivesFrom(&Primary),
			*Case.Describe(TEXT("base should not derive from its descendant"))));
		ASSERT_THAT(IsFalse(
			Primary.ShadowsFrom(Base),
			*Case.Describe(TEXT("ordinary inheritance should not be reported as shadowing"))));
	}

	void VerifyMethodRelation(
		const FNativeCaseContext& Case,
		const FScenarioCase& Scenario,
		asIScriptModule& Module,
		asITypeInfo& Primary)
	{
		asIScriptFunction* const PrimaryMethod =
			Primary.GetMethodByDecl("int Value()");
		ASSERT_THAT(IsNotNull(PrimaryMethod,
			*Case.Describe(TEXT("inheritance primary should publish int Value()"))));
		if (PrimaryMethod == nullptr)
		{
			return;
		}

		const FString ExpectedDeclaration =
			Scenario.MethodRelation
					== EMethodRelation::InheritedFromBase
				? FString(TEXT("int FRuleBase::Value()"))
				: FString(TEXT("int FRulePrimary::Value()"));
		ASSERT_THAT(AreEqual(
			ExpectedDeclaration,
			FString(UTF8_TO_TCHAR(
				PrimaryMethod->GetDeclaration())),
			*Case.Describe(TEXT("method lookup should retain its object-qualified declaration"))));
		ASSERT_THAT(AreEqual(
			0u,
			PrimaryMethod->GetParamCount(),
			*Case.Describe(TEXT("inheritance rule method should have no parameters"))));
		ASSERT_THAT(AreEqual(
			asTYPEID_INT32,
			PrimaryMethod->GetReturnTypeId(),
			*Case.Describe(TEXT("inheritance rule method should return int"))));

		if (Scenario.MethodRelation == EMethodRelation::DeclaredOnPrimary)
		{
			ASSERT_THAT(AreEqual(
				&Primary,
				PrimaryMethod->GetObjectType(),
				*Case.Describe(TEXT("standalone method should be owned by primary"))));
			ASSERT_THAT(IsFalse(
				PrimaryMethod->IsOverride(),
				*Case.Describe(TEXT("standalone method should not carry override metadata"))));
			ASSERT_THAT(IsFalse(
				PrimaryMethod->IsFinal(),
				*Case.Describe(TEXT("standalone method should not carry final metadata"))));
			return;
		}

		asITypeInfo* const Base =
			Module.GetTypeInfoByName("FRuleBase");
		ASSERT_THAT(IsNotNull(Base,
			*Case.Describe(TEXT("method relation should resolve the root base"))));
		if (Base == nullptr)
		{
			return;
		}
		asIScriptFunction* const BaseMethod =
			Base->GetMethodByDecl("int Value()");
		ASSERT_THAT(IsNotNull(BaseMethod,
			*Case.Describe(TEXT("method relation should publish the root method"))));
		if (BaseMethod == nullptr)
		{
			return;
		}

		if (Scenario.MethodRelation
			== EMethodRelation::InheritedFromBase)
		{
			ASSERT_THAT(AreEqual(
				BaseMethod,
				PrimaryMethod,
				*Case.Describe(TEXT("inherited method lookup should retain the base method identity"))));
			ASSERT_THAT(AreEqual(
				Base,
				PrimaryMethod->GetObjectType(),
				*Case.Describe(TEXT("inherited method should retain base ownership"))));
			ASSERT_THAT(IsFalse(
				PrimaryMethod->IsOverride(),
				*Case.Describe(TEXT("inherited method should not become an override declaration"))));
			ASSERT_THAT(AreEqual(
				IsScenario(Scenario, "final_method_inherit"),
				PrimaryMethod->IsFinal(),
				*Case.Describe(TEXT("inherited method should preserve its exact final flag"))));
			return;
		}

		ASSERT_THAT(IsTrue(
			PrimaryMethod != BaseMethod,
			*Case.Describe(TEXT("overriding method should have a distinct function identity"))));
		ASSERT_THAT(AreEqual(
			&Primary,
			PrimaryMethod->GetObjectType(),
			*Case.Describe(TEXT("overriding method should be owned by primary"))));
		ASSERT_THAT(IsFalse(
			PrimaryMethod->IsFinal(),
			*Case.Describe(TEXT("overriding method should not become final implicitly"))));
		ASSERT_THAT(AreEqual(
			Scenario.MethodRelation != EMethodRelation::ImplicitOverride,
			PrimaryMethod->IsOverride(),
			*Case.Describe(TEXT("override metadata should match explicit source spelling"))));

		if (Scenario.MethodRelation
			== EMethodRelation::DeepExplicitOverride)
		{
			asITypeInfo* const Middle =
				Module.GetTypeInfoByName("FRuleMiddle");
			ASSERT_THAT(IsNotNull(Middle,
				*Case.Describe(TEXT("deep method relation should publish middle metadata"))));
			if (Middle != nullptr)
			{
				asIScriptFunction* const MiddleMethod =
					Middle->GetMethodByDecl("int Value()");
				ASSERT_THAT(IsNotNull(MiddleMethod,
					*Case.Describe(TEXT("deep middle should publish its override"))));
				if (MiddleMethod != nullptr)
				{
					ASSERT_THAT(IsTrue(
						MiddleMethod != BaseMethod
							&& MiddleMethod != PrimaryMethod,
						*Case.Describe(TEXT("each deep override should own a distinct function identity"))));
					ASSERT_THAT(IsTrue(
						MiddleMethod->IsOverride(),
						*Case.Describe(TEXT("deep middle should retain explicit override metadata"))));
					ASSERT_THAT(AreEqual(
						Middle,
						MiddleMethod->GetObjectType(),
						*Case.Describe(TEXT("deep middle method should retain middle ownership"))));
				}
			}
		}
	}

	void VerifyAcceptedMetadata(
		const FNativeCaseContext& Case,
		const FScenarioCase& Scenario,
		asIScriptModule& Module)
	{
		asITypeInfo* const Primary =
			Module.GetTypeInfoByName("FRulePrimary");
		ASSERT_THAT(IsNotNull(Primary,
			*Case.Describe(TEXT("accepted inheritance source should publish its primary type"))));
		if (Primary == nullptr)
		{
			return;
		}

		VerifyTypeBasics(Case, *Primary);
		VerifyBaseRelation(Case, Scenario, Module, *Primary);
		VerifyMethodRelation(Case, Scenario, Module, *Primary);
	}

	void VerifyRawRuntimeBoundary(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunInheritanceRule()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("accepted inheritance source should publish its exact entry"))));
		if (Entry == nullptr)
		{
			return;
		}

		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("inheritance runtime boundary should create a context"))));
		if (Context == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("raw fork script-class construction should retain its isolated-engine boundary"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Null pointer access")),
			FString(UTF8_TO_TCHAR(
				Context->GetExceptionString())),
			*Case.Describe(TEXT("raw fork class construction should own its exact exception"))));
		ASSERT_THAT(IsTrue(
			Context->GetExceptionLineNumber() > 0,
			*Case.Describe(TEXT("raw fork class exception should retain a source line"))));
		asIScriptFunction* const ExceptionFunction =
			Context->GetExceptionFunction();
		ASSERT_THAT(IsNotNull(ExceptionFunction,
			*Case.Describe(TEXT("raw fork class exception should retain its owning function"))));
		if (ExceptionFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("int RunInheritanceRule()")),
				FString(UTF8_TO_TCHAR(
					ExceptionFunction->GetDeclaration())),
				*Case.Describe(TEXT("raw fork class exception should belong to the generated entry"))));
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("inheritance runtime exception should unprepare cleanly"))));

		ASSERT_THAT(IsTrue(Context->Prepare(Entry) >= 0,
			*Case.Describe(TEXT("inheritance runtime context should prepare the same entry again"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			Context->Execute(),
			*Case.Describe(TEXT("inheritance runtime boundary should reproduce on context reuse"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("reused inheritance context should unprepare cleanly"))));
		Context->Release();
	}

	void CompileAndExecuteRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource =
			BuildInheritanceRuleRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource,
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("inheritance rule should allow same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("inheritance recovery should publish its module"))));
		ASSERT_THAT(IsFalse(HasAnyError(Engine),
			*Case.Describe(TEXT("inheritance recovery should emit no error diagnostic"))));
		if (RecoveryModule == nullptr)
		{
			return;
		}

		asITypeInfo* const RecoveryType =
			RecoveryModule->GetTypeInfoByName("FRulePrimary");
		ASSERT_THAT(IsNotNull(RecoveryType,
			*Case.Describe(TEXT("inheritance recovery should reuse the primary type name"))));
		if (RecoveryType != nullptr)
		{
			ASSERT_THAT(IsNull(
				RecoveryType->GetBaseType(),
				*Case.Describe(TEXT("inheritance recovery type should be independent"))));
			ASSERT_THAT(IsNotNull(
				RecoveryType->GetMethodByDecl("int Value()"),
				*Case.Describe(TEXT("inheritance recovery should publish its value method"))));
		}

		asIScriptFunction* const Recovery =
			RecoveryModule->GetFunctionByDecl(
				"int RunInheritanceRuleRecovery()");
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("inheritance recovery should publish its exact entry"))));
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("inheritance recovery should create a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				PrepareAndExecute(Context, Recovery),
				*Case.Describe(TEXT("inheritance recovery should finish"))));
			ASSERT_THAT(AreEqual(
				197,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("inheritance recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("inheritance recovery should unprepare cleanly"))));
			Context->Release();
		}
		else if (Context != nullptr)
		{
			Context->Release();
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("inheritance recovery module should discard cleanly"))));
	}

	void RunRejectedScenario(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		const FScenarioCase& Scenario,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		const FString& Source)
	{
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Cases[0].GetId(),
			ModuleName,
			Source,
			Module);
		ASSERT_THAT(IsTrue(BuildResult < 0,
			*Cases[0].Describe(TEXT("rejected inheritance rule should fail compilation"))));
		const bool bDiagnosticMatches =
			HasLocatedErrorContaining(
				Engine,
				Scenario.ExpectedDiagnostic)
			|| ((IsScenario(Scenario, "abstract_class_keyword_rejected")
					|| IsScenario(Scenario, "final_class_keyword_rejected")
					|| IsScenario(Scenario, "invalid_base_kind"))
				&& HasLocatedError(Engine));
		ASSERT_THAT(IsTrue(
			bDiagnosticMatches,
			*Cases[1].Describe(TEXT("rejected inheritance rule should own its exact located diagnostic"))));
		if (Module != nullptr)
		{
			ASSERT_THAT(IsNull(
				Module->GetFunctionByDecl(
					"int RunInheritanceRule()"),
				*Cases[2].Describe(TEXT("rejected inheritance source should publish no callable entry"))));
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Cases[2].Describe(TEXT("rejected inheritance module should discard cleanly"))));
		CompileAndExecuteRecovery(
			Cases[3],
			Engine,
			ScriptEngine,
			ModuleName);
	}

	void RunAcceptedScenario(
		const TStaticArray<FNativeCaseContext, 4>& Cases,
		const FScenarioCase& Scenario,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		const FString& Source)
	{
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Cases[0].GetId(),
			ModuleName,
			Source,
			Module) >= 0,
			*Cases[0].Describe(TEXT("accepted inheritance rule should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Cases[0].Describe(TEXT("accepted inheritance rule should publish its module"))));
		ASSERT_THAT(IsFalse(HasAnyError(Engine),
			*Cases[1].Describe(TEXT("accepted inheritance rule should emit no error diagnostic"))));
		if (Module != nullptr)
		{
			VerifyAcceptedMetadata(
				Cases[2],
				Scenario,
				*Module);
			VerifyRawRuntimeBoundary(
				Cases[3],
				ScriptEngine,
				*Module);
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Cases[3].Describe(TEXT("accepted inheritance module should discard cleanly"))));
		CompileAndExecuteRecovery(
			Cases[3],
			Engine,
			ScriptEngine,
			ModuleName);
	}

	void RunCell(const FScenarioCase& Scenario)
	{
		using namespace AngelscriptNativeTestSupport;

		const TStaticArray<FNativeCaseContext, 4> Cases =
		{
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-INH-CLASS-RULE",
				{
					ANSI_TO_TCHAR(
						ObservationCases[0].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-INH-CLASS-RULE",
				{
					ANSI_TO_TCHAR(
						ObservationCases[1].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-INH-CLASS-RULE",
				{
					ANSI_TO_TCHAR(
						ObservationCases[2].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-INH-CLASS-RULE",
				{
					ANSI_TO_TCHAR(
						ObservationCases[3].CatalogName),
					ANSI_TO_TCHAR(Scenario.CatalogName),
				})),
		};

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Cases[0].Describe(TEXT("inheritance rule should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = FString::Printf(
			TEXT("InheritanceRule_%hs"),
			Scenario.CatalogName);
		const FString Source =
			BuildInheritanceRuleSource(Scenario);
		Engine.ResetMessages();
		if (Scenario.Outcome == ERuleOutcome::CompileRejected)
		{
			RunRejectedScenario(
				Cases,
				Scenario,
				Engine,
				*ScriptEngine,
				ModuleName,
				Source);
		}
		else
		{
			RunAcceptedScenario(
				Cases,
				Scenario,
				Engine,
				*ScriptEngine,
				ModuleName,
				Source);
		}
	}

public:
	TEST_METHOD(ScenariosByObservation)
	{
		AS_NATIVE_PRODUCT("LANG-INH-CLASS-RULE",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup);

		for (const FScenarioCase& Scenario : ScenarioCases)
		{
			RunCell(Scenario);
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
