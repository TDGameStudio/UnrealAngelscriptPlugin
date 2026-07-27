#include "AngelscriptNativeConversionTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::FNativeTestEngine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FValueObjectConversionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.ValueObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FAvailabilityCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FTargetCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FFormCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FOutcomeCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FAvailabilityCase AvailabilityCases[] =
	{
		{ "implicit_constructor" },
		{ "explicit_constructor" },
		{ "implicit_operator" },
		{ "explicit_operator" },
		{ "constructor_and_operator" },
		{ "none" },
	};

	inline static constexpr FTargetCase TargetCases[] =
	{
		{ "same_value" },
		{ "other_value" },
		{ "int" },
		{ "float64" },
		{ "bool" },
	};

	inline static constexpr FFormCase FormCases[] =
	{
		{ "assignment" },
		{ "initializer" },
		{ "argument" },
		{ "return" },
		{ "explicit_cast" },
		{ "direct_constructor" },
	};

	inline static constexpr FOutcomeCase OutcomeCases[] =
	{
		{ "direct" },
		{ "selected" },
		{ "ambiguous" },
		{ "rejected" },
	};

	static bool IsNamed(const ANSICHAR* Value, const ANSICHAR* Name)
	{
		return AngelscriptNativeTestSupport::EqualAnsi(Value, Name);
	}

	static bool IsNamed(const FAvailabilityCase& Case, const ANSICHAR* Name)
	{
		return IsNamed(Case.CatalogName, Name);
	}

	static bool IsNamed(const FTargetCase& Case, const ANSICHAR* Name)
	{
		return IsNamed(Case.CatalogName, Name);
	}

	static bool IsNamed(const FFormCase& Case, const ANSICHAR* Name)
	{
		return IsNamed(Case.CatalogName, Name);
	}

	static bool IsNamed(const FOutcomeCase& Case, const ANSICHAR* Name)
	{
		return IsNamed(Case.CatalogName, Name);
	}

	static bool IsExplicitForm(const FFormCase& FormCase)
	{
		return IsNamed(FormCase, "explicit_cast")
			|| IsNamed(FormCase, "direct_constructor");
	}

	static FString ResolveTargetType(
		const FTargetCase& TargetCase,
		const asIScriptEngine& Engine)
	{
		if (IsNamed(TargetCase, "same_value"))
		{
			return TEXT("FValueConversionSource");
		}

		if (IsNamed(TargetCase, "other_value"))
		{
			return TEXT("FValueConversionTarget");
		}

		if (IsNamed(TargetCase, "float64"))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0
				? TEXT("float")
				: TEXT("float64");
		}

		return ANSI_TO_TCHAR(TargetCase.CatalogName);
	}

	static bool HasImplicitSelectedPath(
		const FAvailabilityCase& AvailabilityCase,
		const FTargetCase& TargetCase)
	{
		if (IsNamed(TargetCase, "same_value"))
		{
			return true;
		}

		if (IsNamed(TargetCase, "other_value"))
		{
			return IsNamed(AvailabilityCase, "implicit_constructor")
				|| IsNamed(AvailabilityCase, "implicit_operator")
				|| IsNamed(AvailabilityCase, "constructor_and_operator");
		}

		return IsNamed(AvailabilityCase, "implicit_operator")
			|| IsNamed(AvailabilityCase, "constructor_and_operator");
	}

	static bool HasExplicitSelectedPath(
		const FAvailabilityCase& AvailabilityCase,
		const FTargetCase& TargetCase)
	{
		if (IsNamed(TargetCase, "same_value"))
		{
			return true;
		}

		if (IsNamed(TargetCase, "other_value"))
		{
			return !IsNamed(AvailabilityCase, "none");
		}

		return IsNamed(AvailabilityCase, "implicit_operator")
			|| IsNamed(AvailabilityCase, "explicit_operator")
			|| IsNamed(AvailabilityCase, "constructor_and_operator");
	}

	static bool ShouldCompilePositivePath(
		const FAvailabilityCase& AvailabilityCase,
		const FTargetCase& TargetCase,
		const FFormCase& FormCase)
	{
		return IsExplicitForm(FormCase)
			? HasExplicitSelectedPath(AvailabilityCase, TargetCase)
			: HasImplicitSelectedPath(AvailabilityCase, TargetCase);
	}

	static int32 ExpectedSelectedMarker(
		const FAvailabilityCase& AvailabilityCase,
		const FTargetCase& TargetCase)
	{
		if (IsNamed(TargetCase, "same_value"))
		{
			return 7;
		}

		if (IsNamed(TargetCase, "other_value"))
		{
			return IsNamed(AvailabilityCase, "implicit_constructor")
				|| IsNamed(AvailabilityCase, "explicit_constructor")
				|| IsNamed(AvailabilityCase, "constructor_and_operator")
				? 107
				: IsNamed(AvailabilityCase, "implicit_operator")
					? 207
					: 307;
		}

		if (IsNamed(TargetCase, "bool"))
		{
			return IsNamed(AvailabilityCase, "explicit_operator") ? 0 : 1;
		}

		return IsNamed(AvailabilityCase, "explicit_operator") ? 307 : 207;
	}

	static void AppendTargetValueDefinition(
		FString& Source,
		const FAvailabilityCase& AvailabilityCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FValueConversionSource;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FValueConversionTarget"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFValueConversionTarget(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);

		if (IsNamed(AvailabilityCase, "implicit_constructor")
			|| IsNamed(AvailabilityCase, "constructor_and_operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFValueConversionTarget(const FValueConversionSource&in InValue)"));
		}
		else if (IsNamed(AvailabilityCase, "explicit_constructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFValueConversionTarget(const FValueConversionSource&in InValue) explicit"));
		}

		if (IsNamed(AvailabilityCase, "implicit_constructor")
			|| IsNamed(AvailabilityCase, "explicit_constructor")
			|| IsNamed(AvailabilityCase, "constructor_and_operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = 107;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendSourceConversionDefinition(
		FString& Source,
		const FAvailabilityCase& AvailabilityCase,
		const FTargetCase& TargetCase,
		const FString& TargetType)
	{
		using namespace AngelscriptNativeTestSupport;

		const bool bImplicitOperator = IsNamed(AvailabilityCase, "implicit_operator")
			|| IsNamed(AvailabilityCase, "constructor_and_operator");
		const bool bExplicitOperator = IsNamed(AvailabilityCase, "explicit_operator");

		AppendGeneratedAsLine(Source, TEXT("struct FValueConversionSource"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFValueConversionSource(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);

		const FString OperatorReturnType = IsNamed(TargetCase, "same_value")
			? TEXT("int")
			: TargetType;
		const FString ImplicitReturn = IsNamed(TargetCase, "other_value")
			? TEXT("FValueConversionTarget(Value + 200)")
			: IsNamed(TargetCase, "bool")
				? TEXT("true")
				: FString::Printf(TEXT("%s(Value + 200)"), *OperatorReturnType);
		const FString ExplicitReturn = IsNamed(TargetCase, "other_value")
			? TEXT("FValueConversionTarget(Value + 300)")
			: IsNamed(TargetCase, "bool")
				? TEXT("false")
				: FString::Printf(TEXT("%s(Value + 300)"), *OperatorReturnType);

		if (bImplicitOperator)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s opImplConv() const"), *OperatorReturnType));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn ") + ImplicitReturn + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
		}

		if (bExplicitOperator)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s opConv() const"), *OperatorReturnType));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn ") + ExplicitReturn + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendTargetObservationHelpers(
		FString& Source,
		const FString& TargetType)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int ObserveValueConversion(%s Value)"), *TargetType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (TargetType == TEXT("FValueConversionSource") || TargetType == TEXT("FValueConversionTarget"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		}
		else if (TargetType == TEXT("bool"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 1 : 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s ReturnValueConversionTarget(%s Value)"), *TargetType, *TargetType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendAmbiguousOrRejectedSource(
		FString& Source,
		const FOutcomeCase& OutcomeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsNamed(OutcomeCase, "ambiguous"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FValueAmbiguousLeft"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFValueAmbiguousLeft(const FValueConversionSource&in Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("struct FValueAmbiguousRight"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFValueAmbiguousRight(const FValueConversionSource&in Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int SelectValueAmbiguity(FValueAmbiguousLeft Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int SelectValueAmbiguity(FValueAmbiguousRight Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 2;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("struct FValueRejectedTarget"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int RejectValueConversion(FValueRejectedTarget Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static FString BuildValueObjectConversionSource(
		const FAvailabilityCase& AvailabilityCase,
		const FTargetCase& TargetCase,
		const FString& TargetType,
		const FFormCase& FormCase,
		const FOutcomeCase& OutcomeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendTargetValueDefinition(Source, AvailabilityCase);
		AppendSourceConversionDefinition(Source, AvailabilityCase, TargetCase, TargetType);
		AppendTargetObservationHelpers(Source, TargetType);
		if (IsNamed(OutcomeCase, "ambiguous") || IsNamed(OutcomeCase, "rejected"))
		{
			AppendAmbiguousOrRejectedSource(Source, OutcomeCase);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunValueObjectConversion()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFValueConversionSource SourceValue(7);"));

		if (IsNamed(OutcomeCase, "ambiguous"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SelectValueAmbiguity(SourceValue);"));
		}
		else if (IsNamed(OutcomeCase, "rejected"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn RejectValueConversion(SourceValue);"));
		}
		else if (IsNamed(FormCase, "assignment"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s TargetValue;"), *TargetType));
			AppendGeneratedAsLine(Source, TEXT("\tTargetValue = SourceValue;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveValueConversion(TargetValue);"));
		}
		else if (IsNamed(FormCase, "initializer"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s TargetValue = SourceValue;"), *TargetType));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveValueConversion(TargetValue);"));
		}
		else if (IsNamed(FormCase, "argument"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveValueConversion(SourceValue);"));
		}
		else if (IsNamed(FormCase, "return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s TargetValue = ReturnValueConversionTarget(SourceValue);"), *TargetType));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveValueConversion(TargetValue);"));
		}
		else if (IsNamed(FormCase, "explicit_cast"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s TargetValue = %s(SourceValue);"), *TargetType, *TargetType));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveValueConversion(TargetValue);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s TargetValue(SourceValue);"), *TargetType));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveValueConversion(TargetValue);"));
		}

		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool ShouldCompile(
		const FAvailabilityCase& AvailabilityCase,
		const FTargetCase& TargetCase,
		const FFormCase& FormCase,
		const FOutcomeCase& OutcomeCase)
	{
		return !IsNamed(OutcomeCase, "ambiguous")
			&& !IsNamed(OutcomeCase, "rejected")
			&& ShouldCompilePositivePath(AvailabilityCase, TargetCase, FormCase);
	}

public:

	TEST_METHOD(AvailabilityByTargetFormAndOutcome)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-VALUE-OBJECT",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Value-object conversion product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FAvailabilityCase& AvailabilityCase : AvailabilityCases)
		{
			for (const FTargetCase& TargetCase : TargetCases)
			{
				for (const FFormCase& FormCase : FormCases)
				{
					for (const FOutcomeCase& OutcomeCase : OutcomeCases)
					{
						const FString TargetType = ResolveTargetType(TargetCase, *ScriptEngine);
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-CONV-VALUE-OBJECT",
							{ ANSI_TO_TCHAR(AvailabilityCase.CatalogName), ANSI_TO_TCHAR(TargetCase.CatalogName),
								ANSI_TO_TCHAR(FormCase.CatalogName), ANSI_TO_TCHAR(OutcomeCase.CatalogName) }));
						const FString ModuleName = TEXT("ValueObjectConversion_") + Case.GetId();
						const FString Source = BuildValueObjectConversionSource(AvailabilityCase, TargetCase, TargetType, FormCase, OutcomeCase);

						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						NativeEngine.Reset(*TestRunner);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(
							ScriptEngine,
							ModuleNameUtf8.Get(),
							SourceUtf8.Get(),
							Module);

						if (!ShouldCompile(AvailabilityCase, TargetCase, FormCase, OutcomeCase))
						{
							ASSERT_THAT(IsTrue(BuildResult < 0,
								*Case.Describe(TEXT("unavailable, ambiguous, or rejected value conversion should fail compilation"))));
							ASSERT_THAT(IsTrue(HasOwnedLocatedDiagnostic(NativeEngine.GetMessages(), ModuleName),
								*Case.Describe(TEXT("rejected value conversion should identify one causal source location"))));
						}
						else if (BuildResult >= 0)
						{
							asIScriptFunction* const Entry = FindNoArgumentEntry(Module, TEXT("int"), TEXT("RunValueObjectConversion"));
							ASSERT_THAT(IsNotNull(Entry,
								*Case.Describe(TEXT("accepted value conversion should resolve its exact entry declaration"))));
							if (Entry != nullptr)
							{
								int32 ConvertedMarker = INDEX_NONE;
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
									ExecuteNoArgumentIntEntry(*ScriptEngine, *Entry, ConvertedMarker),
									*Case.Describe(TEXT("accepted value conversion should execute and release its temporaries"))));
								ASSERT_THAT(AreEqual(ExpectedSelectedMarker(AvailabilityCase, TargetCase),
									ConvertedMarker,
									*Case.Describe(TEXT("accepted value conversion should preserve the selected constructor or operator marker"))));
							}
						}

						ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, ModuleNameUtf8),
							*Case.Describe(TEXT("value conversion cell should discard its module"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
