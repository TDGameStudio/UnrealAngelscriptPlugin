#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FLoopConditionTransferDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.LoopConditionTransferDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FLoopCase
	{
		const ANSICHAR* Name;
		const ANSICHAR* Header;
	};

	struct FConditionCase
	{
		const ANSICHAR* Name;
	};

	struct FCountCase
	{
		const ANSICHAR* Name;
		int32 Limit;
	};

	struct FTransferCase
	{
		const ANSICHAR* Name;
	};

	struct FConditionCounter
	{
		int32 Calls = 0;
	};

	inline static FConditionCounter* ActiveConditionCounter = nullptr;

	inline static constexpr FLoopCase LoopCases[] =
	{
		{ "while", "while" },
		{ "do_while", "do" },
		{ "for", "for" },
	};

	inline static constexpr FConditionCase ConditionCases[] =
	{
		{ "variable" },
		{ "comparison" },
		{ "logical" },
		{ "negated" },
		{ "side_effect" },
	};

	inline static constexpr FCountCase CountCases[] =
	{
		{ "zero", 0 },
		{ "one", 1 },
		{ "two", 2 },
	};

	inline static constexpr FTransferCase TransferCases[] =
	{
		{ "none" },
		{ "break" },
		{ "continue" },
		{ "return" },
	};

	static FString MakeCaseId(
		const FLoopCase& LoopCase,
		const FConditionCase& ConditionCase,
		const FCountCase& CountCase,
		const FTransferCase& TransferCase)
	{
		return FString::Printf(
			TEXT("LANG-CF-LOOP-COND-TRANSFER-DEPTH-%hs-%hs-%hs-%hs"),
			LoopCase.Name,
			ConditionCase.Name,
			CountCase.Name,
			TransferCase.Name);
	}

	static FString MakeModuleName(
		const FLoopCase& LoopCase,
		const FConditionCase& ConditionCase,
		const FCountCase& CountCase,
		const FTransferCase& TransferCase)
	{
		return FString::Printf(
			TEXT("NativeLoopCondition_%hs_%hs_%hs_%hs"),
			LoopCase.Name,
			ConditionCase.Name,
			CountCase.Name,
			TransferCase.Name);
	}

	static FString BuildConditionExpression(const FConditionCase& ConditionCase)
	{
		if (FCStringAnsi::Strcmp(ConditionCase.Name, "variable") == 0)
		{
			return TEXT("KeepGoing");
		}
		if (FCStringAnsi::Strcmp(ConditionCase.Name, "comparison") == 0)
		{
			return TEXT("Index < Limit");
		}
		if (FCStringAnsi::Strcmp(ConditionCase.Name, "logical") == 0)
		{
			return TEXT("(Index < Limit) && (Limit >= 0)");
		}
		if (FCStringAnsi::Strcmp(ConditionCase.Name, "negated") == 0)
		{
			return TEXT("!(Index >= Limit)");
		}
		return TEXT("CheckCondition(Index, Limit)");
	}

	static bool IsSideEffectCondition(const FConditionCase& ConditionCase)
	{
		return FCStringAnsi::Strcmp(ConditionCase.Name, "side_effect") == 0;
	}

	static bool CheckCondition(int Index, int Limit)
	{
		if (ActiveConditionCounter != nullptr)
		{
			++ActiveConditionCounter->Calls;
		}
		return Index < Limit;
	}

	static int GetConditionCalls()
	{
		return ActiveConditionCounter != nullptr ? ActiveConditionCounter->Calls : 0;
	}

	static FString BuildSource(
		const FLoopCase& LoopCase,
		const FConditionCase& ConditionCase,
		const FCountCase& CountCase,
		const FTransferCase& TransferCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Condition = BuildConditionExpression(ConditionCase);
		const bool bDoWhile = FCStringAnsi::Strcmp(LoopCase.Name, "do_while") == 0;
		const bool bFor = FCStringAnsi::Strcmp(LoopCase.Name, "for") == 0;
		const bool bVariableCondition = FCStringAnsi::Strcmp(ConditionCase.Name, "variable") == 0;
		const bool bReturnTransfer = FCStringAnsi::Strcmp(TransferCase.Name, "return") == 0;
		const bool bBreakTransfer = FCStringAnsi::Strcmp(TransferCase.Name, "break") == 0;
		const bool bContinueTransfer = FCStringAnsi::Strcmp(TransferCase.Name, "continue") == 0;

		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("// loop=%hs condition=%hs count=%hs transfer=%hs"),
			LoopCase.Name,
			ConditionCase.Name,
			CountCase.Name,
			TransferCase.Name));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), CountCase.Limit));
		AppendGeneratedAsLine(Source, TEXT("\tint BodyCalls = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Index = 0;"));
		if (bVariableCondition)
		{
			AppendGeneratedAsLine(Source, TEXT("\tbool KeepGoing = Index < Limit;"));
		}
		if (bDoWhile)
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (Limit == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tdo")));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
		}
		else if (bFor)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tfor (; %s; ++Index)"), *Condition));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\twhile (%s)"), *Condition));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
		}

		AppendGeneratedAsLine(Source, TEXT("\t\t++BodyCalls;"));
		if (bBreakTransfer)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Index == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (bContinueTransfer)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Index == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			if (!bFor)
			{
				AppendGeneratedAsLine(Source, TEXT("\t\t\t++Index;"));
			}
			if (bVariableCondition)
			{
				AppendGeneratedAsLine(Source, bFor
					? TEXT("\t\t\tKeepGoing = Index + 1 < Limit;")
					: TEXT("\t\t\tKeepGoing = Index < Limit;"));
			}
			AppendGeneratedAsLine(Source, TEXT("\t\t\tcontinue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (bReturnTransfer)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Index == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\treturn 1000 + BodyCalls * 100 + GetConditionCalls();"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		if (bFor && bVariableCondition && !bBreakTransfer && !bReturnTransfer)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tKeepGoing = Index + 1 < Limit;"));
		}

		if (!bFor)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\t++Index;"));
			if (bVariableCondition)
			{
				AppendGeneratedAsLine(Source, TEXT("\t\tKeepGoing = Index < Limit;"));
			}
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		if (bDoWhile)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\twhile (%s);"), *Condition));
		}
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\treturn BodyCalls * 100 + GetConditionCalls();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int32 GetExpectedBodyCalls(const FCountCase& CountCase, const FTransferCase& TransferCase)
	{
		if (FCStringAnsi::Strcmp(TransferCase.Name, "break") == 0)
		{
			return FMath::Min(CountCase.Limit, 1);
		}
		if (FCStringAnsi::Strcmp(TransferCase.Name, "return") == 0)
		{
			return CountCase.Limit > 0 ? 1 : 0;
		}
		return CountCase.Limit;
	}

	static int32 GetExpectedConditionCalls(
		const FLoopCase& LoopCase,
		const FConditionCase& ConditionCase,
		const FCountCase& CountCase,
		const FTransferCase& TransferCase)
	{
		if (!IsSideEffectCondition(ConditionCase))
		{
			return 0;
		}

		const int32 BodyCalls = GetExpectedBodyCalls(CountCase, TransferCase);
		if (FCStringAnsi::Strcmp(LoopCase.Name, "do_while") == 0)
		{
			return FCStringAnsi::Strcmp(TransferCase.Name, "break") == 0
				|| FCStringAnsi::Strcmp(TransferCase.Name, "return") == 0
				? 0
				: BodyCalls;
		}

		// while/for evaluate their condition once before the first body. A
		// normal or continue path evaluates it once more after the final body;
		// break and return leave without that trailing evaluation.
		return 1 + ((FCStringAnsi::Strcmp(TransferCase.Name, "break") != 0
			&& FCStringAnsi::Strcmp(TransferCase.Name, "return") != 0)
			? BodyCalls
			: 0);
	}

	static int32 GetExpectedResult(
		const FLoopCase& LoopCase,
		const FConditionCase& ConditionCase,
		const FCountCase& CountCase,
		const FTransferCase& TransferCase)
	{
		const int32 BodyCalls = GetExpectedBodyCalls(CountCase, TransferCase);
		const int32 ConditionCalls = GetExpectedConditionCalls(LoopCase, ConditionCase, CountCase, TransferCase);
		if (FCStringAnsi::Strcmp(TransferCase.Name, "return") == 0 && CountCase.Limit > 0)
		{
			return 1000 + BodyCalls * 100 + ConditionCalls;
		}
		return BodyCalls * 100 + ConditionCalls;
	}

