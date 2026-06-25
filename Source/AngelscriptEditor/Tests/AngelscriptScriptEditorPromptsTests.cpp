#include "CQTest.h"
#include "Tests/AngelscriptScriptEditorPromptsTestTypes.h"

#include "EditorMenuExtensions/ScriptEditorPrompts.h"

#include "AssetRegistry/AssetData.h"
#include "Curves/CurveFloat.h"
#include "Math/Vector.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptScriptEditorPromptsTests_Private
{
	template <typename AssetType>
	AssetType* CreatePromptTestAsset(
		FAutomationTestBase& Test,
		const TCHAR* PackageBaseName,
		UPackage*& OutPackage)
	{
		const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString PackagePath = FString::Printf(TEXT("/Temp/%s_%s"), PackageBaseName, *UniqueSuffix);
		OutPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("ScriptEditorPrompts test should create package '%s'"), *PackagePath),
			OutPackage))
		{
			return nullptr;
		}

		const FName DesiredName(*FString::Printf(TEXT("%sAsset"), PackageBaseName));
		const FName UniqueName = MakeUniqueObjectName(OutPackage, AssetType::StaticClass(), DesiredName);
		return NewObject<AssetType>(OutPackage, UniqueName, RF_Public | RF_Standalone | RF_Transient);
	}

	void CleanupPromptTestObject(UObject*& Object)
	{
		if (Object == nullptr)
		{
			return;
		}

		Object->ClearFlags(RF_Public | RF_Standalone);
		Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_AllowPackageLinkerMismatch | REN_NonTransactional);
		Object->MarkAsGarbage();
		Object = nullptr;
	}

	void CleanupPromptTestPackage(UPackage*& Package)
	{
		if (Package == nullptr)
		{
			return;
		}

		Package->MarkAsGarbage();
		Package = nullptr;
	}

	TSharedPtr<FStructOnScope> MakeAssetDataSelection(const FAssetData& AssetData)
	{
		return MakeShared<FStructOnScope>(FAssetData::StaticStruct(), reinterpret_cast<uint8*>(const_cast<FAssetData*>(&AssetData)));
	}

	TSharedPtr<FStructOnScope> MakeVectorSelection(const FVector& Value)
	{
		return MakeShared<FStructOnScope>(TBaseStructure<FVector>::Get(), reinterpret_cast<uint8*>(const_cast<FVector*>(&Value)));
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunStructFirstParameterFiltersMatchingStructs(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptScriptEditorPromptsTests_Private;
	UAngelscriptScriptEditorPromptsAssetReceiver* Receiver = NewObject<UAngelscriptScriptEditorPromptsAssetReceiver>(GetTransientPackage());
	UPackage* FirstPackage = nullptr;
	UPackage* SecondPackage = nullptr;
	UObject* FirstAssetObject = CreatePromptTestAsset<UCurveFloat>(Test, TEXT("ScriptEditorPromptFirst"), FirstPackage);
	UObject* SecondAssetObject = CreatePromptTestAsset<UCurveFloat>(Test, TEXT("ScriptEditorPromptSecond"), SecondPackage);
	if (!TestNotNull(TEXT("ScriptEditorPrompts test should create the receiver object"), Receiver)
		|| !TestNotNull(TEXT("ScriptEditorPrompts test should create the first asset"), FirstAssetObject)
		|| !TestNotNull(TEXT("ScriptEditorPrompts test should create the second asset"), SecondAssetObject))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		CleanupPromptTestObject(FirstAssetObject);
		CleanupPromptTestObject(SecondAssetObject);
		CleanupPromptTestPackage(FirstPackage);
		CleanupPromptTestPackage(SecondPackage);
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	};

	const FAssetData FirstAssetData(FirstAssetObject);
	const FAssetData SecondAssetData(SecondAssetObject);
	const FVector IgnoredVector(1.0f, 2.0f, 3.0f);

	TArray<TSharedPtr<FStructOnScope>> FirstParameterStructs;
	FirstParameterStructs.Add(MakeAssetDataSelection(FirstAssetData));
	FirstParameterStructs.Add(MakeVectorSelection(IgnoredVector));
	FirstParameterStructs.Add(MakeAssetDataSelection(SecondAssetData));

	FScriptEditorPromptOptions Options;
	Options.HiddenProperties.Add(TEXT("ExtraValue"));

	Receiver->ResetRecordedAssets();
	const bool bInvoked = FScriptEditorPrompts::ShowPromptToCallFunction(
		Receiver,
		GET_FUNCTION_NAME_CHECKED(UAngelscriptScriptEditorPromptsAssetReceiver, RecordAsset),
		Options,
		FirstParameterStructs);

	TestTrue(TEXT("ScriptEditorPrompts struct overload should report success when the function exists"), bInvoked);
	TestEqual(TEXT("ScriptEditorPrompts struct overload should invoke ProcessEvent only for matching structs"), Receiver->InvocationCount, 2);
	TestEqual(TEXT("ScriptEditorPrompts struct overload should record two matching assets"), Receiver->RecordedAssetNames.Num(), 2);
	TestEqual(TEXT("ScriptEditorPrompts struct overload should record two matching package names"), Receiver->RecordedPackageNames.Num(), 2);
	TestEqual(TEXT("ScriptEditorPrompts struct overload should record two default-value payloads"), Receiver->RecordedExtraValues.Num(), 2);
	if (Receiver->RecordedAssetNames.Num() >= 2)
	{
		TestEqual(TEXT("ScriptEditorPrompts struct overload should preserve the first asset order"), Receiver->RecordedAssetNames[0], FirstAssetData.AssetName);
		TestEqual(TEXT("ScriptEditorPrompts struct overload should preserve the second asset order"), Receiver->RecordedAssetNames[1], SecondAssetData.AssetName);
	}
	if (Receiver->RecordedPackageNames.Num() >= 2)
	{
		TestEqual(TEXT("ScriptEditorPrompts struct overload should forward the first package name"), Receiver->RecordedPackageNames[0], FirstAssetData.PackageName);
		TestEqual(TEXT("ScriptEditorPrompts struct overload should forward the second package name"), Receiver->RecordedPackageNames[1], SecondAssetData.PackageName);
	}
	if (Receiver->RecordedExtraValues.Num() >= 2)
	{
		TestEqual(TEXT("ScriptEditorPrompts struct overload should preserve the default ExtraValue for the first asset"), Receiver->RecordedExtraValues[0], 42);
		TestEqual(TEXT("ScriptEditorPrompts struct overload should preserve the default ExtraValue for the second asset"), Receiver->RecordedExtraValues[1], 42);
	}

	return true;
}

