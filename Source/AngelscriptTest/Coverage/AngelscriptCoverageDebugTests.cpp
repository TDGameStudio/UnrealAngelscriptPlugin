#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageDebugTests
// -----------------------------------------------------------------------------
// Comprehensive debug drawing and visualization coverage for AngelScript,
// following the matrix from Documents/Coverage/Coverage_DebugAndLogging.md.
//
// Test axes covered:
//   * DrawDebugShapes            - DrawDebugLine, DrawDebugSphere, DrawDebugBox, etc.
//   * DrawDebugParameters        - Color, Duration, Thickness, Persistent lines
//   * DrawDebugAdvanced          - DrawDebugString, DrawDebugCoordinateSystem, DrawDebugArrow
//   * ObjectInspection           - GetName, GetClass, GetFullName
//
// Pattern: Compile script modules with debug drawing calls, spawn actors, verify
// that debug drawing APIs are callable and compile correctly. Note: We verify API
// availability and compilation success, not actual visual output.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_DebugAndLogging.md
// Submatrix 3 (Debug Drawing), Submatrix 8 (Debug Helpers)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageDebugTest,
	"Angelscript.TestModule.Coverage.Debug",
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

	// -------------------------------------------------------------------------
	// DrawDebugLine: Basic line drawing
	// -------------------------------------------------------------------------
	TEST_METHOD(DrawDebugLine)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_DrawDebugLine"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugDrawDebugLine.as"),
			ASTEST_AS(R"AS(
			// Actor that draws debug lines
			UCLASS()
			class ADrawDebugLineActor : AActor
			{
				UPROPERTY()
				bool bDrawLinesCalled = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Start = GetActorLocation();
					FVector End = Start + FVector(100.0f, 0.0f, 0.0f);

					// Basic debug line (red)
					System::DrawDebugLine(
						GetWorld(),
						Start,
						End,
						FLinearColor::Red,
						false,   // bPersistentLines
						2.0f,    // LifeTime
						0,       // DepthPriority
						2.0f     // Thickness
					);

					// Another line (green)
					FVector End2 = Start + FVector(0.0f, 100.0f, 0.0f);
					System::DrawDebugLine(
						GetWorld(),
						Start,
						End2,
						FLinearColor::Green,
						false,
						2.0f,
						0,
						1.0f
					);

					// Vertical line (blue)
					FVector End3 = Start + FVector(0.0f, 0.0f, 100.0f);
					System::DrawDebugLine(
						GetWorld(),
						Start,
						End3,
						FLinearColor::Blue,
						false,
						2.0f,
						0,
						3.0f
					);

					bDrawLinesCalled = true;
				}
			}
			)AS"),
			TEXT("ADrawDebugLineActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DrawDebugLine actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DrawDebugLine actor should spawn")));

		// Verify the function was called
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDrawLinesCalled"), true,
			TEXT("bDrawLinesCalled should be true"));
	}

	// -------------------------------------------------------------------------
	// DrawDebugSphere: Sphere drawing
	// -------------------------------------------------------------------------
	TEST_METHOD(DrawDebugSphere)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_DrawDebugSphere"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugDrawDebugSphere.as"),
			ASTEST_AS(R"AS(
			// Actor that draws debug spheres
			UCLASS()
			class ADrawDebugSphereActor : AActor
			{
				UPROPERTY()
				bool bDrawSpheresCalled = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Center = GetActorLocation();

					// Small sphere (red)
					System::DrawDebugSphere(
						GetWorld(),
						Center,
						50.0f,        // Radius
						12,           // Segments
						FLinearColor::Red,
						false,        // bPersistentLines
						2.0f,         // LifeTime
						0,            // DepthPriority
						1.0f          // Thickness
					);

					// Medium sphere (green)
					FVector Center2 = Center + FVector(200.0f, 0.0f, 0.0f);
					System::DrawDebugSphere(
						GetWorld(),
						Center2,
						100.0f,
						16,
						FLinearColor::Green,
						false,
						2.0f,
						0,
						2.0f
					);

					// Large sphere (blue)
					FVector Center3 = Center + FVector(400.0f, 0.0f, 0.0f);
					System::DrawDebugSphere(
						GetWorld(),
						Center3,
						150.0f,
						24,
						FLinearColor::Blue,
						false,
						2.0f,
						0,
						3.0f
					);

					bDrawSpheresCalled = true;
				}
			}
			)AS"),
			TEXT("ADrawDebugSphereActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DrawDebugSphere actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DrawDebugSphere actor should spawn")));

		// Verify the function was called
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDrawSpheresCalled"), true,
			TEXT("bDrawSpheresCalled should be true"));
	}

	// -------------------------------------------------------------------------
	// DrawDebugBox: Box drawing
	// -------------------------------------------------------------------------
	TEST_METHOD(DrawDebugBox)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_DrawDebugBox"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugDrawDebugBox.as"),
			ASTEST_AS(R"AS(
			// Actor that draws debug boxes
			UCLASS()
			class ADrawDebugBoxActor : AActor
			{
				UPROPERTY()
				bool bDrawBoxesCalled = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Center = GetActorLocation();

					// Small box (red)
					FVector Extent1 = FVector(50.0f, 50.0f, 50.0f);
					System::DrawDebugBox(
						GetWorld(),
						Center,
						Extent1,
						FLinearColor::Red,
						false,        // bPersistentLines
						2.0f,         // LifeTime
						0,            // DepthPriority
						2.0f          // Thickness
					);

					// Rectangular box (green)
					FVector Center2 = Center + FVector(200.0f, 0.0f, 0.0f);
					FVector Extent2 = FVector(100.0f, 50.0f, 25.0f);
					System::DrawDebugBox(
						GetWorld(),
						Center2,
						Extent2,
						FLinearColor::Green,
						false,
						2.0f,
						0,
						1.5f
					);

					// Tall box (blue)
					FVector Center3 = Center + FVector(400.0f, 0.0f, 0.0f);
					FVector Extent3 = FVector(30.0f, 30.0f, 150.0f);
					System::DrawDebugBox(
						GetWorld(),
						Center3,
						Extent3,
						FLinearColor::Blue,
						false,
						2.0f,
						0,
						3.0f
					);

					bDrawBoxesCalled = true;
				}
			}
			)AS"),
			TEXT("ADrawDebugBoxActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DrawDebugBox actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DrawDebugBox actor should spawn")));

		// Verify the function was called
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDrawBoxesCalled"), true,
			TEXT("bDrawBoxesCalled should be true"));
	}

	// -------------------------------------------------------------------------
	// DrawDebugParameters: Testing color, duration, and thickness variations
	// -------------------------------------------------------------------------
	TEST_METHOD(DrawDebugParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_DrawDebugParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugDrawDebugParameters.as"),
			ASTEST_AS(R"AS(
			// Actor testing various draw debug parameters
			UCLASS()
			class ADrawDebugParamsActor : AActor
			{
				UPROPERTY()
				bool bTestsComplete = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Base = GetActorLocation();

					// Test different colors
					System::DrawDebugLine(GetWorld(), Base, Base + FVector(100, 0, 0),
						FLinearColor::Red, false, 2.0f, 0, 1.0f);
					System::DrawDebugLine(GetWorld(), Base, Base + FVector(0, 100, 0),
						FLinearColor::Green, false, 2.0f, 0, 1.0f);
					System::DrawDebugLine(GetWorld(), Base, Base + FVector(0, 0, 100),
						FLinearColor::Blue, false, 2.0f, 0, 1.0f);
					System::DrawDebugLine(GetWorld(), Base, Base + FVector(100, 100, 0),
						FLinearColor::Yellow, false, 2.0f, 0, 1.0f);

					// Test different thicknesses
					FVector ThicknessBase = Base + FVector(200.0f, 0.0f, 0.0f);
					System::DrawDebugLine(GetWorld(), ThicknessBase, ThicknessBase + FVector(0, 100, 0),
						FLinearColor::White, false, 2.0f, 0, 1.0f);   // Thin
					System::DrawDebugLine(GetWorld(), ThicknessBase + FVector(20, 0, 0), ThicknessBase + FVector(20, 100, 0),
						FLinearColor::White, false, 2.0f, 0, 3.0f);   // Medium
					System::DrawDebugLine(GetWorld(), ThicknessBase + FVector(40, 0, 0), ThicknessBase + FVector(40, 100, 0),
						FLinearColor::White, false, 2.0f, 0, 5.0f);   // Thick

					// Test different durations
					FVector DurationBase = Base + FVector(400.0f, 0.0f, 0.0f);
					System::DrawDebugSphere(GetWorld(), DurationBase, 30.0f, 12,
						FLinearColor::Red, false, 1.0f, 0, 1.0f);     // 1 second
					System::DrawDebugSphere(GetWorld(), DurationBase + FVector(80, 0, 0), 30.0f, 12,
						FLinearColor::Green, false, 5.0f, 0, 1.0f);   // 5 seconds
					System::DrawDebugSphere(GetWorld(), DurationBase + FVector(160, 0, 0), 30.0f, 12,
						FLinearColor::Blue, false, 10.0f, 0, 1.0f);   // 10 seconds

					// Test persistent lines (remain until cleared)
					FVector PersistentBase = Base + FVector(600.0f, 0.0f, 0.0f);
					System::DrawDebugLine(GetWorld(), PersistentBase, PersistentBase + FVector(0, 0, 100),
						FLinearColor(1.0f, 0.5f, 0.0f), true, -1.0f, 0, 2.0f);

					// Test custom colors
					FLinearColor CustomColor = FLinearColor(0.5f, 0.2f, 0.8f, 1.0f);  // Purple
					System::DrawDebugBox(GetWorld(), Base + FVector(800.0f, 0.0f, 0.0f),
						FVector(40, 40, 40), CustomColor, false, 2.0f, 0, 2.0f);

					bTestsComplete = true;
				}
			}
			)AS"),
			TEXT("ADrawDebugParamsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DrawDebugParameters actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DrawDebugParameters actor should spawn")));

		// Verify tests completed
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bTestsComplete"), true,
			TEXT("bTestsComplete should be true"));
	}

	// -------------------------------------------------------------------------
	// DrawDebugAdvanced: Arrow, Capsule, and String drawing
	// -------------------------------------------------------------------------
	TEST_METHOD(DrawDebugAdvanced)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_DrawDebugAdvanced"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugDrawDebugAdvanced.as"),
			ASTEST_AS(R"AS(
			// Actor testing advanced debug drawing functions
			UCLASS()
			class ADrawDebugAdvancedActor : AActor
			{
				UPROPERTY()
				bool bAdvancedDrawingComplete = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector Base = GetActorLocation();

					// DrawDebugArrow
					FVector ArrowStart = Base;
					FVector ArrowEnd = Base + FVector(200.0f, 0.0f, 0.0f);
					System::DrawDebugArrow(
						GetWorld(),
						ArrowStart,
						ArrowEnd,
						50.0f,         // ArrowSize
						FLinearColor::Red,
						false,
						2.0f,
						0,
						2.0f
					);

					// DrawDebugCapsule
					FVector CapsuleCenter = Base + FVector(0.0f, 200.0f, 0.0f);
					System::DrawDebugCapsule(
						GetWorld(),
						CapsuleCenter,
						100.0f,        // HalfHeight
						50.0f,         // Radius
						FQuat::Identity,
						FLinearColor::Green,
						false,
						2.0f,
						0,
						1.5f
					);

					// DrawDebugString (3D text in world)
					FVector TextLocation = Base + FVector(0.0f, 0.0f, 150.0f);
					System::DrawDebugString(
						GetWorld(),
						TextLocation,
						"Debug Text",
						nullptr,       // TestBaseActor
						FLinearColor::White,
						2.0f,          // Duration
						true           // bDrawShadow
					);

					// DrawDebugCoordinateSystem
					FVector CoordSystemLoc = Base + FVector(400.0f, 0.0f, 0.0f);
					System::DrawDebugCoordinateSystem(
						GetWorld(),
						CoordSystemLoc,
						FRotator::ZeroRotator,
						100.0f,        // Scale
						false,
						2.0f,
						0,
						2.0f
					);

					// DrawDebugPoint
					FVector PointLoc = Base + FVector(0.0f, 400.0f, 0.0f);
					System::DrawDebugPoint(
						GetWorld(),
						PointLoc,
						20.0f,         // Size
						FLinearColor::Yellow,
						false,
						2.0f,
						0
					);

					bAdvancedDrawingComplete = true;
				}
			}
			)AS"),
			TEXT("ADrawDebugAdvancedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DrawDebugAdvanced actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DrawDebugAdvanced actor should spawn")));

		// Verify advanced drawing completed
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAdvancedDrawingComplete"), true,
			TEXT("bAdvancedDrawingComplete should be true"));
	}

	// -------------------------------------------------------------------------
	// ObjectInspection: GetName, GetClass, GetFullName
	// -------------------------------------------------------------------------
	TEST_METHOD(ObjectInspection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_ObjectInspection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugObjectInspection.as"),
			ASTEST_AS(R"AS(
			// Actor testing object inspection for debugging
			UCLASS()
			class AObjectInspectionActor : AActor
			{
				UPROPERTY()
				FString ActorName = "";

				UPROPERTY()
				FString ClassName = "";

				UPROPERTY()
				FString FullName = "";

				UPROPERTY()
				bool bInspectionComplete = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// GetName - Object name
					ActorName = GetName();
					Print("Actor Name: " + ActorName);

					// GetClass().GetName() - Class name
					ClassName = GetClass().GetName();
					Print("Class Name: " + ClassName);

					// GetFullName - Full object path
					FullName = GetFullName();
					Print("Full Name: " + FullName);

					// Log hierarchical information
					Print("=== Object Inspection ===");
					Print("Name: " + GetName());
					Print("Class: " + GetClass().GetName());
					Print("Full Path: " + GetFullName());

					// Inspect components
					TArray<UActorComponent> Components;
					GetComponents(Components);
					Print("Component Count: " + Components.Num());

					for (int i = 0; i < Components.Num(); i++)
					{
						UActorComponent Comp = Components[i];
						if (Comp != nullptr)
						{
							Print("  Component[" + i + "]: " + Comp.GetName() + " (Class: " + Comp.GetClass().GetName() + ")");
						}
					}

					// Owner inspection
					AActor Owner = GetOwner();
					if (Owner != nullptr)
					{
						Print("Owner: " + Owner.GetName());
					}
					else
					{
						Print("Owner: None");
					}

					// World context
					UWorld World = GetWorld();
					if (World != nullptr)
					{
						Print("World: " + World.GetName());
					}

					bInspectionComplete = true;
				}
			}
			)AS"),
			TEXT("AObjectInspectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("ObjectInspection actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("ObjectInspection actor should spawn")));

		// Verify inspection completed
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInspectionComplete"), true,
			TEXT("bInspectionComplete should be true"));

		// Verify captured data
		FString ActorName;
		if (GetPropertyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ActorName"), ActorName))
		{
			ASSERT_THAT(IsTrue(!ActorName.IsEmpty(), TEXT("ActorName should not be empty")));
		}

		FString ClassName;
		if (GetPropertyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ClassName"), ClassName))
		{
			ASSERT_THAT(IsTrue(!ClassName.IsEmpty(), TEXT("ClassName should not be empty")));
		}

		FString FullName;
		if (GetPropertyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("FullName"), FullName))
		{
			ASSERT_THAT(IsTrue(!FullName.IsEmpty(), TEXT("FullName should not be empty")));
		}
	}

	// -------------------------------------------------------------------------
	// DrawDebugCombined: Combining multiple debug drawing techniques
	// -------------------------------------------------------------------------
	TEST_METHOD(DrawDebugCombined)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDebug_DrawDebugCombined"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDebugDrawDebugCombined.as"),
			ASTEST_AS(R"AS(
			// Actor demonstrating combined debug visualization
			UCLASS()
			class ADrawDebugCombinedActor : AActor
			{
				UPROPERTY()
				bool bVisualizationComplete = false;

				UFUNCTION()
				void DrawCoordinateAxes(FVector Origin, float AxisLength)
				{
					// X-axis (Red)
					System::DrawDebugLine(GetWorld(), Origin, Origin + FVector(AxisLength, 0, 0),
						FLinearColor::Red, false, 2.0f, 0, 3.0f);

					// Y-axis (Green)
					System::DrawDebugLine(GetWorld(), Origin, Origin + FVector(0, AxisLength, 0),
						FLinearColor::Green, false, 2.0f, 0, 3.0f);

					// Z-axis (Blue)
					System::DrawDebugLine(GetWorld(), Origin, Origin + FVector(0, 0, AxisLength),
						FLinearColor::Blue, false, 2.0f, 0, 3.0f);
				}

				UFUNCTION()
				void DrawBoundingVolume(FVector Center, FVector Extent)
				{
					// Draw box
					System::DrawDebugBox(GetWorld(), Center, Extent,
						FLinearColor::Yellow, false, 2.0f, 0, 2.0f);

					// Draw center sphere
					System::DrawDebugSphere(GetWorld(), Center, 10.0f, 8,
						FLinearColor::Red, false, 2.0f, 0, 1.0f);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FVector ActorLoc = GetActorLocation();

					// Draw coordinate system at actor location
					DrawCoordinateAxes(ActorLoc, 150.0f);

					// Draw bounding volume
					DrawBoundingVolume(ActorLoc, FVector(100, 100, 100));

					// Draw direction arrow
					FVector Forward = GetActorForwardVector();
					System::DrawDebugArrow(GetWorld(),
						ActorLoc,
						ActorLoc + Forward * 200.0f,
						40.0f,
						FLinearColor::White,
						false, 2.0f, 0, 2.0f);

					// Label the actor
					System::DrawDebugString(GetWorld(),
						ActorLoc + FVector(0, 0, 150),
						GetName(),
						nullptr,
						FLinearColor::White,
						2.0f,
						true);

					// Draw a path visualization
					FVector PathStart = ActorLoc;
					for (int i = 1; i <= 5; i++)
					{
						FVector PathEnd = PathStart + FVector(100.0f * i, 50.0f * i, 0.0f);
						System::DrawDebugSphere(GetWorld(), PathEnd, 20.0f, 8,
							FLinearColor::Green, false, 2.0f, 0, 1.0f);
						System::DrawDebugLine(GetWorld(), PathStart, PathEnd,
							FLinearColor::Green, false, 2.0f, 0, 2.0f);
						PathStart = PathEnd;
					}

					Print("=== Debug Visualization Complete ===");
					Print("Location: " + ActorLoc.ToString());
					Print("Forward: " + Forward.ToString());

					bVisualizationComplete = true;
				}
			}
			)AS"),
			TEXT("ADrawDebugCombinedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("DrawDebugCombined actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("DrawDebugCombined actor should spawn")));

		// Verify visualization completed
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bVisualizationComplete"), true,
			TEXT("bVisualizationComplete should be true"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
