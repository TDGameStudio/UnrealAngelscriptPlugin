#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "UObject/Package.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageDynamicDelegateTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript dynamic delegate usage, the third slice of the
// delegates-and-events matrix (OpenSpec: test-coverage/coverage-matrix.md
// section 3). Each TEST_METHOD walks one usage axis from the matrix:
//
// Axes covered here:
//   * DynamicDelegateDeclaration  - DECLARE_DYNAMIC_DELEGATE variants (Blueprint
//                                   compatible delegates).
//   * DynamicDelegateBinding      - BindUFunction/AddUFunction/Unbind for
//                                   script-facing dynamic delegate binding;
//                                   BindDynamic/AddDynamic/RemoveDynamic are
//                                   negative C++ macro-name boundaries.
//   * DynamicDelegateSerialization - Persistence support for dynamic delegates.
//   * DynamicDelegateBlueprint    - BlueprintAssignable, BlueprintCallable
//                                   property specifier boundaries.
//
// Pattern D (script execution) from the Angelscript test guide: compile AS
// actors, spawn them, drive dynamic delegate operations, verify results
// through properties.
//
// Dynamic delegates are Blueprint-compatible but slower than regular delegates.
// They support serialization and are required for Blueprint event integration.
//
// Detailed coverage matrix: OpenSpec: test-coverage/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageDynamicDelegateTest,
	"Angelscript.TestModule.Coverage.DynamicDelegate",
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
	// Basic dynamic single-cast delegate: BindUFunction, IsBound, Execute.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateBasics.as"),
			ASTEST_AS(R"AS(
			delegate void FCoverageDynamicSimpleDelegate();

			UCLASS()
			class ACoverageDynamicDelegateBasicsActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				bool DelegateWasCalled = false;

				// Dynamic delegates (single-cast)
				FCoverageDynamicSimpleDelegate OnDynamicEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test IsBound before binding
					if (!OnDynamicEvent.IsBound())
					{
						Counter = 1;
					}

					// Bind to UFUNCTION
					OnDynamicEvent.BindUFunction(this, n"HandleDynamicDelegate");

					// Test IsBound after binding
					if (OnDynamicEvent.IsBound())
					{
						Counter = 2;
					}

					// Execute delegate
					OnDynamicEvent.Execute();

					// Test Clear
					OnDynamicEvent.Clear();
					if (!OnDynamicEvent.IsBound())
					{
						Counter = 3;
					}
				}

				UFUNCTION()
				void HandleDynamicDelegate()
				{
					DelegateWasCalled = true;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicDelegateBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-delegate-basics actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-delegate-basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 3, TEXT("Counter should be 3 (IsBound checks passed)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DelegateWasCalled"), true, TEXT("Dynamic delegate should have been executed"))));
	}

	// -------------------------------------------------------------------------
	// Dynamic multicast delegate: AddUFunction, Unbind, Broadcast.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicMulticastDelegate)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Multicast"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateMulticast.as"),
			ASTEST_AS(R"AS(
			event void FCoverageDynamicMulticastEvent();

			UCLASS()
			class ACoverageDynamicMulticastActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString Result;

				// Dynamic multicast delegate
				FCoverageDynamicMulticastEvent OnMulticastEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add multiple listeners
					OnMulticastEvent.AddUFunction(this, n"Listener1");
					OnMulticastEvent.AddUFunction(this, n"Listener2");
					OnMulticastEvent.AddUFunction(this, n"Listener3");

					// Broadcast - all three should be called
					OnMulticastEvent.Broadcast();

					// Remove one listener
					OnMulticastEvent.Unbind(this, n"Listener2");

					// Broadcast again - only Listener1 and Listener3 should be called
					OnMulticastEvent.Broadcast();
				}

				UFUNCTION()
				void Listener1()
				{
					Counter += 1;
					Result += "A";
				}

				UFUNCTION()
				void Listener2()
				{
					Counter += 10;
					Result += "B";
				}

				UFUNCTION()
				void Listener3()
				{
					Counter += 100;
					Result += "C";
				}
			}
			)AS"),
			TEXT("ACoverageDynamicMulticastActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-multicast actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-multicast actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 100 (first broadcast) + 1 + 100 (second broadcast) = 212
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 212, TEXT("Dynamic multicast listeners should be called correctly"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result"), FString(TEXT("ABCAC")), TEXT("Listeners should be called in order, then without B"))));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate with parameters: passing parameters through dynamic
	// delegates.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Parameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateParameters.as"),
			ASTEST_AS(R"AS(
			delegate void FCoverageDynamicIntEvent(int Value);
			event void FCoverageDynamicIntStringEvent(int IntValue, FString StringValue);

			UCLASS()
			class ACoverageDynamicParamsActor : AActor
			{
				UPROPERTY()
				int ReceivedInt = 0;

				UPROPERTY()
				FString ReceivedString;

				FCoverageDynamicIntEvent OnIntEvent;
				FCoverageDynamicIntStringEvent OnIntStringEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Single parameter dynamic delegate
					OnIntEvent.BindUFunction(this, n"HandleIntEvent");
					OnIntEvent.Execute(42);

					// Two parameter dynamic multicast delegate
					OnIntStringEvent.AddUFunction(this, n"HandleIntStringEvent");
					OnIntStringEvent.Broadcast(100, "Test");
				}

				UFUNCTION()
				void HandleIntEvent(int Value)
				{
					ReceivedInt = Value;
				}

				UFUNCTION()
				void HandleIntStringEvent(int IntValue, FString StringValue)
				{
					ReceivedInt = IntValue;
					ReceivedString = StringValue;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicParamsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-parameters actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-parameters actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedInt"), 100, TEXT("Two-param dynamic delegate should set ReceivedInt to 100"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReceivedString"), FString(TEXT("Test")), TEXT("Two-param dynamic delegate should set ReceivedString"))));
	}

	// -------------------------------------------------------------------------
	// BlueprintAssignable / BlueprintCallable boundary: the current AS fork
	// rejects these event property specifiers during preprocessing.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateBlueprintAssignableAndCallableMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			event void FCoverageBlueprintNoParamEvent();
			event void FCoverageBlueprintValueEvent(int NewValue);

			UCLASS()
			class ACoverageBlueprintDelegateMetadataActor : AActor
			{
				UPROPERTY()
				int EventCount = 0;

				UPROPERTY(BlueprintAssignable)
				FCoverageBlueprintNoParamEvent OnCustomEvent;

				UPROPERTY(BlueprintCallable)
				FCoverageBlueprintValueEvent OnValueChanged;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bind handlers in script
					OnCustomEvent.AddUFunction(this, n"HandleCustomEvent");
					OnValueChanged.AddUFunction(this, n"HandleValueChanged");

					// Trigger events (Blueprint could also bind to these)
					OnCustomEvent.Broadcast();
					OnValueChanged.Broadcast(50);
				}

				UFUNCTION()
				void HandleCustomEvent()
				{
					EventCount++;
				}

				UFUNCTION()
				void HandleValueChanged(int NewValue)
				{
					EventCount += NewValue;
				}
			}
			)AS");

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Unknown property specifier BlueprintAssignable"));
		ExpectedDiagnostics.Add(TEXT("Unknown property specifier BlueprintCallable"));

		TestRunner->AddExpectedError(TEXT("Unknown property specifier BlueprintAssignable"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Unknown property specifier BlueprintCallable"), EAutomationExpectedErrorFlags::Contains, 1);

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDynamicDelegate_BlueprintAssignableCallable"),
			*ScriptSource,
			TEXT("BlueprintAssignable and BlueprintCallable event property specifiers should remain explicit preprocessing boundaries"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate serialization: FScriptDelegate and FMulticastScriptDelegate
	// properties serialize bound UObject/FName pairs.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateSerializationRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Serialization"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateSerialization.as"),
			ASTEST_AS(R"AS(
			delegate void FCoverageSerializedSingle();
			event void FCoverageSerializedEvent();

			UCLASS()
			class UCoverageDynamicDelegateSerializationObject : UObject
			{
				UPROPERTY()
				FCoverageSerializedSingle Single;

				UPROPERTY()
				FCoverageSerializedEvent Multi;

				UFUNCTION()
				void Handler()
				{
				}

				UFUNCTION()
				void BindDelegates()
				{
					Single.BindUFunction(this, n"Handler");
					Multi.AddUFunction(this, n"Handler");
				}
			}
			)AS"),
			TEXT("UCoverageDynamicDelegateSerializationObject"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic delegate serialization class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* SourceObject = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(SourceObject, TEXT("Dynamic delegate serialization source object should be created")));
		if (SourceObject == nullptr)
		{
			return;
		}

		UFunction* BindDelegatesFunction = FindGeneratedFunction(ScriptClass, TEXT("BindDelegates"));
		ASSERT_THAT(IsNotNull(BindDelegatesFunction, TEXT("BindDelegates function should be generated")));
		if (BindDelegatesFunction == nullptr)
		{
			return;
		}
		FFunctionInvoker BindDelegatesInvoker(*TestRunner, SourceObject, TEXT("BindDelegates"));
		ASSERT_THAT(IsTrue(BindDelegatesInvoker.Call(), TEXT("BindDelegates should execute through AS runtime dispatch")));

		const FDelegateProperty* SingleProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("Single"));
		const FMulticastDelegateProperty* MultiProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("Multi"));
		ASSERT_THAT(IsNotNull(SingleProperty, TEXT("Serialized single delegate property should exist")));
		ASSERT_THAT(IsNotNull(MultiProperty, TEXT("Serialized multicast delegate property should exist")));
		if (SingleProperty == nullptr || MultiProperty == nullptr)
		{
			return;
		}

		const FScriptDelegate* SourceSingle = SingleProperty->ContainerPtrToValuePtr<FScriptDelegate>(SourceObject);
		const FMulticastScriptDelegate* SourceMulti = MultiProperty->ContainerPtrToValuePtr<FMulticastScriptDelegate>(SourceObject);
		ASSERT_THAT(IsTrue(SourceSingle->IsBound(), TEXT("Source single delegate should be bound before serialization")));
		ASSERT_THAT(IsTrue(SourceMulti->IsBound(), TEXT("Source multicast delegate should be bound before serialization")));

		TArray<uint8> SerializedBytes;
		FMemoryWriter Writer(SerializedBytes);
		FObjectAndNameAsStringProxyArchive WriterProxy(Writer, false);
		FStructuredArchiveFromArchive StructuredWriter(WriterProxy);
		FStructuredArchiveRecord WriterRecord = StructuredWriter.GetSlot().EnterRecord();
		SingleProperty->SerializeItem(WriterRecord.EnterField(TEXT("Single")), const_cast<FScriptDelegate*>(SourceSingle), nullptr);
		MultiProperty->SerializeItem(WriterRecord.EnterField(TEXT("Multi")), const_cast<FMulticastScriptDelegate*>(SourceMulti), nullptr);
		ASSERT_THAT(IsTrue(SerializedBytes.Num() > 0, TEXT("Dynamic delegate properties should emit serialized bytes")));

		UObject* TargetObject = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(TargetObject, TEXT("Dynamic delegate serialization target object should be created")));
		if (TargetObject == nullptr)
		{
			return;
		}
		FScriptDelegate* TargetSingle = SingleProperty->ContainerPtrToValuePtr<FScriptDelegate>(TargetObject);
		FMulticastScriptDelegate* TargetMulti = MultiProperty->ContainerPtrToValuePtr<FMulticastScriptDelegate>(TargetObject);

		FMemoryReader Reader(SerializedBytes);
		FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, true);
		FStructuredArchiveFromArchive StructuredReader(ReaderProxy);
		FStructuredArchiveRecord ReaderRecord = StructuredReader.GetSlot().EnterRecord();
		SingleProperty->SerializeItem(ReaderRecord.EnterField(TEXT("Single")), TargetSingle, nullptr);
		MultiProperty->SerializeItem(ReaderRecord.EnterField(TEXT("Multi")), TargetMulti, nullptr);

		ASSERT_THAT(IsTrue(TargetSingle->IsBound(), TEXT("Single dynamic delegate binding should survive serialization")));
		ASSERT_THAT(AreEqual(SourceObject, TargetSingle->GetUObject(), TEXT("Single dynamic delegate should serialize the target object")));
		ASSERT_THAT(AreEqual(FName(TEXT("Handler")), TargetSingle->GetFunctionName(), TEXT("Single dynamic delegate should serialize the function name")));
		ASSERT_THAT(IsTrue(TargetMulti->IsBound(), TEXT("Multicast dynamic delegate binding should survive serialization")));
		ASSERT_THAT(AreEqual(1, TargetMulti->GetAllObjects().Num(), TEXT("Multicast dynamic delegate should serialize one bound target")));
	}

	// -------------------------------------------------------------------------
	// C++ macro naming boundary: AS uses BindUFunction/AddUFunction/Unbind.
	// BindDynamic/AddDynamic/RemoveDynamic are C++ macro helpers, not AS APIs.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicMacroNamesAreNotScriptAPIs)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString BindDynamicSource = ASTEST_AS(R"AS(
			delegate void FCoverageDynamicMacroSingle();

			UCLASS()
			class ACoverageBindDynamicMacroActor : AActor
			{
				UPROPERTY()
				FCoverageDynamicMacroSingle Single;

				UFUNCTION()
				void Handler()
				{
				}

				UFUNCTION()
				void TryBindDynamic()
				{
					Single.BindDynamic(this, n"Handler");
				}
			}
			)AS");

		TArray<FString> BindDynamicDiagnostics;
		BindDynamicDiagnostics.Add(TEXT("No matching signatures to 'FCoverageDynamicMacroSingle::BindDynamic"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDynamicDelegate_BindDynamicUnsupported"),
			*BindDynamicSource,
			TEXT("C++ BindDynamic macro name should remain outside the AS API surface"),
			MakeArrayView(BindDynamicDiagnostics))));

		const FString AddDynamicSource = ASTEST_AS(R"AS(
			event void FCoverageDynamicMacroEvent();

			UCLASS()
			class ACoverageAddDynamicMacroActor : AActor
			{
				UPROPERTY()
				FCoverageDynamicMacroEvent Multi;

				UFUNCTION()
				void Handler()
				{
				}

				UFUNCTION()
				void TryAddDynamic()
				{
					Multi.AddDynamic(this, n"Handler");
				}
			}
			)AS");

		TArray<FString> AddDynamicDiagnostics;
		AddDynamicDiagnostics.Add(TEXT("No matching signatures to 'FCoverageDynamicMacroEvent::AddDynamic"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDynamicDelegate_AddDynamicUnsupported"),
			*AddDynamicSource,
			TEXT("C++ AddDynamic macro name should remain outside the AS API surface"),
			MakeArrayView(AddDynamicDiagnostics))));

		const FString RemoveDynamicSource = ASTEST_AS(R"AS(
			event void FCoverageDynamicMacroEvent();

			UCLASS()
			class ACoverageRemoveDynamicMacroActor : AActor
			{
				UPROPERTY()
				FCoverageDynamicMacroEvent Multi;

				UFUNCTION()
				void Handler()
				{
				}

				UFUNCTION()
				void TryRemoveDynamic()
				{
					Multi.RemoveDynamic(this, n"Handler");
				}
			}
			)AS");

		TArray<FString> RemoveDynamicDiagnostics;
		RemoveDynamicDiagnostics.Add(TEXT("No matching signatures to 'FCoverageDynamicMacroEvent::RemoveDynamic"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDynamicDelegate_RemoveDynamicUnsupported"),
			*RemoveDynamicSource,
			TEXT("C++ RemoveDynamic macro name should remain outside the AS API surface"),
			MakeArrayView(RemoveDynamicDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate clear: Clear removes all bindings.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateClear)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Clear"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateClear.as"),
			ASTEST_AS(R"AS(
			event void FCoverageDynamicClearEvent();

			UCLASS()
			class ACoverageDynamicClearActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FCoverageDynamicClearEvent OnEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add multiple listeners
					OnEvent.AddUFunction(this, n"Handler1");
					OnEvent.AddUFunction(this, n"Handler2");
					OnEvent.AddUFunction(this, n"Handler3");

					// Broadcast - all should be called
					OnEvent.Broadcast();

					// Clear all listeners
					OnEvent.Clear();

					// Broadcast again - none should be called
					OnEvent.Broadcast();

					// Verify not bound
					if (!OnEvent.IsBound())
					{
						Counter += 1000;
					}
				}

				UFUNCTION()
				void Handler1()
				{
					Counter += 1;
				}

				UFUNCTION()
				void Handler2()
				{
					Counter += 10;
				}

				UFUNCTION()
				void Handler3()
				{
					Counter += 100;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicClearActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-clear actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-clear actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 100 (first broadcast) + 1000 (not bound) = 1111
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 1111, TEXT("Clear should remove all dynamic delegate bindings"))));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate return value: single-cast dynamic delegates can have
	// return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateReturnValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_ReturnValue"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateReturnValue.as"),
			ASTEST_AS(R"AS(
			delegate bool FCoverageDynamicBoolRetEvent();
			delegate int FCoverageDynamicIntRetIntEvent(int Value);

			UCLASS()
			class ACoverageDynamicRetValActor : AActor
			{
				UPROPERTY()
				bool BoolResult = false;

				UPROPERTY()
				int IntResult = 0;

				FCoverageDynamicBoolRetEvent OnBoolRetEvent;
				FCoverageDynamicIntRetIntEvent OnIntRetIntEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Return value only
					OnBoolRetEvent.BindUFunction(this, n"HandleBoolRetEvent");
					BoolResult = OnBoolRetEvent.Execute();

					// Return value + parameter
					OnIntRetIntEvent.BindUFunction(this, n"HandleIntRetIntEvent");
					IntResult = OnIntRetIntEvent.Execute(50);
				}

				UFUNCTION()
				bool HandleBoolRetEvent()
				{
					return true;
				}

				UFUNCTION()
				int HandleIntRetIntEvent(int Value)
				{
					return Value * 2;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicRetValActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-return-value actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-return-value actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolResult"), true, TEXT("Bool return dynamic delegate should return true"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntResult"), 100, TEXT("Int return dynamic delegate should return doubled value"))));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate with complex parameter types: FVector, FString.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateComplexParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_ComplexParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateComplexParameters.as"),
			ASTEST_AS(R"AS(
			delegate void FCoverageDynamicVectorEvent(FVector V);
			event void FCoverageDynamicVectorStringIntEvent(FVector V, FString S, int I);

			UCLASS()
			class ACoverageDynamicComplexParamsActor : AActor
			{
				UPROPERTY()
				FVector ReceivedVector;

				UPROPERTY()
				FString ReceivedString;

				UPROPERTY()
				int ReceivedInt = 0;

				FCoverageDynamicVectorEvent OnVectorEvent;
				FCoverageDynamicVectorStringIntEvent OnComplexEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// FVector parameter
					OnVectorEvent.BindUFunction(this, n"HandleVectorEvent");
					OnVectorEvent.Execute(FVector(1.0f, 2.0f, 3.0f));

					// Multiple complex parameters
					OnComplexEvent.AddUFunction(this, n"HandleComplexEvent");
					OnComplexEvent.Broadcast(FVector(10.0f, 20.0f, 30.0f), "Complex", 42);
				}

				UFUNCTION()
				void HandleVectorEvent(FVector V)
				{
					ReceivedVector = V;
				}

				UFUNCTION()
				void HandleComplexEvent(FVector V, FString S, int I)
				{
					ReceivedVector = V;
					ReceivedString = S;
					ReceivedInt = I;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicComplexParamsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-complex-parameters actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-complex-parameters actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FVector ExpectedVector(10.0f, 20.0f, 30.0f);
		ASSERT_THAT(IsTrue(VerifyStructByPath<FVector>(*TestRunner, Actor, TEXT("ReceivedVector"), ExpectedVector, TEXT("FVector parameter should pass through"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReceivedString"), FString(TEXT("Complex")), TEXT("FString parameter should pass through"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedInt"), 42, TEXT("Int parameter should pass through"))));
	}

	// -------------------------------------------------------------------------
	// Dynamic multicast delegate property reflection plus runtime execution with
	// an AS USTRUCT payload. This covers event properties and proves the struct
	// fields cross the event argument buffer.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateStructPayloadPropertyExecutes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_StructPayloadProperty"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateStructPayloadProperty.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FCoverageDynamicPayload
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				int Bonus = 0;
			}

			event void FCoverageDynamicPayloadEvent(FCoverageDynamicPayload Payload);

			UCLASS()
			class ACoverageDynamicStructPayloadActor : AActor
			{
				UPROPERTY()
				FCoverageDynamicPayloadEvent OnPayload;

				UPROPERTY()
				int ReceivedValue = 0;

				UPROPERTY()
				int ReceivedBonus = 0;

				UPROPERTY()
				int Result = 0;

				UPROPERTY()
				bool EventWasBound = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnPayload.AddUFunction(this, n"HandlePayload");
					EventWasBound = OnPayload.IsBound();

					FCoverageDynamicPayload Payload;
					Payload.Value = 19;
					Payload.Bonus = 23;
					OnPayload.Broadcast(Payload);
				}

				UFUNCTION()
				void HandlePayload(FCoverageDynamicPayload Payload)
				{
					ReceivedValue = Payload.Value;
					ReceivedBonus = Payload.Bonus;
					Result = Payload.Value + Payload.Bonus;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicStructPayloadActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic struct-payload actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FMulticastDelegateProperty* PayloadProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnPayload"));
		ASSERT_THAT(IsNotNull(PayloadProperty, TEXT("Struct event should generate FMulticastDelegateProperty")));
		if (PayloadProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(PayloadProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable), TEXT("Plain struct payload event should carry default Blueprint assignable/callable flags")));
		ASSERT_THAT(IsNotNull(PayloadProperty->SignatureFunction, TEXT("Struct payload event should keep its signature function")));
		if (PayloadProperty->SignatureFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(PayloadProperty->SignatureFunction, TEXT("Payload")), TEXT("Struct payload event signature should expose the AS USTRUCT parameter")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic struct-payload actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EventWasBound"), true, TEXT("Struct payload event should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedValue"), 19, TEXT("Struct payload event should pass Value field"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedBonus"), 23, TEXT("Struct payload event should pass Bonus field"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Result"), 42, TEXT("Struct payload event should execute the handler path"))));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate declaration runtime: AS-declared single-cast delegates
	// should bind and execute no-param, parameter, and return-value paths.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateDeclaredSingleCastRuntime)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_DeclaredRuntime"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateDeclaredRuntime.as"),
			ASTEST_AS(R"AS(
			delegate void FCoverageDeclaredNoParam();
			delegate void FCoverageDeclaredValue(int Value);
			delegate int FCoverageDeclaredRetVal(int Value);

			UCLASS()
			class ACoverageDynamicDeclaredRuntimeActor : AActor
			{
				UPROPERTY()
				FCoverageDeclaredNoParam OnNoParam;

				UPROPERTY()
				FCoverageDeclaredValue OnValue;

				UPROPERTY()
				FCoverageDeclaredRetVal OnRetVal;

				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				int ReceivedValue = 0;

				UPROPERTY()
				int ReturnResult = 0;

				UPROPERTY()
				bool bNoParamBound = false;

				UPROPERTY()
				bool bValueBound = false;

				UPROPERTY()
				bool bRetValBound = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnNoParam.BindUFunction(this, n"HandleNoParam");
					OnValue.BindUFunction(this, n"HandleValue");
					OnRetVal.BindUFunction(this, n"HandleRetVal");

					bNoParamBound = OnNoParam.IsBound();
					bValueBound = OnValue.IsBound();
					bRetValBound = OnRetVal.IsBound();

					OnNoParam.Execute();
					OnValue.Execute(17);
					ReturnResult = OnRetVal.Execute(25);
				}

				UFUNCTION()
				void HandleNoParam()
				{
					Counter += 1;
				}

				UFUNCTION()
				void HandleValue(int Value)
				{
					ReceivedValue = Value;
					Counter += Value;
				}

				UFUNCTION()
				int HandleRetVal(int Value)
				{
					return Value + 11;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicDeclaredRuntimeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic declared-runtime actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FDelegateProperty* NoParamProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OnNoParam"));
		const FDelegateProperty* ValueProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OnValue"));
		const FDelegateProperty* RetValProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OnRetVal"));
		ASSERT_THAT(IsNotNull(NoParamProperty, TEXT("No-param AS delegate UPROPERTY should generate FDelegateProperty")));
		ASSERT_THAT(IsNotNull(ValueProperty, TEXT("Parameterized AS delegate UPROPERTY should generate FDelegateProperty")));
		ASSERT_THAT(IsNotNull(RetValProperty, TEXT("Return-value AS delegate UPROPERTY should generate FDelegateProperty")));
		if (NoParamProperty == nullptr || ValueProperty == nullptr || RetValProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(ValueProperty->SignatureFunction, TEXT("Parameterized AS delegate should keep a signature function")));
		ASSERT_THAT(IsNotNull(RetValProperty->SignatureFunction, TEXT("Return-value AS delegate should keep a signature function")));
		if (ValueProperty->SignatureFunction == nullptr || RetValProperty->SignatureFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ValueProperty->SignatureFunction, TEXT("Value")), TEXT("Parameterized AS delegate should expose the named parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(RetValProperty->SignatureFunction, TEXT("Value")), TEXT("Return-value AS delegate should expose the input parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(RetValProperty->SignatureFunction, TEXT("ReturnValue")), TEXT("Return-value AS delegate should expose ReturnValue")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic declared-runtime actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNoParamBound"), true, TEXT("No-param AS delegate should bind"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bValueBound"), true, TEXT("Parameterized AS delegate should bind"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRetValBound"), true, TEXT("Return-value AS delegate should bind"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 18, TEXT("No-param and parameterized AS delegates should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedValue"), 17, TEXT("AS delegate parameter should reach its receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReturnResult"), 36, TEXT("AS delegate return value should round-trip"))));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate declaration metadata: single-cast delegates and multicast
	// events should materialize named signature parameters and return properties.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateDeclarationMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_DeclarationMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateDeclarationMetadata.as"),
			ASTEST_AS(R"AS(
			delegate void FCoverageDynamicNoParam();
			delegate void FCoverageDynamicValue(int Value);
			delegate bool FCoverageDynamicBoolResult();
			event void FCoverageDynamicEvent();
			event void FCoverageDynamicValueEvent(int NewValue);

			UCLASS()
			class UCoverageDynamicDelegateMetadataObject : UObject
			{
				UPROPERTY()
				FCoverageDynamicNoParam SingleNoParam;

				UPROPERTY()
				FCoverageDynamicValue SingleValue;

				UPROPERTY()
				FCoverageDynamicBoolResult SingleBoolResult;

				UPROPERTY()
				FCoverageDynamicEvent AssignableEvent;

				UPROPERTY()
				FCoverageDynamicValueEvent CallableValueEvent;
			}
			)AS"),
			TEXT("UCoverageDynamicDelegateMetadataObject"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic delegate metadata object class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> NoParamDelegate = Engine.GetDelegate(TEXT("FCoverageDynamicNoParam"));
		const TSharedPtr<FAngelscriptDelegateDesc> ValueDelegate = Engine.GetDelegate(TEXT("FCoverageDynamicValue"));
		const TSharedPtr<FAngelscriptDelegateDesc> BoolResultDelegate = Engine.GetDelegate(TEXT("FCoverageDynamicBoolResult"));
		const TSharedPtr<FAngelscriptDelegateDesc> EventDelegate = Engine.GetDelegate(TEXT("FCoverageDynamicEvent"));
		const TSharedPtr<FAngelscriptDelegateDesc> ValueEventDelegate = Engine.GetDelegate(TEXT("FCoverageDynamicValueEvent"));
		ASSERT_THAT(IsTrue(NoParamDelegate.IsValid(), TEXT("No-param dynamic delegate metadata should be registered")));
		ASSERT_THAT(IsTrue(ValueDelegate.IsValid(), TEXT("Value dynamic delegate metadata should be registered")));
		ASSERT_THAT(IsTrue(BoolResultDelegate.IsValid(), TEXT("Return-value dynamic delegate metadata should be registered")));
		ASSERT_THAT(IsTrue(EventDelegate.IsValid(), TEXT("No-param dynamic event metadata should be registered")));
		ASSERT_THAT(IsTrue(ValueEventDelegate.IsValid(), TEXT("Value dynamic event metadata should be registered")));
		if (!NoParamDelegate.IsValid() || !ValueDelegate.IsValid() || !BoolResultDelegate.IsValid() || !EventDelegate.IsValid() || !ValueEventDelegate.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsFalse(NoParamDelegate->bIsMulticast, TEXT("delegate declarations should be single-cast")));
		ASSERT_THAT(IsFalse(ValueDelegate->bIsMulticast, TEXT("parameterized delegate declarations should be single-cast")));
		ASSERT_THAT(IsFalse(BoolResultDelegate->bIsMulticast, TEXT("return-value delegate declarations should be single-cast")));
		ASSERT_THAT(IsTrue(EventDelegate->bIsMulticast, TEXT("event declarations should be multicast")));
		ASSERT_THAT(IsTrue(ValueEventDelegate->bIsMulticast, TEXT("parameterized event declarations should be multicast")));
		ASSERT_THAT(IsNotNull(ValueDelegate->Function, TEXT("Value dynamic delegate should generate a signature function")));
		ASSERT_THAT(IsNotNull(BoolResultDelegate->Function, TEXT("Return-value dynamic delegate should generate a signature function")));
		ASSERT_THAT(IsNotNull(ValueEventDelegate->Function, TEXT("Value dynamic event should generate a signature function")));
		if (ValueDelegate->Function == nullptr || BoolResultDelegate->Function == nullptr || ValueEventDelegate->Function == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ValueDelegate->Function, TEXT("Value")), TEXT("Dynamic delegate parameter should keep its declared name")));
		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(BoolResultDelegate->Function, TEXT("ReturnValue")), TEXT("Dynamic delegate return value should materialize as ReturnValue")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ValueEventDelegate->Function, TEXT("NewValue")), TEXT("Dynamic event parameter should keep its declared name")));

		const FDelegateProperty* SingleNoParamProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("SingleNoParam"));
		const FDelegateProperty* SingleValueProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("SingleValue"));
		const FDelegateProperty* SingleBoolResultProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("SingleBoolResult"));
		const FMulticastDelegateProperty* AssignableEventProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("AssignableEvent"));
		const FMulticastDelegateProperty* CallableValueEventProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("CallableValueEvent"));
		ASSERT_THAT(IsNotNull(SingleNoParamProperty, TEXT("Single-cast delegate UPROPERTY should generate FDelegateProperty")));
		ASSERT_THAT(IsNotNull(SingleValueProperty, TEXT("Parameterized delegate UPROPERTY should generate FDelegateProperty")));
		ASSERT_THAT(IsNotNull(SingleBoolResultProperty, TEXT("Return-value delegate UPROPERTY should generate FDelegateProperty")));
		ASSERT_THAT(IsNotNull(AssignableEventProperty, TEXT("Plain event UPROPERTY should generate FMulticastDelegateProperty")));
		ASSERT_THAT(IsNotNull(CallableValueEventProperty, TEXT("Plain value event UPROPERTY should generate FMulticastDelegateProperty")));
	}
};

#endif
