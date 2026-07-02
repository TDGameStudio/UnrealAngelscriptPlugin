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
//   Single legacy automation test merged into TEST_CLASS.
//   $TOKEN$ replacement computed in TEST_METHOD + ReplaceInline.
//   Original `int Entry()` returning bitmask split into per-aspect functions
//   returning 1/0, plus native-side C++ assertions for mutated component state.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptPrimitiveComponentBindingsTest,
	"Angelscript.TestModule.Bindings.PrimitiveComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FVector ExpectedBoxExtent = FVector(10.0f, 20.0f, 30.0f);
	inline static const FVector ExpectedRelativeLocation = FVector(100.0f, 50.0f, 25.0f);
	static constexpr double BoundsTolerance = 0.01;

	static FString FormatDoubleLiteral(const double Value)
	{
		return LexToString(Value);
	}

	static FString FormatVectorLiteral(const FVector& Value)
	{
		return FString::Printf(
			TEXT("FVector(%s, %s, %s)"),
			*LexToString(Value.X),
			*LexToString(Value.Y),
			*LexToString(Value.Z));
	}

	static UBoxComponent* CreatePrimitiveComponentFixture(AActor*& OutHostActor)
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

	// ====================================================================
	// Section: BoundsCompat
	// ====================================================================

	TEST_METHOD(BoundsAndMutableFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Create fixture
		AActor* HostActor = nullptr;
		UBoxComponent* BoxComponent = CreatePrimitiveComponentFixture(HostActor);
		ASSERT_THAT(IsNotNull(HostActor, TEXT("PrimComp should create transient host actor")));
		ASSERT_THAT(IsNotNull(BoxComponent, TEXT("PrimComp should create transient box component")));

		// Capture baseline values for token replacement
		const FString ComponentPath = BoxComponent->GetPathName();
		const FVector CollisionExtents = BoxComponent->GetCollisionShape().GetExtent();
		const FVector BoundsOrigin = BoxComponent->Bounds.Origin;
		const FVector BoundsExtent = BoxComponent->Bounds.BoxExtent;
		const double BoundsRadius = BoxComponent->Bounds.SphereRadius;

		// Verify native baseline
		ASSERT_THAT(IsTrue(
			CollisionExtents.Equals(ExpectedBoxExtent, 0.0f),
			TEXT("PrimComp native collision extents should match configured box extent")));
		ASSERT_THAT(IsTrue(
			BoundsOrigin.Equals(ExpectedRelativeLocation, BoundsTolerance),
			TEXT("PrimComp native bounds origin should reflect configured relative location")));
		ASSERT_THAT(IsTrue(
			BoundsExtent.Equals(ExpectedBoxExtent, BoundsTolerance),
			TEXT("PrimComp native bounds extent should match configured box extent")));
		ASSERT_THAT(IsNear(
			ExpectedBoxExtent.Size(),
			BoundsRadius,
			BoundsTolerance,
			TEXT("PrimComp native bounds radius should match box extent radius")));
		ASSERT_THAT(IsFalse(
			BoxComponent->bSelectable,
			TEXT("PrimComp native selectable flag should start disabled")));
		ASSERT_THAT(AreEqual(
			ELightmapType::Default,
			BoxComponent->GetLightmapType(),
			TEXT("PrimComp native lightmap type should start at default")));

		// Build script with token replacement
		FString BoundsCompatSource = ASTEST_AS(R"AS(
			int BoundsCompat_CollisionExtents()
			{
				UObject FoundObject = FindObject("__COMPONENT_PATH__");
				UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
				if (Component == null)
				{
					return 0;
				}
				return Component.GetBoundingBoxExtents().Equals(__EXPECTED_BOX_EXTENT__, 0.0f) ? 1 : 0;
			}

			int BoundsCompat_BoundsOrigin()
			{
				UObject FoundObject = FindObject("__COMPONENT_PATH__");
				UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
				if (Component == null)
				{
					return 0;
				}
				return Component.GetBoundsOrigin().Equals(__EXPECTED_BOUNDS_ORIGIN__, __BOUNDS_TOLERANCE__) ? 1 : 0;
			}

			int BoundsCompat_BoundsExtent()
			{
				UObject FoundObject = FindObject("__COMPONENT_PATH__");
				UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
				if (Component == null)
				{
					return 0;
				}
				return Component.GetBoundsExtent().Equals(__EXPECTED_BOUNDS_EXTENT__, __BOUNDS_TOLERANCE__) ? 1 : 0;
			}

			int BoundsCompat_BoundsRadius()
			{
				UObject FoundObject = FindObject("__COMPONENT_PATH__");
				UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
				if (Component == null)
				{
					return 0;
				}
				return Math::IsNearlyEqual(Component.GetBoundsRadius(), __EXPECTED_BOUNDS_RADIUS__, __BOUNDS_TOLERANCE__) ? 1 : 0;
			}

			int BoundsCompat_Selectable()
			{
				UObject FoundObject = FindObject("__COMPONENT_PATH__");
				UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
				if (Component == null)
				{
					return 0;
				}
				if (Component.GetbSelectable())
				{
					return 0;
				}
				Component.SetbSelectable(true);
				return Component.GetbSelectable() ? 1 : 0;
			}

			int BoundsCompat_LightmapType()
			{
				UObject FoundObject = FindObject("__COMPONENT_PATH__");
				UPrimitiveComponent Component = Cast<UPrimitiveComponent>(FoundObject);
				if (Component == null)
				{
					return 0;
				}
				Component.SetLightmapType(ELightmapType::ForceSurface);
				return 1;
			}
			)AS");

		BoundsCompatSource.ReplaceInline(TEXT("__COMPONENT_PATH__"), *ComponentPath.ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		BoundsCompatSource.ReplaceInline(TEXT("__EXPECTED_BOX_EXTENT__"), *FormatVectorLiteral(CollisionExtents), ESearchCase::CaseSensitive);
		BoundsCompatSource.ReplaceInline(TEXT("__EXPECTED_BOUNDS_ORIGIN__"), *FormatVectorLiteral(BoundsOrigin), ESearchCase::CaseSensitive);
		BoundsCompatSource.ReplaceInline(TEXT("__EXPECTED_BOUNDS_EXTENT__"), *FormatVectorLiteral(BoundsExtent), ESearchCase::CaseSensitive);
		BoundsCompatSource.ReplaceInline(TEXT("__EXPECTED_BOUNDS_RADIUS__"), *FormatDoubleLiteral(BoundsRadius), ESearchCase::CaseSensitive);
		BoundsCompatSource.ReplaceInline(TEXT("__BOUNDS_TOLERANCE__"), *FormatDoubleLiteral(BoundsTolerance), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASPrimitiveComponent_BoundsCompat"), BoundsCompatSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_CollisionExtents()"), TEXT("collision extents should match configured box extent"), 1), TEXT("collision extents should match configured box extent")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_BoundsOrigin()"), TEXT("bounds origin should reflect configured relative location"), 1), TEXT("bounds origin should reflect configured relative location")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_BoundsExtent()"), TEXT("bounds extent should match configured box extent"), 1), TEXT("bounds extent should match configured box extent")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_BoundsRadius()"), TEXT("bounds radius should match box extent radius"), 1), TEXT("bounds radius should match box extent radius")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_Selectable()"), TEXT("SetbSelectable should update native component immediately"), 1), TEXT("SetbSelectable should update native component immediately")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int BoundsCompat_LightmapType()"), TEXT("SetLightmapType(ForceSurface) should execute without error"), 1), TEXT("SetLightmapType(ForceSurface) should execute without error")));

		// Native-side assertions for mutations done by script
		ASSERT_THAT(IsTrue(
			BoxComponent->bSelectable,
			TEXT("PrimComp SetbSelectable(true) should update native component")));
		ASSERT_THAT(AreEqual(
			ELightmapType::ForceSurface,
			BoxComponent->GetLightmapType(),
			TEXT("PrimComp SetLightmapType(ForceSurface) should update native lightmap type")));

		// Cleanup
		BoxComponent->MarkAsGarbage();
		HostActor->MarkAsGarbage();
	}
};

#endif
