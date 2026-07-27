#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeCallstackTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.Callstack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	inline static constexpr asPWORD CallstackRecorderUserDataSlot = static_cast<asPWORD>(0x4E41544453544143ull);

	struct FFrameObservation
	{
		FString Declaration;
		FString Section;
		int32 Line = INDEX_NONE;
		int32 Column = INDEX_NONE;
		asUINT BlueprintFrame = 0;
		void* StackFrame = nullptr;
		int32 StackFrameSize = 0;
	};

	struct FCallstackObservation
	{
		TArray<FFrameObservation> Frames;
		FString TargetDeclaration;
		int32 MinimumDepth = 0;
		bool bOutOfRangeCaptured = false;
		asIScriptFunction* OutOfRangeFunction = nullptr;
		int32 OutOfRangeLine = INDEX_NONE;
		void* OutOfRangeStackFrame = nullptr;
	};

	static FString BuildSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int CallstackLeaf(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Local = Value + 1;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Local;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallstackMiddle(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn CallstackLeaf(Value + 1);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallstackRoot()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn CallstackMiddle(40);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallstackRecursive(int Remaining)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Remaining == 0)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\treturn 1 + CallstackRecursive(Remaining - 1);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallstackDeepRoot()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn CallstackRecursive(8);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallstackFaultLeaf()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallstackFaultMiddle()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn CallstackFaultLeaf();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CallstackFaultRoot()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn CallstackFaultMiddle();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static void CaptureCallstack(asCContext* Context)
	{
		if (Context == nullptr)
		{
			return;
		}
		FCallstackObservation* const Observation = static_cast<FCallstackObservation*>(Context->GetUserData(CallstackRecorderUserDataSlot));
		asIScriptFunction* const CurrentFunction = Context->GetFunction(0);
		if (Observation == nullptr
			|| CurrentFunction == nullptr
			|| Observation->TargetDeclaration.IsEmpty()
			|| Observation->TargetDeclaration != UTF8_TO_TCHAR(CurrentFunction->GetDeclaration())
			|| static_cast<int32>(Context->GetCallstackSize()) < Observation->MinimumDepth)
		{
			return;
		}

		Observation->Frames.Reset();
		for (asUINT StackLevel = 0; StackLevel < Context->GetCallstackSize(); ++StackLevel)
		{
			FFrameObservation Frame;
			asIScriptFunction* const Function = Context->GetFunction(StackLevel);
			if (Function != nullptr)
			{
				Frame.Declaration = UTF8_TO_TCHAR(Function->GetDeclaration());
			}
			const char* Section = nullptr;
			Frame.Line = Context->GetLineNumber(StackLevel, &Frame.Column, &Section);
			Frame.Section = UTF8_TO_TCHAR(Section != nullptr ? Section : "");
			Frame.BlueprintFrame = Context->GetBlueprintCallstackFrame(StackLevel);
			Frame.StackFrame = Context->GetStackFrame(StackLevel);
			Frame.StackFrameSize = Context->GetStackFrameSize(StackLevel);
			Observation->Frames.Add(MoveTemp(Frame));
		}

		const asUINT OutOfRangeLevel = Context->GetCallstackSize();
		Observation->OutOfRangeFunction = Context->GetFunction(OutOfRangeLevel);
		Observation->OutOfRangeLine = Context->GetLineNumber(OutOfRangeLevel, nullptr, nullptr);
		Observation->OutOfRangeStackFrame = Context->GetStackFrame(OutOfRangeLevel);
		Observation->bOutOfRangeCaptured = true;
	}

public:
	TEST_METHOD(DepthsByStateIndexAndQuery)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-STACK-FRAME-QUERY",
			ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		AS_NATIVE_PRODUCT("DBG-STACK-FRAME-BOUNDARIES",
			ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		FScopedNativeDebugCallbacks DebugCallbacks;
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Callstack product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = TEXT("NativeDebugCallstack");
		const FString Source = BuildSource();
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		PrintGeneratedAsSource(*TestRunner, TEXT("DBG-STACK-FRAME-QUERY"), ModuleName, Source);
		PrintGeneratedAsSource(*TestRunner, TEXT("DBG-STACK-FRAME-BOUNDARIES"), ModuleName, Source);
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult >= 0, TEXT("Callstack source should compile")));
		ASSERT_THAT(IsNotNull(Module, TEXT("Callstack source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		};

		asIScriptFunction* const Leaf = GetNativeFunctionByExactDecl(Module, "int CallstackLeaf(const int)");
		asIScriptFunction* const Middle = GetNativeFunctionByExactDecl(Module, "int CallstackMiddle(const int)");
		asIScriptFunction* const Root = GetNativeFunctionByExactDecl(Module, "int CallstackRoot()");
		asIScriptFunction* const DeepRoot = GetNativeFunctionByExactDecl(Module, "int CallstackDeepRoot()");
		asIScriptFunction* const FaultRoot = GetNativeFunctionByExactDecl(Module, "int CallstackFaultRoot()");
		ASSERT_THAT(IsNotNull(Leaf, TEXT("Callstack product should resolve the one-frame leaf exactly")));
		ASSERT_THAT(IsNotNull(Middle, TEXT("Callstack product should resolve the two-frame middle exactly")));
		ASSERT_THAT(IsNotNull(Root, TEXT("Callstack product should resolve the normal root exactly")));
		ASSERT_THAT(IsNotNull(DeepRoot, TEXT("Callstack product should resolve the deep recursive root exactly")));
		ASSERT_THAT(IsNotNull(FaultRoot, TEXT("Callstack product should resolve the exception root exactly")));
		if (Leaf == nullptr || Middle == nullptr || Root == nullptr || DeepRoot == nullptr || FaultRoot == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Callstack product should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		asCContext* const RawContext = static_cast<asCContext*>(Context);

		FCallstackObservation Observation;
		auto AssertFrameBoundaries = [this, &Observation](const TCHAR* Shape, int32 ExpectedDepth)
		{
			ASSERT_THAT(AreEqual(ExpectedDepth, Observation.Frames.Num(),
				FString::Printf(TEXT("%s should expose the expected active frame depth"), Shape)));
			if (Observation.Frames.Num() != ExpectedDepth || ExpectedDepth <= 0)
			{
				return;
			}

			struct FBoundaryQuery
			{
				const TCHAR* Name;
				int32 Level;
			};
			const FBoundaryQuery Queries[] =
			{
				{ TEXT("first"), 0 },
				{ TEXT("last"), ExpectedDepth - 1 },
				{ TEXT("out_of_range"), ExpectedDepth },
			};
			for (const FBoundaryQuery& Query : Queries)
			{
				if (Query.Level < ExpectedDepth)
				{
					const FFrameObservation& Frame = Observation.Frames[Query.Level];
					ASSERT_THAT(IsNotNull(Frame.StackFrame,
						FString::Printf(TEXT("%s %s frame should expose a concrete frame pointer"), Shape, Query.Name)));
					ASSERT_THAT(IsTrue(Frame.StackFrameSize > 0,
						FString::Printf(TEXT("%s %s frame should expose a positive frame size"), Shape, Query.Name)));
					ASSERT_THAT(IsTrue(Frame.Line > 0,
						FString::Printf(TEXT("%s %s frame should expose a source line"), Shape, Query.Name)));
				}
				else
				{
					ASSERT_THAT(IsTrue(Observation.bOutOfRangeCaptured,
						FString::Printf(TEXT("%s out-of-range query should be captured"), Shape)));
					ASSERT_THAT(IsNull(Observation.OutOfRangeFunction,
						FString::Printf(TEXT("%s out-of-range function query should be null"), Shape)));
					ASSERT_THAT(AreEqual(asINVALID_ARG, Observation.OutOfRangeLine,
						FString::Printf(TEXT("%s out-of-range line query should return invalid"), Shape)));
					ASSERT_THAT(IsNull(Observation.OutOfRangeStackFrame,
						FString::Printf(TEXT("%s out-of-range frame query should be null"), Shape)));
				}
			}
		};

		Observation.TargetDeclaration = TEXT("int CallstackLeaf(const int)");
		Observation.MinimumDepth = 1;
		Context->SetUserData(&Observation, CallstackRecorderUserDataSlot);
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureCallstack), TEXT("Callstack product should install its raw line observer")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Leaf), TEXT("Callstack one-frame path should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 41), TEXT("Callstack one-frame path should set its argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Callstack one-frame path should finish")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Callstack one-frame path should retain its leaf result")));
		ASSERT_THAT(AreEqual(1, Observation.Frames.Num(), TEXT("Callstack one-frame callback should expose only the leaf")));
		AssertFrameBoundaries(TEXT("one-frame"), 1);
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Callstack one-frame path should unprepare before the two-frame path")));

		Observation.Frames.Reset();
		Observation.MinimumDepth = 2;
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Middle), TEXT("Callstack two-frame path should prepare")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 40), TEXT("Callstack two-frame path should set its argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Callstack two-frame path should finish")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Callstack two-frame path should retain its middle result")));
		ASSERT_THAT(AreEqual(2, Observation.Frames.Num(), TEXT("Callstack two-frame callback should expose leaf and middle")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Callstack two-frame path should unprepare before the three-frame path")));

		Observation.Frames.Reset();
		Observation.MinimumDepth = 3;
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Root), TEXT("Callstack normal path should finish")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Callstack normal path should traverse all three frames")));
		ASSERT_THAT(AreEqual(3, Observation.Frames.Num(), TEXT("Callstack leaf observation should contain the exact three script frames")));
		AssertFrameBoundaries(TEXT("three-frame"), 3);
		if (Observation.Frames.Num() == 3)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("int CallstackLeaf(const int)")), Observation.Frames[0].Declaration, TEXT("Callstack current frame should be the leaf")));
			ASSERT_THAT(AreEqual(FString(TEXT("int CallstackMiddle(const int)")), Observation.Frames[1].Declaration, TEXT("Callstack middle frame should be retained")));
			ASSERT_THAT(AreEqual(FString(TEXT("int CallstackRoot()")), Observation.Frames[2].Declaration, TEXT("Callstack root frame should be retained")));
			for (const FFrameObservation& Frame : Observation.Frames)
			{
				ASSERT_THAT(IsTrue(Frame.Line > 0, TEXT("Callstack valid frames should expose a source line")));
				ASSERT_THAT(AreEqual(ModuleName, Frame.Section, TEXT("Callstack valid frames should expose their source section")));
				ASSERT_THAT(AreEqual(0, static_cast<int32>(Frame.BlueprintFrame), TEXT("Raw SDK callstack frames should not fabricate a Blueprint frame")));
				ASSERT_THAT(IsNotNull(Frame.StackFrame, TEXT("Callstack valid frames should expose a concrete stack-frame pointer")));
				ASSERT_THAT(IsTrue(Frame.StackFrameSize > 0, TEXT("Callstack valid frames should expose a non-zero frame size")));
			}
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Callstack normal path should unprepare before the exception path")));

		Observation.Frames.Reset();
		Observation.TargetDeclaration = TEXT("int CallstackRecursive(const int)");
		Observation.MinimumDepth = 10;
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(DeepRoot), TEXT("Callstack deep recursive path should prepare")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Callstack deep recursive path should finish")));
		ASSERT_THAT(AreEqual(9, static_cast<int32>(Context->GetReturnDWord()), TEXT("Callstack deep recursive path should retain its selected depth result")));
		ASSERT_THAT(IsTrue(Observation.Frames.Num() >= 10, TEXT("Callstack deep recursive callback should expose every active frame at the selected depth")));
		if (Observation.Frames.Num() >= 10)
		{
			AssertFrameBoundaries(TEXT("recursive-depth"), Observation.Frames.Num());
		}
		if (Observation.Frames.Num() >= 10)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("int CallstackRecursive(const int)")), Observation.Frames[0].Declaration, TEXT("Callstack deep recursive current frame should retain the recursive declaration")));
			ASSERT_THAT(AreEqual(FString(TEXT("int CallstackDeepRoot()")), Observation.Frames.Last().Declaration, TEXT("Callstack deep recursive root frame should retain its entry declaration")));
			for (const FFrameObservation& Frame : Observation.Frames)
			{
				ASSERT_THAT(IsTrue(Frame.Line > 0, TEXT("Callstack deep recursive frame should expose a source line")));
				ASSERT_THAT(IsNotNull(Frame.StackFrame, TEXT("Callstack deep recursive frame should expose a concrete frame pointer")));
				ASSERT_THAT(IsTrue(Frame.StackFrameSize > 0, TEXT("Callstack deep recursive frame should expose a non-zero frame size")));
			}
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Callstack deep recursive path should unprepare before the exception path")));

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(FaultRoot), TEXT("Callstack exception path should prepare")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Context->Execute(), TEXT("Callstack exception path should stop at the fault")));
		ASSERT_THAT(IsFalse(RawContext->WillExceptionBeCaught(), TEXT("Callstack uncaught exception should directly report that no script handler will catch it")));
		const asUINT ExceptionDepth = Context->GetCallstackSize();
		ASSERT_THAT(IsTrue(ExceptionDepth >= 3, TEXT("Callstack exception should retain its nested script frames")));
		ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(), TEXT("Callstack exception should expose its fault function")));
		ASSERT_THAT(IsTrue(Context->GetExceptionString() != nullptr && Context->GetExceptionString()[0] != '\0', TEXT("Callstack exception should expose diagnostic text")));
		const char* ExceptionSection = nullptr;
		const int ExceptionLine = Context->GetExceptionLineNumber(nullptr, &ExceptionSection);
		ASSERT_THAT(IsTrue(ExceptionLine > 0, TEXT("Callstack exception should expose an exception source line")));
		ASSERT_THAT(AreEqual(ModuleName, FString(UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : "")), TEXT("Callstack exception should expose its source section")));
		ASSERT_THAT(IsNull(Context->GetFunction(ExceptionDepth), TEXT("Callstack out-of-range function query should be null")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(RawContext->GetBlueprintCallstackFrame(ExceptionDepth)), TEXT("Callstack out-of-range Blueprint frame query should be zero")));
		ASSERT_THAT(AreEqual(asINVALID_ARG, Context->GetLineNumber(ExceptionDepth, nullptr, nullptr), TEXT("Callstack out-of-range line query should return the documented invalid value")));
		ASSERT_THAT(IsNull(RawContext->GetStackFrame(ExceptionDepth), TEXT("Callstack out-of-range concrete frame query should be null")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Callstack exception path should unprepare for cleanup")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Root), TEXT("Callstack context should execute a normal root again after exception cleanup")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Callstack context reuse should preserve the normal root result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Callstack reused context should unprepare after normal execution")));
		RawContext->ClearLineCallback();
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
