#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptAdditionalCompileChecksTests,
	"Angelscript.TestModule.ClassGenerator.AdditionalCompileChecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
inline static const FName AdditionalChecksModuleName = FName(TEXT("AdditionalChecksMod"));
inline static const FString AdditionalChecksFilename = FString(TEXT("AdditionalChecksMod.as"));
inline static const FName AdditionalChecksClassName = FName(TEXT("UAdditionalChecksTarget"));

inline static const FName AdditionalChecksRejectedModuleName = FName(TEXT("AdditionalChecksRejectMod"));
inline static const FString AdditionalChecksRejectedFilename = FString(TEXT("AdditionalChecksRejectMod.as"));
inline static const FName AdditionalChecksRejectedClassName = FName(TEXT("UAdditionalChecksRejectedTarget"));

static bool IsHandledReloadResult(const ECompileResult ReloadResult)
{
	return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
}

static bool SummaryContainsDiagnosticMessage(const FAngelscriptCompileTraceSummary& Summary, const FString& ExpectedMessage)
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

public:
	TEST_METHOD(InvokeCompileAndPostReloadHooks)
	{
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

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksModuleName, AdditionalChecksFilename, ScriptV1),
			TEXT("Initial additional-compile-checks module compile should succeed")));
		if (FindGeneratedClass(&Engine, AdditionalChecksClassName) == nullptr)
		{ return; }

		UClass* InitialGeneratedClass = FindGeneratedClass(&Engine, AdditionalChecksClassName);
		ASSERT_THAT(IsNotNull(InitialGeneratedClass, TEXT("Initial additional-compile-checks compile should publish the generated script class")));
		if (InitialGeneratedClass == nullptr)
		{ return; }

		ASSERT_THAT(AreEqual(1, Recorder->CompileCheckCount, TEXT("Initial annotated compile should invoke the compile hook exactly once")));
		ASSERT_THAT(AreEqual(1, Recorder->PostReloadCount, TEXT("Initial annotated compile should invoke the post-reload hook exactly once")));
		ASSERT_THAT(AreEqual(AdditionalChecksModuleName.ToString(), Recorder->LastModuleName, TEXT("Initial annotated compile should report the module name to the hook")));
		ASSERT_THAT(AreEqual(AdditionalChecksClassName.ToString(), Recorder->LastClassName, TEXT("Initial annotated compile should report the generated class name to the hook")));
		ASSERT_THAT(AreEqual(1, Recorder->PostReloadHistory.Num(), TEXT("Initial annotated compile should record exactly one post-reload event")));
		if (Recorder->PostReloadHistory.Num() != 1)
		{ return; }
		ASSERT_THAT(IsTrue(Recorder->PostReloadHistory[0], TEXT("Initial annotated compile helper should surface the post-reload hook as a full reload")));
		ASSERT_THAT(IsTrue(Recorder->bLastFullReload, TEXT("Initial annotated compile helper should leave the last post-reload flag in full-reload state")));

		ECompileResult ReloadResult = ECompileResult::Error;
		const bool bReloadCompiled = CompileModuleWithResult(&Engine, ECompileType::FullReload, AdditionalChecksModuleName, AdditionalChecksFilename, ScriptV2, ReloadResult);
		ASSERT_THAT(IsTrue(bReloadCompiled, TEXT("Additional-compile-checks structural reload should compile successfully")));
		if (!bReloadCompiled)
		{ return; }
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Additional-compile-checks structural reload should stay on a handled reload path")));
		if (!IsHandledReloadResult(ReloadResult))
		{ return; }

		UClass* ReloadedGeneratedClass = FindGeneratedClass(&Engine, AdditionalChecksClassName);
		ASSERT_THAT(IsNotNull(ReloadedGeneratedClass, TEXT("Additional-compile-checks full reload should keep the generated script class queryable")));
		if (ReloadedGeneratedClass == nullptr)
		{ return; }

		ASSERT_THAT(AreNotEqual(ReloadedGeneratedClass, InitialGeneratedClass, TEXT("Additional-compile-checks full reload should replace the generated class object after a structural change")));
		ASSERT_THAT(AreEqual(2, Recorder->CompileCheckCount, TEXT("Full reload should invoke the compile hook for the replacement class")));
		ASSERT_THAT(AreEqual(2, Recorder->PostReloadCount, TEXT("Full reload should invoke the post-reload hook a second time")));
		ASSERT_THAT(AreEqual(2, Recorder->PostReloadHistory.Num(), TEXT("Full reload should record two post-reload events in total")));
		if (Recorder->PostReloadHistory.Num() != 2)
		{ return; }
		ASSERT_THAT(IsTrue(Recorder->PostReloadHistory[1], TEXT("Full reload should report the second post-reload event as a full reload")));
		ASSERT_THAT(IsTrue(Recorder->bLastFullReload, TEXT("Full reload should leave the last post-reload flag in full-reload state")));
		ASSERT_THAT(AreEqual(AdditionalChecksModuleName.ToString(), Recorder->LastModuleName, TEXT("Full reload should continue reporting the target module name")));
		ASSERT_THAT(AreEqual(AdditionalChecksClassName.ToString(), Recorder->LastClassName, TEXT("Full reload should continue reporting the target class name")));

		Recorder->bRejectCompile = true;

		FAngelscriptCompileTraceSummary RejectSummary;
		const bool bRejectedCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::FullReload, AdditionalChecksRejectedModuleName, AdditionalChecksRejectedFilename,
			RejectScript, true, RejectSummary, true);

		ASSERT_THAT(IsFalse(bRejectedCompiled, TEXT("A rejecting additional compile check should fail compilation")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, RejectSummary.CompileResult, TEXT("A rejecting additional compile check should surface an error compile result")));
		ASSERT_THAT(IsTrue(RejectSummary.Diagnostics.Num() > 0, TEXT("A rejecting additional compile check should emit at least one diagnostic")));
		ASSERT_THAT(IsTrue(SummaryContainsDiagnosticMessage(RejectSummary, Recorder->RejectMessage), TEXT("A rejecting additional compile check should preserve the rejection text in diagnostics")));
		ASSERT_THAT(AreEqual(3, Recorder->CompileCheckCount, TEXT("A rejecting additional compile check should still invoke the compile hook")));
		ASSERT_THAT(AreEqual(2, Recorder->PostReloadCount, TEXT("A rejecting additional compile check should not advance the post-reload hook count")));
		ASSERT_THAT(AreEqual(AdditionalChecksRejectedModuleName.ToString(), Recorder->LastModuleName, TEXT("A rejecting additional compile check should report the rejected module name")));
		ASSERT_THAT(AreEqual(AdditionalChecksRejectedClassName.ToString(), Recorder->LastClassName, TEXT("A rejecting additional compile check should report the rejected class name")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, AdditionalChecksRejectedClassName), TEXT("A rejecting additional compile check should not publish the rejected class")));

		}
	}
};

#endif
