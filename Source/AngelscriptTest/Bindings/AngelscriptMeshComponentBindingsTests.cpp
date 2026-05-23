// AngelscriptMeshComponentBindingsTests.cpp
// CQTest compile-check for UPoseableMeshComponent, UProjectileMovementComponent,
// USkeletalMeshComponent.
// Automation IDs: Angelscript.TestModule.Bindings.MeshComponent.*

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsCoverage.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "GameFramework/ProjectileMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

static const FBindingsCoverageProfile GMeshCompProfile{
	TEXT("MeshComp"), TEXT(""), TEXT("ASMeshComp"), TEXT("MeshComp"), TEXT("MeshCompBindings"),
};

TEST_CLASS_WITH_FLAGS(FAngelscriptMeshComponentBindingsTest,
	"Angelscript.TestModule.Bindings.MeshComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(ProjectileMovement)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageModuleScope Mod(*TestRunner, Engine, GMeshCompProfile, TEXT("Projectile"), TEXT(R"(
int Projectile_HomingTargetRoundTrip(UProjectileMovementComponent Comp, USceneComponent Target)
{
	if (Comp == nullptr) return 0;
	if (Target == nullptr) return 0;
	Comp.SetHomingTargetComponent(Target);
	const USceneComponent Current = Comp.GetHomingTargetComponent();
	return (Current == Target) ? 1 : 0;
}
)"));
		if (!Mod.IsValid())
		{
			return;
		}

		UProjectileMovementComponent* Movement = NewObject<UProjectileMovementComponent>();
		USceneComponent* Target = NewObject<USceneComponent>();
		if (!TestRunner->TestNotNull(TEXT("Projectile movement component should be constructible from C++ in headless automation"), Movement)
			|| !TestRunner->TestNotNull(TEXT("Scene component target should be constructible from C++ in headless automation"), Target))
		{
			return;
		}

		AngelscriptReflectiveAccess::FASGlobalFunctionInvoker Invoker(
			*TestRunner,
			Engine,
			Mod.GetModule(),
			TEXT("int Projectile_HomingTargetRoundTrip(UProjectileMovementComponent, USceneComponent)"));
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddArgObject(Movement).AddArgObject(Target);
		TestRunner->TestEqual(
			TEXT("UProjectileMovementComponent homing target binding should round-trip a C++-constructed component"),
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			1);
	}

	TEST_METHOD(SkeletalMeshTypeCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageModuleScope Mod(*TestRunner, Engine, GMeshCompProfile, TEXT("Skeletal"), TEXT(R"(
int Skeletal_TypeExists()
{
	USkeletalMeshComponent Comp;
	return 1;
}
		)"));
		if (!Mod.IsValid())
		{
			TestRunner->TestTrue(TEXT("USkeletalMeshComponent type binding module should compile"), false);
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), GMeshCompProfile,
			TEXT("int Skeletal_TypeExists()"), TEXT("USkeletalMeshComponent compiles"), 1);
	}

	TEST_METHOD(SkeletalMeshAssetAccessorsCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageModuleScope Mod(*TestRunner, Engine, GMeshCompProfile, TEXT("SkeletalAssetAccessors"), TEXT(R"(
void Skeletal_SetAndGetAsset(USkeletalMeshComponent Comp, USkeletalMesh Mesh)
{
	Comp.SetSkeletalMeshAsset(Mesh);
	USkeletalMesh CurrentMesh = Comp.GetSkeletalMeshAsset();
}

int Skeletal_SetAndGetAssetEntry()
{
	return 1;
}
		)"));
		if (!Mod.IsValid())
		{
			TestRunner->TestTrue(TEXT("USkeletalMeshComponent asset accessor binding module should compile"), false);
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), GMeshCompProfile,
			TEXT("int Skeletal_SetAndGetAssetEntry()"), TEXT("USkeletalMeshComponent asset accessors compile"), 1);
	}
};

#endif
