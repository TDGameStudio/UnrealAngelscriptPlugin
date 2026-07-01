#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageAnimInstanceTests
// -----------------------------------------------------------------------------
// Coverage for the AnimInstance slice from:
//
//   OpenSpec: test-coverage/coverage-matrix.md
//
// These cases keep the first coverage wave asset-free: they verify AS can define
// AnimInstance subclasses, expose animation variables, and execute the core
// owner/montage/curve query APIs without requiring a skeletal mesh asset.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace
{
	UClass* CompileCoverageAnimInstanceClass(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName)
	{
		return CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			TEXT("ASCoverageAnimInstance.as"),
			ASTEST_AS(R"AS(
UCLASS()
class UCoverageAnimInstance : UAnimInstance
{
	UPROPERTY(BlueprintReadWrite)
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadWrite)
	float Speed = 320.0f;

	UPROPERTY(BlueprintReadWrite)
	float Direction = 45.0f;

	UPROPERTY()
	bool bOwnerQueriesCallable = false;

	UPROPERTY()
	bool bMontageQueriesCallable = false;

	UPROPERTY()
	bool bCurveQueriesCallable = false;

	UFUNCTION()
	void ProbeOwnerQueries()
	{
		APawn OwnerPawn = TryGetPawnOwner();
		USkeletalMeshComponent OwnerComponent = GetOwningComponent();
		bOwnerQueriesCallable = OwnerPawn == nullptr && OwnerComponent != nullptr;
	}

	UFUNCTION()
	void ProbeMontageQueries(UAnimMontage Montage)
	{
		bool bAnyPlaying = IsAnyMontagePlaying();
		UAnimMontage CurrentMontage = GetCurrentActiveMontage();
		bool bMontagePlaying = Montage_IsPlaying(Montage);
		bool bMontageStopped = Montage_GetIsStopped(Montage);
		float Position = Montage_GetPosition(Montage);
		FName Section = Montage_GetCurrentSection(Montage);
		bMontageQueriesCallable = !bAnyPlaying
			&& CurrentMontage == nullptr
			&& !bMontagePlaying
			&& bMontageStopped
			&& Position == 0.0f
			&& Section == NAME_None;
	}

	UFUNCTION()
	void ProbeCurveQueries()
	{
		float32 Value = 0.0f;
		bool bFoundCurve = GetCurveValueWithDefault(n"CoverageCurve", 7.5f, Value);
		bCurveQueriesCallable = !bFoundCurve && Value == 7.5f;
	}
}
)AS"),
			TEXT("UCoverageAnimInstance"));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageAnimInstanceTest,
	"Angelscript.TestModule.Coverage.Animation.AnimInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(AnimInstanceSubclassAndVariables)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageAnimation_AnimInstanceVariables"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* AnimClass = CompileCoverageAnimInstanceClass(*TestRunner, Engine, ModuleName);
		if (AnimClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AnimClass->IsChildOf(UAnimInstance::StaticClass()),
			TEXT("AS AnimInstance coverage class should derive from UAnimInstance")));

		FBoolProperty* InAirProperty = FindFProperty<FBoolProperty>(AnimClass, TEXT("bIsInAir"));
		FDoubleProperty* SpeedProperty = FindFProperty<FDoubleProperty>(AnimClass, TEXT("Speed"));
		FDoubleProperty* DirectionProperty = FindFProperty<FDoubleProperty>(AnimClass, TEXT("Direction"));
		ASSERT_THAT(IsNotNull(InAirProperty, TEXT("bIsInAir animation variable should be generated")));
		ASSERT_THAT(IsNotNull(SpeedProperty, TEXT("Speed animation variable should be generated as FDoubleProperty")));
		ASSERT_THAT(IsNotNull(DirectionProperty, TEXT("Direction animation variable should be generated as FDoubleProperty")));

		UObject* CDO = AnimClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(CDO, TEXT("AS AnimInstance class should expose a CDO")));
		ASSERT_THAT(IsFalse(InAirProperty->GetPropertyValue_InContainer(CDO),
			TEXT("bIsInAir default should match AS initializer")));
		ASSERT_THAT(IsNear(320.0, SpeedProperty->GetPropertyValue_InContainer(CDO), 0.01,
			TEXT("Speed default should match AS initializer")));
		ASSERT_THAT(IsNear(45.0, DirectionProperty->GetPropertyValue_InContainer(CDO), 0.01,
			TEXT("Direction default should match AS initializer")));
	}

	TEST_METHOD(AnimInstanceQueryFunctionsCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageAnimation_QueryFunctions"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* AnimClass = CompileCoverageAnimInstanceClass(*TestRunner, Engine, ModuleName);
		if (AnimClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(FindGeneratedFunction(AnimClass, TEXT("ProbeOwnerQueries")),
			TEXT("ProbeOwnerQueries should compile owner query calls")));
		ASSERT_THAT(IsNotNull(FindGeneratedFunction(AnimClass, TEXT("ProbeMontageQueries")),
			TEXT("ProbeMontageQueries should compile montage query calls")));
		ASSERT_THAT(IsNotNull(FindGeneratedFunction(AnimClass, TEXT("ProbeCurveQueries")),
			TEXT("ProbeCurveQueries should compile curve query calls")));
	}

	TEST_METHOD(AnimInstanceQueryFunctionsExecute)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageAnimation_QueryFunctionsExecute"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* AnimClass = CompileCoverageAnimInstanceClass(*TestRunner, Engine, ModuleName);
		if (AnimClass == nullptr)
		{
			return;
		}

		USkeletalMeshComponent* OwningComponent = NewObject<USkeletalMeshComponent>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(OwningComponent,
			TEXT("AnimInstance query probe should have a transient skeletal mesh component outer")));
		if (OwningComponent == nullptr)
		{
			return;
		}

		UAnimInstance* AnimInstance = NewObject<UAnimInstance>(OwningComponent, AnimClass);
		ASSERT_THAT(IsNotNull(AnimInstance,
			TEXT("AS AnimInstance query probe should instantiate as UAnimInstance")));
		if (AnimInstance == nullptr)
		{
			return;
		}

		FFunctionInvoker OwnerInvoker(*TestRunner, AnimInstance, TEXT("ProbeOwnerQueries"));
		ASSERT_THAT(IsTrue(OwnerInvoker.Call(), TEXT("Owner query probe should execute")));

		FFunctionInvoker MontageInvoker(*TestRunner, AnimInstance, TEXT("ProbeMontageQueries"));
		ASSERT_THAT(IsTrue(MontageInvoker.AddParam<UAnimMontage*>(nullptr).Call(),
			TEXT("Montage query probe should execute with a null montage")));

		FFunctionInvoker CurveInvoker(*TestRunner, AnimInstance, TEXT("ProbeCurveQueries"));
		ASSERT_THAT(IsTrue(CurveInvoker.Call(), TEXT("Curve query probe should execute")));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, AnimInstance, TEXT("bOwnerQueriesCallable"), true,
			TEXT("Owner queries should run and observe the asset-free component-owned state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, AnimInstance, TEXT("bMontageQueriesCallable"), true,
			TEXT("Montage queries should run and observe the asset-free stopped state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, AnimInstance, TEXT("bCurveQueriesCallable"), true,
			TEXT("Curve query should run and return the default value when no curve exists"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
