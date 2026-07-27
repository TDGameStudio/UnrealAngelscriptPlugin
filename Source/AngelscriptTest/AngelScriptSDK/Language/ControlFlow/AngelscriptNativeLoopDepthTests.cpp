#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FLoopDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.LoopDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FLoopCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FCountCase
	{
		const ANSICHAR* CatalogName;
		int32 Limit;
	};

	struct FTransferCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FLoopExpectation
	{
		int32 BodyCount = 0;
		int32 ConditionCount = 0;
		int32 IncrementCount = 0;
	};

	inline static constexpr FLoopCase LoopCases[] =
	{
		{ "while" },
		{ "do_while" },
		{ "for" },
	};

	inline static constexpr FCountCase CountCases[] =
	{
		{ "zero", 0 },
		{ "one", 1 },
		{ "two", 2 },
		{ "many", 4 },
	};

	inline static constexpr FTransferCase TransferCases[] =
	{
		{ "none" },
		{ "break" },
		{ "continue" },
		{ "return" },
	};


	static bool IsCase(const ANSICHAR* Actual, const ANSICHAR* Expected)
	{
		return FCStringAnsi::Strcmp(Actual, Expected) == 0;
	}

	static void AppendLoopBody(FString& Source, const FTransferCase& TransferCase)
	{
		AppendGeneratedAsLine(Source, TEXT("\t\t++BodyCount;"));
		if (IsCase(TransferCase.CatalogName, "break"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		}
		else if (IsCase(TransferCase.CatalogName, "return"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tCountIncrement(Index, IncrementCount);"));
			if (IsCase(TransferCase.CatalogName, "continue"))
			{
				AppendGeneratedAsLine(Source, TEXT("\t\tcontinue;"));
			}
		}
	}

	static FString BuildLoopSource(
		const FLoopCase& LoopCase,
		const FCountCase& CountCase,
		const FTransferCase& TransferCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("bool CountCondition(int Index, int Limit, int&inout Counter)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\t++Counter;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Index < Limit;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CountIncrement(int&inout Index, int&inout Counter)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\t++Counter;"));
		AppendGeneratedAsLine(Source, TEXT("\t++Index;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Index;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), CountCase.Limit));
		AppendGeneratedAsLine(Source, TEXT("\tint Index = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint BodyCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ConditionCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint IncrementCount = 0;"));

		if (IsCase(LoopCase.CatalogName, "while"))
		{
			AppendGeneratedAsLine(Source, TEXT("\twhile (CountCondition(Index, Limit, ConditionCount))"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendLoopBody(Source, TransferCase);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsCase(LoopCase.CatalogName, "do_while"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tdo"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Index >= Limit)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendLoopBody(Source, TransferCase);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\twhile (CountCondition(Index, Limit, ConditionCount));"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (Index = 0; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t++BodyCount;"));
			if (IsCase(TransferCase.CatalogName, "break"))
			{
				AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
			}
			else if (IsCase(TransferCase.CatalogName, "return"))
			{
				AppendGeneratedAsLine(Source, TEXT("\t\treturn BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
			}
			else if (IsCase(TransferCase.CatalogName, "continue"))
			{
				AppendGeneratedAsLine(Source, TEXT("\t\tcontinue;"));
			}
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}

		AppendGeneratedAsLine(Source, TEXT("\treturn BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FLoopExpectation GetExpected(
		const FLoopCase& LoopCase,
		const FCountCase& CountCase,
		const FTransferCase& TransferCase)
	{
		FLoopExpectation Expected;
		if (CountCase.Limit == 0)
		{
			Expected.ConditionCount = IsCase(LoopCase.CatalogName, "do_while") ? 0 : 1;
			return Expected;
		}

		if (IsCase(TransferCase.CatalogName, "break"))
		{
			Expected.BodyCount = 1;
			Expected.ConditionCount = IsCase(LoopCase.CatalogName, "do_while") ? 0 : 1;
			return Expected;
		}

		if (IsCase(TransferCase.CatalogName, "return"))
		{
			Expected.BodyCount = 1;
			Expected.ConditionCount = IsCase(LoopCase.CatalogName, "do_while") ? 0 : 1;
			return Expected;
		}

		Expected.BodyCount = CountCase.Limit;
		Expected.ConditionCount = IsCase(LoopCase.CatalogName, "do_while") ? CountCase.Limit : CountCase.Limit + 1;
		Expected.IncrementCount = CountCase.Limit;
		return Expected;
	}

public:
	TEST_METHOD(LoopsByKindCountAndTransfer)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-LOOP-DEPTH",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Bytecode
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Loop-depth product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FLoopCase& LoopCase : LoopCases)
		{
			for (const FCountCase& CountCase : CountCases)
			{
				for (const FTransferCase& TransferCase : TransferCases)
				{
					++ObservedCaseCount;
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-CF-LOOP-DEPTH",
						{ ANSI_TO_TCHAR(LoopCase.CatalogName), ANSI_TO_TCHAR(CountCase.CatalogName), ANSI_TO_TCHAR(TransferCase.CatalogName) }));
					const FString ModuleName = FString::Printf(
						TEXT("LoopDepth_%s_%s_%s"),
						ANSI_TO_TCHAR(LoopCase.CatalogName),
						ANSI_TO_TCHAR(CountCase.CatalogName),
						ANSI_TO_TCHAR(TransferCase.CatalogName));
					const FString Source = BuildLoopSource(LoopCase, CountCase, TransferCase);
					const FLoopExpectation Expected = GetExpected(LoopCase, CountCase, TransferCase);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);

					Engine.Reset(*TestRunner);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					ASSERT_THAT(AreEqual(asSUCCESS, BuildResult,
						*Case.Describe(TEXT("loop source should compile for the selected kind, count, and transfer"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("loop source should publish a module"))));

					if (BuildResult >= 0 && Module != nullptr)
					{
						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
						ASSERT_THAT(IsNotNull(Entry,
							*Case.Describe(TEXT("loop source should publish the exact Entry declaration"))));
						if (Entry != nullptr)
						{
							asUINT BytecodeLength = 0;
							Entry->GetByteCode(&BytecodeLength);
							ASSERT_THAT(IsTrue(BytecodeLength > 0,
								*Case.Describe(TEXT("loop entry should retain executable bytecode"))));

							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context,
								*Case.Describe(TEXT("loop source should create an execution context"))));
							if (Context != nullptr)
							{
								const int ExecuteResult = PrepareAndExecute(Context, Entry);
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
									*Case.Describe(TEXT("loop transfer should finish without an exception"))));
								const int32 ExpectedMarker = Expected.BodyCount * 100
									+ Expected.ConditionCount * 10
									+ Expected.IncrementCount;
								ASSERT_THAT(AreEqual(ExpectedMarker, static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("loop body, condition, and increment counters should retain exact control-flow counts"))));
								Context->Release();
							}
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("loop module should be discarded after the selected cell"))));
				}
			}
		}

		ASSERT_THAT(AreEqual(UE_ARRAY_COUNT(LoopCases) * UE_ARRAY_COUNT(CountCases) * UE_ARRAY_COUNT(TransferCases),
			ObservedCaseCount,
			TEXT("LANG-CF-LOOP-DEPTH must execute every loop kind, count, and transfer cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
