#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNestedTargetTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.NestedTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FNamedCase NestingCases[] =
	{
		{ "nested_loop" },
		{ "branch_loop" },
		{ "three_level" },
	};

	inline static constexpr FNamedCase TransferCases[] =
	{
		{ "break" },
		{ "continue" },
		{ "return" },
	};

	inline static constexpr FNamedCase TargetCases[] =
	{
		{ "inner" },
		{ "outer" },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static void AppendTransfer(
		FString& Source,
		const FNamedCase& TransferCase,
		const FNamedCase& TargetCase,
		const FString& Indent)
	{
		if (!IsCase(TargetCase, "inner"))
		{
			return;
		}

		if (IsCase(TransferCase, "break"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsCase(TransferCase, "continue"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("continue;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("return Trace;"));
		}
	}

	static void AppendOuterTransfer(
		FString& Source,
		const FNamedCase& TransferCase,
		const FNamedCase& TargetCase)
	{
		if (!IsCase(TargetCase, "outer"))
		{
			return;
		}

		if (IsCase(TransferCase, "break"))
		{
			AppendGeneratedAsLine(Source, TEXT("		break;"));
		}
		else if (IsCase(TransferCase, "continue"))
		{
			AppendGeneratedAsLine(Source, TEXT("		continue;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("		return Trace;"));
		}
	}

	static FString BuildNestedSource(
		const FNamedCase& NestingCase,
		const FNamedCase& TransferCase,
		const FNamedCase& TargetCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("	int Trace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("	for (int Outer = 0; Outer < 2; ++Outer)"));
		AppendGeneratedAsLine(Source, TEXT("	{"));
		AppendGeneratedAsLine(Source, TEXT("		Trace += 1;"));

		if (IsCase(NestingCase, "nested_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("		for (int Inner = 0; Inner < 2; ++Inner)"));
			AppendGeneratedAsLine(Source, TEXT("		{"));
			AppendGeneratedAsLine(Source, TEXT("			Trace += 10;"));
			AppendTransfer(Source, TransferCase, TargetCase, TEXT("			"));
			AppendGeneratedAsLine(Source, TEXT("		}"));
		}
		else if (IsCase(NestingCase, "branch_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("		if (Outer >= 0)"));
			AppendGeneratedAsLine(Source, TEXT("		{"));
			AppendGeneratedAsLine(Source, TEXT("			Trace += 2;"));
			AppendGeneratedAsLine(Source, TEXT("			for (int Inner = 0; Inner < 2; ++Inner)"));
			AppendGeneratedAsLine(Source, TEXT("			{"));
			AppendGeneratedAsLine(Source, TEXT("				Trace += 10;"));
			AppendTransfer(Source, TransferCase, TargetCase, TEXT("				"));
			AppendGeneratedAsLine(Source, TEXT("			}"));
			AppendGeneratedAsLine(Source, TEXT("		}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("		if (Outer >= 0)"));
			AppendGeneratedAsLine(Source, TEXT("		{"));
			AppendGeneratedAsLine(Source, TEXT("			for (int Middle = 0; Middle < 2; ++Middle)"));
			AppendGeneratedAsLine(Source, TEXT("			{"));
			AppendGeneratedAsLine(Source, TEXT("				Trace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("				for (int Inner = 0; Inner < 2; ++Inner)"));
			AppendGeneratedAsLine(Source, TEXT("				{"));
			AppendGeneratedAsLine(Source, TEXT("					Trace += 100;"));
			AppendTransfer(Source, TransferCase, TargetCase, TEXT("					"));
			AppendGeneratedAsLine(Source, TEXT("				}"));
			AppendGeneratedAsLine(Source, TEXT("			}"));
			AppendGeneratedAsLine(Source, TEXT("		}"));
		}

		AppendGeneratedAsLine(Source, TEXT("		Trace += 1000;"));
		AppendOuterTransfer(Source, TransferCase, TargetCase);
		AppendGeneratedAsLine(Source, TEXT("		Trace += 10000;"));
		AppendGeneratedAsLine(Source, TEXT("	}"));
		AppendGeneratedAsLine(Source, TEXT("	return Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int32 SimulateExpectedTrace(
		const FNamedCase& NestingCase,
		const FNamedCase& TransferCase,
		const FNamedCase& TargetCase)
	{
		int32 Trace = 0;
		for (int32 Outer = 0; Outer < 2; ++Outer)
		{
			Trace += 1;
			if (IsCase(NestingCase, "nested_loop"))
			{
				for (int32 Inner = 0; Inner < 2; ++Inner)
				{
					Trace += 10;
					if (IsCase(TargetCase, "inner"))
					{
						if (IsCase(TransferCase, "break"))
						{
							break;
						}
						if (IsCase(TransferCase, "continue"))
						{
							continue;
						}
						return Trace;
					}
				}
			}
			else if (IsCase(NestingCase, "branch_loop"))
			{
				Trace += 2;
				for (int32 Inner = 0; Inner < 2; ++Inner)
				{
					Trace += 10;
					if (IsCase(TargetCase, "inner"))
					{
						if (IsCase(TransferCase, "break"))
						{
							break;
						}
						if (IsCase(TransferCase, "continue"))
						{
							continue;
						}
						return Trace;
					}
				}
			}
			else
			{
				for (int32 Middle = 0; Middle < 2; ++Middle)
				{
					Trace += 10;
					for (int32 Inner = 0; Inner < 2; ++Inner)
					{
						Trace += 100;
						if (IsCase(TargetCase, "inner"))
						{
							if (IsCase(TransferCase, "break"))
							{
								break;
							}
							if (IsCase(TransferCase, "continue"))
							{
								continue;
							}
							return Trace;
						}
					}
				}
			}

			Trace += 1000;
			if (IsCase(TargetCase, "outer"))
			{
				if (IsCase(TransferCase, "break"))
				{
					break;
				}
				if (IsCase(TransferCase, "continue"))
				{
					continue;
				}
				return Trace;
			}
			Trace += 10000;
		}
		return Trace;
	}

public:
	TEST_METHOD(TransfersByNestingAndTarget)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-NESTED-TARGETS",
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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Nested-target product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FNamedCase& NestingCase : NestingCases)
		{
			for (const FNamedCase& TransferCase : TransferCases)
			{
				for (const FNamedCase& TargetCase : TargetCases)
				{
					++ObservedCaseCount;
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-CF-NESTED-TARGETS",
						{
							ANSI_TO_TCHAR(NestingCase.CatalogName),
							ANSI_TO_TCHAR(TransferCase.CatalogName),
							ANSI_TO_TCHAR(TargetCase.CatalogName),
						}));
					const FString ModuleName = FString::Printf(
						TEXT("NestedTargets_%s_%s_%s"),
						ANSI_TO_TCHAR(NestingCase.CatalogName),
						ANSI_TO_TCHAR(TransferCase.CatalogName),
						ANSI_TO_TCHAR(TargetCase.CatalogName));
					const FString Source = BuildNestedSource(NestingCase, TransferCase, TargetCase);
					const int32 ExpectedTrace = SimulateExpectedTrace(NestingCase, TransferCase, TargetCase);
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
						*Case.Describe(TEXT("nested transfer source should compile for the selected shape, transfer, and target"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("nested transfer source should publish a module"))));

					if (BuildResult >= 0 && Module != nullptr)
					{
						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
						ASSERT_THAT(IsNotNull(Entry,
							*Case.Describe(TEXT("nested transfer source should publish the exact Entry declaration"))));
						if (Entry != nullptr)
						{
							asUINT BytecodeLength = 0;
							Entry->GetByteCode(&BytecodeLength);
							ASSERT_THAT(IsTrue(BytecodeLength > 0,
								*Case.Describe(TEXT("nested transfer entry should retain executable bytecode"))));

							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context,
								*Case.Describe(TEXT("nested transfer source should create an execution context"))));
							if (Context != nullptr)
							{
								const int ExecuteResult = PrepareAndExecute(Context, Entry);
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
									*Case.Describe(TEXT("nested transfer execution should finish without an exception"))));
								ASSERT_THAT(AreEqual(ExpectedTrace, static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("nested transfer should preserve the exact inner or outer target trace"))));
								Context->Release();
							}
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("nested transfer module should be discarded after the selected cell"))));
				}
			}
		}

		ASSERT_THAT(AreEqual(UE_ARRAY_COUNT(NestingCases) * UE_ARRAY_COUNT(TransferCases) * UE_ARRAY_COUNT(TargetCases),
			ObservedCaseCount,
			TEXT("LANG-CF-NESTED-TARGETS must execute every nesting, transfer, and target cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
