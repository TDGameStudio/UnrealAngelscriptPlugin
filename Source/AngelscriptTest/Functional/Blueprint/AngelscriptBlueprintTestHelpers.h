#pragma once

#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptBlueprintTestUtils
{
	using namespace AngelscriptFunctionalTestUtils;

	// -----------------------------------------------------------------
	// Blueprint creation helpers
	// -----------------------------------------------------------------

	inline UBlueprint* CreateTransientBlueprintChild(
		FAutomationTestBase& Test,
		UClass* ParentClass,
		FStringView Suffix,
		const TCHAR* CallingContext = TEXT("AngelscriptBlueprintTests"))
	{
		if (!Test.TestNotNull(TEXT("Blueprint parent class should be valid"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptBP_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("Blueprint package should be created"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			CallingContext);
		if (!Test.TestNotNull(TEXT("Blueprint asset should be created"), Blueprint))
		{
			return nullptr;
		}

		return Blueprint;
	}

	inline bool CompileAndValidateBlueprint(FAutomationTestBase& Test, UBlueprint& Blueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		return Test.TestNotNull(
			TEXT("Blueprint should compile to a generated class"),
			Blueprint.GeneratedClass.Get());
	}

	inline void CleanupBlueprint(UBlueprint*& Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		// After GC the C++ pointer can remain non-null while the
		// UObject's InternalIndex is -1. IsValidLowLevel() catches
		// these stale pointers that IsValid() still approves.
		if (!Blueprint->IsValidLowLevel())
		{
			Blueprint = nullptr;
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			if (BlueprintClass->IsValidLowLevel())
			{
				BlueprintClass->MarkAsGarbage();
			}
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			if (BlueprintPackage->IsValidLowLevel())
			{
				BlueprintPackage->MarkAsGarbage();
			}
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	// -----------------------------------------------------------------
	// RAII Blueprint wrapper — auto-cleanup on scope exit
	// -----------------------------------------------------------------

	struct FScopedTransientBlueprint
	{
		UBlueprint* Blueprint = nullptr;

		~FScopedTransientBlueprint()
		{
			CleanupBlueprint(Blueprint);
		}

		bool CreateAndCompile(
			FAutomationTestBase& Test,
			UClass* ParentClass,
			FStringView Suffix,
			const TCHAR* CallingContext = TEXT("AngelscriptBlueprintTests"))
		{
			Blueprint = CreateTransientBlueprintChild(Test, ParentClass, Suffix, CallingContext);
			return Blueprint != nullptr && CompileAndValidateBlueprint(Test, *Blueprint);
		}

		UClass* GetGeneratedClass() const
		{
			return Blueprint != nullptr ? Blueprint->GeneratedClass.Get() : nullptr;
		}
	};

	// -----------------------------------------------------------------
	// RAII World wrapper — encapsulates FActorTestSpawner lifecycle
	// -----------------------------------------------------------------

	struct FScopedBlueprintWorld
	{
		explicit FScopedBlueprintWorld(FAutomationTestBase& InTest)
			: Test(InTest)
		{
			Spawner.InitializeGameSubsystems();
			World = &Spawner.GetWorld();
			bIsValid = Test.TestNotNull(TEXT("Blueprint test world should be created"), World);
		}

		bool IsValid() const { return bIsValid && World != nullptr; }

		UWorld& GetWorld() const { return *World; }

		template <typename ActorType = AActor>
		ActorType* SpawnActorOfClass(
			UClass* ActorClass,
			const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters(),
			const FVector& Location = FVector::ZeroVector,
			const FRotator& Rotation = FRotator::ZeroRotator)
		{
			if (!Test.TestNotNull(TEXT("Actor class should be valid for spawning"), ActorClass) || !IsValid())
			{
				return nullptr;
			}
			return &Spawner.SpawnActorAt<ActorType>(Location, Rotation, SpawnParameters, ActorClass);
		}

		void BeginPlay(FAngelscriptEngine& Engine, AActor& Actor) const
		{
			AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, Actor);
		}

		void Tick(FAngelscriptEngine& Engine, float DeltaTime, int32 NumTicks) const
		{
			if (IsValid())
			{
				AngelscriptFunctionalTestUtils::TickWorld(Engine, *World, DeltaTime, NumTicks);
			}
		}

	private:
		FAutomationTestBase& Test;
		FActorTestSpawner Spawner;
		UWorld* World = nullptr;
		bool bIsValid = false;
	};

	// -----------------------------------------------------------------
	// Script function invocation helpers
	// -----------------------------------------------------------------

	struct FSingleIntParam
	{
		int32 Value = 0;
	};

	inline bool InvokeNoParamScriptFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		FName FunctionName,
		const TCHAR* Context)
	{
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should have a valid object"), Context), Object))
		{
			return false;
		}

		UFunction* Function = Object->FindFunction(FunctionName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("%s should expose function '%s'"), Context, *FunctionName.ToString()),
			Function))
		{
			return false;
		}

		FAngelscriptEngineScope Scope(Engine, Object);
		Object->ProcessEvent(Function, nullptr);
		return true;
	}

	inline bool InvokeIntScriptFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		FName FunctionName,
		int32 Value,
		const TCHAR* Context)
	{
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should have a valid object"), Context), Object))
		{
			return false;
		}

		UFunction* Function = Object->FindFunction(FunctionName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("%s should expose function '%s'"), Context, *FunctionName.ToString()),
			Function))
		{
			return false;
		}

		FSingleIntParam Params;
		Params.Value = Value;

		FAngelscriptEngineScope Scope(Engine, Object);
		Object->ProcessEvent(Function, &Params);
		return true;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
