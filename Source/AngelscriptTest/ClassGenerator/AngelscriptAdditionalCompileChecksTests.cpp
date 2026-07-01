#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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

	inline static const FName AdditionalChecksSoftReloadModuleName = FName(TEXT("AdditionalChecksSoftReloadMod"));
	inline static const FString AdditionalChecksSoftReloadFilename = FString(TEXT("AdditionalChecksSoftReloadMod.as"));
	inline static const FName AdditionalChecksSoftReloadClassName = FName(TEXT("UAdditionalChecksSoftReloadTarget"));

	inline static const FName AdditionalChecksParentChainModuleName = FName(TEXT("AdditionalChecksParentChainMod"));
	inline static const FString AdditionalChecksParentChainFilename = FString(TEXT("AdditionalChecksParentChainMod.as"));
	inline static const FName AdditionalChecksParentChainClassName = FName(TEXT("AAdditionalChecksParentChainTarget"));

	inline static const FName AdditionalChecksFailedReloadModuleName = FName(TEXT("AdditionalChecksFailedReloadMod"));
	inline static const FString AdditionalChecksFailedReloadFilename = FString(TEXT("AdditionalChecksFailedReloadMod.as"));
	inline static const FName AdditionalChecksFailedReloadClassName = FName(TEXT("UAdditionalChecksFailedReloadTarget"));

	inline static const FName AdditionalChecksMultiClassModuleName = FName(TEXT("AdditionalChecksMultiClassMod"));
	inline static const FString AdditionalChecksMultiClassFilename = FString(TEXT("AdditionalChecksMultiClassMod.as"));
	inline static const FName AdditionalChecksMultiClassFirstClassName = FName(TEXT("UAdditionalChecksMultiClassFirstTarget"));
	inline static const FName AdditionalChecksMultiClassSecondClassName = FName(TEXT("UAdditionalChecksMultiClassSecondTarget"));

	inline static const FName AdditionalChecksInvalidBindingModuleName = FName(TEXT("AdditionalChecksInvalidBindingMod"));
	inline static const FString AdditionalChecksInvalidBindingFilename = FString(TEXT("AdditionalChecksInvalidBindingMod.as"));
	inline static const FName AdditionalChecksInvalidBindingClassName = FName(TEXT("UAdditionalChecksInvalidBindingTarget"));

	inline static const FName AdditionalChecksStructOnlyModuleName = FName(TEXT("AdditionalChecksStructOnlyMod"));
	inline static const FString AdditionalChecksStructOnlyFilename = FString(TEXT("AdditionalChecksStructOnlyMod.as"));
	inline static const FName AdditionalChecksStructOnlyName = FName(TEXT("AdditionalChecksStructOnlyTarget"));

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

	static UScriptStruct* FindGeneratedStruct(FAngelscriptEngine& Engine, const FName StructName)
	{
		UPackage* Package = Engine.GetPackageInstance();
		return Package != nullptr ? FindObject<UScriptStruct>(Package, *StructName.ToString()) : nullptr;
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
		TArray<FString> CompileModuleHistory;
		TArray<FString> CompileClassHistory;
		TArray<FString> PostReloadModuleHistory;
		TArray<FString> PostReloadClassHistory;
		TArray<bool> PostReloadHistory;

		virtual bool ScriptCompileAdditionalChecks(TSharedPtr<FAngelscriptModuleDesc> ModuleDesc, TSharedPtr<FAngelscriptClassDesc> ClassDesc) override
		{
			++CompileCheckCount;
			LastModuleName = ModuleDesc.IsValid() ? ModuleDesc->ModuleName : FString();
			LastClassName = ClassDesc.IsValid() ? ClassDesc->ClassName : FString();
			CompileModuleHistory.Add(LastModuleName);
			CompileClassHistory.Add(LastClassName);

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
			PostReloadModuleHistory.Add(LastModuleName);
			PostReloadClassHistory.Add(LastClassName);
			PostReloadHistory.Add(bFullReload);
		}
	};

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(InvokesCompileAndPostReloadHooksAcrossFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TSharedPtr<FTestAdditionalCompileChecks> Recorder = MakeShared<FTestAdditionalCompileChecks>();
		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(Recorder));

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.DiscardModule(*AdditionalChecksModuleName.ToString());
		};

		const FString CompileReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksTarget : UObject
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS");

		const FString CompileReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksTarget : UObject
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksModuleName, AdditionalChecksFilename, CompileReloadV1Source),
			TEXT("Initial additional-compile-checks module compile should succeed")));

		UClass* InitialGeneratedClass = FindGeneratedClass(&Engine, AdditionalChecksClassName);
		ASSERT_THAT(IsNotNull(InitialGeneratedClass, TEXT("Initial additional-compile-checks compile should publish the generated script class")));
		if (InitialGeneratedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, Recorder->CompileCheckCount, TEXT("Initial annotated compile should invoke the compile hook exactly once")));
		ASSERT_THAT(AreEqual(1, Recorder->PostReloadCount, TEXT("Initial annotated compile should invoke the post-reload hook exactly once")));
		ASSERT_THAT(AreEqual(AdditionalChecksModuleName.ToString(), Recorder->LastModuleName, TEXT("Initial annotated compile should report the module name to the hook")));
		ASSERT_THAT(AreEqual(AdditionalChecksClassName.ToString(), Recorder->LastClassName, TEXT("Initial annotated compile should report the generated class name to the hook")));
		ASSERT_THAT(AreEqual(1, Recorder->PostReloadHistory.Num(), TEXT("Initial annotated compile should record exactly one post-reload event")));
		if (Recorder->PostReloadHistory.Num() != 1)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Recorder->PostReloadHistory[0], TEXT("Initial annotated compile helper should surface the post-reload hook as a full reload")));
		ASSERT_THAT(IsTrue(Recorder->bLastFullReload, TEXT("Initial annotated compile helper should leave the last post-reload flag in full-reload state")));

		ECompileResult ReloadResult = ECompileResult::Error;
		const bool bReloadCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			AdditionalChecksModuleName,
			AdditionalChecksFilename,
			CompileReloadV2Source,
			ReloadResult);

		ASSERT_THAT(IsTrue(bReloadCompiled, TEXT("Additional-compile-checks structural reload should compile successfully")));
		if (!bReloadCompiled)
		{
			return;
		}

		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Additional-compile-checks structural reload should stay on a handled reload path")));
		if (!IsHandledReloadResult(ReloadResult))
		{
			return;
		}

		UClass* ReloadedGeneratedClass = FindGeneratedClass(&Engine, AdditionalChecksClassName);
		ASSERT_THAT(IsNotNull(ReloadedGeneratedClass, TEXT("Additional-compile-checks full reload should keep the generated script class queryable")));
		if (ReloadedGeneratedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreNotEqual(ReloadedGeneratedClass, InitialGeneratedClass, TEXT("Additional-compile-checks full reload should replace the generated class object after a structural change")));
		ASSERT_THAT(AreEqual(2, Recorder->CompileCheckCount, TEXT("Full reload should invoke the compile hook for the replacement class")));
		ASSERT_THAT(AreEqual(2, Recorder->PostReloadCount, TEXT("Full reload should invoke the post-reload hook a second time")));
		ASSERT_THAT(AreEqual(2, Recorder->PostReloadHistory.Num(), TEXT("Full reload should record two post-reload events in total")));
		if (Recorder->PostReloadHistory.Num() != 2)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Recorder->PostReloadHistory[1], TEXT("Full reload should report the second post-reload event as a full reload")));
		ASSERT_THAT(IsTrue(Recorder->bLastFullReload, TEXT("Full reload should leave the last post-reload flag in full-reload state")));
		ASSERT_THAT(AreEqual(AdditionalChecksModuleName.ToString(), Recorder->LastModuleName, TEXT("Full reload should continue reporting the target module name")));
		ASSERT_THAT(AreEqual(AdditionalChecksClassName.ToString(), Recorder->LastClassName, TEXT("Full reload should continue reporting the target class name")));
	}

	TEST_METHOD(RejectingCompileCheckFailsCompilationWithoutPublishingClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TSharedPtr<FTestAdditionalCompileChecks> Recorder = MakeShared<FTestAdditionalCompileChecks>();
		Recorder->bRejectCompile = true;
		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(Recorder));

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.DiscardModule(*AdditionalChecksRejectedModuleName.ToString());
		};

		const FString RejectingCompileSource = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksRejectedTarget : UObject
			{
				UPROPERTY()
				int Value = 3;
			}
			)AS");

		FAngelscriptCompileTraceSummary RejectSummary;
		const bool bRejectedCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			AdditionalChecksRejectedModuleName,
			AdditionalChecksRejectedFilename,
			RejectingCompileSource,
			true,
			RejectSummary,
			true);

		ASSERT_THAT(IsFalse(bRejectedCompiled, TEXT("A rejecting additional compile check should fail compilation")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, RejectSummary.CompileResult, TEXT("A rejecting additional compile check should surface an error compile result")));
		ASSERT_THAT(IsTrue(RejectSummary.Diagnostics.Num() > 0, TEXT("A rejecting additional compile check should emit at least one diagnostic")));
		ASSERT_THAT(IsTrue(SummaryContainsDiagnosticMessage(RejectSummary, Recorder->RejectMessage), TEXT("A rejecting additional compile check should preserve the rejection text in diagnostics")));
		ASSERT_THAT(AreEqual(1, Recorder->CompileCheckCount, TEXT("A rejecting additional compile check should invoke the compile hook once")));
		ASSERT_THAT(AreEqual(0, Recorder->PostReloadCount, TEXT("A rejecting additional compile check should not advance the post-reload hook count")));
		ASSERT_THAT(AreEqual(AdditionalChecksRejectedModuleName.ToString(), Recorder->LastModuleName, TEXT("A rejecting additional compile check should report the rejected module name")));
		ASSERT_THAT(AreEqual(AdditionalChecksRejectedClassName.ToString(), Recorder->LastClassName, TEXT("A rejecting additional compile check should report the rejected class name")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, AdditionalChecksRejectedClassName), TEXT("A rejecting additional compile check should not publish the rejected class")));
	}

	TEST_METHOD(RecordsSoftReloadFlagForBodyOnlyReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TSharedPtr<FTestAdditionalCompileChecks> Recorder = MakeShared<FTestAdditionalCompileChecks>();
		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(Recorder));

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.DiscardModule(*AdditionalChecksSoftReloadModuleName.ToString());
		};

		const FString SoftReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksSoftReloadTarget : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					return 1;
				}
			}
			)AS");

		const FString SoftReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksSoftReloadTarget : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					return 2;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksSoftReloadModuleName, AdditionalChecksSoftReloadFilename, SoftReloadV1Source),
			TEXT("Initial soft-reload target compile should succeed")));

		ASSERT_THAT(IsNotNull(
			FindGeneratedClass(&Engine, AdditionalChecksSoftReloadClassName),
			TEXT("Initial soft-reload target compile should publish the generated script class")));
		ASSERT_THAT(AreEqual(1, Recorder->CompileCheckCount, TEXT("Initial soft-reload target compile should invoke the compile hook once")));
		ASSERT_THAT(AreEqual(1, Recorder->PostReloadCount, TEXT("Initial soft-reload target compile should invoke the post-reload hook once")));

		ECompileResult SoftReloadResult = ECompileResult::Error;
		const bool bSoftReloadCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			AdditionalChecksSoftReloadModuleName,
			AdditionalChecksSoftReloadFilename,
			SoftReloadV2Source,
			SoftReloadResult);

		ASSERT_THAT(IsTrue(bSoftReloadCompiled, TEXT("Body-only reload should compile on the soft reload path")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(SoftReloadResult), TEXT("Body-only reload should stay on a handled reload path")));
		ASSERT_THAT(AreEqual(2, Recorder->CompileCheckCount, TEXT("Body-only soft reload should invoke the compile hook again")));
		ASSERT_THAT(AreEqual(2, Recorder->PostReloadCount, TEXT("Body-only soft reload should invoke the post-reload hook again")));
		ASSERT_THAT(AreEqual(2, Recorder->PostReloadHistory.Num(), TEXT("Soft reload scenario should record initial and reload post events")));
		if (Recorder->PostReloadHistory.Num() != 2)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Recorder->PostReloadHistory[0], TEXT("Initial compile should be reported as full reload")));
		ASSERT_THAT(IsFalse(Recorder->PostReloadHistory[1], TEXT("Body-only reload should pass bFullReload=false to the post-reload hook")));
		ASSERT_THAT(IsFalse(Recorder->bLastFullReload, TEXT("Body-only reload should leave the last post-reload flag in soft-reload state")));
		ASSERT_THAT(AreEqual(AdditionalChecksSoftReloadClassName.ToString(), Recorder->LastClassName, TEXT("Body-only reload should keep reporting the soft-reloaded class name")));
	}

	TEST_METHOD(InvokesChecksRegisteredOnMultipleAncestorClasses)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TSharedPtr<FTestAdditionalCompileChecks> ObjectRecorder = MakeShared<FTestAdditionalCompileChecks>();
		TSharedPtr<FTestAdditionalCompileChecks> ActorRecorder = MakeShared<FTestAdditionalCompileChecks>();
		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(ObjectRecorder));
		Engine.AdditionalCompileChecks.Add(AActor::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(ActorRecorder));

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.AdditionalCompileChecks.Remove(AActor::StaticClass());
			Engine.DiscardModule(*AdditionalChecksParentChainModuleName.ToString());
		};

		const FString ParentChainSource = ASTEST_AS(R"AS(
			UCLASS()
			class AAdditionalChecksParentChainTarget : AActor
			{
				UPROPERTY()
				int Value = 5;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksParentChainModuleName, AdditionalChecksParentChainFilename, ParentChainSource),
			TEXT("Parent-chain additional-compile-checks module should compile successfully")));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, AdditionalChecksParentChainClassName);
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Parent-chain compile should publish the generated actor class")));
		ASSERT_THAT(AreEqual(1, ActorRecorder->CompileCheckCount, TEXT("Check registered on AActor should run for an AS AActor subclass")));
		ASSERT_THAT(AreEqual(1, ObjectRecorder->CompileCheckCount, TEXT("Check registered on UObject should run through the AS class parent chain")));
		ASSERT_THAT(AreEqual(1, ActorRecorder->PostReloadCount, TEXT("Post-reload check registered on AActor should run for an AS AActor subclass")));
		ASSERT_THAT(AreEqual(1, ObjectRecorder->PostReloadCount, TEXT("Post-reload check registered on UObject should run through the AS class parent chain")));
		ASSERT_THAT(AreEqual(AdditionalChecksParentChainClassName.ToString(), ActorRecorder->LastClassName, TEXT("AActor check should receive the generated actor class name")));
		ASSERT_THAT(AreEqual(AdditionalChecksParentChainClassName.ToString(), ObjectRecorder->LastClassName, TEXT("UObject check should receive the generated actor class name")));
	}

	TEST_METHOD(RejectingReloadKeepsPreviouslyPublishedClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TSharedPtr<FTestAdditionalCompileChecks> Recorder = MakeShared<FTestAdditionalCompileChecks>();
		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(Recorder));

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.DiscardModule(*AdditionalChecksFailedReloadModuleName.ToString());
		};

		const FString FailedReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksFailedReloadTarget : UObject
			{
				UPROPERTY()
				int Value = 7;
			}
			)AS");

		const FString FailedReloadRejectedV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksFailedReloadTarget : UObject
			{
				UPROPERTY()
				int Value = 7;

				UPROPERTY()
				int RejectedAddedValue = 9;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksFailedReloadModuleName, AdditionalChecksFailedReloadFilename, FailedReloadV1Source),
			TEXT("Initial failed-reload preservation module compile should succeed")));

		UClass* InitialGeneratedClass = FindGeneratedClass(&Engine, AdditionalChecksFailedReloadClassName);
		ASSERT_THAT(IsNotNull(InitialGeneratedClass, TEXT("Initial failed-reload preservation compile should publish the generated class")));
		if (InitialGeneratedClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNull(
			FindFProperty<FIntProperty>(InitialGeneratedClass, TEXT("RejectedAddedValue")),
			TEXT("Initial failed-reload preservation class should not expose the rejected future property")));
		ASSERT_THAT(AreEqual(1, Recorder->CompileCheckCount, TEXT("Initial failed-reload preservation compile should invoke the compile hook once")));
		ASSERT_THAT(AreEqual(1, Recorder->PostReloadCount, TEXT("Initial failed-reload preservation compile should invoke the post-reload hook once")));

		Recorder->bRejectCompile = true;
		const int32 PostReloadCountBeforeRejectedReload = Recorder->PostReloadCount;

		FAngelscriptCompileTraceSummary RejectedReloadSummary;
		const bool bRejectedReloadCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			AdditionalChecksFailedReloadModuleName,
			AdditionalChecksFailedReloadFilename,
			FailedReloadRejectedV2Source,
			true,
			RejectedReloadSummary,
			true);

		ASSERT_THAT(IsFalse(bRejectedReloadCompiled, TEXT("Rejecting additional compile check should fail a reload of an already published class")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, RejectedReloadSummary.CompileResult, TEXT("Rejected reload should surface an error compile result")));
		ASSERT_THAT(IsTrue(SummaryContainsDiagnosticMessage(RejectedReloadSummary, Recorder->RejectMessage), TEXT("Rejected reload should preserve the rejection diagnostic")));
		ASSERT_THAT(AreEqual(2, Recorder->CompileCheckCount, TEXT("Rejected reload should still invoke the compile hook for the attempted replacement")));
		ASSERT_THAT(AreEqual(PostReloadCountBeforeRejectedReload, Recorder->PostReloadCount, TEXT("Rejected reload should not invoke post-reload checks")));

		UClass* ClassAfterRejectedReload = FindGeneratedClass(&Engine, AdditionalChecksFailedReloadClassName);
		ASSERT_THAT(AreEqual(InitialGeneratedClass, ClassAfterRejectedReload, TEXT("Rejected reload should keep the previously published generated class as the live class")));
		ASSERT_THAT(IsNull(
			FindFProperty<FIntProperty>(ClassAfterRejectedReload, TEXT("RejectedAddedValue")),
			TEXT("Rejected reload should not leak the rejected replacement layout onto the live class")));
	}

	TEST_METHOD(InvokesHooksForEveryClassInAModule)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TSharedPtr<FTestAdditionalCompileChecks> Recorder = MakeShared<FTestAdditionalCompileChecks>();
		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(Recorder));

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.DiscardModule(*AdditionalChecksMultiClassModuleName.ToString());
		};

		const FString MultiClassSource = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksMultiClassFirstTarget : UObject
			{
				UPROPERTY()
				int FirstValue = 11;
			}

			UCLASS()
			class UAdditionalChecksMultiClassSecondTarget : UObject
			{
				UPROPERTY()
				int SecondValue = 13;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksMultiClassModuleName, AdditionalChecksMultiClassFilename, MultiClassSource),
			TEXT("Multi-class additional-compile-checks module should compile successfully")));

		ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, AdditionalChecksMultiClassFirstClassName), TEXT("Multi-class compile should publish the first generated class")));
		ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, AdditionalChecksMultiClassSecondClassName), TEXT("Multi-class compile should publish the second generated class")));
		ASSERT_THAT(AreEqual(2, Recorder->CompileCheckCount, TEXT("Multi-class compile should invoke compile checks for each generated class")));
		ASSERT_THAT(AreEqual(2, Recorder->PostReloadCount, TEXT("Multi-class compile should invoke post-reload checks for each generated class")));
		ASSERT_THAT(IsTrue(Recorder->CompileClassHistory.Contains(AdditionalChecksMultiClassFirstClassName.ToString()), TEXT("Compile check history should include the first class")));
		ASSERT_THAT(IsTrue(Recorder->CompileClassHistory.Contains(AdditionalChecksMultiClassSecondClassName.ToString()), TEXT("Compile check history should include the second class")));
		ASSERT_THAT(IsTrue(Recorder->PostReloadClassHistory.Contains(AdditionalChecksMultiClassFirstClassName.ToString()), TEXT("Post-reload history should include the first class")));
		ASSERT_THAT(IsTrue(Recorder->PostReloadClassHistory.Contains(AdditionalChecksMultiClassSecondClassName.ToString()), TEXT("Post-reload history should include the second class")));
	}

	TEST_METHOD(IgnoresInvalidRegisteredCheckBinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), TSharedPtr<FAngelscriptAdditionalCompileChecks>());

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.DiscardModule(*AdditionalChecksInvalidBindingModuleName.ToString());
		};

		const FString InvalidBindingSource = ASTEST_AS(R"AS(
			UCLASS()
			class UAdditionalChecksInvalidBindingTarget : UObject
			{
				UPROPERTY()
				int Value = 17;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksInvalidBindingModuleName, AdditionalChecksInvalidBindingFilename, InvalidBindingSource),
			TEXT("Invalid additional-compile-check binding should not block compilation")));
		ASSERT_THAT(IsNotNull(
			FindGeneratedClass(&Engine, AdditionalChecksInvalidBindingClassName),
			TEXT("Invalid additional-compile-check binding should not prevent generated class publication")));
	}

	TEST_METHOD(SkipsPostReloadCheckForScriptStructs)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TSharedPtr<FTestAdditionalCompileChecks> Recorder = MakeShared<FTestAdditionalCompileChecks>();
		Engine.AdditionalCompileChecks.Add(UObject::StaticClass(), StaticCastSharedPtr<FAngelscriptAdditionalCompileChecks>(Recorder));

		ON_SCOPE_EXIT
		{
			Engine.AdditionalCompileChecks.Remove(UObject::StaticClass());
			Engine.DiscardModule(*AdditionalChecksStructOnlyModuleName.ToString());
		};

		const FString StructOnlySource = ASTEST_AS(R"AS(
			USTRUCT()
			struct FAdditionalChecksStructOnlyTarget
			{
				UPROPERTY()
				int Value = 19;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, AdditionalChecksStructOnlyModuleName, AdditionalChecksStructOnlyFilename, StructOnlySource),
			TEXT("Struct-only additional-compile-checks module should compile successfully")));

		UScriptStruct* GeneratedStruct = FindGeneratedStruct(Engine, AdditionalChecksStructOnlyName);
		ASSERT_THAT(IsNotNull(GeneratedStruct, TEXT("Struct-only module should publish the generated script struct")));
		ASSERT_THAT(AreEqual(0, Recorder->PostReloadCount, TEXT("Post-reload additional compile checks should skip AS struct descriptors")));
	}
};

#endif
