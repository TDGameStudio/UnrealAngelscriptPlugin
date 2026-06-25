#include "CQTest.h"
#include "HotReload/ClassReloadHelper.h"

#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadReferenceReplacementTests_Private
{
	struct FRecordedReference
	{
		UObject* Object = nullptr;
		const UObject* ReferencingObject = nullptr;
		const FProperty* ReferencingProperty = nullptr;
	};

	class FRecordingReferenceCollector final : public FReferenceCollector
	{
	public:
		virtual bool IsIgnoringArchetypeRef() const override
		{
			return true;
		}

		virtual bool IsIgnoringTransient() const override
		{
			return false;
		}

		virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override
		{
			References.Add({InObject, InReferencingObject, InReferencingProperty});
		}

		bool Contains(UObject* Object) const
		{
			return References.ContainsByPredicate(
				[Object](const FRecordedReference& Reference)
				{
					return Reference.Object == Object;
				});
		}

		TArray<FRecordedReference> References;
	};

	class FTestAssetEditorInstance final : public IAssetEditorInstance
	{
	public:
		virtual FName GetEditorName() const override
		{
			return TEXT("AngelscriptReferenceReplacementHelperTestEditor");
		}

		virtual void FocusWindow(UObject* ObjectToFocusOn = nullptr) override
		{
		}

		virtual bool CloseWindow(EAssetEditorCloseReason InCloseReason) override
		{
			return true;
		}

		virtual bool IncludeAssetInRestoreOpenAssetsPrompt(UObject* Asset) const override
		{
			return false;
		}

		virtual bool IsPrimaryEditor() const override
		{
			return true;
		}

		virtual void InvokeTab(const struct FTabId& TabId) override
		{
		}

		virtual TSharedPtr<class FTabManager> GetAssociatedTabManager() override
		{
			return nullptr;
		}

		virtual double GetLastActivationTime() override
		{
			return 0.0;
		}

		virtual void RemoveEditingAsset(UObject* Asset) override
		{
			RemovedAssets.Add(Asset);
		}

		TArray<TWeakObjectPtr<UObject>> RemovedAssets;
	};

	enum class EAssetEditorNotificationType : uint8
	{
		Closed,
		Opened,
	};

	struct FAssetEditorNotification
	{
		EAssetEditorNotificationType Type = EAssetEditorNotificationType::Opened;
		TWeakObjectPtr<UObject> Asset;
		IAssetEditorInstance* EditorInstance = nullptr;
	};

	UDataTable* CreateReferenceReplacementTestAsset(FAutomationTestBase& Test, const TCHAR* BaseName)
	{
		UDataTable* Asset = NewObject<UDataTable>(
			GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), BaseName),
			RF_Transient);
		Test.TestNotNull(*FString::Printf(TEXT("ReferenceReplacementHelper test should create asset %s"), BaseName), Asset);
		return Asset;
	}

	void CleanupReferenceReplacementTestAsset(UObject*& Asset)
	{
		if (Asset == nullptr)
		{
			return;
		}

		Asset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_AllowPackageLinkerMismatch | REN_NonTransactional);
		Asset->MarkAsGarbage();
		Asset = nullptr;
	}

	int32 RunReferenceReplacementSerialize(
		UAngelscriptReferenceReplacementHelper& Helper,
		const TMap<UObject*, UObject*>& ReplacementMap)
	{
		FArchiveReplaceObjectRef<UObject> ReplaceArchive(
			&Helper,
			ReplacementMap,
			EArchiveReplaceObjectFlags::DelayStart
			| EArchiveReplaceObjectFlags::IgnoreOuterRef
			| EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
		FStructuredArchiveFromArchive StructuredArchive(ReplaceArchive);
		Helper.Serialize(StructuredArchive.GetSlot().EnterRecord());
		return ReplaceArchive.GetCount();
	}
}

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunReferenceReplacementHelper(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadReferenceReplacementTests_Private;
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor != nullptr ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	UAngelscriptReferenceReplacementHelper* Helper = NewObject<UAngelscriptReferenceReplacementHelper>(GetTransientPackage());
	UObject* OriginalAssetObject = CreateReferenceReplacementTestAsset(Test, TEXT("ClassReloadHelperOriginalAsset"));
	UObject* ReplacedAssetObject = CreateReferenceReplacementTestAsset(Test, TEXT("ClassReloadHelperReplacedAsset"));
	UObject* UnrelatedAssetObject = CreateReferenceReplacementTestAsset(Test, TEXT("ClassReloadHelperUnrelatedAsset"));
	FTestAssetEditorInstance EditorInstance;
	TArray<FAssetEditorNotification> Notifications;

	if (!TestNotNull(TEXT("ReferenceReplacementHelper test should resolve the asset editor subsystem"), AssetEditorSubsystem)
		|| !TestNotNull(TEXT("ReferenceReplacementHelper test should create the replacement helper"), Helper)
		|| !TestNotNull(TEXT("ReferenceReplacementHelper test should create the original asset"), OriginalAssetObject)
		|| !TestNotNull(TEXT("ReferenceReplacementHelper test should create the replacement asset"), ReplacedAssetObject)
		|| !TestNotNull(TEXT("ReferenceReplacementHelper test should create the unrelated asset"), UnrelatedAssetObject))
	{
		return false;
	}

	const FDelegateHandle ClosedHandle = AssetEditorSubsystem->OnAssetClosedInEditor().AddLambda(
		[&Notifications](UObject* Asset, IAssetEditorInstance* Editor)
		{
			Notifications.Add({EAssetEditorNotificationType::Closed, Asset, Editor});
		});
	const FDelegateHandle OpenedHandle = AssetEditorSubsystem->OnAssetOpenedInEditor().AddLambda(
		[&Notifications](UObject* Asset, IAssetEditorInstance* Editor)
		{
			Notifications.Add({EAssetEditorNotificationType::Opened, Asset, Editor});
		});

	auto CloseIfTracked = [AssetEditorSubsystem, &EditorInstance](UObject* Asset)
	{
		if (Asset == nullptr)
		{
			return;
		}

		if (AssetEditorSubsystem->FindEditorsForAsset(Asset).Contains(&EditorInstance))
		{
			AssetEditorSubsystem->NotifyAssetClosed(Asset, &EditorInstance);
		}
	};

	ON_SCOPE_EXIT
	{
		AssetEditorSubsystem->OnAssetClosedInEditor().Remove(ClosedHandle);
		AssetEditorSubsystem->OnAssetOpenedInEditor().Remove(OpenedHandle);
		CloseIfTracked(OriginalAssetObject);
		CloseIfTracked(ReplacedAssetObject);
		CloseIfTracked(UnrelatedAssetObject);
		CleanupReferenceReplacementTestAsset(OriginalAssetObject);
		CleanupReferenceReplacementTestAsset(ReplacedAssetObject);
		CleanupReferenceReplacementTestAsset(UnrelatedAssetObject);
	};

	AssetEditorSubsystem->NotifyAssetOpened(OriginalAssetObject, &EditorInstance);
	if (!TestTrue(TEXT("ReferenceReplacementHelper test should register the original asset as edited"), AssetEditorSubsystem->GetAllEditedAssets().Contains(OriginalAssetObject)))
	{
		return false;
	}

	FRecordingReferenceCollector Collector;
	UAngelscriptReferenceReplacementHelper::AddReferencedObjects(Helper, Collector);
	if (!TestTrue(TEXT("ReferenceReplacementHelper.AddReferencedObjects should keep the original open asset alive"), Collector.Contains(OriginalAssetObject)))
	{
		return false;
	}

	Notifications.Reset();

	TMap<UObject*, UObject*> ReplacementMap;
	ReplacementMap.Add(OriginalAssetObject, ReplacedAssetObject);

	const int32 ReplacedReferenceCount = RunReferenceReplacementSerialize(*Helper, ReplacementMap);
	if (!TestEqual(TEXT("ReferenceReplacementHelper.Serialize should record exactly one replacement for the original asset"), ReplacedReferenceCount, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ReferenceReplacementHelper.Serialize should emit one close and one open notification when the asset is replaced"), Notifications.Num(), 2))
	{
		return false;
	}
	if (!TestEqual(TEXT("ReferenceReplacementHelper.Serialize should close the original asset before reopening the replacement"), Notifications[0].Type, EAssetEditorNotificationType::Closed)
		|| !TestEqual(TEXT("ReferenceReplacementHelper.Serialize should close the original asset instance"), Notifications[0].Asset.Get(), OriginalAssetObject)
		|| !TestEqual(TEXT("ReferenceReplacementHelper.Serialize should close the exact editor instance that was open"), Notifications[0].EditorInstance, static_cast<IAssetEditorInstance*>(&EditorInstance))
		|| !TestEqual(TEXT("ReferenceReplacementHelper.Serialize should reopen the replacement asset after closing the original"), Notifications[1].Type, EAssetEditorNotificationType::Opened)
		|| !TestEqual(TEXT("ReferenceReplacementHelper.Serialize should reopen the replacement asset object"), Notifications[1].Asset.Get(), ReplacedAssetObject)
		|| !TestEqual(TEXT("ReferenceReplacementHelper.Serialize should reopen the replacement into the same editor instance"), Notifications[1].EditorInstance, static_cast<IAssetEditorInstance*>(&EditorInstance)))
	{
		return false;
	}

	if (!TestFalse(TEXT("ReferenceReplacementHelper.Serialize should remove the original asset from the asset editor subsystem after replacement"), AssetEditorSubsystem->GetAllEditedAssets().Contains(OriginalAssetObject))
		|| !TestTrue(TEXT("ReferenceReplacementHelper.Serialize should register the replacement asset as edited"), AssetEditorSubsystem->GetAllEditedAssets().Contains(ReplacedAssetObject))
		|| !TestFalse(TEXT("ReferenceReplacementHelper.Serialize should detach the editor from the original asset"), AssetEditorSubsystem->FindEditorsForAsset(OriginalAssetObject).Contains(&EditorInstance))
		|| !TestTrue(TEXT("ReferenceReplacementHelper.Serialize should retarget the editor to the replacement asset"), AssetEditorSubsystem->FindEditorsForAsset(ReplacedAssetObject).Contains(&EditorInstance)))
	{
		return false;
	}

	CloseIfTracked(ReplacedAssetObject);
	AssetEditorSubsystem->NotifyAssetOpened(OriginalAssetObject, &EditorInstance);
	Notifications.Reset();

	TMap<UObject*, UObject*> UnrelatedReplacementMap;
	UnrelatedReplacementMap.Add(UnrelatedAssetObject, ReplacedAssetObject);

	const int32 UntouchedReferenceCount = RunReferenceReplacementSerialize(*Helper, UnrelatedReplacementMap);
	if (!TestEqual(TEXT("ReferenceReplacementHelper.Serialize should not report replacements for unrelated assets"), UntouchedReferenceCount, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ReferenceReplacementHelper.Serialize should not emit notifications when no open asset is replaced"), Notifications.Num(), 0))
	{
		return false;
	}

	if (!TestTrue(TEXT("ReferenceReplacementHelper.Serialize should keep the original asset registered when no replacement occurs"), AssetEditorSubsystem->GetAllEditedAssets().Contains(OriginalAssetObject))
		|| !TestFalse(TEXT("ReferenceReplacementHelper.Serialize should keep the replacement asset closed when no replacement occurs"), AssetEditorSubsystem->GetAllEditedAssets().Contains(ReplacedAssetObject))
		|| !TestTrue(TEXT("ReferenceReplacementHelper.Serialize should keep the editor attached to the original asset when no replacement occurs"), AssetEditorSubsystem->FindEditorsForAsset(OriginalAssetObject).Contains(&EditorInstance))
		|| !TestFalse(TEXT("ReferenceReplacementHelper.Serialize should not attach the editor to the replacement asset when no replacement occurs"), AssetEditorSubsystem->FindEditorsForAsset(ReplacedAssetObject).Contains(&EditorInstance)))
	{
		return false;
	}

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(FAngelscriptClassReloadReferenceReplacementTests,
	"Angelscript.Editor.ClassReloadHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ReferenceReplacementHelperRetargetsOpenEditors)
	{
		ASSERT_THAT(IsTrue(RunReferenceReplacementHelper(*TestRunner)));
	}
};

#endif
