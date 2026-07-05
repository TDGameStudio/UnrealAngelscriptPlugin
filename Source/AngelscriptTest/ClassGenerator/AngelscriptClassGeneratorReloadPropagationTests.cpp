#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptClassGeneratorReloadPropagationTests,
	"Angelscript.TestModule.ClassGenerator.ReloadPlanning.Propagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FReloadDecision
	{
		FAngelscriptClassGenerator::EReloadRequirement Requirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
	};

	static bool IsFullReloadDecision(const FReloadDecision& Decision)
	{
		return Decision.Requirement == FAngelscriptClassGenerator::FullReloadRequired
			|| Decision.Requirement == FAngelscriptClassGenerator::FullReloadSuggested
			|| Decision.bWantsFullReload
			|| Decision.bNeedsFullReload;
	}

	static bool IsHandledReloadResult(const ECompileResult Result)
	{
		return Result == ECompileResult::FullyHandled || Result == ECompileResult::PartiallyHandled;
	}

	static bool AnalyzeReload(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& InitialSource,
		const FString& ReloadSource,
		FReloadDecision& OutDecision,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, InitialSource),
				*FString::Printf(TEXT("%s should compile the initial module"), Context)))
		{
			return false;
		}

		OutDecision = FReloadDecision();
		return LocalAssert.IsTrue(
			AnalyzeReloadFromMemory(
				&Engine,
				ModuleName,
				Filename,
				ReloadSource,
				OutDecision.Requirement,
				OutDecision.bWantsFullReload,
				OutDecision.bNeedsFullReload),
			*FString::Printf(TEXT("%s should analyze the reload edit"), Context));
	}

	static bool CompileFullReload(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& Source,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		ECompileResult CompileResult = ECompileResult::Error;
		if (!LocalAssert.IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, Filename, Source, CompileResult),
				*FString::Printf(TEXT("%s should compile through full reload"), Context)))
		{
			return false;
		}

		return LocalAssert.IsTrue(IsHandledReloadResult(CompileResult), *FString::Printf(TEXT("%s should be handled by full reload"), Context));
	}

	static bool CompileSoftReload(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& Source,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		ECompileResult CompileResult = ECompileResult::Error;
		if (!LocalAssert.IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, Filename, Source, CompileResult),
				*FString::Printf(TEXT("%s should compile through soft reload"), Context)))
		{
			return false;
		}

		return LocalAssert.IsTrue(IsHandledReloadResult(CompileResult), *FString::Printf(TEXT("%s should be handled by soft reload"), Context));
	}

	static bool ExpectObjectPropertyClass(
		FAutomationTestBase& Test,
		UStruct* Owner,
		const FName PropertyName,
		UClass* ExpectedClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FObjectProperty* Property = Owner != nullptr ? FindFProperty<FObjectProperty>(Owner, PropertyName) : nullptr;
		if (!LocalAssert.IsNotNull(Property, *FString::Printf(TEXT("%s should expose object property %s"), Context, *PropertyName.ToString())))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedClass, Property->PropertyClass, Context);
	}

	static bool ExpectFunctionObjectType(
		FAutomationTestBase& Test,
		UFunction* Function,
		const FName PropertyName,
		UClass* ExpectedClass,
		const TCHAR* Context)
	{
		return ExpectObjectPropertyClass(Test, Function, PropertyName, ExpectedClass, Context);
	}

	static bool ExpectArrayInnerObjectClass(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		const FName PropertyName,
		UClass* ExpectedClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FArrayProperty* ArrayProperty = OwnerClass != nullptr ? FindFProperty<FArrayProperty>(OwnerClass, PropertyName) : nullptr;
		if (!LocalAssert.IsNotNull(ArrayProperty, *FString::Printf(TEXT("%s should expose array property %s"), Context, *PropertyName.ToString())))
		{
			return false;
		}

		FObjectProperty* InnerObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner);
		if (!LocalAssert.IsNotNull(InnerObjectProperty, Context))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedClass, InnerObjectProperty->PropertyClass, Context);
	}

	static bool ExpectDelegateParamObjectClass(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& DelegateName,
		const FName ParamName,
		UClass* ExpectedClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const TSharedPtr<FAngelscriptDelegateDesc> DelegateDesc = Engine.GetDelegate(DelegateName);
		if (!LocalAssert.IsTrue(DelegateDesc.IsValid(), Context)
			|| !LocalAssert.IsNotNull(DelegateDesc->Function, Context))
		{
			return false;
		}

		return ExpectObjectPropertyClass(Test, DelegateDesc->Function, ParamName, ExpectedClass, Context);
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

	TEST_METHOD(SingleHopPropertyDependencyRetargetsAfterProviderFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName ModuleName(TEXT("ASClassGeneratorReloadPropagationSingleHop"));
		const FString Filename(TEXT("ASClassGeneratorReloadPropagationSingleHop.as"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationProvider : UObject
			{
				UPROPERTY()
				int Value = 1;
			}

			UCLASS()
			class UClassGeneratorPropagationConsumer : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationProvider Provider;
			}
			)AS");

		const FString ReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationProvider : UObject
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			}

			UCLASS()
			class UClassGeneratorPropagationConsumer : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationProvider Provider;
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, ModuleName, Filename, InitialSource, ReloadSource, Decision, TEXT("Single-hop property dependency"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Provider layout change should request full reload for a property depender")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ModuleName, Filename, ReloadSource, TEXT("Single-hop property dependency"))));

		UClass* ProviderClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationProvider"));
		UClass* ConsumerClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationConsumer"));
		ASSERT_THAT(IsNotNull(ProviderClass, TEXT("Single-hop provider should exist after reload")));
		ASSERT_THAT(IsNotNull(ConsumerClass, TEXT("Single-hop consumer should exist after reload")));
		ASSERT_THAT(IsTrue(ExpectObjectPropertyClass(*TestRunner, ConsumerClass, TEXT("Provider"), ProviderClass, TEXT("Single-hop consumer property should target the reloaded provider"))));
	}

	TEST_METHOD(MultiHopPropertyDependencyRetargetsEntireChain)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName ModuleName(TEXT("ASClassGeneratorReloadPropagationMultiHop"));
		const FString Filename(TEXT("ASClassGeneratorReloadPropagationMultiHop.as"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationLeaf : UObject
			{
				UPROPERTY()
				int Value = 1;
			}

			UCLASS()
			class UClassGeneratorPropagationMiddle : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationLeaf Leaf;
			}

			UCLASS()
			class UClassGeneratorPropagationRoot : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationMiddle Middle;
			}
			)AS");

		const FString ReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationLeaf : UObject
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			}

			UCLASS()
			class UClassGeneratorPropagationMiddle : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationLeaf Leaf;
			}

			UCLASS()
			class UClassGeneratorPropagationRoot : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationMiddle Middle;
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, ModuleName, Filename, InitialSource, ReloadSource, Decision, TEXT("Multi-hop dependency"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Leaf layout change should request full reload through the dependency chain")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ModuleName, Filename, ReloadSource, TEXT("Multi-hop dependency"))));

		UClass* LeafClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationLeaf"));
		UClass* MiddleClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationMiddle"));
		UClass* RootClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationRoot"));
		ASSERT_THAT(IsNotNull(LeafClass, TEXT("Multi-hop leaf should exist after reload")));
		ASSERT_THAT(IsNotNull(MiddleClass, TEXT("Multi-hop middle should exist after reload")));
		ASSERT_THAT(IsNotNull(RootClass, TEXT("Multi-hop root should exist after reload")));
		ASSERT_THAT(IsTrue(ExpectObjectPropertyClass(*TestRunner, MiddleClass, TEXT("Leaf"), LeafClass, TEXT("Middle property should target the reloaded leaf"))));
		ASSERT_THAT(IsTrue(ExpectObjectPropertyClass(*TestRunner, RootClass, TEXT("Middle"), MiddleClass, TEXT("Root property should target the reloaded middle"))));
	}

	TEST_METHOD(CyclicPropertyDependencyTerminatesAndRetargetsBothSides)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName ModuleName(TEXT("ASClassGeneratorReloadPropagationCycle"));
		const FString Filename(TEXT("ASClassGeneratorReloadPropagationCycle.as"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationCycleA : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationCycleB Other;
			}

			UCLASS()
			class UClassGeneratorPropagationCycleB : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationCycleA Other;
			}
			)AS");

		const FString ReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationCycleA : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationCycleB Other;

				UPROPERTY()
				int AddedValue = 2;
			}

			UCLASS()
			class UClassGeneratorPropagationCycleB : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationCycleA Other;
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, ModuleName, Filename, InitialSource, ReloadSource, Decision, TEXT("Cyclic dependency"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Cycle layout change should request full reload without recursion failure")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ModuleName, Filename, ReloadSource, TEXT("Cyclic dependency"))));

		UClass* CycleAClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationCycleA"));
		UClass* CycleBClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationCycleB"));
		ASSERT_THAT(IsNotNull(CycleAClass, TEXT("Cycle A should exist after reload")));
		ASSERT_THAT(IsNotNull(CycleBClass, TEXT("Cycle B should exist after reload")));
		ASSERT_THAT(IsTrue(ExpectObjectPropertyClass(*TestRunner, CycleAClass, TEXT("Other"), CycleBClass, TEXT("Cycle A property should target current cycle B"))));
		ASSERT_THAT(IsTrue(ExpectObjectPropertyClass(*TestRunner, CycleBClass, TEXT("Other"), CycleAClass, TEXT("Cycle B property should target current cycle A"))));
	}

	TEST_METHOD(SuperClassDependencyRetargetsChildToReloadedBase)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName ModuleName(TEXT("ASClassGeneratorReloadPropagationSuper"));
		const FString Filename(TEXT("ASClassGeneratorReloadPropagationSuper.as"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationBase : UObject
			{
				UPROPERTY()
				int Value = 1;
			}

			UCLASS()
			class UClassGeneratorPropagationChild : UClassGeneratorPropagationBase
			{
				UFUNCTION()
				int ReadValue()
				{
					return Value;
				}
			}
			)AS");

		const FString ReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationBase : UObject
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			}

			UCLASS()
			class UClassGeneratorPropagationChild : UClassGeneratorPropagationBase
			{
				UFUNCTION()
				int ReadValue()
				{
					return Value + AddedValue;
				}
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, ModuleName, Filename, InitialSource, ReloadSource, Decision, TEXT("Super-class dependency"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Base layout change should request full reload for script child")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ModuleName, Filename, ReloadSource, TEXT("Super-class dependency"))));

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationBase"));
		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationChild"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("Base class should exist after reload")));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("Child class should exist after reload")));
		ASSERT_THAT(IsTrue(ChildClass != nullptr && ChildClass->IsChildOf(BaseClass), TEXT("Child class should inherit from the reloaded base class")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ChildClass, TEXT("AddedValue")), TEXT("Child should expose inherited property added by reloaded base")));
	}

	TEST_METHOD(MethodSignatureDependencyRetargetsParametersAndReturnValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName ModuleName(TEXT("ASClassGeneratorReloadPropagationSignature"));
		const FString Filename(TEXT("ASClassGeneratorReloadPropagationSignature.as"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationPayload : UObject
			{
				UPROPERTY()
				int Value = 1;
			}

			UCLASS()
			class UClassGeneratorPropagationSignatureUser : UObject
			{
				UFUNCTION()
				UClassGeneratorPropagationPayload Echo(UClassGeneratorPropagationPayload Payload)
				{
					return Payload;
				}
			}
			)AS");

		const FString ReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationPayload : UObject
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			}

			UCLASS()
			class UClassGeneratorPropagationSignatureUser : UObject
			{
				UFUNCTION()
				UClassGeneratorPropagationPayload Echo(UClassGeneratorPropagationPayload Payload)
				{
					return Payload;
				}
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, ModuleName, Filename, InitialSource, ReloadSource, Decision, TEXT("Method-signature dependency"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Payload layout change should request full reload for method signatures")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ModuleName, Filename, ReloadSource, TEXT("Method-signature dependency"))));

		UClass* PayloadClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationPayload"));
		UClass* UserClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationSignatureUser"));
		UFunction* EchoFunction = FindGeneratedFunction(UserClass, TEXT("Echo"));
		ASSERT_THAT(IsNotNull(PayloadClass, TEXT("Payload class should exist after reload")));
		ASSERT_THAT(IsNotNull(UserClass, TEXT("Signature user class should exist after reload")));
		ASSERT_THAT(IsNotNull(EchoFunction, TEXT("Echo function should exist after reload")));
		ASSERT_THAT(IsTrue(ExpectFunctionObjectType(*TestRunner, EchoFunction, TEXT("Payload"), PayloadClass, TEXT("Echo input should target the reloaded payload class"))));
		ASSERT_THAT(IsTrue(ExpectFunctionObjectType(*TestRunner, EchoFunction, TEXT("ReturnValue"), PayloadClass, TEXT("Echo return should target the reloaded payload class"))));
	}

	TEST_METHOD(ContainerSubtypeDependencyRetargetsArrayInnerType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName ModuleName(TEXT("ASClassGeneratorReloadPropagationContainer"));
		const FString Filename(TEXT("ASClassGeneratorReloadPropagationContainer.as"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationItem : UObject
			{
				UPROPERTY()
				int Value = 1;
			}

			UCLASS()
			class UClassGeneratorPropagationContainer : UObject
			{
				UPROPERTY()
				TArray<UClassGeneratorPropagationItem> Items;
			}
			)AS");

		const FString ReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationItem : UObject
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			}

			UCLASS()
			class UClassGeneratorPropagationContainer : UObject
			{
				UPROPERTY()
				TArray<UClassGeneratorPropagationItem> Items;
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, ModuleName, Filename, InitialSource, ReloadSource, Decision, TEXT("Container subtype dependency"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Item layout change should request full reload for container subtype users")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ModuleName, Filename, ReloadSource, TEXT("Container subtype dependency"))));

		UClass* ItemClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationItem"));
		UClass* ContainerClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationContainer"));
		ASSERT_THAT(IsNotNull(ItemClass, TEXT("Container item class should exist after reload")));
		ASSERT_THAT(IsNotNull(ContainerClass, TEXT("Container class should exist after reload")));
		ASSERT_THAT(IsTrue(ExpectArrayInnerObjectClass(*TestRunner, ContainerClass, TEXT("Items"), ItemClass, TEXT("Array inner type should target the reloaded item class"))));
	}

	TEST_METHOD(DelegateSignatureDependencyRetargetsParameterType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName ModuleName(TEXT("ASClassGeneratorReloadPropagationDelegate"));
		const FString Filename(TEXT("ASClassGeneratorReloadPropagationDelegate.as"));
		const FString DelegateName(TEXT("FClassGeneratorPropagationSignal"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationSignalPayload : UObject
			{
				UPROPERTY()
				int Value = 1;
			}

			delegate void FClassGeneratorPropagationSignal(UClassGeneratorPropagationSignalPayload Payload);

			UCLASS()
			class UClassGeneratorPropagationSignalOwner : UObject
			{
				UPROPERTY()
				FClassGeneratorPropagationSignal Signal;
			}
			)AS");

		const FString ReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationSignalPayload : UObject
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			}

			delegate void FClassGeneratorPropagationSignal(UClassGeneratorPropagationSignalPayload Payload);

			UCLASS()
			class UClassGeneratorPropagationSignalOwner : UObject
			{
				UPROPERTY()
				FClassGeneratorPropagationSignal Signal;
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, ModuleName, Filename, InitialSource, ReloadSource, Decision, TEXT("Delegate dependency"))));
		ASSERT_THAT(IsTrue(IsFullReloadDecision(Decision), TEXT("Delegate payload layout change should request full reload for delegate users")));
		ASSERT_THAT(IsTrue(CompileFullReload(*TestRunner, Engine, ModuleName, Filename, ReloadSource, TEXT("Delegate dependency"))));

		UClass* PayloadClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationSignalPayload"));
		UClass* OwnerClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationSignalOwner"));
		ASSERT_THAT(IsNotNull(PayloadClass, TEXT("Delegate payload class should exist after reload")));
		ASSERT_THAT(IsNotNull(OwnerClass, TEXT("Delegate owner class should exist after reload")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDelegateProperty>(OwnerClass, TEXT("Signal")), TEXT("Delegate owner should keep the delegate property")));
		ASSERT_THAT(IsTrue(ExpectDelegateParamObjectClass(*TestRunner, Engine, DelegateName, TEXT("Payload"), PayloadClass, TEXT("Delegate payload parameter should target the reloaded class"))));
	}

	TEST_METHOD(BodyOnlyProviderReloadDoesNotEscalateDependents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		const FName ModuleName(TEXT("ASClassGeneratorReloadPropagationSoftOnly"));
		const FString Filename(TEXT("ASClassGeneratorReloadPropagationSoftOnly.as"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationSoftProvider : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					return 1;
				}
			}

			UCLASS()
			class UClassGeneratorPropagationSoftConsumer : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationSoftProvider Provider;
			}
			)AS");

		const FString ReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorPropagationSoftProvider : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					return 2;
				}
			}

			UCLASS()
			class UClassGeneratorPropagationSoftConsumer : UObject
			{
				UPROPERTY()
				UClassGeneratorPropagationSoftProvider Provider;
			}
			)AS");

		FReloadDecision Decision;
		ASSERT_THAT(IsTrue(AnalyzeReload(*TestRunner, Engine, ModuleName, Filename, InitialSource, ReloadSource, Decision, TEXT("Body-only provider dependency"))));
		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::SoftReload, Decision.Requirement, TEXT("Body-only provider edit should stay on the soft-reload path")));
		ASSERT_THAT(IsFalse(Decision.bWantsFullReload, TEXT("Body-only provider edit should not want full reload through dependents")));
		ASSERT_THAT(IsFalse(Decision.bNeedsFullReload, TEXT("Body-only provider edit should not need full reload through dependents")));
		ASSERT_THAT(IsTrue(CompileSoftReload(*TestRunner, Engine, ModuleName, Filename, ReloadSource, TEXT("Body-only provider dependency"))));

		UClass* ProviderClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationSoftProvider"));
		UClass* ConsumerClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorPropagationSoftConsumer"));
		ASSERT_THAT(IsNotNull(ProviderClass, TEXT("Soft provider should exist after reload")));
		ASSERT_THAT(IsNotNull(ConsumerClass, TEXT("Soft consumer should exist after reload")));
		ASSERT_THAT(IsTrue(ExpectObjectPropertyClass(*TestRunner, ConsumerClass, TEXT("Provider"), ProviderClass, TEXT("Soft consumer property should still target the provider"))));
	}
};

#endif