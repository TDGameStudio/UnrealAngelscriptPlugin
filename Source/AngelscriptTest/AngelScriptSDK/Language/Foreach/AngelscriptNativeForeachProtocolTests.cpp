#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FForeachProtocolTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Foreach.Protocol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FForeachProtocolExpectation
	{
		int32 ReturnValue = 0;
		int32 IteratorBegins = 0;
		int32 IteratorValues = 0;
		bool bThrows = false;
	};

	inline static constexpr FNamedCase ProtocolCases[] =
	{
		{ "complete" }, { "overloaded" }, { "missing_begin" }, { "missing_next" }, { "missing_value" }, { "missing_end" }, { "wrong_parameter" }, { "wrong_return" }, { "inaccessible" }, { "throwing" },
	};
	inline static constexpr FNamedCase ResolutionCases[] =
	{
		{ "exact" }, { "conversion" }, { "const_overload" }, { "ambiguous" }, { "missing" },
	};
	inline static constexpr FNamedCase NestingCases[] =
	{
		{ "single" }, { "same_iterable" }, { "distinct_iterable" }, { "inside_for" }, { "contains_for" },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static bool HasCompleteProtocol(const FNamedCase& ProtocolCase)
	{
		return IsCase(ProtocolCase, "complete") || IsCase(ProtocolCase, "overloaded") || IsCase(ProtocolCase, "throwing");
	}

	static bool HasResolvedValue(const FNamedCase& ResolutionCase)
	{
		// The current fork keeps the first equal-rank candidate for this
		// protocol lookup.  The upstream ambiguous rejection remains a
		// selected-2.38 conformance boundary and is characterized separately
		// by the deterministic int64 candidate marker below.
		return IsCase(ResolutionCase, "exact") || IsCase(ResolutionCase, "conversion") || IsCase(ResolutionCase, "const_overload") || IsCase(ResolutionCase, "ambiguous");
	}

	static bool IsValidProtocol(const FNamedCase& ProtocolCase, const FNamedCase& ResolutionCase)
	{
		return HasCompleteProtocol(ProtocolCase) && HasResolvedValue(ResolutionCase);
	}

	static int32 ResolutionContribution(const FNamedCase& ResolutionCase)
	{
		if (IsCase(ResolutionCase, "conversion"))
		{
			return 2;
		}
		if (IsCase(ResolutionCase, "const_overload"))
		{
			return 3;
		}
		if (IsCase(ResolutionCase, "ambiguous"))
		{
			return 20;
		}
		return 1;
	}

	static FString ForeachVariableDeclaration(const FNamedCase& ResolutionCase, const TCHAR* VariableName)
	{
		if (IsCase(ResolutionCase, "conversion"))
		{
			return FString::Printf(TEXT("int64 %s"), VariableName);
		}
		if (IsCase(ResolutionCase, "const_overload"))
		{
			return FString::Printf(TEXT("const int %s"), VariableName);
		}
		return FString::Printf(TEXT("int %s"), VariableName);
	}

	static void AppendBrokenProtocol(FString& Source, const FNamedCase& ProtocolCase)
	{
		AppendGeneratedAsLine(Source, TEXT("struct FBrokenProtocol"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseRange Native;"));
		if (IsCase(ProtocolCase, "inaccessible"))
		{
			AppendGeneratedAsLine(Source, TEXT("private:"));
		}
		if (!IsCase(ProtocolCase, "missing_begin"))
		{
			if (IsCase(ProtocolCase, "wrong_return"))
			{
				AppendGeneratedAsLine(Source, TEXT("\tvoid opForBegin()"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForBegin();"));
				AppendGeneratedAsLine(Source, TEXT("\t}"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tint opForBegin()"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendGeneratedAsLine(Source, TEXT("\t\treturn Native.opForBegin();"));
				AppendGeneratedAsLine(Source, TEXT("\t}"));
			}
			AppendGeneratedAsLine(Source);
		}
		if (!IsCase(ProtocolCase, "missing_end"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tbool opForEnd(const int Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Iterator < 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
		}
		if (!IsCase(ProtocolCase, "missing_next"))
		{
			if (IsCase(ProtocolCase, "wrong_parameter"))
			{
				AppendGeneratedAsLine(Source, TEXT("\tvoid opForNext(int64& inout Iterator)"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tvoid opForNext(int& inout Iterator)"));
			}
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForNext(Iterator);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
		}
		if (!IsCase(ProtocolCase, "missing_value"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint opForValue(const int Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForValue(Iterator);"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendProtocolValueCallbacks(
		FString& Source,
		const FNamedCase& ProtocolCase,
		const FNamedCase& ResolutionCase)
	{
		if (IsCase(ResolutionCase, "ambiguous"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint opForValue(int64 Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForValue(int(Iterator));"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 20;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opForValue(uint64 Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForValue(int(Iterator));"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 21;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			return;
		}
		if (IsCase(ResolutionCase, "missing"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint opForValue(const int Iterator, const int Unused)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForValue(Iterator);"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Unused;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			return;
		}

		const bool bConversion = IsCase(ResolutionCase, "conversion");
		AppendGeneratedAsLine(Source, bConversion
			? TEXT("\tint8 opForValue(const int Iterator)")
			: TEXT("\tint opForValue(const int Iterator)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForValue(Iterator);"));
		if (IsCase(ProtocolCase, "throwing") && !IsCase(ResolutionCase, "ambiguous"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tRaiseNativeCaseException();"));
			AppendGeneratedAsLine(Source, bConversion ? TEXT("\t\treturn int8(0);") : TEXT("\t\treturn 0;"));
		}
		else if (bConversion)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn int8(2);"));
		}
		else if (IsCase(ResolutionCase, "const_overload"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 3;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		if (IsCase(ProtocolCase, "overloaded") || IsCase(ResolutionCase, "const_overload"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opForValue(int8 Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 100 + int(Iterator);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
	}

	static void AppendCompleteProtocol(FString& Source, const FNamedCase& ProtocolCase, const FNamedCase& ResolutionCase)
	{
		AppendGeneratedAsLine(Source, TEXT("struct FForeachProtocolRange"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseRange Native;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint opForBegin()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Native.opForBegin();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		if (IsCase(ProtocolCase, "overloaded"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opForBegin(const int Ignored)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 100 + Ignored;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tbool opForEnd(const int Iterator)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Iterator < 0;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tvoid opForNext(int& inout Iterator)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForNext(Iterator);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendProtocolValueCallbacks(Source, ProtocolCase, ResolutionCase);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendLoopBody(
		FString& Source,
		const FString& RangeName,
		const FString& Indent,
		const FNamedCase& ResolutionCase,
		const bool bContainsFor)
	{
		AppendGeneratedAsLine(Source, Indent + TEXT("foreach (") + ForeachVariableDeclaration(ResolutionCase, TEXT("Value")) + TEXT(" : ") + RangeName + TEXT(")"));
		AppendGeneratedAsLine(Source, Indent + TEXT("{"));
		AppendGeneratedAsLine(Source, Indent + TEXT("\tTrace += Value;"));
		if (bContainsFor)
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("\tfor (int Index = 0; Index < 2; ++Index)"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\t{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\t\tTrace += Value;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, Indent + TEXT("}"));
	}

	static FString BuildForeachProtocolSource(const FNamedCase& ProtocolCase, const FNamedCase& ResolutionCase, const FNamedCase& NestingCase)
	{
		const bool bProtocolSurfaceValid = HasCompleteProtocol(ProtocolCase);
		FString Source;
		if (bProtocolSurfaceValid)
		{
			AppendCompleteProtocol(Source, ProtocolCase, ResolutionCase);
		}
		else
		{
			AppendBrokenProtocol(Source, ProtocolCase);
		}
		const FString RangeType = bProtocolSurfaceValid ? TEXT("FForeachProtocolRange") : TEXT("FBrokenProtocol");
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\t") + RangeType + TEXT(" Range;"));
		AppendGeneratedAsLine(Source, TEXT("\tRange.Native.Count = 2;"));
		if (IsCase(NestingCase, "distinct_iterable"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t") + RangeType + TEXT(" OtherRange;"));
			AppendGeneratedAsLine(Source, TEXT("\tOtherRange.Native.Count = 2;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		if (IsCase(NestingCase, "same_iterable"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tforeach (") + ForeachVariableDeclaration(ResolutionCase, TEXT("OuterValue")) + TEXT(" : Range)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += OuterValue;"));
			AppendLoopBody(Source, TEXT("Range"), TEXT("\t\t"), ResolutionCase, false);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsCase(NestingCase, "distinct_iterable"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tforeach (") + ForeachVariableDeclaration(ResolutionCase, TEXT("OuterValue")) + TEXT(" : Range)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += OuterValue;"));
			AppendLoopBody(Source, TEXT("OtherRange"), TEXT("\t\t"), ResolutionCase, false);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsCase(NestingCase, "inside_for"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendLoopBody(Source, TEXT("Range"), TEXT("\t\t"), ResolutionCase, false);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendLoopBody(Source, TEXT("Range"), TEXT("\t"), ResolutionCase, IsCase(NestingCase, "contains_for"));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FForeachProtocolExpectation ExpectedResult(
		const FNamedCase& ProtocolCase,
		const FNamedCase& ResolutionCase,
		const FNamedCase& NestingCase)
	{
		FForeachProtocolExpectation Result;
		const int32 Contribution = ResolutionContribution(ResolutionCase);
		if (IsCase(ProtocolCase, "throwing") && !IsCase(ResolutionCase, "ambiguous"))
		{
			Result.bThrows = true;
			Result.IteratorBegins = 1;
			Result.IteratorValues = 1;
			return Result;
		}
		if (IsCase(NestingCase, "single"))
		{
			Result.ReturnValue = 2 * Contribution;
			Result.IteratorBegins = 1;
			Result.IteratorValues = 2;
		}
		else if (IsCase(NestingCase, "inside_for"))
		{
			Result.ReturnValue = 4 * Contribution;
			Result.IteratorBegins = 2;
			Result.IteratorValues = 4;
		}
		else if (IsCase(NestingCase, "contains_for"))
		{
			Result.ReturnValue = 6 * Contribution;
			Result.IteratorBegins = 1;
			Result.IteratorValues = 2;
		}
		else
		{
			Result.ReturnValue = 6 * Contribution;
			Result.IteratorBegins = 3;
			Result.IteratorValues = 6;
		}
		return Result;
	}

public:
	TEST_METHOD(ProtocolsByResolutionAndNesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FE-PROTOCOL",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FNoDiscardAsserter Assertions(*TestRunner);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Foreach protocol product should create a raw SDK engine")))
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		if (!Assertions.IsTrue(RegisterNativeCaseRange(*ScriptEngine, Lifecycle), TEXT("Foreach protocol should register its raw protocol fixture")))
		{
			return;
		}

		for (const FNamedCase& ProtocolCase : ProtocolCases)
		{
			for (const FNamedCase& ResolutionCase : ResolutionCases)
			{
				for (const FNamedCase& NestingCase : NestingCases)
				{
					Lifecycle.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-FE-PROTOCOL",
						{ ANSI_TO_TCHAR(ProtocolCase.CatalogName), ANSI_TO_TCHAR(ResolutionCase.CatalogName), ANSI_TO_TCHAR(NestingCase.CatalogName) }));
					const FString ModuleName = TEXT("ForeachProtocol_") + Case.GetId().RightChop(17).Replace(TEXT("-"), TEXT("_"));
					const FString Source = BuildForeachProtocolSource(ProtocolCase, ResolutionCase, NestingCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					if (!IsValidProtocol(ProtocolCase, ResolutionCase))
					{
						const FString RejectionDescription = FString::Printf(TEXT("%s; result=%d messages=%s"),
							*Case.Describe(TEXT("incomplete or unresolved foreach protocol should be rejected")), BuildResult, *Engine.GetMessagesText());
						(void)Assertions.IsTrue(BuildResult < 0, *RejectionDescription);
						(void)Assertions.IsTrue(Engine.GetMessages().Entries.Num() > 0, *Case.Describe(TEXT("rejected foreach protocol should report one or more diagnostics")));
					}
					else
					{
						const FForeachProtocolExpectation Expected = ExpectedResult(ProtocolCase, ResolutionCase, NestingCase);
						const FString BuildDescription = FString::Printf(TEXT("%s; result=%d messages=%s"),
							*Case.Describe(TEXT("resolved foreach protocol should compile")), BuildResult, *Engine.GetMessagesText());
						const bool bBuildSucceeded = Assertions.AreEqual(asSUCCESS, BuildResult, *BuildDescription);
						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
						const bool bEntryAvailable = Assertions.IsNotNull(Entry, *Case.Describe(TEXT("foreach protocol should publish exact Entry declaration")));
						if (bBuildSucceeded && bEntryAvailable)
						{
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							const bool bContextAvailable = Assertions.IsNotNull(Context, *Case.Describe(TEXT("foreach protocol should create a context")));
							if (bContextAvailable)
							{
								const int ExecutionResult = PrepareAndExecute(Context, Entry);
								const FString ExecutionDescription = FString::Printf(TEXT("%s; actual=%d return=%d exception=%s"),
									*Case.Describe(TEXT("resolved foreach protocol should execute")), ExecutionResult,
									static_cast<int32>(Context->GetReturnDWord()), UTF8_TO_TCHAR(Context->GetExceptionString()));
								(void)Assertions.AreEqual(Expected.bThrows ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED, ExecutionResult, *ExecutionDescription);
								if (Expected.bThrows)
								{
					(void)Assertions.AreEqual(FString(TEXT("foreach callback exception")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
										*Case.Describe(TEXT("throwing protocol callback should preserve its exact exception text")));
								}
								else
								{
									(void)Assertions.AreEqual(Expected.ReturnValue, static_cast<int32>(Context->GetReturnDWord()),
										*Case.Describe(TEXT("resolved foreach protocol should return its exact selected trace")));
								}
								Context->Release();
							}
						}
						(void)Assertions.AreEqual(Expected.IteratorBegins, Lifecycle.Num(ENativeLifecycleEvent::IteratorBegin),
							*Case.Describe(TEXT("foreach nesting should invoke the exact expected begin callback count")));
						(void)Assertions.AreEqual(Expected.IteratorValues, Lifecycle.Num(ENativeLifecycleEvent::IteratorValue),
							*Case.Describe(TEXT("foreach nesting should visit the exact expected native element count")));
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("foreach protocol cell should discard its module")));
					(void)Assertions.AreEqual(0, Lifecycle.GetLiveObjectCount(), *Case.Describe(TEXT("foreach protocol cell should leave no tracked native owner alive")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
