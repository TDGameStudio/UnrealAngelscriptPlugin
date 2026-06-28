#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Containers/Set.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreNet.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageBoolPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript bool *UPROPERTY usage*.
//
// Bool is the simplest type:
//   - Only 1 type (bool)
//   - Only 2 values (true/false)
//   - No arithmetic operations
//   - No methods
//
// Test pattern: Pattern D (Actor + FProperty reflection)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageBoolPropertyTest,
	"Angelscript.TestModule.Coverage.BoolProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const FProperty* RequireProperty(UClass* ScriptClass, const TCHAR* PropertyName)
	{
		return ScriptClass != nullptr ? ScriptClass->FindPropertyByName(FName(PropertyName)) : nullptr;
	}

	static bool CompileFailureDiagnosticsContain(
		FAutomationTestBase& Test,
		const FAngelscriptCompileTraceSummary& Summary,
		const FString& ExpectedDiagnosticFragment,
		const TCHAR* CaseLabel)
	{
		bool bFoundFragment = false;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			Test.AddInfo(FString::Printf(
				TEXT("%s diagnostic: %s Row%d:Col%d %s"),
				CaseLabel,
				Diagnostic.bIsError ? TEXT("ERROR") : (Diagnostic.bIsInfo ? TEXT("INFO") : TEXT("WARN")),
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
			if (Diagnostic.bIsError && Diagnostic.Message.Contains(ExpectedDiagnosticFragment))
			{
				bFoundFragment = true;
			}
		}

		return bFoundFragment;
	}

	static FName ResolveReplicatedPropertyName(const UClass* OwnerClass, const FLifetimeProperty& LifetimeProperty)
	{
		for (TFieldIterator<FProperty> It(OwnerClass); It; ++It)
		{
			if (It->RepIndex == LifetimeProperty.RepIndex)
			{
				return It->GetFName();
			}
		}

		return NAME_None;
	}

	static TSet<FName> CollectReplicatedPropertyNames(
		const UClass* OwnerClass,
		const TArray<FLifetimeProperty>& LifetimeProperties)
	{
		TSet<FName> PropertyNames;
		for (const FLifetimeProperty& LifetimeProperty : LifetimeProperties)
		{
			const FName PropertyName = ResolveReplicatedPropertyName(OwnerClass, LifetimeProperty);
			if (PropertyName != NAME_None)
			{
				PropertyNames.Add(PropertyName);
			}
		}

		return PropertyNames;
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
	// Bool declaration defaults: true, false, no default.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolDefaultsActor : AActor
			{
				UPROPERTY()
				bool TrueValue = true;

				UPROPERTY()
				bool FalseValue = false;

				UPROPERTY()
				bool NoDefaultValue;
			}
			)AS"),
			TEXT("ACoverageBoolDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-defaults actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Bool-defaults actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		// true default
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TrueValue"), true, TEXT("bool UPROPERTY with true default"))));

		// false default
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FalseValue"), false, TEXT("bool UPROPERTY with false default"))));

		// no default (should be false)
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NoDefaultValue"), false, TEXT("bool UPROPERTY without default should be false"))));
	}

	// -------------------------------------------------------------------------
	// Bool write round-trip: SetByPath -> read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolWriteActor : AActor
			{
				UPROPERTY()
				bool BoolValue;
			}
			)AS"),
			TEXT("ACoverageBoolWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-write actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Bool-write actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		// Write true
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true)));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true, TEXT("bool write true round-trip"))));

		// Write false
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), false)));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), false, TEXT("bool write false round-trip"))));

		// Toggle multiple times
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true)));
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), false)));
		ASSERT_THAT(IsTrue(SetByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true)));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true, TEXT("bool multiple toggle"))));
	}

	// -------------------------------------------------------------------------
	// Bool containers: TArray, TMap, TSet.
	// Note: TSet<bool> can only have 0, 1, or 2 elements (true/false).
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolContainerActor : AActor
			{
				UPROPERTY()
				TArray<bool> BoolArray;

				UPROPERTY()
				TMap<int, bool> IntToBoolMap;

				UPROPERTY()
				TMap<bool, int> BoolToIntMap;

				UPROPERTY()
				TMap<FString, bool> StringToBoolMap;

				UPROPERTY()
				TSet<bool> BoolSet;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BoolArray.Add(true);
					BoolArray.Add(false);
					BoolArray.Add(true);

					IntToBoolMap.Add(1, true);
					IntToBoolMap.Add(2, false);

					BoolToIntMap.Add(true, 100);
					BoolToIntMap.Add(false, 200);

					StringToBoolMap.Add("Enabled", true);
					StringToBoolMap.Add("Hidden", false);

					BoolSet.Add(true);
					BoolSet.Add(false);
					BoolSet.Add(true);  // Duplicate
				}
			}
			)AS"),
			TEXT("ACoverageBoolContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-container actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Bool-container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// TArray<bool>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("BoolArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<bool> should have 3 elements")));

			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolArray[0]"), true, TEXT("TArray<bool>[0]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolArray[1]"), false, TEXT("TArray<bool>[1]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolArray[2]"), true, TEXT("TArray<bool>[2]"))));
		}

		// TMap<int, bool>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToBoolMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,bool> should have 2 entries")));

			bool Value = false;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FBoolProperty, bool>(*TestRunner, Actor, TEXT("IntToBoolMap"), 1, Value)));
			ASSERT_THAT(AreEqual(true, Value, TEXT("TMap<int,bool>[1] should be true")));
		}

		// TMap<bool, int> (only 2 possible keys)
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("BoolToIntMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<bool,int> should have 2 entries (max possible)")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<bool, FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolToIntMap"), true, Value)));
			ASSERT_THAT(AreEqual(100, Value, TEXT("TMap<bool,int>[true] should be 100")));

			ASSERT_THAT(IsTrue(GetMapValueByPath<bool, FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolToIntMap"), false, Value)));
			ASSERT_THAT(AreEqual(200, Value, TEXT("TMap<bool,int>[false] should be 200")));
		}

		// TMap<FString, bool>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToBoolMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,bool> should have 2 entries")));

			bool Value = false;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringToBoolMap"), FString(TEXT("Enabled")), Value)));
			ASSERT_THAT(AreEqual(true, Value, TEXT("TMap<FString,bool>[Enabled] should be true")));

			Value = true;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringToBoolMap"), FString(TEXT("Hidden")), Value)));
			ASSERT_THAT(AreEqual(false, Value, TEXT("TMap<FString,bool>[Hidden] should be false")));
		}

		// TSet<bool> (only 2 possible elements)
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("BoolSet"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<bool> should have 2 elements (max possible, deduplicated)")));

			bool bContainsTrue = SetContainsByPath<bool>(*TestRunner, Actor, TEXT("BoolSet"), true);
			bool bContainsFalse = SetContainsByPath<bool>(*TestRunner, Actor, TEXT("BoolSet"), false);
			ASSERT_THAT(IsTrue(bContainsTrue, TEXT("TSet<bool> should contain true")));
			ASSERT_THAT(IsTrue(bContainsFalse, TEXT("TSet<bool> should contain false")));
		}
	}

	// -------------------------------------------------------------------------
	// Bool nested containers: TArray<TArray<bool>> is rejected by this fork.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolNestedArrayProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_NestedArrayUnsupported"));
		static const TCHAR* CaseLabel = TEXT("TArray<TArray<bool>> should remain an explicit unsupported boundary");
		const FString ExpectedDiagnostic(TEXT("Attempting to instantiate invalid template type 'TArray<bool[]>': Containers cannot be nested in other containers"));

		FAngelscriptCompileTraceSummary Summary;
		CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			TEXT("ASCoverageBoolPropertyNestedArrayUnsupported.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolNestedArrayActor : AActor
			{
				UPROPERTY()
				TArray<TArray<bool>> Matrix;
			}
			)AS"),
			true,
			Summary,
			true);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		// Actual diagnostic: "Attempting to instantiate invalid template type 'TArray<bool[]>': Containers cannot be nested in other containers".
		// Reason: this fork keeps reflected container properties one level deep; model nested bool data through a struct or separate property.
		ASSERT_THAT(IsFalse(Summary.bCompileSucceeded, CaseLabel));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("nested bool container compile result should be Error")));
		ASSERT_THAT(IsTrue(CompileFailureDiagnosticsContain(*TestRunner, Summary, ExpectedDiagnostic, CaseLabel), TEXT("nested bool container diagnostics should contain the pinned rejection message")));
	}

	// -------------------------------------------------------------------------
	// Bool replicated properties: Replicated / ReplicatedUsing metadata and
	// lifetime list membership.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolReplicatedProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_Replication"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolPropertyReplication.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolReplicationActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(Replicated)
				bool bReplicatedFlag = true;

				UPROPERTY(ReplicatedUsing=OnRep_Ready)
				bool bReady = false;

				UFUNCTION()
				void OnRep_Ready()
				{
				}
			}
			)AS"),
			TEXT("ACoverageBoolReplicationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-replication actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FBoolProperty* ReplicatedFlagProperty = CastField<FBoolProperty>(RequireProperty(ScriptClass, TEXT("bReplicatedFlag")));
		const FBoolProperty* ReadyProperty = CastField<FBoolProperty>(RequireProperty(ScriptClass, TEXT("bReady")));
		ASSERT_THAT(IsNotNull(ReplicatedFlagProperty, TEXT("Replicated bool property should be generated as FBoolProperty")));
		ASSERT_THAT(IsNotNull(ReadyProperty, TEXT("RepNotify bool property should be generated as FBoolProperty")));
		if (ReplicatedFlagProperty == nullptr || ReadyProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReplicatedFlagProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("Replicated bool should carry CPF_Net")));
		ASSERT_THAT(IsFalse(ReplicatedFlagProperty->HasAnyPropertyFlags(CPF_RepNotify),
			TEXT("plain Replicated bool should not carry CPF_RepNotify")));
		ASSERT_THAT(IsTrue(ReadyProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("ReplicatedUsing bool should carry CPF_Net")));
		ASSERT_THAT(IsTrue(ReadyProperty->HasAnyPropertyFlags(CPF_RepNotify),
			TEXT("ReplicatedUsing bool should carry CPF_RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_Ready")),
			ReadyProperty->RepNotifyFunc,
			TEXT("ReplicatedUsing bool should preserve the RepNotify function name")));

		UFunction* OnRepFunction = FindGeneratedFunction(ScriptClass, TEXT("OnRep_Ready"));
		ASSERT_THAT(IsNotNull(OnRepFunction, TEXT("bool RepNotify callback should be generated")));

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ScriptASClass, TEXT("Bool-replication actor should be backed by UASClass")));
		if (ScriptASClass == nullptr)
		{
			return;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		ScriptASClass->GetLifetimeScriptReplicationList(LifetimeProperties);
		const TSet<FName> LifetimePropertyNames = CollectReplicatedPropertyNames(ScriptClass, LifetimeProperties);

		ASSERT_THAT(AreEqual(2, LifetimeProperties.Num(),
			TEXT("bool lifetime replication list should contain both script replicated bool properties")));
		ASSERT_THAT(AreEqual(2, LifetimePropertyNames.Num(),
			TEXT("bool lifetime replication entries should resolve to unique property names")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("bReplicatedFlag"))),
			TEXT("bool lifetime replication list should include bReplicatedFlag")));
		ASSERT_THAT(IsTrue(LifetimePropertyNames.Contains(FName(TEXT("bReady"))),
			TEXT("bool lifetime replication list should include bReady")));
	}

	// -------------------------------------------------------------------------
	// UPROPERTY specifier coverage for bool members: edit/visible/blueprint
	// flags plus bool-specific edit-condition metadata.
	// -------------------------------------------------------------------------
	TEST_METHOD(BoolPropertySpecifierFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageBoolProperty_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageBoolPropertySpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBoolSpecifierActor : AActor
			{
				UPROPERTY(EditAnywhere)
				bool EditAnywhereBool = true;

				UPROPERTY(EditDefaultsOnly)
				bool EditDefaultsOnlyBool = true;

				UPROPERTY(EditInstanceOnly)
				bool EditInstanceOnlyBool = true;

				UPROPERTY(NotEditable)
				bool NotEditableBool = true;

				UPROPERTY(EditConst)
				bool EditConstBool = true;

				UPROPERTY(VisibleAnywhere)
				bool VisibleAnywhereBool = true;

				UPROPERTY(BlueprintReadWrite)
				bool BlueprintReadWriteBool = true;

				UPROPERTY(BlueprintReadOnly)
				bool BlueprintReadOnlyBool = true;

				UPROPERTY(Transient)
				bool TransientBool = true;

				UPROPERTY(Config)
				bool ConfigBool = true;

				UPROPERTY(SaveGame)
				bool SaveGameBool = true;

				UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
				bool bOtherBool = true;

				UPROPERTY(EditAnywhere, meta = (EditCondition = "bOtherBool"))
				bool bFeatureValue = true;

				UPROPERTY(meta = (DisplayName = "Enable Feature"))
				bool bDisplayNamed = true;

				UPROPERTY(Category = "BoolCoverage")
				bool CategorizedBool = true;

				UPROPERTY(EditAnywhere, BlueprintReadOnly)
				bool EditableReadOnlyBool = true;
			}
			)AS"),
			TEXT("ACoverageBoolSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Bool-specifier actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		auto CheckFlag = [&](const TCHAR* Name, EPropertyFlags Flag, bool bExpected, const TCHAR* Label)
		{
			const FProperty* Found = RequireProperty(ScriptClass, Name);
			ASSERT_THAT(IsNotNull(Found, *FString::Printf(TEXT("Bool specifier property '%s' should exist"), Name)));
			if (Found == nullptr)
			{
				return;
			}

			if (bExpected)
			{
				ASSERT_THAT(IsTrue(Found->HasAnyPropertyFlags(Flag), Label));
			}
			else
			{
				ASSERT_THAT(IsFalse(Found->HasAnyPropertyFlags(Flag), Label));
			}
		};

		CheckFlag(TEXT("EditAnywhereBool"), CPF_Edit, true, TEXT("EditAnywhere bool -> CPF_Edit"));
		CheckFlag(TEXT("EditAnywhereBool"), CPF_DisableEditOnInstance, false, TEXT("EditAnywhere bool -> editable on instance"));
		CheckFlag(TEXT("EditAnywhereBool"), CPF_DisableEditOnTemplate, false, TEXT("EditAnywhere bool -> editable on defaults"));

		CheckFlag(TEXT("EditDefaultsOnlyBool"), CPF_Edit, true, TEXT("EditDefaultsOnly bool -> CPF_Edit"));
		CheckFlag(TEXT("EditDefaultsOnlyBool"), CPF_DisableEditOnInstance, true, TEXT("EditDefaultsOnly bool -> disabled on instance"));
		CheckFlag(TEXT("EditDefaultsOnlyBool"), CPF_DisableEditOnTemplate, false, TEXT("EditDefaultsOnly bool -> editable on defaults"));

		CheckFlag(TEXT("EditInstanceOnlyBool"), CPF_Edit, true, TEXT("EditInstanceOnly bool -> CPF_Edit"));
		CheckFlag(TEXT("EditInstanceOnlyBool"), CPF_DisableEditOnTemplate, true, TEXT("EditInstanceOnly bool -> disabled on defaults"));
		CheckFlag(TEXT("EditInstanceOnlyBool"), CPF_DisableEditOnInstance, false, TEXT("EditInstanceOnly bool -> editable on instance"));

		CheckFlag(TEXT("NotEditableBool"), CPF_Edit, false, TEXT("NotEditable bool -> clears CPF_Edit"));

		CheckFlag(TEXT("EditConstBool"), CPF_Edit, true, TEXT("EditConst bool keeps default CPF_Edit"));
		CheckFlag(TEXT("EditConstBool"), CPF_EditConst, true, TEXT("EditConst bool -> CPF_EditConst"));

		CheckFlag(TEXT("VisibleAnywhereBool"), CPF_Edit, true, TEXT("VisibleAnywhere bool -> CPF_Edit"));
		CheckFlag(TEXT("VisibleAnywhereBool"), CPF_EditConst, true, TEXT("VisibleAnywhere bool -> CPF_EditConst"));
		CheckFlag(TEXT("VisibleAnywhereBool"), CPF_DisableEditOnInstance, false, TEXT("VisibleAnywhere bool -> visible on instance"));
		CheckFlag(TEXT("VisibleAnywhereBool"), CPF_DisableEditOnTemplate, false, TEXT("VisibleAnywhere bool -> visible on defaults"));

		CheckFlag(TEXT("BlueprintReadWriteBool"), CPF_BlueprintVisible, true, TEXT("BlueprintReadWrite bool -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("BlueprintReadWriteBool"), CPF_BlueprintReadOnly, false, TEXT("BlueprintReadWrite bool -> not read-only"));

		CheckFlag(TEXT("BlueprintReadOnlyBool"), CPF_BlueprintVisible, true, TEXT("BlueprintReadOnly bool -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("BlueprintReadOnlyBool"), CPF_BlueprintReadOnly, true, TEXT("BlueprintReadOnly bool -> CPF_BlueprintReadOnly"));

		CheckFlag(TEXT("TransientBool"), CPF_Transient, true, TEXT("Transient bool -> CPF_Transient"));
		CheckFlag(TEXT("ConfigBool"), CPF_Config, true, TEXT("Config bool -> CPF_Config"));
		CheckFlag(TEXT("SaveGameBool"), CPF_SaveGame, true, TEXT("SaveGame bool -> CPF_SaveGame"));

		CheckFlag(TEXT("EditableReadOnlyBool"), CPF_Edit, true, TEXT("EditAnywhere+BlueprintReadOnly bool -> CPF_Edit"));
		CheckFlag(TEXT("EditableReadOnlyBool"), CPF_BlueprintVisible, true, TEXT("EditAnywhere+BlueprintReadOnly bool -> CPF_BlueprintVisible"));
		CheckFlag(TEXT("EditableReadOnlyBool"), CPF_BlueprintReadOnly, true, TEXT("EditAnywhere+BlueprintReadOnly bool -> CPF_BlueprintReadOnly"));

#if WITH_EDITOR
		const FProperty* InlineToggle = RequireProperty(ScriptClass, TEXT("bOtherBool"));
		ASSERT_THAT(IsNotNull(InlineToggle, TEXT("bOtherBool property should exist")));
		if (InlineToggle == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			InlineToggle->HasMetaData(TEXT("InlineEditConditionToggle")),
			TEXT("InlineEditConditionToggle bool meta should be present")));
		ASSERT_THAT(IsTrue(
			InlineToggle->IsA<FBoolProperty>(),
			TEXT("InlineEditConditionToggle should be attached to a bool property")));

		const FProperty* EditCondition = RequireProperty(ScriptClass, TEXT("bFeatureValue"));
		ASSERT_THAT(IsNotNull(EditCondition, TEXT("bFeatureValue property should exist")));
		if (EditCondition == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FString(TEXT("bOtherBool")),
			EditCondition->GetMetaData(TEXT("EditCondition")),
			TEXT("EditCondition bool meta should round-trip")));
		ASSERT_THAT(IsTrue(
			EditCondition->IsA<FBoolProperty>(),
			TEXT("EditCondition=\"bOtherBool\" coverage target should be a bool property")));

		const FProperty* DisplayNamed = RequireProperty(ScriptClass, TEXT("bDisplayNamed"));
		ASSERT_THAT(IsNotNull(DisplayNamed, TEXT("bDisplayNamed property should exist")));
		if (DisplayNamed == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FString(TEXT("Enable Feature")),
			DisplayNamed->GetMetaData(TEXT("DisplayName")),
			TEXT("DisplayName bool meta should round-trip")));
		ASSERT_THAT(IsTrue(
			DisplayNamed->IsA<FBoolProperty>(),
			TEXT("DisplayName=\"Enable Feature\" coverage target should be a bool property")));

		const FProperty* Categorized = RequireProperty(ScriptClass, TEXT("CategorizedBool"));
		ASSERT_THAT(IsNotNull(Categorized, TEXT("CategorizedBool property should exist")));
		if (Categorized == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FString(TEXT("BoolCoverage")),
			Categorized->GetMetaData(TEXT("Category")),
			TEXT("Category bool meta should round-trip")));
#endif
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
