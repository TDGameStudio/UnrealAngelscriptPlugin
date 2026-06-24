#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptComponentMetadataValidationTests,
	"Angelscript.TestModule.ClassGenerator.Component",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
inline static const FName ComponentMetadataValidationModuleName = FName(TEXT("ASComponentInvalidAttachParent"));
static constexpr TCHAR ComponentMetadataValidationFilename[] = TEXT("ComponentInvalidAttachParent.as");
inline static const FName InvalidAttachParentClassName = FName(TEXT("AComponentInvalidAttachParent"));
inline static const FString InvalidAttachParentDiagnosticFragment = FString(
	TEXT("Attach parent MissingParent does not exist for DefaultComponent Billboard."));
inline static const FName MissingOverrideTargetModuleName = FName(TEXT("ASComponentMissingOverrideTarget"));
static constexpr TCHAR MissingOverrideTargetFilename[] = TEXT("ComponentMissingOverrideTarget.as");
inline static const FName MissingOverrideTargetBaseClassName = FName(TEXT("ABaseOverrideMissing"));
inline static const FName MissingOverrideTargetDerivedClassName = FName(TEXT("ADerivedOverrideMissing"));
inline static const FString MissingOverrideTargetDiagnosticFragment = FString(
	TEXT("OverrideComponent ADerivedOverrideMissing::ReplacementRoot could not find component MissingScene in base class to override."));

static const FAngelscriptCompileTraceDiagnosticSummary* FindErrorDiagnosticContaining(
	const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics,
	const FString& Fragment)
{
	return Diagnostics.FindByPredicate(
		[&Fragment](const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
		{
			return Diagnostic.bIsError && Diagnostic.Message.Contains(Fragment);
		});
}

static FString BuildInvalidAttachParentScript()
{
	return TEXT(R"AS(
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
}

static FString BuildMissingOverrideTargetScript()
{
	return TEXT(R"AS(
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
}

public:
	TEST_METHOD(InvalidAttachParentFailsClosed)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ComponentMetadataValidationModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::FullReload, ComponentMetadataValidationModuleName, ComponentMetadataValidationFilename,
			BuildInvalidAttachParentScript(), true, Summary, true);

		const FAngelscriptCompileTraceDiagnosticSummary* MissingParentDiagnostic =
			FindErrorDiagnosticContaining(Summary.Diagnostics, InvalidAttachParentDiagnosticFragment);
		const TSharedPtr<FAngelscriptModuleDesc> FailedModuleRecord =
			Engine.GetModuleByModuleName(ComponentMetadataValidationModuleName.ToString());

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Invalid attach-parent metadata should fail compilation instead of silently succeeding")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Invalid attach-parent metadata should surface an error compile result")));
		ASSERT_THAT(IsTrue(Summary.bUsedPreprocessor, TEXT("Invalid attach-parent metadata should compile through the annotated preprocessor path")));
		ASSERT_THAT(IsTrue(Summary.Diagnostics.Num() > 0, TEXT("Invalid attach-parent metadata should emit at least one diagnostic")));
		ASSERT_THAT(IsNotNull(MissingParentDiagnostic, TEXT("Invalid attach-parent metadata should report the missing attach-parent diagnostic")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, InvalidAttachParentClassName), TEXT("Invalid attach-parent metadata should not publish the generated actor class after failure")));
		ASSERT_THAT(IsTrue(!FailedModuleRecord.IsValid(), TEXT("Invalid attach-parent metadata should not publish a module record after failure")));

		}
	}

	TEST_METHOD(MissingOverrideTargetFailsClosed)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*MissingOverrideTargetModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::FullReload, MissingOverrideTargetModuleName, MissingOverrideTargetFilename,
			BuildMissingOverrideTargetScript(), true, Summary, true);

		const FAngelscriptCompileTraceDiagnosticSummary* MissingOverrideTargetDiagnostic =
			FindErrorDiagnosticContaining(Summary.Diagnostics, MissingOverrideTargetDiagnosticFragment);
		const TSharedPtr<FAngelscriptModuleDesc> FailedModuleRecord =
			Engine.GetModuleByModuleName(MissingOverrideTargetModuleName.ToString());

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Missing override-target metadata should fail compilation instead of silently succeeding")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Missing override-target metadata should surface an error compile result")));
		ASSERT_THAT(IsTrue(Summary.bUsedPreprocessor, TEXT("Missing override-target metadata should compile through the annotated preprocessor path")));
		ASSERT_THAT(IsTrue(Summary.Diagnostics.Num() > 0, TEXT("Missing override-target metadata should emit at least one diagnostic")));
		ASSERT_THAT(IsNotNull(MissingOverrideTargetDiagnostic, TEXT("Missing override-target metadata should report the missing base-component diagnostic")));
		ASSERT_THAT(IsNull(FindGeneratedClass(&Engine, MissingOverrideTargetDerivedClassName), TEXT("Missing override-target metadata should not publish the derived actor class after failure")));
		ASSERT_THAT(IsTrue(!FailedModuleRecord.IsValid(), TEXT("Missing override-target metadata should not publish a live module record after failure")));
		ASSERT_THAT(IsTrue(
			FindGeneratedClass(&Engine, MissingOverrideTargetBaseClassName) == nullptr || FindGeneratedClass(&Engine, MissingOverrideTargetDerivedClassName) == nullptr,
			TEXT("Missing override-target metadata should not silently publish the broken derived class")));

		}
	}
};

#endif
