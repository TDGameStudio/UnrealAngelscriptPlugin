#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASStruct.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace ScriptStructHotReloadTest
{
	static const FName ModuleName(TEXT("ScriptStructHotReloadVersionChain"));
	static const FName UnrealStructName(TEXT("ScriptStructHotReloadVersionChain"));
	static const FString ScriptFilename(TEXT("ScriptStructHotReloadVersionChain.as"));

	FString GetScriptAbsoluteFilename()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), ScriptFilename);
	}

	UASStruct* FindStructByName(const FName StructName)
	{
		return FindObject<UASStruct>(FAngelscriptEngine::GetPackage(), *StructName.ToString());
	}

	UASStruct* FindCurrentStruct()
	{
		return FindStructByName(UnrealStructName);
	}

	FProperty* FindStructProperty(UASStruct* Struct, const FName PropertyName)
	{
		return Struct != nullptr ? Struct->FindPropertyByName(PropertyName) : nullptr;
	}

	bool VerifyHandledReloadResult(FAutomationTestBase& Test, const TCHAR* Context, const ECompileResult ReloadResult)
	{
		return Test.TestTrue(
			Context,
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled);
	}
}

namespace ScriptStructCustomGuidTest
{
	static const FName StableModuleName(TEXT("ScriptStructCustomGuidStable"));
	static const FName StableStructName(TEXT("StableGuidStruct"));
	static const FString StableScriptFilename(TEXT("ScriptStructCustomGuidStable.as"));
	static const FName DifferentModuleName(TEXT("ScriptStructCustomGuidDifferent"));
	static const FName DifferentStructName(TEXT("DifferentGuidStruct"));
	static const FString DifferentScriptFilename(TEXT("ScriptStructCustomGuidDifferent.as"));

	FString GetScriptAbsoluteFilename(const FString& InScriptFilename)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), InScriptFilename);
	}

	FString GetStableScriptAbsoluteFilename()
	{
		return GetScriptAbsoluteFilename(StableScriptFilename);
	}

	FString GetDifferentScriptAbsoluteFilename()
	{
		return GetScriptAbsoluteFilename(DifferentScriptFilename);
	}

	UASStruct* FindStableStruct()
	{
		return ScriptStructHotReloadTest::FindStructByName(StableStructName);
	}

	UASStruct* FindDifferentStruct()
	{
		return ScriptStructHotReloadTest::FindStructByName(DifferentStructName);
	}
}

namespace ScriptStructCapabilityReloadTest
{
	static const FName ModuleName(TEXT("ScriptStructCapabilityReload"));
	static const FName StructName(TEXT("ReloadableCapabilityStruct"));
	static const FString ScriptFilename(TEXT("ScriptStructCapabilityReload.as"));

