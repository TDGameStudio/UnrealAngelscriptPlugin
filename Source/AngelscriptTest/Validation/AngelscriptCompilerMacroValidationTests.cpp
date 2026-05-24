#include "Shared/AngelscriptTestMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerEnumMacroValidationTest,
	"Angelscript.TestModule.Validation.CompilerEnumMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerDelegateMacroValidationTest,
	"Angelscript.TestModule.Validation.CompilerDelegateMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerEnumMacroValidationTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerEnumAvailabilityMacro"),
		TEXT("CompilerEnumAvailabilityMacro.as"),
		TEXT(R"(
UENUM(BlueprintType)
enum class ECompilerMacroAvailabilityState : uint16
{
	Alpha,
	Beta = 4,
	Gamma
}
)"));

	if (!TestTrue(TEXT("Enum availability input via macro should compile"), bCompiled))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptEnumDesc> EnumDesc = Engine.GetEnum(TEXT("ECompilerMacroAvailabilityState"));
	if (!TestTrue(TEXT("Compiled enum metadata should be registered"), EnumDesc.IsValid()))
	{
		return false;
	}

	bPassed =
		TestEqual(TEXT("Compiled enum should have 3 declared values"), EnumDesc->ValueNames.Num(), 3)
		&& TestEqual(TEXT("Beta should have explicit value 4"), static_cast<int32>(EnumDesc->EnumValues[1]), 4);

	}
	return bPassed;
}

bool FAngelscriptCompilerDelegateMacroValidationTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	const bool bCompiled = CompileAnnotatedModuleFromMemory(
		&Engine,
		TEXT("CompilerDelegateSignatureMacro"),
		TEXT("CompilerDelegateSignatureMacro.as"),
		TEXT(R"(
delegate void FCompilerSingleCastSignature(int Value);
event void FCompilerMultiCastSignature(UClass TypeValue, FString Label);

UCLASS()
class UCompilerDelegateCarrier : UObject
{
}
)"));

	if (!TestTrue(TEXT("Delegate signature compilation via macro should succeed"), bCompiled))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> SingleCast = Engine.GetDelegate(TEXT("FCompilerSingleCastSignature"));
	const TSharedPtr<FAngelscriptDelegateDesc> MultiCast = Engine.GetDelegate(TEXT("FCompilerMultiCastSignature"));
	if (!TestTrue(TEXT("Single-cast delegate metadata should exist"), SingleCast.IsValid()))
	{
		return false;
	}
	if (!TestTrue(TEXT("Multicast delegate metadata should exist"), MultiCast.IsValid()))
	{
		return false;
	}

	bPassed =
		TestFalse(TEXT("Single-cast delegate should not be marked multicast"), SingleCast->bIsMulticast)
		&& TestTrue(TEXT("Multicast delegate should be marked multicast"), MultiCast->bIsMulticast)
		&& TestNotNull(TEXT("Single-cast delegate should materialize a UDelegateFunction"), SingleCast->Function)
		&& TestNotNull(TEXT("Multicast delegate should materialize a UDelegateFunction"), MultiCast->Function);

	}
	return bPassed;
}

#endif
