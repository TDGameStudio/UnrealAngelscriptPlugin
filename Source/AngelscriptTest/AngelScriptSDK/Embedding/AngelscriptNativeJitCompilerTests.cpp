#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeJitCompilerTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.JitCompiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FRecordingJitCompiler final : public asIJITCompiler
	{
	public:
		int CompileResult = asSUCCESS;
		bool bPublishFunction = false;
		int32 CompileCount = 0;
		int32 ReleaseCount = 0;
		TArray<FString> CompiledDeclarations;
		TArray<asJITFunction> ReleasedFunctions;

		int CompileFunction(
			asIScriptFunction* Function,
			asJITFunction* Output) override
		{
			++CompileCount;
			CompiledDeclarations.Add(
				Function != nullptr && Function->GetDeclaration() != nullptr
					? UTF8_TO_TCHAR(Function->GetDeclaration())
					: TEXT("<null>"));
			if (Output != nullptr)
			{
				*Output = bPublishFunction ? &SentinelJitFunction : nullptr;
			}
			return CompileResult;
		}

		void ReleaseJITFunction(asJITFunction Function) override
		{
			++ReleaseCount;
			ReleasedFunctions.Add(Function);
		}

		static void SentinelJitFunction(
			FScriptExecution&,
			asDWORD*,
			asQWORD*)
		{
		}
	};

	static std::string BuildSource(const int32 Value)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %d;"), Value));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return TCHAR_TO_UTF8(*Source);
	}

	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* SourceId,
		const ANSICHAR* ModuleName,
		const std::string& Source)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			SourceId,
			UTF8_TO_TCHAR(ModuleName),
			UTF8_TO_TCHAR(Source.c_str()));
	}

