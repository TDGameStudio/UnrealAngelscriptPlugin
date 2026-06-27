#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageUInterfaceTests
// -----------------------------------------------------------------------------
// Comprehensive UINTERFACE coverage for AngelScript, following the matrix from
// Documents/Coverage/Coverage_OtherMacros.md section 4 (UINTERFACE).
//
// Test axes covered:
//   * UInterfaceBasicDeclaration    - interface, UINTERFACE() declaration
//   * UInterfaceSpecifiers          - BlueprintType, Blueprintable specifiers
//   * UInterfaceMethods             - Pure virtual, default implementation, UFUNCTION
//   * UInterfaceImplementation      - Single interface, multiple interfaces, inheritance
//   * UInterfaceCast                - Cast<IInterface> type conversion
//   * UInterfaceTScriptInterface    - TScriptInterface<I> references
//   * UInterfacePolymorphism        - Polymorphic method calls
//   * UInterfaceInContainers        - TArray<TScriptInterface<I>>
//
// Pattern: spawn AS actor, implement interfaces, validate through reflection
// and Cast operations.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUInterfaceTest,
	"Angelscript.TestModule.Coverage.UInterface",
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
	// Basic interface declarations: interface, UINTERFACE()
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceBasicDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_BasicDecl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceBasicDecl.as"),
			ASTEST_AS(R"AS(
			// UINTERFACE() - minimal declaration
			UINTERFACE()
			interface ISimpleInterface
			{
				void SimpleMethod();
			}

			// Plain interface without UINTERFACE (script-only)
			interface IScriptOnlyInterface
			{
				int GetScriptValue();
			}

			UCLASS()
			class ACoverageInterfaceBasicActor : AActor, ISimpleInterface, IScriptOnlyInterface
			{
				UPROPERTY()
				int SimpleValue = 0;

				UPROPERTY()
				int ScriptValue = 0;

				void SimpleMethod()
				{
					SimpleValue = 42;
				}

				int GetScriptValue()
				{
					return ScriptValue;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SimpleMethod();
					ScriptValue = 100;
				}
			}
			)AS"),
			TEXT("ACoverageInterfaceBasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UInterface basic declaration actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UInterface basic declaration actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify interface methods were called
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SimpleValue"), 42, TEXT("ISimpleInterface method should be callable"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ScriptValue"), 100, TEXT("Script-only interface method should be callable"));
	}

	// -------------------------------------------------------------------------
	// UINTERFACE specifiers: BlueprintType, Blueprintable
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceSpecifiers.as"),
			ASTEST_AS(R"AS(
			UINTERFACE(BlueprintType)
			interface IBlueprintTypeInterface
			{
				UFUNCTION(BlueprintCallable)
				int GetBlueprintValue();
			}

			UINTERFACE(Blueprintable)
			interface IBlueprintableInterface
			{
				void BlueprintableMethod();
			}

			UCLASS()
			class ACoverageInterfaceSpecifierActor : AActor, IBlueprintTypeInterface, IBlueprintableInterface
			{
				UPROPERTY()
				int BPValue = 0;

				UPROPERTY()
				int BPableValue = 0;

				int GetBlueprintValue()
				{
					return BPValue;
				}

				void BlueprintableMethod()
				{
					BPableValue = 200;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BPValue = 150;
					BlueprintableMethod();
				}
			}
			)AS"),
			TEXT("ACoverageInterfaceSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UInterface specifier actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UInterface specifier actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BPValue"), 150, TEXT("BlueprintType interface should work"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BPableValue"), 200, TEXT("Blueprintable interface should work"));
	}

	// -------------------------------------------------------------------------
	// Interface methods: pure virtual, default implementation, UFUNCTION
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_Methods"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceMethods.as"),
			ASTEST_AS(R"AS(
			UINTERFACE()
			interface IMethodInterface
			{
				// Pure virtual method (no implementation)
				void PureVirtualMethod();

				// Default implementation
				int DefaultMethod()
				{
					return 999;
				}

				// UFUNCTION method
				UFUNCTION(BlueprintCallable)
				FString UFunctionMethod();
			}

			UCLASS()
			class ACoverageInterfaceMethodActor : AActor, IMethodInterface
			{
				UPROPERTY()
				int PureValue = 0;

				UPROPERTY()
				int DefaultValue = 0;

				UPROPERTY()
				FString UFuncValue;

				// Implement pure virtual method
				void PureVirtualMethod()
				{
					PureValue = 300;
				}

				// Override default implementation
				int DefaultMethod()
				{
					return 777;
				}

				// Implement UFUNCTION method
				FString UFunctionMethod()
				{
					return "UFunctionResult";
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					PureVirtualMethod();
					DefaultValue = DefaultMethod();
					UFuncValue = UFunctionMethod();
				}
			}
			)AS"),
			TEXT("ACoverageInterfaceMethodActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UInterface methods actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UInterface methods actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PureValue"), 300, TEXT("Pure virtual method should be implemented"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DefaultValue"), 777, TEXT("Default method should be overridden"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("UFuncValue"), FString(TEXT("UFunctionResult")), TEXT("UFUNCTION method should work"));
	}

	// -------------------------------------------------------------------------
	// Interface implementation: single interface
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceSingleImplementation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_Single"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceSingle.as"),
			ASTEST_AS(R"AS(
			UINTERFACE()
			interface IFoo
			{
				int GetFoo();
			}

			// Single interface implementation
			UCLASS()
			class ACoverageSingleInterfaceActor : AActor, IFoo
			{
				UPROPERTY()
				int FooValue = 0;

				int GetFoo()
				{
					return 100;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FooValue = GetFoo();
				}
			}
			)AS"),
			TEXT("ACoverageSingleInterfaceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Single interface actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Single interface actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FooValue"), 100, TEXT("IFoo implementation"));
	}

	// -------------------------------------------------------------------------
	// Interface implementation: multiple interfaces
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceMultipleImplementation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_Multiple"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceMultiple.as"),
			ASTEST_AS(R"AS(
			UINTERFACE()
			interface IFoo
			{
				int GetFoo();
			}

			UINTERFACE()
			interface IBar
			{
				int GetBar();
			}

			UINTERFACE()
			interface IBaz
			{
				int GetBaz();
			}

			// Multiple interface implementation
			UCLASS()
			class ACoverageMultiInterfaceActor : AActor, IFoo, IBar, IBaz
			{
				UPROPERTY()
				int FooValue = 0;

				UPROPERTY()
				int BarValue = 0;

				UPROPERTY()
				int BazValue = 0;

				int GetFoo()
				{
					return 200;
				}

				int GetBar()
				{
					return 300;
				}

				int GetBaz()
				{
					return 400;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FooValue = GetFoo();
					BarValue = GetBar();
					BazValue = GetBaz();
				}
			}
			)AS"),
			TEXT("ACoverageMultiInterfaceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multiple interface actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multiple interface actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FooValue"), 200, TEXT("IFoo implementation"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BarValue"), 300, TEXT("IBar implementation"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BazValue"), 400, TEXT("IBaz implementation"));
	}

	// -------------------------------------------------------------------------
	// Interface implementation: inheritance
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceInheritedImplementation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_Inherited"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceInherited.as"),
			ASTEST_AS(R"AS(
			UINTERFACE()
			interface IFoo
			{
				int GetFoo();
			}

			// Parent class implements interface
			UCLASS()
			class ACoverageInterfaceParentActor : AActor, IFoo
			{
				int GetFoo()
				{
					return 500;
				}
			}

			// Child class inherits interface implementation
			UCLASS()
			class ACoverageInterfaceChildActor : ACoverageInterfaceParentActor
			{
				UPROPERTY()
				int InheritedFooValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Inherited interface method
					InheritedFooValue = GetFoo();
				}
			}
			)AS"),
			TEXT("ACoverageInterfaceChildActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Inherited interface actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Inherited interface actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InheritedFooValue"), 500, TEXT("Inherited interface method should work"));
	}

	// -------------------------------------------------------------------------
	// Interface Cast: Cast<IInterface> type conversion and checks
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceCast)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_Cast"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceCast.as"),
			ASTEST_AS(R"AS(
			UINTERFACE()
			interface ICastable
			{
				int GetCastValue();
			}

			UCLASS()
			class ACoverageCastableActor : AActor, ICastable
			{
				UPROPERTY()
				int CastValue = 0;

				int GetCastValue()
				{
					return 600;
				}
			}

			UCLASS()
			class ACoverageNonCastableActor : AActor
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageInterfaceCastActor : AActor
			{
				UPROPERTY()
				int CastSuccessValue = 0;

				UPROPERTY()
				bool HasInterface = false;

				UPROPERTY()
				bool DoesNotHaveInterface = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create actor that implements interface
					ACoverageCastableActor CastableActor = SpawnActor<ACoverageCastableActor>();

					// Cast to interface
					ICastable Intf = Cast<ICastable>(CastableActor);
					if (Intf != nullptr)
					{
						HasInterface = true;
						CastSuccessValue = Intf.GetCastValue();
					}

					// Create actor that does not implement interface
					ACoverageNonCastableActor NonCastable = SpawnActor<ACoverageNonCastableActor>();

					// Cast should fail
					ICastable NullIntf = Cast<ICastable>(NonCastable);
					DoesNotHaveInterface = (NullIntf == nullptr);

					// Cleanup
					if (CastableActor != nullptr)
						CastableActor.DestroyActor();
					if (NonCastable != nullptr)
						NonCastable.DestroyActor();
				}
			}
			)AS"),
			TEXT("ACoverageInterfaceCastActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UInterface cast actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UInterface cast actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HasInterface"), true, TEXT("Cast<IInterface> should succeed for implementing class"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CastSuccessValue"), 600, TEXT("Cast interface method call should work"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DoesNotHaveInterface"), true, TEXT("Cast<IInterface> should fail for non-implementing class"));
	}

	// -------------------------------------------------------------------------
	// TScriptInterface: interface references and UPROPERTY usage
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceTScriptInterface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_TScriptInterface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceTScriptInterface.as"),
			ASTEST_AS(R"AS(
			UINTERFACE()
			interface IStorable
			{
				int GetStoredValue();
				void SetStoredValue(int Value);
			}

			UCLASS()
			class ACoverageStorableActor : AActor, IStorable
			{
				UPROPERTY()
				int StoredValue = 0;

				int GetStoredValue()
				{
					return StoredValue;
				}

				void SetStoredValue(int Value)
				{
					StoredValue = Value;
				}
			}

			UCLASS()
			class ACoverageInterfaceRefActor : AActor
			{
				// TScriptInterface as UPROPERTY
				UPROPERTY()
				TScriptInterface<IStorable> StorableRef;

				UPROPERTY()
				int RetrievedValue = 0;

				UPROPERTY()
				bool RefIsValid = false;

				// Function parameter using TScriptInterface
				void ProcessStorable(TScriptInterface<IStorable> Storable)
				{
					if (Storable != nullptr)
					{
						Storable.SetStoredValue(888);
					}
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create actor that implements interface
					ACoverageStorableActor StorableActor = SpawnActor<ACoverageStorableActor>();

					// Store interface reference
					StorableRef = Cast<IStorable>(StorableActor);
					RefIsValid = (StorableRef != nullptr);

					// Call through interface reference
					if (StorableRef != nullptr)
					{
						StorableRef.SetStoredValue(700);
						RetrievedValue = StorableRef.GetStoredValue();
					}

					// Pass interface as function parameter
					ProcessStorable(StorableRef);

					// Value should be updated
					if (StorableRef != nullptr)
					{
						RetrievedValue = StorableRef.GetStoredValue();
					}

					// Cleanup
					if (StorableActor != nullptr)
						StorableActor.DestroyActor();
				}
			}
			)AS"),
			TEXT("ACoverageInterfaceRefActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("TScriptInterface actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("TScriptInterface actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RefIsValid"), true, TEXT("TScriptInterface reference should be valid"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RetrievedValue"), 888, TEXT("TScriptInterface method call should work"));
	}

	// -------------------------------------------------------------------------
	// Interface polymorphism: different classes implementing same interface
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfacePolymorphism)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_Polymorphism"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfacePolymorphism.as"),
			ASTEST_AS(R"AS(
			UINTERFACE()
			interface ICalculator
			{
				int Calculate();
			}

			UCLASS()
			class AAdderActor : AActor, ICalculator
			{
				int Calculate()
				{
					return 10 + 20;
				}
			}

			UCLASS()
			class AMultiplierActor : AActor, ICalculator
			{
				int Calculate()
				{
					return 10 * 20;
				}
			}

			UCLASS()
			class ASubtractorActor : AActor, ICalculator
			{
				int Calculate()
				{
					return 100 - 50;
				}
			}

			UCLASS()
			class ACoveragePolymorphismActor : AActor
			{
				UPROPERTY()
				int AdderResult = 0;

				UPROPERTY()
				int MultiplierResult = 0;

				UPROPERTY()
				int SubtractorResult = 0;

				UPROPERTY()
				int PolymorphicSum = 0;

				void ProcessCalculator(TScriptInterface<ICalculator> Calc)
				{
					if (Calc != nullptr)
					{
						PolymorphicSum += Calc.Calculate();
					}
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create different actors implementing same interface
					AAdderActor Adder = SpawnActor<AAdderActor>();
					AMultiplierActor Multiplier = SpawnActor<AMultiplierActor>();
					ASubtractorActor Subtractor = SpawnActor<ASubtractorActor>();

					// Direct polymorphic calls
					ICalculator AdderIntf = Cast<ICalculator>(Adder);
					if (AdderIntf != nullptr)
						AdderResult = AdderIntf.Calculate();

					ICalculator MultIntf = Cast<ICalculator>(Multiplier);
					if (MultIntf != nullptr)
						MultiplierResult = MultIntf.Calculate();

					ICalculator SubIntf = Cast<ICalculator>(Subtractor);
					if (SubIntf != nullptr)
						SubtractorResult = SubIntf.Calculate();

					// Polymorphic function calls
					PolymorphicSum = 0;
					ProcessCalculator(AdderIntf);
					ProcessCalculator(MultIntf);
					ProcessCalculator(SubIntf);

					// Cleanup
					if (Adder != nullptr) Adder.DestroyActor();
					if (Multiplier != nullptr) Multiplier.DestroyActor();
					if (Subtractor != nullptr) Subtractor.DestroyActor();
				}
			}
			)AS"),
			TEXT("ACoveragePolymorphismActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Polymorphism actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Polymorphism actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AdderResult"), 30, TEXT("Adder polymorphic call (10+20=30)"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MultiplierResult"), 200, TEXT("Multiplier polymorphic call (10*20=200)"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SubtractorResult"), 50, TEXT("Subtractor polymorphic call (100-50=50)"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PolymorphicSum"), 280, TEXT("Polymorphic sum (30+200+50=280)"));
	}

	// -------------------------------------------------------------------------
	// Interface in containers: TArray<TScriptInterface<I>>
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceInContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUInterface_Containers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceContainers.as"),
			ASTEST_AS(R"AS(
			UINTERFACE()
			interface IScorer
			{
				int GetScore();
			}

			UCLASS()
			class ALowScorer : AActor, IScorer
			{
				int GetScore()
				{
					return 10;
				}
			}

			UCLASS()
			class AMidScorer : AActor, IScorer
			{
				int GetScore()
				{
					return 50;
				}
			}

			UCLASS()
			class AHighScorer : AActor, IScorer
			{
				int GetScore()
				{
					return 100;
				}
			}

			UCLASS()
			class ACoverageInterfaceContainerActor : AActor
			{
				// Array of interface references
				UPROPERTY()
				TArray<TScriptInterface<IScorer>> Scorers;

				UPROPERTY()
				int TotalScore = 0;

				UPROPERTY()
				int ArraySize = 0;

				UPROPERTY()
				int FirstScore = 0;

				UPROPERTY()
				int LastScore = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Create actors implementing interface
					ALowScorer Low = SpawnActor<ALowScorer>();
					AMidScorer Mid = SpawnActor<AMidScorer>();
					AHighScorer High = SpawnActor<AHighScorer>();

					// Add to interface array
					Scorers.Add(Cast<IScorer>(Low));
					Scorers.Add(Cast<IScorer>(Mid));
					Scorers.Add(Cast<IScorer>(High));

					ArraySize = Scorers.Num();

					// Iterate and sum scores
					TotalScore = 0;
					for (int i = 0; i < Scorers.Num(); i++)
					{
						TScriptInterface<IScorer> Scorer = Scorers[i];
						if (Scorer != nullptr)
						{
							TotalScore += Scorer.GetScore();
						}
					}

					// Access by index
					if (Scorers.Num() > 0 && Scorers[0] != nullptr)
						FirstScore = Scorers[0].GetScore();

					if (Scorers.Num() > 2 && Scorers[2] != nullptr)
						LastScore = Scorers[2].GetScore();

					// Cleanup
					if (Low != nullptr) Low.DestroyActor();
					if (Mid != nullptr) Mid.DestroyActor();
					if (High != nullptr) High.DestroyActor();
				}
			}
			)AS"),
			TEXT("ACoverageInterfaceContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Interface container actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Interface container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArraySize"), 3, TEXT("Interface array should have 3 elements"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TotalScore"), 160, TEXT("Total score sum (10+50+100=160)"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FirstScore"), 10, TEXT("First score should be 10"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastScore"), 100, TEXT("Last score should be 100"));
	}
};

#endif
