#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionArityTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.Arity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	struct FArityCase
	{
		const ANSICHAR* CatalogName;
		int32 Count;
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
		{ "three", 3 },
		{ "eight", 8 },
		{ "current_boundary", 64 },
		{ "boundary_plus_one", 65 },
	};

	inline static constexpr FTargetCase TargetCases[] =
	{
		{ "global" },
		{ "namespace_global" },
		{ "instance_method" },
	};

	static FString MakeSuffix(const FArityCase& ArityCase, const FTargetCase& TargetCase)
	{
		return FString::Printf(TEXT("%hs_%hs"), ArityCase.CatalogName, TargetCase.CatalogName);
	}

	static FString MakeParameters(const int32 Count)
	{
		TArray<FString> Parameters;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Parameters.Add(FString::Printf(TEXT("int P%d"), Index));
		}
		return FString::Join(Parameters, TEXT(", "));
	}

	static FString MakeArguments(const int32 Count)
	{
		TArray<FString> Arguments;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Arguments.Add(FString::FromInt(Index + 1));
		}
		return FString::Join(Arguments, TEXT(", "));
	}

	static FString MakeSumExpression(const int32 Count)
	{
		if (Count == 0)
		{
			return TEXT("42");
		}
		TArray<FString> Terms;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Terms.Add(FString::Printf(TEXT("P%d"), Index));
		}
		return FString::Join(Terms, TEXT(" + "));
	}

	static FString BuildAritySource()
	{
		FString Source;
		for (const FArityCase& ArityCase : ArityCases)
		{
			for (const FTargetCase& TargetCase : TargetCases)
			{
				const FString Suffix = MakeSuffix(ArityCase, TargetCase);
				const FString Parameters = MakeParameters(ArityCase.Count);
				const FString Arguments = MakeArguments(ArityCase.Count);
				const FString ProbeName = TEXT("Probe_") + Suffix;
				const FString EntryName = TEXT("Run_") + Suffix;
				if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "global") == 0)
				{
					AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s(%s)"), *ProbeName, *Parameters));
					AppendGeneratedAsLine(Source, TEXT("{"));
					AppendGeneratedAsLine(Source, TEXT("\treturn ") + MakeSumExpression(ArityCase.Count) + TEXT(";"));
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
					AppendGeneratedAsLine(Source, TEXT("\t\treturn ") + MakeSumExpression(ArityCase.Count) + TEXT(";"));
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
					AppendGeneratedAsLine(Source, TEXT("\t\treturn ") + MakeSumExpression(ArityCase.Count) + TEXT(";"));
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
			}
		}
		return Source;
	}

	static asIScriptFunction* FindProbe(
		asIScriptModule& Module,
		const FArityCase& ArityCase,
		const FTargetCase& TargetCase)
	{
		const FString Suffix = MakeSuffix(ArityCase, TargetCase);
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
	TEST_METHOD(AritiesByCallTarget)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-ARITY-TARGET",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function arity product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString GeneratedSource = BuildAritySource();
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("LANG-FN-ARITY-TARGET"),
			TEXT("FunctionArities"),
			GeneratedSource);
		const FTCHARToUTF8 GeneratedSourceUtf8(*GeneratedSource);
		{
			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				"FunctionArities",
				std::string(GeneratedSourceUtf8.Get(), GeneratedSourceUtf8.Length()));
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Function arity product should compile every arity and target")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(GeneratedSource);
				return;
			}

			for (const FArityCase& ArityCase : ArityCases)
			{
				for (const FTargetCase& TargetCase : TargetCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-FN-ARITY-TARGET",
						{ ANSI_TO_TCHAR(ArityCase.CatalogName), ANSI_TO_TCHAR(TargetCase.CatalogName) }));
					asIScriptFunction* const Probe = FindProbe(*Module.Get(), ArityCase, TargetCase);
					ASSERT_THAT(IsNotNull(Probe, *Case.Describe(TEXT("arity probe should be published under its call target"))));
					if (Probe != nullptr)
					{
						ASSERT_THAT(AreEqual(ArityCase.Count, static_cast<int32>(Probe->GetParamCount()),
							*Case.Describe(TEXT("arity metadata should preserve every parameter slot"))));
					}

					const FString EntryDeclaration = FString::Printf(TEXT("int Run_%s()"), *MakeSuffix(ArityCase, TargetCase));
					AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
						*TestRunner,
						ScriptEngine,
						Module,
						TCHAR_TO_ANSI(*EntryDeclaration));
					ASSERT_THAT(IsTrue(Invoker.IsValid(), *Case.Describe(TEXT("arity entry should resolve by exact declaration"))));
					if (Invoker.IsValid())
					{
						const int32 Expected = ArityCase.Count == 0 ? 42 : ArityCase.Count * (ArityCase.Count + 1) / 2;
						ASSERT_THAT(AreEqual(Expected, Invoker.CallAndReturn<int32>(INDEX_NONE),
							*Case.Describe(TEXT("arity call should preserve argument order through the selected target"))));
					}
				}
			}
		}

		Engine.ResetMessages();
		const std::string InvalidSource = ASTEST_AS_ANSI(R"AS(
			int NeedsOne(int Value)
			{
				return Value;
			}

			int WrongArity()
			{
				return NeedsOne();
			}
			)AS");
		asIScriptModule* InvalidModule = nullptr;
		const int InvalidResult = CompileNativeModule(
			ScriptEngine,
			"FunctionArityInvalid",
			InvalidSource.c_str(),
			InvalidModule);
		ASSERT_THAT(IsTrue(InvalidResult < 0,
			TEXT("Function arity product should reject a call with the wrong argument count")));
		ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.ContainsByPredicate([](const FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR && Entry.Row > 0 && Entry.Column > 0;
		}), TEXT("Function arity product should report a located wrong-arity diagnostic")));
		ScriptEngine->DiscardModule("FunctionArityInvalid");
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("FunctionArities", asGM_ONLY_IF_EXISTS),
			TEXT("Function arity product should discard its valid generated module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("FunctionArityInvalid", asGM_ONLY_IF_EXISTS),
			TEXT("Function arity product should discard its invalid module state")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
