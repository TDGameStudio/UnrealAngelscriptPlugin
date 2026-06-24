// ============================================================================
// AngelscriptAssetManagerFunctionLibraryTests.cpp
//
// AssetManager function library binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.FunctionLibraries.AssetManager.FAngelscriptAssetManagerFunctionLibraryTest.*
//
// Sections:
//   NullAndInvalidCallbackGuards — null-manager guards and callback dispatch
//   ScriptMixinCallsFromAngelscript — AS-side UAssetManager mixin calls
//
// CQTest adaptation notes:
//   This test compiles annotated UCLASS modules and exercises native C++
//   UAssetManagerMixinLibrary null-guard paths.  Because the AS source
//   defines UCLASS/UPROPERTY/UFUNCTION types (not plain global functions),
//   module lifecycle is managed manually via CompileAnnotatedModuleFromMemory
//   and DiscardModule rather than through FScopedAngelscriptModule.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptReflectiveAccess.h"

#include "FunctionLibraries/UAssetManagerMixinLibrary.h"

#include "Engine/AssetManager.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace AngelscriptAssetManagerTestHelpers
{
	static constexpr TCHAR ValidReceiverClassName[] = TEXT("UAssetManagerValidScanReceiver");
	static constexpr TCHAR MissingReceiverClassName[] = TEXT("UAssetManagerMissingScanReceiver");
	static constexpr TCHAR ScriptProbeClassName[] = TEXT("UAssetManagerScriptCallProbe");
	static constexpr TCHAR CallbackCountPropertyName[] = TEXT("CallbackCount");

	bool ReadIntPropertyChecked(
		FAutomationTestBase& Test,
		UObject& Object,
		FName PropertyName,
		const TCHAR* ContextLabel,
		int32& OutValue)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FIntProperty* Property = FindFProperty<FIntProperty>(Object.GetClass(), PropertyName);
		if (!LocalAssert.IsNotNull(
			Property,
			*FString::Printf(TEXT("%s should expose int property '%s'"), ContextLabel, *PropertyName.ToString())))
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(&Object);
		return true;
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptAssetManagerFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.AssetManager",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: NullAndInvalidCallbackGuards
	// ====================================================================

	TEST_METHOD(NullAndInvalidCallbackGuards)
	{
		using namespace AngelscriptAssetManagerTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASAssetManagerNullAndInvalidCallbackGuards"));
		};

		const FString ReceiverScript = TEXT(R"AS(
UCLASS()
class UAssetManagerValidScanReceiver : UObject
{
	UPROPERTY()
	int CallbackCount;

	UFUNCTION()
	void OnScanComplete()
	{
		CallbackCount += 1;
	}
}

UCLASS()
class UAssetManagerMissingScanReceiver : UObject
{
	UPROPERTY()
	int CallbackCount;

	UFUNCTION()
	void DifferentFunction()
	{
		CallbackCount += 1;
	}
}
)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, TEXT("ASAssetManagerNullAndInvalidCallbackGuards"), TEXT("ASAssetManagerNullAndInvalidCallbackGuards.as"), ReceiverScript),
			TEXT("AssetManager guard test should compile the callback receiver module")));

		UClass* ValidReceiverClass = FindGeneratedClass(&Engine, ValidReceiverClassName);
		UClass* MissingReceiverClass = FindGeneratedClass(&Engine, MissingReceiverClassName);
		ASSERT_THAT(IsNotNull(ValidReceiverClass, TEXT("AssetManager guard test should generate the valid callback receiver class")));
		ASSERT_THAT(IsNotNull(MissingReceiverClass, TEXT("AssetManager guard test should generate the missing-callback receiver class")));

		UObject* ValidReceiver = NewObject<UObject>(GetTransientPackage(), ValidReceiverClass);
		UObject* MissingReceiver = NewObject<UObject>(GetTransientPackage(), MissingReceiverClass);
		ASSERT_THAT(IsNotNull(ValidReceiver, TEXT("AssetManager guard test should instantiate the valid callback receiver")));
		ASSERT_THAT(IsNotNull(MissingReceiver, TEXT("AssetManager guard test should instantiate the missing-callback receiver")));

		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		ASSERT_THAT(IsNotNull(AssetManager, TEXT("AssetManager guard test should resolve an initialized asset manager")));

		ASSERT_THAT(IsTrue(AssetManager->HasInitialScanCompleted(), TEXT("AssetManager guard test requires the asset manager initial scan to be complete")));

		const FPrimaryAssetId DummyPrimaryAssetId(TEXT("MissingType"), TEXT("MissingName"));
		const FPrimaryAssetType DummyPrimaryAssetType(TEXT("MissingType"));

		FAssetData AssetData;
		TArray<FAssetData> AssetDataList;
		AssetDataList.AddDefaulted();
		TArray<FPrimaryAssetId> PrimaryAssetIdList;
		PrimaryAssetIdList.Add(DummyPrimaryAssetId);
		FPrimaryAssetTypeInfo AssetTypeInfo(FName(TEXT("DirtyType")), UObject::StaticClass(), false, false);
		TArray<FPrimaryAssetTypeInfo> AssetTypeInfoList;
		AssetTypeInfoList.Add(FPrimaryAssetTypeInfo(FName(TEXT("DirtyListType")), UObject::StaticClass(), false, false));
		const FPrimaryAssetRules DefaultRules;
		const FPrimaryAssetTypeInfo DefaultTypeInfo;

		bool bNullGuardResultsValid = true;
		bNullGuardResultsValid &= this->Assert.IsFalse(
			UAssetManagerMixinLibrary::GetPrimaryAssetData(nullptr, DummyPrimaryAssetId, AssetData),
			TEXT("Null asset manager should fail GetPrimaryAssetData"));
		bNullGuardResultsValid &= this->Assert.IsFalse(
			UAssetManagerMixinLibrary::GetPrimaryAssetDataList(nullptr, DummyPrimaryAssetType, AssetDataList),
			TEXT("Null asset manager should fail GetPrimaryAssetDataList"));
		bNullGuardResultsValid &= this->Assert.IsNull(
			UAssetManagerMixinLibrary::GetPrimaryAssetObject(nullptr, DummyPrimaryAssetId),
			TEXT("Null asset manager should return null from GetPrimaryAssetObject"));
		bNullGuardResultsValid &= this->Assert.IsFalse(
			UAssetManagerMixinLibrary::GetPrimaryAssetIdForObject(nullptr, ValidReceiver).IsValid(),
			TEXT("Null asset manager should return an invalid primary asset id for objects"));
		bNullGuardResultsValid &= this->Assert.IsFalse(
			UAssetManagerMixinLibrary::GetPrimaryAssetIdList(nullptr, DummyPrimaryAssetType, PrimaryAssetIdList),
			TEXT("Null asset manager should fail GetPrimaryAssetIdList"));
		bNullGuardResultsValid &= this->Assert.IsFalse(
			UAssetManagerMixinLibrary::GetPrimaryAssetTypeInfo(nullptr, DummyPrimaryAssetType, AssetTypeInfo),
			TEXT("Null asset manager should fail GetPrimaryAssetTypeInfo"));
		if (!bNullGuardResultsValid)
		{
			return;
		}

		UAssetManagerMixinLibrary::GetPrimaryAssetTypeInfoList(nullptr, AssetTypeInfoList);
		const FPrimaryAssetRules Rules = UAssetManagerMixinLibrary::GetPrimaryAssetRules(nullptr, DummyPrimaryAssetId);

		bool bNullOutputStateValid = true;
		bNullOutputStateValid &= this->Assert.IsFalse(AssetData.IsValid(), TEXT("Null asset manager should leave asset data invalid"));
		bNullOutputStateValid &= this->Assert.AreEqual(0, AssetDataList.Num(), TEXT("Null asset manager should clear the asset data list"));
		bNullOutputStateValid &= this->Assert.AreEqual(0, PrimaryAssetIdList.Num(), TEXT("Null asset manager should clear the primary asset id list"));
		bNullOutputStateValid &= this->Assert.AreEqual(DefaultTypeInfo.PrimaryAssetType, AssetTypeInfo.PrimaryAssetType, TEXT("Null asset manager should reset the primary asset type info type"));
		bNullOutputStateValid &= this->Assert.IsTrue(AssetTypeInfo.AssetBaseClassLoaded.Get() == UObject::StaticClass(), TEXT("Null asset manager should reset the primary asset type info base class to UObject"));
		bNullOutputStateValid &= this->Assert.AreEqual(DefaultTypeInfo.bIsDynamicAsset, AssetTypeInfo.bIsDynamicAsset, TEXT("Null asset manager should reset the primary asset type info dynamic flag"));
		bNullOutputStateValid &= this->Assert.AreEqual(DefaultTypeInfo.NumberOfAssets, AssetTypeInfo.NumberOfAssets, TEXT("Null asset manager should reset the primary asset type info asset count"));
		bNullOutputStateValid &= this->Assert.AreEqual(DefaultTypeInfo.AssetScanPaths.Num(), AssetTypeInfo.AssetScanPaths.Num(), TEXT("Null asset manager should reset the primary asset type info scan paths"));
		bNullOutputStateValid &= this->Assert.AreEqual(DefaultRules, AssetTypeInfo.Rules, TEXT("Null asset manager should reset the primary asset type info rules"));
		bNullOutputStateValid &= this->Assert.AreEqual(0, AssetTypeInfoList.Num(), TEXT("Null asset manager should clear the asset type info list"));
		bNullOutputStateValid &= this->Assert.AreEqual(DefaultRules, Rules, TEXT("Null asset manager should return default primary asset rules"));
		if (!bNullOutputStateValid)
		{
			return;
		}

		int32 ValidCallbackCount = INDEX_NONE;
		int32 MissingCallbackCount = INDEX_NONE;
		if (!ReadIntPropertyChecked(*TestRunner, *ValidReceiver, CallbackCountPropertyName, TEXT("Valid asset manager receiver"), ValidCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, *MissingReceiver, CallbackCountPropertyName, TEXT("Missing asset manager receiver"), MissingCallbackCount))
		{
			return;
		}

		UAssetManagerMixinLibrary::CallOrRegister_OnCompletedInitialScan(nullptr, ValidReceiver, TEXT("OnScanComplete"));
		UAssetManagerMixinLibrary::CallOrRegister_OnCompletedInitialScan(AssetManager, nullptr, TEXT("OnScanComplete"));
		UAssetManagerMixinLibrary::CallOrRegister_OnCompletedInitialScan(AssetManager, MissingReceiver, TEXT("OnScanComplete"));
		UAssetManagerMixinLibrary::CallOrRegister_OnCompletedInitialScan(AssetManager, ValidReceiver, TEXT("DoesNotExist"));

		if (!ReadIntPropertyChecked(*TestRunner, *ValidReceiver, CallbackCountPropertyName, TEXT("Valid asset manager receiver after invalid callbacks"), ValidCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, *MissingReceiver, CallbackCountPropertyName, TEXT("Missing asset manager receiver after invalid callbacks"), MissingCallbackCount))
		{
			return;
		}

		bool bInvalidCallbacksSuppressed = true;
		bInvalidCallbacksSuppressed &= this->Assert.AreEqual(
			0,
			ValidCallbackCount,
			TEXT("Invalid asset manager callback inputs should not trigger the valid receiver"));
		bInvalidCallbacksSuppressed &= this->Assert.AreEqual(
			0,
			MissingCallbackCount,
			TEXT("Invalid asset manager callback inputs should not trigger the missing-callback receiver"));
		if (!bInvalidCallbacksSuppressed)
		{
			return;
		}

		UAssetManagerMixinLibrary::CallOrRegister_OnCompletedInitialScan(AssetManager, ValidReceiver, TEXT("OnScanComplete"));
		if (!ReadIntPropertyChecked(*TestRunner, *ValidReceiver, CallbackCountPropertyName, TEXT("Valid asset manager receiver after baseline callback"), ValidCallbackCount))
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, ValidCallbackCount, TEXT("Valid asset manager callback path should still trigger exactly once")));
	}

	// ====================================================================
	// Section: ScriptMixinCallsFromAngelscript
	// ====================================================================

	TEST_METHOD(ScriptMixinCallsFromAngelscript)
	{
		using namespace AngelscriptAssetManagerTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASAssetManagerScriptMixinCalls"));
		};

		const FString ProbeScript = TEXT(R"AS(
UCLASS()
class UAssetManagerScriptCallProbe : UObject
{
	UPROPERTY()
	int CallbackCount;

	UFUNCTION()
	void OnInitialScanComplete()
	{
		CallbackCount += 1;
	}

	UFUNCTION()
	int RunGetPrimaryAssetDataProbe(UAssetManager AssetManager)
	{
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetData: begin");

		if (AssetManager == null)
		{
			Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetData: AssetManager is null, result=10");
			return 10;
		}

		FPrimaryAssetId MissingAssetId("ASAssetManagerMissingType:ASAssetManagerMissingName");
		FAssetData AssetData;
		bool bFoundAssetData = AssetManager.GetPrimaryAssetData(MissingAssetId, AssetData);
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetData: found=" + bFoundAssetData);
		return bFoundAssetData ? 30 : 1;
	}

	UFUNCTION()
	int RunGetPrimaryAssetDataListProbe(UAssetManager AssetManager)
	{
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetDataList: begin");

		if (AssetManager == null)
		{
			Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetDataList: AssetManager is null, result=10");
			return 10;
		}

		FPrimaryAssetType MissingAssetType(n"ASAssetManagerMissingType");
		TArray<FAssetData> AssetDataList;
		bool bFoundAssetDataList = AssetManager.GetPrimaryAssetDataList(MissingAssetType, AssetDataList);
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetDataList: found=" + bFoundAssetDataList + " count=" + AssetDataList.Num());
		if (bFoundAssetDataList)
			return 40;
		if (AssetDataList.Num() != 0)
			return 41;
		return 1;
	}

	UFUNCTION()
	int RunGetPrimaryAssetObjectProbe(UAssetManager AssetManager)
	{
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetObject: begin");

		if (AssetManager == null)
		{
			Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetObject: AssetManager is null, result=10");
			return 10;
		}

		FPrimaryAssetId MissingAssetId("ASAssetManagerMissingType:ASAssetManagerMissingName");
		UObject FoundObject = AssetManager.GetPrimaryAssetObject(MissingAssetId);
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetObject: isValid=" + (FoundObject != null));
		return FoundObject != null ? 50 : 1;
	}

	UFUNCTION()
	int RunGetPrimaryAssetIdForObjectProbe(UAssetManager AssetManager, UObject ProbeObject)
	{
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetIdForObject: begin");

		if (AssetManager == null)
		{
			Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetIdForObject: AssetManager is null, result=10");
			return 10;
		}

		if (ProbeObject == null)
		{
			Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetIdForObject: ProbeObject is null, result=20");
			return 20;
		}

		FPrimaryAssetId ObjectAssetId = AssetManager.GetPrimaryAssetIdForObject(ProbeObject);
		bool bObjectAssetIdValid = ObjectAssetId.IsValid();
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetIdForObject: isValid=" + bObjectAssetIdValid);
		return bObjectAssetIdValid ? 60 : 1;
	}

	UFUNCTION()
	int RunGetPrimaryAssetIdListProbe(UAssetManager AssetManager)
	{
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetIdList: begin");

		if (AssetManager == null)
		{
			Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetIdList: AssetManager is null, result=10");
			return 10;
		}

		FPrimaryAssetType MissingAssetType(n"ASAssetManagerMissingType");
		TArray<FPrimaryAssetId> PrimaryAssetIds;
		bool bFoundPrimaryAssetIds = AssetManager.GetPrimaryAssetIdList(MissingAssetType, PrimaryAssetIds);
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.GetPrimaryAssetIdList: found=" + bFoundPrimaryAssetIds + " count=" + PrimaryAssetIds.Num());
		if (bFoundPrimaryAssetIds)
			return 70;
		if (PrimaryAssetIds.Num() != 0)
			return 71;
		return 1;
	}

	UFUNCTION()
	int RunInitialScanCallbackProbe(UAssetManager AssetManager)
	{
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.InitialScanCallback: begin");

		if (AssetManager == null)
		{
			Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.InitialScanCallback: AssetManager is null, result=10");
			return 10;
		}

		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.InitialScanCallback: callback count before=" + CallbackCount);
		AssetManager.CallOrRegister_OnCompletedInitialScan(this, n"OnInitialScanComplete");
		int Result = CallbackCount == 1 ? 1 : 80;
		Log(n"AssetManagerBindings", "ASAssetManagerScriptMixinCalls.InitialScanCallback: callback count after=" + CallbackCount + " result=" + Result);
		return Result;
	}
}
)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, TEXT("ASAssetManagerScriptMixinCalls"), TEXT("ASAssetManagerScriptMixinCalls.as"), ProbeScript),
			TEXT("AssetManager script mixin test should compile the probe module")));

		UClass* ProbeClass = FindGeneratedClass(&Engine, ScriptProbeClassName);
		ASSERT_THAT(IsNotNull(ProbeClass, TEXT("AssetManager script mixin test should generate the probe class")));

		UObject* Probe = NewObject<UObject>(GetTransientPackage(), ProbeClass);
		ASSERT_THAT(IsNotNull(Probe, TEXT("AssetManager script mixin test should instantiate the probe")));

		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		ASSERT_THAT(IsNotNull(AssetManager, TEXT("AssetManager script mixin test should resolve an initialized asset manager")));

		ASSERT_THAT(IsTrue(AssetManager->HasInitialScanCompleted(), TEXT("AssetManager script mixin test requires the asset manager initial scan to be complete")));

		auto InvokeAssetManagerProbe = [this, Probe, AssetManager](FName FunctionName, const TCHAR* AssertionText) -> bool
		{
			FFunctionInvoker Invoker(*TestRunner, Probe, FunctionName);
			if (!Invoker.IsValid())
			{
				return false;
			}

			Invoker.AddParam<UAssetManager*>(AssetManager);
			return this->Assert.AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE), AssertionText);
		};

		auto InvokeAssetManagerObjectProbe = [this, Probe, AssetManager](FName FunctionName, UObject* ProbeObject, const TCHAR* AssertionText) -> bool
		{
			FFunctionInvoker Invoker(*TestRunner, Probe, FunctionName);
			if (!Invoker.IsValid())
			{
				return false;
			}

			Invoker.AddParam<UAssetManager*>(AssetManager);
			Invoker.AddParam<UObject*>(ProbeObject);
			return this->Assert.AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE), AssertionText);
		};

		if (!InvokeAssetManagerProbe(TEXT("RunGetPrimaryAssetDataProbe"), TEXT("Angelscript should call UAssetManager.GetPrimaryAssetData mixin"))
			|| !InvokeAssetManagerProbe(TEXT("RunGetPrimaryAssetDataListProbe"), TEXT("Angelscript should call UAssetManager.GetPrimaryAssetDataList mixin"))
			|| !InvokeAssetManagerProbe(TEXT("RunGetPrimaryAssetObjectProbe"), TEXT("Angelscript should call UAssetManager.GetPrimaryAssetObject mixin"))
			|| !InvokeAssetManagerObjectProbe(TEXT("RunGetPrimaryAssetIdForObjectProbe"), Probe, TEXT("Angelscript should call UAssetManager.GetPrimaryAssetIdForObject mixin"))
			|| !InvokeAssetManagerProbe(TEXT("RunGetPrimaryAssetIdListProbe"), TEXT("Angelscript should call UAssetManager.GetPrimaryAssetIdList mixin"))
			|| !InvokeAssetManagerProbe(TEXT("RunInitialScanCallbackProbe"), TEXT("Angelscript should call UAssetManager initial-scan callback mixin")))
		{
			return;
		}
		int32 CallbackCount = INDEX_NONE;
		if (ReadIntPropertyChecked(*TestRunner, *Probe, CallbackCountPropertyName, TEXT("Asset manager script mixin probe"), CallbackCount))
		{
			ASSERT_THAT(AreEqual(1, CallbackCount, TEXT("Angelscript AssetManager callback path should increment the probe once")));
		}
	}
};

#endif
