#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMacrosTests
// -----------------------------------------------------------------------------
// Comprehensive coverage for advanced AngelScript macro usage scenarios.
// Based on OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md.
//
// Coverage matrix:
//   * UInterfaceMacroDeclarationRejected - UINTERFACE() unsupported boundary
//   * GeneratedBodyInsideInterfaceRejected - GENERATED_BODY() interface boundary
//   * UDelegateMacroDeclarationRejected - UDELEGATE() unsupported boundary
//   * ScriptDelegateReflectsUDelegateFunction - supported AS delegate reflection
//   * CustomMetadataKeysRoundTrip  - custom reflection metadata keys
//   * EditorConditionActiveBranchReflects - supported EDITOR conditional branch
//   * ReflectionMacroCombination   - combined reflection macro surface
//   * MacroExpansionIgnores...     - macro expansion boundary cases
//   * UEnumAdvancedDeclaration      - UENUM with BlueprintType, DisplayName, ToolTip
//   * UEnumCustomMetadataRoundTrip  - enum-level meta and entry UMETA custom keys
//   * UStructAdvancedUsage          - Complex USTRUCT with nested types, operators
//   * UParamModifiers               - UPARAM usage in function parameters
//   * BlueprintEvent                - AS-supported Blueprint event wrapper behavior
//
// Note: AngelScript uses the fork-specific BlueprintEvent / BlueprintOverride
// specifiers for script-authored Blueprint events. The C++ implementable/native
// event spellings are not AS UFUNCTION specifiers in this fork.
//
// Pattern: spawn AS actor, test advanced macro scenarios, validate through
// reflection and runtime behavior verification.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMacrosTest,
	"Angelscript.TestModule.Coverage.Macros",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static UFunction* RequireGeneratedFunction(UClass* ScriptClass, const TCHAR* FunctionName)
	{
		return ScriptClass != nullptr ? FindGeneratedFunction(ScriptClass, FName(FunctionName)) : nullptr;
	}

	static TArray<FProperty*> GetOrderedParameters(UFunction* Function)
	{
		TArray<FProperty*> Parameters;
		if (Function == nullptr)
		{
			return Parameters;
		}

		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			FProperty* Property = *It;
			if (Property != nullptr
				&& Property->HasAnyPropertyFlags(CPF_Parm)
				&& !Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Parameters.Add(Property);
			}
		}

		Parameters.Sort([](const FProperty& Left, const FProperty& Right)
		{
			return Left.GetOffset_ForUFunction() < Right.GetOffset_ForUFunction();
		});

		return Parameters;
	}

	static FProperty* GetSingleParameter(UFunction* Function)
	{
		const TArray<FProperty*> Parameters = GetOrderedParameters(Function);
		return Parameters.Num() == 1 ? Parameters[0] : nullptr;
	}

	static FProperty* FindScriptProperty(UClass* ScriptClass, const TCHAR* PropertyName)
	{
		return ScriptClass != nullptr ? ScriptClass->FindPropertyByName(FName(PropertyName)) : nullptr;
	}

	static FString GetClassMetadata(UClass* ScriptClass, const TCHAR* MetadataKey)
	{
		return ScriptClass != nullptr ? ScriptClass->GetMetaData(MetadataKey) : FString();
	}

	static FString GetParameterDisplayName(const FProperty* Parameter)
	{
		return Parameter != nullptr ? Parameter->GetMetaData(TEXT("DisplayName")) : FString();
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

	// -------------------------------------------------------------------------
	// UINTERFACE declaration boundary: script-level UINTERFACE() is intentionally
	// unsupported in this fork, so the macro spelling should fail explicitly.
	// -------------------------------------------------------------------------
	TEST_METHOD(UInterfaceMacroDeclarationRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UINTERFACE()
			interface ICoverageMacrosUnsupportedUInterface
			{
				void Execute();
			}
			)AS");

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected identifier"));
		ExpectedDiagnostics.Add(TEXT("Instead found '('"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMacros_UInterfaceMacroUnsupported"),
			ScriptSource,
			TEXT("UINTERFACE() script declarations should remain unsupported in this fork"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// GENERATED_BODY boundary: this C++ macro should not be accepted inside a
	// script interface declaration.
	// -------------------------------------------------------------------------
	TEST_METHOD(GeneratedBodyInsideInterfaceRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			interface ICoverageMacrosUnsupportedGeneratedBodyInterface
			{
				GENERATED_BODY()
			}
			)AS");

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected method or property"));
		ExpectedDiagnostics.Add(TEXT("Instead found identifier 'GENERATED_BODY'"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMacros_GeneratedBodyUnsupported"),
			ScriptSource,
			TEXT("GENERATED_BODY() inside script interface declarations should remain unsupported"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Script interface boundaries: the coverage matrix still lists interface
	// declaration forms and specifiers, but this fork rejects script interfaces.
	// -------------------------------------------------------------------------
	TEST_METHOD(ScriptInterfaceDeclarationBoundariesRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptInterfaceSource = ASTEST_AS(R"AS(
			interface ICoverageMacrosScriptInterface
			{
				void Execute();
			}
			)AS");

		TArray<FString> ScriptInterfaceDiagnostics;
		ScriptInterfaceDiagnostics.Add(TEXT("Virtual property syntax has been removed"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMacros_ScriptInterfaceUnsupported"),
			ScriptInterfaceSource,
			TEXT("script interface declarations should remain unsupported in this fork"),
			MakeArrayView(ScriptInterfaceDiagnostics))));

		const FString BlueprintTypeInterfaceSource = ASTEST_AS(R"AS(
			UINTERFACE(BlueprintType)
			interface ICoverageMacrosBlueprintTypeInterface
			{
				void Execute();
			}
			)AS");

		TArray<FString> BlueprintTypeDiagnostics;
		BlueprintTypeDiagnostics.Add(TEXT("Expected identifier"));
		BlueprintTypeDiagnostics.Add(TEXT("Instead found '('"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMacros_BlueprintTypeInterfaceUnsupported"),
			BlueprintTypeInterfaceSource,
			TEXT("UINTERFACE(BlueprintType) declarations should remain unsupported in script"),
			MakeArrayView(BlueprintTypeDiagnostics))));

		const FString BlueprintableInterfaceSource = ASTEST_AS(R"AS(
			UINTERFACE(Blueprintable)
			interface ICoverageMacrosBlueprintableInterface
			{
				void Execute();
			}
			)AS");

		TArray<FString> BlueprintableDiagnostics;
		BlueprintableDiagnostics.Add(TEXT("Expected identifier"));
		BlueprintableDiagnostics.Add(TEXT("Instead found '('"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMacros_BlueprintableInterfaceUnsupported"),
			BlueprintableInterfaceSource,
			TEXT("UINTERFACE(Blueprintable) declarations should remain unsupported in script"),
			MakeArrayView(BlueprintableDiagnostics))));

		const FString InterfaceUFunctionSource = ASTEST_AS(R"AS(
			interface ICoverageMacrosInterfaceFunction
			{
				UFUNCTION(BlueprintCallable)
				void Execute();
			}
			)AS");

		TArray<FString> InterfaceUFunctionDiagnostics;
		InterfaceUFunctionDiagnostics.Add(TEXT("Expected method or property"));
		InterfaceUFunctionDiagnostics.Add(TEXT("Instead found identifier 'UFUNCTION'"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMacros_InterfaceUFunctionUnsupported"),
			InterfaceUFunctionSource,
			TEXT("UFUNCTION methods inside script interfaces should remain unsupported"),
			MakeArrayView(InterfaceUFunctionDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// UDELEGATE declaration boundary: AS supports delegate declarations, but not
	// the C++ UDELEGATE() macro spelling on script delegates.
	// -------------------------------------------------------------------------
	TEST_METHOD(UDelegateMacroDeclarationRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UDELEGATE()
			delegate void FCoverageUnsupportedUDelegate(int Value);
			)AS");

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected identifier"));
		ExpectedDiagnostics.Add(TEXT("Instead found '('"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMacros_UDelegateMacroUnsupported"),
			ScriptSource,
			TEXT("UDELEGATE() macro spelling should remain unsupported for AS delegate declarations"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Native AS delegate declaration: this is the supported replacement for the
	// rejected UDELEGATE() macro and validates UDelegateFunction reflection.
	// -------------------------------------------------------------------------
	TEST_METHOD(ScriptDelegateReflectsUDelegateFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_ScriptDelegateReflection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosScriptDelegateReflection.as"),
			ASTEST_AS(R"AS(
			delegate void FCoverageMacroSignal(int Value);

			UCLASS()
			class ACoverageMacrosDelegateActor : AActor
			{
				UPROPERTY()
				FCoverageMacroSignal Signal;
			}
			)AS"),
			TEXT("ACoverageMacrosDelegateActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Script delegate carrier should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> DelegateDesc = Engine.GetDelegate(TEXT("FCoverageMacroSignal"));
		ASSERT_THAT(IsTrue(DelegateDesc.IsValid(), TEXT("Script delegate should register an engine delegate descriptor")));
		ASSERT_THAT(IsTrue(DelegateDesc.IsValid() && !DelegateDesc->bIsMulticast,
			TEXT("delegate keyword should create a single-cast delegate")));
		UFunction* DelegateFunction = DelegateDesc.IsValid() ? DelegateDesc->Function : nullptr;
		ASSERT_THAT(IsNotNull(DelegateFunction, TEXT("Script delegate should materialize a UDelegateFunction")));
		if (DelegateFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(DelegateFunction != nullptr && DelegateFunction->IsA<UDelegateFunction>(),
			TEXT("Generated delegate signature should be a UDelegateFunction")));

		FProperty* DelegateParam = GetSingleParameter(DelegateFunction);
		ASSERT_THAT(IsNotNull(DelegateParam, TEXT("Delegate signature should expose its Value parameter")));
		if (DelegateParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FName(TEXT("Value")), DelegateParam->GetFName(),
			TEXT("Delegate signature parameter name should round-trip")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(DelegateParam),
			TEXT("Delegate signature Value parameter should be reflected as an int")));

		FDelegateProperty* SignalProperty = CastField<FDelegateProperty>(FindScriptProperty(ScriptClass, TEXT("Signal")));
		ASSERT_THAT(IsNotNull(SignalProperty, TEXT("Delegate UPROPERTY should reflect as FDelegateProperty")));
		if (SignalProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(DelegateFunction, SignalProperty->SignatureFunction.Get(),
			TEXT("Delegate property should target the generated UDelegateFunction")));
	}

	// -------------------------------------------------------------------------
	// Custom metadata keys: UCLASS / UPROPERTY / UFUNCTION meta should round-trip
	// through the reflected UE metadata maps.
	// -------------------------------------------------------------------------
	TEST_METHOD(CustomMetadataKeysRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_CustomMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosCustomMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS(meta=(CoverageClassKey="ClassValue", DisplayName="Coverage Metadata Actor"))
			class ACoverageMacrosMetadataActor : AActor
			{
				UPROPERTY(meta=(CoveragePropertyKey="PropertyValue", ClampMin="1"))
				int ReflectedValue = 7;

				UFUNCTION(BlueprintCallable, meta=(CoverageFunctionKey="FunctionValue", Keywords="coverage metadata"))
				int ReadValue() const
				{
					return ReflectedValue;
				}
			}
			)AS"),
			TEXT("ACoverageMacrosMetadataActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Custom metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("ClassValue")), GetClassMetadata(ScriptClass, TEXT("CoverageClassKey")),
			TEXT("UCLASS custom metadata key should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage Metadata Actor")), GetClassMetadata(ScriptClass, TEXT("DisplayName")),
			TEXT("UCLASS DisplayName metadata should round-trip with custom keys")));

		FProperty* ReflectedValueProperty = FindScriptProperty(ScriptClass, TEXT("ReflectedValue"));
		ASSERT_THAT(IsNotNull(ReflectedValueProperty, TEXT("ReflectedValue property should exist")));
		if (ReflectedValueProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("PropertyValue")),
			ReflectedValueProperty->GetMetaData(TEXT("CoveragePropertyKey")),
			TEXT("UPROPERTY custom metadata key should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("1")),
			ReflectedValueProperty->GetMetaData(TEXT("ClampMin")),
			TEXT("UPROPERTY standard metadata should remain available beside custom keys")));

		UFunction* ReadValueFunction = RequireGeneratedFunction(ScriptClass, TEXT("ReadValue"));
		ASSERT_THAT(IsNotNull(ReadValueFunction, TEXT("ReadValue UFUNCTION should exist")));
		if (ReadValueFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("FunctionValue")),
			ReadValueFunction->GetMetaData(TEXT("CoverageFunctionKey")),
			TEXT("UFUNCTION custom metadata key should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("coverage metadata")),
			ReadValueFunction->GetMetaData(TEXT("Keywords")),
			TEXT("UFUNCTION standard metadata should remain available beside custom keys")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Custom metadata actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker ReadValueInvoker(*TestRunner, Actor, TEXT("ReadValue"));
		ASSERT_THAT(IsTrue(ReadValueInvoker.IsValid(), TEXT("ReadValue should be invokable through reflection")));
		if (!ReadValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(7, ReadValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Custom metadata should not affect reflected function execution")));
	}

	// -------------------------------------------------------------------------
	// WITH_EDITOR boundary: the AS preprocessor uses EDITOR / EDITORONLY_DATA
	// flags; WITH_EDITOR is a C++ macro and is intentionally rejected.
	// -------------------------------------------------------------------------
	TEST_METHOD(WithEditorMacroNameRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			#if WITH_EDITOR
			int EditorOnlyValue()
			{
				return 1;
			}
			#endif
			)AS");

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Invalid preprocessor condition: WITH_EDITOR"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMacros_WithEditorUnsupported"),
			ScriptSource,
			TEXT("WITH_EDITOR should remain a C++ macro name, not an AS preprocessor flag"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// EDITOR condition branch: validate the supported AS editor condition form
	// and prove the active branch reflects and runs.
	// -------------------------------------------------------------------------
	TEST_METHOD(EditorConditionActiveBranchReflects)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, true);

		static const FName ModuleName(TEXT("ASCoverageMacros_EditorConditionBranch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosEditorConditionBranch.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMacrosEditorConditionActor : AActor
			{
				#if EDITOR
				UPROPERTY()
				int ActiveBranchValue = 57;

				UFUNCTION()
				int GetBranchValue() const
				{
					return ActiveBranchValue;
				}
				#else
				UPROPERTY()
				int InactiveBranchValue = -1;

				UFUNCTION()
				int GetBranchValue() const
				{
					return InactiveBranchValue;
				}
				#endif
			}
			)AS"),
			TEXT("ACoverageMacrosEditorConditionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("EDITOR condition actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FProperty* ActiveBranchProperty = FindScriptProperty(ScriptClass, TEXT("ActiveBranchValue"));
		ASSERT_THAT(IsNotNull(ActiveBranchProperty, TEXT("EDITOR active branch property should be reflected")));
		FProperty* InactiveBranchProperty = FindScriptProperty(ScriptClass, TEXT("InactiveBranchValue"));
		ASSERT_THAT(IsNull(InactiveBranchProperty, TEXT("EDITOR inactive branch property should not be reflected")));

		UFunction* GetBranchValueFunction = RequireGeneratedFunction(ScriptClass, TEXT("GetBranchValue"));
		ASSERT_THAT(IsNotNull(GetBranchValueFunction, TEXT("EDITOR active branch function should be reflected")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("EDITOR condition actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker GetBranchValueInvoker(*TestRunner, Actor, TEXT("GetBranchValue"));
		ASSERT_THAT(IsTrue(GetBranchValueInvoker.IsValid(), TEXT("GetBranchValue should be invokable through reflection")));
		if (!GetBranchValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(57, GetBranchValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("EDITOR active branch function should execute the reflected active branch")));
	}

	// -------------------------------------------------------------------------
	// Reflection macro combination: one script combines UENUM, delegate, USTRUCT,
	// UCLASS, UPROPERTY, and UFUNCTION and validates runtime/reflection together.
	// -------------------------------------------------------------------------
	TEST_METHOD(ReflectionMacroCombination)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_ReflectionCombination"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosReflectionCombination.as"),
			ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum ECoverageMacroCombinedState
			{
				Ready UMETA(DisplayName="Ready State"),
				Blocked UMETA(Hidden)
			}

			delegate void FCoverageMacroCombinedSignal(int Value);

			USTRUCT(BlueprintType)
			struct FCoverageMacroCombinedPayload
			{
				UPROPERTY()
				int Amount = 0;
			}

			UCLASS(BlueprintType, meta=(CoverageCombinedKey="CombinedValue"))
			class ACoverageMacrosCombinedActor : AActor
			{
				UPROPERTY(meta=(CoveragePropertyKey="CombinedProperty"))
				FCoverageMacroCombinedPayload Payload;

				UPROPERTY()
				ECoverageMacroCombinedState State = ECoverageMacroCombinedState::Ready;

				UPROPERTY()
				FCoverageMacroCombinedSignal Signal;

				UPROPERTY()
				int RuntimeResult = 0;

				UFUNCTION(BlueprintCallable, meta=(CoverageFunctionKey="CombinedFunction"))
				int ApplyPayload(int Bonus)
				{
					RuntimeResult = Payload.Amount + Bonus;
					if (State == ECoverageMacroCombinedState::Ready)
					{
						RuntimeResult += 1;
					}
					return RuntimeResult;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Payload.Amount = 20;
					ApplyPayload(21);
				}
			}
			)AS"),
			TEXT("ACoverageMacrosCombinedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Combined reflection macro actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("CombinedValue")), GetClassMetadata(ScriptClass, TEXT("CoverageCombinedKey")),
			TEXT("Combined UCLASS metadata should round-trip")));

		FStructProperty* PayloadProperty = CastField<FStructProperty>(FindScriptProperty(ScriptClass, TEXT("Payload")));
		ASSERT_THAT(IsNotNull(PayloadProperty, TEXT("Combined USTRUCT property should reflect as FStructProperty")));
		if (PayloadProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("CombinedProperty")),
			PayloadProperty->GetMetaData(TEXT("CoveragePropertyKey")),
			TEXT("Combined UPROPERTY metadata should round-trip")));

		FEnumProperty* StateProperty = CastField<FEnumProperty>(FindScriptProperty(ScriptClass, TEXT("State")));
		ASSERT_THAT(IsNotNull(StateProperty, TEXT("Combined UENUM property should reflect as FEnumProperty")));
		if (StateProperty == nullptr)
		{
			return;
		}

		UEnum* StateEnum = StateProperty->GetEnum();
		ASSERT_THAT(IsNotNull(StateEnum, TEXT("Combined enum property should expose its UEnum")));
		if (StateEnum == nullptr)
		{
			return;
		}

		const int32 ReadyStateIndex = StateEnum->GetIndexByNameString(TEXT("Ready"));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, ReadyStateIndex,
			TEXT("Combined enum should expose the Ready enumerator")));
		ASSERT_THAT(AreEqual(FString(TEXT("Ready State")),
			StateEnum != nullptr && ReadyStateIndex != INDEX_NONE ? StateEnum->GetMetaData(TEXT("DisplayName"), ReadyStateIndex) : FString(),
			TEXT("Combined UMETA DisplayName should round-trip")));

		const TSharedPtr<FAngelscriptDelegateDesc> DelegateDesc = Engine.GetDelegate(TEXT("FCoverageMacroCombinedSignal"));
		ASSERT_THAT(IsTrue(DelegateDesc.IsValid(), TEXT("Combined delegate should register an engine delegate descriptor")));
		UFunction* DelegateFunction = DelegateDesc.IsValid() ? DelegateDesc->Function : nullptr;
		ASSERT_THAT(IsNotNull(DelegateFunction, TEXT("Combined delegate should materialize a UDelegateFunction")));
		if (DelegateFunction == nullptr)
		{
			return;
		}

		FDelegateProperty* SignalProperty = CastField<FDelegateProperty>(FindScriptProperty(ScriptClass, TEXT("Signal")));
		ASSERT_THAT(IsNotNull(SignalProperty, TEXT("Combined delegate property should reflect as FDelegateProperty")));
		if (SignalProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(DelegateFunction, SignalProperty->SignatureFunction.Get(),
			TEXT("Combined delegate property should target the generated UDelegateFunction")));

		UFunction* ApplyPayloadFunction = RequireGeneratedFunction(ScriptClass, TEXT("ApplyPayload"));
		ASSERT_THAT(IsNotNull(ApplyPayloadFunction, TEXT("Combined UFUNCTION should be reflected")));
		if (ApplyPayloadFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("CombinedFunction")),
			ApplyPayloadFunction->GetMetaData(TEXT("CoverageFunctionKey")),
			TEXT("Combined UFUNCTION metadata should round-trip")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Combined reflection macro actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		const bool bRuntimeResultMatched = VerifyByPath<FIntProperty, int32>(
			*TestRunner,
			Actor,
			TEXT("RuntimeResult"),
			42,
			TEXT("Combined macro runtime path should execute BeginPlay and UFUNCTION"));
		ASSERT_THAT(IsTrue(bRuntimeResultMatched, TEXT("Combined runtime result should match")));

		FFunctionInvoker ApplyPayloadInvoker(*TestRunner, Actor, TEXT("ApplyPayload"));
		ASSERT_THAT(IsTrue(ApplyPayloadInvoker.IsValid(), TEXT("Combined ApplyPayload should be invokable through reflection")));
		if (!ApplyPayloadInvoker.IsValid())
		{
			return;
		}
		ApplyPayloadInvoker.AddParam<int32>(5);
		ASSERT_THAT(AreEqual(26, ApplyPayloadInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Combined reflected UFUNCTION invocation should consume USTRUCT and UENUM state")));
	}

	// -------------------------------------------------------------------------
	// Macro expansion boundaries: macro-like tokens in comments, strings, and
	// inactive condition branches should not be recognized as reflection macros.
	// -------------------------------------------------------------------------
	TEST_METHOD(MacroExpansionIgnoresCommentsStringsAndInactiveBranches)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_ExpansionBoundaries"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosExpansionBoundaries.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMacrosExpansionBoundaryActor : AActor
			{
				// UPROPERTY()
				// int CommentOnlyProperty = -1;

				UPROPERTY()
				FString LiteralMacroText = "UFUNCTION() int ShouldNotExist()";

				#if COOK_COMMANDLET
				UPROPERTY()
				int CookOnlyProperty = -2;
				#endif

				UFUNCTION()
				int GetLiteralLength() const
				{
					return LiteralMacroText.Len();
				}
			}
			)AS"),
			TEXT("ACoverageMacrosExpansionBoundaryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Macro expansion boundary actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNull(FindScriptProperty(ScriptClass, TEXT("CommentOnlyProperty")),
			TEXT("UPROPERTY token in a comment should not create a reflected property")));
		ASSERT_THAT(IsNull(RequireGeneratedFunction(ScriptClass, TEXT("ShouldNotExist")),
			TEXT("UFUNCTION token in a string literal should not create a reflected function")));
		ASSERT_THAT(IsNull(FindScriptProperty(ScriptClass, TEXT("CookOnlyProperty")),
			TEXT("Reflection macro in inactive commandlet branch should not be reflected in editor tests")));
		ASSERT_THAT(IsNotNull(FindScriptProperty(ScriptClass, TEXT("LiteralMacroText")),
			TEXT("Real property after comment/string macro tokens should still reflect")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Macro expansion boundary actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker GetLiteralLengthInvoker(*TestRunner, Actor, TEXT("GetLiteralLength"));
		ASSERT_THAT(IsTrue(GetLiteralLengthInvoker.IsValid(), TEXT("Boundary function should be invokable through reflection")));
		if (!GetLiteralLengthInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(32, GetLiteralLengthInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Macro-looking string literal should remain runtime string data")));
	}

	// -------------------------------------------------------------------------
	// UENUM advanced declaration with BlueprintType, DisplayName, ToolTip
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumAdvancedDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UEnumAdvanced"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUEnum.as"),
			ASTEST_AS(R"AS(
			// UENUM with BlueprintType specifier
			UENUM(BlueprintType)
			enum EAdvancedEnum
			{
				Option_A UMETA(DisplayName="First Option", ToolTip="This is the first option"),
				Option_B UMETA(DisplayName="Second Option", ToolTip="This is the second option"),
				Option_C UMETA(DisplayName="Third Option", Hidden),
				Option_MAX UMETA(Hidden)
			}

			// UENUM with category and display customization
			UENUM(BlueprintType)
			enum EDisplayEnum
			{
				Low UMETA(DisplayName="Low Priority"),
				Medium UMETA(DisplayName="Medium Priority"),
				High UMETA(DisplayName="High Priority")
			}

			UCLASS()
			class ACoverageMacrosUEnumActor : AActor
			{
				UPROPERTY(BlueprintReadWrite, Category="Enums")
				EAdvancedEnum AdvancedValue = EAdvancedEnum::Option_A;

				UPROPERTY(BlueprintReadWrite, Category="Enums")
				EDisplayEnum DisplayValue = EDisplayEnum::Medium;

				UPROPERTY()
				int TestResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test enum assignment and comparison
					AdvancedValue = EAdvancedEnum::Option_B;
					check(AdvancedValue == EAdvancedEnum::Option_B);

					// Test enum in switch statement
					switch (AdvancedValue)
					{
						case EAdvancedEnum::Option_A:
							TestResult = 1;
							break;
						case EAdvancedEnum::Option_B:
							TestResult = 2;
							break;
						case EAdvancedEnum::Option_C:
							TestResult = 3;
							break;
						default:
							TestResult = 0;
					}

					// Test display enum
					DisplayValue = EDisplayEnum::High;
					check(int(DisplayValue) == 2);
				}
			}
			)AS"),
			TEXT("ACoverageMacrosUEnumActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Advanced UENUM actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Advanced UENUM actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify enum values were set correctly
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TestResult"), 2,
			TEXT("Switch statement should set TestResult to 2 for Option_B")),
			TEXT("Advanced UENUM runtime result should verify")));

		// Verify UENUM property has correct metadata
		FProperty* EnumProp = FindScriptProperty(ScriptClass, TEXT("AdvancedValue"));
		ASSERT_THAT(IsNotNull(EnumProp, TEXT("AdvancedValue property should exist")));
		if (EnumProp == nullptr)
		{
			return;
		}

		FEnumProperty* EnumProperty = CastField<FEnumProperty>(EnumProp);
		ASSERT_THAT(IsNotNull(EnumProperty, TEXT("AdvancedValue should be backed by an enum property")));
		if (EnumProperty == nullptr)
		{
			return;
		}

		UEnum* EnumType = EnumProperty->GetEnum();
		ASSERT_THAT(IsNotNull(EnumType, TEXT("Enum type should be accessible")));
		if (EnumType == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(EnumType->HasMetaData(TEXT("BlueprintType")),
			TEXT("Enum should have BlueprintType metadata")));

		const int32 OptionAIndex = EnumType->GetIndexByNameString(TEXT("Option_A"));
		const int32 OptionBIndex = EnumType->GetIndexByNameString(TEXT("Option_B"));
		const int32 OptionCIndex = EnumType->GetIndexByNameString(TEXT("Option_C"));
		const int32 OptionMaxIndex = EnumType->GetIndexByNameString(TEXT("Option_MAX"));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, OptionAIndex, TEXT("Option_A should exist in reflected UEnum")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, OptionBIndex, TEXT("Option_B should exist in reflected UEnum")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, OptionCIndex, TEXT("Option_C should exist in reflected UEnum")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, OptionMaxIndex, TEXT("Option_MAX should exist in reflected UEnum")));
		ASSERT_THAT(AreEqual(FString(TEXT("First Option")),
			OptionAIndex != INDEX_NONE ? EnumType->GetMetaData(TEXT("DisplayName"), OptionAIndex) : FString(),
			TEXT("UMETA DisplayName should round-trip for Option_A")));
		ASSERT_THAT(AreEqual(FString(TEXT("This is the first option")),
			OptionAIndex != INDEX_NONE ? EnumType->GetMetaData(TEXT("ToolTip"), OptionAIndex) : FString(),
			TEXT("UMETA ToolTip should round-trip for Option_A")));
		ASSERT_THAT(AreEqual(FString(TEXT("Second Option")),
			OptionBIndex != INDEX_NONE ? EnumType->GetMetaData(TEXT("DisplayName"), OptionBIndex) : FString(),
			TEXT("UMETA DisplayName should round-trip for Option_B")));
		ASSERT_THAT(IsTrue(OptionCIndex != INDEX_NONE && EnumType->HasMetaData(TEXT("Hidden"), OptionCIndex),
			TEXT("UMETA Hidden should round-trip for Option_C")));
		ASSERT_THAT(IsTrue(OptionMaxIndex != INDEX_NONE && EnumType->HasMetaData(TEXT("Hidden"), OptionMaxIndex),
			TEXT("UMETA Hidden should round-trip for Option_MAX")));
	}

	// -------------------------------------------------------------------------
	// UENUM / UMETA custom metadata keys should stay scoped to the generated enum
	// and individual enum entries.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumCustomMetadataRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UEnumCustomMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUEnumCustomMetadata.as"),
			ASTEST_AS(R"AS(
			UENUM(BlueprintType, meta=(CoverageEnumKey="EnumValue", CoverageEnumMode="Strict"))
			enum ECoverageMacroCustomMetadataState
			{
				Alpha UMETA(DisplayName="Alpha Visible", CoverageEntryKey="AlphaValue"),
				Beta UMETA(ToolTip="Beta tooltip", CoverageEntryKey="BetaValue"),
				Gamma UMETA(Hidden, CoverageEntryKey="GammaValue")
			}

			UCLASS()
			class ACoverageMacrosEnumCustomMetadataActor : AActor
			{
				UPROPERTY()
				ECoverageMacroCustomMetadataState State = ECoverageMacroCustomMetadataState::Beta;

				UFUNCTION()
				int GetStateIndex() const
				{
					return int(State);
				}
			}
			)AS"),
			TEXT("ACoverageMacrosEnumCustomMetadataActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Custom UENUM metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FEnumProperty* StateProperty = CastField<FEnumProperty>(FindScriptProperty(ScriptClass, TEXT("State")));
		ASSERT_THAT(IsNotNull(StateProperty, TEXT("State should be backed by a reflected enum property")));
		if (StateProperty == nullptr)
		{
			return;
		}

		UEnum* StateEnum = StateProperty->GetEnum();
		ASSERT_THAT(IsNotNull(StateEnum, TEXT("State property should expose its generated UEnum")));
		if (StateEnum == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("EnumValue")), StateEnum->GetMetaData(TEXT("CoverageEnumKey")),
			TEXT("UENUM meta custom key should round-trip on the enum")));
		ASSERT_THAT(AreEqual(FString(TEXT("Strict")), StateEnum->GetMetaData(TEXT("CoverageEnumMode")),
			TEXT("Multiple UENUM meta keys should round-trip on the enum")));
		ASSERT_THAT(IsTrue(StateEnum->HasMetaData(TEXT("BlueprintType")),
			TEXT("BlueprintType should remain present beside custom enum metadata")));

		const int32 AlphaIndex = StateEnum->GetIndexByNameString(TEXT("Alpha"));
		const int32 BetaIndex = StateEnum->GetIndexByNameString(TEXT("Beta"));
		const int32 GammaIndex = StateEnum->GetIndexByNameString(TEXT("Gamma"));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, AlphaIndex, TEXT("Alpha enumerator should exist")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, BetaIndex, TEXT("Beta enumerator should exist")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, GammaIndex, TEXT("Gamma enumerator should exist")));
		ASSERT_THAT(AreEqual(FString(TEXT("AlphaValue")),
			AlphaIndex != INDEX_NONE ? StateEnum->GetMetaData(TEXT("CoverageEntryKey"), AlphaIndex) : FString(),
			TEXT("UMETA custom key should round-trip for Alpha")));
		ASSERT_THAT(AreEqual(FString(TEXT("BetaValue")),
			BetaIndex != INDEX_NONE ? StateEnum->GetMetaData(TEXT("CoverageEntryKey"), BetaIndex) : FString(),
			TEXT("UMETA custom key should round-trip for Beta")));
		ASSERT_THAT(AreEqual(FString(TEXT("GammaValue")),
			GammaIndex != INDEX_NONE ? StateEnum->GetMetaData(TEXT("CoverageEntryKey"), GammaIndex) : FString(),
			TEXT("UMETA custom key should round-trip for Gamma")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha Visible")),
			AlphaIndex != INDEX_NONE ? StateEnum->GetMetaData(TEXT("DisplayName"), AlphaIndex) : FString(),
			TEXT("UMETA DisplayName should remain available beside custom keys")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta tooltip")),
			BetaIndex != INDEX_NONE ? StateEnum->GetMetaData(TEXT("ToolTip"), BetaIndex) : FString(),
			TEXT("UMETA ToolTip should remain available beside custom keys")));
		ASSERT_THAT(IsTrue(GammaIndex != INDEX_NONE && StateEnum->HasMetaData(TEXT("Hidden"), GammaIndex),
			TEXT("UMETA Hidden should remain available beside custom keys")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Custom UENUM metadata actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker GetStateIndexInvoker(*TestRunner, Actor, TEXT("GetStateIndex"));
		ASSERT_THAT(IsTrue(GetStateIndexInvoker.IsValid(), TEXT("GetStateIndex should be invokable through reflection")));
		if (!GetStateIndexInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, GetStateIndexInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Custom enum metadata should not affect enum runtime value conversion")));
	}

	// -------------------------------------------------------------------------
	// UENUM bitflag metadata and integer-backed bitwise usage.
	// -------------------------------------------------------------------------
	TEST_METHOD(UEnumBitflagMetadataAndRuntimeOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UEnumBitflagMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUEnumBitflagMetadata.as"),
			ASTEST_AS(R"AS(
			UENUM(meta=(Bitflags, BitmaskEnum="ECoverageMacroFlagState"))
			enum ECoverageMacroFlagState
			{
				None = 0,
				Read = 1,
				Write = 2,
				Execute = 4
			}

			UENUM()
			enum ECoverageMacroFlagMetadataState
			{
				NoFlags UMETA(DisplayName="No Flags", ToolTip="No flag selected"),
				ReadFlag UMETA(DisplayName="Read Flag"),
				WriteFlag UMETA(ToolTip="Write permission"),
				ExecuteFlag UMETA(Hidden)
			}

			UCLASS()
			class ACoverageMacrosEnumBitflagActor : AActor
			{
				UPROPERTY()
				ECoverageMacroFlagState ReflectedFlag = ECoverageMacroFlagState::Read;

				UPROPERTY()
				ECoverageMacroFlagMetadataState ReflectedEntry = ECoverageMacroFlagMetadataState::ReadFlag;

				UPROPERTY()
				int OrResult = 0;

				UPROPERTY()
				int AndResult = 0;

				UPROPERTY()
				int XorResult = 0;

				UPROPERTY()
				int NotResult = 0;

				UPROPERTY()
				int CompoundResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					int Flags = int(ECoverageMacroFlagState::Read) | int(ECoverageMacroFlagState::Execute);
					OrResult = Flags;
					AndResult = Flags & int(ECoverageMacroFlagState::Read);
					XorResult = Flags ^ int(ECoverageMacroFlagState::Execute);
					NotResult = ~int(ECoverageMacroFlagState::Read);

					Flags |= int(ECoverageMacroFlagState::Write);
					CompoundResult = Flags;
				}
			}
			)AS"),
			TEXT("ACoverageMacrosEnumBitflagActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Enum bitflag metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FEnumProperty* ReflectedFlagProperty = CastField<FEnumProperty>(FindScriptProperty(ScriptClass, TEXT("ReflectedFlag")));
		ASSERT_THAT(IsNotNull(ReflectedFlagProperty, TEXT("ReflectedFlag should be backed by an enum property")));
		if (ReflectedFlagProperty == nullptr)
		{
			return;
		}

		UEnum* FlagEnum = ReflectedFlagProperty->GetEnum();
		ASSERT_THAT(IsNotNull(FlagEnum, TEXT("ReflectedFlag should expose its generated UEnum")));
		if (FlagEnum == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(FlagEnum->GetBoolMetaData(TEXT("Bitflags")),
			TEXT("UENUM meta=(Bitflags) should round-trip as bool enum metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("ECoverageMacroFlagState")), FlagEnum->GetMetaData(TEXT("BitmaskEnum")),
			TEXT("UENUM meta=(BitmaskEnum=...) should round-trip as enum metadata")));

		const int32 NoneIndex = FlagEnum->GetIndexByNameString(TEXT("None"));
		const int32 ReadIndex = FlagEnum->GetIndexByNameString(TEXT("Read"));
		const int32 WriteIndex = FlagEnum->GetIndexByNameString(TEXT("Write"));
		const int32 ExecuteIndex = FlagEnum->GetIndexByNameString(TEXT("Execute"));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, NoneIndex, TEXT("None enumerator should exist")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, ReadIndex, TEXT("Read enumerator should exist")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, WriteIndex, TEXT("Write enumerator should exist")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, ExecuteIndex, TEXT("Execute enumerator should exist")));
		if (NoneIndex == INDEX_NONE || ReadIndex == INDEX_NONE || WriteIndex == INDEX_NONE || ExecuteIndex == INDEX_NONE)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0LL, FlagEnum->GetValueByIndex(NoneIndex), TEXT("None should preserve explicit bit value 0")));
		ASSERT_THAT(AreEqual(1LL, FlagEnum->GetValueByIndex(ReadIndex), TEXT("Read should preserve explicit bit value 1")));
		ASSERT_THAT(AreEqual(2LL, FlagEnum->GetValueByIndex(WriteIndex), TEXT("Write should preserve explicit bit value 2")));
		ASSERT_THAT(AreEqual(4LL, FlagEnum->GetValueByIndex(ExecuteIndex), TEXT("Execute should preserve explicit bit value 4")));

		FEnumProperty* ReflectedEntryProperty = CastField<FEnumProperty>(FindScriptProperty(ScriptClass, TEXT("ReflectedEntry")));
		ASSERT_THAT(IsNotNull(ReflectedEntryProperty, TEXT("ReflectedEntry should be backed by an enum property")));
		if (ReflectedEntryProperty == nullptr)
		{
			return;
		}

		UEnum* MetadataEnum = ReflectedEntryProperty->GetEnum();
		ASSERT_THAT(IsNotNull(MetadataEnum, TEXT("ReflectedEntry should expose its generated UEnum")));
		if (MetadataEnum == nullptr)
		{
			return;
		}

		const int32 NoFlagsIndex = MetadataEnum->GetIndexByNameString(TEXT("NoFlags"));
		const int32 ReadFlagIndex = MetadataEnum->GetIndexByNameString(TEXT("ReadFlag"));
		const int32 WriteFlagIndex = MetadataEnum->GetIndexByNameString(TEXT("WriteFlag"));
		const int32 ExecuteFlagIndex = MetadataEnum->GetIndexByNameString(TEXT("ExecuteFlag"));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, NoFlagsIndex, TEXT("NoFlags enumerator should exist")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, ReadFlagIndex, TEXT("ReadFlag enumerator should exist")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, WriteFlagIndex, TEXT("WriteFlag enumerator should exist")));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, ExecuteFlagIndex, TEXT("ExecuteFlag enumerator should exist")));
		ASSERT_THAT(AreEqual(FString(TEXT("No Flags")),
			NoFlagsIndex != INDEX_NONE ? MetadataEnum->GetMetaData(TEXT("DisplayName"), NoFlagsIndex) : FString(),
			TEXT("UMETA DisplayName should round-trip for bitflag enum entries")));
		ASSERT_THAT(AreEqual(FString(TEXT("No flag selected")),
			NoFlagsIndex != INDEX_NONE ? MetadataEnum->GetMetaData(TEXT("ToolTip"), NoFlagsIndex) : FString(),
			TEXT("UMETA ToolTip should round-trip for bitflag enum entries")));
		ASSERT_THAT(AreEqual(FString(TEXT("Read Flag")),
			ReadFlagIndex != INDEX_NONE ? MetadataEnum->GetMetaData(TEXT("DisplayName"), ReadFlagIndex) : FString(),
			TEXT("UMETA DisplayName should round-trip for non-zero bitflag entries")));
		ASSERT_THAT(AreEqual(FString(TEXT("Write permission")),
			WriteFlagIndex != INDEX_NONE ? MetadataEnum->GetMetaData(TEXT("ToolTip"), WriteFlagIndex) : FString(),
			TEXT("UMETA ToolTip should round-trip for non-zero bitflag entries")));
		ASSERT_THAT(IsTrue(ExecuteFlagIndex != INDEX_NONE && MetadataEnum->HasMetaData(TEXT("Hidden"), ExecuteFlagIndex),
			TEXT("UMETA Hidden should round-trip for bitflag enum entries")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Enum bitflag metadata actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OrResult"), 5,
			TEXT("Bitwise OR should combine Read and Execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AndResult"), 1,
			TEXT("Bitwise AND should keep the Read flag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("XorResult"), 1,
			TEXT("Bitwise XOR should toggle the Execute flag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NotResult"), -2,
			TEXT("Bitwise NOT should invert the Read flag integer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CompoundResult"), 7,
			TEXT("Compound OR assignment should add Write to existing flags"))));
	}

	// -------------------------------------------------------------------------
	// UFUNCTION ordinary members, const members, flags, and editor metadata.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionSpecifiersMetadataAndConstReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UFunctionSpecifiersMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUFunctionSpecifiersMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMacrosFunctionSpecifiersActor : AActor
			{
				UPROPERTY()
				int StoredValue = 5;

				UFUNCTION()
				void SetStoredValue(int Value)
				{
					StoredValue = Value;
				}

				UFUNCTION()
				int GetStoredValue() const
				{
					return StoredValue;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Functions", CallInEditor, meta=(
					DisplayName="Visible Coverage Action",
					Keywords="coverage macro function",
					ToolTip="Full function tooltip",
					ShortToolTip="Short function tooltip",
					CompactNodeTitle="ACT",
					AdvancedDisplay="OptionalValue,OptionalLabel",
					WorldContext="Target",
					DefaultToSelf="Target",
					HidePin="Target",
					AutoCreateRefTerm="OptionalLabel"))
				void VisibleAction(UObject Target, int RequiredValue, int OptionalValue, const FString&in OptionalLabel)
				{
					StoredValue = RequiredValue + OptionalValue + OptionalLabel.Len();
					if (Target != nullptr)
					{
						StoredValue += 1;
					}
				}

				UFUNCTION(BlueprintPure, Category="Coverage|Functions", meta=(DisplayName="Read Coverage Value"))
				int ReadValue() const
				{
					return StoredValue;
				}

				UFUNCTION(Exec, Category="Coverage|Console")
				void CoverageConsoleCommand()
				{
					StoredValue = 77;
				}
			}
			)AS"),
			TEXT("ACoverageMacrosFunctionSpecifiersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION specifier metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* SetStoredValueFunction = RequireGeneratedFunction(ScriptClass, TEXT("SetStoredValue"));
		UFunction* GetStoredValueFunction = RequireGeneratedFunction(ScriptClass, TEXT("GetStoredValue"));
		ASSERT_THAT(IsNotNull(SetStoredValueFunction, TEXT("Plain UFUNCTION member should be generated")));
		ASSERT_THAT(IsNotNull(GetStoredValueFunction, TEXT("Plain const UFUNCTION member should be generated")));
		if (SetStoredValueFunction == nullptr || GetStoredValueFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SetStoredValueFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Plain UFUNCTION member should be BlueprintCallable through reflection")));
		ASSERT_THAT(IsFalse(SetStoredValueFunction->HasAnyFunctionFlags(FUNC_Const),
			TEXT("Plain non-const UFUNCTION member should not set FUNC_Const")));
		ASSERT_THAT(IsTrue(GetStoredValueFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Plain const UFUNCTION member should be BlueprintCallable through reflection")));
		ASSERT_THAT(IsTrue(GetStoredValueFunction->HasAnyFunctionFlags(FUNC_Const),
			TEXT("Plain const UFUNCTION member should set FUNC_Const")));

		UFunction* VisibleActionFunction = RequireGeneratedFunction(ScriptClass, TEXT("VisibleAction"));
		ASSERT_THAT(IsNotNull(VisibleActionFunction, TEXT("BlueprintCallable metadata function should be generated")));
		if (VisibleActionFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VisibleActionFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintCallable should set FUNC_BlueprintCallable")));
		ASSERT_THAT(IsFalse(VisibleActionFunction->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("BlueprintCallable action should not set FUNC_BlueprintPure")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Functions")), VisibleActionFunction->GetMetaData(TEXT("Category")),
			TEXT("UFUNCTION Category metadata should round-trip")));
		ASSERT_THAT(IsTrue(VisibleActionFunction->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor should be preserved as function metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Visible Coverage Action")), VisibleActionFunction->GetMetaData(TEXT("DisplayName")),
			TEXT("UFUNCTION DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("coverage macro function")), VisibleActionFunction->GetMetaData(TEXT("Keywords")),
			TEXT("UFUNCTION Keywords metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Full function tooltip")), VisibleActionFunction->GetMetaData(TEXT("ToolTip")),
			TEXT("UFUNCTION ToolTip metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Short function tooltip")), VisibleActionFunction->GetMetaData(TEXT("ShortToolTip")),
			TEXT("UFUNCTION ShortToolTip metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("ACT")), VisibleActionFunction->GetMetaData(TEXT("CompactNodeTitle")),
			TEXT("UFUNCTION CompactNodeTitle metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionalValue,OptionalLabel")), VisibleActionFunction->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("UFUNCTION AdvancedDisplay metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), VisibleActionFunction->GetMetaData(TEXT("WorldContext")),
			TEXT("UFUNCTION WorldContext metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), VisibleActionFunction->GetMetaData(TEXT("DefaultToSelf")),
			TEXT("UFUNCTION DefaultToSelf metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), VisibleActionFunction->GetMetaData(TEXT("HidePin")),
			TEXT("UFUNCTION HidePin metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionalLabel")), VisibleActionFunction->GetMetaData(TEXT("AutoCreateRefTerm")),
			TEXT("UFUNCTION AutoCreateRefTerm metadata should round-trip")));

		const TArray<FProperty*> VisibleActionParams = GetOrderedParameters(VisibleActionFunction);
		ASSERT_THAT(AreEqual(4, VisibleActionParams.Num(), TEXT("VisibleAction should expose four reflected parameters")));
		ASSERT_THAT(IsFalse(VisibleActionParams.IsValidIndex(0) && VisibleActionParams[0]->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("Target parameter should not be AdvancedDisplay")));
		ASSERT_THAT(IsFalse(VisibleActionParams.IsValidIndex(1) && VisibleActionParams[1]->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("RequiredValue parameter should not be AdvancedDisplay")));
		ASSERT_THAT(IsTrue(VisibleActionParams.IsValidIndex(2) && VisibleActionParams[2]->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("OptionalValue parameter should be AdvancedDisplay")));
		ASSERT_THAT(IsTrue(VisibleActionParams.IsValidIndex(3) && VisibleActionParams[3]->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("OptionalLabel parameter should be AdvancedDisplay")));

		UFunction* ReadValueFunction = RequireGeneratedFunction(ScriptClass, TEXT("ReadValue"));
		ASSERT_THAT(IsNotNull(ReadValueFunction, TEXT("BlueprintPure function should be generated")));
		if (ReadValueFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReadValueFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintPure should also be BlueprintCallable for reflection")));
		ASSERT_THAT(IsTrue(ReadValueFunction->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("BlueprintPure should set FUNC_BlueprintPure")));
		ASSERT_THAT(IsTrue(ReadValueFunction->HasAnyFunctionFlags(FUNC_Const),
			TEXT("BlueprintPure const method should set FUNC_Const")));
		ASSERT_THAT(AreEqual(FString(TEXT("Read Coverage Value")), ReadValueFunction->GetMetaData(TEXT("DisplayName")),
			TEXT("BlueprintPure DisplayName metadata should round-trip")));

		UFunction* ConsoleCommandFunction = RequireGeneratedFunction(ScriptClass, TEXT("CoverageConsoleCommand"));
		ASSERT_THAT(IsNotNull(ConsoleCommandFunction, TEXT("Exec function should be generated")));
		if (ConsoleCommandFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ConsoleCommandFunction->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("Exec should set FUNC_Exec")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Console")), ConsoleCommandFunction->GetMetaData(TEXT("Category")),
			TEXT("Exec function Category metadata should round-trip")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UFUNCTION specifier metadata actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker SetStoredValueInvoker(*TestRunner, Actor, TEXT("SetStoredValue"));
		ASSERT_THAT(IsTrue(SetStoredValueInvoker.IsValid(), TEXT("SetStoredValue should be invokable through reflection")));
		if (!SetStoredValueInvoker.IsValid())
		{
			return;
		}
		SetStoredValueInvoker.AddParam<int32>(12);
		ASSERT_THAT(IsTrue(SetStoredValueInvoker.Call(), TEXT("Plain UFUNCTION reflected invocation should succeed")));

		FFunctionInvoker GetStoredValueInvoker(*TestRunner, Actor, TEXT("GetStoredValue"));
		ASSERT_THAT(IsTrue(GetStoredValueInvoker.IsValid(), TEXT("GetStoredValue should be invokable through reflection")));
		if (!GetStoredValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(12, GetStoredValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Plain const UFUNCTION reflected invocation should read current state")));

		FFunctionInvoker VisibleActionInvoker(*TestRunner, Actor, TEXT("VisibleAction"));
		ASSERT_THAT(IsTrue(VisibleActionInvoker.IsValid(), TEXT("VisibleAction should be invokable through reflection")));
		if (!VisibleActionInvoker.IsValid())
		{
			return;
		}
		VisibleActionInvoker.AddParam<UObject*>(Actor);
		VisibleActionInvoker.AddParam<int32>(20);
		VisibleActionInvoker.AddParam<int32>(3);
		VisibleActionInvoker.AddParam<FString>(FString(TEXT("abcd")));
		ASSERT_THAT(IsTrue(VisibleActionInvoker.Call(), TEXT("BlueprintCallable reflected invocation should succeed")));

		FFunctionInvoker ReadValueInvoker(*TestRunner, Actor, TEXT("ReadValue"));
		ASSERT_THAT(IsTrue(ReadValueInvoker.IsValid(), TEXT("ReadValue should be invokable through reflection")));
		if (!ReadValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(28, ReadValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("BlueprintCallable invocation should update state visible to BlueprintPure getter")));

		FFunctionInvoker ConsoleCommandInvoker(*TestRunner, Actor, TEXT("CoverageConsoleCommand"));
		ASSERT_THAT(IsTrue(ConsoleCommandInvoker.IsValid(), TEXT("Exec function should be invokable through reflection")));
		if (!ConsoleCommandInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConsoleCommandInvoker.Call(), TEXT("Exec reflected invocation should succeed")));

		FFunctionInvoker ReadAfterExecInvoker(*TestRunner, Actor, TEXT("ReadValue"));
		ASSERT_THAT(IsTrue(ReadAfterExecInvoker.IsValid(), TEXT("ReadValue should remain invokable after Exec")));
		if (!ReadAfterExecInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(77, ReadAfterExecInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Exec reflected invocation should update script actor state")));
	}

	// -------------------------------------------------------------------------
	// UFUNCTION special scenarios: recursion, virtual BlueprintOverride dispatch,
	// and calling a parent implementation through Super:: from a child method.
	// -------------------------------------------------------------------------
	TEST_METHOD(UFunctionRecursiveVirtualOverrideAndSuperCall)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UFunctionRecursiveVirtualSuper"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* BaseClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUFunctionRecursiveVirtualSuper.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMacrosFunctionVirtualBase : AActor
			{
				UPROPERTY()
				int TraceValue = 0;

				UFUNCTION(BlueprintEvent)
				int ComputeVirtual(int Value)
				{
					TraceValue = TraceValue * 10 + 1;
					return Value + 1;
				}

				UFUNCTION()
				int Factorial(int Value)
				{
					if (Value <= 1)
					{
						return 1;
					}

					return Value * Factorial(Value - 1);
				}

				UFUNCTION()
				int DispatchVirtual(int Value)
				{
					return ComputeVirtual(Value);
				}

				UFUNCTION()
				int BaseOnlyCompute(int Value)
				{
					TraceValue = TraceValue * 10 + 4;
					return Value + 4;
				}
			}

			UCLASS()
			class ACoverageMacrosFunctionVirtualChild : ACoverageMacrosFunctionVirtualBase
			{
				UFUNCTION(BlueprintOverride)
				int ComputeVirtual(int Value)
				{
					TraceValue = TraceValue * 10 + 2;
					return Value + 10;
				}

				UFUNCTION()
				int CallParentCompute(int Value)
				{
					return Super::BaseOnlyCompute(Value) + 100;
				}
			}
			)AS"),
			TEXT("ACoverageMacrosFunctionVirtualBase"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("Recursive virtual UFUNCTION base class should compile")));
		if (BaseClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageMacrosFunctionVirtualChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("Recursive virtual UFUNCTION child class should compile")));
		if (ChildClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(BaseClass), TEXT("Virtual UFUNCTION child should inherit from base")));

		UFunction* BaseComputeVirtualFunction = RequireGeneratedFunction(BaseClass, TEXT("ComputeVirtual"));
		UFunction* ChildComputeVirtualFunction = RequireGeneratedFunction(ChildClass, TEXT("ComputeVirtual"));
		UFunction* FactorialFunction = RequireGeneratedFunction(BaseClass, TEXT("Factorial"));
		UFunction* CallParentComputeFunction = RequireGeneratedFunction(ChildClass, TEXT("CallParentCompute"));
		ASSERT_THAT(IsNotNull(BaseComputeVirtualFunction, TEXT("Base BlueprintEvent UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ChildComputeVirtualFunction, TEXT("Child BlueprintOverride UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(FactorialFunction, TEXT("Recursive UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(CallParentComputeFunction, TEXT("Super:: caller UFUNCTION should be generated")));
		if (BaseComputeVirtualFunction == nullptr
			|| ChildComputeVirtualFunction == nullptr
			|| FactorialFunction == nullptr
			|| CallParentComputeFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(BaseComputeVirtualFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("BlueprintEvent base function should set FUNC_BlueprintEvent")));
		ASSERT_THAT(IsTrue(ChildComputeVirtualFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("BlueprintOverride child function should surface as FUNC_BlueprintEvent")));
		ASSERT_THAT(IsFalse(FactorialFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("Plain recursive UFUNCTION should not set FUNC_BlueprintEvent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BaseActor = SpawnScriptActor(*TestRunner, Spawner, BaseClass);
		ASSERT_THAT(IsNotNull(BaseActor, TEXT("Recursive virtual base actor should spawn")));
		if (BaseActor == nullptr)
		{
			return;
		}

		AActor* ChildActor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(ChildActor, TEXT("Recursive virtual child actor should spawn")));
		if (ChildActor == nullptr)
		{
			return;
		}

		FFunctionInvoker BaseDispatchInvoker(*TestRunner, BaseActor, TEXT("DispatchVirtual"));
		ASSERT_THAT(IsTrue(BaseDispatchInvoker.IsValid(), TEXT("Base virtual dispatch should be invokable")));
		if (!BaseDispatchInvoker.IsValid())
		{
			return;
		}
		BaseDispatchInvoker.AddParam<int32>(7);
		ASSERT_THAT(AreEqual(8, BaseDispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Base virtual dispatch should execute the parent implementation")));

		FFunctionInvoker FactorialInvoker(*TestRunner, ChildActor, TEXT("Factorial"));
		ASSERT_THAT(IsTrue(FactorialInvoker.IsValid(), TEXT("Recursive UFUNCTION should be invokable on child")));
		if (!FactorialInvoker.IsValid())
		{
			return;
		}
		FactorialInvoker.AddParam<int32>(5);
		ASSERT_THAT(AreEqual(120, FactorialInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Recursive UFUNCTION should call itself until the base case")));

		FFunctionInvoker ChildDispatchInvoker(*TestRunner, ChildActor, TEXT("DispatchVirtual"));
		ASSERT_THAT(IsTrue(ChildDispatchInvoker.IsValid(), TEXT("Inherited dispatch UFUNCTION should be invokable on child")));
		if (!ChildDispatchInvoker.IsValid())
		{
			return;
		}
		ChildDispatchInvoker.AddParam<int32>(7);
		ASSERT_THAT(AreEqual(17, ChildDispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Inherited base call site should dispatch to child BlueprintOverride")));

		FFunctionInvoker ParentComputeInvoker(*TestRunner, ChildActor, TEXT("CallParentCompute"));
		ASSERT_THAT(IsTrue(ParentComputeInvoker.IsValid(), TEXT("Super:: caller should be invokable on child")));
		if (!ParentComputeInvoker.IsValid())
		{
			return;
		}
		ParentComputeInvoker.AddParam<int32>(7);
		ASSERT_THAT(AreEqual(111, ParentComputeInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Super:: call should execute the parent UFUNCTION implementation")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, BaseActor, TEXT("TraceValue"), 1,
			TEXT("Base actor trace should record the parent virtual implementation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, ChildActor, TEXT("TraceValue"), 24,
			TEXT("Child actor trace should record child override then Super:: parent call"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT advanced usage with nested types, constructors, and operators
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructAdvancedUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UStructAdvanced"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUStruct.as"),
			ASTEST_AS(R"AS(
			// Advanced USTRUCT with custom constructor and operators
			USTRUCT(BlueprintType)
			struct FAdvancedStruct
			{
				UPROPERTY(BlueprintReadWrite, Category="Data")
				int Value = 0;

				UPROPERTY(BlueprintReadWrite, Category="Data")
				FString Name = "Default";

				UPROPERTY(BlueprintReadWrite, Category="Data")
				FVector Position;

				// Custom constructor
				FAdvancedStruct()
				{
					Value = 0;
					Name = "Default";
					Position = FVector(0, 0, 0);
				}

				FAdvancedStruct(int InValue, FString InName)
				{
					Value = InValue;
					Name = InName;
					Position = FVector(0, 0, 0);
				}

				// Operator overloads
				bool opEquals(const FAdvancedStruct&in Other) const
				{
					return Value == Other.Value && Name == Other.Name;
				}

				int opCmp(const FAdvancedStruct&in Other) const
				{
					if (Value < Other.Value)
					{
						return -1;
					}

					if (Value > Other.Value)
					{
						return 1;
					}

					return 0;
				}

				FAdvancedStruct opAdd(const FAdvancedStruct&in Other) const
				{
					FAdvancedStruct Result;
					Result.Value = Value + Other.Value;
					Result.Name = Name + Other.Name;
					Result.Position = Position + Other.Position;
					return Result;
				}
			}

			// Nested struct within struct
			USTRUCT(BlueprintType)
			struct FComplexStruct
			{
				UPROPERTY(BlueprintReadWrite)
				FAdvancedStruct Inner;

				UPROPERTY(BlueprintReadWrite)
				TArray<FAdvancedStruct> StructArray;

				UPROPERTY(BlueprintReadWrite)
				TMap<int, FAdvancedStruct> StructMap;
			}

			UCLASS()
			class ACoverageMacrosUStructActor : AActor
			{
				UPROPERTY(BlueprintReadWrite)
				FAdvancedStruct Data1;

				UPROPERTY(BlueprintReadWrite)
				FAdvancedStruct Data2;

				UPROPERTY(BlueprintReadWrite)
				FComplexStruct ComplexData;

				UPROPERTY()
				int ComparisonResult = -1;

				UPROPERTY()
				int AdditionValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test constructor
					Data1 = FAdvancedStruct(100, "First");
					Data2 = FAdvancedStruct(200, "Second");

					// Test comparison operators
					ComparisonResult = (Data1 == Data2) ? 0 : 1;
					check(ComparisonResult == 1);

					// Test addition operator
					FAdvancedStruct Sum = Data1 + Data2;
					AdditionValue = Sum.Value;
					check(AdditionValue == 300);
					check(Sum.Name == "FirstSecond");

					// Test nested struct
					ComplexData.Inner = FAdvancedStruct(500, "Inner");
					ComplexData.StructArray.Add(Data1);
					ComplexData.StructArray.Add(Data2);
					ComplexData.StructMap.Add(1, Data1);
					ComplexData.StructMap.Add(2, Data2);

					// Verify array access
					check(ComplexData.StructArray.Num() == 2);
					check(ComplexData.StructArray[0].Value == 100);
					check(ComplexData.StructArray[1].Value == 200);

					// Verify map access
					check(ComplexData.StructMap.Num() == 2);
					check(ComplexData.StructMap[1].Value == 100);
					check(ComplexData.StructMap[2].Value == 200);
				}
			}
			)AS"),
			TEXT("ACoverageMacrosUStructActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Advanced USTRUCT actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Advanced USTRUCT actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify struct operations
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComparisonResult"), 1,
			TEXT("Struct comparison should return 1 for different structs")),
			TEXT("Struct comparison result should verify")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AdditionValue"), 300,
			TEXT("Struct addition should sum values")),
			TEXT("Struct addition result should verify")));

		// Verify nested struct
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComplexData.Inner.Value"), 500,
			TEXT("Nested struct value should be set")),
			TEXT("Nested struct value should verify")));
	}

	// -------------------------------------------------------------------------
	// UPARAM parameter modifiers in functions
	// -------------------------------------------------------------------------
	TEST_METHOD(UParamModifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_UPARAM"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosUPARAM.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMacrosUParamActor : AActor
			{
				UPROPERTY()
				int ResultValue = 0;

				UPROPERTY()
				FString ResultString;

				// UPARAM with DisplayName
				UFUNCTION(BlueprintCallable, Category="Testing")
				void ProcessValue(
					UPARAM(DisplayName="Input Number") int InValue,
					UPARAM(DisplayName="Multiplier") int Multiplier,
					UPARAM(DisplayName="Result", ref) int&out OutResult)
				{
					OutResult = InValue * Multiplier;
				}

				// UPARAM with ref modifier for output parameters
				UFUNCTION(BlueprintCallable, Category="Testing")
				void SplitValue(
					UPARAM(DisplayName="Input") int Value,
					UPARAM(ref) int&out Half1,
					UPARAM(ref) int&out Half2)
				{
					Half1 = Value / 2;
					Half2 = Value - Half1;
				}

				// UPARAM with const ref input
				UFUNCTION(BlueprintCallable, Category="Testing")
				void ProcessString(
					UPARAM(DisplayName="Input Text") const FString&in Input,
					UPARAM(DisplayName="Output Text", ref) FString&out Output)
				{
					Output = Input + " Processed";
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					int Result;
					ProcessValue(10, 5, Result);
					ResultValue = Result;
					check(ResultValue == 50);

					int Half1, Half2;
					SplitValue(100, Half1, Half2);
					check(Half1 == 50);
					check(Half2 == 50);

					FString Output;
					ProcessString("Test", Output);
					ResultString = Output;
					check(ResultString == "Test Processed");
				}
			}
			)AS"),
			TEXT("ACoverageMacrosUParamActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UPARAM actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UPARAM actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify UPARAM functions executed correctly
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResultValue"), 50,
			TEXT("UPARAM function should calculate result correctly")),
			TEXT("UPARAM numeric result should verify")));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ResultString"),
			FString(TEXT("Test Processed")), TEXT("UPARAM string function should process text")),
			TEXT("UPARAM string result should verify")));

		// Verify UFUNCTION metadata
		UFunction* ProcessFunc = RequireGeneratedFunction(ScriptClass, TEXT("ProcessValue"));
		ASSERT_THAT(IsNotNull(ProcessFunc, TEXT("ProcessValue function should exist")));
		if (ProcessFunc == nullptr)
		{
			return;
		}
		const TArray<FProperty*> ProcessParams = GetOrderedParameters(ProcessFunc);
		ASSERT_THAT(AreEqual(3, ProcessParams.Num(), TEXT("ProcessValue should expose three reflected parameters")));
		ASSERT_THAT(AreEqual(FString(TEXT("Input Number")), GetParameterDisplayName(ProcessParams.IsValidIndex(0) ? ProcessParams[0] : nullptr),
			TEXT("UPARAM DisplayName should round-trip for InValue")));
		ASSERT_THAT(AreEqual(FString(TEXT("Multiplier")), GetParameterDisplayName(ProcessParams.IsValidIndex(1) ? ProcessParams[1] : nullptr),
			TEXT("UPARAM DisplayName should round-trip for Multiplier")));
		ASSERT_THAT(AreEqual(FString(TEXT("Result")), GetParameterDisplayName(ProcessParams.IsValidIndex(2) ? ProcessParams[2] : nullptr),
			TEXT("UPARAM DisplayName should round-trip for OutResult")));
		ASSERT_THAT(IsTrue(ProcessParams.IsValidIndex(2) && ProcessParams[2]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("UPARAM(ref) output parameter should reflect CPF_OutParm")));

		UFunction* SplitFunc = RequireGeneratedFunction(ScriptClass, TEXT("SplitValue"));
		ASSERT_THAT(IsNotNull(SplitFunc, TEXT("SplitValue function should exist")));
		if (SplitFunc == nullptr)
		{
			return;
		}
		const TArray<FProperty*> SplitParams = GetOrderedParameters(SplitFunc);
		ASSERT_THAT(AreEqual(3, SplitParams.Num(), TEXT("SplitValue should expose three reflected parameters")));
		ASSERT_THAT(AreEqual(FString(TEXT("Input")), GetParameterDisplayName(SplitParams.IsValidIndex(0) ? SplitParams[0] : nullptr),
			TEXT("UPARAM DisplayName should round-trip for SplitValue input")));
		ASSERT_THAT(IsTrue(SplitParams.IsValidIndex(1) && SplitParams[1]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("UPARAM(ref) Half1 should reflect CPF_OutParm")));
		ASSERT_THAT(IsTrue(SplitParams.IsValidIndex(2) && SplitParams[2]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("UPARAM(ref) Half2 should reflect CPF_OutParm")));

		UFunction* ProcessStringFunc = RequireGeneratedFunction(ScriptClass, TEXT("ProcessString"));
		ASSERT_THAT(IsNotNull(ProcessStringFunc, TEXT("ProcessString function should exist")));
		if (ProcessStringFunc == nullptr)
		{
			return;
		}
		const TArray<FProperty*> ProcessStringParams = GetOrderedParameters(ProcessStringFunc);
		ASSERT_THAT(AreEqual(2, ProcessStringParams.Num(), TEXT("ProcessString should expose two reflected parameters")));
		ASSERT_THAT(AreEqual(FString(TEXT("Input Text")), GetParameterDisplayName(ProcessStringParams.IsValidIndex(0) ? ProcessStringParams[0] : nullptr),
			TEXT("UPARAM DisplayName should round-trip for const ref input")));
		ASSERT_THAT(AreEqual(FString(TEXT("Output Text")), GetParameterDisplayName(ProcessStringParams.IsValidIndex(1) ? ProcessStringParams[1] : nullptr),
			TEXT("UPARAM DisplayName should round-trip for string output")));
		ASSERT_THAT(IsTrue(ProcessStringParams.IsValidIndex(1) && ProcessStringParams[1]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("UPARAM(ref) string output should reflect CPF_OutParm")));
	}

	// -------------------------------------------------------------------------
	// BlueprintEvent - event wrapper metadata and dispatch.
	// -------------------------------------------------------------------------
	TEST_METHOD(BlueprintEventMetadataAndDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_BlueprintEventMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoveragesMacrosBlueprintEventMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoveragesMacrosBlueprintEventActor : AActor
			{
				UPROPERTY()
				int EventCallCount = 0;

				UPROPERTY()
				int EventValue = 0;

				// BlueprintEvent - can be overridden by Blueprint child classes
				UFUNCTION(BlueprintEvent, Category="Events")
				void OnCustomEvent(int Value)
				{
					EventCallCount++;
					EventValue = Value;
				}

				// BlueprintEvent with return value
				UFUNCTION(BlueprintEvent, Category="Events")
				int CalculateValue(int Input)
				{
					return Input * 2;
				}

				// BlueprintEvent with multiple parameters
				UFUNCTION(BlueprintEvent, Category="Events")
				void OnComplexEvent(int IntParam, FString StringParam, FVector VectorParam)
				{
					EventCallCount += IntParam;
					EventValue = int(VectorParam.X);
				}

				// Function that would call the event (in real usage)
				UFUNCTION(BlueprintCallable, Category="Testing")
				void TriggerEvent(int Value)
				{
					OnCustomEvent(CalculateValue(Value));
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TriggerEvent(21);
				}
			}
			)AS"),
			TEXT("ACoveragesMacrosBlueprintEventActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintEvent actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintEvent actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify function metadata
		UFunction* EventFunc = RequireGeneratedFunction(ScriptClass, TEXT("OnCustomEvent"));
		ASSERT_THAT(IsNotNull(EventFunc, TEXT("OnCustomEvent should exist")));
		if (EventFunc == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(EventFunc->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("OnCustomEvent should have BlueprintEvent flag")));

		UFunction* CalcFunc = RequireGeneratedFunction(ScriptClass, TEXT("CalculateValue"));
		ASSERT_THAT(IsNotNull(CalcFunc, TEXT("CalculateValue should exist")));
		if (CalcFunc == nullptr)
		{
			return;
		}

		UFunction* ComplexFunc = RequireGeneratedFunction(ScriptClass, TEXT("OnComplexEvent"));
		ASSERT_THAT(IsNotNull(ComplexFunc, TEXT("OnComplexEvent should exist")));
		if (ComplexFunc == nullptr)
		{
			return;
		}

		// Verify trigger function worked
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 1,
			TEXT("Event trigger function should increment counter")),
			TEXT("BlueprintEvent call count should verify")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventValue"), 42,
			TEXT("Event trigger function should set value")),
			TEXT("BlueprintEvent value should verify")));
	}

	// -------------------------------------------------------------------------
	// BlueprintEvent - Event with AS default implementation and BP override surface.
	// -------------------------------------------------------------------------
	TEST_METHOD(BlueprintEventDefaultImplementations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMacros_BlueprintEventDefaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMacrosBlueprintEventDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMacrosBlueprintEventDefaultsActor : AActor
			{
				UPROPERTY()
				int NativeEventResult = 0;

				UPROPERTY()
				FString NativeEventString;

				// BlueprintEvent with default implementation
				UFUNCTION(BlueprintEvent, Category="Events")
				int ProcessNativeEvent(int Input)
				{
					// Default implementation - can be overridden in Blueprint
					return Input * 2;
				}

				// BlueprintEvent with string processing
				UFUNCTION(BlueprintEvent, Category="Events")
				FString FormatNativeEvent(const FString&in Input, int Count)
				{
					FString Result = Input;
					for (int i = 1; i < Count; i++)
					{
						Result = Result + " " + Input;
					}
					return Result;
				}

				// BlueprintEvent with void return
				UFUNCTION(BlueprintEvent, Category="Events")
				void ExecuteNativeEvent(int Value)
				{
					NativeEventResult = Value * 3;
				}

				// Function that calls native events
				UFUNCTION(BlueprintCallable, Category="Testing")
				void TestNativeEvents()
				{
					// Call native event with default implementation
					int Result = ProcessNativeEvent(10);
					NativeEventResult = Result;

					// Call string native event
					NativeEventString = FormatNativeEvent("Test", 3);

					// Call void native event
					ExecuteNativeEvent(5);
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TestNativeEvents();
				}
			}
			)AS"),
			TEXT("ACoverageMacrosBlueprintEventDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintEvent defaults actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintEvent defaults actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify native event function metadata
		UFunction* ProcessFunc = RequireGeneratedFunction(ScriptClass, TEXT("ProcessNativeEvent"));
		ASSERT_THAT(IsNotNull(ProcessFunc, TEXT("ProcessNativeEvent should exist")));
		if (ProcessFunc == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ProcessFunc->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("ProcessNativeEvent should have BlueprintEvent flag")));

		UFunction* FormatFunc = RequireGeneratedFunction(ScriptClass, TEXT("FormatNativeEvent"));
		ASSERT_THAT(IsNotNull(FormatFunc, TEXT("FormatNativeEvent should exist")));
		if (FormatFunc == nullptr)
		{
			return;
		}

		UFunction* ExecuteFunc = RequireGeneratedFunction(ScriptClass, TEXT("ExecuteNativeEvent"));
		ASSERT_THAT(IsNotNull(ExecuteFunc, TEXT("ExecuteNativeEvent should exist")));
		if (ExecuteFunc == nullptr)
		{
			return;
		}

		// Verify native event default implementations executed
		// Note: ExecuteNativeEvent overwrites NativeEventResult, so we check its result
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NativeEventResult"), 15,
			TEXT("ExecuteNativeEvent should set result to 5 * 3 = 15")),
			TEXT("BlueprintEvent integer result should verify")));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NativeEventString"),
			FString(TEXT("Test Test Test")), TEXT("FormatNativeEvent should repeat string")),
			TEXT("BlueprintEvent string result should verify")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
