#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional - Round1 deep-fill (default <Component>.<Field> override on CDO)
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptComponentDefaultPropertyOverrideTests,
	"Angelscript.TestModule.Functional.Component.DefaultPropertyOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DefaultStatementsAffectComponentCDOs)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalComponentDefaultOverride"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FRotator ExpectedMeshRotation(0.0f, 45.0f, 0.0f);
		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalComponentDefaultOverride.as"),
			TEXT(R"AS(
UCLASS()
class AFunctionalDefaultOverrideActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USphereComponent Sphere;

	UPROPERTY(DefaultComponent, Attach = Sphere)
	UStaticMeshComponent Mesh;

	default Sphere.SphereRadius = 128.0;
	default Mesh.SetHiddenInGame(true);
	default Mesh.SetCastShadow(false);
	default Mesh.SetRelativeRotation(FRotator(0.0f, 45.0f, 0.0f));
}
)AS"),
			TEXT("AFunctionalDefaultOverrideActor"));
		if (ActorClass == nullptr) { return; }

		AActor* CDO = Cast<AActor>(ActorClass->GetDefaultObject());
		if (!TestRunner->TestNotNull(TEXT("Actor CDO should be available"), CDO)) { return; }

		USphereComponent* SphereCDO = nullptr;
		UStaticMeshComponent* MeshCDO = nullptr;
		for (UActorComponent* Component : CDO->GetComponents())
		{
			if (USphereComponent* AsSphere = Cast<USphereComponent>(Component))
			{
				SphereCDO = AsSphere;
			}
			else if (UStaticMeshComponent* AsMesh = Cast<UStaticMeshComponent>(Component))
			{
				MeshCDO = AsMesh;
			}
		}

		if (TestRunner->TestNotNull(TEXT("Sphere CDO component should exist"), SphereCDO))
		{
			TestRunner->TestEqual(TEXT("Sphere.SphereRadius CDO default should be 128.0"), SphereCDO->GetUnscaledSphereRadius(), 128.0f);
		}

		if (TestRunner->TestNotNull(TEXT("Mesh CDO component should exist"), MeshCDO))
		{
			TestRunner->TestTrue(TEXT("Mesh.bHiddenInGame CDO default should be true"), MeshCDO->bHiddenInGame);
			TestRunner->TestFalse(TEXT("Mesh.CastShadow CDO default should be false"), MeshCDO->CastShadow);
			TestRunner->TestTrue(
				TEXT("Mesh.SetRelativeRotation default statement should set CDO relative rotation"),
				MeshCDO->GetRelativeRotation().Equals(ExpectedMeshRotation));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
