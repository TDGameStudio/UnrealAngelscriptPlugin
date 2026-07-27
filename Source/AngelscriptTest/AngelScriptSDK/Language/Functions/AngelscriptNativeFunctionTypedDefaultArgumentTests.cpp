#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;
using namespace AngelscriptNativeTestSupport;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionTypedDefaultArgumentTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.TypedDefaultArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

	struct FArityCase
	{
		const ANSICHAR* CatalogName;
		int32 Count;
	};

	struct FTypePatternCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FTargetCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FArityCase ArityCases[] =
	{
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "four", 4 },
	};

	inline static constexpr FTypePatternCase TypePatternCases[] =
	{
		{ "homogeneous_int" },
		{ "homogeneous_bool" },
		{ "alternating_int_bool" },
		{ "alternating_bool_int" },
	};

	inline static constexpr FTargetCase TargetCases[] =
	{
		{ "global" },
		{ "namespace_global" },
		{ "instance_method" },
	};

	static bool IsBoolType(const FTypePatternCase& PatternCase, int32 Index)
	{
		if (FCStringAnsi::Strcmp(PatternCase.CatalogName, "homogeneous_bool") == 0)
		{
			return true;
		}
		if (FCStringAnsi::Strcmp(PatternCase.CatalogName, "alternating_int_bool") == 0)
		{
			return (Index % 2) == 1;
		}
		if (FCStringAnsi::Strcmp(PatternCase.CatalogName, "alternating_bool_int") == 0)
		{
			return (Index % 2) == 0;
		}
		return false;
	}

	static const ANSICHAR* TypeName(const FTypePatternCase& PatternCase, int32 Index)
	{
		return IsBoolType(PatternCase, Index) ? "bool" : "int";
	}

	static const ANSICHAR* DefaultExpression(const FTypePatternCase& PatternCase, int32 Index)
	{
		if (IsBoolType(PatternCase, Index))
		{
			static constexpr const ANSICHAR* BoolDefaults[] =
			{
				"true",
				"false",
				"1 < 2",
				"2 == 3",
			};
			return BoolDefaults[Index];
		}

		static constexpr const ANSICHAR* IntDefaults[] =
		{
			"100",
			"10 + 2",
			"0x10",
			"-7",
		};
		return IntDefaults[Index];
	}

	static FString MetadataDefaultExpression(const FTypePatternCase& PatternCase, int32 Index)
	{
		// The current fork keeps the expression structure but prints unary minus
		// with a separating space in the reflected declaration metadata.
		if (!IsBoolType(PatternCase, Index) && Index == 3)
		{
			return TEXT("- 7");
		}
		return FString(UTF8_TO_TCHAR(DefaultExpression(PatternCase, Index)));
	}

	static int32 DefaultValue(const FTypePatternCase& PatternCase, int32 Index)
	{
		if (IsBoolType(PatternCase, Index))
		{
			return Index == 0 || Index == 2 ? 1 : 0;
		}

		static constexpr int32 IntDefaults[] = { 100, 12, 16, -7 };
		return IntDefaults[Index];
	}

	static const ANSICHAR* ExplicitArgument(const FTypePatternCase& PatternCase, int32 Index)
	{
		if (IsBoolType(PatternCase, Index))
		{
			static constexpr const ANSICHAR* BoolArguments[] =
			{
				"false",
				"true",
				"false",
				"true",
			};
			return BoolArguments[Index];
		}

		static constexpr const ANSICHAR* IntArguments[] = { "1", "2", "3", "4" };
		return IntArguments[Index];
	}

	static int32 ExplicitValue(const FTypePatternCase& PatternCase, int32 Index)
	{
		if (IsBoolType(PatternCase, Index))
		{
			return Index % 2 == 1 ? 1 : 0;
		}
		return Index + 1;
	}

	static FString MakeSuffix(
		const FArityCase& ArityCase,
		const FTypePatternCase& PatternCase,
		const FTargetCase& TargetCase,
		int32 OmittedCount)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs_omit%d"),
			ArityCase.CatalogName,
			PatternCase.CatalogName,
			TargetCase.CatalogName,
			OmittedCount);
	}

	static FString MakeParameters(const FArityCase& ArityCase, const FTypePatternCase& PatternCase)
	{
		FString Parameters;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			if (Index > 0)
			{
				Parameters += TEXT(", ");
			}
			Parameters += FString::Printf(
				TEXT("%hs P%d = %hs"),
				TypeName(PatternCase, Index),
				Index,
				DefaultExpression(PatternCase, Index));
		}
		return Parameters;
	}

	static FString MakeCallArguments(
		const FArityCase& ArityCase,
		const FTypePatternCase& PatternCase,
		int32 OmittedCount)
	{
		FString Arguments;
		const int32 ExplicitCount = ArityCase.Count - OmittedCount;
		for (int32 Index = 0; Index < ExplicitCount; ++Index)
		{
			if (Index > 0)
			{
				Arguments += TEXT(", ");
			}
			Arguments += ANSI_TO_TCHAR(ExplicitArgument(PatternCase, Index));
		}
		return Arguments;
	}

	static FString MakeResultExpression(const FArityCase& ArityCase, const FTypePatternCase& PatternCase)
	{
		FString Result;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(" + ");
			}
			Result += IsBoolType(PatternCase, Index)
				? FString::Printf(TEXT("(P%d ? 1 : 0)"), Index)
				: FString::Printf(TEXT("P%d"), Index);
		}
		return Result;
	}

	static int32 ExpectedResult(
		const FArityCase& ArityCase,
		const FTypePatternCase& PatternCase,
		int32 OmittedCount)
	{
		int32 Result = 0;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Result += Index >= ArityCase.Count - OmittedCount
				? DefaultValue(PatternCase, Index)
				: ExplicitValue(PatternCase, Index);
		}
		return Result;
	}

	static FString BuildSource(
		const FArityCase& ArityCase,
		const FTypePatternCase& PatternCase,
		const FTargetCase& TargetCase,
		int32 OmittedCount)
	{
		const FString Suffix = MakeSuffix(ArityCase, PatternCase, TargetCase, OmittedCount);
		const FString ProbeName = TEXT("Probe_") + Suffix;
		const FString EntryName = TEXT("Run_") + Suffix;
		const FString Parameters = MakeParameters(ArityCase, PatternCase);
		const FString Arguments = MakeCallArguments(ArityCase, PatternCase, OmittedCount);
		const FString ResultExpression = MakeResultExpression(ArityCase, PatternCase);
		FString Source;

		if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "global") == 0)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s(%s)"), *ProbeName, *Parameters));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *ResultExpression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *EntryName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s(%s);"), *ProbeName, *Arguments));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "namespace_global") == 0)
		{
			const FString Namespace = TEXT("N_") + Suffix;
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("namespace %s"), *Namespace));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, *Parameters));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *ResultExpression));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *EntryName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s::%s(%s);"), *Namespace, *ProbeName, *Arguments));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			const FString TypeNameText = TEXT("FOwner_") + Suffix;
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("struct %s"), *TypeNameText));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, *Parameters));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *ResultExpression));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *EntryName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Owner;"), *TypeNameText));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn Owner.%s(%s);"), *ProbeName, *Arguments));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}

		AppendGeneratedAsLine(Source);
		return Source;
	}

	static asIScriptFunction* FindProbe(
		asIScriptModule& Module,
		const FString& Suffix,
		const FTargetCase& TargetCase)
	{
		const FString ProbeName = TEXT("Probe_") + Suffix;
		if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "instance_method") == 0)
		{
			const FString TypeNameText = TEXT("FOwner_") + Suffix;
			asITypeInfo* const Type = Module.GetTypeInfoByName(TCHAR_TO_ANSI(*TypeNameText));
			return Type != nullptr ? Type->GetMethodByName(TCHAR_TO_ANSI(*ProbeName)) : nullptr;
		}

		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function != nullptr && FCStringAnsi::Strcmp(Function->GetName(), TCHAR_TO_ANSI(*ProbeName)) == 0)
			{
				return Function;
			}
		}
		return nullptr;
	}

