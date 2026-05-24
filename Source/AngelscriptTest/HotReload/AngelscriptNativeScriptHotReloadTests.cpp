#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Angelscript_AngelscriptNativeScriptHotReloadTests_Private
{
	bool VerifyNativeScriptHotReloadInline(
		FAutomationTestBase& Test,
		const TCHAR* GroupLabel,
		const TArray<TPair<FString, FString>>& InlineScripts)
	{
		FAngelscriptEngine* ProductionEngine = RequireRunningProductionEngine(Test, TEXT("Native script hot reload tests require a production engine."));
		if (ProductionEngine == nullptr)
		{
			return false;
		}

		FAngelscriptEngine& Engine = *ProductionEngine;

		for (int32 Index = 0; Index < InlineScripts.Num(); ++Index)
		{
			const FString& Filename = InlineScripts[Index].Key;
			const FString& Source   = InlineScripts[Index].Value;

			const FName ModuleName(*FPaths::GetBaseFilename(Filename));
			ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

			ECompileResult InitialCompileResult = ECompileResult::Error;
			if (!Test.TestTrue(
				FString::Printf(TEXT("%s should compile '%s' before reload"), GroupLabel, *Filename),
				CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, Filename, Source, InitialCompileResult)))
			{
				return false;
			}

			FString ReloadSource = Source;
			ReloadSource += TEXT("\n// hot reload verification marker\n");
			ECompileResult ReloadCompileResult = ECompileResult::Error;
			if (!Test.TestTrue(
				FString::Printf(TEXT("%s should hot reload '%s' through the compile wrapper"), GroupLabel, *Filename),
				CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ModuleName, Filename, ReloadSource, ReloadCompileResult)))
			{
				return false;
			}

			if (!Test.TestTrue(
				FString::Printf(TEXT("%s should keep '%s' on a handled reload path"), GroupLabel, *Filename),
				ReloadCompileResult == ECompileResult::FullyHandled || ReloadCompileResult == ECompileResult::PartiallyHandled))
			{
				return false;
			}
		}

		return true;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeScriptHotReloadPhase2ATest,
	"Angelscript.TestModule.HotReload.NativeScript.Phase2A",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeScriptHotReloadPhase2BTest,
	"Angelscript.TestModule.HotReload.NativeScript.Phase2B",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeScriptHotReloadPhase2CTest,
	"Angelscript.TestModule.HotReload.NativeScript.Phase2C",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNativeScriptHotReloadPhase2ATest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptNativeScriptHotReloadTests_Private;
	return VerifyNativeScriptHotReloadInline(
		*this,
		TEXT("Phase2A"),
		{
			{
				TEXT("HotReloadPhase2AEnum.as"),
				TEXT(R"AS(
UENUM()
enum class ENativeHotReloadPhase2AEnum : uint8
{
	ValueA,
	ValueB,
	ValueC,
};
)AS")
			},
			{
				TEXT("HotReloadPhase2AInheritance.as"),
				TEXT(R"AS(
UCLASS()
class UNativeHotReloadPhase2ABase : UObject
{
	UFUNCTION(BlueprintEvent)
	int GetValue()
	{
		return 10;
	}
};

UCLASS()
class UNativeHotReloadPhase2ADerived : UNativeHotReloadPhase2ABase
{
	UFUNCTION(BlueprintOverride)
	int GetValue()
	{
		return 20;
	}
};
)AS")
			},
			{
				TEXT("HotReloadPhase2AHandles.as"),
				TEXT(R"AS(
UCLASS()
class UNativeHotReloadPhase2AHandleCarrier : UObject
{
	UPROPERTY()
	UObject HandleRef = nullptr;

	UFUNCTION()
	bool HasHandle()
	{
		return HandleRef != nullptr;
	}
};
)AS")
			},
		});
}

bool FAngelscriptNativeScriptHotReloadPhase2BTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptNativeScriptHotReloadTests_Private;
	return VerifyNativeScriptHotReloadInline(
		*this,
		TEXT("Phase2B"),
		{
			{
				TEXT("HotReloadPhase2BTagCarrier.as"),
				TEXT(R"AS(
UCLASS()
class UNativeHotReloadPhase2BTagCarrier : UObject
{
	UPROPERTY()
	TArray<FName> Tags;

	UFUNCTION()
	void AddTag(FName Tag)
	{
		Tags.Add(Tag);
	}

	UFUNCTION()
	bool HasTag(FName Tag) const
	{
		return Tags.Contains(Tag);
	}
};
)AS")
			},
			{
				TEXT("HotReloadPhase2BSystemUtils.as"),
				TEXT(R"AS(
UCLASS()
class UNativeHotReloadPhase2BSystemUtils : UObject
{
	UFUNCTION()
	int Clamp(int Value, int Min, int Max)
	{
		if (Value < Min) return Min;
		if (Value > Max) return Max;
		return Value;
	}

	UFUNCTION()
	float Lerp(float A, float B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}
};
)AS")
			},
			{
				TEXT("HotReloadPhase2BActorLifecycle.as"),
				TEXT(R"AS(
UCLASS()
class ANativeHotReloadPhase2BActor : AActor
{
	UPROPERTY()
	int LifecycleCounter = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		LifecycleCounter += 1;
	}

	UFUNCTION(BlueprintOverride)
	void EndPlay(EEndPlayReason Reason)
	{
		LifecycleCounter = 0;
	}
};
)AS")
			},
			{
				TEXT("HotReloadPhase2BMathNamespace.as"),
				TEXT(R"AS(
namespace NativeHotReloadMath
{
	float Square(float X) { return X * X; }
	float Cube(float X) { return X * X * X; }
}

UCLASS()
class UNativeHotReloadPhase2BMathCarrier : UObject
{
	UFUNCTION()
	float ComputeSquare(float X)
	{
		return NativeHotReloadMath::Square(X);
	}
};
)AS")
			},
		});
}

bool FAngelscriptNativeScriptHotReloadPhase2CTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptNativeScriptHotReloadTests_Private;
	const FString RelativeFilename = TEXT("Script/Tests/Test_ExampleActorFixture.as");
	const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeFilename);
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *AbsoluteFilename))
	{
		AddError(FString::Printf(TEXT("Phase2C should load source from %s"), *RelativeFilename));
		return false;
	}

	return VerifyNativeScriptHotReloadInline(
		*this,
		TEXT("Phase2C"),
		{
			TPair<FString, FString>(RelativeFilename, Source),
		});
}

#endif
