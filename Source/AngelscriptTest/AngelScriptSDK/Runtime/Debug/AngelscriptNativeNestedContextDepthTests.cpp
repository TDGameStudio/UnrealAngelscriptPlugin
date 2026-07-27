#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeNestedContextDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.NestedContextDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	inline static constexpr asPWORD ProductRecorderUserDataSlot = static_cast<asPWORD>(0x4E4154444D415452ull);

	enum class ENestedAction : uint8
	{
		Success,
		Exception,
		Suspend,
		Abort,
	};

	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FTransition
	{
		int32 Depth = 0;
		FString CallerBefore;
		FString CallerAfter;
		FString Selected;
		asEContextState StateBefore = asEXECUTION_UNINITIALIZED;
		asEContextState StateAtExecuteReturn = asEXECUTION_UNINITIALIZED;
		asEContextState StateAfter = asEXECUTION_UNINITIALIZED;
		int32 CallstackAtExecuteReturn = -1;
		FString FunctionAtExecuteReturn;
		FString ExceptionAtExecuteReturn;
		asUINT NestedCount = 0;
		bool bNested = false;
		int32 PushResult = asERROR;
		int32 PrepareResult = asERROR;
		int32 ExecuteResult = asERROR;
		int32 PopResult = asERROR;
		int32 ActionResult = asERROR;
	};

	struct FDepthObservation
	{
		asIScriptFunction* SameFunction = nullptr;
		asIScriptFunction* DifferentFunction = nullptr;
		asIScriptFunction* FaultFunction = nullptr;
		int32 TargetDepth = 0;
		bool bDifferentSignature = false;
		ENestedAction Action = ENestedAction::Success;
		int32 CurrentDepth = 0;
		TArray<FTransition> Transitions;

		void Reset(asIScriptFunction* InSameFunction,
			asIScriptFunction* InDifferentFunction,
			asIScriptFunction* InFaultFunction,
			int32 InTargetDepth,
			bool bInDifferentSignature,
			ENestedAction InAction)
		{
			SameFunction = InSameFunction;
			DifferentFunction = InDifferentFunction;
			FaultFunction = InFaultFunction;
			TargetDepth = InTargetDepth;
			bDifferentSignature = bInDifferentSignature;
			Action = InAction;
			CurrentDepth = 0;
			Transitions.Reset();
		}
	};

	inline static constexpr FNamedCase DepthCases[] =
	{
		{ "one" }, { "two" }, { "three" }, { "five" },
	};

	inline static constexpr int32 DepthValues[] =
	{
		1, 2, 3, 5,
	};

	inline static constexpr FNamedCase SignatureCases[] =
	{
		{ "same_signature" }, { "different_signature" },
	};

	inline static constexpr FNamedCase ActionCases[] =
	{
		{ "success" }, { "exception" }, { "suspend_request" }, { "abort_request" },
	};

	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static ENestedAction ToAction(const FNamedCase& Case)
	{
		if (IsCase(Case, "exception"))
		{
			return ENestedAction::Exception;
		}
		if (IsCase(Case, "suspend_request"))
		{
			return ENestedAction::Suspend;
		}
		if (IsCase(Case, "abort_request"))
		{
			return ENestedAction::Abort;
		}
		return ENestedAction::Success;
	}

	static FString BuildSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int NestedDepthSame()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugEnterNestedDepth();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int NestedDepthDifferent(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugEnterNestedDepth();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value + 3;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int NestedDepthFault()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugEnterNestedDepth();"));
		AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int NestedDepthOuter()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugEnterNestedDepth();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 42;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static void EnterNestedDepth()
	{
		asIScriptContext* const Context = asGetActiveContext();
		FDepthObservation* const Observation = Context != nullptr
			? static_cast<FDepthObservation*>(Context->GetUserData(ProductRecorderUserDataSlot))
			: nullptr;
		if (Context == nullptr || Observation == nullptr || Observation->CurrentDepth >= Observation->TargetDepth)
		{
			return;
		}

		const int32 TransitionIndex = Observation->Transitions.AddDefaulted();
		FTransition& BeforePush = Observation->Transitions[TransitionIndex];
		BeforePush.Depth = Observation->CurrentDepth;
		BeforePush.StateBefore = Context->GetState();
		if (asIScriptFunction* const Caller = Context->GetFunction(0))
		{
			BeforePush.CallerBefore = UTF8_TO_TCHAR(Caller->GetDeclaration());
		}

		BeforePush.PushResult = Context->PushState();
		if (BeforePush.PushResult != asSUCCESS)
		{
			return;
		}

		BeforePush.bNested = Context->IsNested(&BeforePush.NestedCount);
		++Observation->CurrentDepth;

		const bool bTerminal = Observation->CurrentDepth >= Observation->TargetDepth;
		asIScriptFunction* SelectedFunction = nullptr;
		if (Observation->Action == ENestedAction::Exception && bTerminal)
		{
			SelectedFunction = Observation->FaultFunction;
		}
		else if (Observation->bDifferentSignature && Observation->CurrentDepth == 1)
		{
			SelectedFunction = Observation->DifferentFunction;
		}
		else
		{
			SelectedFunction = Observation->SameFunction;
		}
		BeforePush.Selected = SelectedFunction != nullptr
			? UTF8_TO_TCHAR(SelectedFunction->GetDeclaration())
			: FString();

		if (bTerminal && Observation->Action == ENestedAction::Suspend)
		{
			BeforePush.ActionResult = Context->Suspend();
		}
		else if (bTerminal && Observation->Action == ENestedAction::Abort)
		{
			BeforePush.ActionResult = Context->Abort();
		}

		BeforePush.PrepareResult = SelectedFunction != nullptr
			? Context->Prepare(SelectedFunction)
			: asNO_FUNCTION;
		if (BeforePush.PrepareResult == asSUCCESS && SelectedFunction != nullptr && SelectedFunction->GetParamCount() == 1)
		{
			Context->SetArgDWord(0, 11);
		}
		if (BeforePush.PrepareResult == asSUCCESS)
		{
			const int ExecuteResult = Context->Execute();
			FTransition& AfterExecute = Observation->Transitions[TransitionIndex];
			AfterExecute.ExecuteResult = ExecuteResult;
			AfterExecute.StateAtExecuteReturn = Context->GetState();
			AfterExecute.CallstackAtExecuteReturn = static_cast<int32>(Context->GetCallstackSize());
			if (asIScriptFunction* const ExecuteFunction = Context->GetFunction(0))
			{
				AfterExecute.FunctionAtExecuteReturn = UTF8_TO_TCHAR(ExecuteFunction->GetDeclaration());
			}
			if (const char* const Exception = Context->GetExceptionString())
			{
				AfterExecute.ExceptionAtExecuteReturn = UTF8_TO_TCHAR(Exception);
			}
		}

		--Observation->CurrentDepth;
		FTransition& AfterExecute = Observation->Transitions[TransitionIndex];
		AfterExecute.PopResult = Context->PopState();
		AfterExecute.StateAfter = Context->GetState();
		if (asIScriptFunction* const Caller = Context->GetFunction(0))
		{
			AfterExecute.CallerAfter = UTF8_TO_TCHAR(Caller->GetDeclaration());
		}
	}

public:
	TEST_METHOD(NestedStateByDepthSignatureAndAction)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-NEST-STATE-DEPTH",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		FNoDiscardAsserter Assertions(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Nested depth product should create a raw SDK engine")))
		{
			return;
		}

		const ASAutoCaller::FunctionCaller ProductCaller = ASAutoCaller::MakeFunctionCaller(EnterNestedDepth);
		const int RegisterResult = ScriptEngine->RegisterGlobalFunction(
			"void NativeDebugEnterNestedDepth()",
			asFUNCTION(EnterNestedDepth),
			asCALL_CDECL,
			*(asFunctionCaller*)&ProductCaller);
		if (!Assertions.IsTrue(RegisterResult >= 0,
			*FString::Printf(TEXT("Nested depth product should register its callback bridge. Result=%d Messages={%s}"),
				RegisterResult, *Engine.GetMessagesText())))
		{
			return;
		}

		const FString Source = BuildSource();
		for (int32 DepthIndex = 0; DepthIndex < UE_ARRAY_COUNT(DepthCases); ++DepthIndex)
		{
			const FNamedCase& DepthCase = DepthCases[DepthIndex];
			for (const FNamedCase& SignatureCase : SignatureCases)
			{
				for (const FNamedCase& ActionCase : ActionCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"DBG-NEST-STATE-DEPTH",
						{
							ANSI_TO_TCHAR(DepthCase.CatalogName),
							ANSI_TO_TCHAR(SignatureCase.CatalogName),
							ANSI_TO_TCHAR(ActionCase.CatalogName),
						}));
					const FString ModuleName = TEXT("NestedDepth_") + Case.GetId().RightChop(20).Replace(TEXT("-"), TEXT("_"));
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);

					Engine.ResetMessages();
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					const FString BuildDescription = FString::Printf(TEXT("%s; result=%d messages={%s}"),
						*Case.Describe(TEXT("nested depth source should compile")), BuildResult, *Engine.GetMessagesText());
					if (!Assertions.AreEqual(asSUCCESS, BuildResult, BuildDescription))
					{
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						continue;
					}
					if (!Assertions.IsNotNull(Module, *Case.Describe(TEXT("nested depth source should publish a module"))))
					{
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						continue;
					}

					asIScriptFunction* const Outer = GetNativeFunctionByExactDecl(Module, "int NestedDepthOuter()");
					asIScriptFunction* const SameFunction = GetNativeFunctionByExactDecl(Module, "int NestedDepthSame()");
					asIScriptFunction* const DifferentFunction = GetNativeFunctionByExactDecl(Module, "int NestedDepthDifferent(const int)");
					asIScriptFunction* const FaultFunction = GetNativeFunctionByExactDecl(Module, "int NestedDepthFault()");
					(void)Assertions.IsNotNull(Outer, *Case.Describe(TEXT("nested depth should resolve the outer declaration exactly")));
					(void)Assertions.IsNotNull(SameFunction, *Case.Describe(TEXT("nested depth should resolve the same-signature declaration exactly")));
					(void)Assertions.IsNotNull(DifferentFunction, *Case.Describe(TEXT("nested depth should resolve the different-signature declaration exactly")));
					(void)Assertions.IsNotNull(FaultFunction, *Case.Describe(TEXT("nested depth should resolve the exception declaration exactly")));
					if (Outer == nullptr || SameFunction == nullptr || DifferentFunction == nullptr || FaultFunction == nullptr)
					{
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						continue;
					}

					asIScriptContext* const Context = ScriptEngine->CreateContext();
					if (!Assertions.IsNotNull(Context, *Case.Describe(TEXT("nested depth should create a context"))))
					{
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						continue;
					}

					FDepthObservation Observation;
					Observation.Reset(SameFunction, DifferentFunction, FaultFunction,
						DepthValues[DepthIndex], IsCase(SignatureCase, "different_signature"), ToAction(ActionCase));
					Context->SetUserData(&Observation, ProductRecorderUserDataSlot);

					const int PrepareResult = Context->Prepare(Outer);
					(void)Assertions.AreEqual(asSUCCESS, PrepareResult, *Case.Describe(TEXT("nested depth outer function should prepare")));
					const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : asERROR;
					const bool bExpectedNestedException = ToAction(ActionCase) == ENestedAction::Exception;
					(void)Assertions.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
						*Case.Describe(TEXT("nested depth outer function should resume and finish after PopState, including inner exception")));
					(void)Assertions.AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("nested depth outer function should retain its result after nested restoration")));
					(void)Assertions.AreEqual(DepthValues[DepthIndex], Observation.Transitions.Num(),
						*Case.Describe(TEXT("nested depth should record one transition per requested depth")));

					for (int32 TransitionIndex = 0; TransitionIndex < Observation.Transitions.Num(); ++TransitionIndex)
					{
						const FTransition& Transition = Observation.Transitions[TransitionIndex];
						const bool bTerminal = TransitionIndex + 1 == DepthValues[DepthIndex];
						(void)Assertions.AreEqual(asSUCCESS, Transition.PushResult,
							*Case.Describe(TEXT("nested depth transition should push the active outer state")));
						(void)Assertions.IsTrue(Transition.bNested,
							*Case.Describe(TEXT("nested depth transition should report IsNested after PushState")));
						(void)Assertions.AreEqual(TransitionIndex + 1, static_cast<int32>(Transition.NestedCount),
							*Case.Describe(TEXT("nested depth transition should report its exact saved-state count")));
						(void)Assertions.AreEqual(asSUCCESS, Transition.PrepareResult,
							*Case.Describe(TEXT("nested depth transition should prepare the selected inner declaration")));
						(void)Assertions.AreEqual(asSUCCESS, Transition.PopResult,
							*Case.Describe(TEXT("nested depth transition should pop and restore the active outer state")));
						(void)Assertions.AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(Transition.StateBefore),
							*Case.Describe(TEXT("nested depth transition should begin in the active state")));
						(void)Assertions.AreEqual(static_cast<int32>(asEXECUTION_ACTIVE), static_cast<int32>(Transition.StateAfter),
							*Case.Describe(TEXT("nested depth transition should restore the active state")));
						(void)Assertions.AreEqual(Transition.CallerBefore, Transition.CallerAfter,
							*Case.Describe(TEXT("nested depth transition should restore the caller declaration")));

						const bool bExceptionTransition = bExpectedNestedException && bTerminal;
						const int32 ExpectedExecuteResult = bExceptionTransition
							? asEXECUTION_EXCEPTION
							: asEXECUTION_FINISHED;
						const FString ExecutionDescription = FString::Printf(
							TEXT("%s; depth=%d expected=%d actual=%d state_before=%d state_at_execute_return=%d state_after=%d callstack_at_execute_return=%d push=%d prepare=%d pop=%d selected=%s function_at_execute_return=%s exception_at_execute_return=%s messages={%s}"),
							*Case.Describe(TEXT("nested depth transition should preserve its inner execution result")),
							TransitionIndex,
							ExpectedExecuteResult,
							Transition.ExecuteResult,
							static_cast<int32>(Transition.StateBefore),
							static_cast<int32>(Transition.StateAtExecuteReturn),
							static_cast<int32>(Transition.StateAfter),
							Transition.CallstackAtExecuteReturn,
							Transition.PushResult,
							Transition.PrepareResult,
							Transition.PopResult,
							*Transition.Selected,
							*Transition.FunctionAtExecuteReturn,
							*Transition.ExceptionAtExecuteReturn,
							*Engine.GetMessagesText());
						(void)Assertions.AreEqual(ExpectedExecuteResult, Transition.ExecuteResult, *ExecutionDescription);

						if (bTerminal && ToAction(ActionCase) == ENestedAction::Suspend)
						{
							(void)Assertions.AreEqual(asERROR, Transition.ActionResult,
								*Case.Describe(TEXT("nested depth suspend request should expose the current fork's unsupported result")));
						}
						if (bTerminal && ToAction(ActionCase) == ENestedAction::Abort)
						{
							(void)Assertions.AreEqual(asERROR, Transition.ActionResult,
								*Case.Describe(TEXT("nested depth abort request should expose the current fork's unsupported result")));
						}

						const FString ExpectedSelected = bExceptionTransition
							? FString(TEXT("int NestedDepthFault()"))
							: (TransitionIndex == 0 && IsCase(SignatureCase, "different_signature")
								? FString(TEXT("int NestedDepthDifferent(const int)"))
								: FString(TEXT("int NestedDepthSame()")));
						(void)Assertions.AreEqual(ExpectedSelected, Transition.Selected,
							*Case.Describe(TEXT("nested depth transition should select the exact signature/action target")));
					}

					if (bExpectedNestedException)
					{
						(void)Assertions.IsTrue(Context->GetExceptionString() == nullptr || Context->GetExceptionString()[0] == '\0',
							*Case.Describe(TEXT("nested exception should be consumed by PopState before the outer function resumes")));
					}
					(void)Assertions.AreEqual(asSUCCESS, Context->Unprepare(),
						*Case.Describe(TEXT("nested depth context should unprepare after the outer execution")));
					asUINT NestCount = 99;
					(void)Assertions.IsFalse(Context->IsNested(&NestCount), *Case.Describe(TEXT("nested depth context should not remain nested after cleanup")));
					(void)Assertions.AreEqual(0, static_cast<int32>(NestCount),
						*Case.Describe(TEXT("nested depth cleanup should reset the saved-state count")));
					(void)Assertions.AreEqual(asERROR, Context->PopState(),
						*Case.Describe(TEXT("nested depth PopState at nesting zero should return the documented error")));

					Context->Release();
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("nested depth cell should discard its module")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
