#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestWorld.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/ScriptDelegates.h"
#include "UObject/UnrealType.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadDelegateRuntimeTests,
	"Angelscript.TestModule.HotReload.Delegate.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName BlueprintDelegateModuleName = FName(TEXT("HotReloadDelegateRuntimeBlueprint"));
	inline static const FString BlueprintDelegateFilename = FString(TEXT("HotReloadDelegateRuntimeBlueprint.as"));
	inline static const FName BlueprintDelegateClassName = FName(TEXT("AHotReloadDelegateRuntimeBlueprintParent"));

	inline static const FName GlobalDelegateModuleName = FName(TEXT("HotReloadDelegateRuntimeGlobal"));
	inline static const FString GlobalDelegateFilename = FString(TEXT("HotReloadDelegateRuntimeGlobal.as"));

	inline static const FName TickDelegateModuleName = FName(TEXT("HotReloadDelegateRuntimeTick"));
	inline static const FString TickDelegateFilename = FString(TEXT("HotReloadDelegateRuntimeTick.as"));
	inline static const FName TickDelegateClassName = FName(TEXT("AHotReloadDelegateRuntimeTickParent"));

	struct FScopedTransientBlueprint
	{
		UBlueprint* Blueprint = nullptr;

		~FScopedTransientBlueprint()
		{
			Cleanup();
		}

		bool CreateAndCompile(FAutomationTestBase& Test, UClass* ParentClass, FStringView Suffix)
		{
			if (!CheckNotNull(Test, ParentClass, TEXT("Delegate runtime hot reload should have a valid Blueprint parent class")))
			{
				return false;
			}

			const FString PackagePath = FString::Printf(
				TEXT("/Temp/AngelscriptHotReloadDelegateRuntime_%.*s_%s"),
				Suffix.Len(),
				Suffix.GetData(),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			UPackage* BlueprintPackage = CreatePackage(*PackagePath);
			if (!CheckNotNull(Test, BlueprintPackage, TEXT("Delegate runtime hot reload should create a transient Blueprint package")))
			{
				return false;
			}

			BlueprintPackage->SetFlags(RF_Transient);
			const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

			Blueprint = FKismetEditorUtilities::CreateBlueprint(
				ParentClass,
				BlueprintPackage,
				BlueprintName,
				BPTYPE_Normal,
				UBlueprint::StaticClass(),
				UBlueprintGeneratedClass::StaticClass(),
				TEXT("AngelscriptHotReloadDelegateRuntimeTests"));
			if (!CheckNotNull(Test, Blueprint, TEXT("Delegate runtime hot reload should create a transient Blueprint asset")))
			{
				return false;
			}

			return Compile(Test);
		}

		bool Compile(FAutomationTestBase& Test)
		{
			if (!CheckNotNull(Test, Blueprint, TEXT("Delegate runtime hot reload should have a Blueprint asset to compile")))
			{
				return false;
			}

			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			return CheckNotNull(Test, Blueprint->GeneratedClass.Get(), TEXT("Delegate runtime hot reload should compile a generated Blueprint class"));
		}

		UClass* GetGeneratedClass() const
		{
			return Blueprint != nullptr ? Blueprint->GeneratedClass.Get() : nullptr;
		}

		void Cleanup()
		{
			if (Blueprint == nullptr)
			{
				return;
			}

			if (UClass* BlueprintClass = Blueprint->GeneratedClass)
			{
				BlueprintClass->MarkAsGarbage();
			}

			if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
			{
				BlueprintPackage->MarkAsGarbage();
			}

			Blueprint->MarkAsGarbage();
			CollectGarbage(RF_NoFlags, true);
			Blueprint = nullptr;
		}

	private:
		template <typename ValueType>
		static bool CheckNotNull(FAutomationTestBase& Test, const ValueType& Value, const TCHAR* Message)
		{
			FNoDiscardAsserter LocalAssert(Test);
			return LocalAssert.IsNotNull(Value, Message);
		}
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	template <typename ValueType>
	static bool ExpectEqual(FAutomationTestBase& Test, const ValueType& Expected, const ValueType& Actual, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.AreEqual(Expected, Actual, Message);
	}

	static bool ExpectTrue(FAutomationTestBase& Test, const bool bValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(bValue, Message);
	}

	template <typename ValueType>
	static bool ExpectNotNull(FAutomationTestBase& Test, const ValueType& Value, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNotNull(Value, Message);
	}

	static bool CompileReload(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		ECompileType CompileType,
		FName ModuleName,
		const FString& Filename,
		const FString& Source,
		const TCHAR* Context,
		ECompileResult* OutCompileResult = nullptr)
	{
		ECompileResult ReloadResult = ECompileResult::Error;
		const FString CompileMessage = FString::Printf(TEXT("%s should compile"), Context);
		if (!ExpectTrue(
				Test,
				CompileModuleWithResult(&Engine, CompileType, ModuleName, Filename, Source, ReloadResult),
				*CompileMessage))
		{
			return false;
		}

		if (OutCompileResult != nullptr)
		{
			*OutCompileResult = ReloadResult;
		}

		const FString ReloadMessage = FString::Printf(TEXT("%s should finish on a handled reload path"), Context);
		return ExpectTrue(Test, IsHandledReloadResult(ReloadResult), *ReloadMessage);
	}

	static bool RunDelegateAndExpect(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		AActor& Actor,
		const int32 Value,
		const int32 ExpectedResult,
		const TCHAR* Context)
	{
		FFunctionInvoker Invoker(Test, &Actor, FName(TEXT("RunDelegate")));
		if (!Invoker.IsValid())
		{
			return false;
		}

		const int32 Result = Invoker
			.AddParam<int32>(Value)
			.CallAndReturn<int32>(INDEX_NONE);
		const FString Message = FString::Printf(TEXT("%s should return expected delegate result"), Context);
		return ExpectEqual(Test, ExpectedResult, Result, *Message);
	}

	static bool ReadIntProperty(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		int32& OutValue,
		const TCHAR* Context)
	{
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, PropertyName, OutValue))
		{
			Test.AddError(FString::Printf(TEXT("%s should read property '%s'"), Context, *PropertyName.ToString()));
			return false;
		}

		return true;
	}

	static bool ValidateDelegateProperty(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		const TCHAR* Context,
		const bool bExpectEdit,
		const bool bExpectBlueprintVisible,
		UFunction* ExpectedSignature)
	{
		FDelegateProperty* DelegateProperty = OwnerClass != nullptr ? FindFProperty<FDelegateProperty>(OwnerClass, TEXT("OnCompute")) : nullptr;
		const FString PropertyMessage = FString::Printf(TEXT("%s should expose OnCompute delegate property"), Context);
		if (!ExpectNotNull(Test, DelegateProperty, *PropertyMessage))
		{
			return false;
		}

		bool bPassed = true;
		const FString EditMessage = FString::Printf(TEXT("%s should expose expected CPF_Edit state"), Context);
		bPassed &= ExpectEqual(Test, bExpectEdit, DelegateProperty->HasAnyPropertyFlags(CPF_Edit), *EditMessage);
		const FString BlueprintVisibleMessage = FString::Printf(TEXT("%s should expose expected CPF_BlueprintVisible state"), Context);
		bPassed &= ExpectEqual(Test, bExpectBlueprintVisible, DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), *BlueprintVisibleMessage);
		const FString SignatureMessage = FString::Printf(TEXT("%s should point to the expected delegate signature"), Context);
		bPassed &= ExpectEqual(Test, ExpectedSignature, DelegateProperty->SignatureFunction.Get(), *SignatureMessage);
		return bPassed;
	}

	static asIScriptModule* FindScriptModule(FAngelscriptEngine& Engine, FName ModuleName)
	{
		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModule(ModuleName.ToString());
		return ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	}

	static void EnableActorTick(AActor& Actor)
	{
		Actor.PrimaryActorTick.bCanEverTick = true;
		Actor.SetActorTickEnabled(true);
		Actor.RegisterAllActorTickFunctions(true, false);
	}

