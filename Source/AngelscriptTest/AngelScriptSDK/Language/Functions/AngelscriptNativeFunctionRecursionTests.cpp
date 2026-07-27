#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionRecursionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.Recursion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	struct FDepthCase
	{
		const ANSICHAR* CatalogName;
		int32 OrdinaryDepth;
		bool bMutual;
		bool bConfiguredLimit;
	};

	struct FOutcomeCase
	{
		const ANSICHAR* CatalogName;
		bool bException;
	};

	struct FParameterTypeCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
		bool bValueObject;
		bool bReferenceObject;
	};

	inline static constexpr FDepthCase DepthCases[] =
	{
		{ "zero", 0, false, false },
		{ "one", 1, false, false },
		{ "eight", 8, true, false },
		{ "configured_limit", 12, true, true },
	};

	inline static constexpr FOutcomeCase OutcomeCases[] =
	{
		{ "return", false },
		{ "exception", true },
	};

	inline static constexpr FParameterTypeCase TypeCases[] =
	{
		{ "primitive", "int", false, false },
		{ "value_object", "FNativeCaseValue", true, false },
		{ "reference_object", "FNativeCaseReference", false, true },
	};

	static int32 ExecutionDepth(const FDepthCase& DepthCase, const FOutcomeCase& OutcomeCase)
	{
		if (!DepthCase.bConfiguredLimit)
		{
			return DepthCase.OrdinaryDepth;
		}
		return OutcomeCase.bException ? DepthCase.OrdinaryDepth + 1 : DepthCase.OrdinaryDepth - 1;
	}

	static FString MakeSuffix(
		const FDepthCase& DepthCase,
		const FOutcomeCase& OutcomeCase,
		const FParameterTypeCase& TypeCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			DepthCase.CatalogName,
			OutcomeCase.CatalogName,
			TypeCase.CatalogName);
	}

	static FString MakeParameters(const FParameterTypeCase& TypeCase)
	{
		if (TypeCase.bReferenceObject)
		{
			return TEXT("FNativeCaseReference Value, int Depth, int Accumulator");
		}
		return FString::Printf(TEXT("%hs Value, int Depth"), TypeCase.ScriptType);
	}

	static FString MakeInitialArguments(
		const FParameterTypeCase& TypeCase,
		const int32 Depth)
	{
		if (TypeCase.bReferenceObject)
		{
			return FString::Printf(TEXT("nullptr, %d, 1"), Depth);
		}
		return FString::Printf(TEXT("Value, %d"), Depth);
	}

	static FString MakeRecursiveArguments(
		const FParameterTypeCase& TypeCase)
	{
		if (TypeCase.bReferenceObject)
		{
			return TEXT("Value, Depth - 1, Accumulator + 1");
		}
		return TEXT("Value, Depth - 1");
	}

	static void AppendBaseReturn(
		FString& Source,
		const FOutcomeCase& OutcomeCase,
		const FParameterTypeCase& TypeCase,
		const TCHAR* BaseIndent = TEXT("\t"))
	{
		const FString ReturnIndent = FString(BaseIndent) + TEXT("\t");
		if (OutcomeCase.bException)
		{
			AppendGeneratedAsLine(Source, ReturnIndent + TEXT("int Zero = 0;"));
			AppendGeneratedAsLine(Source, ReturnIndent + TEXT("return 1 / Zero;"));
		}
		else if (TypeCase.bValueObject)
		{
			AppendGeneratedAsLine(Source, ReturnIndent + TEXT("return Value.Value;"));
		}
		else if (TypeCase.bReferenceObject)
		{
			AppendGeneratedAsLine(Source, ReturnIndent + TEXT("return Accumulator;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, ReturnIndent + TEXT("return Value;"));
		}
	}

	static void AppendRecursiveMethod(
		FString& Source,
		const FOutcomeCase& OutcomeCase,
		const FParameterTypeCase& TypeCase)
	{
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Recurse(%s)"), *MakeParameters(TypeCase)));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tif (Depth <= 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendBaseReturn(Source, OutcomeCase, TypeCase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		if (TypeCase.bValueObject)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tFNativeCaseValue NextValue = Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tNextValue.Value += 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Recurse(NextValue, Depth - 1);"));
		}
		else if (!TypeCase.bReferenceObject)
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tint NextValue = Value + 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Recurse(NextValue, Depth - 1);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn Recurse(%s);"), *MakeRecursiveArguments(TypeCase)));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendRecursiveFunction(
		FString& Source,
		const FString& FunctionName,
		const FString& NextFunctionName,
		const FOutcomeCase& OutcomeCase,
		const FParameterTypeCase& TypeCase)
	{
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s(%s)"), *FunctionName, *MakeParameters(TypeCase)));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Depth <= 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendBaseReturn(Source, OutcomeCase, TypeCase);
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		if (TypeCase.bValueObject)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue NextValue = Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tNextValue.Value += 1;"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s(NextValue, Depth - 1);"), *NextFunctionName));
		}
		else if (!TypeCase.bReferenceObject)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint NextValue = Value + 1;"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s(NextValue, Depth - 1);"), *NextFunctionName));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s(%s);"),
				*NextFunctionName,
				*MakeRecursiveArguments(TypeCase)));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildRecursionSource(
		const FDepthCase& DepthCase,
		const FOutcomeCase& OutcomeCase,
		const FParameterTypeCase& TypeCase)
	{
		const FString Suffix = MakeSuffix(DepthCase, OutcomeCase, TypeCase);
		const FString RecurseA = TEXT("RecurseA_") + Suffix;
		const FString RecurseB = TEXT("RecurseB_") + Suffix;
		const FString Entry = TEXT("Run_") + Suffix;
		FString Source;
		if (DepthCase.bMutual)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s(%s);"), *RecurseB, *MakeParameters(TypeCase)));
			AppendGeneratedAsLine(Source);
			AppendRecursiveFunction(Source, RecurseA, RecurseB, OutcomeCase, TypeCase);
			AppendRecursiveFunction(Source, RecurseB, RecurseA, OutcomeCase, TypeCase);
		}
		else
		{
			AppendRecursiveFunction(Source, RecurseA, RecurseA, OutcomeCase, TypeCase);
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *Entry));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (TypeCase.bValueObject)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Value(1);"));
		}
		else if (!TypeCase.bReferenceObject)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 1;"));
		}
		if (DepthCase.bMutual)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s(%s);"),
				*RecurseA,
				*MakeInitialArguments(TypeCase, ExecutionDepth(DepthCase, OutcomeCase))));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s(%s);"),
				*RecurseA,
				*MakeInitialArguments(TypeCase, ExecutionDepth(DepthCase, OutcomeCase))));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CleanAfterRecursion()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 42;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

