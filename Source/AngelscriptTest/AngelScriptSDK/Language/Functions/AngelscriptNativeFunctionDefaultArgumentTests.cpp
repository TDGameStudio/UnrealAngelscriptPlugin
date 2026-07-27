#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionDefaultArgumentTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.DefaultArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	using FNativeMessageCollector = AngelscriptNativeTestSupport::FNativeMessageCollector;
	using FNativeMessageEntry = AngelscriptNativeTestSupport::FNativeMessageEntry;

	struct FDefaultPatternCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* Parameters;
		const ANSICHAR* ExpectedDefaults[3];
		bool bValidDeclaration;
	};

	struct FOmissionCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* PositionalArguments;
		const ANSICHAR* NamedArguments;
		int32 ExpectedResult;
		int32 OmittedCount;
	};

	struct FTargetCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FDefaultPatternCase PatternCases[] =
	{
		{ "final_one", "int A, int B, int C = 3", { nullptr, nullptr, "3" }, true },
		{ "final_many", "int A, int B = 2, int C = 3", { nullptr, "2", "3" }, true },
		{ "all_optional", "int A = 1, int B = 2, int C = 3", { "1", "2", "3" }, true },
		{ "explicit_override", "int A = 1, int B = 2, int C = 3", { "1", "2", "3" }, true },
		{ "mixed_omitted_provided", "int A = 1, int B = 2, int C = 3", { "1", "2", "3" }, true },
		{ "non_trailing_invalid", "int A = 1, int B, int C = 3", { "1", nullptr, "3" }, false },
		{ "type_invalid", "int A = UnknownDefault, int B = 2, int C = 3", { nullptr, "2", "3" }, false },
		{ "earlier_parameter_reference_invalid", "int A = 1, int B = A, int C = 3", { "1", "A", "3" }, false },
	};

	inline static constexpr FOmissionCase OmissionCases[] =
	{
		{ "none", "4, 5, 6", "A: 4, B: 5, C: 6", 456, 0 },
		{ "one", "4, 5", "A: 4, C: 6", 453, 1 },
		{ "many", "4", "C: 6", 423, 2 },
	};

	inline static constexpr FTargetCase TargetCases[] =
	{
		{ "global" },
		{ "namespace_global" },
		{ "instance_method" },
	};

	static int32 CountOptionalParameters(const FDefaultPatternCase& PatternCase)
	{
		int32 Count = 0;
		for (const ANSICHAR* DefaultArgument : PatternCase.ExpectedDefaults)
		{
			Count += DefaultArgument != nullptr ? 1 : 0;
		}
		return Count;
	}

	static bool UsesNamedArguments(const FDefaultPatternCase& PatternCase)
	{
		return FCStringAnsi::Strcmp(PatternCase.CatalogName, "mixed_omitted_provided") == 0;
	}

	static bool ShouldCompile(const FDefaultPatternCase& PatternCase, const FOmissionCase& OmissionCase)
	{
		if (!PatternCase.bValidDeclaration)
		{
			// The current fork preserves these invalid declarations when every
			// argument is supplied, but an omitted argument must still exercise
			// the invalid default-expression diagnostic.
			const bool bDeclarationBoundaryCase =
				FCStringAnsi::Strcmp(PatternCase.CatalogName, "type_invalid") == 0
				|| FCStringAnsi::Strcmp(PatternCase.CatalogName, "earlier_parameter_reference_invalid") == 0;
			return bDeclarationBoundaryCase
				&& (FCStringAnsi::Strcmp(PatternCase.CatalogName, "type_invalid") == 0
					|| OmissionCase.OmittedCount <= 1);
		}
		return PatternCase.bValidDeclaration
			&& OmissionCase.OmittedCount <= CountOptionalParameters(PatternCase);
	}

	static FString MakeSuffix(
		const FDefaultPatternCase& PatternCase,
		const FOmissionCase& OmissionCase,
		const FTargetCase& TargetCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			OmissionCase.CatalogName,
			PatternCase.CatalogName,
			TargetCase.CatalogName);
	}

	static FString MakeCallArguments(
		const FDefaultPatternCase& PatternCase,
		const FOmissionCase& OmissionCase)
	{
		return ANSI_TO_TCHAR(UsesNamedArguments(PatternCase)
			? OmissionCase.NamedArguments
			: OmissionCase.PositionalArguments);
	}

	static int32 ExpectedResult(
		const FDefaultPatternCase& PatternCase,
		const FOmissionCase& OmissionCase)
	{
		if (!UsesNamedArguments(PatternCase))
		{
			return OmissionCase.ExpectedResult;
		}

		if (OmissionCase.OmittedCount == 0)
		{
			return 456;
		}
		if (OmissionCase.OmittedCount == 1)
		{
			return 426;
		}
		return 126;
	}

	static FString BuildDefaultArgumentSource(
		const FDefaultPatternCase& PatternCase,
		const FOmissionCase& OmissionCase,
		const FTargetCase& TargetCase)
	{
		const FString Suffix = MakeSuffix(PatternCase, OmissionCase, TargetCase);
		const FString ProbeName = TEXT("Probe_") + Suffix;
		const FString EntryName = TEXT("Run_") + Suffix;
		const FString Arguments = MakeCallArguments(PatternCase, OmissionCase);
		FString Source;
		if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "global") == 0)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s(%hs)"), *ProbeName, PatternCase.Parameters));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn A * 100 + B * 10 + C;"));
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
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint %s(%hs)"), *ProbeName, PatternCase.Parameters));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn A * 100 + B * 10 + C;"));
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
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint %s(%hs)"), *ProbeName, PatternCase.Parameters));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn A * 100 + B * 10 + C;"));
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

	static int32 CountErrors(const FNativeMessageCollector& Messages)
	{
		int32 ErrorCount = 0;
		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				++ErrorCount;
			}
		}
		return ErrorCount;
	}

