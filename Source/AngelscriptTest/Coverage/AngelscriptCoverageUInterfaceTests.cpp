#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageUInterfaceTests
// -----------------------------------------------------------------------------
// Current fork coverage for script-declared UINTERFACE/interface boundaries and
// supported native interface reference usage.
//
// The AS 2.33-based fork does not support script-level interface declarations or
// TScriptInterface<I> script types. These tests keep the coverage matrix honest
// by proving the unsupported forms fail at compile time with explicit diagnostics.
// Native UInterface references remain supported and are covered through script
// member, null, assignment, parameter, and call-dispatch paths below.
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUInterfaceTest,
	"Angelscript.TestModule.Coverage.UInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExpectUInterfaceBoundaryRejected(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName,
		const FString& Source,
		const TCHAR* Label,
		TArrayView<const FString> ExpectedDiagnostics)
	{
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			*Source,
			Label,
			ExpectedDiagnostics);
	}

	static bool EnsureInvokerValid(FAutomationTestBase& Test, FFunctionInvoker& Invoker, const TCHAR* Message)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsTrue(Invoker.IsValid(), Message);
	}

	static const FImplementedInterface* FindImplementedInterface(UClass* ScriptClass, UClass* InterfaceClass)
	{
		if (ScriptClass == nullptr || InterfaceClass == nullptr)
		{
			return nullptr;
		}

		return ScriptClass->Interfaces.FindByPredicate(
			[InterfaceClass](const FImplementedInterface& ImplementedInterface)
			{
				return ImplementedInterface.Class == InterfaceClass;
			});
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

	TEST_METHOD(ScriptInterfaceKeywordRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Virtual property syntax has been removed"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			interface ICoverageUnsupportedInterface
			{
				void Execute();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_InterfaceKeywordUnsupported"),
			ScriptSource,
			TEXT("script-level interface keyword should remain unsupported in this fork"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(UInterfaceMacroDeclarationRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected identifier"));
		ExpectedDiagnostics.Add(TEXT("Instead found '('"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UINTERFACE()
			interface ICoverageUnsupportedUInterface
			{
				void Execute();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_MacroDeclarationUnsupported"),
			ScriptSource,
			TEXT("UINTERFACE() script declarations should remain unsupported in this fork"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(UInterfaceSpecifierDeclarationRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected identifier"));
		ExpectedDiagnostics.Add(TEXT("Instead found '('"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UINTERFACE(BlueprintType)
			interface ICoverageUnsupportedBlueprintTypeInterface
			{
				UFUNCTION(BlueprintCallable)
				int GetValue();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_SpecifierUnsupported"),
			ScriptSource,
			TEXT("UINTERFACE specifiers should remain unsupported in script declarations"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(UInterfaceBlueprintableSpecifierRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected identifier"));
		ExpectedDiagnostics.Add(TEXT("Instead found '('"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UINTERFACE(Blueprintable)
			interface ICoverageUnsupportedBlueprintableInterface
			{
				void Execute();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_BlueprintableSpecifierUnsupported"),
			ScriptSource,
			TEXT("UINTERFACE(Blueprintable) should remain unsupported in script declarations"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(GeneratedBodyInsideInterfaceRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Virtual property syntax has been removed"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			interface ICoverageUnsupportedGeneratedBodyInterface
			{
				GENERATED_BODY()
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_GeneratedBodyUnsupported"),
			ScriptSource,
			TEXT("GENERATED_BODY() inside script interface declarations should remain unsupported"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(ScriptInterfaceMethodsRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Virtual property syntax has been removed"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			interface ICoverageUnsupportedMethodInterface
			{
				void PureMethod();

				void DefaultMethod()
				{
				}

				UFUNCTION()
				void ReflectedMethod();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_MethodDeclarationsUnsupported"),
			ScriptSource,
			TEXT("pure, default, and UFUNCTION script interface methods should remain unsupported"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(TScriptInterfaceTypeRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected method or property"));
		ExpectedDiagnostics.Add(TEXT("Instead found identifier 'TScriptInterface'"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUnsupportedTScriptInterfaceActor : AActor
			{
				UPROPERTY()
				TScriptInterface<ICoverageUnsupportedInterface> InterfaceRef;
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_TScriptInterfaceUnsupported"),
			ScriptSource,
			TEXT("TScriptInterface<I> script properties should remain unsupported without script interfaces"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(TScriptInterfaceArrayRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected method or property"));
		ExpectedDiagnostics.Add(TEXT("Instead found identifier 'TArray'"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUnsupportedTScriptInterfaceArrayActor : AActor
			{
				UPROPERTY()
				TArray<TScriptInterface<ICoverageUnsupportedInterface>> InterfaceRefs;
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_TScriptInterfaceArrayUnsupported"),
			ScriptSource,
			TEXT("TArray<TScriptInterface<I>> should remain an explicit unsupported container boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(NativeInterfaceReferenceMemberAndDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

		static const FName ModuleName(TEXT("ASCoverageUInterface_NativeReferencePropertyDispatch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceNativeReferencePropertyDispatch.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNativeInterfaceReferenceActor : AActor, UAngelscriptNativeParentInterface
			{
				UAngelscriptNativeParentInterface InterfaceRef;

				UPROPERTY()
				int NativeValue = 42;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UPROPERTY()
				bool DefaultNullWorked = false;

				UPROPERTY()
				bool AssignmentWorked = false;

				UPROPERTY()
				bool NullResetWorked = false;

				UPROPERTY()
				bool InterfaceCallWorked = false;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = Marker;
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UAngelscriptNativeParentInterface EmptyRef;
					DefaultNullWorked = EmptyRef == nullptr;

					UObject SelfObject = this;
					InterfaceRef = Cast<UAngelscriptNativeParentInterface>(SelfObject);
					AssignmentWorked = InterfaceRef != nullptr;

					if (InterfaceRef != nullptr)
					{
						InterfaceCallWorked = InterfaceRef.GetNativeValue() == 42;
						InterfaceRef.SetNativeMarker(n"FromInterfaceRef");
					}

					InterfaceRef = nullptr;
					NullResetWorked = InterfaceRef == nullptr;
				}
			}
			)AS"),
			TEXT("ACoverageNativeInterfaceReferenceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Native interface reference actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Script class should implement the native parent interface")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Native interface reference actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DefaultNullWorked"), true,
			TEXT("Native interface references should default to null"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AssignmentWorked"), true,
			TEXT("Native interface reference assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("InterfaceCallWorked"), true,
			TEXT("Native interface reference should dispatch interface methods"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullResetWorked"), true,
			TEXT("Native interface references should reset to null"))));

		FName NativeMarker = NAME_None;
		ASSERT_THAT(IsTrue(GetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker),
			TEXT("NativeMarker should be readable")));
		ASSERT_THAT(AreEqual(FName(TEXT("FromInterfaceRef")), NativeMarker,
			TEXT("Native interface setter should mutate actor state")));
	}

	TEST_METHOD(NativeInterfacePolymorphicReferencesAndParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeChildInterface::StaticClass());

		static const FName ModuleName(TEXT("ASCoverageUInterface_NativePolymorphicReferences"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceNativePolymorphicReferences.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNativeInterfaceBaseActor : AActor, UAngelscriptNativeParentInterface
			{
				UPROPERTY()
				int NativeValue = 11;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = Marker;
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta + NativeValue;
				}
			}

			UCLASS()
			class ACoverageNativeInterfaceChildActor : AActor, UAngelscriptNativeChildInterface
			{
				UPROPERTY()
				int NativeValue = 23;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UFUNCTION()
				int GetChildValue() const
				{
					return 223;
				}

				UFUNCTION()
				int GetNativeValue() const
				{
					return 46;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = Marker;
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta + NativeValue;
				}
			}

			UCLASS()
			class ACoverageNativeInterfacePolymorphicCollector : AActor
			{
				UPROPERTY()
				UObject FirstSource;

				UPROPERTY()
				UObject SecondSource;

				UAngelscriptNativeParentInterface CurrentRef;

				UPROPERTY()
				int PolymorphicSum = 0;

				UPROPERTY()
				int ParameterAdjustedValue = 0;

				UPROPERTY()
				int ChildInterfaceValue = 0;

				UPROPERTY()
				bool FirstAssigned = false;

				UPROPERTY()
				bool SecondAssigned = false;

				UPROPERTY()
				bool PolymorphicDispatchWorked = false;

				UPROPERTY()
				bool InterfaceParameterWorked = false;

				UPROPERTY()
				bool ChildCastWorked = false;

				void CaptureParent(UAngelscriptNativeParentInterface InInterface)
				{
					if (InInterface == nullptr)
						return;

					int Value = 5;
					InInterface.AdjustNativeValue(3, Value);
					ParameterAdjustedValue += Value;
					InterfaceParameterWorked = ParameterAdjustedValue == 50;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UAngelscriptNativeParentInterface FirstRef = Cast<UAngelscriptNativeParentInterface>(FirstSource);
					UAngelscriptNativeParentInterface SecondRef = Cast<UAngelscriptNativeParentInterface>(SecondSource);

					FirstAssigned = FirstRef != nullptr;
					SecondAssigned = SecondRef != nullptr;
					if (FirstRef == nullptr || SecondRef == nullptr)
						return;

					PolymorphicSum = FirstRef.GetNativeValue() + SecondRef.GetNativeValue();
					PolymorphicDispatchWorked = PolymorphicSum == 57;

					CurrentRef = SecondRef;
					CaptureParent(FirstRef);
					CaptureParent(CurrentRef);

					UAngelscriptNativeChildInterface ChildRef = Cast<UAngelscriptNativeChildInterface>(SecondSource);
					ChildCastWorked = ChildRef != nullptr;
					if (ChildRef != nullptr)
					{
						ChildInterfaceValue = ChildRef.GetChildValue();
					}
				}
			}
			)AS"),
			TEXT("ACoverageNativeInterfacePolymorphicCollector"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Native interface polymorphic collector class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("ACoverageNativeInterfaceBaseActor"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("Native interface base implementer class should compile")));
		if (BaseClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageNativeInterfaceChildActor"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("Native interface child implementer class should compile")));
		if (ChildClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(BaseClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Base script class should implement the native parent interface")));
		ASSERT_THAT(IsTrue(ChildClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Child script class should inherit the native parent interface")));
		ASSERT_THAT(IsTrue(ChildClass->ImplementsInterface(UAngelscriptNativeChildInterface::StaticClass()),
			TEXT("Child script class should implement the native child interface")));

		FObjectPropertyBase* FirstSourceProperty = FindFProperty<FObjectPropertyBase>(ScriptClass, TEXT("FirstSource"));
		ASSERT_THAT(IsNotNull(FirstSourceProperty, TEXT("FirstSource object property should exist")));
		if (FirstSourceProperty == nullptr)
		{
			return;
		}

		FObjectPropertyBase* SecondSourceProperty = FindFProperty<FObjectPropertyBase>(ScriptClass, TEXT("SecondSource"));
		ASSERT_THAT(IsNotNull(SecondSourceProperty, TEXT("SecondSource object property should exist")));
		if (SecondSourceProperty == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BaseActor = SpawnScriptActor(*TestRunner, Spawner, BaseClass);
		ASSERT_THAT(IsNotNull(BaseActor, TEXT("Native interface base actor should spawn")));
		if (BaseActor == nullptr)
		{
			return;
		}

		AActor* ChildActor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(ChildActor, TEXT("Native interface child actor should spawn")));
		if (ChildActor == nullptr)
		{
			return;
		}

		AActor* CollectorActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(CollectorActor, TEXT("Native interface polymorphic collector should spawn")));
		if (CollectorActor == nullptr)
		{
			return;
		}

		FirstSourceProperty->SetObjectPropertyValue_InContainer(CollectorActor, BaseActor);
		SecondSourceProperty->SetObjectPropertyValue_InContainer(CollectorActor, ChildActor);

		BeginPlayActor(Engine, *CollectorActor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, CollectorActor, TEXT("FirstAssigned"), true,
			TEXT("Native interface reference should assign from the first script implementer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, CollectorActor, TEXT("SecondAssigned"), true,
			TEXT("Native interface reference should assign from the second script implementer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, CollectorActor, TEXT("PolymorphicDispatchWorked"), true,
			TEXT("Native interface references should dispatch through different script implementer classes"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, CollectorActor, TEXT("InterfaceParameterWorked"), true,
			TEXT("Native interface parameters should preserve interface dispatch and ref mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, CollectorActor, TEXT("ChildCastWorked"), true,
			TEXT("Native child interface cast should succeed for the child implementer"))));

		int32 PolymorphicSum = 0;
		ASSERT_THAT(IsTrue(ReadIntPropertyChecked(*TestRunner, CollectorActor, TEXT("PolymorphicSum"), PolymorphicSum),
			TEXT("PolymorphicSum should be readable")));
		ASSERT_THAT(AreEqual(57, PolymorphicSum,
			TEXT("Native interface references should sum parent dispatch results from both implementers")));

		int32 ParameterAdjustedValue = 0;
		ASSERT_THAT(IsTrue(ReadIntPropertyChecked(*TestRunner, CollectorActor, TEXT("ParameterAdjustedValue"), ParameterAdjustedValue),
			TEXT("ParameterAdjustedValue should be readable")));
		ASSERT_THAT(AreEqual(50, ParameterAdjustedValue,
			TEXT("Native interface parameter calls should accumulate adjusted by-ref values")));

		int32 ChildInterfaceValue = 0;
		ASSERT_THAT(IsTrue(ReadIntPropertyChecked(*TestRunner, CollectorActor, TEXT("ChildInterfaceValue"), ChildInterfaceValue),
			TEXT("ChildInterfaceValue should be readable")));
		ASSERT_THAT(AreEqual(223, ChildInterfaceValue,
			TEXT("Native child interface method should dispatch on the child implementer")));
	}

	TEST_METHOD(NativeSingleInterfaceMetadataAndReflectedDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

		static const FName ModuleName(TEXT("ASCoverageUInterface_NativeSingleInterfaceMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceNativeSingleInterfaceMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNativeSingleInterfaceActor : AActor, UAngelscriptNativeParentInterface
			{
				UPROPERTY()
				int NativeValue = 64;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UPROPERTY()
				int AdjustedValue = 0;

				UPROPERTY()
				bool SelfCastWorked = false;

				UPROPERTY()
				bool SelfDispatchWorked = false;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = Marker;
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta + NativeValue;
					AdjustedValue = Value;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UObject SelfObject = this;
					UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(SelfObject);
					SelfCastWorked = ParentRef != nullptr;
					if (ParentRef == nullptr)
						return;

					int Value = 3;
					ParentRef.AdjustNativeValue(5, Value);
					ParentRef.SetNativeMarker(n"FromSingleInterfaceCast");
					SelfDispatchWorked = ParentRef.GetNativeValue() == 64 && Value == 72;
				}
			}
			)AS"),
			TEXT("ACoverageNativeSingleInterfaceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Native single-interface actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Script class should implement the native parent interface")));
		const FImplementedInterface* ParentInterface = FindImplementedInterface(ScriptClass, UAngelscriptNativeParentInterface::StaticClass());
		ASSERT_THAT(IsNotNull(ParentInterface,
			TEXT("Reflected interface metadata should include the native parent interface")));
		if (ParentInterface == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ParentInterface->bImplementedByK2,
			TEXT("Script-generated interface implementation should be marked as Blueprint/K2 implemented")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Native single-interface actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SelfCastWorked"), true,
			TEXT("Script-side Cast<UAngelscriptNativeParentInterface> should succeed for self"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SelfDispatchWorked"), true,
			TEXT("Script-side interface dispatch should call implemented methods on self"))));

		int32 AdjustedValue = 0;
		ASSERT_THAT(IsTrue(ReadIntPropertyChecked(*TestRunner, Actor, TEXT("AdjustedValue"), AdjustedValue),
			TEXT("AdjustedValue should be readable")));
		ASSERT_THAT(AreEqual(72, AdjustedValue,
			TEXT("Interface method implementation should mutate by-ref values through script dispatch")));

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("GetNativeValue"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("GetNativeValue should resolve through reflected UFUNCTION metadata")))
			{
				return;
			}
			ASSERT_THAT(AreEqual(64, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("Reflected invoker should execute the interface method implementation")));
		}

		FName NativeMarker = NAME_None;
		ASSERT_THAT(IsTrue(GetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker),
			TEXT("NativeMarker should be readable")));
		ASSERT_THAT(AreEqual(FName(TEXT("FromSingleInterfaceCast")), NativeMarker,
			TEXT("Interface setter dispatch should mutate the script actor")));
	}

	TEST_METHOD(NativeMultipleInterfaceMetadataAndIndependentDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeSecondaryInterface::StaticClass());

		static const FName ModuleName(TEXT("ASCoverageUInterface_NativeMultipleInterfaceMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUInterfaceNativeMultipleInterfaceMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNativeMultipleInterfaceActor : AActor, UAngelscriptNativeParentInterface, UAngelscriptNativeSecondaryInterface
			{
				UPROPERTY()
				int NativeValue = 31;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UPROPERTY()
				int SecondaryValue = 409;

				UPROPERTY()
				FString SecondaryLabel;

				UPROPERTY()
				int ParentResult = 0;

				UPROPERTY()
				int SecondaryResult = 0;

				UPROPERTY()
				bool ParentCastWorked = false;

				UPROPERTY()
				bool SecondaryCastWorked = false;

				UPROPERTY()
				bool IndependentDispatchWorked = false;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = Marker;
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta + NativeValue;
				}

				UFUNCTION()
				int GetSecondaryValue() const
				{
					return SecondaryValue;
				}

				UFUNCTION()
				void SetSecondaryLabel(const FString& NewLabel)
				{
					SecondaryLabel = NewLabel;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UObject SelfObject = this;
					UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(SelfObject);
					UAngelscriptNativeSecondaryInterface SecondaryRef = Cast<UAngelscriptNativeSecondaryInterface>(SelfObject);

					ParentCastWorked = ParentRef != nullptr;
					SecondaryCastWorked = SecondaryRef != nullptr;
					if (ParentRef == nullptr || SecondaryRef == nullptr)
						return;

					ParentResult = ParentRef.GetNativeValue();
					SecondaryResult = SecondaryRef.GetSecondaryValue();
					ParentRef.SetNativeMarker(n"FromParentInterface");
					SecondaryRef.SetSecondaryLabel("FromSecondaryInterface");
					IndependentDispatchWorked = ParentResult == 31 && SecondaryResult == 409;
				}
			}
			)AS"),
			TEXT("ACoverageNativeMultipleInterfaceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Native multiple-interface actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Script class should implement the native parent interface")));
		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeSecondaryInterface::StaticClass()),
			TEXT("Script class should implement the native secondary interface")));
		const FImplementedInterface* ParentInterface = FindImplementedInterface(ScriptClass, UAngelscriptNativeParentInterface::StaticClass());
		const FImplementedInterface* SecondaryInterface = FindImplementedInterface(ScriptClass, UAngelscriptNativeSecondaryInterface::StaticClass());

		ASSERT_THAT(IsNotNull(ParentInterface, TEXT("Reflected metadata should include the parent interface")));
		ASSERT_THAT(IsNotNull(SecondaryInterface, TEXT("Reflected metadata should include the secondary interface")));
		if (ParentInterface == nullptr || SecondaryInterface == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ParentInterface->bImplementedByK2,
			TEXT("Parent interface implementation should be marked as Blueprint/K2 implemented")));
		ASSERT_THAT(IsTrue(SecondaryInterface->bImplementedByK2,
			TEXT("Secondary interface implementation should be marked as Blueprint/K2 implemented")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Native multiple-interface actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ParentCastWorked"), true,
			TEXT("Script-side parent interface cast should succeed for self"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SecondaryCastWorked"), true,
			TEXT("Script-side secondary interface cast should succeed for self"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IndependentDispatchWorked"), true,
			TEXT("Multiple interface dispatch should call each interface implementation independently"))));

		int32 ParentResult = 0;
		ASSERT_THAT(IsTrue(ReadIntPropertyChecked(*TestRunner, Actor, TEXT("ParentResult"), ParentResult),
			TEXT("ParentResult should be readable")));
		ASSERT_THAT(AreEqual(31, ParentResult,
			TEXT("Parent interface method should return the parent backing value")));

		int32 SecondaryResult = 0;
		ASSERT_THAT(IsTrue(ReadIntPropertyChecked(*TestRunner, Actor, TEXT("SecondaryResult"), SecondaryResult),
			TEXT("SecondaryResult should be readable")));
		ASSERT_THAT(AreEqual(409, SecondaryResult,
			TEXT("Secondary interface method should return the secondary backing value")));

		FName NativeMarker = NAME_None;
		ASSERT_THAT(IsTrue(GetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker),
			TEXT("NativeMarker should be readable")));
		ASSERT_THAT(AreEqual(FName(TEXT("FromParentInterface")), NativeMarker,
			TEXT("Parent interface setter should mutate the parent marker")));

		FString SecondaryLabel;
		ASSERT_THAT(IsTrue(GetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("SecondaryLabel"), SecondaryLabel),
			TEXT("SecondaryLabel should be readable")));
		ASSERT_THAT(AreEqual(FString(TEXT("FromSecondaryInterface")), SecondaryLabel,
			TEXT("Secondary interface setter should mutate the secondary label")));

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("GetSecondaryValue"));
			if (!EnsureInvokerValid(*TestRunner, Invoker, TEXT("GetSecondaryValue should resolve through reflected UFUNCTION metadata")))
			{
				return;
			}
			ASSERT_THAT(AreEqual(409, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("Reflected invoker should execute the secondary interface method implementation")));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
