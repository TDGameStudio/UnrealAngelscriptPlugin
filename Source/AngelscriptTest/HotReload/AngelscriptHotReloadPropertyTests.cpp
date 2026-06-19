#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Misc/AutomationTest.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_HotReload_AngelscriptHotReloadPropertyTests_Private
{
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool SoftReloadBasic(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadPropertyTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class USoftReloadTarget : UObject
{
	UPROPERTY()
	int Version;

	default Version = 1;

	UFUNCTION()
	int GetVersion()
	{
		return Version;
	}
}

int GetSoftReloadVersion()
{
	return 1;
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class USoftReloadTarget : UObject
{
	UPROPERTY()
	int Version;

	default Version = 1;

	UFUNCTION()
	int GetVersion()
	{
		return Version + 1;
	}
}

int GetSoftReloadVersion()
{
	return 2;
}
)AS");

	if (!TestTrue(TEXT("Initial soft-reload module compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("SoftReloadMod"), TEXT("SoftReloadMod.as"), ScriptV1)))
	{
		return false;
	}

	UClass* ClassV1 = FindGeneratedClass(&Engine, TEXT("USoftReloadTarget"));
	if (!TestNotNull(TEXT("Soft reload target class should exist before reload"), ClassV1))
	{
		return false;
	}

	UFunction* GetVersionV1 = FindGeneratedFunction(ClassV1, TEXT("GetVersion"));
	TestNotNull(TEXT("GetVersion should exist before soft reload"), GetVersionV1);

	int32 BeforeReloadResult = 0;
	if (!TestTrue(TEXT("Global version function should execute before soft reload"), ExecuteIntFunction(&Engine, TEXT("SoftReloadMod"), TEXT("int GetSoftReloadVersion()"), BeforeReloadResult)))
	{
		return false;
	}
	TestEqual(TEXT("Global version should return v1 before soft reload"), BeforeReloadResult, 1);

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("SoftReload compile wrapper should succeed"), CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, TEXT("SoftReloadMod"), TEXT("SoftReloadMod.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(TEXT("SoftReload compile should remain on the soft reload path"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassAfterReload = FindGeneratedClass(&Engine, TEXT("USoftReloadTarget"));
	if (!TestNotNull(TEXT("Soft reload target class should exist after reload"), ClassAfterReload))
	{
		return false;
	}

	TestEqual(TEXT("Soft reload should preserve the generated UClass instance"), ClassAfterReload, ClassV1);

	UFunction* GetVersionAfterReload = FindGeneratedFunction(ClassAfterReload, TEXT("GetVersion"));
	TestNotNull(TEXT("GetVersion should exist after soft reload"), GetVersionAfterReload);

	int32 AfterReloadResult = 0;
	if (!TestTrue(TEXT("Global version function should execute after soft reload"), ExecuteIntFunction(&Engine, TEXT("SoftReloadMod"), TEXT("int GetSoftReloadVersion()"), AfterReloadResult)))
	{
		return false;
	}
	TestEqual(TEXT("Global version should return v2 after soft reload"), AfterReloadResult, 2);
	bPassed = AfterReloadResult == 2;
	}

	return bPassed;
}

static bool SoftReloadPreservesOtherModules(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadPropertyTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptA = TEXT(R"AS(
int GetValueA()
{
	return 10;
}
)AS");
	const FString ScriptB = TEXT(R"AS(
int GetValueB()
{
	return 20;
}
)AS");
	if (!TestTrue(TEXT("Module A should compile before soft reload preservation test"), CompileModuleFromMemory(&Engine, TEXT("SoftPreserveA"), TEXT("SoftPreserveA.as"), ScriptA)) ||

		!TestTrue(TEXT("Module B should compile before soft reload preservation test"), CompileModuleFromMemory(&Engine, TEXT("SoftPreserveB"), TEXT("SoftPreserveB.as"), ScriptB)))
	{
		return false;
	}

	int32 ResultA = 0;
	int32 ResultB = 0;
	if (!TestTrue(TEXT("Module A should execute before reload"), ExecuteIntFunction(&Engine, TEXT("SoftPreserveA"), TEXT("int GetValueA()"), ResultA)) ||
		!TestTrue(TEXT("Module B should execute before reload"), ExecuteIntFunction(&Engine, TEXT("SoftPreserveB"), TEXT("int GetValueB()"), ResultB)))
	{
		return false;
	}
	TestEqual(TEXT("Module A should return its initial value before reload"), ResultA, 10);
	TestEqual(TEXT("Module B should return its initial value before reload"), ResultB, 20);

	const FString ScriptAV2 = TEXT(R"AS(
int GetValueA()
{
	return 11;
}
)AS");

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("Soft reload of module A should succeed"), CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, TEXT("SoftPreserveA"), TEXT("SoftPreserveA.as"), ScriptAV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Soft reload of module A should remain in soft reload path"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	ResultA = 0;
	ResultB = 0;
	if (!TestTrue(TEXT("Module A should execute after reload"), ExecuteIntFunction(&Engine, TEXT("SoftPreserveA"), TEXT("int GetValueA()"), ResultA)) ||
		!TestTrue(TEXT("Module B should still execute after module A reload"), ExecuteIntFunction(&Engine, TEXT("SoftPreserveB"), TEXT("int GetValueB()"), ResultB)))
	{
		return false;
	}

	TestEqual(TEXT("Module A should reflect its reloaded implementation"), ResultA, 11);
	TestEqual(TEXT("Module B should preserve its original implementation"), ResultB, 20);
	bPassed = ResultA == 11 && ResultB == 20;
	}

	return bPassed;
}

static bool FullReloadBasic(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadPropertyTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UFullReloadTarget : UObject
{
	UPROPERTY()
	int Version;

	default Version = 1;

	UFUNCTION()
	int GetVersion()
	{
		return Version;
	}
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UFullReloadTarget : UObject
{
	UPROPERTY()
	int Version;

	UPROPERTY()
	int Mana;

	default Version = 2;
	default Mana = 5;

	UFUNCTION()
	int GetVersion()
	{
		return Version;
	}

	UFUNCTION()
	int GetMana()
	{
		return Mana;
	}
}
)AS");

	if (!TestTrue(TEXT("Initial full-reload module compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("FullReloadMod"), TEXT("FullReloadMod.as"), ScriptV1)))
	{
		return false;
	}

	UClass* ClassV1 = FindGeneratedClass(&Engine, TEXT("UFullReloadTarget"));
	if (!TestNotNull(TEXT("Full reload target class should exist before reload"), ClassV1))
	{
		return false;
	}

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("Full reload compile wrapper should succeed"), CompileModuleWithResult(&Engine, ECompileType::FullReload, TEXT("FullReloadMod"), TEXT("FullReloadMod.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Full reload compile should be handled without fatal compile error"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	UClass* ClassV2 = FindGeneratedClass(&Engine, TEXT("UFullReloadTarget"));
	if (!TestNotNull(TEXT("Full reload target class should exist after reload"), ClassV2))
	{
		return false;
	}

	FIntProperty* VersionProperty = FindFProperty<FIntProperty>(ClassV2, TEXT("Version"));
	FIntProperty* ManaProperty = FindFProperty<FIntProperty>(ClassV2, TEXT("Mana"));
	UFunction* GetVersionFunction = FindGeneratedFunction(ClassV2, TEXT("GetVersion"));
	UFunction* GetManaFunction = FindGeneratedFunction(ClassV2, TEXT("GetMana"));
	if (!TestNotNull(TEXT("Version property should exist after full reload"), VersionProperty) ||
		!TestNotNull(TEXT("Mana property should exist after full reload"), ManaProperty) ||
		!TestNotNull(TEXT("GetVersion should exist after full reload"), GetVersionFunction) ||
		!TestNotNull(TEXT("GetMana should exist after full reload"), GetManaFunction))
	{
		return false;
	}

	UObject* ObjV2 = NewObject<UObject>(GetTransientPackage(), ClassV2);
	if (!TestNotNull(TEXT("Full reload target object should instantiate after reload"), ObjV2))
	{
		return false;
	}

	TestEqual(TEXT("Version default should update after full reload"), VersionProperty->GetPropertyValue_InContainer(ObjV2), 2);
	TestEqual(TEXT("Mana default should be introduced after full reload"), ManaProperty->GetPropertyValue_InContainer(ObjV2), 5);
	}

	return true;
}

static bool FullReloadEnumBasic(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_HotReload_AngelscriptHotReloadPropertyTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("FullReloadEnumMod"));
	};

	const FString ScriptV1 = TEXT(R"AS(
UENUM(BlueprintType)
enum class EFullReloadEnumState : uint16
{
	Alpha,
	Beta = 4
}

UCLASS()
class UFullReloadEnumTarget : UObject
{
	UPROPERTY()
	EFullReloadEnumState State;

	default State = EFullReloadEnumState::Alpha;
}
)AS");
	const FString ScriptV2 = TEXT(R"AS(
UENUM(BlueprintType)
enum class EFullReloadEnumState : uint16
{
	Alpha,
	Beta = 4,
	Gamma = 9
}

UCLASS()
class UFullReloadEnumTarget : UObject
{
	UPROPERTY()
	EFullReloadEnumState State;

	default State = EFullReloadEnumState::Gamma;
}
)AS");

	if (!TestTrue(TEXT("Initial enum full-reload module compile should succeed"), CompileAnnotatedModuleFromMemory(&Engine, TEXT("FullReloadEnumMod"), TEXT("FullReloadEnumMod.as"), ScriptV1)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptEnumDesc> EnumBeforeReload = Engine.GetEnum(TEXT("EFullReloadEnumState"));
	if (!TestTrue(TEXT("Enum metadata should exist before full reload"), EnumBeforeReload.IsValid()))
	{
		return false;
	}

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(TEXT("Full reload for enum-bearing module should succeed"), CompileModuleWithResult(&Engine, ECompileType::FullReload, TEXT("FullReloadEnumMod"), TEXT("FullReloadEnumMod.as"), ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Enum full reload should complete without fatal compile error"), ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptEnumDesc> EnumAfterReload = Engine.GetEnum(TEXT("EFullReloadEnumState"));
	if (!TestTrue(TEXT("Enum metadata should still exist after full reload"), EnumAfterReload.IsValid()))
	{
		return false;
	}

	UClass* ReloadedClass = FindGeneratedClass(&Engine, TEXT("UFullReloadEnumTarget"));
	if (!TestNotNull(TEXT("Enum carrier class should still exist after full reload"), ReloadedClass))
	{
		return false;
	}

	TestNotNull(TEXT("Enum-backed property should still exist after full reload"), FindFProperty<FProperty>(ReloadedClass, TEXT("State")));
	}

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(FAngelscriptSoftReloadTests,
	"Angelscript.TestModule.HotReload.SoftReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Basic)
	{
		ASSERT_THAT(IsTrue(SoftReloadBasic(*TestRunner)));
	}

	TEST_METHOD(PreservesOtherModules)
	{
		ASSERT_THAT(IsTrue(SoftReloadPreservesOtherModules(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptFullReloadTests,
	"Angelscript.TestModule.HotReload.FullReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Basic)
	{
		ASSERT_THAT(IsTrue(FullReloadBasic(*TestRunner)));
	}

	TEST_METHOD(EnumBasic)
	{
		ASSERT_THAT(IsTrue(FullReloadEnumBasic(*TestRunner)));
	}
};

#endif
