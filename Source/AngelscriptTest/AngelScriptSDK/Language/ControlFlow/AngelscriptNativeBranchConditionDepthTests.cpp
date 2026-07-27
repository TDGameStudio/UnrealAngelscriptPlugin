#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBranchConditionDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.BranchConditionDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FBranchCase
	{
		const ANSICHAR* Name;
	};

	struct FConditionCase
	{
		const ANSICHAR* Name;
	};

	struct FSelectionCase
	{
		const ANSICHAR* Name;
		bool bFirst;
		bool bSecond;
	};

	struct FLineEndingCase
	{
		const ANSICHAR* Name;
		bool bCarriageReturn;
	};

	struct FConditionCounter
	{
		int32 Calls = 0;
	};

	inline static constexpr FBranchCase BranchCases[] =
	{
		{ "if" },
		{ "if_else" },
		{ "else_if_chain" },
	};

	inline static constexpr FConditionCase ConditionCases[] =
	{
		{ "variable" },
		{ "comparison" },
		{ "logical" },
		{ "negated" },
		{ "side_effect" },
	};

	inline static constexpr FSelectionCase SelectionCases[] =
	{
		{ "first", true, false },
		{ "second", false, true },
		{ "none", false, false },
	};

	inline static constexpr FLineEndingCase LineEndingCases[] =
	{
		{ "lf", false },
		{ "crlf", true },
	};

	inline static FConditionCounter* ActiveConditionCounter = nullptr;

	static bool IsNamed(const ANSICHAR* Actual, const ANSICHAR* Expected)
	{
		return FCStringAnsi::Strcmp(Actual, Expected) == 0;
	}

	static bool EvaluateBranchCondition(const bool Value)
	{
		if (ActiveConditionCounter != nullptr)
		{
			++ActiveConditionCounter->Calls;
		}
		return Value;
	}

	static int32 ReadBranchConditionCalls()
	{
		return ActiveConditionCounter != nullptr
			? ActiveConditionCounter->Calls
			: INDEX_NONE;
	}

	static FString BuildConditionExpression(
		const FConditionCase& ConditionCase,
		const FString& ValueName)
	{
		if (IsNamed(ConditionCase.Name, "variable"))
		{
			return ValueName;
		}
		if (IsNamed(ConditionCase.Name, "comparison"))
		{
			return ValueName + TEXT(" == true");
		}
		if (IsNamed(ConditionCase.Name, "logical"))
		{
			return ValueName + TEXT(" && true");
		}
		if (IsNamed(ConditionCase.Name, "negated"))
		{
			return TEXT("!(") + ValueName + TEXT(" == false)");
		}
		return TEXT("EvaluateBranchCondition(") + ValueName + TEXT(")");
	}

	static FString BuildSource(
		const FBranchCase& BranchCase,
		const FConditionCase& ConditionCase,
		const FSelectionCase& SelectionCase,
		const FLineEndingCase& LineEndingCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("// Branch=%hs Condition=%hs Selection=%hs LineEnding=%hs"),
			BranchCase.Name,
			ConditionCase.Name,
			SelectionCase.Name,
			LineEndingCase.Name));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, SelectionCase.bFirst
			? TEXT("\tbool FirstCondition = true;")
			: TEXT("\tbool FirstCondition = false;"));
		AppendGeneratedAsLine(Source, SelectionCase.bSecond
			? TEXT("\tbool SecondCondition = true;")
			: TEXT("\tbool SecondCondition = false;"));
		AppendGeneratedAsLine(Source, TEXT("\tint BranchMarker = 0;"));

		const FString FirstCondition = BuildConditionExpression(ConditionCase, TEXT("FirstCondition"));
		const FString SecondCondition = BuildConditionExpression(ConditionCase, TEXT("SecondCondition"));
		if (IsNamed(BranchCase.Name, "if"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (") + FirstCondition + TEXT(")"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tBranchMarker = 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsNamed(BranchCase.Name, "if_else"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (") + FirstCondition + TEXT(")"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tBranchMarker = 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\telse"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tBranchMarker = 20;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (") + FirstCondition + TEXT(")"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tBranchMarker = 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\telse if (") + SecondCondition + TEXT(")"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tBranchMarker = 20;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\telse"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tBranchMarker = 30;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}

		AppendGeneratedAsLine(Source, TEXT("\treturn BranchMarker * 100 + ReadBranchConditionCalls();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		if (LineEndingCase.bCarriageReturn)
		{
			const FString LineFeed = FString::Chr(10);
			const FString CarriageReturnLineFeed = FString::Chr(13) + FString::Chr(10);
			Source.ReplaceInline(*LineFeed, *CarriageReturnLineFeed);
		}
		return Source;
	}

	static int32 GetExpectedMarker(
		const FBranchCase& BranchCase,
		const FSelectionCase& SelectionCase)
	{
		if (IsNamed(BranchCase.Name, "if"))
		{
			return SelectionCase.bFirst ? 10 : 0;
		}
		if (IsNamed(BranchCase.Name, "if_else"))
		{
			return SelectionCase.bFirst ? 10 : 20;
		}
		if (SelectionCase.bFirst)
		{
			return 10;
		}
		return SelectionCase.bSecond ? 20 : 30;
	}

	static int32 GetExpectedCalls(
		const FBranchCase& BranchCase,
		const FConditionCase& ConditionCase,
		const FSelectionCase& SelectionCase)
	{
		if (!IsNamed(ConditionCase.Name, "side_effect"))
		{
			return 0;
		}
		if (IsNamed(BranchCase.Name, "else_if_chain") && !SelectionCase.bFirst)
		{
			return 2;
		}
		return 1;
	}

	static FString MakeModuleName(
		const FBranchCase& BranchCase,
		const FConditionCase& ConditionCase,
		const FSelectionCase& SelectionCase,
		const FLineEndingCase& LineEndingCase)
	{
		return FString::Printf(
			TEXT("BranchCondition_%hs_%hs_%hs_%hs"),
			BranchCase.Name,
			ConditionCase.Name,
			SelectionCase.Name,
			LineEndingCase.Name);
	}

public:
	TEST_METHOD(BranchesByConditionSelectionAndLineEnding)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-BRANCH-CONDITION-DEPTH",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
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
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Branch condition product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		// The first red run intentionally used the plain registration route. The
		// current fork requires an explicit ASAutoCaller for non-generic native
		// calls; the corrected caller-backed registration is the active path and
		// the discovered requirement is recorded in issues.md.
		const ASAutoCaller::FunctionCaller EvaluateCaller = ASAutoCaller::MakeFunctionCaller(EvaluateBranchCondition);
		const int32 EvaluateRegistration = ScriptEngine->RegisterGlobalFunction(
			"bool EvaluateBranchCondition(bool Value)",
			asFUNCTION(EvaluateBranchCondition),
			asCALL_CDECL,
			*(asFunctionCaller*)&EvaluateCaller);
		ASSERT_THAT(IsTrue(EvaluateRegistration >= 0,
			*FString::Printf(TEXT("branch condition callback should register. Result=%d Messages={%s}"),
			EvaluateRegistration,
			*Engine.GetMessagesText())));
		const ASAutoCaller::FunctionCaller ReadCaller = ASAutoCaller::MakeFunctionCaller(ReadBranchConditionCalls);
		const int32 ReadRegistration = ScriptEngine->RegisterGlobalFunction(
			"int ReadBranchConditionCalls()",
			asFUNCTION(ReadBranchConditionCalls),
			asCALL_CDECL,
			*(asFunctionCaller*)&ReadCaller);
		ASSERT_THAT(IsTrue(ReadRegistration >= 0,
			*FString::Printf(TEXT("branch counter callback should register. Result=%d Messages={%s}"),
			ReadRegistration,
			*Engine.GetMessagesText())));

		for (const FBranchCase& BranchCase : BranchCases)
		{
			for (const FConditionCase& ConditionCase : ConditionCases)
			{
				for (const FSelectionCase& SelectionCase : SelectionCases)
				{
					for (const FLineEndingCase& LineEndingCase : LineEndingCases)
					{
						FConditionCounter Counter;
						ActiveConditionCounter = &Counter;
						ON_SCOPE_EXIT
						{
							ActiveConditionCounter = nullptr;
						};

						const FString CaseId = MakeNativeCaseId(
							"LANG-CF-BRANCH-CONDITION-DEPTH",
							{
								ANSI_TO_TCHAR(BranchCase.Name),
								ANSI_TO_TCHAR(ConditionCase.Name),
								ANSI_TO_TCHAR(SelectionCase.Name),
								ANSI_TO_TCHAR(LineEndingCase.Name),
							});
						const FString ModuleName = MakeModuleName(
							BranchCase,
							ConditionCase,
							SelectionCase,
							LineEndingCase);
						const FString Source = BuildSource(
							BranchCase,
							ConditionCase,
							SelectionCase,
							LineEndingCase);
						PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);

						Engine.ResetMessages();
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						asIScriptModule* Module = nullptr;
						const int32 BuildResult = CompileNativeModule(
							ScriptEngine,
							ModuleNameUtf8.Get(),
							SourceUtf8.Get(),
							Module);
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*FString::Printf(TEXT("%s should compile. Build=%d Messages={%s}"),
								*CaseId,
								BuildResult,
								*Engine.GetMessagesText())));
						ASSERT_THAT(IsNotNull(Module,
							*FString::Printf(TEXT("%s should publish its branch module"), *CaseId)));
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
						ASSERT_THAT(IsNotNull(Entry,
							*FString::Printf(TEXT("%s should resolve exact int Entry()"), *CaseId)));
						if (Entry == nullptr)
						{
							continue;
						}

						asIScriptContext* Context = ScriptEngine->CreateContext();
						ASSERT_THAT(IsNotNull(Context,
							*FString::Printf(TEXT("%s should create a context"), *CaseId)));
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

						const int32 PrepareResult = Context->Prepare(Entry);
						ASSERT_THAT(AreEqual(asSUCCESS, PrepareResult,
							*FString::Printf(TEXT("%s should prepare"), *CaseId)));
						if (PrepareResult < 0)
						{
							continue;
						}
						const int32 ExecuteResult = Context->Execute();
						ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
							*FString::Printf(TEXT("%s should finish. Result=%d Exception={%s}"),
								*CaseId,
								ExecuteResult,
								UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr
									? Context->GetExceptionString()
									: ""))));
						if (ExecuteResult != asEXECUTION_FINISHED)
						{
							continue;
						}
						const int32 ExpectedResult = GetExpectedMarker(BranchCase, SelectionCase) * 100
							+ GetExpectedCalls(BranchCase, ConditionCase, SelectionCase);
						ASSERT_THAT(AreEqual(ExpectedResult,
							static_cast<int32>(Context->GetReturnDWord()),
							*FString::Printf(TEXT("%s should retain exact branch marker and condition-call count. Expected=%d Actual=%d"),
								*CaseId,
								ExpectedResult,
								static_cast<int32>(Context->GetReturnDWord()))));
						ASSERT_THAT(AreEqual(
							GetExpectedCalls(BranchCase, ConditionCase, SelectionCase),
							Counter.Calls,
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
							INDEX_NONE,
							ReadBranchConditionCalls(),
							*FString::Printf(TEXT("%s native counter should be detached before the next generated cell"), *CaseId)));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
