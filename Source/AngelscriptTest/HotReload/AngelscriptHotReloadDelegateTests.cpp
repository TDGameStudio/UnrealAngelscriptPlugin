#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_HotReload_AngelscriptHotReloadDelegateTests_Private
{
	static const FName DelegateReloadModuleName(TEXT("HotReloadDelegateMod"));
	static const FString DelegateReloadFilename(TEXT("HotReloadDelegateMod.as"));
	static const FName DelegateReloadCarrierClassName(TEXT("UHotReloadEventCarrier"));
	static const FString DelegateReloadEnumName(TEXT("EHotReloadEventState"));
	static const FName TypeReloadModuleName(TEXT("HotReloadDelegateTypeMod"));
	static const FString TypeReloadFilename(TEXT("HotReloadDelegateTypeMod.as"));
	static const FString TypeReloadStructName(TEXT("FHotReloadDelegatePayload"));
	static const FName TypeReloadCarrierClassName(TEXT("UHotReloadDelegateCarrier"));
	static const FName SignatureReloadModuleName(TEXT("HotReloadDelegateSignatureMod"));
	static const FString SignatureReloadFilename(TEXT("HotReloadDelegateSignatureMod.as"));
	static const FString SignatureReloadDelegateName(TEXT("FHotReloadSignal"));
	static const FName SignatureReloadCarrierClassName(TEXT("UHotReloadDelegateSignatureCarrier"));

	bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	bool ContainsEnumEntryWithSuffix(const TArray<TPair<FName, int64>>& Entries, const TCHAR* Suffix, const int64 ExpectedValue)
	{
		const FString ExpectedSuffix(Suffix);
		for (const TPair<FName, int64>& Entry : Entries)
		{
			if (Entry.Value == ExpectedValue && Entry.Key.ToString().EndsWith(ExpectedSuffix))
			{
				return true;
			}
		}

		return false;
	}

	bool TryFindEnumValueBySuffix(const UEnum& Enum, const TCHAR* Suffix, int64& OutValue)
	{
		const FString ExpectedSuffix(Suffix);
		const int32 EnumeratorsToCheck = Enum.NumEnums();
		for (int32 Index = 0; Index < EnumeratorsToCheck; ++Index)
		{
			const FString EnumEntryName = Enum.GetNameByIndex(Index).ToString();
			if (EnumEntryName.EndsWith(TEXT("_MAX")))
			{
				continue;
			}

			if (EnumEntryName.EndsWith(ExpectedSuffix))
			{
				OutValue = Enum.GetValueByIndex(Index);
				return true;
			}
		}

		return false;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHotReloadReloadDelegatesBroadcastEnumChangeAndFullReloadTest,
	"Angelscript.TestModule.HotReload.ReloadDelegates.BroadcastEnumChangeAndFullReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHotReloadReloadDelegatesBroadcastOldAndNewTypesTest,
	"Angelscript.TestModule.HotReload.Delegates.BroadcastOldAndNewTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHotReloadReloadDelegatesBroadcastDelegateSignatureSwapTest,
	"Angelscript.TestModule.HotReload.ReloadDelegates.BroadcastDelegateSignatureSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHotReloadReloadDelegatesBroadcastEnumChangeAndFullReloadTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadDelegateTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 EnumChangedCount = 0;
	int32 FullReloadCount = 0;
	int32 PostReloadCount = 0;
	bool bPostReloadSawFullReload = false;
	bool bEnumVisibleDuringPostReload = false;
	bool bCarrierVisibleDuringPostReload = false;
	UEnum* EnumSeenDuringReload = nullptr;
	TArray<TPair<FName, int64>> OldNamesSeenDuringReload;

	FDelegateHandle EnumChangedHandle;
	FDelegateHandle FullReloadHandle;
	FDelegateHandle PostReloadHandle;

	ON_SCOPE_EXIT
	{
		FAngelscriptClassGenerator::OnEnumChanged.Remove(EnumChangedHandle);
		FAngelscriptClassGenerator::OnFullReload.Remove(FullReloadHandle);
		FAngelscriptClassGenerator::OnPostReload.Remove(PostReloadHandle);
		Engine.DiscardModule(*DelegateReloadModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UENUM(BlueprintType)
enum class EHotReloadEventState : uint16
{
	Alpha,
	Beta = 4
}

UCLASS()
class UHotReloadEventCarrier : UObject
{
	UPROPERTY()
	EHotReloadEventState State;

	default State = EHotReloadEventState::Alpha;
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
UENUM(BlueprintType)
enum class EHotReloadEventState : uint16
{
	Alpha,
	Beta = 4,
	Gamma = 9
}

UCLASS()
class UHotReloadEventCarrier : UObject
{
	UPROPERTY()
	EHotReloadEventState State;

	default State = EHotReloadEventState::Gamma;
}
)AS");

	if (!TestTrue(
		TEXT("Initial enum reload-delegate module compile should succeed"),
		CompileAnnotatedModuleFromMemory(&Engine, DelegateReloadModuleName, DelegateReloadFilename, ScriptV1)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptEnumDesc> EnumBeforeReload = Engine.GetEnum(DelegateReloadEnumName);
	if (!TestTrue(TEXT("Enum metadata should exist before reload-delegate test"), EnumBeforeReload.IsValid()))
	{
		return false;
	}

	UEnum* EnumObjectBeforeReload = EnumBeforeReload->Enum;
	if (!TestNotNull(TEXT("Enum object should exist before full reload"), EnumObjectBeforeReload))
	{
		return false;
	}

	if (!TestNotNull(TEXT("Enum carrier class should exist before full reload"), FindGeneratedClass(&Engine, DelegateReloadCarrierClassName)))
	{
		return false;
	}

	EnumChangedHandle = FAngelscriptClassGenerator::OnEnumChanged.AddLambda(
		[&](UEnum* Enum, EnumNameList OldNames)
		{
			++EnumChangedCount;
			EnumSeenDuringReload = Enum;
			OldNamesSeenDuringReload = OldNames;
		});

	FullReloadHandle = FAngelscriptClassGenerator::OnFullReload.AddLambda(
		[&]()
		{
			++FullReloadCount;
		});

	PostReloadHandle = FAngelscriptClassGenerator::OnPostReload.AddLambda(
		[&](const bool bWasFullReload)
		{
			++PostReloadCount;
			bPostReloadSawFullReload = bWasFullReload;
			bEnumVisibleDuringPostReload = Engine.GetEnum(DelegateReloadEnumName).IsValid();
			bCarrierVisibleDuringPostReload = FindGeneratedClass(&Engine, DelegateReloadCarrierClassName) != nullptr;
		});

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(
		TEXT("Enum full reload compile should succeed"),
		CompileModuleWithResult(&Engine, ECompileType::FullReload, DelegateReloadModuleName, DelegateReloadFilename, ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Enum full reload should stay on a handled reload path"),
		IsHandledReloadResult(ReloadResult)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptEnumDesc> EnumAfterReload = Engine.GetEnum(DelegateReloadEnumName);
	if (!TestTrue(TEXT("Enum metadata should still exist after full reload"), EnumAfterReload.IsValid()))
	{
		return false;
	}

	UEnum* EnumObjectAfterReload = EnumAfterReload->Enum;
	if (!TestNotNull(TEXT("Reloaded enum object should still be queryable"), EnumObjectAfterReload))
	{
		return false;
	}

	if (!TestNotNull(TEXT("Enum carrier class should still be queryable after full reload"), FindGeneratedClass(&Engine, DelegateReloadCarrierClassName)))
	{
		return false;
	}

	TestEqual(TEXT("Full reload should broadcast the full-reload delegate exactly once"), FullReloadCount, 1);
	TestEqual(TEXT("Enum full reload should broadcast the enum-changed delegate exactly once"), EnumChangedCount, 1);
	TestEqual(TEXT("Enum full reload should broadcast the post-reload delegate exactly once"), PostReloadCount, 1);
	TestTrue(TEXT("Post-reload delegate should report that the completed reload was a full reload"), bPostReloadSawFullReload);
	TestTrue(TEXT("Enum should already be queryable when post-reload broadcasts"), bEnumVisibleDuringPostReload);
	TestTrue(TEXT("Carrier class should already be queryable when post-reload broadcasts"), bCarrierVisibleDuringPostReload);
	TestEqual(TEXT("Enum-changed broadcast should expose the live enum object"), EnumSeenDuringReload, EnumObjectAfterReload);

	TestEqual(TEXT("Enum-changed broadcast should preserve the old enum member count"), OldNamesSeenDuringReload.Num(), 2);
	TestTrue(TEXT("Old enum member list should keep Alpha before reload"), ContainsEnumEntryWithSuffix(OldNamesSeenDuringReload, TEXT("Alpha"), 0));
	TestTrue(TEXT("Old enum member list should keep Beta before reload"), ContainsEnumEntryWithSuffix(OldNamesSeenDuringReload, TEXT("Beta"), 4));
	TestFalse(TEXT("Old enum member list should not include the new Gamma value"), ContainsEnumEntryWithSuffix(OldNamesSeenDuringReload, TEXT("Gamma"), 9));

	int64 GammaValue = INDEX_NONE;
	if (!TestTrue(TEXT("Reloaded enum should expose the new Gamma enumerator"), TryFindEnumValueBySuffix(*EnumObjectAfterReload, TEXT("Gamma"), GammaValue)))
	{
		return false;
	}

	TestEqual(TEXT("Reloaded enum should assign the expected value to Gamma"), GammaValue, int64(9));
	}

	return true;
}

bool FAngelscriptHotReloadReloadDelegatesBroadcastOldAndNewTypesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadDelegateTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 ClassReloadCount = 0;
	int32 StructReloadCount = 0;
	int32 PostReloadCount = 0;
	bool bPostReloadSawFullReload = false;
	bool bStructVisibleDuringPostReload = false;
	bool bClassVisibleDuringPostReload = false;
	UClass* OldClassSeenDuringReload = nullptr;
	UClass* NewClassSeenDuringReload = nullptr;
	UScriptStruct* OldStructSeenDuringReload = nullptr;
	UScriptStruct* NewStructSeenDuringReload = nullptr;

	FDelegateHandle ClassReloadHandle;
	FDelegateHandle StructReloadHandle;
	FDelegateHandle PostReloadHandle;

	ON_SCOPE_EXIT
	{
		FAngelscriptClassGenerator::OnClassReload.Remove(ClassReloadHandle);
		FAngelscriptClassGenerator::OnStructReload.Remove(StructReloadHandle);
		FAngelscriptClassGenerator::OnPostReload.Remove(PostReloadHandle);
		Engine.DiscardModule(*TypeReloadModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	// Keep class and struct independent so this test exercises delegate payloads,
	// not nested script-struct initialization during class CDO creation.
	const FString ScriptV1 = TEXT(R"AS(
USTRUCT()
struct FHotReloadDelegatePayload
{
	UPROPERTY()
	int Value = 1;
}

UCLASS()
class UHotReloadDelegateCarrier : UObject
{
	UPROPERTY()
	int Revision = 1;
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
USTRUCT()
struct FHotReloadDelegatePayload
{
	UPROPERTY()
	int Value = 2;

	UPROPERTY()
	int Bonus = 7;
}

UCLASS()
class UHotReloadDelegateCarrier : UObject
{
	UPROPERTY()
	int Revision = 2;

	UPROPERTY()
	int Epoch = 9;
}
)AS");

	if (!TestTrue(
		TEXT("Initial class/struct reload-delegate module compile should succeed"),
		CompileAnnotatedModuleFromMemory(&Engine, TypeReloadModuleName, TypeReloadFilename, ScriptV1)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptClassDesc> StructBeforeReload = Engine.GetClass(TypeReloadStructName);
	if (!TestTrue(TEXT("Struct metadata should exist before reload"), StructBeforeReload.IsValid()))
	{
		return false;
	}

	UScriptStruct* StructObjectBeforeReload = static_cast<UScriptStruct*>(StructBeforeReload->Struct);
	if (!TestNotNull(TEXT("Struct object should exist before full reload"), StructObjectBeforeReload))
	{
		return false;
	}

	UClass* ClassObjectBeforeReload = FindGeneratedClass(&Engine, TypeReloadCarrierClassName);
	if (!TestNotNull(TEXT("Carrier class should exist before full reload"), ClassObjectBeforeReload))
	{
		return false;
	}

	ClassReloadHandle = FAngelscriptClassGenerator::OnClassReload.AddLambda(
		[&](UClass* OldClass, UClass* NewClass)
		{
			++ClassReloadCount;
			OldClassSeenDuringReload = OldClass;
			NewClassSeenDuringReload = NewClass;
		});

	StructReloadHandle = FAngelscriptClassGenerator::OnStructReload.AddLambda(
		[&](UScriptStruct* OldStruct, UScriptStruct* NewStruct)
		{
			++StructReloadCount;
			OldStructSeenDuringReload = OldStruct;
			NewStructSeenDuringReload = NewStruct;
		});

	PostReloadHandle = FAngelscriptClassGenerator::OnPostReload.AddLambda(
		[&](const bool bWasFullReload)
		{
			++PostReloadCount;
			bPostReloadSawFullReload = bWasFullReload;
			bStructVisibleDuringPostReload = Engine.GetClass(TypeReloadStructName).IsValid();
			bClassVisibleDuringPostReload = FindGeneratedClass(&Engine, TypeReloadCarrierClassName) != nullptr;
		});

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(
		TEXT("Class/struct full reload compile should succeed"),
		CompileModuleWithResult(&Engine, ECompileType::FullReload, TypeReloadModuleName, TypeReloadFilename, ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Class/struct full reload should stay on a handled reload path"),
		IsHandledReloadResult(ReloadResult)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptClassDesc> StructAfterReload = Engine.GetClass(TypeReloadStructName);
	if (!TestTrue(TEXT("Struct metadata should still exist after reload"), StructAfterReload.IsValid()))
	{
		return false;
	}

	UScriptStruct* StructObjectAfterReload = static_cast<UScriptStruct*>(StructAfterReload->Struct);
	if (!TestNotNull(TEXT("Reloaded struct object should still be queryable"), StructObjectAfterReload))
	{
		return false;
	}

	UClass* ClassObjectAfterReload = FindGeneratedClass(&Engine, TypeReloadCarrierClassName);
	if (!TestNotNull(TEXT("Reloaded carrier class should still be queryable"), ClassObjectAfterReload))
	{
		return false;
	}

	TestEqual(TEXT("Full reload should broadcast class-reload delegate exactly once"), ClassReloadCount, 1);
	TestEqual(TEXT("Full reload should broadcast struct-reload delegate exactly once"), StructReloadCount, 1);
	TestEqual(TEXT("Full reload should broadcast post-reload delegate exactly once"), PostReloadCount, 1);
	TestTrue(TEXT("Post-reload delegate should report a full reload"), bPostReloadSawFullReload);
	TestTrue(TEXT("Struct should already be queryable when post-reload broadcasts"), bStructVisibleDuringPostReload);
	TestTrue(TEXT("Class should already be queryable when post-reload broadcasts"), bClassVisibleDuringPostReload);
	TestEqual(TEXT("Struct-reload delegate should expose the old struct"), OldStructSeenDuringReload, StructObjectBeforeReload);
	TestEqual(TEXT("Struct-reload delegate should expose the new struct"), NewStructSeenDuringReload, StructObjectAfterReload);
	TestEqual(TEXT("Class-reload delegate should expose the old class"), OldClassSeenDuringReload, ClassObjectBeforeReload);
	TestEqual(TEXT("Class-reload delegate should expose the new class"), NewClassSeenDuringReload, ClassObjectAfterReload);
	TestNotEqual(TEXT("Struct-reload delegate should broadcast distinct old/new structs"), OldStructSeenDuringReload, NewStructSeenDuringReload);
	TestNotEqual(TEXT("Class-reload delegate should broadcast distinct old/new classes"), OldClassSeenDuringReload, NewClassSeenDuringReload);
	}

	return true;
}

bool FAngelscriptHotReloadReloadDelegatesBroadcastDelegateSignatureSwapTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadDelegateTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 DelegateReloadCount = 0;
	UDelegateFunction* OldDelegateSeenDuringReload = nullptr;
	UDelegateFunction* NewDelegateSeenDuringReload = nullptr;
	FDelegateHandle DelegateReloadHandle;

	ON_SCOPE_EXIT
	{
		FAngelscriptClassGenerator::OnDelegateReload.Remove(DelegateReloadHandle);
		Engine.DiscardModule(*SignatureReloadModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(delegate void FHotReloadSignal(int Value); UCLASS() class UHotReloadDelegateSignatureCarrier : UObject { UPROPERTY() FHotReloadSignal Signal; })AS");
	const FString ScriptV2 = TEXT(R"AS(delegate void FHotReloadSignal(int Value, const FString& Label); UCLASS() class UHotReloadDelegateSignatureCarrier : UObject { UPROPERTY() FHotReloadSignal Signal; })AS");
	if (!TestTrue(TEXT("Initial delegate-signature baseline compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, SignatureReloadModuleName, SignatureReloadFilename, ScriptV1)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> DelegateBeforeReload = Engine.GetDelegate(SignatureReloadDelegateName);
	UClass* CarrierClassBeforeReload = FindGeneratedClass(&Engine, SignatureReloadCarrierClassName);
	if (!TestTrue(TEXT("Delegate metadata should exist before signature reload"), DelegateBeforeReload.IsValid())
		|| !TestNotNull(TEXT("Initial delegate function should exist before signature reload"), DelegateBeforeReload.IsValid() ? DelegateBeforeReload->Function : nullptr)
		|| !TestNotNull(TEXT("Carrier class should exist before delegate signature reload"), CarrierClassBeforeReload))
	{
		return false;
	}

	TestNull(TEXT("Initial delegate signature should not expose the future Label parameter"), FindFProperty<FProperty>(DelegateBeforeReload->Function, TEXT("Label")));
	DelegateReloadHandle = FAngelscriptClassGenerator::OnDelegateReload.AddLambda(
		[&](UDelegateFunction* OldDelegate, UDelegateFunction* NewDelegate)
		{
			++DelegateReloadCount;
			OldDelegateSeenDuringReload = OldDelegate;
			NewDelegateSeenDuringReload = NewDelegate;
		});

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("Delegate signature full reload compile should succeed"), CompileModuleWithResult(&Engine, ECompileType::FullReload, SignatureReloadModuleName, SignatureReloadFilename, ScriptV2, ReloadResult)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Delegate signature reload should stay on a handled reload path"), IsHandledReloadResult(ReloadResult)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterReload = Engine.GetDelegate(SignatureReloadDelegateName);
	UClass* CarrierClassAfterReload = FindGeneratedClass(&Engine, SignatureReloadCarrierClassName);
	FDelegateProperty* DelegatePropertyAfterReload = CarrierClassAfterReload != nullptr ? FindFProperty<FDelegateProperty>(CarrierClassAfterReload, TEXT("Signal")) : nullptr;
	if (!TestTrue(TEXT("Delegate metadata should still exist after signature reload"), DelegateAfterReload.IsValid())
		|| !TestNotNull(TEXT("Reloaded delegate function should remain queryable"), DelegateAfterReload.IsValid() ? DelegateAfterReload->Function : nullptr)
		|| !TestNotNull(TEXT("Carrier class should remain queryable after delegate signature reload"), CarrierClassAfterReload)
		|| !TestNotNull(TEXT("Carrier class should keep the delegate property after reload"), DelegatePropertyAfterReload))
	{
		return false;
	}
	TestEqual(TEXT("Delegate signature reload should broadcast exactly once"), DelegateReloadCount, 1);
	TestEqual(TEXT("Delegate-reload callback should expose the old delegate function"), OldDelegateSeenDuringReload, DelegateBeforeReload->Function);
	TestEqual(TEXT("Delegate-reload callback should expose the new delegate function"), NewDelegateSeenDuringReload, DelegateAfterReload->Function);
	TestNotEqual(TEXT("Delegate-reload callback should broadcast distinct old/new delegate functions"), OldDelegateSeenDuringReload, NewDelegateSeenDuringReload);
	TestTrue(TEXT("Delegate property should retarget to the reloaded signature function"), DelegatePropertyAfterReload->SignatureFunction == DelegateAfterReload->Function);
	TestNotNull(TEXT("Reloaded delegate signature should expose the new Label parameter"), FindFProperty<FProperty>(DelegateAfterReload->Function, TEXT("Label")));
	}

	return true;
}

#endif