static bool RunShowPromptToCallFunctionOnObjectsSkipsNullAndMismatchedReceivers(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptScriptEditorPromptsTests_Private;
	UAngelscriptScriptEditorPromptsBatchReceiver* FirstReceiver = NewObject<UAngelscriptScriptEditorPromptsBatchReceiver>(GetTransientPackage());
	UAngelscriptScriptEditorPromptsBatchReceiver* SecondReceiver = NewObject<UAngelscriptScriptEditorPromptsBatchReceiver>(GetTransientPackage());
	UObject* UnrelatedObject = NewObject<UAngelscriptScriptEditorPromptsUnrelatedObject>(GetTransientPackage());
	if (!TestNotNull(TEXT("ScriptEditorPrompts batch test should create the first matching receiver"), FirstReceiver)
		|| !TestNotNull(TEXT("ScriptEditorPrompts batch test should create the second matching receiver"), SecondReceiver)
		|| !TestNotNull(TEXT("ScriptEditorPrompts batch test should create the unrelated object"), UnrelatedObject))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		UObject* FirstObject = FirstReceiver;
		UObject* SecondObject = SecondReceiver;
		CleanupPromptTestObject(FirstObject);
		CleanupPromptTestObject(SecondObject);
		CleanupPromptTestObject(UnrelatedObject);
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	};

	FirstReceiver->ReceiverLabel = TEXT("First");
	SecondReceiver->ReceiverLabel = TEXT("Second");
	UAngelscriptScriptEditorPromptsBatchReceiver::ResetBatchInvocations();

	UFunction* BatchFunction = UAngelscriptScriptEditorPromptsBatchReceiver::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAngelscriptScriptEditorPromptsBatchReceiver, RecordBatch));
	if (!TestNotNull(TEXT("ScriptEditorPrompts batch test should find the batch receiver function"), BatchFunction))
	{
		return false;
	}

	FScriptEditorPromptOptions Options;
	Options.HiddenProperties.Add(TEXT("ExtraValue"));

	TArray<UObject*> ObjectsToInvoke;
	ObjectsToInvoke.Add(FirstReceiver);
	ObjectsToInvoke.Add(nullptr);
	ObjectsToInvoke.Add(UnrelatedObject);
	ObjectsToInvoke.Add(SecondReceiver);

	const bool bInvoked = FScriptEditorPrompts::ShowPromptToCallFunctionOnObjects(BatchFunction, ObjectsToInvoke, Options);

	TestTrue(TEXT("ScriptEditorPrompts batch overload should report success when a matching function is supplied"), bInvoked);
	TestEqual(TEXT("ScriptEditorPrompts batch overload should invoke the first matching receiver exactly once"), FirstReceiver->InvocationCount, 1);
	TestEqual(TEXT("ScriptEditorPrompts batch overload should invoke the second matching receiver exactly once"), SecondReceiver->InvocationCount, 1);
	TestEqual(TEXT("ScriptEditorPrompts batch overload should preserve the default ExtraValue for the first receiver"), FirstReceiver->LastExtraValue, 7);
	TestEqual(TEXT("ScriptEditorPrompts batch overload should preserve the default ExtraValue for the second receiver"), SecondReceiver->LastExtraValue, 7);

	const TArray<FName>& InvocationOrder = UAngelscriptScriptEditorPromptsBatchReceiver::GetInvocationOrder();
	TestEqual(TEXT("ScriptEditorPrompts batch overload should only record the two matching receivers"), InvocationOrder.Num(), 2);
	if (InvocationOrder.Num() >= 2)
	{
		TestEqual(TEXT("ScriptEditorPrompts batch overload should preserve the first matching receiver order"), InvocationOrder[0], FirstReceiver->ReceiverLabel);
		TestEqual(TEXT("ScriptEditorPrompts batch overload should preserve the second matching receiver order"), InvocationOrder[1], SecondReceiver->ReceiverLabel);
	}

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptScriptEditorPromptsTests,
	"Angelscript.Editor.ScriptEditorPrompts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StructFirstParameterFiltersMatchingStructs)
	{
		ASSERT_THAT(IsTrue(RunStructFirstParameterFiltersMatchingStructs(*TestRunner)));
	}

	TEST_METHOD(ShowPromptToCallFunctionOnObjectsSkipsNullAndMismatchedReceivers)
	{
		ASSERT_THAT(IsTrue(RunShowPromptToCallFunctionOnObjectsSkipsNullAndMismatchedReceivers(*TestRunner)));
	}
};

#endif