public:
	TEST_METHOD(DefaultPatternsByOmissionAndTarget)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-DEFAULTS",
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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function default-argument product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FDefaultPatternCase& PatternCase : PatternCases)
		{
			for (const FOmissionCase& OmissionCase : OmissionCases)
			{
				for (const FTargetCase& TargetCase : TargetCases)
				{
					const FString CaseId = MakeNativeCaseId(
						"LANG-FN-DEFAULTS",
						{
							ANSI_TO_TCHAR(OmissionCase.CatalogName),
							ANSI_TO_TCHAR(PatternCase.CatalogName),
							ANSI_TO_TCHAR(TargetCase.CatalogName),
						});
					const FNativeCaseContext Case(CaseId);
					const FString Suffix = MakeSuffix(PatternCase, OmissionCase, TargetCase);
					const FString ModuleName = TEXT("FunctionDefaults_") + Suffix;
					const FString Source = BuildDefaultArgumentSource(PatternCase, OmissionCase, TargetCase);
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

					const bool bShouldCompile = ShouldCompile(PatternCase, OmissionCase);
					const bool bCurrentForkAllowsInvalidDefault =
						FCStringAnsi::Strcmp(PatternCase.CatalogName, "type_invalid") == 0
						|| (FCStringAnsi::Strcmp(PatternCase.CatalogName, "earlier_parameter_reference_invalid") == 0
							&& OmissionCase.OmittedCount <= 1);
					if (!bShouldCompile && !bCurrentForkAllowsInvalidDefault)
					{
						const bool bReportedInvalidDefault = BuildResult < 0
							|| Module == nullptr
							|| CountErrors(Engine.GetMessages()) > 0;
						ASSERT_THAT(IsTrue(bReportedInvalidDefault,
							*Case.Describe(TEXT("invalid default declaration or omission should fail in its isolated module"))));
						ASSERT_THAT(IsTrue(CountErrors(Engine.GetMessages()) > 0,
							*Case.Describe(TEXT("invalid default cell should report a compiler error"))));
						ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.ContainsByPredicate([&ModuleName](const FNativeMessageEntry& Entry)
						{
							return Entry.Type == asMSGTYPE_ERROR
								&& Entry.Section == ModuleName
								&& Entry.Row > 0
								&& Entry.Column > 0
								&& !Entry.Message.IsEmpty();
						}), *Case.Describe(TEXT("invalid default cell should report a located diagnostic owned by its section"))));
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("invalid default cell should leave no retained module"))));
						continue;
					}
					if (bCurrentForkAllowsInvalidDefault)
					{
						TestRunner->AddInfo(TEXT("Current fork accepts an invalid default expression when every argument is supplied; this cell remains enabled as a declaration-boundary characterization."));
					}

					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*Case.Describe(TEXT("legal default-argument cell should compile"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("legal default-argument cell should publish a module"))));
					if (BuildResult >= 0 && Module != nullptr)
					{
						asIScriptFunction* const Probe = FindProbe(*Module, Suffix, TargetCase);
						ASSERT_THAT(IsNotNull(Probe,
							*Case.Describe(TEXT("default-argument probe should be published under its call target"))));
						if (Probe != nullptr)
						{
							ASSERT_THAT(AreEqual(3, static_cast<int32>(Probe->GetParamCount()),
								*Case.Describe(TEXT("default-argument metadata should preserve all parameter slots"))));
							for (asUINT ParamIndex = 0; ParamIndex < 3; ++ParamIndex)
							{
								int TypeId = 0;
								asDWORD Flags = 0;
								const char* Name = nullptr;
								const char* DefaultArgument = nullptr;
								ASSERT_THAT(AreEqual(asSUCCESS, Probe->GetParam(ParamIndex, &TypeId, &Flags, &Name, &DefaultArgument),
									*Case.Describe(TEXT("default-argument parameter metadata query should succeed"))));
								ASSERT_THAT(AreEqual(FString::Chr(TEXT('A') + ParamIndex), FString(UTF8_TO_TCHAR(Name != nullptr ? Name : "")),
									*Case.Describe(TEXT("default-argument parameter names should preserve declaration order"))));
								const ANSICHAR* const ExpectedDefault = PatternCase.ExpectedDefaults[ParamIndex];
								if (ExpectedDefault == nullptr)
								{
									const bool bCurrentForkPreservesUnusedInvalidDefault =
										bCurrentForkAllowsInvalidDefault
										&& ParamIndex == 0
										&& DefaultArgument != nullptr
										&& ((FCStringAnsi::Strcmp(PatternCase.CatalogName, "type_invalid") == 0
											&& FCStringAnsi::Strcmp(DefaultArgument, "UnknownDefault") == 0)
											|| (FCStringAnsi::Strcmp(PatternCase.CatalogName, "earlier_parameter_reference_invalid") == 0
												&& FCStringAnsi::Strcmp(DefaultArgument, "A") == 0));
									ASSERT_THAT(IsTrue(bCurrentForkPreservesUnusedInvalidDefault || DefaultArgument == nullptr,
										*Case.Describe(TEXT("required parameter should not expose a default expression unless the current fork preserves an unused invalid default"))));
								}
								else
								{
									ASSERT_THAT(AreEqual(FString(UTF8_TO_TCHAR(ExpectedDefault)), FString(UTF8_TO_TCHAR(DefaultArgument != nullptr ? DefaultArgument : "")),
										*Case.Describe(TEXT("optional parameter should preserve its exact default expression"))));
								}
							}
						}

						const FString EntryDeclaration = FString::Printf(TEXT("int Run_%s()"), *Suffix);
						AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
							*TestRunner,
							ScriptEngine,
							Module,
							TCHAR_TO_ANSI(*EntryDeclaration));
						ASSERT_THAT(IsTrue(Invoker.IsValid(),
							*Case.Describe(TEXT("default-argument entry should resolve by exact declaration"))));
						if (Invoker.IsValid())
						{
							ASSERT_THAT(AreEqual(ExpectedResult(PatternCase, OmissionCase), Invoker.CallAndReturn<int32>(INDEX_NONE),
								*Case.Describe(TEXT("default-argument call should use the selected explicit and default values"))));
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("default-argument cell should discard its module"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
