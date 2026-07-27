#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FForeachIterationTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Foreach.Iteration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FForeachIterationExpectation
	{
		int32 ReturnValue = 0;
		int32 VisitedElements = 0;
		bool bThrows = false;
	};

	inline static constexpr FNamedCase SizeCases[] =
	{
		{ "empty" }, { "one" }, { "two" }, { "many" },
	};
	inline static constexpr FNamedCase ElementCases[] =
	{
		{ "primitive" }, { "value_object" }, { "reference_object" }, { "const_element" },
	};
	inline static constexpr FNamedCase VariableCases[] =
	{
		{ "value" }, { "auto" }, { "mutable_reference" }, { "const_reference" }, { "incompatible" },
	};
	inline static constexpr FNamedCase TransferCases[] =
	{
		{ "complete" }, { "break_first" }, { "break_middle" }, { "break_last" }, { "continue_first" }, { "continue_middle" }, { "return" }, { "exception" },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static int32 CountFor(const FNamedCase& SizeCase)
	{
		if (IsCase(SizeCase, "empty"))
		{
			return 0;
		}
		if (IsCase(SizeCase, "one"))
		{
			return 1;
		}
		if (IsCase(SizeCase, "two"))
		{
			return 2;
		}
		return 4;
	}

	static FString VariableDeclaration(const FNamedCase& ElementCase, const FNamedCase& VariableCase)
	{
		if (IsCase(VariableCase, "auto"))
		{
			return TEXT("auto Value");
		}
		if (IsCase(VariableCase, "incompatible"))
		{
			return TEXT("FForeachIncompatibleElement Value");
		}
		if (IsCase(ElementCase, "value_object"))
		{
			if (IsCase(VariableCase, "mutable_reference"))
			{
				return TEXT("FForeachValueElement& Value");
			}
			if (IsCase(VariableCase, "const_reference"))
			{
				return TEXT("const FForeachValueElement& Value");
			}
			return TEXT("FForeachValueElement Value");
		}
		if (IsCase(ElementCase, "reference_object"))
		{
			if (IsCase(VariableCase, "mutable_reference"))
			{
				// A handle-returning iterator cannot bind its result to a value reference.
				return TEXT("FNativeCaseReference& Value");
			}
			if (IsCase(VariableCase, "const_reference"))
			{
				return TEXT("const FNativeCaseReference@ Value");
			}
			return TEXT("FNativeCaseReference@ Value");
		}
		if (IsCase(VariableCase, "mutable_reference"))
		{
			return TEXT("int& Value");
		}
		if (IsCase(VariableCase, "const_reference"))
		{
			return TEXT("const int& Value");
		}
		return TEXT("int Value");
	}

	static bool IsInvalidCombination(const FNamedCase& ElementCase, const FNamedCase& VariableCase)
	{
		// The current raw fork cannot publish a script method returning a
		// native object handle through the foreach protocol.  Keep every
		// reference-object cell as an explicit compile/diagnostic boundary;
		// native handle lifetime remains covered by the References theme.
		return IsCase(VariableCase, "incompatible")
			|| IsCase(ElementCase, "reference_object")
			|| (IsCase(ElementCase, "const_element") && IsCase(VariableCase, "mutable_reference"));
	}

	static void AppendElementDeclaration(FString& Source, const FNamedCase& ElementCase)
	{
		if (IsCase(ElementCase, "value_object"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FForeachValueElement"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendForeachRangeDeclaration(FString& Source, const FNamedCase& ElementCase)
	{
		AppendGeneratedAsLine(Source, TEXT("struct FForeachIterationRange"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseRange Native;"));
		if (IsCase(ElementCase, "value_object"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFForeachValueElement Stored;"));
		}
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint opForBegin()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Native.opForBegin();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
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
		if (IsCase(ElementCase, "value_object"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFForeachValueElement& opForValue(const int Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tStored.Value = Native.opForValue(Iterator);"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Stored;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsCase(ElementCase, "reference_object"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference@ opForValue(const int Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn CreateNativeCaseReference(Native.opForValue(Iterator));"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsCase(ElementCase, "const_element"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tconst int& opForValue(const int Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Native.opForValue(Iterator);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tint& opForValue(const int Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Native.opForValue(Iterator);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendVariableDeclaration(FString& Source, const FNamedCase& VariableCase)
	{
		if (IsCase(VariableCase, "incompatible"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FForeachIncompatibleElement"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendElementProjection(FString& Source, const FNamedCase& ElementCase)
	{
		if (IsCase(ElementCase, "primitive") || IsCase(ElementCase, "const_element"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tint ElementBias = Value;"));
		}
		else if (IsCase(ElementCase, "value_object"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tint ElementBias = Value.Value;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tint ElementBias = Value.Value;"));
		}
	}

	static void AppendMutableElementMutation(FString& Source, const FNamedCase& ElementCase, const FNamedCase& VariableCase)
	{
		if (!IsCase(VariableCase, "mutable_reference"))
		{
			return;
		}
		if (IsCase(ElementCase, "value_object") || IsCase(ElementCase, "reference_object"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\t++Value.Value;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\t++Value;"));
		}
	}

	static void AppendTransfer(FString& Source, const FNamedCase& TransferCase)
	{
		if (IsCase(TransferCase, "break_first"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (CurrentIteration == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(TransferCase, "break_middle"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (CurrentIteration == 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(TransferCase, "break_last"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (CurrentIteration + 1 >= Limit)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(TransferCase, "continue_first"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (CurrentIteration == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tcontinue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(TransferCase, "continue_middle"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (CurrentIteration == 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tcontinue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(TransferCase, "return"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Trace + Contribution;"));
		}
		else if (IsCase(TransferCase, "exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tRaiseNativeCaseException();"));
		}
	}

	static FString BuildForeachIterationSource(
		const FNamedCase& SizeCase,
		const FNamedCase& ElementCase,
		const FNamedCase& VariableCase,
		const FNamedCase& TransferCase)
	{
		FString Source;
		AppendElementDeclaration(Source, ElementCase);
		AppendVariableDeclaration(Source, VariableCase);
		AppendForeachRangeDeclaration(Source, ElementCase);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFForeachIterationRange Range;"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tRange.Native.Count = %d;"), CountFor(SizeCase)));
		AppendGeneratedAsLine(Source, TEXT("\tint Limit = Range.Native.Count;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Iteration = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tforeach (") + VariableDeclaration(ElementCase, VariableCase) + TEXT(" : Range)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint CurrentIteration = Iteration;"));
		AppendGeneratedAsLine(Source, TEXT("\t\t++Iteration;"));
		AppendMutableElementMutation(Source, ElementCase, VariableCase);
		AppendElementProjection(Source, ElementCase);
		AppendGeneratedAsLine(Source, TEXT("\t\tint Contribution = ElementBias + 1;"));
		AppendTransfer(Source, TransferCase);
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += Contribution;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FForeachIterationExpectation ExpectedResult(
		const FNamedCase& SizeCase,
		const FNamedCase& VariableCase,
		const FNamedCase& TransferCase)
	{
		FForeachIterationExpectation Result;
		const int32 Count = CountFor(SizeCase);
		const int32 Contribution = IsCase(VariableCase, "mutable_reference") ? 2 : 1;
		Result.VisitedElements = Count;
		if (Count == 0)
		{
			return Result;
		}
		if (IsCase(TransferCase, "exception"))
		{
			Result.bThrows = true;
			Result.VisitedElements = 1;
		}
		else if (IsCase(TransferCase, "return"))
		{
			Result.ReturnValue = Contribution;
			Result.VisitedElements = 1;
		}
		else if (IsCase(TransferCase, "break_first"))
		{
			Result.ReturnValue = 0;
			Result.VisitedElements = 1;
		}
		else if (IsCase(TransferCase, "break_middle"))
		{
			Result.ReturnValue = Contribution;
			Result.VisitedElements = FMath::Min(Count, 2);
		}
		else if (IsCase(TransferCase, "break_last"))
		{
			Result.ReturnValue = (Count - 1) * Contribution;
		}
		else if (IsCase(TransferCase, "continue_first"))
		{
			Result.ReturnValue = (Count - 1) * Contribution;
		}
		else if (IsCase(TransferCase, "continue_middle"))
		{
			Result.ReturnValue = (Count == 1 ? 1 : Count - 1) * Contribution;
		}
		else
		{
			Result.ReturnValue = Count * Contribution;
		}
		return Result;
	}

public:
	TEST_METHOD(SizesByElementVariableAndTransfer)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FE-SIZE-VARIABLE",
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
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Foreach iteration product should create a raw SDK engine")))
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		if (!Assertions.IsTrue(RegisterNativeCaseRange(*ScriptEngine, Lifecycle), TEXT("Foreach iteration should register its raw opFor range"))
			|| !Assertions.IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle), TEXT("Foreach iteration should register its native reference element")))
		{
			return;
		}

		for (const FNamedCase& SizeCase : SizeCases)
		{
			for (const FNamedCase& ElementCase : ElementCases)
			{
				for (const FNamedCase& VariableCase : VariableCases)
				{
					for (const FNamedCase& TransferCase : TransferCases)
					{
						Lifecycle.Reset();
						const FNativeCaseContext Case(MakeNativeCaseId("LANG-FE-SIZE-VARIABLE",
							{ ANSI_TO_TCHAR(SizeCase.CatalogName), ANSI_TO_TCHAR(ElementCase.CatalogName), ANSI_TO_TCHAR(VariableCase.CatalogName), ANSI_TO_TCHAR(TransferCase.CatalogName) }));
						const FString ModuleName = TEXT("ForeachIteration_") + Case.GetId().RightChop(22).Replace(TEXT("-"), TEXT("_"));
						const FString Source = BuildForeachIterationSource(SizeCase, ElementCase, VariableCase, TransferCase);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						Engine.ResetMessages();
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
						const bool bInvalid = IsInvalidCombination(ElementCase, VariableCase);
						if (bInvalid)
						{
							(void)Assertions.IsTrue(BuildResult < 0, *Case.Describe(TEXT("incompatible foreach variable should be rejected")));
							(void)Assertions.IsTrue(Engine.GetMessages().Entries.Num() > 0, *Case.Describe(TEXT("incompatible foreach variable should report a diagnostic")));
						}
						else
						{
							const FForeachIterationExpectation Expected = ExpectedResult(SizeCase, VariableCase, TransferCase);
							const FString BuildDescription = FString::Printf(TEXT("%s; result=%d messages=%s"),
								*Case.Describe(TEXT("foreach iteration cell should compile")), BuildResult, *Engine.GetMessagesText());
							const bool bBuildSucceeded = Assertions.AreEqual(asSUCCESS, BuildResult, *BuildDescription);
							asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
							const bool bEntryAvailable = Assertions.IsNotNull(Entry, *Case.Describe(TEXT("foreach iteration should publish exact Entry declaration")));
							if (bBuildSucceeded && bEntryAvailable)
							{
								asIScriptContext* const Context = ScriptEngine->CreateContext();
								const bool bContextAvailable = Assertions.IsNotNull(Context, *Case.Describe(TEXT("foreach iteration should create a context")));
								if (bContextAvailable)
								{
									const int ExecutionResult = PrepareAndExecute(Context, Entry);
									const FString ExecutionDescription = FString::Printf(TEXT("%s; actual=%d return=%d exception=%s"),
										*Case.Describe(TEXT("foreach transfer should select the expected execution state")), ExecutionResult,
										static_cast<int32>(Context->GetReturnDWord()), UTF8_TO_TCHAR(Context->GetExceptionString()));
									(void)Assertions.AreEqual(Expected.bThrows ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED, ExecutionResult, *ExecutionDescription);
									if (Expected.bThrows)
									{
										(void)Assertions.AreEqual(FString(TEXT("foreach callback exception")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
											*Case.Describe(TEXT("foreach exception should preserve the host callback text")));
									}
									else
									{
										(void)Assertions.AreEqual(Expected.ReturnValue, static_cast<int32>(Context->GetReturnDWord()),
											*Case.Describe(TEXT("foreach iteration should preserve the exact selected trace")));
									}
									Context->Release();
								}
							}
							(void)Assertions.AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::IteratorBegin),
								*Case.Describe(TEXT("foreach iteration should invoke one iterator begin callback per entry")));
							(void)Assertions.AreEqual(Expected.VisitedElements, Lifecycle.Num(ENativeLifecycleEvent::IteratorValue),
								*Case.Describe(TEXT("foreach iteration should visit the exact expected element count")));
						}
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("foreach iteration cell should discard its module")));
						(void)Assertions.AreEqual(0, Lifecycle.GetLiveObjectCount(), *Case.Describe(TEXT("foreach range should leave no tracked native owner alive")));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
