#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerMacroValidationTest,
	"Angelscript.TestModule.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompilerEnumMacro)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Enum availability input via macro should compile")));

		const TSharedPtr<FAngelscriptEnumDesc> EnumDesc = Engine.GetEnum(TEXT("ECompilerMacroAvailabilityState"));
		ASSERT_THAT(IsTrue(EnumDesc.IsValid(), TEXT("Compiled enum metadata should be registered")));
		ASSERT_THAT(AreEqual(3, EnumDesc->ValueNames.Num(), TEXT("Compiled enum should have 3 declared values")));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EnumDesc->EnumValues[1]), TEXT("Beta should have explicit value 4")));
	}

	TEST_METHOD(CompilerDelegateMacro)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Delegate signature compilation via macro should succeed")));

		const TSharedPtr<FAngelscriptDelegateDesc> SingleCast = Engine.GetDelegate(TEXT("FCompilerSingleCastSignature"));
		const TSharedPtr<FAngelscriptDelegateDesc> MultiCast = Engine.GetDelegate(TEXT("FCompilerMultiCastSignature"));
		ASSERT_THAT(IsTrue(SingleCast.IsValid(), TEXT("Single-cast delegate metadata should exist")));
		ASSERT_THAT(IsTrue(MultiCast.IsValid(), TEXT("Multicast delegate metadata should exist")));
		ASSERT_THAT(IsFalse(SingleCast->bIsMulticast, TEXT("Single-cast delegate should not be marked multicast")));
		ASSERT_THAT(IsTrue(MultiCast->bIsMulticast, TEXT("Multicast delegate should be marked multicast")));
		ASSERT_THAT(IsNotNull(SingleCast->Function, TEXT("Single-cast delegate should materialize a UDelegateFunction")));
		ASSERT_THAT(IsNotNull(MultiCast->Function, TEXT("Multicast delegate should materialize a UDelegateFunction")));
	}
};

#endif