	FString GetScriptAbsoluteFilename()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), ScriptFilename);
	}

	UASStruct* FindCurrentStruct()
	{
		return ScriptStructHotReloadTest::FindStructByName(StructName);
	}

	bool VerifyCapabilityState(
		FAutomationTestBase& Test,
		UASStruct* Struct,
		const TCHAR* StageLabel,
		bool bExpectIdentical,
		bool bExpectHash)
	{
		const FString StructMessage = FString::Printf(TEXT("%s should publish a script struct"), StageLabel);
		if (!Test.TestNotNull(*StructMessage, Struct))
		{
			return false;
		}

		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		const FString OpsMessage = FString::Printf(TEXT("%s should expose cpp struct ops"), StageLabel);
		if (!Test.TestNotNull(*OpsMessage, CppStructOps))
		{
			return false;
		}

		const bool bStructFlagMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should keep the expected STRUCT_IdenticalNative flag"), StageLabel),
			EnumHasAnyFlags(Struct->StructFlags, STRUCT_IdenticalNative),
			bExpectIdentical);
		const bool bHasIdenticalMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should keep the expected cpp-ops identical capability"), StageLabel),
			CppStructOps->HasIdentical(),
			bExpectIdentical);
		const bool bHasTypeHashMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should keep the expected cpp-ops hash capability"), StageLabel),
			CppStructOps->HasGetTypeHash(),
			bExpectHash);
		const bool bComputedPropertyFlagMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should keep the expected CPF_HasGetValueTypeHash computed property flag"), StageLabel),
			EnumHasAnyFlags(CppStructOps->GetComputedPropertyFlags(), CPF_HasGetValueTypeHash),
			bExpectHash);
		return bStructFlagMatches
			&& bHasIdenticalMatches
			&& bHasTypeHashMatches
			&& bComputedPropertyFlagMatches;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptStructHotReloadTests,
	"Angelscript.TestModule.ClassGenerator.ASStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GetNewestVersionAfterFullReload)
	{
		using namespace ScriptStructHotReloadTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ScriptStructHotReloadTest::ModuleName.ToString());
			IFileManager::Get().Delete(*ScriptStructHotReloadTest::GetScriptAbsoluteFilename(), false, true, true);
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
USTRUCT()
struct FScriptStructHotReloadVersionChain
{
	UPROPERTY()
	int Value = 1;
};
)AS");
		const FString ScriptV2 = TEXT(R"AS(
USTRUCT()
struct FScriptStructHotReloadVersionChain
{
	UPROPERTY()
	int Value = 1;

	UPROPERTY()
	int AddedValue = 2;
};
)AS");
		const FString ScriptV3 = TEXT(R"AS(
USTRUCT()
struct FScriptStructHotReloadVersionChain
{
	UPROPERTY()
	int Value = 1;

	UPROPERTY()
	int AddedValue = 2;

	UPROPERTY()
	int TailValue = 3;
};
)AS");

		if (!TestRunner->TestTrue(TEXT("Initial script struct compile should succeed"),
			CompileAnnotatedModuleFromMemory(&Engine, ScriptStructHotReloadTest::ModuleName, ScriptStructHotReloadTest::ScriptFilename, ScriptV1)))
		{ return; }

		UASStruct* FirstVersion = ScriptStructHotReloadTest::FindCurrentStruct();
		if (!TestRunner->TestNotNull(TEXT("Initial script struct should be registered in the Angelscript package"), FirstVersion)) { return; }
		if (!TestRunner->TestNotNull(TEXT("Initial script struct should expose the original reflected property"), ScriptStructHotReloadTest::FindStructProperty(FirstVersion, TEXT("Value")))) { return; }
		TestRunner->TestNull(TEXT("Initial script struct should not expose the first added property before reload"), ScriptStructHotReloadTest::FindStructProperty(FirstVersion, TEXT("AddedValue")));
		TestRunner->TestNull(TEXT("Initial script struct should not expose the second added property before reload"), ScriptStructHotReloadTest::FindStructProperty(FirstVersion, TEXT("TailValue")));

		ECompileResult ReloadResultV2 = ECompileResult::Error;
		if (!TestRunner->TestTrue(TEXT("First structural script struct reload should compile successfully"),
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ScriptStructHotReloadTest::ModuleName, ScriptStructHotReloadTest::ScriptFilename, ScriptV2, ReloadResultV2)))
		{ return; }
		if (!ScriptStructHotReloadTest::VerifyHandledReloadResult(*TestRunner, TEXT("First structural script struct reload should be handled by the full reload pipeline"), ReloadResultV2))
		{ return; }

		UASStruct* SecondVersion = ScriptStructHotReloadTest::FindCurrentStruct();
		if (!TestRunner->TestNotNull(TEXT("First full reload should publish a new canonical script struct"), SecondVersion)) { return; }

		TestRunner->TestNotEqual(TEXT("First full reload should replace the original struct object"), static_cast<UScriptStruct*>(SecondVersion), static_cast<UScriptStruct*>(FirstVersion));
		TestRunner->TestEqual(TEXT("First full reload should wire the old struct directly to the second version"), FirstVersion->NewerVersion, SecondVersion);
		TestRunner->TestEqual(TEXT("GetNewestVersion should resolve the second version after the first reload"), FirstVersion->GetNewestVersion(), static_cast<UScriptStruct*>(SecondVersion));
		TestRunner->TestEqual(TEXT("The current canonical struct should consider itself the newest version"), SecondVersion->GetNewestVersion(), static_cast<UScriptStruct*>(SecondVersion));
		TestRunner->TestTrue(TEXT("The first version should no longer own the canonical struct name after reload"), FirstVersion->GetFName() != ScriptStructHotReloadTest::UnrealStructName);
		if (!TestRunner->TestNotNull(TEXT("First full reload should expose the newly added reflected property"), ScriptStructHotReloadTest::FindStructProperty(SecondVersion, TEXT("AddedValue")))) { return; }
		TestRunner->TestNull(TEXT("The replaced first version should keep its original reflected layout"), ScriptStructHotReloadTest::FindStructProperty(FirstVersion, TEXT("AddedValue")));
		TestRunner->TestNull(TEXT("The second version should not expose the third-version-only property yet"), ScriptStructHotReloadTest::FindStructProperty(SecondVersion, TEXT("TailValue")));

		ECompileResult ReloadResultV3 = ECompileResult::Error;
		if (!TestRunner->TestTrue(TEXT("Second structural script struct reload should compile successfully"),
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ScriptStructHotReloadTest::ModuleName, ScriptStructHotReloadTest::ScriptFilename, ScriptV3, ReloadResultV3)))
		{ return; }
		if (!ScriptStructHotReloadTest::VerifyHandledReloadResult(*TestRunner, TEXT("Second structural script struct reload should also be handled by the full reload pipeline"), ReloadResultV3))
		{ return; }

		UASStruct* ThirdVersion = ScriptStructHotReloadTest::FindCurrentStruct();
		if (!TestRunner->TestNotNull(TEXT("Second full reload should publish the newest canonical script struct"), ThirdVersion)) { return; }

		TestRunner->TestNotEqual(TEXT("Second full reload should replace the second struct object"), static_cast<UScriptStruct*>(ThirdVersion), static_cast<UScriptStruct*>(SecondVersion));
		TestRunner->TestEqual(TEXT("Second full reload should wire the second version directly to the third version"), SecondVersion->NewerVersion, ThirdVersion);
		TestRunner->TestEqual(TEXT("The original struct should walk the full version chain to the newest struct"), FirstVersion->GetNewestVersion(), static_cast<UScriptStruct*>(ThirdVersion));
		TestRunner->TestEqual(TEXT("The middle struct should also resolve to the newest struct"), SecondVersion->GetNewestVersion(), static_cast<UScriptStruct*>(ThirdVersion));
		TestRunner->TestEqual(TEXT("The newest struct should still resolve to itself"), ThirdVersion->GetNewestVersion(), static_cast<UScriptStruct*>(ThirdVersion));
		TestRunner->TestEqual(TEXT("Canonical lookup should resolve to the newest struct after multiple full reloads"), ScriptStructHotReloadTest::FindCurrentStruct(), ThirdVersion);
		TestRunner->TestTrue(TEXT("The second version should also lose the canonical struct name after the next reload"), SecondVersion->GetFName() != ScriptStructHotReloadTest::UnrealStructName);
		if (!TestRunner->TestNotNull(TEXT("The newest struct should expose the tail property introduced by the second reload"), ScriptStructHotReloadTest::FindStructProperty(ThirdVersion, TEXT("TailValue")))) { return; }
		TestRunner->TestNull(TEXT("The middle replaced struct should keep the layout it had when it was canonical"), ScriptStructHotReloadTest::FindStructProperty(SecondVersion, TEXT("TailValue")));
		TestRunner->TestNull(TEXT("The oldest replaced struct should remain frozen at its original layout"), ScriptStructHotReloadTest::FindStructProperty(FirstVersion, TEXT("AddedValue")));
		TestRunner->TestNull(TEXT("The oldest replaced struct should never gain later properties"), ScriptStructHotReloadTest::FindStructProperty(FirstVersion, TEXT("TailValue")));
		}
	}

	TEST_METHOD(CustomGuidStableAcrossSameNameReload)
	{
		using namespace ScriptStructCustomGuidTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ScriptStructCustomGuidTest::StableModuleName.ToString());
			Engine.DiscardModule(*ScriptStructCustomGuidTest::DifferentModuleName.ToString());
			IFileManager::Get().Delete(*ScriptStructCustomGuidTest::GetStableScriptAbsoluteFilename(), false, true, true);
			IFileManager::Get().Delete(*ScriptStructCustomGuidTest::GetDifferentScriptAbsoluteFilename(), false, true, true);
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString StableScriptV1 = TEXT(R"AS(
USTRUCT()
struct FStableGuidStruct
{
	UPROPERTY()
	int Value = 1;
};
)AS");
		const FString StableScriptV2 = TEXT(R"AS(
USTRUCT()
struct FStableGuidStruct
{
	UPROPERTY()
	int Value = 1;

	UPROPERTY()
	int AddedValue = 2;
};
)AS");
		const FString DifferentScript = TEXT(R"AS(
USTRUCT()
struct FDifferentGuidStruct
{
	UPROPERTY()
	int Value = 7;
};
)AS");

		if (!TestRunner->TestTrue(TEXT("Initial stable script struct compile should succeed"),
			CompileAnnotatedModuleFromMemory(&Engine, ScriptStructCustomGuidTest::StableModuleName, ScriptStructCustomGuidTest::StableScriptFilename, StableScriptV1)))
		{ return; }

		UASStruct* InitialStableStruct = ScriptStructCustomGuidTest::FindStableStruct();
		if (!TestRunner->TestNotNull(TEXT("Initial stable script struct should be registered in the Angelscript package"), InitialStableStruct)) { return; }
		const FGuid StableGuidBeforeReload = InitialStableStruct->GetCustomGuid();
		TestRunner->TestTrue(TEXT("Initial stable script struct should publish a valid custom GUID"), StableGuidBeforeReload.IsValid());

		ECompileResult StableReloadResult = ECompileResult::Error;
		if (!TestRunner->TestTrue(TEXT("Stable script struct full reload should compile successfully"),
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ScriptStructCustomGuidTest::StableModuleName, ScriptStructCustomGuidTest::StableScriptFilename, StableScriptV2, StableReloadResult)))
		{ return; }
		if (!ScriptStructHotReloadTest::VerifyHandledReloadResult(*TestRunner, TEXT("Stable script struct full reload should be handled by the reload pipeline"), StableReloadResult))
		{ return; }

		UASStruct* ReloadedStableStruct = ScriptStructCustomGuidTest::FindStableStruct();
		if (!TestRunner->TestNotNull(TEXT("Stable script struct full reload should publish a replacement struct"), ReloadedStableStruct)) { return; }

		TestRunner->TestNotEqual(TEXT("Stable script struct full reload should replace the canonical struct object"), static_cast<UScriptStruct*>(InitialStableStruct), static_cast<UScriptStruct*>(ReloadedStableStruct));
		TestRunner->TestEqual(TEXT("Stable script struct full reload should preserve the original custom GUID"), ReloadedStableStruct->GetCustomGuid(), StableGuidBeforeReload);
		TestRunner->TestEqual(TEXT("Replaced stable script struct should retain its original custom GUID"), InitialStableStruct->GetCustomGuid(), StableGuidBeforeReload);
		TestRunner->TestEqual(TEXT("Replaced stable script struct should resolve the reload result as its newest version"), InitialStableStruct->GetNewestVersion(), static_cast<UScriptStruct*>(ReloadedStableStruct));

		if (!TestRunner->TestTrue(TEXT("Different-name script struct compile should succeed"),
			CompileAnnotatedModuleFromMemory(&Engine, ScriptStructCustomGuidTest::DifferentModuleName, ScriptStructCustomGuidTest::DifferentScriptFilename, DifferentScript)))
		{ return; }

		UASStruct* DifferentStruct = ScriptStructCustomGuidTest::FindDifferentStruct();
		if (!TestRunner->TestNotNull(TEXT("Different-name script struct should be registered in the Angelscript package"), DifferentStruct)) { return; }
		const FGuid DifferentGuid = DifferentStruct->GetCustomGuid();
		TestRunner->TestTrue(TEXT("Different-name script struct should publish a valid custom GUID"), DifferentGuid.IsValid());
		TestRunner->TestNotEqual(TEXT("Different-name script struct should not collide with the stable struct custom GUID"), DifferentGuid, StableGuidBeforeReload);
		}
	}

	TEST_METHOD(UpdateScriptTypeClearsIdenticalAndHashCapabilitiesAfterReload)
	{
		using namespace ScriptStructCapabilityReloadTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ScriptStructCapabilityReloadTest::ModuleName.ToString());
			IFileManager::Get().Delete(*ScriptStructCapabilityReloadTest::GetScriptAbsoluteFilename(), false, true, true);
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
USTRUCT()
struct FReloadableCapabilityStruct
{
	UPROPERTY()
	int Value = 1;

