#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;
using namespace AngelscriptNativeTestSupport;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionArityTypeStressTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.ArityTypeStress",
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
		{ "zero", 0 },
		{ "one", 1 },
		{ "two", 2 },
		{ "four", 4 },
		{ "eight", 8 },
		{ "sixteen", 16 },
		{ "thirty_two", 32 },
		{ "sixty_four", 64 },
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

	static bool IsBoolParameter(const FTypePatternCase& PatternCase, const int32 Index)
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

	static const ANSICHAR* ParameterType(const FTypePatternCase& PatternCase, const int32 Index)
	{
		return IsBoolParameter(PatternCase, Index) ? "bool" : "int";
	}

	static const ANSICHAR* ArgumentValue(const FTypePatternCase& PatternCase, const int32 Index)
	{
		if (IsBoolParameter(PatternCase, Index))
		{
			return (Index % 2) == 0 ? "true" : "false";
		}
		return nullptr;
	}

	static int32 ArgumentValueAsInt(const FTypePatternCase& PatternCase, const int32 Index)
	{
		if (IsBoolParameter(PatternCase, Index))
		{
			return (Index % 2) == 0 ? 1 : 0;
		}
		return Index + 1;
	}

	static FString MakeSuffix(
		const FArityCase& ArityCase,
		const FTypePatternCase& PatternCase,
		const FTargetCase& TargetCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			ArityCase.CatalogName,
			PatternCase.CatalogName,
			TargetCase.CatalogName);
	}

	static FString MakeParameters(const FArityCase& ArityCase, const FTypePatternCase& PatternCase)
	{
		TArray<FString> Parameters;
		Parameters.Reserve(ArityCase.Count);
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Parameters.Add(FString::Printf(
				TEXT("%hs P%d"),
				ParameterType(PatternCase, Index),
				Index));
		}
		return FString::Join(Parameters, TEXT(", "));
	}

	static FString MakeArguments(const FArityCase& ArityCase, const FTypePatternCase& PatternCase)
	{
		TArray<FString> Arguments;
		Arguments.Reserve(ArityCase.Count);
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			const ANSICHAR* BoolArgument = ArgumentValue(PatternCase, Index);
			Arguments.Add(BoolArgument != nullptr
				? ANSI_TO_TCHAR(BoolArgument)
				: FString::FromInt(Index + 1));
		}
		return FString::Join(Arguments, TEXT(", "));
	}

	static FString MakeResultExpression(const FArityCase& ArityCase, const FTypePatternCase& PatternCase)
	{
		if (ArityCase.Count == 0)
		{
			return TEXT("42");
		}

		TArray<FString> Terms;
		Terms.Reserve(ArityCase.Count);
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Terms.Add(IsBoolParameter(PatternCase, Index)
				? FString::Printf(TEXT("(P%d ? 1 : 0)"), Index)
				: FString::Printf(TEXT("P%d"), Index));
		}
		return FString::Join(Terms, TEXT(" + "));
	}

	static int32 ExpectedResult(const FArityCase& ArityCase, const FTypePatternCase& PatternCase)
	{
		if (ArityCase.Count == 0)
		{
			return 42;
		}

		int32 Result = 0;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Result += ArgumentValueAsInt(PatternCase, Index);
		}
		return Result;
	}

	static FString BuildSource(
		const FArityCase& ArityCase,
		const FTypePatternCase& PatternCase,
		const FTargetCase& TargetCase)
	{
		const FString Suffix = MakeSuffix(ArityCase, PatternCase, TargetCase);
		const FString ProbeName = TEXT("Probe_") + Suffix;
		const FString EntryName = TEXT("Run_") + Suffix;
		const FString Parameters = MakeParameters(ArityCase, PatternCase);
		const FString Arguments = MakeArguments(ArityCase, PatternCase);
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
			const FString TypeName = TEXT("FOwner_") + Suffix;
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("struct %s"), *TypeName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, *Parameters));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *ResultExpression));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *EntryName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Owner;"), *TypeName));
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
			const FString TypeName = TEXT("FOwner_") + Suffix;
			asITypeInfo* const Type = Module.GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName));
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
	TEST_METHOD(AritiesByTypePatternAndCallTarget)
	{
		AS_NATIVE_PRODUCT("LANG-FN-ARITY-TYPE-STRESS",
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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function arity/type stress product should create a raw SDK engine")));
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
					const FString Suffix = MakeSuffix(ArityCase, PatternCase, TargetCase);
					const FString CaseId = MakeNativeCaseId(
						"LANG-FN-ARITY-TYPE-STRESS",
						{
							ANSI_TO_TCHAR(ArityCase.CatalogName),
							ANSI_TO_TCHAR(PatternCase.CatalogName),
							ANSI_TO_TCHAR(TargetCase.CatalogName),
						});
					const FNativeCaseContext Case(CaseId);
					const FString ModuleName = TEXT("FunctionArityTypeStress_") + Suffix;
					const FString Source = BuildSource(ArityCase, PatternCase, TargetCase);
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
						*Case.Describe(TEXT("arity/type stress cell should compile"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("arity/type stress cell should publish a module"))));
					if (BuildResult >= 0 && Module != nullptr)
					{
						asIScriptFunction* const Probe = FindProbe(*Module, Suffix, TargetCase);
						ASSERT_THAT(IsNotNull(Probe,
							*Case.Describe(TEXT("arity/type stress probe should be published"))));
						if (Probe != nullptr)
						{
							ASSERT_THAT(AreEqual(ArityCase.Count, static_cast<int32>(Probe->GetParamCount()),
								*Case.Describe(TEXT("arity/type stress metadata should preserve parameter count"))));
							for (int32 Index = 0; Index < ArityCase.Count; ++Index)
							{
								int TypeId = asTYPEID_VOID;
								asDWORD Flags = asTM_NONE;
								const char* Name = nullptr;
								ASSERT_THAT(AreEqual(asSUCCESS, Probe->GetParam(Index, &TypeId, &Flags, &Name),
									*Case.Describe(TEXT("arity/type stress parameter metadata query should succeed"))));
								ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl(ParameterType(PatternCase, Index)), TypeId,
									*Case.Describe(TEXT("arity/type stress metadata should preserve each scalar type"))));
								ASSERT_THAT(AreEqual(static_cast<uint32>(asTM_CONST), static_cast<uint32>(Flags),
									*Case.Describe(TEXT("arity/type stress value parameter should retain the fork-normalized flag"))));
								ASSERT_THAT(AreEqual(FString::Printf(TEXT("P%d"), Index),
									FString(UTF8_TO_TCHAR(Name != nullptr ? Name : "")),
									*Case.Describe(TEXT("arity/type stress names should preserve parameter order"))));
							}
						}

						const FString EntryDeclaration = FString::Printf(TEXT("int Run_%s()"), *Suffix);
						AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
							*TestRunner,
							ScriptEngine,
							Module,
							TCHAR_TO_ANSI(*EntryDeclaration));
						ASSERT_THAT(IsTrue(Invoker.IsValid(),
							*Case.Describe(TEXT("arity/type stress entry should resolve by exact declaration"))));
						if (Invoker.IsValid())
						{
							ASSERT_THAT(AreEqual(ExpectedResult(ArityCase, PatternCase),
								Invoker.CallAndReturn<int32>(INDEX_NONE),
								*Case.Describe(TEXT("arity/type stress call should preserve all argument slots"))));
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("arity/type stress cell should discard its module"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
