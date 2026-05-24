#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_ClassGenerator_AngelscriptAdditionalCompileChecksTests_Private
{
	static const FName AdditionalChecksModuleName(TEXT("AdditionalChecksMod"));
	static const FString AdditionalChecksFilename(TEXT("AdditionalChecksMod.as"));
	static const FName AdditionalChecksClassName(TEXT("UAdditionalChecksTarget"));

	static const FName AdditionalChecksRejectedModuleName(TEXT("AdditionalChecksRejectMod"));
	static const FString AdditionalChecksRejectedFilename(TEXT("AdditionalChecksRejectMod.as"));
	static const FName AdditionalChecksRejectedClassName(TEXT("UAdditionalChecksRejectedTarget"));

	bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	bool SummaryContainsDiagnosticMessage(const FAngelscriptCompileTraceSummary& Summary, const FString& ExpectedMessage)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			if (Diagnostic.Message.Contains(ExpectedMessage))
			{
				return true;
			}
		}

		return false;
	}

	struct FTestAdditionalCompileChecks final : FAngelscriptAdditionalCompileChecks
	{
		int32 CompileCheckCount = 0;
		int32 PostReloadCount = 0;
		FString LastModuleName;
		FString LastClassName;
		bool bLastFullReload = false;
		bool bRejectCompile = false;
		FString RejectMessage = TEXT("Test additional compile check rejected the script class.");
		TArray<bool> PostReloadHistory;

		virtual bool ScriptCompileAdditionalChecks(TSharedPtr<FAngelscriptModuleDesc> ModuleDesc, TSharedPtr<FAngelscriptClassDesc> ClassDesc) override
		{
			++CompileCheckCount;
			LastModuleName = ModuleDesc.IsValid() ? ModuleDesc->ModuleName : FString();
			LastClassName = ClassDesc.IsValid() ? ClassDesc->ClassName : FString();

			if (bRejectCompile)
			{
				FAngelscriptEngine::Get().ScriptCompileError(ModuleDesc, 1, RejectMessage);
				return false;
			}

			return true;
		}

		virtual void PostReloadAdditionalChecks(bool bFullReload, TSharedPtr<FAngelscriptModuleDesc> ModuleDesc, TSharedPtr<FAngelscriptClassDesc> ClassDesc) override
		{
			++PostReloadCount;
			bLastFullReload = bFullReload;
			LastModuleName = ModuleDesc.IsValid() ? ModuleDesc->ModuleName : FString();
			LastClassName = ClassDesc.IsValid() ? ClassDesc->ClassName : FString();
			PostReloadHistory.Add(bFullReload);
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptAdditionalCompileChecksTests,
	"Angelscript.TestModule.ClassGenerator.AdditionalCompileChecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InvokeCompileAndPostReloadHooks)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptAdditionalCompileChecksTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		TSharedPtr<FTestAdditionalCompileChecks> Recorder = MakeShared<FTestAdditionalCompileChecks>();
		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(Recorder));

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.DiscardModule(*AdditionalChecksModuleName.ToString());
			Engine.DiscardModule(*AdditionalChecksRejectedModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UAdditionalChecksTarget : UObject
{
	UPROPERTY()
	int Value = 1;
}
)AS");
		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UAdditionalChecksTarget : UObject
{
	UPROPERTY()
	int Value = 1;

	UPROPERTY()
	int AddedValue = 2;
}
)AS");
		const FString RejectScript = TEXT(R"AS(
UCLASS()
class UAdditionalChecksRejectedTarget : UObject
{
	UPROPERTY()
	int Value = 3;
}
)AS");

		if (!TestRunner->TestTrue(TEXT("Initial additional-compile-checks module compile should succeed"),
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksModuleName, AdditionalChecksFilename, ScriptV1)))
		{ return; }

		UClass* InitialGeneratedClass = FindGeneratedClass(&Engine, AdditionalChecksClassName);
		if (!TestRunner->TestNotNull(TEXT("Initial additional-compile-checks compile should publish the generated script class"), InitialGeneratedClass))
		{ return; }

		TestRunner->TestEqual(TEXT("Initial annotated compile should invoke the compile hook exactly once"), Recorder->CompileCheckCount, 1);
		TestRunner->TestEqual(TEXT("Initial annotated compile should invoke the post-reload hook exactly once"), Recorder->PostReloadCount, 1);
		TestRunner->TestEqual(TEXT("Initial annotated compile should report the module name to the hook"), Recorder->LastModuleName, AdditionalChecksModuleName.ToString());
		TestRunner->TestEqual(TEXT("Initial annotated compile should report the generated class name to the hook"), Recorder->LastClassName, AdditionalChecksClassName.ToString());
		if (!TestRunner->TestEqual(TEXT("Initial annotated compile should record exactly one post-reload event"), Recorder->PostReloadHistory.Num(), 1))
		{ return; }
		TestRunner->TestTrue(TEXT("Initial annotated compile helper should surface the post-reload hook as a full reload"), Recorder->PostReloadHistory[0]);
		TestRunner->TestTrue(TEXT("Initial annotated compile helper should leave the last post-reload flag in full-reload state"), Recorder->bLastFullReload);

		ECompileResult ReloadResult = ECompileResult::Error;
		if (!TestRunner->TestTrue(TEXT("Additional-compile-checks structural reload should compile successfully"),
			CompileModuleWithResult(&Engine, ECompileType::FullReload, AdditionalChecksModuleName, AdditionalChecksFilename, ScriptV2, ReloadResult)))
		{ return; }
		if (!TestRunner->TestTrue(TEXT("Additional-compile-checks structural reload should stay on a handled reload path"), IsHandledReloadResult(ReloadResult)))
		{ return; }

		UClass* ReloadedGeneratedClass = FindGeneratedClass(&Engine, AdditionalChecksClassName);
		if (!TestRunner->TestNotNull(TEXT("Additional-compile-checks full reload should keep the generated script class queryable"), ReloadedGeneratedClass))
		{ return; }

		TestRunner->TestNotEqual(TEXT("Additional-compile-checks full reload should replace the generated class object after a structural change"), ReloadedGeneratedClass, InitialGeneratedClass);
		TestRunner->TestEqual(TEXT("Full reload should invoke the compile hook for the replacement class"), Recorder->CompileCheckCount, 2);
		TestRunner->TestEqual(TEXT("Full reload should invoke the post-reload hook a second time"), Recorder->PostReloadCount, 2);
		if (!TestRunner->TestEqual(TEXT("Full reload should record two post-reload events in total"), Recorder->PostReloadHistory.Num(), 2))
		{ return; }
		TestRunner->TestTrue(TEXT("Full reload should report the second post-reload event as a full reload"), Recorder->PostReloadHistory[1]);
		TestRunner->TestTrue(TEXT("Full reload should leave the last post-reload flag in full-reload state"), Recorder->bLastFullReload);
		TestRunner->TestEqual(TEXT("Full reload should continue reporting the target module name"), Recorder->LastModuleName, AdditionalChecksModuleName.ToString());
		TestRunner->TestEqual(TEXT("Full reload should continue reporting the target class name"), Recorder->LastClassName, AdditionalChecksClassName.ToString());

		Recorder->bRejectCompile = true;

		FAngelscriptCompileTraceSummary RejectSummary;
		const bool bRejectedCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::FullReload, AdditionalChecksRejectedModuleName, AdditionalChecksRejectedFilename,
			RejectScript, true, RejectSummary, true);

		TestRunner->TestFalse(TEXT("A rejecting additional compile check should fail compilation"), bRejectedCompiled);
		TestRunner->TestEqual(TEXT("A rejecting additional compile check should surface an error compile result"), RejectSummary.CompileResult, ECompileResult::Error);
		TestRunner->TestTrue(TEXT("A rejecting additional compile check should emit at least one diagnostic"), RejectSummary.Diagnostics.Num() > 0);
		TestRunner->TestTrue(TEXT("A rejecting additional compile check should preserve the rejection text in diagnostics"), SummaryContainsDiagnosticMessage(RejectSummary, Recorder->RejectMessage));
		TestRunner->TestEqual(TEXT("A rejecting additional compile check should still invoke the compile hook"), Recorder->CompileCheckCount, 3);
		TestRunner->TestEqual(TEXT("A rejecting additional compile check should not advance the post-reload hook count"), Recorder->PostReloadCount, 2);
		TestRunner->TestEqual(TEXT("A rejecting additional compile check should report the rejected module name"), Recorder->LastModuleName, AdditionalChecksRejectedModuleName.ToString());
		TestRunner->TestEqual(TEXT("A rejecting additional compile check should report the rejected class name"), Recorder->LastClassName, AdditionalChecksRejectedClassName.ToString());
		TestRunner->TestNull(TEXT("A rejecting additional compile check should not publish the rejected class"), FindGeneratedClass(&Engine, AdditionalChecksRejectedClassName));

		}
	}
};

#endif
