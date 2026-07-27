#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FForClauseTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.ForClauses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPresenceCase
	{
		const ANSICHAR* CatalogName;
		bool bPresent;
	};

	struct FCountCase
	{
		const ANSICHAR* CatalogName;
		int32 Limit;
	};

	struct FForExpectation
	{
		int32 InitCount = 0;
		int32 BodyCount = 0;
		int32 ConditionCount = 0;
		int32 IncrementCount = 0;
	};

	inline static constexpr FPresenceCase PresenceCases[] =
	{
		{ "present", true },
		{ "omitted", false },
	};

	inline static constexpr FCountCase CountCases[] =
	{
		{ "zero", 0 },
		{ "one", 1 },
		{ "many", 4 },
	};


	static FString PresenceClause(const FPresenceCase& Case, const TCHAR* PresentText)
	{
		return Case.bPresent ? FString(PresentText) : FString();
	}

	static FString BuildForSource(
		const FPresenceCase& InitCase,
		const FPresenceCase& ConditionCase,
		const FPresenceCase& IncrementCase,
		const FCountCase& CountCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int InitializeIndex(int&inout InitCount)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("	++InitCount;"));
		AppendGeneratedAsLine(Source, TEXT("	return 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("bool CountCondition(int Index, int Limit, int&inout ConditionCount)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("	++ConditionCount;"));
		AppendGeneratedAsLine(Source, TEXT("	return Index < Limit;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CountIncrement(int&inout Index, int&inout IncrementCount)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("	++IncrementCount;"));
		AppendGeneratedAsLine(Source, TEXT("	++Index;"));
		AppendGeneratedAsLine(Source, TEXT("	return Index;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("	int Limit = %d;"), CountCase.Limit));
		AppendGeneratedAsLine(Source, TEXT("	int Index = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	int InitCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	int BodyCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	int ConditionCount = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	int IncrementCount = 0;"));

		const FString InitClause = PresenceClause(InitCase, TEXT("Index = InitializeIndex(InitCount)"));
		const FString ConditionClause = PresenceClause(ConditionCase, TEXT("CountCondition(Index, Limit, ConditionCount)"));
		const FString IncrementClause = PresenceClause(IncrementCase, TEXT("CountIncrement(Index, IncrementCount)"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("	for (%s; %s; %s)"), *InitClause, *ConditionClause, *IncrementClause));
		AppendGeneratedAsLine(Source, TEXT("	{"));
		AppendGeneratedAsLine(Source, TEXT("		++BodyCount;"));
		if (!ConditionCase.bPresent)
		{
			AppendGeneratedAsLine(Source, TEXT("		if (Index >= Limit - 1)"));
			AppendGeneratedAsLine(Source, TEXT("		{"));
			AppendGeneratedAsLine(Source, TEXT("			break;"));
			AppendGeneratedAsLine(Source, TEXT("		}"));
		}
		if (!IncrementCase.bPresent)
		{
			AppendGeneratedAsLine(Source, TEXT("		++Index;"));
		}
		AppendGeneratedAsLine(Source, TEXT("	}"));
		AppendGeneratedAsLine(Source, TEXT("	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FForExpectation GetExpected(
		const FPresenceCase& InitCase,
		const FPresenceCase& ConditionCase,
		const FPresenceCase& IncrementCase,
		const FCountCase& CountCase)
	{
		FForExpectation Expected;
		Expected.InitCount = InitCase.bPresent ? 1 : 0;
		Expected.BodyCount = ConditionCase.bPresent
			? CountCase.Limit
			: (CountCase.Limit > 0 ? CountCase.Limit : 1);
		Expected.ConditionCount = ConditionCase.bPresent ? CountCase.Limit + 1 : 0;
		if (IncrementCase.bPresent)
		{
			Expected.IncrementCount = ConditionCase.bPresent
				? CountCase.Limit
				: (Expected.BodyCount > 0 ? Expected.BodyCount - 1 : 0);
		}
		return Expected;
	}

public:
	TEST_METHOD(ClausesByPresenceAndCount)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-FOR-CLAUSES",
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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("For-clause product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FPresenceCase& InitCase : PresenceCases)
		{
			for (const FPresenceCase& ConditionCase : PresenceCases)
			{
				for (const FPresenceCase& IncrementCase : PresenceCases)
				{
					for (const FCountCase& CountCase : CountCases)
					{
						++ObservedCaseCount;
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-CF-FOR-CLAUSES",
							{
								ANSI_TO_TCHAR(InitCase.CatalogName),
								ANSI_TO_TCHAR(ConditionCase.CatalogName),
								ANSI_TO_TCHAR(IncrementCase.CatalogName),
								ANSI_TO_TCHAR(CountCase.CatalogName),
							}));
						const FString ModuleName = FString::Printf(
							TEXT("ForClauses_%s_%s_%s_%s"),
							ANSI_TO_TCHAR(InitCase.CatalogName),
							ANSI_TO_TCHAR(ConditionCase.CatalogName),
							ANSI_TO_TCHAR(IncrementCase.CatalogName),
							ANSI_TO_TCHAR(CountCase.CatalogName));
						const FString Source = BuildForSource(InitCase, ConditionCase, IncrementCase, CountCase);
						const FForExpectation Expected = GetExpected(InitCase, ConditionCase, IncrementCase, CountCase);
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
							*Case.Describe(TEXT("for clause source should compile for the selected presence and count"))));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("for clause source should publish a module"))));

						if (BuildResult >= 0 && Module != nullptr)
						{
							asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
							ASSERT_THAT(IsNotNull(Entry,
								*Case.Describe(TEXT("for clause source should publish the exact Entry declaration"))));
							if (Entry != nullptr)
							{
								asUINT BytecodeLength = 0;
								Entry->GetByteCode(&BytecodeLength);
								ASSERT_THAT(IsTrue(BytecodeLength > 0,
									*Case.Describe(TEXT("for clause entry should retain executable bytecode"))));

								asIScriptContext* const Context = ScriptEngine->CreateContext();
								ASSERT_THAT(IsNotNull(Context,
									*Case.Describe(TEXT("for clause source should create an execution context"))));
								if (Context != nullptr)
								{
									const int ExecuteResult = PrepareAndExecute(Context, Entry);
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
										*Case.Describe(TEXT("for clause execution should finish without an exception"))));
									const int32 ExpectedMarker = Expected.InitCount * 1000
										+ Expected.BodyCount * 100
										+ Expected.ConditionCount * 10
										+ Expected.IncrementCount;
									ASSERT_THAT(AreEqual(ExpectedMarker, static_cast<int32>(Context->GetReturnDWord()),
										*Case.Describe(TEXT("for clause event counters should preserve exact initialization, body, condition, and increment behavior"))));
									Context->Release();
								}
							}
						}

						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("for clause module should be discarded after the selected cell"))));
					}
				}
			}
		}

		ASSERT_THAT(AreEqual(UE_ARRAY_COUNT(PresenceCases) * UE_ARRAY_COUNT(PresenceCases)
			* UE_ARRAY_COUNT(PresenceCases) * UE_ARRAY_COUNT(CountCases),
			ObservedCaseCount,
			TEXT("LANG-CF-FOR-CLAUSES must execute every clause-presence and count cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
