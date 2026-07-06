#include "AngelscriptNativeScriptTestObject.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptComponentMetadataValidationTests,
	"Angelscript.TestModule.Generator.Component",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ValidModuleName = FName(TEXT("ASComponentVerifyClassValid"));
	inline static const FString ValidFilename = FString(TEXT("ASComponentVerifyClassValid.as"));
	inline static const FName ValidActorClassName = FName(TEXT("AComponentVerifyClassValidActor"));

	inline static const FName InvalidAttachParentModuleName = FName(TEXT("ASComponentInvalidAttachParent"));
	inline static const FString InvalidAttachParentFilename = FString(TEXT("ComponentInvalidAttachParent.as"));
	inline static const FName InvalidAttachParentClassName = FName(TEXT("AComponentInvalidAttachParent"));
	inline static const FString InvalidAttachParentDiagnosticFragment = FString(
		TEXT("Attach parent MissingParent does not exist for DefaultComponent Billboard."));

	inline static const FName MissingOverrideTargetModuleName = FName(TEXT("ASComponentMissingOverrideTarget"));
	inline static const FString MissingOverrideTargetFilename = FString(TEXT("ComponentMissingOverrideTarget.as"));
	inline static const FName MissingOverrideTargetBaseClassName = FName(TEXT("ABaseOverrideMissing"));
	inline static const FName MissingOverrideTargetDerivedClassName = FName(TEXT("ADerivedOverrideMissing"));
	inline static const FString MissingOverrideTargetDiagnosticFragment = FString(
		TEXT("OverrideComponent ADerivedOverrideMissing::ReplacementRoot could not find component MissingScene in base class to override."));

	inline static const FName AbstractOverrideModuleName = FName(TEXT("ASComponentVerifyClassAbstractOverride"));
	inline static const FString AbstractOverrideFilename = FString(TEXT("ASComponentVerifyClassAbstractOverride.as"));
	inline static const FName AbstractOverrideDerivedClassName = FName(TEXT("AComponentVerifyClassConcreteMissingOverrideActor"));
	inline static const FName NonSceneParentModuleName = FName(TEXT("ASComponentVerifyClassNonSceneParent"));
	inline static const FString NonSceneParentFilename = FString(TEXT("ASComponentVerifyClassNonSceneParent.as"));
	inline static const FName NonSceneParentClassName = FName(TEXT("AComponentVerifyClassNonSceneParentActor"));
	inline static const FName EditorOnlyParentModuleName = FName(TEXT("ASComponentVerifyClassEditorOnlyParent"));
	inline static const FString EditorOnlyParentFilename = FString(TEXT("ASComponentVerifyClassEditorOnlyParent.as"));
	inline static const FName EditorOnlyParentClassName = FName(TEXT("AComponentVerifyClassEditorOnlyParentActor"));
	inline static const FString EditorOnlyParentDiagnosticFragment = FString(
		TEXT("Non-Editor DefaultComponent RuntimeBillboard cannot be attached to Editor-Only attach parent EditorParent."));

	inline static const FName EditorOnlyRootModuleName = FName(TEXT("ASComponentVerifyClassEditorOnlyRoot"));
	inline static const FString EditorOnlyRootFilename = FString(TEXT("ASComponentVerifyClassEditorOnlyRoot.as"));
	inline static const FName EditorOnlyRootClassName = FName(TEXT("AComponentVerifyClassEditorOnlyRootActor"));
	inline static const FString EditorOnlyRootDiagnosticFragment = FString(
		TEXT("Editor-Only DefaultComponent EditorRoot cannot be the RootComponent of non-editor actor AComponentVerifyClassEditorOnlyRootActor."));

	inline static const FName NotSpawnableModuleName = FName(TEXT("ASComponentVerifyClassNotSpawnable"));
	inline static const FString NotSpawnableFilename = FString(TEXT("ASComponentVerifyClassNotSpawnable.as"));
	inline static const FName NotSpawnableClassName = FName(TEXT("AComponentVerifyClassNotSpawnableActor"));
	inline static const FName DeprecatedModuleName = FName(TEXT("ASComponentVerifyClassDeprecated"));
	inline static const FString DeprecatedFilename = FString(TEXT("ASComponentVerifyClassDeprecated.as"));
	inline static const FName DeprecatedClassName = FName(TEXT("AComponentVerifyClassDeprecatedActor"));
	inline static const FString DeprecatedDiagnosticFragment = FString(TEXT("is deprecated"));

	inline static const FName DeveloperOnlyBypassModuleName = FName(TEXT("Dev.ASComponentVerifyClassDeveloperOnlyBypass"));
	inline static const FString DeveloperOnlyBypassFilename = FString(TEXT("Dev.ASComponentVerifyClassDeveloperOnlyBypass.as"));
	inline static const FName DeveloperOnlyBypassClassName = FName(TEXT("AComponentVerifyClassDeveloperOnlyBypassActor"));
	inline static const FString DeveloperOnlyBypassDiagnosticFragment = FString(
		TEXT("Editor-Only DefaultComponent EditorRoot cannot be the RootComponent of non-editor actor AComponentVerifyClassDeveloperOnlyBypassActor."));

	struct FCompileObservation
	{
		bool bCompiled = false;
		FAngelscriptCompileTraceSummary Summary;
	};

	static FCompileObservation CompileCase(
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& ScriptSource)
	{
		FCompileObservation Observation;
		Observation.bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			Filename,
			ScriptSource,
			true,
			Observation.Summary,
			true);
		return Observation;
	}

	static const FAngelscriptCompileTraceDiagnosticSummary* FindDiagnosticContaining(
		const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics,
		const FString& Fragment,
		const bool bExpectedError)
	{
		return Diagnostics.FindByPredicate(
			[&Fragment, bExpectedError](const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
			{
				return Diagnostic.bIsError == bExpectedError && Diagnostic.Message.Contains(Fragment);
			});
	}

	static bool HasErrorDiagnostic(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate(
			[](const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
			{
				return Diagnostic.bIsError;
			});
	}

	static FString DescribeCompileObservation(const FCompileObservation& Observation)
	{
		FString DiagnosticsText;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Observation.Summary.Diagnostics)
		{
			DiagnosticsText += FString::Printf(
				TEXT("[%s:%d:%d:%s:%s]"),
				*Diagnostic.Section,
				Diagnostic.Row,
				Diagnostic.Column,
				Diagnostic.bIsError ? TEXT("error") : Diagnostic.bIsInfo ? TEXT("info") : TEXT("warning"),
				*Diagnostic.Message);
		}

		return FString::Printf(
			TEXT("compiled=%s result=%d modules=[%s] diagnostics=%d %s"),
			Observation.bCompiled ? TEXT("true") : TEXT("false"),
			static_cast<int32>(Observation.Summary.CompileResult),
			*FString::Join(Observation.Summary.ModuleNames, TEXT(",")),
			Observation.Summary.Diagnostics.Num(),
			*DiagnosticsText);
	}

	static bool IsHandledResult(const ECompileResult CompileResult)
	{
		return CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled;
	}

	static const UASClass::FDefaultComponent* FindDefaultComponentEntryByName(const UASClass* ScriptClass, FName ComponentName)
	{
		if (ScriptClass == nullptr)
		{
			return nullptr;
		}

		return ScriptClass->DefaultComponents.FindByPredicate(
			[ComponentName](const UASClass::FDefaultComponent& Entry)
			{
				return Entry.ComponentName == ComponentName;
			});
	}

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

	TEST_METHOD(ValidRootAndAttachedSceneComponentsPublishMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ValidModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class AComponentVerifyClassValidActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent RootScene;

				UPROPERTY(DefaultComponent, Attach = RootScene)
				UBillboardComponent Billboard;
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, ValidModuleName, ValidFilename, ScriptSource);
		UASClass* ActorClass = Cast<UASClass>(FindGeneratedClass(&Engine, ValidActorClassName));
		ASSERT_THAT(IsTrue(Observation.bCompiled, TEXT("Valid component metadata should compile")));
		ASSERT_THAT(IsTrue(IsHandledResult(Observation.Summary.CompileResult), TEXT("Valid component metadata should report a handled compile result")));
		ASSERT_THAT(IsFalse(HasErrorDiagnostic(Observation.Summary.Diagnostics), TEXT("Valid component metadata should not emit error diagnostics")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Valid component metadata should publish the actor class")));
		if (ActorClass == nullptr)
		{
			return;
		}

		const UASClass::FDefaultComponent* RootEntry = FindDefaultComponentEntryByName(ActorClass, TEXT("RootScene"));
		const UASClass::FDefaultComponent* BillboardEntry = FindDefaultComponentEntryByName(ActorClass, TEXT("Billboard"));
		ASSERT_THAT(IsNotNull(RootEntry, TEXT("Valid component metadata should retain the root component entry")));
		ASSERT_THAT(IsNotNull(BillboardEntry, TEXT("Valid component metadata should retain the attached component entry")));
		if (RootEntry == nullptr || BillboardEntry == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(RootEntry->bIsRoot, TEXT("Valid component metadata should mark RootScene as root")));
		ASSERT_THAT(IsTrue(RootEntry->Attach.IsNone(), TEXT("Valid component metadata should leave RootScene unattached")));
		ASSERT_THAT(AreEqual(FName(TEXT("RootScene")), BillboardEntry->Attach, TEXT("Valid component metadata should attach Billboard to RootScene")));
	}

	TEST_METHOD(InvalidAttachParentFailsClosed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*InvalidAttachParentModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UComponentInvalidAttachParentRoot : USceneComponent
			{
			}

			UCLASS()
			class AComponentInvalidAttachParent : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UComponentInvalidAttachParentRoot RootScene;

				UPROPERTY(DefaultComponent, Attach = MissingParent)
				UBillboardComponent Billboard;
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, InvalidAttachParentModuleName, InvalidAttachParentFilename, ScriptSource);
		const TSharedPtr<FAngelscriptModuleDesc> FailedModuleRecord =
			Engine.GetModuleByModuleName(InvalidAttachParentModuleName.ToString());

		ASSERT_THAT(IsFalse(Observation.bCompiled, TEXT("Invalid attach-parent metadata should fail compilation instead of silently succeeding")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Observation.Summary.CompileResult, TEXT("Invalid attach-parent metadata should surface an error compile result")));
		ASSERT_THAT(IsNotNull(
			FindDiagnosticContaining(Observation.Summary.Diagnostics, InvalidAttachParentDiagnosticFragment, true),
			TEXT("Invalid attach-parent metadata should report the missing attach-parent diagnostic")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, InvalidAttachParentClassName), TEXT("Invalid attach-parent metadata should not publish the generated actor class after failure")));
		ASSERT_THAT(IsTrue(!FailedModuleRecord.IsValid(), TEXT("Invalid attach-parent metadata should not publish a module record after failure")));
	}

	TEST_METHOD(MissingOverrideTargetFailsClosed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*MissingOverrideTargetModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UBaseOverrideMissingRoot : USceneComponent
			{
			}

			UCLASS()
			class UDerivedOverrideMissingRoot : UBaseOverrideMissingRoot
			{
			}

			UCLASS()
			class ABaseOverrideMissing : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UBaseOverrideMissingRoot RootScene;
			}

			UCLASS()
			class ADerivedOverrideMissing : ABaseOverrideMissing
			{
				UPROPERTY(OverrideComponent = MissingScene)
				UDerivedOverrideMissingRoot ReplacementRoot;
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, MissingOverrideTargetModuleName, MissingOverrideTargetFilename, ScriptSource);
		const TSharedPtr<FAngelscriptModuleDesc> FailedModuleRecord =
			Engine.GetModuleByModuleName(MissingOverrideTargetModuleName.ToString());

		ASSERT_THAT(IsFalse(Observation.bCompiled, TEXT("Missing override-target metadata should fail compilation instead of silently succeeding")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Observation.Summary.CompileResult, TEXT("Missing override-target metadata should surface an error compile result")));
		ASSERT_THAT(IsNotNull(
			FindDiagnosticContaining(Observation.Summary.Diagnostics, MissingOverrideTargetDiagnosticFragment, true),
			TEXT("Missing override-target metadata should report the missing base-component diagnostic")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, MissingOverrideTargetDerivedClassName), TEXT("Missing override-target metadata should not publish the derived actor class after failure")));
		ASSERT_THAT(IsTrue(!FailedModuleRecord.IsValid(), TEXT("Missing override-target metadata should not publish a live module record after failure")));
		ASSERT_THAT(IsTrue(
			FindGeneratedClass(&Engine, MissingOverrideTargetBaseClassName) == nullptr || FindGeneratedClass(&Engine, MissingOverrideTargetDerivedClassName) == nullptr,
			TEXT("Missing override-target metadata should not silently publish the broken derived class")));
	}

	TEST_METHOD(AbstractBaseComponentRequiresConcreteOverride)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*AbstractOverrideModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Abstract)
			class AComponentVerifyClassAbstractBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UAngelscriptVerifyClassAbstractSceneComponent AbstractRoot;
			}

			UCLASS()
			class AComponentVerifyClassConcreteMissingOverrideActor : AComponentVerifyClassAbstractBaseActor
			{
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, AbstractOverrideModuleName, AbstractOverrideFilename, ScriptSource);
		ASSERT_THAT(IsFalse(Observation.bCompiled, TEXT("Concrete actor inheriting an abstract component should fail without an override")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Observation.Summary.CompileResult, TEXT("Missing abstract-component override should surface an error compile result")));
		ASSERT_THAT(IsTrue(
			HasErrorDiagnostic(Observation.Summary.Diagnostics),
			TEXT("Missing abstract-component override should emit an error diagnostic")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, AbstractOverrideDerivedClassName), TEXT("Missing abstract-component override should not publish the concrete actor")));
	}

	TEST_METHOD(AttachParentMustBeSceneComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*NonSceneParentModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class AComponentVerifyClassNonSceneParentActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent RootScene;

				UPROPERTY(DefaultComponent)
				UAngelscriptVerifyClassPlainActorComponent PlainParent;

				UPROPERTY(DefaultComponent, Attach = PlainParent)
				UBillboardComponent Billboard;
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, NonSceneParentModuleName, NonSceneParentFilename, ScriptSource);
		ASSERT_THAT(IsFalse(Observation.bCompiled, TEXT("DefaultComponent attach parent should reject non-scene components")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Observation.Summary.CompileResult, TEXT("Non-scene attach parent should surface an error compile result")));
		ASSERT_THAT(IsTrue(
			HasErrorDiagnostic(Observation.Summary.Diagnostics),
			TEXT("Non-scene attach parent should emit an error diagnostic")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, NonSceneParentClassName), TEXT("Non-scene attach parent should not publish the generated actor")));
	}

	TEST_METHOD(NonEditorComponentCannotAttachToEditorOnlyParent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, true);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*EditorOnlyParentModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class AComponentVerifyClassEditorOnlyParentActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent RootScene;

				#if EDITOR
				UPROPERTY(DefaultComponent, Attach = RootScene)
				USceneComponent EditorParent;
				#endif

				UPROPERTY(DefaultComponent, Attach = EditorParent)
				UBillboardComponent RuntimeBillboard;
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, EditorOnlyParentModuleName, EditorOnlyParentFilename, ScriptSource);
		ASSERT_THAT(IsTrue(Observation.bCompiled, *FString::Printf(
			TEXT("Editor-only parent violation should remain a handled compile with diagnostics; %s"),
			*DescribeCompileObservation(Observation))));
		ASSERT_THAT(IsTrue(IsHandledResult(Observation.Summary.CompileResult), TEXT("Editor-only parent violation should report a handled compile result")));
		ASSERT_THAT(IsNotNull(
			FindDiagnosticContaining(Observation.Summary.Diagnostics, EditorOnlyParentDiagnosticFragment, true),
			TEXT("Editor-only parent violation should report the VerifyClass diagnostic")));
		ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, EditorOnlyParentClassName), TEXT("Editor-only parent violation should still publish the generated actor while reporting diagnostics")));
	}

	TEST_METHOD(EditorOnlyRootRejectedForRuntimeActor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, true);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*EditorOnlyRootModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class AComponentVerifyClassEditorOnlyRootActor : AActor
			{
				#if EDITOR
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent EditorRoot;
				#endif
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, EditorOnlyRootModuleName, EditorOnlyRootFilename, ScriptSource);
		ASSERT_THAT(IsTrue(Observation.bCompiled, *FString::Printf(
			TEXT("Editor-only root violation should remain a handled compile with diagnostics; %s"),
			*DescribeCompileObservation(Observation))));
		ASSERT_THAT(IsTrue(IsHandledResult(Observation.Summary.CompileResult), TEXT("Editor-only root violation should report a handled compile result")));
		ASSERT_THAT(IsNotNull(
			FindDiagnosticContaining(Observation.Summary.Diagnostics, EditorOnlyRootDiagnosticFragment, true),
			TEXT("Editor-only root violation should report the VerifyClass diagnostic")));
		ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, EditorOnlyRootClassName), TEXT("Editor-only root violation should still publish the generated actor while reporting diagnostics")));
	}

	TEST_METHOD(NotAngelscriptSpawnableComponentRejectedAsDefaultComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*NotSpawnableModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class AComponentVerifyClassNotSpawnableActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UAngelscriptVerifyClassNotSpawnableSceneComponent BlockedRoot;
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, NotSpawnableModuleName, NotSpawnableFilename, ScriptSource);
		ASSERT_THAT(IsFalse(Observation.bCompiled, TEXT("NotAngelscriptSpawnable component should be rejected as a DefaultComponent")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Observation.Summary.CompileResult, TEXT("NotAngelscriptSpawnable component should surface an error compile result")));
		ASSERT_THAT(IsTrue(
			HasErrorDiagnostic(Observation.Summary.Diagnostics),
			TEXT("NotAngelscriptSpawnable component should emit an error diagnostic")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, NotSpawnableClassName), TEXT("NotAngelscriptSpawnable component should not publish the generated actor")));
	}

	TEST_METHOD(DeprecatedComponentWarnsButPublishesActor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DeprecatedModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Deprecated)
			class UComponentVerifyClassDeprecatedSceneComponent : USceneComponent
			{
			}

			UCLASS()
			class AComponentVerifyClassDeprecatedActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UComponentVerifyClassDeprecatedSceneComponent DeprecatedRoot;
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, DeprecatedModuleName, DeprecatedFilename, ScriptSource);
		UClass* ActorClass = FindGeneratedClass(&Engine, DeprecatedClassName);
		ASSERT_THAT(IsTrue(Observation.bCompiled, TEXT("Deprecated component should warn but still compile")));
		ASSERT_THAT(IsTrue(IsHandledResult(Observation.Summary.CompileResult), TEXT("Deprecated component warning should keep a handled compile result")));
		ASSERT_THAT(IsFalse(HasErrorDiagnostic(Observation.Summary.Diagnostics), TEXT("Deprecated component warning should not emit an error diagnostic")));
		ASSERT_THAT(IsNotNull(
			FindDiagnosticContaining(Observation.Summary.Diagnostics, DeprecatedDiagnosticFragment, false),
			TEXT("Deprecated component should emit a warning diagnostic")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Deprecated component warning should still publish the actor class")));
	}

	TEST_METHOD(DeveloperOnlyModuleReportsEditorOnlyRootDiagnosticDuringGeneration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, true);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*DeveloperOnlyBypassModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class AComponentVerifyClassDeveloperOnlyBypassActor : AActor
			{
				#if EDITOR
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent EditorRoot;
				#endif
			}
			)AS");

		const FCompileObservation Observation = CompileCase(Engine, DeveloperOnlyBypassModuleName, DeveloperOnlyBypassFilename, ScriptSource);
		UASClass* ActorClass = Cast<UASClass>(FindGeneratedClass(&Engine, DeveloperOnlyBypassClassName));
		ASSERT_THAT(IsTrue(Observation.bCompiled, TEXT("Developer-only module should remain a handled compile while reporting VerifyClass diagnostics")));
		ASSERT_THAT(IsTrue(IsHandledResult(Observation.Summary.CompileResult), TEXT("Developer-only module should report a handled compile result")));
		ASSERT_THAT(IsNotNull(
			FindDiagnosticContaining(Observation.Summary.Diagnostics, DeveloperOnlyBypassDiagnosticFragment, true),
			*FString::Printf(
				TEXT("Developer-only module should report the current VerifyClass diagnostic before module swap-in; %s"),
				*DescribeCompileObservation(Observation))));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Developer-only module should still publish the actor class while reporting diagnostics")));
		if (ActorClass == nullptr)
		{
			return;
		}

		FObjectProperty* EditorRootProperty = FindFProperty<FObjectProperty>(ActorClass, TEXT("EditorRoot"));
		ASSERT_THAT(IsNotNull(EditorRootProperty, TEXT("Dev-style helper module should still reflect EditorRoot while reporting diagnostics")));
		if (EditorRootProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(EditorRootProperty->HasAnyPropertyFlags(CPF_EditorOnly), TEXT("Dev-style helper module should keep EditorRoot marked editor-only")));
	}
};

#endif