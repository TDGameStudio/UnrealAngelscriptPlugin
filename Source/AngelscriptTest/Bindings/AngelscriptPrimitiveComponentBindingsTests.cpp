// ============================================================================
// AngelscriptPrimitiveComponentBindingsTests.cpp
//
// PrimitiveComponent bounds/selectable/lightmap binding coverage — CQTest refactor.
// Automation IDs:
//   Angelscript.TestModule.Bindings.PrimitiveComponent.FAngelscriptPrimitiveComponentBindingsTest.*
//
// Sections:
//   BoundsCompat — collision extents, bounds origin/extent/radius, selectable,
//                  lightmap type round-trip through script
//
// CQTest adaptation notes:
//   Single IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into TEST_CLASS.
//   $TOKEN$ replacement computed in TEST_METHOD + ReplaceInline.
//   Original `int Entry()` returning bitmask split into per-aspect functions
//   returning 1/0, plus native-side C++ assertions for mutated component state.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers (kept from original, scoped to translation unit)
// ----------------------------------------------------------------------------

namespace
{
	static const FVector ExpectedBoxExtent(10.0f, 20.0f, 30.0f);
	static const FVector ExpectedRelativeLocation(100.0f, 50.0f, 25.0f);
	static constexpr double BoundsTolerance = 0.01;

	FString FormatDoubleLiteral(const double Value)
	{
		return LexToString(Value);
	}

	FString FormatVectorLiteral(const FVector& Value)
	{
		return FString::Printf(
			TEXT("FVector(%s, %s, %s)"),
			*LexToString(Value.X),
			*LexToString(Value.Y),
			*LexToString(Value.Z));
	}

