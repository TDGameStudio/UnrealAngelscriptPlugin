#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionOverloadResolutionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.OverloadResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	struct FDiscriminatorCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FOutcomeCase
	{
		const ANSICHAR* CatalogName;
		bool bShouldCompile;
	};

	inline static constexpr FDiscriminatorCase DiscriminatorCases[] =
	{
		{ "type" },
		{ "arity" },
		{ "const" },
		{ "direction" },
		{ "namespace" },
		{ "default" },
		{ "conversion" },
	};

	inline static constexpr FOutcomeCase OutcomeCases[] =
	{
		{ "exact", true },
		{ "promotion", true },
		{ "ambiguous", false },
		{ "missing", false },
	};

	static bool IsDiscriminator(const FDiscriminatorCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static bool IsOutcome(const FOutcomeCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static int32 ExpectedMarker(const FDiscriminatorCase& DiscriminatorCase, const FOutcomeCase& OutcomeCase)
	{
		const int32 Base =
			IsDiscriminator(DiscriminatorCase, "type") ? 100
			: IsDiscriminator(DiscriminatorCase, "arity") ? 200
			: IsDiscriminator(DiscriminatorCase, "const") ? 300
			: IsDiscriminator(DiscriminatorCase, "direction") ? 400
			: IsDiscriminator(DiscriminatorCase, "namespace") ? 500
			: IsDiscriminator(DiscriminatorCase, "default") ? 600
			: 700;
		return Base + (IsOutcome(OutcomeCase, "exact") ? 1 : 2);
	}

	static FString ExpectedDeclaration(
		const FDiscriminatorCase& DiscriminatorCase,
		const FOutcomeCase& OutcomeCase)
	{
		if (IsDiscriminator(DiscriminatorCase, "arity") || IsDiscriminator(DiscriminatorCase, "default"))
		{
			return IsOutcome(OutcomeCase, "exact")
				? TEXT("int Probe(const int, const int)")
				: TEXT("int Probe(double, double)");
		}
		if (IsDiscriminator(DiscriminatorCase, "const"))
		{
			return IsOutcome(OutcomeCase, "exact") ? TEXT("int Probe()") : TEXT("int Probe() const");
		}
		if (IsDiscriminator(DiscriminatorCase, "direction"))
		{
			return IsOutcome(OutcomeCase, "exact")
				? TEXT("int Probe(const int&in)")
				: TEXT("int Probe(const double&in)");
		}
		if (IsOutcome(OutcomeCase, "promotion"))
		{
			return TEXT("int Probe(double)");
		}
		return TEXT("int Probe(const int)");
	}

	static FString ExpectedNamespace(
		const FDiscriminatorCase& DiscriminatorCase,
		const FOutcomeCase& OutcomeCase)
	{
		return DiscriminatorCase.CatalogName != nullptr
			&& IsDiscriminator(DiscriminatorCase, "namespace")
			&& OutcomeCase.bShouldCompile
			? TEXT("A")
			: FString();
	}

	static void AppendGeneratedAsLine(
		FString& Source,
		const FString& Line = FString())
	{
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, Line);
	}

	static void AppendSimpleFunction(
		FString& Source,
		const TCHAR* Parameters,
		const int32 Marker,
		const TCHAR* Indentation = TEXT(""),
		const TCHAR* Qualifier = TEXT(""))
	{
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sint Probe(%s)%s"), Indentation, Parameters, Qualifier));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s{"), Indentation));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s\treturn %d;"), Indentation, Marker));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s}"), Indentation));
	}

	static void AppendEntry(FString& Source, const TCHAR* Body)
	{
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Run()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		TArray<FString> Statements;
		FString(Body).ParseIntoArray(Statements, TEXT(";"), true);
		for (FString& Statement : Statements)
		{
			Statement.TrimStartAndEndInline();
			AppendGeneratedAsLine(Source, TEXT("\t") + Statement + TEXT(";"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void BuildTypeOrConversionSource(
		FString& Source,
		const FDiscriminatorCase& DiscriminatorCase,
		const FOutcomeCase& OutcomeCase)
	{
		if (IsOutcome(OutcomeCase, "exact"))
		{
			AppendSimpleFunction(Source, TEXT("int Value"), ExpectedMarker(DiscriminatorCase, OutcomeCase));
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT("double Value"), ExpectedMarker(DiscriminatorCase, OutcomeCase) + 10);
			const TCHAR* Call = IsDiscriminator(DiscriminatorCase, "conversion") ? TEXT("return Probe(int(7));") : TEXT("return Probe(7);");
			AppendEntry(Source, Call);
		}
		else if (IsOutcome(OutcomeCase, "promotion"))
		{
			AppendSimpleFunction(Source, TEXT("double Value"), ExpectedMarker(DiscriminatorCase, OutcomeCase));
			AppendEntry(Source, TEXT("return Probe(7);"));
		}
		else if (IsOutcome(OutcomeCase, "ambiguous"))
		{
			AppendSimpleFunction(Source, TEXT("float Value"), 1);
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT("double Value"), 2);
			AppendEntry(Source, TEXT("return Probe(7);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("struct FValue"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT("FValue Value"), 1);
			AppendEntry(Source, TEXT("return Probe(true);"));
		}
	}

	static void BuildArityOrDefaultSource(
		FString& Source,
		const FDiscriminatorCase& DiscriminatorCase,
		const FOutcomeCase& OutcomeCase)
	{
		if (IsOutcome(OutcomeCase, "exact"))
		{
			AppendSimpleFunction(Source, TEXT("int Value"), ExpectedMarker(DiscriminatorCase, OutcomeCase) + 10);
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT("int Left, int Right"), ExpectedMarker(DiscriminatorCase, OutcomeCase));
			AppendEntry(Source, TEXT("return Probe(3, 4);"));
		}
		else if (IsOutcome(OutcomeCase, "promotion"))
		{
			AppendSimpleFunction(Source, TEXT("double Left, double Right"), ExpectedMarker(DiscriminatorCase, OutcomeCase));
			AppendEntry(Source, TEXT("return Probe(3, 4);"));
		}
		else if (IsOutcome(OutcomeCase, "ambiguous"))
		{
			AppendSimpleFunction(Source, TEXT("int Value"), 1);
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT("int Value, int Extra = 2"), 2);
			AppendEntry(Source, TEXT("return Probe(3);"));
		}
		else
		{
			AppendSimpleFunction(Source, TEXT("int Left, int Right"), 1);
			AppendEntry(Source, IsDiscriminator(DiscriminatorCase, "default")
				? TEXT("return Probe();")
				: TEXT("return Probe(1, 2, 3);"));
		}
	}

	static void BuildConstSource(
		FString& Source,
		const FOutcomeCase& OutcomeCase)
	{
		AppendGeneratedAsLine(Source, TEXT("struct FOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsOutcome(OutcomeCase, "exact"))
		{
			AppendSimpleFunction(Source, TEXT(""), 301, TEXT("\t"));
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT(""), 311, TEXT("\t"), TEXT(" const"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Invoke()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Probe();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsOutcome(OutcomeCase, "promotion"))
		{
			AppendSimpleFunction(Source, TEXT(""), 302, TEXT("\t"), TEXT(" const"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Invoke()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Probe();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsOutcome(OutcomeCase, "ambiguous"))
		{
			AppendSimpleFunction(Source, TEXT("float Value"), 1, TEXT("\t"), TEXT(" const"));
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT("double Value"), 2, TEXT("\t"), TEXT(" const"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Invoke() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Probe(7);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendSimpleFunction(Source, TEXT(""), 1, TEXT("\t"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Invoke() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Probe();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendEntry(Source, TEXT("FOwner Owner; return Owner.Invoke();"));
	}

	static void BuildDirectionSource(FString& Source, const FOutcomeCase& OutcomeCase)
	{
		if (IsOutcome(OutcomeCase, "exact"))
		{
			AppendSimpleFunction(Source, TEXT("int& in Value"), 401);
			AppendEntry(Source, TEXT("int Value = 7; return Probe(Value);"));
		}
		else if (IsOutcome(OutcomeCase, "promotion"))
		{
			AppendSimpleFunction(Source, TEXT("double& in Value"), 402);
			AppendEntry(Source, TEXT("double Value = 7.0; return Probe(Value);"));
		}
		else if (IsOutcome(OutcomeCase, "ambiguous"))
		{
			AppendSimpleFunction(Source, TEXT("float& in Value"), 1);
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT("double& in Value"), 2);
			AppendEntry(Source, TEXT("int Value = 7; return Probe(Value);"));
		}
		else
		{
			AppendSimpleFunction(Source, TEXT("int& out Value"), 1);
			AppendEntry(Source, TEXT("const int Value = 7; return Probe(Value);"));
		}
	}

	static void BuildNamespaceSource(FString& Source, const FOutcomeCase& OutcomeCase)
	{
		if (IsOutcome(OutcomeCase, "missing"))
		{
			AppendEntry(Source, TEXT("return Missing::Probe(7);"));
			return;
		}

		AppendGeneratedAsLine(Source, TEXT("namespace A"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsOutcome(OutcomeCase, "exact"))
		{
			AppendSimpleFunction(Source, TEXT("int Value"), 501, TEXT("\t"));
		}
		else if (IsOutcome(OutcomeCase, "promotion"))
		{
			AppendSimpleFunction(Source, TEXT("double Value"), 502, TEXT("\t"));
		}
		else
		{
			AppendSimpleFunction(Source, TEXT("float Value"), 1, TEXT("\t"));
			AppendGeneratedAsLine(Source);
			AppendSimpleFunction(Source, TEXT("double Value"), 2, TEXT("\t"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("namespace B"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSimpleFunction(Source, TEXT("int Value"), 599, TEXT("\t"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendEntry(Source, TEXT("return A::Probe(7);"));
	}

	static FString BuildOverloadSource(
		const FDiscriminatorCase& DiscriminatorCase,
		const FOutcomeCase& OutcomeCase)
	{
		FString Source;
		if (IsDiscriminator(DiscriminatorCase, "type") || IsDiscriminator(DiscriminatorCase, "conversion"))
		{
			BuildTypeOrConversionSource(Source, DiscriminatorCase, OutcomeCase);
		}
		else if (IsDiscriminator(DiscriminatorCase, "arity") || IsDiscriminator(DiscriminatorCase, "default"))
		{
			BuildArityOrDefaultSource(Source, DiscriminatorCase, OutcomeCase);
		}
		else if (IsDiscriminator(DiscriminatorCase, "const"))
		{
			BuildConstSource(Source, OutcomeCase);
		}
		else if (IsDiscriminator(DiscriminatorCase, "direction"))
		{
			BuildDirectionSource(Source, OutcomeCase);
		}
		else
		{
			BuildNamespaceSource(Source, OutcomeCase);
		}
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static asIScriptFunction* FindExpectedFunction(
		asIScriptModule& Module,
		const FDiscriminatorCase& DiscriminatorCase,
		const FOutcomeCase& OutcomeCase)
	{
		const FString Declaration = ExpectedDeclaration(DiscriminatorCase, OutcomeCase);
		if (IsDiscriminator(DiscriminatorCase, "const"))
		{
			asITypeInfo* const Owner = Module.GetTypeInfoByName("FOwner");
			return Owner != nullptr ? Owner->GetMethodByDecl(TCHAR_TO_ANSI(*Declaration)) : nullptr;
		}

		const FString Namespace = ExpectedNamespace(DiscriminatorCase, OutcomeCase);
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			FString CandidateDeclaration = Function != nullptr
				? UTF8_TO_TCHAR(Function->GetDeclaration())
				: FString();
			const FString NormalizedExpected = Declaration.Replace(TEXT("const "), TEXT(""));
			const FString NormalizedCandidate = CandidateDeclaration.Replace(TEXT("const "), TEXT(""));
			if (Function != nullptr
				&& (Declaration == CandidateDeclaration || NormalizedExpected == NormalizedCandidate)
				&& (Namespace.IsEmpty() || Namespace == UTF8_TO_TCHAR(Function->GetNamespace())))
			{
				return Function;
			}
		}

		const bool bPromotion = IsOutcome(OutcomeCase, "promotion");
		const bool bTwoParameters = IsDiscriminator(DiscriminatorCase, "arity")
			|| IsDiscriminator(DiscriminatorCase, "default");
		const int ExpectedTypeId = Module.GetEngine()->GetTypeIdByDecl(bPromotion ? "double" : "int");
		const int ExpectedParameterCount = bTwoParameters ? 2 : 1;
		const asDWORD ExpectedDirection = IsDiscriminator(DiscriminatorCase, "direction") ? asTM_INREF : asTM_NONE;
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function == nullptr
				|| FCStringAnsi::Strcmp(Function->GetName(), "Probe") != 0
				|| (!Namespace.IsEmpty() && Namespace != UTF8_TO_TCHAR(Function->GetNamespace()))
				|| static_cast<int32>(Function->GetParamCount()) != ExpectedParameterCount)
			{
				continue;
			}
			bool bMatches = true;
			for (int32 ParameterIndex = 0; ParameterIndex < ExpectedParameterCount; ++ParameterIndex)
			{
				int ParameterTypeId = 0;
				asDWORD ParameterFlags = 0;
				bMatches = Function->GetParam(ParameterIndex, &ParameterTypeId, &ParameterFlags) >= 0
					&& ParameterTypeId == ExpectedTypeId
					&& (ParameterIndex != 0 || (ParameterFlags & 3u) == ExpectedDirection);
				if (!bMatches)
				{
					break;
				}
			}
			if (bMatches)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static bool HasResolutionDiagnostic(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		const FString& ModuleName,
		const FOutcomeCase& OutcomeCase)
	{
		return Messages.Entries.ContainsByPredicate([&](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			if (Entry.Type != asMSGTYPE_ERROR
				|| Entry.Section != ModuleName
				|| Entry.Row <= 0
				|| Entry.Column <= 0)
			{
				return false;
			}

			if (IsOutcome(OutcomeCase, "ambiguous") || IsOutcome(OutcomeCase, "missing"))
			{
				return true;
			}
			return Entry.Message.Contains(TEXT("No matching"), ESearchCase::IgnoreCase)
				|| Entry.Message.Contains(TEXT("not declared"), ESearchCase::IgnoreCase)
				|| Entry.Message.Contains(TEXT("Can't resolve"), ESearchCase::IgnoreCase)
				|| Entry.Message.Contains(TEXT("const"), ESearchCase::IgnoreCase);
		});
	}

public:
	TEST_METHOD(DiscriminatorsByOutcome)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-OVERLOAD",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function overload product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FDiscriminatorCase& DiscriminatorCase : DiscriminatorCases)
		{
			for (const FOutcomeCase& OutcomeCase : OutcomeCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-FN-OVERLOAD",
					{ ANSI_TO_TCHAR(DiscriminatorCase.CatalogName), ANSI_TO_TCHAR(OutcomeCase.CatalogName) }));
				const FString Suffix = FString::Printf(TEXT("%hs_%hs"), DiscriminatorCase.CatalogName, OutcomeCase.CatalogName);
				const FString ModuleName = TEXT("FunctionOverload_") + Suffix;
				const FString Source = BuildOverloadSource(DiscriminatorCase, OutcomeCase);
				PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(
					ScriptEngine,
					ModuleNameUtf8.Get(),
					SourceUtf8.Get(),
					Module);

				if (!OutcomeCase.bShouldCompile)
				{
					ASSERT_THAT(IsTrue(BuildResult < 0,
						*Case.Describe(TEXT("ambiguous or missing overload cell should fail compilation"))));
					ASSERT_THAT(IsTrue(HasResolutionDiagnostic(Engine.GetMessages(), ModuleName, OutcomeCase),
						*Case.Describe(TEXT("overload failure should report the expected located resolution category"))));
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("failed overload cell should leave no retained module"))));
					continue;
				}

				ASSERT_THAT(IsTrue(BuildResult >= 0,
					*Case.Describe(TEXT("exact or promoted overload cell should compile"))));
				ASSERT_THAT(IsNotNull(Module,
					*Case.Describe(TEXT("legal overload cell should publish a module"))));
				if (BuildResult >= 0 && Module != nullptr)
				{
					asIScriptFunction* const ExpectedFunction = FindExpectedFunction(*Module, DiscriminatorCase, OutcomeCase);
					ASSERT_THAT(IsNotNull(ExpectedFunction,
						*Case.DescribeResult(
							TCHAR_TO_ANSI(*ExpectedDeclaration(DiscriminatorCase, OutcomeCase)),
							TEXT("published expected overload"),
							TEXT("missing metadata identity"))));
					if (ExpectedFunction != nullptr)
					{
						ASSERT_THAT(AreEqual(FString(TEXT("Probe")), FString(UTF8_TO_TCHAR(ExpectedFunction->GetName())),
							*Case.Describe(TEXT("selected overload metadata should preserve the function name"))));
					}

					AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
						*TestRunner,
						ScriptEngine,
						Module,
						"int Run()");
					ASSERT_THAT(IsTrue(Invoker.IsValid(),
						*Case.Describe(TEXT("legal overload entry should resolve by exact declaration"))));
					if (Invoker.IsValid())
					{
						ASSERT_THAT(AreEqual(ExpectedMarker(DiscriminatorCase, OutcomeCase), Invoker.CallAndReturn<int32>(INDEX_NONE),
							*Case.Describe(TEXT("runtime marker should prove the exact overload selected by the compiler"))));
					}
				}

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("overload cell should discard its module"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