public:
	TEST_METHOD(TypedDefaultsByArityPatternTargetAndOmission)
	{
		AS_NATIVE_PRODUCT("LANG-FN-TYPED-DEFAULTS",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Typed default-argument product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FArityCase& ArityCase : ArityCases)
		{
			for (const FTypePatternCase& PatternCase : TypePatternCases)
			{
				for (const FTargetCase& TargetCase : TargetCases)
				{
					for (int32 OmittedCount = 0; OmittedCount <= ArityCase.Count; ++OmittedCount)
					{
						const FString OmittedAxis = FString::Printf(TEXT("omit%d"), OmittedCount);
						const FString CaseId = MakeNativeCaseId(
							"LANG-FN-TYPED-DEFAULTS",
							{
								ANSI_TO_TCHAR(ArityCase.CatalogName),
								ANSI_TO_TCHAR(PatternCase.CatalogName),
								ANSI_TO_TCHAR(TargetCase.CatalogName),
								*OmittedAxis,
							});
						const FNativeCaseContext Case(CaseId);
						const FString Suffix = MakeSuffix(ArityCase, PatternCase, TargetCase, OmittedCount);
						const FString ModuleName = TEXT("FunctionTypedDefaults_") + Suffix;
						const FString Source = BuildSource(ArityCase, PatternCase, TargetCase, OmittedCount);
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
						ASSERT_THAT(AreEqual(asSUCCESS, BuildResult,
							*Case.Describe(TEXT("typed default-argument cell should compile"))));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("typed default-argument cell should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							asIScriptFunction* const Probe = FindProbe(*Module, Suffix, TargetCase);
							ASSERT_THAT(IsNotNull(Probe,
								*Case.Describe(TEXT("typed default probe should be published"))));
							if (Probe != nullptr)
							{
								ASSERT_THAT(AreEqual(ArityCase.Count, static_cast<int32>(Probe->GetParamCount()),
									*Case.Describe(TEXT("typed default metadata should preserve every parameter slot"))));
								for (int32 Index = 0; Index < ArityCase.Count; ++Index)
								{
									int TypeId = 0;
									asDWORD Flags = asTM_NONE;
									const char* Name = nullptr;
									const char* DefaultArgument = nullptr;
									ASSERT_THAT(AreEqual(asSUCCESS, Probe->GetParam(Index, &TypeId, &Flags, &Name, &DefaultArgument),
										*Case.Describe(TEXT("typed default parameter metadata query should succeed"))));
									ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl(TypeName(PatternCase, Index)), TypeId,
										*Case.Describe(TEXT("typed default parameter should retain its declared type"))));
									ASSERT_THAT(AreEqual(static_cast<uint32>(asTM_CONST), static_cast<uint32>(Flags),
										*Case.Describe(TEXT("typed default parameter should retain the fork's normalized by-value flag"))));
									ASSERT_THAT(AreEqual(FString::Printf(TEXT("P%d"), Index),
										FString(UTF8_TO_TCHAR(Name != nullptr ? Name : "")),
										*Case.Describe(TEXT("typed default parameter names should preserve declaration order"))));
									const FString ExpectedDefault = MetadataDefaultExpression(PatternCase, Index);
									const FString ActualDefault = FString(UTF8_TO_TCHAR(DefaultArgument != nullptr ? DefaultArgument : ""));
									ASSERT_THAT(IsTrue(ExpectedDefault == ActualDefault,
										*Case.DescribeResult(
											Probe->GetDeclaration(),
											ExpectedDefault,
											ActualDefault)));
								}
							}

							const FString EntryDeclaration = FString::Printf(TEXT("int Run_%s()"), *Suffix);
							AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
								*TestRunner,
								ScriptEngine,
								Module,
								TCHAR_TO_ANSI(*EntryDeclaration));
							ASSERT_THAT(IsTrue(Invoker.IsValid(),
								*Case.Describe(TEXT("typed default entry should resolve by exact declaration"))));
							if (Invoker.IsValid())
							{
								ASSERT_THAT(AreEqual(ExpectedResult(ArityCase, PatternCase, OmittedCount),
									Invoker.CallAndReturn<int32>(INDEX_NONE),
									*Case.Describe(TEXT("typed default invocation should combine explicit and omitted values"))));
							}
						}

						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("typed default cell should discard its module"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