	bool opEquals(const FReloadableCapabilityStruct& Other) const
	{
		return Value == Other.Value;
	}

	uint32 Hash() const
	{
		return uint32(Value + 7);
	}

	FString ToString() const
	{
		return "HasAllCapabilities";
	}
};
)AS");

	const FString ScriptV2 = TEXT(R"AS(
USTRUCT()
struct FReloadableCapabilityStruct
{
	UPROPERTY()
	int Value = 2;

	UPROPERTY()
	int AddedValue = 9;

	FString ToString() const
	{
		return "ToStringOnly";
	}
};
)AS");

		if (!TestRunner->TestTrue(TEXT("Capability baseline script struct compile should succeed"),
			CompileAnnotatedModuleFromMemory(&Engine, ScriptStructCapabilityReloadTest::ModuleName, ScriptStructCapabilityReloadTest::ScriptFilename, ScriptV1)))
		{ return; }

		UASStruct* InitialStruct = ScriptStructCapabilityReloadTest::FindCurrentStruct();
		if (!ScriptStructCapabilityReloadTest::VerifyCapabilityState(*TestRunner, InitialStruct, TEXT("Capability baseline struct"), true, true))
		{ return; }
		if (!TestRunner->TestNotNull(TEXT("Capability baseline struct should keep the script ToString binding"), InitialStruct->GetToStringFunction()))
		{ return; }

		ECompileResult ReloadResult = ECompileResult::Error;
		if (!TestRunner->TestTrue(TEXT("Capability reload script struct compile should succeed"),
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ScriptStructCapabilityReloadTest::ModuleName, ScriptStructCapabilityReloadTest::ScriptFilename, ScriptV2, ReloadResult)))
		{ return; }
		if (!ScriptStructHotReloadTest::VerifyHandledReloadResult(*TestRunner, TEXT("Capability reload should be handled by the full reload pipeline"), ReloadResult))
		{ return; }

		UASStruct* ReloadedStruct = ScriptStructCapabilityReloadTest::FindCurrentStruct();
		if (!TestRunner->TestNotNull(TEXT("Capability reload should publish a replacement script struct"), ReloadedStruct)) { return; }

		TestRunner->TestNotEqual(TEXT("Capability reload should replace the canonical struct object"), static_cast<UScriptStruct*>(InitialStruct), static_cast<UScriptStruct*>(ReloadedStruct));
		TestRunner->TestEqual(TEXT("Capability reload should wire the previous struct to the replacement version"), InitialStruct->GetNewestVersion(), static_cast<UScriptStruct*>(ReloadedStruct));
		TestRunner->TestEqual(TEXT("Capability reload replacement should consider itself the newest version"), ReloadedStruct->GetNewestVersion(), static_cast<UScriptStruct*>(ReloadedStruct));
		TestRunner->TestNotNull(TEXT("Capability reload replacement should expose the added property that forces full reload"), ScriptStructHotReloadTest::FindStructProperty(ReloadedStruct, TEXT("AddedValue")));
		TestRunner->TestNull(TEXT("Capability reload should keep the replaced struct frozen at its original layout"), ScriptStructHotReloadTest::FindStructProperty(InitialStruct, TEXT("AddedValue")));

		if (!ScriptStructCapabilityReloadTest::VerifyCapabilityState(*TestRunner, ReloadedStruct, TEXT("Capability reload replacement struct"), false, false))
		{ return; }

		TestRunner->TestNotNull(TEXT("Capability reload replacement should keep the ToString binding after dropping opEquals and Hash"), ReloadedStruct->GetToStringFunction());

		}
	}
};

#endif
