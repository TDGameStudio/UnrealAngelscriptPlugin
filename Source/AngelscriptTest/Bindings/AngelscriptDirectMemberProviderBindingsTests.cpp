#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptDirectMemberProviderBindingsTest,
	"Angelscript.TestModule.Bindings.DirectMemberProviders",
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

	TEST_METHOD(MigratedMemberPointerSignaturesCompileTogether)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString Source = ASTEST_AS(R"AS(
			int ExerciseBodyInstance(FBodyInstance& Body, FBodyInstance& Other, const FTransform& OtherTransform)
			{
				UBodySetup BodySetup = Body.GetBodySetup();
				bool bWelded = Body.Weld(Other, OtherTransform);
				Body.UnWeld(Other);
				Body.SetUseCCD(true);
				return (BodySetup == nullptr ? 0 : 1) + (bWelded ? 1 : 0);
			}

			bool ExerciseInputMapping(const FInputActionKeyMapping& Left, const FInputActionKeyMapping& Right)
			{
				return Left == Right;
			}

			void ExerciseFXSystemComponent(UFXSystemComponent Component)
			{
				Component.DeactivateImmediate();
			}

			int ExerciseInputSettings(UInputSettings Settings)
			{
				FName UniqueAction = Settings.GetUniqueActionName(n"Action");
				FName UniqueAxis = Settings.GetUniqueAxisName(n"Axis");
				int Count = Settings.GetActionMappings().Num();
				Count += Settings.GetAxisMappings().Num();
				Count += Settings.GetSpeechMappings().Num();
				Count += Settings.DoesActionExist(n"Action") ? 1 : 0;
				Count += Settings.DoesAxisExist(n"Axis") ? 1 : 0;
				Count += Settings.DoesSpeechExist(n"Speech") ? 1 : 0;
				return Count + (UniqueAction == UniqueAction ? 1 : 0) + (UniqueAxis == UniqueAxis ? 1 : 0);
			}

			int ExerciseLocalPlayer(ULocalPlayer LocalPlayer)
			{
				UGameInstance GameInstance = LocalPlayer.GetGameInstance();
				return LocalPlayer.GetControllerId() + (GameInstance == nullptr ? 0 : 1);
			}

			bool ExercisePackage(UPackage Package)
			{
				return Package.IsDirty();
			}

			int ExerciseSkeletalMesh(USkeletalMeshComponent SkeletalMesh, USkeletalMesh Mesh)
			{
				int LinkedInstanceCount = SkeletalMesh.GetLinkedAnimInstances().Num();
				SkeletalMesh.SetSkeletalMeshAsset(Mesh);
				USkeletalMesh CurrentMesh = SkeletalMesh.GetSkeletalMeshAsset();
				SkeletalMesh.UpdateLODStatus();
				SkeletalMesh.InvalidateCachedBounds();
				return LinkedInstanceCount + (CurrentMesh == nullptr ? 0 : 1);
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASDirectMemberProviders"), Source);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("All migrated member-pointer signatures should compile in one module")));
	}
};

#endif
