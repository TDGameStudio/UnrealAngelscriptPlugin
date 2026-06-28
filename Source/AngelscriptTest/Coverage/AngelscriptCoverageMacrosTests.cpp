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
// Based on Documents/Coverage/Coverage_OtherMacros.md.
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
