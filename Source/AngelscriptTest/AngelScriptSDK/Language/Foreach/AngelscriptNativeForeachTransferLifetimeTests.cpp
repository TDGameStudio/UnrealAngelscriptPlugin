#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FForeachTransferLifetimeTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Foreach.TransferLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FForeachExpectation
	{
		int32 ReturnValue = 0;
		int32 IteratorBegins = 0;
		int32 IteratorValues = 0;
		int32 IteratorNexts = 0;
		int32 NativeValueCopies = 0;
		int32 NativeValueDestructs = 0;
		bool bThrows = false;
	};

	inline static constexpr FNamedCase TransferCases[] =
	{
		{ "complete" },
		{ "break_first" },
		{ "break_middle" },
		{ "continue_first" },
		{ "continue_middle" },
		{ "return" },
		{ "exception" },
	};

	inline static constexpr FNamedCase NestingCases[] =
	{
		{ "single" },
		{ "nested_same" },
		{ "nested_distinct" },
		{ "inside_for" },
	};

	inline static constexpr FNamedCase ElementCases[] =
	{
		{ "primitive_value" },
		{ "value_object_copy" },
		{ "value_object_const_ref" },
	};

	inline static constexpr FNamedCase SizeCases[] =
	{
		{ "one" },
		{ "two" },
		{ "many" },
	};

	inline static constexpr FNamedCase MutationCases[] =
	{
		{ "stable" },
		{ "shrink_first" },
		{ "shrink_middle" },
		{ "clear_after_first" },
	};

	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static AngelscriptNativeTestSupport::FNativeLifecycleRecorder RegistrationLifecycle;
	inline static bool bTypesRegistered = false;

	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static int32 SizeValue(const FNamedCase& SizeCase)
	{
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

	static bool IsValueObject(const FNamedCase& ElementCase)
	{
		return !IsCase(ElementCase, "primitive_value");
	}

	static bool IsCopyValue(const FNamedCase& ElementCase)
	{
		return IsCase(ElementCase, "value_object_copy");
	}

	static int32 NestedRangeCount(const FNamedCase& NestingCase)
	{
		return IsCase(NestingCase, "nested_distinct") ? 2 : 1;
	}

	static int32 TransferVisitCount(const FNamedCase& TransferCase, const int32 Limit)
	{
		if (IsCase(TransferCase, "return") || IsCase(TransferCase, "exception"))
		{
			return FMath::Min(Limit, 1);
		}
		if (IsCase(TransferCase, "break_first"))
		{
			return FMath::Min(Limit, 1);
		}
		if (IsCase(TransferCase, "break_middle"))
		{
			return FMath::Min(Limit, 2);
		}
		return Limit;
	}

	static int32 TransferNextCount(const FNamedCase& TransferCase, const int32 Visits)
	{
		if (IsCase(TransferCase, "break_middle"))
		{
			return FMath::Max(0, Visits - 1);
		}
		if (IsCase(TransferCase, "complete")
			|| IsCase(TransferCase, "continue_first")
			|| IsCase(TransferCase, "continue_middle"))
		{
			return Visits;
		}
		return 0;
	}

	static void AppendTransferAction(
		FString& Source,
		const FNamedCase& TransferCase,
		const TCHAR* IterationName,
		const FString& Indent)
	{
		if (IsCase(TransferCase, "break_first"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sif (%s == 0)"), *Indent, IterationName));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tbreak;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(TransferCase, "break_middle"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sif (%s == 1)"), *Indent, IterationName));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tbreak;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(TransferCase, "continue_first"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sif (%s == 0)"), *Indent, IterationName));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tcontinue;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(TransferCase, "continue_middle"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sif (%s == 1)"), *Indent, IterationName));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tcontinue;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(TransferCase, "return"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("return Trace;"));
		}
		else if (IsCase(TransferCase, "exception"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("RaiseNativeCaseException();"));
		}
	}

	static FString VariableDeclaration(
		const FNamedCase& ElementCase,
		const TCHAR* VariableName)
	{
		if (IsCase(ElementCase, "primitive_value"))
		{
			return FString::Printf(TEXT("int %s"), VariableName);
		}
		if (IsCase(ElementCase, "value_object_const_ref"))
		{
			return FString::Printf(TEXT("const FNativeCaseValue& %s"), VariableName);
		}
		return FString::Printf(TEXT("FNativeCaseValue %s"), VariableName);
	}

	static void AppendRangeType(FString& Source, const FNamedCase& ElementCase)
	{
		AppendGeneratedAsLine(Source, TEXT("struct FForeachTransferRange"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseRange Native;"));
		if (IsValueObject(ElementCase))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Stored;"));
		}
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("\tint opForBegin()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Native.opForBegin();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tbool opForEnd(const int Iterator)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Native.opForEnd(Iterator);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tvoid opForNext(int& inout Iterator)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tNative.opForNext(Iterator);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);

		if (IsValueObject(ElementCase))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue& opForValue(const int Iterator)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tStored.Value = Native.opForValue(Iterator);"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Stored;"));
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

	static void AppendLoopBody(
		FString& Source,
		const FNamedCase& ElementCase,
		const FNamedCase& TransferCase,
		const TCHAR* RangeName,
		const TCHAR* IterationName,
		const TCHAR* VariableName,
		const TCHAR* Indent,
		const int32 Contribution)
	{
		const FString Prefix(Indent);
		AppendGeneratedAsLine(Source,
			Prefix + TEXT("foreach (") + VariableDeclaration(ElementCase, VariableName) + TEXT(" : ") + RangeName + TEXT(")"));
		AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
		AppendGeneratedAsLine(Source, Prefix + TEXT("\tint CurrentIteration = ") + IterationName + TEXT(";"));
		AppendGeneratedAsLine(Source, Prefix + TEXT("\t++") + IterationName + TEXT(";"));
		AppendGeneratedAsLine(Source, Prefix + FString::Printf(TEXT("\tTrace += %d;"), Contribution));
		if (IsValueObject(ElementCase))
		{
			AppendGeneratedAsLine(Source, Prefix + FString::Printf(TEXT("\tTrace += %s.Value - %s.Value;"), VariableName, VariableName));
		}
		else
		{
			AppendGeneratedAsLine(Source, Prefix + FString::Printf(TEXT("\tTrace += %s - %s;"), VariableName, VariableName));
		}
		AppendTransferAction(Source, TransferCase, TEXT("CurrentIteration"), Prefix + TEXT("\t"));
		AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
	}

	static FString BuildTransferSource(
		const FNamedCase& TransferCase,
		const FNamedCase& NestingCase,
		const FNamedCase& ElementCase)
	{
		FString Source;
		AppendRangeType(Source, ElementCase);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFForeachTransferRange Range;"));
		AppendGeneratedAsLine(Source, TEXT("\tRange.Native.Count = 3;"));
		if (IsCase(NestingCase, "nested_distinct"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFForeachTransferRange OtherRange;"));
			AppendGeneratedAsLine(Source, TEXT("\tOtherRange.Native.Count = 2;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));

		if (IsCase(NestingCase, "single"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Iteration = 0;"));
			AppendLoopBody(Source, ElementCase, TransferCase, TEXT("Range"), TEXT("Iteration"), TEXT("Value"), TEXT("\t"), 1);
		}
		else if (IsCase(NestingCase, "nested_same") || IsCase(NestingCase, "nested_distinct"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint OuterIteration = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tforeach (") + VariableDeclaration(ElementCase, TEXT("OuterValue")) + TEXT(" : Range)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tint CurrentOuterIteration = OuterIteration;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t++OuterIteration;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 100;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tint InnerIteration = 0;"));
			AppendLoopBody(Source, ElementCase, TransferCase,
				IsCase(NestingCase, "nested_same") ? TEXT("Range") : TEXT("OtherRange"),
				TEXT("InnerIteration"), TEXT("InnerValue"), TEXT("\t\t"), 1);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tint Iteration = 0;"));
			AppendLoopBody(Source, ElementCase, TransferCase, TEXT("Range"), TEXT("Iteration"), TEXT("Value"), TEXT("\t\t"), 1);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}

		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FForeachExpectation SimulateTransfer(
		const FNamedCase& TransferCase,
		const FNamedCase& NestingCase,
		const FNamedCase& ElementCase)
	{
		FForeachExpectation Result;
		const int32 OuterLimit = IsCase(NestingCase, "nested_same") || IsCase(NestingCase, "nested_distinct") ? 3 : 1;
		const int32 InnerLimit = IsCase(NestingCase, "single") || IsCase(NestingCase, "inside_for") || IsCase(NestingCase, "nested_same") ? 3 : 2;
		const bool bNested = IsCase(NestingCase, "nested_same") || IsCase(NestingCase, "nested_distinct");
		const bool bCopyValue = IsCopyValue(ElementCase);
		const int32 InnerVisits = TransferVisitCount(TransferCase, InnerLimit);
		const bool bStopsWholeFunction = IsCase(TransferCase, "return") || IsCase(TransferCase, "exception");

		if (IsCase(NestingCase, "single"))
		{
			Result.IteratorBegins = 1;
			Result.IteratorValues = InnerVisits;
			Result.IteratorNexts = TransferNextCount(TransferCase, InnerVisits);
			Result.ReturnValue = InnerVisits;
			Result.bThrows = IsCase(TransferCase, "exception");
		}
		else if (bNested)
		{
			const int32 ActualOuterVisits = bStopsWholeFunction ? 1 : OuterLimit;
			Result.IteratorBegins = 1 + ActualOuterVisits;
			Result.IteratorValues = ActualOuterVisits + ActualOuterVisits * InnerVisits;
			const int32 OuterNexts = bStopsWholeFunction ? 0 : ActualOuterVisits;
			const int32 InnerNexts = ActualOuterVisits * TransferNextCount(TransferCase, InnerVisits);
			Result.IteratorNexts = OuterNexts + InnerNexts;
			Result.ReturnValue = ActualOuterVisits * 100 + ActualOuterVisits * InnerVisits;
			Result.bThrows = IsCase(TransferCase, "exception");
		}
		else
		{
			const int32 OuterVisits = bStopsWholeFunction ? 1 : 2;
			Result.IteratorBegins = OuterVisits;
			Result.IteratorValues = OuterVisits * InnerVisits;
			Result.IteratorNexts = OuterVisits * TransferNextCount(TransferCase, InnerVisits);
			Result.ReturnValue = OuterVisits * 10 + Result.IteratorValues;
			Result.bThrows = IsCase(TransferCase, "exception");
		}

		if (bCopyValue)
		{
			Result.NativeValueCopies = Result.IteratorValues;
		}
		Result.NativeValueDestructs = Result.NativeValueCopies + NestedRangeCount(NestingCase);
		return Result;
	}

	static FString BuildStructuralMutationSource(
		const FNamedCase& SizeCase,
		const FNamedCase& MutationCase,
		const FNamedCase& ElementCase)
	{
		FString Source;
		AppendRangeType(Source, ElementCase);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFForeachTransferRange Range;"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tRange.Native.Count = %d;"), SizeValue(SizeCase)));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Iteration = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tforeach (") + VariableDeclaration(ElementCase, TEXT("Value")) + TEXT(" : Range)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint CurrentIteration = Iteration;"));
		AppendGeneratedAsLine(Source, TEXT("\t\t++Iteration;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1;"));
		if (IsCase(MutationCase, "shrink_first"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (CurrentIteration == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tRange.Native.Count = 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(MutationCase, "shrink_middle"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (CurrentIteration == 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tRange.Native.Count = 2;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(MutationCase, "clear_after_first"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (CurrentIteration == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tRange.Native.Count = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int32 ExpectedMutationVisits(
		const FNamedCase& SizeCase,
		const FNamedCase& MutationCase)
	{
		const int32 Size = SizeValue(SizeCase);
		if (IsCase(MutationCase, "stable"))
		{
			return Size;
		}
		if (IsCase(MutationCase, "shrink_first") || IsCase(MutationCase, "clear_after_first"))
		{
			return FMath::Min(Size, 1);
		}
		return FMath::Min(Size, 2);
	}

public:
	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		RegistrationLifecycle.Reset();
		asIScriptEngine* const ScriptEngine = Engine.Get();
		bTypesRegistered = ScriptEngine != nullptr
			&& RegisterNativeCaseValue(*ScriptEngine, RegistrationLifecycle)
			&& RegisterNativeCaseRange(*ScriptEngine, RegistrationLifecycle);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		bTypesRegistered = false;
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
	}

	TEST_METHOD(TransfersByNestingAndElementLifetime)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FE-TRANSFER-LIFETIME",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);
		FNoDiscardAsserter Assertions(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Foreach transfer product should create a raw SDK engine")))
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ScriptEngine->SetUserData(&Lifecycle, NativeLifecycleRecorderUserDataSlot);
		ScriptEngine->SetUserData(nullptr, NativeLifecycleFaultUserDataSlot);
		if (!Assertions.IsTrue(bTypesRegistered, TEXT("Foreach transfer should register its raw fixture types once per engine")))
		{
			return;
		}

		for (const FNamedCase& TransferCase : TransferCases)
		{
			for (const FNamedCase& NestingCase : NestingCases)
			{
				for (const FNamedCase& ElementCase : ElementCases)
				{
					Lifecycle.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-FE-TRANSFER-LIFETIME",
						{ ANSI_TO_TCHAR(TransferCase.CatalogName), ANSI_TO_TCHAR(NestingCase.CatalogName), ANSI_TO_TCHAR(ElementCase.CatalogName) }));
					const FString ModuleName = TEXT("ForeachTransferLifetime_") + Case.GetId().RightChop(25).Replace(TEXT("-"), TEXT("_"));
					const FString Source = BuildTransferSource(TransferCase, NestingCase, ElementCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					const FString BuildDescription = FString::Printf(TEXT("%s; result=%d messages=%s"),
						*Case.Describe(TEXT("foreach transfer/lifetime source should compile")), BuildResult, *Engine.GetMessagesText());
					const bool bBuildSucceeded = Assertions.AreEqual(asSUCCESS, BuildResult, *BuildDescription);
					asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
					const bool bEntryAvailable = Assertions.IsNotNull(Entry, *Case.Describe(TEXT("foreach transfer/lifetime should publish exact Entry declaration")));
					if (bBuildSucceeded && bEntryAvailable)
					{
						const FForeachExpectation Expected = SimulateTransfer(TransferCase, NestingCase, ElementCase);
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						const bool bContextAvailable = Assertions.IsNotNull(Context, *Case.Describe(TEXT("foreach transfer/lifetime should create a context")));
						if (bContextAvailable)
						{
							const int ExecutionResult = PrepareAndExecute(Context, Entry);
							const FString ExecutionDescription = FString::Printf(TEXT("%s; result=%d return=%d exception=%s"),
								*Case.Describe(TEXT("foreach transfer/lifetime should preserve the selected exit state")), ExecutionResult,
								static_cast<int32>(Context->GetReturnDWord()), UTF8_TO_TCHAR(Context->GetExceptionString()));
							(void)Assertions.AreEqual(Expected.bThrows ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED, ExecutionResult, *ExecutionDescription);
							if (Expected.bThrows)
							{
								(void)Assertions.AreEqual(FString(TEXT("foreach callback exception")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
									*Case.Describe(TEXT("foreach exception should retain the callback text")));
							}
							else
							{
								(void)Assertions.AreEqual(Expected.ReturnValue, static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("foreach transfer should return the exact simulated trace")));
							}
							Context->Release();
						}
						const FString LifecycleDescription = CollectNativeLifecycleEntries(Lifecycle);
						(void)Assertions.AreEqual(Expected.IteratorBegins, Lifecycle.Num(ENativeLifecycleEvent::IteratorBegin),
							*FString::Printf(TEXT("%s; expected=%d actual=%d events=%s"),
								*Case.Describe(TEXT("foreach transfer should begin the exact number of iterator scopes")),
								Expected.IteratorBegins, Lifecycle.Num(ENativeLifecycleEvent::IteratorBegin), *LifecycleDescription));
						(void)Assertions.AreEqual(Expected.IteratorValues, Lifecycle.Num(ENativeLifecycleEvent::IteratorValue),
							*FString::Printf(TEXT("%s; expected=%d actual=%d events=%s"),
								*Case.Describe(TEXT("foreach transfer should visit the exact number of elements")),
								Expected.IteratorValues, Lifecycle.Num(ENativeLifecycleEvent::IteratorValue), *LifecycleDescription));
						(void)Assertions.AreEqual(Expected.IteratorNexts, Lifecycle.Num(ENativeLifecycleEvent::IteratorNext),
							*FString::Printf(TEXT("%s; expected=%d actual=%d events=%s"),
								*Case.Describe(TEXT("foreach transfer should advance the iterator once per visited element")),
								Expected.IteratorNexts, Lifecycle.Num(ENativeLifecycleEvent::IteratorNext), *LifecycleDescription));
						if (IsValueObject(ElementCase))
						{
							(void)Assertions.AreEqual(Expected.NativeValueCopies, Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
								*FString::Printf(TEXT("%s; expected=%d actual=%d events=%s"),
									*Case.Describe(TEXT("foreach value binding should preserve the exact native copy count")),
									Expected.NativeValueCopies, Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct), *LifecycleDescription));
							(void)Assertions.AreEqual(Expected.NativeValueDestructs, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
								*FString::Printf(TEXT("%s; expected=%d actual=%d events=%s"),
									*Case.Describe(TEXT("foreach value binding should destroy every range and value object")),
									Expected.NativeValueDestructs, Lifecycle.Num(ENativeLifecycleEvent::Destruct), *LifecycleDescription));
						}
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("foreach transfer cell should discard its module")));
					(void)Assertions.AreEqual(0, Lifecycle.GetLiveObjectCount(), *Case.Describe(TEXT("foreach transfer should leave no tracked value alive")));
				}
			}
		}
	}

	TEST_METHOD(StructuralMutationBySizeAndElement)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FE-STRUCTURAL-MUTATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);
		FNoDiscardAsserter Assertions(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Foreach structural mutation product should create a raw SDK engine")))
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ScriptEngine->SetUserData(&Lifecycle, NativeLifecycleRecorderUserDataSlot);
		ScriptEngine->SetUserData(nullptr, NativeLifecycleFaultUserDataSlot);
		if (!Assertions.IsTrue(bTypesRegistered, TEXT("Foreach mutation should register its raw fixture types once per engine")))
		{
			return;
		}

		for (const FNamedCase& SizeCase : SizeCases)
		{
			for (const FNamedCase& MutationCase : MutationCases)
			{
				for (const FNamedCase& ElementCase : ElementCases)
				{
					Lifecycle.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-FE-STRUCTURAL-MUTATION",
						{ ANSI_TO_TCHAR(SizeCase.CatalogName), ANSI_TO_TCHAR(MutationCase.CatalogName), ANSI_TO_TCHAR(ElementCase.CatalogName) }));
					const FString ModuleName = TEXT("ForeachMutation_") + Case.GetId().RightChop(26).Replace(TEXT("-"), TEXT("_"));
					const FString Source = BuildStructuralMutationSource(SizeCase, MutationCase, ElementCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					const FString BuildDescription = FString::Printf(TEXT("%s; result=%d messages=%s"),
						*Case.Describe(TEXT("foreach structural mutation source should compile")), BuildResult, *Engine.GetMessagesText());
					const bool bBuildSucceeded = Assertions.AreEqual(asSUCCESS, BuildResult, *BuildDescription);
					asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
					const bool bEntryAvailable = Assertions.IsNotNull(Entry, *Case.Describe(TEXT("foreach mutation should publish exact Entry declaration")));
					if (bBuildSucceeded && bEntryAvailable)
					{
						const int32 ExpectedVisits = ExpectedMutationVisits(SizeCase, MutationCase);
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						const bool bContextAvailable = Assertions.IsNotNull(Context, *Case.Describe(TEXT("foreach mutation should create a context")));
						if (bContextAvailable)
						{
							const int ExecutionResult = PrepareAndExecute(Context, Entry);
							(void)Assertions.AreEqual(asEXECUTION_FINISHED, ExecutionResult,
								*Case.Describe(TEXT("foreach structural mutation should finish normally")));
							(void)Assertions.AreEqual(ExpectedVisits, static_cast<int32>(Context->GetReturnDWord()),
								*Case.Describe(TEXT("foreach structural mutation should return the exact live-count trace")));
							Context->Release();
						}
						(void)Assertions.AreEqual(1, Lifecycle.Num(ENativeLifecycleEvent::IteratorBegin),
							*Case.Describe(TEXT("foreach mutation should begin one iterator scope")));
						(void)Assertions.AreEqual(ExpectedVisits, Lifecycle.Num(ENativeLifecycleEvent::IteratorValue),
							*Case.Describe(TEXT("foreach mutation should observe the exact live element count")));
						(void)Assertions.AreEqual(ExpectedVisits, Lifecycle.Num(ENativeLifecycleEvent::IteratorNext),
							*Case.Describe(TEXT("foreach mutation should advance once for every observed element")));
						if (IsValueObject(ElementCase))
						{
							const int32 ExpectedCopies = IsCopyValue(ElementCase) ? ExpectedVisits : 0;
							(void)Assertions.AreEqual(ExpectedCopies, Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
								*Case.Describe(TEXT("foreach mutation should preserve the value binding copy count")));
							(void)Assertions.AreEqual(ExpectedCopies + 1, Lifecycle.Num(ENativeLifecycleEvent::Destruct),
								*Case.Describe(TEXT("foreach mutation should destroy the stored value and each copy")));
						}
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("foreach mutation cell should discard its module")));
					(void)Assertions.AreEqual(0, Lifecycle.GetLiveObjectCount(), *Case.Describe(TEXT("foreach mutation should leave no tracked value alive")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