public:
	TEST_METHOD(BlueprintDelegatePropertyReloadsAfterInstanceRuntime)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{ FAngelscriptEngineScope EngineScope(Engine);
		FScopedTransientBlueprint Blueprint;
		ON_SCOPE_EXIT
		{
			Blueprint.Cleanup();
			Engine.DiscardModule(*BlueprintDelegateModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		// Script under test: V1 keeps the delegate property non-editable and uses one integer argument.
		const FString ReloadV1Source = ASTEST_AS(R"AS(
			delegate int FHotReloadRuntimeCompute(int Value);

			UCLASS()
			class AHotReloadDelegateRuntimeBlueprintParent : AActor
			{
				UPROPERTY(NotEditable)
				FHotReloadRuntimeCompute OnCompute;

				UPROPERTY()
				int LastValue = 0;

				UPROPERTY()
				int BeginPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
					OnCompute.BindUFunction(this, n"HandleCompute");
				}

				UFUNCTION()
				int HandleCompute(int Value)
				{
					LastValue = Value;
					return Value + 1;
				}

				UFUNCTION()
				int RunDelegate(int Value)
				{
					return OnCompute.Execute(Value);
				}
			}
			)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			BlueprintDelegateModuleName,
			BlueprintDelegateFilename,
			ReloadV1Source,
			BlueprintDelegateClassName);
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("Delegate runtime hot reload should compile the V1 parent class")));

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("Delegate runtime hot reload should start from an AS parent class")));

		const TSharedPtr<FAngelscriptDelegateDesc> DelegateBeforeReload = Engine.GetDelegate(TEXT("FHotReloadRuntimeCompute"));
		ASSERT_THAT(IsTrue(DelegateBeforeReload.IsValid(), TEXT("Delegate runtime hot reload should expose V1 delegate metadata")));
		ASSERT_THAT(IsNotNull(DelegateBeforeReload->Function, TEXT("Delegate runtime hot reload should expose V1 delegate signature")));
		ASSERT_THAT(IsTrue(ValidateDelegateProperty(
			*TestRunner,
			ParentClass,
			TEXT("Delegate runtime hot reload V1 parent"),
			false,
			true,
			DelegateBeforeReload->Function)));

		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ParentClass, TEXT("DelegateRuntime"))));
		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Delegate runtime hot reload should expose a Blueprint generated class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentClass), TEXT("Delegate runtime hot reload Blueprint should inherit from V1 parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(BlueprintClass), TEXT("Delegate runtime hot reload Blueprint generated class should be a regular Blueprint class")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(BlueprintClass), TEXT("Delegate runtime hot reload should resolve V1 AS parent through Blueprint child")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Delegate runtime hot reload should spawn a Blueprint child actor")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		ASSERT_THAT(IsTrue(RunDelegateAndExpect(
			*TestRunner,
			Engine,
			*BlueprintActor,
			40,
			41,
			TEXT("Delegate runtime hot reload V1 Blueprint actor"))));

		int32 BeginPlayCountBeforeReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("BeginPlayCount"),
			BeginPlayCountBeforeReload,
			TEXT("Delegate runtime hot reload V1 Blueprint actor"))));
		ASSERT_THAT(AreEqual(1, BeginPlayCountBeforeReload, TEXT("Delegate runtime hot reload should begin play once before reload")));

		// Script under test: V2 changes only the handler body, keeping the property definition soft-reloadable.
		const FString ReloadV2Source = ASTEST_AS(R"AS(
			delegate int FHotReloadRuntimeCompute(int Value);

			UCLASS()
			class AHotReloadDelegateRuntimeBlueprintParent : AActor
			{
				UPROPERTY(NotEditable)
				FHotReloadRuntimeCompute OnCompute;

				UPROPERTY()
				int LastValue = 0;

				UPROPERTY()
				int BeginPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
					OnCompute.BindUFunction(this, n"HandleCompute");
				}

				UFUNCTION()
				int HandleCompute(int Value)
				{
					LastValue = Value * 2;
					return LastValue + 3;
				}

				UFUNCTION()
				int RunDelegate(int Value)
				{
					return OnCompute.Execute(Value);
				}
			}
			)AS");

		ECompileResult SoftReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileReload(
			*TestRunner,
			Engine,
			ECompileType::SoftReloadOnly,
			BlueprintDelegateModuleName,
			BlueprintDelegateFilename,
			ReloadV2Source,
			TEXT("Delegate runtime hot reload soft implementation update"),
			&SoftReloadResult)));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, SoftReloadResult, TEXT("Delegate runtime hot reload V2 should stay on the soft reload path")));

		UClass* SoftReloadedParentClass = FindGeneratedClass(&Engine, BlueprintDelegateClassName);
		ASSERT_THAT(IsNotNull(SoftReloadedParentClass, TEXT("Delegate runtime hot reload should resolve parent after V2 reload")));
		ASSERT_THAT(AreEqual(ParentClass, SoftReloadedParentClass, TEXT("Delegate runtime hot reload V2 should preserve parent class identity")));

		const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterSoftReload = Engine.GetDelegate(TEXT("FHotReloadRuntimeCompute"));
		ASSERT_THAT(IsTrue(DelegateAfterSoftReload.IsValid(), TEXT("Delegate runtime hot reload should keep delegate metadata after V2 reload")));
		ASSERT_THAT(IsTrue(ValidateDelegateProperty(
			*TestRunner,
			SoftReloadedParentClass,
			TEXT("Delegate runtime hot reload V2 parent"),
			false,
			true,
			DelegateAfterSoftReload->Function)));

		ASSERT_THAT(IsTrue(RunDelegateAndExpect(
			*TestRunner,
			Engine,
			*BlueprintActor,
			10,
			23,
			TEXT("Delegate runtime hot reload V2 existing Blueprint actor"))));

		int32 LastValueAfterSoftReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("LastValue"),
			LastValueAfterSoftReload,
			TEXT("Delegate runtime hot reload V2 existing Blueprint actor"))));
		ASSERT_THAT(AreEqual(20, LastValueAfterSoftReload, TEXT("Delegate runtime hot reload should run the V2 delegate handler body")));

		int32 BeginPlayCountAfterSoftReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("BeginPlayCount"),
			BeginPlayCountAfterSoftReload,
			TEXT("Delegate runtime hot reload V2 existing Blueprint actor"))));
		ASSERT_THAT(AreEqual(1, BeginPlayCountAfterSoftReload, TEXT("Delegate runtime hot reload should not replay BeginPlay during soft reload")));

		// Script under test: V3 switches UPROPERTY flags while keeping the delegate signature stable.
		const FString ReloadV3Source = ASTEST_AS(R"AS(
			delegate int FHotReloadRuntimeCompute(int Value);

			UCLASS()
			class AHotReloadDelegateRuntimeBlueprintParent : AActor
			{
				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FHotReloadRuntimeCompute OnCompute;

				UPROPERTY()
				int LastValue = 0;

				UPROPERTY()
				int BeginPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
					OnCompute.BindUFunction(this, n"HandleCompute");
				}

				UFUNCTION()
				int HandleCompute(int Value)
				{
					LastValue = Value * 3;
					return LastValue + 4;
				}

				UFUNCTION()
				int RunDelegate(int Value)
				{
					return OnCompute.Execute(Value);
				}
			}
			)AS");

		ECompileResult FlagReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileReload(
			*TestRunner,
			Engine,
			ECompileType::FullReload,
			BlueprintDelegateModuleName,
			BlueprintDelegateFilename,
			ReloadV3Source,
			TEXT("Delegate runtime hot reload full property-flag update"),
			&FlagReloadResult)));

		UClass* FlagReloadedParentClass = FindGeneratedClass(&Engine, BlueprintDelegateClassName);
		ASSERT_THAT(IsNotNull(FlagReloadedParentClass, TEXT("Delegate runtime hot reload should resolve parent after property-flag reload")));
		ASSERT_THAT(IsTrue(FlagReloadedParentClass != SoftReloadedParentClass, TEXT("Delegate runtime hot reload property-flag change should replace the parent class")));

		UASClass* FlagReloadedASClass = Cast<UASClass>(FlagReloadedParentClass);
		ASSERT_THAT(IsNotNull(FlagReloadedASClass, TEXT("Delegate runtime hot reload property-flag parent should still be UASClass")));
		ASSERT_THAT(AreEqual(FlagReloadedParentClass, ParentASClass->GetMostUpToDateClass(), TEXT("Delegate runtime hot reload should chain V1 parent to the property-flag class")));

		const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterFlagReload = Engine.GetDelegate(TEXT("FHotReloadRuntimeCompute"));
		ASSERT_THAT(IsTrue(DelegateAfterFlagReload.IsValid(), TEXT("Delegate runtime hot reload should keep delegate metadata after property-flag reload")));
		ASSERT_THAT(IsTrue(ValidateDelegateProperty(
			*TestRunner,
			FlagReloadedParentClass,
			TEXT("Delegate runtime hot reload V3 parent"),
			true,
			true,
			DelegateAfterFlagReload->Function)));

		ASSERT_THAT(IsTrue(Blueprint.Compile(*TestRunner), TEXT("Delegate runtime hot reload should recompile the Blueprint after property-flag reload")));
		UClass* FlagReloadedBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(FlagReloadedBlueprintClass, TEXT("Delegate runtime hot reload should expose Blueprint class after property-flag reload")));
		UASClass* FlagReloadedBlueprintASParent = UASClass::GetFirstASClass(FlagReloadedBlueprintClass);
		ASSERT_THAT(IsNotNull(FlagReloadedBlueprintASParent, TEXT("Delegate runtime hot reload Blueprint parent chain should resolve after property-flag reload")));
		ASSERT_THAT(AreEqual(FlagReloadedParentClass, FlagReloadedBlueprintASParent->GetMostUpToDateClass(), TEXT("Delegate runtime hot reload Blueprint parent chain should resolve to the property-flag parent")));

		AActor* FlagReloadedBlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, FlagReloadedBlueprintClass);
		ASSERT_THAT(IsNotNull(FlagReloadedBlueprintActor, TEXT("Delegate runtime hot reload should spawn a Blueprint child after property-flag reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *FlagReloadedBlueprintActor);

		ASSERT_THAT(IsTrue(RunDelegateAndExpect(
			*TestRunner,
			Engine,
			*FlagReloadedBlueprintActor,
			8,
			28,
			TEXT("Delegate runtime hot reload V3 fresh Blueprint actor"))));

		int32 LastValueAfterFlagReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			FlagReloadedBlueprintActor,
			TEXT("LastValue"),
			LastValueAfterFlagReload,
			TEXT("Delegate runtime hot reload V3 fresh Blueprint actor"))));
		ASSERT_THAT(AreEqual(24, LastValueAfterFlagReload, TEXT("Delegate runtime hot reload should execute the property-flag version handler body")));

		// Script under test: V4 changes the delegate signature and rebinds a two-argument handler.
		const FString ReloadV4Source = ASTEST_AS(R"AS(
			delegate int FHotReloadRuntimeCompute(int Value, int Bonus);

			UCLASS()
			class AHotReloadDelegateRuntimeBlueprintParent : AActor
			{
				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FHotReloadRuntimeCompute OnCompute;

				UPROPERTY()
				int LastValue = 0;

				UPROPERTY()
				int BeginPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
					OnCompute.BindUFunction(this, n"HandleCompute");
				}

				UFUNCTION()
				int HandleCompute(int Value, int Bonus)
				{
					LastValue = Value + Bonus;
					return LastValue + 5;
				}

				UFUNCTION()
				int RunDelegate(int Value)
				{
					if (!OnCompute.IsBound())
					{
						OnCompute.BindUFunction(this, n"HandleCompute");
					}

					return OnCompute.Execute(Value, 7);
				}
			}
			)AS");

		ECompileResult SignatureReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileReload(
			*TestRunner,
			Engine,
			ECompileType::FullReload,
			BlueprintDelegateModuleName,
			BlueprintDelegateFilename,
			ReloadV4Source,
			TEXT("Delegate runtime hot reload full signature update"),
			&SignatureReloadResult)));

		UClass* SignatureReloadedParentClass = FindGeneratedClass(&Engine, BlueprintDelegateClassName);
		ASSERT_THAT(IsNotNull(SignatureReloadedParentClass, TEXT("Delegate runtime hot reload should resolve parent after signature reload")));
		ASSERT_THAT(IsTrue(SignatureReloadedParentClass != FlagReloadedParentClass, TEXT("Delegate runtime hot reload signature change should replace the parent class")));

		UASClass* SignatureReloadedASClass = Cast<UASClass>(SignatureReloadedParentClass);
		ASSERT_THAT(IsNotNull(SignatureReloadedASClass, TEXT("Delegate runtime hot reload final parent should still be UASClass")));
		ASSERT_THAT(AreEqual(SignatureReloadedParentClass, ParentASClass->GetMostUpToDateClass(), TEXT("Delegate runtime hot reload should chain V1 parent to signature-reloaded class")));

		const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterSignatureReload = Engine.GetDelegate(TEXT("FHotReloadRuntimeCompute"));
		ASSERT_THAT(IsTrue(DelegateAfterSignatureReload.IsValid(), TEXT("Delegate runtime hot reload should expose delegate metadata after signature reload")));
		ASSERT_THAT(IsNotNull(FindFProperty<FProperty>(DelegateAfterSignatureReload->Function, TEXT("Bonus")), TEXT("Delegate runtime hot reload signature should expose the new Bonus parameter")));
		ASSERT_THAT(IsTrue(ValidateDelegateProperty(
			*TestRunner,
			SignatureReloadedParentClass,
			TEXT("Delegate runtime hot reload V4 parent"),
			true,
			true,
			DelegateAfterSignatureReload->Function)));

		ASSERT_THAT(IsTrue(Blueprint.Compile(*TestRunner), TEXT("Delegate runtime hot reload should recompile the Blueprint after signature reload")));
		UClass* SignatureReloadedBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(SignatureReloadedBlueprintClass, TEXT("Delegate runtime hot reload should expose Blueprint class after signature reload")));
		UASClass* SignatureReloadedBlueprintASParent = UASClass::GetFirstASClass(SignatureReloadedBlueprintClass);
		ASSERT_THAT(IsNotNull(SignatureReloadedBlueprintASParent, TEXT("Delegate runtime hot reload Blueprint parent chain should resolve after signature reload")));
		ASSERT_THAT(AreEqual(SignatureReloadedParentClass, SignatureReloadedBlueprintASParent->GetMostUpToDateClass(), TEXT("Delegate runtime hot reload Blueprint parent chain should resolve to the signature-reloaded parent")));

		AActor* SignatureReloadedBlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, SignatureReloadedBlueprintClass);
		ASSERT_THAT(IsNotNull(SignatureReloadedBlueprintActor, TEXT("Delegate runtime hot reload should spawn a Blueprint child after signature reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *SignatureReloadedBlueprintActor);

		ASSERT_THAT(IsTrue(RunDelegateAndExpect(
			*TestRunner,
			Engine,
			*SignatureReloadedBlueprintActor,
			30,
			42,
			TEXT("Delegate runtime hot reload V4 fresh Blueprint actor"))));

		int32 LastValueAfterSignatureReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			SignatureReloadedBlueprintActor,
			TEXT("LastValue"),
			LastValueAfterSignatureReload,
			TEXT("Delegate runtime hot reload V4 fresh Blueprint actor"))));
		ASSERT_THAT(AreEqual(37, LastValueAfterSignatureReload, TEXT("Delegate runtime hot reload should execute the two-argument delegate handler")));
		}
	}

	TEST_METHOD(GlobalDelegateCallerRunsAcrossReloads)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{ FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*GlobalDelegateModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		// Script under test: V1 invokes a one-argument delegate through a global function.
		const FString GlobalReloadV1Source = ASTEST_AS(R"AS(
			delegate int FHotReloadGlobalCompute(int Value);

			UCLASS()
			class UHotReloadGlobalDelegateReceiver : UObject
			{
				UFUNCTION()
				int HandleCompute(int Value)
				{
					return Value + 2;
				}
			}

			int RunGlobal(UHotReloadGlobalDelegateReceiver Receiver, int Value)
			{
				FHotReloadGlobalCompute Compute;
				Compute.BindUFunction(Receiver, n"HandleCompute");
				return Compute.Execute(Value);
			}
			)AS");

		UClass* ReceiverClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			GlobalDelegateModuleName,
			GlobalDelegateFilename,
			GlobalReloadV1Source,
			TEXT("UHotReloadGlobalDelegateReceiver"));
		ASSERT_THAT(IsNotNull(ReceiverClass, TEXT("Delegate global hot reload should compile the V1 receiver class")));

		UObject* Receiver = NewObject<UObject>(GetTransientPackage(), ReceiverClass);
		ASSERT_THAT(IsNotNull(Receiver, TEXT("Delegate global hot reload should create a V1 receiver object")));

		asIScriptModule* Module = FindScriptModule(Engine, GlobalDelegateModuleName);
		ASSERT_THAT(IsNotNull(Module, TEXT("Delegate global hot reload should expose the V1 script module")));
		{
			FAngelscriptTestExecutor Executor(
				*TestRunner,
				Engine,
				*Module,
				TEXT("int RunGlobal(UHotReloadGlobalDelegateReceiver, int)"));
			ASSERT_THAT(IsTrue(Executor.IsValid(), TEXT("Delegate global hot reload should resolve V1 RunGlobal")));
			const int32 Result = Executor
				.AddArgObject(Receiver)
				.AddArg(static_cast<int32>(5))
				.ExecuteAndGet<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(7, Result, TEXT("Delegate global hot reload V1 global function should execute the delegate")));
		}

		// Script under test: V2 changes only the handler body and reuses the same global delegate path.
		const FString GlobalReloadV2Source = ASTEST_AS(R"AS(
			delegate int FHotReloadGlobalCompute(int Value);

			UCLASS()
			class UHotReloadGlobalDelegateReceiver : UObject
			{
				UFUNCTION()
				int HandleCompute(int Value)
				{
					return Value * 3;
				}
			}

			int RunGlobal(UHotReloadGlobalDelegateReceiver Receiver, int Value)
			{
				FHotReloadGlobalCompute Compute;
				Compute.BindUFunction(Receiver, n"HandleCompute");
				return Compute.Execute(Value);
			}
			)AS");

		ECompileResult BodyReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileReload(
			*TestRunner,
			Engine,
			ECompileType::SoftReloadOnly,
			GlobalDelegateModuleName,
			GlobalDelegateFilename,
			GlobalReloadV2Source,
			TEXT("Delegate global hot reload body update"),
			&BodyReloadResult)));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, BodyReloadResult, TEXT("Delegate global hot reload V2 should stay on the soft reload path")));

		Module = FindScriptModule(Engine, GlobalDelegateModuleName);
		ASSERT_THAT(IsNotNull(Module, TEXT("Delegate global hot reload should expose the V2 script module")));
		{
			FAngelscriptTestExecutor Executor(
				*TestRunner,
				Engine,
				*Module,
				TEXT("int RunGlobal(UHotReloadGlobalDelegateReceiver, int)"));
			ASSERT_THAT(IsTrue(Executor.IsValid(), TEXT("Delegate global hot reload should resolve V2 RunGlobal")));
			const int32 Result = Executor
				.AddArgObject(Receiver)
				.AddArg(static_cast<int32>(6))
				.ExecuteAndGet<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(18, Result, TEXT("Delegate global hot reload V2 global function should execute updated delegate body")));
		}

		// Script under test: V3 changes delegate and global function parameter shape.
		const FString GlobalReloadV3Source = ASTEST_AS(R"AS(
			delegate int FHotReloadGlobalCompute(int Value, int Bonus);

			UCLASS()
			class UHotReloadGlobalDelegateReceiver : UObject
			{
				UFUNCTION()
				int HandleCompute(int Value, int Bonus)
				{
					return Value + Bonus + 4;
				}
			}

			int RunGlobal(UHotReloadGlobalDelegateReceiver Receiver, int Value, int Bonus)
			{
				FHotReloadGlobalCompute Compute;
				Compute.BindUFunction(Receiver, n"HandleCompute");
				return Compute.Execute(Value, Bonus);
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileReload(
			*TestRunner,
			Engine,
			ECompileType::FullReload,
			GlobalDelegateModuleName,
			GlobalDelegateFilename,
			GlobalReloadV3Source,
			TEXT("Delegate global hot reload signature update"))));

		UClass* ReloadedReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadGlobalDelegateReceiver"));
		ASSERT_THAT(IsNotNull(ReloadedReceiverClass, TEXT("Delegate global hot reload should resolve V3 receiver class")));
		UObject* ReloadedReceiver = NewObject<UObject>(GetTransientPackage(), ReloadedReceiverClass);
		ASSERT_THAT(IsNotNull(ReloadedReceiver, TEXT("Delegate global hot reload should create a V3 receiver object")));

		const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterSignatureReload = Engine.GetDelegate(TEXT("FHotReloadGlobalCompute"));
		ASSERT_THAT(IsTrue(DelegateAfterSignatureReload.IsValid(), TEXT("Delegate global hot reload should expose V3 delegate metadata")));
		ASSERT_THAT(IsNotNull(FindFProperty<FProperty>(DelegateAfterSignatureReload->Function, TEXT("Bonus")), TEXT("Delegate global hot reload V3 signature should expose Bonus")));

		Module = FindScriptModule(Engine, GlobalDelegateModuleName);
		ASSERT_THAT(IsNotNull(Module, TEXT("Delegate global hot reload should expose the V3 script module")));
		{
			FAngelscriptTestExecutor Executor(
				*TestRunner,
				Engine,
				*Module,
				TEXT("int RunGlobal(UHotReloadGlobalDelegateReceiver, int, int)"));
			ASSERT_THAT(IsTrue(Executor.IsValid(), TEXT("Delegate global hot reload should resolve V3 RunGlobal")));
			const int32 Result = Executor
				.AddArgObject(ReloadedReceiver)
				.AddArg(static_cast<int32>(20))
				.AddArg(static_cast<int32>(8))
				.ExecuteAndGet<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(32, Result, TEXT("Delegate global hot reload V3 global function should execute the new delegate signature")));
		}
		}
	}

	TEST_METHOD(BlueprintDelegateWorldTickContinuesAfterSoftReload)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{ FAngelscriptEngineScope EngineScope(Engine);
		FScopedTransientBlueprint Blueprint;
		ON_SCOPE_EXIT
		{
			Blueprint.Cleanup();
			Engine.DiscardModule(*TickDelegateModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		// Script under test: V1 binds a delegate in BeginPlay and invokes it from Tick.
		const FString TickReloadV1Source = ASTEST_AS(R"AS(
			delegate int FHotReloadTickCompute(int TickIndex);

			UCLASS()
			class AHotReloadDelegateRuntimeTickParent : AActor
			{
				UPROPERTY()
				FHotReloadTickCompute OnTickCompute;

				UPROPERTY()
				int BeginPlayCount = 0;

				UPROPERTY()
				int TickCount = 0;

				UPROPERTY()
				int Total = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
					OnTickCompute.BindUFunction(this, n"HandleTickCompute");
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					TickCount += 1;

					if (!OnTickCompute.IsBound())
					{
						OnTickCompute.BindUFunction(this, n"HandleTickCompute");
					}

					Total += OnTickCompute.Execute(TickCount);
				}

				UFUNCTION()
				int HandleTickCompute(int TickIndex)
				{
					return TickIndex + 10;
				}
			}
			)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			TickDelegateModuleName,
			TickDelegateFilename,
			TickReloadV1Source,
			TickDelegateClassName);
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("Delegate world tick hot reload should compile the V1 parent class")));

		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ParentClass, TEXT("DelegateWorldTick"))));
		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Delegate world tick hot reload should expose a Blueprint generated class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentClass), TEXT("Delegate world tick Blueprint should inherit from V1 parent")));

		FAngelscriptTestWorld World(*TestRunner, Engine);
		ASSERT_THAT(IsTrue(World.IsValid(), TEXT("Delegate world tick hot reload should create a test world")));

		AActor* BlueprintActor = World.SpawnActorOfClass(BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Delegate world tick hot reload should spawn a Blueprint child actor")));
		EnableActorTick(*BlueprintActor);
		World.BeginPlay(*BlueprintActor);
		World.DispatchActorTick(*BlueprintActor, 0.016f, 2);

		int32 BeginPlayCountBeforeReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("BeginPlayCount"),
			BeginPlayCountBeforeReload,
			TEXT("Delegate world tick hot reload V1 Blueprint actor"))));
		ASSERT_THAT(AreEqual(1, BeginPlayCountBeforeReload, TEXT("Delegate world tick hot reload should begin play once before reload")));

		int32 TickCountBeforeReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("TickCount"),
			TickCountBeforeReload,
			TEXT("Delegate world tick hot reload V1 Blueprint actor"))));
		ASSERT_THAT(AreEqual(2, TickCountBeforeReload, TEXT("Delegate world tick hot reload should dispatch two baseline ticks")));

		int32 TotalBeforeReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("Total"),
			TotalBeforeReload,
			TEXT("Delegate world tick hot reload V1 Blueprint actor"))));
		ASSERT_THAT(AreEqual(23, TotalBeforeReload, TEXT("Delegate world tick hot reload should execute V1 delegate handler during ticks")));

		// Script under test: V2 changes only the delegate target body while the Blueprint actor is already running.
		const FString TickReloadV2Source = ASTEST_AS(R"AS(
			delegate int FHotReloadTickCompute(int TickIndex);

			UCLASS()
			class AHotReloadDelegateRuntimeTickParent : AActor
			{
				UPROPERTY()
				FHotReloadTickCompute OnTickCompute;

				UPROPERTY()
				int BeginPlayCount = 0;

				UPROPERTY()
				int TickCount = 0;

				UPROPERTY()
				int Total = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
					OnTickCompute.BindUFunction(this, n"HandleTickCompute");
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaTime)
				{
					TickCount += 1;

					if (!OnTickCompute.IsBound())
					{
						OnTickCompute.BindUFunction(this, n"HandleTickCompute");
					}

					Total += OnTickCompute.Execute(TickCount);
				}

				UFUNCTION()
				int HandleTickCompute(int TickIndex)
				{
					return TickIndex + 100;
				}
			}
			)AS");

		ECompileResult TickReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileReload(
			*TestRunner,
			Engine,
			ECompileType::SoftReloadOnly,
			TickDelegateModuleName,
			TickDelegateFilename,
			TickReloadV2Source,
			TEXT("Delegate world tick hot reload running body update"),
			&TickReloadResult)));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, TickReloadResult, TEXT("Delegate world tick hot reload V2 should stay on the soft reload path")));

		UClass* SoftReloadedParentClass = FindGeneratedClass(&Engine, TickDelegateClassName);
		ASSERT_THAT(IsNotNull(SoftReloadedParentClass, TEXT("Delegate world tick hot reload should resolve parent after soft reload")));
		ASSERT_THAT(AreEqual(ParentClass, SoftReloadedParentClass, TEXT("Delegate world tick hot reload should preserve parent class identity after body reload")));
		ASSERT_THAT(AreEqual(BlueprintClass, BlueprintActor->GetClass(), TEXT("Delegate world tick hot reload should keep the running actor on the Blueprint class")));

		World.DispatchActorTick(*BlueprintActor, 0.016f, 2);

		int32 TickCountAfterReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("TickCount"),
			TickCountAfterReload,
			TEXT("Delegate world tick hot reload V2 Blueprint actor"))));
		ASSERT_THAT(AreEqual(4, TickCountAfterReload, TEXT("Delegate world tick hot reload should continue ticking the running Blueprint actor")));

		int32 TotalAfterReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("Total"),
			TotalAfterReload,
			TEXT("Delegate world tick hot reload V2 Blueprint actor"))));
		ASSERT_THAT(AreEqual(230, TotalAfterReload, TEXT("Delegate world tick hot reload should execute the V2 delegate handler on later ticks")));

		int32 BeginPlayCountAfterReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(
			*TestRunner,
			BlueprintActor,
			TEXT("BeginPlayCount"),
			BeginPlayCountAfterReload,
			TEXT("Delegate world tick hot reload V2 Blueprint actor"))));
		ASSERT_THAT(AreEqual(1, BeginPlayCountAfterReload, TEXT("Delegate world tick hot reload should not replay BeginPlay during soft reload")));
		}
	}
};