	UBoxComponent* CreatePrimitiveComponentFixture(AActor*& OutHostActor)
	{
		OutHostActor = NewObject<AActor>(
			GetTransientPackage(),
			AActor::StaticClass(),
			TEXT("PrimCompBindingsHostActor"),
			RF_Transient);
		if (OutHostActor == nullptr)
		{
			return nullptr;
		}

		UBoxComponent* BoxComponent = NewObject<UBoxComponent>(
			OutHostActor,
			UBoxComponent::StaticClass(),
			TEXT("PrimCompBindingsBox"),
			RF_Transient);
		if (BoxComponent == nullptr)
		{
			return nullptr;
		}

		OutHostActor->AddInstanceComponent(BoxComponent);
		OutHostActor->SetRootComponent(BoxComponent);

		BoxComponent->SetBoxExtent(ExpectedBoxExtent);
		BoxComponent->SetRelativeLocation(ExpectedRelativeLocation);
		BoxComponent->bSelectable = false;
		BoxComponent->SetLightmapType(ELightmapType::Default);
		BoxComponent->UpdateComponentToWorld();
		BoxComponent->UpdateBounds();
		return BoxComponent;
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptPrimitiveComponentBindingsTest,
	"Angelscript.TestModule.Bindings.PrimitiveComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: BoundsCompat
	// ====================================================================

	TEST_METHOD(BoundsCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Create fixture
		AActor* HostActor = nullptr;
		UBoxComponent* BoxComponent = CreatePrimitiveComponentFixture(HostActor);
		if (!TestRunner->TestNotNull(TEXT("PrimComp should create transient host actor"), HostActor) ||
			!TestRunner->TestNotNull(TEXT("PrimComp should create transient box component"), BoxComponent))
		{
			return;
		}

		// Capture baseline values for token replacement
		const FString ComponentPath = BoxComponent->GetPathName();
		const FVector CollisionExtents = BoxComponent->GetCollisionShape().GetExtent();
		const FVector BoundsOrigin = BoxComponent->Bounds.Origin;
		const FVector BoundsExtent = BoxComponent->Bounds.BoxExtent;
		const double BoundsRadius = BoxComponent->Bounds.SphereRadius;

		// Verify native baseline
		TestRunner->TestTrue(TEXT("PrimComp native collision extents should match configured box extent"),
			CollisionExtents.Equals(ExpectedBoxExtent, 0.0f));
		TestRunner->TestTrue(TEXT("PrimComp native bounds origin should reflect configured relative location"),
			BoundsOrigin.Equals(ExpectedRelativeLocation, BoundsTolerance));
		TestRunner->TestTrue(TEXT("PrimComp native bounds extent should match configured box extent"),
			BoundsExtent.Equals(ExpectedBoxExtent, BoundsTolerance));
		TestRunner->TestTrue(TEXT("PrimComp native bounds radius should match box extent radius"),
			FMath::IsNearlyEqual(BoundsRadius, ExpectedBoxExtent.Size(), BoundsTolerance));
		TestRunner->TestFalse(TEXT("PrimComp native selectable flag should start disabled"),
			BoxComponent->bSelectable);
		TestRunner->TestEqual(TEXT("PrimComp native lightmap type should start at default"),
			BoxComponent->GetLightmapType(), ELightmapType::Default);

		// Build script with token replacement
		FString Script = TEXT(R"(
int BoundsCompat_CollisionExtents()
{
	UObject FoundObject = FindObject("__COMPONENT_PATH__");
	UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
	if (Component == null) return 0;
	return Component.GetBoundingBoxExtents().Equals(__EXPECTED_BOX_EXTENT__, 0.0f) ? 1 : 0;
}
int BoundsCompat_BoundsOrigin()
{
	UObject FoundObject = FindObject("__COMPONENT_PATH__");
	UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
	if (Component == null) return 0;
	return Component.GetBoundsOrigin().Equals(__EXPECTED_BOUNDS_ORIGIN__, __BOUNDS_TOLERANCE__) ? 1 : 0;
}
int BoundsCompat_BoundsExtent()
{
	UObject FoundObject = FindObject("__COMPONENT_PATH__");
	UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
	if (Component == null) return 0;
	return Component.GetBoundsExtent().Equals(__EXPECTED_BOUNDS_EXTENT__, __BOUNDS_TOLERANCE__) ? 1 : 0;
}
int BoundsCompat_BoundsRadius()
{
	UObject FoundObject = FindObject("__COMPONENT_PATH__");
	UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
	if (Component == null) return 0;
	return Math::IsNearlyEqual(Component.GetBoundsRadius(), __EXPECTED_BOUNDS_RADIUS__, __BOUNDS_TOLERANCE__) ? 1 : 0;
}
int BoundsCompat_Selectable()
{
	UObject FoundObject = FindObject("__COMPONENT_PATH__");
	UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
	if (Component == null) return 0;
	if (Component.GetbSelectable()) return 0;
	Component.SetbSelectable(true);
	return Component.GetbSelectable() ? 1 : 0;
}
int BoundsCompat_LightmapType()
{
	UObject FoundObject = FindObject("__COMPONENT_PATH__");
	UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
	if (Component == null) return 0;
	Component.SetLightmapType(ELightmapType::ForceSurface);
	return 1;
}
)");

		Script.ReplaceInline(TEXT("__COMPONENT_PATH__"), *ComponentPath.ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_BOX_EXTENT__"), *FormatVectorLiteral(CollisionExtents), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_BOUNDS_ORIGIN__"), *FormatVectorLiteral(BoundsOrigin), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_BOUNDS_EXTENT__"), *FormatVectorLiteral(BoundsExtent), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__EXPECTED_BOUNDS_RADIUS__"), *FormatDoubleLiteral(BoundsRadius), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__BOUNDS_TOLERANCE__"), *FormatDoubleLiteral(BoundsTolerance), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASPrimitiveComponent_BoundsCompat"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_CollisionExtents()"), TEXT("collision extents should match configured box extent"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_BoundsOrigin()"), TEXT("bounds origin should reflect configured relative location"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_BoundsExtent()"), TEXT("bounds extent should match configured box extent"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_BoundsRadius()"), TEXT("bounds radius should match box extent radius"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_Selectable()"), TEXT("SetbSelectable should update native component immediately"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_LightmapType()"), TEXT("SetLightmapType(ForceSurface) should execute without error"), 1);

		// Native-side assertions for mutations done by script
		TestRunner->TestTrue(TEXT("PrimComp SetbSelectable(true) should update native component"),
			BoxComponent->bSelectable);
		TestRunner->TestEqual(TEXT("PrimComp SetLightmapType(ForceSurface) should update native lightmap type"),
			BoxComponent->GetLightmapType(), ELightmapType::ForceSurface);

		// Cleanup
		BoxComponent->MarkAsGarbage();
		HostActor->MarkAsGarbage();
	}
};

#endif