public:
	TEST_METHOD(LoopsByConditionCountAndTransfer)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-LOOP-COND-TRANSFER-DEPTH",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		ActiveConditionCounter = nullptr;
		ON_SCOPE_EXIT
		{
			ActiveConditionCounter = nullptr;
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Loop condition product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller CheckConditionCaller = ASAutoCaller::MakeFunctionCaller(CheckCondition);
		const ASAutoCaller::FunctionCaller GetConditionCallsCaller = ASAutoCaller::MakeFunctionCaller(GetConditionCalls);
		const int CheckConditionRegistration = ScriptEngine->RegisterGlobalFunction(
			"bool CheckCondition(int Index, int Limit)",
			asFUNCTION(CheckCondition),
			asCALL_CDECL,
			*(asFunctionCaller*)&CheckConditionCaller);
		ASSERT_THAT(IsTrue(CheckConditionRegistration >= 0,
			*FString::Printf(TEXT("Loop condition product should register its native side-effect predicate. Result=%d Messages={%s}"),
				CheckConditionRegistration,
				*Engine.GetMessagesText())));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int GetConditionCalls()",
			asFUNCTION(GetConditionCalls),
			asCALL_CDECL,
			*(asFunctionCaller*)&GetConditionCallsCaller) >= 0,
			TEXT("Loop condition product should register its native independent counter")));

		for (const FLoopCase& LoopCase : LoopCases)
		{
			for (const FConditionCase& ConditionCase : ConditionCases)
			{
				for (const FCountCase& CountCase : CountCases)
				{
					for (const FTransferCase& TransferCase : TransferCases)
					{
						FConditionCounter ConditionCounter;
						ActiveConditionCounter = &ConditionCounter;
						ON_SCOPE_EXIT
						{
							ActiveConditionCounter = nullptr;
						};
						const FString CaseId = MakeCaseId(LoopCase, ConditionCase, CountCase, TransferCase);
						const FString ModuleName = MakeModuleName(LoopCase, ConditionCase, CountCase, TransferCase);
						const FString Source = BuildSource(LoopCase, ConditionCase, CountCase, TransferCase);
						PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);

						Engine.ResetMessages();
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*FString::Printf(TEXT("%s should compile. Build=%d Messages={%s}"),
								*CaseId,
								BuildResult,
								*Engine.GetMessagesText())));
						ASSERT_THAT(IsNotNull(Module, *FString::Printf(TEXT("%s should publish its loop module"), *CaseId)));
						if (BuildResult < 0 || Module == nullptr)
						{
							continue;
						}
						ON_SCOPE_EXIT
						{
							if (ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS) != nullptr)
							{
								ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							}
						};

						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
						ASSERT_THAT(IsNotNull(Entry, *FString::Printf(TEXT("%s should resolve its exact entry"), *CaseId)));
						if (Entry == nullptr)
						{
							continue;
						}

						asIScriptContext* Context = ScriptEngine->CreateContext();
						ASSERT_THAT(IsNotNull(Context, *FString::Printf(TEXT("%s should create a context"), *CaseId)));
						if (Context == nullptr)
						{
							continue;
						}
						ON_SCOPE_EXIT
						{
							if (Context != nullptr)
							{
								Context->Release();
							}
						};

						const int PrepareResult = Context->Prepare(Entry);
						ASSERT_THAT(AreEqual(asSUCCESS, PrepareResult,
							*FString::Printf(TEXT("%s should prepare"), *CaseId)));
						if (PrepareResult < 0)
						{
							continue;
						}
						const int ExecuteResult = Context->Execute();
						ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
							*FString::Printf(TEXT("%s should finish. Result=%d Exception={%s}"),
								*CaseId,
								ExecuteResult,
								UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : ""))));
						if (ExecuteResult != asEXECUTION_FINISHED)
						{
							continue;
						}
						const int32 ExpectedResult = GetExpectedResult(LoopCase, ConditionCase, CountCase, TransferCase);
						const int32 ActualResult = static_cast<int32>(Context->GetReturnDWord());
						ASSERT_THAT(AreEqual(
							ExpectedResult,
							ActualResult,
							*FString::Printf(TEXT("%s should retain the independent body/condition result. Expected=%d Actual=%d"),
								*CaseId,
								ExpectedResult,
								ActualResult)));
						ASSERT_THAT(AreEqual(
							GetExpectedConditionCalls(LoopCase, ConditionCase, CountCase, TransferCase),
							ConditionCounter.Calls,
							*FString::Printf(TEXT("%s should retain an independent native condition-call count"), *CaseId)));
						ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
							*FString::Printf(TEXT("%s should unprepare cleanly"), *CaseId)));
						Context->Release();
						Context = nullptr;
						ASSERT_THAT(AreEqual(
							asSUCCESS,
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
							*FString::Printf(TEXT("%s should explicitly discard its module"), *CaseId)));
						ASSERT_THAT(IsNull(
							ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*FString::Printf(TEXT("%s module should be absent before the next generated cell"), *CaseId)));
						ActiveConditionCounter = nullptr;
						ASSERT_THAT(AreEqual(
							0,
							GetConditionCalls(),
							*FString::Printf(TEXT("%s native counter should be detached before the next generated cell"), *CaseId)));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
