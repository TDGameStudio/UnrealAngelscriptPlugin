// AngelscriptMeshComponentBindingsTests.cpp
// CQTest compile-check for UPoseableMeshComponent, UProjectileMovementComponent,
// USkeletalMeshComponent.
// Automation IDs: Angelscript.TestModule.Bindings.MeshComponent.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "GameFramework/ProjectileMovementComponent.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptMeshComponentBindingsTest,
	"Angelscript.TestModule.Bindings.MeshComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}
	AFTER_ALL()
	{
		FAngelscriptEngine& E = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(E);
	}

	TEST_METHOD(ProjectileMovement)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMeshComponent_Projectile"), ASTEST_AS(R"AS(
			int Projectile_HomingTargetRoundTrip(UProjectileMovementComponent Comp, USceneComponent Target)
			{
				if (Comp == nullptr)
				{
					return 0;
				}
				if (Target == nullptr)
				{
					return 0;
				}
				Comp.SetHomingTargetComponent(Target);
				const USceneComponent Current = Comp.GetHomingTargetComponent();
				return (Current == Target) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			return;
		}

		UProjectileMovementComponent* Movement = NewObject<UProjectileMovementComponent>();
		USceneComponent* Target = NewObject<USceneComponent>();
		ASSERT_THAT(IsNotNull(Movement, TEXT("Projectile movement component should be constructible from C++ in headless automation")));
		ASSERT_THAT(IsNotNull(Target, TEXT("Scene component target should be constructible from C++ in headless automation")));

		FASGlobalFunctionInvoker Invoker(
			*TestRunner,
			Engine,
			Mod.GetModule(),
			TEXT("int Projectile_HomingTargetRoundTrip(UProjectileMovementComponent, USceneComponent)"));
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddArgObject(Movement).AddArgObject(Target);
		ASSERT_THAT(AreEqual(
			1,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("UProjectileMovementComponent homing target binding should round-trip a C++-constructed component")));
	}

	TEST_METHOD(SkeletalMeshTypeCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMeshComponent_Skeletal"), ASTEST_AS(R"AS(
			int Skeletal_TypeExists()
			{
				USkeletalMeshComponent Comp;
				return 1;
			}
		)AS"));
		if (!Mod.IsValid())
		{
			ASSERT_THAT(IsTrue(false, TEXT("USkeletalMeshComponent type binding module should compile")));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int Skeletal_TypeExists()"), TEXT("USkeletalMeshComponent compiles"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(SkeletalMeshAssetAccessorsCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMeshComponent_SkeletalAssetAccessors"), ASTEST_AS(R"AS(
			void Skeletal_SetAndGetAsset(USkeletalMeshComponent Comp, USkeletalMesh Mesh)
			{
				Comp.SetSkeletalMeshAsset(Mesh);
				USkeletalMesh CurrentMesh = Comp.GetSkeletalMeshAsset();
			}

			int Skeletal_SetAndGetAssetEntry()
			{
				return 1;
			}
		)AS"));
		if (!Mod.IsValid())
		{
			ASSERT_THAT(IsTrue(false, TEXT("USkeletalMeshComponent asset accessor binding module should compile")));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int Skeletal_SetAndGetAssetEntry()"), TEXT("USkeletalMeshComponent asset accessors compile"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