public:
	TEST_METHOD(DepthsByTypeAndOutcome)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-RECURSION",
			ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function recursion product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Function recursion product should register its tracked value type")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine),
			TEXT("Function recursion product should register its reference type")));

		for (const FDepthCase& DepthCase : DepthCases)
		{
			for (const FOutcomeCase& OutcomeCase : OutcomeCases)
			{
				for (const FParameterTypeCase& TypeCase : TypeCases)
				{
					Lifecycle.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-FN-RECURSION",
						{
							ANSI_TO_TCHAR(DepthCase.CatalogName),
							ANSI_TO_TCHAR(OutcomeCase.CatalogName),
							ANSI_TO_TCHAR(TypeCase.CatalogName),
						}));
					const FString Suffix = MakeSuffix(DepthCase, OutcomeCase, TypeCase);
					const FString ModuleName = TEXT("FunctionRecursion_") + Suffix;
					const FString Source = BuildRecursionSource(DepthCase, OutcomeCase, TypeCase);
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
				if (BuildResult < 0)
				{
					TestRunner->AddInfo(Engine.GetMessagesText());
				}
				if (DepthCase.bMutual)
				{
					TestRunner->AddInfo(TEXT("Current fork records mutual global-function prototypes as a negative embedding boundary: a prototype is parsed as a mutable global and is rejected."));
					ASSERT_THAT(IsTrue(BuildResult < 0,
						*Case.Describe(TEXT("mutual recursion prototype should remain a documented current-fork negative boundary"))));
					ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0,
						*Case.Describe(TEXT("mutual recursion boundary should preserve its compiler diagnostic"))));
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					continue;
				}
				ASSERT_THAT(IsTrue(BuildResult >= 0,
						*Case.Describe(TEXT("recursive cell should compile"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("recursive cell should publish a module"))));
					if (BuildResult >= 0 && Module != nullptr)
					{
						const FString EntryDeclaration = FString::Printf(TEXT("int Run_%s()"), *Suffix);
						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, TCHAR_TO_ANSI(*EntryDeclaration));
						asIScriptFunction* const Clean = GetNativeFunctionByExactDecl(Module, "int CleanAfterRecursion()");
						ASSERT_THAT(IsNotNull(Entry,
							*Case.Describe(TEXT("recursive entry should resolve by exact declaration"))));
						ASSERT_THAT(IsNotNull(Clean,
							*Case.Describe(TEXT("recursive cell should expose its context-reuse probe"))));
						if (Entry != nullptr && Clean != nullptr)
						{
							const asPWORD PreviousNestedLimit = ScriptEngine->GetEngineProperty(asEP_MAX_NESTED_CALLS);
							if (DepthCase.bConfiguredLimit)
							{
								ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_MAX_NESTED_CALLS, DepthCase.OrdinaryDepth),
									*Case.Describe(TEXT("configured-limit cell should install its explicit nested-call limit"))));
							}

							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context,
								*Case.Describe(TEXT("recursive cell should create an execution context"))));
							if (Context != nullptr)
							{
								const int ExecuteResult = PrepareAndExecute(Context, Entry);
								if (OutcomeCase.bException)
								{
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
										*Case.Describe(TEXT("exception recursion cell should stop with an execution exception"))));
									ASSERT_THAT(IsTrue(Context->GetExceptionString() != nullptr && Context->GetExceptionString()[0] != '\0',
										*Case.Describe(TEXT("exception recursion cell should expose exception text"))));
									ASSERT_THAT(IsTrue(Context->GetCallstackSize() > 0,
										*Case.Describe(TEXT("exception recursion cell should expose recursive stack metadata"))));
									ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
										*Case.Describe(TEXT("exception recursion cell should identify the throwing function"))));
								}
								else
								{
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
										*Case.Describe(TEXT("return recursion cell should finish"))));
									if (ExecuteResult == asEXECUTION_FINISHED)
									{
										ASSERT_THAT(AreEqual(1 + ExecutionDepth(DepthCase, OutcomeCase), static_cast<int32>(Context->GetReturnDWord()),
											*Case.Describe(TEXT("recursive result should prove every requested frame was traversed"))));
									}
								}

								ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
									*Case.Describe(TEXT("recursive context should unprepare after return or exception"))));
								ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Clean),
									*Case.Describe(TEXT("recursive context should prepare a clean follow-up function"))));
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
									*Case.Describe(TEXT("recursive context should remain reusable"))));
								ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("reused recursive context should not retain stale result state"))));
								Context->Release();
							}

							if (DepthCase.bConfiguredLimit)
							{
								ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_MAX_NESTED_CALLS, PreviousNestedLimit),
									*Case.Describe(TEXT("configured-limit cell should restore the engine property"))));
							}
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("recursive cell should discard its module"))));
					if (TypeCase.bValueObject && ExecutionDepth(DepthCase, OutcomeCase) > 0)
					{
						ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct) > 0,
							*Case.Describe(TEXT("value recursion should copy the tracked value across frames"))));
						ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Destruct) > 0,
							*Case.Describe(TEXT("value recursion should destroy tracked values during unwind"))));
						ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
							*Case.Describe(TEXT("value recursion should leave no live tracked values"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
