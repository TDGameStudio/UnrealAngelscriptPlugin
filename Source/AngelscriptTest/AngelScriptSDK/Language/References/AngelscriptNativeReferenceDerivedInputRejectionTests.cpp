#include "AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferenceDerivedInputRejectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;
	using FReferenceState =
		AngelscriptNativeReferenceTestSupport::FReferenceState;

	static FString BuildReferenceIdentityRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("int RecoverReferenceIdentity()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 913;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			SourceId,
			ModuleName,
			Source);
		const FTCHARToUTF8 ModuleNameUtf8(
			*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return AngelscriptNativeTestSupport::
			CompileNativeModule(
				&ScriptEngine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get(),
				OutModule);
	}

	void ExecuteRecovery(
		const FNativeCaseContext& Case,
		asIScriptContext& Context,
		asIScriptFunction& Recovery)
	{
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context.Prepare(&Recovery),
			*Case.Describe(TEXT("reference identity recovery should prepare on the same context"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context.Execute(),
			*Case.Describe(TEXT("reference identity recovery should finish"))));
		ASSERT_THAT(AreEqual(
			913,
			static_cast<int32>(
				Context.GetReturnDWord()),
			*Case.Describe(TEXT("reference identity recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("reference identity recovery should unprepare cleanly"))));
	}

	void VerifyLifecycleCleanup(
		const FNativeCaseContext& Case,
		FReferenceState& State)
	{
		State.ReleaseRetainedNativeObject();
		ASSERT_THAT(AreEqual(
			0,
			State.LiveObjects,
			*Case.DescribeResult(
				TEXT("reference identity cleanup"),
				TEXT("Live=0 after module discard"),
				DescribeReferenceState(State))));
		ASSERT_THAT(AreEqual(
			State.Created,
			State.Destroyed,
			*Case.Describe(TEXT("reference identity cell should destroy every created object exactly once"))));
		TArray<int32> Created =
			State.CreatedIdentities;
		TArray<int32> Destroyed =
			State.DestroyedIdentities;
		Created.Sort();
		Destroyed.Sort();
		ASSERT_THAT(AreEqual(
			Created,
			Destroyed,
			*Case.Describe(TEXT("reference identity cell should destroy the exact created identities"))));
		ASSERT_THAT(IsTrue(
			State.ReleaseCalls <= State.AddRefCalls
				+ State.Created,
			*Case.Describe(TEXT("reference identity releases should never exceed owned references"))));
	}

	void CompileAndRunRecoveryModule(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		const FString RecoverySource =
			BuildReferenceIdentityRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(
			CompileAndReport(
				*TestRunner,
				ScriptEngine,
				Case.GetId() + TEXT("-RECOVERY"),
				ModuleName,
				RecoverySource,
				RecoveryModule) >= 0,
			*Case.Describe(TEXT("reference identity failed-build recovery should compile"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("reference identity failed-build recovery should publish a module"))));
		ASSERT_THAT(IsFalse(
			AngelscriptNativeReferenceTestSupport::HasAnyError(
				Engine),
			*Case.Describe(TEXT("reference identity recovery should emit no errors"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				RecoveryModule->GetFunctionByDecl(
					"int RecoverReferenceIdentity()");
			asIScriptContext* const Context =
				ScriptEngine.CreateContext();
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("reference identity recovery should publish its exact entry"))));
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("reference identity recovery should create a context"))));
			if (Recovery != nullptr
				&& Context != nullptr)
			{
				ExecuteRecovery(
					Case,
					*Context,
					*Recovery);
			}
			if (Context != nullptr)
			{
				Context->Release();
			}
		}
		AngelscriptNativeReferenceTestSupport::
			DiscardReferenceModule(
				ScriptEngine,
				ModuleName);
	}

public:
	TEST_METHOD(CurrentForkRejectsDerivedToBaseInputReferenceConversion)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-REF-FORK-DERIVED-INREF",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Cleanup);

		const FNativeCaseContext Case(
			TEXT("LANG-REF-FORK-DERIVED-TO-BASE-INREF"));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine =
			Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("derived-to-base input-reference restriction should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FReferenceState State;
		State.ResetCounters();
		ASSERT_THAT(IsTrue(
			RegisterReferenceFixtures(
				*ScriptEngine,
				State),
			*Case.Describe(TEXT("derived-to-base input-reference restriction should register core reference fixtures"))));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"bool RequireRootInput(const FRefRoot&in First, const FRefRoot&in Second)",
				asFUNCTION(GenericSameReference),
				asCALL_GENERIC) >= 0,
			*Case.Describe(TEXT("derived-to-base input-reference restriction should register a root-only observer"))));

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("int ProbeDerivedInputReference(FRefDerived Source)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn RequireRootInput(Source, Source) ? 1 : 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));

		const FString ModuleName =
			TEXT("ReferenceIdentityForkDerivedToBaseInput");
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				Case.GetId(),
				ModuleName,
				Source,
				Module);
		ASSERT_THAT(IsTrue(
			BuildResult < 0,
			*Case.DescribeResult(
				TEXT("derived-to-base input-reference restriction"),
				TEXT("negative build result"),
				DescribeReferenceBuild(
					Engine,
					BuildResult))));
		ASSERT_THAT(IsTrue(
			HasDiagnosticContaining(
				Engine,
				TEXT("No matching signatures"))
				&& Engine.GetMessagesText().Contains(
					TEXT("Parameter 'First' expected const FRefRoot&, but got FRefDerived&"),
					ESearchCase::IgnoreCase),
			*Case.DescribeResult(
				TEXT("derived-to-base input-reference diagnostic"),
				TEXT("root input reference rejects the derived static view"),
				DescribeReferenceBuild(
					Engine,
					BuildResult))));
		DiscardReferenceModule(
			*ScriptEngine,
			ModuleName);
		CompileAndRunRecoveryModule(
			Case,
			Engine,
			*ScriptEngine,
			ModuleName);
		VerifyLifecycleCleanup(
			Case,
			State);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
