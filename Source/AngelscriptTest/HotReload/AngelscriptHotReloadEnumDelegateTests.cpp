#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_HotReload_AngelscriptHotReloadEnumDelegateTests_Private
{
	static const FName EnumCreatedWarmupModuleName(TEXT("HotReloadEnumCreatedWarmupMod"));
	static const FString EnumCreatedWarmupFilename(TEXT("HotReloadEnumCreatedWarmupMod.as"));
	static const FName EnumCreatedModuleName(TEXT("HotReloadEnumCreatedMod"));
	static const FString EnumCreatedFilename(TEXT("HotReloadEnumCreatedMod.as"));
	static const FString EnumCreatedName(TEXT("EHotReloadCreatedState"));
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool ReloadDelegatesBroadcastEnumCreatedOnFirstCompile(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadEnumDelegateTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 EnumCreatedCount = 0;
	int32 EnumChangedCount = 0;
	UEnum* EnumCreatedDuringCompile = nullptr;
	FString EnumCreatedNameDuringCompile;

	FDelegateHandle EnumCreatedHandle;
	FDelegateHandle EnumChangedHandle;

	ON_SCOPE_EXIT
	{
		Engine.GetOnEnumCreated().Remove(EnumCreatedHandle);
		Engine.GetOnEnumChanged().Remove(EnumChangedHandle);
		Engine.DiscardModule(*EnumCreatedModuleName.ToString());
		Engine.DiscardModule(*EnumCreatedWarmupModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString WarmupScript = TEXT(R"AS(
UCLASS()
class UEnumCreatedWarmupCarrier : UObject
{
	UPROPERTY()
	int Revision = 1;
}
)AS");

	const FString EnumScript = TEXT(R"AS(
UENUM(BlueprintType)
enum class EHotReloadCreatedState : uint8
{
	Alpha,
	Beta
}
)AS");

	if (!TestTrue(
		TEXT("Enum-created delegate test warmup compile should succeed"),
		CompileAnnotatedModuleFromMemory(&Engine, EnumCreatedWarmupModuleName, EnumCreatedWarmupFilename, WarmupScript)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Warmup compile should mark the initial compile as finished"), Engine.IsInitialCompileFinished()))
	{
		return false;
	}

	EnumCreatedHandle = Engine.GetOnEnumCreated().AddLambda(
		[&](UEnum* Enum)
		{
			++EnumCreatedCount;
			EnumCreatedDuringCompile = Enum;
			EnumCreatedNameDuringCompile = Enum != nullptr ? Enum->GetName() : FString();
		});

	EnumChangedHandle = Engine.GetOnEnumChanged().AddLambda(
		[&](UEnum* Enum, EnumNameList OldNames)
		{
			++EnumChangedCount;
		});

	if (!TestTrue(
		TEXT("First enum-declaring module compile should succeed"),
		CompileAnnotatedModuleFromMemory(&Engine, EnumCreatedModuleName, EnumCreatedFilename, EnumScript)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptEnumDesc> CreatedEnumDesc = Engine.GetEnum(EnumCreatedName);
	if (!TestTrue(TEXT("Engine should register the created enum after the first compile"), CreatedEnumDesc.IsValid()))
	{
		return false;
	}

	if (!TestNotNull(TEXT("Engine should expose a live UEnum for the created enum"), CreatedEnumDesc->Enum))
	{
		return false;
	}

	TestEqual(TEXT("OnEnumCreated should broadcast once for the first created enum"), EnumCreatedCount, 1);
	TestEqual(TEXT("OnEnumChanged should not broadcast when the enum is first created"), EnumChangedCount, 0);
	TestEqual(TEXT("OnEnumCreated should broadcast the created enum name"), EnumCreatedNameDuringCompile, EnumCreatedName);
	TestEqual(TEXT("OnEnumCreated should expose the same enum object registered on the engine"), EnumCreatedDuringCompile, CreatedEnumDesc->Enum);
	}

	return true;
}

#undef TestTrue
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadEnumDelegateTests,
	"Angelscript.TestModule.HotReload.ReloadDelegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BroadcastEnumCreatedOnFirstCompile)
	{
		ASSERT_THAT(IsTrue(ReloadDelegatesBroadcastEnumCreatedOnFirstCompile(*TestRunner)));
	}
};

#endif