public:
	TEST_METHOD(InstallCompileReplaceAndClear)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("EMBED-JIT-INSTALL-COMPILE-LIFECYCLE",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("JIT lifecycle product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FRecordingJitCompiler PrimaryCompiler;
		FRecordingJitCompiler ReplacementCompiler;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		ASSERT_THAT(IsNull(ScriptEngine->GetJITCompiler(), TEXT("JIT lifecycle product should begin with no compiler installed")));

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(&PrimaryCompiler), TEXT("JIT lifecycle product should install the primary compiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asIJITCompiler*>(&PrimaryCompiler),
			ScriptEngine->GetJITCompiler(),
			TEXT("JIT lifecycle product should return the exact installed primary compiler")));

		const std::string PrimarySource = BuildSource(11);
		PrintSource(*TestRunner, TEXT("EMBED-JIT.install-primary"), "NativeJitPrimary", PrimarySource);
		{
			FScopedNativeModule PrimaryModule(*TestRunner, Engine, "NativeJitPrimary", PrimarySource);
			ASSERT_THAT(IsTrue(PrimaryModule.IsValid(), TEXT("JIT lifecycle product should compile with the primary compiler installed")));
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
				*TestRunner,
				ScriptEngine,
				PrimaryModule,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("JIT lifecycle product should prepare the primary interpreted entry")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					11,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Primary compiler with no published JIT function should preserve interpreted runtime behavior")));
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeJitPrimary", asGM_ONLY_IF_EXISTS),
			TEXT("Primary JIT module should be absent after scoped cleanup")));
		ASSERT_THAT(IsTrue(PrimaryCompiler.CompileCount > 0, TEXT("JIT lifecycle product should route script functions through the primary compiler")));
		ASSERT_THAT(IsTrue(
			PrimaryCompiler.CompiledDeclarations.Contains(TEXT("int Entry()")),
			TEXT("JIT lifecycle product should report the exact primary script declaration")));
		const int32 PrimaryCompileCount = PrimaryCompiler.CompileCount;

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(&ReplacementCompiler), TEXT("JIT lifecycle product should replace the installed compiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asIJITCompiler*>(&ReplacementCompiler),
			ScriptEngine->GetJITCompiler(),
			TEXT("JIT lifecycle product should return the exact replacement compiler")));

		const std::string ReplacementSource = BuildSource(22);
		PrintSource(*TestRunner, TEXT("EMBED-JIT.install-replacement"), "NativeJitReplacement", ReplacementSource);
		{
			FScopedNativeModule ReplacementModule(*TestRunner, Engine, "NativeJitReplacement", ReplacementSource);
			ASSERT_THAT(IsTrue(ReplacementModule.IsValid(), TEXT("JIT lifecycle product should compile with the replacement compiler installed")));
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
				*TestRunner,
				ScriptEngine,
				ReplacementModule,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("JIT lifecycle product should prepare the replacement interpreted entry")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					22,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Replacement compiler with no published JIT function should preserve interpreted runtime behavior")));
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeJitReplacement", asGM_ONLY_IF_EXISTS),
			TEXT("Replacement JIT module should be absent after scoped cleanup")));
		ASSERT_THAT(AreEqual(PrimaryCompileCount, PrimaryCompiler.CompileCount, TEXT("JIT lifecycle product should stop calling the replaced compiler")));
		ASSERT_THAT(IsTrue(ReplacementCompiler.CompileCount > 0, TEXT("JIT lifecycle product should route later functions through the replacement compiler")));

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(nullptr), TEXT("JIT lifecycle product should clear the replacement compiler")));
		ASSERT_THAT(IsNull(ScriptEngine->GetJITCompiler(), TEXT("JIT lifecycle product should expose the cleared compiler state")));
		const int32 ReplacementCompileCount = ReplacementCompiler.CompileCount;

		const std::string ClearedSource = BuildSource(33);
		PrintSource(*TestRunner, TEXT("EMBED-JIT.cleared"), "NativeJitCleared", ClearedSource);
		{
			FScopedNativeModule ClearedModule(*TestRunner, Engine, "NativeJitCleared", ClearedSource);
			ASSERT_THAT(IsTrue(ClearedModule.IsValid(), TEXT("JIT lifecycle product should retain interpreted compilation after clearing the compiler")));
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
				*TestRunner,
				ScriptEngine,
				ClearedModule,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("JIT lifecycle product should prepare the entry after clearing the compiler")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					33,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Cleared compiler state should execute the exact interpreted entry")));
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeJitCleared", asGM_ONLY_IF_EXISTS),
			TEXT("Cleared-state JIT module should be absent after scoped cleanup")));
		ASSERT_THAT(AreEqual(
			ReplacementCompileCount,
			ReplacementCompiler.CompileCount,
			TEXT("JIT lifecycle product should not call a compiler after clear")));
	}

	TEST_METHOD(CompileFailureAndReleaseBoundary)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("EMBED-JIT-FAILURE-RELEASE-BOUNDARY",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("JIT failure product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FRecordingJitCompiler FailingCompiler;
		FRecordingJitCompiler PublishingCompiler;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		FailingCompiler.CompileResult = asERROR;
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(&FailingCompiler), TEXT("JIT failure product should install the failing compiler")));
		asIJITCompiler* const FailingCompilerInterface = &FailingCompiler;
		asJITFunction DirectFailureOutput = nullptr;
		ASSERT_THAT(AreEqual(
			asERROR,
			FailingCompilerInterface->CompileFunction(nullptr, &DirectFailureOutput),
			TEXT("JIT failure product should expose the rejecting CompileFunction interface directly")));
		ASSERT_THAT(IsTrue(
			DirectFailureOutput == nullptr,
			TEXT("JIT failure product should leave the direct failure output null")));

		const std::string FailureSource = BuildSource(44);
		PrintSource(*TestRunner, TEXT("EMBED-JIT.compile-failure"), "NativeJitCompileFailure", FailureSource);
		{
			FScopedNativeModule FailureModule(*TestRunner, Engine, "NativeJitCompileFailure", FailureSource);
			ASSERT_THAT(IsTrue(FailureModule.IsValid(), TEXT("JIT failure product should retain interpreted module compilation when the JIT callback rejects a function")));
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
				*TestRunner,
				ScriptEngine,
				FailureModule,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("JIT failure product should prepare the interpreted fallback entry")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					44,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Rejected JIT compilation should preserve exact interpreted runtime behavior")));
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeJitCompileFailure", asGM_ONLY_IF_EXISTS),
			TEXT("Rejected-JIT fallback module should be absent after scoped cleanup")));
		ASSERT_THAT(IsTrue(FailingCompiler.CompileCount > 0, TEXT("JIT failure product should invoke the rejecting compiler")));
		ASSERT_THAT(AreEqual(0, FailingCompiler.ReleaseCount, TEXT("JIT failure product should not release a null function after a rejected compile")));

		PublishingCompiler.bPublishFunction = true;
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(&PublishingCompiler), TEXT("JIT release product should install a compiler that publishes a function")));

		const std::string ReleaseSource = BuildSource(55);
		PrintSource(*TestRunner, TEXT("EMBED-JIT.release-boundary"), "NativeJitReleaseBoundary", ReleaseSource);
		{
			FScopedNativeModule ReleaseModule(*TestRunner, Engine, "NativeJitReleaseBoundary", ReleaseSource);
			ASSERT_THAT(IsTrue(ReleaseModule.IsValid(), TEXT("JIT release product should compile a module with a published sentinel function")));
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeJitReleaseBoundary", asGM_ONLY_IF_EXISTS),
			TEXT("Published-JIT module should be absent after scoped cleanup")));
		ASSERT_THAT(IsTrue(PublishingCompiler.CompileCount > 0, TEXT("JIT release product should publish at least one sentinel function")));
		ASSERT_THAT(AreEqual(
			0,
			PublishingCompiler.ReleaseCount,
			TEXT("JIT release product should retain the current fork boundary where module discard does not call ReleaseJITFunction")));
		TestRunner->AddInfo(TEXT("[AS-FORK-LIMITATION] Module discard does not call asIJITCompiler::ReleaseJITFunction in this fork; explicit interface invocation is tested separately so the missing engine-owned release remains visible"));

		asIJITCompiler* const CompilerInterface = &PublishingCompiler;
		CompilerInterface->ReleaseJITFunction(&FRecordingJitCompiler::SentinelJitFunction);
		ASSERT_THAT(AreEqual(1, PublishingCompiler.ReleaseCount, TEXT("JIT release product should expose direct ReleaseJITFunction interface dispatch")));
		ASSERT_THAT(AreEqual(
			static_cast<asJITFunction>(&FRecordingJitCompiler::SentinelJitFunction),
			PublishingCompiler.ReleasedFunctions[0],
			TEXT("JIT release product should preserve the exact explicitly released function pointer")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
