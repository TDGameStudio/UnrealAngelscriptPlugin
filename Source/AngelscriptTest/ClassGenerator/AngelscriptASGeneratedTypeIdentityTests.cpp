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


namespace ASGeneratedTypeIdentityTest
{
	static const FName StructModuleName(TEXT("ASGeneratedStructIdentity"));
	static const FName StructName(TEXT("StructIdentityTarget"));
	static const FString StructScriptFilename(TEXT("ASGeneratedStructIdentity.as"));

	FString GetScriptAbsoluteFilename()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), StructScriptFilename);
	}

	UScriptStruct* FindStructObjectByName(const FName InStructName)
	{
		return FindObject<UScriptStruct>(FAngelscriptEngine::GetPackage(), *InStructName.ToString());
	}

	UASStruct* FindCurrentStruct()
	{
		return Cast<UASStruct>(FindStructObjectByName(StructName));
	}

	bool VerifyHandledReloadResult(FAutomationTestBase& Test, const TCHAR* Context, const ECompileResult ReloadResult)
	{
		return Test.TestTrue(
			Context,
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled);
	}

	bool VerifyLiveStructIdentity(
		FAutomationTestBase& Test,
		UASStruct* Struct,
		const TCHAR* StageLabel)
	{
		const FString StructMessage = FString::Printf(TEXT("%s should publish a generated UASStruct"), StageLabel);
		if (!Test.TestNotNull(*StructMessage, Struct))
		{
			return false;
		}

		const bool bIsScriptStructMatches = Test.TestTrue(
			*FString::Printf(TEXT("%s should mark the struct as script-generated"), StageLabel),
			Struct->bIsScriptStruct);
		const bool bScriptTypeMatches = Test.TestNotNull(
			*FString::Printf(TEXT("%s should publish a live script type pointer"), StageLabel),
			Struct->ScriptType);
		const bool bNewestVersionMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should resolve GetNewestVersion to itself while canonical"), StageLabel),
			Struct->GetNewestVersion(),
			static_cast<UScriptStruct*>(Struct));
		return bIsScriptStructMatches && bScriptTypeMatches && bNewestVersionMatches;
	}

	bool VerifyReplacedStructIdentity(
		FAutomationTestBase& Test,
		UASStruct* Struct,
		UASStruct* ExpectedNewestVersion,
		const TCHAR* StageLabel)
	{
		const FString StructMessage = FString::Printf(TEXT("%s should keep the replaced struct alive for version-chain lookups"), StageLabel);
		if (!Test.TestNotNull(*StructMessage, Struct))
		{
			return false;
		}

		const bool bIsScriptStructMatches = Test.TestTrue(
			*FString::Printf(TEXT("%s should keep the replaced struct tagged as a script struct"), StageLabel),
			Struct->bIsScriptStruct);
		const bool bNewestVersionMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should point GetNewestVersion at the replacement struct"), StageLabel),
			Struct->GetNewestVersion(),
			static_cast<UScriptStruct*>(ExpectedNewestVersion));
		const bool bDirectVersionLinkMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should wire NewerVersion directly to the replacement struct"), StageLabel),
			Struct->NewerVersion,
			ExpectedNewestVersion);
		const bool bClearedScriptTypeMatches = Test.TestNull(
			*FString::Printf(TEXT("%s should clear the stale script type pointer after full reload"), StageLabel),
			Struct->ScriptType);
		return bIsScriptStructMatches
			&& bNewestVersionMatches
			&& bDirectVersionLinkMatches
			&& bClearedScriptTypeMatches;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASGeneratedTypeIdentityTests,
	"Angelscript.TestModule.ClassGenerator.ASStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptIdentityFieldsTrackFullReloadLifecycle)
	{
		using namespace ASGeneratedTypeIdentityTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASGeneratedTypeIdentityTest::StructModuleName.ToString());
			IFileManager::Get().Delete(*ASGeneratedTypeIdentityTest::GetScriptAbsoluteFilename(), false, true, true);
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
USTRUCT()
struct FStructIdentityTarget
{
	UPROPERTY()
	int Value = 1;
};
)AS");

		const FString ScriptV2 = TEXT(R"AS(
USTRUCT()
struct FStructIdentityTarget
{
	UPROPERTY()
	int Value = 1;

	UPROPERTY()
	int AddedValue = 2;
};
)AS");

		if (!TestRunner->TestTrue(
				TEXT("Struct identity baseline compile should succeed"),
				CompileAnnotatedModuleFromMemory(&Engine, ASGeneratedTypeIdentityTest::StructModuleName, ASGeneratedTypeIdentityTest::StructScriptFilename, ScriptV1)))
		{
			return;
		}

		UASStruct* StructV1 = ASGeneratedTypeIdentityTest::FindCurrentStruct();
		if (!ASGeneratedTypeIdentityTest::VerifyLiveStructIdentity(*TestRunner, StructV1, TEXT("Struct identity baseline")))
		{
			return;
		}

		TestRunner->TestNull(
			TEXT("Struct identity baseline should not publish a newer version link before reload"),
			StructV1->NewerVersion);
		TestRunner->TestNull(
			TEXT("Struct identity baseline should not expose the structural-change property before reload"),
			StructV1->FindPropertyByName(TEXT("AddedValue")));

		ECompileResult ReloadResult = ECompileResult::Error;
		if (!TestRunner->TestTrue(
				TEXT("Struct identity full reload should compile successfully"),
				CompileModuleWithResult(
					&Engine,
					ECompileType::FullReload,
					ASGeneratedTypeIdentityTest::StructModuleName,
					ASGeneratedTypeIdentityTest::StructScriptFilename,
					ScriptV2,
					ReloadResult)))
		{
			return;
		}

		if (!ASGeneratedTypeIdentityTest::VerifyHandledReloadResult(
				*TestRunner,
				TEXT("Struct identity full reload should be handled by the full reload pipeline"),
				ReloadResult))
		{
			return;
		}

		UASStruct* StructV2 = ASGeneratedTypeIdentityTest::FindCurrentStruct();
		if (!ASGeneratedTypeIdentityTest::VerifyLiveStructIdentity(*TestRunner, StructV2, TEXT("Struct identity replacement")))
		{
			return;
		}

		TestRunner->TestNotEqual(
			TEXT("Struct identity full reload should replace the canonical struct object"),
			static_cast<UScriptStruct*>(StructV1),
			static_cast<UScriptStruct*>(StructV2));
		TestRunner->TestNotNull(
			TEXT("Struct identity replacement should expose the newly added reflected property"),
			StructV2->FindPropertyByName(TEXT("AddedValue")));
		TestRunner->TestNull(
			TEXT("Struct identity replacement should become the leaf of the version chain"),
			StructV2->NewerVersion);
		TestRunner->TestNull(
			TEXT("Struct identity replaced struct should keep its original reflected layout"),
			StructV1->FindPropertyByName(TEXT("AddedValue")));
		ASGeneratedTypeIdentityTest::VerifyReplacedStructIdentity(
			*TestRunner,
			StructV1,
			StructV2,
			TEXT("Struct identity replaced struct"));

		}
	}
};

#endif
